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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include "stdafx.h"
#include "EventHandler.h"
#include "logger.h"
#include "FilterGraph.h"

VOID CallbackOnMediaEvent(long lEventCode, long lParam1, long lParam2, PVOID pUserData)
{
    LPTSTR pszFnName = TEXT("CallbackOnMediaEvent");
    LogText(__LINE__, pszFnName, TEXT("Received event %s [%08X] Param1 = %08X; Param2 = %08X"), CFilterGraph::GetEventCodeString(lEventCode), lEventCode, lParam1, lParam2);

    if(pUserData != NULL)
    {
        PTEST_EVENT_DATA pTestData = (PTEST_EVENT_DATA) pUserData;

        if(pTestData->hEvent != NULL)
        {
            if(pTestData->lExpectedEventCode == lEventCode)
            {
                pTestData->lParam1 = lParam1;
                pTestData->lParam2 = lParam2;
                SetEvent(pTestData->hEvent);
            }
        }
    }
}

HRESULT InitializeTestEventData(PTEST_EVENT_DATA pTestEventData, long lEventCode)
{
    pTestEventData->lExpectedEventCode = lEventCode;
    pTestEventData->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    DWORD dwLastError = GetLastError();

    if(dwLastError == 0)
    {
        return S_OK;
    }
    else
    {
        return E_FAIL;
    }
}

HRESULT CleanupTestEventData(PTEST_EVENT_DATA pTestEventData)
{
    if(CloseHandle(pTestEventData->hEvent))
    {
        pTestEventData->hEvent = NULL;
        return S_OK;
    }
    else
    {
        return E_FAIL;
    }
}

BOOL WaitForTestEvent(PTEST_EVENT_DATA pTestEventData, DWORD dwMilliseconds)
{
    BOOL retVal = FALSE;

    if(WaitForSingleObject(pTestEventData->hEvent, dwMilliseconds) == WAIT_OBJECT_0)
    {
        retVal = TRUE;
    }

    return retVal;
}

