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

/*++

Module Name:
    lckmgrapi.cpp

Abstract:
    Core Lock Manager API.  This module is used by the Lock Manager API for
    FSDs/File Systems.

Revision History:

--*/

#include "lockmgrapi.hpp"
#include "lockmgrdbg.hpp"
#include "lockcol.hpp"
#include "lockset.hpp"
#include "locklist.hpp"
#include "lock.hpp"
#include "range.hpp"

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL RemoveLock(
    CFileLockSet *pFileLockSet,
    DWORD hOwner,
    CRange *pRange
    )
{
    SETFNAME(_T("RemoveLock"));

    CFileLockList *pFileLockList;
    CFileLock *pFileLock;
    BOOL fInsertFileLockList = TRUE;
    BOOL fResult = FALSE;

    PREFAST_DEBUGCHK(NULL != pFileLockSet);
    PREFAST_DEBUGCHK(NULL != hOwner);
    PREFAST_DEBUGCHK(NULL != pRange);

    // remove list, remove lock, delete lock, re-insert list

    pFileLockList = pFileLockSet->Remove(hOwner);
    if (NULL != pFileLockList) {
        PREFAST_DEBUGCHK(NULL == pFileLockList->GetNext());
        pFileLock = pFileLockList->Remove(pRange);
        if (NULL != pFileLock) {
            PREFAST_DEBUGCHK(NULL == pFileLock->GetNext());
            delete pFileLock;
            if (pFileLockList->IsEmpty()) {
                delete pFileLockList;
                fInsertFileLockList = FALSE;
            }
            fResult = TRUE;
        }
        if (fInsertFileLockList) {
            pFileLockSet->Insert(pFileLockList);
        }
    }

    return fResult;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

PVOID
LXX_CreateLockContainer(
    )
{
    CFileLockCollection *pFileLockCollection = NULL;

    // if malloc fails, null is returned; pass null to caller to indicate failure
    pFileLockCollection = new CFileLockCollection();

    return (PVOID)pFileLockCollection;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

VOID
LXX_DestroyLockContainer(
    PVOID pvLockContainer
    )
{
    SETFNAME(_T("LXX_DestroyLockContainer"));

    CFileLockCollection *pFileLockCollection;

    if (NULL == pvLockContainer) {
        return;
    }

    // only our FSD-s use lock manager, so we trust pvLockContainer (__try not required)

    pFileLockCollection = (CFileLockCollection *)pvLockContainer;
    if (SIG != pFileLockCollection->GetSignature()) {
        DEBUGMSG(1, (_T("%s failed to destroy lock container; bad lock container\r\n"), pszFname));
        DEBUGCHK(0);
        return;
    }

    delete pFileLockCollection;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL
LXX_IsLockContainerEmpty(
    PVOID pvLockContainer
    )
{
    SETFNAME(_T("LXX_IsLockContainerEmpty"));

    CFileLockCollection *pFileLockCollection;

    if (NULL == pvLockContainer) {
        return TRUE;
    }

    pFileLockCollection = (CFileLockCollection *)pvLockContainer;
    if (SIG != pFileLockCollection->GetSignature()) {
        DEBUGMSG(1, (_T("%s failed to check if lock container empty; bad lock container\r\n"), pszFname));
        DEBUGCHK(0);
        return FALSE;
    }

    return pFileLockCollection->IsEmpty();
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

// undo flags

#define DSET   (1 << 0)
#define DLIST  (1 << 1)
#define DRANGE (1 << 2)

LOCKRESULT
LXX_Lock(
    PVOID pvLockContainer,
    DWORD dwFile,
    DWORD dwFlags,
    DWORD dwReserved,
    DWORD nNumberOfBytesToLockLow,
    DWORD nNumberOfBytesToLockHigh,
    LPOVERLAPPED lpOverlapped
    )
{
    SETFNAME(_T("LXX_Lock"));

    CFileLockCollection *pFileLockCollection;
    CFileLockSet *pFileLockSet = NULL;
    CFileLockList *pFileLockList = NULL;
    CFileLock *pFileLock;
    CRange *pRange;

    DWORD dwUndo = 0;

    LOCKRESULT lrResult = LR_ERROR;

    if (NULL == pvLockContainer) {
        DEBUGMSG(1, (_T("%s failed to install lock; lock container null\r\n"), pszFname));
        return LR_ERROR;
    }
    if (0 == dwFile) {
        DEBUGMSG(1, (_T("%s failed to install lock; file handle null\r\n"), pszFname));
        return LR_ERROR;
    }
    if (0 != dwReserved) {
        DEBUGMSG(1, (_T("%s (warning) dwReserved should be 0\r\n"), pszFname));
    }
    if (NULL == lpOverlapped) {
        DEBUGMSG(1, (_T("%s failed to install lock; overlapped null\r\n"), pszFname));
        return LR_ERROR;
    }
    if ((0 == nNumberOfBytesToLockLow) && (0 == nNumberOfBytesToLockHigh)) {
        DEBUGMSG(1, (_T("%s failed to install lock; bytes to lock 0\r\n"), pszFname));
        return LR_ERROR;
    }

    // only our FSD-s use lock manager, so we trust pvLockContainer (__try not required)

    pFileLockCollection = (CFileLockCollection *)pvLockContainer;
    if (SIG != pFileLockCollection->GetSignature()) {
        DEBUGMSG(1, (_T("%s failed to install lock; bad lock container\r\n"), pszFname));
        DEBUGCHK(0);
        return LR_ERROR;
    }

    // create range

    pRange = new CRange();
    if (NULL == pRange) {
        DEBUGMSG(1, (_T("%s failed to install lock; out of memory (new range)\r\n"), pszFname));
        return LR_ERROR;
    }
    pRange->SetOffset(lpOverlapped->Offset, lpOverlapped->OffsetHigh);
    pRange->SetLength(nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh);
    dwUndo |= DRANGE;

    if (!pRange->IsValid()) {
        DEBUGMSG(1, (_T("%s failed to install lock; range(%I64u, %I64u) invalid\r\n"), pszFname, pRange->GetStart(), pRange->GetFinish()));
        goto exit;
    }

    // test for conflict

    if (NULL != pFileLockCollection->GetExclusive()) {
        if (pFileLockCollection->GetExclusive()->IsConflict(pRange)) {
            if (dwFlags & LOCKFILE_FAIL_IMMEDIATELY) {
                goto exit;
            }
            lrResult = LR_CONFLICT;
            goto exit;
        }
    }

    // this lock does not conflict with an exclusive lock; shared locks can overlap,
    // so we only have to check the shared set if this is an exclusive lock

    if (NULL != pFileLockCollection->GetShared() && (dwFlags & LOCKFILE_EXCLUSIVE_LOCK)) {
        if (pFileLockCollection->GetShared()->IsConflict(pRange)) {
            if (dwFlags & LOCKFILE_FAIL_IMMEDIATELY) {
                goto exit;
            }
            lrResult = LR_CONFLICT;
            goto exit;
        }
    }

    // range does not conflict with existing lock; get appropriate set; create,
    // if necessary

    if (dwFlags & LOCKFILE_EXCLUSIVE_LOCK) {
        if (NULL == pFileLockCollection->GetExclusive()) {
            pFileLockCollection->SetExclusive(new CFileLockSet());
            if (NULL == pFileLockCollection->GetExclusive()) {
                goto exit;
            }
            dwUndo |= DSET;
        }
        pFileLockSet = pFileLockCollection->GetExclusive();
    }
    else {
        if (NULL == pFileLockCollection->GetShared()) {
            pFileLockCollection->SetShared(new CFileLockSet());
            if (NULL == pFileLockCollection->GetShared()) {
                goto exit;
            }
            dwUndo |= DSET;
        }
        pFileLockSet = pFileLockCollection->GetShared();
    }

    // remove owner's list from appropriate set; create, if necessary

    pFileLockList = pFileLockSet->Remove(dwFile);
    if (NULL == pFileLockList) {
        pFileLockList = new CFileLockList();
        if (NULL == pFileLockList) {
            DEBUGMSG(1, (_T("%s failed to install lock; out of memory (new lock list)\r\n"), pszFname));
            goto exit;
        }
        dwUndo |= DLIST;
        if (NULL == pFileLockList->GetRange()) {
            DEBUGMSG(1, (_T("%s failed to install lock; out of memory (new lock list range)\r\n"), pszFname));
            goto exit;
        }
        pFileLockList->SetOwner(dwFile);
    }
    pFileLockList->SetNext(NULL);

    // create lock

    pFileLock = new CFileLock();
    if (NULL == pFileLock) {
        DEBUGMSG(1, (_T("%s failed to install lock; out of memory (new lock)\r\n"), pszFname));
        goto exit;
    }
    pFileLock->SetRange(pRange);

    // add lock to owner's list

    pFileLockList->Insert(pFileLock);

    // re-insert owner's list in appropriate set

    pFileLockSet->Insert(pFileLockList);

    lrResult = LR_SUCCESS;
    dwUndo = 0;

exit:;
    if (dwUndo & DRANGE) {
        delete pRange;
    }
    if (dwUndo & DLIST) {
        delete pFileLockList;
    }
    if (dwUndo & DSET) {
        delete pFileLockSet;
        if (dwFlags & LOCKFILE_EXCLUSIVE_LOCK) {
            pFileLockCollection->SetExclusive(NULL);
        }
        else {
            pFileLockCollection->SetShared(NULL);
        }
    }
    return lrResult;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL
LXX_Unlock(
    PVOID pvLockContainer,
    DWORD dwFile,
    DWORD dwReserved,
    DWORD nNumberOfBytesToUnlockLow,
    DWORD nNumberOfBytesToUnlockHigh,
    LPOVERLAPPED lpOverlapped
    )
{
    SETFNAME(_T("LXX_Unlock"));

    CFileLockCollection *pFileLockCollection;
    CRange Range;
    BOOL fResult = FALSE;

    if (NULL == pvLockContainer) {
        DEBUGMSG(1, (_T("%s failed to remove lock; lock container null\r\n"), pszFname));
        return FALSE;
    }
    if (0 == dwFile) {
        DEBUGMSG(1, (_T("%s failed to remove lock; file handle null\r\n"), pszFname));
        return FALSE;
    }
    if (NULL == lpOverlapped) {
        DEBUGMSG(1, (_T("%s failed to remove lock; overlapped null\r\n"), pszFname));
        return FALSE;
    }
    if ((0 == nNumberOfBytesToUnlockLow) && (0 == nNumberOfBytesToUnlockHigh)) {
        DEBUGMSG(1, (_T("%s failed to remove lock; bytes to unlock 0\r\n"), pszFname));
        return FALSE;
    }

    // only our FSD-s use lock manager, so we trust pvLockContainer (__try not required)

    pFileLockCollection = (CFileLockCollection *)pvLockContainer;
    if (SIG != pFileLockCollection->GetSignature()) {
        DEBUGMSG(1, (_T("%s failed to remove lock; bad lock container\r\n"), pszFname));
        DEBUGCHK(0);
        return FALSE;
    }

    Range.SetOffset(lpOverlapped->Offset, lpOverlapped->OffsetHigh);
    Range.SetLength(nNumberOfBytesToUnlockLow, nNumberOfBytesToUnlockHigh);

    if (!Range.IsValid()) {
        return FALSE;
    }

    if (pFileLockCollection->IsEmpty()) {
        goto exit;
    }

    // remove lock

    // check exclusive set

    if (NULL != pFileLockCollection->GetExclusive()) {
        if (RemoveLock(pFileLockCollection->GetExclusive(), dwFile, &Range)) {
            if (pFileLockCollection->GetExclusive()->IsEmpty()) {
                delete pFileLockCollection->GetExclusive();
                pFileLockCollection->SetExclusive(NULL);
            }
            fResult = TRUE;
            goto exit;
        }
    }

    // check shared set

    if (NULL != pFileLockCollection->GetShared()) {
        if (RemoveLock(pFileLockCollection->GetShared(), dwFile, &Range)) {
            if (pFileLockCollection->GetShared()->IsEmpty()) {
                delete pFileLockCollection->GetShared();
                pFileLockCollection->SetShared(NULL);
            }
            fResult = TRUE;
            goto exit;
        }
    }

exit:;
    return fResult;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL
LXX_UnlockLocksOwnedByHandle(
    PVOID pvLockContainer,
    DWORD dwFile
    )
{
    SETFNAME(_T("LXX_UnlockLocksOwnedByHandle"));

    CFileLockCollection *pFileLockCollection;
    CFileLockList *pFileLockList;

    if (0 == dwFile) {
        DEBUGMSG(1, (_T("%s failed to remove lock(s); file handle null\r\n"), pszFname));
        return FALSE;
    }
    if (NULL == pvLockContainer) {
        DEBUGMSG(1, (_T("%s failed to remove lock(s); lock container null\r\n"), pszFname));
        return FALSE;
    }

    // only our FSD-s use lock manager, so we trust pvLockContainer (__try not required)

    pFileLockCollection = (CFileLockCollection *)pvLockContainer;
    if (SIG != pFileLockCollection->GetSignature()) {
        DEBUGMSG(1, (_T("%s failed to remove lock(s); bad lock container\r\n"), pszFname));
        DEBUGCHK(0);
        return FALSE;
    }

    // check exclusive set

    if (NULL != pFileLockCollection->GetExclusive()) {
        pFileLockList = pFileLockCollection->GetExclusive()->Remove(dwFile);
        if (NULL != pFileLockList) {
            delete pFileLockList;
            if (pFileLockCollection->GetExclusive()->IsEmpty()) {
                delete pFileLockCollection->GetExclusive();
                pFileLockCollection->SetExclusive(NULL);
            }
        }
    }

    // check shared set

    if (NULL != pFileLockCollection->GetShared()) {
        pFileLockList = pFileLockCollection->GetShared()->Remove(dwFile);
        if (NULL != pFileLockList) {
            delete pFileLockList;
            if (pFileLockCollection->GetShared()->IsEmpty()) {
                delete pFileLockCollection->GetShared();
                pFileLockCollection->SetShared(NULL);
            }
        }
    }

    return TRUE;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL
LXX_AuthorizeAccess(
    PVOID pvLockContainer,
    DWORD dwFile,
    BOOL fRead,
    DWORD dwFilePositionLow,
    DWORD dwFilePositionHigh,
    DWORD nBytesToBeAccessed
    )
{
    SETFNAME(_T("LXX_AuthorizeAccess"));

    CFileLockCollection *pFileLockCollection;
    CRange Range;
    BOOL fResult = FALSE;

    if (0 == dwFile) {
        DEBUGMSG(1, (_T("%s failed to authorize access; file handle null\r\n"), pszFname));
        return FALSE;
    }
    if (NULL == pvLockContainer) {
        DEBUGMSG(1, (_T("%s failed to authorize access; lock container null\r\n"), pszFname));
        return FALSE;
    }

    // only our FSD-s use lock manager, so we trust pvLockContainer (__try not required)

    pFileLockCollection = (CFileLockCollection *)pvLockContainer;
    if (SIG != pFileLockCollection->GetSignature()) {
        DEBUGMSG(1, (_T("%s failed to authorize access; bad lock container\r\n"), pszFname));
        DEBUGCHK(0);
        return FALSE;
    }

    // create range

    Range.SetOffset(dwFilePositionLow, dwFilePositionHigh);
    Range.SetLength(nBytesToBeAccessed, 0);

    if (!Range.IsValid()) {
        return FALSE;
    }

    // if read, don't check shared set

    if (fRead) {
        if (NULL != pFileLockCollection->GetExclusive()) {
            // ignore owner's list
            if (pFileLockCollection->GetExclusive()->IsConflict(&Range, dwFile)) {
                goto exit;
            }
        }
    }
    else {
        if (NULL != pFileLockCollection->GetExclusive()) {
            // ignore owner's list
            if (pFileLockCollection->GetExclusive()->IsConflict(&Range, dwFile)) {
                goto exit;
            }
        }
        if (NULL != pFileLockCollection->GetShared()) {
            // don't ignore owner's list
            if (pFileLockCollection->GetShared()->IsConflict(&Range)) {
                goto exit;
            }
        }
    }

    fResult = TRUE;

exit:;
    return fResult;
}

