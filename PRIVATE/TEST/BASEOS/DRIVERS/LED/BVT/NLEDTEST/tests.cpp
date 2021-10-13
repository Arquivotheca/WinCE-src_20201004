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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------

#include "tuxmain.h"
#include "debug.h"

#define __FILE_NAME__ TEXT("tests.cpp")

#define LED_OFF   0
#define LED_ON    1
#define LED_BLINK 2

#define DEF_CYCLE_TIME  300000
#define DEF_ON_TIME     150000
#define DEF_OFF_TIME    150000
#define META_CYCLE_ON   10
#define META_CYCLE_OFF  10

#define NO_SETTINGS_ADJUSTABLE      0
#define All_BLINK_TIMES_ADJUSTABLE  1
#define TOTAL_CYCLE_TIME_ADJUSTABLE 2
#define ON_TIME_ADJUSTABLE          3
#define OFF_TIME_ADJUSTABLE         4
#define META_CYCLE_ON_ADJUSTABLE    5
#define META_CYCLE_OFF_ADJUSTABLE   6

extern CKato *g_pKato;

enum ADJUSTMENT_TYPE { PROMPT_ONLY, SHORTEN, LENGTHEN };
enum SETTING_INDICATOR { METACYCLEON, METACYCLEOFF };

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
                          TEXT( "Last error after calling NLedGetDeviceInfo( NLED_COUNT_INFO_ID, %u ) is %d" ),
                          ((NLED_COUNT_INFO*)pOutput)->cLeds,
                          dwLastErr );
            break;
        case NLED_SUPPORTS_INFO_ID:
            g_pKato->Log( LOG_DETAIL,
                          TEXT( "Last error after calling NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, %u ) is %d" ),
                          ((NLED_SUPPORTS_INFO*)pOutput)->LedNum,
                          dwLastErr );
            break;
        case NLED_SETTINGS_INFO_ID:
            g_pKato->Log( LOG_DETAIL,
                          TEXT( "Last error after calling NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID, %u ) is %d" ),
                          ((NLED_SETTINGS_INFO*)pOutput)->LedNum,
                          dwLastErr );
            break;
        default:
        g_pKato->Log( LOG_DETAIL,
                      TEXT( "Last error after calling NLedGetDeviceInfo( %u ) is %d. The parameter %u is unrecognized." ),
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
                  TEXT( "LED Number %d:" ),
                  ledSettingsInfo.LedNum );
    //
    // log ON/OFF/BLINK status
    //
    switch( ledSettingsInfo.OffOnBlink )
    {
    case LED_OFF:
        g_pKato->Log( LOG_DETAIL,
                      TEXT( "   LED:                         OFF" ) );
        break;
    case LED_ON:
        g_pKato->Log( LOG_DETAIL,
                      TEXT( "   LED:                         ON" ) );
        break;
    case LED_BLINK:
        g_pKato->Log( LOG_DETAIL,
                      TEXT( "   LED:                         BLINKING" ) );
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
        TEXT( "   Total Blink Cycle Time:      %d microseconds" ),
        ledSettingsInfo.TotalCycleTime );

    // log Cycle ON Time

    g_pKato->Log( LOG_DETAIL,
        TEXT( "   Cycle ON time:               %d microseconds" ),
        ledSettingsInfo.OnTime );

    // log Cycle OFF Time

    g_pKato->Log( LOG_DETAIL,
        TEXT( "   Cycle OFF time:              %d microseconds" ),
        ledSettingsInfo.OffTime );               

    // log Number of ON Blink Cycles

    g_pKato->Log( LOG_DETAIL,
        TEXT( "   Number of ON Blink Cycles:   %d" ),
        ledSettingsInfo.MetaCycleOn );         

    // log Number of OFF Blink Cycles

    g_pKato->Log( LOG_DETAIL,
        TEXT( "   Number of OFF Blink Cycles:  %d" ),
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
                  TEXT( "LED Number %u:" ),
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
            ? TEXT( "(vibrator):" ) : TEXT( "microseconds" )
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
    BOOL               bRet            = TRUE;
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

    if( LedNum < g_uMinNumOfNLEDs ) LedNum = g_uMinNumOfNLEDs;
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
                                  TEXT( "Failed set LED %u status." ),
                                  pLedSettingsInfo[ LedNum ].LedNum );
                    bResult = FALSE;
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
                TEXT( "Failed set LED %u Info Settings. Last Error = %d." ),
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
    TCHAR szMessage[ 330 ] = TEXT( "" );

    if( !g_bInteractive ) return TRUE;

    _stprintf( szMessage,
        TEXT( "If LED %u can be controlled by another driver, it may not behave according to NLED driver specifications. Do you wish to proceed with the NLED test?" ),
        LedNum );

    dwMboxValue = MessageBox(
        NULL,
        szMessage,
        TEXT( "Input Required" ), 
        MB_YESNO | MB_ICONQUESTION| MB_TOPMOST | MB_SETFOREGROUND );
    if( IDYES == dwMboxValue ) return FALSE;
    else                       return TRUE;
} // skipNLedDriverTest()

//*****************************************************************************
DWORD ValidateMetaCycleSettings( NLED_SETTINGS_INFO ledSettingsInfo,
                                 SETTING_INDICATOR  uSettingIndicator,
                                 UINT               uAdjusmentType,
                                 BOOL               bInteractive )
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
    TCHAR              szPrompt[ 160 ]   = TEXT( "" );
    LPTSTR             szRESULT[]        =
    {
        TEXT( "Unable to increase number of on blink cycles for LED" ),
        TEXT( "Unable to decrease number of on blink cycles for LED" ),
        TEXT( "Unable to increase number of off blink cycles for LED" ),
        TEXT( "Unable to decrease number of off blink cycles for LED" )
    };
    LPTSTR             szPrompt1[]       =
    {
        TEXT( "Please note the blinking rate of LED " ),
        TEXT( "Is LED " ),
        TEXT( "Is LED " )
    };
    LPTSTR             szPrompt2[]       =
    {
        TEXT( "." ),
        TEXT( " ON for a shorter time per cycle?" ),
        TEXT( " ON for a longer time per cycle?" ),
        TEXT( "." ),
        TEXT( " OFF for a shorter time per cycle?" ),
        TEXT( " OFF for a longer time per cycle?" )
    };
    UINT               uType[]           =
    {
        MB_OK    | MB_ICONSTOP     | MB_TOPMOST | MB_SETFOREGROUND,
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND,
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND
    };

    // Make setting adjustment.
    ledSettingsInfo.MetaCycleOn = META_CYCLE_ON;
    if( METACYCLEON == uSettingIndicator )
    {
        if( SHORTEN  == uAdjusmentType ) ledSettingsInfo.MetaCycleOn /= 2;
        if( LENGTHEN == uAdjusmentType ) ledSettingsInfo.MetaCycleOn *= 2;
    } // if METACYCLEON

    ledSettingsInfo.MetaCycleOff = META_CYCLE_OFF;
    if( METACYCLEOFF == uSettingIndicator )
    {
        if( SHORTEN  == uAdjusmentType ) ledSettingsInfo.MetaCycleOff /= 2;
        if( LENGTHEN == uAdjusmentType ) ledSettingsInfo.MetaCycleOff *= 2;
    } // if METACYCLEOFF

    ledSettingsInfo.OffOnBlink = LED_BLINK;
    SetLastError( ERROR_SUCCESS );
    if( !NLedSetDevice( NLED_SETTINGS_INFO_ID, &ledSettingsInfo ) )
    {
        FAIL("NLedSetDevice()");
        g_pKato->Log( LOG_DETAIL,
                      TEXT( "\tLast Error = %ld, LED = %u" ),
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
                      TEXT( "\tLast Error = %ld, LED = %u" ),
                      GetLastError(),
                      ledSettingsInfo.LedNum );
        return TPR_FAIL;
    } // else !NLedSetDevice()

    // If adjustment failed fail and terminate test.
    if( !ValidateLedSettingsInfo( ledSettingsInfo_G, ledSettingsInfo ) )
        return TPR_FAIL;

    // Automated test finished.  Return if not in interactive mode.
    if( !bInteractive ) return dwRet;

    _stprintf( szPrompt,
               TEXT( "%s%u%s" ),
               szPrompt1[ uAdjusmentType ],
               ledSettingsInfo.LedNum,
               szPrompt2[ 3 * uSettingIndicator + uAdjusmentType ] );

    dwMboxValue = MessageBox( NULL,
                              szPrompt,
                              TEXT( "Input Required" ),
                              uType[ uAdjusmentType ] );

    if( ( PROMPT_ONLY != uAdjusmentType ) && ( IDYES  != dwMboxValue ) )
    {
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "User says LED %u Meta Cycle adjustment failed." ),
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
                  TEXT( "\tLED %u: %s" ),
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
    BOOL   bSuportsCombinedTime   = FALSE;
    BOOL   bSuportsUpdatedSetting = FALSE;
    LONG   lCurrentCombinedTime   = 0;
    LONG   lCurrentSetting        = 0;
    LONG   lExpectedSetting       = 0;
    LONG   lOriginalSetting       = 0;

    LPTSTR szSetting[]          =
        { TEXT( "Total cycle" ),
          TEXT( "ON time" ),
          TEXT( "OFF time" ),
          TEXT( "number of ON blink cycles" ),
          TEXT( "number of OFF blink cycles" ) };

    switch( uSetting )
    {
    case ADJUST_TOTAL_CYCLE_TIME:       
        bSuportsCombinedTime   =    Supports_Info.fAdjustOnTime
                                 || Supports_Info.fAdjustOffTime;
        bSuportsUpdatedSetting =    Supports_Info.fAdjustTotalCycleTime;
        lCurrentCombinedTime   =    CurrentSettings.OnTime
                                  + CurrentSettings.OffTime;
        lCurrentSetting        =    CurrentSettings.TotalCycleTime;
        lExpectedSetting       =    ExpectedSettings.TotalCycleTime;
        lOriginalSetting       =    OriginalSettings.TotalCycleTime;
        break;
    case ADJUST_ON_TIME:       
        bSuportsCombinedTime   =    Supports_Info.fAdjustTotalCycleTime
                                 || Supports_Info.fAdjustOffTime;
        bSuportsUpdatedSetting =    Supports_Info.fAdjustOnTime;
        lCurrentCombinedTime   =    CurrentSettings.TotalCycleTime
                                  - CurrentSettings.OffTime;
        lCurrentSetting        =    CurrentSettings.OnTime;
        lExpectedSetting       =    ExpectedSettings.OnTime;
        lOriginalSetting       =    OriginalSettings.OnTime;
        break;
    case ADJUST_OFF_TIME:       
        bSuportsCombinedTime   =    Supports_Info.fAdjustTotalCycleTime
                                 || Supports_Info.fAdjustOnTime;
        bSuportsUpdatedSetting =    Supports_Info.fAdjustOffTime;
        lCurrentCombinedTime   =    CurrentSettings.TotalCycleTime
                                  - CurrentSettings.OnTime;
        lCurrentSetting        =    CurrentSettings.OffTime;
        lExpectedSetting       =    ExpectedSettings.OffTime;
        lOriginalSetting       =    OriginalSettings.OffTime;
        break;
    case ADJUST_META_CYCLE_ON:       
        bSuportsCombinedTime   =    FALSE; // doesn't apply to this setting
        bSuportsUpdatedSetting =    Supports_Info.fMetaCycleOn;
        lCurrentCombinedTime   =    0; // doesn't apply to this setting
        lCurrentSetting        =    CurrentSettings.MetaCycleOn;
        lExpectedSetting       =    ExpectedSettings.MetaCycleOn;
        lOriginalSetting       =    OriginalSettings.MetaCycleOn;
        break;
    case ADJUST_META_CYCLE_OFF:       
        bSuportsCombinedTime   =    FALSE; // doesn't apply to this setting
        bSuportsUpdatedSetting =    Supports_Info.fMetaCycleOff;
        lCurrentCombinedTime   =    0; // doesn't apply to this setting
        lCurrentSetting        =    CurrentSettings.MetaCycleOff;
        lExpectedSetting       =    ExpectedSettings.MetaCycleOff;
        lOriginalSetting       =    OriginalSettings.MetaCycleOff;
        break;
    default:
        FAIL( "switch" );
        g_pKato->Log( LOG_DETAIL, TEXT( "\tuSetting out of range." ) );
        return FALSE;
        break;
    } // switch

    // If the adjustment is supported but it wasn't correctly made, fail the
    //test.
    if( bSuportsUpdatedSetting )
    {
        if( lCurrentSetting != lExpectedSetting )
        {
            FAIL( "NLedSetDevice( NLED_SETTINGS_INFO_ID )" );
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "\tLED %u: Attempt to adjust %s failed." ),
                OriginalSettings.LedNum,
                szSetting[ uSetting ] );
            return FALSE;
        } // if lCurrentSetting
    } // if bSuportsUpdatedSetting
    else // !bSuportsUpdatedSetting
    {
        // The Current Setting is not adjustable by the application.  If it is
        // a time NLedSetDevice() may have still change it.  If either of the
        // other time settings are adjustable.
        if( lCurrentSetting != lOriginalSetting )
        {
            // uSetting has changed.  If it is a time and neither of the other
            // times is adjusable or if the times are inconsistant, fail the
            // test.
            if( !bSuportsCombinedTime
                || ( lCurrentSetting != lCurrentCombinedTime ) )
            {
                FAIL( "NLedSetDevice( NLED_SETTINGS_INFO_ID )" );
                g_pKato->Log(
                    LOG_DETAIL,
                    TEXT( "\tLED %u: %s incorrectly changed." ),
                    OriginalSettings.LedNum,
                    szSetting[ uSetting ] );
                return FALSE;
            } // if !bSuportsCombinedTime
        } // if lCurrentSetting
    } // else !bSuportsUpdatedSetting
    return TRUE;
} // ValidateSettingAdjustment()

//*****************************************************************************

BOOL TestLEDBlinkTimeSettings( DWORD dwSetting    = ADJUST_TOTAL_CYCLE_TIME,
                               UINT  LedNum       = 0,
                               BOOL  bInteractive = g_bInteractive )
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

#define INITIAL_STATE      LED_BLINK
#define INITIAL_CYCLE_TIME   1000000
#define INITIAL_ON_TIME       500000
#define INITIAL_OFF_TIME      500000
#define INITIAL_META_CYCLE_ON      0
#define INITIAL_META_CYCLE_OFF     0

    BOOL               bRet                     = TRUE;
    DWORD              dwLastErr                = 0;
    DWORD              dwMboxValue              = 0;
    NLED_SETTINGS_INFO ledSettingsInfo_Final    = { 0 };
    NLED_SETTINGS_INFO ledSettingsInfo_Set      = { LedNum,
                                                    INITIAL_STATE,
                                                    INITIAL_CYCLE_TIME,
                                                    INITIAL_ON_TIME,
                                                    INITIAL_OFF_TIME,
                                                    INITIAL_META_CYCLE_ON,
                                                    INITIAL_META_CYCLE_OFF };
    NLED_SUPPORTS_INFO ledSupportsInfo          = { 0 };
    TCHAR              szMessage[ 100 ]         = TEXT( "" );

    // Get the supports info for the LED.
    ledSupportsInfo.LedNum = LedNum;
    if( !Check_NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, &ledSupportsInfo ) )
        return FALSE;

    g_pKato->Log( LOG_DETAIL, TEXT( "NLED Supports Information" ) );
    displayNLED_SUPPORTS_INFO( ledSupportsInfo );

    // If the LED doesn't support blinking than it automatically passes.
    if( 0 >= ledSupportsInfo.lCycleAdjust )
    {
        g_pKato->Log( LOG_DETAIL,
                      TEXT( "LED number %u doesn\'t support blinking, so it automatically passes." ),
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
            TEXT( "LED number %u doesn\'t support blink time adjustments, so it automatically passes." ),
            ledSupportsInfo.LedNum );
        return TRUE;
    } // if fAdjustTotalCycleTime

    // Display the current settings information for the LED.
    g_pKato->Log( LOG_DETAIL, TEXT( "Starting NLED settings" ) );
    displayNLED_SETTINGS_INFO( ledSettingsInfo_Set );

    // Initiate blinking.
    SetLastError( ERROR_SUCCESS );
    if( !NLedSetDevice( NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Set ) )
    {
        FAIL("NLedSetDevice( NLED_SETTINGS_INFO_ID, ... )");
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "Unable to enable LED number %u blinking. Last Error = %d" ),
            ledSettingsInfo_Set.LedNum,
            GetLastError() );
        return FALSE;
    } // !NLedSetDevice

    if( bInteractive )
    {
        // Ask the user to observe the current blinking rates, so that they can
        // be compared to the rates after they are adjusted.

        _stprintf( szMessage,
                   TEXT( "Note the blinking rate of LED %u" ),
                   LedNum );

        MessageBox( NULL,
                    szMessage,
                    TEXT( "Input Required" ), 
                    MB_OK | MB_ICONSTOP | MB_TOPMOST | MB_SETFOREGROUND );

    } // if bInteractive


    // If the LED supports all 3 blink time adjustments, make sure that they
    // are compatible.
    if(    ledSupportsInfo.fAdjustTotalCycleTime
        && ledSupportsInfo.fAdjustOnTime
        && ledSupportsInfo.fAdjustOffTime )
    {
        ledSettingsInfo_Set.OnTime  += 500000;
        ledSettingsInfo_Set.OffTime += 500000;
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
                ledSettingsInfo_Set.TotalCycleTime += 1000000;
                break;
            case ADJUST_ON_TIME:
                ledSettingsInfo_Set.OnTime         += 1000000;
                break;
            case ADJUST_OFF_TIME:
                ledSettingsInfo_Set.OffTime        += 1000000;
                break;
            default:
                FAIL("switch failed. dwSetting out of range(0,1,2).");
                g_pKato->Log( LOG_DETAIL, TEXT( "\tdwSetting = %u"), dwSetting );
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

    g_pKato->Log( LOG_DETAIL, TEXT( "NLED settings changed to:" ) );
    displayNLED_SETTINGS_INFO( ledSettingsInfo_Final );

    // Verify that blink times were properly adjusted.
    if( ledSettingsInfo_Final.TotalCycleTime
        != ledSettingsInfo_Final.OnTime + ledSettingsInfo_Final.OffTime )
    {
        FAIL( "NLedSetDevice( NLED_SETTINGS_INFO_ID, ... )");
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "Total Cycle Time != OnTime + OffTime, after NLedSetDevice()." ) );
            bRet = FALSE;
    } // if !=

    // Verify that if total cycle time time is adjustable, it now has the
    // specified value.
    if( ledSupportsInfo.fAdjustTotalCycleTime  &&
        (  ledSettingsInfo_Final.TotalCycleTime
        != ledSettingsInfo_Set.TotalCycleTime ) )
    {
        FAIL( "NLedSetDevice( NLED_SETTINGS_INFO_ID, ... )");
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "Total Cycle Time incorrect after NLedSetDevice()." ) );
        bRet = FALSE;
    } // if fAdjustTotalCycleTime

    // Verify that if on time time is adjustable, it now has the specified
    // value.
    if( ledSupportsInfo.fAdjustOnTime  &&
        ( ledSettingsInfo_Final.OnTime != ledSettingsInfo_Set.OnTime ) )
    {
        FAIL( "NLedSetDevice( NLED_SETTINGS_INFO_ID, ... )");
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "Blink on Time incorrect after NLedSetDevice()." ) );
        bRet = FALSE;
    } // if fAdjustOnTime

    // Verify that if off time time is adjustable, it now has the specified
    // value.
    if( ledSupportsInfo.fAdjustOffTime  &&
        ( ledSettingsInfo_Final.OffTime != ledSettingsInfo_Set.OffTime ) )
    {
        FAIL( "NLedSetDevice( NLED_SETTINGS_INFO_ID, ... )");
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "Blink off Time incorrect after NLedSetDevice()." ) );
        bRet = FALSE;
    } // if fAdjustOffTime

    // If in interactive mode, ask the user if to confirm that the times have
    // been adjusted properly.
    if( bInteractive )
    {
        // If total blinking cycle time increased, ask user to confirm it.
        if(   ledSettingsInfo_Final.TotalCycleTime > INITIAL_CYCLE_TIME )
        {
            _stprintf(
                szMessage,
                TEXT( "Did the total blinking cycle time of LED %u increase?" ),
                LedNum );
            dwMboxValue = MessageBox(
                NULL,
                szMessage,
                TEXT( "Input Required" ), 
                MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );
            if( IDYES == dwMboxValue )
                g_pKato->Log(
                    LOG_DETAIL,
                    TEXT( "\tNLED %u passed total blink cycle time adjustment test."),
                    LedNum );
            else // !IDYES
            {
                g_pKato->Log(
                    LOG_DETAIL,
                    TEXT( "\tExpected increase in total cycle time for NLED %u blink not confirmed by user." ),
                    LedNum );
                bRet = FALSE;
            } // else !IDYES
        } // if TotalCycleTime

        // If total on time increased, ask user to confirm it.
        if( ledSettingsInfo_Final.OnTime > INITIAL_ON_TIME )
        {
            _stprintf(
                szMessage,
                TEXT( "Did the blinking cycle on time of LED %u increase?" ),
                LedNum );
            dwMboxValue = MessageBox(
                NULL,
                szMessage,
                TEXT( "Input Required" ), 
                MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );
            if( IDYES == dwMboxValue )
                g_pKato->Log(
                    LOG_DETAIL,
                    TEXT( "\tNLED %u passed blink cycle on time adjustment test."),
                    LedNum );
            else // !IDYES
            {
                g_pKato->Log(
                    LOG_DETAIL,
                    TEXT( "\tExpected increase in on time for NLED %u blink not confirmed by user." ),
                    LedNum );
                bRet = FALSE;
            } // else !IDYES
        } // if OnTime

        // If total off time increased, ask user to confirm it.
        if( ledSettingsInfo_Final.OffTime > INITIAL_OFF_TIME )
        {
            _stprintf(
                szMessage,
                TEXT( "Did the blinking cycle off time of LED %u increase?" ),
                LedNum );
            dwMboxValue = MessageBox(
                NULL,
                szMessage,
                TEXT( "Input Required" ), 
                MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );
            if( IDYES == dwMboxValue )
                 g_pKato->Log(
                    LOG_DETAIL,
                    TEXT( "\tNLED %u passed blink cycle off time adjustment test."),
                    LedNum );
            else // !IDYES
            {
                g_pKato->Log(
                    LOG_DETAIL,
                    TEXT( "\tExpected increase in off time for NLED %u blink not confirmed by user." ),
                    LedNum );
                bRet = FALSE;
            } // else !IDYES
        } // if OffTime
    } // if bInteractive

    // Turn the LED off.
    if( !SetLEDOffOnBlink( ledSettingsInfo_Set.LedNum,
                           LED_OFF,
                           bInteractive ) )
    {
        g_pKato->Log( LOG_FAIL,
                      TEXT( "Failed to turn LED %d Off." ),
                      ledSettingsInfo_Set.LedNum );
        bRet = FALSE;
    } // if !SetLEDOffOnBlink()

    return bRet;
} // TestLEDBlinkTimeSettings()

//*****************************************************************************

BOOL ValidateLedSettingsInfo( NLED_SETTINGS_INFO ledSettingsInfo,
                              NLED_SETTINGS_INFO ledSettingsInfo_Valid )
//
//  Parameters:   ledSettingsInfo       -- NLED_SETTINGS_INFO struct containing
//                                         settings information to be
//                                         validated.
//                ledSettingsInfo_Valid -- NLED_SETTINGS_INFO struct containing
//                                         valid settings information.
//
//  Return Value: TRUE  -> Every element in ledSettingsInfo matches the
//                         corresponding element in ledSettingsInfo_Valid
//                False -> At least one element in ledSettingsInfo doesn't
//                         matches the corresponding element in
//                         ledSettingsInfo_Valid
//
//  Compares each element in ledSettingsInfo with the corresponding element in
//  ledSettingsInfo_Valid.
//
//*****************************************************************************
{
    BOOL bReturn = TRUE;

    if( ledSettingsInfo.LedNum != ledSettingsInfo_Valid.LedNum )
    {
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "LED number incorrect. LedNum = %u, should be %u" ),
            ledSettingsInfo.LedNum,
            ledSettingsInfo_Valid.LedNum );
        bReturn = FALSE;
    } // if LedNum

    if( ledSettingsInfo.OffOnBlink  != ledSettingsInfo_Valid.OffOnBlink  )
    {
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "LED %d blink setting incorrect. OffOnBlink  = %d, should be %d" ),
            ledSettingsInfo.OffOnBlink,
            ledSettingsInfo_Valid.OffOnBlink );
        bReturn = FALSE;
    } // if OffOnBlink

    if( ledSettingsInfo.TotalCycleTime != ledSettingsInfo_Valid.TotalCycleTime )
    {
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "Total cycle time of a blink incorrect. TotalCycleTime = %u, should be %u microseconds" ),
            ledSettingsInfo.TotalCycleTime,
            ledSettingsInfo_Valid.TotalCycleTime );
        bReturn = FALSE;
    } // if TotalCycleTime

    if( ledSettingsInfo.OnTime != ledSettingsInfo_Valid.OnTime )
    {
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "Cycle on time incorrect. OnTime = %u, should be %u microseconds" ),
            ledSettingsInfo.OnTime,
            ledSettingsInfo_Valid.OnTime );
        bReturn = FALSE;
    } // if OnTime

    if( ledSettingsInfo.OffTime != ledSettingsInfo_Valid.OffTime )
    {
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "Cycle off time incorrect. OffTime = %u, should be %u microseconds" ),
            ledSettingsInfo.OffTime,
            ledSettingsInfo_Valid.OffTime );
        bReturn = FALSE;
    } // if OffTime

    if( ledSettingsInfo.MetaCycleOn != ledSettingsInfo_Valid.MetaCycleOn )
    {
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "Number of on blink cycles incorrect. MetaCycleOn = %d, should be %d microseconds" ),
            ledSettingsInfo.MetaCycleOn,
            ledSettingsInfo_Valid.MetaCycleOn );
        bReturn = FALSE;
    } // if MetaCycleOn

    if( ledSettingsInfo.MetaCycleOff != ledSettingsInfo_Valid.MetaCycleOff )
    {
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "Number of off blink cycles incorrect. MetaCycleOff = %d, should be %d microseconds" ),
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
    UINT  uInvalidNLED_INFO_ID = 0;
    
    do
    {
        switch( uInvalidNLED_INFO_ID )
        {
        case NLED_COUNT_INFO_ID:
        case NLED_SUPPORTS_INFO_ID:
        case NLED_SETTINGS_INFO_ID:
            uInvalidNLED_INFO_ID++;
            break;
        default:
            return uInvalidNLED_INFO_ID;
            break;
        }
    } while( TRUE );
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
    TCHAR szMessage[ 80 ] = TEXT( "" );

    // Verify OffOnBlink
    _stprintf(
        szMessage,
        TEXT( "NLED %u OffOnBlink = %d.\n(0->OFF, 1->ON, 2->BLINK).\nIs that correct?" ),
        ledSettingsInfo.LedNum,
        ledSettingsInfo.OffOnBlink );

    dwMboxValue = MessageBox(
        NULL,
        szMessage,
        TEXT( "Input Required" ), 
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );

    if( IDYES != dwMboxValue )
    {
        FAIL( "ledSettingsInfo.OffOnBlink" );
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "User says OffOnBlink setting is incorrect for LED %ld." ),
            ledSettingsInfo.LedNum );
        dwRet = TPR_FAIL;
    }

    // Verify TotalCycleTime
    _stprintf(
        szMessage,
        TEXT( "NLED %u Total cycle time = %ld microseconds.\nIs that correct?" ),
        ledSettingsInfo.LedNum,
        ledSettingsInfo.TotalCycleTime  );

    dwMboxValue = MessageBox(
        NULL,
        szMessage,
        TEXT( "Input Required" ), 
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );

    if( IDYES != dwMboxValue )
    {
        FAIL( "ledSettingsInfo.TotalCycleTime" );
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "User says TotalCycleTime setting is incorrect for LED %ld." ),
            ledSettingsInfo.LedNum );
        dwRet = TPR_FAIL;
    }

    // Verify OnTime
    _stprintf( szMessage,
        TEXT( "NLED %u On time = %ld microseconds.\nIs that correct?" ),
        ledSettingsInfo.LedNum,
        ledSettingsInfo.OnTime );

    dwMboxValue = MessageBox(
        NULL,
        szMessage,
        TEXT( "Input Required" ), 
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );

    if( IDYES != dwMboxValue )
    {
        FAIL( "ledSettingsInfo.OnTime " );
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "User says OnTime setting is incorrect for LED %ld." ),
            ledSettingsInfo.LedNum );
        dwRet = TPR_FAIL;
    }

    // Verify OffTime
    _stprintf( szMessage,
        TEXT( "NLED %u Off time = %ld microseconds.\nIs that correct?" ),
        ledSettingsInfo.LedNum,
        ledSettingsInfo.OffTime );

    dwMboxValue = MessageBox(
        NULL,
        szMessage,
        TEXT( "Input Required" ), 
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );

    if( IDYES != dwMboxValue )
    {
        FAIL( "ledSettingsInfo.OffTime" );
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "User says OffTime setting is incorrect for LED %ld." ),
            ledSettingsInfo.LedNum );
        dwRet = TPR_FAIL;
    }

    // Verify MetaCycleOn
    _stprintf( szMessage,
        TEXT( "NLED %u number of on blink cycles = %d.\nIs that correct?" ),
        ledSettingsInfo.LedNum,
        ledSettingsInfo.MetaCycleOn );

    dwMboxValue = MessageBox(
        NULL,
        szMessage,
        TEXT( "Input Required" ), 
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );

    if( IDYES != dwMboxValue )
    {
        FAIL( "ledSettingsInfo.MetaCycleOn" );
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "User says number of on blink cycles setting is incorrect for LED %ld." ),
            ledSettingsInfo.LedNum );
        dwRet = TPR_FAIL;
    }

    // Verify MetaCycleOff
    _stprintf( szMessage,
        TEXT( "NLED %u number of off blink cycles = %d.\nIs that correct?" ),
        ledSettingsInfo.LedNum,
        ledSettingsInfo.MetaCycleOff );

    dwMboxValue = MessageBox(
        NULL,
        szMessage,
        TEXT( "Input Required" ), 
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );

    if( IDYES != dwMboxValue )
    {
        FAIL( "ledSettingsInfo.MetaCycleOff" );
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "User says number of off blink cycles setting is incorrect for LED %ld." ),
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
    TCHAR szMessage[ 100 ] = TEXT( "" );

    _stprintf( szMessage,
               TEXT( "Does NLED %u support blinking?" ),
               ledSupportsInfo.LedNum );

    dwMboxValue = MessageBox(
        NULL,
        szMessage,
        TEXT( "Input Required" ), 
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );

    if( IDYES == dwMboxValue )
    {
        // Verify cycle-time adjustments granularity.
        if( 1 > ledSupportsInfo.lCycleAdjust )
        {
            FAIL( "ledSupportsInfo.lCycleAdjust" );
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "User says LED %d supports blinking, so the granularity should be greater 0." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

        // Verify total cycle-time adjustment support.

        _stprintf(
            szMessage,
            TEXT( "Does NLED %u support adjustable total cycle time?" ),
            ledSupportsInfo.LedNum );

        if( TPR_PASS
            != VerifyNLEDSupportsOption( ledSupportsInfo.fAdjustTotalCycleTime,
                                         szMessage ) )
        {
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "LED %d total cycle-time adjustment support is inconsistent with user response." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

        // Verify separate on time adjustment support.

        _stprintf(
            szMessage,
            TEXT( "Does NLED %u support separate adjustable on time?" ),
            ledSupportsInfo.LedNum );

        if( TPR_PASS
            != VerifyNLEDSupportsOption( ledSupportsInfo.fAdjustOnTime,
                                         szMessage ) )
        {
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "LED %d separate adjustable on time support is inconsistent with user response." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

        // Verify separate off time adjustment support.

        _stprintf(
            szMessage,
            TEXT( "Does NLED %u support separate adjustable off time?" ),
            ledSupportsInfo.LedNum );

        if( TPR_PASS
            != VerifyNLEDSupportsOption( ledSupportsInfo.fAdjustOffTime,
                                         szMessage ) )
        {
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "LED %d separate adjustable off time support is inconsistent with user response." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

        // Verify support for "blink n cycles, pause, and blink n cycles."

        _stprintf(
            szMessage,
            TEXT( "Does NLED %u support \"blink n cycles, pause, and blink n cycles?\"" ),
            ledSupportsInfo.LedNum );

        if( TPR_PASS != VerifyNLEDSupportsOption( ledSupportsInfo.fMetaCycleOn,
                                                  szMessage ) )
        {
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "LED %d \"blink n cycles, pause, and blink n cycles\" support is inconsistent with user response." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

        // Verify support for "blink n cycles, pause n cycles, and blink n cycles."

        _stprintf(
            szMessage,
            TEXT( "Does NLED %u support \"blink n cycles, pause n cycles, and blink n cycles?\"" ),
            ledSupportsInfo.LedNum );

        if( TPR_PASS !=
            VerifyNLEDSupportsOption( ledSupportsInfo.fMetaCycleOff,
                                      szMessage ) )
        {
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "LED %d \"blink n cycles, pause n cycles, and blink n cycles\" support is inconsistent with user response." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

    } // if IDYES
    else // !IDYES
    { // user says LED doesn't support blinking.
        if( 0 >= ledSupportsInfo.lCycleAdjust )
        {
            FAIL( "ledSupportsInfo.lCycleAdjust" );
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "User says blinking isn't supported for LED %d, so ledSupportsInfo.lCycleAdjust should be 0." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

        if( ledSupportsInfo.fAdjustTotalCycleTime )
        {
            FAIL( "ledSupportsInfo.fAdjustTotalCycleTime" );
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "User says blinking isn't supported for LED %d, so ledSupportsInfo.fAdjustTotalCycleTime should be FALSE." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

        if( ledSupportsInfo.fAdjustOnTime )
        {
            FAIL( "ledSupportsInfo.fAdjustOnTime" );
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "User says blinking isn't supported for LED %d, so ledSupportsInfo.fAdjustOnTime should be FALSE." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

        if( ledSupportsInfo.fAdjustOffTime )
        {
            FAIL( "ledSupportsInfo.fAdjustOffTime" );
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "User says blinking isn't supported for LED %d, so ledSupportsInfo.fAdjustOffTime should be FALSE." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

        if( ledSupportsInfo.fMetaCycleOn )
        {
            FAIL( "ledSupportsInfo.fMetaCycleOn" );
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "User says blinking isn't supported for LED %d, so ledSupportsInfo.fMetaCycleOn should be FALSE." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

        if( ledSupportsInfo.fMetaCycleOff )
        {
            FAIL( "ledSupportsInfo.fMetaCycleOff" );
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "User says blinking isn't supported for LED %d, so ledSupportsInfo.fMetaCycleOff should be FALSE." ),
                ledSupportsInfo.LedNum );
            dwRet = TPR_FAIL;
        }

    } // else !IDYES
    return dwRet;
} // InteractiveVerifyNLEDSupports()

//*****************************************************************************

BOOL SetLEDOffOnBlink( UINT unLedNum,
                       int  iOffOnBlink,
                       BOOL bInteractive = g_bInteractive )
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
//  with either "Yes" or "No."  Interactive mode can be overridden by inserting
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
    TCHAR              szMessage[ 50 ]   = TEXT( "" );
    
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
            TEXT( "NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID, ... ) failed" ) );
        g_pKato->Log( LOG_DETAIL,
                      TEXT( "Last Error = %ld" ),
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
                      TEXT( "LED %u is now in state %ld." ),
                      ledSettingsInfo_G.LedNum,
                      ledSettingsInfo_G.OffOnBlink );
    else // NLedGetDeviceInfo() failed
    {
        g_pKato->Log( LOG_DETAIL,
            TEXT( "NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID, ... ) failed" ) );
        g_pKato->Log( LOG_DETAIL,
                      TEXT( "Last Error = %ld" ),
                      GetLastError() );
        return FALSE;
    } // else NLedGetDeviceInfo failed

    // Verify that the LEDs state was correctly changed.
    if( ledSettingsInfo_G.OffOnBlink != iOffOnBlink )
    {
        FAIL("NLedSetDevice( NLED_SETTINGS_INFO_ID, ... )");
        g_pKato->Log( LOG_DETAIL,
                      TEXT( "\tLED %us state incorrect" ),
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
            _stprintf( szMessage, TEXT( "Is LED %u OFF?" ),      unLedNum );        
            break;
        case LED_ON:
            _stprintf( szMessage, TEXT( "Is LED %u ON?" ),       unLedNum );
            break;
        case LED_BLINK:
            _stprintf( szMessage, TEXT( "Is LED %u Blinking?" ), unLedNum );
            break;
        default:
            ASSERT( !"Never Reached" );
            break;
        }

        dwMboxVal = MessageBox(
            NULL,
            szMessage,
            TEXT( "Input Required" ), 
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
    NLED_SETTINGS_INFO  updatedLedSettingsInfo = { 0 };
    UINT               uAdjustment             = 0;

    // Get the supports information for later reference.
    ledSupportsInfo.LedNum = newLedSettingsInfo.LedNum;
    if( Check_NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, &ledSupportsInfo ) )
    {
        g_pKato->Log( LOG_DETAIL, TEXT( "NLED Supports Information" ) );
        displayNLED_SUPPORTS_INFO( ledSupportsInfo );
    } // if Check_NLedGetDeviceInfo()
    else // Check_NLedGetDeviceInfo() failed
    {
        FAIL( "Check_NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID )" );
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "\tUnable to get supports information for LED %u." ),
            ledSupportsInfo.LedNum );
        return TPR_FAIL;
    } // else Check_NLedGetDeviceInfo() failed

    // Get the original settings information.
    originalLedSettingsInfo.LedNum = newLedSettingsInfo.LedNum;
    if( Check_NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID,
                                 &originalLedSettingsInfo ) )
    {
        g_pKato->Log( LOG_DETAIL,
                      TEXT( "Original NLED Settings Information" ) );
        displayNLED_SETTINGS_INFO( originalLedSettingsInfo );
    } // if Check_NLedGetDeviceInfo()
    else // Check_NLedGetDeviceInfo() failed
    {
        FAIL( "Check_NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID )" );
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "\tUnable to get original settings for LED %u." ),
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
                TEXT( "NLED Settings Information after updated attempt" ) );
            displayNLED_SETTINGS_INFO( updatedLedSettingsInfo );
        } // if Check_NLedGetDeviceInfo()
        else // Check_NLedGetDeviceInfo() failed
        {
            FAIL( "Check_NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID )" );
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "\tUnable to retrieve updated settings for LED %u." ),
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

        // Make sure that unadjustable blink times add up properly.
        if(    updatedLedSettingsInfo.OnTime + updatedLedSettingsInfo.OffTime
            != updatedLedSettingsInfo.TotalCycleTime )
        {
            FAIL( "NLedSetDevice( NLED_SETTINGS_INFO_ID )" );
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "\tAdjusted Blink OnTime + OffTime != TotalCycleTime for LED %d." ),
                newLedSettingsInfo.LedNum );
            dwRet = TPR_FAIL;
        } // if

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
            FAIL( "NLedSetDevice( NLED_SETTINGS_INFO_ID )" );
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
        TEXT( "Input Required" ), 
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );

    if(    ( ( IDYES == dwMboxValue ) && ( TRUE  == bOption ) )
        || ( ( IDYES != dwMboxValue ) && ( FALSE == bOption ) ) )

        return TPR_PASS;
    else
    {
        FAIL( "NLED_SUPPORTS_INFO" );
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

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

	DWORD           dwRet        = TPR_FAIL;
    NLED_COUNT_INFO ledCountInfo = { 0 };

    //
    // query the number of LEDs
    //
    if( Check_NLedGetDeviceInfo( NLED_COUNT_INFO_ID, &ledCountInfo ) )
    {
        g_pKato->Log( LOG_DETAIL, TEXT( "LED Count = %d" ),
                      ledCountInfo.cLeds );
        SUCCESS( "Get LED Count Test Passed" );
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
    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD              dwLastErr       = 0;
	DWORD              dwRet           = TPR_PASS;
    NLED_SUPPORTS_INFO ledSupportsInfo = { 0 };

    // If there are no LEDs pass test.
    if( 0 == GetLedCount() )
    {
        g_pKato->Log( LOG_DETAIL,
                      TEXT( "There are no LEDs to Test. Automatic PASS" ) );
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
            g_pKato->Log( LOG_DETAIL, TEXT( "NLED supports information" ) );
            displayNLED_SUPPORTS_INFO( ledSupportsInfo );

        } // if
        else // NLedGetDeviceInfo() failed
        {
            FAIL( "NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID ) unexpected error code" );
            g_pKato->Log( LOG_DETAIL,
                          TEXT( "Last Error = %d" ),
                          GetLastError() );
            dwRet = TPR_FAIL;
        } // else NLedGetDeviceInfo() failed
    } // for
            
    if( TPR_PASS == dwRet ) SUCCESS( "Get LED Support Info Test Passed" );
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
    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD              dwLastErr       = 0;
	DWORD              dwRet           = TPR_PASS;
    NLED_SETTINGS_INFO ledSettingsInfo = { 0 };

    // If there are no LEDs pass test.
    if( 0 == GetLedCount() )
    {
        g_pKato->Log( LOG_DETAIL,
                      TEXT( "There are no LEDs to Test. Automatic PASS" ) );
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
            g_pKato->Log( LOG_DETAIL, TEXT( "NLED settings" ) );
            displayNLED_SETTINGS_INFO( ledSettingsInfo );
        } // NLedGetDeviceInfo()
        else // NLedGetDeviceInfo() failed
        {
            FAIL( "NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID ) unexpected error code" );
            g_pKato->Log( LOG_DETAIL,
                          TEXT( "Last Error = %d" ),
                          GetLastError() );
            dwRet = TPR_FAIL;
        } // else NLedGetDeviceInfo() failed
    } // for
            
    if( TPR_PASS == dwRet ) SUCCESS( "Get LED Settings Info Test Passed" );
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
    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD              dwLastErr               = 0;
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
            g_pKato->Log( LOG_DETAIL, TEXT( "Original NLED settings" ) );
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
                    FAIL( "No adjustable setting found." );
                    dwRet = TPR_FAIL;
                } // switch

                SetLastError( ERROR_SUCCESS );
                if( NLedSetDevice( NLED_SETTINGS_INFO_ID, &ledSettingsInfo ) )
                {

                    // log new LED settings

                    g_pKato->Log(
                        LOG_DETAIL,
                        TEXT( "NLED settings successfully changed to:" ) );
                    displayNLED_SETTINGS_INFO( ledSettingsInfo );

                    // Restore the original settings before proceeding.
                    if( NLedSetDevice( NLED_SETTINGS_INFO_ID,
                                       &originalLedSettingsInfo ) )
                    {
                        g_pKato->Log( LOG_DETAIL,
                                      TEXT( "NLED settings restored to:" ) );
                        displayNLED_SETTINGS_INFO( originalLedSettingsInfo );
                    } // if set original LED settings()
                    else // !NLedSetDevice()
                    {
                        FAIL( "Unable to restore LED settings" );
                        dwRet = TPR_FAIL;
                    } // else !NLedSetDevice()

                } // if set new LED settings
                else // NLedGetDeviceInfo() failed
                {
                    FAIL( "NLedSetDevice( NLED_SETTINGS_INFO_ID, ... )" );
                    g_pKato->Log( LOG_DETAIL,
                                  TEXT( "Last Error = %d" ),
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
            FAIL( "Unable to get original LED settings" );
            dwRet = TPR_FAIL;
        } // else !Check_NLedGetDeviceInfo()

        if( !NLedSetDevice( NLED_SETTINGS_INFO_ID,
                            &originalLedSettingsInfo ) )
            {
                FAIL( "Unable to restore LED settings" );
                dwRet = TPR_FAIL;
            } // else !NLedSetDevice()

    } // for
            
    if( TPR_PASS == dwRet ) SUCCESS( "Set LED Settings Info Test Passed" );
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
    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD dwRet = TPR_PASS;
    void* pVoid = NULL;

    // Verify that NLedGetDeviceInfo( InvalidNLED_INFO_ID, ... ) responds
    // gracefully when passed an invalid info ID (first parameter).
    SetLastError( ERROR_SUCCESS );
    if( NLedGetDeviceInfo( generateInvalidNLED_INFO_ID(), pVoid ) )
    {
        FAIL("NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, ... ) failed to catch an attempt to pass it an invalid NLED info ID.");
        g_pKato->Log( LOG_DETAIL, TEXT( "Last Error = %d" ), GetLastError() );
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
        g_pKato->Log( LOG_DETAIL, TEXT( "Last Error = %ld" ), GetLastError() );
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
        g_pKato->Log( LOG_DETAIL, TEXT( "Last Error = %ld" ), GetLastError() );
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
        g_pKato->Log( LOG_DETAIL, TEXT( "Last Error = %ld" ), GetLastError() );
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
        g_pKato->Log( LOG_DETAIL, TEXT( "Last Error = %d" ), GetLastError() );
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
        g_pKato->Log( LOG_DETAIL, TEXT( "Last Error = %d" ), GetLastError() );
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
    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD              dwRet           = TPR_PASS;
    NLED_SETTINGS_INFO ledSettingsInfo = { 0 };
    NLED_SUPPORTS_INFO ledSupportsInfo = { 0 };
    void*              pVoid           = NULL;

    /* Save for future use.

    // Verify that NLedSetDevice() responds gracefully when passed an
    // unsupported info ID (first parameter).
    SetLastError( ERROR_SUCCESS );
    if( NLedSetDevice( NLED_SUPPORTS_INFO_ID, &ledSupportsInfo ) )
    {
        FAIL("NLedSetDevice() failed to catch an attempt to pass it an unsupported info ID.");
        g_pKato->Log( LOG_DETAIL, TEXT( "Last Error = %d" ), GetLastError() );
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
        g_pKato->Log( LOG_DETAIL, TEXT( "Last Error = %d" ), GetLastError() );
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
        g_pKato->Log( LOG_DETAIL, TEXT( "Last Error = %d" ), GetLastError() );
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
        g_pKato->Log( LOG_DETAIL, TEXT( "Last Error = %d" ), GetLastError() );
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
        g_pKato->Log( LOG_DETAIL, TEXT( "NLED-%u, Last Error = %d" ),ledSettingsInfo.LedNum, GetLastError() );
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
    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;
                        
    BOOL                bTest                     = TRUE;
    DWORD               dwLED_Res                 = TPR_FAIL;
    DWORD               dwRet                     = TPR_PASS;
    NLED_SETTINGS_INFO  ledSettingsInfo_Set       = { 0 };
    NLED_SUPPORTS_INFO  ledSupportsInfo           = { 0 };
    NLED_SETTINGS_INFO* pOriginalLedSettingsInfo  = NULL;
    UINT                LedNum                    = 0;
    LPTSTR              szFailMessage             = NULL;
    LPTSTR              szNonSupportFailMessages[] =
    {
        TEXT( "no test" ),
        TEXT( "no test" ),
        TEXT( "no test" ),
        TEXT( "Attempt to make an unsupported \"number of on blink cycles\" adjustment." ),
        TEXT( "Attempt to make an unsupported \"number of off blink cycles\" adjustment." ),
        TEXT( "no test" ),
        TEXT( "no test" ),
        TEXT( "no test" )
    };
    LPTSTR              szSupportFailMessages[]   =
    {
        TEXT( "Attempt to adjust blink \"total cycle time\", to a negative value." ),
        TEXT( "Attempt to adjust blink \"on time\", to a negative value." ),
        TEXT( "Attempt to adjust blink \"off time\", to a negative value." ),
        TEXT( "Attempt to adjust \"number of on blink cycles\" to a negative value." ),
        TEXT( "Attempt to adjust \"number of off blink cycles\" to a negative value." ),
        TEXT( "Attempt to set \"on time\" > \"total cycle time\"" ),
        TEXT( "Attempt to set \"off time\" > \"total cycle time\"" ),
        TEXT( "Attempt to set inconsistent combination of cycle times." )
    };
    UINT                uScenario                 = 0;

    // Allocate space to save origional LED setttings.
    SetLastError( ERROR_SUCCESS );
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];
    if( !pOriginalLedSettingsInfo )
    {
        FAIL("new failed");
        g_pKato->Log( LOG_DETAIL,
                      TEXT( "\tLast Error = %ld" ),
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
            TEXT( "Attempt to make an invalid blink setting." )
            ) ) dwLED_Res = TPR_FAIL;

        // Get supports information
        ledSupportsInfo.LedNum = LedNum;
        if( Check_NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID,
                                     &ledSupportsInfo ) )
        {
            g_pKato->Log( LOG_DETAIL, TEXT( "NLED supports information:" ) );
            displayNLED_SUPPORTS_INFO( ledSupportsInfo );

            // Attempt to enable blinking.
            ledSettingsInfo_Set.OffOnBlink = LED_BLINK;
            if( NLedSetDevice( NLED_SETTINGS_INFO_ID, &ledSettingsInfo_Set ) )
            {
                if( 0 >= ledSupportsInfo.lCycleAdjust )
                {
                    // Blinking is not supported, NLedSetDevice() should have
                    // failed.
                    FAIL("NLedSetDevice()");
                    g_pKato->Log(
                        LOG_DETAIL,
                        TEXT( "\tAttempt to initiate blinking for LED %u, which doesn\'t support blinking." ),
                        ledSettingsInfo_Set.LedNum );
                    dwLED_Res = TPR_FAIL;

                    // Restore OffOnBlink and other setings for future tests.
                    ledSettingsInfo_Set = *(pOriginalLedSettingsInfo + LedNum);

                } // if 0
                else // !0
                { // Blinking is supported. Do more tests.

                    for( uScenario = 0; uScenario < 8; uScenario++ )
                    {
                        bTest = TRUE;
                        ledSettingsInfo_Set
                            = *(pOriginalLedSettingsInfo + LedNum);
                        ledSettingsInfo_Set.OffOnBlink = LED_BLINK;
                        szFailMessage = szNonSupportFailMessages[ uScenario ];

                        switch( uScenario )
                        {
                            case 0:
                                if( ledSupportsInfo.fAdjustTotalCycleTime )
                                {
                                    // Attempt to make an impossible Total 
                                    // Cycle time adjustment.
                                    ledSettingsInfo_Set.TotalCycleTime
                                        = -1000000;
                                    szFailMessage
                                        = szSupportFailMessages[ uScenario ];
                                } // fAdjustTotalCycleTime
                                else bTest = FALSE;
                                break;
                            case 1:
                                if( ledSupportsInfo.fAdjustOnTime )
                                {
                                    // Attempt to make an impossible On time
                                    // adjustment.
                                    ledSettingsInfo_Set.OnTime = -1000000;
                                    szFailMessage
                                        = szSupportFailMessages[ uScenario ];
                                } // fAdjustOnTime
                                else bTest = FALSE;
                                break;
                            case 2:
                                if( ledSupportsInfo.fAdjustOffTime )
                                {
                                    // Attempt to make an impossible Off time
                                    // adjustment.
                                    ledSettingsInfo_Set.OffTime = -1000000;
                                    szFailMessage
                                        = szSupportFailMessages[ uScenario ];
                                } // fAdjustOffTime
                                else bTest = FALSE;
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
                                    // Attempt to make an unsupported number of
                                    // on blink cycles adjustment.
                                    ledSettingsInfo_Set.MetaCycleOn += 1;
                                    szFailMessage =
                                        szNonSupportFailMessages[ uScenario ];
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
                                    // Attempt to make an unsupported number of
                                    // off blink cycles adjustment.
                                    ledSettingsInfo_Set.MetaCycleOff += 1;
                                    szFailMessage =
                                        szNonSupportFailMessages[ uScenario ];
                                } // if !fMetaCycleOff
                                break;
                            case 5:
                                if(    ledSupportsInfo.fAdjustTotalCycleTime
                                    && ledSupportsInfo.fAdjustOnTime )
                                {
                                    // Attempt to set On Time > Total Time.
                                    ledSettingsInfo_Set.OnTime
                                        = ledSettingsInfo_Set.TotalCycleTime
                                        + 1000000;
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
                                        + 1000000;
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
                                        = 1000000;
                                    ledSettingsInfo_Set.OnTime
                                        = 1000000;
                                    ledSettingsInfo_Set.OffTime
                                        = 1000000;
                                    szFailMessage =
                                        szSupportFailMessages[ uScenario ];
                                } // fAdjustTotalCycleTime
                                else bTest = FALSE;
                                break;
                            default:
                                    bTest = FALSE;
                                g_pKato->Log(
                                    LOG_DETAIL,
                                    TEXT( "\tuScenario out of range." ),
                                    ledSettingsInfo_Set.LedNum );
                                break;
                        } // switch

                        if( bTest && !Validate_NLedSetDevice_Settings(
                            ledSettingsInfo_Set,
                            FALSE,
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
                        TEXT( "\tFailed to enable blink for LED %u." ),
                        ledSettingsInfo_Set.LedNum );
                    dwLED_Res = TPR_FAIL;
                }
                else
                {
                    g_pKato->Log( LOG_DETAIL, TEXT("\t* LED %u does not support blinkning"), ledSettingsInfo_Set.LedNum );
                }
            }  //else !NLedSetDevice()
        } // if Check_NLedGetDeviceInfo(NLED_SUPPORTS_INFO_ID)

        if( TPR_PASS == dwLED_Res )
            g_pKato->Log(
            LOG_DETAIL,
            TEXT( "LED %u passed all invalid settings tests." ),
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

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    DWORD           dwMboxValue     = 0;
	DWORD           dwRet           = TPR_PASS;
    NLED_COUNT_INFO ledCountInfo    = { 0 };
    TCHAR           szMessage[ 50 ] = TEXT( "" );

    // query the driver for the number of NLEDs

    if( !Check_NLedGetDeviceInfo( NLED_COUNT_INFO_ID, &ledCountInfo ) )
        return TPR_FAIL;

    g_pKato->Log( LOG_DETAIL, TEXT( "NLED Count = %u." ), ledCountInfo.cLeds );

    if( g_uMinNumOfNLEDs > ledCountInfo.cLeds )
    {
        FAIL("NLedGetDeviceInfo() reported fewer than the minimum number of NLEDs required.");
        g_pKato->Log( LOG_DETAIL,
                      TEXT( "\tNLED Count must be at least %u." ),
                      g_uMinNumOfNLEDs );
        dwRet = TPR_FAIL;
    } // if g_uMinNumOfNLEDs
    else // !g_uMinNumOfNLEDs
    {  //  If in interactive mode, ask user to verify NLED Count.
        if( g_bInteractive )
        {
            _stprintf(
                szMessage,
                TEXT( "NLED Count = %u. Is this correct?" ),
                ledCountInfo.cLeds );

            dwMboxValue = MessageBox(
                NULL,
                szMessage,
                TEXT( "Input Required" ), 
                MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND );

            if( IDYES == dwMboxValue ) g_pKato->Log(
                LOG_DETAIL,
                TEXT( "\tUser confirmed NLED Count." ) );
            else // !IDYES
            {
                g_pKato->Log( LOG_DETAIL,
                              TEXT( "\tUser says NLED Count is incorrect." ) );
                dwRet = TPR_FAIL;
            } // else !IDYES
        } // g_bInteractive
    } // if !g_uMinNumOfNLEDs

    if( TPR_PASS == dwRet ) SUCCESS( "Verify Get NLED Count Test passed" );
    else                    FAIL( "Verify Get NLED Count Test failed" );

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

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

	DWORD              dwRet            = TPR_PASS;
    DWORD              dwLastErr        = 0;
    DWORD              dwMboxValue      = 0;
    NLED_SUPPORTS_INFO ledSupportsInfo  = { 0 };
    TCHAR              szMessage[ 100 ] = TEXT( "" );

    for( ledSupportsInfo.LedNum = 0; 
         ledSupportsInfo.LedNum < GetLedCount(); 
         ledSupportsInfo.LedNum++ )
    {
        // Get LED supports information for each of the LEDs on the system.
        if( Check_NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID, &ledSupportsInfo ) )
        {
            // log the LED support information
            g_pKato->Log( LOG_DETAIL, TEXT( "NLED supports information:" ) );
            displayNLED_SUPPORTS_INFO( ledSupportsInfo );

            // If this is the last LED and a vibrator is required the
            // granularity should be -1.
            if( ( GetLedCount() - 1 ) == ledSupportsInfo.LedNum )
            {
                if( g_bVibrator && ( -1 != ledSupportsInfo.lCycleAdjust ) )
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
                FAIL( "NLED_SUPPORTS_INFO_ID" );
                g_pKato->Log( LOG_DETAIL,
                              TEXT( "LED %ld has invalid Granularity." ),
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
                FAIL( "NLED_SUPPORTS_INFO_ID" );
                g_pKato->Log(
                    LOG_DETAIL,
                    TEXT( "LED %d doesn/'t support blinking, but one or more blinking supports flags are enabled." ),
                    ledSupportsInfo.LedNum );
                dwRet = TPR_FAIL;
            } // if 0

            // All other combinations of support information are permitted, so
            // no other automated validation can be done.

            // If interactive mode is enabled, prompt the user to confirm each
            // supports flag.
            if( g_bInteractive )
            {
                if( TPR_PASS
                    == InteractiveVerifyNLEDSupports( ledSupportsInfo ) )

                    g_pKato->Log( LOG_DETAIL,
                        TEXT( "LED %d supports information validated." ), 
                        ledSupportsInfo.LedNum);

                else dwRet = TPR_FAIL;
            } // if g_bInteractive

        } // if Check_NLedGetDeviceInfo()
        else dwRet = TPR_FAIL;
    } // for
            
    if( TPR_PASS == dwRet ) SUCCESS( "Get LED Support Info Test Passed" );
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
            g_pKato->Log( LOG_DETAIL, TEXT( "NLED supports information:" ) );
            displayNLED_SUPPORTS_INFO( ledSupportsInfo );
        } // if Check_NLedGetDeviceInfo()
        else dwRet = TPR_FAIL;

        // Now get the settings information.
        if( Check_NLedGetDeviceInfo( NLED_SETTINGS_INFO_ID,
                                     &ledSettingsInfo ) )
        {
            g_pKato->Log( LOG_DETAIL, TEXT( "NLED Settings information:" ) );
            displayNLED_SETTINGS_INFO( ledSettingsInfo );

            if( ( LED_BLINK == ledSettingsInfo.OffOnBlink )
                && ( ( ledSettingsInfo.OnTime + ledSettingsInfo.OffTime )
                != ledSettingsInfo.TotalCycleTime ) )
            {
                FAIL( "TotalCycleTime != OnTime + OffTime" );
                dwRet = TPR_FAIL;
            } // if ledSettingsInfo

            // If in interactive mode, solicit user verification of settings
            // info.
            if( g_bInteractive )
            {
                if( skipNLedDriverTest( ledSettingsInfo.LedNum ) )
                {
                    g_pKato->Log(
                        LOG_DETAIL,
                        TEXT(
                        "Skipping interactive portions of NLED driver test for LED."
                        ) );
                    g_pKato->Log( LOG_DETAIL,
                        TEXT( "\tThe user has indicated that LED %u is controlled" ),
                        ledSettingsInfo.LedNum );
                    g_pKato->Log( LOG_DETAIL,
                        TEXT( "\tby another driver, and therefore may not behave according to NLED driver specifications." ) );
                } // if skipNLedDriverTest
                else if( TPR_PASS != InteractiveVerifyNLEDSettings(
                    ledSettingsInfo ) ) dwRet = TPR_FAIL;
            } // if g_bInteractive

        } // if Check_NLedGetDeviceInfo()
        else dwRet = TPR_FAIL;
    } // for
    if( TPR_PASS == dwRet ) SUCCESS( "Validate NLED Settings Passed" );
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
                      TEXT( "\tLast Error = %d" ),
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
                                             2000000,
                                             1000000,
                                             1000000,
                                                   4,
                                                   4 };

    NLED_SETTINGS_INFO ledSettingsInfo_S2 = {      0,
                                           LED_BLINK,
                                             3000000,
                                             1500000,
                                             1500000,
                                                   6,
                                                   6 };

    UINT               uLedNum            = 0;

    // Verify that NLedSetDevice() doesn't fail when passed a NULL buffer
    // pointer (second parameter).
    SetLastError( ERROR_SUCCESS );
    if( NLedSetDevice( NLED_SETTINGS_INFO_ID, NULL ) ) g_pKato->Log(
        LOG_DETAIL,
        TEXT( "NLedSetDevice( NLED_SETTINGS_INFO_ID, NULL ) succeed." ) );
    else
    { // !NLedSetDevice
        FAIL("NLedSetDevice() failed when passed a NULL buffer pointer.");
        g_pKato->Log( LOG_DETAIL, TEXT( "Last Error = %d" ), GetLastError() );
        dwRet = TPR_FAIL;
    } // else !NLedSetDevice()

    for( uLedNum = 0; uLedNum < GetLedCount(); uLedNum++ )
    {
        // Attempt to change the LED settings information and verify results.
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "Attempt to change blink settings for NLED %u." ),
            uLedNum
            );

        ledSettingsInfo_S1.LedNum = uLedNum;
        if( TPR_PASS == ValidateNLEDSettingsChanges( ledSettingsInfo_S1 ) )
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "NLedSetDevice(NLED_SETTINGS_INFO_ID, ...) passes for NLED %u." ),
                uLedNum
                );
        else
        {
            FAIL( "NLedSetDevice(NLED_SETTINGS_INFO_ID, ...) failed first test." );
            dwRet = TPR_FAIL;
        }

        // Make another attempt to change the LED settings using different
        // information.
        g_pKato->Log(
            LOG_DETAIL,
            TEXT( "Attempt to change blink settings for NLED %u again." ),
            uLedNum
            );
        ledSettingsInfo_S2.LedNum = uLedNum;
        if( TPR_PASS == ValidateNLEDSettingsChanges( ledSettingsInfo_S2 ) )
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "NLedSetDevice(NLED_SETTINGS_INFO_ID, ...) passes for NLED %u." ),
                uLedNum
                );
        else
        {
            FAIL( "NLedSetDevice(NLED_SETTINGS_INFO_ID, ...) failed second test." );
            dwRet = TPR_FAIL;
        }

    } // for

    // Restore all original LED Info settings.
    if( !setAllLedSettings( pOriginalLedSettingsInfo ) ) dwRet = TPR_FAIL;
    delete[] pOriginalLedSettingsInfo;
    pOriginalLedSettingsInfo = NULL;

    if( TPR_PASS == dwRet ) SUCCESS( "Verify Set & Get NLED Settings Passed" );
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
    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    BOOL               bInteractive            = g_bInteractive;
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
                TEXT( "LED %u, original OffOnBlink setting = %d: saved." ),
                ledSettingsInfoOriginal.LedNum,
                ledSettingsInfoOriginal.OffOnBlink );
        else // !NLedGetDeviceInfo
        {
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "LED %u, unable to save original OffOnBlink setting, will turn it OFF at end of test." ),
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
                          TEXT( "Failed to turn LED %u OFF" ),
                          uLedNum );
            dwRet = TPR_FAIL;
        }            

        Sleep(1000);
        // Turn the LED ON.
        if( !SetLEDOffOnBlink( uLedNum, LED_ON, bInteractive ) )
        {
            g_pKato->Log( LOG_FAIL,
                          TEXT( "Failed to turn LED %u ON" ),
                          uLedNum );
            dwRet = TPR_FAIL;
        }

        Sleep(1000);
        // Turn the LED back OFF.
        if( !SetLEDOffOnBlink( uLedNum, LED_OFF, bInteractive ) )
        {
            g_pKato->Log( LOG_FAIL,
                          TEXT( "Failed to turn LED %u OFF" ),
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
                TEXT( "Failed to restore LED %u to its original setting." ),
                uLedNum );
            dwRet = TPR_FAIL;
        }
    }

    // Display message, if there are no LEDs to test.
    if( 0 == GetLedCount() )
        g_pKato->Log( LOG_DETAIL, TEXT( "No LEDs to Test. Automatic pass." ) );
    else
        if( TPR_PASS == dwRet ) SUCCESS( "LED ON/OFF Test Passed" );
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

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    BOOL                bInteractive             = g_bInteractive;
    DWORD               dwLastErr                = 0;
    DWORD               dwRet                    = TPR_PASS;
    NLED_SETTINGS_INFO ledSettingsInfo           = { 0,
                                                   LED_BLINK,
                                                   1000000,
                                                   500000,
                                                   500000,
                                                   1,
                                                   0 };
    NLED_SETTINGS_INFO* pOriginalLedSettingsInfo = NULL;
    NLED_SUPPORTS_INFO  ledSupportsInfo          = { 0 };

    // Allocate space to save origional LED setttings.
    SetLastError( ERROR_SUCCESS );
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];
    if( !pOriginalLedSettingsInfo )
    {
        FAIL("new failed");
        g_pKato->Log( LOG_DETAIL,
                      TEXT( "\tLast Error = %d" ),
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
                              TEXT( "LED %u does not support blinking" ),
                              ledSupportsInfo.LedNum );
            }
            else // !0
            {
                // Store settings information, which supports blinking.
                ledSettingsInfo.LedNum = ledSupportsInfo.LedNum;
                ledSettingsInfo.MetaCycleOn = 0;
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
                                      TEXT( "Failed to turn LED %u OFF." ),
                                      ledSupportsInfo.LedNum );
                        dwRet = TPR_FAIL;
                    } // if !LED_OFF
    
                    // set the LED state to blinking
                    if( !SetLEDOffOnBlink( ledSupportsInfo.LedNum,
                                           LED_BLINK,
                                           bInteractive ) )
                    {
                        g_pKato->Log( LOG_FAIL,
                                      TEXT( "Failed to Blink LED %u." ),
                                      ledSupportsInfo.LedNum );
                        dwRet = TPR_FAIL;
                    } // if !LED_BLINK

                    // Turn LED off again.
                    if( !SetLEDOffOnBlink( ledSupportsInfo.LedNum,
                                           LED_OFF,
                                           bInteractive ) )
                    {
                        g_pKato->Log( LOG_FAIL,
                                      TEXT( "Failed to turn LED %u OFF." ),
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
            FAIL( "NLedGetDeviceInfo( NLED_SUPPORTS_INFO_ID )" );
            g_pKato->Log( LOG_DETAIL,
                     TEXT( "Unable to query supports information for LED number %u. Last Error = %d" ),
                     ledSupportsInfo.LedNum,
                     GetLastError() );
            dwRet = TPR_FAIL;
        } // else NLedGetDeviceInfo() failed
    } // for

    if( !setAllLedSettings( pOriginalLedSettingsInfo ) ) dwRet = TPR_FAIL;
    if( TPR_PASS == dwRet ) SUCCESS( "LED Blink Test Passed" );
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
//  "Set NLED_INFO Invalid Blink Setting" to verify that attempts to make
//  unsupported blink time adjustments are handled appropriately.
//
//*****************************************************************************    
{

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    BOOL                bInteractive             = g_bInteractive;
    DWORD               dwRet                    = TPR_PASS;
    UINT                LedNum                   = 0;
    NLED_SETTINGS_INFO* pOriginalLedSettingsInfo = NULL;
    TCHAR               szMessage[ 50 ]          = TEXT( "" );

    // Allocate space to save origional LED setttings.
    SetLastError( ERROR_SUCCESS );
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];
    if( !pOriginalLedSettingsInfo )
    {
        FAIL("new failed");
        g_pKato->Log( LOG_DETAIL,
                      TEXT( "\tLast Error = %d" ),
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
        bInteractive = g_bInteractive;
        if( g_bInteractive )
        {
            if( skipNLedDriverTest( LedNum ) )
            {
                g_pKato->Log(
                    LOG_DETAIL,
                    TEXT(
                    "Skipping interactive portions of NLED driver test for LED."
                    ) );
                g_pKato->Log( LOG_DETAIL,
                    TEXT( "\tThe user has indicated that LED %u is controlled" ),
                    LedNum );
                g_pKato->Log( LOG_DETAIL,
                    TEXT( "\tby another driver, and therefore may not behave according to NLED driver specifications." ) );
                bInteractive = FALSE;
            } // if skipNLedDriverTest
        } //  if g_bInteractive

        // Perform total cycle time adjustment test and log result.
        if( TestLEDBlinkTimeSettings( ADJUST_TOTAL_CYCLE_TIME,
                                      LedNum,
                                      bInteractive ) )
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "\tNLED %u passed adjust blink total cycle time test." ),
                LedNum );
        else // total cycle time adjustment test failed
        {
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "\tNLED %u failed adjust blink total cycle time test." ),
                LedNum );
            dwRet = TPR_FAIL;
        } // else total cycle time adjustment test failed

        // Perform on time adjustment test and log result.
        if( TestLEDBlinkTimeSettings( ADJUST_ON_TIME,
                                      LedNum,
                                      bInteractive ) )
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "\tNLED %u passed adjust blink on time test." ),
                LedNum );
        else // on time adjustment test failed
        {
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "\tNLED %u failed adjust blink on time test." ),
                LedNum );
            dwRet = TPR_FAIL;
        } // else on time adjustment test failed

        // Perform off time adjustment test and log result.
        if( TestLEDBlinkTimeSettings( ADJUST_OFF_TIME,
                                      LedNum,
                                      bInteractive ) )
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "\tNLED %u passed adjust blink off time test." ),
                LedNum );
        else // off time adjustment test failed
        {
            g_pKato->Log(
                LOG_DETAIL,
                TEXT( "\tNLED %u failed adjust blink off time test." ),
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
//  "Set NLED_INFO Invalid Blink Setting" to verify that attempts to make
//  unsupported blink cycle adjustments are handled appropriately.
//
//*****************************************************************************
{

    if( uMsg != TPM_EXECUTE ) return TPR_NOT_HANDLED;

    BOOL                bInteractive             = g_bInteractive;
    DWORD               dwMboxValue              = 0;    
    DWORD               dwRet                    = TPR_PASS;
    NLED_SETTINGS_INFO  ledSettingsInfoDefault   = { 0,
                                                     LED_BLINK,
                                                     DEF_CYCLE_TIME,
                                                     DEF_ON_TIME,
                                                     DEF_OFF_TIME,
                                                     META_CYCLE_ON,
                                                     META_CYCLE_OFF };
    NLED_SUPPORTS_INFO  ledSupportsInfo          = { 0 };
    NLED_SETTINGS_INFO* pOriginalLedSettingsInfo = NULL;
    TCHAR               szMessage[ 50 ]          = TEXT( "" );
    UINT                uAdjustmentType          = PROMPT_ONLY;

    // Allocate space to save origional LED setttings.
    SetLastError( ERROR_SUCCESS );
    pOriginalLedSettingsInfo = new NLED_SETTINGS_INFO[ GetLedCount() ];
    if( !pOriginalLedSettingsInfo )
    {
        FAIL("new failed");
        g_pKato->Log( LOG_DETAIL,
                      TEXT( "\tLast Error = %d" ),
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
            g_pKato->Log( LOG_DETAIL, TEXT( "NLED Supports Information" ) );
            displayNLED_SUPPORTS_INFO( ledSupportsInfo );

            // Determine if the user wants to bypass interactive mode for this
            // LED.
            bInteractive = g_bInteractive;
            if( g_bInteractive
                && skipNLedDriverTest( ledSettingsInfoDefault.LedNum ) )
            {
                g_pKato->Log(
                    LOG_DETAIL,
                    TEXT( "Skipping interactive portions of NLED driver test for LED."
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
            } //  if g_bInteractive

                    // ------------------------------------------------------------
            // If Meta Cycle On time is adjustable, test it.

            if( ledSupportsInfo.fMetaCycleOn )
                for( uAdjustmentType  = PROMPT_ONLY;
                     uAdjustmentType <= LENGTHEN;
                     uAdjustmentType++ )
                    if( TPR_PASS != ValidateMetaCycleSettings(
                        ledSettingsInfoDefault,
                        METACYCLEON,
                        uAdjustmentType,
                        bInteractive ) ) dwRet = TPR_FAIL;

            // If Meta Cycle Off time is adjustable, test it.

            if( ledSupportsInfo.fMetaCycleOff )
                for( uAdjustmentType  = PROMPT_ONLY;
                     uAdjustmentType <= LENGTHEN;
                     uAdjustmentType++ )

                    if( TPR_PASS != ValidateMetaCycleSettings(
                        ledSettingsInfoDefault,
                        METACYCLEOFF,
                        uAdjustmentType,
                        bInteractive ) ) dwRet = TPR_FAIL;

                // turn the LED off
                if( !SetLEDOffOnBlink( ledSettingsInfoDefault.LedNum,
                                       LED_OFF,
                                       bInteractive ) )
                {
                    g_pKato->Log( LOG_FAIL,
                        TEXT( "Failed to turn LED %u OFF." ),
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
