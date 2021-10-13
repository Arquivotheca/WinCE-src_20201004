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
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1997  Microsoft Corporation.  All Rights Reserved.

Module Name:

     keyengin.h  

Abstract:
Functions:
Notes:
--*/
/*++
 
 
Module Name:
 
	Keyengin.h
 
Abstract:
 
	This the include file the TUX Test Selection Library.
	The purpose of this library is to allow Test functions
	to selected at runn time.  This selection is done by 
	presenting the user with a full screen window that lists
	the tests as server helper buttons.  The user can then 
	select the test to be run.
 
Author:
 
	Uknown (unknown)
 
Notes:
 
--*/
#pragma once

#include <KatoEx.h>

HWND CreateKeyMapTestWindow( HINSTANCE hInstance, CKato *pKato );
DWORD RemoveTestWindow( HWND hWnd );
bool ChangeKeyboard(HWND hWnd, HKL newHKL);
bool TestKeyMapChar(CKato *pKato, HWND hWnd, LPBYTE pKeys, DWORD nKeys, const TCHAR caExpects[], INT nNumExpectChars);