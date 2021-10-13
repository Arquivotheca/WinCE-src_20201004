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
#include "binfs.h"

extern BinVolume *g_pVolume;


DWORD CopyDirEntry(PWIN32_FIND_DATAW pfd, BinDirList *pDirectory)
{
    DWORD dwError = ERROR_SUCCESS;
    __try {
        pfd->dwFileAttributes = pDirectory->dwAttributes;
        pfd->nFileSizeLow = pDirectory->dwRealFileSize;
        pfd->nFileSizeHigh = 0;
        wcscpy( pfd->cFileName, pDirectory->szFileName);
        pfd->ftCreationTime = pDirectory->ft;
        pfd->ftLastAccessTime = pDirectory->ft;
        pfd->ftLastWriteTime = pDirectory->ft;
        pfd->dwOID = 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        dwError =  ERROR_INVALID_PARAMETER;
    }
    return dwError;
}


extern "C" HANDLE FSD_FindFirstFileW(BinVolume *pVolume, HANDLE hProc, PCWSTR pwsFileSpec, PWIN32_FIND_DATAW pfd)
{
    DWORD dwError =ERROR_SUCCESS;
    SearchHandle *pSearch = NULL;
    BinDirList *pDirectory = pVolume->pDirectory;
    HANDLE hSearch = INVALID_HANDLE_VALUE;
    
    DEBUGMSG(ZONE_API, (L"BINFS: FSD_FindFirstFileW\n"));

    __try {
        if (!pwsFileSpec || !pfd || (wcslen(pwsFileSpec) >= MAX_PATH)) {
            dwError = ERROR_INVALID_PARAMETER;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
    }
    
    if (ERROR_SUCCESS == dwError) {
        while ((*pwsFileSpec == L'\\') || (*pwsFileSpec == L'/'))
            pwsFileSpec++;

        if (wcslen(pwsFileSpec) > SYSTEM_DIR_LEN) {
            if ((wcsnicmp( pwsFileSpec, SYSTEM_DIR, SYSTEM_DIR_LEN) == 0) && ((*(pwsFileSpec+SYSTEM_DIR_LEN) == L'\\') || (*(pwsFileSpec+SYSTEM_DIR_LEN) == L'/'))) {
                pwsFileSpec += SYSTEM_DIR_LEN;
                while ((*pwsFileSpec == L'\\') || (*pwsFileSpec == L'/')) {
                    pwsFileSpec++;
                }
            }
        }

        while(pDirectory) {
            if (MatchesWildcardMask(wcslen(pwsFileSpec), pwsFileSpec, wcslen(pDirectory->szFileName), pDirectory->szFileName)) {
                break;
            }
            pDirectory = pDirectory->pNext;
        }
        if (pDirectory) {
            pSearch = new SearchHandle;
            if (pSearch) {
                pSearch->pDirectory = pDirectory;
                wcscpy( pSearch->szFileMask, pwsFileSpec);
                dwError = CopyDirEntry( pfd, pDirectory);
                hSearch = ::FSDMGR_CreateSearchHandle(pVolume->hVolume, hProc, (PSEARCH)pSearch);
            } else {
                dwError = GetLastError();
            }    
        } else {
            dwError = ERROR_FILE_NOT_FOUND;
        }    
    }    
    if (ERROR_SUCCESS != dwError) {
        SetLastError( dwError);
    }
    return hSearch;
}


extern "C" BOOL FSD_FindNextFileW(SearchHandle *pSearch, PWIN32_FIND_DATAW pfd)
{
    BOOL fSuccess = FALSE;
    DWORD dwError =ERROR_NO_MORE_FILES;
    pSearch->pDirectory = pSearch->pDirectory->pNext;

    if (!pfd) {
        dwError = ERROR_INVALID_PARAMETER;
        return FALSE;
    }    
    
    DEBUGMSG(ZONE_API, (L"BINFS: FSD_FindNextFileW\n"));
    while(pSearch->pDirectory) {
        if (MatchesWildcardMask(wcslen(pSearch->szFileMask), pSearch->szFileMask, wcslen(pSearch->pDirectory->szFileName), pSearch->pDirectory->szFileName)) {
            BinDirList *pDirectory = g_pVolume->pDirectory;
            BOOL bRepeat = FALSE;
            while(pDirectory && (pDirectory != pSearch->pDirectory)) {
                if (wcscmp(pDirectory->szFileName, pSearch->pDirectory->szFileName) == 0) {
                    bRepeat = TRUE;
                    break;
                }    
                pDirectory = pDirectory->pNext;
            }
            if (!bRepeat)
                break;
        }
        pSearch->pDirectory = pSearch->pDirectory->pNext;
    }
    if (pSearch->pDirectory) {
        fSuccess = TRUE;
        dwError = CopyDirEntry( pfd, pSearch->pDirectory);
    }
    if (ERROR_SUCCESS != dwError) {
        SetLastError( dwError);
    }
    return fSuccess;
}

extern "C" BOOL FSD_FindClose(SearchHandle *pSearch)
{
    BOOL fSuccess = TRUE;
    
    DEBUGMSG(ZONE_API, (L"BINFS: FSD_FindClose\n"));
    delete pSearch;
    return fSuccess;
}


