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
//
//    deadlock.cpp - DDx deadlock diagnosis
//


#include "osaxs_p.h"
// Nkshx.h defines these, so get rid of them before including DwCeDump.h
#ifdef ProcessorLevel
#undef ProcessorLevel
#endif
#ifdef ProcessorRevision
#undef ProcessorRevision
#endif
#include "DwCeDump.h"
#include "diagnose.h"


#define DL_END          0
#define DL_CONTINUE     1
#define DL_ALERT        2
#define DL_WARNING      3
#define DL_SPINCHECK    4

#define HARD_DEADLOCKS      0
#define POSSIBLE_DEADLOCKS  1

#define MAX_PROXY_STACK     32


struct PROXY_STACK 
{
    UINT   tos;
    PROXY* item[MAX_PROXY_STACK];
}
g_proxyStack;



/*++

Routine Description:


Arguments:


Return Value:

--*/
LPCWSTR
GetProxyTypeString(
                    PROXY* pProxy
                    )
{
    if (pProxy)
    {
        switch(pProxy->bType)
        {
            case HT_CRITSEC:
                return L"critical section";
            case HT_MUTEX: 
                return L"mutex";
            case HT_MANUALEVENT:
                return L"event (manual)";
            case HT_EVENT: 
                return L"event";
            case HT_SEMAPHORE:
                return L"semaphore";
            case SH_CURPROC: 
                return L"process";
            case SH_CURTHREAD:
                return L"thread";
            default:
                break;
        }
    }

    return NULL;
}



/*++

Routine Description:


Arguments:


Return Value:

--*/
PTHREAD
GetProxyOwnerThread(
                    PROXY* pProxy
                    )
{
    PMUTEX   pCrit    = NULL;
    THREAD*  pthOwner = NULL;

    if (pProxy)
    {
        switch(pProxy->bType)
        {
            case HT_MUTEX:
                {
                    pCrit = (PMUTEX) pProxy->pObject;

                    if (pCrit)
                    {
                        pthOwner = (THREAD*) pCrit->pOwner;
                    }
                }
                return pthOwner;

            case HT_CRITSEC:
                {
                    pCrit = (PMUTEX) pProxy->pObject;

                    if (pCrit && pCrit->lpcs)
                    {
                        pthOwner = (THREAD*) pCrit->pOwner;
                    }
                }
                return pthOwner;

            case HT_MANUALEVENT:
            case HT_EVENT: 
            case HT_SEMAPHORE:
            case SH_CURPROC: 

                return pProxy->pTh;

            case SH_CURTHREAD:

                return (PTHREAD) pProxy->pObject;

            default:
                break;
        }
    }

    return NULL;
}


/*++

Routine Description:


Arguments:


Return Value:

--*/
void
DescribeProxy(
              PROXY* pProxy
              )
{
    if (pProxy)
    {
        switch(pProxy->bType)
        {
            case HT_MUTEX: 
                ddxlog(L"Mutex            : PROXY @ 0x%08x, MUTEX @ 0x%08x, curr pri: %d\r\n", 
                    pProxy, 
                    pProxy->pObject,
                    pProxy->prio
                    );
                return;

            case HT_CRITSEC:
                {
                    PMUTEX pCrit = (PMUTEX) pProxy->pObject;

                    ddxlog(L"Critical Section : PROXY @ 0x%08x, MUTEX @ 0x%08x, curr pri: %d, CRITICAL_SECTION @ 0x%08x\r\n",
                        pProxy, 
                        pProxy->pObject,  
                        pProxy->prio,
                        pCrit ? pCrit->lpcs : NULL
                        );
                }
                return;

            case HT_MANUALEVENT:
                ddxlog(L"Event (man reset): PROXY @ 0x%08x, EVENT @ 0x%08x, curr pri: %d\r\n", 
                    pProxy, 
                    pProxy->pObject, 
                    pProxy->prio
                    );
                return;

            case HT_EVENT: 
                ddxlog(L"Event            : PROXY @ 0x%08x, EVENT @ 0x%08x, curr pri: %d\r\n", 
                    pProxy, 
                    pProxy->pObject, 
                    pProxy->prio
                    );
                return;

            case HT_SEMAPHORE:
                {
                    PSEMAPHORE pSem = (PSEMAPHORE) pProxy->pObject;

                    ddxlog(L"Semaphore        : PROXY @ 0x%08x, SEMAPHORE @ 0x%08x, curr pri: %d, count: %d\r\n",  
                        pProxy, 
                        pProxy->pObject, 
                        pProxy->prio,
                        pSem ? pSem->lCount : 0
                        );
                }
                return;

            case SH_CURPROC:
                {
                    PPROCESS pProc = (PPROCESS) pProxy->pObject;

                    ddxlog(L"Process          : PROXY @ 0x%08x, PROCESS @ 0x%08x, curr pri: %d, ProcId: 0x%08x\r\n", 
                            pProxy, 
                            pProxy->pObject, 
                            pProxy->prio,
                            pProc ? pProc->dwId : 0
                            );
                }
                return;

            case SH_CURTHREAD:
                {
                    PTHREAD pTh = (PTHREAD) pProxy->pObject;

                    ddxlog(L"Thread           : PROXY @ 0x%08x, THREAD @ 0x%08x, curr pri: %d, ThreadId: 0x%08x\r\n", 
                            pProxy, 
                            pProxy->pObject, 
                            pProxy->prio,
                            pTh ? pTh->dwId : 0
                            );
                }
                return;

            default:
                ddxlog(L"Unknown Type     : PROXY @ 0x%08x, OBJECT @ 0x%08x, curr pri: %d\r\n", 
                    pProxy, 
                    pProxy->pObject, 
                    pProxy->prio
                    );
                return;
        }
    }

    ddxlog(L"ERROR: NULL proxy!\r\n");
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL 
IsKnownDeadlock(
                UINT start
                )
{
    PROXY*  p = NULL;
    THREAD* pthOwner = NULL;
   
    DDX_DIAGNOSIS_ID diag;

    diag.SubType = Type_Deadlock;
    diag.Type    = Deadlock_Circular;

    for (UINT i = 0/*start*/; i < (g_proxyStack.tos); i++)
    {
        p = g_proxyStack.item[i];

        pthOwner = GetProxyOwnerThread(p);

        diag.pProcess   = pthOwner ? pthOwner->pprcOwner : NULL;
        diag.dwThreadId = pthOwner ? pthOwner->dwId : NULL;
        diag.dwData     = (DWORD) p->pObject;

        if (IsKnownDiagnosis(&diag))
        {
            DEBUGGERMSG(OXZONE_DIAGNOSE, (L"OsAxsT0: Deadlock already recorded\n"));
            return TRUE;
        }
    }

    return FALSE;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL 
GenerateDiagnosis( 
                  UINT start
                  )
{
    PROXY*  p = NULL;
    THREAD* pthOwner = NULL;
    UINT    i;
    LPCWSTR pcwszProxy = NULL;
    LPCWSTR pcwszProc = NULL;
    HRESULT hr;
    CEDUMP_DIAGNOSIS_DESCRIPTOR ceDmpDiagnosis = {0};
    CEDUMP_BUCKET_PARAMETERS ceDmpBPs = {0};

    if (IsKnownDeadlock(start))
        return FALSE;

    BeginDDxLogging();

    ceDmpDiagnosis.Type       = Type_Deadlock;
    ceDmpDiagnosis.SubType    = Deadlock_Circular;
    ceDmpDiagnosis.Scope      = Scope_Unknown;
    ceDmpDiagnosis.Depth      = Depth_Cause;
    ceDmpDiagnosis.Severity   = Severity_Fatal;
    ceDmpDiagnosis.Confidence = Confidence_Certain;


    ddxlog(L"DEADLOCK DETECTED\n");
    ddxlog(L"\n");
    ddxlog(L"A circular contention for critical sections and/or mutexs was found.\n");
    ddxlog(L"\n");

    for (i = start; i < (g_proxyStack.tos); i++)
    {
        p = g_proxyStack.item[i];

        // Description

        pthOwner = GetProxyOwnerThread(p);
        pcwszProxy = GetProxyTypeString(p);
        pcwszProc = GetThreadProcName(pthOwner);

        AddAffectedThread(pthOwner);

        ddxlog(L"%s %s (PPROXY 0x%08x) owned by ...\n", ((i == start) ? L"--> " : L"  "), pcwszProxy, p);
        
        ddxlog(L"\n");
                        
        ddxlog(L"     %s thread 0x%08x blocked on ...\n", pcwszProc, pthOwner);

        ddxlog(L"\n");


        if (i == start)
        {
            hr = GetBucketParameters(&g_ceDmpDDxBucketParameters, pthOwner, pthOwner->pprcActv);

            if (FAILED(hr))
            {
                DEBUGGERMSG(OXZONE_DIAGNOSE, (L"Deadlock!GenerateDiagnosis: Failed to get bucket params (1), hr = 0x%08x\n", hr));
                return FALSE;
            }
        }
        else if (i == start + 1)
        {
            PPROCESS pprc = (pthOwner ? pthOwner->pprcOwner : NULL);

            ceDmpDiagnosis.pProcess  = (ULONG32) pprc;
            ceDmpDiagnosis.ProcessId = (ULONG32)(pprc ? pprc->dwId : NULL);
            ceDmpDiagnosis.pThread   = (ULONG32) pthOwner;

            hr = GetBucketParameters(&ceDmpBPs, pthOwner, pthOwner->pprcActv);

            if (FAILED(hr))
            {
                DEBUGGERMSG(OXZONE_DIAGNOSE, (L"Deadlock!GenerateDiagnosis: Failed to get bucket params (2), hr = 0x%08x\n", hr));
                return FALSE;
            }
            else
            {
                g_ceDmpDDxBucketParameters.ExParam[0].Type = BUCKET_PARAM_PROC_NAME;
                g_ceDmpDDxBucketParameters.ExParam[0].Data = ceDmpBPs.AppName;

                g_ceDmpDDxBucketParameters.ExParam[1].Type = BUCKET_PARAM_MOD_NAME;
                g_ceDmpDDxBucketParameters.ExParam[1].Data = ceDmpBPs.ModName;

                g_ceDmpDDxBucketParameters.ExParam[2].Type = BUCKET_PARAM_OFFSET;
                g_ceDmpDDxBucketParameters.ExParam[2].Data = ceDmpBPs.Offset;
            }
        }
        
        // Add persistent diagnosis

        DDX_DIAGNOSIS_ID diag;

        diag.Type       = Type_Deadlock;
        diag.SubType    = Deadlock_Circular;
        diag.pProcess   = pthOwner ? pthOwner->pprcOwner : NULL;
        diag.dwThreadId = pthOwner ? pthOwner->dwId : NULL;
        diag.dwData     = (DWORD) p->pObject;

        AddPersistentDiagnosis(&diag);

        // TODO: List all blocked threads??  Might be a perf issue ...
    }

    p = g_proxyStack.item[0];

    pcwszProxy = GetProxyTypeString(p);
    ddxlog(L"--> %s (PPROXY 0x%08x).\n", pcwszProxy, p);

    ddxlog(L"\n");
    ddxlog(L"\n");
    ddxlog(L"List of blocking object (proxies) involved in this deadlock\n");
    ddxlog(L"=============================================================\n");

    for (i = start; i < (g_proxyStack.tos); i++)
    {
        p = g_proxyStack.item[i];
        DescribeProxy(p);
    }
    ddxlog(L"\n");

    ceDmpDiagnosis.Description.DataSize = EndDDxLogging();

    return AddDiagnosis(&ceDmpDiagnosis);
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL 
OrphanWarning(
              PROXY* pCurrProxy, 
              PROXY* pNextProxy
              )
{
    THREAD* pthOwner = NULL;
    THREAD* pthOwnerNext = NULL;
    UINT    index = g_nDiagnoses;
    DDX_DIAGNOSIS_ID diag;

    
    pthOwner     = GetProxyOwnerThread(pCurrProxy);
    pthOwnerNext = GetProxyOwnerThread(pNextProxy);


    // Check to see if this is an instance of a known diagnosis

    diag.Type       = Type_Deadlock;
    diag.SubType    = Deadlock_AbandonedSyncObj;
    diag.pProcess   = pthOwner ? pthOwner->pprcOwner : NULL;
    diag.dwThreadId = pthOwner ? pthOwner->dwId : NULL;
    diag.dwData     = (DWORD) pCurrProxy->pObject;

    if (IsKnownDiagnosis(&diag))
        return FALSE;

    AddPersistentDiagnosis(&diag);


    // Record the diagnosis

    BeginDDxLogging();

    g_nDiagnoses++;
    
    PPROCESS pprc = (pthOwner ? pthOwner->pprcOwner : NULL);

    g_ceDmpDiagnoses[index].Type       = Type_Deadlock;
    g_ceDmpDiagnoses[index].SubType    = Deadlock_AbandonedSyncObj;
    g_ceDmpDiagnoses[index].Scope      = Scope_Unknown;
    g_ceDmpDiagnoses[index].Depth      = Depth_Symptom;
    g_ceDmpDiagnoses[index].Severity   = Severity_Severe;
    g_ceDmpDiagnoses[index].Confidence = Confidence_Possible;
    g_ceDmpDiagnoses[index].pProcess   = (ULONG32) pprc;
    g_ceDmpDiagnoses[index].ProcessId  = (ULONG32)(pprc ? pprc->dwId : NULL);
    g_ceDmpDiagnoses[index].pThread   = (ULONG32) pthOwner;
    
    
    ddxlog(L"Possible abandoned %s (PPROXY 0x%08x) in %s\n", 
                            GetProxyTypeString(pCurrProxy), 
                            pCurrProxy,
                            GetThreadProcName(pthOwner)
                            );

    AddAffectedThread(pthOwner);


    // Description - Root problem

    ddxlog(L"The %s thread 0x%08x owns the %s (PPROXY 0x%08x).\n",
                                    GetThreadProcName(pthOwner),
                                    pthOwner,
                                    GetProxyTypeString(pCurrProxy),
                                    pCurrProxy
                                    );

    ddxlog(L"\n");
    ddxlog(L"This thread is blocked on the %s (PPROXY 0x%08x).\n", 
                                    GetProxyTypeString(pNextProxy), 
                                    pNextProxy
                                    );

    ddxlog(L"\n");
    if (g_pKData && g_pKData->pNk && pthOwner)
    {
        ddxlog(L"This thread has been blocked for: 0x%08x ms.\n", ((PNKGLOBAL)(g_pKData->pNk))->dwCurMSec - pthOwner->dwTimeWhenBlocked);
        ddxlog(L"                    Current time: 0x%08x ms.\n", ((PNKGLOBAL)(g_pKData->pNk))->dwCurMSec);
        ddxlog(L"                    Time blocked: 0x%08x ms.\n", pthOwner->dwTimeWhenBlocked);
    }

    ddxlog(L"\n");
    ddxlog(L"\n");
    ddxlog(L"List of blocking objects (proxies) involved in this deadlock\n");
    ddxlog(L"=============================================================\n");
    DescribeProxy(pCurrProxy);
    DescribeProxy(pNextProxy);
    ddxlog(L"\n");
    ddxlog(L"\n");


    // Help

    ddxlog(L"This is a *highly unusual* circumstance and is a strong indication that the %s\n", GetProxyTypeString(pCurrProxy));
    ddxlog(L"has been abandoned, i.e. the code path to properly release the object is no longer accessable.\n");
    ddxlog(L"\n");
    ddxlog(L"Further Investigation Required.\n");
    ddxlog(L"\n");
    ddxlog(L"\n");
    ddxlog(L"The common causes of abandoned objects are :\n");
    ddxlog(L"\n");
    ddxlog(L"    1. (Most likely) Inadequate exception handling:  e.g. The thread took\n");
    ddxlog(L"        ownership of the object, hit an exception, and the exception handler\n");
    ddxlog(L"        did not release the object.\n");
    ddxlog(L"\n");
    ddxlog(L"    2. Coding error: the call to release the object is missing or misplaced.\n");
    ddxlog(L"\n");
    ddxlog(L"To confirm scenario 1 scan the debug output for exceptions in thread 0x%08x.\n", pthOwner);
    ddxlog(L"Examine the exception handler to see if it properly releases the object.\n");
    ddxlog(L"\n");
    ddxlog(L"To confirm scenario 2 code review is required.\n");
    ddxlog(L"\n");
    

    g_ceDmpDiagnoses[index].Description.DataSize = EndDDxLogging();

    // Bucket params

    HRESULT hr = GetBucketParameters(&g_ceDmpDDxBucketParameters, pthOwner, pthOwner->pprcActv);

    return (SUCCEEDED(hr));
}


//-----------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
ClearProxyStack(
                void
                )
{
    int i;
    for (i = 0; i < MAX_PROXY_STACK; i++)
        g_proxyStack.item[i] = NULL;

    g_proxyStack.tos = 0;
}



//-----------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
PushProxy(
          PROXY* p
          )
{
    if ((g_proxyStack.tos >= MAX_PROXY_STACK) || !p)
        return;

    g_proxyStack.item[g_proxyStack.tos] = p;
    g_proxyStack.tos++;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void 
PopProxy(
         void
         )
{
    if (g_proxyStack.tos)
        g_proxyStack.tos--;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int 
SearchProxyStack(
                 PROXY* p
                 )
{
    UINT i;

    if (g_proxyStack.tos > 0)
    {
        for (i = 0; (g_proxyStack.tos > 0) && (i < (g_proxyStack.tos - 1)); i++)
        {
            if ((g_proxyStack.item[i] == p) &&
                ((g_proxyStack.tos - i) > 0))
                return i;
        }
    }

    return -1;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
CheckRelationship(
                  BYTE proxy1, 
                  BYTE proxy2
                  )
{
    switch(proxy1)
    {
        case HT_CRITSEC:
        case HT_MUTEX: 
            {
                switch(proxy2)
                {
                    case HT_CRITSEC:
                    case HT_MUTEX: 
                    case SH_CURTHREAD:

                        return DL_CONTINUE;

                        // TODO: 
                        // Check to see if Events are associated with a message queue.
                        // This may narrow the scope, but would cut down on the very 
                        // high number of false positives. 

                    case HT_MANUALEVENT:
                    case HT_EVENT: 
                    case HT_SEMAPHORE:
                    case SH_CURPROC: 

                        return DL_ALERT;

                    default:
                        break;
                }
            }
            break;

        case HT_MANUALEVENT:
        case HT_EVENT: 
        case HT_SEMAPHORE:
        case SH_CURPROC: 

            return DL_END;

        case SH_CURTHREAD:
            
            return DL_CONTINUE;

        default:
            break;
    }

    return DL_END;
}



/*++

Routine Description:


Arguments:


Return Value:

--*/

DDxResult_e
TraverseProxyGraph(
                   PROXY* pCurrentProxy
                   )
{
    PTHREAD pthOwner;
    PROXY* pNextProxy;

    DDxResult_e res = DDxResult_Inconclusive;
    

    if (!pCurrentProxy)
        return DDxResult_Error;

    pthOwner = GetProxyOwnerThread(pCurrentProxy);

    if (!pthOwner)
        return DDxResult_Error;

    pNextProxy = pthOwner->lpProxy;

    while (pNextProxy)
    { 
        int action = CheckRelationship(pCurrentProxy->bType, pNextProxy->bType);

        switch (action)
        {
            case DL_END:
                return DDxResult_Continue;

            case DL_CONTINUE:
                {
                    // If a thread that is blocked on this proxy is owned by another
                    // we need to check to see if that proxy is already in the chain.
                    // If so, we have a circular reference and a deadlock.

                    int i = SearchProxyStack(pNextProxy);

                    //DEBUGGERMSG(KDZONE_DIAGNOSE, (L"      Traverse:  pThread = 0x%08x, pProxy = 0x%08x\r\n", pthOwner, pNextProxy));

                    if (i >= 0)
                    {
                        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"OsAxsT0:  Found DEADLOCK!\r\n"));

                        if (GenerateDiagnosis(i))
                        {
                            return DDxResult_Positive;
                        }
                        else
                        {
                            return DDxResult_Continue;
                        }
                    }
                            
                    
                    PushProxy(pNextProxy);

                    res = TraverseProxyGraph(pNextProxy);

                    PopProxy();

                    
                    if ((res == DDxResult_Error) || 
                        (res == DDxResult_Positive))
                    {
                        // Terminal condition hit, unwind ...
                        return res;
                    }
                }
                break;

            case DL_WARNING:
            case DL_ALERT:
                {
                    //if (m_pass == POSSIBLE_DEADLOCKS)
                    {
                        if (g_pDebuggerData != NULL &&
                            g_pKData != NULL &&
                            g_pKData->pNk != NULL &&
                            ((((PNKGLOBAL)(g_pKData->pNk))->dwCurMSec - pthOwner->dwTimeWhenBlocked) > DDX_ABANDONED_SYNC_THRESHOLD))
                        {
                            DEBUGGERMSG(OXZONE_DIAGNOSE, (L"OsAxsT0:  Found potential ORPHANED sync object!\r\n"));

                            if (OrphanWarning(pCurrentProxy, pNextProxy))
                            {
                                return DDxResult_Positive;
                            }
                        }
                        else
                        {
                            return DDxResult_Continue;
                        }
                    }
                }
                break;

            default:

                DEBUGGERMSG(OXZONE_ALERT, (L"FindDeadlocks-Traverse(): Unknown proxy relationship: 0x%08x\n", action));
                return DDxResult_Error;
        }

        if ((pNextProxy->bType == HT_CRITSEC) || 
            (pNextProxy->bType == HT_MUTEX))
        {
            // CS and Mutexes are sole proxies.  pThLinkNext is uninitialized!
            break;
        }

        pNextProxy = pNextProxy->pThLinkNext;
    }

    return res;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DDxResult_e
DiagnoseDeadlock(void)
{
    DDxResult_e res    = DDxResult_Inconclusive;
    THREAD*     pthCur = (THREAD*) DD_ExceptionState.exception->ExceptionInformation[0];
    PROXY*      pProxy;

    ClearProxyStack();

    if (pthCur)
    {
        pProxy = pthCur->lpProxy;

        while (pProxy)
        {
            PushProxy(pProxy);

                res = TraverseProxyGraph(pProxy);

            PopProxy();

            if ((pProxy->bType == HT_CRITSEC) || 
                (pProxy->bType == HT_MUTEX))
            {
                // CS and Mutexes are sole proxies.  pThLinkNext is uninitialized!
                break;
            }

            if ((res == DDxResult_Positive) || 
                (res == DDxResult_Error))
                break;

            pProxy = pProxy->pThLinkNext;
        }
    }

    return res;
}