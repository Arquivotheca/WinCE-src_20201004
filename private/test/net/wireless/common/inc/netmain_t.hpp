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
// Definitions and declarations for the NetMain_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_NetMain_t_
#define _DEFINED_NetMain_t_
#pragma once

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#ifdef UNDER_CE
#include <winsock2.h>
#endif

#include <windows.h>
#include <netmain.h>
#include <string.hxx>

// ----------------------------------------------------------------------------
//
// tstring: A generic Unicode or ASCII dynamic-string class:
//
namespace ce {
#ifndef CE_TSTRING_DEFINED
#define CE_TSTRING_DEFINED
#ifdef UNICODE
typedef ce::wstring tstring;
#else
typedef ce::string  tstring;
#endif
#endif
namespace qa {

// ----------------------------------------------------------------------------
//
// NetMain is an abstract base class providing an interface for ues by
// console applications. Users derive from this class and re-implement
// the virtual methods required to customize the "winmain" behavior of
// their application.
//
// NetMain_t is an instance a Design Patterns [Gamma, et al] "Template"
// pattern. It abstacts the code common to all console applications and 
// implements it one time. At the same time it provides a way for users
// to customize its behavior by implementing the common tasks performed
// by a console application as a series of virtual "DoSomething" methods
// which can be overridden and/or extended as necessary.
//
// Example Usage:
//
//  #include <NetMain_t.hpp>
//
//  // External variables for NetMain:
//  WCHAR *gszMainProgName = L"APCTool";
//  DWORD  gdwMainInitFlags = 0;
//  
//  class MyNetMain_t : public NetMain_t
//  {
//  private:
//      int m_PrivateStuff;
//  public:
//      virtual DWORD
//      Run(void)
//      {
//          DWORD result = StartupWinsock(2,2);
//          if (NO_ERROR == result)
//          {
//              while (!IsInterrupted())
//                  ... do whatever ...
//              result = CleanupWinsock();
//          }
//          return result;
//      }
//  };
//
//  int
//  mymain(         <== called by netmain
//      int    argc,
//      TCHAR *argv[])
//  {
//      MyNetMain_t realMain;
//      DWORD result = realMain.Init(argc,argv);
//      if (NO_ERROR == result)
//          result = realMain.Run();
//      return int(result);
//  }
//
class NetMain_t
{
private:

    // Program start time:
    long m_StartTime;

    // Process handle:
    HINSTANCE m_ProgInstance;

    // Interruption event:
    HANDLE m_InterruptHandle;

    // Number times winsock has been initialized:
    int m_NumberWinsockStartups;

    // Original command-line:
    mutable ce::tstring m_CommandLine;
    mutable bool        m_CommandLineAssembled;

    // Copy and assignment are deliberately disabled:
    NetMain_t(const NetMain_t &src);
    NetMain_t &operator = (const NetMain_t &src);

public:

    // Constructor and destructor:
    NetMain_t(void);
    virtual
   ~NetMain_t(void);

    // Accessors:
    long
    GetStartTime(void) const { return m_StartTime; }
    HINSTANCE
    GetProgramInstance(void) const { return m_ProgInstance; }
    HANDLE
    GetInterruptHandle(void) const { return m_InterruptHandle; }

    int 
    GetCommandArgc(void) const;
    LPTSTR *
    GetCommandArgv(void) const;
    const TCHAR *
    GetCommandLine(void) const;
    
    // Initializes the program:
    // Returns ERROR_ALREADY_EXISTS if the program is already running.
    virtual DWORD
    Init(
        int    argc,
        TCHAR *argv[]);

    // This method must be implemented by derived classes to implement
    // the program executable:
    virtual DWORD
    Run(void) = 0;
    
    // Interrupts the program by setting the termination event:
    virtual void
    Interrupt(void);
    
    // Determines whether the program has received a termination event:
    bool
    IsInterrupted(void) const;

    // Clears the termination event:
    void
    ClearInterrupt(void);
    
  // Utility methods:

    // Initializes or cleans up winsock:
    // Produces the appropriate error messages on failure.
    DWORD
    StartupWinsock(
        int MinorVersion,
        int MajorVersion);
    DWORD
    CleanupWinsock(void);
    
protected:

    // Displays the program's command-line arguments:
    // The default implementation dumps a list of the standard
    // netmain arguments.
    virtual void
    DoPrintUsage(void);
    
    // Parses the program's command-line arguments:
    // The default implementation does nothing.
    virtual DWORD
    DoParseCommandLine(
        int    argc,
        TCHAR *argv[]);

    // Determines if an instance of the program is already running:
    // Returns ERROR_ALREADY_EXISTS if so.
    // If necessary, creates the program-termination event.
    virtual DWORD
    DoFindPreviousInstance(void);

    // If there's already an existing instance, asks the user whether
    // they want to kill it and, if so, signals an interrupt:
    // If the optional "question" string is supplied, asks that rather
    // than the standard "Already running. Kill it?".
    virtual void
    DoKillPreviousInstance(
        const TCHAR *pKillPreviousQuestion = NULL);
};

};
};
#endif /* _DEFINED_NetMain_t_ */
// ----------------------------------------------------------------------------
