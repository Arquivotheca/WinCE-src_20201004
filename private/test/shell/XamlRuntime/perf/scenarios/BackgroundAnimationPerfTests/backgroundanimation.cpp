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

Copyright (c) 2002 Microsoft Corporation

Module Name:

    BackgroundAnimation.cpp

Abstract:


Environment:

    Win32 CE 


-------------------------------------------------------------------*/
#include <windows.h>
#include <string.h>
#include <PerfScenario.h>
#include "globals.h"
#include <GetGuidUtil.h>
#include "TuxProc.h"
#include "ResManager.h"
#include "resource.h"
#include "BackgroundAnimation.h"

VOID CALLBACK TimerProc_EndDialog(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    if (KillTimer(hwnd, idEvent))
    {
        g_pKato->Log(LOG_COMMENT, TEXT("gTimer successfully destroyed."));
    }

    g_pActiveHost->EndDialog(NULL);
}

TESTPROCAPI BackgroundAnimationPerfTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    TUX_EXECUTE;

    int Result                          = TPR_FAIL;
    HRESULT hr                          = E_FAIL;;
    LPCTSTR lpTestDescription           = lpFTE->lpDescription;
    HWND NativeHWND                     = NULL;
    WCHAR szResultXMLFileName[MAX_PATH] = {0};
    WCHAR szNamespace[MAX_PATH]         = {0};
    WCHAR szName[MAX_PATH]              = {0};
    int sourceID                        = 0;
    bool bSessionOpened                 = false;
    XRXamlSource XamlSource;
    ResourceHandler resource;
    IXRFrameworkElementPtr  pRootElement;
    IXRStoryboardPtr pStoryboard;

    g_pKato->Log(LOG_COMMENT, TEXT("Start LoadImageTest tests"));

    //Set up and start the CePerf session
    StringCchCopy( szName, MAX_PATH, lpTestDescription );
    if( g_bUseBaml )
    {
        StringCchCat( szName, MAX_PATH, L" (baml)" );
    }
    StringCchPrintfW( szNamespace, MAX_PATH, XR_TESTNAME_FORMAT, szName );
    //CTK requires us not to use /release in the path
    StringCchCopy( szResultXMLFileName, MAX_PATH, g_bRunningUnderCTK?L"\\" L"XRBackgroundAnimationPerfResults.xml":RELEASE_DIR L"XRBackgroundAnimationPerfResults.xml");
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
            g_pKato->Log(LOG_DETAIL, L"Failed to load image in xaml");
            Result = g_dwImageLoadRet;
            goto Exit;
    }

    //size up the xaml to fit desktop
    CHK_HR(g_pActiveHost->GetRootElement(&pRootElement), L"GetRootElement", Result);
    CHK_DO(pRootElement, L"RootElement", Result);
    CHK_HR(pRootElement->SetWidth((float)g_ScrnWidth), L"SetWidth", Result)
    CHK_HR(pRootElement->SetHeight((float)g_ScrnHeight), L"SetHeight", Result)

    g_pActiveHost->GetContainerHWND(&NativeHWND);
    CHK_BOOL(SetWindowPos(NativeHWND, NULL, 0, 0, g_ScrnWidth, g_ScrnHeight, SWP_SHOWWINDOW) != 0, L"SetWindowPos", Result);
    g_pActiveHost->ShowWindow();
    UpdateWindow(NativeHWND);

    CHK_HR(pRootElement->FindName(L"Init", &pStoryboard), L"FindName", Result);

    if (pStoryboard)
    {
        //start storyboard
        CHK_HR(pStoryboard->Begin(), L"Storyboard->Begin", Result);
    }

    //start the dialog with a timer:
    if (SetTimer(NativeHWND, 100, g_OptionSeconds*1000, TimerProc_EndDialog) == 0)
    {
        g_pKato->Log(LOG_COMMENT, L"SetTimer Failed");
        goto Exit;
    }

    g_pActiveHost->StartDialog(0);

    // Flush CePerf results
    CHK_HR(PerfScenarioFlushMetrics(FALSE,                  //don't close all sessions
                                    &scenarioGUID,          // Scenario Unique GUID
                                    szNamespace,            // Scenario Namespace
                                    szName,                 // Scenario Name
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
    return Result;
}
