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
    lockcol.cpp

Abstract:
    Lock Collection Abstraction.  A lock container is implemented as a lock
    collection.  A lock collection consists of the exclusive lock set and the
    shared lock set.

Revision History:

--*/

#include "lockcol.hpp"

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

CFileLockCollection::CFileLockCollection()
{
    m_dwSignature = SIG;
    m_pExclusiveSet = NULL;
    m_pSharedSet = NULL;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

CFileLockCollection::~CFileLockCollection()
{
    if (NULL != m_pExclusiveSet) {
        delete m_pExclusiveSet;
    }
    if (NULL != m_pSharedSet) {
        delete m_pSharedSet;
    }
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL CFileLockCollection::IsEmpty()
{
    if ((NULL != m_pExclusiveSet && !m_pExclusiveSet->IsEmpty())) {
        return FALSE;
    }
    if ((NULL != m_pSharedSet) && (!m_pSharedSet->IsEmpty())) {
        return FALSE;
    }
    return TRUE;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL CFileLockCollection::IsValid()
{
    BOOL fResult;

    if (NULL != m_pExclusiveSet) {
        if (!m_pExclusiveSet->IsValid()) {
            fResult = FALSE;
            goto exit;
        }
    }

    if (NULL != m_pSharedSet) {
        if (!m_pSharedSet->IsValid()) {
            fResult = FALSE;
            goto exit;
        }
    }

    fResult = TRUE;

exit:;
    return fResult;
}

