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
//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************
/*-------------------------------------------------------------------------------


  Module Name:

    mthread.cpp

  Abstract:

    Implement Mini Thread object.

  Notes:

-----------------------------------------------------------------------------*/
#include <windows.h>
#include "usbstress.h"
#include "mthread.h"

MiniThread::MiniThread(DWORD dwStackSize, int iPriority, BOOL bSuspended)
{
    bTerminated = FALSE;
    exitCode = (DWORD) - 1;
    threadHandle =::CreateThread(NULL, dwStackSize, MiniThread::ThreadProc, (LPVOID) this, bSuspended ? CREATE_SUSPENDED : 0, &threadId);
    if(threadHandle)
        ::SetThreadPriority(threadHandle, iPriority);
    suspendFlag = bSuspended;
};

MiniThread::~MiniThread()
{
    ForceTerminated();
}

BOOL MiniThread::ForceTerminated()
{
    if(threadHandle) {
        BOOL bReturn =::TerminateThread(threadHandle, (DWORD) - 1);    // terminate abormal
        exitCode = (DWORD)-1;
        ::CloseHandle(threadHandle);
        threadHandle = NULL;
        return bReturn;
    };
    return TRUE;
}

BOOL MiniThread::ThreadStart()
{
    syncObject.Lock();
    if(!threadHandle) {
        syncObject.Unlock();
        return FALSE;
    }

    if(suspendFlag) {
        if(ResumeThread(threadHandle) == (DWORD) (-1)) {    // failure for somehow
            syncObject.Unlock();
            return FALSE;
        };
        suspendFlag = FALSE;
    }
    syncObject.Unlock();
    return TRUE;
}

BOOL MiniThread::ThreadStop()
{
    syncObject.Lock();
    if(!threadHandle) {
        syncObject.Unlock();
        return FALSE;
    }

    if(!suspendFlag) {
        if(SuspendThread(threadHandle) == (DWORD) (-1)) {    // failure
            syncObject.Unlock();
            return FALSE;
        }
        suspendFlag = TRUE;
    }
    syncObject.Unlock();
    return TRUE;
}

BOOL MiniThread::ThreadTerminated(DWORD dwMilliseconds)
{
    bTerminated = TRUE;
    return WaitThreadComplete(dwMilliseconds);
}

BOOL MiniThread::WaitThreadComplete(DWORD dwMilliSeconds)
{
    DEBUGMSG(DBG_THREAD, (TEXT("WaitThreadComplete Wait Thread(%lx) for %ld ticks\n"), threadId, dwMilliSeconds));
    syncObject.Lock();
    if(!threadHandle) {
        syncObject.Unlock();
        DEBUGMSG(DBG_THREAD, (TEXT("WaitThreadComplete for locked Mutex\n")));
        return FALSE;
    };

    if(ThreadStart()) {
        if(::WaitForSingleObject(threadHandle, dwMilliSeconds) == WAIT_OBJECT_0) {    // thread dead
            ::CloseHandle(threadHandle);
            threadHandle = NULL;
            syncObject.Unlock();
            DEBUGMSG(DBG_THREAD, (TEXT("WaitThreadComplete for Completion\n")));
            return TRUE;
        } else {
            DEBUGMSG(DBG_THREAD, (TEXT("WaitThreadComplete Time Out\n")));
        }
    } else
        DEBUGMSG(DBG_THREAD, (TEXT("WaitThreadComplete for stoped thread\n")));
    syncObject.Unlock();
    return FALSE;
}

DWORD WINAPI MiniThread::ThreadProc(LPVOID dParam)
{
    MiniThread *threadPtr = (MiniThread *) dParam;
    threadPtr->exitCode = threadPtr->ThreadRun();
    ::ExitThread(threadPtr->exitCode);
    return threadPtr->exitCode;
};
