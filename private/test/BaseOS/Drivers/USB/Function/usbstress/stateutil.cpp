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
/*
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1998-2000  Microsoft Corporation.  All Rights Reserved.

Module Name: stateutil.cpp  

 */

#include "stateutil.h"

HANDLE g_RTCEvent = NULL;
BOOL g_fWakeupSet = FALSE;
DWORD dwWakeCount = 0;

BOOL SetWakeup(WORD wSeconds)
{

    if(wSeconds < 30) {
        RETAILMSG(TRUE, (_T("ERROR: Invalid seconds value")));
        return FALSE;
    }

    SYSTEMTIME sysTime;

    if(!g_fWakeupSet) {
        DWORD dwWakeSrc = SYSINTR_RTC_ALARM;
        if(KernelIoControl(IOCTL_HAL_ENABLE_WAKE, &dwWakeSrc, sizeof(DWORD), NULL, 0, NULL) == FALSE) {
            RETAILMSG(TRUE, (_T("RTC Wakeup Not supported. CANNOT SUPPORT SUSPEND/RESUME")));
            return FALSE;
        }
        g_fWakeupSet = TRUE;
    }
    // set RTC alarm
    GetLocalTime(&sysTime);

    if(!g_RTCEvent == NULL) {
        g_RTCEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if(!g_RTCEvent) {
            RETAILMSG(TRUE, (_T("Failed to create the event Error =%d"), GetLastError()));
            return FALSE;
        }
    }

    WORD wRemain, wQuotient;
    wQuotient = wSeconds;

    wRemain = sysTime.wSecond + wQuotient;
    sysTime.wSecond = wRemain % 60;
    wQuotient = wRemain / 60;

    wRemain = sysTime.wMinute + wQuotient;
    sysTime.wMinute = wRemain % 60;
    wQuotient = wRemain / 60;

    wRemain = sysTime.wHour + wQuotient;
    sysTime.wHour = wRemain % 24;
    wQuotient = wRemain / 24;

    SetKernelAlarm(g_RTCEvent, &sysTime);

    return TRUE;

}

VOID ClearWakeup()
{

    DWORD dwWakeSrc = SYSINTR_RTC_ALARM;

    if(g_fWakeupSet) {
        KernelIoControl(IOCTL_HAL_DISABLE_WAKE, &dwWakeSrc, sizeof(dwWakeSrc), NULL, 0, NULL);
        g_fWakeupSet = FALSE;
    }

    if(!g_RTCEvent) {
        CloseHandle(g_RTCEvent);
        g_RTCEvent = NULL;
    }

}


DWORD ResetDevice(UsbClientDrv * pDriverPtr, BOOL bReEnum)
{

    if(pDriverPtr == NULL)
        return TPR_SKIP;

    if(bReEnum == TRUE)
        NKDbgPrintfW(_T("Now Reset the device and re-enumerate it"));
    else
        NKDbgPrintfW(_T("Now Reset the device and without re-enumerate it"));

    if(pDriverPtr->DisableDevice(bReEnum, 0) == FALSE) {
        NKDbgPrintfW(_T("Failed on disable device with automatic re-enumeration!"));
        return TPR_FAIL;
    }

    return TPR_PASS;
}

DWORD SuspendDevice(UsbClientDrv * pDriverPtr)
{

    if(pDriverPtr == NULL)
        return TPR_SKIP;

    if(pDriverPtr->SuspendDevice(0) == FALSE) {
        NKDbgPrintfW(_T("Failed on suspend call!"));
        return TPR_FAIL;
    }

    return TPR_PASS;
}

DWORD ResumeDevice(UsbClientDrv * pDriverPtr)
{

    if(pDriverPtr == NULL)
        return TPR_SKIP;

    if(pDriverPtr->ResumeDevice(0) == FALSE) {
        NKDbgPrintfW(_T("Failed on resume call!"));
        return TPR_FAIL;
    }

    return TPR_PASS;
}
