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

typedef struct _TEST_EVENT_DATA
{
    HANDLE hEvent;
    long   lExpectedEventCode;
    long   lParam1;
    long   lParam2;
} TEST_EVENT_DATA, *PTEST_EVENT_DATA;

VOID CallbackOnMediaEvent(long lEventCode, long lParam1, long lParam2, PVOID pUserData);
HRESULT InitializeTestEventData(PTEST_EVENT_DATA pTestEventData, long lEventCode);
HRESULT CleanupTestEventData(PTEST_EVENT_DATA pTestEventData);
BOOL WaitForTestEvent(PTEST_EVENT_DATA pTestEventData, DWORD dwMilliseconds);

