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
*newmode.cxx - defines C++ setHandler mode
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	Defines routines to set and to query the C++ new handler mode.
*
*	The new handler mode flag determines whether malloc() failures will
*	call the new() failure handler, or whether malloc will return NULL.
*
*Revision History:
*	03-03-94  SKS	Initial version.
*	03-04-94  SKS	Rename _nhMode to _newmode
*	04-14-94  GJF	Made declaration of _newmode conditional on
*			DLL_FOR_WIN32S.
*	05-01-95  GJF	Replaced above change by including internal.h.
*	10-10-03  AC	Added validation.
*
*******************************************************************************/

#include <cruntime.h>
#include <new.h>
#include <internal.h>

int __cdecl _set_new_mode( int nhm )
{
    int nhmOld;

    /* validation section */
    _VALIDATE_RETURN(nhm == 0 || nhm == 1, EINVAL, -1);

    /*
     * Set the new mode and return the old
     */
    nhmOld = _newmode;
    _newmode = nhm;

    return nhmOld;
}

int __cdecl _query_new_mode ( void )
{
    return _newmode;
}
