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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include "TestWindow.h"
#include <tchar.h>
#include "DebugOutput.h"

//
//  GetHandle
//
//    Accessor method for window handle.
//
//  Return Value:
//
//    HWND: The window handle that is typically initialized in the constructor for this class.
//          May be NULL if initialization failed.
//
HWND WindowSetup::GetHandle()
{
	return m_hWnd;
}

//
//  IsReady
//
//    Accessor method for "initialization state".
//
//  Return Value:
//
//    BOOL:  True if initialized; false if not initialized.  Currently, this merely
//           checks the handle value to determine the "initialization state".
//
BOOL WindowSetup::IsReady()
{
	if (GetHandle())
		return TRUE;

	return FALSE;
}

//
//  MsgProcPostQuitOnDestroy
//
//    Accessor method for "initialization state".
//
//  Return Value:
//
//    BOOL:  True if initialized; false if not initialized.  Currently, this merely
//           checks the handle value to determine the "initialization state".
//
LRESULT WINAPI WindowSetup::MsgProcPostQuitOnDestroy( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg )
    {
        case WM_DESTROY:
            PostQuitMessage( 0 );
            return 0;
    }

    return DefWindowProc( hWnd, msg, wParam, lParam );
}

//
//  WindowSetup 
//
//    Constructor for WindowSetup class.  Sets up a window based on specified arguments.
//    Note that this constructor is written to ensure that there is no such thing as a 
//    "half initialized" state for this class (e.g., if CreateWindow fails, the constructor
//    also unregisters the WNDCLASS).
//
//  Arguments:
//
//    D3DQA_MSGPROC_STYLE: Indicates whether a user-defined MsgProc shall be used, or
//                         this class' default MsgProc shall be used.
//	  WNDPROC: Indicates the user-defined MsgProc, if applicable
//	  CREATE_WINDOW_ARGS: Arguments used for window creation
//
WindowSetup::WindowSetup(D3DQA_MSGPROC_STYLE MsgProcStyle,
                         WNDPROC lpfnWndProc,
                         LPWINDOW_ARGS pWindowArgs)
{
	RECT ClientRect;
	WNDPROC lpMsgProc;
	UINT uiWindowX;
	UINT uiWindowY;

	WndClass = 0;
	m_hWnd = NULL;

	ClientRect.left = ClientRect.top = 0;

	if (TRUE == pWindowArgs->bPParmsWindowed)
	{
		ClientRect.right  = pWindowArgs->uiWindowWidth;
		ClientRect.bottom = pWindowArgs->uiWindowHeight;
	}
	else
	{
		//
		// If D3DM will be used full-screen anyway, create window extents sufficiently small
		// that no reasonable device would have a display too small to satisfy the extents
		//
		ClientRect.right  = 50;
		ClientRect.bottom = 50;
	}


	//
	// This function calculates the required size of the rectangle of a window with 
	// extended style based on the desired client-rectangle size.  Zero indicates failure. 
	//
	if (0 == AdjustWindowRectEx(&ClientRect,                // LPRECT lpRect, 
	                            pWindowArgs->uiWindowStyle, // DWORD dwStyle, 
	                            FALSE,                      // BOOL bMenu, 
	                            0))                         // DWORD dwExStyle 
	{
		DebugOut(DO_ERROR, L"AdjustWindowRectEx failed. (Error: %d)", GetLastError());
		return;
	}

	//
	// This function moves the specified rectangle by the specified
	// offsets. Zero indicates failure.
	//
	if (0 == OffsetRect(&ClientRect, -ClientRect.left, -ClientRect.top))
	{
		DebugOut(DO_ERROR, L"OffsetRect failed. (Error: %d)", GetLastError());
		return;
	}

	//
	// Test to determine if proposed window extents are valid in this mode
	//
	if ((ClientRect.right > GetSystemMetrics(SM_CXSCREEN)) ||
	    (ClientRect.bottom > GetSystemMetrics(SM_CYSCREEN)))
	{
		DebugOut(L"****************************************************************************");
		DebugOut(L"Attempted window extents including non-client area: (%li, %li)", ClientRect.right, ClientRect.bottom);
		DebugOut(L"Screen extents: (%li, %li)", GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
		DebugOut(L"Test is aborting because window extents are too large.  Try one of         ");
		DebugOut(L"the following:                                                             ");
		DebugOut(L"     (1) Launch test with smaller window extents                           ");
		DebugOut(L"     (2) Launch test with window style that consumes less non-client area. ");
		DebugOut(DO_ERROR, L"****************************************************************************");
		return;
	}

	//
	// Screen extents for centering window
	//
	uiWindowX = (GetSystemMetrics(SM_CXSCREEN) - ClientRect.right) / 2;
	uiWindowY = (GetSystemMetrics(SM_CYSCREEN) - ClientRect.bottom) / 2;

	switch(MsgProcStyle)
	{
	case D3DQA_MSGPROC_CUSTOM:
		lpMsgProc = lpfnWndProc;
		break;
	case D3DQA_MSGPROC_POSTQUIT_ONDESTROY:
	default:
		lpMsgProc = MsgProcPostQuitOnDestroy;
		break;
	};

	//
	// Prepare window class attributes for RegisterClass
	//
	WNDCLASS wc = {CS_HREDRAW | CS_VREDRAW,             // UINT style; 
	               lpMsgProc,                           // WNDPROC lpfnWndProc; 
	               0L,                                  // int cbClsExtra; 
	               0L,                                  // int cbWndExtra; 
	               GetModuleHandle(NULL),               // HANDLE hInstance; 
	               NULL,                                // HICON hIcon; 
	               NULL,                                // HCURSOR hCursor; 
	               (HBRUSH)GetStockObject(WHITE_BRUSH), // HBRUSH hbrBackground; 
	               NULL,                                // LPCTSTR lpszMenuName; 
	               pWindowArgs->lpClassName};           // LPCTSTR lpszClassName; 

	//
	// Zero indicates failure; but if the class already exists, continue anyway
	//
	WndClass = RegisterClass(&wc);
    if ((0 == WndClass) && (GetLastError() != ERROR_CLASS_ALREADY_EXISTS))
	{
		DebugOut(DO_ERROR, L"RegisterClass failed. (Error: %d)", GetLastError());
		return;
	}

	//
	// Create the application's window
	//
    m_hWnd = CreateWindowEx(WS_EX_TOPMOST,               //  DWORD dwExStyle, 
                            pWindowArgs->lpClassName,    //  LPCTSTR lpClassName, 
                            pWindowArgs->lpWindowName,   //  LPCTSTR lpWindowName, 
                            pWindowArgs->uiWindowStyle,  //  DWORD dwStyle, 
                            uiWindowX,                   //  int x, 
                            uiWindowY,                   //  int y, 
                            ClientRect.right,            //  int nWidth, 
                            ClientRect.bottom,           //  int nHeight, 
                            NULL,                        //  HWND hWndParent, 
                            NULL,                        //  HMENU hMenu, 
                            wc.hInstance,                //  HINSTANCE hInstance, 
                            NULL );                      //  LPVOID lpParam 


	//
	// A handle to the new window indicates success. NULL indicates failure.
	//
	if (NULL == m_hWnd)
	{
		//
		// If CreateWindow failed, attempt to unregister the class, and abort
		//
		DebugOut(DO_ERROR, L"CreateWindow failed. (Error: %d)", GetLastError());

		//
		// UnregisterClass:  zero indicates that the class could not be found or if a
		// window still exists that was created with the class
		//
		if (0 == UnregisterClass((LPCTSTR)WndClass, GetModuleHandle(NULL)))
		{
			DebugOut(DO_ERROR, L"SetupWindow:  Unable to unregister class. (Error: %d)", GetLastError());
		}

		return;
	}

	//
	// No return value check (previous visibility status is not interesting)
	//
	(void)ShowWindow( m_hWnd,         // HWND hWnd, 
                      SW_SHOWNORMAL); // int nCmdShow 

	(void)UpdateWindow( m_hWnd );

	//
	// Force Direct3D Mobile test window to top
	//
	(void)SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

	//
	// If running in windowed mode, window will be centered, so this will help
	// get a clear view of the window without the cursor
	//
	(void)SetCursorPos(0,0);

	return;
}

//
//  WindowSetup 
//
//    Destructor for WindowSetup class.
//
WindowSetup::~WindowSetup()
{
	// Nonzero indicates success. Zero indicates failure.
	if (0 == DestroyWindow(m_hWnd))
	{
		DebugOut(DO_ERROR, L"SetupWindow:  Unable to destroy window. (Error: %d)", GetLastError());
	}

	if (0 == UnregisterClass((LPCTSTR)WndClass, GetModuleHandle(NULL)))
	{
		DebugOut(DO_ERROR, L"SetupWindow:  Unable to unregister class. (Error: %d)", GetLastError());
	}

	return; // Success
}

