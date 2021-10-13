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
    lckmgrapi.hpp

Abstract:
    Core Lock Manager API.  This module is used by the Lock Manager API for
    FSDs/File Systems.

Revision History:

--*/

#ifndef __LCKMGRAPI_HPP_
#define __LCKMGRAPI_HPP_

#include <windows.h>

//
// LXX_CreateLockContainer - Allocate lock container
//

PVOID
LXX_CreateLockContainer(
    );

//
// LXX_DestroyLockContainer - Deallocate lock container
//

VOID
LXX_DestroyLockContainer(
    PVOID pvLockContainer
    );

//
// LXX_IsLockContainerEmpty - Determine whether lock container is empty
//

BOOL
LXX_IsLockContainerEmpty(
    PVOID pvLockContainer
    );

//
// LXX_Lock - Add lock to lock container
//

typedef enum _LOCKRESULT {
    LR_ERROR = 0,
    LR_SUCCESS,
    LR_CONFLICT
} LOCKRESULT, *PLOCKRESULT;

LOCKRESULT
LXX_Lock(
    PVOID pvLockContainer,
    DWORD dwFile,
    DWORD dwFlags,
    DWORD dwReserved,
    DWORD nNumberOfBytesToLockLow,
    DWORD nNumberOfBytesToLockHigh,
    LPOVERLAPPED lpOverlapped
    );

//
// LXX_Unlock - Remove lock from lock container
//

BOOL
LXX_Unlock(
    PVOID pvLockContainer,
    DWORD dwFile,
    DWORD dwReserved,
    DWORD nNumberOfBytesToUnlockLow,
    DWORD nNumberOfBytesToUnlockHigh,
    LPOVERLAPPED lpOverlapped
    );

//
// LXX_UnlockLocksOwnedByHandle - Remove all locks from lock container owned by target handle
//

BOOL
LXX_UnlockLocksOwnedByHandle(
    PVOID pvLockContainer,
    DWORD dwFile
    );

//
// LXX_AuthorizeAccess - Determine whether target access is permissible
//

BOOL
LXX_AuthorizeAccess(
    PVOID pvLockContainer,
    DWORD dwFile,
    BOOL fRead,
    DWORD dwFilePositionLow,
    DWORD dwFilePositionHigh,
    DWORD nBytesToBeAccessed
    );

#endif // __LCKMGRAPI_HPP_

