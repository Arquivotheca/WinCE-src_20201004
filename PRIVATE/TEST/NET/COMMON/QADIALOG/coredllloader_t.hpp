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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Definitions and declarations for the CoreDllLoader_t class.
//
// ----------------------------------------------------------------------------

#pragma once

#include <windows.h>

namespace CENetworkQA {

// ----------------------------------------------------------------------------
// 
// Provides a simple mechanism for dynamically loading and using DLLs.
//
class CoreDllLoader_t
{
private:

    // coredll.dll handle:
    HINSTANCE m_DllHandle;

    // DLL load error:
    DWORD m_LoadErrorCode;
    
public:
    
    // Constructs the object by loading the Core DLL:
    CoreDllLoader_t(void);

    // Destructor: unloads the DLL:
   ~CoreDllLoader_t(void);
    
    // Determines whether the library loaded successfully and, if not,
    // inserts an explanation into GetLastError:
    BOOL 
    IsLoaded(void) const
    {
        if (m_DllHandle != NULL)
        {
            return TRUE;
        }
        else
        {
            SetLastError(m_LoadErrorCode);
            return FALSE;
        }
    }
    
    // Retrieves the named procedure from the DLL:
    FARPROC 
    GetProcedureAddress(LPCTSTR pProcedureName) const;
    
	// Function prototypes:
    typedef HWND    (WINAPI *pCreateWindow_t)       (LPCTSTR, LPCTSTR, DWORD,
                                                     int, int, int, int, HWND,
                                                     HMENU, HANDLE, PVOID); 
    typedef HWND    (WINAPI *pCreateWindowEx_t)     (DWORD, LPCTSTR, LPCTSTR,
                                                     DWORD, int, int, int, int,
                                                     HWND, HMENU, HINSTANCE,
                                                     LPVOID);  
    typedef LRESULT (WINAPI *pDefWindowProc_t)      (HWND, UINT, WPARAM,
                                                     LPARAM); 
    typedef BOOL    (WINAPI *pDestroyWindow_t)      (HWND);
    typedef LONG    (WINAPI *pDispatchMessage_t)    (const MSG *);
    typedef BOOL    (WINAPI *pGetMessage_t)         (LPMSG, HWND, UINT, UINT); 
    typedef DWORD   (WINAPI *pGetSystemPowerState_t)(LPWSTR, DWORD, PDWORD);
    typedef int     (WINAPI *pMessageBox_t)         (HWND, LPCTSTR, LPCTSTR,
                                                     UINT);
    typedef BOOL    (WINAPI *pPostMessage_t)        (HWND, UINT, WPARAM,
                                                     LPARAM); 
    typedef void    (WINAPI *pPostQuitMessage_t)    (int);
    typedef BOOL    (WINAPI *pPostThreadMessage_t)  (DWORD, UINT, WPARAM,
                                                     LPARAM);
    typedef ATOM    (WINAPI *pRegisterClass_t)      (const WNDCLASS *);
    typedef BOOL    (WINAPI *pSetEvent_t)           (HANDLE); 
    typedef DWORD   (WINAPI *pSetSystemPowerState_t)(LPCWSTR, DWORD, DWORD);
    typedef BOOL    (WINAPI *pTranslateMessage_t)   (const MSG *); 
    typedef BOOL    (WINAPI *pUnRegisterClass_t)    (LPCTSTR, HINSTANCE); 


    // Look up the CreateWindow function:
    pCreateWindow_t 
    GetCreateWindowProc(void) 
    {
        return (pCreateWindow_t)
                GetProcedureAddress(
#if UNICODE
                                    TEXT("CreateWindowW")
#else
                                    TEXT("CreateWindowA")
#endif
                                    );
    }

    // Look up the CreateWindowEx function:
    pCreateWindowEx_t 
    GetCreateWindowExProc(void) 
    {
        return (pCreateWindowEx_t)
                GetProcedureAddress(
#if UNICODE
                                    TEXT("CreateWindowExW")
#else
                                    TEXT("CreateWindowExA")
#endif
                                    );
    }

    // Look up the DestroyWindow function:
    pDestroyWindow_t 
    GetDestroyWindowProc(void) 
    {
        return (pDestroyWindow_t)
                GetProcedureAddress(TEXT("DestroyWindow"));
    }

    // Look up the DispatchMessage function:
    pDispatchMessage_t 
    GetDispatchMessageProc(void) 
    {
         return (pDispatchMessage_t)
                GetProcedureAddress(TEXT("DispatchMessage"));
    }

    // Look up the GetMessage function:
    pGetMessage_t 
    GetGetMessageProc(void) 
    {
        return (pGetMessage_t)
                GetProcedureAddress(TEXT("GetMessage"));
    }

    // Look up the GetSystemPowerState function:
    pGetSystemPowerState_t 
    GetGetSystemPowerStateProc(void) 
    {
        return (pGetSystemPowerState_t)
                GetProcedureAddress(TEXT("GetSystemPowerState"));
    }

    // Look up the MessageBox function:
    pMessageBox_t 
    GetMessageBoxProc(void) 
    {
        return (pMessageBox_t)
                GetProcedureAddress(
#if UNICODE
                                    TEXT("MessageBoxW")
#else
                                    TEXT("MessageBoxA")
#endif
                                    );
    }

    // Look up the PostMessage function:
    pPostMessage_t 
    GetPostMessageProc(void) 
    {
        return (pPostMessage_t)
                GetProcedureAddress(TEXT("PostMessage"));
    }

    // Look up the PostQuitMessage function:
    pPostQuitMessage_t 
    GetPostQuitMessageProc(void) 
    {
        return (pPostQuitMessage_t)
                GetProcedureAddress(TEXT("PostQuitMessage"));
    }

    // Look up the PostThreadMessage function:
    pPostThreadMessage_t 
    GetPostThreadMessageProc(void) 
    {
        return (pPostThreadMessage_t)
                GetProcedureAddress(TEXT("PostThreadMessage"));
    }

    // Look up the RegisterClass function:
    pRegisterClass_t 
    GetRegisterClassProc(void) 
    {
        return (pRegisterClass_t)
                GetProcedureAddress(TEXT("RegisterClass"));
    }

    // Look up the SetEvent function:
    pSetEvent_t 
    GetSetEventProc(void) 
    {
        return (pSetEvent_t)
                GetProcedureAddress(TEXT("SetEvent"));
    }

    // Look up the SetSystemPowerState function:
    pSetSystemPowerState_t 
    GetSetSystemPowerStateProc(void) 
    {
        return (pSetSystemPowerState_t)
                GetProcedureAddress(TEXT("SetSystemPowerState"));
    }

    // Look up the TranslateMessage function:
    pTranslateMessage_t 
    GetTranslateMessageProc(void) 
    {
        return (pTranslateMessage_t)
                GetProcedureAddress(TEXT("TranslateMessage"));
    }

    // Look up the UnRegisterClass function:
    pUnRegisterClass_t 
    GetUnRegisterClassProc(void) 
    {
        return (pUnRegisterClass_t)
                GetProcedureAddress(
#if UNICODE
                                    TEXT("UnRegisterClassW")
#else
                                    TEXT("UnRegisterClassA")
#endif
                                    );
    }
};

};
// ----------------------------------------------------------------------------
