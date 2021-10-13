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
*Purpose:
*   Defines C++ setHandler routine.
*
*******************************************************************************/

#include <stddef.h>
#include <corecrt.h>
#include <new.h>
#include <new>

/* pointer to old-style C++ new handler */
_PNH _pnhHeap;
#define _mlock(l)
#define _munlock(l)

/***
*_PNH _set_new_handler(_PNH pnh) - set the new handler
*
*Purpose:
*   _set_new_handler is different from the ANSI C++ working standard definition
*   of set_new_handler.  Therefore it has a leading underscore in its name.
*
*Entry:
*   Pointer to the new handler to be installed.
*
*Return:
*   Previous new handler
*
*******************************************************************************/
_PNH __cdecl _set_new_handler(
    _PNH pnh
    )
{
    _PNH pnhOld;

    /* lock the heap */
    _mlock(_HEAP_LOCK);

    pnhOld = _pnhHeap;
    _pnhHeap = pnh;

    /* unlock the heap */
    _munlock(_HEAP_LOCK);

    return(pnhOld);
}


/***
*_PNH _query_new_handler(void) - query value of new handler
*
*Purpose:
*   Obtain current new handler value.
*
*Entry:
*   None
*
*   WARNING: This function is OBSOLETE. Use _query_new_ansi_handler instead.
*
*Return:
*   Currently installed new handler
*
*******************************************************************************/
_PNH __cdecl _query_new_handler (
    void
    )
{
    return _pnhHeap;
}


/***
*int _callnewh - call the appropriate new handler
*
*Purpose:
*   Call the appropriate new handler.
*
*Entry:
*   None
*
*Return:
*   1 for success
*   0 for failure
*   may throw bad_alloc
*
*******************************************************************************/
extern "C" int __cdecl _callnewh(size_t size) throw(std::bad_alloc)
{
    //ANSI_NEW_HANDLER should not be defined for backwards compatibility reasons!
#ifdef ANSI_NEW_HANDLER
    /*
     * if ANSI C++ new handler is activated but not installed, throw exception
     */
#if 0 //def  _MT
    _ptiddata ptd;

    ptd = _getptd();
    if (ptd->_newh == NULL)
        throw bad_alloc();
#else
    if (_defnewh == NULL)
        throw bad_alloc();
#endif

    /* 
     * if ANSI C++ new handler activated and installed, call it
     * if it returns, the new handler was successful, retry allocation
     */
#if 0 //def  _MT
    if (ptd->_newh != _NO_ANSI_NEW_HANDLER)
        (*(ptd->_newh))();
#else
    if (_defnewh != _NO_ANSI_NEW_HANDLER)
        (*_defnewh)();
#endif

    /* ANSI C++ new handler is inactivated, call old handler if installed */
    else
#endif /* ANSI_NEW_HANDLER */
    {
        _PNH pnh = _pnhHeap;

        if ((pnh == NULL) || ((*pnh)(size) == 0))
            return 0;
    }
    return 1;
}
