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
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>

#include "resource.h"
#include "CameraDriverTest.h"

#define countof(x) (sizeof(x) / sizeof(x[0]))

static CCameraDriverTest g_CameraDriver;
static BOOL g_bPreviewAvailable = FALSE;
static BOOL g_bCaptureAvailable = FALSE;
static BOOL g_bStillAvailable = FALSE;
static HINSTANCE ghInstance = NULL;

int g_nCameraDriverIndex = 0;
int g_nPreviewFormatIndex = 0;
int g_nCaptureFormatIndex = 0;
int g_nStillFormatIndex = 0;

typedef struct _BUTTONINFO
{
    int nButtonStyle;
    TCHAR *tszButtonName;
    int nButtonState;
    HWND hwndButton;
    int nButtonWidth;
    int nButtonHeight;
} BUTTONINFO;

// These have been verified to work reasonably well on
// 176x220, 240x320, and higher resolutions.
#define BUTTON_WIDTH 60
#define BUTTON_HEIGHT 15
#define BUTTON_SPACING 2
#define WINDOW_SPACING 2

// this enum corresponds to the button entries.
enum
{
    COMMAND_QUIT,
    COMMAN_PREVIEWRUN,
    COMMAND_PREVIEWPAUSE,
    COMMAND_CAPTURERUN,
    COMMAND_CAPTUREPAUSE,
    COMMAND_STILL
};

BUTTONINFO gButtonInfo [] = 
{
    {BS_PUSHBUTTON, TEXT("Quit"), 0, NULL, BUTTON_WIDTH, BUTTON_HEIGHT },
    {BS_PUSHBUTTON, TEXT("P RUN"), 0, NULL, BUTTON_WIDTH, BUTTON_HEIGHT },
    {BS_PUSHBUTTON, TEXT("P PAUSE"), 0, NULL, BUTTON_WIDTH, BUTTON_HEIGHT },
    {BS_PUSHBUTTON, TEXT("C RUN"), 0, NULL, BUTTON_WIDTH, BUTTON_HEIGHT },
    {BS_PUSHBUTTON, TEXT("C PAUSE"), 0, NULL, BUTTON_WIDTH, BUTTON_HEIGHT },
    {BS_PUSHBUTTON, TEXT("Still"), 0, NULL, BUTTON_WIDTH, BUTTON_HEIGHT },
};

LRESULT APIENTRY WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    RECT rc;
    int index;
    int nMaxWidth = 0;
    CS_DATARANGE_VIDEO csFormat;

    GetClientRect(hwnd, &rc);

    switch(msg)
    {
        case WM_CREATE:
            // initialize our buttons.
            GetClientRect(hwnd, &rc);

            // find the widest button
            for(index = 0; index < countof(gButtonInfo); index++)
                nMaxWidth = max(nMaxWidth, gButtonInfo[index].nButtonWidth + BUTTON_SPACING);

            // now create all of the buttons on the right side, the left edge matching the widest button's width.
            for(index = 0; index < countof(gButtonInfo); index++)
            {
                gButtonInfo[index].hwndButton = CreateWindow(TEXT("Button"), 
                                                                    gButtonInfo[index].tszButtonName,
                                                                    WS_CHILD | WS_VISIBLE | gButtonInfo[index].nButtonStyle,
                                                                    rc.right - nMaxWidth - BUTTON_SPACING,
                                                                    rc.top,
                                                                    gButtonInfo[index].nButtonWidth,
                                                                    gButtonInfo[index].nButtonHeight,
                                                                    hwnd,
                                                                    (HMENU) index, // the index is the identifier for the command
                                                                    ghInstance,
                                                                    NULL);
                rc.top += gButtonInfo[index].nButtonHeight + BUTTON_SPACING;
            }

            if(FALSE == g_CameraDriver.DetermineCameraAvailability())
            {
                RETAILMSG(1, (TEXT( "CameraApp : Failed to find the camera driver")));
                PostQuitMessage(0);
                return 0;
            }

            if(FALSE == g_CameraDriver.SelectCameraDevice(g_nCameraDriverIndex))
            {
                RETAILMSG(1, (TEXT( "CameraApp : Failed to select the camera driver.")));
                PostQuitMessage(0);
                return 0;
            }

            if(FALSE == g_CameraDriver.InitializeDriver())
            {
                RETAILMSG(1, (TEXT( "CameraApp : Failed to select the camera driver.")));
                PostQuitMessage(0);
                return 0;
            }

            // our "preview" window takes 1/4 of our total area, capture takes 1/4, still takes 1/4, 
            // and the bottom left 1/4 is left empty.

            if(g_bPreviewAvailable)
            {

                RETAILMSG(1, (TEXT( "CameraApp : Creating preview stream")));

                if(FALSE == g_CameraDriver.GetFormatInfo(STREAM_PREVIEW, g_nPreviewFormatIndex, &csFormat))
                {
                    RETAILMSG(1, (TEXT( "CameraApp : GetFormatInfo on the preview failed")));
                    PostQuitMessage(0);
                    return 0;
                }

                // retrieve the client area, compensate for some spacing between the windows,
                // calculate the upper left quadrent of the window, compare the quadrent to the
                // supported video format, if the supported format is smaller than use it, otherwise
                // shrink the image to fit the quadrent.
                GetClientRect(hwnd, &rc);
                rc.top += WINDOW_SPACING;
                rc.left += WINDOW_SPACING;
                rc.bottom /= WINDOW_SPACING;
                rc.right = rc.right/2 - nMaxWidth/2 - WINDOW_SPACING;
                rc.bottom = min(rc.bottom, rc.top + csFormat.VideoInfoHeader.bmiHeader.biHeight);
                rc.right = min(rc.right, rc.left + csFormat.VideoInfoHeader.bmiHeader.biWidth);

                if(FALSE == g_CameraDriver.CreateStream(STREAM_PREVIEW, hwnd, rc))
                {
                    RETAILMSG(1, (TEXT( "CameraApp : CreateStream on the preview failed")));
                    PostQuitMessage(0);
                    return 0;
                }

                if(FALSE == g_CameraDriver.SelectVideoFormat(STREAM_PREVIEW, g_nPreviewFormatIndex))
                {
                    RETAILMSG(1, (TEXT( "CameraApp : SelectVideoFormat on the preview failed")));
                    PostQuitMessage(0);
                    return 0;
                }

                if(FALSE == g_CameraDriver.SetupStream(STREAM_PREVIEW))
                {
                    RETAILMSG(1, (TEXT( "CameraApp : SetupSTream on the preview failed")));
                    PostQuitMessage(0);
                    return 0;
                }

                if(FALSE == g_CameraDriver.SetState( STREAM_PREVIEW, CSSTATE_RUN))
                {
                    RETAILMSG(1, (TEXT( "CameraApp : SetState run on the preview failed")));
                    PostQuitMessage(0);
                    return 0;
                }
            }



            if(g_bCaptureAvailable)
            {
                RETAILMSG(1, (TEXT( "CameraApp : Creating capture stream")));

                if(FALSE == g_CameraDriver.GetFormatInfo(STREAM_CAPTURE, g_nCaptureFormatIndex, &csFormat))
                {
                    RETAILMSG(1, (TEXT( "CameraApp : GetFormatInfo on the capture failed")));
                    PostQuitMessage(0);
                    return 0;
                }

                // retrieve the client area, compensate for some spacing between the windows,
                // calculate the upper right quadrent of the window, compare the quadrent to the
                // supported video format, if the supported format is smaller than use it, otherwise
                // shrink the image to fit the quadrent.
                GetClientRect(hwnd, &rc);
                rc.top += WINDOW_SPACING;
                rc.left = rc.right/2 - nMaxWidth/2 + WINDOW_SPACING;
                rc.bottom /= 2;
                rc.right = rc.right - nMaxWidth - WINDOW_SPACING;
                rc.bottom = min(rc.bottom, rc.top + csFormat.VideoInfoHeader.bmiHeader.biHeight);
                rc.right = min(rc.right, rc.left + csFormat.VideoInfoHeader.bmiHeader.biWidth);

                if(FALSE == g_CameraDriver.CreateStream(STREAM_CAPTURE, hwnd, rc))
                {
                    RETAILMSG(1, (TEXT( "CameraApp : CreateStream on the capture failed")));
                    PostQuitMessage(0);
                    return 0;
                }

                if(FALSE == g_CameraDriver.SelectVideoFormat(STREAM_CAPTURE, g_nCaptureFormatIndex))
                {
                    RETAILMSG(1, (TEXT( "CameraApp : SelectVideoFormat on the capture failed")));
                    PostQuitMessage(0);
                    return 0;
                }

                if(FALSE == g_CameraDriver.SetupStream(STREAM_CAPTURE))
                {
                    RETAILMSG(1, (TEXT( "CameraApp : SetupSTream on the capture failed")));
                    PostQuitMessage(0);
                    return 0;
                }

                if(FALSE == g_CameraDriver.SetState( STREAM_CAPTURE, CSSTATE_RUN))
                {
                    RETAILMSG(1, (TEXT( "CameraApp : SetState run on the capture failed")));
                    PostQuitMessage(0);
                    return 0;
                }
            }


            if(g_bStillAvailable)
            {
                RETAILMSG(1, (TEXT( "CameraApp : Creating still window")));

                if(FALSE == g_CameraDriver.GetFormatInfo(STREAM_STILL, g_nStillFormatIndex, &csFormat))
                {
                    RETAILMSG(1, (TEXT( "CameraApp : GetFormatInfo on the still failed")));
                    PostQuitMessage(0);
                    return 0;
                }

                // retrieve the client area, compensate for some spacing between the windows,
                // calculate the lower right quadrent of the window, compare the quadrent to the
                // supported video format, if the supported format is smaller than use it, otherwise
                // shrink the image to fit the quadrent.
                GetClientRect(hwnd, &rc);
                rc.top = rc.bottom/2 + WINDOW_SPACING;
                rc.left = rc.right/2 - nMaxWidth/2 + WINDOW_SPACING;
                rc.bottom -= WINDOW_SPACING;
                rc.right = rc.right - nMaxWidth - WINDOW_SPACING;
                rc.bottom = min(rc.bottom, rc.top + csFormat.VideoInfoHeader.bmiHeader.biHeight);
                rc.right = min(rc.right, rc.left + csFormat.VideoInfoHeader.bmiHeader.biWidth);

                if(FALSE == g_CameraDriver.CreateStream(STREAM_STILL, hwnd, rc))
                {
                    RETAILMSG(1, (TEXT( "CameraApp : CreateStream on the still failed")));
                    PostQuitMessage(0);
                    return 0;
                }

                if(FALSE == g_CameraDriver.SelectVideoFormat(STREAM_STILL, g_nStillFormatIndex))
                {
                    RETAILMSG(1, (TEXT( "CameraApp : SelectVideoFormat on the still failed")));
                    PostQuitMessage(0);
                    return 0;
                }

                if(FALSE == g_CameraDriver.SetupStream(STREAM_STILL))
                {
                    RETAILMSG(1, (TEXT( "CameraApp : SetupSTream on the still failed")));
                    PostQuitMessage(0);
                    return 0;
                }
            }

            return 0;
        case WM_ACTIVATE:
            return 0;
        case WM_CHAR:
            if(wParam == 'q')
                DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if(g_bPreviewAvailable)
                g_CameraDriver.SetState( STREAM_PREVIEW, CSSTATE_STOP);
            if(g_bCaptureAvailable)
                g_CameraDriver.SetState( STREAM_CAPTURE, CSSTATE_STOP);
            if(g_bStillAvailable)
                g_CameraDriver.SetState( STREAM_STILL, CSSTATE_STOP);

            // since the inter-thread communication between the ccamerastreamtest thread
            // and this thread is asyncrnous, we can assume the stops are completed correctly.

            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            return 0;
        case WM_COMMAND:
            int nCommand = LOWORD(wParam);
            if(nCommand >= 0 && nCommand < countof(gButtonInfo))
                RETAILMSG(1, ( TEXT("CameraApp : %s pushed"), gButtonInfo[nCommand].tszButtonName));

            // these are all of the button commands enumerated above.
            switch(nCommand)
            {
                case COMMAND_QUIT:
                    PostQuitMessage(0);
                    break;
                case COMMAN_PREVIEWRUN:
                    if ( g_bPreviewAvailable && FALSE == g_CameraDriver.SetState( STREAM_PREVIEW, CSSTATE_RUN))
                    {
                        RETAILMSG(1, (TEXT( "CameraApp : SetState run on the preview failed")));
                        PostQuitMessage(0);
                        return 0;
                    }
                    break;
                case COMMAND_PREVIEWPAUSE:
                    if ( g_bPreviewAvailable && FALSE == g_CameraDriver.SetState( STREAM_PREVIEW, CSSTATE_PAUSE))
                    {
                        RETAILMSG(1, (TEXT( "CameraApp : SetState pause on the preview failed")));
                        PostQuitMessage(0);
                        return 0;
                    }
                    break;
                case COMMAND_CAPTURERUN:
                    if ( g_bCaptureAvailable && FALSE == g_CameraDriver.SetState( STREAM_CAPTURE, CSSTATE_RUN))
                    {
                        RETAILMSG(1, (TEXT( "CameraApp : SetState run on the capture failed")));
                        PostQuitMessage(0);
                        return 0;
                    }
                    break;
                case COMMAND_CAPTUREPAUSE:
                    if ( g_bCaptureAvailable && FALSE == g_CameraDriver.SetState( STREAM_CAPTURE, CSSTATE_PAUSE))
                    {
                        RETAILMSG(1, (TEXT( "CameraApp : SetState pause on the capture failed")));
                        PostQuitMessage(0);
                        return 0;
                    }
                    break;
                case COMMAND_STILL:
                    CSSTATE csStateCaptureState, csCstatePreviewState;

                    if(g_bCaptureAvailable)
                        csStateCaptureState = g_CameraDriver.GetState( STREAM_CAPTURE);

                    if(g_bPreviewAvailable)
                        csCstatePreviewState = g_CameraDriver.GetState( STREAM_PREVIEW);

                     if ( g_bCaptureAvailable && csStateCaptureState != CSSTATE_PAUSE && FALSE == g_CameraDriver.SetState( STREAM_CAPTURE, CSSTATE_PAUSE))
                    {
                        RETAILMSG(1, (TEXT( "CameraApp : Pausing capture failed")));
                        PostQuitMessage(0);
                        return 0;
                    }

                    if ( g_bPreviewAvailable && csCstatePreviewState != CSSTATE_PAUSE && FALSE == g_CameraDriver.SetState( STREAM_PREVIEW, CSSTATE_PAUSE))
                    {
                        RETAILMSG(1, (TEXT( "CameraApp : Pausing preview failed")));
                        PostQuitMessage(0);
                        return 0;
                    }

                    if ( g_bStillAvailable && FALSE == g_CameraDriver.TriggerCaptureEvent( STREAM_STILL))
                    {
                        RETAILMSG(1, (TEXT( "CameraApp : Running still failed")));
                        PostQuitMessage(0);
                        return 0;
                    }

                    if ( g_bPreviewAvailable && csCstatePreviewState != CSSTATE_PAUSE && FALSE == g_CameraDriver.SetState( STREAM_PREVIEW, csCstatePreviewState))
                    {
                        RETAILMSG(1, (TEXT( "CameraApp : Running preview failed")));
                        PostQuitMessage(0);
                        return 0;
                    }

                     if ( g_bCaptureAvailable && csStateCaptureState != CSSTATE_PAUSE && FALSE == g_CameraDriver.SetState( STREAM_CAPTURE, csStateCaptureState))
                    {
                        RETAILMSG(1, (TEXT( "CameraApp : Running capture failed")));
                        PostQuitMessage(0);
                        return 0;
                    }
                    break;
                default:
                    RETAILMSG(1, (TEXT( "CameraApp : Unhandled button command.")));
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
    int     iNum;
    BOOL    fRet = FALSE;
    int nPos;
    TCHAR **ptszCameraDrivers;
    int nDriverCount;
    TCHAR tszTempString[MAX_PATH];

    switch (message) 
    {
        case WM_INITDIALOG:           

            g_CameraDriver.DetermineCameraAvailability();

            g_CameraDriver.GetDriverList(&ptszCameraDrivers, &nDriverCount);

            // populate the preview combo box
            hID = GetDlgItem(hDlg, IDC_COMBO1);
            SendMessage(hID, CB_RESETCONTENT, 0, 0);

            for(iNum = 0; iNum < nDriverCount; iNum++)
            {
                nPos = SendMessage(hID, CB_INSERTSTRING, -1, (LPARAM)ptszCameraDrivers[iNum]);
            }
            SendMessage(hID, CB_SETCURSEL, 0, 0);
            g_nCameraDriverIndex = 0;

            hID = GetDlgItem(hDlg, IDC_COMBO2);
            SendMessage(hID, CB_RESETCONTENT, 0, 0);

            hID = GetDlgItem(hDlg, IDC_COMBO3);
            SendMessage(hID, CB_RESETCONTENT, 0, 0);

            hID = GetDlgItem(hDlg, IDC_COMBO4);
            SendMessage(hID, CB_RESETCONTENT, 0, 0);

            g_CameraDriver.Cleanup();

            fRet = TRUE;
            break;

        case WM_COMMAND:
            switch(LOWORD(wParam)) 
            {
                case IDC_COMBO1:
                {
                    g_CameraDriver.DetermineCameraAvailability();

                    hID = GetDlgItem(hDlg, IDC_COMBO1);
                    g_nCameraDriverIndex = SendMessage(hID, CB_GETCURSEL, 0, 0);
                    g_CameraDriver.SelectCameraDevice( g_nCameraDriverIndex );

                    if(FALSE == g_CameraDriver.InitializeDriver())
                    {
                        RETAILMSG(1, (TEXT( "CameraApp : Initializing the driver failed.")));
                        PostQuitMessage(0);
                        return 0;
                    }

                    g_bPreviewAvailable = g_CameraDriver.AvailablePinInstance(STREAM_PREVIEW);
                    g_bCaptureAvailable = g_CameraDriver.AvailablePinInstance(STREAM_CAPTURE);
                    g_bStillAvailable = g_CameraDriver.AvailablePinInstance(STREAM_STILL);

                    // reset the list box content for all of the mode selections
                    hID = GetDlgItem(hDlg, IDC_COMBO2);
                    SendMessage(hID, CB_RESETCONTENT, 0, 0);

                    hID = GetDlgItem(hDlg, IDC_COMBO3);
                    SendMessage(hID, CB_RESETCONTENT, 0, 0);

                    hID = GetDlgItem(hDlg, IDC_COMBO4);
                    SendMessage(hID, CB_RESETCONTENT, 0, 0);

                    // now that the driver is selected, we can populate the list boxes with the supported formats.

                    if(g_bPreviewAvailable)
                    {
                        // populate the preview combo box
                        hID = GetDlgItem(hDlg, IDC_COMBO2);
                        for(iNum = 0; iNum < g_CameraDriver.GetNumSupportedFormats(STREAM_PREVIEW); iNum++)
                        {
                            _itot(iNum, tszTempString, 10);
                            nPos = SendMessage(hID, CB_INSERTSTRING, -1, (LPARAM)tszTempString);
                        }
                        SendMessage(hID, CB_SETCURSEL, 0, 0);
                        g_nPreviewFormatIndex = SendMessage(hID, CB_GETCURSEL, 0, 0);
                    }

                    if(g_bCaptureAvailable)
                    {
                        // populate the capture combo box
                        hID = GetDlgItem(hDlg, IDC_COMBO3);
                        for(iNum = 0; iNum < g_CameraDriver.GetNumSupportedFormats(STREAM_CAPTURE); iNum++)
                        {
                            _itot(iNum, tszTempString, 10);
                            nPos = SendMessage(hID, CB_INSERTSTRING, -1, (LPARAM)tszTempString);
                        }
                        SendMessage(hID, CB_SETCURSEL, 0, 0);
                        g_nCaptureFormatIndex = SendMessage(hID, CB_GETCURSEL, 0, 0);
                    }

                    if(g_bStillAvailable)
                    {
                        // populate the still combo box
                        hID = GetDlgItem(hDlg, IDC_COMBO4);
                        for(iNum = 0; iNum < g_CameraDriver.GetNumSupportedFormats(STREAM_STILL); iNum++)
                        {
                            _itot(iNum, tszTempString, 10);
                            nPos = SendMessage(hID, CB_INSERTSTRING, -1, (LPARAM)tszTempString);
                        }
                        SendMessage(hID, CB_SETCURSEL, 0, 0);
                        g_nStillFormatIndex = SendMessage(hID, CB_GETCURSEL, 0, 0);
                    }
                    g_CameraDriver.Cleanup();

                    break;
                }

                case IDC_COMBO2:
                    hID = GetDlgItem(hDlg, IDC_COMBO2);
                    g_nPreviewFormatIndex = SendMessage(hID, CB_GETCURSEL, 0, 0);
                    break;
                case IDC_COMBO3:
                    hID = GetDlgItem(hDlg, IDC_COMBO3);
                    g_nCaptureFormatIndex = SendMessage(hID, CB_GETCURSEL, 0, 0);
                    break;
                case IDC_COMBO4:
                    hID = GetDlgItem(hDlg, IDC_COMBO4);
                    g_nStillFormatIndex = SendMessage(hID, CB_GETCURSEL, 0, 0);
                    break;

                // close the dialog.
                case IDC_BUTTON1:
                    EndDialog(hDlg, TRUE);
                    fRet = TRUE;
                    break;
            }
            break;
        // we've recieved a WM_CLOSE, so exit.
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

    memset(&msg, 0, sizeof(MSG));
        ghInstance = hInstance;


        DialogBox(hInstance, MAKEINTRESOURCE(IDD_FORMATSELECTPAGE), NULL, (DLGPROC)FormatSelectDialog);

        memset(&wc, 0, sizeof(WNDCLASS));
        wc.lpfnWndProc = (WNDPROC) WndProc;
        wc.hInstance = ghInstance;
        wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
        wc.lpszClassName = szClassName;

        if (!RegisterClass(&wc))
            return 0;

        hwnd = CreateWindow( szClassName, TEXT("Camera Test Application"),
                                            WS_CAPTION | WS_CLIPCHILDREN | WS_VISIBLE,
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

    return (msg.wParam);
}

void Log(LPWSTR szFormat, ...)
{
    va_list va;

    WCHAR wszInfo[1024];

    va_start(va, szFormat);
    StringCchVPrintf(wszInfo, _countof(wszInfo), szFormat, va);
    va_end(va);

    OutputDebugString(wszInfo);
}
