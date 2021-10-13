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
#include <windows.h>
#include "captureframework.h"
#include "resource.h"
#include "commctrl.h"

void Debug(LPCTSTR szFormat, ...)
{
    static  TCHAR   szHeader[] = TEXT("");
    TCHAR   szBuffer[1024];

    va_list pArgs;
    va_start(pArgs, szFormat);
    lstrcpy(szBuffer, szHeader);
    _vstprintf(
        szBuffer + countof(szHeader) - 1,
        szFormat,
        pArgs);
    va_end(pArgs);

    _tcscat(szBuffer, TEXT("\r\n"));

    OutputDebugString(szBuffer);
}

#define BUTTON_WIDTH 65
#define BUTTON_HEIGHT 15

static HINSTANCE ghInstance = NULL;
static HWND g_Hwnd=NULL;

typedef struct _BUTTONINFO
{
    int nButtonStyle;
    TCHAR *tszButtonName;
    int nButtonState;
    HWND hwndButton;
    int nButtonWidth;
    int nButtonHeight;
} BUTTONINFO;

enum
{
    BUTTON_QUIT,
    BUTTON_STILL,
    BUTTON_STARTCAPTURE,
    BUTTON_PAUSECAPTURE,
    BUTTON_STOPCAPTURE,
    BUTTON_STOP,
    BUTTON_PAUSE,
    BUTTON_RUN,
    BUTTON_WAIT,
    ENCODE_PROGRESS,
    DROPPED_FRAMES
};

BUTTONINFO gButtonInfo [] = 
{
    {BS_PUSHBUTTON, TEXT("Quit"), 0, NULL, BUTTON_WIDTH, BUTTON_HEIGHT },
    {BS_PUSHBUTTON, TEXT("Still"), 0, NULL, BUTTON_WIDTH, BUTTON_HEIGHT },
    {BS_PUSHBUTTON, TEXT("Start Cap"), 0, NULL, BUTTON_WIDTH, BUTTON_HEIGHT },
    {BS_PUSHBUTTON, TEXT("Pause Cap"), 0, NULL, BUTTON_WIDTH, BUTTON_HEIGHT },
    {BS_PUSHBUTTON, TEXT("Stop Cap"), 0, NULL, BUTTON_WIDTH, BUTTON_HEIGHT },
    {BS_PUSHBUTTON, TEXT("Stop"), 0, NULL, BUTTON_WIDTH, BUTTON_HEIGHT },
    {BS_PUSHBUTTON, TEXT("Pause"), 0, NULL, BUTTON_WIDTH, BUTTON_HEIGHT },
    {BS_PUSHBUTTON, TEXT("Run"), 0, NULL, BUTTON_WIDTH, BUTTON_HEIGHT },
    {BS_PUSHBUTTON, TEXT("Wait"), 0, NULL, BUTTON_WIDTH, BUTTON_HEIGHT },
};

CCaptureFramework g_DShowCaptureGraph;
DWORD g_dwFilters = 0;
TCHAR g_tszStillFileName[MAX_PATH] = STILL_IMAGE_FILE_NAME;
TCHAR g_tszCaptureFileName[MAX_PATH] = VIDEO_CAPTURE_FILE_NAME;

BOOL g_bCaptureActive = FALSE;
BOOL g_bCapturePaused = FALSE;
int g_nFramesProcessed = 0;
int g_nOOMEvents = 0;
RECT g_rcStatusArea = {0, 0, 0, 0};
REFERENCE_TIME g_rtCaptureStart;
REFERENCE_TIME g_rtCurrentEncodePosition;
REFERENCE_TIME g_rtEndEncodePosition;
HWND g_hwndEncodeStatus = NULL;
HWND g_hwndDroppedFrames = NULL;

UINT g_uiLastEncodePosition = 100;
UINT g_uiLastDroppedFramesPosition = 0;

HRESULT
ProcessEvents(BOOL bComplete, BOOL bWaitForVideo, WORD wVideoCookie, BOOL bWaitForAudio, WORD wAudioCookie )
{
    LONG lEvent, lParam1, lParam2;
    HRESULT hrEvent;
    long lEventTimeout = bComplete?60000:0;
    UINT uiNewEncodePosition, uiNewDroppedFramesPositon;
    // if we're waiting for event 1, then set the received state to false.
    // if we're not waiting for the event, then set the received state to true
    BOOL bReceivedVideoEvent = !bWaitForVideo, bReceivedAudioEvent = !bWaitForAudio;

    while(1)
    {
        hrEvent = g_DShowCaptureGraph.GetEvent(&lEvent, &lParam1, &lParam2, lEventTimeout);

        if(SUCCEEDED(hrEvent))
        {
            // if we succeeded, then see what the event was
            if(lEvent == EC_CAP_SAMPLE_PROCESSED)
            {
                // if the event was a sample processed message, nothing was allocated to be freed in the free,
                // so we're OK accessing lParam1 (not a use after free).
                g_rtCurrentEncodePosition = (REFERENCE_TIME) lParam2 * 10000;
                g_nFramesProcessed++;
            }
            else if(lEvent == EC_BUFFER_FULL)
            {
                g_nOOMEvents++;
                // processing frames is already outputted by GetMediaEvent.
                if((g_nOOMEvents % 20) == 0)
                    Log(TEXT("Frames dropped"));
            }
            else if(lEvent == EC_BUFFER_DOWNSTREAM_ERROR)
            {
                Log(TEXT("Buffering filter failed to deliver data downstream."));
            }
            else if(lEvent == EC_STREAM_CONTROL_STOPPED && lParam2 == wVideoCookie)
                bReceivedVideoEvent = TRUE;
            else if(lEvent == EC_STREAM_CONTROL_STOPPED && lParam2 == wAudioCookie)
                bReceivedAudioEvent = TRUE;

            uiNewEncodePosition = (UINT) (((float) (g_rtCurrentEncodePosition - g_rtCaptureStart))/((float) (g_rtEndEncodePosition - g_rtCaptureStart)) * 100.);
            uiNewDroppedFramesPositon = (UINT) (((float) g_nOOMEvents / (float) max((g_nOOMEvents + g_nFramesProcessed), 1)) * 100.);

            SendMessage(g_hwndEncodeStatus, PBM_DELTAPOS, (WPARAM) (uiNewEncodePosition - g_uiLastEncodePosition), 0);
            SendMessage(g_hwndDroppedFrames, PBM_DELTAPOS, (WPARAM) (uiNewDroppedFramesPositon - g_uiLastDroppedFramesPosition), 0);

            g_uiLastEncodePosition = uiNewEncodePosition;
            g_uiLastDroppedFramesPosition = uiNewDroppedFramesPositon;

            if(bComplete && bReceivedVideoEvent && bReceivedAudioEvent)
                return S_OK;

        }
        else return bComplete?E_FAIL:S_FALSE;
    }

    return E_FAIL;
}

HRESULT
StopStreamCapture()
{
    HRESULT hr = S_OK;
    REFERENCE_TIME rtStart = 0;
    REFERENCE_TIME rtStop = 0;
    WORD wVideoStartCookie = 5, wVideoStopCookie = 6;
    WORD wAudioStartCookie = 7, wAudioStopCookie = 8;
    CComPtr<IMediaSeeking> pMediaSeeking;
    int nMediaEventsCounted = 0;
    CComPtr<ICaptureGraphBuilder2> pCaptureGraphBuilder = NULL;
    CComPtr<IGraphBuilder> pFilterGraph = NULL;
    CComPtr<IBaseFilter> pVideoCaptureFilter = NULL;
    CComPtr<IBaseFilter> pAudioCaptureFilter = NULL;

    if (FAILED(g_DShowCaptureGraph.GetBuilderInterface((void**)&pCaptureGraphBuilder, IID_ICaptureGraphBuilder2)))
    {
        RETAILMSG(1, ( TEXT("CameraDShowApp: Failed to retrieve the capture graph builder.")));
        goto cleanup;
    }

    if (FAILED(g_DShowCaptureGraph.GetBuilderInterface((void**)&pFilterGraph, IID_IGraphBuilder)))
    {
        RETAILMSG(1, ( TEXT("CameraDShowApp: Failed to retrieve the graph builder.")));
        goto cleanup;
    }

    hr = pFilterGraph->QueryInterface( &pMediaSeeking );
    if(FAILED(hr) || pMediaSeeking == NULL)
    {
        FAIL(TEXT("CCaptureFramework: Retrieving the MediaSeeking interface failed."));
        goto cleanup;
    }

    hr = pMediaSeeking->GetCurrentPosition(&rtStop);
    if(FAILED(hr))
    {
        RETAILMSG(1, ( TEXT("CameraDShowApp: Failed to retrieve the current position.")));
        goto cleanup;
    }

    g_rtEndEncodePosition = rtStop;

    if(g_dwFilters & VIDEO_CAPTURE_FILTER)
    {
        if(FAILED(g_DShowCaptureGraph.GetBaseFilter((void**)&pVideoCaptureFilter, VIDEO_CAPTURE_FILTER)))
        {
            RETAILMSG(1, ( TEXT("CameraDShowApp: Unable to retrieve the video capture filter.")));
            goto cleanup;
        }

        if(g_dwFilters & VIDEO_CAPTURE_FILTER)
        {
            hr = pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, pVideoCaptureFilter, NULL, 0, 0, 0 );
            if(FAILED(hr))
            {
                RETAILMSG(1, ( TEXT("CameraDShowApp: Failed to pause the preview stream.")));
            }
        }

        hr = pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, pVideoCaptureFilter, &rtStart, &rtStop, wVideoStartCookie, wVideoStopCookie);
        if(FAILED(hr))
        {
            RETAILMSG(1, ( TEXT("CameraDShowApp: Failed to stop the video stream.")));
            goto cleanup;
        }
    }

    if(g_dwFilters & AUDIO_CAPTURE_FILTER)
    {
        if(FAILED(g_DShowCaptureGraph.GetBaseFilter((void**)&pAudioCaptureFilter, AUDIO_CAPTURE_FILTER)))
        {
            RETAILMSG(1, ( TEXT("CameraDShowApp: Unable to retrieve the video capture filter.")));
            goto cleanup;
        }

        hr = pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, pAudioCaptureFilter, &rtStart, &rtStop, wAudioStartCookie, wAudioStopCookie);
        if(FAILED(hr))
        {
            RETAILMSG(1, ( TEXT("CameraDShowApp: Failed to stop the audio stream.")));
            goto cleanup;
        }
    }

    if(FAILED(ProcessEvents(TRUE, g_dwFilters & VIDEO_CAPTURE_FILTER, wVideoStopCookie, g_dwFilters & AUDIO_CAPTURE_FILTER, wAudioStopCookie)))
    {
        RETAILMSG(1, ( TEXT("CameraDShowApp: Failed to process events.")));
        goto cleanup;
    }

    Log(TEXT("%d frames processed"), g_nFramesProcessed);

    if(g_dwFilters & VIDEO_CAPTURE_FILTER)
    {
        LONGLONG llEnd = MAXLONGLONG;
        hr = pCaptureGraphBuilder->ControlStream( &PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, pVideoCaptureFilter, NULL, &llEnd, 0, 0 );
        if(FAILED(hr))
        {
            RETAILMSG(1, ( TEXT("CameraDShowApp: Failed to restart the preview stream.")));
        }
    }

cleanup:
    return hr;
}

LRESULT APIENTRY WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    RECT rc = {0, 0, 0, 0};
    int index;
    int nMaxWidth = 0;
    DWORD dwFiltersToUse = 0;
    HRESULT hr = S_OK;

    // get the client area, since it's used in so many different cases.
    GetClientRect(hwnd, &rc);

    switch(msg)
    {
        case WM_CREATE:
            // find the widest button
            for(index = 0; index < countof(gButtonInfo); index++)
                nMaxWidth = max(nMaxWidth, gButtonInfo[index].nButtonWidth + 2);

            rc.right -= 2;
            rc.bottom -= 2;
            rc.left = rc.right - nMaxWidth;

            // now create all of the buttons on the right side, the left edge matching the widest button's width.
            for(index = 0; index < countof(gButtonInfo); index++)
            {
                gButtonInfo[index].hwndButton = CreateWindow(TEXT("Button"), 
                                                                    gButtonInfo[index].tszButtonName,
                                                                    WS_CHILD | WS_VISIBLE | gButtonInfo[index].nButtonStyle,
                                                                    rc.left,
                                                                    rc.top,
                                                                    gButtonInfo[index].nButtonWidth,
                                                                    gButtonInfo[index].nButtonHeight,
                                                                    hwnd,
                                                                    (HMENU) index, // the index is the identifier for the command
                                                                    ghInstance,
                                                                    NULL);
                rc.top += gButtonInfo[index].nButtonHeight + 2;
            }

            // disable the pause button since we're not capturing.
            EnableWindow(gButtonInfo[BUTTON_PAUSECAPTURE].hwndButton, FALSE);

            // this gives us the leftover rectangle under the buttons (to display status in)
            g_rcStatusArea = rc;

            g_hwndEncodeStatus = CreateWindow(PROGRESS_CLASS, 
                                                                TEXT("%"),
                                                                WS_BORDER | WS_CHILD | WS_VISIBLE | PBS_SMOOTH | PBS_VERTICAL,
                                                                g_rcStatusArea.left + 2,
                                                                g_rcStatusArea.top,
                                                                (g_rcStatusArea.right - g_rcStatusArea.left) / 2 - 2,
                                                                g_rcStatusArea.bottom - g_rcStatusArea.top,
                                                                hwnd,
                                                                (HMENU) ENCODE_PROGRESS, // the index is the identifier for the command
                                                                ghInstance,
                                                                NULL);

            SendMessage(g_hwndEncodeStatus, PBM_SETPOS, 100, 0);

            g_hwndDroppedFrames = CreateWindow(PROGRESS_CLASS, 
                                                                TEXT("E"),
                                                                WS_BORDER | WS_CHILD | WS_VISIBLE | PBS_SMOOTH | PBS_VERTICAL,
                                                                g_rcStatusArea.left + ((g_rcStatusArea.right - g_rcStatusArea.left) / 2) + 2,
                                                                g_rcStatusArea.top,
                                                                (g_rcStatusArea.right - g_rcStatusArea.left) / 2 - 2,
                                                                g_rcStatusArea.bottom - g_rcStatusArea.top,
                                                                hwnd,
                                                                (HMENU) DROPPED_FRAMES, // the index is the identifier for the command
                                                                ghInstance,
                                                                NULL);

            SendMessage(g_hwndDroppedFrames, PBM_SETPOS, 0, 0);

            // set the initial window position to all 0's so the default position will be used.
            rc.top = 0;
            rc.left = 0;
            rc.right = 0;
            rc.bottom = 0;

            if(SUCCEEDED(g_DShowCaptureGraph.Init(hwnd, &rc, g_dwFilters )))
            {

                    hr = g_DShowCaptureGraph.SetStillCaptureFileName(g_tszStillFileName);
                    if(FAILED(hr))
                    {
                        RETAILMSG(1, ( TEXT("CameraDShowApp: Failed to set the still image file name.")));
                    }

                    hr = g_DShowCaptureGraph.SetVideoCaptureFileName(g_tszCaptureFileName);
                    if(FAILED(hr))
                    {
                        RETAILMSG(1, ( TEXT("CameraDShowApp: Failed to set the video capture file name.")));
                    }

                    hr = g_DShowCaptureGraph.RunGraph();
                    if(FAILED(hr))
                    {
                        RETAILMSG(1, ( TEXT("CameraDShowApp: Starting the capture graph failed.")));
                    }
            }
            else
            {
                RETAILMSG(1, ( TEXT("CameraDShowApp: Initializing the capture graph failed.")));
            }
            return 0;

        // handle the WM_SETTINGCHANGE message indicating rotation, move the window,
        // and tear down and rebuild the graph (as we can't dynamically renegotiate the
        // ASF mux connection to the encoder).
        case WM_SETTINGCHANGE:
            if(wParam == SETTINGCHANGE_RESET)
            {
                SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);
                MoveWindow(hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, FALSE);

                hr = g_DShowCaptureGraph.Cleanup();
                if(FAILED(hr))
                {
                    RETAILMSG(1, ( TEXT("CameraDShowApp: Failed to clean up the previously running capture graph.")));
                }

                // find the widest button so we know where to put the preview
                for(index = 0; index < countof(gButtonInfo); index++)
                    nMaxWidth = max(nMaxWidth, gButtonInfo[index].nButtonWidth + 2);

                // Initialize the graph with a video window position of all 0's, so that the default
                // width and height will be used without doing any preview streching or shrinking.
                rc.top = 0;
                rc.left = 0;
                rc.right = 0;
                rc.bottom = 0;

                if(SUCCEEDED(g_DShowCaptureGraph.Init(hwnd, &rc, g_dwFilters )))
                {

                    // reset the still capture file name because it needs to be propogated
                    // to the still image filter, but don't reset the video capture
                    // file name as it's persisted throught the cleanup by the test framework
                    hr = g_DShowCaptureGraph.SetStillCaptureFileName(g_tszStillFileName);
                    if(FAILED(hr))
                    {
                        RETAILMSG(1, ( TEXT("CameraDShowApp: Failed to set the still image file name.")));
                    }

                    hr = g_DShowCaptureGraph.RunGraph();
                    if(FAILED(hr))
                    {
                        RETAILMSG(1, ( TEXT("CameraDShowApp: Starting the capture graph failed.")));
                    }
                }
            }
            break;
        case WM_SIZE:
            // find the widest button
            for(index = 0; index < countof(gButtonInfo); index++)
                nMaxWidth = max(nMaxWidth, gButtonInfo[index].nButtonWidth + 2);

            rc.right -= 2;
            rc.bottom -= 2;
            rc.left = rc.right - nMaxWidth;

            // now create all of the buttons on the right side, the left edge matching the widest button's width.
            for(index = 0; index < countof(gButtonInfo); index++)
            {
                MoveWindow(gButtonInfo[index].hwndButton, 
                                    rc.right - nMaxWidth - 1,
                                    rc.top,
                                    gButtonInfo[index].nButtonWidth,
                                    gButtonInfo[index].nButtonHeight,
                                    FALSE);

                rc.top += gButtonInfo[index].nButtonHeight + 2;
            }

            g_rcStatusArea = rc;

            MoveWindow(g_hwndEncodeStatus,
                                g_rcStatusArea.left + 2,
                                g_rcStatusArea.top,
                                (g_rcStatusArea.right - g_rcStatusArea.left) / 2 - 2,
                                g_rcStatusArea.bottom - g_rcStatusArea.top,
                                FALSE);

            MoveWindow(g_hwndDroppedFrames,
                                g_rcStatusArea.left + ((g_rcStatusArea.right - g_rcStatusArea.left) / 2) + 2,
                                g_rcStatusArea.top,
                                (g_rcStatusArea.right - g_rcStatusArea.left) / 2 - 2,
                                g_rcStatusArea.bottom - g_rcStatusArea.top,
                                FALSE);

            // don't move the preview window, let it stay at it's default location.
            return 0;
        case WM_DESTROY:

            // cleanup the graph, then exit, this needs to happen before the window is destroyed to prevent asserts in the video renderer.
            hr = g_DShowCaptureGraph.Cleanup();
            if(FAILED(hr))
            {
                RETAILMSG(1, ( TEXT("CameraDShowApp: Failed to clean up the previously running capture graph.")));
            }
            PostQuitMessage(0);
            return 0;
        case WM_TIMER:
            hr = g_DShowCaptureGraph.GetCurrentPosition(&g_rtEndEncodePosition);
            if(FAILED(hr))
            {
                RETAILMSG(1, ( TEXT("CameraDShowApp: Failed to retrieve the current graph position.")));
            }

            if(FAILED(ProcessEvents(FALSE, FALSE, 0, FALSE, 0)))
            {
                RETAILMSG(1, ( TEXT("CameraDShowApp: Failed to process events.")));
            }
            return 0;
        case WM_COMMAND:
            int nCommand = LOWORD(wParam);
            if(nCommand >= 0 && nCommand < countof(gButtonInfo))
                RETAILMSG(1, ( TEXT("CameraDShowApp: %s pushed"), gButtonInfo[nCommand].tszButtonName));

            switch(nCommand)
            {
                case BUTTON_QUIT:

                    // cleanup the graph, then exit, this needs to happen before the window is destroyed to prevent asserts in the video renderer.
                    hr = g_DShowCaptureGraph.Cleanup();
                    if(FAILED(hr))
                    {
                        RETAILMSG(1, ( TEXT("CameraDShowApp: Failed to clean up the previously running capture graph.")));
                    }
                    PostQuitMessage(0);
                    break;
                case BUTTON_STILL:
                    // grab a new image
                    hr = g_DShowCaptureGraph.CaptureStillImage();
                    if(FAILED(hr))
                    {
                        RETAILMSG(1, ( TEXT("CameraDShowApp: Capturing a still image failed.")));
                    }
                    break;
                case BUTTON_STARTCAPTURE:
                    g_nFramesProcessed = 0;
                    g_nOOMEvents = 0;

                    hr = g_DShowCaptureGraph.GetCurrentPosition(&g_rtCaptureStart);
                    if(FAILED(hr))
                    {
                        RETAILMSG(1, ( TEXT("CameraDShowApp: Failed to retrieve the current graph position.")));
                    }

                    hr = g_DShowCaptureGraph.StartStreamCapture();
                    if(FAILED(hr))
                    {
                        RETAILMSG(1, ( TEXT("CameraDShowApp: Starting the video capture failed.")));
                    }

                    g_bCaptureActive = TRUE;
                    g_bCapturePaused = FALSE;

                    // enable the pause button since we're capturing.
                    EnableWindow(gButtonInfo[BUTTON_PAUSECAPTURE].hwndButton, TRUE);
                    SetTimer(hwnd, 1, 100, NULL);
                    break;
                case BUTTON_PAUSECAPTURE:

                    // if we're currently paused, then restart.
                    if(g_bCapturePaused)
                    {
                        hr = g_DShowCaptureGraph.StartStreamCapture();
                        if(FAILED(hr))
                        {
                            RETAILMSG(1, ( TEXT("CameraDShowApp: Resuming the capture failed.")));
                        }
                        g_bCapturePaused = FALSE;
                        SetWindowText(gButtonInfo[BUTTON_PAUSECAPTURE].hwndButton, TEXT("Pause Cap"));
                    }
                    else
                    {
                        hr = StopStreamCapture();
                        if(FAILED(hr))
                        {
                            RETAILMSG(1, ( TEXT("CameraDShowApp: pausing the capture failed.")));
                        }
                        g_bCapturePaused = TRUE;
                        SetWindowText(gButtonInfo[BUTTON_PAUSECAPTURE].hwndButton, TEXT("Resume Cap"));
                    }
                    break;
                case BUTTON_STOPCAPTURE:
                    KillTimer(hwnd, 1);

                    hr = StopStreamCapture();
                    if(FAILED(hr))
                    {
                        RETAILMSG(1, ( TEXT("Stopping the video capture failed.")));
                    }

                    if(FAILED(g_DShowCaptureGraph.StopGraph()))
                    {
                        FAIL(TEXT("Failed to stop the capture graph after the capture."));
                    }

                    SendMessage(g_hwndEncodeStatus, PBM_SETPOS, 100, 0);

                    Log(TEXT("Verifying the video file."));

                    if(FAILED(g_DShowCaptureGraph.VerifyVideoFileCaptured(NULL)))
                    {
                        RETAILMSG(1, (TEXT("The video file captured failed file conformance verification")));
                    }

                    if(FAILED(g_DShowCaptureGraph.RunGraph()))
                    {
                        FAIL(TEXT("Failed to stop the capture graph after the capture."));
                    }

                    g_bCaptureActive = FALSE;
                    g_bCapturePaused = FALSE;

                    // disable the pause button since we're not capturing any more
                    SetWindowText(gButtonInfo[BUTTON_PAUSECAPTURE].hwndButton, TEXT("Pause Cap"));
                    EnableWindow(gButtonInfo[BUTTON_PAUSECAPTURE].hwndButton, FALSE);
                    break;
                case BUTTON_STOP:
                    hr = g_DShowCaptureGraph.StopGraph();
                    if(FAILED(hr))
                    {
                        RETAILMSG(1, ( TEXT("CameraDShowApp: Pausing the graph failed.")));
                    }
                    break;
                case BUTTON_PAUSE:
                    hr = g_DShowCaptureGraph.PauseGraph();
                    if(FAILED(hr))
                    {
                        RETAILMSG(1, ( TEXT("CameraDShowApp: Pausing the graph failed.")));
                    }
                    break;
                case BUTTON_RUN:
                    hr = g_DShowCaptureGraph.RunGraph();
                    if(FAILED(hr))
                    {
                        RETAILMSG(1, ( TEXT("CameraDShowApp: Running the graph failed.")));
                    }
                    break;
                 case BUTTON_WAIT:
                    static BOOL buttonwait = FALSE;
                    static HCURSOR hCursor = NULL;

                    if(buttonwait)
                    {
                        SetCursor(hCursor);
                        buttonwait = FALSE;
                    }
                    else
                    {
                        hCursor = SetCursor(LoadCursor(0,IDC_WAIT));
                        buttonwait = TRUE;
                    }
                    break;
            }
            // handle other buttons.
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);

}


/****************************************************************************
    FontViewTest() - Window procedure for dialog box
******************************************************************************/
BOOL CALLBACK FormatSelectDialog(HWND hDlg, unsigned message, LONG wParam, LONG lParam)
{
    HWND    hID;
    BOOL    fRet = FALSE;
    TCHAR **ptszCameraDrivers;
    int nDriverCount;
    int iNum, nPos;

    switch (message) 
    {
        case WM_INITDIALOG:           
            if(FAILED(g_DShowCaptureGraph.GetDriverList(&ptszCameraDrivers, &nDriverCount)))
            {
                RETAILMSG(1, ( TEXT("CameraDShowApp: Retrieving the driver list failed.")));
            }

            // populate the preview combo box
            hID = GetDlgItem(hDlg, IDC_DRIVERSELECT);
            for(iNum = 0; iNum < nDriverCount; iNum++)
            {
                nPos = SendMessage(hID, CB_INSERTSTRING, -1, (LPARAM)ptszCameraDrivers[iNum]);
                SendMessage(hID, CB_SETITEMDATA, nPos, _wtol(ptszCameraDrivers[iNum]));
            }
            SendMessage(hID, CB_SETCURSEL, 0, 0);
            nPos = SendMessage(hID, CB_GETCURSEL, 0, 0);
            if(FAILED(g_DShowCaptureGraph.SelectCameraDevice( nPos )))
            {
                RETAILMSG(1, ( TEXT("CameraDShowApp: Selecting the camera device failed.")));
            }

            // grab and set the file names if they were given, before we close the dialog.
            hID = GetDlgItem(hDlg, IDC_STILLNAME);
            SetWindowText(hID, (LPTSTR)STILL_IMAGE_FILE_NAME);

            hID = GetDlgItem(hDlg, IDC_CAPTURENAME);
            SetWindowText(hID, (LPTSTR)VIDEO_CAPTURE_FILE_NAME);

            hID = GetDlgItem(hDlg, IDC_VCFCHECK);
            SendMessage(hID, BM_SETCHECK, BST_CHECKED, 0);

            hID = GetDlgItem(hDlg, IDC_VRCHECK);
            SendMessage(hID, BM_SETCHECK, BST_CHECKED, 0);

            hID = GetDlgItem(hDlg, IDC_STILLCHECK);
            SendMessage(hID, BM_SETCHECK, BST_CHECKED, 0);

            hID = GetDlgItem(hDlg, IDC_FILEMUXCHECK);
            SendMessage(hID, BM_SETCHECK, BST_CHECKED, 0);

            hID = GetDlgItem(hDlg, IDC_AUDIOCAPTURE);
            SendMessage(hID, BM_SETCHECK, BST_CHECKED, 0);

            hID = GetDlgItem(hDlg, IDC_VIDEOENCODER);
            SendMessage(hID, BM_SETCHECK, BST_CHECKED, 0);

            hID = GetDlgItem(hDlg, IDC_AUDIOENCODER);
            SendMessage(hID, BM_SETCHECK, BST_CHECKED, 0);

            fRet = TRUE;

            g_dwFilters = VIDEO_CAPTURE_FILTER | VIDEO_RENDERER | STILL_IMAGE_SINK | FILE_WRITER | AUDIO_CAPTURE_FILTER | VIDEO_ENCODER | AUDIO_ENCODER;
            break;
        case WM_COMMAND:
            switch(LOWORD(wParam)) 
            {
                case IDC_VCFCHECK:
                    hID = GetDlgItem(hDlg, IDC_VCFCHECK);
                    if(SendMessage(hID, BM_GETCHECK, 0, 0))
                        g_dwFilters |= VIDEO_CAPTURE_FILTER;
                    else
                        g_dwFilters &= ~VIDEO_CAPTURE_FILTER;
                    break;
                case IDC_VRCHECK:
                    hID = GetDlgItem(hDlg, IDC_VRCHECK);
                    if(SendMessage(hID, BM_GETCHECK, 0, 0))
                        g_dwFilters |= VIDEO_RENDERER;
                    else
                        g_dwFilters &= ~VIDEO_RENDERER;
                    break;
                case IDC_STILLCHECK:
                    hID = GetDlgItem(hDlg, IDC_STILLCHECK);
                    if(SendMessage(hID, BM_GETCHECK, 0, 0))
                        g_dwFilters |= STILL_IMAGE_SINK;
                    else
                        g_dwFilters &= ~STILL_IMAGE_SINK;
                    break;
                case IDC_FILEMUXCHECK:
                    hID = GetDlgItem(hDlg, IDC_FILEMUXCHECK);
                    if(SendMessage(hID, BM_GETCHECK, 0, 0))
                        g_dwFilters |= FILE_WRITER;
                    else
                        g_dwFilters &= ~FILE_WRITER;
                    break;
                case IDC_AUDIOCAPTURE:
                    hID = GetDlgItem(hDlg, IDC_AUDIOCAPTURE);
                    if(SendMessage(hID, BM_GETCHECK, 0, 0))
                        g_dwFilters |= AUDIO_CAPTURE_FILTER;
                    else
                        g_dwFilters &= ~AUDIO_CAPTURE_FILTER;
                    break;
                case IDC_VIDEOENCODER:
                    hID = GetDlgItem(hDlg, IDC_VIDEOENCODER);
                    if(SendMessage(hID, BM_GETCHECK, 0, 0))
                        g_dwFilters |= VIDEO_ENCODER;
                    else
                        g_dwFilters &= ~VIDEO_ENCODER;
                    break;
                case IDC_AUDIOENCODER:
                    hID = GetDlgItem(hDlg, IDC_AUDIOENCODER);
                    if(SendMessage(hID, BM_GETCHECK, 0, 0))
                        g_dwFilters |= AUDIO_ENCODER;
                    else
                        g_dwFilters &= ~AUDIO_ENCODER;
                    break;
                case IDC_VRINTELLICHECK:
                    hID = GetDlgItem(hDlg, IDC_VRINTELLICHECK);
                    if(SendMessage(hID, BM_GETCHECK, 0, 0))
                        g_dwFilters |= VIDEO_RENDERER_INTELI_CONNECT;
                    else
                        g_dwFilters &= ~VIDEO_RENDERER_INTELI_CONNECT;
                    break;
                case IDC_SIMULTCONTROL:
                    hID = GetDlgItem(hDlg, IDC_SIMULTCONTROL);
                    if(SendMessage(hID, BM_GETCHECK, 0, 0))
                        g_dwFilters |= OPTION_SIMULT_CONTROL;
                    else
                        g_dwFilters &= ~OPTION_SIMULT_CONTROL;
                    break;
                case IDC_DRIVERSELECT:
                    hID = GetDlgItem(hDlg, IDC_DRIVERSELECT);
                    nPos = SendMessage(hID, CB_GETCURSEL, 0, 0);
                    if(FAILED(g_DShowCaptureGraph.SelectCameraDevice( nPos )))
                    {
                        RETAILMSG(1, ( TEXT("CameraDShowApp: Selecting the camera device failed.")));
                    }
                    break;
                case IDC_STILLNAME:
                case IDC_CAPTURENAME:
                    break;
                case IDC_BUTTON1:
                    // grab and set the file names if they were given, before we close the dialog.
                    hID = GetDlgItem(hDlg, IDC_STILLNAME);
                    GetWindowText(hID, g_tszStillFileName, MAX_PATH);

                    hID = GetDlgItem(hDlg, IDC_CAPTURENAME);
                    GetWindowText(hID, g_tszCaptureFileName, MAX_PATH);

                    EndDialog(hDlg, TRUE);
                    fRet = TRUE;
                    break;
            }
            break;
        case WM_CLOSE:
            EndDialog(hDlg, TRUE);
            fRet = TRUE;
            break;
        default:
            break;
    }

    return (fRet);
}


int WINAPI WinMain(
    HINSTANCE hInstance, 
    HINSTANCE hPrevInstance, 
    LPWSTR lpszCmdLine,
    int nCmdShow)
{
    TCHAR szClassName[] = TEXT("Camera");

    HWND hwnd;
    MSG msg;
    WNDCLASS wc;

    InitCommonControls();

    memset(&msg, 0, sizeof(MSG));

    if(g_DShowCaptureGraph.ParseCommandLine(lpszCmdLine))
    {
        // initialize our camera driver, so we can select the driver in the dialog box.
        if(FAILED(g_DShowCaptureGraph.InitCameraDriver()))
        {
            RETAILMSG(1, ( TEXT("CameraDShowApp: Initializing the camera driver list failed.")));
        }

        DialogBox(hInstance, MAKEINTRESOURCE(IDD_FORMATSELECTPAGE), NULL, (DLGPROC)FormatSelectDialog);

        ghInstance = hInstance;

        memset(&wc, 0, sizeof(WNDCLASS));
        wc.lpfnWndProc = (WNDPROC) WndProc;
        wc.hInstance = ghInstance;
        wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
        wc.lpszClassName = szClassName;

        if (!RegisterClass(&wc))
            return 0;

        g_Hwnd = hwnd = CreateWindow( szClassName, TEXT("Camera Test Application"),
                                            WS_CAPTION | WS_CLIPCHILDREN | WS_VISIBLE | WS_SYSMENU | WS_THICKFRAME,
                                            CW_USEDEFAULT, CW_USEDEFAULT,
                                            CW_USEDEFAULT, CW_USEDEFAULT,
                                            NULL, NULL, ghInstance, NULL);

        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);
        SetForegroundWindow(hwnd);

        while(GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        UnregisterClass(szClassName, ghInstance);
    }

    return (msg.wParam);
}
