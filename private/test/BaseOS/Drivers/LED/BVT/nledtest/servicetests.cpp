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
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// --------------------------------------------------------------------
//
// Module Name: serviceTests.cpp
//
//
// Notes: This file contains implementation of test procs for testing the
//        NLED service.
//
//        These tests actually first replace the original nled registry keys
//        with temporary registry keys and then raise and verify the event.
//        And once the test is complete the original reg keys are restored.
//        During the test, Nled Service is reloaded multiple times.
//
//
    
#include "globals.h"
#include "service.h"
#include "nledobj.hpp"

#define __FILE_NAME__                  TEXT("serviceTests.cpp")
#define REG_TRIGGER_VALUENAME          (L"TriggerValueName")
#define REG_EVENT_PATH_LENGTH          256

BOOL g_fTestCompleted = FALSE;

//*****************************************************************************
TESTPROCAPI NLEDServiceEventPathMappingTest(UINT                   uMsg,
                                            TPPARAM                tpParam,
                                            LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Return Value:   TPR_PASS On Success,
//                  TPR_FAIL On Failure
//                  TPR_NOT_HANDLED If the feature is not handled,
//                  TPR_SKIP If the test is skipped
//
//  To Verify that Event –> LED mapping works. Set the event and corresponding LED
//  number in the [HKEY_LOCAL_MACHINE\Services\NLEDSRV\Config\...
//  Also to verify that the LED is activated when the corresponding event occurs.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //only supporting executing the test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the service or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fServiceTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    //Returning pass if the device does not have any NLEDs
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("The device doesn't have any NLEDs so passing the test"));

        return TPR_PASS;
    }

    //Variable declarations
    NLED_SETTINGS_INFO *pOriginalLedSettingsInfo = NULL;
    LONG lRetVal = 0;
    DWORD *pLedNumEventId = NULL;
    DWORD dwTotalEvents = 0;
    DWORD dwReturnValue = TPR_FAIL;
    NLED_EVENT_DETAILS *pEventDetails_list = NULL;
    HKEY *hEventOrigKey = NULL;
    HKEY hTemporaryEventKey = NULL;
    HKEY hNLEDConfigKey = NULL;
    HKEY hLEDKey = NULL;
    HKEY hEventKey = NULL;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLEDNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwCurLEDIndex = 0;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwCurLEDEventIndex = 0;

    // Allocate space to save origional LED setttings.
    SetLastError(ERROR_SUCCESS);
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];

    if(!pOriginalLedSettingsInfo)
    {
        FAIL("Memory allocation failed");
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tLast Error = %d"),
                     GetLastError());

        return TPR_FAIL;
    }

    ZeroMemory(pOriginalLedSettingsInfo, GetLedCount() * sizeof(NLED_SETTINGS_INFO));

    // Save the Info Settings for all the LEDs and turn them off.
    lRetVal = saveAllLedSettings(pOriginalLedSettingsInfo, LED_OFF);

    if(!lRetVal)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    }

    //Pointer to an array to store the Info if the Nled has any events configured
    pLedNumEventId = new DWORD[GetLedCount()];

    //Getting the Event Ids for all the Nleds
    lRetVal = GetNledEventIds(pLedNumEventId);

    if(TPR_PASS != lRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to retrieve total number of events for all the Leds."));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Calculating the total number of events of all the Leds
    for(DWORD i = 0; i < GetLedCount(); i++)
    {
        dwTotalEvents = dwTotalEvents + pLedNumEventId[i];
    }

    if(0 == dwTotalEvents)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("No LED has at least one event configured.  Skipping the test."));

        dwReturnValue = TPR_SKIP;
        goto RETURN;
    }

    //Storing the event paths and unsetting all the events
    pEventDetails_list = new NLED_EVENT_DETAILS[dwTotalEvents];

    for(DWORD i=0;i<dwTotalEvents;i++)
    {
        ZeroMemory(&pEventDetails_list[i], sizeof(NLED_EVENT_DETAILS));
    }

    hEventOrigKey = new HKEY[dwTotalEvents];

    lRetVal = UnSetAllNledServiceEvents(pEventDetails_list, &hTemporaryEventKey, hEventOrigKey);

    if(!lRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to unset all the Nled Service Events"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Open LED Config Path
    lRetVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE, REG_NLED_KEY_CONFIG_PATH, 0, 0, &hNLEDConfigKey);

    if(lRetVal !=ERROR_SUCCESS)
    {
        FAIL("Unable to open the Config directory for LEDs");
        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Enumerate NLED Config Key
    lRetVal = RegEnumKeyEx(hNLEDConfigKey,
                           dwCurLEDIndex,
                           szLedName,
                           &dwLEDNameSize,
                           NULL,
                           NULL,
                           NULL,
                           NULL);

    if(ERROR_NO_MORE_ITEMS == lRetVal)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("No LEDs configured in the registry"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
     }

    while(lRetVal ==  ERROR_SUCCESS)
    {
            //Open LED Registry
            lRetVal = RegOpenKeyEx(hNLEDConfigKey, szLedName, 0, KEY_ALL_ACCESS, &hLEDKey);

            if(lRetVal !=ERROR_SUCCESS)
            {
                FAIL("Unable to open the LED registry");
                dwReturnValue = TPR_FAIL;
                goto RETURN;
            }
            else
            {
                //Re-Initializing the variable for next Nled
                dwCurLEDEventIndex = 0;

                lRetVal = RegEnumKeyEx(hLEDKey,
                                       dwCurLEDEventIndex,
                                       szEventName,
                                       &dwEventNameSize,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL);

                //If no events configured
                if(ERROR_NO_MORE_ITEMS == lRetVal)
                {
                    if(dwReturnValue != TPR_PASS)
                    {
                        dwReturnValue = TPR_SKIP;
                    }

                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("No events configured for %s"), szLedName);
                }

                while(lRetVal ==  ERROR_SUCCESS)
                {
                    lRetVal = RegOpenKeyEx(hLEDKey,
                                          szEventName,
                                          0,
                                          KEY_ALL_ACCESS,
                                          &hEventKey);

                    if(lRetVal != ERROR_SUCCESS)
                    {
                        FAIL("Unable to open the LED registry");
                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Test if the Event to LED mapping works
                    lRetVal = RaiseTemporaryEvent(hLEDKey, hEventKey, NULL);

                    if(!lRetVal)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("LED-Event Mapping does not work for LED:%s Event:%s"),
                                     szLedName,
                                     szEventName);

                        FAIL("Error in raising the temporary event");
                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }
                    else
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("LED-Event Mapping successfully works for LED:%s Event:%s"),
                                     szLedName,
                                     szEventName);

                        dwReturnValue = TPR_PASS;
                    }

                    //Close The Event Key
                    RegCloseKey(hEventKey);
                    dwCurLEDEventIndex++;
                    dwEventNameSize = gc_dwMaxKeyNameLength + 1;
                    lRetVal = RegEnumKeyEx(hLEDKey,
                                           dwCurLEDEventIndex,
                                           szEventName,
                                           &dwEventNameSize,
                                           NULL,
                                           NULL,
                                           NULL,
                                           NULL);
                }//Continue for all events
            }

            //Close The LED Key
            RegCloseKey(hLEDKey);
            dwCurLEDIndex++;
            dwLEDNameSize = gc_dwMaxKeyNameLength + 1;
            lRetVal = RegEnumKeyEx(hNLEDConfigKey,
                                   dwCurLEDIndex,
                                   szLedName,
                                   &dwLEDNameSize,
                                   NULL,
                                   NULL,
                                   NULL,
                                   NULL);
    }//For all LEDs

RETURN:

    //Restore back the original Event Paths
    lRetVal = RestoreAllNledServiceEventPaths(pEventDetails_list,
                                             hEventOrigKey,
                                             dwTotalEvents);

    if(!lRetVal)
    {
        dwReturnValue = TPR_FAIL;
        FAIL("Unable to restore back the original event paths");
    }

    //Delete the temporary registry key
    if(hTemporaryEventKey != NULL)
    {
        lRetVal =  RegCloseKey(hTemporaryEventKey);

        if(lRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Closing Failed. Last Error = %ld"),
                         GetLastError());

            dwReturnValue = TPR_FAIL;
        }

        lRetVal = RegDeleteKey(HKEY_LOCAL_MACHINE, TEMPORARY_EVENT_PATH);

        if(lRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Deletion Failed. Last Error = %ld"),
                         GetLastError());

            dwReturnValue = TPR_FAIL;
        }
    }

    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo != NULL)
    {
        lRetVal = setAllLedSettings(pOriginalLedSettingsInfo);

        if(!lRetVal)
        {
            dwReturnValue = TPR_FAIL;
            FAIL("Unable to restore original NLED settings");
        }

        //De-allocating the memory
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    //Closing The Config Key
    if(hNLEDConfigKey != NULL)
    {
        RegCloseKey(hNLEDConfigKey);
    }

    //Closing the Led Key
    if(hLEDKey != NULL)
    {
        RegCloseKey(hLEDKey);
    }

    //Closing the Event Key
    if(hEventKey != NULL)
    {
        RegCloseKey(hEventKey);
    }

    if(hEventOrigKey != NULL)
    {
        delete[] hEventOrigKey;
        hEventOrigKey = NULL;
    }

    if(pEventDetails_list != NULL)
    {
        delete[] pEventDetails_list;
        pEventDetails_list = NULL;
    }

    if(pLedNumEventId != NULL)
    {
        delete[] pLedNumEventId;
        pLedNumEventId = NULL;
    }

    if(TPR_PASS == dwReturnValue)
    {
        SUCCESS("LED->Event mapping test is successful");
    }
    else if(TPR_FAIL == dwReturnValue)
        FAIL("LED->Event mapping test is failed");

    return dwReturnValue;
}

//*****************************************************************************
TESTPROCAPI NLEDServiceBlinkingTest(UINT                   uMsg,
                                    TPPARAM                tpParam,
                                    LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Return Value:  TPR_PASS On Success,
//                 TPR_FAIL On Failure
//                 TPR_NOT_HANDLED If the feature is not handled,
//                 TPR_SKIP If the test is skipped
//
//
//  To Update registry for each LED to either ON/OFF/BLINK. LED must
//  behave as expected when the trigger event occurs. And also to ensure that
//  if an LED is configured to blink but no blink parameters are set,
//  the LED will blink with default parameters.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //only supporting executing the test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the service or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fServiceTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    //Returning pass if the device does not have any NLEDs
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("The device doesn't have any NLEDs so passing the test"));

        return TPR_PASS;
    }

    //Declaring the variables
    NLED_SETTINGS_INFO *pOriginalLedSettingsInfo = NULL;
    LONG lRetVal = 0;
    DWORD *pLedNumEventId = NULL;
    DWORD dwTotalEvents = 0;
    DWORD dwTestResult = TPR_FAIL;
    NLED_EVENT_DETAILS *pEventDetails_list = NULL;
    HKEY *hEventOrigKey = NULL;
    HKEY hTemporaryEventKey = NULL;
    HKEY hNLEDConfigKey = NULL;
    HKEY hLEDKey = NULL;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLEDNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwCurLEDIndex = 0;
    HKEY hEventKey = NULL;
    DWORD dwCurLEDEventIndex = 0;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;

    // Allocate space to save origional LED setttings.
    SetLastError(ERROR_SUCCESS);
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];

    if(!pOriginalLedSettingsInfo)
    {
        FAIL("Memory allocation failed");
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tLast Error = %d"),
                     GetLastError());

        return TPR_FAIL;
    }

    ZeroMemory(pOriginalLedSettingsInfo, GetLedCount() * sizeof(NLED_SETTINGS_INFO));

    // Save the Info Settings for all the LEDs and turn them off.
    lRetVal = saveAllLedSettings(pOriginalLedSettingsInfo, LED_OFF);

    if(!lRetVal)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    }

    //Pointer to an array to store the Info if the Nled has any events configured
    pLedNumEventId = new DWORD[GetLedCount()];

    //Getting the Event Ids for all the Nleds
    lRetVal = GetNledEventIds(pLedNumEventId);

    if(TPR_PASS != lRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to retrieve total number of events for all the Leds."));

        dwTestResult = TPR_FAIL;
        goto RETURN;
    }

    //Calculating the total number of events of all the Leds
    for(DWORD i = 0; i < GetLedCount(); i++)
    {
        dwTotalEvents = dwTotalEvents + pLedNumEventId[i];
    }

    if(0 == dwTotalEvents)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("No LED has at least one event configured.  Skipping the test."));

        dwTestResult = TPR_SKIP;
        goto RETURN;
    }

    //Saving the event paths and unsetting all the events
    pEventDetails_list = new NLED_EVENT_DETAILS[dwTotalEvents];

    for(DWORD i=0;i<dwTotalEvents;i++)
    {
        ZeroMemory(&pEventDetails_list[i], sizeof(NLED_EVENT_DETAILS));
    }

    hEventOrigKey = new HKEY[dwTotalEvents];

    lRetVal = UnSetAllNledServiceEvents(pEventDetails_list,
                                       &hTemporaryEventKey,
                                       hEventOrigKey);

    if(!lRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to unset all the Nled Service Events"));

        dwTestResult = TPR_FAIL;
        goto RETURN;
    }

    lRetVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                           REG_NLED_KEY_CONFIG_PATH,
                           0,
                           0,
                           &hNLEDConfigKey);

    if(lRetVal !=ERROR_SUCCESS)
    {
        //Log the error message
        FAIL("Unable to open the Config directory for LEDs");
        dwTestResult = TPR_FAIL;
        goto RETURN;
    }

    lRetVal = RegEnumKeyEx(hNLEDConfigKey,
                           dwCurLEDIndex,
                           szLedName,
                           &dwLEDNameSize,
                           NULL,
                           NULL,
                           NULL,
                           NULL);

    if(ERROR_NO_MORE_ITEMS == lRetVal)
    {
        //Log the error message
        FAIL("No LEDs configured in the registry");
        dwTestResult = TPR_FAIL;
        goto RETURN;
     }

    while(lRetVal ==  ERROR_SUCCESS)
    {
            lRetVal = RegOpenKeyEx(hNLEDConfigKey,
                                   szLedName,
                                   0,
                                   KEY_ALL_ACCESS,
                                   &hLEDKey);

            if(lRetVal !=ERROR_SUCCESS)
            {
                //Log the error message
                FAIL("Unable to open the LED registry");
                dwTestResult = TPR_FAIL;
                goto RETURN;
            }
            else
            {
                //Re-Initializing the index variable for next Nled
                dwCurLEDEventIndex = 0;

                lRetVal = RegEnumKeyEx(hLEDKey,
                                      dwCurLEDEventIndex,
                                      szEventName,
                                      &dwEventNameSize,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL);

                if(ERROR_NO_MORE_ITEMS == lRetVal)
                {
                    //Log the message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("No Events Configured for %s"),
                                 szLedName);

                    dwTestResult = TPR_SKIP;
                 }

                while(lRetVal ==  ERROR_SUCCESS)
                {
                    lRetVal = RegOpenKeyEx(hLEDKey,
                                           szEventName,
                                           0,
                                           KEY_ALL_ACCESS,
                                           &hEventKey);

                    if(lRetVal !=ERROR_SUCCESS)
                    {
                        //Log the error message
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable to open the Event registry key for %s"),
                                     szEventName);

                        dwTestResult = TPR_FAIL;
                        goto RETURN;
                    }

                    //Test if the LED blinks with configured blinking params
                    lRetVal  = BlinkLED(hLEDKey,
                                        szLedName,
                                        hEventKey,
                                        szEventName);

                    if(lRetVal == TPR_FAIL)
                    {
                        //Log the error message
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Could not blink a blinkable LED %s"),
                                     szLedName);

                        dwTestResult = TPR_FAIL;
                        goto RETURN;
                    }
                    else if (lRetVal == TPR_PASS)
                    {
                        dwTestResult = TPR_PASS;
                    }

                    //Test Part 2 - Test if the LED blinks with default blinking params
                    lRetVal  = BlinkLEDWithDefaultParams(hLEDKey,
                                                         szLedName,
                                                         hEventKey,
                                                         szEventName);
                    if(lRetVal == TPR_FAIL)
                    {
                        //Log the error message
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Could not blink a blinkable LED(%s) with default parameters"),
                                     szLedName);

                        dwTestResult = TPR_FAIL;
                        goto RETURN;
                    }
                    else if (lRetVal == TPR_PASS)
                    {
                        dwTestResult = TPR_PASS;
                    }

                    RegCloseKey(hEventKey);
                    dwCurLEDEventIndex++;
                    dwEventNameSize = gc_dwMaxKeyNameLength + 1;
                    lRetVal = RegEnumKeyEx(hLEDKey,
                                           dwCurLEDEventIndex,
                                           szEventName,
                                           &dwEventNameSize,
                                           NULL,
                                           NULL,
                                           NULL,
                                           NULL);
                }
            }

            //Close the key handle
            RegCloseKey(hLEDKey);

            //Incrementing the key index
            dwCurLEDIndex++;
            dwLEDNameSize = gc_dwMaxKeyNameLength + 1;

            //Enumerating for more Leds
            lRetVal = RegEnumKeyEx(hNLEDConfigKey,
                                   dwCurLEDIndex,
                                   szLedName,
                                   &dwLEDNameSize,
                                   NULL,
                                   NULL,
                                   NULL,
                                   NULL);
    }

RETURN:
    //Restore back the original Event Paths
    lRetVal = RestoreAllNledServiceEventPaths(pEventDetails_list,
                                              hEventOrigKey,
                                              dwTotalEvents);

    if(!lRetVal)
    {
        dwTestResult = TPR_FAIL;
        FAIL("Unable to restore back the original event paths");
    }

    //Delete the temporary registry key
    if(hTemporaryEventKey != NULL)
    {
        lRetVal =  RegCloseKey(hTemporaryEventKey);

        if(lRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Closing Failed. Last Error = %ld"),
                         GetLastError());

            dwTestResult = TPR_FAIL;
        }

        lRetVal = RegDeleteKey(HKEY_LOCAL_MACHINE, TEMPORARY_EVENT_PATH);

        if(lRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Deletion Failed. Last Error = %ld"),
                         GetLastError());

            dwTestResult = TPR_FAIL;
        }
    }

    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo != NULL)
    {
        lRetVal = setAllLedSettings(pOriginalLedSettingsInfo);

        if(!lRetVal)
        {
            dwTestResult = TPR_FAIL;
            FAIL("Unable to restore original NLED settings");
        }

        //De-allocating the memory
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    //Closing the key handle
    if(hNLEDConfigKey != NULL)
    {
        RegCloseKey(hNLEDConfigKey);
    }

    if(hEventKey != NULL)
    {
        RegCloseKey(hEventKey);
    }

    if(hLEDKey != NULL)
    {
        RegCloseKey(hLEDKey);
    }

    if(pLedNumEventId != NULL)
    {
        delete[] pLedNumEventId;
        pLedNumEventId = NULL;
    }

    if(hEventOrigKey != NULL)
    {
        delete[] hEventOrigKey;
        hEventOrigKey = NULL;
    }

    if(pEventDetails_list != NULL)
    {
        delete[] pEventDetails_list;
        pEventDetails_list = NULL;
    }

    if(TPR_PASS == dwTestResult)
        SUCCESS("LED blinks with default parameters, if actuals are not set in registry");

    else if(TPR_FAIL == dwTestResult)
        FAIL("LED does not blink with default parameters if actual parameters are not set");

    return dwTestResult;
}

//*****************************************************************************
TESTPROCAPI NLEDServiceBlinkSettingsTest(UINT                   uMsg,
                                         TPPARAM                tpParam,
                                         LPFUNCTION_TABLE_ENTRY lpFTE)
//
//    Parameters: Standard TUX arguments
//
//    Return Value:   TPR_PASS On Success,
//                    TPR_FAIL On Failure
//                    TPR_NOT_HANDLED If the feature is not handled,
//                    TPR_SKIP If the test is skipped
//
//    To Update registry for each LED with state as BLINK and enter a valid total
//    cycle time. LED must behave as expected when the trigger event occurs.
//    To Set a valid value for ON Time in registry. LED must be ON for that
//    duration during blinking.
//    To Set a valid value for OFF Time in registry. LED must be OFF for that
//    duration during blinking.
//    To Set a valid value for MetaCycleOn in registry. LED must ON
//    for that many cycles during blinking.
//    MetaCycle OFF - Set a valid value for MetaCycleOff in registry. LED must OFF
//    for that many cycles during blinking.
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Only Supporting executing the test
    if(uMsg != TPM_EXECUTE)  return TPR_NOT_HANDLED;

    //Skipping the test if the service or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fServiceTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    //Returning pass if the device does not have any NLEDs
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("The device doesn't have any NLEDs so passing the test"));

        return TPR_PASS;
    }

    //Declaring the variables
    NLED_SETTINGS_INFO *pOriginalLedSettingsInfo = NULL;
    BOOL fStatus = FALSE;
    DWORD *pLedNumEventId = NULL;
    DWORD dwTotalEvents = 0;
    DWORD dwTestPass = TPR_FAIL;
    LONG  lRetVal = 0;
    NLED_EVENT_DETAILS *pEventDetails_list = NULL;
    HKEY *hEventOrigKey = NULL;
    HKEY hTemporaryEventKey = NULL;
    HKEY hNLEDConfigKey = NULL;
    LONG lMoreLEDs = 0;
    HKEY hLEDKey = NULL;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLEDNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwCurLEDIndex = 0;
    HKEY hEventKey = NULL;
    DWORD dwCurLEDEventIndex = 0;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;
    LONG lMoreEvents = 0;
    
    // Allocate space to save origional LED setttings.
    SetLastError(ERROR_SUCCESS);
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];

    if(!pOriginalLedSettingsInfo)
    {
        FAIL("Memory allocation failed");
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tLast Error = %d"),
                     GetLastError());

        return TPR_FAIL;
    }

    ZeroMemory(pOriginalLedSettingsInfo, GetLedCount() * sizeof(NLED_SETTINGS_INFO));

    // Save the Info Settings for all the LEDs and turn them off.
    fStatus =  saveAllLedSettings(pOriginalLedSettingsInfo, LED_OFF);

    if(!fStatus)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    }

    //Pointer to Array to store the Info if the Nled has any events configured
    pLedNumEventId = new DWORD[GetLedCount()];

    //Getting the Event Ids for all the Nleds
    lRetVal = GetNledEventIds(pLedNumEventId);

    if(TPR_PASS != lRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to retrieve total number of events for all the Leds."));

        dwTestPass = TPR_FAIL;
        goto RETURN;
    }

    //Calculating the total number of events of all the Leds
    for(DWORD i = 0; i < GetLedCount(); i++)
    {
        dwTotalEvents = dwTotalEvents + pLedNumEventId[i];
    }

    if(0 == dwTotalEvents)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("No LED has at least one event configured. Skipping the test."));

        dwTestPass = TPR_SKIP;
        goto RETURN;
    }

    //Storing the event paths and unsetting all the events
    pEventDetails_list = new NLED_EVENT_DETAILS[dwTotalEvents];

    for(DWORD i=0;i<dwTotalEvents;i++)
    {
        ZeroMemory(&pEventDetails_list[i], sizeof(NLED_EVENT_DETAILS));
    }

    hEventOrigKey = new HKEY[dwTotalEvents];

    lRetVal = UnSetAllNledServiceEvents(pEventDetails_list,
                                       &hTemporaryEventKey,
                                       hEventOrigKey);

    if(!lRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to unset all the Nled Service Events"));

        dwTestPass = TPR_FAIL;
        goto RETURN;
    }

    lRetVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE, REG_NLED_KEY_CONFIG_PATH, 0, 0, &hNLEDConfigKey);

    if(lRetVal !=ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to open the Config directory for LEDs"));

        dwTestPass = TPR_FAIL;
        goto RETURN;
    }

    lMoreLEDs = RegEnumKeyEx(hNLEDConfigKey,
                             dwCurLEDIndex,
                             szLedName,
                             &dwLEDNameSize,
                             NULL,
                             NULL,
                             NULL,
                             NULL);

    if(ERROR_NO_MORE_ITEMS == lMoreLEDs)
    {
        //Log the error message
        FAIL("No LEDs configured in the registry");
        dwTestPass = TPR_FAIL;
        goto RETURN;
     }

    while(lMoreLEDs ==  ERROR_SUCCESS)
    {
        lRetVal = RegOpenKeyEx(hNLEDConfigKey,
                               szLedName,
                               0,
                               KEY_ALL_ACCESS,
                               &hLEDKey);

        if(lRetVal !=ERROR_SUCCESS)
        {
            //Log the error message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to open the LED registry"));

            dwTestPass = TPR_FAIL;
            goto RETURN;
        }
        else
        {
            //Re-Initializing the index for next Nled
            dwCurLEDEventIndex = 0;

            lMoreEvents = RegEnumKeyEx(hLEDKey,
                                       dwCurLEDEventIndex,
                                       szEventName,
                                       &dwEventNameSize,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL);

            if(ERROR_NO_MORE_ITEMS == lMoreEvents)
            {
                //Log the message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("No Events configured for %s"), szLedName);

                dwTestPass = TPR_SKIP;
            }
            while(ERROR_SUCCESS == lMoreEvents)
            {
                lRetVal = RegOpenKeyEx(hLEDKey,
                                       szEventName,
                                       0,
                                       KEY_ALL_ACCESS,
                                       &hEventKey);

                if(lRetVal !=ERROR_SUCCESS)
                {
                    //Log the error message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to open the Event registry key for %s"),
                                 szEventName);

                    dwTestPass = TPR_FAIL;
                    goto RETURN;
                }

                //Test Part 1 - Test if the LED blinks with configured blinking params
                lRetVal  = VerifyBlinkSettings(hLEDKey,
                                               szLedName,
                                               hEventKey,
                                               szEventName);

                if(lRetVal == TPR_FAIL)
                {
                    //Log the error message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Could not blink %s with the specified blinking parameters"),
                                 szLedName);

                    dwTestPass = TPR_FAIL;
                    goto RETURN;
                }
                else if (lRetVal == TPR_PASS)
                {
                    dwTestPass = TPR_PASS;
                }

                RegCloseKey(hEventKey);
                dwCurLEDEventIndex++;
                dwEventNameSize = gc_dwMaxKeyNameLength + 1;
                lMoreEvents = RegEnumKeyEx(hLEDKey,
                                           dwCurLEDEventIndex,
                                           szEventName,
                                           &dwEventNameSize,
                                           NULL,
                                           NULL,
                                           NULL,
                                           NULL);
            }
        }

        //Closing the key handle
        RegCloseKey(hLEDKey);

        //Incrementing the key count
        dwCurLEDIndex++;
        dwLEDNameSize = gc_dwMaxKeyNameLength + 1;

        //Enumerating for more Leds
        lMoreLEDs = RegEnumKeyEx(hNLEDConfigKey,
                                 dwCurLEDIndex,
                                 szLedName,
                                 &dwLEDNameSize,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL);
    }

RETURN:
    //Restore back the original Event Paths
    lRetVal = RestoreAllNledServiceEventPaths(pEventDetails_list,
                                              hEventOrigKey,
                                              dwTotalEvents);

    if(!lRetVal)
    {
        dwTestPass = TPR_FAIL;
        FAIL("Unable to restore back the original event paths");
    }

    //Delete the temporary registry key
    if(hTemporaryEventKey != NULL)
    {
        lRetVal =  RegCloseKey(hTemporaryEventKey);

        if(lRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Closing Failed. Last Error = %ld"),
                         GetLastError());

            dwTestPass = TPR_FAIL;
        }

        lRetVal = RegDeleteKey(HKEY_LOCAL_MACHINE, TEMPORARY_EVENT_PATH);

        if(lRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Deletion Failed. Last Error = %ld"),
                         GetLastError());

            dwTestPass = TPR_FAIL;
        }
    }

    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo != NULL)
    {
        fStatus =  setAllLedSettings(pOriginalLedSettingsInfo);

        if(!fStatus)
        {
            dwTestPass = TPR_FAIL;
            FAIL("Unable to restore original NLED settings");
        }
    }

    if(pOriginalLedSettingsInfo != NULL)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    if(hNLEDConfigKey != NULL)
    {
        RegCloseKey(hNLEDConfigKey);
    }

    if(hLEDKey != NULL)
    {
        RegCloseKey(hLEDKey);
    }

    if(hEventKey != NULL)
    {
        RegCloseKey(hEventKey);
    }

    if(hEventOrigKey != NULL)
    {
        delete[] hEventOrigKey;
        hEventOrigKey = NULL;
    }

    if(pEventDetails_list != NULL)
    {
        delete[] pEventDetails_list;
        pEventDetails_list = NULL;
    }

    if(pLedNumEventId != NULL)
    {
        delete[] pLedNumEventId;
        pLedNumEventId = NULL;
    }

    if(TPR_PASS == dwTestPass)
        SUCCESS("Nled service successfully blinks the Nleds with the blinking parameters that are set");

    else if(TPR_FAIL == dwTestPass)
        FAIL("Nled service fails to blink the Nleds with the blinking parameters that are set");

    return dwTestPass;
}

//*****************************************************************************
DWORD BlinkLED(HKEY hLEDKey,
               TCHAR* szLedName,
               HKEY hEventKey,
               TCHAR* szEventName)
//
// Parameters:     NLED Configuration Key,
//                 LED Key,
//                 LED Name,
//                 Event Key,
//                 Event Name
//
// Return Value:   TPR_PASS On Success,
//                 TPR_FAIL On Failure
//                 TPR_NOT_HANDLED If the feature is not handled,
//                 TPR_SKIP If the test is skipped
//
// To blink the specified Nled(szLedName) by raising the event(szEventName)
//
//*****************************************************************************
{
    //Declaring the variables
    DWORD dwBlinkable    = 0;
    DWORD dwSWBlinkCtrl  = 0;
    DWORD dwTempState    = 0;
    DWORD dwState        = 0;
    LONG  lRetVal        = 0;
    BOOL  fStatus        = FALSE;

    //Getting the Blinkable value for Nled having Key hLEDKey
    fStatus = GetDwordFromReg(hLEDKey, REG_BLINKABLE_VALUE, &dwBlinkable);

    if(!fStatus)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Blinkable not configured in registry for LED %s"),
                     szLedName);

        return TPR_FAIL;
    }

    //LED should blink if it can blink or SWBlinkCtrl is ON
    GetDwordFromReg(hLEDKey, REG_SWBLINKCTRL_VALUE, &dwSWBlinkCtrl);

    if(dwBlinkable || dwSWBlinkCtrl)
    {
        //Get state of LED
        fStatus = GetDwordFromReg(hEventKey, REG_EVENT_STATE_VALUE, &dwState);

        if(!fStatus)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("State not configured in registry for event %s"),
                         szEventName);

            return TPR_FAIL;
        }

        if(dwState != LED_BLINK)
        {
            dwTempState = LED_BLINK;
            lRetVal = RegSetValueEx (hEventKey,
                                     REG_EVENT_STATE_VALUE,
                                     0,
                                     REG_DWORD,
                                     (PBYTE)(&dwTempState),
                                     sizeof(dwTempState));

            if(lRetVal !=ERROR_SUCCESS)
            {
                //Log Error Message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to temporarily update the state in registry for LED %s"),
                             szLedName);

                return TPR_FAIL;
            }
        }

        //Test if the LED Blinks once event is trigerred
        fStatus = RaiseTemporaryEvent(hLEDKey, hEventKey, NULL);

        if(!fStatus)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Service not able to blink the %s LED"),
                         szLedName);

            return TPR_FAIL;
        }
        else
        {
            if(dwState != LED_BLINK)
            {
                //Set the state back to the original value
                lRetVal = RegSetValueEx (hEventKey,
                                         REG_EVENT_STATE_VALUE,
                                         0,
                                         REG_DWORD,
                                         (PBYTE)(&dwState),
                                         sizeof(dwState));

                if(lRetVal !=ERROR_SUCCESS)
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to temporarily update the state in registry for LED %s"),
                                 szLedName);

                    return TPR_FAIL;
                }
            }
            return TPR_PASS;
         }
     }

     return TPR_SKIP;
}
//*****************************************************************************
DWORD BlinkLEDWithDefaultParams(HKEY hLEDKey,
                                TCHAR* szLedName,
                                HKEY hEventKey,
                                TCHAR* szEventName)
//
// Parameters:    NLED Configuration Key,
//                LED Key,
//                LED Name,
//                Event Key,
//                Event Name
//
// Return Value:  TPR_PASS On Success,
//                TPR_FAIL On Failure
//                TPR_NOT_HANDLED If the feature is not handled,
//                TPR_SKIP If the test is skipped
//
// Function to blink the NLED with default blink parameters.
//
//*****************************************************************************
{
    DWORD dwBlinkable                = 0;
    DWORD dwSWBlinkCtrl              = 0;
    DWORD dwTempState                = 0;
    DWORD dwState                    = 0;
    LONG  lRetVal                     = 0;
    BOOL  fRestoreTotalCycleTime     = FALSE;
    BOOL  fRestoreOnTime             = FALSE;
    BOOL  fRestoreOffTime            = FALSE;
    BOOL  fRestoreMetaCycleOn        = FALSE;
    BOOL  fRestoreMetaCycleOff       = FALSE;
    DWORD dwTotalCycleTime           = 0;
    DWORD dwOnTime                   = 0;
    DWORD dwOffTime                  = 0;
    DWORD dwMetaCycleOn              = 0;
    DWORD dwMetaCycleOff             = 0;
    DWORD dwReturn                   = TPR_SKIP;
    BOOL  fStatus                    = FALSE;

    //Getting the Blinkable value for Nled having hLEDKey key
    fStatus = GetDwordFromReg(hLEDKey, REG_BLINKABLE_VALUE, &dwBlinkable);

    if(!fStatus)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Blinkable not configured in registry for LED %s"),
                     szLedName);

        return TPR_FAIL;
    }

    //LED should blink if it can blink or SW Blink Ctrl is ON
    GetDwordFromReg(hLEDKey, REG_SWBLINKCTRL_VALUE, &dwSWBlinkCtrl);

    if(dwBlinkable || dwSWBlinkCtrl)
    {
        //Log Error Message
        fStatus = GetDwordFromReg(hEventKey, REG_EVENT_STATE_VALUE, &dwState);

        if(!fStatus)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("State not configured in registry for event %s"),
                         szEventName);

            return TPR_FAIL;
        }

        if(dwState != LED_BLINK)
        {
            dwTempState = LED_BLINK;
            lRetVal = RegSetValueEx (hEventKey,
                                     REG_EVENT_STATE_VALUE,
                                     0,
                                     REG_DWORD,
                                     (PBYTE)(&dwTempState),
                                     sizeof(dwTempState));

            if(ERROR_SUCCESS != lRetVal)
            {
                //Log Error Message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to temporarily update the state in registry for LED %s"),
                             szLedName);

                return TPR_FAIL;
            }
        }

        //Set the default params
        fStatus = GetDwordFromReg(hEventKey,
                                  REG_EVENT_TOTAL_CYCLE_TIME_VALUE,
                                  &dwTotalCycleTime);

        if(fStatus)
        {
            lRetVal = RegDeleteValue(hEventKey, REG_EVENT_TOTAL_CYCLE_TIME_VALUE);

            if(ERROR_SUCCESS != lRetVal)
            {
                //Log Error Message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to modify registry for LED %s"),
                             szLedName);

                goto cleanup;
            }

            fRestoreTotalCycleTime = TRUE;
        }

        fStatus = GetDwordFromReg(hEventKey, REG_EVENT_ON_TIME_VALUE, &dwOnTime);

        if(fStatus)
        {
            lRetVal = RegDeleteValue(hEventKey, REG_EVENT_ON_TIME_VALUE);

            if(ERROR_SUCCESS != lRetVal)
            {
                //Log Error Message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to modify registry for LED %s"),
                             szLedName);

                goto cleanup;
            }

            fRestoreOnTime = TRUE;
        }

        fStatus = GetDwordFromReg(hEventKey, REG_EVENT_OFF_TIME_VALUE, &dwOffTime);

        if(fStatus)
        {
            lRetVal = RegDeleteValue(hEventKey, REG_EVENT_OFF_TIME_VALUE);

            if(lRetVal != ERROR_SUCCESS)
            {
                //Log Error Message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to modify registry for LED %s"),
                             szLedName);

                goto cleanup;
            }

            fRestoreOffTime = TRUE;
        }

        fStatus = GetDwordFromReg(hEventKey,
                                  REG_EVENT_META_CYCLE_ON_VALUE,
                                  &dwMetaCycleOn);

        if(fStatus)
        {
            lRetVal = RegDeleteValue(hEventKey, REG_EVENT_META_CYCLE_ON_VALUE);

            if(lRetVal != ERROR_SUCCESS)
            {
                //Log Error Message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to modify registry for LED %s"),
                             szLedName);

                goto cleanup;
            }

            fRestoreMetaCycleOn = TRUE;
        }

        fStatus = GetDwordFromReg(hEventKey,
                                  REG_EVENT_META_CYCLE_OFF_VALUE,
                                  &dwMetaCycleOff);

        if(fStatus)
        {
            lRetVal = RegDeleteValue(hEventKey, REG_EVENT_META_CYCLE_OFF_VALUE);

            if(lRetVal != ERROR_SUCCESS)
            {
                //Log Error Message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to modify registry for LED %s"),
                             szLedName);

                goto cleanup;
            }

            fRestoreMetaCycleOff = TRUE;
        }

        //Test if the LED Blinks with default params
        fStatus = RaiseTemporaryEvent(hLEDKey, hEventKey, NULL);

        if(!fStatus)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Service not able to blink the LED for the Event %s"),
                         szEventName);

            dwReturn = TPR_FAIL;
        }
        else
        {
            if(dwState != LED_BLINK)
            {
                //Set the state back to the original value
                lRetVal = RegSetValueEx (hEventKey,
                                         REG_EVENT_STATE_VALUE,
                                         0,
                                         REG_DWORD,
                                         (PBYTE)(&dwState),
                                         sizeof(dwState));

                if(lRetVal != ERROR_SUCCESS)
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to temporarily update the state in registry for LED %s"),
                                 szLedName);

                    dwReturn = TPR_FAIL;
                }
            }

            dwReturn = TPR_PASS;
         }

     cleanup:

         if(fRestoreTotalCycleTime)
         {
            RegSetValueEx (hEventKey,
                           REG_EVENT_TOTAL_CYCLE_TIME_VALUE,
                           0,
                           REG_DWORD,
                           (PBYTE)(&dwTotalCycleTime),
                           sizeof(dwTotalCycleTime));
         }
         if(fRestoreOnTime)
         {
            RegSetValueEx (hEventKey,
                           REG_EVENT_ON_TIME_VALUE,
                           0,
                           REG_DWORD,
                           (PBYTE)(&dwOnTime),
                           sizeof(dwOnTime));
         }
         if(fRestoreOffTime)
         {
            RegSetValueEx (hEventKey,
                           REG_EVENT_OFF_TIME_VALUE,
                           0,
                           REG_DWORD,
                           (PBYTE)(&dwOffTime),
                           sizeof(dwOffTime));
        }
        if(fRestoreMetaCycleOn)
        {
            RegSetValueEx (hEventKey,
                           REG_EVENT_META_CYCLE_ON_VALUE,
                           0,
                           REG_DWORD,
                           (PBYTE)(&dwMetaCycleOn),
                           sizeof(dwMetaCycleOn));
        }
        if(fRestoreMetaCycleOff)
        {
            RegSetValueEx (hEventKey,
                           REG_EVENT_META_CYCLE_OFF_VALUE,
                           0,
                           REG_DWORD,
                           (PBYTE)(&dwMetaCycleOff),
                           sizeof(dwMetaCycleOff));
        }
    }

     return dwReturn;
}

//*****************************************************************************
DWORD VerifyBlinkSettings(HKEY hLEDKey,
                          TCHAR* szLedName,
                          HKEY hEventKey,
                          TCHAR* szEventName)
//
// Parameters:   NLED Configuration Key,
//               LED Key,
//               LED Name,
//               Event Key,
//               Event Name
//
// Return Value: TPR_PASS On Success,
//               TPR_FAIL On Failure
//               TPR_NOT_HANDLED If the feature is not handled,
//               TPR_SKIP If the test is skipped
//
// Function to verify the blink parameters
//
//*****************************************************************************
{
    //Declaring the variables
    DWORD dwLedNum                = 0;
    DWORD dwBlinkable             = 0;
    DWORD dwSWBlinkCtrl           = 0;
    DWORD dwTempState             = 0;
    DWORD dwState                 = 0;
    LONG  lRetVal                  = 0;
    BOOL  fDeleteTotalCycleTime   = FALSE;
    BOOL  fDeleteOnTime           = FALSE;
    BOOL  fDeleteOffTime          = FALSE;
    BOOL  fDeleteMetaCycleOn      = FALSE;
    BOOL  fDeleteMetaCycleOff     = FALSE;
    LONG  lTotalCycleTime        = 0;
    LONG  lOnTime                = 0;
    LONG  lOffTime               = 0;
    INT   MetaCycleOn           = 0;
    INT   MetaCycleOff          = 0;
    DWORD dwReturn                = TPR_PASS;
    BOOL  fStatus                 = FALSE;

    NLED_SETTINGS_INFO* pLedSettingsInfoAfterEvent    = new NLED_SETTINGS_INFO[1];
    ZeroMemory(pLedSettingsInfoAfterEvent, sizeof(NLED_SETTINGS_INFO));
    NLED_SUPPORTS_INFO* pLedSupportsInfo = new NLED_SUPPORTS_INFO[1];
    ZeroMemory(pLedSupportsInfo, sizeof(NLED_SUPPORTS_INFO));

    //Getting the Nled Num for Nled having hLEDKey key
    fStatus = GetDwordFromReg(hLEDKey, REG_LED_NUM_VALUE, &dwLedNum);

    if(!fStatus)
    {
        //Log the error message
        g_pKato->Log(LOG_DETAIL,
                      TEXT("LedNum detail not available in registry"));
        dwReturn = TPR_FAIL;
        goto cleanup;
    }

    fStatus = GetDwordFromReg(hLEDKey, REG_BLINKABLE_VALUE, &dwBlinkable);

    if(!fStatus)
    {
        //Log the error message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Blinkable not configured in registry for LED %s"),
                     szLedName);

        dwReturn = TPR_FAIL;
        goto cleanup;
    }

    //LED should blink if it can blink or SWBlinkCtrl is ON
    GetDwordFromReg(hLEDKey, REG_SWBLINKCTRL_VALUE, &dwSWBlinkCtrl);

    if(dwBlinkable || dwSWBlinkCtrl)
    {
        fStatus = GetDwordFromReg(hEventKey, REG_EVENT_STATE_VALUE, &dwState);

        if(!fStatus)
        {
            //Log the error message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("State not configured in registry for event %s"),
                         szEventName);

            dwReturn = TPR_FAIL;
            goto cleanup;
        }

        if(dwState != LED_BLINK)
        {
            dwTempState = LED_BLINK;
            lRetVal = RegSetValueEx (hEventKey,
                                     REG_EVENT_STATE_VALUE,
                                     0,
                                     REG_DWORD,
                                     (PBYTE)(&dwTempState),
                                     sizeof(dwTempState));

            if(lRetVal !=ERROR_SUCCESS)
            {
                //Log the error message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to temporarily update the state in registry for LED %s"),
                             szLedName);

                dwReturn = TPR_FAIL;
                goto cleanup;
            }
        }

        pLedSupportsInfo->LedNum = dwLedNum;
        fStatus = NLedGetDeviceInfo(NLED_SUPPORTS_INFO_ID, pLedSupportsInfo);

        if(!fStatus)
        {
            //Log the error message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tGetDeviceInfo failed. Last Error = %ld, LED = %u"),
                         GetLastError(),
                         pLedSupportsInfo->LedNum);

            dwReturn = TPR_FAIL;
            goto cleanup;
        }

        //Test if the LED Blinks once event is trigerred
        fStatus = GetDwordFromReg(hEventKey,
                                  REG_EVENT_TOTAL_CYCLE_TIME_VALUE,
                                  (DWORD*)&lTotalCycleTime);

        if(!fStatus)
        {
            lTotalCycleTime = pLedSupportsInfo->lCycleAdjust * (CYCLETIME_CHANGE_VALUE * 2);

            lRetVal = RegSetValueEx (hEventKey,
                                     REG_EVENT_TOTAL_CYCLE_TIME_VALUE,
                                     0,
                                     REG_DWORD,
                                     (PBYTE)(&lTotalCycleTime),
                                     sizeof(lTotalCycleTime));

            if(lRetVal !=ERROR_SUCCESS)
            {
                //Log the error message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to modify registry for LED %s"),
                             szLedName);

                dwReturn = TPR_FAIL;
                goto cleanup;
            }

            fDeleteTotalCycleTime = TRUE;
        }

        fStatus = GetDwordFromReg(hEventKey, REG_EVENT_ON_TIME_VALUE, (DWORD*)&lOnTime);

        if(!fStatus)
        {
            lOnTime = pLedSupportsInfo->lCycleAdjust * (CYCLETIME_CHANGE_VALUE);
            lRetVal = RegSetValueEx (hEventKey,
                                     REG_EVENT_ON_TIME_VALUE,
                                     0,
                                     REG_DWORD,
                                     (PBYTE)(&lOnTime),
                                     sizeof(lOnTime));

            if(lRetVal !=ERROR_SUCCESS)
            {
                //Log the error message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to modify registry for LED %s"),
                             szLedName);

                dwReturn = TPR_FAIL;
                goto cleanup;
            }

            fDeleteOnTime = TRUE;
        }

        fStatus = GetDwordFromReg(hEventKey, REG_EVENT_OFF_TIME_VALUE, (DWORD*)&lOffTime);

        if(!fStatus)
        {
            lOffTime = pLedSupportsInfo->lCycleAdjust * (CYCLETIME_CHANGE_VALUE);
            lRetVal = RegSetValueEx (hEventKey,
                                     REG_EVENT_OFF_TIME_VALUE,
                                     0,
                                     REG_DWORD,
                                     (PBYTE)(&lOffTime),
                                     sizeof(lOffTime));

            if(lRetVal !=ERROR_SUCCESS)
            {
                //Log the error message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to modify registry for LED %s"),
                             szLedName);

                dwReturn = TPR_FAIL;
                goto cleanup;
            }

            fDeleteOffTime = TRUE;
        }

        fStatus = GetDwordFromReg(hEventKey,
                                  REG_EVENT_META_CYCLE_ON_VALUE,
                                  (DWORD*)&MetaCycleOn);

        if(!fStatus)
        {
            MetaCycleOn = META_CYCLE_ON;
            lRetVal = RegSetValueEx (hEventKey,
                                     REG_EVENT_META_CYCLE_ON_VALUE,
                                     0,
                                     REG_DWORD,
                                     (PBYTE)(&MetaCycleOn),
                                     sizeof(MetaCycleOn));

            if(lRetVal !=ERROR_SUCCESS)
            {
                //Log the error message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to modify registry for LED %s"),
                             szLedName);

                dwReturn = TPR_FAIL;
                goto cleanup;
            }

            fDeleteMetaCycleOn = TRUE;
        }

        fStatus = GetDwordFromReg(hEventKey,
                                  REG_EVENT_META_CYCLE_OFF_VALUE,
                                  (DWORD*)&MetaCycleOff);

        if(!fStatus)
        {
            MetaCycleOff = META_CYCLE_OFF;
            lRetVal = RegSetValueEx (hEventKey,
                                     REG_EVENT_META_CYCLE_OFF_VALUE,
                                     0,
                                     REG_DWORD,
                                     (PBYTE)(&MetaCycleOff),
                                     sizeof(MetaCycleOff));

            if(lRetVal != ERROR_SUCCESS)
            {
                //Log the error message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to modify registry for LED %s"),
                             szLedName);

                dwReturn = TPR_FAIL;
                goto cleanup;
            }

            fDeleteMetaCycleOff = TRUE;
        }

        fStatus = RaiseTemporaryEvent(hLEDKey,
                                      hEventKey,
                                      pLedSettingsInfoAfterEvent);

        if(!fStatus)
        {
            //Log the error message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Service not able to blink the LED event %s"),
                         szEventName);

            dwReturn = TPR_FAIL;
            goto cleanup;
        }
        else
        {
            //Check if the LED settings are as per the entried made. Also check for settings that are supported and not all.
            if(pLedSupportsInfo->fAdjustTotalCycleTime)
            {
                if(pLedSettingsInfoAfterEvent->TotalCycleTime == lTotalCycleTime)
                {
                    dwReturn = TPR_PASS;
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("TotalCycleTime setting from test and actually set are different for LED-%s.Test val=%u.Driver Returned=%u"),
                                 szLedName,
                                 lTotalCycleTime,
                                 pLedSettingsInfoAfterEvent->TotalCycleTime);

                    dwReturn = TPR_FAIL;
                    goto cleanup;
                }
            }
            if(pLedSupportsInfo->fAdjustOnTime)
            {
                if(pLedSettingsInfoAfterEvent->OnTime == lOnTime)
                {
                    dwReturn = TPR_PASS;
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("OnTime setting returned by driver and the one actually set are different for LED-%s.Actual set val=%u.Driver Returned=%u"),
                                 szLedName,
                                 lOnTime,
                                 pLedSettingsInfoAfterEvent->OnTime);

                    dwReturn = TPR_FAIL;
                    goto cleanup;
                }
            }
            if(pLedSupportsInfo->fAdjustOffTime)
            {
                if(pLedSettingsInfoAfterEvent->OffTime == lOffTime)
                {
                    dwReturn = TPR_PASS;
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("OffTime setting returned by driver and the one actually set are different for LED-%s.Actual set val=%u.Driver Returned=%u"),
                                 szLedName,
                                 lOffTime,
                                 pLedSettingsInfoAfterEvent->OffTime);

                    dwReturn = TPR_FAIL;
                    goto cleanup;
                }
            }
            if(pLedSupportsInfo->fMetaCycleOn)
            {
                if(pLedSettingsInfoAfterEvent->MetaCycleOn == MetaCycleOn)
                {
                    dwReturn = TPR_PASS;
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("MetaCycleOn setting returned by driver and the one actually set are different for LED-%s.Actual set val=%u.Driver Returned=%u"),
                                 szLedName,
                                 MetaCycleOn,
                                 pLedSettingsInfoAfterEvent->MetaCycleOn);

                    dwReturn = TPR_FAIL;
                    goto cleanup;
                }
            }
            if(pLedSupportsInfo->fMetaCycleOff)
            {
                if(pLedSettingsInfoAfterEvent->MetaCycleOff == MetaCycleOff)
                {
                    dwReturn = TPR_PASS;
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("MetaCycle Off setting returned by driver and the one actually set are different for LED-%s.Actual set val=%u.Driver Returned=%u"),
                                 szLedName, MetaCycleOff,
                                 pLedSettingsInfoAfterEvent->MetaCycleOn);

                    dwReturn = TPR_FAIL;
                    goto cleanup;
                }
            }
         }
         cleanup:

            //Set the state back to the original value
            if(dwState != LED_BLINK)
            {
                lRetVal = RegSetValueEx (hEventKey,
                                         REG_EVENT_STATE_VALUE,
                                         0,
                                         REG_DWORD,
                                         (PBYTE)(&dwState),
                                         sizeof(dwState));

                if(lRetVal != ERROR_SUCCESS)
                {
                    //Log the error message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to temporarily update the state in registry for LED %s"),
                                 szLedName);

                    dwReturn = TPR_FAIL;
                    goto cleanup;
                }
            }

            if(fDeleteTotalCycleTime)
            {
                RegDeleteValue(hEventKey, REG_EVENT_TOTAL_CYCLE_TIME_VALUE);
            }

            if(fDeleteOnTime)
            {
                RegDeleteValue(hEventKey, REG_EVENT_ON_TIME_VALUE);
            }

            if(fDeleteOffTime)
            {
                RegDeleteValue(hEventKey, REG_EVENT_OFF_TIME_VALUE);
            }

            if(fDeleteMetaCycleOn)
            {
                RegDeleteValue(hEventKey, REG_EVENT_META_CYCLE_ON_VALUE);
            }

            if(fDeleteMetaCycleOff)
            {
                RegDeleteValue(hEventKey, REG_EVENT_META_CYCLE_OFF_VALUE);
            }

            if(pLedSettingsInfoAfterEvent != NULL)
            {
                delete[] pLedSettingsInfoAfterEvent;
                pLedSettingsInfoAfterEvent = NULL;
            }

            if(pLedSupportsInfo != NULL)
            {
                delete[] pLedSupportsInfo;
                pLedSupportsInfo = NULL;
            }
     }
     return dwReturn;
}

//*****************************************************************************
BOOL RaiseTemporaryEvent(HKEY hLEDKey,
                         HKEY hEventSubKey,
                         NLED_SETTINGS_INFO* pLedSettingsInfoAfterEvent)
//
// Parameters:   NLED Key,
//               Event Sub Key,
//               NLED Settings Info
//
// Return Value: TRUE on Success
//                FALSE on failure
//
// To raise a temporary event and to test if the LED corresponding to the
// temporary event has changed its state or not.
//
//*****************************************************************************
{
    //Declaring the variables
    BYTE  *bpPath = NULL;
    BYTE  *bpValueName = NULL;
    DWORD dwEventRoot = 0;
    DWORD dwMask = 0;
    DWORD dwValue = 0;
    DWORD dwTempValue = 0;
    DWORD dwOperator = 0;
    INT   State = 0;
    DWORD dwLedNum = 0;
    DWORD dwTempEventPathLen = 0;
    HKEY  hTempoEventKey = NULL;
    DWORD dwDisp = 0;
    LONG  lRetVal = 0;
    BOOL  fEventTriggered = FALSE;
    BOOL  fStatus = FALSE;
    NLED_SETTINGS_INFO *pLedSettingsInfo  = NULL;
    pLedSettingsInfo = new NLED_SETTINGS_INFO[1];

    if(!pLedSettingsInfo)
    {
        //Log error message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tMemory allocation failed. Last Error = %ld"),
                     GetLastError());

        pLedSettingsInfo = NULL;
        return FALSE;
    }

     ZeroMemory(pLedSettingsInfo, sizeof(NLED_SETTINGS_INFO));

    // Read the existing event
    fStatus = GetDwordFromReg(hEventSubKey, REG_EVENT_ROOT_VALUE, &dwEventRoot);

    if(!fStatus)
    {
        //Log error message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Root detail not available in registry"));

        delete[] pLedSettingsInfo;
        pLedSettingsInfo = NULL;
        return FALSE;
    }

    fStatus = GetStringFromReg(hEventSubKey, REG_EVENT_PATH_VALUE, &bpPath);

    if(!fStatus)
    {
        //Log error message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Path detail not available in registry"));

        delete[] pLedSettingsInfo;
        pLedSettingsInfo = NULL;
        return FALSE;
    }

    fStatus = GetStringFromReg(hEventSubKey,
                               REG_EVENT_VALUE_NAME_VALUE,
                               &bpValueName);

    if(!fStatus)
    {
        //Log error message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("ValueName detail not available in registry"));

        delete[] pLedSettingsInfo;
        pLedSettingsInfo = NULL;
        return FALSE;
    }

    fStatus = GetDwordFromReg(hEventSubKey, REG_EVENT_MASK_VALUE, &dwMask);

    if(!fStatus)
    {
        //Log error message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Mask detail not available in registry"));

        delete[] pLedSettingsInfo;
        pLedSettingsInfo = NULL;
        return FALSE;
    }

    fStatus = GetDwordFromReg(hEventSubKey, REG_EVENT_VALUE_VALUE, &dwValue);

    if(!fStatus)
    {
        //Log error message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Value detail not available in registry"));

        delete[] pLedSettingsInfo;
        pLedSettingsInfo = NULL;
        return FALSE;
    }

    fStatus = GetDwordFromReg(hEventSubKey, REG_EVENT_OPERATOR_VALUE, &dwOperator);

    if(!fStatus)
    {
        dwOperator = 0;
    }

    fStatus = GetDwordFromReg(hEventSubKey, REG_EVENT_STATE_VALUE, (DWORD*)&State);

    if(!fStatus)
    {
        //Log error message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("State detail not available in registry"));

        delete[] pLedSettingsInfo;
        pLedSettingsInfo = NULL;
        return FALSE;
    }

    fStatus = GetDwordFromReg(hLEDKey, REG_LED_NUM_VALUE, &dwLedNum);

    if(!fStatus)
    {
        //Log error message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("LedNum detail not available in registry"));

        delete[] pLedSettingsInfo;
        pLedSettingsInfo = NULL;
        return FALSE;
    }

    //Create a temporary trigger
    lRetVal = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                             TEMPO_EVENT_PATH,
                             0,
                             NULL,
                             REG_OPTION_VOLATILE,
                             KEY_ALL_ACCESS,
                             NULL,
                             &hTempoEventKey,
                             &dwDisp);

    if(lRetVal !=ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to create a temporary event in registry"));

        fEventTriggered = FALSE;
        goto cleanup;
    }

    //Do not want to trigger right now, so set the trigger value such that it does not
    //trigger now
    switch(dwOperator)
    {
        case 0:
            dwTempValue = 0;
            break;
        case 1:
            dwTempValue = 0;
            break;
        case 2:
            dwTempValue = 0;
            break;
        case 3:
            dwTempValue = dwValue;
            break;
        case 4:
            for(DWORD dwCount = 0; dwCount < 0xFFFFFFFF; dwCount++)
            {
                if((dwCount * dwMask) > dwValue)
                {
                    dwTempValue = dwCount;
                    break;
                }
            }
            break;
        default:
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Invalid Operator value in registry for LED Num:%u"),
                         dwLedNum);

            fEventTriggered = FALSE;
            goto cleanup;
    }

    lRetVal = RegSetValueEx (hTempoEventKey,
                             (LPCWSTR)bpValueName,
                             0,
                             REG_DWORD,
                             (PBYTE)(&dwTempValue),
                             sizeof(dwTempValue));

    if(lRetVal !=ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to update the temporary event in registry"));

        fEventTriggered = FALSE;
        goto cleanup;
    }

    //Replace original trigger path for the specified LED with the temporary trigger path
    dwTempEventPathLen = (wcslen(TEMPO_EVENT_PATH)+1)*sizeof(WCHAR);

    lRetVal = RegSetValueEx (hEventSubKey,
                             REG_EVENT_PATH_VALUE,
                             0,
                             REG_SZ,
                             (PBYTE)TEMPO_EVENT_PATH,
                             dwTempEventPathLen);

    if(lRetVal !=ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to change the Path to the temporary event path"));

        fEventTriggered = FALSE;
        goto cleanup;
    }

    //Unload/load service
    fStatus = UnloadLoadService();

    //Wait for 1 second
    Sleep(SLEEP_FOR_ONE_SECOND);

    if(!fStatus)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to unload and load the Nled Service"));

        fEventTriggered = FALSE;
        goto cleanup;
    }

    //Raise the trigger
    switch(dwOperator)
    {
        case 0:
            dwTempValue = dwValue;
            break;
        case 1:
            for(DWORD dwCount = 0; dwCount < 0xFFFFFFFF; dwCount++)
            {
                if((dwCount * dwMask) > dwValue)
                {
                    dwTempValue = dwCount;
                    break;
                }
            }
            break;
        case 2:
            dwTempValue = dwValue;
            break;
        case 3:
            dwTempValue = 0;
            break;
        case 4:
            dwTempValue = dwValue;
            break;
    }

    lRetVal = RegSetValueEx (hTempoEventKey,
                             (LPCWSTR)bpValueName,
                             0,
                             REG_DWORD,
                             (PBYTE)(&dwTempValue),
                             sizeof(dwTempValue));

    if(lRetVal !=ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to raise the trigger from test"));

        fEventTriggered = FALSE;
        goto cleanup;
    }

    //Wait for 1 second for the service to act
    Sleep(SLEEP_FOR_ONE_SECOND);

    ZeroMemory(pLedSettingsInfo, sizeof(NLED_SETTINGS_INFO));
    pLedSettingsInfo->LedNum = dwLedNum;
    fStatus = NLedGetDeviceInfo(NLED_SETTINGS_INFO_ID, pLedSettingsInfo);

    if(!fStatus)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tNLedGetDeviceInfo failed. Last Error = %ld, LED = %u"),
                     GetLastError(),
                     pLedSettingsInfo->LedNum);

        fEventTriggered = FALSE;
        goto cleanup;
    }

    //Pass the driver setting after raising the event.This could be used for some analysis.
    if(pLedSettingsInfoAfterEvent != NULL)
    {
        pLedSettingsInfoAfterEvent->LedNum             = pLedSettingsInfo->LedNum;
        pLedSettingsInfoAfterEvent->OffOnBlink         = pLedSettingsInfo->OffOnBlink;
        pLedSettingsInfoAfterEvent->TotalCycleTime     = pLedSettingsInfo->TotalCycleTime;
        pLedSettingsInfoAfterEvent->OnTime             = pLedSettingsInfo->OnTime;
        pLedSettingsInfoAfterEvent->OffTime            = pLedSettingsInfo->OffTime;
        pLedSettingsInfoAfterEvent->MetaCycleOn        = pLedSettingsInfo->MetaCycleOn;
        pLedSettingsInfoAfterEvent->MetaCycleOff       = pLedSettingsInfo->MetaCycleOff;
    }

    // Analyze the result
    if(pLedSettingsInfo->OffOnBlink == State)
    {
        fEventTriggered = TRUE;
    }
    else
    {
        fEventTriggered = FALSE;
    }

    // Reset the raised event and restore the trigger path
    switch(dwOperator)
    {
        case 0:
            dwTempValue = 0;
            break;
        case 1:
            dwTempValue = 0;
            break;
        case 2:
            dwTempValue = 0;
            break;
        case 3:
            dwTempValue = dwValue;
            break;
        case 4:

            for(DWORD dwCount = 0; dwCount < 0xFFFFFFFF; dwCount++)
            {
                if((dwCount * dwMask) > dwValue)
                {
                    dwTempValue = dwCount;
                    break;
                }
            }

            break;
    }

    lRetVal = RegSetValueEx (hTempoEventKey,
                            (LPCWSTR)bpValueName,
                            0,
                            REG_DWORD,
                            (PBYTE)(&dwTempValue),
                            sizeof(dwTempValue));

    if(lRetVal !=ERROR_SUCCESS)
    {
        //Log error message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to reset the trigger from test"));

        fEventTriggered = FALSE;
        goto cleanup;
    }

    dwTempEventPathLen = (wcslen((WCHAR *)bpPath) + 1) * sizeof(WCHAR);
    lRetVal = RegSetValueEx (hEventSubKey,
                             REG_EVENT_PATH_VALUE,
                             0,
                             REG_SZ,
                             (PBYTE)bpPath,
                             dwTempEventPathLen);

    if(lRetVal !=ERROR_SUCCESS)
    {
        //Log error message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to reset the path to original value"));

        fEventTriggered = FALSE;
        goto cleanup;
    }

cleanup:

    lRetVal = RegCloseKey(hTempoEventKey);

    if(lRetVal!=ERROR_SUCCESS)
    {
        //Log error message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Temporary Event Key closing failed. tLast Error = %ld, LED = %u"),
                     GetLastError(),
                     pLedSettingsInfo->LedNum);

        fEventTriggered = FALSE;
    }

    lRetVal= RegDeleteKey(HKEY_LOCAL_MACHINE, TEMPO_EVENT_PATH);

    if(lRetVal != ERROR_SUCCESS)
    {
        //Log error message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tTemporary Event Key deletion failed. Last Error = %ld, LED = %u"),
                     GetLastError(),
                     pLedSettingsInfo->LedNum);

        fEventTriggered = FALSE;
    }

    //De-allocating the memory
    if(bpPath != NULL)
    {
        GetStringFromReg_FreeMem(&bpPath);
        bpPath = NULL;
    }

    if(bpValueName != NULL)
    {
        GetStringFromReg_FreeMem(&bpValueName);
        bpValueName = NULL;
    }

    if(pLedSettingsInfo != NULL)
    {
        delete[] pLedSettingsInfo;
        pLedSettingsInfo = NULL;
    }

    // return result
    return fEventTriggered;
}

//*****************************************************************************
BOOL        GetStringFromReg(HKEY hKey,
                             LPCWSTR pszName,
                             BYTE **ppszValue)
//
// Parameters:   Handle to a reigstry key,
//               Name of the value to retrieve,
//               Out Parameter to store the value
//
// Return Value: TRUE on Success
//               FALSE on failure
//
// Function to get a string value from the registry
//
//*****************************************************************************
{
    //Declaring the variables
    LONG  lRetVal = 0;
    DWORD dwDataSize = 0;
    DWORD dwType = 0;
    BOOL  fReturn = FALSE;

    // First get the size of the string
    lRetVal = RegQueryValueEx(hKey, pszName, NULL, &dwType, NULL, &dwDataSize);

    if (lRetVal != ERROR_SUCCESS || dwDataSize == 0 || dwType != REG_SZ)
    {
        // The key does not exist
        goto cleanup;
    }

    *ppszValue = new BYTE[dwDataSize + 1];

    if (*ppszValue == NULL)
    {
        goto cleanup;
    }

    lRetVal = RegQueryValueEx(hKey, pszName, NULL, &dwType, *ppszValue, &dwDataSize);

    if (lRetVal != ERROR_SUCCESS || dwType != REG_SZ)
    {
        // If the dwType changed, some hacker is trying to do something bad
        delete[] *ppszValue;
        *ppszValue = NULL;
        goto cleanup;
    }

    fReturn = TRUE;

cleanup:
    return fReturn;
}

//*****************************************************************************
BOOL GetStringFromReg_FreeMem(BYTE **ppszValue)
//
// Parameters: Pointer to be freed
//
// Return Value:   TRUE on Success
//                 FALSE on failure
//
// Function tp de-allocate the memory
//*****************************************************************************
{
        if(*ppszValue != NULL)
        {
            delete[] *ppszValue;
            *ppszValue = NULL;
            return TRUE;
        }
        else
            return FALSE;
}

//*****************************************************************************
BOOL GetDwordFromReg(HKEY hKey,
                     LPCWSTR pszName,
                     DWORD *pValue)
//
// Parameters:   Handle to a reigstry key,
//               Name of the value to retrieve,
//               Out Parameter to store the value
//
// Return Value: TRUE on Success
//               FALSE on failure
//
// Function to get a DWORD value from the registry
//
//*****************************************************************************
{
    //Declaring the variables
    LONG  lRetVal = 0;
    DWORD dwData = 0;
    DWORD dwDataSize = sizeof(DWORD);

    lRetVal = RegQueryValueEx(hKey,
                              pszName,
                              NULL,
                              NULL,
                              (PBYTE)(&dwData),
                              &dwDataSize);

    if (ERROR_SUCCESS == lRetVal)
    {
        *pValue = dwData;
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

//*****************************************************************************
BOOL UnloadLoadService()
//
// Parameters: NONE
//
// Return Value:   TRUE on Success
//                 FALSE on failure
//
// Function to unload and reload the Nled service
//
//*****************************************************************************
{
    //Declaring the variables
    DWORD   dwNumservices = 0;
    DWORD   pdwBufferLen  = 5000;
    HANDLE  hService      = NULL;
    BOOL    fStatus       = FALSE;
    BYTE*   pByte         = new BYTE[5000];

    //Enumerate Services currently active on device
    EnumServices(pByte, &dwNumservices, &pdwBufferLen);

    if(pByte != NULL)
    delete pByte;

    hService = CreateFile(L"NLS0:",
                          GENERIC_WRITE|GENERIC_READ,
                          NULL,
                          NULL,
                          OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL,
                          NULL);

    if(INVALID_HANDLE_VALUE == hService)
    {
        //log error message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tLast Error = %ld"),
                     GetLastError());

        return FALSE;
    }

    //UnLoading the Nled Service
    fStatus = DeregisterService(hService);

    if(!fStatus)
    {
        //log error message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tService Unloading Failed. Last Error = %ld"),
                     GetLastError());

        return FALSE;
    }

    g_pKato->Log(LOG_DETAIL,
                 TEXT("SERVICE UN-LOADED"));

    //Waiting for 5 seconds
    Sleep(SLEEP_FOR_FIVE_SECONDS);
    hService = ActivateService(L"NLEDSRV", 0);

    if(hService == NULL)
    {
        //log error message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tService loading Failed. Last Error = %ld"),
                     GetLastError());

        return FALSE;
    }

    g_pKato->Log(LOG_DETAIL,
                 TEXT("SERVICE LOADED Again"));

    return TRUE;
}

//*****************************************************************************
BOOL ReadEventDetails(HKEY hLEDKey,
                      HKEY hEventSubKey,
                      NLED_EVENT_DETAILS* pnledEventDetailsStruct)
//
// Parameters: Nled Key, Event Key, Event Details Structure
//
// Return Value:   TRUE on Success
//                 FALSE on failure
//
// To read the events details from registry and store in the details in the
// event details structure
//
//*****************************************************************************
{
    if(!GetDwordFromReg(hEventSubKey,
                        REG_EVENT_ROOT_VALUE,
                        &(pnledEventDetailsStruct->dwEventRoot)))
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Root detail not available in registry"));

        return FALSE;
    }

    if(!GetStringFromReg(hEventSubKey,
                         REG_EVENT_PATH_VALUE,
                         &(pnledEventDetailsStruct->bpPath)))
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Path detail not available in registry"));

        return FALSE;
    }

    if(!GetStringFromReg(hEventSubKey,
                         REG_EVENT_VALUE_NAME_VALUE,
                         &(pnledEventDetailsStruct->bpValueName)))
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("ValueName detail not available in registry"));

        return FALSE;
    }

    if(!GetDwordFromReg(hEventSubKey,
                        REG_EVENT_MASK_VALUE,
                        &(pnledEventDetailsStruct->dwMask)))
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Mask detail not available in registry"));

        return FALSE;
    }

    if(!GetDwordFromReg(hEventSubKey,
                        REG_EVENT_VALUE_VALUE,
                        &(pnledEventDetailsStruct->dwValue)))
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Value detail not available in registry"));

        return FALSE;
    }

    if(!GetDwordFromReg(hEventSubKey,
                        REG_EVENT_OPERATOR_VALUE,
                        &(pnledEventDetailsStruct->dwOperator)))
    {
        //Operator is not a mandatory value, so if it is not avaiable it should default to 0(meaning ==)
        pnledEventDetailsStruct->dwOperator = 0;
    }

    if(!GetDwordFromReg(hEventSubKey,
                        REG_EVENT_STATE_VALUE,
                        &(pnledEventDetailsStruct->dwState)))
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("State detail not available in registry"));

        return FALSE;
    }

    if(!GetDwordFromReg(hEventSubKey,
                        REG_EVENT_PRIO_VALUE,
                        &(pnledEventDetailsStruct->dwPrio)))
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Priority detail not available in registry"));

        return FALSE;
    }

    if(!GetDwordFromReg(hLEDKey,
                        REG_LED_NUM_VALUE,
                        &(pnledEventDetailsStruct->dwLedNum)))
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("LedNum detail not available in registry"));

        return FALSE;
    }

    return TRUE;
}

//*****************************************************************************
BOOL Create_Set_TemporaryTrigger(HKEY hEventSubKey,
                                 WCHAR* strTempoEventPath,
                                 NLED_EVENT_DETAILS* pnledEventDetailsStruct,
                                 HKEY* phTempoEventKey)
//
// Parameters: Event Key, Temporary Event Path, Event Details structure
//
// Return Value:   TRUE on Success
//                 FALSE on failure
//
// Function to create the temporary event
//*****************************************************************************
{
    //Create a temporary trigger
    LONG lRetVal = 0;
    DWORD dwTempValue = 0;
    DWORD dwDisp = 0;
    DWORD dwTempEventPathLen = 0;
    BOOL fEventSet = TRUE;
    HKEY hTempoEventKey = NULL;

    lRetVal = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                             strTempoEventPath,
                             0,
                             NULL,
                             REG_OPTION_VOLATILE,
                             KEY_ALL_ACCESS,
                             NULL,
                             &hTempoEventKey,
                             &dwDisp);

    if(lRetVal !=ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to create a temporary event in registry"));

        return FALSE;
    }

    //Do not want to trigger right now, so set the trigger value such that it does not
    //trigger now
    switch(pnledEventDetailsStruct->dwOperator)
    {
        case 0:
            dwTempValue = 0;
            break;
        case 1:
            dwTempValue = 0;
            break;
        case 2:
            dwTempValue = 0;
            break;
        case 3:
            dwTempValue = pnledEventDetailsStruct->dwValue;
            break;
        case 4:
            for(DWORD dwCount = 0; dwCount < 0xFFFFFFFF; dwCount++)
            {
                if((dwCount * pnledEventDetailsStruct->dwMask) >
                    pnledEventDetailsStruct->dwValue)
                {
                    dwTempValue = dwCount;
                    break;
                }
            }

            break;
        default:
              g_pKato->Log(LOG_DETAIL,
                           TEXT("Invalid Operator value in registry for LED Num:%u"),
                           pnledEventDetailsStruct->dwLedNum);

            fEventSet = FALSE;
            goto cleanup;
    }

    lRetVal = RegSetValueEx(hTempoEventKey,
                            (LPCWSTR)pnledEventDetailsStruct->bpValueName,
                            0,
                            REG_DWORD,
                            (PBYTE)(&dwTempValue),
                            sizeof(dwTempValue));

    if(lRetVal !=ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to update the temporary event in registry"));

        fEventSet  = FALSE;
        goto cleanup;
    }

    //Replace original trigger path for the specified LED with the temporary trigger path
    dwTempEventPathLen = (wcslen(strTempoEventPath)+1)*sizeof(WCHAR);

    lRetVal = RegSetValueEx (hEventSubKey,
                             REG_EVENT_PATH_VALUE,
                             0,
                             REG_SZ,
                             (PBYTE)strTempoEventPath,
                             dwTempEventPathLen);

    if(lRetVal !=ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to change the Path to the temporary event path"));

        fEventSet  = FALSE;
        goto cleanup;
    }

cleanup:

    if(fEventSet)
    {
        *phTempoEventKey = hTempoEventKey;
        return TRUE;
    }
    else
    {
        if(RegCloseKey(hTempoEventKey)!=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Close Failed. Last Error = %ld, LED = %u"),
                         GetLastError(),
                         pnledEventDetailsStruct->dwLedNum);

            fEventSet  = FALSE;

        }

        if(RegDeleteKey(HKEY_LOCAL_MACHINE, strTempoEventPath)!=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Deletion Failed. Last Error = %ld, LED = %u"),
                         GetLastError(),
                         pnledEventDetailsStruct->dwLedNum);

            fEventSet  = FALSE;
        }

        return FALSE;
    }
}

//*****************************************************************************
BOOL RaiseTemporaryTrigger(HKEY hTempoEventKey,
                           NLED_EVENT_DETAILS* pnledEventDetailsStruct)
//
// Parameters: Temporary Event Key, Event Details Structure
//
// Return Value:   TRUE on Success
//                 FALSE on failure
//
// Function to raise the event temporarily
//*****************************************************************************
{
    //Declaring the variables
    DWORD dwTempValue = 0;
    LONG lRetVal = 0;

    switch(pnledEventDetailsStruct->dwOperator)
    {
        case 0:
            dwTempValue = pnledEventDetailsStruct->dwValue;
            break;
        case 1:
            for(DWORD dwCount = 0; dwCount < 0xFFFFFFFF; dwCount++)
            {
                if((dwCount * pnledEventDetailsStruct->dwMask) >
                    pnledEventDetailsStruct->dwValue)
                {
                    dwTempValue = dwCount;
                    break;
                }
            }
            break;
        case 2:
            dwTempValue = pnledEventDetailsStruct->dwValue;
            break;
        case 3:
            dwTempValue = 0;
            break;
        case 4:
            dwTempValue = pnledEventDetailsStruct->dwValue;
            break;
    }

    lRetVal = RegSetValueEx (hTempoEventKey,
                            (LPCWSTR)pnledEventDetailsStruct->bpValueName,
                            0,
                            REG_DWORD,
                            (PBYTE)(&dwTempValue),
                            sizeof(dwTempValue));

    if(lRetVal !=ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to raise the trigger from test"));

        return FALSE;
    }

    return TRUE;
}

//*****************************************************************************
BOOL GetCurrentNLEDDriverSettings(NLED_EVENT_DETAILS nledEventDetailsStruct,
                                  NLED_SETTINGS_INFO* pLedSettingsInfo)
//
// Parameters: Event Details Structure, Nled Settings Info Structure
//
// Return Value:   TRUE on Success
//                 FALSE on failure
//
// Function to get the Nled current settings
//*****************************************************************************
{
    ZeroMemory(pLedSettingsInfo, sizeof(NLED_SETTINGS_INFO));
    pLedSettingsInfo->LedNum = nledEventDetailsStruct.dwLedNum;

    if(!NLedGetDeviceInfo(NLED_SETTINGS_INFO_ID, pLedSettingsInfo))
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tGetDeviceInfo failed. Last Error = %ld, LED = %u"),
                     GetLastError(),
                     pLedSettingsInfo->LedNum);

        return FALSE;
    }

    return TRUE;
}

//*****************************************************************************
BOOL Unset_TemporaryTrigger(HKEY hEventSubKey, HKEY hTempoEventKey,
                            NLED_EVENT_DETAILS* pnledEventDetailsStruct)
//
// Parameters: Event Key, Temporary Event Key, Event Details Structure
//
// Return Value:   TRUE on Success
//                 FALSE on failure
//
// Function to unset the raised event.
//*****************************************************************************
{
    //Declaring the variables
    DWORD dwTempValue = 0;
    DWORD dwTempEventPathLen = 0;
    LONG lRetVal = 0;

    if(pnledEventDetailsStruct != NULL)
    {
        switch(pnledEventDetailsStruct->dwOperator)
        {
            case 0:
                dwTempValue = pnledEventDetailsStruct->dwValue + 1;
                break;
            case 1:
                dwTempValue = 0;
                break;
            case 2:
                dwTempValue = pnledEventDetailsStruct->dwValue + 1;
                break;
            case 3:
                dwTempValue = pnledEventDetailsStruct->dwMask;
                break;
            case 4:
                dwTempValue = pnledEventDetailsStruct->dwValue + 1;
                break;
        }

        if(hTempoEventKey != NULL)
        {
            lRetVal = RegSetValueEx (hTempoEventKey,
                                     (LPCWSTR)pnledEventDetailsStruct->bpValueName,
                                     0,
                                     REG_DWORD,
                                     (PBYTE)(&dwTempValue),
                                     sizeof(dwTempValue));

            if(lRetVal !=ERROR_SUCCESS)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to reset the trigger from test"));
            }
        }

        if(pnledEventDetailsStruct->bpPath != NULL)
        {
            dwTempEventPathLen = (wcslen((WCHAR *)(pnledEventDetailsStruct->bpPath)) + 1) *
                                                   sizeof(WCHAR);

            if(hEventSubKey != NULL)
            {
                lRetVal = RegSetValueEx (hEventSubKey,
                                         REG_EVENT_PATH_VALUE,
                                         0,
                                         REG_SZ,
                                         (PBYTE)pnledEventDetailsStruct->bpPath,
                                         dwTempEventPathLen);

                if(lRetVal !=ERROR_SUCCESS)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to reset the path to original value"));
                }
            }
        }
    }

    return TRUE;
}

//*****************************************************************************
BOOL DeleteTemporaryTrigger(HKEY hTempoEventKey,
                            WCHAR* strTempoEventPath,
                            NLED_SETTINGS_INFO* pLedSettingsInfo,
                            NLED_EVENT_DETAILS* pnledEventDetailsStruct)
//
// Parameters: Temporary Event Key, Temporary Event Path, Nled Settings Structure
//             Nled Event Details Structure
//
// Return Value:   TRUE on Success
//                 FALSE on failure
//
// Function to Delete the temporary trigger
//*****************************************************************************
{
    //Declaring the variables
    BOOL fRetVal = FALSE;

    if(hTempoEventKey != NULL)
    {
        if(RegCloseKey(hTempoEventKey)!=ERROR_SUCCESS)
        {
            if(pLedSettingsInfo != NULL)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("\tTemp Registry Close Failed. Last Error = %ld, LED = %u"),
                             GetLastError(),
                             pLedSettingsInfo->LedNum);
            }

            fRetVal = FALSE;
        }

        if(RegDeleteKey(HKEY_LOCAL_MACHINE, strTempoEventPath)!=ERROR_SUCCESS)
        {
            if(pLedSettingsInfo != NULL)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("\tTemp Registry Deletion Failed. Last Error = %ld, LED = %u"),
                             GetLastError(),
                             pLedSettingsInfo->LedNum);
            }

            fRetVal = FALSE;
        }
        else
        {
            fRetVal = TRUE;
        }

        if(pnledEventDetailsStruct != NULL)
        {
            GetStringFromReg_FreeMem(&(pnledEventDetailsStruct->bpPath));
            GetStringFromReg_FreeMem(&(pnledEventDetailsStruct->bpValueName));
        }
    }

    return TRUE;
}

//********************************************************************************************************
TESTPROCAPI NledServiceRobustnessTests(UINT                   uMsg,
                                       TPPARAM                tpParam,
                                       LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters:      Standard TUX Arguments
//
//  Return Value:    TPR_PASS           on Success,
//                   TPR_FAIL           on Failure
//                   TPR_NOT_HANDLED    If the feature is not supported
//
//  Objective:    To implement the below requirements
//
//  Event – LED mapping – No path is set (or) path does not exists in the registry for an
//  event. Service must handle this scenario gracefully.
//
//  Event – LED mapping – Set a wrong path in registry for an event. Service must handle
//  this scenario gracefully.
//
//  On Time – Set an invalid value for this key. Service must handle this scenario gracefully.
//
//  Off time – Set an invalid value for this key. Service must handle this scenario gracefully.
//
//  Operator- Set invalid value for the operator registry key. Service must handle this scenario
//  gracefully.
//
//  Total cycle time – Set an invalid value for this key. Service must handle this scenario
//  gracefully.
//
//  LED not present – Add entries in registry for the LED’s which are actually not present
//  on the board. Service must handle this scenario gracefully.
//
//  Delete all registry corresponding to LEDs.  Service must handle this scenario gracefully.
//
//  Corrupt the registry with invalid data. Service must handle this gracefully.
//
//*********************************************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Supporting Only Executing the Test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the service or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fServiceTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    //Returning pass if the device does not have any NLEDs
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("The device doesn't have any NLEDs so passing the test"));

        return TPR_PASS;
    }

    //Notify the user regarding the registry being corrupted
    if(g_fInteractive)
    {
        MessageBox(NULL,
                   TEXT("Nled registry is being corrupted to test the Nled Service Robustness tests, Do not perform any critical operations"),
                   TEXT("NLED Service Robustness Tests"), MB_OK);
    }
    else
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Nled registry is being corrupted to test the Nled Service Robustness tests, Do not perform any critical operations"));
    }

    //Declaring the variables
    DWORD dwTestResult = TPR_PASS;
    DWORD dwRetVal = 0;
    DWORD *pLedNumEventId = NULL;
    DWORD dwTotalEvents = 0;
    NLED_EVENT_DETAILS* pEventDetails_list = NULL;
    HKEY* hEventOrigKey = NULL;
    HKEY hTemporaryEventKey = NULL;

    //Storing the original LED settings
    NLED_SETTINGS_INFO* pOriginalLedSettingsInfo = NULL;

    // Allocate space to save origional LED setttings
    SetLastError(ERROR_SUCCESS);
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];

    if(!pOriginalLedSettingsInfo)
    {
        FAIL("Memory allocation failed");
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tLast Error = %d"),
                     GetLastError());

        return TPR_FAIL;
    }

    ZeroMemory(pOriginalLedSettingsInfo, GetLedCount() * sizeof(NLED_SETTINGS_INFO));

    // Save the Info Settings for all the LEDs and turn them off
    dwRetVal = saveAllLedSettings(pOriginalLedSettingsInfo, LED_OFF);

    if(!dwRetVal)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    }

    if(g_fInteractive)
    {
        MessageBox(NULL,
                   TEXT("NLED Service Robustness tests are in Progress, Please do not run any critical applications"), TEXT("NLED Service Robustness Tests"),
                   MB_OK);
    }
    else
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("NLED Service Robustness tests are in Progress, Please do not run any critical applications"));
    }

    //Pointer to an Array to store the Info if the Nled has any events configured
    pLedNumEventId = new DWORD[GetLedCount()];

    //Getting the Event Ids for all the Nleds
    dwRetVal = GetNledEventIds(pLedNumEventId);

    if(TPR_PASS != dwRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to retrieve total number of events for all the Leds."));

        dwTestResult = TPR_FAIL;
        goto RETURN;
    }

    //Calculating the total number of events of all the Leds
    for(DWORD i = 0; i < GetLedCount(); i++)
    {
        dwTotalEvents = dwTotalEvents + pLedNumEventId[i];
    }

    if(0 == dwTotalEvents)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("No LED has at least one event configured.  Skipping the test."));

        dwTestResult = TPR_SKIP;
        goto RETURN;
    }

    //Storing the event paths and unsetting all the events
    pEventDetails_list = new NLED_EVENT_DETAILS[dwTotalEvents];

    for(DWORD i=0;i<dwTotalEvents;i++)
    {
        ZeroMemory(&pEventDetails_list[i], sizeof(NLED_EVENT_DETAILS));
    }

    hEventOrigKey = new HKEY[dwTotalEvents];

    dwRetVal = UnSetAllNledServiceEvents(pEventDetails_list,
                                          &hTemporaryEventKey,
                                         hEventOrigKey);

    if(!dwRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to unset all the Nled Service Events"));

        dwTestResult = TPR_FAIL;
        goto RETURN;
    }

    //Delete the Event Path Test
    dwRetVal = NledServiceDeleteEventPath();

    if (dwRetVal == TPR_FAIL)
    {
        //Log The Error Message
        dwTestResult = TPR_FAIL;
        FAIL("Nled Service Delete Event Path Test Case failed");
    }
    else if(dwRetVal == TPR_SKIP)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("Skipping the Nled Service Delete Event Path Test"));
    }
    else
    {
        SUCCESS("Nled Service Delete Event Path Test Case is Successful");
    }

    //Testing for invalid value for On Time & Off Time
    dwRetVal = NledServiceInvalidOnOffTime();

    if (dwRetVal == TPR_FAIL)
    {
        //Log The Error Message
        FAIL("Invalid On Time and Off Time Test Case failed");
        dwTestResult = TPR_FAIL;
    }
    else if(dwRetVal == TPR_SKIP)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("Skipping the Invalid On Time and Off Time Test Case"));
    }
    else
    {
        SUCCESS("Invalid On Time and Off Time Test Case is Successful");
    }

    //Set an Invalid Value for Event Path
    dwRetVal = NledServiceInvalidEventPath();

    if (dwRetVal == TPR_FAIL)
    {
        //Log The Error Message
        FAIL("Invalid Event Path Test Case failed");
        dwTestResult = TPR_FAIL;
    }
    else if(dwRetVal == TPR_SKIP)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("Skipping the Invalid Event Path Test Case"));
    }
    else
    {
        SUCCESS("Invalid Event Path Test Case is Successful");
    }

    //Set an Invalid Value for Operator Value
    dwRetVal = NledServiceInvalidOperatorValue();

    if (dwRetVal == TPR_FAIL)
    {
        //Log The Error Message
        FAIL("Invalid Operator Test Case failed.");
        dwTestResult = TPR_FAIL;
    }
    else if(dwRetVal == TPR_SKIP)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("Skipping the Invalid Operator Test Case"));
    }
    else
    {
        SUCCESS("Invalid Operator Test Case is Successful.");
    }

    //Set an Invalid value for Total Cycle Time
    dwRetVal = NledServiceInvalidTotalCycleTime ();

    if (dwRetVal == TPR_FAIL)
    {
        //Log The Error Message
        FAIL("Invalid Total Time Test Case failed.");
        dwTestResult = TPR_FAIL;
    }
    else if(dwRetVal == TPR_SKIP)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Skipping the Invalid TotalCycleTime Test Case"));
    }
    else
    {
        SUCCESS("Invalid Total Time Test Case is Successful.");
    }

    //Set an Invalid Value for LedNum
    dwRetVal = NledServiceInvalidLEDNum();

    if(dwRetVal != TPR_PASS)
    {
        //Log The Error Message
        FAIL("Invalid LED NUM Test Case failed.");
        dwTestResult = TPR_FAIL;
    }
    else if(dwRetVal == TPR_SKIP)
    {
        g_pKato->Log(LOG_SKIP, TEXT("Skipping the Invalid LedNum Test Case"));
    }
    else
    {
        SUCCESS("Invalid LED NUM Test Case is Successful.");
    }

    //Delete Led Registry
    dwRetVal = NledServiceDeleteLEDRegistry();

    if (dwRetVal != TPR_PASS)
    {
        //Log The Error Message
        FAIL("Delete LED Registry Test Case failed.");
        dwTestResult = TPR_FAIL;
    }
    else if(dwRetVal == TPR_SKIP)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("Skipping the Delete Led Registry Test Case"));
    }
    else
    {
        SUCCESS("Delete LED Registry Test Case is Successful.");
    }

    //Corrupt LED Registry
    dwRetVal = NledServiceCorruptLEDRegistry ();

    if (dwRetVal != TPR_PASS)
    {
        //Log The Error Message
        FAIL("Corrupt LED Registry Test Case failed.");
        dwTestResult = TPR_FAIL;
    }
    else if(dwRetVal == TPR_SKIP)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("Skipping the Corrupt Led Registry Test Case"));
    }
    else
    {
        SUCCESS("Corrupt LED Registry Test Case is Successful.");
    }

    //All Service Robustness Tests are Successful
    if (dwTestResult == TPR_PASS)
    {
        SUCCESS("All the Nled Service Robustness Tests are successfully Passed");
    }
    else
    {
        FAIL("Nled Service Robustness Tests Failed");
    }

RETURN:
    //Restore back the original Event Paths
    dwRetVal = RestoreAllNledServiceEventPaths(pEventDetails_list,
                                               hEventOrigKey,
                                               dwTotalEvents);

    if(!dwRetVal)
    {
        dwTestResult = TPR_FAIL;
        FAIL("Unable to restore back the original event paths");
    }

    //Delete the temporary registry key
    if(hTemporaryEventKey != NULL)
    {
        dwRetVal =  RegCloseKey(hTemporaryEventKey);

        if(dwRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Closing Failed. Last Error = %ld"),
                         GetLastError());

            dwTestResult = TPR_FAIL;
        }

        dwRetVal = RegDeleteKey(HKEY_LOCAL_MACHINE, TEMPORARY_EVENT_PATH);

        if(dwRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Deletion Failed. Last Error = %ld"),
                         GetLastError());

            dwTestResult = TPR_FAIL;
        }
    }

    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo != NULL)
    {
        dwRetVal = setAllLedSettings(pOriginalLedSettingsInfo);

        if(!dwRetVal)
        {
            dwTestResult = TPR_FAIL;
            FAIL("Unable to restore original NLED settings");
        }
    }

    if(pOriginalLedSettingsInfo != NULL)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    if(pLedNumEventId != NULL)
    {
        delete[] pLedNumEventId;
        pLedNumEventId = NULL;
    }

    if(hEventOrigKey != NULL)
    {
        delete[] hEventOrigKey;
        hEventOrigKey = NULL;
    }

    if(pEventDetails_list != NULL)
    {
        delete[] pEventDetails_list;
        pEventDetails_list = NULL;
    }

    return dwTestResult;
}

//*******************************************************************************
DWORD NledServiceInvalidEventPath()
//
//  Parameters:   NONE
//
//  Return Value: TPR_PASS On Success,
//                TPR_FAIL On Failure
//
//  To set an Invalid Event Path In the Event Registry
//********************************************************************************
{
    //Declaring the varibales
    HKEY hNLEDConfigKey = NULL;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    HKEY hLEDKey = NULL;
    DWORD dwRetVal = TPR_PASS;
    LONG lReturnValue = 0;
    DWORD dwCurLEDIndex = 0;
    DWORD dwLEDNameSize = gc_dwMaxKeyNameLength + 1;
    BOOL fPathChanged = FALSE;
    NLED_EVENT_DETAILS nledEventDetailsStruct = {0};
    HKEY hEventKey = NULL;
    DWORD dwPathLen = sizeof(DWORD);
    DWORD dwCurLEDEventIndex = 0;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;

    //Opening the config Key
    lReturnValue = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                REG_NLED_KEY_CONFIG_PATH,
                                0,
                                0,
                                &hNLEDConfigKey);

    if(lReturnValue != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to open the Config directory for LEDs: Invalid Event Path Test Failed"));

        return TPR_FAIL;
    }

    //Enumerating LED Name
    lReturnValue = RegEnumKeyEx(hNLEDConfigKey,
                                dwCurLEDIndex,
                                szLedName,
                                &dwLEDNameSize,
                                NULL, NULL, NULL, NULL);

    if(ERROR_NO_MORE_ITEMS == lReturnValue)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("No LEDs configured in the registry. Invalid Event Path Test Failed."));

        RegCloseKey(hNLEDConfigKey);
        return TPR_FAIL;
    }

    //Iterating until All LEDS
    while(lReturnValue ==  ERROR_SUCCESS)
    {
        //Open LED Registry
        lReturnValue = RegOpenKeyEx(hNLEDConfigKey,
                                    szLedName,
                                    0,
                                    KEY_ALL_ACCESS,
                                    &hLEDKey);

        if(lReturnValue != ERROR_SUCCESS)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to open key for %s LED. Invalid Event Path Test Failed."),
                         szLedName);

            RegCloseKey(hNLEDConfigKey);
            return TPR_FAIL;
        }
        else
        {
            //Re-Initializing the index variable
            dwCurLEDEventIndex = 0;

            //Enumerating LED Event
            lReturnValue = RegEnumKeyEx(hLEDKey,
                                        dwCurLEDEventIndex,
                                        szEventName,
                                        &dwEventNameSize,
                                        NULL, NULL, NULL, NULL);

            if(ERROR_NO_MORE_ITEMS == lReturnValue)
            {
                //Log Error Message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("No Events Configured for %s LED"),
                             szLedName);

                dwRetVal = TPR_SKIP;
            }

            //Iteration for All Events in the LED.
            while(ERROR_SUCCESS == lReturnValue)
            {
                //Opening Event Key
                lReturnValue = RegOpenKeyEx(hLEDKey,
                                            szEventName,
                                            0,
                                            KEY_ALL_ACCESS,
                                            &hEventKey);

                if(lReturnValue != ERROR_SUCCESS)
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to open Event-key for %s of %s LED. Invalid Event Path Test Failed."),
                                 szEventName,
                                 szLedName);

                    RegCloseKey(hLEDKey);
                    RegCloseKey(hNLEDConfigKey);
                    return TPR_FAIL;
                }

                //Backup the Event Registry Values
                lReturnValue = ReadEventDetails(hLEDKey,
                                                hEventKey,
                                                &nledEventDetailsStruct);

                if (!lReturnValue)
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to Read Event Details for %s of %s LED. Invalid Event Path Test Failed."),
                                 szEventName,
                                 szLedName);

                    dwRetVal = TPR_FAIL;
                    goto end;
                }

                //Set the InvalidPath
                dwPathLen = (wcslen(INVALID_EVENT_PATH)+1)*sizeof(WCHAR);

                lReturnValue = RegSetValueEx (hEventKey,
                                              REG_EVENT_PATH_VALUE,
                                              0,
                                              REG_SZ,
                                              (PBYTE)INVALID_EVENT_PATH,
                                              dwPathLen);

                if(lReturnValue != ERROR_SUCCESS)
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable To Set Invalid Path: Invalid Event Path Test Failed"));

                    dwRetVal = TPR_FAIL;
                    goto end;
                }
                else
                {
                    fPathChanged = TRUE;
                }

                //Unload and Load Service for affecting updated Registry values.
                if(!UnloadLoadService())
                {
                    dwRetVal = TPR_FAIL;
                    FAIL("Failed to UnLoad-Load the Nled Service");
                    goto end;
                }

                //Wait for 1 second for the service to act on the trigger
                Sleep(SLEEP_FOR_ONE_SECOND);

                //Check if the Service is Still Running
                LPWSTR lpServiceName = L"nledsrv.dll";

                if(!IsServiceRunning(lpServiceName))
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Service is not running after setting an Invalid path for %s Event"),
                                 szLedName);

                    dwRetVal = TPR_FAIL;
                }
                else
                {
                    dwRetVal = TPR_PASS;
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Nled Service is still running even after setting an Invalid path for %s Event"),
                                 szLedName);
                }

                //Perform the test only for one event
                goto end;
            }
        }

        RegCloseKey(hLEDKey);

        //Next LED.
        dwCurLEDIndex++;
        dwLEDNameSize = gc_dwMaxKeyNameLength + 1;

        //Enumerating for Next LED.
        lReturnValue = RegEnumKeyEx(hNLEDConfigKey,
                                    dwCurLEDIndex,
                                    szLedName,
                                    &dwLEDNameSize,
                                    NULL, NULL, NULL, NULL);
    }

end:

    //Restore Valid Path
    if(fPathChanged)
    {
        lReturnValue = RegSetValueEx(hEventKey,
                                     REG_EVENT_PATH_VALUE,
                                     0,
                                     REG_SZ,
                                     (PBYTE) nledEventDetailsStruct.bpPath,
                                     dwPathLen);

        if(lReturnValue != ERROR_SUCCESS)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable To Restore Valid Path. Invalid Event Path Test Failed."));

            dwRetVal = TPR_FAIL;
        }

        //Unload and Load Service for affecting updated Registry values.
        if(!UnloadLoadService())
        {
            dwRetVal = TPR_FAIL;
            FAIL("Failed to UnLoad-Load the Nled Service");
        }
    }

    //Close Config Key.
    if(hNLEDConfigKey != NULL)
        RegCloseKey(hNLEDConfigKey);

    if(hLEDKey != NULL)
        RegCloseKey(hLEDKey);

    if(hEventKey != NULL)
        RegCloseKey(hEventKey);

    return dwRetVal;
}

//*******************************************************************************
DWORD NledServiceDeleteEventPath()
//
//  Parameters:       NONE
//
//  Return Value:     TPR_PASS On Success,
//                    TPR_FAIL On Failure
//
//  Function to Delete Event Path from the Event Registry
//********************************************************************************
{
    //Declaring the variables
    HKEY hNLEDConfigKey = NULL;
    HKEY hLEDKey = NULL;
    DWORD dwRetVal = TPR_PASS;
    LONG  lReturnValue = 0;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwCurLEDIndex = 0;
    DWORD dwLEDNameSize = gc_dwMaxKeyNameLength + 1;
    NLED_EVENT_DETAILS  nledEventDetailsStruct = {0};
    HKEY hEventKey = NULL;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;
    BOOL  fEventPathDelete = FALSE;
    DWORD dwPathLen = 0;

    //Opening Config Key
    lReturnValue = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                REG_NLED_KEY_CONFIG_PATH,
                                0,
                                0,
                                &hNLEDConfigKey);

    if(lReturnValue != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to open the Config directory for LEDs: Delete Event Path Test Failed"));

        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Enumerating LED Name
    lReturnValue = RegEnumKeyEx(hNLEDConfigKey,
                                dwCurLEDIndex,
                                szLedName,
                                &dwLEDNameSize,
                                NULL, NULL, NULL, NULL);

    if(ERROR_NO_MORE_ITEMS == lReturnValue)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("No LEDs configured in the registry : Delete Event Path Test Failed"));

        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Iterating until All LEDS
    while(lReturnValue ==  ERROR_SUCCESS)
    {
        //Open LED Registry
        lReturnValue = RegOpenKeyEx(hNLEDConfigKey,
                                    szLedName,
                                    0,
                                    KEY_ALL_ACCESS,
                                    &hLEDKey);

        if(lReturnValue != ERROR_SUCCESS)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to open the LED registry : Delete Event Path Test Failed"));

            dwRetVal = TPR_FAIL;
            goto end;
        }
        else
        {
            //Enumerating LED Event
            lReturnValue = RegEnumKeyEx(hLEDKey,
                                        0,
                                        szEventName,
                                        &dwEventNameSize,
                                        NULL, NULL, NULL, NULL);

            if(ERROR_NO_MORE_ITEMS == lReturnValue)
            {
                //Log Error Message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("No Events Configured for %s LED"),
                             szLedName);

                dwRetVal = TPR_SKIP;
            }

            //Iteration for All Events in the LED.
            while(ERROR_SUCCESS == lReturnValue)
            {
                //Opening Event Key
                lReturnValue = RegOpenKeyEx(hLEDKey,
                                            szEventName,
                                            0,
                                            KEY_ALL_ACCESS,
                                            &hEventKey);

                if(lReturnValue != ERROR_SUCCESS)
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to open the event key for Led %s - Event %s .Delete Event Path Test Failed"),
                                 szLedName,
                                 szEventName);

                    dwRetVal = TPR_FAIL;
                    goto end;
                }

                //Backup Event Registry Values.
                lReturnValue = ReadEventDetails(hLEDKey,
                                                hEventKey,
                                                &nledEventDetailsStruct);

                if (!lReturnValue)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to Read Event Details. Delete Event Path Test Failed"));

                    dwRetVal = TPR_FAIL;
                    goto end;
                }

                //Delete the PATH value
                dwPathLen = (wcslen((WCHAR *)(nledEventDetailsStruct.bpPath)) + 1) *
                                              sizeof(WCHAR);

                lReturnValue = RegDeleteValue (hEventKey, REG_EVENT_PATH_VALUE);

                if(lReturnValue != ERROR_SUCCESS)
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable To Delete Path : Delete Event Path Test Failed"));

                    dwRetVal = TPR_FAIL;
                    goto end;
                }
                else
                {
                    fEventPathDelete = TRUE;
                }

                //Unload and Load Service for affecting updated Registry values.
                if(!UnloadLoadService())
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unload Load Service Failed For Delete Event Path Test"));

                    dwRetVal = TPR_FAIL;
                    goto end;
                }

                //Wait for 1 second
                Sleep(SLEEP_FOR_ONE_SECOND);

                LPWSTR lpServiceName = L"nledsrv.dll";

                //Check if the Service is Still Running
                if(!IsServiceRunning(lpServiceName))
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Nled Service is not running. Delete Event Path Test Failed"));

                    dwRetVal = TPR_FAIL;
                }
                else
                {
                    dwRetVal = TPR_PASS;
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Nled Service is still successfully running after deleting the path field for %s"),
                                 szLedName);
                }

                //Perform the test only for one event
                goto end;
            }
        }

        //Close the Led key handle
        RegCloseKey(hLEDKey);

        //Iterate for Next LED.
        dwCurLEDIndex++;
        dwLEDNameSize = gc_dwMaxKeyNameLength + 1;
        lReturnValue = RegEnumKeyEx(hNLEDConfigKey,
                                    dwCurLEDIndex,
                                    szLedName,
                                    &dwLEDNameSize,
                                    NULL, NULL, NULL, NULL);
    }

end:

    if(fEventPathDelete)
    {
        //Restore Valid Path
        lReturnValue = RegSetValueEx (hEventKey,
                                      REG_EVENT_PATH_VALUE,
                                      0,
                                      REG_SZ,
                                      nledEventDetailsStruct.bpPath,
                                      dwPathLen);

        if(lReturnValue != ERROR_SUCCESS)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable To Restore Original Path : Delete Event Path Test Failed"));

            dwRetVal = TPR_FAIL;
        }

        //Unload and Load Service for affecting updated Registry values.
        if(!UnloadLoadService())
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unload Load Service Failed For Delete Event Path Test"));

            dwRetVal = TPR_FAIL;
        }
    }

    //Close Config Key.
    if(hNLEDConfigKey != NULL)
    {
        RegCloseKey(hNLEDConfigKey);
    }

    if(hLEDKey != NULL)
    {
        RegCloseKey(hLEDKey);
    }

    if(hEventKey != NULL)
    {
        RegCloseKey(hEventKey);
    }

    return dwRetVal;
}

//*******************************************************************************
DWORD NledServiceInvalidOnOffTime()
//
//  Parameters:      NONE
//
//  Return Value:   TPR_PASS On Success,
//                  TPR_FAIL On Failure
//
//  Function to Set an Invalid Value for ON Time for testing the RobustNess of
//  NLED Driver
//********************************************************************************
{
    //Declaring the variables
    HKEY hNLEDConfigKey = NULL;
    DWORD dwRetVal = TPR_PASS;
    HKEY hLEDKey = NULL;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLEDNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwCurLEDIndex = 0;
    HKEY hEventKey = NULL;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    LONG lReturnValue = 0;
    DWORD dwCurLEDEventIndex = 0;
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;
    NLED_EVENT_DETAILS nledEventDetailsStruct = {0};
    NLED_SETTINGS_INFO *pLedSettingsInfo  = new NLED_SETTINGS_INFO[1];
    ZeroMemory(pLedSettingsInfo, sizeof(NLED_SETTINGS_INFO));

    HKEY hTempoEventKey = NULL;
    BOOL fStateChanged = FALSE;
    BOOL fOnTimeChanged = FALSE;
    BOOL fOffTimeChanged = FALSE;
    DWORD dwOnTime = 0;
    DWORD dwOffTime = 0;
    DWORD dwAdjustType = 0;
    DWORD dwBlinkable = 0;
    DWORD dwSWBlinkCtrl = 0;
    BOOL fStatus = FALSE;
    DWORD dwNewState = 0;
    DWORD dwInvalidOnOffTime = 0xFFFFFFFF;

    //Opening Config Key
    lReturnValue = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                REG_NLED_KEY_CONFIG_PATH,
                                0,
                                0,
                                &hNLEDConfigKey);

    if(lReturnValue != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to open the Config directory for LEDs. Invalid On Off Time Test Failed."));

        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Enumerating LED Name
    lReturnValue = RegEnumKeyEx(hNLEDConfigKey,
                                dwCurLEDIndex,
                                szLedName,
                                &dwLEDNameSize,
                                NULL, NULL, NULL, NULL);

    if(ERROR_NO_MORE_ITEMS == lReturnValue)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                      TEXT("No LEDs configured in the registry.  Invalid On Off Time Test Failed."));

        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Iterating until All LEDS
    while(lReturnValue ==  ERROR_SUCCESS)
    {
        //Open LED Registry
        lReturnValue = RegOpenKeyEx(hNLEDConfigKey,
                                    szLedName,
                                    0,
                                    KEY_ALL_ACCESS,
                                    &hLEDKey);

        if(lReturnValue != ERROR_SUCCESS)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to open the LED registry.  Invalid On Off Time Test Failed."));

            dwRetVal = TPR_FAIL;
            goto end;
        }
        else
        {
            //Checking if this LED supports adjustable OnTime and adjustable Offtime
            lReturnValue = GetDwordFromReg(hLEDKey,
                                           REG_ADJUSTTYPE_VALUE,
                                           &dwAdjustType);

            if(dwAdjustType != 2)
            {
                //Log Error Message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("The %s does not support adjustable OnTime and OffTime"),
                             szLedName);

                dwRetVal = TPR_SKIP;
            }
            else
            {
                //Checking if this LED supports blinking
                lReturnValue = GetDwordFromReg(hLEDKey,
                                               REG_BLINKABLE_VALUE,
                                               &dwBlinkable);

                if(!lReturnValue)
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to get the Blinkable key value for %s. Invalid On Off Time Test Failed."),
                                 szLedName);

                    dwRetVal = TPR_FAIL;
                    goto end;
                }

                //SWBlinkctrl field is optional
                GetDwordFromReg(hLEDKey, REG_SWBLINKCTRL_VALUE, &dwSWBlinkCtrl);

                if(dwBlinkable || dwSWBlinkCtrl)
                {
                    //Clearing off the dwCurLEDEventIndex for next NLEDs
                    dwCurLEDEventIndex = 0;

                    //Enumerating LED Event
                    lReturnValue = RegEnumKeyEx(hLEDKey,
                                                dwCurLEDEventIndex,
                                                szEventName,
                                                &dwEventNameSize,
                                                NULL, NULL, NULL, NULL);

                    if(ERROR_NO_MORE_ITEMS == lReturnValue)
                    {
                        //Log Error Message
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("No Events Configured for %s LED"),
                                     szLedName);

                        dwRetVal = TPR_SKIP;
                    }

                    //Iteration for All Events in the LED.
                    while(ERROR_SUCCESS == lReturnValue)
                    {
                        //Opening Event Key
                        lReturnValue = RegOpenKeyEx(hLEDKey,
                                                    szEventName,
                                                    0,
                                                    KEY_ALL_ACCESS,
                                                    &hEventKey);

                        if(lReturnValue != ERROR_SUCCESS)
                        {
                            //Log Error Message
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Unable to open the event key. Invalid On Off Time Test Failed."));

                            dwRetVal = TPR_FAIL;
                            goto end;
                        }

                        //Check if onTime is configured
                        fStatus = GetDwordFromReg(hEventKey,
                                                  REG_EVENT_ON_TIME_VALUE,
                                                  &dwOnTime);

                        if(!fStatus)
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Nled %s has an adjustable OnTime but Led-event %s does not have a OnTime field in the registry"),
                                         szLedName,
                                         szEventName);

                            dwRetVal = TPR_FAIL;
                            goto end;
                        }
                        else
                        {
                            fStatus = GetDwordFromReg(hEventKey,
                                                      REG_EVENT_OFF_TIME_VALUE,
                                                      &dwOffTime);

                            if(!fStatus)
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("Nled %s has an adjustable OffTime but Led-event %s does not have a OffTime field in the registry"),
                                             szLedName,
                                             szEventName);

                                dwRetVal = TPR_FAIL;
                                goto end;
                            }
                            else
                            {
                                //Backup Event Registry Values.
                                fStatus = ReadEventDetails(hLEDKey,
                                                           hEventKey,
                                                           &nledEventDetailsStruct);

                                if (!fStatus)
                                {
                                    //Log Error Message
                                    g_pKato->Log(LOG_DETAIL,
                                                 TEXT("Unable to Read Event Details. Invalid On Off Time Test Failed."));

                                    dwRetVal = TPR_FAIL;
                                    goto end;
                                }

                                //Set LED State to Blinkable
                                dwNewState = LED_BLINK;

                                lReturnValue = RegSetValueEx (hEventKey,
                                                              REG_EVENT_STATE_VALUE,
                                                              0,
                                                              REG_DWORD,
                                                              (PBYTE) (&dwNewState),
                                                              sizeof(dwNewState));

                                if(lReturnValue != ERROR_SUCCESS)
                                {
                                    //Log Error Message
                                    g_pKato->Log(LOG_DETAIL,
                                                 TEXT("Unable To Set LED State Value to 'Blink'. Invalid On Off Time Test Failed"));

                                    dwRetVal = TPR_FAIL;
                                    goto end;
                                }
                                else
                                {
                                    fStateChanged = TRUE;
                                }

                                //Set Invalid On Off Time
                                lReturnValue = RegSetValueEx (hEventKey,
                                                              REG_EVENT_ON_TIME_VALUE,
                                                              0,
                                                              REG_DWORD,
                                                              (PBYTE) (&dwInvalidOnOffTime),
                                                              sizeof(dwInvalidOnOffTime));

                                if(lReturnValue != ERROR_SUCCESS)
                                {
                                    g_pKato->Log(LOG_DETAIL,
                                                 TEXT("Unable To Set Invalid On Time. Invalid On Off Time Test Failed"));

                                    dwRetVal = TPR_FAIL;
                                    goto end;
                                }
                                else
                                {
                                    fOnTimeChanged = TRUE;
                                }

                                lReturnValue = RegSetValueEx (hEventKey,
                                                              REG_EVENT_OFF_TIME_VALUE,
                                                              0,
                                                              REG_DWORD,
                                                              (PBYTE) (&dwInvalidOnOffTime),
                                                              sizeof(dwInvalidOnOffTime));

                                if(lReturnValue != ERROR_SUCCESS)
                                {
                                    g_pKato->Log(LOG_DETAIL,
                                                 TEXT("Unable To Set Invalid Off Time. Invalid On Off Time Test Failed."));

                                    dwRetVal = TPR_FAIL;
                                    goto end;
                                }
                                else
                                {
                                    fOffTimeChanged = TRUE;
                                }

                                lReturnValue = Create_Set_TemporaryTrigger(hEventKey,
                                                                           TEMPO_EVENT_PATH,
                                                                           &nledEventDetailsStruct,
                                                                           &hTempoEventKey);

                                if (!lReturnValue)
                                {
                                    dwRetVal = TPR_FAIL;
                                    goto end;
                                }

                                //Unload and Load Service for affecting updated Registry values.
                                if(!UnloadLoadService())
                                {
                                    dwRetVal = TPR_FAIL;
                                    FAIL("Failed to UnLoad-Load the Nled Service");
                                    goto end;
                                }

                                //Raising Trigger
                                lReturnValue= RaiseTemporaryTrigger(hTempoEventKey,
                                                                    &nledEventDetailsStruct);

                                if(!lReturnValue)
                                {
                                    g_pKato->Log(LOG_DETAIL,
                                                 TEXT("Unable to raise the %s event temporarly from the test"),
                                                 szEventName);

                                    dwRetVal = TPR_FAIL;
                                    goto end;
                                }

                                //Wait for 1 second for the service to act on the trigger
                                Sleep(SLEEP_FOR_ONE_SECOND);

                                LPWSTR lpServiceName = L"nledsrv.dll";

                                //Check if the Service is Still Running
                                if(!IsServiceRunning(lpServiceName))
                                {
                                    dwRetVal = TPR_FAIL;
                                    g_pKato->Log (LOG_DETAIL,
                                                  TEXT("Nled Service failed to handle the Invalid OnTime & Invalid OffTime setting for %s"),
                                                  szLedName);
                                }
                                else
                                {
                                    dwRetVal = TPR_PASS;
                                    g_pKato->Log (LOG_DETAIL,
                                                  TEXT("Setting Invalid OnTime & Invalid OffTime does affect the Nled Service"));
                                }

                                //Performing the test only for one Led
                                goto end;
                            }
                        }
                    }//Iteration for All Events in the LED. - END
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("%s does not supports blinking"),
                                 szLedName);
                }
            }//Checking for AdjustType

            RegCloseKey(hLEDKey);

            //Next LED.
            dwCurLEDIndex++;
            dwLEDNameSize = gc_dwMaxKeyNameLength + 1;

            //Enumerating for Next LED.
            lReturnValue = RegEnumKeyEx(hNLEDConfigKey,
                                        dwCurLEDIndex,
                                        szLedName,
                                        &dwLEDNameSize,
                                        NULL, NULL, NULL, NULL);
        }
    }//While Next LED.

end:

    //Restore the previous Valid On Time
    if(fOnTimeChanged)
    {
        lReturnValue= RegSetValueEx (hEventKey,
                                     REG_EVENT_ON_TIME_VALUE,
                                     0,
                                     REG_DWORD,
                                     (PBYTE) (&dwOnTime),
                                     sizeof(dwOnTime));

        if(lReturnValue != ERROR_SUCCESS)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable To Restore Original On Time. Invalid On Off Time Test Failed."));

            dwRetVal = TPR_FAIL;
        }
    }

    //Restore previous offTime value
    if(fOffTimeChanged)
    {
        lReturnValue= RegSetValueEx (hEventKey,
                                     REG_EVENT_OFF_TIME_VALUE,
                                     0,
                                     REG_DWORD,
                                     (PBYTE) (&dwOffTime),
                                     sizeof(dwOffTime));

        if(lReturnValue != ERROR_SUCCESS)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable To Restore Original Off Time.  Invalid On Off Time Test Failed."));

            dwRetVal = TPR_FAIL;
        }
    }

    //Restore Original State
    if(fStateChanged)
    {
        lReturnValue = RegSetValueEx(hEventKey,
                                     REG_EVENT_STATE_VALUE,
                                     0,
                                     REG_DWORD,
                                     (PBYTE) (&nledEventDetailsStruct.dwState),
                                     sizeof(nledEventDetailsStruct.dwState));

        if(lReturnValue != ERROR_SUCCESS)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable To Restore Original State.  Invalid On Off Time Test Failed."));

            dwRetVal = TPR_FAIL;
        }
    }

    //Resetting Temporary Trigger
    if(hEventKey)
    {
        lReturnValue = Unset_TemporaryTrigger(hEventKey,
                                              hTempoEventKey,
                                              &nledEventDetailsStruct);

        if(!lReturnValue)
        {
            dwRetVal = TPR_FAIL;
        }
    }

    if(hTempoEventKey)
    {
        //Delete the Temporary Trigger.
        lReturnValue= DeleteTemporaryTrigger(hTempoEventKey,
                                             TEMPO_EVENT_PATH,
                                             pLedSettingsInfo,
                                             &nledEventDetailsStruct);

        if(!lReturnValue)
        {
            dwRetVal = TPR_FAIL;
        }

        //Unload and Load Service for affecting updated Registry values.
        if(!UnloadLoadService())
        {
            dwRetVal = TPR_FAIL;
            FAIL("Failed to UnLoad-Load the Nled Service");
        }
    }

    //Close the key handles
    if(hNLEDConfigKey != NULL)
    {
        RegCloseKey(hNLEDConfigKey);
    }

    if(hLEDKey != NULL)
    {
        RegCloseKey(hLEDKey);
    }

    if(hEventKey != NULL)
    {
        RegCloseKey(hEventKey);
    }

    if(pLedSettingsInfo != NULL)
    {
        delete[] pLedSettingsInfo;
        pLedSettingsInfo = NULL;
    }

    return dwRetVal;
}

//*******************************************************************************
DWORD NledServiceInvalidOperatorValue (VOID)
//
//  Parameters:     NONE
//
//  Return Value:   TPR_PASS On Success,
//                  TPR_FAIL On Failure
//
//  To Set an Invalid Value for Operator for testing the RobustNess of NLED Driver
//********************************************************************************
{
    //Declaring the variables
    HKEY hNLEDConfigKey = NULL;
    DWORD dwRetVal = TPR_PASS;
    HKEY hLEDKey = NULL;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLEDNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwCurLEDIndex = 0;
    HKEY hEventKey = NULL;
    LONG lReturnValue = 0;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwCurLEDEventIndex = 0;
    NLED_EVENT_DETAILS nledEventDetailsStruct = {0};
    HKEY hTempoEventKey = NULL;
    BOOL fStateChanged = FALSE;
    BOOL fOperatorChanged = FALSE;
    DWORD dwOperator = 0;
    BOOL fStatus = FALSE;
    DWORD dwNewState = 0;

    //Opening Config Key
    lReturnValue = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                REG_NLED_KEY_CONFIG_PATH,
                                0,
                                0,
                                &hNLEDConfigKey);

    if(lReturnValue != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to open the Config directory. Invalid Operator Value Test Failed."));

        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Enumerating LED Name
    lReturnValue = RegEnumKeyEx(hNLEDConfigKey,
                                dwCurLEDIndex,
                                szLedName,
                                &dwLEDNameSize,
                                NULL, NULL,
                                NULL, NULL);

    if(ERROR_NO_MORE_ITEMS == lReturnValue)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("No LEDs configured in the registry. Invalid Operator Value Test Failed."));

        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Iterating until All LEDS
    while(lReturnValue ==  ERROR_SUCCESS)
    {
        //Open LED Registry
        lReturnValue = RegOpenKeyEx(hNLEDConfigKey,
                                    szLedName,
                                    0,
                                    KEY_ALL_ACCESS,
                                    &hLEDKey);

        if(lReturnValue != ERROR_SUCCESS)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to open the LED registry. Invalid Operator Value Test Failed."));

            dwRetVal = TPR_FAIL;
            goto end;
        }
        else
        {
            //Clearing the dwCurLEDEventIndex for next LEDs
            dwCurLEDEventIndex = 0;

            //Enumerating LED Event
            lReturnValue = RegEnumKeyEx(hLEDKey,
                                        dwCurLEDEventIndex,
                                        szEventName,
                                        &dwEventNameSize,
                                        NULL, NULL,
                                        NULL, NULL);

            if(ERROR_NO_MORE_ITEMS == lReturnValue)
            {
                //Log Error Message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("No Events Configured for %s LED"),
                             szLedName);

                dwRetVal = TPR_SKIP;
            }

            //Iteration for All Events in the LED.
            while(ERROR_SUCCESS == lReturnValue)
            {
                //Opening Event Key
                lReturnValue = RegOpenKeyEx(hLEDKey,
                                            szEventName,
                                            0,
                                            KEY_ALL_ACCESS,
                                            &hEventKey);

                if(lReturnValue != ERROR_SUCCESS)
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to open the LED Event registry. Invalid Operator Value Test Failed."));

                    dwRetVal = TPR_FAIL;
                    goto end;
                }

                //Check if Operator is configured
                fStatus = GetDwordFromReg(hEventKey, REG_EVENT_OPERATOR_VALUE, &dwOperator);

                if(fStatus)
                {
                    //Backup Event Registry Values.
                    lReturnValue= ReadEventDetails(hLEDKey,
                                                   hEventKey,
                                                   &nledEventDetailsStruct);

                    if (!lReturnValue)
                    {
                        //Log Error Message
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable to Read event details. Invalid Operator Value Test Failed."));

                        dwRetVal = TPR_FAIL;
                        goto end;
                    }

                    //set Invalid Operator Value
                    dwOperator = 10;

                    //Set LED State to Blinkable
                    dwNewState = LED_BLINK;

                    lReturnValue= RegSetValueEx (hEventKey,
                                                 REG_EVENT_STATE_VALUE,
                                                 0,
                                                 REG_DWORD,
                                                 (PBYTE) (&dwNewState),
                                                 sizeof(dwNewState));

                    if(lReturnValue != ERROR_SUCCESS)
                    {
                        //Log Error Message
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable To Set LED State Value to Blink. Invalid Operator Value Test Failed"));

                        dwRetVal = TPR_FAIL;
                        goto end;
                    }
                    else
                    {
                        fStateChanged = TRUE;
                    }

                    //Set Invalid Operator
                    lReturnValue = RegSetValueEx(hEventKey,
                                                 REG_EVENT_OPERATOR_VALUE,
                                                 0,
                                                 REG_DWORD,
                                                 (PBYTE)(&dwOperator),
                                                 sizeof(dwOperator));

                    if (lReturnValue != ERROR_SUCCESS)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable To Set Invalid Operator Value"));
                        dwRetVal = TPR_FAIL;
                        goto end;
                    }
                    else
                    {
                        fOperatorChanged = TRUE;
                    }

                    lReturnValue= Create_Set_TemporaryTrigger(hEventKey,
                                                              TEMPO_EVENT_PATH,
                                                              &nledEventDetailsStruct,
                                                              &hTempoEventKey);

                    if (!lReturnValue)
                    {
                        dwRetVal = TPR_FAIL;
                        goto end;
                    }

                    //Unload and Load Service for affecting updated Registry values.
                    if(!UnloadLoadService())
                    {
                        dwRetVal = TPR_FAIL;
                        FAIL("Failed to UnLoad-Load the Nled Service");
                        goto end;
                    }

                    //Raising Trigger
                    lReturnValue = RaiseTemporaryTrigger(hTempoEventKey,
                                                         &nledEventDetailsStruct);

                    if(!lReturnValue)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable to raise the %s event temporarly from the test"),
                                     szEventName);

                        dwRetVal = TPR_FAIL;
                        goto end;
                    }

                    //Wait for 1 sec for the service to act on the trigger
                    Sleep(SLEEP_FOR_ONE_SECOND);

                    LPWSTR lpServiceName = L"nledsrv.dll";

                    //Check if the Service is Still Running
                    if(!IsServiceRunning(lpServiceName))
                    {
                        dwRetVal = TPR_FAIL;
                        g_pKato->Log(LOG_DETAIL,
                                    TEXT("Nled Service is not running after setting an invalid Operator value for %s"),
                                    szLedName);
                    }
                    else
                    {
                        //Log the result
                        dwRetVal = TPR_PASS;
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Nled Service is still running after setting an invalid Operator value for %s"),
                                     szLedName);
                    }

                    goto end;
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("The Event %s does not have the Operator Field"),
                                 szEventName);

                    dwRetVal = TPR_SKIP;
                }

                //Close Present Event Key
                RegCloseKey(hEventKey);

                //Next Event in the LED
                dwCurLEDEventIndex++;
                dwEventNameSize = gc_dwMaxKeyNameLength + 1;

                //Enumerating the Next Event in LED.
                lReturnValue = RegEnumKeyEx(hLEDKey,
                                            dwCurLEDEventIndex,
                                            szEventName,
                                            &dwEventNameSize,
                                            NULL, NULL,
                                            NULL, NULL);
            }
        }

        //Close the key handle
        RegCloseKey(hLEDKey);

        //Next LED.
        dwCurLEDIndex++;
        dwLEDNameSize = gc_dwMaxKeyNameLength + 1;

        //Enumerating for Next LED.
        lReturnValue = RegEnumKeyEx(hNLEDConfigKey,
                                    dwCurLEDIndex,
                                    szLedName,
                                    &dwLEDNameSize,
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
    }

end:

    //Restore Original Operator Value
    if(fOperatorChanged)
    {
        lReturnValue = RegSetValueEx (hEventKey,
                                      REG_EVENT_OPERATOR_VALUE,
                                      0,
                                      REG_DWORD,
                                      (PBYTE) (&nledEventDetailsStruct.dwOperator),
                                      sizeof(nledEventDetailsStruct.dwOperator));

        if(lReturnValue != ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable To Restore Original Operator Value. Invalid Operator Value Test Failed"));

            dwRetVal = TPR_FAIL;
        }
    }

    //Restore Original State
    if(fStateChanged)
    {
        lReturnValue = RegSetValueEx (hEventKey,
                                      REG_EVENT_STATE_VALUE,
                                      0,
                                      REG_DWORD,
                                      (PBYTE) (&nledEventDetailsStruct.dwState),
                                      sizeof(nledEventDetailsStruct.dwState));

        if(lReturnValue != ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable To Restore Original State. Invalid Operator Value Test Failed"));

            dwRetVal = TPR_FAIL;
        }
    }

    //Resetting Temporary Trigger.
    if(hEventKey)
    {
        lReturnValue = Unset_TemporaryTrigger(hEventKey,
                                              hTempoEventKey,
                                              &nledEventDetailsStruct);

        if(!lReturnValue)
        {
            dwRetVal = TPR_FAIL;
        }
    }

    if(hTempoEventKey)
    {
        //Delete the Temporary Trigger.
        lReturnValue= DeleteTemporaryTrigger(hTempoEventKey,
                                             TEMPO_EVENT_PATH,
                                             NULL,
                                             &nledEventDetailsStruct);

        if(!lReturnValue)
        {
            dwRetVal = TPR_FAIL;
        }

        //Unload and Load Service for affecting updated Registry values.
        if(!UnloadLoadService())
        {
            dwRetVal = TPR_FAIL;
            FAIL("Failed to UnLoad-Load the Nled Service");
        }
    }

    //Close Config Key.
    if(hNLEDConfigKey != NULL)
    {
        RegCloseKey(hNLEDConfigKey);
    }

    if(hLEDKey != NULL)
    {
        RegCloseKey(hLEDKey);
    }

    if(hEventKey != NULL)
    {
        RegCloseKey(hEventKey);
    }

    return dwRetVal;
}

//*******************************************************************************
DWORD NledServiceInvalidTotalCycleTime (VOID)
//
//  Parameters:      NONE
//
//  Return Value:   TPR_PASS On Success,
//                  TPR_FAIL On Failure
//
//  To Set an Invalid Value for Total Cycle Time for testing the Robust Ness of
//  NLED Driver
//********************************************************************************
{
    //Declaring the variables
    HKEY hNLEDConfigKey = NULL;
    DWORD dwRetVal = TPR_PASS;
    HKEY hLEDKey = NULL;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLEDNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwCurLEDIndex = 0;
    HKEY hEventKey = NULL;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    LONG lReturnValue = 0;
    DWORD dwCurLEDEventIndex = 0;
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;
    NLED_EVENT_DETAILS nledEventDetailsStruct = {0};
    NLED_SETTINGS_INFO *pLedSettingsInfo  = new NLED_SETTINGS_INFO[1];
    ZeroMemory(pLedSettingsInfo, sizeof(NLED_SETTINGS_INFO));

    HKEY hTempoEventKey = NULL;
    BOOL fStateChanged = FALSE;
    BOOL fTotalCycleTimeChanged = FALSE;
    DWORD dwTotalCycleTime = 0;
    DWORD dwAdjustType = 0;
    DWORD dwBlinkable = 0;
    DWORD dwSWBlinkCtrl = 0;
    DWORD dwNewState = 0;
    BOOL fStatus = FALSE;
    DWORD dwInvalidTotalCycleTime = 0xFFFFFFFF;

    //Opening Config Key
    lReturnValue = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                REG_NLED_KEY_CONFIG_PATH,
                                0,
                                0,
                                &hNLEDConfigKey);

    if(lReturnValue != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to open the Config directory for LEDs. Invalid TotalCycle Time Test Failed"));

        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Enumerating LED Name
    lReturnValue = RegEnumKeyEx(hNLEDConfigKey,
                                dwCurLEDIndex,
                                szLedName,
                                &dwLEDNameSize,
                                NULL, NULL,
                                NULL, NULL);

    if(ERROR_NO_MORE_ITEMS == lReturnValue)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("No LEDs configured in the registry.  Invalid TotalCycle Time Test Failed"));

        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Iterating until All LEDS
    while(lReturnValue ==  ERROR_SUCCESS)
    {
        //Open LED Registry
        lReturnValue = RegOpenKeyEx(hNLEDConfigKey,
                                    szLedName,
                                    0,
                                    KEY_ALL_ACCESS,
                                    &hLEDKey);

        if(lReturnValue != ERROR_SUCCESS)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to open the LED registry. Invalid TotalCycle Time Test Failed."));

            dwRetVal = TPR_FAIL;
            goto end;
        }
        else
        {
            //Checking if this LED supports adjustable OnTime and adjustable Offtime
            lReturnValue = GetDwordFromReg(hLEDKey, REG_ADJUSTTYPE_VALUE, &dwAdjustType);

            if(dwAdjustType != 1)
            {
                //Log Error Message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("The %s does not support adjustable TotalCycleTime"),
                             szLedName);

                dwRetVal = TPR_SKIP;
            }
            else
            {
                //Checking if this LED supports blinking
                lReturnValue = GetDwordFromReg(hLEDKey, REG_BLINKABLE_VALUE, &dwBlinkable);

                if(!lReturnValue)
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to query Blinkable value for %s. Invalid TotalCycle Time Test Failed."),
                                 szLedName);

                    dwRetVal = TPR_FAIL;
                    goto end;
                }

                //SWBlinkCtrl is a optional field
                GetDwordFromReg(hLEDKey, REG_SWBLINKCTRL_VALUE, &dwSWBlinkCtrl);

                if(dwBlinkable || dwSWBlinkCtrl)
                {
                    //Clearing off the dwCurLEDEventIndex for next NLEDs
                    dwCurLEDEventIndex = 0;

                    //Enumerating LED Event
                    lReturnValue = RegEnumKeyEx(hLEDKey,
                                                dwCurLEDEventIndex,
                                                szEventName,
                                                &dwEventNameSize,
                                                NULL, NULL,
                                                NULL, NULL);

                    if(ERROR_NO_MORE_ITEMS == lReturnValue)
                    {
                        //Log Error Message
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("No Events Configured for %s LED"),
                                     szLedName);

                        dwRetVal = TPR_SKIP;
                    }

                    //Iteration for All Events in the LED.
                    while(ERROR_SUCCESS == lReturnValue)
                    {
                        //Opening Event Key
                        lReturnValue = RegOpenKeyEx(hLEDKey,
                                                    szEventName,
                                                    0,
                                                    KEY_ALL_ACCESS,
                                                    &hEventKey);

                        if(lReturnValue != ERROR_SUCCESS)
                        {
                            //Log Error Message
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Unable to open the event key. Invalid TotalCycle Time Test Failed."));

                            dwRetVal = TPR_FAIL;
                            goto end;
                        }

                        //Check if onTime is configured
                        fStatus = GetDwordFromReg(hEventKey,
                                                  REG_EVENT_TOTAL_CYCLE_TIME_VALUE,
                                                  &dwTotalCycleTime);

                        if(fStatus)
                        {
                            //Backup Event Registry Values.
                            fStatus = ReadEventDetails(hLEDKey,
                                                       hEventKey,
                                                       &nledEventDetailsStruct);

                            if (!fStatus)
                            {
                                //Log Error Message
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("Unable to Read Event Details. Invalid TotalCycle Time Test Failed."));

                                dwRetVal = TPR_FAIL;
                                goto end;
                            }

                            //Set LED State to Blinkable
                            dwNewState = LED_BLINK;

                            lReturnValue = RegSetValueEx(hEventKey,
                                                         REG_EVENT_STATE_VALUE,
                                                         0,
                                                         REG_DWORD,
                                                         (PBYTE) (&dwNewState),
                                                         sizeof(dwNewState));

                            if(lReturnValue != ERROR_SUCCESS)
                            {
                                //Log Error Message
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("Unable To Set LED State Value to 'Blink'. Invalid TotalCycle Time Test Failed."));

                                dwRetVal = TPR_FAIL;
                                goto end;
                            }
                            else
                            {
                                fStateChanged = TRUE;
                            }

                            //Set Invalid On Time
                            lReturnValue = RegSetValueEx(hEventKey,
                                                         REG_EVENT_TOTAL_CYCLE_TIME_VALUE,
                                                         0,
                                                         REG_DWORD,
                                                         (PBYTE) (&dwInvalidTotalCycleTime),
                                                         sizeof(dwInvalidTotalCycleTime));

                            if(lReturnValue != ERROR_SUCCESS)
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("Unable To Set Invalid TotalCycle Time. Invalid TotalCycle Time Test Failed."));

                                dwRetVal = TPR_FAIL;
                                goto end;
                            }
                            else
                            {
                                fTotalCycleTimeChanged = TRUE;
                            }

                            lReturnValue = Create_Set_TemporaryTrigger(hEventKey,
                                                                       TEMPO_EVENT_PATH,
                                                                       &nledEventDetailsStruct,
                                                                       &hTempoEventKey);

                            if (!lReturnValue)
                            {
                                dwRetVal = TPR_FAIL;
                                goto end;
                            }

                            //Unload and Load Service for affecting updated Registry values.
                            if(!UnloadLoadService())
                            {
                                dwRetVal = TPR_FAIL;
                                FAIL("Failed to UnLoad-Load the Nled Service");
                                goto end;
                            }

                            //Raising Trigger
                            lReturnValue= RaiseTemporaryTrigger(hTempoEventKey,
                                                                &nledEventDetailsStruct);

                            if(!lReturnValue)
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("Unable to raise the %s event temporarly from the test"),
                                             szEventName);

                                dwRetVal = TPR_FAIL;
                                goto end;
                            }

                            //Wait for 1 sec for the service to act on the trigger
                            Sleep(SLEEP_FOR_ONE_SECOND);

                            LPWSTR lpServiceName = L"nledsrv.dll";

                            //Check if the Service is Still Running
                            if(!IsServiceRunning(lpServiceName))
                            {
                                dwRetVal = TPR_FAIL;
                                g_pKato->Log (LOG_DETAIL,
                                              TEXT("Nled Service is not running after setting a Invalid TotalCycleTime for %s"),
                                              szLedName);
                            }
                            else
                            {
                                dwRetVal = TPR_PASS;
                                g_pKato->Log (LOG_DETAIL,
                                              TEXT("Nled Service is running after setting a Invalid TotalCycleTime for %s"),
                                              szLedName);
                            }

                            //Performing the test only for one Led
                            goto end;
                        }
                        else
                        {
                            g_pKato->Log(LOG_DETAIL,
                                          TEXT("Led-Event %s has a adjustable TotalCycleTime but does not have a TotalCycleTime field"),
                                          szEventName);

                            dwRetVal = TPR_FAIL;
                            goto end;
                        }
                    }
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("%s does not supports blinking"),
                                 szLedName);
                }
            }

            RegCloseKey(hLEDKey);

            //Next LED.
            dwCurLEDIndex++;
            dwLEDNameSize = gc_dwMaxKeyNameLength + 1;

            //Enumerating for Next LED.
            lReturnValue = RegEnumKeyEx(hNLEDConfigKey,
                                        dwCurLEDIndex,
                                        szLedName,
                                        &dwLEDNameSize,
                                        NULL, NULL,
                                        NULL, NULL);
        }
    }

end:

    //Restore the previous Valid On Time
    if(fTotalCycleTimeChanged)
    {
        lReturnValue= RegSetValueEx(hEventKey,
                                    REG_EVENT_TOTAL_CYCLE_TIME_VALUE,
                                    0,
                                    REG_DWORD,
                                    (PBYTE) (&dwTotalCycleTime),
                                    sizeof(dwTotalCycleTime));

        if(lReturnValue != ERROR_SUCCESS)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable To Restore Original TotalCycle Time!  Invalid TotalCycle Time Test Failed"));

            dwRetVal = TPR_FAIL;
        }
    }

    //Restore the Original State
    if(fStateChanged)
    {
        lReturnValue = RegSetValueEx(hEventKey,
                                     REG_EVENT_STATE_VALUE,
                                     0,
                                     REG_DWORD,
                                     (PBYTE) (&nledEventDetailsStruct.dwState),
                                     sizeof(nledEventDetailsStruct.dwState));

        if(lReturnValue != ERROR_SUCCESS)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable To Restore Original State. Invalid TotalCycle Time Test Failed"));

            dwRetVal = TPR_FAIL;
        }
    }

    //Resetting Temporary Trigger
    if(hEventKey)
    {
        lReturnValue = Unset_TemporaryTrigger(hEventKey,
                                              hTempoEventKey,
                                              &nledEventDetailsStruct);

        if(!lReturnValue)
        {
            dwRetVal = TPR_FAIL;
        }
    }

    if(hTempoEventKey)
    {
        //Delete the Temporary Trigger
        lReturnValue= DeleteTemporaryTrigger(hTempoEventKey,
                                             TEMPO_EVENT_PATH,
                                             pLedSettingsInfo,
                                             &nledEventDetailsStruct);

        if(!lReturnValue)
        {
            dwRetVal = TPR_FAIL;
        }

        //Unload and Load Service for affecting updated Registry values
        if(!UnloadLoadService())
        {
            dwRetVal = TPR_FAIL;
            FAIL("Failed to UnLoad-Load the Nled Service");
        }
    }

    //Close the key handles
    if(hNLEDConfigKey != NULL)
    {
        RegCloseKey(hNLEDConfigKey);
    }

    if(hLEDKey != NULL)
    {
        RegCloseKey(hLEDKey);
    }

    if(hEventKey != NULL)
    {
        RegCloseKey(hEventKey);
    }

    if(pLedSettingsInfo != NULL)
    {
        delete[] pLedSettingsInfo;
        pLedSettingsInfo = NULL;
    }

    return dwRetVal;
}

//*******************************************************************************
DWORD   NledServiceInvalidLEDNum(VOID)
//
//  Parameters:   NONE
//
//  Return Value: TPR_PASS On Success,
//                TPR_FAIL On Failure
//
//  To Set an Invalid Value for Led Num for testing the RobustNess of NLED Driver
//********************************************************************************
{
    //Declaring the variables
    HKEY hNLEDConfigKey = NULL;
    DWORD dwRetVal = TPR_PASS;
    HKEY hLEDKey = NULL;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLEDNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwCurLEDIndex = 0;
    HKEY hEventKey = NULL;
    LONG lReturnValue = 0;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;
    NLED_EVENT_DETAILS nledEventDetailsStruct = {0};
    BOOL fLedNumChanged = FALSE;
    DWORD dwInvalidLedNum = 0xFFFFFFF;

    //Opening the Config Key
    lReturnValue = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                REG_NLED_KEY_CONFIG_PATH,
                                0,
                                0,
                                &hNLEDConfigKey);

    if(lReturnValue != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to open the Config directory for LEDs For Invalid Led Num. Invalid LED Num Test Failed"));

        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Enumerating for LEDs
    lReturnValue = RegEnumKeyEx(hNLEDConfigKey,
                                dwCurLEDIndex,
                                szLedName,
                                &dwLEDNameSize,
                                NULL,
                                NULL,
                                NULL,
                                NULL);

    if(ERROR_NO_MORE_ITEMS == lReturnValue)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("No LEDs configured in the registry. Invalid LED Num Test Failed"));

        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Iterating until All LEDS
    while(lReturnValue ==  ERROR_SUCCESS)
    {
        //Open LED Registry
        lReturnValue = RegOpenKeyEx(hNLEDConfigKey,
                                    szLedName,
                                    0,
                                    KEY_ALL_ACCESS,
                                    &hLEDKey);

        if(lReturnValue != ERROR_SUCCESS)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to open the LED registry. Invalid LED Num Test Failed"));

            dwRetVal = TPR_FAIL;
            goto end;
        }
        else
        {
            //Enumerating for LED Events
            lReturnValue = RegEnumKeyEx(hLEDKey,
                                        0,
                                        szEventName,
                                        &dwEventNameSize,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL);

            if(ERROR_NO_MORE_ITEMS == lReturnValue)
            {
                //Log Error Message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("No Events Configured for %s LED"),
                             szLedName);

                dwRetVal = TPR_SKIP;
            }

            //Iteration for All Events in the LED.
            while(ERROR_SUCCESS == lReturnValue)
            {
                //Opening Event Key
                lReturnValue = RegOpenKeyEx(hLEDKey,
                                            szEventName,
                                            0,
                                            KEY_ALL_ACCESS,
                                            &hEventKey);

                if(lReturnValue != ERROR_SUCCESS)
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to open the LED registry Invalid LED Num Test Failed."));

                    dwRetVal = TPR_FAIL;
                    goto end;
                }

                //Backup the event Registry Values
                if (!ReadEventDetails(hLEDKey,
                                      hEventKey,
                                      &nledEventDetailsStruct))
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to Read event details. Invalid LED Num Test Failed."));

                    dwRetVal = TPR_FAIL;
                    goto end;
                }

                //Set Invalid Value
                lReturnValue = RegSetValueEx (hLEDKey,
                                              REG_LED_NUM_VALUE,
                                              0,
                                              REG_DWORD,
                                              (PBYTE)(&dwInvalidLedNum),
                                              sizeof(dwInvalidLedNum));

                if(lReturnValue != ERROR_SUCCESS)
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable To Set Invalid Led Num. Invalid LED Num Test Failed."));

                    dwRetVal = TPR_FAIL;
                    goto end;
                }
                else
                {
                    fLedNumChanged = TRUE;
                }

                if(!UnloadLoadService())
                {
                    dwRetVal = TPR_FAIL;
                    FAIL("Failed to UnLoad-Load the Nled Service");
                    goto end;
                }

                //Let the service act on the trigger
                Sleep(SLEEP_FOR_ONE_SECOND);

                LPWSTR lpServiceName = L"nledsrv.dll";

                //Check if the Changes in the Registry are affected.
                if(!IsServiceRunning(lpServiceName))
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Service is not running after setting invalid LedNum for %s. Invalid LED Num Test Failed"),
                                 szLedName);

                    dwRetVal = TPR_FAIL;
                }
                else
                {
                    //Log the result
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Service is successfully running even after setting invalid LedNum for %s. Invalid LED Num Test Passed"),
                                 szLedName);

                    dwRetVal = TPR_PASS;
                }

                goto end;
            }
        }

        RegCloseKey(hLEDKey);

        //Next LED
        dwCurLEDIndex++;
        dwLEDNameSize = gc_dwMaxKeyNameLength + 1;

        //Enumerating for Next LED.
        lReturnValue = RegEnumKeyEx(hNLEDConfigKey,
                                    dwCurLEDIndex,
                                    szLedName,
                                    &dwLEDNameSize,
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
    }

end:

    //Restore Original LedNum Value
    if(fLedNumChanged)
    {
        lReturnValue = RegSetValueEx (hLEDKey,
                                      REG_LED_NUM_VALUE,
                                      0,
                                      REG_DWORD,
                                      (PBYTE) (&nledEventDetailsStruct.dwLedNum),
                                      sizeof(nledEventDetailsStruct.dwLedNum));

        if(lReturnValue != ERROR_SUCCESS)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable To Restore Original Led Num Value. Invalid LED Num Test Failed"));

            dwRetVal = TPR_FAIL;
        }

        //Unload and Load Service for affecting updated Registry values.
        if(!UnloadLoadService())
        {
            dwRetVal = TPR_FAIL;
            FAIL("Failed to UnLoad-Load the Nled Service");
        }
    }

    //Close keys
    if(hNLEDConfigKey != NULL)
    {
        RegCloseKey(hNLEDConfigKey);
    }

    if(hLEDKey != NULL)
    {
        RegCloseKey(hLEDKey);
    }

    if(hEventKey != NULL)
    {
        RegCloseKey(hEventKey);
    }

    return dwRetVal;
}

//*****************************************************************************
DWORD NledServiceDeleteLEDRegistry (VOID)
//
//  Parameters:      NONE
//  Return Value:    TPR_PASS On Success,
//                   TPR_FAIL On Failure
//
//  Function to Delete Led Details Registry for Testing the Robustness of NLED
//  driver.
//*****************************************************************************
{
    //Declaring the Variables
    HKEY hNLEDConfigKey = NULL;
    DWORD dwRetVal = TPR_PASS;
    HKEY hLEDKey = NULL;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLEDNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwCurLEDIndex = 0;
    HKEY hEventKey = NULL;
    LONG lReturnValue = 0;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;
    NLED_EVENT_DETAILS nledEventDetailsStruct = {0};
    NLED_LED_DETAILS LED_Details;
    BOOL fLedDetailsChanged = FALSE;

    //Opening the Config Key
    lReturnValue = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                REG_NLED_KEY_CONFIG_PATH,
                                0,
                                0,
                                &hNLEDConfigKey);

    if(lReturnValue != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to open the Config directory. Delete LED Registry test Failed."));

        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Enumerating for LEDs
    lReturnValue = RegEnumKeyEx(hNLEDConfigKey,
                                dwCurLEDIndex,
                                szLedName,
                                &dwLEDNameSize,
                                NULL,
                                NULL,
                                NULL,
                                NULL);

    if(ERROR_NO_MORE_ITEMS == lReturnValue)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("No LEDs configured in the registry. Delete LED Registry test Failed."));

        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Iterating until All LEDS
    while(lReturnValue ==  ERROR_SUCCESS)
    {
        //Open LED Registry
        lReturnValue = RegOpenKeyEx(hNLEDConfigKey,
                                    szLedName,
                                    0,
                                    KEY_ALL_ACCESS,
                                    &hLEDKey);

        if(lReturnValue != ERROR_SUCCESS)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to open the LED registry. Delete LED Registry test Failed."));

            dwRetVal = TPR_FAIL;
            goto end;
        }
        else
        {
            //Enumerating LED Event
            lReturnValue = RegEnumKeyEx(hLEDKey,
                                        0,
                                        szEventName,
                                        &dwEventNameSize,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL);

            if(ERROR_NO_MORE_ITEMS == lReturnValue)
            {
                //Log Error Message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("No Events Configured for %s LED"),
                             szLedName);

                dwRetVal = TPR_SKIP;
            }

            //Iteration for All Events in the LED.
            while(ERROR_SUCCESS == lReturnValue)
            {
                //Opening Event Key
                lReturnValue = RegOpenKeyEx(hLEDKey,
                                            szEventName,
                                            0,
                                            KEY_ALL_ACCESS,
                                            &hEventKey);

                if(lReturnValue != ERROR_SUCCESS)
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to open the LED registry. Delete LED Registry test Failed."));

                    dwRetVal = TPR_FAIL;
                    goto end;
                }

                //Backup Registry Values
                if (!ReadEventDetails(hLEDKey, hEventKey, &nledEventDetailsStruct))
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to Read event details. Delete LED Registry test Failed."));

                    dwRetVal = TPR_FAIL;
                    goto end;
                }

                if (!ReadLEDDetails(&LED_Details, &hLEDKey))
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable To Read LED Details. Delete LED Registry test Failed."));

                    dwRetVal = TPR_FAIL;
                    goto end;
                }

                //Delete LED Registry
                if (!DeleteLEDRegistry (&hLEDKey))
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable To Delete LED Registry. Delete LED Registry test Failed."));

                    dwRetVal = TPR_FAIL;
                    goto end;
                }
                else
                {
                    fLedDetailsChanged = TRUE;
                }

                if(!UnloadLoadService())
                {
                    dwRetVal = TPR_FAIL;
                    FAIL("Failed to UnLoad-Load the Nled Service");
                    goto end;
                }

                //Waiting for 1 second
                Sleep(SLEEP_FOR_ONE_SECOND);

                LPWSTR lpServiceName = L"nledsrv.dll";

                //Check if the Service is Still Running
                if(!IsServiceRunning(lpServiceName))
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Nled Service is not running after deleting LED deatils for %s. Delete LED Registry test Failed"),
                                 szLedName);

                    dwRetVal = TPR_FAIL;
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Nled Service is successfully running even after deleting LED details for %s"),
                                 szLedName);

                    dwRetVal = TPR_PASS;
                }
                goto end;
            }
        }

        RegCloseKey(hLEDKey);

        //Next LED.
        dwCurLEDIndex++;
        dwLEDNameSize = gc_dwMaxKeyNameLength + 1;

        //Enumerating for Next LED.
        lReturnValue = RegEnumKeyEx(hNLEDConfigKey,
                                    dwCurLEDIndex,
                                    szLedName,
                                    &dwLEDNameSize,
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
    }

end:

    //Restore the Original LED Details
    if(fLedDetailsChanged)
    {
        if (!RestoreLEDDetails (&LED_Details, &hLEDKey))
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to restore LED Details. Delete LED Registry test Failed."));

            dwRetVal = TPR_FAIL;
        }
        else
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("LED Registry Restored Successfully. Delete LED Registry test Passed."));
        }

        //Unload and Load Service for affecting updated Registry values.
        if(!UnloadLoadService())
        {
            dwRetVal = TPR_FAIL;
            FAIL("Failed to UnLoad-Load the Nled Service");
        }
    }

    //Close key handles
    if(hNLEDConfigKey != NULL)
    {
        RegCloseKey(hNLEDConfigKey);
    }

    if(hLEDKey != NULL)
    {
        RegCloseKey(hLEDKey);
    }

    if(hEventKey != NULL)
    {
        RegCloseKey(hEventKey);
    }

    return dwRetVal;
}

//*****************************************************************************
DWORD NledServiceCorruptLEDRegistry (VOID)
//
//  Parameters:      NONE
//
//  Return Value:    TPR_PASS On Success,
//                   TPR_FAIL On Failure
//
//  Function to Corrupt Led Details Registry for Testing the Robustness of
//  NLED driver.
//*****************************************************************************
{
    //Declaring the variables
    HKEY hNLEDConfigKey = NULL;
    DWORD dwRetVal = TPR_PASS;
    HKEY hLEDKey = NULL;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLEDNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwCurLEDIndex = 0;
    HKEY hEventKey = NULL;
    LONG lReturnValue = 0;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;
    NLED_EVENT_DETAILS nledEventDetailsStruct = {0};
    NLED_LED_DETAILS LED_Details = {0};
    BOOL fLedDetailsChanged = FALSE;

    //Opening Config Key
    lReturnValue = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                REG_NLED_KEY_CONFIG_PATH,
                                0,
                                0,
                                &hNLEDConfigKey);

    if(lReturnValue != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to open the Config directory for LEDs. Corrupt LED Registry Test Failed."));

        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Enumerating for LEDs
    lReturnValue = RegEnumKeyEx(hNLEDConfigKey,
                                dwCurLEDIndex,
                                szLedName,
                                &dwLEDNameSize,
                                NULL,
                                NULL,
                                NULL,
                                NULL);

    if(ERROR_NO_MORE_ITEMS == lReturnValue)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("No LEDs configured in the registry. Corrupt LED Registry Test Failed."));

        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Iterating until All LEDS
    while(lReturnValue ==  ERROR_SUCCESS)
    {
        //Open LED Registry
        lReturnValue = RegOpenKeyEx(hNLEDConfigKey,
                                    szLedName,
                                    0,
                                    KEY_ALL_ACCESS,
                                    &hLEDKey);

        if(lReturnValue != ERROR_SUCCESS)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to open the LED registry. Corrupt LED Registry Test Failed."));

            dwRetVal = TPR_FAIL;
            goto end;
        }
        else
        {
            //Enumerating LED Event
            lReturnValue = RegEnumKeyEx(hLEDKey,
                                        0,
                                        szEventName,
                                        &dwEventNameSize,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL);

            if(ERROR_NO_MORE_ITEMS == lReturnValue)
            {
                //Log Error Message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("No Events Configured for %s LED"),
                             szLedName);

                dwRetVal = TPR_SKIP;
            }

            //Iteration for All Events in the LED.
            while(ERROR_SUCCESS == lReturnValue)
            {
                //Opening Event Key
                lReturnValue = RegOpenKeyEx(hLEDKey,
                                            szEventName,
                                            0,
                                            KEY_ALL_ACCESS,
                                            &hEventKey);

                if(lReturnValue != ERROR_SUCCESS)
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to open the LED registry. Corrupt LED Registry Test Failed."));

                    dwRetVal = TPR_FAIL;
                    goto end;
                }

                //Backup Registry Values
                if (!ReadEventDetails(hLEDKey, hEventKey, &nledEventDetailsStruct))
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to Read event details. Corrupt LED Registry Test Failed."));

                    dwRetVal = TPR_FAIL;
                    goto end;
                }

                if (!ReadLEDDetails (&LED_Details, &hLEDKey))
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable To Read LED Details. Corrupt LED Registry Test Failed."));

                    dwRetVal = TPR_FAIL;
                    goto end;
                }

                //Corrupt LED Registry
                if (!CorruptLEDDetails (&hLEDKey))
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable To Corrupt LED Registry: Test Failed"));

                    dwRetVal = TPR_FAIL;
                    goto end;
                }
                else
                {
                    fLedDetailsChanged = TRUE;
                }

                if(!UnloadLoadService())
                {
                    dwRetVal = TPR_FAIL;
                    FAIL("Failed to UnLoad-Load the Nled Service");
                    goto end;
                }

                //Wait for 1 second
                Sleep(SLEEP_FOR_ONE_SECOND);

                LPWSTR lpServiceName = L"nledsrv.dll";

                //Check if the Changes in Registry are affected.
                if(!IsServiceRunning(lpServiceName))
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Service is not running after corrupting LED details for %s. Corrupt LED Registry Test Failed"),
                                 szLedName);

                    dwRetVal = TPR_FAIL;
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Service is successfully running even after corrupting the Led Details for %s. Corrupt LED Registry Test is passed"),
                                 szLedName);

                    dwRetVal = TPR_PASS;
                }

                goto end;
            }
        }

        RegCloseKey(hLEDKey);

        //Next LED.
        dwCurLEDIndex++;
        dwLEDNameSize = gc_dwMaxKeyNameLength + 1;

        //Enumerating for Next LED.
        lReturnValue = RegEnumKeyEx(hNLEDConfigKey,
                                    dwCurLEDIndex,
                                    szLedName,
                                    &dwLEDNameSize,
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
    }

end:

    //Restore Original LED Details
    if(fLedDetailsChanged)
    {
        if (!RestoreLEDDetails(&LED_Details, &hLEDKey))
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to restore LED Details. Corrupt LED Registry Test Failed."));

            dwRetVal = TPR_FAIL;
        }
        else
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("LED Registry Restored Successfully"));
        }

        //Unload and Load Service for affecting updated Registry values.
        if(!UnloadLoadService())
        {
            dwRetVal = TPR_FAIL;
            FAIL("Failed to UnLoad-Load the Nled Service");
        }
    }

    //Close key handles
    if(hNLEDConfigKey != NULL)
    {
        RegCloseKey(hNLEDConfigKey);
    }

    if(hLEDKey != NULL)
    {
        RegCloseKey(hLEDKey);
    }

    if(hEventKey != NULL)
    {
        RegCloseKey(hEventKey);
    }

    return dwRetVal;
}

//*****************************************************************************
BOOL IsServiceRunning(LPWSTR lpszDllServiceName)
//
//  Parameters:   Name of the service whose running status is to be checked.
//
//  Return Value: TRUE if service is Running/Success and FALSE on Failure
//
//  To Check if a particular service like NLED SERVICE is running on Target
//  Hardware.
//*****************************************************************************
{
    //Declaring the variables
    BOOL fReturn              = FALSE;
    DWORD dwServiceEntries    = 0;
    DWORD dwBufferLen         = 10000;
    BYTE  rgbBuffer[MAX_SIZE];
    BOOL  fResult             = FALSE;

    // validate input Service Name
    if (!lpszDllServiceName)
    {
        goto Error;
    }

    // Enumerate all the services running on Target Hardware
    fResult = EnumServices(rgbBuffer, &dwServiceEntries, &dwBufferLen);

    if (!fResult)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL, TEXT("Enum Service Failed"));
        goto Error;
    }

    // Get the ServiceEnumInfo struct for each service in order to compare its dll name
    ServiceEnumInfo *serviceEnumInfo = (ServiceEnumInfo *)&rgbBuffer;

    for (INT i=0; i<(INT)dwServiceEntries; i++)
    {
        if (wcsncmp(serviceEnumInfo[i].szDllName,
                    lpszDllServiceName, wcslen(lpszDllServiceName)) == 0)
        {
            // Found our service in the list of running services
            fReturn = TRUE;
            break;
        }
    }

    return fReturn;

Error:

    g_pKato->Log(LOG_DETAIL, TEXT("IsService Running Failed"));
    return fReturn;

}

//*****************************************************************************
BOOL  DeleteLEDRegistry (HKEY *hLedKey)
//
//  Parameters:   Handle to the Led
//
//  Return Value: TRUE on Success and FALSE on Failure
//
//  Function to Delete Led Details Registry Values
//*****************************************************************************
{
    //Declaring the variables
    DWORD dwRetVal = 0;

    //Deleting the AdjustType Value
    dwRetVal = RegDeleteValue (*hLedKey, REG_ADJUSTTYPE_VALUE);

    if (dwRetVal != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL, TEXT("Unable to delete Adjust Type Value"));
    }

    //Deleting the LedNum Value
    dwRetVal = RegDeleteValue (*hLedKey, REG_LED_NUM_VALUE);

    if (dwRetVal != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL, TEXT("Unable to delete LedNum Value"));
    }

    //Deleting the Blinkable Value
    dwRetVal = RegDeleteValue (*hLedKey, REG_BLINKABLE_VALUE);

    if (dwRetVal != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL, TEXT("Unable to delete Blinkable Value"));
    }

    //Deleting the Cycle Adjust Value
    dwRetVal = RegDeleteValue (*hLedKey, REG_CYCLEADJUST_VALUE);

    if (dwRetVal != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL, TEXT("Unable to delete Cycle Adjust Value"));
    }

    //Deleting the LedGroup Value
    dwRetVal = RegDeleteValue (*hLedKey, REG_LED_GROUP_VALUE);

    if (dwRetVal != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL, TEXT("Unable to delete LedGroup Value"));
    }

    //Deleting the LedType Value
    dwRetVal = RegDeleteValue (*hLedKey, REG_LED_TYPE_VALUE);

    if (dwRetVal != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL, TEXT("Unable to delete LedType Value"));
    }

    //Deleting the Metacycle Value
    dwRetVal = RegDeleteValue (*hLedKey, REG_METACYCLE_VALUE);

    if (dwRetVal != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL, TEXT("Unable to delete Meta Cycle Value"));
    }

    //Deleting the SWBlinkCtrl Value
    dwRetVal = RegDeleteValue (*hLedKey, REG_SWBLINKCTRL_VALUE);

    if (dwRetVal != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL, TEXT("Unable to delete SWBlinkCtrl Value"));
    }

    return TRUE;
}

//*****************************************************************************
BOOL ReadLEDDetails (NLED_LED_DETAILS *LEDDetails, HKEY *hLEDKey)
//
//  Parameters:      Handle to Led Registry,
//                   [Out]Pointer to hold LED Details Structure.
//
//  Return Value: TRUE on Success and FALSE on Failure
//
//  Function to read the Led Details from the Registry
//*****************************************************************************
{
    //Declaring the variables
    DWORD dwRetVal = 0;
    DWORD dwDataSize = sizeof (DWORD);

    //Reading the Adjust Type Value
    dwRetVal = RegQueryValueEx(*hLEDKey,
                               REG_ADJUSTTYPE_VALUE,
                               NULL, NULL,
                               (PBYTE)&(LEDDetails -> AdjustType),
                               &dwDataSize);

    if (dwRetVal != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Failed reading LED Details: AdjustType Value"));

        LEDDetails -> AdjustType = -1;
    }

    //Reading the Blinkable Value
    dwRetVal = RegQueryValueEx(*hLEDKey,
                               REG_BLINKABLE_VALUE,
                               NULL,
                               NULL,
                               (PBYTE)&(LEDDetails -> Blinkable),
                               &dwDataSize);

    if (dwRetVal != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Failed reading LED Details: Blinkable Value"));
        goto end;
    }

    //Reading the CycleAdjust Value
    dwRetVal = RegQueryValueEx(*hLEDKey,
                               REG_CYCLEADJUST_VALUE,
                               NULL,
                               NULL,
                               (PBYTE)&(LEDDetails -> CycleAdjust),
                               &dwDataSize);

    if (dwRetVal != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Failed at reading LED Details: CycleAdjust Value"));

        LEDDetails -> CycleAdjust = -1;
    }

    //Reading the Led Group Value
    dwRetVal = RegQueryValueEx(*hLEDKey,
                               REG_LED_GROUP_VALUE,
                               NULL,
                               NULL,
                               (PBYTE)&(LEDDetails -> LedGroup),
                               &dwDataSize);

    if (dwRetVal != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Failed reading LED Details: LedGroup Value"));

        LEDDetails -> LedGroup = -1;
    }

    //Reading the LedNum Value
    dwRetVal = RegQueryValueEx(*hLEDKey,
                               REG_LED_NUM_VALUE,
                               NULL,
                               NULL,
                               (PBYTE)&(LEDDetails -> LedNum),
                               &dwDataSize);

    if (dwRetVal != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Failed reading LED Details: LedNum Value"));
        goto end;
    }

    //Reading the LedType Value
    dwRetVal = RegQueryValueEx(*hLEDKey,
                               REG_LED_TYPE_VALUE,
                               NULL,
                               NULL,
                               (PBYTE)&(LEDDetails -> LedType),
                               &dwDataSize);

    if (dwRetVal != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Failed reading LED Details: LedType Value"));
        goto end;
    }

    //Reading the Metacycle Value
    dwRetVal = RegQueryValueEx(*hLEDKey,
                               REG_METACYCLE_VALUE,
                               NULL,
                               NULL,
                               (PBYTE)&(LEDDetails -> MetaCycle),
                               &dwDataSize);

    if (dwRetVal != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Failed reading LED Details: MetaCycle Value"));

        LEDDetails -> MetaCycle = -1;
    }

    //Reading the SWBlinkCtrl Value
    dwRetVal = RegQueryValueEx(*hLEDKey,
                               REG_SWBLINKCTRL_VALUE,
                               NULL,
                               NULL,
                               (PBYTE)&(LEDDetails -> SWBlinkCtrl),
                               &dwDataSize);

    if (dwRetVal != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Failed reading LED Details: SWBlinkCtrl Value"));

        LEDDetails -> SWBlinkCtrl = -1;
    }

    return TRUE;

end:
    g_pKato->Log(LOG_DETAIL, TEXT("Reading LED Details failed"));
    return FALSE;
}

//*****************************************************************************
BOOL RestoreLEDDetails (NLED_LED_DETAILS *LEDDetails, HKEY *hLEDKey)
//
//  Parameters:      Nled Key,
//                  [OUT]Pointer to LED Details Structure.
//
//  Return Value: TRUE on Success and FALSE on Failure
//
//  Function to Restore Led Details Registry values
//*****************************************************************************
{
    //Restoring Adjust Type Value
    if(LEDDetails -> AdjustType != -1)
    {
        if ((RegSetValueEx (*hLEDKey,
                            REG_ADJUSTTYPE_VALUE,
                            0,
                            REG_DWORD,
                            (CONST BYTE*) &(LEDDetails -> AdjustType),
                            sizeof(DWORD)) != ERROR_SUCCESS))
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL, TEXT("Last Error = %d"), GetLastError ());
            goto end;
        }
    }

    //Restoring Blinkable Value
    if ((RegSetValueEx (*hLEDKey,
                        REG_BLINKABLE_VALUE,
                        0,
                        REG_DWORD,
                        (CONST BYTE*) &(LEDDetails -> Blinkable),
                        sizeof(DWORD)) != ERROR_SUCCESS))
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL, TEXT("Last Error = %d"), GetLastError ());
        goto end;
    }

    //Restoring Cycle Adjust Value
    if(LEDDetails -> CycleAdjust != -1)
    {
        if ((RegSetValueEx(*hLEDKey,
                           REG_CYCLEADJUST_VALUE,
                           0,
                           REG_DWORD,
                           (CONST BYTE*) &(LEDDetails -> CycleAdjust),
                           sizeof(DWORD)) != ERROR_SUCCESS))
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL, TEXT("Last Error = %d"), GetLastError ());
            goto end;
        }
    }

    //Restoring LED Group Value
    if(LEDDetails -> LedGroup != -1)
    {
        if ((RegSetValueEx (*hLEDKey,
                            REG_LED_GROUP_VALUE,
                            0,
                            REG_DWORD,
                            (CONST BYTE*) &(LEDDetails -> LedGroup),
                            sizeof(DWORD)) != ERROR_SUCCESS))
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL, TEXT("Last Error = %d"), GetLastError ());
            goto end;
        }
    }

    //Restoring LED Num Value
    if ((RegSetValueEx (*hLEDKey,
                        REG_LED_NUM_VALUE,
                        0,
                        REG_DWORD,
                        (CONST BYTE*) &(LEDDetails -> LedNum),
                        sizeof(DWORD)) != ERROR_SUCCESS))
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL, TEXT("Last Error = %d"), GetLastError ());
        goto end;
    }

    //Restoring LED Type Value
    if ((RegSetValueEx(*hLEDKey,
                       REG_LED_TYPE_VALUE,
                       0,
                       REG_DWORD,
                       (CONST BYTE*) &(LEDDetails -> LedType),
                       sizeof(DWORD)) != ERROR_SUCCESS))
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL, TEXT("Last Error = %d"), GetLastError ());
        goto end;
    }

    //Restoring Metacycle Value
    if(LEDDetails -> MetaCycle != -1)
    {
        if ((RegSetValueEx(*hLEDKey,
                           REG_METACYCLE_VALUE,
                           0,
                           REG_DWORD,
                           (CONST BYTE*) &(LEDDetails -> MetaCycle),
                           sizeof(DWORD)) != ERROR_SUCCESS))
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL, TEXT("Last Error = %d"), GetLastError ());
            goto end;
        }
    }

    //Restoring SWBlinkCtrl Value
    if(LEDDetails -> SWBlinkCtrl != -1)
    {
        if ((RegSetValueEx (*hLEDKey,
                            REG_SWBLINKCTRL_VALUE,
                            0,
                            REG_DWORD,
                            (CONST BYTE*) &(LEDDetails -> SWBlinkCtrl),
                            sizeof(DWORD)) != ERROR_SUCCESS))
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL, TEXT("Last Error = %d"), GetLastError ());
            goto end;
        }
    }

    return TRUE;

end:
    g_pKato->Log(LOG_DETAIL, TEXT("Reading LED Details failed"));
    return FALSE;
}

//*****************************************************************************
BOOL CorruptLEDDetails (HKEY *hLedKey)
//
//  Parameters:   NLed key
//
//  Return Value: TRUE on Success and FALSE on Failure
//
//  Function to corrupt Led Details Registry for Testing the Robustness
//  of NLED driver.
//*****************************************************************************
{
    //Declaring the variables
    DWORD dwInvalidSwBlinkCtrl  =    0xfffffff;
    DWORD dwInvalidMetaCycle    =    0xfffffff;
    DWORD dwInvalidLedType      =    0xfffffff;
    DWORD dwInvalidLedNum       =    0xfffffff;
    DWORD dwInvalidLedGroup     =    0xfffffff;
    DWORD dwInvalidCycleAdjust  =    0xfffffff;
    DWORD dwInvalidBlinkable    =    0xfffffff;
    DWORD dwInvalidAdjustType   =    0xfffffff;
    DWORD dwRetVal              =    0;

    //Setting an invalid value for SwBlinkCtrl
    if(ERROR_SUCCESS == RegQueryValueEx(*hLedKey,
                                        REG_SWBLINKCTRL_VALUE,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL))
    {
        dwRetVal = RegSetValueEx (*hLedKey,
                                  REG_SWBLINKCTRL_VALUE,
                                  0,
                                  REG_DWORD,
                                  (PBYTE) (&dwInvalidSwBlinkCtrl),
                                  sizeof(dwInvalidSwBlinkCtrl));

        if(dwRetVal != ERROR_SUCCESS)
        {
            //Log the Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable To Corrupt SWBlinkCtrl Value. Test Failed"));
            goto end;
        }
    }

    //Setting an invalid value for MetaCycle
    if(ERROR_SUCCESS == RegQueryValueEx(*hLedKey,
                                        REG_METACYCLE_VALUE,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL))
    {
        dwRetVal = RegSetValueEx (*hLedKey,
                                  REG_METACYCLE_VALUE,
                                  0,
                                  REG_DWORD,
                                  (PBYTE) (&dwInvalidMetaCycle),
                                  sizeof(dwInvalidMetaCycle));

        if(dwRetVal != ERROR_SUCCESS)
        {
            //Log the Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable To Corrupt Metacycle Value. Test Failed"));
            goto end;
        }
    }

    //Setting an invalid value for LedType
    dwRetVal = RegSetValueEx(*hLedKey,
                             REG_LED_TYPE_VALUE,
                             0,
                             REG_DWORD,
                             (PBYTE) (&dwInvalidLedType),
                             sizeof(dwInvalidLedType));

    if(dwRetVal != ERROR_SUCCESS)
    {
        //Log the Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable To Corrupt Led Type Value. Test Failed"));
        goto end;
    }

    //Setting an invalid value for LedNum
    dwRetVal = RegSetValueEx (*hLedKey,
                              REG_LED_NUM_VALUE,
                              0,
                              REG_DWORD,
                              (PBYTE) (&dwInvalidLedNum),
                              sizeof(dwInvalidLedNum));

    if(dwRetVal != ERROR_SUCCESS)
    {
        //Log the Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable To Corrupt Led Num Value. Test Failed"));
        goto end;
    }

    //Setting an invalid value for LedGroup
    if(ERROR_SUCCESS == RegQueryValueEx(*hLedKey,
                                        REG_LED_GROUP_VALUE,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL))
    {
        dwRetVal = RegSetValueEx (*hLedKey,
                                  REG_LED_GROUP_VALUE,
                                  0,
                                  REG_DWORD,
                                  (PBYTE) (&dwInvalidLedGroup),
                                  sizeof(dwInvalidLedGroup));

        if(dwRetVal != ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable To Corrupt Led Group Value. Test Failed"));
            goto end;
        }
    }

    //Setting an invalid value for CycleAdjust
    if(ERROR_SUCCESS == RegQueryValueEx(*hLedKey,
                                        REG_CYCLEADJUST_VALUE,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL))
    {
        dwRetVal = RegSetValueEx (*hLedKey,
                                  REG_CYCLEADJUST_VALUE,
                                  0,
                                  REG_DWORD,
                                  (PBYTE) (&dwInvalidCycleAdjust),
                                  sizeof(dwInvalidCycleAdjust));

        if(dwRetVal != ERROR_SUCCESS)
        {
            //Log the Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable To Corrupt Cycle Adjust Value. Test Failed"));
            goto end;
        }
    }

    //Setting an invalid value for Blinkable
    dwRetVal = RegSetValueEx (*hLedKey,
                              REG_BLINKABLE_VALUE,
                              0,
                              REG_DWORD,
                              (PBYTE) (&dwInvalidBlinkable),
                              sizeof(dwInvalidBlinkable));

    if(dwRetVal != ERROR_SUCCESS)
    {
        //Log the Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable To Corrupt Blinkable Value. Test Failed"));
        goto end;
    }

    //Setting an invalid value for AdjustType
    if(ERROR_SUCCESS == RegQueryValueEx(*hLedKey,
                                        REG_ADJUSTTYPE_VALUE,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL))
    {
        dwRetVal = RegSetValueEx (*hLedKey,
                                  REG_ADJUSTTYPE_VALUE,
                                  0,
                                  REG_DWORD,
                                  (PBYTE) (&dwInvalidAdjustType),
                                  sizeof(dwInvalidAdjustType));

        if(dwRetVal != ERROR_SUCCESS)
        {
            //Log the Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable To Corrupt Adjust Type Value. Test Failed"));
            goto end;
        }
    }

    return TRUE;

end:
    g_pKato->Log(LOG_DETAIL, TEXT("Corrupting LED Details failed"));
    return FALSE;
}

//*****************************************************************************
TESTPROCAPI NledServiceBlinkOffTest(UINT                   uMsg,
                                    TPPARAM                tpParam,
                                    LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Objective:
//          Raising the event of a Nled to turn it ON/OFF/BLINK through Service.
//  Service should change the state of the LED. Also from TUX calling the Core
//  DLL NLED APIs to change the state of the same LED. And checking that service
//  and NLED driver handle this scenario gracefully.
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Supporting Only Executing the Test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the service or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fServiceTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    //Returning pass if the device does not have any NLEDs
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("The device doesn't have any NLEDs so passing the test"));

        return TPR_PASS;
    }

    //Declaring the variables
    NLED_SETTINGS_INFO *pOriginalLedSettingsInfo = NULL;
    DWORD *pLedNumEventId = NULL;
    DWORD dwTotalEvents = 0;
    DWORD dwReturnValue = TPR_PASS;
    LONG lRetVal = 0;
    NLED_EVENT_DETAILS *pEventDetails_list = NULL;
    HKEY *hEventOrigKey = NULL;
    HKEY hTemporaryEventKey = NULL;
    HKEY hLEDKey = NULL;
    HKEY hNLEDConfigKey = NULL;
    HKEY hEventKey = NULL;
    HKEY hTempoEventKey = NULL;
    DWORD dwCurLEDIndex = 0;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLEDNameSize = gc_dwMaxKeyNameLength + 1;
    NLED_EVENT_DETAILS nledEventDetailsStruct;
    NLED_SETTINGS_INFO *pLedSettingsInfo  = NULL;
    DWORD dwBlinkable = 0;
    DWORD dwSWBlinkCtrl = 0;
    DWORD dwCurLEDEventIndex = 0;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwState = 0;

    // Allocates space to save origional LED setttings.
    SetLastError(ERROR_SUCCESS);
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];

    if(!pOriginalLedSettingsInfo)
    {
        FAIL("Memory allocation for NLED_SETTINGS_INFO failed");
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tLast Error = %d"),
                     GetLastError());

        return TPR_FAIL;
    }

    ZeroMemory(pOriginalLedSettingsInfo, GetLedCount() * sizeof(NLED_SETTINGS_INFO));

    // Save the Info Settings for all the LEDs and turn them off.
    lRetVal = saveAllLedSettings(pOriginalLedSettingsInfo, LED_OFF);

    if(!lRetVal)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    }

    //Pointer to an Array to store the Info if the Nled has any events configured
    pLedNumEventId = new DWORD[GetLedCount()];

    //Getting the Event Ids for all the Nleds
    lRetVal = GetNledEventIds(pLedNumEventId);

    if(TPR_PASS != lRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to retrieve total number of events for all the Leds."));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Calculating the total number of events of all the Leds
    for(DWORD i = 0; i < GetLedCount(); i++)
    {
        dwTotalEvents = dwTotalEvents + pLedNumEventId[i];
    }

    if(0 == dwTotalEvents)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("No LED has at least one event configured. Skipping the test."));

        dwReturnValue = TPR_SKIP;
        goto RETURN;
    }

    //Storing the event paths and unsetting all the events
    pEventDetails_list = new NLED_EVENT_DETAILS[dwTotalEvents];

    for(DWORD i=0;i<dwTotalEvents;i++)
    {
        ZeroMemory(&pEventDetails_list[i], sizeof(NLED_EVENT_DETAILS));
    }

    hEventOrigKey = new HKEY[dwTotalEvents];

    lRetVal = UnSetAllNledServiceEvents(pEventDetails_list,
                                        &hTemporaryEventKey,
                                        hEventOrigKey);

    if(!lRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to unset all the Nled Service Events"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Allocating memory
    pLedSettingsInfo  = new NLED_SETTINGS_INFO[1];
    ZeroMemory(pLedSettingsInfo, sizeof(NLED_SETTINGS_INFO));

    //Opening Config Key
    lRetVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                           REG_NLED_KEY_CONFIG_PATH,
                           0,
                           0,
                           &hNLEDConfigKey);

    if(lRetVal != ERROR_SUCCESS){
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to open the Config directory for LEDs"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Enumerating for LEDs
    lRetVal = RegEnumKeyEx(hNLEDConfigKey,
                           dwCurLEDIndex,
                           szLedName,
                           &dwLEDNameSize,
                           NULL,
                           NULL,
                           NULL,
                           NULL);

    if(ERROR_NO_MORE_ITEMS == lRetVal)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("No LEDs configured in the registry"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Iterating until All LEDS
    while(lRetVal ==  ERROR_SUCCESS)
    {
        //Open LED Registry
        lRetVal = RegOpenKeyEx(hNLEDConfigKey,
                               szLedName,
                               0,
                               KEY_ALL_ACCESS,
                               &hLEDKey);

        if(lRetVal != ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to open the LED registry"));

            dwReturnValue = TPR_FAIL;
            goto RETURN;
        }
        else
        {
            lRetVal = GetDwordFromReg(hLEDKey, REG_BLINKABLE_VALUE, &dwBlinkable);

            if(!lRetVal)
            {
                //Log the error message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Blinkable field not configured in registry for LED %s"),
                             szLedName);

                dwReturnValue = TPR_FAIL;
                goto RETURN;
            }

            //LED should blink if it can blink or SWBlinkCtrl is set to 1
            //SWBlinkCtrl field is optional
            GetDwordFromReg(hLEDKey, REG_SWBLINKCTRL_VALUE, &dwSWBlinkCtrl);

            if(dwBlinkable || dwSWBlinkCtrl)
            {
                //Re-Initializing the index variable
                dwCurLEDEventIndex = 0;

                //Enumerating LED Event
                lRetVal = RegEnumKeyEx(hLEDKey,
                                       dwCurLEDEventIndex,
                                       szEventName,
                                       &dwEventNameSize,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL);

                if(ERROR_NO_MORE_ITEMS == lRetVal)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("No events configured for %s in the registry"),
                                 szLedName);

                    dwReturnValue = TPR_SKIP;
                }

                //Got the first Led that has blinking support and also has an event configured
                if(ERROR_SUCCESS == lRetVal)
                {
                    //Opening the Event Key
                    lRetVal = RegOpenKeyEx(hLEDKey,
                                           szEventName,
                                           0,
                                           KEY_ALL_ACCESS,
                                           &hEventKey);

                    if(lRetVal != ERROR_SUCCESS)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable to open the Event registry"));

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Backup  Event Registry Values
                    lRetVal = ReadEventDetails(hLEDKey,
                                               hEventKey,
                                               &nledEventDetailsStruct);

                    if (!lRetVal)
                    {
                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Set LED State to Blink
                    dwState = LED_BLINK;

                    //Set Event State to Blink.
                    lRetVal = RegSetValueEx (hEventKey,
                                             REG_EVENT_STATE_VALUE,
                                             0,
                                             REG_DWORD,
                                             (PBYTE)&dwState,
                                             sizeof(DWORD));

                    if(lRetVal != ERROR_SUCCESS)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable To Set State for %s Event for Nled %u"),
                                     szEventName,
                                     nledEventDetailsStruct.dwLedNum);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Create Temporary Trigger and Modify the Paths of two events to Temporary Trigger
                    //Set the temporary Trigger

                    //Create  Temporary Event
                    lRetVal = Create_Set_TemporaryTrigger(hEventKey,
                                                          TEMPO_EVENT_PATH,
                                                          &nledEventDetailsStruct,
                                                          &hTempoEventKey);

                    if (!lRetVal)
                    {
                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Unload and Load Service for the Path Change to get affect
                    lRetVal = UnloadLoadService();

                    if(!lRetVal)
                    {
                        dwReturnValue = TPR_FAIL;
                        FAIL("Failed to UnLoad-Load the Nled Service");
                        goto RETURN;
                    }

                    //Raise the Temporary Trigger
                    lRetVal = RaiseTemporaryTrigger(hTempoEventKey,
                                                    &nledEventDetailsStruct);

                    if(!lRetVal)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable to raise the event for %s from the test"),
                                     szEventName);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Wait for 1 sec for the service to act
                    Sleep(SLEEP_FOR_ONE_SECOND);

                    //Get Currrent LED Settings
                    lRetVal = GetCurrentNLEDDriverSettings(nledEventDetailsStruct,
                                                           pLedSettingsInfo);

                    if(!lRetVal)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable to get the current led settings for Nled %u"),
                                     nledEventDetailsStruct.dwLedNum);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Check if Led State is Blinking.
                    if (pLedSettingsInfo->OffOnBlink != LED_BLINK)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable to blink the %u Nled"),
                                     nledEventDetailsStruct.dwLedNum);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Now change the State of LED to OFF.
                    pLedSettingsInfo->OffOnBlink = LED_OFF;

                    lRetVal = NLedSetDevice(NLED_SETTINGS_INFO_ID, pLedSettingsInfo);

                    if(!lRetVal)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable to turn off the %u Nled"),
                                     nledEventDetailsStruct.dwLedNum);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Get Present NLED Settings.
                    lRetVal = GetCurrentNLEDDriverSettings(nledEventDetailsStruct,
                                                           pLedSettingsInfo);

                    if(!lRetVal)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable to get the current led settings for Nled %u"),
                                     nledEventDetailsStruct.dwLedNum);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Check if the LED is set to OFF.
                    if(pLedSettingsInfo->OffOnBlink != LED_OFF)
                    {
                        FAIL("Nled Driver failed to turn off the Led which was being blinked by Nled Service");
                        dwReturnValue = TPR_FAIL;
                    }
                    else
                    {
                        SUCCESS("Nled Driver successfully turned off the Led which was being blinked by Nled Service");
                        dwReturnValue = TPR_PASS;
                    }

                    goto RETURN;
                }
            }
            else
            {
                //Log the error message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("%s does support blinking"), szLedName);
            }
        }

        RegCloseKey(hLEDKey);

        //Next LED.
        dwCurLEDIndex++;
        dwLEDNameSize = gc_dwMaxKeyNameLength + 1;

        //Enumerating for Next LED.
        lRetVal = RegEnumKeyEx(hNLEDConfigKey,
                               dwCurLEDIndex,
                               szLedName,
                               &dwLEDNameSize,
                               NULL,
                               NULL,
                               NULL,
                               NULL);
    }

    if(TPR_PASS == dwReturnValue)
    {
        dwReturnValue = TPR_SKIP;
        g_pKato->Log(LOG_DETAIL,
                     TEXT("None of the Nleds on the device support blinking. Skipping the test."));
    }

RETURN:
    //Delete Temporary Trigger
    if (hTempoEventKey)
    {
        //Resetting Temporary Trigger.
        lRetVal = Unset_TemporaryTrigger(hEventKey,
                                         hTempoEventKey,
                                         &nledEventDetailsStruct);

        if(!lRetVal)
        {
            dwReturnValue = TPR_FAIL;
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to unset temporary trigger for Nled %u"),
                         nledEventDetailsStruct.dwLedNum);
        }

        lRetVal = DeleteTemporaryTrigger(hTempoEventKey,
                                         TEMPO_EVENT_PATH,
                                         pLedSettingsInfo,
                                         &nledEventDetailsStruct);

        if(!lRetVal)
        {
            dwReturnValue = TPR_FAIL;
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to delete temporary trigger for Nled %u"),
                         nledEventDetailsStruct.dwLedNum);
        }
    }

    if (hEventKey)
    {
        //Restrore state for Event
        lRetVal = RegSetValueEx(hEventKey,
                                REG_EVENT_STATE_VALUE,
                                0,
                                REG_DWORD,
                                (PBYTE)&(nledEventDetailsStruct.dwState),
                                sizeof(DWORD));

        if(lRetVal != ERROR_SUCCESS)
        {
            dwReturnValue = TPR_FAIL;
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable To Restore State of the Event"));
        }

        RegCloseKey(hEventKey);
    }

    //Restore back the original Event Paths
    lRetVal = RestoreAllNledServiceEventPaths(pEventDetails_list,
                                              hEventOrigKey,
                                              dwTotalEvents);

    if(!lRetVal)
    {
        dwReturnValue = TPR_FAIL;
        FAIL("Unable to restore back the original event paths");
    }

    //Delete the temporary registry key
    if(hTemporaryEventKey != NULL)
    {
        lRetVal =  RegCloseKey(hTemporaryEventKey);

        if(lRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Closing Failed. Last Error = %ld"),
                         GetLastError());

            dwReturnValue = TPR_FAIL;
        }

        lRetVal = RegDeleteKey(HKEY_LOCAL_MACHINE, TEMPORARY_EVENT_PATH);

        if(lRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Deletion Failed. Last Error = %ld"),
                         GetLastError());

            dwReturnValue = TPR_FAIL;
        }
    }

    if(hLEDKey)
    {
        RegCloseKey(hLEDKey);
    }

    if(hNLEDConfigKey)
    {
        RegCloseKey(hNLEDConfigKey);
    }

    if(pLedNumEventId != NULL)
    {
        delete[] pLedNumEventId;
        pLedNumEventId = NULL;
    }

    if(hEventOrigKey != NULL)
    {
        delete[] hEventOrigKey;
        hEventOrigKey = NULL;
    }

    if(pEventDetails_list != NULL)
    {
        delete[] pEventDetails_list;
        pEventDetails_list = NULL;
    }

    if(pLedSettingsInfo != NULL)
    {
        delete[] pLedSettingsInfo;
        pLedSettingsInfo = NULL;
    }

    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo != NULL)
    {
        lRetVal = setAllLedSettings(pOriginalLedSettingsInfo);

        if(!lRetVal)
        {
            FAIL("Failed to restore the previous LED settings");
            dwReturnValue = TPR_FAIL;
        }

        //De-Allocating the memory
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    return dwReturnValue;
}

//*****************************************************************************
TESTPROCAPI NledServiceEventPriorityOrderTest(UINT                   uMsg,
                                              TPPARAM                tpParam,
                                              LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Test to Check if proper priority order is followed when all the
//  events are triggered one by one for a LED. Also checking that when all the
//  other high priority events are cleared the lower priority event is still
//  executing.
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Supporting Only Executing the Test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the service or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fServiceTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    //Returning pass if the device does not have any NLEDs
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("The device doesn't have any NLEDs so passing the test"));

        return TPR_PASS;
    }

    //Declaring the variables
    DWORD dwReturn = 0;
    DWORD *pLedNumEventId = NULL;
    DWORD dwTotalEvents = 0;
    DWORD dwReturnValue = TPR_FAIL;
    NLED_EVENT_DETAILS *pEventDetails_list = NULL;
    HKEY *hEventOrigKey = NULL;
    HKEY hTemporaryEventKey = NULL;
    HKEY hNledConfigKey = NULL;
    DWORD dwCurLEDIndex = 0;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLedNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwFinalEventCount = 0;
    DWORD dwLedWithMoreEvents = 0;
    DWORD dwLen = sizeof(DWORD);;
    DWORD dwLedNumber = 0;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwState = 0;
    DWORD dwGroupNledCount = 0;
    DWORD dwLowestPriority = 0;
    DWORD dwHighestPriority = 0;
    BOOL fInitializeOnce = TRUE;
    HKEY hNledNameKey = NULL;
    HKEY *hEventKey = NULL;
    HKEY *hTempoEventKey = NULL;
    NLED_EVENT_DETAILS *pNledEventDetails = NULL;
    NLED_SETTINGS_INFO *pNledSettingsInfo  = NULL;
    LPTSTR *lptStrTempoEventPath = NULL;
    BOOL fEventTrigger = FALSE;
    DWORD *pLedNumGroupId = NULL;
    TCHAR szTempoEventString[255] = TEMPO_EVENT_PATH;
    WCHAR wszString[10];
    DWORD dwSameLedEventCount = 0;
    long lRetVal =0;

    //Storing the original LED settings
    NLED_SETTINGS_INFO* pOriginalLedSettingsInfo = NULL;

    // Allocates space to save origional LED setttings.
    SetLastError(ERROR_SUCCESS);
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];

    if(!pOriginalLedSettingsInfo)
    {
        FAIL("Memory allocation for NLED_SETTINGS_INFO failed");
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tLast Error = %d"),
                     GetLastError());

        return TPR_FAIL;
    }

    ZeroMemory(pOriginalLedSettingsInfo, GetLedCount() * sizeof(NLED_SETTINGS_INFO));

    // Save the Info Settings for all the LEDs and turn them off.
    dwReturn = saveAllLedSettings(pOriginalLedSettingsInfo, LED_OFF);

    if(!dwReturn)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    }

    //Pointer to an Array to store the Info if the Nled has any events configured
    pLedNumEventId = new DWORD[GetLedCount()];

    //Getting the Event Ids for all the Nleds
    dwReturn = GetNledEventIds(pLedNumEventId);

    if(TPR_PASS != dwReturn)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to retrieve total number of events for all the Leds."));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Calculating the total number of events of all the Leds
    for(DWORD i = 0; i < GetLedCount(); i++)
    {
        dwTotalEvents = dwTotalEvents + pLedNumEventId[i];
    }

    if(0 == dwTotalEvents)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("No LED has at least one event configured.  Skipping the test."));

        dwReturnValue = TPR_SKIP;
        goto RETURN;
    }

    //Storing the event paths and unsetting all the events
    pEventDetails_list = new NLED_EVENT_DETAILS[dwTotalEvents];

    for(DWORD i=0;i<dwTotalEvents;i++)
    {
        ZeroMemory(&pEventDetails_list[i], sizeof(NLED_EVENT_DETAILS));
    }

    hEventOrigKey = new HKEY[dwTotalEvents];

    dwReturn = UnSetAllNledServiceEvents(pEventDetails_list,
                                          &hTemporaryEventKey,
                                         hEventOrigKey);

    if(!dwReturn)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to unset all the Nled Service Events"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Pointer to an Array to store the Group Id for the all the Leds
    pLedNumGroupId = new DWORD[GetLedCount()];

    //Getting the Group Ids for the Nleds
    dwReturn = GetNledGroupIds(pLedNumGroupId);

    if(TPR_PASS == dwReturn)
    {
        //Finding the Led that has more 'events' configured
        for(DWORD dwLedNum = 0; dwLedNum < GetLedCount(); dwLedNum++)
        {
            //Checking if the Nled is present in any Group
            if(pLedNumGroupId[dwLedNum] != 0)
            {
                //Check if this Nled has any events configured
                if(pLedNumEventId[dwLedNum] != 0)
                {
                    //Getting the Nled that has more Events configured
                    if(pLedNumEventId[dwLedNum] > dwFinalEventCount)
                    {
                        dwFinalEventCount = pLedNumEventId[dwLedNum];
                        dwLedWithMoreEvents = dwLedNum;
                    }
                }

                //Incrementing the count if the Nled is present in any group
                dwGroupNledCount++;
            }
        }
    }
    else
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to get the Nled Group Ids for all the Nleds"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    if(0 == dwGroupNledCount)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("None of the Nleds on the device is present in a Group. Skipping the test"));

        dwReturnValue = TPR_SKIP;
        goto RETURN;
    }
    else if(0 == dwFinalEventCount)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("None of the Nleds present in a group has atleast one event configured. Skipping the test"));

        dwReturnValue = TPR_SKIP;
        goto RETURN;

    }
    else if(ONE_LED == dwFinalEventCount)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("None of the Nleds present in a group has more than one event configured. Skipping the test"));

        dwReturnValue = TPR_SKIP;
        goto RETURN;
    }

    //Allocating memory
    hEventKey = new HKEY[dwFinalEventCount];
    hTempoEventKey = new HKEY[dwFinalEventCount];
    pNledEventDetails = new NLED_EVENT_DETAILS[dwFinalEventCount];
    pNledSettingsInfo  = new NLED_SETTINGS_INFO[dwFinalEventCount];

    for(DWORD i=0; i < dwFinalEventCount; i++)
    {
        ZeroMemory(&pNledEventDetails[i], sizeof(NLED_EVENT_DETAILS));
        ZeroMemory(&pNledSettingsInfo[i], sizeof(NLED_SETTINGS_INFO));
    }

    lptStrTempoEventPath = new LPTSTR[dwFinalEventCount];

    //Creating different tempoEvent paths for different events
    for(DWORD i = 0; i < dwFinalEventCount; i++)
    {
        lptStrTempoEventPath[i] = (TCHAR *)calloc(REG_EVENT_PATH_LENGTH, sizeof(TCHAR));
        wcscpy_s(lptStrTempoEventPath[i], REG_EVENT_PATH_LENGTH, szTempoEventString);

        //Concatinating to get the new tempo event path
         _itow_s(i, wszString, 10, 10);
        _tcscat_s(lptStrTempoEventPath[i], REG_EVENT_PATH_LENGTH, wszString);
    }

    //Checking for the priority order for the selected NLED
    //Opening the Config Key
    lRetVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE, REG_NLED_KEY_CONFIG_PATH, 0, 0, &hNledConfigKey);

    if(lRetVal != ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to open the Config key for LEDs"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Enumerating for LEDs
    lRetVal = RegEnumKeyEx(hNledConfigKey, dwCurLEDIndex, szLedName, &dwLedNameSize, NULL, NULL, NULL, NULL);

    if(ERROR_NO_MORE_ITEMS == lRetVal)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("No LEDs configured in the registry"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Iterating until All LEDS
    while(lRetVal ==  ERROR_SUCCESS)
    {
        //Open LED Registry
        lRetVal = RegOpenKeyEx(hNledConfigKey,
                               szLedName,
                               0,
                               KEY_ALL_ACCESS,
                               &hNledNameKey);

        if(lRetVal != ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to open the LED key: %s"),
                         szLedName);

            dwReturnValue = TPR_FAIL;
            goto RETURN;
        }
        else if(ERROR_SUCCESS == lRetVal)
        {
            //Querying the key values
            lRetVal = RegQueryValueEx(hNledNameKey,
                                      REG_LED_NUM_VALUE,
                                      NULL,
                                      NULL,
                                      (LPBYTE) &dwLedNumber, &dwLen);

            if(lRetVal != ERROR_SUCCESS)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to query the LedNum for %s"),
                             szLedName);

                RegCloseKey(hNledConfigKey);
                dwReturnValue = TPR_FAIL;
                goto RETURN;
            }

            //Checking if this Nled is the selected Nled which has more number of events configured
            if(dwLedWithMoreEvents == dwLedNumber)
            {
                fEventTrigger = TRUE;

                //Enumerating the LED Events
                lRetVal = RegEnumKeyEx(hNledNameKey,
                                       dwSameLedEventCount,
                                       szEventName,
                                       &dwEventNameSize,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL);

                //The selected Nled has atleast one event configured so no need of checking against ERROR_NO_MORE_ITEMS
                while(ERROR_SUCCESS == lRetVal)
                {
                    //Triggering event
                    //Opening the Event Key
                    lRetVal = RegOpenKeyEx(hNledNameKey,
                                           szEventName,
                                           0,
                                           KEY_ALL_ACCESS,
                                           &hEventKey[dwSameLedEventCount]);

                    if(lRetVal != ERROR_SUCCESS)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable to open the LED Event registry for %u LED"),
                                     dwLedNumber);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Backup Event Registry Values
                    lRetVal = ReadEventDetails(hNledNameKey,
                                               hEventKey[dwSameLedEventCount],
                                               &pNledEventDetails[dwSameLedEventCount]);

                    if (!lRetVal)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable to read %s Event-Details for %u LED"),
                                     szEventName,
                                     dwLedNumber);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Set the state as ON
                    dwState = LED_ON;
                    dwLen = sizeof(dwState);

                    //Set state to ON
                    lRetVal = RegSetValueEx (hEventKey[dwSameLedEventCount],
                                             REG_EVENT_STATE_VALUE,
                                             0,
                                             REG_DWORD,
                                             (PBYTE)&dwState,
                                             dwLen);

                    if(lRetVal != ERROR_SUCCESS)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable To Set the state"));

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Set Temporary Trigger
                    lRetVal = Create_Set_TemporaryTrigger(hEventKey[dwSameLedEventCount],
                                                          lptStrTempoEventPath[dwSameLedEventCount],
                                                          &pNledEventDetails[dwSameLedEventCount],
                                                          &hTempoEventKey[dwSameLedEventCount]);

                    if (!lRetVal)
                    {
                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Unload and Load Service for affecting updated Registry values.
                    lRetVal = UnloadLoadService();

                    if(!lRetVal)
                    {
                        dwReturnValue = TPR_FAIL;
                        FAIL("Failed to UnLoad-Load the Nled Service");
                        goto RETURN;
                    }

                    //Raising Trigger
                    lRetVal = RaiseTemporaryTrigger(hTempoEventKey[dwSameLedEventCount],
                                                    &pNledEventDetails[dwSameLedEventCount]);

                    if(!lRetVal)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable to raise the event for %s from the test"),
                                     szEventName);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Sleep for 1 second, and wait for the service to act
                    Sleep(SLEEP_FOR_ONE_SECOND);

                    //Get Current LED Settings
                    lRetVal = GetCurrentNLEDDriverSettings(pNledEventDetails[dwSameLedEventCount],
                                                           &pNledSettingsInfo[dwSameLedEventCount]);

                    if(!lRetVal)
                    {
                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Check for the Nled state for the first iteration
                    if(fInitializeOnce)
                    {
                        if(LED_ON != pNledSettingsInfo[dwSameLedEventCount].OffOnBlink)
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Nled Service failed to turn ON the %s - LED No. %u"),
                                         szEventName,
                                         dwLedNumber);

                            dwReturnValue = TPR_FAIL;
                            goto RETURN;
                        }
                        else
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Nled Service successfully executes the first active event: %s - LED No. %u"),
                                         szEventName,
                                         dwLedNumber);
                        }

                        //Initializing the highest priority variable
                        dwHighestPriority = pNledEventDetails[dwSameLedEventCount].dwPrio;

                        //Initializing the lowest priority variable as well
                        dwLowestPriority = pNledEventDetails[dwSameLedEventCount].dwPrio;

                        //Only for the first iteration
                        fInitializeOnce = FALSE;
                    }
                    else
                    {
                        //Check if the Nled is ON
                        if(dwHighestPriority > pNledEventDetails[dwSameLedEventCount].dwPrio)
                        {
                            if(LED_ON != pNledSettingsInfo[dwSameLedEventCount].OffOnBlink)
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("Nled Service failed to turn ON the Event: %s of LED No. %u"),
                                             szEventName,
                                             dwLedNumber);

                                dwReturnValue = TPR_FAIL;
                                goto RETURN;
                            }
                            else
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("Nled Service successfully executes the Event: %s of LED No. %u which has higher priority than the previous executing event"),
                                             szEventName,
                                             dwLedNumber);

                                 g_pKato->Log(LOG_DETAIL,
                                              TEXT("Nled Service successfully follows priority Order in executing the events."));
                            }

                            //Getting the highest priority in the Group
                            dwHighestPriority = pNledEventDetails[dwSameLedEventCount].dwPrio;
                        }
                        else
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("The %s event of Nled %u has priority less than the executing high priority event"),
                                         szEventName,
                                         dwLedNumber);

                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Nled Service successfully follows priority Order in executing the events."));
                        }

                        //Getting the lowest priority in the group
                        if(dwLowestPriority < pNledEventDetails[dwSameLedEventCount].dwPrio)
                        {
                            dwLowestPriority = pNledEventDetails[dwSameLedEventCount].dwPrio;
                        }
                    }

                    //Incrementing the Count
                    dwSameLedEventCount++;
                    dwEventNameSize = gc_dwMaxKeyNameLength + 1;

                    //Iterating for more Leds
                    lRetVal = RegEnumKeyEx(hNledNameKey,
                                           dwSameLedEventCount,
                                           szEventName,
                                           &dwEventNameSize,
                                           NULL,
                                           NULL,
                                           NULL,
                                           NULL);
                }
            }
        }

        //Closing the Led Key
        RegCloseKey(hNledNameKey);

        //Incrementing the count
        dwCurLEDIndex++;
        dwLedNameSize = gc_dwMaxKeyNameLength + 1;

        //Enumerating for Next LED.
        lRetVal = RegEnumKeyEx(hNledConfigKey,
                               dwCurLEDIndex,
                               szLedName,
                               &dwLedNameSize,
                               NULL,
                               NULL,
                               NULL,
                               NULL);
    }

    //Unset all the events except the one with the lowest priority
    for(DWORD dwTempoCount = 0; dwTempoCount < dwSameLedEventCount; dwTempoCount++)
    {
        if (hTempoEventKey[dwTempoCount])
        {
            //Resetting Temporary Trigger
            if(pNledEventDetails[dwTempoCount].dwPrio != dwLowestPriority)
            {
                lRetVal = Unset_TemporaryTrigger(hEventKey[dwTempoCount],
                                                 hTempoEventKey[dwTempoCount],
                                                 &pNledEventDetails[dwTempoCount]);

                if(!lRetVal)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to unset the trigger settings for %u Nled"),
                                 dwTempoCount);

                    dwReturnValue = TPR_FAIL;
                    continue;
                }
            }
        }
    }

    //Unload and Load Service for affecting updated Registry values.
    lRetVal = UnloadLoadService();

    if(!lRetVal)
    {
        dwReturnValue = TPR_FAIL;
        FAIL("Failed to UnLoad-Load the Nled Service");
        goto RETURN;
    }

    //Checking that only the lowest priority event is executing now
    for(dwTempoCount = 0; dwTempoCount < dwSameLedEventCount; dwTempoCount++)
    {
        //Get Current LED Settings
        lRetVal = GetCurrentNLEDDriverSettings(pNledEventDetails[dwTempoCount],
                                               &pNledSettingsInfo[dwTempoCount]);

        if(!lRetVal)
        {
            dwReturnValue = TPR_FAIL;
            goto RETURN;
        }

        if(pNledEventDetails[dwTempoCount].dwPrio == dwLowestPriority)
        {
            if(pNledSettingsInfo[dwTempoCount].OffOnBlink == LED_ON)
            {
                SUCCESS("Nled Service successfully executes the lowest priority event after the high priority events are Unset");
                dwReturnValue = TPR_PASS;
            }
            else
            {
                FAIL("Nled Service fails in executing the lowest priority event after the high priority events are Unset");
                dwReturnValue = TPR_FAIL;
            }
            goto RETURN;
        }
    }

RETURN:
    if(fEventTrigger)
    {
        //Delete the Temporary Trigger.
        for(DWORD dwTempoCount = 0; dwTempoCount < dwSameLedEventCount; dwTempoCount++)
        {
            if (hTempoEventKey[dwTempoCount])
            {
                //Resetting Temporary Trigger
                if(pNledEventDetails[dwTempoCount].dwPrio == dwLowestPriority)
                {
                    lRetVal = Unset_TemporaryTrigger(hEventKey[dwTempoCount],
                                                     hTempoEventKey[dwTempoCount],
                                                     &pNledEventDetails[dwTempoCount]);

                    if(!lRetVal)
                    {
                        dwReturnValue = TPR_FAIL;
                    }
                }

                lRetVal = DeleteTemporaryTrigger(hTempoEventKey[dwTempoCount],
                                                 lptStrTempoEventPath[dwTempoCount],
                                                 &pNledSettingsInfo[dwTempoCount],
                                                 &pNledEventDetails[dwTempoCount]);

                if(!lRetVal)
                {
                    dwReturnValue = TPR_FAIL;
                }
            }

            if (hEventKey[dwTempoCount])
            {

                //Restoring state
                lRetVal = RegSetValueEx (hEventKey[dwTempoCount],
                                         REG_EVENT_STATE_VALUE,
                                         0,
                                         REG_DWORD,
                                         (PBYTE)&(pNledEventDetails[dwTempoCount].dwState),
                                         sizeof(DWORD));

                if(lRetVal != ERROR_SUCCESS)
                {
                    dwReturnValue = TPR_FAIL;
                    g_pKato->Log(LOG_DETAIL, TEXT("Unable to Set back the State"));
                }

                RegCloseKey(hEventKey[dwTempoCount]);
            }
        }

        if(hNledNameKey != NULL)
        {
            RegCloseKey(hNledNameKey);
        }

        if(hNledConfigKey != NULL)
        {
            RegCloseKey(hNledConfigKey);
        }

        //Deallocating the memory
        if(hEventKey != NULL)
        {
            delete[] hEventKey;
            hEventKey = NULL;
        }

        if(hTempoEventKey != NULL)
        {
            delete[] hTempoEventKey;
            hTempoEventKey = NULL;
        }

        if(pNledEventDetails != NULL)
        {
            delete[] pNledEventDetails;
            pNledEventDetails = NULL;
        }

        if(pNledSettingsInfo != NULL)
        {
            delete[] pNledSettingsInfo;
            pNledSettingsInfo = NULL;
        }

        for(dwTempoCount = 0; dwTempoCount < dwSameLedEventCount; dwTempoCount++)
        {
            free(lptStrTempoEventPath[dwTempoCount]);
            lptStrTempoEventPath[dwTempoCount] = NULL;
        }

        if(lptStrTempoEventPath != NULL)
        {
            delete[] lptStrTempoEventPath;
            lptStrTempoEventPath = NULL;
        }
    }

    //Restore back the original Event Paths
    lRetVal = RestoreAllNledServiceEventPaths(pEventDetails_list,
                                              hEventOrigKey,
                                              dwTotalEvents);

    if(!lRetVal)
    {
        dwReturnValue = TPR_FAIL;
        FAIL("Unable to restore back the original event paths");
    }

    //Delete the temporary registry key
    if(hTemporaryEventKey != NULL)
    {
        lRetVal =  RegCloseKey(hTemporaryEventKey);

        if(lRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Closing Failed. Last Error = %ld"),
                         GetLastError());

            dwReturnValue = TPR_FAIL;
        }

        lRetVal = RegDeleteKey(HKEY_LOCAL_MACHINE, TEMPORARY_EVENT_PATH);

        if(lRetVal != ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Deletion Failed. Last Error = %ld"),
                         GetLastError());

            dwReturnValue = TPR_FAIL;
        }
    }

    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo != NULL)
    {
        lRetVal = setAllLedSettings(pOriginalLedSettingsInfo);

        if(!lRetVal)
        {
            FAIL("Failed to restore the previous LED settings");
            dwReturnValue = TPR_FAIL;
        }

        //De-Allocating the memory
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    if(pLedNumGroupId != NULL)
    {
        delete[] pLedNumGroupId;
        pLedNumGroupId = NULL;
    }

    if(pLedNumEventId != NULL)
    {
        delete[] pLedNumEventId;
        pLedNumEventId = NULL;
    }

    if(hEventOrigKey != NULL)
    {
        delete[] hEventOrigKey;
        hEventOrigKey = NULL;
    }

    if(pEventDetails_list != NULL)
    {
        delete[] pEventDetails_list;
        pEventDetails_list = NULL;
    }

    return dwReturnValue;
}

//*****************************************************************************
TESTPROCAPI NledServiceEventsSamePriorityTest(UINT                   uMsg,
                                              TPPARAM                tpParam,
                                              LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Checking that Nled Service successfully handles the error scenario
//  of two or more events of a Nled having same priority within a Group.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Supporting Only Executing the Test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the service or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fServiceTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    //Returning pass if the device does not have any NLEDs
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("The device doesn't have any NLEDs so passing the test"));

        return TPR_PASS;
    }

    //Declaring the variables
    LONG lRetVal = 0;
    DWORD *pLedNumEventId = NULL;
    DWORD dwTotalEvents = 0;
    DWORD dwReturnValue = TPR_FAIL;
    NLED_EVENT_DETAILS *pEventDetails_list = NULL;
    HKEY *hEventOrigKey = NULL;
    HKEY hTemporaryEventKey = NULL;
    HKEY hNledConfigKey = NULL;
    DWORD dwCurLEDIndex = 0;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLedNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwFinalEventCount = 0;
    DWORD dwLedWithMoreEvents = 0;
    DWORD dwLen = sizeof(DWORD);;
    DWORD dwLedNumber = 0;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwState = 0;
    HKEY hNledNameKey = NULL;
    HKEY *hEventKey = NULL;
    HKEY *hTempoEventKey = NULL;
    NLED_EVENT_DETAILS *pNledEventDetails = NULL;
    NLED_SETTINGS_INFO *pNledSettingsInfo  = NULL;
    LPTSTR *lptStrTempoEventPath = NULL;
    BOOL fEventTrigger = FALSE;
    DWORD dwGroupNledCount = 0;
    DWORD *pLedNumGroupId = NULL;
    TCHAR szTempoEvent[255] = TEMPO_EVENT_PATH;
    WCHAR wszString[10];
    DWORD dwSameLedEventCount = 0;
    DWORD dwPriority = 0;

    //Storing the original LED settings
    NLED_SETTINGS_INFO* pOriginalLedSettingsInfo = NULL;

    // Allocates space to save origional LED setttings.
    SetLastError(ERROR_SUCCESS);
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];

    if(!pOriginalLedSettingsInfo)
    {
        FAIL("Memory allocation for NLED_SETTINGS_INFO failed");
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tLast Error = %d"),
                     GetLastError());

        return TPR_FAIL;
    }

    ZeroMemory(pOriginalLedSettingsInfo, GetLedCount() * sizeof(NLED_SETTINGS_INFO));

    // Save the Info Settings for all the LEDs and turn them off.
    lRetVal = saveAllLedSettings(pOriginalLedSettingsInfo, LED_OFF);

    if(!lRetVal)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    }

    //Pointer to an Array to store the Info if the Nled has any events configured
    pLedNumEventId = new DWORD[GetLedCount()];

    //Getting the Event Ids for all the Nleds
    lRetVal = GetNledEventIds(pLedNumEventId);

    if(TPR_PASS != lRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to retrieve total number of events for all the Leds."));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Calculating the total number of events of all the Leds
    for(DWORD i = 0; i < GetLedCount(); i++)
    {
        dwTotalEvents = dwTotalEvents + pLedNumEventId[i];
    }

    if(0 == dwTotalEvents)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("No LED has at least one event configured. Skipping the test."));

        dwReturnValue = TPR_SKIP;
        goto RETURN;
    }

    //Storing the event paths and unsetting all the events
    pEventDetails_list = new NLED_EVENT_DETAILS[dwTotalEvents];

    for(DWORD i=0;i<dwTotalEvents;i++)
    {
        ZeroMemory(&pEventDetails_list[i], sizeof(NLED_EVENT_DETAILS));
    }

    hEventOrigKey = new HKEY[dwTotalEvents];

    lRetVal = UnSetAllNledServiceEvents(pEventDetails_list,
                                        &hTemporaryEventKey,
                                        hEventOrigKey);

    if(!lRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to unset all the Nled Service Events"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Pointer to an Array to store the Group Id for the all the Leds
    pLedNumGroupId = new DWORD[GetLedCount()];

    //Getting the Group Ids for the Nleds
    lRetVal = GetNledGroupIds(pLedNumGroupId);

    if(TPR_PASS == lRetVal)
    {
        //Finding the Led that has more 'events' configured
        for(DWORD dwLedNum = 0; dwLedNum < GetLedCount(); dwLedNum++)
        {
            //Checking if the Nled is present in any Group
            if(pLedNumGroupId[dwLedNum] != 0)
            {
                //Check if this Nled has any events configured
                if(pLedNumEventId[dwLedNum] != 0)
                {
                    //Getting the Nled that has more Events configured
                    if(pLedNumEventId[dwLedNum] > dwFinalEventCount)
                    {
                        dwFinalEventCount = pLedNumEventId[dwLedNum];
                        dwLedWithMoreEvents = dwLedNum;
                    }
                }

                //Incrementing the count if the Nled is present in any group
                dwGroupNledCount++;
            }
        }
    }
    else
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to get the Nled Group Ids for all the Nleds"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    if(0 == dwGroupNledCount)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("None of the Nleds on the device is present in a Group. Skipping the test"));

        dwReturnValue = TPR_SKIP;
        goto RETURN;
    }
    else if(0 == dwFinalEventCount)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("None of the Nleds present in a group has atleast one event configured. Skipping the test"));

        dwReturnValue = TPR_SKIP;
        goto RETURN;

    }
    else if(1 == dwFinalEventCount)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("None of the Nleds present in a group has more than one event configured. Skipping the test"));

        dwReturnValue = TPR_SKIP;
        goto RETURN;
    }

    //Memory allocations
    hEventKey = new HKEY[dwFinalEventCount];
    hTempoEventKey = new HKEY[dwFinalEventCount];
    pNledEventDetails = new NLED_EVENT_DETAILS[dwFinalEventCount];
    pNledSettingsInfo  = new NLED_SETTINGS_INFO[dwFinalEventCount];

    for(DWORD i=0;i<dwFinalEventCount;i++)
    {
        ZeroMemory(&pNledEventDetails[i], sizeof(NLED_EVENT_DETAILS));
        ZeroMemory(&pNledSettingsInfo[i], sizeof(NLED_SETTINGS_INFO));
    }

    lptStrTempoEventPath = new LPTSTR[dwFinalEventCount];

    //Creating different tempoEvent paths for different events
    for(DWORD dwLedNum = 0; dwLedNum < dwFinalEventCount; dwLedNum++)
    {
        lptStrTempoEventPath[dwLedNum] = (TCHAR *)calloc(REG_EVENT_PATH_LENGTH, sizeof(TCHAR));
        wcscpy_s(lptStrTempoEventPath[dwLedNum], REG_EVENT_PATH_LENGTH, szTempoEvent);

        //Concatinating to get the new tempo event path
         _itow_s(dwLedNum, wszString, 10, 10);
         _tcscat_s(lptStrTempoEventPath[dwLedNum], REG_EVENT_PATH_LENGTH, wszString);
    }

    //Checking for the priority order for the selected NLED
    //Opening the Config Key
    lRetVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                           REG_NLED_KEY_CONFIG_PATH,
                           0,
                           0,
                           &hNledConfigKey);

    if(lRetVal != ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to open the Config key for LEDs"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Enumerating for LEDs
    lRetVal = RegEnumKeyEx(hNledConfigKey,
                           dwCurLEDIndex,
                           szLedName,
                           &dwLedNameSize,
                           NULL,
                           NULL,
                           NULL,
                           NULL);

    if(ERROR_NO_MORE_ITEMS == lRetVal)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("No LEDs configured in the registry"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Iterating until All LEDS
    while(lRetVal !=  ERROR_NO_MORE_ITEMS)
    {
        //Open LED Registry
        lRetVal = RegOpenKeyEx(hNledConfigKey,
                               szLedName,
                               0,
                               KEY_ALL_ACCESS,
                               &hNledNameKey);

        if(lRetVal != ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to open the LED registry"));

            dwReturnValue = TPR_FAIL;
            goto RETURN;
        }
        else if(ERROR_SUCCESS == lRetVal)
        {
            //Querying the key values
            lRetVal = RegQueryValueEx(hNledNameKey,
                                      REG_LED_NUM_VALUE,
                                      NULL,
                                      NULL,
                                      (LPBYTE) &dwLedNumber,
                                      &dwLen);

            if(lRetVal != ERROR_SUCCESS)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to query the LedNum for %s"),
                             szLedName);

                dwReturnValue = TPR_FAIL;
                goto RETURN;
            }

            //Checking if this Nled is the selected Nled which has more number of events configured
            if(dwLedWithMoreEvents == dwLedNumber)
            {
                fEventTrigger = TRUE;

                //The selected nled has atleast one event configured so no need for checking against ERROR_NO_MORE_ITEMS
                //Enumerating the LED Events
                lRetVal = RegEnumKeyEx(hNledNameKey,
                                       dwSameLedEventCount,
                                       szEventName,
                                       &dwEventNameSize,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL);

                while(ERROR_SUCCESS == lRetVal)
                {
                    //Triggering event
                    //Opening the Event Key
                    lRetVal = RegOpenKeyEx(hNledNameKey,
                                           szEventName,
                                           0,
                                           KEY_ALL_ACCESS,
                                           &hEventKey[dwSameLedEventCount]);

                    if(lRetVal != ERROR_SUCCESS)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable to open the LED Event registry for %u LED"),
                                     dwLedNumber);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Backup Event Registry Values
                    lRetVal = ReadEventDetails(hNledNameKey,
                                               hEventKey[dwSameLedEventCount],
                                               &pNledEventDetails[dwSameLedEventCount]);

                    if (!lRetVal)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable to read %s Event-Details for %u NLED"),
                                     szEventName,
                                     dwLedNumber);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Set the state as ON
                    dwState = LED_ON;

                    //Set state to ON
                    lRetVal = RegSetValueEx(hEventKey[dwSameLedEventCount],
                                            REG_EVENT_STATE_VALUE,
                                            0,
                                            REG_DWORD,
                                            (PBYTE)&dwState,
                                            sizeof(DWORD));

                    if(lRetVal != ERROR_SUCCESS)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable To Set the state for %s event of %u Nled"),
                                     szEventName,
                                     dwLedNumber);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Set Same Priorities to all the Events for the selected Nled
                    dwPriority = 1;

                    lRetVal = RegSetValueEx (hEventKey[dwSameLedEventCount],
                                             REG_EVENT_PRIO_VALUE,
                                             0,
                                             REG_DWORD,
                                             (PBYTE)&dwPriority,
                                             sizeof(DWORD));

                    if(lRetVal != ERROR_SUCCESS)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable To Set Priority for %s event of %u Nled"),
                                     szEventName,
                                     dwLedNumber);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Set Temporary Trigger
                    lRetVal = Create_Set_TemporaryTrigger(hEventKey[dwSameLedEventCount],
                                                          lptStrTempoEventPath[dwSameLedEventCount],
                                                          &pNledEventDetails[dwSameLedEventCount],
                                                          &hTempoEventKey[dwSameLedEventCount]);

                    if (!lRetVal)
                    {
                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Unload and Load Service for affecting updated Registry values.
                    lRetVal = UnloadLoadService();

                    if(!lRetVal)
                    {
                        dwReturnValue = TPR_FAIL;
                        FAIL("Failed to UnLoad-Load the Nled Service");
                        goto RETURN;
                    }

                    //Incrementing the Event count
                    dwSameLedEventCount++;

                    //Re-assigning the EventName Size
                    dwEventNameSize = gc_dwMaxKeyNameLength + 1;

                    //Enumerating for more LED Events
                    lRetVal = RegEnumKeyEx(hNledNameKey,
                                           dwSameLedEventCount,
                                           szEventName,
                                           &dwEventNameSize,
                                           NULL,
                                           NULL,
                                           NULL,
                                           NULL);
                }

                //Triggering all the events
                for(DWORD dwCount = 0; dwCount < dwSameLedEventCount; dwCount++)
                {
                    //Raising Trigger
                    lRetVal = RaiseTemporaryTrigger(hTempoEventKey[dwCount],
                                                    &pNledEventDetails[dwCount]);

                    if(!lRetVal)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable to raise the Nled %u event from the test"),
                                     pNledEventDetails[dwCount].dwLedNum);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Wait for 1 second for service to act
                    Sleep(SLEEP_FOR_ONE_SECOND);

                    //Get Current LED Settings
                    lRetVal = GetCurrentNLEDDriverSettings(pNledEventDetails[dwCount],
                                                           &pNledSettingsInfo[dwCount]);

                    if(!lRetVal)
                    {
                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Check for the Nled state
                    if(LED_ON != pNledSettingsInfo[dwCount].OffOnBlink)
                    {
                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }
                }

                //All the events for this led are triggered.
                //Check if the Service is Still Running
                LPWSTR lpServiceName = L"nledsrv.dll";

                lRetVal = IsServiceRunning(lpServiceName);

                if(!lRetVal)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Nled Service failed to handle the error condition for all the events having same priority for NLED Number %u"),
                                 dwLedNumber);

                    dwReturnValue = TPR_FAIL;
                    goto RETURN;
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Nled Service successfully handled the error condition for all the events having same priority for NLED Number %u"),
                                 dwLedNumber);

                    dwReturnValue = TPR_PASS;
                    goto RETURN;
                }
            }
        }

        //Closing the Led Key
        RegCloseKey(hNledNameKey);

        //Incrementing the count
        dwCurLEDIndex++;
        dwLedNameSize = gc_dwMaxKeyNameLength + 1;

        //Enumerating for Next LED.
        lRetVal = RegEnumKeyEx(hNledConfigKey,
                               dwCurLEDIndex,
                               szLedName,
                               &dwLedNameSize,
                               NULL,
                               NULL,
                               NULL,
                               NULL);
    }

RETURN:
    if(fEventTrigger)
    {
        //Delete the Temporary Trigger.
        for(DWORD dwTempoCount = 0; dwTempoCount < dwSameLedEventCount; dwTempoCount++)
        {
            if (hTempoEventKey[dwTempoCount])
            {
                lRetVal = Unset_TemporaryTrigger(hEventKey[dwTempoCount],
                                                 hTempoEventKey[dwTempoCount],
                                                 &pNledEventDetails[dwTempoCount]);

                //Resetting Temporary Trigger.
                if(!lRetVal)
                {
                    dwReturnValue = TPR_FAIL;
                }

                lRetVal= DeleteTemporaryTrigger(hTempoEventKey[dwTempoCount],
                                                lptStrTempoEventPath[dwTempoCount],
                                                &pNledSettingsInfo[dwTempoCount],
                                                &pNledEventDetails[dwTempoCount]);

                if(!lRetVal)
                {
                    dwReturnValue = TPR_FAIL;
                }
            }

            if (hEventKey[dwTempoCount])
            {
                //Restore the previous state
                lRetVal = RegSetValueEx(hEventKey[dwTempoCount],
                                         REG_EVENT_STATE_VALUE,
                                         0,
                                         REG_DWORD,
                                         (PBYTE)&(pNledEventDetails[dwTempoCount].dwState),
                                         sizeof(DWORD));

                if(lRetVal != ERROR_SUCCESS)
                {
                    dwReturnValue = TPR_FAIL;
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to restore the previous State"));
                }

                //Restore the previous priority
                lRetVal = RegSetValueEx(hEventKey[dwTempoCount],
                                        REG_EVENT_PRIO_VALUE,
                                        0,
                                        REG_DWORD,
                                        (PBYTE)&(pNledEventDetails[dwTempoCount].dwPrio),
                                        sizeof(DWORD));

                if(lRetVal != ERROR_SUCCESS)
                {
                    dwReturnValue = TPR_FAIL;
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to restore the previous priority"));
                }

                RegCloseKey(hEventKey[dwTempoCount]);
            }
        }

        //Closing the key handled
        if(hNledNameKey != NULL)
        {
            RegCloseKey(hNledNameKey);
        }

        if(hNledConfigKey != NULL)
        {
            RegCloseKey(hNledConfigKey);
        }

        //Deallocating the memory
        if(hEventKey != NULL)
        {
            delete[] hEventKey;
            hEventKey = NULL;
        }

        if(hTempoEventKey != NULL)
        {
            delete[] hTempoEventKey;
            hTempoEventKey = NULL;
        }

        if(pNledEventDetails != NULL)
        {
            delete[] pNledEventDetails;
            pNledEventDetails = NULL;
        }

        if(pNledSettingsInfo != NULL)
        {
            delete[] pNledSettingsInfo;
            pNledSettingsInfo = NULL;
        }

        for(dwTempoCount = 0; dwTempoCount < dwSameLedEventCount; dwTempoCount++)
        {
            if(lptStrTempoEventPath[dwTempoCount] != NULL)
            {
                free(lptStrTempoEventPath[dwTempoCount]);
                lptStrTempoEventPath[dwTempoCount] = NULL;
            }
        }

        if(lptStrTempoEventPath != NULL)
        {
            delete[] lptStrTempoEventPath;
            lptStrTempoEventPath = NULL;
        }
    }

    //Restore back the original Event Paths
    lRetVal = RestoreAllNledServiceEventPaths(pEventDetails_list,
                                              hEventOrigKey,
                                              dwTotalEvents);

    if(!lRetVal)
    {
        dwReturnValue = TPR_FAIL;
        FAIL("Unable to restore back the original event paths");
    }

    //Delete the temporary registry key
    if(hTemporaryEventKey != NULL)
    {
        lRetVal =  RegCloseKey(hTemporaryEventKey);

        if(lRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Closing Failed. Last Error = %ld"),
                         GetLastError());

            dwReturnValue = TPR_FAIL;
        }

        lRetVal = RegDeleteKey(HKEY_LOCAL_MACHINE, TEMPORARY_EVENT_PATH);

        if(lRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Deletion Failed. Last Error = %ld"),
                         GetLastError());

            dwReturnValue = TPR_FAIL;
        }
    }

    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo != NULL)
    {
        lRetVal = setAllLedSettings(pOriginalLedSettingsInfo);

        if(!lRetVal)
        {
            FAIL("Failed to restore the previous LED settings");
            dwReturnValue = TPR_FAIL;
        }

        //De-allocating the Memory
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    if(pLedNumEventId != NULL)
    {
        delete[] pLedNumEventId;
        pLedNumEventId = NULL;
    }

    if(pLedNumGroupId != NULL)
    {
        delete[] pLedNumGroupId;
        pLedNumGroupId = NULL;
    }

    if(hEventOrigKey != NULL)
    {
        delete[] hEventOrigKey;
        hEventOrigKey = NULL;
    }

    if(pEventDetails_list != NULL)
    {
        delete[] pEventDetails_list;
        pEventDetails_list = NULL;
    }

    return dwReturnValue;
}

//*****************************************************************************
TESTPROCAPI NledServiceEventsWithOutPriorityTest(UINT                   uMsg,
                                                 TPPARAM                tpParam,
                                                 LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  To Check that Nled Service handles the error scenario when one or
//  more events of a Nled have no priority field.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Supporting Only Executing the Test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the service or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fServiceTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    //Returning pass if the device does not have any NLEDs
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("The device doesn't have any NLEDs so passing the test"));

        return TPR_PASS;
    }

    //Declaring the variables
    LONG lRetVal = 0;
    DWORD *pLedNumEventId = NULL;
    DWORD dwTotalEvents = 0;
    DWORD dwReturnValue = TPR_FAIL;
    NLED_EVENT_DETAILS *pEventDetails_list = NULL;
    HKEY *hEventOrigKey = NULL;
    HKEY hTemporaryEventKey = NULL;
    HKEY hNledConfigKey = NULL;
    DWORD dwCurLEDIndex = 0;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLedNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwFinalEventCount = 0;
    DWORD dwLedWithMoreEvents = 0;
    DWORD dwLen = sizeof(DWORD);;
    DWORD dwLedNumber = 0;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwState = 0;
    HKEY hNledNameKey = NULL;
    HKEY *hEventKey = NULL;
    HKEY *hTempoEventKey = NULL;
    NLED_EVENT_DETAILS *pNledEventDetails = NULL;
    LPTSTR *lptStrTempoEventPath = NULL;
    BOOL fEventTrigger = FALSE;
    DWORD dwGroupNledCount = 0;
    DWORD *pLedNumGroupId = NULL;
    TCHAR szTempoEvent[255] = TEMPO_EVENT_PATH;
    WCHAR wszString[10];
    DWORD dwSameLedEventCount = 0;

    //Storing the original LED settings
    NLED_SETTINGS_INFO *pOriginalLedSettingsInfo = NULL;

    // Allocates space to save origional LED setttings.
    SetLastError(ERROR_SUCCESS);
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];

    if(!pOriginalLedSettingsInfo)
    {
        FAIL("Memory allocation for NLED_SETTINGS_INFO failed");
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tLast Error = %d"),
                     GetLastError());

        return TPR_FAIL;
    }

    ZeroMemory(pOriginalLedSettingsInfo, GetLedCount() * sizeof(NLED_SETTINGS_INFO));

    // Save the Info Settings for all the LEDs and turn them off.
    lRetVal = saveAllLedSettings(pOriginalLedSettingsInfo, LED_OFF);

    if(!lRetVal)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    }

    //Pointer to an Array to store the Info if the Nled has any events configured
    pLedNumEventId = new DWORD[GetLedCount()];

    //Getting the Event Ids for all the Nleds
    lRetVal = GetNledEventIds(pLedNumEventId);

    if(TPR_PASS != lRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to retrieve total number of events for all the Leds."));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Calculating the total number of events of all the Leds
    for(DWORD i = 0; i < GetLedCount(); i++)
    {
        dwTotalEvents = dwTotalEvents + pLedNumEventId[i];
    }

    if(0 == dwTotalEvents)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("No LED has at least one event configured. Skipping the test."));

        dwReturnValue = TPR_SKIP;
        goto RETURN;
    }

    //Storing the event paths and unsetting all the events
    pEventDetails_list = new NLED_EVENT_DETAILS[dwTotalEvents];

    for(DWORD i=0;i<dwTotalEvents;i++)
    {
        ZeroMemory(&pEventDetails_list[i], sizeof(NLED_EVENT_DETAILS));
    }

    hEventOrigKey = new HKEY[dwTotalEvents];

    lRetVal = UnSetAllNledServiceEvents(pEventDetails_list,
                                         &hTemporaryEventKey,
                                        hEventOrigKey);

    if(!lRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to unset all the Nled Service Events"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Pointer to an Array to store the Group Id for the all the Leds
    pLedNumGroupId = new DWORD[GetLedCount()];

    //Getting the Group Ids for the Nleds
    lRetVal = GetNledGroupIds(pLedNumGroupId);

    if(TPR_PASS == lRetVal)
    {
        //Finding the Led that has more 'events' configured
        for(DWORD dwLedNum = 0; dwLedNum < GetLedCount(); dwLedNum++)
        {
            //Checking if the Nled is present in any Group
            if(pLedNumGroupId[dwLedNum] != 0)
            {
                //Check if this Nled has any events configured
                if(pLedNumEventId[dwLedNum] != 0)
                {
                    //Getting the Nled that has more Events configured
                    if(pLedNumEventId[dwLedNum] > dwFinalEventCount)
                    {
                        dwFinalEventCount = pLedNumEventId[dwLedNum];
                        dwLedWithMoreEvents = dwLedNum;
                    }
                }

                //Incrementing the count if the Nled is present in any group
                dwGroupNledCount++;
            }
        }
    }
    else
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to get the Nled Group Ids for all the Nleds"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    if(0 == dwGroupNledCount)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("None of the Nleds on the device is present in a Group. Skipping the test"));

        dwReturnValue = TPR_SKIP;
        goto RETURN;
    }
    else if(0 == dwFinalEventCount)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("None of the Nleds present in a group has atleast one event configured. Skipping the test"));

        dwReturnValue = TPR_SKIP;
        goto RETURN;

    }
    else if(1 == dwFinalEventCount)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("None of the Nleds present in a group has more than one event cofigured. Skipping the test"));

        dwReturnValue = TPR_SKIP;
        goto RETURN;
    }

    //Memory allocations
    hEventKey = new HKEY[dwFinalEventCount];
    hTempoEventKey = new HKEY[dwFinalEventCount];
    pNledEventDetails = new NLED_EVENT_DETAILS[dwFinalEventCount];

    for(DWORD i=0;i<dwFinalEventCount;i++)
    {
        ZeroMemory(&pNledEventDetails[i], sizeof(NLED_EVENT_DETAILS));
    }

    lptStrTempoEventPath = new LPTSTR[dwFinalEventCount];

    //Creating different tempoEvent paths for different events
    for(DWORD dwLedNum = 0; dwLedNum < dwFinalEventCount; dwLedNum++)
    {
        lptStrTempoEventPath[dwLedNum] = (TCHAR *)calloc(REG_EVENT_PATH_LENGTH, sizeof(TCHAR));
        wcscpy_s(lptStrTempoEventPath[dwLedNum], REG_EVENT_PATH_LENGTH, szTempoEvent);

        //Concatinating to get the new tempo event path
        _itow_s(dwLedNum, wszString, 10, 10);
        _tcscat_s(lptStrTempoEventPath[dwLedNum], REG_EVENT_PATH_LENGTH, wszString);
    }

    //Checking for the priority order for the selected NLED
    //Opening the Config Key
    lRetVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                           REG_NLED_KEY_CONFIG_PATH,
                           0,
                           0,
                           &hNledConfigKey);

    if(lRetVal != ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to open the Config key for LEDs"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Enumerating for LEDs
    lRetVal = RegEnumKeyEx(hNledConfigKey,
                           dwCurLEDIndex,
                           szLedName,
                           &dwLedNameSize,
                           NULL,
                           NULL,
                           NULL,
                           NULL);

    if(ERROR_NO_MORE_ITEMS == lRetVal)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("No LEDs configured in the registry"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Iterating until All LEDS
    while(lRetVal !=  ERROR_NO_MORE_ITEMS)
    {
        //Open LED Registry
        lRetVal = RegOpenKeyEx(hNledConfigKey,
                               szLedName,
                               0,
                               KEY_ALL_ACCESS,
                               &hNledNameKey);

        if(lRetVal != ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to open the LED registry"));

            dwReturnValue = TPR_FAIL;
            goto RETURN;
        }
        else if(ERROR_SUCCESS == lRetVal)
        {
            //Querying the key values
            lRetVal = RegQueryValueEx(hNledNameKey,
                                      REG_LED_NUM_VALUE,
                                      NULL,
                                      NULL,
                                      (LPBYTE) &dwLedNumber,
                                      &dwLen);

            if(lRetVal != ERROR_SUCCESS)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to query the LedNum for %s"),
                             szLedName);

                dwReturnValue = TPR_FAIL;
                goto RETURN;
            }

            //Checking if this Nled is the selected Nled which has more number of events configured
            if(dwLedWithMoreEvents == dwLedNumber)
            {
                fEventTrigger = TRUE;

                //Enumerating the LED Events
                lRetVal = RegEnumKeyEx(hNledNameKey,
                                       dwSameLedEventCount,
                                       szEventName,
                                       &dwEventNameSize,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL);

                //The selected Nled has atleast one event configured so no need of checking against ERROR_NO_MORE_ITEMS
                while(ERROR_SUCCESS == lRetVal)
                {
                    //Triggering event
                    //Opening the Event Key
                    lRetVal = RegOpenKeyEx(hNledNameKey,
                                           szEventName,
                                           0,
                                           KEY_ALL_ACCESS,
                                           &hEventKey[dwSameLedEventCount]);

                    if(lRetVal != ERROR_SUCCESS)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable to open the LED Event registry for %u LED"),
                                     dwLedNumber);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Backup Event Registry Values
                    lRetVal = ReadEventDetails(hNledNameKey,
                                               hEventKey[dwSameLedEventCount],
                                               &pNledEventDetails[dwSameLedEventCount]);

                    if (!lRetVal)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable to read %s Event-Details for %u NLED"),
                                     szEventName,
                                     dwLedNumber);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Set the state as ON
                    dwState = LED_ON;

                    //Set state to ON
                    lRetVal = RegSetValueEx(hEventKey[dwSameLedEventCount],
                                            REG_EVENT_STATE_VALUE,
                                            0,
                                            REG_DWORD,
                                            (PBYTE)&dwState,
                                            sizeof(DWORD));

                    if(lRetVal != ERROR_SUCCESS)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable To Set the state for %s event of %u Nled"),
                                     szEventName,
                                     dwLedNumber);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Delete Priority for all the Events
                    lRetVal = RegDeleteValue(hEventKey[dwSameLedEventCount],
                                             REG_EVENT_PRIO_VALUE);

                    if (lRetVal != ERROR_SUCCESS)
                    {
                        //Log Error Message
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable to delete priority field for %s Event of %u Nled"),
                                     szEventName,
                                     dwLedNumber);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Set Temporary Trigger
                    lRetVal = Create_Set_TemporaryTrigger(hEventKey[dwSameLedEventCount],
                                                          lptStrTempoEventPath[dwSameLedEventCount],
                                                          &pNledEventDetails[dwSameLedEventCount],
                                                          &hTempoEventKey[dwSameLedEventCount]);

                    if (!lRetVal)
                    {
                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Unload and Load Service for affecting updated Registry values.
                    lRetVal = UnloadLoadService();

                    if(!lRetVal)
                    {
                        dwReturnValue = TPR_FAIL;
                        FAIL("Failed to UnLoad-Load the Nled Service");
                        goto RETURN;
                    }

                    //Incrementing the Event count
                    dwSameLedEventCount++;

                    //Re-assigning the EventName Size
                    dwEventNameSize = gc_dwMaxKeyNameLength + 1;

                    //Enumerating for more LED Events
                    lRetVal = RegEnumKeyEx(hNledNameKey,
                                           dwSameLedEventCount,
                                           szEventName,
                                           &dwEventNameSize,
                                           NULL,
                                           NULL,
                                           NULL,
                                           NULL);
                }

                //Triggering all the events
                for(DWORD dwCount = 0; dwCount < dwSameLedEventCount; dwCount++)
                {
                    //Raising Trigger
                    lRetVal = RaiseTemporaryTrigger(hTempoEventKey[dwCount],
                                                    &pNledEventDetails[dwCount]);

                    if(!lRetVal)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable to raise Nled %u event from the test"),
                                     pNledEventDetails[dwCount].dwLedNum);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }

                    //Wait for 1 Second for service to act
                    Sleep(SLEEP_FOR_ONE_SECOND);
                }

                //All the events for this led are triggered.

                //Check if the Service is Still Running
                LPWSTR lpServiceName = L"nledsrv.dll";
                lRetVal= IsServiceRunning(lpServiceName);

                if(!lRetVal)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Nled Service failed to handle the error condition for all the events having no priority for NLED Number %u"),
                                 dwLedNumber);

                    dwReturnValue = TPR_FAIL;
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Nled Service successfully handled the error condition for all the events having no priority for NLED Number %u"),
                                 dwLedNumber);

                    dwReturnValue = TPR_PASS;
                }
                goto RETURN;
            }
        }

        //Closing the Led Key handle
        RegCloseKey(hNledNameKey);

        //Incrementing the count
        dwCurLEDIndex++;
        dwLedNameSize = gc_dwMaxKeyNameLength + 1;

        //Enumerating for Next LED.
        lRetVal = RegEnumKeyEx(hNledConfigKey,
                               dwCurLEDIndex,
                               szLedName,
                               &dwLedNameSize,
                               NULL,
                               NULL,
                               NULL,
                               NULL);
    }

RETURN:
    //Delete the Temporary Trigger.
    if(fEventTrigger)
    {
        for(DWORD dwTempoCount = 0; dwTempoCount < dwSameLedEventCount; dwTempoCount++)
        {
            if (hTempoEventKey[dwTempoCount])
            {
                //Resetting Temporary Trigger.
                lRetVal = Unset_TemporaryTrigger(hEventKey[dwTempoCount],
                                                 hTempoEventKey[dwTempoCount],
                                                 &pNledEventDetails[dwTempoCount]);

                if(!lRetVal)
                {
                    dwReturnValue = TPR_FAIL;
                }

                lRetVal = DeleteTemporaryTrigger(hTempoEventKey[dwTempoCount],
                                                 lptStrTempoEventPath[dwTempoCount],
                                                 NULL,
                                                 &pNledEventDetails[dwTempoCount]);

                if(!lRetVal)
                {
                    dwReturnValue = TPR_FAIL;
                }
            }

            if (hEventKey[dwTempoCount])
            {
                //Restore the previous state
                lRetVal = RegSetValueEx(hEventKey[dwTempoCount],
                                        REG_EVENT_STATE_VALUE,
                                        0,
                                        REG_DWORD,
                                        (PBYTE)&(pNledEventDetails[dwTempoCount].dwState),
                                        sizeof(DWORD));

                if(lRetVal != ERROR_SUCCESS)
                {
                    dwReturnValue = TPR_FAIL;
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to restore the previous State"));
                }

                //Restore the previous priority
                lRetVal = RegSetValueEx(hEventKey[dwTempoCount],
                                        REG_EVENT_PRIO_VALUE,
                                        0,
                                        REG_DWORD,
                                        (PBYTE)&(pNledEventDetails[dwTempoCount].dwPrio),
                                        sizeof(DWORD));

                if(lRetVal != ERROR_SUCCESS)
                {
                    dwReturnValue = TPR_FAIL;
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to restore the previous priority"));
                }

                RegCloseKey(hEventKey[dwTempoCount]);
            }
        }

        if(hNledNameKey != NULL)
        {
            RegCloseKey(hNledNameKey);
        }

        if(hNledConfigKey != NULL)
        {
            RegCloseKey(hNledConfigKey);
        }

        //Deallocating the memory
        if(hEventKey != NULL)
        {
            delete[] hEventKey;
            hEventKey = NULL;
        }

        if(hTempoEventKey != NULL)
        {
            delete[] hTempoEventKey;
            hTempoEventKey = NULL;
        }

        if(pNledEventDetails != NULL)
        {
            delete[] pNledEventDetails;
            pNledEventDetails = NULL;
        }

        for(dwTempoCount = 0; dwTempoCount < dwSameLedEventCount; dwTempoCount++)
        {
            if(lptStrTempoEventPath[dwTempoCount] != NULL)
            {
                free(lptStrTempoEventPath[dwTempoCount]);
                lptStrTempoEventPath[dwTempoCount] = NULL;
            }
        }

        if(lptStrTempoEventPath != NULL)
        {
            delete[] lptStrTempoEventPath;
            lptStrTempoEventPath = NULL;
        }
    }

    //Restore back the original Event Paths
    lRetVal = RestoreAllNledServiceEventPaths(pEventDetails_list,
                                              hEventOrigKey,
                                              dwTotalEvents);

    if(!lRetVal)
    {
        dwReturnValue = TPR_FAIL;
        FAIL("Unable to restore back the original event paths");
    }

    //Delete the temporary registry key
    if(hTemporaryEventKey != NULL)
    {
        lRetVal =  RegCloseKey(hTemporaryEventKey);

        if(lRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Closing Failed. Last Error = %ld"),
                         GetLastError());

            dwReturnValue = TPR_FAIL;
        }

        lRetVal = RegDeleteKey(HKEY_LOCAL_MACHINE, TEMPORARY_EVENT_PATH);

        if(lRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Deletion Failed. Last Error = %ld"),
                         GetLastError());

            dwReturnValue = TPR_FAIL;
        }
    }

    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo != NULL)
    {
        lRetVal = setAllLedSettings(pOriginalLedSettingsInfo);

        if(!lRetVal)
        {
            FAIL("Failed to restore the previous LED settings");
            dwReturnValue = TPR_FAIL;
        }

        //De-allocating the Memory
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    if(pLedNumEventId != NULL)
    {
        delete[] pLedNumEventId;
        pLedNumEventId = NULL;
    }

    if(pLedNumGroupId != NULL)
    {
        delete[] pLedNumGroupId;
        pLedNumGroupId = NULL;
    }

    if(hEventOrigKey != NULL)
    {
        delete[] hEventOrigKey;
        hEventOrigKey = NULL;
    }

    if(pEventDetails_list != NULL)
    {
        delete[] pEventDetails_list;
        pEventDetails_list = NULL;
    }

    return dwReturnValue;
}

//*****************************************************************************
DWORD GetNledEventIds(DWORD *pLedNumEventId)
//
//  Parameters: Pointer to an Array to store the number of events for each Nled
//
//  Return Value: TPR_PASS, TPR_FAIL
//
//  Function to return info about the number of events that are
//  configured for each NLED
//*****************************************************************************
{
    //Declaring the variables
    HKEY hConfigKey = NULL;
    HKEY hLedNameKey = NULL;
    DWORD dwReturnValue = TPR_FAIL;
    DWORD dwLedNum = 0;
    DWORD dwLedCount = 0;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLedNameSize=gc_dwMaxKeyNameLength + 1;
    DWORD dwLen = sizeof(DWORD);
    DWORD dwEventCount = 0;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwEventNameSize=gc_dwMaxKeyNameLength + 1;

    //Opening the key
    dwReturnValue = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                 REG_NLED_KEY_CONFIG_PATH,
                                 0,
                                 KEY_ENUMERATE_SUB_KEYS,
                                 &hConfigKey);

    if(ERROR_SUCCESS == dwReturnValue)
    {
        dwReturnValue = RegEnumKeyEx(hConfigKey,
                                     dwLedCount,
                                     szLedName,
                                     &dwLedNameSize,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL);

        while(ERROR_SUCCESS == dwReturnValue)
        {
            //Opening the LedName key
            dwReturnValue = RegOpenKeyEx(hConfigKey,
                                         szLedName,
                                         0,
                                         KEY_ALL_ACCESS,
                                         &hLedNameKey);

            if(ERROR_SUCCESS == dwReturnValue)
            {
                //Checking for LedNum
                dwReturnValue = RegQueryValueEx(hLedNameKey,
                                                REG_LED_NUM_VALUE,
                                                NULL,
                                                NULL,
                                                (LPBYTE) &dwLedNum,
                                                &dwLen);

                if(ERROR_SUCCESS != dwReturnValue)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to query the LedNum for %s"),
                                 szLedName);

                    dwReturnValue = TPR_FAIL;
                    goto CLEANUP;
                }

                //Clearing the dwEventCount for the next iterations
                dwEventCount = 0;

                dwReturnValue = RegEnumKeyEx(hLedNameKey,
                                             dwEventCount,
                                             szEventName,
                                             &dwEventNameSize,
                                             NULL,
                                             NULL,
                                             NULL,
                                             NULL);

                //Checking if this LED has any event configured
                if(ERROR_NO_MORE_ITEMS == dwReturnValue)
                {
                    //This LED does not have any event configured
                    if(dwLedNum < GetLedCount())
                    {
                        pLedNumEventId[dwLedNum] = 0;
                    }
                    else
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("The NLED index %u is not correct. Please verify the result of VerifyNledIndex test for more details"),
                                     dwLedNum);

                        dwReturnValue = TPR_FAIL;
                        goto CLEANUP;
                    }
                 }

                while(ERROR_SUCCESS == dwReturnValue)
                {
                    //Incrementing the count
                    dwEventCount++;
                    dwEventNameSize=gc_dwMaxKeyNameLength + 1;

                    //Enumerating for all the available event keys
                    dwReturnValue = RegEnumKeyEx(hLedNameKey,
                                                 dwEventCount,
                                                 szEventName,
                                                 &dwEventNameSize,
                                                 NULL,
                                                 NULL,
                                                 NULL,
                                                 NULL);
                }

                //Storing the number of events 'dwLedNum' has in pLedNumEventId
                if(dwEventCount != 0)
                {
                    if(dwLedNum < GetLedCount())
                    {
                        pLedNumEventId[dwLedNum] = dwEventCount;
                    }
                    else
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("The NLED index %u is not correct. Please verify the result of VerifyNledIndex test for more details"),
                                     dwLedNum);

                        dwReturnValue = TPR_FAIL;
                        goto CLEANUP;
                    }
                }
            }
            else
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Failed to open the %s key"),
                             szLedName);

                dwReturnValue = TPR_FAIL;
                break;
            }

            //Closing the Event key Handle
            RegCloseKey(hLedNameKey);

            //Incrementing the count
            dwLedCount++;

            //Resetting the buffer size
            dwLedNameSize=gc_dwMaxKeyNameLength + 1;

            //Enumerating for other keys
            dwReturnValue = RegEnumKeyEx(hConfigKey,
                                         dwLedCount,
                                         szLedName,
                                         &dwLedNameSize,
                                         NULL,
                                         NULL,
                                         NULL,
                                         NULL);
        }

        //Successfully got the Events for all the Nleds
        dwReturnValue = TPR_PASS;
    }
    else
    {
        FAIL("Failed to open Nled Config key");
        dwReturnValue = TPR_FAIL;
    }

CLEANUP:
        //Closing the handles
        if(hLedNameKey != NULL)
        {
            RegCloseKey(hLedNameKey);
        }

        if(hConfigKey != NULL)
        {
            RegCloseKey(hConfigKey);
        }

        //Returning the status
        if(TPR_PASS == dwReturnValue)
        {
            return TPR_PASS;
        }
        else
          return TPR_FAIL;
}

//*****************************************************************************
TESTPROCAPI NledServiceGroupingTest(UINT                   uMsg,
                                    TPPARAM                tpParam,
                                    LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Test to verify that only one NLED event(i.e. The Highest Priority one)
//  is in ON state at any given time in a Group.
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Supporting Only Executing the Test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the service or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fServiceTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    //Returning pass if the device does not have any NLEDs
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("The device doesn't have any NLEDs so passing the test"));

        return TPR_PASS;
    }

    //Declaring the variables
    LONG lRetVal = 0;
    DWORD *pLedNumEventId = NULL;
    DWORD dwTotalEvents = 0;
    DWORD dwReturnValue = TPR_FAIL;
    NLED_SETTINGS_INFO ledSettingsInfo_Get = {0};
    NLED_EVENT_DETAILS *pEventDetails_list = NULL;
    HKEY *hEventOrigKey = NULL;
    HKEY hTemporaryEventKey = NULL;
    HKEY hNledConfigKey = NULL;
    DWORD dwCurLEDIndex = 0;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLedNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwFinalGroupId = 0;
    DWORD dwFinalGroupEventCount = 0;
    DWORD dwTempGroupId = 0;
    DWORD dwTempGroupEventCount = 0;
    DWORD dwLen = sizeof(DWORD);;
    DWORD dwLedNumber = 0;
    DWORD dwLedGroup = 0;
    DWORD dwOnLedCount = 0;
    DWORD dwHighestPriority = 0;
    DWORD dwOnLedIndex = 0;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwState = 0;
    BOOL fInitializeOnce = TRUE;
    DWORD dwSameGroupEventTriggerCount = 0;
    HKEY hNledNameKey = NULL;
    HKEY *hEventKey = NULL;
    HKEY *hTempoEventKey = NULL;
    NLED_EVENT_DETAILS *pNledEventDetails = NULL;
    NLED_SETTINGS_INFO *pNledSettingsInfo  = NULL;
    LPTSTR *lptStrTempoEventPath = NULL;
    BOOL fEventTriggered = FALSE;
    DWORD *pLedNumGroupId = NULL;
    TCHAR szTempoEventString[255] = TEMPO_EVENT_PATH;
    WCHAR wszString[10];

    //Storing the original LED settings
    NLED_SETTINGS_INFO *pOriginalLedSettingsInfo = NULL;

    // Allocates space to save origional LED setttings.
    SetLastError(ERROR_SUCCESS);
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];

    if(!pOriginalLedSettingsInfo)
    {
        FAIL("Memory allocation for NLED_SETTINGS_INFO failed");
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tLast Error = %d"),
                     GetLastError());

        return TPR_FAIL;
    }

    ZeroMemory(pOriginalLedSettingsInfo, GetLedCount() * sizeof(NLED_SETTINGS_INFO));

    // Save the Info Settings for all the LEDs and turn them off.
    lRetVal = saveAllLedSettings(pOriginalLedSettingsInfo, LED_OFF);

    if(!lRetVal)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    }

    //Pointer to an Array to store the Info if the Nled has any events configured
    pLedNumEventId = new DWORD[GetLedCount()];

    //Getting the Event Ids for all the Nleds
    lRetVal = GetNledEventIds(pLedNumEventId);

    if(TPR_PASS != lRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to retrieve total number of events for all the Leds."));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Calculating the total number of events of all the Leds
    for(DWORD i = 0; i < GetLedCount(); i++)
    {
        dwTotalEvents = dwTotalEvents + pLedNumEventId[i];
    }

    if(0 == dwTotalEvents)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("No LED has at least one event configured.  Skipping the test."));

        dwReturnValue = TPR_SKIP;
        goto RETURN;
    }

    //Storing the event paths and unsetting all the events
    pEventDetails_list = new NLED_EVENT_DETAILS[dwTotalEvents];

    for(DWORD i=0;i<dwTotalEvents;i++)
    {
        ZeroMemory(&pEventDetails_list[i], sizeof(NLED_EVENT_DETAILS));
    }

    hEventOrigKey = new HKEY[dwTotalEvents];

    lRetVal = UnSetAllNledServiceEvents(pEventDetails_list,
                                         &hTemporaryEventKey,
                                        hEventOrigKey);

    if(!lRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to unset all the Nled Service Events"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Pointer to an Array to store the Group Id for the all the Leds
    pLedNumGroupId = new DWORD[GetLedCount()];

    //Getting the Group Ids for the Nleds
    lRetVal = GetNledGroupIds(pLedNumGroupId);

    if(TPR_PASS == lRetVal)
    {
        //Finding the group which has atleast Two Leds and also whose Leds atleast one event configured
        for(DWORD dwLedNum = 0; dwLedNum < GetLedCount(); dwLedNum++)
        {
            dwTempGroupEventCount = 0;

           if(pLedNumGroupId[dwLedNum] != 0)
           {
               //Check if this Nled has any events configured
               if(pLedNumEventId[dwLedNum] != 0)
               {
                   dwTempGroupId = pLedNumGroupId[dwLedNum];

                  //Finding the occurances of this groupId in the rest of the list
                  for(DWORD dwLedNumCount = 0; dwLedNumCount < GetLedCount(); dwLedNumCount++)
                  {
                      //Also Checking if the selected Nled also has atleast one event configured
                      if((dwTempGroupId == pLedNumGroupId[dwLedNumCount]) &&
                          (pLedNumEventId[dwLedNumCount] != 0))
                      {
                          dwTempGroupEventCount++;
                      }
                  }

                  //Selecting the Group which has more LEDs
                  if(dwTempGroupEventCount > dwFinalGroupEventCount)
                  {
                     dwFinalGroupEventCount = dwTempGroupEventCount;
                     dwFinalGroupId = dwTempGroupId;
                  }
               }
           }
        }
    }
    else
    {
        //Logging the output using the kato logging engine
         g_pKato->Log(LOG_DETAIL,
                      TEXT("Unable to get Group Ids for all Nleds"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    if(0 == dwFinalGroupEventCount)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("No Nled is present as part of any Group and also has atleast one event configured. Skipping the test"));

        dwReturnValue = TPR_SKIP;
        goto RETURN;
    }
    else if(1 == dwFinalGroupEventCount)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("No two Nleds are present as part of same Group have atleast one event cofigured. Skipping the test"));

        dwReturnValue = TPR_SKIP;
        goto RETURN;
    }

    //Memory Allocations
    hEventKey = new HKEY[dwFinalGroupEventCount];
    hTempoEventKey = new HKEY[dwFinalGroupEventCount];
    pNledEventDetails = new NLED_EVENT_DETAILS[dwFinalGroupEventCount];
    pNledSettingsInfo  = new NLED_SETTINGS_INFO[dwFinalGroupEventCount];

    for(DWORD i=0;i<dwFinalGroupEventCount;i++)
    {
        ZeroMemory(&pNledEventDetails[i], sizeof(NLED_EVENT_DETAILS));
        ZeroMemory(&pNledSettingsInfo[i], sizeof(NLED_SETTINGS_INFO));
    }

    lptStrTempoEventPath = new LPTSTR[dwFinalGroupEventCount];

    //Checking if any LED is present in any group
    if(dwFinalGroupId > 0)
    {
        //Creating different tempoEvent paths for different events
        for(DWORD i = 0; i < dwFinalGroupEventCount; i++)
        {
            lptStrTempoEventPath[i] = (TCHAR *)calloc(REG_EVENT_PATH_LENGTH, sizeof(TCHAR));
            wcscpy_s(lptStrTempoEventPath[i], REG_EVENT_PATH_LENGTH, szTempoEventString);

            //Concatinating to get the new tempo event path
             _itow_s(i, wszString, 10, 10);
            _tcscat_s(lptStrTempoEventPath[i], REG_EVENT_PATH_LENGTH, wszString);                
        }

        //Opening the Config Key
        lRetVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                               REG_NLED_KEY_CONFIG_PATH,
                               0,
                               0,
                               &hNledConfigKey);

        if(lRetVal != ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to open the Config key for LEDs"));

            dwReturnValue = TPR_FAIL;
            goto RETURN;
        }

        //Enumerating for LEDs
        lRetVal = RegEnumKeyEx(hNledConfigKey,
                               dwCurLEDIndex,
                               szLedName,
                               &dwLedNameSize,
                               NULL,
                               NULL,
                               NULL,
                               NULL);

        if(ERROR_NO_MORE_ITEMS == lRetVal)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("No LEDs configured in the registry"));

            dwReturnValue = TPR_FAIL;
            goto RETURN;
        }

        //Iterating until All LEDS
        while(lRetVal !=  ERROR_NO_MORE_ITEMS)
        {
            //Open LED Registry
            lRetVal = RegOpenKeyEx(hNledConfigKey,
                                   szLedName,
                                   0,
                                   KEY_ALL_ACCESS,
                                   &hNledNameKey);

            if(lRetVal != ERROR_SUCCESS)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to open the LED key for %s"),
                             szLedName);

                dwReturnValue = TPR_FAIL;
                goto RETURN;
            }
            else
            {
                //Querying the key values
                lRetVal = RegQueryValueEx(hNledNameKey,
                                          REG_LED_NUM_VALUE,
                                          NULL,
                                          NULL,
                                          (LPBYTE) &dwLedNumber,
                                          &dwLen);

                if(lRetVal != ERROR_SUCCESS)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to query the LedNum for %s"),
                                 szLedName);

                    dwReturnValue = TPR_FAIL;
                    goto RETURN;
                }

                //Querying the LedGroup Val
                lRetVal = RegQueryValueEx(hNledNameKey,
                                          REG_LED_GROUP_VALUE,
                                          NULL,
                                          NULL,
                                          (LPBYTE) &dwLedGroup,
                                          &dwLen);

                if(ERROR_SUCCESS == lRetVal)
                {
                    if((dwFinalGroupId == dwLedGroup) &&
                       (pLedNumEventId[dwLedNumber] != 0))
                    {
                        //Enumerating the LED Events
                        //Only triggering the first event in each Nled
                        lRetVal = RegEnumKeyEx(hNledNameKey,
                                               0,
                                               szEventName,
                                               &dwEventNameSize,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL);

                        //The selected Nled has atleast one event configured. so no need for checking against ERROR_NO_MORE_ITEMS
                        if(ERROR_SUCCESS == lRetVal)
                        {
                            fEventTriggered = TRUE;

                            //Triggering event
                            //Opening the Event Key
                            lRetVal = RegOpenKeyEx(hNledNameKey,
                                                   szEventName,
                                                   0,
                                                   KEY_ALL_ACCESS,
                                                   &hEventKey[dwSameGroupEventTriggerCount]);

                            if(lRetVal != ERROR_SUCCESS)
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("Unable to open the LED Event registry for %u LED"),
                                             dwLedNumber);

                                dwReturnValue = TPR_FAIL;
                                goto RETURN;
                            }

                            //Backup Event Registry Values
                            lRetVal = ReadEventDetails(hNledNameKey,
                                                       hEventKey[dwSameGroupEventTriggerCount],
                                                       &pNledEventDetails[dwSameGroupEventTriggerCount]);

                            if (!lRetVal)
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("Unable to read %s Event-Details for %u LED"),
                                             szEventName,
                                             dwLedNumber);

                                dwReturnValue = TPR_FAIL;
                                goto RETURN;
                            }

                            //Set the state as ON
                            dwState = LED_ON;
                            dwLen = sizeof(dwState);

                            lRetVal = RegSetValueEx (hEventKey[dwSameGroupEventTriggerCount],
                                                     REG_EVENT_STATE_VALUE,
                                                     0,
                                                     REG_DWORD,
                                                     (PBYTE)&dwState,
                                                     dwLen);

                            if(lRetVal != ERROR_SUCCESS)
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("Unable To Set the state for Nled %u, Event %s"),
                                             dwLedNumber,
                                             szEventName);

                                dwReturnValue = TPR_FAIL;
                                goto RETURN;
                            }

                            //Set Temporary Trigger
                            lRetVal = Create_Set_TemporaryTrigger(hEventKey[dwSameGroupEventTriggerCount],
                                                                  lptStrTempoEventPath[dwSameGroupEventTriggerCount],
                                                                  &pNledEventDetails[dwSameGroupEventTriggerCount],
                                                                  &hTempoEventKey[dwSameGroupEventTriggerCount]);

                            if (!lRetVal)
                            {
                                dwReturnValue = TPR_FAIL;
                                goto RETURN;
                            }

                            //Unload and Load Service for affecting updated Registry values.
                            lRetVal = UnloadLoadService();

                            if(!lRetVal)
                            {
                                dwReturnValue = TPR_FAIL;
                                FAIL("Failed to UnLoad-Load the Nled Service");
                                goto RETURN;
                            }

                            //Raising the Trigger
                            lRetVal = RaiseTemporaryTrigger(hTempoEventKey[dwSameGroupEventTriggerCount],
                                                            &pNledEventDetails[dwSameGroupEventTriggerCount]);

                            if(!lRetVal)
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("Unable to raise the %s event temporarly from the test"),
                                             szEventName);

                                dwReturnValue = TPR_FAIL;
                                goto RETURN;
                            }

                            //Wait for 1 second for the service to act
                            Sleep(SLEEP_FOR_ONE_SECOND);

                            //Get Current LED Settings
                            lRetVal = GetCurrentNLEDDriverSettings(pNledEventDetails[dwSameGroupEventTriggerCount],
                                                                   &pNledSettingsInfo[dwSameGroupEventTriggerCount]);

                            if(!lRetVal)
                            {
                                dwReturnValue = TPR_FAIL;
                                goto RETURN;
                            }

                            //Check for the Nled state for the first iteration
                            if(fInitializeOnce)
                            {
                                if(LED_ON != pNledSettingsInfo[dwSameGroupEventTriggerCount].OffOnBlink)
                                {
                                    g_pKato->Log(LOG_DETAIL,
                                                 TEXT("Nled Service failed to turn ON the %s - LED No. %u"),
                                                 szEventName,
                                                 dwLedNumber);

                                    dwReturnValue = TPR_FAIL;
                                    goto RETURN;
                                }

                                //Initializing the highest priority variable
                                dwHighestPriority = pNledEventDetails[dwSameGroupEventTriggerCount].dwPrio;

                                //Only for the first iteration
                                fInitializeOnce = FALSE;
                            }
                            else
                            {
                                //Check if the Nled is ON
                                if(dwHighestPriority > pNledEventDetails[dwSameGroupEventTriggerCount].dwPrio)
                                {
                                    if(LED_ON != pNledSettingsInfo[dwSameGroupEventTriggerCount].OffOnBlink)
                                    {
                                        g_pKato->Log(LOG_DETAIL,
                                                     TEXT("Nled Service failed to turn ON the %s - LED No. %u"),
                                                     szEventName,
                                                     dwLedNumber);

                                        dwReturnValue = TPR_FAIL;
                                        goto RETURN;
                                    }

                                    //Getting the highest priority in the Group
                                    dwHighestPriority = pNledEventDetails[dwSameGroupEventTriggerCount].dwPrio;
                                }
                            }

                            //Resetting the EventNameSize value
                            dwEventNameSize = gc_dwMaxKeyNameLength + 1;

                            //Incrementing the EventTrigger Count
                            dwSameGroupEventTriggerCount++;
                        }

                        else
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Error in enumerating registry for Nled details"));

                            dwReturnValue = TPR_FAIL;
                            goto RETURN;
                        }
                    }
                }
                else if(ERROR_SUCCESS != lRetVal)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("%s is not present in any LedGroup"),
                                 szLedName);
                }
            }

            //Closing the Led Key
            RegCloseKey(hNledNameKey);

            //Incrementing the count
            dwCurLEDIndex++;
            dwLedNameSize = gc_dwMaxKeyNameLength + 1;

            //Enumerating for Next LED.
            lRetVal = RegEnumKeyEx(hNledConfigKey,
                                   dwCurLEDIndex,
                                   szLedName,
                                   &dwLedNameSize,
                                   NULL,
                                   NULL,
                                   NULL,
                                   NULL);
        }

        //Checking that only one LED is in ON state in that group
        for(DWORD dwLedIndex = 0; dwLedIndex < GetLedCount(); dwLedIndex++)
        {
           if(dwFinalGroupId == pLedNumGroupId[dwLedIndex])
           {
              ledSettingsInfo_Get.LedNum = dwLedIndex;
              lRetVal = NLedGetDeviceInfo(NLED_SETTINGS_INFO_ID,
                                          &ledSettingsInfo_Get);

              if(!lRetVal)
              {
                  //Logging the output using the kato logging engine
                  g_pKato->Log(LOG_DETAIL,
                               TEXT("Unable to get the settings for Nled %u"),
                               dwLedIndex);

                  dwReturnValue = TPR_FAIL;
                  goto RETURN;
              }

              if(LED_ON == ledSettingsInfo_Get.OffOnBlink)
              {
                  dwOnLedCount++;
                  dwOnLedIndex = dwLedIndex;
              }
           }
        }

       //Checking that the highest priority event among the selected
       //events is active
       if(1 == dwOnLedCount)
       {
           for(DWORD i = 0; i < dwFinalGroupEventCount; i++)
           {
               if(dwOnLedIndex == pNledEventDetails[i].dwLedNum)
               {
                   if(pNledEventDetails[i].dwPrio == dwHighestPriority)
                   {
                       SUCCESS("Only the highest priority Nled is in active state in the selected group.");
                       SUCCESS("Nled Service successfully handles Nled Grouping.");
                       dwReturnValue = TPR_PASS;
                       goto RETURN;
                   }
               }
           }
       }
       else
       {
            FAIL("More than one Nled is in active state in the selected group");
            FAIL("Nled Service fails to handle Nled Grouping.");
            dwReturnValue = TPR_FAIL;
            goto RETURN;
       }
    }
    else
    {
        //No LED is present in any Group
    }

RETURN:
    //Delete the Temporary Trigger.
    if(fEventTriggered)
    {
        for(DWORD dwTempoCount = 0; dwTempoCount < dwFinalGroupEventCount; dwTempoCount++)
        {
            if (hTempoEventKey[dwTempoCount])
            {
                //Resetting Temporary Trigger.
                lRetVal = Unset_TemporaryTrigger(hEventKey[dwTempoCount],
                                                 hTempoEventKey[dwTempoCount],
                                                 &pNledEventDetails[dwTempoCount]);

                if(!lRetVal)
                {
                    dwReturnValue = TPR_FAIL;
                }

                lRetVal = DeleteTemporaryTrigger(hTempoEventKey[dwTempoCount],
                                                 lptStrTempoEventPath[dwTempoCount],
                                                 &pNledSettingsInfo[dwTempoCount],
                                                 &pNledEventDetails[dwTempoCount]);

                if(!lRetVal)
                {
                    dwReturnValue = TPR_FAIL;
                }
            }

            if (hEventKey[dwTempoCount])
            {

                //Restore state
                lRetVal = RegSetValueEx (hEventKey[dwTempoCount],
                                         REG_EVENT_STATE_VALUE,
                                         0,
                                         REG_DWORD,
                                         (PBYTE)&(pNledEventDetails[dwTempoCount].dwState),
                                         sizeof(DWORD));

                if(lRetVal != ERROR_SUCCESS)
                {
                    dwReturnValue = TPR_FAIL;
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable To Set the previous State"));
                }

                RegCloseKey(hEventKey[dwTempoCount]);
            }
        }

        //Closing the keys
        if(hNledNameKey != NULL)
        {
            RegCloseKey(hNledNameKey);
        }

        if(hNledConfigKey != NULL)
        {
            RegCloseKey(hNledConfigKey);
        }

        //Deallocating the memory
        if(hEventKey != NULL)
        {
            delete[] hEventKey;
            hEventKey = NULL;
        }

        if(hTempoEventKey != NULL)
        {
            delete[] hTempoEventKey;
            hTempoEventKey = NULL;
        }

        if(pNledEventDetails != NULL)
        {
            delete[] pNledEventDetails;
            pNledEventDetails = NULL;
        }

        if(pNledSettingsInfo != NULL)
        {
            delete[] pNledSettingsInfo;
            pNledSettingsInfo = NULL;
        }

        for(dwTempoCount = 0; dwTempoCount < dwFinalGroupEventCount; dwTempoCount++)
        {
            if(lptStrTempoEventPath[dwTempoCount] != NULL)
            {
                free(lptStrTempoEventPath[dwTempoCount]);
                lptStrTempoEventPath[dwTempoCount] = NULL;
            }
        }

        if(lptStrTempoEventPath != NULL)
        {
            delete[] lptStrTempoEventPath;
            lptStrTempoEventPath = NULL;
        }
    }

    //Restore back the original Event Paths
    lRetVal = RestoreAllNledServiceEventPaths(pEventDetails_list,
                                              hEventOrigKey,
                                              dwTotalEvents);

    if(!lRetVal)
    {
        dwReturnValue = TPR_FAIL;
        FAIL("Unable to restore back the original event paths");
    }

    //Delete the temporary registry key
    if(hTemporaryEventKey != NULL)
    {
        lRetVal =  RegCloseKey(hTemporaryEventKey);

        if(lRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Closing Failed. Last Error = %ld"),
                         GetLastError());

            dwReturnValue = TPR_FAIL;
        }

        lRetVal = RegDeleteKey(HKEY_LOCAL_MACHINE, TEMPORARY_EVENT_PATH);

        if(lRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Deletion Failed. Last Error = %ld"),
                         GetLastError());

            dwReturnValue = TPR_FAIL;
        }
    }

    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo != NULL)
    {
        lRetVal = setAllLedSettings(pOriginalLedSettingsInfo);

        if(!lRetVal)
        {
            FAIL("Failed to restore the previous LED settings");
            dwReturnValue = TPR_FAIL;
        }

        //De-Allocating the memory
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    if(pLedNumGroupId != NULL)
    {
        delete[] pLedNumGroupId;
        pLedNumGroupId = NULL;
    }

    if(pLedNumEventId != NULL)
    {
        delete[] pLedNumEventId;
        pLedNumEventId = NULL;
    }

    if(hEventOrigKey != NULL)
    {
        delete[] hEventOrigKey;
        hEventOrigKey = NULL;
    }

    if(pEventDetails_list != NULL)
    {
        delete[] pEventDetails_list;
        pEventDetails_list = NULL;
    }

    return dwReturnValue;
}

//*****************************************************************************
TESTPROCAPI NledServiceSuspendResumeTest(UINT                   uMsg,
                                         TPPARAM                tpParam,
                                         LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Test to check that Nled Service successfully re-stores the Nled State
//  after a device suspend resume.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Supporting Only Executing the Test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the service or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fServiceTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    //Returning pass if the device does not have any NLEDs
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("The device doesn't have any NLEDs so passing the test"));

        return TPR_PASS;
    }

    //Declaring the variables
    DWORD dwRetVal = 0;
    DWORD dwReturnValue = TPR_FAIL;
    WCHAR wszState[1024] = {0};
    LPWSTR pState = &wszState[0];
    DWORD dwBufChars = (sizeof(wszState) / sizeof(wszState[0]));
    DWORD dwStateFlags = 0;
    DWORD *pLedNumEventId = NULL;
    DWORD dwTotalEvents = 0;
    NLED_EVENT_DETAILS *pEventDetails_list = NULL;
    HKEY *hEventOrigKey = NULL;
    HKEY hTemporaryEventKey = NULL;
    HKEY hNLEDConfigKey = NULL;
    HKEY hLEDKey = NULL;
    HKEY hTempoEventKey = NULL;
    HKEY hEventKey = NULL;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLEDNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwCurLEDIndex = 0;
    NLED_EVENT_DETAILS pNledEventDetailsStruct = {0};
    NLED_SETTINGS_INFO *pLedSettingsInfo  = NULL;
    BOOL fStateSet = FALSE;
    BOOL fEventTrigger = FALSE;
    CSysPwrStates *g_pCSysPwr = NULL;
    TCHAR PowerStateChangeOn[] = L"On";
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwNewState = 0;

    //Storing the original LED settings
    NLED_SETTINGS_INFO *pOriginalLedSettingsInfo = NULL;

    // Allocates space to save origional LED setttings.
    SetLastError(ERROR_SUCCESS);
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];

    if(!pOriginalLedSettingsInfo)
    {
        FAIL("Memory allocation for NLED_SETTINGS_INFO failed");
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tLast Error = %d"),
                     GetLastError());

        return TPR_FAIL;
    }

    ZeroMemory(pOriginalLedSettingsInfo, GetLedCount() * sizeof(NLED_SETTINGS_INFO));

    // Save the Info Settings for all the LEDs and turn them off.
    dwRetVal = saveAllLedSettings(pOriginalLedSettingsInfo, LED_OFF);

    if(!dwRetVal)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    }

    //Check Whether Power Management service exists on the device
    dwRetVal = GetSystemPowerState(pState, dwBufChars, &dwStateFlags);

    if ((dwRetVal != ERROR_SUCCESS) || (dwRetVal == ERROR_SERVICE_DOES_NOT_EXIST))
    {
        //Prompting the error message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("SKIPPING TEST: Power Management does not exist on this device."));

        dwReturnValue = ERROR_NOT_SUPPORTED;
        goto RETURN;
    }

    //Pointer to an Array to store the Info if the Nled has any events configured
    pLedNumEventId = new DWORD[GetLedCount()];

    //Getting the Event Ids for all the Nleds
    dwRetVal = GetNledEventIds(pLedNumEventId);

    if(TPR_PASS != dwRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to retrieve total number of events for all the Leds."));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Calculating the total number of events of all the Leds
    for(DWORD i = 0; i < GetLedCount(); i++)
    {
        dwTotalEvents = dwTotalEvents + pLedNumEventId[i];
    }

    if(0 == dwTotalEvents)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("No LED has at least one event configured. Skipping the test."));

        dwReturnValue = TPR_SKIP;
        goto RETURN;
    }

    //Storing the event paths and unsetting all the events
    pEventDetails_list = new NLED_EVENT_DETAILS[dwTotalEvents];

    for(DWORD i=0;i<dwTotalEvents;i++)
    {
        ZeroMemory(&pEventDetails_list[i], sizeof(NLED_EVENT_DETAILS));
    }

    hEventOrigKey = new HKEY[dwTotalEvents];

    dwRetVal = UnSetAllNledServiceEvents(pEventDetails_list,
                                         &hTemporaryEventKey,
                                         hEventOrigKey);

    if(!dwRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to unset all the Nled Service Events"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Suspend Resume for Nled Service
    pLedSettingsInfo  = new NLED_SETTINGS_INFO[1];

    if(pLedSettingsInfo == NULL)
    {
        g_pKato->Log(LOG_FAIL, TEXT("Memory Allocation failed"));
    }

    ZeroMemory(pLedSettingsInfo, sizeof(NLED_SETTINGS_INFO));

    g_pCSysPwr = new CSysPwrStates();

    //Opening Config Key
    dwRetVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                            REG_NLED_KEY_CONFIG_PATH,
                            0,
                            0,
                            &hNLEDConfigKey);

    if(dwRetVal != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to open the Config directory for LEDs"));

        dwReturnValue = TPR_FAIL;
        goto RETURN ;
    }

    //Enumerating for NLEDs
    dwRetVal = RegEnumKeyEx(hNLEDConfigKey,
                            dwCurLEDIndex,
                            szLedName,
                            &dwLEDNameSize,
                            NULL,
                            NULL,
                            NULL,
                            NULL);

    if(ERROR_NO_MORE_ITEMS == dwRetVal)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("No LEDs configured in the registry."));

        dwReturnValue = TPR_FAIL;
        goto RETURN ;
    }

    //Iterating until All LEDS
    while(dwRetVal ==  ERROR_SUCCESS)
    {
        //Open LED Registry
        dwRetVal = RegOpenKeyEx(hNLEDConfigKey,
                                szLedName,
                                0,
                                KEY_ALL_ACCESS,
                                &hLEDKey);

        if(dwRetVal != ERROR_SUCCESS)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to open the LED registry"));

            dwReturnValue = TPR_FAIL;
            goto RETURN ;
        }
        else
        {
            //Enumerating LED Event
            dwRetVal = RegEnumKeyEx(hLEDKey,
                                    0,
                                    szEventName,
                                    &dwEventNameSize,
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);

            if(ERROR_NO_MORE_ITEMS == dwRetVal)
            {
                //Log Error Message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("No events configured in the registry for %s"),
                             szLedName);

                dwReturnValue = TPR_SKIP;
            }

            //Iteration for All Events in the LED.
            while(ERROR_SUCCESS == dwRetVal)
            {
                //Opening Event Key
                dwRetVal = RegOpenKeyEx(hLEDKey,
                                        szEventName,
                                        0,
                                        KEY_ALL_ACCESS,
                                        &hEventKey);

                if(dwRetVal != ERROR_SUCCESS)
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to open the Event key"));

                    dwReturnValue = TPR_FAIL;
                    goto RETURN ;
                }

                //Backup Event Registry Values.
                dwRetVal = ReadEventDetails(hLEDKey,
                                            hEventKey,
                                            &pNledEventDetailsStruct);

                if (!dwRetVal)
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to Read Event Details"));

                    dwReturnValue = TPR_FAIL;
                    goto RETURN;
                }

                //Set LED State to ON
                dwNewState = LED_ON;
                dwRetVal= RegSetValueEx(hEventKey,
                                        REG_EVENT_STATE_VALUE,
                                        0,
                                        REG_DWORD,
                                        (PBYTE) (&dwNewState),
                                        sizeof(dwNewState));

                if(dwRetVal != ERROR_SUCCESS)
                {
                    //Log Error Message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable To Set LED State Value to ON in the registry"));

                    dwReturnValue = TPR_FAIL;
                    goto RETURN;
                }
                else
                {
                    fStateSet = TRUE;
                }

                //Set Temporary Trigger
                dwRetVal = Create_Set_TemporaryTrigger(hEventKey,
                                                       TEMPO_EVENT_PATH,
                                                       &pNledEventDetailsStruct,
                                                       &hTempoEventKey);

                if (!dwRetVal)
                {
                    dwReturnValue = TPR_FAIL;
                    goto RETURN;
                }

                //Unload and Load Service for affecting updated Registry values.
                dwRetVal = UnloadLoadService();

                if(!dwRetVal)
                {
                    dwReturnValue = TPR_FAIL;
                    FAIL("Failed to UnLoad-Load the Nled Service");
                    goto RETURN;
                }
                else
                {
                    fEventTrigger = TRUE;
                }

                //Raising Trigger
                dwRetVal = RaiseTemporaryTrigger(hTempoEventKey,
                                                 &pNledEventDetailsStruct);

                if(!dwRetVal)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to raise the %s event temporarly from the test"),
                                 szEventName);

                    dwReturnValue = TPR_FAIL;
                    goto RETURN;
                }

                //Wait for 1 second fro service to act
                Sleep(SLEEP_FOR_ONE_SECOND);

                //Check if the Changes in Registry are affected.
                dwRetVal= GetCurrentNLEDDriverSettings(pNledEventDetailsStruct,
                                                       pLedSettingsInfo);

                if(!dwRetVal)
                {
                    dwReturnValue = TPR_FAIL;
                    goto RETURN;
                }

                //Check if LED is in Blinking State
                if (pLedSettingsInfo->OffOnBlink != LED_ON)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Service failed to turn ON the NLED %s"),
                                 szLedName);

                    dwReturnValue = TPR_FAIL;
                    goto RETURN;
                }

                g_pKato->Log(LOG_DETAIL, TEXT("Device is going to suspend now."));

                //Power suspends if power management is suppored on this device
                dwRetVal = g_pCSysPwr->SetupRTCWakeupTimer(TIME_BEFORE_RESUME);

                if(!dwRetVal)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("SKIPPING TEST: Power management is not supported on this device"));

                    dwReturnValue = ERROR_NOT_SUPPORTED;
                    goto RETURN;
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Device Suspend Is In Progress, It Will Resume In %d Seconds."), TIME_BEFORE_RESUME);
                }

                //Suspend the system if Power management is supported else display the error message
                dwRetVal = SetSystemPowerState(_T("Suspend"), NULL, POWER_FORCE);

                if(dwRetVal != ERROR_SUCCESS)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("ERROR: SetSystemPowerState failed."));

                    dwReturnValue = ERROR_GEN_FAILURE;
                    goto RETURN;
                }

                dwRetVal = PollForSystemPowerChange(PowerStateChangeOn, 0, TRANSITION_CHANGE_MAX);

                if(!dwRetVal)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("ERROR: PollForSystemPowerChange failed."));

                    dwReturnValue = ERROR_GEN_FAILURE;
                    goto RETURN;
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL, TEXT("Device power is resumed"));
                }

                //Check if LED is still in the Blinking State after suspend-resume
                dwRetVal= GetCurrentNLEDDriverSettings(pNledEventDetailsStruct,
                                                       pLedSettingsInfo);

                if(!dwRetVal)
                {
                    dwReturnValue = TPR_FAIL;
                    goto RETURN;
                }

                //Check if LED is in Blinking State
                if (pLedSettingsInfo->OffOnBlink != LED_ON)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Nled Service failed to resume the same Nled State after a Suspend Resume"));

                    dwReturnValue = TPR_FAIL;
                }
                else
                {
                    SUCCESS("Nled Service successfully restores the same Nled State after a Suspend Resume");
                    dwReturnValue = TPR_PASS;
                }

                //Executing the test only for one Nled
                goto RETURN;
            }
        }

        RegCloseKey(hLEDKey);

        //Next LED.
        dwCurLEDIndex++;
        dwLEDNameSize = gc_dwMaxKeyNameLength + 1;

        //Enumerating for Next LED.
        dwRetVal = RegEnumKeyEx(hNLEDConfigKey,
                                dwCurLEDIndex,
                                szLedName,
                                &dwLEDNameSize,
                                NULL,
                                NULL,
                                NULL,
                                NULL);
    }

RETURN:
    //Clearing the timer
    if(g_pCSysPwr != NULL)
    {
        g_pCSysPwr->CleanupRTCTimer();
    }

    //Restore the previous State value
    if(fStateSet)
    {
        dwRetVal = RegSetValueEx (hEventKey,
                                  REG_EVENT_STATE_VALUE,
                                  0,
                                  REG_DWORD,
                                  (PBYTE) (&pNledEventDetailsStruct.dwState),
                                  sizeof(pNledEventDetailsStruct.dwState));

        if(dwRetVal != ERROR_SUCCESS)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable To Restore previous State value"));

            dwReturnValue = TPR_FAIL;
        }
    }

    //Resetting Temporary Trigger.
    if(fEventTrigger)
    {
        dwRetVal = Unset_TemporaryTrigger(hEventKey,
                                          hTempoEventKey,
                                          &pNledEventDetailsStruct);

        if(!dwRetVal)
        {
            dwReturnValue = TPR_FAIL;
        }

        //Delete the Temporary Trigger.
        dwRetVal = DeleteTemporaryTrigger(hTempoEventKey,
                                          TEMPO_EVENT_PATH,
                                          pLedSettingsInfo,
                                          &pNledEventDetailsStruct);

        if(!dwRetVal)
        {
            dwReturnValue = TPR_FAIL;
        }
    }

    //Unload and Load Service for affecting updated Registry values
    if(fStateSet || fEventTrigger)
    {
        dwRetVal = UnloadLoadService();

        if(!dwRetVal)
        {
            dwReturnValue = TPR_FAIL;
            FAIL("Failed to UnLoad-Load the Nled Service");
        }
    }

    //Restore back the original Event Paths
    dwRetVal = RestoreAllNledServiceEventPaths(pEventDetails_list,
                                               hEventOrigKey,
                                               dwTotalEvents);

    if(!dwRetVal)
    {
        dwReturnValue = TPR_FAIL;
        FAIL("Unable to restore back the original event paths");
    }

    //Delete the temporary registry key
    if(hTemporaryEventKey != NULL)
    {
        dwRetVal =  RegCloseKey(hTemporaryEventKey);

        if(dwRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Closing Failed. Last Error = %ld"),
                         GetLastError());

            dwReturnValue = TPR_FAIL;
        }

        dwRetVal = RegDeleteKey(HKEY_LOCAL_MACHINE, TEMPORARY_EVENT_PATH);

        if(dwRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Deletion Failed. Last Error = %ld"),
                         GetLastError());

            dwReturnValue = TPR_FAIL;
        }
    }

    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo != NULL)
    {
        dwRetVal = setAllLedSettings(pOriginalLedSettingsInfo);

        if(!dwRetVal)
        {
            FAIL("Failed to restore the previous LED settings");
            dwReturnValue = TPR_FAIL;
        }

        //De-Allocating the memory
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    // Deallocating the Memory
    if(g_pCSysPwr != NULL)
    {
        delete g_pCSysPwr;
        g_pCSysPwr = NULL;
    }

    if(pLedSettingsInfo != NULL)
    {
        delete[] pLedSettingsInfo;
        pLedSettingsInfo = NULL;
    }

    //Close Config Key.
    if(hNLEDConfigKey != NULL)
    {
        RegCloseKey(hNLEDConfigKey);
    }

    if(hLEDKey != NULL)
    {
        RegCloseKey(hLEDKey);
    }

    if(hEventKey != NULL)
    {
        RegCloseKey(hEventKey);
    }

    if(pLedNumEventId != NULL)
    {
        delete[] pLedNumEventId;
        pLedNumEventId = NULL;
    }

    if(hEventOrigKey != NULL)
    {
        delete[] hEventOrigKey;
        hEventOrigKey = NULL;
    }

    if(pEventDetails_list != NULL)
    {
        delete[] pEventDetails_list;
        pEventDetails_list = NULL;
    }

    //Skipping the test if power mangement or wake-up timer is not supported
    if((dwReturnValue == ERROR_NOT_SUPPORTED) ||
       (dwReturnValue == ERROR_GEN_FAILURE))
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Skipping Test: Suspend Resume is not supported on the device"));

        dwReturnValue = TPR_SKIP;
    }
    else if(dwReturnValue == TPR_SKIP)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Skipping Test: None of the Nleds has atleast one event configured"));

        dwReturnValue = TPR_SKIP;
    }

    return dwReturnValue;
}

//*****************************************************************************
TESTPROCAPI NLEDServiceSimultaneousBlinkOffTest(UINT                   uMsg,
                                                TPPARAM                tpParam,
                                                LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: Standard TUX arguments
//
//  Return Value:   TPR_PASS On Success,
//                  TPR_FAIL On Failure
//                  TPR_NOT_HANDLED If the feature is not handled,
//                  TPR_SKIP If the test is skipped
//
// "To Set an LED to blink using a service and at the same time
//  turn the LED off.  The service must be stable."
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Only Supporting executing the test
    if(uMsg != TPM_EXECUTE)  return TPR_NOT_HANDLED;

    //Skipping the test if the service or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fServiceTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    //Returning pass if the device does not have any NLEDs
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("The device doesn't have any NLEDs so passing the test"));

        return TPR_PASS;
    }

    //Declaring the variables
    BOOL  fStatus = FALSE;
    DWORD dwRetVal = 0;
    DWORD dwTestResult = TPR_PASS;
    DWORD *pLedNumEventId = NULL;
    DWORD dwTotalEvents = 0;
    NLED_EVENT_DETAILS  nledEventDetailsStruct = {0};
    NLED_EVENT_DETAILS *pEventDetails_list = NULL;
    HKEY *hEventOrigKey = NULL;
    HKEY hTemporaryEventKey = NULL;
    HKEY hNLEDConfigKey = NULL;
    HKEY hLEDKey = NULL;
    HKEY hEventKey = NULL;
    HKEY hTempoEventKey = NULL;
    HANDLE hThread[1] = {NULL}; //Size is set to one, as only one thread is being created
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLEDNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwCurLEDIndex = 0;
    DWORD dwCurLEDEventIndex = 0;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwLedNum = 0;
    DWORD dwBlinkable = 0;
    DWORD dwSWBlinkCtrl = 0;
    DWORD dwState = LED_BLINK;
    NLED_SETTINGS_INFO *pLedSettingsInfo = NULL;
    NLED_SETTINGS_INFO *pServiceLedSettingsInfo = NULL;

    SetLastError(ERROR_SUCCESS);
    NLED_SETTINGS_INFO *pOriginalLedSettingsInfo = NULL;

    // Allocate space to save origional LED setttings.
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];

    if(!pOriginalLedSettingsInfo)
    {
        FAIL("Memory Allocation failed");
        g_pKato->Log(LOG_DETAIL,
                      TEXT("\tLast Error = %d"),
                      GetLastError());
        return TPR_FAIL;
    }

    ZeroMemory(pOriginalLedSettingsInfo, GetLedCount() * sizeof(NLED_SETTINGS_INFO));

    // Save the Info Settings for all the LEDs and turn them off.
    fStatus = saveAllLedSettings(pOriginalLedSettingsInfo, LED_OFF);

    if(!fStatus)
    {
        FAIL("Saving LED settings for all the Nleds failed");
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    }

    //Pointer to an Array to store the Info if the Nled has any events configured
    pLedNumEventId = new DWORD[GetLedCount()];

    //Getting the Event Ids for all the Nleds
    dwRetVal = GetNledEventIds(pLedNumEventId);

    if(TPR_PASS != dwRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to retrieve total number of events for all the Leds."));

        dwTestResult = TPR_FAIL;
        goto end;
    }

    //Calculating the total number of events of all the Leds
    for(DWORD i = 0; i < GetLedCount(); i++)
    {
        dwTotalEvents = dwTotalEvents + pLedNumEventId[i];
    }

    if(0 == dwTotalEvents)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("No LED has at least one event configured. Skipping the test."));

        dwTestResult = TPR_SKIP;
        goto end;
    }

    //Storing the event paths and unsetting all the events
    pEventDetails_list = new NLED_EVENT_DETAILS[dwTotalEvents];

    for(DWORD i=0;i<dwTotalEvents;i++)
    {
        ZeroMemory(&pEventDetails_list[i], sizeof(NLED_EVENT_DETAILS));
    }

    hEventOrigKey = new HKEY[dwTotalEvents];

    dwRetVal = UnSetAllNledServiceEvents(pEventDetails_list,
                                          &hTemporaryEventKey,
                                         hEventOrigKey);

    if(!dwRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to unset all the Nled Service Events"));

        dwTestResult = TPR_FAIL;
        goto end;
    }

    //Allocating Memory
    pLedSettingsInfo = new NLED_SETTINGS_INFO[1];
    pServiceLedSettingsInfo = new NLED_SETTINGS_INFO[1];

    if(!pLedSettingsInfo || !pServiceLedSettingsInfo)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tMemory allocation failed. Error = %ld"),
                     GetLastError());

        dwTestResult = TPR_FAIL;
        goto end;
    }

    ZeroMemory(pLedSettingsInfo, sizeof(NLED_SETTINGS_INFO));
    ZeroMemory(pServiceLedSettingsInfo, sizeof(NLED_SETTINGS_INFO));

    dwRetVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                            REG_NLED_KEY_CONFIG_PATH,
                            0,
                            0,
                            &hNLEDConfigKey);

    if(dwRetVal != ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to open the Config directory for LEDs"));

        dwTestResult = TPR_FAIL;
        goto end;
    }

    dwRetVal = RegEnumKeyEx(hNLEDConfigKey,
                            dwCurLEDIndex,
                            szLedName,
                            &dwLEDNameSize,
                            NULL,
                            NULL,
                            NULL,
                            NULL);

    if(ERROR_NO_MORE_ITEMS == dwRetVal)
    {
        //Log the error message
        g_pKato->Log(LOG_FAIL, TEXT("No LEDs configured in the registry"));
        dwTestResult = TPR_FAIL;
        goto end;
     }
    else if(dwRetVal != ERROR_SUCCESS)
    {
        //Log the error message
        g_pKato->Log(LOG_FAIL, TEXT("Registry Enumeration failed"));
        dwTestResult = TPR_FAIL;
        goto end;
     }

    while(dwRetVal ==  ERROR_SUCCESS)
    {
        //Restoring event index
        dwCurLEDEventIndex = 0;

        dwRetVal = RegOpenKeyEx(hNLEDConfigKey,
                                szLedName,
                                0,
                                KEY_ALL_ACCESS,
                                &hLEDKey);

        if(dwRetVal != ERROR_SUCCESS)
        {
            //Log the error message
            g_pKato->Log(LOG_FAIL,
                         TEXT("Unable to open the LED registry"));

            dwTestResult = TPR_FAIL;
            goto end;
        }

        dwRetVal = GetDwordFromReg(hLEDKey, REG_BLINKABLE_VALUE, &dwBlinkable);

        if(!dwRetVal)
        {
            //Log the error message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Blinkable field not configured in registry for LED %s"),
                         szLedName);

            dwTestResult = TPR_FAIL;
            goto end;
        }

        //LED should blink if it can blink or SWBlinkCtrl is set to 1
        //SWBlinkCtrl field is optional
        GetDwordFromReg(hLEDKey, REG_SWBLINKCTRL_VALUE, &dwSWBlinkCtrl);

        if(dwBlinkable || dwSWBlinkCtrl)
        {
            //Got the first LED that supports blinking.  Now proceed.
            dwRetVal = RegEnumKeyEx(hLEDKey,
                                    dwCurLEDEventIndex,
                                    szEventName,
                                    &dwEventNameSize,
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);

            if(ERROR_NO_MORE_ITEMS == dwRetVal)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("No events configured for %s"),
                             szLedName);

                dwTestResult = TPR_SKIP;
            }

            //For first event of the LED
            if(ERROR_SUCCESS == dwRetVal)
            {
                dwRetVal = RegOpenKeyEx(hLEDKey,
                                        szEventName,
                                        0,
                                        KEY_ALL_ACCESS,
                                        &hEventKey);

                if(dwRetVal != ERROR_SUCCESS)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to open the Event registry key for %s"),
                                 szEventName);

                    dwTestResult = TPR_FAIL;
                    goto end;
                }

                //Get the Led Number
                dwRetVal = GetDwordFromReg(hLEDKey, REG_LED_NUM_VALUE, &dwLedNum);

                if(!dwRetVal)
                {
                    //Log the error message
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to get the Led Num for LED %s"),
                                 szLedName);

                    dwTestResult = TPR_FAIL;
                    goto end;
                }

                //Turning the selected Led to OFF state
                pLedSettingsInfo->LedNum = dwLedNum;
                pLedSettingsInfo->OffOnBlink = LED_OFF;

                // Clearing of the previous value
                g_fMULTIPLENLEDSETFAILED = FALSE;

                //Create the thread
                hThread[0] =  CreateThread (NULL,
                                            0,
                                            (LPTHREAD_START_ROUTINE)SetNledThread,
                                            pLedSettingsInfo,
                                            0,
                                            NULL);

                //Backup  Event Registry Values
                dwRetVal = ReadEventDetails(hLEDKey,
                                            hEventKey,
                                            &nledEventDetailsStruct);

                if (!dwRetVal)
                {
                    dwTestResult = TPR_FAIL;

                    //Waiting for the thread to complete execution
                    CheckThreadCompletion(hThread, sizeof(hThread), MAX_THREAD_TIMEOUT_VAL);

                    goto end;
                }

                if(LED_BLINK != nledEventDetailsStruct.dwState)
                {
                    //Set Event State to Blink.
                    dwRetVal = RegSetValueEx (hEventKey,
                                              REG_EVENT_STATE_VALUE,
                                              0,
                                              REG_DWORD,
                                              (PBYTE)&dwState,
                                              sizeof(DWORD));

                    if(dwRetVal != ERROR_SUCCESS)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable To Set State for %s Event for Nled %u"),
                                     szEventName,
                                     nledEventDetailsStruct.dwLedNum);

                        dwTestResult = TPR_FAIL;

                        //Waiting for the thread to complete execution
                        CheckThreadCompletion(hThread, sizeof(hThread), MAX_THREAD_TIMEOUT_VAL);

                        goto end;
                    }
                }

                //Create Temporary Trigger and Modify the Path to Temporary Trigger
                //Set the temporary Trigger

                //Create  Temporary Event
                dwRetVal = Create_Set_TemporaryTrigger(hEventKey,
                                                       TEMPO_EVENT_PATH,
                                                       &nledEventDetailsStruct,
                                                       &hTempoEventKey);

                if (!dwRetVal)
                {
                    dwTestResult = TPR_FAIL;

                    //Waiting for the thread to complete execution
                    CheckThreadCompletion(hThread, sizeof(hThread), MAX_THREAD_TIMEOUT_VAL);
                    
                    goto end;
                }

                //Unload and Load Service for the Path Change to get affect
                dwRetVal = UnloadLoadService();

                if(!dwRetVal)
                {
                    dwTestResult = TPR_FAIL;
                    FAIL("Failed to UnLoad-Load the Nled Service");

                    //Waiting for the thread to complete execution
                    CheckThreadCompletion(hThread, sizeof(hThread), MAX_THREAD_TIMEOUT_VAL);

                    goto end;
                }

                //Raise the Temporary Trigger
                dwRetVal = RaiseTemporaryTrigger(hTempoEventKey,
                                                 &nledEventDetailsStruct);

                if(!dwRetVal)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to raise the %s event temporarly from the test"),
                                 szEventName);

                    dwTestResult = TPR_FAIL;

                    //Waiting for the thread to complete execution
                    CheckThreadCompletion(hThread, sizeof(hThread), MAX_THREAD_TIMEOUT_VAL);

                    goto end;
                }

                //Wait for service to act
                Sleep(SLEEP_FOR_ONE_SECOND);

                //Get Currrent LED Settings
                dwRetVal = GetCurrentNLEDDriverSettings(nledEventDetailsStruct,
                                                        pServiceLedSettingsInfo);

                if(!dwRetVal)
                {
                    dwTestResult = TPR_FAIL;

                    //Waiting for the thread to complete execution
                    CheckThreadCompletion(hThread, sizeof(hThread), MAX_THREAD_TIMEOUT_VAL);

                    goto end;
                }

                //Check whether service still exists
                LPWSTR lpServiceName = L"nledsrv.dll";
                dwRetVal = IsServiceRunning(lpServiceName);

                if(!dwRetVal)
                {
                    FAIL("Nled Service fails to handle setting of different events to same LED simultaneously");
                    dwTestResult = TPR_FAIL;
                }
                else
                {
                    SUCCESS("Nled Service successfully handles setting of different events to same LED simultaneously");
                    dwTestResult = TPR_PASS;
                }

                //Test is completed so terminate the thread
                g_fTestCompleted = TRUE;

                //Check if all the threads are done with the execution
                dwRetVal = CheckThreadCompletion(hThread, sizeof(hThread), MAX_THREAD_TIMEOUT_VAL);

                if((TPR_FAIL == dwRetVal) || g_fMULTIPLENLEDSETFAILED)
                {
                   dwTestResult = TPR_FAIL;
                }

                goto end;
            }
        }

        RegCloseKey(hLEDKey);
        dwCurLEDIndex++;
        dwLEDNameSize = gc_dwMaxKeyNameLength + 1;

        //Enumerate for another Leds
        dwRetVal = RegEnumKeyEx(hNLEDConfigKey,
                                dwCurLEDIndex,
                                szLedName,
                                &dwLEDNameSize,
                                NULL,
                                NULL,
                                NULL,
                                NULL);
    }

    if(TPR_PASS == dwTestResult)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("None of the Nleds on the device supports blinking. Skipping the test"));

        dwTestResult = TPR_SKIP;
    }

end:

    //Delete Temporary Trigger
    if (hTempoEventKey)
    {
        //Resetting Temporary Trigger.
        dwRetVal = Unset_TemporaryTrigger(hEventKey,
                                          hTempoEventKey,
                                          &nledEventDetailsStruct);

        if(!dwRetVal)
        {
            dwTestResult = TPR_FAIL;
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to unset temporary trigger for Nled %u"),
                         nledEventDetailsStruct.dwLedNum);
        }

        dwRetVal = DeleteTemporaryTrigger(hTempoEventKey,
                                          TEMPO_EVENT_PATH,
                                          pLedSettingsInfo,
                                          &nledEventDetailsStruct);

        if(!dwRetVal)
        {
            dwTestResult = TPR_FAIL;
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to delete temporary trigger for Nled %u"),
                         nledEventDetailsStruct.dwLedNum);
        }
    }

    if (hEventKey)
    {
        //Restore state for  Event
        dwRetVal = RegSetValueEx(hEventKey,
                                 REG_EVENT_STATE_VALUE,
                                 0,
                                 REG_DWORD,
                                 (PBYTE)&(nledEventDetailsStruct.dwState),
                                 sizeof(DWORD));

        if(dwRetVal != ERROR_SUCCESS)
        {
            dwTestResult = TPR_FAIL;
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable To Restore State of the Event"));
        }
    }

    //Restore back the original Event Paths
    dwRetVal = RestoreAllNledServiceEventPaths(pEventDetails_list,
                                               hEventOrigKey,
                                               dwTotalEvents);

    if(!dwRetVal)
    {
        dwTestResult = TPR_FAIL;
        FAIL("Unable to restore back the original event paths");
    }

    //Delete the temporary registry key
    if(hTemporaryEventKey != NULL)
    {
        dwRetVal =  RegCloseKey(hTemporaryEventKey);

        if(dwRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Closing Failed. Last Error = %ld"),
                         GetLastError());

            dwTestResult = TPR_FAIL;
        }

        dwRetVal = RegDeleteKey(HKEY_LOCAL_MACHINE, TEMPORARY_EVENT_PATH);

        if(dwRetVal !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Deletion Failed. Last Error = %ld"),
                         GetLastError());

            dwTestResult = TPR_FAIL;
        }
    }

    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo != NULL)
    {
        fStatus =  setAllLedSettings(pOriginalLedSettingsInfo);

        if(!fStatus)
        {
            dwTestResult = TPR_FAIL;
            FAIL("Unable to restore original NLED settings");
        }

        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    if(pLedSettingsInfo != NULL)
    {
        delete[] pLedSettingsInfo;
        pLedSettingsInfo = NULL;
    }

    if(pServiceLedSettingsInfo != NULL)
    {
        delete[] pServiceLedSettingsInfo;
        pServiceLedSettingsInfo = NULL;
    }

    if(NULL != hThread[0])
    {
        CloseHandle(hThread[0]);
        hThread[0] = NULL;
    }

    //Close the registry keys and return
    if(hNLEDConfigKey != NULL)
    {
        RegCloseKey(hNLEDConfigKey);
    }

    if(hLEDKey != NULL)
    {
        RegCloseKey(hLEDKey);
    }

    if(hEventKey != NULL)
    {
        RegCloseKey(hEventKey);
    }

    if(pLedNumEventId != NULL)
    {
        delete[] pLedNumEventId;
        pLedNumEventId = NULL;
    }

    if(hEventOrigKey != NULL)
    {
        delete[] hEventOrigKey;
        hEventOrigKey = NULL;
    }

    if(pEventDetails_list != NULL)
    {
        delete[] pEventDetails_list;
        pEventDetails_list = NULL;
    }

    if(!g_fTestCompleted)
    {
        //Test is completed so terminate the thread
        g_fTestCompleted = TRUE;
    }

    return dwTestResult;
}

//*****************************************************************************
void SetNledThread(NLED_SETTINGS_INFO* ledSettingsInfo_Set)
//
// Parameters: NLED_SETTINGS_INFO structure
//
// Return Value: NONE
//
// A thread that continuously sets the LED with a state of off
//
//*****************************************************************************
{
    //Continuously set the LED with state of OFF
       
    BOOL fRetVal = NLedSetDevice(NLED_SETTINGS_INFO_ID, ledSettingsInfo_Set);

    while((fRetVal) && (!g_fTestCompleted))
    {
        fRetVal = NLedSetDevice(NLED_SETTINGS_INFO_ID, ledSettingsInfo_Set);
    }

    if(!fRetVal)
    {
        //Logging the output using the kato logging engine
        FAIL("Nled Driver failed to set the Nled Settings Info");
        g_fMULTIPLENLEDSETFAILED = TRUE;
    }

    return;
}

//*****************************************************************************
TESTPROCAPI NLEDServiceEventDelayTest(UINT                   uMsg,
                                      TPPARAM                tpParam,
                                      LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Return Value:   TPR_PASS On Success,
//                  TPR_FAIL On Failure
//                  TPR_NOT_HANDLED If the feature is not handled,
//                  TPR_SKIP If the test is skipped
//
//  Test to verify whether the delay between firing an event and the resultant LED
//  state change is within a specified limit.  This test is done for the first
//  LED which has at least one event configured.  This test will check whether
//  the time difference between setting an event and actual firing of that event
//  is within the allowed limits.
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Only Supporting executing the test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the service or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fServiceTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    //Returning pass if the device does not have any NLEDs
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("There are no LEDs to Test. Automatic PASS"));

        return TPR_PASS;
    }

    //Storing the original LED settings
    NLED_SETTINGS_INFO *pOriginalLedSettingsInfo = NULL;
    NLED_EVENT_DETAILS nledEventDetailsStruct = {0};
    BOOL fStatus = FALSE;
    DWORD dwRetVal = TPR_FAIL;
    DWORD *pLedNumEventId = NULL;
    DWORD dwTotalEvents = 0;
    NLED_EVENT_DETAILS *pEventDetails_list = NULL;
    HKEY *hEventOrigKey = NULL;
    HKEY hTemporaryEventKey = NULL;
    HKEY hNLEDConfigKey = NULL;
    HKEY hLEDKey = NULL;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLEDNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwCurLEDIndex = 0;
    DWORD dwResult = 0;
    HKEY hEventKey = NULL;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;
    BOOL fFoundMatchingLED = FALSE;
    INT NewState = 0;
    HKEY hTempoEventKey = NULL;
    NLED_SETTINGS_INFO *pLedSettingsInfo  = NULL;

    //Variables for retrieving time
    DWORD dwEndTickCount = 0;
    DWORD dwStartTickCount = 0;
    DWORD dwCurrentTickCount = 0;

    // Allocate space to save origional LED setttings.
    SetLastError(ERROR_SUCCESS);
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];

    if(!pOriginalLedSettingsInfo)
    {
        FAIL("Memory allocation failed");
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tLast Error = %d"),
                     GetLastError());

        return TPR_FAIL;
    }

    ZeroMemory(pOriginalLedSettingsInfo, GetLedCount() * sizeof(NLED_SETTINGS_INFO));

    // Save the Info Settings for all the LEDs and turn them off.
    fStatus =  saveAllLedSettings(pOriginalLedSettingsInfo, LED_OFF);

    if(!fStatus)
    {
        FAIL("Save LED settings failed");
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    }

    //Pointer to an Array to store the Info if the Nled has any events configured
    pLedNumEventId = new DWORD[GetLedCount()];

    //Getting the Event Ids for all the Nleds
    dwRetVal = GetNledEventIds(pLedNumEventId);

    if(TPR_PASS != dwRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to retrieve total number of events for all the Leds."));

        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Calculating the total number of events of all the Leds
    for(DWORD i = 0; i < GetLedCount(); i++)
    {
        dwTotalEvents = dwTotalEvents + pLedNumEventId[i];
    }

    if(0 == dwTotalEvents)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("No LED has at least one event configured.  Skipping the test."));

        dwRetVal = TPR_SKIP;
        goto end;
    }

    //Storing the event paths and unsetting all the events
    pEventDetails_list = new NLED_EVENT_DETAILS[dwTotalEvents];

    for(DWORD i=0;i<dwTotalEvents;i++)
    {
        ZeroMemory(&pEventDetails_list[i], sizeof(NLED_EVENT_DETAILS));
    }

    hEventOrigKey = new HKEY[dwTotalEvents];

    dwRetVal = UnSetAllNledServiceEvents(pEventDetails_list,
                                          &hTemporaryEventKey,
                                         hEventOrigKey);

    if(!dwRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to unset all the Nled Service Events"));

        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Allocating memory for getting the current nled settings
    pLedSettingsInfo  = new NLED_SETTINGS_INFO[1];

    if(!pLedSettingsInfo)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("\tMemory allocation failed. Error code = %ld"),
                     GetLastError());

        dwRetVal = TPR_FAIL;
        goto end;
    }

    ZeroMemory(pLedSettingsInfo, sizeof(NLED_SETTINGS_INFO));

    //Getting the config key
    dwRetVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                            REG_NLED_KEY_CONFIG_PATH,
                            0,
                            0,
                            &hNLEDConfigKey);

    if(ERROR_SUCCESS != dwRetVal)
    {
        //Log the error message
        FAIL("Unable to open the LED registry for LED config key");
        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Enumerating LED Name
    dwRetVal = RegEnumKeyEx(hNLEDConfigKey,
                            dwCurLEDIndex,
                            szLedName,
                            &dwLEDNameSize,
                            NULL,
                            NULL,
                            NULL,
                            NULL);

    if(ERROR_NO_MORE_ITEMS == dwRetVal)
    {
        //Log Error Message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("No LEDs configured in the registry.  Service Delay Test Failed"));

        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Iterating until we find an LED with at least one event
    while(ERROR_SUCCESS == dwRetVal)
    {
        //Open LED Registry
        dwRetVal = RegOpenKeyEx(hNLEDConfigKey,
                                szLedName,
                                0,
                                KEY_ALL_ACCESS,
                                &hLEDKey);

        if(ERROR_SUCCESS != dwRetVal)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to open the %s LED key in registry. Service delay test failed"),
                         szLedName);

            dwRetVal = TPR_FAIL;
            goto end;
        }

        //Enumerating LED Event
        dwRetVal = RegEnumKeyEx(hLEDKey,
                                0,
                                szEventName,
                                &dwEventNameSize,
                                NULL,
                                NULL,
                                NULL,
                                NULL);

        if(ERROR_NO_MORE_ITEMS == dwRetVal)
        {
           g_pKato->Log(LOG_DETAIL,
                        TEXT("No events configured for %s LED"),
                        szLedName);
        }
        else if (ERROR_SUCCESS == dwRetVal)
        {
            fFoundMatchingLED = TRUE;
            break;
        }

        //Enumerating for Next LED.
        dwCurLEDIndex++;
        RegCloseKey(hLEDKey);
        dwLEDNameSize = gc_dwMaxKeyNameLength + 1;
        dwRetVal = RegEnumKeyEx(hNLEDConfigKey,
                                dwCurLEDIndex,
                                szLedName,
                                &dwLEDNameSize,
                                NULL,
                                NULL,
                                NULL,
                                NULL);
    }

    //Return fail if no LED with an event present
    if(!fFoundMatchingLED)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("No LED with at least one event present. Skipping the test."));

        dwRetVal = TPR_SKIP;
        goto end;
    }

    //Opening Event Key
    dwRetVal = RegOpenKeyEx(hLEDKey, szEventName, 0, KEY_ALL_ACCESS, &hEventKey);

    if(ERROR_SUCCESS != dwRetVal)
    {
        //Log Error Message
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to open the %s event key. Service Delay Test Failed"),
                     szEventName);

        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Backup Event Registry Values.
    dwRetVal = ReadEventDetails(hLEDKey, hEventKey, &nledEventDetailsStruct);

    if (!dwRetVal)
    {
        //Log Error Message
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to Read Event Details. Service Delay Test Failed."));

        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Get the driver settings
    dwRetVal = GetCurrentNLEDDriverSettings(nledEventDetailsStruct,
                                            pLedSettingsInfo);

    if(!dwRetVal)
    {
        g_pKato->Log(LOG_FAIL, TEXT("Failed to retrieve NLED driver settings"));
        dwRetVal = TPR_FAIL;
        goto end;
    }

    //If the current state is ON, set it to OFF. Else set to ON
    if(LED_ON == pLedSettingsInfo->OffOnBlink)
        NewState = LED_OFF;
    else
        NewState = LED_ON;

    //Set the new state
    dwRetVal = RegSetValueEx (hEventKey,
                              REG_EVENT_STATE_VALUE,
                              0,
                              REG_DWORD,
                              (PBYTE) (&NewState),
                              sizeof(NewState));

    if(dwRetVal != ERROR_SUCCESS)
    {
        //Log Error Message
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable To Set LED State. Service Delay Test Failed"));

        dwRetVal = TPR_FAIL;
        goto end;
    }

    //Set Temporary Trigger
    dwRetVal = Create_Set_TemporaryTrigger(hEventKey,
                                           TEMPO_EVENT_PATH,
                                           &nledEventDetailsStruct,
                                           &hTempoEventKey);

    if (!dwRetVal)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to create a temporary trigger. Service Delay Test Failed"));

        dwRetVal = TPR_FAIL;
        goto cleanup;
    }

    //Inform the service about the registry changes
    dwRetVal = UnloadLoadService();

    if(!dwRetVal)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Error in re-loading the service. Service Delay test Failed"));

        dwRetVal = TPR_FAIL;
        goto cleanup;
    }

    //Get the time when event is fired
    dwStartTickCount = GetTickCount();

    //Raising Trigger
    dwRetVal = RaiseTemporaryTrigger(hTempoEventKey, &nledEventDetailsStruct);

    if(!dwRetVal)
    {
        g_pKato->Log(LOG_DETAIL, TEXT("Error in raising the temporary Event"));
        dwRetVal = TPR_FAIL;
        goto cleanup;
    }

    //Wait for service to act
    Sleep(SLEEP_FOR_ONE_SECOND);

      
    //Get the driver settings
    dwRetVal = GetCurrentNLEDDriverSettings(nledEventDetailsStruct,
                                                pLedSettingsInfo);
    if(!dwRetVal)
    {
        g_pKato->Log(LOG_FAIL,
            TEXT("Failed to retrieve NLED driver settings"));
        dwRetVal = TPR_FAIL;
        goto cleanup;
    }

    //Checking untill the Led State is changed
    while(!pLedSettingsInfo->OffOnBlink == NewState)
    {
        //Checking for Max wait time
        dwCurrentTickCount = GetTickCount();

        if((dwCurrentTickCount - dwStartTickCount) > MAX_WAIT_TIME)
        {
            g_pKato->Log(LOG_DETAIL,
                TEXT("Configured event not fired within Time. Timed out"));
            dwRetVal = TPR_FAIL;
            goto cleanup;
        }

        //Get the driver settings again
        dwRetVal = GetCurrentNLEDDriverSettings(nledEventDetailsStruct,
                                                pLedSettingsInfo);

        if(!dwRetVal)
        {
            g_pKato->Log(LOG_FAIL,
                TEXT("Failed to retrieve NLED driver settings"));
            dwRetVal = TPR_FAIL;
            goto cleanup;
        }
    }

    //Finding time interval between event getting fired and the led actually changing the state
    dwEndTickCount = GetTickCount();

    if((dwEndTickCount - dwStartTickCount) < g_nMaxTimeDelay)
    {
        dwRetVal = TPR_PASS;
        SUCCESS("Event fired in time, Service Time Delay Test passed");
    }
    else
    {
        dwRetVal = TPR_FAIL;
        FAIL("Event did not fire in time, Service Time Delay Test failed");
    }

cleanup:
    //Reset the raised event and restore trigger path
    if(fFoundMatchingLED)
    {
        dwResult = Unset_TemporaryTrigger(hEventKey,
                                          hTempoEventKey,
                                          &nledEventDetailsStruct);

        if(!dwResult)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to reset the trigger from test"));

            dwRetVal = TPR_FAIL;
        }

        //Delete temporary trigger
        dwResult = DeleteTemporaryTrigger(hTempoEventKey,
                                          TEMPO_EVENT_PATH,
                                          pLedSettingsInfo,
                                          &nledEventDetailsStruct);

        if(!dwResult)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to delete the temporary trigger path from registry"));

            dwRetVal = TPR_FAIL;
        }

        //Restore Original State
        dwResult = RegSetValueEx (hEventKey, REG_EVENT_STATE_VALUE, 0, REG_DWORD, (PBYTE) (&nledEventDetailsStruct.dwState), sizeof(nledEventDetailsStruct.dwState));

        if(dwResult != ERROR_SUCCESS)
        {
            //Log Error Message
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable To Restore Original State. Service Delay Test Failed"));

            dwRetVal = TPR_FAIL;
        }
    }

end:
    //Restore back the original Event Paths
    dwResult = RestoreAllNledServiceEventPaths(pEventDetails_list,
                                               hEventOrigKey,
                                               dwTotalEvents);

    if(!dwResult)
    {
        dwRetVal = TPR_FAIL;
        FAIL("Unable to restore back the original event paths");
    }

    //Delete the temporary registry key
    if(hTemporaryEventKey != NULL)
    {
        dwResult =  RegCloseKey(hTemporaryEventKey);

        if(dwResult !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Closing Failed. Last Error = %ld"),
                         GetLastError());

            dwRetVal = TPR_FAIL;
        }

        dwResult = RegDeleteKey(HKEY_LOCAL_MACHINE, TEMPORARY_EVENT_PATH);

        if(dwResult !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Deletion Failed. Last Error = %ld"),
                         GetLastError());

            dwRetVal = TPR_FAIL;
        }
    }

    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo != NULL)
    {
        fStatus =  setAllLedSettings(pOriginalLedSettingsInfo);

        if(!fStatus)
        {
            dwRetVal = TPR_FAIL;
            FAIL("Unable to restore original NLED settings");
        }

        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

       if(pLedSettingsInfo != NULL)
    {
        delete[] pLedSettingsInfo;
        pLedSettingsInfo = NULL;
    }

    //Close the registry keys and return
    if(hNLEDConfigKey != NULL)
    {
        RegCloseKey(hNLEDConfigKey);
    }

    if(hLEDKey != NULL)
    {
        RegCloseKey(hLEDKey);
    }

    if(hEventKey != NULL)
    {
        RegCloseKey(hEventKey);
    }

    if(pLedNumEventId != NULL)
    {
        delete[] pLedNumEventId;
        pLedNumEventId = NULL;
    }

    if(hEventOrigKey != NULL)
    {
        delete[] hEventOrigKey;
        hEventOrigKey = NULL;
    }

    if(pEventDetails_list != NULL)
    {
        delete[] pEventDetails_list;
        pEventDetails_list = NULL;
    }

    return dwRetVal;
}

//*****************************************************************************
TESTPROCAPI NledServiceContinuousRegistryChangesTest(UINT                   uMsg,
                                                     TPPARAM                tpParam,
                                                     LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters:     standard Tux arguments
//
//  Return Value:   TPR_PASS On Success,
//                  TPR_FAIL On Failure
//                  TPR_NOT_HANDLED If the feature is not handled,
//                  TPR_SKIP If the test is skipped
//
//  Objectives:
//  For each event of each LED in the registry, change the state value continuously
//  to ON, OFF or BLINK. For Blink state, also try with different blink parameter
//  values.  After each state change, fire the events continuously for a prolonged
//  time. Atlast check whether the service is still active.  If yes, the test case
//  is considered as passed.  Otherwise the test case fails.
//
//  Simultaneous event firing – Service must handle the scenario when all the
//  events corresponding to all LEDs are fired simultaneously.
//
//  Prolonged event firing – Service must handle the scenario when all the
//  events are fired for long duration of time.
//
//  Continuous registry changes – Service must handle the scenario when registry
//  values are modified continuously and corresponding events are fired
//  continuously.
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Only Supporting executing the test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the service or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fServiceTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    //Returning pass if the device does not have any NLEDs
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("There are no LEDs to Test. Automatic PASS"));

        return TPR_PASS;
    }

    //Notify the user regarding the registry being changed
    if(g_fInteractive)
    {
        MessageBox(NULL,
                   TEXT("Nled registry is being changed to test the Nled Service performance tests, Do not perform any critical operations"),
                   TEXT("NLED Service Continuous Registry changes Test"), MB_OK);
    }
    else
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Nled registry is being changed to test the Nled Service performance tests, Do not perform any critical operations"));
    }

    //Declaring the variables
    BOOL  fStatus = FALSE;
    DWORD dwRetVal = 0;
    DWORD *pLedNumEventId = NULL;
    DWORD dwTotalEvents = 0;
    NLED_SUPPORTS_INFO ledSupportsInfo_Get  = {0};
    NLED_EVENT_DETAILS *pEventDetailsList = NULL;
    HKEY *hEventOrigKey = NULL;
    HKEY  hTemporaryEventKey = NULL;
    HKEY  hNLEDConfigKey = NULL;
    HKEY  hLEDKey =  NULL;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLEDNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwCurLEDIndex = 0;
    BOOL  fCreatedTemporaryRegistryKey = FALSE;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwCurLEDEventIndex = 0;
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;
    HKEY  hTempoEventKey = NULL;
    DWORD dwDisp = 0;
    INT   TriggerValue = -1;
    NLED_EVENT_DETAILS *pEventDetails_list = NULL;
    DWORD dwEventDetailsCount = 0;
    HKEY *hEventKey = NULL;
    DWORD *dwDeleteOperator = NULL;
    DWORD *dwDeleteTotalCycleTime = NULL;
    DWORD *dwDeleteOnTime = NULL;
    DWORD *dwDeleteOffTime = NULL;
    DWORD *dwDeleteMetaCycleOn = NULL;
    DWORD *dwDeleteMetaCycleOff = NULL;
    DWORD dwNewOperator = 0;
    DWORD dwNewValue = 0;
    DWORD dwTempEventPathLen = 0;
    DWORD dwTempState = 0;
    DWORD dwTempValue = 0;
    DWORD dwReturn = 0;
    DWORD dwValueLen = 0;
    DWORD dwPathLen = 0;

    //Storing the original LED settings
    NLED_SETTINGS_INFO *pOriginalLedSettingsInfo = NULL;

    // Allocate space to save origional LED setttings.
    SetLastError(ERROR_SUCCESS);
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];

    if(!pOriginalLedSettingsInfo)
    {
        FAIL("Memory allocation failed");
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tLast Error = %d"),
                     GetLastError());

        return TPR_FAIL;
    }

    ZeroMemory(pOriginalLedSettingsInfo, GetLedCount() * sizeof(NLED_SETTINGS_INFO));

    // Save the Info Settings for all the LEDs and turn them off.
    fStatus =  saveAllLedSettings(pOriginalLedSettingsInfo, LED_OFF);

    if(!fStatus)
    {
        FAIL("Save LED settings failed");
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    }

    //Pointer to an Array to store the Info if the Nled has any events configured
    pLedNumEventId = new DWORD[GetLedCount()];

    //Getting the Event Ids for all the Nleds
    dwRetVal = GetNledEventIds(pLedNumEventId);

    if(TPR_PASS != dwRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to retrieve total number of events for all the Leds."));

        dwRetVal = TPR_FAIL;
        goto cleanup;
    }

    //Calculating the total number of events of all the Leds
    for(DWORD i = 0; i < GetLedCount(); i++)
    {
        dwTotalEvents = dwTotalEvents + pLedNumEventId[i];
    }

    if(0 == dwTotalEvents)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("No LED has at least one event configured.  Skipping the test."));

        dwRetVal = TPR_SKIP;
        goto cleanup;
    }

    //Storing the event paths and unsetting all the events
    pEventDetailsList = new NLED_EVENT_DETAILS[dwTotalEvents];

    for(DWORD i=0;i<dwTotalEvents;i++)
    {
        ZeroMemory(&pEventDetailsList[i], sizeof(NLED_EVENT_DETAILS));
    }

    hEventOrigKey = new HKEY[dwTotalEvents];

    dwRetVal = UnSetAllNledServiceEvents(pEventDetailsList,
                                          &hTemporaryEventKey,
                                         hEventOrigKey);

    if(!dwRetVal)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to unset all the Nled Service Events"));

        dwRetVal = TPR_FAIL;
        goto cleanup;
    }

    //Create the Temporary Trigger event
    dwRetVal = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                              TEMPO_EVENT_PATH,
                              0,
                              NULL,
                              REG_OPTION_VOLATILE,
                              KEY_ALL_ACCESS,
                              NULL,
                              &hTempoEventKey,
                              &dwDisp);

    if(dwRetVal != ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to create a temporary event in registry"));

        dwRetVal = TPR_FAIL;
        goto cleanup;
    }

    fCreatedTemporaryRegistryKey = TRUE;

    //Set the triggervalue for the temporary trigger
    dwRetVal = RegSetValueEx (hTempoEventKey,
                              REG_TRIGGER_VALUENAME,
                              0,
                              REG_DWORD,
                              (PBYTE)(&TriggerValue),
                              sizeof(DWORD));

    if(dwRetVal !=ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to set the triggervalue for the temporary event from the test"));

         dwRetVal = TPR_FAIL;
        goto cleanup;
    }

    //Unload and Load Service for affecting updated Registry values.
    dwRetVal = UnloadLoadService();

    if(!dwRetVal)
    {
        FAIL("Error in re-loading the service");
        dwRetVal = TPR_FAIL;
        goto cleanup;
    }

    //Variables for storing event details info
    pEventDetails_list = new NLED_EVENT_DETAILS[dwTotalEvents];

    for(DWORD i=0;i<dwTotalEvents;i++)
    {
        ZeroMemory(&pEventDetails_list[i], sizeof(NLED_EVENT_DETAILS));
    }

    //Memory allocations
    hEventKey = new HKEY[dwTotalEvents];
    dwDeleteOperator = new DWORD[dwTotalEvents];
    dwDeleteTotalCycleTime = new DWORD[dwTotalEvents];
    dwDeleteOnTime = new DWORD[dwTotalEvents];
    dwDeleteOffTime = new DWORD[dwTotalEvents];
    dwDeleteMetaCycleOn = new DWORD[dwTotalEvents];
    dwDeleteMetaCycleOff = new DWORD[dwTotalEvents];

    //Opening Config Key
    dwRetVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                            REG_NLED_KEY_CONFIG_PATH,
                            0,
                            0,
                            &hNLEDConfigKey);

    if(dwRetVal != ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to open the Config directory for LEDs. Continuous Registry Changes Test Failed"));

        dwRetVal = TPR_FAIL;
        goto cleanup;
    }

    //Enumerating LED Name
    dwRetVal = RegEnumKeyEx(hNLEDConfigKey,
                            dwCurLEDIndex,
                            szLedName,
                            &dwLEDNameSize,
                            NULL,
                            NULL,
                            NULL,
                            NULL);

    if(dwRetVal != ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("No LEDs configured in the registry. Continuous Registry Changes Test Failed"));

        dwRetVal = TPR_FAIL;
        goto cleanup;
    }

    //Iteration for All Events in the LED.
    while(ERROR_SUCCESS == dwRetVal)
    {
        //Re-Initializing the event index
        dwCurLEDEventIndex = 0;

        //Opening Event Key
        dwRetVal = RegOpenKeyEx(hNLEDConfigKey,
                                szLedName,
                                0,
                                KEY_ALL_ACCESS,
                                &hLEDKey);

        if(dwRetVal != ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to open the LED registry for LED name %s"),
                         szLedName);

            dwRetVal = TPR_FAIL;
            goto cleanup;
        }

        //Enumerating Event
        dwRetVal = RegEnumKeyEx(hLEDKey,
                                dwCurLEDEventIndex,
                                szEventName,
                                &dwEventNameSize,
                                NULL,
                                NULL,
                                NULL,
                                NULL);

        //Iteration for All Events in the LED.
        while(ERROR_SUCCESS == dwRetVal)
        {
            //Opening the Event Key
            dwRetVal = RegOpenKeyEx(hLEDKey,
                                    szEventName,
                                    0,
                                    KEY_ALL_ACCESS,
                                    &hEventKey[dwEventDetailsCount]);

            if(dwRetVal != ERROR_SUCCESS)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to open the LED events registry for %s"),
                             szEventName);

                dwRetVal = TPR_FAIL;
                goto cleanup;
            }

            //Checking if Operator field is present
            if(!GetDwordFromReg(hEventKey[dwEventDetailsCount],
                                REG_EVENT_OPERATOR_VALUE,
                                &pEventDetails_list[dwEventDetailsCount].dwOperator))
            {
                //Operator is not a mandatory value. so delete it if it is not present for any event in the cleanup
                dwDeleteOperator[dwEventDetailsCount] = 1;
            }

            //Checking for TotalCycleTime field
            if(!GetDwordFromReg(hEventKey[dwEventDetailsCount],
                                REG_EVENT_TOTAL_CYCLE_TIME_VALUE,
                                &pEventDetails_list[dwEventDetailsCount].dwTotalCycleTime))
            {
                //TotalCycleTime is not a mandatory value. so delete it if it is not present for any event in the cleanup
                dwDeleteTotalCycleTime[dwEventDetailsCount] = 1;
            }

            //Checking for OnTime field
            if(!GetDwordFromReg(hEventKey[dwEventDetailsCount],
                                REG_EVENT_ON_TIME_VALUE,
                                &pEventDetails_list[dwEventDetailsCount].dwOnTime))
            {
                //OnTime is not a mandatory value. so delete it if it is not present for any event in the cleanup
                dwDeleteOnTime[dwEventDetailsCount] = 1;
            }

            //Checking for OffTime field
            if(!GetDwordFromReg(hEventKey[dwEventDetailsCount],
                                REG_EVENT_OFF_TIME_VALUE,
                                &pEventDetails_list[dwEventDetailsCount].dwOffTime))
            {
                //OffTime is not a mandatory value. so delete it if it is not present for any event in the cleanup
                dwDeleteOffTime[dwEventDetailsCount] = 1;
            }

            //Checking for MetaCycleOn field
            if(!GetDwordFromReg(hEventKey[dwEventDetailsCount],
                                REG_EVENT_META_CYCLE_ON_VALUE,
                                &pEventDetails_list[dwEventDetailsCount].dwMetaCycleOn))
            {
                //MetaCycleOn is not a mandatory value. so delete it if it is not present for any event in the cleanup
                dwDeleteMetaCycleOn[dwEventDetailsCount] = 1;
            }

            //Checking for MetaCycleOff field
            if(!GetDwordFromReg(hEventKey[dwEventDetailsCount],
                                REG_EVENT_META_CYCLE_OFF_VALUE,
                                &pEventDetails_list[dwEventDetailsCount].dwMetaCycleOff))
            {
                //MetaCycleOff is not a mandatory value. so delete it if it is not present for any event in the cleanup
                dwDeleteMetaCycleOff[dwEventDetailsCount] = 1;
            }

            //Preserving event details in the array
            dwRetVal = ReadEventDetails(hLEDKey,
                                        hEventKey[dwEventDetailsCount],
                                        &pEventDetails_list[dwEventDetailsCount]);

            if (!dwRetVal)
            {
                //Log Error Message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to Read Event Details. Simultaneous event Test Failed."));

                dwRetVal = TPR_FAIL;
                goto cleanup;
            }

            //Set the operator as 0 (meaning, =) for each Event
            dwRetVal = RegSetValueEx (hEventKey[dwEventDetailsCount],
                                      REG_EVENT_OPERATOR_VALUE,
                                      0,
                                      REG_DWORD,
                                      (PBYTE) (&dwNewOperator),
                                      sizeof(dwNewOperator));

            if(dwRetVal != ERROR_SUCCESS)
            {
                //Log Error Message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable To Set event Operator value for %s.  Continuous Registry Changes Test Failed"),
                             szEventName);

                dwRetVal = TPR_FAIL;
                goto cleanup;
            }

            //Set the trigger value as 0 for each Event
            dwRetVal = RegSetValueEx (hEventKey[dwEventDetailsCount],
                                      REG_EVENT_VALUE_VALUE,
                                      0,
                                      REG_DWORD,
                                      (PBYTE) (&dwNewValue),
                                      sizeof(dwNewValue));

            if(dwRetVal != ERROR_SUCCESS)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable To Set LED Trigger value for %s.  Continuous Registry Changes Test Failed"),
                             szEventName);

                dwRetVal = TPR_FAIL;
                goto cleanup;
            }

            //Set the ValueName as TriggerValueName for each Event
            dwRetVal = RegSetValueEx (hEventKey[dwEventDetailsCount],
                                      REG_EVENT_VALUE_NAME_VALUE,
                                      0,
                                      REG_SZ,
                                      (PBYTE)(REG_TRIGGER_VALUENAME),
                                      (wcslen(REG_TRIGGER_VALUENAME)+1)*sizeof(WCHAR));

            if(dwRetVal != ERROR_SUCCESS)
            {
                //Log Error Message
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable To Set LED Value name for %s.  Continuous Registry Changes Test Failed"),
                             szEventName);

                dwRetVal = TPR_FAIL;
                goto cleanup;
            }

            //Replace path with temporary trigger path
            dwTempEventPathLen = (wcslen(TEMPO_EVENT_PATH)+1)*sizeof(WCHAR);
            dwRetVal = RegSetValueEx (hEventKey[dwEventDetailsCount],
                                      REG_EVENT_PATH_VALUE,
                                      0,
                                      REG_SZ,
                                      (PBYTE)TEMPO_EVENT_PATH,
                                      dwTempEventPathLen);

            if(dwRetVal !=ERROR_SUCCESS)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable To change path for %s. Continuous Registry Changes Test Failed"),
                             szEventName);

                dwRetVal = TPR_FAIL;
                goto cleanup;
            }

            //Unload and Load Service for affecting updated Registry values.
            dwRetVal = UnloadLoadService();

            if(!dwRetVal)
            {
                FAIL("Error in re-loading the service");
                dwRetVal = TPR_FAIL;
                goto cleanup;
            }

            //Incrementing array counter after each event
            dwEventDetailsCount++;

            //Next Event in the LED
            dwCurLEDEventIndex++;
            dwEventNameSize = gc_dwMaxKeyNameLength + 1;

            dwRetVal = RegEnumKeyEx(hLEDKey,
                                    dwCurLEDEventIndex,
                                    szEventName,
                                    &dwEventNameSize,
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);

        }//Iteration for All Events in the LED.

        //Close the present Led Key
        RegCloseKey(hLEDKey);

        //Enumerating for Next LED.
        dwCurLEDIndex++;
        dwLEDNameSize = gc_dwMaxKeyNameLength + 1;

        dwRetVal = RegEnumKeyEx(hNLEDConfigKey,
                                dwCurLEDIndex,
                                szLedName,
                                &dwLEDNameSize,
                                NULL,
                                NULL,
                                NULL,
                                NULL);
    }

    //Firing all the events of all the Nleds repeatedly for a prolonged time
    //And also changing the registry values continously
    for(int i=0; i<100; i++)
    {
        //Fire all the events
        dwRetVal = fireEventsForRobustnessTests();

        //Wait for service to act
        Sleep(100);

        if(!dwRetVal)
        {
            //Return after cleanup
            g_pKato->Log(LOG_FAIL,
                         TEXT("Error in triggering all the events continuously"));

            dwRetVal = TPR_FAIL;
            goto cleanup;
        }

        //Reset the events fired just now.
        dwRetVal = ResetEventsForRobustnessTests();

        //Wait for service to act
        Sleep(100);

        if(!dwRetVal)
        {
            //Return after cleanup
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Error in resetting the triggered events"));

            dwRetVal = TPR_FAIL;
            goto cleanup;
        }

        //State can take a value of 0, 1, 2
        dwTempState = i % 3;
        dwTempValue = 0;

        //Now changing the state and blinking parameters continuously for all the events
        for(DWORD dwCount = 0; dwCount < dwEventDetailsCount; dwCount++)
        {
            dwRetVal = RegSetValueEx(hEventKey[dwCount],
                                     REG_EVENT_STATE_VALUE,
                                     0,
                                     REG_DWORD,
                                     (PBYTE) (&dwTempState),
                                     sizeof(DWORD));

            if(dwRetVal !=ERROR_SUCCESS)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable To change the state field in the Nled registry"));

                dwRetVal = TPR_FAIL;
                goto cleanup;
            }

            if(LED_BLINK == dwTempState)
            {
                ledSupportsInfo_Get.LedNum = pEventDetails_list[dwCount].dwLedNum;

                //Getting the supports Info
                dwRetVal = NLedGetDeviceInfo(NLED_SUPPORTS_INFO_ID,
                                             &ledSupportsInfo_Get);

                if(!dwRetVal)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("\tGet device info failed. Last Error = %ld, LED = %u"),
                                 GetLastError(),
                                 ledSupportsInfo_Get.LedNum);

                    dwRetVal = TPR_FAIL;
                    goto cleanup;
                }

                //Check if the NLED supports blinking
                if(ledSupportsInfo_Get.lCycleAdjust > 0)
                {
                    //Setting the blinking parameters
                    if(ledSupportsInfo_Get.fAdjustTotalCycleTime)
                    {
                        dwTempValue = (ledSupportsInfo_Get.lCycleAdjust) * dwCount;
                        dwRetVal = RegSetValueEx (hEventKey[dwCount],
                                                  REG_EVENT_TOTAL_CYCLE_TIME_VALUE,
                                                  0,
                                                  REG_DWORD,
                                                  (PBYTE) (&dwTempValue),
                                                  sizeof(DWORD));

                        if(dwRetVal !=ERROR_SUCCESS)
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Unable To change the TotalCycleTime field in the Nled registry"));

                            dwRetVal = TPR_FAIL;
                            goto cleanup;
                        }
                    }//fAdjustTotalCycleTime

                    if(ledSupportsInfo_Get.fAdjustOnTime)
                    {
                        dwTempValue = (ledSupportsInfo_Get.lCycleAdjust) * dwCount;
                        dwRetVal = RegSetValueEx (hEventKey[dwCount],
                                                  REG_EVENT_ON_TIME_VALUE,
                                                  0,
                                                  REG_DWORD,
                                                  (PBYTE) (&dwTempValue),
                                                  sizeof(DWORD));

                        if(dwRetVal !=ERROR_SUCCESS)
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Unable To change the OnTime field in the Nled registry"));

                            dwRetVal = TPR_FAIL;
                            goto cleanup;
                        }

                    }//fAdjustOnTime

                    if(ledSupportsInfo_Get.fAdjustOffTime)
                    {
                        dwTempValue = (ledSupportsInfo_Get.lCycleAdjust) * dwCount;
                        dwRetVal = RegSetValueEx (hEventKey[dwCount],
                                                  REG_EVENT_OFF_TIME_VALUE,
                                                  0,
                                                  REG_DWORD,
                                                  (PBYTE) (&dwTempValue),
                                                  sizeof(DWORD));

                        if(dwRetVal !=ERROR_SUCCESS)
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Unable To change the OffTime field in the Nled registry"));

                            dwRetVal = TPR_FAIL;
                            goto cleanup;
                        }
                    }//fAdjustOffTime

                    if(ledSupportsInfo_Get.fMetaCycleOn)
                    {
                        dwTempValue = (META_CYCLE_ON) * dwCount;
                        dwRetVal = RegSetValueEx (hEventKey[dwCount],
                                                  REG_EVENT_META_CYCLE_ON_VALUE,
                                                  0,
                                                  REG_DWORD,
                                                  (PBYTE) (&dwTempValue),
                                                  sizeof(DWORD));

                        if(dwRetVal !=ERROR_SUCCESS)
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Unable To change the MetaCycleOn field in the Nled registry"));

                            dwRetVal = TPR_FAIL;
                            goto cleanup;
                        }
                    }//fMetaCycleOn

                    if(ledSupportsInfo_Get.fMetaCycleOff)
                    {
                        dwTempValue = (META_CYCLE_OFF) * dwCount;
                        dwRetVal = RegSetValueEx (hEventKey[dwCount],
                                                  REG_EVENT_META_CYCLE_OFF_VALUE,
                                                  0,
                                                  REG_DWORD,
                                                  (PBYTE) (&dwTempValue),
                                                  sizeof(DWORD));

                        if(dwRetVal !=ERROR_SUCCESS)
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Unable To change the MetaCycleOff field in the Nled registry"));

                            dwRetVal = TPR_FAIL;
                            goto cleanup;
                        }
                    }//fMetaCycleOff
                }
            }
        }
    }

    LPWSTR lpServiceName = L"nledsrv.dll";

    //Check if the Service is Still Running
    dwRetVal = IsServiceRunning(lpServiceName);

    if(!dwRetVal)
    {
        FAIL("Nled Service fails to handle Continuous Registry Changes for all the events. Test failed");
        dwRetVal = TPR_FAIL;
    }
    else
    {
        dwRetVal = TPR_PASS;
        SUCCESS("Continous registry changes does not affect the Nled service. Test Passed");
    }

cleanup:

    if(fCreatedTemporaryRegistryKey)
    {
        for(DWORD j = 0; j < dwEventDetailsCount; j++)
        {
            //Re-set the operator
            if(dwDeleteOperator[j] == 1)
            {
                RegDeleteValue(hEventKey[j], REG_EVENT_OPERATOR_VALUE);
            }
            else
            {
                dwReturn = RegSetValueEx (hEventKey[j],
                                          REG_EVENT_OPERATOR_VALUE,
                                          0,
                                          REG_DWORD,
                                          (PBYTE) (&pEventDetails_list[j].dwOperator),
                                          sizeof(DWORD));

                if(dwReturn != ERROR_SUCCESS)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Error in re-storing the original value of operator"));

                    dwRetVal = TPR_FAIL;
                }
            }

            //Re-set the TotalCycleTime
            if(dwDeleteTotalCycleTime[j] == 1)
            {
                RegDeleteValue(hEventKey[j],
                               REG_EVENT_TOTAL_CYCLE_TIME_VALUE);
            }
            else
            {
                dwReturn = RegSetValueEx (hEventKey[j],
                                          REG_EVENT_TOTAL_CYCLE_TIME_VALUE,
                                          0,
                                          REG_DWORD,
                                          (PBYTE) (&pEventDetails_list[j].dwTotalCycleTime),
                                          sizeof(DWORD));

                if(dwReturn != ERROR_SUCCESS)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Error in re-storing the original value of TotalCycleTime"));

                    dwRetVal = TPR_FAIL;
                }
            }

            //Re-set the OnTime
            if(dwDeleteOnTime[j] == 1)
            {
                RegDeleteValue(hEventKey[j], REG_EVENT_ON_TIME_VALUE);
            }
            else
            {
                dwReturn = RegSetValueEx (hEventKey[j],
                                          REG_EVENT_ON_TIME_VALUE,
                                          0,
                                          REG_DWORD,
                                          (PBYTE) (&pEventDetails_list[j].dwOnTime),
                                          sizeof(DWORD));

                if(dwReturn != ERROR_SUCCESS)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Error in re-storing the original value of OnTime"));

                    dwRetVal = TPR_FAIL;
                }
            }

            //Re-set the OffTime
            if(dwDeleteOffTime[j] == 1)
            {
                RegDeleteValue(hEventKey[j], REG_EVENT_OFF_TIME_VALUE);
            }
            else
            {
                dwReturn = RegSetValueEx (hEventKey[j],
                                          REG_EVENT_OFF_TIME_VALUE,
                                          0,
                                          REG_DWORD,
                                          (PBYTE) (&pEventDetails_list[j].dwOffTime),
                                          sizeof(DWORD));

                if(dwReturn != ERROR_SUCCESS)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Error in re-storing the original value of OffTime"));

                    dwRetVal = TPR_FAIL;
                }
            }

            //Re-set the MetaCycleOn
            if(dwDeleteMetaCycleOn[j] == 1)
            {
                RegDeleteValue(hEventKey[j], REG_EVENT_META_CYCLE_ON_VALUE);
            }
            else
            {
                dwReturn = RegSetValueEx (hEventKey[j],
                                          REG_EVENT_META_CYCLE_ON_VALUE,
                                          0,
                                          REG_DWORD,
                                          (PBYTE) (&pEventDetails_list[j].dwMetaCycleOn),
                                          sizeof(DWORD));

                if(dwReturn != ERROR_SUCCESS)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Error in re-storing the original value of MetaCycleOn"));

                    dwRetVal = TPR_FAIL;
                }
            }

            //Re-set the MetaCycleOff
            if(dwDeleteMetaCycleOff[j] == 1)
            {
                RegDeleteValue(hEventKey[j], REG_EVENT_META_CYCLE_OFF_VALUE);
            }
            else
            {
                dwReturn = RegSetValueEx (hEventKey[j],
                                          REG_EVENT_META_CYCLE_OFF_VALUE,
                                          0,
                                          REG_DWORD,
                                          (PBYTE) (&pEventDetails_list[j].dwMetaCycleOff),
                                          sizeof(DWORD));

                if(dwReturn != ERROR_SUCCESS)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Error in re-storing the original value of MetaCycleOff"));

                    dwRetVal = TPR_FAIL;
                }
            }

            dwReturn = RegSetValueEx (hEventKey[j],
                                      REG_EVENT_STATE_VALUE,
                                      0,
                                      REG_DWORD,
                                      (PBYTE) (&pEventDetails_list[j].dwState),
                                      sizeof(DWORD));

            if(dwReturn != ERROR_SUCCESS)
            {
                g_pKato->Log(LOG_FAIL,
                             TEXT("Error in setting the original value of state"));

                dwRetVal = TPR_FAIL;
            }

            dwReturn = RegSetValueEx (hEventKey[j],
                                      REG_EVENT_VALUE_VALUE,
                                      0,
                                      REG_DWORD,
                                      (PBYTE) (&pEventDetails_list[j].dwValue),
                                      sizeof(DWORD));

            if(dwReturn != ERROR_SUCCESS)
            {
                g_pKato->Log(LOG_FAIL,
                             TEXT("Error in setting the original value of triggerValue"));

                dwRetVal = TPR_FAIL;
            }

            dwValueLen = (wcslen((WCHAR *)(pEventDetails_list[j].bpValueName)) + 1) * sizeof(WCHAR);
            dwReturn = RegSetValueEx (hEventKey[j],
                                      REG_EVENT_VALUE_NAME_VALUE,
                                      0,
                                      REG_SZ,
                                      pEventDetails_list[j].bpValueName,
                                      dwValueLen);

            if(dwReturn != ERROR_SUCCESS)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Error in restoring the original Value name of the LED"));

                dwRetVal = FALSE;
            }

            //Set the new path value
            dwPathLen = (wcslen((WCHAR *)(pEventDetails_list[j].bpPath)) + 1) * sizeof(WCHAR);
            dwReturn = RegSetValueEx (hEventKey[j],
                                      REG_EVENT_PATH_VALUE,
                                      0,
                                      REG_SZ,
                                      pEventDetails_list[j].bpPath,
                                      dwPathLen);

            if(dwReturn != ERROR_SUCCESS)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Error in restoring the original path of the LED"));

                dwRetVal = FALSE;
            }

            //De-Allocating the memory
            GetStringFromReg_FreeMem(&(pEventDetails_list[j].bpValueName));
            GetStringFromReg_FreeMem(&(pEventDetails_list[j].bpPath));

            //Close the Event Key.
            if(hEventKey[j] != NULL)
            RegCloseKey(hEventKey[j]);
        }

        if(hTempoEventKey != NULL)
        {
            dwReturn = RegCloseKey(hTempoEventKey);

            if(dwReturn!=ERROR_SUCCESS)
            {
                //Log error message
                g_pKato->Log (LOG_DETAIL,
                              TEXT("Temporary event Registry Close Failed"));

                dwRetVal = TPR_FAIL;
            }

            dwReturn = RegDeleteKey(HKEY_LOCAL_MACHINE, TEMPO_EVENT_PATH);

            if(dwReturn != ERROR_SUCCESS)
            {
                //Log error message
                g_pKato->Log (LOG_DETAIL,
                              TEXT("Temporary event Registry Deletion Failed"));

                dwRetVal = TPR_FAIL;
            }
        }

        //De-allocating the memory
        if(dwDeleteOperator != NULL)
        {
            delete[] dwDeleteOperator;
            dwDeleteOperator = NULL;
        }

        if(dwDeleteTotalCycleTime != NULL)
        {
            delete[] dwDeleteTotalCycleTime;
            dwDeleteTotalCycleTime = NULL;
        }

        if(dwDeleteOnTime != NULL)
        {
            delete[] dwDeleteOnTime;
            dwDeleteOnTime = NULL;
        }

        if(dwDeleteOffTime != NULL)
        {
            delete[] dwDeleteOffTime;
            dwDeleteOffTime = NULL;
        }

        if(dwDeleteMetaCycleOn != NULL)
        {
            delete[] dwDeleteMetaCycleOn;
            dwDeleteMetaCycleOn = NULL;
        }

        if(dwDeleteMetaCycleOff != NULL)
        {
            delete[] dwDeleteMetaCycleOff;
            dwDeleteMetaCycleOff = NULL;
        }

        if(pEventDetails_list != NULL)
        {
            delete[] pEventDetails_list;
            pEventDetails_list = NULL;
        }

        if(hEventKey != NULL)
        {
            delete[] hEventKey;
            hEventKey = NULL;
        }
    }

    //Restore back the original Event Paths
    dwReturn = RestoreAllNledServiceEventPaths(pEventDetailsList,
                                               hEventOrigKey,
                                               dwTotalEvents);

    if(!dwReturn)
    {
        dwRetVal = TPR_FAIL;
        FAIL("Unable to restore back the original event paths");
    }

    //Delete the temporary registry key
    if(hTemporaryEventKey != NULL)
    {
        dwReturn =  RegCloseKey(hTemporaryEventKey);

        if(dwReturn !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Closing Failed. Last Error = %ld"),
                         GetLastError());

            dwRetVal = TPR_FAIL;
        }

        dwReturn = RegDeleteKey(HKEY_LOCAL_MACHINE, TEMPORARY_EVENT_PATH);

        if(dwReturn !=ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("\tTemp Registry Deletion Failed. Last Error = %ld"),
                         GetLastError());

            dwRetVal = TPR_FAIL;
        }
    }

    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo != NULL)
    {
        fStatus =  setAllLedSettings(pOriginalLedSettingsInfo);

        if(!fStatus)
        {
            dwRetVal = TPR_FAIL;
            FAIL("Unable to restore original NLED settings");
        }

        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    //Close the registry keys and return
    if(hNLEDConfigKey != NULL)
    {
        RegCloseKey(hNLEDConfigKey);
    }

    if(hLEDKey != NULL)
    {
        RegCloseKey(hLEDKey);
    }

    if(pLedNumEventId != NULL)
    {
        delete[] pLedNumEventId;
        pLedNumEventId = NULL;
    }

    if(hEventOrigKey != NULL)
    {
        delete[] hEventOrigKey;
        hEventOrigKey = NULL;
    }

    if(pEventDetailsList != NULL)
    {
        delete[] pEventDetailsList;
        pEventDetailsList = NULL;
    }

    return dwRetVal;
}

//*****************************************************************************
BOOL fireEventsForRobustnessTests()
//
//  Private function
//  Parameters: None
//
//  Return Value:TRUE if function executed succesfully. FALSE otherwise.
//
//  Objective:
//  Function to set the correct trigger value to the temporary path
//  trigger- valuename variable. This will trigger all events whose path is
//  given as temporary path.
//*****************************************************************************
{
    //Declaring the variables
    int nRetVal = INVALID_REG_KEY_VALUE;
    HKEY hTempoEventKey = NULL;
    DWORD dwTrigger = 0;

    nRetVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                           TEMPO_EVENT_PATH,
                           0,
                           KEY_ALL_ACCESS,
                           &hTempoEventKey);

    if(nRetVal !=ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to open the Temporary event path in registry"));

        return FALSE;
    }

    //When we set the following value, all events will be triggered simultaneously
    nRetVal = RegSetValueEx (hTempoEventKey,
                             REG_TRIGGER_VALUENAME,
                             0,
                             REG_DWORD,
                             (PBYTE)(&dwTrigger),
                             sizeof(DWORD));

    if(nRetVal !=ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL, TEXT("Unable to raise the trigger from test"));
        RegCloseKey(hTempoEventKey);
        return FALSE;
    }

    RegCloseKey(hTempoEventKey);
    return TRUE;
}

//*****************************************************************************
BOOL ResetEventsForRobustnessTests()
//
//  Return Value:TRUE if function executed succesfully. FALSE otherwise.
//
//  Objective:
//  Function to set incorrect trigger value to the temporary path
//  trigger-valuename variable. This will unset all those events from being active,
//  whose path is given as temporary path and trigger value is the value set.
//*****************************************************************************
{
    //Declaring the variables
    int nRetVal = INVALID_REG_KEY_VALUE;
    HKEY hTempoEventKey = NULL;
    LONG lTrigger = INVALID_REG_KEY_VALUE;

    nRetVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                           TEMPO_EVENT_PATH,
                           0,
                           KEY_ALL_ACCESS,
                           &hTempoEventKey);

    if(nRetVal !=ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to open the Temporary event path in registry"));

        return FALSE;
    }

    //Invalid value to reset trigger
    nRetVal = RegSetValueEx (hTempoEventKey,
                             REG_TRIGGER_VALUENAME,
                             0,
                             REG_DWORD,
                             (PBYTE)(&lTrigger),
                             sizeof(DWORD));

    if(nRetVal !=ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL, TEXT("Unable to raise the trigger from test"));
        RegCloseKey(hTempoEventKey);
        return FALSE;
    }

    RegCloseKey(hTempoEventKey);
    return TRUE;
}

//*****************************************************************************
BOOL RestoreAllNledServiceEventPaths(NLED_EVENT_DETAILS *pEventDetailsStruct,
                                     HKEY* hEventKey,
                                     DWORD dwTotalEvents)
//
//  Return Value:TRUE if restoring of original event paths for all the events is
//  successful and returns false otherwise.
//
//  Objective: To restore the original paths of all the evets.
//
//*****************************************************************************
{
    //Declaring the variables
    BOOL fReturn = TRUE;
    DWORD dwRetVal = 0;

    if(pEventDetailsStruct != NULL)
    {
        //Restoring back the original path for all the events
        for(DWORD i = 0; i < dwTotalEvents; i++)
        {
            if(pEventDetailsStruct[i].bpPath != NULL)
            {
                //Replace event path with original path
                DWORD dwTempEventPathLen = (wcslen((WCHAR *)(pEventDetailsStruct[i].bpPath))+1)*sizeof(WCHAR);

                if(hEventKey != NULL)
                {
                    dwRetVal = RegSetValueEx (hEventKey[i],
                                              REG_EVENT_PATH_VALUE,
                                              0,
                                              REG_SZ,
                                              (PBYTE)pEventDetailsStruct[i].bpPath,
                                              dwTempEventPathLen);

                    if(dwRetVal !=ERROR_SUCCESS)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Unable To change path for %u Nled Event."),
                                     pEventDetailsStruct[i].dwLedNum);

                        fReturn = FALSE;
                    }
                }

                if(pEventDetailsStruct[i].bpPath != NULL)
                {
                    if(!GetStringFromReg_FreeMem(&(pEventDetailsStruct[i].bpPath)))
                    {
                        fReturn = FALSE;
                    }

                    pEventDetailsStruct[i].bpPath = NULL;
                }

                //Closing the event key
                RegCloseKey(hEventKey[i]);
            }
        }

        //Unload and Load Service for affecting updated Registry values
        dwRetVal = UnloadLoadService();

        if(!dwRetVal)
        {
            FAIL("Error in re-loading the service");
            fReturn = FALSE;
        }

        //Waiting for 1 Second
        Sleep(SLEEP_FOR_ONE_SECOND);
    }

    return fReturn;
}

//*****************************************************************************
BOOL UnSetAllNledServiceEvents(NLED_EVENT_DETAILS* pnledEventDetailsStruct,
                               HKEY* hTemporaryEventKey,
                               HKEY* hEventKey)
//
//  Return Value:TRUE if all the events are unset and returns false otherwise
//
//  Objective: To unset all the Nled Service Events.
//
//*****************************************************************************
{
    //Variable Declarations
    HKEY hNLEDConfigKey = NULL;
    HKEY hLEDKey =  NULL;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLEDNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwCurLEDIndex = 0;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwCurLEDEventIndex = 0;
    DWORD dwEventNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwEventDetailsCount = 0;

    //Create the Temporary Trigger event
    TCHAR szTempoEventString[255] = TEMPORARY_EVENT_PATH;
    DWORD dwDisp = 0;
    DWORD dwRetVal = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                                    szTempoEventString,
                                    0,
                                    NULL,
                                    REG_OPTION_VOLATILE,
                                    KEY_ALL_ACCESS,
                                    NULL,
                                    hTemporaryEventKey, &dwDisp);

    if(dwRetVal != ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_FAIL,
                     TEXT("Unable to create a temporary event in registry"));

        return FALSE;
    }

    //Opening Config Key
    dwRetVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                            REG_NLED_KEY_CONFIG_PATH,
                            0,
                            0,
                            &hNLEDConfigKey);

    if(dwRetVal != ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to open the Config directory for LEDs."));

        return FALSE;
    }

    //Enumerating LEDs
    dwRetVal = RegEnumKeyEx(hNLEDConfigKey,
                            dwCurLEDIndex,
                            szLedName,
                            &dwLEDNameSize,
                            NULL,
                            NULL,
                            NULL,
                            NULL);

    if(dwRetVal != ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL, TEXT("No NLEDs configured in the registry."));
        return FALSE;
    }

    //Iteration for All Events in the LED.
    while(ERROR_SUCCESS == dwRetVal)
    {
        //Re-Initializing the event index
        dwCurLEDEventIndex = 0;

        //Opening Led Key
        dwRetVal = RegOpenKeyEx(hNLEDConfigKey,
                                szLedName,
                                0,
                                KEY_ALL_ACCESS,
                                &hLEDKey);

        if(dwRetVal != ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to open the LED registry for LED name %s"),
                         szLedName);

            return FALSE;
        }

        //Enumerating for Events
        dwRetVal = RegEnumKeyEx(hLEDKey,
                                dwCurLEDEventIndex,
                                szEventName,
                                &dwEventNameSize,
                                NULL,
                                NULL,
                                NULL,
                                NULL);

        if((dwRetVal != ERROR_SUCCESS) && (ERROR_NO_MORE_ITEMS == dwRetVal))
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("No events are configured for %s Nled"),
                         szLedName);
        }

        //Iteration for All Events in the LED.
        while(ERROR_SUCCESS == dwRetVal)
        {
            //Opening Event Key
            dwRetVal = RegOpenKeyEx(hLEDKey,
                                    szEventName,
                                    0,
                                    KEY_ALL_ACCESS,
                                    &hEventKey[dwEventDetailsCount]);

            if(dwRetVal != ERROR_SUCCESS)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to open the LED events registry for %s"),
                             szEventName);

                return FALSE;
            }

            //Preserving event path details
            if(!GetStringFromReg(hEventKey[dwEventDetailsCount],
                                 REG_EVENT_PATH_VALUE,
                                 &(pnledEventDetailsStruct[dwEventDetailsCount].bpPath)))
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Path detail not available in registry for %s Event"),
                             szEventName);

                return FALSE;
            }

            //Replace path with temporary trigger path
            DWORD dwTempEventPathLen = (wcslen((WCHAR *)(szTempoEventString))+1)*sizeof(WCHAR);
            dwRetVal = RegSetValueEx (hEventKey[dwEventDetailsCount],
                                      REG_EVENT_PATH_VALUE,
                                      0,
                                      REG_SZ,
                                      (PBYTE) szTempoEventString,
                                      dwTempEventPathLen);

            if(dwRetVal !=ERROR_SUCCESS)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to change path for %s."),
                             szEventName);

                return FALSE;
            }

            //Incrementing array counter after each event
            dwEventDetailsCount++;

            //Next Event in the LED
            dwCurLEDEventIndex++;
            dwEventNameSize = gc_dwMaxKeyNameLength + 1;

            dwRetVal = RegEnumKeyEx(hLEDKey,
                                    dwCurLEDEventIndex,
                                    szEventName,
                                    &dwEventNameSize,
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
        }//Iteration for All Events in the LED.

        //Close the present Led Key
        RegCloseKey(hLEDKey);

        //Enumerating for Next LED.
        dwCurLEDIndex++;
        dwLEDNameSize = gc_dwMaxKeyNameLength + 1;

        dwRetVal = RegEnumKeyEx(hNLEDConfigKey,
                                dwCurLEDIndex,
                                szLedName,
                                &dwLEDNameSize,
                                NULL,
                                NULL,
                                NULL,
                                NULL);
    }

    //Unload and Load Service for affecting updated Registry values
    dwRetVal = UnloadLoadService();

    if(!dwRetVal)
    {
        FAIL("Error in re-loading the service");
        return FALSE;
    }

    return TRUE;
}