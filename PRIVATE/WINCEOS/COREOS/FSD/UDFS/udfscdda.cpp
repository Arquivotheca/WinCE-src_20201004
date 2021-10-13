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
//  File:       udfscdda.cpp
//
//  Contents:
//
//  Classes:
//
//  Functions:
//
//--------------------------------------------------------------------------

#include "udfs.h"

//+-------------------------------------------------------------------------
//
//  Member:     CCDDAFileSystem::DetectCreateAndInit
//
//  Synopsis:   Determines if the volume is an iso 9660 volume.  If it is
//              create an iso 9660 class and returns it initialized
//
//  Arguments:  [pCache]                --
//              [pRootDirectoryPointer] --
//              [ppNewFS]               --
//
//  Returns:    BOOL
//
//  Notes:  TBD!!!
//
//--------------------------------------------------------------------------

BYTE CCDDAFileSystem::DetectCreateAndInit( PUDFSDRIVER pDrv, PDIRECTORY_ENTRY pRootDirectoryPointer, CFileSystem** ppNewFS)
{
    CDROM_TOC cdtoc;
    BOOL bRet = TRUE;
    DWORD cb;

    if (pDrv->UDFSDeviceIoControl(IOCTL_CDROM_READ_TOC, NULL, 0, &cdtoc, CDROM_TOC_SIZE, &cb, NULL)) {

#ifdef DEBUG    
        NKDbgPrintfW(L"Got the CDROM TOC Information Length=%ld\r\n", ((WORD)cdtoc.Length[0] << 8) | (WORD)cdtoc.Length[1]);
        NKDbgPrintfW(L"First track is %ld Last track is %ld\r\n", cdtoc.FirstTrack, cdtoc.LastTrack);
        CDROM_ADDR cdaddr, cdaddrnext,cdaddrmsf;
        MSF_ADDR msftemp;
        memset( &cdaddr, 0, sizeof(CDROM_ADDR));
        memset( &cdaddrnext, 0, sizeof(CDROM_ADDR));
        for (DWORD i=0; i < cdtoc.LastTrack; i++) {
            cdaddr.Mode = CDROM_ADDR_MSF;
            cdaddr.Address.msf.msf_Frame = cdtoc.TrackData[i].Address[3];
            cdaddr.Address.msf.msf_Second = cdtoc.TrackData[i].Address[2];
            cdaddr.Address.msf.msf_Minute = cdtoc.TrackData[i].Address[1];
            cdaddr.Mode = CDROM_ADDR_MSF;
            cdaddrnext.Address.msf.msf_Frame = cdtoc.TrackData[i+1].Address[3];
            cdaddrnext.Address.msf.msf_Second = cdtoc.TrackData[i+1].Address[2];
            cdaddrnext.Address.msf.msf_Minute = cdtoc.TrackData[i+1].Address[1];
            CDROM_MSF_TO_LBA(&cdaddr);
            CDROM_MSF_TO_LBA(&cdaddrnext);
            cdaddrmsf.Address.lba = cdaddrnext.Address.lba-cdaddr.Address.lba;
            CDROM_LBA_TO_MSF(&cdaddrmsf, msftemp);
            NKDbgPrintfW( L"Track[%2u] at \tMSF(%02u:%02u:%02u)\tMSFLength(%02u:%02u:%02u)\tLBA(%7d)\tLBA_Length(%7d)\r\n",
                        cdtoc.TrackData[i].TrackNumber, 
                        cdtoc.TrackData[i].Address[1],
                        cdtoc.TrackData[i].Address[2],
                        cdtoc.TrackData[i].Address[3],
                        cdaddrmsf.Address.msf.msf_Minute,
                        cdaddrmsf.Address.msf.msf_Second,
                        cdaddrmsf.Address.msf.msf_Frame,
                        cdaddr.Address.lba,
                        cdaddrnext.Address.lba-cdaddr.Address.lba);
        }
#endif

        // Filesystem initialization
        *ppNewFS = new CCDDAFileSystem(pDrv, cdtoc);

        if (*ppNewFS == NULL)
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return FALSE;
        }

        pRootDirectoryPointer->dwDiskSize = 0;
        pRootDirectoryPointer->dwDiskLocation = 0;
        pRootDirectoryPointer->cbSize = 1;

        pRootDirectoryPointer->pCacheLocation = NULL;
        pDrv->SaveRootName("CD Audio", TRUE);
    }else {
        bRet = FALSE;
    }    
    
    return bRet;
}


//+-------------------------------------------------------------------------
//
//  Member:     CCDDAFileSystem::ReadDirectory
//
//  Synopsis:   Parse the structures for this directory
//
//  Arguments:  [pszFullPath]           -- The full pathname, to be stored
//                                         in the header
//              [pParentDirectoryEntry] -- The parent directory entry
//
//  Returns:    BOOL
//
//  Notes:
//
//--------------------------------------------------------------------------

BOOL CCDDAFileSystem::ReadDirectory(LPWSTR pszFullPath, PDIRECTORY_ENTRY    pParentDirectoryEntry)
{
    if (m_cdtoc.LastTrack) {
        PDIRECTORY_ENTRY pEntry;
        CDROM_ADDR cdaddr, cdaddrnext;
        WORD wSize = sizeof(DIRECTORY_ENTRY)+32;
        ASSERT(m_pDriver);
        pEntry = pParentDirectoryEntry->pCacheLocation = (PDIRECTORY_ENTRY)UDFSAlloc(m_pDriver->m_hHeap, wSize * (m_cdtoc.LastTrack+2));

        if (pEntry == NULL) {
            CompactAllHeaps();
            pEntry = pParentDirectoryEntry->pCacheLocation = (PDIRECTORY_ENTRY)UDFSAlloc(m_pDriver->m_hHeap, wSize * (m_cdtoc.LastTrack+2));
        }

        ((PDIRECTORY_HEADER)pEntry)->dwLockCount = 1;
        pEntry->cbSize = wSize;
        pEntry = (PDIRECTORY_ENTRY) ((DWORD)pEntry + wSize);
        for (int i=1; i < m_cdtoc.LastTrack+2;i++) {
            if ( i == m_cdtoc.LastTrack+1) {
                pEntry->cbSize = 0;
            } else {
                cdaddr.Mode = CDROM_ADDR_MSF;
                cdaddr.Address.msf.msf_Frame = m_cdtoc.TrackData[i-1].Address[3];
                cdaddr.Address.msf.msf_Second = m_cdtoc.TrackData[i-1].Address[2];
                cdaddr.Address.msf.msf_Minute = m_cdtoc.TrackData[i-1].Address[1];
                cdaddr.Mode = CDROM_ADDR_MSF;
                cdaddrnext.Address.msf.msf_Frame = m_cdtoc.TrackData[i].Address[3];
                cdaddrnext.Address.msf.msf_Second = m_cdtoc.TrackData[i].Address[2];
                cdaddrnext.Address.msf.msf_Minute = m_cdtoc.TrackData[i].Address[1];
                CDROM_MSF_TO_LBA(&cdaddr);
                CDROM_MSF_TO_LBA(&cdaddrnext);
                pEntry->dwDiskSize = cdaddrnext.Address.lba-cdaddr.Address.lba;
                wsprintf( pEntry->szNameW, L"Track%02ld.cda", i);
                pEntry->cbSize = wSize;
            }    
            pEntry = (PDIRECTORY_ENTRY) ((DWORD)pEntry + wSize);
        }
        return TRUE;
    } else {
        return FALSE;
    }    
    return FALSE;
}


