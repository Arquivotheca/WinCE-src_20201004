//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
//
// TouchRecognizer.cpp
//  This is the standard Microsoft touch recognizer engine.
//
////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <GweMemory.hpp>
#include <guts.h>
#include "TouchRecognizer.h"
#include <ehm.h>
#include <touchperf.h>
#include <touchgesture.h>
#include <intsafe.h>
#include <TouchUtil.h>
#include <gesture_priv.h>

// Instantiate required CePerf globals
LPVOID pCePerfInternal = NULL;

// constants and globals that are solely for this CPP file
namespace 
{
    // gesture recognizer defaults 
    // - times are in milliseconds
    // - distances are in 1000ths of an inch
    // - angles are in radians
    const DWORD DEFAULT_SELECT_TIMEOUT_MS = 900;
    const DWORD DEFAULT_SELECT_DISTANCE = 197; // 5 mm
    const DWORD DEFAULT_HOLD_TIMEOUT_MS = 901;
    const DWORD DEFAULT_HOLD_DISTANCE = 197;
    const DWORD DEFAULT_DOUBLESELECT_TIMEOUT_MS = 350;
    const DWORD DEFAULT_DOUBLESELECT_DISTANCE = 197;
    const DWORD DEFAULT_SCROLL_TIMEOUT_MS = 250;
    const DWORD DEFAULT_SCROLL_DISTANCE = 198;
    const DWORD DEFAULT_PAN_DISTANCE = 198;
    const DWORD DEFAULT_FLICK_PAN_POINTS = 10;
    const double DEFAULT_FLICK_ANGLE_TOLERANCE = 0.34586;
    const double DEFAULT_FLICK_DIRECTION_TOLERANCE = 0.50000;
    
    PFN_SETTOUCHRECOGNIZERTIMER g_pfnSetTimer = NULL;
    PFN_KILLTOUCHRECOGNIZERTIMER g_pfnKillTimer = NULL;
    
    const double PI = 3.14159265358979323846;

    enum enumInputAction
    {
        eActionMove,
        eActionDown,
        eActionUp,
        eActionNone
    };

    inline ULONGLONG CalcSquare64(DWORD dwX)
    {
        return (ULONGLONG)dwX * (ULONGLONG) dwX;
    }

    inline DWORD PackFlickInertiaArgumentsHiDWORD(DWORD dwVelocity, DWORD dwDirection, USHORT usAngle)
    {
        USHORT usVelocity = static_cast<USHORT>(min(MAXWORD, dwVelocity));
        return MAKELONG(usVelocity, (dwDirection & 0x000F) | (usAngle & 0xFFF0));
    }
}


TouchGesture::SETTINGS TouchGesture::s_Settings = {0};
TouchGesture* TouchGesture::s_pTouchGesture = NULL;

const int TouchGesture::c_nBufferGrowth = 32;

DWORD TouchGesture::s_dwFlickDistanceTolerance = DEFAULT_SCROLL_DISTANCE;
DWORD TouchGesture::s_dwFlickTimeout = DEFAULT_SCROLL_TIMEOUT_MS; //time in ms that a flick must be 
DWORD TouchGesture::s_dwTapRange = DEFAULT_SELECT_DISTANCE;
DWORD TouchGesture::s_dwHoldRange = DEFAULT_HOLD_DISTANCE;
DWORD TouchGesture::s_dwPanRange = DEFAULT_PAN_DISTANCE;
DWORD TouchGesture::s_dwDoubleTapRange = DEFAULT_DOUBLESELECT_DISTANCE;
DWORD TouchGesture::s_dwFlickPanPoints = DEFAULT_FLICK_PAN_POINTS;

// this tolerance is the max delta of the angles in a flick from the end point to each intermediary point
// this is used to determine if the set of points are a flick, and to detect which is the start point
// for the flick
double TouchGesture::s_cFlickAngleTolerance = DEFAULT_FLICK_ANGLE_TOLERANCE; // 10degrees in radians

// this tolerance is used to detect if a flick is 4-way or free form
double TouchGesture::s_cFlickDirectionTolerance = DEFAULT_FLICK_DIRECTION_TOLERANCE; // 20degrees in radians

static HRESULT SetCallbackTimer(DWORD* pdwTimerId, DWORD dwElapse, TIMERPROC pfnCallback)
{
    HRESULT hr = S_OK;

    CPRAEx(g_pfnSetTimer, E_UNEXPECTED);

    hr = g_pfnSetTimer(pdwTimerId, dwElapse, pfnCallback);
    CHR(hr);

Error:
    return hr;
}

static HRESULT KillCallbackTimer(DWORD dwTimerId)
{
    HRESULT hr = S_OK;

    CPRAEx(g_pfnKillTimer, E_UNEXPECTED);

    hr = g_pfnKillTimer(dwTimerId);
    CHR(hr);

Error:
    return hr;
}

TouchGesture * TouchGesture::Instance()
{
    if (s_pTouchGesture == NULL)
    {
        s_pTouchGesture = new TouchGesture;
        if (NULL != s_pTouchGesture)
        {
            s_pTouchGesture->Init();
        }
    }
    return s_pTouchGesture;
}

TouchGesture::TouchGesture() :
    m_pPoints(NULL),
    m_cPoints(0),
    m_iHead(0),
    m_iCur(0),
    m_dwHoldTimerId(0),
    m_dwTapTimerId(0),
    m_uType(GID_NONE),
    m_dwEndTime(0),
    m_dwFlags(0),
    m_dwAdditionalInfo(0),
    m_dwAdditionalInfoHiDWORD(0),
    m_dwDoubleTapStart(0),
    m_fInProgress(FALSE),
    m_fCouldBeTap(FALSE),
    m_fCouldBeFlick(FALSE),
    m_cGesturesInSession(0),
    m_fHoldStarted(FALSE),
    m_ePanState(ePANNONE),
    m_dwTouchInputTime(0),
    m_hPrimaryHandle(INVALID_HANDLE_VALUE),
    m_hSecondaryHandle(INVALID_HANDLE_VALUE)
{
    memset(&m_ptDown, 0, sizeof(m_ptDown));
    memset(&m_ptStart, 0, sizeof(m_ptStart));
    memset(&m_ptPanStart, 0, sizeof(m_ptPanStart));
    memset(&m_ptPanEnd, 0, sizeof(m_ptPanEnd));
    memset(&m_rcGesture, 0, sizeof(m_rcGesture));
    memset(&m_rcDoubleTap, 0, sizeof(m_rcDoubleTap));
    memset(&m_ptEndStartPoint, 0, sizeof(m_ptEndStartPoint));
}

TouchGesture::~TouchGesture()
{
    if (NULL != m_pPoints)
    {
        delete[] m_pPoints;
        m_pPoints = NULL;
        m_cPoints = 0;
    }
}

HRESULT TouchGesture::AddPoint(int iXPrimary, int iYPrimary, int iXSecondary, int iYSecondary, DWORD dwTime)
{
    HRESULT hr = S_OK;
    
    if (PtIndexIncrement(m_iCur) == m_iHead)
    {
        // Reach the buffer limitation. Grow buffer size...
        hr = GrowBuffer();
        CHR(hr);
    }

    m_iCur = PtIndexIncrement(m_iCur);
    m_pPoints[m_iCur].iXPrimary = iXPrimary;
    m_pPoints[m_iCur].iYPrimary = iYPrimary;
    m_pPoints[m_iCur].iXSecondary = iXSecondary;
    m_pPoints[m_iCur].iYSecondary = iYSecondary;
    m_pPoints[m_iCur].dwTime = dwTime;

Error:    
    return hr;
}

void TouchGesture::EliminateTimeoutPoints()
{
    int i;
    DWORD dwDuration;


    // This checks each point from the oldest to the newest
    // moving the head (i.e. oldest) forward until the flick duration
    // is less than the maximum value allowed

    for (i = m_iHead; i != m_iCur; i = PtIndexIncrement(i))
    {
        dwDuration = m_pPoints[m_iCur].dwTime - m_pPoints[i].dwTime;
        if (dwDuration <= s_Settings.dwFlickTimeout)
        {
            break;
        }
    }
    m_iHead = i;
}

HRESULT TouchGesture::GrowBuffer()
{
    HRESULT hr = S_OK;
    CETOUCHPOINTS* pNewPoints;
    int cNewPoints = m_cPoints + c_nBufferGrowth; // Grow buffer size by s_cBufferGrowth.

    pNewPoints = new CETOUCHPOINTS[cNewPoints];
    CPR(pNewPoints);

    if (NULL != m_pPoints)
    {
        // Copy the existing points to new buffer
        if (m_iCur > m_iHead)
        {
            memcpy(pNewPoints, &(m_pPoints[m_iHead]), (m_iCur - m_iHead + 1) * sizeof(m_pPoints[0]));
        }
        else if (m_iCur < m_iHead)
        {
            memcpy(pNewPoints, &(m_pPoints[m_iHead]), (m_cPoints - m_iHead) * sizeof(m_pPoints[0]));
            memcpy(&(pNewPoints[m_cPoints - m_iHead]), m_pPoints, (m_iCur + 1) * sizeof(m_pPoints[0]));
            m_iCur += (m_cPoints - m_iHead);
            m_iHead = 0;
        }
        
        delete[] m_pPoints;
    }

    m_pPoints = pNewPoints;
    m_cPoints = cNewPoints;

Error:
    return hr;
}


void TouchGesture::GrowRect(RECT *pRect, int x, int y)
{
    if (x < pRect->left)
    {
        pRect->left = x;
    }
    else if (x > pRect->right)
    {
        pRect->right = x;
    }

    if (y < pRect->top)
    {
        pRect->top = y;
    }
    else if (y > pRect->bottom)
    {
        pRect->bottom = y;
    }
}

void TouchGesture::GrowGestureRect(int x, int y)
{
    GrowRect(&m_rcGesture, x, y);
}


void TouchGesture::SetType(UINT uType, BOOL fInitial, BOOL fTerminal)
{
    BOOL fEnabled = TRUE;

    m_dwFlags = 0L;
    if (fInitial && GID_NONE != uType)
    {
        // This is where we should query whether the specified gesture is
        // enabled and set m_uType to GID_NONE if not. For now, we assume all
        // gestures are enabled and that the message queue will filter out
        // the disabled ones. This comment is here to point to where we need
        // to add the code if this checking moves to the recognizer.
        fEnabled = TRUE;
    }
    if (fEnabled)
    {
        m_uType = uType;
        if (fInitial)
        {
            m_dwFlags |= GF_BEGIN;
        }
        if (fTerminal)
        {
            m_dwFlags |= GF_END;
        }
    }
    else
    {
        // Gesture is disabled.
        m_uType = GID_NONE;
    }
}


HRESULT TouchGesture::HandleSecondaryTouchDown(int xPrimary, int yPrimary, int xSecondary, int ySecondary)
{
    HRESULT hr = S_OK;

    SetType(GID_NONE, FALSE, FALSE);
    m_fCouldBeTap = FALSE;
    SetRect(&m_rcGestureSecondaryPoint, xSecondary, ySecondary, xSecondary, ySecondary);

    KillHoldTimer();
    KillTapTimer();
    
    if (m_fHoldStarted)
    {
        m_dwAdditionalInfoHiDWORD = 0;
        m_fHoldStarted = FALSE;
    }
    m_fCouldBeFlick = FALSE;
    return hr;
}

HRESULT TouchGesture::HandleSecondaryTouchUp(int xPrimary, int yPrimary, int xSecondary, int ySecondary)
{
    HRESULT hr = S_OK;

    SetType(GID_NONE, FALSE, FALSE);
    m_hSecondaryHandle = INVALID_HANDLE_VALUE;

    return hr;
}

HRESULT TouchGesture::HandleTouchDown(int xPrimary, int yPrimary, int xSecondary, int ySecondary)
{
    HRESULT hr = S_OK;

    SetType(GID_NONE, FALSE, FALSE);
    CBR(!m_fInProgress);
    m_dwTouchInputTime = m_pPoints[m_iCur].dwTime;

    if (!m_fInProgress)
    {
        m_iHead = m_iCur;
        m_ptDown.iXPrimary = m_pPoints[m_iCur].iXPrimary;
        m_ptDown.iYPrimary = m_pPoints[m_iCur].iYPrimary;
        
        SetStartPoint(m_pPoints[m_iCur].iXPrimary, m_pPoints[m_iCur].iYPrimary,
                      m_pPoints[m_iCur].iXSecondary, m_pPoints[m_iCur].iYSecondary,
                      m_pPoints[m_iCur].dwTime, TRUE);

        m_fInProgress = TRUE; 
        m_fCouldBeTap = TRUE;
        m_fCouldBeFlick = TRUE;
        m_fHoldStarted = FALSE;
        m_ePanState = ePANNONE;
        m_cGesturesInSession = 0;
    }

Error:
    return hr;
}


HRESULT TouchGesture::HandleTouchMove(int xPrimary, int yPrimary, int xSecondary, int ySecondary)
{
    HRESULT hr = S_OK;
    BOOL fSendHoldEnd = FALSE;
    BOOL fFirstPan = FALSE;

    SetType(GID_NONE, FALSE, FALSE);

    // Bail out the move messages if is not in progress.
    CBR(m_fInProgress);

    EliminateTimeoutPoints();
    m_dwTouchInputTime = m_pPoints[m_iCur].dwTime;
    GrowGestureRect(xPrimary, yPrimary);
    if (xSecondary != INVALID_TOUCH_POINT && ySecondary != INVALID_TOUCH_POINT)
    {
        GrowRect(&m_rcGestureSecondaryPoint, xSecondary, ySecondary);
    }
    if (m_fCouldBeTap)
    {
        if(((DWORD)RECTWIDTH(m_rcGesture) >= s_dwTapRange ||
         (DWORD)RECTHEIGHT(m_rcGesture) >= s_dwTapRange) )
        {   
            //this cannot be a tap anymore
            m_fCouldBeTap = FALSE;

            KillTapTimer();
        }
    }
    // Handle first time movement that counts as a PAN
    if( ePANNONE == m_ePanState)
    {
        if(((DWORD)RECTWIDTH(m_rcGesture) >= s_dwHoldRange ||
         (DWORD)RECTHEIGHT(m_rcGesture) >= s_dwHoldRange) )
        {        
            //this gesture cannot be a hold anymore

            KillHoldTimer();

            if (m_fHoldStarted)
            {
                fSendHoldEnd = TRUE;
                m_fHoldStarted = FALSE;
            }
        }

        if(FALSE == m_fCouldBeTap && ((DWORD)RECTWIDTH(m_rcGesture) >= s_dwPanRange ||
            (DWORD)RECTHEIGHT(m_rcGesture) >= s_dwPanRange)||
            ((DWORD)RECTWIDTH(m_rcGestureSecondaryPoint) >= s_dwPanRange ||
            (DWORD)RECTHEIGHT(m_rcGestureSecondaryPoint) >= s_dwPanRange)
            )
        {
            // Start PAN from where we've moved to
            SetStartPoint(m_pPoints[m_iCur].iXPrimary, m_pPoints[m_iCur].iYPrimary,
                          m_pPoints[m_iCur].iXSecondary, m_pPoints[m_iCur].iYSecondary,
                          m_pPoints[m_iCur].dwTime, FALSE);
            if (m_pPoints[m_iCur].iXSecondary != INVALID_TOUCH_POINT && 
                m_pPoints[m_iCur].iYSecondary != INVALID_TOUCH_POINT)
            {
                m_ePanState = ePANMULTIPOINT;
            }
            else
            {
                m_ePanState = ePANSINGLEPOINT;
            }
            fFirstPan = TRUE;
        }
    }

    m_dwAdditionalInfoHiDWORD = 0;

    // Handling all PAN movements
    if( ePANNONE != m_ePanState )
    {        
        // We're PANning - only send if it's there's a delta
        // except if this is our first PAN.
        // check for multi touch
        if (xSecondary != INVALID_TOUCH_POINT && 
            ySecondary != INVALID_TOUCH_POINT)
        {
            if (SetMultitouchPanPoints(fFirstPan))
            {
                SetType(GID_PAN, fFirstPan, FALSE);
            }
        }
        else if( SetPanPoints(fFirstPan) )
        {
            SetType(GID_PAN, fFirstPan, FALSE);
        }
    }

Error:
    return hr;
}

HRESULT TouchGesture::HandleTouchUp(int xPrimary, int yPrimary, int xSecondary, int ySecondary)
{
    HRESULT hr = S_OK;

    SetType(GID_NONE, FALSE, FALSE);
    // Bail out the touch up messages if is not in progress.
    CBR(m_fInProgress);
    
    KillHoldTimer();
    KillTapTimer();
    
    EliminateTimeoutPoints();
    m_dwTouchInputTime = m_pPoints[m_iCur].dwTime;
    if (m_fCouldBeTap)
    {
        // first check if this is a double tap
        if ((m_pPoints[m_iCur].dwTime - m_dwDoubleTapStart) < s_Settings.dwDoubleTapTimeout)
        {
            GrowRect(&m_rcDoubleTap, xPrimary,yPrimary);
            if ((DWORD)RECTWIDTH(m_rcDoubleTap) <= s_dwDoubleTapRange &&
                (DWORD)RECTHEIGHT(m_rcDoubleTap) <= s_dwDoubleTapRange)
            {
                // this is a double tap
                SetType(GID_DOUBLESELECT, TRUE, TRUE);
                m_dwEndTime = m_pPoints[m_iCur].dwTime;
                m_dwAdditionalInfo = 0;
                m_dwAdditionalInfoHiDWORD = 0;
            }
        }
        m_dwDoubleTapStart = 0;
        memset(&m_rcDoubleTap, 0, sizeof(m_rcDoubleTap));
    
        GrowGestureRect(xPrimary, yPrimary);
        if (m_uType == GID_NONE &&
            (DWORD)RECTWIDTH(m_rcGesture) <= s_dwTapRange &&
            (DWORD)RECTHEIGHT(m_rcGesture) <= s_dwTapRange)
        {
            SetType(GID_SELECT, TRUE, TRUE);
            m_dwEndTime = m_pPoints[m_iCur].dwTime;
            m_dwAdditionalInfo = 0;
            m_dwAdditionalInfoHiDWORD = 0;
            // save off this tap info for double tap detection
            m_dwDoubleTapStart = m_dwEndTime;
            SetRect(&m_rcDoubleTap, m_pPoints[m_iCur].iXPrimary, m_pPoints[m_iCur].iYPrimary, m_pPoints[m_iCur].iXPrimary, m_pPoints[m_iCur].iYPrimary);

            // Record how long the stylus was down
            TouchPerf::AddStatistic(TOUCHPERFITEM_TAPSTYLUSDOWN_STATS, m_pPoints[m_iCur].dwTime - m_pPoints[m_iHead].dwTime);
        }
        else
        {
            m_fCouldBeTap = FALSE;
        }
    }

    if (GID_NONE == m_uType)
    {
        CETOUCHPOINTS ptEndStartPoint = m_ptStart;

        // RecognizeFlick could change the points, so set Pan Points first
        if (xSecondary != INVALID_TOUCH_POINT && 
            ySecondary != INVALID_TOUCH_POINT)
        {
            SetMultitouchPanPoints(TRUE);
        }
        else
        {
            SetPanPoints(TRUE);
        }
        

        // If a PAN started then we want to send a final PAN with GF_END set before we
        // transition to a possible FLICK. We'll reuse the location from the last PAN
        // so the delta is effectively 0.
        GESTUREINFO giEndPan;
        ZeroMemory(&giEndPan, sizeof(GESTUREINFO));
        PANSTATE eCurrentPanState = m_ePanState;
        if (ePANNONE != eCurrentPanState)
        {
            giEndPan.cbSize = sizeof(GESTUREINFO);
            giEndPan.dwID = GID_PAN;
            giEndPan.dwFlags = GF_END;
            if (m_ePanState == ePANMULTIPOINT)
            {
                giEndPan.ptsLocation.x = (m_ptPanEnd.iXPrimary / TOUCH_SCALING_FACTOR + m_ptPanEnd.iXSecondary / TOUCH_SCALING_FACTOR) / 2;
                giEndPan.ptsLocation.y = (m_ptPanEnd.iYPrimary / TOUCH_SCALING_FACTOR + m_ptPanEnd.iYSecondary / TOUCH_SCALING_FACTOR) / 2;
            }
            else
            {
                giEndPan.ptsLocation.x = m_ptPanEnd.iXPrimary / TOUCH_SCALING_FACTOR;
                giEndPan.ptsLocation.y = m_ptPanEnd.iYPrimary / TOUCH_SCALING_FACTOR;
            }
            giEndPan.ullArguments = (((ULONGLONG)m_dwAdditionalInfoHiDWORD) << (sizeof(DWORD)*8)) | m_dwAdditionalInfo;
        }

        // Store the delta for a delayed PAN END messages
        m_dwAdditionalInfoHiDWORD = 0;
        hr = RecognizeFlick();
        // if we detect a flick, and we are sending an PAN with GF_END, then
        // add the scroll information to this PAN with the GF_INERTIA flag
        if (ePANNONE != eCurrentPanState)
        {
            giEndPan.dwSequenceID = m_dwTouchInputTime;
            if (GID_SCROLL == m_uType)
            {
                giEndPan.dwFlags |= GF_INERTIA;
                giEndPan.ullArguments = (((ULONGLONG)m_dwAdditionalInfoHiDWORD) << (sizeof(DWORD)*8)) | m_dwAdditionalInfo;
                // we are using the instance ID to specify the time in which the up event occured.  set that here for intertia
            }
            Gesture(NULL, &giEndPan, NULL);
            ++m_cGesturesInSession;
        }
        // if there is no flick, then we might have broken the PAN threshold
        // check for that and send a begin/end PAN here
        if (GID_NONE == m_uType && m_ePanState == ePANNONE)
        {
            GrowGestureRect(xPrimary, yPrimary);
            if(((DWORD)RECTWIDTH(m_rcGesture) >= s_dwPanRange ||
                (DWORD)RECTHEIGHT(m_rcGesture) >= s_dwPanRange))
            {
                SetType(GID_PAN, TRUE, TRUE);
            }

        }
        
    }

Error:
    m_fInProgress = FALSE;
    m_ePanState = ePANNONE;
    m_fHoldStarted = FALSE;
    m_hPrimaryHandle = INVALID_HANDLE_VALUE;
    m_hSecondaryHandle = INVALID_HANDLE_VALUE;
    return hr;
}

HRESULT TouchGesture::HandleTimer(DWORD dwTimerId)
{
    HRESULT hr = E_PENDING;

    DWORD dwTime;
    
    CBR(m_fInProgress);
    CBREx(((dwTimerId == m_dwHoldTimerId) || (dwTimerId == m_dwTapTimerId)), E_INVALIDARG);

    dwTime = GetTickCount() - m_ptStart.dwTime;

    if (m_dwHoldTimerId == dwTimerId)
    {
        // Stop the timer firing again
        KillHoldTimer();

        // It's a HOLD
        SetType(GID_HOLD, TRUE, TRUE);
        m_dwEndTime = dwTime + m_ptStart.dwTime;
        m_dwAdditionalInfo = 0;
        m_dwAdditionalInfoHiDWORD = 0;
        m_fHoldStarted = TRUE;
    }
    
    if (m_dwTapTimerId == dwTimerId)
    {
        // Stop the timer firing again
        KillTapTimer();

        ASSERT(dwTime >= s_Settings.dwTapTimeout);

        //the timer was hit, this cannot be a tap anymore
        m_fCouldBeTap = FALSE;
        // If we've had some movement within the TAP timeout
        // send a PAN, as long as we're moved outside the PAN tolerance
        if (m_iCur != m_iHead &&
            ePANNONE == m_ePanState &&
            ((DWORD)RECTWIDTH(m_rcGesture) >= s_dwPanRange ||
            (DWORD)RECTHEIGHT(m_rcGesture) >= s_dwPanRange)||
            ((DWORD)RECTWIDTH(m_rcGestureSecondaryPoint) >= s_dwPanRange ||
            (DWORD)RECTHEIGHT(m_rcGestureSecondaryPoint) >= s_dwPanRange)
            )        
        { 
            // Can't be HOLD anymore
            KillHoldTimer();
        }
    }
    
    hr = S_OK;

Error:
    return hr;
}

void TouchGesture::Init()
{
    HRESULT hr = S_OK;
    GESTUREMETRICS gmSettings = {0};

    // Settings is not initialized. Initialize it.
    if (0 == s_Settings.dwHoldTimeout)
    {
        gmSettings.cbSize = sizeof(GESTUREMETRICS);

        //read GID_HOLD settings
        gmSettings.dwID = GID_HOLD;
        //read the settings
        (void)GetSettingsFromRegistry(&gmSettings);
        VHR(SetSettings(&gmSettings, FALSE, FALSE));
        
        //read GID_SELECT settings
        gmSettings.dwID = GID_SELECT;
        //read the settings
        (void)GetSettingsFromRegistry(&gmSettings);
        VHR(SetSettings(&gmSettings, FALSE, FALSE));
        
        //read GID_DOUBLESELECT settings
        gmSettings.dwID = GID_DOUBLESELECT;
        //read the settings
        (void)GetSettingsFromRegistry(&gmSettings);
        VHR(SetSettings(&gmSettings, FALSE, FALSE));

        //read GID_SCROLL settings
        gmSettings.dwID = GID_SCROLL;
        //read the settings
        (void)GetSettingsFromRegistry(&gmSettings);
        VHR(SetSettings(&gmSettings, FALSE, FALSE));

        //read GID_PAN settings
        gmSettings.dwID = GID_PAN;
        //read the settings
        (void)GetSettingsFromRegistry(&gmSettings);
        VHR(SetSettings(&gmSettings, FALSE, FALSE));
    }
}

BOOL TouchGesture::FindFlickStartPoints(
    __out int* piStart, 
    __out int* piVelocityPoint)
{    
    BOOL fFlickStartFound = FALSE;
    ULONGLONG ullLengthSquaredMax = 0;
    int iStart = m_iCur;
    int iVelocityPoint = m_iCur;

    // We need to check the points that include where m_iHead is.
    // So set iEnd to the previous point index of m_iHead.
    // Note: iEnd is either going to be m_iCur (if the buffer is full)
    // or an invalid point.
    int iEnd = PtIndexDecrement(m_iHead);    

    double dMaxAngle = 0.0;
    // choose anything greater than pi/2 for min, since it will be set in the first calc
    double dMinAngle = 3.0;

    ASSERT(NULL != piStart);
    ASSERT(NULL != piVelocityPoint);

    const ULONGLONG ullFlickDistanceToleranceSquared = 
        CalcSquare64(s_dwFlickDistanceTolerance);

    // This is looping from the point-before last, backwards in time to the
    // first point (m_iHead). It exits if the point is outside the angular
    // tolerance or if the length is getting smaller, as this means that the points
    // must be travelling back towards the end point.
    for (int i = PtIndexDecrement(m_iCur); i != iEnd; i = PtIndexDecrement(i))
    {
        // calculate the arctan of the angle between our end point and this current point
        DWORD dwXDelta = abs(m_pPoints[m_iCur].iXPrimary - m_pPoints[i].iXPrimary);
        DWORD dwYDelta = abs(m_pPoints[m_iCur].iYPrimary - m_pPoints[i].iYPrimary);
        ULONGLONG ullLengthSquaredTemp = CalcSquare64(dwXDelta) + CalcSquare64(dwYDelta);

        // make sure we move the minimum distance away before tracking the angle
        if (fFlickStartFound || 
            (ullLengthSquaredTemp >= ullFlickDistanceToleranceSquared)) 
        {
            if (!fFlickStartFound)
            {
                // First valid point that could be the start of a flick
                fFlickStartFound = TRUE;
            }

            double dTempAngle = (dwXDelta == 0 ? PI/2.0 : atan2(dwYDelta, dwXDelta));
            if (dTempAngle > dMaxAngle)
            {
                dMaxAngle = dTempAngle;
            }
            if (dTempAngle < dMinAngle)
            {
                dMinAngle = dTempAngle;
            }
            // compare the difference between the max and min angle, if greater
            // than our threshold, then this point is not part of the flick
            //so bail out
            if (dMaxAngle - dMinAngle > s_cFlickAngleTolerance)
            {
                break;
            }

            // check that the length is monotonically increasing 
            // (i.e. the points aren't coming back towards the touch up point)
            // If so this is the end of the flick.
            if (ullLengthSquaredTemp < ullLengthSquaredMax)
            {
                break;
            }
            ullLengthSquaredMax = ullLengthSquaredTemp;
        }
        // if still in the loop, update iStart
        iStart = i;
    }

    // find the velocity point
    if (fFlickStartFound)
    {
        // ASSERT if invalid number of flick pan points
        ASSERT(s_dwFlickPanPoints > 0);        
        if (GetPointCount(iStart) < (int)s_dwFlickPanPoints)
        {
            iVelocityPoint = iStart;
        }
        else
        {
            iVelocityPoint = PtIndexDecrementBy(m_iCur, s_dwFlickPanPoints-1);            
        }
    }
    
    *piStart = iStart;
    *piVelocityPoint = iVelocityPoint;
    return fFlickStartFound;
}

HRESULT TouchGesture::RecognizeFlick()
{
    HRESULT hr = S_OK;
    int iStart = m_iCur;
    int iVelocityPoint = m_iCur;
    BOOL fFlickStartFound = FALSE;

    if (m_pPoints == NULL || GetPointCount() <= 1 || !m_fCouldBeFlick)
    {
        // we can not be a flick if we have one or fewer points or we are told we can't
        goto Exit;
    }
    
    // Checking the input touch points to meet flick gesture requirements
    ASSERT(GID_NONE == m_uType);

    if (!FindFlickStartPoints(&iStart, &iVelocityPoint))
    {
        goto Exit;
    }
    
    DWORD dwLength = 0;
    DWORD dwVelocity = 0;
    if (s_dwFlickTimeout != 0)
    {
        // calculate velocity based on distance over time
        // Use previously calculated point to find velocity
        DWORD dwTimeInterval = 0;
        
        // dwLength is in touch units, i.e. it is in pixels * TOUCH_SCALING_FACTOR
        // Convert the square root result into an integer straight away - we lose some
        // fraction of 1/4 of a pixel by doing this, but make the velocity calculation more
        // efficient as a result.
        dwLength = static_cast<DWORD>(sqrt(sqr(m_pPoints[m_iCur].iXPrimary - m_pPoints[iVelocityPoint].iXPrimary) + 
                                           sqr(m_pPoints[m_iCur].iYPrimary - m_pPoints[iVelocityPoint].iYPrimary)));

        // We want velocity to be in pixels per second, so we apply the touch scaling factor conversion
        // here. s_dwFlickDistanceTolerance is in touch units, and the times here are in milli-seconds.
        dwTimeInterval = m_pPoints[m_iCur].dwTime - m_pPoints[iVelocityPoint].dwTime;
        if (0 == dwTimeInterval)
        {
            // Time interval cannot be 0. In this case set it to 1 ms.
            dwTimeInterval = 1;
        }
        dwVelocity = ((dwLength * 1000) / (TOUCH_SCALING_FACTOR * dwTimeInterval));
        if (dwVelocity >= (s_dwFlickDistanceTolerance * 1000) / (TOUCH_SCALING_FACTOR * s_dwFlickTimeout))
        {
            // It is fast enough. A flick gesture.
            SetType(GID_SCROLL, TRUE, TRUE);
            m_dwEndTime = m_pPoints[m_iCur].dwTime;
        }
    }

    if (GID_SCROLL != m_uType)
    {
        goto Error;
    }

    SetStartPoint(m_pPoints[iStart].iXPrimary, m_pPoints[iStart].iYPrimary, INVALID_TOUCH_POINT, INVALID_TOUCH_POINT, m_pPoints[iStart].dwTime, FALSE);


    // calculate our flick angle
    double dAngle = 0.0;

    // for this calculation, we invert the y coordinates (use start - current) because our coordinate system has
    // positive values of y going up.  We therefor invert below.  X coordiantes work fine
    
    // handle the invalid arctan case
    if ((m_pPoints[m_iCur].iXPrimary - m_pPoints[iStart].iXPrimary) == 0)
    {
        dAngle = (m_pPoints[iStart].iYPrimary - m_pPoints[m_iCur].iYPrimary > 0 ? PI / 2.0 : 3.0 * PI / 2.0);
    }
    else
    {
        dAngle = (atan2((double)(m_pPoints[iStart].iYPrimary - m_pPoints[m_iCur].iYPrimary), (double)(m_pPoints[m_iCur].iXPrimary - m_pPoints[iStart].iXPrimary)));
        // normalize to 0 to 2PI
        if (dAngle < 0.0)
        {
            dAngle += 2.0 * PI;
        }
    }
   
    m_dwAdditionalInfo = 0;         // The low 32-bits of a flick gesture args are always zero
    if (dAngle < 0 + s_cFlickDirectionTolerance && dAngle >= 0.0)
    {
        // right flick
        m_dwAdditionalInfoHiDWORD = PackFlickInertiaArgumentsHiDWORD(dwVelocity, ARG_SCROLL_LEFT, GID_ROTATE_ANGLE_TO_ARGUMENT(dAngle));
    }
    else if (dAngle <= 2.0 * PI && dAngle > ((2 * PI) - s_cFlickDirectionTolerance))
    {
        // right flick
        m_dwAdditionalInfoHiDWORD = PackFlickInertiaArgumentsHiDWORD(dwVelocity, ARG_SCROLL_LEFT, GID_ROTATE_ANGLE_TO_ARGUMENT(dAngle));
    }
    else if (dAngle < PI + s_cFlickDirectionTolerance && dAngle > PI - s_cFlickDirectionTolerance)
    {
        // left flick
        m_dwAdditionalInfoHiDWORD = PackFlickInertiaArgumentsHiDWORD(dwVelocity, ARG_SCROLL_RIGHT, GID_ROTATE_ANGLE_TO_ARGUMENT(dAngle));
    }
    else if (dAngle < ((PI/2.0) + s_cFlickDirectionTolerance) && dAngle > ((PI/2.0) - s_cFlickDirectionTolerance))
    {
        // up flick
        m_dwAdditionalInfoHiDWORD = PackFlickInertiaArgumentsHiDWORD(dwVelocity, ARG_SCROLL_DOWN, GID_ROTATE_ANGLE_TO_ARGUMENT(dAngle));
    }
    else if (dAngle < ((3.0 * PI/2.0) + s_cFlickDirectionTolerance) && dAngle > ((3.0 * PI/2.0) - s_cFlickDirectionTolerance))
    {
        // down flick
        m_dwAdditionalInfoHiDWORD = PackFlickInertiaArgumentsHiDWORD(dwVelocity, ARG_SCROLL_UP, GID_ROTATE_ANGLE_TO_ARGUMENT(dAngle));
    }
    else
    {
        m_dwAdditionalInfoHiDWORD = PackFlickInertiaArgumentsHiDWORD(dwVelocity, ARG_SCROLL_NONE, GID_ROTATE_ANGLE_TO_ARGUMENT(dAngle));
    }

Error:
Exit:
    return hr;
}

HRESULT TouchGesture::StartTapTimer(DWORD dwTimeout)
{
    HRESULT hr = S_OK;

    // Kill the timer if one is already queued
    KillTapTimer();

    CHR(SetCallbackTimer(&m_dwTapTimerId, dwTimeout, &TouchGesture::TimerProc));
    ASSERT(0 != m_dwTapTimerId);

Error:
    return hr;
}

void TouchGesture::KillTapTimer()
{
    if (0 != m_dwTapTimerId)
    {
        // Kill the timer
        KillCallbackTimer(m_dwTapTimerId);
        m_dwTapTimerId = 0;
    }
}

HRESULT TouchGesture::StartHoldTimer(DWORD dwTimeout)
{
    HRESULT hr = S_OK;

    // Kill the timer if one is already queued
    KillHoldTimer();

    CHR(SetCallbackTimer(&m_dwHoldTimerId, dwTimeout, &TouchGesture::TimerProc));
    ASSERT(0 != m_dwHoldTimerId);

Error:
    return hr;
}

void TouchGesture::KillHoldTimer()
{
    if (0 != m_dwHoldTimerId)
    {
        // Kill the timer
        KillCallbackTimer(m_dwHoldTimerId);
        m_dwHoldTimerId = 0;
    }
}

void CALLBACK TouchGesture::TimerProc(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
    UNREFERENCED_PARAMETER(hwnd);
    UNREFERENCED_PARAMETER(uMsg);
    UNREFERENCED_PARAMETER(dwTime);

    TouchGesture * pTouchGesture = NULL;

    pTouchGesture = TouchGesture::Instance();
    ASSERT(NULL != pTouchGesture);      // if this fires, how did we get here?
    if (NULL != pTouchGesture)
    {
        pTouchGesture->ClearType();

        if (SUCCEEDED(pTouchGesture->HandleTimer(idEvent)))
        {
            if (pTouchGesture->GetType() != GID_NONE)
            {
                pTouchGesture->SendEvent();
            }
        }
    }
}

BOOL TouchGesture::SendEvent(void)
{
    HRESULT hr = S_OK;
    GESTUREINFO gi;

    if (GID_NONE != GetType())
    {
        // If we're starting a gesture, send GID_BEGIN first. It
        // has the point where we came down on
        if (m_cGesturesInSession == 0)
        {
            ASSERT(GID_NONE != GetType());

            ZeroMemory(&gi, sizeof(GESTUREINFO));
            gi.cbSize = sizeof(GESTUREINFO);
            gi.dwID = GID_BEGIN;
            gi.ptsLocation.x = m_ptDown.iXPrimary / TOUCH_SCALING_FACTOR;;
            gi.ptsLocation.y = m_ptDown.iYPrimary / TOUCH_SCALING_FACTOR;;
            Gesture(NULL, &gi, NULL);

            ++m_cGesturesInSession;
        }

        ZeroMemory(&gi, sizeof(GESTUREINFO));
        gi.cbSize = sizeof(GESTUREINFO);
        gi.dwID = GetType();
        gi.dwFlags = m_dwFlags;
        // using time in the instance id field provides a fairly unique time stamp that can 
        // also be used by apps that need this to handle latency.  set that here
        gi.dwSequenceID = m_dwTouchInputTime;
        gi.ullArguments = (((ULONGLONG)m_dwAdditionalInfoHiDWORD) << (sizeof(DWORD)*8)) | m_dwAdditionalInfo;
        if (GID_PAN == m_uType)
        {
            if (m_hSecondaryHandle == INVALID_HANDLE_VALUE)
            {
                gi.ptsLocation.x = m_ptPanEnd.iXPrimary / TOUCH_SCALING_FACTOR;
                gi.ptsLocation.y = m_ptPanEnd.iYPrimary / TOUCH_SCALING_FACTOR;
                // if we are transitioning from a multi to a single, send an end, and mark this as a begin
                if (ePANMULTIPOINT == m_ePanState)
                {
                    GESTUREINFO giEndPan;
                    ZeroMemory(&giEndPan, sizeof(GESTUREINFO));
                    giEndPan.cbSize = sizeof(GESTUREINFO);
                    giEndPan.dwID = GID_PAN;
                    giEndPan.dwFlags = GF_END;
                    giEndPan.ptsLocation.x = m_ptsLastPanPoints.x;
                    giEndPan.ptsLocation.y = m_ptsLastPanPoints.y;
                    giEndPan.ullArguments = m_ullLastPanAdditionalInfo;
                    Gesture(NULL, &giEndPan, NULL);
                    ++m_cGesturesInSession;
                    m_ePanState = ePANSINGLEPOINT;
                    gi.dwFlags |= GF_BEGIN;
                }
            }
            else
            {
                // this is a multi touch PAN, so we need to calculate our midpoint
                gi.ptsLocation.x = (m_ptPanEnd.iXPrimary / TOUCH_SCALING_FACTOR + m_ptPanEnd.iXSecondary / TOUCH_SCALING_FACTOR) / 2;
                gi.ptsLocation.y = (m_ptPanEnd.iYPrimary / TOUCH_SCALING_FACTOR + m_ptPanEnd.iYSecondary / TOUCH_SCALING_FACTOR) / 2;
                // if this is our first multi touch PAN, set the START flag
                if (ePANSINGLEPOINT == m_ePanState)
                {
                    GESTUREINFO giEndPan;
                    ZeroMemory(&giEndPan, sizeof(GESTUREINFO));
                    giEndPan.cbSize = sizeof(GESTUREINFO);
                    giEndPan.dwID = GID_PAN;
                    giEndPan.dwFlags = GF_END;
                    giEndPan.ptsLocation.x = m_ptsLastPanPoints.x;
                    giEndPan.ptsLocation.y = m_ptsLastPanPoints.y;
                    giEndPan.ullArguments = 0;
                    Gesture(NULL, &giEndPan, NULL);
                    ++m_cGesturesInSession;
                    m_ePanState = ePANMULTIPOINT;
                    gi.dwFlags |= GF_BEGIN;
                }
            }
            // and save off the values in case we send an END
            m_ptsLastPanPoints.x = gi.ptsLocation.x;
            m_ptsLastPanPoints.y = gi.ptsLocation.y;
            // for this value, we really only want the HIDWORD in case of a multi to single pan
            m_ullLastPanAdditionalInfo = (((ULONGLONG)m_dwAdditionalInfoHiDWORD) << (sizeof(DWORD)*8)) | 0;
            
        }
        else
        {
            gi.ptsLocation.x = m_ptPanStart.iXPrimary / TOUCH_SCALING_FACTOR;
            gi.ptsLocation.y = m_ptPanStart.iYPrimary / TOUCH_SCALING_FACTOR;
        }
        Gesture(NULL, &gi, NULL);
        ++m_cGesturesInSession;
    }
    m_uType = GID_NONE;
    return SUCCEEDED(hr);
}



void TouchGesture::SetStartPoint(int xPrimary, int yPrimary, int xSecondary, int ySecondary, DWORD dwTime, BOOL fSetTimer)
{
    // The m_pPoints only contains the points within the flick timeout time.
    // I.E., any point is older than that time will be eliminated.
    // So we need to keep the start point info separately.
    m_ptStart.iXPrimary = xPrimary;
    m_ptStart.iYPrimary = yPrimary;
    m_ptStart.iXSecondary = xSecondary;
    m_ptStart.iYSecondary = ySecondary;
    m_ptStart.dwTime = dwTime;
    SetRect(&m_rcGesture, xPrimary, yPrimary, xPrimary, yPrimary);
    SetRect(&m_rcGestureSecondaryPoint, xSecondary, ySecondary, xSecondary, ySecondary);
    
    // Initialize the pan start and end points
    m_ptPanStart.iXPrimary = xPrimary;
    m_ptPanStart.iYPrimary = yPrimary;
    m_ptPanStart.iXSecondary = xSecondary;
    m_ptPanStart.iYSecondary = ySecondary;
    m_ptPanStart.dwTime = dwTime;
    m_ptPanEnd.iXPrimary = xPrimary;
    m_ptPanEnd.iYPrimary = yPrimary;
    m_ptPanEnd.iXSecondary = xSecondary;
    m_ptPanEnd.iYSecondary = ySecondary;
    m_ptPanEnd.dwTime = dwTime;

    KillHoldTimer();
    KillTapTimer();

    if( fSetTimer )
    {
        StartHoldTimer(s_Settings.dwHoldTimeout);
        StartTapTimer(s_Settings.dwTapTimeout);
    }
}

/// <summary>
/// Keeps track of the current PAN movements
/// </summary>
/// <param name="fForcePan">A BOOL - to indicate we have to accept this PAN
/// even if it is has a (0,0) delta</param>
/// <returns>
/// <para>TRUE: PAN accepted, FALSE: when fForcePan is FALSE and the delta is (0,0)</para>
/// </returns>
BOOL TouchGesture::SetPanPoints(BOOL fForcePAN)
{
    DWORD dwDeltaYOnScreen = (m_pPoints[m_iCur].iYPrimary - m_ptPanEnd.iYPrimary) / TOUCH_SCALING_FACTOR;
    DWORD dwDeltaXOnScreen = (m_pPoints[m_iCur].iXPrimary - m_ptPanEnd.iXPrimary) / TOUCH_SCALING_FACTOR;
    DWORD dwDeltaOnScreen = MAKEWPARAM(dwDeltaYOnScreen, dwDeltaXOnScreen);

    if( FALSE == fForcePAN && 0L == dwDeltaOnScreen )
    {
        return FALSE;
    }
    
    // The start of a PAN is the previous point
    m_ptPanStart.iXPrimary = m_ptPanEnd.iXPrimary;
    m_ptPanStart.iYPrimary = m_ptPanEnd.iYPrimary;
    m_ptPanStart.iXSecondary = m_ptPanEnd.iXSecondary;
    m_ptPanStart.iYSecondary = m_ptPanEnd.iYSecondary;
    m_ptPanStart.dwTime = m_ptPanEnd.dwTime;

    m_ptPanEnd.iXPrimary = m_pPoints[m_iCur].iXPrimary;
    m_ptPanEnd.iYPrimary = m_pPoints[m_iCur].iYPrimary;
    m_ptPanEnd.iXSecondary = m_pPoints[m_iCur].iXSecondary;
    m_ptPanEnd.iYSecondary = m_pPoints[m_iCur].iYSecondary;
    m_ptPanEnd.dwTime = m_pPoints[m_iCur].dwTime;

    m_dwEndTime = m_pPoints[m_iCur].dwTime;

    m_dwAdditionalInfo = dwDeltaOnScreen;
    m_dwAdditionalInfoHiDWORD = 0;

    return TRUE;
}



/// <summary>
/// Keeps track of the current PAN movements for multitouch pans
/// </summary>
/// <param name="fForcePan">A BOOL - to indicate we have to accept this PAN
/// even if it is has a (0,0) delta</param>
/// <returns>
/// <para>TRUE: PAN accepted, FALSE: when fForcePan is FALSE and the delta is (0,0)</para>
/// </returns>
BOOL TouchGesture::SetMultitouchPanPoints(BOOL fForcePAN)
{
    DWORD dwXMidpointCurrent = (m_pPoints[m_iCur].iXPrimary + m_pPoints[m_iCur].iXSecondary) / 2 * TOUCH_SCALING_FACTOR;
    DWORD dwYMidpointCurrent = (m_pPoints[m_iCur].iYPrimary + m_pPoints[m_iCur].iYSecondary) / 2 * TOUCH_SCALING_FACTOR;
    DWORD dwXMidpointPrevious = dwXMidpointCurrent;
    DWORD dwYMidpointPrevious = dwYMidpointCurrent;
    DWORD dwDeltaXOnScreen = 0;
    DWORD dwDeltaYOnScreen = 0;

    // when this is not the first multitouch point handle calcualtions
    if (m_ptPanEnd.iXSecondary != INVALID_TOUCH_POINT && 
        m_ptPanEnd.iYSecondary != INVALID_TOUCH_POINT)
    {
        dwXMidpointPrevious = (m_ptPanEnd.iXPrimary + m_ptPanEnd.iXSecondary) / 2;
        dwYMidpointPrevious = (m_ptPanEnd.iYPrimary + m_ptPanEnd.iYSecondary) / 2;
        dwDeltaXOnScreen = (dwXMidpointCurrent - dwXMidpointPrevious);
        dwDeltaYOnScreen = (dwYMidpointCurrent - dwYMidpointPrevious);
        if( FALSE == fForcePAN && (0L == dwDeltaYOnScreen && 0L == dwDeltaXOnScreen))
        {
            return FALSE;
        }
    }
    
    float flDistance = Distance(m_pPoints[m_iCur].iXPrimary / TOUCH_SCALING_FACTOR,
                                m_pPoints[m_iCur].iXSecondary / TOUCH_SCALING_FACTOR,
                                m_pPoints[m_iCur].iYPrimary / TOUCH_SCALING_FACTOR,
                                m_pPoints[m_iCur].iYSecondary / TOUCH_SCALING_FACTOR);
    // The start of a PAN is the previous point
    m_ptPanStart.iXPrimary = m_ptPanEnd.iXPrimary;
    m_ptPanStart.iYPrimary = m_ptPanEnd.iYPrimary;
    m_ptPanStart.iXSecondary = m_ptPanEnd.iXSecondary;
    m_ptPanStart.iYSecondary = m_ptPanEnd.iYSecondary;
    m_ptPanStart.dwTime = m_ptPanEnd.dwTime;

    m_ptPanEnd.iXPrimary = m_pPoints[m_iCur].iXPrimary;
    m_ptPanEnd.iYPrimary = m_pPoints[m_iCur].iYPrimary;
    m_ptPanEnd.iXSecondary = m_pPoints[m_iCur].iXSecondary;
    m_ptPanEnd.iYSecondary = m_pPoints[m_iCur].iYSecondary;
    m_ptPanEnd.dwTime = m_pPoints[m_iCur].dwTime;

    m_dwEndTime = m_pPoints[m_iCur].dwTime;

    // total hack for now, we need to pass distance plus deltas of X and Y
    // passback 16bits of distance and 8 bits each of X and Y deltas
    m_dwAdditionalInfo =  dwDeltaXOnScreen & 0xFFFF << 16 |
                          dwDeltaYOnScreen & 0xFFFF;
    m_dwAdditionalInfoHiDWORD = ((DWORD)(ceil(flDistance)));
    return TRUE;
}


/// <summary>
/// Internally load default values. This function is used as a fall back option.
/// It does not change the registry values for the gesture metrics.
/// </summary>
/// <param name="pSettings">A GESTUREMETRICS structure receiving the default
/// fallback values.</param>
/// <returns>
/// <para>TRUE: success, FALSE: unknown gesture.</para>
/// </returns>
BOOL TouchGesture::GetDefaultSettings(GESTUREMETRICS* pSettings)
{
    // Distance tolerance is stored in "config units" which are 1000th of an inch
    DWORD dwDistanceToleranceInConfigUnits = 0;

    switch (pSettings->dwID)
    {
    case GID_SELECT: 
        {
            pSettings->dwTimeout = min(DEFAULT_SELECT_TIMEOUT_MS, s_Settings.dwHoldTimeout); 
            dwDistanceToleranceInConfigUnits = DEFAULT_SELECT_DISTANCE;
            pSettings->dwAngularTolerance = 0;                                          // not used
            pSettings->dwExtraInfo = 0;                                                 // not used
            break;
        }
    case GID_DOUBLESELECT: 
        {
            pSettings->dwTimeout = DEFAULT_DOUBLESELECT_TIMEOUT_MS;
            dwDistanceToleranceInConfigUnits = DEFAULT_DOUBLESELECT_DISTANCE;
            pSettings->dwAngularTolerance = 0;                                          // not used
            pSettings->dwExtraInfo = 0;                                                 // not used
            break;
        }
    case GID_HOLD: 
        {
            pSettings->dwTimeout = max(s_Settings.dwTapTimeout, DEFAULT_HOLD_TIMEOUT_MS);
            dwDistanceToleranceInConfigUnits = DEFAULT_HOLD_DISTANCE;
            pSettings->dwAngularTolerance = 0;                                          // not used
            pSettings->dwExtraInfo = 0;                                                 // not used    
            break;
        }
    case GID_SCROLL:
        {
            pSettings->dwTimeout = DEFAULT_SCROLL_TIMEOUT_MS;
            dwDistanceToleranceInConfigUnits = DEFAULT_SCROLL_DISTANCE;
            pSettings->dwAngularTolerance = GID_ROTATE_ANGLE_TO_ARGUMENT(DEFAULT_FLICK_ANGLE_TOLERANCE);
            pSettings->dwExtraInfo = GID_ROTATE_ANGLE_TO_ARGUMENT(DEFAULT_FLICK_DIRECTION_TOLERANCE);
            break;
        }
    case GID_PAN:
        {
            pSettings->dwTimeout = 0;                                                  // not used
            // make sure this is >= tap and hold ranges
            dwDistanceToleranceInConfigUnits = DEFAULT_PAN_DISTANCE;
            pSettings->dwAngularTolerance = 0;                                         // not used
            pSettings->dwExtraInfo = 0;                                                // not used
            break;
        }
    default:
        {
            //we simply ignore unknown values
            ASSERT(FALSE);
            return FALSE;
        }
    }

    // The distance tolerance is specified in config units - we need to convert it to pixels.
    // This should not fail. If it does, we assert, and attempt to use the value unchanged (which
    // will be bad, but better than not booting)
    if (FAILED(ConvertConfigUnitsToPixels(dwDistanceToleranceInConfigUnits, &pSettings->dwDistanceTolerance)))
    {
        ASSERT(FALSE);
        // This should not happen - it should be possible to convert any valid value
        RETAILMSG(TRUE, (TEXT("Invalid default distance tolerance %lu for gesture %lu!!\r\n"), dwDistanceToleranceInConfigUnits, pSettings->dwID));

        // Fallback to the original value
        pSettings->dwDistanceTolerance = dwDistanceToleranceInConfigUnits;
    }

    return TRUE;
}


/// <summary>
/// Apply new metrics to the gesture recognizer and store them in the 
/// registry for future reference.
/// </summary>
/// <param name="pSettings">A GESTUREMETRICS structure as outlined in SystemParametersInfo API</param>
/// <param name="fUpdateRegistry">A BOOL flag that tells whether to update the values in registry or not</param>
/// <param name="fValidate">Specifies whether or not the settings should be validated</param>
/// <returns>
/// <para>HRESULT indicating success or failure</para>
/// </returns>
HRESULT TouchGesture::SetSettings(GESTUREMETRICS* pSettings, BOOL fUpdateRegistry, BOOL fValidate)
{
    LONG lRes;
    HKEY hKeySettings = NULL;
    HKEY hKeyGesture = NULL;
    DWORD dwDisposition;
    TCHAR pszGestureKey[9];
    DWORD dwDistanceToleranceInConfigUnits = 0;
    HRESULT hr = S_OK;

    //first, check the parameters
    CBREx(((NULL != pSettings) && (sizeof(GESTUREMETRICS) == pSettings->cbSize)), E_INVALIDARG);

    // The following gestures need to convert the dwDistanceTolerance into
    // touch units. We perform that conversion up front since it can fail.
    // We don't do the conversion for any other gesture, since the value
    // is not used, so it is allowed to be an illegal value. Even though we
    // only use this value when saving to the registry, we perform the conversion
    // for every set, since it would be painful to only fail if fUpdateRegistry
    // was TRUE.
    if (pSettings->dwID == GID_SELECT ||
        pSettings->dwID == GID_DOUBLESELECT ||
        pSettings->dwID == GID_HOLD ||
        pSettings->dwID == GID_SCROLL ||
        pSettings->dwID == GID_PAN )
    {
        CHR(ConvertPixelsToConfigUnits(pSettings->dwDistanceTolerance, &dwDistanceToleranceInConfigUnits));
    }
    
    if (fValidate)
    {
        switch (pSettings->dwID)
        {
        case GID_SELECT: 
            {
                //verify parameters.
                CBREx((s_Settings.dwHoldTimeout > pSettings->dwTimeout), E_INVALIDARG);
                CBREx((s_Settings.dwPanRange >= pSettings->dwDistanceTolerance), E_INVALIDARG);
                CBREx((s_Settings.dwHoldRange >= pSettings->dwDistanceTolerance), E_INVALIDARG);
                break;
            }
        case GID_DOUBLESELECT: 
            {
                break;
            }
        case GID_HOLD: 
            {
                //verify parameters.
                CBREx((s_Settings.dwTapTimeout < pSettings->dwTimeout), E_INVALIDARG);
                CBREx((s_Settings.dwPanRange >= pSettings->dwDistanceTolerance), E_INVALIDARG);
                CBREx((s_Settings.dwTapRange <= pSettings->dwDistanceTolerance), E_INVALIDARG);
                break;
            }
        case GID_SCROLL:
            {
                //verify parameters.
                CBREx((0 < pSettings->dwTimeout), E_INVALIDARG);
                CBREx((s_Settings.dwPanRange < pSettings->dwDistanceTolerance), E_INVALIDARG);
                break;
            }
        case GID_PAN:
            {
                //verify parameters
                CBREx((s_Settings.dwHoldRange <= pSettings->dwDistanceTolerance), E_INVALIDARG);
                CBREx((s_Settings.dwTapRange <= pSettings->dwDistanceTolerance), E_INVALIDARG);
                CBREx((s_Settings.dwFlickDistanceTolerance > pSettings->dwDistanceTolerance), E_INVALIDARG);
                break;
            }
        default:
            {
                //we simply ignore unknown values
                break;
            }
        }
    }

    if(TRUE == fUpdateRegistry)
    {
        //store the content of the struct in the registry
        //open the regkey and store every gesture's parameters seperatly
        lRes = RegCreateKeyEx(HKEY_CURRENT_USER, REG_szTouchGestureSettingsRegKey, 0, NULL,
                                0, KEY_WRITE, NULL, &hKeySettings, &dwDisposition);
        CBR((ERROR_SUCCESS == lRes) && (NULL != hKeySettings));

        //create the gesture specific key
        CHR(StringCchPrintf(pszGestureKey, ARRAYSIZE(pszGestureKey), TEXT("%08X"), pSettings->dwID));
        lRes = RegCreateKeyEx(hKeySettings, pszGestureKey, 0, NULL,
                                0, KEY_WRITE, NULL, &hKeyGesture, &dwDisposition);
        CBR((ERROR_SUCCESS == lRes) && (NULL != hKeyGesture));

        //store the values
        //timeout
        lRes = RegSetValueEx(hKeyGesture, 
                            (LPCTSTR)REG_szTouchGestureTimeoutValue, 0,
                            REG_DWORD, 
                            (LPBYTE) &pSettings->dwTimeout, 
                            sizeof(pSettings->dwTimeout));
        CBR(ERROR_SUCCESS == lRes);

        //distance tolerance
        // The distance tolerance is recorded in "config units" which are defined
        // as 1000th of an inch.
        lRes = RegSetValueEx(hKeyGesture, 
                            (LPCTSTR)REG_szTouchGestureDistanceToleranceValue, 0,
                            REG_DWORD, 
                            (LPBYTE) &dwDistanceToleranceInConfigUnits, 
                            sizeof(dwDistanceToleranceInConfigUnits));
        CBR(ERROR_SUCCESS == lRes);

        //angular tolerance
        lRes = RegSetValueEx(hKeyGesture, 
                            (LPCTSTR)REG_szTouchGestureAngularToleranceValue, 0,
                            REG_DWORD, 
                            (LPBYTE) &pSettings->dwAngularTolerance, 
                            sizeof(pSettings->dwAngularTolerance));
        CBR(ERROR_SUCCESS == lRes);

        //extra info
        lRes = RegSetValueEx(hKeyGesture, 
                            (LPCTSTR)REG_szTouchGestureExtraInfoValue, 0,
                            REG_DWORD, 
                            (LPBYTE) &pSettings->dwExtraInfo, 
                            sizeof(pSettings->dwExtraInfo));
        CBR(ERROR_SUCCESS == lRes);
    }

    //finally apply the new values
    switch (pSettings->dwID)
    {
    case GID_SELECT: 
        {
            s_Settings.dwTapRange = pSettings->dwDistanceTolerance;
            s_Settings.dwTapTimeout = pSettings->dwTimeout;

            // Convert the screen coordinates to touch coordinates;
            s_dwTapRange = pSettings->dwDistanceTolerance *  TOUCH_SCALING_FACTOR;
            break;
        }
    case GID_DOUBLESELECT: 
        {
            s_Settings.dwDoubleTapRange = pSettings->dwDistanceTolerance;
            s_Settings.dwDoubleTapTimeout = pSettings->dwTimeout;

             // Convert the screen coordinates to touch coordinates;
            s_dwDoubleTapRange = pSettings->dwDistanceTolerance *  TOUCH_SCALING_FACTOR;
            break;
        }
    case GID_HOLD: 
        {
            s_Settings.dwHoldTimeout = pSettings->dwTimeout;
            s_Settings.dwHoldRange = pSettings->dwDistanceTolerance;

            // Convert the screen coordinates to touch coordinates;
            s_dwHoldRange = pSettings->dwDistanceTolerance *  TOUCH_SCALING_FACTOR;
            break;
        }
    case GID_SCROLL:
        {
            s_Settings.dwFlickTimeout = pSettings->dwTimeout;
            s_Settings.dwFlickDistanceTolerance = pSettings->dwDistanceTolerance;
            s_Settings.cFlickAngularTolerance = GID_ROTATE_ANGLE_FROM_ARGUMENT(pSettings->dwAngularTolerance);
            s_Settings.cFlickExtraInfo = GID_ROTATE_ANGLE_FROM_ARGUMENT(pSettings->dwExtraInfo);
            s_dwFlickTimeout = s_Settings.dwFlickTimeout = pSettings->dwTimeout;
             // Convert the screen coordinates to touch coordinates;
            s_dwFlickDistanceTolerance = pSettings->dwDistanceTolerance *  TOUCH_SCALING_FACTOR;
            s_cFlickAngleTolerance = s_Settings.cFlickAngularTolerance;
            s_cFlickDirectionTolerance = s_Settings.cFlickExtraInfo;
            break;
        }
    case GID_PAN:
        {
            s_Settings.dwPanRange = pSettings->dwDistanceTolerance;

            // Convert the screen coordinates to touch coordinates;
            s_dwPanRange = pSettings->dwDistanceTolerance *  TOUCH_SCALING_FACTOR;
            break;
        }
    default:
        {
            //we simply ignore unknown values
            break;
        }
    }

Error:
    if (NULL != hKeySettings)
    {
        RegCloseKey(hKeySettings);
    }

    if (NULL != hKeyGesture)
    {
        RegCloseKey(hKeyGesture);
    }

    if(E_INVALIDARG == hr)        
    {
        SetLastError(ERROR_INVALID_PARAMETER);
    }

    return hr;
}

/// <summary>
/// Read current gesture metrics from the structure in memory.
/// </summary>
/// <param name="pSettings">A GESTUREMETRICS structure as outlined in SystemParametersInfo API</param>
/// <returns>
/// <para>HRESULT indicating success or failure</para>
/// </returns>
HRESULT TouchGesture::GetSettings(GESTUREMETRICS* pSettings)
{
    HRESULT hr = S_OK;

    //first, check the parameters
    CBREx(((NULL != pSettings) && (sizeof(GESTUREMETRICS) == pSettings->cbSize)), E_INVALIDARG);

    //finally apply the new values
    switch (pSettings->dwID)
    {
    case GID_SELECT: 
        {
            pSettings->dwTimeout = s_Settings.dwTapTimeout;
            pSettings->dwDistanceTolerance = s_Settings.dwTapRange;
            pSettings->dwAngularTolerance = 0;
            pSettings->dwExtraInfo = 0;
            break;
        }
    case GID_DOUBLESELECT: 
        {
            pSettings->dwTimeout = s_Settings.dwDoubleTapTimeout;
            pSettings->dwDistanceTolerance = s_Settings.dwDoubleTapRange;
            pSettings->dwAngularTolerance = 0;
            pSettings->dwExtraInfo = 0;
            break;
        }
    case GID_HOLD: 
        {
            pSettings->dwTimeout = s_Settings.dwHoldTimeout;
            pSettings->dwDistanceTolerance = s_Settings.dwHoldRange;
            pSettings->dwAngularTolerance = 0;
            pSettings->dwExtraInfo = 0;
            break;
        }
    case GID_SCROLL:
        {
            pSettings->dwTimeout = s_Settings.dwFlickTimeout;
            pSettings->dwDistanceTolerance = s_Settings.dwFlickDistanceTolerance;
            pSettings->dwAngularTolerance = GID_ROTATE_ANGLE_TO_ARGUMENT(s_Settings.cFlickAngularTolerance);
            pSettings->dwExtraInfo = GID_ROTATE_ANGLE_TO_ARGUMENT(s_Settings.cFlickExtraInfo);
            break;
        }
    case GID_PAN:
        {
            pSettings->dwTimeout = 0;
            pSettings->dwDistanceTolerance = s_Settings.dwPanRange;
            pSettings->dwAngularTolerance = 0;
            pSettings->dwExtraInfo = 0;
            break;
        }
    default:
        {
            //we simply ignore unknown values
            break;
        }
    }

Error:
    if(E_INVALIDARG == hr)        
    {
        SetLastError(ERROR_INVALID_PARAMETER);
    }

    return hr;
}


HRESULT TouchGesture::InternalRecognizeGesture(PCCETOUCHINPUT pInputs, UINT cInputs)
{
    HRESULT hr = S_OK;
    BOOL    fIsSymmetricPoints = FALSE;

    ClearType();

    int iXPrimary = INVALID_TOUCH_POINT;
    int iYPrimary = INVALID_TOUCH_POINT;
    int iXSecondary = INVALID_TOUCH_POINT;
    int iYSecondary = INVALID_TOUCH_POINT;
    DWORD dwTime = 0;

    enumInputAction eActionTypePrimary = eActionNone;
    enumInputAction eActionTypeSecondary = eActionNone;


    // first we look for our primary point

    for (UINT c = 0; c < cInputs; ++c)
    {
        if (iXPrimary != INVALID_TOUCH_POINT)
        {
            // we have found our primary and secondary point and set what action to take
            // so break and call the right function
            break;
        }
        if ((pInputs[c].dwFlags & TOUCHEVENTF_PRIMARY || pInputs[c].dwFlags & TOUCHEVENTF_SYMMETRIC) && 
            pInputs[c].hSource != INVALID_HANDLE_VALUE)
        {
            if (pInputs[c].dwFlags & TOUCHEVENTF_SYMMETRIC)
            {
                fIsSymmetricPoints = TRUE;
            }
            if (m_hPrimaryHandle == INVALID_HANDLE_VALUE)
            {   
                m_hPrimaryHandle = pInputs[c].hSource;
                m_dwPrimaryStreamID = pInputs[c].dwID;
            }
            else
            {
                if (m_dwPrimaryStreamID != pInputs[c].dwID || m_hPrimaryHandle != pInputs[c].hSource)
                {
                    // assume nothing for symmetric points
                    if (fIsSymmetricPoints && m_hPrimaryHandle == pInputs[c].hSource)
                    {
                        m_dwPrimaryStreamID = pInputs[c].dwID;
                    }
                    else
                    {
                        // uh oh, we should really not be in this state
                        ASSERT(FALSE);
                        RETAILMSG(1, (L"Invalid touch input points sent, primary stream not consistent\n"));
                        return (E_FAIL);
                    }
                }
            }
            iXPrimary = pInputs[c].x;
            iYPrimary = pInputs[c].y;
            if (pInputs[c].dwFlags & TOUCHEVENTF_DOWN)
            {
                eActionTypePrimary = eActionDown;
            }
            else if (pInputs[c].dwFlags & TOUCHEVENTF_UP)
            {
                eActionTypePrimary = eActionUp;
            }
            else if (pInputs[c].dwFlags & TOUCHEVENTF_MOVE)
            {
                eActionTypePrimary = eActionMove;
            }

            // We take the point timestamp from the primary point.
            if (pInputs[c].dwFlags & TOUCHINPUTMASKF_TIMEFROMSYSTEM)
            {
                dwTime = pInputs[c].dwTime;
            }
            else
            {
                // we cannot use the timestamp from the point since the timestamps must
                // be based on GetTickCount, so we use GetTickCount here to get the time
                // This isn't ideal, since the time ought to have been read before the
                // point went into the user input queue.
                dwTime = GetTickCount();
            }
            break;
        }
    }

    // if we did not find a primary point, then we ignore any secondary point
    if (m_hPrimaryHandle != INVALID_HANDLE_VALUE)
    {
        for (UINT c = 0; c < cInputs; ++c)
        {
            if ((iXPrimary != INVALID_TOUCH_POINT && iXSecondary != INVALID_TOUCH_POINT))
            {
                // we have found our primary and secondary point and set what action to take
                // so break and call the right function
                break;
            }
            if (m_dwPrimaryStreamID != pInputs[c].dwID && pInputs[c].hSource != INVALID_HANDLE_VALUE)
            {
                // if either we have not set our secondary point, or this value is the secondary point
                // then update the correct data
                if (m_hSecondaryHandle == INVALID_HANDLE_VALUE || (pInputs[c].dwID == m_dwSecondaryStreamID))
                {
                    iXSecondary = pInputs[c].x;
                    iYSecondary = pInputs[c].y;
                    if (pInputs[c].dwFlags & TOUCHEVENTF_DOWN)
                    {
                        // prioritize the down state for the secondary
                        eActionTypeSecondary = eActionDown;
                    }
                    else if (pInputs[c].dwFlags & TOUCHEVENTF_UP)
                    {
                        eActionTypeSecondary = eActionUp;
                    }
                    else if (pInputs[c].dwFlags & TOUCHEVENTF_MOVE)
                    {
                        eActionTypeSecondary = eActionMove;
                    }
                    if (m_hSecondaryHandle == INVALID_HANDLE_VALUE)
                    {
                        m_dwSecondaryStreamID = pInputs[c].dwID;
                        m_hSecondaryHandle = pInputs[c].hSource;
                    }
                }
            }
            else
            {
                continue;
            }
        }
    }
    if (iXPrimary == INVALID_TOUCH_POINT)
    {
        // we are receiving secondary points with no primary, that is bad
        RETAILMSG(1, (L"Invalid touch input points sent, secondary points sent without primary points sent\n"));
        return E_INVALIDARG;
    }

    // add the point if we have an action here
    if (eActionTypeSecondary == eActionNone && eActionTypePrimary == eActionNone)
    {
        // nothing to do, exit
        goto Exit;
    }
    // Note: using touch coordinates
    hr = AddPoint(iXPrimary, iYPrimary, iXSecondary, iYSecondary, dwTime);
    CHR(hr);
    // order of handling is important here.  We want to handle primary down, followed
    // by seocndary down, followed by secondary up, followed by any move, followed by 
    // primary up
    if (eActionTypePrimary == eActionDown)
    {
        CHR(HandleTouchDown(iXPrimary, iYPrimary, iXSecondary, iYSecondary));
        if (IsGesturePending())
        {
            SendEvent();
        }
    }
    if (eActionTypeSecondary == eActionDown)
    {
        CHR(HandleSecondaryTouchDown(iXPrimary, iYPrimary, iXSecondary, iYSecondary));
        if (IsGesturePending())
        {
            SendEvent();
        }
    }
    if (eActionTypeSecondary == eActionUp)
    {
        CHR(HandleSecondaryTouchUp(iXPrimary, iYPrimary, iXSecondary, iYSecondary));
        if (IsGesturePending())
        {
            SendEvent();
        }
    }
    if (eActionTypePrimary == eActionMove || eActionTypeSecondary == eActionMove)
    {
        CHR(HandleTouchMove(iXPrimary, iYPrimary, iXSecondary, iYSecondary));
        if (IsGesturePending())
        {
            SendEvent();
        }
    }
    if (eActionTypePrimary == eActionUp)
    {
        CHR(HandleTouchUp(iXPrimary, iYPrimary, iXSecondary, iYSecondary));
        if (IsGesturePending())
        {
            SendEvent();
        }

        // If any gesture was sent, send GID_END
        if (m_cGesturesInSession)
        {
            m_dwAdditionalInfoHiDWORD = 0;
            m_dwAdditionalInfo = 0L;
            m_ptPanStart.iXPrimary = 0;
            m_ptPanStart.iYPrimary = 0;
            SetType(GID_END, FALSE, FALSE);
            SendEvent();
        }
    }
Error:
Exit:
    return hr;
}




/// <summary>
/// Read current gesture metrics from the from the registry.
/// </summary>
/// <param name="pSettings">A GESTUREMETRICS structure as outlined in SystemParametersInfo API</param>
/// <returns>
/// <para>TRUE: success, FALSE: failure, call GetLastError()</para>
/// </returns>
BOOL TouchGesture::GetSettingsFromRegistry(GESTUREMETRICS* pSettings)
{
    LONG lRes;
    HKEY hKeySettings = NULL;
    HKEY hKeyGesture = NULL;
    TCHAR pszGestureKey[9];
    HRESULT hr = S_OK;
    DWORD dwType = REG_DWORD;
    DWORD dwValue;
    DWORD cbValue;
    
    //first, check the parameters
    CBREx(((NULL != pSettings) && (sizeof(GESTUREMETRICS) == pSettings->cbSize)), E_INVALIDARG);

    // Get the default settings for this gesture
    (void)GetDefaultSettings(pSettings);

    //load the content of the struct in the registry
    //open the regkey and load every gesture's parameters seperatly
    lRes = RegOpenKeyEx(HKEY_CURRENT_USER, REG_szTouchGestureSettingsRegKey, 0, 0,
                        &hKeySettings);
    CBR((ERROR_SUCCESS == lRes) && (NULL != hKeySettings));

    //create the gesture specific key
    CHR(StringCchPrintf(pszGestureKey, ARRAYSIZE(pszGestureKey), TEXT("%08X"), pSettings->dwID));
    lRes = RegOpenKeyEx(hKeySettings, pszGestureKey, 0, 0, &hKeyGesture);
    CBR((ERROR_SUCCESS == lRes) && (NULL != hKeyGesture));

    //load the values
    //timeout
    cbValue = sizeof(dwValue);
    lRes = RegQueryValueEx(hKeyGesture, 
                        (LPCTSTR)REG_szTouchGestureTimeoutValue, 0,
                        &dwType, 
                        (LPBYTE)&dwValue, 
                        &cbValue);
    if (ERROR_SUCCESS == lRes &&
        REG_DWORD == dwType &&
        sizeof(dwValue) == cbValue)
    {
        pSettings->dwTimeout = dwValue;
    }

    //distance tolerance
    // Distance tolerance is stored in "config units" which are 1000th of an inch
    cbValue = sizeof(dwValue);
    lRes = RegQueryValueEx(hKeyGesture, 
                        (LPCTSTR)REG_szTouchGestureDistanceToleranceValue, 0,
                        &dwType, 
                        (LPBYTE)&dwValue, 
                        &cbValue);
    if (ERROR_SUCCESS == lRes &&
        REG_DWORD == dwType &&
        sizeof(dwValue) == cbValue)
    {
        // Internally we track the configuration in pixels. If we read an invalid value
        // here, the default value in dwDistanceTolerance is left unchanged.
        (void)ConvertConfigUnitsToPixels(dwValue, &pSettings->dwDistanceTolerance);
    }

    //angular tolerance
    cbValue = sizeof(dwValue);
    lRes = RegQueryValueEx(hKeyGesture, 
                        (LPCTSTR)REG_szTouchGestureAngularToleranceValue, 0,
                        &dwType, 
                        (LPBYTE)&dwValue, 
                        &cbValue);
    if (ERROR_SUCCESS == lRes &&
        REG_DWORD == dwType &&
        sizeof(dwValue) == cbValue)
    {
        pSettings->dwAngularTolerance = dwValue;
    }

    //extra info
    cbValue = sizeof(dwValue);
    lRes = RegQueryValueEx(hKeyGesture, 
                        (LPCTSTR)REG_szTouchGestureExtraInfoValue, 0,
                        &dwType, 
                        (LPBYTE)&dwValue, 
                        &cbValue);
    if (ERROR_SUCCESS == lRes &&
        REG_DWORD == dwType &&
        sizeof(dwValue) == cbValue)
    {
        pSettings->dwExtraInfo = dwValue;
    }

    // SCROLL has a special reg key for the number of Pan Points
    if (pSettings->dwID == GID_SCROLL)
    {
        cbValue = sizeof(dwValue);
        lRes = RegQueryValueEx(hKeyGesture, 
                            (LPCTSTR)REG_szTouchGesturePanPointCountValue, 0,
                            &dwType, 
                            (LPBYTE)&dwValue, 
                            &cbValue);
        if (ERROR_SUCCESS == lRes &&
            REG_DWORD == dwType &&
            sizeof(dwValue) == cbValue &&
            dwValue >= DEFAULT_FLICK_PAN_POINTS)
        {
            s_dwFlickPanPoints = dwValue;
        }
    }
    
Error:
    if (NULL != hKeySettings)
    {
        RegCloseKey(hKeySettings);
    }

    if (NULL != hKeyGesture)
    {
        RegCloseKey(hKeyGesture);
    }

    if(E_INVALIDARG == hr)        
    {
        SetLastError(ERROR_INVALID_PARAMETER);
    }

    return SUCCEEDED(hr);
}

extern "C" BOOL WINAPI DllMain(HANDLE hInstDll, DWORD fdwReason, LPVOID lpvReserved)
{
    UNREFERENCED_PARAMETER(lpvReserved);

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls((HMODULE)hInstDll);
            break;

        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

extern "C" HRESULT APIENTRY Init(const TOUCHRECOGNIZERAPIINIT* pInit)
{
    HRESULT hr = S_OK;

    CPRAEx(pInit, E_INVALIDARG);

    // Copy the init arguments we need
    g_pfnSetTimer = pInit->pfnSetTimer;
    g_pfnKillTimer = pInit->pfnKillTimer;

Error:
    return hr;
}

extern "C" HRESULT APIENTRY RecognizeGesture(PCCETOUCHINPUT pInputs, UINT cInputs)
{
    TouchGesture * pTouchGesture = TouchGesture::Instance();
    HRESULT hr = S_OK;

    CPR(pTouchGesture);
    CHR(pTouchGesture->InternalRecognizeGesture(pInputs, cInputs));
Error:
    return hr;
}

extern "C" HRESULT APIENTRY ConfigSettings(GESTUREMETRICS * pSettings, BOOL fSet, BOOL fUpdateRegistry)
{
    TouchGesture * pTouchGesture = TouchGesture::Instance();

    if (pTouchGesture == NULL)
    {
        return FALSE;
    }
    if (fSet)
    {
        // Want to set the settings
        return pTouchGesture->SetSettings(pSettings, fUpdateRegistry, TRUE);
    }
    // Want to get the settings
    return pTouchGesture->GetSettings(pSettings);
}
