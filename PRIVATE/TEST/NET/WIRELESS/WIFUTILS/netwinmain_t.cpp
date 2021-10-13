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
// Implementation of the NetWinMain_t class.
//
// ----------------------------------------------------------------------------

#include "NetWinMain_t.hpp"
#include "WiFUtils.hpp"

#include <assert.h>

using namespace ce::qa;

// Singleton instance:
static void * volatile s_pWinMain = NULL;

// Terminator thread handle:
static HANDLE s_TerminatorThread = NULL;

// ----------------------------------------------------------------------------
//
// Constructor.
//
NetWinMain_t::
NetWinMain_t(void)
    : NetMain_t(),
      m_RefreshInterval(2000),
      m_RefreshIntervalSaved(0),
      m_MainWindowHandle(NULL),
      m_KbdAccelerators(NULL)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
NetWinMain_t::
~NetWinMain_t(void)
{
    if (NULL != s_TerminatorThread)
    {
        CloseHandle(s_TerminatorThread);
        s_TerminatorThread = NULL;
    }
    
    InterlockedExchangePointer(&s_pWinMain, NULL);
}

// ----------------------------------------------------------------------------
//
// A simple sub-thread for watching for the termination signal and,
// when it arives, posting a quit message to shut down the window's
// main message-pump.
//
static DWORD WINAPI
WindowTerminator(
    IN LPVOID pParameter)
{    
    NetWinMain_t *pWinMain = (NetWinMain_t *)pParameter;
    if (WAIT_OBJECT_0 == WaitForSingleObject(pWinMain->GetInterruptHandle(), INFINITE))
    {
        void *pObject = InterlockedCompareExchangePointer(&s_pWinMain, NULL, NULL);
        if (NULL != pObject)
        {
            pWinMain = (NetWinMain_t *)pObject;
            pWinMain->KillMainWindow();
        }
    }
    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Initializes the program.
// Returns ERROR_ALREADY_EXISTS if the program is already running.
//
DWORD
NetWinMain_t::
Init(
    IN int    argc,
    IN TCHAR *argv[])
{
    DWORD result;

    assert(NULL == s_pWinMain);
    assert(NULL == s_TerminatorThread);
    
    // Parse the non-netmain command-line arguments.
    // Also look for a "help" argument.
    result = DoParseCommandLine(argc, argv);
    if (NO_ERROR != result
     || WasOption(argc, argv, TEXT("?")) >= 0
     || WasOption(argc, argv, TEXT("help")) >= 0)
    {
        DoPrintUsage();
        return ERROR_INVALID_PARAMETER;
    }

    // Make sure only one instance exists.
    result = DoFindPreviousWindow();
    if (NO_ERROR != result)
    {
        // If there's already an existing instance, ask the user
        // whether they want to kill it.
        if (ERROR_ALREADY_EXISTS == result)
        {
            DoKillPreviousWindow();
        }
        else
        {
            ShowErrorDialog(TEXT("Error finding last instance: %s"), 
                            Win32ErrorText(result));
            return result;
        }
    }

    // Create the main window.
    result = DoCreateMainWindow(&m_MainWindowHandle);
    if (NO_ERROR != result)
    {
        ShowErrorDialog(TEXT("Error creating window: %s"), 
                        Win32ErrorText(result));
        return result;
    }

    // Load the menu bars (if any).
    result = DoCreateMenuBars();
    if (NO_ERROR != result)
    {
        ShowErrorDialog(TEXT("Error creating manu bars: %s"),
                        Win32ErrorText(result));
        return result;
    }
        
    // Load the keyboard accelerators (if any).
    result = DoCreateAccelerators(&m_KbdAccelerators);
    if (NO_ERROR != result)
    {
        ShowErrorDialog(TEXT("Error loading accellerators: %s"),
                        Win32ErrorText(result));
        return result;
    }
    
    // Give the terminator and winproc access to this object.
    s_pWinMain = this;
    
    // Start a thread to watch for the terminate event and, when it
    // arrives, post a quit message to the window's message-pump.
    s_TerminatorThread = CreateThread(NULL, 0, WindowTerminator,
                                     (LPVOID)this, 0, NULL);
    if (NULL == s_TerminatorThread)
    {
        result = GetLastError();
        ShowErrorDialog(TEXT("Error creating terminator thread: %s"),
                        Win32ErrorText(result));
        return result;
    }
        
    return result;
}
    
// ----------------------------------------------------------------------------
//
// Runs the window-message loop.
//
DWORD
NetWinMain_t::
Run(void)
{
    MSG message = {0};
    while (GetMessage(&message, NULL, 0, 0))
    {
        if (!m_KbdAccelerators
         || !TranslateAccelerator(m_MainWindowHandle, m_KbdAccelerators, &message))
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
    }
    
    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Terminates the main program window.
//
void
NetWinMain_t::
KillMainWindow(void)
{
    PostMessage(m_MainWindowHandle, WM_DESTROY, 0, 0);
}

// ----------------------------------------------------------------------------
//
// Determines if an instance of the program is already running.
// Returns ERROR_ALREADY_EXISTS if so.
// If necessary, creates the program-termination event and looks
// up the handle of the existing program's main window.
//
DWORD
NetWinMain_t::
DoFindPreviousWindow(
    IN const TCHAR *pWindowTitle)
{
    assert(NULL == m_MainWindowHandle);
    m_MainWindowHandle = NULL;
    
    DWORD result;
    for (int tries = 1 ;; ++tries)
    {
        result = DoFindPreviousInstance();
        if (ERROR_ALREADY_EXISTS != result)
            break;
   
        HWND hWnd = FindWindow(gszMainProgName, pWindowTitle);
        if (NULL == hWnd)
        {
            // It's possible that the other window is in the process
            // of being created...
            Sleep(500);
            hWnd = FindWindow(gszMainProgName, pWindowTitle);
        }
        
        if (NULL == hWnd)
        {
            // It's possible the previous instance is going down so
            // try one more time.
            if (tries < 2)
            {
                // Telling the other app to terminate shouldn't hurt
                // if it's already going down. On the other hand, if
                // it's stalled, this might get it moving again.
                Interrupt();
            }
            // If this fails twice, just ignore the other app and take
            // over the event.
            else
            {
                result = NO_ERROR;
                break;
            }
        }
        else
        {
            m_MainWindowHandle = hWnd;
            break;
        }
    } 

    return result;
}

// ----------------------------------------------------------------------------
//
// If there's already an existing instance, asks the user whether
// they want to kill it and, if so, signals an interrupt.
// If the optional "question" string is supplied, asks that rather
// than the standard "Already running. Kill it?".
//
void
NetWinMain_t::
DoKillPreviousWindow(
    IN const TCHAR *pKillPreviousQuestion)
{
    // Ask the user whether to kill the other window.
    DoKillPreviousInstance(pKillPreviousQuestion);
    
    // In any case, set the other window to the foreground.
    SetForegroundWindow((HWND) (((ULONG)m_MainWindowHandle) | 0x01));
}
  
// ----------------------------------------------------------------------------
// 
// Registers and/or creates the main window class
//
DWORD
NetWinMain_t::
DoCreateMainWindow(
    OUT HWND *pWinHandle)
{
    assert(NULL != pWinHandle);
    
    // Set up the main window class
    WNDCLASS winClass = {0};
    winClass.style         = 0;                      // Class style(s)
    winClass.lpfnWndProc   = (WNDPROC)WndProcCallback;
    winClass.cbClsExtra    = 0;                      // No per-class extra data
    winClass.cbWndExtra    = 0;
    winClass.hInstance     = GetProgramInstance();   // Owner of this class
    winClass.hIcon         = NULL;
    winClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    winClass.hbrBackground = (HBRUSH)(COLOR_WINDOW); // Default color
    winClass.lpszMenuName  = NULL;                   // No menus -- use command bars
    winClass.lpszClassName = gszMainProgName;

    if (RegisterClass(&winClass))
    {
        *pWinHandle = CreateWindow(gszMainProgName, 
                                   gszMainProgName, 
                                   WS_VISIBLE, // !WS_VISIBLE, no default styles
                                   CW_USEDEFAULT, CW_USEDEFAULT,
                                   CW_USEDEFAULT, CW_USEDEFAULT, 
                                   NULL, 
                                   (HMENU)NULL,
                                   GetProgramInstance(), 
                                   NULL);
    
        if (NULL != *pWinHandle)
        {
            return NO_ERROR;
        }
    }

    return GetLastError();
}

// ----------------------------------------------------------------------------
//
// Creates the menu bars (if any).
//
DWORD
NetWinMain_t::
DoCreateMenuBars(void)
{
    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//   
// Creates the keyboard accelerator table (if any).
//
DWORD
NetWinMain_t::
DoCreateAccelerators(
    OUT HACCEL *pAccel)
{
    assert(NULL != pAccel);
   *pAccel = NULL;
    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Processes windows messages.
// The default implementation only handles WM_DESTROY messages.
//
LRESULT
NetWinMain_t::
DoWindowMessage(
    HWND   hWnd,
    UINT   uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
} 

// ----------------------------------------------------------------------------
//
// Generates an error message and shows it to the user in an error dialog.
//
void
NetWinMain_t::
ShowErrorDialog(
    IN const TCHAR *pFormat, ...)
{
    va_list varArgs;
    ce::tstring message;
    
    va_start(varArgs, pFormat);
    HRESULT hr = WiFUtils::AddMessageV(&message, pFormat, varArgs);
    va_end(varArgs);

    if (SUCCEEDED(hr))
    {
        LogError(TEXT("%s"), &message[0]);
        MessageBox(NULL, &message[0], gszMainProgName, MB_OK|MB_ICONSTOP);
    }
}

// ----------------------------------------------------------------------------
//
// Default window procedure - hands off all messages to DoWindowMessage.
//
LRESULT CALLBACK
NetWinMain_t::
WndProcCallback(
    HWND   hWnd,
    UINT   uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    void *pObject = InterlockedCompareExchangePointer(&s_pWinMain, NULL, NULL);
    if (NULL == pObject)
    {
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    else
    {
        NetWinMain_t *pWinMain = (NetWinMain_t *)pObject;

        // If necessary, set a timer to refresh the main window.
        if (pWinMain->m_RefreshIntervalSaved != pWinMain->m_RefreshInterval)
        {
            pWinMain->m_RefreshIntervalSaved = pWinMain->m_RefreshInterval;
            SetTimer(hWnd, 10, pWinMain->m_RefreshInterval, NULL); 
        }

        // Process the message.
        return pWinMain->DoWindowMessage(hWnd, uMsg, wParam, lParam);
    }
}
    
// ----------------------------------------------------------------------------
