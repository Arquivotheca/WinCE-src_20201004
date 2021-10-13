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
#ifndef TEST_WINDOW_H
#define TEST_WINDOW_H

#include <windows.h>
#include "VideoRenderer.h"

// define the function pointer for Window proc.
typedef LRESULT (CALLBACK *WinProc)(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

class TestWindow
{
public:
	TestWindow();
	~TestWindow();
	HRESULT Init();
	void Reset();
	HRESULT Minimize();
	HRESULT Maximize();
	HRESULT SetClientRect(WndRect* pWndRect);
	HRESULT TestWindow::ShowWindow();
	WinProc m_fptrWinProc;

	void SetWindProc( WinProc fptr ) { m_fptrWinProc = fptr; }
	WinProc GetWinProc() { return m_fptrWinProc; }
	HWND	GetWinHandle() { return m_hWnd; }

private:
	static LRESULT CALLBACK TestWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static DWORD WINAPI WindowThread(void* lpVoid);
	DWORD RunMessagePump();

private:
	HWND m_hWnd;
	HANDLE m_hThread;
	TCHAR m_szClassName[128];
};

#endif