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

