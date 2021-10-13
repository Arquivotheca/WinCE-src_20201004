//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "dskioctl.h"


static const DWORD DEF_SECTOR_SIZE      = 512;

BOOL Dsk_GetDiskInfo(HANDLE hDisk, DISK_INFO *pDiskInfo)
{
    BOOL fRet;
    DWORD bytes;

    g_pKato->Log(LOG_COMMENT, L"DeviceIoControl(0x%08x, DISK_IOCTL_GETINFO)", hDisk);
    fRet = DeviceIoControl(hDisk, DISK_IOCTL_GETINFO, 
        pDiskInfo, sizeof(DISK_INFO),
        pDiskInfo, sizeof(DISK_INFO), 
        &bytes, NULL);

    if(FALSE == fRet) {
        g_pKato->Log(LOG_DETAIL, L"FAILED: DeviceIoControl(0x%08x, DISK_IOCTL_GETINFO); error %u", hDisk, GetLastError());
    }

    return fRet;
}

BOOL Dsk_FormatMedia(HANDLE hDisk)
{
    BOOL fRet;
    DWORD bytes;

    g_pKato->Log(LOG_COMMENT, L"DeviceIoControl(0x%08x, DISK_IOCTL_FORMAT_MEDIA)", hDisk);
    fRet = DeviceIoControl(hDisk, DISK_IOCTL_FORMAT_MEDIA, NULL, 0, NULL, 0, &bytes, NULL); 

    if(FALSE == fRet) {
        g_pKato->Log(LOG_DETAIL, L"FAILED: DeviceIoControl(0x%08x, DISK_IOCTL_FORMAT_MEDIA) failed error %u", hDisk, GetLastError());
    }

    return fRet;
}


BOOL Dsk_WriteSectors(HANDLE hDisk, DWORD startSector, DWORD cSectors, PBYTE pData)
{
    BOOL fRet;
    SG_REQ sgReq;
    DWORD cBytes;

    // build sg request buffer -- single sg buffer
    sgReq.sr_start = startSector;
    sgReq.sr_num_sec = cSectors;
    sgReq.sr_num_sg = 1;
    sgReq.sr_callback = NULL; // no callback under CE
    sgReq.sr_sglist[0].sb_len = cSectors * DEF_SECTOR_SIZE;
    sgReq.sr_sglist[0].sb_buf = pData;

    fRet = DeviceIoControl(hDisk, DISK_IOCTL_WRITE, &sgReq, sizeof(sgReq), NULL, 0, &cBytes, NULL);

    if(FALSE == fRet) {
        g_pKato->Log(LOG_DETAIL, L"FAILED: DeviceIoControl(0x%08x, DISK_IOCTL_WRITE) failed error %u", hDisk, GetLastError());
    }

    return fRet;
}

BOOL Dsk_ReadSectors(HANDLE hDisk, DWORD startSector, DWORD cSectors, PBYTE pData)
{
    BOOL fRet;
    SG_REQ sgReq;
    DWORD cBytes;

    // build sg request buffer -- single sg buffer
    sgReq.sr_start = startSector;
    sgReq.sr_num_sec = cSectors;
    sgReq.sr_num_sg = 1;
    sgReq.sr_callback = NULL; // no callback under CE
    sgReq.sr_sglist[0].sb_len = cSectors * DEF_SECTOR_SIZE;
    sgReq.sr_sglist[0].sb_buf = pData;

    fRet = DeviceIoControl(hDisk, DISK_IOCTL_READ, &sgReq, sizeof(sgReq), NULL, 0, &cBytes, NULL);

    if(FALSE == fRet) {
        g_pKato->Log(LOG_DETAIL, L"FAILED: DeviceIoControl(0x%08x, DISK_IOCTL_READ) failed error %u", hDisk, GetLastError());
    }
    
    return fRet;
}

BOOL Dsk_DeleteSectors(HANDLE hDisk, DWORD startSector, DWORD cSectors)
{
    BOOL fRet;
    DELETE_SECTOR_INFO dsInfo;
    DWORD cBytes;
    
    dsInfo.cbSize = sizeof(DELETE_SECTOR_INFO);
    dsInfo.startsector = startSector;
    dsInfo.numsectors = cSectors;

    fRet = DeviceIoControl(hDisk, IOCTL_DISK_DELETE_SECTORS, &dsInfo, sizeof(dsInfo), NULL, 0, &cBytes, NULL);

    if(FALSE == fRet) {
        g_pKato->Log(LOG_DETAIL, L"FAILED: DeviceIoControl(0x%08x, IOCTL_DISK_DELETE_SECTORS) failed error %u", hDisk, GetLastError());
    }
    
    return fRet;
}

