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

