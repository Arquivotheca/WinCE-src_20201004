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

