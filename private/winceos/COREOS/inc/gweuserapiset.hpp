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

#ifndef __GWEUSER_API_SET_HPP_INCLUDED__
#define __GWEUSER_API_SET_HPP_INCLUDED__

class GweUserApiSet_t
{
public:

    typedef
    void
    (*GweUser_NotifyCallback_t)(
        DWORD       cause,
        HPROCESS    hprc,
        HTHREAD     hthd
        );

    typedef
    BOOL
    (*GweUser_StartupScreenSetup_t)(
        BOOL fBypassWantStartupScreen
        );

    typedef
    void
    (*GweUser_StartupScreenFinish_t)(
        void
        );

    typedef
    void
    (*GweUser_StartupPowerOffWhileStartupActiveNotification_t)(
        void
        );

    typedef
    BOOL
    (*GweUser_InitializeStartupUI_t)(
        HANDLE hStartupScreenActivationEvent,
        HANDLE hStartupScreenActivationAckEvent,
        DWORD *pStartupScreenThreadID
        );

    typedef
    void
    (*GweUser_CleanupStartupUI_t)(
        void
        );

    typedef
    BOOL
    (*GweUser_InitializeOomUI_t)(
        DWORD   OkToKillWindowInfoListPtr,
        DWORD   CloseWindowWndHandlePtr,
        HANDLE  OomUICallbackEventHandle,
        HANDLE  OomUICallbackAckEventHandle,
        HANDLE  CallOomUIEventHandle,
        HANDLE  CallOomUIAckEventHandle,
        HWND   *pOomWindowHandle,
        HWND   *pNotRespondingWindowHandle,
        DWORD  *pOomThreadId
        );

    typedef
    void
    (*GweUser_CleanupOomUI_t)(
        void
        );


    typedef
    BOOL
    (*GweUser_CalibrateUIDrawMainScreen_t)(
        void
        );


    typedef
    BOOL
    (*GweUser_CalibrateUIDrawConfirmationScreen_t)(
        void
        );


    typedef
    BOOL
    (*GweUser_CalibrateUIWaitForConfirmation_t)(
        void
        );


    typedef
    BOOL
    (*GweUser_CalibrateUIClearScreen_t)(
        void
        );


    typedef
    void
    (*GweUser_CleanupCalibrateUI_t)(
        void
        );


    typedef
    BOOL
    (*GweUser_InitializeCalibrateUI_t)(
        DWORD   TocuhCalibrateStatePtr,
        DWORD   CalibrateUserInputPtr,
        HANDLE  CalibrateUIEventHandle,
        HANDLE  CalibrateUIAckEventHandle,
        HANDLE   CalibrateUIDoneEventHandle,
        DWORD *pCalibrateScreenThreadID
        );

    typedef
    BOOL
    (*GweUser_TestPSLSecurity_t)(
        void
        );

    GweUser_NotifyCallback_t        m_pGweUser_NotifyCallback;
    GweUser_StartupScreenSetup_t    m_pGweUser_StartupScreenSetup;
    GweUser_StartupScreenFinish_t   m_pGweUser_StartupScreenFinish;
    GweUser_StartupPowerOffWhileStartupActiveNotification_t m_pGweUser_StartupPowerOffWhileStartupActiveNotification;
    GweUser_InitializeStartupUI_t   m_pGweUser_InitializeStartupUI;
    GweUser_CleanupStartupUI_t      m_pGweUser_CleanupStartupUI;
    GweUser_InitializeOomUI_t       m_pGweUser_InitializeOomUI;
    GweUser_CleanupOomUI_t          m_pGweUser_CleanupOomUI;

    GweUser_CalibrateUIDrawMainScreen_t m_pGweUser_CalibrateUIDrawMainScreen;
    GweUser_CalibrateUIDrawConfirmationScreen_t m_pGweUser_CalibrateUIDrawConfirmationScreen;
    GweUser_CalibrateUIWaitForConfirmation_t m_pGweUser_CalibrateUIWaitForConfirmation;
    GweUser_CalibrateUIClearScreen_t m_pGweUser_CalibrateUIClearScreen;
    GweUser_CleanupCalibrateUI_t m_pGweUser_CleanupCalibrateUI;
    GweUser_InitializeCalibrateUI_t m_pGweUser_InitializeCalibrateUI;

    GweUser_TestPSLSecurity_t m_pGweUser_TestPSLSecurity;
    
};

#endif //__GWEUSER_API_SET_HPP_INCLUDED__

