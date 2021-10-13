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

#ifndef __GESTUREWND2_H__
#define __GESTUREWND2_H__

#include <shellsdk.h>
#include "defs2.h"
#include "utils2.h"
#include "GestureList2.h"

// AUX Window related WM messages
#define WM_USER_AUXISALIVE  WM_USER+1       // ACK after being created
#define WM_USER_DOACTION    WM_USER+2       // Used to make the AUX window do stuff

// AUX Window related wParams
#define AUXWNDWPARAM_FWDHANDLE          1


typedef HRESULT (__cdecl *SHRECOGNIZEGESTURE)(SHRGINFO * shrg); 
typedef HRESULT (__cdecl *SHFULLSCREEN)(HWND hwndRequester, DWORD dwState); 

class CGestureWnd 
{
public:
    // Constructor
    CGestureWnd(UINT nCSStyle = CS_DBLCLKS | CS_NOCLOSE, DWORD dwDelayMessageLoop = 0, HWND hWndParent = NULL);
    CGestureWnd(const TCHAR *lpszTitle, RECT rcRect, BOOL bShowSKBar, BOOL bShowTitlebar,
        UINT nCSStyle = CS_DBLCLKS | CS_NOCLOSE, DWORD dwDelayMessageLoop = 0, HWND hWndParent = NULL);
    ~CGestureWnd();

    // Data
    HWND                m_hwnd;
    HWND                m_hWndParent;
    DWORD               m_dwThreadID;
    HANDLE              m_hThread;
    HANDLE              m_hCreateEvent;
    HANDLE              m_hDestroyEvent;
    HANDLE              m_hWmGestureInfo;
    HANDLE              m_hWmGestureInfoPan;
    HANDLE              m_hWmGestureInfoMultiTouchPan;
    HANDLE              m_hWmGestureInfoScroll;
    HANDLE              m_hWmGestureInfoHold;
    HANDLE              m_hWmGestureInfoSelect;
    HANDLE              m_hWmGestureInfoDblSelect;
    HANDLE              m_hWmGestureInfoRotate;
    HANDLE              m_hWmGestureInfoCustom;
    HANDLE              m_hWmGestureInfoBegin;
    HANDLE              m_hWmGestureInfoEnd;
    HANDLE              m_hWmGestureInfoDirectManipulation;
    HANDLE              m_hWmSHRecognizeGesture;
    HANDLE              m_hSettingsChanged;
    HANDLE              m_hWindowPosChanged;
    HANDLE              m_hPostQuitEvent;
    HANDLE              m_hWmAuxWndStartup;
    HANDLE              m_hWmSettingsChanged;
    HANDLE              m_hTrackMessageArrivalComplete;
    
    BOOL                m_bIsAygshellLoaded;
    
    BOOL                m_bTrackMessageArrivalOrder;
    BOOL                m_bTrackedInputMessage;
    BOOL                m_bTrackedGestureMessage;
    enum                { NOT_TRACKED, TRACKED_INPUT_FIRST, TRACKED_GESTURE_FIRST } m_TrackedMessageResult;

    CGestureList        m_GestureList;
    
    HBRUSH              m_hWindowColor;

    // AUX Window 
    HWND                m_hwndAuxWindow;
    HWND                m_hwndCallingWindow;
    BOOL                m_bForwardGestureHandle;

    SHRECOGNIZEGESTURE  m_fSHRecognizeGesture;
    SHFULLSCREEN        m_fSHFullScreen;
    BOOL                m_bUseSHRecognizeGesture;
    BOOL                m_bHideSKBar;
    BOOL                m_bHideTitlebar;
    BOOL                m_bCorruptGestureMsgLParam;
    RECT                m_rcRect;
    RECT                m_rcWndRect;
    TCHAR               m_lpszTitle[MAX_PATH];
    DWORD               m_dwDelayMessageLoop;
    BOOL                m_bPrintGestureMessages;

    static BOOL         m_bChainGestures;
    static BOOL         m_bSetDirectManipulation;
    

    static const int    EVENT_DEFAULTTIMEOUT = 3000;

    // Function calls
    HRESULT SetScreenOrientation(DWORD dwDisplayOrientation, DWORD* pdwOldOrientation);
    HRESULT SetScreenOrientationPortrait(DWORD* pdwOldOrientation);
    HRESULT SetScreenOrientation90(DWORD* pdwOldOrientation);
    HRESULT SetScreenOrientation180(DWORD* pdwOldOrientation);
    HRESULT SetScreenOrientation270(DWORD* pdwOldOrientation);

    HRESULT EnableGestures_DirectManipulation();
    HRESULT EnableGestures_PanScrollMode();

    static DWORD WINAPI GestureWndMessageLoop(PVOID p);
    static LRESULT WINAPI GestureWndProc(HWND hwnd, UINT wm, WPARAM wParam, LPARAM lParam);
    static BOOL RegisterClass(UINT style, WNDPROC lpfnWndProc);

    BOOL EnableWindow(BOOL bEnable);
    BOOL EnableGestures(BOOL bEnable);
    
    void SetBackgroundColor(BYTE r, BYTE g, BYTE b);

protected:
    BOOL                m_fOwnThread;
    UINT                m_nWindowStyle;
    HMODULE             m_hAygshell;

    friend DWORD WINAPI GestureWndMessageLoop(PVOID);
    friend LRESULT WINAPI GestureWndProc(HWND, UINT, WPARAM, LPARAM);
    void InitializeEvents();
    void CleanupEvents();
    void Init();
    void AttemptLoadingAygshell();
    const TCHAR* DebugPrintOrientation(DWORD dwDisplayOrientation);
    HRESULT FullScreenWindow();
    void CreateMsgLoopThread();
    void UpdateMessageLoopTracking(MSG &msg);
};

#endif // __GESTUREWND_H__