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
/*++
 
Module Name:
 
	TuxSlct.h
 
Abstract:
 
	This the include file the TUX Test Selection Library.
	The purpose of this library is to allow Test functions
	to selected at runn time.  This selection is done by 
	presenting the user with a full screen window that lists
	the tests as server helper buttons.  The user can then 
	select the test to be run.
 
Notes:
 
--*/
#ifndef _TUX_SLCT_H_
#define _TUX_SLCT_H_

#include <tux.h>
extern "C"
LPFUNCTION_TABLE_ENTRY TuxSelectTestsStr( LPFUNCTION_TABLE_ENTRY lpFTE, HINSTANCE hInstance, LPTSTR szName );

#define TuxSelectTests(X, Y) TuxSelectTestsStr( X, Y, NULL);



#endif _TUX_SLCT_H_
