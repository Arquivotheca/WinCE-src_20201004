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
/*++


Module Name:

    exdsptch.c

Abstract:

    This module implements the dispatching of exception and the unwinding of
    procedure call frames.

--*/

#ifdef KCOREDLL
#define WIN32_CALL(type, api, args) (*(type (*) args) g_ppfnWin32Calls[W32_ ## api])
#else
#define WIN32_CALL(type, api, args) IMPLICIT_DECL(type, SH_WIN32, W32_ ## api, args)
#endif


#include "kernel.h"
#include "excpt.h"
#include "coredll.h"
#include "errorrep.h"
#include "corexcpt.h"

//
// API to set extended PData
//

BOOL
CeSetExtendedPdata(
    LPVOID  pExtPData
    )
{
    NKD (L"CeSetExtendedPdata is no longer supported\r\n");
    ASSERT(0);
    return FALSE;
}


// restore SP and raise an "Unhandle Exception"
static PEXCEPTION_ROUTINE g_pfnEH;

//
// API to set global exception handler
//
void
SetExceptionHandler(
    PEXCEPTION_ROUTINE  per
    )
{
    g_pfnEH = per;
}


void CallGlobalHandler (PCONTEXT pCtx, PEXCEPTION_RECORD pExr)
{
    DISPATCHER_CONTEXT DispatcherContext = {0};

    if (g_pfnEH) {
#ifndef x86
        DispatcherContext.ContextRecord = pCtx;
#endif
        // clear "Unwinding" flag, we only call the global handler for dispatch.
        pExr->ExceptionFlags &= ~EXCEPTION_UNWINDING;

        // if the global excepiton handler return anything but ExceptionExecuteHandler, we would've failed
        // searching for exception handler and there is no need to contintue searching.
        if ((ExceptionExecuteHandler == (* g_pfnEH) (pExr, 0, pCtx, &DispatcherContext))
            && DispatcherContext.ControlPc) {
            CONTEXT_TO_RETVAL (pCtx) = (*((DWORD (*) (void)) DispatcherContext.ControlPc)) ();

        }
    }
}

#ifndef KCOREDLL

DLIST            g_veList = {&g_veList, &g_veList};
CRITICAL_SECTION g_veCS;    // Critical section to protect vector handler list
DWORD            g_nVectors;

typedef struct _VECTORHANDLERS {
    DLIST                       link;           // DLIST
    PVECTORED_EXCEPTION_HANDLER pfnHandler;     // the handler
} VECTORHANDLERS, *PVECTORHANDLERS;

#define CV_RETRY        0xbeaf

ERRFALSE (EXCEPTION_CONTINUE_EXECUTION != CV_RETRY);
ERRFALSE (EXCEPTION_CONTINUE_SEARCH    != CV_RETRY);

#pragma warning(push)
#pragma warning(disable:4995 4996)
//
// in excepiton handler, we can't do heap allocation as heap can be corrupted and we'll get into infinite recursion.
// use _alloca here should be pretty safe as there ususally isn't a lot of Vector Handler registered.
//
static DWORD DoCallVectorHandlers (PEXCEPTION_POINTERS pEp, DWORD nVectors)
{
    DWORD dwRet = EXCEPTION_CONTINUE_SEARCH;
    PVECTORED_EXCEPTION_HANDLER *pveh;
    if (nVectors 
        && (NULL != (pveh = (PVECTORED_EXCEPTION_HANDLER *) _alloca (nVectors * sizeof (PVECTORED_EXCEPTION_HANDLER))))) {

        EnterCriticalSection (&g_veCS);

        if (nVectors < g_nVectors) {
            // a new vector added between alloca and we grabbed CS, retry
            dwRet = CV_RETRY;
        } else {

            PDLIST ptrav;
            nVectors = 0;
            for (ptrav = g_veList.pFwd; ptrav != &g_veList; ptrav = ptrav->pFwd, nVectors ++) {
                // Record the handler, for we don't want to call the handlers holding the vector-list
                // critical section. Or we can get into deadlock situation.
                // e.g. - one of the handler allocate memory, asking for heap CS. Then
                //      thread 1 - exception in heap code, hold heap CS, accquiring vector CS
                //      thread 2 - regular exception, hold vector CS, call vector handler and acquire heap CS.
                pveh[nVectors] = ((PVECTORHANDLERS) ptrav)->pfnHandler;
            }
        }
        LeaveCriticalSection (&g_veCS);

        if (CV_RETRY != dwRet) {
            DWORD idx;
            for (idx = 0; idx < nVectors; idx ++) {
                if ((dwRet = pveh[idx] (pEp)) == EXCEPTION_CONTINUE_EXECUTION) {
                    break;
                }
                // the vector is supposed to only return EXCEPTION_CONTINUE_SEARCH or EXCEPTION_CONTINUE_EXECUTION
                DEBUGCHK (EXCEPTION_CONTINUE_SEARCH == dwRet);
                dwRet = EXCEPTION_CONTINUE_SEARCH;
            }
        }
    }

    return dwRet;
}
#pragma warning(pop)

// C4204: nonstandard extension used : non-constant aggregate initializer
#pragma warning(push)
#pragma warning(disable:4204)

void CallVectorHandlers (PEXCEPTION_RECORD pExr, PCONTEXT pCtx)
{
    // don't call vector handler if stack overflow.
    if (STATUS_STACK_OVERFLOW != pExr->ExceptionCode) {
        EXCEPTION_POINTERS ep = { pExr, pCtx };
        DWORD dwAction;

        do {
            dwAction = DoCallVectorHandlers (&ep, g_nVectors);
            if (EXCEPTION_CONTINUE_EXECUTION == dwAction) {
                RtlResumeFromContext (pCtx);
                // should never return
                DEBUGCHK (0);
            }

        } while (CV_RETRY == dwAction);
    }
}

#pragma warning(pop)

//
// Vector exception handler support
//
WINBASEAPI
PVOID
WINAPI
AddVectoredExceptionHandler(
    IN ULONG FirstHandler,
    IN PVECTORED_EXCEPTION_HANDLER VectoredHandler
    )
{
    if (!VectoredHandler) {
        return NULL;
    } else {
        PVECTORHANDLERS pvh = LocalAlloc (0, sizeof (VECTORHANDLERS));
        if (pvh) {
            pvh->pfnHandler = VectoredHandler;

            EnterCriticalSection (&g_veCS);
            if (FirstHandler) {
                AddToDListHead (&g_veList, &pvh->link);
            } else {
                AddToDListTail (&g_veList, &pvh->link);
            }
            g_nVectors ++;
            LeaveCriticalSection (&g_veCS);
        }
        return pvh;
    }
}

WINBASEAPI
ULONG
WINAPI
RemoveVectoredExceptionHandler(
    IN PVOID VectoredHandlerHandle
    )
{
    if (!VectoredHandlerHandle) {
        return 0;
    } else {
        PDLIST ptrav;
        EnterCriticalSection (&g_veCS);
        for (ptrav = g_veList.pFwd; ptrav != &g_veList; ptrav = ptrav->pFwd) {
            if (ptrav == (PDLIST) VectoredHandlerHandle) {
                RemoveDList (ptrav);
                g_nVectors --;
                break;
            }
        }
        LeaveCriticalSection (&g_veCS);

        if (&g_veList != ptrav) {
            // found
            LocalFree (ptrav);
        }
        return (&g_veList != ptrav);
    }
}

#else //KCOREDLL

// KCOREDLL - vector handlers not supported
void CallVectorHandlers (PEXCEPTION_RECORD pExr, PCONTEXT pCtx)
{
}

#endif


