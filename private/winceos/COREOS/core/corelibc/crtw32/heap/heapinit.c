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
*heapinit.c -  Initialize the heap
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*
*Revision History:
*       06-28-89  JCR   Module created.
*       06-30-89  JCR   Added _heap_grow_emptylist
*       11-13-89  GJF   Fixed copyright
*       11-15-89  JCR   Moved _heap_abort routine to another module
*       12-15-89  GJF   Removed DEBUG286 stuff, did some tuning, changed header
*                       file name to heap.h and made functions explicitly
*                       _cdecl.
*       12-19-89  GJF   Removed plastdesc field from _heap_desc_ struct
*       03-11-90  GJF   Replaced _cdecl with _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       07-24-90  SBM   Removed '32' from API names
*       10-03-90  GJF   New-style function declarators.
*       12-04-90  SRW   Changed to include <oscalls.h> instead of <doscalls.h>
*       12-06-90  SRW   Added _CRUISER_ and _WIN32 conditionals.
*       02-01-91  SRW   Changed for new VirtualAlloc interface (_WIN32_)
*       03-05-91  GJF   Added definition of _heap_resetsize (conditioned on
*                       _OLDROVER_ not being #define-d).
*       04-04-91  GJF   Reduce _heap_regionsize to 1/2 a meg for Dosx32
*                       (_WIN32_).
*       04-05-91  GJF   Temporary hack for Win32/DOS folks - special version
*                       of _heap_init which calls HeapCreate. The change
*                       conditioned on _WIN32DOS_.
*       04-09-91  PNT   Added _MAC_ conditional
*       02-23-93  SKS   Remove DOSX32 support under WIN32 ifdef
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-26-93  SKS   Add variable _heap_maxregsize (was _HEAP_MAXREGIONSIZE)
*                       Initialize _heap_[max,]reg[,ion]size to Large defaults
*                       heapinit() tests _osver to detect Small memory case
*       05-06-93  SKS   Add _heap_term() to return heap memory to the o.s.
*       01-17-94  SKS   Check _osver AND _winmajor to detect Small memory case.
*       03-02-94  GJF   Changed _heap_grow_emptylist so that it returns a
*                       failure code rather than calling _heap_abort.
*       03-30-94  GJF   Made definitions/declarations of:
*                               _heap_desc
*                               _heap_descpages,
*                               _heap_growsize (aka, _amblksiz),
*                               _heap_maxregsize
*                               _heap_regionsize
*                               _heap_regions
*                       conditional on ndef DLL_FOR_WIN32S.
*       02-08-95  GJF   Removed obsolete _OLDROVER_ support.
*       02-14-95  GJF   Appended Mac version of source file.
*       04-30-95  GJF   Spliced on winheap version.
*       03-06-96  GJF   Added initialization and termination code for the
*                       small-block heap.
*       04-22-97  GJF   Changed _heap_init to return 1 for success, 0 for
*                       failure.
*       05-30-96  GJF   Minor changes for latest version of small-block heap.
*       05-22-97  RDK   New small-block heap scheme implemented.
*       07-23-97  GJF   Changed _heap_init() slightly to accomodate optional
*                       heap running directly on Win32 API (essentially the
*                       scheme we used in VC++ 4.0 and 4.1).
*       09-26-97  BWT   Fix POSIX
*       09-28-98  GJF   Don't initialize the small-block heap on NT 5.
*       11-18-98  GJF   Merged in VC++ 5.0 small-block heap and a selection
*                       scheme from James "Triple-Expresso" MacCalman and
*                       Chairman Dan Spalding.
*       05-01-99  PML   Disable small-block heap for Win64.
*       05-13-99  PML   Remove Win32s
*       05-17-99  PML   Remove all Macintosh support.
*       06-17-99  GJF   Removed old small-block heap from static libs.
*       09-28-99  PML   Processing heap-select env var must not do anything
*                       that uses the heap (vs7#44259).  Also, check the env
*                       var before checking for NT5.0 or better.
*       03-28-01  PML   Protect against GetModuleFileName overflow (vs7#231284)
*       06-25-01  BWT   Alloc heap select strings off the process heap instead of
*                       the stack (ntbug: 423988)
*       11-03-01  PML   Add _get_heap_handle
*       03-13-04  MSL   Removed pointless MEM_DECOMMIT calls for prefast
*       10-09-04  MSL   Prefast initialisation fixes
*       09-24-04  MSL   Ensure _crtheap is valid and checked
*       11-08-06  PMB   Remove support for Win9x and _osplatform and relatives.
*                       DDB#11325
*       02-02-07  STL   DevDiv Bugs.64555 - If not overridden by the environment
*                       variable, unconditionally use the system heap.
*
*******************************************************************************/

#include <cruntime.h>
#include <malloc.h>
#include <stdlib.h>
#include <winheap.h>
#include <internal.h>

HANDLE _crtheap=NULL;

/***
*_heap_init() - Initialize the heap
*
*Purpose:
*       Setup the initial C library heap.
*
*       NOTES:
*       (1) This routine should only be called once!
*       (2) This routine must be called before any other heap requests.
*
*Entry:
*       <void>
*Exit:
*       Returns 1 if successful, 0 otherwise.
*
*Exceptions:
*       If heap cannot be initialized, the program will be terminated
*       with a fatal runtime error.
*
*******************************************************************************/

int __cdecl _heap_init (void)
{
        //  Initialize the "big-block" heap first.
        if ( (_crtheap = GetProcessHeap()) == NULL )      
            return 0;

        return 1;
}

/***
*_heap_term() - return the heap to the OS
*
*Purpose:
*
*       NOTES:
*       (1) This routine should only be called once!
*       (2) This routine must be called AFTER any other heap requests.
*
*Entry:
*       <void>
*Exit:
*       <void>
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _heap_term (void)
{
        _crtheap=NULL;
}

/***
*_get_heap_handle() - Get the HANDLE to the Win32 system heap used by the CRT
*
*Purpose:
*       Retrieve the HANDLE to the Win32 system heap used by the CRT.
*
*Entry:
*       <void>
*Exit:
*       Returns the CRT heap HANDLE as an intptr_t
*
*Exceptions:
*
*******************************************************************************/

intptr_t __cdecl _get_heap_handle(void)
{
    _ASSERTE(_crtheap);
    return (intptr_t)_crtheap;
}
