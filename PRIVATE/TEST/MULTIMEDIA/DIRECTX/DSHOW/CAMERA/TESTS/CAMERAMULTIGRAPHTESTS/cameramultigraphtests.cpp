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
#include "cameramultigraphtests.h"

CAPTUREFRAMEWORK g_CaptureGraph;
PLAYBACKFRAMEWORK g_PlaybackGraph;

int g_nAudioDeviceCount;

// Thread 0 must be the message pumping thread, as it creates the windows.
struct sTestCaseData g_sTestCaseData[] =
                                                    {
                                                        {MessagePump, 0, 0, TEXT(""), NULL},
                                                        {CaptureGraph, ALL_COMPONENTS, 3000, TEXT("CaptureFile.asf"), NULL},
                                                        {PlaybackGraph, ALL_COMPONENTS, 1000, TEXT("PlaybackFile1.asf"), NULL},
                                                        {PlaybackGraph, ALL_COMPONENTS, 3000, TEXT("PlaybackFile2.asf"), NULL},
                                                        {PlaybackGraph, ALL_COMPONENTS, 6000, TEXT("PlaybackFile3.asf"), NULL},
                                                        {PlaybackGraph, ALL_COMPONENTS, 9000, TEXT("PlaybackFile4.asf"), NULL},
                                                    };

struct sTestScenarioData g_sScenarioTestData[] =
                                                                                {
                                                                                    {5, {&g_sTestCaseData[0], &g_sTestCaseData[1], &g_sTestCaseData[2], &g_sTestCaseData[3], &g_sTestCaseData[4]}},
                                                                                    {4, {&g_sTestCaseData[1], &g_sTestCaseData[2], &g_sTestCaseData[3], &g_sTestCaseData[4]}}
                                                                                };


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

PPLAYBACKFRAMEWORK GetPlaybackGraph()
{
    return &g_PlaybackGraph;
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

HWND GetWindow(HWND hwndParent, int left, int top, int width, int height)
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
                                        WS_VISIBLE | WS_BORDER | (hwndParent?WS_CHILD:0),
                                        left, top,
                                        width, height,
                                        hwndParent, NULL, globalInst, NULL);

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

TESTPROCAPI CapturePlaybackTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PPLAYBACKFRAMEWORK pPlaybackGraph = NULL;
    PCAPTUREFRAMEWORK pCaptureGraph = NULL;

    HWND hwnd;
    RECT rc;
    DWORD dwGraphLayout = lpFTE->dwUserData;
    HRESULT hr = S_OK;
    long lEventCode = 0;
    TCHAR tszBaseFileName[] = TEXT("%s\\CapturePlaybackTest-%d-");
    TCHAR tszCompletor[] = TEXT("%d.asf");
#ifdef SAVE_CAPTURE_FILES
    TCHAR tszExtraPath[] = TEXT("\\release");
#else
    TCHAR tszExtraPath [] = TEXT("\0");
#endif
    TCHAR tszFileName[MAX_PATH];
    int nStringLength = MAX_PATH;
    int nCaptureLength = 6000;
    int nTickCountStart = 0;
    int nTickCountElapsed = 0;
    int nVarianceAllowed = 2000;
    HANDLE hFile = NULL;
    WIN32_FIND_DATA fd;

    hwnd = GetWindow(NULL, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT);

    if(hwnd)
    {
        if(!GetClientRect(hwnd, &rc))
        {
            FAIL(TEXT("Failed to retrieve the client rectangle for the window."));
        }

        // retrieve the global graph pointers
        pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);
        pPlaybackGraph = GetPlaybackGraph();

        if(pCaptureGraph && pPlaybackGraph)
        {

            if(FAILED(hr = pCaptureGraph->Init(hwnd, &rc, dwGraphLayout)))
            {
                FAIL(TEXT("Initializing the capture graph failed."));
                goto CleanupCapture;
            }

            _stprintf(tszFileName, tszBaseFileName, tszExtraPath, dwGraphLayout);
            _tcscat(tszFileName, tszCompletor);

            if(FAILED(hr = pCaptureGraph->SetVideoCaptureFileName(tszFileName)))
            {
                FAIL(TEXT("Failed to set the capture filename."));
                goto CleanupCapture;
            }

            if(FAILED(hr = pCaptureGraph->RunGraph()))
            {
                FAIL(TEXT("Starting the capture graph failed."));
                goto CleanupCapture;
            }

            Log(TEXT("Capture length %d seconds"), nCaptureLength/1000);

            Log(TEXT("  Capture starting"));

            // loop through a few times verifying the capture was correct.
            if(FAILED(hr = pCaptureGraph->StartStreamCapture())  && hr != E_WRITE_ERROR_EVENT && hr != E_OOM_EVENT)
            {
                if(hr != E_NO_FRAMES_PROCESSED)
                {
                    FAIL(TEXT("Failed to start the stream capture."));
                }
                else Log(TEXT("Insufficient memory to process even one frame of the capture."));

                goto CleanupCapture;
            }

            Log(TEXT("  Capture running"));

            Sleep(nCaptureLength);

            Log(TEXT("  Capture complete"));

            HRESULT hr = S_OK;
            if(FAILED(hr = pCaptureGraph->StopStreamCapture()))
            {
                if(hr == E_WRITE_ERROR_EVENT)
                    Log(TEXT("Received a write error event while doing the capture (out of storage space?)."));
                else if(hr == E_OOM_EVENT)
                    Log(TEXT("Received an OOM event while doing the capture (insufficient memory for this capture length?)."));
                else
                {
                    FAIL(TEXT("Failed to stop the stream capture."));
                    goto CleanupCapture;
                }
            }

            Log(TEXT("  Capture stream stopped"));

            if(FAILED(hr = pCaptureGraph->StopGraph()))
            {
                FAIL(TEXT("Failed to stop the graph after capture."));
                goto CleanupCapture;
            }

            // reset the string length for retrieval
            nStringLength = MAX_PATH;
            // retrieve the file name after the capture was started, so it's the file name of the captured file
            if(FAILED(hr = pCaptureGraph->GetVideoCaptureFileName(tszFileName, &nStringLength)))
            {
                FAIL(TEXT("Failed to retrieve the video capture file name."));
                goto CleanupCapture;
            }

CleanupCapture:
            if(FAILED(pCaptureGraph->Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
                hr = E_FAIL;
            }

            if(SUCCEEDED(hr))
            {
                Log(TEXT("Playing back the video file just captured."));

                if(FAILED(hr = pPlaybackGraph->Init(hwnd, &rc, tszFileName)))
                {
                    FAIL(TEXT("Initializing the playback graph failed."));
                    goto CleanupPlayback;
                }

                if(FAILED(hr = pPlaybackGraph->PauseGraph()))
                {
                    FAIL(TEXT("Pausing the playback graph failed."));
                    goto CleanupPlayback;
                }

                nTickCountStart = GetTickCount();

                if(FAILED(hr = pPlaybackGraph->RunGraph()))
                {
                    FAIL(TEXT("Running the playback graph failed."));
                    goto CleanupPlayback;
                }

                if(FAILED(hr = pPlaybackGraph->WaitForCompletion(INFINITE, &lEventCode)))
                {
                    FAIL(TEXT("Waiting for playback completion failed."));
                    goto CleanupPlayback;
                }
                 nTickCountElapsed = GetTickCount() - nTickCountStart;

                if(nTickCountElapsed < nCaptureLength)
                    FAIL(TEXT("Playback shorter than the capture time."));

                // the playback time should never be less than the capture time, and up to 1 second more
                // when taking into account dshow latencies
                if(nTickCountElapsed - nCaptureLength > nVarianceAllowed)
                {
                    FAIL(TEXT("Payback time incorrect wasn't as expected."));
                    Log(TEXT("Actual playback time %d, expected %d +- %d"), nTickCountElapsed, nCaptureLength, nVarianceAllowed);
                }

CleanupPlayback:
                if(FAILED(pPlaybackGraph->Cleanup()))
                {
                    FAIL(TEXT("Cleaning up the graph failed."));
                }

                // verify that the file was created
                hFile = FindFirstFile(tszFileName, &fd);
                if(hFile == INVALID_HANDLE_VALUE)
                {
                    FAIL(TEXT("Failed to find the video file."));
                }
                else if(FALSE == FindClose(hFile))
                {
                    FAIL(TEXT("Failed to close the video file handle."));
                }

#ifndef SAVE_CAPTURE_FILES
                if(FALSE == DeleteFile(tszFileName))
                {
                    FAIL(TEXT("Failed to delete the video file."));
                }
#endif
            }
        }

        ReleaseWindow(hwnd);
    }
    else FAIL(TEXT("Failed to create the window necessary for testing."));

    return GetTestResult();
}

TESTPROCAPI MultigraphCaptureAndPlaybackTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    PPLAYBACKFRAMEWORK pPlaybackGraph = NULL;
    PCAPTUREFRAMEWORK pCaptureGraph = NULL;

    HWND hwndParent, hwndPlayback, hwndCapture;
    RECT rc;
    DWORD dwGraphLayout = lpFTE->dwUserData;
    HRESULT hr = S_OK;
    long lEventCode = 0;
    TCHAR tszBaseFileName[] = TEXT("%s\\MultigraphCaptureAndPlaybackTest-%d-");
    TCHAR tszCompletor[] = TEXT("%d.asf");
#ifdef SAVE_CAPTURE_FILES
    TCHAR tszExtraPath[] = TEXT("\\release");
#else
    TCHAR tszExtraPath [] = TEXT("\0");
#endif
    TCHAR tszFileName[MAX_PATH] = {0};
    TCHAR *ptszPlaybackFileName = NULL;
    int nStringLength = MAX_PATH;
    int nCaptureLength[] = {30000, 25000, 20000, 15000, 10000, 5000};
    int nVarianceAllowed[] = { 8000, 8000, 7000, 6000, 6000, 6000};
    HANDLE hFile = NULL;
    WIN32_FIND_DATA fd;
    RECT rcPlayback, rcCapture;
    int nTickCountStart = 0;
    int nElapsedTickCount = 0;

    hwndParent = GetWindow(NULL, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT);

    if(hwndParent)
    {
        if(!GetClientRect(hwndParent, &rc))
        {
            FAIL(TEXT("Failed to retrieve the client rectangle for the parent window."));
        }

        hwndCapture = GetWindow(hwndParent, rc.left, rc.top, ((rc.right - rc.left)/2), ((rc.bottom - rc.top)/2));
        hwndPlayback = GetWindow(hwndParent, ((rc.right - rc.left)/2) + 1, rc.top, ((rc.right - rc.left)/2), ((rc.bottom - rc.top)/2));

        if(hwndPlayback && hwndCapture)
        {
            if(!GetClientRect(hwndCapture, &rcCapture))
            {
                FAIL(TEXT("Failed to retrieve the client rectangle for the capture window."));
            }

            if(!GetClientRect(hwndPlayback, &rcPlayback))
            {
                FAIL(TEXT("Failed to retrieve the client rectangle for the playback window."));
            }

            // retrieve the global graph pointers
            pCaptureGraph = GetCaptureGraphAndAdjustLayout(&dwGraphLayout);
            pPlaybackGraph = GetPlaybackGraph();

            if(pCaptureGraph && pPlaybackGraph)
            {
                if(FAILED(hr = pCaptureGraph->Init(hwndCapture, &rcCapture, dwGraphLayout)))
                {
                    FAIL(TEXT("Initializing the capture graph failed."));
                    goto Cleanup;
                }

                _stprintf(tszFileName, tszBaseFileName, tszExtraPath, dwGraphLayout);
                _tcscat(tszFileName, tszCompletor);

                if(FAILED(hr = pCaptureGraph->SetVideoCaptureFileName(tszFileName)))
                {
                    FAIL(TEXT("Failed to set the capture filename."));
                    goto Cleanup;
                }

                for(int i = 0; i < countof(nCaptureLength); i++)
                {

                    if(ptszPlaybackFileName)
                    {
                        if(FAILED(hr = pPlaybackGraph->Init(hwndPlayback, &rcPlayback, ptszPlaybackFileName)))
                        {
                            FAIL(TEXT("Initializing the playback graph failed."));
                            goto Cleanup;
                        }

                        if(FAILED(hr = pPlaybackGraph->PauseGraph()))
                        {
                            FAIL(TEXT("Pausing the playback graph failed."));
                            goto Cleanup;
                        }
                    }

                    if(FAILED(hr = pCaptureGraph->RunGraph()))
                    {
                        FAIL(TEXT("Starting the capture graph failed."));
                        goto Cleanup;
                    }

                    Log(TEXT("Capture length %d seconds"), nCaptureLength[i]/1000);

                    Log(TEXT("  Capture starting"));

                    // loop through a few times verifying the capture was correct.
                    if(FAILED(hr = pCaptureGraph->StartStreamCapture())  && hr != E_WRITE_ERROR_EVENT && hr != E_OOM_EVENT)
                    {
                        if(hr != E_NO_FRAMES_PROCESSED)
                        {
                            FAIL(TEXT("Failed to start the stream capture."));
                        }
                        else Log(TEXT("Insufficient memory to process even one frame of the capture."));

                        goto Cleanup;
                    }

                    Log(TEXT("  Capture running"));

                    Sleep(nCaptureLength[i]);

                    Log(TEXT("  Capture timeup complete, stopping the stream."));

                    if(FAILED(hr = pCaptureGraph->StopStreamCapture()))
                    {
                        if(hr == E_WRITE_ERROR_EVENT)
                            Log(TEXT("Received a write error event while doing the capture (out of storage space?)."));
                        else if(hr == E_OOM_EVENT)
                            Log(TEXT("Received an OOM event while doing the capture (insufficient memory for this capture length?)."));
                        else
                        {
                            FAIL(TEXT("Failed to stop the stream capture."));
                            goto Cleanup;
                        }

                    }

                    if(ptszPlaybackFileName)
                    {
                        /* fix for MPEG Bug# 332587 */
                        /* verifying the play back after capture is completed */
                        Log(TEXT("  Playback running"));

                        nTickCountStart = GetTickCount();
                        if(FAILED(hr = pPlaybackGraph->RunGraph()))
                        {
                            FAIL(TEXT("Running the playback graph failed."));
                            goto Cleanup;
                        }

                        Log(TEXT("  Waiting for playback to complete."));

                        if(FAILED(hr = pPlaybackGraph->WaitForCompletion(INFINITE, &lEventCode)))
                        {
                            FAIL(TEXT("Waiting for playback completion failed."));
                            goto Cleanup;
                        }
                        nElapsedTickCount = GetTickCount() - nTickCountStart;
                        Log(TEXT("  Playback complete."));

                        if(nElapsedTickCount < nCaptureLength[i])
                            FAIL(TEXT("Playback shorter than the capture time."));
    
                        if((nElapsedTickCount - nCaptureLength[i]) > nVarianceAllowed[i])
                        {
                            FAIL(TEXT("Payback time incorrect wasn't as expected."));
                            Log(TEXT("Actual playback time %d, expected %d +- %d"), nElapsedTickCount, nCaptureLength[i], nVarianceAllowed[i]);
                        }
                    }

                    Log(TEXT("  Capture stream stopped, stopping the graph."));

                    if(FAILED(hr = pCaptureGraph->StopGraph()))
                    {
                        FAIL(TEXT("Failed to stop the graph after capture."));
                        goto Cleanup;
                    }

                    if(ptszPlaybackFileName)
                    {
                        if(FAILED(pPlaybackGraph->Cleanup()))
                        {
                            FAIL(TEXT("Cleaning up the graph failed."));
                        }

                        // verify that the file was created
                        hFile = FindFirstFile(ptszPlaybackFileName, &fd);
                        if(hFile == INVALID_HANDLE_VALUE)
                        {
                            FAIL(TEXT("Failed to find the video file."));
                        }
                        else if(FALSE == FindClose(hFile))
                        {
                            FAIL(TEXT("Failed to close the video file handle."));
                        }

#ifndef SAVE_CAPTURE_FILES
                        if(FALSE == DeleteFile(ptszPlaybackFileName))
                        {
                            FAIL(TEXT("Failed to delete the video file."));
                        }
#endif
                    }
                    // reset the string length for retrieval
                    nStringLength = MAX_PATH;
                    // retrieve the file name after the capture was started, so it's the file name of the captured file
                    if(FAILED(hr = pCaptureGraph->GetVideoCaptureFileName(tszFileName, &nStringLength)))
                    {
                        FAIL(TEXT("Failed to retrieve the video capture file name."));
                        goto Cleanup;
                    }

                    // for the first loop the playback file name isn't set, for subsequent loops it's set to the file name just captured.
                    ptszPlaybackFileName = tszFileName;
                }

    Cleanup:
                if(FAILED(pCaptureGraph->Cleanup()))
                {
                    FAIL(TEXT("Cleaning up the graph failed."));
                    hr = E_FAIL;
                }
            }

            ReleaseWindow(hwndPlayback);
            ReleaseWindow(hwndCapture);
        }
        ReleaseWindow(hwndParent);
    }
    else FAIL(TEXT("Failed to create the windows necessary for testing."));

    return GetTestResult();
}

TESTPROCAPI MultiThreadedBuildTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // static variables shared across threads (only used at intitialization and teardown, and therefore do not need to be protected
    // by a critical section)
    static HWND hwndParent = NULL;
    static BOOL volatile bInitialized = FALSE;
    static BOOL volatile bAbort = FALSE;
    static LONG volatile ThreadCount = 0;

    // local variables, unique to each thread.
    int i;
    RECT rc;
    PCAPTUREFRAMEWORK pCaptureGraph;

    if(uMsg == TPM_QUERY_THREAD_COUNT)
    {
        // retrieve the thread count
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = g_sScenarioTestData[lpFTE->dwUserData].nThreadCount;
        return SPR_HANDLED;
    }
    else if(uMsg == SPM_BEGIN_TEST)
    {
        return SPR_HANDLED;
    }
    else if(uMsg == SPM_END_TEST)
    {
        // a failure in here will never be registered with the test status.

        // cleanup the video files created
        for(i = 0; i < g_sScenarioTestData[lpFTE->dwUserData].nThreadCount; i++)
        {
            if(g_sScenarioTestData[lpFTE->dwUserData].pTestData[i]->tszFileName)
                DeleteFile(g_sScenarioTestData[lpFTE->dwUserData].pTestData[i]->tszFileName);
        }

        // close out the windows created
        for(i = 0; i < g_sScenarioTestData[lpFTE->dwUserData].nThreadCount; i++)
        {
            if(g_sScenarioTestData[lpFTE->dwUserData].pTestData[i]->hwnd)
                ReleaseWindow(g_sScenarioTestData[lpFTE->dwUserData].pTestData[i]->hwnd);
        }

        return SPR_HANDLED;
    }
    else if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    struct sTestCaseData * pTestData = g_sScenarioTestData[lpFTE->dwUserData].pTestData[lpExecute->dwThreadNumber - 1];

    InterlockedIncrement(&ThreadCount);

    if(pTestData->nGraphType == MessagePump)
    {
        // NOTE: The message pump thread must be the same thread that creates the windows, if it's not then
        // the messages won't be processed. If messages aren't processed the video renderer will hang until the messages
        // are processed.

        // create the windows in each screen quadrant as needed, and assign the window to the corresponding thread
        hwndParent = GetWindow(NULL, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT);

        if(hwndParent)
        {
            if(!GetClientRect(hwndParent, &rc))
            {
                ABORT(TEXT("Failed to retrieve the client rectangle for the parent window."));
                bAbort = TRUE;
            }

            if(g_sScenarioTestData[lpFTE->dwUserData].pTestData[1])
            {
                if(NULL == (g_sScenarioTestData[lpFTE->dwUserData].pTestData[1]->hwnd = GetWindow(hwndParent, rc.left + 1, rc.top + 1, ((rc.right - rc.left)/2) - 2, ((rc.bottom - rc.top)/2) - 2)))
                {
                    ABORT(TEXT("Failed to create the window requested."));
                    bAbort = TRUE;
                }
            }

            if(g_sScenarioTestData[lpFTE->dwUserData].pTestData[2])
            {
                if(NULL == (g_sScenarioTestData[lpFTE->dwUserData].pTestData[2]->hwnd = GetWindow(hwndParent, ((rc.right - rc.left)/2) + 1, rc.top + 1, ((rc.right - rc.left)/2) - 2, ((rc.bottom - rc.top)/2) - 2)))
                {
                    ABORT(TEXT("Failed to create the window requested."));
                    bAbort = TRUE;
                }
            }

            if(g_sScenarioTestData[lpFTE->dwUserData].pTestData[3])
            {
                if(NULL == (g_sScenarioTestData[lpFTE->dwUserData].pTestData[3]->hwnd = GetWindow(hwndParent, rc.left + 1, ((rc.bottom - rc.top)/2) + 1, ((rc.right - rc.left)/2) - 2, ((rc.bottom - rc.top)/2) - 2)))
                {
                    ABORT(TEXT("Failed to create the window requested."));
                    bAbort = TRUE;
                }
            }

            if(g_sScenarioTestData[lpFTE->dwUserData].pTestData[4])
            {
                if(NULL == (g_sScenarioTestData[lpFTE->dwUserData].pTestData[4]->hwnd = GetWindow(hwndParent, ((rc.right - rc.left)/2) + 1, ((rc.bottom - rc.top)/2) + 1, ((rc.right - rc.left)/2) - 2, ((rc.bottom - rc.top)/2) - 2)))
                {
                    ABORT(TEXT("Failed to create the window requested."));
                    bAbort = TRUE;
                }
            }

            for(i = 0; i < g_sScenarioTestData[lpFTE->dwUserData].nThreadCount; i++)
            {
                // only do captures for playback tests (so we have media to play).
                if(g_sScenarioTestData[lpFTE->dwUserData].pTestData[i]->nGraphType == PlaybackGraph)
                {
                    Log(TEXT("Doing a capture for plaback instance %d, %d seconds."), i, g_sScenarioTestData[lpFTE->dwUserData].pTestData[i]->nFileLength/1000);

                    pCaptureGraph = GetCaptureGraphAndAdjustLayout(&(g_sScenarioTestData[lpFTE->dwUserData].pTestData[i]->dwGraphLayout));
                    if(!pCaptureGraph)
                    {
                        bAbort = TRUE;
                        break;
                    }

                    if(!GetClientRect(g_sScenarioTestData[lpFTE->dwUserData].pTestData[i]->hwnd, &rc))
                    {
                        ABORT(TEXT("Failed to retrieve the client rectangle for the parent window."));
                        bAbort = TRUE;
                        break;
                    }

                    if(FAILED(pCaptureGraph->Init(g_sScenarioTestData[lpFTE->dwUserData].pTestData[i]->hwnd, &rc, g_sScenarioTestData[lpFTE->dwUserData].pTestData[i]->dwGraphLayout)))
                    {
                        ABORT(TEXT("Initializing the playback graph failed."));
                        bAbort = TRUE;
                        break;
                    }

                    if(FAILED(pCaptureGraph->SetVideoCaptureFileName(g_sScenarioTestData[lpFTE->dwUserData].pTestData[i]->tszFileName)))
                    {
                        ABORT(TEXT("Failed to set the capture filename."));
                        bAbort = TRUE;
                        break;
                    }

                    if(FAILED(pCaptureGraph->RunGraph()))
                    {
                        ABORT(TEXT("Starting the capture graph failed."));
                        bAbort = TRUE;
                        break;
                    }

                    // loop through a few times verifying the capture was correct.
                    if(FAILED(pCaptureGraph->StartStreamCapture()))
                    {
                        ABORT(TEXT("Insufficient memory to process even one frame of the capture."));
                        bAbort = TRUE;
                        break;
                    }

                    Sleep(g_sScenarioTestData[lpFTE->dwUserData].pTestData[i]->nFileLength);

                    HRESULT hr = S_OK;
                    if(FAILED(hr = pCaptureGraph->StopStreamCapture()))
                    {
                        if(hr == E_WRITE_ERROR_EVENT)
                            Log(TEXT("Received a write error event while doing the capture (out of storage space?)."));
                        else if(hr == E_OOM_EVENT)
                            Log(TEXT("Received an OOM event while doing the capture (insufficient memory for this capture length?)."));
                        else
                        {
                            ABORT(TEXT("Failed to stop the stream capture."));
                            bAbort = TRUE;
                            break;
                        }
                    }

                    if(FAILED(pCaptureGraph->StopGraph()))
                    {
                        ABORT(TEXT("Failed to stop the graph after capture."));
                        bAbort = TRUE;
                        break;
                    }

                    if(FAILED(pCaptureGraph->Cleanup()))
                    {
                        ABORT(TEXT("Cleaning up the graph failed."));
                        bAbort = TRUE;
                        break;
                    }
                }
            }
        }
        else bAbort = TRUE;

        // intialization succeeded, notify the other threads and get the tests running
        if(!bAbort)
            bInitialized = TRUE;

        // let the other threads get ready to run
        Sleep(1000);

        // make sure we let the child threads get running.
        // once started every thread will run for a minute or so,
        // so we aren't at risk of the thread count getting back to 1
        // in the middle of the test, thereby killing the message pump thread.
        while(ThreadCount == 1)
            Sleep(100);


        if(bInitialized)
        {
            // now that everything is running, process the windows messages.
            // keep processing until all of the other threads exit.
            while(ThreadCount > 1)
            {
                // pump messages is non-blocking. Because we don't want to block infinitaly
                // processing all of the windows messages for all of the playback/preview windows,
                // we check and process until we're finished once a second.
                pumpMessages();
                Sleep(1000);
            }
        }
    }
    else if(pTestData->nGraphType == CaptureGraph)
    {
        Sleep(rand() % 10);

        // wait until the system is initialized before testing.
        while(!bInitialized && !bAbort)
            Sleep(100);

        if(bInitialized)
        {
            //execute the capture thread
            for(int i=0; i < 5; i++)
            {
                Log(TEXT("Executing the capture thread instance %d, %d seconds."), i, pTestData->nFileLength/1000);

                pCaptureGraph = GetCaptureGraphAndAdjustLayout(&(pTestData->dwGraphLayout));
                if(!pCaptureGraph)
                {
                    break;
                }

                if(!GetClientRect(pTestData->hwnd, &rc))
                {
                    FAIL(TEXT("Failed to retrieve the client rectangle for the parent window."));
                    break;
                }

                if(FAILED(pCaptureGraph->Init(pTestData->hwnd, &rc, pTestData->dwGraphLayout)))
                {
                    FAIL(TEXT("Initializing the playback graph failed."));
                    break;
                }

                if(FAILED(pCaptureGraph->SetVideoCaptureFileName(pTestData->tszFileName)))
                {
                    FAIL(TEXT("Failed to set the capture filename."));
                    break;
                }

                if(FAILED(pCaptureGraph->RunGraph()))
                {
                    FAIL(TEXT("Starting the capture graph failed."));
                    break;
                }

                // loop through a few times verifying the capture was correct.
                if(FAILED(pCaptureGraph->StartStreamCapture()))
                {
                    FAIL(TEXT("Insufficient memory to process even one frame of the capture."));
                    break;
                }

                Sleep(pTestData->nFileLength);

                HRESULT hr = S_OK;
                if(FAILED(hr = pCaptureGraph->StopStreamCapture()))
                {
                    if(hr == E_WRITE_ERROR_EVENT)
                        Log(TEXT("Received a write error event while doing the capture (out of storage space?)."));
                    else if(hr == E_OOM_EVENT)
                        Log(TEXT("Received an OOM event while doing the capture (insufficient memory for this capture length?)."));
                    else
                    {
                        FAIL(TEXT("Failed to stop the stream capture."));
                        break;
                    }
                }

                if(FAILED(pCaptureGraph->StopGraph()))
                {
                    FAIL(TEXT("Failed to stop the graph after capture."));
                    break;
                }

                if(FAILED(pCaptureGraph->Cleanup()))
                {
                    FAIL(TEXT("Cleaning up the graph failed."));
                    break;
                }
                Sleep(rand() % 10);
            }

            // if something failed above, resulting in exiting the loop, make sure we clean up.
            if(FAILED(pCaptureGraph->Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
            }
        }
        InterlockedDecrement(&ThreadCount);
    }
    else if(pTestData->nGraphType == PlaybackGraph)
    {
        PLAYBACKFRAMEWORK cPlaybackGraph;
        PLAYBACKVIDEORENDERER_STATS Stats;
        LONG lEventCode;

        // wait until the system is initialized before doing the testing.
        while(!bInitialized && !bAbort)
            Sleep(100);

        if(bInitialized)
        {

            for(int i=0; i < 5; i++)
            {
                Log(TEXT("Executing the playback thread %d."), lpExecute->dwThreadNumber);

                if(!GetClientRect(pTestData->hwnd, &rc))
                {
                    ABORT(TEXT("Failed to retrieve the client rectangle for the parent window."));
                    break;
                }

                if(FAILED(cPlaybackGraph.Init(pTestData->hwnd, &rc, pTestData->tszFileName)))
                {
                    FAIL(TEXT("Initializing the playback graph failed."));
                    break;
                }

                if(FAILED(cPlaybackGraph.PauseGraph()))
                {
                    FAIL(TEXT("Pausing the playback graph failed."));
                    break;
                }

                if(FAILED(cPlaybackGraph.RunGraph()))
                {
                    FAIL(TEXT("Running the playback graph failed."));
                    break;
                }

                if(FAILED(cPlaybackGraph.WaitForCompletion(INFINITE, &lEventCode)))
                {
                    FAIL(TEXT("Waiting for playback completion failed."));
                    break;
                }

                if(FAILED(cPlaybackGraph.GetVideoRenderStats(&Stats)))
                {
                    FAIL(TEXT("Retrieving the video renderer stats failed."));
                    break;
                }

                if(Stats.lFramesDrawn == 0)
                {
                    FAIL(TEXT("0 frames drawn by the video renderer, expected something rendered during playback."));
                    Log(TEXT("FAIL: Thread %d, %d frames dropped, 0 frames rendered"), lpExecute->dwThreadNumber - 1, Stats.lFramesDropped);
                }

                if(FAILED(cPlaybackGraph.Cleanup()))
                {
                    FAIL(TEXT("Cleaning up the graph failed."));
                    break;
                }

                Sleep(rand() % 10);
            }

            // if something failed above, resulting in exiting the loop, make sure we clean up.
            if(FAILED(cPlaybackGraph.Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
            }
        }

        InterlockedDecrement(&ThreadCount);
    }
    else
        FAIL(TEXT("Unknown graph type."));

    return GetTestResult();
}

TESTPROCAPI MultiThreadedBuildTest2( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // static variables shared across threads (only used at intitialization and teardown, and therefore do not need to be protected
    // by a critical section)
    static BOOL bInitialized = FALSE;
    static RECT rc = {0, 0, 0, 0};

    // local variables, unique to each thread.
    HWND hwndParent = NULL;
    int i;
    PCAPTUREFRAMEWORK pCaptureGraph;

    if(uMsg == TPM_QUERY_THREAD_COUNT)
    {
        // retrieve the thread count
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = g_sScenarioTestData[lpFTE->dwUserData].nThreadCount;
        return SPR_HANDLED;
    }
    else if(uMsg == SPM_BEGIN_TEST)
    {

        bInitialized = TRUE;

        // create the parent window first, this gives us our work area and is used for the initial captures
        hwndParent = GetWindow(NULL, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT);

        if(hwndParent)
        {
            if(!GetClientRect(hwndParent, &rc))
            {
                ABORT(TEXT("Failed to retrieve the client rectangle for the parent window."));
                bInitialized = FALSE;
            }

            for(i = 0; i < g_sScenarioTestData[lpFTE->dwUserData].nThreadCount; i++)
            {
                // only do captures for playback tests (so we have media to play).
                if(g_sScenarioTestData[lpFTE->dwUserData].pTestData[i]->nGraphType == PlaybackGraph)
                {
                    Log(TEXT("Doing a capture for plaback instance %d, %d seconds."), i, g_sScenarioTestData[lpFTE->dwUserData].pTestData[i]->nFileLength/1000);

                    pCaptureGraph = GetCaptureGraphAndAdjustLayout(&(g_sScenarioTestData[lpFTE->dwUserData].pTestData[i]->dwGraphLayout));
                    if(!pCaptureGraph)
                    {
                        bInitialized = FALSE;
                        break;
                    }

                    if(!GetClientRect(hwndParent, &rc))
                    {
                        ABORT(TEXT("Failed to retrieve the client rectangle for the parent window."));
                        bInitialized = FALSE;
                        break;
                    }

                    if(FAILED(pCaptureGraph->Init(hwndParent, &rc, g_sScenarioTestData[lpFTE->dwUserData].pTestData[i]->dwGraphLayout)))
                    {
                        ABORT(TEXT("Initializing the playback graph failed."));
                        bInitialized = FALSE;
                        break;
                    }

                    if(FAILED(pCaptureGraph->SetVideoCaptureFileName(g_sScenarioTestData[lpFTE->dwUserData].pTestData[i]->tszFileName)))
                    {
                        ABORT(TEXT("Failed to set the capture filename."));
                        bInitialized = FALSE;
                        break;
                    }

                    if(FAILED(pCaptureGraph->RunGraph()))
                    {
                        ABORT(TEXT("Starting the capture graph failed."));
                        bInitialized = FALSE;
                        break;
                    }

                    // loop through a few times verifying the capture was correct.
                    if(FAILED(pCaptureGraph->StartStreamCapture()))
                    {
                        ABORT(TEXT("Insufficient memory to process even one frame of the capture."));
                        bInitialized = FALSE;
                        break;
                    }

                    Sleep(g_sScenarioTestData[lpFTE->dwUserData].pTestData[i]->nFileLength);

                    HRESULT hr = S_OK;
                    if(FAILED(hr = pCaptureGraph->StopStreamCapture()))
                    {
                        if(hr == E_WRITE_ERROR_EVENT)
                            Log(TEXT("Received a write error event while doing the capture (out of storage space?)."));
                        else if(hr == E_OOM_EVENT)
                            Log(TEXT("Received an OOM event while doing the capture (insufficient memory for this capture length?)."));
                        else
                        {
                            ABORT(TEXT("Failed to stop the stream capture."));
                            bInitialized = FALSE;
                            break;
                        }
                    }

                    if(FAILED(pCaptureGraph->StopGraph()))
                    {
                        ABORT(TEXT("Failed to stop the graph after capture."));
                        bInitialized = FALSE;
                        break;
                    }

                    if(FAILED(pCaptureGraph->Cleanup()))
                    {
                        ABORT(TEXT("Cleaning up the graph failed."));
                        bInitialized = FALSE;
                        break;
                    }
                }
            }

            ReleaseWindow(hwndParent);
        }
        else bInitialized = FALSE;

        return SPR_HANDLED;
    }
    else if(uMsg == SPM_END_TEST)
    {
        // a failure in here will never be registered with the test status as the test case has already exited.

        // cleanup the video files created
        for(i = 0; i < g_sScenarioTestData[lpFTE->dwUserData].nThreadCount; i++)
        {
            if(g_sScenarioTestData[lpFTE->dwUserData].pTestData[i]->tszFileName)
                DeleteFile(g_sScenarioTestData[lpFTE->dwUserData].pTestData[i]->tszFileName);
        }

        return SPR_HANDLED;
    }
    else if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }
    else if(!bInitialized)
    {
        ABORT(TEXT("Initialization of the test suite failed, cannot continue"));
        return GetTestResult();
    }

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    POINT ptTopLeftInUse;
    int nWidth = ((rc.right - rc.left)/2) - 2;
    int nHeight = ((rc.bottom - rc.top)/2) - 2;
    struct sTestCaseData * pTestData = g_sScenarioTestData[lpFTE->dwUserData].pTestData[lpExecute->dwThreadNumber - 1];

    switch(lpExecute->dwThreadNumber)
    {
        case 1:
            ptTopLeftInUse.x = rc.left + 1;
            ptTopLeftInUse.y = rc.top + 1;
            break;
        case 2:
            ptTopLeftInUse.x = ((rc.right - rc.left)/2) + 1;
            ptTopLeftInUse.y = rc.top + 1;
            break;
        case 3:
            ptTopLeftInUse.x = rc.left + 1;
            ptTopLeftInUse.y = ((rc.bottom - rc.top)/2) + 1;
            break;
        case 4:
            ptTopLeftInUse.x = ((rc.right - rc.left)/2) + 1;
            ptTopLeftInUse.y = ((rc.bottom - rc.top)/2) + 1;
            break;
    }

    if(NULL == (pTestData->hwnd = GetWindow(NULL, ptTopLeftInUse.x, ptTopLeftInUse.y, nWidth, nHeight)))
    {
        FAIL(TEXT("Failed to create the window requested."));
    }
    else
    {

        if(pTestData->nGraphType == CaptureGraph)
        {
            //execute the capture thread
            for(int i=0; i < 10; i++)
            {
                Log(TEXT("Executing the capture thread instance %d, %d seconds."), i, pTestData->nFileLength/1000);

                pCaptureGraph = GetCaptureGraphAndAdjustLayout(&(pTestData->dwGraphLayout));
                if(!pCaptureGraph)
                {
                    break;
                }

                if(!GetClientRect(pTestData->hwnd, &rc))
                {
                    FAIL(TEXT("Failed to retrieve the client rectangle for the parent window."));
                    break;
                }

                if(FAILED(pCaptureGraph->Init(pTestData->hwnd, &rc, pTestData->dwGraphLayout)))
                {
                    FAIL(TEXT("Initializing the playback graph failed."));
                    break;
                }

                if(FAILED(pCaptureGraph->SetVideoCaptureFileName(pTestData->tszFileName)))
                {
                    FAIL(TEXT("Failed to set the capture filename."));
                    break;
                }

                if(FAILED(pCaptureGraph->RunGraph()))
                {
                    FAIL(TEXT("Starting the capture graph failed."));
                    break;
                }

                // loop through a few times verifying the capture was correct.
                if(FAILED(pCaptureGraph->StartStreamCapture()))
                {
                    FAIL(TEXT("Insufficient memory to process even one frame of the capture."));
                    break;
                }

                Sleep(pTestData->nFileLength);

                HRESULT hr = S_OK;
                if(FAILED(hr = pCaptureGraph->StopStreamCapture()))
                {
                    if(hr == E_WRITE_ERROR_EVENT)
                        Log(TEXT("Received a write error event while doing the capture (out of storage space?)."));
                    else if(hr == E_OOM_EVENT)
                        Log(TEXT("Received an OOM event while doing the capture (insufficient memory for this capture length?)."));
                    else
                    {
                        FAIL(TEXT("Failed to stop the stream capture."));
                        break;
                    }
                }

                if(FAILED(pCaptureGraph->StopGraph()))
                {
                    FAIL(TEXT("Failed to stop the graph after capture."));
                    break;
                }

                if(FAILED(pCaptureGraph->Cleanup()))
                {
                    FAIL(TEXT("Cleaning up the graph failed."));
                    break;
                }
            }

            // if something failed above, resulting in exiting the loop, make sure we clean up.
            if(FAILED(pCaptureGraph->Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
            }
        }
        else if(pTestData->nGraphType == PlaybackGraph)
        {
            PLAYBACKFRAMEWORK cPlaybackGraph;
            PLAYBACKVIDEORENDERER_STATS Stats;
            LONG lEventCode;

            for(int i=0; i < 10; i++)
            {
                Log(TEXT("Executing the playback thread %d."), lpExecute->dwThreadNumber - 1);

                if(!GetClientRect(pTestData->hwnd, &rc))
                {
                    ABORT(TEXT("Failed to retrieve the client rectangle for the parent window."));
                    break;
                }

                if(FAILED(cPlaybackGraph.Init(pTestData->hwnd, &rc, pTestData->tszFileName)))
                {
                    FAIL(TEXT("Initializing the playback graph failed."));
                    break;
                }

                if(FAILED(cPlaybackGraph.PauseGraph()))
                {
                    FAIL(TEXT("Pausing the playback graph failed."));
                    break;
                }

                if(FAILED(cPlaybackGraph.RunGraph()))
                {
                    FAIL(TEXT("Running the playback graph failed."));
                    break;
                }

                if(FAILED(cPlaybackGraph.WaitForCompletion(INFINITE, &lEventCode)))
                {
                    FAIL(TEXT("Waiting for playback completion failed."));
                    break;
                }

                if(FAILED(cPlaybackGraph.GetVideoRenderStats(&Stats)))
                {
                    FAIL(TEXT("Retrieving the video renderer stats failed."));
                    break;
                }

                if(Stats.lFramesDrawn == 0)
                {
                    FAIL(TEXT("0 frames drawn by the video renderer, expected something rendered during playback."));
                    Log(TEXT("FAIL: Thread %d, %d frames dropped, 0 frames rendered"), lpExecute->dwThreadNumber - 1, Stats.lFramesDropped);
                }

                if(FAILED(cPlaybackGraph.Cleanup()))
                {
                    FAIL(TEXT("Cleaning up the graph failed."));
                    break;
                }
            }

            // if something failed above, resulting in exiting the loop, make sure we clean up.
            if(FAILED(cPlaybackGraph.Cleanup()))
            {
                FAIL(TEXT("Cleaning up the graph failed."));
            }
        }
        else
            FAIL(TEXT("Unknown graph type."));

        ReleaseWindow(pTestData->hwnd);
        pTestData->hwnd = NULL;
    }

    return GetTestResult();
}


