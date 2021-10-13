//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*---------------------------------------------------------------------------*\
 *  module: dst.h
 *  purpose: header file for dst.cpp
 *
\*---------------------------------------------------------------------------*/

#pragma once

#include <windows.h>
#include <notify.h>

#include <svsutil.hxx>
#include <service.h>

#include <tzreg.h>
#include <dstioctl.h>
#include <kerncmn.h>

#if defined (DEBUG) || defined (_DEBUG)
#define ZONE_INIT               DEBUGZONE(0)        
#define ZONE_SERVER             DEBUGZONE(1)        
#define ZONE_CLIENT             DEBUGZONE(2)        
#define ZONE_PACKETS            DEBUGZONE(3)        
#define ZONE_TRACE              DEBUGZONE(4)        
#define ZONE_DST                DEBUGZONE(5)        

#define ZONE_MISC               DEBUGZONE(13)
#define ZONE_WARNING            DEBUGZONE(14)       
#define ZONE_ERROR              DEBUGZONE(15)       

#endif

#define MINUTES_TO_FILETIME(x) ((x) * (10 * 1000 * 1000 * 60LL))
#define MILLISECOND_TO_FILETIME(x)  ((x) * (1000 * 10L))

//The maximum timer resolution for CeRunAppAtTime is 60 seconds
#define MAX_TIMER_RESOLUTION            60000

//When we setup a timer for the year boundary, we should set it 
//at the first second of the new year. But actually, due to the 
//existence of timer resolution, we need delay it by several seconds 
//to make sure this timer is always triggered in the new year
#define YEAR_BOUNDARY_DELAY                 5000

//When user installs the dynamic DST data by other software, there 
//may be multiple registry notification triggered. To avoid performing 
//dynamic DST update too frequently, we will handle all registry 
//change notification but delay the DST update action by several 
//seconds. DYN_DST_UPDATE_DELAY is the time in milli-seconds that 
//we will delay after a registry notifcation.Due to system timer 
//resolution, the actual delay is DYN_DST_UPDATE_DELAY + System_Timer_Resolution
#define DYN_DST_UPDATE_DELAY                5000

//Due to the current system design, in some scenarios, when user 
//performs an action, there are multiple events being triggered. 
//This makes our event handling system more complicated, and even 
//worse, due to process and thread scheduling, those events are 
//actually triggered one by one but the order is not fixed. To 
//handle them better we will cache all those events triggered "at 
//the same time". Actually we will cache all the events consecutively 
//triggered within a predefined interval. MAX_INTERVAL_BETWEEN_CONSECUTIVE_EVENTS 
//is the maximum interval in milli-second
#define MAX_INTERVAL_BETWEEN_CONSECUTIVE_EVENTS     200

DWORD DSTThread(HANDLE hExitEvent);
int InitializeDST(HINSTANCE hInst);
void DestroyDST(void);
int StartDST(void);
int StopDST(void);
DWORD GetStateDST(void);


#ifdef DEBUG
// Event names for debugging
static LPCTSTR szEventNames[] = 
{
    TEXT("Event_Exit"),
    TEXT("Event_TzChange"),
    TEXT("Event_TzRegChange"),
    TEXT("Event_PreTimeChange"),
    TEXT("Event_TimeChange"),
    TEXT("Event_DynamicDSTUpdateTimer"),
    TEXT("Event_DSTTimer"),
};
#endif

// Event IDs
enum
{
    EI_FIRST = 0,
    EI_EXIT = EI_FIRST,
    EI_TZ_CHANGE,
    EI_TZ_REG_CHANGE,
    EI_PRE_TIME_CHANGE,
    EI_TIME_CHANGE,
    EI_DYN_DST_TIMER,
    EI_DST_TIMER,
    EI_NUM_EVENTS,
};


extern HANDLE g_rghEvents[EI_NUM_EVENTS];
extern HANDLE g_hDSTHandled;
extern DWORD g_dwDSTThreadId;
extern LPDSTSVC_TIME_CHANGE_INFORMATION g_lpTimeChangeInformation;
