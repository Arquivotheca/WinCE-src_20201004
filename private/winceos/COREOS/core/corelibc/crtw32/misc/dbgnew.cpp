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
*dbgnew.cpp - defines C++ new() routines, debug version
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Defines C++ new() routines.
*
*Revision History:
*       01-12-95  CFW   Initial version.
*       01-19-95  CFW   Need oscalls.h to get DebugBreak definition.
*       01-20-95  CFW   Change unsigned chars to chars.
*       01-23-95  CFW   Delete must check for NULL.
*       02-10-95  CFW   _MAC_ -> _MAC.
*       03-16-95  CFW   delete() only for normal, ignore blocks.
*       03-21-95  CFW   Add _delete_crt, _delete_client.
*       03-21-95  CFW   Remove _delete_crt, _delete_client.
*       06-27-95  CFW   Add win32s support for debug libs.
*       12-28-95  JWM   Split off delete() for granularity.
*       05-22-98  JWM   Support for KFrei's RTC work, and operator new[].
*       07-28-98  JWM   RTC update.
*       01-05-99  GJF   Changes for 64-bit size_t.
*       05-26-99  KBF   Updated RTC_Allocate_hook params
*       01-16-06  AC    The debug version of operator new and operator[] throws std::bad_alloc.
*                       Also added in retail libraries the implementation of the debug operator new
*                       and operator new[], so that they can be easily overriden by nothrownew.obj
*                       VSW#572834
*
*******************************************************************************/

#ifdef _DEBUG

#include <cruntime.h>
#include <malloc.h>
#include <mtdll.h>
#include <dbgint.h>
#include <rtcsup.h>
#include <new>

/***
*void * operator new() - Get a block of memory from the debug heap
*
*Purpose:
*       Allocate of block of memory of at least size bytes from the heap and
*       return a pointer to it.
*
*       Allocates any type of supported memory block.
*
*Entry:
*       size_t          cb          - count of bytes requested
*       int             nBlockUse   - block type
*       char *          szFileName  - file name
*       int             nLine       - line number
*
*Exit:
*       Success:  Pointer to memory block
*       Failure:  throws std::bad_alloc
*
*Exceptions:
*       Failure:  throws std::bad_alloc
*
*******************************************************************************/
void *__CRTDECL operator new(
        size_t cb,
        int nBlockUse,
        const char * szFileName,
        int nLine
        )
        _THROW1(_STD bad_alloc)
{
    /* _nh_malloc_dbg already calls _heap_alloc_dbg in a loop and calls _callnewh
       if the allocation fails. If _callnewh returns (very likely because no
       new handlers have been installed by the user), _nh_malloc_dbg returns NULL.
     */
    void *res = _nh_malloc_dbg( cb, 1, nBlockUse, szFileName, nLine );

    RTCCALLBACK(_RTC_Allocate_hook, (res, cb, 0));

    /* if the allocation fails, we throw std::bad_alloc */
    if (res == 0)
    {
        _THROW_NCEE(_XSTD bad_alloc, );
    }

    return res;
}

/***
*void * operator new() - Get a block of memory from the debug heap
*
*Purpose:
*       Allocate of block of memory of at least size bytes from the heap and
*       return a pointer to it.
*
*       Allocates any type of supported memory block.
*
*Entry:
*       size_t          cb          - count of bytes requested
*       int             nBlockUse   - block type
*       char *          szFileName  - file name
*       int             nLine       - line number
*
*Exit:
*       Success:  Pointer to memory block
*       Failure:  NULL (or some error value)
*
*Exceptions:
*
*******************************************************************************/
void *__CRTDECL operator new[](
        size_t cb,
        int nBlockUse,
        const char * szFileName,
        int nLine
        )
        _THROW1(_STD bad_alloc)
{
    void *res = operator new(cb, nBlockUse, szFileName, nLine );

    RTCCALLBACK(_RTC_Allocate_hook, (res, cb, 0));

    return res;
}

#else /* _DEBUG */

#include <new>

/* Implement debug operator new and operator new[] in retail libraries as well,
 * simply calling the retail operator new.
 * These debug versions are declared only in crtdbg.h.
 * Implementing there operators in the CRT libraries makes it easier to override them
 * linking to nothrownew.obj.
 */

void *__CRTDECL operator new(
        size_t cb,
        int nBlockUse,
        const char * szFileName,
        int nLine
        )
        _THROW1(_STD bad_alloc)
{
    (nBlockUse);
    (szFileName);
    (nLine);
    return operator new(cb);
}

void *__CRTDECL operator new[](
        size_t cb,
        int nBlockUse,
        const char * szFileName,
        int nLine
        )
        _THROW1(_STD bad_alloc)
{
    (nBlockUse);
    (szFileName);
    (nLine);
    return operator new[](cb);
}

#endif /* _DEBUG */
