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
#ifndef __POWERMANAGER_HPP_
#define __POWERMANAGER_HPP_

#pragma once

#include <windows.h>
#include <tchar.h>
#include <Pm.h>
#include <ntddndis.h>
#include <strsafe.h>
#include <ndispwr.h>
#include <Winbase.h>
#include <Pm.h>
#include <assert.h>
#include "netmain.h"
#include "cmnprint.h"
#include "PowerLog.hpp"

namespace ce {
namespace qa {



static const DWORD POWER_STATE_DEFAULT =0x00000001;       // only used by automation test. revert to default (test start)
static const DWORD POWER_STATE_CURRENT = 0x00000002;        // only used by automation test. do not change pm state
static const DWORD POWER_STATE_AUTOSUSPEND = 0x00001000;        // enable autosuspend for this test?   

struct SysPowerState
{
private:
    DWORD dwState;
    WCHAR szSysPowerState[255]; //System's current state.
    SYSTEM_POWER_STATUS_EX2 sSysPower; //System's adavanced power state

    friend class PowerManager;  
public:

};



// Get power state and save power states
class PowerManager //: public PowerMeasurement
{
private:
    
    // default states (as determined by saving current states at startup)
    BOOL       bDefaultSet;
    SysPowerState defaultState;    
    SysPowerState currentState;

    PowerManager() : bDefaultSet(FALSE)
    {     
    }

    void Init()
    {
           if (ERROR_SUCCESS == GetSysState(defaultState))
            bDefaultSet = TRUE;        
     }
    
   static PowerManager* m_PMInstance;
public :

    static PowerManager* GetInstance()
    {
        if (!m_PMInstance)
        {
            m_PMInstance = new PowerManager();
            m_PMInstance->Init();            
        }
        assert(m_PMInstance);
        return m_PMInstance;
    }

    static void DeleteInstance()
    {
        if (m_PMInstance)
            delete m_PMInstance;
    }
    
    void LogSysState()
    {
        PowerLog* pLog  = PowerLog::GetInstance();
            
        pLog->LogTime();
        pLog->Log(TEXT("System Power State:- \n"));

        GetSysState(currentState);
   
        pLog->Log(TEXT("System PM state=%s [%d] \n"),currentState.szSysPowerState,currentState.dwState);
        pLog->Log(TEXT("Battery Life percent = %d %% \n"),currentState.sSysPower.BatteryLifePercent);
        pLog->Log(TEXT("Battery Life left = %d sec \n"),currentState.sSysPower.BatteryLifeTime);
        pLog->Log(TEXT("Battery Total Life = %d sec \n"),currentState.sSysPower.BatteryFullLifeTime);
        
        pLog->Log(TEXT("Battery AHour consumed = %d \n"),currentState.sSysPower.BatterymAHourConsumed);
        pLog->Log(TEXT("Battery Current Drain = %d mA \n"),currentState.sSysPower.BatteryCurrent);
        pLog->Log(TEXT("Battery Average Current Drain = %d mA \n"),currentState.sSysPower.BatteryAverageCurrent);              

        pLog->Flush();
    }
    
    //Calls GetSystemPowerState
    DWORD GetSysState(SysPowerState& ps)
    {
        DWORD dwResult = GetSystemPowerState(ps.szSysPowerState, sizeof(ps.szSysPowerState),&ps.dwState);
        if (dwResult != ERROR_SUCCESS)
        {
            CmnPrint(PT_FAIL, TEXT("Failed to get System Power State: Err = %d"), dwResult);
            StringCbCopy(ps.szSysPowerState, sizeof(ps.szSysPowerState) , TEXT("Unspecified"));
            return dwResult;
        }
        CmnPrint(PT_LOG, TEXT("Current System Power State: %s"), ps.szSysPowerState);

        if (0 == GetSystemPowerStatusEx2(&ps.sSysPower, sizeof(ps.sSysPower),TRUE))
        {
            CmnPrint(PT_FAIL, TEXT("Failed to get System Power State: Err = %d"), dwResult);
            ps.sSysPower.ACLineStatus = AC_LINE_UNKNOWN;
            ps.sSysPower.BatteryFlag = BATTERY_FLAG_UNKNOWN;  
        }
        else
        {
            if (ps.sSysPower.BatteryLifePercent > 100)
            {
                CmnPrint(PT_WARN1, TEXT("Battery life is shown more than 100%"));
                ps.sSysPower.BatteryLifePercent = 100;
            }
        }
        return dwResult;
    }
    
    // Calls SetSystemPowerState
    DWORD SetSysState(DWORD dwState)
    {
        DWORD dwResult;
        
        if (dwState == POWER_STATE_CURRENT) 
            return ERROR_SUCCESS;

        if (dwState & POWER_STATE_DEFAULT)
        {      
            if (!bDefaultSet)
                return ERROR_INVALID_STATE;
            dwState = defaultState.dwState | (0x0000FFFF & dwState);
        }

        dwResult = GetSysState(currentState);
        if (dwResult != ERROR_SUCCESS)
            return dwResult;
        

        // change state only if different than current state
        if (!(currentState.dwState & POWER_STATE(dwState)))
        {
            CmnPrint(PT_LOG, TEXT("Power Manager"));
            dwResult = SetSystemPowerState(NULL, POWER_STATE(dwState), POWER_FORCE);
            if (dwResult != ERROR_SUCCESS)
                return dwResult;
        }

        // TODO:
        /* handle auto-suspend       
        
        if (dwState & POWER_STATE_AUTOSUSPEND)
        {
            EnableAutoSuspend();
        }
        else
        {
            DisableAutoSuspend();
        }*/

        return dwResult;
    }
};

};
};

#endif
