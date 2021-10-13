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
// AuthMat is a user-interface for the Authentication Matrix test suite.
// It takes the same command-line arguments as the Tux/AuthMatrix.dll duo.
//
// ----------------------------------------------------------------------------

#include "AuthMat_t.hpp"
#include "resource.h"

#include <Factory_t.hpp>
#include <Utils.hpp>
#include <NetWinMain_t.hpp>

#include <aygshell.h>
#include <katoex.h>

// Program name:
TCHAR *gszMainProgName = TEXT("AuthMat");

// NetMain inits required:
DWORD gdwMainInitFlags = 0;

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// MyWinMain derives from NetWinMain to customize it for our purposes.
//
class MyWinMain_t : public NetWinMain_t
{
private:

    // Manu-bar handle:
    HWND m_MenuBarHandle;

    // Test thread:
    AuthMat_t *m_pAuthMat;
    
public:

    // Constructor and destructor:
    MyWinMain_t(void);
    virtual
   ~MyWinMain_t(void);

    // Initializes the program:
    virtual DWORD
    Init(
        IN int    argc,
        IN TCHAR *argv[]);

    // Starts the test thread and runs the window-message loop:
    virtual DWORD
    Run(void);
    
protected:

    // Displays the program's command-line arguments:
    virtual void
    DoPrintUsage(void) const;
    
    // Creates the menu bars:
    virtual DWORD
    DoCreateMenuBars(void);
    
    // Creates the keyboard accelerator table:
    virtual DWORD
    DoCreateAccelerators(
        OUT HACCEL *pAccel);

    // Processes window messages:
    virtual LRESULT
    DoWindowMessage(
        IN HWND   hWnd,
        IN UINT   uMsg,
        IN WPARAM wParam,
        IN LPARAM lParam);
};

// ----------------------------------------------------------------------------
//
// Constructor.
//
MyWinMain_t::
MyWinMain_t(void)
    : NetWinMain_t(),
      m_MenuBarHandle(NULL),
      m_pAuthMat(NULL)
{
    Utils::StartupInitialize();
}

// ----------------------------------------------------------------------------
//  
// Destructor.
//
MyWinMain_t::
~MyWinMain_t(void)
{
    // Halt the test thread if it's still running.
    if (NULL != m_pAuthMat)
    {
        m_pAuthMat->Stop(5*60*1000L);  // Wait at most 5 minutes
        AuthMat_t::DeleteInstance();
    }

    Utils::ShutdownCleanup();
}

// ----------------------------------------------------------------------------
//
// Initializes the program.
//
DWORD
MyWinMain_t::
Init(
    IN int    argc,
    IN TCHAR *argv[])
{
    DWORD result;
    
    // Initialize winsock.
    result = StartupWinsock(2,2);
    if (NO_ERROR != result)
        return result;

    // Let the base-class do its thing.
    result = NetWinMain_t::Init(argc, argv);
    if (NO_ERROR != result)
        return result;

    // Initialize the test thread.
    m_pAuthMat = AuthMat_t::GetInstance(this);
    if (NULL == m_pAuthMat)
    {
        ShowErrorDialog(TEXT("Can't allocate AuthMat object"));
        return ERROR_OUTOFMEMORY;
    }

    ce::tstring errors;
    ErrorLock errorLock(&errors);
    result = m_pAuthMat->Init(argc, argv);
    errorLock.StopCapture();  
    
    if (NO_ERROR != result)
    {
        ShowErrorDialog(TEXT("%s"), &errors[0]);
        return result;
    }

    return result;
}
    
// ----------------------------------------------------------------------------
//
// Starts the test thread and runs the window-message loop.
//
DWORD
MyWinMain_t::
Run(void)
{
    DWORD result;
    
    // Start the test thread.
    ce::tstring errors;
    ErrorLock errorLock(&errors);
    result = m_pAuthMat->Start(GetInterruptHandle());
    errorLock.StopCapture();

    if (NO_ERROR != result)
    {
        ShowErrorDialog(TEXT("%s"), &errors[0]);
        return result;
    }

    // Run the window-message loop.
    result = NetWinMain_t::Run();
    if (NO_ERROR != result)
        return result;
        
    // Determine whether the program was halted normally or due to an
    // external signal. Do this before terminating the tests, because
    // that will set the signal.
    bool interrupted = IsInterrupted();
    
    // Tell the tests to stop (if they haven't already).
    m_pAuthMat->Wait(100);
    
    // If the program wasn't interrupted, log the final results.
    if (!interrupted)
    {
        ce::tstring status;
        TCHAR timeBuffer[30];
        Utils::FormatRunTime(GetStartTime(), timeBuffer, COUNTOF(timeBuffer));
        
        WiFUtils::FmtMessage(&status, TEXT("%hs after %s"),
                             m_pAuthMat->IsFinished()? "Finished" 
                                                     : "Stopped", timeBuffer);

        long totalTests = m_pAuthMat->GetTestsPassed()
                        + m_pAuthMat->GetTestsSkipped()
                        + m_pAuthMat->GetTestsFailed()
                        + m_pAuthMat->GetTestsAborted();
        if (totalTests)
        {
            WiFUtils::AddMessage(&status, TEXT(": ran %ld tests"),
                                 totalTests);
        }
        if (m_pAuthMat->GetTestsPassed())
        {
            WiFUtils::AddMessage(&status, TEXT(", %ld passed"), 
                                 m_pAuthMat->GetTestsPassed());
        }
        if (m_pAuthMat->GetTestsSkipped())
        {
            WiFUtils::AddMessage(&status, TEXT(", %ld skipped"), 
                                 m_pAuthMat->GetTestsSkipped());
        }
        if (m_pAuthMat->GetTestsFailed())
        {
            WiFUtils::AddMessage(&status, TEXT(", %ld failed"), 
                                 m_pAuthMat->GetTestsFailed());
        }
        if (m_pAuthMat->GetTestsAborted())
        {
            WiFUtils::AddMessage(&status, TEXT(", %ld aborted"), 
                                 m_pAuthMat->GetTestsAborted());
        }

        LogStatus(TEXT("%s"), &status[0]);
        MessageBox(GetMainWindowHandle(), status, 
                   gszMainProgName, MB_OK|MB_ICONINFORMATION);
    }

    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Displays the program's command-line arguments.
//
void
MyWinMain_t::
DoPrintUsage(void) const
{
    LogAlways(TEXT("++Authentication Matrix Tests++"));
    LogAlways(TEXT("%s.exe [options]"), gszMainProgName);
    LogAlways(TEXT("\nOptions:"));
    LogAlways(TEXT("-?      show this information"));
    NetWinMain_t::DoPrintUsage();
    AuthMat_t::PrintUsage();
}

// ----------------------------------------------------------------------------
//
// Creates the menu bars.
//
DWORD
MyWinMain_t::
DoCreateMenuBars(void)
{
    SHMENUBARINFO menuBar = {0};
    
    menuBar.cbSize     = sizeof(SHMENUBARINFO);
    menuBar.hwndParent = GetMainWindowHandle();
    menuBar.nToolBarId = IDR_MAIN_CMDBAR;
    menuBar.hInstRes   = GetProgramInstance();
    
    if (SHCreateMenuBar(&menuBar))
    {
        m_MenuBarHandle = menuBar.hwndMB;
        return NO_ERROR;
    }
    else
    {
        DWORD result = GetLastError();
        LogWarn(TEXT("Failure in SHCreateMenuBar: %s"),
                Win32ErrorText(result));
        return NO_ERROR;
     // return result;
    }
}

// ----------------------------------------------------------------------------
//
// Creates the keyboard accelerator table.
//
DWORD
MyWinMain_t::
DoCreateAccelerators(
    OUT HACCEL *pAccel)
{
    assert(NULL != pAccel);
    
   *pAccel = ::LoadAccelerators(GetProgramInstance(),
                                MAKEINTRESOURCE(IDR_AUTHMAT_ACCEL));

    if (NULL != *pAccel)
    {
        return NO_ERROR;
    }
    else
    {
        DWORD result = GetLastError();
        LogWarn(TEXT("Can't load accellerators: %s"),
                Win32ErrorText(result));
        return NO_ERROR;
     // return result;
    }
}

// ----------------------------------------------------------------------------
//
// Processes window messages.
// The default implementation only handles WM_DESTROY messages.
//
LRESULT
MyWinMain_t::
DoWindowMessage(
    IN HWND   hWnd,
    IN UINT   uMsg,
    IN WPARAM wParam,
    IN LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_PAINT:
            {
                PAINTSTRUCT Paint;
                AuthMat_t::GetInstance()->Draw(hWnd, BeginPaint(hWnd, &Paint));
                EndPaint(hWnd, &Paint);
            }
            break;

        case WM_TIMER:
            {
                HDC hdc = GetDC(hWnd);
				AuthMat_t::GetInstance()->Draw(hWnd, hdc);
                ReleaseDC(hWnd, hdc);
            }
            break;

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDM_APP_EXIT:
                {
                    LogStatus(TEXT("Terminating"));
                    PostQuitMessage(0);
                    break;
                }
                break;

                case IDM_APP_START_STOP:
                {
                    TBBUTTONINFO buttonInfo = {0};
                    TCHAR        buttonText[100] = {0};

                    // Get the current state of the button.
                    buttonInfo.cbSize = sizeof(buttonInfo);
                    buttonInfo.dwMask = TBIF_TEXT;
                    buttonInfo.pszText = buttonText;
                    buttonInfo.cchText = COUNTOF(buttonText);

                    ::SendMessage(m_MenuBarHandle, TB_GETBUTTONINFO,
                                  IDM_APP_START_STOP, (LPARAM)&buttonInfo);

                    if (0 == _tcscmp(TEXT("Start"), buttonInfo.pszText)) // Start
                    {
                        // Start the test.

                        // Change the menu text
                        LoadString(GetProgramInstance(), IDS_APP_STOP,
                                   buttonInfo.pszText, COUNTOF(buttonText));
                    }
                    else // Stop
                    {
                        // Halt the test.

                        // Change the menu text
                        LoadString(GetProgramInstance(), IDS_APP_START,
                                   buttonInfo.pszText, COUNTOF(buttonText));
                    }

                    ::SendMessage(m_MenuBarHandle, TB_SETBUTTONINFO,
                                  IDM_APP_START_STOP, (LPARAM)&buttonInfo);

                    // Update the display
                    HDC hdc = GetDC(hWnd);
                    AuthMat_t::GetInstance()->Draw(hWnd, hdc);
                    ReleaseDC(hWnd, hdc);
                }
                break;

            }
            break;
            
        default:
            return NetWinMain_t::DoWindowMessage(hWnd, uMsg, wParam, lParam);
    }

    return 0;
}

// ----------------------------------------------------------------------------
//
// Shows status messages at the bottom of the display.
//
static void
ForwardStatus(const TCHAR *pMessage)
{
    AuthMat_t::GetInstance()->SetStatus(TEXT("%s"), pMessage);
}
 
// ----------------------------------------------------------------------------
//
// Called by netmain after basic arguments are filtered and NetLog has
// been initialized.
//
int
mymain(
    IN int    argc,
    IN TCHAR *argv[])
{
    DWORD result = NO_ERROR;
    
    // Initialize the program.
    MyWinMain_t *pWinMain = new MyWinMain_t;
    if (NULL == pWinMain)
        result = ERROR_OUTOFMEMORY;
    else
    {
        result = pWinMain->Init(argc, argv);
        if (NO_ERROR == result)
        {
            // Send status messages to the display.
            SetStatusLogger(ForwardStatus);
            LogStatus(TEXT("Initializing"));

            // Main message loop.
            result = pWinMain->Run();

            // Quit displaying status reports.
            ClearStatusLogger();
        }
        delete pWinMain;
    }
    
    return result;
}

// ----------------------------------------------------------------------------
