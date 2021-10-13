//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "font.h"

DWORD g_dwFontCount = 0;
HINSTANCE g_hInst = NULL;
HWND g_hWnd = NULL;
RECT g_rect;
int g_iSeed = 0;

// tszText contains the text that will be used to test the fonts.
// This is known to be a bad joke.
TCHAR tszText[TEXT_COUNT][100] =
{
    // The setup
    TEXT("How many surrealists does it take to change a lightbulb?"),

    // Two possible punchlines, one that's really short, and one that is long with linebreaks,
    // to test the word wrapping.
    TEXT("Three, two to hold up the giraffe \r\nand one to fill the \r\nbathtub with clocks."),
    TEXT("FISH!!!!")
};

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
    TCHAR tsz[MAX_PATH];

    *pnThreads = 2;
    InitializeStressUtils (
                            _T("s2_font"),	// Module name to be used in logging
                            LOGZONE(SLOG_SPACE_GDI, S2FONTZONES),	    // Logging zones used by default
                            pmp			    // Forward the Module params passed on the cmd line
                            );

    GetModuleFileName((HINSTANCE) g_hInst, tsz, MAX_PATH);
    tsz[MAX_PATH-1] = 0x00;
    LogComment(_T("Module File Name: %s"), tsz);

    g_iSeed = GetTickCount();
    if (pmp->tszUser)
    {
        _stscanf(pmp->tszUser, TEXT("%d"), &g_iSeed);
    }
    srand(g_iSeed);
    LogComment(TEXT("Using random seed %d"), g_iSeed);
    OpenWindow();
    g_dwFontCount = 0;
    HDC hdc = GetDC(g_hWnd);
    g_dwFontCount = EnumFontFamilies(hdc, NULL, (FONTENUMPROC)EnumerateFace, NULL);
    ReleaseDC(g_hWnd, hdc);
    LogComment(TEXT("Initialized: %d font families"), g_dwFontCount);
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////
BOOL OpenWindow(void)
{
    WNDCLASS  wc;
    TCHAR tszClassName[] = TEXT("s2_font");

    wc.style = 0;
    wc.lpfnWndProc = (WNDPROC) DefWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = (HINSTANCE) g_hInst;
    wc.hIcon = 0;
    wc.hCursor = 0;
    wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = tszClassName;

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
        tszClassName,
        tszClassName,
        WS_OVERLAPPED|WS_CAPTION|WS_BORDER|WS_VISIBLE,
        rcWorkArea.left + rand()%(2*(rcWorkArea.right - rcWorkArea.left)/3),
        rcWorkArea.top + rand() % (2*(rcWorkArea.bottom - rcWorkArea.top)/3),
        (rcWorkArea.right - rcWorkArea.left)/3,
        (rcWorkArea.bottom - rcWorkArea.top)/3,
        NULL,
        NULL,
        (HINSTANCE) g_hInst,
        NULL );
    if( g_hWnd == NULL )
    {
        LogFail(TEXT("UpdateWindow Failed, GLE = %d"));
        return FALSE;
    }

    // ShowWindow doesn't return an error.
    ShowWindow( g_hWnd, SW_SHOWNORMAL );

    if (!UpdateWindow( g_hWnd ))
    {
        LogFail(TEXT("UpdateWindow Failed, GLE = %d"));
        return FALSE;
    }
    MessagePump();
    if (!GetClientRect(g_hWnd, &g_rect))
    {
        LogFail(TEXT("GetClientRect Failed, GLE = %d"));
        return FALSE;
    }

    return TRUE;
}


/////////////////////////////////////////////////////////////////////////////////////
UINT DoStressIteration (
                        /*[in]*/ HANDLE hThread,
                        /*[in]*/ DWORD dwThreadId,
                        /*[in,out]*/ LPVOID pv /*unused*/)
{
    SCBParams params;
    RECT      rc;
    HDC       hdc = NULL;
    HFONT     hfont = NULL,
              hfontold = NULL;
    UINT      uiRet = CESTRESS_PASS;
    COLORREF  crTextColor,
              crBkColor;
    LogVerbose(TEXT("DoStressIteration: ThreadId = %d"), dwThreadId);
    hdc = GetDC(g_hWnd);

    hfont = GetRandomFont(hdc, &params);
    if (!hfont)
    {
        uiRet = CESTRESS_FAIL;
        goto LCleanup;
    }

    hfontold = (HFONT) SelectObject(hdc, hfont);
    if (!hfontold)
    {
        LogFail(TEXT("SelectObject(hfont) failed, GLE = %d"), GetLastError());
        LogFont(SLOG_FAIL, &params.logfont);
        uiRet = CESTRESS_FAIL;
        goto LCleanup;
    }

    // set colors
    crTextColor = RGB(rand()%0xff, rand()%0xff, rand()%0xff);
    if (CLR_INVALID == SetTextColor(hdc, crTextColor))
    {
        LogFail(TEXT("SetTextColor(0x%08X) failed, GLE = %d"), crTextColor, GetLastError());
        uiRet = CESTRESS_FAIL;
        goto LCleanup;
    }

    crBkColor = RGB(rand()%0xff, rand()%0xff, rand()%0xff);
    if (CLR_INVALID == SetBkColor(hdc, crBkColor))
    {
        LogFail(TEXT("SetBkColor(0x%08X) failed, GLE = %d"), crBkColor, GetLastError());
        uiRet = CESTRESS_FAIL;
        goto LCleanup;
    }

    rc.left = rand()%(g_rect.right - g_rect.left) + g_rect.left;
    rc.right = rc.left + rand()%(g_rect.right - rc.left);
    rc.top = rand()%(g_rect.bottom - g_rect.top - 1) + g_rect.top - 1;
    rc.bottom = rc.top + 1 + rand()%(g_rect.bottom - rc.top - 1);

    // Choose between DrawText and ExtTextOut
    if (RANDOM_CHOICE)
    {
        uiRet = CallExtTextOut(hdc, &rc, &params);
    }
    else
    {
        uiRet = CallDrawText(hdc, &rc, &params);
    }

LCleanup:
    if (hfont)
    {
        SelectObject(hdc, hfontold);
        DeleteObject(hfont);
    }
    if (hdc)
    {
        ReleaseDC(g_hWnd, hdc);
    }
    MessagePump();
    return uiRet;
}

UINT InitializeTestThread(HANDLE hThread, DWORD dwThreadId, int iIndex)
{
    srand(g_iSeed * (1 + iIndex));
    return CESTRESS_PASS;
}

/////////////////////////////////////////////////////////////////////////////////////
DWORD TerminateStressModule (void)
{
    if (g_hWnd)
    {
        DestroyWindow(g_hWnd);
    }
    return ((DWORD) -1);
}

/////////////////////////////////////////////////////////////////////////////////////
int CALLBACK EnumerateFace(ENUMLOGFONT *lpelf, const NEWTEXTMETRIC *lpntm,DWORD dwType, LPARAM lparam)
{
    // static DWORD s_dwFont = 0;
    static DWORD s_dwFontCount = 0;
    SCBParams * pParams = (SCBParams*) lparam;

    // If pParams is NULL, we are initializing the font count
    if (NULL == pParams)
    {
        s_dwFontCount++;
        return s_dwFontCount;
    }

    // otherwise we are choosing a random font.
    ASSERT(!IsBadWritePtr(pParams, sizeof(SCBParams)));
    ASSERT(pParams->dwCurrent < s_dwFontCount);
    if (pParams->dwCurrent == pParams->dwChosen)
    {
        // this is the one we want
        memcpy ((void*)&(pParams->logfont), lpelf, sizeof(LOGFONT));
        return 0;
    }

    // We are going to the next font face.
    pParams->dwCurrent++;
    return 1;
}

/////////////////////////////////////////////////////////////////////////////////////
HFONT GetRandomFont (HDC hdc, SCBParams* pParams)
{
    HFONT hfont = NULL;

    // Choose the font we are going to use
    pParams->dwChosen = rand() % g_dwFontCount;
    pParams->dwCurrent = 0;
    EnumFontFamilies(hdc, NULL, (FONTENUMPROC)EnumerateFace, (LPARAM)pParams);

    // set random size
    pParams->logfont.lfHeight = rand() % (MAXHEIGHT * 2) - MAXHEIGHT;
    pParams->logfont.lfWidth = rand() % MAXWIDTH;

    // set random orientation
    pParams->logfont.lfEscapement = pParams->logfont.lfOrientation = rand() % 3600;

    // set random weight
    pParams->logfont.lfWeight = rand() % 1000;

    // set random attributes
    pParams->logfont.lfItalic = RANDOM_CHOICE;
    pParams->logfont.lfUnderline = RANDOM_CHOICE;
    pParams->logfont.lfStrikeOut = RANDOM_CHOICE;

    // set clear type option
    switch (rand()%3)
    {
    case 0:
        pParams->logfont.lfQuality = DEFAULT_QUALITY;
        break;
    case 1:
        pParams->logfont.lfQuality = CLEARTYPE_QUALITY;
        break;
    case 2:
        pParams->logfont.lfQuality = CLEARTYPE_COMPAT_QUALITY;
        break;
    }

    // Log the font that we're using.
    LogFont(SLOG_VERBOSE, &pParams->logfont);

    // create the font and select it in.
    hfont = CreateFontIndirect(&pParams->logfont);
    if (!hfont)
    {
        LogFail(TEXT("Could not create font, GLE = %d"), GetLastError());
        LogFont(SLOG_FAIL, &pParams->logfont);
    }
    return hfont;
}

/////////////////////////////////////////////////////////////////////////////////////
UINT CallExtTextOut(HDC hdc, RECT* prc, SCBParams* pParams)
{
    // ExtTextOut
    UINT uiOptions = 0;
    int iX, iY;
    int iIndex = rand()%TEXT_COUNT;

    UINT uiAlign = 0;
    uiAlign |= RANDOM_CHOICE ? TA_TOP : TA_BOTTOM;
    uiAlign |= RANDOM_CHOICE ? TA_UPDATECP : TA_NOUPDATECP;

    switch (rand()%3)
    {
    case 0 :
        uiAlign |= TA_RIGHT;
        break;
    case 1 :
        uiAlign |= TA_CENTER;
        break;
    case 2 :
        uiAlign |= TA_LEFT;
        break;
    }

    if (RANDOM_CHOICE)
        uiAlign |= TA_BASELINE;

    if (SetTextAlign(hdc, uiAlign) == GDI_ERROR)
    {
        LogFail(TEXT("SetTextAlign failed, GLE: %d"), GetLastError());
        LogFont(SLOG_FAIL, &pParams->logfont);
        return CESTRESS_FAIL;
    }

    iX = rand()%(g_rect.right - g_rect.left) + g_rect.left;
    iY = rand()%(g_rect.bottom - g_rect.top) + g_rect.top;

    if (RANDOM_CHOICE)
        uiOptions |= ETO_CLIPPED;
    if (RANDOM_CHOICE)
        uiOptions |= ETO_OPAQUE;

    if (!ExtTextOut(hdc, iX, iY, uiOptions, prc, tszText[iIndex], _tcslen(tszText[iIndex]), NULL))
    {
        LogFail(TEXT("ExtTextOut failed, GLE: %d"), GetLastError());
        LogFont(SLOG_FAIL, &pParams->logfont);
        return CESTRESS_FAIL;
    }
    return CESTRESS_PASS;
}

/////////////////////////////////////////////////////////////////////////////////////
UINT CallDrawText(HDC hdc, RECT* prc, SCBParams* pParams)
{
    UINT uiFormat = 0;
    int iIndex = rand()%TEXT_COUNT;

    // shall we do single line?
    if (RANDOM_CHOICE)
    {
        uiFormat |= DT_SINGLELINE;

        // the following options are only valid if uiFormat is SingleLine
        switch (rand()%3)
        {
        case 0 :
            uiFormat |= DT_TOP;
            break;
        case 1 :
            uiFormat |= DT_VCENTER;
            break;
        case 2 :
            uiFormat |= DT_BOTTOM;
            break;
        }
        switch (rand()%3)
        {
        case 0 :
            uiFormat |= DT_LEFT;
            break;
        case 1 :
            uiFormat |= DT_CENTER;
            break;
        case 2 :
            uiFormat |= DT_RIGHT;
            break;
        }
    }
    else
    {
        // We are not doing single line. Shall we break at words,
        // or only at line breaks?
        if (RANDOM_CHOICE)
            uiFormat |= DT_WORDBREAK;
    }

    if(!DrawText(hdc, tszText[iIndex], -1, prc, uiFormat))
    {
        LogFail(TEXT("DrawText failed, GLE: %d"), GetLastError());
        LogFont(SLOG_FAIL, &pParams->logfont);
        LogFail(TEXT("top: %d; bottom: %d; left: %d; right: %d"),
            prc->top, prc->bottom, prc->left, prc->right);
        return CESTRESS_FAIL;
    }
    return CESTRESS_PASS;
}

/////////////////////////////////////////////////////////////////////////////////////
void LogFont(DWORD dwLevel, LOGFONT* plogfont)
{
    // here we spit out all the information about the font we are displaying
    LogEx(dwLevel, S2FONTZONES, TEXT("Font Face: %s; Height: %d; Width: %d"),
        plogfont->lfFaceName, plogfont->lfHeight, plogfont->lfWidth);
    LogEx(dwLevel, S2FONTZONES, TEXT("Orientation: %d; Weight: %d; Italic: %d; Underline: %d"),
        plogfont->lfOrientation, plogfont->lfWeight, plogfont->lfItalic, plogfont->lfUnderline);
    LogEx(dwLevel, S2FONTZONES, TEXT("StrikeOut: %d; CharSet: %d; OutPrecision: %d; ClipPrecision: %d"),
        plogfont->lfStrikeOut, plogfont->lfCharSet, plogfont->lfOutPrecision, plogfont->lfClipPrecision);
    LogEx(dwLevel, S2FONTZONES, TEXT("Quality: %d; PitchAndFamily: %d"),
        plogfont->lfQuality, plogfont->lfPitchAndFamily);
}

///////////////////////////////////////////////////////////
BOOL WINAPI DllMain(
                    HANDLE hInstance,
                    ULONG dwReason,
                    LPVOID lpReserved
                    )
{
    g_hInst = (HINSTANCE) hInstance;
    return TRUE;
}
