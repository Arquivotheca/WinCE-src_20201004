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
//  File:       
//          udfsudfs.cpp
//
//  Contents:   
//          UDFS specific methods
//
//  Classes:    
//          CUDFSFileSystem
//
//  Functions:
//
//--------------------------------------------------------------------------

#include "udfs.h"

//+-------------------------------------------------------------------------
//
//  Member:     CUDFSFileSystem::DetectCreateAndInit
//
//  Synopsis:   Detects a udf volume
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

BYTE CUDFSFileSystem::DetectCreateAndInit( PUDFSDRIVER	pDrv, PDIRECTORY_ENTRY pRootDirectoryPointer, CFileSystem** ppNewFS)
{
    NSR_ANCHOR      Anchor;
    VSD_GENERIC     GenericVSD;
    BOOL            fRet;
    DWORD           dwSector;
    DWORD           dwPartitionStart = (DWORD)-1;
    DWORD           dwVolumeStart = (DWORD)-1;
    DWORD           dwDiskSize;
    DWORD           dwBytesRead;
    DWORD           dwSectorOffset;
    PNSR_FSD        pFSD;				
	BOOL			fUnicodeFileSystem = FALSE;	// TODO:


    CUDFSFileSystem* pUDFS;

    //
    // TODO: Add more checks to this code later
    //
    memset( &Anchor, 0, sizeof(1)); // Prefix
    memset( &GenericVSD, 0, sizeof(1)); // Prefix

    fRet = pDrv->Read( ANCHOR_SECTOR, 0, sizeof(NSR_ANCHOR), (PBYTE)&Anchor, &dwBytesRead, NULL);

    if ((fRet == FALSE) || (Anchor.Destag.Ident != DESTAG_ID_NSR_ANCHOR)) {
        return FALSE;
    }

    dwSector = Anchor.Main.Lsn;

    //
    //  Track down the important structures:
    //      logical volume identifier
    //      partition descriptor
    //

    dwSectorOffset = 0;

    do
    {
        fRet = pDrv->Read( dwSector + dwSectorOffset, 0, sizeof(VSD_GENERIC), (PBYTE)&GenericVSD, &dwBytesRead, NULL);

        if (fRet == FALSE) {
            return FALSE;
        }
	
        if (GenericVSD.Type == DESTAG_ID_NSR_PART) {
            dwPartitionStart = ((PNSR_PART)&GenericVSD)->Start;
        }

        if (GenericVSD.Type == DESTAG_ID_NSR_LVOL) {
            dwVolumeStart = ((PNSR_LVOL)&GenericVSD)->FSD.Start.Lbn;
        }

        //
        //  If we've found both items we where looking for then
        //  terminate the loop
        //

        if ((dwVolumeStart != (DWORD)-1) && (dwPartitionStart != (DWORD)-1)) {
            break;
        }

        dwSectorOffset ++;

   } while ((GenericVSD.Type != DESTAG_ID_NSR_TERM) || (dwSectorOffset < 20));

    //
    //  If we didn't find the logical volume descriptor then bail.
    //

    if ((dwVolumeStart == (DWORD)-1) || (dwPartitionStart == (DWORD)-1)) {
        return FALSE;
    }

    //
    //  Find the pointer to the fsd in the logical volume desc.
    //

    dwSector = dwVolumeStart + dwPartitionStart;

    fRet = pDrv->Read( dwSector, 0, sizeof(VSD_GENERIC), (PBYTE)&GenericVSD, &dwBytesRead, NULL);

    if (fRet == FALSE) {
        return FALSE;
    }

	pFSD = (PNSR_FSD)&GenericVSD ;
    pUDFS = new CUDFSFileSystem(pDrv);

    if (pUDFS == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    //
    //  Make the partition start in the class.  All sector values have to
    //  be offset by this value.
    //

    pUDFS->m_dwPartitionStart = dwPartitionStart;

    fRet = pUDFS->ResolveFilePosition( pFSD->IcbRoot.Start.Lbn + dwPartitionStart, pRootDirectoryPointer, &dwDiskSize);

    if (fRet == FALSE) {
        delete pUDFS;
        return FALSE;
    }

	//	Save Volume Id used for Volume Registration
	//	pFSD->VolID[1] is probably wrong ?????
	//	Check structure definition in include fike!!!!!!
	//
	//	Also fUnicodeFileSystem should be properly set according
	//	DiskInfo!!!!

	
	pDrv->SaveRootName((PCHAR)&pFSD->VolID[1],fUnicodeFileSystem);

    //
    //  The location of the root directory is specified in the IcbRoot
    //  member of the FSD
    //

    pRootDirectoryPointer->cbSize = sizeof(DIRECTORY_ENTRY);
    pRootDirectoryPointer->pCacheLocation = NULL;

    (*ppNewFS) = pUDFS;

    return fRet;
}


//+-------------------------------------------------------------------------
//
//  Member:     CUDFSFileSystem::ReadDirectory
//
//  Synopsis:   Reads in a udf directory structure and parses the
//              information into a the standard directory structures
//
//  Arguments:  [pszFullPath]           --
//              [pParentDirectoryEntry] --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

BOOL CUDFSFileSystem::ReadDirectory( LPWSTR pszFullPath, PDIRECTORY_ENTRY pParentDirectoryEntry)
{
    PNSR_FID            pCurrentDiskDirectory;

    PDIRECTORY_ENTRY    pCurrentCacheEntry;

    PDIRECTORY_HEADER   pCacheHeader;

    PBYTE               pDiskBuffer;
    PBYTE               pDirectoryStart;
    PBYTE               pCacheBuffer;
    BOOL                fRet;
    int                 count;
    int                 byte;
    PUCHAR              pStr;

    DWORD               dwDiskSize;
    DWORD               dwSector;
    DWORD               dwFileSize;
    DWORD               dwBytesRead;

    //
    //  We only get to this point if this node has not been cached.
    //

    DEBUGCHK(pParentDirectoryEntry->pCacheLocation == NULL);

    dwDiskSize = pParentDirectoryEntry->dwDiskSize;
    dwSector = pParentDirectoryEntry->dwDiskLocation;

    //
    //  Allocate enough memory to read the entire directory
    //

    if (dwDiskSize < CD_SECTOR_SIZE) {
        dwDiskSize = CD_SECTOR_SIZE;
    }

    ASSERT(m_pDriver);
    pDiskBuffer = (PBYTE)UDFSAlloc(m_pDriver->m_hHeap, dwDiskSize);

    if (pDiskBuffer == NULL) {
        //
        // shrink our cache 
        //

        CompactAllHeaps();
        pDiskBuffer = (PBYTE)UDFSAlloc(m_pDriver->m_hHeap, dwDiskSize);

        if (pDiskBuffer == NULL) {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return FALSE;
        }
    }

    //
    //  Allocate more than enough memory
    //

	pCacheBuffer = (PBYTE)UDFSAlloc(m_pDriver->m_hHeap, dwDiskSize * 5);

    if (pCacheBuffer == NULL) {
        CompactAllHeaps();
    	pCacheBuffer = (PBYTE)UDFSAlloc(m_pDriver->m_hHeap, dwDiskSize * 5);

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

    if (pCacheBuffer == NULL) {
        //
        // TODO: shrink our cache
        //

        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        fRet = FALSE;
        goto Error;
    }

    fRet = m_pDriver->Read( dwSector, 0, dwDiskSize, pDiskBuffer, &dwBytesRead, NULL);


    // TODO: Verrify that Read() return accurate bytes read count in all cases and do (dwBytesRead != pParentDirectoryEntry->dwDiskSize)
    
    if (fRet == FALSE) {
        goto Error;
    }

    dwDiskSize = pParentDirectoryEntry->dwDiskSize;
    pDirectoryStart = pDiskBuffer;
    pCurrentDiskDirectory = (PNSR_FID)pDiskBuffer;

    if (pCurrentDiskDirectory->Destag.Ident == DESTAG_ID_NSR_FILE) {
        //
        //  We get this if we have immediate data the previous time
        //  around.
        //

        pDirectoryStart = (PBYTE)((&(((PICBFILE)pCurrentDiskDirectory)->EAs)) + ((PICBFILE)pCurrentDiskDirectory)->EALength);

        pCurrentDiskDirectory = (PNSR_FID)pDirectoryStart;
    }

    if (pCurrentDiskDirectory->Destag.Ident != DESTAG_ID_NSR_FID) {
    
        SetLastError(ERROR_DISK_CORRUPT);
        fRet = FALSE;
        goto Error;
    }

    //
    //  Parse the structures
    //

    while (TRUE)
    {
        if (pCurrentDiskDirectory->Destag.Ident == 0)
        {
            //
            //  This generally means we've arrived at a sector boundary
            //

            // TODO: Clean this up
            pCurrentDiskDirectory = (PNSR_FID) ((PBYTE)pDiskBuffer + (((PBYTE)pCurrentDiskDirectory - pDiskBuffer + (CD_SECTOR_SIZE-1)) & ~(CD_SECTOR_SIZE-1)));
        }

        if (((DWORD)pCurrentDiskDirectory) >= (((DWORD)pDirectoryStart) + dwDiskSize)) {
            pCurrentCacheEntry->cbSize = 0;
            break;
        }

        //
        //  If this bit is set then this is a reference to itself, a . entry
        //  so we skip it
        //

        if ((pCurrentDiskDirectory->Flags & 8) == 0) {
            //
            //  This is very inefficient but this is what we have to do ! We have to go out and read a whole
            //  sector for every directory entry.  This is the only way to find the file size. 
            //

            fRet = ResolveFilePosition( pCurrentDiskDirectory->Icb.Start.Lbn + m_dwPartitionStart, pCurrentCacheEntry, &dwFileSize);

            if (fRet == FALSE) {
                goto Error;
            }

            //
            //  Copy information
            //

            if (pCurrentDiskDirectory->Flags & NSR_FID_F_DIRECTORY) {
                pCurrentCacheEntry->fFlags = FILE_ATTRIBUTE_DIRECTORY;
                pCurrentCacheEntry->dwDiskSize = dwFileSize;
            } else {
                pCurrentCacheEntry->fFlags = 0;
            }

            if (pCurrentDiskDirectory->Flags & NSR_FID_F_HIDDEN) {
                pCurrentCacheEntry->fFlags |= FILE_ATTRIBUTE_HIDDEN;
            }

            

            pCurrentCacheEntry->pCacheLocation = NULL;
            pCurrentCacheEntry->pHeader = pCacheHeader;
            pCurrentCacheEntry->cbSize = ALIGN(sizeof(DIRECTORY_ENTRY) + (pCurrentDiskDirectory->FileIDLen * sizeof(WCHAR)));

            //
            //  Copy file name
            //

            pStr = pCurrentDiskDirectory->ImpUse + (pCurrentDiskDirectory->ImpUseLen);

            if (pStr[0] == 8) {

                pStr++;

                for (count = 0, byte = 0; byte < (pCurrentDiskDirectory->FileIDLen - 1); byte++, count += 2) {
                    pCurrentCacheEntry->szNameA[count] = pStr[byte];
                    pCurrentCacheEntry->szNameA[count+1] = 0;
                }
            } else 
            if (pStr[0] == 16) {
                pStr++;

                for (count = 0; count < (pCurrentDiskDirectory->FileIDLen - 1); count+=2)
                {
                    //
                    //  convert from big to little endian
                    //

                    pCurrentCacheEntry->szNameA[count] = pStr[count+1];
                    pCurrentCacheEntry->szNameA[count+1] = pStr[count];
                }
                pCurrentCacheEntry->szNameA[count] = 0;
                count++;
            } else {
                SetLastError(ERROR_DISK_CORRUPT);
                fRet = FALSE;
                goto Error;
            }

            pCurrentCacheEntry->szNameA[count] = 0;

            //
            //  Update the cache endFileIDLen
            //

            pCurrentCacheEntry = NextDirectoryEntry(pCurrentCacheEntry);
        }

        //
        //  Move the disk cache pointer forward
        //

        // TODO: Clean this up
        pCurrentDiskDirectory = (PNSR_FID) (((PBYTE)pCurrentDiskDirectory)+
                                (((DWORD)(sizeof(NSR_FID) + pCurrentDiskDirectory->ImpUseLen + pCurrentDiskDirectory->FileIDLen)) & (DWORD)(~3)));
    }
Error:

    UDFSFree(m_pDriver->m_hHeap, pDiskBuffer);

    if (fRet == FALSE) {
        UDFSFree(m_pDriver->m_hHeap, pCacheBuffer);
    } else {
        //
        //  Shrink the size of the cache entry to the actual size
        //

        pParentDirectoryEntry->pCacheLocation = (PDIRECTORY_ENTRY)UDFSReAlloc( m_pDriver->m_hHeap, pCacheBuffer,
                                                                              ((DWORD)pCurrentCacheEntry - (DWORD)pCacheBuffer) +
                                                                              sizeof(pCurrentCacheEntry->cbSize));
        if (!pParentDirectoryEntry->pCacheLocation) {
            UDFSFree( m_pDriver->m_hHeap, pCacheBuffer);
            fRet = FALSE;
        }
    }

    return fRet;
}


//+-------------------------------------------------------------------------
//
//  Member:     CUDFSFileSystem::ResolveFilePosition
//
//  Synopsis:   udf volumes never directly point to the file or directory
//              they go through a file icb first.  This resolves that
//              indirection.
//
//              The indirection is there to support for information on the
//              file, including fragmentation information, which we don't
//              support.
//
//  Arguments:  [dwDiskLocation] --
//              [pdwSector]      --
//              [pdwDiskSize]    --
//              [pInfoLength]    --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

BOOL CUDFSFileSystem::ResolveFilePosition(
        DWORD               dwDiskLocation,
        PDIRECTORY_ENTRY    pCurrentCacheEntry,
        PDWORD              pInfoLength)
{
    BOOL        fRet;
    VSD_GENERIC GenericVSD;
    PICBFILE    pICB = (PICBFILE)&GenericVSD;
    SYSTEMTIME  SystemTime;


    memset( pICB, 0, sizeof(1)); // Prefix
    fRet = m_pDriver->ReadDisk( dwDiskLocation, (PBYTE)&GenericVSD, sizeof(VSD_GENERIC));

    if (fRet == FALSE) {
        return FALSE;
    }


    //
    //  Make sure we have the correct tag identifier
    //

    if (pICB->Destag.Ident != DESTAG_ID_NSR_FILE) {
        SetLastError(ERROR_DISK_CORRUPT);
        return FALSE;
    }

    //
    //  Copy the file time
    //

    SystemTime.wYear = pICB->ModifyTime.Year;
    SystemTime.wMonth = pICB->ModifyTime.Month;
    SystemTime.wDay = pICB->ModifyTime.Day;
    SystemTime.wHour = pICB->ModifyTime.Hour;
    SystemTime.wMinute = pICB->ModifyTime.Minute;
    SystemTime.wSecond = pICB->ModifyTime.Second;
    SystemTime.wMilliseconds = (pICB->ModifyTime.Usec100 / 10) + (pICB->ModifyTime.CentiSecond * 10);

    SystemTimeToFileTime(&SystemTime, &pCurrentCacheEntry->Time);

    //
    //  Make sure the size isn't too big
    //  If it become important to have big files we need to change the
    //  entry directoryentry
    //

    *pInfoLength = (DWORD)pICB->InfoLength;

    //
    //  The position of the file is at the end of the structure
    //

    pCurrentCacheEntry->dwByteOffset = 0;

    switch (pICB->Icbtag.Flags & ICBTAG_F_ALLOC_MASK) {
    case ICBTAG_F_ALLOC_SHORT:

        PSHORTAD pShortAD;

        pShortAD = (PSHORTAD)((&(pICB->EAs)) + pICB->EALength);

        //
        //  Update the size based on allocation structure
        //

        pCurrentCacheEntry->dwDiskSize = (DWORD)*pInfoLength;
        pCurrentCacheEntry->dwDiskLocation = pShortAD->Start + m_dwPartitionStart;
        break;

    case ICBTAG_F_ALLOC_LONG:

        PLONGAD pLongAD;

        pLongAD = (PLONGAD)((&(pICB->EAs)) + pICB->EALength);

        //
        // Update the size based on the allocation structure
        //

        pCurrentCacheEntry->dwDiskSize = (DWORD)*pInfoLength;
        pCurrentCacheEntry->dwDiskLocation = pLongAD->Start.Lbn + m_dwPartitionStart;
        break;

    case ICBTAG_F_ALLOC_EXTENDED:

        //
        // EXTAD
        //

        DebugBreak();
        break;

    case ICBTAG_F_ALLOC_IMMEDIATE:

        pCurrentCacheEntry->dwDiskSize = pICB->AllocLength;
        *pInfoLength = pICB->AllocLength;
        pCurrentCacheEntry->dwDiskLocation = dwDiskLocation;
        pCurrentCacheEntry->dwByteOffset = sizeof(ICBFILE)+pICB->EALength-1;
      break;
    }


    return TRUE;
}

