//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#pragma once
#pragma warning(disable: 4512)

#ifndef __WINAPP_H__
#define __WINAPP_H__

class CWindowsModule
{
public:
    static CWindowsModule& GetWinModule();
    static CWindowsModule* GetWinModulePtr();

    CWindowsModule();
    virtual ~CWindowsModule();

    virtual bool Initialize();
    virtual void Shutdown();

#   if defined(UNDER_CE) || defined(UNICODE) || defined(_UNICODE)
    typedef LPCWSTR command_line_type;
#   else
    typedef LPCSTR command_line_type;
#   endif

    const HINSTANCE m_hInstance;

    const command_line_type m_pszCommandLine;
    LPCTSTR m_pszModuleName;
    INT m_iAppReturnCode;

    HWND m_hWnd;
    int m_nWidth,
        m_nHeight;
    DWORD m_dwWindowStyle;

protected:
    virtual bool InitConsoleOutput();

    virtual bool RegisterWindowClass();
    virtual void UnregisterWindowClass();

    virtual bool CreateMainWindow();
    virtual void DestroyMainWindow();

    WNDCLASS m_WindowClass;

public:
    static BOOL DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved);
};

class CWindowsApplication : public CWindowsModule
{
public:
    static CWindowsApplication& GetWinApp();
    static CWindowsApplication* GetWinAppPtr();

    CWindowsApplication();
    virtual ~CWindowsApplication();

    virtual void Run() = 0; 
    virtual void Shutdown();

public:
    static int WinMain(HINSTANCE hInstance, HINSTANCE, command_line_type pszCmdLine, int);
};

// Use this macro to create the DllMain function
#define DEFINE_DllMain \
    BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved) \
    { \
        return CWindowsModule::DllMain(hInstance, dwReason, lpReserved); \
    } 


// Use this macro to create the WinMain function

#define DEFINE_WinMain \
    int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, CWindowsApplication::command_line_type pszCmdLine, int nCmdShow) \
    { \
        return CWindowsApplication::WinMain(hInstance, 0, pszCmdLine, nCmdShow); \
    }


#endif
