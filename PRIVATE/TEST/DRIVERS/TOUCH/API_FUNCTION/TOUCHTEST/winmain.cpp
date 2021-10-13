// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
// Copyright (c) 2000 Microsoft Corporation.  All rights reserved.
//                                                                     
// --------------------------------------------------------------------

#include "globals.h"

#define __FILE_NAME__   TEXT("WINMAIN.CPP")

HINSTANCE	g_hInstance = NULL;
TCHAR g_szTouchDriver[MAX_TOUCHDRV_NAMELEN] = TOUCH_DRIVER;

static HWND 		g_hWnd              = NULL;
static HDC			g_hDC               = NULL;
static HFONT		g_hFont             = NULL;
static HINSTANCE    g_hTouchDriver      = NULL;

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
static TCHAR        g_szMessage[50]     = TEXT("");

//
// global function pointers to the touch driver DLL functions
//
PFN_TOUCH_PANEL_GET_DEVICE_CAPS			g_pfnTouchPanelGetDeviceCaps        = NULL;
PFN_TOUCH_PANEL_SET_MODE				g_pfnTouchPanelSetMode              = NULL;
PFN_TOUCH_PANEL_SET_CALIBRATION			g_pfnTouchPanelSetCalibration       = NULL;
PFN_TOUCH_PANEL_CALIBRATE_A_POINT		g_pfnTouchPanelCalibrateAPoint      = NULL;
PFN_TOUCH_PANEL_READ_CALIBRATION_POINT	g_pfnTouchPanelReadCalibrationPoint = NULL;
PFN_TOUCH_PANEL_READ_CALIBRATION_ABORT	g_pfnTouchPanelReadCalibrationAbort = NULL;
PFN_TOUCH_PANEL_ENABLE					g_pfnTouchPanelEnable               = NULL;
PFN_TOUCH_PANEL_DISABLE                 g_pfnTouchPanelDisable              = NULL;
PFN_TOUCH_PANEL_POWER_HANDLER			g_pfnTouchPanelPowerHandler         = NULL;
PFN_TOUCH_PANEL_INITIALIZE_CURSOR		g_pfnTouchPanelInitializeCursor     = NULL;

// --------------------------------------------------------------------
LRESULT CALLBACK
WndProc( 
    HWND hWnd, 
    UINT message, 
    WPARAM wParam, 
    LPARAM lParam )
//
//  Main window procedure
//
// --------------------------------------------------------------------   
{
    static PAINTSTRUCT ps;
    static HFONT hTitleFont;
    static HFONT hMessageFont;    
    static HPEN hCrosshairPen;    
    
    HPEN hOldPen;
    HFONT hOldFont;
    HDC hdc;
        
    switch( message )
    {   

    case WM_PAINT:
    
        hdc = BeginPaint( hWnd, &ps );                

        hCrosshairPen = CreatePenIndirect( &g_logPen );
        hTitleFont = CreateFontIndirect( &g_logFont );
        hMessageFont = CreateFontIndirect( &g_msgFont );

        //FillRect( hdc, &g_rectWorking, (HBRUSH)GetStockObject(WHITE_BRUSH) );

        // draw title
        hOldFont = (HFONT) SelectObject( hdc, hTitleFont );
        DrawText( hdc, g_szTitle, -1, &g_rectTitle, DT_CENTER );

        // draw message
        SelectObject( hdc, hMessageFont );
        DrawText( hdc, g_szMessage, -1, &g_rectMessage, DT_CENTER );

        // reset font 
        SelectObject( hdc, hOldFont );
        
        // draw crosshair
        hOldPen = (HPEN) SelectObject( hdc, hCrosshairPen );       
        if( g_fShowCrosshair )
        {
            DrawCrosshair( hdc, g_dwCrosshairX, g_dwCrosshairY, 15 );
        }

        if( g_fDrawingOn )
        {
            Polyline( hdc, g_ptLine, 2 );
            SetLineFrom( g_ptLine[1] );
        }
        
        // reset pen
        SelectObject( hdc, hOldPen );
        
        EndPaint( hWnd, &ps );
        
        return 0;
        
    case WM_SETFOCUS:
    
        InvalidateRect( g_hWnd, &g_rectWorking, 0 );
        UpdateWindow( g_hWnd );
        return 0;

    case WM_DESTROY:

        PostQuitMessage(0);
        return 0;

        
    }
    
    return DefWindowProc( hWnd, message, wParam, lParam );
}

// --------------------------------------------------------------------
BOOL 
TouchPanelCallback(
    TOUCH_PANEL_SAMPLE_FLAGS    Flags,
    INT                         X,
    INT                         Y )
//
//  Callback routine for the touch panel calibration test. This function is of
//  type PFN_TOUCH_PANEL_CALLBACK defined in tchddi.h.
//
//  This function is basically here to be a placeholder. It does nothing with
//  the calibration points when it is called.
//
// --------------------------------------------------------------------
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

    // first make sure the point is calibrated
    if( !(Flags & TouchSampleIsCalibratedFlag) )
    {
        DETAIL("Uncalibrated touch point received");

        if( g_pfnTouchPanelCalibrateAPoint )
        {
            // TODO: add calibration stuff

            // Flags |= TouchSampleIsCalibratedFlag
        }        
    }    

    //
    // if the down flag is set
    //
    if( TouchSampleDown(Flags) )
    {
        tNew = GetTickCount();

        //
        // if the previous sample was also down
        //
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

        //
        // first pen down
        //
        else
        {
            // first touch!
            if( TouchSamplePreviousDown(Flags) )
            {
                DETAIL("TouchSamplePreviousDownFlag set on pen down!");
            }
            tpd.dwTouchEventFlags |= PEN_WENT_DOWN;
        }
        
        XPrev = X;
        YPrev = Y;
        fTipDownPrev = TRUE;
        tPrev = tNew;
    }

    //
    // the down flag is not set
    //
    else
    {        
        if( !fTipDownPrev )
        {
            // repeat up event... sometimes happens
            goto Exit;
        }

        // flag checking
        if( !TouchSamplePreviousDown(Flags) )
        {
            DETAIL("TouchSamplePreviousDownFlag not set on pen up!");
        }
        
        tpd.dwTouchEventFlags |= PEN_WENT_UP;
        fTipDownPrev = FALSE;                    
    }

    tpd.dwTouchEventFlags |= PEN_MOVE;
    tpd.flags = Flags;
    tpd.x = X / TOUCH_SCALE;
    tpd.y = Y / TOUCH_SCALE;
    
    if( !(g_touchPointQueue.Enqueue( tpd )) )
    {
        //
        // queue must be full, send a warning
        //
        DETAIL("Touch message queue full! Data point will be lost!");
        fRet = FALSE;
    }
    
Exit:
    return fRet;      
}

// --------------------------------------------------------------------
BOOL
LoadTouchDriver( )
//
//  loads the touch dll (in case it is not already loaded) and sets the
//  global function pointers to the dll exports
//
// --------------------------------------------------------------------
{
    BOOL fRet = TRUE;

    //
    // Load the touch driver dll
    //
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

     g_pfnTouchPanelEnable =
        (PFN_TOUCH_PANEL_ENABLE)GetProcAddress(
            g_hTouchDriver, TEXT("TouchPanelEnable") );

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

// --------------------------------------------------------------------
VOID 
UnloadTouchDriver( )
//
//  Free the touch driver
//
// --------------------------------------------------------------------
{
    FreeLibrary( g_hTouchDriver );
}

// --------------------------------------------------------------------
BOOL 
Initialize( VOID )
//
// setup window
//
// --------------------------------------------------------------------
{
    BOOL fRet = TRUE;
    
    RECT rectWorking    = {0};
    WNDCLASS wc         = {0};    
    LOGFONT lgFont      = {0};
    LPCTSTR szClass;

    g_fCalibrated       = FALSE;
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


    //
    // make sure the TouchPanelGetDeviceCaps function was loaded
    //
    if( NULL == g_pfnTouchPanelEnable )
    {
        DETAIL("Driver does not support TouchPanelEnable");
        fRet = FALSE;
        goto Exit;
    }

    //
    // enable the touch driver
    //    
    if( !(*g_pfnTouchPanelEnable)( TouchPanelCallback ) )
    {
        // unable to enable the device
        FAIL("Unable to enable the touch panel device");
        fRet = FALSE;
        goto Exit;        
    }        


    // initialize the global message string
    _tcscpy( g_szTitle, TEXT("Starting Test...") );

    //
    // create and register window class
    //
    wc.lpfnWndProc      = (WNDPROC)WndProc;
    wc.hInstance        = g_hInstance;
    wc.hbrBackground    = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName    = WNDNAME;
    wc.style            = CS_HREDRAW | CS_VREDRAW | CS_NOCLOSE;    

    szClass = (LPCTSTR) RegisterClass( &wc );    
    
    if( !szClass )
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
    UpdateWindow( g_hWnd );
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
    g_logFont.lfWidth           = g_dwDisplayWidth / countof(g_szTitle);
    g_logFont.lfHeight          = 2 * g_logFont.lfWidth; // height = 2 * width
    g_logFont.lfWeight          = FW_MEDIUM;
    g_logFont.lfCharSet         = DEFAULT_CHARSET;
    g_logFont.lfQuality         = DEFAULT_QUALITY;
    g_logFont.lfPitchAndFamily  = FIXED_PITCH | FF_DONTCARE;

    g_msgFont.lfWidth           = g_dwDisplayWidth / countof(g_szMessage);
    g_msgFont.lfHeight          = 2 * g_logFont.lfWidth; // height = 2 * width
    g_msgFont.lfWeight          = FW_BOLD;
    g_msgFont.lfCharSet         = DEFAULT_CHARSET;
    g_msgFont.lfQuality         = DEFAULT_QUALITY;
    g_msgFont.lfPitchAndFamily  = FF_DONTCARE;
        
    DETAIL("INIT COMPLETE");
    
Exit:
    return fRet;
}  

// --------------------------------------------------------------------
VOID 
Deinitialize( )
//
//  Misc cleanup
//
// --------------------------------------------------------------------
{    

    //
    // destroy the main window
    //
    DestroyWindow( g_hWnd );

    //
    // disable the touch panel
    //

#if 0 
    //   
    // this command will indirectly cause exceptions in the ODO touch
    // driver that cannot be caught, so it is not called
    //   
    (*g_pfnTouchPanelDisable)( );    
#endif    
}

// --------------------------------------------------------------------
BOOL
WaitForClick( 
    DWORD dwTimeout )
//
//  Waits until the timeout is reached or the user taps the screen. 
//  If the timeout is reached first, FALSE is returned. If the tap occurs
//  first, TRUE is returned.
//
// --------------------------------------------------------------------
{
    DWORD dwStartTime       = GetTickCount();
    BOOL fPenDown           = FALSE;
    BOOL fRet               = FALSE;
    TOUCH_POINT_DATA tpd    = {0};

    Sleep( INPUT_DELAY_MS );    
    g_touchPointQueue.Clear();

    PostMessage( MSG_TAP_SCREEN );
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
                    fRet = TRUE;
                    goto Exit;
                }
            }
        }
    }
    while( ((GetTickCount() - dwStartTime) < dwTimeout) ||
            (INFINITE == dwTimeout) );

Exit:
    ClearMessage();
    return fRet;
}

// --------------------------------------------------------------------
BOOL
WaitForInput( 
    DWORD dwTimeout,
    BOOL fDefaultReturnVal )
//
//  Prompts the user for a choice; a touch in the right half of the screen
//  will result in a FALSE return value, a touch in the left half of the screen
//  will result in a TRUE return value. If no touch events are noticed during
//  the timeout, then fDefaultReturnVal is returned instead.
//
//  NOTE that the choice is based on where the pen is first touched, not where
//  it is released.
//
// --------------------------------------------------------------------
{
    
    DWORD dwStartTime       = GetTickCount();
    BOOL fPenDown           = FALSE;
    BOOL fRet               = fDefaultReturnVal;
    TOUCH_POINT_DATA tpd    = {0};

    // Doesn't work on uncalibrated screens...
    //CalibrateScreen( FALSE );
    
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

Exit:
    ClearMessage();
    return fRet;
}

// --------------------------------------------------------------------
VOID
SetDrawPen( 
    BOOL fDrawOn )
//
//  turns the drawing mode on or off
//
// --------------------------------------------------------------------    
{
    g_fDrawingOn = fDrawOn;
}

// --------------------------------------------------------------------
VOID
DrawLineTo(
    POINT pt )
//
//  draw a line from the point set byt SetLineFrom to the point 
//  specified
//
// --------------------------------------------------------------------    
{
    g_ptLine[1] = pt;
    InvalidateRect( g_hWnd, & g_rectWorking, FALSE );
    UpdateWindow( g_hWnd );
}

// --------------------------------------------------------------------
VOID
SetLineFrom(
    POINT pt )
//
//  Sets the drawing start location for the next line
//
// --------------------------------------------------------------------    
{
    g_ptLine[0] = pt;
}

// --------------------------------------------------------------------
VOID
ClearDrawing( )
//
//  clears the pen drawing from the screen
//
// --------------------------------------------------------------------
{
    InvalidateRect( g_hWnd, &g_rectWorking, TRUE );
    UpdateWindow( g_hWnd );
}   

// --------------------------------------------------------------------    
VOID
SetCrosshair(
    DWORD x,
    DWORD y,
    BOOL fVisible )
//
//  move the crosshair to the desired location on the screen
//
// --------------------------------------------------------------------       
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

// --------------------------------------------------------------------
BOOL
DrawCrosshair( 
    HDC hdc, 
    DWORD dwCrosshairX, 
    DWORD dwCrosshairY,
    DWORD dwRadius )
//
//  draws a crosshair at the specified X, Y coordinate given device context 
//  and radius in pixels. Returns TRUE on success, FALSE otherwise.
//
// --------------------------------------------------------------------   
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
   
// --------------------------------------------------------------------  
VOID
HideCrosshair( )
//
//  hide the crosshair
//
// --------------------------------------------------------------------  
{
    SetCrosshair( 0, 0, FALSE );
}

// --------------------------------------------------------------------
VOID
PostTitle( 
    LPCTSTR szFormat, 
    ...) 
//
//  display title message on the main window
//
// --------------------------------------------------------------------        
{
    va_list pArgs; 
    va_start(pArgs, szFormat);
    _vsntprintf(g_szTitle, countof(g_szTitle), szFormat, pArgs);
    va_end(pArgs);

    // invalidate, erase, and refresh the entire window
    InvalidateRect( g_hWnd, &g_rectTitle, TRUE );
    UpdateWindow( g_hWnd );
}

// --------------------------------------------------------------------
VOID
ClearTitle( )
//
//  clears the title
//
// --------------------------------------------------------------------
{
    PostTitle( TEXT("") );
}

// --------------------------------------------------------------------
VOID
PostMessage( 
    LPCTSTR szFormat, 
    ...) 
//
//  display text message on the main window
//
// --------------------------------------------------------------------        
{
    va_list pArgs; 
    va_start(pArgs, szFormat);
    _vsntprintf(g_szMessage, countof(g_szTitle), szFormat, pArgs);
    va_end(pArgs);

    // invalidate, erase, and refresh the entire window
    InvalidateRect( g_hWnd, &g_rectMessage, TRUE );
    UpdateWindow( g_hWnd );
}

// --------------------------------------------------------------------
VOID
ClearMessage( )
//
//  clears the text message from the middle of the screen
//
// --------------------------------------------------------------------
{
    PostMessage( TEXT("") );
}

// --------------------------------------------------------------------    
BOOL
CalibrateScreen( 
    BOOL fForceCalibrate )
//
//  Runs through the calibration algorithm if the device is not already 
//  calibrated. If fForceCalibrate is set to TRUE, then calibration will be
//  forced even if the sceen is already calibrated.
//
//  Returns TRUE if calibration is successfull, FALSE if it fails.
//  
// --------------------------------------------------------------------    
{     
    DWORD dwTime                            = 0;
    BOOL fRet                               = TRUE;  
    BOOL fDone                              = FALSE;
    UINT cCalibPoints                       = 0;
    UINT cPoint                             = 0;
    HANDLE hThread                          = INVALID_HANDLE_VALUE;
    INT32 *nXActual                         = NULL;
    INT32 *nYActual                         = NULL;
    INT32 *nXRaw                            = NULL;
    INT32 *nYRaw                            = NULL;
    TOUCH_POINT_DATA tpd                    = {0}; 
    ABORT_THREAD_DATA atd                   = {0};
    TPDC_CALIBRATION_POINT cCalibData       = {0};
    TPDC_CALIBRATION_POINT_COUNT cPointData = {0};

    // 
    // if the screen is already calibrated, return
    //
    if( g_fCalibrated && !fForceCalibrate)
    {
        DETAIL("Screen already calibrated");
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
        PostTitle( TEXT("Touch Crosshairs as they appear") );
            
        //
        // query every calibration point
        //
        for( cPoint = 0; cPoint < cCalibPoints; cPoint++ )
        {
            PostMessage( TEXT("Touch Calibration Point %d"), cPoint + 1 );

            //
            // get the location of the point to calibrate
            //
            cCalibData.PointNumber = cPoint;
            cCalibData.cDisplayWidth = g_dwDisplayWidth;
            cCalibData.cDisplayHeight = g_dwDisplayHeight;
            if( !(*g_pfnTouchPanelGetDeviceCaps)( TPDC_CALIBRATION_POINT_ID, &cCalibData ) )
            {
                FAIL("TouchPanelGetDeviceCaps( TPDC_CALIBRATION_POINT_ID )");
                fRet = FALSE;
                goto Exit;
            }

            //
            // store the point
            //
            nXActual[cPoint] = cCalibData.CalibrationX;
            nYActual[cPoint] = cCalibData.CalibrationY;
            
            //
            // update crosshair position
            //
            SetCrosshair( cCalibData.CalibrationX, cCalibData.CalibrationY, TRUE );

            //
            // create a kill thread
            //
            atd.dwCountdownMs = 10000;
            atd.fTerminate = FALSE;

            hThread = CreateThread( NULL, NULL, CalibrationAbortThreadProc, &atd, 0, NULL );
            if( INVALID_HANDLE_VALUE == hThread )
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
                FAIL("TouchPanelReadCalibrationPoint( )");
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
            hThread = INVALID_HANDLE_VALUE;                 
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
            fDone = !WaitForInput( USER_WAIT_TIME, FALSE );
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
    if( INVALID_HANDLE_VALUE != hThread )
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
    WaitForClick( USER_WAIT_TIME );

Leave:        
    return fRet;
}

// --------------------------------------------------------------------
DWORD
WINAPI CalibrationAbortThreadProc(
    LPVOID lpParam )    // ABORT_THREAD_DATA
//
//  A simple thread routine that sleeps for the specified amount of time and 
//  then calls TouchPanelReadCalibrationAbort().
//
// --------------------------------------------------------------------
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

