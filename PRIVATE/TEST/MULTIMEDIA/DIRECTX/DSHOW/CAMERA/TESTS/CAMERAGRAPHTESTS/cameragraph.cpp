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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//
//  Module: Video_API.cpp
//          Contains Video capture Filter Interface(s) tests
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////
#include <windows.h>
#include "logging.h"
#include "Globals.h"
#include "captureframework.h"
#include <imaging.h>
#include <GdiplusEnums.h>

CAPTUREFRAMEWORK g_CaptureGraph;
int g_nAudioDeviceCount;

PCAPTUREFRAMEWORK GetCaptureGraphAndAdjustLayout(DWORD *dwGraphLayout)
{
    if(NULL == dwGraphLayout)
    {
        FAIL(TEXT("Invalid graph layout pointer given."));
    }
    else
    {
        // if we have an audio driver available, or this graph doesn't contain audio, then return the graph pointer.
        if(g_nAudioDeviceCount > 0 || !(*dwGraphLayout  & AUDIO_CAPTURE_FILTER))
        {
            return &g_CaptureGraph;
        }
        else
        {
            DWORD dwTmpLayout = *dwGraphLayout & COMPONENTS_MASK;
            if(dwTmpLayout == ALL_COMPONENTS || dwTmpLayout == ALL_COMPONENTS_INTELI_CONNECT_VIDEO)
            {
                *dwGraphLayout  &= ~AUDIO_CAPTURE_FILTER;
                return &g_CaptureGraph;
            }
        }

        SKIP(TEXT("Audio isn't present for testing this graph combination"));
    }
    return NULL;
}


//#define SAVE_CAPTURE_FILES

static TCHAR g_szClassName[] = TEXT("CameraTest");

void
pumpMessages(void)
{
    MSG     msg;

    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

LRESULT APIENTRY WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    RECT rc;

    switch(msg)
    {
        // handle the WM_SETTINGCHANGE message indicating rotation, move the window.
        case WM_SETTINGCHANGE:
            if(wParam == SETTINGCHANGE_RESET)
            {
                SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);
                MoveWindow(hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, FALSE);
            }
            break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);

}

HWND GetWindow()
{
    HWND hwnd;
    WNDCLASS wc;

    memset(&wc, 0, sizeof(WNDCLASS));
    wc.lpfnWndProc = (WNDPROC) WndProc;
    wc.hInstance = globalInst;
    wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = g_szClassName;

    // RegisterClass can fail if the previous test bombed out on cleanup (due to an exception or such).
    // so, if we allow ourselves to continue even if the registering failed, 
    // we might be able to run a test which would otherwise be blocked.
    RegisterClass(&wc);

    hwnd = CreateWindow( g_szClassName, TEXT("Camera Test Application"),
                                        WS_VISIBLE | WS_BORDER,
                                        CW_USEDEFAULT, CW_USEDEFAULT,
                                        CW_USEDEFAULT, CW_USEDEFAULT,
                                        NULL, NULL, globalInst, NULL);

    SetFocus(hwnd);
    SetForegroundWindow(hwnd);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    Sleep(100);                   // NT4.0 needs this
    pumpMessages();
    Sleep(100);                   // NT4.0 needs this

    if(!hwnd)
    {
        ABORT(TEXT("Failed to create a window for testing."));
    }

    return hwnd;
}

void
ReleaseWindow(HWND hwnd)
{
    DestroyWindow(hwnd);
    UnregisterClass(g_szClassName, globalInst);
}

TESTPROCAPI GraphBuild( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    HRESULT hr = S_OK;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {

            // for each of the graph layouts given, create and destroy it 10 times
            // to make sure that creation is solid.
            for(int i = 0; i < 5; i++)
            {
                Log(TEXT("Building the capture graph, iteration %d"), i);

                if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
                {
                    // if this isn't supported by one of the components in the graph, then it's OK.

                    if(hr == E_GRAPH_UNSUPPORTED || (dwGraphLayout & VIDEO_RENDERER && !(dwGraphLayout & VIDEO_CAPTURE_FILTER)))
                    {
                        DETAIL(TEXT("Initializing the capture graph failed as expected."));
                    }
                    else
                    {
                        FAIL(TEXT("Initializing the capture graph failed."));
                    }
                }
                else
                {
                    if(FAILED(pCaptureGraph->RunGraph()))
                    {
                        if(dwGraphLayout == VIDEO_ENCODER)
                        {
                            DETAIL(TEXT("Running the graph failed as expected."));
                        }
                        else
                        {
                            FAIL(TEXT("Running the capture graph failed."));
                        }
                    }

                    // let the graph run for a little bit.
                    Sleep(200);
                }

                if(FAILED(pCaptureGraph->Cleanup()))
                {
                    FAIL(TEXT("Cleaning up the graph failed."));
                }
            }
        }

        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI GraphState( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    HRESULT hr = S_OK;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                if(hr == E_GRAPH_UNSUPPORTED || (dwGraphLayout & VIDEO_RENDERER && !(dwGraphLayout & VIDEO_CAPTURE_FILTER)))
                {
                    DETAIL(TEXT("Initializing the capture graph failed as expected."));
                }
                else
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                }
            }
            else
            {
                Log(TEXT("Testing run-pause-run scenario"));

                // we're paused, so try pausing first, since we're already paused
                if(FAILED(pCaptureGraph->PauseGraph()))
                {
                    FAIL(TEXT("Re-pausing the capture graph failed."));
                }

                // now stop the graph
                if(FAILED(pCaptureGraph->StopGraph()))
                {
                    FAIL(TEXT("Repeated stopping the capture graph failed."));
                }

                // exercise graph state changes run/pause to verify stability
                for(int i = 0; i < 100; i++)
                {
                    if(0 == i%10)
                        Log(TEXT("Pausing and running the graph"));

                    if(FAILED(pCaptureGraph->RunGraph()))
                    {
                        FAIL(TEXT("Repeated running the capture graph failed."));
                    }

                    if(FAILED(pCaptureGraph->PauseGraph()))
                    {
                        FAIL(TEXT("Repeated pausing the capture graph failed."));
                     }
                }

                // we're paused, so try pausing again
                if(FAILED(pCaptureGraph->PauseGraph()))
                {
                    FAIL(TEXT("Re-pausing the capture graph failed."));
                }

                if(FAILED(pCaptureGraph->RunGraph()))
                {
                    FAIL(TEXT("Restarting the capture graph failed."));
                }

                // both the video renderer and capture filter are necessary to verify the preview.
                if(dwGraphLayout & VIDEO_RENDERER && (dwGraphLayout & VIDEO_CAPTURE_FILTER))
                {
                    if(FAILED(pCaptureGraph->VerifyPreviewWindow()))
                    {
                        FAIL(TEXT("The preview window wasn't correct."));
                    }
                }

                Log(TEXT("Testing run-stop-run scenario"));

                // exercise graph state changes run/pause to verify stability
                for(int i = 0; i < 100; i++)
                {
                    if(0 == i%10)
                        Log(TEXT("Stopping and running the graph"));

                    if(FAILED(pCaptureGraph->RunGraph()))
                    {
                        FAIL(TEXT("Repeated running the capture graph failed."));
                    }

                    if(FAILED(pCaptureGraph->StopGraph()))
                    {
                        FAIL(TEXT("Repeated stopping the capture graph failed."));
                    }
                }

                // the graph is stopped, so try stopping again
                if(FAILED(pCaptureGraph->StopGraph()))
                {
                    FAIL(TEXT("Stopping the capture graph failed."));
                }

                if(FAILED(pCaptureGraph->RunGraph()))
                {
                    FAIL(TEXT("Restarting the capture graph failed."));
                }

                // both the video renderer and capture filter are necessary to verify the preview.
                if(dwGraphLayout & VIDEO_RENDERER && (dwGraphLayout & VIDEO_CAPTURE_FILTER))
                {
                    if(FAILED(pCaptureGraph->VerifyPreviewWindow()))
                    {
                        FAIL(TEXT("The preview window wasn't correct."));
                    }
                }

                Log(TEXT("Testing run-pause-stop-pause-run scenario"));

                // exercise graph state changes run/pause to verify stability
                for(int i = 0; i < 100; i++)
                {
                    if(0 == i%10)
                        Log(TEXT("Pausing, stopping, and running the graph"));

                    if(FAILED(pCaptureGraph->RunGraph()))
                    {
                        FAIL(TEXT("Repeated running the capture graph failed."));
                    }

                    if(FAILED(pCaptureGraph->PauseGraph()))
                    {
                        FAIL(TEXT("Repeated pausing the capture graph failed."));
                    }

                    if(FAILED(pCaptureGraph->StopGraph()))
                    {
                        FAIL(TEXT("Repeated stopping the capture graph failed."));
                    }

                    if(FAILED(pCaptureGraph->PauseGraph()))
                    {
                        FAIL(TEXT("Repeated pausing the capture graph failed."));
                    }
                }

                // we're paused, so try pausing again
                if(FAILED(pCaptureGraph->PauseGraph()))
                {
                    FAIL(TEXT("Re-pausing the capture graph failed."));
                }

                // now run, and verify that we can tell it to run again
                if(FAILED(pCaptureGraph->RunGraph()))
                {
                    FAIL(TEXT("Running the capture graph failed."));
                }

                if(FAILED(pCaptureGraph->RunGraph()))
                {
                     FAIL(TEXT("Re-Running the capture graph failed."));
                }

                // the graph is stopped, it shouldn't be able to go into any state from here.
                if(FAILED(pCaptureGraph->StopGraph()))
                {
                    FAIL(TEXT("Stopping the capture graph failed."));
                }

                if(FAILED(pCaptureGraph->StopGraph()))
                {
                        FAIL(TEXT("Re-Stopping the capture graph failed."));
                }

                if(FAILED(pCaptureGraph->RunGraph()))
                {
                    FAIL(TEXT("Running a stopped capture graph failed."));
                }

                if(FAILED(pCaptureGraph->PauseGraph()))
                {
                     FAIL(TEXT("Pausing a stopped capture graph failed."));
                }

                if(FAILED(pCaptureGraph->RunGraph()))
                {
                    FAIL(TEXT("Running a stopped capture graph failed."));
                }

                // both the video renderer and capture filter are necessary to verify the preview.
                if(dwGraphLayout & VIDEO_RENDERER && (dwGraphLayout & VIDEO_CAPTURE_FILTER))
                {
                    if(FAILED(pCaptureGraph->VerifyPreviewWindow()))
                    {
                        FAIL(TEXT("The preview window wasn't correct."));
                    }
                }
            }

            if(FAILED(pCaptureGraph->Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
            }
        }

        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}


TESTPROCAPI VerifyPreview( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    HRESULT hr = S_OK;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                if(hr == E_GRAPH_UNSUPPORTED)
                {
                    DETAIL(TEXT("Initializing the capture graph failed as expected."));
                }
                else
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                }
            }
            else
            {
                if(FAILED(pCaptureGraph->RunGraph()))
                {
                    FAIL(TEXT("Starting the capture graph failed."));
                }
                else
                {
                    for(int i = 0; i < 4; i++)
                    {
                        Log(TEXT("Preview window verification, iteration %d"), i);

                        // this may result in some user interaction
                        if(FAILED(pCaptureGraph->VerifyPreviewWindow()))
                        {
                            FAIL(TEXT("The preview window wasn't correct."));
                        }

                        // delay between verifications
                        Sleep(400);
                    }
                }
            }
            if(FAILED(pCaptureGraph->Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
            }
         }

        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI VerifyPreviewRotationStatic( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    DWORD dwAngles[4] = {DMDO_0, DMDO_90, DMDO_180, DMDO_270 };
    int AngleIndex;
    DWORD dwBaseOrientation;
    HRESULT hr = S_OK;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            for(AngleIndex = 0; AngleIndex < countof(dwAngles); AngleIndex++)
            {
                if(FAILED(pCaptureGraph->GetScreenOrientation(&dwBaseOrientation)))
                {
                    FAIL(TEXT("Failed to retrieve the base orientation."));
                }

                if(FAILED(pCaptureGraph->SetScreenOrientation(dwAngles[AngleIndex])))
               {
                    Log(TEXT("Failed to set angle %d, this is acceptable on devices which don't support rotation."), dwAngles[AngleIndex]);
                    continue;
                }

                // pump the messages to get the window updated
                pumpMessages();

                if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
                {
                    if(hr == E_GRAPH_UNSUPPORTED)
                    {
                        DETAIL(TEXT("Initializing the capture graph failed as expected."));
                    }
                    else
                    {
                        FAIL(TEXT("Initializing the capture graph failed."));
                    }
                }
                else
                {
                    if(FAILED(pCaptureGraph->RunGraph()))
                    {
                        FAIL(TEXT("Starting the capture graph failed."));
                    }
                    else
                    {
                        for(int i = 0; i < 4; i++)
                        {
                            Log(TEXT("Preview window verification, iteration %d"), i);

                            // this may result in some user interaction
                            if(FAILED(pCaptureGraph->VerifyPreviewWindow()))
                            {
                                FAIL(TEXT("The preview window wasn't correct."));
                            }

                            // delay between verifications
                            Sleep(400);
                        }

                        if(FAILED(pCaptureGraph->SetScreenOrientation(dwBaseOrientation)))
                        {
                            // On some devices which don't support dynamic rotation, setting the orientation to the current orientation fails.
                            Log(TEXT("Failed to restore base orientation, this is acceptable on devices which don't support rotation."));
                        }
                    }
                }
                if(FAILED(pCaptureGraph->Cleanup()))
                {
                    FAIL(TEXT("Cleaning up the graph failed."));
                }
            }
        }

        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI VerifyPreviewRotationDynamic( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    DWORD dwAngles[4] = {DMDO_0, DMDO_90, DMDO_180, DMDO_270 };
    int AngleIndex;
    DWORD dwBaseOrientation;
    HRESULT hr = S_OK;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                if(hr == E_GRAPH_UNSUPPORTED)
                {
                    DETAIL(TEXT("Initializing the capture graph failed as expected."));
                }
                else
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                }
            }
            else
            {
                if(FAILED(pCaptureGraph->RunGraph()))
                {
                    FAIL(TEXT("Starting the capture graph failed."));
                }
                else
                {
                    if(FAILED(pCaptureGraph->GetScreenOrientation(&dwBaseOrientation)))
                    {
                        FAIL(TEXT("Failed to retrieve the base orientation."));
                    }

                    for(AngleIndex = 0; AngleIndex < countof(dwAngles); AngleIndex++)
                    {
                        for(int i = 0; i < 4; i++)
                        {
                            Log(TEXT("Preview window verification, iteration %d"), i);

                            if(FAILED(pCaptureGraph->SetScreenOrientation(dwAngles[AngleIndex])))
                            {
                                Log(TEXT("Failed to set angle %d, this is acceptable on devices which don't support rotation."), dwAngles[AngleIndex]);
                                continue;
                            }

                            // pump the messages to get the window updated
                            pumpMessages();

                            Sleep(400);

                            // this may result in some user interaction
                            if(FAILED(pCaptureGraph->VerifyPreviewWindow()))
                            {
                                FAIL(TEXT("The preview window wasn't correct."));
                            }
                        }
                    }

                    if(FAILED(pCaptureGraph->SetScreenOrientation(dwBaseOrientation)))
                    {
                        // On some devices which don't support dynamic rotation, setting the orientation to the current orientation fails.
                        Log(TEXT("Failed to restore the base orientation, this is acceptable on devices which don't support rotation."));
                    }
                }
            }
            if(FAILED(pCaptureGraph->Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
            }
         }

        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI VerifyPreviewRotationStatic_NoStretch( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    DWORD dwAngles[4] = {DMDO_0, DMDO_90, DMDO_180, DMDO_270 };
    int AngleIndex;
    DWORD dwBaseOrientation;
    HRESULT hr = S_OK;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            for(AngleIndex = 0; AngleIndex < countof(dwAngles); AngleIndex++)
            {
                if(FAILED(pCaptureGraph->GetScreenOrientation(&dwBaseOrientation)))
                {
                    FAIL(TEXT("Failed to retrieve the base orientation."));
                }

                if(FAILED(pCaptureGraph->SetScreenOrientation(dwAngles[AngleIndex])))
               {
                    Log(TEXT("Failed to set angle %d, this is acceptable on devices which don't support rotation."), dwAngles[AngleIndex]);
                    continue;
                }

                // pump the messages to get the window updated
                pumpMessages();

                if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
                {
                    if(hr == E_GRAPH_UNSUPPORTED)
                    {
                        DETAIL(TEXT("Initializing the capture graph failed as expected."));
                    }
                    else
                    {
                        FAIL(TEXT("Initializing the capture graph failed."));
                    }
                }
                else
                {
                    if(FAILED(pCaptureGraph->RunGraph()))
                    {
                        FAIL(TEXT("Starting the capture graph failed."));
                    }
                    else
                    {
                        for(int i = 0; i < 4; i++)
                        {
                            Log(TEXT("Preview window verification, iteration %d"), i);

                            // this may result in some user interaction
                            if(FAILED(pCaptureGraph->VerifyPreviewWindow()))
                            {
                                FAIL(TEXT("The preview window wasn't correct."));
                            }

                            // delay between verifications
                            Sleep(400);
                        }

                        if(FAILED(pCaptureGraph->SetScreenOrientation(dwBaseOrientation)))
                        {
                            // On some devices which don't support dynamic rotation, setting the orientation to the current orientation fails.
                            Log(TEXT("Failed to restore base orientation, this is acceptable on devices which don't support rotation."));
                        }
                    }
                }
                if(FAILED(pCaptureGraph->Cleanup()))
                {
                    FAIL(TEXT("Cleaning up the graph failed."));
                }
            }
        }

        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI VerifyPreviewRotationDynamic_NoStretch( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    DWORD dwAngles[4] = {DMDO_0, DMDO_90, DMDO_180, DMDO_270 };
    int AngleIndex;
    DWORD dwBaseOrientation;
    HRESULT hr = S_OK;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                if(hr == E_GRAPH_UNSUPPORTED)
                {
                    DETAIL(TEXT("Initializing the capture graph failed as expected."));
                }
                else
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                }
            }
            else
            {
                if(FAILED(pCaptureGraph->RunGraph()))
                {
                    FAIL(TEXT("Starting the capture graph failed."));
                }
                else
                {
                    if(FAILED(pCaptureGraph->GetScreenOrientation(&dwBaseOrientation)))
                    {
                        FAIL(TEXT("Failed to retrieve the base orientation."));
                    }

                    for(AngleIndex = 0; AngleIndex < countof(dwAngles); AngleIndex++)
                    {
                        for(int i = 0; i < 4; i++)
                        {
                            Log(TEXT("Preview window verification, iteration %d"), i);

                            if(FAILED(pCaptureGraph->SetScreenOrientation(dwAngles[AngleIndex])))
                            {
                                Log(TEXT("Failed to set angle %d, this is acceptable on devices which don't support rotation."), dwAngles[AngleIndex]);
                                continue;
                            }

                            // pump the messages to get the window updated
                            pumpMessages();

                            Sleep(400);

                            // this may result in some user interaction
                            if(FAILED(pCaptureGraph->VerifyPreviewWindow()))
                            {
                                FAIL(TEXT("The preview window wasn't correct."));
                            }
                        }
                    }

                    if(FAILED(pCaptureGraph->SetScreenOrientation(dwBaseOrientation)))
                    {
                        // On some devices which don't support dynamic rotation, setting the orientation to the current orientation fails.
                        Log(TEXT("Failed to restore the base orientation, this is acceptable on devices which don't support rotation."));
                    }
                }
            }
            if(FAILED(pCaptureGraph->Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
            }
         }

        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI VerifyPreviewFormats( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    AM_MEDIA_TYPE **ppAMT = NULL;
    int i, iSize, iCount;
    VIDEO_STREAM_CONFIG_CAPS *pvscc = NULL;
    VIDEORENDERER_STATS vrs;
    HRESULT hr = S_OK;
    DWORD dwStream = STREAM_PREVIEW;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                if(hr == E_GRAPH_UNSUPPORTED)
                {
                    DETAIL(TEXT("Initializing the capture graph failed as expected."));
                }
                else
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                }
            }
            else
            {
                // if we can't retrieve the number of capabilities, then it's because there's no preview stream available.
                if(FAILED(hr = pCaptureGraph->GetNumberOfCapabilities(STREAM_PREVIEW, &iCount, &iSize)))
                {
                    if(hr == VFW_E_NO_INTERFACE)
                    {
                        // if there's no preview pin, then do this test using the capture pin.
                        if(FAILED(hr = pCaptureGraph->GetNumberOfCapabilities(STREAM_CAPTURE, &iCount, &iSize)))
                        {
                            SKIP(TEXT("Failed to retrieve the number of capabilities, preview stream not available."));
                            goto cleanup;
                        }
                        else dwStream = STREAM_CAPTURE;
                    }
                    else
                    {
                        FAIL(TEXT("Failed to retrieve the number of capabilieis for the preview stream"));
                        goto cleanup;
                    }
                }
                // if retrieving the preview caps works, then use the preview stream config interface.
                else dwStream = STREAM_PREVIEW;

                if(iSize != sizeof(VIDEO_STREAM_CONFIG_CAPS))
                {
                    FAIL(TEXT("size of the configuration structure is incorrect, expected sizeof VIDEO_STREAM_CONFIG_CAPS."));
                }

                // entry 0 will be a NULL "default" entry, everything else will be retrieved from the graph.
                iCount ++;

                ppAMT = new AM_MEDIA_TYPE*[iCount];
                if(!ppAMT)
                {
                    FAIL(TEXT("Failed to allocate the media type array"));
                    goto cleanup;
                }
                // clear out the space just allocated
                memset(ppAMT, 0, sizeof(AM_MEDIA_TYPE *) * iCount);

                pvscc = new VIDEO_STREAM_CONFIG_CAPS[iCount];
                if(!pvscc)
                {
                    FAIL(TEXT("Failed to allocate the media type array"));
                    goto cleanup;
                }
                // clear out the space just allocated
                memset(pvscc, 0, sizeof(VIDEO_STREAM_CONFIG_CAPS) * iCount);


                // grab all of the formats available

                for(i = 1; i < iCount; i++)
                {
                    // because entry 0 is the "default" entry, the entry id is offset by 1
                    if(FAILED(pCaptureGraph->GetStreamCaps(dwStream, i - 1, &(ppAMT[i]), (BYTE *) &(pvscc[i]))))
                    {
                        FAIL(TEXT("Failed to retrieve the stream caps."));
                        goto cleanup;
                    }
                }

                // cleanup the graph so the new format can be applied to a newly built graph
                if(FAILED(pCaptureGraph->Cleanup()))
                {
                    FAIL(TEXT("Cleaning up the graph failed."));
                }

                // for each of the formats gathered above, re-initialize the graph with the new
                // format and test it.

                for(i = 0; i < iCount; i++)
                {
                    VIDEOINFOHEADER *pVih = NULL;

                    Log(TEXT("Testing preview format %d of %d"), i, iCount);

                    if(ppAMT[i])
                    {
                        if(ppAMT[i]->formattype != FORMAT_VideoInfo)
                            FAIL(TEXT("Didn't receive a video info format type"));

                        if(ppAMT[i]->cbFormat < sizeof(VIDEOINFOHEADER))
                            FAIL(TEXT("data structure size is less than a VIDEOINFOHEADER"));


                        pVih = (VIDEOINFOHEADER *) ppAMT[i]->pbFormat;

                        Log(TEXT("Testing a format of %dx%dx%d."), pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight, pVih->bmiHeader.biBitCount);
                    }

                    if(FAILED(pCaptureGraph->SetFormat(dwStream, ppAMT[i])))
                    {
                        FAIL(TEXT("Failed to set the format of the preview."));
                    }

                    if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
                    {
                        if(hr == E_GRAPH_UNSUPPORTED)
                        {
                            DETAIL(TEXT("Initializing the capture graph failed as expected."));
                        }
                        else
                        {
                            FAIL(TEXT("Initializing the capture graph failed."));
                        }
                    }
                    else
                    {
                        // run the graph
                        if(FAILED(hr = pCaptureGraph->RunGraph()))
                        {
                            if(hr != E_OUTOFMEMORY)
                                FAIL(TEXT("Starting the capture graph failed."));
                            else Log(TEXT("Unable to run the graph due to insufficient memory at this format"));
                        }
                        else
                        {
                            Log(TEXT("Verifying setting an invalid format fails."));
                            if(SUCCEEDED(pCaptureGraph->SetFormat(dwStream, ppAMT[0])))
                            {
                                FAIL(TEXT("Succeeded setting the format of the preview after it was connected which is invalid."));
                            }

                            if(FAILED(pCaptureGraph->GetVideoRenderStats(&vrs)))
                            {
                                FAIL(TEXT("Failed to retrieve the video render stats."));
                            }

                            // let the graph run for some time, so the preview is updated
                            Sleep(1000);

                            // if we set the format, then the resolution rendered should be the one requested.
                            // the format may not be the same if the color converter was added.
                            if(pVih)
                            {
                                if(vrs.lSourceHeight != abs(pVih->bmiHeader.biHeight))
                                {
                                    FAIL(TEXT("Preview source height is not as expected"));
                                    Log(TEXT("Source height is %d, expected %d"), vrs.lSourceHeight, pVih->bmiHeader.biHeight);
                                }

                                if(vrs.lSourceWidth != pVih->bmiHeader.biWidth)
                                {
                                    FAIL(TEXT("Preview source width is not as expected"));
                                    Log(TEXT("Source width is %d, expected %d"), vrs.lSourceWidth, pVih->bmiHeader.biWidth);
                                }
                            }
                            // if we didn't set a format, then we don't know which one was picked.
                            else
                            {
                                Log(TEXT("Best pick by the video renderer was used."));
                            }

                            // this may result in some user interaction, verify the preview is running
                            if(FAILED(pCaptureGraph->VerifyPreviewWindow()))
                            {
                                FAIL(TEXT("The preview window wasn't correct."));
                            }
                        }
                    }
                    // cleanup the graph so the next format can be applied to a newly built graph
                    if(FAILED(pCaptureGraph->Cleanup()))
                    {
                        FAIL(TEXT("Cleaning up the graph failed."));
                    }
                }
            }
        }
    }

cleanup:

    // cleanup the graph on error
    if(pCaptureGraph && FAILED(pCaptureGraph->Cleanup()))
    {
        FAIL(TEXT("Cleaning up the graph failed."));
    }

    if(ppAMT)
    {
        for(i = 0; i < iCount; i++)
        {
            if(ppAMT[i])
            {
                DeleteMediaType(ppAMT[i]);
                ppAMT[i] = NULL;
            }
        }
        delete[] ppAMT;
        ppAMT = NULL;
    }

    if(pvscc)
    {
        delete[] pvscc;
        pvscc = NULL;
    }

    if(hwnd)
    {
        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI VerifyPreviewFormats_NoStretch( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    AM_MEDIA_TYPE **ppAMT = NULL;
    int i, iSize, iCount;
    VIDEO_STREAM_CONFIG_CAPS *pvscc = NULL;
    VIDEORENDERER_STATS vrs;
    HRESULT hr = S_OK;
    DWORD dwStream = STREAM_PREVIEW;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                if(hr == E_GRAPH_UNSUPPORTED)
                {
                    DETAIL(TEXT("Initializing the capture graph failed as expected."));
                }
                else
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                }
            }
            else
            {
                // if we can't retrieve the number of capabilities, then it's because there's no preview stream available.
                if(FAILED(hr = pCaptureGraph->GetNumberOfCapabilities(STREAM_PREVIEW, &iCount, &iSize)))
                {
                    if(hr == VFW_E_NO_INTERFACE)
                    {
                        // if there's no preview pin, then do this test using the capture pin.
                        if(FAILED(hr = pCaptureGraph->GetNumberOfCapabilities(STREAM_CAPTURE, &iCount, &iSize)))
                        {
                            SKIP(TEXT("Failed to retrieve the number of capabilities, preview stream not available."));
                            goto cleanup;
                        }
                        else dwStream = STREAM_CAPTURE;
                    }
                    else
                    {
                        FAIL(TEXT("Failed to retrieve the number of capabilieis for the preview stream"));
                        goto cleanup;
                    }
                }
                // if retrieving the preview caps works, then use the preview stream config interface.
                else dwStream = STREAM_PREVIEW;

                if(iSize != sizeof(VIDEO_STREAM_CONFIG_CAPS))
                {
                    FAIL(TEXT("size of the configuration structure is incorrect, expected sizeof VIDEO_STREAM_CONFIG_CAPS."));
                }

                // entry 0 will be a NULL "default" entry, everything else will be retrieved from the graph.
                iCount ++;

                ppAMT = new AM_MEDIA_TYPE*[iCount];
                if(!ppAMT)
                {
                    FAIL(TEXT("Failed to allocate the media type array"));
                    goto cleanup;
                }
                // clear out the space just allocated
                memset(ppAMT, 0, sizeof(AM_MEDIA_TYPE *) * iCount);

                pvscc = new VIDEO_STREAM_CONFIG_CAPS[iCount];
                if(!pvscc)
                {
                    FAIL(TEXT("Failed to allocate the media type array"));
                    goto cleanup;
                }
                // clear out the space just allocated
                memset(pvscc, 0, sizeof(VIDEO_STREAM_CONFIG_CAPS) * iCount);


                // grab all of the formats available

                for(i = 1; i < iCount; i++)
                {
                    // because entry 0 is the "default" entry, the entry id is offset by 1
                    if(FAILED(pCaptureGraph->GetStreamCaps(dwStream, i - 1, &(ppAMT[i]), (BYTE *) &(pvscc[i]))))
                    {
                        FAIL(TEXT("Failed to retrieve the stream caps."));
                        goto cleanup;
                    }
                }

                // cleanup the graph so the new format can be applied to a newly built graph
                if(FAILED(pCaptureGraph->Cleanup()))
                {
                    FAIL(TEXT("Cleaning up the graph failed."));
                }

                // for each of the formats gathered above, re-initialize the graph with the new
                // format and test it.

                for(i = 0; i < iCount; i++)
                {
                    VIDEOINFOHEADER *pVih = NULL;

                    Log(TEXT("Testing preview format %d of %d"), i, iCount);

                    if(ppAMT[i])
                    {
                        if(ppAMT[i]->formattype != FORMAT_VideoInfo)
                            FAIL(TEXT("Didn't receive a video info format type"));

                        if(ppAMT[i]->cbFormat < sizeof(VIDEOINFOHEADER))
                            FAIL(TEXT("data structure size is less than a VIDEOINFOHEADER"));


                        pVih = (VIDEOINFOHEADER *) ppAMT[i]->pbFormat;

                        Log(TEXT("Testing a format of %dx%dx%d."), pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight, pVih->bmiHeader.biBitCount);
                    }

                    if(FAILED(pCaptureGraph->SetFormat(dwStream, ppAMT[i])))
                    {
                        FAIL(TEXT("Failed to set the format of the preview."));
                    }

                    if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
                    {
                        if(hr == E_GRAPH_UNSUPPORTED)
                        {
                            DETAIL(TEXT("Initializing the capture graph failed as expected."));
                        }
                        else
                        {
                            FAIL(TEXT("Initializing the capture graph failed."));
                        }
                    }
                    else
                    {
                        // run the graph
                        if(FAILED(hr = pCaptureGraph->RunGraph()))
                        {
                            if(hr != E_OUTOFMEMORY)
                                FAIL(TEXT("Starting the capture graph failed."));
                            else Log(TEXT("Unable to run the graph due to insufficient memory at this format"));
                        }
                        else
                        {
                            if(FAILED(pCaptureGraph->GetVideoRenderStats(&vrs)))
                            {
                                FAIL(TEXT("Failed to retrieve the video render stats."));
                            }

                            // let the graph run for some time, so the preview is updated
                            Sleep(1000);

                            // if we set the format, then the resolution rendered should be the one requested.
                            // the format may not be the same if the color converter was added.
                            if(pVih)
                            {
                                if(vrs.lSourceHeight != abs(pVih->bmiHeader.biHeight))
                                {
                                    FAIL(TEXT("Preview source height is not as expected"));
                                    Log(TEXT("Source height is %d, expected %d"), vrs.lSourceHeight, pVih->bmiHeader.biHeight);
                                }

                                if(vrs.lSourceWidth != pVih->bmiHeader.biWidth)
                                {
                                    FAIL(TEXT("Preview source width is not as expected"));
                                    Log(TEXT("Source width is %d, expected %d"), vrs.lSourceWidth, pVih->bmiHeader.biWidth);
                                }

                                if(vrs.lDestHeight != abs(pVih->bmiHeader.biHeight))
                                {
                                    FAIL(TEXT("Preview destination height is not as expected"));
                                    Log(TEXT("destination height is %d, expected %d"), vrs.lSourceHeight, pVih->bmiHeader.biHeight);
                                }

                                if(vrs.lDestWidth != pVih->bmiHeader.biWidth)
                                {
                                    FAIL(TEXT("Preview destination width is not as expected"));
                                    Log(TEXT("destination width is %d, expected %d"), vrs.lSourceWidth, pVih->bmiHeader.biWidth);
                                }
                            }
                            // if we didn't set a format, then we don't know which one was picked.
                            else
                            {
                                Log(TEXT("Best pick by the video renderer was used."));
                            }

                            // this may result in some user interaction, verify the preview is running
                            if(FAILED(pCaptureGraph->VerifyPreviewWindow()))
                            {
                                FAIL(TEXT("The preview window wasn't correct."));
                            }
                        }
                    }
                    // cleanup the graph so the next format can be applied to a newly built graph
                    if(FAILED(pCaptureGraph->Cleanup()))
                    {
                        FAIL(TEXT("Cleaning up the graph failed."));
                    }
                }
            }
        }
    }

cleanup:

    // cleanup the graph on error
    if(pCaptureGraph && FAILED(pCaptureGraph->Cleanup()))
    {
        FAIL(TEXT("Cleaning up the graph failed."));
    }

    if(ppAMT)
    {
        for(i = 0; i < iCount; i++)
        {
            if(ppAMT[i])
            {
                DeleteMediaType(ppAMT[i]);
                ppAMT[i] = NULL;
            }
        }
        delete[] ppAMT;
        ppAMT = NULL;
    }

    if(pvscc)
    {
        delete[] pvscc;
        pvscc = NULL;
    }

    if(hwnd)
    {
        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI VerifyPreviewFormatsPostConnect( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    AM_MEDIA_TYPE *ppAMT = NULL;
    VIDEOINFOHEADER *pVih = NULL;
    int i, iSize, iCount;
    VIDEO_STREAM_CONFIG_CAPS pvscc;
    VIDEORENDERER_STATS vrs;
    HRESULT hr = S_OK;
    DWORD dwStream = STREAM_PREVIEW;
    BOOL bOldForceColorConversion = FALSE;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            bOldForceColorConversion = pCaptureGraph->GetForceColorConversion();
            pCaptureGraph->SetForceColorConversion(FALSE);

            if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                SKIP(TEXT("Initializing the capture graph failed, this is OK because the color converter was forced out of the graph."));
            }
            else
            {
                CComPtr<IGraphBuilder> pFilterGraph = NULL;
                CComPtr<IBaseFilter> pColorConverter = NULL;

                if (FAILED(pCaptureGraph->GetBuilderInterface((void**)&pFilterGraph, IID_IGraphBuilder)))
                {
                    FAIL(TEXT("Failed to retrieve the IGraphBuilder interface from the capture framework.."));
                    goto cleanup;
                }

                hr = pFilterGraph->FindFilterByName(TEXT("Color Converter"), &pColorConverter);

                // now that we're done with these, free them
                pFilterGraph = NULL;
                pColorConverter = NULL;

                // if we found the color converter above, then we need to continue on because it
                // doesn't support reconnects with different formats.
                if (SUCCEEDED(hr))
                {
                    SKIP(TEXT("The color conversion filter is in the graph, skipping because it does not support post connect operations."));
                    goto cleanup;
                }

                // if we can't retrieve the number of capabilities, then it's because there's no preview stream available.
                if(FAILED(hr = pCaptureGraph->GetNumberOfCapabilities(STREAM_PREVIEW, &iCount, &iSize)))
                {
                    if(hr == VFW_E_NO_INTERFACE)
                    {
                        // if there's no preview pin, then do this test using the capture pin.
                        if(FAILED(hr = pCaptureGraph->GetNumberOfCapabilities(STREAM_CAPTURE, &iCount, &iSize)))
                        {
                            FAIL(TEXT("Failed to retrieve the number of capabilities for the capture/preview stream"));
                            goto cleanup;
                        }
                        else dwStream = STREAM_CAPTURE;
                    }
                    else
                    {
                        FAIL(TEXT("Failed to retrieve the number of capabilities for the preview stream"));
                        goto cleanup;
                    }
                }
                // if retrieving the preview caps works, then use the preview stream config interface.
                else dwStream = STREAM_PREVIEW;

                if(iSize != sizeof(VIDEO_STREAM_CONFIG_CAPS))
                {
                    FAIL(TEXT("size of the configuration structure is incorrect, expected sizeof VIDEO_STREAM_CONFIG_CAPS."));
                }

                for(i = 0; i < iCount; i++)
                {

                    Log(TEXT("Testing preview format %d of %d"), i + 1, iCount);

                    if(FAILED(pCaptureGraph->GetStreamCaps(dwStream, i, &ppAMT, (BYTE *) &pvscc)))
                    {
                        FAIL(TEXT("Failed to retrieve the stream caps."));
                        goto cleanup;
                    }

                    if(ppAMT)
                    {
                        if(ppAMT->formattype != FORMAT_VideoInfo)
                            FAIL(TEXT("Didn't receive a video info format type"));

                        if(ppAMT->cbFormat < sizeof(VIDEOINFOHEADER))
                            FAIL(TEXT("data structure size is less than a VIDEOINFOHEADER"));

                        pVih = (VIDEOINFOHEADER *) ppAMT->pbFormat;

                        Log(TEXT("Testing a format of %dx%dx%d."), pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight, pVih->bmiHeader.biBitCount);
                    }

                    // if the format is YUV, then skip because the color converter is not in the graph and the video renderer
                    // is not guaranteed to render YUV formats.
                    if(pVih)
                    {
                        // remove the source prerotate flag in case it's set.
                        DWORD dwCompression = pVih->bmiHeader.biCompression & ~BI_SRCPREROTATE;
                        // now if the compression indicates that it's not an RGB bitmap, then continue on.
                        if(dwCompression != BI_RGB && 
                            dwCompression != BI_BITFIELDS && 
                            dwCompression != BI_ALPHABITFIELDS)
                        {
                            Log(TEXT("Unable to exercise YUV formats without a color converter in the graph, continuing on."));
                            continue;
                        }
                    }

                    if(FAILED(pCaptureGraph->SetFormat(dwStream, ppAMT)))
                    {
                        FAIL(TEXT("Failed to set the format of the preview."));
                    }

                    if(pVih && FAILED(pCaptureGraph->SetVideoWindowSize(pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight)))
                    {
                        FAIL(TEXT("Failed to update the preview window dimensions."));
                    }

                    // run the graph
                    if(FAILED(hr = pCaptureGraph->RunGraph()))
                    {
                        if(hr != E_OUTOFMEMORY)
                            FAIL(TEXT("Starting the capture graph failed."));
                        else Log(TEXT("Unable to run the graph due to insufficient memory at this format"));
                    }
                    else
                    {
                        if(FAILED(pCaptureGraph->GetVideoRenderStats(&vrs)))
                        {
                            FAIL(TEXT("Failed to retrieve the video render stats."));
                        }

                        // let the graph run for some time, so the preview is updated
                        Sleep(5000);

                        // if we set the format, then the resolution rendered should be the one requested.
                        // the format may not be the same if the color converter was added.
                        if(pVih)
                        {
                            if(vrs.lSourceHeight != abs(pVih->bmiHeader.biHeight))
                            {
                                FAIL(TEXT("Preview source height is not as expected"));
                                Log(TEXT("Source height is %d, expected %d"), vrs.lSourceHeight, pVih->bmiHeader.biHeight);
                            }

                            if(vrs.lSourceWidth != pVih->bmiHeader.biWidth)
                            {
                                FAIL(TEXT("Preview source width is not as expected"));
                                Log(TEXT("Source width is %d, expected %d"), vrs.lSourceWidth, pVih->bmiHeader.biWidth);
                            }
                        }

                        // this may result in some user interaction, verify the preview is running
                        if(FAILED(pCaptureGraph->VerifyPreviewWindow()))
                        {
                            FAIL(TEXT("The preview window wasn't correct."));
                        }
                    }

                    if(FAILED(hr = pCaptureGraph->StopGraph()))
                    {
                        FAIL(TEXT("Stopping the capture graph failed."));
                    }

                    if(ppAMT)
                    {
                        DeleteMediaType(ppAMT);
                        ppAMT = NULL;
                        pVih = NULL;
                    }
                }
            }

            pCaptureGraph->SetForceColorConversion(bOldForceColorConversion);
        }
    }

cleanup:

    if(ppAMT)
    {
        DeleteMediaType(ppAMT);
        ppAMT = NULL;
        pVih = NULL;
    }


    // cleanup the graph on error
    if(pCaptureGraph && FAILED(pCaptureGraph->Cleanup()))
    {
        FAIL(TEXT("Cleaning up the graph failed."));
    }

    if(hwnd)
    {
        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI VerifyStill( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    int anQualities[] = { QUALITY_LOW, QUALITY_MEDIUM, QUALITY_HIGH };
    int nQualityIndex;
#ifdef SAVE_CAPTURE_FILES
    TCHAR tszStillFileName[] = TEXT("\\release\\test%d.jpg");
#else
    TCHAR tszStillFileName[] = TEXT("\\test%d.jpg");
#endif
    HRESULT hr = S_OK;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                if(hr == E_GRAPH_UNSUPPORTED)
                {
                    DETAIL(TEXT("Initializing the capture graph failed as expected."));
                }
                else
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                }
            }
            else
            {
                if(FAILED(pCaptureGraph->RunGraph()))
                {
                    FAIL(TEXT("Starting the capture graph failed."));
                }
                else
                {
                    if(FAILED(pCaptureGraph->SetStillCaptureFileName(TEXT("\\StillVerifyImage.jpg"))))
                    {
                        FAIL(TEXT("Failed to set the still capture filename."));
                    }

                    // Our first test is to verify that the still image is captured and the contents is correct (if possible).
                    for(nQualityIndex = 0; nQualityIndex < countof(anQualities); nQualityIndex++)
                    {
                        Log(TEXT("Testing still image quality %d"), nQualityIndex);

                        if(FAILED(pCaptureGraph->SetStillImageQuality(anQualities[nQualityIndex])))
                        {
                            FAIL(TEXT("Unable to set still image quality"));
                        }

                        for(int i = 0; i < 2; i++)
                        {
                            Log(TEXT("Still image verification, iteration %d"), i);

                            // loop through a few times verifying the capture was correct.
                            if(FAILED(pCaptureGraph->VerifyStillImageCapture()))
                            {
                                FAIL(TEXT("The still image wasn't correct."));
                            }

                            // delay between verifications, enough for the screen to change
                            Sleep(500);
                        }
                    }

                    // now that we've completed the above test while reusing the file name, delete it.
                    if(FALSE == DeleteFile(TEXT("\\StillVerifyImage.jpg")))
                    {
                        FAIL(TEXT("Failed to delete the temporary still image file."));
                    }

                    // now we verify how fast we can capture still images and that they are immediatly deletable
                    if(FAILED(pCaptureGraph->SetStillCaptureFileName(tszStillFileName)))
                    {
                        FAIL(TEXT("Failed to set the still capture filename."));
                    }

                    for(nQualityIndex = 0; nQualityIndex < countof(anQualities); nQualityIndex++)
                    {
                        Log(TEXT("Testing still image quality %d"), nQualityIndex);

                        if(FAILED(pCaptureGraph->SetStillImageQuality(anQualities[nQualityIndex])))
                        {
                            FAIL(TEXT("Unable to set still image quality"));
                        }

                        for(int i = 0; i < 10; i++)
                        {
                            Log(TEXT("Still capture testing, iteration %d"), i);

                            TCHAR tszFileName[MAX_PATH];
                            int nStringLength = MAX_PATH;
                            HANDLE hFile = NULL;
                            WIN32_FIND_DATA fd;

                            // loop through a few times verifying the capture was correct.
                            if(FAILED(pCaptureGraph->CaptureStillImage()))
                            {
                                FAIL(TEXT("The still image capture failed."));
                            }

                            if(FAILED(pCaptureGraph->GetStillCaptureFileName(tszFileName, &nStringLength)))
                            {
                                FAIL(TEXT("Failed to retrieve the still capture file name."));
                            }

                            hFile = FindFirstFile(tszFileName, &fd);
                            if(hFile == INVALID_HANDLE_VALUE)
                            {
                                FAIL(TEXT("Failed to find the image file."));
                            }
                            else if(FALSE == FindClose(hFile))
                            {
                                FAIL(TEXT("Failed to close the image file handle."));
                            }

                            if(FALSE == DeleteFile(tszFileName))
                            {
                                FAIL(TEXT("Failed to delete the still image file."));
                            }
                        }
                    }
                }
            }
            if(FAILED(pCaptureGraph->Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
            }
        }

        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}


TESTPROCAPI VerifyStillRotationStatic( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    DWORD dwAngles[4] = {DMDO_0, DMDO_90, DMDO_180, DMDO_270 };
    int AngleIndex;
    DWORD dwBaseOrientation;
#ifdef SAVE_CAPTURE_FILES
    TCHAR tszStillFileName[] = TEXT("\\release\\test%d.jpg");
#else
    TCHAR tszStillFileName[] = TEXT("\\test%d.jpg");
#endif
    HRESULT hr = S_OK;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            // Our first test is to verify that the still image is captured and the contents are correct (if possible).
            for(AngleIndex = 0; AngleIndex < countof(dwAngles); AngleIndex++)
            {

                if(FAILED(pCaptureGraph->GetScreenOrientation(&dwBaseOrientation)))
                {
                    FAIL(TEXT("Failed to retrieve the base orientation."));
                }

                if(FAILED(pCaptureGraph->SetScreenOrientation(dwAngles[AngleIndex])))
                {
                    Log(TEXT("Failed to set angle %d, this is acceptable on devices which don't support rotation."), dwAngles[AngleIndex]);
                    continue;
                }

                // pump the messages to get the window updated
                pumpMessages();

                if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
                {
                    if(hr == E_GRAPH_UNSUPPORTED)
                    {
                        DETAIL(TEXT("Initializing the capture graph failed as expected."));
                    }
                    else
                    {
                        FAIL(TEXT("Initializing the capture graph failed."));
                    }
                }
                else
                {
                    if(FAILED(pCaptureGraph->RunGraph()))
                    {
                        FAIL(TEXT("Starting the capture graph failed."));
                    }
                    else
                    {
                        if(FAILED(pCaptureGraph->SetStillCaptureFileName(TEXT("\\StillVerifyImage.jpg"))))
                        {
                            FAIL(TEXT("Failed to set the still capture filename."));
                        }

                        for(int i = 0; i < 2; i++)
                        {
                            Log(TEXT("Still image verification, iteration %d"), i);

                            // loop through a few times verifying the capture was correct.
                            if(FAILED(pCaptureGraph->VerifyStillImageCapture()))
                            {
                                FAIL(TEXT("The still image wasn't correct."));
                            }

                            // delay between verifications, enough for the screen to change
                            Sleep(500);
                        }

                        // now that we've completed the above test while reusing the file name, delete it.
                        if(FALSE == DeleteFile(TEXT("\\StillVerifyImage.jpg")))
                        {
                            FAIL(TEXT("Failed to delete the temporary still image file."));
                        }

                        // now we verify how fast we can capture still images and that they are immediatly deletable
                        if(FAILED(pCaptureGraph->SetStillCaptureFileName(tszStillFileName)))
                        {
                            FAIL(TEXT("Failed to set the still capture filename."));
                        }

                        for(int i = 0; i < 10; i++)
                        {
                            Log(TEXT("Still capture testing, iteration %d"), i);

                            TCHAR tszFileName[MAX_PATH];
                            int nStringLength = MAX_PATH;
                            HANDLE hFile = NULL;
                            WIN32_FIND_DATA fd;

                            // loop through a few times verifying the capture was correct.
                            if(FAILED(pCaptureGraph->CaptureStillImage()))
                            {
                                FAIL(TEXT("The still image capture failed."));
                            }

                            if(FAILED(pCaptureGraph->GetStillCaptureFileName(tszFileName, &nStringLength)))
                            {
                                FAIL(TEXT("Failed to retrieve the still capture file name."));
                            }

                            hFile = FindFirstFile(tszFileName, &fd);
                            if(hFile == INVALID_HANDLE_VALUE)
                            {
                                FAIL(TEXT("Failed to find the image file."));
                            }
                            else if(FALSE == FindClose(hFile))
                            {
                                FAIL(TEXT("Failed to close the image file handle."));
                            }

                            if(FALSE == DeleteFile(tszFileName))
                            {
                                FAIL(TEXT("Failed to delete the still image file."));
                            }
                        }
                    }
                }
                if(FAILED(pCaptureGraph->Cleanup()))
                {
                    FAIL(TEXT("Cleaning up the graph failed."));
                }

                if(FAILED(pCaptureGraph->SetScreenOrientation(dwBaseOrientation)))
                {
                    // On some devices which don't support dynamic rotation, setting the orientation to the current orientation fails.
                    Log(TEXT("Failed to restore base orientation, this is acceptable on devices which don't support rotation."));
                }
            }
        }

        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI VerifyStillRotationDynamic( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    DWORD dwAngles[4] = {DMDO_0, DMDO_90, DMDO_180, DMDO_270 };
    int AngleIndex;
    DWORD dwBaseOrientation;
#ifdef SAVE_CAPTURE_FILES
    TCHAR tszStillFileName[] = TEXT("\\release\\test%d.jpg");
#else
    TCHAR tszStillFileName[] = TEXT("\\test%d.jpg");
#endif
    HRESULT hr = S_OK;
    BOOL bCapturedAStillImageToBeDeleted = FALSE;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            if(FAILED(pCaptureGraph->GetScreenOrientation(&dwBaseOrientation)))
            {
                FAIL(TEXT("Failed to retrieve the base orientation."));
            }

            if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                if(hr == E_GRAPH_UNSUPPORTED)
                {
                    DETAIL(TEXT("Initializing the capture graph failed as expected."));
                }
                else
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                }
            }
            else
            {
                if(FAILED(pCaptureGraph->RunGraph()))
                {
                    FAIL(TEXT("Starting the capture graph failed."));
                }
                else
                {
                    if(FAILED(pCaptureGraph->SetStillCaptureFileName(TEXT("\\StillVerifyImage.jpg"))))
                    {
                        FAIL(TEXT("Failed to set the still capture filename."));
                    }

                    bCapturedAStillImageToBeDeleted = FALSE;

                    // Our first test is to verify that the still image is captured and the contents is correct (if possible).
                    for(AngleIndex = 0; AngleIndex < countof(dwAngles); AngleIndex++)
                    {
                        for(int i = 0; i < 2; i++)
                        {
                            Log(TEXT("Still image verification, iteration %d"), i);

                            if(FAILED(pCaptureGraph->SetScreenOrientation(dwAngles[AngleIndex])))
                            {
                                Log(TEXT("Failed to set angle %d, this is acceptable on devices which don't support rotation."), dwAngles[AngleIndex]);
                                continue;
                            }

                            // pump the messages to get the window updated
                            pumpMessages();

                            // delay between verifications, enough for the screen to change
                            Sleep(500);

                            // loop through a few times verifying the capture was correct.
                            if(FAILED(pCaptureGraph->VerifyStillImageCapture()))
                            {
                                FAIL(TEXT("The still image wasn't correct."));
                            }

                            bCapturedAStillImageToBeDeleted = TRUE;
                        }
                    }

                    // now that we've completed the above test while reusing the file name, delete it.
                    if(bCapturedAStillImageToBeDeleted && FALSE == DeleteFile(TEXT("\\StillVerifyImage.jpg")))
                    {
                        FAIL(TEXT("Failed to delete the temporary still image file."));
                    }

                    // now we verify how fast we can capture still images and that they are immediatly deletable
                    if(FAILED(pCaptureGraph->SetStillCaptureFileName(tszStillFileName)))
                    {
                        FAIL(TEXT("Failed to set the still capture filename."));
                    }

                    // Our first test is to verify that the still image is captured and the contents is correct (if possible).
                    for(AngleIndex = 0; AngleIndex < countof(dwAngles); AngleIndex++)
                    {
                        for(int i = 0; i < 10; i++)
                        {
                            Log(TEXT("Still capture testing, iteration %d"), i);

                            TCHAR tszFileName[MAX_PATH];
                            int nStringLength = MAX_PATH;
                            HANDLE hFile = NULL;
                             WIN32_FIND_DATA fd;

                             if(FAILED(pCaptureGraph->SetScreenOrientation(dwAngles[AngleIndex])))
                            {
                                Log(TEXT("Failed to set angle %d, this is acceptable on devices which don't support rotation."), dwAngles[AngleIndex]);
                                continue;
                            }

                            // pump the messages to get the window updated
                            pumpMessages();

                            // loop through a few times verifying the capture was correct.
                            if(FAILED(pCaptureGraph->CaptureStillImage()))
                            {
                                FAIL(TEXT("The still image capture failed."));
                            }

                            if(FAILED(pCaptureGraph->GetStillCaptureFileName(tszFileName, &nStringLength)))
                            {
                               FAIL(TEXT("Failed to retrieve the still capture file name."));
                            }

                            hFile = FindFirstFile(tszFileName, &fd);
                            if(hFile == INVALID_HANDLE_VALUE)
                            {
                                FAIL(TEXT("Failed to find the image file."));
                            }
                            else if(FALSE == FindClose(hFile))
                            {
                                FAIL(TEXT("Failed to close the image file handle."));
                            }

                            if(FALSE == DeleteFile(tszFileName))
                            {
                                FAIL(TEXT("Failed to delete the still image file."));
                            }
                        }
                    }

                    if(FAILED(pCaptureGraph->SetScreenOrientation(dwBaseOrientation)))
                    {
                        // On some devices which don't support dynamic rotation, setting the orientation to the current orientation fails.
                        Log(TEXT("Failed to restore base orientation, this is acceptable on devices which don't support rotation."));
                    }
                }
            }
            if(FAILED(pCaptureGraph->Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
            }
        }

        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI VerifyStillFormats( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    AM_MEDIA_TYPE **ppAMT = NULL;
    int i, iSize, iCount;
    VIDEO_STREAM_CONFIG_CAPS *pvscc = NULL;
    HRESULT hr = S_OK;
    DWORD dwOrientation = 0;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                if(hr == E_GRAPH_UNSUPPORTED)
                {
                    DETAIL(TEXT("Initializing the capture graph failed as expected."));
                }
                else
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                }
            }
            else
            {
                if(FAILED(pCaptureGraph->GetScreenOrientation(&dwOrientation)))
                {
                    FAIL(TEXT("Failed to retrieve the base orientation."));
                }

                // if we can't retrieve the number of capabilities, then it's because there's no preview stream available.
                if(FAILED(hr = pCaptureGraph->GetNumberOfCapabilities(STREAM_STILL, &iCount, &iSize)))
                {
                    if(hr == VFW_E_NO_INTERFACE)
                        SKIP(TEXT("Failed to retrieve the number of capabilities, still stream not available."));
                    else FAIL(TEXT("Failed to retrieve the number of capabilieis for the still stream"));
                    goto cleanup;
                }

                if(iSize != sizeof(VIDEO_STREAM_CONFIG_CAPS))
                {
                    FAIL(TEXT("size of the configuration structure is incorrect, expected sizeof VIDEO_STREAM_CONFIG_CAPS."));
                }

                // entry 0 will be a NULL "default" entry, everything else will be retrieved from the graph.
                iCount ++;

                ppAMT = new AM_MEDIA_TYPE*[iCount];
                if(!ppAMT)
                {
                    FAIL(TEXT("Failed to allocate the media type array"));
                    goto cleanup;
                }
                // clear out the space just allocated
                memset(ppAMT, 0, sizeof(AM_MEDIA_TYPE *) * iCount);

                pvscc = new VIDEO_STREAM_CONFIG_CAPS[iCount];
                if(!pvscc)
                {
                    FAIL(TEXT("Failed to allocate the media type array"));
                    goto cleanup;
                }
                // clear out the space just allocated
                memset(pvscc, 0, sizeof(VIDEO_STREAM_CONFIG_CAPS) * iCount);


                // grab all of the formats available

                for(i = 1; i < iCount; i++)
                {
                    // because entry 0 is the "default" entry, the entry id is offset by 1
                    if(FAILED(pCaptureGraph->GetStreamCaps(STREAM_STILL, i - 1, &(ppAMT[i]), (BYTE *) &(pvscc[i]))))
                    {
                        FAIL(TEXT("Failed to retrieve the stream caps."));
                        goto cleanup;
                    }
                }

                // cleanup the graph so the new format can be applied to a newly built graph
                if(FAILED(pCaptureGraph->Cleanup()))
                {
                    FAIL(TEXT("Cleaning up the graph failed."));
                }

                for(i = 0; i < iCount; i++)
                {
                    VIDEOINFOHEADER *pVih = NULL;
                    CComPtr<IImagingFactory> pImagingFactory = NULL;
                    CComPtr<IImage> pJPEGImage = NULL;
                    ImageInfo ImageInfo;
                    BOOL bSrcPrerotated = false;
                    AM_MEDIA_TYPE *pamt = ppAMT[i];

                    Log(TEXT("Testing still format %d of %d"), i, iCount);

                    if(FAILED(pCaptureGraph->SetFormat(STREAM_STILL, ppAMT[i])))
                    {
                        FAIL(TEXT("Failed to set the format of the still."));
                    }

                    if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
                    {
                        if(hr == E_GRAPH_UNSUPPORTED)
                        {
                            DETAIL(TEXT("Initializing the capture graph failed as expected."));
                        }
                        else
                        {
                            FAIL(TEXT("Initializing the capture graph failed."));
                        }
                    }
                    else
                    {
                        // if this is the case where we have no format set, then 
                        // retrieve the current format. this has to be done after the graph is built,
                        // and the downstream filters negotiated everything.
                        if(!pamt)
                        {
                            if(FAILED(pCaptureGraph->GetFormat(STREAM_STILL, &pamt)))
                            {
                                FAIL(TEXT("Failed to set the format of the still."));
                            }
                        }

                        if(pamt)
                        {
                            if(pamt->formattype != FORMAT_VideoInfo)
                                FAIL(TEXT("Didn't receive a video info format type"));
                            else if(pamt->cbFormat < sizeof(VIDEOINFOHEADER))
                                FAIL(TEXT("Data structure size is less than a VIDEOINFOHEADER"));
                            else
                            {
                                pVih = (VIDEOINFOHEADER *) pamt->pbFormat;
                                bSrcPrerotated = (pVih->bmiHeader.biCompression & BI_SRCPREROTATE);
                                Log(TEXT("Testing a format of %dx%dx%d."), pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight, pVih->bmiHeader.biBitCount);
                            }
                        }
                        else
                        {
                            FAIL(TEXT("Unable to determine the format of the still pin."));
                        }

                         // run the graph
                        if(FAILED(pCaptureGraph->RunGraph()))
                        {
                            FAIL(TEXT("Starting the capture graph failed."));
                        }
                        else
                        {
                            Log(TEXT("Verifying setting an invalid format fails."));
                            if(SUCCEEDED(pCaptureGraph->SetFormat(STREAM_STILL, ppAMT[0])))
                            {
                                FAIL(TEXT("Succeeded resetting the format on the graph after it was built and running."));
                            }

                            // delay for a little bit for the graph to get running before we capture.
                            Sleep(500);

                            if(FAILED(pCaptureGraph->SetStillCaptureFileName(TEXT("\\StillVerifyImage.jpg"))))
                            {
                                FAIL(TEXT("Failed to set the still capture filename."));
                            }

                            // loop through a few times verifying the capture was correct.
                            if(FAILED(pCaptureGraph->VerifyStillImageCapture()))
                            {
                                FAIL(TEXT("The still image wasn't correct."));
                            }

                            // retrieve the still image info
                            if(FAILED(CoCreateInstance( CLSID_ImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IImagingFactory, (void**) &pImagingFactory )))
                            {
                                FAIL(TEXT("CCaptureFramework: Unable to retrieve the Imaging factory."));
                            }

                            if(pImagingFactory && FAILED(pImagingFactory->CreateImageFromFile(TEXT("\\StillVerifyImage.jpg"), &pJPEGImage)))
                            {
                                FAIL(TEXT("CCaptureFramework: Unable to load the image file."));
                            }

                            if(pJPEGImage && FAILED(pJPEGImage->GetImageInfo(&ImageInfo)))
                            {
                                FAIL(TEXT("CCaptureFramework: Unable to retrieve the image information."));
                            }

                            // if we're rotated 90 or 270, then the width and height are swapped as long as the driver is setting the BI_SRCPREROTATE flag
                            if(bSrcPrerotated && (dwOrientation == DMDO_90 || dwOrientation == DMDO_270))
                            {
                                if(pVih && ImageInfo.Width != pVih->bmiHeader.biHeight)
                                {
                                    FAIL(TEXT("Width of file captured is not the width expected."));
                                }

                                if(pVih && ImageInfo.Height != abs(pVih->bmiHeader.biWidth))
                                {
                                    FAIL(TEXT("Height of file captured is not the height expected."));
                                }
                            }
                            // either we're not following the src prerotate mechanism, or
                            // we're at 0 or 180, which means the width and height aren't reversed.
                            else
                            {
                                if(pVih && ImageInfo.Width != pVih->bmiHeader.biWidth)
                                {
                                    FAIL(TEXT("Width of file captured is not the width expected."));
                                }

                                if(pVih && ImageInfo.Height != abs(pVih->bmiHeader.biHeight))
                                {
                                    FAIL(TEXT("Height of file captured is not the height expected."));
                                }
                            }

                            pJPEGImage = NULL;
                            pImagingFactory = NULL;

                            // now that we've completed the above test while reusing the file name, delete it.
                            if(FALSE == DeleteFile(TEXT("\\StillVerifyImage.jpg")))
                            {
                                FAIL(TEXT("Failed to delete the temporary still image file."));
                            }
                        }

                        if(pamt != ppAMT[i])
                        {
                            DeleteMediaType(pamt);
                            pamt = NULL;
                        }
                    }
                    // cleanup the graph so the next format can be applied to a newly built graph
                    if(FAILED(pCaptureGraph->Cleanup()))
                    {
                        FAIL(TEXT("Cleaning up the graph failed."));
                    }
                 }
            }
        }
    }

cleanup:

    // cleanup the graph on error
    if(pCaptureGraph && FAILED(pCaptureGraph->Cleanup()))
    {
        FAIL(TEXT("Cleaning up the graph failed."));
    }

    if(ppAMT)
    {
        for(i = 0; i < iCount; i++)
        {
            if(ppAMT[i])
            {
                DeleteMediaType(ppAMT[i]);
                ppAMT[i] = NULL;
            }
        }
        delete[] ppAMT;
        ppAMT = NULL;
    }

    if(pvscc)
    {
        delete[] pvscc;
        pvscc = NULL;
    }

    if(hwnd)
    {
        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI VerifyStillFormatsPostConnect( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    AM_MEDIA_TYPE *ppAMT = NULL;
    AM_MEDIA_TYPE *pamt = NULL;
    int i, iSize, iCount;
    VIDEO_STREAM_CONFIG_CAPS pvscc;
    HRESULT hr = S_OK;
    DWORD dwOrientation = 0;
    BOOL bOldForceColorConversion = FALSE;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            bOldForceColorConversion = pCaptureGraph->GetForceColorConversion();
            pCaptureGraph->SetForceColorConversion(FALSE);

            if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                SKIP(TEXT("Initializing the capture graph failed, this is OK because the color converter was forced out of the graph."));
            }
            else
            {
                CComPtr<IGraphBuilder> pFilterGraph = NULL;
                CComPtr<IBaseFilter> pColorConverter = NULL;

                if (FAILED(pCaptureGraph->GetBuilderInterface((void**)&pFilterGraph, IID_IGraphBuilder)))
                {
                    FAIL(TEXT("Failed to retrieve the IGraphBuilder interface from the capture framework.."));
                    goto cleanup;
                }

                hr = pFilterGraph->FindFilterByName(TEXT("Color Converter"), &pColorConverter);

                // now that we're done with these, free them
                pFilterGraph = NULL;
                pColorConverter = NULL;

                // if we found the color converter above, then we need to continue on because it
                // doesn't support reconnects with different formats.
                if (SUCCEEDED(hr))
                {
                    SKIP(TEXT("The color conversion filter is in the graph, skipping because it does not support post connect operations."));
                    goto cleanup;
                }

                if(FAILED(pCaptureGraph->GetScreenOrientation(&dwOrientation)))
                {
                    FAIL(TEXT("Failed to retrieve the base orientation."));
                }

                // if we can't retrieve the number of capabilities, then it's because there's no preview stream available.
                if(FAILED(hr = pCaptureGraph->GetNumberOfCapabilities(STREAM_STILL, &iCount, &iSize)))
                {
                    if(hr == VFW_E_NO_INTERFACE)
                        SKIP(TEXT("Failed to retrieve the number of capabilities, still stream not available."));
                    else FAIL(TEXT("Failed to retrieve the number of capabilieis for the still stream"));
                    goto cleanup;
                }

                if(iSize != sizeof(VIDEO_STREAM_CONFIG_CAPS))
                {
                    FAIL(TEXT("size of the configuration structure is incorrect, expected sizeof VIDEO_STREAM_CONFIG_CAPS."));
                }

                for(i = 0; i < iCount; i++)
                {
                    VIDEOINFOHEADER *pVih = NULL;
                    CComPtr<IImagingFactory> pImagingFactory = NULL;
                    CComPtr<IImage> pJPEGImage = NULL;
                    ImageInfo ImageInfo;
                    BOOL bSrcPrerotated = false;

                    Log(TEXT("Testing still format %d of %d"), i + 1, iCount);

                    if(FAILED(pCaptureGraph->GetStreamCaps(STREAM_STILL, i, &ppAMT, (BYTE *) &pvscc)))
                    {
                        FAIL(TEXT("Failed to retrieve the stream caps."));
                        goto cleanup;
                    }

                    if(FAILED(pCaptureGraph->SetFormat(STREAM_STILL, ppAMT)))
                    {
                        FAIL(TEXT("Failed to set the format of the still."));
                    }

                    if(FAILED(pCaptureGraph->GetFormat(STREAM_STILL, &pamt)))
                    {
                        FAIL(TEXT("Failed to set the format of the still."));
                    }
 
                    if(pamt)
                    {
                        if(pamt->formattype != FORMAT_VideoInfo)
                            FAIL(TEXT("Didn't receive a video info format type"));
                        else if(pamt->cbFormat < sizeof(VIDEOINFOHEADER))
                            FAIL(TEXT("Data structure size is less than a VIDEOINFOHEADER"));
                        else
                        {
                            pVih = (VIDEOINFOHEADER *) pamt->pbFormat;
                            bSrcPrerotated = (pVih->bmiHeader.biCompression & BI_SRCPREROTATE);
                            Log(TEXT("Testing a format of %dx%dx%d."), pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight, pVih->bmiHeader.biBitCount);
                        }
                    }
                    else
                    {
                        FAIL(TEXT("Unable to determine the format of the still pin."));
                    }

                     // run the graph
                    if(FAILED(pCaptureGraph->RunGraph()))
                    {
                        FAIL(TEXT("Starting the capture graph failed."));
                    }
                    else
                    {
                        // delay for a little bit for the graph to get running before we capture.
                        Sleep(500);

                        if(FAILED(pCaptureGraph->SetStillCaptureFileName(TEXT("\\StillVerifyImage.jpg"))))
                        {
                            FAIL(TEXT("Failed to set the still capture filename."));
                        }

                        // loop through a few times verifying the capture was correct.
                        if(FAILED(pCaptureGraph->VerifyStillImageCapture()))
                        {
                            FAIL(TEXT("The still image wasn't correct."));
                        }

                        // retrieve the still image info
                        if(FAILED(CoCreateInstance( CLSID_ImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IImagingFactory, (void**) &pImagingFactory )))
                        {
                            FAIL(TEXT("CCaptureFramework: Unable to retrieve the Imaging factory."));
                        }

                        if(pImagingFactory && FAILED(pImagingFactory->CreateImageFromFile(TEXT("\\StillVerifyImage.jpg"), &pJPEGImage)))
                        {
                            FAIL(TEXT("CCaptureFramework: Unable to load the image file."));
                        }

                        if(pJPEGImage && FAILED(pJPEGImage->GetImageInfo(&ImageInfo)))
                        {
                            FAIL(TEXT("CCaptureFramework: Unable to retrieve the image information."));
                        }

                        // if we're rotated 90 or 270, then the width and height are swapped as long as the driver is setting the BI_SRCPREROTATE flag
                        if(bSrcPrerotated && (dwOrientation == DMDO_90 || dwOrientation == DMDO_270))
                        {
                            if(pVih && ImageInfo.Width != pVih->bmiHeader.biHeight)
                            {
                                FAIL(TEXT("Width of file captured is not the width expected."));
                            }

                            if(pVih && ImageInfo.Height != abs(pVih->bmiHeader.biWidth))
                            {
                                FAIL(TEXT("Height of file captured is not the height expected."));
                            }
                        }
                        // either we're not following the src prerotate mechanism, or
                        // we're at 0 or 180, which means the width and height aren't reversed.
                        else
                        {
                            if(pVih && ImageInfo.Width != pVih->bmiHeader.biWidth)
                            {
                                FAIL(TEXT("Width of file captured is not the width expected."));
                            }

                            if(pVih && ImageInfo.Height != abs(pVih->bmiHeader.biHeight))
                            {
                                FAIL(TEXT("Height of file captured is not the height expected."));
                            }
                        }

                        pJPEGImage = NULL;
                        pImagingFactory = NULL;

                        // now that we've completed the above test while reusing the file name, delete it.
                        if(FALSE == DeleteFile(TEXT("\\StillVerifyImage.jpg")))
                        {
                            FAIL(TEXT("Failed to delete the temporary still image file."));
                        }
                    }

                    if(FAILED(pCaptureGraph->StopGraph()))
                    {
                        FAIL(TEXT("Stopping the capture graph failed."));
                    }

                    if(pamt)
                    {
                        DeleteMediaType(pamt);
                        pamt = NULL;
                    }

                    if(ppAMT)
                    {
                        DeleteMediaType(ppAMT);
                        ppAMT = NULL;
                    }
                 }
            }

            pCaptureGraph->SetForceColorConversion(bOldForceColorConversion);
        }
    }

cleanup:

    if(pamt)
    {
        DeleteMediaType(pamt);
        pamt = NULL;
    }

    if(ppAMT)
    {
        DeleteMediaType(ppAMT);
        ppAMT = NULL;
    }

    // cleanup the graph on error
    if(pCaptureGraph && FAILED(pCaptureGraph->Cleanup()))
    {
        FAIL(TEXT("Cleaning up the graph failed."));
    }

    if(hwnd)
    {
        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}



#define BURSTCOUNT 30
struct StillBurstData
{
    BOOL breceived;
    DWORD dwStartTime;
    DWORD dwEndTime;
    LONG lParam1;
    LONG lParam2;
};

TESTPROCAPI VerifyStillBurst( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
#ifdef SAVE_CAPTURE_FILES
    TCHAR tszStillFileName[] = TEXT("\\release\\StillBurstTest-%d.jpg");
    TCHAR tszStillFileName2[] = TEXT("\\release\\StillBurstTest2-%d.jpg");
#else
    TCHAR tszStillFileName[] = TEXT("\\StillBurstTest-%d.jpg");
    TCHAR tszStillFileName2[] = TEXT("\\StillBurstTest2-%d.jpg");
#endif
    HRESULT hr = S_OK;

    int nBurstIndex = 0;
    struct StillBurstData SBD[BURSTCOUNT];
    DWORD dwBurstStartTime = 0;


    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);


        // verify everything works after graph teardown and rebuild
        for(int TearDownIteration =0; TearDownIteration < 2; TearDownIteration++)
        {
            if(pCaptureGraph)
            {
                if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
                {
                    if(hr == E_GRAPH_UNSUPPORTED)
                    {
                        DETAIL(TEXT("Initializing the capture graph failed as expected."));
                    }
                    else
                    {
                        FAIL(TEXT("Initializing the capture graph failed."));
                    }
                }
                else
                {
                    if(FAILED(pCaptureGraph->RunGraph()))
                    {
                        FAIL(TEXT("Starting the capture graph failed."));
                    }
                    else
                    {

                        // run the test, change the name, run the test with a new name,
                        // to verifty that everything is reset on a file name change.
                        for(int iteration = 0; iteration < 2; iteration++)
                        {

                            // reset the burst structure for the new test run
                            memset(&SBD, 0, sizeof(struct StillBurstData) * BURSTCOUNT);

                            if(FAILED(pCaptureGraph->SetStillCaptureFileName(iteration == 0?tszStillFileName:tszStillFileName2)))
                            {
                                FAIL(TEXT("Failed to set the still capture filename."));
                            }

                            dwBurstStartTime = GetTickCount();
                            for(nBurstIndex = 0; nBurstIndex < BURSTCOUNT; nBurstIndex++)
                            {
                                if(FAILED(pCaptureGraph->TriggerStillImage()))
                                {
                                    FAIL(TEXT("Failed to trigger a still image"));
                                }
                            }

                            for(nBurstIndex = 0; nBurstIndex < BURSTCOUNT;)
                            {
                                LONG lEvent, lParam1, lParam2;
                                // wait for up to 10 seconds per picture, which should be plenty of time.
                                long lEventTimeout = 10000;
                                HRESULT hrEvent;

                                // this is a pseudo GetEvent call, it returns the next event on the EventSink's 
                                // vector of events processed, if it's at the end then it waits for the next even.
                                // That means there's no need to free the event, and also never use the event params
                                // returned beyond this subtest within the test case. New video and still captures
                                // purge the event sink.
                                hrEvent = pCaptureGraph->GetEvent(&lEvent, &lParam1, &lParam2, lEventTimeout);

                                if(SUCCEEDED(hrEvent))
                                {
                                    if(lEvent == EC_CAP_FILE_COMPLETED)
                                    {
                                        SBD[nBurstIndex].dwEndTime = GetTickCount();
                                        SBD[nBurstIndex].dwStartTime = dwBurstStartTime;
                                        SBD[nBurstIndex].lParam1 = lParam1;
                                        SBD[nBurstIndex].lParam2 = lParam2;
                                        SBD[nBurstIndex].breceived = TRUE;

                                        // now that we successfully received the image, increment the burst count
                                        nBurstIndex++;
                                    }
                                }
                                else
                                {
                                    FAIL(TEXT("CaptureFramework: Failed to receive the requested number of events."));
                                    break;
                                }
                            }

                            for(nBurstIndex = 0; nBurstIndex < BURSTCOUNT; nBurstIndex++)
                            {
                                TCHAR tszTempName[MAX_PATH] = {0};

                                if(!SBD[nBurstIndex].breceived)
                                    continue;

                                _sntprintf(tszTempName, countof(tszTempName), iteration == 0?tszStillFileName:tszStillFileName2, nBurstIndex);
                                tszTempName[MAX_PATH - 1] = NULL;

                                // first, make sure the file name returned is the same as expected.
                                if(_tcscmp(tszTempName, (TCHAR *) SBD[nBurstIndex].lParam2))
                                {
                                    FAIL(TEXT("File name of image captured doesn't match the file name expected"));
                                    Log(TEXT("File name returned is %s, file name expected is %s"), SBD[nBurstIndex].lParam2, tszTempName);
                                }

                                // now verify the image contents.
                                if(FAILED(pCaptureGraph->VerifyStillImageLocation((TCHAR *) SBD[nBurstIndex].lParam2, SBD[nBurstIndex].dwStartTime, SBD[nBurstIndex].dwEndTime)))
                                {
                                    FAIL(TEXT("StillImage content from burst is incorrect"));
                                    Log(TEXT("Index %d, file name %s"), nBurstIndex, SBD[nBurstIndex].lParam2);
                                }

#ifndef SAVE_CAPTURE_FILES
                                if(FALSE == DeleteFile((TCHAR *) SBD[nBurstIndex].lParam2))
                                {
                                    FAIL(TEXT("Failed to delete the video file."));
                                }
#endif
                            }
                        }
                    }
                }
                if(FAILED(pCaptureGraph->Cleanup()))
                {
                    FAIL(TEXT("Cleaning up the graph failed."));
                }
            }
        }

        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI VerifyASFWriting( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    int nCaptureLength;

    int anTestCombinations[][3] = {
                                                        {75, 10000, 2}, // defaults
                                                        {0, 10000, 2},
                                                        {50, 10000, 2},
                                                        {100, 10000, 2},
                                                        {75, 0, 2},
                                                        {75, 5000, 2},
                                                        {75, 15000, 2},
                                                        {75, 10000, 0},
                                                        {75, 10000, 1},
                                                        {75, 10000, 2},
                                                    };

    // diagonalize the graph some.  If we're running with all of the components, then
    // test all of the encoding parameters, otherwise just test with the default
    int nCombinationCount = lpFTE->dwUserData == ALL_COMPONENTS?10:1;
    int nCurrentCombination;
    TCHAR tszFileName[MAX_PATH];
    int nStringLength = MAX_PATH;
    TCHAR tszBaseFileName[] = TEXT("%s\\StartStop%d");
    TCHAR tszCompletor[] = TEXT("File%d.asf");
#ifdef SAVE_CAPTURE_FILES
    TCHAR tszExtraPath[] = TEXT("\\release");
#else
    TCHAR tszExtraPath [] = TEXT("\0");
#endif
    HRESULT hr = S_OK;
    VARIANT Var;
    int nStartLength = 0;
    int nTotalCaptureLength = 10000;
    int nShortLength = 4000;
    int nShortIncrement = 1000;
    int nMainIncrement = 3000;

    VariantInit(&Var);

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {

            for(nCurrentCombination = 0; nCurrentCombination < nCombinationCount; nCurrentCombination++)
            {
                Log(TEXT("Testing encoder quality combination %d"), nCurrentCombination);

                if(FAILED(pCaptureGraph->InitCore(hwnd, &rc, dwGraphLayout)))
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                }
                else
                {
                    // If retrieving a property succeeds, then setting it should succeed also.
                    // if we can't retrieve it then we ignore it.
                    if(SUCCEEDED(hr = pCaptureGraph->GetVideoEncoderInfo(g_wszWMVCVBRQuality, &Var)))
                    {
                        VariantClear(&Var);
                        Var.vt = VT_I4;
                        Var.lVal = (LONG) anTestCombinations[nCurrentCombination][0];

                        hr = pCaptureGraph->SetVideoEncoderProperty(g_wszWMVCVBRQuality, &Var);
                        if(FAILED(hr))
                        {
                            FAIL(TEXT("Failed to set encoder quality property."));
                        }
                    }

                    if(SUCCEEDED(hr = pCaptureGraph->GetVideoEncoderInfo(g_wszWMVCKeyframeDistance, &Var)))
                    {
                        VariantClear(&Var);
                        Var.vt = VT_I4;
                        Var.lVal = (LONG) anTestCombinations[nCurrentCombination][1];
                        hr = pCaptureGraph->SetVideoEncoderProperty(g_wszWMVCKeyframeDistance, &Var);
                        if(FAILED(hr))
                        {
                            FAIL(TEXT("Failed to set encoder key frame property."));
                        }
                    }

                    if(SUCCEEDED(hr = pCaptureGraph->GetVideoEncoderInfo(g_wszWMVCComplexityEx, &Var)))
                    {
                        VariantClear(&Var);
                        Var.vt = VT_I4;
                        Var.lVal = (LONG) anTestCombinations[nCurrentCombination][2];
                        hr = pCaptureGraph->SetVideoEncoderProperty(g_wszWMVCComplexityEx, &Var);
                        if(FAILED(hr))
                        {
                            FAIL(TEXT("Failed to set encoder complexity property."));
                        }
                    }

                    if(FAILED(hr = pCaptureGraph->FinalizeGraph()))
                    {
                        if(hr == E_GRAPH_UNSUPPORTED)
                        {
                            DETAIL(TEXT("Initializing the capture graph failed as expected."));
                        }
                        else
                        {
                            FAIL(TEXT("Initializing the capture graph failed."));
                        }
                    }
                    else
                    {
                        for(nCaptureLength = nStartLength; nCaptureLength < nTotalCaptureLength; nCaptureLength<nShortLength?nCaptureLength+=nShortIncrement:nCaptureLength += nMainIncrement)
                        {
                            _stprintf(tszFileName, tszBaseFileName, tszExtraPath, nCurrentCombination);
                            _tcscat(tszFileName, tszCompletor);

                            if(FAILED(pCaptureGraph->SetVideoCaptureFileName(tszFileName)))
                            {
                                FAIL(TEXT("Failed to set the capture filename."));
                            }

                            if(FAILED(pCaptureGraph->RunGraph()))
                            {
                                FAIL(TEXT("Starting the capture graph failed."));
                            }
                            else
                            {

                                HANDLE hFile = NULL;
                                WIN32_FIND_DATA fd;

                                Log(TEXT("Capture length %d seconds"), nCaptureLength/1000);

                                Log(TEXT("  Capture starting"));

                                // loop through a few times verifying the capture was correct.
                                if(FAILED(hr = pCaptureGraph->StartStreamCapture())  && hr != E_WRITE_ERROR_EVENT && hr != E_OOM_EVENT)
                                {
                                    if(hr != E_NO_FRAMES_PROCESSED)
                                        FAIL(TEXT("Failed to start the stream capture."));
                                    else Log(TEXT("Insufficient memory to process even one frame of the capture."));
                                }
                                else
                                {
                                    Log(TEXT("  Capture running"));

                                    Sleep(nCaptureLength);

                                    Log(TEXT("  Capture complete"));

                                    // make sure we properly handle the case where we ran out of memory while doing the test.
                                    hr = pCaptureGraph->StopStreamCapture();
                                    if(FAILED(hr))
                                    {
                                        if(hr == E_WRITE_ERROR_EVENT)
                                            Log(TEXT("Received a write error event while doing the capture (out of storage space?)."));
                                        else if(hr == E_OOM_EVENT)
                                            Log(TEXT("Received an OOM event while doing the capture (insufficient memory for this capture length?)."));
                                        else
                                            FAIL(TEXT("Failed to stop the stream capture."));
                                    }

                                    Log(TEXT("  Capture stream stopped"));

                                    if(FAILED(pCaptureGraph->StopGraph()))
                                    {
                                        FAIL(TEXT("Failed to stop the capture graph after the capture."));
                                    }

                                    // reset the string length for retrieval
                                    nStringLength = MAX_PATH;
                                    // retrieve the file name after the capture was started, so it's the file name of the captured file
                                    if(FAILED(pCaptureGraph->GetVideoCaptureFileName(tszFileName, &nStringLength)))
                                    {
                                        FAIL(TEXT("Failed to retrieve the video capture file name."));
                                    }

                                    if(nCaptureLength > 0 && FAILED(pCaptureGraph->VerifyVideoFileCaptured(tszFileName)))
                                    {
                                        FAIL(TEXT("The video file captured failed file conformance verification"));
                                    }

                                    // verify that the file was created
                                    hFile = FindFirstFile(tszFileName, &fd);
                                    if(hFile == INVALID_HANDLE_VALUE)
                                    {
                                        if(nCaptureLength > 0)
                                            FAIL(TEXT("Failed to find the video file."));

                                        Log(TEXT("Looking for file %s, not found."), tszFileName);
                                    }
                                    else if(FALSE == FindClose(hFile))
                                    {
                                        FAIL(TEXT("Failed to close the video file handle."));
                                    }

#ifndef SAVE_CAPTURE_FILES
                                    if(FALSE == DeleteFile(tszFileName))
                                    {
                                        if(nCaptureLength > 0)
                                            FAIL(TEXT("Failed to delete the video file."));
                                    }
#endif
                                }
                            }
                        }
                    }
                }
                if(FAILED(pCaptureGraph->Cleanup()))
                {
                    FAIL(TEXT("Cleaning up the graph failed."));
                }
            }
        }

        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI VerifyASFStaticRotation( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    // do the capture for 10 seconds, long enough to get a good capture,
    // short enough that the test doesn't take forever.
    int nCaptureLength = 10000;

    TCHAR tszFileName[MAX_PATH];
    int nStringLength = MAX_PATH;
    TCHAR tszBaseFileName[] = TEXT("%s\\ASFRotation%d");
    TCHAR tszCompletor[] = TEXT("File%d.asf");
#ifdef SAVE_CAPTURE_FILES
    TCHAR tszExtraPath[] = TEXT("\\release");
#else
    TCHAR tszExtraPath [] = TEXT("\0");
#endif
    DWORD dwAngles[4] = {DMDO_0, DMDO_90, DMDO_180, DMDO_270 };
    int AngleIndex;
    DWORD dwBaseOrientation;
    HRESULT hr = S_OK;

    hwnd = GetWindow();

    if(hwnd)
    {

        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {

            for(AngleIndex = 0; AngleIndex < countof(dwAngles); AngleIndex++)
            {
                Log(TEXT("Testing orientation %d"), AngleIndex);
                
                if(FAILED(pCaptureGraph->GetScreenOrientation(&dwBaseOrientation)))
                {
                    FAIL(TEXT("Failed to retrieve the base orientation."));
                }

                if(FAILED(pCaptureGraph->SetScreenOrientation(dwAngles[AngleIndex])))
                {
                    Log(TEXT("Failed to set angle %d, this is acceptable on devices which don't support rotation."), dwAngles[AngleIndex]);
                    continue;
                }

                // pump the messages to get the window updated
                pumpMessages();

                if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
                {
                    if(hr == E_GRAPH_UNSUPPORTED)
                    {
                        DETAIL(TEXT("Initializing the capture graph failed as expected."));
                    }
                    else
                    {
                        FAIL(TEXT("Initializing the capture graph failed."));
                    }
                }
                else
                {

                    _stprintf(tszFileName, tszBaseFileName, tszExtraPath, AngleIndex);
                    _tcscat(tszFileName, tszCompletor);

                    if(FAILED(pCaptureGraph->SetVideoCaptureFileName(tszFileName)))
                    {
                        FAIL(TEXT("Failed to set the capture filename."));
                    }

                    HANDLE hFile = NULL;
                    WIN32_FIND_DATA fd;

                    Log(TEXT("Capture length %d seconds"), nCaptureLength/1000);

                    Log(TEXT("  Capture starting"));

                    if(FAILED(pCaptureGraph->RunGraph()))
                    {
                        FAIL(TEXT("Starting the capture graph failed."));
                    }
                    else
                    {
                        // loop through a few times verifying the capture was correct.
                        if(FAILED(hr = pCaptureGraph->StartStreamCapture())  && hr != E_WRITE_ERROR_EVENT && hr != E_OOM_EVENT)
                        {
                            if(hr != E_NO_FRAMES_PROCESSED)
                                FAIL(TEXT("Failed to start the stream capture."));
                            else Log(TEXT("Insufficient memory to process even one frame of the capture."));
                        }
                        else
                        {
                            Log(TEXT("  Capture running"));

                            Sleep(nCaptureLength);

                            Log(TEXT("  Capture complete"));

                            // make sure we properly handle the case where we ran out of memory while doing the test.
                            hr = pCaptureGraph->StopStreamCapture();
                            if(FAILED(hr))
                            {
                                if(hr == E_WRITE_ERROR_EVENT)
                                    Log(TEXT("Received a write error event while doing the capture (out of storage space?)."));
                                else if(hr == E_OOM_EVENT)
                                    Log(TEXT("Received an OOM event while doing the capture (insufficient memory for this capture length?)."));
                                else
                                    FAIL(TEXT("Failed to stop the stream capture."));
                            }

                            Log(TEXT("  Capture stream stopped"));

                            if(FAILED(pCaptureGraph->StopGraph()))
                            {
                                FAIL(TEXT("Failed to stop the graph after the capture"));
                            }

                            // reset the string length for retrieval
                            nStringLength = MAX_PATH;
                            // retrieve the file name after the capture was started, so it's the file name of the captured file
                            if(FAILED(pCaptureGraph->GetVideoCaptureFileName(tszFileName, &nStringLength)))
                            {
                                FAIL(TEXT("Failed to retrieve the video capture file name."));
                            }

                            if(nCaptureLength > 0 && FAILED(pCaptureGraph->VerifyVideoFileCaptured(tszFileName)))
                            {
                                FAIL(TEXT("The video file captured failed file conformance verification"));
                            }

                            // verify that the file was created
                            hFile = FindFirstFile(tszFileName, &fd);
                            if(hFile == INVALID_HANDLE_VALUE)
                            {
                                if(nCaptureLength > 0)
                                    FAIL(TEXT("Failed to find the video file."));

                                Log(TEXT("Looking for file %s, not found."), tszFileName);
                            }
                            else if(FALSE == FindClose(hFile))
                            {
                                FAIL(TEXT("Failed to close the video file handle."));
                            }

#ifndef SAVE_CAPTURE_FILES
                            if(FALSE == DeleteFile(tszFileName))
                            {
                                if(nCaptureLength > 0)
                                    FAIL(TEXT("Failed to delete the video file."));
                            }
#endif
                        }

                        if(FAILED(pCaptureGraph->SetScreenOrientation(dwBaseOrientation)))
                        {
                            // On some devices which don't support dynamic rotation, setting the orientation to the current orientation fails.
                            Log(TEXT("Failed to restore base orientation, this is acceptable on devices which don't support rotation."));
                        }
                    }
                }
                if(FAILED(pCaptureGraph->Cleanup()))
                {
                    FAIL(TEXT("Cleaning up the graph failed."));
                }
            }
        }

        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI VerifyASFDynamicRotation( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    // do the capture for 20 seconds, 5 seconds at each orientation
    int nCaptureLength = 20000;
    DWORD dwAngles[4] = {DMDO_0, DMDO_90, DMDO_180, DMDO_270 };
    int AngleIndex;
    DWORD dwBaseOrientation;

#ifndef SAVE_CAPTURE_FILES
    TCHAR tszFileName[] = TEXT("\\ASFDynamicRotation.asf");
#else
    TCHAR tszFileName [] = TEXT("\\release\\ASFDynamicRotation.asf");
#endif
    HRESULT hr = S_OK;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                if(hr == E_GRAPH_UNSUPPORTED)
                {
                    DETAIL(TEXT("Initializing the capture graph failed as expected."));
                }
                else
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                }
            }
            else
            {
                if(FAILED(pCaptureGraph->GetScreenOrientation(&dwBaseOrientation)))
                {
                    FAIL(TEXT("Failed to retrieve the base orientation."));
                }

                if(FAILED(pCaptureGraph->SetVideoCaptureFileName(tszFileName)))
                {
                    FAIL(TEXT("Failed to set the capture filename."));
                }

                if(FAILED(pCaptureGraph->RunGraph()))
                {
                    FAIL(TEXT("Starting the capture graph failed."));
                }
                else
                {
                    Log(TEXT("Capture length %d seconds"), nCaptureLength/1000);

                    Log(TEXT("  Capture starting"));

                    // loop through a few times verifying the capture was correct.
                    if(FAILED(hr = pCaptureGraph->StartStreamCapture())  && hr != E_WRITE_ERROR_EVENT && hr != E_OOM_EVENT)
                    {
                        if(hr != E_NO_FRAMES_PROCESSED)
                            FAIL(TEXT("Failed to start the stream capture."));
                        else Log(TEXT("Insufficient memory to process even one frame of the capture."));
                    }
                    else
                    {
                        Log(TEXT("  Capture running"));

                        for(AngleIndex = 0; AngleIndex < countof(dwAngles); AngleIndex++)
                        {

                            if(FAILED(pCaptureGraph->SetScreenOrientation(dwAngles[AngleIndex])))
                            {
                                Log(TEXT("Failed to set angle %d, this is acceptable on devices which don't support rotation."), dwAngles[AngleIndex]);
                            }

                            // pump the messages to get the window updated
                            pumpMessages();

                            Sleep(nCaptureLength/4);
                        }
                        Log(TEXT("  Capture complete"));

                        // make sure we properly handle the case where we ran out of memory while doing the test.
                        hr = pCaptureGraph->StopStreamCapture();
                        if(FAILED(hr))
                        {
                            if(hr == E_WRITE_ERROR_EVENT)
                                Log(TEXT("Received a write error event while doing the capture (out of storage space?)."));
                            else if(hr == E_OOM_EVENT)
                                Log(TEXT("Received an OOM event while doing the capture (insufficient memory for this capture length?)."));
                            else
                                FAIL(TEXT("Failed to stop the stream capture."));
                        }

                        Log(TEXT("  Capture stream stopped"));

                        if(FAILED(pCaptureGraph->StopGraph()))
                        {
                            FAIL(TEXT("Failed to stop the graph"));
                        }

                        if(nCaptureLength > 0 && FAILED(pCaptureGraph->VerifyVideoFileCaptured(tszFileName)))
                        {
                            FAIL(TEXT("The video file captured failed file conformance verification"));
                        }

                        HANDLE hFile = NULL;
                        WIN32_FIND_DATA fd;

                        // verify that the file was created
                        hFile = FindFirstFile(tszFileName, &fd);
                        if(hFile == INVALID_HANDLE_VALUE)
                        {
                            if(nCaptureLength > 0)
                                FAIL(TEXT("Failed to find the video file."));

                            Log(TEXT("Looking for file %s, not found."), tszFileName);
                        }
                        else if(FALSE == FindClose(hFile))
                        {
                            FAIL(TEXT("Failed to close the video file handle."));
                        }

#ifndef SAVE_CAPTURE_FILES
                        if(FALSE == DeleteFile(tszFileName))
                        {
                            if(nCaptureLength > 0)
                                FAIL(TEXT("Failed to delete the video file."));
                        }
#endif
                    }

                    if(FAILED(pCaptureGraph->SetScreenOrientation(dwBaseOrientation)))
                    {
                        // On some devices which don't support dynamic rotation, setting the orientation to the current orientation fails.
                        Log(TEXT("Failed to restore base orientation, this is acceptable on devices which don't support rotation."));
                    }
                }
            }
            if(FAILED(pCaptureGraph->Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
            }
        }

        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI VerifyASFWritingFormats( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    AM_MEDIA_TYPE **ppAMT = NULL;
    int i, iSize, iCount;
    VIDEO_STREAM_CONFIG_CAPS *pvscc = NULL;
    TCHAR tszFileName[MAX_PATH];
    int nStringLength = MAX_PATH;
    TCHAR tszBaseFileName[] = TEXT("%s\\StartStop%d");
    TCHAR tszCompletor[] = TEXT("File%d.asf");
#ifdef SAVE_CAPTURE_FILES
    TCHAR tszExtraPath[] = TEXT("\\release");
#else
    TCHAR tszExtraPath [] = TEXT("\0");
#endif
    HRESULT hr = S_OK;
    int nCaptureLength;
    int nStartLength = 0;
    int nTotalCaptureLength = 10000;
    int nShortLength = 4000;
    int nShortIncrement = 1000;
    int nMainIncrement = 3000;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            if(FAILED(pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                if(hr == E_GRAPH_UNSUPPORTED)
                {
                    DETAIL(TEXT("Initializing the capture graph failed as expected."));
                }
                else
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                }
            }
            else
            {
                // if we can't retrieve the number of capabilities, then it's because there's no preview stream available.
                if(FAILED(hr = pCaptureGraph->GetNumberOfCapabilities(STREAM_CAPTURE, &iCount, &iSize)))
                {
                    if(hr == VFW_E_NO_INTERFACE)
                        SKIP(TEXT("Failed to retrieve the number of capabilities, capture stream not available."));
                    else FAIL(TEXT("Failed to retrieve the number of capabilieis for the capture stream"));
                    goto cleanup;
                }

                if(iSize != sizeof(VIDEO_STREAM_CONFIG_CAPS))
                {
                    FAIL(TEXT("size of the configuration structure is incorrect, expected sizeof VIDEO_STREAM_CONFIG_CAPS."));
                }

                // entry 0 will be a NULL "default" entry, everything else will be retrieved from the graph.
                iCount ++;

                ppAMT = new AM_MEDIA_TYPE*[iCount];
                if(!ppAMT)
                {
                    FAIL(TEXT("Failed to allocate the media type array"));
                    goto cleanup;
                }
                // clear out the space just allocated
                memset(ppAMT, 0, sizeof(AM_MEDIA_TYPE *) * iCount);

                pvscc = new VIDEO_STREAM_CONFIG_CAPS[iCount];
                if(!pvscc)
                {
                    FAIL(TEXT("Failed to allocate the media type array"));
                    goto cleanup;
                }
                // clear out the space just allocated
                memset(pvscc, 0, sizeof(VIDEO_STREAM_CONFIG_CAPS) * iCount);


                // grab all of the formats available

                for(i = 1; i < iCount; i++)
                {
                    // because entry 0 is the "default" entry, the entry id is offset by 1
                    if(FAILED(pCaptureGraph->GetStreamCaps(STREAM_CAPTURE, i - 1, &(ppAMT[i]), (BYTE *) &(pvscc[i]))))
                    {
                        FAIL(TEXT("Failed to retrieve the stream caps."));
                        goto cleanup;
                    }
                }

                // cleanup the graph so the new format can be applied to a newly built graph
                if(FAILED(pCaptureGraph->Cleanup()))
                {
                    FAIL(TEXT("Cleaning up the graph failed."));
                }

                // for each of the formats gathered above, re-initialize the graph with the new
                // format and test it.

                for(i = 0; i < iCount; i++)
                {
                    VIDEOINFOHEADER *pVih = NULL;

                    Log(TEXT("Testing capture format %d of %d"), i, iCount);

                    if(ppAMT[i])
                    {
                        if(ppAMT[i]->formattype != FORMAT_VideoInfo)
                            FAIL(TEXT("Didn't receive a video info format type"));

                        if(ppAMT[i]->cbFormat < sizeof(VIDEOINFOHEADER))
                            FAIL(TEXT("data structure size is less than a VIDEOINFOHEADER"));


                        pVih = (VIDEOINFOHEADER *) ppAMT[i]->pbFormat;

                        Log(TEXT("Testing a format of %dx%dx%d."), pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight, pVih->bmiHeader.biBitCount);
                    }

                    if(FAILED(pCaptureGraph->SetFormat(STREAM_CAPTURE, ppAMT[i])))
                    {
                        FAIL(TEXT("Failed to set the format of the capture stream."));
                    }

                    if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
                    {
                        if(hr == E_GRAPH_UNSUPPORTED)
                        {
                            DETAIL(TEXT("Initializing the capture graph failed as expected."));
                        }
                        else
                        {
                            FAIL(TEXT("Initializing the capture graph failed."));
                        }
                    }
                    else
                    {
                        _stprintf(tszFileName, tszBaseFileName, tszExtraPath, i);
                        _tcscat(tszFileName, tszCompletor);

                        if(FAILED(pCaptureGraph->SetVideoCaptureFileName(tszFileName)))
                        {
                            FAIL(TEXT("Failed to set the capture filename."));
                        }

                        for(nCaptureLength = nStartLength; nCaptureLength < nTotalCaptureLength; nCaptureLength<nShortLength?nCaptureLength+=nShortIncrement:nCaptureLength += nMainIncrement)
                        {

                            // run the graph
                            if(FAILED(hr = pCaptureGraph->RunGraph()))
                            {
                                if(hr != E_OUTOFMEMORY)
                                    FAIL(TEXT("Starting the capture graph failed."));
                                else Log(TEXT("Unable to run the graph due to insufficient memory at this format"));
                            }
                            else
                            {
                                Log(TEXT("Verifying setting an invalid format fails."));
                                if(SUCCEEDED(pCaptureGraph->SetFormat(STREAM_CAPTURE, ppAMT[0])))
                                {
                                    FAIL(TEXT("Succeeded changing the format of the capture stream after it was initialized."));
                                }

                                HANDLE hFile = NULL;
                                WIN32_FIND_DATA fd;

                                Log(TEXT("Capture length %d seconds"), nCaptureLength/1000);

                                Log(TEXT("  Capture starting"));

                                // loop through a few times verifying the capture was correct.
                                if(FAILED(hr = pCaptureGraph->StartStreamCapture()) && hr != E_WRITE_ERROR_EVENT && hr != E_OOM_EVENT)
                                {
                                    if(hr != E_NO_FRAMES_PROCESSED)
                                        FAIL(TEXT("Failed to start the stream capture."));
                                    else Log(TEXT("Insufficient memory to process even one frame of the capture."));
                                }
                                else
                                {
                                    Log(TEXT("  Capture running"));

                                    Sleep(nCaptureLength);

                                    Log(TEXT("  Capture complete"));

                                    // make sure we properly handle the case where we ran out of memory while doing the test.
                                    hr = pCaptureGraph->StopStreamCapture();
                                    if(FAILED(hr))
                                    {
                                        if(hr == E_WRITE_ERROR_EVENT)
                                            Log(TEXT("Received a write error event while doing the capture (out of storage space?)."));
                                        else if(hr == E_OOM_EVENT)
                                            Log(TEXT("Received an OOM event while doing the capture (insufficient memory for this capture length?)."));
                                        else
                                            FAIL(TEXT("Failed to stop the stream capture."));
                                    }

                                    Log(TEXT("  Capture stream stopped"));

                                    if(FAILED(pCaptureGraph->StopGraph()))
                                    {
                                        FAIL(TEXT("Failed to stop the graph after the capture."));
                                    }

                                    // reset the string length for retrieval
                                    nStringLength = MAX_PATH;
                                    // retrieve the file name after the capture was started, so it's the file name of the captured file
                                    if(FAILED(pCaptureGraph->GetVideoCaptureFileName(tszFileName, &nStringLength)))
                                    {
                                        FAIL(TEXT("Failed to retrieve the video capture file name."));
                                    }

                                    if(nCaptureLength > 0 && FAILED(pCaptureGraph->VerifyVideoFileCaptured(tszFileName)))
                                    {
                                        FAIL(TEXT("The video file captured failed file conformance verification"));
                                    }

                                    // verify that the file was created
                                    hFile = FindFirstFile(tszFileName, &fd);
                                    if(hFile == INVALID_HANDLE_VALUE)
                                    {
                                        if(nCaptureLength > 0)
                                            FAIL(TEXT("Failed to find the video file."));

                                        Log(TEXT("Looking for file %s, not found."), tszFileName);
                                    }
                                    else if(FALSE == FindClose(hFile))
                                    {
                                        FAIL(TEXT("Failed to close the video file handle."));
                                    }

#ifndef SAVE_CAPTURE_FILES
                                    if(FALSE == DeleteFile(tszFileName))
                                    {
                                        if(nCaptureLength > 0)
                                            FAIL(TEXT("Failed to delete the video file."));
                                    }
#endif
                                }
                            }
                        }
                    }
                    // cleanup the graph so the next format can be applied to a newly built graph
                    if(FAILED(pCaptureGraph->Cleanup()))
                    {
                        FAIL(TEXT("Cleaning up the graph failed."));
                    }
                 }
            }
            // cleanup the graph so the next format can be applied to a newly built graph
            if(FAILED(pCaptureGraph->Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
            }
        }
    }

cleanup:

    // cleanup the graph on error
    if(pCaptureGraph && FAILED(pCaptureGraph->Cleanup()))
    {
        FAIL(TEXT("Cleaning up the graph failed."));
    }

    if(ppAMT)
    {
        for(i = 0; i < iCount; i++)
        {
            if(ppAMT[i])
            {
                DeleteMediaType(ppAMT[i]);
                ppAMT[i] = NULL;
            }
        }
        delete[] ppAMT;
        ppAMT = NULL;
    }

    if(pvscc)
    {
        delete[] pvscc;
        pvscc = NULL;
    }

    if(hwnd)
    {
        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI VerifyASFWritingFormatsPostConnect( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    AM_MEDIA_TYPE *ppAMT = NULL;
    int i, iSize, iCount;
    VIDEO_STREAM_CONFIG_CAPS pvscc;
    TCHAR tszFileName[MAX_PATH];
    int nStringLength = MAX_PATH;
    TCHAR tszBaseFileName[] = TEXT("%s\\StartStop%d");
    TCHAR tszCompletor[] = TEXT("File%d.asf");
#ifdef SAVE_CAPTURE_FILES
    TCHAR tszExtraPath[] = TEXT("\\release");
#else
    TCHAR tszExtraPath [] = TEXT("\0");
#endif
    HRESULT hr = S_OK;
    int nCaptureLength;
    int nStartLength = 0;
    int nTotalCaptureLength = 10000;
    int nShortLength = 4000;
    int nShortIncrement = 1000;
    int nMainIncrement = 3000;
    BOOL bOldForceColorConversion = FALSE;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            bOldForceColorConversion = pCaptureGraph->GetForceColorConversion();
            pCaptureGraph->SetForceColorConversion(FALSE);

            if(FAILED(pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                SKIP(TEXT("Initializing the capture graph failed, this is OK because the color converter was forced out of the graph."));
            }
            else
            {
                CComPtr<IGraphBuilder> pFilterGraph = NULL;
                CComPtr<IBaseFilter> pColorConverter = NULL;

                if (FAILED(pCaptureGraph->GetBuilderInterface((void**)&pFilterGraph, IID_IGraphBuilder)))
                {
                    FAIL(TEXT("Failed to retrieve the IGraphBuilder interface from the capture framework.."));
                    goto cleanup;
                }

                hr = pFilterGraph->FindFilterByName(TEXT("Color Converter"), &pColorConverter);

                // now that we're done with these, free them
                pFilterGraph = NULL;
                pColorConverter = NULL;

                // if we found the color converter above, then we need to continue on because it
                // doesn't support reconnects with different formats.
                if (SUCCEEDED(hr))
                {
                    SKIP(TEXT("The color conversion filter is in the graph, skipping because it does not support post connect operations."));
                    goto cleanup;
                }

                // if we can't retrieve the number of capabilities, then it's because there's no capture stream available.
                if(FAILED(hr = pCaptureGraph->GetNumberOfCapabilities(STREAM_CAPTURE, &iCount, &iSize)))
                {
                    if(hr == VFW_E_NO_INTERFACE)
                        SKIP(TEXT("Failed to retrieve the number of capabilities, capture stream not available."));
                    else FAIL(TEXT("Failed to retrieve the number of capabilieis for the capture stream"));
                    goto cleanup;
                }

                if(iSize != sizeof(VIDEO_STREAM_CONFIG_CAPS))
                {
                    FAIL(TEXT("size of the configuration structure is incorrect, expected sizeof VIDEO_STREAM_CONFIG_CAPS."));
                }


                for(i = 0; i < iCount; i++)
                {
                    VIDEOINFOHEADER *pVih = NULL;

                    Log(TEXT("Testing capture format %d of %d"), i + 1, iCount);

                    // because entry 0 is the "default" entry, the entry id is offset by 1
                    if(FAILED(pCaptureGraph->GetStreamCaps(STREAM_CAPTURE, i, &ppAMT, (BYTE *) &pvscc)))
                    {
                        FAIL(TEXT("Failed to retrieve the stream caps."));
                        goto cleanup;
                    }

                    if(ppAMT)
                    {
                        if(ppAMT->formattype != FORMAT_VideoInfo)
                            FAIL(TEXT("Didn't receive a video info format type"));

                        if(ppAMT->cbFormat < sizeof(VIDEOINFOHEADER))
                            FAIL(TEXT("data structure size is less than a VIDEOINFOHEADER"));


                        pVih = (VIDEOINFOHEADER *) ppAMT->pbFormat;

                        Log(TEXT("Testing a format of %dx%dx%d."), pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight, pVih->bmiHeader.biBitCount);
                    }

                    if(FAILED(pCaptureGraph->SetFormat(STREAM_CAPTURE, ppAMT)))
                    {
                        FAIL(TEXT("Failed to set the format of the capture stream."));
                    }

                    _stprintf(tszFileName, tszBaseFileName, tszExtraPath, i);
                    _tcscat(tszFileName, tszCompletor);

                    if(FAILED(pCaptureGraph->SetVideoCaptureFileName(tszFileName)))
                    {
                        FAIL(TEXT("Failed to set the capture filename."));
                    }

                    for(nCaptureLength = nStartLength; nCaptureLength < nTotalCaptureLength; nCaptureLength<nShortLength?nCaptureLength+=nShortIncrement:nCaptureLength += nMainIncrement)
                    {

                        // run the graph
                        if(FAILED(hr = pCaptureGraph->RunGraph()))
                        {
                            if(hr != E_OUTOFMEMORY)
                                FAIL(TEXT("Starting the capture graph failed."));
                            else Log(TEXT("Unable to run the graph due to insufficient memory at this format"));
                        }
                        else
                        {
                            HANDLE hFile = NULL;
                            WIN32_FIND_DATA fd;

                            Log(TEXT("Capture length %d seconds"), nCaptureLength/1000);

                            Log(TEXT("  Capture starting"));

                            // loop through a few times verifying the capture was correct.
                            if(FAILED(hr = pCaptureGraph->StartStreamCapture()) && hr != E_WRITE_ERROR_EVENT && hr != E_OOM_EVENT)
                            {
                                if(hr != E_NO_FRAMES_PROCESSED)
                                    FAIL(TEXT("Failed to start the stream capture."));
                                else Log(TEXT("Insufficient memory to process even one frame of the capture."));
                            }
                            else
                            {
                                Log(TEXT("  Capture running"));

                                Sleep(nCaptureLength);

                                Log(TEXT("  Capture complete"));

                                // make sure we properly handle the case where we ran out of memory while doing the test.
                                hr = pCaptureGraph->StopStreamCapture();
                                if(FAILED(hr))
                                {
                                    if(hr == E_WRITE_ERROR_EVENT)
                                        Log(TEXT("Received a write error event while doing the capture (out of storage space?)."));
                                    else if(hr == E_OOM_EVENT)
                                        Log(TEXT("Received an OOM event while doing the capture (insufficient memory for this capture length?)."));
                                    else
                                        FAIL(TEXT("Failed to stop the stream capture."));
                                }

                                Log(TEXT("  Capture stream stopped"));

                                if(FAILED(pCaptureGraph->StopGraph()))
                                {
                                    FAIL(TEXT("Failed to stop the graph after the capture."));
                                }

                                // reset the string length for retrieval
                                nStringLength = MAX_PATH;
                                // retrieve the file name after the capture was started, so it's the file name of the captured file
                                if(FAILED(pCaptureGraph->GetVideoCaptureFileName(tszFileName, &nStringLength)))
                                {
                                    FAIL(TEXT("Failed to retrieve the video capture file name."));
                                }

                                if(nCaptureLength > 0 && FAILED(pCaptureGraph->VerifyVideoFileCaptured(tszFileName)))
                                {
                                    FAIL(TEXT("The video file captured failed file conformance verification"));
                                }

                                // verify that the file was created
                                hFile = FindFirstFile(tszFileName, &fd);
                                if(hFile == INVALID_HANDLE_VALUE)
                                {
                                    if(nCaptureLength > 0)
                                        FAIL(TEXT("Failed to find the video file."));

                                    Log(TEXT("Looking for file %s, not found."), tszFileName);
                                }
                                else if(FALSE == FindClose(hFile))
                                {
                                    FAIL(TEXT("Failed to close the video file handle."));
                                }

#ifndef SAVE_CAPTURE_FILES
                                if(FALSE == DeleteFile(tszFileName))
                                {
                                    if(nCaptureLength > 0)
                                        FAIL(TEXT("Failed to delete the video file."));
                                }
#endif
                            }
                        }
                    }

                    if(ppAMT)
                    {
                        DeleteMediaType(ppAMT);
                        ppAMT = NULL;
                    }
                 }
            }
            // cleanup the graph so the next format can be applied to a newly built graph
            if(FAILED(pCaptureGraph->Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
            }

            pCaptureGraph->SetForceColorConversion(bOldForceColorConversion);
        }
    }

cleanup:

    if(ppAMT)
    {
        DeleteMediaType(ppAMT);
        ppAMT = NULL;
    }

    // cleanup the graph on error
    if(pCaptureGraph && FAILED(pCaptureGraph->Cleanup()))
    {
        FAIL(TEXT("Cleaning up the graph failed."));
    }

    if(hwnd)
    {
        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}


TESTPROCAPI VerifyASFWritingAudioFormats( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    AM_MEDIA_TYPE **ppAMT = NULL;
    int i, iSize, iCount;
    AUDIO_STREAM_CONFIG_CAPS *pascc = NULL;
    TCHAR tszFileName[MAX_PATH];
    int nStringLength = MAX_PATH;
    TCHAR tszBaseFileName[] = TEXT("%s\\StartStop%d");
    TCHAR tszCompletor[] = TEXT("File%d.asf");
#ifdef SAVE_CAPTURE_FILES
    TCHAR tszExtraPath[] = TEXT("\\release");
#else
    TCHAR tszExtraPath [] = TEXT("\0");
#endif
    HRESULT hr = S_OK;
    int nCaptureLength;
    int nStartLength = 1000;
    int nTotalCaptureLength = 3000;
    int nShortLength = 0;
    int nShortIncrement = 0;
    int nMainIncrement = 2000;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                if(hr == E_GRAPH_UNSUPPORTED)
                {
                    DETAIL(TEXT("Initializing the capture graph failed as expected."));
                }
                else
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                }
            }
            else
            {
                // if we can't retrieve the number of capabilities, then it's because there's no preview stream available.
                if(FAILED(hr = pCaptureGraph->GetNumberOfCapabilities(STREAM_AUDIO, &iCount, &iSize)))
                {
                    if(hr == VFW_E_NO_INTERFACE)
                        SKIP(TEXT("Failed to retrieve the number of capabilities, capture stream not available."));
                    else FAIL(TEXT("Failed to retrieve the number of capabilieis for the capture stream"));
                    goto cleanup;
                }

                if(iSize != sizeof(AUDIO_STREAM_CONFIG_CAPS))
                {
                    FAIL(TEXT("size of the configuration structure is incorrect, expected sizeof VIDEO_STREAM_CONFIG_CAPS."));
                }

                // entry 0 will be a NULL "default" entry, everything else will be retrieved from the graph.
                iCount ++;

                ppAMT = new AM_MEDIA_TYPE*[iCount];
                if(!ppAMT)
                {
                    FAIL(TEXT("Failed to allocate the media type array"));
                    goto cleanup;
                }
                // clear out the space just allocated
                memset(ppAMT, 0, sizeof(AM_MEDIA_TYPE *) * iCount);

                pascc = new AUDIO_STREAM_CONFIG_CAPS[iCount];
                if(!pascc)
                {
                    FAIL(TEXT("Failed to allocate the media type array"));
                    goto cleanup;
                }
                // clear out the space just allocated
                memset(pascc, 0, sizeof(AUDIO_STREAM_CONFIG_CAPS) * iCount);


                // grab all of the formats available

                for(i = 1; i < iCount; i++)
                {
                    // because entry 0 is the "default" entry, the entry id is offset by 1
                    if(FAILED(pCaptureGraph->GetStreamCaps(STREAM_AUDIO, i - 1, &(ppAMT[i]), (BYTE *) &(pascc[i]))))
                    {
                        FAIL(TEXT("Failed to retrieve the stream caps."));
                        goto cleanup;
                    }
                }

                // cleanup the graph so the new format can be applied to a newly built graph
                if(FAILED(pCaptureGraph->Cleanup()))
                {
                    FAIL(TEXT("Cleaning up the graph failed."));
                }

                // for each of the formats gathered above, re-initialize the graph with the new
                // format and test it.

                for(i = 0; i < iCount; i++)
                {
                    Log(TEXT("Testing audio capture format %d of %d"), i, iCount);

                    if(FAILED(pCaptureGraph->SetFormat(STREAM_AUDIO, ppAMT[i])))
                    {
                        FAIL(TEXT("Failed to set the format of the capture stream."));
                    }

                    if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
                    {
                        if(hr == E_GRAPH_UNSUPPORTED)
                        {
                            DETAIL(TEXT("Initializing the capture graph failed as expected."));
                        }
                        else
                        {
                            FAIL(TEXT("Initializing the capture graph failed."));
                        }
                    }
                    else
                    {
                        Log(TEXT("Verifying setting an invalid format fails."));
                        if(SUCCEEDED(pCaptureGraph->SetFormat(STREAM_AUDIO, ppAMT[0])))
                        {
                            FAIL(TEXT("Succeeded setting the format of the capture stream after it was initialized."));
                        }

                        _stprintf(tszFileName, tszBaseFileName, tszExtraPath, i);
                        _tcscat(tszFileName, tszCompletor);

                        if(FAILED(pCaptureGraph->SetVideoCaptureFileName(tszFileName)))
                        {
                            FAIL(TEXT("Failed to set the capture filename."));
                        }

                        for(nCaptureLength = nStartLength; nCaptureLength < nTotalCaptureLength; nCaptureLength<nShortLength?nCaptureLength+=nShortIncrement:nCaptureLength += nMainIncrement)
                        {
                            // run the graph
                            if(FAILED(hr = pCaptureGraph->RunGraph()))
                            {
                                if(hr != E_OUTOFMEMORY)
                                    FAIL(TEXT("Starting the capture graph failed."));
                                else Log(TEXT("Unable to run the graph due to insufficient memory at this format"));
                            }
                            else
                            {
                                Log(TEXT("Verifying setting an invalid format fails."));
                                if(SUCCEEDED(pCaptureGraph->SetFormat(STREAM_AUDIO, ppAMT[0])))
                                {
                                    FAIL(TEXT("Succeeded setting the format of the capture stream after it was initialized."));
                                }

                                HANDLE hFile = NULL;
                                WIN32_FIND_DATA fd;

                                Log(TEXT("Capture length %d seconds"), nCaptureLength/1000);

                                Log(TEXT("  Capture starting"));

                                // loop through a few times verifying the capture was correct.
                                if(FAILED(hr = pCaptureGraph->StartStreamCapture())  && hr != E_WRITE_ERROR_EVENT && hr != E_OOM_EVENT)
                                {
                                    if(hr != E_NO_FRAMES_PROCESSED)
                                        FAIL(TEXT("Failed to start the stream capture."));
                                    else Log(TEXT("Insufficient memory to process even one frame of the capture."));
                                }
                                else
                                {
                                    Log(TEXT("  Capture running"));

                                    Sleep(nCaptureLength);

                                    Log(TEXT("  Capture complete"));

                                    // make sure we properly handle the case where we ran out of memory while doing the test.
                                    hr = pCaptureGraph->StopStreamCapture();
                                    if(FAILED(hr))
                                    {
                                        if(hr == E_WRITE_ERROR_EVENT)
                                            Log(TEXT("Received a write error event while doing the capture (out of storage space?)."));
                                        else if(hr == E_OOM_EVENT)
                                            Log(TEXT("Received an OOM event while doing the capture (insufficient memory for this capture length?)."));
                                        else
                                            FAIL(TEXT("Failed to stop the stream capture."));
                                    }

                                    Log(TEXT("  Capture stream stopped"));

                                    if(FAILED(pCaptureGraph->StopGraph()))
                                    {
                                        FAIL(TEXT("Failed to stop the graph after capture."));
                                    }

                                    // reset the string length for retrieval
                                    nStringLength = MAX_PATH;
                                    // retrieve the file name after the capture was started, so it's the file name of the captured file
                                    if(FAILED(pCaptureGraph->GetVideoCaptureFileName(tszFileName, &nStringLength)))
                                    {
                                        FAIL(TEXT("Failed to retrieve the video capture file name."));
                                    }

                                    if(nCaptureLength > 0 && FAILED(pCaptureGraph->VerifyVideoFileCaptured(tszFileName)))
                                    {
                                        FAIL(TEXT("The video file captured failed file conformance verification"));
                                    }

                                    // verify that the file was created
                                    hFile = FindFirstFile(tszFileName, &fd);
                                    if(hFile == INVALID_HANDLE_VALUE)
                                    {
                                        if(nCaptureLength > 0)
                                            FAIL(TEXT("Failed to find the video file."));

                                        Log(TEXT("Looking for file %s, not found."), tszFileName);
                                    }
                                    else if(FALSE == FindClose(hFile))
                                    {
                                        FAIL(TEXT("Failed to close the video file handle."));
                                    }

#ifndef SAVE_CAPTURE_FILES
                                    if(FALSE == DeleteFile(tszFileName))
                                    {
                                        if(nCaptureLength > 0)
                                            FAIL(TEXT("Failed to delete the video file."));
                                    }
#endif
                                }
                            }
                        }
                    }
                    // cleanup the graph so the next format can be applied to a newly built graph
                    if(FAILED(pCaptureGraph->Cleanup()))
                    {
                        FAIL(TEXT("Cleaning up the graph failed."));
                    }
                 }
            }
            // cleanup the graph so the next format can be applied to a newly built graph
            if(FAILED(pCaptureGraph->Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
            }
        }
    }

cleanup:

    // cleanup the graph on error
    if(pCaptureGraph && FAILED(pCaptureGraph->Cleanup()))
    {
        FAIL(TEXT("Cleaning up the graph failed."));
    }

    if(ppAMT)
    {
        for(i = 0; i < iCount; i++)
        {
            if(ppAMT[i])
            {
                DeleteMediaType(ppAMT[i]);
                ppAMT[i] = NULL;
            }
        }
        delete[] ppAMT;
        ppAMT = NULL;
    }

    if(pascc)
    {
        delete[] pascc;
        pascc = NULL;
    }

    if(hwnd)
    {
        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}


TESTPROCAPI VerifyASFFrameDropping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    int nCaptureLength = 30000;
    // maximum of 5 minutes possible
    int nMaxCaptureLength = 300000;
    int nCaptureLengthIncrease = 30000;
    TCHAR tszFileName[MAX_PATH];
    int nStringLength = MAX_PATH;
    TCHAR tszBaseFileName[] = TEXT("%s\\FrameDropping");
    TCHAR tszCompletor[] = TEXT("%d.asf");
#ifdef SAVE_CAPTURE_FILES
    TCHAR tszExtraPath[] = TEXT("\\release");
#else
    TCHAR tszExtraPath [] = TEXT("\0");
#endif

    // entry 0 is video, entry 1 is audio
    REFERENCE_TIME rtBufferDepths[][2] = {{0, 0},
                                             {MAXLONGLONG, MAXLONGLONG},
                                             {10000000, MAXLONGLONG},
                                             {10000000, 30000000},
                                             {MAXLONGLONG, 10000000 },
                                             {30000000, 10000000},
                                           };
    HRESULT hr = S_OK;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                if(hr == E_GRAPH_UNSUPPORTED)
                {
                    DETAIL(TEXT("Initializing the capture graph failed as expected."));
                }
                else
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                }
            }
            else
            {
                DShowEvent dse;
                memset(&dse, 0, sizeof(DShowEvent));

                dse.Code = EC_CAP_SAMPLE_PROCESSED;
                dse.FilterFlags = EVT_EVCODE_EQUAL | EVT_EXCLUDE;

                if(FAILED(hr = pCaptureGraph->SetEventFilters(1, &dse)))
                {
                    FAIL(TEXT("Failed to clear the event filters."));
                }

                _stprintf(tszFileName, tszBaseFileName, tszExtraPath);
                _tcscat(tszFileName, tszCompletor);

                if(FAILED(pCaptureGraph->SetVideoCaptureFileName(tszFileName)))
                {
                    FAIL(TEXT("Failed to set the capture filename."));
                }

                for(int i = 0; i < countof(rtBufferDepths); i++)
                {
                    if(FAILED(pCaptureGraph->RunGraph()))
                    {
                        FAIL(TEXT("Starting the capture graph failed."));
                    }
                    else
                    {
                        Log(TEXT("Testing combination %d"), i);

                        if(FAILED(pCaptureGraph->SetBufferingDepth(STREAM_CAPTURE, &rtBufferDepths[i][0])))
                        {
                            FAIL(TEXT("Failed to set the video buffer depth"));
                        }

                        // only attempt to set the buffering depth on the audio stream if the audio stream is in the graph.
                        if(dwGraphLayout  & AUDIO_CAPTURE_FILTER)
                        {
                            if(FAILED(pCaptureGraph->SetBufferingDepth(STREAM_AUDIO, &rtBufferDepths[i][1])))
                            {
                                FAIL(TEXT("Failed to set the audio buffer depth"));
                            }
                        }

                        HANDLE hFile = NULL;
                        WIN32_FIND_DATA fd;

                        Log(TEXT("Capture length %f seconds"), nCaptureLength/1000.);

                        Log(TEXT("  Capture starting"));

                        // loop through a few times verifying the capture was correct.
                        if(FAILED(hr = pCaptureGraph->StartStreamCapture())  && hr != E_WRITE_ERROR_EVENT && hr != E_OOM_EVENT)
                        {
                            if(hr != E_NO_FRAMES_PROCESSED)
                                FAIL(TEXT("Failed to start the stream capture."));
                            else Log(TEXT("Insufficient memory to process even one frame of the capture."));
                        }
                        else
                        {
                            Log(TEXT("  Capture running"));
                            LONG lEvent, lParam1, lParam2;
                            HRESULT hrEvent;
                            DWORD dwTickCountStart = GetTickCount();
                            DWORD dwTickCountCurrent;
                            LONG lVideoFramesProcessed, lAudioFramesProcessed, lVideoFramesDropped, lAudioFramesDropped;
                            BOOL bFramesDropped = FALSE;
                            BOOL bSystemOOM = FALSE;

                            // busy wait to push the cpu usage up to force dropping
                            while(1)
                            {
                                hrEvent = pCaptureGraph->GetEvent(&lEvent, &lParam1, &lParam2, 0);

                                if(SUCCEEDED(hrEvent))
                                {
                                    // we hit an oom, so let some frames get processed.
                                    // this means that we hit an oom, then back off from ooming the system a little, and then push
                                    // oom again.  we repeat this for 1 minute, trying to force a lot of frames dropped, increasing
                                    // the probability of an invalid asf file.
                                    if(lEvent == EC_BUFFER_FULL)
                                    {
                                        if(lParam2 == 0)
                                            bSystemOOM = TRUE;

                                        if(!bFramesDropped)
                                        {
                                            bFramesDropped = TRUE;
                                            Log(TEXT("First dropped frame hit."));
                                        }

                                        Sleep(rand() % 100);
                                    }
                                }
                                // if we failed, and it wasn't a timeout, then we have a problem.
                                else if(FAILED(hrEvent) && hrEvent != E_ABORT)
                                {
                                    FAIL(TEXT("Retrieving the media event failed for an unknown reason"));
                                    break;
                                }

                                dwTickCountCurrent = GetTickCount();
                                if((int) (dwTickCountCurrent - dwTickCountStart) >= nCaptureLength)
                                {
                                    // if we haven't dropped frames after 1 minute of running, increase the capture
                                    // length by 1 minute and keep going (up to 5 minutes).
                                    // if we still haven't dropped any frames within 5 minutes, then give up.
                                    if(!bFramesDropped && nCaptureLength < nMaxCaptureLength)
                                    {
                                        Log(TEXT("Capture length hit without dropping frames, extending capture length by %d seconds"), nCaptureLengthIncrease/1000);
                                        nCaptureLength += nCaptureLengthIncrease;
                                    }
                                    else
                                        break;
                                }

                                // if frames have already been dropped, then give up a little cpu, but not until then.
                                if(bFramesDropped)
                                    Sleep(0);
                            }

                            Log(TEXT("  Capture complete"));

                            // make sure we properly handle the case where we ran out of memory while doing the test.
                            // also, as we're waiting for a significant amount of intentionally buffered video, increase the
                            // default delay to compensate
                            hr = pCaptureGraph->StopStreamCapture(nCaptureLength);
                            if(FAILED(hr))
                            {
                                if(hr == E_WRITE_ERROR_EVENT)
                                    Log(TEXT("Received a write error event while doing the capture (out of storage space?)."));
                                else if(hr == E_OOM_EVENT)
                                    Log(TEXT("Frames were dropped."));
                                else
                                    FAIL(TEXT("Failed to stop the stream capture."));
                            }


                            if(rtBufferDepths[i][0] == MAXLONGLONG && rtBufferDepths[i][1] == MAXLONGLONG)
                            {
                                // if system oom was hit, then everything's fine
                                if(bSystemOOM)
                                    Log(TEXT("System OOM hit"));
                                // otherwise, if frames were dropped and system oom wasn't hit, then we have a problem
                                else if(bFramesDropped)
                                    FAIL(TEXT("Should have been limited by system oom, but system OOM not hit"));
                            }

                            if(FAILED(pCaptureGraph->GetFramesProcessed(&lVideoFramesProcessed, &lAudioFramesProcessed)))
                            {
                                FAIL(TEXT("Failed to retrieve the frames processed."));
                            }

                            if(FAILED(pCaptureGraph->GetFramesDropped(&lVideoFramesDropped, &lAudioFramesDropped)))
                            {
                                FAIL(TEXT("Failed to retrieve the frames dropped."));
                            }

                            // if there's an audio driver in the graph, then check the audio frame drop count. Otherwise ignore it.
                            if(dwGraphLayout  & AUDIO_CAPTURE_FILTER)
                            {
                                if(lAudioFramesDropped > 0)
                                {
                                    if(rtBufferDepths[i][1] > rtBufferDepths[i][0] && rtBufferDepths[i][1] == MAXLONGLONG && !bSystemOOM)
                                    {
                                        FAIL(TEXT("Audio frames dropped when no audio frames are supposed to be dropped."));
                                    }
                                }
                            }

                            if(lVideoFramesDropped > 0)
                            {
                                if(rtBufferDepths[i][0] > rtBufferDepths[i][1] && rtBufferDepths[i][0] == MAXLONGLONG && !bSystemOOM)
                                {
                                    FAIL(TEXT("Video frames dropped when no video frames are supposed to be dropped."));
                                }
                            }

                            Log(TEXT("  Capture stream stopped, %d video frames processed, %d audio frames processed, %d video frames dropped, %d audio frames dropped, %d total frames."), lVideoFramesProcessed, lAudioFramesProcessed, lVideoFramesDropped, lAudioFramesDropped,lVideoFramesProcessed + lAudioFramesProcessed + lVideoFramesDropped + lAudioFramesDropped);

                            // retrieve all of the capture stats before we stop the graph, which wipes them out.
                            if(FAILED(pCaptureGraph->StopGraph()))
                            {
                                FAIL(TEXT("Failed to stop the graph after capture"));
                            }

                            // reset the string length for retrieval
                            nStringLength = MAX_PATH;
                            // retrieve the file name after the capture was started, so it's the file name of the captured file
                            if(FAILED(pCaptureGraph->GetVideoCaptureFileName(tszFileName, &nStringLength)))
                            {
                                FAIL(TEXT("Failed to retrieve the video capture file name."));
                                Log(TEXT("File %s, failed verification."), tszFileName);
                            }

                            if(nCaptureLength > 0 && FAILED(pCaptureGraph->VerifyVideoFileCaptured(tszFileName)))
                            {
                                FAIL(TEXT("The video file captured failed file conformance verification"));
                            }

                            // verify that the file was created
                            hFile = FindFirstFile(tszFileName, &fd);
                            if(hFile == INVALID_HANDLE_VALUE)
                            {
                                if(nCaptureLength > 0)
                                    FAIL(TEXT("Failed to find the video file."));

                                Log(TEXT("Looking for file %s, not found."), tszFileName);
                            }
                            else if(FALSE == FindClose(hFile))
                            {
                                FAIL(TEXT("Failed to close the video file handle."));
                            }

#ifndef SAVE_CAPTURE_FILES
                            if(FALSE == DeleteFile(tszFileName))
                            {
                                if(nCaptureLength > 0)
                                    FAIL(TEXT("Failed to delete the video file."));
                            }
#endif
                        }
                    }
                }
            }
            if(FAILED(pCaptureGraph->Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
            }
        }


        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI VerifyASFStopGraph( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    int nCaptureLength;

    TCHAR tszFileName[MAX_PATH];
    int nStringLength = MAX_PATH;
    TCHAR tszBaseFileName[] = TEXT("%s\\CaptureStopped");
    TCHAR tszCompletor[] = TEXT("File%d.asf");
#ifdef SAVE_CAPTURE_FILES
    TCHAR tszExtraPath[] = TEXT("\\release");
#else
    TCHAR tszExtraPath [] = TEXT("\0");
#endif
    HRESULT hr = S_OK;
    int nStartLength = 0;
    int nTotalCaptureLength = 10000;
    int nShortLength = 4000;
    int nShortIncrement = 1000;
    int nMainIncrement = 3000;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                if(hr == E_GRAPH_UNSUPPORTED)
                {
                    DETAIL(TEXT("Initializing the capture graph failed as expected."));
                }
                else
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                }
            }
            else
            {
                _stprintf(tszFileName, tszBaseFileName, tszExtraPath);
                _tcscat(tszFileName, tszCompletor);

                if(FAILED(pCaptureGraph->SetVideoCaptureFileName(tszFileName)))
                {
                    FAIL(TEXT("Failed to set the capture filename."));
                }

                for(nCaptureLength = nStartLength; nCaptureLength < nTotalCaptureLength; nCaptureLength<nShortLength?nCaptureLength+=nShortIncrement:nCaptureLength += nMainIncrement)
                {
                    if(FAILED(pCaptureGraph->RunGraph()))
                    {
                        FAIL(TEXT("Starting the capture graph failed."));
                    }
                    else
                    {
                        HANDLE hFile = NULL;
                        WIN32_FIND_DATA fd;

                        Log(TEXT("Capture length %d seconds"), nCaptureLength/1000);

                        Log(TEXT("  Capture starting"));

                        // loop through a few times verifying the capture was correct.
                        if(FAILED(hr = pCaptureGraph->StartStreamCapture())  && hr != E_WRITE_ERROR_EVENT && hr != E_OOM_EVENT)
                        {
                            if(hr != E_NO_FRAMES_PROCESSED)
                                FAIL(TEXT("Failed to start the stream capture."));
                            else Log(TEXT("Insufficient memory to process even one frame of the capture."));
                        }
                        else
                        {
                            Log(TEXT("  Capture running"));

                            Sleep(nCaptureLength);

                            Log(TEXT("  Capture complete"));

                            // now stop the graph in the middle of the capture, this should flush the pipe
                            // and result in a well formed (but possibly shortened) asf file.
                            if(FAILED(pCaptureGraph->StopGraph()))
                            {
                                FAIL(TEXT("Failed to stop the graph."));
                            }

                            Log(TEXT("  Capture stream stopped"));

                            // reset the string length for retrieval
                            nStringLength = MAX_PATH;
                            // retrieve the file name after the capture was started, so it's the file name of the captured file
                            if(FAILED(pCaptureGraph->GetVideoCaptureFileName(tszFileName, &nStringLength)))
                            {
                                FAIL(TEXT("Failed to retrieve the video capture file name."));
                            }

                            if(nCaptureLength > 0 && FAILED(pCaptureGraph->VerifyVideoFileCaptured(tszFileName)))
                            {
                                FAIL(TEXT("The video file captured failed file conformance verification"));
                            }

                            // verify that the file was created
                            hFile = FindFirstFile(tszFileName, &fd);
                            if(hFile == INVALID_HANDLE_VALUE)
                            {
                                if(nCaptureLength > 0)
                                    FAIL(TEXT("Failed to find the video file."));

                                Log(TEXT("Looking for file %s, not found."), tszFileName);
                            }
                            else if(FALSE == FindClose(hFile))
                            {
                                FAIL(TEXT("Failed to close the video file handle."));
                            }

#ifndef SAVE_CAPTURE_FILES
                            if(FALSE == DeleteFile(tszFileName))
                            {
                                if(nCaptureLength > 0)
                                    FAIL(TEXT("Failed to delete the video file."));
                            }
#endif
                        }
                    }
                }
            }
            if(FAILED(pCaptureGraph->Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
            }
        }

        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI VerifyASFPauseGraph( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    int nCaptureLength;

    TCHAR tszFileName[MAX_PATH];
    int nStringLength = MAX_PATH;
    TCHAR tszBaseFileName[] = TEXT("%s\\CapturePaused");
    TCHAR tszCompletor[] = TEXT("File%d.asf");
#ifdef SAVE_CAPTURE_FILES
    TCHAR tszExtraPath[] = TEXT("\\release");
#else
    TCHAR tszExtraPath [] = TEXT("\0");
#endif
    HRESULT hr = S_OK;
    int nStartLength = 0;
    int nTotalCaptureLength = 10000;
    int nShortLength = 4000;
    int nShortIncrement = 1000;
    int nMainIncrement = 3000;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                if(hr == E_GRAPH_UNSUPPORTED)
                {
                    DETAIL(TEXT("Initializing the capture graph failed as expected."));
                }
                else
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                }
            }
            else
            {
                _stprintf(tszFileName, tszBaseFileName, tszExtraPath);
                _tcscat(tszFileName, tszCompletor);

                if(FAILED(pCaptureGraph->SetVideoCaptureFileName(tszFileName)))
                {
                    FAIL(TEXT("Failed to set the capture filename."));
                }

                for(nCaptureLength = nStartLength; nCaptureLength < nTotalCaptureLength; nCaptureLength<nShortLength?nCaptureLength+=nShortIncrement:nCaptureLength += nMainIncrement)
                {
                    if(FAILED(pCaptureGraph->RunGraph()))
                    {
                        FAIL(TEXT("Starting the capture graph failed."));
                    }
                    else
                    {
                        HANDLE hFile = NULL;
                        WIN32_FIND_DATA fd;

                        Log(TEXT("Capture length %d seconds"), nCaptureLength/1000);

                        Log(TEXT("  Capture starting"));

                        // loop through a few times verifying the capture was correct.
                        if(FAILED(hr = pCaptureGraph->StartStreamCapture())  && hr != E_WRITE_ERROR_EVENT && hr != E_OOM_EVENT)
                        {
                            if(hr != E_NO_FRAMES_PROCESSED)
                                FAIL(TEXT("Failed to start the stream capture."));
                            else Log(TEXT("Insufficient memory to process even one frame of the capture."));
                        }
                        else
                        {
                            Log(TEXT("  Capture running"));

                            // we're going to split the clip into 10 pieces, and those ten pieces will spend 50% of the time running,
                            // and 50% of the time paused.

                            for(int PauseCount = 0; PauseCount < 10; PauseCount++)
                            {
                                Sleep(nCaptureLength / 20);

                                hr = pCaptureGraph->PauseGraph();
                                if(FAILED(hr))
                                {
                                    FAIL(TEXT("Failed to stop the graph."));
                                }

                                Sleep(nCaptureLength / 20);

                                hr = pCaptureGraph->RunGraph();
                                if(FAILED(hr))
                                {
                                    FAIL(TEXT("Starting the capture graph failed."));
                                }
                            }

                            Log(TEXT("  Capture complete"));

                            // make sure we properly handle the case where we ran out of memory while doing the test.
                            hr = pCaptureGraph->StopStreamCapture();
                            if(FAILED(hr))
                            {
                                if(hr == E_WRITE_ERROR_EVENT)
                                    Log(TEXT("Received a write error event while doing the capture (out of storage space?)."));
                                else if(hr == E_OOM_EVENT)
                                    Log(TEXT("Received an OOM event while doing the capture (insufficient memory for this capture length?)."));
                                else if(hr == E_BUFFER_DELIVERY_FAILED)
                                {
                                    Log(TEXT("Received a downstream error while waiting for the graph to stop."));
                                    Log(TEXT("This is OK, because the pause is delivered upstream from the end and can cause a delivery failure."));
                                }
                                else
                                    FAIL(TEXT("Failed to stop the stream capture."));
                            }

                            Log(TEXT("  Capture stream stopped"));

                            if(FAILED(pCaptureGraph->StopGraph()))
                            {
                                FAIL(TEXT("Failed to stop the graph after capture"));
                            }

                            // reset the string length for retrieval
                            nStringLength = MAX_PATH;
                            // retrieve the file name after the capture was started, so it's the file name of the captured file
                            if(FAILED(pCaptureGraph->GetVideoCaptureFileName(tszFileName, &nStringLength)))
                            {
                                FAIL(TEXT("Failed to retrieve the video capture file name."));
                            }

                            if(nCaptureLength > 0 && FAILED(pCaptureGraph->VerifyVideoFileCaptured(tszFileName)))
                            {
                                FAIL(TEXT("The video file captured failed file conformance verification"));
                            }

                            // verify that the file was created
                            hFile = FindFirstFile(tszFileName, &fd);
                            if(hFile == INVALID_HANDLE_VALUE)
                            {
                                if(nCaptureLength > 0)
                                    FAIL(TEXT("Failed to find the video file."));

                                Log(TEXT("Looking for file %s, not found."), tszFileName);
                            }
                            else if(FALSE == FindClose(hFile))
                            {
                                FAIL(TEXT("Failed to close the video file handle."));
                            }

#ifndef SAVE_CAPTURE_FILES
                            if(FALSE == DeleteFile(tszFileName))
                            {
                                if(nCaptureLength > 0)
                                    FAIL(TEXT("Failed to delete the video file."));
                            }
#endif
                        }
                    }
                }
            }
            if(FAILED(pCaptureGraph->Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
            }
        }

        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}


TESTPROCAPI VerifyASFContinued( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    int nCaptureLength;

    TCHAR tszFileName[MAX_PATH];
    int nStringLength = MAX_PATH;
    TCHAR tszBaseFileName[] = TEXT("%s\\CapturePaused");
    TCHAR tszCompletor[] = TEXT("File%d.asf");
#ifdef SAVE_CAPTURE_FILES
    TCHAR tszExtraPath[] = TEXT("\\release");
#else
    TCHAR tszExtraPath [] = TEXT("\0");
#endif
    HRESULT hr = S_OK;
    int nStartLength = 0;
    int nTotalCaptureLength = 20000;
    int nShortLength = 4000;
    int nShortIncrement = 1000;
    int nMainIncrement = 3000;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                if(hr == E_GRAPH_UNSUPPORTED)
                {
                    DETAIL(TEXT("Initializing the capture graph failed as expected."));
                }
                else
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                }
            }
            else
            {
                _stprintf(tszFileName, tszBaseFileName, tszExtraPath);
                _tcscat(tszFileName, tszCompletor);

                if(FAILED(pCaptureGraph->SetVideoCaptureFileName(tszFileName)))
                {
                    FAIL(TEXT("Failed to set the capture filename."));
                }

                for(nCaptureLength = nStartLength; nCaptureLength < nTotalCaptureLength; nCaptureLength<nShortLength?nCaptureLength+=nShortIncrement:nCaptureLength += nMainIncrement)
                {
                    if(FAILED(pCaptureGraph->RunGraph()))
                    {
                        FAIL(TEXT("Starting the capture graph failed."));
                    }
                    else
                    {
                        HANDLE hFile = NULL;
                        WIN32_FIND_DATA fd;

                        // retrieve the file information for the capture in progress, to verify that the file grows                        
                        nStringLength = MAX_PATH;
                        if(FAILED(pCaptureGraph->GetVideoCaptureFileName(tszFileName, &nStringLength)))
                        {
                            FAIL(TEXT("Failed to retrieve the video capture file name."));
                        }

                        Log(TEXT("Capture length %d seconds"), nCaptureLength/1000);

                        Log(TEXT("  Capture starting"));

                        // loop through a few times verifying the capture was correct.
                        if(FAILED(hr = pCaptureGraph->StartStreamCapture())  && hr != E_WRITE_ERROR_EVENT && hr != E_OOM_EVENT)
                        {
                            if(hr != E_NO_FRAMES_PROCESSED)
                                FAIL(TEXT("Failed to start the stream capture."));
                            else Log(TEXT("Insufficient memory to process even one frame of the capture."));
                        }
                        else
                        {
                            Log(TEXT("  Capture running"));
                            DWORD dwPreviousIterationFileSize = 0;

                            // we're going to split the clip into 10 pieces, and those ten pieces will spend 50% of the time running,
                            // and 50% of the time paused.

                            for(int PauseCount = 0; PauseCount < 10; PauseCount++)
                            {
                                Sleep(nCaptureLength / 20);

                                hr = pCaptureGraph->StopStreamCapture();
                                if(FAILED(hr))
                                {
                                    if(hr == E_WRITE_ERROR_EVENT)
                                        Log(TEXT("Received a write error event while doing the capture (out of storage space?)."));
                                    else if(hr == E_OOM_EVENT)
                                        Log(TEXT("Received an OOM event while doing the capture (insufficient memory for this capture length?)."));
                                    else
                                        FAIL(TEXT("Failed to stop the stream capture."));
                                }

                                // delay for 1/20th of the total capture before resuming the capture.
                                // delay before gathering file info to give the file system a chance to flush the data.
                                Sleep(nCaptureLength / 20);

                                // verify that the file was created, do the verification while the capture is paused.
                                hFile = FindFirstFile(tszFileName, &fd);
                                if(hFile == INVALID_HANDLE_VALUE)
                                {
                                    FAIL(TEXT("Failed to find the video file."));
                                }
                                else if(FALSE == FindClose(hFile))
                                {
                                    FAIL(TEXT("Failed to close the video file handle."));
                                }

                                // depending on the length of the clip burst captured above, it's possible that the size may stay the same
                                // from one clip to another, but in general it should be increasing.  if the file got smaller then
                                // we're definatly overwriting instead of appending.
                                if(fd.nFileSizeLow < dwPreviousIterationFileSize)
                                {
                                    FAIL(TEXT("File size did not grow from one iteration to the next"));
                                    Log(TEXT("File size before is %d, current file size is %d"), dwPreviousIterationFileSize, fd.nFileSizeLow);
                                }

                                // set the size we just retrieved for the next iterations check.
                                dwPreviousIterationFileSize = fd.nFileSizeLow;

                                // start the next burst.
                                if(FAILED(hr = pCaptureGraph->StartStreamCapture())  && hr != E_WRITE_ERROR_EVENT && hr != E_OOM_EVENT)
                                {
                                    if(hr != E_NO_FRAMES_PROCESSED)
                                        FAIL(TEXT("Failed to start the stream capture."));
                                    else Log(TEXT("Insufficient memory to process even one frame of the capture."));
                                }
                            }

                            Log(TEXT("  Capture complete"));

                            // make sure we properly handle the case where we ran out of memory while doing the test.
                            hr = pCaptureGraph->StopStreamCapture();
                            if(FAILED(hr))
                            {
                                if(hr == E_WRITE_ERROR_EVENT)
                                    Log(TEXT("Received a write error event while doing the capture (out of storage space?)."));
                                else if(hr == E_OOM_EVENT)
                                    Log(TEXT("Received an OOM event while doing the capture (insufficient memory for this capture length?)."));
                                else
                                    FAIL(TEXT("Failed to stop the stream capture."));
                            }

                            Log(TEXT("  Capture stream stopped"));

                            if(FAILED(pCaptureGraph->StopGraph()))
                            {
                                FAIL(TEXT("Failed to stop the graph after capture"));
                            }

                            if(nCaptureLength > 0 && FAILED(pCaptureGraph->VerifyVideoFileCaptured(tszFileName)))
                            {
                                FAIL(TEXT("The video file captured failed file conformance verification"));
                            }

                            // verify that the file was created
                            hFile = FindFirstFile(tszFileName, &fd);
                            if(hFile == INVALID_HANDLE_VALUE)
                            {
                                if(nCaptureLength > 0)
                                    FAIL(TEXT("Failed to find the video file."));

                                Log(TEXT("Looking for file %s, not found."), tszFileName);
                            }
                            else if(FALSE == FindClose(hFile))
                            {
                                FAIL(TEXT("Failed to close the video file handle."));
                            }

#ifndef SAVE_CAPTURE_FILES
                            if(FALSE == DeleteFile(tszFileName))
                            {
                                if(nCaptureLength > 0)
                                    FAIL(TEXT("Failed to delete the video file."));
                            }
#endif
                        }
                    }
                }
            }
            if(FAILED(pCaptureGraph->Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
            }
        }

        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}


TESTPROCAPI VerifyASFWriting_TimedCapture( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    int nCaptureLength;
    int anTestCombinations[][3] = { 
                                                        {75, 10000, 2}, // defaults
                                                        {0, 10000, 2},
                                                        {50, 10000, 2},
                                                        {100, 10000, 2},
                                                        {75, 0, 2},
                                                        {75, 5000, 2},
                                                        {75, 15000, 2},
                                                        {75, 10000, 0},
                                                        {75, 10000, 1},
                                                        {75, 10000, 2}
                                                    };

    // diagonalize the graph some.  If we're running with all of the components, then
    // test all of the encoding parameters, otherwise just test with the default
    int nCombinationCount = lpFTE->dwUserData == ALL_COMPONENTS?10:1;
    int nCurrentCombination;
    TCHAR tszFileName[MAX_PATH];
     int nStringLength = MAX_PATH;
    TCHAR tszBaseFileName[] = TEXT("%s\\StartStop%d");
    TCHAR tszCompletor[] = TEXT("File%d.asf");
#ifdef SAVE_CAPTURE_FILES
    TCHAR tszExtraPath[] = TEXT("\\release");
#else
    TCHAR tszExtraPath[] = TEXT("\0");
#endif
    HRESULT hr = S_OK;
    VARIANT Var;
    int nStartLength = 0;
    int nTotalCaptureLength = 10000;
    int nShortLength = 4000;
    int nShortIncrement = 1000;
    int nMainIncrement = 3000;

    VariantInit(&Var);

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {

            for(nCurrentCombination = 0; nCurrentCombination < nCombinationCount; nCurrentCombination++)
            {
                Log(TEXT("Testing encoder quality combination %d"), nCurrentCombination);

                if(FAILED(pCaptureGraph->InitCore(hwnd, &rc, dwGraphLayout)))
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                }
                else
                {
                    // If retrieving a property succeeds, then setting it should succeed also.
                    // if we can't retrieve it then we ignore it.
                    if(SUCCEEDED(hr = pCaptureGraph->GetVideoEncoderInfo(g_wszWMVCVBRQuality, &Var)))
                    {
                        VariantClear(&Var);
                        Var.vt = VT_I4;
                        Var.lVal = (LONG) anTestCombinations[nCurrentCombination][0];

                        hr = pCaptureGraph->SetVideoEncoderProperty(g_wszWMVCVBRQuality, &Var);
                        if(FAILED(hr))
                        {
                            FAIL(TEXT("Failed to set encoder quality property."));
                        }
                    }

                    if(SUCCEEDED(hr = pCaptureGraph->GetVideoEncoderInfo(g_wszWMVCKeyframeDistance, &Var)))
                    {
                        VariantClear(&Var);
                        Var.vt = VT_I4;
                        Var.lVal = (LONG) anTestCombinations[nCurrentCombination][1];
                        hr = pCaptureGraph->SetVideoEncoderProperty(g_wszWMVCKeyframeDistance, &Var);
                        if(FAILED(hr))
                        {
                            FAIL(TEXT("Failed to set encoder key frame property."));
                        }
                    }

                    if(SUCCEEDED(hr = pCaptureGraph->GetVideoEncoderInfo(g_wszWMVCComplexityEx, &Var)))
                    {
                        VariantClear(&Var);
                        Var.vt = VT_I4;
                        Var.lVal = (LONG) anTestCombinations[nCurrentCombination][2];
                        hr = pCaptureGraph->SetVideoEncoderProperty(g_wszWMVCComplexityEx, &Var);
                        if(FAILED(hr))
                        {
                            FAIL(TEXT("Failed to set encoder complexity property."));
                        }
                    }

                    if(FAILED(hr = pCaptureGraph->FinalizeGraph()))
                    {
                        if(hr == E_GRAPH_UNSUPPORTED)
                        {
                            DETAIL(TEXT("Initializing the capture graph failed as expected."));
                        }
                        else
                        {
                            FAIL(TEXT("Initializing the capture graph failed."));
                        }
                    }
                    else
                    {
                        _stprintf(tszFileName, tszBaseFileName, tszExtraPath, nCurrentCombination);
                        _tcscat(tszFileName, tszCompletor);

                        if(FAILED(pCaptureGraph->SetVideoCaptureFileName(tszFileName)))
                        {
                            FAIL(TEXT("Failed to set the capture filename."));
                        }

                        for(nCaptureLength = nStartLength; nCaptureLength < nTotalCaptureLength; nCaptureLength<nShortLength?nCaptureLength+=nShortIncrement:nCaptureLength += nMainIncrement)
                        {
                            if(FAILED(pCaptureGraph->RunGraph()))
                            {
                                FAIL(TEXT("Starting the capture graph failed."));
                            }
                            else
                            {
                                HANDLE hFile = NULL;
                                WIN32_FIND_DATA fd;

                                Log(TEXT("Capture length %d seconds"), nCaptureLength/1000);

                                // loop through a few times verifying the capture was correct.
                                hr = pCaptureGraph->TimedStreamCapture(nCaptureLength);
                                if(FAILED(hr) && hr != E_WRITE_ERROR_EVENT && hr != E_OOM_EVENT)
                                {
                                    if(hr != E_NO_FRAMES_PROCESSED)
                                        FAIL(TEXT("The stream capture Failed."));
                                    else Log(TEXT("Insufficient memory to process even one frame of the capture."));
                                }
                                else
                                {
                                    if(FAILED(pCaptureGraph->StopGraph()))
                                    {
                                        FAIL(TEXT("Failed to stop the stream capture"));
                                    }

                                    // reset the file name string length
                                    nStringLength = MAX_PATH;
                                    if(FAILED(pCaptureGraph->GetVideoCaptureFileName(tszFileName, &nStringLength)))
                                    {
                                        FAIL(TEXT("Failed to retrieve the video capture file name."));
                                    }

                                    if(nCaptureLength > 0 && FAILED(pCaptureGraph->VerifyVideoFileCaptured(tszFileName)))
                                    {
                                        FAIL(TEXT("The video file captured failed file conformance verification"));
                                    }

                                    // verify that the file was created
                                    hFile = FindFirstFile(tszFileName, &fd);
                                    if(hFile == INVALID_HANDLE_VALUE)
                                    {
                                        if(nCaptureLength > 0)
                                            FAIL(TEXT("Failed to find the video file."));

                                        Log(TEXT("Looking for file %s, not found."), tszFileName);
                                    }
                                    else if(FALSE == FindClose(hFile))
                                    {
                                        FAIL(TEXT("Failed to close the video file handle."));
                                    }

#ifndef SAVE_CAPTURE_FILES
                                    if(FALSE == DeleteFile(tszFileName))
                                    {
                                        if(nCaptureLength > 0)
                                            FAIL(TEXT("Failed to delete the video image file."));
                                    }
#endif
                                }
                            }
                        }
                    }
                }
                if(FAILED(pCaptureGraph->Cleanup()))
                {
                    FAIL(TEXT("Cleaning up the graph failed."));
                }
            }
        }

        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI VerifyASFWriting_TimedContinuedCapture( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;
    int nCaptureLength;


    TCHAR tszFileName[MAX_PATH];
     int nStringLength = MAX_PATH;
    TCHAR tszBaseFileName[] = TEXT("%s\\StartStopContinued");
    TCHAR tszCompletor[] = TEXT("File%d.asf");
#ifdef SAVE_CAPTURE_FILES
    TCHAR tszExtraPath[] = TEXT("\\release");
#else
    TCHAR tszExtraPath[] = TEXT("\0");
#endif
    HRESULT hr = S_OK;
    VARIANT Var;
    int nStartLength = 0;
    int nTotalCaptureLength = 20000;
    int nShortLength = 4000;
    int nShortIncrement = 1000;
    int nMainIncrement = 3000;

    VariantInit(&Var);

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            if(FAILED(pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                FAIL(TEXT("Initializing the capture graph failed."));
            }
            else
            {
                _stprintf(tszFileName, tszBaseFileName, tszExtraPath);
                _tcscat(tszFileName, tszCompletor);

                if(FAILED(pCaptureGraph->SetVideoCaptureFileName(tszFileName)))
                {
                    FAIL(TEXT("Failed to set the capture filename."));
                }

                for(nCaptureLength = nStartLength; nCaptureLength < nTotalCaptureLength; nCaptureLength<nShortLength?nCaptureLength+=nShortIncrement:nCaptureLength += nMainIncrement)
                {
                    if(FAILED(pCaptureGraph->RunGraph()))
                    {
                        FAIL(TEXT("Starting the capture graph failed."));
                    }
                    else
                    {
                        HANDLE hFile = NULL;
                        WIN32_FIND_DATA fd;
                        DWORD dwPreviousIterationFileSize = 0;

                        // reset the file name string length
                        nStringLength = MAX_PATH;
                        if(FAILED(pCaptureGraph->GetVideoCaptureFileName(tszFileName, &nStringLength)))
                        {
                            FAIL(TEXT("Failed to retrieve the video capture file name."));
                        }

                        Log(TEXT("Capture length %d seconds"), nCaptureLength/1000);

                        for(int i = 0; i < 10; i++)
                        {
                            // do a 1/20th of the total clip capture, then delay an amount of time and do it again.
                            hr = pCaptureGraph->TimedStreamCapture(nCaptureLength/20);
                            if(FAILED(hr) && hr != E_WRITE_ERROR_EVENT && hr != E_OOM_EVENT)
                            {
                                if(hr != E_NO_FRAMES_PROCESSED)
                                    FAIL(TEXT("The stream capture Failed."));
                                else Log(TEXT("Insufficient memory to process even one frame of the capture."));
                            }

                            // verify that the file was created, do the verification while the capture is paused.
                            hFile = FindFirstFile(tszFileName, &fd);
                            if(hFile == INVALID_HANDLE_VALUE)
                            {
                                FAIL(TEXT("Failed to find the video file."));
                            }
                            else if(FALSE == FindClose(hFile))
                            {
                                FAIL(TEXT("Failed to close the video file handle."));
                            }

                            if(fd.nFileSizeLow < dwPreviousIterationFileSize)
                            {
                                FAIL(TEXT("File size did not grow from one iteration to the next"));
                                Log(TEXT("File size before is %d, current file size is %d"), dwPreviousIterationFileSize, fd.nFileSizeLow);
                            }

                            // set the size we just retrieved for the next iterations check.
                            dwPreviousIterationFileSize = fd.nFileSizeLow;

                            Sleep(nCaptureLength / 20);
                        }

                        if(FAILED(pCaptureGraph->StopGraph()))
                        {
                            FAIL(TEXT("Failed to stop the stream capture"));
                        }

                        if(nCaptureLength > 0 && FAILED(pCaptureGraph->VerifyVideoFileCaptured(tszFileName)))
                        {
                            FAIL(TEXT("The video file captured failed file conformance verification"));
                        }

                        // verify that the file was created
                        hFile = FindFirstFile(tszFileName, &fd);
                        if(hFile == INVALID_HANDLE_VALUE)
                        {
                            if(nCaptureLength > 0)
                                FAIL(TEXT("Failed to find the video file."));

                            Log(TEXT("Looking for file %s, not found."), tszFileName);
                        }
                        else if(FALSE == FindClose(hFile))
                        {
                            FAIL(TEXT("Failed to close the video file handle."));
                        }

#ifndef SAVE_CAPTURE_FILES
                        if(FALSE == DeleteFile(tszFileName))
                        {
                            if(nCaptureLength > 0)
                                FAIL(TEXT("Failed to delete the video file."));
                        }
#endif
                    }
                }
            }
            if(FAILED(pCaptureGraph->Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
            }
        }

        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}


TESTPROCAPI FindInterfaceTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    DWORD dwGraphLayout = lpFTE->dwUserData;

    hwnd = GetWindow();

    HRESULT hr = S_OK;
    CComPtr<ICaptureGraphBuilder2> pCaptureGraphBuilder = NULL;
    CComPtr<IGraphBuilder> pFilterGraph = NULL;
    CComPtr<IBaseFilter> pVideoCaptureFilter = NULL;
    CComPtr<IBaseFilter> pAudioCaptureFilter = NULL;
    CComPtr<IBaseFilter> pWM9Encoder = NULL;
    CComPtr<IBaseFilter> pAsfMux = NULL;
    CComPtr<IBaseFilter> pVideoRenderer = NULL;
    CComPtr<IVideoWindow> pVideoWindowFromGraph = NULL;
    CComPtr<IWMHeaderInfo3> pWMHeaderInfo3FromGraph = NULL;
    CComPtr<IWMHeaderInfo3> pWMHeaderInfo3FromGraph2 = NULL;
    CComPtr<IAMResourceControl> pAudioResourceControlFromGraph = NULL;
    CComPtr<IAMStreamConfig> pStreamConfigFromGraph = NULL;
    CComPtr<IGraphBuilder> pRetrievedFilterGraph = NULL;
    CComPtr<IBaseFilter> pBaseFilter = NULL;

    if(hwnd)
    {
        Log(TEXT("Building the capture graph"));

        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {
            if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                if(hr == E_GRAPH_UNSUPPORTED)
                {
                    DETAIL(TEXT("Initializing the capture graph failed as expected."));
                }
                else
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                }
                goto cleanup;
            }
            else
            {

                if(FAILED(pCaptureGraph->RunGraph()))
                {
                    FAIL(TEXT("Running the capture graph failed."));
                }
                else
                {
                    // let the graph run for a little bit.
                    Sleep(200);

                    if (FAILED(pCaptureGraph->GetBuilderInterface((void**)&pCaptureGraphBuilder, IID_ICaptureGraphBuilder2)))
                    {
                        FAIL(TEXT("Capture graph returned NULL for the capture graph builder interface."));
                        goto cleanup;
                    }

                    if (FAILED(pCaptureGraph->GetBuilderInterface((void**)&pFilterGraph, IID_IGraphBuilder)))
                    {
                        FAIL(TEXT("Capture graph returned NULL for the builder interface."));
                        goto cleanup;
                    }

                    // First look for the video capture filter and the audio capture as these would be really upstream
                    if (dwGraphLayout & VIDEO_CAPTURE_FILTER)
                    {
                        if(FAILED(pCaptureGraph->GetBaseFilter((void**)&pVideoCaptureFilter, VIDEO_CAPTURE_FILTER)))
                        {
                            FAIL(TEXT("Unable to retrieve capture filter interface."));
                            goto cleanup;
                        }
                    }

                    if (dwGraphLayout & AUDIO_CAPTURE_FILTER)
                    {
                        if(FAILED(pCaptureGraph->GetBaseFilter((void**)&pAudioCaptureFilter, AUDIO_CAPTURE_FILTER)))
                        {
                            FAIL(TEXT("Unable to retrieve capture filter interface."));
                            goto cleanup;
                        }
                    }

                    // Then look for the ASF writer and Video Renderer for something really downstream - the asf mux
                    if (dwGraphLayout & FILE_WRITER)
                    {
                        if(FAILED(pCaptureGraph->GetBaseFilter((void**)&pAsfMux, FILE_WRITER)))
                        {
                            FAIL(TEXT("Unable to retrieve capture filter interface."));
                            goto cleanup;
                        }
                    }

                    if (dwGraphLayout & VIDEO_RENDERER)
                    {
                        if(FAILED(pCaptureGraph->GetBaseFilter((void**)&pVideoRenderer, VIDEO_RENDERER)))
                        {
                            FAIL(TEXT("Unable to retrieve capture filter interface."));
                            goto cleanup;
                        }
                    }

                    // Then look for the encoder for something in the middle
                    if (dwGraphLayout & VIDEO_ENCODER)
                    {
                        if(FAILED(pCaptureGraph->GetBaseFilter((void**)&pWM9Encoder, VIDEO_ENCODER)))
                        {
                            FAIL(TEXT("Unable to retrieve capture filter interface."));
                            goto cleanup;
                        }
                    }
                    // Test Case: verify the filtergraph returned by the graph builder is correct
                    if(pFilterGraph && pCaptureGraphBuilder)
                    {
                        if (FAILED(pCaptureGraphBuilder->GetFiltergraph(&pRetrievedFilterGraph)))
                        {
                            FAIL(TEXT("Retrieving the filtergraph from the graph builder failed."));
                            goto cleanup;
                        }

                        if(pFilterGraph != pRetrievedFilterGraph)
                        {
                            FAIL(TEXT("The filtergraph retrieved from the graph builder was incorrect."));
                            goto cleanup;
                        }
                        pRetrievedFilterGraph = NULL;
                    }

                    // Test Case: look for interface: IBasicVideo in a specifc direction: Downstream for a specified Mediatype: Video from a specified filter: Video Capture
                    // ASSUMPTION: Only the video renderer supports an IVideoWindow on the graph, thus we always receive the video renderer as the base filter
                    // NOTE: this test is only valid if we co-created the video renderer ourselves.  If the capture graph builder created it then our internal pointer is a proxy
                    // video renderer for us and not the real video renderer.
                    if (pVideoCaptureFilter && !(dwGraphLayout & VIDEO_RENDERER_INTELI_CONNECT))
                    {
                        // First specify a downstream filter
                        DETAIL(TEXT("FindInterface: looking downstream from video capture filter for IBasicVideo interface by specifying a video stream"));
                        hr = pCaptureGraphBuilder->FindInterface(&LOOK_DOWNSTREAM_ONLY, &MEDIATYPE_Video, pVideoCaptureFilter, IID_IVideoWindow, (void**)&pVideoWindowFromGraph);
                        if (FAILED(hr) && (hr != E_NOINTERFACE))
                        {
                            FAIL(TEXT("FindInterface returned an error"));
                            goto cleanup;
                        }
                        else if ((hr == E_NOINTERFACE) && pVideoRenderer)
                        {
                            FAIL(TEXT("FindInterface returned E_NOINTERFACE when there was a Video Renderer interface"));
                            goto cleanup;
                        }
                        else 
                        {
                            // Verify that the interfaces match
                            DETAIL(TEXT("Querying the IBasicVideo interface for the base filter"));
                            if (pVideoWindowFromGraph)
                                pVideoWindowFromGraph->QueryInterface(IID_IBaseFilter, (void**)&pBaseFilter);
                            // If we got the filter, we have released it but we just need the pointer for comparison now
                            if (pBaseFilter != pVideoRenderer)
                            {
                                FAIL(TEXT("FindInterface returned a different IBasicVideo interface than the one supported by the video renderer.\n"));
                                goto cleanup;
                            }
                            else
                            {
                                DETAIL(TEXT("FindInterface returned the same base filter interface for IBasicVideo and the video renderer.\n"));
                            }
                            pBaseFilter = NULL;
                            pVideoWindowFromGraph = NULL;
                        }
                    }

                    // Test Case: look for interface: IAMResourceControl in a specific direction: Upstream for a specified Mediatype: Audio from a specified filter: ASF Mux
                    if (pAsfMux)
                    {
                        DETAIL(TEXT("FindInterface: looking upstream from asf mux capture filter for the audio capture's IAMResourceControl interface by specifying an audio stream"));
                        hr = pCaptureGraphBuilder->FindInterface(&LOOK_UPSTREAM_ONLY, &MEDIATYPE_Audio, pAsfMux, IID_IAMResourceControl, (void**)&pAudioResourceControlFromGraph);
                        if (FAILED(hr) && (hr != E_NOINTERFACE))
                        {
                            FAIL(TEXT("FindInterface returned an error"));
                            goto cleanup;
                        }
                        else if ((hr == E_NOINTERFACE) && pAudioCaptureFilter)
                        {
                            FAIL(TEXT("FindInterface returned E_NOINTERFACE when there was an Audio Capture interface"));
                            goto cleanup;
                        }
                        else 
                        {
                            // Verify that the interfaces match
                            DETAIL(TEXT("Querying the IAMResourceControl interface for the base filter"));
                            if (pAudioResourceControlFromGraph)
                                pAudioResourceControlFromGraph->QueryInterface(IID_IBaseFilter, (void**)&pBaseFilter);
                            // If we got the filter we have released it but we just need the pointer for comparison now
                            if (pBaseFilter != pAudioCaptureFilter)
                            {
                                FAIL(TEXT("FindInterface returned a different IAMResourceControl interface than the one supported by the audio capture filter.\n"));
                                goto cleanup;
                            }
                            else
                            {
                                DETAIL(TEXT("FindInterface returned the same base filter interface for IAMResourceControl and the audio capture filter.\n"));
                            }
                            pBaseFilter = NULL;
                            pAudioResourceControlFromGraph = NULL;
                        }
                    }

                    // Now we find the video capture filter using the IAM_StreamControl
                    if (pVideoCaptureFilter)
                    {
                        DETAIL(TEXT("FindInterface: looking from video capture filter for iamstreamconfig by specifying PIN_CATEGORY_PREVIEW"));

                        hr = pCaptureGraphBuilder->FindInterface(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, pVideoCaptureFilter, IID_IAMStreamConfig, (void **) &pStreamConfigFromGraph);
                        if (FAILED(hr) && (hr != E_NOINTERFACE))
                        {
                            FAIL(TEXT("FindInterface returned an error"));
                            goto cleanup;
                        }
                        // this can fail with E_NOINTERFACE in the case of a two pin driver.

                        // unable to do any further verification, as there's no way to get a base filter from this type of interface.
                        pStreamConfigFromGraph = NULL;
                    }
                }
            }
        }
    }

cleanup:

    pCaptureGraphBuilder = NULL;
    pFilterGraph = NULL;
    pVideoCaptureFilter = NULL;
    pAudioCaptureFilter = NULL;
    pWM9Encoder = NULL;
    pAsfMux = NULL;
    pVideoRenderer = NULL;
    pVideoWindowFromGraph = NULL;
    pWMHeaderInfo3FromGraph = NULL;
    pWMHeaderInfo3FromGraph2 = NULL;
    pAudioResourceControlFromGraph = NULL;
    pStreamConfigFromGraph = NULL;
    pRetrievedFilterGraph = NULL;
    pBaseFilter = NULL;

    if (pCaptureGraph)
        pCaptureGraph->Cleanup();

    if (hwnd)
        ReleaseWindow(hwnd);

    return GetTestResult();
}

TESTPROCAPI TestProperties(DWORD dwGraphLayout, int nPropertySet, long lProperty)
{

    PCAPTUREFRAMEWORK pCaptureGraph = NULL;
    HWND hwnd;
    RECT rc = {0, 0, 0, 0};
    HRESULT hr = S_OK;

    hwnd = GetWindow();

    if(hwnd)
    {
        // retrieve the global graph pointer
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);

        if(pCaptureGraph)
        {

            if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                if(hr == E_GRAPH_UNSUPPORTED)
                {
                    DETAIL(TEXT("Initializing the capture graph failed as expected."));
                }
                else
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                }
            }
            else
            {
                if(FAILED(pCaptureGraph->RunGraph()))
                {
                    FAIL(TEXT("Running the capture graph failed."));
                }
                else
                {
                    long lMin, lMax, lStepping, lDefault, lFlagRange;
                    long lValue, lFlags;

                    // If we're unable to retrieve the property range, then fail or skip the test.
                    if(SUCCEEDED(hr = pCaptureGraph->GetRange(nPropertySet, lProperty, &lMin, &lMax, &lStepping, &lDefault, &lFlagRange)))
                    {
                        Log(TEXT("Succeeded in retrieving range, min %d, max %d, stepping %d, default %d, flag range %d"), lMin, lMax, lStepping, lDefault, lFlagRange);

                        Log(TEXT("Verifying that the range values are consistent with themselves."));
                        // Test 0, general consistency.  Is the values returned consistent with themselves?
                        if(lMin > lMax)
                        {
                            FAIL(TEXT("The minimum is greater than the maximum."));
                            Log(TEXT("    Values found were minimum %d, maximum %d."), lMin, lMax);
                        }

                        if(lDefault > lMax || lDefault < lMin || lDefault % lStepping != 0)
                        {
                            FAIL(TEXT("The default value is outside of the minimum/maximum range or not within stepping."));
                            Log(TEXT("     Default value detected was %d."), lDefault);
                        }

                        // current known ranges are videoprocamp_flags_auto, videoprocamp_flags_manual, cameracontrol_flags_auto, 
                        // and cameracontrol_flags_manual.  The videoprocamp and cameracontrolflag values are equal for auto and manual,
                        // so the known flags are 1 and 2.  This a major assumption on behalf of the test, and could change in the future, however
                        // for now it's correct.
                        if(lFlagRange == 0 || (lFlagRange | (Flags_Auto | Flags_Manual)) != (Flags_Auto | Flags_Manual))
                        {
                            FAIL(TEXT("The flags returned from GetRange isn't valid."));
                            Log(TEXT("    The known flags are 1 and 2, the flags returned was 0x%0x8."), lFlagRange);
                        }

                        // Test 1, get the current value and verify it's valid (it may be different from the default value if the test ran previously)
                        Log(TEXT("Verifying the current value returned."));

                        if(FAILED(pCaptureGraph->Get(nPropertySet, lProperty, &lValue, &lFlags)))
                        {
                            FAIL(TEXT("Unable to retrieve the current property value."));
                        }

                        if(lValue > lMax || lValue < lMin || lValue % lStepping != 0)
                        {
                            FAIL(TEXT("The current value value is outside of the minimum/maximum range or disobeys stepping."));
                            Log(TEXT("    The value found was %d."), lValue);
                        }

                        // if the current flag isn't a known flag, then we have a problem.
                        if(!(lFlagRange & lFlags))
                        {
                            FAIL(TEXT("Flag value retrieved isn't a known flag value."));
                            Log(TEXT("    Current flag detected is 0x%08x, flag range is 0x%08x"), lFlags, lFlagRange);
                        }

                        // test 2, verify that we can set the current value, it's currently set so it must be valid.
                        Log(TEXT("Verifying the settability of the current value."));

                        if(FAILED(pCaptureGraph->Set(nPropertySet, lProperty, lValue, lFlags)))
                        {
                            FAIL(TEXT("Failed to set the current value back to the driver"));
                            Log(TEXT("    Value attempted to set was %d"), lValue);
                        }

                        // test 3, verify that we can set the default value, it's the default, so it must be settable.
                        Log(TEXT("Verifying the settability of the current value."));

                        // if the manual flag is supported, and we failed to set the default value, then we have a problem.
                        if((lFlagRange & Flags_Manual) && FAILED(pCaptureGraph->Set(nPropertySet, lProperty, lDefault, Flags_Manual)))
                        {
                            FAIL(TEXT("Failed to set the default value to the driver."));
                            Log(TEXT("    Value attempted to set was %d"), lDefault);
                        }

                        // test 3, let's verify that we can set the all of the values that the driver claims that it can set for this property.
                        Log(TEXT("Verifying the settability of the values supported."));

                        for(LONG lSetFlags = Flags_Auto; lSetFlags <= Flags_Manual; lSetFlags++)
                        {
                            // if this flag isn't supported, then we can't test it.
                            if(!(lFlagRange & lSetFlags))
                            {
                                Log(TEXT("Flag %d unsupported by the driver, skipping a portion of this test."), lSetFlags);
                                continue;
                            }

                            for(LONG lSetValue = lMin; lSetValue <= lMax; lSetValue+=lStepping)
                            {
                                // as the driver reported that this value is in the range and stepping, verify that we can set it.
                                if(FAILED(pCaptureGraph->Set(nPropertySet, lProperty, lSetValue, lSetFlags)))
                                {
                                    FAIL(TEXT("Failed to set a valid property value"));
                                    Log(TEXT("    Value attempted to set was %d, with a flag value of 0x%08x"), lSetValue, lSetFlags);
                                    break;
                                }

                                if(FAILED(pCaptureGraph->Get(nPropertySet, lProperty, &lValue, &lFlags)))
                                {
                                    FAIL(TEXT("Unable to retrieve the current property value that was just set."));
                                    break;
                                }

                                if((lFlags == Flags_Manual && lSetValue != lValue) || lSetFlags != lFlags)
                                {
                                    FAIL(TEXT("The value returned is not the same as the value just set."));
                                    Log(TEXT("    The value expected was %d, the actual value was %d."), lSetValue, lValue);
                                    Log(TEXT("    The flag expected was %d, the actual flag was %d."), lSetFlags, lFlags);
                                    break;
                                }
                            }

                            // if we're testing the auto flag, then make sure that the set value is ignored.
                            if(lSetFlags == Flags_Auto)
                            {

                                if(FAILED(pCaptureGraph->Set(nPropertySet, lProperty, lMin - 1, lSetFlags)))
                                {
                                    FAIL(TEXT("Failed to set an invalid small value when setting to auto."));
                                    Log(TEXT("    Value attempted to set was %d, with a flag value of 0x%08x"), lValue, lFlags);
                                }

                                if(FAILED(pCaptureGraph->Set(nPropertySet, lProperty, lMax + 1, lSetFlags)))
                                {
                                    FAIL(TEXT("Failed to set an invalid value large when setting to auto."));
                                    Log(TEXT("    Value attempted to set was %d, with a flag value of 0x%08x"), lValue, lFlags);
                                }
                            }
                            // we're testing the manual flag
                            else if(lSetFlags == Flags_Manual)
                            {
                                if(SUCCEEDED(pCaptureGraph->Set(nPropertySet, lProperty, lMax + lStepping, lSetFlags)))
                                {
                                    FAIL(TEXT("Succeed in setting a value greater than the maximum value."));
                                    Log(TEXT("    Value attempted to set was %d, the maximum is %d"), lValue, lMax);
                                }

                                if(SUCCEEDED(pCaptureGraph->Set(nPropertySet, lProperty, lMin - lStepping, lSetFlags)))
                                {
                                    FAIL(TEXT("Succeed in setting a value less than the minimum value."));
                                    Log(TEXT("    Value attempted to set was %d, the maximum is %d"), lValue, lMax);
                                }

                                // verify that disobeying the stepping fails correctly
                                if(lStepping != 1)
                                {
                                    for(int i = 1; i < lStepping; i++)
                                    {
                                        if(SUCCEEDED(pCaptureGraph->Set(nPropertySet, lProperty, lDefault + i, lSetFlags)))
                                        {
                                            FAIL(TEXT("Succeed in setting a value which disobeys stepping requirements."));
                                            Log(TEXT("    Value attempted to set was %d, the stepping is %d, stepping attemped is %d"), lValue, lStepping, i);
                                        }
                                    }
                                }
                            }
                        }

                        // if we can only do auto or manual, make sure the other fails properly.
                        if(lFlagRange == Flags_Auto)
                        {
                            if(SUCCEEDED(pCaptureGraph->Set(nPropertySet, lProperty, lDefault, Flags_Manual)))
                            {
                                FAIL(TEXT("Succeed in setting the property using a manual flag when only auto was reported as supported."));
                            }
                        }
                        else if(lFlagRange == Flags_Manual)
                        {
                            if(SUCCEEDED(pCaptureGraph->Set(nPropertySet, lProperty, lDefault, Flags_Auto)))
                            {
                                FAIL(TEXT("Succeed in setting the property using an auto flag when only manual was reported as supported."));
                            }
                        }

                        // test 4, check out some out of range values for these functions.
                        Log(TEXT("Verifying the settability of values not supported."));

                        if(SUCCEEDED(pCaptureGraph->Set(nPropertySet, lProperty, lDefault, 0)))
                        {
                            FAIL(TEXT("Succeed in setting the default value with an invalid flag of 0."));
                        }

                        if(SUCCEEDED(pCaptureGraph->Set(nPropertySet, lProperty, lDefault, 3)))
                        {
                            FAIL(TEXT("Succeed in setting the default value with an invalid flag of 3."));
                        }

                        if(SUCCEEDED(pCaptureGraph->Set(nPropertySet, lProperty, lDefault, 4)))
                        {
                            FAIL(TEXT("Succeed in setting the default value with an invalid flag of 4."));
                        }

                        if(SUCCEEDED(pCaptureGraph->Set(nPropertySet, lProperty, lDefault, -1)))
                        {
                            FAIL(TEXT("Succeed in setting the default value with an invalid flag of -1."));
                        }

                        // test 5, check out some invalid properties within this set
                        Log(TEXT("Verifying the usability of invalid property sets."));

                        if(SUCCEEDED(pCaptureGraph->GetRange(nPropertySet, 10, &lMin, &lMax, &lStepping, &lDefault, &lFlagRange)))
                        {
                            FAIL(TEXT("Succeeded in retrieving the range for property set 10, which isn't a valid property id."));
                        }

                        // a property set of 10 is VideoProcAmpGain + 1.
                        if(SUCCEEDED(pCaptureGraph->Set(nPropertySet, 10, lMax + lStepping, lFlags)))
                        {
                            FAIL(TEXT("Succeeded in setting a value for property set 10, which isn't a valid property id."));
                        }

                        if(SUCCEEDED(pCaptureGraph->Get(nPropertySet, 10, &lValue, &lFlags)))
                        {
                            FAIL(TEXT("Succeeded in getting the value for property set 10, which isn't a valid property id."));
                        }
                    }
                    else if(hr == E_PROP_ID_UNSUPPORTED)
                    {
                        SKIP(TEXT("Property unsupported, skipping test"));
                    }
                    else
                    {
                        FAIL(TEXT("Running the capture graph failed."));
                    }
                }
            }
            if(FAILED(pCaptureGraph->Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
            }
        }

        ReleaseWindow(hwnd);
    }

    return GetTestResult();
}

TESTPROCAPI TestCamControlProperties( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    return TestProperties(ALL_COMPONENTS, PROPERTY_CAMERACONTROL, lpFTE->dwUserData);
}

TESTPROCAPI TestVidProcAmpProperties( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    return TestProperties(ALL_COMPONENTS, PROPERTY_VIDPROCAMP, lpFTE->dwUserData);
}

#pragma optimize("",on)

