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
//  File:       udfscln.cpp
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
//  Member:     CReadOnlyFileSystemDriver::Clean
//
//  Synopsis:
//
//  Arguments:  (none)
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

void CReadOnlyFileSystemDriver::Clean()
{
    PFILE_HANDLE_INFO   pFileHandle;
    DWORD               c;

    if (InterlockedTestExchange( &(m_State), StateDirty, StateCleaning) == StateDirty) {
        //
        //  We've marked the list as in a cleaning state
        //

        if (m_RootDirectoryPointer.pCacheLocation != NULL) {
            CleanRecurse(m_RootDirectoryPointer.pCacheLocation);
        }

        m_RootDirectoryPointer.cbSize = 0;
        m_RootDirectoryPointer.pCacheLocation = NULL;

        //
        //  Clean the file handle list
        //

        pFileHandle = m_pFileHandleList;

        while (pFileHandle != NULL) {
        
            InterlockedExchange(&(pFileHandle->State), StateDirty);
            pFileHandle = pFileHandle->pNext;
        }

        //
        //  Clean the find handle list
        //

        for (c = 0; c < m_cFindHandleListSize; c++) {
            if (m_ppFindHandles[c] != NULL) {
                DEBUGMSG(ZONE_HANDLES, (TEXT("UDFS!Clean: marking handle %x as dirty\n"), m_ppFindHandles[c]));

                InterlockedExchange(
                        &(m_ppFindHandles[c]->State),
                        StateDirty);
            }
        }

        InterlockedExchange( &m_State, StateClean);
    }
}

//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::CleanRecurse
//
//  Synopsis:
//
//  Arguments:  [pDir] --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

void CReadOnlyFileSystemDriver::CleanRecurse(PDIRECTORY_ENTRY pDir)
{
    PDIRECTORY_ENTRY    pCurrentDir;
    PDIRECTORY_HEADER   pCurrentHeader;

    //
    //  skip the header
    //

    pCurrentDir = NextDirectoryEntry(pDir);

    while (pCurrentDir->cbSize != 0) {
        if (pCurrentDir->pCacheLocation != NULL) {
            CleanRecurse(pCurrentDir->pCacheLocation);
        }

        pCurrentDir = NextDirectoryEntry(pCurrentDir);
    }

    pCurrentHeader = (PDIRECTORY_HEADER)pDir;

    if (InterlockedDecrement(&(pCurrentHeader->dwLockCount)) == 0) {
        UDFSFree(m_hHeap, pCurrentHeader);
    } else {
        pCurrentHeader->pParent = (PDIRECTORY_ENTRY)-1;
    }
}

//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::IsHandleDirty
//
//  Synopsis:
//
//  Arguments:  [pHandle] --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

BOOL CReadOnlyFileSystemDriver::IsHandleDirty(PFILE_HANDLE_INFO pHandle)
{
    //
    //  Check to see if the handle itself is diry.
    //  If so, return right away
    //

    if (pHandle->State != StateClean) {
        return TRUE;
    }

    //
    //  Check to see if we are marked as clean.  If we are then return ok
    //

    if (m_State == StateClean) {
        return FALSE;
    }

    //
    //  If we are dirty then mark us as cleaning and go ahead and clean
    //

    EnterCriticalSection(&m_csListAccess);

    Clean();

    LeaveCriticalSection(&m_csListAccess);

    return TRUE;
}


