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
#include <windows.h>
#include <coredll.h>
#include <UsrCoredllCallbacks.h>

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
    )
{
    LRESULT lResult = 0;
    
#ifdef KCOREDLL
    if (GetCurrentProcessId()!=(DWORD)hprocDest)
    {
        if ((g_UsrCoredllCallbackArray != NULL) &&
            g_UsrCoredllCallbackArray[USRCOREDLLCB_CallWindowProcW])
        {
            CALLBACKINFO cbi;
            cbi.hProc = hprocDest;
            cbi.pfn = g_UsrCoredllCallbackArray[USRCOREDLLCB_CallWindowProcW];
            cbi.pvArg0 = hprocDest;
            __try
            {
                lResult = (LRESULT)PerformCallBack(&cbi, WndProc, hWnd, Msg, wParam, lParam);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            *pbFaulted = TRUE;
            }
        }
    }
#else
    UNREFERENCED_PARAMETER(hprocDest);
    lResult = CallWindowProcW( WndProc, hWnd, Msg, wParam, lParam);
#endif

    return lResult;
}

