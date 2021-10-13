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

#include "main.h"
#include "globals.h"
#include <pm.h>
#include <pmpolicy.h>
#include <nkintr.h>

////////////////////////////////////////////////////////////////////////////////
/***************  Constants and Defines Local to This File  *******************/

TCHAR* g_PdaPowerStates[] = {_T("On"),
                             _T("BacklightOff"),
                             _T("UserIdle"),
                             _T("ScreenOff"),
                             _T("Reboot"),
                             _T("ColdReboot"),
                             _T("Off")};

const DWORD g_PdaPowerStateFlags[] = {POWER_STATE_ON,
                                      POWER_STATE_IDLE,
                                      POWER_STATE_USERIDLE,
                                      POWER_STATE_RESET,
                                      POWER_STATE_OFF,
                                      POWER_STATE_CRITICAL};

#define NUM_POWER_STATES 7


////////////////////////////////////////////////////////////////////////////////

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

TESTPROCAPI UsageMessage(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    PWRMSG(LOG_DETAIL, _T("Use this test suite to test Power Manager APIs."));
    PWRMSG(LOG_DETAIL, _T("")); //Blank line
    PWRMSG(LOG_DETAIL, _T("The tests take the following optional command line arguments:"));
    PWRMSG(LOG_DETAIL, _T("1. PdaVersion - specify this option(-c\"PdaVersion\") to let the tests know this is a PDA type device."));

    return TPR_PASS;
}

////////////////////////////////////////////////////////////////////////////////
// Tst_SystemPowerStateList
//  Checks if all named states in the power manager registry can be set and if
//  SetSystemPowerState and GetSystemPowerState work correctly.
//
// Parameters:
//  uMsg           Message code.
//  tpParam      Additional message-dependent data.
//  lpFTE          Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Tst_SystemPowerStateList(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwStateCount = 0;
    CSystemPowerStates sps;
    PPOWER_STATE pps = NULL;
    DWORD dwTestRepeatedArray[NUM_POWER_STATES] = {0};

    SET_PWR_TESTNAME(_T("Tst_SystemPowerStateList"));

    // Check if PDA tests are enabled
    if(g_bPdaVersion == FALSE)
    {
        PWRMSG(LOG_SKIP,_T("This test will skip as it targets the PDA power model and that command line option was not set."));
        PWRMSG(LOG_DETAIL,_T("Specify this option (-c\"PdaVersion\") to allow this test to execute."));
        return GetTestResult();
    }   

    // Continue with the test
    PWRMSG(LOG_DETAIL, _T("This test will get the list of supported power states from"));
    PWRMSG(LOG_DETAIL, _T("the registry and verifies they are the expected ones."));
    PWRMSG(LOG_DETAIL, _T(""));

    // Iterate through each of the supported power states and verify the expected ones
    for(dwStateCount = 0; dwStateCount < sps.GetPowerStateCount(); dwStateCount++)
        {
            // Get the power state
            pps = sps.GetPowerState(dwStateCount);

            PWRMSG(LOG_DETAIL, _T("Got System Power State '%s'"), pps->szStateName);

            if(NULL == pps)
            {
                PWRMSG(LOG_DETAIL,_T("Could not get Power State information."));
                PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
                goto CleanAndReturn;
            }

            // Check if this power state matches any of the expected ones
            // iterate though the array and compare. Stop when you find it
            DWORD dwIndex = 0; BOOL found = FALSE;
            for(dwIndex = 0; dwIndex < NUM_POWER_STATES; dwIndex++)
                {
                    // Compare state names.
                    if(0 == _tcsnicmp(pps->szStateName, g_PdaPowerStates[dwIndex],
                        sizeof(g_PdaPowerStates[dwIndex])))
                        {
                            // Found a match
                            found = TRUE;
                            break;
                        }
                }

            if(!found)
            {
                PWRMSG(LOG_DETAIL,_T("This is an unexpected power state."));
                PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
                continue;
            }

            // Power state found, check if this state has been repeated before.
            if(dwTestRepeatedArray[dwIndex])
            {
                // Has been repeated
                PWRMSG(LOG_DETAIL,_T("This power state has been repeated. There are two instances of the same power state."));
                PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
                continue;
            }
            else
            {
                // Unique power state
                dwTestRepeatedArray[dwIndex] = 1;
            }

          }

    // Done getting the power states, veriy # of power states from the registry match NUM_POWER_STATES.
    if(dwStateCount != NUM_POWER_STATES)
    {
        PWRMSG(LOG_DETAIL, _T(""));
        PWRMSG(LOG_DETAIL,_T("The number of power states in the registry do not match expected power states."));
        PWRMSG(LOG_DETAIL,_T("Expected %d power states and found %d."), NUM_POWER_STATES, dwStateCount);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

CleanAndReturn:
    return GetTestResult();
}


TESTPROCAPI Tst_SetSystemPowerStates(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwIndex = 0;
    DWORD dwErr = 0;

    TCHAR szGetPowerState[MAX_PATH];
    DWORD dwGetFlags;

    SET_PWR_TESTNAME(_T("Tst_SetSystemPowerStates"));

    // Check if PDA tests are enabled
    if(g_bPdaVersion == FALSE)
    {
        PWRMSG(LOG_SKIP,_T("This test will skip as it targets the PDA power model and that command line option was not set."));
        PWRMSG(LOG_DETAIL,_T("Specify this option (-c\"PdaVersion\") to allow this test to execute."));
        return GetTestResult();
    }   

    // Continue with the test
    PWRMSG(LOG_DETAIL, _T("This test will set the system to various power states and verify the states are set."));

    // We will try different variations for setting the System Power State, with Power State Names,
    // with Power State Flags and with the POWER_FORCE flag.

    PWRMSG(LOG_DETAIL, _T("-----------------------------------------------------------------"));
    //Set with Power State Name
    for(dwIndex = 0; dwIndex < NUM_POWER_STATES; dwIndex++)
    {
        // Compare state names.
        if((0 == _tcsnicmp(g_PdaPowerStates[1], g_PdaPowerStates[dwIndex], sizeof(g_PdaPowerStates[dwIndex]))) ||
           (0 == _tcsnicmp(g_PdaPowerStates[4], g_PdaPowerStates[dwIndex], sizeof(g_PdaPowerStates[dwIndex]))) ||
           (0 == _tcsnicmp(g_PdaPowerStates[5], g_PdaPowerStates[dwIndex], sizeof(g_PdaPowerStates[dwIndex]))) ||
           (0 == _tcsnicmp(g_PdaPowerStates[6], g_PdaPowerStates[dwIndex], sizeof(g_PdaPowerStates[dwIndex]))) )
        {
            // Matches a state not settable using the API or matches a state that resets/powers down the device
            // In this case, ignore and continue.
            PWRMSG(LOG_DETAIL, _T("We will skip power state %s."), g_PdaPowerStates[dwIndex]);
            continue;
        }

        // Set the power state
        PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(%s, 0, 0)"), g_PdaPowerStates[dwIndex]);
        if(ERROR_SUCCESS !=  SetSystemPowerState(g_PdaPowerStates[dwIndex], 0, 0))
        {
            dwErr = GetLastError();
            PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }

        // Now Get Power State to check if the Power State was indeed set
        if(ERROR_SUCCESS !=  GetSystemPowerState(szGetPowerState, _countof(szGetPowerState), &dwGetFlags))
        {
            PWRMSG(LOG_DETAIL,_T("GetSystemPowerState() returned FALSE."));
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
        else
        {
            PWRMSG(LOG_DETAIL, _T("System Power state is '%s', State Flag: 0x%08x\n"), szGetPowerState, dwGetFlags);

            // Add testing to verify that this is the same power state that was set
            if((0 == _tcsnicmp(szGetPowerState, g_PdaPowerStates[dwIndex], sizeof(g_PdaPowerStates[dwIndex]))))
            {
                PWRMSG(LOG_DETAIL, _T("System Power state obtained matches the Power State that was set."));
            }
            else
            {
                PWRMSG(LOG_DETAIL, _T("The Power state obtained does not match the Power State that was set."));
                PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
            }
        }
    }
    PWRMSG(LOG_DETAIL, _T("-----------------------------------------------------------------"));
    //Set with Power State Name and POWER_FORCE flag
    for(dwIndex = 0; dwIndex < NUM_POWER_STATES; dwIndex++)
    {
        // Compare state names.
        if((0 == _tcsnicmp(g_PdaPowerStates[1], g_PdaPowerStates[dwIndex], sizeof(g_PdaPowerStates[dwIndex]))) ||
           (0 == _tcsnicmp(g_PdaPowerStates[4], g_PdaPowerStates[dwIndex], sizeof(g_PdaPowerStates[dwIndex]))) ||
           (0 == _tcsnicmp(g_PdaPowerStates[5], g_PdaPowerStates[dwIndex], sizeof(g_PdaPowerStates[dwIndex]))) ||
           (0 == _tcsnicmp(g_PdaPowerStates[6], g_PdaPowerStates[dwIndex], sizeof(g_PdaPowerStates[dwIndex]))) )
        {
            // Matches a state not settable using the API or matches a state that
            // resets/powers down the device
            // In this case, ignore and continue.
            PWRMSG(LOG_DETAIL, _T("We will skip power state %s."), g_PdaPowerStates[dwIndex]);
            continue;
        }

        // Set the power state
        PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(%s, 0, POWER_FORCE)"), g_PdaPowerStates[dwIndex]);
        if(ERROR_SUCCESS !=  SetSystemPowerState(g_PdaPowerStates[dwIndex], 0, POWER_FORCE))
        {
            dwErr = GetLastError();
            PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
        // Now Get Power State to check if the Power State was indeed set
        if(ERROR_SUCCESS !=  GetSystemPowerState(szGetPowerState, _countof(szGetPowerState), &dwGetFlags))
        {
            PWRMSG(LOG_DETAIL,_T("GetSystemPowerState() returned FALSE."));
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
        else
        {
            PWRMSG(LOG_DETAIL, _T("System Power state is '%s', State Flag: 0x%08x\n"), szGetPowerState, dwGetFlags);

            // Add testing to verify that this is the same power state that was set
            if((0 == _tcsnicmp(szGetPowerState, g_PdaPowerStates[dwIndex], sizeof(g_PdaPowerStates[dwIndex]))))
            {
                PWRMSG(LOG_DETAIL, _T("System Power state obtained matches the Power State that was set."));
            }
            else
            {
                PWRMSG(LOG_DETAIL, _T("The Power state obtained does not match the Power State that was set."));
                PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
            }
        }
    }
    PWRMSG(LOG_DETAIL, _T("-----------------------------------------------------------------"));

    //Set with Power State Flag
    for(dwIndex = 0; dwIndex < _countof(g_PdaPowerStateFlags); dwIndex++)
    {
        // Compare state flags.
        if((g_PdaPowerStateFlags[dwIndex] & (POWER_STATE_RESET |POWER_STATE_OFF | POWER_STATE_CRITICAL )) != 0)
        {
            // Matches a state not settable using the API or matches a state that
            // resets/powers down the device
            // In this case, ignore and continue.
            PWRMSG(LOG_DETAIL, _T("We will skip this power state flag since it will reset the device - 0x%08x."), g_PdaPowerStateFlags[dwIndex]);
            continue;
        }

        // Set the power state
        PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(NULL, 0x%08x, 0)"), g_PdaPowerStateFlags[dwIndex]);
        if(ERROR_SUCCESS !=  SetSystemPowerState(NULL, g_PdaPowerStateFlags[dwIndex], 0))
        {
            dwErr = GetLastError();
            PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }

        // Now Get Power State to check if the Power State was indeed set
        if(ERROR_SUCCESS !=  GetSystemPowerState(szGetPowerState, _countof(szGetPowerState), &dwGetFlags))
        {
            PWRMSG(LOG_DETAIL,_T("GetSystemPowerState() returned FALSE."));
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
        else
        {
            PWRMSG(LOG_DETAIL, _T("System Power state is '%s', State Flag: 0x%08x"), szGetPowerState, dwGetFlags);

            // Add testing to verify that this is the same power state that was set.
            // Mask off the BacklightOn and Password flags. These are attributes of some of the system power states.
            if(g_PdaPowerStateFlags[dwIndex] == (dwGetFlags & ~(POWER_STATE_BACKLIGHTON | POWER_STATE_PASSWORD)))
            {
                PWRMSG(LOG_DETAIL, _T("System Power state obtained matches the Power State that was set."));
            }
            else
            {
                PWRMSG(LOG_DETAIL, _T("The Power state obtained does not match the Power State that was set."));
                PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
            }
        }
    }

    PWRMSG(LOG_DETAIL, _T("-----------------------------------------------------------------"));

    //Set with Power State Flag and POWER_FORCE
    for(dwIndex = 0; dwIndex < _countof(g_PdaPowerStateFlags); dwIndex++)
    {
        // Compare state flags.
        if((g_PdaPowerStateFlags[dwIndex] == POWER_STATE_RESET) ||
           (g_PdaPowerStateFlags[dwIndex] == POWER_STATE_OFF) ||
           (g_PdaPowerStateFlags[dwIndex] == POWER_STATE_CRITICAL))
        {
            // Matches a state not settable using the API or matches a state that resets/powers down the device
            // In this case, ignore and continue.
            PWRMSG(LOG_DETAIL, _T("We will skip this power state flag since it will reset the device - 0x%08x."), 
                                   g_PdaPowerStateFlags[dwIndex]);
            continue;
        }

        // Set the power state
        PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(NULL, 0x%08x, POWER_FORCE)"),
                              g_PdaPowerStateFlags[dwIndex]);
        if(ERROR_SUCCESS !=  SetSystemPowerState(NULL, g_PdaPowerStateFlags[dwIndex], POWER_FORCE))
        {
            dwErr = GetLastError();
            PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }

        // Now Get Power State to check if the Power State was indeed set
        if(ERROR_SUCCESS !=  GetSystemPowerState(szGetPowerState, _countof(szGetPowerState), &dwGetFlags))
        {
            PWRMSG(LOG_DETAIL,_T("GetSystemPowerState() returned FALSE."));
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
        else
        {
            PWRMSG(LOG_DETAIL, _T("System Power state is '%s', State Flag: 0x%08x"),
                                   szGetPowerState, dwGetFlags);

            // Add testing to verify that this is the same power state that was set
            if(g_PdaPowerStateFlags[dwIndex] == (dwGetFlags & ~(POWER_STATE_BACKLIGHTON | POWER_STATE_PASSWORD)))
            {
                PWRMSG(LOG_DETAIL, _T("System Power state obtained matches the Power State that was set."));
            }
            else
            {
                PWRMSG(LOG_DETAIL, _T("The Power state obtained does not match the Power State that was set."));
                PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
            }
        }
    }

    return GetTestResult();
}


TESTPROCAPI Tst_SetSystemPowerStateReboot(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwErr = 0;
    DWORD dwVariation = lpFTE->dwUserData;

    SET_PWR_TESTNAME(_T("Tst_SetSystemPowerStateReboot"));

    PWRMSG(LOG_DETAIL, _T("This is a semi-automated test. Test will set the system to reboot. Please verify that the device reboots."));

    // Set system power state to Reboot
    if(dwVariation == 1)
    {
     // Set using State Name
     PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(Reboot, 0, 0)"));
        if(ERROR_SUCCESS !=  SetSystemPowerState(_T("Reboot"), 0, 0))
        {
            dwErr = GetLastError();
            PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
    }
    else if(dwVariation == 2)
    {
        // Set using State Name with POWER_FORCE flag
        PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(Reboot, 0, POWER_FORCE)"));
        if(ERROR_SUCCESS !=  SetSystemPowerState(_T("Reboot"), 0, POWER_FORCE))
        {
            dwErr = GetLastError();
            PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
    }

    // If we got here, we did not reboot. If so, fail.
    PWRMSG(LOG_FAIL,_T("The device was supposed to reboot, but it didn't. Test failed."));

    return GetTestResult();
}


TESTPROCAPI Tst_SetSystemPowerStateColdReboot(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwErr = 0;
    DWORD dwVariation = lpFTE->dwUserData;

    SET_PWR_TESTNAME(_T("Tst_SetSystemPowerStateColdReboot"));

    PWRMSG(LOG_DETAIL, _T("This is a semi-automated test. Test will set the system to reboot. Please verify that the device reboots."));

    // Set system power state to ColdReboot
    if(dwVariation == 1)
    {
     // Set using State Name
     PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(ColdReboot, 0, 0)"));
        if(ERROR_SUCCESS !=  SetSystemPowerState(_T("ColdReboot"), 0, 0))
        {
            dwErr = GetLastError();
            PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
    }
    else if(dwVariation == 2)
    {
        // Set using State Name with POWER_FORCE flag
        PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(ColdReboot, 0, POWER_FORCE)"));
        if(ERROR_SUCCESS !=  SetSystemPowerState(_T("ColdReboot"), 0, POWER_FORCE))
        {
            dwErr = GetLastError();
            PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
    }

    // If we got here, we did not reboot. If so, fail.
    PWRMSG(LOG_FAIL,_T("The device was supposed to reboot, but it didn't. Test failed."));

    return GetTestResult();
}


TESTPROCAPI Tst_SetSystemPowerStateOff(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwErr = 0;
    DWORD dwVariation = lpFTE->dwUserData;

    SET_PWR_TESTNAME(_T("Tst_SetSystemPowerStateOff"));

    // Check if PDA tests are enabled
    if(g_bPdaVersion == FALSE)
    {
        PWRMSG(LOG_SKIP,_T("This test will skip as it targets the PDA power model and that command line option was not set."));
        PWRMSG(LOG_DETAIL,_T("Specify this option (-c\"PdaVersion\") to allow this test to execute."));
        return GetTestResult();
    }   

    // Continue with the test
    PWRMSG(LOG_DETAIL, _T("This is a semi-automated test. Test will turn off the system. Please verify that the device powers down."));

    // Set system power state to Off
    if(dwVariation == 1)
    {
        // Set using State Name
        PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(Off, 0, 0)"));
        if(ERROR_SUCCESS !=  SetSystemPowerState(_T("Off"), 0, 0))
        {
            dwErr = GetLastError();
            PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
    }
    else if(dwVariation == 2)
    {
        // Set using State Name with POWER_FORCE flag
        PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(Off, 0, POWER_FORCE)"));
        if(ERROR_SUCCESS !=  SetSystemPowerState(_T("Off"), 0, POWER_FORCE))
        {
            dwErr = GetLastError();
            PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
    }

    // If we got here, we did not power off. If so, fail.
    PWRMSG(LOG_FAIL,_T("The device was supposed to power off, but it didn't. Test failed."));

    return GetTestResult();
}


TESTPROCAPI Tst_SetSystemPowerStateHintReset(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwErr = 0;
    DWORD dwVariation = lpFTE->dwUserData;

    SET_PWR_TESTNAME(_T("Tst_SetSystemPowerStateHintReset"));

    PWRMSG(LOG_DETAIL, _T("This is a semi-automated test. Test will rest the system. Please verify that the device reboots."));

    // Set system power state to Reset
    if(dwVariation == 1)
    {
        // Set using State Flag
        PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(NULL, POWER_STATE_RESET, 0)"));
        if(ERROR_SUCCESS !=  SetSystemPowerState(NULL, POWER_STATE_RESET, 0))
        {
            dwErr = GetLastError();
            PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
    }
    else if(dwVariation == 2)
    {
        // Set using State Flag with POWER_FORCE flag
        PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(NULL, POWER_STATE_RESET, POWER_FORCE)"));
        if(ERROR_SUCCESS !=  SetSystemPowerState(NULL, POWER_STATE_RESET, POWER_FORCE))
        {
            dwErr = GetLastError();
            PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
    }

    // If we got here, we did not reset. If so, fail.
    PWRMSG(LOG_FAIL,_T("The device was supposed to reboot, but it didn't. Test failed."));

    return GetTestResult();
}


TESTPROCAPI Tst_SetSystemPowerStateHintOff(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwErr = 0;
    DWORD dwVariation = lpFTE->dwUserData;

    SET_PWR_TESTNAME(_T("Tst_SetSystemPowerStateHintOff"));

    PWRMSG(LOG_DETAIL, _T("This is a semi-automated test. Test will turn off the system. Please verify that the device powers down."));

    // Set system power state to Reset
    if(dwVariation == 1)
    {
        // Set using State Flag
        PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(NULL, POWER_STATE_OFF, 0)"));
        if(ERROR_SUCCESS !=  SetSystemPowerState(NULL, POWER_STATE_OFF, 0))
        {
            dwErr = GetLastError();
            PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
    }
    else if(dwVariation == 2)
    {
        // Set using State Flag with POWER_FORCE flag
        PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(NULL, POWER_STATE_OFF, POWER_FORCE)"));
        if(ERROR_SUCCESS !=  SetSystemPowerState(NULL, POWER_STATE_OFF, POWER_FORCE))
        {
            dwErr = GetLastError();
            PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
    }

    // If we got here, we did not power off. If so, fail.
    PWRMSG(LOG_FAIL,_T("The device was supposed to power off, but it didn't. Test failed."));

    return GetTestResult();
}

TESTPROCAPI Tst_SetSystemPowerStateHintCritical(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwErr = 0;
    DWORD dwVariation = lpFTE->dwUserData;

    SET_PWR_TESTNAME(_T("Tst_SetSystemPowerStateHintCritical"));

    PWRMSG(LOG_DETAIL, _T("This is a semi-automated test. Test will turn off the system. Please verify that the device powers down."));

    // Set system power state to Reset
    if(dwVariation == 1)
    {
        // Set using State Flag
        PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(NULL, POWER_STATE_CRITICAL, 0)"));
        if(ERROR_SUCCESS !=  SetSystemPowerState(NULL, POWER_STATE_CRITICAL, 0))
        {
            dwErr = GetLastError();
            PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
    }
    else if(dwVariation == 2)
    {
        // Set using State Flag with POWER_FORCE flag
        PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(NULL, POWER_STATE_CRITICAL, POWER_FORCE)"));
        if(ERROR_SUCCESS !=  SetSystemPowerState(NULL, POWER_STATE_CRITICAL, POWER_FORCE))
        {
            dwErr = GetLastError();
            PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed. GetLastError = %d"), dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
    }

    // If we got here, we did not power off. If so, fail.
    PWRMSG(LOG_FAIL,_T("The device was supposed to power off, but it didn't. Test failed."));

    return GetTestResult();
}