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
#pragma once

#include <windows.h>
#include <tchddi.h>
#include <tchar.h>
#include "globals.h"

//----------------------------------------------------------------------
// For WinMain.cpp
//----------------------------------------------------------------------

// main processing functions
LRESULT CALLBACK WndProc( HWND, UINT, WPARAM, LPARAM ); 
BOOL TouchPanelCallback( TOUCH_PANEL_SAMPLE_FLAGS, INT, INT );

// init functions
BOOL LoadTouchDriver( VOID );
VOID UnloadTouchDriver( VOID );
BOOL Initialize( VOID );
VOID Deinitialize( VOID );

// user input functions
BOOL WaitForInput( DWORD );

// drawing functions
VOID SetDrawPen( BOOL );
VOID DrawLineTo( POINT );
VOID SetLineFrom( POINT );
VOID ClearDrawing( VOID );
BOOL DrawOnScreen( unsigned int, bool userVerification );

// crosshair drawing functions
VOID SetCrosshair( DWORD, DWORD, BOOL );
BOOL DrawCrosshair( HDC, DWORD, DWORD, DWORD );
VOID HideCrosshair( VOID );

// text output functions
VOID PostTitle( LPCTSTR szFormat, ...);
VOID ClearTitle( VOID );
VOID PostMessage( LPCTSTR szFormat, ...);
VOID ClearMessage( VOID );

DWORD WINAPI WinThread( LPVOID );
BOOL InitTouchWindow( TCHAR * );
void DeinitTouchWindow( TCHAR *, DWORD );

VOID SystemErrorMessage( DWORD dwMessageId );
