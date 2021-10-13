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
#pragma once
#include <windows.h>
#include <parseargs.h>

typedef enum _D3DQA_MSGPROC_STYLE
{
	D3DQA_MSGPROC_POSTQUIT_ONDESTROY = 1,
	D3DQA_MSGPROC_CUSTOM
} D3DQA_MSGPROC_STYLE;

class WindowSetup
{
private:
    
	HWND m_hWnd;
	ATOM WndClass;

	static LRESULT WINAPI MsgProcPostQuitOnDestroy(HWND hWnd,
	                                               UINT msg,
	                                               WPARAM wParam,
	                                               LPARAM lParam);
public:

	WindowSetup(D3DQA_MSGPROC_STYLE MsgProcStyle,
	            WNDPROC lpfnWndProc,
	            LPWINDOW_ARGS pWindowArgs);

	HWND GetHandle();

	BOOL IsReady();

	~WindowSetup();
};
