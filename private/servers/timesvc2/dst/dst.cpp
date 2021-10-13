//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#include "dst.h"
#include "DSTHandler.h"

HANDLE g_rghEvents[EI_NUM_EVENTS] = {NULL};
HANDLE g_hDSTHandled = NULL;
LPDSTSVC_TIME_CHANGE_INFORMATION g_lpTimeChangeInformation;

DWORD DSTThread(HANDLE hExitEvent)
{
    DEBUGMSG(TRUE, (TEXT("[TIMESVC]====>DST thread created")));
    g_dwDSTThreadId =  GetCurrentThreadId();

    DSTHandler* pDSTHandler = DSTHandler::Create();
    if (pDSTHandler != NULL)
    {
        g_hDSTHandled = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (g_hDSTHandled == NULL)
        {
            goto Error;
        }

        g_rghEvents[EI_EXIT] = hExitEvent;
        g_rghEvents[EI_DST_TIMER]  = pDSTHandler->GetDSTTimerEvent();
        g_rghEvents[EI_DYN_DST_TIMER] = pDSTHandler->GetDynDSTTimerEvent();
        g_rghEvents[EI_PRE_TIME_CHANGE] = pDSTHandler->GetPreTimeChangeEvent();
        g_rghEvents[EI_TIME_CHANGE] = pDSTHandler->GetTimeChangeEvent();
        g_rghEvents[EI_TZ_CHANGE] = pDSTHandler->GetTzChangeEvent();

        BOOL bSuccess = TRUE;
        while (bSuccess)
        {
            // Handle for the registry change event may change
            g_rghEvents[EI_TZ_REG_CHANGE] = pDSTHandler->GetTzRegChangeEvent();

            DEBUGMSG(TRUE, (TEXT("[TIMESVC]====>Waiting for events\r\n")));
            DWORD dwObj = WaitForMultipleObjects(EI_NUM_EVENTS, g_rghEvents, FALSE, INFINITE);

            DWORD dwEventIdx = dwObj - WAIT_OBJECT_0;

            SYSTEMTIME stLocal;
            GetLocalTime(&stLocal);
            pDSTHandler->SetLocalTime(&stLocal);

            DEBUGMSG(TRUE, (TEXT("[%02d-%02d-%04d %02d:%02d:%02d]Handling events %s"), stLocal.wMonth, stLocal.wDay, stLocal.wYear, stLocal.wHour, stLocal.wMinute, stLocal.wSecond, szEventNames[dwEventIdx]));
            switch (dwEventIdx)
            {
            case EI_DYN_DST_TIMER:
            case EI_DST_TIMER:
            case EI_TIME_CHANGE:
            case EI_TZ_CHANGE:
                bSuccess = pDSTHandler->OnDstGenericEvent();
                break;

            case EI_PRE_TIME_CHANGE:
                bSuccess = pDSTHandler->OnPreTimeChange();
                break;

            case EI_TZ_REG_CHANGE:
                bSuccess = pDSTHandler->OnTzRegChange();
                break;

            case EI_EXIT:
                bSuccess = FALSE;
                break;

            default:
                ERRORMSG(TRUE, (TEXT("[TIMESVC]====>Unexpected event index (%d)\r\n"),dwEventIdx));
                bSuccess = FALSE;
                break;
            }
            DEBUGMSG(!bSuccess, (TEXT("[%02d-%02d-%04d %02d:%02d:%02d]%s handler returns FALSE, exit"), stLocal.wMonth, stLocal.wDay, stLocal.wYear, stLocal.wHour, stLocal.wMinute, stLocal.wSecond, szEventNames[dwEventIdx]));
            if ((dwEventIdx == EI_PRE_TIME_CHANGE) ||
                (dwEventIdx == EI_TZ_CHANGE))
            {
                SetEvent(g_hDSTHandled);
            }
        }
    }
    else
    {
        DEBUGMSG(TRUE, (TEXT("[TIMESVC]====>Failed to create DSTHandler, exit")));
    }

Error:
    if (pDSTHandler)
    {
        delete pDSTHandler;
    }
    if (g_hDSTHandled)
    {
        CloseHandle(g_hDSTHandled);
    }
    DEBUGMSG(TRUE, (TEXT("[TIMESVC]====>DST thread is exitting now")));
    return TRUE;
}

//
//  Public interface section
//
HINSTANCE           g_hInst = NULL;
HANDLE              g_hDSTShutdownEvent = NULL;
HANDLE              g_hDSTThread = NULL;
DWORD               g_dwDSTThreadId = 0;

int InitializeDST (HINSTANCE hInst) 
{
    DEBUGMSG(TRUE, (TEXT("[TIMESVC]====>Initializing DST service")));

    g_hInst = hInst;
    g_hDSTShutdownEvent = CreateEvent (NULL, TRUE, TRUE, NULL);

    return (g_hDSTShutdownEvent != NULL) ? ERROR_SUCCESS : ERROR_OUTOFMEMORY;
}

void DestroyDST (void) 
{
    CloseHandle(g_hDSTShutdownEvent);
    g_hDSTShutdownEvent = NULL;
    g_hInst = NULL;
}

int StartDST (void) 
{
    DEBUGMSG(TRUE, (TEXT("[TIMESVC]====>Starting DST service")));

    if (!g_hInst)
        return ERROR_SERVICE_DOES_NOT_EXIST;

    if (WaitForSingleObject (g_hDSTShutdownEvent, 0) == WAIT_TIMEOUT)
        return ERROR_SERVICE_ALREADY_RUNNING;

    ResetEvent (g_hDSTShutdownEvent);
    g_hDSTThread = CreateThread (NULL, 0, DSTThread, g_hDSTShutdownEvent, 0, NULL);
    if (g_hDSTThread == NULL) 
    {
        int iErr = GetLastError ();
        SetEvent (g_hDSTShutdownEvent);
        return iErr;
    }

    DEBUGMSG(TRUE, (TEXT("[TIMESVC]====>DST service started")));
    return ERROR_SUCCESS;
}

int StopDST (void) 
{
    if (! g_hInst)
        return ERROR_SERVICE_DOES_NOT_EXIST;

    if (WaitForSingleObject (g_hDSTShutdownEvent, 0) == WAIT_OBJECT_0)
        return ERROR_SERVICE_NOT_ACTIVE;

    SetEvent (g_hDSTShutdownEvent);
    WaitForSingleObject (g_hDSTThread, INFINITE);
    CloseHandle (g_hDSTThread);
    g_hDSTThread = NULL;

    return ERROR_SUCCESS;
}

DWORD GetStateDST (void) 
{
    if (! g_hInst)
        return SERVICE_STATE_UNINITIALIZED;

    if (WaitForSingleObject (g_hDSTShutdownEvent, 0) == WAIT_OBJECT_0)
        return SERVICE_STATE_OFF;

    return SERVICE_STATE_ON;
}
