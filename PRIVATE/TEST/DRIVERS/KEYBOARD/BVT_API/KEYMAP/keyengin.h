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
 
Copyright (c) 1996  Microsoft Corporation
 
Module Name:
 
	Keyengin.h
 
Abstract:
 
	This the include file the TUX Test Selection Library.
	The purpose of this library is to allow Test functions
	to selected at runn time.  This selection is done by 
	presenting the user with a full screen window that lists
	the tests as server helper buttons.  The user can then 
	select the test to be run.
 

 
	Uknown (unknown)
 
Notes:
 
--*/
#ifndef __KEYENGIN_H__
#define __KEYENGIN_H__

#include <KatoEx.h>

HWND CreateKeyMapTestWindow( HINSTANCE hInstance, CKato *pKato );
DWORD RemoveTestWindow( HWND hWnd );
BOOL TestKeyMapChar( CKato *pKato, HWND hWnd, LPBYTE pKeys, 
                     DWORD nKeys, const TCHAR cExpect );

// BOOL TestKeyMapChar( CKato *pKato, HWND hWnd, LPBYTE pKeys, 
//                     DWORD nKeys, const TCHAR cExpect );

#endif __KEYENGIN_H__
