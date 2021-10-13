//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once
#ifndef __TEST_wavetest_H__
#define __TEST_wavetest_H__
#include "TUXMAIN.H"
#include "MAP.H"

TESTPROCAPI BVT				(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI EasyPlayback		(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI PlaybackCapabilities	(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Playback			(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI PlaybackNotifications	(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI PlaybackExtended		(UINT,TPPARAM,LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestVolume		(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

#define REG_HKEY_ACTIVE TEXT("Drivers\\Active")
//#define MORE_VOLUME_TESTS 1

extern DWORD g_dwDeviceNum;
extern DWORD g_useSound;
extern DWORD g_interactive;
extern DWORD g_duration;
extern DWORD g_bSkipIn;
extern DWORD g_bSkipOut;
extern DWORD g_dwAllowance;
extern DWORD g_dwInAllowance;
extern DWORD g_dwSleepInterval;
extern MapEntry<DWORD> lpFormats[];
extern MapEntry<DWORD> lpCallbacks[];
extern ULONG SineWave(void *lpvStart,ULONG dwNumBytes, WAVEFORMATEX *wfx);
extern TESTPROCAPI StringFormatToWaveFormatEx(WAVEFORMATEX *wfx,TCHAR*szWaveFormat);
extern HANDLE hEvent;
extern DWORD state;
extern HWND hwnd;

#endif
