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
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


Module Name:

    ImageManipulation.cpp

Abstract:


Environment:

    Win32 CE 


-------------------------------------------------------------------*/
#include <windows.h>
#include <string.h>
#include <PerfScenario.h>
#include <GSEHelper2.h>
#include <GestureWnd2.h>
#include "globals.h"
#include "ImageManipulation.h"
#include "TuxProc.h"
#include "ResManager.h"
#include <XRDelegate.h>
#include <GetGuidUtil.h>

IM_DIRECTIONS g_Direction = IM_DOWN;

//////////////////////////////////////////////////////////////////////////
// ImageUC Custom User Control Implementation
//////////////////////////////////////////////////////////////////////////
HRESULT ImageUC::GetXamlSource(XRXamlSource* pXamlSource)
{
    if (!pXamlSource)
    {
        g_pKato->Log(LOG_COMMENT, L"ImageUC::GetXamlSource pXamlSource is NULL.");
        return E_FAIL;
    }

    if (g_bIsGraph)
    {
        if (g_bUseBaml)
        {
            pXamlSource->SetResource(g_ResourceModule, IDX_PINCH_STRETCH_UC_GRAPH_BAML);
        }
        else
        {
            pXamlSource->SetResource(g_ResourceModule, IDX_PINCH_STRETCH_UC_GRAPH);
        }
    }
    else
    {
        if (g_bUseBaml)
        {
            pXamlSource->SetResource(g_ResourceModule, IDX_PINCH_STRETCH_UC_PHOTO_BAML);
        }
        else
        {
            pXamlSource->SetResource(g_ResourceModule, IDX_PINCH_STRETCH_UC_PHOTO);
        }
    }

    return S_OK;
}

HRESULT ImageUC::Register()
{
    int Result = TPR_FAIL;
    HRESULT hr = XRCustomUserControlImpl::Register(__uuidof(ImageUC), L"ImageUC",L"clr-namespace:ImageManipulation");
    CHK_HR(hr, L"Register ImageUC failed.", Result);

    hr = S_OK;
Exit:
    return hr;
}

//////////////////////////////////////////////////////////////////////////
// ImageManipulationEventHandler functions
//////////////////////////////////////////////////////////////////////////
HRESULT ImageManipulationEventHandler::OnEventVisualStateChanged(IXRDependencyObject* pSender, XRValueChangedEventArgs<IXRVisualState*>* pArgs)
{
    HRESULT hr = E_FAIL;

    if (!m_bStateNormal)
    {
        m_bStateNormal = true;
        CHK_HR(m_pImageUC->GoToVisualState( L"Normal", true ), L"GoToState", m_Result );
    }
    else if (m_bNextStatePinch)
    {
        m_bNextStatePinch = false;
        m_bStateNormal = false;
        CHK_HR(m_pImageUC->GoToVisualState( L"Pinch", true ), L"GoToState", m_Result );
        ++g_DoneRepeats;
    }
    else
    {
        m_bNextStatePinch = true;
        m_bStateNormal = false;
        CHK_HR(m_pImageUC->GoToVisualState( L"Stretch", true ), L"GoToState", m_Result );
    }

    hr = S_OK;

Exit:

    if (FAILED(hr))
    {
        m_Result = TPR_FAIL;
    }
    else
    {
        m_Result = TPR_PASS;
    }

    if ((g_DoneRepeats >= g_OptionRepeats) || FAILED(hr))
    {
        g_pKato->Log(LOG_COMMENT, L"Test run terminating after %i repetitions", g_DoneRepeats);
        g_pActiveHost->EndDialog(NULL);
    }

    return hr;
}

//////////////////////////////////////////////////////////////////////////
// Function will scroll the image list 50 selections every time it's called
//////////////////////////////////////////////////////////////////////////
VOID CALLBACK TimerProc_Scroll(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
    HRESULT hr = E_FAIL;
    hr = S_OK;
    float fOffset = 0.0;
    float fEndOfScroll = 0.0;

    Sleep(MS_DELAY);

    //decide which direction we're going:
    switch (g_Direction)
    {
    case IM_DOWN:
        CHK_HR(g_pScroller->GetVerticalOffset(&fOffset), L"GetVerticalOffset", g_Result);
        fOffset += MY_OFFSET;
        CHK_HR(g_pScroller->ScrollToVerticalOffset(fOffset), L"ScrollToVerticalOffset", g_Result);

        //did this take us to the end of the scoll area?  Switch direction
        CHK_HR(g_pScroller->GetScrollableHeight(&fEndOfScroll), L"GetScrollableHeight", g_Result);
        if (MY_OFFSET/2 > fEndOfScroll - fOffset)
        {
            g_Direction = IM_UP;
        }
        break;
    case IM_UP:
        CHK_HR(g_pScroller->GetVerticalOffset(&fOffset), L"GetVerticalOffset", g_Result);
        fOffset -= MY_OFFSET;
        CHK_HR(g_pScroller->ScrollToVerticalOffset(fOffset), L"ScrollToVerticalOffset", g_Result);

        //did this take us to the end of the scoll area?  Switch direction
        CHK_HR(g_pScroller->GetScrollableHeight(&fEndOfScroll), L"GetScrollableHeight", g_Result);
        if (fOffset < MY_OFFSET/2)
        {
            g_Direction = IM_DOWN;
            ++g_DoneRepeats;
        }
        break;
    case IM_RIGHT:
        CHK_HR(g_pScroller->GetHorizontalOffset(&fOffset), L"GetHorizontalOffset", g_Result);
        fOffset += MY_OFFSET;
        CHK_HR(g_pScroller->ScrollToHorizontalOffset(fOffset), L"ScrollToHorizontalOffset", g_Result);

        //did this take us to the end of the scoll area?  Switch direction
        CHK_HR(g_pScroller->GetScrollableWidth(&fEndOfScroll), L"GetScrollableWidth", g_Result);
        if (MY_OFFSET/2 > fEndOfScroll - fOffset)
        {
            g_Direction = IM_LEFT;
        }
        break;
    case IM_LEFT:
        CHK_HR(g_pScroller->GetHorizontalOffset(&fOffset), L"GetHorizontalOffset", g_Result);
        fOffset -= MY_OFFSET;
        CHK_HR(g_pScroller->ScrollToHorizontalOffset(fOffset), L"ScrollToHorizontalOffset", g_Result);

        //did this take us to the end of the scoll area?  Switch direction
        CHK_HR(g_pScroller->GetScrollableWidth(&fEndOfScroll), L"GetScrollableWidth", g_Result);
        if (fOffset < MY_OFFSET/2)
        {
            g_Direction = IM_RIGHT;
            ++g_DoneRepeats;
        }
        break;
    }

    g_Result = TPR_PASS;

Exit:

    if (FAILED(hr))
    {
        g_Result = TPR_FAIL;
    }

    if ((g_DoneRepeats >= g_OptionRepeats) || FAILED(hr))
    {
        if (KillTimer(hwnd, idEvent))
        {
            g_pKato->Log(LOG_COMMENT, L"gTimer successfully destroyed.");
        }

        g_pActiveHost->EndDialog(NULL);
    }
}

//////////////////////////////////////////////////////////////////////////
// Runs the tests while the window is open and running, then closes the window
//////////////////////////////////////////////////////////////////////////
DWORD WINAPI RunTestsThreadProc (LPVOID lpParameter)
{
    HRESULT hr                  = E_FAIL;
    int* TestID                 = (int*) lpParameter;
    HWND NativeHWND             = NULL;
    WCHAR szName[MY_MAX_PATH]   = {0};
    float fWidth                = 0.0;
    float fHeight               = 0.0;
    GSEHelper* pGSE             = new GSEHelper();
    GestureDataPoint gdp[5];
    XRPoint PtTarget;
    IXRDependencyObjectPtr pDO;
    IXRFrameworkElementPtr pFrameworkElement;

    g_pActiveHost->GetContainerHWND( &NativeHWND );
    CHK_HR(GetTargetElementName(szName,MY_MAX_PATH),L"GetTargetElementName",g_Result);
    CHK_HR(GetTargetElement(g_pActiveHost,szName,&pDO), L"GetTargetElement", g_Result);
    CHK_DO(pDO, L"IXRDependencyObject", g_Result);
    pFrameworkElement = pDO;
    CHK_DO(pFrameworkElement, L"IXRFrameworkElement", g_Result);

    //Get the coordinates of the target element
    CHK_HR(pFrameworkElement->GetActualWidth(&fWidth), L"GetWidth", g_Result);
    CHK_HR(pFrameworkElement->GetActualHeight(&fHeight), L"GetHeight", g_Result);
    CHK_HR(pFrameworkElement->GetActualX(&PtTarget.x), L"Get Actual X Position", g_Result);
    CHK_HR(pFrameworkElement->GetActualY(&PtTarget.y), L"Get Actual Y Position", g_Result);

    //Get the middle POINT of the application window - for gestures
    gdp[0].point.x = (int)PtTarget.x + (int)fWidth/2;
    gdp[0].point.y = (int)PtTarget.y + (int)fHeight/2;

    for (int i = 0; i < g_OptionRepeats; i++)
    {
        switch (*TestID)
        {
        case TC_GRAPH_PAN:
        case TC_PHOTO_PAN:
            CHK_PTR(pGSE, L"GSEHelper", g_Result);
            //Note:Pan expects at least 2 data point structures per move!
            gdp[0].timeBetweenCurrAndNextPoint = 0;     //use default engine value
            gdp[1].timeBetweenCurrAndNextPoint = 0;
            gdp[1].point.x = gdp[0].point.x;
            gdp[1].point.y = gdp[0].point.y + PAN_SAMPLES;       //Pan 100 samples down
            gdp[2].timeBetweenCurrAndNextPoint = 0;
            gdp[2].point.x = gdp[0].point.x;
            gdp[2].point.y = gdp[0].point.y;            //Pan 100 samples up (back)
            gdp[3].timeBetweenCurrAndNextPoint = 0;
            gdp[3].point.y = gdp[0].point.y;
            gdp[3].point.x = gdp[0].point.x + PAN_SAMPLES;       //Pan 100 samples to the right
            gdp[4].timeBetweenCurrAndNextPoint = 0;
            gdp[4].point.y = gdp[0].point.y;
            gdp[4].point.x = gdp[0].point.x;            //Pan 100 samples to the left (back)

            CHK_HR(pGSE->DoGesture(GESTURETYPE_PAN, 5, gdp), L"DoGesture", g_Result);
            break;
        case TC_GRAPH_FLICK:
        case TC_PHOTO_FLICK:
            CHK_PTR(pGSE, L"GSEHelper", g_Result);
            //Note: Flick expects a single data point structure per move!
            gdp[0].lParam1 = 0;                         //Flick to the right
            gdp[0].lParam2 = MAKELONG(0, 3);
            CHK_HR(pGSE->DoGesture(GESTURETYPE_FLICK, 1, gdp), L"DoGesture", g_Result);
            Sleep(5000);        //FLICK animation does not block

            gdp[0].lParam1 = 90;                         //Flick down
            gdp[0].lParam2 = MAKELONG(0, 3);
            CHK_HR(pGSE->DoGesture(GESTURETYPE_FLICK, 1, gdp), L"DoGesture", g_Result);
            Sleep(5000);        //FLICK animation does not block

            gdp[0].lParam1 = 180;                       //Flick left
            gdp[0].lParam2 = MAKELONG(0, 3);
            CHK_HR(pGSE->DoGesture(GESTURETYPE_FLICK, 1, gdp), L"DoGesture", g_Result);
            Sleep(5000);        //FLICK animation does not block

            gdp[0].lParam1 = 270;                       //Flick up
            gdp[0].lParam2 = MAKELONG(0, 3);
            CHK_HR(pGSE->DoGesture(GESTURETYPE_FLICK, 1, gdp), L"DoGesture", g_Result);
            Sleep(5000);        //FLICK animation does not block
            break;
        default:
            g_pKato->Log(LOG_COMMENT, L"ERROR: Unknown Test Type");
            break;
        }
    }

    g_Result = TPR_PASS;

Exit:
    g_pActiveHost->EndDialog(0);

    CLEAN_PTR( pGSE );

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send Pan and Flick gestures to the Scroll Viewer from a separate thread
////////////////////////////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI ImageManipulationPerfTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
     TUX_EXECUTE;

    int Result                              = TPR_FAIL;
    HRESULT hr                              = E_FAIL;;
    LPCTSTR lpTestDescription               = lpFTE->lpDescription;
    HWND NativeHWND                         = NULL;
    WCHAR szResultXMLFileName[MY_MAX_PATH]  = {0};
    WCHAR szNamespace[MY_MAX_PATH]          = {0};
    WCHAR szResourcePath[MY_MAX_PATH]       = {0};
    WCHAR szName[MY_MAX_PATH]               = {0};
    HANDLE hThread                          = NULL;
    int TestID                              = lpFTE->dwUniqueID;
    int sourceID                            = 0;
    XRWindowCreateParams WindowParameters;
    XRXamlSource XamlSource;
    ResourceHandler resource;
    IXRFrameworkElementPtr pRootElement;

    g_pKato->Log(LOG_COMMENT, L"Start ImageManipulationPerf tests");

    //Set up and start the CePerf session
    StringCchCopy( szName, MY_MAX_PATH, lpTestDescription );
    if( g_bUseBaml )
    {
        StringCchCat( szName, MY_MAX_PATH, L" (baml)" );
    }

    StringCchPrintfW( szNamespace, MY_MAX_PATH, XR_TESTNAME_FORMAT, szName );
    //CTK requires us not to use /release in the path
    StringCchCopy( szResultXMLFileName, MY_MAX_PATH, g_bRunningUnderCTK?L"\\" IM_PERF_LOG_NAME:RELEASE_DIR IM_PERF_LOG_NAME );
    g_pKato->Log(LOG_COMMENT, L"Using output file: %s", szResultXMLFileName );

    GUID scenarioGUID = GetGUID(szNamespace);
    CHK_HR(PerfScenarioOpenSession(XRPERF_API_PATH, true),L"PerfScenarioOpenSession", Result);

    //get test data
    CHK_HR(Getbool(L"IsGraph", &g_bIsGraph), L"Getbool IsGraph", Result); // retrieve value from xml file

    ZeroMemory(&XamlSource, sizeof(XamlSource));

    //load the resource dll
    if (g_bIsGraph)
    {
        if (g_bUseBaml) //Large Graph png baml
        {
            sourceID = IDX_LARGE_GRAPH_BAML;
            StringCchPrintfW( szResourcePath, MY_MAX_PATH, g_bRunningUnderCTK?BAML_RESOURCE_GRAPH_NAME:RELEASE_DIR BAML_RESOURCE_GRAPH_NAME);
        }
        else //Large Graph png xaml
        {
            sourceID = IDX_LARGE_GRAPH;
            StringCchPrintfW( szResourcePath, MY_MAX_PATH, g_bRunningUnderCTK?XAML_RESOURCE_GRAPH_NAME:RELEASE_DIR XAML_RESOURCE_GRAPH_NAME);
        }
    }
    else 
    {
        if (g_bUseBaml) //Large Photo png baml
        {
            sourceID = IDX_LARGE_PHOTO_BAML;
            StringCchPrintfW( szResourcePath, MY_MAX_PATH, g_bRunningUnderCTK?BAML_RESOURCE_PHOTO_NAME:RELEASE_DIR BAML_RESOURCE_PHOTO_NAME);
        }
        else //Large Photo png xaml
        {
            sourceID = IDX_LARGE_PHOTO;
            StringCchPrintfW( szResourcePath, MY_MAX_PATH, g_bRunningUnderCTK?XAML_RESOURCE_PHOTO_NAME:RELEASE_DIR XAML_RESOURCE_PHOTO_NAME);
        }
    }

    g_pKato->Log(LOG_COMMENT, L"Will load resource module: %s", szResourcePath);
    g_ResourceModule = LoadLibrary(szResourcePath);
    CHK_PTR(g_ResourceModule, L"ResourceModule", Result);
    CHK_HR(g_pTestApplication->AddResourceModule(g_ResourceModule), L"AddResourceModue", Result);
    XamlSource.SetResource(g_ResourceModule, sourceID);
    g_pKato->Log(LOG_COMMENT, L"Loaded sourceID: %i", sourceID);

    CHK_HR(g_pTestApplication->RegisterResourceManager(&resource), L"RegisterResourceManager", Result);

    ZeroMemory(&WindowParameters, sizeof(WindowParameters));
    WindowParameters.Style  = WS_POPUP;
    WindowParameters.pTitle = lpTestDescription;
    WindowParameters.Left   = 0;
    WindowParameters.Top    = 0;
    WindowParameters.Width  = g_ScrnWidth;
    WindowParameters.Height = g_ScrnHeight;
    WindowParameters.AllowsMultipleThreadAccess   = true;

    //initialize global flags for this test:
    g_dwImageLoadRet    = TPR_PASS;
    g_Result            = TPR_FAIL;

    CHK_HR(g_pTestApplication->CreateHostFromXaml(&XamlSource, &WindowParameters, &g_pActiveHost), L"CreateHostFromXaml", Result);

    //size up the xaml to fit desktop
    CHK_HR(g_pActiveHost->GetRootElement(&pRootElement), L"GetRootElement", Result);
    CHK_DO(pRootElement, L"IXRFrameworkElement", Result);
    CHK_HR(pRootElement->SetWidth((float)g_ScrnWidth), L"SetWidth", Result)
    CHK_HR(pRootElement->SetHeight((float)g_ScrnHeight), L"SetHeight", Result)

    CHK_HR(g_pActiveHost->GetContainerHWND(&NativeHWND), L"Error - VisualHost::GetContainerHWND failure in ImageManipulationPerfTest", Result);
    CHK_HR(g_pActiveHost->ShowWindow(), L"Error - VisualHost::ShowWindow failure in ImageManipulationPerfTest", Result);
    CHK_BOOL(UpdateWindow(NativeHWND) != FALSE, L"Error - UpdateWindow failure in ImageManipulationPerfTest", Result);

    //check for missing images:
    if (TPR_PASS != g_dwImageLoadRet)
    {
            g_pKato->Log(LOG_COMMENT, L"Failed to load all the images");
            Result = g_dwImageLoadRet;
            goto Exit;
    }

    hThread = CreateThread(NULL, 0, RunTestsThreadProc, &TestID, 0, 0); 
    if (!hThread)
    {
        // Error starting thread.
        g_pKato->Log(LOG_COMMENT, L"Error starting monitoring thread");
        FAIL(Result);
    }

    //pump messages until hThread stops the dialog:
    g_pActiveHost->StartDialog(0);

    Result = g_Result;

    if (Result == TPR_PASS)
    {
        // Flush CePerf results
        CHK_HR(PerfScenarioFlushMetrics(FALSE,                  //don't close all sessions
                                        &scenarioGUID,          // Scenario Unique GUID
                                        szNamespace,            // Scenario Namespace
                                        szName,                 // Scenario Name
                                        szResultXMLFileName,    // Result XML (if first char isn't '\', will prepend VirtualRelease
                                        NULL,NULL),L"PerfScenarioFlushMetrics", Result);
    }

Exit:
    //Close the perf session
    hr = PerfScenarioCloseSession(XRPERF_API_PATH);
    if (FAILED(hr))
    {
        g_pKato->Log(LOG_COMMENT, L"PerfScenarioCloseSession for %s failed with HRESULT: 0x%x",XRPERF_API_PATH, hr);
    }

    if(hThread)
    {
        CloseHandle(hThread);
        hThread = NULL;
    }

    if (g_ResourceModule)
    {
        g_pTestApplication->RemoveResourceModule(g_ResourceModule);
        FreeLibrary(g_ResourceModule);
        g_ResourceModule = NULL;
    }

    g_pTestApplication->RegisterResourceManager(NULL);
    SAFE_RELEASE(g_pActiveHost);

    return Result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Change states from zoomed in to zoomed out with a transform: Normal->Stretch->Normal->Pinch and so on
////////////////////////////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI ZoomImagePerfTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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
    WCHAR szResourcePath[MY_MAX_PATH]       = {0};
    int sourceID                            = 0;
    bool bSessionOpened                     = false;
    ImageManipulationEventHandler* pHandler = NULL;
    int CollectionCount                     = 0;
    IXRDelegate<XRValueChangedEventArgs<IXRVisualState*>>* pDelegateChanged = NULL;
    XRWindowCreateParams WindowParameters;
    XRXamlSource XamlSource;
    ResourceHandler resource;
    IXRVisualStateGroupCollectionPtr pGroups;
    IXRVisualStateGroupPtr pVisualGroup;
    IXRGridPtr pGrid;
    IXRFrameworkElementPtr pRootElement, pRoot;
    XRPtr<ImageUC> pImageUC;

    g_pKato->Log(LOG_COMMENT, L"Start PinchStretchImagePerfTest tests");

    //Set up and start the CePerf session
    StringCchCopy( szName, MAX_PATH, lpTestDescription );
    if( g_bUseBaml )
    {
        StringCchCat( szName, MAX_PATH, L" (baml)" );
    }
    StringCchPrintfW( szNamespace, MAX_PATH, XR_TESTNAME_FORMAT, szName );
    //CTK requires us not to use /release in the path
    StringCchCopy( szResultXMLFileName, MAX_PATH, g_bRunningUnderCTK?L"\\" IM_PERF_LOG_NAME:RELEASE_DIR IM_PERF_LOG_NAME );
    g_pKato->Log(LOG_COMMENT, L"Using output file: %s", szResultXMLFileName );

    GUID scenarioGUID = GetGUID(szNamespace);
    CHK_HR(PerfScenarioOpenSession(XRPERF_API_PATH, true),L"PerfScenarioOpenSession for " XRPERF_API_PATH, Result);
    bSessionOpened = true;

    //get test data
    CHK_HR(Getbool(L"IsGraph", &g_bIsGraph), L"Getbool IsGraph", Result); // retrieve value from xml file

    //load the right resource module:
    if (g_bIsGraph)
    {
        if (g_bUseBaml) //Large Graph png baml
        {
            StringCchPrintfW( szResourcePath, MY_MAX_PATH, g_bRunningUnderCTK?BAML_RESOURCE_GRAPH_NAME:RELEASE_DIR BAML_RESOURCE_GRAPH_NAME);
        }
        else //Large Graph png xaml
        {
            StringCchPrintfW( szResourcePath, MY_MAX_PATH, g_bRunningUnderCTK?XAML_RESOURCE_GRAPH_NAME:RELEASE_DIR XAML_RESOURCE_GRAPH_NAME);
        }
    }
    else 
    {
        if (g_bUseBaml) //Large Photo png baml
        {
            StringCchPrintfW( szResourcePath, MY_MAX_PATH, g_bRunningUnderCTK?BAML_RESOURCE_PHOTO_NAME:RELEASE_DIR BAML_RESOURCE_PHOTO_NAME);
        }
        else //Large Photo png xaml
        {
            StringCchPrintfW( szResourcePath, MY_MAX_PATH, g_bRunningUnderCTK?XAML_RESOURCE_PHOTO_NAME:RELEASE_DIR XAML_RESOURCE_PHOTO_NAME);
        }
    }
    g_pKato->Log(LOG_COMMENT, L"Will load resource module: %s", szResourcePath);
    g_ResourceModule = LoadLibrary(szResourcePath);
    CHK_PTR(g_ResourceModule, L"ResourceModule", Result);
    CHK_HR(g_pTestApplication->AddResourceModule(g_ResourceModule), L"AddResourceModue", Result);

    CHK_HR(g_pTestApplication->RegisterResourceManager(&resource), L"RegisterResourceManager", Result);

    //register custom user control:
    CHK_HR(ImageUC::Register(),L"ImageUC::Register", Result); // register ImageUC

    //load the xaml
    if (g_bUseBaml)
    {
        CHK_HR(Getint(L"baml", &sourceID), L"Getint baml", Result); // retrieve value from xml file
    }
    else
    {
        CHK_HR(Getint(L"xaml", &sourceID), L"Getint xaml", Result); // retrieve value from xml file
    }
    ZeroMemory(&XamlSource, sizeof(XamlSource));
    XamlSource.SetResource(g_ResourceModule, sourceID);
    g_pKato->Log(LOG_COMMENT, L"Loaded sourceID: %i", sourceID);

    ZeroMemory(&WindowParameters, sizeof(WindowParameters));
    WindowParameters.Style       = WS_POPUP;
    WindowParameters.pTitle      = lpTestDescription;
    WindowParameters.Left        = 0;
    WindowParameters.Top         = 0;
    WindowParameters.Width      = g_ScrnWidth;
    WindowParameters.Height     = g_ScrnHeight;

    //initialize global flags for this test pass:
    g_dwImageLoadRet    = TPR_PASS;
    g_DoneRepeats       = 0;

    CHK_HR(g_pTestApplication->CreateHostFromXaml(&XamlSource, &WindowParameters, &g_pActiveHost), L"CreateHostFromXaml", Result);

    //size up the xaml to fit desktop
    CHK_HR(g_pActiveHost->GetRootElement(&pRootElement), L"GetRootElement", Result);
    CHK_DO(pRootElement, L"IXRFrameworkElement", Result);
    CHK_HR(pRootElement->SetWidth((float)g_ScrnWidth), L"SetWidth", Result)
    CHK_HR(pRootElement->SetHeight((float)g_ScrnHeight), L"SetHeight", Result)

    CHK_HR(g_pActiveHost->GetContainerHWND(&NativeHWND), L"Error - VisualHost::GetContainerHWND failure in ZoomImagePerfTest", Result);
    CHK_HR(g_pActiveHost->ShowWindow(), L"Error - VisualHost::ShowWindow failure in ZoomImagePerfTest", Result);
    CHK_BOOL(UpdateWindow(NativeHWND)!=FALSE, L"Error - UpdateWindow failure in ZoomImagePerfTest", Result);
    //check for missing images:
    if (TPR_PASS != g_dwImageLoadRet)
    {
            g_pKato->Log(LOG_COMMENT, L"Failed to load all the images");
            Result = g_dwImageLoadRet;
            goto Exit;
    }

    //Get the UC Object:
    CHK_HR(pRootElement->FindName(L"ImageUC", &pImageUC),L"FindName(ImageUC)",Result);
    CHK_DO(pImageUC, L"pImageUC", Result);
    pRoot = pImageUC;

    //Get VisualStateGroup - there should be only one
    CHK_HR(pRoot->FindName(L"LayoutRoot", &pGrid), L"FindName(LayoutRoot)", Result);
    CHK_DO(pGrid, L"IXRGrid", Result);

    CHK_HR(pGrid->GetAttachedProperty(L"VisualStateManager.VisualStateGroups", NULL,(IXRDependencyObject**)&pGroups), L"GetAttachedProperty", Result);
    CHK_DO(pGroups, L"IXRVisualStateGroupCollection", Result);
    CHK_HR(pGroups->GetCount(&CollectionCount), L"GetCount", Result);
    CHK_VAL_int(CollectionCount, 1, Result);

    CHK_HR(pGroups->GetItem(0, &pVisualGroup), L"GetItem", Result);
    CHK_DO(pVisualGroup, L"IXRVisualStateGroup", Result);

    pHandler = new ImageManipulationEventHandler(false, true, pImageUC);
    CHK_PTR(pHandler, L"ScreenTransitionEventHandler", Result);
    CHK_HR(CreateDelegate(pHandler, &ImageManipulationEventHandler::OnEventVisualStateChanged, &pDelegateChanged), L"CreateDelegate", Result);

    //Add StateChanged event handler
    CHK_HR(pVisualGroup->AddVisualStateChangedEventHandler(pDelegateChanged), L"AddVisualStateChangedEventHandler", Result);

    //Kick-start the states transitions, leaving everything to the event handler
    CHK_HR(pImageUC->GoToVisualState( L"Stretch", true ), L"GoToState", Result );

    g_pActiveHost->StartDialog(0);

    Result = pHandler->m_Result;

    if (TPR_PASS == Result)
    {
        // Flush CePerf results
        CHK_HR(PerfScenarioFlushMetrics(FALSE,                  //don't close all sessions
                                        &scenarioGUID,          // Scenario Unique GUID
                                        szNamespace,            // Scenario Namespace
                                        szName,                 // Scenario Name
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

    if (g_ResourceModule)
    {
        g_pTestApplication->RemoveResourceModule(g_ResourceModule);
        FreeLibrary(g_ResourceModule);
        g_ResourceModule = NULL;
    }

    g_pTestApplication->RegisterResourceManager(NULL);

    SAFE_RELEASE(g_pActiveHost);

    return Result;
}

//////////////////////////////////////////////////////////////////////////
// Scroll the image in scrollviewer using API calls every 20 ms
//////////////////////////////////////////////////////////////////////////
TESTPROCAPI ScrollImagePerfTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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
    WCHAR szResourcePath[MY_MAX_PATH]       = {0};
    WCHAR szTemp[MY_MAX_PATH]               = {0};
    int sourceID                            = 0;
    bool bSessionOpened                     = false;
    ImageManipulationEventHandler* pHandler = NULL;
    int CollectionCount                     = 0;
    XRWindowCreateParams WindowParameters;
    XRXamlSource XamlSource;
    ResourceHandler resource;
    IXRFrameworkElementPtr pRootElement, pRoot;
    IXRDependencyObjectPtr pDO;

    g_pKato->Log(LOG_COMMENT, L"Start PinchStretchImagePerfTest tests");

    //Set up and start the CePerf session
    StringCchCopy( szName, MAX_PATH, lpTestDescription );
    if( g_bUseBaml )
    {
        StringCchCat( szName, MAX_PATH, L" (baml)" );
    }
    StringCchPrintfW( szNamespace, MAX_PATH, XR_TESTNAME_FORMAT, szName );
    //CTK requires us not to use /release in the path
    StringCchCopy( szResultXMLFileName, MAX_PATH, g_bRunningUnderCTK?L"\\" IM_PERF_LOG_NAME:RELEASE_DIR IM_PERF_LOG_NAME );
    g_pKato->Log(LOG_COMMENT, L"Using output file: %s", szResultXMLFileName );

    GUID scenarioGUID = GetGUID(szNamespace);
    CHK_HR(PerfScenarioOpenSession(XRPERF_API_PATH, true),L"PerfScenarioOpenSession for " XRPERF_API_PATH, Result);
    bSessionOpened = true;

    //get test data
    CHK_HR(Getbool(L"IsGraph", &g_bIsGraph), L"Getbool IsGraph", Result); // retrieve value from xml file

    ZeroMemory(&XamlSource, sizeof(XamlSource));

    //load the resource dll
    if (g_bIsGraph)
    {
        if (g_bUseBaml) //Large Graph png baml
        {
            sourceID = IDX_LARGE_GRAPH_BAML;
            StringCchPrintfW( szResourcePath, MY_MAX_PATH, g_bRunningUnderCTK?BAML_RESOURCE_GRAPH_NAME:RELEASE_DIR BAML_RESOURCE_GRAPH_NAME);
        }
        else //Large Graph png xaml
        {
            sourceID = IDX_LARGE_GRAPH;
            StringCchPrintfW( szResourcePath, MY_MAX_PATH, g_bRunningUnderCTK?XAML_RESOURCE_GRAPH_NAME:RELEASE_DIR XAML_RESOURCE_GRAPH_NAME);
        }
    }
    else 
    {
        if (g_bUseBaml) //Large Photo png baml
        {
            sourceID = IDX_LARGE_PHOTO_BAML;
            StringCchPrintfW( szResourcePath, MY_MAX_PATH, g_bRunningUnderCTK?BAML_RESOURCE_PHOTO_NAME:RELEASE_DIR BAML_RESOURCE_PHOTO_NAME);
        }
        else //Large Photo png xaml
        {
            sourceID = IDX_LARGE_PHOTO;
            StringCchPrintfW( szResourcePath, MY_MAX_PATH, g_bRunningUnderCTK?XAML_RESOURCE_PHOTO_NAME:RELEASE_DIR XAML_RESOURCE_PHOTO_NAME);
        }
    }

    g_pKato->Log(LOG_COMMENT, L"Will load resource module: %s", szResourcePath);
    g_ResourceModule = LoadLibrary(szResourcePath);
    CHK_PTR(g_ResourceModule, L"ResourceModule", Result);
    CHK_HR(g_pTestApplication->AddResourceModule(g_ResourceModule), L"AddResourceModue", Result);
    XamlSource.SetResource(g_ResourceModule, sourceID);
    g_pKato->Log(LOG_COMMENT, L"Loaded sourceID: %i", sourceID);

    CHK_HR(g_pTestApplication->RegisterResourceManager(&resource), L"RegisterResourceManager", Result);

    ZeroMemory(&WindowParameters, sizeof(WindowParameters));
    WindowParameters.Style  = WS_POPUP;
    WindowParameters.pTitle = lpTestDescription;
    WindowParameters.Left   = 0;
    WindowParameters.Top    = 0;
    WindowParameters.Width  = g_ScrnWidth;
    WindowParameters.Height = g_ScrnHeight;

    //initialize global flags for this test pass:
    g_dwImageLoadRet    = TPR_PASS;
    g_DoneRepeats       = 0;
    g_Result            = TPR_FAIL;

    CHK_HR(g_pTestApplication->CreateHostFromXaml(&XamlSource, &WindowParameters, &g_pActiveHost), L"CreateHostFromXaml", Result);

    //size up the xaml to fit desktop
    CHK_HR(g_pActiveHost->GetRootElement(&pRootElement), L"GetRootElement", Result);
    CHK_DO(pRootElement, L"IXRFrameworkElement", Result);
    CHK_HR(pRootElement->SetWidth((float)g_ScrnWidth), L"SetWidth", Result)
    CHK_HR(pRootElement->SetHeight((float)g_ScrnHeight), L"SetHeight", Result)

    CHK_HR(g_pActiveHost->GetContainerHWND(&NativeHWND), L"Error - VisualHost::GetContainerHWND failure in ScrollImagePerfTest", Result);
    CHK_HR(g_pActiveHost->ShowWindow(), L"Error - VisualHost::ShowWindow failure in ScrollImagePerfTest", Result);
    CHK_BOOL(UpdateWindow(NativeHWND)!=FALSE, L"Error - UpdateWindow failure in ScrollImagePerfTest", Result);
    //check for missing images:
    if (TPR_PASS != g_dwImageLoadRet)
    {
            g_pKato->Log(LOG_COMMENT, L"Failed to load all the images in xaml");
            Result = g_dwImageLoadRet;
            goto Exit;
    }

    //get the scroller
    CHK_HR(GetTargetElementName(szTemp,MY_MAX_PATH),L"GetTargetElementName",Result);
    CHK_HR(GetTargetElement(g_pActiveHost,szTemp,&pDO), L"GetTargetElement", Result);
    CHK_DO(pDO, L"IXRDependencyObject", Result);
    CHK_HR(pDO->QueryInterface(IID_IXRScrollViewer, (void**)&g_pScroller), L"QueryInterface", Result);
    CHK_DO(g_pScroller, L"IXRScollViewer", Result);

    //change default direction if we're doing horizontal scrolling
    if ((TC_GRAPH_H_SCROLL == TestID)||(TC_PHOTO_H_SCROLL == TestID))
    {
        g_Direction = IM_RIGHT;
    }
    else
    {
        g_Direction = IM_DOWN;
    }

    //Start the timer that scrolls the list every 20 ms
    if (SetTimer(NativeHWND, 100, 20, TimerProc_Scroll) == 0)
    {
        g_pKato->Log(LOG_COMMENT, L"SetTimer Failed");
        goto Exit;
    }

    g_pActiveHost->StartDialog(0);

    Result = g_Result;

    if (TPR_PASS == Result)
    {
        // Flush CePerf results
        CHK_HR(PerfScenarioFlushMetrics(FALSE,                  //don't close all sessions
                                        &scenarioGUID,          // Scenario Unique GUID
                                        szNamespace,            // Scenario Namespace
                                        szName,                 // Scenario Name
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

    SAFE_RELEASE(g_pScroller);

    if (g_ResourceModule)
    {
        g_pTestApplication->RemoveResourceModule(g_ResourceModule);
        FreeLibrary(g_ResourceModule);
        g_ResourceModule = NULL;
    }

    g_pTestApplication->RegisterResourceManager(NULL);

    SAFE_RELEASE(g_pActiveHost);

    return Result;
}
