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

HANDLE g_hDisk = INVALID_HANDLE_VALUE;

BOOL  g_fIsDVD = FALSE;

BOOL  g_fOpenAsStore = FALSE;

BOOL  g_fOpenAsVolume = FALSE;

DWORD g_endSector = CD_MAX_SECTORS;

DWORD g_cMaxSectors = MAX_READ_SECTORS;

DWORD g_cMaxReadRetries = MAX_READ_RETRIES;

TCHAR g_szImgFile[MAX_PATH] = DEF_IMG_FILE_NAME;
TCHAR g_szProfile[PROFILENAMESIZE] = _T(""); 
TCHAR g_szDiskName[MAX_PATH] = _T("");

// --------------------------------------------------------------------
VOID
DumpCDToc(
    CDROM_TOC *pcdToc)    
// --------------------------------------------------------------------
{
    DWORD trackSectors = 0;
    CDROM_ADDR cdTrkStart = {0};
    CDROM_ADDR cdTrkEnd = {0};
    MSF_ADDR msfAddrStart = {0};
    MSF_ADDR msfAddrEnd = {0};

    for(DWORD nTrack = pcdToc->FirstTrack; nTrack < pcdToc->LastTrack; nTrack++)
    {
        // start address of track
        msfAddrStart.msf_Frame = pcdToc->TrackData[nTrack-1].Address[3];
        msfAddrStart.msf_Second = pcdToc->TrackData[nTrack-1].Address[2];
        msfAddrStart.msf_Minute = pcdToc->TrackData[nTrack-1].Address[1];
        cdTrkStart.Mode = CDROM_ADDR_MSF;
        cdTrkStart.Address.msf = msfAddrStart;
        CDROM_MSF_TO_LBA(&cdTrkStart);
        
        // end address of track
        msfAddrEnd.msf_Frame = pcdToc->TrackData[nTrack].Address[3];
        msfAddrEnd.msf_Second = pcdToc->TrackData[nTrack].Address[2];
        msfAddrEnd.msf_Minute = pcdToc->TrackData[nTrack].Address[1];
        cdTrkEnd.Mode = CDROM_ADDR_MSF;
        cdTrkEnd.Address.msf = msfAddrEnd;
        CDROM_MSF_TO_LBA(&cdTrkEnd);

        // total number of sectors in the track
        trackSectors = (cdTrkEnd.Address.lba - cdTrkStart.Address.lba);

        g_pKato->Log(LOG_DETAIL, _T("TRACK %2u -- BEGINS @ %02u:%02u:%02u (M:S:F), sector %u (LBA)"),
            nTrack, msfAddrStart.msf_Minute, msfAddrStart.msf_Second, msfAddrStart.msf_Frame, cdTrkStart.Address.lba);
        
        g_pKato->Log(LOG_DETAIL, _T("         -- ENDS   @ %02u:%02u:%02u (M:S:F), sector %u (LBA)"),
            msfAddrEnd.msf_Minute, msfAddrEnd.msf_Second, msfAddrEnd.msf_Frame, cdTrkEnd.Address.lba);
        
        g_pKato->Log(LOG_DETAIL, _T("         -- SIZE     %.2f MB"), 
            (float)trackSectors / (float)(0x100000) * (float)CDROM_RAW_SECTOR_SIZE);
    }
}

// --------------------------------------------------------------------
HANDLE 
OpenDevice(
    LPCTSTR pszDiskName)
// --------------------------------------------------------------------
{
    WCHAR szDevice[MAX_PATH];
    StringCchCopyEx(szDevice, MAX_PATH, pszDiskName, NULL, NULL, STRSAFE_NULL_ON_FAILURE);
    
    // open the device as either a store or a stream device
    if(g_fOpenAsStore)
    {
        return OpenStore(szDevice);
    }
    else if(g_fOpenAsVolume)
    {
        StringCchCatEx(szDevice, MAX_PATH, L"\\VOL:", NULL, NULL, STRSAFE_NULL_ON_FAILURE);
    }
    
    return CreateFile(pszDiskName, GENERIC_READ, FILE_SHARE_READ, 
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}


// --------------------------------------------------------------------
HANDLE 
OpenCDRomDiskHandle()
// --------------------------------------------------------------------
{
    TCHAR szDisk[MAX_PATH] = _T("");
    
    HANDLE hDisk = INVALID_HANDLE_VALUE;
    HANDLE hDetect = NULL;

    STORAGEDEVICEINFO sdi = {0};

    CDROM_TOC cdToc = {0};
    DISK_INFO diskInfo = {0};
    DWORD cbReturned = 0;

    // if a disk name was specified, try to open it
    if(g_szDiskName[0])
    {
        hDisk = OpenDevice(g_szDiskName);
        if(INVALID_HANDLE_VALUE == hDisk)
        {
            g_pKato->Log(LOG_DETAIL, _T("unable to open disk \"%s\"; error %u"), g_szDiskName, GetLastError());
        }
        return hDisk;
    }

    // enumerate storage devices using the specified method
    //
    if(g_fOpenAsVolume)
    {   
        // open the storage device as a volume; e.g. CreateFile on \CDROM Drive\VOL:
        // this method requires the storage device to be mounted
        
        // detect any mounted UDFS devices
        hDetect = DEV_DetectFirstDevice(&UDFS_MOUNT_GUID, szDisk, MAX_PATH);
        if(!hDetect)
        {
            // detect any mounted CDFS devices
            hDetect = DEV_DetectFirstDevice(&CDFS_MOUNT_GUID, szDisk, MAX_PATH);
        }
    }
    else if(g_fOpenAsStore)
    {
        // open the storage device as a store using OpenStore
        
        // look for stores advertized by storage manager
        hDetect = DEV_DetectFirstDevice(&STORE_MOUNT_GUID, szDisk, MAX_PATH);
    }
    else // open as a device
    {
        // open the storage device 
        // look for block devices advertized by device manager
        hDetect = DEV_DetectFirstDevice(&BLOCK_DRIVER_GUID, szDisk, MAX_PATH);
    }
    if(hDetect)
    {
        do
        {
            // open a handle to the enumerated device
            g_pKato->Log(LOG_DETAIL, _T("opening disk \"%s\"..."), szDisk);
            hDisk = OpenDevice(szDisk);
            if(INVALID_HANDLE_VALUE == hDisk)
            {
                g_pKato->Log(LOG_DETAIL, _T("unable to open disk \"%s\"; error %u"), szDisk, GetLastError());
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

            // query driver for cdrom disk information structure
            if(!DeviceIoControl(hDisk, IOCTL_CDROM_DISC_INFO, NULL, 0, &diskInfo, sizeof(DISK_INFO), &cbReturned, NULL))        
            {
                // if the call fails, this is not a cdrom device
                g_pKato->Log(LOG_DETAIL, _T("\"%s\" is not a cdrom device"), szDisk);
                VERIFY(CloseHandle(hDisk));
                hDisk = INVALID_HANDLE_VALUE;
                continue;
            }

            // this check is too restrictive-- some drivers may not support TOC if they don't support
            // audio CDs, so we won't check this
            #if 0
            // query driver for cdrom TOC structure
            // verify that there is AT LEAST 1 track, and that the track is a data track
            if(!DeviceIoControl(hDisk, IOCTL_CDROM_READ_TOC, NULL, 0, &cdToc, CDROM_TOC_SIZE, &cbReturned, NULL) ||
                cdToc.LastTrack == 0 || !(cdToc.TrackData[0].Control & 0x4))
            {
                // if the call fails, this is not a data disk
                g_pKato->Log(LOG_DETAIL, _T("\"%s\" is not a UDFS/CDFS (data) disk"), szDisk);
                VERIFY(CloseHandle(hDisk));
                hDisk = INVALID_HANDLE_VALUE;
                continue;
            }    
            #endif

            g_pKato->Log(LOG_DETAIL, _T("found CDROM device: \"%s\""), szDisk);
            DumpCDToc(&cdToc);
            break;

        } while(DEV_DetectNextDevice(hDetect, szDisk, MAX_PATH));
        DEV_DetectClose(hDetect);
    }

    if(INVALID_HANDLE_VALUE == hDisk)
    {
        g_pKato->Log(LOG_DETAIL, _T("found no data CD-ROM disks!"));
        g_pKato->Log(LOG_DETAIL, _T("please insert a UDFS/CDFS CD into your CD-ROM drive and re-run the test"));
    }

    return hDisk;
}

// --------------------------------------------------------------------
BOOL 
ReadDiskSectors(
    HANDLE hDisk, 
    DWORD sector, 
    PBYTE pBuffer, 
    DWORD cbBuffer, 
    LPDWORD pcbRead)
// --------------------------------------------------------------------    
{
    CDROM_READ cdRead = {0};

    ASSERT(VALID_HANDLE(hDisk));
    ASSERT(pBuffer);
    ASSERT(pcbRead);
    ASSERT(0 == (cbBuffer % CD_SECTOR_SIZE));

    cdRead.StartAddr.Mode = CDROM_ADDR_LBA;
    cdRead.StartAddr.Address.lba = sector;
    cdRead.TransferLength = cbBuffer / CD_SECTOR_SIZE;
    cdRead.bRawMode = FALSE;
    cdRead.sgcount = 1;
    cdRead.sglist[0].sb_buf = pBuffer;
    cdRead.sglist[0].sb_len = cbBuffer;

    *pcbRead = cbBuffer;
    return DeviceIoControl(hDisk, IOCTL_CDROM_READ_SG, &cdRead, sizeof(CDROM_READ), NULL, 0, pcbRead, NULL);
}


// --------------------------------------------------------------------
BOOL 
ReadDiskMulti(    
    HANDLE hDisk, 
    DWORD startSector,
    DWORD startSectorOffset,    
    PBYTE pBuffer, 
    DWORD cbBuffer, 
    LPDWORD pcbRead)
// --------------------------------------------------------------------    
{

    static BYTE preBuffer[CD_SECTOR_SIZE];
    static BYTE postBuffer[CD_SECTOR_SIZE];    
    BYTE reqBuffer[sizeof(CDROM_READ) + sizeof(SGX_BUF) * 2];

    memset(preBuffer, 0, CD_SECTOR_SIZE);
    memset(postBuffer, 0, CD_SECTOR_SIZE);
    memset(reqBuffer, 0, sizeof(CDROM_READ) + sizeof(SGX_BUF) * 2);
    
    PCDROM_READ pCdReq = (PCDROM_READ)reqBuffer;

    BOOL fRet = TRUE;

    // reads must start on a sector boundary, even if the request does not
    DWORD dwTransferSize = cbBuffer + startSectorOffset;

    // the excess data in the last (incomplete) sector
    DWORD dwExcess = dwTransferSize % CD_SECTOR_SIZE;

    DWORD dwReceived;

    ASSERT(VALID_HANDLE(hDisk));
    ASSERT(pBuffer);
    ASSERT(pcbRead);

    pCdReq->bRawMode = FALSE;
    pCdReq->sgcount = 0;

    // add an sg buffer, if necessary, for the excess data at the start of the first sector
    if(startSectorOffset)
    {
        pCdReq->sglist[0].sb_buf = preBuffer;
        pCdReq->sglist[0].sb_len = startSectorOffset;
        pCdReq->sgcount++;
    }

    // sg buffer for the actual requested data
    pCdReq->sglist[pCdReq->sgcount].sb_buf = pBuffer;
    pCdReq->sglist[pCdReq->sgcount].sb_len = cbBuffer;
    pCdReq->sgcount++;

    // add an sg buffer, if necessary, for the excess data at end of final sector
    if(dwExcess)
    {
        pCdReq->sglist[pCdReq->sgcount].sb_buf = postBuffer;

        if(CD_SECTOR_SIZE < dwTransferSize)
        {
            pCdReq->sglist[pCdReq->sgcount].sb_len = CD_SECTOR_SIZE - dwExcess;
            dwTransferSize += CD_SECTOR_SIZE - dwExcess;
        }
        else // CD_SECTOR_SIZE >= dwTransferSize
        {
            pCdReq->sglist[pCdReq->sgcount].sb_len = CD_SECTOR_SIZE - dwTransferSize;
            dwTransferSize  = CD_SECTOR_SIZE;
        }

        pCdReq->sgcount++;
    }

    // entire read should include no partial sectors
    ASSERT(0 == (dwTransferSize % CD_SECTOR_SIZE));

    // finalize the request
    pCdReq->StartAddr.Mode = CDROM_ADDR_LBA;
    pCdReq->StartAddr.Address.lba = startSector;

    pCdReq->TransferLength = dwTransferSize / CD_SECTOR_SIZE;

    ASSERT(pCdReq->TransferLength < MAX_READ_SECTORS);

    fRet = DeviceIoControl(hDisk, 
                           IOCTL_CDROM_READ_SG, pCdReq, 
                           sizeof(CDROM_READ) + sizeof(SGX_BUF) * (pCdReq->sgcount - 1),
                           NULL, 0, &dwReceived, NULL);
    if(fRet)
    {
        if(pcbRead)
        {
            *pcbRead = cbBuffer;
        }
    }
    
    return fRet;
}



