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
*handler.cpp - defines C++ setHandler routine
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Defines C++ setHandler routine.
*
*Revision History:
*       05-07-90  WAJ   Initial version.
*       08-30-90  WAJ   new now takes unsigned ints.
*       08-08-91  JCR   call _halloc/_hfree, not halloc/hfree
*       08-13-91  KRS   Change new.hxx to new.h.  Fix copyright.
*       08-13-91  JCR   ANSI-compatible _set_new_handler names
*       10-30-91  JCR   Split new, delete, and handler into seperate sources
*       11-13-91  JCR   32-bit version
*       06-15-92  KRS   Remove per-thread handler for multi-thread libs
*       03-02-94  SKS   Add _query_new_handler(), remove commented out
*                       per-thread thread handler version of _set_new_h code.
*       04-01-94  GJF   Made declaration of _pnhHeap conditional on ndef
*                       DLL_FOR_WIN32S.
*       05-03-94  CFW   Add set_new_handler.
*       06-03-94  SKS   Remove set_new_hander -- it does NOT conform to ANSI
*                       C++ working standard.  We may implement it later.
*       09-21-94  SKS   Fix typo: no leading _ on "DLL_FOR_WIN32S"
*       02-01-95  GJF   Merged in common\handler.cxx (used for Mac).
*       02-01-95  GJF   Comment out _query_new_handler for Mac builds
*                       (temporary).
*       04-13-95  CFW   Add set_new_handler stub (asserts on non-NULL handler).
*       05-08-95  CFW   Official ANSI C++ new handler added.
*       06-23-95  CFW   ANSI new handler removed from build.
*       12-28-95  JWM   set_new_handler() moved to setnewh.cpp for granularity.
*       10-02-96  GJF   In _callnewh, use a local to hold the value of _pnhHeap
*                       instead of asserting _HEAP_LOCK.
*       09-22-97  JWM   "OBSOLETE" warning removed from _set_new_handler().
*       05-13-99  PML   Remove Win32s
*       12-12-01  BWT   Switch to _getptd_noexit instead of getptd - it's ok to
*                       throw bad_alloc instead of terminating.
*       03-30-04  AC    Added _set_new_handler overload.
*                       VSW#250700
*       01-12-05  DJT   Use OS-encoded global function pointers
*       03-24-05  AC    Used encode/decode function which do not take the loader lock
*                       VSW#473724
*       06-03-05  MSL   Revert 64 bit definition of NULL due to calling-convention fix
*
*******************************************************************************/

#include <stddef.h>
#include <internal.h>
#include <cruntime.h>
#include <mtdll.h>
#include <process.h>
#include <new.h>
#include <dbgint.h>

/* pointer to old-style C++ new handler */
_PNH _pnhHeap;

/***
*_initp_heap_handler()
*
*Purpose:
*
*******************************************************************************/

extern "C"
void __cdecl _initp_heap_handler(void *enull)
{
        _pnhHeap = (_PNH) enull;
}


/***
*_PNH _set_new_handler(_PNH pnh) - set the new handler
*
*Purpose:
*       _set_new_handler is different from the ANSI C++ working standard definition
*       of set_new_handler.  Therefore it has a leading underscore in its name.
*
*Entry:
*       Pointer to the new handler to be installed.
*
*Return:
*       Previous new handler
*
*******************************************************************************/
_PNH __cdecl _set_new_handler( 
        _PNH pnh 
        )
{
        _PNH pnhOld;

        /* lock the heap */
        _mlock(_HEAP_LOCK);

        pnhOld = (_PNH) DecodePointer(_pnhHeap);
        _pnhHeap = (_PNH) EncodePointer(pnh);

        /* unlock the heap */
        _munlock(_HEAP_LOCK);

        return(pnhOld);
}


/***
*_PNH _set_new_handler(int) - set the new handler
*
*Purpose:
*       Overload of _set_new_handler to be used when caller
*       wants to pass NULL or 0.
*
*Entry:
*       NULL or 0.
*
*Return:
*       Previous new handler
*
*******************************************************************************/
_CRTIMP _PNH __cdecl _set_new_handler( 
        int pnh 
        )
{
        _ASSERTE(pnh == 0);

        return _set_new_handler(static_cast<_PNH>(NULL));
}

/***
*_PNH _query_new_handler(void) - query value of new handler
*
*Purpose:
*       Obtain current new handler value.
*
*Entry:
*       None
*
*       WARNING: This function is OBSOLETE. Use _query_new_ansi_handler instead.
*
*Return:
*       Currently installed new handler
*
*******************************************************************************/
_PNH __cdecl _query_new_handler ( 
        void 
        )
{
        return (_PNH) DecodePointer(_pnhHeap);
}


/***
*int _callnewh - call the appropriate new handler
*
*Purpose:
*       Call the appropriate new handler.
*
*Entry:
*       None
*
*Return:
*       1 for success
*       0 for failure
*       may throw bad_alloc
*
*******************************************************************************/
extern "C" int __cdecl _callnewh(size_t size)
{
        {
            _PNH pnh = (_PNH) DecodePointer(_pnhHeap);

            if ( (pnh == NULL) || ((*pnh)(size) == 0) )
                return 0;
        }
        return 1;
}
