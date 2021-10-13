//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
