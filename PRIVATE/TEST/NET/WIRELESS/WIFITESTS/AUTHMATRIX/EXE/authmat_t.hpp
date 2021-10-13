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
// Definitions and declarations for the AuthMat_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_AuthMat_T_
#define _DEFINED_AuthMat_T_
#pragma once

#include <NetWinMain_t.hpp>

#include <inc/auto_xxx.hxx>
#include <tux.h>

namespace ce {
namespace qa {
    
// ----------------------------------------------------------------------------
//
// This is the actual AuthMat executable. The wmain function creates the
// singleton object then calls Init and Start. This starts the configured
// test(s) running in a sub-thread. While the tests are running the
// window's message-pump is expected to periodically call the DrawStatus
// method to update the display. Finally, if the program is killed, the
// Stop method terminates the currently-running test.
//
class AuthMat_t
{
private:

    // Thread call-back procedure:
    static DWORD WINAPI
    ThreadProc(LPVOID pParameter);
            
    // Runs the sub-thread:
    DWORD
    Run(void);
    
    // Main window object:
    NetWinMain_t *m_pWinMain;
    
    // Sub-thread handle and id:
    ce::auto_handle m_ThreadHandle;
    DWORD           m_ThreadId;

    // Interruption signal:
    HANDLE m_InterruptHandle;
    
    // Sub-thread's result-code:
    DWORD m_ThreadResult;
    
    // Synchronization object
    ce::critical_section m_Locker;

    // Function-table:
    FUNCTION_TABLE_ENTRY *m_pFunctionTable;
    
    // Current test:
    FUNCTION_TABLE_ENTRY *m_pCurrentFTE;

    // Current stats:
    long m_TestsPassed;
    long m_TestsSkipped;
    long m_TestsFailed;
    long m_TestsAborted;
    bool m_Finished;

    // Status message:
    TCHAR m_Status[33];
    long  m_StatusTime;  // Time status-message added

    // Construction and destruction is handled by the Get/DeleteInstance:
    AuthMat_t(
        IN NetWinMain_t *pWinMain);
   ~AuthMat_t(void);
    AuthMat_t(const AuthMat_t &src);
    AuthMat_t &operator = (const AuthMat_t &src);
  
public:

    // Create and/or retreive the singleton instance:
    static AuthMat_t *
    GetInstance(
        IN NetWinMain_t *pWinMain = NULL);

    // Delete the singleton instance:
    static void
    DeleteInstance(void);

    // Displays the program's command-line arguments:
    static void
    PrintUsage(void);
    
    // Initialize the configuration from the specified command-line args:
    DWORD
    Init(
        IN int    argc,
        IN TCHAR *argv[]);

    // Starts the tests running in a sub-thread:
    DWORD
    Start(
        IN HANDLE InterruptHandle);

    // Determines whether the test thread is still running:
    bool
    IsAlive(void) const { return m_ThreadHandle.valid() != (BOOL)0; }
    
    // Waits the specified time then interrupts the test thread:
    // Returns immediately if the thread is already done.
    // If grace-time is non-zero, terminates the thread if it ignores
    // the interruption too long.
    DWORD
    Wait(
        IN long MaxWaitTimeMS, 
        IN long MaxGraceTimeMS = 0);

    // Interrupts the test thread:
    // If grace-time is non-zero, terminates the thread if it ignores
    // the interruption too long.
    DWORD
    Stop(
        IN long MaxGraceTimeMS = 0);

    // Gets the current overall test statistics:
    long
    GetTestsPassed(void) const { return m_TestsPassed; }
    long
    GetTestsSkipped(void) const { return m_TestsSkipped; }
    long
    GetTestsFailed(void) const { return m_TestsFailed; }
    long
    GetTestsAborted(void) const { return m_TestsAborted; }

    // Determines whether the tests are all finished:
    bool
    IsFinished(void) const { return m_Finished; }
                
    // Draws the current test status in the specified window using the
    // specified drawing context:
    void
    Draw(
        IN HWND hWnd,
        IN HDC  hdc);
    
    // Add a status-message to the last line of the display:
    void 
    SetStatus(
        IN const TCHAR *pFormat, ...);
    void 
    ClearStatus(void);
};


};
};

#endif /* _DEFINED_AuthMat_T_ */
// ----------------------------------------------------------------------------
