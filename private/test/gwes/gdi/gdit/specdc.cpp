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

/**************************************************************************


Module Name:

   specdc.cpp

Abstract:

   Gdi Tests:

***************************************************************************/

#include "global.h"

HINSTANCE globalInst;
static int windRef;
float   adjustRatio = 1;
int     gMinWinWidth = 0;
int     gMinWinHeight = 0;
RECT    grcToUse;
RECT    grcInUse;
RECT    grcWorkArea;
BYTE *glpBits = NULL;
struct MYBITMAPINFO gBitmapInfo;
int gBitmapType;

DWORD gdwRedBitMask = 0x00FF0000;
DWORD gdwGreenBitMask = 0x0000FF00;
DWORD gdwBlueBitMask = 0x000000FF;

DWORD gdwShiftedRedBitMask = 0x00FF0000;
DWORD gdwShiftedGreenBitMask = 0x0000FF00;
DWORD gdwShiftedBlueBitMask = 0x000000FF;

extern BOOL g_fUseGetDC;
extern BOOL g_fUsePrimaryDisplay;
extern TCHAR *gszDisplayPath;

/***********************************************************************************
***
***   Memory DC Support
***
************************************************************************************/

static TBITMAP stockBmp = NULL;

//***********************************************************************************
TDC doMemDCCreate(void)
{
    BOOL    bVerify;
    TDC     tempDC,
            hdc = NULL;
    TBITMAP hBmp = NULL;
    glpBits = NULL;
    gBitmapType = DIB_RGB_COLORS;

    // set our origin to (0,0) since offscreen surfaces don't depend on the workarea
    grcInUse.right = grcToUse.right - grcToUse.left;
    grcInUse.bottom = grcToUse.bottom - grcToUse.top;
    grcInUse.left = 0;
    grcInUse.top = 0;

    tempDC = myGetDC(NULL);
    if (tempDC)
    {
        hdc = CreateCompatibleDC(tempDC);

        if (hdc)
        {
            switch(DCFlag)
            {
                case EGDI_VidMemory:
                    hBmp = CreateCompatibleBitmap(tempDC, zx, zy);
                    break;
                case EGDI_SysMemory:
                    hBmp = myCreateBitmap(zx, zy, 1, GetDeviceCaps(tempDC, BITSPIXEL), NULL);
                    break;
                case EGDI_1bppBMP:
                    hBmp = myCreateBitmap(zx, zy, 1, 1, NULL);
                    break;
                case EGDI_2bppBMP:
                    hBmp = myCreateBitmap(zx, zy, 1, 2, NULL);
                    break;
                case EGDI_4bppBMP:
                    hBmp = myCreateBitmap(zx, zy, 1, 4, NULL);
                    break;
                case EGDI_8bppBMP:
                    hBmp = myCreateBitmap(zx, zy, 1, 8, NULL);
                    break;
                case EGDI_16bppBMP:
                    hBmp = myCreateBitmap(zx, zy, 1, 16, NULL);
                    break;
                case EGDI_24bppBMP:
                    hBmp = myCreateBitmap(zx, zy, 1, 24, NULL);
                    break;
                case EGDI_32bppBMP:
                    hBmp = myCreateBitmap(zx, zy, 1, 32, NULL);
                    break;
                // DIB's with a negative height are top down, with a positive height are bottom up.
               case EGDI_8bppPalDIB:
                    gBitmapType = DIB_PAL_COLORS;
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & glpBits, 8, zx, -zy, DIB_PAL_COLORS, &gBitmapInfo, FALSE);
                    break;
                case EGDI_1bppRGBDIB:
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & glpBits, 1, zx, -zy, DIB_RGB_COLORS, &gBitmapInfo, FALSE);
                    break;
                case EGDI_2bppRGBDIB:
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & glpBits, 2, zx, -zy, DIB_RGB_COLORS, &gBitmapInfo, FALSE);
                    break;
                case EGDI_4bppRGBDIB:
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & glpBits, 4, zx, -zy, DIB_RGB_COLORS, &gBitmapInfo, FALSE);
                    break;
                case EGDI_8bppRGBDIB:
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & glpBits, 8, zx, -zy, DIB_RGB_COLORS, &gBitmapInfo, FALSE);
                    break;
                case EGDI_16bppRGBDIB:
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & glpBits, 16, zx, -zy, DIB_RGB_COLORS, &gBitmapInfo, FALSE);
                    break;
                case EGDI_16bppRGB4444DIB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 16, zx, -zy, RGB4444, BI_ALPHABITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_16bppRGB1555DIB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 16, zx, -zy, RGB1555, BI_ALPHABITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_16bppRGB555DIB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 16, zx, -zy, RGB555, BI_BITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_16bppRGB565DIB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 16, zx, -zy, RGB565, BI_BITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_16bppBGR4444DIB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 16, zx, -zy, BGR4444, BI_ALPHABITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_16bppBGR1555DIB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 16, zx, -zy, BGR1555, BI_ALPHABITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_16bppBGR555DIB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 16, zx, -zy, BGR555, BI_BITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_16bppBGR565DIB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 16, zx, -zy, BGR565, BI_BITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_24bppRGBDIB:
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & glpBits, 24, zx, -zy, DIB_RGB_COLORS, &gBitmapInfo, FALSE);
                    break;
                case EGDI_32bppRGBDIB:
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & glpBits, 32, zx, -zy, DIB_RGB_COLORS, &gBitmapInfo, FALSE);
                    break;
                case EGDI_32bppRGB8888DIB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 32, zx, -zy, RGB8888, BI_ALPHABITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_32bppRGB888DIB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 32, zx, -zy, RGB888, BI_BITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_32bppBGR8888DIB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 32, zx, -zy, BGR8888, BI_ALPHABITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_32bppBGR888DIB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 32, zx, -zy, BGR888, BI_BITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_8bppPalDIBBUB:
                    gBitmapType = DIB_PAL_COLORS;
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & glpBits, 8, zx, zy, DIB_PAL_COLORS, &gBitmapInfo, FALSE);
                    break;
                case EGDI_1bppRGBDIBBUB:
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & glpBits, 1, zx, zy, DIB_RGB_COLORS, &gBitmapInfo, FALSE);
                    break;
                case EGDI_2bppRGBDIBBUB:
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & glpBits, 2, zx, zy, DIB_RGB_COLORS, &gBitmapInfo, FALSE);
                    break;
                case EGDI_4bppRGBDIBBUB:
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & glpBits, 4, zx, zy, DIB_RGB_COLORS, &gBitmapInfo, FALSE);
                    break;
                case EGDI_8bppRGBDIBBUB:
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & glpBits, 8, zx, zy, DIB_RGB_COLORS, &gBitmapInfo, FALSE);
                    break;
                case EGDI_16bppRGBDIBBUB:
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & glpBits, 16, zx, zy, DIB_RGB_COLORS, &gBitmapInfo, FALSE);
                    break;
                case EGDI_16bppRGB4444DIBBUB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 16, zx, zy, RGB4444, BI_ALPHABITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_16bppRGB1555DIBBUB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 16, zx, zy, RGB1555, BI_ALPHABITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_16bppRGB555DIBBUB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 16, zx, zy, RGB555, BI_BITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_16bppRGB565DIBBUB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 16, zx, zy, RGB565, BI_BITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_16bppBGR4444DIBBUB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 16, zx, zy, BGR4444, BI_ALPHABITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_16bppBGR1555DIBBUB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 16, zx, zy, BGR1555, BI_ALPHABITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_16bppBGR555DIBBUB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 16, zx, zy, BGR555, BI_BITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_16bppBGR565DIBBUB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 16, zx, zy, BGR565, BI_BITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_24bppRGBDIBBUB:
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & glpBits, 24, zx, zy, DIB_RGB_COLORS, &gBitmapInfo, FALSE);
                    break;
                case EGDI_32bppRGBDIBBUB:
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & glpBits, 32, zx, zy, DIB_RGB_COLORS, &gBitmapInfo, FALSE);
                    break;
                case EGDI_32bppRGB8888DIBBUB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 32, zx, zy, RGB8888, BI_ALPHABITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_32bppRGB888DIBBUB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 32, zx, zy, RGB888, BI_BITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_32bppBGR8888DIBBUB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 32, zx, zy, BGR8888, BI_ALPHABITFIELDS, &gBitmapInfo);
                    break;
                case EGDI_32bppBGR888DIBBUB:
                    hBmp = myCreateRGBDIBSection(tempDC, (VOID **) & glpBits, 32, zx, zy, BGR888, BI_BITFIELDS, &gBitmapInfo);
                    break;

            }

            if (hBmp)
            {
                stockBmp = (TBITMAP) SelectObject(hdc, hBmp);

                if(stockBmp == NULL)
                {
                    info(ABORT, TEXT("Failed to select the bitmap into the DC"));
                    DeleteObject(hBmp);
                }
            }
            else
            {
                info(ABORT, TEXT("doMemDCCreate: CreateCompatibleBitmap Failure(%d %d)"), zx, zy);
                DeleteDC(hdc);
                hdc = NULL;
            }
        }
        else
        {
            info(ABORT, TEXT("doMemDCCreate: CreateCompatibleDC Failure(%d %d)"), zx, zy);
        }

        // turn off verification when releasing the tempdc, we didn't blt to it, so it will fail.
        bVerify = SetSurfVerify(FALSE);
        myReleaseDC(NULL, tempDC);
        SetSurfVerify(bVerify);
    }

    return hdc;
}

//***********************************************************************************
BOOL doMemDCRelease(TDC hdc)
{
    TBITMAP hBmp = (TBITMAP) SelectObject(hdc, stockBmp);

    if ((TBITMAP)GDI_ERROR != hBmp)
    {
        DeleteObject(hBmp);
    }

    return DeleteDC(hdc);
}

/***********************************************************************************
***
***   Window Support
***
************************************************************************************/
HWND    myHwnd = NULL;
LPCTSTR myAtom;

//***********************************************************************************
void
setGlobalClass(void)
{
    WNDCLASS wc;
    static  i;
    TCHAR  *pszStyle = TEXT("???");
    TCHAR   szBuf[6] = {NULL};

    _sntprintf_s(szBuf, countof(szBuf) - 1, TEXT("%d"), i++);  // This will give us a unique ID each time

    memset(&wc, 0, sizeof (WNDCLASS));
    wc.lpfnWndProc = (WNDPROC) DefWindowProc;   // Window Procedure
    wc.cbWndExtra = 8;
    wc.hInstance = globalInst;  // Owner of this class
    wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1); // Default color
    wc.lpszClassName = szBuf;
    wc.style = CS_GLOBALCLASS;
    pszStyle = TEXT("Global Class");

    myAtom = (LPCTSTR) RegisterClass(&wc);
    if (!myAtom)
        info(FAIL, TEXT("MakeMeAClass failed GLE:%d"), GetLastError());

}

//***********************************************************************************
void
pumpMessages(void)
{
    MSG     msg;

    for (UINT i=0; i<5; ++i)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // allow incoming messages to be received while we are processing
        Sleep(10);
    }
}

//***********************************************************************************
TDC doWindowCreate(void)
{
    RECT rcPosition;
    DWORD dwWinStyle;
    int nMonitors = GetSystemMetrics(SM_CMONITORS);

    rcPosition.top = 0;
    rcPosition.left = 0;

    // if the user requested a screen that's larger than the primary when we're using the primary
    // use the full primary.
    if((grcInUse.right - grcInUse.left) > (grcWorkArea.right - grcWorkArea.left) ||
        (grcInUse.bottom - grcInUse.top) > (grcWorkArea.bottom - grcWorkArea.top))
        rcPosition = grcWorkArea;
    // gMinWinWidth/gMinWinHeight can be set to be fullscreen, in which case
    // the sizing and positioning are set.
    else if((grcInUse.right - grcInUse.left) > gMinWinWidth &&
                (grcInUse.bottom - grcInUse.top) > gMinWinHeight)
    {
        // divided/multipled by 4 to make the window divisable by 4, for some tests
        // subtracted by width/4, increased by width/4 to make the minimum size 1/2 of the screen (dont' want to go too small).
        rcPosition.right = (((GenRand() % (grcInUse.right - grcInUse.left - gMinWinWidth)) + gMinWinWidth)/4) *4;
        rcPosition.bottom = (GenRand() % (grcInUse.bottom - grcInUse.top - gMinWinHeight)) + gMinWinHeight;

        // make the top left corner somewhere inside the desktop
        rcPosition.left = (GenRand()%(grcInUse.right - grcInUse.left - rcPosition.right)) + grcInUse.left;
        rcPosition.top = (GenRand()%(grcInUse.bottom - grcInUse.top - rcPosition.bottom)) + grcInUse.top;

        // reset the width/height to compensate for a non-zero position
        // bottom right should still be within the primary due to the positioning of the top left within the
        // top left of the unused area on the screen.
        rcPosition.right += rcPosition.left;
        rcPosition.bottom += rcPosition.top;
    }
    else rcPosition = grcInUse;

    // if we're on a multimon system, pick a random monitor.
    // NOTENOTE: this assumes that each monitor on the system is the same width.
    // if we're spread across monitors, then this doesn't apply.
    if(nMonitors > 1 && !g_bMonitorSpread)
    {
        int nMonitorChosen;
        int nXOffset;

        // if the user requested a specific monitor (monitor 1, 2, etc), use it.
        // nMonitorChosen is 0 based, so the primary is monitor 0.
        if(gMonitorToUse > (DWORD) 0 && gMonitorToUse <= (DWORD) nMonitors)
            nMonitorChosen = (gMonitorToUse - 1);
        // otherwise pick a random monitor. (0 based)
        else nMonitorChosen = GenRand() % nMonitors;

        // figure out the left edge of the monitor chosen
        nXOffset = (nMonitorChosen * (grcWorkArea.right - grcWorkArea.left));
        // offset the random window to the monitor.
        rcPosition.left += nXOffset;
        rcPosition.right += nXOffset;
    }

    dwWinStyle = WS_POPUP | WS_VISIBLE;

    if(GenRand() % 2)
        dwWinStyle |= WS_BORDER;
    if(GenRand() % 2)
        dwWinStyle |= WS_CAPTION;
    if(GenRand() % 2)
        dwWinStyle |= WS_HSCROLL;
    if(GenRand() % 2)
        dwWinStyle |= WS_THICKFRAME;
    if(GenRand() % 2)
        dwWinStyle |= WS_SYSMENU;
    if(GenRand() % 2)
        dwWinStyle |= WS_VSCROLL;

    // this will adjust the top, left, bottom, and right to compensate for the outside of the window, so we may have
    // a negative top left, that just means the borders and title bar will be off the edge of the screen.
    // we also may have a bottom right that's greater than the work area, that means that the bottom right
    // of the borders will be off the screen.  The actual work area should never go offscreen, that will cause test failures
    DWORD dwExtStyle = NULL;
    if(g_bRTL)
        dwExtStyle = WS_EX_LAYOUTRTL;
    AdjustWindowRectEx(&rcPosition, dwWinStyle, FALSE, dwExtStyle);

    if (windRef++ == 0)
    {
        setGlobalClass();
        myHwnd = CreateWindow(myAtom, TEXT("GDI"), dwWinStyle | WS_CHILD, rcPosition.left, rcPosition.top, rcPosition.right - rcPosition.left, rcPosition.bottom - rcPosition.top, ghwndTopMain, NULL, globalInst, NULL);

        if (!myHwnd)
            info(ABORT, TEXT("CreateWindow failed GLE:%d"), GetLastError());

// NT does not support the window compositor APIs for flags
#ifdef UNDER_CE 
        // the verification driver requires our test HWND to have the same bitdepth as our
        // our display driver under test. if the compositor is in use, increase the format
        // of our test HWND by requesting WCF_TRUECOLOR.
        if (IsCompositorEnabled())
        {
            HDC hdc = GetDC(myHwnd);
            if (!hdc)
                info(ABORT, TEXT("GetDC(NULL) failed GLE: %d"), GetLastError());
            else
            {
                if ((int)GetBackbufferBPP() < GetDeviceCaps(hdc, BITSPIXEL))
                    // set our backbuffer format to A8R8G8B8
                    if (!SetWindowCompositionFlags(myHwnd, WCF_TRUECOLOR))
                        info(ABORT, TEXT("SetWindowCompositionFlags failed GLE:%d"), GetLastError());
            
                ReleaseDC(myHwnd, hdc);
            }
        }
#endif 
    }

    // set the width and height of the rectangle for the rest of the test to scale to.
    GetClientRect(myHwnd, &grcInUse);

    // screen has to be divisable by 2.
    grcInUse.right = grcInUse.right/2 * 2;

    if (g_bRTL)  // if RTL mirror test then flip window
    {
        LONG lExStyles = GetWindowLong(myHwnd, GWL_EXSTYLE);
        lExStyles |= WS_EX_LAYOUTRTL;
//        lExStyles |= WS_EX_LTRREADING;          // all test text english...so LTR reading order
        SetWindowLong(myHwnd, GWL_EXSTYLE, lExStyles);
    }

    SetFocus(myHwnd);
    SetForegroundWindow(myHwnd);
    ShowWindow(myHwnd, SW_SHOW);
    UpdateWindow(myHwnd);

    // make sure the cursor doesn't reappear during the test pass
    // which could cause performance slowdowns.
    SetCursor(NULL);
    
    pumpMessages();
    TDC tdctmp = myGetDC(myHwnd);
    return tdctmp;
}

//***********************************************************************************
BOOL doWindowRelease(TDC oldDC)
{
    BOOL result = myReleaseDC(myHwnd, oldDC);

    if (--windRef == 0)
    {
        DestroyWindow(myHwnd);
        UnregisterClass(myAtom, globalInst);
        myHwnd = NULL;
        myAtom = NULL;
    }
    return result;
}

//***********************************************************************************
#ifdef UNDER_NT
TDC CreateNTPrnDC(void)
{
    BOOL bPrintDlg;             // Return code from PrintDlg function
    HDC hdc = NULL;

    static BOOL s_fInitialized = FALSE;
    static PRINTDLG s_pd;

    if (!s_fInitialized)
    {
        //  Initialize variables     hDC = NULL;
        memset (&s_pd, 0, sizeof (PRINTDLG));
        /* Initialize the PRINTDLG members. */
        s_pd.lStructSize = sizeof (PRINTDLG);
        s_pd.hwndOwner = NULL;
        // PD_RETURNDC was removed.
        s_pd.Flags = PD_PRINTSETUP;
        s_pd.hDevMode = (HANDLE) NULL;
        s_pd.hDevNames = (HANDLE) NULL;
        s_pd.nFromPage = 1;
        s_pd.nToPage = 1;
        s_pd.nMinPage = 0;
        s_pd.nMaxPage = 0;
        s_pd.nCopies = 1;
        s_pd.hInstance = NULL;

        bPrintDlg = PrintDlg (&s_pd);
        if (!bPrintDlg)
        {
            return (NULL);
        }
    }

    // now create the printer DC
    DEVNAMES* pdn = (DEVNAMES*) GlobalLock (s_pd.hDevNames);
    DEVMODE* pdm = (DEVMODE*) GlobalLock (s_pd.hDevMode);

    hdc = CreateDC(_TEXT("winspool"), (TCHAR*)pdn + pdn->wDeviceOffset, NULL, pdm);
    if (!hdc)
    {
        info (ABORT, _TEXT("CreateNTPrnDC: CreateDC Failed, GLE = %d"), GetLastError());
    }
    else
    {
        // we have a working configuration
        s_fInitialized = TRUE;
    }

    GlobalUnlock (s_pd.hDevMode);
    GlobalUnlock (s_pd.hDevNames);
    return hdc;

}
#endif // UNDER_NT

//***********************************************************************************
TDC doPrnDCCreate(void)
{
    TDC hdcPrint = NULL;
    DOCINFO docinfo;
#ifdef UNDER_CE
    DEVMODE dm;

    // set up the DEVMODE for CreateDC
    memset (&dm, 0, sizeof (DEVMODE));
    dm.dmSize = sizeof (DEVMODE);
    dm.dmSpecVersion = 0x400;

    dm.dmFields |= DM_COPIES;
    dm.dmCopies = 1;         // CE only support 1: (short) ( (dwUserData & PRN_SELECT_PAGE_NUM) >> 24 );

    dm.dmFields |= DM_COLOR;
    dm.dmColor = DMCOLOR_COLOR;

    dm.dmFields |= DM_PAPERSIZE;
    dm.dmPaperSize = DMPAPER_LETTER;

    dm.dmFields |= DM_PRINTQUALITY;
    dm.dmPrintQuality = DMRES_DRAFT;

    dm.dmFields |= DM_ORIENTATION;
    dm.dmOrientation = DMORIENT_PORTRAIT;

    // Open up the dummy printer driver.
    hdcPrint = TCreateDC (TEXT("PCL_test.DLL"), NULL, TEXT("\\\\invalid\\printer"), &dm);
    if(!hdcPrint)
        info(ABORT, TEXT("CreateDC failed for the printer DC, suspect missing or invalid pcl_test.dll file"));
#else
    hdcPrint = CreateNTPrnDC ();
#endif

    // set up the physical parameters (compensating for the paper margins).
    grcInUse.left = GetDeviceCaps (hdcPrint, PHYSICALOFFSETX);
    grcInUse.top = GetDeviceCaps (hdcPrint, PHYSICALOFFSETY);
    grcInUse.right = GetDeviceCaps (hdcPrint, PHYSICALWIDTH) - 2 * GetDeviceCaps (hdcPrint, PHYSICALOFFSETX);
    grcInUse.bottom = GetDeviceCaps (hdcPrint, PHYSICALHEIGHT) - 2 * GetDeviceCaps (hdcPrint, PHYSICALOFFSETY);

    docinfo.cbSize = sizeof (docinfo);
    // Set print to file option (does nothing on CE, redirects output on NT
    docinfo.lpszOutput = TEXT("printoutput.prn");  // default to printer
    docinfo.lpszDocName = NULL;
    docinfo.lpszDatatype = NULL;
    docinfo.fwType = 0;

    g_iPrinterCntr = PCINIT;

    gpfnStartDoc(hdcPrint, &docinfo);
    gpfnStartPage (hdcPrint);

    return hdcPrint;
}

//**********************************************************************************
BOOL doPrnDCRelease(TDC tdcPrinter)
{
    BOOL pass = 1;
    if (tdcPrinter)
    {
        pass = pass && gpfnEndPage(tdcPrinter);
        pass = pass && gpfnEndDoc(tdcPrinter);
        pass = pass && DeleteDC(tdcPrinter);
    }
    return pass ? 1 : 0;
}

/***********************************************************************************
***
***   Exposed Functions
***
************************************************************************************/
void
InitRectToUse()
{
    // Initialize the full surface rectangle.
    if (g_fUsePrimaryDisplay)
    {
        SystemParametersInfo(SPI_GETWORKAREA, 0, &grcWorkArea, 0);

        if(g_bMonitorSpread)
            grcWorkArea.right = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    }
    else
    {
        HDC hdc = CreateDC(gszDisplayPath, NULL, NULL, NULL);

        grcWorkArea.left   = 0;
        grcWorkArea.top    = 0;
        grcWorkArea.right  = GetDeviceCaps(hdc, HORZRES);
        grcWorkArea.bottom = GetDeviceCaps(hdc, VERTRES);

        DeleteDC(hdc);
    }

    // set up the global max surface size rectangle.
    grcToUse = grcWorkArea;

    if(gWidth > 0 && gHeight > 0) // if the width and height are set
    {
         if(gWidth >= MINSCREENSIZE && // and they're greater than the minimum
           gHeight >= MINSCREENSIZE)
        {
            // set the screen to them.
            grcToUse.right = grcToUse.left + gWidth;
            grcToUse.bottom = grcToUse.top + gHeight;
        }
        else gWidth = gHeight = 0;
    }
    // set up the global rectangle in use size, modified for use on windows and such.
    grcInUse = grcToUse;

    // only the primary display may use windows
    if (ghwndTopMain)
    {
		// update the window position so the client rect is correct and compensates for rotation.
		CheckNotRESULT(0, SetWindowPos(ghwndTopMain, HWND_TOP, grcWorkArea.left, grcWorkArea.top,
							grcWorkArea.right - grcWorkArea.left, grcWorkArea.bottom - grcWorkArea.top, SWP_SHOWWINDOW));

		// reset the top main window client area.
		CheckNotRESULT(FALSE, SetForegroundWindow(ghwndTopMain));
		ShowWindow(ghwndTopMain, SW_SHOWNORMAL);
		CheckNotRESULT(FALSE, UpdateWindow(ghwndTopMain));

		// re-retrieve the client area of the top main window.
		CheckNotRESULT(FALSE, GetClientRect(ghwndTopMain, &grcTopMain));

		// clear out any messages recieved from window adjustments
		pumpMessages();
	}

    // do not reset the window constraints.  The window constraints keep track of rotation issues taking the minimum/maximums as appropriate
    // when rotation is available.  This is called after a rotation to re-initialize the rectangles to use, so resetting the window constraints break
    // the test with the dependancy.
}

//***********************************************************************************
void
InitSurf(void)
{
    // initial surface initialization.  get the initial device caps and surface size.
    HDC hdc;
    DIBSECTION ds;
    HBITMAP hbmp;

    if (g_fUseGetDC)
        hdc = GetDC(NULL);
    else
        hdc = CreateDC(gszDisplayPath, NULL, NULL, NULL);

    gBpp = GetDeviceCaps(hdc, BITSPIXEL);
    CheckNotRESULT(NULL, hbmp = CreateCompatibleBitmap(hdc, 1, 1));
    CheckNotRESULT(0, GetObject(hbmp, sizeof(DIBSECTION), &ds));
    CheckNotRESULT(FALSE, DeleteObject(hbmp));

#ifdef UNDER_CE
        // on CE, if we're over 16bpp, then use the bit fields from the DIBSECTION retrieved above.
        if(gBpp >= 16)
        {
            gdwRedBitMask = ds.dsBitfields[0];
            gdwGreenBitMask = ds.dsBitfields[1];
            gdwBlueBitMask = ds.dsBitfields[2];
        }
        // if we're less than 16bpp, then we're paletted and the bitmasks don't apply.
        else
        {
            gdwRedBitMask = 0x00FF0000;
            gdwGreenBitMask = 0x0000FF00;
            gdwBlueBitMask = 0x000000FF;
        }
#else
        // on xp, the primary is either 565 or 8888
        if(gBpp == gBpp)
        {
            gdwRedBitMask = 0x00F80000;
            gdwGreenBitMask = 0x00007E00;
            gdwBlueBitMask = 0x0000001F;
        }
        // if it's not 16bpp on xp, then it's 32bpp.
        else
        {
            gdwRedBitMask = 0x00FF0000;
            gdwGreenBitMask = 0x0000FF00;
            gdwBlueBitMask = 0x000000FF;
        }
#endif
    info(DETAIL, TEXT("Mask for Red is: 0x%x Mask for Green is: 0x%x Mask for Blue is: 0x%x "),
                                    gdwRedBitMask, gdwGreenBitMask, gdwBlueBitMask);

    gdwShiftedRedBitMask = gdwRedBitMask;
    gdwShiftedGreenBitMask = gdwGreenBitMask;
    gdwShiftedBlueBitMask = gdwBlueBitMask;

    ShiftBitMasks(&gdwShiftedRedBitMask, &gdwShiftedGreenBitMask, &gdwShiftedBlueBitMask);
    info(DETAIL, TEXT("Shifted mask for Red is: 0x%08x Shifted mask for Green is: 0x%08x Shifted mask for Blue is: 0x%08x "),
                                    gdwShiftedRedBitMask, gdwShiftedGreenBitMask, gdwShiftedBlueBitMask);

    windRef = 0;

    InitRectToUse();

    // initialize the window constraints.
    SetWindowConstraint(0,0);
    if (g_fUseGetDC)
        ReleaseDC(NULL, hdc);
    else
        DeleteDC(hdc);
}

//***********************************************************************************
void
FreeSurf(void)
{
}

//***********************************************************************************
BOOL
RandRotate(int nDirection = -1)
{
#ifdef UNDER_CE
    DEVMODE devMode;

    if(gRotateAvail && !gRotateDisabledByUser)
    {
        if (nDirection == -1)
        {
            switch(GenRand() % 4)
            {
            case 0:
                nDirection = DMDO_0;
                break;
            case 1:
                nDirection = DMDO_90;
                break;
            case 2:
                nDirection = DMDO_180;
                break;
            case 3:
                nDirection = DMDO_270;
                break;
            }
        }
        memset(&devMode,0x00,sizeof(devMode));
        devMode.dmSize=sizeof(devMode);
        devMode.dmFields = DM_DISPLAYORIENTATION;
        devMode.dmDisplayOrientation = nDirection;
        LONG lrval = ChangeDisplaySettingsEx(NULL,&devMode,NULL,CDS_RESET,NULL);
        if (DISP_CHANGE_SUCCESSFUL == lrval)
            return TRUE;

        info(FAIL, TEXT("RandRotate: ChangeDisplaySettingsEx failed, ret=%d."), lrval);
    }
#endif
    return FALSE;
}

//***********************************************************************************
BOOL
RandResize(void)
{
#ifdef UNDER_CE
    if(gResizeAvail && !gResizeDisabledByUser)
    {
        // randomly pick a supported mode
        DEVMODE devMode;
        memset(&devMode,0x00,sizeof(devMode));
        devMode.dmSize = sizeof(devMode);
        DWORD dwMode = GenRand()%gdwMaxDisplayModes();
        if (EnumDisplaySettings(NULL, dwMode, &devMode))
        {
            devMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
            LONG lrval = ChangeDisplaySettingsEx(NULL,&devMode,NULL,CDS_RESET,NULL);
            if (DISP_CHANGE_SUCCESSFUL == lrval)
                return TRUE;

            info(FAIL, TEXT("RandResize: ChangeDisplaySettingsEx failed, ret=%d."), lrval);
        }
        else
        {
            info(FAIL, TEXT("RandResize: EnumDisplaySettings failed, GetLastError()=%d."), GetLastError());
        }
    }
#endif
    return FALSE;
}

//***********************************************************************************
void
ResetScreen(void)
{
    // let the rotation take effect.
    pumpMessages();

    ResetVerifyDriver();

    // reset the base surface size to the rotated size
    InitRectToUse();
    RehideShell(ghwndTopMain);
}

// used to disable surface verification when run on a printer driver.
static BOOL bOldSurfVerify;

//***********************************************************************************
TDC myGetDC(void)
{
    TDC     hdc = NULL;
    TBITMAP hbmp = NULL;
    DIBSECTION ds;

    // rotate the device randomly.  This changes the surface size to use.
    BOOL fRotated = RandRotate();

    // change the resolution randomly. This also changes the surface size to use.
    BOOL fResized = RandResize();

    if (fRotated || fResized) 
        ResetScreen();

    // reset the rectangle in use for the new creation.
    grcInUse = grcToUse;

    switch (DCFlag)
    {
        case EGDI_VidMemory:
        case EGDI_SysMemory:
        case EGDI_1bppBMP:
        case EGDI_2bppBMP:
        case EGDI_4bppBMP:
        case EGDI_8bppBMP:
        case EGDI_16bppBMP:
        case EGDI_24bppBMP:
        case EGDI_32bppBMP:
        case EGDI_8bppPalDIB:
        case EGDI_1bppRGBDIB:
        case EGDI_2bppRGBDIB:
        case EGDI_4bppRGBDIB:
        case EGDI_8bppRGBDIB:
        case EGDI_16bppRGBDIB:
        case EGDI_16bppRGB4444DIB:
        case EGDI_16bppRGB1555DIB:
        case EGDI_16bppRGB555DIB:
        case EGDI_16bppRGB565DIB:
        case EGDI_16bppBGR4444DIB:
        case EGDI_16bppBGR1555DIB:
        case EGDI_16bppBGR555DIB:
        case EGDI_16bppBGR565DIB:
        case EGDI_24bppRGBDIB:
        case EGDI_32bppRGBDIB:
        case EGDI_32bppRGB8888DIB:
        case EGDI_32bppRGB888DIB:
        case EGDI_32bppBGR8888DIB:
        case EGDI_32bppBGR888DIB:
        case EGDI_8bppPalDIBBUB:
        case EGDI_1bppRGBDIBBUB:
        case EGDI_2bppRGBDIBBUB:
        case EGDI_4bppRGBDIBBUB:
        case EGDI_8bppRGBDIBBUB:
        case EGDI_16bppRGBDIBBUB:
        case EGDI_16bppRGB4444DIBBUB:
        case EGDI_16bppRGB1555DIBBUB:
        case EGDI_16bppRGB555DIBBUB:
        case EGDI_16bppRGB565DIBBUB:
        case EGDI_16bppBGR4444DIBBUB:
        case EGDI_16bppBGR1555DIBBUB:
        case EGDI_16bppBGR555DIBBUB:
        case EGDI_16bppBGR565DIBBUB:
        case EGDI_24bppRGBDIBBUB:
        case EGDI_32bppRGBDIBBUB:
        case EGDI_32bppRGB8888DIBBUB:
        case EGDI_32bppRGB888DIBBUB:
        case EGDI_32bppBGR8888DIBBUB:
        case EGDI_32bppBGR888DIBBUB:
            hdc = doMemDCCreate();
            break;
        case EFull_Screen:
        case ECreateDC_Primary:
            hdc = myGetDC(NULL);
            break;
        case EWin_Primary:
            hdc = doWindowCreate();
            break;
        case EGDI_Printer:
            // disable driver verification when running on a printer DC.
            bOldSurfVerify = SetSurfVerify(FALSE);
            hdc = doPrnDCCreate();
            break;
        }

    if (!hdc)
        info(ABORT, TEXT("myGetDC: GetDC Failed for DC: %s"), DCName[DCFlag]);

    // if the width decreases, then we'll scale down, if the height decreases,
    // we'll scale down, etc.  we'll only scale up if both scale up.
    adjustRatio = min((float) zy / (float) 480, (float) zx / (float) 640);
    //info(ECHO, TEXT("gdit: adjustRatio = %7.3f\r\n"), adjustRatio);

    // get the DIBSECTION object off of the surface if it's a bitmap or dib.
    if((hbmp = (TBITMAP) GetCurrentObject(hdc, OBJ_BITMAP)) == NULL || !GetObject(hbmp, sizeof(DIBSECTION), &ds))
    {
        // if it's not a dibsection, then it's the primary, so create a compatible bitmap to retrieve the dibsection data from.
        hbmp = CreateCompatibleBitmap(hdc, 1, 1);
        CheckNotRESULT(0, GetObject(hbmp, sizeof(DIBSECTION), &ds));
        CheckNotRESULT(FALSE, DeleteObject(hbmp));
    }

    gBpp = ds.dsBm.bmBitsPixel;

    // in these cases, the DIBSECTION data doesn't apply,
    // and the bitmask is always 888
    if(DCFlag == EGDI_Printer ||
        DCFlag == EGDI_24bppBMP ||
        DCFlag == EGDI_32bppBMP ||
        DCFlag == EGDI_24bppRGBDIB ||
        DCFlag == EGDI_32bppRGBDIB ||
        DCFlag == EGDI_24bppRGBDIBBUB ||
        DCFlag == EGDI_32bppRGBDIBBUB ||
        gBpp < 16)
    {
        gdwRedBitMask = 0x00FF0000;
        gdwGreenBitMask = 0x0000FF00;
        gdwBlueBitMask = 0x000000FF;
    }
    // otherwise we use the bitmasks returned.
    else
    {
        gdwRedBitMask = ds.dsBitfields[0];
        gdwGreenBitMask = ds.dsBitfields[1];
        gdwBlueBitMask = ds.dsBitfields[2];
    }

#ifndef UNDER_CE
    // on xp, only dib's will give the bit masks
    // so if we're not on a dib, then we assume 16bpp means 565
    // and anything else is 888
    // if we aren't on a dib, then whatever was chosen above applies
    if(!isDIBDC())
    {
        if(16 == gBpp)
        {
            gdwRedBitMask = 0x00F80000;
            gdwGreenBitMask = 0x00007E00;
            gdwBlueBitMask = 0x0000001F;
        }
        // if it's not 16bpp on xp, then it's 32bpp.
        else
        {
            gdwRedBitMask = 0x00FF0000;
            gdwGreenBitMask = 0x0000FF00;
            gdwBlueBitMask = 0x000000FF;
        }
    }
#endif

    gdwShiftedRedBitMask = gdwRedBitMask;
    gdwShiftedGreenBitMask = gdwGreenBitMask;
    gdwShiftedBlueBitMask = gdwBlueBitMask;

    ShiftBitMasks(&gdwShiftedRedBitMask, &gdwShiftedGreenBitMask, &gdwShiftedBlueBitMask);

    // if we are able to write to this dc, put the dc into a known state for the test
    if (IsWritableHDC(hdc))
    {
        HBRUSH hbr = CreateSolidBrush(randColorRef());
        if(hbr)
        {
            RECT rc = { 0, 0, zx, zy};
            CheckNotRESULT(FALSE, FillRect(hdc, &rc, hbr));
            CheckNotRESULT(FALSE, DeleteObject(hbr));
        }
        else
            // not enough memory to make the brush, just initialize the surface to white.
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
    }

#ifndef UNDER_CE
    HRGN hRgn = NULL;
    CheckNotRESULT(NULL, hRgn = CreateRectRgn(0, 0, zx, zy));
    CheckNotRESULT(ERROR, SelectClipRgn(hdc, hRgn));
    CheckNotRESULT(FALSE, DeleteObject(hRgn));

    GdiSetBatchLimit(1);
#endif

    return hdc;
}

//***********************************************************************************
BOOL myReleaseDC(TDC hdc)
{
    BOOL    result = 0;

    switch (DCFlag)
    {
        case EGDI_VidMemory:
        case EGDI_SysMemory:
        case EGDI_1bppBMP:
        case EGDI_2bppBMP:
        case EGDI_4bppBMP:
        case EGDI_8bppBMP:
        case EGDI_16bppBMP:
        case EGDI_24bppBMP:
        case EGDI_32bppBMP:
        case EGDI_8bppPalDIB:
        case EGDI_1bppRGBDIB:
        case EGDI_2bppRGBDIB:
        case EGDI_4bppRGBDIB:
        case EGDI_8bppRGBDIB:
        case EGDI_16bppRGBDIB:
        case EGDI_16bppRGB4444DIB:
        case EGDI_16bppRGB1555DIB:
        case EGDI_16bppRGB555DIB:
        case EGDI_16bppRGB565DIB:
        case EGDI_16bppBGR4444DIB:
        case EGDI_16bppBGR1555DIB:
        case EGDI_16bppBGR555DIB:
        case EGDI_16bppBGR565DIB:
        case EGDI_24bppRGBDIB:
        case EGDI_32bppRGBDIB:
        case EGDI_32bppRGB8888DIB:
        case EGDI_32bppRGB888DIB:
        case EGDI_32bppBGR8888DIB:
        case EGDI_32bppBGR888DIB:
        case EGDI_8bppPalDIBBUB:
        case EGDI_1bppRGBDIBBUB:
        case EGDI_2bppRGBDIBBUB:
        case EGDI_4bppRGBDIBBUB:
        case EGDI_8bppRGBDIBBUB:
        case EGDI_16bppRGBDIBBUB:
        case EGDI_16bppRGB4444DIBBUB:
        case EGDI_16bppRGB1555DIBBUB:
        case EGDI_16bppRGB555DIBBUB:
        case EGDI_16bppRGB565DIBBUB:
        case EGDI_16bppBGR4444DIBBUB:
        case EGDI_16bppBGR1555DIBBUB:
        case EGDI_16bppBGR555DIBBUB:
        case EGDI_16bppBGR565DIBBUB:
        case EGDI_24bppRGBDIBBUB:
        case EGDI_32bppRGBDIBBUB:
        case EGDI_32bppRGB8888DIBBUB:
        case EGDI_32bppRGB888DIBBUB:
        case EGDI_32bppBGR8888DIBBUB:
        case EGDI_32bppBGR888DIBBUB:
            result = doMemDCRelease(hdc);
            break;
        case EFull_Screen:
        case ECreateDC_Primary:
            result = myReleaseDC(NULL, hdc);
            break;
        case EWin_Primary:
            result = doWindowRelease(hdc);
            break;
        case EGDI_Printer:
            result = doPrnDCRelease(hdc);
            SetSurfVerify(bOldSurfVerify);
            break;
    }
    return result;
}

//***********************************************************************************
BOOL isMemDC(void)
{
    // if it's not a primary, default, window, or printer, it's a memory dc.
    return (!(DCFlag == EFull_Screen || DCFlag == EWin_Primary || DCFlag == ECreateDC_Primary || DCFlag == EGDI_Printer));
}

//***********************************************************************************
BOOL isWinDC(void)
{
    // check if it's a primary dc
    return (DCFlag == EWin_Primary);
}

//***********************************************************************************
BOOL isPalDIBDC(void)
{
   return (DCFlag == EGDI_1bppRGBDIB || DCFlag == EGDI_2bppRGBDIB ||
              DCFlag == EGDI_4bppRGBDIB || DCFlag == EGDI_8bppRGBDIB ||
              DCFlag == EGDI_8bppPalDIB ||  DCFlag == EGDI_1bppRGBDIBBUB ||
              DCFlag == EGDI_2bppRGBDIBBUB || DCFlag == EGDI_4bppRGBDIBBUB ||
              DCFlag == EGDI_8bppRGBDIBBUB || DCFlag == EGDI_8bppPalDIBBUB);
}

//***********************************************************************************
BOOL isSysPalDIBDC()
{
    return (DCFlag == EGDI_8bppPalDIB || DCFlag == EGDI_8bppPalDIBBUB);
}

//***********************************************************************************
BOOL isDIBDC(void)
{
   return (DCFlag >= EGDI_DIBDCSTART && DCFlag <= EGDI_DIBDCEND);
}

//***********************************************************************************
BOOL isPrinterDC(TDC hdc)
{
    if(hdc == NULL)
        return (DCFlag == EGDI_Printer);
    else
        return (GetDeviceCaps(hdc, TECHNOLOGY) == DT_RASPRINTER);
}

// send 0,0 to set normal width and heigh constraints
BOOL SetWindowConstraint(int nx, int ny)
{
    int minSize, minWidth, minHeight;

    // if the user requested something larger than the screeen, fail.
    if(nx > (grcToUse.right - grcToUse.left) || ny > (grcToUse.bottom - grcToUse.top))
        return FALSE;

    // if rotation is available and the user requested something larger than a rotated screen, fail.
    if(gRotateAvail && (ny > (grcToUse.right - grcToUse.left) || nx > (grcToUse.bottom - grcToUse.top)))
        return FALSE;

    // the standard minimum window width/height is 1/2 the screen
    minWidth = (grcToUse.right - grcToUse.left)/2;
    minHeight = (grcToUse.bottom - grcToUse.top)/2;

    // if the default minimum's could make the width or height less than the minimum, use the smaller of the
    // minum screen size or the actual screen size.
    if(minWidth < MINSCREENSIZE || minHeight < MINSCREENSIZE)
    {
        minWidth = min((grcToUse.right - grcToUse.left), MINSCREENSIZE);
        minHeight = min((grcToUse.bottom - grcToUse.top), MINSCREENSIZE);
    }

    // if using rotation, the minimum window width/height is the smaller of the minumums above.
    minSize = min(minWidth, minHeight);

    // set the window constraint to the larger of the user request and standard for the system type.
    if(gRotateAvail)
    {
        gMinWinWidth = max(nx, minSize);
        gMinWinHeight = max(ny, minSize);
    }
    else
    {
        gMinWinWidth = max(nx, minWidth);
        gMinWinHeight = max(ny, minHeight);
    }

    return TRUE;
}


