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
// Definitions and declarations for the NetWinMain_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_NetWinMain_t_
#define _DEFINED_NetWinMain_t_
#pragma once

#include <NetMain_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// NetWinMain derives from NetMain to provide an abstract base class
// defining an interface for ues by the "mymain" functions of Windows GUI
// applications. Applications derive a descendant version of this class
// and implement those virtual methods required to cusomize its behavior
// for their purposes.
//
// In order to support a GUI app, this class adds the DoCreateMainWindow
// and other GUI-initialization methods. In addition, the class implements
// a basic Run method which runs the Windows message-pump.
// 
// NetWinMain_t is an instance a Design Patterns [Gamma, et al] "Template"
// pattern. It abstacts the code common to all Windows GUI applications and 
// implements it one time. At the same time it provides a way for users
// to customize its behavior by implementing the common tasks performed
// by a Windows GUI application as a series of virtual "DoSomething"
// methods which can be overridden and/or extended as necessary.
//
// Example Usage:
//
//  class MyNetWinMain_t : public NetWinMain_t
//  {
//  private:
//      int m_PrivateInt;
//  public:
//      __override virtual DWORD
//      Run(void)
//      {
//          ... start the app running in a sub-thread ...
//          ... it should call KillMainWindow when it finishes ...
//
//          // Run the message-pump until interrupted or finished
//          DWORD result = NetWinMain_t::Run();
//          
//          ... if necessary terminate the app thread and show final stats ...
//          return result;
//      }
//  protected:
//      __override virtual LRESULT
//      DoWindowMessage(
//          IN HWND   hWnd,
//          IN UINT   uMsg,
//          IN WPARAM wParam,
//          IN LPARAM lParam)
//      {
//          switch (uMsg)
//          {
//              case WM_PAINT:
//                  {
//                      PAINTSTRUCT paint;
//                      HDC hdc = BeginPaint(hWnd, &paint);
//                      ... paint the window in hdc ...
//                      EndPaint(hWnd, &paint);
//                  }
//                  break;
//              case WM_TIMER:
//                  {
//                      HDC hdc = GetDC(hWnd);
//                      ... paint the window in hdc ...
//                      ReleaseDC(hWnd, hdc);
//                  }
//                  break;
//
//              ... other messages ...
//
//              default:
//                  return NetWinMain_t::DoWindowMessage(hWnd, uMsg, 
//                                                       wParam, lParam);
//          }
//          return 0;
//      }
//  };
//
//  int
//  mymain(         <== called by netmain
//      IN int    argc,
//      IN TCHAR *argv)
//  {
//      MyNetWinMain_t realMain;
//      DWORD result = realMain.Init(argc,argv);
//      if (NO_ERROR == result)
//          result = realMain.Run();
//      return int(result);
//  }
//
class NetWinMain_t : public NetMain_t
{
private:

    // Main window handle:
    HWND m_MainWindowHandle;
    
    // Keyboard accelerator table:
    HACCEL m_KbdAccelerators;
    
    // Main-window refresh interval:
    UINT m_RefreshInterval;
    UINT m_RefreshIntervalSaved;

public:

    // Constructor and destructor:
    NetWinMain_t(void);
    virtual
   ~NetWinMain_t(void);

    // Accessors:
    HWND
    GetMainWindowHandle(void) const { return m_MainWindowHandle; }
    HACCEL
    GetKeyboardAccelerators(void) const { return m_KbdAccelerators; }

    // Gets or sets the main-window refresh interval:
    UINT
    GetRefreshInterval(void) const { return m_RefreshInterval; }
    void
    SetRefreshInterval(UINT Millis) { m_RefreshInterval = Millis; }

    // Initializes the program:
    // Returns ERROR_ALREADY_EXISTS if the program is already running.
    __override virtual DWORD
    Init(
        IN int    argc,
        IN TCHAR *argv[]);

    // Runs the window-message loop:
    __override virtual DWORD
    Run(void);

    // Interrupts the program by sending a termination event to the
    // top-level window:
    void
    KillMainWindow(void);
    
  // Utility methods:
  
    // Generates an error message and shows it to the user in an error dialog:
    virtual void
    ShowErrorDialog(
        IN const TCHAR *pFormat, ...);
  
protected:
    
    // Determines if an instance of the program is already running:
    // Returns ERROR_ALREADY_EXISTS if so.
    // If necessary, creates the program-termination event and looks
    // up the handle of the existing program's main window.
    virtual DWORD
    DoFindPreviousWindow(
        IN const TCHAR *pWindowTitle = NULL);

    // If there's already an existing instance, asks the user whether
    // they want to kill it and, if so, signals an interrupt:
    // If the optional "question" string is supplied, asks that rather
    // than the standard "Already running. Kill it?".
    virtual void
    DoKillPreviousWindow(
        IN const TCHAR *pKillPreviousQuestion = NULL);

    // Registers and/or creates the main window class:
    virtual DWORD
    DoCreateMainWindow(
        OUT HWND *pMainWindowHandle);

    // Creates the menu bars (if any):
    virtual DWORD
    DoCreateMenuBars(void);
    
    // Creates the keyboard accelerator table (if any):
    virtual DWORD
    DoCreateAccelerators(
        OUT HACCEL *pAccelerators);

    // Processes window messages:
    // The default implementation only handles WM_DESTROY messages.
    virtual LRESULT
    DoWindowMessage(
        IN HWND   hWnd,
        IN UINT   uMsg,
        IN WPARAM wParam,
        IN LPARAM lParam);
  
private:

    // Default window procedure - passes messages to DoWindowMessage:
    static LRESULT CALLBACK
    WndProcCallback(
        IN HWND   hWnd,
        IN UINT   uMsg,
        IN WPARAM wParam,
        IN LPARAM lParam);
};

};
};
#endif /* _DEFINED_NetWinMain_t_ */
// ----------------------------------------------------------------------------
