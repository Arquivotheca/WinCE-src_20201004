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
*crtheap.c - Get a block of memory from the heap
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines the _malloc_crt(), _calloc_crt(), _realloc_crt() function.
*
*Revision History:
*       11-06-02  GB    Module integrated from NT sources.      
*       04-09-02  SSM   don't export *_crt allocation functions when building 
*                       SYSCRT
*       03-23-05  MSL   Review comment cleanup
*       04-01-05  MSL   Added _recalloc
*       05-11-05  AGH   Bypass setting of errno for _calloc_crt.
*
*******************************************************************************/
#include <malloc.h>
#include <windows.h>

#define INCR_WAIT 1000

unsigned long _maxwait = 0;

__forceinline unsigned long wait_a_bit(unsigned long WaitTime)
{
     Sleep(WaitTime);
     WaitTime += INCR_WAIT;
     if (WaitTime > _maxwait)      // ~30 minutes total
         WaitTime = -1;
     return WaitTime;
}

#ifdef _SYSCRT
#undef _CRTIMP
#define _CRTIMP
#endif

_CRTIMP unsigned long __cdecl _set_malloc_crt_max_wait(unsigned long newvalue)
{
    unsigned long oldvalue = _maxwait;
    _maxwait = newvalue;
    return oldvalue;
}

_CRTIMP void * __cdecl _malloc_crt(size_t cb)
{
    unsigned long WaitTime = 0;
    void *pv;

Retry:
    pv = malloc(cb);
    if (!pv && _maxwait > 0) {
        WaitTime = wait_a_bit(WaitTime);
        if (WaitTime != -1)
            goto Retry;
    }
    return pv;
}

_CRTIMP void * __cdecl _calloc_crt(size_t count, size_t size)
{
    unsigned long WaitTime = 0;
    void *pv;

    extern void * __cdecl _calloc_impl (size_t, size_t, int *);

Retry:
    pv = _calloc_impl(count, size, NULL);
    if (!pv && _maxwait > 0) {
        WaitTime = wait_a_bit(WaitTime);
        if (WaitTime != -1)
            goto Retry;
    }
    return pv;
}

_CRTIMP void * __cdecl _realloc_crt(void *ptr, size_t size)
{
    unsigned long WaitTime = 0;
    void *pv;

Retry:
    pv = realloc(ptr, size);
    if (!pv && size && _maxwait > 0) {
        WaitTime = wait_a_bit(WaitTime);
        if (WaitTime != -1)
            goto Retry;
    }
    return pv;
}

_CRTIMP void * __cdecl _recalloc_crt(void *ptr, size_t count, size_t size)
{
    unsigned long WaitTime = 0;
    void *pv;

Retry:
    pv = _recalloc(ptr, count, size);
    if (!pv && size && _maxwait > 0)
    {
        WaitTime = wait_a_bit(WaitTime);
        if (WaitTime != -1)
        {
            goto Retry;
        }
    }
    return pv;
}
