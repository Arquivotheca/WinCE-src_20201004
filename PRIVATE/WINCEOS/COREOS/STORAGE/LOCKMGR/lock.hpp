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
