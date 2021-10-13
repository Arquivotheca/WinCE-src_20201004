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
    Lock Set Abstraction.  A lock set is a set of lock lists and a global
    set range.  The set range is defined as the lowest byte locked by a list
    in the set and the highest byte locked by a list in the set.

Revision History:

--*/

#include "lockset.hpp"
#include "locklist.hpp"
#include "lock.hpp"
#include "range.hpp"

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

CFileLockSet::CFileLockSet()
{
    m_pRange = new CRange();
    m_pSetHead = NULL;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

CFileLockSet::~CFileLockSet()
{
    CFileLockList *pListToDelete;
    CFileLockList *pNextList;

    if (NULL != m_pRange) {
        delete m_pRange;
    }

    // delete set

    pNextList = m_pSetHead;
    while (NULL != pNextList) {
        pListToDelete = pNextList;
        pNextList = pListToDelete->GetNext();
        pListToDelete->SetNext(NULL);
        delete pListToDelete;
    }
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

CFileLockList *CFileLockSet::GetLast()
{
    CFileLockList *pCurList;
    CFileLockList *pMax;
    ULONGLONG ullFurthestByte = 0;

    pCurList = m_pSetHead;
    pMax = pCurList;

    // find last list in set

    while (NULL != pCurList) {
        PREFAST_DEBUGCHK(NULL != pCurList->GetRange());
        if (ullFurthestByte < pCurList->GetRange()->GetFinish()) {
            ullFurthestByte = pCurList->GetRange()->GetFinish();
            pMax = pCurList;
        }
        pCurList = pCurList->GetNext();
    }

    return pMax;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL CFileLockSet::IsEmpty()
{
    return (NULL == m_pSetHead) ? TRUE : FALSE;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL CFileLockSet::IsConflict(CRange *pRange)
{
    CFileLockList *pCurList;

    PREFAST_DEBUGCHK(NULL != pRange);

    if (this->IsEmpty()) {
        return FALSE;
    }

    PREFAST_DEBUGCHK(NULL != m_pRange);
    switch (m_pRange->IsConflict(pRange)) {
    case RCT_NOCONFLICT_BEFORE:
    case RCT_NOCONFLICT_AFTER:
        return FALSE;
    case RCT_BOUNDARY:
        return TRUE;
    }

    // target lies within range of set

    pCurList = m_pSetHead;
    while (NULL != pCurList) {
        if (pCurList->IsConflict(pRange)) {
            return TRUE;
        }
        pCurList = pCurList->GetNext();
    }

    // target range lies after all lists in set

    return FALSE;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL CFileLockSet::IsConflict(CRange *pRange, DWORD dwOwnerOfListToIgnore)
{
    CFileLockList *pCurList;

    PREFAST_DEBUGCHK(NULL != pRange);
    PREFAST_DEBUGCHK(0 != dwOwnerOfListToIgnore);

    if (this->IsEmpty()) {
        return FALSE;
    }

    // ignore RCT_BOUNDARY; dwOwnerOfListToIgnore may own a boundary

    PREFAST_DEBUGCHK(NULL != m_pRange);
    switch (m_pRange->IsConflict(pRange)) {
    case RCT_NOCONFLICT_BEFORE:
    case RCT_NOCONFLICT_AFTER:
        return FALSE;
    }

    // target lies within range of set

    pCurList = m_pSetHead;
    while (NULL != pCurList) {
        if (pCurList->GetOwner() != dwOwnerOfListToIgnore) {
            if (pCurList->IsConflict(pRange)) {
                return TRUE;
            }
        }
        pCurList = pCurList->GetNext();
    }

    // target range lies after all lists in set

    return FALSE;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

VOID CFileLockSet::Insert(CFileLockList *pFileLockList)
{
    CFileLockList *pBefore;
    CFileLockList *pAfter;

    PREFAST_DEBUGCHK(NULL != pFileLockList);
    PREFAST_DEBUGCHK(NULL != pFileLockList->GetRange());
    PREFAST_DEBUGCHK(NULL == pFileLockList->GetNext());

    // update range of set

    PREFAST_DEBUGCHK(NULL != m_pRange);
    if (pFileLockList->GetRange()->GetStart() < m_pRange->GetStart()) {
        m_pRange->SetStart(pFileLockList->GetRange()->GetStart());
    }
    if (pFileLockList->GetRange()->GetFinish() > m_pRange->GetFinish()) {
        m_pRange->SetFinish(pFileLockList->GetRange()->GetFinish());
    }

    // find position of insertion

    if (this->IsEmpty()) {
        m_pSetHead = pFileLockList;
        return;
    }

    pBefore = m_pSetHead;
    pAfter = pBefore;

    while (NULL != pAfter) {
        PREFAST_DEBUGCHK(NULL != pAfter->GetRange());
        if (pFileLockList->GetRange()->GetStart() < pAfter->GetRange()->GetStart()) {
            break;
        }
        pBefore = pAfter;
        pAfter = pBefore->GetNext();
    }

    // insert list

    // is list to be inserted before first list in set?
    if (pAfter == m_pSetHead) {
        pFileLockList->SetNext(m_pSetHead);
        m_pSetHead = pFileLockList;
    }
    else {
        pBefore->SetNext(pFileLockList);
        pFileLockList->SetNext(pAfter);
    }
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

CFileLockList *CFileLockSet::Remove(DWORD dwOwner)
{
    CFileLockList *pBefore;
    CFileLockList *pAfter;
    BOOL fFound = FALSE;

    PREFAST_DEBUGCHK(0 != dwOwner);

    if (this->IsEmpty()) {
        return NULL;
    }

    // find list to remove

    pBefore = m_pSetHead;
    pAfter = pBefore;

    while (NULL != pAfter) {
        if (dwOwner == pAfter->GetOwner()) {
            fFound = TRUE;
            break;
        }
        pBefore = pAfter;
        pAfter = pAfter->GetNext();
    }

    // was list found?

    if (!fFound) {
        return NULL;
    }

    // remove list

    if (pAfter == m_pSetHead) {
        m_pSetHead = pAfter->GetNext();
    }
    else {
        pBefore->SetNext(pAfter->GetNext());
    }

    // update range of set

    PREFAST_DEBUGCHK(NULL != m_pRange);
    if (!this->IsEmpty()) {
        PREFAST_DEBUGCHK(NULL != pAfter->GetRange());
        if (m_pRange->GetStart() == pAfter->GetRange()->GetStart()) {
            m_pRange->SetStart(m_pSetHead->GetRange()->GetStart());
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

    // isolate removed list

    pAfter->SetNext(NULL);

    // return removed list

    return pAfter;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL CFileLockSet::IsValid()
{
    BOOL fResult;
    CFileLockList *pFileLockList;

    if (NULL == m_pRange) {
        fResult = FALSE;
        goto exit;
    }

    // is set range valid?

    if (!m_pRange->IsValid()) {
        fResult = FALSE;
        goto exit;
    }

    // is each list in set valid?

    pFileLockList = m_pSetHead;
    while (NULL != pFileLockList) {
        if (!pFileLockList->IsValid()) {
            fResult = FALSE;
            goto exit;
        }
        pFileLockList = pFileLockList->GetNext();
    }

    fResult = TRUE;

exit:;
    return fResult;
}

