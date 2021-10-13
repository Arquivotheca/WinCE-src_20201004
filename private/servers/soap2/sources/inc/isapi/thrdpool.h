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
//+---------------------------------------------------------------------------------
//
//
// File:
//      thrdpool.h
//
// Contents:
//
//      thread pooling support
//
//----------------------------------------------------------------------------------

#ifndef THRDPOOL_H_INCLUDED
#define THRDPOOL_H_INCLUDED

enum
{
    EVENT_SHUTDOWN = 0,
    EVENT_WORKERQUEUE,
    EVENT_LAST
};

enum
{
    NUM_WORKERTHREADS = 5
};

typedef struct _THREADINFO
{
    DWORD           dwThreadID;
    CObjCache *  pObjCache;
} THREADINFO, *PTHREADINFO;

typedef struct _RequestInfo
{
#ifdef UNDER_CE
	HANDLE hFinishedProcessing;
#endif 
    void * m_pvContext;
    void * m_pvThreadInfo;
} REQUESTINFO, *PREQUESTINFO;

typedef struct _THREADPOOL
{
    LONG                cThreads;
    HANDLE *            phThread;
    THREADINFO *        pThreadInfo;
    HANDLE              hShutdownEvent;
    HANDLE              rghEvents[EVENT_LAST];
    PWORKERQUEUE        pWorkerQueue;
} THREADPOOL, *PTHREADPOOL;

extern
DWORD
InitializeThreadPool
    (
    IN DWORD            dwThreadCount,
    IN DWORD            dwObjCachePerThread
    );

extern
DWORD
TerminateThreadPool
    (
    VOID
    );

#endif
