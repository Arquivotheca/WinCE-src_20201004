//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "main.h"

HANDLE g_hDisk = INVALID_HANDLE_VALUE;

TCHAR g_szDiskName[MAX_PATH] = _T("");
TCHAR g_szProfile[PROFILENAMESIZE] = _T(""); 
DISK_INFO g_diskInfo = {0};
BOOL g_fOldIoctls = FALSE;
BOOL g_fOpenAsStore = FALSE;
DWORD g_dwMaxSectorsPerOp = 256;

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
    if(g_fOldIoctls)
    {
        if(!DeviceIoControl(hDisk, DISK_IOCTL_GETINFO, pDiskInfo, sizeof(DISK_INFO), NULL, 0, &cbReturned, NULL))        
        {
            g_pKato->Log(LOG_DETAIL, _T("\"%s\" does not respond to DISK_IOCTL_GETINFO"), pszDisk);
            return FALSE;
        }
    }
    else
    {
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
            g_fOldIoctls = TRUE;
        }
    }
    return TRUE;
}

// --------------------------------------------------------------------
HANDLE 
OpenDevice(
    LPCTSTR pszDiskName)
// --------------------------------------------------------------------
{
    // open the device as either a store or a stream device
    if(g_fOpenAsStore)
    {
        return OpenStore(pszDiskName);
    }
    else
    {
        return CreateFile(pszDiskName, GENERIC_READ, FILE_SHARE_READ, 
                NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
}

// --------------------------------------------------------------------
HANDLE 
OpenDiskHandle()
// --------------------------------------------------------------------
{
    TCHAR szDisk[MAX_PATH] = _T("");
    
    HANDLE hDisk = INVALID_HANDLE_VALUE;
    HANDLE hDetect = NULL;

    STORAGEDEVICEINFO sdi = {0};

    DWORD cbReturned = 0;

    // if a disk name was specified, try to open it
    if(g_szDiskName[0])
    {
        hDisk = OpenDevice(g_szDiskName);
        if(INVALID_HANDLE_VALUE == hDisk)
        {
            g_pKato->Log(LOG_DETAIL, _T("ERROR: unable to open user-specified mass storage device \"%s\"; error %u"), g_szDiskName, GetLastError());
        }
        else
        {
            // don't check the profile on a user specified disk, but query for required disk info
            if(!GetDiskInfo(hDisk, g_szDiskName, &g_diskInfo))
            {
                g_pKato->Log(LOG_DETAIL, _T("ERROR: user-specified device \"%s\" does not appear to be a mass storage device"), g_szDiskName);
                VERIFY(CloseHandle(hDisk));
                hDisk = INVALID_HANDLE_VALUE;
            }
            else
            {
                g_pKato->Log(LOG_DETAIL, _T("opened user-specified mass storage device: \"%s\""), g_szDiskName);
            }
        }
       
    } 

    // no disk name specified by the user, try to detect a disk to test
    else
    {
        if(g_fOpenAsStore)
        {
            // enumerate STORE_MOUNT_GUID devices advertized by storage manager
            hDetect = DEV_DetectFirstDevice(&STORE_MOUNT_GUID, szDisk, MAX_PATH);
        }
        else
        {
            // enumerate BLOCK_DRIVER_GUID devices advertized by device manager
            hDetect = DEV_DetectFirstDevice(&BLOCK_DRIVER_GUID, szDisk, MAX_PATH);
        }

        if(hDetect)
        {
            do
            {
                // open a handle to the enumerated device
                g_pKato->Log(LOG_DETAIL, _T("checking device \"%s\"..."), szDisk);
                hDisk = OpenDevice(szDisk);
                if(INVALID_HANDLE_VALUE == hDisk)
                {
                    g_pKato->Log(LOG_DETAIL, _T("unable to open mass storage device \"%s\"; error %u"), szDisk, GetLastError());
                    continue;
                }

                // support filtering devices by user-specified profile
                if(g_szProfile[0])
                {
                    if(!DeviceIoControl(hDisk, IOCTL_DISK_DEVICE_INFO, &sdi, sizeof(STORAGEDEVICEINFO), NULL, 0, &cbReturned, NULL))
                    {
                        g_pKato->Log(LOG_DETAIL, _T("device \"%s\" does not support IOCTL_DISK_DEVICE_INFO (required for /profile option); error %u"), szDisk, GetLastError());
                        VERIFY(CloseHandle(hDisk));
                        hDisk = INVALID_HANDLE_VALUE;
                        continue;
                    }
                    else
                    {
                        // check for a profile match
                        if(0 != wcsicmp(g_szProfile, sdi.szProfile))
                        {
                            g_pKato->Log(LOG_DETAIL, _T("device \"%s\" profile \"%s\" does not match specified profile \"%s\""), szDisk, sdi.szProfile, g_szProfile);
                            VERIFY(CloseHandle(hDisk));
                            hDisk = INVALID_HANDLE_VALUE;
                            continue;
                        }
                    }
                }

                if(!GetDiskInfo(hDisk, szDisk, &g_diskInfo))
                {
                    g_pKato->Log(LOG_DETAIL, _T("ERROR: device \"%s\" does not appear to be a mass storage device"), szDisk);
                    VERIFY(CloseHandle(hDisk));
                    hDisk = INVALID_HANDLE_VALUE;
                    continue;
                }
                
                g_pKato->Log(LOG_DETAIL, _T("found appropriate mass storage device: \"%s\""), szDisk);

                break;

            } while(DEV_DetectNextDevice(hDetect, szDisk, MAX_PATH));
            DEV_DetectClose(hDetect);
        }
    }

    if(INVALID_HANDLE_VALUE == hDisk)
    {
        g_pKato->Log(LOG_DETAIL, _T("ERROR: found no mass storage devices!"));
    }
    else
    {        
        g_pKato->Log(LOG_DETAIL, _T("    Total Sectors: %u"), g_diskInfo.di_total_sectors); 
        g_pKato->Log(LOG_DETAIL, _T("    Bytes Per Sector: %u"), g_diskInfo.di_bytes_per_sect); 
        g_pKato->Log(LOG_DETAIL, _T("    Cylinders: %u"), g_diskInfo.di_cylinders); 
        g_pKato->Log(LOG_DETAIL, _T("    Heads: %u"), g_diskInfo.di_heads); 
        g_pKato->Log(LOG_DETAIL, _T("    Sectors: %u"), g_diskInfo.di_sectors);             
        g_pKato->Log(LOG_DETAIL, _T("    Flags: 0x%08X"), g_diskInfo.di_flags); 
    }

    return hDisk;
}

// --------------------------------------------------------------------
BOOL 
ReadWriteDiskMulti(
    HANDLE hDisk, 
    DWORD ctlCode,
    PDISK_INFO pDiskInfo, 
    PSG_REQ psgReq)
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

    // remap old IOCTL codes to new IOCTL codes if requested
    if(IOCTL_DISK_READ == ctlCode && g_fOldIoctls) {
        ctlCode = DISK_IOCTL_READ;
    } else if(IOCTL_DISK_WRITE == ctlCode && g_fOldIoctls) {
        ctlCode = DISK_IOCTL_WRITE;
    }

    // a few warnings, but still allow these cases to go through because they should be tested
    if((IOCTL_DISK_READ == ctlCode || DISK_IOCTL_READ == ctlCode) && 
       IsBadWritePtr(pbBuffer, cSectors * pDiskInfo->di_bytes_per_sect))
    {
        g_pKato->Log(LOG_DETAIL, _T("WARNING: buffer cannot handle entire request"));
    }

    else if(IsBadReadPtr(pbBuffer, cSectors * pDiskInfo->di_bytes_per_sect))
    {
        g_pKato->Log(LOG_DETAIL, _T("WARNING: buffer cannot handle entire request"));
    }
    
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
   
    if(IsBadWritePtr(pbBuffer, cBufferSize))
    {
        g_pKato->Log(LOG_DETAIL, _T("WARNING: buffer cannot handle entire request"));
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
    if(dwExcess)
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
    if(IsBadWritePtr(pBuffer, cBytes))
    {
        return FALSE;
    }
    
    for(UINT i = 0; i < cBytes; i++)
    {
        pBuffer[i] = (BYTE)rand();
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

