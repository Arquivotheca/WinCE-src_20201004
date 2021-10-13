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
//  TUXTEST TUX DLL
//  Copyright (c) Microsoft Corporation
//
//  Module: test.cpp
//          Contains the test functions.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"
#include <pm.h>
#include <msgqueue.h>

// Number of variations to call the SetSystemPowerState() in the State transition test
#define MAX_TEST_VARIATION 4


////////////////////////////////////////////////////////////////////////////////
// InitPowerMsg
//  Initializes Message Queue and Requests Power Notification be sent.
//
// Parameters:
//  hMsgQ        Handle to Msg Queue
//  hNotif       Handle to RequestPowerNotification
//
// Return value:
//  TRUE if initialization successful, FALSE otherwise.
////////////////////////////////////////////////////////////////////////////////
BOOL  InitPowerMsg(HANDLE* hMsgQ, HANDLE* hNotif)
{
    MSGQUEUEOPTIONS msgOpt = {0};
    DWORD dwErr;

    msgOpt.dwSize = sizeof(MSGQUEUEOPTIONS);
    msgOpt.dwFlags = 0;
    msgOpt.cbMaxMessage = QUEUE_SIZE;
    msgOpt.bReadAccess = TRUE;

    PWRMSG(LOG_DETAIL,_T("Creating message queue for power notifications"));
    *hMsgQ = CreateMsgQueue(NULL, &msgOpt);
    dwErr = GetLastError();
    if(NULL == *hMsgQ)
    {
        PWRMSG(LOG_FAIL,
            _T("Fail in %s at line %d : CreateMsgQueue() Failed. GetLastError = %d "),
            _T(__FILE__),__LINE__, dwErr);
        return FALSE;
    }

    PWRMSG(LOG_DETAIL,_T("Requesting power notifications"));
    *hNotif = RequestPowerNotifications(*hMsgQ, POWER_NOTIFY_ALL);
    dwErr = GetLastError();
    if(NULL == hNotif)
    {
        PWRMSG(LOG_FAIL,
            _T("Fail in %s at line %d : RequestPowerNotifications() Failed. GetLastError = %d "),
            _T(__FILE__),__LINE__, dwErr);
        return FALSE;
    }

    return TRUE ;
}


////////////////////////////////////////////////////////////////////////////////
// ClearMsgQ
//  Clears the Notification Queue
//
// Parameters:
//  hMsgQ        Handle to Msg Queue that needs clean up
//
// Return value:
//  TRUE if successful, FALSE otherwise.
////////////////////////////////////////////////////////////////////////////////
BOOL ClearMsgQ(HANDLE hMsgQ)
{
    BYTE buffer[QUEUE_MAX_MSG_SIZE];
    DWORD msgBytes, msgFlags;

    memset(buffer, 0, sizeof(buffer));
    while(ReadMsgQueue(hMsgQ, buffer, sizeof(buffer), &msgBytes, PWR_MSG_WAIT_TIME, &msgFlags))
    {
        PWRMSG(LOG_DETAIL,_T("Cleared extraneous message from the queue"));
        memset(buffer, 0, sizeof(buffer));
    }
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////
// VerifyPowerMsg
//  Verifies that we receive timely and expected notifications of the state changes
//
// Parameters:
//  hMsgQ              Handle to the Notification Message Queue
//  dwReqMsg           Type of the Notification Message that was requested
//  szReqStateName     Power State Name that the system was requested to transition to
//  dwReqFlags         Power State Flag that the system was requested to transition to
//
// Return value:
//  TRUE if successful, FALSE otherwise
////////////////////////////////////////////////////////////////////////////////
BOOL VerifyPowerMsg(HANDLE hMsgQ,DWORD dwReqMsg, const TCHAR* szReqStateName,DWORD dwReqFlags)
{
    BYTE buffer[QUEUE_MAX_MSG_SIZE];
    PPOWER_BCAST_MSG pPBMActual = NULL;
    DWORD msgBytes, msgFlags;

    PWRMSG(LOG_DETAIL,_T("Waiting for power notification..."));
    if(!ReadMsgQueue(hMsgQ, buffer, sizeof(buffer), &msgBytes, PWR_MSG_WAIT_TIME, &msgFlags))
    {
        PWRMSG(LOG_FAIL,_T("ReadMsgQueue() failed. Fail in %s at line %d."),
            _T(__FILE__),__LINE__);
        return FALSE;
    }

    pPBMActual = (PPOWER_BCAST_MSG)buffer;
    LogPowerMessage((PPOWER_BROADCAST)pPBMActual);

    PWRMSG(LOG_DETAIL, _T("Compare if the message received was of the requested type"));
    if(pPBMActual->Message != dwReqMsg)
    {
        // Message mismatch
        PWRMSG(LOG_FAIL, _T("Requested Message type %d and obtained %d in the Notification message."),
            dwReqMsg, pPBMActual->Message);
        return FALSE;
    }

    //For Status Change we need to verify only the message type
    if(dwReqMsg == PBT_POWERSTATUSCHANGE)
    {
        return TRUE;
    }

    PWRMSG(LOG_DETAIL, _T("Compare that we received a message that included correct information"));
    if(szReqStateName)
    {
        if(0 != _tcsncicmp(szReqStateName, pPBMActual->SystemPowerState, QUEUE_MAX_NAMELEN))
        {
            // State name mismatch
            PWRMSG(LOG_FAIL,_T("Requested state name \"%s\" and obtained \"%s\" in the Notification message."),
                szReqStateName, pPBMActual->SystemPowerState);
            return  FALSE;
        }
    }

    if(dwReqFlags != pPBMActual->Flags)
    {
        // State flags mismatch
        PWRMSG(LOG_FAIL,_T("Requested state flags 0x%08x and obtained 0x%08x in the Notification message."),
            dwReqFlags, pPBMActual->Flags);
        return FALSE;
    }

    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////
// VerifySuspendResumeMsg
//  Verify that we receive timely and expected notifications for state changes to Suspend and Resume
//
// Parameters:
//  hMsgQ        Handle to the Notification Message Queue
//
// Return value:
//  TRUE if successful, FALSE otherwise
////////////////////////////////////////////////////////////////////////////////
BOOL VerifySuspendResumeMsg(HANDLE hMsgQ)
{
    // Verify message for transition to Suspend state
    if(!VerifyPowerMsg(hMsgQ,PBT_TRANSITION,POWER_STATE_NAME_SUSPEND,POWER_STATE_SUSPEND))
    {
        PWRMSG(LOG_FAIL,_T("VerifySuspendResumeMsg() failed. Fail in %s at line %d."),_T(__FILE__),__LINE__);
        return FALSE;
    }

    // Verify message for transition to Resume state
    if(!VerifyPowerMsg(hMsgQ,PBT_TRANSITION, POWER_STATE_NAME_RESUMING, 0))
    {
        PWRMSG(LOG_FAIL,_T("VerifySuspendResumeMsg() failed. Fail in %s at line %d."),_T(__FILE__),__LINE__);
        return FALSE;
    }

    // Verify Resume state notification message
    if(!VerifyPowerMsg(hMsgQ,PBT_RESUME, 0, 0))
    {
        PWRMSG(LOG_FAIL,_T("VerifySuspendResumeMsg() failed. Fail in %s at line %d."),_T(__FILE__),__LINE__);
        return FALSE;
    }

    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////
// Usage Message
//  This test prints out the usage message for this test suite.
//
// Parameters:
//  uMsg         Message code.
//  tpParam      Additional message-dependent data.
//  lpFTE        Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI UsageMessage(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    PWRMSG(LOG_DETAIL, _T("Use this test suite to test Power Manager Notification messages."));
    PWRMSG(LOG_DETAIL, _T("")); //Blank line
    PWRMSG(LOG_DETAIL, _T("The tests take the following optional command line arguments:"));
    PWRMSG(LOG_DETAIL, _T("1. AllowSuspend - specify this option(-c\"AllowSuspend\") to allow the tests to suspend the system."));
    PWRMSG(LOG_DETAIL, _T("2. PdaVersion - specify this option(-c\"PdaVersion\") to let the tests know this is a PDA type device."));

    return TPR_PASS;
}


////////////////////////////////////////////////////////////////////////////////
// Tst_StateTransitionMsg
//  Test for verifying power messages during state transitions
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_SysPowerStateTransitionMsg(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD  dwStateCount;
    DWORD dwErr = 0;

    CSystemPowerStates sps;
    PPOWER_STATE pps = NULL;

    TCHAR szStateFlag[MAX_PATH];
    TCHAR szCurPowerState[MAX_PATH];
    DWORD dwCurFlags;
    DWORD dwForce;

    HANDLE hMsgQ = NULL;
    HANDLE hNotif = NULL;

    SET_PWR_TESTNAME(_T("Tst_SetAndGetSystemPowerState"));

    PWRMSG(LOG_DETAIL, _T("This test will iterate through the supported power states and try to transition to"));
    PWRMSG(LOG_DETAIL, _T("different power states and then verify that an appropriate notification is sent."));
    PWRMSG(LOG_DETAIL, _T("")); //Blank line

    // Initialize Message Queue and Request power Notification
    if(!InitPowerMsg(&hMsgQ,&hNotif))
    {
        PWRMSG(LOG_FAIL,_T("InitPowerMsg()  Failed. Fail in %s at line %d :%s"),_T(__FILE__),__LINE__,pszPwrTest);
    }

    // We will try different variations for setting the System Power State, with Power State Names,
    // with Power State Flags and with the POWER_FORCE flag.
    for(DWORD dwVariation=1; dwVariation<=MAX_TEST_VARIATION; dwVariation++)
    {
        PWRMSG(LOG_DETAIL, _T("")); //Blank line

        if(1 == dwVariation)
            PWRMSG(LOG_DETAIL, _T("We will call SetSystemPowerState with just the Power State Name."));
        if(2 == dwVariation)
            PWRMSG(LOG_DETAIL, _T("We will call SetSystemPowerState with Power State Name and POWER_FORCE flag."));
        if(3 == dwVariation)
            PWRMSG(LOG_DETAIL, _T("We will call SetSystemPowerState with just the Power State Flag."));
        if(4 == dwVariation)
            PWRMSG(LOG_DETAIL, _T("We will call SetSystemPowerState with Power State Flag and POWER_FORCE flag."));

        // Iterate through each of the supported power states and try to set it and verify that it is set
        for(dwStateCount = 0; dwStateCount < sps.GetPowerStateCount(); dwStateCount++)
        {
            // Get the power state to be set
            pps = sps.GetPowerState(dwStateCount);

            PWRMSG(LOG_DETAIL, _T("")); //Blank line
            PWRMSG(LOG_DETAIL, _T("Test transition to System Power State '%s', State Flag: 0x%08x\n"),
                pps->szStateName, pps->dwFlags);

            if(NULL == pps)
            {
                PWRMSG(LOG_DETAIL,_T("Could not get Power State information."));
                PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
                goto CleanAndReturn;
            }

            if((0 == _tcsicmp(_T("Off"), pps->szStateName) ))
            {
                PWRMSG(LOG_DETAIL,_T("Transition to Off will power down the device. We will skip this state."), pps->szStateName);
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

            // Suspend /Resume power message notification handled in a seperate test
            if(POWER_STATE_SUSPEND == pps->dwFlags )
            {
                PWRMSG(LOG_DETAIL,_T("Power State \"%s\" will be handled in a separate test. "), pps->szStateName);
                continue;
            }

            // Clear the notification queue
            if(!ClearMsgQ(hMsgQ))
            {
                PWRMSG(LOG_FAIL,_T("ClearMsgQ()  Failed. Fail in %s at line %d :%s"),_T(__FILE__),__LINE__,pszPwrTest);
                goto CleanAndReturn;
            }

            //Check if the Current Power State is same as the one we are planning to set
            if(ERROR_SUCCESS != GetSystemPowerState(szCurPowerState, MAX_PATH, &dwCurFlags))
            {
                PWRMSG(LOG_DETAIL,_T("GetSystemPowerState() returned FALSE."));
                PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
                goto CleanAndReturn;
            }

            if((0 == _tcsicmp(szCurPowerState, pps->szStateName) ))
            {
                PWRMSG(LOG_DETAIL,_T("Current State is \"%s\", transition to which does not generate notification. We will skip."), pps->szStateName);
                continue;
            }

            // Get the variation
            if(dwVariation % 2)
                dwForce = 0 ;
            else
                dwForce = POWER_FORCE;

            // Finally, if we made it here, set the power state
            if(dwVariation <= 2)
            {
                //Set with Power State Name
                PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(%s, 0, %d)"), pps->szStateName, dwForce);
                if(ERROR_SUCCESS !=  SetSystemPowerState(pps->szStateName, 0, dwForce))
                {
                    dwErr = GetLastError();
                    PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed for \"%s\". GetLastError = %d"),
                        pps->szStateName, dwErr);
                    PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
                    continue;
                }
            }
            else
            {
                //Set with Power State Flag
                PWRMSG(LOG_DETAIL,_T("Calling SetSystemPowerState(NULL, %s, %d)"),
                    StateFlag2String(pps->dwFlags, szStateFlag,MAX_PATH), dwForce);
                if(ERROR_SUCCESS !=  SetSystemPowerState(NULL, pps->dwFlags, dwForce))
                {
                    dwErr = GetLastError();
                    PWRMSG(LOG_DETAIL,_T("SetSystemPowerState() failed for \"%s\". GetLastError = %d"),
                        pps->szStateName, dwErr);
                    PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
                    continue;
                }
            }

            // Now Get Power State to check if the Power State was indeed set
            if(ERROR_SUCCESS !=  GetSystemPowerState(szCurPowerState, _countof(szCurPowerState), &dwCurFlags))
            {
                PWRMSG(LOG_DETAIL,_T("GetSystemPowerState() returned FALSE."));
                PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
                continue;
            }
            else
            {
                PWRMSG(LOG_DETAIL, _T("Current System Power State is '%s', State Flag: 0x%08x\n"),
                    szCurPowerState, dwCurFlags);
            }

            // If Power State Transition occured, we will check for notification
            if(dwCurFlags == pps->dwFlags)
            {
                PWRMSG(LOG_DETAIL, _T("System Power state transition occured. Verify notification."));

                // Verify Notification Message
                if(!VerifyPowerMsg(hMsgQ, PBT_TRANSITION, pps->szStateName, pps->dwFlags))
                {
                    PWRMSG(LOG_FAIL,_T("VerifyPowerMsg() Failed. Fail in %s at line %d :%s"),
                        _T(__FILE__),__LINE__,pszPwrTest);
                }
            }
            else
            {
                PWRMSG(LOG_DETAIL, _T("The Power state obtained does not match the Power State that was set."));
                PWRMSG(LOG_DETAIL, _T("Power State Transition did not work. Notification will not be generated. We will continue."));
            }
        }
    }

    PWRMSG(LOG_DETAIL,_T(""));
    PWRMSG(LOG_DETAIL,_T("We are done."));

CleanAndReturn:

    if(hNotif)
    {
        PWRMSG(LOG_DETAIL,_T("Stopping power notifications."));
        if(ERROR_SUCCESS != StopPowerNotifications(hNotif))
        {
            PWRMSG(LOG_FAIL,_T(" StopPowerNotifications() failed. Fail in %s at line %d :%s."),
                _T(__FILE__),__LINE__,pszPwrTest);
        }
    }
    else if(hMsgQ)
    {
        PWRMSG(LOG_DETAIL,_T("Closing power notifications message queue."));
        CloseMsgQueue(hMsgQ);
    }

    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Tst_SuspendResumeMsg
//  Test for verifying power messages during Suspend and Resume
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_SuspendResumeTransitionMsg(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{

    // The shell doesn't necessarily want us to execute the test.
    // Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    HANDLE hMsgQ = NULL;
    HANDLE hNotif = NULL;

    SET_PWR_TESTNAME(_T("Tst_SuspendResumeTransitionMsg"));

    // This test is for the DEFAULT power model only, check if PDA tests are enabled
    if(g_bPdaVersion == TRUE)
    {
        PWRMSG(LOG_SKIP,_T("This test will skip as it targets the DEFAULT power model and the command line specified the PDA version."));
        PWRMSG(LOG_DETAIL,_T("Do not specify the option (-c\"PdaVersion\") to allow this test to execute."));
        return GetTestResult();
    }       

    // Continue with the test
    PWRMSG(LOG_DETAIL, _T("This test will Suspend and Resume the system and verify that"));
    PWRMSG(LOG_DETAIL, _T("appropriate notification messages are sent."));
    PWRMSG(LOG_DETAIL, _T("")); //Blank line

    // Check if suspend is allowed
    if(!g_bAllowSuspend)
    {
        PWRMSG(LOG_SKIP,_T("Cannot Transition to State \"%s\", this option is not allowed. "), POWER_STATE_NAME_SUSPEND);
        PWRMSG(LOG_DETAIL, _T("Specify this option(-c\"AllowSuspend\") to allow the tests to suspend the system."));
        goto CleanAndReturn;
    }

    // Make sure the System is in On State
    if(ERROR_SUCCESS != SetSystemPowerState(POWER_STATE_NAME_ON, POWER_NAME, POWER_FORCE))
    {
        PWRMSG(LOG_DETAIL,_T("Could not set the system to 'On' state."));
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        goto CleanAndReturn;
    }

    //Initialize Message Queue and Request power Notification
    if(!InitPowerMsg(&hMsgQ, &hNotif))
    {
        PWRMSG(LOG_FAIL,_T(" InitPowerMsg() Failed. Fail in %s at line %d :%s"),_T(__FILE__),__LINE__,pszPwrTest);
    }

    // Try all 4 variations???

    // Clear the notification queue
    if(!ClearMsgQ(hMsgQ))
    {
        PWRMSG(LOG_FAIL,_T("ClearMsgQ()  Failed. Fail in %s at line %d :%s"),_T(__FILE__),__LINE__,pszPwrTest);
    }

    // Suspend and Resume the system
    PWRMSG(LOG_DETAIL,TEXT("Calling SetSystemPowerState(%s, 0, 0)"),POWER_STATE_NAME_SUSPEND);
    if(!SuspendResumeSystem(POWER_STATE_NAME_SUSPEND, 0, 0))
    {
        PWRMSG(LOG_DETAIL,_T("Suspend/Resume system did not work."));
        PWRMSG(LOG_FAIL,_T("Fail in %s at line %d :%s Failed "),__FILE__,__LINE__,pszPwrTest);
        goto CleanAndReturn;
    }

    // Verify Notification Messages
    if(!VerifySuspendResumeMsg(hMsgQ))
    {
        PWRMSG(LOG_FAIL,_T("VerifySuspendResumeMsg() failed. Fail in %s at line %d :%s"),
            _T(__FILE__),__LINE__,pszPwrTest);
    }


CleanAndReturn:

    if(hNotif)
    {
        PWRMSG(LOG_DETAIL,_T("Stopping power notifications."));
        if(ERROR_SUCCESS != StopPowerNotifications(hNotif))
        {
            PWRMSG(LOG_FAIL,_T(" StopPowerNotifications() failed. Fail in %s at line %d :%s."),
                _T(__FILE__),__LINE__,pszPwrTest);
        }
    }
    else if(hMsgQ)
    {
        PWRMSG(LOG_DETAIL,_T("Closing power notifications message queue."));
        CloseMsgQueue(hMsgQ);
    }

    return GetTestResult();
}
