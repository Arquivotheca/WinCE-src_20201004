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
*onexit.c - save function for execution on exit
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _onexit(), atexit() - save function for execution at exit
*
*       In order to save space, the table is allocated via malloc/realloc,
*       and only consumes as much space as needed.  __onexittable is
*       set to point to the table if onexit() is ever called.
*
*Revision History:
*       06-30-89  PHG   module created, based on asm version
*       03-15-90  GJF   Replace _cdecl with _CALLTYPE1, added #include
*                       <cruntime.h> and fixed the copyright. Also,
*                       cleaned up the formatting a bit.
*       05-21-90  GJF   Fixed compiler warning.
*       10-04-90  GJF   New-style function declarators.
*       12-28-90  SRW   Added casts of func for Mips C Compiler
*       01-21-91  GJF   ANSI naming.
*       09-09-91  GJF   Revised for C++ needs.
*       03-20-92  SKS   Revamped for new initialization model
*       04-01-92  XY    add init code reference for MAC version
*       04-23-92  DJM   POSIX support.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       05-24-93  SKS   Add __dllonexit for DLLs using MSVCRT10.DLL
*       09-15-93  GJF   Merged NT SDK and Cuda versions. This amounted to
*                       resurrecting and cleaning up the Posix verion (which
*                       may prove obsolete after later review).
*       10-28-93  GJF   Define entry for initialization section (used to be
*                       in i386\cinitone.asm).
*       04-12-94  GJF   Made declarations of _onexitbegin and _onexitend
*                       conditional on ndef DLL_FOR_WIN32S.
*       05-19-94  GJF   For DLL_FOR_WIN32S, changed the reallocation of the
*                       onexit/atexit table in __dllonexit to use malloc and
*                       __mark_block_as_free, instead of realloc.
*       06-06-94  GJF   Replaced 5-19-94 code with use of GlobalAlloc and
*                       GlobalFree.
*       07-18-94  GJF   Must specify GMEM_SHARE in GlobalAlloc.
*       08-22-94  GJF   Fixed table size test to remove implicit assumption
*                       that the heap allocation granularity is at least
*                       sizeof(_PVFV). This removes a barrier to working with
*                       a user-supplied, or third party, heap manager.
*       01-10-95  CFW   Debug CRT allocs.
*       02-02-95  BWT   Update POSIX support (it's the same as Win32 now)
*       02-14-95  CFW   Debug CRT allocs.
*       02-16-95  JWM   Spliced _WIN32 & Mac versions.
*       03-29-95  BWT   Add _msize prototype to fix POSIX build.
*       08-01-96  RDK   Changed initialization pointer data type, changed
*                       _onexit and added _dllonexit to parallel x86
*                       functionality.
*       03-06-98  GJF   Exception-safe locking.
*       12-01-98  GJF   Grow the atexit table much more rapidly.
*       12-18-98  GJF   Changes for 64-bit size_t.
*       04-28-99  PML   Wrap __declspec(allocate()) in _CRTALLOC macro.
*       05-17-99  PML   Remove all Macintosh support.
*       03-27-01  PML   .CRT$XI routines must now return 0 or _RT_* fatal
*                       error code (vs7#231220)
*       10-31-04  AC    Prefast fixes
*                       VSW#373224
*       02-11-05  AC    Remove redundant #pragma section
*                       VSW#445138
*       04-01-05  MSL   Integer overflow protection
*       05-26-05  PML   Encode __onexitbegin/end (vsw#191114)
*       02-16-06  SSM   Encode function pointers stored in atexit list
*
*******************************************************************************/

#include <sect_attribs.h>
#include <cruntime.h>
#include <mtdll.h>
#include <stdlib.h>
#include <internal.h>
#include <malloc.h>
#include <rterr.h>
#include <windows.h>
#include <dbgint.h>

#ifdef  _POSIX_
_CRTIMP size_t __cdecl _msize(void *);
#endif

int __cdecl __onexitinit(void);

_CRTALLOC(".CRT$XIC") static _PIFV pinit = __onexitinit;

/*
 * Define pointers to beginning and end of the table of function pointers
 * manipulated by _onexit()/atexit().
 * NOTE - the pointers are stored encoded.
 */
extern _PVFV *__onexitbegin;
extern _PVFV *__onexitend;

/*
 * Define increments (in entries) for growing the _onexit/atexit table
 */
#define MININCR     4
#define MAXINCR     512

static _onexit_t __cdecl _onexit_nolock(_onexit_t);
static _onexit_t __cdecl _dllonexit_nolock(_onexit_t, _PVFV **, _PVFV **);

/***
*_onexit(func), atexit(func) - add function to be executed upon exit
*
*Purpose:
*       The _onexit/atexit functions are passed a pointer to a function
*       to be called when the program terminate normally.  Successive
*       calls create a register of functions that are executed last in,
*       first out.
*
*Entry:
*       void (*func)() - pointer to function to be executed upon exit
*
*Exit:
*       onexit:
*           Success - return pointer to user's function.
*           Error - return NULL pointer.
*       atexit:
*           Success - return 0.
*           Error - return non-zero value.
*
*Notes:
*       This routine depends on the behavior of _initterm() in CRT0DAT.C.
*       Specifically, _initterm() must not skip the address pointed to by
*       its first parameter, and must also stop before the address pointed
*       to by its second parameter.  This is because _onexitbegin will point
*       to a valid address, and _onexitend will point at an invalid address.
*
*Exceptions:
*
*******************************************************************************/

_onexit_t __cdecl _onexit (
        _onexit_t func
        )
{
        _onexit_t retval;

        _lockexit();

        __try {
            retval = _onexit_nolock(func);
        }
        __finally {
            _unlockexit();
        }

        return retval;
}


static _onexit_t __cdecl _onexit_nolock (
        _onexit_t func
        )
{
        _PVFV * p;
        size_t  oldsize;
        _PVFV * onexitbegin = (_PVFV *) DecodePointer(__onexitbegin);
        _PVFV * onexitend = (_PVFV *) DecodePointer(__onexitend);

        /* overflow check */
        if (onexitend < onexitbegin ||
            ((char *)onexitend - (char *)onexitbegin) + sizeof(_PVFV) < sizeof(_PVFV))
        {
            return NULL;
        }

        /*
         * First, make sure the table has room for a new entry
         */
        if ( (oldsize = _msize_crt(onexitbegin))
                < ((size_t)((char *)onexitend -
            (char *)onexitbegin) + sizeof(_PVFV)) )
        {
            /*
             * not enough room, try to grow the table. first, try to double it.
             */
            size_t newsize = oldsize + __min(oldsize, (MAXINCR * sizeof(_PVFV)));            
            if ( newsize < oldsize ||
                 (p = (_PVFV *)_realloc_crt(onexitbegin, newsize)) == NULL )
            {
                /*
                 * failed, try to grow by MININCR
                 */
                newsize = oldsize + MININCR * sizeof(_PVFV);
                if ( newsize < oldsize ||
                     (p = (_PVFV *)_realloc_crt(onexitbegin, newsize)) == NULL )
                    /*
                     * failed again. don't do anything rash, just fail
                     */
                    return NULL;
            }

            /*
             * update __onexitend and __onexitbegin
             */
#pragma warning(suppress: 22008) /* prefast is confused */
            onexitend = p + (onexitend - onexitbegin);
            onexitbegin = p;
            __onexitbegin = (_PVFV *) EncodePointer(onexitbegin);
        }

        /*
         * Put the new entry into the table and update the end-of-table
         * pointer.
         */
         *(onexitend++) = (_PVFV) EncodePointer(func);
        __onexitend = (_PVFV *) EncodePointer(onexitend);

        return func;
}

int __cdecl atexit (
        _PVFV func
        )
{
        return (_onexit((_onexit_t)func) == NULL) ? -1 : 0;
}


/***
* void __onexitinit(void) - initialization routine for the function table
*       used by _onexit() and atexit().
*
*Purpose:
*       Allocate the table with room for 32 entries (minimum required by
*       ANSI). Also, initialize the pointers to the beginning and end of
*       the table.
*
*Entry:
*       None.
*
*Exit:
*       Returns _RT_ONEXIT if the table cannot be allocated.
*
*Notes:
*       This routine depends on the behavior of doexit() in CRT0DAT.C.
*       Specifically, doexit() must not skip the address pointed to by
*       __onexitbegin, and it must also stop before the address pointed
*       to by __onexitend.  This is because _onexitbegin will point
*       to a valid address, and _onexitend will point at an invalid address.
*
*       Since the table of onexit routines is built in forward order, it
*       must be traversed by doexit() in CRT0DAT.C in reverse order.  This
*       is because these routines must be called in last-in, first-out order.
*
*       If __onexitbegin == __onexitend, then the onexit table is empty!
*
*Exceptions:
*
*******************************************************************************/

int __cdecl __onexitinit (
        void
        )
{
        _PVFV * onexitbegin;

        onexitbegin = (_PVFV *)_calloc_crt(32, sizeof(_PVFV));
        __onexitend = __onexitbegin = (_PVFV *) EncodePointer(onexitbegin);

        if ( onexitbegin == NULL )
            /*
             * cannot allocate minimal required size. return
             * fatal runtime error.
             */
            return _RT_ONEXIT;

        *onexitbegin = (_PVFV) NULL;

        return 0;
}


#ifdef  CRTDLL

/***
*__dllonexit(func, pbegin, pend) - add function to be executed upon DLL detach
*
*Purpose:
*       The _onexit/atexit functions in a DLL linked with MSVCRT.LIB
*       must maintain their own atexit/_onexit list.  This routine is
*       the worker that gets called by such DLLs.  It is analogous to
*       the regular _onexit above except that the __onexitbegin and
*       __onexitend variables are not global variables visible to this
*       routine but rather must be passed as parameters.
*
*Entry:
*       void (*func)() - pointer to function to be executed upon exit
*       void (***pbegin)() - pointer to variable pointing to the beginning
*                   of list of functions to execute on detach
*       void (***pend)() - pointer to variable pointing to the end of list
*                   of functions to execute on detach
*
*Exit:
*       Success - return pointer to user's function.
*       Error - return NULL pointer.
*
*Notes:
*       This routine depends on the behavior of _initterm() in CRT0DAT.C.
*       Specifically, _initterm() must not skip the address pointed to by
*       its first parameter, and must also stop before the address pointed
*       to by its second parameter.  This is because *pbegin will point
*       to a valid address, and *pend will point at an invalid address.
*
*       Note also that we do not decode/encode *pbegin/*pend.  That should
*       be done in the caller, which will be the onexit in atexit.c.
*
*Exceptions:
*
*******************************************************************************/

_onexit_t __cdecl __dllonexit (
        _onexit_t func,
        _PVFV ** pbegin,
        _PVFV ** pend
        )
{
        _onexit_t retval;

        _lockexit();

        __try {
            retval = _dllonexit_nolock(func, pbegin, pend);
        }
        __finally {
            _unlockexit();
        }

        return retval;
}

static _onexit_t __cdecl _dllonexit_nolock (
        _onexit_t func,
        _PVFV ** pbegin,
        _PVFV ** pend
        )
{
        _PVFV   *p=NULL;
        size_t oldsize;

        /*
         * First, make sure the table has room for a new entry
         */
        if ( (oldsize = _msize_crt(*pbegin)) <= (size_t)((char *)(*pend) -
            (char *)(*pbegin)) )
        {
            /*
             * not enough room, try to grow the table
             */
            size_t grow=__min(oldsize, MAXINCR * sizeof(_PVFV));
            if((_HEAP_MAXREQ-grow<oldsize) ||
                ((p = (_PVFV *)_realloc_crt((*pbegin), oldsize + grow)) == NULL))
            {
                /*
                 * failed, try to grow by ONEXITTBLINCR
                 */
                grow=MININCR * sizeof(_PVFV);
                if ( (_HEAP_MAXREQ-grow<oldsize) ||
                    ((p = (_PVFV *)_realloc_crt((*pbegin), oldsize + grow)) == NULL ))
                {
                    /*
                     * failed again. don't do anything rash, just fail
                     */
                    return NULL;
                }
            }

            /*
             * update (*pend) and (*pbegin)
             */
            (*pend) = p + ((*pend) - (*pbegin));
            (*pbegin) = p;
        }

        /*
         * Put the new entry into the table and update the end-of-table
         * pointer.
         */
         *((*pend)++) = (_PVFV)func;

        return func;

}

#endif  /* CRTDLL */
