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
//  File:       udfssrch.cpp
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
//  Member:     CReadOnlyFileSystemDriver::ROFS_FindFirstFile
//
//  Synopsis:   Implements findfirst
//
//  Arguments:  [hProc]       --
//              [pwsFileSpec] --
//              [pfd]         --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

HANDLE CReadOnlyFileSystemDriver::ROFS_FindFirstFile( HANDLE hProc, PCWSTR pwsFileSpec, PWIN32_FIND_DATAW pfd)
{
    PDIRECTORY_ENTRY        pDirEntry;
    BOOL                    fRet;
    HANDLE                  hFind = INVALID_HANDLE_VALUE;
    PFIND_HANDLE_INFO       psfr = NULL;
    DWORD                   dwFileSpecSize;
    DWORD                   dwMaskStart;
    DWORD                   c;
    DWORD                   dwBufferSize = 0;

    DEBUGMSG( ZONE_FUNCTION, (TEXT("UDFS!FindFirstFile Entered hProc=%08X FileSpec=%s\r\n"), hProc, pwsFileSpec));

    __try {
    
        memset(pfd, 0, sizeof(WIN32_FIND_DATAW));
        dwFileSpecSize = lstrlen(pwsFileSpec);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    //
    //  Find the mask by looking from the end and finding the first \
    //

    dwMaskStart = dwFileSpecSize;

    while ((pwsFileSpec[dwMaskStart-1] != '\\') && (dwMaskStart > 0)) {
        dwMaskStart--;
    }

    //
    //  *.* means they really want *, so take off the last two chars
    //

    if (wcscmp(pwsFileSpec + dwMaskStart, L"*.*") == 0) {
        dwFileSpecSize -= 2;
    }

    EnterCriticalSection(&m_csListAccess);

    fRet = FindFile(pwsFileSpec, &pDirEntry);

    if (fRet == FALSE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            SetLastError(ERROR_NO_MORE_FILES);
        }
        goto error;
    }

    //
    //  Allocate space on the end of the struct to store the mask
    //
    dwBufferSize = sizeof(FIND_HANDLE_INFO) + ((dwFileSpecSize - dwMaskStart) * sizeof(WCHAR));
    psfr = (PFIND_HANDLE_INFO)new BYTE[dwBufferSize];

    if (psfr == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        goto error;
    }

    DEBUGMSG(ZONE_HANDLES, (TEXT("UDFS!FindFirstFile: Created Find Handle (%x)\n"), psfr));

    //
    //  Copy the information into the search handle info.
    //

    ZeroMemory( psfr, dwBufferSize );

    psfr->pVol = this;
    psfr->pDirectoryEntry = pDirEntry;
    psfr->pHeader = pDirEntry->pHeader;

    psfr->szFileMask[dwFileSpecSize - dwMaskStart] = 0;

    memcpy(psfr->szFileMask, pwsFileSpec + dwMaskStart, (dwFileSpecSize - dwMaskStart) * sizeof(WCHAR));

    //
    //  Add it to the handle list
    //

    for (c = 0; c < m_cFindHandleListSize; c++) {
        if (m_ppFindHandles[c] == NULL) {
            DEBUGMSG(ZONE_HANDLES, (TEXT("UDFS!FindFirstFile: adding handle %x to slot %d\n"), psfr, c));

            m_ppFindHandles[c] = psfr;
            break;
        }
    }

    //
    //  No more room so grow it !!!
    //

    if (c == m_cFindHandleListSize)  {
    
        if (m_ppFindHandles == NULL) { // Is it the first time ???
            DEBUGMSG(ZONE_HANDLES, (TEXT("UDFS!FindFirstFile: trying to grow to %d slots\n"), FIND_HANDLE_LIST_START_SIZE));

            m_ppFindHandles = (PPFIND_HANDLE_INFO)UDFSAlloc( m_hHeap, sizeof(PFILE_HANDLE_INFO) * FIND_HANDLE_LIST_START_SIZE);
            if (m_ppFindHandles == NULL) {
                CompactAllHeaps();
                m_ppFindHandles = (PPFIND_HANDLE_INFO)UDFSAlloc( m_hHeap, sizeof(PFILE_HANDLE_INFO) * FIND_HANDLE_LIST_START_SIZE);

                if (m_ppFindHandles == NULL) {
                    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                    goto error;
                }
            }
            m_cFindHandleListSize = FIND_HANDLE_LIST_START_SIZE;
        } else {
            DEBUGMSG(ZONE_HANDLES, (TEXT("UDFS!FindFirstFile: trying to grow to %d slots\n"), m_cFindHandleListSize * 2));

            PVOID pTemp = UDFSReAlloc( m_hHeap, m_ppFindHandles, sizeof(PFILE_HANDLE_INFO) * 2 * m_cFindHandleListSize);
            
            if (pTemp == NULL) {
            
                CompactAllHeaps();
                pTemp = UDFSReAlloc( m_hHeap, m_ppFindHandles, sizeof(PFILE_HANDLE_INFO) * 2 * m_cFindHandleListSize);
            
                if (pTemp == NULL) {
                    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                    goto error;
                }
            }

            m_ppFindHandles = (PPFIND_HANDLE_INFO)pTemp;
            m_cFindHandleListSize *= 2;
        }

        DEBUGMSG(ZONE_HANDLES, (TEXT("UDFS!FindFirstFile: list grown to %d slots\n"), m_cFindHandleListSize));

        DEBUGMSG(ZONE_HANDLES, (TEXT("UDFS!FindFirstFile: adding handle %x to slot %d\n"), psfr, c));

        m_ppFindHandles[c] = psfr;
    }

    CopyDirEntryToWin32FindStruct(pfd, psfr->pDirectoryEntry);

    hFind = ::FSDMGR_CreateSearchHandle(m_hVolume, hProc, (PSEARCH)psfr);

error:

    LeaveCriticalSection(&m_csListAccess);

    return hFind;
}

//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::ROFS_FindNextFile
//
//  Synopsis:   Implements find next
//
//  Arguments:  [psfr] --
//              [pfd]  --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

BOOL CReadOnlyFileSystemDriver::ROFS_FindNextFile(PFIND_HANDLE_INFO   psfr, PWIN32_FIND_DATAW   pfd)
{
    BOOL                fRet = FALSE;
    PDIRECTORY_ENTRY    pCurrentDirectoryEntry;
    DWORD               dwCharLen;
    DWORD               ccNameLen;
    DWORD               lLen;

    EnterCriticalSection(&m_csListAccess);
	DEBUGMSG(ZONE_HANDLES, (TEXT("UDFS!FindNextFile enterned PFHI=%08X\n"), psfr));
    __try {
        memset(pfd, 0, sizeof(WIN32_FIND_DATAW));

        if (psfr->State == StateDirty) {
            SetLastError(ERROR_MEDIA_CHANGED);
            __leave;
        }

        //
        //  Get this if they call next after we already told them
        //  no more
        //

        if (IsLastDirectoryEntry(psfr->pDirectoryEntry)) {
            SetLastError(ERROR_NO_MORE_FILES);
            __leave;
        }

        lLen = lstrlen(psfr->szFileMask);

        //
        //  Start looking from one beyond where we last left off
        //

        pCurrentDirectoryEntry = NextDirectoryEntry(psfr->pDirectoryEntry);

        while (!IsLastDirectoryEntry(pCurrentDirectoryEntry)) {
            //
            //  Yes, it is possible on a udfs volume that files in the same
            //  dir can be be mixed ansi and unicode
            //

            if ((pCurrentDirectoryEntry->fFlags & IS_ANSI_FILENAME) == 0) {
            
                ccNameLen = lstrlen(pCurrentDirectoryEntry->szNameW);
                dwCharLen = 2;
            } else {
            
                ccNameLen = strlen(pCurrentDirectoryEntry->szNameA);
                dwCharLen = 1;
            }

            fRet = UDFSMatchesWildcard( lLen, psfr->szFileMask, ccNameLen, pCurrentDirectoryEntry->szNameA, dwCharLen);

            if (fRet == TRUE) {
                break;
            }

            //
            //  Move on to the next entry
            //

            pCurrentDirectoryEntry = NextDirectoryEntry(pCurrentDirectoryEntry);
        }

        //
        //  Store the current search position back info the search info.
        //

        psfr->pDirectoryEntry = pCurrentDirectoryEntry;

        if (fRet == FALSE) {
            SetLastError(ERROR_NO_MORE_FILES);
            __leave;
        } else {
            CopyDirEntryToWin32FindStruct(pfd, psfr->pDirectoryEntry);
        }

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        SetLastError(ERROR_INVALID_PARAMETER);
    }

    LeaveCriticalSection(&m_csListAccess);

    return fRet;
}

//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::ROFS_FindClose
//
//  Synopsis:   Closes down the search, unlocks the structure
//              frees memory
//
//  Arguments:  [psfr] --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

BOOL CReadOnlyFileSystemDriver::ROFS_FindClose(PFIND_HANDLE_INFO psfr)
{
    DWORD   c;

    DEBUGMSG(ZONE_HANDLES, (TEXT("UDFS!FindClose: trying to close handle %x\n"), psfr));

    EnterCriticalSection(&m_csListAccess);

    for (c = 0; c < m_cFindHandleListSize; c++) {
        if (m_ppFindHandles[c] == psfr) {
            DEBUGMSG(ZONE_HANDLES, (TEXT("UDFS!FindClose: closed handle %x\n"), psfr));

            m_ppFindHandles[c] = NULL;
            break;
        }
    }

    LeaveCriticalSection(&m_csListAccess);

    delete psfr;

    return TRUE;
}

//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::CopyDirEntryToWin32FindStruct
//
//  Synopsis:   copies the information from an internal directory entry to
//              a win32 entry
//
//  Arguments:  [pfd]       --
//              [pDirEntry] --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

void CReadOnlyFileSystemDriver::CopyDirEntryToWin32FindStruct(
        PWIN32_FIND_DATAW   pfd,
        PDIRECTORY_ENTRY    pDirEntry)
{
    pfd->dwFileAttributes = FILE_ATTRIBUTE_READONLY;

    pfd->nFileSizeLow = pDirEntry->dwDiskSize;

    if ((pDirEntry->fFlags & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
        pfd->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
        pfd->nFileSizeLow = 0;
    }

    if (pDirEntry->fFlags & FILE_ATTRIBUTE_HIDDEN) {
        pfd->dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;
    }    

    pfd->ftCreationTime = pDirEntry->Time;
    pfd->ftLastWriteTime = pDirEntry->Time;
    pfd->ftLastAccessTime = pDirEntry->Time;

    if ((pDirEntry->fFlags & IS_ANSI_FILENAME) == 0) {
        StringCchCopy( pfd->cFileName, MAX_PATH, pDirEntry->szNameW );
    } else {
    
        MultiByteToWideChar( CP_OEMCP, 0, pDirEntry->szNameA, -1, pfd->cFileName, MAX_PATH);
    }

}

