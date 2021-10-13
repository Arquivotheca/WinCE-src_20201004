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
    lockset.h

Abstract:
    Lock List Abstraction.  A lock list is a set of locks a global list range.
    A lock list is sorted by the lowest byte locked by a lock.  The list range
    is defined as the lowest byte locked by a lock in the list and the highest
    byte locked by a lock in the list.

Revision History:

--*/

#include "locklist.hpp"

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

CFileLockList::CFileLockList()
{
    m_dwOwner = NULL;
    m_pRange = new CRange();
    m_pListHead = NULL;
    m_pNextList = NULL;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

CFileLockList::~CFileLockList()
{
    CFileLock *pLockToDelete;
    CFileLock *pNextLock;

    if (NULL != m_pRange) {
        delete m_pRange;
    }

    DEBUGCHK(NULL == m_pNextList);

    // delete list

    pLockToDelete = m_pListHead;
    while (NULL != pLockToDelete) {
        pNextLock = pLockToDelete->GetNext();
        pLockToDelete->SetNext(NULL);
        delete pLockToDelete;
        pLockToDelete = pNextLock;
    }
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

CFileLock *CFileLockList::GetLast()
{
    CFileLock *pCurLock;
    CFileLock *pMax;
    ULONGLONG ullFurthestByte = 0;

    pCurLock = m_pListHead;
    pMax = pCurLock;

    // find last lock in list

    while (NULL != pCurLock) {
        DEBUGCHK(NULL != pCurLock->GetRange());
        if (ullFurthestByte < pCurLock->GetRange()->GetFinish()) {
            ullFurthestByte = pCurLock->GetRange()->GetFinish();
            pMax = pCurLock;
        }
        pCurLock = pCurLock->GetNext();
    }

    return pMax;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL CFileLockList::IsEmpty()
{
    return (NULL == m_pListHead) ? TRUE : FALSE;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL CFileLockList::IsConflict(CRange *pRange)
{
    CFileLock *pCurLock;

    DEBUGCHK(NULL != pRange);

    if (this->IsEmpty()) {
        return FALSE;
    }

    DEBUGCHK(NULL != m_pRange);
    switch (m_pRange->IsConflict(pRange)) {
    case RCT_NOCONFLICT_BEFORE:
    case RCT_NOCONFLICT_AFTER:
        return FALSE;
    case RCT_BOUNDARY:
        return TRUE;
    }

    // target lies within range of list

    pCurLock = m_pListHead;
    while (NULL != pCurLock) {
        DEBUGCHK(NULL != pCurLock->GetRange());
        switch (pRange->IsConflict(pCurLock->GetRange())) {
        case RCT_NOCONFLICT_BEFORE:
            // (lock lists are sorted by start of range)
            return FALSE;
        case RCT_NOCONFLICT_AFTER:
            pCurLock = pCurLock->GetNext();
            break;
        case RCT_BETWEEN:
        case RCT_BOUNDARY:
            return TRUE;
        }
    }

    // target is after all locks in list

    return FALSE;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

VOID CFileLockList::Insert(CFileLock *pFileLock)
{
    CFileLock *pBefore;
    CFileLock *pAfter;

    PREFAST_DEBUGCHK(NULL != pFileLock);
    PREFAST_DEBUGCHK(NULL != pFileLock->GetRange());
    PREFAST_DEBUGCHK(NULL == pFileLock->GetNext());

    // update range of list

    PREFAST_DEBUGCHK(NULL != m_pRange);
    if (pFileLock->GetRange()->GetStart() < m_pRange->GetStart()) {
        m_pRange->SetStart(pFileLock->GetRange()->GetStart());
    }
    if (pFileLock->GetRange()->GetFinish() > m_pRange->GetFinish()) {
        m_pRange->SetFinish(pFileLock->GetRange()->GetFinish());
    }

    // find position of insertion

    if (this->IsEmpty()) {
        m_pListHead = pFileLock;
        return;
    }

    pBefore = m_pListHead;
    pAfter = pBefore;

    while (NULL != pAfter) {
        PREFAST_DEBUGCHK(NULL != pAfter->GetRange());
        if (pFileLock->GetRange()->GetStart() < pAfter->GetRange()->GetStart()) {
            break;
        }
        pBefore = pAfter;
        pAfter = pAfter->GetNext();
    }

    // insert lock

    // is lock to be inserted before first lock in list?
    if (pAfter == m_pListHead) {
        pFileLock->SetNext(m_pListHead);
        m_pListHead = pFileLock;
    }
    else {
        pBefore->SetNext(pFileLock);
        pFileLock->SetNext(pAfter);
    }
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

CFileLock *CFileLockList::Remove(CRange *pRange)
{
    CFileLock *pBefore;
    CFileLock *pAfter;
    BOOL fFound = FALSE;

    PREFAST_DEBUGCHK(NULL != pRange);

    if (this->IsEmpty()) {
        return NULL;
    }

    // find lock to remove

    pBefore = m_pListHead;
    pAfter = pBefore;

    while (NULL != pAfter) {
        PREFAST_DEBUGCHK(NULL != pAfter->GetRange());
        if (pAfter->GetRange()->IsEqualTo(pRange)) {
            fFound = TRUE;
            break;
        }
        pBefore = pAfter;
        pAfter = pAfter->GetNext();
    }

    // was lock found?

    if (!fFound) {
        return NULL;
    }

    // remove lock

    if (pAfter == m_pListHead) {
        m_pListHead = pAfter->GetNext();
    }
    else {
        pBefore->SetNext(pAfter->GetNext());
    }

    // update range of list

    PREFAST_DEBUGCHK(NULL != m_pRange);
    if (!this->IsEmpty()) {
        PREFAST_DEBUGCHK(NULL != pAfter->GetRange());
        if (m_pRange->GetStart() == pAfter->GetRange()->GetStart()) {
            m_pRange->SetStart(m_pListHead->GetRange()->GetStart());
        }
        if (m_pRange->GetFinish() == pAfter->GetRange()->GetFinish()) {
            pBefore = this->GetLast();
            PREFAST_DEBUGCHK(NULL != pBefore);
            PREFAST_DEBUGCHK(NULL != pBefore->GetRange());
            m_pRange->SetFinish(pBefore->GetRange()->GetFinish());
        }
    }
    else {
        m_pRange->SetStart(CRANGE_MAX_START);
        m_pRange->SetFinish(CRANGE_MIN_FINISH);
    }

    // isolate removed lock

    pAfter->SetNext(NULL);

    // return removed lock

    return pAfter;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL CFileLockList::IsValid()
{
    BOOL fResult;
    CFileLock *pFileLock;

    if (0 == m_dwOwner) {
        fResult = FALSE;
        goto exit;
    }
    if (NULL == m_pRange) {
        fResult = FALSE;
        goto exit;
    }

    // is list range valid?

    if (!m_pRange->IsValid()) {
        fResult = FALSE;
        goto exit;
    }

    // is each lock in list valid?

    pFileLock = m_pListHead;
    while (NULL != pFileLock) {
        if (!pFileLock->IsValid()) {
            fResult = FALSE;
            goto exit;
        }
        pFileLock = pFileLock->GetNext();
    }

    fResult = TRUE;

exit:;
    return fResult;
}

