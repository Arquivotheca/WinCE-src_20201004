//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//


// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.

/**************************************************************************

Module Name:

   nfyBrdth.cpp

Abstract:

History:
***************************************************************************/
#include <windows.h>
#include <notify.h>
#include <tux.h>
#include "tuxmain.h"
#include "..\names.h"
#include "..\nfyLib\timelib.h"
#include "..\nfyLib\CMMFile.h"
#include "..\nfyLib\CNfyData.h"
extern HINSTANCE	globalExeInst;


BOOL ClearAllNotifications()
{
	HANDLE	NotifHandles[100];
	DWORD HandlesNeeded=0;

	CeGetUserNotificationHandles(NULL, 0, &HandlesNeeded);
	if(!HandlesNeeded) return false;

	BOOL bRet = CeGetUserNotificationHandles(NotifHandles, HandlesNeeded, &HandlesNeeded);

	for(DWORD i = 0; i< HandlesNeeded;i++)
		bRet &= CeClearUserNotification(NotifHandles[i]);

	return bRet;
}



//***********************************************************************************
//		Breadth Tests for CeRunAppAtEvent
//***********************************************************************************

TESTPROCAPI TestCeRunAppAtEvent(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	NO_MESSAGES;

	// Bad pointer
	if(CeRunAppAtEvent((TCHAR *)0xDEAD, NOTIFICATION_EVENT_SYNC_END))
		info(FAIL, TEXT("Line# %d: CeRunAppAtEvent succeeded for junk appname pointer"), __LINE__);

	// Null param test
	if(CeRunAppAtEvent(NULL, NOTIFICATION_EVENT_NONE))
		info(FAIL, TEXT("Line# %d: CeRunAppAtEvent succeeded for null appname"),__LINE__);

	// Invalid event
	if(CeRunAppAtEvent(APPNAME1, 420 ))
		info(FAIL, TEXT("Line# %d: CeRunAppAtEvent succeeded for invalid event"),__LINE__);

	// Good call
	if(!CeRunAppAtEvent(APPNAME1, NOTIFICATION_EVENT_SYNC_END))
		info(FAIL, TEXT("Line# %d: CeRunAppAtEvent failed for good params"),__LINE__);

	// Good call
	if(!CeRunAppAtEvent(APPNAME1, NOTIFICATION_EVENT_NONE))
		info(FAIL, TEXT("Line# %d: CeRunAppAtEvent failed for good params"),__LINE__);

	CeEventHasOccurred(NOTIFICATION_EVENT_SYNC_END, NULL);
	CeEventHasOccurred(NOTIFICATION_EVENT_NONE, NULL);
	Sleep(3000);
	return getCode();
}


//***********************************************************************************
//		Breadth Tests for CeRunAppAtTime
//***********************************************************************************

TESTPROCAPI TestCeRunAppAtTime(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	NO_MESSAGES;

	SYSTEMTIME time1;
	GetLocalTime(&time1);

	// Bad pointer
	if(CeRunAppAtTime((TCHAR *)0xDEAD, &time1))
		info(FAIL, TEXT("Line# %d: CeRunAppAtTime succeeded for junk appname pointer"),__LINE__);

	// Null pointer
	if(CeRunAppAtTime(NULL, &time1))
		info(FAIL, TEXT("Line# %d: CeRunAppAtTime succeeded for NULL appname pointer"),__LINE__);

	// Invalid system time
	time1.wMonth = 13;
	if(CeRunAppAtTime(APPNAME1, &time1))
		info(FAIL, TEXT("Line# %d: CeRunAppAtTime succeeded for invalid time"),__LINE__);

	//Good params
	GetLocalTime(&time1);
	if(!CeRunAppAtTime(APPNAME1, &time1))
		info(FAIL, TEXT("Line# %d: CeRunAppAtTime failed for good params"),__LINE__);

	Sleep(3000);
	return getCode();
}

//***********************************************************************************
//		Breadth Tests for CeSetUserNotification
//***********************************************************************************

TESTPROCAPI TestCeSetUserNotification(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	NO_MESSAGES;

	HANDLE h1;
	SYSTEMTIME time1;

	GetLocalTime(&time1);
	AddSecondsToSystemTime( &time1, 25);

	CE_USER_NOTIFICATION UserNotif = {PUN_DIALOG, TEXT("CeSetUserNotification"), TEXT("Notification BVT"), NULL, MAX_PATH, 0};

	// Bad Handle
	h1 = CeSetUserNotification((HANDLE)0xDEAD, APPNAME1, &time1, &UserNotif);
	if(h1) info(FAIL, TEXT("Line# %d: CeSetUserNotification succeeded for invalid handle"),__LINE__);

	// NULL App name
	h1 = CeSetUserNotification(NULL, NULL, &time1, &UserNotif);
	if(h1) info(FAIL, TEXT("Line# %d: CeSetUserNotification succeeded for null app name"),__LINE__);

	CeClearUserNotification(h1);

	// Good Call
	h1 = CeSetUserNotification(NULL, APPNAME1, &time1, &UserNotif);
	if(!h1) info(FAIL, TEXT("Line# %d: CeSetUserNotification failed for valid parameters. Error %d"), __LINE__,GetLastError());

	if(!CeClearUserNotification(h1))
		info(FAIL, TEXT("Line# %d: CeClearUserNotification failed for valid handle"),__LINE__);

	// Bad Date
	time1.wMonth += 12;
	h1 = CeSetUserNotification(NULL, APPNAME1, &time1, &UserNotif);
	if(h1) info(FAIL, TEXT("Line# %d: CeSetUserNotification succeeded for invalid date"),__LINE__);
	time1.wMonth -= 12;

	
	// NULL Dialog Title and text, Should succeed with no dialog displayed
	UserNotif.pwszDialogTitle = UserNotif.pwszDialogText = NULL;
	h1 = CeSetUserNotification(NULL, APPNAME1, &time1, &UserNotif);
	if(!h1) info(FAIL, TEXT("Line# %d: CeSetUserNotification failed for Null dialog Name and text. Error %d"),__LINE__, GetLastError());

	if(!CeClearUserNotification(h1))
		info(FAIL, TEXT("Line# %d: CeClearUserNotification failed for valid handle. Error %d"), __LINE__, GetLastError());

	return getCode();
}



//***********************************************************************************
//		Breadth Tests for CeClearUserNotification
//***********************************************************************************

TESTPROCAPI TestCeClearUserNotification(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	NO_MESSAGES;
	HANDLE h1;
	SYSTEMTIME time1;

	GetLocalTime(&time1);
	AddSecondsToSystemTime( &time1, 25);

	CE_USER_NOTIFICATION UserNotif = {PUN_DIALOG, TEXT("CeClearUserNotification"), TEXT("Notification BVT"), NULL, MAX_PATH, 0};
	h1 = CeSetUserNotification(NULL, APPNAME1, &time1, &UserNotif);
	if(!h1) info(FAIL, TEXT("Line# %d: CeSetUserNotification failed for valid parameters"),__LINE__);

	// Null handle
	if(CeClearUserNotification(NULL))
		info(FAIL, TEXT("Line# %d: CeClearUserNotification succeeded for Null handle"),__LINE__);

	// Invalid handle
	if(CeClearUserNotification((HANDLE)0xDDDD))
		info(FAIL, TEXT("Line# %d: CeClearUserNotification succeeded for invalid handle"),__LINE__);

	// Good call
	if(!CeClearUserNotification(h1))
		info(FAIL, TEXT("Line# %d: CeClearUserNotification failed for valid handle"),__LINE__);

	// Again. should fail
	if(CeClearUserNotification(h1))
		info(FAIL, TEXT("Line# %d: CeClearUserNotification passed 2nd time"),__LINE__);

	return getCode();
}


//***********************************************************************************
//		Breadth Tests for CeGetUserNotificationPreferences
//***********************************************************************************

TESTPROCAPI TestCeGetUserNotificationPreferences(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	NO_MESSAGES;

	CE_USER_NOTIFICATION UserNotif = {PUN_DIALOG, TEXT("CeGetUserNotificationPreferences"), TEXT("Notification Test Txt"), NULL, MAX_PATH, 0};

	// Null CE_USER_NOTIFICATION
	if(CeGetUserNotificationPreferences(NULL, NULL))
		info(FAIL, TEXT("Line# %d: CeGetUserNotificationPreferences succeeded for NULL CE_USER_NOTIFICATION"),__LINE__);

	// Invalid Parent window handle
	if( CeGetUserNotificationPreferences((HWND)0xDADA, &UserNotif))
		info(FAIL, TEXT("Line# %d: CeGetUserNotificationPreferences Passed with invalid parent window handle"),__LINE__);

	// NULL value for CE_USER_NOTIFICATION.pwszSound
	if( CeGetUserNotificationPreferences(NULL, &UserNotif)) 
		info(FAIL, TEXT("Line# %d: CeGetUserNotificationPreferences passed for null CE_USER_NOTIFICATION.pwszSound"),__LINE__);

	UserNotif.pwszSound = new TCHAR[MAX_PATH];

	// Invalid value for CE_USER_NOTIFICATION.nMaxSound 
	UserNotif.nMaxSound = 1;
	if( CeGetUserNotificationPreferences(NULL, &UserNotif)) 
		info(FAIL, TEXT("Line# %d: CeGetUserNotificationPreferences passed for invalid CE_USER_NOTIFICATION.nMaxSound"),__LINE__);
	UserNotif.nMaxSound = MAX_PATH;

	// Valid Breadths
	PostQuitMessage(0);	//	This cancels the following dialog
	if( CeGetUserNotificationPreferences(NULL, &UserNotif)) // should fail because dialog is cancelled.
		info(FAIL, TEXT("Line# %d: CeGetUserNotificationPreferences BVT failed"),__LINE__);

	delete (UserNotif.pwszSound);

	return getCode();
}

//***********************************************************************************
//		Breadth Tests for CeHandleAppNotifications
//***********************************************************************************

TESTPROCAPI TestCeHandleAppNotifications(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	NO_MESSAGES;

	// Null appname
	if(CeHandleAppNotifications(NULL))
		info(FAIL, TEXT("Line# %d: CeHandleAppNotifications passed for invalid Appname"),__LINE__);

	HANDLE h1;
	SYSTEMTIME time1;

	GetLocalTime(&time1);
	AddSecondsToSystemTime( &time1, 1);

	CE_USER_NOTIFICATION UserNotif = {PUN_DIALOG|PUN_SOUND|PUN_REPEAT, NULL, NULL, TEXT("default"), 8/*MAX_PATH*/, 0};
	h1 = CeSetUserNotification(NULL, APPNAME1, &time1, &UserNotif);
	if(!h1) info(FAIL, TEXT("Line# %d: CeSetUserNotification failed for valid parameters"),__LINE__);

	PrintSystemTime(&time1);

	Sleep(2000);
	// Valid call
	if(!CeHandleAppNotifications(APPNAME1))
		info(FAIL, TEXT("Line# %d: CeHandleAppNotifications failed for valid Appname"),__LINE__);

	/* this is not a reliable check
	if(!sndPlaySound(TEXT("default"), SND_SYNC|SND_NOSTOP))
		info(FAIL, TEXT("Line# %d:  CeHandleAppNotifications failed to turn off sound"));
	*/
	if(!CeClearUserNotification(h1))
		info(ECHO, TEXT("CeClearUserNotification failed for valid handle"),__LINE__);

	return getCode();
}

//***********************************************************************************
//		Breadth Tests for CeSetUserNotificationEx
//***********************************************************************************

TESTPROCAPI TestCeSetUserNotificationEx(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	NO_MESSAGES;

	SYSTEMTIME time1;
	GetLocalTime(&time1);
	AddSecondsToSystemTime( &time1, 25);

	CE_USER_NOTIFICATION UserNotif = {PUN_DIALOG, TEXT("CeSetUserNotificationEx"), TEXT("Notification BVT"), NULL, MAX_PATH, 0};
	CE_NOTIFICATION_TRIGGER NotifTrig = {sizeof(CE_NOTIFICATION_TRIGGER), CNT_TIME, 0, APPNAME1, TEXT(""), 0, 0};

	NotifTrig.stStartTime = time1;

	UserNotif.pwszSound = new TCHAR[MAX_PATH];

	HANDLE h1;

	// Null Trigger
	h1 = CeSetUserNotificationEx(NULL, NULL, &UserNotif);
	if( h1)
		info(FAIL, TEXT("Line# %d: CeSetUserNotificationEx succeeded for null trigger"),__LINE__);

	// Null UserNotification	--valid call
	h1 = CeSetUserNotificationEx(NULL, &NotifTrig, NULL);
	if( !h1)
		info(FAIL, TEXT("Line# %d: CeSetUserNotificationEx failed for null UserNotification"),__LINE__);
	if(!CeClearUserNotification(h1))
		info(FAIL, TEXT("Line# %d: CeClearUserNotification failed for valid handle"),__LINE__);

	// valid call
	h1 = CeSetUserNotificationEx(NULL, &NotifTrig, &UserNotif);
	if( !h1)
		info(FAIL, TEXT("Line# %d: CeSetUserNotificationEx failed for valid parameters"),__LINE__);
	if(!CeClearUserNotification(h1))
		info(FAIL, TEXT("Line# %d: CeClearUserNotification failed for valid handle"),__LINE__);

	//---- Invalid Notif Trigger
	//	Invalid dwSize
	NotifTrig.dwSize -= 500;
	h1 = CeSetUserNotificationEx(NULL, &NotifTrig, &UserNotif);
	if( h1)
		info(FAIL, TEXT("Line# %d: CeSetUserNotificationEx succeeded for invalid dwSize"),__LINE__);
	CeClearUserNotification(h1);
	NotifTrig.dwSize += 500;

	//	Invalid dwType
	NotifTrig.dwType += 500;
	h1 = CeSetUserNotificationEx(NULL, &NotifTrig, &UserNotif);
	if( h1)
		info(FAIL, TEXT("Line# %d:  CeSetUserNotificationEx succeeded for Invalid dwType"),__LINE__);
	NotifTrig.dwType -= 500;
	CeClearUserNotification(h1);

	//----

	//--- Invalid UserNotif
	UserNotif.ActionFlags = PUN_SOUND;
	//  invalid pwszSound
	delete (UserNotif.pwszSound ); UserNotif.pwszSound = NULL;
	h1 = CeSetUserNotificationEx(NULL, &NotifTrig, &UserNotif);
	if( h1)
		info(FAIL, TEXT("Line# %d: CeSetUserNotificationEx succeeded for invalid pwszSound"),__LINE__);
	//---

	return getCode();
}


//***********************************************************************************
//		Breadth Tests for CeGetUserNotificationHandles
//***********************************************************************************

TESTPROCAPI TestCeGetUserNotificationHandles(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	NO_MESSAGES;

	HANDLE h1=NULL;
	HANDLE h2=NULL;
	DWORD cHandles=0;
	DWORD HandlesNeeded=0;

	{
		// Clear all existing notifications
		HANDLE handles[10]; //will be <10
		CeGetUserNotificationHandles(NULL, 0, &HandlesNeeded);
		CeGetUserNotificationHandles(handles, HandlesNeeded, &cHandles);
		for(DWORD dwIndex=0; dwIndex<HandlesNeeded; dwIndex++)
			CeClearUserNotification(handles[dwIndex]);
	}

	//Set a notification
	SYSTEMTIME time1;
	GetLocalTime(&time1);
	AddSecondsToSystemTime( &time1, 25);
	CE_USER_NOTIFICATION UserNotif = {PUN_DIALOG, TEXT("CeGetUserNotificationHandles"), TEXT("Notification BVT"), NULL, MAX_PATH, 0};
	h1 = CeSetUserNotification(NULL, APPNAME1, &time1, &UserNotif);
	if(!h1) info(FAIL, TEXT("Line# %d: CeSetUserNotification failed for valid parameters"));

	//--Valid call
	if(!CeGetUserNotificationHandles(NULL, 0, &HandlesNeeded))
		info(FAIL, TEXT("Line# %d: CeGetUserNotificationHandles failed for valid param (find count)"),__LINE__);
	if(HandlesNeeded != 1)
		info(FAIL, TEXT("Line# %d: CeGetUserNotificationHandles returned %d notifications (<>1)"),__LINE__, HandlesNeeded);

	//--Invalid calls
	if(CeGetUserNotificationHandles(NULL, 10, &HandlesNeeded))
		info(FAIL, TEXT("Line# %d: CeGetUserNotificationHandles passed for invalid param"),__LINE__);
	if(CeGetUserNotificationHandles(NULL, 10, NULL))
		info(FAIL, TEXT("Line# %d: CeGetUserNotificationHandles passed for invalid param"),__LINE__);
	if(CeGetUserNotificationHandles(&h2, 0, NULL))
		info(FAIL, TEXT("Line# %d: CeGetUserNotificationHandles passed for invalid param"),__LINE__);

	//--Valid call
	if(!CeGetUserNotificationHandles(&h2, 1, &HandlesNeeded))
		info(FAIL, TEXT("Line# %d: CeGetUserNotificationHandles failed for valid param"),__LINE__);

	if(!CeClearUserNotification(h1))
		info(FAIL, TEXT("Line# %d: CeClearUserNotification failed for valid handle"),__LINE__);

	return getCode();
}



//***********************************************************************************
//		Breadth Tests for CeGetUserNotification
//***********************************************************************************

TESTPROCAPI TestCeGetUserNotification(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	NO_MESSAGES;

	SYSTEMTIME time1;
	GetLocalTime(&time1);

	CE_USER_NOTIFICATION UserNotif = {PUN_DIALOG, TEXT("CeGetUserNotification"), TEXT("Notification BVT"), NULL, MAX_PATH, 0};

	CE_NOTIFICATION_TRIGGER NotifTrig = {sizeof(CE_NOTIFICATION_TRIGGER), CNT_TIME, 0, APPNAME1, TEXT(""), 0, 0};
	NotifTrig.stStartTime = time1;

	HANDLE h2 = CeSetUserNotificationEx(NULL, &NotifTrig, &UserNotif);
	if( ! h2)
		info(FAIL, TEXT("Line# %d: CeSetUserNotificationEx BVT failed"),__LINE__);

	DWORD cBufferSize=0;
	BYTE *pBuffer = NULL;
	DWORD cBytesNeeded=0;
	SetLastError(0);

	//--NULL handle. should fail
	if(CeGetUserNotification(NULL, 0, &cBytesNeeded, NULL))
		info(FAIL, TEXT("Line# %d: CeGetUserNotification Passed with invalid params."),__LINE__);

	//--Invalid handle. should fail
	if(CeGetUserNotification((HANDLE)100, cBufferSize, &cBytesNeeded, pBuffer))
		info(FAIL, TEXT("Line# %d: CeGetUserNotification Passed with invalid params."),__LINE__);


	//--Invalid pointer. should fail
	if(CeGetUserNotification(h2, cBufferSize, &cBytesNeeded, NULL))
		info(FAIL, TEXT("Line# %d: CeGetUserNotification Passed with invalid params."),__LINE__);

	//--Invalid call. should fail but set cBytesNeeded
	if(CeGetUserNotification(h2, cBufferSize, &cBytesNeeded, pBuffer))
		info(FAIL, TEXT("Line# %d: CeGetUserNotification Passed with invalid params."),__LINE__);

	if(cBytesNeeded == 0 || cBytesNeeded > 500)
		info(FAIL, TEXT("Line# %d: invalid cBytesNeeded %d"), cBytesNeeded);


	cBufferSize = cBytesNeeded;
	pBuffer = (BYTE *) new BYTE[cBytesNeeded]; //(BYTE *)LocalAlloc(LMEM_FIXED, cBytesNeeded);

	//--Valid call. should pass
	if(!CeGetUserNotification(h2, cBufferSize, &cBytesNeeded, pBuffer))
		info(FAIL, TEXT("Line# %d: CeGetUserNotification failed for valid params %d"),__LINE__,GetLastError());
	delete[] pBuffer; //LocalFree(pBuffer);

	CeClearUserNotification(h2);

	return getCode();
}



//***********************************************************************************
//		Functional Tests for Notification
//          The test sets timed notification and checks if the app is lauched at the correct time
//		The app is expected to get launched within 10sec of expected time
//           The app should be getting command line params as expected
//***********************************************************************************

TESTPROCAPI TestNotificationsTime(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	NO_MESSAGES;

	ClearAllNotifications();

	CNfyData nfyData;	//<-- This creates memory mapped file for interprocess communication
	nfyData.ClearData();

	SYSTEMTIME time1,time2;
	HANDLE h1;

	CE_NOTIFICATION_TRIGGER NotifTrig[] = 
		{
				{sizeof(CE_NOTIFICATION_TRIGGER), CNT_TIME, 0, APPNAME1, TEXT("WINDOWS CE IS THE BEST"), 0, 0},
				{sizeof(CE_NOTIFICATION_TRIGGER), CNT_TIME, 0, APPNAME1, TEXT("1"), 0, 0},
				{sizeof(CE_NOTIFICATION_TRIGGER), CNT_TIME, 0, APPNAME1, TEXT("2"), 0, 0},
				{sizeof(CE_NOTIFICATION_TRIGGER), CNT_TIME, 0, APPNAME1, TEXT(""), 0, 0},

				{sizeof(CE_NOTIFICATION_TRIGGER), CNT_TIME, 0, APPNAME1, TEXT("WINDOWS CE ONE"), 0, 0},
				{sizeof(CE_NOTIFICATION_TRIGGER), CNT_TIME, 0, APPNAME1, TEXT("WINDOWS CE TWO"), 0, 0},
				{sizeof(CE_NOTIFICATION_TRIGGER), CNT_TIME, 0, APPNAME1, TEXT("WINDOWS CE THREE"), 0, 0}
		};
	int delay[] = {6, 15,126,337, 450, 661, 875};	// delay in seconds
	int num = sizeof(NotifTrig)/sizeof(NotifTrig[0]);



	// Setup a series of Timed Notifications
	GetLocalTime(&time1);
	for(int i=0;i<num;i++)
	{
		time2 = time1;
		AddSecondsToSystemTime( &time2, delay[i]);
		NotifTrig[i].stStartTime = time2;

		// Setup to launch notification app without alert dialog
		h1 = CeSetUserNotificationEx(NULL, &NotifTrig[i], NULL);
		if( !h1)
			info(FAIL, TEXT("Line# %d: CeSetUserNotificationEx failed for valid parameters"),__LINE__);
	}


	// Get Pointer to shared memory
	NOTIFTESTDATA *pntd = nfyData.GetNotificationTestData();
	if(!pntd)
		info(FAIL, TEXT("Line# %d: Problems with memory mapped file"),__LINE__);


	// Wait till all Notifications have fired
	for(i=0;i<=1000;i++)
	{
		if(pntd->count >= num) break;
		Sleep(1000);
	}


	// Check cmdlines and time of launch
	if(pntd->count < num)
		info(FAIL, TEXT("Line# %d: Some Notify Apps Failed to launch"),__LINE__);

	for(i=0;i<pntd->count;i++)
	{
		if(0 != _tcscmp(pntd->CmdLine[i], NotifTrig[i].lpszArguments ) ) // if equal
			info(FAIL, TEXT("Line# %d: App received wrong command line: %s instead of %s"),__LINE__, pntd->CmdLine[i], NotifTrig[i].lpszArguments);

		//Check time when app was launched	###### need a date compare routine
		info(ECHO, TEXT("Time Diff %d"), SystemTimeDiff(NotifTrig[i].stStartTime, pntd->LaunchTime[i]));
		if(SystemTimeDiff(NotifTrig[i].stStartTime, pntd->LaunchTime[i]) > 10)
		{
			info(ECHO, TEXT("Expected Launch Time:"));
			PrintSystemTime(&NotifTrig[i].stStartTime);
			info(FAIL, TEXT("Line# %d: Actual Launch Time:"),__LINE__);
			PrintSystemTime(&pntd->LaunchTime[i]);
		}
	}

	ClearAllNotifications();

	return getCode();
 }


//***********************************************************************************
//		Functional Tests for Event based Notification
//          The test sets event based notification and then it simulates the events
//		It varifies that the app gets launched as expected
//***********************************************************************************


TESTPROCAPI TestNotificationsEvent(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	NO_MESSAGES;

	ClearAllNotifications();

	CNfyData nfyData;	//<-- This creates memory mapped file for interprocess communication
	nfyData.ClearData();

	HANDLE h1;

	CE_NOTIFICATION_TRIGGER NotifTrig[] = 
		{
			{sizeof(CE_NOTIFICATION_TRIGGER), CNT_EVENT, NOTIFICATION_EVENT_TIME_CHANGE,	APPNAME1, TEXT("1"), 0, 0},
			{sizeof(CE_NOTIFICATION_TRIGGER), CNT_EVENT, NOTIFICATION_EVENT_SYNC_END,		APPNAME1, TEXT("2"), 0, 0},
			{sizeof(CE_NOTIFICATION_TRIGGER), CNT_EVENT, NOTIFICATION_EVENT_ON_AC_POWER,	APPNAME1, TEXT("3"), 0, 0},
			{sizeof(CE_NOTIFICATION_TRIGGER), CNT_EVENT, NOTIFICATION_EVENT_OFF_AC_POWER,	APPNAME1, TEXT("4"), 0, 0},
			{sizeof(CE_NOTIFICATION_TRIGGER), CNT_EVENT, NOTIFICATION_EVENT_NET_CONNECT,	APPNAME1, TEXT("5"), 0, 0},
			{sizeof(CE_NOTIFICATION_TRIGGER), CNT_EVENT, NOTIFICATION_EVENT_NET_DISCONNECT,	APPNAME1, TEXT("6"), 0, 0},
			{sizeof(CE_NOTIFICATION_TRIGGER), CNT_EVENT, NOTIFICATION_EVENT_DEVICE_CHANGE,	APPNAME1, TEXT("7"), 0, 0},
			{sizeof(CE_NOTIFICATION_TRIGGER), CNT_EVENT, NOTIFICATION_EVENT_IR_DISCOVERED,	APPNAME1, TEXT("8"), 0, 0},
			{sizeof(CE_NOTIFICATION_TRIGGER), CNT_EVENT, NOTIFICATION_EVENT_RS232_DETECTED,	APPNAME1, TEXT("9"), 0, 0}
		};
	int num = sizeof(NotifTrig)/sizeof(NotifTrig[0]);


	// Setup a series of Timed Notifications
	for(int i=0;i<num;i++)
	{
		// Setup to launch notification app without alert dialog
		h1 = CeSetUserNotificationEx(NULL, &NotifTrig[i], NULL);
		if( !h1)
			info(FAIL, TEXT("Line# %d: CeSetUserNotificationEx failed for valid parameters"),__LINE__);
	}


	// Get Pointer to shared memory
	NOTIFTESTDATA *pntd = nfyData.GetNotificationTestData();
	if(!pntd)
		info(FAIL, TEXT("Line# %d: Problems with memory mapped file"),__LINE__);


	// Simulate all Notification events and wait till they have fired
	for(i=0;i<num;i++)
	{
		CeEventHasOccurred( NotifTrig[i].dwEvent, NULL );
		if(pntd->count >= num) break;
		Sleep(1000);
	}


	// Check cmdlines and time of launch
	if(pntd->count < num)
		info(FAIL, TEXT("Line# %d: Some Notify Apps Failed to launch. %d launched out of %d"),__LINE__, pntd->count, num);

	for(i=0;i<pntd->count;i++)
	{
		if(0 != _tcscmp(pntd->CmdLine[i], NotifTrig[i].lpszArguments ) ) // if equal
			info(FAIL, TEXT("Line# %d: App received wrong command line: %s"),__LINE__, pntd->CmdLine[i]);
	}

	return getCode();
}




