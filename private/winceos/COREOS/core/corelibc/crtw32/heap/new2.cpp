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
*new2.cpp - defines C++ new routine
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
*       11-04-98  JWM   Split off from new.cpp for better granularity.
*       05-12-99  PML   Win64 fix: unsigned -> size_t
*       05-26-99  KBF   Updated RTC hook func params
*       01-28-05  AJS   Call __clrcall version of new for /clr:pure. VSW#416354.
*
*******************************************************************************/


#include <cruntime.h>
#include <malloc.h>
#include <new.h>
#include <stdlib.h>
#include <winheap.h>
#include <rtcsup.h>

void *__CRTDECL operator new(size_t) /*_THROW1(std::bad_alloc)*/;

void * operator new[]( size_t cb )
{
    void *res = operator new(cb);

    RTCCALLBACK(_RTC_Allocate_hook, (res, cb, 0));

    return res;
}
