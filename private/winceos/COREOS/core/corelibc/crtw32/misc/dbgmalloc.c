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
*dbgmalloc.c - Debug CRT Heap Function of malloc()
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines debug versions of heap function malloc().
*
*Revision History:
*       02-13-07  WL    Module created. Moved from dbgheap.c
*
*******************************************************************************/

#ifdef  _DEBUG

#include <windows.h>
#include <winheap.h>
#include <ctype.h>
#include <dbgint.h>
#include <crtdbg.h>
#include <rtcsup.h>
#include <internal.h>
#include <limits.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <locale.h>
#include <mtdll.h>
#include <setlocal.h>

#pragma warning(disable:4390)


/***
*void *malloc() - Get a block of memory from the debug heap
*
*Purpose:
*       Allocate of block of memory of at least size bytes from the heap and
*       return a pointer to it.
*
*       Allocates 'normal' memory block.
*
*Entry:
*       size_t          nSize       - size of block requested
*
*Exit:
*       Success:  Pointer to memory block
*       Failure:  NULL (or some error value)
*
*Exceptions:
*
*******************************************************************************/

extern "C" _CRTIMP void * __cdecl malloc (
        size_t nSize
        )
{
        void *res = _nh_malloc_dbg(nSize, _newmode, _NORMAL_BLOCK, NULL, 0);

        RTCCALLBACK(_RTC_Allocate_hook, (res, nSize, 0));

        return res;
}

#endif  /* _DEBUG */
