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
////////////////////////////////////////////////////////////////////////////////
//
//  TUX DLL
//  Copyright (c) Microsoft Corporation
//
//  Module:
//          Contains the test functions.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"
#include <pm.h>

////////////////////////////////////////////////////////////////////////////////
/***************  Constants and Defines Local to This File  *******************/
DWORD g_dwUserIdleTimeout;
DWORD g_dwBacklightTimeout;

// Max seconds allowed above the timeout value for a system power change to occur.
// This value should be the basic activity timer timeout (10 seconds)
#define TRANSITION_TOLERANCE   10

typedef struct _SYS_TIMEOUTS{
    DWORD  dwACUserIdleTimeout;
    DWORD  dwACSystemIdleTimeout;
    DWORD  dwACSuspendTimeout;
    DWORD  dwUserActivityEventTimeout;
    DWORD  dwSystemActivityEventTimeout;
}SYS_TIMEOUTS, *PSYS_TIMEOUTS;

SYS_TIMEOUTS    g_sysTimeouts;


////////////////////////////////////////////////////////////////////////////////
/********************  Helper Functions  ************************/

BOOL VerifyPowerState(LPCWSTR pszPwrStateName);
BOOL RetrieveTimeouts_PDA(void);
BOOL SetUpTimeouts_PDA(DWORD dwDisplay, DWORD dwBacklight);
BOOL RetrieveTimeouts_DEFAULT(void);
BOOL RetrieveActivityEventTimeouts_DEFAULT(void);
BOOL SetUpTimeouts_DEFAULT(DWORD dwACUserIdle, DWORD dwACSystemIdle, DWORD dwACSuspend);


////////////////////////////////////////////////////////////////////////////////
/********************  Test Procs  ************************/

////////////////////////////////////////////////////////////////////////////////
// Usage Message
//  This test prints out the usage message for this test suite. 
//
// Parameters:
//  uMsg           Message code.
//  tpParam      Additional message-dependent data.
//  lpFTE          Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI UsageMessage(UINT uMsg, 
                         TPPARAM,/* tpParam*/
                         LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test. 
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    PWRMSG(LOG_DETAIL, _T("Use this test suite to verify transitions between different power states."));
    PWRMSG(LOG_DETAIL, _T("")); //Blank line
    PWRMSG(LOG_DETAIL, _T("The tests take the following optional command line arguments:"));
    PWRMSG(LOG_DETAIL, _T("1. AllowSuspend - specify this option(-c\"AllowSuspend\") to allow the tests to suspend the system."));
    PWRMSG(LOG_DETAIL, _T("2. PdaVersion - specify this option(-c\"PdaVersion\") to let the tests know this is a PDA type device."));

    return TPR_PASS;
}


/*
PDA Power Model State Transitions

*Remain in On state*
Remain in On state - User activity
Remain in On state - On event
Remain in On state - Power API

*Transitions for BacklightOff state*
On to BacklightOff - Backlight timeout
BacklightOff to On - User activity
On to BacklightOff - Backlight timeout
BacklightOff to On - On event
On to BacklightOff - Power API - should fail
On to BacklightOff - Backlight timeout
BacklightOff to On - Power API

*Transitions for UserIdle state*
On to UserIdle - Idle timeout
UserIdle to On - User activity
On to UserIdle - Idle timeout
UserIdle to On - On event
On to UserIdle - Power API
UserIdle to UserIdle - Power API
UserIdle to On - Power API
On to BacklightOff to UserIdle - Backlight timeout\Idle timeout
UserIdle to On - User activity

*Transitions for ScreenOff state*
On to ScreenOff - Power API
ScreenOff to ScreenOff - Power API
Remain in ScreenOff state - Backlight timeout
Remain in ScreenOff state - UserIdle timeout
Remain in ScreenOff state - User activity
ScreenOff to On - On event
On to ScreenOff to UserIdle - Power API
UserIdle to ScreenOff to On - Power API
*/


/*
PDA Power Model State Transitions

*Remain in On state*
Remain in On state - User activity
Remain in On state - On event
Remain in On state - Power API
*/
TESTPROCAPI Tst_TransitionOn_PDA(UINT uMsg,
                                 TPPARAM /*tpParam*/,
                                 LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test. 
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    SET_PWR_TESTNAME(_T("Tst_TransitionOn_PDA"));
    DWORD dwErr = 0;

    // Check if PDA tests are enabled
    if(g_bPdaVersion == FALSE)
    {
        PWRMSG(LOG_SKIP,_T("This test will skip as it targets the PDA power model and that command line option was not set."));
        PWRMSG(LOG_DETAIL,_T("Specify this option (-c\"PdaVersion\") to allow this test to execute."));
        return GetTestResult();
    }   

    // Continue with the test
    PWRMSG(LOG_DETAIL, _T("This test will verify remaining in the On state when different events occur."));

    PWRMSG(LOG_DETAIL,_T(""));

    // Set system power state to On
    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState(On, 0, POWER_FORCE)"));
    if(ERROR_SUCCESS !=  SetSystemPowerState(_T("On"), 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now check if the Power State was indeed set
    if(!VerifyPowerState(_T("On")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------Remain in On state - User activity ------------------*/

    // Simulate user activity and verify we remain in On state
    PWRMSG(LOG_DETAIL,_T("Simulating user activity."));
    if(!SimulateUserEvent())
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SimulateUserEvent() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now check if the Power State is still On
    if(!VerifyPowerState(_T("On")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------Remain in On state - On event ------------------*/

    // Trigger an On event and verify we remain in On state
    PWRMSG(LOG_DETAIL,_T("Trigger On event."));
    if(!PowerPolicyNotify(PPN_APPBUTTONPRESSED, 0))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("PowerPolicyNotify(PPN_APPBUTTONPRESSED, 0) failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now Get Power State to check if the Power State is still On
    if(!VerifyPowerState(_T("On")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));

    /*-------------------Remain in On state - Power API ------------------*/

    // Call Power API to set to On state and verify we remain in On state
    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState(On, 0, POWER_FORCE)"));
    if(ERROR_SUCCESS !=  SetSystemPowerState(_T("On"), 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now check if the Power State is still On
    if(!VerifyPowerState(_T("On")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    return GetTestResult();
}


/*
PDA Power Model State Transitions

*Transitions for BacklightOff state*
On to BacklightOff - Backlight timeout
BacklightOff to On - User activity
On to BacklightOff - Backlight timeout
BacklightOff to On - On event
On to BacklightOff - Power API - should fail
On to BacklightOff - Backlight timeout
BacklightOff to On - Power API
*/
TESTPROCAPI Tst_TransitionBacklightOff_PDA(UINT uMsg,
                                           TPPARAM /*tpParam*/,
                                           LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test. 
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    SET_PWR_TESTNAME(_T("Tst_TransitionBacklightOff_PDA"));
    DWORD dwErr = 0;

    // Check if PDA tests are enabled
    if(g_bPdaVersion == FALSE)
    {
        PWRMSG(LOG_SKIP,_T("This test will skip as it targets the PDA power model and that command line option was not set."));
        PWRMSG(LOG_DETAIL,_T("Specify this option (-c\"PdaVersion\") to allow this test to execute."));
        return GetTestResult();
    }   

    // Continue with the test
    PWRMSG(LOG_DETAIL, _T("This test will verify transitions to and from the BacklightOff state."));

    PWRMSG(LOG_DETAIL,_T(""));
    // Set system power state to On
    PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(On, 0, POWER_FORCE)"));
    if(ERROR_SUCCESS !=  SetSystemPowerState(_T("On"), 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now Get Power State to check if the Power State was indeed set
    if(!VerifyPowerState(_T("On")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------On to BacklightOff - Backlight timeout ------------------*/

    // Set Backlight timeout value so its less than useridle timeout. We want to transition to BacklightOff.

    // First retrieve original timeouts so we can set them back
    if(!RetrieveTimeouts_PDA())
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Store them
    DWORD dwOrigBacklightTimeout = g_dwBacklightTimeout;
    DWORD dwOrigUserIdleTimeout = g_dwUserIdleTimeout;

    // Now setup the timeouts that we want for the test
    if(!SetUpTimeouts_PDA(5, 60))
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Retrieve timeouts
    if(!RetrieveTimeouts_PDA())
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Verify we got what we set
    if((g_dwBacklightTimeout != 5) || (g_dwUserIdleTimeout != 60))
    {
        PWRMSG(LOG_DETAIL, _T("Failed to setup timeouts."));
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }
    else
    {
        PWRMSG(LOG_DETAIL, _T("Setup timeouts done. Backlight timeout = %d, UserIdle timeout = %d."), g_dwBacklightTimeout, g_dwUserIdleTimeout);
    }

    PWRMSG(LOG_DETAIL,_T(""));

    // Sleep so that backlight timeout expires
    PWRMSG(LOG_DETAIL, _T("Now we will sleep until Backlight timeout expires so we transition to BacklightOff state."));
    Sleep((g_dwBacklightTimeout+3) * 1000);

    // Verify if we transitioned to the BacklightOff
    if(!VerifyPowerState(_T("BacklightOff")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------BacklightOff to On - User activity ------------------*/

    // Get back to On due to user activity
    PWRMSG(LOG_DETAIL, _T("Simulate user activity so we transition back to the On state."));
    if(!SimulateUserEvent())
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SimulateUserEvent() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we transitioned to On
    if(!VerifyPowerState(_T("On")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------On to BacklightOff - Backlight timeout ------------------*/

    // Sleep so that backlight timeout expires
    PWRMSG(LOG_DETAIL, _T("Now we will sleep until Backlight timeout expires so we transition to BacklightOff state."));
    Sleep((g_dwBacklightTimeout+3) * 1000);

    // Verify if we transitioned to the BacklightOff
    if(!VerifyPowerState(_T("BacklightOff")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------BacklightOff to On - On event ------------------*/

    // Get back to On due to On event
    PWRMSG(LOG_DETAIL, _T("Simulate an On event so we transition back to the On state."));
    // Trigger On event
    if(!PowerPolicyNotify(PPN_APPBUTTONPRESSED, 0))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("PowerPolicyNotify(PPN_APPBUTTONPRESSED, 0) failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we transitioned to On
    if(!VerifyPowerState(_T("On")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));

    /*-------------------On to BacklightOff - Power API - should fail ------------------*/

    // Set system to BacklightOff using power API, should fail
    PWRMSG(LOG_DETAIL,_T("Now we will call SetSystemPowerState(BacklightOff, 0, POWER_FORCE) and verify the call fails."));
    if(ERROR_SUCCESS ==  SetSystemPowerState(_T("BacklightOff"), 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we are still in the On state
    if(!VerifyPowerState(_T("On")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------On to BacklightOff - Backlight timeout ------------------*/

    // Sleep so that backlight timeout expires
    PWRMSG(LOG_DETAIL, _T("Now we will sleep until Backlight timeout expires so we transition to BacklightOff state."));
    Sleep((g_dwBacklightTimeout+3) * 1000);

    // Verify if we transitioned to the BacklightOff
    if(!VerifyPowerState(_T("BacklightOff")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------BacklightOff to On - Power API ------------------*/

    // Set system to On using power API
    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState to transition back to On state."));
    if(ERROR_SUCCESS !=  SetSystemPowerState(_T("On"), 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState(On, 0, POWER_FORCE) failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we transitioned to On
    if(!VerifyPowerState(_T("On")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Now set back the original timeouts
    if(!SetUpTimeouts_PDA(dwOrigBacklightTimeout, dwOrigUserIdleTimeout))
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    return GetTestResult();
}



/*
PDA Power Model State Transitions

*Transitions for UserIdle state*
On to UserIdle - Idle timeout
UserIdle to On - User activity
On to UserIdle - Idle timeout
UserIdle to On - On event
On to UserIdle - Power API
UserIdle to UserIdle - Power API
UserIdle to On - Power API
On to BacklightOff to UserIdle - Backlight timeout\Idle timeout
UserIdle to On - User activity
*/
TESTPROCAPI Tst_TransitionUserIdle_PDA(UINT uMsg,
                                       TPPARAM /*tpParam*/,
                                       LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test. 
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    SET_PWR_TESTNAME(_T("Tst_TransitionUserIdle_PDA"));
    DWORD dwErr = 0;

    // Check if PDA tests are enabled
    if(g_bPdaVersion == FALSE)
    {
        PWRMSG(LOG_SKIP,_T("This test will skip as it targets the PDA power model and that command line option was not set."));
        PWRMSG(LOG_DETAIL,_T("Specify this option (-c\"PdaVersion\") to allow this test to execute."));
        return GetTestResult();
    }   

    // Continue with the test
    PWRMSG(LOG_DETAIL, _T("This test will verify transitions to and from the UserIdle state."));

    PWRMSG(LOG_DETAIL,_T(""));

    // Set system power state to On
    PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(On, 0, POWER_FORCE)"));
    if(ERROR_SUCCESS !=  SetSystemPowerState(_T("On"), 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now Get Power State to check if the Power State was indeed set
    if(!VerifyPowerState(_T("On")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }
    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------On to UserIdle - Idle timeout------------------*/

    // Set UserIdle timeout value so its less than the backlight timeout. We want to transition to UserIdle.

    // First retrieve original timeouts so we can set them back
    if(!RetrieveTimeouts_PDA())
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }
    // Store them
    DWORD dwOrigBacklightTimeout = g_dwBacklightTimeout;
    DWORD dwOrigUserIdleTimeout = g_dwUserIdleTimeout;

    // Now setup the timeouts that we want for the test
    if(!SetUpTimeouts_PDA(60, 5))
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Retrieve timeouts
    if(!RetrieveTimeouts_PDA())
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Verify we got what we set
    if((g_dwBacklightTimeout != 60) || (g_dwUserIdleTimeout != 5))
    {
        PWRMSG(LOG_DETAIL, _T("Failed to setup timeouts."));
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }
    else
    {
        PWRMSG(LOG_DETAIL, _T("Setup timeouts done. Backlight timeout = %d, UserIdle timeout = %d."), g_dwBacklightTimeout, g_dwUserIdleTimeout);
    }

    PWRMSG(LOG_DETAIL,_T(""));

    // Sleep so that UserIdle timeout expires
    PWRMSG(LOG_DETAIL, _T("Now we will sleep until UserIdle timeout expires so we transition to UserIdle state."));
    Sleep((g_dwUserIdleTimeout+3) * 1000);

    // Verify if we transitioned to UserIdle
    if(!VerifyPowerState(_T("UserIdle")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------UserIdle to On - User activity------------------*/

    // Get back to On due to user activity
    PWRMSG(LOG_DETAIL, _T("Simulate user activity so we transition back to the On state."));
    if(!SimulateUserEvent())
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SimulateUserEvent() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we transitioned to On
    if(!VerifyPowerState(_T("On")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------On to UserIdle - Idle timeout------------------*/

    // Sleep so that UserIdle timeout expires
    PWRMSG(LOG_DETAIL, _T("Now we will sleep until UserIdle timeout expires so we transition to UserIdle state."));
    Sleep((g_dwUserIdleTimeout+3) * 1000);

    // Verify if we transitioned to UserIdle
    if(!VerifyPowerState(_T("UserIdle")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------UserIdle to On - On event------------------*/

    // Get back to On due to On event
    PWRMSG(LOG_DETAIL, _T("Simulate an On event so we transition back to the On state."));

    // Trigger On event
    if(!PowerPolicyNotify(PPN_APPBUTTONPRESSED, 0))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("PowerPolicyNotify(PPN_APPBUTTONPRESSED, 0) failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we transitioned to On
    if(!VerifyPowerState(_T("On")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------On to UserIdle - Power API------------------*/

    // Set system to UserIdle using power API
    PWRMSG(LOG_DETAIL,_T("Now we will call SetSystemPowerState(UserIdle, 0, POWER_FORCE) so we transition to UserIdle state."));
    if(ERROR_SUCCESS !=  SetSystemPowerState(_T("UserIdle"), 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we transitioned to UserIdle
    if(!VerifyPowerState(_T("UserIdle")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------UserIdle to UserIdle - Power API------------------*/

    // Set system to UserIdle using power API
    PWRMSG(LOG_DETAIL,_T("Now we will call SetSystemPowerState(UserIdle, 0, POWER_FORCE) again and verify we are still in UserIdle state."));
    if(ERROR_SUCCESS !=  SetSystemPowerState(_T("UserIdle"), 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we are still in UserIdle
    if(!VerifyPowerState(_T("UserIdle")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------UserIdle to On - Power API------------------*/

    // Set system to On using power API
    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState to transition back to On state."));
    if(ERROR_SUCCESS !=  SetSystemPowerState(_T("On"), 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState(On, 0, POWER_FORCE) failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we transitioned to On
    if(!VerifyPowerState(_T("On")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*----On to BacklightOff to UserIdle - Backlight timeout\Idle timeout---*/

    // Set Backlight timeout value so its less than useridle timeout. 
    // We want to transition to BacklightOff and then to UserIdle.
    if(!SetUpTimeouts_PDA(5, 20))
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Retrieve timeouts
    if(!RetrieveTimeouts_PDA())
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Verify we got what we set
    if((g_dwBacklightTimeout != 5) || (g_dwUserIdleTimeout != 20))
    {
        PWRMSG(LOG_DETAIL, _T("Failed to setup timeouts."));
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }
    else
    {
        PWRMSG(LOG_DETAIL, _T("Setup timeouts done. Backlight timeout = %d, UserIdle timeout = %d."), g_dwBacklightTimeout, g_dwUserIdleTimeout);
    }

    PWRMSG(LOG_DETAIL,_T(""));

    // Sleep so that backlight timeout expires
    PWRMSG(LOG_DETAIL, _T("Now we will sleep until Backlight timeout expires so we transition to BacklightOff state."));
    Sleep((g_dwBacklightTimeout+3) * 1000);

    // Verify if we transitioned to BacklightOff
    if(!VerifyPowerState(_T("BacklightOff")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));

    // Sleep so that UserIdle timeout expires
    PWRMSG(LOG_DETAIL, _T("Now we will sleep until UserIdle timeout expires so we transition to UserIdle state."));
    // Sleep for the delta time that it takes for UserIdle. (we already slept for g_dwBacklightTimeout+5 for Backlight timeout.)
    Sleep((g_dwUserIdleTimeout-(g_dwBacklightTimeout+3)+3) * 1000);

    // Verify if we transitioned to UserIdle
    if(!VerifyPowerState(_T("UserIdle")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------UserIdle to On - User activity------------------*/

    // Get back to On due to user activity
    PWRMSG(LOG_DETAIL, _T("Simulate user activity so we transition back to the On state."));
    if(!SimulateUserEvent())
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SimulateUserEvent() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we transitioned to On
    if(!VerifyPowerState(_T("On")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Now set back the original timeouts
    if(!SetUpTimeouts_PDA(dwOrigBacklightTimeout, dwOrigUserIdleTimeout))
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    return GetTestResult();
}


/*
PDA Power Model State Transitions

*Transitions for ScreenOff state*
On to ScreenOff - Power API
ScreenOff to ScreenOff - Power API
Remain in ScreenOff state - Backlight timeout\UserIdle timeout
Remain in ScreenOff state - User activity
ScreenOff to On - On event
On to ScreenOff to UserIdle - Power API
UserIdle to ScreenOff to On - Power API
*/
TESTPROCAPI Tst_TransitionScreenOff_PDA(UINT uMsg,
                                        TPPARAM /*tpParam*/,
                                        LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test. 
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    SET_PWR_TESTNAME(_T("Tst_TransitionScreenOff_PDA"));
    DWORD dwErr = 0;

    // Check if PDA tests are enabled
    if(g_bPdaVersion == FALSE)
    {
        PWRMSG(LOG_SKIP,_T("This test will skip as it targets the PDA power model and that command line option was not set."));
        PWRMSG(LOG_DETAIL,_T("Specify this option (-c\"PdaVersion\") to allow this test to execute."));
        return GetTestResult();
    }   

    // Continue with the test
    PWRMSG(LOG_DETAIL, _T("This test will transitions to and from the ScreenOff state."));

    PWRMSG(LOG_DETAIL,_T(""));

    // First Set system power state to On
    PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(On, 0, POWER_FORCE)"));
    if(ERROR_SUCCESS !=  SetSystemPowerState(_T("On"), 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now check if the Power State was indeed set
    if(!VerifyPowerState(_T("On")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------On to ScreenOff - Power API------------------*/

    // Set power state to ScreenOff
    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState(ScreenOff, 0, POWER_FORCE)"));
    if(ERROR_SUCCESS !=  SetSystemPowerState(_T("ScreenOff"), 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now check if the Power State was indeed set
    if(!VerifyPowerState(_T("ScreenOff")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------ScreenOff to ScreenOff - Power API---------------*/

    // Set power state to ScreenOff again
    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState(ScreenOff, 0, POWER_FORCE)"));
    if(ERROR_SUCCESS !=  SetSystemPowerState(_T("ScreenOff"), 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now check if we remained in ScreenOff
    if(!VerifyPowerState(_T("ScreenOff")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*---------Remain in ScreenOff state - Backlight timeout\UserIdle timeout----------*/

    // First retrieve original timeouts so we can set them back
    if(!RetrieveTimeouts_PDA())
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }
    // Store them
    DWORD dwOrigBacklightTimeout = g_dwBacklightTimeout;
    DWORD dwOrigUserIdleTimeout = g_dwUserIdleTimeout;

    // Now setup the timeouts that we want for the test
    if(!SetUpTimeouts_PDA(5, 20))
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Retrieve timeouts
    if(!RetrieveTimeouts_PDA())
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Verify we got what we set
    if((g_dwBacklightTimeout != 5) || (g_dwUserIdleTimeout != 20))
    {
        PWRMSG(LOG_DETAIL, _T("Failed to setup timeouts."));
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }
    else
        PWRMSG(LOG_DETAIL, _T("Setup timeouts done. Backlight timeout = %d, UserIdle timeout = %d."), g_dwBacklightTimeout, g_dwUserIdleTimeout);


    PWRMSG(LOG_DETAIL,_T(""));

    // Sleep so that backlight timeout expires
    PWRMSG(LOG_DETAIL, _T("Now we will sleep until Backlight timeout expires and verify we still remain in ScreenOff state."));
    Sleep((g_dwBacklightTimeout+3) * 1000);

    // Verify if we remained in ScreenOff state
    if(!VerifyPowerState(_T("ScreenOff")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));

    // Sleep so that UserIdle timeout expires
    PWRMSG(LOG_DETAIL, _T("Now we will sleep until UserIdle timeout expires and verify we remain in ScreenOff state."));
    // Sleep for the delta time that it takes for UserIdle. (we already slept for g_dwBacklightTimeout+3 for Backlight timeout.)
    Sleep((g_dwUserIdleTimeout-(g_dwBacklightTimeout+3)+3) * 1000);

    // Verify if we remained in ScreenOff state
    if(!VerifyPowerState(_T("ScreenOff")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------Remain in ScreenOff state - User activity------------*/

    // Simulate user activity and verify we are still in On state
    PWRMSG(LOG_DETAIL, _T("Now we will simulate user activity and verify we remain in ScreenOff state."));

    PWRMSG(LOG_DETAIL,_T("Simulating user activity."));
    if(!SimulateUserEvent())
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SimulateUserEvent() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now Get Power State to check if the Power State is still ScreenOff
    if(!VerifyPowerState(_T("ScreenOff")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------ScreenOff to On - On event------------------*/

    // ScreenOff - On event
    PWRMSG(LOG_DETAIL, _T("Now we will trigger an On event and verify that we transition to the On state."));
    PWRMSG(LOG_DETAIL,_T("Trigger On event."));
    if(!PowerPolicyNotify(PPN_APPBUTTONPRESSED, 0))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("PowerPolicyNotify(PPN_APPBUTTONPRESSED, 0) failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now Get Power State to check if the Power State is On
    if(!VerifyPowerState(_T("On")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*---------------On to ScreenOff to UserIdle - Power API--------------*/

    // Set system to ScreenOff using power API
    PWRMSG(LOG_DETAIL, _T("Now we will set power state to ScreenOff state using the SetSystemPowerState API."));
    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState(ScreenOff, 0, POWER_FORCE)"));
    if(ERROR_SUCCESS !=  SetSystemPowerState(_T("ScreenOff"), 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now Get Power State to check if the Power State was indeed set
    if(!VerifyPowerState(_T("ScreenOff")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));

    // Set system to UserIdle using power API
    PWRMSG(LOG_DETAIL, _T("Now we will set power state to UserIdle state using the SetSystemPowerState API."));
    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState(UserIdle, 0, POWER_FORCE)"));
    if(ERROR_SUCCESS !=  SetSystemPowerState(_T("UserIdle"), 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now Get Power State to check if the Power State was indeed set
    if(!VerifyPowerState(_T("UserIdle")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-----------------UserIdle to ScreenOff to On - Power API--------------*/

    // Set system to ScreenOff using power API
    PWRMSG(LOG_DETAIL, _T("Now we will set power state to ScreenOff state using the SetSystemPowerState API."));
    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState(ScreenOff, 0, POWER_FORCE)"));
    if(ERROR_SUCCESS !=  SetSystemPowerState(_T("ScreenOff"), 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now Get Power State to check if the Power State was indeed set
    if(!VerifyPowerState(_T("ScreenOff")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));

    // Set system to On using power API
    PWRMSG(LOG_DETAIL, _T("Now we will set power state to On state using SetSystemPowerState API and verify we transition to On."));
    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState(On, 0, POWER_FORCE)"));
    if(ERROR_SUCCESS !=  SetSystemPowerState(_T("On"), 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now Get Power State to check if the Power State was indeed set
    if(!VerifyPowerState(_T("On")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Now set back the original timeouts
    if(!SetUpTimeouts_PDA(dwOrigBacklightTimeout, dwOrigUserIdleTimeout))
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    return GetTestResult();
}



/*
DEFAULT Power Model State Transitions

*Remain in On state*
Remain in On state - User activity
Remain in On state - System activity
Remain in On state - On event
Remain in On state - Power API

*Transitions for UserIdle state*
On to UserIdle - UserIdle timeout
UserIdle to On - User activity
On to UserIdle - Power API
UserIdle to UserIdle - Power API
UserIdle to On - Power API


*Transitions for SystemIdle state*
On to SystemIdle - SystemIdle timeout
SystemIdle to On - System activity - should fail
SystemIdle to On - User activity
On to SystemIdle - Power API - should fail
On to UserIdle to SystemIdle - timeout
SystemIdle to UserIdle - Power API
UserIdle to SystemIdle - Power API - will fail
UserIdle to SystemIdle - timeout
SystemIdle to On - Power API


*Transitions for Suspend\Resuming states*
On to UserIdle to SystemIdle to Suspend - timeouts
Suspend to resuming\On - Wake source
On to Suspend - Suspend timeout
Suspend to resuming\On - Wake source
On to SystemIdle to Suspend - timeouts
TO DO {Suspend to Unattended -System Activity in unattended mode??
Unattended to Suspend - out of unattended mode}
Suspend to resuming\On - Wake source
On to Resuming - Power API - should fail
On to Suspend - Power API
Suspend to resuming\On - Wake source
UserIdle to Resuming - Power API - should fail
UserIdle to Suspend - Power API
Suspend to resuming\On - Wake source
SystemIdle to Resuming - Power API - should fail
SystemIdle to Suspend - Power API
Suspend to resuming\On - Wake source

TODO
*Transitions for Unattended state*
*/


/*
DEFAULT Power Model State Transitions

*Remain in On state*
Remain in On state - User activity
Remain in On state - System activity
Remain in On state - On event
Remain in On state - Power API
Remain in On state - continuous User activity beyond UserIdle Transition timeout
*/
TESTPROCAPI Tst_TransitionOn_DEFAULT(UINT uMsg,
                                     TPPARAM /*tpParam*/,
                                     LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test. 
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    SET_PWR_TESTNAME(_T("Tst_TransitionOn_DEFAULT"));
    DWORD dwErr = 0;

    // This test is for the DEFAULT power model only, check if PDA tests are enabled
    if(g_bPdaVersion == TRUE)
    {
        PWRMSG(LOG_SKIP,_T("This test will skip as it targets the DEFAULT power model and the command line specified the PDA version."));
        PWRMSG(LOG_DETAIL,_T("Do not specify the option (-c\"PdaVersion\") to allow this test to execute."));
        return GetTestResult();
    }       

    // Continue with the test
    PWRMSG(LOG_DETAIL, _T("This test will verify remaining in the \"%s\" state when different events occur."), POWER_STATE_NAME_ON);

    PWRMSG(LOG_DETAIL,_T(""));
    // Set system power state to On
    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState(%s, 0, POWER_FORCE)"), POWER_STATE_NAME_ON);
    if(ERROR_SUCCESS !=  SetSystemPowerState(POWER_STATE_NAME_ON, 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now check if the Power State was indeed set
    if(!VerifyPowerState(POWER_STATE_NAME_ON))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*----------------Remain in On state - User activity---------------*/

    // Simulate User activity
    PWRMSG(LOG_DETAIL,_T("Simulating user activity."));
    if(!SimulateUserEvent())
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SimulateUserEvent() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now check if the Power State is still On
    if(!VerifyPowerState(POWER_STATE_NAME_ON))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*----------------Remain in On state - System activity-------------*/

    // Simulate System activity
    PWRMSG(LOG_DETAIL,_T("Simulating system activity."));
    if(!SimulateSystemEvent())
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SimulateSystemEvent() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now check if the Power State is still On
    if(!VerifyPowerState(POWER_STATE_NAME_ON))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-----------------Remain in On state - On event--------------*/

    PWRMSG(LOG_DETAIL,_T("Trigger On event."));
    if(!PowerPolicyNotify(PPN_APPBUTTONPRESSED, 0))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("PowerPolicyNotify(PPN_APPBUTTONPRESSED, 0) failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now check if the Power State is still On
    if(!VerifyPowerState(POWER_STATE_NAME_ON))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*------------------Remain in On state - Power API----------------*/

    // Set system to On using power API
    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState(%s, 0, POWER_FORCE)"), POWER_STATE_NAME_ON);
    if(ERROR_SUCCESS !=  SetSystemPowerState(POWER_STATE_NAME_ON, 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now check if the Power State was indeed set
    if(!VerifyPowerState(POWER_STATE_NAME_ON))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    /*------------------Remain in On state - Continuous User activity----------------*/

    // Verify that continuous user input will not allow the device 
    // to exit the "On" power state
    
    IDLE_TIMEOUTS itPrev = {0};
    IDLE_TIMEOUTS itNew = {20, 20, 0};
    
    if(!GetIdleTimeouts(&itPrev, FALSE))
    {
        PWRMSG(LOG_FAIL,_T("GetIdleTimeouts()"));
        goto Error;
    }
    PWRMSG(LOG_DETAIL,_T("Current Idle Timeout settings:"));
    PrintIdleTimeouts(&itPrev, FALSE);

    if(!SetIdleTimeouts(&itNew, FALSE))
    {
        PWRMSG(LOG_FAIL,_T("SetIdleTimeouts()"));
        goto Error;
    }

    if(!GetIdleTimeouts(&itNew, FALSE))
    {
        PWRMSG(LOG_FAIL,_T("GetIdleTimeouts()"));
        goto Error;
    }
    PWRMSG(LOG_DETAIL,_T("New Idle Timeout settings for this test:"));
    PrintIdleTimeouts(&itNew, FALSE);

    DWORD startTick = GetTickCount();
    DWORD elapsedTime = 0;
    DWORD waitTime = ((itNew.UserIdle + TRANSITION_TOLERANCE) * 1000);
    
    PWRMSG(LOG_DETAIL,_T("Waiting %d seconds to verify we stay in the \"%s\" system power state with continuous user activity"), 
        waitTime/1000, POWER_STATE_NAME_ON);
    while(elapsedTime < waitTime )
    {
        keybd_event(VK_SPACE, 0, 0, 0); // simulate keyboard event
        Sleep(0); // allow the system to service other threads
        elapsedTime = GetTickCount() - startTick; // re-calculate the time elapsed
    }

    // verify the device is still in the "On" system power state
    if(!PollForSystemPowerChange(POWER_STATE_NAME_ON, 0, 0))
    {
        PWRMSG(LOG_FAIL,_T("Device is NOT in the \"%s\" system power state as expected!"), POWER_STATE_NAME_ON);
        goto Error;
    }
    
    PWRMSG(LOG_DETAIL,_T("PASS: Continuous user input kept the device in the \"%s\" system power state."), POWER_STATE_NAME_ON);
    
Error:
    if(!SetIdleTimeouts(&itPrev, FALSE))
    {
        PWRMSG(LOG_FAIL,_T("SetIdleTimeouts()"));
    }
    else
    {
        PWRMSG(LOG_DETAIL,_T("Restored previous Idle Timeout settings:"));
        PrintIdleTimeouts(&itPrev, FALSE);
    }

    return GetTestResult();
}


/*
DEFAULT Power Model State Transitions

*Transitions for UserIdle state*
On to UserIdle - UserIdle timeout
UserIdle to On - User activity
On to UserIdle - Power API
UserIdle to UserIdle - Power API
UserIdle to On - Power API
*/
TESTPROCAPI Tst_TransitionUserIdle_DEFAULT(UINT uMsg,
                                           TPPARAM /*tpParam*/,
                                           LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test. 
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    SET_PWR_TESTNAME(_T("Tst_TransitionUserIdle_DEFAULT"));
    DWORD dwErr = 0;

    // This test is for the DEFAULT power model only, check if PDA tests are enabled
    if(g_bPdaVersion == TRUE)
    {
        PWRMSG(LOG_SKIP,_T("This test will skip as it targets the DEFAULT power model and the command line specified the PDA version."));
        PWRMSG(LOG_DETAIL,_T("Do not specify the option (-c\"PdaVersion\") to allow this test to execute."));
        return GetTestResult();
    }       

    // Continue with the test
    PWRMSG(LOG_DETAIL, _T("This test will verify transitions to and from the \"%s\" state."), POWER_STATE_NAME_USERIDLE);

    PWRMSG(LOG_DETAIL,_T(""));

    // Before we force the state to On, we need to guarantee any events from a previous test have been processed by the PM
    if(!RetrieveActivityEventTimeouts_DEFAULT())
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }
    else
    {
        PWRMSG(LOG_DETAIL,_T("Sleeping (%d + %d) to wait for previous event timeouts\r\n"), g_sysTimeouts.dwUserActivityEventTimeout, g_sysTimeouts.dwSystemActivityEventTimeout);
        Sleep(g_sysTimeouts.dwUserActivityEventTimeout);
        Sleep(g_sysTimeouts.dwSystemActivityEventTimeout);
    }

    // Set system power state to On
    PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(%s, 0, POWER_FORCE)"), POWER_STATE_NAME_ON);
    if(ERROR_SUCCESS !=  SetSystemPowerState(POWER_STATE_NAME_ON, 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now Get Power State to check if the Power State was indeed set
    if(!VerifyPowerState(POWER_STATE_NAME_ON))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }


    /*------------------On to UserIdle - UserIdle timeout----------------*/

    // Set UserIdle timeout value so it is less than the SystemIdle and Suspend timeouts. We want to transition to UserIdle.
    PWRMSG(LOG_DETAIL,_T(""));

    // First retrieve original timeouts so we can set them back
    if(!RetrieveTimeouts_DEFAULT())
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }
    // Store them
    DWORD dwOrigACUserIdleTimeout = g_sysTimeouts.dwACUserIdleTimeout;
    DWORD dwOrigACSystemIdleTimeout = g_sysTimeouts.dwACSystemIdleTimeout;
    DWORD dwOrigACSuspendTimeout = g_sysTimeouts.dwACSuspendTimeout;

    // Now setup the timeouts that we want for the test
    if(!SetUpTimeouts_DEFAULT(15, 30, 100))
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Retrieve timeouts
    if(!RetrieveTimeouts_DEFAULT())
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Verify we got what we set
    if((g_sysTimeouts.dwACUserIdleTimeout != 15) || (g_sysTimeouts.dwACSystemIdleTimeout != 30) || (g_sysTimeouts.dwACSuspendTimeout != 100))
    {
        PWRMSG(LOG_DETAIL, _T("Failed to setup timeouts."));
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }
    else
    {
        PWRMSG(LOG_DETAIL, _T("Setup timeouts done. ACUserIdle timeout = %d, ACSystemIdle timeout = %d, ACSuspend timeout = %d."), 
            g_sysTimeouts.dwACUserIdleTimeout, g_sysTimeouts.dwACSystemIdleTimeout, g_sysTimeouts.dwACSuspendTimeout);
    }

    PWRMSG(LOG_DETAIL,_T(""));

    // Sleep so that UserIdle timeout expires
    PWRMSG(LOG_DETAIL, _T("Now we will sleep until UserIdle timeout expires so we transition to \"%s\" state."), POWER_STATE_NAME_USERIDLE);
    Sleep((g_sysTimeouts.dwACUserIdleTimeout + 3) * 1000);

    // Verify if we transitioned to UserIdle
    if(!VerifyPowerState(POWER_STATE_NAME_USERIDLE))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-----------------UserIdle to On - User activity----------------*/

    // Get back to On due to user activity
    PWRMSG(LOG_DETAIL, _T("Simulate user activity so we transition back to the \"%s\" state."), POWER_STATE_NAME_ON);
    if(!SimulateUserEvent())
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SimulateUserEvent() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Need to allow the state change to take place.
    // This sleep is needed for systems with a multi-core processor
    Sleep(200);

    // Verify if we transitioned to On
    if(!VerifyPowerState(POWER_STATE_NAME_ON))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------On to UserIdle - Power API------------------*/

    // Set system to UserIdle using power API
    PWRMSG(LOG_DETAIL,_T("Now we will call SetSystemPowerState(%s, 0, POWER_FORCE) so we transition to \"%s\" state."), 
        POWER_STATE_NAME_USERIDLE, POWER_STATE_NAME_USERIDLE);
    if(ERROR_SUCCESS !=  SetSystemPowerState(POWER_STATE_NAME_USERIDLE, 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we transitioned to UserIdle
    if(!VerifyPowerState(POWER_STATE_NAME_USERIDLE))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------UserIdle to UserIdle - Power API------------------*/

    // Set system to UserIdle using power API
    PWRMSG(LOG_DETAIL,_T("Now we will call SetSystemPowerState(%s, 0, POWER_FORCE) again and verify we are still in \"%s\" state."),
        POWER_STATE_NAME_USERIDLE, POWER_STATE_NAME_USERIDLE);
    if(ERROR_SUCCESS !=  SetSystemPowerState(POWER_STATE_NAME_USERIDLE, 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we are still in UserIdle
    if(!VerifyPowerState(POWER_STATE_NAME_USERIDLE))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------UserIdle to On - Power API------------------*/

    // Set system to On using power API
    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState to transition back to \"%s\" state."), POWER_STATE_NAME_ON);
    if(ERROR_SUCCESS !=  SetSystemPowerState(POWER_STATE_NAME_ON, 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState(%s, 0, POWER_FORCE) failed. GetLastError = %d"), POWER_STATE_NAME_ON, dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we transitioned to On
    if(!VerifyPowerState(POWER_STATE_NAME_ON))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));

    // Now set back the original timeouts
    if(!SetUpTimeouts_DEFAULT(dwOrigACUserIdleTimeout, dwOrigACSystemIdleTimeout, dwOrigACSuspendTimeout))
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    return GetTestResult();

}


/*
DEFAULT Power Model State Transitions

*Transitions for SystemIdle state*
On to SystemIdle - SystemIdle timeout
SystemIdle to On - System activity - should fail
SystemIdle to On - User activity
On to SystemIdle - Power API - should fail
On to UserIdle to SystemIdle - timeout
SystemIdle to UserIdle - Power API
UserIdle to SystemIdle - Power API - will fail
UserIdle to SystemIdle - timeout
SystemIdle to On - Power API
*/
TESTPROCAPI Tst_TransitionSystemIdle_DEFAULT(UINT uMsg,
                                             TPPARAM /*tpParam*/,
                                             LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test. 
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    SET_PWR_TESTNAME(_T("Tst_TransitionSystemIdle_DEFAULT"));
    DWORD dwErr = 0;

    // This test is for the DEFAULT power model only, check if PDA tests are enabled
    if(g_bPdaVersion == TRUE)
    {
        PWRMSG(LOG_SKIP,_T("This test will skip as it targets the DEFAULT power model and the command line specified the PDA version."));
        PWRMSG(LOG_DETAIL,_T("Do not specify the option (-c\"PdaVersion\") to allow this test to execute."));
        return GetTestResult();
    }       

    // Continue with the test
    PWRMSG(LOG_DETAIL, _T("This test will verify transitions to and from the \"%s\" state."), POWER_STATE_NAME_SYSTEMIDLE);

    PWRMSG(LOG_DETAIL,_T(""));

    if(!RetrieveActivityEventTimeouts_DEFAULT())
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }
    else
    {
        PWRMSG(LOG_DETAIL,_T("Sleeping (%d + %d) to wait for previous event timeouts\r\n"), g_sysTimeouts.dwUserActivityEventTimeout, g_sysTimeouts.dwSystemActivityEventTimeout);
        Sleep(g_sysTimeouts.dwUserActivityEventTimeout);
        Sleep(g_sysTimeouts.dwSystemActivityEventTimeout);
    }

    // Set system power state to On
    PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(%s, 0, POWER_FORCE)"), POWER_STATE_NAME_ON);
    if(ERROR_SUCCESS !=  SetSystemPowerState(POWER_STATE_NAME_ON, 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now Get Power State to check if the Power State was indeed set
    if(!VerifyPowerState(POWER_STATE_NAME_ON))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }


    /*-------------------On to SystemIdle - SystemIdle timeout------------------*/

    // Set up timeouts. SystemIdle timeout will occur at UserIdle+SystemIdle timeout.
    PWRMSG(LOG_DETAIL,_T(""));

    // First retrieve original timeouts so we can set them back
    if(!RetrieveTimeouts_DEFAULT())
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }
    // Store them
    DWORD dwOrigACUserIdleTimeout = g_sysTimeouts.dwACUserIdleTimeout;
    DWORD dwOrigACSystemIdleTimeout = g_sysTimeouts.dwACSystemIdleTimeout;
    DWORD dwOrigACSuspendTimeout = g_sysTimeouts.dwACSuspendTimeout;

    // Now setup the timeouts that we want for the test
    if(!SetUpTimeouts_DEFAULT(30, 15, 100))
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Retrieve timeouts
    if(!RetrieveTimeouts_DEFAULT())
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Verify we got what we set
    if((g_sysTimeouts.dwACUserIdleTimeout != 30) || (g_sysTimeouts.dwACSystemIdleTimeout != 15) || (g_sysTimeouts.dwACSuspendTimeout != 100))
    {
        PWRMSG(LOG_DETAIL, _T("Failed to setup timeouts."));
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }
    else
    {
        PWRMSG(LOG_DETAIL, _T("Setup timeouts done. ACUserIdle timeout = %d, ACSystemIdle timeout = %d, ACSuspend timeout = %d."), 
            g_sysTimeouts.dwACUserIdleTimeout, g_sysTimeouts.dwACSystemIdleTimeout, g_sysTimeouts.dwACSuspendTimeout);
    }

    PWRMSG(LOG_DETAIL,_T(""));

    // Sleep so that SystemIdle timeout expires
    PWRMSG(LOG_DETAIL, _T("Now we will sleep until SystemIdle timeout expires so we transition to \"%s\" state."), POWER_STATE_NAME_SYSTEMIDLE);
    Sleep((g_sysTimeouts.dwACUserIdleTimeout + g_sysTimeouts.dwACSystemIdleTimeout + 3) * 1000);

    // Verify if we transitioned to SystemIdle
    if(!VerifyPowerState(POWER_STATE_NAME_SYSTEMIDLE))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*---------------SystemIdle to On - System activity - should fail-----------*/

    // System activity should not transition the device back to On
    PWRMSG(LOG_DETAIL, _T("Simulate system activity so verify we don't transition back to the \"%s\" state."), POWER_STATE_NAME_ON);
    if(!SimulateSystemEvent())
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SimulateSystemEvent() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we remained in SystemIdle
    if(!VerifyPowerState(POWER_STATE_NAME_SYSTEMIDLE))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------SystemIdle to On - User activity--------------------*/

    // Get back to On due to user activity
    PWRMSG(LOG_DETAIL, _T("Simulate user activity so we transition back to the \"%s\" state."), POWER_STATE_NAME_ON);
    if(!SimulateUserEvent())
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SimulateUserEvent() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Need to allow the state change to take place.
    // This sleep is needed for systems with a multi-core processor
    Sleep(200);

    // Verify if we transitioned to On
    if(!VerifyPowerState(POWER_STATE_NAME_ON))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-----------------On to SystemIdle - Power API - should fail--------------*/

    // Set system to SystemIdle using power API
    PWRMSG(LOG_DETAIL,_T("Now we will call SetSystemPowerState(%s, 0, POWER_FORCE), this call should fail."), POWER_STATE_NAME_SYSTEMIDLE);
    if(ERROR_SUCCESS ==  SetSystemPowerState(_T("SystemIdle"), 0, POWER_FORCE))
    {
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() succeeded, it was expected to fail."));
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we remained in On state
    if(!VerifyPowerState(POWER_STATE_NAME_ON))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-----------------On to UserIdle to SystemIdle - timeout--------------------*/

    // Set UserIdle timeout value so it is less than the SystemIdle which is less than the Suspend timeout.
    // We want to transition to UserIdle and then on to SystemIdle.

    if(!SetUpTimeouts_DEFAULT(15, 30, 100))
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Retrieve timeouts
    if(!RetrieveTimeouts_DEFAULT())
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Verify we got what we set
    if((g_sysTimeouts.dwACUserIdleTimeout != 15) || (g_sysTimeouts.dwACSystemIdleTimeout != 30) || (g_sysTimeouts.dwACSuspendTimeout != 100))
    {
        PWRMSG(LOG_DETAIL, _T("Failed to setup timeouts."));
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }
    else
    {
        PWRMSG(LOG_DETAIL, _T("Setup timeouts done. ACUserIdle timeout = %d, ACSystemIdle timeout = %d, ACSuspend timeout = %d."), 
            g_sysTimeouts.dwACUserIdleTimeout, g_sysTimeouts.dwACSystemIdleTimeout, g_sysTimeouts.dwACSuspendTimeout);
    }

    PWRMSG(LOG_DETAIL,_T(""));

    // Sleep so that UserIdle timeout expires
    PWRMSG(LOG_DETAIL, _T("Now we will sleep until UserIdle timeout expires so we transition to \"%s\" state."), POWER_STATE_NAME_USERIDLE);
    Sleep((g_sysTimeouts.dwACUserIdleTimeout + 3) * 1000);

    // Verify if we transitioned to UserIdle
    if(!VerifyPowerState(POWER_STATE_NAME_USERIDLE))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));

    // Sleep so that SystemIdle timeout expires
    PWRMSG(LOG_DETAIL, _T("Now we will sleep until SystemIdle timeout expires so we transition to \"%s\" state."), POWER_STATE_NAME_SYSTEMIDLE);
    Sleep((g_sysTimeouts.dwACSystemIdleTimeout + 3) * 1000);

    // Verify if we transitioned to SystemIdle
    if(!VerifyPowerState(POWER_STATE_NAME_SYSTEMIDLE))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-----------------SystemIdle to UserIdle - Power API--------------------*/

    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState to transition to \"%s\" state."), POWER_STATE_NAME_USERIDLE);
    if(ERROR_SUCCESS !=  SetSystemPowerState(POWER_STATE_NAME_USERIDLE, 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState(UserIdle, 0, POWER_FORCE) failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we transitioned to UserIdle
    if(!VerifyPowerState(POWER_STATE_NAME_USERIDLE))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*---------------UserIdle to SystemIdle - Power API - will fail-----------*/

    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState to transition to \"%s\" state, it should fail."), POWER_STATE_NAME_SYSTEMIDLE);
    if(ERROR_SUCCESS ==  SetSystemPowerState(POWER_STATE_NAME_SYSTEMIDLE, 0, POWER_FORCE))
    {
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState(%s, 0, POWER_FORCE) succeeded, it is expected to fail."), POWER_STATE_NAME_SYSTEMIDLE);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we remained in UserIdle
    if(!VerifyPowerState(POWER_STATE_NAME_USERIDLE))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------UserIdle to SystemIdle - timeout-------------------*/

    // Sleep so that SystemIdle timeout expires
    PWRMSG(LOG_DETAIL, _T("Now we will sleep until SystemIdle timeout expires so we transition to \"%s\" state."), POWER_STATE_NAME_SYSTEMIDLE);
    Sleep((g_sysTimeouts.dwACSystemIdleTimeout + 3) * 1000);

    // Verify if we transitioned to SystemIdle
    if(!VerifyPowerState(POWER_STATE_NAME_SYSTEMIDLE))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*---------------------SystemIdle to On - Power API----------------------*/

    // Set system to On using power API
    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState to transition back to \"%s\" state."), POWER_STATE_NAME_ON);
    if(ERROR_SUCCESS !=  SetSystemPowerState(POWER_STATE_NAME_ON, 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState(%s, 0, POWER_FORCE) failed. GetLastError = %d"), POWER_STATE_NAME_ON, dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we transitioned to On
    if(!VerifyPowerState(POWER_STATE_NAME_ON))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    // Now set back the original timeouts
    if(!SetUpTimeouts_DEFAULT(dwOrigACUserIdleTimeout, dwOrigACSystemIdleTimeout, dwOrigACSuspendTimeout))
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    return GetTestResult();
}


/*
DEFAULT Power Model State Transitions

*Transitions for Suspend\Resuming states*
On to UserIdle to SystemIdle to Suspend - timeouts
Suspend to resuming\On - Wake source
On to Suspend - Suspend timeout
Suspend to resuming\On - Wake source
On to Resuming - Power API - should fail
On to Suspend - Power API
Suspend to resuming\On - Wake source
UserIdle to Resuming - Power API - should fail
UserIdle to Suspend - Power API
Suspend to resuming\On - Wake source
SystemIdle to Resuming - Power API - should fail
SystemIdle to Suspend - Power API
Suspend to resuming\On - Wake source
*/

TESTPROCAPI Tst_TransitionSuspend_DEFAULT(UINT uMsg,
                                          TPPARAM /*tpParam*/,
                                          LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test. 
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    SET_PWR_TESTNAME(_T("Tst_TransitionSuspend_DEFAULT"));
    DWORD dwErr = 0;

    CSysPwrStates *pCSysPwr = NULL;

    pCSysPwr = new CSysPwrStates;
    if(pCSysPwr == NULL)
    {
        PWRMSG(LOG_FAIL, _T("Out of Memory!"));
        return TPR_FAIL;
    }

    // This test is for the DEFAULT power model only, check if PDA tests are enabled
    if(g_bPdaVersion == TRUE)
    {
        PWRMSG(LOG_SKIP,_T("This test will skip as it targets the DEFAULT power model and the command line specified the PDA version."));
        PWRMSG(LOG_DETAIL,_T("Do not specify the option (-c\"PdaVersion\") to allow this test to execute."));
        return GetTestResult();
    }       

    // Continue with the test
    PWRMSG(LOG_DETAIL, _T("This test will verify transitions to and from the \"%s\" state."), POWER_STATE_NAME_SUSPEND);

    PWRMSG(LOG_DETAIL,_T(""));

    // Set system power state to On
    PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(%s, 0, POWER_FORCE)"), POWER_STATE_NAME_ON);
    if(ERROR_SUCCESS !=  SetSystemPowerState(POWER_STATE_NAME_ON, 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Now Get Power State to check if the Power State was indeed set
    if(!VerifyPowerState(POWER_STATE_NAME_ON))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }


    /*------------On to UserIdle to SystemIdle to Suspend - timeouts------------*/

    // Set timeouts so that we transition to UserIdle->SystemIdle->Suspend.

    PWRMSG(LOG_DETAIL,_T(""));

    // First retrieve original timeouts so we can set them back
    if(!RetrieveTimeouts_DEFAULT())
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Store them
    DWORD dwOrigACUserIdleTimeout = g_sysTimeouts.dwACUserIdleTimeout;
    DWORD dwOrigACSystemIdleTimeout = g_sysTimeouts.dwACSystemIdleTimeout;
    DWORD dwOrigACSuspendTimeout = g_sysTimeouts.dwACSuspendTimeout;

    // Now setup the timeouts that we want for the test
    if(!SetUpTimeouts_DEFAULT(15, 30, 45))
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Retrieve timeouts
    if(!RetrieveTimeouts_DEFAULT())
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Verify we got what we set
    if((g_sysTimeouts.dwACUserIdleTimeout != 15) || (g_sysTimeouts.dwACSystemIdleTimeout != 30) || (g_sysTimeouts.dwACSuspendTimeout != 45))
    {
        PWRMSG(LOG_DETAIL, _T("Failed to setup timeouts."));
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }
    else
    {
        PWRMSG(LOG_DETAIL, _T("Setup timeouts done. ACUserIdle timeout = %d, ACSystemIdle timeout = %d, ACSuspend timeout = %d."), 
            g_sysTimeouts.dwACUserIdleTimeout, g_sysTimeouts.dwACSystemIdleTimeout, g_sysTimeouts.dwACSuspendTimeout);
    }

    PWRMSG(LOG_DETAIL,_T(""));

    // Check if suspending the device is allowed
    if(g_bAllowSuspend == FALSE)
    {
        PWRMSG(LOG_SKIP,_T("Cannot Transition to State \"%s\", this option is not allowed. "), POWER_STATE_NAME_SUSPEND);
        PWRMSG(LOG_DETAIL,_T("Specify this option (-c\"AllowSuspend\") to allow the tests to suspend the system."));
        goto CleanAndExit;
    }

    // Sleep so that UserIdle timeout expires
    PWRMSG(LOG_DETAIL, _T("Now we will sleep until UserIdle timeout expires so we transition to \"%s\" state."), POWER_STATE_NAME_USERIDLE);
    Sleep((g_sysTimeouts.dwACUserIdleTimeout + 3) * 1000);

    // Verify if we transitioned to UserIdle
    if(!VerifyPowerState(POWER_STATE_NAME_USERIDLE))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));

    // Sleep so that SystemIdle timeout expires
    PWRMSG(LOG_DETAIL, _T("Now we will sleep until SystemIdle timeout expires so we transition to \"%s\" state."), POWER_STATE_NAME_SYSTEMIDLE);
    Sleep((g_sysTimeouts.dwACSystemIdleTimeout + 3) * 1000);

    // Verify if we transitioned to SystemIdle
    if(!VerifyPowerState(POWER_STATE_NAME_SYSTEMIDLE))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));

    // First set up wakeup resource, then suspend.
    WORD wWakeupTime = ((WORD)(g_sysTimeouts.dwACSuspendTimeout + 3)) + 15; //Time it takes to suspend + 15 min.
    PWRMSG(LOG_DETAIL, _T("Prepare RTC wakeup resource-wake up in %d seconds..."), wWakeupTime);

    if(pCSysPwr->SetupRTCWakeupTimer(wWakeupTime) == FALSE )
    {
        PWRMSG(LOG_FAIL, _T("Can not setup RTC alarm as the wakeup resource"));
        goto CleanAndExit;
    }

    // Sleep so that Suspend timeout expires
    PWRMSG(LOG_DETAIL, _T("Now we will sleep until Suspend timeout expires so we transition to \"%s\" state."), POWER_STATE_NAME_SUSPEND);
    Sleep((g_sysTimeouts.dwACSuspendTimeout + 3) * 1000);

    PWRMSG(LOG_DETAIL,_T(""));

    // Check if we are back to On state after resume
    // Verify if we transitioned to On
    if(!VerifyPowerState(POWER_STATE_NAME_ON))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    pCSysPwr->CleanupRTCTimer();

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------On to Suspend - Suspend timeout------------------*/

    // First set up wakeup resource, then suspend.
    wWakeupTime = ((WORD)(g_sysTimeouts.dwACUserIdleTimeout + g_sysTimeouts.dwACSystemIdleTimeout + g_sysTimeouts.dwACSuspendTimeout + 3)) + 15; //Time it takes to suspend + 15 min.
    PWRMSG(LOG_DETAIL, _T("Prepare RTC wakeup resource-wake up in %d seconds..."), wWakeupTime);

    if(pCSysPwr->SetupRTCWakeupTimer(wWakeupTime) == FALSE )
    {
        PWRMSG(LOG_FAIL, _T("Can not setup RTC alarm as the wakeup resource"));
        goto CleanAndExit;
    }

    // Sleep so that Suspend timeout expires - need to sleep for UserIdle+SystemIdle+Suspend timeout length
    PWRMSG(LOG_DETAIL, _T("Now we will sleep until Suspend timeout expires so we transition to \"%s\" state."), POWER_STATE_NAME_SUSPEND);
    Sleep((g_sysTimeouts.dwACUserIdleTimeout + g_sysTimeouts.dwACSystemIdleTimeout + g_sysTimeouts.dwACSuspendTimeout + 3) * 1000);

    PWRMSG(LOG_DETAIL,_T(""));

    // Check if we are back to On state after resume
    // Verify if we transitioned to On
    if(!VerifyPowerState(_T("On")))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    pCSysPwr->CleanupRTCTimer();

    PWRMSG(LOG_DETAIL,_T(""));


    /*----------------On to Resuming - Power API - should fail---------------*/

    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState to transition to \"%s\" state."), POWER_STATE_NAME_RESUMING);
    if(ERROR_SUCCESS ==  SetSystemPowerState(POWER_STATE_NAME_RESUMING, 0, POWER_FORCE))
    {
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState(%s, 0, POWER_FORCE) succeeded, expected to fail."), POWER_STATE_NAME_RESUMING);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we are still in On state
    if(!VerifyPowerState(POWER_STATE_NAME_ON))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-------------------On to Suspend - Power API------------------------*/

    // First set up wakeup resource, then suspend.
    wWakeupTime = 15;
    PWRMSG(LOG_DETAIL, _T("Prepare RTC wakeup resource-wake up in %d seconds..."), wWakeupTime);

    if(pCSysPwr->SetupRTCWakeupTimer(wWakeupTime) == FALSE )
    {
        PWRMSG(LOG_FAIL, _T("Can not setup RTC alarm as the wakeup resource"));
        goto CleanAndExit;
    }

    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState to transition to \"%s\" state."), POWER_STATE_NAME_SUSPEND);
    if(ERROR_SUCCESS !=  SetSystemPowerState(POWER_STATE_NAME_SUSPEND, 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState(%s, 0, POWER_FORCE) failed. GetLastError = %d"), POWER_STATE_NAME_SUSPEND, dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    PWRMSG(LOG_DETAIL,_T(""));

    // Check if we are back to On state after resume
    // Verify if we transitioned to On
    if(!VerifyPowerState(POWER_STATE_NAME_ON))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    pCSysPwr->CleanupRTCTimer();


    /*--------------UserIdle to Resuming - Power API - should fail-------------*/

    // First set system to UserIdle
    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState to transition to \"%s\" state."), POWER_STATE_NAME_USERIDLE);
    if(ERROR_SUCCESS !=  SetSystemPowerState(POWER_STATE_NAME_USERIDLE, 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState(%s, 0, POWER_FORCE) failed. GetLastError = %d"), POWER_STATE_NAME_USERIDLE, dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we transitioned to UserIdle
    if(!VerifyPowerState(POWER_STATE_NAME_USERIDLE))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    // Now try to set to resuming state
    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState to transition to \"%s\" state."), POWER_STATE_NAME_RESUMING);
    if(ERROR_SUCCESS ==  SetSystemPowerState(POWER_STATE_NAME_RESUMING, 0, POWER_FORCE))
    {
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState(%s, 0, POWER_FORCE) succeeded, expected to fail."), POWER_STATE_NAME_RESUMING);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we are still in UserIdle state
    if(!VerifyPowerState(POWER_STATE_NAME_USERIDLE))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-----------------UserIdle to Suspend - Power API----------------------*/

    // First set up wakeup resource, then suspend.
    wWakeupTime = 15;
    PWRMSG(LOG_DETAIL, _T("Prepare RTC wakeup resource-wake up in %d seconds..."), wWakeupTime);

    if(pCSysPwr->SetupRTCWakeupTimer(wWakeupTime) == FALSE )
    {
        PWRMSG(LOG_FAIL, _T("Can not setup RTC alarm as the wakeup resource"));
        goto CleanAndExit;
    }

    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState to transition to \"%s\" state."), POWER_STATE_NAME_SUSPEND);
    if(ERROR_SUCCESS !=  SetSystemPowerState(POWER_STATE_NAME_SUSPEND, 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState(%s, 0, POWER_FORCE) failed. GetLastError = %d"), POWER_STATE_NAME_SUSPEND, dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    PWRMSG(LOG_DETAIL,_T(""));

    // Check if we are back to On state after resume
    // Verify if we transitioned to On
    if(!VerifyPowerState(POWER_STATE_NAME_ON))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    pCSysPwr->CleanupRTCTimer();


    /*---------------SystemIdle to Resuming - Power API - should fail------------*/

    // First set system to SystemIdle
    // Sleep so that SystemIdle timeout expires - sleep for the length of UserIdle + SystemIdle timeouts
    PWRMSG(LOG_DETAIL, _T("Now we will sleep until SystemIdle timeout expires so we transition to \"%s\" state."), POWER_STATE_NAME_SYSTEMIDLE);
    Sleep((g_sysTimeouts.dwACUserIdleTimeout + g_sysTimeouts.dwACSystemIdleTimeout + 3) * 1000);

    // Verify if we transitioned to SystemIdle
    if(!VerifyPowerState(POWER_STATE_NAME_SYSTEMIDLE))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));

    // SystemIdle to Resuming - Power API - should fail
    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState to transition to \"%s\" state."), POWER_STATE_NAME_RESUMING);
    if(ERROR_SUCCESS ==  SetSystemPowerState(POWER_STATE_NAME_RESUMING, 0, POWER_FORCE))
    {
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState(%s, 0, POWER_FORCE) succeeded, expected to fail."), POWER_STATE_NAME_RESUMING);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    // Verify if we are still in SystemIdle state
    if(!VerifyPowerState(POWER_STATE_NAME_SYSTEMIDLE))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T(""));


    /*-----------------SystemIdle to Suspend - Power API-------------------*/

    // First set up wakeup resource, then suspend.
    wWakeupTime = 15;
    PWRMSG(LOG_DETAIL, _T("Prepare RTC wakeup resource-wake up in %d seconds..."), wWakeupTime);

    if(pCSysPwr->SetupRTCWakeupTimer(wWakeupTime) == FALSE )
    {
        PWRMSG(LOG_FAIL, _T("Can not setup RTC alarm as the wakeup resource"));
        goto CleanAndExit;
    }

    PWRMSG(LOG_DETAIL,_T("Call SetSystemPowerState to transition to \"%s\" state."), POWER_STATE_NAME_SUSPEND);
    if(ERROR_SUCCESS !=  SetSystemPowerState(POWER_STATE_NAME_SUSPEND, 0, POWER_FORCE))
    {
        dwErr = GetLastError();
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState(%s, 0, POWER_FORCE) failed. GetLastError = %d"), POWER_STATE_NAME_SUSPEND, dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);    
    }

    PWRMSG(LOG_DETAIL,_T(""));

    // Check if we are back to On state after resume
    // Verify if we transitioned to On
    if(!VerifyPowerState(POWER_STATE_NAME_ON))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    pCSysPwr->CleanupRTCTimer();

    PWRMSG(LOG_DETAIL,_T(""));


CleanAndExit:

    // Now set back the original timeouts
    if(!SetUpTimeouts_DEFAULT(dwOrigACUserIdleTimeout, dwOrigACSystemIdleTimeout, dwOrigACSuspendTimeout))
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    if(pCSysPwr)
    {
        delete pCSysPwr;
    }

    return GetTestResult();
}


BOOL VerifyPowerState(LPCWSTR pszPwrStateName)
{

    TCHAR szGetPowerState[MAX_PATH];
    DWORD dwGetFlags = 0;

    // Now Get Power State to check if the Power State was indeed set
    if(ERROR_SUCCESS !=  GetSystemPowerState(szGetPowerState, _countof(szGetPowerState), &dwGetFlags))
    {
        PWRMSG(LOG_DETAIL,_T("GetSystemPowerState() returned FALSE. GetLastError = %d"), GetLastError());
        return FALSE;
    }
    else 
    {
        PWRMSG(LOG_DETAIL, _T("System Power state is \"%s\", State Flag: 0x%08x\n"), szGetPowerState, dwGetFlags);

        // Verify that it is the expected power state
        if(0 == _tcsnicmp(szGetPowerState, pszPwrStateName, sizeof(szGetPowerState)))
        {
            PWRMSG(LOG_DETAIL, _T("System Power state obtained matches the expected Power State."));
        }
        else
        {
            PWRMSG(LOG_DETAIL, _T("The Power state obtained does not match the expected Power State."));
            return FALSE;
        }
    }
    return TRUE;
}


DWORD RegQueryTypedValue(HKEY hk, LPCWSTR pszValue, PVOID pvData, 
                         LPDWORD pdwSize, DWORD dwType)
{
    DWORD dwActualType;

    DWORD dwStatus = RegQueryValueEx(hk, pszValue, NULL, &dwActualType, 
        (PBYTE) pvData, pdwSize);
    if(dwStatus == ERROR_SUCCESS && dwActualType != dwType) {
        dwStatus = ERROR_INVALID_DATA;
    }

    return dwStatus;
}


BOOL RetrieveTimeouts_PDA(void)
{
    BOOL bRet = FALSE;
    HKEY hKey;
    DWORD dwStatus;
    SET_PWR_TESTNAME(_T("RetrieveTimeouts_PDA"));
    //check control panel settings

    // Display\UserIdle timeout
    dwStatus = RegOpenKeyEx(HKEY_CURRENT_USER, _T("ControlPanel\\Power"), 0, 0, &hKey);
    if(dwStatus != ERROR_SUCCESS)
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        goto EXIT;
    }

    DWORD   dwValue = 0;
    DWORD   cbSize = sizeof(DWORD);
    //get Idle Timeout
    DWORD dwRet = RegQueryTypedValue(hKey, _T("Display"), &dwValue, &cbSize, REG_DWORD);
    if(dwRet != ERROR_SUCCESS)
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        goto EXIT;
    }
    g_dwUserIdleTimeout = dwValue;
    RegCloseKey(hKey);


    // Backlight timeout
    dwStatus = RegOpenKeyEx(HKEY_CURRENT_USER, _T("ControlPanel\\Backlight"), 0, 0, &hKey);
    if(dwStatus != ERROR_SUCCESS)
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        goto EXIT;
    }
    dwValue = 0;
    cbSize = sizeof(DWORD);
    //get Backlight Timeout
    dwRet = RegQueryTypedValue(hKey, _T("ACTimeout"), &dwValue, &cbSize, REG_DWORD);
    if(dwRet != ERROR_SUCCESS)
    {
        PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        goto EXIT;
    }
    g_dwBacklightTimeout = dwValue;
    RegCloseKey(hKey);

    bRet = TRUE;

EXIT:    

    if (hKey)
    {
        RegCloseKey(hKey);
    }

    PWRMSG (LOG_DETAIL, _T("BacklightTimeout=%d, UserTimeout=%d"), g_dwBacklightTimeout, g_dwUserIdleTimeout);

    return bRet;
}


BOOL SetUpTimeouts_PDA(DWORD dwBacklight, DWORD dwDisplay)
{
    BOOL bRet = FALSE;
    HKEY hKey = NULL;
    DWORD dwDisposition;
    DWORD dwLen = sizeof(DWORD);
    SET_PWR_TESTNAME(_T("RetrieveTimeouts_PDA"));
    
    // Set Backlight timeout
    if(ERROR_SUCCESS != RegCreateKeyEx(HKEY_CURRENT_USER, _T("ControlPanel\\Backlight"), 0, NULL, 
        REG_OPTION_NON_VOLATILE, 0, NULL, &hKey, &dwDisposition))
        goto EXIT;

    if(ERROR_SUCCESS != RegSetValueEx(hKey, _T("ACTimeout"), 0, REG_DWORD,(LPBYTE)&dwBacklight, dwLen))
        goto EXIT;

    RegCloseKey(hKey);
    hKey = NULL;


    // Set Display timeout
    if(ERROR_SUCCESS != RegCreateKeyEx(HKEY_CURRENT_USER, _T("ControlPanel\\Power"), 0, NULL, 
        REG_OPTION_NON_VOLATILE, 0, NULL, &hKey, &dwDisposition))
        goto EXIT;

    if(ERROR_SUCCESS != RegSetValueEx(hKey, _T("Display"), 0, REG_DWORD,(LPBYTE)&dwDisplay, dwLen))
        goto EXIT;

    RegCloseKey(hKey);
    hKey = NULL;


    SimulateReloadTimeoutsEvent();

    // notify system of new timeouts
    PREFAST_SUPPRESS(25084, "suppress EVENT_ALL_ACCESS warning");
    HANDLE hBLEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, TEXT("BackLightChangeEvent"));
    if (hBLEvent) 
    {
        SetEvent(hBLEvent);
        CloseHandle(hBLEvent);
    }

    PREFAST_SUPPRESS(25084, "suppress EVENT_ALL_ACCESS warning");
    HANDLE hPMEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, TEXT("PowerManager/ReloadActivityTimeouts"));
    if (hPMEvent) 
    {
        SetEvent(hPMEvent);
        CloseHandle(hPMEvent);
    }

    NotifyWinUserSystem(NWUS_MAX_IDLE_TIME_CHANGED);

    bRet = TRUE;

EXIT:    

    if (hKey)
    {
        RegCloseKey(hKey);
    }

    return bRet;
}


BOOL RetrieveTimeouts_DEFAULT(void)
{
    BOOL bRet = FALSE;
    TCHAR szPath[MAX_PATH];
    HKEY hKey;
    DWORD dwRet;

    // Construct the reg key and get the timeouts
    StringCchPrintf(szPath, MAX_PATH, _T("%s\\%s"), PWRMGR_REG_KEY, _T("Timeouts"));

    DWORD dwStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szPath, 0, 0, &hKey);
    if(dwStatus != ERROR_SUCCESS)
    {
        PWRMSG(LOG_FAIL, _T("Could not open reg key %s "), szPath);
        goto EXIT;
    }

    DWORD   dwValue = 0;
    DWORD   cbSize = sizeof(DWORD);

    //get ACUserIdleTimeout
    dwRet = RegQueryTypedValue(hKey, _T("ACUserIdle"), &dwValue, &cbSize, REG_DWORD);
    if(dwRet != ERROR_SUCCESS){
        PWRMSG(LOG_FAIL, _T("Failed to get ACUserIdle value!"));
        goto EXIT;
    }
    else{
        g_sysTimeouts.dwACUserIdleTimeout= dwValue;
    }

    //get ACSystemIdleTimeout
    dwRet = RegQueryTypedValue(hKey, _T("ACSystemIdle"), &dwValue, &cbSize, REG_DWORD);
    if(dwRet != ERROR_SUCCESS){
        PWRMSG(LOG_FAIL, _T("Can not get ACSystemIdle value!"));
        goto EXIT;
    }
    else{
        g_sysTimeouts.dwACSystemIdleTimeout = dwValue;
    }

    //get ACSuspendTimeout
    dwRet = RegQueryTypedValue(hKey, _T("ACSuspend"), &dwValue, &cbSize, REG_DWORD);
    if(dwRet != ERROR_SUCCESS){
        PWRMSG(LOG_FAIL, _T("Failed to get ACSuspend value!"));
        goto EXIT;
    }
    else{
        g_sysTimeouts.dwACSuspendTimeout = dwValue;
    }    

    bRet = TRUE;

EXIT:    

    if (hKey)
    {
        RegCloseKey(hKey);
    }

    PWRMSG(LOG_DETAIL, _T("ACUserIdleTimeout=%d, ACSystemIdleTimeout=%d, ACSuspendTimeout=%d"), 
        g_sysTimeouts.dwACUserIdleTimeout, g_sysTimeouts.dwACSystemIdleTimeout, g_sysTimeouts.dwACSuspendTimeout);

    return bRet;
}

BOOL RetrieveActivityEventTimeouts_DEFAULT(void)
{
    BOOL bRet = FALSE;
    TCHAR szUserPath[MAX_PATH];
    TCHAR szSystemPath[MAX_PATH];
    HKEY hKeyUser = NULL;
    HKEY hKeySystem = NULL;
    DWORD dwRet;

    // Construct the reg key and get the timeouts
    StringCchPrintf(szUserPath, MAX_PATH, _T("%s\\%s"), PWRMGR_ACTIVITY_KEY, USER_ACTIVITY);
    StringCchPrintf(szSystemPath, MAX_PATH, _T("%s\\%s"), PWRMGR_ACTIVITY_KEY, SYSTEM_ACTIVITY);

    DWORD dwStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szUserPath, 0, 0, &hKeyUser);
    if(dwStatus != ERROR_SUCCESS)
    {
        PWRMSG(LOG_FAIL, _T("Could not open reg key %s "), szUserPath);
        goto EXIT;
    }

    dwStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szSystemPath, 0, 0, &hKeySystem);
    if(dwStatus != ERROR_SUCCESS)
    {
        PWRMSG(LOG_FAIL, _T("Could not open reg key %s "), szSystemPath);
        goto EXIT;
    }
    
    DWORD   dwValue = 0;
    DWORD   cbSize = sizeof(DWORD);

    //get UserActivityTimeout
    dwRet = RegQueryTypedValue(hKeyUser, _T("Timeout"), &dwValue, &cbSize, REG_DWORD);
    if(dwRet != ERROR_SUCCESS){
        // No timeout present, try for TimeoutMS
        dwRet = RegQueryTypedValue(hKeyUser, _T("TimeoutMS"), &dwValue, &cbSize, REG_DWORD);
        if(dwRet != ERROR_SUCCESS){
            PWRMSG(LOG_DETAIL, _T("Can not get UserActivityEvent timeout value, defaulting to 0."));
            g_sysTimeouts.dwUserActivityEventTimeout = 0;
        }
        else{                
            g_sysTimeouts.dwUserActivityEventTimeout = dwValue;
        }
    }
    else{
        g_sysTimeouts.dwUserActivityEventTimeout = dwValue * 1000; // Timeout is in seconds
    }

    dwRet = RegQueryTypedValue(hKeySystem, _T("Timeout"), &dwValue, &cbSize, REG_DWORD);
    if(dwRet != ERROR_SUCCESS){
        // No timeout present, try for TimeoutMS
        dwRet = RegQueryTypedValue(hKeySystem, _T("TimeoutMS"), &dwValue, &cbSize, REG_DWORD);
        if(dwRet != ERROR_SUCCESS){
            PWRMSG(LOG_DETAIL, _T("Can not get SystemActivityEvent timeout value, defaulting to 0."));
            g_sysTimeouts.dwSystemActivityEventTimeout = 0;
        }
        else{                
            g_sysTimeouts.dwSystemActivityEventTimeout = dwValue;
        }
    }
    else{
        g_sysTimeouts.dwSystemActivityEventTimeout = dwValue * 1000; // Timeout is in seconds
    }
    

    bRet = TRUE;

EXIT:    

    if (hKeyUser)
    {
        RegCloseKey(hKeyUser);
    }
    if (hKeySystem)
    {
        RegCloseKey(hKeySystem);
    }

    PWRMSG(LOG_DETAIL, _T("UserActivityEventTimeout=%d, SystemActivityEventTimeout=%d"), 
        g_sysTimeouts.dwUserActivityEventTimeout, g_sysTimeouts.dwSystemActivityEventTimeout);

    return bRet;
}



BOOL SetUpTimeouts_DEFAULT(DWORD dwACUserIdle, DWORD dwACSystemIdle, DWORD dwACSuspend)
{
    BOOL bRet = FALSE;
    TCHAR szPath[MAX_PATH];
    HKEY hKey;

    // Construct the reg key and set the timeouts
    StringCchPrintf(szPath, MAX_PATH, _T("%s\\%s"), PWRMGR_REG_KEY, _T("Timeouts"));

    DWORD dwStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szPath, 0, 0, &hKey);
    if(dwStatus != ERROR_SUCCESS)
    {
        PWRMSG(LOG_FAIL, _T("Could not open reg key %s "), szPath);
        goto EXIT;
    }

    // Set ACUserIdle timeout
    if(ERROR_SUCCESS != RegSetValueEx(hKey, _T("ACUserIdle"), 0, REG_DWORD,(LPBYTE)&dwACUserIdle, sizeof(DWORD)))
    {
        PWRMSG(LOG_FAIL, _T("Failed to set ACUserIdle value."));
        goto EXIT;
    }

    // Set ACSystemIdle timeout
    if(ERROR_SUCCESS != RegSetValueEx(hKey, _T("ACSystemIdle"), 0, REG_DWORD,(LPBYTE)&dwACSystemIdle, sizeof(DWORD)))
    {
        PWRMSG(LOG_FAIL, _T("Failed to set ACSystemIdle value."));
        goto EXIT;
    }

    // Set ACSuspend timeout
    if(ERROR_SUCCESS != RegSetValueEx(hKey, _T("ACSuspend"), 0, REG_DWORD,(LPBYTE)&dwACSuspend, sizeof(DWORD)))
    {
        PWRMSG(LOG_FAIL, _T("Failed to set ACSuspend value."));
        goto EXIT;
    }

    SimulateReloadTimeoutsEvent();

    bRet = TRUE;

EXIT:    

    if (hKey)
    {
        RegCloseKey(hKey);
    }

    return bRet;
}


