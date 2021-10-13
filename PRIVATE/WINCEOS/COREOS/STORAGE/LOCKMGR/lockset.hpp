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
    lockset.hpp

Abstract:
    Lock Set Abstraction.  A lock set is a set of lock lists and a global
    set range.  A lock set is sorted by the lowest byte locked by a list.  The
    set range is defined as the lowest byte locked by a list in the set and the
    highest byte locked by a list in the set.

Revision History:

--*/

#ifndef __LOCKSET_HPP_
#define __LOCKSET_HPP_

#include "windows.h"
#include "lockmgrdbg.hpp"
#include "range.hpp"
#include "locklist.hpp"

class CFileLockSet {
    CRange *m_pRange;
    CFileLockList *m_pSetHead;
  public:
    CFileLockSet();
    ~CFileLockSet();
    CFileLockList *GetLast();
    BOOL IsEmpty();
    BOOL IsConflict(CRange *pRange);
    BOOL IsConflict(CRange *pRange, DWORD dwOwnerOfListToIgnore);
    VOID Insert(CFileLockList *pFileLockList);
    CFileLockList *Remove(DWORD dwOwner);
    BOOL IsValid();
};

#endif // __LOCKSET_HPP_

