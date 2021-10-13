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
#include "main.h"

BOOL g_fOldIoctls = FALSE;
DWORD g_dwMaxSectorsPerOp = 256;
DWORD g_dwMaxSectorsPerOpFor48BitsLBA = 65536;
DWORD g_dw28BitLBASectorsLimit = 268435456;         // 2^28 bits

// --------------------------------------------------------------------
BOOL
ReadWriteDiskMulti(
    HANDLE hDisk,
    DWORD ctlCode,
    PDISK_INFO pDiskInfo,
    PSG_REQ psgReq,
    LPDWORD pdwErrVal)
// --------------------------------------------------------------------
{
    DWORD cbSg = 0;
    DWORD cBytes = 0;

    ASSERT(IOCTL_DISK_READ == ctlCode || IOCTL_DISK_WRITE == ctlCode ||
           DISK_IOCTL_READ == ctlCode || DISK_IOCTL_WRITE == ctlCode);
    if(ctlCode != IOCTL_DISK_READ && ctlCode != IOCTL_DISK_WRITE &&
       ctlCode != DISK_IOCTL_READ && ctlCode != DISK_IOCTL_WRITE)
    {
        g_pKato->Log(LOG_DETAIL, _T("ReadWriteDiskMulti received invalid IOCTL code"));
        return FALSE;
    }

    ASSERT(VALID_HANDLE(hDisk));
    if(INVALID_HANDLE(hDisk))
    {
        g_pKato->Log(LOG_DETAIL, _T("ReadWriteDiskMulti received invalid handle"));
        return FALSE;
    }

    ASSERT(pDiskInfo);
    if(!pDiskInfo)
    {
        g_pKato->Log(LOG_DETAIL, _T("ReadWriteDiskMulti received NULL pDiskInfo buffer"));
        return FALSE;
    }

    ASSERT(psgReq);
    if(!psgReq)
    {
        g_pKato->Log(LOG_DETAIL, _T("ReadWriteDiskMulti received NULL psgReq buffer"));
        return FALSE;
    }

    // remap old IOCTL codes to new IOCTL codes if requested
    if(IOCTL_DISK_READ == ctlCode && g_fOldIoctls) {
        ctlCode = DISK_IOCTL_READ;
    } else if(IOCTL_DISK_WRITE == ctlCode && g_fOldIoctls) {
        ctlCode = DISK_IOCTL_WRITE;
    }

    // make sure all exceptions are caught by the driver
    __try
    {
        cbSg = sizeof(SG_REQ) + sizeof(SG_BUF) * (psgReq->sr_num_sg - 1);
        if(!DeviceIoControl(hDisk, ctlCode, psgReq, cbSg, NULL, 0, &cBytes, NULL))
        {
            *pdwErrVal = GetLastError();
            g_pKato->Log(LOG_DETAIL, _T("DeviceIoControl() failed; error %u"), *pdwErrVal);
            return FALSE;
        }

    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        g_pKato->Log(LOG_DETAIL, _T("DeviceIoControl() caused an unhandled exception"));
        return FALSE;
    }

    return TRUE;

}


// --------------------------------------------------------------------
BOOL
ReadWriteDisk(
    HANDLE hDisk,
    DWORD ctlCode,
    PDISK_INFO pDiskInfo,
    DWORD sector,
    int cSectors,
    PBYTE pbBuffer,
    LPDWORD pdwErrVal)
// --------------------------------------------------------------------
{
    SG_REQ sgReq = {0};
    DWORD cBytes = 0;

    ASSERT(IOCTL_DISK_READ == ctlCode || IOCTL_DISK_WRITE == ctlCode ||
           DISK_IOCTL_READ == ctlCode || DISK_IOCTL_WRITE == ctlCode);
    if(ctlCode != IOCTL_DISK_READ && ctlCode != IOCTL_DISK_WRITE &&
       ctlCode != DISK_IOCTL_READ && ctlCode != DISK_IOCTL_WRITE)
    {
        g_pKato->Log(LOG_DETAIL, _T("ReadWriteDisk received invalid IOCTL code"));
        return FALSE;
    }

    ASSERT(VALID_HANDLE(hDisk));
    if(INVALID_HANDLE(hDisk))
    {
        g_pKato->Log(LOG_DETAIL, _T("ReadWriteDisk received invalid handle"));
        return FALSE;
    }

    ASSERT(pDiskInfo);
    if(!pDiskInfo)
    {
        g_pKato->Log(LOG_DETAIL, _T("ReadWriteDisk received NULL pDiskInfo buffer"));
        return FALSE;
    }

    // remap old IOCTL codes to new IOCTL codes if requested
    if(IOCTL_DISK_READ == ctlCode && g_fOldIoctls) {
        ctlCode = DISK_IOCTL_READ;
    } else if(IOCTL_DISK_WRITE == ctlCode && g_fOldIoctls) {
        ctlCode = DISK_IOCTL_WRITE;
    }

    // a few warnings, but still allow these cases to go through because they should be tested
    if(sector > pDiskInfo->di_total_sectors)
    {
        g_pKato->Log(LOG_DETAIL, _T("WARNING: requested start sector is past end of disk"));
    }
    else if(sector + cSectors > pDiskInfo->di_total_sectors)
    {
        g_pKato->Log(LOG_DETAIL, _T("WARNING: requested read will go past end of disk"));
    }

    // build sg request buffer -- single sg buffer
    sgReq.sr_start = sector;
    sgReq.sr_num_sec = cSectors;
    sgReq.sr_num_sg = 1;
    sgReq.sr_callback = NULL; // no callback under CE
    sgReq.sr_sglist[0].sb_len = cSectors * pDiskInfo->di_bytes_per_sect;
    sgReq.sr_sglist[0].sb_buf = pbBuffer;

    // make sure all exceptions are caught by the driver
    __try
    {
        if(!DeviceIoControl(hDisk, ctlCode, &sgReq, sizeof(sgReq), NULL, 0, &cBytes, NULL))
        {
            *pdwErrVal = GetLastError();
            g_pKato->Log(LOG_DETAIL, _T("DeviceIoControl() failed; error %u"), *pdwErrVal);
            return FALSE;
        }

    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        g_pKato->Log(LOG_DETAIL, _T("DeviceIoControl() caused an unhandled exception"));
        return FALSE;
    }

    return TRUE;
}

// --------------------------------------------------------------------
BOOL
ReadDiskSg(
    HANDLE hDisk,
    PDISK_INFO pDiskInfo,
    DWORD startSector,
    DWORD startSectorOffset,
    PBYTE pbBuffer,
    DWORD cBufferSize)
// --------------------------------------------------------------------
{
    BYTE reqData[sizeof(SG_REQ) + 2*sizeof(SG_BUF)];
    PSG_REQ pSgReq = (PSG_REQ)reqData;
    PBYTE pPreBuffer = NULL;
    PBYTE pPostBuffer = NULL;
    DWORD cBytes = 0;

    // parameter verification
    ASSERT(VALID_HANDLE(hDisk));
    if(INVALID_HANDLE(hDisk))
    {
        g_pKato->Log(LOG_DETAIL, _T("ReadWriteDiskEx received invalid handle"));
        return FALSE;
    }

    ASSERT(pDiskInfo);
    if(!pDiskInfo)
    {
        g_pKato->Log(LOG_DETAIL, _T("ReadWriteDisk received NULL pDiskInfo buffer"));
        return FALSE;
    }

    // request starts past end of disk
    if(startSector > pDiskInfo->di_total_sectors)
    {
        g_pKato->Log(LOG_DETAIL, _T("WARNING: requested start sector is past end of disk"));
    }

    // request will extend past end of disk
    else if(startSector + (cBufferSize / pDiskInfo->di_bytes_per_sect) > pDiskInfo->di_total_sectors)
    {
        g_pKato->Log(LOG_DETAIL, _T("WARNING: requested read will go past end of disk"));
    }

    DWORD dwTransferSize = cBufferSize + startSectorOffset;
    DWORD dwExcess = dwTransferSize % pDiskInfo->di_bytes_per_sect;

    // allocate pre request buffer
    pPreBuffer = (PBYTE)VirtualAlloc(NULL, pDiskInfo->di_bytes_per_sect, MEM_COMMIT, PAGE_READWRITE);
    if(NULL == pPreBuffer)
    {
        g_pKato->Log(LOG_DETAIL, _T("VirualAlloc failed to allocate buffer; error %u"), GetLastError());
        return FALSE;
    }
    // allocate post request buffer
    pPostBuffer = (PBYTE)VirtualAlloc(NULL, pDiskInfo->di_bytes_per_sect, MEM_COMMIT, PAGE_READWRITE);
    if(NULL == pPostBuffer)
    {
        VirtualFree(pPreBuffer, 0, MEM_RELEASE);
        g_pKato->Log(LOG_DETAIL, _T("VirualAlloc failed to allocate buffer; error %u"), GetLastError());
        return FALSE;
    }

    pSgReq->sr_start = startSector;
    pSgReq->sr_num_sg = 0;

    // add an sg buffer, if necessary, for the excess data at the start of the first sector
    if(startSectorOffset)
    {
        pSgReq->sr_sglist[0].sb_buf = pPreBuffer;
        pSgReq->sr_sglist[0].sb_len = startSectorOffset;
        pSgReq->sr_num_sg++;
    }

    // add an sg buffer for the requested data
    pSgReq->sr_sglist[pSgReq->sr_num_sg].sb_buf = pbBuffer;
    pSgReq->sr_sglist[pSgReq->sr_num_sg].sb_len = cBufferSize;
    pSgReq->sr_num_sg++;

    // add an sg buffer, if necessary, for the excess data at the end of the final sector
    if(dwExcess > 0)
    {
        pSgReq->sr_sglist[pSgReq->sr_num_sg].sb_buf = pPostBuffer;

        if(dwTransferSize > pDiskInfo->di_bytes_per_sect)
        {
            pSgReq->sr_sglist[pSgReq->sr_num_sg].sb_len = pDiskInfo->di_bytes_per_sect - dwExcess;
            dwTransferSize += pDiskInfo->di_bytes_per_sect - dwExcess;
        }
        else
        {
            pSgReq->sr_sglist[pSgReq->sr_num_sg].sb_len = pDiskInfo->di_bytes_per_sect - dwTransferSize;
            dwTransferSize = pDiskInfo->di_bytes_per_sect;
        }

        pSgReq->sr_num_sg++;
    }

    ASSERT(0 == (dwTransferSize % pDiskInfo->di_bytes_per_sect));

    pSgReq->sr_num_sec = dwTransferSize / pDiskInfo->di_bytes_per_sect;

    // make sure all exceptions are caught by the driver
    __try
    {
        if(!DeviceIoControl(hDisk, g_fOldIoctls ? DISK_IOCTL_READ : IOCTL_DISK_READ, pSgReq, sizeof(SG_REQ) + (pSgReq->sr_num_sg - 1) * sizeof(SG_BUF), NULL, 0, &cBytes, NULL))
        {
            g_pKato->Log(LOG_DETAIL, _T("DeviceIoControl() failed; error %u"), GetLastError());
            goto Fail;
        }

    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        g_pKato->Log(LOG_DETAIL, _T("DeviceIoControl() caused an unhandled exception"));
        goto Fail;
    }

    if(pPreBuffer)  VirtualFree(pPreBuffer, 0, MEM_RELEASE);
    if(pPostBuffer) VirtualFree(pPostBuffer, 0, MEM_RELEASE);

    return TRUE;

Fail:
    if(pPreBuffer)  VirtualFree(pPreBuffer, 0, MEM_RELEASE);
    if(pPostBuffer) VirtualFree(pPostBuffer, 0, MEM_RELEASE);
    return FALSE;
}


// --------------------------------------------------------------------
BOOL
MakeJunkBuffer(
    PBYTE pBuffer,
    DWORD cBytes)
// --------------------------------------------------------------------
{
    unsigned int    number;    // Variable to pass to rand_s API.
    for(UINT i = 0; i < cBytes; i++)
    {
        rand_s( &number);
        pBuffer[i] = (BYTE)number;
    }

    return TRUE;
}

// --------------------------------------------------------------------
BOOL DeleteSectors(
    HANDLE hDisk,
    DWORD startSector,
    DWORD cSectors)
// --------------------------------------------------------------------
{
    BOOL fRet;
    DELETE_SECTOR_INFO dsInfo;
    DWORD cBytes;

    dsInfo.cbSize = sizeof(DELETE_SECTOR_INFO);
    dsInfo.startsector = startSector;
    dsInfo.numsectors = cSectors;

    g_pKato->Log(LOG_COMMENT, L"DeviceIoControl(0x%08x, IOCTL_DISK_DELETE_SECTORS)", hDisk);
    fRet = DeviceIoControl(hDisk, IOCTL_DISK_DELETE_SECTORS, &dsInfo, sizeof(dsInfo), NULL, 0, &cBytes, NULL);

    if(FALSE == fRet) {
        g_pKato->Log(LOG_DETAIL, L"DeviceIoControl(0x%08x, IOCTL_DISK_DELETE_SECTORS) failed error %u", hDisk, GetLastError());
    }

    return fRet;
}

// --------------------------------------------------------------------
BOOL FormatMedia(
    HANDLE hDisk)
// --------------------------------------------------------------------
{
    BOOL fRet;
    DWORD bytes;

    g_pKato->Log(LOG_COMMENT, L"DeviceIoControl(0x%08x, DISK_IOCTL_FORMAT_MEDIA)", hDisk);
    fRet = DeviceIoControl(hDisk, g_fOldIoctls ? DISK_IOCTL_FORMAT_MEDIA : IOCTL_DISK_FORMAT_MEDIA, NULL, 0, NULL, 0, &bytes, NULL);

    if(FALSE == fRet) {
        g_pKato->Log(LOG_DETAIL, L"FAILED: DeviceIoControl(0x%08x, DISK_IOCTL_FORMAT_MEDIA) failed error %u", hDisk, GetLastError());
    }

    return fRet;
}

// --------------------------------------------------------------------
DWORD MaxSectorPerOp(
    PDISK_INFO pDiskInfo)
// --------------------------------------------------------------------
{
    ASSERT(pDiskInfo);

    if (pDiskInfo && (pDiskInfo->di_total_sectors > g_dw28BitLBASectorsLimit))
        return g_dwMaxSectorsPerOpFor48BitsLBA;
    else
        return g_dwMaxSectorsPerOp;
}
