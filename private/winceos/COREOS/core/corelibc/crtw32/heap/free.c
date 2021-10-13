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
*free.c - free an entry in the heap
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines the following functions:
*           free()     - free a memory block in the heap
*
*Revision History:
*       06-30-89  JCR   Module created;
*       07-07-89  GJF   Fixed test for resetting proverdesc
*       11-10-89  GJF   Added MTHREAD support. Also, a little cleanup.
*       12-18-89  GJF   Change header file name to heap.h, added register
*                       declarations and added explicit _cdecl to function
*                       definitions
*       03-09-90  GJF   Replaced _cdecl with _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       09-27-90  GJF   New-style function declarators. Also, rewrote expr.
*                       so that a cast was not used as an lvalue.
*       03-05-91  GJF   Changed strategy for rover - old version available
*                       by #define-ing _OLDROVER_.
*       04-08-91  GJF   Temporary hack for Win32/DOS folks - special version
*                       of free that just calls HeapFree. Change conditioned
*                       on _WIN32DOS_.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       11-12-93  GJF   Incorporated Jonathan Mark's suggestion for reducing
*                       fragmentation: if the block following the newly freed
*                       block is the rover block, reset the rover to the
*                       newly freed block. Also, replaced MTHREAD with _MT
*                       and deleted obsolete WIN32DOS support.
*       05-19-94  GJF   Added __mark_block_as_free() for the sole use by
*                       __dllonexit() in the Win32s version of msvcrt*.dll.
*       06-06-94  GJF   Removed __mark_block_as_free()!
*       11-03-94  CFW   Debug heap support.
*       12-01-94  CFW   Simplify debug interface.
*       01-12-95  GJF   Fixed bogus test to reset rover. Also, purged
*                       _OLDROVER_ code.
*       02-01-95  GJF   #ifdef out the *_base names for the Mac builds
*                       (temporary).
*       02-09-95  GJF   Restored *_base names.
*       04-30-95  GJF   Made conditional on WINHEAP.
*       03-01-96  GJF   Added support fro small-block heap.
*       04-10-96  GJF   Return type of __sbh_find_block and __sbh_free_block
*                       changed to __map_t *.
*       05-30-96  GJF   Minor changes for latest version of small-block heap.
*       05-22-97  RDK   New small-block heap scheme implemented.
*       09-26-97  BWT   Fix POSIX
*       11-05-97  GJF   Small POSIX fix from Roger Lanser.
*       12-17-97  GJF   Exception-safe locking.
*       05-22-98  JWM   Support for KFrei's RTC work.
*       07-28-98  JWM   RTC update.
*       09-29-98  GJF   Bypass all small-block heap code when __sbh_initialized
*                       is 0.
*       11-16-98  GJF   Merged in VC++ 5.0 version of small-block heap.
*       05-01-99  PML   Disable small-block heap for Win64.
*       05-17-99  PML   Remove all Macintosh support.
*       05-26-99  KBF   Updated RTC hook func params
*       06-22-99  GJF   Removed old small-block heap from static libs.
*       10-10-03  AC    Added validation.
*       01-13-04  AC    Added _freea_s.
*       02-02-05  AC    Renamed _freea_s to _freea (_freea_s is an alias of _freea)
*                       VSW#416634
*       03-23-05  MSL   Review comment cleanup
*       04-04-05  JL    Remove _alloca_s and _freea_s VSW#452275
*       05-13-05  MSL   Move _freea to inline to get the right allocator
*
*******************************************************************************/

#include <cruntime.h>
#include <malloc.h>
#include <winheap.h>
#include <windows.h>
#include <internal.h>
#include <mtdll.h>
#include <dbgint.h>
#include <rtcsup.h>

/***
*void free(pblock) - free a block in the heap
*
*Purpose:
*       Free a memory block in the heap.
*
*       Special ANSI Requirements:
*
*       (1) free(NULL) is benign.
*
*Entry:
*       void *pblock - pointer to a memory block in the heap
*
*Return:
*       <void>
*
*******************************************************************************/

void __cdecl _free_base (void * pBlock)
{
#ifdef  _POSIX_
        HeapFree(_crtheap,
             0,
             pBlock
            );

#else   /* _POSIX_ */

        int retval = 0;

#ifdef  HEAPHOOK
        /* call heap hook if installed */
        if (_heaphook)
        {
            if ((*_heaphook)(_HEAP_FREE, 0, pBlock, NULL))
                return;
        }
#endif  /* HEAPHOOK */

        if (pBlock == NULL)
            return;

        RTCCALLBACK(_RTC_Free_hook, (pBlock, 0));

        retval = HeapFree(_crtheap, 0, pBlock);
        if (retval == 0)
        {
            errno = _get_errno_from_oserr(GetLastError());
        }
#endif  /* _POSIX_ */
}
