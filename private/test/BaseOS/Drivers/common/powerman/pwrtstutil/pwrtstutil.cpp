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
#include "pwrtstutil.h"

VOID
CSysPwrStates::VerifyWakeupResources(){


    m_fCanRTCEnabled = FALSE;

    //check RTC
    DWORD dwWakeSrc = SYSINTR_RTC_ALARM;

    BOOL fIoctlRet = FALSE;
    if(m_fKMode){
        fIoctlRet = KernelIoControl(IOCTL_HAL_ENABLE_WAKE,                
                    &dwWakeSrc,                            
                    sizeof(dwWakeSrc),                    
                    NULL,                                
                    0,                                    
                    NULL);
    }
    else if(m_hTUDFile != NULL){
        fIoctlRet = DeviceIoControl(m_hTUDFile, 
                          IOCTL_TUD_ENABLE_WAKEUPRESOURCE, 
                    &dwWakeSrc,                            
                    sizeof(dwWakeSrc),                    
                    NULL,                                
                    0,                                    
                    NULL, NULL);
    }
    if(fIoctlRet == FALSE){
        NKDbgPrintfW(_T("WARN: The system does not support enabling waking up from suspend using RTC interrupt!"));
    }
    else {
        m_fCanRTCEnabled = TRUE;
    }
    
}

VOID
CSysPwrStates::CleanupAllWakeupResources(){
    //cleanup RTC
    DWORD dwWakeSrc = SYSINTR_RTC_ALARM;
    if(m_fCanRTCEnabled == TRUE){
        if(m_fKMode == TRUE){
            KernelIoControl(IOCTL_HAL_DISABLE_WAKE,                
                        &dwWakeSrc,                            
                        sizeof(dwWakeSrc),                    
                        NULL,                                
                        0,                                    
                        NULL);
        }
        else{
            DeviceIoControl(m_hTUDFile, 
                          IOCTL_TUD_DISABLE_WAKEUPRESOURCE, 
                    &dwWakeSrc,                            
                    sizeof(dwWakeSrc),                    
                    NULL,                                
                    0,                                    
                    NULL, NULL);        
        }
        m_fUsingRTCNow = FALSE;
    }

}

BOOL
CSysPwrStates::SetupRTCWakeupTimer(WORD wSeconds){

    Lock();

    if(m_fKMode == FALSE && m_hTUDFile == NULL){
        NKDbgPrintfW(_T("ERROR: test is in user mode, however the test driver uitility is not in the system!"));
        Unlock();
        return FALSE;
    }
    if(m_fCanRTCEnabled == FALSE){
        NKDbgPrintfW(_T("ERROR: RTC is not a valid wakeup resource in this system!"));
        Unlock();
        return FALSE;
    }

    if(m_fUsingRTCNow == TRUE){
        NKDbgPrintfW(_T("ERROR: RTC is already using by another thread in this system!"));
        Unlock();
        return FALSE;
    }
   
    SYSTEMTIME sysTime;

    // set RTC alarm
    GetLocalTime(&sysTime);

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

    memset((PBYTE)&m_trigger, 0, sizeof(m_trigger));
    
    m_trigger.dwSize = sizeof(m_trigger);
    m_trigger.dwType = CNT_TIME;
    m_trigger.dwEvent = 0;
    m_trigger.lpszApplication = L"\\\\.\\Notifications\\NamedEvents\\SetAlarm";
    m_trigger.lpszArguments = NULL;
    m_trigger.stStartTime = sysTime;

    m_hRTCNotify = CeSetUserNotificationEx(NULL, &m_trigger, NULL);
    if (m_hRTCNotify == NULL){
        NKDbgPrintfW(_T("CeSetUserNotificationEx failed. Error =%d"), GetLastError());
        Unlock();
        return FALSE;
    }

    NKDbgPrintfW(_T("The RTC Alarm is set to wake up the device in %d sec."), wSeconds);
    NKDbgPrintfW(_T("In case the device does not wake up, check the implementation of OEMSetAlarmTime() function on your BSP.") );

    m_fUsingRTCNow = TRUE;

    Unlock();
    return TRUE;    

}

VOID
CSysPwrStates::CleanupRTCTimer(){

    Lock();

    //clear timer notification
    if(m_hRTCNotify!= NULL){
        CeClearUserNotification(m_hRTCNotify);
        m_hRTCNotify = NULL;
    }

    m_fUsingRTCNow = FALSE;

    Unlock();
    
}


BOOL SimulateUserEvent(VOID){
    BOOL fRet = FALSE;
    HANDLE hEvent = NULL;

    hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, USER_ACTIVITY_RESET_EVT);
    if(NULL == hEvent)
    {
        NKDbgPrintfW(_T("SimulateUserEvent: Failed to create activity timer event \"%s\"\r\n"), 
            USER_ACTIVITY_RESET_EVT);
        return FALSE;
    }

    NKDbgPrintfW(TEXT("SimulateUserEvent: Simulating user activity...\r\n"));
    fRet = SetEvent(hEvent);

    if(hEvent){
        CloseHandle(hEvent);
    }
    
    return fRet;
}


BOOL SimulateSystemEvent(VOID){
    BOOL fRet = FALSE;
    HANDLE hEvent = NULL;

    hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, SYSTEM_ACTIVITY_RESET_EVT);
    if(NULL == hEvent)
    {
        NKDbgPrintfW(TEXT("SimulateUserEvent: Failed to create activity timer event \"%s\"\r\n"), 
            SYSTEM_ACTIVITY_RESET_EVT);
        return FALSE;
    }
    
    NKDbgPrintfW(TEXT("SimulateSystemEvent: Simulating system activity...\r\n"));
    fRet = SetEvent(hEvent);
    
    if(hEvent)
    {
        CloseHandle(hEvent);
    }
    return fRet;
}


BOOL SimulateReloadTimeoutsEvent(VOID){
    BOOL fRet = FALSE;
    HANDLE hEvent = NULL;

    hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, ACTIVITY_TIMER_RELOAD_EVT);
    if(NULL == hEvent)
    {
        NKDbgPrintfW(_T("SimulateReloadTimeoutsEvent: Failed to create ReloadTimeouts event \"%s\"\r\n"), 
            ACTIVITY_TIMER_RELOAD_EVT);
        return FALSE;
    }

    NKDbgPrintfW(TEXT("SimulateReloadTimeoutsEvent: Simulating ReloadTimeouts activity...\r\n"));
    fRet = SetEvent(hEvent);

    if(hEvent)
    {
        CloseHandle(hEvent);
    }
    return fRet;
}


BOOL IsCurrentPowerState(LPTSTR pszState){
    BOOL fRet = FALSE;
    TCHAR szCurState[MAX_PATH] = {0};
    DWORD dwCurFlags = 0;
    DWORD dwErr = 0;

    dwErr = GetSystemPowerState(szCurState, MAX_PATH, &dwCurFlags);
    if(ERROR_SUCCESS != dwErr)
    {
        SetLastError(dwErr);
        NKDbgPrintfW(_T("GetSystemPowerState() call failed, Error=0x%x"), dwErr);
        goto Error;
    }
    NKDbgPrintfW(_T("Current state is: %s"), szCurState);
    fRet = (0 == _tcsicmp(szCurState, pszState));

Error:
    return fRet;
}

VOID LogPowerMessage(PPOWER_BROADCAST pPB){

    PPOWER_BCAST_MSG pPBM = (PPOWER_BCAST_MSG)pPB;
    NKDbgPrintfW(TEXT("Power Broadcast Message Conents:"));
    switch(pPBM->Message)
    {
        case PBT_TRANSITION:
            NKDbgPrintfW(TEXT("\tPBT_TRANSITION"));
            NKDbgPrintfW(TEXT("\tSystemPowerState: %s"), pPBM->SystemPowerState);
            switch(POWER_STATE(pPBM->Flags))
            {
                case POWER_STATE_ON:
                    NKDbgPrintfW(TEXT("\tPOWER_STATE_ON"));
                    break;
                case POWER_STATE_OFF:
                    NKDbgPrintfW(TEXT("\tPOWER_STATE_OFF"));
                    break;
                case POWER_STATE_CRITICAL:
                    NKDbgPrintfW(TEXT("\tPOWER_STATE_CRITICAL"));
                    break;
                case POWER_STATE_IDLE:
                    NKDbgPrintfW(TEXT("\tPOWER_STATE_IDLE"));
                    break;
                case POWER_STATE_SUSPEND:
                    NKDbgPrintfW(TEXT("\tPOWER_STATE_SUSPEND"));
                    break;
                default:
                    // no flags specified
                    break;
            }
            break;

        case PBT_RESUME:
            NKDbgPrintfW(TEXT("\tPBT_RESUME"));
            break;
            
        case PBT_POWERSTATUSCHANGE:
            NKDbgPrintfW(TEXT("\tPBT_POWERSTATUSCHANGE"));
            break;  

        case PBT_POWERINFOCHANGE:
            NKDbgPrintfW(TEXT("\tPBT_POWERINFOCHANGE"));            
            NKDbgPrintfW(TEXT("\tPOWER_BROADCAST_INFO:"));
            NKDbgPrintfW(TEXT("\t\tdwNumLevels :                 %d"), pPBM->pbpi.dwNumLevels);
            NKDbgPrintfW(TEXT("\t\tdwBatteryLifeTime :           %d"), pPBM->pbpi.dwBatteryLifeTime);
            NKDbgPrintfW(TEXT("\t\tdwBatteryFullLifeTime :       %d"), pPBM->pbpi.dwBatteryFullLifeTime);
            NKDbgPrintfW(TEXT("\t\tdwBackupBatteryLifeTime :     %d"), pPBM->pbpi.dwBackupBatteryLifeTime );
            NKDbgPrintfW(TEXT("\t\tdwBackupBatteryFullLifeTime : %d"), pPBM->pbpi.dwBackupBatteryFullLifeTime );
            NKDbgPrintfW(TEXT("\t\tbACLineStatus :               %d"), pPBM->pbpi.bACLineStatus);
            NKDbgPrintfW(TEXT("\t\tbBatteryFlag :                %d"), pPBM->pbpi.bBatteryFlag);       
            NKDbgPrintfW(TEXT("\t\tbBackupBatteryFlag :          %d"), pPBM->pbpi.bBackupBatteryFlag);    
            NKDbgPrintfW(TEXT("\t\tbBackupBatteryLifePercent :   %d"), pPBM->pbpi.bBackupBatteryLifePercent);  
            break;

        default:
            NKDbgPrintfW(TEXT("Invalid Power Transition Message"));
            break;
    }
}


BOOL WaitForSystemPowerChange(LPTSTR pszState, DWORD dwMinTime, DWORD dwMaxTime, HANDLE hEvReset)
{
    BOOL fDone = FALSE;
    BOOL fSuccess = FALSE;
    DWORD lastErr = 0;
    DWORD dwStartTick = 0;
    DWORD dwElapsedTime = 0;
    DWORD dwWaitTime = 0;
    DWORD msgBytes = 0;
    DWORD msgFlags = 0;
    HANDLE hMsgQ = NULL;
    HANDLE hNotif = NULL;
    HANDLE hWait[2] = {INVALID_HANDLE_VALUE};

    PPOWER_BCAST_MSG pPBM = new POWER_BCAST_MSG;
    if(pPBM == NULL){
        NKDbgPrintfW(_T("OOM!"));
        return FALSE;
    }
    memset(pPBM, 0, sizeof(POWER_BCAST_MSG));
    
    UINT cResetEvents = 2;
    MSGQUEUEOPTIONS msgOpt = {0};
    
    msgOpt.dwSize = sizeof(MSGQUEUEOPTIONS);
    msgOpt.dwFlags = 0;
    msgOpt.cbMaxMessage = QUEUE_SIZE;
    msgOpt.bReadAccess = TRUE;

    NKDbgPrintfW(TEXT("Waiting %u-%u s for power change notification to state \"%s\""),
        dwMinTime, dwMaxTime, pszState);

    hMsgQ = CreateMsgQueue(NULL, &msgOpt);
    if(NULL == hMsgQ)
    {
        NKDbgPrintfW(_T("CreateMsgQueue() call failed, Error=0x%x"), GetLastError());
        goto Error;
    }

    hNotif = RequestPowerNotifications(hMsgQ, POWER_NOTIFY_ALL);
    if(NULL == hNotif)
    {
        NKDbgPrintfW(_T("RequestPowerNotifications() call failed, Error=0x%x"), GetLastError());
        goto Error;
    }

    hWait[0] = hMsgQ;
    hWait[1] = hEvReset;

    while(!fDone)
    {
        NKDbgPrintfW(TEXT("Waiting %u-%u seconds."), dwMinTime, dwMaxTime);

        // reset our elapsed time counter
        dwStartTick = GetTickCount();
        dwElapsedTime = 0;
        
        // spin here until the reset event becomes un-signalled
        while(WAIT_OBJECT_0 == WaitForSingleObject(hEvReset, 0))
        { ; }

        // determine how much time we have waited so far
        dwElapsedTime = GetTickCount() - dwStartTick;

        while(!fDone)
        {
            // determine how long we should wait. if the ElapsedTime exceeds the MaxTime
            // we need to set the WaitTime to 0 so that WaitForMultipleObjects will perform
            // the check and return immediately. Otherwise, we want to wait the difference 
            // between the MaxTime and the ElapsedTime.
            dwWaitTime = ( (dwElapsedTime >= (dwMaxTime * 1000)) ? 0 : ((dwMaxTime * 1000) - dwElapsedTime) );
            
            // the reset event is un-signalled, wait for it to become signalled
            // again or we receive a power notification message
            lastErr = WaitForMultipleObjects(cResetEvents, hWait, FALSE, dwWaitTime);
            dwElapsedTime = GetTickCount() - dwStartTick;

            if(WAIT_FAILED == lastErr)
            {
                NKDbgPrintfW(_T("WaitForMultipleObjects() failed"));
                goto Error;
            }
            else if(WAIT_OBJECT_0 == lastErr)
            {
                // received a power notification (first event in the wait object list)
                memset(pPBM, 0, sizeof(POWER_BCAST_MSG));
                if(!ReadMsgQueue(hMsgQ, pPBM, sizeof(POWER_BCAST_MSG), &msgBytes, 0, &msgFlags))
                {
                    NKDbgPrintfW(_T("ReadMsgQueue() failed"));
                    goto Error;
                }
                else
                {
                    // verify we received a PBT_TRANSITION notification
                    // all others are ignored
                    NKDbgPrintfW(_T("Received power notification"));
                    LogPowerMessage((PPOWER_BROADCAST)pPBM);
                    if(PBT_TRANSITION == pPBM->Message)
                    {
                        // received a PBT_TRANSITION message-- break out of these loops
                        fDone = TRUE;
                    }
                    else
                    {
                        // wasn't a PBT_TRANSITION message, continue but don't reset
                        // the timer
                        NKDbgPrintfW(_T("Not a transition message.. continuing wait"));
                    }
                }
            }
            else if(WAIT_TIMEOUT == lastErr)
            {
                NKDbgPrintfW(_T("Failed to receive power changed notification within %u ms (%u s)"),
                    dwElapsedTime, ROUND_MS_TO_SEC(dwElapsedTime));
                fDone = TRUE;
                break;
            }
            else if(WAIT_OBJECT_0 < lastErr && WAIT_OBJECT_0+cResetEvents > lastErr)
            {
                NKDbgPrintfW(_T("Activity detected on reset event... resetting wait"));
                break;
            }   
        }
    }

    if(WAIT_TIMEOUT == lastErr)
    {
        NKDbgPrintfW(_T("Timed out waiting for power notification"));
        goto Error;
    }

    NKDbgPrintfW(_T("Power change notification received in %u ms (%u s)"), dwElapsedTime, ROUND_MS_TO_SEC(dwElapsedTime));

    if(ROUND_MS_TO_SEC(dwElapsedTime) < dwMinTime)
    {
        // did not wait long enough
        NKDbgPrintfW(_T("Received power notification prematurely"));
        goto Error;
    }

    if(ROUND_MS_TO_SEC(dwElapsedTime) > dwMaxTime)
    {
        NKDbgPrintfW(_T("Received power notification too late"));
        goto Error;
    }

    if(0 != _tcsicmp(pPBM->SystemPowerState, pszState))
    {
        NKDbgPrintfW(_T("Received incorrect power state change notification to state \"%s\""), 
            pPBM->SystemPowerState);
        goto Error;
    }

    NKDbgPrintfW(_T("Correctly received power state change notification to state \"%s\""), pszState);

    fSuccess = TRUE;

Error:
    if(hNotif)
    {
        lastErr = StopPowerNotifications(hNotif);
        if(ERROR_SUCCESS != lastErr)
        {
            SetLastError(lastErr);
            NKDbgPrintfW(_T("StopPowerNotifications() call failed"));
            goto Error;
        }
    }
    else if(hMsgQ)
    {
        CloseMsgQueue(hMsgQ);
    }

    if(pPBM)
        delete pPBM;
    return fSuccess;
}

BOOL PollForSystemPowerChange(LPTSTR pszState, DWORD dwMinTime, DWORD dwMaxTime)
{
    BOOL fSuccess = FALSE;
    TCHAR szStartState[MAX_PATH] = {0};
    TCHAR szCurState[MAX_PATH] = {0};
    DWORD dwErr = 0;
    DWORD dwCurFlags = 0;
    DWORD tElapsed = 0;
    DWORD tPollTime = 1000;
    DWORD tStart = 0;

    dwErr = GetSystemPowerState(szStartState, MAX_PATH, &dwCurFlags);
    if(ERROR_SUCCESS != dwErr){
        SetLastError(dwErr);
        NKDbgPrintfW(_T("PollForSystemPowerChange::GetSystemPowerState() failed, Error=0x%x"), dwErr);
        return FALSE;
    }

    NKDbgPrintfW(TEXT("Polling for %u-%u s to transition to state \"%s\""),
        dwMinTime, dwMaxTime, pszState);

    tStart = GetTickCount();
    while(!fSuccess){   
        tElapsed = GetTickCount() - tStart;
        
        NKDbgPrintfW(TEXT("Waiting to go to state \"%s\" Elapsed time = %u ms"), 
            pszState, tElapsed);

        if(ROUND_MS_TO_SEC(tElapsed) > dwMaxTime)
        {
            // timed out waiting
            //
            NKDbgPrintfW(TEXT("Failed to transition to state \"%s\" after waiting %u ms (%u s)"),
                pszState, tElapsed, ROUND_MS_TO_SEC(tElapsed));
            break;
        }

        dwErr = GetSystemPowerState(szCurState, MAX_PATH, &dwCurFlags);
        if(ERROR_SUCCESS != dwErr)
        {
            SetLastError(dwErr);
            NKDbgPrintfW(_T("GetSystemPowerState() call failed, error=0x%x"), dwErr);
            break;
        }
        else{
            NKDbgPrintfW(TEXT("Current state is: %s"), szCurState);
        }
        if(0 == _tcsicmp(pszState, szCurState))
        {
            // we are in the desired power state, is the timing correct?
            //
            if(ROUND_MS_TO_SEC(tElapsed) < dwMinTime)
            {
                NKDbgPrintfW(TEXT("Transitioned to state \"%s\" before the timeout was reached"),
                    pszState);
                break;
            }
            else
            {
                NKDbgPrintfW(TEXT("Transitioned to state \"%s\" in %u ms (%u s)"),
                    pszState, tElapsed, ROUND_MS_TO_SEC(tElapsed));
                fSuccess = TRUE;
                break;
            }
        }
        else if(0 != _tcsicmp(szStartState, szCurState))
        {
            // we are in some state other than the desired one and the
            // one we started in
            //
            NKDbgPrintfW(TEXT("Incorrectly transitioned to state \"%s\" instead of \"%s\""),
                szCurState, pszState);
            break;
        }
        
        Sleep(tPollTime);
    }

    return fSuccess;
}


