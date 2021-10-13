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

#ifndef __SCREEN_TRANSITION_H__
#define __SCREEN_TRANSITION_H__

#include <XRCommon.h>
#include <xamlruntime.h>
#include <XRPtr.h>
#include <XRCustomControl.h>
#include <perftestutils.h>

#define CEPERF_ENABLE
#include <ceperf.h>
#undef TRACE //windowsqa.h defines TRACE

#define XAML_PATH_FORMAT                XRPERF_RESOURCE_DIR L"ScreenTransition\\%s"
#define ST_TIMER_ID                     200

// Tests cases
//
#define ID_TC_SLIDE_WIZ_IN_TREE         1
#define ID_TC_ZOOM_WIZ_IN_TREE          2
#define ID_TC_WIZ_ON_DEMAND             3
#define ID_TC_SLIDE_CMPLX_IN_TREE       4
#define ID_TC_ZOOM_CMPLX_IN_TREE        5
#define ID_TC_CMPLX_ON_DEMAND           6

//////////////////////////////////////////////////////////////////////////
// Complex Screens Custom User Controls
//////////////////////////////////////////////////////////////////////////
class __declspec(uuid("{182B9500-3CB8-4495-B4B4-F87A9AB3C08A}")) Screen1:public XRCustomUserControlImpl<Screen1>
{
   QI_IDENTITY_MAPPING(Screen1, XRCustomUserControlImpl)

public:
    static HRESULT GetXamlSource(XRXamlSource* pXamlSource);
    static HRESULT Register();
};

class __declspec(uuid("{80B7AD3D-01E5-40F3-B710-31908F681AB2}")) Screen2:public XRCustomUserControlImpl<Screen2>
{
   QI_IDENTITY_MAPPING(Screen2, XRCustomUserControlImpl)

public:
    static HRESULT GetXamlSource(XRXamlSource* pXamlSource);
    static HRESULT Register();
};

//////////////////////////////////////////////////////////////////////////
// Wizard Pages Custom User Controls
//////////////////////////////////////////////////////////////////////////
class __declspec(uuid("{bbd67649-6342-4de8-85d4-f877830f25a5}")) Page1:public XRCustomUserControlImpl<Page1>
{
   QI_IDENTITY_MAPPING(Page1, XRCustomUserControlImpl)

public:
    static HRESULT GetXamlSource(XRXamlSource* pXamlSource);
    static HRESULT Register();
};

class __declspec(uuid("{638865bc-a85b-46f6-8618-affe67cd6898}")) Page2:public XRCustomUserControlImpl<Page2>
{
   QI_IDENTITY_MAPPING(Page2, XRCustomUserControlImpl)

public:
    static HRESULT GetXamlSource(XRXamlSource* pXamlSource);
    static HRESULT Register();
};

class ScreenTransitionEventHandler
{
public:

    int m_Result;
    bool m_bStateNormal;
    XRPtr<Screen1> m_pScreen1;
    XRPtr<Screen2> m_pScreen2;
    XRPtr<Page1> m_pPage1;
    XRPtr<Page2> m_pPage2;

    ScreenTransitionEventHandler(bool bState, Page1* pP1, Page2* pP2, Screen1* pS1, Screen2* pS2)
    {
        m_bStateNormal = bState;
        m_pPage1 = pP1;
        m_pPage2 = pP2;
        m_pScreen1 = pS1;
        m_pScreen2 = pS2;
        m_Result = TPR_PASS;
    }

    ~ScreenTransitionEventHandler()
    {}

    HRESULT OnEventVisualStateChanged(IXRDependencyObject* pSender, XRValueChangedEventArgs<IXRVisualState*>* pArgs);

};

VOID CALLBACK TimerProc_EndThread(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

#endif // __SCREEN_TRANSITION_H__