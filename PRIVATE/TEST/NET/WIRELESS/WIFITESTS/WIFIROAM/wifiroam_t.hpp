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
// Definitions and declarations for the WiFiRoam_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WiFiRoam_T_
#define _DEFINED_WiFiRoam_T_
#pragma once

#include "ConnStatus_t.hpp"
#include <NetWinMain_t.hpp>

#include <inc/auto_xxx.hxx>
#include <inc/sync.hxx>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// This is the actual WiFiRoam executable. The wmain function creates the
// singleton object then calls Init and Start. This starts the configured
// test(s) running in a sub-thread. While the tests are running the
// window's message-pump is expected to periodically call the DrawStatus
// method to update the display. Finally, if the program is killed, the
// Stop method terminates the currently-running test.
//
class NetlogAnalyser_t;
class WiFiRoam_t
{
private:

    // Thread call-back procedure:
    static DWORD WINAPI
    ThreadProc(LPVOID pParameter);
            
    // Called ever second or so by ThreadProc to ask Connection-Manager
    // the current connection status:
    HRESULT
    ConnectCMConnection(
        IN long RunDuration);

    // Main window object:
    NetWinMain_t *m_pWinMain;

    // Sub-thread handle and id:
    ce::auto_handle m_ThreadHandle;
    DWORD           m_ThreadId;

    // Interruption signal:
    HANDLE m_InterruptHandle;
    
    // Sub-thread's result-code:
    DWORD m_ThreadResult;
    
    // Synchronization object:
    ce::critical_section m_Locker;
    
    // Netlog WiFi packet analyser:
    NetlogAnalyser_t *m_pNetlogAnalyser;

    // Connection status:
    ConnStatus_t m_Status;

    // Minimum, maximum EAPOL roam-time, in milliseconds:
   _int64 m_MinEAPOLRoamTime;
   _int64 m_MaxEAPOLRoamTime;

    // Total EAPOL roam time, in milliseconds, and number roaming samples:
   _int64 m_TotalEAPOLRoamTime;
    long  m_TotalEAPOLRoamSamples;
    
    // Status message:
    TCHAR m_StatusMsg[33];
    long  m_StatusMsgTime;  // Time status-message added
    static const m_StatusMsgLifetime = 45; // Time, in seconds, messages live

    // Construction and destruction is handled by the Get/DeleteInstance:
    WiFiRoam_t(
        IN NetWinMain_t *pWinMain);
   ~WiFiRoam_t(void);

    // Copy and assignment are deliberately disabled:
    WiFiRoam_t(const WiFiRoam_t &src);
    WiFiRoam_t &operator = (const WiFiRoam_t &src);
  
public:

  // Test management:
  
    // Creates and/or retreives the singleton instance:
    static WiFiRoam_t *
    GetInstance(
        IN NetWinMain_t *pWinMain = NULL);

    // Deletes the singleton instance:
    static void
    DeleteInstance(void);

    // Displays the program's command-line arguments:
    static void
    PrintUsage(void);
    
    // Initializes the configuration from the specified command-line args:
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
    // Returns immediately if the thread is already finished.
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

  // Accessors:

    // Gets the current connection status information:
    const ConnStatus_t &
    GetStatus(void) const { return m_Status; }
    
    // Accumulates the specified EAPOL roam-time calculations:
    void
    AddEAPOLRoamTimes(
        IN _int64 MinRoamTime,
        IN _int64 MaxRoamTime,
        IN _int64 TotalRoamTime,
        IN  long  TotalRoamSamples);
  
  // User-Interface:

    // Draws the current test status in the specified window using the
    // specified drawing context:
    void
    Draw(
        IN HWND hWnd,
        IN HDC  hdc);
    
    // Add a status-message to the last line of the display:
    void 
    SetStatusMsg(
        IN const TCHAR *pFormat, ...);
    void 
    ClearStatusMsg(void);
};

};
};

#endif /* _DEFINED_WiFiRoam_T_ */
// ----------------------------------------------------------------------------
