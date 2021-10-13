//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

/*++

Module Name:
    range.cpp

Abstract:
    Range Abstraction.  A range is a 64-bit 2-tuple.

Revision History:

--*/

#include "range.hpp"

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

CRange::CRange()
{
    m_ullStart = CRANGE_MAX_START;
    m_ullFinish = CRANGE_MIN_FINISH;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

VOID CRange::SetOffset(DWORD dwOffsetLow, DWORD dwOffsetHigh)
{
    m_ullStart = (ULONGLONG)dwOffsetLow;
    m_ullStart |= (((ULONGLONG)dwOffsetHigh) << 32);
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

VOID CRange::SetLength(DWORD dwLengthLow, DWORD dwLengthHigh)
{
    ULONGLONG ullLength;

    ullLength = (ULONGLONG)dwLengthLow;
    ullLength |= (((ULONGLONG)dwLengthHigh) << 32);

    m_ullFinish = (m_ullStart + ullLength) - 1;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

RANGECONFLICTTYPE CRange::IsConflict(CRange *pRange)
{
    PREFAST_DEBUGCHK(NULL != pRange);

    // test for no conflict
    if (m_ullFinish < pRange->GetStart()) {
        return RCT_NOCONFLICT_BEFORE;
    }
    if (m_ullStart > pRange->GetFinish()) {
        return RCT_NOCONFLICT_AFTER;
    }
    // test for conflict
    if ((pRange->GetStart() > m_ullStart) && (pRange->GetFinish() < m_ullFinish)) {
        return RCT_BETWEEN;
    }
    return RCT_BOUNDARY;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL CRange::IsEqualTo(CRange *pRange)
{
    PREFAST_DEBUGCHK(NULL != pRange);

    if ((this->GetStart() == pRange->GetStart()) && (this->GetFinish() == pRange->GetFinish())) {
        return TRUE;
    }
    return FALSE;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL CRange::IsValid()
{
    if (m_ullStart > m_ullFinish) {
        return FALSE;
    }
    return TRUE;
}

