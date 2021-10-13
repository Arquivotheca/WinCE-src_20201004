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
//  File:       udfscdda.cpp
//
//  Contents:
//
//  Classes:
//
//  Functions:
//
//--------------------------------------------------------------------------

#include "cdfs.h"

//+-------------------------------------------------------------------------
//
//  Member:     CCDDAFileSystem::DetectCreateAndInit
//
//  Synopsis:   Check if the session is an audio session.
//
//  Arguments:  [pCache]                --
//              [pRootDirectoryPointer] --
//              [ppNewFS]               --
//
//  Returns:    BOOL
//
//  Notes:  
//
//--------------------------------------------------------------------------

BOOL CCDDAFileSystem::DetectCreateAndInit( PUDFSDRIVER pDrv, 
                                           PDIRECTORY_ENTRY pRootDirectoryPointer, 
                                           CFileSystem** ppNewFS,
                                           PCD_PARTITION_INFO pPartInfo )
{
    BOOL fResult = TRUE;
    
    if( !pPartInfo->fAudio )
    {
        fResult = FALSE;
        goto exit_detectcreateandinit;
    }

    //
    // Filesystem initialization
    //
    *ppNewFS = new CCDDAFileSystem(pDrv, pPartInfo);

    if (*ppNewFS == NULL)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        fResult = FALSE;
        goto exit_detectcreateandinit;
    }

    pRootDirectoryPointer->dwDiskSize = 0;
    pRootDirectoryPointer->dwDiskLocation = 0;
    pRootDirectoryPointer->cbSize = 1;

    pRootDirectoryPointer->pCacheLocation = NULL;

    //
    // Is this function right?
    //
    pDrv->SaveRootName( (PCHAR)L"CD Audio", TRUE );

exit_detectcreateandinit:
    return fResult;
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

BOOL CCDDAFileSystem::ReadDirectory( const WCHAR* pszFullPath, 
                                     PDIRECTORY_ENTRY pParentDirectoryEntry )
{
    PDIRECTORY_ENTRY pEntry = NULL;
    const WORD wSize = sizeof(DIRECTORY_ENTRY)+32;
    
    ASSERT(m_pDriver);

    //
    // We are allocating enough room for each of the tracks as a directory entry,
    // a blank directory entry to mark the end, and a directory header at the 
    // beginning.
    //
    pEntry = pParentDirectoryEntry->pCacheLocation 
           = (PDIRECTORY_ENTRY)UDFSAlloc( m_pDriver->m_hHeap, 
                                          wSize * (m_pPartInfo->NumberOfTracks + 2) );

    if (pEntry == NULL) {
        CompactAllHeaps();
        pEntry = pParentDirectoryEntry->pCacheLocation 
               = (PDIRECTORY_ENTRY)UDFSAlloc( m_pDriver->m_hHeap, 
                                              wSize * (m_pPartInfo->NumberOfTracks + 2) );
    }

    if( pEntry == NULL )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        goto exit_readdirectory;
    }

    ((PDIRECTORY_HEADER)pEntry)->dwLockCount = 1;
    pEntry->cbSize = wSize;
    pEntry = (PDIRECTORY_ENTRY) ((DWORD)pEntry + wSize);
    
    for( int i=1; i <= m_pPartInfo->NumberOfTracks + 1; i++ )
    {
        if ( i == m_pPartInfo->NumberOfTracks + 1 ) 
        {
            pEntry->cbSize = 0;
        } 
        else 
        {
            //
            // I'm hoping that we can just take the track size and location and 
            // it will work fine.  However, the desktop does this in a different
            // way and I'm not entirely convinced this will work.  We may have 
            // to revisit this if we see problems.
            //
            SwapCopyUchar4(&pEntry->dwDiskLocation,
                           m_pPartInfo->TrackInformation[i-1].TrackStartAddress);
            
            SwapCopyUchar4(&pEntry->dwDiskSize,
                           m_pPartInfo->TrackInformation[i-1].TrackSize);

            pEntry->dwDiskSize = pEntry->dwDiskSize * 2352 + sizeof(RIFF_HEADER);

            pEntry->MediaFlags |= MF_PREPEND_HEADER;
            pEntry->MediaFlags |= MF_CHECK_TRACK_MODE;
            pEntry->TrackMode = CDDA;
            pEntry->HeaderSize = sizeof(RIFF_HEADER);

            _snwprintf( pEntry->szNameW, 12, L"Track%02ld.wav", i );
            pEntry->cbSize = wSize;
        }
        
        pEntry = (PDIRECTORY_ENTRY) ((DWORD)pEntry + wSize);
    }
    return TRUE;

exit_readdirectory:
    
    return FALSE;
}

//+-------------------------------------------------------------------------
//
//  Member:     CCDDAFileSystem::GetHeader
//
//  Synopsis:   Returns the RIFF header that should prepend the file
//
//  Arguments:  [pHeader]   -- The header to fill in
//              [pFileInfo] -- The file information from disc
//
//  Returns:    BOOL
//
//  Notes:
//
//--------------------------------------------------------------------------

BOOL CCDDAFileSystem::GetHeader( PRIFF_HEADER pHeader, PFILE_HANDLE_INFO pFileInfo )
{
    CopyMemory( pHeader, &CdXAAudioPhileHeader, sizeof(RIFF_HEADER) );

    pHeader->ChunkSize += pFileInfo->dwDiskSize;
    pHeader->RawSectors += pFileInfo->dwDiskSize;

    return TRUE;
}

