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
/*
 * tuxFunctionTable.cpp
 */

/*
 * pull in the function defs for the alarm tests
 */

#include "tuxRTCAlarm.h"

FUNCTION_TABLE_ENTRY g_lpFTE[] = {

  _T("RTC Alarm Tests"    ), 0, 0, 0, NULL,

  _T("Usage message for the RTC Alarm tests"), 1, 0, 1, UsageMessage,

  _T("Verify RTC Alarm Resolution"), 1, 0, 1000, testAlarmResolution,

  _T("Verify RTC Alarm fires after the correct duration"), 1, 0, 1001, testAlarmFiresAfterGivenDuration,

  _T("Verify RTC Alarm fires at all times that fall within the RTC supported time range"), 1, 0, 1002, testAlarmFiresAtGivenTime,

  _T("Verify RTC alarm fires at user provided time and duration"), 1, 0, 1003, testUserGivenAlarmTime,

   NULL, 0, 0, 0, NULL  // marks end of list
};


