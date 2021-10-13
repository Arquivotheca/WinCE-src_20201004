//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
***   Simple SetBrushOrgEx Test 
***
************************************************************************************/

// sets random points as the brush origin, and verifies that the returned brush origin is correct.
void
SimpleSetBrushOrgExTest()
{
    info(ECHO, TEXT("SimpelSetBrushOrgExTest"));

    int x = 0, y = 0;
    int xprev, yprev;
    POINT pt, ptOrig;
    TDC hdc = myGetDC();

    if(!SetBrushOrgEx(hdc, x, y, &ptOrig))
        info(FAIL, TEXT("SetBrushOrgEx(hdc, 0, 0, NULL) call failed.  GLE:%d"), GetLastError());

    for(int i=0; i < 50; i++)
    {
        xprev = x;
        yprev = y;

        x = rand() % zx;
        y = rand() % zy;
        
        if(!SetBrushOrgEx(hdc, x, y, &pt))
            info(FAIL, TEXT("SetBrushOrgEx(hdc, 0, 0, &pt) call failed.  GLE:%d"), GetLastError());

        if(pt.x != xprev || pt.y != yprev)
            info(FAIL, TEXT("Expected the previous coordinate to be (%d, %d), got (%d, %d)"), xprev, yprev, pt.x, pt.y);
        
    }

    if(!SetBrushOrgEx(hdc, ptOrig.x, ptOrig.y, NULL))
        info(FAIL, TEXT("SetBrushOrgEx(hdc, 0, 0, NULL) call failed.  GLE:%d"), GetLastError());

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Simple CreateSolidBrush Test 
***
************************************************************************************/

//***********************************************************************************
void
SimpleCreateSolidBrushTest(int testFunc)
{

    info(ECHO, TEXT("%s - %d Repeating calls"), funcName[testFunc], testCycles);

    HBRUSH  hBrush;
    COLORREF color;
    int     counter = 0;
    DWORD   dwError;

    while (counter++ < testCycles)
    {
        color = randColorRef();

        mySetLastError();
        hBrush = CreateSolidBrush(color);

        if (!hBrush)
        {
            dwError = GetLastError();
            if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
                info(DETAIL, TEXT("CreateSolidBrush(%d) failed:  Out of Memory: err=%ld"), color, dwError);
            else
                info(FAIL, TEXT("CreateSolidBrush(%d) failed: err=%ld"), color, dwError);
        }
        else
            DeleteObject(hBrush);
    }
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
    HBRUSH  hBrush;
    TBITMAP hBmp = CreateCompatibleBitmap(hdc, 8, 8);
    int     counter = 0;
    DWORD   dwError;

    if (!hBmp)
    {
        dwError = GetLastError();
        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("CreateCompatibleBmp failed:  Out of Memory: err=%ld"), dwError);
        else
            info(FAIL, TEXT("CreateCompatibleBmp failed: err=%ld"), dwError);
        goto LReleaseDC;
    }

    while (counter++ < testCycles)
    {

        mySetLastError();
        hBrush = CreatePatternBrush(hBmp);
        myGetLastError(NADA);

        if (!hBrush)
        {
            dwError = GetLastError();
            if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
                info(DETAIL, TEXT("CreatePatternBrush() failed:  Out of Memory: err=%ld"), dwError);
            else
                info(FAIL, TEXT("CreatePatternBrush(%0x%x) failed: err=%ld"), hBmp, dwError);
            break;
        }
        DeleteObject(hBrush);
    }
    DeleteObject(hBmp);

  LReleaseDC:
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

    TDC     compDC = myCreateCompatibleDC(hdc);
    TBITMAP hBmp = 0,
            stockBmp;
    HBRUSH  hBrush = NULL;
    COLORREF aColor;
    TCHAR   szBuf[30];
    BYTE   *lpBits = NULL;
    DWORD   dwError = 0;
    BOOL    fSetLastError = FALSE;
    BITMAP  bmp;

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
            hBmp = CreateCompatibleBitmap(hdc, brushSize, brushSize);
            break;
        case lb:
            _stprintf(szBuf, TEXT("BMP_GDI%d"), depth);
            hBmp = myLoadBitmap(globalInst, szBuf);
            break;
        case DibRGB:
            hBmp = myCreateDIBSection(hdc, (VOID **) & lpBits, depth, brushSize, brushSize, DIB_RGB_COLORS);
            break;
        case DibPal:
            hBmp = myCreateDIBSection(hdc, (VOID **) & lpBits, depth, brushSize, brushSize, DIB_PAL_COLORS);
            break;
        default:
            info(FAIL, TEXT("Unrecognized bitmap type %d"), bmType);
            goto LDeleteDC;
            break;
    }

    if (!hBmp)
    {
        dwError = GetLastError();
        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("%s failed:  Out of Memory: err=%ld"), rgBitmapTypes[bmType].szName, dwError);
        else
            info(FAIL, TEXT("%s failed:  file:%s err=%ld"), rgBitmapTypes[bmType].szName, szBuf, dwError);
        fSetLastError = TRUE;
        goto LDeleteDC;
    }

    stockBmp = (TBITMAP) SelectObject(compDC, hBmp);
    if (bmType != lb)
    {
        PatBlt(compDC, 0, 0, brushSize, brushSize, WHITENESS);
        if (random)
        {
            for (int i = 0; i < brushSize; i++)
                for (int j = 0; j < brushSize; j++)
                {
                    aColor = goodRandomNum(256);
                    SetPixel(compDC, i, j, RGB(aColor, aColor, aColor));
                }
        }
        else
        {
            // if the brush size is 1 (1x1), then set 1 pixel to black
            if(brushSize == 1)
                SetPixel(compDC, 0, 0, RGB(0, 0, 0));
            // the brush size is 2 (a 2x2 brush, totalling 4 pixels), set it to a checkerboard pattern.
            else
            {
                // do an initial fill of all one color so all surfaces match in driver verification.
                PatBlt(compDC, 0, 0, brushSize, brushSize, WHITENESS);
                PatBlt(compDC, brushSize/2, 0, brushSize/2, brushSize/2, BLACKNESS);
                PatBlt(compDC, 0, brushSize/2, brushSize/2, brushSize/2, BLACKNESS);
            }
        }
    }

    memset(&bmp, 0, sizeof (BITMAP));
    GetObject(hBmp, sizeof (BITMAP), &bmp);
    if (!quiet)
    {
        info(DETAIL, TEXT("Brush bmp: bmWidth    =%ld"), bmp.bmWidth);
        info(DETAIL, TEXT("Brush bmp: bmHeight   =%ld"), bmp.bmHeight);
        info(DETAIL, TEXT("Brush bmp: bmBitsPixel=%ld"), bmp.bmBitsPixel);
    }

    mySetLastError();
    hBrush = CreatePatternBrush(hBmp);
    myGetLastError(NADA);

    // DIBs work on NT1314
    //if (hBrush && bmType == Dib)
    //   info(FAIL,TEXT("CreatePatternBrush(%0x%x) should have failed"),hBmp); 
    //else if (!hBrush && !bmType == Dib)
    //   info(FAIL,TEXT("CreatePatternBrush(%0x%x) failed"),hBmp);    

    if (!hBrush)
    {
        dwError = GetLastError();
        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("CreatePatternBrush(0x%X) failed: OUT of Memory: err=%ld"), hBmp, dwError);
        else
            info(FAIL, TEXT("CreatePatternBrush(0x%X) failed: err=%ld"), hBmp, dwError);
        fSetLastError = TRUE;
    }

    SelectObject(compDC, stockBmp);
    DeleteObject(hBmp);

  LDeleteDC:
    myDeleteDC(compDC);

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

    for (int t = 0; t < numBitMapTypes; t++)
        for (int d = (t != compatibleBitmap) ? 0 : countof(bitmapBrushDepths) - 1; d < countof(bitmapBrushDepths); d++)
        {
            // set up brush.  no bitmaps available for 2, 16, or 32bpp.
            if((t == DibPal && bitmapBrushDepths[d] > 8) || 
                (t==lb && (bitmapBrushDepths[d] == 2 || bitmapBrushDepths[d] == 16 || bitmapBrushDepths[d] == 32)))
                continue;

            hBrush = setupBrush(hdc, t, bitmapBrushDepths[d], 8, 1, 0);
            SetBrushOrgEx(hdc, 0, 0, &aPoint);
            stockBrush = (HBRUSH) SelectObject(hdc, hBrush);

            if (hBrush)
            {
                // set up screen 
                myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);
                myPatBlt(hdc, 0, 0, zx / 2, zy, PATCOPY);
                myBitBlt(hdc, zx / 2, 0, zx / 2, zy, hdc, 0, 0, SRCCOPY);

                // draw on screen 
                // n = times to loop
                for (n = 0; n < 20; n++)
                    for (i = 7; i >= 0; i--)
                    {
                        // SetBrushOrg must be called prior to selecting in the brush.
                        //UnrealizeObject(hBrush); REVIEW
                        SelectObject(hdc, GetStockObject(NULL_BRUSH));
                        SetBrushOrgEx(hdc, i, i, &aPoint);

                        SelectObject(hdc, hBrush);
                        for (x = 0; x + Cube <= Board.x; x += Cube)
                            for (y = 0; y < Board.y; y += Cube)
                            {
                                if ((x/Cube + y/Cube) % 2 == 0)
                                {
                                    myPatBlt(hdc, x, y, Cube, Cube, PATCOPY);
                                }
                            }
                    }
                CheckScreenHalves(hdc);
            }
            // clean up
            SelectObject(hdc, stockBrush);
            DeleteObject(hBrush);
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
            pixels,
            addFactor = 1,
            expected = 0;
    DWORD   dw;

    SetBrushOrgEx(hdc, 0, 0, &aPoint);

    for (t = 0; t < zy && t < zx; t += addFactor)
    {
        if (t > 15)
            addFactor = 100;

        // set up brush
        hBrush = setupBrush(hdc, compatibleBitmap, 8, t, 0, 0);
        if (hBrush)
        {

            SetLastError(0);
            if (NULL == (stockBrush = (HBRUSH) SelectObject(hdc, hBrush)))
            {
                dw = GetLastError();
                if (dw == ERROR_NOT_ENOUGH_MEMORY || dw == ERROR_OUTOFMEMORY)
                    info(DETAIL, TEXT("SelectObject() failed: Out of Memory: brush size:%d:"), t);
                else
                {
                    info(DETAIL, TEXT("SelectObject() failed: brush size:%d:  err=%ld"), t, dw);
                    goto LDeleteBrush;
                }
            }

            // set up screen 
            myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);

            // draw on screen 
            myPatBlt(hdc, 0, 0, t, t, PATCOPY);

            // setupBrush will make a checkerboard pattern, so calculate the number of black
            // pixels that would be in that checkerboard.
            if(0 == t)
                expected = 0;
            else if(1 == t)
                expected = 1;
            else 
                expected = (((int)(t/2)) * ((int)(t/2))) * 2;

            pixels = CheckAllWhite(hdc, 1, 1, 1, expected);

            if (expected != pixels)
            {
                dw = GetLastError();
                if (dw == ERROR_NOT_ENOUGH_MEMORY || dw == ERROR_OUTOFMEMORY)
                    info(DETAIL, TEXT("CheckAllWhite() failed: Out of Memory: size:%d: found %d"), t, pixels);
                else
                {
                    info(FAIL, TEXT("For size:%d expected:%d found:%d"), t, expected, pixels);
                }
            }
            else
            {
                info(DETAIL, TEXT("CheckAllWhite() Succeeded: size:%d: found %d"), t, pixels);
            }
            
            // clean up
            SelectObject(hdc, stockBrush);
          LDeleteBrush:
            DeleteObject(hBrush);
        }
        else
        {
            dw = GetLastError();
            if (dw == ERROR_NOT_ENOUGH_MEMORY || dw == ERROR_OUTOFMEMORY)
                info(DETAIL, TEXT("Brush Create Failed size:%d: Out of Memory"), t);
            else
                info(FAIL, TEXT("Brush Create Failed size:%d: err=%ld"), t, dw);
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
            DeleteObject(hBrush);
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
    int x = rand() % 255, y = rand() % 255;
    POINT pt;

    switch(testFunc)
        {
        case ECreatePatternBrush:
            hBrush = CreatePatternBrush((TBITMAP) NULL);
            if (hBrush)
                info(FAIL, TEXT("CreatePatternBrush(NULL) should have failed"));
            break;

        case ESetBrushOrgEx:
            if(SetBrushOrgEx((HDC) NULL, x, y, NULL))
                info(FAIL, TEXT("SetBrushOrgEx(NULL, %d, %d, NULL) succeeded"), x, y);
            if(SetBrushOrgEx((HDC) NULL, x, y, &pt))
                info(FAIL, TEXT("SetBrushOrgEx(NULL, %d, %d, lppt) succeeded"), x, y);
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

    HBRUSH  result;

    result = CreateDIBPatternBrushPt(NULL, DIB_PAL_COLORS);
    passCheck(result != NULL, 0, TEXT("NULL,DIB_PAL_COLORS"));
    result = CreateDIBPatternBrushPt(NULL, DIB_RGB_COLORS);
    passCheck(result != NULL, 0, TEXT("NULL,DIB_RGB_COLORS"));
    result = CreateDIBPatternBrushPt(NULL, 777);
    passCheck(result != NULL, 0, TEXT("NULL,777"));
}

void
CreateThickPenWidth20(int testFunc)
{
    info(ECHO, TEXT("%s - EXPECT: thick rect with nice corners: pen width=20"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    LOGPEN  lPen;
    HPEN    hPen,
            hPenTemp;
    int     result;

    lPen.lopnStyle = PS_SOLID;
    lPen.lopnWidth.x = 20;
    lPen.lopnColor = 0x0;

    hPen = CreatePenIndirect(&lPen);
    hPenTemp = (HPEN) SelectObject(hdc, hPen);
    result = RoundRect(hdc, 50, 50, 150, 150, 15, 15);
    passCheck(result, 1, TEXT("RoundRect(hdc,50,50,150,150,15,15)"));

    Sleep(1);
    hPenTemp = (HPEN) SelectObject(hdc, hPenTemp);
    passCheck(hPenTemp == hPen, TRUE, TEXT("Incorrect pen returned"));

    DeleteObject(hPen);
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

    hPen = CreatePenIndirect(&lPen);
    hPenTemp = (HPEN) SelectObject(hdc, hPen);
    Polyline(hdc, points, 7);

    Sleep(10);
    hPenTemp = (HPEN) SelectObject(hdc, hPenTemp);
    passCheck(hPenTemp == hPen, TRUE, TEXT("Incorrect pen returned"));

    DeleteObject(hPen);
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
    TDC     hdc;

    logPen.lopnStyle = PS_DASH;
    logPen.lopnWidth.x = 4;
    logPen.lopnColor = 0x0;

    mySetLastError();
    hPen = CreatePenIndirect(&logPen);
    myGetLastError(0);
    passCheck(hPen != NULL, 1, TEXT("CreatePenIndirect(&logPen)"));

    // Verify  this pen
    hdc = myGetDC();

    hPenTemp = (HPEN) SelectObject(hdc, hPen);
    Rectangle(hdc, 100, 100, 200, 200);

    Sleep(1000);
    hPenTemp = (HPEN) SelectObject(hdc, hPenTemp);
    passCheck(hPenTemp == hPen, TRUE, TEXT("Incorrect pen returned"));

    DeleteObject(hPen);
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
            mySetLastError();
            hPen = CreatePenIndirect(&logPen);
            myGetLastError(NADA);
            if (!hPen)
                info(FAIL, TEXT("Failed to create style:%d x:%d y:%d color:0x%x"), logPen.lopnStyle, logPen.lopnWidth.x,
                     logPen.lopnWidth.y, logPen.lopnColor);
            else
            {
                DeleteObject(hPen);
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

    for (int i = 0; i < 10; i++)
    {
        logPen.lopnStyle = badRandomNum();
        logPen.lopnWidth.x = badRandomNum();
        logPen.lopnWidth.y = badRandomNum();
        logPen.lopnColor = randColorRef();
        hPen = CreatePenIndirect(&logPen);
        if (hPen && (logPen.lopnStyle < 0 || logPen.lopnStyle > 5))
            info(FAIL, TEXT("Should have failed to create style:%d x:%d y:%d color:0x%x"), logPen.lopnStyle, logPen.lopnWidth.x,
                 logPen.lopnWidth.y, logPen.lopnColor);
        else
            DeleteObject(hPen);
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

    if(SetWindowConstraint(180,180))
    {
        TDC     hdc = myGetDC();
        
        float     OldMaxErrors = SetMaxErrorPercentage((float) .005);
        POINT   p[2] = { {190, 20}, {190, 160} }, p0[2] =
        {
            {
            190, 20}
            ,
            {
            250, 160}
        };
        TCHAR   str[256];
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

        SelectObject(hdc, GetStockObject(NULL_BRUSH));

        for (int i = 0; i < countof(styles); i++)
        {
            for (int n = -1; n < 40; n += (n < 5) ? 1 : 5)
            {
                logPen.lopnStyle = styles[i];
                logPen.lopnWidth.x = n;
                logPen.lopnWidth.y = n;
                logPen.lopnColor = RGB(0, 0, 0);
                hPen = CreatePenIndirect(&logPen);
                hPen = (HPEN) SelectObject(hdc, hPen);
                Ellipse(hdc, 30, 30, 150, 150);
                RoundRect(hdc, 20, 20, 160, 160, 20, 20);
                Polyline(hdc, p, 2);
                Polyline(hdc, p0, 2);
                DeleteObject(hPen);
                Sleep(50);
                PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
                _stprintf(str, TEXT("Pen:(%d %d) Type:%d"), n, n, i);
                //info(ECHO,TEXT("%s"),str);
                DrawText(hdc, str, -1, &StrRect, DT_LEFT);

                hPen = (HPEN) SelectObject(hdc, hPen);
                DeleteObject(hPen);
            }
        }
        myReleaseDC(hdc);
        SetMaxErrorPercentage(OldMaxErrors);
        SetWindowConstraint(0,0);
    }
    else info(DETAIL, TEXT("Screen too small for test case"));
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

    mySetLastError();
    hPen = CreatePenIndirect(&logPen);
    myGetLastError(NADA);
    passCheck(hPen != NULL, 1, TEXT("CreatePenIndirect(&logPen)"));
    DeleteObject(hPen);
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

    mySetLastError();
    hPen = CreatePenIndirect(&logPen);
    myGetLastError(NADA);
    passCheck(hPen != NULL, 1, TEXT("CreatePenIndirect(&logPen)"));
    DeleteObject(hPen);
}

//***********************************************************************************
void
CreatePenWidth10Color(int testFunc)
{
    info(ECHO, TEXT("%s - : With Width=10: Color=RGB(0xFF,0,0)"), funcName[testFunc]);

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

    colorSet = GetNearestColor(hdc, RGB(0xFF, 0, 0));

    hPen = CreatePenIndirect(&logPen);
    passCheck(hPen != NULL, 1, TEXT("CreatePenIndirect(&logPen)"));
    hPenTemp = (HPEN) SelectObject(hdc, (HPEN) hPen);
    Polyline(hdc, pp, 2);
    colorGet = GetPixel(hdc, 11, 11);

    info(((colorGet != colorSet) && !g_fPrinterDC) ? FAIL : ECHO, TEXT("ColorPen=0x%lX: Get=0x%lX: Expected=0x%lX"), logPen.lopnColor, colorGet,
         colorSet);

    hPenTemp = (HPEN) SelectObject(hdc, hPenTemp);
    if (hPenTemp != hPen)
        info(FAIL, TEXT("Set pen not returned: Get=0x%lX: Expected=0x%lX"), hPenTemp, hPen);

    DeleteObject(hPen);
    myReleaseDC(hdc);
}

//***********************************************************************************
void
CreatePenWidth0(int testFunc)
{
    info(ECHO, TEXT("%s - : With Width = 0"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HPEN    hPen,
            hPenTemp;
    LOGPEN  logPen;
    POINT   p[10] = { {0, 0}, {5, 5}, {10, 10}, {15, 15}, {20, 20}, {25, 25}, {30, 30}, {35, 35}, {40, 40}, {45, 45} }, *pp = p;
    int     i,
            j,
            n;

    logPen.lopnStyle = PS_SOLID;
    logPen.lopnWidth.x = 0;
    logPen.lopnWidth.y = 5;
    logPen.lopnColor = RGB(0xFF, 0x0, 0x0);

    hPen = CreatePenIndirect(&logPen);
    passCheck(hPen != NULL, 1, TEXT("CreatePenIndirect(&logPen)"));
    hPenTemp = (HPEN) SelectObject(hdc, (HPEN) hPen);

    n = 1000;
    for (i = 0; i < n; i++)
    {
        if ((i % 100) == 0)
            info(ECHO, TEXT("starting set %d of %d"), i, n);
        for (j = 0; j < 100; j++)
        {
            Polyline(hdc, pp, (j % 11));
        }
    }
    hPenTemp = (HPEN) SelectObject(hdc, hPenTemp);
    if (hPenTemp != hPen)
        info(FAIL, TEXT("Set pen not returned: Get=0x%lX: Expected=0x%lX"), hPenTemp, hPen);

    DeleteObject(hPen);
    myReleaseDC(hdc);
}

void
SimpleCreatePatternBrushSizeTest(int testFunc)
{
    info(ECHO, TEXT("%s - Size Test: 1 to 50"), funcName[testFunc]);

    info(ECHO, TEXT(""));

    TDC     hdc = myGetDC(),
            compDC = CreateCompatibleDC(hdc);
    TBITMAP hBmp,
            stockBmp;
    HBRUSH  hBrush;

    for (int i = 0; i < 50; i++)
    {
        info(ECHO, TEXT("Attempting size %dx%d"), i, i);
        hBmp = CreateCompatibleBitmap(hdc, i, i);
        passCheck(hBmp != NULL, 1, TEXT("hBmp != NULL"));
        stockBmp = (TBITMAP) SelectObject(compDC, hBmp);
        hBrush = CreatePatternBrush(hBmp);
        passCheck(hBrush != NULL, 1, TEXT("hBrush != NULL"));
        DeleteObject(hBrush);
        SelectObject(compDC, stockBmp);
        DeleteObject(hBmp);
    }
    myReleaseDC(hdc);
    DeleteDC(compDC);
}

/* Parameters for GetStockObject  in wingdi.h */
//#define WHITE_BRUSH         0
//#define LTGRAY_BRUSH        1
//#define GRAY_BRUSH          2
//#define DKGRAY_BRUSH        3
//#define BLACK_BRUSH         4
//#define NULL_BRUSH          5
//#define HOLLOW_BRUSH        NULL_BRUSH
void FAR PASCAL
SimpleGetSystemStockBrushTest()
{
#define MAX_STOCK_BRUSH  5

    info(ECHO, TEXT("GetStockObject(XXX_BRUSH"));
    TDC     hdc = myGetDC();
    COLORREF rgcolor[6];
    HBRUSH  hbrush;
    int     i;
    RECT    rc;

    int     rgBrushStyle[MAX_STOCK_BRUSH] = {
        WHITE_BRUSH,            //    0 
        LTGRAY_BRUSH,           //    1 
        //      GRAY_BRUSH      ,//    2 
        DKGRAY_BRUSH,           //    3 : DKGRAY_BRUSH == GRAY_BRUSH
        BLACK_BRUSH,            //    4 
        NULL_BRUSH,             //    5 
    };

    rc.left = rc.top = 0;
    rc.right = zx;
    rc.bottom = zy;

    // Expect whole screen is white
    FillRect(hdc, &rc, (HBRUSH) GetStockObject(WHITE_BRUSH));

    for (i = 0; i < MAX_STOCK_BRUSH; i++)
    {
        hbrush = (HBRUSH) GetStockObject(rgBrushStyle[i]);
        rc.left = i * zx / 5;
        rc.right = rc.left + zx / 5;
        FillRect(hdc, &rc, hbrush);
        rgcolor[i] = GetPixel(hdc, rc.left + 5, (rc.bottom - rc.top)/2);
        info(DETAIL, TEXT("GetStockObject(%d): brush color = 0x%lX"), rgBrushStyle[i], rgcolor[i]);
    }

    // check: except NULL BRUSH
    for (i = 0; i < 3; i++)
    {
        if (rgcolor[i + 1] >= rgcolor[i] && !g_fPrinterDC)
            info(FAIL, TEXT("GetStockObject(%d): brush color=0x%lX  not as expected"), rgBrushStyle[i + 1], rgcolor[i + 1]);
    }

    // Chceck NULL Brush
    if (rgcolor[4] != rgcolor[0] && !g_fPrinterDC)
        info(FAIL, TEXT("GetStockObject(NULL_BRUSH): brush color=0x%lX  not as expected 0x%lX"), rgBrushStyle[4], rgcolor[0]);

    Sleep(5000);
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
    SimpleCreateSolidBrushTest(ECreateSolidBrush);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI SetBrushOrgEx_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passBrushNULL(ESetBrushOrgEx);
    SimpleSetBrushOrgExTest();

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
    // None

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

    // Depth
    CreatePenIndirectParamTest(ECreatePenIndirect);
    CreatePenDrawTest(ECreatePenIndirect);

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
