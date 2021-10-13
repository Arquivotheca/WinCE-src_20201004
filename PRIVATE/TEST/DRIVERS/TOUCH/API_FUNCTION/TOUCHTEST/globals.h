// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
// Copyright (c) 2000 Microsoft Corporation.  All rights reserved.
//                                                                     
// --------------------------------------------------------------------

#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include <windows.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include <tchddi.h>

#include "miniqueue.h"
#include "debug.h"

// --------------------------------------------------------------------
// global constants

#define WNDNAME         TEXT("Touch Panel Test")
#define TOUCH_DRIVER    TEXT("TOUCH.DLL")

// touch filter parameters 
#define MIN_MOVE                    20
#define MIN_TIME                    500

#define CROSSHAIR_DIAMETER          10

#define TOUCH_SCALE                 4

#define MSG_YES_NO                  TEXT("YES                              NO")
#define MSG_TAP_SCREEN              TEXT("Tap screen to continue")

// thread return values
#define CALIBRATION_ABORTED         0
#define CALIBRATION_NOT_ABORTED     1

// time values
#define CALIBRATION_TIMEOUT         10000   // 10 seconds
#define TEST_TIME                   5000    // 5 seconds
#define USER_WAIT_TIME              10000   // 10 seconds
#define INPUT_DELAY_MS              250     // 0.25 seconds

// touch event properties flags
#define PEN_WENT_DOWN               0x00000001
#define PEN_WENT_UP                 0x00000002
#define PEN_MOVE                    0x00000004

// max name length for a touch driver dll
#define MAX_TOUCHDRV_NAMELEN        128

// global macros
#define ABS_UI(X)   ((X) < 0 ? (-X) : (X))
#define countof(a) (sizeof(a)/sizeof(*(a)))

// --------------------------------------------------------------------
// structs

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

// --------------------------------------------------------------------
// global vars

//
// from winmain.cpp 
//
extern HINSTANCE    g_hInstance;
extern TCHAR        g_szTouchDriver[];

//
// from tuxmain.cpp:
//
// tux info
extern CKato*                                   g_pKato;
extern SPS_SHELL_INFO*                          g_pShellInfo;
// touch message queue
extern MiniQueue<TOUCH_POINT_DATA, 100>         g_touchPointQueue;
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
// function defs

// from winmain.cpp

// main processing functions
LRESULT CALLBACK WndProc( HWND, UINT, WPARAM, LPARAM ); 
BOOL TouchPanelCallback( TOUCH_PANEL_SAMPLE_FLAGS, INT, INT );

// init functions
BOOL LoadTouchDriver( VOID );
VOID UnloadTouchDriver( VOID );
BOOL Initialize( VOID );
VOID Deinitialize( VOID );

// user input functions
BOOL WaitForClick( DWORD );
BOOL WaitForInput( DWORD, BOOL );

// drawing functions
VOID SetDrawPen( BOOL );
VOID DrawLineTo( POINT );
VOID SetLineFrom( POINT );
VOID ClearDrawing( VOID );

// crosshair drawing functions
VOID SetCrosshair( DWORD, DWORD, BOOL );
BOOL DrawCrosshair( HDC, DWORD, DWORD, DWORD );
VOID HideCrosshair( VOID );

// text output functions
VOID PostTitle( LPCTSTR szFormat, ...);
VOID ClearTitle( VOID );
VOID PostMessage( LPCTSTR szFormat, ...);
VOID ClearMessage( VOID );

// calibration functions
BOOL CalibrateScreen( BOOL );
DWORD WINAPI CalibrationAbortThreadProc( LPVOID );

// from tuxmain.cpp

// debug text and logging functions
VOID SystemErrorMessage( DWORD );
VOID PrintSampleFlags( DWORD );

#endif

