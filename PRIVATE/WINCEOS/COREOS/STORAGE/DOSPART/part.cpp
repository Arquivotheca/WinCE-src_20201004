//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

#include <windows.h>
#include <diskio.h>
#include <partdrv.h>
#include <fsdmgr.h>
#include "part.h"
#include "bpb.h"
#include "bootsec.h"
#include "drvdbg.h"

static DWORD LoadDiskInfo(PDriverState pState)
{
    DWORD dummy;

    // fail the open if we can't get params from the device driver
    if (!DoDiskIoControl(pState, IOCTL_DISK_GETINFO, &pState->diskInfo, sizeof(DISK_INFO),
            &pState->diskInfo, sizeof(DISK_INFO), &dummy, NULL))
    {
        if (DoDiskIoControl(pState, DISK_IOCTL_GETINFO, &pState->diskInfo, sizeof(DISK_INFO),
            &pState->diskInfo, sizeof(DISK_INFO), &dummy, NULL))
        {
            DEBUGMSG(ZONE_WARNING, (L"MSPART!LoadDiskInfo (hDisk=0x%08x) driver only supports deprecated ioctl DISK_IOCTL_GETINFO; should support IOCTL_DISK_GETINFO instead!!!\r\n", pState->hDsk));

        } else {

            return ERROR_GEN_FAILURE;
        }
    }

    // simple check to verify the device is useful
    if (pState->diskInfo.di_bytes_per_sect == 0 || pState->diskInfo.di_total_sectors == 0)
    {
        return ERROR_GEN_FAILURE;
    }
    
    if (pState->diskInfo.di_heads == 0)
        pState->diskInfo.di_heads = 1;
        
    if (pState->diskInfo.di_cylinders == 0) 
        pState->diskInfo.di_cylinders = 1;

    if (pState->diskInfo.di_sectors == 0)
        pState->diskInfo.di_sectors = pState->diskInfo.di_total_sectors;
    
    return ERROR_SUCCESS;
}

/*****************************************************************************/


DWORD PD_OpenStore(HANDLE hDisk, LPDWORD pdwStoreId)
{
    PPARTENTRY      tmpbuffer;
    PartState       *pPartState;
    DriverState     *pState;
    DWORD           dwError=ERROR_SUCCESS, lastUsedSector;
    SECTORNUM       snPartSector;
    PBYTE           buffer = NULL;
    int             i;
       
    *pdwStoreId = NULL;
    DEBUGMSG( ZONE_API, (L"MSPART!PD_OpenStore hDisk=%08X\r\n", hDisk));
    pState = (PDriverState)LocalAlloc(LMEM_ZEROINIT, sizeof(DriverState));
    if (!pState)
        return ERROR_OUTOFMEMORY;

    pState->hDsk = hDisk;

    // convert the device handle to an HDSK for FSDMGR_* registry helper functions
    HDSK hDsk = FSDMGR_DeviceHandleToHDSK(hDisk);

    dwError = LoadDiskInfo(pState);
    if (ERROR_SUCCESS != dwError) 
    {
        LocalFree(pState);
        return dwError;
    }

    // Find out if we should create primary partitions
    DWORD dwRegValue;
    FSDMGR_GetRegistryValue( hDsk, TEXT("PrimaryPart"), (PDWORD)&dwRegValue);
    if (dwRegValue)
        pState->dwFlags |= MSPART_CREATE_PRIMARY;

    FSDMGR_GetRegistryValue( hDsk, TEXT("AlignCylinder"), (PDWORD)&dwRegValue);
    if (dwRegValue)
        pState->dwFlags |= MSPART_ALIGN_CYLINDER;

    FSDMGR_GetRegistryValue( hDsk, TEXT("SkipSector1"), (PDWORD)&dwRegValue);
    if (dwRegValue)
        pState->dwFlags |= MSPART_SKIP_SECTOR1;
    
    pState->pPartState = NULL;
    pState->pSearchState = NULL;

    // assume this is unformatted for now
    pState->bFormatState = FALSE;

    if (!ReadSectors(pState, 0, 1, &buffer)) {
        if (!(pState->diskInfo.di_flags & DISK_INFO_FLAG_UNFORMATTED)) {
            dwError = ERROR_READ_FAULT;
        }    
        goto Exit;
    }


    pPartState = NULL;
    snPartSector = 0;

    // process the DOS partition tables and potentially extended partition tables
    dwError = GetDOSPartitions (pState, &pPartState, buffer, FALSE, &snPartSector);
    if (dwError) {
        // If the sector has a boot signature, then we assume that it is a superfloppy
        if (IS_VALID_BOOTSEC(buffer))
        {
            dwError = ERROR_DEVICE_NOT_PARTITIONED;
            goto Exit;
        } 
        else 
        {
            // in this case, we don't fail the call, but the store is not formatted
            if (dwError == ERROR_INVALID_DATA) {
                dwError = ERROR_SUCCESS;

                //  For primary partitions, if there is a valid bootsig, then it is formatted.
                if ((pState->dwFlags & MSPART_CREATE_PRIMARY) && IS_VALID_BOOTSEC(buffer))
                    pState->bFormatState = TRUE;
                
                goto Exit;
            }
        }    
        dwError = ERROR_READ_FAULT;
        goto Exit;
    }

    // this drive has a valid partition table - do a sanity check to adjust the diskInfo structure in case this drive
    //  was formatted through another OS - if that's the case, our CHS geometry should match the previously used CHS
    //  this involves three assumptions: 1 - that all partitions in the partition table are using the same CHS geometry
    //  2 - the CHS parameters fit within the legacy restrictions of 6 bits/sector 8 bits/heads and 10 bits/cylinders
    //  3 - the partitions all end on a cylinder boundary

    tmpbuffer = (PPARTENTRY)(buffer + PARTTABLE_OFFSET);

    // there's no extended partition, add one now to hold our data, and use the rest of the disk
    if (pState->snExtPartSector == 0) {
        lastUsedSector = 0;

        for (i=0, tmpbuffer = (PPARTENTRY)(buffer + PARTTABLE_OFFSET); i < 4; i++, tmpbuffer++) {
            if (tmpbuffer->Part_TotalSectors == 0)
                continue;

            lastUsedSector = tmpbuffer->Part_StartSector + tmpbuffer->Part_TotalSectors -1;

            // the extended partition starts on the next cylinder boundary, so adjust if needed
            if (pState->diskInfo.di_heads > 1)
                lastUsedSector += ((pState->diskInfo.di_heads - (tmpbuffer->Part_LastHead +1)) * pState->diskInfo.di_sectors) + pState->diskInfo.di_sectors - (tmpbuffer->Part_LastSector & 0x3f);
        }

        // create an extended partition only if there's room on the disk and in the MBR
#if 0
	    BOOL            bResult;
        if ((lastUsedSector < (pState->diskInfo.di_total_sectors -1)) &&
            ((i != 3) || (!tmpbuffer->Part_TotalSectors)))
        {

            // if this fails, it's not because there's no room for it
            bResult = WriteMBR(pState, lastUsedSector + 1, pState->diskInfo.di_total_sectors - lastUsedSector -1, PART_EXTENDED, FALSE);
            if (!bResult) {
                PD_CloseStore((DWORD)pState);
                dwError = ERROR_WRITE_FAULT;
                goto Exit;
            }

            // we added it successfully, update the pState structure so we know where it is
            pState->snExtPartSector = lastUsedSector + 1;
            pState->snExtPartEndSec = pState->diskInfo.di_total_sectors - 1;

        }
#endif
        
    }

    // we have a valid format
    pState->bFormatState = TRUE;

#if HIDDEN_PARTITIONS
    pState->bUseHiddenPartitions = TRUE;
    // process the hidden partition table to add in the extra partition data that we track
    if (pState->pHiddenPartState) {
        bResult = GetHiddenPartitions (pState);

        GeneratePartitionNames(pState);

        if (!bResult) {
            UpdateFileTime(pState, NULL, TRUE, FALSE);
            if (!WriteHiddenPartitionTable(pState)) {
                pState->bUseHiddenPartitions = FALSE;
            }
        }
    } else {
        // no hidden partition, try to add one now
        if (!AddHiddenPartition(pState)) {
            pState->bUseHiddenPartitions = FALSE;
        } 

        GeneratePartitionNames(pState);

        if (pState->bUseHiddenPartitions) {
            UpdateFileTime(pState, NULL, TRUE, FALSE);
            if (!WriteHiddenPartitionTable(pState)) {
                pState->bUseHiddenPartitions = FALSE;
            }
        }
    }
#else
        GeneratePartitionNames(pState);
#endif
Exit:
    if (buffer) {
        LocalFree( buffer);
    }
    *pdwStoreId = (DWORD)pState;
    return dwError;
}



/*****************************************************************************/


DWORD PD_GetStoreInfo(DWORD dwStoreId, PPD_STOREINFO psi)
{

    BOOL    bResult;
    DriverState *pState = (PDriverState) dwStoreId;

    DEBUGMSG(ZONE_API, (L"MSPART!PD_GetStoreInfo: dwStoreId=%08X \r\n", dwStoreId));

    if (psi->cbSize != sizeof(PD_STOREINFO)) {
        return ERROR_INVALID_PARAMETER;
    }
    memset( psi, 0, sizeof(PD_STOREINFO));
    psi->cbSize = sizeof(PD_STOREINFO); 

    psi->ftCreated = pState->ftCreate;
    psi->ftLastModified = pState->ftAccess;
    psi->dwBytesPerSector = pState->diskInfo.di_bytes_per_sect;
    psi->snNumSectors = pState->diskInfo.di_total_sectors;

    bResult = CalculatePartitionSpace(pState, &psi->snFreeSectors, &psi->snBiggestPartCreatable);
    if (!bResult)
        return ERROR_GEN_FAILURE;

    if (!pState->bFormatState)
        psi->dwAttributes |= STORE_ATTRIBUTE_UNFORMATTED;
    
    return ERROR_SUCCESS;
}


/*****************************************************************************/


void PD_CloseStore(DWORD dwStoreId)
{
    DriverState *pState = (PDriverState)dwStoreId;
    ClearPartitionList(pState);
    LocalFree(pState);
}


/*****************************************************************************/


DWORD PD_FormatStore(DWORD dwStoreId)
{

    PBYTE           buffer = NULL;
    DWORD           bufferSize;
    DWORD           dummy;
    BOOL            bResult;
    DWORD           dwError;
    DriverState *pState = (PDriverState) dwStoreId;


    DEBUGMSG(ZONE_API, (L"PD_FormatStore: dwStoreId=%08X\n", dwStoreId));
    
    if (!DoDiskIoControl(pState, IOCTL_DISK_FORMAT_MEDIA, &(pState->diskInfo), sizeof(DISK_INFO), NULL, 0, &dummy, NULL)) {

        if (DoDiskIoControl(pState, DISK_IOCTL_FORMAT_MEDIA, &(pState->diskInfo), sizeof(DISK_INFO), NULL, 0, &dummy, NULL)) {

            DEBUGMSG(ZONE_WARNING, (L"MSPART!PD_FormatStore (hDisk=0x%08x) driver only supports deprecated ioctl DISK_IOCTL_FORMAT_MEDIA; should support IOCTL_DISK_FORMAT_MEDIA instead!!!\r\n", pState->hDsk));

        } else {

            dwError= GetLastError();
            RETAILMSG(TRUE,(L"MSPART!FormatStore: Driver has failed low-level format request (%d)\n", dwError));
            return dwError? dwError : ERROR_GEN_FAILURE;
        }
    }

    /* update the disk information structure */
    dwError = LoadDiskInfo(pState);
    if (ERROR_SUCCESS != dwError) {
        return dwError;
    }

    //
    // We should never see a device with a sector size this large.
    //
    if( ULONG_MAX / 8 < pState->diskInfo.di_bytes_per_sect )
    {
        return ERROR_MEDIA_INCOMPATIBLE;
    }
    
    /* get an I/O buffer that is a multiple of the sector size */
    bufferSize = pState->diskInfo.di_bytes_per_sect * 8;
    buffer     = (PBYTE)LocalAlloc(LMEM_ZEROINIT, bufferSize);
    if (!buffer)
        return ERROR_OUTOFMEMORY;   

    if (pState->dwFlags & MSPART_CREATE_PRIMARY) {
        // add header and trailer info to the MBR.  Clear partition table.
        *(WORD *)(buffer + BOOT_SIGNATURE) = BOOTSECTRAILSIGH;
        buffer[0] = 0xE9;
        buffer[1] = 0xfd;
        buffer[2] = 0xff;
        memset (buffer + PARTTABLE_OFFSET, 0, MAX_PARTTABLE_ENTRIES * SIZE_PARTTABLE_ENTRIES);
    }     

    bResult = WriteSectors(pState, 0, bufferSize / pState->diskInfo.di_bytes_per_sect, buffer);

    LocalFree(buffer);

    if (!bResult)
        return ERROR_WRITE_FAULT;


    // if we are formatted a previously formatted store, we need to clear out the existing partition structures first
    if (pState->bFormatState)
        ClearPartitionList(pState);
    pState->dwNumParts = 0;

    if (!(pState->dwFlags & MSPART_CREATE_PRIMARY)) {

        // carve out the extended partition

        // skip to the next cylinder boundary        
        if (pState->diskInfo.di_heads > 1)
            pState->snExtPartSector = pState->diskInfo.di_sectors * pState->diskInfo.di_heads;
        else
            pState->snExtPartSector = 1;

        pState->snExtPartEndSec = pState->diskInfo.di_total_sectors - 1;

        bResult = WriteMBR(pState, pState->snExtPartSector, pState->diskInfo.di_total_sectors - pState->snExtPartSector, PART_EXTENDED, TRUE);
        if (!bResult)
            return ERROR_WRITE_FAULT;
    }
        
#if HIDDEN_PARTITIONS
    pState->bUseHiddenPartitions = TRUE;
    // next create a PBR for the hidden partition
    bResult = AddHiddenPartition(pState);
    if (!bResult) {
        pState->bUseHiddenPartitions = FALSE;
    } else {    
        UpdateFileTime(pState, NULL, TRUE, FALSE);
        WriteHiddenPartitionTable(pState);
    }    
#endif
    
    if (bResult)
        pState->bFormatState = TRUE;

    return bResult ? ERROR_SUCCESS : ERROR_GEN_FAILURE;
}

/*****************************************************************************/


DWORD PD_IsStoreFormatted(DWORD dwStoreId)
{
    PDriverState pState = (PDriverState) dwStoreId;

    return pState->bFormatState ? ERROR_SUCCESS : ERROR_BAD_FORMAT;
}


/*****************************************************************************/

DWORD PD_CreatePartition(DWORD dwStoreId, LPCTSTR szPartitionName, BYTE bPartType, SECTORNUM snNumSectors, BOOL bAuto)
{
    PartState    *pNewPartState;
    PartState    *pTmpPartState;
    SearchState  *pSearchState;
    BOOL            bResult;
    SECTORNUM       snStartSector = 0;
    SECTORNUM       snNextPBRSector;
    SECTORNUM       snPrevPBRSector;
    SECTORNUM       snSectorCount;
    SECTORNUM       snPBRSector;
    DriverState     *pState = (PDriverState) dwStoreId;

    DEBUGMSG (ZONE_API,(L"MSPART!PD_CreatePartition: dwStoreId=%08X, PartName %s, PartType=%ld, NumSectors=%d Auto=%s\n", dwStoreId, szPartitionName, bPartType, (DWORD)snNumSectors, bAuto ? L"TRUE" : L"FALSE"));

    if (!pState->bFormatState) { 
        return ERROR_BAD_FORMAT;
    }

    if (!wcslen(szPartitionName)) {
        return ERROR_INVALID_NAME;
    }
    
    if ((!snNumSectors) || (snNumSectors > pState->diskInfo.di_total_sectors)) {
        return ERROR_DISK_FULL;
    }

    GetPartition(pState, &pTmpPartState, NULL, szPartitionName);

    // don't allow the create if the name is not unique
    if (pTmpPartState) {
        return ERROR_ALREADY_EXISTS;
    }

    // the number of partitions is historically limited to the # of drive letters available through the extended partition
    if (pState->dwNumParts == MAX_PARTITIONS) {
        return ERROR_PARTITION_FAILURE;
    }

#if HIDDEN_PARTITIONS
    // if we don't have a hidden partition, we probably don't have room for this one either
    // but we'll try to create the hidden partition first and then if that works create the one requested
    if (pState->bUseHiddenPartitions) {
        UpdateFileTime(pState, NULL, TRUE, FALSE);
        if (!WriteHiddenPartitionTable(pState)) {
            pState->bUseHiddenPartitions = FALSE;
        }   
    }
#endif    

    bResult = GetPartLocation (pState, snNumSectors, bPartType, &pTmpPartState, &snStartSector, &snPBRSector, &snSectorCount);
    if (!bResult)
        return ERROR_PARTITION_FAILURE;

    pNewPartState = (PPartState)LocalAlloc(LMEM_ZEROINIT, sizeof(PartState));
    if (!pNewPartState)
        return ERROR_OUTOFMEMORY;

    snNextPBRSector = 0;
    snPrevPBRSector = 0;

    // we are linking up to an existing partition
    if (pTmpPartState) {
        snPrevPBRSector = pTmpPartState->snPBRSector;

        if (pTmpPartState->pNextPartState)
            snNextPBRSector = pTmpPartState->pNextPartState->snPBRSector;
    } else {
        // this partition may be added to the front of the list
        if (pState->pPartState)
            snNextPBRSector = pState->pPartState->snPBRSector;
    }

    // update info for new partition
    wcscpy(pNewPartState->cPartName, szPartitionName);

    pNewPartState->snStartSector = snStartSector;

    if (snPBRSector)
        pNewPartState->snNumSectors = snSectorCount - (snStartSector - snPBRSector);
    else
        pNewPartState->snNumSectors = snSectorCount;

    pNewPartState->pNextPartState = NULL;
    pNewPartState->pState = pState;
    pNewPartState->snPBRSector = snPBRSector;

    bResult = InitPartition (pState, pNewPartState);
    if (!bResult) {
        LocalFree(pNewPartState);
        return ERROR_PARTITION_FAILURE;
    }

    // add the partition info the DriverState structure
    if (pTmpPartState == NULL) {
        // we're adding to the front of the list
        if (pState->pPartState)
            pNewPartState->pNextPartState = pState->pPartState;

        pState->pPartState = pNewPartState;
    } else {
        // if this partition is filling a gap, add the link at the right point in the list
        if (pTmpPartState->pNextPartState)
            pNewPartState->pNextPartState = pTmpPartState->pNextPartState;

        pTmpPartState->pNextPartState = pNewPartState;
    }

    pState->dwNumParts++;

#if HIDDEN_PARTITIONS
    if (pState->bUseHiddenPartitions) {
        UpdateFileTime(pState, pNewPartState, FALSE, TRUE);

        // add this to the partition table on the media
        WriteHiddenPartitionTable(pState);
    }   
#endif    

    if (bResult) {
        if (bAuto)
            GenerateFATFSType(pState, pNewPartState->snNumSectors, &pNewPartState->partType);
        else
            pNewPartState->partType = bPartType;

        DEBUGMSG(ZONE_STORE,(L"MSPART!PD_CreatePartition - type is %d\n", pNewPartState->partType));

        // after everything that can fail succeeds, hook this partition in to the partition tables
        if (snPBRSector)
            bResult = CreatePBR(pState, snPrevPBRSector, snNextPBRSector, snPBRSector, snSectorCount, pNewPartState->partType);
        else
            bResult = WriteMBR(pState, snStartSector, snSectorCount, pNewPartState->partType, FALSE);
    }

    // if we failed to write out the partition table, set the pState back to the way it was
    if (!bResult) {
        if (pTmpPartState == NULL)
            pState->pPartState = NULL;
        else
            pTmpPartState->pNextPartState = pNewPartState->pNextPartState;

        pState->dwNumParts--;

        LocalFree(pNewPartState);
        return ERROR_PARTITION_FAILURE;
    }

    // the last thing we do is to fix up any search pStates for this store that may be in process
    if (pNewPartState->pNextPartState) {
        pSearchState = pState->pSearchState;

        // walk through the list of SearchStates to see if any need to be adjusted for this new partition
        while(pSearchState) {
            // if the new partition is added at the search point, set the search point to the new partition
            if (pSearchState->pPartState == pNewPartState->pNextPartState)
                pSearchState->pPartState = pNewPartState;

            pSearchState = pSearchState->pNextSearchState;

        }
    }

    return ERROR_SUCCESS;
}

/*****************************************************************************/


DWORD PD_OpenPartition(DWORD dwStoreId, LPCTSTR szPartitionName, LPDWORD pdwPartitionId)
{

    PartState * pPartState;
    DriverState *pState = (PDriverState) dwStoreId;

    DEBUGMSG(1,(L"MSPART!PD_OpenPartition: dwStoreId=%08X, PartName=%s\n", dwStoreId, szPartitionName));
    if (!pState) {
        return ERROR_INVALID_PARAMETER;
    }    

    GetPartition(pState, &pPartState, NULL, szPartitionName);

    // don't allow the create if the name is not unique
    if (!pPartState) {
        return ERROR_NOT_FOUND;
    }

    *pdwPartitionId = (DWORD)pPartState;
    return ERROR_SUCCESS;
}


/*****************************************************************************/


DWORD ReadPartition(DWORD dwPartitionId,
                       PBYTE pInBuf, DWORD nInBufSize,
                       PBYTE pOutBuf, DWORD nOutBufSize,
                       PDWORD pBytesReturned)
{
    PartState *pPartState = (PPartState)dwPartitionId;
    DWORD dwError = ERROR_SUCCESS;
    SG_REQ *req;

    req = (SG_REQ *)pInBuf;

    // be sure the sector # is within range of the partition
    if ((req->sr_start >= pPartState->snNumSectors) || 
        (pPartState->snNumSectors - req->sr_start < req->sr_num_sec))
    {
        return ERROR_INVALID_BLOCK;
    }

    req->sr_start += (DWORD)pPartState->snStartSector;
    if (!DoDiskIoControl(pPartState->pState, IOCTL_DISK_READ, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned, NULL)) {
        
        if (DoDiskIoControl(pPartState->pState, DISK_IOCTL_READ, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned, NULL)) {

            DEBUGMSG(ZONE_WARNING, (L"MSPART!ReadPartition (hDisk=0x%08x) driver only supports deprecated ioctl DISK_IOCTL_READ; should support IOCTL_DISK_READ instead!!!\r\n", pPartState->pState->hDsk));

        } else {
            
            dwError = GetLastError();
            if (dwError == ERROR_SUCCESS) {
                dwError = ERROR_GEN_FAILURE;
            }
        }
    }
    req->sr_start -= (DWORD)pPartState->snStartSector;

    return dwError;
}


/*****************************************************************************/


DWORD WritePartition(DWORD dwPartitionId,
                        PBYTE pInBuf, DWORD nInBufSize,
                        PBYTE pOutBuf, DWORD nOutBufSize,
                        PDWORD pBytesReturned)
{
    PartState *pPartState = (PPartState)dwPartitionId;
    DEBUGMSG( ZONE_PARTITION, (L"MSPART!WritePartition dwPartitionId=%08X pInBuf=%08X nInBufSize=%ld\r\n", dwPartitionId, pInBuf, nInBufSize));
    DWORD dwError = ERROR_SUCCESS;
    SG_REQ *req;

    req = (SG_REQ *)pInBuf;

    // be sure the sector # is within range of the partition
    if  ((req->sr_start >= pPartState->snNumSectors) || 
         (pPartState->snNumSectors - req->sr_start < req->sr_num_sec))
    {
        return ERROR_INVALID_BLOCK;
    }

    if (pPartState->dwAttributes & PARTITION_ATTRIBUTE_READONLY) {
        return ERROR_ACCESS_DENIED;        
    }

    req->sr_start += (DWORD)pPartState->snStartSector;
    if (!DoDiskIoControl(pPartState->pState, IOCTL_DISK_WRITE, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned, NULL)) {
        
        if (DoDiskIoControl(pPartState->pState, DISK_IOCTL_WRITE, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned, NULL)) {

            DEBUGMSG(ZONE_WARNING, (L"MSPART!WritePartition (hDisk=0x%08x) driver only supports deprecated ioctl DISK_IOCTL_WRITE; should support IOCTL_DISK_WRITE instead!!!\r\n", pPartState->pState->hDsk));

        } else {
            
            dwError = GetLastError();
            if (dwError == ERROR_SUCCESS) {
                dwError = ERROR_GEN_FAILURE;
            }
        }
    }
    req->sr_start -= (DWORD)pPartState->snStartSector;

    return dwError;
}

DWORD DeleteSectors(PartState *pPartState, DELETE_SECTOR_INFO *pdsi, PDWORD pBytesReturned)
{
    DWORD dwError = ERROR_SUCCESS;
    
    if (pdsi->cbSize != sizeof(DELETE_SECTOR_INFO))  {
        return ERROR_INVALID_PARAMETER;
    }
    if ((pdsi->startsector >= pPartState->snNumSectors) || (pdsi->startsector + pdsi->numsectors > pPartState->snNumSectors)) {
        return ERROR_INVALID_BLOCK;
    } 
    if (pPartState->dwAttributes & PARTITION_ATTRIBUTE_READONLY) {
        return ERROR_ACCESS_DENIED;        
    }
    
    pdsi->startsector += (DWORD)pPartState->snStartSector;
    if (!DoDiskIoControl(pPartState->pState, IOCTL_DISK_DELETE_SECTORS, (PBYTE)pdsi, sizeof(DELETE_SECTOR_INFO), NULL, 0, pBytesReturned, NULL)) {
        dwError = GetLastError();            
        if (ERROR_SUCCESS == dwError) {
            dwError = ERROR_GEN_FAILURE;
        }
    }
    pdsi->startsector -= (DWORD)pPartState->snStartSector;
    return dwError;
}

DWORD SecureWipe(DWORD dwCode, PartState *pPartState)
{
    DWORD dwError = ERROR_SUCCESS;

    DELETE_SECTOR_INFO dsi;
    dsi.cbSize = sizeof(DELETE_SECTOR_INFO);
    dsi.startsector =  (DWORD)pPartState->snStartSector;
    dsi.numsectors = (DWORD)pPartState->snNumSectors;
    
    if (pPartState->dwAttributes & PARTITION_ATTRIBUTE_READONLY) {
        return ERROR_ACCESS_DENIED;        
    }
    
    if (!DoDiskIoControl(pPartState->pState, dwCode, (PBYTE)&dsi, sizeof(DELETE_SECTOR_INFO), NULL, 0, NULL, NULL)) {
        dwError = GetLastError();            
        if (ERROR_SUCCESS == dwError) {
            dwError = ERROR_GEN_FAILURE;
        }
    }
    return dwError;
}

DWORD GetSectorAddrs(PartState *pPartState, DWORD *pdwSectorList, DWORD cbSectorList, DWORD *pdwAddrList, DWORD cbAddrList, DWORD *pBytesReturned)
{
    DWORD dwError = ERROR_SUCCESS;
    DWORD iSector, cSectors = cbSectorList / sizeof(DWORD);

    // change partition relative sector addrs to absolute sector addrs
    for (iSector = 0; iSector < cSectors; iSector++) {
        pdwSectorList[iSector] += (DWORD)pPartState->snStartSector;
    }

    if (!DoDiskIoControl(pPartState->pState, IOCTL_DISK_GET_SECTOR_ADDR, pdwSectorList, cbSectorList, pdwAddrList, cbAddrList, pBytesReturned, NULL)) {
        dwError = GetLastError();
        if (ERROR_SUCCESS == dwError) {
            dwError = ERROR_GEN_FAILURE;
        }
    }

    // revert addrs back to partition relative
    for (iSector = 0; iSector < cSectors; iSector++) {
        pdwSectorList[iSector] -= (DWORD)pPartState->snStartSector;
    }
    
    return dwError;
}

DWORD CopyExternal(PartState *pPartState, DISK_COPY_EXTERNAL *pdce, LPBYTE pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned)
{
    DWORD dwError = ERROR_SUCCESS;
    DWORD dwNumSectors = pdce->cbSectorListSize / sizeof(SECTOR_LIST_ENTRY);
    SECTOR_LIST_ENTRY *pSectorList = (SECTOR_LIST_ENTRY *)(pdce+1);

    while(dwNumSectors--) {
        pSectorList->dwStartSector += (DWORD)pPartState->snStartSector;
        pSectorList++;
    }
    if (!DoDiskIoControl(pPartState->pState, IOCTL_DISK_COPY_EXTERNAL_START, (PBYTE)pdce, sizeof(DISK_COPY_EXTERNAL)+pdce->cbSectorListSize,  pOutBuf, nOutBufSize, pBytesReturned, NULL)) {
        dwError = GetLastError();            
        if (ERROR_SUCCESS == dwError) {
            dwError = ERROR_GEN_FAILURE;
        }
    }
    return dwError;
}


/*****************************************************************************/
DWORD PD_DeviceIoControl(DWORD dwPartitionId, DWORD dwCode, PBYTE pInBuf, DWORD nInBufSize, PBYTE pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned)
{
    PartState *pPartState = (PPartState)dwPartitionId;
    DWORD dwError=ERROR_SUCCESS;
    DISK_INFO *pdi = NULL;

    DEBUGMSG( ZONE_API, (L"MSPART!PD_DeviceIoControl dwPartitionId=%08X pInBuf=%08X nInBufSize=%ld pOutBuf=%08X nOutBufSize=%ld\r\n", 
            dwPartitionId, 
            pInBuf, 
            nInBufSize,
            pOutBuf,
            nOutBufSize));
    if ((dwCode == IOCTL_DISK_READ) || (dwCode == DISK_IOCTL_READ)) {
        dwError = ReadPartition(dwPartitionId, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned);
    } else
    if ((dwCode == IOCTL_DISK_WRITE) || (dwCode == DISK_IOCTL_WRITE)) {
        dwError = WritePartition(dwPartitionId, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned);
    } else
    if (dwCode == IOCTL_DISK_DELETE_SECTORS) {
        if (nInBufSize != sizeof(DELETE_SECTOR_INFO)) {
            return ERROR_INVALID_PARAMETER;
        } else {    
            dwError = DeleteSectors( pPartState, (DELETE_SECTOR_INFO *)pInBuf, pBytesReturned);
        }    
    } else 
    if ((dwCode == IOCTL_DISK_SECURE_WIPE) || (dwCode == IOCTL_DISK_SET_SECURE_WIPE_FLAG)) {
        dwError = SecureWipe(dwCode, pPartState);
    } else 
    if (dwCode == IOCTL_DISK_COPY_EXTERNAL_START) {
        if (nInBufSize >= sizeof(DISK_COPY_EXTERNAL)) {
            dwError = CopyExternal( pPartState, (DISK_COPY_EXTERNAL *)pInBuf, pOutBuf, nOutBufSize,  pBytesReturned);
        } else {
            return ERROR_INVALID_PARAMETER;
        }
    } else 
    if ((dwCode == IOCTL_DISK_GETINFO) || (dwCode == DISK_IOCTL_GETINFO)) {
        if (pInBuf) {
            if (nInBufSize != sizeof(DISK_INFO)) 
                return ERROR_INVALID_PARAMETER;
            pdi = (DISK_INFO *)pInBuf;
        }
        if (pOutBuf) {
            if (nOutBufSize!= sizeof(DISK_INFO)) 
                return ERROR_INVALID_PARAMETER;
            pdi = (DISK_INFO *)pOutBuf;
        }
        if (pdi) {
            if (DoDiskIoControl(pPartState->pState, dwCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned, NULL)) {
                pdi->di_total_sectors = (DWORD)pPartState->snNumSectors;
                pdi->di_cylinders = 1;
                pdi->di_heads = 1;
                pdi->di_sectors = 1;
                pdi->di_flags &= ~DISK_INFO_FLAG_MBR;
                if (pPartState->pState->bFormatState) {
                    pdi->di_flags &= ~DISK_INFO_FLAG_UNFORMATTED;
                }    
                if (pBytesReturned)
                    *pBytesReturned = sizeof(DISK_INFO);
                dwError = ERROR_SUCCESS;                    
            } else {
                dwError = GetLastError();
                if (dwError == ERROR_SUCCESS) 
                    dwError = ERROR_GEN_FAILURE;
            }    
        } else {
            dwError =  ERROR_INVALID_PARAMETER;
        }   
    } else 
    if (dwCode == IOCTL_DISK_GET_SECTOR_ADDR) {
        if (nOutBufSize && (nInBufSize == nOutBufSize) && pInBuf && pOutBuf) {
            dwError = GetSectorAddrs( pPartState, (DWORD*)pInBuf, nInBufSize, (DWORD*)pOutBuf, nOutBufSize, pBytesReturned);
        } else {
            dwError = ERROR_INVALID_PARAMETER;
        }
    } else {
        // Don't recognize the IOCTL, forward it.
        if (!DoForwardDiskIoControl(pPartState->pState, dwCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned, NULL)) {
            dwError = GetLastError();
            if (dwError == ERROR_SUCCESS) 
                dwError = ERROR_GEN_FAILURE;
        }
    }    
    return dwError;
}

/*****************************************************************************/

void PD_ClosePartition(DWORD dwPartitionId)
{
    PartState *pPartState = (PPartState)dwPartitionId;
    DEBUGMSG( ZONE_API, (L"MSPART!PD_ClosePartition dwPartitionId=%08X\r\n", dwPartitionId));
    // the partition structures are freed when the store is closed
    // we use them even if the partition isn't 'mounted' by the file system
    // to perform other partitioning functions

    return;
}


/*****************************************************************************/


DWORD PD_DeletePartition(DWORD dwStoreId, LPCTSTR szPartitionName)
{
    DriverState *pState = (PDriverState) dwStoreId;
    PartState *pPartState;
    PartState *pPrevPartState;
    SearchState *pSearchState;
    BOOL bResult = TRUE;

    DEBUGMSG(ZONE_API,(L"MSPART!PD_DeletePartition: dwStoreId=%08X PartName=%s\n", dwStoreId, szPartitionName));

    GetPartition(pState, &pPartState, &pPrevPartState, szPartitionName);

    if (!pPartState) {
        SetLastError(ERROR_NOT_FOUND);
        return ERROR_NOT_FOUND;
    }

    if (pPartState->dwAttributes & PARTITION_ATTRIBUTE_READONLY) {
        SetLastError(ERROR_ACCESS_DENIED);
        return ERROR_ACCESS_DENIED;        
    }

#if HIDDEN_PARTITIONS
    // don't allow the hidden partition to be deleted
    if (pPartState == pState->pHiddenPartState) {
        SetLastError(ERROR_NOT_FOUND);
        return ERROR_NOT_FOUND;
    }
#endif    

    // remove the link from the DOS partition table
    if (pPartState->snPBRSector)
        bResult = DeletePBR(pState, pPartState, pPrevPartState);
    else
        bResult = DeleteMBR(pState, pPartState);

    if (!bResult)
        return ERROR_GEN_FAILURE;


    // Delete the sectors that made up the partition to indicate they are no longer needed.  Used by flash drivers.
    DeletePartitionSectors(pState, pPartState);

    // remove this partition from the linked list
    if (pPrevPartState)
        pPrevPartState->pNextPartState = pPartState->pNextPartState;
    else
        pState->pPartState = pPartState->pNextPartState;

    pState->dwNumParts--;

#if HIDDEN_PARTITIONS
    if (pState->bUseHiddenPartitions) {
        UpdateFileTime(pState, NULL, FALSE, FALSE);

        WriteHiddenPartitionTable(pState);
    }   
#endif    

    pSearchState = pState->pSearchState;

    // walk through the list of SearchStates to see if any need to be adjusted for this new partition
    while(pSearchState) {
        // if the new partition is added at the search point, set the search point to the new partition
        if (pSearchState->pPartState == pPartState)
            pSearchState->pPartState = pPartState->pNextPartState;

        pSearchState = pSearchState->pNextSearchState;

    }

    LocalFree(pPartState);
    return ERROR_SUCCESS;
}


/*****************************************************************************/


DWORD PD_RenamePartition(DWORD dwStoreId, LPCTSTR szOldPartName, LPCTSTR szNewPartNmae)
{
    DriverState *pState = (PDriverState) dwStoreId;
    PartState *pPartState;
    BOOL bResult = TRUE;

    DEBUGMSG(ZONE_API,(L"MSPART!PD_RenamePartition: dwStoreId=%08X, name %s, new name %s\n", dwStoreId, szOldPartName, szNewPartNmae));
    if (!pState) {
        return ERROR_INVALID_PARAMETER;
    }    

    GetPartition(pState, &pPartState, NULL, szNewPartNmae);
    // don't allow the rename if the new name is already used
    if (pPartState) {
        return ERROR_ALREADY_EXISTS;
    }

    GetPartition(pState, &pPartState, NULL, szOldPartName);
    if (!pPartState) {
        return ERROR_NOT_FOUND;
    }

    memset(&pPartState->cPartName, 0, sizeof(pPartState->cPartName));
    wcscpy(pPartState->cPartName, szNewPartNmae);

#if HIDDEN_PARTITIONS
    if (pState->bUseHiddenPartitions) {
        UpdateFileTime(pState, pPartState, FALSE, FALSE);

        // update the partition table on the media
        WriteHiddenPartitionTable(pState);
    }    
#endif    

    // if we failed to update the partition table, update the partition info to the way it was before we started
    if (!bResult) {
        memset(&pPartState->cPartName, 0, sizeof(pPartState->cPartName));
        wcscpy(pPartState->cPartName, szOldPartName);
        return ERROR_GEN_FAILURE;
    }

    return ERROR_SUCCESS;
}


/*****************************************************************************/


DWORD PD_SetPartitionAttrs(DWORD dwStoreId, LPCTSTR szPartitionName, DWORD attrs)
{
    DriverState *pState = (PDriverState) dwStoreId;
    PartState *pPartState;
    DWORD oldAttrs;
    BOOL bResult = TRUE;

    DEBUGMSG(ZONE_API,(L"MSPART!PD_SetPartitionAttrs: dwStoreId=%08X, name %s, attrs 0x%x\n", dwStoreId, szPartitionName, attrs));
    if (!pState) {
        return ERROR_INVALID_PARAMETER;
    }    

    GetPartition(pState, &pPartState, NULL, szPartitionName);

    if (!pPartState)
    {
        return ERROR_NOT_FOUND;
    }

    // save in case we need to restore it
    oldAttrs = pPartState->dwAttributes;
    pPartState->dwAttributes = attrs;

#if HIDDEN_PARTITIONS
    if (pState->bUseHiddenPartitions) {
        UpdateFileTime(pState, pPartState, FALSE, FALSE);

       // update the partition table on the media
        bResult = WriteHiddenPartitionTable(pState);
    }   
#endif    

    // if we failed to update the partition table, update the partition info to the way it was before we started
    if (!bResult)
    {
        pPartState->dwAttributes = oldAttrs;
        return ERROR_GEN_FAILURE;
    }

    return ERROR_SUCCESS;
}


/*****************************************************************************/


DWORD PD_SetPartitionSize(DWORD dwStoreId, LPCTSTR szPartitionName, SECTORNUM snNumSectors)
{
    DriverState *pState = (PDriverState) dwStoreId;
    PartState *pPartState;
    SECTORNUM snPrevious;
    DWORD dwAlign = (pState->dwFlags & MSPART_ALIGN_CYLINDER) ? pState->diskInfo.di_sectors * pState->diskInfo.di_heads : pState->diskInfo.di_sectors;

    DEBUGMSG(ZONE_API,(L"MSPART!PD_SetPartitionSize: dwStoreId=%08X, name %s, size %08x%08x\n", dwStoreId, 
        szPartitionName, (DWORD)(snNumSectors>>32), (DWORD)snNumSectors));
    if (!pState) {
        return ERROR_INVALID_PARAMETER;
    }    

    GetPartition(pState, &pPartState, NULL, szPartitionName);

    if (!pPartState || pPartState->snPBRSector)
    {
        // logical drives cannot be resized
        return ERROR_NOT_FOUND;
    }

    // align the end on a cylinder boundary
    if (dwAlign) {
        DWORD dwExtra = (DWORD)((pPartState->snStartSector + snNumSectors) % dwAlign);
        if (dwExtra) {
            // the requested size will not end on a cylinder boundary, so round up to the next cylinder
            snNumSectors += (dwAlign - dwExtra);
        }
    }

    // store the new sector size
    snPrevious = pPartState->snNumSectors;
    pPartState->snNumSectors = snNumSectors;

    // update the MBR
    if (!UpdateMBR(pState, pPartState)) 
    {
        // revert to the old size
        pPartState->snNumSectors = snPrevious;
        return ERROR_GEN_FAILURE;
    }

    return ERROR_SUCCESS;
}


/*****************************************************************************/


DWORD PD_FormatPartition(DWORD dwStoreId, LPCTSTR szPartitionName, BYTE bPartType, BOOL bAuto)
{
    DriverState *pState = (PDriverState) dwStoreId;
    PartState *pPartState;
    BOOL      bResult = TRUE;
    BYTE  newFileSys = 0xff;
    PBYTE       buffer = NULL;
    PPARTENTRY   tmpbuffer;

    DEBUGMSG(ZONE_API,(L"MSPART!PD_FormatPartition: dwStoreId=%08X, PartName=%s, PartType=%ld Auto=%s\n", dwStoreId, szPartitionName, bPartType, bAuto ? L"TRUE" : L"FALSE"));
    if (!pState) {
        return ERROR_INVALID_PARAMETER;
    }    

    GetPartition(pState, &pPartState, NULL, szPartitionName);

    if (!pPartState) {
        return ERROR_NOT_FOUND;
    }

    if (pPartState->dwAttributes & PARTITION_ATTRIBUTE_READONLY) {
        SetLastError(ERROR_ACCESS_DENIED);
        return ERROR_ACCESS_DENIED;        
    }

    bResult = InitPartition (pState, pPartState);
    if (!bResult)
        return FALSE;

    newFileSys = bPartType;
    if (bAuto && (bPartType != pPartState->partType)) {
        GenerateFATFSType(pState, pPartState->snNumSectors, &newFileSys);
    }

#if HIDDEN_PARTITIONS
    if (pState->bUseHiddenPartitions) {
        UpdateFileTime(pState, pPartState, FALSE, FALSE);

        // update the partition table on the media
        WriteHiddenPartitionTable(pState);
    }    
#endif    

    if (bResult && (newFileSys != 0xff) && (newFileSys != pPartState->partType)) {
        // we need to update the partition entry for this partition to reflect the change in format
        // if the partition type has not changed, then don't bother to update the MBR

        DEBUGMSG(1,(L"PD_FormatPartition - type is %d\n", newFileSys));

        if (pPartState->snPBRSector) {
            buffer = NULL;
            bResult = ReadSectors(pState, pPartState->snPBRSector, 1, &buffer);
            if (bResult) {
                tmpbuffer = (PPARTENTRY)(buffer + PARTTABLE_OFFSET);
                tmpbuffer->Part_FileSystem = newFileSys;
                bResult = WriteSectors(pState, pPartState->snPBRSector, 1, buffer);
                LocalFree (buffer);
            }   

        } else {
            // if it's in the MBR - we'll take the easy way out and delete the entry and re-add it
            bResult = DeleteMBR(pState, pPartState);

            if (bResult)
                bResult = WriteMBR(pState, pPartState->snStartSector, pPartState->snNumSectors, newFileSys, FALSE);
        }

        if (bResult)
            pPartState->partType = newFileSys;
    }

    return bResult ? ERROR_SUCCESS : ERROR_GEN_FAILURE;
}


/*****************************************************************************/


DWORD PD_GetPartitionInfo(DWORD dwStoreId, LPCTSTR szPartitionName, PPD_PARTINFO info)
{
    DriverState *pState = (PDriverState) dwStoreId;
    PartState *pPartState;

    DEBUGMSG(ZONE_API,(L"MSPART!PD_GetPartitionInfo: dwStoreId=%08X, PartName=%s\n", dwStoreId, szPartitionName));
    if (!pState) {
        return ERROR_INVALID_PARAMETER;
    }    

    GetPartition(pState, &pPartState, NULL, szPartitionName);

    if (!pPartState) {
        return ERROR_NOT_FOUND;
    }

    wcscpy(info->szPartitionName, szPartitionName);

    info->snNumSectors = pPartState->snNumSectors;
    info->ftCreated = pPartState->ftCreation;
    info->ftLastModified = pPartState->ftModified;
    info->dwAttributes = pPartState->dwAttributes;
    info->bPartType = pPartState->partType;

    return ERROR_SUCCESS;
}


/*****************************************************************************/


DWORD PD_FindPartitionStart(DWORD dwStoreId, LPDWORD pdwSearchId)
{
    DriverState *pState = (PDriverState) dwStoreId;
    SearchState * pSearchState;
    SearchState * pTmpSearchState;

    DEBUGMSG(ZONE_API,(L"MSPART!PD_FindPartitionStart: dwStoreId=%08X\n", dwStoreId));

    if (!pState->bFormatState) { 
        return ERROR_BAD_FORMAT;
    }
    pSearchState = (PSearchState)LocalAlloc(LMEM_ZEROINIT, sizeof(SearchState));
    if (!pSearchState) {
        return ERROR_OUTOFMEMORY;
    }    
        
    pSearchState->pState = pState;

    // set up the search pState to return the first partition
    if (pState->pPartState) {
        // don't return the hidden partition
        if (pState->pPartState == pState->pHiddenPartState) {
            pSearchState->pPartState = pState->pHiddenPartState->pNextPartState;
        } else {
            pSearchState->pPartState = pState->pPartState;
        }    
    }

    pTmpSearchState = pState->pSearchState;

    // find the end of the linked list
    while(pTmpSearchState) {
        pTmpSearchState = pTmpSearchState->pNextSearchState;
    }    

    // add this search pState to the end
    if (pTmpSearchState)
        pTmpSearchState->pNextSearchState = pSearchState;
    else
        pState->pSearchState = pSearchState;
    *pdwSearchId = (DWORD)pSearchState;            
    return ERROR_SUCCESS;
}


/*****************************************************************************/


DWORD PD_FindPartitionNext(DWORD dwSearchId, PPD_PARTINFO info)
{
    SearchState *pSearchState = (PSearchState)dwSearchId;
    DWORD dwError = ERROR_SUCCESS;

    DEBUGMSG(ZONE_API,(L"MSPART!PD_FindPartitionNext dwSearchId=%08X\n", dwSearchId));

    if (pSearchState->pPartState == NULL) {
        return ERROR_NO_MORE_ITEMS;
    }

    dwError = PD_GetPartitionInfo((DWORD)pSearchState->pState, pSearchState->pPartState->cPartName, info);
    if (dwError == ERROR_SUCCESS) {
        // set up for the next search request - and don't return the hidden partition
        if ((pSearchState->pPartState->pNextPartState) &&
             (pSearchState->pPartState->pNextPartState == pSearchState->pState->pHiddenPartState))
        {    
            pSearchState->pPartState = pSearchState->pState->pHiddenPartState->pNextPartState;
        } else {
            pSearchState->pPartState = pSearchState->pPartState->pNextPartState;
        }    
    }

    return dwError;
}


/*****************************************************************************/


void PD_FindPartitionClose(DWORD dwSearchId)
{
    SearchState *pSearchState = (PSearchState)dwSearchId;
    SearchState * pNewSearchState;
    SearchState * pLastSearchState;

    DEBUGMSG(ZONE_API,(L"MSPART!PD_FindPartitionClose dwSearchId=%08X\n", dwSearchId));

    pNewSearchState = pSearchState->pState->pSearchState;
    pLastSearchState = NULL;

    // this should never happen, but just in case, we don't want to hang
    if (pNewSearchState == NULL)
        return;

    // find this search pState in the linked list
    while (pNewSearchState) {
        if (pNewSearchState == pSearchState)
            break;

        pLastSearchState = pNewSearchState;
        pNewSearchState = pNewSearchState->pNextSearchState;
    }

    // remove it from the linked list
    if (pLastSearchState) {
        pLastSearchState->pNextSearchState = pSearchState->pNextSearchState;
    } else {
        pSearchState->pState->pSearchState = NULL;
    }    

    LocalFree(pSearchState);

}


//--------------------------------------------------------------------------    
//
//  ATAPIMain - The Main Dll load entry point
//
//  Input:  DllInstance -  a Handle to the DLL. The value is the base address of the DLL. 
//              The HINSTANCE of a DLL is the same as the HMODULE of the DLL, so hinstDLL 
//              can be used in subsequent calls to the GetModuleFileName function and other 
//              functions that require a module handle. 
//
//          dwReason -  a flag indicating why the DLL entry-point function is being called. 
//                      This parameter can be one of the following values: 
//  Return: TRUE    -   ok. 
//          FALSE   -   error. 
//  
//  NOTES   Register Debug Zones only.
//          This function is an optional method of entry into a dynamic-link library (DLL). 
//          If the function is used, it is called by the system when processes and threads 
//          are initialized and terminated, or upon calls to the LoadLibrary and FreeLibrary 
//          functions. 
//          DllMain is a placeholder for the library-defined function name. 
//          You must specify the actual name you use when you build your DLL. 
//          For more information, see the documentation included with your development tools. 
//
//--------------------------------------------------------------------------    

BOOL WINAPI DllMain(HANDLE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls( (HMODULE)hInstance);
        DEBUGREGISTER((HINSTANCE)hInstance);
        break;
        
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

