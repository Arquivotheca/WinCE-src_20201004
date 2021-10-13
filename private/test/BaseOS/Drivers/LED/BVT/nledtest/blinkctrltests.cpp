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

#include "globals.h"
#include "debug.h"

#define __FILE_NAME__ TEXT("blinkCtrlTests.cpp")

//*****************************************************************************
TESTPROCAPI VerifyBlinkCtrlLibNledBlinking(UINT                   uMsg,
                                           TPPARAM                tpParam,
                                           LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
//  Checking that BlinkCtrl Library succeeds in blinking a Nled 
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Only Supporting executing the test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the PQD or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fPQDTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    // If there are no NLEDs on the device pass the test.
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("There are no NLEDs to Test on the Device. Automatic PASS"));

        return TPR_PASS;
    }

    //Pointer to an array to store the original led settings
    NLED_SETTINGS_INFO *pOriginalLedSettingsInfo = NULL;

    //Declaring the variables
    BOOL fStatus = FALSE;
    DWORD dwNledSWBlinkCtrlValue = 0;
    DWORD dwReturnValue = TPR_FAIL;

    //Storing the original LED settings
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
    fStatus = saveAllLedSettings(pOriginalLedSettingsInfo, LED_OFF);

    if(!fStatus)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        FAIL("Unable to save original settings for all LEDs");
        return TPR_FAIL;
    }

    //Checking for the first LED that has Blink Control library configured
    for(DWORD dwLedCount = 0; dwLedCount < GetLedCount(); dwLedCount++)
    {
        //Checking if the NLED has BlinkCtrl Lib configured
        dwNledSWBlinkCtrlValue = GetSWBlinkCtrlLibraryNLed(dwLedCount);

        if(TPR_SKIP == dwNledSWBlinkCtrlValue)
        {
            //Logging the error output using the kato logging engine
            g_pKato->Log(LOG_SKIP,
                         TEXT("Blink Control Library is not configured for any of the NLEDs on the device"));

            dwReturnValue = TPR_SKIP;
            goto RETURN;
        }
        else if(TPR_PASS == dwNledSWBlinkCtrlValue)
        {
            //Blink the led
            if(TPR_PASS == BlinkNled(dwLedCount))
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Blink Control Library: SetLED successful for NLED %u"), 
                             dwLedCount);

                SUCCESS("SetLED successful for Blink control library");
                dwReturnValue = TPR_PASS;
                goto RETURN;
            }
            else
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Blink Control Library: SetLED failed for NLED %u"), 
                             dwLedCount);

                dwReturnValue = TPR_FAIL;
                goto RETURN;
            }
        }
        else
        {
            //Logging the error output using the kato logging engine
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Blink Control Library is not configured for NLED No. %u"),
                         dwLedCount);
        }
    }

RETURN:

    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo != NULL)
    {
        fStatus = setAllLedSettings(pOriginalLedSettingsInfo);

        if(!fStatus)
        {
            dwReturnValue = TPR_FAIL;
            FAIL("Unable to restore original NLED settings");
        }

        //De-Allocating the memory
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    if(TPR_FAIL == dwReturnValue)
    {
        FAIL("SetLED for blink control library failed");
        return TPR_FAIL;
    }

    return dwReturnValue;
}

//*****************************************************************************
TESTPROCAPI VerifyBlinkctrlLibraryRegistrySettings(UINT                   uMsg,
                                                   TPPARAM                tpParam,
                                                   LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
//  Objective:
//
//  "Checking if the SWBlinkCtrl flag is present for any NLED then it has valid
//   values(i.e. '0' or '1')."
//
//   "Checking that for any NLED if SWBlinkCtrl is set to '1' then BLINKABLE is
//   set to '0'."
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Only Supporting executing the test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the PQD or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fPQDTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    // If there are no NLEDs on the device pass the test.
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("There are no NLEDs to Test on the Device. Automatic PASS"));

        return TPR_PASS;
    }

    //Declaring the variables
    DWORD dwConfigKeyResult = 0;
    HKEY hConfigKey = NULL;
    HKEY hLedNameKey = NULL;
    DWORD dwLedNum = 0;
    DWORD dwBlinkable = 0;
    DWORD dwSWBlinkCtrl =0;
    DWORD dwCountKey = 0;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLedNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwLen = sizeof(DWORD);
    DWORD dwReturnValue = TPR_PASS;
    DWORD dwReturn = 0;
    DWORD dwNoSWBlinkCtrl = 0;

    //Opening the key
    dwReturn = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                            REG_NLED_KEY_CONFIG_PATH,
                            0,
                            KEY_ENUMERATE_SUB_KEYS,
                            &hConfigKey);

    if(ERROR_SUCCESS == dwReturn)
    {
        dwConfigKeyResult=RegEnumKeyEx(hConfigKey,
                                       dwCountKey,
                                       szLedName,
                                       &dwLedNameSize,
                                       NULL,
                                       NULL, 
                                       NULL, 
                                       NULL);
 
        if(ERROR_NO_MORE_ITEMS == dwConfigKeyResult)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("No NLEDs configured in the registry"));

            dwReturnValue = TPR_PASS;
            goto CLEANUP;
        }

        while(ERROR_NO_MORE_ITEMS != dwConfigKeyResult)
        {
            //Querying the LED parameters
            dwReturn = RegOpenKeyEx(hConfigKey, 
                                    szLedName,
                                    0,
                                    KEY_ALL_ACCESS,
                                    &hLedNameKey);

            if(ERROR_SUCCESS == dwReturn)
            {
                //Querying the key values
                //Querying the LedNum key value
                dwReturn = RegQueryValueEx(hLedNameKey, 
                                           REG_LED_NUM_VALUE, 
                                           NULL, 
                                           NULL,
                                           (LPBYTE) &dwLedNum,
                                           &dwLen);

                if(ERROR_SUCCESS != dwReturn)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to query the LedNum key value for %s"),
                                 szLedName);

                    dwReturn = TPR_FAIL;
                    goto CLEANUP;
                }

                //Querying the Blinkable key value
                dwReturn = RegQueryValueEx(hLedNameKey, 
                                           REG_BLINKABLE_VALUE, 
                                           NULL, 
                                           NULL,
                                           (LPBYTE) &dwBlinkable,
                                           &dwLen);

                if(ERROR_SUCCESS != dwReturn)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to query the Blinkable key value for %s"), 
                                 szLedName);

                    dwReturn = TPR_FAIL;
                    goto CLEANUP;
                }

                //Querying the SWBlinkCtrl flag value
                dwReturn = RegQueryValueEx(hLedNameKey,
                                           REG_SWBLINKCTRL_VALUE, 
                                           NULL,
                                           NULL,
                                           (LPBYTE) &dwSWBlinkCtrl,
                                           &dwLen);

                if(ERROR_SUCCESS == dwReturn)
                {
                    if(ZERO_SWBLINKCTRL == dwSWBlinkCtrl)
                    {
                        dwNoSWBlinkCtrl++;
                    }

                    //Checking for valid SWBlinkCtrl flag
                    if(!((VALID_SWBLINKCTRL == dwSWBlinkCtrl) || 
                         (ZERO_SWBLINKCTRL == dwSWBlinkCtrl)))

                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Invalid SWBlinkCtrl flag for NLED No. %u"), 
                                     dwLedNum);

                        dwReturnValue = TPR_FAIL;
                        goto CLEANUP;
                    }

                     //Checking that when SWBlinkCtrl flag is set to 1 then Blinkable is set to 0
                    if(VALID_SWBLINKCTRL == dwSWBlinkCtrl)
                    {
                        if(ZERO_BLINKABLE != dwBlinkable)
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Invalid Blinkable set for NLED No. %u Which has SWBlinkCtrl set to 1"), 
                                         dwLedNum);

                            dwReturnValue = TPR_FAIL;
                            goto CLEANUP;
                        }
                    }
                }
                else if(ERROR_FILE_NOT_FOUND ==  dwReturn)
                {
                    dwNoSWBlinkCtrl++;
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to query the SWBlinkCtrl value for NLED No. %u"), 
                                 dwLedNum);

                    dwReturnValue = TPR_FAIL;
                    goto CLEANUP;
                }
            }
            else
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Failed to open the %s key"), 
                             szLedName);

                dwReturnValue = TPR_FAIL;
                goto CLEANUP;
            }

            //Closing the handle
            if(hLedNameKey != NULL)
            {
                RegCloseKey(hLedNameKey);
            }

            //Incrementing the count
            dwCountKey++;

            //Resetting the buffer size
            dwLedNameSize=gc_dwMaxKeyNameLength + 1;

            //Enumerating for other Led keys
            dwConfigKeyResult=RegEnumKeyEx(hConfigKey, 
                                           dwCountKey, 
                                           szLedName,
                                           &dwLedNameSize, 
                                           NULL,
                                           NULL,
                                           NULL, 
                                           NULL);
        }
    }
    else
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Failed to open NLED-LedName keys for Enumeration"));

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

        if(dwReturnValue == TPR_PASS)
        {
            if(dwNoSWBlinkCtrl == dwCountKey)
            {
                g_pKato->Log(LOG_SKIP,
                             TEXT("Blink Control Library is not configured for any of the NLEDs on the device"));

                return TPR_SKIP;
            }

            SUCCESS("BlinkControl Library is configured correctly");
            return TPR_PASS;
        }
        else
        {
            FAIL("BlinkControl Library is not configured correctly");
            return TPR_FAIL;
        }
}

//*****************************************************************************
TESTPROCAPI BlinkCtrlLibWithBlinkOffBlinkTest(UINT                   uMsg,
                                              TPPARAM                tpParam,
                                              LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
//  "Set all LED to BLINK state. Then turn the LEDs to OFF one by one. Let
//  only one of the LEDs in the blinking state. Last LED must continue to Blink."
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Only Supporting executing the test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the PQD or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fPQDTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    // If there are no NLEDs on the device pass the test.
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("There are no NLEDs to Test on the Device. Automatic PASS"));

        return TPR_PASS;
    }

    //Declaring the variables
    DWORD dwReturnValue = TPR_FAIL;
    DWORD dwNledSWBlinkCtrlValue = 0;
    DWORD dwLastSWBlinkCtrlNledNum = 0;

    //Variable to store the count of Nleds that have the SWBlinkCtrl set to 1
    DWORD dwSWBlinkCtrlNleds = 0;

    //Declaring the structures
    NLED_SETTINGS_INFO ledSettingsInfo_Set = {0};
    NLED_SETTINGS_INFO ledSettingsInfo_Get = {0};

    //Pointer to an array to store the original led settings
    NLED_SETTINGS_INFO *pOriginalLedSettingsInfo = NULL;

    BOOL fStatus = FALSE;

    //Checking if the Blink Control library is configured for any LED
    dwNledSWBlinkCtrlValue = GetSWBlinkCtrlLibraryNLed(LED_INDEX_FIRST);

    if(dwNledSWBlinkCtrlValue == TPR_SKIP)
    {
        //Logging the error output using the kato logging engine
        g_pKato->Log(LOG_SKIP,
                     TEXT("Blink Control Library is not configured for any of the NLEDs on the device"));

        return TPR_SKIP;
    }

    //Storing the original LED settings
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
    fStatus = saveAllLedSettings(pOriginalLedSettingsInfo, LED_OFF);

    if(!fStatus)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        FAIL("Unable to save original settings for all LEDs");
        return TPR_FAIL;
    }

    //Blinking all the NLEDs that have the SWBlinkCtrl field set to 1
    for(DWORD dwLedCount = 0; dwLedCount < GetLedCount(); dwLedCount++)
    {
        //Checking if this NLED has Blink Control Lib configured
        dwNledSWBlinkCtrlValue = GetSWBlinkCtrlLibraryNLed(dwLedCount);

        if(TPR_PASS == dwNledSWBlinkCtrlValue)
        {
            //Incrementing the count if this NLED has SWBlinkCtrl lib configured
            dwSWBlinkCtrlNleds++;

            //Setting the LED to blink
            dwReturnValue = BlinkNled(dwLedCount);

            if(TPR_PASS != dwReturnValue)
            {
                //Logging the output using the kato logging engine
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Blink Control library failed to blink the NLED %u"), 
                             dwLedCount);

                dwReturnValue = TPR_FAIL;
                goto CLEANUP;
            }

            dwLastSWBlinkCtrlNledNum = dwLedCount;
        }
        else
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Blink Control Library is not configured for NLED %u"), 
                         dwLedCount);
        }
    }

    //If only one nled has BlinkCtrl configured
    if(ONE_LED == dwSWBlinkCtrlNleds)
    {
        g_pKato->Log(LOG_SKIP,
                     TEXT("Only one Nled has BlinkCtrl Library configured. So skipping the BLINK/OFF/BLINK test"));

        dwReturnValue = TPR_SKIP;
        goto CLEANUP;
    }

    //Now turn off all the LEDS except the last one
    for(DWORD dwCount = 0; dwCount < (GetLedCount() - 1); dwCount++)
    {
        ledSettingsInfo_Set.LedNum = dwCount;
        ledSettingsInfo_Set.OffOnBlink = LED_OFF;

        //Not turning off the last Nled in the list that has SWBlinkCtrl configured
        if(dwLastSWBlinkCtrlNledNum != dwCount)
        {
            dwReturnValue = NLedSetDevice(NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Set);

            if(!dwReturnValue)
            {
                //Logging the output using the kato logging engine
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Nled Driver failed to turn off the %u Nled"), 
                             dwCount);

                dwReturnValue = TPR_FAIL;
                goto CLEANUP;
            }
        }
    }

    //Check if the last NLED is still blinking
    ledSettingsInfo_Get.LedNum = dwLastSWBlinkCtrlNledNum;
    dwReturnValue = NLedGetDeviceInfo(NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Get);

    if(!dwReturnValue)
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Nled Driver failed to get the Nled Settings Info"));

        dwReturnValue = TPR_FAIL;
        goto CLEANUP;
    }

    if(ledSettingsInfo_Get.OffOnBlink == LED_BLINK)
    {
        //Logging the output using the kato logging engine
        SUCCESS("Nled BlinkCtrl Lib is successfully blinking the last LED that has SWBlinkCtrl configured. BLINK/OFF/BLINK test passed");
        dwReturnValue = TPR_PASS;
    }
    else
    {
        //Logging the output using the kato logging engine
        FAIL("Nled BlinkCtrl Lib failed in blinking the last LED that has SWBlinkCtrl configured. BLINK/OFF/BLINK test failed");
        dwReturnValue = TPR_FAIL;
    }

CLEANUP:

    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo != NULL)
    {
        fStatus = setAllLedSettings(pOriginalLedSettingsInfo);

        if(!fStatus)
        dwReturnValue = TPR_FAIL;

        //De-allocating the memory
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    return dwReturnValue;
}

//*****************************************************************************
TESTPROCAPI VerifyBlinkCtrlLibValidNledBlinkTime(UINT                   uMsg,
                                                 TPPARAM                tpParam,
                                                 LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
//  "Checking if the Nled being blinked by the Blink Ctrl library is blinking
//  with correct timing"
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Only Supporting executing the test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the PQD and interactive tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fInteractive) || (!g_fPQDTests))
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Skipping the test since the user has not enabled the interactive(-i) and the PQD(-pqd) tests execution from the command line"));

        return TPR_SKIP;
    }

    // If there are no NLEDs on the device pass the test.
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("There are no NLEDs to Test on the Device. Automatic PASS"));

        return TPR_PASS;
    }    

    //Pointer to an array to store the original led settings
    NLED_SETTINGS_INFO *pOriginalLedSettingsInfo = NULL;

    //Structures to store the supports info
    NLED_SUPPORTS_INFO ledSupportsInfo = { 0 };
    TCHAR szMessage[ 100 ] = TEXT("");
    NLED_SETTINGS_INFO ledSettingsInfo_Set  = {0};

    //Declaring the variables
    DWORD dwReturnValue = TPR_FAIL;
    DWORD dwMboxValue = 0;
    BOOL fStatus = FALSE;
    DWORD dwReturn = 0;
    DWORD *pSWBlinkCtrlNledNums = NULL;

    //Checking if the Blink Control library is configured for any of the LEDs
    DWORD dwNledSWBlinkCtrlValue = GetSWBlinkCtrlLibraryNLed(LED_INDEX_FIRST);

    if(dwNledSWBlinkCtrlValue == TPR_SKIP)
    {
        //Logging the error output using the kato logging engine
        g_pKato->Log(LOG_SKIP,
                     TEXT("Blink Control Library is not configured for any of the NLEDs on the device"));

        return TPR_SKIP;
    }

    //Storing the original LED settings
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
    fStatus = saveAllLedSettings(pOriginalLedSettingsInfo, LED_OFF);

    if(!fStatus)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        FAIL("Unable to save original settings for all LEDs");
        return TPR_FAIL;
    }

    pSWBlinkCtrlNledNums = new DWORD[GetLedCount()];

    //Getting the Nleds having SWBlinkCtrl enabled
    dwReturn = GetSWBlinkCtrlNleds(pSWBlinkCtrlNledNums);

    if(dwReturn != TPR_PASS)
    {
        //Logging the error output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to Query the SWBlinkCtrl for all the Nleds"));

        dwReturnValue = TPR_FAIL;
        goto CLEANUP;
    }

    //Testing for the first NLED that has Blink Control library configured
    for(DWORD dwLedCount = 0; dwLedCount < GetLedCount(); dwLedCount++)
    {
        //Checking if the this NLED has SWBlinkCtrl flag set to 1
        if(pSWBlinkCtrlNledNums[dwLedCount] == VALID_SWBLINKCTRL)
        {
            // Get the supports info
            ledSupportsInfo.LedNum = dwLedCount;
            dwReturnValue = NLedGetDeviceInfo(NLED_SUPPORTS_INFO_ID, &ledSupportsInfo);

            if(!dwReturnValue)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to get the supports info for NLED %u"), 
                             dwLedCount);

                dwReturnValue = TPR_FAIL;
                goto CLEANUP;
            }

            //Blinking the Nled and asking the user to validate the blinking time
            ledSettingsInfo_Set.LedNum = dwLedCount;
            ledSettingsInfo_Set.OffOnBlink = LED_BLINK;

            swprintf_s(szMessage, _countof(szMessage),
                      TEXT("Note the Blinking Rate for the Nled %u"), dwLedCount);

            MessageBox(NULL,
                       szMessage,
                       g_szInputMsg,
                       MB_OK | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND);

            for(DWORD i = 0; i < MEASURE_CYCLETIME_THREE_TIMES; i++)
            {
                //Adjusting the blink settings
                if(ledSupportsInfo.fAdjustTotalCycleTime)
                {
                    ledSettingsInfo_Set.TotalCycleTime += (ledSupportsInfo.lCycleAdjust) * 2;
                } // fAdjustTotalCycleTime

                if(ledSupportsInfo.fAdjustOnTime)
                {
                    ledSettingsInfo_Set.OnTime += ledSupportsInfo.lCycleAdjust;
                } // fAdjustOnTime

                if(ledSupportsInfo.fAdjustOffTime)
                {
                    ledSettingsInfo_Set.OffTime += ledSupportsInfo.lCycleAdjust;
                } // fAdjustOffTime

                if(ledSupportsInfo.fMetaCycleOn)
                {
                    ledSettingsInfo_Set.MetaCycleOn = META_CYCLE_ON;
                }//fMetaCycleOn

                if(ledSupportsInfo.fMetaCycleOff)
                {
                    ledSettingsInfo_Set.MetaCycleOff = META_CYCLE_OFF;
                }//fMetaCycleOff

                fStatus = NLedSetDevice(NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Set);

                if(!fStatus)
                {
                    g_pKato->Log(LOG_DETAIL, 
                                 TEXT("\tBlinkControl Lib failed to blink the NLED No. %u"), 
                                 dwLedCount);
                    break;
                }

                 //Asking the user's input
                swprintf_s( szMessage, _countof(szMessage),
                          TEXT("Did the Total Blinking Cycle Time increase? Click Yes/No after observing"));

                dwMboxValue = MessageBox(
                                         NULL,
                                         szMessage,
                                         g_szInputMsg,
                                         MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND);

                if(IDNO == dwMboxValue)
                {
                    dwReturnValue = TPR_FAIL;
                    break;
                }
            }

            //Testing only for one NLED
            break;
        }
        else
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Blink Control Library is not configured for NLED %u"), 
                         dwLedCount);
        }
    }

    if(IDYES == dwMboxValue)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tNLED %u passed Adjust Blink Total CycleTime Test."),
                     dwLedCount);

        dwReturnValue = TPR_PASS;
    }
    else
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tNLED %u failed Adjust Blink Total CycleTime Test."),
                     dwLedCount);

        dwReturnValue = TPR_FAIL;
    }

CLEANUP:

    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo != NULL)
    {
        fStatus = setAllLedSettings(pOriginalLedSettingsInfo);

        if(!fStatus)
        dwReturnValue = TPR_FAIL;

        //De-allocating the memory
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    if(pSWBlinkCtrlNledNums != NULL)
    {
        delete[] pSWBlinkCtrlNledNums;
        pSWBlinkCtrlNledNums = NULL;
    }

    return dwReturnValue;
}

//*****************************************************************************
TESTPROCAPI BlinkCtrlLibSystemBehaviorTest(UINT                   uMsg,
                                           TPPARAM                tpParam,
                                           LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
//  "Continuously changing the values of the members of the struct 
//  NLED_SETTINGS_INFO and making the Nled/Nleds to Blink/ON/OFF and checking 
//  that BlinkCtrl Library handles this scenario gracefully."
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the PQD or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fPQDTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    //Passing the test if the device does not have any Nleds
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("The device doesn't have any NLEDs so passing the test"));

        return TPR_PASS;
    }

    //Declaring the variables
    DWORD dwTestResult = TPR_PASS;
    DWORD dwReturn = 0;
    DWORD *pSWBlinkCtrlNledNums = NULL;
    NLED_SETTINGS_INFO *pOriginalLedSettingsInfo = NULL;
    BOOL fStatus = FALSE;

    //Checking if the Blink Control library is configured for any LED
    DWORD dwNledSWBlinkCtrlValue = GetSWBlinkCtrlLibraryNLed(LED_INDEX_FIRST);

    if(dwNledSWBlinkCtrlValue == TPR_SKIP)
    {
        //Logging the error output using the kato logging engine
        g_pKato->Log(LOG_SKIP,
                     TEXT("Blink Control Library is not configured for any of the NLEDs on the device"));

        return TPR_SKIP;
    }

    // Allocate space to save origional LED setttings.
    SetLastError(ERROR_SUCCESS);
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];

    if(!pOriginalLedSettingsInfo)
    {
        FAIL("new failed");
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
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        g_pKato->Log(LOG_DETAIL, TEXT("Unable to save Nled Settings for all the Leds"));
        return TPR_FAIL;
    }

    //Clearing the previous value
    g_fMULTIPLENLEDSETFAILED = FALSE;

    //Clearing off the previous thread count
    g_nThreadCount = 0;

    //Allocating memory
    pSWBlinkCtrlNledNums = new DWORD[GetLedCount()];

    //Getting the Nleds having SWBlinkCtrl enabled
    dwReturn = GetSWBlinkCtrlNleds(pSWBlinkCtrlNledNums);

    if(dwReturn != TPR_PASS)
    {
        //Logging the error output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to Query the SWBlinkCtrl for all the Nleds"));

        dwTestResult = TPR_FAIL;
        goto RETURN;
    }

    //Testing for one LED
    for(DWORD dwLedCount = 0; dwLedCount < GetLedCount(); dwLedCount++)
    {
        //Clearing off the previous thread count
        g_nThreadCount = 0;

        if(VALID_SWBLINKCTRL == pSWBlinkCtrlNledNums[dwLedCount])
        {
            dwReturn = CheckSystemBehavior(dwLedCount, ONE_LED);

            if(TPR_PASS != dwReturn)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Failed to create the thread for invoking NledSetdevice"));

                dwTestResult = TPR_FAIL;

                //Waiting for all the threads to complete execution
                CheckThreadCompletion(g_hThreadHandle, MAX_THREAD_COUNT, MAX_THREAD_TIMEOUT_VAL);

                goto RETURN;
            }

            //Check if all the threads are done with the execution            
            dwReturn = CheckThreadCompletion(g_hThreadHandle, MAX_THREAD_COUNT, MAX_THREAD_TIMEOUT_VAL);

            if(TPR_FAIL == dwReturn)
            {
                dwTestResult = TPR_FAIL;
                goto RETURN;
            }

            //Testing the behavior
            if(g_fMULTIPLENLEDSETFAILED)
            {
                //Logging the error output using the kato logging engine
                FAIL("NLED driver fails to retrieve correct settings Info for simultaneous NledSetDevice calls");
                dwTestResult = TPR_FAIL;
            }
            else
            {
                //Logging the error output using the kato logging engine
                SUCCESS("NLED driver successfully retrieves settings Info for simultaneous NledSetDevice calls");
            }

            //Testing for one Nled only
            break;
        }
        else
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Blink Control Library is not configured for NLED %u"), dwLedCount);
        }
    }

    //Closing thread handles before moving to the next tests
    for (UINT nTCount = 0; nTCount < MAX_THREAD_COUNT; nTCount ++)
    {
        if(g_hThreadHandle[nTCount] != NULL)
        {
            fStatus = CloseHandle(g_hThreadHandle[nTCount]);
            g_hThreadHandle[nTCount] = NULL;

            if(!fStatus)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Failed to close the handle of the thread created for invoking NledSetDevice"));

                dwTestResult = TPR_FAIL;
                break;
            }
        }
    }

    //Testing for all the LEDs that have Blink Control library configured

    //Clearing the previous value
    g_fMULTIPLENLEDSETFAILED = FALSE;

    //Clearing off the previous thread count
    g_nThreadCount = 0;

    for(DWORD dwLedCounter = 0; dwLedCounter < GetLedCount(); dwLedCounter++)
    {
        //Checking if the this NLED has SWBlinkCtrl flag set to 1
        if(pSWBlinkCtrlNledNums[dwLedCounter] == VALID_SWBLINKCTRL)
        {
            dwReturn = CheckSystemBehavior(dwLedCounter, GetLedCount());

            if(TPR_PASS != dwReturn)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Failed to create the threads for invoking multiple NledSetDevice"));

                dwTestResult = TPR_FAIL;

                //Waiting for all the threads to complete execution
                CheckThreadCompletion(g_hThreadHandle, MAX_THREAD_COUNT, MAX_THREAD_TIMEOUT_VAL);

                goto RETURN;
            }
        }
        else
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Blink Control Library is not configured for NLED %u"),
                         dwLedCounter);
        }
    }

    //Check if all the threads are done with the execution
    dwReturn = CheckThreadCompletion(g_hThreadHandle, MAX_THREAD_COUNT, MAX_THREAD_TIMEOUT_VAL);

    if(TPR_FAIL == dwReturn)
    {
        dwTestResult = TPR_FAIL;
        goto RETURN;
    }

    //Testing the behavior
    if(g_fMULTIPLENLEDSETFAILED)
    {
        //Logging the error output using the kato logging engine
        FAIL("NLED driver fails to retrieve settings Info of all the Nleds for simultaneous NledSetDevice calls");
        dwTestResult = TPR_FAIL;
    }
    else
    {
        //Logging the output using the kato logging engine
        SUCCESS("NLED driver successfully retrieves settings Info of all the Nleds for simultaneous NledSetDevice calls");
    }

RETURN:

    //Closing the thread handles
    for (UINT nThCount = 0; nThCount < g_nThreadCount; nThCount++)
    {
        if(g_hThreadHandle[nThCount] != NULL)
        {
            fStatus = CloseHandle(g_hThreadHandle[nThCount]);
            g_hThreadHandle[nThCount] = NULL;

            if(!fStatus)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Failed to close the handle of the thread created for invoking NledSetDevice"));

                dwTestResult = TPR_FAIL;
                break;
            }
        }
    }

    //Restoring all the original led settings
    fStatus = setAllLedSettings(pOriginalLedSettingsInfo);

    if(!fStatus)
    dwTestResult = TPR_FAIL;

    //Releasing the memory allocated
    if(pOriginalLedSettingsInfo != NULL)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    //De-allocating the memory
    if(pSWBlinkCtrlNledNums != NULL)
    {
        delete[] pSWBlinkCtrlNledNums;
        pSWBlinkCtrlNledNums = NULL;
    }

    return dwTestResult;
}

//*****************************************************************************
TESTPROCAPI NledBlinkCtrlLibMemUtilizationTest(UINT                   uMsg,
                                               TPPARAM                tpParam,
                                               LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
//  This test checks whether the memory utilization is greater
//  than a given threshold value when all the Nleds having blink control library
//  configured are blinking.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    if((!g_fPQDTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("The device doesn't have any NLEDs so passing the test"));

        return TPR_PASS;
    }

    //Get the initial memory utilization
    DWORD dwOriginalMemoryLoad = 0;
    DWORD dwFinalMemoryLoad = 0;
    MEMORYSTATUS *lpBuffer  = (MEMORYSTATUS*)LocalAlloc(LMEM_ZEROINIT, sizeof (MEMORYSTATUS));

    //Blinking all the LEDs that have Blink Control Library configured
    //Declaring the variables
    DWORD *pSWBlinkCtrlNledNums = NULL;
    DWORD dwReturnValue = TPR_FAIL;
    DWORD dwNledSWBlinkCtrlValue = 0;

    //Pointer to an array to store the original led settings
    NLED_SETTINGS_INFO *pOriginalLedSettingsInfo = NULL;
    BOOL fStatus = FALSE;

    //Checking if the Blink Control library is configured for any LED
    dwNledSWBlinkCtrlValue = GetSWBlinkCtrlLibraryNLed(LED_INDEX_FIRST);

    if(dwNledSWBlinkCtrlValue == TPR_SKIP)
    {
        //Logging the error output using the kato logging engine
        g_pKato->Log(LOG_SKIP,
                     TEXT("Blink Control Library is not configured for any of the NLEDs on the device. Skipping the test"));

        //Releasing the memory
        if(lpBuffer)
        {
            LocalFree(lpBuffer);
            lpBuffer = NULL;
        }

        return TPR_PASS;
    }

    //Storing the original LED settings
    // Allocate space to save origional LED setttings.
    SetLastError(ERROR_SUCCESS);
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];

    if(!pOriginalLedSettingsInfo)
    {
        FAIL("Memory allocation failed");
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\tLast Error = %d"),
                     GetLastError());

        //Releasing the memory
        if(lpBuffer)
        {
            LocalFree(lpBuffer);
            lpBuffer = NULL;
        }

        return TPR_FAIL;
    }

    ZeroMemory(pOriginalLedSettingsInfo, GetLedCount() * sizeof(NLED_SETTINGS_INFO));

    // Save the Info Settings for all the LEDs and turn them off.
    fStatus = saveAllLedSettings(pOriginalLedSettingsInfo, LED_OFF);

    if(!fStatus)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        FAIL("Unable to save original settings for all LEDs");

        //Releasing the memory
        if(lpBuffer)
        {
            LocalFree(lpBuffer);
            lpBuffer = NULL;
        }

        return TPR_FAIL;
    }

    //Getting the Nleds having SWBlinkCtrl enabled
    pSWBlinkCtrlNledNums = new DWORD[GetLedCount()];
    dwReturnValue = GetSWBlinkCtrlNleds(pSWBlinkCtrlNledNums);

    if(dwReturnValue != TPR_PASS)
    {
        //Logging the error output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to Query the SWBlinkCtrl for all the Nleds"));

        dwReturnValue = TPR_FAIL;
        goto CLEANUP;
    }

    //Getting the Initial Memory Load value
    GlobalMemoryStatus(lpBuffer);
    dwOriginalMemoryLoad = lpBuffer->dwMemoryLoad;

    //Blinking all the NLEDs that have the SWBlinkCtrl field set to 1
    for(DWORD dwLedCount = 0; dwLedCount < GetLedCount(); dwLedCount++)
    {
        //Checking if the this NLED has SWBlinkCtrl flag set to 1
        if(pSWBlinkCtrlNledNums[dwLedCount] == VALID_SWBLINKCTRL)
        {
            //Setting the LED to blink
            dwReturnValue = BlinkNled(dwLedCount);

            if(TPR_PASS != dwReturnValue)
            {
                //Logging the output using the kato logging engine
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Blink Control library failed to blink the NLED %u"), 
                             dwLedCount);

                dwReturnValue = TPR_FAIL;
                goto CLEANUP;
            }
        }
        else
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Blink Control Library is not configured for NLED %u"),
                         dwLedCount);
        }
    }

    //Calculate the memory utilization again
    GlobalMemoryStatus(lpBuffer);
    dwFinalMemoryLoad = lpBuffer->dwMemoryLoad;

    if((dwFinalMemoryLoad - dwOriginalMemoryLoad) < g_nMaxMemoryUtilization)
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("BlinkControl Lib memory utilization is less than the Max allowed Memory Utilization value"));

        dwReturnValue = TPR_PASS;
    }
    else
    {
        //Logging the error output using the kato logging engine
        g_pKato->Log(LOG_FAIL,
                     TEXT("BlinkControl memory utilization is greater than the Max allowed Memory Utilization value"));

        dwReturnValue = TPR_FAIL;
    }

CLEANUP:

    //Releasing the memory
    if(lpBuffer)
    {
        LocalFree(lpBuffer);
        lpBuffer = NULL;
    }

    //De-allocating the memory
    if(pSWBlinkCtrlNledNums)
    {
        delete[] pSWBlinkCtrlNledNums;
        pSWBlinkCtrlNledNums = NULL;
    }

    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo)
    {
        fStatus = setAllLedSettings(pOriginalLedSettingsInfo);

        if(!fStatus)
        dwReturnValue = TPR_FAIL;

        //De-allocating the memory
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    if(TPR_FAIL == dwReturnValue)
        g_pKato->Log(LOG_FAIL,
                     TEXT("Memory utilization tests for blink control library failed"));

    return dwReturnValue;
}

//*****************************************************************************
DWORD GetSWBlinkCtrlLibraryNLed(DWORD dwLedNumber)
//
// Parameters: LED Number
//
// Return Value:
//
//             Returns TPR_PASS if the LedNum has Blink Control Library configured
//
//             Returns TPR_FAIL if the LedNum does not have the  Blink Control
//             Library configured
//
//             Returns TPR_SKIP if none of the LEDS on the device have
//             Blink Control Library configured
//
// Function to check if any Nled has SWBlinkCtrl configured.
//*****************************************************************************
{
    //Declaring the variables
    DWORD dwConfigKeyResult=0;
    HKEY hConfigKey = NULL;
    HKEY hLedNameKey = NULL;
    DWORD dwLedNum = 0;
    DWORD dwBlinkable = 0;
    DWORD dwSWBlinkCtrl =0;
    DWORD dwCountKey = 0;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLedNameSize=gc_dwMaxKeyNameLength + 1;
    DWORD dwLen = sizeof(DWORD);
    DWORD dwReturnValue = TPR_FAIL;
    DWORD dwReturn = 0;
    DWORD dwNoSWBlinkCtrlFlag = 0;

    //Opening the key
    dwReturn = RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                            REG_NLED_KEY_CONFIG_PATH,
                            0, 
                            KEY_ENUMERATE_SUB_KEYS,
                            &hConfigKey);

    if(ERROR_SUCCESS == dwReturn)
    {
        dwConfigKeyResult=RegEnumKeyEx(hConfigKey,
                                       dwCountKey,
                                       szLedName,
                                       &dwLedNameSize,
                                       NULL, 
                                       NULL, 
                                       NULL,
                                       NULL);

        while(ERROR_SUCCESS == dwConfigKeyResult)
        {
            //Opening the key and querying the LED parameters
            dwReturn = RegOpenKeyEx(hConfigKey, szLedName, 0, KEY_ALL_ACCESS, &hLedNameKey);

            if(ERROR_SUCCESS == dwReturn)
            {
                //Querying the LedNum key value
                dwReturn = RegQueryValueEx(hLedNameKey, 
                                           REG_LED_NUM_VALUE, 
                                           NULL,
                                           NULL,
                                           (LPBYTE) &dwLedNum,
                                           &dwLen);

                if(ERROR_SUCCESS != dwReturn)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to query the LedNum key value for %s"), szLedName); 

                    dwReturnValue = TPR_FAIL;
                    goto RETURN;
                }

                //Querying the Blinkable key value
                dwReturn = RegQueryValueEx(hLedNameKey, 
                                           REG_BLINKABLE_VALUE,
                                           NULL, 
                                           NULL,
                                           (LPBYTE) &dwBlinkable,
                                           &dwLen);

                if(ERROR_SUCCESS != dwReturn)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to query the Blinkable key value for %s"), 
                                 szLedName);

                    dwReturnValue = TPR_FAIL;
                    goto RETURN;
                }

                //Querying the SWBlinkCtrl flag value
                dwReturn = RegQueryValueEx(hLedNameKey,
                                           REG_SWBLINKCTRL_VALUE,
                                           NULL,
                                           NULL,
                                           (LPBYTE) &dwSWBlinkCtrl,
                                           &dwLen);

                if(ERROR_SUCCESS == dwReturn)
                {
                    if(0 == dwSWBlinkCtrl)
                    {
                        //BlinkCtrl lib is not configured
                        dwNoSWBlinkCtrlFlag++;
                    }

                    //Checking if the LED has Blink Ctrl configured
                    if(dwLedNumber == dwLedNum)
                    {
                        if((VALID_SWBLINKCTRL == dwSWBlinkCtrl) && 
                           (ZERO_BLINKABLE == dwBlinkable))
                        {
                            dwReturnValue = TPR_PASS;
                            goto RETURN;
                        }
                        else
                        {
                            dwReturnValue = TPR_FAIL;
                        }
                    }
                }

                else if(ERROR_FILE_NOT_FOUND == dwReturn)
                {
                    //BlinkCtrl lib is not configured
                    dwNoSWBlinkCtrlFlag++;
                }

                else
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to query the SWBlinkCtrl flag for %s"), 
                                 szLedName);

                    dwReturnValue = TPR_FAIL;
                    goto RETURN;
                }
            }
            else
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Failed to open the %s key"), szLedName);

                dwReturnValue = TPR_FAIL;
                goto RETURN;
            }

            //Closing the key
            RegCloseKey(hLedNameKey);

            //Incrementing the count
            dwCountKey++;

            //Resetting the buffer size
            dwLedNameSize=gc_dwMaxKeyNameLength + 1;

            //Enumerating for other keys
            dwConfigKeyResult=RegEnumKeyEx(hConfigKey, 
                                           dwCountKey, 
                                           szLedName,
                                           &dwLedNameSize,
                                           NULL,
                                           NULL,
                                           NULL, 
                                           NULL);
        }
    }
    else
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Failed to open NLED-Config key for Enumeration"));
    }

    if(dwNoSWBlinkCtrlFlag == dwCountKey)
    {
        dwReturnValue = TPR_SKIP;
    }

RETURN:

    //Closing the handles
    if(hLedNameKey != NULL)
    {
        RegCloseKey(hLedNameKey);
    }
    if(hConfigKey != NULL)
    {
        RegCloseKey(hConfigKey);
    }

    return dwReturnValue;
}

//*****************************************************************************
DWORD GetSWBlinkCtrlNleds(DWORD *pSWBlinkCtrlNledNums)
//
//  Parameters: DWORD*
//
//  Return Value: TPR_PASS, TPR_FAIL
//
//  Function to return info if Nleds have SWBlinkCtrl Lib configured.
//
//*****************************************************************************
{
    //Declaring the variables
    DWORD dwConfigKeyReslt=0;
    HKEY hConfigKey = NULL;
    HKEY hLedNameKey = NULL;
    DWORD dwReturnValue = TPR_PASS;
    DWORD dwReturn = 0;
    DWORD dwLedCount = 0;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLedNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwLedNum = 0;
    DWORD dwSWBlinkCtrl = 0;
    DWORD dwLen = sizeof(DWORD);

    //Opening the Nled Config key
    dwReturn = RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                            REG_NLED_KEY_CONFIG_PATH, 
                            0,
                            KEY_ENUMERATE_SUB_KEYS, 
                            &hConfigKey);

    if(ERROR_SUCCESS == dwReturn)
    {
        //Enumerating the LedName key
        dwConfigKeyReslt=RegEnumKeyEx(hConfigKey,
                                      dwLedCount, 
                                      szLedName, 
                                      &dwLedNameSize, 
                                      NULL, 
                                      NULL, 
                                      NULL, 
                                      NULL);

        if(ERROR_NO_MORE_ITEMS == dwConfigKeyReslt)
        {
            FAIL("No LEDs configured in the registry");
            RegCloseKey(hConfigKey);
            return TPR_FAIL;
         }

        while (ERROR_SUCCESS == dwConfigKeyReslt)
        {
            //Opening the NLED key for Querying the LED parameters
            dwReturn = RegOpenKeyEx(hConfigKey, 
                                    szLedName, 
                                    0,
                                    KEY_ALL_ACCESS,
                                    &hLedNameKey);

            if(ERROR_SUCCESS == dwReturn)
            {
                //Querying the LedNum parameter
                dwReturn = RegQueryValueEx(hLedNameKey, 
                                           REG_LED_NUM_VALUE,
                                           NULL, 
                                           NULL, 
                                           (LPBYTE) &dwLedNum,
                                           &dwLen);

                if(dwReturn != ERROR_SUCCESS)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to query the LedNum field for %s"), 
                                 szLedName);

                    dwReturnValue = TPR_FAIL;
                    goto RETURN;
                }

                if((dwLedNum < 0) || (dwLedNum > GetLedCount()))
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("The Led Index is not configured correctly for %s"), 
                                 szLedName);

                    dwReturnValue = TPR_FAIL;
                    goto RETURN;
                }

                //Querying the SWBlinkCtrl
                dwReturn = RegQueryValueEx(hLedNameKey, 
                                           REG_SWBLINKCTRL_VALUE,
                                           NULL,
                                           NULL, 
                                           (LPBYTE) &dwSWBlinkCtrl,
                                           &dwLen);

                if(ERROR_SUCCESS == dwReturn)
                {
                    //Checking if the LED has SWBlinkCtrl configured
                    if(VALID_SWBLINKCTRL == dwSWBlinkCtrl)
                    {
                        pSWBlinkCtrlNledNums[dwLedNum] = dwSWBlinkCtrl;
                    }
                    else
                    {
                        //Logging error message using the kato logging engine
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("The NLED %u does not have SWBlinkCtrl configured"), 
                                     dwLedNum);

                        pSWBlinkCtrlNledNums[dwLedNum] = ZERO_SWBLINKCTRL;
                    }
                }
                else if(ERROR_FILE_NOT_FOUND == dwReturn)
                {
                    //No SWBlinkCtrl flag is present
                    pSWBlinkCtrlNledNums[dwLedNum] = ZERO_SWBLINKCTRL;
                }
                else
                {
                    //Logging error message using the kato logging engine
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Failed to query SWBlinkCtrl value for %s key"),
                                 szLedName);

                    dwReturnValue = TPR_FAIL;
                    goto RETURN;
                }
            }
            else
            {
                //Logging error message using the kato logging engine
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Failed to open the %s key"), szLedName);

                dwReturnValue = TPR_FAIL;
                goto RETURN;
            }

            //Closing the key
            if(hLedNameKey)
            {
                RegCloseKey(hLedNameKey);
            }

            //Incrementing the Count
            dwLedCount++;
            dwLedNameSize = gc_dwMaxKeyNameLength + 1;
            dwConfigKeyReslt = RegEnumKeyEx(hConfigKey,
                                            dwLedCount,
                                            szLedName,
                                            &dwLedNameSize,
                                            NULL,
                                            NULL,
                                            NULL,
                                            NULL);
        }
    }
    else
    {
        //Logging error message using the kato logging engine
        FAIL("Failed to open NLED-LedName keys for Enumeration");
        dwReturnValue = TPR_FAIL;
    }

RETURN:

    //Closing the LedName key
    if(hLedNameKey != NULL)
    {
        RegCloseKey(hLedNameKey);
    }

    //Closing the config key
    if(NULL != hConfigKey)
    {
        RegCloseKey(hConfigKey);
    }

    //Returning the status
    return dwReturnValue;
}