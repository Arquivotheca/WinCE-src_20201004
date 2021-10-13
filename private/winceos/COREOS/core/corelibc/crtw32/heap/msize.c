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
*msize.c - calculate the size of a memory block in the heap
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines the following function:
*           _msize()    - calculate the size of a block in the heap
*
*Revision History:
*       07-18-89  GJF   Module created
*       11-13-89  GJF   Added MTHREAD support. Also fixed copyright and got
*                       rid of DEBUG286 stuff.
*       12-18-89  GJF   Changed name of header file to heap.h, also added
*                       explicit _cdecl to function definitions.
*       03-11-90  GJF   Replaced _cdecl with _CALLTYPE1 and added #include
*                       <cruntime.h>
*       07-30-90  SBM   Added return statement to MTHREAD _msize function
*       09-28-90  GJF   New-style function declarators.
*       04-08-91  GJF   Temporary hack for Win32/DOS folks - special version
*                       of _msize that calls HeapSize. Change conditioned on
*                       _WIN32DOS_.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       11-03-94  CFW   Debug heap support.
*       12-01-94  CFW   Simplify debug interface.
*       02-01-95  GJF   #ifdef out the *_base names for the Mac builds
*                       (temporary).
*       02-09-95  GJF   Restored *_base names.
*       05-01-95  GJF   Spliced on winheap version.
*       03-05-96  GJF   Added support for small-block heap.
*       04-10-96  GJF   Return type of __sbh_find_block changed to __map_t *.
*       05-30-96  GJF   Minor changes for latest version of small-block heap.
*       05-22-97  RDK   New small-block heap scheme implemented.
*       09-26-97  BWT   Fix POSIX
*       11-04-97  GJF   Changed occurences of pBlock to pblock (in POSIX
*                       support).
*       12-17-97  GJF   Exception-safe locking.
*       09-30-98  GJF   Bypass all small-block heap code when __sbh_initialized
*                       is 0.
*       11-16-98  GJF   Merged in VC++ 5.0 version of small-block heap.
*       05-01-99  PML   Disable small-block heap for Win64.
*       06-22-99  GJF   Removed old small-block heap from static libs.
*       10-10-03  AC    Added validation.
*
*******************************************************************************/

#include <cruntime.h>
#include <malloc.h>
#include <mtdll.h>
#include <winheap.h>
#include <windows.h>
#include <dbgint.h>
#include <internal.h>

/***
*size_t _msize(pblock) - calculate the size of specified block in the heap
*
*Purpose:
*       Calculates the size of memory block (in the heap) pointed to by
*       pblock.
*
*Entry:
*       void *pblock - pointer to a memory block in the heap
*
*Return:
*       size of the block
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function. 
*
*******************************************************************************/

size_t __cdecl _msize_base (void * pblock)
{
#ifdef  _POSIX_
        return (size_t) HeapSize( _crtheap, 0, pblock );
#else
        size_t      retval;

        /* validation section */
        _VALIDATE_RETURN(pblock != NULL, EINVAL, -1);

#ifdef  HEAPHOOK
        if (_heaphook)
        {
            size_t size;
            if ((*_heaphook)(_HEAP_MSIZE, 0, pblock, (void *)&size))
                return size;
        }
#endif  /* HEAPHOOK */

        retval = (size_t)HeapSize(_crtheap, 0, pblock);

        return retval;

#endif  /* _POSIX_ */
}
