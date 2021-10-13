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
*heapchk.c - perform a consistency check on the heap
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines the _heapchk() and _heapset() functions
*
*Revision History:
*       06-30-89  JCR   Module created.
*       07-28-89  GJF   Added check for free block preceding the rover
*       11-13-89  GJF   Added MTHREAD support, also fixed copyright
*       12-13-89  GJF   Added check for descriptor order, did some tuning,
*                       changed header file name to heap.h
*       12-15-89  GJF   Purged DEBUG286 stuff. Also added explicit _cdecl to
*                       function definitions.
*       12-19-89  GJF   Got rid of checks involving plastdesc (revised check
*                       of proverdesc and DEBUG errors accordingly)
*       03-09-90  GJF   Replaced _cdecl with _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       03-29-90  GJF   Made _heap_checkset() _CALLTYPE4.
*       09-27-90  GJF   New-style function declarators.
*       03-05-91  GJF   Changed strategy for rover - old version available
*                       by #define-ing _OLDROVER_.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       02-08-95  GJF   Removed obsolete _OLDROVER_ code.
*       04-30-95  GJF   Spliced on winheap version.
*       05-26-95  GJF   Heap[Un]Lock is stubbed on Win95.
*       07-04-95  GJF   Fixed change above.
*       03-07-96  GJF   Added support for the small-block heap to _heapchk().
*       04-30-96  GJF   Deleted obsolete _heapset code, the functionality is
*                       not very well defined nor useful on Win32. _heapset 
*                       now just returns _heapchk.
*       05-22-97  RDK   New small-block heap scheme implemented.
*       12-17-97  GJF   Exception-safe locking.
*       09-30-98  GJF   Bypass all small-block heap code when __sbh_initialized
*                       is 0.
*       11-16-98  GJF   Merged in VC++ 5.0 version of small-block heap.
*       05-01-99  PML   Disable small-block heap for Win64.
*       06-22-99  GJF   Removed old small-block heap from static libs.
*
*******************************************************************************/

#include <cruntime.h>
#include <windows.h>
#include <errno.h>
#include <malloc.h>
#include <mtdll.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <winheap.h>

#ifndef _POSIX_

/***
*int _heapchk()         - Validate the heap
*int _heapset(_fill)    - Obsolete function! 
*
*Purpose:
*       Both functions perform a consistency check on the heap. The old 
*       _heapset used to fill free blocks with _fill, in addition to 
*       performing the consistency check. The current _heapset ignores the 
*       passed parameter and just returns _heapchk.
*
*Entry:
*       For heapchk()
*           No arguments
*       For heapset()
*           int _fill - ignored
*
*Exit:
*       Returns one of the following values:
*
*           _HEAPOK         - completed okay
*           _HEAPEMPTY      - heap not initialized
*           _HEAPBADBEGIN   - can't find initial header info
*           _HEAPBADNODE    - malformed node somewhere
*
*       Debug version prints out a diagnostic message if an error is found
*       (see errmsg[] above).
*
*       NOTE:  Add code to support memory regions.
*
*Uses:
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _heapchk (void)
{
        int retcode = _HEAPOK;

        if (!HeapValidate(_crtheap, 0, NULL))
        {
            retcode = _HEAPBADNODE;
        }
        return retcode;
}

/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */

int __cdecl _heapset (
        unsigned int _fill
        )
{
        return _heapchk();
}

#endif  /* !_POSIX_ */
