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

/**************************************************************************


Module Name:

   dc.cpp

Abstract:

   Gdi Tests: Device Context APIs

***************************************************************************/

#include "global.h"

/***********************************************************************************
***
***   Support Functions
***
************************************************************************************/

// all of the supported objects
static HRGN     g_rghRgn[2], g_stockRgn;
static HBRUSH   g_rghBrush[2], g_stockBrush;
static TBITMAP  g_rghBmp[2], g_stockBmp;
static HFONT    g_rghFont[2], g_stockFont;
static HPALETTE g_rghPal[2], g_stockPal;

//**********************************************************************************
void
passNull2DC(int testFunc)
{
    info(ECHO, TEXT("%s - Passing NULL"), funcName[testFunc]);
    DWORD dwQuery;

    switch (testFunc)
    {
        case ECreateCompatibleDC:
            // should succeed.
            CheckNotRESULT(0, CreateCompatibleDC((TDC) NULL));

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, CreateCompatibleDC(g_hdcBAD));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, CreateCompatibleDC(g_hdcBAD2));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EDeleteDC:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, DeleteDC((HDC) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, DeleteDC((HDC) 0xBAD));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, DeleteDC((HDC) g_hbrushStock));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EGetDeviceCaps:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetDeviceCaps(g_hdcBAD, BITSPIXEL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetDeviceCaps(g_hdcBAD2, BITSPIXEL));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case ERestoreDC:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, RestoreDC((TDC) NULL, 0));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, RestoreDC(g_hdcBAD, 0));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, RestoreDC(g_hdcBAD2, 0));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case ESaveDC:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SaveDC((TDC) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SaveDC(g_hdcBAD));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SaveDC(g_hdcBAD2));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case ECreateDC:
            break;
        case EExtEscape:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, ExtEscape((TDC) NULL, QUERYESCSUPPORT, sizeof(DWORD), (LPSTR) &dwQuery, 0, NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, ExtEscape(g_hdcBAD, QUERYESCSUPPORT, sizeof(DWORD), (LPSTR) &dwQuery, 0, NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, ExtEscape(g_hdcBAD2, QUERYESCSUPPORT, sizeof(DWORD), (LPSTR) &dwQuery, 0, NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);
            break;
        default:
            info(FAIL, TEXT("PassNull called on an invalid case"));
            break;
    }
}

//**********************************************************************************
void
initObjectsDC(TDC hdc, int index)
{
    if (gSanityChecks)
        info(DETAIL, TEXT("Init Objects"));

    int     code = (index + 1) * 3;
    HRGN hrgnTmp = NULL;
    HBRUSH hbrTmp = NULL;
    HFONT hfntTmp = NULL;
    TBITMAP hbmpTmp = NULL;

    g_rghRgn[index] = (HRGN) NULL;
    g_rghBrush[index] = (HBRUSH) NULL;
    g_rghBmp[index] = (TBITMAP) NULL;
    g_rghFont[index] = (HFONT) NULL;
    g_rghPal[index] = (HPALETTE) NULL;

    // create all supported objects
    CheckNotRESULT(NULL, g_rghRgn[index] = CreateRectRgn(0, 0, code, code));
    CheckNotRESULT(NULL, g_rghBrush[index] = CreateSolidBrush(RGB(code, code, code)));
    CheckNotRESULT(NULL, g_rghBmp[index] = CreateCompatibleBitmap(hdc, code, code));

    LOGFONT myFont;

    myFont.lfHeight = code;
    myFont.lfWidth = code;
    myFont.lfEscapement = code;
    myFont.lfOrientation = code;
    myFont.lfWeight = code;
    myFont.lfItalic = 0;
    myFont.lfUnderline = 0;
    myFont.lfStrikeOut = 0;
    myFont.lfCharSet = ANSI_CHARSET;
    myFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
    myFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    myFont.lfQuality = DEFAULT_QUALITY;
    myFont.lfPitchAndFamily = DEFAULT_PITCH;
    myFont.lfFaceName[0] = NULL;
    CheckNotRESULT(NULL, g_rghFont[index] = CreateFontIndirect(&myFont));
    CheckNotRESULT(NULL, g_rghPal[index] = myCreatePal(hdc, code));

    if (index == 0)
    {
        // Save the stock objects on call 0
        CheckNotRESULT(GDI_ERROR, g_stockRgn = (HRGN) SelectObject(hdc, g_rghRgn[index]));

        CheckNotRESULT(NULL, g_stockBrush = (HBRUSH) SelectObject(hdc, g_rghBrush[index]));

        CheckNotRESULT(NULL, g_stockFont = (HFONT) SelectObject(hdc, g_rghFont[index]));

        SetLastError(ERROR_SUCCESS);
        g_stockBmp = (TBITMAP) SelectObject(hdc, g_rghBmp[index]);

        if(!g_stockBmp)
            CheckForLastError(ERROR_INVALID_PARAMETER);
        if(!g_stockBmp && isMemDC())
            info(FAIL, TEXT("Selection of bitmap failed, index 0"));
        if(g_stockBmp && !isMemDC())
            info(FAIL, TEXT("Selection of bitmap Succeeded on a primary surface, index 0"));
    }
    else
    {
        // the values returned by the second call should match the ones selected in the first,
        // except for regions.
        CheckNotRESULT(GDI_ERROR, hrgnTmp = (HRGN) SelectObject(hdc, g_rghRgn[index]));

        CheckNotRESULT(NULL, hbrTmp = (HBRUSH) SelectObject(hdc, g_rghBrush[index]));

        if(hbrTmp != g_rghBrush[0])
            info(FAIL, TEXT("Brush returned does not match previous brush selected, index 1"));

        CheckNotRESULT(NULL, hfntTmp = (HFONT) SelectObject(hdc, g_rghFont[index]));

        if(hfntTmp != g_rghFont[0])
            info(FAIL, TEXT("Font returned does not match previous font selected, index 1"));

        SetLastError(ERROR_SUCCESS);
        hbmpTmp = (TBITMAP) SelectObject(hdc, g_rghBmp[index]);
        if(!hbmpTmp)
            CheckForLastError(ERROR_INVALID_PARAMETER);
        if(!hbmpTmp && isMemDC())
            info(FAIL, TEXT("Selection of bitmap failed, index 1"));
        if(hbmpTmp != g_rghBmp[0] && isMemDC())
            info(FAIL, TEXT("Bitmap returned does not match previous bitmap selected, index 1"));
        if(hbmpTmp && !isMemDC())
            info(FAIL, TEXT("Selection of bitmap succeeded on a primary surface, index 1"));
    }
}

//**********************************************************************************
void
checkObjectsDC(TDC hdc, int index)
{

    if (gSanityChecks)
        info(DETAIL, TEXT("check Objects"));

    int     code = (index + 1) * 3;

    LOGBRUSH logBrush;
    BITMAP  logBmp;
    LOGFONT logFont;

    HBRUSH  tempBrush;
    HFONT   tempFont;
    TBITMAP tempBmp;
    int     result0,
            result1;

    // brush
    CheckNotRESULT(NULL, tempBrush = (HBRUSH) GetCurrentObject(hdc, OBJ_BRUSH));
    CheckNotRESULT(0, result0 = GetObject(tempBrush, 0, NULL));
    CheckNotRESULT(0, result1 = GetObject(tempBrush, result0, (LPVOID) & logBrush));
    if (logBrush.lbColor != RGB(code, code, code))
        info(FAIL, TEXT("Brush not correctly recovered 0x%x != 0x%x"), logBrush.lbColor, RGB(code, code, code));

    // font
    CheckNotRESULT(NULL, tempFont = (HFONT) GetCurrentObject(hdc, OBJ_FONT));
    CheckNotRESULT(0, result0 = GetObject(tempFont, 0, NULL));
    CheckNotRESULT(0, result1 = GetObject(tempFont, result0, (LPVOID) & logFont));
    if (logFont.lfHeight != code)
        info(FAIL, TEXT("Font not correctly recovered %d"), logFont.lfHeight);

    if (isMemDC())
    {                           // !getDC(null)
        // bitmap
        CheckNotRESULT(NULL, tempBmp = (TBITMAP) GetCurrentObject(hdc, OBJ_BITMAP));
        CheckNotRESULT(0, result0 = GetObject(tempBmp, 0, NULL));
        CheckNotRESULT(0, result1 = GetObject(tempBmp, result0, (LPVOID) & logBmp));
        if (logBmp.bmWidth != code)
            info(FAIL, TEXT("Bitmap not correctly recovered %d 0x%x"), logBmp.bmWidth, tempBmp);
    }
    else
        info(DETAIL, TEXT("Skipping Bitmap check in this mode"));

    // palette
    PALETTEENTRY outPal[256];
    HPALETTE tempPal;

    CheckNotRESULT(NULL, tempPal = (HPALETTE) GetCurrentObject(hdc, OBJ_PAL));
    CheckNotRESULT(0, GetPaletteEntries(tempPal, 0, 235, (LPPALETTEENTRY)outPal));
    for (int i = 10; i < 235; i++)
        if (outPal[i].peRed != i + code)
        {
            info(FAIL, TEXT("Palette not correctly recovered Pal [%d](%d %d %d) != (%d)"), i, outPal[i].peRed,
                 outPal[i].peGreen, outPal[i].peBlue, i + code);
            break;
        }
}

//**********************************************************************************
void
destroyObjectsDC(TDC hdc, int index)
{

    if (gSanityChecks)
        info(DETAIL, TEXT("Delete Objects"));

    //   No need to restore regions -- the "stockRgn" isn't even a region.
    //   SelectObject(hdc,(HRGN)     stockRgn);
    if(index == 0)
    {
        CheckNotRESULT(NULL, SelectObject(hdc, (HBRUSH) g_stockBrush));

        if( isMemDC())
            CheckNotRESULT(NULL, SelectObject(hdc, (TBITMAP) g_stockBmp));

        CheckNotRESULT(NULL, SelectObject(hdc, (HFONT) g_stockFont));
    }

    // palette
    myDeletePal(hdc, g_rghPal[index]);

    // get rid of any created object
    CheckNotRESULT(FALSE, DeleteObject(g_rghRgn[index]));
    CheckNotRESULT(FALSE, DeleteObject(g_rghBrush[index]));
    CheckNotRESULT(FALSE, DeleteObject(g_rghBmp[index]));
    CheckNotRESULT(FALSE, DeleteObject(g_rghFont[index]));
}

/***********************************************************************************
***
***
***
************************************************************************************/

//**********************************************************************************
void
TestGetDCOrgEx(int testFunc)
{
// not available on CE.
#ifndef UNDER_CE
    info(ECHO, TEXT("%s: Testing"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    POINT   p = { -10, -10 };
    BOOL    result;

#if 0 // not valid on a window, which will have a non-zero or random dc origin.
    info(DETAIL, TEXT("Part 0"));

    CheckNotRESULT(0, result = GetDCOrgEx(hdc, &p));
    if (p.x != 0 || p.y != 0)
        info(FAIL, TEXT("passing valid DC got offset point x:%d y:%d result:%d"), p.x, p.y, result);
#endif

    p.x = p.y = -10;
    info(DETAIL, TEXT("Part 1"));
    CheckForRESULT(0, result = GetDCOrgEx(NULL, &p));

    if (p.x != -10 || p.y != -10)
        info(FAIL, TEXT("passing NULL DC got offset point x:%d y:%d result:%d"), p.x, p.y, result);

    p.x = p.y = -10;
    info(DETAIL, TEXT("Part 2"));
    CheckForRESULT(0, result = GetDCOrgEx((TDC) 0x911, &p));

    if (p.x != -10 || p.y != -10)
        info(FAIL, TEXT("passing NULL DC got offset point x:%d y:%d result:%d"), p.x, p.y, result);

    myReleaseDC(hdc);
#endif // UNDER_CE
}

/***********************************************************************************
***
***   Stress test DeleteDC
***
************************************************************************************/

//**********************************************************************************
void
StressPairs(int testFunc)
{

    BOOL    bOldVerify = SetSurfVerify(FALSE);

    info(ECHO, TEXT("%s: Doing 1 minute of create and DeleteDC"), funcName[testFunc]);

    TDC     hdc;
    // run the test for 1 minute
    DWORD EndTime = (GetTickCount() + 60000);
    int i=0;

    for(i =0; GetTickCount() <= EndTime; i++)
    {
        if (testFunc == ECreateCompatibleDC)
        {
            CheckNotRESULT(NULL, hdc = CreateCompatibleDC((TDC) NULL));
            CheckNotRESULT(FALSE, DeleteDC(hdc));
        }
        else
        {
            CheckNotRESULT(NULL, hdc = myGetDC());
            CheckNotRESULT(FALSE, myReleaseDC(hdc));
        }
    }

    info(DETAIL, TEXT("Did %d Get/Release pairs"), i);
    SetSurfVerify(bOldVerify);
}

/***********************************************************************************
***
***   Program Error
***
************************************************************************************/

//**********************************************************************************
void
ProgramError(int testFunc)
{

    info(ECHO, TEXT("%s: Program Error test."), funcName[testFunc]);

    //This test case is inproperly handling dc's, which cannot be handled properly by
    // the TDC wrapper, so we must fall back to using HDC's
    HDC     hdc;

    CheckNotRESULT(NULL, hdc = GetDC(NULL));

    CheckNotRESULT(FALSE, DeleteDC(hdc));

    SetLastError(ERROR_SUCCESS);
    CheckForRESULT(FALSE, ReleaseDC(NULL, hdc));
    CheckForLastError(ERROR_INVALID_HANDLE);
}

/***********************************************************************************
***
***   Save\Restore DC Testing
***
************************************************************************************/

//**********************************************************************************
void
SaveSomeDCs(TDC hdc, int num)
{

    int     sResult0;

    while (RestoreDC(hdc, -1))
        ;                       // remove all saved DCs

    for (int i = 0; i < num; i++)
    {
        sResult0 = SaveDC(hdc);

        if (sResult0 != i + 1)
            info(FAIL, TEXT("SaveDC returned: %d not expected:%d"), sResult0, i + 1);
    }
}

//**********************************************************************************
void
SaveRestoreDCPairs(int testFunc)
{

    info(ECHO, TEXT("%s: Save & Restore DC Testing"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    int     i,
            rResult0;

    info(DETAIL, TEXT("Part 0"));
    // try -1 restore sequence
    SaveSomeDCs(hdc, testCycles);
    for (i = 0; i < testCycles + 1; i++)
    {
        rResult0 = RestoreDC(hdc, -1);
        CheckNotRESULT(FALSE, Rectangle(hdc, 0, 0, 50, 50));
        if (!rResult0 && i != testCycles)
            info(FAIL, TEXT("RestoreDC (hdc,-1) returned: %d not expected: 1"), rResult0);
        else if (rResult0 && i == testCycles)
            info(FAIL, TEXT("RestoreDC (hdc,-1) past last index returned: %d expected: 0"), rResult0);
    }

    info(DETAIL, TEXT("Part 1"));
    // try numerical sequence
    SaveSomeDCs(hdc, testCycles);
    // we have to go backward here
    for (i = testCycles; i > -1; i--)
    {
        rResult0 = RestoreDC(hdc, i);
        CheckNotRESULT(FALSE, Rectangle(hdc, 0, 0, 50, 50));
        if (!rResult0 && i != 0)
            info(FAIL, TEXT("RestoreDC (hdc,%d) returned: %d not expected: 1"), i, rResult0);
        else if (rResult0 && i == 0)
            info(FAIL, TEXT("RestoreDC (hdc,%d) past last index returned: %d expected: 0"), i, rResult0);
    }

    info(DETAIL, TEXT("Part 2"));
    // try -1 accessing top of stack when none exists
    SaveSomeDCs(hdc, testCycles);
    // this should remove everything above it on stack
    rResult0 = RestoreDC(hdc, 1);
    // now try to get items that were deleted
    for (i = 0; i < testCycles; i++)
    {
        rResult0 = RestoreDC(hdc, -1);
        CheckNotRESULT(FALSE, Rectangle(hdc, 0, 0, 50, 50));
        if (rResult0)
            info(FAIL, TEXT("RestoreDC (hdc,%d) returned:%d after top of stack - expected failure"), i, rResult0);
    }

    info(DETAIL, TEXT("Part 3"));
    // try numerically accessing top of stack when none exists
    SaveSomeDCs(hdc, testCycles);
    // this should remove everything above it on stack
    rResult0 = RestoreDC(hdc, 1);
    // now try to get items that were deleted
    for (i = testCycles; i > 0; i--)
    {
        rResult0 = RestoreDC(hdc, i);
        if (rResult0)
            info(FAIL, TEXT("RestoreDC (hdc,%d) returned:%d after top of stack - expected failure"), i, rResult0);
    }
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Test Recovering Objects
***
************************************************************************************/

//**********************************************************************************
void
RecoverObjectsTest(int testFunc)
{

    if (0 != testFunc)
        info(ECHO, TEXT("%s: Recovering SaveDC Objects 1"), funcName[testFunc]);
    TDC     hdc = myGetDC();
    int     sResult,
            rResult;

    initObjectsDC(hdc, 0);
    checkObjectsDC(hdc, 0);

    sResult = SaveDC(hdc);
    if (sResult <= 0)
        info(FAIL, TEXT("SaveDC (hdc) => %d"), sResult);

    initObjectsDC(hdc, 1);
    checkObjectsDC(hdc, 1);

    rResult = RestoreDC(hdc, -1);

    if (rResult != 1)
        info(FAIL, TEXT("RestoreDC (hdc,-1) => %d"), rResult);

    checkObjectsDC(hdc, 0);

    destroyObjectsDC(hdc, 0);
    destroyObjectsDC(hdc, 1);

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Check Off Screen Drawing
***
************************************************************************************/

//**********************************************************************************
void
TestCreateCompatibleDC(int testFunc)
{

    info(ECHO, TEXT("%s - Offscreen Drawing Test"), funcName[testFunc]);

    // set up vars
    TDC     hdc = myGetDC();
    TDC     offDC;
    TBITMAP hBmp,
            stockBmp;

    CheckNotRESULT(NULL, offDC = CreateCompatibleDC(hdc));

    CheckNotRESULT(NULL, hBmp = CreateCompatibleBitmap(hdc, zx / 2, zy));

    CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(offDC, hBmp));
    info(DETAIL, TEXT("Prior to any calls"));
    CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
    info(DETAIL, TEXT("PatBlt(hdc,   0, 0,zx,   zy, WHITENESS)"));
    CheckNotRESULT(FALSE, PatBlt(offDC, 0, 0, zx / 2, zy, WHITENESS));
    info(DETAIL, TEXT("PatBlt(offDC, 0, 0,zx/2, zy, WHITENESS)"));

    //BitBlt
    CheckNotRESULT(FALSE, BitBlt(offDC, 10, 10, 10, 10, offDC, 10, 10, SRCCOPY));  // off to off
    info(DETAIL, TEXT("BitBlt(offDC,10,10,10,10,offDC,10,10,SRCCOPY)"));
    CheckNotRESULT(FALSE, BitBlt(hdc, 10, 20, 10, 10, offDC, 10, 10, SRCCOPY));    // off to on
    info(DETAIL, TEXT("BitBlt(hdc,10,20,10,10,offDC,10,10,SRCCOPY)"));
    CheckNotRESULT(FALSE, BitBlt(offDC, 10, 30, 10, 10, hdc, 10, 10, SRCCOPY));    // on to off
    info(DETAIL, TEXT("BitBlt(offDC,10,30,10,10,hdc,10,10,SRCCOPY)"));

    //Get/SetPixel
    CheckNotRESULT(CLR_INVALID, SetPixel(offDC, 35, 35, RGB(0, 0, 0)));
    info(DETAIL, TEXT("SetPixel(offDC,35,35,RGB(0,0,0))"));
    CheckNotRESULT(CLR_INVALID, GetPixel(offDC, 35, 35));
    info(DETAIL, TEXT("GetPixel(offDC,35,35)"));
    CheckNotRESULT(CLR_INVALID, SetPixel(hdc, 35, 35, RGB(0, 0, 0)));
    info(DETAIL, TEXT("SetPixel(hdc,35,35,RGB(0,0,0))"));
    CheckNotRESULT(CLR_INVALID, GetPixel(hdc, 35, 35));
    info(DETAIL, TEXT("GetPixel(hdc,35,35)"));

    //PatBlt
    CheckNotRESULT(FALSE, PatBlt(offDC, 40, 40, 10, 10, BLACKNESS));
    info(DETAIL, TEXT("PatBlt(offDC,40,40,10,10,BLACKNESS)"));
    CheckNotRESULT(FALSE, PatBlt(hdc, 40, 40, 10, 10, BLACKNESS));
    info(DETAIL, TEXT("PatBlt(hdc,40,40,10,10,BLACKNESS)"));

    //Rectangle
    CheckNotRESULT(NULL, SelectObject(offDC,GetStockObject(BLACK_BRUSH)));
    CheckNotRESULT(FALSE, Rectangle(offDC,50,50,60,60));
    info(DETAIL, TEXT("Rectangle(offDC,50,50,60,60)"));
    CheckNotRESULT(NULL, SelectObject(hdc,GetStockObject(BLACK_BRUSH)));
    CheckNotRESULT(FALSE, Rectangle(hdc,50,50,60,60));
    info(DETAIL, TEXT("Rectangle(hdc,50,50,60,60)"));

    // copy squares from offscreen to top screen
    CheckNotRESULT(FALSE, BitBlt(hdc, zx / 2, 0, zx / 2, zy, offDC, 0, 0, SRCCOPY));

    CheckScreenHalves(hdc);

    CheckNotRESULT(NULL, SelectObject(offDC, stockBmp));
    CheckNotRESULT(FALSE, DeleteObject(hBmp));
    CheckNotRESULT(FALSE, DeleteDC(offDC));

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Globe Test
***
************************************************************************************/

//**********************************************************************************
void
globeTest(int testFunc)
{

    info(ECHO, TEXT("%s - Globe Test"), funcName[testFunc]);

    TDC     aDC = myGetDC(),
            compDC[MAX_GLOBE];
    TBITMAP hBmp[MAX_GLOBE],
            stockBmp[MAX_GLOBE];
    POINT   Globes = { zx / 2 / GLOBE_SIZE, zy / GLOBE_SIZE };
    TCHAR   szBuf[30];
    int     i,
            j;
    HPALETTE hPal;

    CheckNotRESULT(NULL, hPal = myCreateNaturalPal(aDC));

    for (i = 0; i < MAX_GLOBE; i++)
    {
        _sntprintf(szBuf, countof(szBuf), TEXT("GLOBE%d"), i);
        CheckNotRESULT(NULL, hBmp[i] = myLoadBitmap(globalInst, szBuf));
        CheckNotRESULT(NULL, compDC[i] = CreateCompatibleDC(aDC));
        if (!hBmp[i])
            info(FAIL, TEXT("Bmp %d failed"), i);
        if (!compDC[i])
            info(FAIL, TEXT("CompDC %d failed"), i);
        CheckNotRESULT(NULL, stockBmp[i] = (TBITMAP) SelectObject(compDC[i], hBmp[i]));
    }

    CheckNotRESULT(FALSE, PatBlt(aDC, 0, 0, zx, zy, BLACKNESS));

    for (int z = 0; z <= MAX_GLOBE * 15; z++)
    {
        if (z == 1)
            CheckNotRESULT(FALSE, BitBlt(aDC, zx / 2, 0, zx / 2, zy, aDC, 0, 0, SRCCOPY));
        for (i = 0; i < Globes.x; i++)
            for (j = 0; j < Globes.y; j++)
            {
                if ((i + j) % 2 == 0)
                    CheckNotRESULT(FALSE, BitBlt(aDC, i * GLOBE_SIZE, j * GLOBE_SIZE, GLOBE_SIZE, GLOBE_SIZE, compDC[(i + j + z) % MAX_GLOBE], 0, 0,
                           SRCCOPY));
            }
    }

    CheckScreenHalves(aDC);

    for (i = 0; i < MAX_GLOBE; i++)
    {
        CheckNotRESULT(NULL, SelectObject(compDC[i], stockBmp[i]));
        CheckNotRESULT(FALSE, DeleteObject(hBmp[i]));
        CheckNotRESULT(FALSE, DeleteDC(compDC[i]));
    }
    myDeletePal(aDC, hPal);
    myReleaseDC(aDC);
}

/***********************************************************************************
***
***   Paris Test
***
************************************************************************************/

//**********************************************************************************
void
parisTest(int testFunc)
{

    info(ECHO, TEXT("%s - Paris Test"), funcName[testFunc]);

    // if the screen/surface is smaller than the bitmaps we're testing, don't test, not enough room.
    if(SetWindowConstraint(3*PARIS_X, PARIS_Y))
    {
        TDC     aDC = myGetDC(),
                compDC[MAX_PARIS];
        TBITMAP hBmp[MAX_PARIS],
                stockBmp[MAX_PARIS];
        TCHAR   szBuf[30];
        int     i,
                z;
        POINT   Faces = { zx / 2 - (zx / 2 + PARIS_X) / 2, zy - (zy + PARIS_Y) / 2 };
        HPALETTE hPal[MAX_PARIS],
                aPal;

        CheckNotRESULT(NULL, aPal = myCreateNaturalPal(aDC));

        // make sure our width is divisable by 4
        zx = (zx/4)*4;

        for (i = 0; i < MAX_PARIS; i++)
        {
            _sntprintf(szBuf, countof(szBuf), TEXT("PARIS%d"), i);
            CheckNotRESULT(NULL, compDC[i] = CreateCompatibleDC(aDC));
            CheckNotRESULT(NULL, hBmp[i] = myLoadBitmap(globalInst, szBuf));
            if (!hBmp[i])
                info(FAIL, TEXT("Bmp %d failed"), i);
            if (!compDC[i])
                info(FAIL, TEXT("CompDC %d failed"), i);
            CheckNotRESULT(NULL, hPal[i] = myCreateNaturalPal(compDC[i]));
            CheckNotRESULT(NULL, stockBmp[i] = (TBITMAP) SelectObject(compDC[i], hBmp[i]));
        }

        CheckNotRESULT(FALSE, PatBlt(aDC, 0, 0, zx, zy, BLACKNESS));

        CheckNotRESULT(FALSE, BitBlt(aDC, Faces.x, Faces.y, PARIS_X, PARIS_Y, compDC[0], 0, 0, SRCCOPY));
        CheckNotRESULT(FALSE, BitBlt(aDC, zx / 2, 0, zx / 2, zy, aDC, 0, 0, SRCCOPY));

        CheckNotRESULT(FALSE, BitBlt(aDC, 0, 0, 30, 30, aDC, 0, 0, SRCCOPY));

        for (i = 0; i < 10; i++)
        {
            for (z = 0; z < MAX_PARIS; z++)
            {
                CheckNotRESULT(FALSE, BitBlt(aDC, Faces.x, Faces.y, PARIS_X, PARIS_Y, compDC[z], 0, 0, SRCCOPY));
            }
            for (z = MAX_PARIS - 2; z > 0; z--)
            {
                CheckNotRESULT(FALSE, BitBlt(aDC, Faces.x, Faces.y, PARIS_X, PARIS_Y, compDC[z], 0, 0, SRCCOPY));
            }
        }

        CheckNotRESULT(FALSE, BitBlt(aDC, Faces.x, Faces.y, PARIS_X, PARIS_Y, compDC[0], 0, 0, SRCCOPY));

        CheckScreenHalves(aDC);

        for (i = 0; i < MAX_PARIS; i++)
        {
            CheckNotRESULT(NULL, SelectObject(compDC[i], stockBmp[i]));
            CheckNotRESULT(FALSE, DeleteObject(hBmp[i]));
            myDeletePal(compDC[i], hPal[i]);
            CheckNotRESULT(FALSE, DeleteDC(compDC[i]));
        }

        myDeletePal(aDC, aPal);
        myReleaseDC(aDC);
        SetWindowConstraint(0,0);
    }
    else info(DETAIL, TEXT("Screen too small for test, skipping"));
}

//**********************************************************************************
void
TestRestoreDC0(int testFunc)
{

    info(ECHO, TEXT("%s - Call RestoreDC(hdc, 0)"), funcName[testFunc]);

    int     iDCSaved;
    BOOL    bDCRestored;
    TDC     hdc = myGetDC();

    iDCSaved = SaveDC(hdc);
    info(DETAIL, TEXT("SaveDC() return %d"), iDCSaved);

    SetLastError(ERROR_SUCCESS);
    CheckForRESULT(0, bDCRestored = RestoreDC(hdc, 0));
    CheckForLastError(ERROR_INVALID_PARAMETER);
    info(DETAIL, TEXT("RestoreDC(hdc,0) return %d"), bDCRestored);

    CheckNotRESULT(0, bDCRestored = RestoreDC(hdc, iDCSaved));

    myReleaseDC(hdc);
}

//**********************************************************************************
void ScrollDCBadParam()
{
    info(ECHO, TEXT("Checking ScrollDC bad parameters"));

    TDC hdc = myGetDC();
    HRGN hrgn;
#ifdef UNDER_CE
    RECT rc;
#endif

    CheckNotRESULT(NULL, hrgn = CreateRectRgn(0, 0, 10, 10));
    CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
    CheckNotRESULT(FALSE, PatBlt(hdc, zx/3, zy/3, zx/3, zy/3, BLACKNESS));


    SetLastError(ERROR_SUCCESS);
    CheckForRESULT(0, ScrollDC((TDC) NULL, 0, 0, NULL, NULL, NULL, NULL));
    CheckForLastError(ERROR_INVALID_HANDLE);

    // nt allows diagonal scrolling and doesn't fail on bad hdc's
    #ifdef UNDER_CE
    SetLastError(ERROR_SUCCESS);
    CheckForRESULT(FALSE, ScrollDC(g_hdcBAD, 0, 0, &rc, &rc, hrgn, &rc));
    CheckForLastError(ERROR_INVALID_HANDLE);

    SetLastError(ERROR_SUCCESS);
    CheckForRESULT(FALSE, ScrollDC(hdc, 1, 1, &rc, &rc, hrgn, &rc));
    CheckForLastError(ERROR_INVALID_PARAMETER);

    SetLastError(ERROR_SUCCESS);
    CheckForRESULT(FALSE, ScrollDC(hdc, -1, 1, &rc, &rc, hrgn, &rc));
    CheckForLastError(ERROR_INVALID_PARAMETER);

    SetLastError(ERROR_SUCCESS);
    CheckForRESULT(FALSE, ScrollDC(hdc, 1, -1, &rc, &rc, hrgn, &rc));
    CheckForLastError(ERROR_INVALID_PARAMETER);

    SetLastError(ERROR_SUCCESS);
    CheckForRESULT(FALSE, ScrollDC(hdc, -1, -1, &rc, &rc, hrgn, &rc));
    CheckForLastError(ERROR_INVALID_PARAMETER);
    #endif

    CheckNotRESULT(FALSE, PatBlt(hdc, zx/3, zy/3, zx/3, zy/3, WHITENESS));
    CheckAllWhite(hdc, FALSE, 0);

    CheckNotRESULT(FALSE, DeleteObject(hrgn));
    myReleaseDC(hdc);
}

//**********************************************************************************
void SimpleScrollDCTest()
{
    info(ECHO, TEXT("SimpleScrollDCTest"));
    TDC hdc = myGetDC();
    HRGN hrgn;

    CheckNotRESULT(NULL, hrgn = CreateRectRgn(0, 0, zx, zy));

    // this test relies on the clip region to drag
    // the color around on the screen

    CheckNotRESULT(ERROR, SelectClipRgn(hdc, hrgn));

    int stepy = 0;
    int stepx = 0;
    // zx/2+1 just in case we're an odd sized window,
    // we want to go all the way to the edge
    int rcWidth = zx/2 + 1;
    int rcHeight = zy/2 + 1;

    if(!hdc)
    {
        info(ABORT, TEXT("dc creation failed"));
        return;
    }

    CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));

    CheckNotRESULT(FALSE, PatBlt(hdc, 1, 1, rcWidth, rcHeight, WHITENESS));

    info(DETAIL, TEXT("Using ScrollDc to fill the surface with white"));

    // scroll us to into the upper left corner
    CheckNotRESULT(FALSE, ScrollDC(hdc, -1, 0, NULL, NULL, NULL, NULL));

    // scroll us to into the upper left corner
    CheckNotRESULT(FALSE, ScrollDC(hdc, 0, -1, NULL, NULL, NULL, NULL));

    // scroll the rectangle to the far right
    for(stepx=0; stepx <= 2; stepx++)
    {
       CheckNotRESULT(FALSE, ScrollDC(hdc, zx/4, 0, NULL, NULL, NULL, NULL));
    }

    for(stepy=0; stepy<= 2; stepy++)
    {
       CheckNotRESULT(FALSE, ScrollDC(hdc, 0, zy/4, NULL, NULL, NULL, NULL));
    }

    // we should have scrolled makeing the whole screen white
    CheckAllWhite(hdc, FALSE, 0);

    CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));

    CheckNotRESULT(FALSE, PatBlt(hdc, zx-rcWidth-1, zy-rcHeight-1, rcWidth, rcHeight, WHITENESS));

    info(DETAIL, TEXT("Using ScrollDc to fill the surface with white"));

    // scroll us to into the lower right corner
    CheckNotRESULT(FALSE, ScrollDC(hdc, 1, 0, NULL, NULL, NULL, NULL));

    // scroll us to into the lower right corner
    CheckNotRESULT(FALSE, ScrollDC(hdc, 0, 1, NULL, NULL, NULL, NULL));

    // scroll the rectangle to the far right
    for(stepx=0; stepx <= 2; stepx++)
    {
       CheckNotRESULT(FALSE, ScrollDC(hdc, -zx/4, 0, NULL, NULL, NULL, NULL));
    }

    for(stepy=0; stepy<= 2; stepy++)
       CheckNotRESULT(FALSE, ScrollDC(hdc, 0, -zy/4, NULL, NULL, NULL, NULL));

    // we should have scrolled makeing the whole screen white
    CheckAllWhite(hdc, FALSE, 0);

    CheckNotRESULT(NULL, SelectClipRgn(hdc, NULL));
    CheckNotRESULT(FALSE, DeleteObject(hrgn));
    myReleaseDC(hdc);
}

//**********************************************************************************
void ScrollDCTest()
{
    info(ECHO, TEXT("ScrollDCTest"));

    TDC hdc = myGetDC();
    int stepy = 0;
    int stepx = 0;
    int nx = 0, ny = 0;
    int nIteration = 0;
    RECT rcInvalidated = {0};
    HRGN hrgn;

    CheckNotRESULT(NULL, hrgn = CreateRectRgn(0, 0, zx, zy));

    if (DCFlag == EGDI_Default || DCFlag == EGDI_Display)
    {
        CheckNotRESULT(ERROR, SelectClipRgn(hdc, hrgn));
    }

    for(nIteration = 0; nIteration < 2; nIteration++)
    {
        RECT rcClipRect = { 0, 0, zx, zy};
        RECT *lprcClipRect = NULL;

        if(1 == nIteration)
        {
            info(DETAIL, TEXT("ScrollDc with a clip rectangle"));
            rcClipRect.left = rand() % zx/4;
            rcClipRect.top = rand() % zy/4;
            // 0 - 3zn/4 + zn/4 to be in the bottom/right 3/4th of the screen.
            // +-6 so bottom - top - 4 != 0
            rcClipRect.right = (rand() % (3*zx/4 - 6)) + zx/4 + 6;
            rcClipRect.bottom = (rand() % (3* zy/4 - 6)) + zy/4 + 6;
            lprcClipRect = & rcClipRect;
        }
        else info(DETAIL, TEXT("ScrollDc without a clip rectangle"));

        // -4 /+ 1 so the width/height of the box to draw isn't bigger than the
        // clip rect, 4 so it doesn't step off when we scroll, which will cause a failure.
        int rcWidth = (rand() % (rcClipRect.right - rcClipRect.left - 4)) + 1;
        int rcHeight = (rand() % (rcClipRect.bottom - rcClipRect.top - 4)) + 1;
        int rcTravelx = rcClipRect.right - rcClipRect.left - rcWidth -1;
        int rcTravely = rcClipRect.bottom - rcClipRect.top - rcHeight -1;
        HBRUSH hbr = NULL;

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

        CheckNotRESULT(NULL, hbr = CreateSolidBrush(randColorRef()));

        if(hbr)
        {
            RECT rc = { rcClipRect.left + 1, rcClipRect.top + 1, rcClipRect.left + rcWidth, rcClipRect.top + rcHeight};
            CheckNotRESULT(FALSE, FillRect(hdc, &rc, hbr));
            CheckNotRESULT(FALSE, DeleteObject(hbr));
        }

        for(stepx=0, nx=0; nx + stepx + 1 < rcTravelx; nx += stepx++)
        {
           CheckNotRESULT(FALSE, ScrollDC(hdc, stepx, 0, NULL, lprcClipRect, NULL, &rcInvalidated));

           if(!stepx && rcInvalidated.left !=0 && rcInvalidated.right !=0 && rcInvalidated.top != 0 && rcInvalidated.bottom !=0)
               info(FAIL, TEXT("ScrollDC set the invalidated when no scroll performed t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
           if(stepx && (rcInvalidated.left != rcClipRect.left || rcInvalidated.right != rcClipRect.left + stepx || rcInvalidated.top != rcClipRect.top || rcInvalidated.bottom != rcClipRect.bottom))
               info(FAIL, TEXT("ScrollDC didn't set the invalidated rect correct t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
        }

        CheckNotRESULT(NULL, hbr = CreateSolidBrush(randColorRef()));
        if(hbr)
        {
            RECT rc = { rcClipRect.left + nx + 1, rcClipRect.top + ny + 1, rcClipRect.left + rcWidth + nx + 1, rcClipRect.top + rcHeight + ny + 1};
            CheckNotRESULT(FALSE, FillRect(hdc, &rc, hbr));
            CheckNotRESULT(FALSE, DeleteObject(hbr));
        }

        for(stepy=0, ny = 0; ny + stepy + 1 < rcTravely; ny += stepy++)
        {
           CheckNotRESULT(FALSE, ScrollDC(hdc, 0, stepy, NULL, lprcClipRect, NULL, &rcInvalidated));

           if(!stepy && rcInvalidated.left !=0 && rcInvalidated.right !=0 && rcInvalidated.top != 0 && rcInvalidated.bottom !=0)
               info(FAIL, TEXT("ScrollDC set the invalidated when no scroll performed t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
           if(stepy && (rcInvalidated.left != rcClipRect.left || rcInvalidated.right != rcClipRect.right  || rcInvalidated.top != rcClipRect.top || rcInvalidated.bottom != rcClipRect.top + stepy))
               info(FAIL, TEXT("ScrollDC didn't set the invalidated rect correct t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
        }

        CheckNotRESULT(NULL, hbr = CreateSolidBrush(randColorRef()));
        if(hbr)
        {
            RECT rc = { rcClipRect.left + nx + 1, rcClipRect.top + ny + 1, rcClipRect.left + rcWidth + nx + 1, rcClipRect.top + rcHeight + ny + 1};
            CheckNotRESULT(FALSE, FillRect(hdc, &rc, hbr));
            CheckNotRESULT(FALSE, DeleteObject(hbr));
        }

        for(stepx=0; nx - stepx >= 0; nx -= stepx++)
        {
           CheckNotRESULT(FALSE, ScrollDC(hdc, -stepx, 0, NULL, lprcClipRect, NULL, &rcInvalidated));

           if(!stepx && rcInvalidated.left !=0 && rcInvalidated.right !=0 && rcInvalidated.top != 0 && rcInvalidated.bottom !=0)
               info(FAIL, TEXT("ScrollDC set the invalidated when no scroll performed t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
           if(stepx && (rcInvalidated.left != rcClipRect.right - stepx || rcInvalidated.right != rcClipRect.right  || rcInvalidated.top != rcClipRect.top || rcInvalidated.bottom != rcClipRect.bottom))
               info(FAIL, TEXT("ScrollDC didn't set the invalidated rect correct t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
        }

        CheckNotRESULT(NULL, hbr = CreateSolidBrush(randColorRef()));
        if(hbr)
        {
            RECT rc = { rcClipRect.left + nx + 1, rcClipRect.top + ny + 1, rcClipRect.left + rcWidth + nx + 1, rcClipRect.top + rcHeight + ny + 1};
            CheckNotRESULT(FALSE, FillRect(hdc, &rc, hbr));
            CheckNotRESULT(FALSE, DeleteObject(hbr));
        }

        for(stepy=0; ny - stepy >= 0; ny -= stepy++)
        {
           CheckNotRESULT(FALSE, ScrollDC(hdc, 0, -stepy, NULL, lprcClipRect, NULL, &rcInvalidated));

           if(!stepy && rcInvalidated.left !=0 && rcInvalidated.right !=0 && rcInvalidated.top != 0 && rcInvalidated.bottom !=0)
               info(FAIL, TEXT("ScrollDC set the invalidated when no scroll performed t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
           if(stepy && (rcInvalidated.left != rcClipRect.left || rcInvalidated.right != rcClipRect.right  || rcInvalidated.top != rcClipRect.bottom - stepy || rcInvalidated.bottom != rcClipRect.bottom))
               info(FAIL, TEXT("ScrollDC didn't set the invalidated rect correct t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
        }

        CheckNotRESULT(FALSE, PatBlt(hdc, rcClipRect.left + 1, rcClipRect.top + 1, rcClipRect.left + rcWidth, rcClipRect.top + rcHeight, WHITENESS));

        CheckAllWhite(hdc, FALSE, 0);
    }

    CheckNotRESULT(ERROR, SelectClipRgn(hdc, NULL));
    CheckNotRESULT(FALSE, DeleteObject(hrgn));
    myReleaseDC(hdc);

}

//**********************************************************************************
void ScrollDCScrollTest()
{
    info(ECHO, TEXT("ScrollDCScrollTest"));

    TDC hdc = myGetDC();

    for(int i = 0; i < 6; i++)
    {
        RECT rcScroll = { rand() % zx/8 + zx/4, rand() % zy/8 + zy/4, rand() % zx/8 + 5*zx/8, rand() % zy/8 + 5*zy/8};
        RECT rcClip =  { rand() % zx/8 + zx/4, rand() % zy/8 + zy/4, rand() % zx/8 + 5*zx/8, rand() % zy/8 + 5*zy/8};
        RECT rcRgn = {0};
        HRGN hrgnInvalidated = NULL;
        RECT * lprcScroll = NULL;
        RECT * lprcClip = NULL;

        RECT rcBlackRect = {0};
        RECT rcClipRect = {0};
        RECT rcInvalidated = {0};
        switch(i)
        {
            case 0:
                CheckNotRESULT(NULL, hrgnInvalidated = CreateRectRgn(0,0,0,0));
            case 1:
                if(hrgnInvalidated)
                    info(DETAIL, TEXT("testing scroll rect w/ hrgn"));
                else info(DETAIL, TEXT("testing scroll rect"));
                lprcScroll = &rcScroll;
                rcClipRect = rcScroll;
                lprcClip = NULL;

                break;
            case 2:
                CheckNotRESULT(NULL, hrgnInvalidated = CreateRectRgn(0,0,0,0));
            case 3:
                if(hrgnInvalidated)
                    info(DETAIL, TEXT("testing clip rect w/ hrgn"));
                else info(DETAIL, TEXT("testing clip rect"));
                rcClipRect = rcClip;
                lprcClip = &rcClip;
                lprcScroll = NULL;
                break;
            case 4:
                CheckNotRESULT(NULL, hrgnInvalidated = CreateRectRgn(0,0,0,0));
            case 5:
                if(hrgnInvalidated)
                    info(DETAIL, TEXT("testing scroll and clip rect w/ hrgn"));
                else info(DETAIL, TEXT("testing scroll and clip rect"));
                lprcScroll = &rcScroll;
                lprcClip = &rcClip;
                rcClipRect.left = max(rcScroll.left, rcClip.left);
                rcClipRect.right = min(rcScroll.right, rcClip.right);
                rcClipRect.top = max(rcScroll.top, rcClip.top);
                rcClipRect.bottom = min(rcScroll.bottom, rcClip.bottom);
                break;
        }
        rcBlackRect.left = (rand() % (rcClipRect.right - rcClipRect.left - 1)) + (rcClipRect.left + 1);
        rcBlackRect.right = (rand() % (rcClipRect.right - rcBlackRect.left)) + rcBlackRect.left;
        rcBlackRect.top = (rand() % (rcClipRect.bottom - rcClipRect.top - 1)) + (rcClipRect.top + 1);
        rcBlackRect.bottom = (rand() % (rcClipRect.bottom - rcBlackRect.top)) + rcBlackRect.top;

        int nWidth = rcBlackRect.right - rcBlackRect.left;
        int nHeight = rcBlackRect.bottom - rcBlackRect.top;
        int step = 0;

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
        CheckNotRESULT(FALSE, PatBlt(hdc, rcBlackRect.left, rcBlackRect.top, nWidth, nHeight, BLACKNESS));

        info(DETAIL, TEXT("scrolling to the right"));
        for(step=0; step <= rcClipRect.right - rcBlackRect.left; step++)
        {
            CheckNotRESULT(FALSE, ScrollDC(hdc, 1, 0, lprcScroll, lprcClip, hrgnInvalidated, &rcInvalidated));

            if(hrgnInvalidated)
            {
                if(SIMPLEREGION == GetRgnBox(hrgnInvalidated, &rcRgn))
                {
                    if(rcRgn.left != rcInvalidated.left || rcRgn.right != rcInvalidated.right || rcRgn.top != rcInvalidated.top || rcRgn.bottom != rcInvalidated.bottom)
                        info(FAIL, TEXT("Hrgn mismatch"));
                }
                else info(DETAIL, TEXT("complex region"));
            }
            if(rcInvalidated.left != rcClipRect.left || rcInvalidated.top != rcClipRect.top ||
                    rcInvalidated.right != rcClipRect.left+1 || rcInvalidated.bottom != rcClipRect.bottom)
                info(FAIL, TEXT("ScrollDC didn't set the invalidated rect correct t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
        }
        // just scrolled off of the area
        CheckAllWhite(hdc, FALSE, 0);

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
        CheckNotRESULT(FALSE, PatBlt(hdc, rcBlackRect.left, rcBlackRect.top, nWidth, nHeight, BLACKNESS));

        info(DETAIL, TEXT("scrolling to the left"));
        for(step=0; step <= rcBlackRect.right - rcClipRect.left; step++)
        {
           CheckNotRESULT(FALSE, ScrollDC(hdc, -1, 0, lprcScroll, lprcClip, hrgnInvalidated, &rcInvalidated));

           if(hrgnInvalidated)
           {
                if(SIMPLEREGION == GetRgnBox(hrgnInvalidated, &rcRgn))
                {
                    if(rcRgn.left != rcInvalidated.left || rcRgn.right != rcInvalidated.right || rcRgn.top != rcInvalidated.top || rcRgn.bottom != rcInvalidated.bottom)
                        info(FAIL, TEXT("Hrgn mismatch"));
                }
                else info(DETAIL, TEXT("complex region"));
           }
           if(rcInvalidated.left != rcClipRect.right -1 || rcInvalidated.top != rcClipRect.top ||
                    rcInvalidated.right != rcClipRect.right || rcInvalidated.bottom != rcClipRect.bottom)
                info(FAIL, TEXT("ScrollDC didn't set the invalidated rect correct t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
        }
        // just scrolled off of the area
        CheckAllWhite(hdc, FALSE, 0);

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
        CheckNotRESULT(FALSE, PatBlt(hdc, rcBlackRect.left, rcBlackRect.top, nWidth, nHeight, BLACKNESS));

        info(DETAIL, TEXT("scrolling up"));
        for(step=0; step <= rcClipRect.bottom - rcBlackRect.top; step++)
        {
           CheckNotRESULT(FALSE, ScrollDC(hdc, 0, 1, lprcScroll, lprcClip, hrgnInvalidated, &rcInvalidated));

           if(hrgnInvalidated)
           {
                if(SIMPLEREGION == GetRgnBox(hrgnInvalidated, &rcRgn))
                {
                    if(rcRgn.left != rcInvalidated.left || rcRgn.right != rcInvalidated.right || rcRgn.top != rcInvalidated.top || rcRgn.bottom != rcInvalidated.bottom)
                        info(FAIL, TEXT("Hrgn mismatch"));
                }
                else info(DETAIL, TEXT("complex region"));
           }
           if(rcInvalidated.left != rcClipRect.left || rcInvalidated.top != rcClipRect.top ||
                    rcInvalidated.right != rcClipRect.right || rcInvalidated.bottom != rcClipRect.top + 1)
                info(FAIL, TEXT("ScrollDC didn't set the invalidated rect correct t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
        }
        // just scrolled off of the area
        CheckAllWhite(hdc, FALSE, 0);

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
        CheckNotRESULT(FALSE, PatBlt(hdc, rcBlackRect.left, rcBlackRect.top, nWidth, nHeight, BLACKNESS));

        info(DETAIL, TEXT("scrolling down"));
        for(step=0; step <= rcBlackRect.bottom - rcClipRect.top; step++)
        {
           CheckNotRESULT(FALSE, ScrollDC(hdc, 0, -1, lprcScroll, lprcClip, hrgnInvalidated, &rcInvalidated));

           if(hrgnInvalidated)
           {
                if(SIMPLEREGION == GetRgnBox(hrgnInvalidated, &rcRgn))
                {
                    if(rcRgn.left != rcInvalidated.left || rcRgn.right != rcInvalidated.right || rcRgn.top != rcInvalidated.top || rcRgn.bottom != rcInvalidated.bottom)
                        info(FAIL, TEXT("Hrgn mismatch"));
                }
                else info(DETAIL, TEXT("complex region"));
           }
           if(rcInvalidated.left != rcClipRect.left || rcInvalidated.top != rcClipRect.bottom -1 ||
                    rcInvalidated.right != rcClipRect.right || rcInvalidated.bottom != rcClipRect.bottom)
                info(FAIL, TEXT("ScrollDC didn't set the invalidated rect correct t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
        }
        // just scrolled off of the area
        CheckAllWhite(hdc, FALSE, 0);

        if(hrgnInvalidated)
        {
            CheckNotRESULT(FALSE, DeleteObject(hrgnInvalidated));
            hrgnInvalidated = NULL;
        }
    }

    myReleaseDC(hdc);
}

//**********************************************************************************
// This test operates by using the dummy display driver's ability to load and unload.  Every time the driver is reloaded
// it will read in the GPE registry key that's used to control the DPI and other aspects, thus this lets us dynamically change a registry
// key value that's usually only read once at boot during the initial load of the system display driver.

// THIS TEST DOES NOT USE TDC'S BECAUSE IT'S TESTING THE BEHAVIOR OF THE SECONDARY DISPLAY DRIVER ONLY.
void
DeviceCapsGPERegTest(TCHAR *tcName, int Index)
{
    info(ECHO, TEXT("DeviceCapsGPERegTest - %s"), tcName);
// ce specific behavior.
#ifdef UNDER_CE
    HKEY hKey;
    DWORD dwOldValue;
    BOOL bOldValueValid = FALSE;

    DWORD dwValueReturned;
    DWORD dwDisposition;
    // interesting values to test with.
    DWORD dwMax = UINT_MAX;
    DWORD dwValues[] = { 0, 100, 200, dwMax - 2, dwMax - 1, dwMax };
    // base registry key for GPE.
    TCHAR szGPERegKey[] = TEXT("Drivers\\Display\\GPE");

    // Load the test display driver to verify it's existance.  If it doesn't load then skip the test
    // and output detailed info on why the test is skipped and give remedies on how to make it not skip.
    if(!g_hdcSecondary)
    {
        info(DETAIL, TEXT("Secondary display driver unavailable, test skipped.  Please verify that ddi_test.dll is available and there is sufficient memory for driver verification."));
        return;
    }

    if (CeGetCallerTrust() != OEM_CERTIFY_TRUST)
    {
        info(DETAIL, TEXT("Trust level not high enough to access protected registry key, test skipped."));
        return;
    }

    // retreive the key.
    CheckForRESULT(ERROR_SUCCESS, RegCreateKeyEx(HKEY_LOCAL_MACHINE, szGPERegKey, 0, 0, 0, NULL, 0, &hKey, &dwDisposition));

    // if the key was new, then don't save the old value.
    if(dwDisposition == REG_OPENED_EXISTING_KEY)
    {
        DWORD dwSize, dwType;
        // backup the old key.
        dwSize = sizeof(DWORD);
        if(ERROR_SUCCESS == RegQueryValueEx(hKey, tcName, NULL, &dwType, (BYTE*) &dwOldValue, &dwSize))
        {
            // the registry key type should be DWORD
            CheckForRESULT(REG_DWORD, dwType);
            // the size should be a DWORD
            CheckForRESULT(sizeof(DWORD), dwSize);
            bOldValueValid = TRUE;
        }
    }

    // step through the interesting values for this registry key and verify the driver returns them.
    for(int i=0; i < countof(dwValues); i++)
    {
        // change the registry key to the entry to something interesting
        CheckForRESULT(ERROR_SUCCESS, RegSetValueEx(hKey, tcName, NULL, REG_DWORD, (BYTE*) &dwValues[i], sizeof(DWORD)));

        // reset the verification driver that's already loaded.
        ResetVerifyDriver();

        // retrive the registry key set
        dwValueReturned = GetDeviceCaps(g_hdcSecondary, Index);

        // if the value returned through the middleware doesn't match the value set in the registry, flag an error.
        if(dwValueReturned != dwValues[i])
        {
            info(FAIL, TEXT("The registry key value set doesn't match the value the driver returned."));
            info(FAIL, TEXT("Retrieved %d, Expected %d"), dwValueReturned, dwValues[i]);
        }
    }

    // if the key was new, don't restore the old value
    if(dwDisposition == REG_OPENED_EXISTING_KEY)
    {
        // restore the old value if we got an old value, otherwise delete it.
        if(bOldValueValid)
        {
            CheckForRESULT(ERROR_SUCCESS, RegSetValueEx(hKey, tcName, NULL, REG_DWORD, (BYTE*) &dwOldValue, sizeof(DWORD)));
        }
        else
        {
            CheckForRESULT(ERROR_SUCCESS, RegDeleteValue(hKey, tcName));
        }
    }

    // close the open reg key.
    CheckForRESULT(ERROR_SUCCESS, RegCloseKey(hKey));

    // if the key was created, then delete it.
    if(dwDisposition == REG_CREATED_NEW_KEY)
        CheckForRESULT(ERROR_SUCCESS, RegDeleteKey(HKEY_LOCAL_MACHINE, szGPERegKey));

    // now that everything is back to normal, reset the verification driver to prevent future failures.
    ResetVerifyDriver();

#else
    info(DETAIL, TEXT("This test is only valid on CE."));
#endif
}

void
GetDeviceCapsBadParamTest()
{
    info(DETAIL, TEXT("GetDeviceCapsBadParamTest"));
    TDC hdc = myGetDC();

    SetLastError(ERROR_SUCCESS);
    CheckForRESULT(0, GetDeviceCaps(g_hdcBAD, DRIVERVERSION));
    CheckForLastError(ERROR_INVALID_HANDLE);

    SetLastError(ERROR_SUCCESS);
    CheckForRESULT(0, GetDeviceCaps(g_hdcBAD2, DRIVERVERSION));
    CheckForLastError(ERROR_INVALID_HANDLE);

// CE assumes primary when given a NULL DC.
#ifndef UNDER_CE
    SetLastError(ERROR_SUCCESS);
    CheckForRESULT(0, GetDeviceCaps((TDC) NULL, DRIVERVERSION));
    CheckForLastError(ERROR_INVALID_HANDLE);
#endif

    SetLastError(ERROR_SUCCESS);
    CheckForRESULT(0, GetDeviceCaps(hdc, -1));
    CheckForLastError(ERROR_INVALID_PARAMETER);

    SetLastError(ERROR_SUCCESS);
    CheckNotRESULT(0, GetDeviceCaps(hdc, DRIVERVERSION));
    CheckForLastError(ERROR_SUCCESS);

    myReleaseDC(hdc);
}

void
OuputExtendedCapsData(int nCapsValue, NameValPair *pnvCapabilityInfo, int nCapabilityInfoCount)
{
    for(int i = 0; i < nCapabilityInfoCount; i++)
    {
        if(nCapsValue & pnvCapabilityInfo[i].dwVal)
            info(DETAIL, TEXT("    %s"), pnvCapabilityInfo[i].szName);
    }
}

#define FRAMEBUFFER 200

void
OutputDeviceCapsInfo(TDC hdc)
{
        NameValPair nvDeviceCaps[] = {
                                NAMEVALENTRY(DRIVERVERSION),
                                NAMEVALENTRY(TECHNOLOGY),
                                NAMEVALENTRY(HORZSIZE),
                                NAMEVALENTRY(VERTSIZE),
                                NAMEVALENTRY(HORZRES),
                                NAMEVALENTRY(VERTRES),
                                NAMEVALENTRY(LOGPIXELSX),
                                NAMEVALENTRY(LOGPIXELSY),
                                NAMEVALENTRY(BITSPIXEL),
                                NAMEVALENTRY(PLANES),
                                NAMEVALENTRY(NUMBRUSHES),
                                NAMEVALENTRY(NUMPENS),
                                NAMEVALENTRY(NUMFONTS),
                                NAMEVALENTRY(NUMCOLORS),
                                NAMEVALENTRY(ASPECTX),
                                NAMEVALENTRY(ASPECTY),
                                NAMEVALENTRY(ASPECTXY),
                                NAMEVALENTRY(PDEVICESIZE),
                                NAMEVALENTRY(CLIPCAPS),
                                NAMEVALENTRY(SIZEPALETTE),
                                NAMEVALENTRY(NUMRESERVED),
                                NAMEVALENTRY(COLORRES),
                                NAMEVALENTRY(PHYSICALWIDTH),
                                NAMEVALENTRY(PHYSICALHEIGHT),
                                NAMEVALENTRY(PHYSICALOFFSETX),
                                NAMEVALENTRY(PHYSICALOFFSETY),
                                NAMEVALENTRY(RASTERCAPS),
                                NAMEVALENTRY(CURVECAPS),
                                NAMEVALENTRY(LINECAPS),
                                NAMEVALENTRY(POLYGONALCAPS),
                                NAMEVALENTRY(TEXTCAPS),
                                NAMEVALENTRY(SHADEBLENDCAPS),

#ifdef UNDER_CE
                                NAMEVALENTRY(FRAMEBUFFER),
#else
                                NAMEVALENTRY(VREFRESH),
                                NAMEVALENTRY(SCALINGFACTORX),
                                NAMEVALENTRY(SCALINGFACTORY),
                                NAMEVALENTRY(BLTALIGNMENT),
                                NAMEVALENTRY(COLORMGMTCAPS),
#endif
                                };

#ifndef UNDER_CE
    NameValPair nvColorMgmtCaps[] = {
                                NAMEVALENTRY(CM_CMYK_COLOR),
                                NAMEVALENTRY(CM_DEVICE_ICM),
                                NAMEVALENTRY(CM_GAMMA_RAMP),
                                NAMEVALENTRY(CM_NONE)
                                };
#endif

    NameValPair nvShadeBlendCaps[] = {
                                NAMEVALENTRY(SB_CONST_ALPHA),
                                NAMEVALENTRY(SB_GRAD_RECT),
                                NAMEVALENTRY(SB_GRAD_TRI),
                                NAMEVALENTRY(SB_PIXEL_ALPHA),
                                NAMEVALENTRY(SB_PREMULT_ALPHA)
                                };

    NameValPair nvTechnologyCaps[] = {
                                NAMEVALENTRY(DT_PLOTTER),
                                NAMEVALENTRY(DT_RASDISPLAY),
                                NAMEVALENTRY(DT_RASPRINTER),
                                NAMEVALENTRY(DT_RASCAMERA),
                                NAMEVALENTRY(DT_CHARSTREAM),
                                NAMEVALENTRY(DT_DISPFILE)
                                };
    NameValPair nvRasterCaps[] = {
                                NAMEVALENTRY(RC_BANDING),
                                NAMEVALENTRY(RC_BITBLT),
                                NAMEVALENTRY(RC_BITMAP64),
                                NAMEVALENTRY(RC_DI_BITMAP),
                                NAMEVALENTRY(RC_DIBTODEV),
                                NAMEVALENTRY(RC_GDI20_OUTPUT),
                                NAMEVALENTRY(RC_PALETTE),
                                NAMEVALENTRY(RC_SCALING),
#ifdef UNDER_CE
                                NAMEVALENTRY(RC_ROTATEBLT),
#endif
                                NAMEVALENTRY(RC_STRETCHBLT),
                                NAMEVALENTRY(RC_STRETCHDIB)
                                };

    NameValPair nvCurveCaps[] = {
                                NAMEVALENTRY(CC_NONE),
                                NAMEVALENTRY(CC_CHORD),
                                NAMEVALENTRY(CC_CIRCLES),
                                NAMEVALENTRY(CC_ELLIPSES),
                                NAMEVALENTRY(CC_INTERIORS),
                                NAMEVALENTRY(CC_PIE),
                                NAMEVALENTRY(CC_ROUNDRECT),
                                NAMEVALENTRY(CC_STYLED),
                                NAMEVALENTRY(CC_WIDE),
                                NAMEVALENTRY(CC_WIDESTYLED)
                                };

    NameValPair nvLineCaps[] = {
                                NAMEVALENTRY(LC_NONE),
                                NAMEVALENTRY(LC_INTERIORS),
                                NAMEVALENTRY(LC_MARKER),
                                NAMEVALENTRY(LC_POLYLINE),
                                NAMEVALENTRY(LC_POLYMARKER),
                                NAMEVALENTRY(LC_STYLED),
                                NAMEVALENTRY(LC_WIDE),
                                NAMEVALENTRY(LC_WIDESTYLED)
                                };

    NameValPair nvPolygonCaps[] = {
                                NAMEVALENTRY(PC_NONE),
                                NAMEVALENTRY(PC_INTERIORS),
                                NAMEVALENTRY(PC_POLYGON),
                                NAMEVALENTRY(PC_RECTANGLE),
                                NAMEVALENTRY(PC_SCANLINE),
                                NAMEVALENTRY(PC_STYLED),
                                NAMEVALENTRY(PC_WIDE),
                                NAMEVALENTRY(PC_WIDESTYLED),
                                NAMEVALENTRY(PC_WINDPOLYGON)
                                };

    NameValPair nvTextCaps[] = {
                                NAMEVALENTRY(TC_OP_CHARACTER),
                                NAMEVALENTRY(TC_OP_STROKE),
                                NAMEVALENTRY(TC_CP_STROKE),
                                NAMEVALENTRY(TC_CR_90),
                                NAMEVALENTRY(TC_CR_ANY),
                                NAMEVALENTRY(TC_SF_X_YINDEP),
                                NAMEVALENTRY(TC_SA_DOUBLE),
                                NAMEVALENTRY(TC_SA_INTEGER),
                                NAMEVALENTRY(TC_SA_CONTIN),
                                NAMEVALENTRY(TC_EA_DOUBLE),
                                NAMEVALENTRY(TC_IA_ABLE),
                                NAMEVALENTRY(TC_UA_ABLE),
                                NAMEVALENTRY(TC_SO_ABLE),
                                NAMEVALENTRY(TC_RA_ABLE),
                                NAMEVALENTRY(TC_VA_ABLE),
                                NAMEVALENTRY(TC_RESERVED),
                                NAMEVALENTRY(TC_SCROLLBLT)
                                };

    for(int i = 0; i < countof(nvDeviceCaps); i++)
    {
        int nValue;

        nValue = GetDeviceCaps(hdc, nvDeviceCaps[i].dwVal);

        info(DETAIL, TEXT("Capability %s = %d"), nvDeviceCaps[i].szName, nValue);

        switch(nvDeviceCaps[i].dwVal)
        {
            case TECHNOLOGY:
                OuputExtendedCapsData(nValue, nvTechnologyCaps, countof(nvTechnologyCaps));
                break;
            case RASTERCAPS:
                OuputExtendedCapsData(nValue, nvRasterCaps, countof(nvRasterCaps));
                break;
            case CURVECAPS:
                OuputExtendedCapsData(nValue, nvCurveCaps, countof(nvCurveCaps));
                break;
            case LINECAPS:
                OuputExtendedCapsData(nValue, nvLineCaps, countof(nvLineCaps));
                break;
            case POLYGONALCAPS:
                OuputExtendedCapsData(nValue, nvPolygonCaps, countof(nvPolygonCaps));
                break;
            case TEXTCAPS:
                OuputExtendedCapsData(nValue, nvTextCaps, countof(nvTextCaps));
                break;
            case SHADEBLENDCAPS:
                OuputExtendedCapsData(nValue, nvShadeBlendCaps, countof(nvShadeBlendCaps));
                break;
#ifndef UNDER_CE
            case COLORMGMTCAPS:
                OuputExtendedCapsData(nValue, nvColorMgmtCaps, countof(nvColorMgmtCaps));
                break;
#endif
        }
    }
}
void
GetDeviceCapsEntryTest()
{
    info(DETAIL, TEXT("GetDeviceCapsEntryTest"));
    TDC hdc = myGetDC();

    OutputDeviceCapsInfo(hdc);

    myReleaseDC(hdc);
}

void
ExtEscapeInvalidAccess()
{
#ifdef UNDER_CE
    info(ECHO, TEXT("ExtEscapeInvalidAccess"));
    TDC hdc = myGetDC();
    int nEscapes[] = { DRVESC_GETSCREENROTATION,
                                DRVESC_SETSCREENROTATION,
                                DRVESC_GETVIDEOPROTECTION,
                                DRVESC_SETVIDEOPROTECTION,
                                DRVESC_RESTOREVIDEOMEM,
                                DRVESC_SAVEVIDEOMEM,
                                DRVESC_SETGAMMAVALUE,
                                DRVESC_GETGAMMAVALUE };


    for(int i = 0; i < countof(nEscapes); i++)
    {
        info(DETAIL, TEXT("Testing escape code %d."), nEscapes[i]);

        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(-1, ExtEscape(hdc, nEscapes[i], 0, NULL, 0, NULL));
        CheckForLastError(ERROR_INVALID_ACCESS);
    }

    DWORD dwInData = 0x00020001; // DRVESC_GETRAWFRAMEBUFFER;

    info(DETAIL, TEXT("verifying get raw frame buffer unsupported."));
    SetLastError(ERROR_SUCCESS);
    CheckForRESULT(0, ExtEscape(hdc, QUERYESCSUPPORT, sizeof(DWORD), (LPCSTR) &dwInData, 0, NULL));
    CheckForLastError(ERROR_NOT_SUPPORTED);

    myReleaseDC(hdc);
#endif
}


/***********************************************************************************
***
***   APIs
***
************************************************************************************/

//**********************************************************************************
TESTPROCAPI CreateCompatibleDC_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2DC(ECreateCompatibleDC);
    StressPairs(ECreateCompatibleDC);

    // Depth
    globeTest(ECreateCompatibleDC);
    parisTest(ECreateCompatibleDC);
    TestCreateCompatibleDC(ECreateCompatibleDC);

    return getCode();
}

//**********************************************************************************
TESTPROCAPI DeleteDC_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2DC(EDeleteDC);
    StressPairs(EDeleteDC);
    ProgramError(EDeleteDC);

    // Depth
    // None

    return getCode();
}

//**********************************************************************************
TESTPROCAPI GetDCOrgEx_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

#ifdef UNDER_CE
    info(ECHO, TEXT("Currently not implented in Windows CE, placeholder for future test development."));
#else
    // Breadth
    TestGetDCOrgEx(EGetDCOrgEx);

    // Depth
    // None

#endif
    return getCode();
}

//**********************************************************************************
TESTPROCAPI GetDeviceCaps_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2DC(EGetDeviceCaps);
    GetDeviceCapsBadParamTest();
    GetDeviceCapsEntryTest();
    DeviceCapsGPERegTest(TEXT("HorizontalSize"), HORZSIZE);
    DeviceCapsGPERegTest(TEXT("VerticalSize"), VERTSIZE);
    DeviceCapsGPERegTest(TEXT("LogicalPixelsX"), LOGPIXELSX);
    DeviceCapsGPERegTest(TEXT("LogicalPixelsY"), LOGPIXELSY);
    DeviceCapsGPERegTest(TEXT("AspectRatioX"), ASPECTX);
    DeviceCapsGPERegTest(TEXT("AspectRatioY"), ASPECTY);
    DeviceCapsGPERegTest(TEXT("AspectRatioXY"), ASPECTXY);

    //Depth
    // None

    return getCode();
}

//**********************************************************************************
TESTPROCAPI RestoreDC_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2DC(ERestoreDC);
    TestRestoreDC0(ERestoreDC);

    // Depth
    SaveRestoreDCPairs(ERestoreDC);
    RecoverObjectsTest(ERestoreDC);

    return getCode();
}

//**********************************************************************************
TESTPROCAPI SaveDC_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2DC(ESaveDC);
    TestRestoreDC0(ESaveDC);

    // Depth
    SaveRestoreDCPairs(ESaveDC);
    RecoverObjectsTest(ESaveDC);

    return getCode();
}

//**********************************************************************************
TESTPROCAPI ScrollDC_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;
    NOPRINTERDC(NULL, getCode());

    // Breadth
    ScrollDCBadParam();
    SimpleScrollDCTest();

    // Depth
    ScrollDCTest();
    ScrollDCScrollTest();


    return getCode();
}

//**********************************************************************************
TESTPROCAPI CreateDC_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2DC(ECreateDC);

    // Depth
    // None

    return getCode();
}

//**********************************************************************************
TESTPROCAPI ExtEscape_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2DC(EExtEscape);
    ExtEscapeInvalidAccess();

    // Depth
    // None

    return getCode();
}
