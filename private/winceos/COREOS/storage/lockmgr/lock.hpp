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
    lock.hpp

Abstract:
    Lock Abstraction.  A lock a range.

Revision History:

--*/

#ifndef __LOCK_HPP_
#define __LOCK_HPP_

#include <windows.h>
#include "lockmgrdbg.hpp"
#include "range.hpp"

class CFileLock;
class CFileLock {
    CRange *m_pRange;
    CFileLock *m_pNextLock;
  public:
    CFileLock();
    ~CFileLock();
    inline CRange *GetRange();
    inline VOID SetRange(CRange *pRange);
    inline CFileLock *GetNext();
    inline VOID SetNext(CFileLock *pFileLock);
    BOOL IsValid();
};

inline CRange *CFileLock::GetRange() {return m_pRange;}
inline VOID CFileLock::SetRange(CRange *pRange) {m_pRange = pRange;}
inline CFileLock *CFileLock::GetNext() {return m_pNextLock;}
inline VOID CFileLock::SetNext(CFileLock *pFileLock) {m_pNextLock = pFileLock;}

#endif // __LOCK_HPP_
