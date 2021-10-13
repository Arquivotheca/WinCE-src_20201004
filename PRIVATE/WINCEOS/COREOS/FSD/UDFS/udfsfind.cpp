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
//  File:       udfsfind.cpp
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
    DWORD               c;
    BOOL                fRet = FALSE;

    if ((m_State != StateClean) && (m_State != StateInUse) && (m_State != StateWindUp) ) {
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
    //  Make sure the root is read in
    //

    if (m_RootDirectoryPointer.cbSize == 0) {
        fRet = Mount();

        if (fRet == FALSE) {
            goto error;
        }
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

    if (wcscmp(pLastFileName, L"*.*") == 0) {
        pLastFileName[1] = 0;
        pLastFileName[2] = 0;
    }

    if(m_pFileSystem) {
        if (m_RootDirectoryPointer.pCacheLocation == NULL) {
            fRet = m_pFileSystem->ReadDirectory(L"", &m_RootDirectoryPointer);
            if (fRet == FALSE) {
                goto error; 
            }
        }
    }
    else {
        SetLastError(ERROR_MEDIA_CHANGED);
        goto error; 
    }

    //
    //  Changing status from StateClean to StateInUse so that cache wont 
    //  get cleaned suddenly on unmount (causing access violation).
    //  On unmount the state will change from  StateInUse -> StateWindUp by 
    //  UDFSDeviceIoControl() so that FindFileRecurse() can return
    //  without causing access violation (accessing deleted cache).
    //

    if (InterlockedTestExchange( &(m_State), StateClean, StateInUse) == StateClean) {
        DEBUGMSG(ZONE_FUNCTION,(TEXT("CReadOnlyFileSystemDriver::FindFile(): Changing state from StateClean -> StateInUse\r\n")));
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
	
    // Resetting the states StateInUse/StateWindUp to StateClean  

    if (InterlockedTestExchange( &(m_State), StateInUse, StateClean) == StateInUse) {
        DEBUGMSG(ZONE_FUNCTION,(TEXT("CReadOnlyFileSystemDriver::FindFile(): Changing state from StateInUse -> StateClean\r\n")));		
    }
    
    if (InterlockedTestExchange( &(m_State), StateWindUp, StateClean) == StateWindUp) {
        DEBUGMSG(ZONE_FUNCTION,(TEXT("CReadOnlyFileSystemDriver::FindFile(): Changing state from StateWindUp -> StateClean\r\n")));					
    }
 
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

BOOL CReadOnlyFileSystemDriver::FindFileRecurse( LPWSTR pFileName, LPWSTR pFullPath, PDIRECTORY_ENTRY    pInitialDirectoryEntry, PPDIRECTORY_ENTRY   ppFileInfo)
{
    PDIRECTORY_ENTRY    pCurrentDirectoryEntry;
    BOOL                fRet;
    DWORD               ccNameLen;

    int                 lLen = lstrlen(pFileName);

    //
    //  We're actually reading the cbSize from the header,
    //  but what the heck
    //

    pCurrentDirectoryEntry = (PDIRECTORY_ENTRY) ((DWORD)pInitialDirectoryEntry + (DWORD)pInitialDirectoryEntry->cbSize);
    
    //
    //  Loop through all entries in this directory
    //

    while (TRUE) {
        DWORD   dwCharLen;
	
        if (InterlockedTestExchange( &(m_State), StateWindUp, StateClean) == StateWindUp) {
            DEBUGMSG(ZONE_FUNCTION,(TEXT("CReadOnlyFileSystemDriver::FindFileRecurse(): Changing state from StateWindUp -> StateClean\r\n")));		
            return FALSE;
        }

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
            if(m_pFileSystem) {
                fRet = m_pFileSystem->ReadDirectory(pFullPath, pCurrentDirectoryEntry);

                if (fRet == FALSE) {
                    // oops, need better error here

                    SetLastError(ERROR_PATH_NOT_FOUND);
                    return FALSE;
                }
            }
            else {
                SetLastError(ERROR_MEDIA_CHANGED);
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

    *ppFileInfo = pCurrentDirectoryEntry;

    return TRUE;
}


