//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
// 
// File:    workers.cpp
// 
// Contents:
//
//  Implements support code for use by worker threads of extension agent.
//
//
//-----------------------------------------------------------------------------
#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif 
#include "isapihdr.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: InitializeWorkerQueue ( IN PWORKERQUEUE pQueue )
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
extern DWORD  InitializeWorkerQueue ( IN PWORKERQUEUE pQueue )
{
    if ( pQueue == NULL )
    {
        return ERROR_INVALID_PARAMETER;
    }

    pQueue->hEnqueueEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    if ( pQueue->hEnqueueEvent == NULL )
    {
        return GetLastError();
    }

    InitializeCriticalSection( &pQueue->CriticalSection );

    pQueue->dwSize = 0;

    InitializeListHead( &(pQueue->ListHead ) );

    pQueue->fInitialized = TRUE;
    g_fWorkerQueueInitialized = TRUE;

    return ERROR_SUCCESS;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: WorkerEnqueue(IN PWORKERQUEUE pQueue, IN PWORKERITEM  pItem)
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
extern  DWORD  WorkerEnqueue(IN PWORKERQUEUE pQueue, IN PWORKERITEM  pItem)
{
    if ( ( pQueue == NULL ) || ( pItem == NULL ) )
    {
        return ERROR_INVALID_PARAMETER;
    }

    EnterCriticalSection( &(pQueue->CriticalSection) );

    pQueue->dwSize++;

    InsertTailList( &(pQueue->ListHead), &(pItem->ListEntry) );

    LeaveCriticalSection( &(pQueue->CriticalSection) );

    SignalQueueChanges(pQueue);

    return ERROR_SUCCESS;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: WorkerDequeue (IN PWORKERQUEUE pQueue, OUT PWORKERITEM * ppItem)
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
extern  DWORD WorkerDequeue (IN PWORKERQUEUE pQueue, OUT PWORKERITEM * ppItem)
{
    DWORD                   dwError = ERROR_SUCCESS;
    
    if ( ( pQueue == NULL ) || ( ppItem == NULL ) )
    {
        return ERROR_INVALID_PARAMETER;
    }

    *ppItem = NULL;

    EnterCriticalSection( &(pQueue->CriticalSection) );

    if ( pQueue->dwSize == 0 )
    {
        dwError = ERROR_NO_DATA;
    }
    else
    {
        *ppItem = CONTAINING_RECORD( pQueue->ListHead.Flink,
                                     WORKERITEM,
                                     ListEntry );

        RemoveHeadList( &(pQueue->ListHead) );

        pQueue->dwSize--;
    }

    LeaveCriticalSection( &(pQueue->CriticalSection ) );

    return dwError;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:TerminateWorkerQueue ( IN PWORKERQUEUE pQueue )
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
extern DWORD  TerminateWorkerQueue ( IN PWORKERQUEUE pQueue )
{
    PWORKERITEM             pItem;
    
    if ( !g_fWorkerQueueInitialized ) {
        return ERROR_SUCCESS;
    }

    if ( pQueue == NULL )
    {
        return ERROR_INVALID_PARAMETER;
    }

    pQueue->fInitialized = FALSE;

    // remove all items from queue

    while ( WorkerDequeue(pQueue, &pItem ) == ERROR_SUCCESS)
    {
        FreeWorkerItem( pItem );
    }

    if ( pQueue->hEnqueueEvent != NULL )
    {
        CloseHandle( pQueue->hEnqueueEvent );
    }

    DeleteCriticalSection( &(pQueue->CriticalSection) );
    g_fWorkerQueueInitialized = FALSE;

    return ERROR_SUCCESS;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: SignalQueueChanges ( IN PWORKERQUEUE pQueue )
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
extern DWORD SignalQueueChanges ( IN PWORKERQUEUE pQueue )
{
    if ( pQueue == NULL )
    {
        return ERROR_INVALID_PARAMETER;
    }

    if ( !SetEvent( pQueue->hEnqueueEvent ) )
    {
        return GetLastError();
    }
    else
    {
        return ERROR_SUCCESS;
    }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CreateWorkerItem (IN PTHREADFUNC  pfnThreadFunc, IN PVOID  pvContext)
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
extern PWORKERITEM CreateWorkerItem (IN PTHREADFUNC  pfnThreadFunc, IN PVOID  pvContext)
{
    PWORKERITEM      pItem;
    
    pItem = (PWORKERITEM)new BYTE[sizeof(WORKERITEM)] ;
    if ( pItem == NULL )
    {
        return NULL;
    }

    //
    // Increment global object count
    //
    OBJECT_CREATED();

    pItem->pfnThreadFunc = pfnThreadFunc;
    pItem->pvContext = pvContext;
    pItem->hFinishedProcessing = CreateEvent(NULL, FALSE, FALSE, NULL);

    if(NULL == pItem->hFinishedProcessing)
    {
        ASSERT(FALSE);
        delete [] pItem;
        return 0;
    }


    return pItem;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: FreeWorkerItem (IN PWORKERITEM  pItem )
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
extern DWORD FreeWorkerItem (IN PWORKERITEM  pItem )
{
    if ( pItem == NULL )
    {
        return ERROR_INVALID_PARAMETER;
    }

    delete [] pItem ;

    //
    // decrement global object count
    //
    OBJECT_DELETED();

    return ERROR_SUCCESS;
}
