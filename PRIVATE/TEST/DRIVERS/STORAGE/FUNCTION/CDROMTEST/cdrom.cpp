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

BOOL  g_fIsDVD = FALSE;

DWORD g_endSector = CD_MAX_SECTORS;

DWORD g_cMaxSectors = MAX_READ_SECTORS;

DWORD g_cMaxReadRetries = MAX_READ_RETRIES;

DWORD g_cTrackNo = 1;

TCHAR g_szImgFile[MAX_PATH] = DEF_IMG_FILE_NAME;
TCHAR g_szDiskName[DEVICE_NAME_LEN + 2] = _T(""); // 2 more chars for ':' and NULL

// --------------------------------------------------------------------
HANDLE 
OpenCDRomDiskHandle()
// --------------------------------------------------------------------
{
    TCHAR szDisk[MAX_PATH] = _T("");
    
    HANDLE hDisk = INVALID_HANDLE_VALUE;

    DISK_INFO diskInfo = {0};
    DWORD cbReturned = 0;

    // check all possible disk names
    for(UINT i = 0; i < MAX_DISK; i++)
    {
        // build disk name if one was not specified
        if(_T('') == g_szDiskName[0])
        {
            _stprintf(szDisk, _T("DSK%u:"), i+1);
        }
        // otherwise use specified disk name
        else
        {
            _tcscpy(szDisk, g_szDiskName);
        }
        
        hDisk = CreateFile(szDisk,
                           GENERIC_READ,
                           NULL, 
                           NULL,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL,
                           NULL);

        // couldn't open disk, must not be cdrom drive
        if(INVALID_HANDLE(hDisk))
        {
            g_pKato->Log(LOG_DETAIL, _T("%s does not exist"), szDisk);
            continue; // keep checking
        }

        // if a disk name was given, use it
        if(g_szDiskName[0])
        {
            g_pKato->Log(LOG_DETAIL, _T("Using user specified disk: %s"), szDisk);
            return hDisk;
        }

        // opened disk, see if it is a cdrom drive
        else
        {
            // query driver for cdrom disk information structure
            if(!DeviceIoControl(hDisk, IOCTL_CDROM_DISC_INFO, NULL, 0, &diskInfo, sizeof(DISK_INFO), &cbReturned, NULL))        
            {
                // if the call fails, this is not a cdrom device
                g_pKato->Log(LOG_DETAIL, _T("%s is not a cdrom device"), szDisk);
                CloseHandle(hDisk);
                continue;
            }

            g_pKato->Log(LOG_DETAIL, _T("Found likely CDROM device: %s"), szDisk);
            return hDisk;
        }
    }

    return INVALID_HANDLE_VALUE;
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

    BYTE preBuffer[CD_SECTOR_SIZE];
    BYTE postBuffer[CD_SECTOR_SIZE];    
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



