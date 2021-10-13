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
// TUX DLL
// Copyright (c) Microsoft Corporation
//
// Module:
// Contains the test functions.
//
// Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"
#include <pm.h>
#include <pmpolicy.h>
#include <nkintr.h>
#include <wchar.h>

////////////////////////////////////////////////////////////////////////////////

#define MAX_TEST_VARIATION 4

#define GUID_STR_LEN 39


setSysPwrStateParam setSysPwrStateIncParam[] = {
    // NULL and empty StateNames, invalid StateFlags
    {NULL, 0, 0, _T("(NULL, 0, 0)")},
    {NULL, NULL, NULL, _T("(NULL, NULL, NULL)")},
    {NULL, 0xABCD, POWER_FORCE, _T("(NULL, 0xABCD, POWER_FORCE)")},
    {_T(""), POWER_NAME, 0, _T("(\"\", POWER_NAME, 0)")},
    {_T(""), 0xFFFF, POWER_FORCE, _T("(\"\", 0xFFFF, POWER_FORCE)")},

    // Invalid StateNames and StateFlags
    {_T("abcd"), 0xFFFF, 0xFFFF, _T("(\"abcd\", 0xFFFF, 0xFFFF)")},
    {_T("1234"), 0xFF, POWER_FORCE, _T("(\"1234\", 0xFF, POWER_FORCE)")},
    {_T(" "), POWER_NAME, 0, _T("(\" \", POWER_NAME, 0)")},
    {_T("*"), 0, POWER_FORCE, _T("(\"*\", 0, POWER_FORCE)")},

    // StateNames are modifications of actual StateNames
    {_T("OnState"), POWER_NAME, POWER_FORCE, _T("(\"OnState\", POWER_NAME, POWER_FORCE)")},
    {_T("Idle"), 0xFFF0, 0, _T("(\"Idle\", 0xFFF0, 0)")},
    {_T("ScreenOn"), 0xFF, POWER_FORCE, _T("(\"ScreenOn\", 0xFF, POWER_FORCE)")},
    {_T("resume"), 0, POWER_FORCE, _T("(\"resume\", 0, POWER_FORCE)")},

    // Invalid StateNames, valid StateFlags
    {_T("abcd"), POWER_STATE_ON, 0xFFFF, _T("(\"abcd\", POWER_STATE_ON, 0xFFFF)")},
    {_T("1234"), POWER_STATE_IDLE, POWER_FORCE, _T("(\"1234\", POWER_STATE_IDLE, POWER_FORCE)")},
    {_T("*"), POWER_STATE_BACKLIGHTON, 0, _T("(\"*\", POWER_STATE_BACKLIGHTON, 0)")},
};

setPwrReqParam setPwrReqIncParam[] = {
    // NULL and empty DeviceNames
    {NULL, PwrDeviceUnspecified, POWER_NAME, NULL, 0, _T("(NULL, PwrDeviceUnspecified, POWER_NAME, NULL, 0)")},
    {_T(""), D0, POWER_NAME, _T(""), 0, _T("(\"\", D0, POWER_NAME, \"\")")},
    {NULL, PwrDeviceUnspecified, 0xFFFF, POWER_STATE_NAME_ON, 0xFFFF, _T("(NULL, PwrDeviceUnspecified, 0xFFFF, POWER_STATE_NAME_ON, 0xFFFF)")},
    {NULL, D0, (POWER_NAME || POWER_FORCE), POWER_STATE_NAME_USERIDLE, 0, _T("(NULL, D0, (POWER_NAME || POWER_FORCE), POWER_STATE_NAME_USERIDLE, 0)")},
    {_T(""), D0, POWER_NAME, POWER_STATE_NAME_SCREENOFF, 0, _T("(\"\", D0, POWER_NAME, POWER_STATE_NAME_SCREENOFF, 0)")},

    // Random DeviceNames and Invalid Device Flags
    {_T("COMM1:"), D0, 0, POWER_STATE_NAME_SCREENOFF, 0, _T("(\"COMM1:\", D3, 0, POWER_STATE_NAME_SCREENOFF, 0)")},
    {_T("C OM1:"), D1, 0, NULL, 0, _T("(\"C OM1:\", D3, 0, NULL, 0)")},
    {_T("USB 1:"), D2, 0, POWER_STATE_NAME_ON, 0, _T("(\"USB 1:\", D3, 0, POWER_STATE_NAME_ON, 0)")},
    {_T("USB1: "), D4, 0, POWER_STATE_NAME_ON, 0, _T("(\"USB1: \", D3, 0, POWER_STATE_NAME_ON, 0)")},

    // Valid DeviceNames with Invalid DeviceFlags will be tested separately. See below.
};

setPwrReqParam_noDev setPwrReqIncParam_noDev[] = {
    // Valid DeviceNames and Invalid DeviceFlags
    {PwrDeviceUnspecified, 0, _T("OnState"), 0},
    {D0, 0, _T("IdleState"), 0},
    {D1, 0, NULL, 0},
    {D2, POWER_FORCE, _T(" "), 0xE},
    {D3, 0, POWER_STATE_NAME_USERIDLE, 0},
    {PwrDeviceMaximum, 0, POWER_STATE_NAME_SCREENOFF, 0xAB},
};

setDevPwrParam setDevPwrIncParam[] = {
    // NULL and empty DeviceNames, invalid DeviceFlags
    {NULL, 0, PwrDeviceUnspecified, _T("(NULL, 0, PwrDeviceUnspecified)")},
    {NULL, NULL, D0, _T("(NULL, NULL, D0)")},
    {NULL, 0xABCD, D1, _T("(NULL, 0xABCD, D1)")},
    {NULL, 0xFF, D2, _T("NULL, 0xFF, D2")},
    {_T(""), 0, D3, _T("(\"\", 0, D3)")},
    {_T(""), 0xFFFF, D4, _T("\"\", 0xFFFF, D4")},
    {_T(""), 0xFF, PwrDeviceMaximum, _T("\"\", 0xFF, PwrDeviceMaximum")},

    // Invalid DeviceNames and DeviceFlags
    {_T("abcd"), 0, D0, _T("(\"abcd\", 0, D0")},
    {_T("1234"), 0xABCD, D1, _T("(\"1234\", 0xABCD, D1)")},
    {_T(" "), 0xFF, D2, _T("(\" \", 0xFF, D2)")},
    {_T("*"), 0xFFFF, D3, _T("(\"*\", 0xFFFF, D3)")},
    {_T(""), NULL, D4, _T("(\"\", NULL, D4)")},

    // DeviceNames are slight variation of actual DeviceNames
    {_T("COMM1:"), POWER_NAME, D0, _T("(\"COMM1:\", POWER_NAME, D0)")},
    {_T("USB 1:"), POWER_NAME, D3, _T("(\"USB 1:\", POWER_NAME, D3)")},

    // Valid DeviceNames with Invalid DeviceFlags will be tested separately. See TestProc.
};

// Array of different combinations of notifications for use with RequestPowerNotif API tests
DWORD dwArrayPwrNotif[] = {PBT_POWERINFOCHANGE, PBT_POWERSTATUSCHANGE, PBT_RESUME, PBT_TRANSITION,
POWER_NOTIFY_ALL, PBT_POWERINFOCHANGE|PBT_POWERSTATUSCHANGE|PBT_RESUME|PBT_TRANSITION,
PBT_POWERINFOCHANGE|PBT_POWERSTATUSCHANGE, PBT_RESUME|PBT_TRANSITION,
PBT_POWERINFOCHANGE|PBT_RESUME, PBT_POWERSTATUSCHANGE|PBT_TRANSITION,
PBT_POWERINFOCHANGE|PBT_TRANSITION, PBT_POWERSTATUSCHANGE|PBT_RESUME};



////////////////////////////////////////////////////////////////////////////////
// Helper Functions
////////////////////////////////////////////////////////////////////////////////
// ConvertGuidToString
// Converts GUID to string.
//
// Parameters:
// pszGuid pointer to string that holds the converted string value
// cchGuid size of the string that holds the converted string value
// pGuid pointer to GUID, that needs to be converted
//
// Return value:
// TRUE if conversion is successful, FALSE otherwise.
BOOL ConvertGuidToString (__out_ecount(cchGuid) LPTSTR pszGuid, int cchGuid, const GUID *pGuid)
{
    BOOL fOk = FALSE;

    // format the class name
    fOk = SUCCEEDED(StringCchPrintf(pszGuid, cchGuid, _T("{%08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x}"),
        pGuid->Data1, pGuid->Data2, pGuid->Data3,
        (pGuid->Data4[0] << 8) + pGuid->Data4[1], pGuid->Data4[2], pGuid->Data4[3],
        pGuid->Data4[4], pGuid->Data4[5], pGuid->Data4[6], pGuid->Data4[7]));

    return fOk;
}



////////////////////////////////////////////////////////////////////////////////
// Test Procs
////////////////////////////////////////////////////////////////////////////////
// Usage Message
// This test prints out the usage message for this test suite.
//
// Parameters:
// uMsg Message code.
// tpParam Additional message-dependent data.
// lpFTE Function table entry that generated this call.
//
// Return value:
// TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
// special conditions.

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
    PWRMSG(LOG_DETAIL, _T("1. AllowSuspend - specify this option(-c\"AllowSuspend\") to allow the tests to suspend the system."));

    return TPR_PASS;
}

////////////////////////////////////////////////////////////////////////////////
// Tst_SetAndGetSystemPowerState
// Checks if all named states in the power manager registry can be set and if
// SetSystemPowerState and GetSystemPowerState work correctly.
//
// Parameters:
// uMsg Message code.
// tpParam Additional message-dependent data.
// lpFTE Function table entry that generated this call.
//
// Return value:
// TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
// special conditions.

TESTPROCAPI Tst_SetAndGetSystemPowerState(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwStateCount = 0;
    DWORD dwErr = 0;
    CSystemPowerStates sps;
    TCHAR szStateFlag[MAX_PATH];
    PPOWER_STATE pps = NULL;

    TCHAR szGetPowerState[MAX_PATH];
    DWORD dwGetFlags;
    DWORD dwForce;

    SET_PWR_TESTNAME(_T("Tst_SetAndGetSystemPowerState"));

    PWRMSG(LOG_DETAIL, _T("This test will verify the GetSystemPowerState API with an optional"));
    PWRMSG(LOG_DETAIL, _T("parameter for pFlags."));
    PWRMSG(LOG_DETAIL, _T("This test will also iterate through each of the supported power states and"));
    PWRMSG(LOG_DETAIL, _T("try to set the power state and then verify that it is set."));
    PWRMSG(LOG_DETAIL, _T("")); //Blank line
    
    // Test GetSystemPowerState with NULL flag parameter
    DWORD len = MAX_PATH;
    TCHAR lpzBuf[MAX_PATH] = {0};
    PWRMSG(LOG_DETAIL, _T("Calling GetSystemPowerState() with NULL state flag parameter."));
    if ((ERROR_SUCCESS != GetSystemPowerState(lpzBuf, len, NULL)))
    {
        PWRMSG(LOG_DETAIL,_T("Function call FAILED."));
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // We will try different variations for setting the System Power State, with Power State Names,
    // with Power State Flags and with the POWER_FORCE flag.
    for(DWORD dwVariation=1; dwVariation<=MAX_TEST_VARIATION; dwVariation++)
    {
        PWRMSG(LOG_DETAIL, _T("")); //Blank line

        if(1 == dwVariation)
            PWRMSG(LOG_DETAIL, _T("***We will call SetSystemPowerState with just the Power State Name***"));
        if(2 == dwVariation)
            PWRMSG(LOG_DETAIL, _T("***We will call SetSystemPowerState with Power State Name and POWER_FORCE flag***"));
        if(3 == dwVariation)
            PWRMSG(LOG_DETAIL, _T("***We will call SetSystemPowerState with just the Power State Flag***"));
        if(4 == dwVariation)
            PWRMSG(LOG_DETAIL, _T("***We will call SetSystemPowerState with Power State Flag and POWER_FORCE flag***"));

        // Iterate through each of the supported power states and try to set it and verify that it is set
        for(dwStateCount = 0; dwStateCount < sps.GetPowerStateCount(); dwStateCount++)
        {
            PWRMSG(LOG_DETAIL, _T("")); // Insert blank line here for readability

            // Get the power state to be set
            pps = sps.GetPowerState(dwStateCount);

            PWRMSG(LOG_DETAIL, _T("Test transition to System Power State '%s', State Flag: 0x%08x\n"), pps->szStateName, pps->dwFlags);

            if(NULL == pps)
            {
                PWRMSG(LOG_DETAIL,_T("Could not get Power State information."));
                PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
                goto CleanAndReturn;
            }

            if((0 == _tcsicmp(_T("Off"), pps->szStateName) ))
            {
                PWRMSG(LOG_DETAIL,_T("Transition to Off will power down the device. We will skip this state."));
                continue;
            }

            // Check if this Power State is Settable Using PM API
            if(!(pps->bSettable))
            {
                PWRMSG(LOG_DETAIL,_T("Power State \"%s\" cannot be set using Power Manager APIs. "), pps->szStateName);
                continue;
            }

            // Check if this is Power State Reset, if so, we will skip
            if(POWER_STATE_RESET == pps->dwFlags)
            {
                PWRMSG(LOG_DETAIL,_T("Cannot transition to state \"%s\" because it will reset the system and resets are not allowed."),
                    pps->szStateName);
                continue;
            }

            // Get the variation
            if(dwVariation % 2)
                dwForce = 0 ;
            else
                dwForce = POWER_FORCE;

            // Check if this is Power State Suspend. If so, check if suspend is allowed, else skip
            if(POWER_STATE_SUSPEND == pps->dwFlags )
            {
                if(g_bAllowSuspend)
                {

                    if(dwVariation <= 2)
                    {
                        //Set with Power State Name
                        PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(%s, 0, %d)"), pps->szStateName, dwForce);

                        if(!SuspendResumeSystem(pps->szStateName, 0, dwForce))
                        {
                            PWRMSG(LOG_DETAIL,_T("Suspend/Resume system did not work."));
                            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
                        }
                    }
                    else
                    {
                        //Set with Power State Flag
                        PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(NULL, %s, %d)"),StateFlag2String(pps->dwFlags, szStateFlag,MAX_PATH), dwForce);
                        if(!SuspendResumeSystem(NULL, pps->dwFlags, dwForce))
                        {
                            PWRMSG(LOG_DETAIL,_T("Suspend/Resume system did not work."));
                            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
                        }
                    }

                    // Now Get Power State to check if the system resumed from suspend
                    if(ERROR_SUCCESS != GetSystemPowerState(szGetPowerState, _countof(szGetPowerState), &dwGetFlags))
                    {
                        PWRMSG(LOG_DETAIL,_T("GetSystemPowerState() returned FALSE."));
                        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
                    }
                    else
                    {
                        PWRMSG(LOG_DETAIL, _T("System Power state is set to '%s', State Flag: 0x%08x\n"), szGetPowerState, dwGetFlags);

                        // Add testing to verify that this is the expected Power State, i.e., POWER_STATE_ON
                        if(0 == _tcscmp(szGetPowerState, POWER_STATE_NAME_ON))
                        {
                            PWRMSG(LOG_DETAIL, _T("System Power state obtained matches the expected Power State."));
                        }
                        else
                        {
                            PWRMSG(LOG_DETAIL, _T("The expected Power State is '%s'."), POWER_STATE_NAME_ON);
                            PWRMSG(LOG_DETAIL, _T("The Power state obtained does not match the expected Power State."));
                            PWRMSG(LOG_DETAIL, _T("Either SetSystemPowerState() or GetSystemPowerState() did not work correctly."));
                            PWRMSG(LOG_DETAIL, _T("Or it is possible that Suspend/Resume left the system in an unexpected state."));
                            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
                        }
                    }
                }
                else
                {
                    PWRMSG(LOG_DETAIL,_T("Cannot Transition to State \"%s\", this option is not allowed. "), pps->szStateName);
                    PWRMSG(LOG_DETAIL,_T("You can enable this option from the command line. See documentation for more information."));
                }
                continue;
            }

            // Finally, if we made it here, set each of the supported power states
            if(dwVariation <= 2)
            {
                //Set with Power State Name
                PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(%s, 0, %d)"), pps->szStateName, dwForce);
                if(ERROR_SUCCESS != SetSystemPowerState(pps->szStateName, 0, dwForce))
                {
                    dwErr = GetLastError();
                    PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed for \"%s\". GetLastError = %d"), pps->szStateName, dwErr);
                    PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
                }
            }
            else
            {
                //Set with Power State Flag
                PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(NULL, %s, %d)"),StateFlag2String(pps->dwFlags, szStateFlag,MAX_PATH), dwForce);
                if(ERROR_SUCCESS != SetSystemPowerState(NULL, pps->dwFlags, dwForce))
                {
                    dwErr = GetLastError();
                    PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed for \"%s\". GetLastError = %d"), pps->szStateName, dwErr);
                    PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
                }
            }

            // Now Get Power State to check if the Power States were indeed set
            if(ERROR_SUCCESS != GetSystemPowerState(szGetPowerState, _countof(szGetPowerState), &dwGetFlags))
            {
                PWRMSG(LOG_DETAIL,_T("GetSystemPowerState() returned FALSE."));
                PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
            }
            else
            {
                PWRMSG(LOG_DETAIL, _T("System Power state is '%s', State Flag: 0x%08x\n"), szGetPowerState, dwGetFlags);

                // Add testing to verify that this is the same power state that was set
                if(dwGetFlags == pps->dwFlags)
                {
                    PWRMSG(LOG_DETAIL, _T("System Power state obtained matches the Power State that was set."));
                }
                else
                {
                    PWRMSG(LOG_DETAIL, _T("The Power state obtained does not match the Power State that was set."));
                    PWRMSG(LOG_DETAIL, _T("Either SetSystemPowerState() or GetSystemPowerState() did not work correctly."));
                    PWRMSG(LOG_FAIL, _T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
                }
            }
        }
    }

CleanAndReturn:

    return GetTestResult();

}


////////////////////////////////////////////////////////////////////////////////
// Tst_SetAndReleasePowerReq
// Test SetPowerRequirement and ReleasePowerRequirement APIs
//
// Parameters:
// uMsg Message code.
// tpParam Additional message-dependent data.
// lpFTE Function table entry that generated this call.
//
// Return value:
// TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
// special conditions.

TESTPROCAPI Tst_SetAndReleasePowerReq(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwErr = ERROR_SUCCESS;
    DWORD dwDevice = 0;
    HANDLE hPwrReq = NULL;
    CEDEVICE_POWER_STATE dx = D0;
    WIN32_FIND_DATA fd = {0};

    SET_PWR_TESTNAME(_T("Tst_SetAndReleasePowerReq"));

    PWRMSG(LOG_DETAIL, _T("This test will iterate through all of the stream devices and"));
    PWRMSG(LOG_DETAIL, _T("try to set and then release power requirements for each of them."));
    PWRMSG(LOG_DETAIL,_T("")); //Blank line

    // Make sure the System is in On State
    if(ERROR_SUCCESS != SetSystemPowerState(POWER_STATE_NAME_ON, 0, POWER_FORCE))
    {
        PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() with POWER_FORCE failed for \"%s\" state. GetLastError = %d"),
            POWER_STATE_NAME_ON, GetLastError());
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed"),__FILE__,__LINE__,pszPwrTest);
        goto CleanAndReturn;
    }

    // Enumerate all stream devices
    for(dwDevice = 0; ; dwDevice++)
    {
        if(!GetDeviceByIndex(dwDevice, &fd))
        {
            PWRMSG(LOG_DETAIL,_T("GetDeviceByIndex: No more devices, last index = %u"), dwDevice);
            break;
        }

        // For devices that did not register fully (sometimes happens with PCI devices),
        // we could get an empty filename. Skip testing these devices because it makes
        // no sense
        if(!fd.cFileName[0])
        {
            continue;
        }

        PWRMSG(LOG_DETAIL,_T("Testing for device: %s"), fd.cFileName);

        // Test each device with each power requirement - Set and Release the requirement
        for(dx = D0; dx < PwrDeviceMaximum; dx = (CEDEVICE_POWER_STATE)((UINT)dx + 1))
        {
            hPwrReq = SetPowerRequirement(fd.cFileName, dx, POWER_NAME, NULL, 0);
            if(NULL == hPwrReq)
            {
                PWRMSG(LOG_DETAIL,_T("SetPowerRequirement(%s, D%u, POWER_NAME, NULL, 0) failed. GetLastError = %d"),
                    fd.cFileName, dx, GetLastError());
                PWRMSG(LOG_FAIL,_T("Fail in %s at line %d : %s Failed."),__FILE__,__LINE__,pszPwrTest);
            }

            if(ERROR_SUCCESS != (dwErr = ReleasePowerRequirement(hPwrReq)))
            {
                PWRMSG(LOG_DETAIL,_T("ReleasePowerRequirement() failed. Function returned error code = %d"), dwErr);
                PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed."),__FILE__,__LINE__,pszPwrTest);
            }
        }
    }

    // Make sure there was at least one stream device enumerated
    if(0 == dwDevice)
    {
        PWRMSG(LOG_SKIP,_T("No devices were available for testing; test skipped."));
    }

CleanAndReturn:

    return GetTestResult();

}


////////////////////////////////////////////////////////////////////////////////
// Tst_SetAndGetDevicePower
// Tests the SetDevicePower and GetDevicePower APIs
//
// Parameters:
// uMsg Message code.
// tpParam Additional message-dependent data.
// lpFTE Function table entry that generated this call.
//
// Return value:
// TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
// special conditions.

TESTPROCAPI Tst_SetAndGetDevicePower(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    CPowerManagedDevices pPMDevices;
    PPMANAGED_DEVICE_STATE pDeviceState;
    CEDEVICE_POWER_STATE setDx = D0, getDx = PwrDeviceUnspecified;
    DWORD dwErr;

    SET_PWR_TESTNAME(_T("Tst_SetAndGetDevicePower"));

    PWRMSG(LOG_DETAIL,_T("")); //Blank line
    PWRMSG(LOG_DETAIL, _T("This test will iterate through all of the power managed devices and"));
    PWRMSG(LOG_DETAIL, _T("try to set and then get the device power for each of them."));

    // Iterate through all the Power Managed Devices - Set Power and Get Power
    while((pDeviceState=pPMDevices.GetNextPMDevice())!=NULL)
    {
        PWRMSG(LOG_DETAIL,_T("")); //Blank line
        PWRMSG(LOG_DETAIL,_T("Testing for device: %s"), pDeviceState->LegacyName);

        TCHAR pszClassGuid[GUID_STR_LEN];

        if(!ConvertGuidToString(pszClassGuid, _countof(pszClassGuid), &pDeviceState->guid))
        {
            PWRMSG(LOG_DETAIL,_T("ConvertGuidToString for the device failed."));
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d: %s Failed."),_T(__FILE__),__LINE__,pszPwrTest);
        }

        TCHAR pszIdDevice[MAX_PATH];
        if(!SUCCEEDED(StringCchPrintf(pszIdDevice, _countof(pszIdDevice), _T("%s\\%s"), pszClassGuid, pDeviceState->LegacyName)))
        {
            PWRMSG(LOG_DETAIL,_T("Failed to create a class-qualified device name prefixed with GUID for use with Power Manager APIs."));
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d: %s Failed."),_T(__FILE__),__LINE__,pszPwrTest);
        }

        PWRMSG(LOG_DETAIL,_T("We will use class-qualified device name with Power Manager APIs: %s"), pszIdDevice);

        for(setDx = D0; setDx < PwrDeviceMaximum; setDx = (CEDEVICE_POWER_STATE)((UINT)setDx + 1))
        {
            PWRMSG(LOG_DETAIL, _T("Setting device %s to Power State D%d"),pDeviceState->LegacyName, setDx);

            // First call SetDevicePower
            if(ERROR_SUCCESS != (dwErr = SetDevicePower(pszIdDevice, POWER_NAME, setDx)) )
            {
                PWRMSG(LOG_DETAIL,_T("SetDevicePower(%s, POWER_NAME, D%u) failed. Function returned error code = %d"),
                    pszIdDevice, setDx, dwErr);
                PWRMSG(LOG_FAIL,_T("Fail in %s at line %d: %s Failed."),_T(__FILE__),__LINE__,pszPwrTest);
            }

            // Now call GetDevicePower
            if ( ERROR_SUCCESS != (dwErr = GetDevicePower(pszIdDevice, POWER_NAME, &getDx) ))
            {
                PWRMSG(LOG_DETAIL,_T("GetDevicePower(%s, POWER_NAME, xxx) failed. Function returned error code = %d"),
                    pszIdDevice, dwErr);
                PWRMSG(LOG_FAIL,_T("Fail in %s at line %d: %s Failed."),_T(__FILE__),__LINE__,pszPwrTest);
            }
            else
            {
                PWRMSG(LOG_DETAIL, _T("Device %s is currently at Power State D%d"),pDeviceState->LegacyName, getDx);
            }

            // Now compare and see if we got what we set
            // This will not work because the SetDevicePower will not always set the given device power
            // It depends on the device power capabilities
        }
    }

    return GetTestResult();

}


////////////////////////////////////////////////////////////////////////////////
// Tst_DevicePowerNotify
// Tests DevicePowerNotify API
//
// Parameters:
// uMsg Message code.
// tpParam Additional message-dependent data.
// lpFTE Function table entry that generated this call.
//
// Return value:
// TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
// special conditions.

TESTPROCAPI Tst_DevicePowerNotify(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{

    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    CPowerManagedDevices pPMDevices;
    PPMANAGED_DEVICE_STATE pDeviceState;
    CEDEVICE_POWER_STATE dx = D0;
    DWORD dwErr;

    SET_PWR_TESTNAME(_T("Tst_DevicePowerNotify"));

    PWRMSG(LOG_DETAIL,_T("")); //Blank line
    PWRMSG(LOG_DETAIL, _T("This test will iterate through all of the power managed devices and"));
    PWRMSG(LOG_DETAIL, _T("try to set the device power by calling DevicePowerNotify()."));


    // Iterate through all the Power Managed Devices - Try to set device power state by calling DevicePowerNotify
    while((pDeviceState=pPMDevices.GetNextPMDevice())!=NULL)
    {

        PWRMSG(LOG_DETAIL,_T("")); //Blank line
        PWRMSG(LOG_DETAIL,_T("Testing for device: %s"), pDeviceState->LegacyName); //Blank line

        TCHAR pszClassGuid[GUID_STR_LEN];

        if(!ConvertGuidToString(pszClassGuid, _countof(pszClassGuid), &pDeviceState->guid))
        {
            PWRMSG(LOG_DETAIL,_T("ConvertGuidToString for the device failed."));
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d: %s Failed."),_T(__FILE__),__LINE__,pszPwrTest);
        }

        TCHAR pszIdDevice[MAX_PATH];
        if(!SUCCEEDED(StringCchPrintf(pszIdDevice, _countof(pszIdDevice), _T("%s\\%s"), pszClassGuid, pDeviceState->LegacyName)))
        {
            PWRMSG(LOG_DETAIL,_T("Failed to create a class-qualified device name prefixed with GUID for use with Power Manager APIs."));
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d: %s Failed."),_T(__FILE__),__LINE__,pszPwrTest);
        }

        PWRMSG(LOG_DETAIL,_T("We will use the following class-qualified device name with the Power Manager APIs: %s"), pszIdDevice);

        for (dx = D0; dx < PwrDeviceMaximum; dx = (CEDEVICE_POWER_STATE)((UINT)dx + 1))
        {
            PWRMSG(LOG_DETAIL, _T("Calling DevicePowerNotify for Device %s with Device Power D%u"), pDeviceState->LegacyName, dx);

            if( ERROR_SUCCESS != (dwErr = DevicePowerNotify(pszIdDevice, dx, POWER_NAME)) )
            {
                PWRMSG(LOG_DETAIL,_T("DevicePowerNotify(%s, D%u, POWER_NAME) failed. Function returned error code = %d"),
                    pszIdDevice, dx, dwErr);
                PWRMSG(LOG_FAIL,_T("Fail in %s at line %d: %s Failed."),_T(__FILE__),__LINE__,pszPwrTest);
            }
        }
    }

    return GetTestResult();

}


////////////////////////////////////////////////////////////////////////////////
// Tst_PowerNotification
// Checks to receive power notification
//
// Parameters:
// uMsg Message code.
// tpParam Additional message-dependent data.
// lpFTE Function table entry that generated this call.
//
// Return value:
// TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
// special conditions.

TESTPROCAPI Tst_RequestPowerNotif(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{

    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    HANDLE hMsgQ = NULL;
    HANDLE hNotif = NULL;
    DWORD dwErr;

    MSGQUEUEOPTIONS msgOpt = {0};
    msgOpt.dwSize = sizeof(MSGQUEUEOPTIONS);
    msgOpt.dwFlags = 0;
    msgOpt.cbMaxMessage = QUEUE_SIZE;
    msgOpt.bReadAccess = TRUE;

    SET_PWR_TESTNAME(_T("Tst_RequestPowerNotif"));

    PWRMSG(LOG_DETAIL, _T("This test will create a Message Queue and Request Power Notifications."));

    // Make sure the System is in On State
    if(ERROR_SUCCESS != SetSystemPowerState(POWER_STATE_NAME_ON, POWER_NAME, POWER_FORCE))
    {
        PWRMSG(LOG_DETAIL,_T("Could not set the system to 'On' state."));
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        goto CleanAndReturn;
    }

    PWRMSG(LOG_DETAIL,_T("Creating message queue for power notifications"));
    hMsgQ = CreateMsgQueue(NULL, &msgOpt);
    dwErr = GetLastError();
    if(NULL == hMsgQ)
    {
        PWRMSG(LOG_DETAIL,_T("CreateMsgQueue() failed. GetLastError() is %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        goto CleanAndReturn;
    }

    // Call RequestPowerNotif with each of the notifications in dwArrayPwrNotif
    PWRMSG(LOG_DETAIL,_T("Requesting power notifications."));

    for(DWORD dw=0; dw < _countof(dwArrayPwrNotif); dw++)
    {
        hNotif = RequestPowerNotifications(hMsgQ, dwArrayPwrNotif[dw]);
        dwErr = GetLastError();
        if(NULL == hNotif)
        {
            PWRMSG(LOG_DETAIL,_T("Call to RequestPowerNotifications() Failed when called with Notification Flags 0x%08x. GetLastError() is %d"),
                dwArrayPwrNotif[dw], dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
    }

CleanAndReturn:

    if(hNotif)
    {
        PWRMSG(LOG_DETAIL,_T("Stopping power notifications."));
        if(ERROR_SUCCESS != StopPowerNotifications(hNotif))
        {
            dwErr = GetLastError();
            PWRMSG(LOG_DETAIL,_T("Call to StopPowerNotifications() Failed. GetLastError() is %d"), dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
    }

    if(hMsgQ)
    {
        PWRMSG(LOG_DETAIL,_T("Closing power notifications message queue."));
        if(!CloseMsgQueue(hMsgQ))
        {
            dwErr = GetLastError();
            PWRMSG(LOG_DETAIL,_T("Call to CloseMsgQueue() Failed. GetLastError() is %d"), dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
    }

    return GetTestResult();

}


////////////////////////////////////////////////////////////////////////////////
// Tst_PowerPolicy
// Check the PowerPolicyNotify API
//
// Parameters:
// uMsg Message code.
// tpParam Additional message-dependent data.
// lpFTE Function table entry that generated this call.
//
// Return value:
// TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
// special conditions.

TESTPROCAPI Tst_PowerPolicyNotify(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{

    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    SET_PWR_TESTNAME(_T("Tst_PowerPolicyNotify"));

    PWRMSG(LOG_DETAIL, _T("This test will call PowerPolicyNotify with various PPN_XXX messages."));

    // Make sure the System is in On State
    if(ERROR_SUCCESS != SetSystemPowerState(POWER_STATE_NAME_ON, POWER_NAME, POWER_FORCE))
    {
        PWRMSG(LOG_DETAIL,_T("Could not set the system to 'On' state."));
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        goto CleanAndReturn;
    }

    PWRMSG(LOG_DETAIL,_T("Calling PowerPolicyNotify(PPN_APPBUTTONPRESSED, 0)"));
    if(!PowerPolicyNotify(PPN_APPBUTTONPRESSED, 0))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s PowerPolicyNotify() Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T("Calling PowerPolicyNotify(PPN_POWERCHANGE, 0)"));
    if(!PowerPolicyNotify(PPN_POWERCHANGE, 0))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s PowerPolicyNotify() Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T("Calling PowerPolicyNotify(PPN_REEVALUATESTATE, 0)"));
    if(!PowerPolicyNotify(PPN_REEVALUATESTATE, 0))
    {
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s PowerPolicyNotify() Failed "),__FILE__,__LINE__,pszPwrTest);
    }

CleanAndReturn:

    return GetTestResult();

}


////////////////////////////////////////////////////////////////////////////////
// Tst_SetSystemPowerStateFI
// Test SetSystemPowerState API with Incorrect Parameters
//
// Parameters:
// uMsg Message code.
// tpParam Additional message-dependent data.
// lpFTE Function table entry that generated this call.
//
// Return value:
// TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
// special conditions.

TESTPROCAPI Tst_SetSysPwrStateIncParam(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwErr;

    SET_PWR_TESTNAME(_T("Tst_SetSysPwrStateIncParam"));

    PWRMSG(LOG_DETAIL, _T("This test will call SetSystemPowerState() with various incorrect parameters and"));
    PWRMSG(LOG_DETAIL, _T("will verify that the API call will FAIL."));
    PWRMSG(LOG_DETAIL,_T("")); //Blank line

    // Try all variations listed in the setSysPwrStateIncParam struct
    for(DWORD dwCount = 0; dwCount < _countof(setSysPwrStateIncParam); dwCount++)
    {
        PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState with %s"), setSysPwrStateIncParam[dwCount].szComment);

        dwErr = SetSystemPowerState(setSysPwrStateIncParam[dwCount].pszStateName, setSysPwrStateIncParam[dwCount].dwStateFlag,
            setSysPwrStateIncParam[dwCount].dwOptionFlag);
        if(ERROR_SUCCESS == dwErr)
        {
            PWRMSG(LOG_DETAIL,_T("The function returned SUCCESS while it was expected to FAIL."));
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed."),__FILE__,__LINE__,pszPwrTest);
            PWRMSG(LOG_DETAIL,_T("")); //Blank line
        }
    }

    PWRMSG(LOG_DETAIL,_T("Done testing."));

    return GetTestResult();

}


////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Tst_SetPowerReqFI
// Test SetPowerRequirement API with incorrect parameters
//
// Parameters:
// uMsg Message code.
// tpParam Additional message-dependent data.
// lpFTE Function table entry that generated this call.
//
// Return value:
// TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
// special conditions.

TESTPROCAPI Tst_SetPwrReqIncParam(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD device = 0;
    HANDLE hPwrReq = ERROR_SUCCESS;
    WIN32_FIND_DATA fd = {0};
    DWORD dwErr;

    SET_PWR_TESTNAME(_T("Tst_SetPwrReqIncParam"));

    PWRMSG(LOG_DETAIL, _T("This test will call SetPowerRequirement() with various incorrect parameters and"));
    PWRMSG(LOG_DETAIL, _T("will verify that the API call will FAIL."));
    PWRMSG(LOG_DETAIL,_T("")); //Blank line

    // Try all variations listed in the setPwrReqIncParam struct
    for(DWORD dwCount = 0; dwCount < _countof(setPwrReqIncParam); dwCount++)
    {
        PWRMSG(LOG_DETAIL,_T("Calling SetPowerRequirement with %s"), setPwrReqIncParam[dwCount].szComment);

        hPwrReq = SetPowerRequirement(setPwrReqIncParam[dwCount].pvDevName, setPwrReqIncParam[dwCount].devState,
            setPwrReqIncParam[dwCount].ulDevFlag, setPwrReqIncParam[dwCount].pvStateName, setPwrReqIncParam[dwCount].ulStateFlag);

        dwErr = GetLastError();
        if(NULL != hPwrReq)
        {
            PWRMSG(LOG_DETAIL,_T("The function was expected to return a NULL handle, but it returned 0x%08x. GetLastError = %d"),
                hPwrReq, dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed."),__FILE__,__LINE__,pszPwrTest);
        }
    }

    PWRMSG(LOG_DETAIL,_T("")); //Blank line
    PWRMSG(LOG_DETAIL,_T("Now we will iterate through all stream devices and test."));

    // Now enumerate all stream devices and test
    for(device = 0; ; device++)
    {
        if(!GetDeviceByIndex(device, &fd))
        {
            PWRMSG(LOG_DETAIL,_T("GetDeviceByIndex: No more devices, last index = %u"), device);
            break;
        }

        // For devices that did not register fully (sometimes happens with PCI devices),
        // we could get an empty filename. Skip testing these devices because it makes no sense
        if(!fd.cFileName[0])
        {
            continue;
        }

        PWRMSG(LOG_DETAIL,_T("")); //Blank line
        PWRMSG(LOG_DETAIL,_T("Testing for device: %s"), fd.cFileName);

        // Test each device with incorrect parameters
        for(DWORD dwCount = 0; dwCount < _countof(setPwrReqIncParam_noDev); dwCount++)
        {
            PWRMSG(LOG_DETAIL,_T("Calling SetPowerRequirement with (%s, D%d, 0x%08x, %s, 0x%08x)"), fd.cFileName,
                setPwrReqIncParam_noDev[dwCount].devState, setPwrReqIncParam_noDev[dwCount].ulDevFlag,
                setPwrReqIncParam_noDev[dwCount].pvStateName, setPwrReqIncParam_noDev[dwCount].ulStateFlag);

            hPwrReq = SetPowerRequirement(fd.cFileName, setPwrReqIncParam_noDev[dwCount].devState,
                setPwrReqIncParam_noDev[dwCount].ulDevFlag, setPwrReqIncParam_noDev[dwCount].pvStateName,
                setPwrReqIncParam_noDev[dwCount].ulStateFlag);

            if(NULL != hPwrReq)
            {
                PWRMSG(LOG_DETAIL,_T("The function was expected to return a NULL handle, but it returned 0x%08x\n"), hPwrReq);
                PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed."),__FILE__,__LINE__,pszPwrTest);
            }
        }
    }

    PWRMSG(LOG_DETAIL,_T("Done testing."));

    return GetTestResult();

}


////////////////////////////////////////////////////////////////////////////////
// Tst_SetDevPwrIncParam
// Tests SetDevicePower API with Incorrect Parameters
//
// Parameters:
// uMsg Message code.
// tpParam Additional message-dependent data.
// lpFTE Function table entry that generated this call.
//
// Return value:
// TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
// special conditions.

TESTPROCAPI Tst_SetDevPwrIncParam(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    CPowerManagedDevices pPMDevices;
    PPMANAGED_DEVICE_STATE pDeviceState;
    DWORD dwErr;

    SET_PWR_TESTNAME(_T("Tst_SetDevPwrIncParam"));

    PWRMSG(LOG_DETAIL,_T("")); //Blank line
    PWRMSG(LOG_DETAIL, _T("This test will call SetDevicePower() with various incorrect parameters and"));
    PWRMSG(LOG_DETAIL, _T("will verify that the API call will FAIL."));
    PWRMSG(LOG_DETAIL,_T("")); //Blank line

    // Try all variations listed in the setDevPwrIncParam struct
    for(DWORD dwCount = 0; dwCount < _countof(setDevPwrIncParam); dwCount++)
    {
        PWRMSG(LOG_DETAIL,_T("Calling SetDevicePower with %s"), setDevPwrIncParam[dwCount].szComment);

        dwErr = SetDevicePower(setDevPwrIncParam[dwCount].pvDevName, setDevPwrIncParam[dwCount].dwDevFlag,
            setDevPwrIncParam[dwCount].devState);
        if(ERROR_SUCCESS == dwErr)
        {
            PWRMSG(LOG_DETAIL,_T("The function returned SUCCESS while it was expected to FAIL."));
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed."),__FILE__,__LINE__,pszPwrTest);
        }
    }

    PWRMSG(LOG_DETAIL,_T("")); //Blank line
    PWRMSG(LOG_DETAIL,_T("Now we will iterate through all power managed devices and test."));

    // Iterate through all the Power Managed Devices
    while((pDeviceState=pPMDevices.GetNextPMDevice())!=NULL)
    {
        PWRMSG(LOG_DETAIL,_T("")); //Blank line
        PWRMSG(LOG_DETAIL,_T("Testing for device: %s"), pDeviceState->LegacyName);

        PWRMSG(LOG_DETAIL,_T("Calling SetDevicePower with (%s, 0xFFFE, D0)"), pDeviceState->LegacyName);
        dwErr = SetDevicePower(pDeviceState->LegacyName, 0xFFFE, D0);
        if(ERROR_SUCCESS == dwErr)
        {
            PWRMSG(LOG_DETAIL,_T("The function returned SUCCESS while it was expected to FAIL"));
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed."),__FILE__,__LINE__,pszPwrTest);
        }

        PWRMSG(LOG_DETAIL,_T("Calling SetDevicePower with (%s, 0, D4)"), pDeviceState->LegacyName);
        dwErr = SetDevicePower(pDeviceState->LegacyName, 0, D4);
        if(ERROR_SUCCESS == dwErr)
        {
            PWRMSG(LOG_DETAIL,_T("The function returned SUCCESS while it was expected to FAIL"));
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed."),__FILE__,__LINE__,pszPwrTest);
        }
    }

    PWRMSG(LOG_DETAIL,_T("Done testing."));

    return GetTestResult();

}


////////////////////////////////////////////////////////////////////////////////
// Tst_DevicePowerNotifyIncParam
// Tests DevicePowerNotify API with Incorrect parameters
//
// Parameters:
// uMsg Message code.
// tpParam Additional message-dependent data.
// lpFTE Function table entry that generated this call.
//
// Return value:
// TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
// special conditions.

TESTPROCAPI Tst_DevPwrNotifyIncParam(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    CPowerManagedDevices pPMDevices;
    PPMANAGED_DEVICE_STATE pDeviceState;
    DWORD dwErr;

    SET_PWR_TESTNAME(_T("Tst_DevicePowerNotifyIncParam"));

    PWRMSG(LOG_DETAIL,_T("")); //Blank line
    PWRMSG(LOG_DETAIL, _T("This test will call DevicePowerNotify() with various incorrect parameters and"));
    PWRMSG(LOG_DETAIL, _T("will verify that the API call will FAIL."));
    PWRMSG(LOG_DETAIL,_T("")); //Blank line


    // Try all variations listed in the setDevPwrIncParam struct
    for(DWORD dwCount = 0; dwCount < _countof(setDevPwrIncParam); dwCount++)
    {
        PWRMSG(LOG_DETAIL,_T("Calling DevicePowerNotify with (%s, D%u, 0x%08x)"),
            setDevPwrIncParam[dwCount].pvDevName, setDevPwrIncParam[dwCount].devState, setDevPwrIncParam[dwCount].dwDevFlag);

        dwErr = DevicePowerNotify(setDevPwrIncParam[dwCount].pvDevName, setDevPwrIncParam[dwCount].devState,
            setDevPwrIncParam[dwCount].dwDevFlag);
        if(ERROR_SUCCESS == dwErr)
        {
            PWRMSG(LOG_DETAIL,_T("The function returned SUCCESS while it was expected to FAIL."));
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed."),__FILE__,__LINE__,pszPwrTest);
        }
    }

    PWRMSG(LOG_DETAIL,_T("")); //Blank line
    PWRMSG(LOG_DETAIL,_T("Now we will iterate through all power managed devices and test."));

    // Iterate through all the Power Managed Devices
    while((pDeviceState=pPMDevices.GetNextPMDevice())!=NULL)
    {
        PWRMSG(LOG_DETAIL,_T("")); //Blank line
        PWRMSG(LOG_DETAIL,_T("Testing for device: %s"), pDeviceState->LegacyName);

        PWRMSG(LOG_DETAIL,_T("Calling DevicePowerNotify with (%s, D0, 0xFFFE)"), pDeviceState->LegacyName);
        dwErr = DevicePowerNotify(pDeviceState->LegacyName, D0, 0xFFFE);
        if(ERROR_SUCCESS == dwErr)
        {
            PWRMSG(LOG_DETAIL,_T("The function returned SUCCESS while it was expected to FAIL"));
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed."),__FILE__,__LINE__,pszPwrTest);
        }

        PWRMSG(LOG_DETAIL,_T("Calling DevicePowerNotify with (%s, D4, 0)"), pDeviceState->LegacyName);
        dwErr = DevicePowerNotify(pDeviceState->LegacyName, D4, 0);
        if(ERROR_SUCCESS == dwErr)
        {
            PWRMSG(LOG_DETAIL,_T("The function returned SUCCESS while it was expected to FAIL"));
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed."),__FILE__,__LINE__,pszPwrTest);
        }
    }

    PWRMSG(LOG_DETAIL,_T("Done testing."));

    return GetTestResult();

}


////////////////////////////////////////////////////////////////////////////////
// Tst_RequestPowerNotifIncParam
// Tests RequestPowerNotification API with Incorrect parameters
//
// Parameters:
// uMsg Message code.
// tpParam Additional message-dependent data.
// lpFTE Function table entry that generated this call.
//
// Return value:
// TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
// special conditions.

TESTPROCAPI Tst_RequestPowerNotifIncParam(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    HANDLE hNotif;
    HANDLE hMsgQ;

    MSGQUEUEOPTIONS msgOpt = {0};
    msgOpt.dwSize = sizeof(MSGQUEUEOPTIONS);
    msgOpt.dwFlags = 0;
    msgOpt.cbMaxMessage = QUEUE_SIZE;
    msgOpt.bReadAccess = TRUE;

    SET_PWR_TESTNAME(_T("Tst_RequestPowerNotifIncParam"));

    PWRMSG(LOG_DETAIL, _T("This test will call RequestPowerNotifications() with various incorrect parameters and"));
    PWRMSG(LOG_DETAIL, _T("will verify that the API call will FAIL."));
    PWRMSG(LOG_DETAIL,_T("")); //Blank line


    // Try all notification combinations listed in the dwArrayPwrNotif array with an Invalid MsgQueue handles
    PWRMSG(LOG_DETAIL,_T("Requesting various power notifications with an Invalid MsgQueue handle."));

    for(DWORD dw=0; dw < _countof(dwArrayPwrNotif); dw++)
    {
        hNotif = RequestPowerNotifications(NULL, dwArrayPwrNotif[dw]);
        if(NULL != hNotif)
        {
            PWRMSG(LOG_DETAIL,_T("Call to RequestPowerNotifications(NULL, 0x%08x) succeeded while it was expected to fail."), dwArrayPwrNotif[dw]);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }

        hNotif = RequestPowerNotifications(INVALID_HANDLE_VALUE, dwArrayPwrNotif[dw]);
        if(NULL != hNotif)
        {
            PWRMSG(LOG_DETAIL,_T("Call to RequestPowerNotifications(INVALID_HANDLE_VALUE, 0x%08x) succeeded while it was expected to fail."),
                dwArrayPwrNotif[dw]);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
    }

    // Try with invalid notifications
    PWRMSG(LOG_DETAIL,_T("Requesting invalid notifications with invalid MsgQueue handles."));
    hNotif = RequestPowerNotifications(NULL, 0);
    if(NULL != hNotif)
    {
        PWRMSG(LOG_DETAIL,_T("Call to RequestPowerNotifications(NULL, 0) succeeded while it was expected to fail."));
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }
    hNotif = RequestPowerNotifications(INVALID_HANDLE_VALUE, 0);
    if(NULL != hNotif)
    {
        PWRMSG(LOG_DETAIL,_T("Call to RequestPowerNotifications(INVALID_HANDLE_VALUE, 0) succeeded while it was expected to fail."));
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Try with invalid notifications and valid MsgQueue handle
    PWRMSG(LOG_DETAIL,_T("Creating message queue for power notifications"));
    hMsgQ = CreateMsgQueue(NULL, &msgOpt);
    DWORD dwErr = GetLastError();
    if(NULL == hMsgQ)
    {
        PWRMSG(LOG_DETAIL,_T("CreateMsgQueue() failed. GetLastError() is %d"), dwErr);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        goto CleanAndReturn;
    }

    PWRMSG(LOG_DETAIL,_T("Requesting invalid notifications with valid MsgQueue handles."));
    hNotif = RequestPowerNotifications(hMsgQ, 0);
    if(NULL != hNotif)
    {
        PWRMSG(LOG_DETAIL,_T("Call to RequestPowerNotifications(0x%08x, 0) succeeded while it was expected to fail."), hMsgQ);
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

    // Test StopPowerNotifications
    PWRMSG(LOG_DETAIL,_T("Calling StopPowerNotifications with an invalid MsgQueue handle."));
    if(ERROR_SUCCESS == StopPowerNotifications(NULL))
    {
        PWRMSG(LOG_DETAIL,_T("Call to StopPowerNotifications(NULL) succeeded while it was expected to fail."));
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }
    if(ERROR_SUCCESS == StopPowerNotifications(INVALID_HANDLE_VALUE))
    {
        PWRMSG(LOG_DETAIL,_T("Call to StopPowerNotifications(INVALID_HANDLE_VALUE) succeeded while it was expected to fail."));
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }

CleanAndReturn:
    if(hNotif)
    {
        PWRMSG(LOG_DETAIL,_T("Stopping power notifications."));
        if(ERROR_SUCCESS != StopPowerNotifications(hNotif))
        {
            dwErr = GetLastError();
            PWRMSG(LOG_DETAIL,_T("Call to StopPowerNotifications() Failed. GetLastError() is %d"), dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
    }

    if(hMsgQ)
    {
        PWRMSG(LOG_DETAIL,_T("Closing power notifications message queue."));
        if(!CloseMsgQueue(hMsgQ))
        {
            dwErr = GetLastError();
            PWRMSG(LOG_DETAIL,_T("Call to CloseMsgQueue() Failed. GetLastError() is %d"), dwErr);
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
    }

    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Tst_PowerPolicyNotifyIncParam
// Check the PowerPolicyNotify API with Incorrect parameters
//
// Parameters:
// uMsg Message code.
// tpParam Additional message-dependent data.
// lpFTE Function table entry that generated this call.
//
// Return value:
// TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
// special conditions.

TESTPROCAPI Tst_PowerPolicyNotifyIncParam(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{

    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    SET_PWR_TESTNAME(_T("Tst_PowerPolicyNotifyIncParam"));

    PWRMSG(LOG_DETAIL, _T("This test will call PowerPolicyNotify() with various incorrect parameters and"));
    PWRMSG(LOG_DETAIL, _T("will verify that the API call will FAIL."));
    PWRMSG(LOG_DETAIL,_T("")); //Blank line

    // Make sure the System is in On State
    if(ERROR_SUCCESS != SetSystemPowerState(POWER_STATE_NAME_ON, POWER_NAME, POWER_FORCE))
    {
        PWRMSG(LOG_DETAIL,_T("Could not set the system to 'On' state."));
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        goto CleanAndReturn;
    }

    PWRMSG(LOG_DETAIL,_T("Calling PowerPolicyNotify(0,0)"));
    if(!PowerPolicyNotify(0,0))
    {
        PWRMSG(LOG_FAIL,_T("Function call was expected to FAIL but it succeeded. Fail in %s at line %d :%s Failed."),
            __FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T("Calling PowerPolicyNotify(0,0xFFFF)"));
    if(!PowerPolicyNotify(0,0xFFFF))
    {
        PWRMSG(LOG_FAIL,_T("Function call was expected to FAIL but it succeeded. Fail in %s at line %d :%s Failed."),
            __FILE__,__LINE__,pszPwrTest);
    }

    PWRMSG(LOG_DETAIL,_T("Calling PowerPolicyNotify(0,0x07)"));
    if(!PowerPolicyNotify(0,0x07))
    {
        PWRMSG(LOG_FAIL,_T("Function call was expected to FAIL but it succeeded. Fail in %s at line %d :%s Failed."),
            __FILE__,__LINE__,pszPwrTest);
    }


CleanAndReturn:

    return GetTestResult();

}


////////////////////////////////////////////////////////////////////////////////
// Tst_GetSystemPowerStateIncParam
// Check the GetSystemPowerState API with Incorrect parameters
//
// Parameters:
// uMsg Message code.
// tpParam Additional message-dependent data.
// lpFTE Function table entry that generated this call.
//
// Return value:
// TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
// special conditions.

TESTPROCAPI Tst_GetSystemPowerStateIncParam(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    SET_PWR_TESTNAME(_T("Tst_GetSystemPowerStateIncParam"));

    PWRMSG(LOG_DETAIL, _T("This test will call GetSystemPowerState() with various incorrect parameters and"));
    PWRMSG(LOG_DETAIL, _T("will verify that the API call will FAIL."));
    PWRMSG(LOG_DETAIL,_T("")); //Blank line

    TCHAR lpzBuf[MAX_PATH] = {0};
    TCHAR StateChar = 0;
    DWORD len = MAX_PATH;
    DWORD flag = 0;

    // Test with 0 length parameter
    PWRMSG(LOG_DETAIL, _T("Calling GetSystemPowerState() with valid buffer and zero buffer length value."));
    if ((ERROR_INSUFFICIENT_BUFFER != GetSystemPowerState(lpzBuf, 0, &flag)))
    {
        PWRMSG(LOG_DETAIL,_T("Function call was expected to FAIL with ERROR_INSUFFICIENT_BUFFER."));
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }
    else
    {
        // Verify the buffer was not modified
        TCHAR temp[MAX_PATH] = {0};
        if(0 != wmemcmp(lpzBuf, temp, MAX_PATH))
        {
            PWRMSG(LOG_DETAIL,_T("ERROR! The buffer was modified."));
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
    }

    // Test with buffer length of 1 wchar 
    len = 1;
    PWRMSG(LOG_DETAIL, _T("Calling GetSystemPowerState() with 1 WCHAR buffer and length."));
    if ((ERROR_INSUFFICIENT_BUFFER != GetSystemPowerState(&StateChar, len, &flag)))
    {
        PWRMSG(LOG_DETAIL,_T("Function call was expected to FAIL with ERROR_INSUFFICIENT_BUFFER."));
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }
    else
    {
        // Verify the buffer was not modified
        TCHAR temp = 0;
        if(0 != wmemcmp(&StateChar, &temp, 1))
        {
            PWRMSG(LOG_DETAIL,_T("ERROR! The buffer was modified."));
            PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        }
    }

    // Test with NULL buffer
    len = MAX_PATH;
    PWRMSG(LOG_DETAIL, _T("Calling GetSystemPowerState() with NULL buffer parameter and valid length."));
    if ((ERROR_SUCCESS == GetSystemPowerState(NULL, len, &flag)))
    {
        PWRMSG(LOG_DETAIL,_T("Function call was expected to FAIL but it succeeded."));
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
    }
    
    return GetTestResult();
}