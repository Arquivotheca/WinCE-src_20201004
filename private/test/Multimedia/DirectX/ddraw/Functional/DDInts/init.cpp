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
////////////////////////////////////////////////////////////////////////////////
//
//  DDBVT TUX DLL
//  Copyright (c) 1996-1998, Microsoft Corporation
//
//  Module: init.cpp
//          Initialization and finalization functions called by some test cases.
//
//  Revision History:
//      10/04/96    lblanco     Created.
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"
#include <pm.h>

#if NEED_INITDD

////////////////////////////////////////////////////////////////////////////////
// Local prototypes

HRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
HRESULT CALLBACK InitDDCallback(GUID *, LPTSTR, LPTSTR, void *, HMONITOR);

////////////////////////////////////////////////////////////////////////////////
// Module globals

static IDirectDraw      *g_pDD = NULL;
static bool             g_bRegistered = false;
static TCHAR            g_szClassName[] = TEXT("DirectDraw BVT Window");
static DDSTATE          g_ddState = Default;
static bool             g_bSetCoopLevel = true;
static bool             g_bCursorHidden = false;
static CFinishManager   *g_pfmDD = NULL;

static HANDLE g_hPowerManagement = NULL;

////////////////////////////////////////////////////////////////////////////////
// InitDirectDraw
//  Initializes a DirectDraw object (if one wasn't already created) and sets
//  the cooperative level.
//
// Parameters:
//  pDD             Pointer that will hold the interface.
//  pfnFinish       Pointer to a FinishXXX function. If this parameter is not
//                  NULL, the system understands that whenever this object is
//                  destroyed, the FinishXXX function must be called as well.
//
// Return value:
//  true if successful, false otherwise.

bool InitDirectDraw(IDirectDraw *&pDD, FNFINISH pfnFinish)
{


    HRESULT     hr;
    MSG         msg;
    WNDCLASS    wc;
    int         iDisplayIndex = g_iDisplayIndex;
#ifdef UNDER_CE
    if(!g_hPowerManagement)
    {
        TCHAR szDriverName[256];
        TCHAR szBuffer[256];
        HKEY hKey;
        ULONG nDriverName = countof(szDriverName);
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                TEXT("System\\GDI\\Drivers"), 
                                0, // Reseved. Must == 0.
                                0, // Must == 0.
                                &hKey) == ERROR_SUCCESS) 
        {
            if (RegQueryValueEx(hKey,
                                    TEXT("MainDisplay"),
                                    NULL,  // Reserved. Must == NULL.
                                    NULL,  // Do not need type info.
                                    (BYTE *)szDriverName,
                                    &nDriverName) == ERROR_SUCCESS) 
            {
                _stprintf_s(szBuffer, TEXT("%s\\%s"), PMCLASS_DISPLAY, szDriverName);
                g_hPowerManagement = SetPowerRequirement(szBuffer, D0, POWER_NAME, NULL, 0);
            }

            RegCloseKey(hKey);
        }
    }
#endif
    g_tprReturnVal = ITPR_ABORT;

    // Clean out the pointer
    pDD = NULL;
    
    // Did we register our main window class?
    if(!g_bRegistered)
    {
        wc.style        = NULL;
        wc.lpfnWndProc  = MainWndProc;
        wc.cbClsExtra   = 0;
        wc.cbWndExtra   = 0;
        wc.hInstance    = g_hInstance;
        wc.hIcon        = NULL;
        wc.hCursor      = NULL;
        wc.hbrBackground= NULL;
        wc.lpszMenuName = NULL;
        wc.lpszClassName= g_szClassName;

        if(!RegisterClass(&wc))
        {
            Debug(LOG_ABORT, FAILURE_HEADER TEXT("RegisterClass failed"));
            return false;
        }

        g_bRegistered = true;
    }

    // Did we create our main window?
    if(!g_hMainWnd)
    {
        RECT rc;
        g_hMainWnd = CreateWindowEx(
            0,
            g_szClassName,
            NULL,
            0,
            0,
            0,
            GetSystemMetrics(SM_CXSCREEN),
            GetSystemMetrics(SM_CYSCREEN),
            NULL,
            NULL,
            g_hInstance,
            NULL);

        if(!g_hMainWnd)
        {
            Debug(LOG_ABORT, FAILURE_HEADER TEXT("CreateWindow failed"));
            return false;
        }

        // make the window visable
        ShowWindow(g_hMainWnd, SW_SHOW);
        SetFocus(g_hMainWnd);
        UpdateWindow(g_hMainWnd);
        Sleep(500);
        // put the cursor over the window
        GetClientRect(g_hMainWnd, &rc);
        SetCursorPos((rc.left + rc.right)/2, (rc.top + rc.bottom)/2);
        Sleep(500);
        // turn off the cursor
        SetCursor(NULL);
    }

    // Pump any messages
    while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Did we create the DirectDraw object?
    if(!g_pDD)
    {
        Debug(LOG_COMMENT, TEXT("Running InitDirectDraw..."));

        hr = DirectDrawEnumerateEx(
            (LPDDENUMCALLBACKEX)InitDDCallback, 
            &iDisplayIndex, 
            iDisplayIndex >= 0 ? 
                DDENUM_ATTACHEDSECONDARYDEVICES | DDENUM_DETACHEDSECONDARYDEVICES : 0);
        if(FAILED(hr))
        {
            Report(RESULT_ABORT, c_szDirectDrawEnumerate, NULL, hr);
            g_pDD = NULL;
            return false;
        }

        // Did we not create a hardware device?
        if(!g_pDD)
        {
            // Default to the default device
            GUID *pguidDevice = NULL;
            g_pDD = (IDirectDraw *)PRESET;

            hr = DirectDrawCreate(pguidDevice, &g_pDD, NULL);
            if(hr == DDERR_UNSUPPORTED)
            {
                g_tprReturnVal = ITPR_SKIP;
                Debug(LOG_DETAIL,TEXT("DirectDraw not supported, skipping."));
                return false;
            }
            
            if(!CheckReturnedPointer(
                CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
                g_pDD,
                c_szDirectDrawCreate,
                NULL,
                hr,
                TEXT("NULL")))
            {
                g_pDD = NULL;
                return false;
            }
            Debug(
                LOG_DETAIL,
                TEXT("Using %s device for all tests"),
                pguidDevice == NULL ? TEXT("default") : TEXT("emulation"));
        }

        if(g_bSetCoopLevel)
        {
            // Set the cooperative level
            hr = g_pDD->SetCooperativeLevel(
                g_hMainWnd,
                DDSCL_FULLSCREEN);
            if(FAILED(hr))
            {
                Report(
                    RESULT_ABORT,
                    c_szIDirectDraw,
                    c_szSetCooperativeLevel,
                    hr);
                g_pDD->Release();
                g_pDD = NULL;
                return false;
            }

#ifndef UNDER_CE
            // Change to our preferred display mode
            hr = g_pDD->SetDisplayMode(
                640,
                480,
                16);
            if(FAILED(hr))
            {
                Report(
                    RESULT_ABORT,
                    c_szIDirectDraw,
                    c_szSetDisplayMode,
                    hr,
                    TEXT("640x480x16"));
                g_pDD->Release();
                g_pDD = NULL;
                return false;
            }
#endif // UNDER_CE
        }
        Debug(LOG_COMMENT, TEXT("Done with InitDirectDraw"));
    }

    // Do we have a CFinishManager object?
    if(!g_pfmDD)
    {
        g_pfmDD = new CFinishManager;
        if(!g_pfmDD)
        {
            g_pDD->Release();
            g_pDD = NULL;
            return false;
        }
    }

    // Add the FinishXXX function to the chain.
    g_pfmDD->AddFunction(pfnFinish);
    pDD = g_pDD;

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// FinishDirectDraw
//  Cleans up after InitDirectDraw
//
// Parameters:
//  None.
//
// Return value:
//  None.

void FinishDirectDraw(void)
{
    MSG     msg;

    // Terminate any dependent objects
    if(g_pfmDD)
    {
        g_pfmDD->Finish();
        delete g_pfmDD;
        g_pfmDD = NULL;
    }

    // Destroy the DirectDraw object
    if(g_pDD)
    {
        // We do not need to restore the display mode here, since any
        // tests that change the display mode will restore when they
        // are finished.
#ifndef KATANA
        g_pDD->SetCooperativeLevel(NULL, DDSCL_NORMAL);
#endif // KATANA

        g_pDD->Release();
        g_pDD = NULL;
    }

    // Get rid of any remaining messages
    while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

#ifndef UNDER_CE
    // Show the cursor
    if(g_bCursorHidden)
    {
        ShowCursor(TRUE);
        g_bCursorHidden = false;
    }
#endif // UNDER_CE

    // Destroy the main window
    if(g_hMainWnd)
    {
        DestroyWindow(g_hMainWnd);
        g_hMainWnd = NULL;
    }

    // Get rid of any further messages
    while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Unregister the window class
    UnregisterClass(g_szClassName, g_hInstance);
    g_bRegistered = false;

    if(g_hPowerManagement)
    {
        ReleasePowerRequirement(g_hPowerManagement);
        g_hPowerManagement = NULL;
    }
    
}

////////////////////////////////////////////////////////////////////////////////
// MainWndProc
//  Processes messages for the DirectDraw BVT main window.
//
// Parameters:
//  hWnd            Handle of the window.
//  message         Message code.
//  wParam          Message-dependend data.
//  lParam          More message-dependend data.
//
// Return value:
//  Message dependent.

LRESULT CALLBACK MainWndProc(
    HWND    hWnd,
    UINT    message,
    WPARAM  wParam,
    LPARAM  lParam)
{
    switch(message)
    {
    case WM_DESTROY:
        break;

    case WM_CHAR:
        g_bKeyHit = true;
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// ResetDirectDraw
//  Deletes all DirectDraw-related objects, and sets the global state so that
//  a certain type of objects are created next time InitDirectDraw or
//  InitDirectDrawSurface are called.
//
// Parameters:
//  ddState         State code for the next incarnation of DirectDraw objects.
//
// Return value:
//  The current state code if successful, Error otherwise.

DDSTATE ResetDirectDraw(DDSTATE ddState)
{
    DDSTATE ddOldState = g_ddState;

    switch(ddState)
    {
    case Default:
        g_bSetCoopLevel = true;
        break;

    case DDOnly:
        g_bSetCoopLevel = false;
        break;

    default:
        return Error;
    }

    g_ddState = ddState;

    // Delete any objects we might have
    if(g_pDD)
    {
        g_pDD->Release();
        g_pDD = NULL;
    }

    return ddOldState;
}

////////////////////////////////////////////////////////////////////////////////
// InitDDCallback
//  Enumeration callback for InitDirectDraw. This function looks for any
//  hardware device. If it doesn't find one, it doesn't create a DirectDraw
//  object.
//
// Parameters:
//  pGUID           The GUID of the enumerated device.
//  pszName         The driver name.
//  pszDescription  The description of the device.
//  pvContext       This parameter is ignored.
//
// Return value:
//  DDENUMRET_OK to continue enumerating or DDENUMRET_CANCEL to stop.

HRESULT CALLBACK InitDDCallback(
    GUID     *pGUID,
    LPTSTR   pszDescription,
    LPTSTR   pszName,
    void     *vpContext,
    HMONITOR hMon)
{
    HRESULT     hr;
    TCHAR   szGUID[50];
    int   * piDisplayIndex = (int *) vpContext;
    
    if(!pszDescription)
        pszDescription = (LPTSTR)TEXT("<NULL driver description>");
    if(!pszName)
        pszName = (LPTSTR)TEXT("<NULL driver name>");

    // Build a string representation of the GUID
    if(pGUID)
    {
        _stprintf_s(
            szGUID,
            TEXT("{%08X-%08X-%08X-%02X%02X-%02X%02X%02X%02X%02X%02X}"),
            pGUID->Data1,
            pGUID->Data2,
            pGUID->Data3,
            pGUID->Data4[0],
            pGUID->Data4[1],
            pGUID->Data4[2],
            pGUID->Data4[3],
            pGUID->Data4[4],
            pGUID->Data4[5],
            pGUID->Data4[6],
            pGUID->Data4[7]);
    }
    else
        _tcscpy_s(szGUID, TEXT("NULL"));

    // Log the found device
    Debug(
        LOG_DETAIL,
        TEXT("Found DirectDraw device %s (%s, GUID %s)"),
        pszName,
        pszDescription,
        szGUID);

    // If this is the default (NULL GUID) device, keep enumerating
    if(!pGUID)
    {
        return (HRESULT)DDENUMRET_OK;
    }

    // Otherwise this is a hardware device. Keep the first hardware device
    // we find
    if (*piDisplayIndex > 0)
    {
        (*piDisplayIndex)--;
        return (HRESULT)DDENUMRET_OK;
    }
    
    g_pDD = (IDirectDraw *)PRESET;
    hr = DirectDrawCreate(pGUID, &g_pDD, NULL);

    if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        g_pDD,
        c_szDirectDrawCreate,
        NULL,
        hr,
        szGUID))
    {
        g_pDD = NULL;

        // Keep looking for a device that we can create
        return (HRESULT)DDENUMRET_OK;
    }

    // Keep this device
    Debug(
        LOG_DETAIL,
        TEXT("Using device \"%s\" (%s) for all tests"),
        pszName,
        pszDescription);

    return (HRESULT)DDENUMRET_CANCEL;
}
#endif // NEED_IDD

////////////////////////////////////////////////////////////////////////////////
