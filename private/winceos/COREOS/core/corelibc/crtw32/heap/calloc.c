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
*calloc.c - allocate storage for an array from the heap
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines the calloc() function.
*
*Revision History:
*       07-25-89  GJF   Module created
*       11-13-89  GJF   Added MTHREAD support. Also fixed copyright and got
*                       rid of DEBUG286 stuff.
*       12-04-89  GJF   Renamed header file (now heap.h). Added register
*                       declarations
*       12-18-89  GJF   Added explicit _cdecl to function definition
*       03-09-90  GJF   Replaced _cdecl with _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       09-27-90  GJF   New-style function declarator.
*       05-28-91  GJF   Tuned a bit.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       11-03-94  CFW   Debug heap support.
*       12-01-94  CFW   Use malloc with new handler, remove locks.
*       02-01-95  GJF   #ifdef out the *_base names for the Mac builds
*                       (temporary).
*       02-09-95  GJF   Restored *_base names.
*       04-28-95  GJF   Spliced on winheap version.
*       05-24-95  CFW   Official ANSI C++ new handler added.
*       03-04-96  GJF   Added support for small-block heap.
*       05-22-97  RDK   New small-block heap scheme implemented.
*       09-26-97  BWT   Fix POSIX
*       11-05-97  GJF   Small POSIX fixes from Roger Lanser.
*       12-17-97  GJF   Exception-safe locking.
*       05-22-98  JWM   Support for KFrei's RTC work.
*       07-28-98  JWM   RTC update.
*       09-30-98  GJF   Bypass all small-block heap code when __sbh_initialized
*                       is 0.
*       11-16-98  GJF   Merged in VC++ 5.0 version of small-block heap.
*       12-18-98  GJF   Changes for 64-bit size_t.
*       05-01-99  PML   Disable small-block heap for Win64.
*       05-26-99  KBF   Updated RTC_Allocate_hook params
*       06-17-99  GJF   Removed old small-block heap from static libs.
*       08-04-00  PML   Don't round allocation sizes when using system 
*                       heap (VS7#131005).
*       10-10-03  AC    Added validation.
*       09-13-04  DJT   Changed error handling to invoke invalid parameter
*                       routine (VSWhidbey:273186).
*       03-31-05  AC    Remove set of errno. This can cause stack overflow in low memory condition.
*                       VSW#476715
*       04-01-05  MSL   Overflow check in ndef WINHEAP
*       05-11-05  AGH   Bypass setting of errno for _calloc_crt.
*       02-13-07  WL    Moved _calloc_impl to its standalone unit.
*
*******************************************************************************/

#include <malloc.h>
#include <string.h>
#include <winheap.h>
#include <windows.h>
#include <internal.h>
#include <mtdll.h>
#include <dbgint.h>
#include <rtcsup.h>

void * __cdecl _calloc_impl (size_t num, size_t size, int * errno_tmp);

/***
*void *calloc(size_t num, size_t size) - allocate storage for an array from
*       the heap
*
*Purpose:
*       Allocate a block of memory from heap big enough for an array of num
*       elements of size bytes each, initialize all bytes in the block to 0
*       and return a pointer to it.
*
*Entry:
*       size_t num  - number of elements in the array
*       size_t size - size of each element
*
*Exit:
*       Success:  void pointer to allocated block
*       Failure:  NULL
*
*Uses:
*
*Exceptions:
*
*******************************************************************************/

void * __cdecl _calloc_base (size_t num, size_t size)
{
    int errno_tmp = 0;
    void * pv = _calloc_impl(num, size, &errno_tmp);

    if ( pv == NULL && errno_tmp != 0 && _errno())
    {
        errno = errno_tmp; // recall, #define errno *_errno() 
    }
    return pv;
}
