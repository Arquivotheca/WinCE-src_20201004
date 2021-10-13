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

#pragma once

#include "bldver.h"

#if CE_MAJOR_VER <= 6
#include <tchddi_priv.h>
#else
#include <tchddi.h>
#endif

// Forward declaration
typedef struct tagGESTUREMETRICS GESTUREMETRICS, *LPGESTUREMETRICS; 

#define GID_NONE      0

#define INVALID_TOUCH_POINT  INT_MAX

#define REG_szTouchGestureSettingsRegKey            TEXT("ControlPanel\\Gestures\\Touch")
#define REG_szTouchGestureTimeoutValue              TEXT("Timeout")
#define REG_szTouchGestureDistanceToleranceValue    TEXT("DistanceTolerance")
#define REG_szTouchGesturePanPointCountValue        TEXT("ScrollPanPoints")
#define REG_szTouchGestureAngularToleranceValue     TEXT("AngularTolerance")
#define REG_szTouchGestureExtraInfoValue            TEXT("ExtraInfo")


class TouchGesture
{
private:
    struct CETOUCHPOINTS
    {
        // for this version, we are only going to be concerned with at most 2 points,
        // the primary point and a single secondary point
        int     iXPrimary;
        int     iYPrimary;
        int     iXSecondary;
        int     iYSecondary;
        DWORD   dwTime;
    };
    enum PANSTATE
    {
        ePANNONE,
        ePANSINGLEPOINT,
        ePANMULTIPOINT
    };
    HRESULT AddPoint(int iXPrimary, int iYPrimary, int iXSecondary, int iYSecondary, DWORD dwTime);
    void    EliminateTimeoutPoints();
    HRESULT GrowBuffer();
    static void    GrowRect(RECT *pRect, int x, int y);
    void    GrowGestureRect(int x, int y);
    float Distance(int x1, int x2, int y1, int y2){return (float)sqrt( ((x1 - x2) * (x1 - x2)) + ((y1 - y2) * (y1 - y2)));};
    HRESULT RecognizeFlick();
    BOOL FindFlickStartPoints(
        __out int* piStart, 
        __out int* piVelocityPoint);

    void    SetStartPoint(int xPrimary, int yPrimary, int xSecondary, int ySecondary, DWORD dwTime, BOOL fSetTimer);
    BOOL    SetPanPoints(BOOL fForce);
    BOOL    SetMultitouchPanPoints(BOOL fForce);
    inline int sqr(int a) { return (a * a);}
    inline double sqr(double a) { return (a * a);}
    inline int PtIndexDecrement(int index) { return (--index < 0) ? (m_cPoints - 1) : index; }
    inline int PtIndexDecrementBy(int index, int iDecrement) { return (index - (iDecrement % m_cPoints) < 0) ? (m_cPoints - (iDecrement % m_cPoints)  + index) : (index - (iDecrement % m_cPoints));}
    inline int PtIndexIncrement(int index) { return (++index >= m_cPoints) ? 0 : index; }
    inline int GetPointCount() { return GetPointCount(m_iHead); }
    inline int GetPointCount(int iStart) { return (m_iCur >= iStart) ? (m_iCur - iStart + 1) : (m_cPoints - iStart + m_iCur + 1); }

    int                  m_cPoints;
    CETOUCHPOINTS*       m_pPoints;
    CETOUCHPOINTS        m_ptDown;
    CETOUCHPOINTS        m_ptStart;
    CETOUCHPOINTS        m_ptPanStart;
    CETOUCHPOINTS        m_ptPanEnd;
    POINTS               m_ptsLastPanPoints;
    ULONGLONG            m_ullLastPanAdditionalInfo;
    int                  m_iHead;
    int                  m_iCur;
    UINT                 m_uType;
    DWORD                m_dwHoldTimerId;
    DWORD                m_dwTapTimerId;
    UINT                 m_cGesturesInSession;
    RECT                 m_rcGesture;
    RECT                 m_rcGestureSecondaryPoint;
    RECT                 m_rcDoubleTap;
    DWORD                m_dwEndTime;
    DWORD                m_dwTouchInputTime; // time of the current touch input point
    DWORD                m_dwAdditionalInfo;  // If the m_uType is FLICK, this parameter will contains angle info.
    DWORD                m_dwAdditionalInfoHiDWORD;  
    DWORD                m_dwFlags;
    DWORD                m_dwDoubleTapStart;
    DWORD                m_dwEndAdditionalInfo; // when sending a compound gesture (end, followed by a new gesture) we need this to hold the additionalinfo for the end
    DWORD                m_dwPrimaryStreamID;
    DWORD                m_dwSecondaryStreamID;
    HANDLE               m_hPrimaryHandle;
    HANDLE               m_hSecondaryHandle;
    CETOUCHPOINTS        m_ptEndStartPoint;          // Point for compound End gesture
    PANSTATE             m_ePanState;
    BOOL        m_fInProgress : 1;
    BOOL        m_fCouldBeTap : 1;
    BOOL        m_fCouldBeFlick : 1;
    BOOL        m_fHoldStarted: 1;
    static const int  c_nBufferGrowth;
    static DWORD  s_dwFlickDistanceTolerance;
    static DWORD  s_dwTapRange;
    static DWORD  s_dwDoubleTapRange;
    static DWORD  s_dwHoldRange;
    static DWORD  s_dwPanRange;
    static DWORD  s_dwFlickTimeout;
    static double s_cFlickAngleTolerance;
    static double s_cFlickDirectionTolerance;
    static DWORD  s_dwFlickPanPoints;    // Number of pan points to consider for the velocity calculation for a Scroll

    struct SETTINGS
    {
        DWORD dwTapRange;                // The range of tap. In 100th inches.
        DWORD dwTapTimeout;              // The time out time for TAP. In ms.
        DWORD dwDoubleTapRange;          // The double tap tolerance. In 100th inches.
        DWORD dwDoubleTapTimeout;        // The double tap timeout. In ms.
        DWORD dwHoldRange;               // The range of hold. In 100th inches.
        DWORD dwHoldTimeout;             // The time out time for HOLD. In ms.
        DWORD dwPanRange;                // The range of pan. In 100th inches.
        DWORD dwFlickTimeout;            // The time out for flick. In ms.
        DWORD dwFlickDistanceTolerance;  // The tolerance allowed of flick gesture. In 100th inches.
        double cFlickAngularTolerance;   // The tolerance allowed for flick gesture in terms of angular movement along the direction of flick.
        double cFlickExtraInfo;          // Extra Information with the flick gesture.
    };

    TouchGesture();
    ~TouchGesture();

    UINT    GetType() { return m_uType; }
    BOOL    IsGesturePending() {return m_uType != GID_NONE;}
    void    SetType(UINT uType, BOOL fInitial, BOOL fTerminal);
    void    Init();
    HRESULT HandleTimer(DWORD dwTimerId);
    HRESULT HandleTouchDown(int xPrimary, int yPrimary, int xSecondary, int ySecondary);
    HRESULT HandleTouchMove(int xPrimary, int yPrimary, int xSecondary, int ySecondary);
    HRESULT HandleTouchUp(int xPrimary, int yPrimary, int xSecondary, int ySecondary);
    HRESULT HandleSecondaryTouchDown(int xPrimary, int yPrimary, int xSecondary, int ySecondary);
    HRESULT HandleSecondaryTouchUp(int xPrimary, int yPrimary, int xSecondary, int ySecondary);

    HRESULT StartTapTimer(DWORD dwTimeout);
    HRESULT StartHoldTimer(DWORD dwTimeout);
    void KillTapTimer();
    void KillHoldTimer();

    static void CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime);

    static BOOL GetSettingsFromRegistry(GESTUREMETRICS* pSettings);
    static BOOL GetDefaultSettings(GESTUREMETRICS* pSettings);

    void    ClearType() { m_uType = GID_NONE; }
    BOOL    SendEvent(void);

    static SETTINGS s_Settings;
    static TouchGesture* s_pTouchGesture;

public:
    static TouchGesture * Instance();

    HRESULT InternalRecognizeGesture(PCCETOUCHINPUT pInputs, UINT cInputs);
    HRESULT SetSettings(GESTUREMETRICS* pSettings, BOOL fUpdateRegistry, BOOL fValidate);
    HRESULT GetSettings(GESTUREMETRICS* pSettings);
};


