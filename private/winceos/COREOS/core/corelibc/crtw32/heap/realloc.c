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
*realloc.c - Reallocate a block of memory in the heap
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines the realloc() and _expand() functions.
*
*Revision History:
*       10-25-89  GJF   Module created.
*       11-06-89  GJF   Massively revised to handle 'tiling' and to properly
*                       update proverdesc.
*       11-10-89  GJF   Added MTHREAD support.
*       11-17-89  GJF   Fixed pblck validation (i.e., conditional call to
*                       _heap_abort())
*       12-18-89  GJF   Changed header file name to heap.h, also added explicit
*                       _cdecl or _pascal to function defintions
*       12-20-89  GJF   Removed references to plastdesc
*       01-04-90  GJF   Fixed a couple of subtle and nasty bugs in _expand().
*       03-11-90  GJF   Replaced _cdecl with _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       03-29-90  GJF   Made _heap_expand_block() _CALLTYPE4.
*       07-25-90  SBM   Replaced <stdio.h> by <stddef.h>, replaced
*                       <assertm.h> by <assert.h>
*       09-28-90  GJF   New-style function declarators.
*       12-28-90  SRW   Added cast of void * to char * for Mips C Compiler
*       03-05-91  GJF   Changed strategy for rover - old version available
*                       by #define-ing _OLDROVER_.
*       04-08-91  GJF   Temporary hack for Win32/DOS folks - special version
*                       of realloc that uses just malloc, _msize, memcpy and
*                       free. Change conditioned on _WIN32DOS_.
*       05-28-91  GJF   Removed M_I386 conditionals and put in _WIN32_
*                       conditionals to build non-tiling heap for Win32.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       08-06-93  SKS   Fix bug in realloc() - if block cannot be expanded in
*                       place, and call to malloc() fails, the code in this
*                       routine was inadvertently freeing the sucessor block.
*                       Reported by Phar Lap TNT team after MSVCNT was final.
*       09-27-93  GJF   Added checks of block size argument(s) against
*                       _HEAP_MAXREQ. Removed old _CRUISER_ and _WIN32DOS_
*                       support. Added some indenting to complicated exprs.
*       12-10-93  GJF   Replace 4 (bytes) with _GRANULARITY.
*       03-02-94  GJF   If _heap_split_block() returns failure, which it now
*                       can, return the untrimmed allocation block.
*       11-03-94  CFW   Debug heap support.
*       12-01-94  CFW   Use malloc with new handler.
*       02-06-95  CFW   assert -> _ASSERTE.
*       02-07-95  GJF   Merged in Mac version. Temporarily #ifdef-ed out the
*                       dbgint.h stuff. Removed obsolete _OLDROVER_ code.
*       02-09-95  GJF   Restored *_base names.
*       05-01-95  GJF   Spliced on winheap version.
*       05-08-95  CFW   Changed new handler scheme.
*       05-22-95  GJF   Test against _HEAP_MAXREQ before calling API.
*       05-24-95  CFW   Official ANSI C++ new handler added.
*       03-05-96  GJF   Added support for small-block heap.
*       04-10-96  GJF   Return type of __sbh_find_block, __sbh_resize_block
*                       and __sbh_free_block changed to __map_t *.
*       05-30-96  GJF   Minor changes for latest version of small-block heap.
*       05-22-97  RDK   New small-block heap scheme implemented.
*       09-26-97  BWT   Fix POSIX
*       11-05-97  GJF   Small POSIX fix from Roger Lanser.
*       12-17-97  GJF   Exception-safe locking.
*       05-22-98  JWM   Support for KFrei's RTC work.
*       07-28-98  JWM   RTC update.
*       09-30-98  GJF   Bypass all small-block heap code when __sbh_initialized
*                       is 0.
*       11-16-98  GJF   Merged in VC++ 5.0 version of small-block heap.
*       12-18-98  GJF   Changes for 64-bit size_t.
*       03-30-99  GJF   __sbh_alloc_block may have moved the header list
*       05-01-99  PML   Disable small-block heap for Win64.
*       05-17-99  PML   Remove all Macintosh support.
*       05-26-99  KBF   Updated RTC hook func params
*       06-22-99  GJF   Removed old small-block heap from static libs.
*       08-04-00  PML   Don't round allocation sizes when using system
*                       heap (VS7#131005).
*       10-10-03  AC    Added validation.
*       04-01-05  MSL   Added _recalloc
*       01-06-06  AEI   VSW#552904: Recalloc initializes memory with 0 
*                       after calling realloc
*       02-13-07  WL    Moved _recalloc to its standalone unit.
*
*******************************************************************************/

#include <cruntime.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <winheap.h>
#include <windows.h>
#include <internal.h>
#include <mtdll.h>
#include <dbgint.h>
#include <rtcsup.h>


/***
*void *realloc(pblock, newsize) - reallocate a block of memory in the heap
*
*Purpose:
*       Reallocates a block in the heap to newsize bytes. newsize may be
*       either greater or less than the original size of the block. The
*       reallocation may result in moving the block as well as changing
*       the size. If the block is moved, the contents of the original block
*       are copied over.
*
*       Special ANSI Requirements:
*
*       (1) realloc(NULL, newsize) is equivalent to malloc(newsize)
*
*       (2) realloc(pblock, 0) is equivalent to free(pblock) (except that
*           NULL is returned)
*
*       (3) if the realloc() fails, the object pointed to by pblock is left
*           unchanged
*
*Entry:
*       void *pblock    - pointer to block in the heap previously allocated
*                         by a call to malloc(), realloc() or _expand().
*
*       size_t newsize  - requested size for the re-allocated block
*
*Exit:
*       Success:  Pointer to the re-allocated memory block
*       Failure:  NULL
*
*Uses:
*
*Exceptions:
*       If pblock does not point to a valid allocation block in the heap,
*       realloc() will behave unpredictably and probably corrupt the heap.
*
*******************************************************************************/

void * __cdecl _realloc_base (void * pBlock, size_t newsize)
{
#ifdef _POSIX_
        return HeapReAlloc(_crtheap, 0, pBlock, newsize);
#else
        void *      pvReturn;
        size_t      origSize = newsize;

        //  if ptr is NULL, call malloc
        if (pBlock == NULL)
            return(_malloc_base(newsize));

        //  if ptr is nonNULL and size is zero, call free and return NULL
        if (newsize == 0)
        {
            _free_base(pBlock);
            return(NULL);
        }

#ifdef  HEAPHOOK
        //  call heap hook if installed
        if (_heaphook)
        {
            if ((*_heaphook)(_HEAP_REALLOC, newsize, pBlock, (void *)&pvReturn))
                return pvReturn;
        }
#endif  /* HEAPHOOK */

        for (;;) {

            pvReturn = NULL;
            if (newsize <= _HEAP_MAXREQ)
            {
                if (newsize == 0)
                    newsize = 1;
                pvReturn = HeapReAlloc(_crtheap, 0, pBlock, newsize);
            }
            else
            {
                _callnewh(newsize);
                errno = ENOMEM;
                return NULL;
            }

            if ( pvReturn || _newmode == 0)
            {
                if (pvReturn)
                {
                    RTCCALLBACK(_RTC_Free_hook, (pBlock, 0));
                    RTCCALLBACK(_RTC_Allocate_hook, (pvReturn, newsize, 0));
                }
                else
                {
                    errno = _get_errno_from_oserr(GetLastError());
                }
                return pvReturn;
            }

            //  call installed new handler
            if (!_callnewh(newsize))
            {
                errno = _get_errno_from_oserr(GetLastError());
                return NULL;
            }

            //  new handler was successful -- try to allocate again
        }
#endif  /* ndef _POSIX_ */
}
