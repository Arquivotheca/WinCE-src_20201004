//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _AUTO_DIAL_PRIV_H_
#define _AUTO_DIAL_PRIV_H_

// Contains privatly used AutoDial structures and reg keys.

#include <windows.h>
#include <ras.h>
#include <cxport.h>
#include <raserror.h>
#include <autodial.h>
#include "natglob.h"

// Registry Values
// Base registry key
#define RK_AUTODIAL    L"COMM\\Autodial"

// Is Autodial enabled?
#define RV_ENABLED					TEXT("Enabled")
// Hang up the phone after this many milliseconds 
#define RV_IDLE_TIMEOUT	   			TEXT("IdleTimeoutMS")
// Phone book entry names to dial, trying #1 first and then #2
#define RV_ENTRYNAME1 				TEXT("RasEntryName1")
#define RV_ENTRYNAME2 				TEXT("RasEntryName2")
// How much time to wait between multilpe unsuccessful calls to AutoDialStartConnection.
#define RV_FAIL_RETRY_WAIT			TEXT("FailRetryWaitMS")
// How many times to try and call through retry loop if we don't connect right away.
#define RV_REDIAL_ATTEMPTS			TEXT("RedialAttempts")
// How much time to pause between redails
#define RV_REDIAL_PAUSE_TIME		TEXT("RedialPauseTimeMS")
// Do we automatically switch autodial to be the public interface for Connection Sharing?
#define RV_MAKE_PUBLIC_INTERFACE    TEXT("EnableAsPublicInterface")

// Default values.  These are used if the registry settings are not set.
#define DEFAULT_IDLE_TIMEOUT		1000*60*30
#define DEFAULT_FAIL_RETRY_WAIT		0
#define DEFAULT_REDIAL_PAUSE_TIME   0
#define DEFAULT_REDIAL_ATTEMPTS		0

// Amount of time we wait between firing up the thread to look at net activity
#ifdef DEBUG
// Make the quanta shorter for easier testing.
#define HANGUP_SLEEP_INTERVAL		1000*10   
#else
#define HANGUP_SLEEP_INTERVAL		1000*60	
#endif
// Length of list we keep that contains failure times.
#define FAIL_RETRY_ATTEMPTS    		3

// Autodial status
#ifdef __cplusplus 
extern "C"
{
#endif

DWORD StatusInit(HINSTANCE hInstance);
DWORD StatusUninit(HINSTANCE hInstance);
LRESULT CALLBACK StatusWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

#define WND_CLASS L"AutoDialStatusWindowsClass"

#ifdef __cplusplus 
}
#endif



#endif // _AUTO_DIAL_PRIV_H_

