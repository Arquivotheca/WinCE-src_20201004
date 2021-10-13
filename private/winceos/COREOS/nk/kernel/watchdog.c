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

#include <kernel.h>

//
// globals that can be updated by OEM 
//
extern CRITICAL_SECTION     WDcs;

//
// globals internal to watchdog support
//
static DLIST     g_wdList;          // list of watch dogs
static PHDATA    phdWDEvt;
static HANDLE    hWDThrd;

//
// watchdog states
//
#define WD_STATE_STOPPED            0
#define WD_STATE_STARTED            1
#define WD_STATE_SIGNALED           2

static BOOL CompareTime (PDLIST pItem, LPVOID pEnumData)
{
    PWatchDog pWD      = (PWatchDog) pItem;

    return ((int) ((DWORD) pEnumData - pWD->dwExpire)) < 0;
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
static void WDenqueue (PWatchDog pWD)
{

    PWatchDog pNext = (PWatchDog) EnumerateDList (&g_wdList, CompareTime, (LPVOID) pWD->dwExpire);

    AddToDListTail (pNext? &pNext->link: &g_wdList, &pWD->link);
    
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
static void WDdequeue (PWatchDog pWD)
{
    RemoveDList (&pWD->link);
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
static DWORD WDTakeDfltAction (PWatchDog pWD)
{
    DWORD dwProcId = 0;
    switch (pWD->wDfltAction) {
    case WDOG_KILL_PROCESS:
        {
            PHDATA phd = LockHandleData ((HANDLE) pWD->dwProcId, g_pprcNK);
            PPROCESS pprc = GetProcPtr (phd);

            if (pprc) {
                dwProcId = pprc->dwId;
                DEBUGCHK (dwProcId == pWD->dwProcId);
                // don't call SC_ProcTerminate because it'll block waiting for the process to exit
                KillOneThread ((PTHREAD) pprc->thrdList.pBack, 0);
            }
            UnlockHandleData (phd);
        }
        break;
    case WDOG_RESET_DEVICE:
        KernelIoctl (IOCTL_HAL_REBOOT, &pWD->dwParam, sizeof(DWORD), NULL, 0, NULL);
        break;
    default:
        // no default action
        break;
    }
    return dwProcId;
}


//--------------------------------------------------------------------------------------------
// the watchdog timer thread
//--------------------------------------------------------------------------------------------
static DWORD WINAPI WatchDogTimerThrd (
    LPVOID   pParam)
{
    DWORD           dwTimeout, dwTick, dwDiff;
    PWatchDog       pWD;
    DWORD           dwIdProcKilled;

    // the watchdog thread run with access to all processes so that it can set the watchdog event
    
    // we need to set the affinity the watchdog timer thread to the core that handles interrupt (master core)
    // such that the watchdog will be triggered in case of interrupt storm.
    SCHL_SetThreadAffinity (pCurThread, 1);

    do {
        // refresh hardware watchdog if exist
        g_pOemGlobal->pfnRefreshWatchDog ();

        // default timeout is either hardware watchdog period, or infinite when there is no
        // hardware watchdog
        dwTimeout = g_pOemGlobal->dwWatchDogPeriod? g_pOemGlobal->dwWatchDogPeriod : INFINITE;

        // look into the timer queue and see if there is any watchdog to be signaled. Also
        // calculate timeout if we need to wakeup before the watchdog period expired
        EnterCriticalSection (&WDcs);
        
        while (!IsDListEmpty (&g_wdList)) {

            pWD = (PWatchDog) g_wdList.pFwd;
            
            // default, no process killed
            dwIdProcKilled = 0;
                
            // get current time and diff
            dwTick = OEMGetTickCount ();
            dwDiff = pWD->dwExpire - dwTick;
            
            if ((int) dwDiff > 0) {
                // No need to signal watchdog yet. But we need to
                // adjust timeout if we need to wakeup earlier.
                if (dwTimeout > dwDiff) {
                    dwTimeout = dwDiff;
                }
                break;
            }
            
            // there is at least one watchdog needs to be signaled

            // remove the watchdog from the list
            WDdequeue (pWD);

            // take action based on state
            switch (pWD->wState) {
            case WD_STATE_STARTED:
                // signal the watchdog
                pWD->wState = WD_STATE_SIGNALED;
                EVNTModify (pWD->pEvt, EVENT_SET);

                // put ourself back to the timer queue if dwWait is specified
                if (pWD->dwWait) {
                    pWD->dwExpire = dwTick + pWD->dwWait;
                    WDenqueue (pWD);
                    break;
                }
                // fall through to take default action
                __fallthrough;
                
            case WD_STATE_SIGNALED:
                // watchdog isn't refreshed after dwWait, take default action
                dwIdProcKilled = WDTakeDfltAction (pWD);
                pWD->wState = WD_STATE_STOPPED;
                break;

            case WD_STATE_STOPPED:
                // watchdog is stopped, just remove ignore the entry
                break;
                
            default:
                // should not happen
                DEBUGCHK (0);
                break;
            }

            // notify PSL if default action taken is to kill the process.
            if (dwIdProcKilled) {
                // release watchdog CS before notifying PSL. Or we can deadlock if the PSL
                // notification happens to call into kernel again.
                LeaveCriticalSection (&WDcs);
            
                // notify PSLs that the process is exiting
                NKPSLNotify (DLL_PROCESS_EXITING, dwIdProcKilled, 0);
            
                // enter critical section again
                EnterCriticalSection (&WDcs);
            }
        
        }
        LeaveCriticalSection (&WDcs);
    } while (DoWaitForObjects (1, &phdWDEvt, dwTimeout) != WAIT_FAILED);

    DEBUGCHK (0);
    NKExitThread (0);
    return 0;
}

//--------------------------------------------------------------------------------------------
// create/open a watchdog timer
//--------------------------------------------------------------------------------------------
HANDLE NKCreateWatchDog (LPSECURITY_ATTRIBUTES lpsa,    // security attributes
    LPCWSTR pszWatchDogName,                            // name of the watchdog
    DWORD dwDfltAction,                                 // default action if watchdog is signaled
    DWORD dwParam,                                      // reset parameter if default action is reset
    DWORD dwPeriod,                                     // watchdog period
    DWORD dwWait,                                       // wait period before taking default action
    BOOL  fCreate)                                      // create or open
{
    HANDLE  hWDog = NULL;

    DEBUGMSG (ZONE_ENTRY, (L"WDOpenOrCreate: '%s', %d, %d, %8.8lx, %8.8lx\r\n", 
                pszWatchDogName, dwPeriod, dwWait, dwDfltAction, dwParam));

    // validate arguments
    if ((fCreate && ((int) dwPeriod <= 0))
        || ((int) dwWait < 0)
        || (dwDfltAction >= WD_TOTAL_DFLT_ACTION)) {
        SetLastError (ERROR_INVALID_PARAMETER);
        
    } else {

        DEBUGCHK(hWDThrd);
        DEBUGCHK(phdWDEvt);

        EnterCriticalSection (&WDcs);
        
        // create/open the watchdog event
        hWDog  = fCreate
            ? NKCreateWdogEvent (lpsa, FALSE, FALSE, pszWatchDogName)
            : NKOpenWdogEvent  (EVENT_ALL_ACCESS, FALSE, pszWatchDogName);

        if (hWDog) {
            DWORD   dwErr  = GetLastError ();
            PHDATA  phdEvt = LockHandleData (hWDog, pActvProc);
            PEVENT  pEvt   = GetEventPtr (phdEvt);

            DEBUGCHK (!dwErr || (ERROR_ALREADY_EXISTS == dwErr));

            if (!pEvt) {
                // preempted and another thread closed the event.
                dwErr = ERROR_INVALID_PARAMETER;
                DEBUGCHK (0);
                
            } else {
                PWatchDog pWD;

                if (dwErr) {
                    // event exists, get the watchdog ptr from event
                    dwErr = pEvt->pWD? 0 : ERROR_INVALID_NAME;

                } else if (NULL != (pWD = AllocMem (HEAP_WATCHDOG))) {
                    // new watchdog, initialized the watchdog structre
                    memset (pWD, 0, sizeof (WatchDog));
                    pWD->wDfltAction  = (WORD) dwDfltAction;
                    pWD->dwParam      = dwParam;
                    pWD->dwPeriod     = dwPeriod;
                    pWD->wState       = WD_STATE_STOPPED;
                    pWD->dwWait       = dwWait;
                    pWD->pEvt         = pEvt;
                    pEvt->pWD         = pWD;
                } else {
                    dwErr = ERROR_OUTOFMEMORY;
                }

            }
            UnlockHandleData (phdEvt);
            
            if (dwErr) {
                HNDLCloseHandle (pActvProc, hWDog);
                hWDog = NULL;
                SetLastError (dwErr);
            }
        }
        
        LeaveCriticalSection (&WDcs);
    }
    
    DEBUGMSG (ZONE_ENTRY, (L"WDOpenOrCreate: returns %8.8lx\r\n", hWDog));

    return hWDog;
}

//--------------------------------------------------------------------------------------------
// start a watchdog timer
//--------------------------------------------------------------------------------------------
BOOL WDStart (PEVENT pEvt)
{
    PPROCESS pprc = pActvProc;
    PTHREAD pth   = pCurThread;
    PWatchDog pWD = pEvt->pWD;
    BOOL dwErr    = (pWD) ? 0: ERROR_INVALID_PARAMETER;
    
    DEBUGMSG (ZONE_ENTRY, (L"WDStart: %8.8lx\r\n", pWD));

    if (pWD) {

        if (!dwErr) {
            EnterCriticalSection (&WDcs);
            if (WD_STATE_STOPPED == pWD->wState) {
                pWD->dwExpire = OEMGetTickCount () + pWD->dwPeriod;
                pWD->wState = WD_STATE_STARTED;
                pWD->dwProcId = pprc->dwId;
                WDenqueue (pWD);

                // signal watchdog timer thread to start running and re-calculate timeout
                EVNTModify (GetEventPtr (phdWDEvt), EVENT_SET);

                dwErr = 0;
                    
            } else {
            
                // already started
                dwErr = ERROR_ALREADY_INITIALIZED;
            }
            LeaveCriticalSection (&WDcs);
        }
    }

    if (dwErr) {
        KSetLastError (pth, dwErr);
    }

    DEBUGMSG (ZONE_ENTRY, (L"WDStart: exits, dwErr = %8.8lx\r\n", dwErr));

    return !dwErr;
}

//--------------------------------------------------------------------------------------------
// stop a watchdog timer
//--------------------------------------------------------------------------------------------
BOOL WDStop (PEVENT pEvt)
{
    PWatchDog pWD = pEvt->pWD;
    DEBUGMSG (ZONE_ENTRY, (L"WDStop: %8.8lx\r\n", pWD));

    if (!pWD) {
        
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        
    } else {

        EnterCriticalSection (&WDcs);
        if (WD_STATE_STOPPED != pWD->wState) {

            pWD->wState = WD_STATE_STOPPED;
            WDdequeue (pWD);
        }
        LeaveCriticalSection (&WDcs);
    }

    DEBUGMSG (ZONE_ENTRY, (L"WDStop: returns %8.8lx\r\n", NULL != pWD));

    return NULL != pWD;
}

//--------------------------------------------------------------------------------------------
// refresh a watchdog timer
//--------------------------------------------------------------------------------------------
BOOL WDRefresh (PEVENT pEvt)
{
    PWatchDog pWD = pEvt->pWD;
    DEBUGMSG (ZONE_ENTRY, (L"WDRefresh: %8.8lx\r\n", pWD));

    if (!pWD) {
        
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        
    } else {
        EnterCriticalSection (&WDcs);
        if (WD_STATE_STOPPED != pWD->wState) {
    
            WDdequeue (pWD);
            pWD->dwExpire = OEMGetTickCount () + pWD->dwPeriod;
            pWD->wState = WD_STATE_STARTED;
            WDenqueue (pWD);
        }
        LeaveCriticalSection (&WDcs);
    }

    DEBUGMSG (ZONE_ENTRY, (L"WDRefresh: returns %8.8lx\r\n", NULL != pWD));

    return NULL != pWD;
}


//--------------------------------------------------------------------------------------------
// Initialize Watchdog support
//--------------------------------------------------------------------------------------------
BOOL InitWatchDog (void)
{
    // initialize active watchdog list
    InitDList (&g_wdList);

    // create the watchdog thread signal event
    phdWDEvt = NKCreateAndLockEvent (FALSE, FALSE);
    DEBUGCHK (phdWDEvt);

    // create the watchdog thread
    hWDThrd = CreateKernelThread (WatchDogTimerThrd, NULL, (WORD) g_pOemGlobal->dwWatchDogThreadPriority, 0);
    DEBUGCHK (hWDThrd);
    
    DEBUGMSG (ZONE_ENTRY, (L"WatchDog Timer thread %8.8lx Created\r\n", hWDThrd));

    DEBUGMSG ((g_pOemGlobal->dwWatchDogPeriod && ZONE_ENTRY), 
        (L"Hardware watchdog supported, watchdog period = %dms, watchdog thread priority = %d\r\n", 
        g_pOemGlobal->dwWatchDogPeriod, g_pOemGlobal->dwWatchDogThreadPriority));
    
    return (NULL != phdWDEvt) && (NULL != hWDThrd);
}


//--------------------------------------------------------------------------------------------
// delete a watchdog timer (called by EventCloseHandle)
//--------------------------------------------------------------------------------------------
BOOL WDDelete (PEVENT pEvt)
{
    PWatchDog pWD = pEvt->pWD;
    DEBUGMSG (ZONE_ENTRY, (L"WDDelete: %8.8lx\r\n", pWD));

    if (pWD) {
        DWORD dwIdProcKilled = 0;
        EnterCriticalSection (&WDcs);

        pEvt->pWD = NULL;
        if (WD_STATE_STOPPED != pWD->wState) {
            // watchdog active when there are no more reference to it.
            // - always take default action as it wouldn't be able to be signaled again.
            dwIdProcKilled = WDTakeDfltAction (pWD);
            WDdequeue (pWD);
        }
        FreeMem (pWD, HEAP_WATCHDOG);

        LeaveCriticalSection (&WDcs);

        if (dwIdProcKilled) {
            // notify PSLs that the process is exiting
            NKPSLNotify (DLL_PROCESS_EXITING, dwIdProcKilled, 0);
        }
    }

    DEBUGMSG (ZONE_ENTRY, (L"WDDelete: returns %8.8lx\r\n", NULL != pWD));

    return NULL != pWD;
}


