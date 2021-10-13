//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <windows.h>
#include <diskio.h>
#include <partdrv.h>
#include "part.h"
#include "bpb.h"
#include "bootsec.h"
#include "drvdbg.h"

/*****************************************************************************
 *  ClearPartitionList - frees all existing partition structures
 *
 *  Input:  state - structure for this store
 *
 *  Output: state - updated to remove all references to PartState structures
 *                  and associated partition information
 *
 *  Return: none
 *
 *****************************************************************************/
void ClearPartitionList(DriverState *state)
{

    PartState * pNextPartState;
    PartState * pPartState;

    pNextPartState = state->pPartState;

    // deallocate the list of partition structures
    while (pNextPartState)
    {

        pPartState = pNextPartState;
        pNextPartState = pPartState->pNextPartState;
        LocalFree(pPartState);
    }

    state->pPartState = NULL;
    state->pHiddenPartState = NULL;
    state->snExtPartSector = 0;
    state->snExtPartEndSec = 0;
    state->dwNumParts = 0;

}

/*****************************************************************************
 *  CalculatePartitionSpace - calculates the amount of free space on the store
 *  and the largest contiguous section available for a partition.
 *
 *  Input:  state - structure for this store
 *
 *  Output: pFreeSpace - available space on the store
 *          pLargestPart - largest partition that can be created
 *
 *  Return: none
 *
 *****************************************************************************/
BOOL CalculatePartitionSpace(DriverState *state, SECTORNUM *pFreeSpace, SECTORNUM *pLargestPart)
{

    PartState    *pPartState;
    SECTORNUM       snLastPart;
    SECTORNUM       snFreeSpace;
    PBYTE           buffer = NULL;
    PPARTENTRY      tmpbuffer, tmpbuffer1;
    int             i;
    BOOL            bResult;

    *pLargestPart = (SECTORNUM)0;
    *pFreeSpace = (SECTORNUM)0;

    // first see if there's room in the extended partition
    if (state->snExtPartSector)
    {
        snLastPart = state->snExtPartSector;
        pPartState = state->pPartState;

        // find the first partition within the extended partition
        for (;pPartState != NULL; pPartState = pPartState->pNextPartState)
        {
            if (pPartState->snPBRSector == 0)
                continue;

            break;
        }

        if (pPartState)
        {
            while (pPartState)
            {
                // only traverse the partitions within the extended partition
                if (pPartState->snPBRSector == 0)
                    break;

                snFreeSpace = pPartState->snPBRSector - snLastPart;
                *pFreeSpace += snFreeSpace;

                // add the overhead for an extended partition before calculating the available size for a partition
                if (snFreeSpace)
                {
                    if (state->diskInfo.di_heads > 1)
                        snFreeSpace -= state->diskInfo.di_sectors;
                    else
                        snFreeSpace--;
                }

                if (snFreeSpace > *pLargestPart)
                    *pLargestPart = snFreeSpace;

                snLastPart = pPartState->snStartSector + pPartState->snNumSectors;
                pPartState = pPartState->pNextPartState;
            }

            // add in any of the extended partition that's not used
            if (state->snExtPartEndSec > snLastPart) {
                snFreeSpace = state->snExtPartEndSec - snLastPart +1;
                *pFreeSpace += snFreeSpace;
            }    

            // add the overhead for an extended partition before calculating the available size for a partition
            if (snFreeSpace)
            {
                if ((state->diskInfo.di_heads > 1) && (snFreeSpace >= state->diskInfo.di_sectors))
                    snFreeSpace -= state->diskInfo.di_sectors;
                else
                    snFreeSpace--;
            }

            if (snFreeSpace > *pLargestPart)
                *pLargestPart = snFreeSpace;
        }
        else
        {
            // the hidden partition is empty
            *pFreeSpace = state->snExtPartEndSec - state->snExtPartSector +1;

            if (state->diskInfo.di_heads > 1)
                *pLargestPart = *pFreeSpace - state->diskInfo.di_sectors;
            else
                *pLargestPart = *pFreeSpace - 1;

        }
    }

    // now check for room in the main partition table
    buffer = NULL;
    bResult = ReadSectors (state, 0, 1, &buffer);
    if (!bResult)
        return FALSE;

    tmpbuffer  = (PPARTENTRY)(buffer + PARTTABLE_OFFSET);
    tmpbuffer1 = (PPARTENTRY)(buffer + PARTTABLE_OFFSET + sizeof(PARTENTRY));

    // the MBR is full can't add anything outside of the extended partition anyway
    if (tmpbuffer[3].Part_TotalSectors)
    {
        LocalFree(buffer);
        return TRUE;
    }

    // check for a gap at the beginning of the table
    if (tmpbuffer->Part_FirstTrack > 1)
    {
        if (state->diskInfo.di_heads > 1)
        {
            // adjust for the reserved MBR track
            snFreeSpace = tmpbuffer->Part_StartSector - state->diskInfo.di_sectors;
            // adjust in case the partition doesn't start at head 0
            snFreeSpace -= tmpbuffer->Part_FirstHead * state->diskInfo.di_sectors;
        }
        else
            snFreeSpace = tmpbuffer->Part_StartSector - 1;

        *pFreeSpace += snFreeSpace;
        if (snFreeSpace > *pLargestPart)
            *pLargestPart = snFreeSpace;
    }


    for (i=0; i<4; i++, tmpbuffer++, tmpbuffer1++)
    {
        if (tmpbuffer->Part_TotalSectors == 0)
            break;

        // calculate the space between two partitions
        if (tmpbuffer->Part_TotalSectors && tmpbuffer1->Part_TotalSectors)
            snFreeSpace = tmpbuffer1->Part_StartSector - (tmpbuffer->Part_StartSector + tmpbuffer->Part_TotalSectors);
        else
            // calculate space from the last partition to the end of the drive
            snFreeSpace = state->diskInfo.di_total_sectors - (tmpbuffer->Part_StartSector + tmpbuffer->Part_TotalSectors);

        *pFreeSpace += snFreeSpace;

        if (snFreeSpace > *pLargestPart)
            *pLargestPart = snFreeSpace;

    }

    LocalFree(buffer);
    return TRUE;
}

/*****************************************************************************
 *  GenerateFATFSType - generates a FATxx partition type based upon the size
 *  of the partition.  This algorithm is a duplicate of the code found in
 *  FATFS.
 *
 *  Input:  state - structure for this store
 *          snNumSectors - size of the partition
 *          pFileSys - pointer to the partition type
 *
 *  Output: pFileSys is updated with the partition type
 *
 *  Return: none
 *
 *****************************************************************************/
void GenerateFATFSType(DriverState *state, SECTORNUM snNumSectors, PBYTE pFileSys)
{
// TODO: Verify that we support the LBA modes correctly
    DWORD   dwSectorCount;
    DWORD   dwBytesPerSector;
    int     cLoops;
    DWORD   csecReserved, csecClus;
    DWORD   csecFATs, csecFATsLast;
    DWORD   csecRoot, csecData, cclusData;
    DWORD   cRootEntries, cbitsClus;

    dwBytesPerSector = state->diskInfo.di_bytes_per_sect;
    dwSectorCount = (DWORD)snNumSectors;

    // assumptions for FAT layout
    csecClus = 1;
    cRootEntries = 256;
    csecReserved = 1;

    // this algorithm was lifted out of format.c in the fatfs code - we need to generate partition types
    //  the same way FATFS does

restart:

    do
    {
        cLoops = 4;
        csecFATs = csecRoot = 0;

        do
        {
            csecData = dwSectorCount - csecReserved - csecFATs - csecRoot;

            do
            {
                cclusData = csecData / csecClus;

                if (csecClus > (DWORD)(MAX_CLUS_SIZE/dwBytesPerSector)) {

                    dwSectorCount = (FAT32_BAD-1)*(MAX_CLUS_SIZE/dwBytesPerSector);
                    goto restart;
                }

                if (cclusData < FAT1216_THRESHOLD)
                {
                    cbitsClus = 12; // Should use a 12-bit FAT
                    break;
                }


                if (cclusData < FAT1632_THRESHOLD)
                {
                    cbitsClus = 16; // Should use a 16-bit FAT
                    break;
                }

                // So, at the current cluster size, a 32-bit FAT is the only answer;
                // however, we won't settle on that as long as the cluster size is still small
                // (less than 4K).

                if (csecClus < (DWORD)(4096/dwBytesPerSector))
                {
                    csecClus *= 2;      // grow the cluster size
                }
                else
                {   
                    cbitsClus = 32;
                    csecReserved = 2;   // why only 2? see "minimalist" comments below...
                    break;
                }

            } while (TRUE);

            // NOTE: when computing csecFATs, we must add DATA_CLUSTER (2) to cclusData, because
            // the first two entries in a FAT aren't real entries, but we must still account for them.

            csecFATsLast = csecFATs;
            csecFATs = (((cclusData+DATA_CLUSTER) * cbitsClus + 7)/8 + dwBytesPerSector-1) / dwBytesPerSector;

            if (cbitsClus == 32)        // csecRoot comes into play only on non-FAT32 volumes
                csecRoot = 0;
            else
                csecRoot = (sizeof(DIRENTRY) * cRootEntries + dwBytesPerSector-1) / dwBytesPerSector;

        } while (csecFATs != csecFATsLast && --cLoops);

        if (cLoops)
            break;

        // We couldn't get a consistent answer, so chop off one cluster

        dwSectorCount -= csecClus;

    } while (TRUE);


    if (cbitsClus == 12)
        *pFileSys = PART_DOS2_FAT;
    else if (cbitsClus == 32)
        *pFileSys = PART_DOS32;
    else if (dwSectorCount < 65536)
        *pFileSys = PART_DOS3_FAT;
    else
        *pFileSys = PART_DOS4_FAT;

}

/*****************************************************************************
 *  GeneratePartitionChecksum - generates the checksum for the OnDiskPartState
 *  structures contained in the buffer.  The number of partitions to checksum
 *  is contained within the buffer as well as the existing checksum.
 *
 *  Input:  state - structure for this store
 *          buffer - pointer to the buffer containing the partition data
 *          pChecksum - pointer to the checksum
 *
 *  Output: pChecksum is updated with the new checksum
 *
 *
 *  Return: TRUE if the checksum agrees with the on media checksum, FALSE if not
 *
 *****************************************************************************/
BOOL GeneratePartitionChecksum(DriverState *state, PBYTE buffer, DWORD *pChecksum)
{

    DWORD   dwChecksum = 0;
    DWORD   *tmpbuffer;
    int     numEntries = 0;

    if (pChecksum)
        *pChecksum = 0;

    tmpbuffer = (DWORD *)(buffer + STORESTART);

    numEntries = ((DriverState *)tmpbuffer)->dwNumParts;

    if (!numEntries)
        return TRUE;

    // this is the max # of partitions we'll hold, if the count is larger, the sector is hosed
    if (numEntries > HIDDEN_PART_SEC_COUNT*3)
        return FALSE;

    numEntries *= sizeof(OnDiskPartState);

    // # DWORDS to checksum
    numEntries >>= 2;

    tmpbuffer = (DWORD *)(buffer + STORESTART + offsetof(DriverState, hStore));

    // simple DWORD checksum - don't worry about overflow
    while (numEntries)
    {
        dwChecksum += tmpbuffer[numEntries];
        numEntries --;
    }
    
    if (pChecksum)
        *pChecksum = dwChecksum;

    tmpbuffer = (DWORD *)(buffer + STORESTART);

    if (((DriverState *)tmpbuffer)->dwChecksum == dwChecksum)
        return TRUE;
    else
        return FALSE;

}

/*****************************************************************************
 *  CreatePBR - creates a partition boot record within the extended partition
 *  and links it to the previous and next partition if they apply.
 *
 *  Input:  state - structure for this store
 *          snLastPBRSec - LBA of the PBR that points to this PBR
 *          snNextPBRSec - LBA of the PBR that this PBR will point to
 *          snPBRSector - LBA of this PBR
 *          snNumSectors - number of sectors for this partition
 *          fileSysType -  partition type of this partition
 *
 *  Output: none
 *
 *  Return: TRUE if PBR created successfully, FALSE if not
 *
 *****************************************************************************/
BOOL CreatePBR(DriverState *state, SECTORNUM snLastPBRSec, SECTORNUM snNextPBRSec, SECTORNUM snPBRSector, SECTORNUM snNumSectors, BYTE fileSysType)
{

    BOOL        bResult;
    PBYTE       buffer=NULL, buffer1=NULL;
    PPARTENTRY  tmpbuffer, tmpbuffer1;
    SECTORNUM   snDataStart;

    // the partition data starts at the next head location
    if (state->diskInfo.di_heads > 1)
        snDataStart = snPBRSector + state->diskInfo.di_sectors;
    else
        snDataStart = snPBRSector + 1;

    buffer = (PBYTE)LocalAlloc(LMEM_ZEROINIT, state->diskInfo.di_bytes_per_sect);
    if (!buffer)
        return FALSE;

    // set the trailer signature for the EBR
    *(WORD *)(buffer + BOOT_SIGNATURE) = BOOTSECTRAILSIGH;
    tmpbuffer = (PPARTENTRY)(buffer + PARTTABLE_OFFSET);

    // build up the entry for the new PBR first
    bResult = ConvertLBAtoCHS (state, snDataStart, &tmpbuffer->Part_FirstTrack, &tmpbuffer->Part_FirstHead, &tmpbuffer->Part_FirstSector);
    bResult = ConvertLBAtoCHS (state, snPBRSector + snNumSectors -1, &tmpbuffer->Part_LastTrack, &tmpbuffer->Part_LastHead, &tmpbuffer->Part_LastSector);
    tmpbuffer->Part_FileSystem = fileSysType;

    // start sector of the data area is relative to the beginning of the logical drive
    if (state->diskInfo.di_heads > 1)
        tmpbuffer->Part_StartSector = state->diskInfo.di_sectors;
    else
        tmpbuffer->Part_StartSector = 1;

    // sector count does not include the PBR sector(s)
    tmpbuffer->Part_TotalSectors =(DWORD)snNumSectors - tmpbuffer->Part_StartSector;

    if (snNextPBRSec)
    {
        buffer1 = NULL;
        bResult = ReadSectors (state, snNextPBRSec, 1, &buffer1);
        if (!bResult)
        {
            LocalFree(buffer);
            return FALSE;
        }

        tmpbuffer1 = (PPARTENTRY)(buffer + PARTTABLE_OFFSET);

        // if we're hooking in between two logical drives, we need an entry in this PBR to point to the next one
        tmpbuffer++;

        // start sector of the data area is relative to the beginning of the logical drive
        tmpbuffer->Part_StartSector = (DWORD)(snNextPBRSec - state->snExtPartSector);

        // sector count comes from the length of the next logical drive
        tmpbuffer->Part_TotalSectors = tmpbuffer1->Part_StartSector + tmpbuffer1->Part_TotalSectors;

        bResult = ConvertLBAtoCHS (state, snNextPBRSec, &tmpbuffer->Part_FirstTrack, &tmpbuffer->Part_FirstHead, &tmpbuffer->Part_FirstSector);
        bResult = ConvertLBAtoCHS (state, snNextPBRSec + tmpbuffer->Part_TotalSectors -1, &tmpbuffer->Part_LastTrack, &tmpbuffer->Part_LastHead, &tmpbuffer->Part_LastSector);

        // the file system type for the next logical drive is always the extended partition type, not the actual FS type
        tmpbuffer->Part_FileSystem = PART_EXTENDED;

        LocalFree(buffer1);
    }

    bResult = WriteSectors(state, snPBRSector, 1, buffer);
    LocalFree(buffer);

    if (!bResult)
        return FALSE;

    // We have successfully written out the new PBR and (possibly) linked it to the next one in the chain
    // The last thing that needs to be done is hook this into the previous PBR if it exists

    if (!snLastPBRSec)
        return TRUE;

    buffer = NULL;
    bResult = ReadSectors (state, snLastPBRSec, 1, &buffer);
    if (!bResult)
        return FALSE;

    tmpbuffer = (PPARTENTRY)(buffer + PARTTABLE_OFFSET + sizeof(PARTENTRY));

    bResult = ConvertLBAtoCHS (state, snPBRSector, &tmpbuffer->Part_FirstTrack, &tmpbuffer->Part_FirstHead, &tmpbuffer->Part_FirstSector);
    bResult = ConvertLBAtoCHS (state, snPBRSector + snNumSectors -1, &tmpbuffer->Part_LastTrack, &tmpbuffer->Part_LastHead, &tmpbuffer->Part_LastSector);
    tmpbuffer->Part_FileSystem = PART_EXTENDED;

    // start sector of the data area is relative to the beginning of the logical drive
    tmpbuffer->Part_StartSector = (DWORD)(snPBRSector - state->snExtPartSector);

    // sector count comes from the length of the next logical drive and includes the PBR track
    tmpbuffer->Part_TotalSectors = (DWORD)snNumSectors;

    bResult = WriteSectors(state, snLastPBRSec, 1, buffer);
    LocalFree(buffer);

    if (!bResult)
        return FALSE;

    return TRUE;

}

/*****************************************************************************
 *  Delete PBR - deletes the partition boot record for the partition and updates
 *  the previous PBR if one exists.
 *
 *  Input:  state - structure for this store
 *          pPartState - partition structure for the PBR to delete
 *          pPrevPartState - partition structure for the partition that preceeds
 *                           the partition being deleted
 *
 *  Output: none
 *
 *
 *  Return: TRUE if PBR successfully deleted, FALSE if not
 *
 *****************************************************************************/
BOOL DeletePBR(DriverState *state, PartState *pPartState, PartState *pPrevPartState)
{

    
    BOOL        bResult;
    PBYTE       buffer = NULL, buffer1 = NULL;
    PPARTENTRY  tmpbuffer, tmpbuffer1;

    // first get the PBR sector for the logical drive that's being deleted
    bResult = ReadSectors (state, pPartState->snPBRSector, 1, &buffer);
    if (!bResult)
    {
        LocalFree(buffer);
        return FALSE;
    }

    // if this logical drive follows another logical drive, the previous PBR needs to be updated
    if (pPrevPartState && pPrevPartState->snPBRSector)
    {
        buffer1 = NULL;
        bResult = ReadSectors (state, pPrevPartState->snPBRSector, 1, &buffer1);
        if (!bResult)
        {
            LocalFree(buffer);
            return FALSE;
        }

        // copy the partition entry that points to the next logical drive (from the PBR being deleted)
        //  and update the previous PBR to point to it
        tmpbuffer = (PPARTENTRY)(buffer + PARTTABLE_OFFSET + sizeof(PARTENTRY));
        tmpbuffer1 = (PPARTENTRY)(buffer1 + PARTTABLE_OFFSET + sizeof(PARTENTRY));
        memcpy(tmpbuffer1, tmpbuffer, sizeof(PARTENTRY));

        // update the previous PBR - which removes the link to the part being deleted
        bResult = WriteSectors(state, pPrevPartState->snPBRSector, 1, buffer1);
        LocalFree(buffer1);

        if (!bResult)
        {
            LocalFree(buffer);
            return FALSE;
        }

        // zero out the old PBR so it isn't mistakenly used as a PBR anymore by some ill behaved code
        memset(buffer, 0, state->diskInfo.di_bytes_per_sect);
        bResult = WriteSectors(state, pPartState->snPBRSector, 1, buffer);

    }
    else
    {
        // this is the first logical drive in the extended partition - all that needs to be done is zero
        //  the entry for this logical drive, the pointer to the next logical drive remains in tact
        //  at part entry 2

        tmpbuffer = (PPARTENTRY)(buffer + PARTTABLE_OFFSET);
        memset(tmpbuffer, 0, sizeof(PARTENTRY));

        bResult = WriteSectors(state, pPartState->snPBRSector, 1, buffer);
    }
    

    LocalFree(buffer);
    return bResult;
}


/*****************************************************************************
 *  DeleteMBR - deletes a partition entry in the master boot record - all
 *  partition records that follow the one to be deleted are shifted back 1
 *  entry so no holes exist in the MBR.
 *
 *  Input:  state - structure for this store
 *          pPartState - partition structure for the partition to delete
 *
 *  Output: none
 *
 *
 *  Return: TRUE if MBR updated successfully, FALSE if not
 *
 *****************************************************************************/
BOOL DeleteMBR(DriverState *state, PartState *pPartState)
{

    PBYTE       buffer = NULL;
    PPARTENTRY  tmpbuffer, tmpbuffer1;
    BOOL        bResult;
    int         i, partNum, partIndex = -1;

    bResult = ReadSectors (state, 0, 1, &buffer);
    if (!bResult)
        return FALSE;

    for (i = 0, partNum = 3, tmpbuffer = (PPARTENTRY)(buffer + PARTTABLE_OFFSET); i < 4; i++, tmpbuffer++) {
        if (tmpbuffer->Part_StartSector == pPartState->snStartSector)
            partIndex = i;

        if (tmpbuffer->Part_TotalSectors == 0)
        {
            partNum = i - 1;
            break;
        }
    }

    if ((partIndex == -1 || partNum == -1))
    {
        // for some reason we didn't find the partition we're trying to delete - this should never happen
        LocalFree(buffer);
        SetLastError(ERROR_GEN_FAILURE);
        return FALSE;
    }

    tmpbuffer = (PPARTENTRY)(buffer + PARTTABLE_OFFSET + (partIndex * sizeof(PARTENTRY)));
    tmpbuffer1 = (PPARTENTRY)(buffer + PARTTABLE_OFFSET + ((partIndex +1) * sizeof(PARTENTRY)));

    // remove this partition entry by shifting all the others up in the table
    for (; partIndex < partNum; partIndex++)
    {
        memcpy(tmpbuffer, tmpbuffer1, sizeof(PARTENTRY));
        tmpbuffer++;
        tmpbuffer1++;
    }

    // set the last entry to 0's
    memset (tmpbuffer, 0, sizeof(PARTENTRY));

    bResult = WriteSectors(state, 0, 1, buffer);
    LocalFree(buffer);

    return bResult;

}

/*****************************************************************************
 *  WriteMBR - adds a partition entry to the master boot record. The partition
 *  table is traversed to find the placement for this partition.  Entries within
 *  the MBR are sorted by the start sector.
 *
 *  Input:  state - structure for this store
 *          snSectorNum - start sector number for this partition
 *          snNumSectors - number of sectors in this partition
 *          fileSysType - partition type
 *          bCreateMBR - TRUE to generate a new MBR, FALSE to add to the
 *                       existing MBR
 *
 *  Output: none
 *
 *  Return: TRUE if successfully written, FALSE for failures, ERROR_DISK_FULL
 *          will be set if no room in the MBR
 *
 *****************************************************************************/
BOOL WriteMBR(DriverState *state, SECTORNUM snSectorNum, SECTORNUM snNumSectors, BYTE fileSysType, BOOL bCreateMBR)
{

    PBYTE       buffer = NULL;
    PPARTENTRY  tmpbuffer, tmpbuffer1;
    BOOL        bResult;
    int         i, partIndex, partNum = -1;

    // don't create the MBR, it already exists so use it
    if (!bCreateMBR)
    {
        bResult = ReadSectors (state, 0, 1, &buffer);
        if (!bResult)
            return FALSE;

        for (i = 0, partIndex = 0, tmpbuffer = (PPARTENTRY)(buffer + PARTTABLE_OFFSET); i < 4; i++, tmpbuffer++)
        {
            if (tmpbuffer->Part_TotalSectors == 0)
            {
                partNum = i;
                break;
            }

            // find the index of the partition located just before this one
            if (snSectorNum > tmpbuffer->Part_StartSector)
                partIndex = i + 1;
        }

        if (partNum == -1)
        {
            // we return this error code so the caller can tell that there's no room in the MBR
            LocalFree(buffer);
            SetLastError(ERROR_DISK_FULL);
            return FALSE;
        }

        // these indexes would be equal if we are adding to the end of the table
        if (partIndex != partNum)
        {
            // this partition needs to be added in the order that it appears on the disk - so this may involve
            //  shifting some of the existing partition entries to open up the partition entry where this belongs
            tmpbuffer = (PPARTENTRY)(buffer + PARTTABLE_OFFSET + (partNum * sizeof(PARTENTRY)));
            tmpbuffer1 = (PPARTENTRY)(buffer + PARTTABLE_OFFSET + ((partNum -1) * sizeof(PARTENTRY)));

            for (; partNum > partIndex; partNum--)
            {
                memcpy(tmpbuffer, tmpbuffer1, sizeof(PARTENTRY));
                tmpbuffer--;
                tmpbuffer1--;
            }
        }
    }
    else
    {
        buffer = (PBYTE)LocalAlloc(LMEM_ZEROINIT, state->diskInfo.di_bytes_per_sect);
        if (!buffer)
            return FALSE;

        // add header and trailer info to the MBR
        *(WORD *)(buffer + BOOT_SIGNATURE) = BOOTSECTRAILSIGH;
        buffer[0] = 0xE9;
        buffer[1] = 0xfd;
        buffer[2] = 0xff;
        partNum = 0;
    }

    tmpbuffer = (PPARTENTRY)(buffer + PARTTABLE_OFFSET + partNum * sizeof(PARTENTRY));

    // create the partition entry for the extended partition
    bResult = ConvertLBAtoCHS (state, snSectorNum, &tmpbuffer->Part_FirstTrack, &tmpbuffer->Part_FirstHead, &tmpbuffer->Part_FirstSector);

    tmpbuffer->Part_FileSystem = fileSysType;
    tmpbuffer->Part_StartSector = (DWORD)snSectorNum;
    tmpbuffer->Part_TotalSectors = (DWORD)snNumSectors;

    bResult = ConvertLBAtoCHS (state, tmpbuffer->Part_StartSector + tmpbuffer->Part_TotalSectors -1, &tmpbuffer->Part_LastTrack, &tmpbuffer->Part_LastHead, &tmpbuffer->Part_LastSector);

    bResult = WriteSectors(state, 0, 1, buffer);
    LocalFree(buffer);

    return bResult;

}


/*****************************************************************************
 *  WriteSectors - calls the device driver to write out data to the media
 *
 *  Input:  state - structure for this store
 *          snSectorNum - starting LBA sector to write to
 *          snNumSectors - number of sectors to write
 *          pBuffer - data buffer
 *
 *  Output: none
 *
 *  Return: TRUE if successfully written, FALSE if not
 *
 *****************************************************************************/
BOOL WriteSectors(DriverState *state, SECTORNUM snSectorNum, SECTORNUM snNumSectors, PBYTE pBuffer)
{
    
    SG_REQ          sgReq;
    DWORD           dummy;
    BOOL            bResult;

    bResult = FALSE;

    sgReq.sr_start            = (DWORD)snSectorNum;
    sgReq.sr_num_sec          = (DWORD)snNumSectors;
    sgReq.sr_num_sg           = 1;
    sgReq.sr_status           = 0;
    sgReq.sr_callback         = NULL;
    sgReq.sr_sglist[0].sb_buf = pBuffer;
    sgReq.sr_sglist[0].sb_len = (DWORD)snNumSectors * state->diskInfo.di_bytes_per_sect;

    bResult = DeviceIoControl(state->hStore, DISK_IOCTL_WRITE, &sgReq, sizeof(SG_REQ), NULL, 0, &dummy, NULL);

    return bResult;
}


/*****************************************************************************
 *  ReadSectors - allocates a buffer and calls the device driver to read the media.
 *
 *  Input:  state - structure for this store
 *          snSectorNum - starting LBA sector to read
 *          snNumSectors - number of sectors to read
 *          pBuffer - pointer to the data buffer pointer
 *
 *
 *  Output: pBuffer updated to allocated buffer
 *
 *  Return: TRUE if successfully read, FALSE if not
 *
 *****************************************************************************/
BOOL ReadSectors(DriverState *state, SECTORNUM snSectorNum, SECTORNUM snNumSectors, PBYTE *pBuffer)
{
    
    PBYTE           buffer = NULL;
    DWORD           bufferSize;
    SG_REQ          sgReq;
    DWORD           dummy;
    BOOL            bResult;

    bResult = FALSE;

    bufferSize = (DWORD)snNumSectors * state->diskInfo.di_bytes_per_sect;
    if (*pBuffer) {
        buffer = *pBuffer;
    } else {
        buffer     = (PBYTE)LocalAlloc(LMEM_ZEROINIT, bufferSize);
    }    
    if (!buffer)
        return FALSE;

    sgReq.sr_start            = (DWORD)snSectorNum;
    sgReq.sr_num_sec          = (DWORD)snNumSectors;
    sgReq.sr_num_sg           = 1;
    sgReq.sr_status           = 0;
    sgReq.sr_callback         = NULL;
    sgReq.sr_sglist[0].sb_buf = buffer;
    sgReq.sr_sglist[0].sb_len = bufferSize;

    bResult = DeviceIoControl(state->hStore, DISK_IOCTL_READ, &sgReq, sizeof(SG_REQ), NULL, 0, &dummy, NULL);
    if (!*pBuffer) {
        if (bResult)
            *pBuffer = buffer;
        else
            LocalFree(buffer);
    }        

    return bResult;
}


/*****************************************************************************
 *  GetPartLocation - finds the first location on the store where the partition
 *  will fit.  The algorithm first checks the extended partition to see if it
 *  can add one there.  If not, then it checks for room in the MBR and adds
 *  the partition if there's room.  Checks are made to ensure that the placement
 *  of the partition does not cross either the starting or ending extended
 *  partition boundaries.  The partition must be completely continued within
 *  the extended partition or outside the extended partition.  This means that
 *  the disk could be considered 'full' when the free sector count would accomodate
 *  the partition size, but it can't fit because those sectors straddle an extended
 *  partition boundary.  However, since this driver needs to support > 4 partitions,
 *  the only way to accomplish this is through the use of the extended partition.
 *
 *
 *  Input:  state - structure for this store
 *          numSectors - requested size of the partition
 *          pStartSector - LBA sector to start the partition
 *
 *  Output: ppPartState - points to the partition structure for the partition
 *              previous to this one on the media
 *          pPBRSector - LBA sector location of the partition boot record if
 *              placed in the extended partition
 *          pNumSectors - allocated size of the partition, this is requested size
 *              adjusted for track and cylinder boundaries
 *
 *  Return: TRUE if a location could be found, FALSE if not
 *
 *****************************************************************************/
BOOL GetPartLocation (DriverState * state, SECTORNUM numSectors, PartState **ppPartState, SECTORNUM *pStartSector, SECTORNUM *pPBRSector, SECTORNUM *pNumSectors)
{
    PartState    *pPartState, *pLastPartState;
    SECTORNUM       snGap = 0;
    SECTORNUM       snNextPartSector;
    SECTORNUM       curNumSectors;
    PBYTE           buffer = NULL;
    PPARTENTRY      tmpbuffer, tmpbuffer1;
    BOOL            bExtPart, bResult;
    BOOL            bLocFound = FALSE;
    int             i;

    pPartState = state->pPartState;
    pLastPartState = NULL;

    // default in case there are no existing user partitions
    snNextPartSector = state->snExtPartSector;

    *pNumSectors = numSectors;
    *pStartSector = 0;

    // adjust the number of sectors so the data ends on a track boundary
    if (state->diskInfo.di_heads > 1)
        if (numSectors < state->diskInfo.di_sectors)
            *pNumSectors = state->diskInfo.di_sectors;
        else
            if (numSectors % state->diskInfo.di_sectors)
                *pNumSectors = (numSectors / state->diskInfo.di_sectors +1) * state->diskInfo.di_sectors;

    // calculate the number of sectors we need if this is placed in an extended partition
    // in an extended partition - adjust the sector count to include a track for the PBR
    //  and be sure the partition ends on a cylinder boundary
    curNumSectors = *pNumSectors;
    if (state->diskInfo.di_heads > 1)
    {
        curNumSectors += state->diskInfo.di_sectors;
        if (curNumSectors % (state->diskInfo.di_sectors * state->diskInfo.di_heads))
            curNumSectors = (curNumSectors / (state->diskInfo.di_sectors * state->diskInfo.di_heads)+1) * (state->diskInfo.di_sectors * state->diskInfo.di_heads);
    }
    else
        curNumSectors += 1;

    // first see if there are any partitions to begin with, this may be the first one
    if (!state->pPartState)
    {
        // if no partitions exist, we should be adding this to the extended partition because we should have created one
        bExtPart = TRUE;
        bLocFound = TRUE;
    }

    if ((!bLocFound) && snNextPartSector)
    {
        // see if there's room for this partition in the extended partition - the first available opening is used
        while (pPartState)
        {
            // there could be partitions outside of the extended partition - skip any prior to the extended partition
            if (pPartState->snPBRSector == 0)
            {
                // we've reached the end of the extended partitions
                if (pLastPartState && pLastPartState->snPBRSector)
                    break;

                pLastPartState = pPartState;
                pPartState = pPartState->pNextPartState;
                continue;
            }

            snGap = pPartState->snPBRSector - snNextPartSector;

            if (snGap >= curNumSectors)
            {
                bLocFound = TRUE;
                bExtPart = TRUE;
                break;
            }

            snNextPartSector = pPartState->snStartSector + pPartState->snNumSectors;
            pLastPartState = pPartState;
            pPartState = pPartState->pNextPartState;
        }

        // we didn't find any gaps that fit
        if (!bLocFound)
        {
            if (snNextPartSector == state->snExtPartSector)
            {
                // the extended partition is empty - see if the new partition will fit
                if ((state->snExtPartEndSec - snNextPartSector + 1) >= curNumSectors)
                {
                    bExtPart = TRUE;
                    bLocFound = TRUE;
                    pLastPartState = state->pPartState;

                    // the extended partition may come 1st on the media - if that's the case, this will be the first
                    //      partition in the list
                    if (pLastPartState->snStartSector > snNextPartSector)
                        pLastPartState = NULL;
                    else
                    {
                        while (TRUE)
                        {
                            // end of the list, use it
                            if (!pLastPartState->pNextPartState)
                                    break;

                                if (pLastPartState->pNextPartState->snStartSector > snNextPartSector)
                                    break;

                                pLastPartState = pLastPartState->pNextPartState;
                        }
                    }
                }

            }
            else
            {
                // see if the partition will fit at the end
                snGap = state->snExtPartEndSec - snNextPartSector +1;

                if (snGap >= curNumSectors)
                {
                    bLocFound = TRUE;
                    bExtPart = TRUE;
                }

            }
        }
    }

    buffer = NULL;
    
    while (!bLocFound)
    {
        // there was no room in the extended partition, so try the MBR to see if there's room
        bResult = ReadSectors (state, 0, 1, &buffer);
        if (!bResult)
            return FALSE;

        tmpbuffer = (PPARTENTRY)(buffer + PARTTABLE_OFFSET);
        tmpbuffer1 = (PPARTENTRY)(buffer + PARTTABLE_OFFSET + sizeof(PARTENTRY));

        // the MBR is full can't add anything outside of the extended partition anyway
        if (tmpbuffer[3].Part_TotalSectors)
            break;

        // calculate the number of sectors we need if this is placed in an MBR partition
        // don't need the extra track for the PBR since we're using the MBR but it does need to fit within
        //  cylinder boundaries
        curNumSectors = *pNumSectors;
        if (state->diskInfo.di_heads > 1)
        {
            if (curNumSectors % (state->diskInfo.di_sectors * state->diskInfo.di_heads))
                curNumSectors = (curNumSectors / (state->diskInfo.di_sectors * state->diskInfo.di_heads)+1) * (state->diskInfo.di_sectors * state->diskInfo.di_heads);
        }

        snGap = 0;

        // check for a gap at the beginning of the table
        if (tmpbuffer->Part_FirstTrack > 1)
        {
            if (state->diskInfo.di_heads > 1)
            {
                snGap = tmpbuffer->Part_StartSector - state->diskInfo.di_sectors;

                // adjust in case the partition doesn't start at head 0
                snGap -= tmpbuffer->Part_FirstHead * state->diskInfo.di_sectors;
                snNextPartSector = state->diskInfo.di_sectors;
            }
            else
            {
                snGap = tmpbuffer->Part_StartSector - 1;
                snNextPartSector = 1;
            }

            if (snGap < (curNumSectors - snNextPartSector))
                snGap = 0;
            else
                // the total sectors for the first partition doesn't include the MBR track
                curNumSectors -= snNextPartSector;
        }

        for (i=0; i<4; i++, tmpbuffer++, tmpbuffer1++)
        {
            if (tmpbuffer->Part_TotalSectors == 0)
                break;

            // calculate the space between two partitions
            if (!snGap)
            {
                if (tmpbuffer->Part_TotalSectors && tmpbuffer1->Part_TotalSectors)
                    snGap = tmpbuffer1->Part_StartSector - (tmpbuffer->Part_StartSector + tmpbuffer->Part_TotalSectors);
                else
                    // calculate space from the last partition to the end of the drive
                    snGap = state->diskInfo.di_total_sectors - (tmpbuffer->Part_StartSector + tmpbuffer->Part_TotalSectors);

                // beginning of the next partition - MBR partitions always start at head 0 sector 1 which should be
                //  one sector > the end sector of the previous partition
                snNextPartSector = tmpbuffer->Part_StartSector + tmpbuffer->Part_TotalSectors;
            }

            if (snGap >= curNumSectors)
            {
                bLocFound = TRUE;
                bExtPart = FALSE;

                // get the corresponding pPartState structure
                pLastPartState = NULL;
                pPartState = state->pPartState;
                while (pPartState)
                {
                    if (pPartState->snStartSector > snNextPartSector)
                        break;

                    pLastPartState = pPartState;
                    pPartState = pPartState->pNextPartState;
                }

                break;
            }

            snGap = 0;
        }

        break;
    }

    if (buffer)
        LocalFree(buffer);

    if (!bLocFound)
    {
        SetLastError(ERROR_DISK_FULL);
        return FALSE;
    }


    *pNumSectors = curNumSectors;
    *ppPartState = pLastPartState;

    if (bExtPart)
    {
        *pPBRSector = snNextPartSector;

        if (state->diskInfo.di_heads > 1)
            *pStartSector = snNextPartSector + state->diskInfo.di_sectors;
        else
            *pStartSector = snNextPartSector + 1;
    }
    else
    {
        *pPBRSector = 0;
        *pStartSector = snNextPartSector;
    }

    return TRUE;

}

/*****************************************************************************
 *  ConvertLBAtoCHS - converts the LBA sector number to cylinder, head, sector
 *  geometry that's used for the partition table entries.
 *
 *  Input:  state - structure for this store
 *          snSectorNum - LBA to convert
 *
 *  Output: pCyl - cylinder of the LBA
 *          pSector - sector of the LBA
 *          pHead - head of the LBA
 *
 *  Return: FALSE if snSectorNum out of range, else TRUE
 *
 *****************************************************************************/
BOOL ConvertLBAtoCHS (DriverState * state, SECTORNUM snSectorNum, PBYTE pCyl, PBYTE pHead, PBYTE pSector)
{

    DWORD sectorNum;
    DWORD head, cyl, sector, sectorsPerCyl;

    sectorNum = (DWORD)snSectorNum;

    // the sectorNum is zero based, this takes that sectorNum and turns it into CHS and the
    // sector is one based

    // be sure the requested sector is within the capacity of the drive
    if (sectorNum > state->diskInfo.di_total_sectors)
        return FALSE;

    sectorsPerCyl = state->diskInfo.di_heads * state->diskInfo.di_sectors;

    cyl = sectorNum / sectorsPerCyl;

    head = (sectorNum-(cyl * sectorsPerCyl)) / state->diskInfo.di_sectors;

    sector = sectorNum - ((cyl * sectorsPerCyl) + (head * state->diskInfo.di_sectors)) + 1;

    *pHead = (BYTE)head;
    *pCyl = (BYTE)cyl;

    // the sector # is actually 6 bits
    *pSector = (BYTE)sector & 0x3F;

    // the cylinder # is actually 10 bits and the upper 2 bits goes in the upper 2 bits of the sector #
    *pSector |= (cyl & 0x0300) >> 2;

    return TRUE;
}


#if HIDDEN_PARTITIONS
/*****************************************************************************
 *  WriteHiddenPartitionTable - using the state and the on disk partition
 *  structures, this function writes out the additional data we need (beyond)
 *  what's held in the DOS partition table) for the store and partition info.
 *
 *  Input: state - structure for this store
 *
 *  Output: none
 *
 *  Return: TRUE if write successful, FALSE if not
 *
 *****************************************************************************/
BOOL WriteHiddenPartitionTable(DriverState * state)
{

    PartState    *pPartState;
    PartState    *pHiddenPart;
    PBYTE           buffer = NULL;
    PBYTE           tmpbuffer = NULL;
    DWORD           bufferSize;
    int             partCount;
    BOOL            bResult;
    DWORD           dwChecksum;

    bResult = FALSE;

    pHiddenPart = state->pHiddenPartState;

    // it's possible this could be called without there being room for a hidden partition table
    if (!pHiddenPart)
        return TRUE;

    // HIDDEN_PART_SEC_COUNT is used to calculate the buffer size because the partition was probably padded to a
    //  cylinder boundary and we have no interest in the extra sectors - using it would cause an unnecessarily
    //  large buffer allocation
    bufferSize = state->diskInfo.di_bytes_per_sect * HIDDEN_PART_SEC_COUNT;
    buffer = (PBYTE)LocalAlloc(LMEM_ZEROINIT, bufferSize);
    if (!buffer)
        return FALSE;

    // set the storage signature
    *(DWORD *)buffer = STORESIG;

    // update the store info
    tmpbuffer = buffer + STORESTART;
    memcpy(tmpbuffer, state, offsetof(DriverState, hStore));

    tmpbuffer = buffer + STORESTART + offsetof(DriverState, hStore);

    pPartState = state->pPartState;
    partCount = 0;

    // update the partition info in the table
    while (pPartState)
    {
        // we don't need to track info on the hidden partition
        if (pPartState == state->pHiddenPartState)
        {
            pPartState = pPartState->pNextPartState;
            continue;
        }

        if (partCount == (HIDDEN_PART_SEC_COUNT * 3))
            break;

        memcpy(tmpbuffer, pPartState, sizeof(OnDiskPartState));
        pPartState = pPartState->pNextPartState;
        tmpbuffer += sizeof(OnDiskPartState);
        partCount++;
    }

    tmpbuffer = buffer + STORESTART;

    // update the partition count before the checksum so it checksums the proper #
    ((DriverState *)tmpbuffer)->dwNumParts = partCount;

    // before writing out the partition data, update the checksum
    GeneratePartitionChecksum(state, buffer, &dwChecksum);
    ((DriverState *)tmpbuffer)->dwChecksum = dwChecksum;

    bResult = WriteSectors(state, (DWORD)pHiddenPart->snStartSector, HIDDEN_PART_SEC_COUNT, buffer);

    LocalFree(buffer);
    return bResult;
}
#endif

/*****************************************************************************
 *  InitPartition - using the partition structure, the first 4K of the
 *  partition is initialized.
 *
 *  Input:  state - structure for this store
 *          pPartState - partition structure for the partition being init'd
 *
 *  Output: none
 *
 *
 *  Return: TRUE if successfully init'd, FALSE if not
 *
 *****************************************************************************/

BOOL InitPartition(DriverState * state, PartState * pPartState)
{

    PBYTE     buffer = NULL;
    DWORD     bufferSize;
    SECTORNUM numSectors;
    SECTORNUM sectorNum;
    SG_REQ    sgReq;
    DWORD     dummy;
    int       result;

    result = FALSE;

    /* get an I/O buffer that is a multiple of the sector size */
//    bufferSize = ((4 * 1024 + state->diskInfo.di_bytes_per_sect - 1) / state->diskInfo.di_bytes_per_sect) * state->diskInfo.di_bytes_per_sect;
    bufferSize = state->diskInfo.di_bytes_per_sect;
    buffer     = (PBYTE)LocalAlloc(0, bufferSize);
    if (buffer)
    {
        sectorNum  = pPartState->snStartSector;
        numSectors = (4*1024 + state->diskInfo.di_bytes_per_sect -1) / state->diskInfo.di_bytes_per_sect;
        if (numSectors > (pPartState->snNumSectors - 1))
            numSectors = pPartState->snNumSectors -1;

        // FDISK actually inits the partition start to 'F6', so do the same
        // TODO:    
//        memset (buffer, 0xF6, bufferSize);
        memset (buffer, 0, bufferSize);
        for (;;)
        {
            if (numSectors == 0)
            {
                result = TRUE;
                break;
            }

            sgReq.sr_start            = (DWORD)sectorNum;
            sgReq.sr_num_sec          = bufferSize / state->diskInfo.di_bytes_per_sect;
            sgReq.sr_num_sg           = 1;
            sgReq.sr_status           = 0;
            sgReq.sr_callback         = NULL;
            sgReq.sr_sglist[0].sb_buf = buffer;
            sgReq.sr_sglist[0].sb_len = bufferSize;

            result = DeviceIoControl(state->hStore, DISK_IOCTL_WRITE, &sgReq, sizeof(SG_REQ), NULL, 0, &dummy, NULL);
            if (!result)
            {
                break;
            }

            sectorNum  += bufferSize / state->diskInfo.di_bytes_per_sect;
            numSectors -= bufferSize / state->diskInfo.di_bytes_per_sect;
        }

        LocalFree(buffer);

    }

    return result;
}


/*****************************************************************************
 *  UpdateFileTime - updates the FILETIME members of the state and partition
 *  structures.
 *
 *  Input:  state - structure for this store
 *          pPartState - partition structure to update
 *          bUpdateStateCreate - flag to determine if the create time in the
 *              state structure should be updated
 *          bUpdatePartCreate - flag to determine if the create time in the
 *              partition structure should be updated
 *
 *  Output: pPartState - ftCreation updated
 *          state - ftAccess updated, ftCreate may be updated
 *
 *  Return: none
 *
 *****************************************************************************/
void UpdateFileTime(DriverState * state, PartState * pPartState, BOOL bUpdateStateCreate, BOOL bUpdatePartCreate)
{

    SYSTEMTIME systemTime;
    FILETIME   fileTime;

    GetSystemTime(&systemTime);
    SystemTimeToFileTime(&systemTime,&fileTime);

    if (pPartState)
    {
        pPartState->ftModified = fileTime;

        if (bUpdatePartCreate)
            pPartState->ftCreation = fileTime;
    }

    if (!state)
        return;

    state->ftAccess = fileTime;

    if (bUpdateStateCreate)
        state->ftCreate = fileTime;

    return;
}


/*****************************************************************************
 *  GetPartition - finds the partition structure and optionally the previous
 *  partition structure for the partition name.
 *
 *  Input:  state - structure for this store
 *          partName - partition structure to locate
 *
 *  Output: ppPartState - pointer to the partition structure for this partition
 *          ppPrevPartState - pointer to the partition structure immediatly
 *              preceeding this one on the media
 *
 *  Return: none
 *
 *****************************************************************************/
void GetPartition(DriverState * state, PartState ** ppPartState, PartState ** ppPrevPartState, LPCTSTR partName)
{

    PartState * pPartState;
    PartState * pPrevPartState;

    pPartState = state->pPartState;
    pPrevPartState = NULL;
    *ppPartState = NULL;

    if (ppPrevPartState)
        *ppPrevPartState = NULL;

    if (!pPartState)
        return;

    while (pPartState)
    {
        // is this the partition we're looking for
        if (wcsicmp(partName, pPartState->cPartName) == 0)
        {
            *ppPartState = pPartState;

            if (ppPrevPartState)
                *ppPrevPartState = pPrevPartState;

            break;
        }
        else
        {
            pPrevPartState = pPartState;
            pPartState = pPartState->pNextPartState;

        }
    }

    return;
}


/*****************************************************************************
 *  GetDOSPartitions - traverses the partition table passed in the buffer to
 *  build up the linked list of partition structures.  This validates the
 *  contents of the partition table first before building up the structures.
 *  If an entry for an extended partition is found, is this is called recursively
 *  to process the extended partition.
 *
 *  Input:  state - structure for this store
 *          pPrevPartState - pointer to the last partition structure in the list
 *          pSector - buffer with the partition table
 *          bExtPart - TRUE if processing an extended partition
 *          pStartSector - offset of the extended partition for generating the
 *              correct starting sector of the partition
 *
 *  Output: partition structures added to the state structure
 *
 *  Return: ERROR_SUCCESS or ERROR_INVALID_DATA if the partition table contents
 *          are invalid, other error codes may be returned
 *****************************************************************************/
DWORD GetDOSPartitions(DriverState * state, PartState ** pPrevPartState, PBYTE pSector, BOOL bExtPart, SECTORNUM *pStartSec)
{

    PartState *pPartState;
    PBYTE        extPartbuffer = NULL;
    PPARTENTRY   tmpbuffer;
    DWORD        lastStartSector;
    DWORD        dwError;
    BOOL         bResult;
    int          i, partCount;

#ifdef  DEBUG_PART_START
    PBYTE       pPartContents;
#endif

    if (bExtPart)
        partCount = 2;
    else
        partCount = 4;

    // first check the trailer to see if it looks valid
    if (*(WORD *)(pSector + BOOT_SIGNATURE) == BOOTSECTRAILSIGH)
    {
        lastStartSector = 0;

        // looks OK, now validate the partition table entries
        for (i=0, tmpbuffer = (PPARTENTRY)(pSector + PARTTABLE_OFFSET); i < partCount; i++, tmpbuffer ++)
        {
#if 0            
            if ((tmpbuffer->Part_BootInd != PART_NON_BOOTABLE) &&
                (tmpbuffer->Part_BootInd != PART_BOOTABLE))
                return ERROR_INVALID_DATA;
#endif            
            // at least the first partition should have sectors allocated
            if ((i==0) && (!tmpbuffer->Part_TotalSectors))
            {
                // if we're in an extended partition, the logical drive in part 0 may have been deleted
                // so we continue processing to gather the remaining logical drives - if it's really the
                // end of the logical drives the next iteration will fail anyway
                if (bExtPart)
                    continue;

                return ERROR_INVALID_DATA;
            }

            if (!tmpbuffer->Part_StartSector)
                break;
            
            if (tmpbuffer->Part_StartSector < lastStartSector)
                return ERROR_INVALID_DATA;

            lastStartSector = tmpbuffer->Part_StartSector;
        }

    }
    else
        if (bExtPart)
            // we could have reached the end of the logical drives so in an extended partition, in this case
            //  just return saying we finished parsing them
            return ERROR_SUCCESS;
        else
            return ERROR_INVALID_DATA;

    // the partition table looks reasonable, now build up the structures
    for (i = 0, tmpbuffer = (PPARTENTRY)(pSector + PARTTABLE_OFFSET); i < partCount; i++, tmpbuffer ++)
    {
        // no more partitions??
        if (!tmpbuffer->Part_TotalSectors)
        {
            // a logical drive may have been deleted, leaving a hole in the PBR
            if ((i==0) && (bExtPart))
                continue;

            break;
        }

        // if we're in the extended partition, the second entry in the table can only point to the next logical drive
        if (bExtPart && (i == 1) &&
            (tmpbuffer->Part_FileSystem != PART_EXTENDED) && (tmpbuffer->Part_FileSystem != PART_DOSX13X))
            break;

        if (tmpbuffer->Part_FileSystem == PART_EXTENDED || tmpbuffer->Part_FileSystem == PART_DOSX13X)
        {

            // there are cases where FDISK may create a bogus entry for an extended partition of 0 sectors
            // in this case, move to the next entry in the table which is probably the end of the list anyway
            if (!tmpbuffer->Part_TotalSectors)
                continue;

            // we've encountered an extended partition, retrieve its partition table and process it
            // the start sector of the extended partition or its logical drives is relative to
            //     the start of the extended partition location from the MBR
            extPartbuffer = NULL;
            bResult = ReadSectors (state, state->snExtPartSector + tmpbuffer->Part_StartSector, 1, &extPartbuffer);
            if (!bResult)
            {
                dwError = GetLastError();
                return dwError;
            }

            // there can only be 1 extended partition in the primary table - hang on to the start of it
            //  also calculate the start of the next logical drive
            if (!bExtPart)
            {
                state->snExtPartSector = tmpbuffer->Part_StartSector;
                state->snExtPartEndSec = tmpbuffer->Part_StartSector + tmpbuffer->Part_TotalSectors -1;
                *pStartSec = tmpbuffer->Part_StartSector;
            }
            else
                *pStartSec = state->snExtPartSector + tmpbuffer->Part_StartSector;

            dwError = GetDOSPartitions(state, pPrevPartState, extPartbuffer, TRUE, pStartSec);

            LocalFree(extPartbuffer);

            if (dwError)
                return dwError;

            // this is 'global' data, and we're traversed back to finishing up the MBR processing so there is no
            //  sector skew to account for
            if (!bExtPart)
                *pStartSec = 0;

            continue;
        }

        pPartState = (PPartState)LocalAlloc(LMEM_ZEROINIT, sizeof(PartState));
        if (!pPartState)
            return ERROR_NOT_ENOUGH_MEMORY;

        // skew the start sector by the partition offset in case we have extended partitions
        pPartState->snStartSector = tmpbuffer->Part_StartSector + *pStartSec;
        pPartState->snNumSectors = tmpbuffer->Part_TotalSectors;
        pPartState->partType = tmpbuffer->Part_FileSystem;
        if (tmpbuffer->Part_BootInd & PART_BOOTABLE) {
            pPartState->dwAttributes |= PARTITION_ATTRIBUTE_BOOT;
        }
        if (tmpbuffer->Part_BootInd & PART_READONLY) {
            pPartState->dwAttributes |= PARTITION_ATTRIBUTE_READONLY;
        }

        pPartState->snPBRSector = *pStartSec;

#ifdef DEBUG_PART_START
        pPartContents = NULL;
        bResult = ReadSectors (state, pPartState->snStartSector, 1, &pPartContents);

#endif

        // add to the linked list
        if (state->pPartState == NULL)
            state->pPartState = pPartState;
        else
            (*pPrevPartState)->pNextPartState = pPartState;

        pPartState->pState = state;
        *pPrevPartState = pPartState;

        // see if this is our hidden partition and if so, keep track
        if (tmpbuffer->Part_FileSystem == HIDDEN_PART_TYPE)
            state->pHiddenPartState = pPartState;
        else
            // we track the number of partitions we have but don't include the hidden partition
            state->dwNumParts++;
    }

    return ERROR_SUCCESS;
}

#if HIDDEN_PARTITIONS
/*****************************************************************************
 *  AddHiddenPartition - adds a hidden partition to track the additional store
 *  and partition information we want to track.
 *
 *  Input:  state - structure for this store
 *
 *  Output: state - adds a partition structure to the linked list and updates
 *          the hidden partition pointer
 *
 *  Return: TRUE if successfully written, FALSE if not
 *
 *****************************************************************************/
BOOL AddHiddenPartition(DriverState * state)
{

    PartState *pPartState;
    PartState *pNewPartState;
    BOOL         bResult;
    SECTORNUM    snStartSector;
    SECTORNUM    snPBRSector;
    SECTORNUM    snSectorCount;
    SECTORNUM    snNextPBRSector = 0;
    SECTORNUM    snPrevPBRSector = 0;

    // make sure we can allocate the structure to track the hidden partition before updating the media
    pNewPartState = (PPartState)LocalAlloc(LMEM_ZEROINIT, sizeof(PartState));
    if (!pNewPartState)
        return FALSE;

    bResult = GetPartLocation (state, HIDDEN_PART_SEC_COUNT, &pPartState, &snStartSector, &snPBRSector, &snSectorCount);
    if (!bResult)
    {
        LocalFree(pNewPartState);
        return FALSE;
    }

    // we are linking up to an existing partition
    if (pPartState)
    {
        snPrevPBRSector = pPartState->snPBRSector;

        if (pPartState->pNextPartState)
            snNextPBRSector = pPartState->pNextPartState->snPBRSector;
    }
    else
    {
        // this partition may be added to the front of the list
        if (state->pPartState)
            snNextPBRSector = state->pPartState->snPBRSector;

    }

    // we're adding to the extended partition
    if (snPBRSector)
        bResult = CreatePBR(state, snPrevPBRSector, snNextPBRSector, snPBRSector, snSectorCount, HIDDEN_PART_TYPE);
    else
        bResult = WriteMBR(state, snStartSector, snSectorCount, HIDDEN_PART_TYPE, FALSE);

    if (!bResult)
    {
        LocalFree(pNewPartState);
        return FALSE;
    }

    // now update the structure to track the hidden partition
    pNewPartState->snStartSector = snStartSector;
    pNewPartState->snPBRSector = snPBRSector;

    if (snPBRSector)
        pNewPartState->snNumSectors = snSectorCount - (snStartSector - snPBRSector);
    else
        pNewPartState->snNumSectors = snSectorCount;

    pNewPartState->pNextPartState = NULL;
    pNewPartState->pState = state;
    pNewPartState->partType = HIDDEN_PART_TYPE;
    state->pHiddenPartState = pNewPartState;

    if (!pPartState)
    {
        // we're adding to the front of the list
        if (state->pPartState)
            pNewPartState->pNextPartState = state->pPartState;

        state->pPartState = pNewPartState;
    }
    else
    {
        pNewPartState->pNextPartState = pPartState->pNextPartState;
        pPartState->pNextPartState = pNewPartState;
    }

    return TRUE;
}
#endif

/*****************************************************************************
 *  GeneratePartitionNames - traverses the list of partition structures and
 *  generates a partition name for any partition with NULL names.  This will
 *  attempt to get the name from the volume label in the BPB but if that fails
 *  will generate a name of Partxx so unique names are assigned.
 *
 *  Input:  state - structure for this store
 *
 *  Output: partition structures cPartName updated
 *
 *  Return: none
 *
 *****************************************************************************/
void GeneratePartitionNames(DriverState *state)
{

    PartState    *pTmpPartState;
    PartState    *pPartState;
    TCHAR   partName[PARTITIONNAMESIZE];
    int     partID = 0;
    pPartState = state->pPartState;

    while (pPartState)
    {
        // no need to name the hidden partition or generate names for partitions that already have them
        if (pPartState->partType == HIDDEN_PART_TYPE || (*pPartState->cPartName))
        {
            pPartState = pPartState->pNextPartState;
            continue;
        }




#if 0 // Grab label from disk
        int     labelLength;
        DWORD   volOffset;
        BOOL    bResult;
        BYTE    fileSysType;
        PBYTE   buffer, tmpbuffer, tmpbuffer1;
        fileSysType = pPartState->partType;

        if (fileSysType)
        {

            if (fileSysType == PART_DOS2_FAT || fileSysType == PART_DOS3_FAT ||
                fileSysType == PART_DOS4_FAT || fileSysType == PART_DOS32)
            {
                // read this partition's "boot sector"
                buffer = NULL;  
                bResult = ReadSectors(pPartState->pState, pPartState->snStartSector, 1, &buffer);
                if (!bResult)
                    return;

                // offset to the volume label
                tmpbuffer = buffer + volOffset;

                // be sure the boot sector has been formatted before grabbing the label
                if ((*(WORD *)(buffer + BOOT_SIGNATURE) == BOOTSECTRAILSIGH) &&
                    (*buffer == 0x90 || *buffer == 0xEB || *buffer == 0xE9))
                {
                    // be sure there's a valid unique label
                    if ((*tmpbuffer) && (*tmpbuffer != 0x20) && (memcmp("NO NAME", tmpbuffer, 7)))
                    {
                        tmpbuffer1 = tmpbuffer;

                        // calculate the length of the label so the output is not padded with spaces
                        labelLength = 0;

                        for (labelLength = 0; (*tmpbuffer1 != 0x20) && (labelLength < 11); labelLength++, tmpbuffer1++);

                        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (const char *)tmpbuffer, labelLength, pPartState->cPartName, PARTITIONNAMESIZE);
                        pPartState = pPartState->pNextPartState;
                        LocalFree(buffer);
                        continue;
                    }
                }

                LocalFree(buffer);
            }
        }
#else
        BYTE fileSysType = pPartState->partType;
        BOOL    bResult;
        PBYTE   buffer = NULL;
        char    szVolumeName[OEMNAMESIZE];
        if (fileSysType == PART_DOS2_FAT || fileSysType == PART_DOS3_FAT ||
            fileSysType == PART_DOS4_FAT || fileSysType == PART_DOS32 ||
            fileSysType == PART_DOS32X13 || fileSysType == PART_DOSX13)
        {
            buffer = NULL;
            if (!(bResult = ReadSectors(pPartState->pState, pPartState->snStartSector, 1, &buffer)))
                return;
            if ((*(WORD *)(buffer + BOOT_SIGNATURE) == BOOTSECTRAILSIGH) &&
                (*buffer == 0x90 || *buffer == 0xEB || *buffer == 0xE9))
            {
                if (fileSysType == PART_DOS2_FAT || 
                    fileSysType == PART_DOS3_FAT || 
                    fileSysType == PART_DOS4_FAT || 
                    fileSysType == PART_DOSX13) 
                {
                    PBOOTSEC pBootSec = (PBOOTSEC)buffer;
                    memcpy( szVolumeName, pBootSec->bsVolumeLabel, OEMNAMESIZE);
                } else {
                    PBIGFATBOOTSEC pBootSec = (PBIGFATBOOTSEC)buffer;
                    memcpy( szVolumeName, pBootSec->bgbsVolumeLabel, OEMNAMESIZE);
                }   
                if (memcmp( szVolumeName, "NO NAME", 7) != 0) {
                    DWORD labellength;
                    for (labellength =0; labellength < OEMNAMESIZE; labellength++) {
                        if (!szVolumeName[labellength] || (szVolumeName[labellength] == 0x20)) 
                            break;
                    }
                    if (labellength) {
                        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szVolumeName, labellength, pPartState->cPartName, PARTITIONNAMESIZE);
                        pPartState = pPartState->pNextPartState;
                        LocalFree(buffer);
                        buffer = NULL;
                        continue;
                    }   
                }   
            }
            if (buffer) {
                LocalFree( buffer);
            }    
        }            
            

#endif        
        // if we're here, either the volume label is blank or the partition type is unknown
        // generate a name so all partition names are unique    
        while (TRUE)
        {
            memset (partName, 0, PARTITIONNAMESIZE*sizeof(TCHAR));

            //Generate partition name 
            TCHAR* tmpName = partName;
            wcscpy (tmpName, L"Part");
            tmpName += 4;
            if (partID < 10)
                *tmpName++ = L'0';
            _itow (partID, tmpName, 10);
            
            partID++;

            // see if this one is already used
            GetPartition(state, &pTmpPartState, NULL, partName);
            if (!pTmpPartState)
            {
                memcpy(pPartState->cPartName, partName, (wcslen(partName)*2));
                break;
            }
        }
        pPartState = pPartState->pNextPartState;
    }
}

#if 0
void WritePartitionName(PartState *pPartState)
{
    PBYTE buffer = NULL;
    BYTE fileSysType = pPartState->partType;
    BOOL bResult;
    char szVolumeName[MAX_PATH];
    if (!(bResult = ReadSectors(pPartState->pState, pPartState->snStartSector, 1, &buffer)))
        return;
    WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, pPartState->cPartName, -1, szVolumeName, OEMNAMESIZE, NULL, NULL);
    if ((*(WORD *)(buffer + BOOT_SIGNATURE) == BOOTSECTRAILSIGH) &&
        (*buffer == 0x90 || *buffer == 0xEB || *buffer == 0xE9))
    {
        if (fileSysType == PART_DOS2_FAT || 
            fileSysType == PART_DOS3_FAT || 
            fileSysType == PART_DOS4_FAT || 
            fileSysType == PART_DOSX13) 
        {
            PBOOTSEC pBootSec = (PBOOTSEC)buffer;
            memcpy( pBootSec->bsVolumeLabel, szVolumeName, OEMNAMESIZE);
        } else {
            PBIGFATBOOTSEC pBootSec = (PBIGFATBOOTSEC)buffer;
            memcpy( pBootSec->bgbsVolumeLabel, szVolumeName, OEMNAMESIZE);
        }   
        WriteSectors( pPartState->pState, pPartState->snStartSector, 1, buffer);
    }
    LocalFree( buffer);
}
#endif

#if HIDDEN_PARTITIONS
/*****************************************************************************
 *  GetHiddenPartitions - traverses the list of partition structures and compares
 *  each to the entries in the hidden partition.  If a match is found the
 *  additional data is copied to the partition structure.
 *
 *  Input: state - structure for this store
 *
 *  Output: partition structures may be updated
 *
 *  Return: TRUE if successful, FALSE if not including a checksum failure
 *
 *****************************************************************************/
BOOL GetHiddenPartitions(DriverState * pState)
{

    OnDiskPartState  *tmpbuffer;
    PBYTE               buffer = NULL;
    PartState        *pHiddenPart;
    PartState        *pPartState;
    BOOL                bResult;
    int                 partCount;

    pHiddenPart = pState->pHiddenPartState;

    bResult = ReadSectors(pState, pHiddenPart->snStartSector, HIDDEN_PART_SEC_COUNT, &buffer);
    if (!bResult)
        return FALSE;

    // see if the partition table looks valid
    if (*(DWORD *)buffer != STORESIG)
    {
        LocalFree(buffer);
        return FALSE;
    }

    // verify the checksum before using the partition table
    bResult = GeneratePartitionChecksum(pState, buffer, NULL);
    if (!bResult)
    {
        LocalFree(buffer);
        return FALSE;
    }

    // looks OK, build up the structures

    // note - don't just memcpy the portion of the store structure that's on the media, the # partitions is
    //  generated during boot while reading the DOS partition tables which could differ from what was last
    //  written to the count in the hidden partition
    tmpbuffer = (OnDiskPartState*)(buffer + STORESTART);
    pState->ftCreate = ((DriverState *)tmpbuffer)->ftCreate;
    pState->ftAccess = ((DriverState *)tmpbuffer)->ftAccess;

    tmpbuffer = (OnDiskPartState *)(buffer + STORESTART + offsetof(DriverState, hStore));
    partCount = 0;

    while (tmpbuffer->snNumSectors)
    {
        if (partCount == (HIDDEN_PART_SEC_COUNT*3))
            break;

        for (pPartState = pState->pPartState; pPartState != NULL; pPartState = pPartState->pNextPartState)
        {
            if (pPartState == pHiddenPart)
                continue;

            if ((pPartState->snStartSector == tmpbuffer->snStartSector) &&
                (pPartState->snNumSectors == tmpbuffer->snNumSectors))
            {
                // we found the partition structure for this partition, add in the data
                memcpy(pPartState, (PBYTE)tmpbuffer, sizeof(OnDiskPartState));
                break;
            }
        }

        tmpbuffer++;
        partCount++;

    }

    LocalFree(buffer);
    return TRUE;
    
}
#endif
