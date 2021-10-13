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
*div.c - contains the div routine
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	Performs a signed divide and returns quotient
*	and remainder.
*
*Revision History:
*	06-02-89  PHG	module created
*	03-14-90  GJF	Made calling type _CALLTYPE1 and added #include
*			<cruntime.h>. Also, fixed the copyright.
*	10-04-90  GJF	New-style function declarator.
*	04-06-93  SKS	Replace _CRTAPI* with __cdecl
*
*******************************************************************************/

#include <cruntime.h>
#include <stdlib.h>

/***
*div_t div(int numer, int denom) - do signed divide
*
*Purpose:
*	This routine does an divide and returns the results.
*
*Entry:
*	int numer - Numerator passed in on stack
*	int denom - Denominator passed in on stack
*
*Exit:
*	returns quotient and remainder in structure
*
*Exceptions:
*	No validation is done on [denom]* thus, if [denom] is 0,
*	this routine will trap.
*
*******************************************************************************/

div_t __cdecl div (
	int numer,
	int denom
	)
{
	div_t result;

	result.quot = numer / denom;
	result.rem = numer % denom;

	return result;
}
