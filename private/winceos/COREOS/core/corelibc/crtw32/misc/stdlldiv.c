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
*stdlldiv.c - contains the lldiv routine
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	Performs a signed divide on long longs and returns quotient
*	and remainder.
*
*Revision History:
*	12-22-08  HG	module created
*
*******************************************************************************/

#include <cruntime.h>
#include <stdlib.h>

/***
*lldiv_t lldiv(long long numer, long long denom) - do signed divide
*
*Purpose:
*	This routine does an long long divide and returns the results.
*
*Entry:
*	long long numer - Numerator passed in on stack
*	long long denom - Denominator passed in on stack
*
*Exit:
*	returns quotient and remainder in structure
*
*Exceptions:
*	No validation is done on [denom]* thus, if [denom] is 0,
*	this routine will trap on 64-bit platforms.
*
*******************************************************************************/

lldiv_t __cdecl lldiv (
	long long numer,
	long long denom
	)
{
	lldiv_t result;

	result.quot = numer / denom;
	result.rem = numer % denom;

	return result;
}
