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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#include "dskioctl.h"


static const DWORD DEF_SECTOR_SIZE      = 512;

BOOL Dsk_GetDiskInfo(HANDLE hDisk, DISK_INFO *pDiskInfo)
{
    BOOL fRet;
    DWORD bytes;
    FLASH_REGION_INFO flashRegionInfo = {0};
    FLASH_PARTITION_INFO flashPartInfo = {0};

    g_pKato->Log(LOG_COMMENT, L"DeviceIoControl(0x%08x, DISK_IOCTL_GETINFO)", hDisk);
    fRet = DeviceIoControl(hDisk, DISK_IOCTL_GETINFO, 
        pDiskInfo, sizeof(DISK_INFO),
        pDiskInfo, sizeof(DISK_INFO), 
        &bytes, NULL);

    if(FALSE == fRet) {
        g_pKato->Log(LOG_COMMENT, L"FAILED: DeviceIoControl(0x%08x, DISK_IOCTL_GETINFO); error %u", hDisk, GetLastError());
        g_pKato->Log(LOG_COMMENT, L"DeviceIoControl(0x%08x, IOCTL_DISK_GETINFO)", hDisk);
        fRet = DeviceIoControl(hDisk, IOCTL_DISK_GETINFO,
            pDiskInfo, sizeof(DISK_INFO),
            pDiskInfo, sizeof(DISK_INFO),
            &bytes, NULL);
    }

    if(FALSE == fRet) {
        g_pKato->Log(LOG_DETAIL, L"FAILED: DeviceIoControl(0x%08x, IOCTL_DISK_GETINFO); error %u", hDisk, GetLastError());
    }

    // The data obtained by the DISK_INFO ioctl will be sufficient for the tests
    // with the exception of the number of sectors on the disk. In the new flash driver
    // this value must be obtained through a specific ioctl
    if (fRet && g_bUseFlashIOCTLs)
    {
        if (!Dsk_GetFlashRegionInfo(hDisk, &flashRegionInfo))
        {
            g_pKato->Log(LOG_DETAIL, L"FAILED: Dsk_GetFlashRegionInfo");
            fRet = FALSE;
        }
        if (fRet && !Dsk_GetFlashPartitionInfo(hDisk, &flashPartInfo))
        {
            g_pKato->Log(LOG_DETAIL, L"FAILED: Dsk_GetFlashPartitionInfo");
            fRet = FALSE;
        }
        // Calculate the number of sectors in the flash and adjust
        // the diskinfo value
        if (fRet)
        {
            pDiskInfo->di_total_sectors = (DWORD)(flashRegionInfo.SectorsPerBlock *
                flashPartInfo.LogicalBlockCount);
        }
    }

    return fRet;
}

BOOL Dsk_FormatMedia(HANDLE hDisk)
{
    BOOL fRet;
    DWORD bytes;

    if(g_bUseFlashIOCTLs){
        g_pKato->Log(LOG_COMMENT, L"DeviceIoControl(0x%08x, IOCTL_FLASH_FORMAT_PARTITION)", hDisk);
        fRet = DeviceIoControl(hDisk, IOCTL_FLASH_FORMAT_PARTITION, &(g_FlashPartInfo.PartitionId), sizeof(PARTITION_ID),
            NULL, 0, NULL, NULL); 

        if(FALSE == fRet) {
            g_pKato->Log(LOG_DETAIL, L"FAILED: DeviceIoControl(0x%08x, IOCTL_FLASH_FORMAT_PARTITION) failed error %u", hDisk, GetLastError());
        }
    }
    else{
        g_pKato->Log(LOG_COMMENT, L"DeviceIoControl(0x%08x, DISK_IOCTL_FORMAT_MEDIA)", hDisk);
        fRet = DeviceIoControl(hDisk, DISK_IOCTL_FORMAT_MEDIA, NULL, 0, NULL, 0, &bytes, NULL); 

        if(FALSE == fRet) {
            g_pKato->Log(LOG_COMMENT, L"FAILED: DeviceIoControl(0x%08x, DISK_IOCTL_FORMAT_MEDIA) failed error %u", hDisk, GetLastError());
            g_pKato->Log(LOG_COMMENT, L"DeviceIoControl(0x%08x, IOCTL_DISK_FORMAT_MEDIA)", hDisk);
            fRet = DeviceIoControl(hDisk, IOCTL_DISK_FORMAT_MEDIA, NULL, 0, NULL, 0, &bytes, NULL); 
        }

        if(FALSE == fRet) {
            g_pKato->Log(LOG_DETAIL, L"FAILED: DeviceIoControl(0x%08x, IOCTL_DISK_FORMAT_MEDIA) failed error %u", hDisk, GetLastError());
        }
    }

    return fRet;
}


BOOL Dsk_WriteSectors(HANDLE hDisk, DWORD startSector, DWORD cSectors, PBYTE pData)
{
    BOOL fRet;
    DWORD cBytes;

    if(g_bUseFlashIOCTLs){ 
        FLASH_TRANSFER_REQUEST flReq = {0};

        flReq.TransferList[0].SectorRun.StartSector = startSector;
        flReq.TransferList[0].SectorRun.SectorCount = cSectors;
        flReq.TransferList[0].pBuffer = pData;

        flReq.PartitionId = g_FlashPartInfo.PartitionId;
        flReq.RequestFlags = FLASH_SECTOR_REQUEST_FLAG_WRITE_THROUGH ||FLASH_SECTOR_REQUEST_FLAG_ATOMIC;
        flReq.TransferCount = 1;
        
        fRet = DeviceIoControl(hDisk, IOCTL_FLASH_WRITE_LOGICAL_SECTORS, &flReq, sizeof(flReq), NULL, 0, &cBytes, NULL);
        if(FALSE == fRet) {
            g_pKato->Log(LOG_DETAIL, L"FAILED: DeviceIoControl(0x%08x, IOCTL_FLASH_WRITE_LOGICAL_SECTORS) failed error %u", hDisk, GetLastError());
        }
    }
    else{
        SG_REQ sgReq = {0};
        // build sg request buffer -- single sg buffer
        sgReq.sr_start = startSector;
        sgReq.sr_num_sec = cSectors;
        sgReq.sr_num_sg = 1;
        sgReq.sr_callback = NULL; // no callback under CE
        sgReq.sr_sglist[0].sb_len = cSectors * g_diskInfo.di_bytes_per_sect;
        sgReq.sr_sglist[0].sb_buf = pData;

        fRet = DeviceIoControl(hDisk, DISK_IOCTL_WRITE, &sgReq, sizeof(sgReq), NULL, 0, &cBytes, NULL);

        if(FALSE == fRet) {
            g_pKato->Log(LOG_COMMENT, L"FAILED: DeviceIoControl(0x%08x, DISK_IOCTL_WRITE) failed error %u", hDisk, GetLastError());
            fRet = DeviceIoControl(hDisk, IOCTL_DISK_WRITE, &sgReq, sizeof(sgReq), NULL, 0, &cBytes, NULL);
        }

        if(FALSE == fRet) {
            g_pKato->Log(LOG_DETAIL, L"FAILED: DeviceIoControl(0x%08x, IOCTL_DISK_WRITE) failed error %u", hDisk, GetLastError());
        }
    }

    return fRet;
}

BOOL Dsk_ReadSectors(HANDLE hDisk, DWORD startSector, DWORD cSectors, PBYTE pData)
{
    BOOL fRet;
    DWORD cBytes;

    if(g_bUseFlashIOCTLs) {
        FLASH_TRANSFER_REQUEST flReq = {0};

        flReq.TransferList[0].SectorRun.StartSector = startSector;
        flReq.TransferList[0].SectorRun.SectorCount = cSectors;
        flReq.TransferList[0].pBuffer = pData;

        flReq.PartitionId = g_FlashPartInfo.PartitionId;
        flReq.RequestFlags = FLASH_SECTOR_REQUEST_FLAG_WRITE_THROUGH || FLASH_SECTOR_REQUEST_FLAG_ATOMIC;
        flReq.TransferCount = 1;

        fRet = DeviceIoControl(hDisk, IOCTL_FLASH_READ_LOGICAL_SECTORS, &flReq, sizeof(flReq), NULL, 0, &cBytes, NULL);
        if(FALSE == fRet) {
            g_pKato->Log(LOG_DETAIL, L"FAILED: DeviceIoControl(0x%08x, IOCTL_FLASH_READ_LOGICAL_SECTORS) failed error %u", hDisk, GetLastError());
        }
    }
    else{
        SG_REQ sgReq = {0};
        // build sg request buffer -- single sg buffer
        sgReq.sr_start = startSector;
        sgReq.sr_num_sec = cSectors;
        sgReq.sr_num_sg = 1;
        sgReq.sr_callback = NULL; // no callback under CE
        sgReq.sr_sglist[0].sb_len = cSectors * g_diskInfo.di_bytes_per_sect;
        sgReq.sr_sglist[0].sb_buf = pData;

        fRet = DeviceIoControl(hDisk, DISK_IOCTL_READ, &sgReq, sizeof(sgReq), NULL, 0, &cBytes, NULL);

        if(FALSE == fRet) {
            g_pKato->Log(LOG_COMMENT, L"FAILED: DeviceIoControl(0x%08x, DISK_IOCTL_READ) failed error %u", hDisk, GetLastError());
            fRet = DeviceIoControl(hDisk, IOCTL_DISK_READ, &sgReq, sizeof(sgReq), NULL, 0, &cBytes, NULL);
        }

        if(FALSE == fRet) {
            g_pKato->Log(LOG_DETAIL, L"FAILED: DeviceIoControl(0x%08x, IOCTL_DISK_READ) failed error %u", hDisk, GetLastError());
        }
    }
    return fRet;
}

BOOL Dsk_DeleteSectors(HANDLE hDisk, DWORD startSector, DWORD cSectors)
{
    BOOL fRet;
    DWORD cBytes;

    if(g_bUseFlashIOCTLs){
        FLASH_SECTOR_DELETE flDelete = {0};

        flDelete.PartitionId = g_FlashPartInfo.PartitionId;
        flDelete.SectorRunCount = 1;
        flDelete.SectorRunList[0].StartSector = startSector;
        flDelete.SectorRunList[0].SectorCount = cSectors;

        fRet = DeviceIoControl(hDisk, IOCTL_FLASH_DELETE_LOGICAL_SECTORS, &flDelete, sizeof(flDelete), NULL, 0, &cBytes, NULL);
        if(FALSE == fRet) {
            g_pKato->Log(LOG_DETAIL, L"FAILED: DeviceIoControl(0x%08x, IOCTL_FLASH_DELETE_LOGICAL_SECTORS) failed error %u", hDisk, GetLastError());
        }
    }
    else{
        DELETE_SECTOR_INFO dsInfo = {0};

        dsInfo.cbSize = sizeof(DELETE_SECTOR_INFO);
        dsInfo.startsector = startSector;
        dsInfo.numsectors = cSectors;

        fRet = DeviceIoControl(hDisk, IOCTL_DISK_DELETE_SECTORS, &dsInfo, sizeof(dsInfo), NULL, 0, &cBytes, NULL);

        if(FALSE == fRet) {
            g_pKato->Log(LOG_DETAIL, L"FAILED: DeviceIoControl(0x%08x, IOCTL_DISK_DELETE_SECTORS) failed error %u", hDisk, GetLastError());
        }
    }

    return fRet;
}

BOOL Dsk_GetFlashPartitionInfo(HANDLE hDisk, FLASH_PARTITION_INFO * pPartTable)
{
    BOOL fRet = FALSE;
    DWORD dwPartTableSize = 0;
    FLASH_PARTITION_INFO * pTempPartTable = NULL;

    if (!g_bUseFlashIOCTLs)
    {
        g_pKato->Log(LOG_DETAIL, L"INFO: Dsk_GetFlashPartitionInfo called without specifying /flash test flag.");
        goto done;
    }

    fRet = DeviceIoControl(hDisk, IOCTL_FLASH_GET_PARTITION_TABLE, NULL, 0, NULL, 0, &dwPartTableSize, NULL);
    if(FALSE == fRet){
        g_pKato->Log(LOG_DETAIL, L"FAILED: couldn't get size of partition table, failing test.  Error %u", GetLastError());
        goto done;
    }
    pTempPartTable = (FLASH_PARTITION_INFO*)malloc(dwPartTableSize);
    if(!pTempPartTable){
        g_pKato->Log(LOG_DETAIL, L"FAILED: couldn't allocated memory for partition table, failing");
        goto done;
    }
    //get the partition table
    fRet = DeviceIoControl(hDisk, IOCTL_FLASH_GET_PARTITION_TABLE, NULL, 0, pTempPartTable, dwPartTableSize, NULL, NULL); 
    if(FALSE == fRet){
        g_pKato->Log(LOG_DETAIL, L"FAILED: couldn't get partition table, failing test.  Error %u", GetLastError());
        goto done;
    }

    // copy the first partition info entry from the table
    memcpy(pPartTable, pTempPartTable, sizeof(FLASH_PARTITION_INFO));

    fRet = TRUE;
done:
    if (pTempPartTable)
    {
        free(pTempPartTable);
    }
    return fRet;
}

BOOL Dsk_GetFlashRegionInfo(HANDLE hDisk, FLASH_REGION_INFO * pRegionTable)
{
    BOOL fRet = FALSE;
    DWORD dwRegionCount = 0;
    DWORD dwRegionTableSize = 0;
    FLASH_REGION_INFO * pTempRegionTable = NULL;

    if (!g_bUseFlashIOCTLs)
    {
        g_pKato->Log(LOG_DETAIL, L"INFO: Dsk_GetFlashPartitionInfo called without specifying /flash test flag.");
        goto done;
    }

    // Query device for the number of regions
    if (!DeviceIoControl(hDisk, IOCTL_FLASH_PDD_GET_REGION_COUNT, NULL, 0, &dwRegionCount, sizeof(DWORD),
        NULL, NULL))
    {
        g_pKato->Log(LOG_DETAIL, L"Dsk_GetFlashRegionInfo() : DeviceIoControl(IOCTL_FLASH_PDD_GET_REGION_COUNT) failed:  Error %u", GetLastError());
        goto done;
    }

    // Allocate partition table stucture
    dwRegionTableSize = dwRegionCount*sizeof(FLASH_REGION_INFO);
    pTempRegionTable = (FLASH_REGION_INFO*)malloc(dwRegionTableSize);
    if (NULL == pTempRegionTable)
    {
        g_pKato->Log(LOG_DETAIL, L"Error : Dsk_GetFlashRegionInfo() : out of memory");
        goto done;
    }

    // Get the region table
    if (!DeviceIoControl(hDisk, IOCTL_FLASH_PDD_GET_REGION_INFO, NULL, 0,
        pTempRegionTable, dwRegionTableSize, NULL, NULL))
    {
        g_pKato->Log(LOG_DETAIL,
            L"Dsk_GetFlashRegionInfo() : DeviceIoControl(IOCTL_FLASH_PDD_GET_REGION_INFO) failed:  Error %u",
            GetLastError());
        goto done;
    }

    // Make sure we have at least one region
    if (0 == dwRegionCount)
    {
        g_pKato->Log(LOG_DETAIL, L"Error : Dsk_GetFlashRegionInfo() : 0 regions found");
        goto done;
    }

    // copy the first region info entry from the table
    memcpy(pRegionTable, pTempRegionTable, sizeof(FLASH_REGION_INFO));

    fRet = TRUE;
done:
    if (pTempRegionTable)
    {
        free(pTempRegionTable);
    }
    return fRet;
}
