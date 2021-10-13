//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
#ifndef __TEST_DSNDTEST_H__
#define __TEST_DSNDTEST_H__
#include "TUXMAIN.H"
#include "MAP.H"

TESTPROCAPI BVT				(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI EasyPlayback		(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI PlaybackCapabilities	(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Playback			(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Playback_Software		(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Playback_Hardware_Streaming	(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Playback_Hardware_Static	(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Playback_Primary		(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI PlaybackNotifications	(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI PlaybackExtended		(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);

#define REG_HKEY_ACTIVE TEXT("Drivers\\Active")
#define CALLBACK_NONE	0
#define CALLBACK_DIRECTSOUNDNOTIFY 1

#ifndef UNDER_CE
#define Priority(pDS,dwType) \
{ \
	HWND hWnd=GetForegroundWindow(); \
	if (hWnd==NULL)  \
		hWnd=GetDesktopWindow(); \
	if(dwType==DSBCAPS_PRIMARYBUFFER)  \
		pDS->SetCooperativeLevel(hWnd,DSSCL_WRITEPRIMARY); \
	else \
		pDS->SetCooperativeLevel(hWnd,DSSCL_NORMAL); \
}
#else
#define Priority(pDS,dwType) ;
#endif			

extern DWORD g_all;
extern HINSTANCE g_hInstance;
extern DWORD g_dwDeviceNum;
extern DWORD g_bSkipOut;
extern DWORD g_bSkipIn;
extern DWORD g_useNOTIFY;
extern DWORD g_interactive;
extern MapEntry<DWORD> lpFormats[];
extern MapEntry<DWORD> lpSecondaryFormats[];
extern MapEntry<DWORD> lpPrimaryFormats[];
extern MapEntry<DWORD> lpCallbacks[];
extern DWORD g_duration;
extern DWORD g_useSound;
extern LPGUID g_DS;
extern LPGUID g_DSC;
extern DWORD g_dwAllowance;
extern DWORD g_dwInAllowance;
extern DWORD g_dwSleepInterval;
extern ULONG SineWave(void *lpvStart,ULONG dwNumBytes, WAVEFORMATEX *wfx);
extern TESTPROCAPI StringFormatToWaveFormatEx(WAVEFORMATEX *wfx,TCHAR*szWaveFormat);

extern HINSTANCE		g_hInstDSound;
extern PFNDSOUNDCAPTURECREATE	g_pfnDSCCreate;
extern PFNDSOUNDCREATE		g_pfnDSCreate;

#endif

