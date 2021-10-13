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
*llabs.c - find absolute value of a long long integer
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines llabs() - find absolute value of a long long integer.
*
*Revision History:
*
*******************************************************************************/

#include <cruntime.h>
#include <stdlib.h>

#pragma function(llabs)

/***
*long long llabs(lnumber) - find absolute value of long long.
*
*Purpose:
*	Find the absolute value of a long long integer (lnumber if lnumber >= 0),
*	-lnumber if lnumber < 0).
*
*Entry:
*	long long lnumber - number to find absolute value of
*
*Exit:
*	returns the absolute value of lnumber
*
*Exceptions:
*
*******************************************************************************/

long long __cdecl llabs (
	long long lnumber
	)
{
	return( lnumber>=0LL ? lnumber : -lnumber );
}
