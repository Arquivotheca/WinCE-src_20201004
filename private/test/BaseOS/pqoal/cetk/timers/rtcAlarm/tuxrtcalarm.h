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
 * tuxRTCAlarm.h
 */

#ifndef __TUX_RTC_ALARM_H__
#define __TUX_RTC_ALARM_H__


#include <windows.h>
#include <tux.h>

#define MIN_NKALARMRESOLUTION_MSEC      (1000)
#define MAX_NKALARMRESOLUTION_MSEC      (60*1000)



/**************************************************************************
 * 
 *                          RTC Alarm Test Functions
 *
 **************************************************************************/

/****************************** Usage Message *****************************/
TESTPROCAPI UsageMessage(
             UINT uMsg, 
             TPPARAM tpParam, 
             LPFUNCTION_TABLE_ENTRY lpFTE);


/*********************** RTC Alarm Resolution ***********************/
TESTPROCAPI testAlarmResolution(
             UINT uMsg, 
             TPPARAM tpParam, 
             LPFUNCTION_TABLE_ENTRY lpFTE);


/************************* RTC Alarm Times **************************/
TESTPROCAPI testAlarmFiresAfterGivenDuration(
             UINT uMsg, 
             TPPARAM tpParam, 
             LPFUNCTION_TABLE_ENTRY lpFTE);


/************************ OEMSetAlarmTime **************************/
TESTPROCAPI testAlarmFiresAtGivenTime(
             UINT uMsg, 
             TPPARAM tpParam, 
             LPFUNCTION_TABLE_ENTRY lpFTE);


/************************ User Alarm Time ***************************/
TESTPROCAPI testUserGivenAlarmTime(
             UINT uMsg, 
             TPPARAM tpParam, 
             LPFUNCTION_TABLE_ENTRY lpFTE);


#endif // __TUX_RTC_ALARM_H__
