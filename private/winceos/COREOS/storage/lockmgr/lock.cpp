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

