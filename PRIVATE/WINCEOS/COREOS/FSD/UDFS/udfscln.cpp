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
//  File:       udfscln.cpp
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

    if ((pHandle->State != StateClean) && (pHandle->State != StateInUse) && (pHandle->State != StateWindUp) ) {
        return TRUE;
    }

    //
    //  Check to see if we are marked as clean/StateInUse - cache in use/StateWindUp - disk removed or error.  If we are then return ok
    //

    if ((m_State == StateClean) || (m_State == StateInUse) || (m_State == StateWindUp)) {
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


