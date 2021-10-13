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


// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.

/**************************************************************************

Module Name:

   nfyBrdth.cpp

Abstract:

History:
***************************************************************************/
#include <windows.h>
#include <notify.h>
#include <tux.h>
#include "tuxmain.h"
#include "kutil.h"
#include "clparse.h"

#include "..\names.h"
#include "..\nfyLib\timelib.h"
#include "..\nfyLib\CMMFile.h"
#include "..\nfyLib\CNfyData.h"
extern HINSTANCE    globalExeInst;

#define FLAG_TRIGGER L"trig"
#define FLAG_RANDOM  L"rand"
#define FLAG_COMMAND L"cmd"
#define FLAG_COMMAND_COPY L"cpy"
#define SECOND  (1000)
#define MAX_WAIT_TIME (180 * SECOND)

BOOL ClearAllNotifications()
{
    HANDLE  NotifHandles[100];
    DWORD HandlesNeeded=0;

    CeGetUserNotificationHandles(NULL, 0, &HandlesNeeded);
    if(!HandlesNeeded) return false;

    BOOL bRet = CeGetUserNotificationHandles(NotifHandles, HandlesNeeded, &HandlesNeeded);

    for(DWORD i = 0; i< HandlesNeeded;i++)
        bRet &= CeClearUserNotification(NotifHandles[i]);

    return bRet;
}


//***********************************************************************************
//      Set the filename for nfyApp.exe using the current directory
//***********************************************************************************

VOID SetAppPath(TCHAR* szPath, DWORD dwMaxLength)
{
    if (szPath == NULL)
        return;

    DWORD dwSize = 0;
    dwSize = GetModuleFileName(NULL, szPath, dwMaxLength);
    while ((dwSize > 0) && (szPath[dwSize-1] != TEXT('\\')))
        dwSize--;
    szPath[dwSize] = 0;
    _tcsncat_s(szPath, dwMaxLength, APPNAME1, _tcsclen(APPNAME1));
}

//***********************************************************************************
//      Retrieve the kernel alarm resolution specified by the OEM
//***********************************************************************************

int GetRealTimeClockResolution(void)
{
    int timerResolution = 0;
    DWORD dwRet         = 0; //not used
    DWORD dwTimer       = 0;
    if (!KernelLibIoControl((HANDLE)KMOD_CORE, IOCTL_KLIB_GETALARMRESOLUTION, NULL, 0, &dwTimer, sizeof(dwTimer), &dwRet))
    {
        timerResolution = 10; // 10 seconds is the default value unless changed by OEMs
        info(ECHO, TEXT("Using the DEFAULT accurary of the real-time clock: %d seconds."), timerResolution);
    }
    else
    {
        timerResolution = (int)dwTimer / 1000;
        info(ECHO, TEXT("Accurary of the real-time clock is: %d seconds."), timerResolution);
    }
    return timerResolution;
}

//***********************************************************************************
//        Breadth Tests for CeRunAppAtEvent
//***********************************************************************************

TESTPROCAPI TestCeRunAppAtEvent(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);
    NO_MESSAGES;

    // Get path for nfyApp.exe, used for interprocess communication
    TCHAR szPath[MAX_PATH];
    SetAppPath(szPath, MAX_PATH);

    // Null param test
    if(CeRunAppAtEvent(NULL, NOTIFICATION_EVENT_NONE))
        info(FAIL, TEXT("Line# %d: CeRunAppAtEvent succeeded for null appname"),__LINE__);

    // Invalid event (test only valid for CEBase images - smartphone does not filter events)
    if (0 == CheckConfig())
    {
        if(CeRunAppAtEvent(szPath, 420 ))
            info(FAIL, TEXT("Line# %d: CeRunAppAtEvent succeeded for invalid event"),__LINE__);
    }

    // Good call
    if(!CeRunAppAtEvent(szPath, NOTIFICATION_EVENT_SYNC_END))
        info(FAIL, TEXT("Line# %d: CeRunAppAtEvent failed for good params"),__LINE__);

    // Good call
    if(!CeRunAppAtEvent(szPath, NOTIFICATION_EVENT_NONE))
        info(FAIL, TEXT("Line# %d: CeRunAppAtEvent failed for good params"),__LINE__);

    CeEventHasOccurred(NOTIFICATION_EVENT_SYNC_END, NULL);
    CeEventHasOccurred(NOTIFICATION_EVENT_NONE, NULL);
    Sleep(3000);
    return getCode();
}


//***********************************************************************************
//        Breadth Tests for CeRunAppAtTime
//***********************************************************************************

TESTPROCAPI TestCeRunAppAtTime(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);
    NO_MESSAGES;

    SYSTEMTIME time1;
    GetLocalTime(&time1);

    CNfyData nfyData;   //<-- This creates memory mapped file for interprocess communication
    nfyData.ClearData();

    // Get Pointer to shared memory
    NOTIFTESTDATA *pntd = nfyData.GetNotificationTestData();
    if(!pntd)
    {
        info(FAIL, TEXT("Line# %d: Problems with memory mapped file"),__LINE__);
        goto done;
    }

    // Retrieve the kernel alarm resolution specified by the OEM
    int timerResolution = GetRealTimeClockResolution();

    // Get path for nfyApp.exe, used for interprocess communication
    TCHAR szPath[MAX_PATH];
    SetAppPath(szPath, MAX_PATH);

    // Null pointer
    if(CeRunAppAtTime(NULL, &time1))
        info(FAIL, TEXT("Line# %d: CeRunAppAtTime succeeded for NULL appname pointer"),__LINE__);

    // Invalid system time
    time1.wMonth = 13;
    if(CeRunAppAtTime(szPath, &time1))
        info(FAIL, TEXT("Line# %d: CeRunAppAtTime succeeded for invalid time"),__LINE__);

    // Null system time when App is not in the queue
    if(CeRunAppAtTime(szPath, NULL))
        info(FAIL, TEXT("Line# %d: CeRunAppAtTime succeeded for NULL system time when App is not in the queue"),__LINE__);

    // Good params
    {
        GetLocalTime(&time1);
        if(!CeRunAppAtTime(szPath, &time1))
            info(FAIL, TEXT("Line# %d: CeRunAppAtTime failed for good params"),__LINE__);

        // Verify app launched
        Sleep(2000 + timerResolution*1000);
        if (0==pntd->count)
            info(FAIL, TEXT("Line# %d: CeRunAppAtTime failed to launch app with good params"),__LINE__);
    }

    // Verify replacement of previous request for same app
    {
        const int LAUNCH_A = timerResolution+1;
        const int LAUNCH_B = (2*timerResolution)+1;
        nfyData.ClearData();

        // First request
        GetLocalTime(&time1);
        AddSecondsToSystemTime(&time1, LAUNCH_A);
        if(!CeRunAppAtTime(szPath, &time1))
            info(FAIL, TEXT("Line# %d: CeRunAppAtTime failed for good params"),__LINE__);

        // Overwrite previous launch request with new launch time
        GetLocalTime(&time1);
        AddSecondsToSystemTime(&time1, LAUNCH_B);
        if(!CeRunAppAtTime(szPath, &time1))
            info(FAIL, TEXT("Line# %d: CeRunAppAtTime failed for good params"),__LINE__);

        // Verify app launched
        Sleep(LAUNCH_B*1000 + timerResolution*1000);
        if (1==pntd->count)
        {
            // Verify LAUNCH_B replaced LAUNCH_A
            int actual = SystemTimeDiff(time1, pntd->LaunchTime[0]);
            if (actual>timerResolution)
                info(FAIL, TEXT("Line# %d: CeRunAppAtTime failed to replace previous request"),__LINE__);
        }
        else
        {
            info(FAIL, TEXT("Line# %d: CeRunAppAtTime failed to launch app with good params"),__LINE__);
        }
    }

    // Verify an existing request can be deleted from the launch queue
    {
        nfyData.ClearData();

        // First request
        GetLocalTime(&time1);
        AddSecondsToSystemTime(&time1, timerResolution+1);
        if(!CeRunAppAtTime(szPath, &time1))
            info(FAIL, TEXT("Line# %d: CeRunAppAtTime failed for good params"),__LINE__);

        // Delete the first request
        if(!CeRunAppAtTime(szPath, NULL))
            info(FAIL, TEXT("Line# %d: CeRunAppAtTime failed for good params"),__LINE__);

        // Verify app is not launched
        Sleep(2000 + timerResolution*1000);
        if (1==pntd->count)
            info(FAIL, TEXT("Line# %d: CeRunAppAtTime failed to remove request from launch queue"),__LINE__);
    }

    // Verify many applications can be launched
    {
        const int APPS_TO_RUN = 20;
        nfyData.ClearData();

        for (int i=0; i<APPS_TO_RUN; i++)
        {
            UINT uNumber = 0;
            rand_s(&uNumber);
            int delay = uNumber % 30;

            // Create a new notification
            GetLocalTime(&time1);
            AddSecondsToSystemTime(&time1, delay);
            info(ECHO, TEXT("Notification %d -- Expected to fire in:  %d seconds."), i, delay);
            if (!CeRunAppAtTime(szPath, &time1))
                info(FAIL, TEXT("Line# %d: CeRunAppAtTime failed for localized stress run"),__LINE__);

            // Give the app enough time to fire
            Sleep(delay*1000 + timerResolution*1000);

            // Verify app launched
            if (i+1==pntd->count)
            {
                // Verify launch time is within the RTC resolution
                int actual = SystemTimeDiff(time1, pntd->LaunchTime[i]);
                info(ECHO, TEXT("Notification %d -- Time Difference is:   %d seconds."), i, actual);
                if(actual > timerResolution)
                {
                    // Failure, so quit immediately
                    info(FAIL, TEXT("Line# %d: CeRunAppAtTime launch time of %d seconds exceeds the real-time clock resolution of %d seconds."),__LINE__, actual, timerResolution);
                    break;
                }
            }
            else
            {
                // Failure, so quit immediately
                info(FAIL, TEXT("Line# %d: App %d failed to launch. Exit test run due to launch failure."),__LINE__, i);
                break;
            }
        }
    }

done:
    Sleep(3000);
    return getCode();
}

//***********************************************************************************
//        Breadth Tests for CeSetUserNotification
//***********************************************************************************

TESTPROCAPI TestCeSetUserNotification(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);
    NO_MESSAGES;

    HANDLE h1;
    SYSTEMTIME time1;

    GetLocalTime(&time1);
    AddSecondsToSystemTime( &time1, 25);

    CE_USER_NOTIFICATION UserNotif = {PUN_DIALOG, TEXT("CeSetUserNotification"), TEXT("Notification BVT"), NULL, MAX_PATH, 0};

    // Get path for nfyApp.exe, used for interprocess communication
    TCHAR szPath[MAX_PATH];
    SetAppPath(szPath, MAX_PATH);

    // Bad Handle
    h1 = CeSetUserNotification((HANDLE)0xDEAD, szPath, &time1, &UserNotif);
    if(h1)
        info(FAIL, TEXT("Line# %d: CeSetUserNotification succeeded for invalid handle"),__LINE__);

    // Bad notif struct
    h1 = CeSetUserNotification(NULL, szPath, &time1, (PCE_USER_NOTIFICATION)0xDEAD);
    if(h1)
        info(FAIL, TEXT("Line# %d: CeSetUserNotification succeeded for invalid PCE_USER_NOTIFICATION"),__LINE__);

    // NULL App name
    h1 = CeSetUserNotification(NULL, NULL, &time1, &UserNotif);
    if(h1) info(FAIL, TEXT("Line# %d: CeSetUserNotification succeeded for null app name"),__LINE__);

    CeClearUserNotification(h1);

    // Good Call
    h1 = CeSetUserNotification(NULL, szPath, &time1, &UserNotif);
    if(!h1) info(FAIL, TEXT("Line# %d: CeSetUserNotification failed for valid parameters. Error %d"), __LINE__,GetLastError());

    if(!CeClearUserNotification(h1))
        info(FAIL, TEXT("Line# %d: CeClearUserNotification failed for valid handle"),__LINE__);

    // Bad Date
    time1.wMonth += 12;
    h1 = CeSetUserNotification(NULL, szPath, &time1, &UserNotif);
    if(h1) info(FAIL, TEXT("Line# %d: CeSetUserNotification succeeded for invalid date"),__LINE__);
    time1.wMonth -= 12;


    // NULL Dialog Title and text, Should succeed with no dialog displayed
    UserNotif.pwszDialogTitle = UserNotif.pwszDialogText = NULL;
    h1 = CeSetUserNotification(NULL, szPath, &time1, &UserNotif);
    if(!h1) info(FAIL, TEXT("Line# %d: CeSetUserNotification failed for Null dialog Name and text. Error %d"),__LINE__, GetLastError());

    if(!CeClearUserNotification(h1))
        info(FAIL, TEXT("Line# %d: CeClearUserNotification failed for valid handle. Error %d"), __LINE__, GetLastError());

    return getCode();
}



//***********************************************************************************
//        Breadth Tests for CeClearUserNotification
//***********************************************************************************

TESTPROCAPI TestCeClearUserNotification(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);
    NO_MESSAGES;
    HANDLE h1;
    SYSTEMTIME time1;

    GetLocalTime(&time1);
    AddSecondsToSystemTime( &time1, 25);

    // Get path for nfyApp.exe, used for interprocess communication
    TCHAR szPath[MAX_PATH];
    SetAppPath(szPath, MAX_PATH);

    CE_USER_NOTIFICATION UserNotif = {PUN_DIALOG, TEXT("CeClearUserNotification"), TEXT("Notification BVT"), NULL, MAX_PATH, 0};
    h1 = CeSetUserNotification(NULL, szPath, &time1, &UserNotif);
    if(!h1)
        info(FAIL, TEXT("Line# %d: CeSetUserNotification failed for valid parameters"),__LINE__);

    // Null handle
    if(CeClearUserNotification(NULL))
        info(FAIL, TEXT("Line# %d: CeClearUserNotification succeeded for Null handle"),__LINE__);

    // Invalid handle
    if(CeClearUserNotification((HANDLE)0xDEAD))
        info(FAIL, TEXT("Line# %d: CeClearUserNotification succeeded for invalid handle"),__LINE__);

    // Good call
    if(!CeClearUserNotification(h1))
        info(FAIL, TEXT("Line# %d: CeClearUserNotification failed for valid handle"),__LINE__);

    // Again. should fail
    if(CeClearUserNotification(h1))
        info(FAIL, TEXT("Line# %d: CeClearUserNotification passed 2nd time"),__LINE__);

    return getCode();
}


//***********************************************************************************
//        Breadth Tests for CeGetUserNotificationPreferences
//***********************************************************************************

TESTPROCAPI TestCeGetUserNotificationPreferences(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);
    NO_MESSAGES;

    CE_USER_NOTIFICATION UserNotif = {PUN_DIALOG, TEXT("CeGetUserNotificationPreferences"), TEXT("Notification Test Txt"), NULL, MAX_PATH, 0};

    // Null CE_USER_NOTIFICATION
    if(CeGetUserNotificationPreferences(NULL, NULL))
        info(FAIL, TEXT("Line# %d: CeGetUserNotificationPreferences succeeded for NULL CE_USER_NOTIFICATION"),__LINE__);

    // Invalid Parent window handle
    if(CeGetUserNotificationPreferences((HWND)0xDEAD, &UserNotif))
        info(FAIL, TEXT("Line# %d: CeGetUserNotificationPreferences Passed with invalid parent window handle"),__LINE__);

    // Invalid notif struct
    if(CeGetUserNotificationPreferences(NULL, (PCE_USER_NOTIFICATION)0xDEAD))
        info(FAIL, TEXT("Line# %d: CeGetUserNotificationPreferences Passed with invalid notif struct"),__LINE__);


    // NULL value for CE_USER_NOTIFICATION.pwszSound
    if(CeGetUserNotificationPreferences(NULL, &UserNotif))
        info(FAIL, TEXT("Line# %d: CeGetUserNotificationPreferences passed for null CE_USER_NOTIFICATION.pwszSound"),__LINE__);

    UserNotif.pwszSound = new TCHAR[MAX_PATH];
    if (UserNotif.pwszSound)
    {
        // Invalid value for CE_USER_NOTIFICATION.nMaxSound
        UserNotif.nMaxSound = 1;
        if( CeGetUserNotificationPreferences(NULL, &UserNotif))
            info(FAIL, TEXT("Line# %d: CeGetUserNotificationPreferences passed for invalid CE_USER_NOTIFICATION.nMaxSound"),__LINE__);
        UserNotif.nMaxSound = MAX_PATH;

        // Valid Breadths
        PostQuitMessage(0);    //    This cancels the following dialog
        if( CeGetUserNotificationPreferences(NULL, &UserNotif)) // should fail because dialog is cancelled.
            info(FAIL, TEXT("Line# %d: CeGetUserNotificationPreferences BVT failed"),__LINE__);

        delete (UserNotif.pwszSound);
        UserNotif.pwszSound = NULL;
    }

    return getCode();
}

//***********************************************************************************
//        Breadth Tests for CeHandleAppNotifications
//***********************************************************************************

TESTPROCAPI TestCeHandleAppNotifications(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);
    NO_MESSAGES;

    // Null appname
    if(CeHandleAppNotifications(NULL))
        info(FAIL, TEXT("Line# %d: CeHandleAppNotifications passed for invalid Appname"),__LINE__);

    HANDLE h1;
    SYSTEMTIME time1;

    GetLocalTime(&time1);
    AddSecondsToSystemTime( &time1, 1);

    // Get path for nfyApp.exe, used for interprocess communication
    TCHAR szPath[MAX_PATH];
    SetAppPath(szPath, MAX_PATH);

    CE_USER_NOTIFICATION UserNotif = {PUN_DIALOG|PUN_SOUND|PUN_REPEAT, NULL, NULL, TEXT("default"), MAX_PATH*sizeof(TCHAR), 0};
    h1 = CeSetUserNotification(NULL, szPath, &time1, &UserNotif);
    if(!h1) info(FAIL, TEXT("Line# %d: CeSetUserNotification failed for valid parameters"),__LINE__);

    PrintSystemTime(&time1);

    Sleep(2000);
    // Valid call
    if(!CeHandleAppNotifications(szPath))
        info(FAIL, TEXT("Line# %d: CeHandleAppNotifications failed for valid Appname"),__LINE__);

    if(!CeClearUserNotification(h1))
        info(ECHO, TEXT("CeClearUserNotification failed for valid handle"),__LINE__);

    return getCode();
}

//***********************************************************************************
//        Breadth Tests for CeSetUserNotificationEx
//***********************************************************************************

TESTPROCAPI TestCeSetUserNotificationEx(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);
    NO_MESSAGES;

    SYSTEMTIME time1;
    GetLocalTime(&time1);
    AddSecondsToSystemTime( &time1, 25);

    // Get path for nfyApp.exe, used for interprocess communication
    TCHAR szPath[MAX_PATH];
    SetAppPath(szPath, MAX_PATH);

    CE_USER_NOTIFICATION UserNotif = {PUN_DIALOG, TEXT("CeSetUserNotificationEx"), TEXT("Notification BVT"), NULL, MAX_PATH, 0};
    CE_NOTIFICATION_TRIGGER NotifTrig = {sizeof(CE_NOTIFICATION_TRIGGER), CNT_TIME, 0, szPath, TEXT(""), 0, 0};

    NotifTrig.stStartTime = time1;

    UserNotif.pwszSound = new TCHAR[MAX_PATH];
    if (UserNotif.pwszSound)
        UserNotif.pwszSound[0] = '\0';

    HANDLE h1;

    // Null Trigger
    h1 = CeSetUserNotificationEx(NULL, NULL, &UserNotif);
    if( h1)
        info(FAIL, TEXT("Line# %d: CeSetUserNotificationEx succeeded for null trigger"),__LINE__);

    // Null UserNotification    --valid call
    h1 = CeSetUserNotificationEx(NULL, &NotifTrig, NULL);
    if( !h1)
        info(FAIL, TEXT("Line# %d: CeSetUserNotificationEx failed for null UserNotification"),__LINE__);
    if(!CeClearUserNotification(h1))
        info(FAIL, TEXT("Line# %d: CeClearUserNotification failed for valid handle"),__LINE__);

    // valid call
    h1 = CeSetUserNotificationEx(NULL, &NotifTrig, &UserNotif);
    if( !h1)
        info(FAIL, TEXT("Line# %d: CeSetUserNotificationEx failed for valid parameters"),__LINE__);
    if(!CeClearUserNotification(h1))
        info(FAIL, TEXT("Line# %d: CeClearUserNotification failed for valid handle"),__LINE__);

    //---- Invalid Notif Trigger
    //    Invalid dwSize
    NotifTrig.dwSize -= 500;
    h1 = CeSetUserNotificationEx(NULL, &NotifTrig, &UserNotif);
    if( h1)
        info(FAIL, TEXT("Line# %d: CeSetUserNotificationEx succeeded for invalid dwSize"),__LINE__);
    CeClearUserNotification(h1);
    NotifTrig.dwSize += 500;

    //    Invalid dwType
    NotifTrig.dwType += 500;
    h1 = CeSetUserNotificationEx(NULL, &NotifTrig, &UserNotif);
    if( h1)
        info(FAIL, TEXT("Line# %d:  CeSetUserNotificationEx succeeded for Invalid dwType"),__LINE__);
    NotifTrig.dwType -= 500;
    CeClearUserNotification(h1);

    //----

    //--- Invalid UserNotif
    UserNotif.ActionFlags = PUN_SOUND;
    //  invalid pwszSound
    if (UserNotif.pwszSound)
    {
        delete (UserNotif.pwszSound);
        UserNotif.pwszSound = NULL;
    }

    h1 = CeSetUserNotificationEx(NULL, &NotifTrig, &UserNotif);
    if( h1)
        info(FAIL, TEXT("Line# %d: CeSetUserNotificationEx succeeded for invalid pwszSound"),__LINE__);
    //---

    return getCode();
}


//***********************************************************************************
//        Breadth Tests for CeGetUserNotificationHandles
//***********************************************************************************

TESTPROCAPI TestCeGetUserNotificationHandles(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);
    NO_MESSAGES;

    HANDLE h1=NULL;
    HANDLE h2=NULL;
    DWORD cHandles=0;
    DWORD HandlesNeeded=0;

    {
        // Clear all existing notifications
        HANDLE handles[10]; //will be <10
        CeGetUserNotificationHandles(NULL, 0, &HandlesNeeded);
        CeGetUserNotificationHandles(handles, HandlesNeeded, &cHandles);
        for(DWORD dwIndex=0; dwIndex<HandlesNeeded; dwIndex++)
            CeClearUserNotification(handles[dwIndex]);
    }

    // Get path for nfyApp.exe, used for interprocess communication
    TCHAR szPath[MAX_PATH];
    SetAppPath(szPath, MAX_PATH);

    //Set a notification
    SYSTEMTIME time1;
    GetLocalTime(&time1);
    AddSecondsToSystemTime( &time1, 25);
    CE_USER_NOTIFICATION UserNotif = {PUN_DIALOG, TEXT("CeGetUserNotificationHandles"), TEXT("Notification BVT"), NULL, MAX_PATH, 0};
    h1 = CeSetUserNotification(NULL, szPath, &time1, &UserNotif);
    if(!h1) info(FAIL, TEXT("Line# %d: CeSetUserNotification failed for valid parameters"));

    //--Valid call
    if(!CeGetUserNotificationHandles(NULL, 0, &HandlesNeeded))
        info(FAIL, TEXT("Line# %d: CeGetUserNotificationHandles failed for valid param (find count)"),__LINE__);
    if(HandlesNeeded != 1)
        info(FAIL, TEXT("Line# %d: CeGetUserNotificationHandles returned %d notifications (<>1)"),__LINE__, HandlesNeeded);

    //--Invalid calls
    if(CeGetUserNotificationHandles(NULL, 10, &HandlesNeeded))
        info(FAIL, TEXT("Line# %d: CeGetUserNotificationHandles passed for invalid param"),__LINE__);
    if(CeGetUserNotificationHandles(NULL, 10, NULL))
        info(FAIL, TEXT("Line# %d: CeGetUserNotificationHandles passed for invalid param"),__LINE__);
    if(CeGetUserNotificationHandles(&h2, 0, NULL))
        info(FAIL, TEXT("Line# %d: CeGetUserNotificationHandles passed for invalid param"),__LINE__);

    //--Valid call
    if(!CeGetUserNotificationHandles(&h2, 1, &HandlesNeeded))
        info(FAIL, TEXT("Line# %d: CeGetUserNotificationHandles failed for valid param"),__LINE__);

    if(!CeClearUserNotification(h1))
        info(FAIL, TEXT("Line# %d: CeClearUserNotification failed for valid handle"),__LINE__);

    return getCode();
}



//***********************************************************************************
//        Breadth Tests for CeGetUserNotification
//***********************************************************************************

TESTPROCAPI TestCeGetUserNotification(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);
    NO_MESSAGES;

    SYSTEMTIME time1;
    GetLocalTime(&time1);

    // Get path for nfyApp.exe, used for interprocess communication
    TCHAR szPath[MAX_PATH];
    SetAppPath(szPath, MAX_PATH);

    CE_USER_NOTIFICATION UserNotif = {PUN_DIALOG, TEXT("CeGetUserNotification"), TEXT("Notification BVT"), NULL, MAX_PATH, 0};

    CE_NOTIFICATION_TRIGGER NotifTrig = {sizeof(CE_NOTIFICATION_TRIGGER), CNT_TIME, 0, szPath, TEXT(""), 0, 0};
    NotifTrig.stStartTime = time1;

    HANDLE h2 = CeSetUserNotificationEx(NULL, &NotifTrig, &UserNotif);
    if( !h2 )
        info(FAIL, TEXT("Line# %d: CeSetUserNotificationEx BVT failed"),__LINE__);

    DWORD cBufferSize=0;
    BYTE *pBuffer = NULL;
    DWORD cBytesNeeded=0;
    SetLastError(0);

    //--NULL handle. should fail
    if(CeGetUserNotification(NULL, 0, &cBytesNeeded, NULL))
        info(FAIL, TEXT("Line# %d: CeGetUserNotification Passed with invalid params."),__LINE__);

    //--Invalid handle. should fail
    if(CeGetUserNotification((HANDLE)100, cBufferSize, &cBytesNeeded, pBuffer))
        info(FAIL, TEXT("Line# %d: CeGetUserNotification Passed with invalid params."),__LINE__);


    //--Invalid pointer. should fail
    if(CeGetUserNotification(h2, cBufferSize, &cBytesNeeded, NULL))
        info(FAIL, TEXT("Line# %d: CeGetUserNotification Passed with invalid params."),__LINE__);

    //--Invalid call. should fail but set cBytesNeeded
    if(CeGetUserNotification(h2, cBufferSize, &cBytesNeeded, pBuffer))
        info(FAIL, TEXT("Line# %d: CeGetUserNotification Passed with invalid params."),__LINE__);

    if(cBytesNeeded == 0 || cBytesNeeded > 500)
        info(FAIL, TEXT("Line# %d: invalid cBytesNeeded %d"), cBytesNeeded);


    cBufferSize = cBytesNeeded;
    pBuffer = (BYTE *) new BYTE[cBytesNeeded]; //(BYTE *)LocalAlloc(LMEM_FIXED, cBytesNeeded);

    if (pBuffer)
    {
        //--Valid call. should pass
        if(!CeGetUserNotification(h2, cBufferSize, &cBytesNeeded, pBuffer))
            info(FAIL, TEXT("Line# %d: CeGetUserNotification failed for valid params %d"),__LINE__,GetLastError());
        delete[] pBuffer;
        pBuffer = NULL;
    }

    CeClearUserNotification(h2);

    return getCode();
}



//***********************************************************************************
//        Functional Tests for Notification
//          The test sets timed notification and checks if the app is lauched at the correct time
//        The app is expected to get launched within 10sec of expected time
//           The app should be getting command line params as expected
//***********************************************************************************

TESTPROCAPI TestNotificationsTime(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);
    NO_MESSAGES;

    ClearAllNotifications();

    CNfyData nfyData;    //<-- This creates memory mapped file for interprocess communication
    nfyData.ClearData();

    SYSTEMTIME time1,time2;
    HANDLE h1;

    // Get path for nfyApp.exe, used for interprocess communication
    TCHAR szPath[MAX_PATH];
    SetAppPath(szPath, MAX_PATH);

    CE_NOTIFICATION_TRIGGER NotifTrig[] =
        {
                {sizeof(CE_NOTIFICATION_TRIGGER), CNT_TIME, 0, szPath, TEXT("WINDOWS CE IS THE BEST"), 0, 0},
                {sizeof(CE_NOTIFICATION_TRIGGER), CNT_TIME, 0, szPath, TEXT("1"), 0, 0},
                {sizeof(CE_NOTIFICATION_TRIGGER), CNT_TIME, 0, szPath, TEXT("2"), 0, 0},
                {sizeof(CE_NOTIFICATION_TRIGGER), CNT_TIME, 0, szPath, TEXT(""), 0, 0},
                {sizeof(CE_NOTIFICATION_TRIGGER), CNT_TIME, 0, szPath, TEXT("WINDOWS CE ONE"), 0, 0},
                {sizeof(CE_NOTIFICATION_TRIGGER), CNT_TIME, 0, szPath, TEXT("WINDOWS CE TWO"), 0, 0},
                {sizeof(CE_NOTIFICATION_TRIGGER), CNT_TIME, 0, szPath, TEXT("WINDOWS CE THREE"), 0, 0}
        };
    int delay[] = {6, 15, 126, 337, 450, 661, 875};   // delay in seconds
    int num = sizeof(NotifTrig)/sizeof(NotifTrig[0]);

    // Retrieve the kernel alarm resolution specified by the OEM
    int timerResolution = GetRealTimeClockResolution();

    // Setup a series of Timed Notifications
    GetLocalTime(&time1);
    for(int i=0;i<num;i++)
    {
        time2 = time1;
        AddSecondsToSystemTime( &time2, delay[i]);
        NotifTrig[i].stStartTime = time2;

        // Setup to launch notification app without alert dialog
        h1 = CeSetUserNotificationEx(NULL, &NotifTrig[i], NULL);
        if(!h1)
        {
            info(FAIL, TEXT("Line# %d: CeSetUserNotificationEx failed for valid parameters"),__LINE__);
            goto done;
        }
        else
        {
            info(ECHO, TEXT("Timed Notification %d initialized and expected in %d seconds."), i,
                SystemTimeDiff(time1, NotifTrig[i].stStartTime));
        }
    }

    // Get Pointer to shared memory
    NOTIFTESTDATA *pntd = nfyData.GetNotificationTestData();
    if(!pntd)
    {
        info(FAIL, TEXT("Line# %d: Problems with memory mapped file"),__LINE__);
        goto done;
    }


    // Wait till all Notifications have fired
    int last = 0;
    for(i=0;i<=1000;i++)
    {
        while (last!=pntd->count)
        {
            info(ECHO, TEXT("Timed Notification %d fired. Expected: %d seconds, Actual %d seconds."), last,
                SystemTimeDiff(time1, NotifTrig[last].stStartTime),
                SystemTimeDiff(time1, pntd->LaunchTime[last])+1);
            last++;
        }
        if (pntd->count >= num) break;
        Sleep(1000);
    }


    // Check cmdlines and time of launch
    if(pntd->count < num)
    {
        info(FAIL, TEXT("Line# %d: Only %d out of %d notify apps successfully launched."),__LINE__, pntd->count, num);
        if (pntd->count == 0)
            info(ECHO, TEXT("Verify file %s exists and try again."), szPath);
    }

    for(i=0;i<pntd->count;i++)
    {
        if(0 != _tcscmp(pntd->CmdLine[i], NotifTrig[i].lpszArguments ) ) // if equal
            info(FAIL, TEXT("Line# %d: App received wrong command line: %s instead of %s"),__LINE__, pntd->CmdLine[i], NotifTrig[i].lpszArguments);

        // Check time when app was launched
        int actual = SystemTimeDiff(NotifTrig[i].stStartTime, pntd->LaunchTime[i]);
        info(ECHO, TEXT("Time Difference for notification %d is: %d seconds."), i, actual);
        if(actual > timerResolution)
        {
            info(ECHO, TEXT("Expected Launch Time:"));
            PrintSystemTime(&NotifTrig[i].stStartTime);
            info(FAIL, TEXT("Line# %d: Actual Launch Time:"),__LINE__);
            PrintSystemTime(&pntd->LaunchTime[i]);
        }
    }

done:
    ClearAllNotifications();

    return getCode();
 }


//***********************************************************************************
//        Functional Tests for Event based Notification
//          The test sets event based notification and then it simulates the events
//        It varifies that the app gets launched as expected
//***********************************************************************************


 TESTPROCAPI TestNotificationsEvent(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
 {
     UNREFERENCED_PARAMETER(lpFTE);
     UNREFERENCED_PARAMETER(tpParam);
     NO_MESSAGES;

     // Get random seed from tux
     DWORD dwRandomSeed = ((TPS_EXECUTE *)tpParam)->dwRandomSeed;

     ClearAllNotifications();

     //TODO: Add check to make sure this test has TCB Level Access.

     CNfyData nfyData;    //<-- This creates memory mapped file for interprocess communication
     nfyData.ClearData();

     HANDLE h1;

     // Get path for nfyApp.exe, used for interprocess communication
     TCHAR szPath[MAX_PATH];
     SetAppPath(szPath, MAX_PATH);

     CE_NOTIFICATION_TRIGGER NotifTrig[] =
     {
         {sizeof(CE_NOTIFICATION_TRIGGER), CNT_EVENT, NOTIFICATION_EVENT_TIME_CHANGE,    szPath, TEXT("1"), 0, 0},
         {sizeof(CE_NOTIFICATION_TRIGGER), CNT_EVENT, NOTIFICATION_EVENT_SYNC_END,        szPath, TEXT("2"), 0, 0},
         {sizeof(CE_NOTIFICATION_TRIGGER), CNT_EVENT, NOTIFICATION_EVENT_ON_AC_POWER,    szPath, TEXT("3"), 0, 0},
         {sizeof(CE_NOTIFICATION_TRIGGER), CNT_EVENT, NOTIFICATION_EVENT_OFF_AC_POWER,    szPath, TEXT("4"), 0, 0},
         {sizeof(CE_NOTIFICATION_TRIGGER), CNT_EVENT, NOTIFICATION_EVENT_NET_CONNECT,    szPath, TEXT("5"), 0, 0},
         {sizeof(CE_NOTIFICATION_TRIGGER), CNT_EVENT, NOTIFICATION_EVENT_NET_DISCONNECT,    szPath, TEXT("6"), 0, 0},
         {sizeof(CE_NOTIFICATION_TRIGGER), CNT_EVENT, NOTIFICATION_EVENT_DEVICE_CHANGE,    szPath, TEXT("7"), 0, 0},
         {sizeof(CE_NOTIFICATION_TRIGGER), CNT_EVENT, NOTIFICATION_EVENT_IR_DISCOVERED,    szPath, TEXT("8"), 0, 0},
         {sizeof(CE_NOTIFICATION_TRIGGER), CNT_EVENT, NOTIFICATION_EVENT_RS232_DETECTED,    szPath, TEXT("9"), 0, 0}
     };
     int num = sizeof(NotifTrig)/sizeof(NotifTrig[0]);



     // Setup a series of Timed Notifications
     TCHAR szCommandLine[64]=L"";
     LPTSTR szTmpArg = NULL;
     for(int i=0;i<num;i++)
     {
         // Setup to launch notification app without alert dialog
         szTmpArg = NotifTrig[i].lpszArguments;
         StringCchPrintf(szCommandLine, _countof(szCommandLine), L"/%s %s /%s %u",
             FLAG_COMMAND, szTmpArg, FLAG_RANDOM, dwRandomSeed);
         NotifTrig[i].lpszArguments = szCommandLine;
         h1 = CeSetUserNotificationEx(NULL, &NotifTrig[i], NULL);
         NotifTrig[i].lpszArguments = szTmpArg;
         if( !h1)
             info(FAIL, TEXT("CeSetUserNotificationEx failed for valid parameters"));
     }


     // Get Pointer to shared memory
     NOTIFTESTDATA *pntd = nfyData.GetNotificationTestData();
     if(!pntd)
         info(FAIL, TEXT("Problems with memory mapped file"));

     TCHAR szTriggerMarker[64] = L"";
     // Simulate all Notification events and wait till they have fired
     for (i=0;i<num;i++)
     {
         // Append a random seed to the commandline so that if another app triggers
         // the same event, we can differentiate between the two
         StringCchPrintf(szTriggerMarker, _countof(szTriggerMarker), L"/%s %u /%s %s",
             FLAG_TRIGGER, dwRandomSeed, FLAG_COMMAND_COPY, NotifTrig[i].lpszArguments);
         if (!CeEventHasOccurred( NotifTrig[i].dwEvent, szTriggerMarker))
             info(FAIL, TEXT("Line# %d: CeEventHasOccurred %d call failed"),__LINE__, i);
     }

     // Loop until all instances of the helper application have been launched
     int launchCount = 0;
     int lastIndex = 0;
     DWORD dwI = 0;
     CClParse* pCmdLine = NULL;
     DWORD dwTmpRandomSeed = 0;
     TCHAR szTmpCommandLine[64]=L"";
     TCHAR szTmpCommandLineCopy[64]=L"";
     while ((launchCount<num) && (dwI < (MAX_WAIT_TIME/SECOND)))
     {
         for(i=lastIndex;i<pntd->count;i++)
         {
             pCmdLine = new CClParse(pntd->CmdLine[i]);
             // The commandline recieved by the helper app will consist of two parts:
             // the command string that was registered with CeSetUserNotificationEx and
             // the command string that was appended by CeEventHasOccurred. The random seed
             // and the actual command parameter in both commandlines must match.
             if (NULL == pCmdLine)
             {
                 continue;
             }
             info(ECHO, L"%s launched with commandline [%s] at index %u", APPNAME1, pntd->CmdLine[i], i);
             // Check for random seed that should be appended by this test when calling CeEventHasOccurred()
             if ((!pCmdLine->GetOptVal(FLAG_TRIGGER, &dwTmpRandomSeed)) ||
                 (dwTmpRandomSeed != dwRandomSeed))
             {
                 info(ECHO,
                     TEXT("Wrong or missing random seed, ignoring entry (another process may have called CeEventHasOccurred)"));
                 continue;
             }
             // Check for random seed in the commandline passed to CeSetUserNotificationEx
             if ((!pCmdLine->GetOptVal(FLAG_RANDOM, &dwTmpRandomSeed)) ||
                 (dwTmpRandomSeed != dwRandomSeed))
             {
                 info(ECHO,
                     TEXT("App launched with wrong random seed, ignoring entry (possibly from a previous iteration of this test)"));
                 continue;
             }
             // Check that the argument matches the expected value
             if (!pCmdLine->GetOptString(FLAG_COMMAND, szTmpCommandLine, _countof(szTmpCommandLine)) ||
                 !pCmdLine->GetOptString(FLAG_COMMAND_COPY, szTmpCommandLineCopy, _countof(szTmpCommandLineCopy)) ||
                 (0 != _tcsncmp(szTmpCommandLine, szTmpCommandLineCopy, _countof(szTmpCommandLine))))
             {
                 info(FAIL, TEXT("App received wrong command line: %s"), pntd->CmdLine[i]);
                 continue;
             }
             // Everything looks okay, continue
             launchCount++;
         }
         // Save the last index where we looked in the pntd structure
         lastIndex = i;
         // Give apps a time to start
         Sleep(SECOND);
         // Increment the loop counter
         dwI++;
     }

     // Check if all the events resulted in app launches
     if (launchCount < num)
     {
         info(FAIL, TEXT("Not enough launches of %s occured: [%u/%u]"), APPNAME1, launchCount, num);
     }

     return getCode();
 }

