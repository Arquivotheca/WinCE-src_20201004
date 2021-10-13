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
#include <tchddi.h>
#include <tchstream.h>
#include "miniqueue.h"

// --------------------------------------------------------------------
// global constants
//---------------------------------------------------------------------
#define BVT_BASE  0000
#define API_BASE  1000
#define FUNC_BASE 2000
#define PM_BASE   3000
#define FI_BASE   4000
#define PERF_BASE 5000
#define SEC_BASE  6000
#define DETECTOR_BASE 7000

#define WNDNAME         TEXT("Touch Panel Test")
#define TOUCH_DRIVER    TEXT("tchproxy.dll") 

#define TOUCH_SCALE                 4
// touch filter parameters 
#define MIN_MOVE                    20
#define MIN_TIME                    500

#define CROSSHAIR_DIAMETER          10

#define MSG_YES_NO                  TEXT("YES                              NO")
#define MSG_TAP_SCREEN              TEXT("Tap screen to continue")

// thread return values
#define CALIBRATION_ABORTED         0
#define CALIBRATION_NOT_ABORTED     1

// time values
#define CALIBRATION_TIMEOUT              12000     // 12 seconds
#define TEST_TIME                        8000      // 8 seconds
#define USER_WAIT_TIME                   20000     // 20 seconds. Delay to get user input of test results
#define INPUT_DELAY_MS                   500       // half second Delay to start a new test
#define DRAWING_TEST_TIME                15000    // 15 seconds, for drawing test
#define SHORT_DRAWING_TEST_TIME          12000    // 12 seconds, use in other tests that requires drawing 

// touch event properties flags
#define PEN_WENT_DOWN                    0x00000001
#define PEN_WENT_UP                      0x00000002
#define PEN_MOVE                         0x00000004

enum DRIVER_VERSION
{
    NO_TOUCH = 0,
    CE6 = 6,
    CE7 = 7,
};

enum TOUCH_PANEL
{
    CAPACITIVE = 1,
    RESISTIVE = 2,
};

// max name length for a touch driver dll
#define MAX_TOUCHDRV_NAMELEN        128

#define MAX_NUM_SAMPLES 502  //Size of "point buffer" used in PDD performance test

// global macros
#define ABS_UI(X)   ((X) < 0 ? (-X) : (X))
#define countof(a) (sizeof(a)/sizeof(*(a)))

const DWORD MAX_OPT_ARG = 256;
const TCHAR END_OF_STRING = (TCHAR)NULL;
const TCHAR BAD_ARG = _T('?');
const TCHAR ARG_FLAG = _T('-');
const TCHAR OPT_ARG = _T(':');

// --------------------------------------------------------------------
// structs
//---------------------------------------------------------------------

// struct to hold touch point data
typedef struct _TOUCH_POINT_DATA {
    TOUCH_PANEL_SAMPLE_FLAGS flags;
    DWORD dwTouchEventFlags;
    INT x;
    INT y;   
} TOUCH_POINT_DATA, *LPTOUCH_POINT_DATA;    

// thread control data structure
typedef struct _ABORT_THREAD_DATA {
    DWORD   dwCountdownMs;
    BOOL    fTerminate;
} ABORT_THREAD_DATA, *LPABORT_THREAD_DATA;


typedef struct _PDD_PERF_DATA_ {

    DWORD dwStartTime;
    DWORD dwEndTime;
    DWORD dwSamples;

} PDD_PERF_DATA, *LPPDD_PERF_DATA;


// --------------------------------------------------------------------
// Externs
//---------------------------------------------------------------------
extern CKato*                                   g_pKato;
extern SPS_SHELL_INFO*                          g_pShellInfo;
// touch message queue
extern MiniQueue<TOUCH_POINT_DATA, 100>         g_touchPointQueue;



extern TCHAR optArg[MAX_OPT_ARG];

extern HINSTANCE    g_hInstance;
extern TCHAR      g_szTouchDriver[MAX_TOUCHDRV_NAMELEN];

// function pointers
extern PFN_TOUCH_PANEL_GET_DEVICE_CAPS          g_pfnTouchPanelGetDeviceCaps;
extern PFN_TOUCH_PANEL_SET_MODE                 g_pfnTouchPanelSetMode;
extern PFN_TOUCH_PANEL_SET_CALIBRATION          g_pfnTouchPanelSetCalibration;
extern PFN_TOUCH_PANEL_CALIBRATE_A_POINT        g_pfnTouchPanelCalibrateAPoint;
extern PFN_TOUCH_PANEL_READ_CALIBRATION_POINT   g_pfnTouchPanelReadCalibrationPoint;
extern PFN_TOUCH_PANEL_READ_CALIBRATION_ABORT   g_pfnTouchPanelReadCalibrationAbort;
extern PFN_TOUCH_PANEL_ENABLE                   g_pfnTouchPanelEnable;
extern PFN_TOUCH_PANEL_DISABLE                  g_pfnTouchPanelDisable;
extern PFN_TOUCH_PANEL_POWER_HANDLER            g_pfnTouchPanelPowerHandler;
extern PFN_TOUCH_PANEL_INITIALIZE_CURSOR        g_pfnTouchPanelInitializeCursor;

// --------------------------------------------------------------------
// Function pointers
//---------------------------------------------------------------------

// calibration functions
BOOL CalibrateScreen( BOOL );
DWORD WINAPI CalibrationAbortThreadProc( LPVOID );

// for TouchPerf.cpp
void DoPddPerformTest            ( DWORD           );
int GetPddPerformTestStatus      ( PDD_PERF_DATA * );
void MessageBoxForPerformanceTest( DWORD,    DWORD );


//Power management tests
void MessageBoxForPowerSuspendTest( void );

//Registry query functions
static BOOL GetTouchDiverName( WCHAR *, int );

// common function to all touch test suites
int TouchDriverVersion();
INT  WinMainGetOpt( LPCTSTR szCmdLine, LPCTSTR szOptions );

//-----------------------------------------------------------------------


#define FAIL(x)         g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) )
                        
#define SUCCESS(x)      g_pKato->Log( LOG_PASS, \
                        TEXT("SUCCESS: %s"), TEXT(x) )

#define WARN(x)         g_pKato->Log( LOG_DETAIL, \
                        TEXT("WARNING in %s at line %d: %s"), \
                        __FILE_NAME__, __LINE__, TEXT(x) )

#define DETAIL(x)       g_pKato->Log( LOG_DETAIL, TEXT(x) )


