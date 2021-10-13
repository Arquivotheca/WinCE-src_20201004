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
//  File:       cdfscdfs.cpp
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
//  Member:     CCDFSFileSystem::Recognize
//
//  Synopsis:   Determines if the volume is an iso 9660 volume.
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

BOOL CCDFSFileSystem::Recognize( HDSK hDisk,
                                 PDIRECTORY_ENTRY pRootDirectoryPointer, 
                                 BOOL* fUnicode,
                                 BOOL* fCDXA,
                                 __out_ecount_opt(LabelSize) BYTE* VolumeLabel,
                                 DWORD LabelSize )
{
    BOOL fFound = FALSE;
    DWORD dwSector = 0;
    BYTE buffer[CD_SECTOR_SIZE] = { 0 };
    PISO9660_VOLUME_DESCRIPTOR pVD = (PISO9660_VOLUME_DESCRIPTOR)buffer;
    DWORD dwBufferSize = 0;
    CD_PARTITION_INFO* pPartInfo = NULL;
    DWORD dwBytesReturned = 0;
    USHORT TracksToTry = 0;

    dwBufferSize = sizeof(CD_PARTITION_INFO) +
                   MAXIMUM_NUMBER_TRACKS_LARGE * sizeof(TRACK_INFORMATION2);
    
    pPartInfo = (PCD_PARTITION_INFO)new BYTE[ dwBufferSize ];
    if( !pPartInfo )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return FALSE;
    }

    //
    // Find out where our session begins, track information, etc.
    //
    if( !::FSDMGR_DiskIoControl( (DWORD)hDisk,
                                 IOCTL_CDROM_GET_PARTITION_INFO,
                                 NULL,
                                 0,
                                 pPartInfo,
                                 dwBufferSize,
                                 &dwBytesReturned,
                                 NULL ) )
    {
        fFound = FALSE;
        goto exit_recognize;
    }

    //
    //  Read the volume descriptors.
    //
    fFound = FALSE;

    dwSector = 16 + pPartInfo->dwStartingLBA;
    while (dwSector < 16 + pPartInfo->dwStartingLBA + MAX_SECTORS_TO_PROBE) 
    {
        if( !CReadOnlyFileSystemDriver::ReadDisk( hDisk,
                                                  dwSector, 
                                                  (PBYTE)pVD, 
                                                  CD_SECTOR_SIZE ) )
        {
            break;
        }

        if( memcmp( &(pVD->ID), "CD001", 5 ) != 0 )
        {
            //
            // This is not a valid VRS descriptor.
            //
            break;
        }

        if( (pVD->Type == 2) && 
            ((pVD->EscapeSequence[0] == '%') && 
             (pVD->EscapeSequence[1] == '/') && 
             ((pVD->EscapeSequence[2] == '@') || 
              (pVD->EscapeSequence[2] == 'C') || 
              (pVD->EscapeSequence[2] == 'E')))) 
        {
            //
            //  Type == 2 then this is a supplemental volume
            //  descriptor.  If we also find the escape sequence
            //  we looking for then this is a joliet file system
            //
            *fUnicode = TRUE;
        }
        else if( pVD->Type == 1 )
        {
            //
            //  Type == 1 then this is a plain old primary pVD
            //  We need to keep looking for an svd.
            //
            *fUnicode = FALSE;

            //
            // Each PVD must have this signature in order to identify it as
            // a CD-XA disc.
            //
            if( memcmp( &buffer[1024], "CD-XA001", 8 ) == 0 )
            {
                *fCDXA = TRUE;
            }
            else
            {
                *fCDXA = FALSE;
            }
        }
        else if( pVD->Type == 0xFF )
        {
            //
            //  Type == ff signals the end.
            //
            break;
        }
        else
        {
            dwSector++;
            continue;
        }

        pRootDirectoryPointer->dwDiskSize = pVD->RootDirectory.ExtentSize;
        pRootDirectoryPointer->dwDiskLocation = pVD->RootDirectory.ExtentLocation;
        pRootDirectoryPointer->cbSize = sizeof(DIRECTORY_ENTRY);
        pRootDirectoryPointer->pCacheLocation = NULL;

        fFound = TRUE;
        
        if( VolumeLabel && LabelSize )
        {
            CopyMemory( VolumeLabel, &pVD->Fill1[34], min(LabelSize, 32) );
        }

        dwSector++;
    }

exit_recognize:
    if( pPartInfo )
    {
        delete [] pPartInfo;
    }

    return fFound;
}

//+-------------------------------------------------------------------------
//
//  Member:     CCDFSFileSystem::DetectCreateAndInit
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
//  Notes:
//
//--------------------------------------------------------------------------

BOOL CCDFSFileSystem::DetectCreateAndInit( PUDFSDRIVER pDrv, 
                                           PDIRECTORY_ENTRY pRootDirectoryPointer, 
                                           CFileSystem** ppNewFS,
                                           PCD_PARTITION_INFO pPartInfo )
{
    BOOL fUnicode = FALSE;
    BOOL fCDXA = FALSE;
    BYTE VolumeLabel[32];
    BOOL fFound = Recognize( pDrv->GetDisk(),
                             pRootDirectoryPointer, 
                             &fUnicode,
                             &fCDXA,
                             VolumeLabel, 
                             sizeof(VolumeLabel) );
    
    if (fFound == FALSE) {
        SetLastError(ERROR_UNRECOGNIZED_MEDIA);
    } else {
        //
        // Save Volume Id
        //
        pDrv->SaveRootName( (PCHAR)VolumeLabel, fUnicode );
        
        (*ppNewFS) = new CCDFSFileSystem(pDrv, fUnicode, fCDXA, pPartInfo );

        if ((*ppNewFS) == NULL) {
        
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return FALSE;
        }
    }

    return fFound;
}

//+-------------------------------------------------------------------------
//
//  Member:     CCDFSFileSystem::ReadDirectory
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

BOOL CCDFSFileSystem::ReadDirectory( const WCHAR* pszFullPath, PDIRECTORY_ENTRY pParentDirectoryEntry)
{
    PISO9660_DIRECTORY_RECORD pCurrentDiskDirectory;

    PDIRECTORY_ENTRY    pCurrentCacheEntry = NULL;

    PDIRECTORY_HEADER   pCacheHeader = NULL;

    SYSTEMTIME          SystemTime = { 0 };

    PBYTE           pDiskBuffer = NULL;
    PBYTE           pCacheBuffer = NULL;
    DWORD           dwBufferSize = 0;
    BOOL            fRet = TRUE;
    int             count = 0;
    DWORD           dwBytesRead = 0;
    DWORD           dwFullPathSize= sizeof(DIRECTORY_HEADER) + (lstrlen(pszFullPath)+1) * sizeof(TCHAR) ;

    //
    //  We only get to this point if this node has not been cached.
    //

    //
    //  Allocate enough memory to read the entire directory
    //

    ASSERT(m_pDriver);
    pDiskBuffer = (PBYTE)UDFSAlloc(m_pDriver->m_hHeap, pParentDirectoryEntry->dwDiskSize);

    if (pDiskBuffer == NULL) {
        //
        // shrink our cache
        //
        CompactAllHeaps();
        pDiskBuffer = (PBYTE)UDFSAlloc(m_pDriver->m_hHeap, pParentDirectoryEntry->dwDiskSize);

        if (pDiskBuffer == NULL) {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return FALSE;
        }
    }

    //
    //  Allocate more than enough memory
    //

    dwBufferSize = max(pParentDirectoryEntry->dwDiskSize * 5, dwFullPathSize);
    pCacheBuffer = (PBYTE)UDFSAlloc(m_pDriver->m_hHeap, dwBufferSize);
    
    if (pCacheBuffer == NULL) {
        CompactAllHeaps();
        pCacheBuffer = (PBYTE)UDFSAlloc(m_pDriver->m_hHeap, max(pParentDirectoryEntry->dwDiskSize * 5,dwFullPathSize));
        
        if (pCacheBuffer == NULL) {
            UDFSFree(m_pDriver->m_hHeap, pDiskBuffer);
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return FALSE;
        }
    }

    //
    //  Create the header
    //

    pCacheHeader = (PDIRECTORY_HEADER)pCacheBuffer;
    pCacheHeader->cbSize = ALIGN(sizeof(DIRECTORY_HEADER) + (lstrlen(pszFullPath) * sizeof(WCHAR)));
    pCacheHeader->dwLockCount = 1;
    pCacheHeader->pParent = pParentDirectoryEntry;
    memcpy(&(pCacheHeader->Signature), DIR_HEADER_SIGNATURE, sizeof(pCacheHeader->Signature));

    //
    // Make dwBufferSize the size in wchars allotted to the full path to 
    // use for the copy operation.  (Compiler should optimize the /2.)
    //

    if(dwBufferSize <(sizeof(DIRECTORY_HEADER) + sizeof(WCHAR)))
    {
       goto Error;
    }
    dwBufferSize -= sizeof(DIRECTORY_HEADER) - sizeof(WCHAR);
    dwBufferSize = dwBufferSize / sizeof(WCHAR);
    StringCchCopy( pCacheHeader->szFullPath, 
                   dwBufferSize,
                   pszFullPath );

    pCurrentCacheEntry = (PDIRECTORY_ENTRY)(pCacheBuffer + pCacheHeader->cbSize);

    fRet = m_pDriver->Read( pParentDirectoryEntry->dwDiskLocation, 0, pParentDirectoryEntry->dwDiskSize, pDiskBuffer, &dwBytesRead, NULL);

    if ((fRet == FALSE) || (dwBytesRead != pParentDirectoryEntry->dwDiskSize)) {
        goto Error;
    }

    //
    //  Skip the first two entries (current dir and parent dir)
    //

    pCurrentDiskDirectory = (PISO9660_DIRECTORY_RECORD)(pDiskBuffer + pDiskBuffer[0] + pDiskBuffer[pDiskBuffer[0]]);

    //
    //  Parse the structures
    //

    while (TRUE) {
        //
        //  Check to see if we're at the end of the directory
        //

        if ((pCurrentDiskDirectory->RecordLength == 0) ||
            ((((DWORD)pCurrentDiskDirectory) + pCurrentDiskDirectory->RecordLength) >
             (((DWORD)pDiskBuffer) + pParentDirectoryEntry->dwDiskSize)))
        {
            pCurrentCacheEntry->cbSize = 0;
            break;
        }

        //
        //  Copy information
        //

        pCurrentCacheEntry->dwDiskLocation = pCurrentDiskDirectory->ExtentLocation;
        pCurrentCacheEntry->dwDiskSize = pCurrentDiskDirectory->ExtentSize;

        if (pCurrentDiskDirectory->Flags & 2) {
            pCurrentCacheEntry->fFlags = FILE_ATTRIBUTE_DIRECTORY;
        } else {
            pCurrentCacheEntry->fFlags = 0;
        }

        if (pCurrentDiskDirectory->Flags & 1) {
            pCurrentCacheEntry->fFlags |= FILE_ATTRIBUTE_HIDDEN;
        }

        pCurrentCacheEntry->pCacheLocation = NULL;
        pCurrentCacheEntry->pHeader = pCacheHeader;
        pCurrentCacheEntry->cbSize = ALIGN(sizeof(DIRECTORY_ENTRY) + (pCurrentDiskDirectory->StrLen * sizeof(WCHAR)));

        //
        //  Copy file time
        //

        SystemTime.wYear = pCurrentDiskDirectory->Time[0] + 1900;
        SystemTime.wMonth = pCurrentDiskDirectory->Time[1];
        SystemTime.wDay = pCurrentDiskDirectory->Time[2];
        SystemTime.wHour = pCurrentDiskDirectory->Time[3];
        SystemTime.wMinute = pCurrentDiskDirectory->Time[4];
        SystemTime.wSecond = pCurrentDiskDirectory->Time[5];
        SystemTime.wMilliseconds = 0; // ISO9660 doesn't expose milliseconds

        SystemTimeToFileTime(&SystemTime, &pCurrentCacheEntry->Time);

        //
        //  Copy file name
        //

        if (m_fUnicodeFileSystem == TRUE) {
            for (count = 0; count < pCurrentDiskDirectory->StrLen; count+=2)
            {
                if (
                    (pCurrentDiskDirectory->FileName[count] == 0) &&
                    (pCurrentDiskDirectory->FileName[count+1] == ';')
                ) {
                    break;
                }

                //
                //  convert from big to little endian (or the other way around)
                //

                pCurrentCacheEntry->szNameA[count] = pCurrentDiskDirectory->FileName[count+1];
                pCurrentCacheEntry->szNameA[count+1] = pCurrentDiskDirectory->FileName[count];
            }

            DEBUGMSG( ZONE_FUNCTION, (TEXT("UDFS!CFileSystem::ReadDirectory Filename: %s\n"), pCurrentCacheEntry->szNameW));

            while( count > 0 && *((WCHAR*)&pCurrentCacheEntry->szNameA[count-2]) == L'.' ) {
                count -= 2;
            }    
            pCurrentCacheEntry->szNameA[count] = 0;
            pCurrentCacheEntry->szNameA[count + 1] = 0;
        } else {
            pCurrentCacheEntry->fFlags |= IS_ANSI_FILENAME;

            for (count = 0; count < pCurrentDiskDirectory->StrLen; count++) {
                if (pCurrentDiskDirectory->FileName[count] == ';') {
                    break;
                }
                pCurrentCacheEntry->szNameA[count] = pCurrentDiskDirectory->FileName[count];
            }
            while( count > 0 && pCurrentCacheEntry->szNameA[count-1] == '.' ) {
                --count;
            }    
            pCurrentCacheEntry->szNameA[count] = 0;
        }

        //
        // If the volume is marked as XA and the directory entry extends beyond 
        // the name, check for Mode information for the file.
        //
        if( m_fCDXA && 
            pCurrentDiskDirectory->RecordLength >=
                 sizeof(PISO9660_DIRECTORY_RECORD) +
                 pCurrentDiskDirectory->StrLen + 
                 sizeof(XA_SYSTEM_USE) -
                 ((pCurrentDiskDirectory->StrLen & 1) ? 1 : 0) )
        {
            PXA_SYSTEM_USE pXAInfo = (PXA_SYSTEM_USE)&pCurrentDiskDirectory->FileName[pCurrentDiskDirectory->StrLen + 
                                                                                    ((pCurrentDiskDirectory->StrLen & 1) ? 0 : 1)];

            if( pXAInfo->Signature == SYSTEM_XA_SIGNATURE )
            {
                if (FlagOn( pXAInfo->Attributes, SYSTEM_USE_XA_DA ) ||
                    FlagOn( pXAInfo->Attributes, SYSTEM_USE_XA_FORM2 )) 
                {

                    pCurrentCacheEntry->MediaFlags |= MF_PREPEND_HEADER;
                    pCurrentCacheEntry->MediaFlags |= MF_CHECK_TRACK_MODE;
                    pCurrentCacheEntry->HeaderSize = sizeof(RIFF_HEADER);

                    if( FlagOn( pXAInfo->Attributes, SYSTEM_USE_XA_DA ) )
                    {
                        pCurrentCacheEntry->TrackMode = CDDA;
                    }
                    else
                    {
                        pCurrentCacheEntry->TrackMode = XAForm2;
                    }
                    
                    //
                    // TODO::Check for 64-bit support throughout CDFS.  
                    // Currently I don't think it exists at all and we probably will
                    // never need worry about it... but just in case.
                    //
                    // Note::I'm not sure why we use 2352 for Mode 2 Form2, but the 
                    // desktop does and the approach seems to work.
                    //
                    pCurrentCacheEntry->dwDiskSize = 
                            (pCurrentCacheEntry->dwDiskSize >> CD_SHIFT_COUNT) * 2352;
                    

                    pCurrentCacheEntry->dwDiskSize += sizeof(RIFF_HEADER);
                }
            }
        }

        DEBUGMSG(ZONE_FUNCTION, (TEXT("UDFS!CFileSystem::ReadDirectory Moving disk  %d\n"), pCurrentDiskDirectory->RecordLength));
        DEBUGMSG(ZONE_FUNCTION, (TEXT("UDFS!CFileSystem::ReadDirectory Moving cache %d\n"), pCurrentCacheEntry->cbSize));

        //
        //  Update the cache end
        //

        pCurrentCacheEntry = NextDirectoryEntry(pCurrentCacheEntry);
        pCurrentDiskDirectory = (PISO9660_DIRECTORY_RECORD) ((PBYTE)pCurrentDiskDirectory + pCurrentDiskDirectory->RecordLength);

        if (pCurrentDiskDirectory->RecordLength == 0) {
            PISO9660_DIRECTORY_RECORD pProposedRecord;

            //
            //  If we hit a null length record we may be on a sector
            //  boundary.  Move the pointer up to the next sector only
            //  if this doesn't put us beyond the end of the disk
            //  buffer.
            //
            DWORD dwOffset = (((PBYTE)pCurrentDiskDirectory - pDiskBuffer + (CD_SECTOR_SIZE-1)) & ~(CD_SECTOR_SIZE-1));

            pProposedRecord = (PISO9660_DIRECTORY_RECORD) ((PBYTE)pDiskBuffer + dwOffset);
            
            DEBUGMSG(ZONE_FUNCTION, (TEXT("UDFS!CFileSystem::ReadDirectory Moving disk manually due to null length record: %d\r\n"), (PBYTE) pProposedRecord - (PBYTE) pCurrentDiskDirectory));
            
            if ((DWORD) pProposedRecord < ((DWORD) pDiskBuffer + pParentDirectoryEntry->dwDiskSize - 1)) {
                pCurrentDiskDirectory = pProposedRecord;
            }
        }
    }

Error:

    UDFSFree(m_pDriver->m_hHeap, pDiskBuffer);

    if (fRet == FALSE) {
        UDFSFree(m_pDriver->m_hHeap, pCacheBuffer);
    } else {
        pParentDirectoryEntry->pCacheLocation =
            (PDIRECTORY_ENTRY)UDFSReAlloc( m_pDriver->m_hHeap, pCacheBuffer, ((DWORD)pCurrentCacheEntry - (DWORD)pCacheBuffer) + sizeof(pCurrentCacheEntry->cbSize));
        if (!pParentDirectoryEntry->pCacheLocation) {
            UDFSFree( m_pDriver->m_hHeap, pCacheBuffer);
            fRet = FALSE;
        }
    }

    return fRet;

}

//+-------------------------------------------------------------------------
//
//  Member:     CCDFSFileSystem::GetHeader
//
//  Synopsis:   Returns a RIFF header for MPG/WAV files on video CDs/XA
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

BOOL CCDFSFileSystem::GetHeader( PRIFF_HEADER pHeader, PFILE_HANDLE_INFO pFileInfo )
{
    ASSERT( FlagOn(pFileInfo->MediaFlags, MF_PREPEND_HEADER) != 0 );

    if( pFileInfo->TrackMode == CDDA )
    {
        CopyMemory( pHeader, &CdXAAudioPhileHeader, sizeof(RIFF_HEADER) );

        pHeader->ChunkSize += pFileInfo->dwDiskSize;
        pHeader->RawSectors += pFileInfo->dwDiskSize;
    }
    else
    {
        CopyMemory( pHeader, &CdXAFileHeader, sizeof(RIFF_HEADER) );

        pHeader->ChunkSize += pFileInfo->dwDiskSize;
        pHeader->RawSectors += pFileInfo->dwDiskSize;

        pHeader->Attributes = pFileInfo->XAAttributes;
        pHeader->FileNumber = pFileInfo->XAFileNumber;
    }

    return TRUE;
}

