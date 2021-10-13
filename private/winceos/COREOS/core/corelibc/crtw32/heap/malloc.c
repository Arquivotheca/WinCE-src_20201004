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
*malloc.c - Get a block of memory from the heap
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines the malloc() function.
*
*Revision History:
*       06-29-89  GJF   Module created (no rest for the wicked).
*       07-07-89  GJF   Several bug fixes
*       07-21-89  GJF   Added code to maintain proverdesc such that proverdesc
*                       either points to the descriptor for the first free
*                       block in the heap or, if there are no free blocks, is
*                       the same as plastdesc.
*       11-07-89  GJF   Substantially revised to cope with 'tiling'.
*       11-09-89  GJF   Embarrassing bug (didn't bother to assign pdesc)
*       11-10-89  GJF   Added MTHREAD support.
*       11-17-89  GJF   Oops, must call __free_lk() instead of free()!
*       12-18-89  GJF   Changed name of header file to heap.h, also added
*                       explicit _cdecl to function definitions.
*       12-19-89  GJF   Got rid of plastdesc from _heap_split_block() and
*               _       heap_advance_rover().
*       03-11-90  GJF   Replaced _cdecl with _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       07-25-90  SBM   Replaced <stdio.h> by <stddef.h>, replaced
*                       <assertm.h> by <assert.h>
*       09-28-90  GJF   New-style function declarators.
*       02-26-91  SRW   Optimize heap rover for _WIN32_.
*       03-07-91  FAR   Fix bug in heap rover
*       03-11-91  FAR   REALLY Fix bug in heap rover
*       03-14-91  GJF   Changed strategy for rover - old version available
*                       by #define-ing _OLDROVER_.
*       04-05-91  GJF   Temporary hack for Win32/DOS folks - special version
*                       of malloc that just calls HeapAlloc. Change conditioned
*                       on _WIN32DOS_.
*       05-28-91  GJF   Removed M_I386 conditionals and put in _CRUISER_
*                       conditionals so the 'tiling' version is built only for
*                       Cruiser.
*       03-03-93  SKS   Add new handler support (_pnhHeap and related code)
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-22-93  GJF   Turned on range check. Also, removed old Cruiser
*                       support.
*       12-09-93  GJF   Replace 4 (bytes) with _GRANULARITY.
*       02-16-94  SKS   Move set_new_handler support to new() in new.cxx
*       03-01-94  SKS   Declare _pnhHeap, pointer to the new handler, here
*       03-02-94  GJF   Changed logic of, and interface to, _heap_split_block
*                       so that it may fail for lack of a free descriptor.
*                       Also, changed malloc() so that the too-large block is
*                       returned to the caller when _heap_split_block fails.
*       03-03-94  SKS   Reimplement new() handling within malloc - new scheme
*                       allows the new() handler to be called optionally on
*                       malloc failures.  _set_new_mode(1) enables the option.
*       03-04-94  SKS   Rename _nhMode to _newmode, move it to its own module.
*       03-02-94  GJF   Changed logic of, and interface to, _heap_split_block
*                       so that it may fail for lack of a free descriptor.
*                       Also, changed malloc() so that the too-large block is
*                       returned to the caller when _heap_split_block fails.
*       04-01-94  GJF   Made definition of _pnhHeap and declaration of
*                       _newmode conditional on DLL_FOR_WIN32S.
*       11-03-94  CFW   Debug heap support.
*       12-01-94  CFW   Simplify debug interface.
*       02-06-95  CFW   assert -> _ASSERTE.
*       02-07-95  GJF   Deleted _OLDROVER_ code (obsolete). Also made some
*                       temporary, conditional changes so this file can be
*                       used in the MAC builds.
*       02-09-95  GJF   Restored *_base names.
*       05-01-95  GJF   Spliced on winheap version.
*       05-24-95  CFW   Official ANSI C++ new handler added.
*       06-23-95  CFW   ANSI new handler removed, fix block size check.
*       03-01-96  GJF   Added support for small-block heap. Also, the case
*                       where size is too big now gets passed to the
*                       new-handler.
*       05-22-97  RDK   New small-block heap scheme implemented.
*       09-26-97  BWT   Fix POSIX
*       10-03-97  JWM   Removed superfluous "#endif * _POSIX *"
*       11-04-97  GJF   Small POSIX fixes from Roger Lanser.
*       12-17-97  GJF   Exception-safe locking.
*       05-22-98  JWM   Support for KFrei's RTC work.
*       07-28-98  JWM   RTC update.
*       09-30-98  GJF   Bypass all small-block heap code when __sbh_initialized
*                       is 0.
*       11-16-98  GJF   Merged in VC++ 5.0 version of small-block heap.
*       12-18-98  GJF   Changes for 64-bit size_t.
*       05-01-99  PML   Disable small-block heap for Win64.
*       05-13-99  PML   Remove Win32s
*       05-26-99  KBF   Updated RTC hook func params
*       06-22-99  GJF   Removed old small-block heap from static libs.
*       04-28-00  BWT   Fix Posix
*       08-04-00  PML   Don't round allocation sizes when using system
*                       heap (VS7#131005).
*       02-19-02  BWT   Remove posix and !winheap code.  Collapse code
*                       to a single routine to assist with watson dumps.
*       09-30-02  GB    Added error message for unitialized crt calls.
*       10-25-02  SJ    New Handler wasn't being called before returning NULL.
*                       Fixed it to call New Handler before returning NULL 
*                       VSWhidbey#2413 
*       10-10-03  AC    Added validation.
*
*******************************************************************************/

#include <cruntime.h>
#include <malloc.h>
#include <awint.h>
#include <internal.h>
#include <mtdll.h>
#include <dbgint.h>
#include <rterr.h>

#include <windows.h>
#include <winheap.h>
#include <rtcsup.h>

extern int _newmode;    /* malloc new() handler mode */

#ifdef _DEBUG
#define _heap_alloc _heap_alloc_base
#endif

/***
*void *_heap_alloc_base(size_t size) - does actual allocation
*
*Purpose:
*       Same as malloc() except the new handler is not called.
*
*Entry:
*       See malloc
*
*Exit:
*       See malloc
*
*Exceptions:
*
*******************************************************************************/

__forceinline void * __cdecl _heap_alloc (size_t size)

{
#ifdef  HEAPHOOK
    //  call heap hook if installed
    if (_heaphook)
    {
        void * pvReturn;
        if ((*_heaphook)(_HEAP_MALLOC, size, NULL, (void *)&pvReturn))
            return pvReturn;
    }
#endif  /* HEAPHOOK */

    if (_crtheap == 0) {
        _FF_MSGBANNER();    /* write run-time error banner */
            
        _NMSG_WRITE(_RT_CRT_NOTINIT);  /* write message */
        __crtExitProcess(255);  /* normally _exit(255) */
    }

    return HeapAlloc(_crtheap, 0, size ? size : 1);
}


/***
*void *malloc(size_t size) - Get a block of memory from the heap
*
*Purpose:
*       Allocate of block of memory of at least size bytes from the heap and
*       return a pointer to it.
*
*       Calls the new appropriate new handler (if installed).
*
*Entry:
*       size_t size - size of block requested
*
*Exit:
*       Success:  Pointer to memory block
*       Failure:  NULL (or some error value)
*
*Uses:
*
*Exceptions:
*
*******************************************************************************/

void * __cdecl _malloc_base (size_t size)
{
    void *res = NULL;

    //  validate size
    if (size <= _HEAP_MAXREQ) {
        for (;;) {

            //  allocate memory block
            res = _heap_alloc(size);

            //  if successful allocation, return pointer to memory
            //  if new handling turned off altogether, return NULL

            if (res != NULL)
            {
                break;
            }
            if (_newmode == 0)
            {
                errno = ENOMEM;
                break;
            }

            //  call installed new handler
            if (!_callnewh(size))
                break;

            //  new handler was successful -- try to allocate again
        }
    } else {
        _callnewh(size);
        errno = ENOMEM;
        return NULL;
    }

    RTCCALLBACK(_RTC_Allocate_hook, (res, size, 0));
    if (res == NULL)
    {
        errno = ENOMEM;
    }
    return res;
}
