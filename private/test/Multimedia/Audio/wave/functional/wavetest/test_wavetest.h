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
#pragma once
#ifndef __TEST_wavetest_H__
#define __TEST_wavetest_H__
#include "TUXMAIN.H"
#include "MAP.H"

// For use with Dynamic Linking Without Imports
// This enable the Audio CETK to run on headless devices which 
// do not have displays and this do not have windowing libraries installed.

// define a function type for MessageBox
typedef int (WINAPI * PFNMESSAGEBOX)(HWND, LPCTSTR, LPCTSTR, UINT);

// define a function type for PostQuitMessage
typedef void (WINAPI * PFNPOSTQUITMESSAGE)(int);

// define a function type for PostMessage
typedef BOOL (WINAPI * PFNPOSTMESSAGE)(HWND, UINT, WPARAM, LPARAM); 

// define a function type for PostThreadMessage
typedef BOOL (WINAPI * PFNPOSTTHREADMESSAGE)(DWORD, UINT, WPARAM, LPARAM);

// define a function type for RegisterClass
typedef ATOM (WINAPI * PFNREGISTERCLASS)(const WNDCLASS*);

// define a function type for CreateWindow
typedef HWND (WINAPI * PFNCREATEWINDOW)(LPCTSTR, LPCTSTR, DWORD, int, int, int, int, HWND, HMENU, HANDLE, PVOID ); 

// define a function type for CreateWindowEx
typedef HWND (WINAPI * PFNCREATEWINDOWEX)( DWORD,  LPCTSTR, LPCTSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID); 

// define a function type for SetEvent
typedef BOOL (WINAPI * PFNSETEVENT)(HANDLE); 

// define a function type for GetMessage
typedef BOOL (WINAPI * PFNGETMESSAGE)(LPMSG, HWND, UINT, UINT ); 

// define a function type for TranslateMessage
typedef BOOL (WINAPI * PFNTRANSLATEMESSAGE)( const MSG*); 

// define a function type for DispatchMessage
typedef LONG (WINAPI * PFNDISPATCHMESSAGE)(  const MSG*);

// define a function type for DestroyWindow
typedef BOOL (WINAPI * PFNDESTROYWINDOW)(HWND);

// define a function type for UnRegisterClass
typedef BOOL (WINAPI * PFNUNREGISTERCLASS)( LPCTSTR, HINSTANCE); 

// define a function type for DefWindowProc
typedef LRESULT (WINAPI * PFNDEFWINDOWPROC)(HWND, UINT, WPARAM, LPARAM); 

// define a function type for GetSystemPowerState
typedef DWORD (WINAPI * PFNGETSYSTEMPOWERSTATE)( LPWSTR, DWORD, PDWORD);

// define a function type for SetSystemPowerState
typedef DWORD (WINAPI * PFNSETSYSTEMPOWERSTATE)( LPCWSTR, DWORD, DWORD);

HMODULE GetCoreDLLHandle();

BOOL FreeCoreDLLHandle(HMODULE hCoreDLL);

// Audio Type Defines for Power Management Tests
#ifndef PLAYSOUND_SYNC
#define PLAYSOUND_SYNC 0
#endif 

#ifndef PLAYSOUND_ASYNC
#define PLAYSOUND_ASYNC 1
#endif

#ifndef SNDPLAYSOUND_SYNC
#define SNDPLAYSOUND_SYNC 2
#endif 

#ifndef SNDPLAYSOUND_ASYNC
#define SNDPLAYSOUND_ASYNC 3
#endif 

#ifndef WAVEOUT  
#define WAVEOUT 4
#endif 

// for Mixing tests in milliseconds
// RLE Dec 5, 2005 #define PLAY_GAP       1009
#define PLAY_GAP        509 // RLE Dec 5, 2005
#define TPR_RERUN_TEST (TPR_ABORT+1000) 

typedef struct _AUDIOTYPE_TAG {
    DWORD           dwAudioType;
} AUDIOTYPE, *LPAUDIOTYPE;

static AUDIOTYPE aatTests[] = {
    PLAYSOUND_SYNC,
    PLAYSOUND_ASYNC,
    SNDPLAYSOUND_SYNC,
    SNDPLAYSOUND_ASYNC,
    WAVEOUT,
    0
};

typedef struct _LATENCY_TAG {
    int             iTime;
    LPTSTR          szWaveFormat;
} LATENCYINFO, *LPLATENCYINFO;


static LATENCYINFO aliLatencyTests[] = {
    4096, TEXT( "WAVE_FORMAT_8000M08"  ),
    4096, TEXT( "WAVE_FORMAT_8000M16"  ),
    4096, TEXT( "WAVE_FORMAT_8000S08"  ),
    4096, TEXT( "WAVE_FORMAT_8000S16"  ),
    4096, TEXT( "WAVE_FORMAT_16000M08" ),
    4096, TEXT( "WAVE_FORMAT_16000M16" ),
    4096, TEXT( "WAVE_FORMAT_16000S08" ),
    4096, TEXT( "WAVE_FORMAT_16000S16" ),
    4096, TEXT( "WAVE_FORMAT_1M08"     ),
    4096, TEXT( "WAVE_FORMAT_1M16"     ),
    4096, TEXT( "WAVE_FORMAT_1S08"     ),
    4096, TEXT( "WAVE_FORMAT_1S16"     ),
    4096, TEXT( "WAVE_FORMAT_2M08"     ),
    4096, TEXT( "WAVE_FORMAT_2M16"     ),
    4096, TEXT( "WAVE_FORMAT_2S08"     ),
    4096, TEXT( "WAVE_FORMAT_2S16"     ),
    4096, TEXT( "WAVE_FORMAT_4M08"     ),
    4096, TEXT( "WAVE_FORMAT_4M16"     ),
    4096, TEXT( "WAVE_FORMAT_4S08"     ),
    4096, TEXT( "WAVE_FORMAT_4S16"     ),
    4096, TEXT( "WAVE_FORMAT_48000M08" ),
    4096, TEXT( "WAVE_FORMAT_48000M16" ),
    4096, TEXT( "WAVE_FORMAT_48000S08" ),
    4096, TEXT( "WAVE_FORMAT_48000S16" ),
    0, NULL,
};

TESTPROCAPI BVT                          (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI EasyPlayback                 (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI PlaybackCapabilities         (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Playback                     (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI PlaybackNotifications        (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI PlaybackExtended             (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI PlaybackVirtualFree          (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI WaveOutTestReportedFormats   (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI PlaybackInitialLatency       (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI PlaybackInitialLatencySeries (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestVolume                   (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestwaveOutSet_GetVolume     (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestPowerUpAndDown           (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI PlaybackMixing               (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// Helper Functions for use by the tests
void        BlockAlignBuffer( DWORD* lpBufferLength, DWORD Alignment );
TESTPROCAPI GetHeadlessUserInput(TCHAR *szPrompt, TCHAR *tchResult);
TESTPROCAPI GetReturnCode(DWORD current, DWORD next);
DWORD       GetVolumeControl( UINT uDeviceID );
TESTPROCAPI LaunchMultipleAudioPlaybackThreads( UINT uMsg );
void        ValidateResolution( timer_type ttResolution );

#define REG_HKEY_ACTIVE TEXT("Drivers\\Active")
//#define MORE_VOLUME_TESTS 1

#define TPR_CODE_TO_TEXT(retVal) \
                    (   retVal == TPR_PASS  ? TEXT( "PASS"  ) \
                    : ( retVal == TPR_FAIL  ? TEXT( "FAIL"  ) \
                    : ( retVal == TPR_ABORT ? TEXT( "ABORT" ) \
                    : ( retVal == TPR_SKIP  ? TEXT( "SKIP"  ) \
                    : TEXT( "unknown" ) \
                    ) ) ) \
                    )

#define TT_RESOLUTION_THRESHOLD 1001

extern DWORD g_dwNoOfThreads;
extern int   g_iLatencyTestDuration;
extern TCHAR g_tcharUserInput;
extern DWORD g_dwOutDeviceID;
extern DWORD g_dwInDeviceID;
extern DWORD g_useSound;
extern DWORD g_interactive;
extern DWORD g_headless;
extern DWORD g_powerTests;
extern DWORD g_duration;
extern DWORD g_bSkipIn;
extern DWORD g_bSkipOut;
extern DWORD g_dwAllowance;
extern DWORD g_dwInAllowance;
extern DWORD g_dwSleepInterval;
extern TCHAR *g_pszCSVFileName;
extern TCHAR *g_pszWaveCharacteristics;
extern MapEntry<DWORD> lpFormats[];
extern MapEntry<DWORD> lpCallbacks[];
extern DWORD WINAPI PlaybackMixing_sndPlaySound_Thread( LPVOID lpParameter );
extern DWORD WINAPI PlaybackMixing_waveOutWrite_Thread( LPVOID lpParameter );
extern ULONG SineWave( void *lpvStart, ULONG dwNumBytes, WAVEFORMATEX *wfx,
    double dFrequency = 440.0 );
extern TESTPROCAPI StringFormatToWaveFormatEx(
    WAVEFORMATEX *wfx, const TCHAR* szWaveFormat );
extern TESTPROCAPI CommandLineToWaveFormatEx( WAVEFORMATEX* wfx );
extern HANDLE hEvent;
extern DWORD state;
extern HWND hwnd;

enum VOLUME_CONTROL { NONE, JOINT, SEPARATE, VOLUME_ERROR };

#endif