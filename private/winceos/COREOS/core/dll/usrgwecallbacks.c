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

typedef BOOL (*SipPreference)(HWND, SIPSTATE);

LRESULT
WINAPI
UserCallWindowProc(
    __in HPROCESS hprocDest,
    __in WNDPROC WndProc,
    HWND    hWnd,
    UINT    Msg,
    WPARAM  wParam,
    LPARAM  lParam,
    LPDWORD  pdwErr
    )
{
    LRESULT lResult = 0;
    
#ifdef KCOREDLL
    *pdwErr = 0;
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
                *pdwErr = cbi.dwErrRtn;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                *pdwErr = ERROR_EXCEPTION_IN_SERVICE;
            }
        }
    }
#else
    UNREFERENCED_PARAMETER(hprocDest);
    UNREFERENCED_PARAMETER(pdwErr);
    lResult = CallWindowProcW( WndProc, hWnd, Msg, wParam, lParam);
#endif

    return lResult;
}

//This API is moved here from UsrAygshellCallbacks.c due to it should 
//not be a shell-dependent feature
//User version of BOOL SipPreference(HWND hwnd, SIPSTATE st)
BOOL
WINAPI
UserSHSipPreference(
    __in HPROCESS hprocDest,
    HWND hWnd,
    SIPSTATE st
    )
{
    BOOL fResult = FALSE;
#ifdef KCOREDLL
    if (GetCurrentProcessId()!=(DWORD)hprocDest)
    {
        if ((g_UsrCoredllCallbackArray != NULL) &&
            g_UsrCoredllCallbackArray[USRCOREDLLCB_SHSipPreference])
        {
            CALLBACKINFO cbi;
            cbi.hProc = hprocDest;
            cbi.pfn = g_UsrCoredllCallbackArray[USRCOREDLLCB_SHSipPreference];
            cbi.pvArg0 = hprocDest;
            __try
            {
                fResult = (LRESULT)PerformCallBack(&cbi, hWnd, st) && !cbi.dwErrRtn;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }
    }
#else
    HMODULE hCoreDll= NULL;
    SipPreference pfnSipPreference = NULL;
    UNREFERENCED_PARAMETER(hprocDest);

    hCoreDll = GetModuleHandle(TEXT("coredll.dll"));
    if( hCoreDll != NULL )
    {
        pfnSipPreference = (SipPreference)GetProcAddress(hCoreDll, (LPCTSTR)L"SipPreference");
        if (NULL == pfnSipPreference)
        {
            ERRORMSG(1, (TEXT("Could not find SipPreference.\r\n")));
            SetLastError(ERROR_PROC_NOT_FOUND);
        }
        else
        {
            fResult = pfnSipPreference(hWnd, st);
        }
    }
#endif

    return fResult;
}
