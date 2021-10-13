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
//+-------------------------------------------------------------------------
//
//
//  File:       udfsread.cpp
//
//  Contents:
//
//  Classes:
//
//  Functions:
//
//
//--------------------------------------------------------------------------

#include "cdfs.h"


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

BOOL CReadOnlyFileSystemDriver::ReadDisk( HDSK hDisk, 
                                          ULONG Sector, 
                                          PBYTE pBuffer, 
                                          DWORD cbBuffer)
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
    return FSDMGR_DiskIoControl( hDisk,
                                 IOCTL_CDROM_READ_SG, 
                                 &cdRead, 
                                 sizeof(CDROM_READ), 
                                 NULL, 
                                 0, 
                                 &dwAvail, 
                                 NULL );
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
BOOL CReadOnlyFileSystemDriver::Read( DWORD dwSector, 
                                      DWORD dwStartSectorOffset, 
                                      ULONG nBytesToRead, 
                                      PBYTE pBuffer, 
                                      DWORD * pNumBytesRead, 
                                      LPOVERLAPPED lpOverlapped,
                                      BOOL fRaw,
                                      TRACK_MODE_TYPE TrackMode )
{
    
    PCDROM_READ pRequest;
    BYTE RequestBuffer[sizeof(CDROM_READ) + sizeof(SGX_BUF) * 2];
    DWORD dwTransferSize, dwExcess, dwReceived;
    BOOL fRet;
    LPBYTE EarlyBuffer, LateBuffer;
    //This warning suppressed because .dwSectorSize do not overflow/underflow
    #pragma warning(push)
    #pragma warning(disable:22011)
    DWORD dwSectorSize = CD_SECTOR_SIZE;

    if( fRaw )
    {
        //
        // Apparently we read the entire 2352 bytes when reading 
        // XAForm2/YellowMode2 even if the "user data" is a different
        // size.
        //
        switch( TrackMode )
        {
        case YellowMode2:
        case XAForm2:
        case CDDA:
            dwSectorSize = CDROM_RAW_SECTOR_SIZE;
            break;

        default:
            ASSERT(FALSE);
            return FALSE;
        }
    }

    if (!(EarlyBuffer = (LPBYTE)UDFSAlloc(m_hHeap, dwSectorSize))) {
        return FALSE;
    }
    if (!(LateBuffer = (LPBYTE)UDFSAlloc(m_hHeap, dwSectorSize))) {
        UDFSFree(m_hHeap, EarlyBuffer);
        return FALSE;
    }

    ASSERT((dwStartSectorOffset < dwSectorSize) && nBytesToRead);
    ASSERT((nBytesToRead / dwSectorSize) < 65533);

    pRequest = (PCDROM_READ) RequestBuffer;

    pRequest->bRawMode = fRaw;
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

    dwExcess = dwTransferSize % dwSectorSize;
    
    if (dwExcess) {
    
        pRequest->sglist[pRequest->sgcount].sb_buf = LateBuffer;
        
        if (dwTransferSize > dwSectorSize) {
        
            pRequest->sglist[pRequest->sgcount].sb_len = dwSectorSize - dwExcess;
            dwTransferSize += dwSectorSize - dwExcess;
        } else {
        
            pRequest->sglist[pRequest->sgcount].sb_len = dwSectorSize - dwTransferSize;
            dwTransferSize = dwSectorSize;
        }
        
        pRequest->sgcount++;
    }
    #pragma warning(pop)
    if( fRaw )
    {
        pRequest->TrackMode = TrackMode;
    }

    pRequest->StartAddr.Mode = CDROM_ADDR_LBA;
    pRequest->StartAddr.Address.lba = dwSector;

    pRequest->TransferLength = dwTransferSize / dwSectorSize;
    
    ASSERT((pRequest->TransferLength < 65536) && (dwTransferSize % dwSectorSize) == 0);

    if (dwExcess || dwStartSectorOffset) {
        lpOverlapped = NULL;
    }

    fRet = UDFSDeviceIoControl( IOCTL_CDROM_READ_SG, 
                                pRequest,
                                sizeof(CDROM_READ) + sizeof(SGX_BUF) * (pRequest->sgcount - 1),
                                NULL, 
                                0, 
                                &dwReceived, 
                                lpOverlapped );

    if (fRet == TRUE) {
        if (pNumBytesRead) {
            *pNumBytesRead = nBytesToRead;
        }
    }

    UDFSFree(m_hHeap, EarlyBuffer);
    UDFSFree(m_hHeap, LateBuffer);
    
    return fRet;
}

