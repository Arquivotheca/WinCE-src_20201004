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
*merr.c - floating point exception handling
*
*	Copyright (c) Microsoft Corporation.	All rights reserved.
*
*Purpose:
*
*Revision History:
*	08-03-94  GJF	Created. Basically, this is a modified copy of the
*			old fpw32\tran\matherr.c.
*
*******************************************************************************/

#ifdef MRTDLL
#undef MRTDLL
#endif

#include <internal.h>
#include <math.h>

/*
 * Define flag signifying the default _matherr routine is being used.
 */
int __defaultmatherr = 1;

/***
*int _matherr(struct _exception *pexcept) - handle math errors
*
*Purpose:
*   Permits the user customize fp error handling by redefining this function.
*
*   The default matherr does nothing and returns 0
*
*Entry:
*
*Exit:
*
*Exceptions:
*******************************************************************************/
int __CRTDECL _matherr (struct _exception *pexcept)
{
    return 0;
}
