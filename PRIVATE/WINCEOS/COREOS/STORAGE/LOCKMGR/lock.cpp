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
    lock.cpp

Abstract:
    Lock Abstraction.  A lock a range.

Revision History:

--*/

#include "lock.hpp"

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

CFileLock::CFileLock()
{
    m_pRange = NULL;
    m_pNextLock = NULL;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

CFileLock::~CFileLock()
{
    DEBUGCHK(NULL == m_pNextLock);

    if (NULL != m_pRange) {
        delete m_pRange;
    }
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL CFileLock::IsValid()
{
    DEBUGCHK(NULL != m_pRange);

    return ((m_pRange->IsValid()) ? TRUE : FALSE);
}

