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

#include "globals.h"
#include "WinMain.h"

#define __FILE_NAME__   TEXT("WINMAIN.CPP")


HINSTANCE   g_hInstance = NULL;

TCHAR g_szTouchDriver[MAX_TOUCHDRV_NAMELEN] = TOUCH_DRIVER;

LPCTSTR szClass;

static HWND          g_hWnd              = NULL;
static HDC            g_hDC               = NULL;
static HFONT          g_hFont             = NULL;
static HINSTANCE  g_hTouchDriver      = NULL;
static HPEN            g_hCrosshairPen     = NULL;
static HFONT         g_hTitleFont        = NULL;
static HFONT        g_hMessageFont      = NULL;


static BOOL         g_fShowCrosshair    = FALSE;
static BOOL         g_fShowCoordinates  = FALSE;

static BOOL         g_fCalibrated       = FALSE;

static DWORD        g_dwDisplayWidth    = 0;
static DWORD        g_dwDisplayHeight   = 0;

static DWORD        g_dwCrosshairX      = 0;
static DWORD        g_dwCrosshairY      = 0;

static BOOL         g_fDrawingOn        = FALSE;
static POINT        g_ptLine[2]         = {0};

static LOGPEN       g_logPen            = {0};
static LOGFONT      g_logFont           = {0};
static LOGFONT      g_msgFont           = {0};

static RECT         g_rectWorking       = {0};
static RECT         g_rectTitle         = {0};
static RECT         g_rectMessage       = {0};

static TCHAR        g_szTitle[50]       = TEXT("");
static TCHAR        g_szMessage[100]    = TEXT("");

static BOOL g_bKillWinThread;

static PDD_PERF_DATA  g_pddPerfData;
static DWORD          g_dwPddPerfSampleSize;
static int            g_nDoPddPerformTest = FALSE;

static HANDLE g_hThread = NULL;
static DWORD  g_dwThreadId;

MiniQueue<TOUCH_POINT_DATA, 100>         g_touchPointQueue;
TCHAR optArg[MAX_OPT_ARG] = TEXT("");

//-----------------------------------------------------------------------------
// global function pointers to the touch driver DLL functions
//-----------------------------------------------------------------------------
PFN_TOUCH_PANEL_GET_DEVICE_CAPS         g_pfnTouchPanelGetDeviceCaps        = NULL;
PFN_TOUCH_PANEL_SET_MODE                g_pfnTouchPanelSetMode              = NULL;
PFN_TOUCH_PANEL_SET_CALIBRATION         g_pfnTouchPanelSetCalibration       = NULL;
PFN_TOUCH_PANEL_CALIBRATE_A_POINT       g_pfnTouchPanelCalibrateAPoint      = NULL;
PFN_TOUCH_PANEL_READ_CALIBRATION_POINT  g_pfnTouchPanelReadCalibrationPoint = NULL;
PFN_TOUCH_PANEL_READ_CALIBRATION_ABORT  g_pfnTouchPanelReadCalibrationAbort = NULL;
PFN_TOUCH_PANEL_ENABLE                  g_pfnTouchPanelEnable               = NULL;
PFN_TOUCH_PANEL_ENABLE_EX               g_pfnTouchPanelEnableEx             = NULL;
PFN_TOUCH_PANEL_DISABLE                 g_pfnTouchPanelDisable              = NULL;
PFN_TOUCH_PANEL_POWER_HANDLER           g_pfnTouchPanelPowerHandler         = NULL;
PFN_TOUCH_PANEL_INITIALIZE_CURSOR       g_pfnTouchPanelInitializeCursor     = NULL;

DWORD
WINAPI CalibrationAbortThreadProc(LPVOID lpParam );

DWORD WINAPI WinThread( LPVOID lpParam );


// --------------------------------------------------------------------
VOID
SystemErrorMessage( DWORD dwMessageId )
{
    LPTSTR msgBuffer;       // string returned from system
    DWORD cBytes;           // length of returned string

    cBytes = FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_ALLOCATE_BUFFER,
        NULL,
        dwMessageId,
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        (TCHAR *)&msgBuffer,
        500,
        NULL );
    if( msgBuffer ) {
        msgBuffer[ cBytes ] = TEXT('\0');
        g_pKato->Log(LOG_DETAIL, TEXT( "SYSTEM ERROR %d: %s"),
            dwMessageId, msgBuffer );
        LocalFree( msgBuffer );
    }
    else {
        g_pKato->Log(LOG_DETAIL, TEXT( "FormatMessage error: %d"),
            GetLastError());
    }
}

//-----------------------------------------------------------------------------
// Main window procedure
//-----------------------------------------------------------------------------
LRESULT CALLBACK
WndProc(
    HWND hWnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam )
{
    static PAINTSTRUCT ps;
    static HDC      hdcPerf;
    static POINT    _pntClientOrigin;
    static BOOL     bPerfTestErr = FALSE;
    static POINT    _pntBuf[ MAX_NUM_SAMPLES ];
    static TCHAR        szMessage[100] = TEXT("");
    static TCHAR        szTitle[100] = TEXT("");


    HDC hdc;

    switch( message )
    {

    case WM_PAINT:

        hdc = BeginPaint( hWnd, &ps );

        // draw title only when title changes
        if ( wcsncmp(szTitle, g_szTitle, sizeof(g_szTitle)) != 0)
        {
            wcsncpy_s(szTitle, g_szTitle, sizeof(g_szTitle));
            SelectObject( hdc, g_hTitleFont );
            DrawText( hdc, g_szTitle, -1, &g_rectTitle, DT_CENTER );
        }

        // draw message only when message changes
        if ( wcsncmp(szMessage, g_szMessage, sizeof(g_szMessage)) != 0)
        {
            wcsncpy_s(szMessage, g_szMessage, sizeof(g_szMessage)/sizeof(g_szMessage[0]));
            SelectObject( hdc, g_hMessageFont );
            DrawText( hdc, g_szMessage, -1, &g_rectMessage, DT_CENTER );
        }

        // draw crosshair
        SelectObject( hdc, g_hCrosshairPen );
        if( g_fShowCrosshair )
        {
            DrawCrosshair( hdc, g_dwCrosshairX, g_dwCrosshairY, 15 );
        }

        if( g_fDrawingOn )
        {
            Polyline( hdc, g_ptLine, 2 );
            SetLineFrom( g_ptLine[1] );
        }

        EndPaint( hWnd, &ps );
        break;

    case WM_SETFOCUS:

        InvalidateRect( g_hWnd, &g_rectWorking, 0 );
        UpdateWindow( g_hWnd );
        break;

    case WM_DESTROY:

        PostQuitMessage(0);
        break;

    case WM_WINDOWPOSCHANGED:
        UpdateWindow( g_hWnd );
        break;

    default:
        break;

    }

    return DefWindowProc( hWnd, message, wParam, lParam );
}

// ----------------------------------------------------------------------------
// Callback routine to fetch sample inputs from TchProxy.
// It is registered in TouchPanelEnableEx
// ----------------------------------------------------------------------------
BOOL TouchPanelCallbackEx( DWORD cInputs,
                            __in_ecount(cInputs) PCETOUCHINPUT pInputs,
                           DWORD cbSize)
{
    static long XPrev  = 0;
    static long YPrev  = 0;
    static DWORD tNew   = 0;
    static DWORD tPrev  = 0;
    long deltaX = 0;
    long deltaY = 0;
    TOUCH_POINT_DATA tpd = {0};

    long X = 0;
    long Y = 0;
    DWORD Flags = 0;
    bool fRet = true;
    DWORD i = 0;

    for (i = 0; i < cInputs; i++)
    {
        const CETOUCHINPUT *pCurrent = &pInputs[i];

        if (pCurrent->dwFlags & TOUCHEVENTF_PRIMARY )
        {
            X = pCurrent->x;
            Y = pCurrent->y;
            Flags = pCurrent->dwFlags;
            break;
        }
    }

    if ( i == cInputs)  //no primary touch point found
    {
        goto Exit;
    }
    if( (Flags & TOUCHEVENTF_INRANGE) && ( (Flags & TOUCHEVENTF_DOWN) || (Flags & TOUCHEVENTF_MOVE )))
    {
        tNew = GetTickCount();

        if ( g_nDoPddPerformTest == TRUE )
        {
            if ( g_pddPerfData.dwStartTime == 0 )
            {
                g_pddPerfData.dwStartTime = tNew;
                g_pddPerfData.dwSamples   = 1;
            }
            else
            {
                ++g_pddPerfData.dwSamples;
                g_pddPerfData.dwEndTime = tNew;

               if ( g_pddPerfData.dwSamples >= g_dwPddPerfSampleSize )
               {
                   g_nDoPddPerformTest = FALSE;
               }
            }
        }
        if( Flags & TOUCHEVENTF_MOVE )
        {
            deltaX = X - XPrev;
            deltaY = Y - YPrev;
            // are the samples too close together?
            //
            if( (ABS_UI(deltaX) < MIN_MOVE) &&
                (ABS_UI(deltaY) < MIN_MOVE) &&
                ((tNew - tPrev) < MIN_TIME) )
            {
                tPrev = tNew;
                goto Exit;
            }
        }

       //pen down
        else if (Flags & TOUCHEVENTF_DOWN)
        {
            tpd.dwTouchEventFlags |= PEN_WENT_DOWN;
        }

        XPrev = X;
        YPrev = Y;
        tPrev = tNew;
    }
    else
    {
        tpd.dwTouchEventFlags |= PEN_WENT_UP;
    }

    tpd.dwTouchEventFlags |= PEN_MOVE;
    tpd.flags = Flags;
    tpd.x = X / TOUCH_SCALE;
    tpd.y = Y / TOUCH_SCALE;

    if( !(g_touchPointQueue.Enqueue( tpd )) )
    {
        // queue must be full, send a warning
        DETAIL("Touch message queue full! Data point will be lost!");
        fRet = FALSE;
    }

Exit:
    return fRet;
}


// ----------------------------------------------------------------------------
// Callback routine to fetch sample inputs from TchProxy.
// It is registered in TouchPanelEnable
// ----------------------------------------------------------------------------
BOOL TouchPanelCallback(
    TOUCH_PANEL_SAMPLE_FLAGS    Flags,
    INT                         X,
    INT                         Y )


{
    static INT32 XPrev  = 0;
    static INT32 YPrev  = 0;
    static DWORD tNew   = 0;
    static DWORD tPrev  = 0;
    static BOOL fTipDownPrev = FALSE;

    BOOL fRet = TRUE;
    INT32 deltaX = 0;
    INT32 deltaY = 0;
    TOUCH_POINT_DATA tpd = {0};

    // if the down flag is set
    if( TouchSampleDown(Flags) )
    {
        tNew = GetTickCount();

        if ( g_nDoPddPerformTest == TRUE )
        {
            if ( g_pddPerfData.dwStartTime == 0 )
            {
                g_pddPerfData.dwStartTime = tNew;
                g_pddPerfData.dwSamples   = 1;
            }
            else
            {
                ++g_pddPerfData.dwSamples;
                g_pddPerfData.dwEndTime = tNew;

               if ( g_pddPerfData.dwSamples >= g_dwPddPerfSampleSize )
               {
                   g_nDoPddPerformTest = FALSE;
               }
            }
        }
        if( fTipDownPrev )
        {
            deltaX = X - XPrev;
            deltaY = Y - YPrev;

            //
            // are the samples too close together?
            //
            if( (ABS_UI(deltaX) < MIN_MOVE) &&
                (ABS_UI(deltaY) < MIN_MOVE) &&
                ((tNew - tPrev) < MIN_TIME) )
            {
                tPrev = tNew;
                goto Exit;
            }
        }
        else
        {
            tpd.dwTouchEventFlags |= PEN_WENT_DOWN;
        }

        XPrev = X;
        YPrev = Y;
        fTipDownPrev = TRUE;
        tPrev = tNew;
    }
    else
    {   tpd.dwTouchEventFlags |= PEN_WENT_UP;
        fTipDownPrev = FALSE;
    }

    tpd.dwTouchEventFlags |= PEN_MOVE;
    tpd.flags = Flags;
    tpd.x = X / TOUCH_SCALE;
    tpd.y = Y / TOUCH_SCALE;

    if( !(g_touchPointQueue.Enqueue( tpd )) )
    {
        // queue must be full, send a warning
        DETAIL("Touch message queue full! Data point will be lost!");
        fRet = FALSE;
    }

Exit:
    return fRet;
}

// --------------------------------------------------------------------
//
//  loads the touch dll (in case it is not already loaded) and sets the
//  global function pointers to the dll exports
//
// --------------------------------------------------------------------
BOOL LoadTouchDriver( )
{
    BOOL fRet = TRUE;

    // g_szTouchDriver can be any of these three names:
    // TOUCH.DLL - pre-7 OS
    // TCHPROXY.DLL - 7 OS
    // Any name that user set through -v command line option


    if ( wcsncmp(TOUCH_DRIVER, g_szTouchDriver, sizeof(g_szTouchDriver)) != 0)
    {
        //do not change the name of touch driver if user sets it
    }
    else if (TouchDriverVersion() == CE6)
    {
         wcsncpy_s(g_szTouchDriver, TEXT("TOUCH.DLL"), sizeof(TEXT("TOUCH.DLL")));
    }

    g_hTouchDriver = LoadDriver( g_szTouchDriver );
    if( NULL == g_hTouchDriver )
    {
        g_pKato->Log( LOG_DETAIL,
            TEXT("Failed to load Touch Panel Driver: %s"), g_szTouchDriver );
        fRet = FALSE;
        goto Exit;
    }

    //
    // load the touch driver functions
    //
    g_pfnTouchPanelGetDeviceCaps =
        (PFN_TOUCH_PANEL_GET_DEVICE_CAPS)GetProcAddress(
            g_hTouchDriver, TEXT("TouchPanelGetDeviceCaps") );

    g_pfnTouchPanelSetMode =
        (PFN_TOUCH_PANEL_SET_MODE)GetProcAddress(
            g_hTouchDriver, TEXT("TouchPanelSetMode") );

    g_pfnTouchPanelSetCalibration =
        (PFN_TOUCH_PANEL_SET_CALIBRATION)GetProcAddress(
            g_hTouchDriver, TEXT("TouchPanelSetCalibration") );

    g_pfnTouchPanelCalibrateAPoint =
        (PFN_TOUCH_PANEL_CALIBRATE_A_POINT)GetProcAddress(
            g_hTouchDriver, TEXT("TouchPanelCalibrateAPoint") );

    g_pfnTouchPanelReadCalibrationPoint =
        (PFN_TOUCH_PANEL_READ_CALIBRATION_POINT)GetProcAddress(
            g_hTouchDriver, TEXT("TouchPanelReadCalibrationPoint") );

    g_pfnTouchPanelReadCalibrationAbort =
        (PFN_TOUCH_PANEL_READ_CALIBRATION_ABORT)GetProcAddress(
            g_hTouchDriver, TEXT("TouchPanelReadCalibrationAbort") );

    g_pfnTouchPanelEnableEx =
        (PFN_TOUCH_PANEL_ENABLE_EX)GetProcAddress(
            g_hTouchDriver, TEXT("TouchPanelEnableEx") );

    if (!g_pfnTouchPanelEnableEx)
    {
        g_pfnTouchPanelEnable =
        (PFN_TOUCH_PANEL_ENABLE)GetProcAddress(
            g_hTouchDriver, TEXT("TouchPanelEnable") );
    }

    g_pfnTouchPanelDisable =
        (PFN_TOUCH_PANEL_DISABLE)GetProcAddress(
            g_hTouchDriver, TEXT("TouchPanelDisable") );

    g_pfnTouchPanelPowerHandler =
        (PFN_TOUCH_PANEL_POWER_HANDLER)GetProcAddress(
            g_hTouchDriver, TEXT("TouchPanelPowerHandler") );

    g_pfnTouchPanelInitializeCursor =
        (PFN_TOUCH_PANEL_INITIALIZE_CURSOR)GetProcAddress(
            g_hTouchDriver, TEXT("TouchPanelInitializeCursor") );

Exit:
    return fRet;
}

//-----------------------------------------------------------------------------
//  Free the touch driver
//-----------------------------------------------------------------------------
VOID UnloadTouchDriver( )
{
    FreeLibrary( g_hTouchDriver );
}

//-----------------------------------------------------------------------------
// set up window
//-----------------------------------------------------------------------------
BOOL Initialize( VOID )
{
    BOOL fRet = TRUE;

    RECT rectWorking    = {0};
    WNDCLASS wc         = {0};
    LOGFONT lgFont      = {0};
    DWORD dwErr = 0;

    HANDLE hnd;
    int    nError;

    g_fShowCoordinates  = FALSE;
    g_dwDisplayWidth    = GetSystemMetrics(SM_CXSCREEN);
    g_dwDisplayHeight   = GetSystemMetrics(SM_CYSCREEN);

    g_rectWorking.top = 0;
    g_rectWorking.left = 0;
    g_rectWorking.bottom = g_dwDisplayHeight;
    g_rectWorking.right = g_dwDisplayWidth;

    g_rectTitle.top = g_dwDisplayHeight*9 / 10;
    g_rectTitle.left = 0;
    g_rectTitle.bottom = g_dwDisplayHeight ;
    g_rectTitle.right = g_dwDisplayWidth;

    g_rectMessage.top = g_dwDisplayHeight / 3;
    g_rectMessage.left = 0;
    g_rectMessage.bottom = 2 * g_dwDisplayHeight / 3;
    g_rectMessage.right = g_dwDisplayWidth;

    // initialize the crosshair
    SetCrosshair( g_dwDisplayWidth / 2, g_dwDisplayHeight / 2, FALSE );

    // clear message
    ClearMessage();

    if ((g_pfnTouchPanelEnable == NULL)&& (g_pfnTouchPanelEnableEx == NULL))
    {
        DETAIL("Driver does not support either TouchPanelEnableEx or TouchPanelEnable ");
        fRet = FALSE;
        goto Exit;
    }

    // Call TouchPanelDisable to clear callback before calling TouchPanelEnableEx
    (*g_pfnTouchPanelDisable)();


    if ( ( hnd = CreateFile(  TEXT( "TCH1:" ),
                              GENERIC_READ,
                              FILE_SHARE_READ,
                              NULL,
                              OPEN_EXISTING,
                              0,
                              NULL          ) ) == INVALID_HANDLE_VALUE )
    {
        nError = GetLastError();
        FAIL( "Error getting handle to Touch Panel" );
        g_pKato->Log( LOG_DETAIL, "Return code = %d", nError );

        goto Exit;

    }

     DeviceIoControl(hnd, IOCTL_TOUCH_ENABLE_TOUCHPANEL, NULL, 0, NULL, 0, NULL, NULL);


    if (g_pfnTouchPanelEnableEx)
    {
        if( !(*g_pfnTouchPanelEnableEx)( TouchPanelCallbackEx ) )
        {
            FAIL("Unable to use TouchPanelEnableEx");
            fRet = FALSE;
            goto Exit;
        }
    }
    else
    {
        if( !(*g_pfnTouchPanelEnable)( TouchPanelCallback ) )
        {
            FAIL("Unable to use Unable to use TouchPanelEnable");
            fRet = FALSE;
            goto Exit;
        }
    }

    // initialize the global message string
    _tcscpy_s( g_szTitle, TEXT("Starting Test...") );

    //
    // create and register window class
    //
    wc.lpfnWndProc      = (WNDPROC)WndProc;
    wc.hInstance        = g_hInstance;
    wc.hbrBackground    = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName    = WNDNAME;
    wc.style            = CS_HREDRAW | CS_VREDRAW | CS_NOCLOSE;

    szClass = (LPCTSTR) RegisterClass( &wc );

    if( !szClass && ((dwErr = GetLastError()) != 0))
    {
        FAIL("RegisterClass");
        SystemErrorMessage( GetLastError() );
        fRet = FALSE;
        goto Exit;
    }

    //
    // create a blank window that covers the screen and taskbar
    //
    g_hWnd = CreateWindowEx(
        WS_EX_TOPMOST,
        szClass,
        WNDNAME,
        WS_POPUP | WS_VISIBLE,
        0, 0,
        g_dwDisplayWidth,
        g_dwDisplayHeight,
        NULL, NULL, NULL, NULL );

    if( !g_hWnd )
    {
        FAIL("CreateWindow");
        SystemErrorMessage( GetLastError() );
        fRet = FALSE;
        goto Exit;
    }

    //
    // Display the window
    //
    ShowWindow( g_hWnd, SW_SHOW );
    SetFocus( g_hWnd );
    SetForegroundWindow( g_hWnd );

    //
    // Create a Pen
    //
    g_logPen.lopnStyle          = PS_SOLID;
    g_logPen.lopnWidth.x        = 2; // pen width
    g_logPen.lopnColor          = 0; // color = black

    //
    // Create Fonts
    //
    g_logFont.lfWidth           = g_dwDisplayWidth / countof(g_szTitle)+2;
    g_logFont.lfHeight          = 2 * g_logFont.lfWidth; // height = 2 * width
    g_logFont.lfWeight          = FW_MEDIUM;
    g_logFont.lfCharSet         = DEFAULT_CHARSET;
    g_logFont.lfQuality         = DEFAULT_QUALITY;
    g_logFont.lfPitchAndFamily  = FIXED_PITCH | FF_DONTCARE;

    g_msgFont.lfWidth           = g_dwDisplayWidth / countof(g_szMessage)+4;
    g_msgFont.lfHeight          = 2 * g_logFont.lfWidth; // height = 2 * width
    g_msgFont.lfWeight          = FW_BOLD;
    g_msgFont.lfCharSet         = DEFAULT_CHARSET;
    g_msgFont.lfQuality         = DEFAULT_QUALITY;
    g_msgFont.lfPitchAndFamily  = FF_DONTCARE;

    g_hCrosshairPen = CreatePenIndirect( &g_logPen );
    g_hTitleFont = CreateFontIndirect( &g_logFont );
    g_hMessageFont = CreateFontIndirect( &g_msgFont );

    DETAIL("INIT COMPLETE");

    UpdateWindow( g_hWnd );


Exit:
    return fRet;
}

//-----------------------------------------------------------------------------
VOID Deinitialize( )
{
    DestroyWindow( g_hWnd );
    UnregisterClass(szClass,NULL);
    (*g_pfnTouchPanelDisable)();

        // we should disable the touch panel
        // but due to the touch driver structure, we assume that enable
        // has been called before we run, so if we disable here,
        // we cannot rerun the test without a reboot.
}

//-----------------------------------------------------------------------------
// Prompts the user for a choice; a touch in the right half of the screen
// will result in a FALSE return value, a touch in the left half of the screen
// will result in a TRUE return value. If no touch events are noticed during
// the timeout, then fDefaultReturnVal is returned instead.
// NOTE that the choice is based on where the pen is first touched, not where
// it is released.
//-----------------------------------------------------------------------------
BOOL
WaitForInput( DWORD dwTimeout)
{

    DWORD dwStartTime       = GetTickCount();
    BOOL fPenDown           = FALSE;
    BOOL fRet               = false;
    TOUCH_POINT_DATA tpd    = {0};

    Sleep( INPUT_DELAY_MS );
    PostMessage( MSG_YES_NO );
    g_touchPointQueue.Clear();
    do
    {
        if( !fPenDown )
        {
            // wait for pen to go down
            if( g_touchPointQueue.Dequeue( &tpd ) )
            {
                if( PEN_WENT_DOWN & tpd.dwTouchEventFlags )
                {
                    fPenDown = TRUE;
                    // check the coordinates to see if this
                    // is an affirmative or a negative
                    if( tpd.x  < (INT)(g_dwDisplayWidth / 2) )
                    {
                        // touch was on the left half
                        fRet = TRUE;
                    }
                    else
                    {
                        // touch was on the right half
                        fRet = FALSE;
                    }
                }
            }
        }

        else
        {
            // wait for pen to come back up
            if( g_touchPointQueue.Dequeue( &tpd ) )
            {
                if( PEN_WENT_UP & tpd.dwTouchEventFlags )
                {
                    goto Exit;
                }
            }
        }
    }
    while( ((GetTickCount() - dwStartTime) < dwTimeout) ||
            (INFINITE == dwTimeout) );

    if( !fPenDown )
        DETAIL( "User prompt timed out waiting for user input" );

Exit:
    ClearMessage();
    return fRet;
}

//  turns the drawing mode on or off
VOID
SetDrawPen(
    BOOL fDrawOn )
{
    g_fDrawingOn = fDrawOn;
}

//  draw a line from the point set byt SetLineFrom to the point specified
VOID DrawLineTo( POINT pt )
{
    g_ptLine[1] = pt;
    InvalidateRect( g_hWnd, & g_rectWorking, FALSE );
    UpdateWindow( g_hWnd );
}

// Sets the drawing start location for the next line
VOID
SetLineFrom(POINT pt)
{
    g_ptLine[0] = pt;
}

// clears the pen drawing from the screen
VOID
ClearDrawing( )
{
    InvalidateRect( g_hWnd, &g_rectWorking, TRUE );
    UpdateWindow( g_hWnd );
}

// move the crosshair to the desired location on the screen
VOID
SetCrosshair(
    DWORD x,
    DWORD y,
    BOOL fVisible )
{
    g_dwCrosshairX = x;
    g_dwCrosshairY = y;
    g_fShowCrosshair = fVisible;

    if( g_fShowCoordinates )
    {
        PostMessage(TEXT("(%d, %d)"), x, y );
    }

    // invalidate, erase, and refresh the entire window
    InvalidateRect( g_hWnd, &g_rectWorking, TRUE );
    UpdateWindow( g_hWnd );
}

//-----------------------------------------------------------------------------
//  draws a crosshair at the specified X, Y coordinate given device context
//  and radius in pixels. Returns TRUE on success, FALSE otherwise.
//-----------------------------------------------------------------------------

BOOL
DrawCrosshair(
    HDC hdc,
    DWORD dwCrosshairX,
    DWORD dwCrosshairY,
    DWORD dwRadius )
{
    if( NULL == hdc )
    {
        return FALSE;
    }

    POINT lineHoriz[2] =
    {
        dwCrosshairX - dwRadius,
        dwCrosshairY,
        dwCrosshairX + dwRadius,
        dwCrosshairY
    };

    POINT lineVert[2] =
    {
        dwCrosshairX,
        dwCrosshairY - dwRadius,
        dwCrosshairX,
        dwCrosshairY + dwRadius
    };

    Polyline( hdc, lineHoriz, 2 );
    Polyline( hdc, lineVert, 2 );

    return TRUE;
}


VOID HideCrosshair( )
{
    SetCrosshair( 0, 0, FALSE );
}

// display title message on the main window
VOID PostTitle( LPCTSTR szFormat, ...)
{
    va_list pArgs;
    va_start(pArgs, szFormat);
    _vsntprintf_s(g_szTitle, countof(g_szTitle), szFormat, pArgs);
    va_end(pArgs);

    // invalidate, erase, and refresh the entire window
    InvalidateRect( g_hWnd, &g_rectTitle, TRUE );
    UpdateWindow( g_hWnd );
}

// clears the title
VOID ClearTitle( )
{
    PostTitle( TEXT("") );
}

// display text message on the main window
VOID PostMessage(LPCTSTR szFormat, ...)
{
    va_list pArgs;
    va_start(pArgs, szFormat);
    _vsntprintf_s(g_szMessage, countof(g_szMessage), szFormat, pArgs);
    va_end(pArgs);

    // invalidate, erase, and refresh the entire window
    InvalidateRect( g_hWnd, &g_rectMessage, TRUE );
    UpdateWindow( g_hWnd );
}

// clears the text message from the middle of the screen
VOID ClearMessage( )
{
    PostMessage( TEXT("") );
}

//-----------------------------------------------------------------------------
//Runs through the calibration algorithm if the device is not already
//calibrated. If fForceCalibrate is set to TRUE, then calibration will be
//forced even if the sceen is already calibrated.
//Returns TRUE if calibration is successfull, FALSE if it fails.
//-----------------------------------------------------------------------------

BOOL CalibrateScreen( BOOL fForceCalibrate )
{
    DWORD dwTime                            = 0;
    BOOL fRet                               = TRUE;
    BOOL fDone                              = FALSE;
    UINT cCalibPoints                       = 0;
    UINT cPoint                             = 0;
    HANDLE hThread                          = NULL;
    INT32 *nXActual                         = NULL;
    INT32 *nYActual                         = NULL;
    INT32 *nXRaw                            = NULL;
    INT32 *nYRaw                            = NULL;
    TOUCH_POINT_DATA tpd                    = {0};
    ABORT_THREAD_DATA atd                   = {0};
    TPDC_CALIBRATION_POINT cCalibData       = {0};
    TPDC_CALIBRATION_POINT_COUNT cPointData = {0};

    int nError;

    //
    // if the screen is already calibrated, return
    //
    if( g_fCalibrated && !fForceCalibrate)
    {
        fRet = TRUE;
        goto Leave;
    }

    //
    // make sure the TouchPanelReadCalibrationPoint function was loaded
    //
    if( NULL == g_pfnTouchPanelReadCalibrationPoint )
    {
        DETAIL("Driver does not support TouchPanelReadCalibrationPoint");
        fRet = FALSE;
        goto Leave;
    }

    //
    // make sure the TouchPanelReadCalibrationAbort function was loaded
    //
    if( NULL == g_pfnTouchPanelReadCalibrationAbort )
    {
        DETAIL("Driver does not support TouchPanelReadCalibrationAbort");
        fRet = FALSE;
        goto Leave;
    }

    //
    // make sure the TouchPanelGetDeviceCaps function was loaded
    //
    if( NULL == g_pfnTouchPanelGetDeviceCaps )
    {
        DETAIL("Driver does not support TouchPanelGetDeviceCaps");
        fRet = FALSE;
        goto Leave;
    }

    //
    // make sure the TouchPanelSetCalibration function was loaded
    //
    if( NULL == g_pfnTouchPanelSetCalibration )
    {
        DETAIL("Driver does not support TouchPanelSetCalibration");
        fRet = FALSE;
        goto Leave;
    }

    //
    // query the calibration point count
    //
    if( !(*g_pfnTouchPanelGetDeviceCaps)( TPDC_CALIBRATION_POINT_COUNT_ID, &cPointData ) )
    {
        FAIL("TouchPanelGetDeviceCaps( TPDC_CALIBRATION_POINT_COUNT_ID )");
        fRet = FALSE;
        goto Exit;
    }

    cCalibPoints = cPointData.cCalibrationPoints;

    //
    // allocate buffers
    //
    nXActual = new INT32[cCalibPoints];
    nYActual = new INT32[cCalibPoints];
    nXRaw = new INT32[cCalibPoints];
    nYRaw = new INT32[cCalibPoints];

    if( NULL == nXActual ||
        NULL == nYActual ||
        NULL == nXRaw ||
        NULL == nYRaw )
    {
        FAIL("Unable to allocate point buffers");
        fRet = FALSE;
        goto Exit;
    }

    //
    // keep calibrating until it works or user says it's okay
    //
    do
    {

        // query every calibration point
        for( cPoint = 0; cPoint < cCalibPoints; cPoint++ )
        {
            // get the location of the point to calibrate
            cCalibData.PointNumber = cPoint;
            cCalibData.cDisplayWidth = g_dwDisplayWidth;
            cCalibData.cDisplayHeight = g_dwDisplayHeight;
            if( !(*g_pfnTouchPanelGetDeviceCaps)( TPDC_CALIBRATION_POINT_ID, &cCalibData ) )
            {
                // Not supported on an Emulator
                if ( ( nError = GetLastError() ) == 50 )
                {
                    fRet = EOF;
                    goto Exit;
                }

                FAIL("TouchPanelGetDeviceCaps( TPDC_CALIBRATION_POINT_ID )");
                g_pKato->Log( LOG_DETAIL, TEXT( "Error code = %ld" ), nError );
                fRet = FALSE;
                goto Exit;
            }
            // store the point
            nXActual[cPoint] = cCalibData.CalibrationX;
            nYActual[cPoint] = cCalibData.CalibrationY;

            // update crosshair position
            SetCrosshair( cCalibData.CalibrationX, cCalibData.CalibrationY, TRUE );


            PostMessage( TEXT("Touch Calibration Point %d"), cPoint + 1 );
            PostTitle( TEXT("Touch Crosshairs as they appear") );

            // create a kill thread
            atd.dwCountdownMs = 20000; //abort calibration if user does not touch the screen withing 20 seconds
            atd.fTerminate = FALSE;

            hThread = CreateThread( NULL, NULL, CalibrationAbortThreadProc, &atd, 0, NULL );
            if( NULL == hThread )
            {
                FAIL("CreateThread");
                fRet = FALSE;
                goto Exit;
            }

            //
            // calibrate the point
            //
            if( !(*g_pfnTouchPanelReadCalibrationPoint)( &(nXRaw[cPoint]), &(nYRaw[cPoint]) ) )
            {
                FAIL("TouchPanelReadCalibrationPoint() returned error while attempting to read user input");
                fRet = FALSE;
                goto Exit;
            }

            //
            // signal thread to stop and wait for it to finish
            //
            atd.fTerminate = TRUE;
            if( WAIT_OBJECT_0 != WaitForSingleObject( hThread, INFINITE ) )
            {
                FAIL("WaitForSingleObject");
                fRet = FALSE;
                goto Exit;
            }
            hThread = NULL;
        }

        HideCrosshair( );
        //
        // attempt to set the calibration points
        //
        if( !(*g_pfnTouchPanelSetCalibration)( cCalibPoints, nXActual, nYActual, nXRaw, nYRaw ) )
        {
            //
            // calibration failed
            //
            DETAIL("WARNING: TouchPanelSetCalibration failed; this could be due to noisy hardware");
            PostTitle( TEXT("Failed to Set Calibration\r\nDo you want try again?") );

            fDone = !WaitForInput( USER_WAIT_TIME);
        }

        else
        {
            //
            // calibration succeeded
            //
            fDone = TRUE;
        }
    }
    while( !fDone );

Exit:
    HideCrosshair( );
    ClearMessage( );

    // free buffers
    if( nXActual )  delete[] nXActual;
    if( nYActual )  delete[] nYActual;
    if( nXRaw )     delete[] nXRaw;
    if( nYRaw )     delete[] nYRaw;

    // kill thread
    if( NULL != hThread )
    {
        // if the thread wasn't killed before, kill it now
        TerminateThread( hThread, CALIBRATION_NOT_ABORTED );
    }

    // post results
    if( fRet )
    {
        g_fCalibrated = TRUE;
        PostTitle(
            TEXT("Calibration Succeeded") );
    }
    else
    {
        g_fCalibrated = FALSE;
        PostTitle(
            TEXT("Calibration Failed") );
    }
    
Leave:
    return fRet;
}

//-----------------------------------------------------------------------------
//  A simple thread routine that sleeps for the specified amount of time and
//  then calls TouchPanelReadCalibrationAbort().
//-----------------------------------------------------------------------------
DWORD
WINAPI CalibrationAbortThreadProc(LPVOID lpParam )    // ABORT_THREAD_DATA
{
    LPABORT_THREAD_DATA pData = (LPABORT_THREAD_DATA)lpParam;
    DWORD dwStartTime = GetTickCount();

    do
    {   //
        // sleep for 10 ms
        //
        Sleep( 100 );

        //
        // check for early termination on wake
        //
        if( pData->fTerminate )
        {
            //
            // calibration must have succeeded, termination was signaled so
            // exit without aboring the calibration
            //
            ExitThread( CALIBRATION_NOT_ABORTED );
        }
    }
    //
    // Loop while the elapsed time is less than the allotted time
    //
    while( pData->dwCountdownMs > (GetTickCount() - dwStartTime) );

    //
    // Timed out, abort the calibration attempt (this will allow the main
    // test thread to continue because the calibration call will return)
    //
    DETAIL("Calling TouchPanelReadCalibrationAbort()");
    g_pfnTouchPanelReadCalibrationAbort( );

    ExitThread( CALIBRATION_ABORTED );
    return 1;
}

//-----------------------------------------------------------------------------
// InitTouchWindow:  Initializes the test window used by some tests and loads the
//                   touch panel call back function
// ARGUMENTS:
//  pszDecription:  test dewscription
//
// RETURNS:
//  TRUE:   If successfull
//  FALSE:  If otherwise
//-----------------------------------------------------------------------------
BOOL InitTouchWindow( TCHAR *pszDecription )
{
    if ( !Initialize() )
    {
        FAIL("Failed to initialize");
        return FALSE;
    }



    g_hThread = CreateThread( NULL,
                              0,
                              WinThread,
                              0,
                              0,
                              &g_dwThreadId );

    if( g_hThread == NULL )
    {
         FAIL("Failed to initialize");
         return SPR_FAIL;
    }

        g_pKato->Log( LOG_DETAIL, TEXT( "Created thread %d (%x)" ), g_dwThreadId, g_dwThreadId );

    PostTitle( TEXT("%s"), pszDecription );

    Sleep( INPUT_DELAY_MS );
    g_touchPointQueue.Clear();
    ClearMessage();

    return TRUE;
}


//-----------------------------------------------------------------------------
// InitTouchWindow:  Removes the main test window
// RETURNS:
//  Nothing
//-----------------------------------------------------------------------------
void DeinitTouchWindow( TCHAR *pszDescription, DWORD dwResult )
{
    PostTitle( TEXT("%s : %s"),
               pszDescription,
               dwResult == TPR_SKIP ? TEXT("SKIPPED") :
               dwResult == TPR_PASS ? TEXT("PASSED") :
               dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED") );

    Sleep( INPUT_DELAY_MS );
    g_touchPointQueue.Clear();
    ClearMessage();

    if ( g_hThread != NULL )
    {
        Deinitialize();

        g_bKillWinThread = TRUE;
        WaitForSingleObject( g_hThread, INFINITE );
        CloseHandle( g_hThread );

    }

    g_hThread = NULL;

    // Delete all gdi objects
    DeleteObject(g_hCrosshairPen);
    g_hCrosshairPen = NULL;
    DeleteObject(g_hTitleFont);
    g_hTitleFont = NULL;
    DeleteObject(g_hMessageFont);
    g_hMessageFont = NULL;
}

//-----------------------------------------------------------------------------
// WinThread:  The entry point for the test window used by some of the
// Touch Panel Tests
//-----------------------------------------------------------------------------
DWORD WINAPI WinThread( LPVOID lpParam )
{
        MSG msg;

        g_bKillWinThread = FALSE;

        while ( g_bKillWinThread  == FALSE )
        {
            if ( PeekMessage( &msg, g_hWnd, 0, 0, PM_REMOVE ) != 0 )
            {
                TranslateMessage( &msg );
                DispatchMessage( &msg );
            }

        }


        return 0;
}

//-----------------------------------------------------------------------------
//  DoPddPerformTest: Sets the PDD Performance Test flag
//
// ARGUMENTS:
//      dwPnts: samples to collect
//-----------------------------------------------------------------------------
void DoPddPerformTest( DWORD dwPnts )
{
    g_dwPddPerfSampleSize = dwPnts > MAX_NUM_SAMPLES ? MAX_NUM_SAMPLES : dwPnts;
    memset( &g_pddPerfData, 0, sizeof( PDD_PERF_DATA ) );
    g_nDoPddPerformTest = TRUE;
}

//-----------------------------------------------------------------------------
 // GetPddPerformTestStatus:  Returns PDD Performance Test flag
 //
 // ARGUMENTS:
 //  pPDD:   Pointer to object to wrute results
 //
 // RETURNS:
 //  TRUE:  Test is running
 //  FALSE: Test is over or not running
 //  EOF:   Error has occurred
//-----------------------------------------------------------------------------
int GetPddPerformTestStatus( PDD_PERF_DATA *pPDD )
{
   memcpy( pPDD, &g_pddPerfData, sizeof( PDD_PERF_DATA ) );
   return g_nDoPddPerformTest;
}


//------------------------------------------------------------------------------
// TouchDriverVersion: Detect touch driver version in the device
// This function detects both old (pre 7) and new touch drivers
// Return: 0 - Touch driver is not detected
//             6 - legacy driver is detected
//             7 - Seven driver is detected
//------------------------------------------------------------------------------//

int TouchDriverVersion()
 {
     HANDLE hndFindDrvr;
     DEVMGR_DEVICE_INFORMATION ddiTouchDriver;
     int driverVersion = NO_TOUCH;

     // To detect 7 touch driver, check the DeviceClass Id
     GUID touchGUID = { 0x25121442, 0x8ca, 0x48dd, { 0x91, 0xcc, 0xbf, 0xc9, 0xf7, 0x90, 0x2, 0x7c } };

     memset( &ddiTouchDriver, 0, sizeof( DEVMGR_DEVICE_INFORMATION ) );
     ddiTouchDriver.dwSize = sizeof( DEVMGR_DEVICE_INFORMATION );

     hndFindDrvr = FindFirstDevice( DeviceSearchByGuid,&touchGUID,&ddiTouchDriver );
     FindClose( hndFindDrvr );
     if ( hndFindDrvr != INVALID_HANDLE_VALUE )
     {
         driverVersion = CE7;
     }
     else
     {
        // Can not detect 7 touch driver, check legacy touch driver
        LONG   lStatus;
        HKEY   hKey;

        lStatus = RegOpenKeyEx( HKEY_LOCAL_MACHINE               ,
                                TEXT( "HARDWARE\\DEVICEMAP\\TOUCH" ),
                                0,
                                KEY_QUERY_VALUE,
                                &hKey );
        RegCloseKey( hKey );
        if ( lStatus == ERROR_SUCCESS )
        {
           driverVersion = CE6;
        }
     }
     return driverVersion;
}

// --------------------------------------------------------------------
INT
WinMainGetOpt(
    LPCTSTR szCmdLine,
    LPCTSTR szOptions )
{
    static LPCTSTR pCurPos = szCmdLine;
    LPCTSTR pCurOpt = szOptions;
    LPTSTR pOptArg = optArg;
    UINT cOptArg = 0;
    INT option = 0;
    TCHAR quoteChar = TCHAR(' ');

    // end of string reached, or NULL string
    if( NULL == pCurPos || END_OF_STRING == *pCurPos )
    {
        return EOF;
    }

    // eat leading white space
    while( _T(' ') == *pCurPos )
        pCurPos++;

    // trailing white space
    if( END_OF_STRING == *pCurPos )
    {
       return EOF;
    }

    // found an option
    if( ARG_FLAG != *pCurPos )
    {
        return BAD_ARG;
    }

    pCurPos++;

    // found - at end of string
    if( END_OF_STRING == *pCurPos )
    {
        return BAD_ARG;
    }

    // search all options requested
    for( pCurOpt = szOptions; *pCurOpt != END_OF_STRING; pCurOpt++ )
    {
        // found matching option
        if( *pCurOpt == *pCurPos )
        {
            option = (int)*pCurPos;

            pCurOpt++;
            pCurPos++;
            if( OPT_ARG == *pCurOpt )
            {
                // next argument is the arg value
                // look for something between quotes or whitespace
                while( _T(' ') == *pCurPos )
                    pCurPos++;

                if( END_OF_STRING == *pCurPos )
                {
                    return BAD_ARG;
                }

                if( _T('\"') == *pCurPos )
                {
                    // arg value is between quotes
                    quoteChar = *pCurPos;
                    pCurPos++;
                }

                else if( ARG_FLAG == *pCurPos )
                {
                    return BAD_ARG;
                }

                else
                {
                    // arg end at next whitespace
                    quoteChar = _T(' ');
                }

                pOptArg = optArg;
                cOptArg = 0;

                // TODO: handle embedded, escaped quotes
                while( quoteChar != *pCurPos && END_OF_STRING != *pCurPos  && cOptArg < MAX_OPT_ARG )
                {
                    *pOptArg++ = *pCurPos++;
                    cOptArg++;
                }

                // missing final quote
                if( _T(' ') != quoteChar && quoteChar != *pCurPos )
                {
                    return BAD_ARG;
                }

                // append NULL char to output string
                *pOptArg = END_OF_STRING;

                // if there was no argument value, fail
                if( 0 == cOptArg )
                {
                    return BAD_ARG;
                }
            }

            return option;
        }
    }
    pCurPos++;
    // did not find a macthing option in list
    return BAD_ARG;
}

