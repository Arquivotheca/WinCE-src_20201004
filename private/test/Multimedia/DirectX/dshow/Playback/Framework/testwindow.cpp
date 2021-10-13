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
#include <TCHAR.h>
#include "TestWindow.h"

TestWindow::TestWindow()
{
	m_hWnd = NULL;
	m_hThread = NULL;
	m_fptrWinProc = NULL;
	_tcscpy_s(m_szClassName, 128, TEXT("TestWindow"));
}

TestWindow::~TestWindow()
{
	if (m_hWnd)
		CloseHandle(m_hWnd);
	if (m_hThread != NULL)
		CloseHandle(m_hThread);
}

HRESULT TestWindow::Init()
{
	HRESULT hr = S_OK;
    HWND hWnd;
    WNDCLASS wc;

	// Check if the window class is already registered - if not, register it
	if (!GetClassInfo(0, m_szClassName, &wc))
	{
		memset(&wc, 0, sizeof(WNDCLASS));

		// use customed win procedure
		if ( NULL == m_fptrWinProc )
			wc.lpfnWndProc = (WNDPROC) TestWindowProc;
		else
			wc.lpfnWndProc = (WNDPROC) m_fptrWinProc;

		wc.hInstance = 0;
		wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
		wc.lpszClassName = m_szClassName;
		if (!RegisterClass(&wc))
		{
			DWORD err = GetLastError();
			return HRESULT_FROM_WIN32(err);
		}
	}

	// Create the window
    hWnd = CreateWindow(m_szClassName, TEXT("Test Window"),
						WS_CAPTION | WS_CLIPCHILDREN | WS_THICKFRAME,
                        CW_USEDEFAULT, CW_USEDEFAULT,
                        CW_USEDEFAULT, CW_USEDEFAULT,
                        NULL, NULL, NULL, NULL);
	if (hWnd == NULL)
		hr = E_FAIL;

	// Start the window thread
	HANDLE hThread = NULL;
	if (SUCCEEDED(hr))
	{
		DWORD threadId = 0;
		hThread = CreateThread(NULL, 1024, TestWindow::WindowThread, (void*)this, 0, &threadId);
		if (hThread == NULL)
			hr = E_FAIL;
	}

	if (SUCCEEDED(hr))
	{
		m_hThread = hThread;
		m_hWnd = hWnd;
	}
	else {
		if (hThread != NULL)
			CloseHandle(hThread);
		if (hWnd != NULL)
			CloseHandle(hWnd);
	}


	return hr;
}

DWORD TestWindow::RunMessagePump()
{
	MSG msg;
    memset(&msg, 0, sizeof(MSG));

    while(GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    UnregisterClass(m_szClassName, NULL);

    return (msg.wParam);
}

DWORD WINAPI TestWindow::WindowThread(void* lpParam)
{
	TestWindow* pTestWindow = (TestWindow*)lpParam;
	if (pTestWindow)
		return pTestWindow->RunMessagePump();
	return 0;
}

HRESULT APIENTRY TestWindow::TestWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HRESULT hr = S_OK;

    switch(msg)
    {
        case WM_CREATE:
            return 0;

        case WM_SETTINGCHANGE:
            break;

        case WM_SIZE:
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_TIMER:
            return 0;
        
		case WM_COMMAND:
            int nCommand = LOWORD(wParam);

            // handle other buttons.
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void TestWindow::Reset()
{
	DestroyWindow(m_hWnd);
    m_hWnd = NULL;
}

HRESULT TestWindow::Minimize()
{
	BOOL bResult = 0;
#if 0
	BOOL bResult = 0;
	WINDOWPLACEMENT wndpl;
	wndpl.length = sizeof(WINDOWPLACEMENT);
	wndpl.flags = 0;
	wndpl.showCmd = SW_MINIMIZE;
	wndpl.ptMinPosition = 0;
	wndpl.ptMaxPosition = 0;
	bResult = SetWindowPlacement(m_hWnd, &wndpl);
#endif
	bResult = ::ShowWindow(m_hWnd, SW_HIDE);
	::UpdateWindow(m_hWnd);
	return (bResult) ? S_OK : E_FAIL;
}

HRESULT TestWindow::Maximize()
{
	BOOL bResult = 0;
#if 0
	WINDOWPLACEMENT wndpl;
	wndpl.length = sizeof(WINDOWPLACEMENT);
	wndpl.flags = 0;
	wndpl.showCmd = SW_MAXIMIZE;
	wndpl.ptMinPosition = 0;
	wndpl.ptMaxPosition = 0;
	bResult = SetWindowPlacement(m_hWnd, &wndpl);
#endif
	bResult = ::ShowWindow(m_hWnd, SW_MAXIMIZE);
	::UpdateWindow(m_hWnd);
	if (bResult) {
		return S_OK;
	}
	else {
		DWORD err = GetLastError();
		return HRESULT_FROM_WIN32(err);
	}
}


HRESULT TestWindow::SetClientRect(WndRect* pWndRect)
{
	BOOL bResult = 0;
	bResult = ::SetWindowPos(m_hWnd, HWND_TOP, 
		pWndRect->left, pWndRect->top, pWndRect->right - pWndRect->left, pWndRect->bottom - pWndRect->top, 0);

	if (bResult) {
		return S_OK;
	}
	else {
		DWORD err = GetLastError();
		return HRESULT_FROM_WIN32(err);
	}
}

HRESULT TestWindow::ShowWindow()
{
	// Show the window
	::ShowWindow(m_hWnd, SW_SHOWNORMAL);

	// Update the window
	::UpdateWindow(m_hWnd);

	// Set the window foreground
	::SetForegroundWindow(m_hWnd);

	return S_OK;
}
