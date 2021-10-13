//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <wnd.h>


INT _wnd_t::_nObjCount = 0;


//______________________________________________________________________________
_wnd_t::_wnd_t(HINSTANCE hInstance, LPTSTR lpszWndTitle, DWORD dwStyle, DWORD dwExStyle,
	HWND hParentWnd, COLORREF crBackground, LPTSTR lpszWndCls, BOOL fThread, LPRECT lprc)
{
	WNDCLASS wc;
	PTHREADINFO_CW_DATA pticwd = NULL;
	HANDLE hCreated = NULL;

	memset(_szWndCls, 0, sizeof _szWndCls);
	_hThread = NULL;

	// app instance

	if (hInstance)
	{
		_hInstance = hInstance;
	}
	else
	{
		DebugDump(_T("#### _wnd_t::_wnd_t - Invalid hInstance ####\r\n"));
		goto cleanup;
	}

	// background

	_crBackground = crBackground;

	// wnd class

	if (lpszWndCls)
	{
		if (_tcslen(lpszWndCls) < MAX_PATH)
		{
			_tcscpy(_szWndCls, lpszWndCls);
		}
		else
		{
			DebugDump(_T("#### _wnd_t::_wnd_t - Window class length must be less than %d (MAX_PATH) ####\r\n"),
				MAX_PATH);
			goto cleanup;
		}

		if (!GetClassInfo(_hInstance, lpszWndCls, &wc))	// wndclass is not registered
		{
	        if (!RegisterClass())
	        {
				DebugDump(_T("#### _wnd_t::_wnd_t - RegisterClass failed - Error: 0x%x ####\r\n"),
					GetLastError());
				goto cleanup;
			}
		}
	}
	else
	{
		_stprintf(_szWndCls, _T("%s_0x%x"), DEFAULT_WNDCLS, (DWORD)GetCurrentThread());

        if (!RegisterClass())
       	{
			DebugDump(_T("#### _wnd_t::_wnd_t - RegisterClass failed - Error: 0x%x ####\r\n"),
				GetLastError());
			goto cleanup;
		}
	}

	// creation

	if (fThread)
	{
		pticwd = new THREADINFO_CW_DATA;

		if (!pticwd)
		{
			DebugDump(_T("#### _wnd_t::_wnd_t - Unable to allocate THREADINFO_CW_DATA ####\r\n"));
			goto cleanup;
		}

		pticwd->dwExStyle = dwExStyle;
		pticwd->dwStyle = dwStyle;

		hCreated = CreateEvent(NULL, FALSE /* auto-reset */, 0 /* non-signaled */, NULL);

		if (!hCreated)
		{
			DebugDump(_T("#### _wnd_t::_wnd_t - CreateEvent failed - Error: 0x%x ####\r\n"),
				GetLastError());
			goto cleanup;
		}

		pticwd->hCreated = hCreated;
		pticwd->hParentThread = GetCurrentThread();
		pticwd->hParentWnd = hParentWnd;
		pticwd->pwnd = this;

		// rect

		if (!lprc)
		{
			SetRectEmpty((LPRECT)&pticwd->rc);
		}
		else
		{
			CopyRect((LPRECT)&pticwd->rc, lprc);
		}

		if (_tcslen(lpszWndTitle) < MAX_PATH)
		{
			_tcscpy(pticwd->szWndTitle, lpszWndTitle);
		}
		else
		{
			DebugDump(_T("#### _wnd_t::_wnd_t - Window title length must be less than %d (MAX_PATH) ####\r\n"),
				MAX_PATH);
			goto cleanup;
		}

		_hThread = CreateThread(NULL, 0, ThreadProc, (LPVOID)pticwd, 0, &_dwThreadID);

		DWORD dwReturn = WaitForSingleObject(hCreated, MAX_WAIT);

		delete pticwd;

		if (WAIT_OBJECT_0 == dwReturn)
		{
			if (!_hWnd)
			{
				DebugDump(_T("#### _wnd_t::_wnd_t - Unable to create window in the spawned thread ####\r\n"));
				goto cleanup;
			}
		}
		else
		{
			DebugDump(_T("#### _wnd_t::_wnd_t - WaitForSingleObject not signaled ####\r\n"));
			goto cleanup;
		}
	}
	else
	{
		SetLastError(0);
		_hWnd = CreateWindowEx(
			dwExStyle, _szWndCls, lpszWndTitle, dwStyle,
			lprc ? lprc->left : CW_USEDEFAULT,
			lprc ? lprc->top : CW_USEDEFAULT,
			lprc ? RECT_WIDTH((*lprc)) : CW_USEDEFAULT,
			lprc ? RECT_HEIGHT((*lprc)) : CW_USEDEFAULT,
			hParentWnd, NULL, _hInstance, (LPVOID)this);

		if (!_hWnd)
		{
			DebugDump(_T("#### _wnd_t::_wnd_t - CreateWindowEx failed - Error: 0x%x ####\r\n"));
			goto cleanup;
		}
	}

	SetWindowLong(_hWnd, GWL_USERDATA, (LONG)this);

	_nObjCount++;

cleanup:
	if (pticwd)
	{
		delete pticwd;
	}

	if (hCreated)
	{
		CloseHandle(hCreated);
	}
}


//______________________________________________________________________________
_wnd_t::~_wnd_t()
{
	if (_hThread)
	{
		CloseThread();
	}
	else
	{
		Destroy();
	}

	_nObjCount--;

	UnregisterClass(_szWndCls, _hInstance);
}


//______________________________________________________________________________
BOOL _wnd_t::RegisterClass()
{
	WNDCLASS wc;

	wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = sizeof(_wnd_t *);
    wc.hInstance     = _hInstance;
    wc.hIcon         = 0;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = _szWndCls;

	SetLastError(0);
    return ::RegisterClass(&wc) ? TRUE : FALSE;
}


//______________________________________________________________________________
BOOL _wnd_t::CatchThreadMessage(MSG *pmsg)
{
	if (pmsg->message == MSG_EXITTHRD)
	{
		return Destroy();
	}

	return FALSE;
}


//______________________________________________________________________________
DWORD _wnd_t::CloseThread()
{
	DWORD dwExitCode = 0;

	if (_hThread)
	{
		PostThreadMessage(_dwThreadID, MSG_EXITTHRD, 0, 0);
		WaitForSingleObject(_hThread, MAX_WAIT);

		GetExitCodeThread(_hThread, &dwExitCode);
		CloseHandle(_hThread);
	}

	return dwExitCode;
}


//______________________________________________________________________________
VOID  _wnd_t::Refresh(BOOL fEraseBackground)
{
    InvalidateRect(_hWnd, NULL, fEraseBackground);
	UpdateWindow(_hWnd);
}


//______________________________________________________________________________
VOID _wnd_t::MessagePump(HWND hWnd)
{
	MSG msg;
	const MSGPUMP_LOOPCOUNT = 10;
	const MSGPUMP_LOOPDELAY = 50; // msec

	for (INT i = 0; i < MSGPUMP_LOOPCOUNT; i++)
	{
		while (PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		Sleep(MSGPUMP_LOOPDELAY);
	}
}


//______________________________________________________________________________
DWORD WINAPI ThreadProc(LPVOID lpParameter)
{
	PTHREADINFO_CW_DATA pticwd = (PTHREADINFO_CW_DATA)lpParameter;	// PTHREADINFO_CW_DATA deleted after created event fired
	_wnd_t *pwnd = pticwd->pwnd;

	SetLastError(0);

	bool fUseDefaultDimension = (!RECT_WIDTH(pticwd->rc) && !RECT_HEIGHT(pticwd->rc));

	pwnd->_hWnd = CreateWindowEx(
			pticwd->dwExStyle, pwnd->_szWndCls, pticwd->szWndTitle, pticwd->dwStyle,
			fUseDefaultDimension ? CW_USEDEFAULT : pticwd->rc.left,
			fUseDefaultDimension ? CW_USEDEFAULT : pticwd->rc.top,
			fUseDefaultDimension ? CW_USEDEFAULT : RECT_WIDTH(pticwd->rc),
			fUseDefaultDimension ? CW_USEDEFAULT : RECT_HEIGHT(pticwd->rc),
			pticwd->hParentWnd, NULL, pwnd->_hInstance, (LPVOID)pwnd);

	SetEvent(pticwd->hCreated);	// PTHREADINFO_CW_DATA invalid after created event fired

	if (!pwnd->_hWnd)
	{
		DebugDump(_T("#### _wnd_t::_wnd_t - CreateWindowEx failed - Error: 0x%x ####\r\n"));
		return FALSE;
	}

    MSG msg;
    while (TRUE)
    {
        GetMessage(&msg, NULL, 0, 0);

		if (!pwnd->CatchThreadMessage(&msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			break;
		}
    }

    return TRUE;
}


//______________________________________________________________________________
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	_wnd_t *pwnd = (_wnd_t *)GetWindowLong(hWnd, GWL_USERDATA);

	switch (uMsg)
	{
	case WM_CREATE:
		pwnd = (_wnd_t *)((CREATESTRUCT *)lParam)->lpCreateParams;
		break;

	case WM_PAINT:
	    if (IsWindow(hWnd) && pwnd)
    	{
	        PAINTSTRUCT ps;
	        BeginPaint(hWnd, &ps);
	        HBRUSH hBrush = CreateSolidBrush(pwnd->_crBackground);
	        FillRect(ps.hdc, &ps.rcPaint, hBrush);
	        DeleteObject(hBrush);
	        EndPaint(hWnd, &ps);
	    }
	    return 0;

	default:
		break;
	}

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

