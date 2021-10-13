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

#include "bldver.h"
#include <cmsgque.h>
#include <gesture_priv.h>
#include <touchgesture.h>

#if CE_MAJOR_VER <= 6
#include <tchddi_priv.h>
#else
#include <tchddi.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif

typedef HRESULT (APIENTRY *LPINITPROC)(const TOUCHRECOGNIZERAPIINIT* pInit);
typedef HRESULT (APIENTRY *LPRECOGNIZEGESTUREPROC)(PCCETOUCHINPUT pInputs, UINT cInputs);
typedef HRESULT (APIENTRY *LPCONFIGSETTINGSPROC)(GESTUREMETRICS* pSettings, BOOL fSet, BOOL fUpdateRegistry);

#ifdef __cplusplus
}
#endif

#define REG_szTouchExtensionsRegKey                 TEXT("System\\GWE\\Recognizers")

#define MAX_RECOGNIZERS         10

inline ULONGLONG MakeFlickArguments(DWORD dwFlickAngle, USHORT usFlickSpeed, BYTE bFlickOrientation)
{
    ULARGE_INTEGER uliRet;
    uliRet.HighPart = MAKELONG(usFlickSpeed, (bFlickOrientation & 0x000F) | (dwFlickAngle & 0xFFF0));
    uliRet.LowPart = 0;
    return uliRet.QuadPart;
}

struct GESTUREPROC
{
    HMODULE hModule;
    DWORD dwOrder;
    LPRECOGNIZEGESTUREPROC lpRecognizeGesture;
    LPCONFIGSETTINGSPROC lpConfigSettings;
};


class GestureRecognizer
{
    static MsgQueue*            s_pmsgq;
    static BOOL                 s_fRecognizerInitialized;
    static UINT                 s_cRecognizers;
    static GESTUREPROC          s_rgRecognizers[MAX_RECOGNIZERS];
    static LPCWSTR*             s_ppszGestureNames;
    static UINT                 s_cGestureNames;
    static UINT                 s_cRegisteredGestures;
    static HWND                 s_hwndHook;

public:
    GestureRecognizer() {};
    ~GestureRecognizer() {};

    static BOOL WINAPI ConfigSettings_I(GESTUREMETRICS* pSettings, DWORD dwSource, BOOL fSet, BOOL fUpdateRegistry);
    static BOOL WINAPI EnableGestures_I(
                            HWND hwnd,
                            ULONGLONG *pullFlags,
                            UINT uScope);
    static BOOL WINAPI DisableGestures_I(
                            HWND hwnd,
                            ULONGLONG *pullFlags,
                            UINT uScope);
    static BOOL WINAPI QueryGestures_I(
                            HWND hwnd,
                            UINT uScope,
                            __out PULONGLONG pullFlags);
    static BOOL WINAPI RegisterGesture_I(
#if CE_MAJOR_VER <= 6
                            const __nullterminated LPCWSTR pszName,
#else
                            const __in_z LPCWSTR pszName,
#endif
                            __out PDWORD_PTR pdwID);
    static BOOL WINAPI RegisterDefaultGestureHandler_I(
                            HWND hwnd,
                            BOOL fRegister);
    static HRESULT RecognizeTouch(PCCETOUCHINPUT pInputs, UINT cInputs);
    static void SetTouchTargetMsgQueue(MsgQueue* pmsgq);
    static void MessageQueueTerminated(MsgQueue* pmsgq);
    static HWND GetHookWindow(void);

    static BOOL Gesture_I(
        HWND hwnd,
        __in_bcount(cbInfo) const GESTUREINFO* pGestureInfo,
        UINT cbInfo,
        __in_bcount_opt(cbExtraArguments) const BYTE* pExtraArguments,
        UINT cbExtraArguments);
    
    static HRESULT MergeGestureInfo(__in HGESTUREINFO hSourceInfo, __out HGESTUREINFO hTargetInfo);

private:
    static void LoadCustomRecognizers();
    static void RegisterCoreGestures(void);
    static HRESULT LoadRecognizer(LPCTSTR pszDllPath, DWORD dwOrder);

    static void Init(void);
    static BOOL InternalEnableDisableGestures(HWND hwnd, ULONGLONG *pullFlags, UINT uScope, BOOL fDisable);
};

