//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+-------------------------------------------------------------------------
//
//
//  File:       udfsio.cpp
//
//  Contents:
//
//  Classes:
//
//  Functions:
//
//
//--------------------------------------------------------------------------

#include "udfs.h"


//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::ReadDisk
//
//  Synopsis:
//
//  Arguments:  [Sector]   --
//              [pBuffer]  --
//              [cbBuffer] --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

BOOL CReadOnlyFileSystemDriver::ReadDisk( ULONG Sector, PBYTE pBuffer, DWORD cbBuffer)
{
    CDROM_READ  cdRead;
    DWORD       dwAvail;

    DEBUGCHK((cbBuffer % CD_SECTOR_SIZE) == 0);

    memset(&cdRead, 0, sizeof(CDROM_READ));

    cdRead.StartAddr.Mode = CDROM_ADDR_LBA;
    cdRead.StartAddr.Address.lba = Sector;
    cdRead.TransferLength = cbBuffer / CD_SECTOR_SIZE;
    cdRead.bRawMode = FALSE;
    cdRead.sgcount = 1;
    cdRead.sglist[0].sb_buf = pBuffer;
    cdRead.sglist[0].sb_len = cbBuffer;

    // we only use 1 sg buffer for now
    return UDFSDeviceIoControl( IOCTL_CDROM_READ_SG, &cdRead, sizeof(CDROM_READ), NULL, 0, &dwAvail, NULL);
}



//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::Read
//
//  Synopsis:
//
//  Arguments:  [dwSector]            --
//              [dwStartSectorOffset] --
//              [nBytesToRead]        --
//              [pBuffer]             --
//              [pNumBytesRead]       --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------
BOOL CReadOnlyFileSystemDriver::Read( DWORD dwSector, DWORD dwStartSectorOffset, ULONG nBytesToRead, PBYTE pBuffer, DWORD * pNumBytesRead, LPOVERLAPPED lpOverlapped)
{
    
    PCDROM_READ pRequest;
    BYTE RequestBuffer[sizeof(CDROM_READ) + sizeof(SGX_BUF) * 2];
    BYTE EarlyBuffer[CD_SECTOR_SIZE], LateBuffer[CD_SECTOR_SIZE];
    DWORD dwTransferSize, dwExcess, dwReceived;
    BOOL fRet;

    ASSERT((dwStartSectorOffset < CD_SECTOR_SIZE) && nBytesToRead);
    ASSERT((nBytesToRead / CD_SECTOR_SIZE) < 65533);

    pRequest = (PCDROM_READ) RequestBuffer;

    pRequest->bRawMode = FALSE;
    pRequest->sgcount = 0;

    dwTransferSize = nBytesToRead + dwStartSectorOffset;

    if (dwStartSectorOffset) {
        pRequest->sglist[0].sb_buf = EarlyBuffer;
        pRequest->sglist[0].sb_len = dwStartSectorOffset;
        pRequest->sgcount++;
    }

    pRequest->sglist[pRequest->sgcount].sb_buf = pBuffer;
    pRequest->sglist[pRequest->sgcount].sb_len = nBytesToRead;
    pRequest->sgcount++;

    dwExcess = dwTransferSize % CD_SECTOR_SIZE;
    
    if (dwExcess) {
    
        pRequest->sglist[pRequest->sgcount].sb_buf = LateBuffer;
        
        if (dwTransferSize > CD_SECTOR_SIZE) {
        
            pRequest->sglist[pRequest->sgcount].sb_len = CD_SECTOR_SIZE - dwExcess;
            dwTransferSize += CD_SECTOR_SIZE - dwExcess;
        } else {
        
            pRequest->sglist[pRequest->sgcount].sb_len = CD_SECTOR_SIZE - dwTransferSize;
            dwTransferSize = CD_SECTOR_SIZE;
        }
        
        pRequest->sgcount++;
    }

    pRequest->StartAddr.Mode = CDROM_ADDR_LBA;
    pRequest->StartAddr.Address.lba = dwSector;

    pRequest->TransferLength = dwTransferSize / CD_SECTOR_SIZE;
    
    ASSERT((pRequest->TransferLength < 65536) && (dwTransferSize % CD_SECTOR_SIZE) == 0);

    if (dwExcess || dwStartSectorOffset) {
        lpOverlapped = NULL;
    }

    fRet = UDFSDeviceIoControl(
        IOCTL_CDROM_READ_SG, pRequest,
        sizeof(CDROM_READ) + sizeof(SGX_BUF) * (pRequest->sgcount - 1),
        NULL, 0, &dwReceived, lpOverlapped);

    if (fRet == TRUE) {
        if (pNumBytesRead) {
            *pNumBytesRead = nBytesToRead;
        }
    }
    
    return fRet;
}

