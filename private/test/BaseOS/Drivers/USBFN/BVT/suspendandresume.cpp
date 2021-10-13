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
// Copyright (c) 1996-2003 Microsoft Corporation.  All rights reserved.
//
// --------------------------------------------------------------------


#include <windows.h>
#include <time.h>
#include <tchar.h>
#include "tuxmain.h"
#include "..\..\common\inc\pwrtstutil.h"

#define CLOCKS_PER_SEC  1000

#define ERROR_GEN_FAILURE                31L
#define ERROR_SUCCESS                    0L
#define ERROR_SERVICE_DOES_NOT_EXIST     1060L
#define ERROR_NOT_SUPPORTED              50L



static CSysPwrStates* g_pCSysPwr = NULL;  //use the power management libraray API

//-------------------------------------------------------------------
// Purpose: Check whether the device supports power suspend & resume
//          if yes, proceeds to power suspend & resume
//          if no, the test will be skipped
// Parameter: DWORD in_dwSecsBeforeSuspend -- time before suspend
//--------------------------------------------------------------------

DWORD WINAPI
SuspendDeviceAndScheduleWakeupThread(DWORD userData)
{


    const DWORD TIME_BEFORE_RESUME = 15;

    DWORD l_dwRefMillis;
    DWORD l_dwRepeatSecs;
    DWORD l_dwRet;

    DWORD in_dwSecsBeforeSuspend = (DWORD) userData;

    WCHAR  state[1024] = {0};
    LPWSTR pState = &state[0];
    DWORD dwBufChars = (sizeof(state) / sizeof(state[0]));
    DWORD dwStateFlags = 0;

//check whether power management service exists on the device

    if (GetSystemPowerState(pState, dwBufChars, &dwStateFlags) != ERROR_SUCCESS ||
        GetSystemPowerState(pState, dwBufChars, &dwStateFlags) == ERROR_SERVICE_DOES_NOT_EXIST)
    {
         LOG(TEXT("ERROR: SKIPPING TEST: Power Management does not exist on this device.\n"));
        return ERROR_NOT_SUPPORTED;
    }

//Initialize time before suspend to 1 if it's not specified

   if (0==in_dwSecsBeforeSuspend)
    {
        in_dwSecsBeforeSuspend=1;
    }


    LOG(TEXT("\n\n"));
    LOG(TEXT(" Start counting down before going into power suspend ... \n\n\n"));


// counting down before power suspend

    for (l_dwRepeatSecs = 0;
         l_dwRepeatSecs < (DWORD)in_dwSecsBeforeSuspend;
         l_dwRepeatSecs++)
    {
        l_dwRefMillis = GetTickCount();
        while (GetTickCount()-l_dwRefMillis < CLOCKS_PER_SEC)
        {
            ; // spin
        }
        LOG(L"%2ld ", in_dwSecsBeforeSuspend - l_dwRepeatSecs-1);
    }

    LOG(TEXT("System is going to suspend now.\n"));

// power suspends if power management is suppored on this device

    if(g_pCSysPwr->SetupRTCWakeupTimer(TIME_BEFORE_RESUME) == FALSE ){

        LOG(TEXT("ERROR: SKIPPING TEST: Power Management does not exist on this device.\n"));
        g_pCSysPwr->CleanupRTCTimer();
        return ERROR_NOT_SUPPORTED;
    }
      else{

         LOG(TEXT("\n\n\n System power suspend is in progress, will resume in 5 secs ... \n"));

      }

    if((l_dwRet = SetSystemPowerState(_T("suspend"), NULL, POWER_FORCE)) != ERROR_SUCCESS){
		LOG(TEXT("ERROR: SetSystemPowerState failed.\n"));
		g_pCSysPwr->CleanupRTCTimer();
		return ERROR_GEN_FAILURE;
    }
	LOG(TEXT("\n\n System power has resumed.\n"));

	//Manually Force to On State
	if((l_dwRet = SetSystemPowerState(_T("on"), NULL, POWER_FORCE)) != ERROR_SUCCESS){
		LOG(TEXT("ERROR: SetSystemPowerState failed.\n"));
		g_pCSysPwr->CleanupRTCTimer();
		return ERROR_GEN_FAILURE;
    }
	LOG(TEXT("\n\n System power is now on.\n"));

	g_pCSysPwr->CleanupRTCTimer();
    return ERROR_SUCCESS;
}


/****** test procs ************************************************************/


DWORD TestSuspendAndResume()
{

  g_pCSysPwr = new CSysPwrStates();

  DWORD dwStatus,dwRet = (DWORD)-1;
  dwStatus=SuspendDeviceAndScheduleWakeupThread(10);

  if(dwStatus == ERROR_NOT_SUPPORTED)
      {
            dwRet =  ERROR_NOT_SUPPORTED;
            LOG(_T("Suspend Resume Unsupported"));

      }
  if(dwStatus == ERROR_SUCCESS)
  {
        dwRet = ERROR_SUCCESS;
        LOG(_T("Successfully Suspended and Resumed"));
  }

  return (dwRet);

}

