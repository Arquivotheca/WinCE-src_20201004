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
// File:    thrdpool.cpp
// 
// Contents:
//
//
//
//-----------------------------------------------------------------------------
//    Implements a collection (pool) of threads for use by the extension agent
//    to poll the statistics of the various computer in the unit of analysis.

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif 
#include "isapihdr.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: PoolThread(PVOID pArg)
//
//  parameters: 
//
//  description:
//      Arguments:
//          pArg - Points to THREADINFO structure for this thread
//      Return Value:
//          ERROR_SUCCESS if successful, else Win32 Error code
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD WINAPI  PoolThread ( PVOID pArg )
{
    DWORD        dwError;
    HRESULT      hr;
    PWORKERITEM  pItem;
    REQUESTINFO  requestinfo;
    
    if ( pArg == NULL )
    {
        return ERROR_INVALID_PARAMETER;
    }

    //
    // Ensure COM is initialized
    //

#ifndef UNDER_CE
    if (FAILED(hr = CoInitialize( NULL )))
#else
    if (FAILED(hr = CoInitializeEx(NULL,COINIT_MULTITHREADED)))
#endif 
    {
        return ERROR_NOT_READY;
    }

    for ( ; ; )
    {
        dwError = WaitForMultipleObjects( EVENT_LAST, g_pThreadPool->rghEvents, FALSE, INFINITE );
        if ( dwError == WAIT_FAILED )
        {
            break;
        }

        if ( dwError == WAIT_OBJECT_0 + EVENT_SHUTDOWN)
        {
            break;
        }
        else
        {
            while (WorkerDequeue(g_pThreadPool->pWorkerQueue, &pItem) == ERROR_SUCCESS)
            {
                requestinfo.m_pvContext = pItem->pvContext;
                requestinfo.m_pvThreadInfo = pArg;
#ifdef UNDER_CE
                requestinfo.hFinishedProcessing = pItem->hFinishedProcessing;
#endif 


                dwError = pItem->pfnThreadFunc( &requestinfo );

                FreeWorkerItem(pItem);
            }
        }
    }

    InterlockedDecrement(&g_pThreadPool->cThreads);

    CoUninitialize( );

    return ERROR_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: InitializeThreadPool(DWORD dwThreadCount, DWORD dwObjCachePerThread)
//
//  parameters: 
//
//  description:
//      Arguments:
//          dwThreadCount - Number of threads to create
//      Return Value:
//          ERROR_SUCCESS if successful, else Win32 Error code
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
extern DWORD InitializeThreadPool (DWORD dwThreadCount, DWORD dwObjCachePerThread)
{
    DWORD   dwCounter;
    HANDLE  hThread;
    DWORD   dwError = ERROR_SUCCESS;
    
    if ( dwThreadCount == 0 )
    {
        ASSERT( 0 );
        return ERROR_INVALID_PARAMETER;
    }

    g_pThreadPool = new THREADPOOL;

    if (g_pThreadPool == NULL)
    {
        return GetLastError();
    }
    memset((void *)(g_pThreadPool), 0, sizeof(THREADPOOL));

    //
    // Create a worker queue and initialize it
    //
    g_pThreadPool->pWorkerQueue = new WORKERQUEUE;

    if ( g_pThreadPool->pWorkerQueue == NULL )
    {
        return GetLastError();
    }
    memset((void *)(g_pThreadPool->pWorkerQueue), 0, sizeof(WORKERQUEUE));

    if (InitializeWorkerQueue(g_pThreadPool->pWorkerQueue) != ERROR_SUCCESS)
    {
        delete g_pThreadPool->pWorkerQueue;
        g_pThreadPool->pWorkerQueue = NULL;
        return GetLastError();
    }

    g_pThreadPool->hShutdownEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    if ( g_pThreadPool->hShutdownEvent == NULL )
    {
        goto Error;
    }

    g_pThreadPool->rghEvents[EVENT_SHUTDOWN]    = g_pThreadPool->hShutdownEvent;
    g_pThreadPool->rghEvents[EVENT_WORKERQUEUE] = g_pThreadPool->pWorkerQueue->hEnqueueEvent;

    g_pThreadPool->phThread = new HANDLE[dwThreadCount];
    if ( g_pThreadPool->phThread == NULL )
    {
        goto Error;
    }
    memset((void *)(g_pThreadPool->phThread), 0, sizeof(HANDLE) * dwThreadCount);

    g_pThreadPool->pThreadInfo = new THREADINFO[dwThreadCount];
    if ( g_pThreadPool->pThreadInfo == NULL )
    {
        goto Error;
    }
    memset((void *)(g_pThreadPool->pThreadInfo), 0, sizeof(THREADINFO) * dwThreadCount);
    for ( dwCounter = 0; dwCounter < dwThreadCount; dwCounter++ )
    {
        hThread = CreateThread( NULL,
                                0,
                                PoolThread,
                                (PVOID) (&g_pThreadPool->pThreadInfo[ dwCounter ]),
                                0,
                                &g_pThreadPool->pThreadInfo[ dwCounter ].dwThreadID );
        if ( hThread == NULL )
        {
            dwError = GetLastError();
            goto Cleanup;
        }
        g_pThreadPool->pThreadInfo[dwCounter].pObjCache =
                new CObjCache((ULONG) dwObjCachePerThread,
                            (ULONG) g_pThreadPool->pThreadInfo[ dwCounter ].dwThreadID,
                            ThdCacheOKForAccess,
                            ThdCacheDelete,
                            ThdCacheGetKey);
        if (g_pThreadPool->pThreadInfo[dwCounter].pObjCache == NULL)
        {
            dwError = E_OUTOFMEMORY;
            goto Cleanup;
        }
        
        g_pThreadPool->phThread[dwCounter] = hThread;

        InterlockedIncrement( (LPLONG) &(g_pThreadPool->cThreads) );
    }

    g_fThreadPoolInitialized = TRUE;

Cleanup:
    if ( dwError != ERROR_SUCCESS )
    {
        TerminateThreadPool();
    }
    return dwError;
Error:
    dwError = GetLastError();
    goto Cleanup;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: TerminateThreadPool(VOID)
//
//  parameters: 
//
//  description:
//      Arguments:
//          None
//      Return Value:
//          ERROR_SUCCESS if successful, else Win32 Error code
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
extern  DWORD TerminateThreadPool(VOID)
{
    EnterCriticalSection(&g_cs);

    if (g_fThreadPoolInitialized)
    {
        DWORD dwCounter;
        DWORD cThreads;

        cThreads = g_pThreadPool->cThreads;

        SetEvent( g_pThreadPool->hShutdownEvent );

        TRACEL((2,"Waiting for all pool threads to shutdown...\n"));

#ifndef UNDER_CE
        WaitForMultipleObjects(cThreads, g_pThreadPool->phThread, TRUE, INFINITE );
#else
        //CE doesnt have a wait for multipleobjects
        for(DWORD i=0; i<cThreads; i++)
        {
            #ifdef DEBUG
            DWORD ret = 
            #endif
            WaitForSingleObject(g_pThreadPool->phThread[i], INFINITE);

            ASSERT(ret != WAIT_FAILED); 
        }
#endif 

        for ( dwCounter = 0; dwCounter < cThreads; dwCounter++ )
        {
            CObjCache *  pObjCache;
            pObjCache = g_pThreadPool->pThreadInfo[ dwCounter ].pObjCache;
            if (pObjCache)
            {
#ifndef UNDER_CE
                try
#else
                __try    
#endif 
                {
                    pObjCache->DeleteAll();
                }                
#ifndef UNDER_CE
                catch (...)
#else
                __except(1)
#endif 
                {
                    // We got an AV while releasing the objects in the
                    // cache. This happens when the DLLs we are holding a
                    // reference to have been unloaded before our DLL.
                    // Skip this one and continue releasing others
                    TRACEL((2,"Got an Access Violation Exception during thread shutdown...\n"));
                }
                delete pObjCache;
            }    
            CloseHandle(g_pThreadPool->phThread[dwCounter]);
        }

        TRACEL((2,"Wait finished.  All threads shutdown.\n"));
    }

    CloseHandle( g_pThreadPool->hShutdownEvent );

    TerminateWorkerQueue( g_pThreadPool->pWorkerQueue );

    delete g_pThreadPool->pWorkerQueue;
    delete [] g_pThreadPool->phThread;
    delete [] g_pThreadPool->pThreadInfo;
    delete g_pThreadPool;
    g_pThreadPool = NULL;
    g_fThreadPoolInitialized = FALSE;

    LeaveCriticalSection(&g_cs);

    return ERROR_SUCCESS;
}
