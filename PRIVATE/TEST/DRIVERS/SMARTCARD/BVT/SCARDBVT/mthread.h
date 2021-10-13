//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*-------------------------------------------------------------------------------


  Module Name:

    ft.h

  Description:
    Declare mini thread object

  -------------------------------------------------------------------------------*/
#include "SyncObj.h"
#ifndef MTHREAD_H
#define MTHREAD_H

class MiniThread  {

public :
    MiniThread (DWORD dwStackSize=0, int iPriority=THREAD_PRIORITY_NORMAL, BOOL bSuspended=FALSE);
    ~MiniThread();
    BOOL ThreadStart();
    BOOL ThreadStop();
    BOOL ThreadTerminated(DWORD dwMilliSeconds);
    BOOL WaitThreadComplete(DWORD dwMilliSeconds);
    BOOL SetThreadPriority(int iPriority) {
        ASSERT(threadHandle);
        return ::SetThreadPriority(threadHandle,iPriority);
    };
    BOOL ForceTerminated();

    DWORD GetThreadId() { return threadId; };
    BOOL IsTerminated() { return bTerminated; };
    HANDLE GetThreadHandle() { return threadHandle; };
    BOOL GetExitCodeThread(LPDWORD lpExitCode) {
        if (!threadHandle) {
            *lpExitCode=exitCode;
            return TRUE;
        }
        else
            return FALSE;
    };
    CMutex syncObject;
private:
    virtual DWORD ThreadRun()=0;
    static DWORD WINAPI ThreadProc( LPVOID dParam);

    BOOL bTerminated;
    HANDLE threadHandle;
    DWORD  threadId;
    BOOL suspendFlag;
    DWORD exitCode;
};

#endif
