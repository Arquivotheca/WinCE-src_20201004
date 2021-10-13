//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
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
