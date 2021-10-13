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
#include <svsutil.hxx>
#include "nledobj.hpp" 

#define __FILE_NAME__         TEXT("tests.cpp")

//*****************************************************************************
BOOL Check_NLedGetDeviceInfo( UINT nInfold, void* pOutput )
//
//  Parameters:   same as NLedGetDeviceInfo parameters
//                nInfold -- Integer specifying the information to return.
//                pOutput -- Pointer to the buffer to return the information.
//                           The buffer points to various structure types,
//                           depending on the value of nInfoId.
//
//  Return Value: same as NLedGetDeviceInfo parameters
//                TRUE if the operation succeeded, FALSE otherwise.
//
//  Query NLedGetDeviceInfo for NLED device information, and display results.
//  The structure pointed to by pOutput contains the number of the LED.
//
//*****************************************************************************
{
    DWORD dwLastErr = 0;
    //
    // get NLED count information
    //
    SetLastError( ERROR_SUCCESS );
    if( NLedGetDeviceInfo( nInfold, pOutput ) )
        return TRUE;
    else // NLedGetDeviceInfo() failed
    {
        dwLastErr = GetLastError();

        switch( nInfold )
        {
        case NLED_COUNT_INFO_ID:
            g_pKato->Log( LOG_DETAIL,
                          TEXT("Last error after calling NLedGetDeviceInfo( NLED_COUNT_INFO_ID, %u ) is %d" ),
                          ((NLED_COUNT_INFO*)pOutput)->cLeds,
                          dwLastErr );
            break;
        case NLED_SUPPORTS_INFO_ID:
            g_pKato->Log( LOG_DETAIL,
                          TEXT("Last error after calling NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, %u ) is %d" ),
                          ((NLED_SUPPORTS_INFO*)pOutput)->LedNum,
                          dwLastErr );
            break;
        case NLED_SETTINGS_INFO_ID:
            g_pKato->Log( LOG_DETAIL,
                          TEXT("Last error after calling NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID, %u ) is %d" ),
                          ((NLED_SETTINGS_INFO*)pOutput)->LedNum,
                          dwLastErr );
            break;
        default:
        g_pKato->Log( LOG_DETAIL,
                      TEXT("Last error after calling NLedGetDeviceInfo( %u ) is %d. The parameter %u is unrecognized." ),
                      nInfold,
                      nInfold,
                      dwLastErr );
            break;
        } // switch
        return FALSE;
    } // else NLedGetDeviceInfo() failed
} // Check_NLedGetDeviceInfo

//*****************************************************************************

void displayNLED_SETTINGS_INFO( NLED_SETTINGS_INFO ledSettingsInfo )
//
//  Parameters:   ledSettingsInfo -- NLED_SETTINGS_INFO struct containing
//                                   information to be displayed.
//
//  Return Value: none
//
//  Displays all the information contained in ledSettingsInfo.
//
//*****************************************************************************
{
    //
    // log the LED number
    //
    g_pKato->Log( LOG_DETAIL,
                  TEXT("LED Number %d:" ),
                  ledSettingsInfo.LedNum );
    //
    // log ON/OFF/BLINK status
    //
    switch( ledSettingsInfo.OffOnBlink )
    {
    case LED_OFF:
        g_pKato->Log( LOG_DETAIL,
                      TEXT("   LED:                         OFF" ) );
        break;
    case LED_ON:
        g_pKato->Log( LOG_DETAIL,
                      TEXT("   LED:                         ON" ) );
        break;
    case LED_BLINK:
        g_pKato->Log( LOG_DETAIL,
                      TEXT("   LED:                         BLINKING" ) );
        break;
    default:
        g_pKato->Log(
            LOG_DETAIL,
            TEXT(
               "   LED:                         Invalid OffOnBlink setting" )
            );
        break;
    }

    // log Blink Cycle Time

    g_pKato->Log( LOG_DETAIL,
        TEXT("   Total Blink Cycle Time:      %d microseconds" ),
        ledSettingsInfo.TotalCycleTime );

    // log Cycle ON Time

    g_pKato->Log( LOG_DETAIL,
        TEXT("   Cycle ON time:               %d microseconds" ),
        ledSettingsInfo.OnTime );

    // log Cycle OFF Time

    g_pKato->Log( LOG_DETAIL,
        TEXT("   Cycle OFF time:              %d microseconds" ),
        ledSettingsInfo.OffTime );

    // log Number of ON Blink Cycles

    g_pKato->Log( LOG_DETAIL,
        TEXT("   Number of ON Blink Cycles:   %d" ),
        ledSettingsInfo.MetaCycleOn );

    // log Number of OFF Blink Cycles

    g_pKato->Log( LOG_DETAIL,
        TEXT("   Number of OFF Blink Cycles:  %d" ),
        ledSettingsInfo.MetaCycleOff );

} // displayNLED_SETTINGS_INFO

//*****************************************************************************

void displayNLED_SUPPORTS_INFO( NLED_SUPPORTS_INFO ledSupportsInfo )
//
//  Parameters:   ledSupportsInfo -- NLED_SUPPORTS_INFO struct containing
//                                   information to be displayed.
//
//  Return Value: none
//
//  Displays all the information contained in ledSupportsInfo.
//
//*****************************************************************************
{
    // log the LED support information

    // log the LED number

    g_pKato->Log( LOG_DETAIL,
                  TEXT("LED Number %u:" ),
                  ledSupportsInfo.LedNum
                  );

    // Log the granularity.  In general, it is given in microseconds, however
    // -1 is a special case indicating that the LED is a vibrator.
    g_pKato->Log(
        LOG_DETAIL,
        TEXT(
       "\tCycle time adjustment (granularity):                                 %-ld %s"
        ),
        ledSupportsInfo.lCycleAdjust,
        -1 == ledSupportsInfo.lCycleAdjust
            ? TEXT("(vibrator):" ) : TEXT("microseconds" )
      );

    g_pKato->Log(
        LOG_DETAIL,
        TEXT(
       "\tfAdjustTotalCycleTime (adjustable total cycle time):                 %ssupported"
        ),
        ledSupportsInfo.fAdjustTotalCycleTime ? TEXT( "" ) : TEXT( "not " )
        );

    g_pKato->Log(
        LOG_DETAIL,
        TEXT(
       "\tfAdjustOnTime (separate ON time):                                    %ssupported"
        ),
        ledSupportsInfo.fAdjustOnTime ? TEXT( "" ) : TEXT( "not " )
        );

    g_pKato->Log(
        LOG_DETAIL,
        TEXT(
       "\tfAdjustOffTime (separate OFF time):                                  %ssupported"
        ),
        ledSupportsInfo.fAdjustOffTime ? TEXT( "" ) : TEXT( "not " )
        );

    g_pKato->Log(
        LOG_DETAIL,
        TEXT(
       "\tfMetaCycleOn (blink n cycles, pause, blink n cycles, ...):           %ssupported"
        ),
        ledSupportsInfo.fMetaCycleOn ? TEXT( "" ) : TEXT( "not " )
        );

    g_pKato->Log(
        LOG_DETAIL,
        TEXT(
       "\tfMetaCycleOff (blink n cycles, pause n cycles, blink n cycles, ...): %ssupported"
        ),
        ledSupportsInfo.fMetaCycleOff ? TEXT( "" ) : TEXT( "not " )
        );

} // displayNLED_SUPPORTS_INFO

//*****************************************************************************

    BOOL FindAnAdjustableNLEDSetting( UINT LedNum, UINT* puAdjustableSetting )

//
//  Parameters: LedNum              -- Number of LED to be searched for an
//                                     adjustable setting
//              puAdjustableSetting -- Pointer to a value, identifying an item
//                                     of adjustable settings information.  It
//                                     will be set to one of the following
//                                     values:
//                                       NO_SETTINGS_ADJUSTABLE -> no settings
//                                         -> are adjustable
//                                       All_BLINK_TIMES_ADJUSTABLE  -> total
//                                         cycle, on & off times are all
//                                         adjustable
//                                       TOTAL_CYCLE_TIME_ADJUSTABLE -> total
//                                         cycle time is adjustable
//                                       ON_TIME_ADJUSTABLE          -> on time
//                                         is adjustable
//                                       OFF_TIME_ADJUSTABLE         -> off
//                                         time is adjustable
//                                       META_CYCLE_ON_ADJUSTABLE    -> the
//                                         number of on blink cycles is
//                                         adjustable
//                                       META_CYCLE_OFF_ADJUSTABLE   -> the
//                                         number of on blink cycles is
//                                         adjustable
//
//  Return value: TRUE  -> success
//                FALSE -> failed
//
//  This helper function calls Check_NLedGetDeviceInfo() to get supports
//  information for LedNum.  It then checks the supports flags and returns the
//  first one it finds to be TRUE.  If none are TRUE or if
//  Check_NLedGetDeviceInfo() fails it returns NO_SETTINGS_ADJUSTABLE.
//
//*****************************************************************************
{
    NLED_SUPPORTS_INFO ledSupportsInfo = { 0 };

    *puAdjustableSetting = NO_SETTINGS_ADJUSTABLE;

    ledSupportsInfo.LedNum = LedNum;
    if( !Check_NLedGetDeviceInfo(  NLED_SUPPORTS_INFO_ID, &ledSupportsInfo ) )
        return FALSE;

    if( ledSupportsInfo.fAdjustTotalCycleTime
        && ledSupportsInfo.fAdjustOnTime && ledSupportsInfo.fAdjustOffTime )
    {
        *puAdjustableSetting = All_BLINK_TIMES_ADJUSTABLE;
        return TRUE;
    } // if fAdjustTotalCycleTime, fAdjustOnTime & fAdjustOffTime

    if( ledSupportsInfo.fAdjustTotalCycleTime )
    {
        *puAdjustableSetting = TOTAL_CYCLE_TIME_ADJUSTABLE;
        return TRUE;
    } // if fAdjustTotalCycleTime

        if( ledSupportsInfo.fAdjustOnTime )
    {
        *puAdjustableSetting = ON_TIME_ADJUSTABLE;
        return TRUE;
    } // if fAdjustOnTime

        if( ledSupportsInfo.fAdjustOffTime )
    {
        *puAdjustableSetting = OFF_TIME_ADJUSTABLE;
        return TRUE;
    } // if fAdjustOffTime

        if( ledSupportsInfo.fMetaCycleOn )
    {
        *puAdjustableSetting = META_CYCLE_ON_ADJUSTABLE;
        return TRUE;
    } // if fMetaCycleOn

        if( ledSupportsInfo.fMetaCycleOff )
    {
        *puAdjustableSetting = META_CYCLE_OFF_ADJUSTABLE;
        return TRUE;
    } // if fMetaCycleOff

    // There are no adjustable LED settings.
    return FALSE;

} // FindAnAdjustableNLEDSetting()

//*****************************************************************************

UINT GetLedCount()
//
//  Parameters: none
//  Return value: count of the notification LEDs on the system
//
//  Wrapper routine for NLedGetDeviceInfo( NLED_COUNT_INFO_ID, ... )
//
//*****************************************************************************
{
    NLED_COUNT_INFO ledCountInfo    = { 0 };
    UINT cLeds                      = 0;

    //
    // query the driver for info
    //
    SetLastError( ERROR_SUCCESS );
    if( NLedGetDeviceInfo( NLED_COUNT_INFO_ID, &ledCountInfo ) )
        cLeds = ledCountInfo.cLeds; // the call succeeded, return LED count

    else
    {
        FAIL("NLedGetDeviceInfo( NLED_COUNT_INFO_ID )");
        g_pKato->Log( LOG_DETAIL, TEXT("Last Error = %d"), GetLastError() );
        cLeds = 0;
    }

    return cLeds;
} // GetLedCount()

//*****************************************************************************

UINT invalidLedNum( void )
//
//  Parameters:   -- none
//
//  Return Value: an invalid LED number
//
//  Returns a UINT value larger than any LED number expected by either the user
//  or the system.
//
//*****************************************************************************
{

    UINT LedNum = GetLedCount();

    if( LedNum < g_nMinNumOfNLEDs ) LedNum = g_nMinNumOfNLEDs;
    return LedNum;

} // invalidLednum

//*****************************************************************************

BOOL saveAllLedSettings( NLED_SETTINGS_INFO* pLedSettingsInfo, int OffOnBlink )
//
//  Parameters:   pLedSettingsInfo pointer -- pointer to an array of
//                                            NLED_SETTINGS_INFO structs in
//                                            which to store current settings.
//                OffOnBlink               -- new status for each LED
//                    -1 -> no change
//                     0 -> Off
//                     1 -> On
//                     2 -> Blink or On
//
//  Return Value: FALSE -> unable to store all LED settings
//                TRUE  -> all LED settings copied to pLedSettingsInfo
//
//  First copies settings of each LED into array pLedSettingsInfo, then
//  attempts to set the status to OffOnBlink.  If the attempt fails and
//  OffOnBlink == 2 it is assumed that the LED doesn't support blinking and
//  attempts to turn it on instead.
//
//*****************************************************************************
{
    BOOL bResult = FALSE;
    BOOL bRet    = TRUE;
    UINT LedNum  = 0;

    // Cycle through the LEDs.
    for( LedNum = 0; LedNum < GetLedCount(); LedNum++ )
    {
        pLedSettingsInfo[ LedNum ].LedNum = LedNum;
        if( Check_NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID,
                                     pLedSettingsInfo + LedNum ) )
        {
            if( ( -1 < OffOnBlink ) && ( 3 > OffOnBlink ) )
            {
                bResult = SetLEDOffOnBlink( (pLedSettingsInfo+LedNum)->LedNum,
                                            OffOnBlink,
                                            FALSE );
                if( !bResult && ( LED_BLINK == OffOnBlink ) )
                    // Attempt to enable blinking failed. Blinking may not be
                    // supported, so attempt to turn the LED On instead.
                    bResult = SetLEDOffOnBlink(
                        (pLedSettingsInfo+LedNum)->LedNum,
                        LED_ON,
                        FALSE );

                if( !bResult )
                {
                    g_pKato->Log( LOG_FAIL,
                                  TEXT("Failed set LED %u status." ),
                                  pLedSettingsInfo[ LedNum ].LedNum );
                    bRet= FALSE;
                } // if !LED_OFF
            } // if OffOnBlink
        } // if Check_NLedGetDeviceInfo()
        else bRet = FALSE;
    } // for LedNum
    return bRet;
} // saveAllLedSettings()

//*****************************************************************************
BOOL setAllLedSettings( NLED_SETTINGS_INFO* pLedSettingsInfo )
//
//  Parameters:   pLedSettingsInfo pointer -- pointer to an array of
//                                            NLED_SETTINGS_INFO structs
//                                            containing settings for each LED.
//
//  Return Value: FALSE -> unable to set all LED settings
//                TRUE  -> all LED settings set
//
//  Sets the settings for each LED to the values contained in pLedSettingsInfo.
//
//*****************************************************************************
{
    BOOL bRet       = TRUE;
    int iOffOnBlink = LED_BLINK;
    UINT LedNum     = 0;

    // Cycle through the LEDs.
    for( LedNum = 0; LedNum < GetLedCount(); LedNum++ )
    {
        pLedSettingsInfo[ LedNum ].LedNum = LedNum;

        // Blink settings may only be stored if blinking is being enaled.  If
        // blinking is not enabled it may be necesary to make two attempts to
        // restore the settings.  The first time, with blinking enabled, so
        // that other settings are restored, and a second time bith blinking
        // set to its desired value.

        iOffOnBlink = pLedSettingsInfo[ LedNum ].OffOnBlink;

        // First attempt.  No error checking this time, because the LED may not
        // support blinking, thus causing NLedSetDevice() to fail.
        if( LED_BLINK != iOffOnBlink )
        {
            pLedSettingsInfo[ LedNum ].OffOnBlink = LED_BLINK;
            NLedSetDevice( NLED_SETTINGS_INFO_ID, pLedSettingsInfo + LedNum );
            pLedSettingsInfo[ LedNum ].OffOnBlink = iOffOnBlink;
        } // LED_BLINK

        // Second attempt with desired OffOnBlink.  Check for errors this time.
        SetLastError( ERROR_SUCCESS );
        if( !NLedSetDevice( NLED_SETTINGS_INFO_ID,
                            pLedSettingsInfo + LedNum ) )
        {
            g_pKato->Log( LOG_FAIL,
                TEXT("Failed set LED %u Info Settings. Last Error = %d." ),
                ( pLedSettingsInfo + LedNum )->LedNum,
                GetLastError() );
            bRet = FALSE;
        } // if !NLedSetDevice()
    } // for LedNum
    return bRet;
} // setAllLedSettings()

//*****************************************************************************
BOOL skipNLedDriverTest( UINT LedNum )
//
//  Parameters:   LedNum    -- number of LED.
//
//  Return Value: TRUE      -- Skip interactive NLED test for LedNum.
//                FALSE     -- Don't skip interactive NLED test for LedNum.
//
//  This is an interactive helper function.  It gives the user an opportunity
//  to skip NLED the interactive portion of tests for a LED that may not
//  respond in a predictable way, because it is controlled by another driver.
//  If not in interactive mode it returns TRUE (skip).
//
//*****************************************************************************
{
    DWORD dwMboxValue      = 0;
    TCHAR szMessage[ 330 ] = TEXT("" );

    if( !g_fInteractive ) return TRUE;

    swprintf_s( szMessage, _countof(szMessage),
        TEXT("If LED %u can be controlled by another driver, it may not behave according to NLED driver specifications. Do you wish to proceed with the NLED test?" ),
        LedNum );

    dwMboxValue = MessageBox(
        NULL,
        szMessage,
        TEXT("Input Required" ),
        MB_YESNO | MB_ICONQUESTION| MB_TOPMOST | MB_SETFOREGROUND );
    if( IDYES == dwMboxValue ) return FALSE;
    else                       return TRUE;
} // skipNLedDriverTest()

//*****************************************************************************
DWORD ValidateMetaCycleSettings( NLED_SETTINGS_INFO ledSettingsInfo,
                                 SETTING_INDICATOR  uSettingIndicator,
                                 UINT               uAdjusmentType,
                                 BOOL               bInteractive,
                         NLED_SUPPORTS_INFO ledSupportsInfo)
//
//  Parameters:   LedSettingsInfo   -- struct containing settings info for LED
//                                     being validated
//                uSettingIndicator --
//                                  METACYCLEON  -> MetaCycleOn
//                                  METACYCLEOFF -> MetaCycleOff
//                uAdjusmentType    --
//                                  PROMPT_ONLY -> no adjustment, just observe
//                                                 blinking rate
//                                  SHORTEN     -> shorten setting
//                                  LENGTHEN    -> lengthen setting
//                bInteractive      --
//                                  TRUE  -> solicit user verification of
//                                           adjustment
//                                  FALSE -> don't solicit user verification of
//                                           adjustment
//                ledSupportingInfo -- struct containing supports info for LED
//
//  Return Value: TPR_PASS -> adjustment succeeded
//                TPR_FAIL -> adjustment failed.
//
//  This function first adjusts Setting as specified by uAdjusmentType.  It
//  then queries NLedGetDeviceInfo() to see if the adjustment succeeded.  If
//  so, and if in interactive mode, it then queries the user to observe the LED
//  and verify the adjustment.
//
//*****************************************************************************
{
    DWORD              dwMboxValue       = 0;
    DWORD              dwRet             = TPR_PASS;
    NLED_SETTINGS_INFO ledSettingsInfo_G = { 0 };
    TCHAR              szPrompt[ 160 ]   = TEXT("" );
    LPTSTR             szPrompt1[]       =
    {
        TEXT( "Please note the blinking rate of LED " ),
        TEXT( "Is LED " ),
        TEXT( "Is LED " )
    };
    LPTSTR             szPrompt2[]       =
    {
        TEXT("." ),
        TEXT(" ON for a shorter time per cycle?" ),
        TEXT(" ON for a longer time per cycle?" ),
        TEXT("." ),
        TEXT(" OFF for a shorter time per cycle?" ),
        TEXT(" OFF for a longer time per cycle?" )
    };
    UINT               uType[]           =
    {
        MB_OK    | MB_ICONSTOP     | MB_TOPMOST | MB_SETFOREGROUND,
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND,
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND
    };

    // Make setting adjustment.
    if( METACYCLEON == uSettingIndicator )
    {
        if( SHORTEN  == uAdjusmentType ) ledSettingsInfo.MetaCycleOn = ((INT)META_CYCLE_ON)/2;
        else if( LENGTHEN == uAdjusmentType ) ledSettingsInfo.MetaCycleOn = ((INT)META_CYCLE_ON)*2;
    } // if METACYCLEON
   else //(METACYCLEOFF == uSettingIndicator)
    {
        if( SHORTEN  == uAdjusmentType ) ledSettingsInfo.MetaCycleOff = ((INT)META_CYCLE_OFF)/2;
        else if( LENGTHEN == uAdjusmentType ) ledSettingsInfo.MetaCycleOff = ((INT)META_CYCLE_OFF)*2;
    } // if METACYCLEOFF

    ledSettingsInfo.OffOnBlink = LED_BLINK;
    SetLastError( ERROR_SUCCESS );

    if(!NLedSetDevice( NLED_SETTINGS_INFO_ID, &ledSettingsInfo ) )
    {
        FAIL("NLedSetDevice()");
        g_pKato->Log( LOG_DETAIL,
                      TEXT("\tLast Error = %ld, LED = %u" ),
                      GetLastError(),
                      ledSettingsInfo.LedNum );
        return TPR_FAIL;
    } // else !NLedSetDevice()

   // Get the settings info and compare it to those in ledSettingsInfo
   ledSettingsInfo_G.LedNum = ledSettingsInfo.LedNum;
   SetLastError( ERROR_SUCCESS );
   if( !NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID, &ledSettingsInfo_G ) )
   {
      FAIL("NLedGetDeviceInfo()");
      g_pKato->Log( LOG_DETAIL,
                     TEXT("\tLast Error = %ld, LED = %u" ),
                     GetLastError(),
                     ledSettingsInfo.LedNum );
       return TPR_FAIL;
    } // else !NLedSetDevice()

    // If adjustment failed fail and terminate test.
    if( !ValidateLedSettingsInfo( ledSettingsInfo_G, ledSettingsInfo, ledSupportsInfo) )
        return TPR_FAIL;

    // Automated test finished.  Return if not in interactive mode.
    if( !bInteractive ) return dwRet;

    swprintf_s( szPrompt, _countof(szPrompt),
               TEXT("%s%u%s" ),
               szPrompt1[ uAdjusmentType ],
               ledSettingsInfo.LedNum,
               szPrompt2[ 3 * uSettingIndicator + uAdjusmentType ] );

    dwMboxValue = MessageBox( NULL,
                              szPrompt,
                              TEXT("Input Required" ),
                              uType[ uAdjusmentType ] );

    if( ( PROMPT_ONLY != uAdjusmentType ) && ( IDYES  != dwMboxValue ) )
    {
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("User says LED %u Meta Cycle adjustment failed." ),
            ledSettingsInfo.LedNum );
        dwRet = TPR_FAIL;
    } // if PROMPT_ONLY

    return dwRet;
} // ValidateMetaCycleSettings()

//*****************************************************************************

DWORD Validate_NLedSetDevice_Settings( NLED_SETTINGS_INFO ledSettingsInfo,
                                       BOOL               bExpectedResult,
                                       TCHAR*             szFailMessage )
//
//  Parameters:   ledSettingsInfo -- array of settings info to be passed to
//                                   NLedSetDevice()
//                bExpectedResult -- Value expected to be returned by
//                                   NLedSetDevice()
//                szFailMessage   -- Message to be displayed if NLedSetDevice()
//                                   doesn't return bExpectedResult.
//
//  Return Value: FALSE -> return value from NLedSetDevice() != bExpectedResult
//                TRUE  -> return value from NLedSetDevice() == bExpectedResult
//
//  Attempts to invoke NLedSetDevice(NLED_SETTINGS_INFO_ID, pLedSettingsInfo).
//  If the return value equals bExpectedResult a value of TRUE is returned,
//  otherwise szFailMessage is displayed and the value FALSE is returned.
//
//*****************************************************************************
{
    if( NLedSetDevice( NLED_SETTINGS_INFO_ID, &ledSettingsInfo )
        == bExpectedResult ) return TRUE;

    FAIL("NLedSetDevice()");
    g_pKato->Log( LOG_DETAIL,
                  TEXT("\tLED %u: %s" ),
                  ledSettingsInfo.LedNum,
                  szFailMessage );

    return FALSE;

} // Validate_NLedSetDevice_Settings()

//*****************************************************************************

BOOL ValidateSettingAdjustment( UINT               uSetting,
                                NLED_SETTINGS_INFO CurrentSettings,
                                NLED_SETTINGS_INFO ExpectedSettings,
                                NLED_SETTINGS_INFO OriginalSettings,
                                NLED_SUPPORTS_INFO Supports_Info )
//
//  Parameters:   uSetting         -- setting to be verified.  May be
//                                    ADJUST_TOTAL_CYCLE_TIME, ADJUST_ON_TIME,
//                                    ADJUST_OFF_TIME, ADJUST_META_CYCLE_ON,
//                                    ADJUST_META_CYCLE_OFF.
//                CurrentSettings  -- struct containing current settings
//                ExpectedSettings -- struct containing expected settings
//                OriginalSettings -- struct containing original settings
//                Supports_Info    -- struct containing supports information
//
//  Return Value: FALSE -> current uSetting invalid
//                TRUE  -> current uSetting validated
//
//  If the LED supports uSetting adjustment, and the current uSetting isn't
//  equal to expected uSetting a failure message is logged and FALSE is
//  immediately returned.
//
//  If uSetting is ADJUST_TOTAL_CYCLE_TIME, ADJUST_ON_TIME, or
//  ADJUST_OFF_TIME and the LED doesn't support this adjustment, it is possible
//  that uSetting was adjusted by the driver to be consistent with another time
//  adjustment.  In order to avoid incorrectly failing the test when and the
//  current uSetting isn't equal to the original uSetting, a check is made to
//  see if another time adjustment is supported and if the three times satisfy
//  the equation: Total cycle time = On time + Off time.  If these conditions
//  aren't met a unique failure message is logged and FALSE is immediately
//  returned.
//
//  If neither of the failures described above are detected TRUE is returned.
//
//*****************************************************************************
{
    BOOL   bSuportsUpdatedSetting = FALSE;
    LONG   lCurrentSetting        = 0;
    LONG   lExpectedSetting       = 0;
    LONG   lOriginalSetting       = 0;

    LPTSTR szSetting[]          =
        { TEXT("Total cycle" ),
          TEXT("ON time" ),
          TEXT("OFF time" ),
          TEXT("number of ON blink cycles" ),
          TEXT("number of OFF blink cycles" ) };

    switch( uSetting )
    {
    case ADJUST_TOTAL_CYCLE_TIME:
        bSuportsUpdatedSetting =    Supports_Info.fAdjustTotalCycleTime;
        lCurrentSetting        =    CurrentSettings.TotalCycleTime;
        lExpectedSetting       =    ExpectedSettings.TotalCycleTime;
        lOriginalSetting       =    OriginalSettings.TotalCycleTime;
        break;
    case ADJUST_ON_TIME:
        bSuportsUpdatedSetting =    Supports_Info.fAdjustOnTime;
        lCurrentSetting        =    CurrentSettings.OnTime;
        lExpectedSetting       =    ExpectedSettings.OnTime;
        lOriginalSetting       =    OriginalSettings.OnTime;
        break;
    case ADJUST_OFF_TIME:
        bSuportsUpdatedSetting =    Supports_Info.fAdjustOffTime;
        lCurrentSetting        =    CurrentSettings.OffTime;
        lExpectedSetting       =    ExpectedSettings.OffTime;
        lOriginalSetting       =    OriginalSettings.OffTime;
        break;
    case ADJUST_META_CYCLE_ON:
        bSuportsUpdatedSetting =    Supports_Info.fMetaCycleOn;
        lCurrentSetting        =    CurrentSettings.MetaCycleOn;
        lExpectedSetting       =    ExpectedSettings.MetaCycleOn;
        lOriginalSetting       =    OriginalSettings.MetaCycleOn;
        break;
    case ADJUST_META_CYCLE_OFF:
        bSuportsUpdatedSetting =    Supports_Info.fMetaCycleOff;
        lCurrentSetting        =    CurrentSettings.MetaCycleOff;
        lExpectedSetting       =    ExpectedSettings.MetaCycleOff;
        lOriginalSetting       =    OriginalSettings.MetaCycleOff;
        break;
    default:
        FAIL("switch" );
        g_pKato->Log( LOG_DETAIL, TEXT("\tuSetting out of range." ) );
        return FALSE;
        break;
    } // switch

    // If the adjustment is supported but it wasn't correctly made, fail the
    //test.
    if( bSuportsUpdatedSetting )
    {
        if( lCurrentSetting != lExpectedSetting )
        {
            FAIL("NLedSetDevice( NLED_SETTINGS_INFO_ID )" );
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("\tLED %u: Attempt to adjust %s failed." ),
                OriginalSettings.LedNum,
                szSetting[ uSetting ] );
            return FALSE;
        } // if lCurrentSetting
    } // if bSuportsUpdatedSetting
    return TRUE;
} // ValidateSettingAdjustment()

//*****************************************************************************

BOOL TestLEDBlinkTimeSettings( DWORD dwSetting    = ADJUST_TOTAL_CYCLE_TIME,
                               UINT  LedNum       = 0,
                               BOOL  bInteractive = g_fInteractive )
//
//  Parameters:   dwSetting -- setting to be tested.  May be:
//                                ADJUST_TOTAL_CYCLE_TIME,
//                                ADJUST_ON_TIME, or
//                                ADJUST_OFF_TIME.
//                LedNum    -- number of LED to be tested.
//
//  Return Value: TRUE      -- The adjustment succeeded.
//                FALSE     -- The adjustment failed.
//
//  This helper function uses NLedSetDevice() to adjust the specified setting,
//  and NLedGetDeviceInfo() to get the new settings.  The new settings are then
//  validated.
//
//  If interactive mode has been activated from the command line, the user is
//  asked to observe the LED being tested.  Finally, prompts are displayed for
//  each adjusted setting, asking if the LED responded as expected.
//
//*****************************************************************************
{
    #define INITIAL_STATE           LED_BLINK
    #define INITIAL_META_CYCLE_ON   1
    #define INITIAL_META_CYCLE_OFF  0

    BOOL               bRet                     = TRUE;
    DWORD              dwMboxValue              = 0;
    NLED_SETTINGS_INFO ledSettingsInfo_Final    = { 0 };
    NLED_SETTINGS_INFO ledSettingsInfo_Set      = { LedNum,
                                                    INITIAL_STATE,
                                                    0,
                                                    0,
                                                    0,
                                                    INITIAL_META_CYCLE_ON,
                                                    INITIAL_META_CYCLE_OFF };

    NLED_SUPPORTS_INFO ledSupportsInfo          = { 0 };
    TCHAR              szMessage[ 100 ]         = TEXT("" );

    // Get the supports info for the LED.
    ledSupportsInfo.LedNum = LedNum;
    if( !Check_NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, &ledSupportsInfo ) )
        return FALSE;

    g_pKato->Log( LOG_DETAIL, TEXT("NLED Supports Information" ) );
    displayNLED_SUPPORTS_INFO( ledSupportsInfo );

    // If the LED doesn't support blinking than it automatically passes.
    if( 0 >= ledSupportsInfo.lCycleAdjust )
    {
        g_pKato->Log( LOG_DETAIL,
                      TEXT("LED number %u doesn\'t support blinking, so it automatically passes." ),
                      ledSupportsInfo.LedNum );
        return TRUE;
    } // if 0

    // If the LED doesn't support any blink time adjustments.  It automatically
    // passes.
    if(    !ledSupportsInfo.fAdjustTotalCycleTime
        && !ledSupportsInfo.fAdjustOnTime
        && !ledSupportsInfo.fAdjustOffTime )
    {
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("LED number %u doesn\'t support blink time adjustments, so it automatically passes." ),
            ledSupportsInfo.LedNum );
        return TRUE;
    } // if fAdjustTotalCycleTime

    //Setting the blinking parameters
    ledSettingsInfo_Set.TotalCycleTime = ledSupportsInfo.lCycleAdjust * (CYCLETIME_CHANGE_VALUE * 2);
    ledSettingsInfo_Set.OnTime = ledSupportsInfo.lCycleAdjust * (CYCLETIME_CHANGE_VALUE);
    ledSettingsInfo_Set.OffTime = ledSupportsInfo.lCycleAdjust * (CYCLETIME_CHANGE_VALUE);

    // Display the current settings information for the LED.
    g_pKato->Log( LOG_DETAIL, TEXT("Starting NLED settings" ) );
    displayNLED_SETTINGS_INFO( ledSettingsInfo_Set );

    // Initiate blinking.
    SetLastError( ERROR_SUCCESS );
    if( !NLedSetDevice( NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Set ) )
    {
        FAIL("NLedSetDevice( NLED_SETTINGS_INFO_ID, ... )");
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("Unable to enable LED number %u blinking. Last Error = %d" ),
            ledSettingsInfo_Set.LedNum,
            GetLastError() );
        return FALSE;
    } // !NLedSetDevice

    if( bInteractive )
    {
        // Ask the user to observe the current blinking rates, so that they can
        // be compared to the rates after they are adjusted.

        swprintf_s( szMessage, 100, 
                   TEXT("Note the blinking rate of LED %u" ),
                   LedNum );

        MessageBox( NULL,
                    szMessage,
                    TEXT("Input Required" ),
                    MB_OK | MB_ICONSTOP | MB_TOPMOST | MB_SETFOREGROUND );

    } // if bInteractive

    // If the LED supports all 3 blink time adjustments, make sure that they
    // are compatible.
    if(    ledSupportsInfo.fAdjustTotalCycleTime
        && ledSupportsInfo.fAdjustOnTime
        && ledSupportsInfo.fAdjustOffTime )
    {
        ledSettingsInfo_Set.OnTime  += ledSupportsInfo.lCycleAdjust * (CYCLETIME_CHANGE_VALUE);
        ledSettingsInfo_Set.OffTime += ledSupportsInfo.lCycleAdjust * (CYCLETIME_CHANGE_VALUE);
        ledSettingsInfo_Set.TotalCycleTime
            = ledSettingsInfo_Set.OnTime + ledSettingsInfo_Set.OffTime;
    } // if ledSupportsInfo
    else // !ledSupportsInfo
    {
        // At least one of the times isn't adjustable and therefore will be
        // determined by the driver, so only adjust the time currently being
        // tested.
        switch( dwSetting )
        {
            case ADJUST_TOTAL_CYCLE_TIME:
                ledSettingsInfo_Set.TotalCycleTime += ledSupportsInfo.lCycleAdjust * (CYCLETIME_CHANGE_VALUE * 2);
                break;
            case ADJUST_ON_TIME:
                ledSettingsInfo_Set.OnTime         += ledSupportsInfo.lCycleAdjust * (CYCLETIME_CHANGE_VALUE);
                break;
            case ADJUST_OFF_TIME:
                ledSettingsInfo_Set.OffTime        += ledSupportsInfo.lCycleAdjust * (CYCLETIME_CHANGE_VALUE);
                break;
            default:
                FAIL("switch failed. dwSetting out of range(0,1,2).");
                g_pKato->Log( LOG_DETAIL, TEXT("\tdwSetting = %u"), dwSetting );
                return FALSE;
                break;
        } // switch dwSetting
    } // else !ledSupportsInfo

    // Attempt to update settings.
    SetLastError( ERROR_SUCCESS );
    if( !NLedSetDevice( NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Set ) )
    {
        FAIL("NLedSetDevice( NLED_SETTINGS_INFO_ID, ... )");
        g_pKato->Log(
            LOG_DETAIL,
            TEXT(
           "Unable to adjust settings information for LED number %u. Last Error = %ld"
            ),
            ledSettingsInfo_Set.LedNum,
            GetLastError() );
        return FALSE;
    } // !NLedSetDevice

    // Attempt to get the settings just updated.
    ledSettingsInfo_Final.LedNum = LedNum;
    if( !Check_NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID,
                                  &ledSettingsInfo_Final ) )
        return FALSE;

    g_pKato->Log( LOG_DETAIL, TEXT("NLED settings changed to:" ) );
    displayNLED_SETTINGS_INFO( ledSettingsInfo_Final );

    // Verify that blink times were properly adjusted.
    if(    ledSupportsInfo.fAdjustTotalCycleTime
        && ledSupportsInfo.fAdjustOnTime
        && ledSupportsInfo.fAdjustOffTime )
    {
        if( ledSettingsInfo_Final.TotalCycleTime
            != ledSettingsInfo_Final.OnTime + ledSettingsInfo_Final.OffTime )
        {
            FAIL("NLedSetDevice( NLED_SETTINGS_INFO_ID, ... )");
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("Total Cycle Time != OnTime + OffTime, after NLedSetDevice()." ) );
                bRet = FALSE;
        } // if !=
    }

    // Verify that if total cycle time time is adjustable, it now has the
    // specified value.
    if( ledSupportsInfo.fAdjustTotalCycleTime  &&
        (  ledSettingsInfo_Final.TotalCycleTime
        != ledSettingsInfo_Set.TotalCycleTime ) )
    {
        FAIL("NLedSetDevice( NLED_SETTINGS_INFO_ID, ... )");
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("Total Cycle Time incorrect after NLedSetDevice()." ) );
        bRet = FALSE;
    } // if fAdjustTotalCycleTime

    // Verify that if on time time is adjustable, it now has the specified
    // value.
    if( ledSupportsInfo.fAdjustOnTime  &&
        ( ledSettingsInfo_Final.OnTime != ledSettingsInfo_Set.OnTime ) )
    {
        FAIL("NLedSetDevice( NLED_SETTINGS_INFO_ID, ... )");
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("Blink on Time incorrect after NLedSetDevice()." ) );
        bRet = FALSE;
    } // if fAdjustOnTime

    // Verify that if off time time is adjustable, it now has the specified
    // value.
    if( ledSupportsInfo.fAdjustOffTime  &&
        ( ledSettingsInfo_Final.OffTime != ledSettingsInfo_Set.OffTime ) )
    {
        FAIL("NLedSetDevice( NLED_SETTINGS_INFO_ID, ... )");
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("Blink off Time incorrect after NLedSetDevice()." ) );
        bRet = FALSE;
    } // if fAdjustOffTime

    // If in interactive mode, ask the user to confirm that the times have
    // been adjusted properly.
    if( bInteractive )
    {
        // If total blinking cycle time increased, ask user to confirm it.
        if( ledSupportsInfo.fAdjustTotalCycleTime )
        {
            if(   ledSettingsInfo_Final.TotalCycleTime > (ledSupportsInfo.lCycleAdjust * (CYCLETIME_CHANGE_VALUE * 2)) )
            {
                swprintf_s(
                    szMessage,
                    TEXT("Did the total blinking cycle time of LED %u increase?" ),
                    LedNum );
                dwMboxValue = MessageBox(
                    NULL,
                    szMessage,
                    TEXT("Input Required" ),
                    MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );
                if( IDYES == dwMboxValue )
                    g_pKato->Log(
                        LOG_DETAIL,
                        TEXT("\tNLED %u passed total blink cycle time adjustment test."),
                        LedNum );
                else // !IDYES
                {
                    g_pKato->Log(
                        LOG_DETAIL,
                        TEXT("\tExpected increase in total cycle time for NLED %u blink not confirmed by user." ),
                        LedNum );
                    bRet = FALSE;
                } // else !IDYES
            } // if TotalCycleTime
        }

        // If total on time increased, ask user to confirm it.
        if( ledSupportsInfo.fAdjustOnTime )
        {
            if( ledSettingsInfo_Final.OnTime > (ledSupportsInfo.lCycleAdjust * (CYCLETIME_CHANGE_VALUE)) )
            {
                swprintf_s(
                    szMessage, _countof(szMessage),
                    TEXT("Did the blinking cycle on time of LED %u increase?" ),
                    LedNum );
                dwMboxValue = MessageBox(
                                         NULL,
                                         szMessage,
                                         TEXT("Input Required" ),
                                         MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );

                if( IDYES == dwMboxValue )
                    g_pKato->Log(
                        LOG_DETAIL,
                        TEXT("\tNLED %u passed blink cycle on time adjustment test."),
                        LedNum );
                else // !IDYES
                {
                    g_pKato->Log(
                        LOG_DETAIL,
                        TEXT("\tExpected increase in on time for NLED %u blink not confirmed by user." ),
                        LedNum );
                    bRet = FALSE;
                } // else !IDYES
            } // if OnTime
        }

        if( ledSupportsInfo.fAdjustOffTime )
        {
            // If total off time increased, ask user to confirm it.
            if( ledSettingsInfo_Final.OffTime > (ledSupportsInfo.lCycleAdjust * (CYCLETIME_CHANGE_VALUE)) )
            {
                swprintf_s(
                    szMessage, _countof(szMessage),
                    TEXT("Did the blinking cycle off time of LED %u increase?" ),
                    LedNum );
                dwMboxValue = MessageBox(
                    NULL,
                    szMessage,
                    TEXT("Input Required" ),
                    MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );
                if( IDYES == dwMboxValue )
                     g_pKato->Log(
                        LOG_DETAIL,
                        TEXT("\tNLED %u passed blink cycle off time adjustment test."),
                        LedNum );
                else // !IDYES
                {
                    g_pKato->Log(
                        LOG_DETAIL,
                        TEXT("\tExpected increase in off time for NLED %u blink not confirmed by user." ),
                        LedNum );
                    bRet = FALSE;
                } // else !IDYES
            } // if OffTime
        }
    } // if bInteractive

    // Turn the LED off.
    if( !SetLEDOffOnBlink( ledSettingsInfo_Set.LedNum,
                           LED_OFF,
                           bInteractive ) )
    {
        g_pKato->Log( LOG_FAIL,
                      TEXT("Failed to turn LED %d Off." ),
                      ledSettingsInfo_Set.LedNum );
        bRet = FALSE;
    } // if !SetLEDOffOnBlink()

    return bRet;
} // TestLEDBlinkTimeSettings()

//*****************************************************************************

BOOL ValidateLedSettingsInfo( NLED_SETTINGS_INFO ledSettingsInfo,
                              NLED_SETTINGS_INFO ledSettingsInfo_Valid,
                       NLED_SUPPORTS_INFO ledSupportsInfo)
//
//  Parameters:   ledSettingsInfo       -- NLED_SETTINGS_INFO struct containing
//                                         settings information to be
//                                         validated.
//                ledSettingsInfo_Valid -- NLED_SETTINGS_INFO struct containing
//                                         valid settings information.
//              ledSupportsInfo       -- NLED_SUPPORTS_INFO struct containing
//                                 which settings are supported
//
//  Return Value: TRUE  -> Every element that is supported in ledSettingsInfo 
//                         matches the corresponding element in 
//                         ledSettingsInfo_Valid
//                False -> At least one element in ledSettingsInfo doesn't
//                         matches the corresponding element in
//                         ledSettingsInfo_Valid
//
//  Compares each element that is supported in ledSettingsInfo with the 
//  corresponding element in ledSettingsInfo_Valid.
//
//*****************************************************************************
{
    BOOL bReturn = TRUE;

    if(ledSettingsInfo.LedNum != ledSettingsInfo_Valid.LedNum)
    {
        g_pKato->Log(
            LOG_FAIL,
            TEXT("LED number incorrect. LedNum = %u, should be %u" ),
            ledSettingsInfo.LedNum,
            ledSettingsInfo_Valid.LedNum );
        bReturn = FALSE;
    } // if LedNum

   if((ledSupportsInfo.lCycleAdjust > 0) && 
      (ledSettingsInfo.OffOnBlink  != ledSettingsInfo_Valid.OffOnBlink))
    {
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("LED %d blink setting incorrect. OffOnBlink  = %d, should be %d" ),
            ledSettingsInfo.OffOnBlink,
            ledSettingsInfo_Valid.OffOnBlink );
        bReturn = FALSE;
    } // if OffOnBlink

    if((ledSupportsInfo.fAdjustTotalCycleTime) && 
      (ledSettingsInfo.TotalCycleTime != ledSettingsInfo_Valid.TotalCycleTime))
    {
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("Total cycle time of a blink incorrect. TotalCycleTime = %u, should be %u microseconds" ),
            ledSettingsInfo.TotalCycleTime,
            ledSettingsInfo_Valid.TotalCycleTime );
        bReturn = FALSE;
    } // if TotalCycleTime

    if((ledSupportsInfo.fAdjustOnTime) && 
      (ledSettingsInfo.OnTime != ledSettingsInfo_Valid.OnTime))
    {
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("Cycle on time incorrect. OnTime = %u, should be %u microseconds" ),
            ledSettingsInfo.OnTime,
            ledSettingsInfo_Valid.OnTime );
        bReturn = FALSE;
    } // if OnTime

    if((ledSupportsInfo.fAdjustOffTime) &&
      (ledSettingsInfo.OffTime != ledSettingsInfo_Valid.OffTime))
    {
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("Cycle off time incorrect. OffTime = %u, should be %u microseconds" ),
            ledSettingsInfo.OffTime,
            ledSettingsInfo_Valid.OffTime );
        bReturn = FALSE;
    } // if OffTime

    if((ledSupportsInfo.fMetaCycleOn) &&
      (ledSettingsInfo.MetaCycleOn != ledSettingsInfo_Valid.MetaCycleOn))
    {
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("Number of on blink cycles incorrect. MetaCycleOn = %d, should be %d microseconds" ),
            ledSettingsInfo.MetaCycleOn,
            ledSettingsInfo_Valid.MetaCycleOn );
        bReturn = FALSE;
    } // if MetaCycleOn

    if((ledSupportsInfo.fMetaCycleOff) &&
      (ledSettingsInfo.MetaCycleOff != ledSettingsInfo_Valid.MetaCycleOff))
    {
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("Number of off blink cycles incorrect. MetaCycleOff = %d, should be %d microseconds" ),
            ledSettingsInfo.MetaCycleOff,
            ledSettingsInfo_Valid.MetaCycleOff );
        bReturn = FALSE;
    } // if MetaCycleOff

    return bReturn;

} // ValidateLedSettingsInfo()

//*****************************************************************************

UINT generateInvalidNLED_INFO_ID( void )
//
//  Parameters:   none
//  Return value: A value unequal to any NLED INFO ID.
//
//*****************************************************************************
{
    enum
    { 
        NLED_COUNT_INFO_ID_T = NLED_COUNT_INFO_ID,
        NLED_SUPPORTS_INFO_ID_T = NLED_SUPPORTS_INFO_ID,
        NLED_SETTINGS_INFO_ID_T = NLED_SETTINGS_INFO_ID,
        NLED_TYPE_INFO_ID_T = NLED_TYPE_INFO_ID,
        NLED_INVALID_ID_T
    }; 

    return((UINT)NLED_INVALID_ID_T);


} // generateInvalidNLED_INFO_ID

//*****************************************************************************
DWORD InteractiveVerifyNLEDSettings( NLED_SETTINGS_INFO ledSettingsInfo )
//
//  Parameters:   ledSettingsInfo -- NLED_SETTINGS_INFO struct containing LED
//                                   settings information to be verified.
//
//  Return Value: TPR_PASS        -- All the information has been confirmed by
//                                   the user
//                TPR_FAIL        -- The user has indicated that at least one
//                                   value in ledSettingsInfo is incorrect.
//
//  This helper function displays each item of settings information and prompts
//  the user to verify that it is correct.
//
//*****************************************************************************
{
    DWORD dwRet           = TPR_PASS;
    DWORD dwMboxValue     = 0;
    TCHAR szMessage[ 80 ] = TEXT("" );

    // Verify OffOnBlink
    swprintf_s(
        szMessage, _countof(szMessage),
        TEXT("NLED %u OffOnBlink = %d.\n(0->OFF, 1->ON, 2->BLINK).\nIs that correct?" ),
        ledSettingsInfo.LedNum,
        ledSettingsInfo.OffOnBlink );

    dwMboxValue = MessageBox(
        NULL,
        szMessage,
        TEXT("Input Required" ),
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );

    if( IDYES != dwMboxValue )
    {
        FAIL("ledSettingsInfo.OffOnBlink" );
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("User says OffOnBlink setting is incorrect for LED %ld." ),
            ledSettingsInfo.LedNum );
        dwRet = TPR_FAIL;
    }

    // Verify TotalCycleTime
    swprintf_s(
        szMessage, _countof(szMessage),
        TEXT("NLED %u Total cycle time = %ld microseconds.\nIs that correct?" ),
        ledSettingsInfo.LedNum,
        ledSettingsInfo.TotalCycleTime  );

    dwMboxValue = MessageBox(
        NULL,
        szMessage,
        TEXT("Input Required" ),
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );

    if( IDYES != dwMboxValue )
    {
        FAIL("ledSettingsInfo.TotalCycleTime" );
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("User says TotalCycleTime setting is incorrect for LED %ld." ),
            ledSettingsInfo.LedNum );
        dwRet = TPR_FAIL;
    }

    // Verify OnTime
    swprintf_s(
        szMessage, _countof(szMessage),
        TEXT("NLED %u On time = %ld microseconds.\nIs that correct?" ),
        ledSettingsInfo.LedNum,
        ledSettingsInfo.OnTime );

    dwMboxValue = MessageBox(
        NULL,
        szMessage,
        TEXT("Input Required" ),
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );

    if( IDYES != dwMboxValue )
    {
        FAIL( "ledSettingsInfo.OnTime " );
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("User says OnTime setting is incorrect for LED %ld." ),
            ledSettingsInfo.LedNum );
        dwRet = TPR_FAIL;
    }

    // Verify OffTime
    swprintf_s(
        szMessage, _countof(szMessage),
        TEXT("NLED %u Off time = %ld microseconds.\nIs that correct?" ),
        ledSettingsInfo.LedNum,
        ledSettingsInfo.OffTime );

    dwMboxValue = MessageBox(
        NULL,
        szMessage,
        TEXT("Input Required" ),
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );

    if( IDYES != dwMboxValue )
    {
        FAIL("ledSettingsInfo.OffTime" );
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("User says OffTime setting is incorrect for LED %ld." ),
            ledSettingsInfo.LedNum );
        dwRet = TPR_FAIL;
    }

    // Verify MetaCycleOn
    swprintf_s(
        szMessage, _countof(szMessage),
        TEXT("NLED %u number of on blink cycles = %d.\nIs that correct?" ),
        ledSettingsInfo.LedNum,
        ledSettingsInfo.MetaCycleOn );

    dwMboxValue = MessageBox(
        NULL,
        szMessage,
        TEXT("Input Required" ),
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );

    if( IDYES != dwMboxValue )
    {
        FAIL("ledSettingsInfo.MetaCycleOn" );
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("User says number of on blink cycles setting is incorrect for LED %ld." ),
            ledSettingsInfo.LedNum );
        dwRet = TPR_FAIL;
    }

    // Verify MetaCycleOff
    swprintf_s(
        szMessage, _countof(szMessage),
        TEXT("NLED %u number of off blink cycles = %d.\nIs that correct?" ),
        ledSettingsInfo.LedNum,
        ledSettingsInfo.MetaCycleOff );

    dwMboxValue = MessageBox(
        NULL,
        szMessage,
        TEXT("Input Required" ),
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );

    if( IDYES != dwMboxValue )
    {
        FAIL("ledSettingsInfo.MetaCycleOff" );
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("User says number of off blink cycles setting is incorrect for LED %ld." ),
            ledSettingsInfo.LedNum );
        dwRet = TPR_FAIL;
    }

    return dwRet;

} // InteractiveVerifyNLEDSettings()

//*****************************************************************************
DWORD InteractiveVerifyNLEDSupports( NLED_SUPPORTS_INFO ledSupportsInfo )
//
//  Parameters:   ledSupportsInfo -- NLED_SUPPORTS_INFO struct containing
//                                   supports information about an LED.
//
//  Return Value: TPR_PASS        -- All the information has been confirmed by
//                                   the user
//                TPR_FAIL        -- The user has indicated that at least one
//                                   value in ledSupportsInfo is incorrect.
//
//  This test prompts the user to verify that the NLED supports information is
//  correct.
//
//*****************************************************************************
{
    DWORD dwRet            = TPR_PASS;
    DWORD dwMboxValue      = 0;
    TCHAR szMessage[ 100 ] = TEXT("" );

    swprintf_s(szMessage, _countof(szMessage), 
               TEXT("Does NLED %u support blinking?" ),
               ledSupportsInfo.LedNum );

    dwMboxValue = MessageBox(
        NULL,
        szMessage,
        TEXT("Input Required" ),
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );

    if( IDYES == dwMboxValue )
    {
        // Verify cycle-time adjustments granularity.
        if( 1 > ledSupportsInfo.lCycleAdjust )
        {
            FAIL("ledSupportsInfo.lCycleAdjust" );
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("User says LED %d supports blinking, so the granularity should be greater 0." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

        // Verify total cycle-time adjustment support.

        swprintf_s(
            szMessage, _countof(szMessage),
            TEXT("Does NLED %u support adjustable total cycle time?" ),
            ledSupportsInfo.LedNum );

        if( TPR_PASS
            != VerifyNLEDSupportsOption( ledSupportsInfo.fAdjustTotalCycleTime,
                                         szMessage ) )
        {
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("LED %d total cycle-time adjustment support is inconsistent with user response." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

        // Verify separate on time adjustment support.

        swprintf_s(
            szMessage, _countof(szMessage), 
            TEXT("Does NLED %u support separate adjustable on time?" ),
            ledSupportsInfo.LedNum );

        if( TPR_PASS
            != VerifyNLEDSupportsOption( ledSupportsInfo.fAdjustOnTime,
                                         szMessage ) )
        {
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("LED %d separate adjustable on time support is inconsistent with user response." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

        // Verify separate off time adjustment support.

        swprintf_s(
            szMessage, _countof(szMessage),
            TEXT("Does NLED %u support separate adjustable off time?" ),
            ledSupportsInfo.LedNum );

        if( TPR_PASS
            != VerifyNLEDSupportsOption( ledSupportsInfo.fAdjustOffTime,
                                         szMessage ) )
        {
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("LED %d separate adjustable off time support is inconsistent with user response." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

        // Verify support for"blink n cycles, pause, and blink n cycles."

        swprintf_s(
            szMessage, _countof(szMessage),
            TEXT("Does NLED %u support \"blink n cycles, pause, and blink n cycles?\"" ),
            ledSupportsInfo.LedNum );

        if( TPR_PASS != VerifyNLEDSupportsOption( ledSupportsInfo.fMetaCycleOn,
                                                  szMessage ) )
        {
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("LED %d \"blink n cycles, pause, and blink n cycles\" support is inconsistent with user response." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

        // Verify support for"blink n cycles, pause n cycles, and blink n cycles."

        swprintf_s(
            szMessage, _countof(szMessage),
            TEXT("Does NLED %u support \"blink n cycles, pause n cycles, and blink n cycles?\"" ),
            ledSupportsInfo.LedNum );

        if( TPR_PASS !=
            VerifyNLEDSupportsOption( ledSupportsInfo.fMetaCycleOff,
                                      szMessage ) )
        {
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("LED %d \"blink n cycles, pause n cycles, and blink n cycles\" support is inconsistent with user response." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

    } // if IDYES
    else // !IDYES
    { // user says LED doesn't support blinking.
        if( 0 >= ledSupportsInfo.lCycleAdjust )
        {
            FAIL("ledSupportsInfo.lCycleAdjust" );
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("User says blinking isn't supported for LED %d, so ledSupportsInfo.lCycleAdjust should be 0." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

        if( ledSupportsInfo.fAdjustTotalCycleTime )
        {
            FAIL("ledSupportsInfo.fAdjustTotalCycleTime" );
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("User says blinking isn't supported for LED %d, so ledSupportsInfo.fAdjustTotalCycleTime should be FALSE." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

        if( ledSupportsInfo.fAdjustOnTime )
        {
            FAIL("ledSupportsInfo.fAdjustOnTime" );
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("User says blinking isn't supported for LED %d, so ledSupportsInfo.fAdjustOnTime should be FALSE." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

        if( ledSupportsInfo.fAdjustOffTime )
        {
            FAIL("ledSupportsInfo.fAdjustOffTime" );
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("User says blinking isn't supported for LED %d, so ledSupportsInfo.fAdjustOffTime should be FALSE." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

        if( ledSupportsInfo.fMetaCycleOn )
        {
            FAIL("ledSupportsInfo.fMetaCycleOn" );
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("User says blinking isn't supported for LED %d, so ledSupportsInfo.fMetaCycleOn should be FALSE." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

        if( ledSupportsInfo.fMetaCycleOff )
        {
            FAIL("ledSupportsInfo.fMetaCycleOff" );
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("User says blinking isn't supported for LED %d, so ledSupportsInfo.fMetaCycleOff should be FALSE." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

    } // else !IDYES
    return dwRet;
} // InteractiveVerifyNLEDSupports()

//*****************************************************************************

BOOL SetLEDOffOnBlink( UINT unLedNum,
                       int  iOffOnBlink,
                       BOOL bInteractive = g_fInteractive )
//
//  Parameters: unLedNum    -- number of the LED to change (zero-indexed)
//              iOffOnBlink -- LED_OFF, LED_ON, or LED_BLINK; any other fails.
//              bInteractive --
//                  TRUE  -> do interactive portion of test
//                  FALSE -> skip interactive portion of test
//
//  Return Value: TRUE  -> LED state successfully changed
//                FALSE -> LED state change failed.
//
//  This is a wrapper function for changing the ON/OFF/Blink state of a LED.
//  It changes the LED state and verifies that it is in the correct state.  If
//  interactive mode is activated, the user is prompted to verify the change
//  with either"Yes" or"No."  Interactive mode can be overridden by inserting
//  FALSE or TRUE as the third parameter.
//
//  Note: It is the invoking functions responsibility to insure that the LED
//        supports blinking, before attempting to set the state to blinking. If
//        blinking isn't supported and iOffOnBlink == LED_BLINK,
//        NLedSetDevice() should fail, causing this function to display an
//        error message and return FALSE.
//
//*****************************************************************************
{
    DWORD              dwMboxVal         = 0;
    BOOL               fRet              = TRUE;
    NLED_SETTINGS_INFO ledSettingsInfo   = { 0 };
    NLED_SETTINGS_INFO ledSettingsInfo_G = { 0 };
    TCHAR              szMessage[ 50 ]   = TEXT("" );

    g_pKato->Log( LOG_DETAIL,
                  TEXT("Attempting to set LED %us state to %d"),
                  unLedNum,
                  iOffOnBlink );

    // Check parameters
    if( unLedNum >= GetLedCount() )
    {
        FAIL("Bad LED Number");
        return FALSE;
    }

    if( LED_OFF   != iOffOnBlink &&
        LED_ON    != iOffOnBlink &&
        LED_BLINK != iOffOnBlink )
    {
        FAIL("Invalid ON/OFF/Blink State");
        return FALSE;
    }

    // Get the current settings information.
    ledSettingsInfo_G.LedNum = unLedNum;
    SetLastError( ERROR_SUCCESS );
    if( !NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID, &ledSettingsInfo_G ) )
    {
        g_pKato->Log( LOG_DETAIL,
            TEXT("NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID, ... ) failed" ) );
        g_pKato->Log( LOG_DETAIL,
                      TEXT("Last Error = %ld" ),
                      GetLastError() );
        return FALSE;
    } // else !NLedGetDeviceInfo failed

    // Set the LED state
    ledSettingsInfo = ledSettingsInfo_G;
    ledSettingsInfo.OffOnBlink = iOffOnBlink;
    SetLastError( ERROR_SUCCESS );
    if( !NLedSetDevice( NLED_SETTINGS_INFO_ID, &ledSettingsInfo ) )
    {
        FAIL("NLedSetDevice( NLED_SETTINGS_INFO_ID )");
        g_pKato->Log( LOG_DETAIL, TEXT("Last Error = %d"), GetLastError() );
        return FALSE;
    }

    // Attempt to get and display the information just set.
    ledSettingsInfo_G.LedNum = unLedNum;
    SetLastError( ERROR_SUCCESS );
    if( NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID, &ledSettingsInfo_G ) )
        g_pKato->Log( LOG_DETAIL,
                      TEXT("LED %u is now in state %ld." ),
                      ledSettingsInfo_G.LedNum,
                      ledSettingsInfo_G.OffOnBlink );
    else // NLedGetDeviceInfo() failed
    {
        g_pKato->Log( LOG_DETAIL,
            TEXT("NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID, ... ) failed" ) );
        g_pKato->Log( LOG_DETAIL,
                      TEXT("Last Error = %ld" ),
                      GetLastError() );
        return FALSE;
    } // else NLedGetDeviceInfo failed

    // Verify that the LEDs state was correctly changed.
    if( ledSettingsInfo_G.OffOnBlink != iOffOnBlink )
    {
        FAIL("NLedSetDevice( NLED_SETTINGS_INFO_ID, ... )");
        g_pKato->Log( LOG_DETAIL,
                      TEXT("\tLED %us state incorrect" ),
                      ledSettingsInfo_G.LedNum
                      );
        fRet = FALSE;
    } // if OffOnBlink

    if( bInteractive )
        {

        // Ask the user if the LED is in the correct state

        switch(iOffOnBlink)
        {
        case LED_OFF:
            swprintf_s(szMessage, _countof(szMessage), TEXT("Is LED %u OFF?" ),      unLedNum );
            break;
        case LED_ON:
            swprintf_s(szMessage, _countof(szMessage), TEXT("Is LED %u ON?" ),       unLedNum );
            break;
        case LED_BLINK:
            swprintf_s(szMessage, _countof(szMessage), TEXT("Is LED %u Blinking?" ), unLedNum );
            break;
        default:
            ASSERT( !"Never Reached" );
            break;
        }

        dwMboxVal = MessageBox(
            NULL,
            szMessage,
            TEXT("Input Required" ),
            MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );

        // determinte what the user selected
        //
        switch(dwMboxVal)
        {
        case IDNO:
            fRet = FALSE;
            break;
        case IDYES: // continue testing
            break;
        default:
            ASSERT(!"Never Reached");
            break;
        } // switch

    } // if bInteractive

    return fRet;

} // SetLEDOffOnBlink()

//*****************************************************************************
DWORD ValidateNLEDSettingsChanges( NLED_SETTINGS_INFO newLedSettingsInfo )
//
//  Parameter:    newLedSettingsInfo -- NLED settings information to be
//                                      validated
//
//  Return Value: TPR_PASS           -- all changes to NLED settings are valid
//                TPR_FAIL           -- one or more NLED settings either failed
//                                      to change as expected, or was changed
//                                      inappropriately.
//
//  This test first calls NLedGetDeviceInfo() to record the original settings
//  information for the LED.  It then calls NLedSetDevice() in an attempt to
//  change the settings to the values in newLedSettingsInfo.  Finally it calls
//  NLedGetDeviceInfo() again and compares each setting to both
//  newLedSettingsInfo and to its original value.  If an unsupported change is
//  found, or if an expected supported change didn't occur, TPR_FAIL is
//  returned, otherwise TPR_PASS is returned.
//
//*****************************************************************************
{
    BOOL               bBlinkTimesCanChange    = FALSE;
    DWORD              dwRet                   = TPR_PASS;
    NLED_SUPPORTS_INFO ledSupportsInfo         = { 0 };
    NLED_SETTINGS_INFO originalLedSettingsInfo = { 0 };
    NLED_SETTINGS_INFO updatedLedSettingsInfo  = { 0 };
    UINT               uAdjustment             = 0;

    // Get the supports information for later reference.
    ledSupportsInfo.LedNum = newLedSettingsInfo.LedNum;
    if( Check_NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, &ledSupportsInfo ) )
    {
        g_pKato->Log( LOG_DETAIL, TEXT("NLED Supports Information" ) );
        displayNLED_SUPPORTS_INFO( ledSupportsInfo );
    } // if Check_NLedGetDeviceInfo()
    else // Check_NLedGetDeviceInfo() failed
    {
        FAIL("Check_NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID )" );
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("\tUnable to get supports information for LED %u." ),
            ledSupportsInfo.LedNum );
        return TPR_FAIL;
    } // else Check_NLedGetDeviceInfo() failed

    // Get the original settings information.
    originalLedSettingsInfo.LedNum = newLedSettingsInfo.LedNum;
    if( Check_NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID,
                                 &originalLedSettingsInfo ) )
    {
        g_pKato->Log( LOG_DETAIL,
                      TEXT("Original NLED Settings Information" ) );
        displayNLED_SETTINGS_INFO( originalLedSettingsInfo );
    } // if Check_NLedGetDeviceInfo()
    else // Check_NLedGetDeviceInfo() failed
    {
        FAIL("Check_NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID )" );
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("\tUnable to get original settings for LED %u." ),
            originalLedSettingsInfo.LedNum );
        return TPR_FAIL;
    } // else Check_NLedGetDeviceInfo() failed

    // Attempt to change settings information.
    SetLastError( ERROR_SUCCESS );
    if( NLedSetDevice( NLED_SETTINGS_INFO_ID, &newLedSettingsInfo ) )
    {
        // Verify that only adjustable settings were changed, that the new
        // values are correct, and that any attempt to change un-adjustable
        // settings is handled gracefully.

        // Get the updated settings information.
        updatedLedSettingsInfo.LedNum = newLedSettingsInfo.LedNum;
        if( Check_NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID,
                                     &updatedLedSettingsInfo ) )
        {
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("NLED Settings Information after updated attempt" ) );
            displayNLED_SETTINGS_INFO( updatedLedSettingsInfo );
        } // if Check_NLedGetDeviceInfo()
        else // Check_NLedGetDeviceInfo() failed
        {
            FAIL("Check_NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID )" );
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("\tUnable to retrieve updated settings for LED %u." ),
                updatedLedSettingsInfo.LedNum );
            dwRet = TPR_FAIL;
        } // else Check_NLedGetDeviceInfo() failed

        // Verify that every attempt to adjust a setting has been properly
        // handled.
        for( uAdjustment = ADJUST_TOTAL_CYCLE_TIME;
            uAdjustment <= ADJUST_META_CYCLE_OFF; uAdjustment++ )
            if( !ValidateSettingAdjustment( uAdjustment,
                                            updatedLedSettingsInfo,
                                            newLedSettingsInfo,
                                            originalLedSettingsInfo,
                                            ledSupportsInfo ) )
            dwRet = TPR_FAIL;

    } // NLedSetDevice()
    else // NLedSetDevice() failed
    {
        // NLedSetDevice() should have failed if an attempt was made to make
        // an unsupported adjustment.

        // The following compound if statement verifies that only supported
        // adjustments have occured, before concliding that NLedSetDevice()
        // didn't respond appropriatly.

        // In spite of its length, this statement is not hard to understand.
        // The key is to look at each component separately.  The interspersed
        // comments are intended to facilitate that.  It helps to just read
        // them, before reading the code.

        // If any blink time is adjusted it may cause other times to change,
        // even if they aren't adjustable.  The following flag is being set to
        // simplify the if statement somewhat.

        bBlinkTimesCanChange =  ledSupportsInfo.fAdjustTotalCycleTime
                             || ledSupportsInfo.fAdjustOnTime
                             || ledSupportsInfo.fAdjustOffTime;

        if( (
            // If blinking is supported, only supported adjustments, should
            // have been successful.
               ( 0 != ledSupportsInfo.lCycleAdjust )
            // and OffOnBlink is OFF, ON, or BLINK
            && (  ( LED_OFF   == newLedSettingsInfo.OffOnBlink )
               || ( LED_ON    == newLedSettingsInfo.OffOnBlink )
               || ( LED_BLINK == newLedSettingsInfo.OffOnBlink ) )
            // and either blink times can change or TotalCycleTime didn't
            // change
            && ( bBlinkTimesCanChange || ( newLedSettingsInfo.TotalCycleTime
                == originalLedSettingsInfo.TotalCycleTime ) )
            // and either blink times can change or On time didn't change
            && ( bBlinkTimesCanChange || (      newLedSettingsInfo.OnTime
                ==                         originalLedSettingsInfo.OnTime ) )
            // and either blink times can change or Off time didn't change
            && ( bBlinkTimesCanChange || (      newLedSettingsInfo.OffTime
                ==                         originalLedSettingsInfo.OffTime ) )
            // and either number of on blink cycles is adjustable or it wasn't
            // attempted
            && ( ledSupportsInfo.fMetaCycleOn
                || (     newLedSettingsInfo.MetaCycleOn
                ==  originalLedSettingsInfo.MetaCycleOn ) )
            // and either number of off blink cycles is adjustable or it wasn't
            // attempted
            && ( ledSupportsInfo.fMetaCycleOff
                 || ( newLedSettingsInfo.MetaCycleOff
                 == originalLedSettingsInfo.MetaCycleOff ) )
            // and blink cycle times add up correctly
            && (   newLedSettingsInfo.OnTime
                +  newLedSettingsInfo.OffTime
                == newLedSettingsInfo.TotalCycleTime )
            )
        || (
            // If blinking is not supported, the LED must be either ON or OFF,
            // and no adjustments to other settings should have been attempted.
               ( 0 == ledSupportsInfo.lCycleAdjust )
            // and OffOnBlink is either OFF or ON
            && (  ( LED_OFF == newLedSettingsInfo.OffOnBlink )
               || ( LED_ON  == newLedSettingsInfo.OffOnBlink ) )
            // and no attempt to change total blink cycle time was made
            && ( newLedSettingsInfo.TotalCycleTime
                 == originalLedSettingsInfo.TotalCycleTime )
            // and no attempt to change blink on time was made
            && ( newLedSettingsInfo.OnTime
                == originalLedSettingsInfo.OnTime )
            // and no attempt to change blink off time was made
            && ( newLedSettingsInfo.OffTime
                == originalLedSettingsInfo.OffTime )
            // and no attempt to change the number of on blink cycles was made
            && ( newLedSettingsInfo.MetaCycleOn
                 == originalLedSettingsInfo.MetaCycleOn )
            // and no attempt to change the number of off blink cycles was made
            && ( newLedSettingsInfo.MetaCycleOff
                 == originalLedSettingsInfo.MetaCycleOff )
        ) )
        {
            FAIL("NLedSetDevice( NLED_SETTINGS_INFO_ID )" );
            g_pKato->Log(
                LOG_DETAIL,
                TEXT(
               "Failed while attempting to make a supported settings adjustment for LED %u. Last error = %ld."
                ),
                newLedSettingsInfo.LedNum,
                GetLastError() );
            dwRet = TPR_FAIL;
        }
        else
            // NLedSetDevice() gracefully handled an error and responded
            // correctly.
            g_pKato->Log(
                LOG_DETAIL,
                TEXT(
               "NLedSetDevice() correctly responded to an attempt to make a supported settings adjustment for LED %u. Last error = %ld."
                ),
                newLedSettingsInfo.LedNum,
                GetLastError() );
    } // else NLedSetDevice() failed

    // Turn off LED.
    SetLEDOffOnBlink( newLedSettingsInfo.LedNum, LED_OFF, FALSE );

    return dwRet;
} // ValidateNLEDSettingsChanges()

//*****************************************************************************
DWORD VerifyNLEDSupportsOption( BOOL bOption, LPCTSTR szPrompt )
//
//  Parameters:   bOption  -- NLED supports option to be verified
//                szPrompt -- Prompt string to be displayed
//
//  Return Value: TPR_PASS        -- NLED supports option consistant with user
//                                   response
//                TPR_FAIL        -- NLED supports option inconsistant with
//                                   user response
//
//  This test asks the user if a NLED supports an option and then compares the
//  response to the expected value.
//
//*****************************************************************************
{
    DWORD dwMboxValue = 0;

     // Display prompt asking if the option is supported.
    dwMboxValue = MessageBox(
        NULL,
        szPrompt,
        TEXT("Input Required" ),
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );

    if(    ( ( IDYES == dwMboxValue ) && ( TRUE  == bOption ) )
        || ( ( IDYES != dwMboxValue ) && ( FALSE == bOption ) ) )

        return TPR_PASS;
    else
    {
        FAIL("NLED_SUPPORTS_INFO" );
        return TPR_FAIL;
    }
} // VerifyNLEDSupportsOption()

//*****************************************************************************
TESTPROCAPI TestGetLEDCount( UINT uMsg,
                             TPPARAM tpParam,
                             LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
//  This test function tests the ability to query the count of notification
//  LEDs on the system. The test should pass even if there are zero LEDs.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD           dwRet        = TPR_FAIL;
    NLED_COUNT_INFO ledCountInfo = { 0 };

    //
    // query the number of LEDs
    //
    if( Check_NLedGetDeviceInfo( NLED_COUNT_INFO_ID, &ledCountInfo ) )
    {
        g_pKato->Log( LOG_DETAIL, TEXT("LED Count = %d" ),
                      ledCountInfo.cLeds );
        SUCCESS("Get LED Count Test Passed" );
        dwRet = TPR_PASS;
    } // if NLedGetDeviceInfo()

    return dwRet;
} // TestGetLEDCount()

//*****************************************************************************
TESTPROCAPI
TestGetLEDSupportsInfo(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL
//
//  This test function test the ability to query the NLED_SUPPORTS_INFO struct
//  from the driver. The test will return TPR_PASS if there are no LEDs.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD              dwRet           = TPR_PASS;
    NLED_SUPPORTS_INFO ledSupportsInfo = { 0 };

    // If there are no LEDs pass test.
    if( 0 == GetLedCount() )
    {
        g_pKato->Log( LOG_DETAIL,
                      TEXT("There are no LEDs to Test. Automatic PASS" ) );
        return TPR_PASS;
    } // if

    for( ledSupportsInfo.LedNum = 0;
         ledSupportsInfo.LedNum < GetLedCount();
         ledSupportsInfo.LedNum++ )
    {
        //
        // get LED supports information for each of the LEDs on the system
        SetLastError( ERROR_SUCCESS );
        if( NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, &ledSupportsInfo ) )
        {
            //
            // Display the LED supports information.
            //
            g_pKato->Log( LOG_DETAIL, TEXT("NLED supports information" ) );
            displayNLED_SUPPORTS_INFO( ledSupportsInfo );

        } // if
        else // NLedGetDeviceInfo() failed
        {
            FAIL("NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID ) unexpected error code" );
            g_pKato->Log( LOG_DETAIL,
                          TEXT("Last Error = %d" ),
                          GetLastError() );
            dwRet = TPR_FAIL;
        } // else NLedGetDeviceInfo() failed
    } // for

    if( TPR_PASS == dwRet ) SUCCESS("Get LED Support Info Test Passed" );
    return dwRet;
} // TestGetLEDSupportsInfo()

//*****************************************************************************
TESTPROCAPI
TestGetLEDSettingsInfo(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL
//
//  This function test the ability to retrive values from the
//  NLED_SETTINGS_INFO struct using the driver. The test will return TPR_PASS
//  if there are no LEDs.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD              dwRet           = TPR_PASS;
    NLED_SETTINGS_INFO ledSettingsInfo = { 0 };

    // If there are no LEDs pass test.
    if( 0 == GetLedCount() )
    {
        g_pKato->Log( LOG_DETAIL,
                      TEXT("There are no LEDs to Test. Automatic PASS" ) );
        return TPR_PASS;
    } // if

    for( ledSettingsInfo.LedNum = 0;
         ledSettingsInfo.LedNum < GetLedCount();
         ledSettingsInfo.LedNum++ )
    {
        //
        // Get LED settings information for each of the LEDs on the system
        SetLastError( ERROR_SUCCESS );
        if( NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID, &ledSettingsInfo ) )
        {
            g_pKato->Log( LOG_DETAIL, TEXT("NLED settings" ) );
            displayNLED_SETTINGS_INFO( ledSettingsInfo );
        } // NLedGetDeviceInfo()
        else // NLedGetDeviceInfo() failed
        {
            FAIL("NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID ) unexpected error code" );
            g_pKato->Log( LOG_DETAIL,
                          TEXT("Last Error = %d" ),
                          GetLastError() );
            dwRet = TPR_FAIL;
        } // else NLedGetDeviceInfo() failed
    } // for

    if( TPR_PASS == dwRet ) SUCCESS("Get LED Settings Info Test Passed" );
    return dwRet;
} // TestGetLEDSettingsInfo()

//*****************************************************************************
TESTPROCAPI
TestSetLEDSettingsInfo(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
//  This function test the ability to set values in the NLED_SETTINGS_INFO
//  struct from the driver. The test will return TPR_PASS if there are no LEDs.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD              dwRet                   = TPR_PASS;
    NLED_SETTINGS_INFO ledSettingsInfo         = {       0,
                                                         2,
                                                   2000000,
                                                   1000000,
                                                   1000000,
                                                         4,
                                                         4 };
    NLED_SETTINGS_INFO originalLedSettingsInfo = { 0 };
    UINT               uAdjustableSetting      = NO_SETTINGS_ADJUSTABLE;

    // Attempt to change a setting for each adjustable LED.

    for( ledSettingsInfo.LedNum = 0;
         ledSettingsInfo.LedNum < GetLedCount();
         ledSettingsInfo.LedNum++ )
    {
        // Get the original settings.
        originalLedSettingsInfo.LedNum = ledSettingsInfo.LedNum;
        if( Check_NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID,
                                     &originalLedSettingsInfo ) )

        {
            g_pKato->Log( LOG_DETAIL, TEXT("Original NLED settings" ) );
            displayNLED_SETTINGS_INFO( originalLedSettingsInfo );

            ledSettingsInfo = originalLedSettingsInfo;

            SetLastError( ERROR_SUCCESS );
            if( FindAnAdjustableNLEDSetting( originalLedSettingsInfo.LedNum,
                                             &uAdjustableSetting ) )
            {
                switch ( uAdjustableSetting )
                {
                case All_BLINK_TIMES_ADJUSTABLE:
                    ledSettingsInfo.OnTime *= 2;
                    ledSettingsInfo.TotalCycleTime
                        = ledSettingsInfo.OnTime + ledSettingsInfo.OnTime;
                    break;
                case TOTAL_CYCLE_TIME_ADJUSTABLE:
                    ledSettingsInfo.TotalCycleTime *= 2;
                    break;
                case ON_TIME_ADJUSTABLE:
                    ledSettingsInfo.OnTime *= 2;
                    break;
                case OFF_TIME_ADJUSTABLE:
                    ledSettingsInfo.OffTime *= 2;
                    break;
                case META_CYCLE_ON_ADJUSTABLE:
                    ledSettingsInfo.MetaCycleOn *= 2;
                    break;
                case META_CYCLE_OFF_ADJUSTABLE:
                    ledSettingsInfo.MetaCycleOff *= 2;
                    break;
                default: // Should never get here, because the if statment just
                         // before this switch clause should have bypassed it
                         // if no adjustable settings were found.
                    FAIL("No adjustable setting found." );
                    dwRet = TPR_FAIL;
                } // switch

                SetLastError( ERROR_SUCCESS );
                if( NLedSetDevice( NLED_SETTINGS_INFO_ID, &ledSettingsInfo ) )
                {

                    // log new LED settings

                    g_pKato->Log(
                        LOG_DETAIL,
                        TEXT("NLED settings successfully changed to:" ) );
                    displayNLED_SETTINGS_INFO( ledSettingsInfo );

                    // Restore the original settings before proceeding.
                    if( NLedSetDevice( NLED_SETTINGS_INFO_ID,
                                       &originalLedSettingsInfo ) )
                    {
                        g_pKato->Log( LOG_DETAIL,
                                      TEXT("NLED settings restored to:" ) );
                        displayNLED_SETTINGS_INFO( originalLedSettingsInfo );
                    } // if set original LED settings()
                    else // !NLedSetDevice()
                    {
                        FAIL("Unable to restore LED settings" );
                        dwRet = TPR_FAIL;
                    } // else !NLedSetDevice()

                } // if set new LED settings
                else // NLedGetDeviceInfo() failed
                {
                    FAIL("NLedSetDevice( NLED_SETTINGS_INFO_ID, ... )" );
                    g_pKato->Log( LOG_DETAIL,
                                  TEXT("Last Error = %d" ),
                                  GetLastError() );
                    dwRet = TPR_FAIL;
                } // else NLedSetDevice() failed

            } // if FindFirstAdjustableSetting()
            else if( GetLastError() ) dwRet = TPR_FAIL; // User information for
            // this error has already been displayed by
            // Check_NLedGetDeviceInfo() which was invoked by
            // FindAnAdjustableNLEDSetting().

        } // if Check_NLedGetDeviceInfo()
        else // !Check_NLedGetDeviceInfo()
        {
            FAIL("Unable to get original LED settings" );
            dwRet = TPR_FAIL;
        } // else !Check_NLedGetDeviceInfo()

        if( !NLedSetDevice( NLED_SETTINGS_INFO_ID,
                            &originalLedSettingsInfo ) )
        {
            FAIL("Unable to restore LED settings" );
            dwRet = TPR_FAIL;
        } // else !NLedSetDevice()
    } // for

    if( TPR_PASS == dwRet ) SUCCESS("Set LED Settings Info Test Passed" );
    return dwRet;
} // TestSetLEDSettingsInfo()

//*****************************************************************************

TESTPROCAPI
TestNLedGetDeviceInfoInvaladParms(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_NOT_HANDLED, TPR_FAIL
//
//  This function tests the ability of NLedGetDeviceInfo() to capture invalid
//  parameters.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD dwRet = TPR_PASS;
    void* pVoid = NULL;

    // Verify that NLedGetDeviceInfo( InvalidNLED_INFO_ID, ... ) responds
    // gracefully when passed an invalid info ID (first parameter).
    SetLastError( ERROR_SUCCESS );
    if( NLedGetDeviceInfo( generateInvalidNLED_INFO_ID(), pVoid ) )
    {
        FAIL("NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, ... ) failed to catch an attempt to pass it an invalid NLED info ID.");
        g_pKato->Log( LOG_DETAIL, TEXT("Last Error = %d" ), GetLastError() );
        dwRet = TPR_FAIL;
    } // if NLedGetDeviceInfo()
    else
        SUCCESS("NLedGetDeviceInfo( InvalidNLED_INFO_ID, ... ) responded gracefully when passed an invalid NLED info ID.");

    // Verify that NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, ... ) responds
    // gracefully when passed a NULL buffer pointer (second parameter).
    SetLastError( ERROR_SUCCESS );
    if( NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, NULL ) )
    {
        FAIL("NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, ... ) failed to catch an attempt to pass it a NULL buffer pointer.");
        g_pKato->Log( LOG_DETAIL, TEXT("Last Error = %ld" ), GetLastError() );
        dwRet = TPR_FAIL;
    } // if NLedGetDeviceInfo()
    else
        SUCCESS("NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, ... ) responded gracefully when passed a NULL buffer pointer.");

    /* Saved for future use.
    NLED_SETTINGS_INFO ledSettingsInfo = { 0 };
    // Verify that NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, ... ) responds
    // gracefully when passed inconsistent parameters.
    SetLastError( ERROR_SUCCESS );
    if( NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, &ledSettingsInfo ) )
    {
        FAIL("NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, ... ) failed to catch an attempt to pass it inconsistent parameters.");
        g_pKato->Log( LOG_DETAIL, TEXT("Last Error = %ld" ), GetLastError() );
        dwRet = TPR_FAIL;
    } // if NLedGetDeviceInfo()
    else
        SUCCESS("NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, ... ) responded gracefully when passed inconsistent parameters.");
    */

    // Verify that NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, ... ) responds
    // gracefully when passed an invalid buffer pointer (second parameter).
    SetLastError( ERROR_SUCCESS );
    if( NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, (NLED_SETTINGS_INFO*)0xBADBAD ) )
    {
        FAIL("NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, ... ) failed to catch an attempt to pass it an invalid buffer pointer.");
        g_pKato->Log( LOG_DETAIL, TEXT("Last Error = %ld" ), GetLastError() );
        dwRet = TPR_FAIL;
    } // if NLedGetDeviceInfo()
    else
        SUCCESS("NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, ... ) responded gracefully when passed an invalid buffer pointer.");

    return dwRet;

} // TestNLedGetDeviceInfoInvaladParms()

//*****************************************************************************

TESTPROCAPI
TestNLedGet_WithInvaladNLED(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_NOT_HANDLED, TPR_FAIL
//
//  This function verifies that NLedGetDeviceInfo() responds gracefully when
//  passed an invalid NLED number.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD              dwRet           = TPR_PASS;
    NLED_SETTINGS_INFO ledSettingsInfo = { 0 };
    NLED_SUPPORTS_INFO ledSupportsInfo = { 0 };

    // Verify that NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID, ... ) responds
    // gracefully when passed an invalid NLED number.
    ledSettingsInfo.LedNum = invalidLedNum();
    SetLastError( ERROR_SUCCESS );
    if( NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID, &ledSettingsInfo ) )
    {
        FAIL("NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID, ... ) failed to catch an attempt to pass it an invalid NLED number.");
        g_pKato->Log( LOG_DETAIL, TEXT("Last Error = %d" ), GetLastError() );
        dwRet = TPR_FAIL;
    } // if NLedGetDeviceInfo()
    else
        SUCCESS("NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID, ... ) responded gracefully when passed an invalid NLED number.");

    // Verify that NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, ... ) responds
    // gracefully when passed an invalid NLED number.
    ledSupportsInfo.LedNum = invalidLedNum();
    SetLastError( ERROR_SUCCESS );
    if( NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, &ledSupportsInfo ) )
    {
        FAIL("NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, ... ) failed to catch an attempt to pass it an invalid NLED number.");
        g_pKato->Log( LOG_DETAIL, TEXT("Last Error = %d" ), GetLastError() );
        dwRet = TPR_FAIL;
    } // if NLedGetDeviceInfo()
    else
        SUCCESS("NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, ... ) responded gracefully when passed an invalid NLED number.");

    return dwRet;

} // TestNLedGet_WithInvaladNLED()

//*****************************************************************************

TESTPROCAPI TestNLedSetDeviceInvaladParms( UINT                   uMsg,
                                           TPPARAM                tpParam,
                                           LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_NOT_HANDLED, TPR_FAIL
//
//  This function verifies that NLedGetDeviceInfo() responds gracefully when
//  passed invalid parameters.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD              dwRet           = TPR_PASS;

    /* Save for future use
    
    NLED_SETTINGS_INFO ledSettingsInfo = { 0 };
    NLED_SUPPORTS_INFO ledSupportsInfo = { 0 };
    void*              pVoid           = NULL;*/

    /* Save for future use.

    // Verify that NLedSetDevice() responds gracefully when passed an
    // unsupported info ID (first parameter).
    SetLastError( ERROR_SUCCESS );
    if( NLedSetDevice( NLED_SUPPORTS_INFO_ID, &ledSupportsInfo ) )
    {
        FAIL("NLedSetDevice() failed to catch an attempt to pass it an unsupported info ID.");
        g_pKato->Log( LOG_DETAIL, TEXT("Last Error = %d" ), GetLastError() );
        dwRet = TPR_FAIL;
    } // if NLedSetDevice()
    else
        SUCCESS("NLedSetDevice() responded gracefully when passed an unsupported NLED info ID." );

    // Verify that NLedSetDevice() responds gracefully when passed inconsistent
    // parameters.
    SetLastError( ERROR_SUCCESS );
    if( NLedSetDevice( NLED_SETTINGS_INFO_ID, &ledSupportsInfo ) )
    {
        FAIL("NLedSetDevice() failed to catch an attempt to pass it inconsistent parameters.");
        g_pKato->Log( LOG_DETAIL, TEXT("Last Error = %d" ), GetLastError() );
        dwRet = TPR_FAIL;
    } // if NLedSetDevice()
    else
        SUCCESS("NLedSetDevice() responded gracefully when passed inconsistent parameters." );

    // Verify that NLedSetDevice() responds gracefully when passed an invalid
    // LED number in the second parameter).
    // Note that the first parameter is also invalid, so this test may report
    // either failure, but it should defiantly catch one or the other.
    ledSettingsInfo.LedNum = invalidLedNum();
    SetLastError( ERROR_SUCCESS );
    if( NLedSetDevice( NLED_SUPPORTS_INFO_ID, &ledSettingsInfo ) )
    {
        FAIL("NLedSetDevice() failed to catch an attempt to pass it a unsupported NLED info ID, and an invalid LED number.");
        g_pKato->Log( LOG_DETAIL, TEXT("Last Error = %d" ), GetLastError() );
        dwRet = TPR_FAIL;
    } // if NLedSetDevice()
    else
        SUCCESS("NLedSetDevice() responded gracefully to an attempt to pass it an unsupported NLED info ID, and an invalid LED number." );
    */

    // Verify that NLedSetDevice() responds gracefully when passed an invalid
    // buffer pointer (second parameter).
    SetLastError( ERROR_SUCCESS );
    if( NLedSetDevice( NLED_SETTINGS_INFO_ID, (NLED_SETTINGS_INFO*)0xBADBAD ) )
    {
        FAIL("NLedSetDevice() failed to catch an attempt to pass it an invalid buffer pointer.");
        g_pKato->Log( LOG_DETAIL, TEXT("Last Error = %d" ), GetLastError() );
        dwRet = TPR_FAIL;
    } // if NLedSetDevice()
    else
        SUCCESS("NLedSetDevice() responded gracefully to an attempt to pass it an invalid buffer pointer." );

    return dwRet;

} // TestNLedSetDeviceInvaladParms()

//*****************************************************************************

TESTPROCAPI
TestNLedSet_WithInvaladNLED(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_NOT_HANDLED, TPR_FAIL
//
//  This function tests the ability of NLedSetDevice() to gracefully handle
//  attempts to make adjustments when passed an invalid NLED number.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD              dwRet           = TPR_PASS;
    NLED_SETTINGS_INFO ledSettingsInfo = { 0 };

    // Verify that NLedSetDevice() gracefully handles attempts to make
    // adjustments when passed an invalid NLED number.
    ledSettingsInfo.LedNum = invalidLedNum();
    SetLastError( ERROR_SUCCESS );
    if( NLedSetDevice( NLED_SETTINGS_INFO_ID, &ledSettingsInfo ) )
    {
        FAIL("NLedSetDevice(): Attempt to adjust settings with an invalid NLED number.");
        g_pKato->Log( LOG_DETAIL, TEXT("NLED-%u, Last Error = %d" ),ledSettingsInfo.LedNum, GetLastError() );
        dwRet = TPR_FAIL;
    } // if NLedSetDevice()
    else SUCCESS("NLedSetDevice() successfully caught an attempt to adjust settings with an invalid NLED number.");

    return dwRet;

} // TestNLedSet_WithInvaladNLED()

//*****************************************************************************

TESTPROCAPI TestNLedSet_WithInvaladBlinkSetting(
    UINT                   uMsg,
    TPPARAM                tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_NOT_HANDLED, TPR_FAIL
//
//  This function tests the ability of NLedSetDevice() to gracefully handle
//  attempts to make invalid blink settings.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    BOOL                bTest                     = TRUE;
    BOOL bExpectedResult = FALSE;
    DWORD               dwLED_Res                 = TPR_FAIL;
    DWORD               dwRet                     = TPR_PASS;
    NLED_SETTINGS_INFO  ledSettingsInfo_Set       = { 0 };
    NLED_SETTINGS_INFO  ledSettingsInfo_Get       = { 0 };
    NLED_SUPPORTS_INFO  ledSupportsInfo           = { 0 };
    NLED_SETTINGS_INFO* pOriginalLedSettingsInfo  = NULL;
    UINT                LedNum                    = 0;
    LPTSTR              szFailMessage             = NULL;
    LPTSTR              szNonSupportFailMessages[]=
    {
        TEXT("Attempt to adjust the unsupported TotalCycleTime value. The Driver should have ignored this value" ),
        TEXT("Attempt to adjust the unsupported OnTime value. The Driver should have ignored this value" ),
        TEXT("Attempt to adjust the unsupported OffTime value. The Driver should have ignored this value" ),
        TEXT("Attempt to adjust the unsupported MetaCycleOn value. The Driver should have ignored this value" ),
        TEXT("Attempt to adjust the unsupported MetaCycleOff value. The Driver should have ignored this value" ),
        TEXT("no test" ),
        TEXT("no test" ),
        TEXT("no test" )
    };
    LPTSTR              szSupportFailMessages[]   =
    {
        TEXT("Attempt to adjust blink \"total cycle time\", to a negative value." ),
        TEXT("Attempt to adjust blink \"on time\", to a negative value." ),
        TEXT("Attempt to adjust blink \"off time\", to a negative value." ),
        TEXT("Attempt to adjust \"number of on blink cycles\" to a negative value." ),
        TEXT("Attempt to adjust \"number of off blink cycles\" to a negative value." ),
        TEXT("Attempt to set \"on time\" > \"total cycle time\"" ),
        TEXT("Attempt to set \"off time\" > \"total cycle time\"" ),
        TEXT("Attempt to set inconsistent combination of cycle times." )
    };
    UINT                uScenario                 = 0;

    // Allocate space to save origional LED setttings.
    SetLastError( ERROR_SUCCESS );
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];
    if( !pOriginalLedSettingsInfo )
    {
        FAIL("new failed");
        g_pKato->Log( LOG_DETAIL,
                      TEXT("\tLast Error = %ld" ),
                      GetLastError() );
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    } // else !fMetaCycleOn
    ZeroMemory( pOriginalLedSettingsInfo,
        GetLedCount() * sizeof( NLED_SETTINGS_INFO ) );

    // Save the Info Settings for all the LEDs and turn them Off.
    if( !saveAllLedSettings( pOriginalLedSettingsInfo, LED_OFF ) )
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    } // if !saveAllLedSettings()

    for( LedNum = 0; LedNum < GetLedCount(); LedNum++ )
    {
        dwLED_Res = TPR_PASS;

        // Verify that NLedSetDevice() fails when an attempt to make an invalid
        // blink setting is made.  First copy the original settings and then
        // store an invalid value in OffOnBlink.
        ledSettingsInfo_Set = *(pOriginalLedSettingsInfo + LedNum);
        ledSettingsInfo_Set.OffOnBlink = 3;
        if( !Validate_NLedSetDevice_Settings(
            ledSettingsInfo_Set,
            FALSE,
            TEXT("Attempt to make an invalid blink setting." )
            ) ) dwLED_Res = TPR_FAIL;

        // Get supports information
        ledSupportsInfo.LedNum = LedNum;
        if( Check_NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID,
                                     &ledSupportsInfo ) )
        {
            g_pKato->Log( LOG_DETAIL, TEXT("NLED supports information:" ) );
            displayNLED_SUPPORTS_INFO( ledSupportsInfo );

            // Attempt to enable blinking.
            ledSettingsInfo_Set.OffOnBlink = LED_BLINK;
            ledSettingsInfo_Set.OnTime = 1;
            ledSettingsInfo_Set.OffTime = 1;
            ledSettingsInfo_Set.MetaCycleOn = 1;
            ledSettingsInfo_Set.MetaCycleOff = 0;

            if( NLedSetDevice( NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Set ) )
            {
                Check_NLedGetDeviceInfo(NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Get);
                if(ledSettingsInfo_Get.OffOnBlink == ledSettingsInfo_Set.OffOnBlink &&
                   ledSupportsInfo.lCycleAdjust == -1)
                {
                    // Blinking is not supported, NLedSetDevice() should have
                    // failed.
                    FAIL("NLedSetDevice()");
                    g_pKato->Log(
                        LOG_DETAIL,
                        TEXT("\tAttempt to initiate blinking for LED %u, which doesn\'t support blinking." ),
                        ledSettingsInfo_Set.LedNum );
                    dwLED_Res = TPR_FAIL;

                    // Restore OffOnBlink and other setings for future tests.
                    ledSettingsInfo_Set = *(pOriginalLedSettingsInfo + LedNum);

                }
                else if(ledSettingsInfo_Get.OffOnBlink == ledSettingsInfo_Set.OffOnBlink)
                { // Blinking is supported. Do more tests.

                    for( uScenario = 0; uScenario < 8; uScenario++ )
                    {
                        bTest = TRUE;
                        bExpectedResult = FALSE;
                        ledSettingsInfo_Set
                            = *(pOriginalLedSettingsInfo + LedNum);
                        ledSettingsInfo_Set.OffOnBlink = LED_BLINK;
                        ledSettingsInfo_Set.OnTime = 1;
                        ledSettingsInfo_Set.OffTime = 1;
                        ledSettingsInfo_Set.MetaCycleOn = 1;
                        ledSettingsInfo_Set.MetaCycleOff = 0;
                        szFailMessage = szNonSupportFailMessages[ uScenario ];

                        switch( uScenario )
                        {
                            case 0:
                                if( ledSupportsInfo.fAdjustTotalCycleTime )
                                {
                                    // Attempt to make an impossible Total
                                    // Cycle time adjustment.
                                    ledSettingsInfo_Set.TotalCycleTime
                                        = -10 * (ledSupportsInfo.lCycleAdjust);
                                    szFailMessage
                                        = szSupportFailMessages[ uScenario ];
                                } // fAdjustTotalCycleTime
                                else
                                {
                                    //TotalCycleTime is not configurable.
                                    //The driver should ignore TotalCycleTime values when NLedSetDevice () is called.
                                    ledSettingsInfo_Set.TotalCycleTime = 1000;// 1ms
                                    szFailMessage = szNonSupportFailMessages [uScenario];
                                    bExpectedResult = TRUE;
                                }
                                break;
                            case 1:
                                if( ledSupportsInfo.fAdjustOnTime )
                                {
                                    // Attempt to make an impossible On time
                                    // adjustment.
                                    ledSettingsInfo_Set.OnTime = -5 * (ledSupportsInfo.lCycleAdjust);
                                    szFailMessage
                                        = szSupportFailMessages[ uScenario ];
                                } // fAdjustOnTime
                                else
                                {
                                    //OnTime is not configurable.
                                    //The driver should ignore OnTime values when NLedSetDevice () is called.
                                    ledSettingsInfo_Set.OnTime = 1000; // 1ms
                                    szFailMessage = szNonSupportFailMessages [uScenario];
                                    bExpectedResult = TRUE;
                                }
                                break;
                            case 2:
                                if( ledSupportsInfo.fAdjustOffTime )
                                {
                                    // Attempt to make an impossible Off time
                                    // adjustment.
                                    ledSettingsInfo_Set.OffTime = -5 * (ledSupportsInfo.lCycleAdjust);
                                    szFailMessage
                                        = szSupportFailMessages[ uScenario ];
                                } // fAdjustOffTime
                                else
                                {
                                    //OffTime is not configurable.
                                    //The driver should ignore OffTime values when NLedSetDevice () is called.
                                    ledSettingsInfo_Set.OffTime = 1000; // 1ms
                                    szFailMessage = szNonSupportFailMessages [uScenario];
                                    bExpectedResult = TRUE;
                                }
                                break;
                            case 3:
                                if( ledSupportsInfo.fMetaCycleOn )
                                {
                                    // Attempt to make an impossible On blink
                                    // cycles adjustment.
                                    ledSettingsInfo_Set.MetaCycleOn = -1;
                                    szFailMessage =
                                        szSupportFailMessages[ uScenario ];
                                }
                                else
                                { // !fMetaCycleOn
                                    //MetaCycleOn is not configurable.
                                    //The driver should ignore MetaCycleOn values when NLedSetDevice () is called.
                                    ledSettingsInfo_Set.MetaCycleOn += 1;
                                    szFailMessage =
                                        szNonSupportFailMessages[ uScenario ];
                                    bExpectedResult = TRUE;
                                } // if !fMetaCycleOn
                                break;
                            case 4:
                                if( ledSupportsInfo.fMetaCycleOff )
                                {
                                    // Attempt to make an impossible Off blink
                                    // cycles adjustment.
                                    ledSettingsInfo_Set.MetaCycleOff = -1;
                                    szFailMessage =
                                        szSupportFailMessages[ uScenario ];
                                }
                                else
                                { // !fMetaCycleOff
                                    //MetaCycleOff is not configurable.
                                    //The driver should ignore MetaCycleOff values when NLedSetDevice () is called.
                                    ledSettingsInfo_Set.MetaCycleOff += 1;
                                    szFailMessage =
                                        szNonSupportFailMessages[ uScenario ];
                                    bExpectedResult = TRUE;
                                } // if !fMetaCycleOff
                                break;
                            case 5:
                                if(    ledSupportsInfo.fAdjustTotalCycleTime
                                    && ledSupportsInfo.fAdjustOnTime )
                                {
                                    // Attempt to set On Time > Total Time.
                                    ledSettingsInfo_Set.OnTime
                                        = ledSettingsInfo_Set.TotalCycleTime
                                        + 10* (ledSupportsInfo.lCycleAdjust);
                                    szFailMessage =
                                        szSupportFailMessages[ uScenario ];
                                } // fAdjustTotalCycleTime
                                else bTest = FALSE;

                                break;
                            case 6:
                                if(    ledSupportsInfo.fAdjustTotalCycleTime
                                    && ledSupportsInfo.fAdjustOffTime )
                                {
                                    // Attempt to set Off Time > Total Time.
                                    ledSettingsInfo_Set.OffTime
                                        = ledSettingsInfo_Set.TotalCycleTime
                                        + 10* (ledSupportsInfo.lCycleAdjust);
                                    szFailMessage =
                                        szSupportFailMessages[ uScenario ];
                                } // fAdjustTotalCycleTime
                                else bTest = FALSE;
                                break;
                            case 7:
                                if(    ledSupportsInfo.fAdjustTotalCycleTime
                                    && ledSupportsInfo.fAdjustOnTime
                                    && ledSupportsInfo.fAdjustOffTime )
                                {
                                    // Attempt to set an impossible combination
                                    // of cycle times.
                                    ledSettingsInfo_Set.TotalCycleTime
                                        = 10* (ledSupportsInfo.lCycleAdjust);
                                    ledSettingsInfo_Set.OnTime
                                        = 10* (ledSupportsInfo.lCycleAdjust);
                                    ledSettingsInfo_Set.OffTime
                                        = 10* (ledSupportsInfo.lCycleAdjust);
                                    szFailMessage =
                                        szSupportFailMessages[ uScenario ];
                                } // fAdjustTotalCycleTime
                                else bTest = FALSE;
                                break;
                            default:
                                    bTest = FALSE;
                                g_pKato->Log(
                                    LOG_DETAIL,
                                    TEXT("\tuScenario out of range." ),
                                    ledSettingsInfo_Set.LedNum );
                                break;
                        } // switch

                        if( bTest && !Validate_NLedSetDevice_Settings(
                            ledSettingsInfo_Set,
                            bExpectedResult,
                            szFailMessage
                            ) ) dwLED_Res = TPR_FAIL;

                    } // for uScenario

                } // !0
            } // if NLedSetDevice()
            else // !NLedSetDevice()
            {
                // Check to see if blinking is even supported.
                // If it is then fail. Otherwise ignore it.
                if( 1 <= ledSupportsInfo.lCycleAdjust )
                {
                    // NLedSetDevice() failed.
                    FAIL("NLedSetDevice()");
                    g_pKato->Log(
                        LOG_DETAIL,
                        TEXT("\tFailed to enable blink for LED %u." ),
                        ledSettingsInfo_Set.LedNum );
                    dwLED_Res = TPR_FAIL;
                }
                else
                {
                    g_pKato->Log( LOG_DETAIL, TEXT("\t* LED %u does not support blinking"), ledSettingsInfo_Set.LedNum );
                }
            }  //else !NLedSetDevice()
        } // if Check_NLedGetDeviceInfo(NLED_SUPPORTS_INFO_ID)

        if( TPR_PASS == dwLED_Res )
            g_pKato->Log(
            LOG_DETAIL,
            TEXT("LED %u passed all invalid settings tests." ),
            ledSupportsInfo.LedNum );
        else dwRet = dwLED_Res;

    } // for ledSupportsInfo.LedNum

    // Restore all original LED Info settings.
    if( !setAllLedSettings( pOriginalLedSettingsInfo ) ) dwRet = TPR_FAIL;

    delete[] pOriginalLedSettingsInfo;
    pOriginalLedSettingsInfo = NULL;

    return dwRet;

} // TestNLedSet_WithInvaladBlinkSetting()

//*****************************************************************************
TESTPROCAPI
VerifyNLEDCount(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_NOT_HANDLED, TPR_FAIL
//
//  This test verifies that the NLED count reported by NLedGetDeviceInfo() is
//  correct.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD           dwMboxValue     = 0;
    DWORD           dwRet           = TPR_PASS;
    NLED_COUNT_INFO ledCountInfo    = { 0 };
    TCHAR           szMessage[ 50 ] = TEXT("" );

    // query the driver for the number of NLEDs

    if( !Check_NLedGetDeviceInfo( NLED_COUNT_INFO_ID, &ledCountInfo ) )
        return TPR_FAIL;

    g_pKato->Log( LOG_DETAIL, TEXT("NLED Count = %u." ), ledCountInfo.cLeds );

    if( g_nMinNumOfNLEDs > ledCountInfo.cLeds )
    {
        FAIL("NLedGetDeviceInfo() reported fewer than the minimum number of NLEDs required.");
        g_pKato->Log( LOG_DETAIL,
                      TEXT("\tNLED Count must be at least %u." ),
                      g_nMinNumOfNLEDs );
        dwRet = TPR_FAIL;
    } // if g_nMinNumOfNLEDs
    else // !g_nMinNumOfNLEDs
    {  //  If in interactive mode, ask user to verify NLED Count.
        if( g_fInteractive )
        {
            swprintf_s(szMessage, _countof(szMessage), 
                TEXT("NLED Count = %u. Is this correct?" ),
                ledCountInfo.cLeds );

            dwMboxValue = MessageBox(
                NULL,
                szMessage,
                TEXT("Input Required" ),
                MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );

            if( IDYES == dwMboxValue ) g_pKato->Log(
                LOG_DETAIL,
                TEXT("\tUser confirmed NLED Count." ) );
            else // !IDYES
            {
                g_pKato->Log( LOG_DETAIL,
                              TEXT("\tUser says NLED Count is incorrect." ) );
                dwRet = TPR_FAIL;
            } // else !IDYES
        } // g_fInteractive
    } // if !g_nMinNumOfNLEDs

    if( TPR_PASS == dwRet ) SUCCESS("Verify Get NLED Count Test passed" );
    else                    FAIL("Verify Get NLED Count Test failed" );

    return dwRet;
} // VerifyNLEDCount()

//*****************************************************************************
TESTPROCAPI
VerifyNLEDSupports(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_NOT_HANDLED, TPR_FAIL
//
//  This test verifies that the NLED supports information reported by
//  NLedGetDeviceInfo() for each NLED is correct.  First, it verifies that all
//  blinking option flags are off, for LEDs that don't support blinking.  Then,
//  if user interaction has been enabled, each support information flag is
//  displayed and the user is prompted to verify it.  If there are no LEDS,
//  TPR_PASS is returned.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD              dwRet            = TPR_PASS;
    NLED_SUPPORTS_INFO ledSupportsInfo  = { 0 };

    for( ledSupportsInfo.LedNum = 0;
         ledSupportsInfo.LedNum < GetLedCount();
         ledSupportsInfo.LedNum++ )
    {
        // Get LED supports information for each of the LEDs on the system.
        if( Check_NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, &ledSupportsInfo ) )
        {
            // log the LED support information
            g_pKato->Log( LOG_DETAIL, TEXT("NLED supports information:" ) );
            displayNLED_SUPPORTS_INFO( ledSupportsInfo );

            // If this is the last LED and a vibrator is required the
            // granularity should be -1.
            if( ( GetLedCount() - 1 ) == ledSupportsInfo.LedNum )
            {
                if( g_fVibrator && ( -1 != ledSupportsInfo.lCycleAdjust ) )
                    g_pKato->Log(
                        LOG_DETAIL,
                        TEXT(
                       "WARNING: LED %u is a vibrator. Its Granularity should be -1, but %ld was found."
                        ),
                        ledSupportsInfo.LedNum,
                        ledSupportsInfo.lCycleAdjust );
            } // if GetLedCount()
            else if( 0  > ledSupportsInfo.lCycleAdjust )
            { // This isn't the last LED, so it shouldn't be a vibrator.
                FAIL("NLED_SUPPORTS_INFO_ID" );
                g_pKato->Log( LOG_DETAIL,
                              TEXT("LED %ld has invalid Granularity." ),
                              ledSupportsInfo.LedNum );
                dwRet = TPR_FAIL;
            } // else if

            // If lCycleAdjust < 1, then the LED is either a vibrator or
            // doesn't support blinking. All blink support options should be
            // FALSE.

            if( ( 1 >  ledSupportsInfo.lCycleAdjust )
                && (   ledSupportsInfo.fAdjustTotalCycleTime
                    || ledSupportsInfo.fAdjustOnTime
                    || ledSupportsInfo.fAdjustOffTime
                    || ledSupportsInfo.fMetaCycleOn
                    || ledSupportsInfo.fMetaCycleOff ) )
            {
                FAIL("NLED_SUPPORTS_INFO_ID" );
                g_pKato->Log(
                    LOG_DETAIL,
                    TEXT("LED %d doesn/'t support blinking, but one or more blinking supports flags are enabled." ),
                    ledSupportsInfo.LedNum );
                dwRet = TPR_FAIL;
            } // if 0

            // All other combinations of support information are permitted, so
            // no other automated validation can be done.

            // If interactive mode is enabled, prompt the user to confirm each
            // supports flag.
            if( g_fInteractive )
            {
                if( TPR_PASS
                    == InteractiveVerifyNLEDSupports( ledSupportsInfo ) )

                    g_pKato->Log( LOG_DETAIL,
                        TEXT("LED %d supports information validated." ),
                        ledSupportsInfo.LedNum);

                else dwRet = TPR_FAIL;
            } // if g_fInteractive

        } // if Check_NLedGetDeviceInfo()
        else dwRet = TPR_FAIL;
    } // for

    if( TPR_PASS == dwRet ) SUCCESS("Get LED Support Info Test Passed" );
    return dwRet;

} // VerifyNLEDSupports()

//*****************************************************************************
TESTPROCAPI
ValidateNLEDSettings(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
//  This function validates the NLED settings informaion for each NLED, by
//  verifying that it is valid and consistant with the NLEDs supports
//  information.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD              dwRet           = TPR_PASS;
    NLED_SETTINGS_INFO ledSettingsInfo = { 0 };
    NLED_SUPPORTS_INFO ledSupportsInfo = { 0 };

    // Test each NLED's supports information.
    for( ledSettingsInfo.LedNum = 0; ledSettingsInfo.LedNum < GetLedCount();
         ledSettingsInfo.LedNum++ )
    {
         // First get the supports information.
        ledSupportsInfo.LedNum = ledSettingsInfo.LedNum;
        if( Check_NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID,
                                     &ledSupportsInfo ) )
        {
            g_pKato->Log( LOG_DETAIL, TEXT("NLED supports information:" ) );
            displayNLED_SUPPORTS_INFO( ledSupportsInfo );
        } // if Check_NLedGetDeviceInfo()
        else dwRet = TPR_FAIL;

        // Now get the settings information.
        if( Check_NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID,
                                     &ledSettingsInfo ) )
        {
            g_pKato->Log( LOG_DETAIL, TEXT("NLED Settings information:" ) );
            displayNLED_SETTINGS_INFO( ledSettingsInfo );

            if( ( LED_BLINK == ledSettingsInfo.OffOnBlink )
                && ( ( ledSettingsInfo.OnTime + ledSettingsInfo.OffTime )
                != ledSettingsInfo.TotalCycleTime ) )
            {
                FAIL("TotalCycleTime != OnTime + OffTime" );
                dwRet = TPR_FAIL;
            } // if ledSettingsInfo

            // If in interactive mode, solicit user verification of settings
            // info.
            if( g_fInteractive )
            {
                if( skipNLedDriverTest( ledSettingsInfo.LedNum ) )
                {
                    g_pKato->Log(
                        LOG_DETAIL,
                        TEXT(
                       "Skipping interactive portions of NLED driver test for LED."
                        ) );
                    g_pKato->Log( LOG_DETAIL,
                        TEXT("\tThe user has indicated that LED %u is controlled" ),
                        ledSettingsInfo.LedNum );
                    g_pKato->Log( LOG_DETAIL,
                        TEXT("\tby another driver, and therefore may not behave according to NLED driver specifications." ) );
                } // if skipNLedDriverTest
                else if( TPR_PASS != InteractiveVerifyNLEDSettings(
                    ledSettingsInfo ) ) dwRet = TPR_FAIL;
            } // if g_fInteractive

        } // if Check_NLedGetDeviceInfo()
        else dwRet = TPR_FAIL;
    } // for
    if( TPR_PASS == dwRet ) SUCCESS("Validate NLED Settings Passed" );
    return dwRet;
} // ValidateNLEDSettings()

//*****************************************************************************
TESTPROCAPI VerifySetGetNLEDSettings( UINT uMsg,
                                      TPPARAM tpParam,
                                      LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
//  This function test NLedSetDevice() and NLedGetDeviceInfo(), by verifying
//  that settings values set by NLedSetDevice() match values returned by
//  NLedGetDeviceInfo().  The test will return TPR_PASS if there are no LEDs.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD               dwRet                    = TPR_PASS;
    NLED_SETTINGS_INFO* pOriginalLedSettingsInfo = NULL;

    // Allocate space to save origional LED setttings.
    SetLastError( ERROR_SUCCESS );
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];
    if( !pOriginalLedSettingsInfo )
    {
        FAIL("new failed");
        g_pKato->Log( LOG_DETAIL,
                      TEXT("\tLast Error = %d" ),
                      GetLastError() );
        return TPR_FAIL;
    } // else !fMetaCycleOn
    ZeroMemory( pOriginalLedSettingsInfo,
        GetLedCount() * sizeof( NLED_SETTINGS_INFO ) );

    // Save the Info Settings for all the LEDs and turn them off.
    if( !saveAllLedSettings( pOriginalLedSettingsInfo, LED_OFF ) )
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    } // if !saveAllLedSettings()

    NLED_SETTINGS_INFO ledSettingsInfo_S1 = {      0,
                                           LED_BLINK,
                                                   0,
                                                   0,
                                                   0,
                                                   4,
                                                   4 };

    NLED_SETTINGS_INFO ledSettingsInfo_S2 = {      0,
                                           LED_BLINK,
                                                   0,
                                                   0,
                                                   0,
                                                   6,
                                                   6 };

    UINT               uLedNum            = 0;
    NLED_SUPPORTS_INFO ledSupports_Get = {0};

    // Verify that NLedSetDevice() doesn't fail when passed a NULL buffer
    // pointer (second parameter).
    SetLastError( ERROR_SUCCESS );

    if( NLedSetDevice(NLED_SETTINGS_INFO_ID, NULL) == TRUE) {
        FAIL ("NLedSetDevice succeeded when passed NULL Buffer Parameter");
        dwRet = TPR_FAIL;
    } else if (ERROR_INVALID_PARAMETER == GetLastError ()) {
        SUCCESS ("NLedSetDevice failed and did SetLastError (ERROR_INVALID_PARAMETER) when passed with NULL Buffer Parameter");
    } else {
        FAIL ("NLedSetDevice failed but did not SetLastError (ERROR_INVALID_PARAMETER) when passed with NULL Buffer Parameter");
        g_pKato->Log (LOG_DETAIL, TEXT ("Last Error = %d"), GetLastError ());
        dwRet = TPR_FAIL;
    }

    for( uLedNum = 0; uLedNum < GetLedCount(); uLedNum++ )
    {
        // Attempt to change the LED settings information and verify results.
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("Attempt to change blink settings for NLED %u." ),
            uLedNum
            );

        //Getting the supports Info
        ledSupports_Get.LedNum = uLedNum;

        if(!NLedGetDeviceInfo(NLED_SUPPORTS_INFO_ID, &ledSupports_Get))
        {
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "Error in getting the supports info for LED %u." ),uLedNum);
            dwRet = TPR_FAIL;
        }

        ledSettingsInfo_S1.LedNum = uLedNum;

        //Setting the different blinking parameters
        ledSettingsInfo_S1.TotalCycleTime = (ledSupports_Get.lCycleAdjust) * (CYCLETIME_CHANGE_VALUE * 4);
        ledSettingsInfo_S1.OnTime = (ledSupports_Get.lCycleAdjust) * (CYCLETIME_CHANGE_VALUE * 2);
        ledSettingsInfo_S1.OffTime = (ledSupports_Get.lCycleAdjust) * (CYCLETIME_CHANGE_VALUE * 2);

        if( TPR_PASS == ValidateNLEDSettingsChanges( ledSettingsInfo_S1 ) )
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("NLedSetDevice(NLED_SETTINGS_INFO_ID, ...) passes for NLED %u." ),
                uLedNum
                );
        else
        {
            FAIL("NLedSetDevice(NLED_SETTINGS_INFO_ID, ...) failed first test." );
            dwRet = TPR_FAIL;
        }

        // Make another attempt to change the LED settings using different
        // information.
        g_pKato->Log(
            LOG_DETAIL,
            TEXT("Attempt to change blink settings for NLED %u again." ),
            uLedNum
            );

        ledSettingsInfo_S2.LedNum = uLedNum;

        //Setting the different blinking parameters
        ledSettingsInfo_S2.TotalCycleTime = (ledSupports_Get.lCycleAdjust) * (CYCLETIME_CHANGE_VALUE * 6);
        ledSettingsInfo_S2.OnTime = (ledSupports_Get.lCycleAdjust) * (CYCLETIME_CHANGE_VALUE * 3);
        ledSettingsInfo_S2.OffTime = (ledSupports_Get.lCycleAdjust) * (CYCLETIME_CHANGE_VALUE * 3);

        if( TPR_PASS == ValidateNLEDSettingsChanges( ledSettingsInfo_S2 ) )
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("NLedSetDevice(NLED_SETTINGS_INFO_ID, ...) passes for NLED %u." ),
                uLedNum
                );
        else
        {
            FAIL("NLedSetDevice(NLED_SETTINGS_INFO_ID, ...) failed second test." );
            dwRet = TPR_FAIL;
        }

    } // for

    // Restore all original LED Info settings.
    if( !setAllLedSettings( pOriginalLedSettingsInfo ) ) dwRet = TPR_FAIL;
    delete[] pOriginalLedSettingsInfo;
    pOriginalLedSettingsInfo = NULL;

    if( TPR_PASS == dwRet ) SUCCESS("Verify Set & Get NLED Settings Passed" );
    return dwRet;

} // VerifySetGetNLEDSettings()

//*****************************************************************************
TESTPROCAPI
TestLEDOffOnOff(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
//  This tests the ability of each notification LED to be turned ON and OFF.
//  Returns TPR_PASS if the LED count is zero.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    BOOL               bInteractive            = g_fInteractive;
    DWORD              dwRet                   = TPR_PASS;
    NLED_SETTINGS_INFO ledSettingsInfoOriginal = { 0 };
    UINT               uLedNum                 = 0;

    for( uLedNum = 0; uLedNum < GetLedCount(); uLedNum++ )
    {
        // Save the original OffOnBlink setting if possible.
        ledSettingsInfoOriginal.LedNum = uLedNum;
        if( NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID,
                               &ledSettingsInfoOriginal ) )
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("LED %u, original OffOnBlink setting = %d: saved." ),
                ledSettingsInfoOriginal.LedNum,
                ledSettingsInfoOriginal.OffOnBlink );
        else // !NLedGetDeviceInfo
        {
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("LED %u, unable to save original OffOnBlink setting, will turn it OFF at end of test." ),
                ledSettingsInfoOriginal.LedNum );
            ledSettingsInfoOriginal.OffOnBlink = LED_OFF;
        } // else !NLedGetDeviceInfo

        // See if the user wants to do the interactive portion of the test for
        // this LED.
        bInteractive = !skipNLedDriverTest( uLedNum );

        // Make sure the LED is OFF.
        if( !SetLEDOffOnBlink( uLedNum, LED_OFF, bInteractive ) )
        {
            g_pKato->Log( LOG_FAIL,
                          TEXT("Failed to turn LED %u OFF" ),
                          uLedNum );
            dwRet = TPR_FAIL;
        }

        Sleep(1000);
        // Turn the LED ON.
        if( !SetLEDOffOnBlink( uLedNum, LED_ON, bInteractive ) )
        {
            g_pKato->Log( LOG_FAIL,
                          TEXT("Failed to turn LED %u ON" ),
                          uLedNum );
            dwRet = TPR_FAIL;
        }

        Sleep(1000);
        // Turn the LED back OFF.
        if( !SetLEDOffOnBlink( uLedNum, LED_OFF, bInteractive ) )
        {
            g_pKato->Log( LOG_FAIL,
                          TEXT("Failed to turn LED %u OFF" ),
                          uLedNum );
            dwRet = TPR_FAIL;
        }

        Sleep(1000);
        // Restore the LED to its original setting.
        if( !SetLEDOffOnBlink( uLedNum,
                               ledSettingsInfoOriginal.OffOnBlink,
                               FALSE ) )
        {
            g_pKato->Log(
                LOG_FAIL,
                TEXT("Failed to restore LED %u to its original setting." ),
                uLedNum );
            dwRet = TPR_FAIL;
        }
    }

    // Display message, if there are no LEDs to test.
    if( 0 == GetLedCount() )
        g_pKato->Log( LOG_DETAIL, TEXT("No LEDs to Test. Automatic pass." ) );
    else
        if( TPR_PASS == dwRet ) SUCCESS("LED ON/OFF Test Passed" );
    return dwRet;
} // TestLEDOffOnOff

//*****************************************************************************

TESTPROCAPI
TestLEDOffBlinkOff(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
//  This test function tests the ability to toggle blinking on and off for each
//  LED on the system.  If the LED count is zero or if none of the LEDs support
//  blinking (ledSupportsInfo.lCycleAdjust == 0), the test automatically
//  passes.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    BOOL                bInteractive             = g_fInteractive;
    DWORD               dwRet                    = TPR_PASS;
    NLED_SETTINGS_INFO  ledSettingsInfo          = { 0,
                                                   LED_BLINK,
                                                   0,
                                                   0,
                                                   0,
                                                   1,
                                                   0 };
    NLED_SETTINGS_INFO* pOriginalLedSettingsInfo = NULL;
    NLED_SUPPORTS_INFO ledSupportsInfo           = { 0 };

    // Allocate space to save origional LED setttings.
    SetLastError( ERROR_SUCCESS );
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];
    if( !pOriginalLedSettingsInfo )
    {
        FAIL("new failed");
        g_pKato->Log( LOG_DETAIL,
                      TEXT("\tLast Error = %d" ),
                      GetLastError() );
        return TPR_FAIL;
    } // else !fMetaCycleOn
    ZeroMemory( pOriginalLedSettingsInfo,
        GetLedCount() * sizeof( NLED_SETTINGS_INFO ) );

    // Save the Info Settings for all the LEDs and turn them off.
    if( !saveAllLedSettings( pOriginalLedSettingsInfo, LED_OFF ) )
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    } // if !saveAllLedSettings()

    // test blink features of each LED
    for( ledSupportsInfo.LedNum = 0; ledSupportsInfo.LedNum < GetLedCount();
         ledSupportsInfo.LedNum++ )
    {
        // Get blink features of the LED
        SetLastError( ERROR_SUCCESS );
        if( NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, &ledSupportsInfo ) )
        {
            // If the granularity is 0 the LED doesn't support blinking, skip
            // to the next one. If the granularity is -1, this is a vibrator
            // which also doesn't support blinking.
            if( 0 >= ledSupportsInfo.lCycleAdjust )
            {
                g_pKato->Log( LOG_DETAIL,
                              TEXT("LED %u does not support blinking" ),
                              ledSupportsInfo.LedNum );
            }
            else // !0
            {
                // Store settings information, which supports blinking.
                ledSettingsInfo.LedNum = ledSupportsInfo.LedNum;
                ledSettingsInfo.TotalCycleTime = (ledSupportsInfo.lCycleAdjust)* (CYCLETIME_CHANGE_VALUE * 2);
                ledSettingsInfo.OnTime = (ledSupportsInfo.lCycleAdjust)* (CYCLETIME_CHANGE_VALUE);
                ledSettingsInfo.OffTime = (ledSupportsInfo.lCycleAdjust)* (CYCLETIME_CHANGE_VALUE);
                ledSettingsInfo.MetaCycleOn = 1;
                ledSettingsInfo.MetaCycleOff = 0;
                SetLastError( ERROR_SUCCESS );
                if( NLedSetDevice( NLED_SETTINGS_INFO_ID, &ledSettingsInfo ) )
                {
                    // See if the user wants to do the interactive portion of the
                    // test for this LED.
                    bInteractive = !skipNLedDriverTest( ledSupportsInfo.LedNum );

                    // Make sure LED is off.
                    if( !SetLEDOffOnBlink( ledSupportsInfo.LedNum,
                                           LED_OFF,
                                           bInteractive ) )
                    {
                        g_pKato->Log( LOG_FAIL,
                                      TEXT("Failed to turn LED %u OFF." ),
                                      ledSupportsInfo.LedNum );
                        dwRet = TPR_FAIL;
                    } // if !LED_OFF

                    // set the LED state to blinking
                    if( !SetLEDOffOnBlink( ledSupportsInfo.LedNum,
                                           LED_BLINK,
                                           bInteractive ) )
                    {
                        g_pKato->Log( LOG_FAIL,
                                      TEXT("Failed to Blink LED %u." ),
                                      ledSupportsInfo.LedNum );
                        dwRet = TPR_FAIL;
                    } // if !LED_BLINK

                    // Turn LED off again.
                    if( !SetLEDOffOnBlink( ledSupportsInfo.LedNum,
                                           LED_OFF,
                                           bInteractive ) )
                    {
                        g_pKato->Log( LOG_FAIL,
                                      TEXT("Failed to turn LED %u OFF." ),
                                      ledSupportsInfo.LedNum );
                        dwRet = TPR_FAIL;
                    } // if !LED_OFF
                } // ledSettingsInfo()
                else
                { // !ledSettingsInfo()
                    g_pKato->Log(
                        LOG_FAIL,
                        TEXT(
                       "Failed set LED %u Info Settings. Last Error = %d." ),
                        ledSettingsInfo.LedNum,
                        GetLastError() );
                    dwRet = TPR_FAIL;
                } // else !ledSettingsInfo()
            } // else !0
        } // if NLedGetDeviceInfo()
        else // NLedGetDeviceInfo() failed
        {
            FAIL("NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID )" );
            g_pKato->Log( LOG_DETAIL,
                     TEXT("Unable to query supports information for LED number %u. Last Error = %d" ),
                     ledSupportsInfo.LedNum,
                     GetLastError() );
            dwRet = TPR_FAIL;
        } // else NLedGetDeviceInfo() failed
    } // for

    // Restore all original LED Info settings.
    if( !setAllLedSettings( pOriginalLedSettingsInfo ) ) dwRet = TPR_FAIL;

    if(NULL != pOriginalLedSettingsInfo)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    if( TPR_PASS == dwRet ) SUCCESS("LED Blink Test Passed" );
    return dwRet;

} // TestLEDOffBlinkOff

//*****************************************************************************

TESTPROCAPI TestLEDBlinkCycleTime( UINT                   uMsg,
                                   TPPARAM                tpParam,
                                   LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
//  This function tests the LED driver's ability to adjust the TotalCycleTime,
//  OffTime and OnTime blinking parameters for each LED.  If interactive mode
//  has been activated from the command line, the user is asked to observe the
//  LED and indicate if expected timing changes have occurred.  If the LED
//  count is zero, or if no timing adjustments are supported for any LED, then
//  TPR_PASS is returned.
//
//  This test only verifies supported blink time adjustments.  Run test case
// "Set NLED_INFO Invalid Blink Setting" to verify that attempts to make
//  unsupported blink time adjustments are handled appropriately.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    BOOL                bInteractive             = g_fInteractive;
    DWORD               dwRet                    = TPR_PASS;
    UINT                LedNum                   = 0;
    NLED_SETTINGS_INFO* pOriginalLedSettingsInfo = NULL;

    // Allocate space to save origional LED setttings.
    SetLastError( ERROR_SUCCESS );
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];
    if( !pOriginalLedSettingsInfo )
    {
        FAIL("new failed");
        g_pKato->Log( LOG_DETAIL,
                      TEXT("\tLast Error = %d" ),
                      GetLastError() );
        return TPR_FAIL;
    } // else !fMetaCycleOn
    ZeroMemory( pOriginalLedSettingsInfo,
        GetLedCount() * sizeof( NLED_SETTINGS_INFO ) );

    // Save the Info Settings for all the LEDs and turn them off.
    if( !saveAllLedSettings( pOriginalLedSettingsInfo, LED_OFF ) )
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    } // if !saveAllLedSettings()

    // test blink features of each LED
    for( LedNum = 0; LedNum < GetLedCount(); LedNum++ )
    {
        bInteractive = g_fInteractive;
        if( g_fInteractive )
        {
            if( skipNLedDriverTest( LedNum ) )
            {
                g_pKato->Log(
                    LOG_DETAIL,
                    TEXT(
                   "Skipping interactive portions of NLED driver test for LED."
                    ) );
                g_pKato->Log( LOG_DETAIL,
                    TEXT("\tThe user has indicated that LED %u is controlled" ),
                    LedNum );
                g_pKato->Log( LOG_DETAIL,
                    TEXT("\tby another driver, and therefore may not behave according to NLED driver specifications." ) );
                bInteractive = FALSE;
            } // if skipNLedDriverTest
        } //  if g_fInteractive

        // Perform total cycle time adjustment test and log result.
        if( TestLEDBlinkTimeSettings( ADJUST_TOTAL_CYCLE_TIME,
                                      LedNum,
                                      bInteractive ) )
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("\tNLED %u passed adjust blink total cycle time test." ),
                LedNum );
        else // total cycle time adjustment test failed
        {
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("\tNLED %u failed adjust blink total cycle time test." ),
                LedNum );
            dwRet = TPR_FAIL;
        } // else total cycle time adjustment test failed

        // Perform on time adjustment test and log result.
        if( TestLEDBlinkTimeSettings( ADJUST_ON_TIME,
                                      LedNum,
                                      bInteractive ) )
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("\tNLED %u passed adjust blink on time test." ),
                LedNum );
        else // on time adjustment test failed
        {
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("\tNLED %u failed adjust blink on time test." ),
                LedNum );
            dwRet = TPR_FAIL;
        } // else on time adjustment test failed

        // Perform off time adjustment test and log result.
        if( TestLEDBlinkTimeSettings( ADJUST_OFF_TIME,
                                      LedNum,
                                      bInteractive ) )
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("\tNLED %u passed adjust blink off time test." ),
                LedNum );
        else // off time adjustment test failed
        {
            g_pKato->Log(
                LOG_DETAIL,
                TEXT("\tNLED %u failed adjust blink off time test." ),
                LedNum );
            dwRet = TPR_FAIL;
        } // else off time adjustment test failed
    } // end for each LED loop

    if( !setAllLedSettings( pOriginalLedSettingsInfo ) ) dwRet = TPR_FAIL;
    delete[] pOriginalLedSettingsInfo;
    pOriginalLedSettingsInfo = NULL;

    if( TPR_PASS == dwRet ) SUCCESS("LED Blink Test Passed");
    return dwRet;
} // TestLEDBlinkCycleTime()

//*****************************************************************************
TESTPROCAPI TestLEDMetaCycleBlink( UINT                   uMsg,
                                   TPPARAM                tpParam,
                                   LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
//  This function tests the LED driver's ability to adjust the MetaCycleOn, and
//  MetaCycleOff blink settings for each LED.  When in interactive mode, the
//  user is asked to observe each LED and indicate if expected blink cycle
//  changes have occurred.  If the LED count is zero or if neither of the
//  meta cycle adjustments are supported for any of the LED's, this test case
//  automatically passes.
//
//  This test only verifies supported blink cycle adjustments.  Run test case
// "Set NLED_INFO Invalid Blink Setting" to verify that attempts to make
//  unsupported blink cycle adjustments are handled appropriately.
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    BOOL                bInteractive             = g_fInteractive;
    DWORD               dwRet                    = TPR_PASS;
    NLED_SETTINGS_INFO  ledSettingsInfoDefault   = { 0,
                                                     LED_BLINK,
                                                     0,
                                                     0,
                                                     0,
                                                     META_CYCLE_ON,
                                                     META_CYCLE_OFF };
    NLED_SUPPORTS_INFO  ledSupportsInfo          = { 0 };
    NLED_SETTINGS_INFO* pOriginalLedSettingsInfo = NULL;
    UINT                uAdjustmentType          = PROMPT_ONLY;

    // Allocate space to save origional LED setttings.
    SetLastError( ERROR_SUCCESS );
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];
    if( !pOriginalLedSettingsInfo )
    {
        FAIL("new failed");
        g_pKato->Log( LOG_DETAIL,
                      TEXT("\tLast Error = %d" ),
                      GetLastError() );
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    } // else !fMetaCycleOn
    ZeroMemory( pOriginalLedSettingsInfo,
        GetLedCount() * sizeof( NLED_SETTINGS_INFO ) );

    // Save the Info Settings for all the LEDs and turn them off.
    if( !saveAllLedSettings( pOriginalLedSettingsInfo, LED_OFF ) )
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    } // if !saveAllLedSettings()

    // test blink features of each LED
    for( ledSettingsInfoDefault.LedNum = 0;
         ledSettingsInfoDefault.LedNum < GetLedCount();
         ledSettingsInfoDefault.LedNum++ )
    {
        // Get supports information for the LED.
        ledSupportsInfo.LedNum = ledSettingsInfoDefault.LedNum;
        if( Check_NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID,
                                     &ledSupportsInfo ) )
        {
            g_pKato->Log( LOG_DETAIL, TEXT("NLED Supports Information" ) );
            displayNLED_SUPPORTS_INFO( ledSupportsInfo );

            // Determine if the user wants to bypass interactive mode for this
            // LED.
            bInteractive = g_fInteractive;
            if( g_fInteractive
                && skipNLedDriverTest( ledSettingsInfoDefault.LedNum ) )
            {
                g_pKato->Log(
                    LOG_DETAIL,
                    TEXT("Skipping interactive portions of NLED driver test for LED."
                    ) );
                g_pKato->Log( LOG_DETAIL,
                    TEXT(
                   "\tThe user has indicated that LED %u is controlled by another driver,"
                    ),
                    ledSettingsInfoDefault.LedNum );
                g_pKato->Log( LOG_DETAIL,
                    TEXT(
                   "\tand therefore may not behave according to NLED driver specifications."
                    ) );
                bInteractive = FALSE;
            } //  if g_fInteractive

            // Setting the blinking parameters
            ledSettingsInfoDefault.TotalCycleTime = (ledSupportsInfo.lCycleAdjust)* (CYCLETIME_CHANGE_VALUE * 2);
            ledSettingsInfoDefault.OnTime = (ledSupportsInfo.lCycleAdjust)* (CYCLETIME_CHANGE_VALUE);
            ledSettingsInfoDefault.OffTime = (ledSupportsInfo.lCycleAdjust)* (CYCLETIME_CHANGE_VALUE);

            // If Meta Cycle On time is adjustable, test it.

            if( ledSupportsInfo.fMetaCycleOn )
                for( uAdjustmentType  = PROMPT_ONLY;
                     uAdjustmentType <= LENGTHEN;
                     uAdjustmentType++ )
                    if( TPR_PASS != ValidateMetaCycleSettings(
                        ledSettingsInfoDefault,
                        METACYCLEON,
                        uAdjustmentType,
                        bInteractive,
                  ledSupportsInfo) ) dwRet = TPR_FAIL;

            // If Meta Cycle Off time is adjustable, test it.

            if( ledSupportsInfo.fMetaCycleOff )
                for( uAdjustmentType  = PROMPT_ONLY;
                     uAdjustmentType <= LENGTHEN;
                     uAdjustmentType++ )

                    if( TPR_PASS != ValidateMetaCycleSettings(
                        ledSettingsInfoDefault,
                        METACYCLEOFF,
                        uAdjustmentType,
                        bInteractive,
                  ledSupportsInfo) ) dwRet = TPR_FAIL;

                // turn the LED off
                if( !SetLEDOffOnBlink( ledSettingsInfoDefault.LedNum,
                                       LED_OFF,
                                       bInteractive ) )
                {
                    g_pKato->Log( LOG_FAIL,
                        TEXT("Failed to turn LED %u OFF." ),
                        ledSettingsInfoDefault.LedNum );
                    dwRet = TPR_FAIL;
                } // if !LED_OFF

            } // if Check_NLedGetDeviceInfo()
            else // can't get information needed to test blinking for the
                 // curent LED, so move on to the next LED.
                dwRet = TPR_FAIL;
    } // for

    // Restore all original LED Info settings.
    if( !setAllLedSettings( pOriginalLedSettingsInfo ) ) dwRet = TPR_FAIL;

    delete[] pOriginalLedSettingsInfo;
    pOriginalLedSettingsInfo = NULL;
    if( TPR_PASS == dwRet ) SUCCESS("LED Blink Test Passed");
    return dwRet;
} // TestLEDMetaCycleBlink()

//*****************************************************************************
TESTPROCAPI NLedSetWithInvalidParametersTest(UINT                   uMsg,
                                             TPPARAM                tpParam,
                                             LPFUNCTION_TABLE_ENTRY lpFTE)
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED, TPR_SKIP
//
//  This function tests the LED driver's ability to behave gracefully when
//  invalid parameters are passed.
//
// "Call NLedSetDevice with OffOnBlink, TotalCycleTime, OnTime, OffTime,
//  MetaCycleOn, and MetaCycleOFF being negative.  Function must return false
//  and exit gracefully"
//
// "Call NLedSetDevice with null pointer for"NLED_SETTINGS_INFO".
//  Function must exit gracefully and return false"
//
// "NLedSetDevice is generally called with nDeviceId being
// "NLED_SETTINGS_INFO_ID". Call NLedSetDevice with nDeviceId being a
//  different value. Function must return false and exit gracefully"
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Only Supporting executing the test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the general or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fGeneralTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    // If there are no NLEDs on the device pass the test.
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("There are no NLEDs to Test on the Device. Automatic PASS"));

        return TPR_PASS;
    }

    //Declaring and initializing the variables
    DWORD dwReturn = TPR_FAIL;
    DWORD dwReturnValue = TPR_FAIL;

    //Get the original led settings.
    NLED_SETTINGS_INFO originalLedSettingsInfo = { 0 };
    NLED_SUPPORTS_INFO ledSupportsInfo = { 0 };
    NLED_SETTINGS_INFO ledSettingsInfo_Set = { 0 };

    BOOL fReturnValue = NLedGetDeviceInfo(NLED_SETTINGS_INFO_ID,
                                          &originalLedSettingsInfo);

    if(!fReturnValue)
    {
        FAIL("Unable to get the original LED settings");
        return TPR_FAIL;
    }

    //Get the supportsInfo
    fReturnValue = NLedGetDeviceInfo(NLED_SUPPORTS_INFO_ID, &ledSupportsInfo);

    if(!fReturnValue)
    {
        FAIL("Unable to get the LED supports Info");
        return TPR_FAIL;
    }

    //Testing for Invalid Info ID
    //Initializing the structure members
    fReturnValue = NLedSetDevice(NLED_SUPPORTS_INFO_ID, &ledSettingsInfo_Set);

    //Calling the NledSetDevice function by passing invalid ID
    if(fReturnValue)
    {
        //Logging the output using the kato logging engine
        FAIL("Nled Driver failed to handle an attempt to pass Invalid Info ID");
        dwReturn = TPR_FAIL;
        goto RETURN;
    }
    else
    {
        fReturnValue = NLedSetDevice(NLED_COUNT_INFO_ID, &ledSettingsInfo_Set);

        if(fReturnValue)
        {
            //Logging the output using the kato logging engine
            FAIL("Nled Driver failed to handle an attempt to pass Invalid Info ID");
            dwReturn = TPR_FAIL;
            goto RETURN;
        }

        //Reporting success if the driver handles the above scenario gracefully
        SUCCESS("NLedSetDevice() responded gracefully to an attempt to pass Invalid Info ID");
        dwReturn = TPR_PASS;
    }

    //Testing by passing negative blink settings
    //Initializing the blink parameters with negative values.
    ledSettingsInfo_Set.OffOnBlink = (LED_BLINK * (-1));

    //Setting the blinking parameters
    if(ledSupportsInfo.fAdjustTotalCycleTime)
    {
        ledSettingsInfo_Set.TotalCycleTime = ((ledSupportsInfo.lCycleAdjust) * (-1));
    }//fAdjustTotalCycleTime

    if(ledSupportsInfo.fAdjustOnTime)
    {
        ledSettingsInfo_Set.OnTime = ((ledSupportsInfo.lCycleAdjust) * (-1));
    }//fAdjustOnTime

    if(ledSupportsInfo.fAdjustOffTime)
    {
        ledSettingsInfo_Set.OffTime = ((ledSupportsInfo.lCycleAdjust) * (-1));
    }//fAdjustOffTime

    if(ledSupportsInfo.fMetaCycleOn)
    {
        ledSettingsInfo_Set.MetaCycleOn = (META_CYCLE_ON * (-1));
    }//fMetaCycleOn

    if(ledSupportsInfo.fMetaCycleOff)
    {
        ledSettingsInfo_Set.MetaCycleOff = (META_CYCLE_OFF * (-1));
    }//fMetaCycleOff

    fReturnValue = NLedSetDevice(NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Set);

    //Calling the NLedSetDevice function by passing negative blink settings
    if(fReturnValue)
    {
        //Logging the output using the kato logging engine
        FAIL("Nled Driver did not handle Negative blink settings properly");
        dwReturn = TPR_FAIL;
        goto RETURN;
    }
    else
    {
        //Reporting success if the driver handles the above scenario gracefully
        SUCCESS("NLedSetDevice() responded gracefully to an attempt to pass negative blink settings");
        dwReturn = TPR_PASS;
    }

    //Testing by passing NULL buffer
    //Calling the NledSetDevice function by passing NULL buffer pointer
    fReturnValue = NLedSetDevice(NLED_SETTINGS_INFO_ID, NULL);

    if(fReturnValue)
    {
        //Logging the output using the kato logging engine
        FAIL("Nled Driver failed to handle an attempt to pass a Null buffer pointer");
        dwReturn = TPR_FAIL;
        goto RETURN;
    }
    else
    {
        //Reporting success if the driver handles the above scenario gracefully
        SUCCESS("NLedSetDevice() responded gracefully to an attempt to pass Null buffer pointer");
        dwReturn = TPR_PASS;
    }

RETURN:
    //Restoring the original led settings
    dwReturnValue = RestoreOriginalNledSettings(&originalLedSettingsInfo);

    if(TPR_PASS != dwReturnValue)
    {
        //Logging the error output using the kato logging engine
        FAIL("Nled Driver failed to restore the original NLED settings");
        dwReturn = TPR_FAIL;
    }
    return dwReturn;
}

//*****************************************************************************
TESTPROCAPI VerifyNledSuspendResume(UINT                   uMsg,
                                    TPPARAM                tpParam,
                                    LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED, TPR_SKIP
//
//  This function tests the LED driver's ability to restore the LED state
//  after a device suspend-resume
//
// "Suspend-Resume scenario - If system goes in suspend state and then
//  resumes, the LED status must be restored"
//
// "Suspend-Resume scenario - System goes in suspended state and then
//  resumes. Test IDs 1001 through 1004 must pass."
//
// "If system goes in suspend state and then resumes, the blinking must
//  be restored"
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Only Supporting executing the test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the general or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fGeneralTests) && (!g_fAutomaticTests)) return TPR_SKIP;

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
    DWORD dwTestCount = 0;
    DWORD dwNledSWBlinkCtrlValue = 0;

    //Suspend Resume test for the NLED Driver
    //Checking if the state is retained (Checking for the first LED by turning on the LED)
    dwReturnValue = SuspendResumeDevice(LED_INDEX_FIRST,LED_ON);

    if(TPR_PASS == dwReturnValue)
    {
        SUCCESS("Nled Driver Successfully restores the same state after a device Suspend-Resume");
    }
    else if((ERROR_NOT_SUPPORTED == dwReturnValue) || (ERROR_GEN_FAILURE == dwReturnValue))
    {
        //Logging the error output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Skipping the test: Device does not support Suspend / Resume"));

        return TPR_SKIP;
    }
    else
    {
        //Logging the error output using the kato logging engine
        FAIL("Nled driver fails to restore same LED state after a device suspend-resume");
        return TPR_FAIL;
    }

    //Checking if the basic previous tests are working after a device Suspend Resume
    //Testing the TestGetLEDCount test
    dwReturnValue = TestGetLEDCount(TPM_EXECUTE, NULL, NULL);

    if(TPR_PASS == dwReturnValue)
    {
        SUCCESS("Get NLED Count Info Test Successfully Passed after a Device Suspend Resume");
    }
    else if(TPR_FAIL == dwReturnValue)
    {
        //Logging the error output using the kato logging engine
        FAIL("Get NLED Count Info Test failed after a Device Suspend Resume");
        return TPR_FAIL;
    }
    else
    {
        //Logging the error output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Skipping the Get NLED Count Info Test after a Device Suspend Resume"));

        dwReturnValue = TPR_SKIP;

        //Incrementing the test count implemented in this test proc
        dwTestCount++;
    }

    //Testing the TestGetLEDSupportsInfo test
    dwReturnValue = TestGetLEDSupportsInfo(TPM_EXECUTE, NULL, NULL);

    if(TPR_PASS == dwReturnValue)
    {
        SUCCESS("Get NLED Supports Info Test Successfully Passed after a Device Suspend Resume");
    }
    else if(TPR_FAIL == dwReturnValue)
    {
        //Logging the error output using the kato logging engine
        FAIL("Get NLED Supports Info Test failed after a Device Suspend Resume");
        return TPR_FAIL;
    }
    else
    {
        //Logging the error output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Skipping the Get NLED Supports Info Test after a Device Suspend Resume"));

        dwReturnValue = TPR_SKIP;

        //Incrementing the test count implemented in this test proc
        dwTestCount++;
    }

    //Testing the TestGetLEDSettingsInfo test
    dwReturnValue = TestGetLEDSettingsInfo(TPM_EXECUTE, NULL, NULL);

    if(TPR_PASS == dwReturnValue)
    {
        SUCCESS("Get NLED Settings Info Test Successfully Passed after a Device Suspend Resume");
    }
    else if(TPR_FAIL == dwReturnValue)
    {
        //Logging the error output using the kato logging engine
        FAIL("Get NLED Settings Info Test failed after a Device Suspend Resume");
        return TPR_FAIL;
    }
    else
    {
        //Logging the error output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Skipping the Get NLED Settings Info Test after a Device Suspend Resume"));

        dwReturnValue = TPR_SKIP;

        //Incrementing the test count implemented in this test proc
        dwTestCount++;
    }

    //TestSetLEDSettingsInfo test
    dwReturnValue = TestSetLEDSettingsInfo(TPM_EXECUTE, NULL, NULL);

    if(TPR_PASS == dwReturnValue)
    {
        SUCCESS("Set NLED Settings Info Test  Successfully Passed after a Device Suspend Resume");
    }
    else if(TPR_FAIL == dwReturnValue)
    {
        //Logging the error output using the kato logging engine
        FAIL("Set NLED Settings Info Test  failed after a Device Suspend Resume");
        return TPR_FAIL;
    }
    else
    {
        //Logging the error output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Skipping the Set NLED Settings Info Test after a Device Suspend Resume"));

        dwReturnValue = TPR_SKIP;

        //Incrementing the test count implemented in this test proc
        dwTestCount++;
    }

    //Suspend Resume test for the NLED Driver (Blink Control Library)
    //Checking if the blinking is retained

    //Checking for the first NLED that has Blink Control Library configured
    for(DWORD dwLedCount = 0; dwLedCount < GetLedCount(); dwLedCount++)
    {
        //Checking if 'dwLedCount' NLED has SWBlinkCtrl flag set to 1 and
        //Blinkable flag set to 0
        dwNledSWBlinkCtrlValue = GetSWBlinkCtrlLibraryNLed(dwLedCount);

        if(dwNledSWBlinkCtrlValue == TPR_SKIP)
        {
            //Logging the error output using the kato logging engine
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Test Passed: Blink Control Library is not configured for any of the NLEDs on the device"));

            return TPR_PASS;
        }
        else if(dwNledSWBlinkCtrlValue == TPR_PASS)
        {
            //Suspending the device
            dwReturnValue = SuspendResumeDevice(dwLedCount, LED_BLINK);

            //Checking if the blinking is retained
            if(TPR_PASS == dwReturnValue)
            {
                SUCCESS("Nled Driver Successfully restores the same state after a device Suspend-Resume");
                return TPR_PASS;
            }
            else if((ERROR_NOT_SUPPORTED == dwReturnValue) || (ERROR_GEN_FAILURE == dwReturnValue))
            {
                //Logging the error output using the kato logging engine
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Power management not supported, Device does not support suspend-Resume"));

                //The number of tests implemented above in this test proc are 4
                if((dwReturnValue == TPR_SKIP) && (dwTestCount == 4))
                {
                    //Indicates all the above tests have skipped
                    dwReturnValue = TPR_SKIP;
                }
                else
                {
                    //Indicates not all of the above tests have skipped so passing the test
                    dwReturnValue = TPR_PASS;
                }

                goto RETURN;
            }
            else
            {
                //Logging the error output using the kato logging engine
                FAIL("Nled driver fails to restore same LED state after a device suspend-resume");
                return TPR_FAIL;
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
    return dwReturnValue;
}

//*****************************************************************************
TESTPROCAPI NledCpuUtilizationTest(UINT                   uMsg,
                                   TPPARAM                tpParam,
                                   LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED, TPR_SKIP
//
// "Blink Control Library: Check the CPU utilization when all LEDs are
//  blinking. Increase in CPU utilization must not exceed a pre defined limit."
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Only Supporting executing the test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the pqd and automatic tests are not enabled from the
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
    DWORD dwReturn = TPR_FAIL;
    DWORD dwInitialCPU_Utilization = 0;
    DWORD dwCpuUtilizationValue[4];
    DWORD *pSWBlinkCtrlNledNums = NULL;
    DWORD dwAverageCpuUtilizationValue = 0;
   DWORD dwCurrentCPU_Utilization = 0;

    //Checking if atleast one Nled has SWBlinkCtrl configured
    dwReturn = GetSWBlinkCtrlLibraryNLed(LED_INDEX_FIRST);

    if(dwReturn == TPR_SKIP)
    {
        //Logging the error output using the kato logging engine
        g_pKato->Log(LOG_SKIP,
                     TEXT("Blink Control Library is not configured for any of the NLEDs on the device"));

        return TPR_SKIP;
    }

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
    dwReturn = saveAllLedSettings(pOriginalLedSettingsInfo, LED_OFF);

    if(!dwReturn)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    }

    //Variable to store the cpu utilization value
    dwInitialCPU_Utilization = GetCpuUtilization();
    pSWBlinkCtrlNledNums = new DWORD[GetLedCount()];

    //Getting the Nleds having SWBlinkCtrl enabled
    dwReturn = GetSWBlinkCtrlNleds(pSWBlinkCtrlNledNums);

    if(dwReturn != TPR_PASS)
    {
        //Logging the error output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to Query the SWBlinkCtrl for all the Nleds"));

        dwReturnValue = TPR_FAIL;
        goto RETURN;
    }

    //Calculating the CPU Utilization value 3 times
    for(DWORD i = 0; i < CALCULATE_CPU_UTIL_THREE_TIMES; i++)
    {
        //Blinking all the NLEDs which have the Blink Control Library configured
        for(DWORD dwLedCount = 0; dwLedCount < GetLedCount(); dwLedCount++)
        {
            if(pSWBlinkCtrlNledNums[dwLedCount] == VALID_SWBLINKCTRL)
            {
                //Blink the LED
                if(TPR_PASS != BlinkNled(dwLedCount))
                {
                     g_pKato->Log(LOG_DETAIL,
                                  TEXT("Unable to blink the NLED No. %u through Blink Control Library"), 
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

        //Now all the LEDs that support BlinkCtrl Lib are blinking,
        //calculate the cpu utilization again
        dwCurrentCPU_Utilization = GetCpuUtilization();
        if(dwCurrentCPU_Utilization < dwInitialCPU_Utilization)
        {
            dwCpuUtilizationValue[i] = 0;
        }else
        {
            dwCpuUtilizationValue[i] = dwCurrentCPU_Utilization - dwInitialCPU_Utilization;
        }
    }

    //Calculating the average value for the cpu utilization
    for(DWORD i = 0; i < CALCULATE_CPU_UTIL_THREE_TIMES; i++)
    {
        dwAverageCpuUtilizationValue = dwAverageCpuUtilizationValue + dwCpuUtilizationValue[i];
    }

    //Calculating the average cpu utilization
    dwAverageCpuUtilizationValue = (dwAverageCpuUtilizationValue) / (CALCULATE_CPU_UTIL_THREE_TIMES);

    //Checking if the average CPU utilization value is more than the allowed value
    if(dwAverageCpuUtilizationValue <= g_nMaxCpuUtilization)
    {
        //Logging the output using the kato logging engine
        SUCCESS("The average CPU utilization is less than the reference value");
        dwReturnValue = TPR_PASS;
    }
    else
    {
        //Logging the error output using the kato logging engine
        FAIL("The average CPU utilization is is greater than the reference value");
        g_pKato->Log(LOG_DETAIL,
                     TEXT("The actual calculated CPU Utilization value is %u %% and the reference value is %u %%"), 
                     dwAverageCpuUtilizationValue,
                     g_nMaxCpuUtilization);
        dwReturnValue = TPR_FAIL;
    }

RETURN:

    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo != NULL)
    {
        dwReturn = setAllLedSettings(pOriginalLedSettingsInfo);

        if(!dwReturn)
        {
            FAIL("Failed to restore the previous LED settings");
            dwReturnValue = TPR_FAIL;
        }

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
TESTPROCAPI VerifyNledTotalOnOffTimeSupportsInfo(UINT                   uMsg,
                                                 TPPARAM                tpParam,
                                                 LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED, TPR_SKIP
//
// "The driver must report only the following combinations for
//  blink settings:
//
//  Valid combinations:
//
//    fAdjustTotalCycleTime   = FALSE
//    fAdjustOnTime           = FALSE
//    fAdjustOffTime          = FALSE
//
//    fAdjustTotalCycleTime   = TRUE
//    fAdjustOnTime           = FALSE
//    fAdjustOffTime          = FALSE
//
//    fAdjustTotalCycleTime   = FALSE
//    fAdjustOnTime           = TRUE
//    fAdjustOffTime          = TRUE "
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Only Supporting executing the test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the general or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fGeneralTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    // If there are no NLEDs on the device pass the test
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("There are no NLEDs to Test on the Device. Automatic PASS"));

        return TPR_PASS;
    }

    //Declaring the variables
    DWORD dwReturnValue = TPR_FAIL;
    NLED_SUPPORTS_INFO ledSupportsInfo_Get = {0};

    for(DWORD dwLedCount = 0; dwLedCount < GetLedCount(); dwLedCount++)
    {
        ledSupportsInfo_Get.LedNum = dwLedCount;

        //Calling the NledGetDeviceInfo function to retrieve the supports info
        dwReturnValue = NLedGetDeviceInfo(NLED_SUPPORTS_INFO_ID, &ledSupportsInfo_Get);

        if(dwReturnValue)
        {
            if(!ledSupportsInfo_Get.fAdjustTotalCycleTime)
            {
                if((!ledSupportsInfo_Get.fAdjustOnTime) &&
                   (!ledSupportsInfo_Get.fAdjustOffTime))
                {
                    //Logging the output using the kato logging engine
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("NLedGetDeviceInfo retrieves Correct combination of [TotalCycle][On][Off]Time parameters for Nled No: %u"), 
                                 dwLedCount);

                    dwReturnValue = TPR_PASS;
                }
                else if(ledSupportsInfo_Get.fAdjustOnTime &&
                        ledSupportsInfo_Get.fAdjustOffTime)
                {
                    //Logging the output using the kato logging engine
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("NLedGetDeviceInfo retrieves Correct combination of [TotalCycle][On][Off]Time parameters for Nled No: %u"), 
                                 dwLedCount);

                    dwReturnValue = TPR_PASS;
                }
                else
                {
                    //Logging the output using the kato logging engine
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("NLedGetDeviceInfo retrieves wrong combination of [TotalCycle][On][Off]Time parameters for Nled No: %u"), 
                                 dwLedCount);

                    dwReturnValue = TPR_FAIL;
                    break;
                }
            }
            else if(ledSupportsInfo_Get.fAdjustTotalCycleTime)
            {
                if((!ledSupportsInfo_Get.fAdjustOnTime) &&
                   (!ledSupportsInfo_Get.fAdjustOffTime))
                {
                    //Logging the output using the kato logging engine
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("NLedGetDeviceInfo retrieves Correct combination of [TotalCycle][On][Off]Time parameters for Nled No: %u"), 
                                 dwLedCount);

                    dwReturnValue = TPR_PASS;
                }
                else
                {
                    //Logging the output using the kato logging engine
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("NLedGetDeviceInfo retrieves wrong combination of [TotalCycle][On][Off]Time parameters for Nled No: %u"), 
                                 dwLedCount);

                    dwReturnValue = TPR_FAIL;
                    break;
                }
            }
            else
            {
                //Logging the output using the kato logging engine
                g_pKato->Log(LOG_DETAIL,
                             TEXT("NLedGetDeviceInfo retrieves wrong combination of [TotalCycle][On][Off]Time parameters for Nled No: %u"), 
                             dwLedCount);

                dwReturnValue = TPR_FAIL;
                break;
            }
        }
        else
        {
            //Logging the error output using the kato logging engine
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Nled Driver is unable to retrieve the supports Info parameters for Nled No. %u"), 
                         dwLedCount);

            dwReturnValue = TPR_FAIL;
            break;
        }
    }

    if(TPR_PASS == dwReturnValue)
    {
        //Logging the output using the kato logging engine
        SUCCESS("NLedGetDeviceInfo retrieves correct [TotalCycle][On][Off]Time supports Info parameters");
    }
    else
    {
        //Logging the output using the kato logging engine
        FAIL("NLedGetDeviceInfo retrieves wrong [TotalCycle][On][Off]Time supports Info parameters");
    }

    return dwReturnValue;
}

//*****************************************************************************
TESTPROCAPI VerifyNledMetaCycleSupportsInfo(UINT                   uMsg,
                                            TPPARAM                tpParam,
                                            LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED, TPR_SKIP
//
// "The NledGetDeviceInfo function must return only the following metaCycle
//  combinations:
//
//  fMetaCycleON           =       TRUE
//  fMetaCycleOFF          =       TRUE
//  (OR)
//  fMetaCycleON           =       FALSE
//  fMetaCycleOFF          =       FALSE"
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Only Supporting executing the test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the general or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fGeneralTests) && (!g_fAutomaticTests)) return TPR_SKIP;

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
    NLED_SUPPORTS_INFO ledSupportsInfo_Get = {0};

    for(DWORD dwLedCount = 0; dwLedCount < GetLedCount(); dwLedCount++)
    {
        ledSupportsInfo_Get.LedNum = dwLedCount;

        //Calling the NledGetDeviceInfo function to retrieve the MetaCycle supports info
        dwReturnValue = NLedGetDeviceInfo(NLED_SUPPORTS_INFO_ID, &ledSupportsInfo_Get);

        if(dwReturnValue)
        {
            if(ledSupportsInfo_Get.fMetaCycleOn && ledSupportsInfo_Get.fMetaCycleOff)
            {
                //Logging the output using the kato logging engine
                g_pKato->Log(LOG_DETAIL,
                             TEXT("NLedGetDeviceInfo retrieves correct MetaCycle combination for LED No. %u"), 
                             dwLedCount);

                dwReturnValue = TPR_PASS;
            }
            else if((!ledSupportsInfo_Get.fMetaCycleOn) &&
                    (!ledSupportsInfo_Get.fMetaCycleOff))
            {
                //Logging the output using the kato logging engine
                g_pKato->Log(LOG_DETAIL,
                             TEXT("NLedGetDeviceInfo retrieves correct MetaCycle combination for LED No. %u"), 
                             dwLedCount);

                dwReturnValue = TPR_PASS;
            }
            else
            {
                //Logging the output using the kato logging engine
                g_pKato->Log(LOG_DETAIL,
                             TEXT("NLedGetDeviceInfo retrieves wrong MetaCycle combination for LED No. %u"), 
                             dwLedCount);

                dwReturnValue = TPR_FAIL;
                break;
            }
        }
        else
        {
            //Logging the error output using the kato logging engine
            g_pKato->Log(LOG_DETAIL,
                         TEXT(" Nled Driver is unable to retrieve the MetaCycle supports Info parameters for LED No. %u"), 
                         dwLedCount);

            dwReturnValue = TPR_FAIL;
            break;
        }
    }

    if(TPR_PASS == dwReturnValue)
    {
        //Logging the output using the kato logging engine
        SUCCESS("NLedGetDeviceInfo retrieves correct MetaCycle combination");
    }
    else
    {
        //Logging the output using the kato logging engine
        FAIL("NLedGetDeviceInfo retrieves wrong MetaCycle combination");
    }

    return dwReturnValue;
}

//*****************************************************************************
TESTPROCAPI VerifyNledGetSupportsInfo(UINT                   uMsg,
                                      TPPARAM                tpParam,
                                      LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
// "Call the NLedSetDevice multiple times with different setting
//  combinations and check that it returns true all the time, and also call the
//  NledGetDeviceInfo function each time to get the supports information. The
//  NledGetDeviceInfo function must return true for only the combinations that
//  are supported."
//
// "Test to validate the below Nled supported settings:
//
//  1. The cycle time is not adjustable:
//     fAdjustTotalCycleTime = False
//     fAdjustOnTime == FALSE
//     fAdjustOffTime == FALSE
//
//  2. Only the overall cycle time is adjustable:
//     fAdjustTotalCycleTime = True
//     fAdjustOnTime == FALSE
//     fAdjustOffTime == FALSE
//
//  3. The on and off times are independently adjustable.
//     fAdjustTotalCycleTime = False
//     fAdjustOnTime == TRUE
//     fAdjustOffTime == TRUE"
//
// 4.  fMetaCycleON = TRUE
//     fMetaCycleOFF = TRUE
//
//     OR
//
//  5. fMetaCycleON =  FALSE
//     fMetaCycleOFF = FALSE
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Only Supporting executing the test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Checking if the interactive mode is enabled for this test
    if(g_fInteractive && g_fGeneralTests)
    {
        goto INTERACTIVE_TEST;
    }

    //Skipping the test if the general or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fGeneralTests) && (!g_fAutomaticTests))
    {
        return TPR_SKIP;
    }

INTERACTIVE_TEST:

    // If there are no NLEDs on the device pass the test.
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("There are no NLEDs to Test on the Device. Automatic PASS"));

        return TPR_PASS;
    }

    //Declaring the structures
    NLED_SUPPORTS_INFO ledSupportsInfo_Get = {0};

    //Declaring the variables
    DWORD dwRetValue = TPR_PASS;
    DWORD dwTempRetValue = 0;

    //Testing for all the NLEDs
    for(DWORD dwLedCount = 0; dwLedCount < GetLedCount(); dwLedCount++)
    {
        ledSupportsInfo_Get.LedNum = dwLedCount;

        //Calling the NledGetDeviceInfo function to retrieve the supports info
        dwTempRetValue = NLedGetDeviceInfo(NLED_SUPPORTS_INFO_ID, 
                                           &ledSupportsInfo_Get);

        if(dwTempRetValue)
        {
            if(!ledSupportsInfo_Get.fAdjustTotalCycleTime)
            {
                if((!ledSupportsInfo_Get.fAdjustOnTime) &&
                   (!ledSupportsInfo_Get.fAdjustOffTime))
                {
                    if(((ledSupportsInfo_Get.fMetaCycleOn) &&
                        (ledSupportsInfo_Get.fMetaCycleOff)) ||
                       ((!ledSupportsInfo_Get.fMetaCycleOn) &&
                       (!ledSupportsInfo_Get.fMetaCycleOff)))
                    {
                        //If the user has enabled the interactive mode
                        //Testing the validity of NledGetDeviceInfo
                        if(g_fInteractive)
                        {
                            dwTempRetValue = GetUserResponse(ledSupportsInfo_Get);

                            // Changing the value of dwRetValue only in case of a failure since
                            // the test is to be executed for all NLEDs and it is to be failed
                            // even if the supports info are wrong for even one NLED
                            if(dwTempRetValue == TPR_FAIL)
                            {
                                dwRetValue = TPR_FAIL;
                            }
                        }
                        else
                        {
                            //Logging the output using the kato logging engine
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("NLedGetDeviceInfo retrieves correct combination of supports Info for LED %u"), 
                                         dwLedCount);
                        }
                    }
                }
                else if(ledSupportsInfo_Get.fAdjustOnTime &&
                        ledSupportsInfo_Get.fAdjustOffTime)
                {
                    if((ledSupportsInfo_Get.fMetaCycleOn &&
                        ledSupportsInfo_Get.fMetaCycleOff) ||
                      ((!ledSupportsInfo_Get.fMetaCycleOn) &&
                      (!ledSupportsInfo_Get.fMetaCycleOff)))
                    {
                        //If the user has enabled the interactive mode
                        //Testing the validity of NledGetDeviceInfo
                        if(g_fInteractive)
                        {
                            dwTempRetValue = GetUserResponse(ledSupportsInfo_Get);

                            // Changing the value of dwRetValue only in case of a failure since
                            // the test is to be executed for all NLEDs and it is to be failed
                            // even if the supports info are wrong for even one NLED
                            if(TPR_FAIL == dwTempRetValue)
                            {
                                dwRetValue = TPR_FAIL;
                            }
                        }
                        else
                        {
                            //Logging the output using the kato logging engine
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("NLedGetDeviceInfo retrieves correct combination of supports Info for LED %u"), 
                                         dwLedCount);
                        }
                    }
                }
                else
                {
                    //Logging the output using the kato logging engine
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("NLedGetDeviceInfo retrieves wrong combination of supports Info for LED No. %u"), 
                                 dwLedCount);

                    dwRetValue = TPR_FAIL;
                }
            }
            else if(ledSupportsInfo_Get.fAdjustTotalCycleTime)
            {
                if((!ledSupportsInfo_Get.fAdjustOnTime) &&
                  (!ledSupportsInfo_Get.fAdjustOffTime))
                {
                    if((ledSupportsInfo_Get.fMetaCycleOn &&
                        ledSupportsInfo_Get.fMetaCycleOff) ||
                      ((!ledSupportsInfo_Get.fMetaCycleOn) &&
                      (!ledSupportsInfo_Get.fMetaCycleOff)))
                    {
                        //If the user has enabled the interactive mode
                        //Testing the validity of NledGetDeviceInfo
                        if(g_fInteractive)
                        {
                            dwTempRetValue = GetUserResponse(ledSupportsInfo_Get);

                            // Changing the value of dwRetValue only in case of a failure since
                            // the test is to be executed for all NLEDs and it is to be failed
                            // even if the supports info are wrong for even one NLED
                            if(TPR_FAIL == dwTempRetValue)
                            {
                                dwRetValue = TPR_FAIL;
                            }
                        }
                        else
                        {
                            //Logging the output using the kato logging engine
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("NLedGetDeviceInfo retrieves correct combination of supports Info for LED %u"), 
                                         dwLedCount);
                        }
                    }
                }
                else
                {
                    //Logging the output using the kato logging engine
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("NLedGetDeviceInfo retrieves wrong combination of supports Info for LED No. %u"), 
                                 dwLedCount);

                    dwRetValue = TPR_FAIL;
                }
            }
            else
            {
                //Logging the output using the kato logging engine
                g_pKato->Log(LOG_DETAIL,
                             TEXT("NLedGetDeviceInfo retrieves wrong combination of supports Info for LED No. %u"), 
                             dwLedCount);

                dwRetValue = TPR_FAIL;
            }
        }
        else
        {
            //Logging the error output using the kato logging engine
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Nled driver is unable to retrieve the supports Info for NLED %u"), 
                         dwLedCount);

            dwRetValue = TPR_FAIL;
            break;
        }
    }

    //Logging the Result
    if(TPR_PASS == dwRetValue)
    {
        SUCCESS("NLedGetDeviceInfo retrieves correct combination of supports Info parameters");
    }
    else
    {
        FAIL("NLedGetDeviceInfo retrieves wrong combination of supports Info parameters");
    }

    return dwRetValue;
}

//*****************************************************************************
TESTPROCAPI VerifyNledIndex(UINT                   uMsg,
                            TPPARAM                tpParam,
                            LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//    Check that the index starts from 0 and is sequential
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
// "If the system has one or more notification devices, they must be
//  identifiable by consecutive integer index numbers. The first notification
//  device should have an index 0."
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Only Supporting executing the test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the general or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fGeneralTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    // If there are no NLEDs on the device pass the test.
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("There are no NLEDs to Test on the Device. Automatic PASS"));

        return TPR_PASS;
    }

    //Declaring the variables
    DWORD dwReturnValue = 0;
    DWORD dwReturn = 0;
    BOOL fStatus = TRUE;
    BOOL fFirstLedIndex = FALSE;

    //Declaring the settings Info structures
    NLED_SETTINGS_INFO ledSettingsInfo_Set = {0};
    NLED_SETTINGS_INFO ledSettingsInfo_Get = {0};

    //Storing the original LED settings
    NLED_SETTINGS_INFO *pOriginalLedSettingsInfo = NULL;

    //Allocate space to save origional LED setttings.
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
    dwReturnValue = saveAllLedSettings(pOriginalLedSettingsInfo, LED_OFF);

    if(!dwReturnValue)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    }

    for(UINT nCount = 0; nCount < GetLedCount(); nCount++)
    {
        ledSettingsInfo_Set.LedNum = nCount;
        ledSettingsInfo_Get.LedNum = nCount;

        //Turning on the NLEDs
        ledSettingsInfo_Set.OffOnBlink = LED_ON;

        //Calling NledSetDevice to turn ON the NLED
        dwReturnValue = NLedSetDevice(NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Set);

        if(!dwReturnValue)
        {
            //Logging the error output using the kato logging engine
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Nled Driver failed to turn on the NLED No : %u"),
                         nCount);

            //nled index is not sequential
            fStatus = FALSE;
            break;
        }

        //Calling NledGetDevice to get the settings Info
        dwReturnValue = NLedGetDeviceInfo(NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Get);

        if(!dwReturnValue)
        {
            //Logging the error output using the kato logging engine
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Nled Driver failed to retrieve settings Info for the NLED No : %u"),
                         nCount);

            //nled index not sequential
            fStatus = FALSE;
            break;
        }

        //Checking if the first LED has a index equal to zero
        if((!fFirstLedIndex) && (LED_INDEX_FIRST == nCount))
        {
            if((LED_INDEX_FIRST == ledSettingsInfo_Get.LedNum) &&
               (LED_ON == ledSettingsInfo_Get.OffOnBlink))
            {
                //The First NLed has a index equal to '0'
                g_pKato->Log(LOG_DETAIL, TEXT("The first LED index is '0'"));
                fFirstLedIndex = TRUE;
            }
            else
            {
                //The First NLed does not have index equal to '0'
                g_pKato->Log(LOG_DETAIL,
                             TEXT("The first LED does not have a LED index equal to '0'"));
                break;
            }
        }
        if(ledSettingsInfo_Get.OffOnBlink != LED_ON)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("The state set for NLED : %u by NledSetDevice and state retrieved by NledGetDeviceInfo do not match"), 
                         nCount);

            //nled index not sequential
            fStatus = FALSE;
            break;
        }
    }

    if(fStatus && fFirstLedIndex)
    {
        //Logging the error output using the kato logging engine
        SUCCESS("The Nled index starts at zero and is sequential");
        dwReturnValue = TPR_PASS;
    }
    else if (fFirstLedIndex)
    {
        //Logging the error output using the kato logging engine
        FAIL("The Nled index starts at zero but is not sequential");
        dwReturnValue = TPR_FAIL;
    }
    else if (fStatus)
    {
        //Logging the error output using the kato logging engine
        FAIL("The Nled index does not start at zero but is sequential");
        dwReturnValue = TPR_FAIL;
    }
    else
    {
        //Logging the error output using the kato logging engine
        FAIL("The Nled index does not start at zero is also not sequential");
        dwReturnValue = TPR_FAIL;
    }

    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo)
    {
        dwReturn= setAllLedSettings(pOriginalLedSettingsInfo);

        if(!dwReturn)
        {
            dwReturnValue = TPR_FAIL;
            FAIL("Unable to restore original NLED settings");
        }

        //De-allocating the memory
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    return dwReturnValue;
}

//*****************************************************************************
TESTPROCAPI VerifyNledType(UINT                   uMsg,
                           TPPARAM                tpParam,
                           LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
// "Call GetDeviceInfo to retrieve the type of the Notification device.
//  The function must return the corresponding LedType_t value for"Led" or
// "Vibrator".
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Only Supporting executing the test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Checking if the interactive mode is enabled for this test
    if(g_fInteractive && g_fGeneralTests)
    {
        goto INTERACTIVE_TEST;
    }

    //Skipping the test if the general and also automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fGeneralTests) && (!g_fAutomaticTests)) return TPR_SKIP;

INTERACTIVE_TEST:

    // If there are no NLEDs on the device pass the test.
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("There are no NLEDs to Test on the Device. Automatic PASS"));

        return TPR_PASS;
    }

    // Declaring the variables
    NLED_TYPE_INFO *ledTypeInfo_Get = new NLED_TYPE_INFO[1];
    ZeroMemory(ledTypeInfo_Get, sizeof(NLED_TYPE_INFO));

    DWORD dwReturnValue = TPR_PASS;
    BOOL fReturn = FALSE;
    DWORD dwMboxValue = 0;
    UINT nLedType = 0;
    TCHAR szMessage[ 50 ] = TEXT("");

    for(DWORD dwCount = 0; dwCount < GetLedCount(); dwCount++)
    {
        ledTypeInfo_Get->LedNum = dwCount;

        //Calling NledGetDeviceInfo to get the ledType
        fReturn = NLedGetDeviceInfo(NLED_TYPE_INFO_ID, ledTypeInfo_Get);

        if(fReturn)
        {
            //Getting the led type
            nLedType = (UINT) ledTypeInfo_Get->LedType;

            switch(nLedType)
            {
                case LED_TYPE_UNKNOWN:
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("The NLed %u is a unknown Nled"), 
                                     dwCount);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;

                case LED_TYPE_NLED:

                        if(g_fInteractive)
                        {
                            swprintf_s(szMessage, _countof(szMessage), 
                                      TEXT("The NLed %u is a LED. Is this correct?"), 
                                      dwCount);

                            dwMboxValue = MessageBox(NULL,
                                                     szMessage,
                                                     g_szInputMsg,
                                                     MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND);

                            if(IDYES == dwMboxValue)
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("\tUser confirmed the %u NLED Type."), 
                                             dwCount);
                            }
                            else
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("\tUser says %u NLED Type is incorrect."), 
                                             dwCount);

                                dwReturnValue = TPR_FAIL;
                            }
                        }
                        else
                        {
                            g_pKato->Log(LOG_DETAIL,
                            TEXT("The NLed %u is a LED"), dwCount);
                        }

                        break;

                case LED_TYPE_VIBRATOR:

                        if(g_fInteractive)
                        {
                            swprintf_s(szMessage, _countof(szMessage), 
                                      TEXT("The NLed %u is a Vibrator. Is this correct?"), 
                                      dwCount);

                            dwMboxValue = MessageBox(NULL,
                                                     szMessage,
                                                     g_szInputMsg,
                                                     MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND);

                            if(IDYES == dwMboxValue)
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("\tUser confirmed the %u NLED Type."), 
                                             dwCount);
                            }
                            else
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("\tUser says %u NLED Type is incorrect."), 
                                             dwCount);

                                dwReturnValue = TPR_FAIL;
                            }
                        }
                        else
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("The NLed %u is a Vibrator"), 
                                         dwCount);
                        }

                        break;

                case LED_TYPE_INVALID:
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("The NLed %u has a invalid LedType"), 
                                     dwCount);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;

                default:
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("The LedType %u for LED %u is unknown/invalid"), 
                                     nLedType, 
                                     dwCount);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
            }
        }
        else
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("The Nled driver failed to retrieve the LedType for Led %u"), 
                         dwCount);

            dwReturnValue = TPR_FAIL;
        }
    }

RETURN:
    if(ledTypeInfo_Get != NULL)
    {
        delete [] ledTypeInfo_Get;
        ledTypeInfo_Get = NULL;
    }

    if(TPR_PASS == dwReturnValue)
    {
        SUCCESS("Correctly verified the LED type for LED and Vibrator");

        //Returning Pass if the LedType_t is verified by the User
        return TPR_PASS;
    }

    FAIL("Error in verifying the LED type for LED and Vibrator");
    return dwReturnValue;
}

//*****************************************************************************
TESTPROCAPI NledGroupingTest(UINT                   uMsg,
                             TPPARAM                tpParam,
                             LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
// "Turn on all the Nleds present in any group and verify that only
//  one NLED is in active state at a given time"
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
    DWORD dwReturn = TPR_FAIL;
    DWORD dwFinalGroupId = 0;
    DWORD dwFinalGroupCount = 0;
    DWORD dwTempGroupId = 0;
    DWORD dwTempGroupCount = 0;
    DWORD dwOnLedCount = 0;
    DWORD* pLedNumGroupId = NULL;

    //Declaring the settings info structure
    NLED_SETTINGS_INFO ledSettingsInfo_Set = {0};
    NLED_SETTINGS_INFO ledSettingsInfo_Get = {0};

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
    dwReturn = saveAllLedSettings(pOriginalLedSettingsInfo, LED_OFF);

    if(!dwReturn)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        return TPR_FAIL;
    }

    //Pointer to a Array to store the Group Id for the all the Leds
    pLedNumGroupId = new DWORD[GetLedCount()];

    //Getting the Group Ids for the Nleds
    dwReturn = GetNledGroupIds(pLedNumGroupId);

    if(TPR_PASS == dwReturn)
    {
        //Finding the group which has more NLeds
        for(DWORD dwLedNum = 0; dwLedNum < GetLedCount(); dwLedNum++)
        {
            dwTempGroupCount = 0;

           if(pLedNumGroupId[dwLedNum] != 0)
           {
              dwTempGroupId = pLedNumGroupId[dwLedNum];

              //Finding the occurances of this groupId in the rest of the list
              for(DWORD dwLedNumCount = 0; dwLedNumCount < GetLedCount(); dwLedNumCount++)
              {
                  if(dwTempGroupId == pLedNumGroupId[dwLedNumCount])
                  {
                      dwTempGroupCount++;
                  }
              }

              //Selecting the Group which has more LEDs
              if(dwTempGroupCount > dwFinalGroupCount)
              {
                 dwFinalGroupCount = dwTempGroupCount;
                 dwFinalGroupId = dwTempGroupId;
              }
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

    //Checking if any LED is present in any group
    if(dwFinalGroupId > 0)
    {
        //Turning on the Leds belonging to the 'dwFinalGroupId' group Id
        ledSettingsInfo_Set.OffOnBlink = LED_ON;

         for(DWORD dwLedCount = 0; dwLedCount < GetLedCount(); dwLedCount++)
         {
             if(dwFinalGroupId == pLedNumGroupId[dwLedCount])
             {
                ledSettingsInfo_Set.LedNum = dwLedCount;
                dwReturnValue = NLedSetDevice(NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Set);

                if(!dwReturnValue)
                {
                    //Logging the output using the kato logging engine
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to turn On the Nled %u"), 
                                 dwLedCount);

                    dwReturnValue = TPR_FAIL;
                    goto RETURN;
                }
             }
         }

         //Checking that only one LED is in ON state in that group
         for(DWORD dwLedIndex = 0; dwLedIndex < GetLedCount(); dwLedIndex++)
         {
            if(dwFinalGroupId == pLedNumGroupId[dwLedIndex])
            {
               ledSettingsInfo_Get.LedNum = dwLedIndex;
               dwReturnValue = NLedGetDeviceInfo(NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Get);

               if(!dwReturnValue)
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
               }
            }
         }

         //Checking if only one LED was ON
         if(ONE_LED == dwOnLedCount)
         {
             dwReturnValue = TPR_PASS;
         }
    }
    else
    {
        //No Led Present in any Group
        dwReturnValue = TPR_PASS;
    }

RETURN:
    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo != NULL)
    {
        dwReturn= setAllLedSettings(pOriginalLedSettingsInfo);

        if(!dwReturn)
        {
            dwReturnValue = TPR_FAIL;
            FAIL("Unable to restore original NLED settings");
        }

        //De-allocating the memory
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    if(pLedNumGroupId != NULL)
    {
        delete[] pLedNumGroupId;
        pLedNumGroupId = NULL;
    }

    //Returning the status
    if((0 == dwFinalGroupCount) && (TPR_PASS == dwReturnValue))
    {
        SUCCESS("No Nled is present as part of any Group.");
        return TPR_PASS;

    }
    else if((ONE_LED == dwFinalGroupCount) && (TPR_PASS == dwReturnValue))
    {
        SUCCESS("No two Nleds are present as part of same Group. Test Passes");
        return TPR_PASS;

    }
    else if(TPR_PASS == dwReturnValue)
    {
        SUCCESS("Only One Nled is in active state at anypoint in a group");
        return TPR_PASS;
    }
    else
    {
        FAIL("More than One Nled is in active state at anypoint in a group");
        return TPR_FAIL;
    }
}

//*****************************************************************************
TESTPROCAPI VerifyNledRegistrySettings(UINT                   uMsg,
                                       TPPARAM                tpParam,
                                       LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//  
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
// "If registry is being used to configure the LEDs for a platform, then
//  for each notification device, following entries are must:
//  1) LED index
//  2) LED Type
//  3) Blinking capability"
//
// "The registry settings for the driver and service must be complete,
//  consistent and correct."
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
    DWORD dwConfigKeyResult=0;
    HKEY hConfigKey = NULL;
    HKEY hLedNameKey = NULL;
    DWORD dwReturnValue = ERROR_SUCCESS;
    DWORD dwLedNum = 0;
    DWORD dwLedType = 0;
    DWORD dwBlinkable = 0;
    DWORD dwLedCount = 0;
    DWORD dwLedValueCount = 0;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLedNameSize=gc_dwMaxKeyNameLength + 1;
    TCHAR szValueName[gc_dwMaxKeyNameLength + 1];
    DWORD dwValueNameSize=gc_dwMaxKeyNameLength + 1;
    DWORD dwLen = sizeof(DWORD);
    DWORD dwEventResult = 0;
    DWORD dwLedNameKeyResult = 0;
    DWORD dwEventValueResult = 0;
    HKEY  hEventNameKey = NULL;
    DWORD dwEventCount = 0;
    TCHAR szEventName[gc_dwMaxKeyNameLength + 1];
    DWORD dwEventNameSize=gc_dwMaxKeyNameLength + 1;
    DWORD dwRoot = 0;
    DWORD dwOperator = 0;
    DWORD dwState = 0;
    DWORD dwEventValueCount = 0;
    TCHAR szEventValueName[gc_dwMaxKeyNameLength + 1];
    DWORD dwEventValueNameSize=gc_dwMaxKeyNameLength + 1;

    //Opening the key
    dwReturnValue = RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                                 REG_NLED_KEY_CONFIG_PATH, 
                                 0, 
                                 KEY_ENUMERATE_SUB_KEYS,
                                 &hConfigKey);

    if(ERROR_SUCCESS == dwReturnValue)
    {
        dwConfigKeyResult=RegEnumKeyEx(hConfigKey,
                                       dwLedCount,
                                       szLedName,
                                       &dwLedNameSize,
                                       NULL, 
                                       NULL, 
                                       NULL, 
                                       NULL);

        if(ERROR_NO_MORE_ITEMS == dwConfigKeyResult)
        {
            FAIL("No LEDs configured in the Registry");
            RegCloseKey(hConfigKey);
            return TPR_FAIL;
         }

        while(ERROR_NO_MORE_ITEMS != dwConfigKeyResult)
        {
            //Opening the LedName key
            dwReturnValue = RegOpenKeyEx(hConfigKey, 
                                         szLedName, 
                                         0, 
                                         KEY_ALL_ACCESS, 
                                         &hLedNameKey);

            if(ERROR_SUCCESS == dwReturnValue)
            {
                //Checking if the registry is configured correctly, completely and consistently
                //Checking for LedNum
                dwReturnValue = RegQueryValueEx(hLedNameKey, 
                                                REG_LED_NUM_VALUE, 
                                                NULL, 
                                                NULL, 
                                                (LPBYTE) &dwLedNum, 
                                                &dwLen);

                if(ERROR_SUCCESS == dwReturnValue)
                {
                    if(!((dwLedNum >= LED_INDEX_FIRST) && (dwLedNum < GetLedCount())))
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("The NLED %s does not have the correct LedNum"), 
                                     szLedName);

                        dwReturnValue = TPR_FAIL;
                        goto CLEANUP;
                    }
                }
                else if(ERROR_FILE_NOT_FOUND == dwReturnValue)
                {
                    //Logging the error message using the kato logging engine
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("The NLED %s does not have the mandatory LedNum registry setting"), 
                                 szLedName);

                    FAIL("Test fails as all LEDs do not posess the mandatory registry settings");
                    dwReturnValue = TPR_FAIL;
                    goto CLEANUP;
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Querying for the mandatory registry settings for NLED %u failed"), 
                                 dwLedCount);

                    FAIL("Queriying for the mandatory settings for NLED failed");
                    dwReturnValue = TPR_FAIL;
                    goto CLEANUP;
                }

                //Checking for LedType
                dwReturnValue = RegQueryValueEx(hLedNameKey, 
                                                REG_LED_TYPE_VALUE, 
                                                NULL,
                                                NULL, 
                                                (LPBYTE) &dwLedType, &dwLen);

                if(ERROR_SUCCESS == dwReturnValue)
                {
                    if(!((dwLedType >= LED_TYPE_UNKNOWN) && 
                         (dwLedType <= LED_TYPE_INVALID)))
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("The NLED %u does not have the correct LedType"), 
                                     dwLedNum);

                        dwReturnValue = TPR_FAIL;
                        goto CLEANUP;
                    }
                }
                else if(ERROR_FILE_NOT_FOUND == dwReturnValue)
                {
                    //Logging the error message using the kato logging engine
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("The NLED %u does not have the mandatory LedType registry setting"), 
                                 dwLedNum);

                    FAIL("Test fails as all LEDs do not posess the mandatory registry settings");
                    dwReturnValue = TPR_FAIL;
                    goto CLEANUP;
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Querying for the mandatory registry settings for NLED %u failed"), 
                                 dwLedNum);

                    FAIL("Queriying for the mandatory settings for NLED failed");
                    dwReturnValue = TPR_FAIL;
                    goto CLEANUP;
                }

                //Checking for Blinkable flag
                dwReturnValue = RegQueryValueEx(hLedNameKey, 
                                                REG_BLINKABLE_VALUE, 
                                                NULL, 
                                                NULL, 
                                                (LPBYTE) &dwBlinkable, 
                                                &dwLen);

                if(ERROR_SUCCESS == dwReturnValue)
                {
                    if(!((ZERO_BLINKABLE == dwBlinkable) || 
                         (VALID_BLINKABLE == dwBlinkable)))
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("The NLED %u does not have the correct Blinkable flag setting"), 
                                     dwLedNum);

                        dwReturnValue = TPR_FAIL;
                        goto CLEANUP;
                    }
                }
                else if(ERROR_FILE_NOT_FOUND == dwReturnValue)
                {
                    //Logging the error message using the kato logging engine
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("The NLED %u does not have the mandatory Blinkable registry setting"), 
                                 dwLedNum);

                    FAIL("Test fails as all LEDs do not posess the mandatory registry settings");
                    dwReturnValue = TPR_FAIL;
                    goto CLEANUP;
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Querying for the mandatory registry settings for NLED %u failed"), 
                                 dwLedNum);

                    FAIL("Queriying for the mandatory settings for NLED failed");
                    dwReturnValue = TPR_FAIL;
                    goto CLEANUP;
                }

                //Clearing the dwLedValueCount count for the next iterations
                dwLedValueCount = 0;

                //Checking for consistent registry settings
                dwLedNameKeyResult = RegEnumValue(hLedNameKey, 
                                                  dwLedValueCount, 
                                                  szValueName,
                                                  &dwValueNameSize, 
                                                  NULL, 
                                                  NULL, 
                                                  NULL, 
                                                  NULL);

                while(ERROR_NO_MORE_ITEMS != dwLedNameKeyResult)
                {
                    //Checking that the Nled has valid feilds
                    if(!((ERROR_SUCCESS == _tcscmp(szValueName,REG_LED_NUM_VALUE)) ||
                         (ERROR_SUCCESS == _tcscmp(szValueName,REG_LED_TYPE_VALUE)) ||
                         (ERROR_SUCCESS == _tcscmp(szValueName,REG_BLINKABLE_VALUE)) ||
                         (ERROR_SUCCESS == _tcscmp(szValueName,REG_SWBLINKCTRL_VALUE)) ||
                         (ERROR_SUCCESS == _tcscmp(szValueName,REG_LED_GROUP_VALUE)) ||
                         (ERROR_SUCCESS == _tcscmp(szValueName,REG_ADJUSTTYPE_VALUE)) ||
                         (ERROR_SUCCESS == _tcscmp(szValueName,REG_METACYCLE_VALUE)) ||
                         (ERROR_SUCCESS == _tcscmp(szValueName,REG_CYCLEADJUST_VALUE))))
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("The NLED %u has invalid sub-keys, the registry is not consistent"), 
                                     dwLedNum);

                        dwReturnValue = TPR_FAIL;
                        FAIL("Registry is not consistent");
                        goto CLEANUP;
                    }

                    //Incrementing the count
                    dwLedValueCount++;

                    //Resetting the buffer size
                    dwValueNameSize=gc_dwMaxKeyNameLength + 1;
                    dwLedNameKeyResult=RegEnumValue(hLedNameKey,
                                                    dwLedValueCount, 
                                                    szValueName,
                                                    &dwValueNameSize, 
                                                    NULL, 
                                                    NULL, 
                                                    NULL, 
                                                    NULL);
                }

                //Checking for the completness and correctness for the service registry

                //Clearing the dwEventCount count for the next iterations
                dwEventCount = 0;

                dwEventResult=RegEnumKeyEx(hLedNameKey,
                                           dwEventCount,
                                           szEventName,
                                           &dwEventNameSize,
                                           NULL, 
                                           NULL, 
                                           NULL, 
                                           NULL);

                if(ERROR_NO_MORE_ITEMS == dwEventResult)
                {
                    g_pKato->Log(LOG_DETAIL, 
                                 TEXT("No Events configured for NLED %u"), 
                                 dwLedNum);
                }

                while(ERROR_NO_MORE_ITEMS != dwEventResult)
                {
                    //opening the Led Event Key
                    dwReturnValue = RegOpenKeyEx(hLedNameKey, 
                                                 szEventName, 
                                                 0, 
                                                 KEY_ALL_ACCESS, 
                                                 &hEventNameKey);

                    if(ERROR_SUCCESS == dwReturnValue)
                    {
                        //Checking for correct mandatory registry settings for Service
                        //Checking for Root field
                        dwReturnValue = RegQueryValueEx(hEventNameKey, 
                                                        REG_EVENT_ROOT_VALUE, 
                                                        NULL, 
                                                        NULL, 
                                                        (LPBYTE) &dwRoot, 
                                                        &dwLen);

                        if(ERROR_SUCCESS == dwReturnValue)
                        {
                            //0x80000000, 0x80000001, 0x80000002, 0x80000004 are
                            //valid Root values

                            if(!((dwRoot == SERVICE__EVENT_BASE_ROOT_VALUE) || 
                                 (dwRoot == (SERVICE__EVENT_BASE_ROOT_VALUE + 1)) ||
                                 (dwRoot == (SERVICE__EVENT_BASE_ROOT_VALUE + 2)) || 
                                 (dwRoot == (SERVICE__EVENT_BASE_ROOT_VALUE + 4))))
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("The NLED %u does not have the correct Root field setting for Event %s"), 
                                             dwLedNum,
                                             szEventName);

                                dwReturnValue = TPR_FAIL;
                                goto CLEANUP;
                            }
                        }
                        else if(ERROR_FILE_NOT_FOUND == dwReturnValue)
                        {
                            //Logging the error message using the kato logging engine
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("The NLED %u does not have the Root field for Event %s"), 
                                         dwLedNum, 
                                         szEventName);

                            dwReturnValue = TPR_FAIL;
                            goto CLEANUP;
                        }
                        else
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Querying for the Root registry field for Event %s for NLED %u failed"), 
                                         szEventName, 
                                         dwLedNum);

                            dwReturnValue = TPR_FAIL;
                            goto CLEANUP;
                        }

                        //Checking for Operator field
                        dwReturnValue = RegQueryValueEx(hEventNameKey, 
                                                        REG_EVENT_OPERATOR_VALUE, 
                                                        NULL, 
                                                        NULL, 
                                                        (LPBYTE) &dwOperator, 
                                                        &dwLen);

                        if(ERROR_SUCCESS == dwReturnValue)
                        {
                            if(!((dwOperator >= LEAST_OPERATOR_VALUE) && 
                                 (dwOperator <= MAX_OPERATOR_VALUE)))
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("The NLED %u does not have the correct Operator field setting for %s"), 
                                             dwLedNum, 
                                             szEventName);

                                dwReturnValue = TPR_FAIL;
                                goto CLEANUP;
                            }
                        }
                        //Operator field is Optional
                        else if(ERROR_FILE_NOT_FOUND != dwReturnValue)
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Querying for the Operator registry field for Event %s for NLED %u failed"), 
                                         szEventName, 
                                         dwLedNum);

                            dwReturnValue = TPR_FAIL;
                            goto CLEANUP;
                        }

                        //Checking for state field
                        dwReturnValue = RegQueryValueEx(hEventNameKey, 
                                                        REG_EVENT_STATE_VALUE,
                                                        NULL, 
                                                        NULL,
                                                        (LPBYTE) &dwState,
                                                        &dwLen);

                        if(ERROR_SUCCESS == dwReturnValue)
                        {
                            if(!((dwState >= LED_OFF) && (LED_BLINK >= dwState)))
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("The NLED %u does not have the correct state field setting for %s"), 
                                             dwLedNum, 
                                             szEventName);

                                dwReturnValue = TPR_FAIL;
                                goto CLEANUP;
                            }
                        }
                        else if(ERROR_FILE_NOT_FOUND == dwReturnValue)
                        {
                            //Logging the error message using the kato logging engine
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("The NLED %u does not have the State field for Event %s"), 
                                         dwLedNum, 
                                         szEventName);

                            dwReturnValue = TPR_FAIL;
                            goto CLEANUP;
                        }
                        else
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Querying for the state registry field for Event %s for NLED %u failed"), 
                                         szEventName, 
                                         dwLedNum);

                            dwReturnValue = TPR_FAIL;
                            goto CLEANUP;
                        }

                        //Checking for complete mandatory registry settings
                        //Checking for state field
                        dwReturnValue = RegQueryValueEx(hEventNameKey, 
                                                        REG_EVENT_PATH_VALUE, 
                                                        NULL,
                                                        NULL, 
                                                        NULL, 
                                                        NULL);

                        if(ERROR_FILE_NOT_FOUND == dwReturnValue)
                        {
                            //Logging the error message using the kato logging engine
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("The NLED %u does not have the Path field for Event %s"), 
                                         dwLedNum, 
                                         szEventName);

                            dwReturnValue = TPR_FAIL;
                            goto CLEANUP;
                        }
                        else if(ERROR_SUCCESS != dwReturnValue)
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Querying for the path registry field for Event %s for NLED %u failed"), 
                                         szEventName, 
                                         dwLedNum);

                            dwReturnValue = TPR_FAIL;
                            goto CLEANUP;
                        }

                        //Checking for Mask field
                        dwReturnValue = RegQueryValueEx(hEventNameKey, 
                                                        REG_EVENT_MASK_VALUE, 
                                                        NULL,
                                                        NULL, 
                                                        NULL, 
                                                        NULL);

                        if(ERROR_FILE_NOT_FOUND == dwReturnValue)
                        {
                            //Logging the error message using the kato logging engine
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("The NLED %u does not have the Mask field for Event %s"), 
                                         dwLedNum, 
                                         szEventName);

                            dwReturnValue = TPR_FAIL;
                            goto CLEANUP;
                        }
                        else if(ERROR_SUCCESS != dwReturnValue)
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Querying for the Mask registry field for Event %s for NLED %u failed"), 
                                         szEventName, 
                                         dwLedNum);

                            dwReturnValue = TPR_FAIL;
                            goto CLEANUP;
                        }

                        //Checking for Value field
                        dwReturnValue = RegQueryValueEx(hEventNameKey,
                                                        REG_EVENT_VALUE_VALUE, 
                                                        NULL, 
                                                        NULL,
                                                        NULL, 
                                                        NULL);

                        if(ERROR_FILE_NOT_FOUND == dwReturnValue)
                        {
                            //Logging the error message using the kato logging engine
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("The NLED %u does not have the Value field for Event %s"), 
                                         dwLedNum, 
                                         szEventName);

                            dwReturnValue = TPR_FAIL;
                            goto CLEANUP;
                        }
                        else if(ERROR_SUCCESS != dwReturnValue)
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Querying for the Value registry field for Event %s for NLED %u failed"), 
                                         szEventName, 
                                         dwLedNum);

                            dwReturnValue = TPR_FAIL;
                            goto CLEANUP;
                        }

                        //Checking for Prio field
                        dwReturnValue = RegQueryValueEx(hEventNameKey, 
                                                        REG_EVENT_PRIO_VALUE, 
                                                        NULL,
                                                        NULL, 
                                                        NULL, 
                                                        NULL);

                        if(ERROR_FILE_NOT_FOUND == dwReturnValue)
                        {
                            //Logging the error message using the kato logging engine
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("The NLED %u does not have the Priority field for Event %s"), 
                                         dwLedNum, 
                                         szEventName);

                            dwReturnValue = TPR_FAIL;
                            goto CLEANUP;
                        }
                        else if(ERROR_SUCCESS != dwReturnValue)
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Querying for the Priority registry field for Event %s for NLED %u failed"), 
                                         szEventName, 
                                         dwLedNum);

                            dwReturnValue = TPR_FAIL;
                            goto CLEANUP;
                        }

                        //Checking for szValueName field
                        dwReturnValue = RegQueryValueEx(hEventNameKey, 
                                                        REG_EVENT_VALUE_NAME_VALUE,
                                                        NULL, 
                                                        NULL, 
                                                        NULL, 
                                                        NULL);

                        if(ERROR_FILE_NOT_FOUND == dwReturnValue)
                        {
                            //Logging the error message using the kato logging engine
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("The NLED %u does not have the ValueName field for Event %s"), 
                                         dwLedNum, 
                                         szEventName);

                            dwReturnValue = TPR_FAIL;
                            goto CLEANUP;
                        }
                        else if(ERROR_SUCCESS != dwReturnValue)
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("Querying for the ValueName registry field for Event %s for NLED %u failed"), 
                                         szEventName, 
                                         dwLedNum);

                            dwReturnValue = TPR_FAIL;
                            goto CLEANUP;
                        }

                        if(LED_BLINK == dwState)
                        {
                            //Checking for TotalCycleTime field
                            dwReturnValue = RegQueryValueEx(hEventNameKey, 
                                                            REG_EVENT_TOTAL_CYCLE_TIME_VALUE, 
                                                            NULL,
                                                            NULL, 
                                                            NULL, 
                                                            NULL);

                            if(ERROR_FILE_NOT_FOUND == dwReturnValue)
                            {
                                //Logging the error message using the kato logging engine
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("The NLED %u does not have the TotalCycleTime field for Event %s"), 
                                             dwLedNum, 
                                             szEventName);

                                dwReturnValue = ERROR_SUCCESS;
                            }
                            else if(ERROR_SUCCESS != dwReturnValue)
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("Querying for the TotalCycleTime registry field for Event %s for NLED %u failed"), 
                                             szEventName, 
                                             dwLedNum);

                                dwReturnValue = TPR_FAIL;
                                goto CLEANUP;
                            }

                            //Checking for OnTime field
                            dwReturnValue = RegQueryValueEx(hEventNameKey, 
                                                            REG_EVENT_ON_TIME_VALUE, 
                                                            NULL,
                                                            NULL, 
                                                            NULL, 
                                                            NULL);

                            if(ERROR_FILE_NOT_FOUND == dwReturnValue)
                            {
                                //Logging the error message using the kato logging engine
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("The NLED %u does not have the OnTime field for Event %s"), 
                                             dwLedNum, 
                                             szEventName);

                                dwReturnValue = ERROR_SUCCESS;
                            }
                            else if(ERROR_SUCCESS != dwReturnValue)
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("Querying for the OnTime registry field for Event %s for NLED %u failed"), 
                                             szEventName, 
                                             dwLedNum);

                                dwReturnValue = TPR_FAIL;
                                goto CLEANUP;
                            }

                            //Checking for OffTime field
                            dwReturnValue = RegQueryValueEx(hEventNameKey, 
                                                            REG_EVENT_OFF_TIME_VALUE, 
                                                            NULL, 
                                                            NULL,
                                                            NULL, 
                                                            NULL);

                            if(ERROR_FILE_NOT_FOUND == dwReturnValue)
                            {
                                //Logging the error message using the kato logging engine
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("The NLED %u does not have the OffTime field for Event %s"), 
                                             dwLedNum, 
                                             szEventName);

                                dwReturnValue = ERROR_SUCCESS;
                            }
                            else if(ERROR_SUCCESS != dwReturnValue)
                            {      
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("Querying for the OffTime registry field for Event %s for NLED %u failed"), 
                                             szEventName, 
                                             dwLedNum);

                                dwReturnValue = TPR_FAIL;
                                goto CLEANUP;
                            }

                            //Checking for MetaCycleOn field
                            dwReturnValue = RegQueryValueEx(hEventNameKey, 
                                                            REG_EVENT_META_CYCLE_ON_VALUE, 
                                                            NULL,
                                                            NULL, 
                                                            NULL, 
                                                            NULL);

                            if(ERROR_FILE_NOT_FOUND == dwReturnValue)
                            {
                                //Logging the error message using the kato logging engine
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("The NLED %u does not have the MetaCycleOn field for Event %s"), 
                                             dwLedNum, 
                                             szEventName);

                                dwReturnValue = ERROR_SUCCESS;
                            }
                            else if(ERROR_SUCCESS != dwReturnValue)
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("Querying for the MetaCycleOn registry field for Event %s for NLED %u failed"), 
                                             szEventName, 
                                             dwLedNum);

                                dwReturnValue = TPR_FAIL;
                                goto CLEANUP;
                            }

                            //Checking for MetaCycleOff field
                            dwReturnValue = RegQueryValueEx(hEventNameKey, 
                                                            REG_EVENT_META_CYCLE_OFF_VALUE,
                                                            NULL, 
                                                            NULL, 
                                                            NULL, 
                                                            NULL);

                            if(ERROR_FILE_NOT_FOUND == dwReturnValue)
                            {
                                //Logging the error message using the kato logging engine
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("The NLED %u does not have the MetaCycleOff field for Event %s"), 
                                             dwLedNum, 
                                             szEventName);

                                dwReturnValue = ERROR_SUCCESS;
                            }
                            else if(ERROR_SUCCESS != dwReturnValue)
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("Querying for the MetaCycleOff registry field for Event %s for NLED %u failed"), 
                                             szEventName, 
                                             dwLedNum);

                                dwReturnValue = TPR_FAIL;
                                goto CLEANUP;
                            }
                        }

                        //Checking for consistent registry settings

                        //Clearing the dwEventValueCount count for the next iterations
                        dwEventValueCount = 0;

                        dwEventValueResult=RegEnumValue(hEventNameKey,
                                                        dwEventValueCount,
                                                        szEventValueName,
                                                        &dwEventValueNameSize,
                                                        NULL, 
                                                        NULL, 
                                                        NULL, 
                                                        NULL);

                        while(ERROR_NO_MORE_ITEMS != dwEventValueResult)
                        {
                            //Checking if any invalid keys are present
                            if(!((ERROR_SUCCESS == _tcscmp(szEventValueName, REG_EVENT_ROOT_VALUE)) ||
                                 (ERROR_SUCCESS == _tcscmp(szEventValueName, REG_EVENT_PATH_VALUE)) ||
                                 (ERROR_SUCCESS == _tcscmp(szEventValueName, REG_EVENT_VALUE_NAME_VALUE)) ||
                                 (ERROR_SUCCESS == _tcscmp(szEventValueName, REG_EVENT_MASK_VALUE)) ||
                                 (ERROR_SUCCESS == _tcscmp(szEventValueName, REG_EVENT_VALUE_VALUE)) ||
                                 (ERROR_SUCCESS == _tcscmp(szEventValueName, REG_EVENT_OPERATOR_VALUE)) ||
                                 (ERROR_SUCCESS == _tcscmp(szEventValueName, REG_EVENT_PRIO_VALUE)) ||
                                 (ERROR_SUCCESS == _tcscmp(szEventValueName, REG_EVENT_STATE_VALUE))||
                                 (ERROR_SUCCESS == _tcscmp(szEventValueName, REG_EVENT_TOTAL_CYCLE_TIME_VALUE)) ||
                                 (ERROR_SUCCESS == _tcscmp(szEventValueName, REG_EVENT_ON_TIME_VALUE)) ||
                                 (ERROR_SUCCESS == _tcscmp(szEventValueName, REG_EVENT_OFF_TIME_VALUE)) ||
                                 (ERROR_SUCCESS == _tcscmp(szEventValueName, REG_EVENT_META_CYCLE_ON_VALUE)) ||
                                 (ERROR_SUCCESS == _tcscmp(szEventValueName, REG_EVENT_META_CYCLE_OFF_VALUE))))
                            {
                                g_pKato->Log(LOG_DETAIL,
                                             TEXT("The NLED %u has invalid sub-keys, the registry is not consistent for Service"), 
                                             dwLedNum);

                                dwReturnValue = TPR_FAIL;
                                goto CLEANUP;
                            }

                            //Incrementing the event value count
                            dwEventValueCount++;

                            //Resetting the buffer size
                            dwEventValueNameSize=gc_dwMaxKeyNameLength + 1;

                            //Enumerating for other keys
                            dwEventValueResult=RegEnumValue(hEventNameKey,
                                                            dwEventValueCount,
                                                            szEventValueName,
                                                            &dwEventValueNameSize,
                                                            NULL, 
                                                            NULL, 
                                                            NULL, 
                                                            NULL);
                        }
                    }
                    else
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("Failed to open the %s key"), 
                                     szEventName);

                        dwReturnValue = TPR_FAIL;
                    }

                    //Closing the Event key Handle
                    RegCloseKey(hEventNameKey);

                    //Incrementing the count
                    dwEventCount++;

                    //Resetting the buffer size
                    dwEventNameSize=gc_dwMaxKeyNameLength + 1;

                    //Enumerating for other keys
                    dwEventResult=RegEnumKeyEx(hLedNameKey, 
                                               dwEventCount,
                                               szEventName, 
                                               &dwEventNameSize,
                                               NULL, 
                                               NULL, 
                                               NULL, 
                                               NULL);
                }
            }
            else
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Failed to open the %s key"), szLedName);

                dwReturnValue = TPR_FAIL;
                FAIL("Unable to access registry for LEDs");
                break;
            }

            //Closing the Event key Handle
            RegCloseKey(hLedNameKey);

            //Incrementing the count
            dwLedCount++;

            //Resetting the buffer size
            dwLedNameSize=gc_dwMaxKeyNameLength + 1;

            //Enumerating for other keys
            dwConfigKeyResult=RegEnumKeyEx(hConfigKey,
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
        FAIL("Failed to open NLED-LedName keys for Enumeration");
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

        if(hEventNameKey != NULL)
        {
            RegCloseKey(hEventNameKey);
        }

        //Returning the status
        if(ERROR_SUCCESS == dwReturnValue)
        {
            SUCCESS("The Nled Registry for Driver & Service is complete, consistent and correct");
            return TPR_PASS;
        }
        else
        {
            FAIL("The Nled Registry for Driver & Service is not complete, consistent and correct");
            return TPR_FAIL;
        }
}

//*****************************************************************************
TESTPROCAPI VerifyNledRegistryCount(UINT                   uMsg,
                                    TPPARAM                tpParam,
                                    LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
//  The number of LEDs returned by NledGetDeviceInfo function must be
//  equal to the number of LEDs in the registry.
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
    DWORD dwConfigKeyResult=0;
    HKEY hConfigKey = NULL;
    DWORD dwReturnValue = TPR_FAIL;
    DWORD dwRegLedCount = 0;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLedNameSize=gc_dwMaxKeyNameLength + 1;

    //Opening the key
    dwReturnValue = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                 REG_NLED_KEY_CONFIG_PATH, 
                                 0, 
                                 KEY_ENUMERATE_SUB_KEYS,
                                 &hConfigKey);

    if(ERROR_SUCCESS == dwReturnValue)
    {
        //Enumerating for LedName keys
        dwConfigKeyResult=RegEnumKeyEx(hConfigKey, 
                                       0, 
                                       szLedName, 
                                       &dwLedNameSize, 
                                       NULL, 
                                       NULL, 
                                       NULL, 
                                       NULL);

        if(ERROR_NO_MORE_ITEMS == dwConfigKeyResult)
        {
            FAIL("No LEDs configured, Register Count is 0");
            RegCloseKey(hConfigKey);
            return TPR_FAIL;
         }

        while(ERROR_SUCCESS == dwConfigKeyResult)
        {
            //Re-Initializing the size
            dwLedNameSize=gc_dwMaxKeyNameLength + 1;

            //Incrementing the count
            dwRegLedCount++;

            //Enumerating for all the available keys
            dwConfigKeyResult=RegEnumKeyEx(hConfigKey, 
                                           dwRegLedCount, 
                                           szLedName, 
                                           &dwLedNameSize,
                                           NULL, 
                                           NULL, 
                                           NULL, 
                                           NULL);
        }

        //Checking if the Registry count and GetCount() are same
        if(dwRegLedCount == GetLedCount())
        {
            SUCCESS("LED count in registry matching with actual LED count.");
            dwReturnValue = TPR_PASS;
        }
        else
        {
            FAIL("LED count in registry does not match with actual LED count");
            dwReturnValue = TPR_FAIL;
        }
    }
    else
    {
        //Logging the error message using the kato loggin engine
        FAIL("Failed to open NLED-LedName keys for Enumeration");
        dwReturnValue = TPR_FAIL;
    }

    //Closing the handles
    if(hConfigKey != NULL)
    {
        RegCloseKey(hConfigKey);
    }

    return dwReturnValue;
}

//*****************************************************************************
TESTPROCAPI VerifyNledBlinking(UINT                   uMsg,
                               TPPARAM                tpParam,
                               LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
//  To verify the NLed Blinking for Nled having Blinkable = 1 in the registry
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    //Only Supporting executing the test
    if(uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

    //Skipping the test if the General or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fGeneralTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    // If there are no NLEDs on the device pass the test.
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("There are no NLEDs to Test on the Device. Automatic PASS"));

        return TPR_PASS;
    }    

    //Variable declarations
    DWORD dwReturnValue = TPR_FAIL;
    DWORD dwReturn = 0;

    //Declaring the settings Info structure
    NLED_SETTINGS_INFO originalBlinkingLedSettingsInfo = { 0 };
    NLED_SETTINGS_INFO originalNonBlinkingLedSettingsInfo = {0};
    NLED_SETTINGS_INFO ledSettingsInfo_Get = {0};

    //Getting the lednum that supports blinking
    INT nTempLedNum = GetBlinkingNledNum();

    if(NO_MATCHING_NLED == nTempLedNum)
    {
        //Skipping the test since there is no nled which supports blinking
        g_pKato->Log(LOG_DETAIL,
                     TEXT("None of the Nleds on the device have Blinkable set to 1 in the registry"));

        return TPR_SKIP;
    }

    //Storing the original settings
    originalBlinkingLedSettingsInfo.LedNum = nTempLedNum;
    dwReturnValue = NLedGetDeviceInfo(NLED_SETTINGS_INFO_ID, 
                                      &originalBlinkingLedSettingsInfo);

    if(!dwReturnValue)
    {
        FAIL("Unable to get the original LED settings");
        return TPR_FAIL;
    }

    if(nTempLedNum != NO_MATCHING_NLED)
    {
        //Setting the LED to Blink
        ledSettingsInfo_Get.LedNum = nTempLedNum;

        //Blinking the LED that supports blinking
        dwReturnValue = BlinkNled(nTempLedNum);

        if(TPR_PASS != dwReturnValue)
        {
            //Logging the error output using the kato logging engine
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Nled Driver failed to Blink the %u NLed"), 
                         nTempLedNum);

            dwReturnValue = TPR_FAIL;
            goto RETURN;
        }

        //Call NledGetDevice to get the settings Info
        dwReturnValue = NLedGetDeviceInfo(NLED_SETTINGS_INFO_ID, 
                                          &ledSettingsInfo_Get);

        if(dwReturnValue)
        {
            if(ledSettingsInfo_Get.OffOnBlink == LED_BLINK)
            {
                //Logging the output using the kato logging engine
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Nled Driver is successful to blink the Nled that supports blinking"));

                dwReturnValue = TPR_PASS;
            }
            else
            {
                //Logging the error output using the kato logging engine
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Nled Driver failed to Blink the %u NLed"), 
                             nTempLedNum);

                dwReturnValue = TPR_FAIL;
                goto RETURN;
            }
        }
        else
        {
            //Logging the error output using the kato logging engine
            FAIL("Nled Driver failed to retrieve settings Info");
            dwReturnValue = TPR_FAIL;
            goto RETURN;
        }
    }

    //Checking for the Nled driver fails to blink a NLed that does not
    //supports Blinking.
    //Getting the lednum that does not supports blinking

    nTempLedNum = GetNonBlinkingNledNum();

    if(NO_MATCHING_NLED == nTempLedNum)
    {
        //Skipping the test since there is no nled which does not supports blinking
         g_pKato->Log(LOG_DETAIL,
                      TEXT("All the Nleds on the device support blinking. So skipping the Non Blinking test"));         

         return dwReturnValue;
    }

    //Getting the original settings for Nled that does not supports blinking
    originalNonBlinkingLedSettingsInfo.LedNum = nTempLedNum;

    dwReturnValue = NLedGetDeviceInfo(NLED_SETTINGS_INFO_ID, 
                                      &originalNonBlinkingLedSettingsInfo);

    if(!dwReturnValue)
    {
        FAIL("Unable to get the original LED settings");
        return TPR_FAIL;
    }

    if(nTempLedNum != NO_MATCHING_NLED)
    {
        //Setting the LED to Blink
        ledSettingsInfo_Get.LedNum = nTempLedNum;

        //Blinking the LED that does not supports blinking
        dwReturnValue = BlinkNled(nTempLedNum);

        if(TPR_FAIL != dwReturnValue)
        {
            //Logging the error output using the kato logging engine
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Test Fails: Nled Driver Blinked the NLed %u that does not supports Blinking"), 
                         nTempLedNum);

            dwReturnValue = TPR_FAIL;
            goto RETURN;
        }

        //Call NledGetDevice to get the settings Info
        dwReturnValue = NLedGetDeviceInfo(NLED_SETTINGS_INFO_ID, 
                                          &ledSettingsInfo_Get);

        if(dwReturnValue)
        {
            if(ledSettingsInfo_Get.OffOnBlink != LED_BLINK)
            {
                //Logging the output using the kato logging engine
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Nled Driver successfully does not blinks the Nled that does not supports Blinking"));

                dwReturnValue = TPR_PASS;
            }
        }
        else
        {
            //Logging the error output using the kato logging engine
            FAIL("Nled Driver failed to retrieve settings Info");
            dwReturnValue = TPR_FAIL;
            goto RETURN;
        }
    }

RETURN:

    //Restoring the original led settings
    dwReturn = RestoreOriginalNledSettings(&originalBlinkingLedSettingsInfo);

    if(TPR_PASS != dwReturn)
    {
        //Logging the error output using the kato logging engine
        FAIL("Nled Driver failed to restore the original NLED settings");
        dwReturnValue = TPR_FAIL;
    }

    dwReturn = RestoreOriginalNledSettings(&originalNonBlinkingLedSettingsInfo);

    if(TPR_PASS != dwReturn)
    {
        //Logging the error output using the kato logging engine
        FAIL("Nled Driver failed to restore the original NLED settings");
        dwReturnValue = TPR_FAIL;
    }

    return dwReturnValue;
}

//*****************************************************************************
TESTPROCAPI NledDriverRobustnessTests(UINT                   uMsg,
                                      TPPARAM                tpParam,
                                      LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
// "Set the LedName key in the registry to have more than 255 characters.
//  The system should handle this scenario gracefully."
//
// "Set the LedType key in the registry to a value other than (0,1,2). The
//  system should handle this scenario gracefully."
//
// "Set the value for the CycleAdjust key in the registry and set the
//  Blinkable and SWBlinkCtrl registry keys to zero. The system should handle
//  this scenario gracefully."
//
// "Set the value for the Blinkable key in the registry to a value other
//  than (0,1). The system should handle this scenario gracefully."
//
// "Set the value of the AdjustType registry key to a value other than (0,1,2).
//  The system should handle this scenario gracefully."
//
// "Set the value of the AdjustType registry key to (1 or 2), and set the
//  Blinkable registry key to Zero. The system should handle this scenario gracefully."
//
// "Set the value of the MetaCycle registry key to a value other than (0,1).
//  The system should handle this scenario gracefully."
//
// "Set the value of the MetaCycle registry key to 1, and set the Blinkable
//  registry key to Zero. The system should handle this scenario gracefully."
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

    //Storing the original LED settings
    NLED_SETTINGS_INFO *pOriginalLedSettingsInfo = NULL;

    //Declaring the variables
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLedNameSize=gc_dwMaxKeyNameLength + 1;
    DWORD dwConfigKeyReslt=0;
    HKEY hConfigKey = NULL;
    HKEY hLedNameKey = NULL;
    DWORD dwLen = sizeof(DWORD);
    UINT nLedNum = 0;
    DWORD dwReturn = 0;
    DWORD dwPreLedType = 0;
    BOOL fStatus = FALSE;
    DWORD dwLedNum = 0;
    DWORD dwTestResult = TPR_FAIL;
    UINT nVibratorCount = 0;
    BOOL fExecuteOnlyOnce = TRUE;

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
        return TPR_FAIL;
    }

    //Notify the user regarding the registry being corrupted
    if(g_fInteractive)
    {
        MessageBox(NULL,
                   TEXT("The Nled registry is being corrupted to test the Nled Driver Robustness tests, Do not perform any critical operations"),
                   TEXT("NLED Driver Robustness Tests"),MB_OK);
    }
    else
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("The Nled registry is being corrupted to test the Nled Driver Robustness tests, Do not perform any critical operations"));
    }

    //Opening the key
    dwReturn = RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                            REG_NLED_KEY_CONFIG_PATH, 
                            0, 
                            KEY_ENUMERATE_SUB_KEYS, 
                            &hConfigKey);

    if(ERROR_SUCCESS == dwReturn)
    {
        dwConfigKeyReslt = RegEnumKeyEx(hConfigKey, 
                                        nLedNum, 
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
            dwTestResult = TPR_FAIL;
            goto RETURN;
         }

        //These tests are only for a single LED
        while(nLedNum < GetLedCount())
        {
            //Setting the LEDNAME key
            dwReturn = RegOpenKeyEx(hConfigKey, 
                                    szLedName, 
                                    0, 
                                    KEY_ALL_ACCESS, 
                                    &hLedNameKey);

            if(ERROR_SUCCESS != dwReturn)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to open the %s LED"),szLedName);

                RegCloseKey(hConfigKey);
                dwTestResult = TPR_FAIL;
                goto RETURN;
            }
            else if(ERROR_SUCCESS == dwReturn)
            {
                dwReturn = RegQueryValueEx(hLedNameKey,
                                           REG_LED_NUM_VALUE,
                                           NULL,
                                           NULL,
                                           (LPBYTE) &dwLedNum,
                                           &dwLen);

                if(dwReturn != ERROR_SUCCESS)
                {
                    FAIL("Nled Robustness Test - Querying LedNum Failed");
                    dwTestResult = TPR_FAIL;
                    goto RETURN;
                }

                if(fExecuteOnlyOnce)
                {
                    //Test for Invalid LedName
                    dwReturn = TestNledWithInvalidLedName(&hConfigKey);

                    if(TPR_PASS != dwReturn)
                    {
                        dwTestResult = TPR_FAIL;
                        goto RETURN;
                    }

                    //Test for Invalid LedType
                    dwReturn = TestNledWithInvalidLedType(&hLedNameKey, dwLedNum);

                    if (TPR_PASS != dwReturn)
                    {
                        dwTestResult = TPR_FAIL;
                        goto RETURN;
                    }

                    fExecuteOnlyOnce = FALSE;
                }

                dwReturn = RegQueryValueEx(hLedNameKey,
                                           REG_LED_TYPE_VALUE,
                                           NULL,
                                           NULL,
                                           (LPBYTE) &dwPreLedType,
                                           &dwLen);

                if(dwReturn != ERROR_SUCCESS)
                {
                    FAIL("Nled Robustness Test - Querying Led Type Failed");
                    dwTestResult = TPR_FAIL;
                    goto RETURN;
                }

                //Check if the NLED is a Vibrator
                if (dwPreLedType == LED_TYPE_VIBRATOR)
                {
                    nLedNum++;
                    dwLedNameSize = gc_dwMaxKeyNameLength + 1;
                    dwConfigKeyReslt = RegEnumKeyEx(hConfigKey, 
                                                    nLedNum, 
                                                    szLedName,
                                                    &dwLedNameSize, 
                                                    NULL, 
                                                    NULL, 
                                                    NULL,
                                                    NULL);

                    nVibratorCount++;

                    //All the other test scenarios mentioned below in this testcase are applicable
                    //for a LED, hence continuing here to check if the device has any LED
                    continue;
                }

                //Test for invalid Blinkable, valid SWBlinkCtrl, valid CycleAdjust
                dwReturn = InvalidBlinkableValidCycleAdjustTest(&hLedNameKey, 
                                                                dwLedNum);

                if (dwReturn == TPR_FAIL)
                {
                    dwTestResult = TPR_FAIL;
                    goto RETURN;
                }

                //Test for Invalid Blinkable
                dwReturn = TestNledWithInvalidBlinkableKey(&hLedNameKey, dwLedNum);

                if(dwReturn == TPR_FAIL)
                {
                    dwTestResult = TPR_FAIL;
                    goto RETURN;
                }

                //Test for Invalid AdjustType
                dwReturn = TestNledWithInvalidAdjustType(&hLedNameKey, dwLedNum);

                if (dwReturn == TPR_FAIL)
                {
                    dwTestResult = TPR_FAIL;
                    goto RETURN;
                }

                //Test for Invalid Blinkable, valid SWBlinkCtrl, valid AdjustType
                dwReturn = InvalidBlinkableValidAdjustTypeTest(&hLedNameKey, 
                                                               dwLedNum);

                if (dwReturn == TPR_FAIL)
                {
                    dwTestResult = TPR_FAIL;
                    goto RETURN;
                }

                //Test for invalid Metacycle
                dwReturn = TestNledWithInvalidMetaCycle(&hLedNameKey, dwLedNum);

                if(dwReturn == TPR_FAIL)
                {
                    dwTestResult = TPR_FAIL;
                    goto RETURN;
                }

                //Test for invalid Blinkable and valid Metacycle
                dwReturn = InvalidBlinkableValidMetaCycleTest(&hLedNameKey, 
                                                              dwLedNum);

                if(dwReturn == TPR_FAIL)
                {
                    dwTestResult = TPR_FAIL;
                    goto RETURN;
                }

                //Calling the Nled Device Driver IOCTLs
                fStatus = CallNledDeviceDriverIOCTL();

                if(!fStatus)
                {
                    FAIL("Nled Driver IOCTL test failed");
                    dwTestResult = TPR_FAIL;
                    goto RETURN;
                }
                else
                {
                    SUCCESS("Nled Driver IOCTL test passed");
                }

                //All Tests are Passed.
                dwTestResult = TPR_PASS;
                goto RETURN;
            }

            //Closing the Led key Handle
            if(hLedNameKey != NULL)
            {
                RegCloseKey(hLedNameKey);
            }

            //Incrementing the Count
            nLedNum++;
            dwLedNameSize = gc_dwMaxKeyNameLength + 1;
            dwConfigKeyReslt = RegEnumKeyEx(hConfigKey, 
                                            nLedNum, 
                                            szLedName, 
                                            &dwLedNameSize, 
                                            NULL, 
                                            NULL, 
                                            NULL, 
                                            NULL);
        }

        if(GetLedCount() == nVibratorCount)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("All the notification LEDs on this device are Vibrators. Hence skipping the remaining test scenarios in this testcase as those are only applicable to LEDs and not Vibrators"));

            dwTestResult = TPR_PASS;
            goto RETURN;
        }
    }
    else if(ERROR_SUCCESS != dwReturn)
    {
        FAIL("Unable to open the Nled Config key");
        dwTestResult = TPR_FAIL;
    }

RETURN:

    //Restoring all the original led settings
    if(pOriginalLedSettingsInfo != NULL)
    {
        fStatus = setAllLedSettings(pOriginalLedSettingsInfo);

        if(!fStatus)
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

    //Closing the handles and freeing the memory
    if(hLedNameKey != NULL)
    {
        RegCloseKey(hLedNameKey);
    }

    if(hConfigKey != NULL)
    {
        RegCloseKey(hConfigKey);
    }

    return dwTestResult;
}

//*****************************************************************************
TESTPROCAPI NledDriverMemUtilizationTest(UINT                   uMsg,
                                         TPPARAM                tpParam,
                                         LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
//  This test checks whether the memory utilization is greater
//  than a given threshold value when the states of all the Nleds are changing
//  continuously.
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

    //Declaring the variables
    NLED_SETTINGS_INFO ledSettingsInfo_Set = {0};
    BOOL fStatus = FALSE;
    DWORD dwOriginalMemoryLoad = 0;
    DWORD dwFinalMemoryLoad = 0;
    DWORD dwReturnValue = 0;
    DWORD dwTestResult = TPR_PASS;
    MEMORYSTATUS *lpBuffer = NULL;
    INT nTempLedNum = 0;    

    //Storing the original LED settings
    NLED_SETTINGS_INFO* pOriginalLedSettingsInfo = NULL;

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
        return TPR_FAIL;
    }

    //Get the initial memory utilization
    lpBuffer = (MEMORYSTATUS*)LocalAlloc(LMEM_ZEROINIT, sizeof (MEMORYSTATUS));

    //Getting the memory status
    GlobalMemoryStatus(lpBuffer);
    dwOriginalMemoryLoad = lpBuffer->dwMemoryLoad;

    //Getting the blinkable Nled Number
    nTempLedNum = GetBlinkingNledNum();

    if(NO_MATCHING_NLED == nTempLedNum)
    {
        //If there is no NLED that supports blinking then test the first Nled
        nTempLedNum = LED_INDEX_FIRST;
    }

    //Performing the test for a LED
    //Clearing off the previous thread count
    g_nThreadCount = 0;

    //Clearing off the previous value
    g_fMULTIPLENLEDSETFAILED = FALSE;

    //Calling NledSetDevice with different combinations in the CheckSysTemBehavior function
    dwReturnValue = CheckSystemBehavior(nTempLedNum, ONE_LED);

    if(TPR_PASS != dwReturnValue)
    {
        g_pKato->Log(LOG_DETAIL, 
                     TEXT("Failed to create the threads for invoking multiple NledSetDevice"));

        dwTestResult = TPR_FAIL;

        //Waiting for all the threads to complete execution
        CheckThreadCompletion(g_hThreadHandle, MAX_THREAD_COUNT, MAX_THREAD_TIMEOUT_VAL);

        goto RETURN;
    }

    //Check if all the threads are done with the execution
    dwReturnValue = CheckThreadCompletion(g_hThreadHandle, MAX_THREAD_COUNT, MAX_THREAD_TIMEOUT_VAL);

    if(TPR_FAIL == dwReturnValue)
    {
        dwTestResult = TPR_FAIL;
        goto RETURN;
    }

    if(g_fMULTIPLENLEDSETFAILED)
    {
        dwTestResult = TPR_FAIL;
        goto RETURN;
    }

    //Calculate the memory utilization again
    GlobalMemoryStatus(lpBuffer);
    dwFinalMemoryLoad = lpBuffer->dwMemoryLoad;

    if((dwFinalMemoryLoad - dwOriginalMemoryLoad) < g_nMaxMemoryUtilization)
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Nled Driver memory utilization for a Nled is less than the Max allowed Memory Utilization value"));
    }
    else
    {
        //Logging the error output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Nled Driver memory utilization for a Nled is greater than the Max allowed Memory Utilization value"));

        dwTestResult = TPR_FAIL;
    }

    //Closing the thread handles before going to the next test
    for (UINT nThreadCount = 0; nThreadCount < MAX_THREAD_COUNT; nThreadCount++)
    {
        if(g_hThreadHandle[nThreadCount] != NULL)
        {
            fStatus = CloseHandle(g_hThreadHandle[nThreadCount]);
            g_hThreadHandle[nThreadCount] = NULL;

            if(!fStatus)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Failed to close the handle of the thread created for invoking NledSetDevice"));

                dwTestResult = TPR_FAIL;
                goto RETURN;
            }
        }
    }

    //Clearing off the previous thread count
    g_nThreadCount = 0;

    //Clearing off the previous value
    g_fMULTIPLENLEDSETFAILED = FALSE;

    //Turn OFF all the LEDs before going to the next test
    ledSettingsInfo_Set.OffOnBlink = LED_OFF;

    for(UINT nCount = 0; nCount < GetLedCount(); nCount++)
    {
        fStatus = NLedSetDevice(NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Set);

        if(!fStatus)
        {
            //Logging the output using the kato logging engine
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to turn Off the Nled No: %u"), 
                         nCount);

            dwTestResult = TPR_FAIL;
            goto RETURN;
        }
    }

    //Performing the test for all the leds   

    //Calculating the Initial Memory Load
    GlobalMemoryStatus(lpBuffer);
    dwOriginalMemoryLoad = lpBuffer->dwMemoryLoad;

    //Invoking NledSetDevice for all the NLEDs
    for(UINT nLedCount = 0; nLedCount < GetLedCount(); nLedCount++)
    {
        dwReturnValue = CheckSystemBehavior(nLedCount, GetLedCount());

        if(TPR_PASS != dwReturnValue)
        {
            g_pKato->Log(LOG_DETAIL, 
                         TEXT("Failed to create the threads for invoking multiple NledSetDevice for NLED %u"), 
                         nLedCount);

            dwTestResult = TPR_FAIL;

            //Waiting for all the threads to complete execution
            CheckThreadCompletion(g_hThreadHandle, MAX_THREAD_COUNT, MAX_THREAD_TIMEOUT_VAL);

            goto RETURN;
        }
    }

    //Check if all the threads are done with the execution
    dwReturnValue = CheckThreadCompletion(g_hThreadHandle, MAX_THREAD_COUNT, MAX_THREAD_TIMEOUT_VAL);

    if(TPR_FAIL == dwReturnValue)
    {
       dwTestResult = TPR_FAIL;
       goto RETURN;
    }

    if(g_fMULTIPLENLEDSETFAILED)
    {
        dwTestResult = TPR_FAIL;
        goto RETURN;
    }

    //Calculate the memory utilization again
    GlobalMemoryStatus(lpBuffer);
    dwFinalMemoryLoad = lpBuffer->dwMemoryLoad;

    if((dwFinalMemoryLoad - dwOriginalMemoryLoad) < g_nMaxMemoryUtilization)
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Nled Driver memory utilization for all the Nleds is less than the Max allowed Memory Utilization value"));
    }
    else
    {
        //Logging the error output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Nled Driver memory utilization for all the Nleds is greater than the Max allowed Memory Utilization value"));

        dwTestResult = TPR_FAIL;
    }

RETURN:

    //Releasing the memory
    if(lpBuffer != NULL)
    {
        LocalFree(lpBuffer);
        lpBuffer = NULL;
    }

    //Closing the thread handles
    for (UINT nCount = 0; nCount < g_nThreadCount; nCount ++)
    {
        if(g_hThreadHandle[nCount] != NULL)
        {
            fStatus = CloseHandle(g_hThreadHandle[nCount]);
            g_hThreadHandle[nCount] = NULL;

            if(!fStatus)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Failed to close the handle of the thread created for invoking NledSetDevice"));

                dwTestResult = TPR_FAIL;
            }
        }
    }

    //Restoring all the original led settings
    fStatus = setAllLedSettings(pOriginalLedSettingsInfo);

    if(!fStatus)
    dwTestResult = TPR_FAIL;

    //Releasing the memory  allocated
    if(pOriginalLedSettingsInfo != NULL)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    return dwTestResult;
}

//*****************************************************************************
TESTPROCAPI NledSystemBehaviorTest(UINT                   uMsg,
                                   TPPARAM                tpParam,
                                   LPFUNCTION_TABLE_ENTRY lpFTE)
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_NOT_HANDLED
//
// "Call setNLEDInfo multiple times by passing different parameters for
//  each LED over a period of time. Driver must be able to handle this condition."
//
// "Call setNLEDInfo multiple times by passing different parameters for
//  all LEDs simultaneously over a period of time. Driver must be able to handle
//  this condition"
//
//*****************************************************************************
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if(uMsg != TPM_EXECUTE)  return TPR_NOT_HANDLED;

    //Skipping the test if the general or automatic tests are not enabled from the
    //command line - By default all tests are enabled except the interactive tests
    if((!g_fGeneralTests) && (!g_fAutomaticTests)) return TPR_SKIP;

    //Passing the test if the device does not have any Nleds
    if(NO_NLEDS_PRESENT == GetLedCount())
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("The device doesn't have any NLEDs so passing the test"));

        return TPR_PASS;
    }

    //Storing the original LED settings
    NLED_SETTINGS_INFO *pOriginalLedSettingsInfo = NULL;
    BOOL fStatus = FALSE;
    DWORD dwTestResult = TPR_PASS;
    DWORD dwReturn = 0;

    // Allocate space to save origional LED setttings.
    SetLastError(ERROR_SUCCESS);
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
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
        g_pKato->Log(LOG_DETAIL, 
                     TEXT("Unable to save Nled Settings for all the Leds"));

        return TPR_FAIL;
    }

    //Clearing the previous value
    g_fMULTIPLENLEDSETFAILED = FALSE;

    //Clearing off the previous thread count
    g_nThreadCount = 0;

    dwReturn = CheckSystemBehavior(LED_INDEX_FIRST, ONE_LED);

    if(TPR_PASS != dwReturn)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Failed to create the thread to invoke NledSetDevice"));

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
        FAIL("NLED driver fails to retrieve correct Settings Info against simultaneous NledSetDevice calls");
        dwTestResult = TPR_FAIL;
    }
    else
    {
        //Logging the output using the kato logging engine
        SUCCESS("NLED driver successfully retrieves correct Settings Info against simultaneous NledSetDevice calls");
    }

    //Closing thread handles before moving to other tests
    for (UINT nTCount = 0; nTCount < MAX_THREAD_COUNT; nTCount++)
    {
        if(g_hThreadHandle[nTCount] != NULL)
        {
            fStatus = CloseHandle(g_hThreadHandle[nTCount]);
            g_hThreadHandle[nTCount] = NULL;

            if(!fStatus)
            {
                //Logging the error output using the kato logging engine
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Failed to close the handle of the thread created for invoking NledSetDevice"));

                dwTestResult = TPR_FAIL;
                break;
            }
        }
    }

    //Testing the above test for all the leds simultaneously

    //Clearing the previous value
    g_fMULTIPLENLEDSETFAILED = FALSE;

    //Clearing off the previous thread count
    g_nThreadCount = 0;

    //Invoking NledSetDevice for all the NLEDs
    for (UINT nLedCount = 0; nLedCount < GetLedCount(); nLedCount ++)
    {
        dwReturn = CheckSystemBehavior(nLedCount, GetLedCount());

        if(TPR_PASS != dwReturn)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Failed to create the thread for invoking NledSetDevice"));

            dwTestResult = TPR_FAIL;

            //Waiting for all the threads to complete execution
            CheckThreadCompletion(g_hThreadHandle, MAX_THREAD_COUNT, MAX_THREAD_TIMEOUT_VAL);

            goto RETURN;
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
        FAIL("NLED driver fails to retrieve correct settings Info of all the Nleds against simultaneous NledSetDevice calls");
        dwTestResult = TPR_FAIL;
    }
    else
    {
        //Logging the error output using the kato logging engine
        SUCCESS("NLED driver successfully retrieves correct settings Info of all the Nleds against simultaneous NledSetDevice calls");
    }

RETURN:

    //Closing the handles
    for (UINT nThCount = 0; nThCount < MAX_THREAD_COUNT; nThCount++)
    {
        if(g_hThreadHandle[nThCount] != NULL)
        {
            fStatus = CloseHandle(g_hThreadHandle[nThCount]);
            g_hThreadHandle[nThCount] = NULL;

            if(!fStatus)
            {
                //Logging the error output using the kato logging engine
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Failed to close the handle of the thread created for invoking NledSetdevice"));

                dwTestResult = TPR_FAIL;
                break;
            }
        }
    }

    //Restoring all the original led settings
    fStatus = setAllLedSettings(pOriginalLedSettingsInfo);

    if(!fStatus)
    dwTestResult = TPR_FAIL;

    if(pOriginalLedSettingsInfo != NULL)
    {
        delete[] pOriginalLedSettingsInfo;
        pOriginalLedSettingsInfo = NULL;
    }

    return dwTestResult;
}

//****************************************************************************
DWORD RestoreOriginalNledSettings(NLED_SETTINGS_INFO* originalLedSettingsInfo)
//
//  Parameters: Orignal LED Settings Information
//
//  Return Value: TPR_PASS on success
//                  TPR_FAIL on failure
//
//  This function is used to restore the Original NLED Settings
//
//****************************************************************************
{
    //Declaring the variables
    BOOL fStatus = FALSE;

    //Restoring the original led settings
    fStatus = NLedSetDevice(NLED_SETTINGS_INFO_ID, originalLedSettingsInfo);

    if(!fStatus)
    {
        FAIL("Unable to restore LED settings");
        return TPR_FAIL;
    }

    return TPR_PASS;
}

//****************************************************************************
DWORD SuspendResumeDevice(UINT nLedNum, INT nOffOnBlink)
//
//  Parameters: Number of LEDs and the OFFON Blink
//
//  Return Value:   TPR_PASS on success
//                  TPR_FAIL on failure
//
//  This function supends the device for 30 sec. After resuming the device
//  this functions checks if the LED status is restored. This function returns
//  ERROR_NOT_SUPPORTED if power management is not supported on the device.
//
//****************************************************************************/
{
    //Declaring the variables
    DWORD dwReturn;
    WCHAR wszState[1024] = {0};
    LPWSTR pState = &wszState[0];
    DWORD dwBufChars = (sizeof(wszState) / sizeof(wszState[0]));
    DWORD dwStateFlags = 0;
    DWORD dwReturnValue = TPR_FAIL;
    NLED_SETTINGS_INFO originalLedSettingsInfo = { 0 };
    NLED_SETTINGS_INFO ledSettingsInfo_Set = {0};
    NLED_SETTINGS_INFO ledSettingsInfo_Get = {0};
    BOOL fStatus = FALSE;
    TCHAR szPowerStateChangeOn[] = L"On";
    CSysPwrStates *pCSysPwr = NULL;

    //Check Whether Power Management service exists on the device
    dwReturn = GetSystemPowerState(pState, dwBufChars, &dwStateFlags);

    if ((dwReturn != ERROR_SUCCESS) || (dwReturn == ERROR_SERVICE_DOES_NOT_EXIST))
    {
        //Prompting the error message
        g_pKato->Log(LOG_DETAIL,
                     TEXT("ERROR: SKIPPING TEST: Power Management does not exist on this device.\n"));

        return ERROR_NOT_SUPPORTED;
    }

    //Initializing the structure lednum
    ledSettingsInfo_Set.LedNum = nLedNum;
    ledSettingsInfo_Get.LedNum = nLedNum;
    originalLedSettingsInfo.LedNum = nLedNum;

    //Getting the original settings
    fStatus = NLedGetDeviceInfo(NLED_SETTINGS_INFO_ID, &originalLedSettingsInfo);

    if(!fStatus)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to get the original LED settings for %u NLED"),
                     nLedNum);

        return  TPR_FAIL;
    }

    pCSysPwr = new CSysPwrStates();

    //Checking if blinking is to be enabled
    //Blinking the LED and adjusting the blink settings

    if(LED_BLINK == nOffOnBlink)
    {
        dwReturnValue = BlinkNled(nLedNum);

        if(TPR_PASS != dwReturnValue)
        {
            g_pKato->Log(LOG_DETAIL, 
                         TEXT("Unable to blink the LED no: %u"),
                         nLedNum);

            dwReturnValue = TPR_FAIL;
            goto CLEANUP;
        }
    }
    else
    {
        ledSettingsInfo_Set.OffOnBlink = nOffOnBlink;
        dwReturnValue = NLedSetDevice(NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Set);

        if(!dwReturnValue)
        {
            g_pKato->Log(LOG_DETAIL, 
                         TEXT("Unable to change the led state for LED no: %u"),
                         nLedNum);

            dwReturnValue = TPR_FAIL;
            goto CLEANUP;
        }
    }

    g_pKato->Log(LOG_DETAIL,
                 TEXT("Device is going to suspend now.\n"));

    //power suspends if power management is suppored on this device
    fStatus = pCSysPwr->SetupRTCWakeupTimer(TIME_BEFORE_RESUME);

    if(!fStatus)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Power management is not supported on this device"));

        dwReturnValue = ERROR_NOT_SUPPORTED;
        goto CLEANUP;
    }
    else
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Device Suspend Is In Progress, It Will Resume In %d Seconds"),
                     TIME_BEFORE_RESUME);
    }

    //Suspend the system if Power management is supported else display the error message
    dwReturn = SetSystemPowerState(_T("Suspend"), NULL, POWER_FORCE);

    if(dwReturn!= ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("ERROR: SetSystemPowerState failed.\n"));

        pCSysPwr->CleanupRTCTimer();
        dwReturnValue = ERROR_GEN_FAILURE;
        goto CLEANUP;
    }    

    fStatus = PollForSystemPowerChange(szPowerStateChangeOn, 0, TRANSITION_CHANGE_MAX);

    if(!fStatus)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("ERROR: PollForSystemPowerChange failed.\n"));

        pCSysPwr->CleanupRTCTimer();
        dwReturnValue = ERROR_GEN_FAILURE;
        goto CLEANUP;
    }
    else
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("\nDevice power is Resumed\n"));
    }

    //Calling the clean-up timer
    pCSysPwr->CleanupRTCTimer();

    //The device has waked up, now check the LED state
    fStatus = NLedGetDeviceInfo(NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Get);

    if(!fStatus)
    {
        //Reporting error
        FAIL("Nled Driver is unable to Get the LED Settings");
        dwReturnValue = TPR_FAIL;
        goto CLEANUP;
    }

    if(nOffOnBlink == ledSettingsInfo_Get.OffOnBlink)
    {
        SUCCESS("LED state retrieved is the same as LED state set. Test passes.");
        dwReturnValue = TPR_PASS;
    }
    else
    {
        FAIL("LED state retrieved is not the same as LED state set. Test Fails.");
        dwReturnValue = TPR_FAIL;
    }

CLEANUP:

    // Deallocating the Memory
    if(pCSysPwr)
    {
        delete pCSysPwr;
        pCSysPwr = NULL;
    }

    //Restoring the original led settings
    dwReturn = RestoreOriginalNledSettings(&originalLedSettingsInfo);

    if(TPR_PASS != dwReturn)
    {
        //Logging the error output using the kato logging engine
        FAIL("Nled Driver failed to restore the original NLED settings");
        dwReturnValue = TPR_FAIL;
    }

    return dwReturnValue;
}

//****************************************************************************
DWORD GetCpuUtilization()
//
//  Parameters: None
//
//  Return Value: CPU Utilization
//
//  This function calculates the CPU Utilization and returns the CPU utilization
//  Value.
//
//*****************************************************************************
{
    // Getting the CPU Idle time
    DWORD dwTickCount_Start = GetTickCount();
    DWORD dwIdleTime_Start = GetIdleTime();
    DWORD dwPercentIdle =0;
    DWORD dwTickCount_Stop = 0;
    DWORD dwIdleTime_End =0;
    DWORD dwCpuUtilization = 0;

    //Making this thread to sleep for 1000 ms
    Sleep(SLEEP_FOR_ONE_SECOND);

    //Again getting the CPU Idle time
    dwTickCount_Stop = GetTickCount();
    dwIdleTime_End = GetIdleTime();

    dwPercentIdle = ((100*(dwIdleTime_End - dwIdleTime_Start)) / (dwTickCount_Stop - dwTickCount_Start));
    dwCpuUtilization = (100 - dwPercentIdle);

    //Returing the CPU utilization value
    return dwCpuUtilization;
}

//*****************************************************************************
DWORD TestNledWithInvalidLedName(HKEY* hConfigKey)
//
//  Parameters: Config KEY
//
//  Return Value: TPR_PASS, TPR_FAIL
//
//  This function sets the LedName key in the registry to have more than 255
//  characters. And also checks if the system should handle this scenario
//  gracefully."
//
//*****************************************************************************
{
    //Declaring the variables
    TCHAR *pszInvalidLedName = NULL;
    HKEY hLedNameKey = NULL;
    DWORD dwReturnStatus = 0;
    DWORD dwResult = TPR_FAIL;

    //Allocating memory for String:
    pszInvalidLedName = (TCHAR *)calloc((INVALID_KEY_SIZE + 1), sizeof(TCHAR));
   if (!pszInvalidLedName)
   {
       FAIL("Failed on calling calloc");
      return TPR_FAIL;
   }

    //Concatenating the string to form the new Led Name having more than 255 characters
    for(DWORD dwCount = 0; dwCount < 10; dwCount++)
    {
        //Creating a string having more than 255 characters
        wcscat_s(pszInvalidLedName, INVALID_KEY_SIZE + 1, TEXT("InValidLedNameKeyLength260"));
    }

    //Notify the user regarding the registry being corrupted
    if(g_fInteractive)
    {
        MessageBox(NULL,
                   TEXT("Creating a sub-key having name more than 255 Characters"),
                   TEXT("NLED Invalid LedName Test"),MB_OK);
    }
    else
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Creating a sub-key having name more than 255 Characters"));
    }

    //Creating a new key with the invalidname
    dwReturnStatus = RegCreateKeyEx(*hConfigKey,
                                    pszInvalidLedName,
                                    0,
                                    NULL,
                                    0,
                                    KEY_ALL_ACCESS,
                                    NULL, &hLedNameKey,
                                    &dwReturnStatus);

    if(ERROR_SUCCESS == dwReturnStatus)
    {
        FAIL("Invalid Key(Key name more than 255 characters) Creation Successful.");

        //Deleting the created key
        RegCloseKey(hLedNameKey);
        RegDeleteKey(*hConfigKey, pszInvalidLedName);
    }
    else
    {
        SUCCESS("Key Creation failure: System successfully handled the invalid request.");
        dwResult =  TPR_PASS;
    }

    //De-allocating the memory
    if(pszInvalidLedName)
    {
        free(pszInvalidLedName);
        pszInvalidLedName = NULL;
    }
    return dwResult;
}

//*****************************************************************************
DWORD TestNledWithInvalidLedType(HKEY* hLedNameKey, DWORD dwLedNum)
//
//  Parameters: LedName KEY
//
//  Return Value: TPR_PASS, TPR_FAIL
//
//  This function sets the LedType key in the registry to a value other than
//  (0,1,2). And also checks that the system should handle this scenario
//  gracefully.
//
//*****************************************************************************
{
    //Declaring the variables
    DWORD dwReturnValue = TPR_FAIL;
    DWORD dwReturn = 0;
    INT dwInvalidLedTypeValue = INVALID_VALUE;
    DWORD dwPreLedTypeValue;
    DWORD dwLen = sizeof(DWORD);

    //Querying the previous ledtype value
    dwReturnValue = RegQueryValueEx(*hLedNameKey,
                                    REG_LED_TYPE_VALUE,
                                    NULL,
                                    NULL,
                                    (LPBYTE) &dwPreLedTypeValue,
                                    &dwLen);

    if(dwReturnValue != ERROR_SUCCESS)
    {
        FAIL("Invalid LED Type Test - Querying For existing Led Type Value Failed");
        return TPR_FAIL;
    }

    //Setting the invalid ledtype value
    dwReturnValue = RegSetValueEx (*hLedNameKey,
                                   REG_LED_TYPE_VALUE,
                                   0,
                                   REG_DWORD,
                                   (CONST BYTE*) &dwInvalidLedTypeValue,
                                   sizeof(DWORD));

    if (dwReturnValue != ERROR_SUCCESS)
    {
        //Logging the error output using the kato logging engine
        FAIL("Invalid LED Type Test - Setting Invalid led type Value Failed");
        return TPR_FAIL;
    }

    //Testing the working status for NledGetDeviceInfo and NledSetDevice
    dwReturnValue = TestNLEDSetGetDeviceInfo(dwLedNum);

    if (dwReturnValue != TPR_PASS)
    {
        FAIL("Invalid LED Type Test - NLED Set/Get Failed");
    }

    //Restoring back the previous ledtype value
    dwReturn = RegSetValueEx (*hLedNameKey, 
                              REG_LED_TYPE_VALUE, 
                              0, 
                              REG_DWORD,
                              (CONST BYTE*) &dwPreLedTypeValue,
                              sizeof(DWORD));

    if (dwReturn !=  ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Invalid LED Type Test - Restoring LedType Failed"));

        dwReturnValue = TPR_FAIL;
    }

    if (TPR_PASS == dwReturnValue)
    {
        SUCCESS("Invalid LED Type Test: Driver successfully handles the Invalid LedType");
    }

    return dwReturnValue;
}

//*****************************************************************************
DWORD TestNledWithInvalidBlinkableKey(HKEY* hLedNameKey, DWORD dwLedNum)
//
//  Parameters: None
//  Return Value: TPR_PASS, TPR_FAIL
//
//  Set the value for the Blinkable key in the registry to a value other
//  than (0,1). The system should handle this scenario gracefully.
//
//*****************************************************************************
{
    //Declaring the variables
    DWORD dwReturnValue = TPR_FAIL;
    DWORD dwReturn = TPR_FAIL;
    INT dwInvalidBlinkValue = INVALID_VALUE;
    DWORD dwPreBlinkValue;
    DWORD dwLen = sizeof(DWORD);

    //Querying the previous Blinkable value
    dwReturnValue = RegQueryValueEx(*hLedNameKey,
                                    REG_BLINKABLE_VALUE,
                                    NULL,
                                    NULL,
                                    (LPBYTE) &dwPreBlinkValue,
                                    &dwLen);

    if(dwReturnValue!= ERROR_SUCCESS)
    {
        FAIL("Invalid Blinkable Value Test - Querying For Pre Blinkable Value Failed");
        return TPR_FAIL;
    }

    //Setting the invalid Blinkable value
    dwReturnValue = RegSetValueEx (*hLedNameKey,
                                   REG_BLINKABLE_VALUE,
                                   0,
                                   REG_DWORD,
                                   (CONST BYTE*) &dwInvalidBlinkValue,
                                   sizeof(DWORD));

    if (dwReturnValue != ERROR_SUCCESS)
    {
        FAIL("Invalid Blinkable Value Test - Setting Invalid Blinkable Value Failed");
        return TPR_FAIL;
    }

    //Checking the working status for NledGetDeviceInfo and NledSetDevice
    dwReturnValue = TestNLEDSetGetDeviceInfo(dwLedNum);

    if (dwReturnValue != TPR_PASS)
    {
        FAIL("Invalid Blinkable Value Test - NLED Set/Get Failed");
    }

    //Restoring back the previous values
    dwReturn = RegSetValueEx (*hLedNameKey,
                              REG_BLINKABLE_VALUE, 
                              0, 
                              REG_DWORD,
                              (CONST BYTE*) &dwPreBlinkValue,
                              sizeof(DWORD));

    if (dwReturn !=  ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Invalid Blinkable Value Test - Restoring Blinkable Value Failed"));

        dwReturnValue = TPR_FAIL;
    }

    if (TPR_PASS == dwReturnValue)
    {
        SUCCESS("Invalid Blinkable Value Test: Driver Successfully handles the Invalid Blinkable Setting");
        return TPR_PASS;
    }

    return dwReturnValue;
}

//*****************************************************************************
DWORD TestNledWithInvalidAdjustType(HKEY* hLedNameKey, DWORD dwLedNum)
//
//  Parameters: NLed Key, Nled Number
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_SKIP
//
//  Set the value of the AdjustType registry key to a value other than
//  (0,1,2). The system should handle this scenario gracefully.
//
//*****************************************************************************
{
    //Declaring the variables
    DWORD dwReturnValue = TPR_FAIL;
    DWORD dwReturn = TPR_FAIL;
    INT dwInvalidAdjustTypeValue = INVALID_VALUE;
    DWORD dwPreAdjustTypeValue;
    DWORD dwLen = sizeof(DWORD);

    //Querying the previous Adjust type value
    dwReturnValue = RegQueryValueEx(*hLedNameKey,
                                    REG_ADJUSTTYPE_VALUE, 
                                    NULL, 
                                    NULL,
                                    (LPBYTE) &dwPreAdjustTypeValue, &dwLen);

    if(dwReturnValue != ERROR_SUCCESS)
    {
        //Checking if the AdjustType flag is present
        dwReturnValue = RegQueryValueEx(*hLedNameKey,
                                        REG_ADJUSTTYPE_VALUE, 
                                        NULL, 
                                        NULL,
                                        (LPBYTE) &dwPreAdjustTypeValue, &dwLen);

        if(dwReturnValue == ERROR_FILE_NOT_FOUND)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("AdjustType field not present, Skipping the Invalid Adjust Type Test"));

            return TPR_SKIP;
        }

        FAIL("Invalid Adjust Type Test - Querying For Pre AdjustType Value Failed");
        return TPR_FAIL;
    }

    //Setting the invalid adjust type value
    dwReturnValue = RegSetValueEx (*hLedNameKey,
                                   REG_ADJUSTTYPE_VALUE, 
                                   0, 
                                   REG_DWORD,
                                   (CONST BYTE*) &dwInvalidAdjustTypeValue,
                                   sizeof(DWORD));

    if (dwReturnValue != ERROR_SUCCESS)
    {
        FAIL("Invalid Adjust Type Test - Setting Invalid AdjustType Value Failed");
        return TPR_FAIL;
    }

    //Checking the working status of the NledGetDeviceInfo and NledSetDevice
    dwReturnValue = TestNLEDSetGetDeviceInfo(dwLedNum);

    if (dwReturnValue != TPR_PASS)
    {
        FAIL("Invalid Adjust Type Test - NLED Set/Get Failed");
    }

    //Restoring back the previous values
    dwReturn = RegSetValueEx (*hLedNameKey,
                              REG_ADJUSTTYPE_VALUE, 
                              0, 
                              REG_DWORD,
                              (CONST BYTE*) &dwPreAdjustTypeValue,
                              sizeof(DWORD));

    if (dwReturn !=  ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Invalid Adjust Type Test - Restoring Adjust Type Value Failed"));

        dwReturnValue = TPR_FAIL;
    }

    if (dwReturnValue == TPR_PASS)
    {
        SUCCESS("Invalid Adjust Type Test: Driver successfully handles the Invalid AdjustType setting");
    }

    return dwReturnValue;
}

//*****************************************************************************
DWORD TestNledWithInvalidMetaCycle(HKEY* hLedNameKey, DWORD dwLedNum)
//
//  Parameters: Nled Key, Nled Number
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_SKIP
//
//  Set the value of the MetaCycle registry key to a value other than (0,1).
//  The system should handle this scenario gracefully.
//
//*****************************************************************************
{
    //Declaring the variables
    DWORD dwReturnValue = TPR_FAIL;
    DWORD dwReturn = 0;
    INT dwInvalidMetaCycleValue = INVALID_VALUE;
    DWORD dwPreMetaCycleValue;
    DWORD dwLen = sizeof(DWORD);

    //Querying the previous MetaCycle value
    dwReturnValue = RegQueryValueEx(*hLedNameKey,
                                    REG_METACYCLE_VALUE, 
                                    NULL, 
                                    NULL,
                                    (LPBYTE) &dwPreMetaCycleValue, 
                                    &dwLen);

    if(dwReturnValue != ERROR_SUCCESS)
    {
        //Checking if the MetaCycle flag is present
        dwReturnValue = RegQueryValueEx(*hLedNameKey,
                                        REG_METACYCLE_VALUE, 
                                        NULL, 
                                        NULL,
                                        (LPBYTE) &dwPreMetaCycleValue, 
                                        &dwLen);

        if(dwReturnValue == ERROR_FILE_NOT_FOUND)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("MetaCycle field not present, Skipping the Invalid Meta Cycle Test"));

            return TPR_SKIP;
        }

        FAIL("Invalid Meta Cycle Test - Querying For existing MetaCycle Value Failed");
        return TPR_FAIL;
    }

    //Setting the invalid metacycle value
    dwReturnValue = RegSetValueEx (*hLedNameKey,
                                   REG_METACYCLE_VALUE, 
                                   0, 
                                   REG_DWORD,
                                   (CONST BYTE*) &dwInvalidMetaCycleValue,
                                   sizeof(DWORD));

    if (dwReturnValue != ERROR_SUCCESS)
    {
        FAIL("Invalid Meta Cycle Test - Setting Invalid Metacycle Value Failed");
        return dwReturnValue;
    }

    //Checking the working status of the NledGetDeviceInfo and NledSetDevice
    dwReturnValue = TestNLEDSetGetDeviceInfo(dwLedNum);

    if (dwReturnValue != TPR_PASS)
    {
        FAIL("Invalid Meta Cycle Test - NLED Set/Get Failed");
    }

    //Restoring back the previous value
    dwReturn = RegSetValueEx (*hLedNameKey,
                              REG_METACYCLE_VALUE, 
                              0, 
                              REG_DWORD,
                              (CONST BYTE*) &dwPreMetaCycleValue,
                              sizeof(DWORD));

    if (dwReturn !=  ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Invalid Meta Cycle Test - Restoring Metacycle Value Failed"));

        dwReturnValue = TPR_FAIL;
    }

    if (TPR_PASS == dwReturnValue)
    {
        SUCCESS("Invalid MetaCycle Test - Driver successfully handles the Invalid MetaCycle Setting");
    }

    return dwReturnValue;
}

//*****************************************************************************
DWORD InvalidBlinkableValidMetaCycleTest(HKEY* hLedNameKey, DWORD dwLedNum)
//
//  Parameters: LedNameKey, Nled Number
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_SKIP
//
//  Set the value of the MetaCycle registry key to 1, and set the Blinkable
//  registry key to Zero. The system should handle this scenario gracefully.
//
//*****************************************************************************
{
    //Declaring the variables
    DWORD dwReturnValue = TPR_FAIL;
    DWORD dwReturn = TPR_FAIL;
    DWORD dwInvalidBlinkValue = ZERO_BLINKABLE;
    DWORD dwPreBlinkValue = 0;
    DWORD dwValidMetaCycleValue = VALID_METACYCLE_VALUE;
    DWORD dwPreMetaCycleValue;
    DWORD dwLen = sizeof(DWORD);

    //Querying the previous MetaCycle value
    dwReturnValue = RegQueryValueEx(*hLedNameKey,
                                    REG_METACYCLE_VALUE,
                                    NULL,
                                    NULL,
                                    (LPBYTE) &dwPreMetaCycleValue,
                                    &dwLen);

    if(dwReturnValue != ERROR_SUCCESS)
    {
        //Checking if the MetaCycle field is present
        dwReturnValue = RegQueryValueEx(*hLedNameKey,
                                        REG_METACYCLE_VALUE,
                                        NULL,
                                        NULL,
                                        (LPBYTE) &dwPreMetaCycleValue,
                                        &dwLen);

        if(dwReturnValue != ERROR_FILE_NOT_FOUND)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("MetaCycle field not present, Skipping the Invalid Blinkable Valid Metacycle test"));

            return TPR_SKIP;
        }

        FAIL("Invalid Blinkable Valid Metacycle test - Querying For Existing Metacycle Value Failed");
        return TPR_FAIL;
    }

    //Querying the previous blinkable value
    dwReturnValue = RegQueryValueEx(*hLedNameKey,
                                    REG_BLINKABLE_VALUE,
                                    NULL,
                                    NULL,
                                    (LPBYTE) &dwPreBlinkValue,
                                    &dwLen);

    if(dwReturnValue != ERROR_SUCCESS)
    {
        FAIL("Invalid Blinkable Valid Metacycle test - Querying For Existing blinkable Value Failed");
        return TPR_FAIL;
    }

    //Setting the invalid metacycle value
    dwReturnValue = RegSetValueEx (*hLedNameKey,
                                   REG_METACYCLE_VALUE,
                                   0,
                                   REG_DWORD,
                                   (CONST BYTE*) &dwValidMetaCycleValue,
                                   sizeof(DWORD));

    if (dwReturnValue != ERROR_SUCCESS)
    {
        FAIL("Invalid Blinkable Valid Metacycle test - Setting Valid Metacycle Value Failed");
        return TPR_FAIL;
    }

    //Setting the invalid Blinkable value
    dwReturnValue = RegSetValueEx (*hLedNameKey,
                                   REG_BLINKABLE_VALUE,
                                   0,
                                   REG_DWORD,
                                   (CONST BYTE*) &dwInvalidBlinkValue,
                                   sizeof(DWORD));

    if (dwReturnValue != ERROR_SUCCESS)
    {
        dwReturnValue = RegSetValueEx (*hLedNameKey,
                                       REG_METACYCLE_VALUE,
                                       0,
                                       REG_DWORD,
                                       (CONST BYTE*) &dwPreMetaCycleValue,
                                       sizeof(DWORD));

        //Restoring back the previous MetaCycle value
        if(dwReturnValue != ERROR_SUCCESS)
        {
            FAIL("Invalid Blinkable Valid Metacycle test - Setting Existing-MetaCycle value Failed");
        }

        g_pKato->Log(LOG_DETAIL,
                     TEXT("Invalid Blinkable Valid Metacycle test - Setting Invalid Blinkable Value Failed"));

        return TPR_FAIL;
    }

    //Checking the working status of the NledGetDeviceInfo and NledSetDevice
    dwReturnValue = TestNLEDSetGetDeviceInfo(dwLedNum);

    if (dwReturnValue != TPR_PASS)
    {
        FAIL("Invalid Blinkable Valid Metacycle test - NLED Set/Get Failed");
    }

    //Restoring back the previous metacycle value
    dwReturn = RegSetValueEx (*hLedNameKey,
                              REG_METACYCLE_VALUE,
                              0,
                              REG_DWORD,
                              (CONST BYTE*) &dwPreMetaCycleValue,
                              sizeof(DWORD));

    if (dwReturn !=  ERROR_SUCCESS)
    {
        FAIL("Invalid Blinkable Valid Metacycle test - Restoring Existing metacycle value Failed");
        dwReturnValue = TPR_FAIL;
    }

    //Restoring back the previous Blinkable value
    dwReturn = RegSetValueEx (*hLedNameKey,
                              REG_BLINKABLE_VALUE,
                              0,
                              REG_DWORD,
                              (CONST BYTE*) &dwPreBlinkValue,
                              sizeof(DWORD));

    if (dwReturn !=  ERROR_SUCCESS)
    {
        FAIL("Invalid Blinkable Valid Metacycle test - Restoring Existing blinkable value Failed");
        dwReturnValue = TPR_FAIL;
    }

    if (TPR_PASS == dwReturnValue)
    {
        SUCCESS("Invalid Blinkable Valid Metacycle Test: Driver successfully handles the Invalid Blinkable Setting");
    }

    return dwReturnValue;
}

//*****************************************************************************
DWORD InvalidBlinkableValidAdjustTypeTest(HKEY* hLedNameKey, DWORD dwLedNum)
//
//  Parameters: NledName key, Nled Num
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_SKIP
//
//  Set the value of the AdjustType registry key to (1 or 2), and set the
//  Blinkable registry key to Zero. The system should handle this scenario gracefully.
//
//*****************************************************************************
{
    //Declaring the variables
    DWORD dwReturnValue = TPR_FAIL;
    DWORD dwReturn = TPR_FAIL;
    DWORD dwInvalidBlinkValue = 0;
    DWORD dwPreBlinkValue = 0;
    DWORD dwValidAdjustTypeValue = VALID_ADJUST_TYPE_VALUE;
    DWORD dwPreAdjustTypeValue;
    DWORD dwLen = sizeof(DWORD);

    //Querying for the pre Adjusttype value
    dwReturnValue = RegQueryValueEx(*hLedNameKey,
                                    REG_ADJUSTTYPE_VALUE,
                                    NULL,
                                    NULL,
                                    (LPBYTE) &dwPreAdjustTypeValue,
                                    &dwLen);

    if(dwReturnValue != ERROR_SUCCESS)
    {
        //Checking if the AdjustType field is present
        dwReturnValue = RegQueryValueEx(*hLedNameKey,
                                        REG_ADJUSTTYPE_VALUE,
                                        NULL,
                                        NULL,
                                        (LPBYTE) &dwPreAdjustTypeValue,
                                        &dwLen);

        if(dwReturnValue != ERROR_FILE_NOT_FOUND)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("AdjustType field not present, Skipping the Invalid Blinkable Valid AdjustType Test"));

            return TPR_SKIP;
        }

        FAIL("Invalid Blinkable Valid AdjustType Test - Querying For Existing adjusttype Value Failed");
        return TPR_FAIL;
    }

    //Querying for the existing blinkable value
    dwReturnValue = RegQueryValueEx(*hLedNameKey,
                                    REG_BLINKABLE_VALUE,
                                    NULL,
                                    NULL,
                                    (LPBYTE) &dwPreBlinkValue,
                                    &dwLen);

    if(dwReturnValue != ERROR_SUCCESS)
    {
        FAIL("Invalid Blinkable Valid AdjustType Test - Querying For Existing blinkable Value Failed");
        return TPR_FAIL;
    }

    //Setting the invalid adjusttype value
    dwReturnValue = RegSetValueEx (*hLedNameKey,
                                   REG_ADJUSTTYPE_VALUE,
                                   0,
                                   REG_DWORD,
                                   (CONST BYTE*) &dwValidAdjustTypeValue,
                                   sizeof(DWORD));

    if (dwReturnValue != ERROR_SUCCESS)
    {
        FAIL("Invalid Blinkable Valid AdjustType Test - Setting valid Adjust Value Failed");
        return TPR_FAIL;
    }

    //Setting the invalid Blinkable value
    dwReturnValue = RegSetValueEx (*hLedNameKey,
                                   REG_BLINKABLE_VALUE,
                                   0,
                                   REG_DWORD,
                                   (CONST BYTE*) &dwInvalidBlinkValue,
                                   sizeof(DWORD));

    if (dwReturnValue != ERROR_SUCCESS)
    {
        //Restoring back the previous adjusttype value
        dwReturnValue = RegSetValueEx (*hLedNameKey,
                                       REG_ADJUSTTYPE_VALUE,
                                       0,
                                       REG_DWORD,
                                       (CONST BYTE*) &dwPreAdjustTypeValue,
                                       sizeof(DWORD));

        if(dwReturnValue != ERROR_SUCCESS)
        {
            FAIL("Invalid Blinkable Valid AdjustType Test - Setting existing Adjust value Failed");
        }

        FAIL("Invalid Blinkable Valid AdjustType Test - Setting Invalid Blink Value Failed");
        return TPR_FAIL;
    }

    //Checking for the working status of NledGetDeviceInfo and NledSetDevice
    dwReturnValue = TestNLEDSetGetDeviceInfo(dwLedNum);

    if (dwReturnValue != TPR_PASS)
    {
        FAIL("Invalid Blinkable Valid AdjustType Test - NLED Set/Get Failed");
    }

    //Restoring back the previous adjusttype value
    dwReturn = RegSetValueEx (*hLedNameKey,
                              REG_ADJUSTTYPE_VALUE,
                              0,
                              REG_DWORD,
                              (CONST BYTE*) &dwPreAdjustTypeValue,
                              sizeof(DWORD));

    if (dwReturn !=  ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Invalid Blinkable Valid AdjustType Test - Restoring existing adjusttype value Failed"));

        dwReturnValue = TPR_FAIL;
    }

    //Restoring back the previous blinkable value
    dwReturn = RegSetValueEx (*hLedNameKey,
                              REG_BLINKABLE_VALUE, 
                              0, 
                              REG_DWORD,
                              (CONST BYTE*) &dwPreBlinkValue,
                              sizeof(DWORD));

    if (dwReturn !=  ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Invalid Blinkable Valid AdjustType Test - Restoring existing blinkable value Failed"));

        dwReturnValue = TPR_FAIL;
    }

    if (TPR_PASS == dwReturnValue)
    {
        SUCCESS("Invalid Blinkable Valid AdjustType Test: Driver Successfully handles the Invalid Blinkable setting");
    }

    return dwReturnValue;
}

//*****************************************************************************
DWORD InvalidBlinkableValidCycleAdjustTest(HKEY* hLedNameKey, DWORD dwLedNum)
//
//  Parameters: Led Key, Led Num
//
//  Return Value: TPR_PASS, TPR_FAIL, TPR_SKIP
//
//  Set a valid value for the CycleAdjust key in the registry and set the
//  Blinkable and SWBlinkCtrl registry keys to zero. The system should handle this
//  scenario gracefully.
//
//*****************************************************************************
{
    //Declaring the variables
    DWORD dwReturnValue = TPR_FAIL;
    DWORD dwReturn = 0;
    DWORD dwInvalidBlinkValue = ZERO_BLINKABLE;
    DWORD dwPreBlinkValue = 0;
    DWORD dwInvalidSWBlinkCtrlValue = ZERO_BLINKABLE;
    DWORD dwPreSWBlinkCtrlValue = 0;
    DWORD dwValidCycleAdjustValue = VALID_CYCLEADJUST_VALUE;
    DWORD dwPreCycleAdjustValue = 0;
    DWORD dwLen = sizeof(DWORD);

    //Querying the Pre cycleadjust Value
    dwReturnValue = RegQueryValueEx(*hLedNameKey,
                                    REG_CYCLEADJUST_VALUE,
                                    NULL,
                                    NULL,
                                    (LPBYTE) &dwPreCycleAdjustValue,
                                    &dwLen);

    if(dwReturnValue == ERROR_FILE_NOT_FOUND)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("CycleAdjust field not present, Skipping the Invalid Blinkable And Valid CycleAdjust Test"));

        return TPR_SKIP;
    }
    else if(dwReturnValue != ERROR_SUCCESS)
    {
        FAIL("Invalid Blinkable And Valid CycleAdjust - Querying For existing CycleAdjustValue Failed");
        return TPR_FAIL;
    }

    //Querying the previous blinkable value
    dwReturnValue = RegQueryValueEx(*hLedNameKey,
                                    REG_BLINKABLE_VALUE, 
                                    NULL, 
                                    NULL, 
                                    (LPBYTE) &dwPreBlinkValue,
                                    &dwLen);

    if(dwReturnValue != ERROR_SUCCESS)
    {
        FAIL("Invalid Blinkable And Valid CycleAdjust - Querying For existing blinkable Value Failed");
        return TPR_FAIL;
    }

    //Querying the previous SWBlinkctrl value
    dwReturnValue = RegQueryValueEx(*hLedNameKey,
                                    REG_SWBLINKCTRL_VALUE,
                                    NULL,
                                    NULL,
                                    (LPBYTE) &dwPreSWBlinkCtrlValue,
                                    &dwLen);

    if(dwReturnValue == ERROR_FILE_NOT_FOUND)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("SWBlinkCtrl field not present, Skipping the Invalid Blinkable And Valid CycleAdjust Test"));

        return TPR_SKIP;
    }
    else if(dwReturnValue != ERROR_SUCCESS)
    {
        FAIL("Invalid Blinkable And Valid CycleAdjust Test - Querying For existing SWBlinkCtrl Value Failed");
        return TPR_FAIL;
    }

    //Setting the Invalid cycleadjust Value
    dwReturnValue = RegSetValueEx (*hLedNameKey,
                                   REG_CYCLEADJUST_VALUE,
                                   0,
                                   REG_DWORD,
                                   (CONST BYTE*) &dwValidCycleAdjustValue,
                                   sizeof(DWORD));

    if (dwReturnValue != ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Invalid Blinkable And valid CycleAdjust Test - Setting Valid cycleAdjustValue Failed"));

        return TPR_FAIL;
    }

    //Setting the Invalid Blinkable Value
    dwReturnValue = RegSetValueEx (*hLedNameKey,
                                   REG_BLINKABLE_VALUE,
                                   0,
                                   REG_DWORD,
                                   (CONST BYTE*) &dwInvalidBlinkValue,
                                   sizeof(DWORD));

    if (dwReturnValue != ERROR_SUCCESS)
    {
        //Restoring back the previous cycleadjust value
        dwReturnValue = RegSetValueEx (*hLedNameKey,
                                       REG_CYCLEADJUST_VALUE, 
                                       0, 
                                       REG_DWORD,
                                       (CONST BYTE*) &dwPreCycleAdjustValue,
                                       sizeof(DWORD));

        if(dwReturnValue != ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Invalid Blinkable And valid CycleAdjust Test - Setting pre cycleadjustvalue Failed"));
        }

        FAIL("Invalid Blinkable And valid CycleAdjust Test - Setting Invalid blinkable Value Failed");
        return TPR_FAIL;
    }

    //Setting the invalid values
    dwReturnValue = RegSetValueEx (*hLedNameKey,
                                   REG_SWBLINKCTRL_VALUE,
                                   0,
                                   REG_DWORD,
                                   (CONST BYTE*) &dwInvalidSWBlinkCtrlValue,
                                   sizeof(DWORD));

    if (dwReturnValue != ERROR_SUCCESS)
    {
        //Restoring back the previous values
        dwReturnValue = RegSetValueEx (*hLedNameKey,
                                       REG_CYCLEADJUST_VALUE,
                                       0,
                                       REG_DWORD,
                                       (CONST BYTE*) &dwPreCycleAdjustValue,
                                       sizeof(DWORD));

        if(dwReturnValue != ERROR_SUCCESS)
        {
            FAIL("Invalid Blinkable And valid CycleAdjust Test - Setting existing CycleAdjust Value Failed");
        }

        //Restoring back the previous values
        dwReturnValue = RegSetValueEx (*hLedNameKey,
                                       REG_BLINKABLE_VALUE,
                                       0,
                                       REG_DWORD,
                                       (CONST BYTE*) &dwPreBlinkValue,
                                       sizeof(DWORD));

        if(dwReturnValue != ERROR_SUCCESS)
        {
            FAIL("Invalid Blinkable And valid CycleAdjust Test - Setting preBlinkable Value Failed");
        }

        FAIL("Invalid Blinkable And valid CycleAdjust Test - Setting Invalid SWBlinkctrl Value Failed");
        return TPR_FAIL;
    }

    //Testing the working status for NLED GetDeviceInfo And SetDevice
    dwReturnValue = TestNLEDSetGetDeviceInfo(dwLedNum);

    if (dwReturnValue != TPR_PASS)
    {
        FAIL("Invalid Blinkable And Valid CycleAdjust Test: NLED Set/Get Failed");
    }

    //Restoring the Previous Values
    dwReturn = RegSetValueEx (*hLedNameKey,
                              REG_CYCLEADJUST_VALUE,
                              0,
                              REG_DWORD,
                              (CONST BYTE*) &dwPreCycleAdjustValue,
                              sizeof(DWORD));

    if (dwReturn != ERROR_SUCCESS)
    {
        FAIL("Invalid Blinkable And Valid CycleAdjust Test - Restoring existing CycleAdjust Value Failed");
        dwReturnValue = TPR_FAIL;
    }

    //Restoring the Previous Values
    dwReturn = RegSetValueEx (*hLedNameKey,
                              REG_BLINKABLE_VALUE,
                              0,
                              REG_DWORD,
                              (CONST BYTE*) &dwPreBlinkValue,
                              sizeof(DWORD));

    if (dwReturn != ERROR_SUCCESS)
    {
        FAIL("Invalid Blinkable And Valid CycleAdjust Test - Restoring existing Blinkable Value Failed");
        dwReturnValue = TPR_FAIL;
    }

    //Restoring the Previous Values
    dwReturn = RegSetValueEx (*hLedNameKey,
                              REG_SWBLINKCTRL_VALUE,
                              0,
                              REG_DWORD,
                              (CONST BYTE*) &dwPreSWBlinkCtrlValue,
                              sizeof(DWORD));

    if (dwReturn != ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Invalid Blinkable And Valid CycleAdjust Test - Restoring preSWBlinkCtrl Value Failed"));

        dwReturnValue = TPR_FAIL;
    }

    if (dwReturnValue == TPR_PASS)
    {
        SUCCESS("Invalid Blinkable And Valid CycleAdjust Test: Driver Successfully handles the Invalid Blinkable setting");
    }

    return dwReturnValue;
}

//*****************************************************************************
DWORD TestNLEDSetGetDeviceInfo(DWORD dwLedNum)
//
//  Parameters: Led Num
//
//  Return Value: TPR_PASS, TPR_FAIL
//
//  Function to test the NLED Set and Get functions
//
//*****************************************************************************
{
    //Declaring the variables
    DWORD dwReturnValue = TPR_FAIL;
    BOOL fStatus = FALSE;
    NLED_SETTINGS_INFO ledSettingsInfo_Set = {0};
    NLED_SETTINGS_INFO ledSettingsInfo_Get = {0};

    //Initializing the led structures members
    ledSettingsInfo_Set.LedNum = dwLedNum;
    ledSettingsInfo_Get.LedNum = dwLedNum;
    ledSettingsInfo_Set.OffOnBlink = LED_ON;

    //Calling the NledSetDevice function to change the LED settings
    fStatus = NLedSetDevice(NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Set);

    if(!fStatus)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Nled Driver failed to change the Nled settings"));

        dwReturnValue = TPR_FAIL;
    }

    //Calling the NledGetDevice to get the current settings
    fStatus = NLedGetDeviceInfo(NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Get);

    if(fStatus)
    {
        if(LED_ON == ledSettingsInfo_Get.OffOnBlink)
        {
            dwReturnValue = TPR_PASS;
        }
    }

    return dwReturnValue;
}

//*****************************************************************************
DWORD GetUserResponse(NLED_SUPPORTS_INFO ledSupportsInfo)
//
// Parameters:  Nled Supports Info structure
//
// Return Value: TPR_FAIL, TPR_PASS
//
// Function used for user intervention and to ask the user to verify the
// Nled supports Info.
//
//*****************************************************************************
{
    DWORD dwMboxValue      = 0;
    TCHAR szMessage[ 500 ] = TEXT("");

    //Prompting the user with the supports Info data and asking to verify it
    swprintf_s(szMessage, _countof(szMessage), 
              TEXT("The Nled Supports Info retrieved for Nled %u is fAdjustTotalCycleTime: %s, \
              fAdjustOnTime: %s, \
              fAdjustOffTime: %s, \
              fMetaCycleOn: %s, \
              fMetaCycleOff: %s \
              Is this correct?"),ledSupportsInfo.LedNum,
              ((1 == ledSupportsInfo.fAdjustTotalCycleTime) ? TEXT("TRUE") : TEXT("FALSE")),
              ((1 == ledSupportsInfo.fAdjustOnTime) ? TEXT("TRUE") : TEXT("FALSE")),
              ((1 == ledSupportsInfo.fAdjustOffTime) ? TEXT("TRUE") : TEXT("FALSE")),
              ((1 == ledSupportsInfo.fMetaCycleOn) ? TEXT("TRUE") : TEXT("FALSE")),
              ((1 == ledSupportsInfo.fMetaCycleOff) ? TEXT("TRUE") : TEXT("FALSE")));

    dwMboxValue = MessageBox(NULL,
                             szMessage,
                             g_szInputMsg,
                             MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND);

    if(IDYES == dwMboxValue)
    {
        g_pKato->Log(LOG_DETAIL, 
                     TEXT("\tUser confirmed the NLED supports settings for Nled %u"),
                     ledSupportsInfo.LedNum);

        return TPR_PASS;
    }
    else
    {
        g_pKato->Log(LOG_DETAIL, 
                     TEXT("\tUser rejected the NLED supports settings for Nled %u"),
                     ledSupportsInfo.LedNum);

        return TPR_FAIL;
    }
}

//*****************************************************************************
DWORD BlinkNled(UINT nLedNum)
//
//  Parameters: LedNum
//
//  Return Value: TPR_PASS on success
//                TPR_FAIL on failure
//
// This function is used to blink the Nled having specified led Num.
//***************************************************************************
{
    //Declaring the variables
    DWORD dwReturnValue = TPR_PASS;
    BOOL fStatus = FALSE;
    NLED_SETTINGS_INFO *ledSettingsInfo_Set = new NLED_SETTINGS_INFO[ 1 ];
    ZeroMemory(ledSettingsInfo_Set, sizeof(NLED_SETTINGS_INFO));

    NLED_SUPPORTS_INFO* ledSupportsInfo = new NLED_SUPPORTS_INFO[ 1 ];
    ZeroMemory(ledSupportsInfo, sizeof(NLED_SUPPORTS_INFO));

    NLED_SETTINGS_INFO ledSettingsInfo_Get = { 0 };

    //Getting the supports Info for the LED
    ledSupportsInfo->LedNum = nLedNum;
    fStatus = NLedGetDeviceInfo(NLED_SUPPORTS_INFO_ID, ledSupportsInfo);

    if(!fStatus)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to get the LED supports info for %u NLED"), 
                     nLedNum);

        dwReturnValue = TPR_FAIL;
        goto CLEANUP;
    }

    //Setting the blinking parameters
    if(ledSupportsInfo->fAdjustTotalCycleTime)
    {
        ledSettingsInfo_Set->TotalCycleTime = ((ledSupportsInfo->lCycleAdjust) * 2);
    }//fAdjustTotalCycleTime

    if(ledSupportsInfo->fAdjustOnTime)
    {
        ledSettingsInfo_Set->OnTime = ledSupportsInfo->lCycleAdjust;
    }//fAdjustOnTime

    if(ledSupportsInfo->fAdjustOffTime)
    {
        ledSettingsInfo_Set->OffTime = ledSupportsInfo->lCycleAdjust;
    }//fAdjustOffTime

    if(ledSupportsInfo->fMetaCycleOn)
    {
        ledSettingsInfo_Set->MetaCycleOn = META_CYCLE_ON;
    }//fMetaCycleOn

    if(ledSupportsInfo->fMetaCycleOff)
    {
        ledSettingsInfo_Set->MetaCycleOff = META_CYCLE_OFF;
    }//fMetaCycleOff

    //Setting the lednum and led state
    ledSettingsInfo_Set->LedNum = nLedNum;
    ledSettingsInfo_Set->OffOnBlink = LED_BLINK;

    //Calling the NledSetDevice function to change LED settings
    fStatus = NLedSetDevice(NLED_SETTINGS_INFO_ID, ledSettingsInfo_Set);

    if(!fStatus)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("NledSetDevice failed to change the LED settings for LED : %u"),
                     nLedNum);

        dwReturnValue = TPR_FAIL;
        goto CLEANUP;
    }

    ledSettingsInfo_Get.LedNum = nLedNum;
    fStatus = NLedGetDeviceInfo(NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Get);

    if(fStatus)
    {
        if(LED_BLINK != ledSettingsInfo_Get.OffOnBlink)
        {
            dwReturnValue = TPR_FAIL;
            goto CLEANUP;
        }
    }
    else
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("NledGetDeviceInfo failed to get the LED settings for LED : %u"),
                     nLedNum);

        dwReturnValue = TPR_FAIL;
        goto CLEANUP;
    }

CLEANUP:

    //Releasing the memory
    if(ledSettingsInfo_Set != NULL)
    {
        delete [] ledSettingsInfo_Set;
        ledSettingsInfo_Set = NULL;
    }

    if(ledSupportsInfo != NULL)
    {
        delete [] ledSupportsInfo;
        ledSupportsInfo = NULL;
    }

    return dwReturnValue;
}

//*****************************************************************************
INT GetBlinkingNledNum()
//
//  Parameters: None
//
//  Return Value: The LedNum that supports blinking
//
//  This function returns the first led-Num in the list which supports
//  blinking (i.e. led which has Blinkable = 1 in the registry). Function
//  returns -1 if no nled present which has Blinkable =1.
//***************************************************************************
{
    //Declaring the variables
    DWORD dwReturnValue = TPR_PASS;
    BOOL fStatus = FALSE;
    INT nBlinkingLedNum = 0;
    DWORD dwRet = 0;
    NLED_SUPPORTS_INFO *ledSupportsInfo = new NLED_SUPPORTS_INFO[ 1 ];
    ZeroMemory(ledSupportsInfo, sizeof(NLED_SUPPORTS_INFO));

    //Also getting the Nleds having SWBlinkCtrl enabled
    DWORD *pSWBlinkCtrlNledNums = new DWORD[GetLedCount()];
    dwRet = GetSWBlinkCtrlNleds(pSWBlinkCtrlNledNums);

    if(dwRet != TPR_PASS)
    {
        //Logging the error output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to Query the SWBlinkCtrl for all the Nleds"));

        dwReturnValue = TPR_FAIL;
        goto CLEANUP;
    }

    //Getting the supports Info for the LEDs
    for(INT nCount = 0; nCount < (INT) GetLedCount(); nCount++)
    {
        ledSupportsInfo->LedNum = nCount;
        dwRet = NLedGetDeviceInfo(NLED_SUPPORTS_INFO_ID, ledSupportsInfo);

        if(!dwRet)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to get the LED supports info for %u NLED"), 
                         nCount);

            goto CLEANUP;
        }

        //Check if the NLED supports blinking
        if(ledSupportsInfo->lCycleAdjust > 0)
        {
            //Checking if the Nled has SWBlinkCtrl configured
            if(pSWBlinkCtrlNledNums[nCount] != VALID_SWBLINKCTRL)
            {
                //Returning the led num which supports blinking and has Blinkable = 1
                fStatus = TRUE;
                nBlinkingLedNum = nCount;
                break;
            }
        }
    }

CLEANUP:

    if(ledSupportsInfo != NULL)
    {
        delete [] ledSupportsInfo;
        ledSupportsInfo = NULL;
    }

    if(pSWBlinkCtrlNledNums != NULL)
    {
        delete[] pSWBlinkCtrlNledNums;
        pSWBlinkCtrlNledNums = NULL;
    }

    if(fStatus)
    {
        return nBlinkingLedNum;
    }

    return NO_MATCHING_NLED;
}

//*****************************************************************************
INT GetNonBlinkingNledNum()
//
//  Parameters: None
//
//  Return Value: The LedNum that does not supports blinking
//
//  This function returns the first led-Num in the list which does not
//  supports blinking (i.e. Led not having blinkable = 1 in the registry).
//  Function returns -1 if no such Nled present.
//***************************************************************************
{
    //Declaring the variables
    BOOL fStatus = FALSE;
    NLED_SUPPORTS_INFO *ledSupportsInfo = new NLED_SUPPORTS_INFO[ 1 ];
    ZeroMemory(ledSupportsInfo, sizeof(NLED_SUPPORTS_INFO));

    //Getting the supports Info for the LEDs
    for(INT nCount = 0; nCount < (INT) GetLedCount(); nCount++)
    {
        ledSupportsInfo->LedNum = nCount;
        fStatus = NLedGetDeviceInfo(NLED_SUPPORTS_INFO_ID, ledSupportsInfo);

        if(!fStatus)
        {
            g_pKato->Log(LOG_DETAIL,
                         TEXT("Unable to get the LED supports info for %u NLED"), 
                         nCount);

            goto CLEANUP;
        }

        //Check if the NLED supports blinking
        if(ledSupportsInfo->lCycleAdjust <= 0)
        {
            delete[] ledSupportsInfo;
            ledSupportsInfo = NULL;

            //Returning the led num which does not supports blinking
            return nCount;
        }
    }

CLEANUP:

    if(ledSupportsInfo)
    {
        delete [] ledSupportsInfo;
        ledSupportsInfo = NULL;
    }

    //No NLED that does not support Blinking
    return NO_MATCHING_NLED;
}

//****************************************************************************
DWORD CheckSystemBehavior(UINT nLedNum, UINT nLeds)
//
//  Parameters: LedNum, Number of LEDs to be tested
//
//  Return Value: TPR_PASS On Success
//                TPR_FAIL on failure
//
//  To call NledSetDevice multiple times for the Nleds specified using threads
//
//*****************************************************************************
{
    //Declaring the variables for creating multiple threads
    DWORD dwThreadId[MAX_THREAD_COUNT];
    UINT nThreadsPerLed = 0;
    NLED_SETTINGS_INFO ledSettingsInfo_Set = {0};
    NLED_SUPPORTS_INFO ledSupportsInfo = {0};
    BOOL fStatus = FALSE;

    //Dividing the threads equally among all the LEDs
    nThreadsPerLed = (UINT) (MAX_THREAD_COUNT/nLeds);

    //Creating the threads
    ledSettingsInfo_Set.LedNum = nLedNum;

    //Creating not more than MAX_THREAD_COUNT threads
    while(g_nThreadCount < MAX_THREAD_COUNT)
    {
        //The LED state can take a value 0, 1 or 2
        ledSettingsInfo_Set.OffOnBlink = g_nThreadCount % (LED_BLINK + 1);

        //If the led state is blink then adjusting the blink settings
        if(LED_BLINK == ledSettingsInfo_Set.OffOnBlink)
        {
            //Getting the supports Info for the LED
            ledSupportsInfo.LedNum = nLedNum;

            fStatus = NLedGetDeviceInfo(NLED_SUPPORTS_INFO_ID, &ledSupportsInfo);

            if(!fStatus)
            {
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Unable to get the LED supports info for %u NLED"),
                             nLedNum);

                return TPR_FAIL;
            }

            //Check if the NLED supports blinking
            if(ledSupportsInfo.lCycleAdjust > 0)
            {
                //Setting the blinking parameters
                if(ledSupportsInfo.fAdjustTotalCycleTime)
                {
                    ledSettingsInfo_Set.TotalCycleTime = ((ledSupportsInfo.lCycleAdjust) * (CYCLETIME_CHANGE_VALUE * 2));
                }

                if(ledSupportsInfo.fAdjustOnTime)
                {
                    ledSettingsInfo_Set.OnTime = ((ledSupportsInfo.lCycleAdjust) * (CYCLETIME_CHANGE_VALUE));
                }

                if(ledSupportsInfo.fAdjustOffTime)
                {
                    ledSettingsInfo_Set.OffTime = ((ledSupportsInfo.lCycleAdjust) * (CYCLETIME_CHANGE_VALUE));
                }

                if(ledSupportsInfo.fMetaCycleOn)
                {
                    ledSettingsInfo_Set.MetaCycleOn = META_CYCLE_ON;
                }

                if(ledSupportsInfo.fMetaCycleOff)
                {
                    ledSettingsInfo_Set.MetaCycleOff = META_CYCLE_OFF;
                }
            }
            else
            {
                //If the Nled Does not supports blinking then turn On the Nled instead of blinking
                ledSettingsInfo_Set.OffOnBlink = LED_ON;
            }
        }

        //Creating multiple threads
        g_hThreadHandle[g_nThreadCount] = CreateThread(0,
                                                       0,
                                                       (LPTHREAD_START_ROUTINE)ThreadProc,
                                                       &ledSettingsInfo_Set,
                                                       0,
                                                       &dwThreadId[g_nThreadCount]);
        //Waiting for 1000 milli seconds
        Sleep(SLEEP_FOR_ONE_SECOND);

        //Creating not more than MAX_THREAD_COUNT even if the NLEDs are more than 1
        //Equally dividing the number of threads available among all the leds
        if(((g_nThreadCount % nThreadsPerLed) == 0) &&
            (g_nThreadCount != 0) &&
            (nLeds == GetLedCount()))
        {
            //Incrementing the thread count
            g_nThreadCount++;
            break;
        }

        //Incrementing the thread count
        g_nThreadCount++;
    }

    return TPR_PASS;
}

//****************************************************************************
BOOL WINAPI ThreadProc(NLED_SETTINGS_INFO* ledSettingsInfo_Set)
//
//  Parameters: NLED Setting Information to set
//
//  Return Value: TRUE on success
//                FALSE on failure
//
//  This thread proc actually invokes the NLedSetDevice
//
//*****************************************************************************
{
    //Declaring the variables
    BOOL fStatus = FALSE;
    NLED_SETTINGS_INFO tempLedSettingsInfo_Set = {0};
    NLED_SETTINGS_INFO ledSettingsInfo_Get = {0};

    //Copying the structure into local variable
    tempLedSettingsInfo_Set.LedNum = ledSettingsInfo_Set->LedNum;
    tempLedSettingsInfo_Set.OffOnBlink = ledSettingsInfo_Set->OffOnBlink;
    tempLedSettingsInfo_Set.TotalCycleTime = ledSettingsInfo_Set->TotalCycleTime;
    tempLedSettingsInfo_Set.OnTime = ledSettingsInfo_Set->OnTime;
    tempLedSettingsInfo_Set.OffTime = ledSettingsInfo_Set->OffTime;
    tempLedSettingsInfo_Set.MetaCycleOn = ledSettingsInfo_Set->MetaCycleOn;
    tempLedSettingsInfo_Set.MetaCycleOff = ledSettingsInfo_Set->MetaCycleOff;

    //Invoking the NLedSetDevice to change the LED settings
    fStatus = NLedSetDevice(NLED_SETTINGS_INFO_ID, &tempLedSettingsInfo_Set);

    if(!fStatus)
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Nled Driver failed to set the Nled Settings Info for Nled %u"),
                     tempLedSettingsInfo_Set.LedNum);

        //Nled Driver unable to set the Nled settings
        g_fMULTIPLENLEDSETFAILED = TRUE;

        return FALSE;
    }

    //Calling the NledGetDeviceInfo function to retrieve the settings info
    ledSettingsInfo_Get.LedNum = tempLedSettingsInfo_Set.LedNum;
    fStatus = NLedGetDeviceInfo(NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Get);

    if(!fStatus)
    {
        //Logging the output using the kato logging engine
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Nled Driver failed to Get the Nled Settings Info for %u Nled"),
                     ledSettingsInfo_Get.LedNum);

        //Nled Driver unable to get the Nled settings
        g_fMULTIPLENLEDSETFAILED = TRUE;

        return FALSE;
    }

    //Checking if the LED state set and that retrieved are same
    if(ledSettingsInfo_Get.OffOnBlink != tempLedSettingsInfo_Set.OffOnBlink)
    {
        //Nled Driver retrieved wrong settings
        g_fMULTIPLENLEDSETFAILED = TRUE;
    }

    //If the led state is blink then also checking the blinking parameters
    if(LED_BLINK == tempLedSettingsInfo_Set.OffOnBlink)
    {
        //Checking if the settings set are equal to the settings retrieved
        if((ledSettingsInfo_Get.MetaCycleOff != tempLedSettingsInfo_Set.MetaCycleOff) ||
           (ledSettingsInfo_Get.MetaCycleOn != tempLedSettingsInfo_Set.MetaCycleOn)   ||
           (ledSettingsInfo_Get.OffTime != tempLedSettingsInfo_Set.OffTime)           ||
           (ledSettingsInfo_Get.OnTime != tempLedSettingsInfo_Set.OnTime)             ||
           (ledSettingsInfo_Get.TotalCycleTime != tempLedSettingsInfo_Set.TotalCycleTime))
        {
            //Nled Driver retrieved wrong settings
            g_fMULTIPLENLEDSETFAILED = TRUE;
        }
    }
    return TRUE;
}

//*****************************************************************************
BOOL LoadUnloadTestFunc()
//
//  Parameters: None
//
//  Load and Unload an instance of Nled Driver
//
//  Return Value: True/False
//
//*****************************************************************************
{
    BOOL fReturn = TRUE;
    DWORD dwParam = 0;
    HANDLE hNledDrvr = NULL;

    hNledDrvr = ActivateDeviceEx(REG_NLED_KEY_PATH, NULL, 0, &dwParam);

    if(!hNledDrvr)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to load an new instance of Nled Driver"));

        fReturn = FALSE;
    }

    BOOL bretval = DeactivateDevice(hNledDrvr);

    if(!bretval)
    {
        g_pKato->Log(LOG_DETAIL,
                     TEXT("Unable to unload an instance of Nled Driver"));

        fReturn = FALSE;
    }

    return fReturn;
}

//*****************************************************************************
HANDLE GetDeviceHandleFromGUID(LPCTSTR pszBusGuid)
//
//  Parameters: LPCTSTR pszBusGuid - GUID of the Nled Device Driver
//
//  Return Value: HANDLE to Nled Driver
//
//*****************************************************************************
{
    //Declaring the variables
    HANDLE hDevice = NULL;

    union {
           BYTE rgbGuidBuffer[sizeof(GUID) + 4];
           GUID guidBus;
    } u = { 0 };

    LPGUID pguidBus = &u.guidBus;

    // Parse the GUID
    swscanf_s(pszBusGuid, 
                        SVSUTIL_GUID_FORMAT, 
                        SVSUTIL_PGUID_ELEMENTS(&pguidBus));

    // Get a handle to the Nled driver
    DEVMGR_DEVICE_INFORMATION di;
    memset(&di, 0, sizeof(di));
    di.dwSize = sizeof(di);

    if (FindFirstDevice(DeviceSearchByGuid, pguidBus, &di) != INVALID_HANDLE_VALUE) {
        hDevice = CreateFile(di.szLegacyName, 0, 0, NULL, 0, 0, NULL);
    }
    else {
        g_pKato->Log(LOG_DETAIL, TEXT("%s device not found\n"), pszBusGuid);
    }

    return hDevice;
}

//*****************************************************************************
BOOL CallNledDeviceDriverIOCTL(void)
//
//  Parameters: None
//
//  Return Value: True, False
//
//  Function to invoke different Nled Device Driver DeviceIO control messages.
//
//*****************************************************************************
{
    //Declaring the variables
    BOOL fStatus = TRUE;

    //Get the Nled Device Driver Handle
    g_hNledDeviceHandle = GetDeviceHandleFromGUID(NLED_GUID_VALUE);

    if (!g_hNledDeviceHandle || g_hNledDeviceHandle == INVALID_HANDLE_VALUE)
    {
        g_pKato->Log(LOG_DETAIL, 
                     TEXT("Invalid Device handle returned for Nled Device Driver"));

        fStatus = FALSE;
    }

    //Get Power Info capabilities
    POWER_CAPABILITIES PowerCaps;
    PowerCaps.DeviceDx = 0;
    DWORD dwBytesReturned = 0;
    BOOL fRet = DeviceIoControl(g_hNledDeviceHandle,
                                IOCTL_POWER_CAPABILITIES,
                                NULL,
                                0,
                                (LPVOID) &PowerCaps,
                                sizeof(PowerCaps),
                                &dwBytesReturned,
                                NULL);

    if (fRet && PowerCaps.DeviceDx != 0)
    {
        g_pKato->Log(LOG_DETAIL, 
                     TEXT("Nled Device Driver Supports POWER CAPABILITIES IOCTL"));
    }
    else
    {
        g_pKato->Log(LOG_DETAIL, 
                     TEXT("Nled Device Driver does not Supports POWER CAPABILITIES IOCTL"));

        fStatus = FALSE;
    }

    ///Getting supported power states
    UCHAR szSupportedStates = PowerCaps.DeviceDx;

    if (!szSupportedStates)
    {
        g_pKato->Log(LOG_DETAIL, 
                     TEXT("Nled Device Driver does not supports any state changes"));

        fStatus = FALSE;
    }

    CEDEVICE_POWER_STATE OrigState = D0;
    fRet = DeviceIoControl(g_hNledDeviceHandle,
                           IOCTL_POWER_GET,
                           NULL,
                           0,
                           (LPVOID) &OrigState,
                           sizeof(OrigState),
                           &dwBytesReturned,
                           NULL);
    if (!fRet)
    {
        g_pKato->Log(LOG_DETAIL, 
                     TEXT("Nled Device Driver does not Supports POWER GET IOCTL"));

        fStatus = FALSE;
    }
    else
    {
        g_pKato->Log(LOG_DETAIL, 
                     TEXT("Nled Device Driver Supports POWER GET IOCTL"));
    }


    CEDEVICE_POWER_STATE state = D0;

    fRet = DeviceIoControl(g_hNledDeviceHandle,
                           IOCTL_POWER_QUERY,
                           NULL,
                           0,
                           (LPVOID) &state,
                           sizeof(state),
                           &dwBytesReturned,
                           NULL);
    if (!fRet)
    {
        g_pKato->Log(LOG_DETAIL, 
                     TEXT("Nled Device Driver does not supports POWER QUERY IOCTL"));

        fStatus = FALSE;
    }
    else
    {
        g_pKato->Log(LOG_DETAIL, 
                     TEXT("Nled Device Driver supports POWER QUERY IOCTL"));
    }

    for (int i=0; i < (int)PwrDeviceMaximum; i++)
    {
        state = (CEDEVICE_POWER_STATE)i;

        if(VALID_DX(state) && (DX_MASK(state) & szSupportedStates))
        {
            dwBytesReturned = 0;
            fRet = DeviceIoControl(g_hNledDeviceHandle,
                                   IOCTL_POWER_QUERY,
                                   NULL,
                                   0,
                                   (LPVOID) &state,
                                   sizeof(state),
                                   &dwBytesReturned,
                                   NULL);
            if(fRet)
            {
                fRet = DeviceIoControl(g_hNledDeviceHandle,
                                       IOCTL_POWER_SET,
                                       NULL,
                                       0,
                                       (LPVOID) &state,
                                       sizeof(state),
                                       &dwBytesReturned,
                                       NULL);
                if (!fRet)
                {
                    g_pKato->Log(LOG_DETAIL, 
                                 TEXT("Nled Device Driver does not supports POWER SET IOCTL"));

                    fStatus = FALSE;
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL, 
                                 TEXT("Nled Device Driver supports the POWER SET IOCTL"));
                }

                fRet = DeviceIoControl(g_hNledDeviceHandle,
                                       IOCTL_POWER_GET,
                                       NULL,
                                       0,
                                       (LPVOID) &state,
                                       sizeof(state),
                                       &dwBytesReturned,
                                       NULL);

                if ((CEDEVICE_POWER_STATE)i != state)
                {
                    g_pKato->Log(LOG_DETAIL, 
                                 TEXT("Nled Device Driver is unable to set D%u state"), 
                                 i);

                    fStatus = FALSE;
                }
            }
        }
    }

    //Restore original power state
    fRet = DeviceIoControl(g_hNledDeviceHandle,
                           IOCTL_POWER_SET,
                           NULL,
                           0,
                           (LPVOID) &OrigState,
                           sizeof(OrigState),
                           &dwBytesReturned,
                           NULL);

    if (!fRet)
    {
        g_pKato->Log(LOG_DETAIL, 
                     TEXT("Nled Device Driver unable to restore original D%u power state"),
                     OrigState);

        fStatus = FALSE;
    }

    //Sending invalid IOCTL
    DWORD IOCTL_NLED_INVALID = 0xFFFF;

    fRet = DeviceIoControl(g_hNledDeviceHandle,
                           IOCTL_NLED_INVALID,
                           NULL,
                           0,
                           NULL,
                           0,
                           NULL,
                           NULL);

    if (fRet)
    {
        g_pKato->Log(LOG_DETAIL, 
                     TEXT("Nled Device Driver failed to handle a invalid IOCTL"));

        fStatus = FALSE;
    }
    else
    {
        g_pKato->Log(LOG_DETAIL, 
                     TEXT("Nled Device Driver successfully handles an invalid IOCTL"));
    }

    //Close the handle
    if (g_hNledDeviceHandle)
    {
        CloseHandle(g_hNledDeviceHandle);
        g_hNledDeviceHandle = NULL;
    }

    return fStatus;
}

//*****************************************************************************
DWORD GetNledGroupIds(DWORD *pLedNumGroupId)
//
//  Parameters: pLedNumGroupId: pointer to an array to store the group Id of each Nled
//
//  Return Value: TPR_PASS, TPR_FAIL
//
//  Function to get info about the group Ids for each NLED
//
//*****************************************************************************
{
    //Declaring the variables
    DWORD dwConfigKeyReslt=0;
    HKEY hConfigKey = NULL;
    HKEY hLedNameKey = NULL;
    DWORD dwReturnValue = TPR_FAIL;
    DWORD dwLedCount = 0;
    TCHAR szLedName[gc_dwMaxKeyNameLength + 1];
    DWORD dwLedNameSize = gc_dwMaxKeyNameLength + 1;
    DWORD dwLedNum = 0;

    //Opening the Nled Config key
    dwReturnValue = RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                                 REG_NLED_KEY_CONFIG_PATH, 
                                 0,
                                 KEY_ENUMERATE_SUB_KEYS, 
                                 &hConfigKey);

    if(ERROR_SUCCESS == dwReturnValue)
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
            DWORD dwGroupId = 0;
            DWORD dwLen = sizeof(DWORD);

            //Opening the NLED key for Querying the LED parameters
            dwReturnValue = RegOpenKeyEx(hConfigKey, 
                                         szLedName, 
                                         0,
                                         KEY_ALL_ACCESS,
                                         &hLedNameKey);

            if(ERROR_SUCCESS == dwReturnValue)
            {
                //Querying the LedNum parameter
                dwReturnValue = RegQueryValueEx(hLedNameKey, 
                                                REG_LED_NUM_VALUE,
                                                NULL, 
                                                NULL, 
                                                (LPBYTE) &dwLedNum,
                                                &dwLen);
 
                if(dwReturnValue != ERROR_SUCCESS)
                {
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Unable to query the LedNum field for %s"), 
                                 szLedName);

                    dwReturnValue = TPR_FAIL;
                    goto RETURN;
                }

                //Querying the LedGroup
                dwReturnValue = RegQueryValueEx(hLedNameKey, 
                                                REG_LED_GROUP_VALUE,
                                                NULL, 
                                                NULL, 
                                                (LPBYTE) &dwGroupId,
                                                &dwLen);

                if(ERROR_SUCCESS == dwReturnValue)
                {
                    //Checking if the group id is valid
                    if(dwGroupId > 0)
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("The NLED %u is present in group %u"), 
                                     dwLedNum, dwGroupId);

                        if(dwLedNum < GetLedCount())
                        {
                            pLedNumGroupId[dwLedNum] = dwGroupId;
                        }
                        else
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("The NLED index %u is not correct. Please verify the result of VerifyNledIndex test for more details"),
                                         dwLedNum);

                            dwReturnValue = TPR_FAIL;
                            goto RETURN;
                        }
                    }
                    else
                    {
                        //Logging error message using the kato logging engine
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("The NLED %u is not present in any group"), 
                                     dwLedNum);

                        if(dwLedNum < GetLedCount())
                        {
                            pLedNumGroupId[dwLedNum] = 0;
                        }
                        else
                        {
                            g_pKato->Log(LOG_DETAIL,
                                         TEXT("The NLED index %u is not correct. Please verify the result of VerifyNledIndex test for more details"),
                                         dwLedNum);

                            dwReturnValue = TPR_FAIL;
                            goto RETURN;
                        }
                    }
                }
                else if(ERROR_FILE_NOT_FOUND == dwReturnValue)
                {
                    //No LedGroup flag is present
                    if(dwLedNum < GetLedCount())
                    {
                        //Logging error message using the kato logging engine
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("The NLED %u is not present in any group"), 
                                     dwLedNum);

                        pLedNumGroupId[dwLedNum] = 0;
                    }
                    else
                    {
                        g_pKato->Log(LOG_DETAIL,
                                     TEXT("The NLED index %u is not correct. Please verify the result of VerifyNledIndex test for more details"),
                                     dwLedNum);

                        dwReturnValue = TPR_FAIL;
                        goto RETURN;
                    }
                }
                else
                {
                    //Logging error message using the kato logging engine
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Failed to query LedGroup for %s key"), 
                                 szLedName);

                    dwReturnValue = TPR_FAIL;
                    goto RETURN;
                }
            }
            else
            {
                //Logging error message using the kato loggin engine
                g_pKato->Log(LOG_DETAIL,
                             TEXT("Failed to open the %s key"), 
                             szLedName);

                dwReturnValue = TPR_FAIL;
                goto RETURN;
            }

            //Closing the key
            if(hLedNameKey != NULL)
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

        //Successfully Got the Group Ids for all the Nleds
        dwReturnValue = TPR_PASS;
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
    if(dwReturnValue == dwReturnValue)
    {
        return TPR_PASS;
    }
    else
      return TPR_FAIL;
}

//*****************************************************************************
DWORD CheckThreadCompletion(HANDLE threadHandle[MAX_THREAD_COUNT], INT nThreadCount, INT nTimeOutVal)
//
//  Parameters: threadHandle[] - Array of thread handles
//              nThreadCount - thread count
//              nTimeOutVal - Maximum wait time
//
//  Return Value: TPR_PASS, TPR_FAIL
//
//  Function to check if all the threads whose handles are being passed have 
//  completed their execution or not.
//
//*****************************************************************************
{
    DWORD dwReturnValue = TPR_PASS;
    INT nCompletedThreadsCount = 0;

    for(INT nThreadHandleCount = 0; nThreadHandleCount < nThreadCount; nThreadHandleCount++)
    {
        if(NULL != threadHandle[nThreadHandleCount])
        {
            //Wait until the thread completes it's execution
            DWORD dwWaitResult = WaitForSingleObject(threadHandle[nThreadHandleCount], nTimeOutVal);

            switch(dwWaitResult)
            {
                case WAIT_TIMEOUT :            
                    dwReturnValue = TPR_FAIL;
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("Timeout (%d seconds) reached, and not all threads have terminated, hence failing the test"), nTimeOutVal/1000);
                    break;
            
                case WAIT_FAILED :
                    dwReturnValue = TPR_FAIL;
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("WaitForSingleObject function failed, hence failing the test"));
                    break;

                case WAIT_ABANDONED :
                    dwReturnValue = TPR_FAIL;
                    g_pKato->Log(LOG_DETAIL,
                                 TEXT("A Mutex object was not released before the owning thread terminated, hence failing the test"));
                    break;
            
                case WAIT_OBJECT_0 :
                    nCompletedThreadsCount++;
                    break;

                default :
                    dwReturnValue = TPR_FAIL;
            }

            if(TPR_FAIL == dwReturnValue)
            {
                break;
            }
        }
    }

    if(nThreadCount != nCompletedThreadsCount)
    {
        dwReturnValue = TPR_FAIL;
    }

    return dwReturnValue;
}