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
#include "globals.h"

// --------------------------------------------------------------------
BOOL
GetDiskInfo(
    HANDLE hDisk,
    LPWSTR pszDisk,
    DISK_INFO *pDiskInfo)
// --------------------------------------------------------------------
{
    ASSERT(pDiskInfo);

    DWORD cbReturned;

    // query driver for disk information structure

        // first, try IOCTL_DISK_GETINFO call
        if(!DeviceIoControl(hDisk, IOCTL_DISK_GETINFO, NULL, 0, pDiskInfo, sizeof(DISK_INFO), &cbReturned, NULL))
        {
            g_pKato->Log(LOG_DETAIL, _T("\"%s\" does not respond to IOCTL_DISK_GETINFO"), pszDisk);

            // the disk may only support the legacy DISK_IOCTL_GETINFO call
            if(!DeviceIoControl(hDisk, DISK_IOCTL_GETINFO, pDiskInfo, sizeof(DISK_INFO), NULL, 0, &cbReturned, NULL))
            {
                g_pKato->Log(LOG_DETAIL, _T("\"%s\" does not respond to DISK_IOCTL_GETINFO"), pszDisk);
                return FALSE;
            }

            // this disk will work, but it only supports old DISK_IOCTLs so set the global flag
            g_pKato->Log(LOG_DETAIL, _T("test will use old DISK_IOCTL_* control codes"));
         }

    return TRUE;
}

// enumerates all storage devices in the system
void EnumerateStorageDevice()
{
    // enumerate STORE_MOUNT_GUID /  BLOCK_DRIVER_GUID devices advertized by storage manager
    HANDLE hDetect = DEV_DetectFirstDevice(g_fOpenAsStore ? &STORE_MOUNT_GUID : &BLOCK_DRIVER_GUID,
        g_szDiskName, MAX_PATH);

    LOG(_T("Enumerating all storage devices in the system"));

    if(hDetect)
    {
        DWORD nFound = 0;
        HANDLE hDisk = INVALID_HANDLE_VALUE;
        do {
            // open a handle to the enumerated device
            hDisk = OpenDevice(g_szDiskName, g_fOpenAsStore, g_fOpenAsPartition);
            if(INVALID_HANDLE_VALUE == hDisk)
            {
                LOG(_T("Unable to open storage device \"%s\"; error %u"), g_szDiskName, GetLastError());
            }
            else
            {
                STORAGEDEVICEINFO sdi = {0};
                DWORD cbReturned = 0;

                // get profile name of the device
                if(!DeviceIoControl(hDisk, IOCTL_DISK_DEVICE_INFO, &sdi,
                    sizeof(STORAGEDEVICEINFO), NULL, 0, &cbReturned, NULL))
                {
                    LOG(_T("Device \"%s\" does not support IOCTL_DISK_DEVICE_INFO"
                        _T("(required for /profile option); error %u")), g_szDiskName, GetLastError());
                }
                else
                {
                    LOG(_T("\tDEVICE \"%s\" : PROFILE \"%s\" "), g_szDiskName, sdi.szProfile);
                    nFound++;
                }
                // close the handle, since don't use it any more
                CloseHandle(hDisk);
            }
        } while (DEV_DetectNextDevice(hDetect, g_szDiskName, MAX_PATH));

        DEV_DetectClose(hDetect);

        if (nFound > 0)
        {
            LOG(_T("Please select a profile or disk listed above as the test device"));
        }
    }
    else
    {
        LOG(_T("There is no storage device in the system, please check it"));
    }
}

// open device by Profile or diskname
BOOL OpenDevice()
{
    if(g_szProfile[0])
    {
        if(g_szDiskName[0])
        {
            LOG(_T("WARNING:  Overriding /disk option with /profile option\n"));
        }
        g_hDisk = OpenDiskHandleByProfile(g_pKato,g_fOpenAsStore,g_szProfile,g_fOpenAsPartition);
    }
    else
    {
        if (g_szDiskName[0])
        {
            g_hDisk = OpenDiskHandleByDiskName(g_pKato,g_fOpenAsStore,g_szDiskName,g_fOpenAsPartition);
        }
        else
        {
            g_hDisk = NULL;
        }
    }
    if(INVALID_HANDLE(g_hDisk))
    {
        EnumerateStorageDevice();
        LOG(_T("FAIL: unable to open media"));
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
    PBYTE pbBuffer)
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
            g_pKato->Log(LOG_DETAIL, _T("DeviceIoControl() failed; error %u"), GetLastError());
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
    if(dwExcess && pDiskInfo->di_bytes_per_sect > dwExcess)
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
        if(!DeviceIoControl(hDisk, IOCTL_DISK_READ, pSgReq, sizeof(SG_REQ) + (pSgReq->sr_num_sg - 1) * sizeof(SG_BUF), NULL, 0, &cBytes, NULL))
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
    unsigned int    number;
    for(UINT i = 0; i < cBytes; i++)
    {
        rand_s(&number);
        pBuffer[i] = (BYTE)number;
    }

    return TRUE;
}

// --------------------------------------------------------------------
BOOL FormatMedia(
    HANDLE hDisk)
// --------------------------------------------------------------------
{
    BOOL fRet;
    DWORD bytes;

    g_pKato->Log(LOG_COMMENT, L"DeviceIoControl(0x%08x, DISK_IOCTL_FORMAT_MEDIA)", hDisk);
    fRet = DeviceIoControl(hDisk, IOCTL_DISK_FORMAT_MEDIA, NULL, 0, NULL, 0, &bytes, NULL);

    if(FALSE == fRet) {
        g_pKato->Log(LOG_DETAIL, L"FAILED: DeviceIoControl(0x%08x, DISK_IOCTL_FORMAT_MEDIA) failed error %u", hDisk, GetLastError());
    }

    return fRet;
}

