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
//      workers.h
//
// Contents:
//
//      worker thread pooling support
//
//----------------------------------------------------------------------------------

#ifndef WORKERS_H_INCLUDED
#define WORKERS_H_INCLUDED

typedef DWORD ( *PTHREADFUNC )( IN PVOID pContext );

typedef struct _WORKERQUEUE
{
    //DECLARE_MEMCLEAR_NEW_DELETE()

    LIST_ENTRY          ListHead;
    CRITICAL_SECTION    CriticalSection;
    DWORD               dwSize;
    HANDLE              hEnqueueEvent;
    BOOL                fInitialized;
} WORKERQUEUE, *PWORKERQUEUE;

typedef struct _WORKERITEM
{
    //DECLARE_MEMCLEAR_NEW_DELETE()
#ifdef UNDER_CE
	HANDLE hFinishedProcessing;
#endif 
    LIST_ENTRY          ListEntry;
    PTHREADFUNC         pfnThreadFunc;
    VOID *              pvContext;
} WORKERITEM, *PWORKERITEM;

#define IsWorkerQueueEmpty( x )         ((((PWORKERQUEUE)x)->dwSize)==0)
#define IsWorkerQueueInitialized( x )   (((PWORKERQUEUE)x)->fInitialized)
#define GetWorkerContext( x )           (((PWORKERITEM)x)->pvContext)

extern
DWORD
InitializeWorkerQueue
    (
    IN PWORKERQUEUE pQueue
    );

extern
DWORD
WorkerEnqueue
    (
    IN PWORKERQUEUE     pQueue,
    IN PWORKERITEM      pItem
    );

extern
DWORD
WorkerDequeue
    (
    IN PWORKERQUEUE     pQueue,
    OUT PWORKERITEM *   ppItem
    );

extern
DWORD
TerminateWorkerQueue
    (
    IN PWORKERQUEUE     pQueue
    );

extern
DWORD
SignalQueueChanges
    (
    IN PWORKERQUEUE     pQueue
    );

extern
PWORKERITEM
CreateWorkerItem
    (
    PTHREADFUNC         pfnThreadFunc,
    IN PVOID            pvContext
    );

extern
DWORD
FreeWorkerItem
    (
    IN PWORKERITEM      pItem
    );
#endif
