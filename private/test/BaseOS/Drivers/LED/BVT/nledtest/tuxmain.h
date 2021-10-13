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

#pragma once


#include <windows.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include <led_drvr.h>
#include <nled.h>

// Test function prototypes (TestProc's)
TESTPROCAPI TestGetLEDCount(                          UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestGetLEDSupportsInfo(                   UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestGetLEDSettingsInfo(                   UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestSetLEDSettingsInfo(                   UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestNLedGetDeviceInfoInvaladParms(        UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestNLedGet_WithInvaladNLED(              UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestNLedSetDeviceInvaladParms(            UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestNLedSet_WithInvaladNLED(              UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestNLedSet_WithInvaladBlinkSetting(      UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI VerifyNLEDCount(                          UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI VerifyNLEDSupports(                       UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI ValidateNLEDSettings(                     UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI VerifySetGetNLEDSettings(                 UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestLEDOffOnOff(                          UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestLEDOffBlinkOff(                       UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestLEDBlinkCycleTime(                    UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestLEDMetaCycleBlink(                    UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );

// Nled Driver - Test Procs
TESTPROCAPI NLedSetWithInvalidParametersTest(         UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI VerifyNledIndex(                          UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI VerifyNledGetSupportsInfo(                UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI VerifyNledMetaCycleSupportsInfo(          UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI VerifyNledTotalOnOffTimeSupportsInfo(     UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI VerifyNledSuspendResume(                  UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI VerifyNledRegistryCount(                  UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI VerifyNledRegistrySettings(               UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NledGroupingTest(                         UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI VerifyNledType(                           UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NledDriverRobustnessTests(                UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI VerifyNledBlinking(                       UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NledSystemBehaviorTest(                   UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NledDriverMemUtilizationTest(             UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NledBlinkCtrlLibMemUtilizationTest(       UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );

// Nled BlinkCtrl Library -  Test Procs
TESTPROCAPI VerifyBlinkCtrlLibNledBlinking(           UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI VerifyBlinkctrlLibraryRegistrySettings(   UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NledCpuUtilizationTest(                   UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI BlinkCtrlLibWithBlinkOffBlinkTest(        UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI VerifyBlinkCtrlLibValidNledBlinkTime(     UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI BlinkCtrlLibSystemBehaviorTest(           UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );

// Nled Service - Test Procs
TESTPROCAPI NLEDServiceEventPathMappingTest(          UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NLEDServiceBlinkingTest(                  UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NLEDServiceBlinkSettingsTest(             UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NledServiceRobustnessTests(               UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NledServiceSuspendResumeTest(             UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NledServiceGroupingTest(                  UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NledServiceEventPriorityOrderTest(        UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NledServiceBlinkOffTest(                  UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NledServiceEventsWithOutPriorityTest(     UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NledServiceEventsSamePriorityTest(        UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestSetLEDUsingServiceAndFunction(        UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NLEDServiceSimultaneousBlinkOffTest(      UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NLEDServiceEventDelayTest(                UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NledServiceContinuousRegistryChangesTest( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );

// Helper function prototypes
BOOL ProcessCommandLine( LPCTSTR szDLLCmdLine );

// BASE is a unique value assigned to a given tester or component.  This value,
// when combined with each of the following test's unique IDs, allows every
// test case within the entire team to be uniquely identified.

#define BASE 1000

// Our function table that we pass to Tux
static FUNCTION_TABLE_ENTRY g_lpFTE[] =
{
       TEXT( "Group 1: BVT Tests"                         ), 0, 0,           0, NULL,
       TEXT( "Get NLED Count Info"                        ), 1, 0, BASE +    1, TestGetLEDCount,
       TEXT( "Get NLED Supports Info"                     ), 1, 0, BASE +    2, TestGetLEDSupportsInfo,
       TEXT( "Get NLED Settings Info"                     ), 1, 0, BASE +    3, TestGetLEDSettingsInfo,
       TEXT( "Set NLED Settings Info"                     ), 1, 0, BASE +  103, TestSetLEDSettingsInfo,

       TEXT( "Group 2: Robustness Tests"                  ), 0, 0,           0, NULL,
       TEXT( "Get NLED_INFO Invalid Parms"                ), 1, 0, BASE + 1000, TestNLedGetDeviceInfoInvaladParms,
       TEXT( "Get NLED_INFO Invalid NLED #"               ), 1, 0, BASE + 1001, TestNLedGet_WithInvaladNLED,
       TEXT( "Set NLED_INFO Invalid Parms"                ), 1, 0, BASE + 1100, TestNLedSetDeviceInvaladParms,
       TEXT( "Set NLED_INFO Invalid NLED #"               ), 1, 0, BASE + 1101, TestNLedSet_WithInvaladNLED,
       TEXT( "Set NLED_INFO Invalid Blink Setting"        ), 1, 0, BASE + 1102, TestNLedSet_WithInvaladBlinkSetting,

       TEXT( "Group 3: Verify NLED Data"                  ), 0, 0,           0, NULL,
       TEXT( "Verify Get NLED Count"                      ), 1, 0, BASE + 2001, VerifyNLEDCount,
       TEXT( "Verify Get NLED Supports"                   ), 1, 0, BASE + 2002, VerifyNLEDSupports,
       TEXT( "Verify Get NLED Settings & Validate Them"   ), 1, 0, BASE + 2003, ValidateNLEDSettings,
       TEXT( "Verify Set & Get NLED Settings"             ), 1, 0, BASE + 2103, VerifySetGetNLEDSettings,

       TEXT( "Group 4: Operational API Tests"             ), 0, 0,           0, NULL,
       TEXT( "LED OFF/ON/OFF Test"                        ), 1, 0, BASE + 3100, TestLEDOffOnOff,
       TEXT( "LED OFF/BLINK/OFF Test"                     ), 1, 0, BASE + 3101, TestLEDOffBlinkOff,
       TEXT( "LED Blink Cycle Time Test"                  ), 1, 0, BASE + 3102, TestLEDBlinkCycleTime,
       TEXT( "LED Meta Cycle Blink Test"                  ), 1, 0, BASE + 3103, TestLEDMetaCycleBlink,

       TEXT( "Group 5: Nled Driver Tests"                                ), 0, 0,           0, NULL,
       TEXT( "Verify NLED Blinking"                                      ), 1, 0, BASE + 4000, VerifyNledBlinking,
       TEXT( "Set NLED_SETTINGS Invalid Parms Test"                      ), 1, 0, BASE + 4001, NLedSetWithInvalidParametersTest,
       TEXT( "Verify Nled Index"                                         ), 1, 0, BASE + 4003, VerifyNledIndex,
       TEXT( "Verify Get NLED_SUPPORTS_INFO"                             ), 1, 0, BASE + 4005, VerifyNledGetSupportsInfo,
       TEXT( "Verify Get NLED_SUPPORTS for (MetaCycle[On][Off])"         ), 1, 0, BASE + 4006, VerifyNledMetaCycleSupportsInfo,
       TEXT( "Verify Get NLED_SUPPORTS for([TotalCycle][On][Off]Time)"   ), 1, 0, BASE + 4007, VerifyNledTotalOnOffTimeSupportsInfo,
       TEXT( "Verify Suspend Resume for Driver and BlinkCtrl Library"    ), 1, 0, BASE + 4008, VerifyNledSuspendResume,
       TEXT( "Verify NLED Registry Count"                                ), 1, 0, BASE + 4009, VerifyNledRegistryCount,
       TEXT( "NLED Driver Registry Robustness Tests"                     ), 1, 0, BASE + 4010, NledDriverRobustnessTests,
       TEXT( "Verify Nled Registry Settings"                             ), 1, 0, BASE + 4011, VerifyNledRegistrySettings,
       TEXT( "NLED Grouping Test"                                        ), 1, 0, BASE + 4012, NledGroupingTest,
       TEXT( "Verify NLED GetTypeInfo"                                   ), 1, 0, BASE + 4013, VerifyNledType,
       TEXT( "Check System Behavior Test"                                ), 1, 0, BASE + 4014, NledSystemBehaviorTest,

       TEXT( "Group 6: Nled Service Tests"                ), 0, 0,           0, NULL,
       TEXT( "NLED Service Event Path Mapping Test"       ), 1, 0, BASE + 5000, NLEDServiceEventPathMappingTest,
       TEXT( "NLED Service Blinking Test"                 ), 1, 0, BASE + 5001, NLEDServiceBlinkingTest,
       TEXT( "NLED Service Blink Settings Test"           ), 1, 0, BASE + 5002, NLEDServiceBlinkSettingsTest,
       TEXT( "Nled Service Event Delay Test"              ), 1, 0, BASE + 5003, NLEDServiceEventDelayTest,
       TEXT( "Nled Service Validation Test"               ), 1, 0, BASE + 5004, NLEDServiceSimultaneousBlinkOffTest,
       TEXT( "NLED Service Grouping Test"                 ), 1, 0, BASE + 5005, NledServiceGroupingTest,
       TEXT( "NLED Service Priority Test"                 ), 1, 0, BASE + 5006, NledServiceEventPriorityOrderTest,
       TEXT( "NLED Service Blink Off Test"                ), 1, 0, BASE + 5007, NledServiceBlinkOffTest,
       TEXT( "NLED Service Same Priority Test"            ), 1, 0, BASE + 5008, NledServiceEventsSamePriorityTest,
       TEXT( "NLED Service without Priority Test"         ), 1, 0, BASE + 5009, NledServiceEventsWithOutPriorityTest,
       TEXT( "NLED Service Suspend Resume Test"           ), 1, 0, BASE + 5010, NledServiceSuspendResumeTest,
       TEXT( "Nled Continuous state changes for each LED" ), 1, 0, BASE + 5011, NledServiceContinuousRegistryChangesTest,
       TEXT( "NLED Service Robustness Tests"              ), 1, 0, BASE + 5012, NledServiceRobustnessTests,

       TEXT( "Group 7: Nled Blink Control Library Tests"                 ), 0, 0,           0, NULL,
       TEXT( "NLED Blink Control Library SetLed() Test"                  ), 1, 0, BASE + 6000, VerifyBlinkCtrlLibNledBlinking,
       TEXT( "NLED Blink Control Library Verify Registry Settings Test"  ), 1, 0, BASE + 6001, VerifyBlinkctrlLibraryRegistrySettings,
       TEXT( "NLED CPU Utilization Test"                                 ), 1, 0, BASE + 6002, NledCpuUtilizationTest,
       TEXT( "NLED Blink Control Library Blink OFF Blink Test "          ), 1, 0, BASE + 6004, BlinkCtrlLibWithBlinkOffBlinkTest,
       TEXT( "NLED Blink Control Library Verify Blink Time Test"         ), 1, 0, BASE + 6005, VerifyBlinkCtrlLibValidNledBlinkTime,
       TEXT( "NLED Blink Control Library Check System behavior Test"     ), 1, 0, BASE + 6006, BlinkCtrlLibSystemBehaviorTest,

       TEXT( "Group 8: Memory Tests"                      ), 0, 0,           0, NULL,
       TEXT( "NLED Driver Memory Utilization Test"        ), 1, 0, BASE + 7001, NledDriverMemUtilizationTest,
       TEXT( "NLED BlinkCtrl Lib Memory Utilization Test" ), 1, 0, BASE + 7002, NledBlinkCtrlLibMemUtilizationTest,

      // marks end of list
       NULL,                                               0, 0,           0, NULL
};
