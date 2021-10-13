//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <windows.h>
#include <tchar.h>
#include <stressutils.h>

#define STRINGSIZE 5

HANDLE g_hInst = NULL;
HWND g_hWnd = NULL;
TCHAR g_tszClassName[] = TEXT("s2_eudc");

BOOL OpenWindow(void);
BOOL SetupEUDC(void);


///////////////////////////////////////////////////////////
VOID MessagePump(HWND hWnd = NULL)
{
	MSG msg;
	const MSGPUMP_LOOPCOUNT = 10;
	const MSGPUMP_LOOPDELAY = 50; // msec

	for (INT i = 0; i < MSGPUMP_LOOPCOUNT; i++)
	{
		while (PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		Sleep(MSGPUMP_LOOPDELAY);
	}
}


/////////////////////////////////////////////////////////////////////////////////////
BOOL InitializeStressModule (
                            /*[in]*/ MODULE_PARAMS* pmp,
                            /*[out]*/ UINT* pnThreads
                            )
{
    *pnThreads = 1;

    InitializeStressUtils (
                            _T("s2_eudc"),                                    // Module name to be used in logging
                            LOGZONE(SLOG_SPACE_GDI, SLOG_ALL),    // Logging zones used by default
                            pmp                                                // Forward the Module params passed on the cmd line
                            );

    BOOL fRet = TRUE;
    TCHAR tsz[MAX_PATH] = {NULL};
    int iSeed = GetTickCount();
    HDC hdc = GetDC(g_hWnd);

    if (pmp->tszUser && !_stscanf(pmp->tszUser, TEXT("%d"), &iSeed))
    {
        iSeed = GetTickCount();
    }
    srand(iSeed);


    GetModuleFileName((HINSTANCE) g_hInst, tsz, MAX_PATH-1);

    LogComment(_T("Module File Name: %s"), tsz);
    LogComment(TEXT("Using random seed %d"), iSeed);

    fRet = OpenWindow();

    fRet = fRet && SetupEUDC();

    return fRet;
}

BOOL OpenWindow(void)
{
    WNDCLASS  wc;

    wc.style = 0;
    wc.lpfnWndProc = (WNDPROC) DefWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = (HINSTANCE) g_hInst;
    wc.hIcon = 0;
    wc.hCursor = 0;
    wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = g_tszClassName;

    RegisterClass(&wc);
    RECT rcWorkArea;

    if (!SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0))
    {
        LogWarn1(_T("#### SystemParametersInfo(SPI_GETWORKAREA) failed - Error: 0x%x ####\r\n"),
            GetLastError());
        SetRect(&rcWorkArea, 0, 0, GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN));
    }

    g_hWnd = CreateWindowEx(
        0,
        g_tszClassName,
        g_tszClassName,
        WS_OVERLAPPED|WS_CAPTION|WS_BORDER|WS_VISIBLE,
        rcWorkArea.left + rand()%(3*(rcWorkArea.right - rcWorkArea.left)/4),
        rcWorkArea.top + rand() % (3*(rcWorkArea.bottom - rcWorkArea.top)/4),
        (rcWorkArea.right - rcWorkArea.left)/4,
        (rcWorkArea.bottom - rcWorkArea.top)/4,
        NULL,
        NULL,
        (HINSTANCE) g_hInst,
        NULL );
    if( g_hWnd == NULL )
        return FALSE;
    ShowWindow( g_hWnd, SW_SHOWNORMAL );
    UpdateWindow( g_hWnd );
    MessagePump();
    return TRUE;
}


////////////////////////////////////////////////////////////////////////
// SetupEUDC
//      This function sets up registry values that associate an EUDC
//      font file with a font face. Typically this will be
//          Tahoma=\release\test03.tte
//      However, if the system font is different (e.g. Tahoma isn't in
//      the image), this will still associate the EUDC font file with
//      the current system font. This should only be inappropriate with
//      an hlbase config.
BOOL SetupEUDC (void)
{
    HFONT hfont = NULL;
    LOGFONT logfont;
    TCHAR tszKey[] = TEXT("EUDC\\");
    TCHAR tszValue[LF_FACESIZE] = TEXT("");
    TCHAR tszEquals[] = TEXT("\\release\\test03.tte");
    HKEY hkey = NULL;
    DWORD dwDisposition = 0;
    LONG lRet;
    // now, set up the eudc
    EnableEUDC(FALSE);

    // Add HKCU\EUDC to the registry
    LogComment(TEXT("Adding Registry Key for EUDC"));
    lRet = RegCreateKeyEx(
                HKEY_CURRENT_USER,
                tszKey,
                0,
                TEXT(""), // lpClass, the class (object type) of the key
                0,
                0,
                NULL,
                &hkey,
                &dwDisposition);
    if (lRet != ERROR_SUCCESS)
    {
        LogFail(TEXT("RegCreateKeyEx returned %d; Cannot continue"), lRet);
        return FALSE;
    }

    // Here we are determining what the name is for the system font.
    // This is so we can be sure to use a font that is actually on the
    // system when we load our font.
    // We don't need to release the font since it is a stock object.
    hfont = (HFONT)GetStockObject(SYSTEM_FONT);
    GetObject(hfont, sizeof(logfont), (void*)&logfont);
    _tcscpy(tszValue, logfont.lfFaceName);

    lRet = RegSetValueEx(
                hkey,
                tszValue,
                0,
                REG_SZ,
                (LPBYTE) tszEquals,
                (_tcslen(tszEquals)+1)*sizeof(tszEquals[0]));
    if (lRet != ERROR_SUCCESS)
    {
        LogFail(TEXT("RegSetValueEx returned %d; cannot continue"), lRet);
        return FALSE;
    }

    RegCloseKey(hkey);
    LogComment(TEXT("Added key to registry: %s = %s"), tszValue, tszKey);
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////
UINT DoStressIteration (
                        /*[in]*/ HANDLE hThread,
                        /*[in]*/ DWORD dwThreadId,
                        /*[in,out]*/ LPVOID pv /*unused*/)
{
    //
    //        You can do whatever you want here, including calling other functions.
    //        Just keep in mind that this is supposed to represent one iteration
    //        of a stress test (with a single result), so try to do one discrete
    //        operation each time this function is called.


    HDC   hdc = NULL;
    LPBYTE lpByte;
    RECT  rc;
    HFONT hfont = NULL;
    HFONT hfontold = NULL;
    TCHAR szText[STRINGSIZE] = {NULL};
    LOGFONT logfont;
    UINT uiRet = CESTRESS_PASS;
    BOOL fClipped = FALSE;

    LogVerbose(TEXT("Enabling EUDC"));
    if (!EnableEUDC(TRUE))
    {
        LogFail(TEXT("EnableEUDC(TRUE) failed, GLE = %d"), GetLastError());
        uiRet = CESTRESS_FAIL;
        goto cleanup;
    }

    hdc = GetDC(g_hWnd);
    if (!hdc)
    {
        LogWarn1(TEXT("GetDC failed, GLE = %d"), GetLastError());
        uiRet = CESTRESS_WARN1;
        goto cleanup;
    }

    hfont = (HFONT)GetStockObject(SYSTEM_FONT);
    if (!hfont)
    {
        LogFail(TEXT("GetStockObject failed, GLE = %d"), GetLastError());
        uiRet = CESTRESS_FAIL;
        goto cleanup;
    }

    if (!GetObject(hfont,sizeof(logfont),&logfont))
    {
        LogFail(TEXT("GetObject failed, GLE = %d"), GetLastError());
        uiRet = CESTRESS_FAIL;
        goto cleanup;
    }
    logfont.lfHeight = rand()%40;
    logfont.lfWidth = 0;
    logfont.lfEscapement = rand() % 3600;

    logfont.lfOrientation = logfont.lfEscapement;

    LogVerbose(TEXT("Creating system font with Escapement = %d"), logfont.lfEscapement);
    hfont = CreateFontIndirect(&logfont);
    if (!hfont)
    {
        LogFail(TEXT("CreateFontIndirect failed, GLE = %d"), GetLastError());
        uiRet = CESTRESS_FAIL;
        goto cleanup;
    }

    hfont = (HFONT) SelectObject(hdc, hfont);
    if (!hfont)
    {
        LogFail(TEXT("SelectObject failed, GLE = %d"), GetLastError());
        uiRet = CESTRESS_FAIL;
        goto cleanup;
    }

    lpByte = (LPBYTE) szText;
    *lpByte++ = 0x00;  // first EUDC char in all test0x.tte
    *lpByte++  = 0xE0;
    *lpByte++ = 0x74;  // the one in test02.tte and  test03.tte
    *lpByte++ = 0xE0;

    GetClientRect(g_hWnd, &rc);
    rc.top = (rc.top + rc.bottom)/2;
    rc.left = (rc.left + rc.right)/2+50;

    fClipped = rand()%2;
    LogVerbose(TEXT("Drawing UDC's in middle of screen"));
    if (rand()%2)
    {
        LogVerbose(TEXT("Calling DrawText; clipping: %d"), !fClipped);
        if (!DrawText(hdc, szText, -1, &rc, DT_SINGLELINE | (DT_NOCLIP * fClipped)))
        {
            LogFail(TEXT("DrawText failed, GLE = %d"), GetLastError());
            uiRet = CESTRESS_FAIL;
            goto cleanup;
        }
    }
    else
    {
        LogVerbose(TEXT("Calling ExtTextOut; clipping: %d"), fClipped);
        rc.left -= 100;
        if (!ExtTextOut(hdc, rc.left, rc.top, ETO_CLIPPED * fClipped, &rc, szText, _tcslen(szText), NULL))
        {
            LogFail(TEXT("ExtTextOut failed, GLE = %d"), GetLastError());
            uiRet = CESTRESS_FAIL;
            goto cleanup;
        }
    }
    //Sleep(1000);

cleanup:
    if (hfont)
    {
        hfont = (HFONT) SelectObject(hdc, hfont);
        DeleteObject(hfont);
    }
    if (hdc)
    {
        ReleaseDC(g_hWnd, hdc);
    }
    EnableEUDC(FALSE);
    MessagePump();

    return uiRet;
}



/////////////////////////////////////////////////////////////////////////////////////
DWORD TerminateStressModule (void)
{
    if(!DestroyWindow(g_hWnd))
        LogFail(TEXT("Failed to destroy application window.  GLE = %d"), GetLastError());

    return ((DWORD) -1);
}



///////////////////////////////////////////////////////////
BOOL WINAPI DllMain(
                    HANDLE hInstance,
                    ULONG dwReason,
                    LPVOID lpReserved
                    )
{
    g_hInst = hInstance;
    return TRUE;
}
