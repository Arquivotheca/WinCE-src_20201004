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


#include "autodprv.h"
#include <auto_xxx.hxx>
#include <string.hxx>

typedef void (*RAS_LOG_INIT)();
typedef void (*RAS_LOG_CONN_STATE)(RASCONNSTATE rasconnstate, DWORD dwError);
typedef void (*RAS_LOG_UNINIT)();

struct LoggerDll
{
    RAS_LOG_INIT        pfnLogInit;
    RAS_LOG_CONN_STATE  pfnLogConnState;
    RAS_LOG_UNINIT      pfnLogUninit;
    HINSTANCE           hLoggerDll;
};


//
// Global variables
//
LoggerDll   g_LoggerDll;


//
// Window Proc for status window
//
LRESULT CALLBACK StatusWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_RASDIALEVENT:
            {
                RASCONNSTATE    rasconnstate = (RASCONNSTATE)(wParam);
                DWORD           dwError = lParam;
                
                if(g_LoggerDll.pfnLogConnState)
                    (*g_LoggerDll.pfnLogConnState)(rasconnstate, dwError);
                
                if((rasconnstate & RASCS_DONE) || dwError != 0)
                    PostQuitMessage(0);
            }
            return TRUE;
    }
    
    return DefWindowProc(hWnd, message, wParam, lParam);
}


DWORD StatusInit(HINSTANCE hInstance)
{
    ce::auto_hkey   hkey;
    
    if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, RK_AUTODIAL, 0, 0, &hkey))
    {
        ce::wstring strDll;
        
        ce::RegQueryValue(hkey, L"LoggerDLL", strDll);
        
        g_LoggerDll.hLoggerDll = LoadLibrary(strDll);
        
        g_LoggerDll.pfnLogInit =      (RAS_LOG_INIT)GetProcAddress(g_LoggerDll.hLoggerDll, L"RasLogInit");
        g_LoggerDll.pfnLogConnState = (RAS_LOG_CONN_STATE)GetProcAddress(g_LoggerDll.hLoggerDll, L"RasConnState");
        g_LoggerDll.pfnLogUninit =    (RAS_LOG_UNINIT)GetProcAddress(g_LoggerDll.hLoggerDll, L"RasLogUninit");
        
        if(g_LoggerDll.pfnLogInit)
            (*g_LoggerDll.pfnLogInit)();
    }
	
    return 0;
}


DWORD StatusUninit(HINSTANCE hInstance)
{
    UnregisterClass(WND_CLASS, hInstance);
	
	if(g_LoggerDll.pfnLogUninit)
        (*g_LoggerDll.pfnLogUninit)();
    
    if(g_LoggerDll.hLoggerDll)
        FreeLibrary(g_LoggerDll.hLoggerDll);
    
    return 0;
}

