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

#ifndef __TUXDEMO_H__
#define __TUXDEMO_H__

#include <windows.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include <led_drvr.h>
#include <nled.h>

#define countof(a) (sizeof(a)/sizeof(*(a)))

#define ADJUST_TOTAL_CYCLE_TIME 0
#define ADJUST_ON_TIME          1
#define ADJUST_OFF_TIME         2
#define ADJUST_META_CYCLE_ON    3
#define ADJUST_META_CYCLE_OFF   4

extern BOOL g_bInteractive;
extern BOOL g_bVibrator;
extern UINT g_uMinNumOfNLEDs;

// Test function prototypes (TestProc's)
TESTPROCAPI TestGetLEDCount		  (              UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestGetLEDSupportsInfo(              UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestGetLEDSettingsInfo(              UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestSetLEDSettingsInfo(              UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestNLedGetDeviceInfoInvaladParms(   UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestNLedGet_WithInvaladNLED(         UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestNLedSetDeviceInvaladParms(       UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestNLedSet_WithInvaladNLED(         UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestNLedSet_WithInvaladBlinkSetting( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI VerifyNLEDCount(                     UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI VerifyNLEDSupports(                  UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI ValidateNLEDSettings(                UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI VerifySetGetNLEDSettings(            UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestLEDOffOnOff(                     UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestLEDOffBlinkOff(                  UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestLEDBlinkCycleTime(               UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestLEDMetaCycleBlink(               UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );

// Helper function prototypes
BOOL  ProcessCommandLine( LPCTSTR szDLLCmdLine );
BOOL  SetLEDOffOnBlink( UINT unLedNum, int iOffOnBlink, BOOL bInteractive );
BOOL  TestLEDBlinkTimeSettings( DWORD dwSetting,
                                UINT  LedNum,
                                BOOL  Interactive );
BOOL  ValidateLedSettingsInfo( NLED_SETTINGS_INFO ledSettingsInfo,
                               NLED_SETTINGS_INFO ledSettingsInfo_Valid );
DWORD VerifyNLEDSupportsOption( BOOL bOption, LPCTSTR szPrompt );

// BASE is a unique value assigned to a given tester or component.  This value,
// when combined with each of the following test's unique IDs, allows every 
// test case within the entire team to be uniquely identified.

#define BASE 1000

// Our function table that we pass to Tux
static FUNCTION_TABLE_ENTRY g_lpFTE[] =
{
   	TEXT( "Group 1: BVT Tests"	                     ), 0, 0,           0, NULL,
   	TEXT( "Get NLED Count Info"	                     ), 1, 0, BASE +    1, TestGetLEDCount,
   	TEXT( "Get NLED Supports Info"                   ), 1, 0, BASE +    2, TestGetLEDSupportsInfo,
   	TEXT( "Get NLED Settings Info"                   ), 1, 0, BASE +    3, TestGetLEDSettingsInfo,
   	TEXT( "Set NLED Settings Info"                   ), 1, 0, BASE +  103, TestSetLEDSettingsInfo,

    TEXT( "Group 2: Robustness Tests"                ), 0, 0,           0, NULL,
   	TEXT( "Get NLED_INFO Invalid Parms"              ), 1, 0, BASE + 1000, TestNLedGetDeviceInfoInvaladParms,
   	TEXT( "Get NLED_INFO Invalid NLED #"             ), 1, 0, BASE + 1001, TestNLedGet_WithInvaladNLED,
   	TEXT( "Set NLED_INFO Invalid Parms"              ), 1, 0, BASE + 1100, TestNLedSetDeviceInvaladParms,
   	TEXT( "Set NLED_INFO Invalid NLED #"             ), 1, 0, BASE + 1101, TestNLedSet_WithInvaladNLED,
   	TEXT( "Set NLED_INFO Invalid Blink Setting"      ), 1, 0, BASE + 1102, TestNLedSet_WithInvaladBlinkSetting,

   	TEXT( "Group 3: Verify NLED Data"                ), 0, 0,           0, NULL,
   	TEXT( "Verify Get NLED Count"	                 ), 1, 0, BASE + 2001, VerifyNLEDCount,
   	TEXT( "Verify Get NLED Supports"                 ), 1, 0, BASE + 2002, VerifyNLEDSupports,
   	TEXT( "Verify Get NLED Settings & Validate Them" ), 1, 0, BASE + 2003, ValidateNLEDSettings,
   	TEXT( "Verify Set & Get NLED Settings"           ), 1, 0, BASE + 2103, VerifySetGetNLEDSettings,

   	TEXT( "Group 4: Operational API Tests"	         ), 0, 0,           0, NULL,
   	TEXT( "LED OFF/ON/OFF Test"	                     ), 1, 0, BASE + 3100, TestLEDOffOnOff,
   	TEXT( "LED OFF/BLINK/OFF Test"	                 ), 1, 0, BASE + 3101, TestLEDOffBlinkOff,
   	TEXT( "LED Blink Cycle Time Test"	             ), 1, 0, BASE + 3102, TestLEDBlinkCycleTime,
   	TEXT( "LED Meta Cycle Blink Test"                ), 1, 0, BASE + 3103, TestLEDMetaCycleBlink,

    // marks end of list
   	NULL,                                               0, 0,           0, NULL
};

#endif // __TUXDEMO_H__
