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
    lockcol.hpp

Abstract:
    Lock Collection Abstraction.  A lock container is implemented as a lock
    collection.  A lock collection consists of the exclusive lock set and the
    shared lock set.

Revision History:

--*/

#ifndef __LOCKCOL_HPP_
#define __LOCKCOL_HPP_

#include <windows.h>
#include "lockmgrdbg.hpp"
#include "lockset.hpp"

#define SIG 0xDEADBEEF

class CFileLockCollection {
    DWORD m_dwSignature;
    CFileLockSet *m_pExclusiveSet;
    CFileLockSet *m_pSharedSet;
  public:
    CFileLockCollection();
    ~CFileLockCollection();
    inline DWORD GetSignature();
    inline CFileLockSet *GetExclusive();
    inline VOID SetExclusive(CFileLockSet *pFileLockSet);
    inline CFileLockSet *GetShared();
    inline VOID SetShared(CFileLockSet *pFileLockSet);
    BOOL IsEmpty();
    BOOL IsValid();
};

inline DWORD CFileLockCollection::GetSignature() {return m_dwSignature;}
inline CFileLockSet *CFileLockCollection::GetExclusive() {return m_pExclusiveSet;}
inline VOID CFileLockCollection::SetExclusive(CFileLockSet *pFileLockSet) {m_pExclusiveSet = pFileLockSet;}
inline CFileLockSet *CFileLockCollection::GetShared() {return m_pSharedSet;}
inline VOID CFileLockCollection::SetShared(CFileLockSet *pFileLockSet) {m_pSharedSet = pFileLockSet;}

#endif // __LOCKCOL_HPP_

