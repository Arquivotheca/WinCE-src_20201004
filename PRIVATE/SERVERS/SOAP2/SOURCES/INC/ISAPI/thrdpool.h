//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
