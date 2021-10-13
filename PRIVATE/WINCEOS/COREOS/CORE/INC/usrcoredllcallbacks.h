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
#ifndef __COREDLLCALLBACKS_H_INCLUDED__
#define __COREDLLCALLBACKS_H_INCLUDED__

// CoredllCallbacks enumeration to index into the callback array
// Do not hardcode any of the enumeration values as it will affect the value of USRCOREDLLCB_MAXCALLBACKS

typedef enum
{
    USRCOREDLLCB_IsForcePixelDoubling,
    USRCOREDLLCB_CallWindowProcW,
    USRCOREDLLCB_MAXCALLBACKS
} EnumCoredllCallback;

extern FARPROC * g_UsrCoredllCallbackArray;

BOOL WINAPI InitializeUsrCoredllCallbacks();

// Declarations of the methods we need to store callbacks to.
int WINAPI IsForcePixelDoubling( void);

LRESULT
WINAPI
UserCallWindowProc(
    __in HPROCESS hprocDest,
    __in WNDPROC WndProc,
    HWND    hWnd,
    UINT    Msg,
    WPARAM  wParam,
    LPARAM  lParam,
    BOOL *pbFaulted
    );

#endif // __COREDLLCALLBACKS_H_INCLUDED__

