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
//  File:       udfsfind.cpp
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
//  Member:     CReadOnlyFileSystemDriver::FindFile
//
//  Synopsis:
//
//  Arguments:  [pFileName]  --
//              [ppFileInfo] --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

BOOL CReadOnlyFileSystemDriver::FindFile( LPCWSTR pFileName, PPDIRECTORY_ENTRY   ppDirEntry)
{
    LPWSTR              pParsedFileName;
    LPWSTR              pLastFileName;
    DWORD               dwLastFileNameLength = 0;
    int                 c;
    BOOL                fRet;

    if (m_State != StateClean) {
        Clean();
    }

    //
    //  Make a local copy.  Need the 2 to double null terminate.
    //

    pLastFileName = pParsedFileName = new WCHAR[lstrlen(pFileName) + 2];

    if (pParsedFileName == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    //
    //  If the file name starts with \ then move beyond it
    //

    if (pFileName[0] == '\\') {
        pFileName++;
    }

    //
    //  Convert all \ characters into nulls
    //

    for (c = 0; c < lstrlen(pFileName) + 1; c++) {
        if (pFileName[c] == '\\') {
            pParsedFileName[c] = 0;
            pLastFileName = pParsedFileName + c + 1;
        } else {
            pParsedFileName[c] = pFileName[c];
        }
    }

    pParsedFileName[c] = 0; // double null terminate.

    //
    //  We take the length of pLastFileName because if pLastFileName is
    //  actually shorter than 3 characters (3*sizeof(WCHAR) bytes) and the
    //  two characters after happen to match ".*", then we might write outside
    //  of pLastFileName
    //

    dwLastFileNameLength = lstrlen(pLastFileName);
    if ((3 == dwLastFileNameLength) && (wcscmp(pLastFileName, L"*.*") == 0)) {
        pLastFileName[1] = 0;
        pLastFileName[2] = 0;
    }

    if (m_RootDirectoryPointer.pCacheLocation == NULL) {
        fRet = m_pFileSystem->ReadDirectory(L"", &m_RootDirectoryPointer);

        if (fRet == FALSE) {
            goto error; 
        }
    }

    //
    //  Search for each entry recursively
    //

    fRet = FindFileRecurse(
        pParsedFileName,
        pParsedFileName,
        m_RootDirectoryPointer.pCacheLocation,
        ppDirEntry);

error:
    if (pParsedFileName) 
            delete [] pParsedFileName;

    return fRet;
}

//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::FindFileRecurse
//
//  Synopsis:
//
//  Arguments:  [pFileName]              --
//              [pFullPath]              --
//              [pInitialDirectoryEntry] --
//              [ppFileInfo]             --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

BOOL CReadOnlyFileSystemDriver::FindFileRecurse( __inout PWSTR pFileName,
                                                 __in PWSTR pFullPath, 
                                                 PDIRECTORY_ENTRY pInitialDirectoryEntry, 
                                                 PPDIRECTORY_ENTRY  ppFileInfo )
{
    PDIRECTORY_ENTRY    pCurrentDirectoryEntry;
    BOOL                fRet;
    DWORD               ccNameLen;
    DWORD               dwLastError;

    int                 lLen = lstrlen(pFileName);

    //
    //  We're actually reading the cbSize from the header,
    //  but what the heck
    //

    pCurrentDirectoryEntry = (PDIRECTORY_ENTRY) ((DWORD)pInitialDirectoryEntry + (DWORD)pInitialDirectoryEntry->cbSize);
    
#ifdef _PREFAST_
#pragma prefast (disable : 2016,"Not sure how to cleanly handle 2-null-terminated string with prefast")
#endif

    //
    //  Loop through all entries in this directory
    //
    while (TRUE) {
        DWORD   dwCharLen;

        if (IsLastDirectoryEntry(pCurrentDirectoryEntry)) {
            //
            //  If there is still more to search
            //

            if (pFileName[lLen + 1] != 0) {
                SetLastError(ERROR_PATH_NOT_FOUND);
            } else {
                SetLastError(ERROR_FILE_NOT_FOUND);
            }
            return FALSE;
        }

        if ((pCurrentDirectoryEntry->fFlags & IS_ANSI_FILENAME) == 0) {
            ccNameLen = lstrlen(pCurrentDirectoryEntry->szNameW);
            dwCharLen = 2;
        } else {
            ccNameLen = strlen(pCurrentDirectoryEntry->szNameA);
            dwCharLen = 1;
        }

        fRet = UDFSMatchesWildcard(lLen, pFileName, ccNameLen, pCurrentDirectoryEntry->szNameA ,dwCharLen);

        if (fRet == TRUE) {
            break;
        }

        //
        //  Go to the next one
        //

        pCurrentDirectoryEntry = NextDirectoryEntry(pCurrentDirectoryEntry);

    }

    if (pFileName[lLen + 1] != 0) {
        //
        //  There is still more to search, make sure this is a directory
        //

        if ((pCurrentDirectoryEntry->fFlags & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            //
            //  They have asked to a path where one of the elements before
            //  the end isn't a directory.  Needless to say that doesn't
            //  work.
            //

            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }

        if (pCurrentDirectoryEntry->pCacheLocation == NULL) {
            //
            //  If this directory isn't cached then read it in.
            //

            dwLastError = GetLastError();
            fRet = m_pFileSystem->ReadDirectory(pFullPath, pCurrentDirectoryEntry);

            if (fRet == FALSE) {
                // oops, need better error here
                if( dwLastError == GetLastError() )
                {
                    SetLastError(ERROR_PATH_NOT_FOUND);
                }
                return FALSE;
            }
        }

        //
        //  When we first came around we turned all the \'s into nulls.
        //  We turn then back so we can look up the whole path in the
        //  second arg.
        //

        pFileName[lLen] = '\\';

        return FindFileRecurse( pFileName + lLen + 1, pFullPath, pCurrentDirectoryEntry->pCacheLocation,  ppFileInfo);
    }

#ifdef _PREFAST_
#pragma prefast (enable : 2016,"")
#endif

    *ppFileInfo = pCurrentDirectoryEntry;

    return TRUE;
}


