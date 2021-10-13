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
    
    DWORD dwQuery = 0x00;

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
        CheckForRESULT(0, DeleteDC(g_hdcBAD));
        CheckForLastError(ERROR_INVALID_HANDLE);

        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(0, DeleteDC(g_hdcBAD2));
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
        CheckForRESULT(-1, ExtEscape((TDC) NULL, QUERYESCSUPPORT, sizeof(DWORD), (LPSTR) &dwQuery, 0, NULL));
        CheckForLastError(ERROR_INVALID_PARAMETER);

        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(-1, ExtEscape(g_hdcBAD, QUERYESCSUPPORT, sizeof(DWORD), (LPSTR) &dwQuery, 0, NULL));
        CheckForLastError(ERROR_INVALID_PARAMETER);

        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(-1, ExtEscape(g_hdcBAD2, QUERYESCSUPPORT, sizeof(DWORD), (LPSTR) &dwQuery, 0, NULL));
        CheckForLastError(ERROR_INVALID_PARAMETER);
        break;
    case EBitmapEscape:
#ifdef UNDER_CE
        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(-1, BitmapEscape((TBITMAP) NULL, QUERYESCSUPPORT, sizeof(DWORD), (LPSTR) &dwQuery, 0, NULL));
        CheckForLastError(ERROR_INVALID_PARAMETER);

        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(-1, BitmapEscape(g_hbmpBAD, QUERYESCSUPPORT, sizeof(DWORD), (LPSTR) &dwQuery, 0, NULL));
        CheckForLastError(ERROR_INVALID_PARAMETER);

        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(-1, BitmapEscape(g_hbmpBAD2, QUERYESCSUPPORT, sizeof(DWORD), (LPSTR) &dwQuery, 0, NULL));
        CheckForLastError(ERROR_INVALID_PARAMETER);
#endif
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

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
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

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
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
    }

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

    if (!IsWritableHDC(aDC))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hPal = myCreateNaturalPal(aDC));

        for (i = 0; i < MAX_GLOBE; i++)
        {
            _sntprintf_s(szBuf, countof(szBuf)-1, TEXT("GLOBE%d"), i);
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
    }

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

        if (!IsWritableHDC(aDC))
            info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
        else
        {
            CheckNotRESULT(NULL, aPal = myCreateNaturalPal(aDC));

            for (i = 0; i < MAX_PARIS; i++)
            {
                _sntprintf_s(szBuf, countof(szBuf)-1, TEXT("PARIS%d"), i);
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
        }
        myReleaseDC(aDC);
        SetWindowConstraint(0,0);
    }
    else
    {
        info(DETAIL, TEXT("Screen too small for test, skipping"));
    }
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

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // prepare the screen
        CheckNotRESULT(NULL, hrgn = CreateRectRgn(0, 0, 10, 10));
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
        CheckNotRESULT(FALSE, PatBlt(hdc, zx/3, zy/3, zx/3, zy/3, BLACKNESS));

        // run bad param tests
        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(0, ScrollDC((TDC) NULL, 0, 0, NULL, NULL, NULL, NULL));
        CheckForLastError(ERROR_INVALID_HANDLE);

        // nt doesn't fail on bad hdc's
#ifdef UNDER_CE
        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(FALSE, ScrollDC(g_hdcBAD, 0, 0, &rc, &rc, hrgn, &rc));
        CheckForLastError(ERROR_INVALID_HANDLE);
#endif 

        // make sure we didn't scroll
        CheckNotRESULT(FALSE, PatBlt(hdc, zx/3, zy/3, zx/3, zy/3, WHITENESS));
        CheckAllWhite(hdc, FALSE, 0);

        // cleanup
        CheckNotRESULT(FALSE, DeleteObject(hrgn));
    }

    myReleaseDC(hdc);
}

//**********************************************************************************
void SimpleScrollDCTest(void)
{
    info(ECHO, TEXT("SimpleScrollDCTest"));
    TDC hdc = myGetDC();
    HRGN hrgn;

    CheckNotRESULT(NULL, hrgn = CreateRectRgn(0, 0, zx, zy));

    // this test relies on the clip region to drag
    // the color around on the screen
    CheckNotRESULT(ERROR, SelectClipRgn(hdc, hrgn));

    int stepy  = 0;
    int stepx  = 0;
    int stepxy = 0;
    // zx/2+1 just in case we're an odd sized window,
    // we want to go all the way to the edge
    int rcWidth  = zx/2 + 1;
    int rcHeight = zy/2 + 1;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // Validating the x and y parameters when scrolled in a single direction.
        //-------------------------------------------------------------------------

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
        CheckNotRESULT(FALSE, PatBlt(hdc, 1, 1, rcWidth, rcHeight, WHITENESS));

        info(DETAIL, TEXT("Using ScrollDc to fill the surface with white: upper-left corner"));

        // scroll us to into the upper left corner
        CheckNotRESULT(FALSE, ScrollDC(hdc, -1, 0, NULL, NULL, NULL, NULL));
        CheckNotRESULT(FALSE, ScrollDC(hdc, 0, -1, NULL, NULL, NULL, NULL));

        // scroll the rectangle to the right
        for (stepx=0; stepx <= 2; stepx++)
        {
            CheckNotRESULT(FALSE, ScrollDC(hdc, zx/4, 0, NULL, NULL, NULL, NULL));
        }

        // scroll the rectangle to the bottom
        for (stepy=0; stepy<= 2; stepy++)
        {
            CheckNotRESULT(FALSE, ScrollDC(hdc, 0, zy/4, NULL, NULL, NULL, NULL));
        }

        // we should have scrolled making the whole screen white
        CheckAllWhite(hdc, FALSE, 0);

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
        CheckNotRESULT(FALSE, PatBlt(hdc, zx-rcWidth-1, zy-rcHeight-1, rcWidth, rcHeight, WHITENESS));

        info(DETAIL, TEXT("Using ScrollDc to fill the surface with white: lower-right corner"));

        // scroll us to into the lower right corner
        CheckNotRESULT(FALSE, ScrollDC(hdc, 1, 0, NULL, NULL, NULL, NULL));
        CheckNotRESULT(FALSE, ScrollDC(hdc, 0, 1, NULL, NULL, NULL, NULL));

        // scroll the rectangle to the left
        for (stepx=0; stepx <= 2; stepx++)
        {
           CheckNotRESULT(FALSE, ScrollDC(hdc, -zx/4, 0, NULL, NULL, NULL, NULL));
        }

        // scroll the rectangle to the bottom
        for (stepy=0; stepy<= 2; stepy++)
        {
           CheckNotRESULT(FALSE, ScrollDC(hdc, 0, -zy/4, NULL, NULL, NULL, NULL));
        }

        // we should have scrolled makeing the whole screen white
        CheckAllWhite(hdc, FALSE, 0);
        
        // Validating the x and y parameters when scrolled diagonally.
        //-------------------------------------------------------------------------

        // Adding Scenario to color the region with black, then draw a white reregion 
        // Filling the region with black
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));

        // drawing a white region that can be scrolled 
        CheckNotRESULT(FALSE, PatBlt(hdc, 1, 1, rcWidth, rcHeight, WHITENESS));
        // move it to the corner using 2 non zero params
        CheckNotRESULT(FALSE, ScrollDC(hdc, -1, -1, NULL, NULL, NULL, NULL));
        for (stepxy=0; stepxy<=2; stepxy++)
        {    
            // Check for result verifies if a function returns what its supposed to return.
            CheckForRESULT(TRUE, ScrollDC(hdc, zx/4, zy/4, NULL, NULL, NULL, NULL));           
        }

        // create a white region that can be scrolled 
        CheckNotRESULT(FALSE, PatBlt(hdc, 1, zy-rcHeight-1, rcWidth, rcHeight, WHITENESS));
        // move it to the corner using 2 non zero params
        CheckNotRESULT(FALSE, ScrollDC(hdc, -1, 1, NULL, NULL, NULL, NULL));
        for (stepxy=0; stepxy<=2; stepxy++)
        {    
            // Check for result verifies if a function returns what its supposed to return.
            CheckForRESULT(TRUE, ScrollDC(hdc, zx/4, -zy/4, NULL, NULL, NULL, NULL));           
        }

        // we should have scrolled from upper left down and from lower left up making the whole screen white.
        CheckAllWhite(hdc, FALSE, 0);
      
        // Below code Checks for diagonal scrolling by comparing screen halves.
        int rcWidthDiag   = zx/4;
        int rcHeightDiag  = zy/4;
        int xOffset       = 0;
        int scrollXOffset = 0;
        int scrollYOffset = 0;

        // +1 to allow it to scroll 1 in the -ve direction just incase the rand val turns out to be the min
        // we dont need a yoffset because we use these rectangle of height and width 1/4 the size of the region; 
        //with starting at middle of the screen height
        xOffset = (GenRand() % zx/4)+1; 

        for(int scrollCase=0; scrollCase<4; scrollCase++)
        {
            switch(scrollCase)
            {
            case 0:
                scrollXOffset =  1; 
                scrollYOffset =  1;
                info(DETAIL, TEXT("ScrollDC with non zero parameters right/down"));
                break;
            case 1:
                scrollXOffset = -1; 
                scrollYOffset =  1;
                info(DETAIL, TEXT("ScrollDC with non zero parameters left/down"));
                break;
            case 2:
                scrollXOffset =  1; 
                scrollYOffset = -1;
                info(DETAIL, TEXT("ScrollDC with non zero parameters up/right"));
                break;
            case 3:
                scrollXOffset = -1; 
                scrollYOffset = -1;
                info(DETAIL, TEXT("ScrollDC with non zero parameters up/left"));
                break;      
            default:
                info(FAIL, TEXT("Invalid diagonal ScrollDC case"));
                break;
            }

            // Fill the region with black
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
            // Draw a rectangle of height and width 1/4 the size of the region; with starting at middle of the screen height
            CheckNotRESULT(FALSE, PatBlt(hdc, xOffset, zy/2, rcWidthDiag, rcHeightDiag, WHITENESS));
            // Scroll with offsets diagonally in both +ve direction
            CheckForRESULT(TRUE, ScrollDC(hdc,scrollXOffset,scrollYOffset, NULL, NULL, NULL, NULL)); 
            // We draw another rectangle with similar height and width on the other half of the screen
            CheckNotRESULT(FALSE, PatBlt(hdc, (zx/2)+xOffset+scrollXOffset,(zy/2)+scrollYOffset, rcWidthDiag, rcHeightDiag, WHITENESS));
            // the two halves should now match
            CheckScreenHalves(hdc);
        }

        CheckNotRESULT(NULL, SelectClipRgn(hdc, NULL));
        CheckNotRESULT(FALSE, DeleteObject(hrgn));
    }

    myReleaseDC(hdc);
}

//**********************************************************************************
void OutputRectInfo(const LPRECT lprcInfo, LPCTSTR szMessage)
{
    if (!szMessage)
        return;

    if (lprcInfo)
    {
        info(DETAIL, TEXT("%s:  l:%3d t:%3d r:%3d b:%3d"), szMessage,
            lprcInfo->left, lprcInfo->top, lprcInfo->right, lprcInfo->bottom);
    }
    else
    {
        info(DETAIL, TEXT("%s:  NULL rect"), szMessage);
    }
}

//**********************************************************************************
// This will loop through a series of randomly generated input parameters to ScrollDC
// and will verify the contents using API validation and visual verification.
//
void RandomScrollDCTest(void)
{
    TDC hdc = myGetDC();

    info(ECHO, TEXT("RandomScrollDCTest"));
    int lzy=zy;

    // if we're doing a getdc(null) or createdc(null), then the task bar is in bounds, so count it.
    if (DCFlag == EFull_Screen || DCFlag == ECreateDC_Primary)
        lzy = GetDeviceCaps(hdc, VERTRES);

    if (!IsWritableHDC(hdc))
    {
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    }
    else
    {
        // define the number of test iterations and how frequently we'll randomly 
        // generate new input parameters
        const UINT nMaxScenarios    = 250;
        const UINT nFramesScrolled  = 10;

        // since visual verification is expensive, decide upfront how many scenarios
        // will be verified using this verification
        const UINT nVisualScenarios = 20;

        // commonly used RECTs
        const RECT rcEmpty  = { 0, 0, 0,  0 };
        const RECT rcClient = { 0, 0, zx, lzy };

        // use a secondary surface to visually verify the ScrollDC result
        TDC     hdcVerify = NULL;
        TBITMAP hbmpStock = NULL;
        TBITMAP hbmpMem   = NULL;

        // fill colors used to distinguish our reference points during scroll
        HBRUSH hBrushBlock = NULL;

        // create a surface which we will use to verify the visual ScrollDC result
        CheckNotRESULT(NULL, hdcVerify = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hbmpMem   = CreateCompatibleBitmap(hdc, zx, lzy));
        CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcVerify, hbmpMem));

        // create a reference brush for our test rendering
        CheckNotRESULT(NULL, hBrushBlock = CreateSolidBrush(randColorRef()));

        for (UINT nCase=0; nCase<nMaxScenarios; nCase++)
        {
            //
            // Initialization
            //

            // randomly select positive or negative scrolling offsets
            const INT dx = (GenRand() % 5) - 2;
            const INT dy = (GenRand() % 5) - 2;

            // ScrollDC input parameters, if needed
            RECT rcScroll = {0};
            RECT rcClip   = {0};

            // actual parameters which will be passed to ScrollDC
            LPRECT lprcScroll = NULL;
            LPRECT lprcClip   = NULL;
            HRGN hrgnUpdate   = NULL;

            // used to define our work area
            RECT rcValidWork = {0};

            // block to be scrolled within the work area, to be verified against the expected rendering
            RECT rcBlock = {0};

            // expected update region
            RECT rcUpdateExpected = {0};
            HRGN hrgnUpdateExpected = NULL;

            // possible update regions, will be combined to form rcUpdateExpected
            RECT rcUpdateX = {0};
            RECT rcUpdateY = {0};

            // randomly initialize lprcScroll
            if (GenRand() % 2)
            {
                rcScroll.left   = (GenRand() % (zx - 1));
                rcScroll.top    = (GenRand() % (lzy - 1));
                rcScroll.right  = (GenRand() % (zx - rcScroll.left)) + rcScroll.left + 1;
                rcScroll.bottom = (GenRand() % (lzy - rcScroll.top))  + rcScroll.top  + 1;
                lprcScroll = &rcScroll;
            }
            else
            {
                lprcScroll = NULL;
            }

            // randomly initialize lprcClip
            if (GenRand() % 2)
            {
                rcClip.left   = (GenRand() % (zx - 1));
                rcClip.top    = (GenRand() % (lzy - 1));
                rcClip.right  = (GenRand() % (zx - rcClip.left)) + rcClip.left + 1;
                rcClip.bottom = (GenRand() % (lzy - rcClip.top))  + rcClip.top  + 1;
                lprcClip = &rcClip;
            }
            else
            {
                lprcClip = NULL;
            }

            // randomly use an hrgnUpdate
            if (GenRand() % 2)
            {
                CheckNotRESULT(NULL, hrgnUpdate = CreateRectRgnIndirect(&rcEmpty));
            }
            else
            {
                hrgnUpdate = NULL;
            }

            // define our scroll workarea
            if (lprcScroll && lprcClip)
            {
                // only the bits inside this rect will be affected by the scroll (if no
                // intersection, no bits will be updated)
                IntersectRect(&rcValidWork, lprcScroll, lprcClip);
            }
            else if (lprcScroll)
            {
                // no clipping region, so restrict our boundary to the scrolled area
                CheckNotRESULT(FALSE, CopyRect(&rcValidWork, lprcScroll));
            }
            else if (lprcClip)
            {
                // no scroll area, so restrict our boundary to the clipping region
                CheckNotRESULT(FALSE, CopyRect(&rcValidWork, lprcClip));
            }
            else
            {
                // if both lprcScroll and lprcClip are NULL, the entire work area will be used
                CheckNotRESULT(FALSE, CopyRect(&rcValidWork, &rcClient));
            }
            
            // only bits within the work area will be scrolled, so verify our work area is valid
            if (EqualRect(&rcValidWork, &rcEmpty))
            {
                // work area is empty, so we will have nothing to render
                CheckNotRESULT(FALSE, CopyRect(&rcBlock, &rcEmpty));
                CheckNotRESULT(FALSE, CopyRect(&rcUpdateExpected, &rcEmpty));
                if (hrgnUpdate)
                {
                    CheckNotRESULT(NULL, hrgnUpdateExpected = CreateRectRgnIndirect(&rcEmpty));
                }
            }
            else
            {
                // create a randomly sized block to be rendered within our client region
                rcBlock.left   = (GenRand() % (zx - 1));
                rcBlock.top    = (GenRand() % (lzy - 1));
                rcBlock.right  = (GenRand() % (zx - rcBlock.left)) + rcBlock.left + 1;
                rcBlock.bottom = (GenRand() % (lzy - rcBlock.top))  + rcBlock.top  + 1;

                // create our X update region, if any
                if (dx > 0)
                {
                    // scrolling right
                    CheckNotRESULT(FALSE, CopyRect(&rcUpdateX, &rcValidWork));
                    rcUpdateX.right = rcUpdateX.left + min(dx, rcValidWork.right-rcValidWork.left);
                }
                else if (dx < 0)
                {
                    // scrolling left
                    CheckNotRESULT(FALSE, CopyRect(&rcUpdateX, &rcValidWork));
                    rcUpdateX.left = rcUpdateX.right - min(abs(dx), rcValidWork.right-rcValidWork.left);
                }

                // create our Y update region, if any
                if (dy > 0)
                {
                    // scrolling down
                    CheckNotRESULT(FALSE, CopyRect(&rcUpdateY, &rcValidWork));
                    rcUpdateY.bottom = rcUpdateY.top + min(dy, rcValidWork.bottom-rcValidWork.top);
                }
                else if (dy < 0)
                {
                    // scrolling up
                    CheckNotRESULT(FALSE, CopyRect(&rcUpdateY, &rcValidWork));
                    rcUpdateY.top = rcUpdateY.bottom - min(abs(dy), rcValidWork.bottom-rcValidWork.top);
                }

                // calculate the expected update RECT and region
                if (dx!=0 || dy!=0)
                {
                    // we're scrolling, so combine both update regions (it's ok if one is empty)
                    CheckNotRESULT(FALSE, UnionRect(&rcUpdateExpected, &rcUpdateX, &rcUpdateY));

                    // build the expected update region, if needed
                    if (hrgnUpdate)
                    {
                        HRGN hrgnUpdateX = NULL;
                        HRGN hrgnUpdateY = NULL;

                        // convert each update RECT in to a region
                        CheckNotRESULT(NULL, hrgnUpdateX = CreateRectRgnIndirect(&rcUpdateX));
                        CheckNotRESULT(NULL, hrgnUpdateY = CreateRectRgnIndirect(&rcUpdateY));

                        // create a complex expected region
                        CheckNotRESULT(NULL, hrgnUpdateExpected = CreateRectRgnIndirect(&rcEmpty));
                        CheckNotRESULT(NULLREGION, CombineRgn(hrgnUpdateExpected, hrgnUpdateX, hrgnUpdateY, RGN_OR));

                        // cleanup
                        CheckNotRESULT(FALSE, DeleteObject(hrgnUpdateX));
                        CheckNotRESULT(FALSE, DeleteObject(hrgnUpdateY));
                        hrgnUpdateX = NULL;
                        hrgnUpdateY = NULL;
                    }
                }
                else
                {
                    // we're not scrolling at all, so our update will be empty
                    CheckNotRESULT(FALSE, CopyRect(&rcUpdateExpected, &rcEmpty));

                    // create an empty update region
                    if (hrgnUpdate)
                    {
                        CheckNotRESULT(NULL, hrgnUpdateExpected = CreateRectRgnIndirect(&rcEmpty));
                    }
                }
            }

            // initialize our DCs
            CheckNotRESULT(FALSE, PatBlt(hdc,       0, 0, zx, lzy, WHITENESS));
            CheckNotRESULT(FALSE, PatBlt(hdcVerify, 0, 0, zx, lzy, WHITENESS));

            // draw something which will be scrolled during each ScrollDC iteration
            CheckNotRESULT(FALSE, FillRect(hdc,       &rcBlock, hBrushBlock));
            CheckNotRESULT(FALSE, FillRect(hdcVerify, &rcBlock, hBrushBlock));

            //
            // Initialization complete
            //

            info(DETAIL, TEXT("ScrollDC case %d using dx=%d, dy=%d, with input parameters: %s, %s, %s"),
                nCase, dx, dy,
                lprcScroll ? TEXT("lprcScroll") : TEXT("NULL lprcScroll"),
                lprcClip   ? TEXT("lprcClip")   : TEXT("NULL lprcClip"),
                hrgnUpdate ? TEXT("hrgnUpdate") : TEXT("NULL hrgnUpdate") );

            //
            // scroll the block for a few frames
            //
            // loop logic:
            //   we will scroll for nFrmaesScrolled, then discontinue
            //   if we scroll outside of the clipping region, this does not affect the for loop
            //   if we quit before we've scrolled to the clipping region, this does not affect the for loop
            //
            // verification (done on each frame):
            //   verify lprcUpdate RECT matches the expected update RECT
            //   verify lprcUpdate RECT and RECT from hrgnUpdate match
            //   verify hrgnUpdate matches expected update region
            //   visually confirm actual rendering (hdc) matches expected rendering (hdcVerify)
            //
            for (UINT nStep=0; nStep<nFramesScrolled; nStep++)
            {
                RECT rcUpdate = {0};

                //
                // Execute Tests
                //

                // perform scroll
                CheckNotRESULT(FALSE, ScrollDC(hdc, dx, dy, lprcScroll, lprcClip, hrgnUpdate, &rcUpdate));

                //
                // API Verification
                //

                // verify lprcUpdate matches expected update RECT
                if (!EqualRect(&rcUpdateExpected, &rcUpdate))
                {
                    info(FAIL, TEXT("Actual lprcUpdate returned by ScrollDC does not match expected lprcUpdate."));

                    // output region info
                    OutputRectInfo(&rcUpdate,         TEXT("lprcUpdate (actual)   "));
                    OutputRectInfo(&rcUpdateExpected, TEXT("lprcUpdate (expected) "));
                    OutputRectInfo(lprcClip,          TEXT("lprcClip              "));
                    OutputRectInfo(lprcScroll,        TEXT("lprcScroll            "));
                }

                // verify hrgnUpdate matches expected update region
                if (hrgnUpdate)
                {
                    RECT rcRgnUpdate = {0};
                    int nRegionType  = GetRgnBox(hrgnUpdate, &rcRgnUpdate);

                    // verify hrgnUpdate and lprcUpdate match (both are set by ScrollDC)
                    CheckForRESULT(TRUE, EqualRect(&rcRgnUpdate, &rcUpdate));
                    
                    // hrgnUpdate should match the expected update region
                    CheckForRESULT(TRUE, EqualRgn(hrgnUpdateExpected, hrgnUpdate));
                }    

                //
                // Visual Verification
                //

                // since visual verification is expensive to perform and each iteration
                // needs to build off of the last visual verification step, only execute 
                // for a small number of scenarios to decrease test duration
                if (nCase<nVisualScenarios)
                {
                    RECT rcScrollOrigin = {0};
                    RECT rcScrollOffset = {0};
                    RECT rcClipOrigin   = {0};
                    RECT rcDest         = {0};
                    RECT rcValid        = {0};

                    // define the scrollable region
                    if (lprcScroll)
                    {
                        CheckNotRESULT(FALSE, CopyRect(&rcScrollOrigin, lprcScroll));
                    }
                    else
                    {
                        CheckNotRESULT(FALSE, CopyRect(&rcScrollOrigin, &rcClient));
                    }

                    // define the visible region
                    if (lprcClip)
                    {
                        CheckNotRESULT(FALSE, CopyRect(&rcClipOrigin, lprcClip));
                    }
                    else
                    {
                        CheckNotRESULT(FALSE, CopyRect(&rcClipOrigin, &rcClient));
                    }

                    // SrcOffset = Offset (ScrollOrigin, dx, dy)
                    CheckNotRESULT(FALSE, CopyRect(&rcScrollOffset, &rcScrollOrigin));
                    CheckNotRESULT(FALSE, OffsetRect(&rcScrollOffset, dx, dy));

                    // Dest = Offset(ScrollOrigin, dx, dy) & Clip
                    // i.e. the region you can copy to
                    IntersectRect(&rcDest, &rcScrollOffset, &rcClipOrigin);

                    // ScrollOrigin = ScrollOrigin & Clip
                    // i.e. where you can copy from
                    IntersectRect(&rcScrollOrigin, &rcScrollOrigin, &rcClipOrigin);

                    // SrcOffset = Offset (ScrollOrigin, dx, dy)
                    CheckNotRESULT(FALSE, CopyRect(&rcScrollOffset, &rcScrollOrigin));
                    CheckNotRESULT(FALSE, OffsetRect(&rcScrollOffset, dx, dy));

                    // Valid = Dest & Offset (ScrollOrigin, dx, dy)
                    IntersectRect(&rcValid, &rcDest, &rcScrollOffset);

                    // do expected ScrollDC rendering on test verification HDC
                    CheckNotRESULT(FALSE, BitBlt(hdcVerify,
                        rcValid.left, rcValid.top,
                        rcValid.right-rcValid.left, rcValid.bottom-rcValid.top,
                        hdcVerify,
                        rcValid.left - dx, rcValid.top - dy, SRCCOPY));

                    // verify the ScrollDC rendering (hdc) matches the expected rendering (hdcVerify)
                    CompareSurface(hdc, hdcVerify);

                } // end visual verification

            } // end for loop

            // scenario cleanup
            if (hrgnUpdate)
            {
                CheckNotRESULT(FALSE, DeleteObject(hrgnUpdate));
                hrgnUpdate = NULL;
            }

            if (hrgnUpdateExpected)
            {
                CheckNotRESULT(FALSE, DeleteObject(hrgnUpdateExpected));
                hrgnUpdateExpected = NULL;
            }
        }

        // general test cleanup
        CheckNotRESULT(NULL, SelectObject(hdcVerify, hbmpStock));
        CheckNotRESULT(FALSE, DeleteDC(hdcVerify));
        CheckNotRESULT(FALSE, DeleteObject(hbmpMem));
        
        if (hBrushBlock)
        {
            CheckNotRESULT(FALSE, DeleteObject(hBrushBlock));
            hBrushBlock = NULL;            
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
    DWORD dwValues[] = { 0, 1, 100, 200, dwMax - 2, dwMax - 1, dwMax };
    // base registry key for GPE.
    TCHAR szGPERegKey[] = TEXT("Drivers\\Display\\GPE");

    // Load the test display driver to verify it's existance.  If it doesn't load then skip the test
    // and output detailed info on why the test is skipped and give remedies on how to make it not skip.
    if(!g_hdcSecondary)
    {
        info(DETAIL, TEXT("Secondary display driver unavailable, test skipped.  Please verify that ddi_test.dll is available and there is sufficient memory for driver verification."));
        return;
    }

    if (GetDirectCallerProcessId() != OEM_CERTIFY_TRUST)
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
                                NAMEVALENTRY(TOUCHTARGETSIZE),
                                NAMEVALENTRY(FRAMEBUFFER),
                                NAMEVALENTRY(TOUCHCAPS),
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
#ifdef UNDER_CE
    NameValPair nvTouchTargetSize[] = {
                                NAMEVALENTRY(TTS_STYLUS),
                                NAMEVALENTRY(TTS_FINGER)               
                                };
    NameValPair nvTouchCaps[] = {
                            NAMEVALENTRY(TC_NO_TOUCH),
                            NAMEVALENTRY(TC_SINGLE_TOUCH),
                            NAMEVALENTRY(TC_MULTI_TOUCH),
                            NAMEVALENTRY(TC_SYMMETRIC_TOUCH)
                            };
#endif
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
#ifdef UNDER_CE
            case TOUCHTARGETSIZE:
                OuputExtendedCapsData(nValue, nvTouchTargetSize, countof(nvTouchTargetSize));
                break;
            case TOUCHCAPS:
                OuputExtendedCapsData(nValue, nvTouchCaps, countof(nvTouchCaps));
                break;
#else
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

//**********************************************************************************
// Improper usage of DrvEscape scenarios.
void
DrvEscapeInvalidParam(int testFunc)
{
#ifdef UNDER_CE
    info(ECHO, TEXT("%s - DrvEscapeInvalidParam"), funcName[testFunc]);

    TDC     hdc       = myGetDC();
    TDC     hdcCompat = NULL;
    TBITMAP hbmpStock = NULL;
    TBITMAP hbmpMem   = NULL;
    TBITMAP hbmpBmp   = NULL;
    TBITMAP hbmpDib   = NULL;
    LPVOID  lpBits    = NULL;
    CHAR    cbInput   = 0;
    CHAR    cbOutput  = 0;

    switch (testFunc)
    {
    case EExtEscape:
        // invalid size
        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(0, ExtEscape(hdc, QUERYESCSUPPORT, -1, (LPCSTR)&cbInput, sizeof(cbOutput), (LPSTR)&cbOutput));
        CheckForLastError(ERROR_INVALID_PARAMETER);

        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(0, ExtEscape(hdc, QUERYESCSUPPORT, sizeof(cbOutput), (LPCSTR)&cbInput, -1, (LPSTR)&cbOutput));
        CheckForLastError(ERROR_INVALID_PARAMETER);
        break;

    case EBitmapEscape:
        // create various bitmap types
        CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hbmpMem = CreateCompatibleBitmap(hdcCompat, zx, zy));
        CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmpMem));
        // create bitmaps which don't point to the driver (since DrvEscape doesn't deal with these)
        CheckNotRESULT(NULL, hbmpBmp = myCreateBitmap(zx, zy, 1, gBpp, NULL));
        CheckNotRESULT(NULL, hbmpDib = myCreateDIBSection(hdc, (VOID **)&lpBits, gBpp, zx, zy, DIB_RGB_COLORS, NULL, FALSE));

        // invalid size
        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(0, BitmapEscape(hbmpMem, QUERYESCSUPPORT, -1, (LPCSTR)&cbInput, sizeof(cbOutput), (LPSTR)&cbOutput));
        CheckForLastError(ERROR_INVALID_PARAMETER);

        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(0, BitmapEscape(hbmpMem, QUERYESCSUPPORT, sizeof(cbOutput), (LPCSTR)&cbInput, -1, (LPSTR)&cbOutput));
        CheckForLastError(ERROR_INVALID_PARAMETER);

        // invalid bitmap source
        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(-1, BitmapEscape(hbmpBmp, QUERYESCSUPPORT, sizeof(cbInput), (LPCSTR)&cbInput, sizeof(cbOutput), (LPSTR)&cbOutput));
        CheckForLastError(ERROR_INVALID_PARAMETER);

        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(-1, BitmapEscape(hbmpDib, QUERYESCSUPPORT, sizeof(cbInput), (LPCSTR)&cbInput, sizeof(cbOutput), (LPSTR)&cbOutput));
        CheckForLastError(ERROR_INVALID_PARAMETER);

        // cleanup
        CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
        CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
        CheckNotRESULT(FALSE, DeleteObject(hbmpMem));
        CheckNotRESULT(FALSE, DeleteObject(hbmpBmp));
        CheckNotRESULT(FALSE, DeleteObject(hbmpDib));
        break;

    default:
        info(FAIL, TEXT("DrvEscapeInvalidParam called on an invalid case"));
        break;
    }

    myReleaseDC(hdc);
#endif
}

//**********************************************************************************
// Verify external usage of escapes are blocked.
void
DrvEscapeInvalidAccess(int testFunc)
{
#ifdef UNDER_CE
    info(ECHO, TEXT("%s - DrvEscapeInvalidAccess"), funcName[testFunc]);

    // verify internal escape codes are not denied access using DrvEscape since these
    // should only be invoked using supported APIs (ChangeDisplaySettingsEx, for instance)
    const int nExtEscapes[] = {
        DRVESC_GETSCREENROTATION,
        DRVESC_SETSCREENROTATION,
        DRVESC_GETVIDEOPROTECTION,
        DRVESC_SETVIDEOPROTECTION,
        DRVESC_RESTOREVIDEOMEM,
        DRVESC_SAVEVIDEOMEM,
        DRVESC_SETGAMMAVALUE,
        DRVESC_GETGAMMAVALUE };

    const int nBitmapEscapes[] = {
        SETPOWERMANAGEMENT,
        DRVESC_GETSCREENROTATION,
        DRVESC_SETSCREENROTATION,
        DRVESC_GETVIDEOPROTECTION,
        DRVESC_SETVIDEOPROTECTION,
        DRVESC_RESTOREVIDEOMEM,
        DRVESC_SAVEVIDEOMEM,
        DRVESC_SETGAMMAVALUE,
        DRVESC_GETGAMMAVALUE };

    TDC     hdc       = myGetDC();
    TDC     hdcCompat = NULL;
    TBITMAP hbmp      = NULL;
    TBITMAP hbmpStock = NULL;

    switch (testFunc)
    {
    case EExtEscape:
        for(int i = 0; i < countof(nExtEscapes); i++)
        {
            info(DETAIL, TEXT("Testing escape code %d."), nExtEscapes[i]);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(-1, ExtEscape(hdc, nExtEscapes[i], 0, NULL, 0, NULL));
            CheckForLastError(ERROR_INVALID_ACCESS);
        }
        break;

    case EBitmapEscape:
        // create our bitmap
        CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hbmp = CreateCompatibleBitmap(hdcCompat, zx, zy));
        CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));

        for(int i = 0; i < countof(nBitmapEscapes); i++)
        {
            info(DETAIL, TEXT("Testing escape code %d."), nBitmapEscapes[i]);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(-1, BitmapEscape(hbmp, nBitmapEscapes[i], 0, NULL, 0, NULL));
            CheckForLastError(ERROR_INVALID_ACCESS);
        }

        // cleanup
        CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
        CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
        CheckNotRESULT(FALSE, DeleteObject(hbmp));
        break;

    default:
        info(FAIL, TEXT("DrvEscapeInvalidAccess called on an invalid case"));
        break;
    }

    myReleaseDC(hdc);
#endif
}

//**********************************************************************************
// Microsoft reserves the range 0 to 0x00010000 for its escape codes. Third-party vendors
// are free to choose escape codes for their own use above this range. This test verifies
// the display driver has not implemented escape codes within the reserved range.
void
DrvEscapeReserved(int testFunc)
{
#ifdef UNDER_CE
    info(ECHO, TEXT("%s - DrvEscapeReserved"), funcName[testFunc]);

    const DWORD dwMinReservedRange = 0x00000000;
    const DWORD dwMaxReservedRange = 0x00010000;
    const DWORD dwValidReservedEscapes[] ={
        DBGDRIVERSTAT,                     // 6146
        SETPOWERMANAGEMENT,                // 6147
        GETPOWERMANAGEMENT,                // 6148
        CONTRASTCOMMAND,                   // 6149
        DRVESC_UPDATE_ACTIVATE,            // 6152
        DRVESC_SETGAMMAVALUE,              // 6201
        DRVESC_GETGAMMAVALUE,              // 6202
        DRVESC_SETSCREENROTATION,          // 6301
        DRVESC_GETSCREENROTATION,          // 6302
        DRVESC_BEGINSCREENROTATION,        // 6303
        DRVESC_ENDSCREENROTATION,          // 6304
        DRVESC_SETVIDEOPROTECTION,         // 6401
        DRVESC_GETVIDEOPROTECTION,         // 6402
        DRVESC_SAVEVIDEOMEM,               // 6501
        DRVESC_RESTOREVIDEOMEM,            // 6502
        DRVESC_QUERYVIDEOMEMUSED,          // 6503
        };

    TDC     hdc       = myGetDC();
    TDC     hdcCompat = NULL;
    TBITMAP hbmp      = NULL;
    TBITMAP hbmpStock = NULL;

    // create our bitmap
    CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
    CheckNotRESULT(NULL, hbmp = CreateCompatibleBitmap(hdcCompat, zx, zy));
    CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));

    // verify there are no driver implemented escapes accessable within the reserved range
    for (DWORD dwEscape=dwMinReservedRange; dwEscape<=dwMaxReservedRange; dwEscape++)
    {
        // input and output params must be DWORD aligned
        DWORD cbInput  = dwEscape;
        DWORD cbOutput = 0;
        int nResult   = 0;
        switch (testFunc)
        {
        case EExtEscape:
            nResult = ExtEscape(hdc, QUERYESCSUPPORT, sizeof(cbInput), (LPCSTR)&cbInput, sizeof(cbOutput), (LPSTR)&cbOutput);
            break;
        case EBitmapEscape:
            nResult = BitmapEscape(hbmp, QUERYESCSUPPORT, sizeof(cbInput), (LPCSTR)&cbInput, sizeof(cbOutput), (LPSTR)&cbOutput);
            break;
        default:
            info(FAIL, TEXT("DrvEscapeReserved called on an invalid case"));
            break;
        }

        //if the driver reports this escape was implemented, verify this is expected to be withing the reserved range 
        //if (nResult)
        //{
        //     this escape should not have been implemented by the driver within the reserved range
        //    info(FAIL, TEXT("Invalid escape %d implemented within reserved range: 0x%08x to 0x%08x."),
        //        dwEscape, dwMinReservedRange, dwMaxReservedRange);
        //}
        if (EXTESC_SUPPORTED == nResult)
        {
            BOOL fValidEscape = FALSE;

            // if the driver supports this escape, verify this is allowed within the reserved range 
            for (UINT i=0; i<_countof(dwValidReservedEscapes); ++i)
            {
                if (dwValidReservedEscapes[i] == dwEscape)
                {
                    fValidEscape = TRUE;
                    break;
                }
            }

            if (!fValidEscape)
            {
                // invalid escape implemented by the driver within the reserved range
                info(FAIL, TEXT("Invalid escape %d implemented within reserved range: 0x%08x to 0x%08x."),
                    dwEscape, dwMinReservedRange, dwMaxReservedRange);
            }
            else
            {
                // this is a known and valid escape which can be supported by the driver
                info(DETAIL, TEXT("Known escape %d implemented within reserved range."), dwEscape);
            }
        }

    }

    // cleanup
    CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
    CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
    CheckNotRESULT(FALSE, DeleteObject(hbmp));
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
#ifdef UNDER_CE
    DeviceCapsGPERegTest(TEXT("TouchTargetSize"), TOUCHTARGETSIZE );
#endif

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
    RandomScrollDCTest();

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
    DrvEscapeInvalidParam(EExtEscape);
    DrvEscapeInvalidAccess(EExtEscape);
    DrvEscapeReserved(EExtEscape);

    // Depth
    // since we don't know what escapes might be implemented by the driver, no depth
    // test coverage here

    return getCode();
}

//**********************************************************************************
TESTPROCAPI BitmapEscape_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2DC(EBitmapEscape);
    DrvEscapeInvalidParam(EBitmapEscape);
    DrvEscapeInvalidAccess(EBitmapEscape);
    DrvEscapeReserved(EBitmapEscape);

    // Depth
    // since we don't know what escapes might be implemented by the driver, no depth
    // test coverage here

    return getCode();
}
