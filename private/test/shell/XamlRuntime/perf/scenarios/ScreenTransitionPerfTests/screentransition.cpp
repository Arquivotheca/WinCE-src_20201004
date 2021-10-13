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

/*-------------------------------------------------------------------

Module Name:

    ScreenTransition.cpp

Abstract:


Environment:

    Win32 CE 


-------------------------------------------------------------------*/
#include <windows.h>
#include <string.h>
#include <PerfScenario.h>
#include "globals.h"
#include "TuxProc.h"
#include "ResManager.h"
#include "resource.h"
#include "ScreenTransition.h"
#include <XRDelegate.h>
#include <GetGuidUtil.h>

BITMAP_INFO BmpInfo[NUM_BITMAP] = {
        //AppWide
        { ID_WIZARD_PNG             ,L"ScreenTransition/HomeScreen6.png"},
        { ID_ALBUM_COVER_PNG        ,L"ScreenTransition/AlbumCoverExample.png"},
        { ID_MUSIC_IMAGE_PNG        ,L"ScreenTransition/wmpMLMusicAlbumImageBGBig.png"},
        { ID_RATE_SELECTED_PNG      ,L"ScreenTransition/wmpMLMusicRateSelected.png"},
        { ID_MEDIA_TITLE_PNG        ,L"ScreenTransition/wmpPBMediaTitleBG.png"},
        { ID_NEXT_PRESSED_PNG       ,L"ScreenTransition/wmpPBNextFFPressed.png"},
        { ID_LIST_SELECTED_PNG      ,L"ScreenTransition/wmpPBNPListSelected.png"},
        { ID_PLAY_PRESSED_PNG       ,L"ScreenTransition/wmpPBPlayPressed.png"},
        { ID_PREV_PRESSED_PNG       ,L"ScreenTransition/wmpPBPrevFRPressed.png"},
        { ID_REPEAT_PRESSED_PNG     ,L"ScreenTransition/wmpPBRepeatOnePressed.png"},
        { ID_SHUFFLE_SELECTED_PNG   ,L"ScreenTransition/wmpPBShuffleVFrameSelected.png"},
        { ID_SOUND_PRESSED_PNG      ,L"ScreenTransition/wmpPBSoundPressed.png"},
};

BITMAP_INFO BmpInfoBaml[NUM_BITMAP] = {
        //AppWide
        { ID_WIZARD_PNG_BAML            ,L"#501"},
        { ID_ALBUM_COVER_PNG_BAML       ,L"#502"},
        { ID_MUSIC_IMAGE_PNG_BAML       ,L"#503"},
        { ID_RATE_SELECTED_PNG_BAML     ,L"#504"},
        { ID_MEDIA_TITLE_PNG_BAML       ,L"#505"},
        { ID_NEXT_PRESSED_PNG_BAML      ,L"#506"},
        { ID_LIST_SELECTED_PNG_BAML     ,L"#507"},
        { ID_PLAY_PRESSED_PNG_BAML      ,L"#508"},
        { ID_PREV_PRESSED_PNG_BAML      ,L"#509"},
        { ID_REPEAT_PRESSED_PNG_BAML    ,L"#510"},
        { ID_SHUFFLE_SELECTED_PNG_BAML  ,L"#511"},
        { ID_SOUND_PRESSED_PNG_BAML     ,L"#512"},
};

//////////////////////////////////////////////////////////////////////////
// ScreenTransitionEventHandler functions
//////////////////////////////////////////////////////////////////////////
HRESULT ScreenTransitionEventHandler::OnEventVisualStateChanged(IXRDependencyObject* pSender, XRValueChangedEventArgs<IXRVisualState*>* pArgs)
{
    HRESULT hr = E_FAIL;

    if (m_bStateNormal)
    {
        m_bStateNormal = false;

        if (m_pScreen1 && m_pScreen2)
        {
            CHK_HR(m_pScreen1->GoToVisualState( L"Minimized", true ), L"GoToState", m_Result );
            CHK_HR(m_pScreen2->GoToVisualState( L"Normal", true ), L"GoToState", m_Result );
        }
        else if (m_pPage1 && m_pPage2)
        {
            CHK_HR(m_pPage1->GoToVisualState( L"Minimized", true ), L"GoToState", m_Result );
            CHK_HR(m_pPage2->GoToVisualState( L"Normal", true ), L"GoToState", m_Result );
        }
        else
        {
            g_pKato->Log(LOG_COMMENT, L"FAIL: Custom User Control(s) NULL");
            goto Exit;
        }
    }
    else
    {
        m_bStateNormal = true;

        if (m_pScreen1 && m_pScreen2)
        {
            CHK_HR(m_pScreen1->GoToVisualState( L"Normal", true ), L"GoToState", m_Result );
            CHK_HR(m_pScreen2->GoToVisualState( L"Minimized", true ), L"GoToState", m_Result );
        }
        else if (m_pPage1 && m_pPage2)
        {
            CHK_HR(m_pPage1->GoToVisualState( L"Normal", true ), L"GoToState", m_Result );
            CHK_HR(m_pPage2->GoToVisualState( L"Minimized", true ), L"GoToState", m_Result );
        }
        else
        {
            g_pKato->Log(LOG_COMMENT, L"FAIL: Custom User Control(s) NULL");
            goto Exit;
        }
    }

    hr = S_OK;

Exit:

    if (FAILED(hr))
    {
        m_Result = TPR_FAIL;

        if (KillTimer(g_NativeHWND, ST_TIMER_ID))
        {
            g_pKato->Log(LOG_COMMENT, TEXT("gTimer successfully destroyed."));
        }

        g_pActiveHost->EndDialog(NULL);
    }

    return hr;
}
//////////////////////////////////////////////////////////////////////////
// Screen1 Custom User Control Implementation
//////////////////////////////////////////////////////////////////////////
HRESULT Screen1::GetXamlSource(XRXamlSource* pXamlSource)
{
    if (!pXamlSource)
    {
        g_pKato->Log(LOG_COMMENT, L"Screen1::GetXamlSource pXamlSource is NULL.");
        return E_FAIL;
    }

    if (g_bUseBaml)
    {
        pXamlSource->SetResource(g_hInstance, ID_XAML_ZOOM_SCREEN_1_BAML);
    }
    else
    {
        pXamlSource->SetResource(g_hInstance, ID_XAML_ZOOM_SCREEN_1);
    }


    return S_OK;
}

HRESULT Screen1::Register()
{
    int Result = TPR_FAIL;
    HRESULT hr = XRCustomUserControlImpl::Register(__uuidof(Screen1), L"Screen1",L"clr-namespace:ScreenTransition");
    CHK_HR(hr, L"Register Screen1 failed.", Result);

    hr = S_OK;
Exit:
    return hr;
}

//////////////////////////////////////////////////////////////////////////
// Screen2 Custom User Control Implementation
//////////////////////////////////////////////////////////////////////////
HRESULT Screen2::GetXamlSource(XRXamlSource* pXamlSource)
{
    if (!pXamlSource)
    {
        g_pKato->Log(LOG_COMMENT, L"IScreen2::GetXamlSource pXamlSource is NULL.");
        return E_FAIL;
    }

    if (g_bUseBaml)
    {
        pXamlSource->SetResource(g_hInstance, ID_XAML_ZOOM_SCREEN_2_BAML);
    }
    else
    {
        pXamlSource->SetResource(g_hInstance, ID_XAML_ZOOM_SCREEN_2);
    }

    return S_OK;
}

HRESULT Screen2::Register()
{
    int Result = TPR_FAIL;
    HRESULT hr = XRCustomUserControlImpl::Register(__uuidof(Screen2), L"Screen2",L"clr-namespace:ScreenTransition");
    CHK_HR(hr, L"Register Screen2 failed.", Result);

    hr = S_OK;
Exit:
    return hr;
}

//////////////////////////////////////////////////////////////////////////
// Page1 Custom User Control Implementation
//////////////////////////////////////////////////////////////////////////
HRESULT Page1::GetXamlSource(XRXamlSource* pXamlSource)
{
    if (!pXamlSource)
    {
        g_pKato->Log(LOG_COMMENT, L"Page1::GetXamlSource pXamlSource is NULL.");
        return E_FAIL;
    }

    if (g_bUseBaml)
    {
        pXamlSource->SetResource(g_hInstance, ID_XAML_ZOOM_PAGE_1_BAML);
    }
    else
    {
        pXamlSource->SetResource(g_hInstance, ID_XAML_ZOOM_PAGE_1);
    }

    return S_OK;
}

HRESULT Page1::Register()
{
    int Result = TPR_FAIL;
    HRESULT hr = XRCustomUserControlImpl::Register(__uuidof(Page1), L"Page1",L"clr-namespace:ScreenTransition");
    CHK_HR(hr, L"Register Page1 failed.", Result);

    hr = S_OK;
Exit:
    return hr;
}

//////////////////////////////////////////////////////////////////////////
// Page2 Custom User Control Implementation
//////////////////////////////////////////////////////////////////////////
HRESULT Page2::GetXamlSource(XRXamlSource* pXamlSource)
{
    if (!pXamlSource)
    {
        g_pKato->Log(LOG_COMMENT, L"IPage2::GetXamlSource pXamlSource is NULL.");
        return E_FAIL;
    }

    if (g_bUseBaml)
    {
        pXamlSource->SetResource(g_hInstance, ID_XAML_ZOOM_PAGE_2_BAML);
    }
    else
    {
        pXamlSource->SetResource(g_hInstance, ID_XAML_ZOOM_PAGE_2);
    }

    return S_OK;
}

HRESULT Page2::Register()
{
    int Result = TPR_FAIL;
    HRESULT hr = XRCustomUserControlImpl::Register(__uuidof(Page2), L"Page2",L"clr-namespace:ScreenTransition");
    CHK_HR(hr, L"Register Page2 failed.", Result);

    hr = S_OK;
Exit:
    return hr;
}

//////////////////////////////////////////////////////////////////////////
// End the current dialog and kill the timer when the time is up
//////////////////////////////////////////////////////////////////////////
VOID CALLBACK TimerProc_EndDialog(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    if (KillTimer(hwnd, ST_TIMER_ID))
    {
        g_pKato->Log(LOG_COMMENT, TEXT("gTimer successfully destroyed."));
    }

    g_pActiveHost->EndDialog(NULL);
}


//////////////////////////////////////////////////////////////////////////
// Set the ExitThread flag and kill the timer when the time is up
//////////////////////////////////////////////////////////////////////////
VOID CALLBACK TimerProc_EndThread(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    EnterCriticalSection(&g_cs); // the g_bExitThread is not thread-safe, need to take precaution
    g_bExitThread = true;
    LeaveCriticalSection(&g_cs);

    if (KillTimer(hwnd, ST_TIMER_ID))
    {
        g_pKato->Log(LOG_COMMENT, TEXT("gTimer successfully destroyed."));
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// While the window is open and running, loads, adds and removes screens in the current visual tree
///////////////////////////////////////////////////////////////////////////////////////////////////
DWORD WINAPI LoadControlsThreadProc (LPVOID lpParameter)
{
    HRESULT hr          = E_FAIL;
    HWND NativeHWND     = NULL;
    int* TestID         = (int*)lpParameter;
    int InsertionIndex  = 0;
    XRThickness Margin  = {0, 0, 0, 0};
    XRPtr<Screen1> pScreen1   = NULL;
    XRPtr<Screen2> pScreen2   = NULL;
    XRPtr<Page1> pPage1       = NULL;
    XRPtr<Page2> pPage2       = NULL;
    IXRGridPtr pGrid;
    IXRUIElementCollectionPtr pCollection;
    DWORD StartingTime = GetTickCount();

    g_pActiveHost->GetContainerHWND(&NativeHWND);
    g_bExitThread = false;

    //Get the visual tree
    CHK_HR(g_pRootElement->FindName(L"RootGrid", &pGrid), L"FindName(RootGrid)", g_ThreadResult);
    CHK_HR(pGrid->GetChildren(&pCollection), L"GetChildren", g_ThreadResult);

    if( g_bRunInMultiThreadMode )
    {
        if (SetTimer(NativeHWND, ST_TIMER_ID, g_OptionSeconds*1000, TimerProc_EndThread) == 0)
        {
            g_pKato->Log(LOG_COMMENT, L"SetTimer Failed");
            goto Exit;
        }
    }

    while (true)
    {
        //load the first page
        switch (*TestID)
        {
        case ID_TC_WIZ_ON_DEMAND:
            CHK_HR(g_pTestApplication->CreateObject(__uuidof(Page1), &pPage1), L"CreateObject", g_ThreadResult);
            pPage1->SetMargin(&Margin);

            if (pPage2)
            {
                CHK_HR(pCollection->Remove(pPage2), L"Remove", g_ThreadResult);
            }

            CHK_HR(pCollection->Add(pPage1, &InsertionIndex), L"Add", g_ThreadResult);
            break;
        case ID_TC_CMPLX_ON_DEMAND:
            CHK_HR(g_pTestApplication->CreateObject(__uuidof(Screen1), &pScreen1), L"CreateObject", g_ThreadResult);
            pScreen1->SetMargin(&Margin);

            if (pScreen2)
            {
                CHK_HR(pCollection->Remove(pScreen2), L"Remove", g_ThreadResult);
            }

            CHK_HR(pCollection->Add(pScreen1, &InsertionIndex), L"Add", g_ThreadResult);
            break;
        }

        UpdateWindow(NativeHWND);
        Sleep(100);

        //load the second page
        switch (*TestID)
        {
        case ID_TC_WIZ_ON_DEMAND:
            CHK_HR(g_pTestApplication->CreateObject(__uuidof(Page2), &pPage2), L"CreateObject", g_ThreadResult);
            pPage2->SetMargin(&Margin);

            if (pPage1)
            {
                CHK_HR(pCollection->Remove(pPage1), L"Remove", g_ThreadResult);
            }

            CHK_HR(pCollection->Add(pPage2, &InsertionIndex), L"Add", g_ThreadResult);
            break;
        case ID_TC_CMPLX_ON_DEMAND:
            CHK_HR(g_pTestApplication->CreateObject(__uuidof(Screen2), &pScreen2), L"CreateObject", g_ThreadResult);
            pScreen2->SetMargin(&Margin);

            if (pScreen1)
            {
                CHK_HR(pCollection->Remove(pScreen1), L"Remove", g_ThreadResult);
            }

            CHK_HR(pCollection->Add(pScreen2, &InsertionIndex), L"Add", g_ThreadResult);
            break;
        }

        UpdateWindow(NativeHWND);
        Sleep(100);

        if( g_bRunInMultiThreadMode)
        {
            EnterCriticalSection(&g_cs);
            int temp = g_bExitThread;
            LeaveCriticalSection(&g_cs);

            if (temp)
            {
                g_pKato->Log(LOG_DETAIL, L"bExitThread = true");
                break;
            }
        }
        else
        {
            if (GetTickCount() - StartingTime > (DWORD)g_OptionSeconds*1000)
            {
                g_pKato->Log(LOG_DETAIL, L"bExitThread = true");
                break;
            }
        }
    }

    g_ThreadResult = TPR_PASS;

Exit:

    g_pKato->Log(LOG_DETAIL, L"Test run terminating");
    g_pActiveHost->EndDialog(NULL);

    return 0;
}

//////////////////////////////////////////////////////////////////////////
// Run Screen Transition test with a single story board
//////////////////////////////////////////////////////////////////////////
TESTPROCAPI StoryboardScreenTransitionPerfTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    TUX_EXECUTE;

    int Result                          = TPR_FAIL;
    HRESULT hr                          = E_FAIL;;
    LPCTSTR lpTestDescription           = lpFTE->lpDescription;
    WCHAR szResultXMLFileName[MAX_PATH] = {0};
    WCHAR szNamespace[MAX_PATH]         = {0};
    WCHAR szName[MAX_PATH]              = {0};
    int sourceID                          = 0;
    bool bSessionOpened                 = false;
    XRXamlSource XamlSource;
    ResourceHandler resource;
    IXRStoryboardPtr pStoryboard;

    g_pKato->Log(LOG_COMMENT, TEXT("Start StoryboardScreenTransitionPerfTest tests"));

    //Init the globals for this test pass
    g_dwImageLoadRet = TPR_PASS;

    //Set up and start the CePerf session
    StringCchCopy( szName, MAX_PATH, lpTestDescription );
    if( g_bUseBaml )
    {
        StringCchCat( szName, MAX_PATH, L" (baml)" );
    }
    StringCchPrintfW( szNamespace, MAX_PATH, XR_TESTNAME_FORMAT, szName );
    //CTK requires us not to use /release in the path
    StringCchCopy( szResultXMLFileName, MAX_PATH, g_bRunningUnderCTK?L"\\" L"XRScreenTransitionPerfResults.xml":RELEASE_DIR L"XRScreenTransitionPerfResults.xml" );
    g_pKato->Log(LOG_COMMENT, L"Using output file: %s", szResultXMLFileName );

    GUID scenarioGUID = GetGUID(szNamespace);
    CHK_HR(PerfScenarioOpenSession(XRPERF_API_PATH, true),L"PerfScenarioOpenSession for " XRPERF_API_PATH, Result);
    bSessionOpened = true;

    //load the xaml
    if (g_bUseBaml)
    {
        CHK_HR(Getint(L"baml", &sourceID), L"Getint baml", Result); // retrieve value from xml file
    }
    else
    {
        CHK_HR(Getint(L"xamlID", &sourceID), L"Getint xamlID", Result); // retrieve value from xml file
    }
    ZeroMemory(&XamlSource, sizeof(XamlSource));
    XamlSource.SetResource(g_hInstance, sourceID);

    CHK_HR(g_pTestApplication->RegisterResourceManager(&resource), L"RegisterResourceManager", Result);
    CHK_HR(g_pTestApplication->CreateHostFromXaml(&XamlSource, NULL, &g_pActiveHost), L"CreateHostFromXaml", Result);

    //check for missing images:
    if (TPR_PASS != g_dwImageLoadRet)
    {
            g_pKato->Log(LOG_COMMENT, L"Failed to load image in xaml");
            Result = g_dwImageLoadRet;
            goto Exit;
    }

    //size up the xaml to fit desktop
    CHK_HR(g_pActiveHost->GetRootElement(&g_pRootElement), L"GetRootElement", Result);
    CHK_DO(g_pRootElement, L"IXRFrameworkElement", Result);
    CHK_HR(g_pRootElement->SetWidth((float)g_ScrnWidth), L"SetWidth", Result)
    CHK_HR(g_pRootElement->SetHeight((float)g_ScrnHeight), L"SetHeight", Result)

    g_pActiveHost->GetContainerHWND(&g_NativeHWND);
    CHK_BOOL(SetWindowPos(g_NativeHWND, NULL, 0, 0, g_ScrnWidth, g_ScrnHeight, SWP_SHOWWINDOW) != 0, L"SetWindowPos", Result);
    g_pActiveHost->ShowWindow();
    UpdateWindow(g_NativeHWND);

    //start storyboard
    CHK_HR(g_pRootElement->FindName(L"Init", &pStoryboard), L"FindName", Result);
    CHK_DO(pStoryboard, L"IXRStoryboard", Result);
    CHK_HR(pStoryboard->Begin(), L"Storyboard->Begin", Result);

    //start the dialog with a timer
    if (SetTimer(g_NativeHWND, ST_TIMER_ID, g_OptionSeconds*1000, TimerProc_EndDialog) == 0)
    {
        g_pKato->Log(LOG_COMMENT, L"SetTimer Failed");
        goto Exit;
    }

    g_pActiveHost->StartDialog(0);

    // Flush CePerf results
    CHK_HR(PerfScenarioFlushMetrics(FALSE,                  //don't close all sessions
                                    &scenarioGUID,          // Scenario Unique GUID
                                    szNamespace,            // Scenario Namespace
                                    szName,      // Scenario Name
                                    szResultXMLFileName,    // Result XML (if first char isn't '\', will prepend VirtualRelease
                                    NULL,NULL),L"PerfScenarioFlushMetrics", Result);

    Result = TPR_PASS;

Exit:
    if (bSessionOpened)
    {
        //Close the perf session
        if (FAILED(PerfScenarioCloseSession(XRPERF_API_PATH)))
        {
            g_pKato->Log(LOG_COMMENT, L"PerfScenarioCloseSession for %s failed with HRESULT: 0x%x", XRPERF_API_PATH, hr);
        }
    }

    g_pTestApplication->RegisterResourceManager(NULL);
    SAFE_RELEASE(g_pActiveHost);
    SAFE_RELEASE(g_pRootElement);
    g_NativeHWND = NULL;
    return Result;
}

//////////////////////////////////////////////////////////////////////////
// Run Screen Transition test that involves changing screen states
//////////////////////////////////////////////////////////////////////////
TESTPROCAPI StatesScreenTransitionPerfTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    TUX_EXECUTE;

    int Result                              = TPR_FAIL;
    HRESULT hr                              = E_FAIL;;
    LPCTSTR lpTestDescription               = lpFTE->lpDescription;
    int TestID                              = lpFTE->dwUniqueID;
    HWND NativeHWND                         = NULL;
    WCHAR szResultXMLFileName[MAX_PATH]     = {0};
    WCHAR szNamespace[MAX_PATH]             = {0};
    WCHAR szName[MAX_PATH]                  = {0};
    int sourceID                            = 0;
    bool bSessionOpened                     = false;
    int CollectionCount                     = 0;
    ScreenTransitionEventHandler* pHandler  = NULL;
    IXRDelegate<XREventArgs> *pDelegate     = NULL;
    IXRDelegate<XRValueChangedEventArgs<IXRVisualState*>>* pDelegateChanged = NULL;
    XRPtr<Screen1> pScreen1;
    XRPtr<Screen2> pScreen2;
    XRPtr<Page1> pPage1;
    XRPtr<Page2> pPage2;
    XRWindowCreateParams WindowParameters;
    XRXamlSource XamlSource;
    ResourceHandler resource;
    IXRVisualStateGroupCollectionPtr pGroups;
    IXRVisualStateGroupPtr pVisualGroup;
    IXRGridPtr pGrid;
    IXRFrameworkElementPtr pRoot;

    g_pKato->Log(LOG_COMMENT, TEXT("Start StatesScreenTransitionPerfTest tests"));

    //Init the globals for this test pass
    g_dwImageLoadRet = TPR_PASS;

    //Set up and start the CePerf session
    StringCchCopy( szName, MAX_PATH, lpTestDescription );
    if( g_bUseBaml )
    {
        StringCchCat( szName, MAX_PATH, L" (baml)" );
    }
    StringCchPrintfW( szNamespace, MAX_PATH, XR_TESTNAME_FORMAT, szName );
    //CTK requires us not to use /release in the path
    StringCchCopy( szResultXMLFileName, MAX_PATH, g_bRunningUnderCTK?L"\\" L"XRScreenTransitionPerfResults.xml":RELEASE_DIR L"XRScreenTransitionPerfResults.xml" );
    g_pKato->Log(LOG_COMMENT, L"Using output file: %s", szResultXMLFileName );

    GUID scenarioGUID = GetGUID(szNamespace);
    CHK_HR(PerfScenarioOpenSession(XRPERF_API_PATH, true),L"PerfScenarioOpenSession for " XRPERF_API_PATH, Result);
    bSessionOpened = true;

    //load the xaml
    if (g_bUseBaml)
    {
        CHK_HR(Getint(L"baml", &sourceID), L"Getint baml", Result); // retrieve value from xml file
    }
    else
    {
        CHK_HR(Getint(L"xamlID", &sourceID), L"Getint xamlID", Result); // retrieve value from xml file
    }
    ZeroMemory(&XamlSource, sizeof(XamlSource));
    XamlSource.SetResource(g_hInstance, sourceID);

    //register custom user controls:
    switch (TestID)
    {
    case ID_TC_ZOOM_WIZ_IN_TREE:
        CHK_HR(Page1::Register(),L"Page1::Register",Result); // register Screen1
        CHK_HR(Page2::Register(),L"Page1::Register",Result); // register Screen2
        break;
    case ID_TC_ZOOM_CMPLX_IN_TREE:
        CHK_HR(Screen1::Register(),L"Screen1::Register",Result); // register Screen1
        CHK_HR(Screen2::Register(),L"Screen2::Register",Result); // register Screen2
        break;
    default:
        g_pKato->Log(LOG_COMMENT, L"Unsupported scenario");
        goto Exit;
    }

    CHK_HR(g_pTestApplication->RegisterResourceManager(&resource), L"RegisterResourceManager", Result);

    ZeroMemory(&WindowParameters, sizeof(WindowParameters));
    WindowParameters.Style       = WS_POPUP;
    WindowParameters.pTitle      = lpTestDescription;
    WindowParameters.Left        = 0;
    WindowParameters.Top         = 0;
    WindowParameters.Width      = g_ScrnWidth;
    WindowParameters.Height     = g_ScrnHeight;

    CHK_HR(g_pTestApplication->CreateHostFromXaml(&XamlSource, &WindowParameters, &g_pActiveHost), L"CreateHostFromXaml", Result);

    //check for missing images:
    if (TPR_PASS != g_dwImageLoadRet)
    {
            g_pKato->Log(LOG_COMMENT, L"Failed to load image in xaml");
            Result = g_dwImageLoadRet;
            goto Exit;
    }

    //size up the xaml to fit desktop
    CHK_HR(g_pActiveHost->GetRootElement(&g_pRootElement), L"GetRootElement", Result);
    CHK_DO(g_pRootElement, L"IXRFrameworkElement", Result);
    CHK_HR(g_pRootElement->SetWidth((float)g_ScrnWidth), L"SetWidth", Result)
    CHK_HR(g_pRootElement->SetHeight((float)g_ScrnHeight), L"SetHeight", Result)

    g_pActiveHost->GetContainerHWND(&NativeHWND);
    g_pActiveHost->ShowWindow();
    UpdateWindow(NativeHWND);

    switch (TestID)
    {
    case ID_TC_ZOOM_WIZ_IN_TREE:
        CHK_HR(g_pRootElement->FindName(L"ZoomPage1",(Page1**)&pPage1),L"FindName(Page1)",Result);
        CHK_DO(pPage1, L"Page1", Result);
        pRoot = pPage1;
        CHK_HR(g_pRootElement->FindName(L"ZoomPage1",(Page1**)&pPage2),L"FindName(Page2)",Result);
        CHK_DO(pPage2, L"Page2", Result);
        break;
    case ID_TC_ZOOM_CMPLX_IN_TREE:
        CHK_HR(g_pRootElement->FindName(L"ZoomScreen1",(Screen1**)&pScreen1),L"FindName(Screen1)",Result);
        CHK_DO(pScreen1, L"Screen1", Result);
        pRoot = pScreen1;
        CHK_HR(g_pRootElement->FindName(L"ZoomScreen2",(Screen2**)&pScreen2),L"FindName(Screen2)",Result);
        CHK_DO(pScreen2, L"Screen2", Result);
        break;
    default:
        g_pKato->Log(LOG_COMMENT, L"Unsupported scenario");
        goto Exit;
    }

    //Get VisualStateGroup - there should be only one
    CHK_DO(pRoot, L"IXRFrameworkElement", Result);
    CHK_HR(pRoot->FindName(L"LayoutRoot", &pGrid), L"FindName(LayoutRoot)", Result);
    CHK_DO(pGrid, L"IXRGrid", Result);

    CHK_HR(pGrid->GetAttachedProperty(L"VisualStateManager.VisualStateGroups", NULL,(IXRDependencyObject**)&pGroups), L"GetAttachedProperty", Result);
    CHK_DO(pGroups, L"IXRVisualStateGroupCollection", Result);
    CHK_HR(pGroups->GetCount(&CollectionCount), L"GetCount", Result);
    CHK_VAL_int(CollectionCount, 1, Result);

    CHK_HR(pGroups->GetItem(0, &pVisualGroup), L"GetItem", Result);
    CHK_DO(pVisualGroup, L"IXRVisualStateGroup", Result);

    pHandler = new ScreenTransitionEventHandler(false, pPage1, pPage2, pScreen1, pScreen2);
    CHK_PTR(pHandler, L"ScreenTransitionEventHandler", Result);
    CHK_HR(CreateDelegate(pHandler, &ScreenTransitionEventHandler::OnEventVisualStateChanged, &pDelegateChanged), L"CreateDelegate", Result);

    //Add StateChanged event handler
    CHK_HR(pVisualGroup->AddVisualStateChangedEventHandler(pDelegateChanged), L"AddVisualStateChangedEventHandler", Result);

    if (SetTimer(NativeHWND, 100, g_OptionSeconds*1000, TimerProc_EndDialog) == 0)
    {
        g_pKato->Log(LOG_COMMENT, L"SetTimer Failed");
        goto Exit;
    }

    //Kick-start the states transitions, leaving everything to the event handler
    switch (TestID)
    {
    case ID_TC_ZOOM_WIZ_IN_TREE: 
        CHK_HR(pPage1->GoToVisualState( L"Minimized", true ), L"GoToState", Result );
        CHK_HR(pPage2->GoToVisualState( L"Normal", true ), L"GoToState", Result );
        g_pKato->Log(LOG_COMMENT, L"Starting States Transitions");
        break;
    case ID_TC_ZOOM_CMPLX_IN_TREE:
        CHK_HR(pScreen1->GoToVisualState( L"Minimized", true ), L"GoToState", Result );
        CHK_HR(pScreen2->GoToVisualState( L"Normal", true ), L"GoToState", Result );
        g_pKato->Log(LOG_COMMENT, L"Starting States Transitions");
        break;
    default:
        g_pKato->Log(LOG_COMMENT, L"Unsupported scenario");
        goto Exit;
    }

    g_pActiveHost->StartDialog(0);

    Result = pHandler->m_Result;

    if (TPR_PASS == Result)
    {
        // Flush CePerf results
        CHK_HR(PerfScenarioFlushMetrics(FALSE,                  //don't close all sessions
                                        &scenarioGUID,          // Scenario Unique GUID
                                        szNamespace,            // Scenario Namespace
                                        szName,      // Scenario Name
                                        szResultXMLFileName,    // Result XML (if first char isn't '\', will prepend VirtualRelease
                                        NULL,NULL),L"PerfScenarioFlushMetrics", Result);
    }


Exit:
    if (bSessionOpened)
    {
        //Close the perf session
        if (FAILED(PerfScenarioCloseSession(XRPERF_API_PATH)))
        {
            g_pKato->Log(LOG_COMMENT, L"PerfScenarioCloseSession for %s failed with HRESULT: 0x%x", XRPERF_API_PATH, hr);
        }
    }

    if (pVisualGroup && pDelegateChanged)
    {
        pVisualGroup->RemoveVisualStateChangedEventHandler(pDelegateChanged);
    }

    SAFE_RELEASE(pDelegateChanged);
    CLEAN_PTR(pHandler);

    g_pTestApplication->RegisterResourceManager(NULL);
    SAFE_RELEASE(g_pActiveHost);
    SAFE_RELEASE(g_pRootElement);
    return Result;
}

/////////////////////////////////////////////////////////////////////////////////
// Run Screen Transition test that adds and removes visual components dynamically
/////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI OnDemandScreenTransitionPerfTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    TUX_EXECUTE;

    int Result                          = TPR_FAIL;
    HRESULT hr                          = E_FAIL;;
    LPCTSTR lpTestDescription           = lpFTE->lpDescription;
    int TestID                          = lpFTE->dwUniqueID;
    HANDLE hThread                      = NULL;
    HWND NativeHWND                     = NULL;
    WCHAR szResultXMLFileName[MAX_PATH] = {0};
    WCHAR szNamespace[MAX_PATH]         = {0};
    WCHAR szName[MAX_PATH]              = {0};
    int sourceID                        = 0;
    bool bSessionOpened                 = false;
    XRWindowCreateParams WindowParameters;
    XRXamlSource XamlSource;
    ResourceHandler resource;

    g_pKato->Log(LOG_COMMENT, TEXT("Start OnDemandScreenTransitionPerfTest tests"));

    //Init the globals for this test pass
    g_dwImageLoadRet = TPR_PASS;

    //Set up and start the CePerf session
    StringCchCopy( szName, MAX_PATH, lpTestDescription );
    if( g_bUseBaml )
    {
        StringCchCat( szName, MAX_PATH, L" (baml)" );
    }
    StringCchPrintfW( szNamespace, MAX_PATH, XR_TESTNAME_FORMAT, szName );
    //CTK requires us not to use /release in the path
    StringCchCopy( szResultXMLFileName, MAX_PATH, g_bRunningUnderCTK?L"\\" L"XRScreenTransitionPerfResults.xml":RELEASE_DIR L"XRScreenTransitionPerfResults.xml" );
    g_pKato->Log(LOG_COMMENT, L"Using output file: %s", szResultXMLFileName );

    GUID scenarioGUID = GetGUID(szNamespace);
    CHK_HR(PerfScenarioOpenSession(XRPERF_API_PATH, true),L"PerfScenarioOpenSession for " XRPERF_API_PATH, Result);
    bSessionOpened = true;

    //load the xamls 
    if (g_bUseBaml)
    {
        CHK_HR(Getint(L"baml", &sourceID), L"Getint baml", Result); 
    }
    else
    {
        CHK_HR(Getint(L"xamlID", &sourceID), L"Getint xamlID", Result); 
    }

    CHK_HR(g_pTestApplication->RegisterResourceManager(&resource), L"RegisterResourceManager", Result);

    //register custom user controls:
    switch (TestID)
    {
    case ID_TC_WIZ_ON_DEMAND:
        CHK_HR(Page1::Register(),L"Page1::Register",Result); // register Screen1
        CHK_HR(Page2::Register(),L"Page1::Register",Result); // register Screen2
        break;
    case ID_TC_CMPLX_ON_DEMAND:
        CHK_HR(Screen1::Register(),L"Screen1::Register",Result); // register Screen1
        CHK_HR(Screen2::Register(),L"Screen2::Register",Result); // register Screen2
        break;
    default:
        g_pKato->Log(LOG_COMMENT, L"Unsupported scenario");
        goto Exit;
    }

    //load the main xaml
    ZeroMemory(&XamlSource, sizeof(XamlSource));
    XamlSource.SetResource(g_hInstance, sourceID);

    ZeroMemory(&WindowParameters, sizeof(WindowParameters));
    WindowParameters.Style  = WS_POPUP;
    WindowParameters.pTitle = lpTestDescription;
    WindowParameters.Left   = 0;
    WindowParameters.Top    = 0;
    WindowParameters.Width  = g_ScrnWidth;
    WindowParameters.Height = g_ScrnHeight;
    WindowParameters.AllowsMultipleThreadAccess   = true;

    CHK_HR(g_pTestApplication->CreateHostFromXaml(&XamlSource, &WindowParameters, &g_pActiveHost), L"CreateHostFromXaml", Result);

    //check for missing images:
    if (TPR_PASS != g_dwImageLoadRet)
    {
            g_pKato->Log(LOG_COMMENT, L"Failed to load image in xaml");
            Result = g_dwImageLoadRet;
            goto Exit;
    }

    //size up the xaml to fit desktop
    CHK_HR(g_pActiveHost->GetRootElement(&g_pRootElement), L"GetRootElement", Result);
    CHK_DO(g_pRootElement, L"IXRFrameworkElement", Result);
    CHK_HR(g_pRootElement->SetWidth((float)g_ScrnWidth), L"SetWidth", Result)
    CHK_HR(g_pRootElement->SetHeight((float)g_ScrnHeight), L"SetHeight", Result)

    g_pActiveHost->GetContainerHWND(&NativeHWND);
    g_pActiveHost->ShowWindow();

    if( g_bRunInMultiThreadMode )
    {
        InitializeCriticalSection(&g_cs);
        hThread = CreateThread(NULL, 0, LoadControlsThreadProc, &TestID, 0, 0); 
        if (!hThread)
        {
            // Error starting thread.
            g_pKato->Log(LOG_COMMENT, L"Error starting monitoring thread");
            FAIL(Result);
        }

        g_pActiveHost->StartDialog(0);
    }
    else
    {
        //Call the test proc directly from this thread instead of creating another one
        LoadControlsThreadProc( &TestID );
    }

    Result = g_ThreadResult;

    if (TPR_PASS == Result)
    {
        // Flush CePerf results
        CHK_HR(PerfScenarioFlushMetrics(FALSE,                  //don't close all sessions
                                        &scenarioGUID,          // Scenario Unique GUID
                                        szNamespace,            // Scenario Namespace
                                        szName,      // Scenario Name
                                        szResultXMLFileName,    // Result XML (if first char isn't '\', will prepend VirtualRelease
                                        NULL,NULL),L"PerfScenarioFlushMetrics", Result);
    }

Exit:
    if (bSessionOpened)
    {
        //Close the perf session
        if (FAILED(PerfScenarioCloseSession(XRPERF_API_PATH)))
        {
            g_pKato->Log(LOG_COMMENT, L"PerfScenarioCloseSession for %s failed with HRESULT: 0x%x", XRPERF_API_PATH, hr);
        }
    }

    if (hThread)
    {
        CloseHandle(hThread);
        hThread = NULL;
    }

    g_pTestApplication->RegisterResourceManager(NULL);
    SAFE_RELEASE(g_pActiveHost);
    SAFE_RELEASE(g_pRootElement);

    return Result;
}