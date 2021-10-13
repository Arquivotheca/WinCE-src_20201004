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


#include "AppLoadTest.h"
#include "globals.h"
#include <PerfTestUtils.h>
#include <GetGuidUtil.h>

// Required by CePerf
LPVOID pCePerfInternal;

// XR CePerf Path
// The following functions are used to determine where to put the resulting
// performance output file.
WCHAR g_szPerfOutputFilename[MAX_PATH] = {0};

// Define the statistics we wish to capture
enum {PERF_ITEM_DUR_APPLOAD,
      NUM_PERF_ITEMS1};

DWORD DEFAULT_PERF_FLAGS = CEPERF_RECORD_ABSOLUTE_TICKCOUNT | CEPERF_DURATION_RECORD_MIN;

//---------------------------------------------------------------------------
// Purpose:
//  Makes sure the XRShell that launches at startup is killed before the test begins
//
// Arguments:
//  none
//
// Returns:
//  S_OK if XRShell was killed or not running, E_FAIL if XRShell was running and failed to stop
//---------------------------------------------------------------------------
HRESULT KillXRShell( bool* pbXRShellWasStopped )
{
    HWND hXRAppWindow = FindWindow(L"DesktopExplorerWindow", L"XRShell");
    *pbXRShellWasStopped = false;

    if( hXRAppWindow == NULL )
    {
        g_pKato->Log(LOG_DETAIL, L"XRShell was not running, no need to kill it.");
        return S_OK;
    }
    g_pKato->Log(LOG_DETAIL, L"Sending WM_CLOSE to XRShell");
    SendMessage(hXRAppWindow, WM_CLOSE, false, 0);  //Attempt a graceful close

    Sleep( 2000 );

    if( IsWindow( hXRAppWindow ) )
    {   //XRShell didn't close in response to the WM_CLOSE
        g_pKato->Log(LOG_DETAIL, L"Failed to stop XRShell. Please check for XRShell.cmdline file in release directory.");
        return E_FAIL;
    }

    *pbXRShellWasStopped = true;
    return S_OK;
}

//---------------------------------------------------------------------------
// Purpose:
//  Take performance measurement.
//
// Arguments:
//  Standard Tux TESTPROC arguments.
//
// Returns:
//  TPR_PASS on pass, TPR_FAIL otherwise.
//---------------------------------------------------------------------------
TESTPROCAPI XRAppStartupPerfTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{

    CEPERF_BASIC_ITEM_DESCRIPTOR rgPerfItems1[] = {
        {
            INVALID_HANDLE_VALUE,
            CEPERF_TYPE_DURATION,
            L"First Paint",
            DEFAULT_PERF_FLAGS | CEPERF_DURATION_RECORD_SHARED
        }
    };

    UNREFERENCED_PARAMETER(tpParam);

    // The shell doesn't necessarily want us to execute the test.
    // Make sure before executing.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    bool bAppStarted = false;
    bool bRegisterSuccessful = false;
    bool bPerfScenOpened = false;
    bool bCePerfOpened = false;
    INT   iRet        = TPR_FAIL;
    WCHAR szScenarioName[STR_SIZE];
    DWORD dwErr       = 0;
    GUID guidXRAppPerf = GetGUID(g_lpszPerfScenarioNamespace);
    HRESULT hr;
    HANDLE hPerfSession = INVALID_HANDLE_VALUE;
    LPPROCESS_INFORMATION piRunningProcessInfo = {0};
    LPWSTR tempNameStr = new WCHAR[MAX_PATH];

    //-------------------------------------------------------------------------
    // 1. Perform necessary setup
    //-------------------------------------------------------------------------
    bool bXRShellWasKilled = false;
    
    if( wcsnicmp( g_lpszExeName, L"xrshell", 7 ) == 0 )
    {
        CHK_HR( KillXRShell( &bXRShellWasKilled ), L"KillXRShell", iRet );
    }

    // PerfScenario successfully initialized.
    CEPERF_SESSION_INFO csi;
    ZeroMemory(&csi, sizeof(CEPERF_SESSION_INFO));
    csi.wVersion          = 1;
    csi.dwStorageFlags    = CEPERF_STORE_DEBUGOUT; 
    csi.rgdwRecordingFlags[CEPERF_TYPE_DURATION]          = DEFAULT_PERF_FLAGS | CEPERF_DURATION_RECORD_SHARED;
    csi.rgdwRecordingFlags[CEPERF_TYPE_STATISTIC]         = 0;
    csi.rgdwRecordingFlags[CEPERF_TYPE_LOCALSTATISTIC]    = 0;

    // Open CePerf session
    CHK_HR(CePerfOpenSession(&hPerfSession, g_lpszCePerfSessionName , CEPERF_STATUS_RECORDING_ENABLED | CEPERF_STATUS_STORAGE_ENABLED, &csi), L"CePerfOpenSession", iRet);
    bCePerfOpened = true;

    // Register the performance items with CEPerf
    CHK_HR(CePerfRegisterBulk(hPerfSession, rgPerfItems1, NUM_PERF_ITEMS1, 0), L"CePerfRegisterBulk", iRet);
    bRegisterSuccessful = true;
    
    // Open PerfScenario session
    CHK_HR(PerfScenarioOpenSession(g_lpszPerfScenarioSessionName, TRUE), L"PerfScenarioOpenSession", iRet);
    bPerfScenOpened = true;

    // Set the output filename.
    //CTK requires us not to use /release in the path
    StringCchCopy( g_szPerfOutputFilename, MAX_PATH, g_bRunningUnderCTK?L"\\" XR_OUTFILE:RELEASE_DIR XR_OUTFILE );
    g_pKato->Log(LOG_COMMENT, L"Using output file: %s", g_szPerfOutputFilename );

    // set up the Perf Scenario reporting name and reporting namespace
    StringCchPrintfW( szScenarioName, STR_SIZE, L"%s %s", g_lpszProcessName, FIRSTFRAME );

    g_pKato->Log(LOG_DETAIL, L"");
    g_pKato->Log(LOG_DETAIL, L"TEST: Perform measurements.");

    //------------------------------------------------------------------------
    // Begin Duration Measurement
    //------------------------------------------------------------------------
    CHK_HR(CePerfBeginDuration(rgPerfItems1[PERF_ITEM_DUR_APPLOAD].hTrackedItem), L"CePerfBeginDuration", iRet);

    //------------------------------------------------------------------------
    // Start XRShell (or any XR app that we want to measure Duration until "First Frame")
    //------------------------------------------------------------------------
    g_pKato->Log(LOG_DETAIL, L"TEST: Will create process: %s", g_lpszExeName);
    CHK_BOOL(TRUE == CreateProcess(g_lpszExeName, g_lpszParams, NULL, NULL, FALSE, 0, NULL, NULL, NULL, piRunningProcessInfo), L"CreateProcess", iRet);
    bAppStarted = true;

    Sleep(g_SleepSeconds * 1000);

    //------------------------------------------------------------------------
    // End Duration Measurement is done within the Xaml Runtime code.
    //------------------------------------------------------------------------

    //------------------------------------------------------------------------
    // 4. Flush performance results.
    //------------------------------------------------------------------------
    g_pKato->Log(LOG_DETAIL, L"");
    g_pKato->Log(LOG_DETAIL, 
                 L"TEST: Use PerfScenario to record duration of starting application.");


    // Flush CePerf/PerfScenario data
    CHK_HR(PerfScenarioFlushMetrics(
                                        FALSE,
                                        &guidXRAppPerf,
                                        g_lpszPerfScenarioNamespace,
                                        szScenarioName,
                                        g_szPerfOutputFilename,
                                        NULL,
                                        NULL
                                        ), L"PerfScenarioFlushMetrics", iRet);
    
    iRet = TPR_PASS;


Exit:
    //------------------------------------------------------------------------
    // 5. Tear down CePerf and PerfScenario sessions.
    //------------------------------------------------------------------------
    // Close the PerfScenario session
    if(bPerfScenOpened)
    {
        hr = PerfScenarioCloseSession(g_lpszPerfScenarioSessionName );
        if (FAILED(hr))
        {
            g_pKato->Log(LOG_FAIL, L"PerfScenarioCloseSession FAILED - hr: %#x", hr);
            iRet = TPR_FAIL;
        }
    }

    // Deregister tracked items.
    if(bRegisterSuccessful)
    {
        hr = CePerfDeregisterBulk(rgPerfItems1, NUM_PERF_ITEMS1);
        if (FAILED(hr))
        {
            g_pKato->Log(LOG_FAIL, L"CePerfDeregisterBulk FAILED - hr: %#x", hr);
            iRet = TPR_FAIL;
        }
    }

    // Close the CEPerf session.
    if(bCePerfOpened)
    {
        hr = CePerfCloseSession(hPerfSession);
        if (FAILED(hr))
        {
            g_pKato->Log(LOG_FAIL, L"CePerfCloseSession FAILED - hr: %#x", hr);
            iRet = TPR_FAIL;
        }
    }

    ////--------------------------------------------------------------------
    //// 9. Close App
    ////--------------------------------------------------------------------
    if(bAppStarted && g_bCloseWindow)
    {
        HWND hXRAppWindow = GetForegroundWindow();

        if(!hXRAppWindow)
        {
            g_pKato->Log(LOG_FAIL, L"FindWindow did not return a HWND!");
        }
        SendMessage(hXRAppWindow, WM_CLOSE, false, 0);
    }

    if( bXRShellWasKilled )
    {
        g_pKato->Log(LOG_FAIL, L"Restarting XRShell");
        Sleep(2000);    //give the test window time to close
        if( FALSE==CreateProcess(L"XRShell", L"", NULL, NULL, FALSE, 0, NULL, NULL, NULL, piRunningProcessInfo))
        {
            g_pKato->Log(LOG_FAIL, L"Failed to restart XRShell");
        }
    }

    // Clean up strings Variables
    CLEAN_PTR ( g_lpszExeName );
    CLEAN_PTR ( g_lpszCePerfSessionName );
    CLEAN_PTR ( g_lpszPerfScenarioNamespace );
    CLEAN_PTR ( g_lpszPerfScenarioSessionName );
    CLEAN_PTR ( g_lpszProcessName );
    CLEAN_PTR ( g_lpszParams );
    CLEAN_PTR ( g_szAppPath );
    CLEAN_PTR ( tempNameStr );
    //CLEAN_PTR ( g_lpszClassName );

    return iRet;
}