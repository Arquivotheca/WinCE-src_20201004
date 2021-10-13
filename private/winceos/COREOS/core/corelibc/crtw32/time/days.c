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
*days.c - static arrays with days from beg of year for each month
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	contains static arrays used by gmtime and statconv to determine
*	date and time values.  Shows days from beg of year.
*
*Revision History:
*	03-??-84  RLB	initial version
*	05-??-84  DFW	split out definitions from ctime routines
*	07-03-89  PHG	removed _NEAR_ for 386
*	03-20-90  GJF	Fixed copyright.
*
*******************************************************************************/

#include <internal.h>

int _lpdays[] = {
	-1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365
};

int _days[] = {
	-1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364
};
