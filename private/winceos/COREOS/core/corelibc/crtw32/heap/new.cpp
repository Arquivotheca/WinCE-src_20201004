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
*new.cxx - defines C++ new routine
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Defines C++ new routine.
*
*Revision History:
*       05-07-90  WAJ   Initial version.
*       08-30-90  WAJ   new now takes unsigned ints.
*       08-08-91  JCR   call _halloc/_hfree, not halloc/hfree
*       08-13-91  KRS   Change new.hxx to new.h.  Fix copyright.
*       08-13-91  JCR   ANSI-compatible _set_new_handler names
*       10-30-91  JCR   Split new, delete, and handler into seperate sources
*       11-13-91  JCR   32-bit version
*       02-16-94  SKS   Move set_new_handler functionality here from malloc()
*       03-01-94  SKS   _pnhHeap must be declared in malloc.c, not here
*       03-03-94  SKS   New handler functionality moved to malloc.c but under
*                       a new name, _nh_malloc().
*       02-01-95  GJF   #ifdef out the change above for the Mac (temporary).
*       05-01-95  GJF   Spliced on winheap version.
*       05-08-95  CFW   Turn on new handling for Mac.
*       05-22-98  JWM   Support for KFrei's RTC work, and operator new[].
*       07-28-98  JWM   RTC update.
*       11-04-98  JWM   Split off new[] to new2.cpp for better granularity.
*       12-18-98  GJF   Changes for 64-bit size_t.
*       05-26-99  KBF   Updated RTC hook func params
*       02-20-02  BWT   Reduce the call frame by calling heap_alloc directly (better DW traces)
*       03-25-03  SJ    VSWhidbey#48185 - The HEAP_MAXREQ check is unneccessary,
*                       HeapAlloc will fail & return an error.
*       11-04-04  JL    Make operator new and delete as a part of pure CRT
*
*******************************************************************************/


#ifdef _SYSCRT
#include <cruntime.h>
#include <crtdbg.h>
#include <malloc.h>
#include <new.h>
#include <stdlib.h>
#include <winheap.h>
#include <rtcsup.h>
#include <internal.h>

void * operator new( size_t cb )
{
    void *res;

    for (;;) {

        //  allocate memory block
        res = _heap_alloc(cb);

        //  if successful allocation, return pointer to memory

        if (res)
            break;

        //  call installed new handler
        if (!_callnewh(cb))
            break;

        //  new handler was successful -- try to allocate again
    }

    RTCCALLBACK(_RTC_Allocate_hook, (res, cb, 0));

    return res;
}
#else

#include <cstdlib>
#include <new>

_C_LIB_DECL
int __cdecl _callnewh(size_t size) _THROW1(_STD bad_alloc);
_END_C_LIB_DECL

void *__CRTDECL operator new(size_t size) _THROW1(_STD bad_alloc)
	{	// try to allocate size bytes
	void *p;
	while ((p = malloc(size)) == 0)
		if (_callnewh(size) == 0)
		{	// report no memory
			_THROW_NCEE(_XSTD bad_alloc, );
		}

	return (p);
	}

/*
 * Copyright (c) 1992-2002 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
 V3.13:0009 */
#endif
