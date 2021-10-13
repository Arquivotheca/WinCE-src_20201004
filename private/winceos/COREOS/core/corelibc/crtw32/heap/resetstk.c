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
/***
*resetstk.c - Recover from Stack overflow.
*
*       Copyright (c) 1989-2001, Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines the _resetstkoflw() function.
*
*Revision History:
*       12-10-99  GB    Module Created
*       04-17-01  PML   Enable for Win9x, return success code (vs7#239962)
*       06-04-01  PML   Do nothing if guard page not missing, don't shrink
*                       committed space (vs7#264306)
*       04-25-02  PML   Don't set guard page below pMinGuard (vs7#530044)
*       11-11-03  AJS   Updated code from NT sources (vswhidbey#191097)
*       03-13-04  MSL   Let prefast know our use of _alloca here is safe
*       10-21-04  AC    Replace #pragma prefast with #pragma warning
*                       VSW#368280
*       11-08-06  PMB   Remove support for Win9x and _osplatform and relatives.
*                       DDB#11325
*
*******************************************************************************/

#include <stdlib.h>
#include <malloc.h>
#include <windows.h>
#include <internal.h>

#define MIN_STACK_REQ_WINNT 2

/***
* void _resetstkoflw(void) - Recovers from Stack Overflow
*
* Purpose:
*       Sets the guard page to its position before the stack overflow.
*
* Exit:
*       Returns nonzero on success, zero on failure
*
*******************************************************************************/

int __cdecl _resetstkoflw(void)
{
#ifdef _WIN32_WCE

    CeResetStack();

#else
    LPBYTE pStack, pStackBase, pMaxGuard, pMinGuard;
    MEMORY_BASIC_INFORMATION mbi;
    SYSTEM_INFO si;
    DWORD PageSize;
    DWORD RegionSize;
    DWORD flOldProtect;
    ULONG StackSizeInBytes;

    // Use _alloca() to get the current stack pointer
#pragma warning(push)
#pragma warning(disable:6255)
    // prefast(6255): This alloca is safe and we do not want a __try here
    pStack = (LPBYTE)_alloca(1);
#pragma warning(pop)

    // Find the base of the stack.

    if (VirtualQuery(pStack, &mbi, sizeof mbi) == 0) {
        return 0;
    }

    pStackBase = (LPBYTE)mbi.AllocationBase;

    GetSystemInfo(&si);
    PageSize = si.dwPageSize;
    RegionSize = 0;
    StackSizeInBytes = 0;               // Indicate just querying
#ifdef _WIN32_WCE
    StackSizeInBytes = 0x10000; // assume 64K 
#else
    if (SetThreadStackGuarantee(&StackSizeInBytes) && StackSizeInBytes > 0) {
        RegionSize = StackSizeInBytes;
    }
#endif // _WIN32_WCE

#pragma warning(push)
#pragma warning(disable:6255)
    // Silence prefast about overflow/underflow
    RegionSize = (RegionSize + PageSize - 1) & ~(PageSize - 1);
#pragma warning(pop)

    //
    // If there is a stack guarantee (RegionSize nonzero), then increase
    // our guard page size by 1 so that even a subsequent fault that occurs
    // midway (instead of at the beginning) through the first guard page
    // will have the extra page to preserve the guarantee.
    //

    if (RegionSize != 0) {
        RegionSize += PageSize;
    }

    if (RegionSize < MIN_STACK_REQ_WINNT * PageSize) {
        RegionSize = MIN_STACK_REQ_WINNT * PageSize;
    }

    //
    // Find the page(s) just below where the stack pointer currently points.
    // This is the highest potential guard page.
    //

    pMaxGuard = (LPBYTE) (((DWORD_PTR)pStack & ~(DWORD_PTR)(PageSize - 1))
                       - RegionSize);

    //
    // If the potential guard page is too close to the start of the stack
    // region, abandon the reset effort for lack of space.  Win9x has a
    // larger reserved stack requirement.
    //

    pMinGuard = pStackBase + PageSize;

    if (pMaxGuard < pMinGuard) {
        return 0;
    }

    // Set the new guard page just below the current stack page.

    if (VirtualAlloc (pMaxGuard, RegionSize, MEM_COMMIT, PAGE_READWRITE) == NULL ||
        VirtualProtect (pMaxGuard, RegionSize, PAGE_READWRITE | PAGE_GUARD, &flOldProtect) == 0) {
        return 0;
    }

#endif // _WIN32_WCE

    return 1;
}
