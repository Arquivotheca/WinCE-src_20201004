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
*purevirt.c - stub to trap pure virtual function calls
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines _purecall() -
*
*Revision History:
*	09-30-92  GJF	Module created
*	04-06-93  SKS	Replace _CRTAPI* with __cdecl
*	05-20-02  MSL	Added _set_purecall_handler
*	08-25-03  AC 	Call abort on the default _purecall handler
*	11-07-04  MSL	Add ability to get the handler
*	01-12-05  DJT   Use OS-encoded global function pointers
*	03-04-05  JL	Split _initp_misc_purevirt into a separate module so
*			that it doesn't always pull in _purecall (VSW#465724)
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <internal.h>
#include <rterr.h>
#include <stdlib.h>

/////////////////////////////////////////////////////////////////////////////
//
// The global variable:
//

extern _purecall_handler __pPurecall;

/***
*void _purecall(void) -
*
*Purpose:
*       The compiler calls this if a pure virtual happens
*
*Entry:
*	No arguments
*
*Exit:
*	Never returns
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _purecall(
	void
	)
{
    _purecall_handler purecall = (_purecall_handler) DecodePointer(__pPurecall);
    if(purecall != NULL)
    {
        purecall();

        /*  shouldn't return, but if it does, we drop back to 
            default behaviour 
        */
    }

    _NMSG_WRITE(_RT_PUREVIRT);
    /* do not write the abort message */
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
    abort();
}

/***
*void _set_purecall_handler(void) -
*
*Purpose:
*       Establish a handler to be called when a pure virtual is called
*       Note that if you link to the crt statically, and replace
*       _purecall, then none of this will happen.
*
*       This function is not thread-safe
*
*Entry:
*	New handler
*
*Exit:
*	Old handler
*
*Exceptions:
*
*******************************************************************************/

_CRTIMP _purecall_handler __cdecl
_set_purecall_handler( _purecall_handler pNew )
{
    _purecall_handler pOld = NULL;

    pOld = (_purecall_handler) DecodePointer(__pPurecall);
    __pPurecall = (_purecall_handler) EncodePointer(pNew);

    return pOld;
}

_CRTIMP _purecall_handler __cdecl _get_purecall_handler(void)
{
    return (_purecall_handler) DecodePointer(__pPurecall);
}

#endif
