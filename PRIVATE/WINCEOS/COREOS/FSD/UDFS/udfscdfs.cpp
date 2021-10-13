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
//  File:       udfscdfs.cpp
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

BYTE CCDFSFileSystem::DetectCreateAndInit( PUDFSDRIVER pDrv, PDIRECTORY_ENTRY pRootDirectoryPointer, CFileSystem** ppNewFS)
{
    BOOL bResult;
    // Try different locations to see if we can recognize a iso 9660 volume

    bResult = DetectCreateAndInitAtLocation(45016, pDrv, pRootDirectoryPointer, ppNewFS);
    if (bResult) {
        return bResult;
    }

    bResult = DetectCreateAndInitAtLocation(45000, pDrv, pRootDirectoryPointer, ppNewFS);
    if (bResult) {
        return bResult;
    }

    bResult = DetectCreateAndInitAtLocation(16, pDrv, pRootDirectoryPointer, ppNewFS);
    return bResult;
}

BOOL CCDFSFileSystem::DetectCreateAndInitAtLocation( DWORD dwInitialSector,PUDFSDRIVER pDrv, PDIRECTORY_ENTRY pRootDirectoryPointer, CFileSystem** ppNewFS)
{
    BOOL    fRet;
    BOOL    fFound;
    DWORD   dwSector;
    BOOL   fUnicodeFileSystem;
    BYTE    buffer[CD_SECTOR_SIZE];

    PISO9660_VOLUME_DESCRIPTOR pVD = (PISO9660_VOLUME_DESCRIPTOR)&buffer;

    memset( pVD, 0, sizeof(1)); // Prefix

    //
    //  Read the volume descriptors

    dwSector = dwInitialSector;
    fFound = FALSE;

    while (dwSector < dwInitialSector + MAX_SECTORS_TO_PROBE) {
       fRet = pDrv->ReadDisk( 
                    dwSector,
                    (PBYTE)pVD,
                    CD_SECTOR_SIZE);

        if (fRet == FALSE) {
            break;
        }

        if (memcmp(&(pVD->ID), "CD001", 5) != 0) {
            break;
        }

		if (pVD->Type == 2) {
				//
				//  Type == 2 then this is a supplemental volume
				//  descriptor.  If we also find the escape sequence
				//  we looking for then this is a joliet file system
				//  
			if( (pVD->EscapeSequence[0] == '%') && (pVD->EscapeSequence[1] == '/') && ((pVD->EscapeSequence[2] == '@') ||
                                                                                                     (pVD->EscapeSequence[2] == 'C') ||
                                                                                                     (pVD->EscapeSequence[2] == 'E'))) {
				fUnicodeFileSystem = TRUE;
			} else {
				//
				//  as we have not found the escape sequence, this must
				//  be a file system we don't understand, so lets ignore it
				dwSector++;		
				continue;
			}
			
		} else 
        if (pVD->Type == 1) {
            //
            //  Type == 1 then this is a plain old primary pVD
            //  We need to keep looking for an svd.
            //

            fUnicodeFileSystem = FALSE;
        }else if(pVD->Type == 0) {
            //
            //  Type == 0 then this is an el torito bootable CD
            //  We need to ignore it and move onto the next sector
            dwSector++;		
            continue;
		} else if (pVD->Type == 0xFF) {
            //
            //  Type == ff signals the end.
            //

            break;
        }

        pRootDirectoryPointer->dwDiskSize = pVD->RootDirectory.ExtentSize;
        pRootDirectoryPointer->dwDiskLocation = pVD->RootDirectory.ExtentLocation;
        pRootDirectoryPointer->cbSize = sizeof(DIRECTORY_ENTRY);
        pRootDirectoryPointer->pCacheLocation = NULL;

        fFound = TRUE;
		
        dwSector++;
		//
		// Save Volume Id
		//
		pDrv->SaveRootName((PCHAR)&pVD->Fill1[0x22],fUnicodeFileSystem);	// TODO:

		
    }

    if (fFound == FALSE) {
        SetLastError(ERROR_UNRECOGNIZED_MEDIA);
    } else {
        (*ppNewFS) = new CCDFSFileSystem(pDrv, fUnicodeFileSystem);

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

BOOL CCDFSFileSystem::ReadDirectory( LPWSTR pszFullPath, PDIRECTORY_ENTRY pParentDirectoryEntry)
{
    PISO9660_DIRECTORY_RECORD pCurrentDiskDirectory;

    PDIRECTORY_ENTRY    pCurrentCacheEntry;

    PDIRECTORY_HEADER   pCacheHeader;

    SYSTEMTIME          SystemTime;

    PBYTE           pDiskBuffer;
    PBYTE           pCacheBuffer;
    BOOL            fRet;
    int             count;
    DWORD           dwBytesRead = 0;
	DWORD 			dwFullPathSize= sizeof(DIRECTORY_HEADER) + (lstrlen(pszFullPath)+1) * sizeof(TCHAR) ;

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

	pCacheBuffer = (PBYTE)UDFSAlloc(m_pDriver->m_hHeap, max(pParentDirectoryEntry->dwDiskSize * 5,dwFullPathSize));
	
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
    lstrcpy(pCacheHeader->szFullPath, pszFullPath);

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
                if ((pCurrentDiskDirectory->FileName[count] == 0) &&
                    (pCurrentDiskDirectory->FileName[count+1] == ';')) {
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


