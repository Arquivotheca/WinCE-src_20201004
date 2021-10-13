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
*dbghook.c - Debug CRT Hook Functions
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Allow users to override default alloc hook at link time.
*
*Revision History:
*       11-28-94  CFW   Module created.
*       12-14-94  CFW   Remove incorrect comments.
*       01-09-94  CFW   Filename pointers are const.
*       02-09-95  CFW   PMac work.
*       06-27-95  CFW   Add win32s support for debug libs.
*       05-13-99  PML   Remove Win32s
*       11-03-04  AC    Added __crt_debugger_hook.
*
*******************************************************************************/

#include <dbgint.h>

#ifdef _DEBUG

#include <internal.h>
#include <limits.h>
#include <mtdll.h>
#include <malloc.h>
#include <stdlib.h>

_CRT_ALLOC_HOOK _pfnAllocHook = _CrtDefaultAllocHook;

/***
*int _CrtDefaultAllocHook() - allow allocation
*
*Purpose:
*       allow allocation
*
*Entry:
*       all parameters ignored
*
*Exit:
*       returns TRUE
*
*Exceptions:
*
*******************************************************************************/
int __cdecl _CrtDefaultAllocHook(
        int nAllocType,
        void * pvData,
        size_t nSize,
        int nBlockUse,
        long lRequest,
        const unsigned char * szFileName,
        int nLine
        )
{
        return 1; /* allow all allocs/reallocs/frees */
}

#endif /* _DEBUG */

int _debugger_hook_dummy;

__declspec(noinline)
void __cdecl _CRT_DEBUGGER_HOOK(int _Reserved)
{
    /* assign 0 to _debugger_hook_dummy so that the function is not folded in retail */
    (_Reserved);
    _debugger_hook_dummy = 0;
}
