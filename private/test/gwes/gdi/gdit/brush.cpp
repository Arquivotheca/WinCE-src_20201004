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

   brush.cpp

Abstract:

   Gdi Tests: Brush & Pen APIs

***************************************************************************/

#include "global.h"

/***********************************************************************************
***
***   Uses random mode to check return values of Get/Set BrushOrgEx pairs
***
************************************************************************************/

//***********************************************************************************
void
checkGetSetBrushOrgEx(int testFunc, BOOL evil)
{
#ifndef UNDER_CE
    info(ECHO, TEXT("%s - %d Set/Get BrushOrgEx Pairs - %s"), funcName[testFunc], testCycles,
         (evil) ? TEXT("evil") : TEXT("good"));

    TDC     hdc = myGetDC();
    int     counter = 0;
    BOOL    getResult0,
            getResult1,
            setResult0,
            setResult1;
    POINT   src,
            cur0,
            cur1,
            prev0,
            prev1;

    while (counter++ < testCycles)
    {
        src.x = (evil) ? badRandomNum() : goodRandomNum(8);
        src.y = (evil) ? badRandomNum() : goodRandomNum(8);

        CheckNotRESULT(FALSE, getResult0 = GetBrushOrgEx(hdc, &prev0));
        CheckNotRESULT(FALSE, setResult0 = SetBrushOrgEx(hdc, src.x, src.y, &prev1));
        CheckNotRESULT(FALSE, getResult1 = GetBrushOrgEx(hdc, &cur0));
        CheckNotRESULT(FALSE, setResult1 = SetBrushOrgEx(hdc, src.x, src.y, &cur1));

        if (gSanityChecks)
        {
            info(DETAIL, TEXT("src:%d %d"), src.x, src.y);
            info(DETAIL, TEXT("cur0:%d %d"), cur0.x, cur0.y);
            info(DETAIL, TEXT("cur1:%d %d"), cur1.x, cur1.y);
            info(DETAIL, TEXT("prev0:%d %d"), prev0.x, prev0.y);
            info(DETAIL, TEXT("prev1:%d %d"), prev1.x, prev1.y);
        }

        if (!getResult0)
            info(FAIL, TEXT("initial GetBrushOrgEx (0x%x) failed"), prev0);

        if (!setResult0)
            info(FAIL, TEXT("initial SetBrushOrgEx (%d %d 0x%x) failed"), src.x, src.y, prev1);

        if (!getResult0)
            info(FAIL, TEXT("second GetBrushOrgEx (0x%x) failed"), cur0);

        if (!setResult0)
            info(FAIL, TEXT("second SetBrushOrgEx (%d %d 0x%x) failed"), src.x, src.y, cur1);

        if (prev0.x != prev1.x || prev0.y != prev1.y)
            info(FAIL, TEXT("initial get(%d %d) != initial set(%d %d)"), prev0.x, prev0.y, prev1.x, prev1.y);

        if (src.x != cur0.x || src.y != cur0.y)
            info(FAIL, TEXT("initial set(%d %d) != second get(%d %d)"), src.x, src.y, cur0.x, cur0.y);

        if (cur1.x != cur0.x || cur1.y != cur0.y)
            info(FAIL, TEXT("second set(%d %d) != second get(%d %d)"), cur1.x, cur1.y, cur0.x, cur0.y);
    }
    myReleaseDC(hdc);
#endif // UNDER_CE
}

/***********************************************************************************
***
***   Check Get/Set BrushOrgEx Passing Null Test
***
************************************************************************************/

//***********************************************************************************
void
checkGetSetBrushOrgExParams(int testFunc, BOOL useDC)
{
#ifndef UNDER_CE
    info(ECHO, TEXT("%s - %d Set/Get BrushOrgEx Pairs - passing NULL"), funcName[testFunc], testCycles);

    int     counter = 0;
    POINT   src,
            cur0,
            cur1,
            prev0,
            prev1;
    TDC     hdc = useDC ? myGetDC() : NULL;
    BOOL exp = useDC ? TRUE : FALSE;

    while (counter++ < testCycles)
    {
        src.x = goodRandomNum(8);
        src.y = goodRandomNum(8);

        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(exp, GetBrushOrgEx(hdc, &prev0));
        // verify that the last error is set correctly.
        if(!exp)
            CheckForLastError(ERROR_INVALID_PARAMETER);

        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(exp, SetBrushOrgEx(hdc, src.x, src.y, &prev1));
        // verify that the last error is set correctly.
        if(!exp)
            CheckForLastError(ERROR_INVALID_HANDLE);

        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(exp, GetBrushOrgEx(hdc, &cur0));
        // verify that the last error is set correctly.
        if(!exp)
            CheckForLastError(ERROR_INVALID_PARAMETER);

        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(exp, SetBrushOrgEx(hdc, src.x, src.y, &cur1));
        // verify that the last error is set correctly.
        if(!exp)
            CheckForLastError(ERROR_INVALID_HANDLE);
    }

    if (useDC)
        myReleaseDC(hdc);
#endif // UNDER_CE
}

/***********************************************************************************
***
***   Simple SetBrushOrgExTest
***
************************************************************************************/

// sets random points as the brush origin, and verifies that the returned brush origin is correct.
void
SimpleSetBrushOrgExTest()
{
    info(ECHO, TEXT("SimpelSetBrushOrgExTest"));

    TDC hdc = myGetDC();
    int x = 0, y = 0;
    int xprev, yprev;
    POINT pt, ptOrig;

    CheckNotRESULT(FALSE, SetBrushOrgEx(hdc, x, y, &ptOrig));

    for(int i=0; i < 50; i++)
    {
        xprev = x;
        yprev = y;

        x = GenRand() % zx;
        y = GenRand() % zy;

        CheckNotRESULT(FALSE, SetBrushOrgEx(hdc, x, y, &pt));

        if(pt.x != xprev || pt.y != yprev)
            info(FAIL, TEXT("Expected the previous coordinate to be (%d, %d), got (%d, %d)"), xprev, yprev, pt.x, pt.y);

    }
    CheckNotRESULT(FALSE, SetBrushOrgEx(hdc, ptOrig.x, ptOrig.y, NULL));

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Simple CreateSolidBrush Test
***
************************************************************************************/

//***********************************************************************************
void
SimpleCreateSolidBrushTest(int testFunc, BOOL bAlpha)
{

    info(ECHO, TEXT("%s - %d Repeating calls"), funcName[testFunc], testCycles);
    TDC hdc = myGetDC();
    HBRUSH  hBrush, hBrushStock;
    COLORREF color;
    int     counter = 0;

    SetLastError(ERROR_SUCCESS);
    CheckForRESULT(NULL, CreateSolidBrush(CLR_INVALID));
    CheckForLastError(ERROR_INVALID_PARAMETER);

    while (counter++ < testCycles)
    {
        if(bAlpha)
            color = randColorRefA();
        else
            color = randColorRef();

        CheckNotRESULT(NULL, hBrush = CreateSolidBrush(color));

        if (hBrush)
        {
            CheckNotRESULT(NULL, hBrushStock = (HBRUSH) SelectObject(hdc, hBrush));
            CheckNotRESULT(NULL, SelectObject(hdc, hBrushStock));
            CheckNotRESULT(FALSE, DeleteObject(hBrush));
        }
    }
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Simple CreatePatternBrush Test
***
************************************************************************************/

//***********************************************************************************
void
SimpleCreatePatternBrushTest(int testFunc)
{
    info(ECHO, TEXT("%s - %d Simple BVT calls"), funcName[testFunc], testCycles);

    TDC     hdc = myGetDC();
    HBRUSH  hBrush, hBrushStock;
    TBITMAP hBmp;
    int     counter = 0;

    CheckNotRESULT(NULL, hBmp = CreateCompatibleBitmap(hdc, 8, 8));

    if (hBmp)
    {
        while (counter++ < testCycles)
        {

            CheckNotRESULT(NULL, hBrush = CreatePatternBrush(hBmp));

            if (hBrush)
            {
                CheckNotRESULT(NULL, hBrushStock = (HBRUSH) SelectObject(hdc, hBrush));
                CheckNotRESULT(NULL, SelectObject(hdc, hBrushStock));
                CheckNotRESULT(FALSE, DeleteObject(hBrush));
            }
        }
        CheckNotRESULT(FALSE, DeleteObject(hBmp));
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Simple CreatePatternBrush Test
***
************************************************************************************/

const int bitmapBrushDepths[] = { 1,
#ifdef UNDER_CE
                                                       2,
#endif
                                                       4, 8, 16, 24, 32};

//***********************************************************************************
HBRUSH setupBrush(TDC hdc, int bmType, int depth, int brushSize, BOOL random, BOOL quiet)
{

    TDC     compDC;
    TBITMAP hBmp = 0,
            stockBmp;
    HBRUSH  hBrush = NULL;
    COLORREF aColor;
    TCHAR   szBuf[30] =  {NULL};
    BYTE   *lpBits = NULL;
    DWORD   dwError = 0;
    BOOL    fSetLastError = FALSE;
    BITMAP  bmp;

    CheckNotRESULT(NULL, compDC = CreateCompatibleDC(hdc));

    if(compDC)
    {
        if (!quiet)
        {
            if (bmType != compatibleBitmap)
                info(DETAIL, TEXT("setupBrush: creating type: <%s> depth:%d"), rgBitmapTypes[bmType].szName, depth);
            else
                info(DETAIL, TEXT("setupBrush: creating type: <%s>"), rgBitmapTypes[bmType].szName);
        }

        switch (bmType)
        {
            case compatibleBitmap:
                CheckNotRESULT(NULL, hBmp = CreateCompatibleBitmap(hdc, brushSize, brushSize));
                break;
            case lb:
                _sntprintf_s(szBuf, countof(szBuf)-1, TEXT("BMP_GDI%d"), depth);
                CheckNotRESULT(NULL, hBmp = myLoadBitmap(globalInst, szBuf));
                break;
            case DibRGB:
                CheckNotRESULT(NULL, hBmp = myCreateDIBSection(hdc, (VOID **) & lpBits, depth, brushSize, brushSize, DIB_RGB_COLORS, NULL, FALSE));
                break;
            case DibPal:
                CheckNotRESULT(NULL, hBmp = myCreateDIBSection(hdc, (VOID **) & lpBits, depth, brushSize, brushSize, DIB_PAL_COLORS, NULL, FALSE));
                break;
            default:
                info(FAIL, TEXT("Unrecognized bitmap type %d"), bmType);
                break;
        }

        if(hBmp)
        {
            CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(compDC, hBmp));
            if (bmType != lb)
            {
                CheckNotRESULT(FALSE, PatBlt(compDC, 0, 0, brushSize, brushSize, WHITENESS));
                if (random)
                {
                    for (int i = 0; i < brushSize; i++)
                        for (int j = 0; j < brushSize; j++)
                        {
                            aColor = goodRandomNum(256);
                            CheckNotRESULT(-1, SetPixel(compDC, i, j, RGB(aColor, aColor, aColor)));
                        }
                }
                else
                {
                    // if the brush size is 1 (1x1), then set 1 pixel to black
                    if(brushSize == 1)
                    {
                        CheckNotRESULT(-1, SetPixel(compDC, 0, 0, RGB(0, 0, 0)));
                    }
                    // the brush size is 2 (a 2x2 brush, totalling 4 pixels), set it to a checkerboard pattern.
                    else
                    {
                        // do an initial fill of all one color so all surfaces match in driver verification.
                        CheckNotRESULT(FALSE, PatBlt(compDC, 0, 0, brushSize, brushSize, WHITENESS));
                        CheckNotRESULT(FALSE, PatBlt(compDC, brushSize/2, 0, brushSize/2, brushSize/2, BLACKNESS));
                        CheckNotRESULT(FALSE,PatBlt(compDC, 0, brushSize/2, brushSize/2, brushSize/2, BLACKNESS));
                    }
                }
            }

            memset(&bmp, 0, sizeof (BITMAP));
            CheckNotRESULT(0, GetObject(hBmp, sizeof (BITMAP), &bmp));
            if (!quiet)
            {
                info(DETAIL, TEXT("Brush bmp: bmWidth    =%ld"), bmp.bmWidth);
                info(DETAIL, TEXT("Brush bmp: bmHeight   =%ld"), bmp.bmHeight);
                info(DETAIL, TEXT("Brush bmp: bmBitsPixel=%ld"), bmp.bmBitsPixel);
            }

            CheckNotRESULT(NULL, hBrush = CreatePatternBrush(hBmp));
            CheckNotRESULT(NULL, SelectObject(compDC, stockBmp));
            CheckNotRESULT(FALSE, DeleteObject(hBmp));
        }

        CheckNotRESULT(FALSE, DeleteDC(compDC));
    }

    if (fSetLastError && dwError != 0)
        SetLastError(dwError);  // Tell Caller what is caused the failure

    return hBrush;
}

//***********************************************************************************
void
CreatePatternBrushDrawingTest(int testFunc)
{
    info(ECHO, TEXT("%s - %d Drawing Test"), funcName[testFunc], testCycles);

    TDC     hdc = myGetDC();
    HBRUSH  hBrush,
            stockBrush;

    POINT   Board = { zx / 2, zy };
    POINT   aPoint;

    int     Cube = zx/8;
    int     i,
            n,
            x,
            y;

    // Cube must be divisable by 8 becuase the pattern is 8 wide.
    // it's ok if it only fills half of the screen.
    Cube = (Cube + 0x7) & ~0x7;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        for (int t = 0; t < numBitMapTypes; t++)
        {
            for (int d = (t != compatibleBitmap) ? 0 : countof(bitmapBrushDepths) - 1; d < countof(bitmapBrushDepths); d++)
            {
                // set up brush.  no bitmaps available for 2, 16, or 32bpp.
                if((t == DibPal && bitmapBrushDepths[d] > 8) ||
                    (t==lb && (bitmapBrushDepths[d] == 2 || bitmapBrushDepths[d] == 16 || bitmapBrushDepths[d] == 32)))
                    continue;

                CheckNotRESULT(NULL, hBrush = setupBrush(hdc, t, bitmapBrushDepths[d], 8, 1, TRUE));
                CheckNotRESULT(FALSE, SetBrushOrgEx(hdc, 0, 0, &aPoint));
                CheckNotRESULT(NULL, stockBrush = (HBRUSH) SelectObject(hdc, hBrush));

                if (hBrush)
                {
                    // set up screen
                    CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
                    CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx / 2, zy, PATCOPY));
                    CheckNotRESULT(FALSE, BitBlt(hdc, zx / 2, 0, zx / 2, zy, hdc, 0, 0, SRCCOPY));

                    // draw on screen
                    // n = times to loop
                    for (n = 0; n < 20; n++)
                        for (i = 7; i >= 0; i--)
                        {
                            // SetBrushOrg must be called prior to selecting in the brush.
                            //UnrealizeObject(hBrush); REVIEW
                            CheckNotRESULT(NULL, SelectObject(hdc, GetStockObject(NULL_BRUSH)));
                            CheckNotRESULT(FALSE, SetBrushOrgEx(hdc, i, i, &aPoint));

                            CheckNotRESULT(NULL, SelectObject(hdc, hBrush));
                            for (x = 0; x + Cube <= Board.x; x += Cube)
                                for (y = 0; y < Board.y; y += Cube)
                                {
                                    if ((x/Cube + y/Cube) % 2 == 0)
                                    {
                                        CheckNotRESULT(FALSE, PatBlt(hdc, x, y, Cube, Cube, PATCOPY));
                                    }
                                }
                        }
                    CheckScreenHalves(hdc);
                }
                // clean up
                CheckNotRESULT(NULL, SelectObject(hdc, stockBrush));
                CheckNotRESULT(FALSE, DeleteObject(hBrush));
            }
        }
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Simple Size CreatePatternBrush Test
***
************************************************************************************/

//***********************************************************************************
void
CreatePatternBrushSizeTest(int testFunc)
{
    info(ECHO, TEXT("%s - Size Test"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HBRUSH  hBrush,
            stockBrush;
    POINT   aPoint;
    int     t,
            addFactor = 1;
    DWORD pixels,
               expected = 0;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(FALSE, SetBrushOrgEx(hdc, 0, 0, &aPoint));

        for (t = 0; t < zy && t < zx; t += addFactor)
        {
            if (t > 15)
                addFactor = 100;

            // set up brush
            CheckNotRESULT(NULL, hBrush = setupBrush(hdc, compatibleBitmap, 8, t, 0, TRUE));

            if (hBrush)
            {
                CheckNotRESULT(NULL, stockBrush = (HBRUSH) SelectObject(hdc, hBrush));

                // set up screen
                CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

                // draw on screen
                CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, t, t, PATCOPY));

                // setupBrush will make a checkerboard pattern, so calculate the number of black
                // pixels that would be in that checkerboard.
                if(0 == t)
                    expected = 0;
                else if(1 == t)
                    expected = 1;
                else
                    expected = (((int)(t/2)) * ((int)(t/2))) * 2;

                CheckForRESULT(expected, pixels = CheckAllWhite(hdc, FALSE, expected));

                if (expected != pixels)
                    info(FAIL, TEXT("For size:%d expected:%d found:%d"), t, expected, pixels);

                // clean up
                CheckNotRESULT(NULL, SelectObject(hdc, stockBrush));
                CheckNotRESULT(FALSE, DeleteObject(hBrush));
            }
        }
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   SimpleGetSysColorBrush
***
************************************************************************************/
#ifndef UNDER_CE
#define C_SYS_COLOR_TYPES 31
#endif
//***********************************************************************************
void
SimpleGetSysColorBrush(int testFunc)
{

    info(ECHO, TEXT("%s - Simple GetSysColorBrush"), funcName[testFunc]);

    HBRUSH  hBrush;

    for (int i = -256; i < 256; i++)
    {
        hBrush = GetSysColorBrush(i);


        if (hBrush && (i >= C_SYS_COLOR_TYPES || i < 0))
        {
            info(FAIL, TEXT("GetSysColorbrush(%d) should have failed"), i);
            CheckNotRESULT(FALSE, DeleteObject(hBrush));
        }
        else if (!hBrush && (i < C_SYS_COLOR_TYPES && i >= 0))
            info(FAIL, TEXT("GetSysColorbrush(%d) failed"), i);
    }
}


/***********************************************************************************
***
***   passBrushNULL
***
************************************************************************************/

//***********************************************************************************
void
passBrushNULL(int testFunc)
{
    info(ECHO, TEXT("%s - passing CreatePatternBrush NULL"), funcName[testFunc]);
    HBRUSH hBrush;
    int x = GenRand() % 255, y = GenRand() % 255;
    POINT pt;

    switch(testFunc)
        {
        case ECreatePatternBrush:
             SetLastError(ERROR_SUCCESS);
            CheckForRESULT(NULL, hBrush = CreatePatternBrush((TBITMAP) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(NULL, hBrush = CreatePatternBrush(g_hbmpBAD));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(NULL, hBrush = CreatePatternBrush(g_hbmpBAD2));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case ESetBrushOrgEx:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, SetBrushOrgEx((TDC) NULL, x, y, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, SetBrushOrgEx((TDC) NULL, x, y, &pt));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, SetBrushOrgEx(g_hdcBAD, x, y, &pt));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, SetBrushOrgEx(g_hdcBAD2, x, y, &pt));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        }
}

/***********************************************************************************
***
***   CreateDIBPatternBrushPt NULL
***
************************************************************************************/

//***********************************************************************************
void
CreateDIBPatternBrushPtNULL(int testFunc)
{

    info(ECHO, TEXT("%s - passing CreatePatternBrushPt NULL"), funcName[testFunc]);

    SetLastError(ERROR_SUCCESS);
    CheckForRESULT(NULL, CreateDIBPatternBrushPt(NULL, DIB_PAL_COLORS));
    CheckForLastError(ERROR_INVALID_PARAMETER);

    SetLastError(ERROR_SUCCESS);
    CheckForRESULT(NULL, CreateDIBPatternBrushPt(NULL, DIB_RGB_COLORS));
    CheckForLastError(ERROR_INVALID_PARAMETER);

    SetLastError(ERROR_SUCCESS);
    CheckForRESULT(NULL, CreateDIBPatternBrushPt(NULL, 777));
    CheckForLastError(ERROR_INVALID_PARAMETER);
}

void
CreateDIBPatternBrushPtTest()
{
    info(ECHO, TEXT("CreateDIBPatternBrushPtTest"));
    NOPRINTERDCV(NULL);

    TDC hdc = myGetDC();
    TDC hdcCompat;
    TBITMAP hbmpDIB, hbmpStock;
    HBRUSH hbr, hbrStock;
    MYBITMAPINFO bmi;
    BYTE *lpPackedDIB;
    VOID *ppvBits;
    int nWidth = 8;
    int nHeight = 8;
    int nBpp = myGetBitDepth();
    int nBMISize;
    int nppvBitsSize;
    int i, j, l;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // test bottom up and top down DIB's
        for(l = 0; l < 2 && nBpp > 1; l++)
        {
            if(l)
                info(DETAIL, TEXT("Testing with a top down DIB"));
            else info(DETAIL, TEXT("Testing with a bottom up DIB"));

            // set the BitmapInfo size based on the bit depth
            if(nBpp <=8)
                nBMISize = (int) (sizeof(BITMAPINFO) - sizeof(RGBQUAD) + (pow((double)2, (double)nBpp) * sizeof(RGBQUAD)));
            else
                nBMISize = sizeof(BITMAPINFO) - sizeof(RGBQUAD);

            // make sure we're dword aligned.
            if(nBpp <= 8)
                nppvBitsSize = (((nWidth / (8/nBpp)) + 3) & ~3) * nHeight;
            else nppvBitsSize = (((nWidth * (nBpp/8)) + 3) & ~3) * nHeight;

            lpPackedDIB = new(BYTE[nBMISize + nppvBitsSize]);

            CheckNotRESULT(NULL, hbmpDIB = myCreateDIBSection(hdc, (VOID **) &ppvBits, nBpp, nWidth, (l?-1:1) * nHeight, DIB_RGB_COLORS, &bmi, FALSE));
            CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
            CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmpDIB));

            // fill the surface with random colors.
            for (i = 0; i < nWidth; i++)
                for (j = 0; j < nHeight; j++)
                    CheckNotRESULT(CLR_INVALID, SetPixel(hdcCompat, i, j, randColorRef()));

            memcpy(lpPackedDIB, (VOID *) &bmi, nBMISize);
            memcpy(lpPackedDIB + nBMISize, ppvBits, nppvBitsSize);

            CheckNotRESULT(NULL, hbr = CreateDIBPatternBrushPt(lpPackedDIB, DIB_RGB_COLORS));
            CheckNotRESULT(NULL, hbrStock = (HBRUSH) SelectObject(hdc, hbr));

            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, nWidth, nHeight, PATCOPY));

            CheckNotRESULT(NULL, SelectObject(hdc, hbrStock));
            CheckNotRESULT(FALSE, DeleteObject(hbr));

            for (j = 0; j < nHeight; j++)
                for (i = 0; i < nWidth; i++)
                {
                    COLORREF crResult = GetPixel(hdc, i, j);
                    COLORREF crExpected = SetPixel(hdc, nWidth + 1, nHeight + 1, GetPixel(hdcCompat, i, j));

                    if(crExpected != crResult)
                        info(FAIL, TEXT("(%d, %d) - Expected color 0x%08x, returned color 0x%08x"), i, j, crExpected, crResult);
                }

            CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
            CheckNotRESULT(FALSE, DeleteObject(hbmpDIB));
            CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
            delete[] lpPackedDIB;
        }
    }

    myReleaseDC(hdc);
}

void
CreateThickPenWidth20(int testFunc)
{
    info(ECHO, TEXT("%s - EXPECT: thick rect with nice corners: pen width=20"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    LOGPEN  lPen;
    HPEN    hPen,
            hPenTemp;

    lPen.lopnStyle = PS_SOLID;
    lPen.lopnWidth.x = 20;
    lPen.lopnColor = 0x0;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        switch(testFunc)
        {
            case ECreatePenIndirect:
                CheckNotRESULT(NULL, hPen = CreatePenIndirect(&lPen));
                break;
            case ECreatePen:
                CheckNotRESULT(NULL, hPen = CreatePen(lPen.lopnStyle, lPen.lopnWidth.x, lPen.lopnColor));
                break;
        }

        CheckNotRESULT(NULL, hPenTemp = (HPEN) SelectObject(hdc, hPen));
        CheckNotRESULT(FALSE, RoundRect(hdc, 50, 50, 150, 150, 15, 15));

        CheckNotRESULT(NULL, hPenTemp = (HPEN) SelectObject(hdc, hPenTemp));

        if(hPenTemp != hPen)
            info(FAIL, TEXT("Incorrect pen returned"));

        CheckNotRESULT(FALSE, DeleteObject(hPen));
    }

    myReleaseDC(hdc);
}

void
CreatePenAlphaTest(int testFunc)
{
    info(ECHO, TEXT("%s - CreatePenAlphaTest"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    LOGPEN  lPen;
    HPEN    hPen,
            hPenTemp;
    int nStyle[] = {PS_SOLID, PS_DASH, PS_NULL};

    for(int i = 0; i < countof(nStyle); i++)
    {
        lPen.lopnStyle = nStyle[i];
        lPen.lopnWidth.x = GenRand() % 20;
        lPen.lopnColor = randColorRefA();

        switch(testFunc)
        {
            case ECreatePenIndirect:
                CheckNotRESULT(NULL, hPen = CreatePenIndirect(&lPen));
                break;
            case ECreatePen:
                CheckNotRESULT(NULL, hPen = CreatePen(lPen.lopnStyle, lPen.lopnWidth.x, lPen.lopnColor));
                break;
        }

        CheckNotRESULT(NULL, hPenTemp = (HPEN) SelectObject(hdc, hPen));
        CheckNotRESULT(NULL, hPenTemp = (HPEN) SelectObject(hdc, hPenTemp));
        CheckNotRESULT(FALSE, DeleteObject(hPen));

    }
    myReleaseDC(hdc);
}

void
CreateThickPenWidth15(int testFunc)
{
    info(ECHO, TEXT("%s - EXPECT: thick rect with nice corners: pen width=15"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    POINT   points[7] = { {40, 10}, {70, 30}, {70, 60}, {40, 80}, {10, 60}, {10, 30}, {40, 10} };
    LOGPEN  lPen;
    HPEN    hPen,
            hPenTemp;

    lPen.lopnStyle = PS_SOLID;
    lPen.lopnWidth.x = 15;
    lPen.lopnColor = 0x0;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        switch(testFunc)
        {
            case ECreatePenIndirect:
                CheckNotRESULT(NULL, hPen = CreatePenIndirect(&lPen));
                break;
            case ECreatePen:
                CheckNotRESULT(NULL, hPen = CreatePen(lPen.lopnStyle, lPen.lopnWidth.x, lPen.lopnColor));
                break;
        }

        CheckNotRESULT(NULL, hPenTemp = (HPEN) SelectObject(hdc, hPen));
        CheckNotRESULT(FALSE, Polyline(hdc, points, 7));
        CheckNotRESULT(NULL, hPenTemp = (HPEN) SelectObject(hdc, hPenTemp));

        if(hPenTemp != hPen)
            info(FAIL, TEXT("Incorrect pen returned"));

        CheckNotRESULT(FALSE, DeleteObject(hPen));
    }

    myReleaseDC(hdc);
}

//***********************************************************************************
void
CreateDashPen(int testFunc)
{

    info(ECHO, TEXT("%s - Create PS_DASH Pen: width=4"), funcName[testFunc]);

    HPEN    hPen,
            hPenTemp;
    LOGPEN  logPen;
    TDC     hdc= myGetDC();

    logPen.lopnStyle = PS_DASH;
    logPen.lopnWidth.x = 4;
    logPen.lopnColor = 0x0;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        switch(testFunc)
        {
            case ECreatePenIndirect:
                CheckNotRESULT(NULL, hPen = CreatePenIndirect(&logPen));
                break;
            case ECreatePen:
                CheckNotRESULT(NULL, hPen = CreatePen(logPen.lopnStyle, logPen.lopnWidth.x, logPen.lopnColor));
                break;
        }

        // Verify  this pen
        CheckNotRESULT(NULL, hPenTemp = (HPEN) SelectObject(hdc, hPen));
        CheckNotRESULT(FALSE, Rectangle(hdc, 100, 100, 200, 200));

        CheckNotRESULT(NULL, hPenTemp = (HPEN) SelectObject(hdc, hPenTemp));
        if(hPenTemp != hPen)
            info(FAIL, TEXT("Incorrect pen returned"));

        CheckNotRESULT(FALSE, DeleteObject(hPen));
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   CreatePenIndirect Param Test
***
************************************************************************************/

//***********************************************************************************
void
CreatePenIndirectParamTest(int testFunc)
{

    info(ECHO, TEXT("%s - CreatePenIndirectParamTest"), funcName[testFunc]);

    HPEN    hPen;
    LOGPEN  logPen;
    UINT    styles[] = {
        PS_SOLID,
        PS_DASH,
        PS_NULL,
#ifndef UNDER_CE
        PS_DOT,
        PS_DASHDOT,
        PS_DASHDOTDOT,
        PS_INSIDEFRAME
#endif                          // UNDER_CE
    };
    int     numStyles = sizeof (styles) / sizeof (UINT);

    for (int i = 0; i < numStyles; i++)
        for (int n = -1; n < 2; n++)
        {
            logPen.lopnStyle = styles[i];
            logPen.lopnWidth.x = n;
            logPen.lopnWidth.y = n;
            logPen.lopnColor = RGB(0, 0, 0);

            switch(testFunc)
            {
                case ECreatePenIndirect:
                    CheckNotRESULT(NULL, hPen = CreatePenIndirect(&logPen));
                    break;
                case ECreatePen:
                    CheckNotRESULT(NULL, hPen = CreatePen(logPen.lopnStyle, logPen.lopnWidth.x, logPen.lopnColor));
                    break;
            }

            if (!hPen)
                info(FAIL, TEXT("Failed to create style:%d x:%d y:%d color:0x%x"),
                    logPen.lopnStyle, logPen.lopnWidth.x, logPen.lopnWidth.y, logPen.lopnColor);
            else
            {
                CheckNotRESULT(FALSE, DeleteObject(hPen));
            }
        }
}

/***********************************************************************************
***
***   CreatePenIndirect Bad Param Test
***
************************************************************************************/

//***********************************************************************************
void
CreatePenIndirectBadParamTest(int testFunc)
{

    info(ECHO, TEXT("%s - %d random CreatePenIndirectParamTest"), funcName[testFunc], testCycles);

// XP allows the creation of pens with random parameters
#ifdef UNDER_CE
    HPEN    hPen;
    LOGPEN  logPen;

    for (int i = 0; i < testCycles; i++)
    {
        logPen.lopnStyle = badRandomNum();
        logPen.lopnWidth.x = badRandomNum();
        logPen.lopnWidth.y = badRandomNum();
        logPen.lopnColor = randColorRef();

        switch(testFunc)
        {
            case ECreatePenIndirect:
                hPen = CreatePenIndirect(&logPen);
                break;
            case ECreatePen:
                hPen = CreatePen(logPen.lopnStyle, logPen.lopnWidth.x, logPen.lopnColor);
                break;
        }

        // pen styles 0, 1, and 5 are the only ones supported.
        if (hPen && (logPen.lopnStyle != PS_SOLID && logPen.lopnStyle != PS_DASH && logPen.lopnStyle != PS_NULL))
            info(FAIL, TEXT("Should have failed to create style:%d x:%d y:%d color:0x%x"),
                logPen.lopnStyle, logPen.lopnWidth.x, logPen.lopnWidth.y, logPen.lopnColor);
        else if(!hPen && (logPen.lopnStyle == PS_SOLID || logPen.lopnStyle == PS_DASH || logPen.lopnStyle == PS_NULL))
            info(FAIL, TEXT("Should have succeeded to create style: %d x:%d y:%d color:0x%x"),
                logPen.lopnStyle, logPen.lopnWidth.x, logPen.lopnWidth.y, logPen.lopnColor);

        if(hPen)
            CheckNotRESULT(FALSE, DeleteObject(hPen));
    }
#endif
}

/***********************************************************************************
***
***   CreatePenDrawTest
***
************************************************************************************/

//***********************************************************************************
void
CreatePenDrawTest(int testFunc)
{

    info(ECHO, TEXT("%s - CreatePenDrawTest"), funcName[testFunc]);

    SetMaxErrorPercentage(.005);

    if(SetWindowConstraint(180,180))
    {
        TDC     hdc = myGetDC();
        HBRUSH stockBrush;
        POINT   p[2] = { {190, 20}, {190, 160} }, p0[2] =
        {
            {
            190, 20}
            ,
            {
            250, 160}
        };
        RECT    StrRect = { 20, 180, 200, 200 };
        HPEN    hPen;
        LOGPEN  logPen;
        UINT    styles[] = {
            PS_SOLID,
            PS_DASH,
            PS_NULL,
    #ifndef UNDER_CE
            PS_DOT,
            PS_DASHDOT,
            PS_DASHDOTDOT,
            PS_INSIDEFRAME
    #endif                          // UNDER_CE
        };
        TCHAR   str[256] = {NULL};

        if (!IsWritableHDC(hdc))
            info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
        else
        {
            CheckNotRESULT(NULL, stockBrush = (HBRUSH) SelectObject(hdc, GetStockObject(NULL_BRUSH)));

            for (int i = 0; i < countof(styles); i++)
            {
                for (int n = -1; n < 40; n += (n < 5) ? 1 : 5)
                {
                    logPen.lopnStyle = styles[i];
                    logPen.lopnWidth.x = n;
                    logPen.lopnWidth.y = n;
                    logPen.lopnColor = RGB(0, 0, 0);

                    switch(testFunc)
                    {
                        case ECreatePenIndirect:
                            CheckNotRESULT(NULL, hPen = CreatePenIndirect(&logPen));
                            break;
                        case ECreatePen:
                            CheckNotRESULT(NULL, hPen = CreatePen(logPen.lopnStyle, logPen.lopnWidth.x, logPen.lopnColor));
                            break;
                    }

                    CheckNotRESULT(NULL, hPen = (HPEN) SelectObject(hdc, hPen));
                    CheckNotRESULT(FALSE, Ellipse(hdc, 30, 30, 150, 150));
                    CheckNotRESULT(FALSE, RoundRect(hdc, 20, 20, 160, 160, 20, 20));
                    CheckNotRESULT(FALSE, Polyline(hdc, p, 2));
                    CheckNotRESULT(FALSE, Polyline(hdc, p0, 2));
                    CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

                    _sntprintf_s(str, countof(str) - 1, TEXT("Pen:(%d %d) Type:%d"), n, n, i);
                    if(gSanityChecks)
                        info(DETAIL,TEXT("Pen:(%d %d) Type:%d"), n, n, i);

                    CheckNotRESULT(0, DrawText(hdc, str, -1, &StrRect, DT_LEFT));
                    CheckNotRESULT(NULL, hPen = (HPEN) SelectObject(hdc, hPen));
                    CheckNotRESULT(FALSE, DeleteObject(hPen));
                }
            }

            CheckNotRESULT(NULL, SelectObject(hdc, stockBrush));
        }

        myReleaseDC(hdc);

        SetWindowConstraint(0,0);
    }
    else info(DETAIL, TEXT("Screen too small for test case"));

    ResetCompareConstraints();
}

//***********************************************************************************
void
CreatePenWidthNegtive(int testFunc)
{

    info(ECHO, TEXT("%s - : Width.y = -2"), funcName[testFunc]);

    HPEN    hPen;
    LOGPEN  logPen;

    logPen.lopnStyle = PS_SOLID;
    logPen.lopnWidth.x = -2;
    logPen.lopnColor = RGB(0, 0, 0);

    switch(testFunc)
    {
        case ECreatePenIndirect:
            CheckNotRESULT(NULL, hPen = CreatePenIndirect(&logPen));
            break;
        case ECreatePen:
            CheckNotRESULT(NULL, hPen = CreatePen(logPen.lopnStyle, logPen.lopnWidth.x, logPen.lopnColor));
            break;
    }

    CheckNotRESULT(FALSE, DeleteObject(hPen));
}

//***********************************************************************************
void
CreatePenWidthRandom(int testFunc)
{

    info(ECHO, TEXT("%s - : Width.y=55555"), funcName[testFunc]);

    HPEN    hPen;
    LOGPEN  logPen;

    logPen.lopnStyle = PS_NULL;
    logPen.lopnWidth.x = 100;
    logPen.lopnWidth.y = 55555; // garbage
    logPen.lopnColor = 0x0;

    switch(testFunc)
    {
        case ECreatePenIndirect:
            CheckNotRESULT(NULL, hPen = CreatePenIndirect(&logPen));
            break;
        case ECreatePen:
            CheckNotRESULT(NULL, hPen = CreatePen(logPen.lopnStyle, logPen.lopnWidth.x, logPen.lopnColor));
            break;
    }

    CheckNotRESULT(FALSE, DeleteObject(hPen));
}

//***********************************************************************************
void
CreatePenWidth10Color(int testFunc)
{
    info(ECHO, TEXT("%s - Width=10: Color=RGB(0xFF,0,0)"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HPEN    hPen,
            hPenTemp;
    LOGPEN  logPen;
    POINT   p[2] = { {10, 0}, {10, 200} }, *pp = p;
    COLORREF colorGet,
            colorSet;

    logPen.lopnStyle = PS_SOLID;
    logPen.lopnWidth.x = 10;
    logPen.lopnWidth.y = 10;
    logPen.lopnColor = RGB(0xFF, 0, 0);

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(CLR_INVALID, colorSet = GetNearestColor(hdc, RGB(0xFF, 0, 0)));

        switch(testFunc)
        {
            case ECreatePenIndirect:
                CheckNotRESULT(NULL, hPen = CreatePenIndirect(&logPen));
                break;
            case ECreatePen:
                CheckNotRESULT(NULL, hPen = CreatePen(logPen.lopnStyle, logPen.lopnWidth.x, logPen.lopnColor));
                break;
        }

        CheckNotRESULT(NULL, hPenTemp = (HPEN) SelectObject(hdc, (HPEN) hPen));
        CheckNotRESULT(FALSE, Polyline(hdc, pp, 2));
        CheckNotRESULT(CLR_INVALID, colorGet = GetPixel(hdc, 11, 11));

        info(((colorGet != colorSet) && !isPrinterDC(hdc)) ? FAIL : DETAIL, TEXT("ColorPen=0x%lX: Get=0x%lX: Expected=0x%lX"),
            logPen.lopnColor, colorGet, colorSet);

        CheckNotRESULT(NULL, hPenTemp = (HPEN) SelectObject(hdc, hPenTemp));

        if (hPenTemp != hPen)
            info(FAIL, TEXT("Set pen not returned: Get=0x%lX: Expected=0x%lX"), hPenTemp, hPen);

        CheckNotRESULT(FALSE, DeleteObject(hPen));
    }

    myReleaseDC(hdc);
}

//***********************************************************************************
void
CreatePenWidth0(int testFunc)
{
    info(ECHO, TEXT("%s - Width = 0"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HPEN    hPen,
            hPenTemp;
    LOGPEN  logPen;
    POINT   p[] = { {0, 0}, {5, 5}, {10, 10}, {15, 15}, {20, 20}, {25, 25}, {30, 30}, {35, 35}, {40, 40}, {45, 45} };
    int     i,
            n;
    BOOL bResult;
    int pointcount;

    logPen.lopnStyle = PS_SOLID;
    logPen.lopnWidth.x = 0;
    logPen.lopnWidth.y = 5;
    logPen.lopnColor = RGB(0xFF, 0x0, 0x0);

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        switch(testFunc)
        {
            case ECreatePenIndirect:
                CheckNotRESULT(NULL, hPen = CreatePenIndirect(&logPen));
                break;
            case ECreatePen:
                CheckNotRESULT(NULL, hPen = CreatePen(logPen.lopnStyle, logPen.lopnWidth.x, logPen.lopnColor));
                break;
        }
        CheckNotRESULT(NULL, hPenTemp = (HPEN) SelectObject(hdc, (HPEN) hPen));

        n = 200;
        for (i = 0; i < n; i++)
        {
            if ((i % 50) == 0)
                info(DETAIL, TEXT("starting set %d of %d"), i, n);

            for (pointcount = 0; pointcount < countof(p); pointcount++)
            {
                // A line with 0 or 1 points is invalid, expect polyline failure with ERROR_INVALID_PARAMETER.
                CheckForRESULT((DWORD) (pointcount >= 2 ? TRUE : FALSE), bResult = Polyline(hdc, p, pointcount));

                if(!bResult && pointcount >= 2)
                    info(FAIL, TEXT("p = %d"), pointcount);
            }

        }
        CheckNotRESULT(NULL, hPenTemp = (HPEN) SelectObject(hdc, hPenTemp));

        if (hPenTemp != hPen)
            info(FAIL, TEXT("Set pen not returned: Get=0x%lX: Expected=0x%lX"), hPenTemp, hPen);

        CheckNotRESULT(FALSE, DeleteObject(hPen));
    }

    myReleaseDC(hdc);
}

void
SimpleCreatePatternBrushSizeTest(int testFunc)
{
    info(ECHO, TEXT("%s - Size Test: 1 to 50"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
              compDC;
    TBITMAP hBmp,
                stockBmp;
    HBRUSH  hBrush;

    CheckNotRESULT(NULL, compDC = CreateCompatibleDC(hdc));

    if(compDC)
    {
        for (int i = 0; i < 50; i++)
        {
            CheckNotRESULT(NULL, hBmp = CreateCompatibleBitmap(hdc, i, i));
            CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(compDC, hBmp));
            CheckNotRESULT(NULL, hBrush = CreatePatternBrush(hBmp));
            CheckNotRESULT(FALSE, DeleteObject(hBrush));
            CheckNotRESULT(NULL, SelectObject(compDC, stockBmp));
            CheckNotRESULT(FALSE, DeleteObject(hBmp));
        }
        CheckNotRESULT(FALSE, DeleteDC(compDC));
    }

    myReleaseDC(hdc);
}

/* Parameters for GetStockObject  in wingdi.h */
//#define WHITE_BRUSH         0
//#define LTGRAY_BRUSH        1
//#define GRAY_BRUSH          2
//#define DKGRAY_BRUSH        3
//#define BLACK_BRUSH         4
//#define NULL_BRUSH          5
//#define HOLLOW_BRUSH        NULL_BRUSH
void
SimpleGetSystemStockBrushTest()
{
    info(ECHO, TEXT("GetStockObject(XXX_BRUSH)"));
    TDC     hdc = myGetDC();
    HBRUSH  hbrush;
    int     i;
    RECT    rc;
    int rgBrushStyle[] = {
        WHITE_BRUSH,            //    0
        LTGRAY_BRUSH,           //    1
        GRAY_BRUSH,              //    2
        DKGRAY_BRUSH,           //    3 : DKGRAY_BRUSH == GRAY_BRUSH
        BLACK_BRUSH,            //    4
        NULL_BRUSH,             //    5
        HOLLOW_BRUSH         // 6
    };

    COLORREF crBrushColor[] = {
        RGB(0xFF, 0xFF, 0xFF),           // white
        RGB(0xC0, 0xC0, 0xC0),           // lt gray
        RGB(0x80, 0x80, 0x80),           // gray
#ifdef UNDER_CE
        RGB(0x80, 0x80, 0x80),           // dk gray
#else
        RGB(0x40, 0x40, 0x40),
#endif
        RGB(0x00, 0x00, 0x00),           // black
        RGB(0xFF, 0xFF, 0xFF),          // white/NULL - because we fill the screen with the white brush
        RGB(0xFF, 0xFF, 0xFF)           // white/Hollow - because we fill the screen with the white brush
    };

    COLORREF rgcolor[countof(rgBrushStyle)];

    // these arrays must line up for a brush name and color match.
    assert(countof(rgBrushStyle) == countof(crBrushColor));
    assert(countof(rgcolor) == countof(crBrushColor));

    rc.left = rc.top = 0;
    rc.right = zx;
    rc.bottom = zy;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // Expect whole screen is white
        CheckNotRESULT(0, FillRect(hdc, &rc, (HBRUSH) GetStockObject(WHITE_BRUSH)));

        for (i = 0; i < countof(rgBrushStyle); i++)
        {
            hbrush = (HBRUSH) GetStockObject(rgBrushStyle[i]);
            rc.left = i * (zx / countof(rgBrushStyle));
            rc.right = rc.left + (zx / countof(rgBrushStyle));
            CheckNotRESULT(0, FillRect(hdc, &rc, hbrush));
            // grab a pixel from the center of the rect.
            CheckNotRESULT(CLR_INVALID, rgcolor[i] = GetPixel(hdc, rc.left + ((rc.right - rc.left)/2), (rc.bottom - rc.top)/2));
            info(DETAIL, TEXT("GetStockObject(%d): brush color = 0x%lX"), rgBrushStyle[i], rgcolor[i]);
        }

        for (i = 0; i < countof(rgBrushStyle); i++)
        {
            COLORREF crExpectedColor = crBrushColor[i];

            // for paletted surfaces, some shades of gray may not be available.
            if(myGetBitDepth() <= 8)
            {
                // for 1bpp, brushes are always the forground unless it's the background.
                if(myGetBitDepth() == 1)
                {
                    if(crExpectedColor != GetBkColor(hdc))
                        crExpectedColor = GetTextColor(hdc);
                }
                else
                {
                    CheckNotRESULT(CLR_INVALID, crExpectedColor = GetNearestColor(hdc, crExpectedColor));
                }
            }
            // for white the 16bpp to 32bpp color conversion is correct, so do nothing for the case where
            // we're expecting white.  For shades of gray the 16bpp to 32bpp
            // conversion may be different because of the lower previously unused bits, so mask them off.
            else if(16 == myGetBitDepth() && crExpectedColor != RGB(0xFF, 0xFF, 0xFF))
                rgcolor[i] &= (gdwShiftedRedBitMask | gdwShiftedGreenBitMask | gdwShiftedBlueBitMask);

            if (rgcolor[i] != crExpectedColor && !isPrinterDC(hdc))
                info(FAIL, TEXT("For brush %d expected brush color 0x%lX, recieved brush color=0x%lX."), rgBrushStyle[i], crExpectedColor, rgcolor[i]);
        }
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   APIs
***
************************************************************************************/

//***********************************************************************************
TESTPROCAPI CreatePatternBrush_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passBrushNULL(ECreatePatternBrush);
    SimpleCreatePatternBrushTest(ECreatePatternBrush);
    SimpleCreatePatternBrushSizeTest(ECreatePatternBrush);

    // Depth
    CreatePatternBrushSizeTest(ECreatePatternBrush);
    CreatePatternBrushDrawingTest(ECreatePatternBrush);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI CreateSolidBrush_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    SimpleCreateSolidBrushTest(ECreateSolidBrush, FALSE);
    SimpleCreateSolidBrushTest(ECreateSolidBrush, TRUE);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetBrushOrgEx_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;
#ifdef UNDER_CE
    info(ECHO, TEXT("Currently not implented in Windows CE, placeholder for future test development."));
#else
    // Breadth
    checkGetSetBrushOrgEx(EGetBrushOrgEx, 0);
    checkGetSetBrushOrgEx(EGetBrushOrgEx, 1);
    checkGetSetBrushOrgExParams(EGetBrushOrgEx, 0);
    checkGetSetBrushOrgExParams(EGetBrushOrgEx, 1);

    // Depth
    // None

#endif
    return getCode();
}

//***********************************************************************************
TESTPROCAPI SetBrushOrgEx_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passBrushNULL(ESetBrushOrgEx);
    SimpleSetBrushOrgExTest();
    // None of these cases are run due to GetBrushOrgEx unavailable.
    //checkGetSetBrushOrgEx(ESetBrushOrgEx, 0);
    //checkGetSetBrushOrgEx(ESetBrushOrgEx, 1);
    //checkGetSetBrushOrgExParams(ESetBrushOrgEx, 0);
     //checkGetSetBrushOrgExParams(ESetBrushOrgEx, 1);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI CreateDIBPatternBrushPt_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    CreateDIBPatternBrushPtNULL(ECreateDIBPatternBrushPt);

    // Depth
    CreateDIBPatternBrushPtTest();

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetSysColorBrush_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    SimpleGetSysColorBrush(EGetSysColorBrush);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI CreatePenIndirect_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    CreatePenWidth0(ECreatePenIndirect);
    CreatePenWidth10Color(ECreatePenIndirect);
    CreatePenWidthRandom(ECreatePenIndirect);
    CreatePenWidthNegtive(ECreatePenIndirect);
    CreateDashPen(ECreatePenIndirect);
    CreatePenIndirectBadParamTest(ECreatePenIndirect);
    CreateThickPenWidth15(ECreatePenIndirect);
    CreateThickPenWidth20(ECreatePenIndirect);
    CreatePenAlphaTest(ECreatePenIndirect);

    // Depth
    CreatePenIndirectParamTest(ECreatePenIndirect);
    CreatePenDrawTest(ECreatePenIndirect);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI CreatePen_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    CreatePenWidth0(ECreatePen);
    CreatePenWidth10Color(ECreatePen);
    CreatePenWidthRandom(ECreatePen);
    CreatePenWidthNegtive(ECreatePen);
    CreateDashPen(ECreatePen);
    CreatePenIndirectBadParamTest(ECreatePen);
    CreateThickPenWidth15(ECreatePen);
    CreateThickPenWidth20(ECreatePen);
    CreatePenAlphaTest(ECreatePen);

    // Depth
    CreatePenIndirectParamTest(ECreatePen);
    CreatePenDrawTest(ECreatePen);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetSystemStockBrush_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    SimpleGetSystemStockBrushTest();

    // Depth
    // None

    return getCode();
}
