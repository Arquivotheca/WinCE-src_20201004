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
*heapmin.c - Minimize the heap
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Minimize the heap freeing as much memory as possible back
*       to the OS.
*
*Revision History:
*       08-28-89  JCR   Module created.
*       11-06-89  JCR   Improved, partitioned
*       11-13-89  GJF   Added MTHREAD support, also fixed copyright
*       12-14-89  GJF   Couple of bug fixes, some tuning, cleaned up the
*                       formatting a bit and changed header file name to
*                       heap.h
*       12-20-89  GJF   Removed references to plastdesc
*       03-11-90  GJF   Replaced _cdecl with _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       03-29-90  GJF   Made _heapmin_region() and _free_partial_region()
*                       _CALLTYPE4.
*       07-24-90  SBM   Compiles cleanly with -W3 (tentatively removed
*                       unreferenced labels and unreachable code), removed
*                       '32' from API names
*       09-28-90  GJF   New-style function declarators. Also, rewrote expr.
*                       to avoid using cast as lvalue.
*       12-04-90  SRW   Changed to include <oscalls.h> instead of <doscalls.h>
*       12-06-90  SRW   Added _CRUISER_ and _WIN32 conditionals.
*       12-28-90  SRW   Added cast of void * to char * for Mips C Compiler
*       03-05-91  GJF   Changed strategy for rover - old version available
*                       by #define-ing _OLDROVER_.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       03-03-94  GJF   Changed references to _GETEMPTY macro to calls to
*                       the __getempty function. Added graceful handling for
*                       failure of the call to __getempty in _heapmin_region.
*                       However, failure in _free_partial_region will still
*                       result in termination via _heap_abort (very difficult
*                       to handle any other way, very unlikely to occur).
*       02-07-95  GJF   Merged in Mac version. Removed obsolete _OLDROVER_
*                       support.
*       04-30-95  GJF   Spliced on winheap version.
*       03-07-96  GJF   Added support for small-block heap.
*       05-22-97  RDK   New small-block heap scheme implemented.
*       09-26-97  BWT   Remove POSIX
*       12-17-97  GJF   Exception-safe locking.
*       09-30-98  GJF   Bypass all small-block heap code when __sbh_initialized
*                       is 0.
*       11-19-98  GJF   Merged in VC++ 5.0 small-block heap support.
*       05-01-99  PML   Disable small-block heap for Win64.
*       05-17-99  PML   Remove all Macintosh support.
*       06-22-99  GJF   Removed old small-block heap from static libs.
*
*******************************************************************************/

#include <cruntime.h>
#include <windows.h>
#include <errno.h>
#include <malloc.h>
#include <mtdll.h>
#include <stdlib.h>
#include <winheap.h>

/***
*_heapmin() - Minimize the heap
*
*Purpose:
*       Minimize the heap freeing as much memory as possible back
*       to the OS.
*
*Entry:
*       (void)
*
*Exit:
*
*        0 = no error has occurred
*       -1 = an error has occurred (errno is set)
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _heapmin(void)
{
        if ( HeapCompact( _crtheap, 0 ) == 0 ) {
            return -1;
        }
        else {
            return 0;
        }
}
