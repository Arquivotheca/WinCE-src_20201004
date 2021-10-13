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
*heaphook.c - set the heap hook
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines the following functions:
*           _setheaphook() - set the heap hook
*
*Revision History:
*       05-24-95  CFW   Official ANSI C++ new handler added.
*       01-12-05  DJT   Use OS-encoded global function pointers
*
*******************************************************************************/

#ifdef HEAPHOOK

#include <cruntime.h>
#include <stddef.h>

#include <winheap.h>

_HEAPHOOK _heaphook = NULL;

/***
*_HEAPHOOK _setheaphook - set the heap hook
*
*Purpose:
*       Allow testers/users/third-parties to hook in and monitor heap activity or
*       add their own heap.
*
*Entry:
*       _HEAPHOOK newhook - pointer to new heap hook routine
*
*Exit:
*       Return old hook.
*
*Exceptions:
*
*******************************************************************************/

_CRTIMP _HEAPHOOK __cdecl _setheaphook(_HEAPHOOK newhook)
{
        _HEAPHOOK oldhook = _heaphook;

        _heaphook = newhook;

        return oldhook;
}

void _setinitheaphook(void)
{
}

#endif /* HEAPHOOK */
