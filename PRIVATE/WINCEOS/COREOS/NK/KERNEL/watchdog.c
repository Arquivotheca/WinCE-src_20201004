//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <kernel.h>

#define DEFAULT_WATCHDOG_PRIORITY   100

void FakedRefreshWatchDog (void)
{
}

//
// globals that can be updated by OEM 
//
void (* pfnOEMRefreshWatchDog) (void) = FakedRefreshWatchDog;
DWORD   dwOEMWatchDogPeriod;
DWORD   dwNKWatchDogThreadPriority = DEFAULT_WATCHDOG_PRIORITY;
extern CRITICAL_SECTION     WDcs;
extern LPDWORD              pIsExiting;

//
// watchdog structure
//
typedef struct _WatchDog {
    struct _WatchDog *pNext;        // next entry in the timer list
    struct _WatchDog *pPrev;        // previous entry in the timer list
    DWORD           dwExpire;       // time expired
    DWORD           dwState;        // current state
    HANDLE          hWDog;          // handle to the watchdog timer
    DWORD           dwPeriod;       // watchdog period
    DWORD           dwWait;         // time to wait before default action taken
    DWORD           dwDfltAction;   // default action
    DWORD           dwParam;        // parameter passed to IOCTL_HAL_REBOOT
    HANDLE          hProc;          // process to be watched
} WatchDog, *PWatchDog;

ERRFALSE(sizeof(WatchDog) <= sizeof(FULLREF));

//
// globals internal to watchdog support
//
static PWatchDog pWDList;
static HANDLE    hWDEvt;
static HANDLE    hWDThrd;

//
// watchdog states
//
#define WD_STATE_STOPPED            0
#define WD_STATE_STARTED            1
#define WD_STATE_SIGNALED           2


HANDLE KillOneThread (HANDLE hTh, DWORD dwRetVal);
BOOL KernelIoctl (DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
static void WDenqueue (PWatchDog pWD)
{
    if (!pWDList || ((int) (pWD->dwExpire - pWDList->dwExpire) < 0)) {
        
        pWD->pNext = pWDList;
        pWD->pPrev = NULL;
        if (pWDList) {
            pWDList->pPrev = pWD;
        }
        pWDList = pWD;
        
    } else {
        PWatchDog pPrev = pWDList, pNext;

        while ((pNext = pPrev->pNext) && ((int) (pNext->dwExpire - pWD->dwExpire) <= 0)) {
            pPrev = pNext;
        }
        pWD->pPrev = pPrev;
        pPrev->pNext = pWD;
        if (pWD->pNext = pNext) {
            pNext->pPrev = pWD;
        }
    }
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
static void WDdequeue (PWatchDog pWD)
{
    PWatchDog pPrev = pWD->pPrev, pNext = pWD->pNext;
    if (pPrev) {
        pPrev->pNext = pNext;
    } else {
        // pWD is the head of the list
        pWDList = pNext;
    }

    if (pNext) {
        pNext->pPrev = pPrev;
    }
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
static PPROCESS WDTakeDfltAction (PWatchDog pWD)
{
    PPROCESS pProc = NULL;
    LPDWORD  pProcExiting;
    switch (pWD->dwDfltAction) {
    case WDOG_KILL_PROCESS:
        // don't call SC_ProcTerminate because it'll block waiting for the process to exit
        if ((pProc = HandleToProc (pWD->hProc))
            && (pProcExiting = (LPDWORD)MapPtrProc (pIsExiting, pProc))
            && !*pProcExiting) {

            KillOneThread (pProc->pMainTh->hTh, 0);

        } else {
            pProc = NULL;
        }
        break;
    case WDOG_RESET_DEVICE:
        KernelIoctl (IOCTL_HAL_REBOOT, &pWD->dwParam, sizeof(DWORD), NULL, 0, NULL);
        break;
    default:
        // no default action
        break;
    }

    return pProc;
}

//--------------------------------------------------------------------------------------------
// get the watchdog pointer from a watchdog handle
//--------------------------------------------------------------------------------------------
PWatchDog GetWatchDog (HANDLE hWDog)
{
    PWatchDog pWD = (PWatchDog) SC_EventGetData (hWDog);
    return (IsValidKPtr (pWD) && (hWDog == pWD->hWDog))? pWD : NULL;
}

//--------------------------------------------------------------------------------------------
// associate a watchdog pointer with a watchdog handle
//--------------------------------------------------------------------------------------------
static BOOL SetWatchDog (HANDLE hWDog, PWatchDog pWD)
{
    DEBUGCHK (!pWD || (pWD->hWDog == hWDog));
    
    return SC_EventSetData (hWDog, (DWORD) pWD);
}

//--------------------------------------------------------------------------------------------
// the watchdog timer thread
//--------------------------------------------------------------------------------------------
static DWORD WINAPI WatchDogTimerThrd (
    LPVOID   pParam)
{
    DWORD           dwTimeout, dwTick, dwDiff;
    PWatchDog       pWD;
    PPROCESS        pprcKilled;
    

    // the watchdog thread run with access to all processes so that it can set the watchdog event
    SWITCHKEY (dwTick, 0xffffffff);
    
    do {
        // refresh hardware watchdog if exist
        pfnOEMRefreshWatchDog ();

        // default timeout is either hardware watchdog period, or infinite when there is no
        // hardware watchdog
        dwTimeout = dwOEMWatchDogPeriod? dwOEMWatchDogPeriod : INFINITE;

        // look into the timer queue and see if there is any watchdog to be signaled. Also
        // calculate timeout if we need to wakeup before the watchdog period expired
        EnterCriticalSection (&WDcs);
        
        while (pWDList) {

            // default, no process killed
            pprcKilled = NULL;
            
            // get current time and diff
            dwTick = SC_GetTickCount ();
            dwDiff = pWDList->dwExpire - dwTick;
            
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
            pWD = pWDList;
            if (pWDList = pWDList->pNext) {
                pWDList->pPrev = NULL;
            }

            // take action based on state
            switch (pWD->dwState) {
            case WD_STATE_STARTED:
                // signal the watchdog
                pWD->dwState = WD_STATE_SIGNALED;
                SetEvent (pWD->hWDog);

                // put ourself back to the timer queue if dwWait is specified
                if (pWD->dwWait) {
                    pWD->dwExpire = dwTick + pWD->dwWait;
                    WDenqueue (pWD);
                    break;
                }
                // fall through to take default action
                
            case WD_STATE_SIGNALED:
                // watchdog isn't refreshed after dwWait, take default action
                pprcKilled = WDTakeDfltAction (pWD);
                pWD->dwState = WD_STATE_STOPPED;
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
            if (pprcKilled) {
                // release watchdog CS before notifying PSL. Or we can deadlock if the PSL
                // notification happens to call into kernel again.
                LeaveCriticalSection (&WDcs);

                // notify PSLs that the process is exiting
                (* pPSLNotify) (DLL_PROCESS_EXITING, (DWORD) pprcKilled->hProc, (DWORD)NULL);

                // enter critical section again
                EnterCriticalSection (&WDcs);
            }

        }
        LeaveCriticalSection (&WDcs);
    } while (SC_WaitForMultiple (1, &hWDEvt, FALSE, dwTimeout) != WAIT_FAILED);

    DEBUGCHK (0);
    SC_NKTerminateThread (0);
    return 0;
}

//--------------------------------------------------------------------------------------------
// create the watchdog timer thread
//--------------------------------------------------------------------------------------------
static HANDLE CreateWatchDogThread (void)
{
    if (!hWDThrd) {
        hWDThrd = CreateKernelThread (WatchDogTimerThrd, NULL, (WORD) dwNKWatchDogThreadPriority, 0);
        DEBUGMSG (1, (L"WatchDog Timer thread %8.8lx Created\r\n", hWDThrd));
    }
    return hWDThrd;
}

//--------------------------------------------------------------------------------------------
// create/open a watchdog timer
//--------------------------------------------------------------------------------------------
static HANDLE WDOpenOrCreate (int apiCode, PCWDAPIStruct pwdas)
{
    HANDLE  hWDog;
    LPCWSTR pszWatchDogName = (LPCWSTR) pwdas->watchdog;

    DEBUGMSG (ZONE_ENTRY, (L"WDOpenOrCreate: '%s', %d, %d, %8.8lx, %8.8lx\r\n", 
                pszWatchDogName, pwdas->dwPeriod, pwdas->dwWait, pwdas->dwDfltAction, pwdas->dwParam));
    
    // create/open the watchdog event
    hWDog  = (WDAPI_CREATE == apiCode)
        ? SC_CreateEvent (NULL, FALSE, FALSE, pszWatchDogName)
        : SC_OpenEvent  (EVENT_ALL_ACCESS, FALSE, pszWatchDogName);

    if (hWDog) {
           
        PWatchDog pWD;
        DWORD     dwErr = ERROR_OUTOFMEMORY;

        if ((WDAPI_OPEN == apiCode) 
            || (pszWatchDogName && (ERROR_ALREADY_EXISTS == KGetLastError (pCurThread)))) {
            // event exists, verify if it's a watchdog timer
            dwErr = GetWatchDog (hWDog)? 0 : ERROR_INVALID_NAME;
            
        } else if (pWD = AllocMem (HEAP_WATCHDOG)) {
            // new watchdog, initialized the watchdog structre
            pWD->dwDfltAction = pwdas->dwDfltAction;
            pWD->dwParam      = pwdas->dwParam;
            pWD->dwPeriod     = pwdas->dwPeriod;
            pWD->dwState      = WD_STATE_STOPPED;
            pWD->dwWait       = pwdas->dwWait;
            pWD->hProc        = NULL;
            pWD->hWDog        = hWDog;
            pWD->pNext        = NULL;
            pWD->pPrev        = NULL;
            SetWatchDog (hWDog, pWD);

            dwErr = 0;
        }
        
        if (dwErr) {
            KSetLastError (pCurThread, dwErr);
            SC_EventCloseHandle (hWDog);
            hWDog = NULL;
        }
    }
    
    DEBUGMSG (ZONE_ENTRY, (L"WDOpenOrCreate: returns %8.8lx\r\n", hWDog));

    return hWDog;
}

//--------------------------------------------------------------------------------------------
// start a watchdog timer
//--------------------------------------------------------------------------------------------
static BOOL WDStart (HANDLE  hWDog)
{
    PWatchDog pWD;
    BOOL dwErr = ERROR_INVALID_PARAMETER;
    DEBUGMSG (ZONE_ENTRY, (L"WDStart: %8.8lx\r\n", hWDog));

    if (pWD = GetWatchDog (hWDog)) {
        if (WD_STATE_STOPPED == pWD->dwState) {
            pWD->dwExpire = SC_GetTickCount () + pWD->dwPeriod;
            pWD->dwState = WD_STATE_STARTED;
            pWD->hProc = hCurProc;
            WDenqueue (pWD);

            // signal watchdog timer thread to start running and re-calculate timeout
            SetEvent (hWDEvt);

            dwErr = 0;
                
        } else {
        
            // already started
            dwErr = ERROR_ALREADY_INITIALIZED;
        }
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }

    DEBUGMSG (ZONE_ENTRY, (L"WDStart: exits, dwErr = %8.8lx\r\n", dwErr));

    return !dwErr;
}

//--------------------------------------------------------------------------------------------
// stop a watchdog timer
//--------------------------------------------------------------------------------------------
static BOOL WDStop (HANDLE  hWDog)
{
    PWatchDog pWD;
    DEBUGMSG (ZONE_ENTRY, (L"WDStop: %8.8lx\r\n", hWDog));

    if (!(pWD = GetWatchDog (hWDog))) {
        
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        
    } else if (WD_STATE_STOPPED != pWD->dwState) {

        pWD->dwState = WD_STATE_STOPPED;
        WDdequeue (pWD);
   }

    DEBUGMSG (ZONE_ENTRY, (L"WDStop: returns %8.8lx\r\n", NULL != pWD));

    return NULL != pWD;
}

//--------------------------------------------------------------------------------------------
// refresh a watchdog timer
//--------------------------------------------------------------------------------------------
static BOOL WDRefresh (
    HANDLE  hWDog)
{
    PWatchDog pWD;
    DEBUGMSG (ZONE_ENTRY, (L"WDRefresh: %8.8lx\r\n", hWDog));

    if (!(pWD = GetWatchDog (hWDog))) {
        
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        
    } else if (WD_STATE_STOPPED != pWD->dwState) {
    
        WDdequeue (pWD);
        pWD->dwExpire = SC_GetTickCount () + pWD->dwPeriod;
        pWD->dwState = WD_STATE_STARTED;
        WDenqueue (pWD);
    }

    DEBUGMSG (ZONE_ENTRY, (L"WDRefresh: returns %8.8lx\r\n", NULL != pWD));

    return NULL != pWD;
}


//--------------------------------------------------------------------------------------------
// Initialize Watchdog support
//--------------------------------------------------------------------------------------------
BOOL InitWatchDog (void)
{
    // create the watchdog thread signal event
    hWDEvt = SC_CreateEvent (NULL, FALSE, FALSE, NULL);
    DEBUGCHK (hWDEvt);

    // create watchdog thread if hardware watchdog exist
    if (FakedRefreshWatchDog != pfnOEMRefreshWatchDog) {
        DEBUGMSG (1, (L"Hardware watchdog supported, watchdog period = %dms, watchdog thread priority = %d\r\n",
            dwOEMWatchDogPeriod, dwNKWatchDogThreadPriority));

        CreateWatchDogThread ();
        DEBUGCHK (hWDThrd);
    }

    return NULL != hWDEvt;
}




//--------------------------------------------------------------------------------------------
// watchdog timer API handler
//--------------------------------------------------------------------------------------------
DWORD WatchDogAPI (DWORD apiCode, PCWDAPIStruct pwdas)
{
    DWORD dwErr = 0;
    DWORD dwRet = 0;
    
    if (!pwdas || pwdas->dwFlags) {
        dwErr = ERROR_INVALID_PARAMETER;
    } else {
        EnterCriticalSection (&WDcs);
        
        switch (apiCode) {
        case WDAPI_CREATE:
            // validate parameters
            if (((int) pwdas->dwPeriod <= 0)
                || ((int) pwdas->dwWait < 0)
                || (pwdas->dwDfltAction >= WD_TOTAL_DFLT_ACTION)) {
                dwErr = ERROR_INVALID_PARAMETER;
                break;
            }
            // create the watchdog thread if not already created
            if (!CreateWatchDogThread ()) {
                dwErr = ERROR_OUTOFMEMORY;
                break;
            }
            // fall through to open/create watchdog
        case WDAPI_OPEN:
            dwRet = (DWORD) WDOpenOrCreate (apiCode, pwdas);
            break;
        case WDAPI_START:
            dwRet = WDStart ((HANDLE) pwdas->watchdog);
            break;
        case WDAPI_STOP:
            dwRet = WDStop ((HANDLE) pwdas->watchdog);
            break;
        case WDAPI_REFRESH:
            dwRet = WDRefresh ((HANDLE) pwdas->watchdog);
            break;
        default:
            dwErr = ERROR_INVALID_PARAMETER;
            break;
        }
        
        LeaveCriticalSection (&WDcs);
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }

    return dwRet;
}

//--------------------------------------------------------------------------------------------
// delete a watchdog timer (called by EventCloseHandle)
//--------------------------------------------------------------------------------------------
BOOL WDDelete (HANDLE hWDog)
{
    PWatchDog pWD;
    DEBUGMSG (ZONE_ENTRY, (L"WDDelete: %8.8lx\r\n", hWDog));

    EnterCriticalSection (&WDcs);

    if (pWD = GetWatchDog (hWDog)) {
        SetWatchDog (hWDog, NULL);
        if (WD_STATE_STOPPED != pWD->dwState) {
            WDdequeue (pWD);
        }
        FreeMem (pWD, HEAP_WATCHDOG);
    }

    LeaveCriticalSection (&WDcs);

    DEBUGMSG (ZONE_ENTRY, (L"WDDelete: returns %8.8lx\r\n", NULL != pWD));

    return NULL != pWD;
}


