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
#pragma once

#include <windows.h>
#include <pm.h>
#include <nkintr.h>
#include <pkfuncs.h>
#include <devload.h>
#include <msgqueue.h>
#include <CSync.h>
#include <notify.h>
#include "kModeTUD.h"

#define PWRMGR_STATE_KEY                    PWRMGR_REG_KEY _T("\\State")
#define PWRMGR_TIMEOUTS_KEY              PWRMGR_REG_KEY _T("\\Timeouts")
#define PWRMGR_ACTIVITY_KEY               PWRMGR_REG_KEY _T("\\ActivityTimers")

#define USER_ACTIVITY  				    _T("UserActivity")
#define USER_ACTIVITY_RESET_EVT          _T("PowerManager/ActivityTimer/") USER_ACTIVITY
#define USER_ACTIVITY_ACTIVE_EVT        _T("PowerManager/") USER_ACTIVITY _T("_Active")
#define USER_ACTIVITY_INACTIVE_EVT     _T("PowerManager/") USER_ACTIVITY _T("_Inactive")

#define SYSTEM_ACTIVITY 			    _T("SystemActivity")
#define SYSTEM_ACTIVITY_RESET_EVT      _T("PowerManager/ActivityTimer/") SYSTEM_ACTIVITY
#define SYSTEM_ACTIVITY_ACTIVE_EVT    _T("PowerManager/") SYSTEM_ACTIVITY _T("_Active")
#define SYSTEM_ACTIVITY_INACTIVE_EVT _T("PowerManager/") SYSTEM_ACTIVITY _T("_Inactive")

#define ACTIVITY_TIMER_RELOAD_EVT      _T("PowerManager/ReloadActivityTimeouts")

#define ROUND_MS_TO_SEC(T)              ((T+500)/1000)

#define PWR_NOTIFY_WAIT_TIME    1000
#define QUEUE_MAX_NAMELEN       200 

#define QUEUE_ENTRIES           3
#define QUEUE_MAX_MSG_SIZE      (sizeof(POWER_BCAST_MSG))
#define QUEUE_SIZE              (QUEUE_ENTRIES*QUEUE_MAX_MSG_SIZE)

__inline BOOL InKernelMode( void ) {

    return 0 > (int)InKernelMode;

}

// Our own version of POWER_BROADCAST structure with the SystemPowerState
// string defined
//

typedef struct _POWER_BCAST_MSG
{
    DWORD Message;
    DWORD Flags;
    DWORD Length;
    union 
    {
        WCHAR SystemPowerState[QUEUE_MAX_NAMELEN];
        POWER_BROADCAST_POWER_INFO pbpi;
    };
        
} POWER_BCAST_MSG, *PPOWER_BCAST_MSG;

VOID LogPowerMessage(PPOWER_BROADCAST pPBM);
BOOL SimulateUserEvent(VOID);
BOOL SimulateSystemEvent(VOID);
BOOL SimulateReloadTimeoutsEvent(VOID);

BOOL IsCurrentPowerState(LPTSTR pszState);
BOOL WaitForSystemPowerChange(LPTSTR pszState, DWORD dwWaitTime, DWORD dwDeltaTime, HANDLE hEvReset);
BOOL PollForSystemPowerChange(LPTSTR pszState, DWORD dwWaitTime, DWORD dwDeltaTime);



class CSysPwrStates : public CLockObject{

private:

    BOOL m_fCanRTCEnabled;
    BOOL m_fUsingRTCNow;

    HANDLE m_hRTCNotify;

    //deal with usermode situation
    BOOL m_fKMode;
    HANDLE m_hTUDDrv;
    HANDLE m_hTUDFile;
    
    CE_NOTIFICATION_TRIGGER m_trigger;

    VOID VerifyWakeupResources();
    VOID CleanupAllWakeupResources();

public:

    CSysPwrStates(){
        Lock();
        m_hTUDDrv = m_hTUDFile = NULL;
        m_fKMode = InKernelMode();
        if(m_fKMode == FALSE){
            if((m_hTUDFile = OpenTestUtilDriver()) == INVALID_HANDLE_VALUE){
                if((m_hTUDDrv = LoadTestUtilDriver()) != INVALID_HANDLE_VALUE){
                    m_hTUDFile = OpenTestUtilDriver();
                }
            }
        }
        VerifyWakeupResources();
        m_fUsingRTCNow = FALSE;
        m_hRTCNotify = NULL;
        Unlock();
    }

    ~CSysPwrStates(){
        Lock();
        CleanupAllWakeupResources();
        if(m_hTUDFile != NULL)
            CloseHandle(m_hTUDFile);
        if(m_hTUDDrv != NULL)
            UnloadTestUtilDriver(m_hTUDDrv);
        Unlock();
    }

    BOOL SetupRTCWakeupTimer(WORD wSeconds);
    VOID CleanupRTCTimer();

};
