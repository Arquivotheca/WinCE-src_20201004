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
    locklist.hpp

Abstract:
    Lock List Abstraction.  A lock list is a set of locks a global list range.
    A lock list is sorted by the lowest byte locked by a lock.  The list range
    is defined as the lowest byte locked by a lock in the list and the highest
    byte locked by a lock in the list.

Revision History:

--*/

#ifndef __LOCKLIST_HPP_
#define __LOCKLIST_HPP_

#include <windows.h>
#include "lockmgrdbg.hpp"
#include "range.hpp"
#include "lock.hpp"

class CFileLockList;
class CFileLockList {
    DWORD m_dwOwner;
    CRange *m_pRange;
    CFileLock *m_pListHead;
    CFileLockList *m_pNextList;
  public:
    CFileLockList();
    ~CFileLockList();
    inline DWORD GetOwner();
    inline VOID SetOwner(DWORD dwOwner);
    inline CRange *GetRange();
    inline VOID SetRange(CRange *pRange);
    inline CFileLockList *GetNext();
    inline VOID SetNext(CFileLockList *pFileLockList);
    CFileLock *GetLast();
    BOOL IsEmpty();
    BOOL IsConflict(CRange *pRange);
    VOID Insert(CFileLock *pFileLock);
    CFileLock *Remove(CRange *pRange);
    BOOL IsValid();
};

inline DWORD CFileLockList::GetOwner() {return m_dwOwner;}
inline VOID CFileLockList::SetOwner(DWORD dwOwner) {m_dwOwner = dwOwner;}
inline CRange *CFileLockList::GetRange() {return m_pRange;}
inline VOID CFileLockList::SetRange(CRange *pRange) {m_pRange = pRange;}
inline CFileLockList *CFileLockList::GetNext() {return m_pNextList;}
inline VOID CFileLockList::SetNext(CFileLockList *pFileLockList) {m_pNextList = pFileLockList;}

#endif // __LOCKLIST_HPP_

