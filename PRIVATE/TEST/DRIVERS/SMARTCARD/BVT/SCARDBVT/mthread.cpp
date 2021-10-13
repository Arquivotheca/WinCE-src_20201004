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

    mthread.cpp

  Abstract:

    Implement Mini Thread object.

  Notes:

-----------------------------------------------------------------------------*/
#include "stdafx.h"
#include "mthread.h"

MiniThread::MiniThread (DWORD dwStackSize, int iPriority, BOOL bSuspended)
{
    bTerminated=FALSE;
    exitCode=(DWORD)-1;
    threadHandle=::CreateThread(NULL,dwStackSize,MiniThread::ThreadProc,(LPVOID)this,
        bSuspended?CREATE_SUSPENDED:0,&threadId);
    if (threadHandle)
        ::SetThreadPriority(threadHandle,iPriority);
    suspendFlag=bSuspended;
};
MiniThread::~MiniThread()
{
    ForceTerminated();
}
BOOL MiniThread::ForceTerminated()
{
    if (threadHandle) {
        BOOL bReturn=::TerminateThread(threadHandle,(DWORD)-1); // terminate abormal
        exitCode=(DWORD)-1;
        ::CloseHandle(threadHandle);
        threadHandle=NULL;
        return bReturn;
    };
    return TRUE;
}

BOOL MiniThread::ThreadStart()
{
    syncObject.Lock();
    if (!threadHandle) {
        syncObject.Unlock();
        return FALSE;
    }

    if (suspendFlag) {
        if (ResumeThread( threadHandle)== (DWORD)(-1)) { // failure for somehow
            syncObject.Unlock();
            return FALSE;
        };
        suspendFlag=FALSE;
    }
    syncObject.Unlock();
    return TRUE;
}
BOOL MiniThread::ThreadStop()
{
    syncObject.Lock();
    if (!threadHandle) {
        syncObject.Unlock();
        return FALSE;
    }

    if (!suspendFlag) {
        if (SuspendThread(threadHandle) == (DWORD) (-1)) { // failure
            syncObject.Unlock();
            return FALSE;
        }
        suspendFlag=TRUE;
    }
    syncObject.Unlock();
    return TRUE;
}
BOOL MiniThread::ThreadTerminated(DWORD dwMilliseconds)
{
    bTerminated=TRUE;
    return WaitThreadComplete(dwMilliseconds);
}
BOOL MiniThread::WaitThreadComplete(DWORD dwMilliSeconds)
{
    syncObject.Lock();
    if (!threadHandle) {
        syncObject.Unlock();
        return FALSE;
    };

    if (ThreadStart()) {
        if (::WaitForSingleObject(threadHandle,dwMilliSeconds)==WAIT_OBJECT_0) {// thread dead
            ::CloseHandle(threadHandle);
            threadHandle=NULL;
            syncObject.Unlock();
            return TRUE;
        };
    };
    syncObject.Unlock();
    return FALSE;
}

DWORD WINAPI MiniThread::ThreadProc( LPVOID dParam)
{
    MiniThread * threadPtr= (MiniThread *)dParam;
    threadPtr->exitCode=threadPtr-> ThreadRun();
    ::ExitThread(threadPtr->exitCode);
    return threadPtr->exitCode;
};
    
