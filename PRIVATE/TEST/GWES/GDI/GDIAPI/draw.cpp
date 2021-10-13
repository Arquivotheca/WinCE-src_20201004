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

   draw.cpp

Abstract:

   Gdi Tests:  Palette & Drawing APIs

***************************************************************************/

#include "global.h"
#include "fontdata.h"
#include "drawdata.h"
#include "rgnData.h"

/***********************************************************************************
***
***   Universal Data used in this file
***
************************************************************************************/
#define  NUMRECTS  9
static RECT rTest[NUMRECTS];

// this is a fairly comprehensive list of all of the important named and unnamed rop's,
// based on what many drivers accelerate in hardware, what the emul library accelerates,
// and what is known to be commonly used by RDP (which is probably accelerated in emul).

static NameValPair gnvRop4Array[] = {
                                NAMEVALENTRY(MAKEROP4(DSTINVERT, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(SRCINVERT, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(SRCCOPY, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(SRCPAINT, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(SRCAND, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(SRCERASE, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(MERGECOPY, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(MERGEPAINT, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(NOTSRCCOPY, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(NOTSRCERASE, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(PATCOPY, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(PATINVERT, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(PATPAINT, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(BLACKNESS, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(WHITENESS, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(0x00AA0000, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(0x00B80000, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(0x00A00000, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(0x000F0000, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(0x00220000, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(0x00C00000, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(0x00BA0000, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(0x00990000, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(0x00690000, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(0X00E20000, BLACKNESS)),
                                //NAMEVALENTRY(MAKEROP4(WHITENESS, SRCCOPY)),
                                NAMEVALENTRY(MAKEROP4(SRCCOPY, 0X00AA0029)),
                                //NAMEVALENTRY(MAKEROP4(SRCAND, SRCCOPY)),
                                NAMEVALENTRY(MAKEROP4(SRCCOPY, ~DSTINVERT)),
                                NAMEVALENTRY(MAKEROP4(SRCCOPY, PATINVERT)),
                                NAMEVALENTRY(MAKEROP4(0X00AA0000, SRCCOPY))
                                };

static NameValPair gnvRop3Array[] = {
                                NAMEVALENTRY(DSTINVERT), //5555
                                NAMEVALENTRY(SRCINVERT), // 6666
                                NAMEVALENTRY(SRCCOPY), //cccc
                                NAMEVALENTRY(SRCPAINT), //eeee
                                NAMEVALENTRY(SRCAND), //8888
                                NAMEVALENTRY(SRCERASE), //4444
                                NAMEVALENTRY(MERGECOPY), //0c0c
                                NAMEVALENTRY(MERGEPAINT), //bbbb
                                NAMEVALENTRY(NOTSRCCOPY), //3333
                                NAMEVALENTRY(NOTSRCERASE), //1111
                                NAMEVALENTRY(PATCOPY), //f0f0
                                NAMEVALENTRY(PATINVERT), //5a5a
                                NAMEVALENTRY(PATPAINT), //fbfb
                                NAMEVALENTRY(BLACKNESS), //0000
                                NAMEVALENTRY(WHITENESS), //ffff
                                NAMEVALENTRY(0x00AA0000), // NOP
                                NAMEVALENTRY(0x00B80000),
                                NAMEVALENTRY(0x00A00000),
                                NAMEVALENTRY(0x00220000),
                                NAMEVALENTRY(0x00C00000),
                                NAMEVALENTRY(0x00BA0000),
                                NAMEVALENTRY(0x00990000),
                                NAMEVALENTRY(0x00690000),
                                NAMEVALENTRY(0x00E20000),
                                NAMEVALENTRY(0x00050000),
                                NAMEVALENTRY(0x000A0000),
                                NAMEVALENTRY(0x000F0000),
                                NAMEVALENTRY(0x00500000),
                                NAMEVALENTRY(0x005F0000),
                                NAMEVALENTRY(0x00A50000),
                                NAMEVALENTRY(0x00AF0000),
                                NAMEVALENTRY(0x00F50000),
                                NAMEVALENTRY(0x00FA0000),
                                NAMEVALENTRY(0x005A0000),
                                NAMEVALENTRY(0x00A00000),
                                };

//***********************************************************************************
static NameValPair gnvRop2Array[] = {
                                NAMEVALENTRY(R2_BLACK),
                                NAMEVALENTRY(R2_COPYPEN),
                                NAMEVALENTRY(R2_MASKNOTPEN),
                                NAMEVALENTRY(R2_MASKPEN),
                                NAMEVALENTRY(R2_MASKPENNOT),
                                NAMEVALENTRY(R2_MERGENOTPEN),
                                NAMEVALENTRY(R2_MERGEPEN),
                                NAMEVALENTRY(R2_MERGEPENNOT),
                                NAMEVALENTRY(R2_NOP),
                                NAMEVALENTRY(R2_NOT),
                                NAMEVALENTRY(R2_NOTCOPYPEN),
                                NAMEVALENTRY(R2_NOTMASKPEN),
                                NAMEVALENTRY(R2_NOTMERGEPEN),
                                NAMEVALENTRY(R2_NOTXORPEN),
                                NAMEVALENTRY(R2_WHITE),
                                NAMEVALENTRY(R2_XORPEN),
                                };

static int gBitDepths[] = { 1, 
#ifdef UNDER_CE // desktop doesn't support 2bpp surfaces
                                        2, 
#endif
                                        4, 8, 16, 24, 32 };

VOID FAR PASCAL
CheckRgnData(HRGN hrgn, int x)
{
    UINT    i;
    RECT   *lpr;
    LPRGNDATA lprgnData;

    lprgnData = (LPRGNDATA) LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, 256);
    GetRegionData(hrgn, 256, lprgnData);

    info(DETAIL, TEXT("hrgn: rcBound: left=%d  top=%d  right=%d  bottom=%d\r\n"), lprgnData->rdh.rcBound.left,
         lprgnData->rdh.rcBound.top, lprgnData->rdh.rcBound.right, lprgnData->rdh.rcBound.bottom);

    lpr = (LPRECT) lprgnData->Buffer;
    for (i = 0; i < lprgnData->rdh.nCount; i++)
    {
        info(DETAIL, TEXT("rect %d:  left=%d, top=%d, right=%d bottom=%d\r\n"), i, lpr->left, lpr->top, lpr->right,
             lpr->bottom);
        lpr++;
    }

    LocalFree(lprgnData);
}

void
setTestRects(void)
{
    int     x1 = zx / 4,
            y1 = zy / 4,
            x2 = zx / 2,
            y2 = zy / 2,
            x3 = x2 + x1,
            y3 = y2 + y1,
            x5 = zx + x1,
            y5 = zy + y1;
//            x6 = zx + x2,
//            y6 = zy + y2,
//            x7 = zx + x3,
//            y7 = zy + y3;

    SetRect(&rTest[0], -x1,  y1, x1, y3);   // left edge clipping
    SetRect(&rTest[1],  x1, -y1, x3, y1);   // topedgeclipping
    SetRect(&rTest[2],  x3,  y1, x5, y3);   // rightedgeclipping
    SetRect(&rTest[3],  x1,  y3, x3, y5);   // bottomedgeclipping
    SetRect(&rTest[4], -x1, -y1, x1, y1);   // upper-leftcornerclipping
    SetRect(&rTest[5],  x3, -y1, x5, y1);   // upper-rightcornerclipping
    SetRect(&rTest[6],  x3,  y3, x5, y5);   // lower-rightcornerclipping
    SetRect(&rTest[7], -x1,  y3, x1, y5);   // lower-leftcornerclipping
    SetRect(&rTest[8],  x1,  y1, x3, y3);   // center
}

/***********************************************************************************
***
***   Support - draws rect in some color
***
************************************************************************************/

//***********************************************************************************
void
blastRect(TDC hdc, int x, int y, int sizeX, int sizeY, COLORREF color, int testFunc)
{

    if (gSanityChecks)
    {
        info(ECHO, TEXT("Inside blastRect"));
        info(DETAIL, TEXT("{%3d,%3d,%3d,%3d,%9d},"), x, y, sizeX, sizeY, color);
    }

    HBRUSH  hBrush = CreateSolidBrush(color),
            stockBrush = (HBRUSH) SelectObject(hdc, hBrush);
    int     result = 0;

    SelectObject(hdc, (HPEN) GetStockObject(NULL_PEN));

    switch (testFunc)
    {
        case EPatBlt:
            result = myPatBlt(hdc, x, y, sizeX, sizeY, PATCOPY);
            break;
        case ERectangle:
            result = Rectangle(hdc, x, y, x + sizeX, y + sizeY);    // fill interior only
            break;
        case EEllipse:
            result = Ellipse(hdc, x, y, x + sizeX, y + sizeY);
            break;
        case ERoundRect:
            result = RoundRect(hdc, x, y, x + sizeX, y + sizeY, 0, 0);
            break;
    }

    if (!result)
        info(FAIL, TEXT("%s failed on(0x%x %d %d %d %d %d)"), funcName[testFunc], hdc, x, y, sizeX, sizeY, PATCOPY);

    if (!hBrush)
        info(FAIL, TEXT("CreateBrushIndirect failed"));

    RECT    r = { x, y, x + sizeX, y + sizeY };

    if (sizeX < 1 || sizeY < 1)
        goodBlackRect(hdc, &r);

    SelectObject(hdc, stockBrush);
    DeleteObject(hBrush);
}

/***********************************************************************************
***
***   Blasts black rect and patblt covers with white, then checks for all white
***
************************************************************************************/

//***********************************************************************************
void
checkRectangles(int testFunc)
{

    info(ECHO, TEXT("%s: checkRectangles"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    int     test = NUMRECTS;

    setTestRects();

    // look at all of the tests
    while (--test >= 0)
    {
        info(DETAIL, TEXT("Testing Rect (%d %d %d %d) index:%d"), rTest[test].left, rTest[test].top, rTest[test].right,
             rTest[test].bottom, test);

        // clear the screen
        myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);
        SelectObject(hdc, GetStockObject(BLACK_BRUSH));
        SelectObject(hdc, (HPEN) GetStockObject(NULL_PEN));
        // blast a rectangle
        mySetLastError();
        Rectangle(hdc, rTest[test].left, rTest[test].top, rTest[test].right, rTest[test].bottom);
        myGetLastError(NADA);
        // cover over the rectangle
        myPatBlt(hdc, rTest[test].left, rTest[test].top, rTest[test].right - rTest[test].left,
                 rTest[test].bottom - rTest[test].top, WHITENESS);
        // is the screen clear?
        CheckAllWhite(hdc, 0, 0, 0, 0);
    }
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Draws rectangular concentric lines
***
************************************************************************************/

BOOL
DrawDiagonals(TDC hdc,int w, int h)
{
    BOOL bResult = TRUE;

    // draw a diagonal stripe across the mask, useful for getting an idea of the locations of the mask being used.
    double fAddx = ((double) w)/20.0, fAddy = ((double) h)/20.0;
    double fw =0, fh = 0;

    while((int) fw <= w && (int) fh <= h)
    {
        bResult &= PatBlt(hdc, (int) fw, (int) fh, (int) fAddx, (int) fAddy, BLACKNESS);
        fw += fAddx;
        fh += fAddy;
    }

    // draw another diagonal covering the other corners, but make it smaller boxes so they are visually different.
    fAddx = ((double) w)/40.0;
    fAddy = ((double) h)/40.0;
    fw = 0; 
    fh = ((double) h) - fAddy;
    while((int) fw <= w && (int) fh >= 0)
    {
        bResult &= PatBlt(hdc, (int) fw, (int) fh, (int) fAddx, (int) fAddy, BLACKNESS);
        fw += fAddx;
        fh -= fAddy;
    }

    if(!bResult)
        info(FAIL, TEXT("drawing diagonals on surface failed."));

    return bResult;
}
//***********************************************************************************
void
PatternLines(int testFunc)
{

    info(ECHO, TEXT("%s: Testing PatBlts Ability to Draw Lines"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    int     test = NUMRECTS,
            h,
            w;
    RECT    temp;

    setTestRects();

    while (--test >= 0)
    {
        info(DETAIL, TEXT("Testing Rect (%d %d %d %d) index:%d"), rTest[test].left, rTest[test].top, rTest[test].right,
             rTest[test].bottom, test);

        // clear the screen
        myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);
        // draw a black rect
        myPatBlt(hdc, rTest[test].left, rTest[test].top, rTest[test].right - rTest[test].left,
                 rTest[test].bottom - rTest[test].top, BLACKNESS);

        // fill in temps
        temp.left = rTest[test].left;
        temp.top = rTest[test].top;
        w = temp.right = rTest[test].right - rTest[test].left;  // the width
        h = temp.bottom = rTest[test].bottom - rTest[test].top; // the height

        // draw concentric white squares using patblt as a line function
        for (int i = 0; temp.right >= 0 && temp.bottom >= 0; i++)
        {
            myPatBlt(hdc, temp.left, temp.top, 1, temp.bottom, WHITENESS);  // L
            myPatBlt(hdc, temp.left, temp.top, temp.right, 1, WHITENESS);   // T
            myPatBlt(hdc, temp.left + temp.right, temp.top, 1, temp.bottom + 1, WHITENESS); // R
            myPatBlt(hdc, temp.left, temp.top + temp.bottom, temp.right + 1, 1, WHITENESS); // B
            // move inward stepping 2
            temp.left += 2;
            temp.top += 2;
            if (temp.right >= 0)
                temp.right -= 4;
            if (temp.bottom >= 0)
                temp.bottom -= 4;
        }

        // set up temps for move outward
        temp.left++;
        temp.top++;
        temp.right -= 2;
        temp.bottom -= 2;

        // draw concentric white squares using patblt as a line function
        for (i = 0; temp.right <= w && temp.bottom <= h; i++)
        {
            myPatBlt(hdc, temp.left, temp.top, 1, temp.bottom, WHITENESS);  // L
            myPatBlt(hdc, temp.left + 1, temp.top, temp.right, 1, WHITENESS);   // T
            myPatBlt(hdc, temp.left + temp.right, temp.top + 1, 1, temp.bottom - 1, WHITENESS); // R
            myPatBlt(hdc, temp.left, temp.top + temp.bottom, temp.right + 1, 1, WHITENESS); // B
            // move outward covering missed black lines
            temp.left -= 2;
            temp.top -= 2;
            if (temp.right <= w)
                temp.right += 4;
            if (temp.bottom <= h)
                temp.bottom += 4;
        }
        // is the screen clear?
        CheckAllWhite(hdc, 0, 0, 0, 0);
    }
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Blasts rgn and checks if it was drawn correctly
***
************************************************************************************/

//***********************************************************************************
void
FillCheck(int testFunc)
{

    info(ECHO, TEXT("%s - %d Checks for correct drawing"), funcName[testFunc], NUMRECTS);
    float     OldMaxErrors = SetMaxErrorPercentage((testFunc == EEllipse || testFunc == ERoundRect)? (float) .005 : (float) 0);
    TDC     hdc = myGetDC();
    setTestRects();

    SelectObject(hdc, GetStockObject(BLACK_BRUSH));

    for (int test = 0; test < NUMRECTS; test++)
    {
        info(DETAIL, TEXT("Drawing %d %d %d %d"), rTest[test].left, rTest[test].top, rTest[test].right, rTest[test].bottom);
        // cover screen with white
        myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);

        // blast a black rect using rectangle or a patblt
        switch (testFunc)
        {
            case EPatBlt:
                myPatBlt(hdc, rTest[test].left, rTest[test].top, rTest[test].right - rTest[test].left,
                         rTest[test].bottom - rTest[test].top, PATCOPY);
                break;
            case ERectangle:
                SelectObject(hdc, (HPEN) GetStockObject(NULL_PEN));
                mySetLastError();
                Rectangle(hdc, rTest[test].left, rTest[test].top, rTest[test].right, rTest[test].bottom);
                myGetLastError(NADA);
                break;
            case EEllipse:
                Ellipse(hdc, rTest[test].left, rTest[test].top, rTest[test].right, rTest[test].bottom);
                break;
            case ERoundRect:
                RoundRect(hdc, rTest[test].left, rTest[test].top, rTest[test].right, rTest[test].bottom, 5, 5);
                break;
        }
        // make sure the rect was black and borders are correct
        goodBlackRect(hdc, &rTest[test]);
    }
    myReleaseDC(hdc);
    // verification tolerances must be left active until after myReleaseDC does verification.
    SetMaxErrorPercentage(OldMaxErrors);
}

/***********************************************************************************
***
***   Checks for Visibility of a rect
***
************************************************************************************/

//***********************************************************************************
void
VisibleCheck(int testFunc)
{
    info(ECHO, TEXT("%s - %d Tests in/out of View"), funcName[testFunc], NUMRECTS * 2);

    TDC     hdc = myGetDC();
    int     test = 0;
    BOOL    result;
    RECT    temp;

    setTestRects();

    // look for ects inside hdc
    for (test = 0; test < NUMRECTS; test++)
    {
        //info(ECHO,TEXT("[%d] %d %d %d %d"),test,rTest[0].top,rTest[0].left,rTest[0].right,rTest[0].bottom);
        mySetLastError();
        result = RectVisible(hdc, &rTest[test]);
        myGetLastError(NADA);
        if (!result)
            info(FAIL, TEXT("Visible Rect(%d %d %d %d) returned False"), rTest[test].left, rTest[test].top,
                 rTest[test].right, rTest[test].bottom);
    }

    // look for rects outside of hdc
    for (test = 0; test < NUMRECTS; test++)
    {
        temp.left = rTest[test].left + 5000;
        temp.top = rTest[test].top + 5000;
        temp.right = rTest[test].right + 5000;
        temp.bottom = rTest[test].bottom + 5000;
        result = RectVisible(hdc, &temp);
        if (result)
            info(FAIL, TEXT("Not visible Rect(%d %d %d %d) returned True"), temp.left, temp.top, temp.right, temp.bottom);
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Check Set Get pixel pairs
***
************************************************************************************/

//***********************************************************************************
void
GetSetPixelPairs(int testFunc)
{

    info(ECHO, TEXT("%s - Test Set/GetPixel Pair"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    COLORREF color,
            BlackColor = RGB(0, 0, 0),
            WhiteColor = RGB(255, 255, 255);
    BOOL    outBounds;
    DWORD   result;
    int     errors = 0;
    int     lzy = zy;

    // if we're doing a getdc(null), then the task bar is in bounds, so count it.
    if (DCFlag == EGDI_Default)
        lzy = GetDeviceCaps(hdc, VERTRES);

    // clear the screen
    myPatBlt(hdc, 0, 0, zx, lzy, WHITENESS);

    // pass twice: first  set color black: second set color white
    for (int pass = 0; pass < 2; pass++)
    {
        color = (pass == 0) ? BlackColor : WhiteColor;
        // from (-1000,-1000) to (1000,1000)
        for (int x = -1000; x < 1000; x += 113)
            for (int y = -1000; y < 1000; y += 113)
            {
                // are e inbounds of hdc?
                outBounds = (x < 0 || y < 0 || x >= zx || y >= lzy);

                // set the pixel
                mySetLastError();
                result = SetPixel(hdc, x, y, color);
                if (myGetLastError(NADA))
                {
                    info(ECHO, TEXT("SetPixel (%d %d): pass=%d"), x, y, pass);
                }
                if (errors < 10)
                {
                    if ((outBounds && result != (DWORD) - 1) || (!outBounds && result == (DWORD) - 1))
                    {
                        info(FAIL, TEXT("SetPixel(%d,%d,0x%x) returned 0x%lx  outside:%d"), x, y, color, result, outBounds);
                        errors++;
                    }
                    else if (!outBounds)
                    {
                        // first time: set black, expect return black; second time set white, expect return white
                        if ((pass == 0 && isWhitePixel(hdc, result, x, y, 1)) ||
                            (pass == 1 && !isWhitePixel(hdc, result, x, y, 1)))
                        {
                            info(FAIL, TEXT("GetPixel(%d,%d) expected:0x%x returned:0x%lx outside:%d"), x, y, color, result,
                                 outBounds);
                            errors++;
                        }
                    }
                }

                // get the same pixel
                mySetLastError();
                result = GetPixel(hdc, x, y);
                if (myGetLastError(NADA))
                {
                    info(ECHO, TEXT("SetPixel (%d %d %d)"), x, y, pass);
                }
                if (errors < 10)
                {
                    if ((outBounds && result != CLR_INVALID) || (!outBounds && result == CLR_INVALID))
                    {
                        info(FAIL, TEXT("GetPixel(%d,%d) expected:0x%x returned:0x%lx outside:%d"), x, y, color, result,
                             outBounds);
                        errors++;
                    }
                    else if (!outBounds)
                    {
                        // first time: set black, expect return black; second time set white, expect return white
                        if ((pass == 0 && isWhitePixel(hdc, result, x, y, 1)) ||
                            (pass == 1 && !isWhitePixel(hdc, result, x, y, 1)))
                        {
                            info(FAIL, TEXT("GetPixel(%d,%d) expected:0x%x returned:0x%lx outside:%d"), x, y, color, result,
                                 outBounds);
                            errors++;
                        }
                    }
                }

            }
        if (gSanityChecks)
            info(ECHO, TEXT("Using color:0x%x pass:%d"), color, pass);
    }

    if (errors >= 10)
        info(FAIL, TEXT("Exiting due to large number of errors"));

    // is the screen clear
    CheckAllWhite(hdc, 0, 0, 0, 0);
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Checks set/get pixel bounderies
***
************************************************************************************/

//***********************************************************************************
void
GetSetPixelBounderies(int testFunc)
{

    info(ECHO, TEXT("%s - Test Set/GetPixel Bounderies"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    COLORREF WhiteColorPal = PALETTERGB(255, 255, 255),
            WhiteColorRGB = RGB(255, 255, 255);
    DWORD   result;
    int     x = 0,
            y = 0,
            expected;
    int     lzy = zy;

    if (DCFlag == EGDI_Default)
        lzy = GetDeviceCaps(hdc, VERTRES);

    const int spot[8][3] = {    // x        y        expected
        {0, 0, 1},              // left     top      good
        {zx - 1, 0, 1},         // right    top      good
        {0, lzy - 1, 1},        // left     bottom   good
        {zx - 1, lzy - 1, 1},   // right    bottom   good
        {-1, -1, 0},            // left     top      bad
        {zx, 0, 0},             // right    top      bad
        {0, lzy, 0},            // left     bottom   bad
        {zx, lzy, 0},           // right    bottom   bad
    };

    for (int i = 0; i < 8; i++)
    {
        x = (int) spot[i][0];
        y = (int) spot[i][1];
        expected = (int) spot[i][2];

        // set a pixel on a boundery using the palrgb color
        mySetLastError();
        result = SetPixel(hdc, x, y, WhiteColorPal);
        myGetLastError(NADA);
        if (expected == 1 && !isWhitePixel(hdc, result, x, y, 1))
        {
            info(FAIL, TEXT("SetPixel(%d,%d,0x%x): returned 0x%x: expect WHITE"), x, y, WhiteColorPal, result);
        }
        else if (expected == 0 && result != -1)
            info(FAIL, TEXT("SetPixel(%d,%d,0x%x):returned 0x%x: expect -1"), x, y, WhiteColorPal, result);

        // set a pixel on a boundery using the pal color
        mySetLastError();
        result = SetPixel(hdc, x, y, WhiteColorRGB);
        myGetLastError(NADA);
        if (expected == 1 && !isWhitePixel(hdc, result, x, y, 1))
        {
            info(FAIL, TEXT("SetPixel(%d,%d,0x%x):returned 0x%x: expect WHITE"), x, y, WhiteColorRGB, result);
        }
        else if (expected == 0 && result != -1)
            info(FAIL, TEXT("SetPixel(%d,%d,0x%x):returned 0x%x: expect -1"), x, y, WhiteColorRGB, result);

        // try to get this pixel
        mySetLastError();
        result = GetPixel(hdc, x, y);
        myGetLastError(NADA);
        if (expected == 1 && !isWhitePixel(hdc, result, x, y, 1))
        {
            info(FAIL, TEXT("GetPixel(%d,%d):returned 0x%x: expect WHITE"), x, y, result);
        }
        else if (expected == 0 && result != -1)
            info(FAIL, TEXT("GetPixel(%d,%d):returned 0x%x: expect -1"), x, y, result);
    }

    // cover up last pixel
    mySetLastError();
    result = SetPixel(hdc, x, y, WhiteColorPal);
    myGetLastError(NADA);
    if (result != -1)
        info(FAIL, TEXT("SetPixel(NULL,%d,%d,0x%x):returned 0x%x: expect -1"), x, y, WhiteColorPal, result);
    // check if it was cleared
    mySetLastError();
    result = GetPixel(hdc, x, y);
    myGetLastError(NADA);
    if (result != -1)
        info(FAIL, TEXT("GetPixel(NULL,%d,%d):returned 0x%x: expect -1"), x, y, result);

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Simple Checks on WinLogo
***
************************************************************************************/

//***********************************************************************************
void
DrawSquareWinLogo(int testFunc, int quiet, int offset)
{

    if (!quiet)
        info(ECHO, TEXT("%s - Drawing winlogo with blocks"), funcName[testFunc]);

    TDC     hdc = myGetDC();

    for (int i = 0; i < blockSize; i++)
        blastRect(hdc, RESIZE(blocks[i][0] - offset), RESIZE(blocks[i][1]), RESIZE(blocks[i][2]), RESIZE(blocks[i][3]),
                  blocks[i][4], EPatBlt);

    if (!quiet)
        Sleep(10);
    myReleaseDC(hdc);
}

//***********************************************************************************
void
drawLogo(TDC hdc, int offset)
{
    int     i;

    for (i = 0; i < blockSize; i++)
        blastRect(hdc, RESIZE(blocks[i][0] - offset), RESIZE(blocks[i][1]), RESIZE(blocks[i][2]), RESIZE(blocks[i][3]),
                  blocks[i][4], EPatBlt);

    if (ISPRINTERDC(hdc))
    {
        return;
    }

    if (zy > 200)
    {                           // for some reason 320x200 screws up
        for (i = 0; i < curveSize; i++)
            myBitBlt(hdc, RESIZE(curves[i][0] - offset), RESIZE(curves[i][1]), RESIZE(curves[i][2]), RESIZE(curves[i][3]), hdc,
                     RESIZE(curves[i][4] - offset), RESIZE(curves[i][5]), SRCCOPY);
    }

    Sleep(10);
    AskMessage(TEXT("BitBlt"), TEXT("makeCurves"), TEXT("the Windows95 Logo"));
}

/***********************************************************************************
***
***   Simple DIB Test
***
************************************************************************************/

//***********************************************************************************
void
SimpleCreateDIBSectionTest(int testFunc, BOOL nullHDC)
{

    info(ECHO, TEXT("%s - Simple DIB create using diff sizes - passing %s DC"), funcName[testFunc],
         (nullHDC) ? TEXT("NULL") : TEXT("valid"));

    TDC     hdc = (nullHDC) ? NULL : myGetDC();
    UINT nType[] = {DIB_RGB_COLORS, DIB_PAL_COLORS};
    
    // screen metrics
    int     leap = zx / 10;
    BYTE   *lpBits = NULL;
    TBITMAP hBmp;
    BITMAP myBmp;
    float   maxSize;
    int     lastPass;

    for (int d = 0; d < countof(gBitDepths); d++)
    {
        for(int i = 0; i < countof(nType); i++)
        {
            if(gBitDepths[d] > 8 && nType[i] == DIB_PAL_COLORS)
                continue;

            maxSize = (gBitDepths[d] <= 8) ? (float) 1.5 : (float) 0.5;
            lastPass = (int) ((float) zx * (float) maxSize);
            for (int j = 0; j < lastPass; j += leap)
            {
                if (gSanityChecks)
                    info(DETAIL, TEXT("Creating DIB size:(%d x %d) depth:%d"), j, -j, gBitDepths[d]);
                hBmp = myCreateDIBSection(hdc, (VOID **) & lpBits, gBitDepths[d], j, j, nType[i]);
                // all checks done in myCreateDIBSection
                if (hBmp)
                {
                    GetObject(hBmp, sizeof (BITMAP), &myBmp);
                    if (myBmp.bmBitsPixel != gBitDepths[d] || myBmp.bmWidth != j || myBmp.bmHeight != j)
                        info(FAIL, TEXT("expected bpp:%d, W:%d, H:%d, got bpp:%d W:%d H:%d "), 
                                                gBitDepths[d], j, j, myBmp.bmBitsPixel, myBmp.bmBitsPixel, myBmp.bmWidth, myBmp.bmHeight);
                    DeleteObject(hBmp);
                }
            }
        }
    }
    if (hdc)
        myReleaseDC(hdc);
}

/***********************************************************************************
***
***   CreateDIBSection Bad iUsage
***
************************************************************************************/

//***********************************************************************************
void
CreateDIBSectionBadiUsage(int testFunc)
{

    info(ECHO, TEXT("%s - %d random CreateDIBSection iUsage"), funcName[testFunc], testCycles);
    BOOL bOldVerify = SetSurfVerify(FALSE);
    TDC     hdc = myGetDC();
    BYTE   *lpBits = NULL;
    TBITMAP hBmp;
    int     num;
    struct
    {
        BITMAPINFOHEADER bmih;
        RGBQUAD ct[4];
    }
    bmi;

    memset(&bmi.bmih, 0, sizeof (BITMAPINFOHEADER));
    bmi.bmih.biSize = sizeof (BITMAPINFOHEADER);
    bmi.bmih.biWidth = 10;
    bmi.bmih.biHeight = 10;
    bmi.bmih.biPlanes = 1;
    bmi.bmih.biBitCount = 2;
    bmi.bmih.biCompression = BI_RGB;
    setRGBQUAD(&bmi.ct[0], 0x0, 0x0, 0x0);
    setRGBQUAD(&bmi.ct[1], 0x55, 0x55, 0x55);
    setRGBQUAD(&bmi.ct[2], 0xaa, 0xaa, 0xaa);
    setRGBQUAD(&bmi.ct[3], 0xff, 0xff, 0xff);

    for (int i = 0; i < testCycles; i++)
    {
        do
        {
            num = badRandomNum();
        }
        while (num == DIB_PAL_COLORS || num == DIB_RGB_COLORS);
        mySetLastError();
        hBmp = CreateDIBSection(hdc, (LPBITMAPINFO) & bmi, num, (VOID **) & lpBits, NULL, NULL);
        myGetLastError(NADA);
        if (hBmp)
            info(FAIL, TEXT("CreateDIBSection didn't fail given bogus iUsage:%d"), num);
        else
            DeleteObject(hBmp);
    }
    myReleaseDC(hdc);
    SetSurfVerify(bOldVerify);
}

void
CreateDIBSectionPalBadUsage(int testFunc)
{
    info(ECHO, TEXT("CreateDIBSectionPalBadUsage"));

    BOOL bOldVerify = SetSurfVerify(FALSE);
    TDC     hdc = myGetDC();
    BYTE   *lpBits = NULL;
    TBITMAP hBmp;
    BITMAP bmpData;

    NameValPair biCompression[] = {
                                                        NAMEVALENTRY(BI_RGB), 
                                                        NAMEVALENTRY(BI_BITFIELDS)
                                                      };
    unsigned short biClrsUsed[] = {0, 1, 2, 4, 8, 16, 32, 64, 128, 255, 256, 257, 512 };
    NameValPair Options[] = {
                                            NAMEVALENTRY(DIB_PAL_COLORS), 
                                            NAMEVALENTRY(DIB_RGB_COLORS) 
                                           };

    struct
    {
        BITMAPINFOHEADER bmih;
        RGBQUAD ct[512];
    }
    bmi;

    for (int i = 0; i < countof(bmi.ct); i++)
    {
        setRGBQUAD(&bmi.ct[i], (BYTE) rand() % 0xFF, (BYTE) rand() % 0xFF, (BYTE) rand() % 0xFF);
    }

    memset(&bmi.bmih, 0, sizeof (BITMAPINFOHEADER));
    bmi.bmih.biSize = sizeof (BITMAPINFOHEADER);
    bmi.bmih.biWidth = 10;
    bmi.bmih.biHeight = 10;

    for(unsigned short planes = 0; planes < 3; planes ++)
        for(unsigned short bpp = 0; bpp <= 33; bpp++)
            for(int comp = 0; comp < countof(biCompression); comp++)
                for(int clrs = 0; clrs < countof(biClrsUsed); clrs++)
                    for(int opt = 0; opt < countof(Options); opt++)
                    {
                        bmi.bmih.biPlanes = planes;
                        bmi.bmih.biBitCount = bpp;
                        bmi.bmih.biCompression = biCompression[comp].dwVal;
                        bmi.bmih.biClrUsed = biClrsUsed[clrs];

                        hBmp = CreateDIBSection(hdc, (LPBITMAPINFO) & bmi, Options[opt].dwVal, (VOID **) & lpBits, NULL, NULL);

                        if(hBmp)
                        {
                        // by design, we differ from the desktop.  no point in flagging the differences as failures.
#ifdef UNDER_CE
                            if(!(planes == 1 && 
                                    (bmi.bmih.biClrUsed <= 256 || bpp >= 16 || (bpp < 16 && biCompression[comp].dwVal == BI_RGB && Options[opt].dwVal == DIB_PAL_COLORS)) && 
                                    (((bpp == 1 || bpp == 2 || bpp == 4 || bpp == 8) && biCompression[comp].dwVal == BI_RGB) ||((bpp == 16 || bpp == 24 || bpp == 32) && Options[opt].dwVal == DIB_RGB_COLORS))))
                                info(FAIL, TEXT("creation of a surface succeed when expected to fail.  planes:%d, bpp:%d, compression:%s, ClrsUsed:%d, Options:%s"), planes, bpp, biCompression[comp].szName, biClrsUsed[clrs], Options[opt].szName);
#endif

                            GetObject(hBmp, sizeof(BITMAP), &bmpData);
                            if(bmpData.bmPlanes != planes || bmpData.bmType != 0 || bmpData.bmWidth != 10 || bmpData.bmHeight != 10 || bmpData.bmBitsPixel != bpp || bmpData.bmBits == NULL)
                                 info(FAIL, TEXT("Failed to create the correct DIB.  expect - planes:%d type:0 width:10 height: 10 bpp:%d bmBits != NULL.  Got planes:%d type:%d width:%d height:%d bpp:%d bmBits:%d"), planes, bpp, bmpData.bmPlanes, bmpData.bmType, bmpData.bmWidth, bmpData.bmHeight, bmpData.bmBitsPixel, bmpData.bmBits);
                            
                            DeleteObject(hBmp);
                        }
#ifdef UNDER_CE
                        else if((planes == 1 && 
                                    (bmi.bmih.biClrUsed <= 256 || bpp >= 16 || (bpp < 16 && biCompression[comp].dwVal == BI_RGB && Options[opt].dwVal == DIB_PAL_COLORS)) && 
                                    (((bpp == 1 || bpp == 2 || bpp == 4 || bpp == 8) && biCompression[comp].dwVal == BI_RGB) ||((bpp == 16 || bpp == 24 || bpp == 32) && Options[opt].dwVal == DIB_RGB_COLORS))))
                            info(FAIL, TEXT("Failed creation of a surface expected to succeed.  planes:%d, bpp:%d, compression:%s, ClrsUsed:%d, Options:%s"), planes, bpp, biCompression[comp].szName, biClrsUsed[clrs], Options[opt].szName);
#endif
                    }

    myReleaseDC(hdc);
    SetSurfVerify(bOldVerify);
}

/***********************************************************************************
***
***   Simple CreateCompatibleBitmap Test
***
************************************************************************************/

//***********************************************************************************
void
SimpleCreatecompatibleBitmapTest(int testFunc, int nullHDC)
{

    info(ECHO, TEXT("%s - Simple CreateCompatibleBitmap using diff sizes - passing %s DC"), funcName[testFunc],
         (nullHDC) ? TEXT("NULL") : TEXT("valid"));

    TDC     hdc = (nullHDC) ? NULL : myGetDC();

    // screen metrics
    TBITMAP hBmp;

    for (int j = 0; j < 520; j += 40)
    {
        mySetLastError();
        hBmp = CreateCompatibleBitmap(hdc, j, j);
        myGetLastError(NADA);
        // REVIEW myGetLastError(nullHDC)?ERROR_INVALID_HANDLE:NADA);

        if (j > 0 && !hBmp && !nullHDC)
            info(FAIL, TEXT("hBmp(%dx%d) failed returned NULL"), j, j);
        else if (hBmp && nullHDC)
        {
            info(FAIL, TEXT("hBmp should have failed %dx%d"), j, j);
            DeleteObject(hBmp);
        }
        else
            DeleteObject(hBmp);
    }
    if (hdc)
        myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Odd CreateCompatibleBitmap Test
***
************************************************************************************/

//***********************************************************************************
void
OddCreatecompatibleBitmapTest(int testFunc)
{

    info(ECHO, TEXT("%s - Odd CreateCompatibleBitmap"), funcName[testFunc]);

    TDC     hdc[3];
    int     bpp;
    TBITMAP hBmp,
            hBmpTemp;
    BITMAP  myBmp;


    bpp = myGetBitDepth();
    
    hdc[0] = myGetDC();
    hdc[1] = CreateCompatibleDC(hdc[0]);
    hdc[2] = CreateCompatibleDC(hdc[1]);

    PatBlt(hdc[0], 0, 0, zx, zy, WHITENESS);
    PatBlt(hdc[1], 0, 0, zx, zy, WHITENESS);
    PatBlt(hdc[2], 0, 0, zx, zy, WHITENESS);

    info(DETAIL, TEXT("creating a bitmap compatible to the test DC."));
    hBmp = CreateCompatibleBitmap(hdc[0], 40, 40);
    GetObject(hBmp, sizeof (BITMAP), &myBmp);
    if (!hBmp)
        info(FAIL, TEXT("CreateBitmap failed.  gle:%d"), GetLastError());
    // a bitmap compatible to the dc retrieved from myGetDC() should be it's bit depth and size.
    else if (myBmp.bmBitsPixel != bpp || myBmp.bmWidth != 40 || myBmp.bmHeight != 40)
        info(FAIL, TEXT("expected bpp:%d, W:%d, H:%d, got bpp:%d W:%d H:%d "), bpp, 40, 40, myBmp.bmBitsPixel, myBmp.bmWidth, myBmp.bmHeight);
    DeleteObject(hBmp);

    info(DETAIL, TEXT("Creating a bitmap compatible to a dc compatible to the test dc."));
    hBmp = CreateCompatibleBitmap(hdc[1], 40, 40);
    GetObject(hBmp, sizeof (BITMAP), &myBmp);
    if (!hBmp)
        info(FAIL, TEXT("CreateBitmap failed.  gle:%d"), GetLastError());
    // the default bitmap of a compatible DC is a 1x1 monochrome
    else if (myBmp.bmBitsPixel != 1 || myBmp.bmWidth != 40 || myBmp.bmHeight != 40)
        info(FAIL, TEXT("expected bpp:1, w:1, d:1, got  bpp:%d W:%d H:%d "), myBmp.bmBitsPixel, myBmp.bmWidth, myBmp.bmHeight);
    DeleteObject(hBmp);

    info(DETAIL, TEXT("Creating a bitmap compatible to a dc compatible to a compatible dc."));
    hBmp = CreateCompatibleBitmap(hdc[2], 40, 40);
    GetObject(hBmp, sizeof (BITMAP), &myBmp);
    if (!hBmp)
        info(FAIL, TEXT("CreateBitmap failed.  gle:%d"), GetLastError());
    // the default bitmap of a compatible DC is a 1x1 monochrome
    else if (myBmp.bmBitsPixel != 1 || myBmp.bmWidth != 40 || myBmp.bmHeight != 40)
        info(FAIL, TEXT("expected bpp:1, w:1, d:1, got  bpp:%d W:%d H:%d "), myBmp.bmBitsPixel, myBmp.bmWidth, myBmp.bmHeight);
    DeleteObject(hBmp);

    info(DETAIL, TEXT("Creating a bitmap compatible to the test dc with a width of 0"));
    hBmp = CreateCompatibleBitmap(hdc[0], 0, 40);
    GetObject(hBmp, sizeof (BITMAP), &myBmp);
    if (!hBmp)
        info(FAIL, TEXT("CreateBitmap failed.  gle:%d"), GetLastError());
    // the default bitmap of a compatible DC is a 1x1 monochrome
    else if (myBmp.bmBitsPixel != 1 || myBmp.bmWidth != 1 || myBmp.bmHeight != 1)
        info(FAIL, TEXT("expected bpp:1, w:1, d:1, got  bpp:%d W:%d H:%d "), myBmp.bmBitsPixel, myBmp.bmWidth, myBmp.bmHeight);    DeleteObject(hBmp);
    DeleteObject(hBmp);

    info(DETAIL, TEXT("Creating a bitmap compatible to the test dc with a height of 0"));
    hBmp = CreateCompatibleBitmap(hdc[0], 40, 0);
    GetObject(hBmp, sizeof (BITMAP), &myBmp);
    if (!hBmp)
        info(FAIL, TEXT("CreateBitmap failed.  gle:%d"), GetLastError());
    // the default bitmap of a compatible DC is a 1x1 monochrome
    else if (myBmp.bmBitsPixel != 1 || myBmp.bmWidth != 1 || myBmp.bmHeight != 1)
        info(FAIL, TEXT("expected bpp:1, w:1, d:1, got  bpp:%d W:%d H:%d "), myBmp.bmBitsPixel, myBmp.bmWidth, myBmp.bmHeight);
    DeleteObject(hBmp);

    info(DETAIL, TEXT("Creating a bitmap compatible to the test dc, with a width of 40, height of 40"));
    hBmp = CreateCompatibleBitmap(hdc[0], 40, 40);
    hBmpTemp = (TBITMAP) SelectObject(hdc[0], hBmp);
    if (isMemDC())
    {
        if (NULL == hBmpTemp)
            info(FAIL, TEXT("SelectObject failed when not expected"));
        hBmpTemp = (TBITMAP) SelectObject(hdc[0], hBmpTemp);
        passCheck(hBmpTemp == hBmp, TRUE, TEXT("Incorrect bitmap returned"));
    }
    else
    {
        if (NULL != hBmpTemp)
            info(FAIL, TEXT("Expected NULL for SelectObject on a primary DC"));
    }
    DeleteObject(hBmp);

    myReleaseDC(hdc[0]);
    DeleteDC(hdc[1]);
    DeleteDC(hdc[2]);
}

/***********************************************************************************
***
***   Passes Null as every possible parameter
***
************************************************************************************/

//***********************************************************************************
void
passNull2Draw(int testFunc)
{

    info(ECHO, TEXT("%s - Passing NULL"), funcName[testFunc]);

    TDC     aDC = myGetDC();
    TDC     aCompDC = CreateCompatibleDC(aDC);
    TBITMAP hBmp;
    RECT    test = { 10, 10, 50, 50 };
    int     result = 1;
    POINT   point,
            points[20],
           *p = points;
    RECT    r;
    HBRUSH  hBrush;
    BYTE   *lpBits = NULL;
    TBITMAP aBmp = NULL;
    TBITMAP abmpStock = NULL;
    RGBQUAD rgbqTmp;
    TRIVERTEX        vert[2] = {0};
    GRADIENT_RECT    gRect[1] = {0};
    LPBITMAPINFO pBmi;

    struct
    {
        BITMAPINFOHEADER bmih;
        RGBQUAD ct[4];
    }
    bmi;

    memset(&bmi.bmih, 0, sizeof (BITMAPINFOHEADER));
    bmi.bmih.biSize = sizeof (BITMAPINFOHEADER);
    bmi.bmih.biWidth = 10;
    bmi.bmih.biHeight = -10;
    bmi.bmih.biPlanes = 1;
#ifdef UNDER_CE
    bmi.bmih.biBitCount = 2;
#else
    bmi.bmih.biBitCount = 1;
#endif
    bmi.bmih.biCompression = BI_RGB;
    setRGBQUAD(&bmi.ct[0], 0x0, 0x0, 0x0);
    setRGBQUAD(&bmi.ct[1], 0x55, 0x55, 0x55);
    setRGBQUAD(&bmi.ct[2], 0xaa, 0xaa, 0xaa);
    setRGBQUAD(&bmi.ct[3], 0xff, 0xff, 0xff);
    pBmi = (LPBITMAPINFO) &bmi;
    
    hBmp = CreateDIBSection(aDC, (LPBITMAPINFO) & bmi, DIB_RGB_COLORS, (VOID **) & lpBits, NULL, NULL);

    mySetLastError();
    switch (testFunc)
    {
        case ERectangle:
            result = Rectangle((TDC) NULL, test.left, test.top, test.right, test.bottom);
            passCheck(result, 0, TEXT("NULL,10,10,50,50"));
            break;
        case EEllipse:
            result = Ellipse((TDC) NULL, test.left, test.top, test.right, test.bottom);
            passCheck(result, 0, TEXT("NULL,10,10,50,50"));
            break;
        case ERoundRect:
            result = RoundRect((TDC) NULL, test.left, test.top, test.right, test.bottom, 10, 10);
            passCheck(result, 0, TEXT("NULL,10,10,50,50,10,10"));
            break;
        case ERectVisible:
            result = RectVisible((TDC) NULL, &test);
            passCheck(result, -1, TEXT("NULL,&test"));
            result = RectVisible(aDC, NULL);
            passCheck(result, FALSE, TEXT("aDC,NULL"));
            break;
            case EBitBlt:
            result = BitBlt((TDC) NULL, test.left, test.top, test.right, test.bottom, (TDC) NULL, 0, 0, SRCCOPY);
            passCheck(result, 0, TEXT("NULL,test.left,test.top,test.right,test.bottom,NULL,0,0,SRCCOPY"));
            result = BitBlt(aDC, test.left, test.top, test.right, test.bottom, (TDC) NULL, 0, 0, SRCCOPY);
            passCheck(result, 0, TEXT("aDC,test.left,test.top,test.right,test.bottom,NULL,0,0,SRCCOPY"));
            result = BitBlt((TDC) NULL, test.left, test.top, test.right, test.bottom, (HDC) 0xBAD, 0, 0, SRCCOPY);
            passCheck(result, 0, TEXT("NULL,test.left,test.top,test.right,test.bottom,NULL,0,0,SRCCOPY"));
            result = BitBlt(aDC, test.left, test.top, test.right, test.bottom, (HDC) 0xBAD, 0, 0, SRCCOPY);
            passCheck(result, 0, TEXT("aDC,test.left,test.top,test.right,test.bottom,NULL,0,0,SRCCOPY"));
            
            if (!g_fPrinterDC)
            {
                result = BitBlt((TDC) NULL, test.left, test.top, test.right, test.bottom, aDC, 0, 0, SRCCOPY);
                passCheck(result, 0, TEXT("NULL,test.left,test.top,test.right,test.bottom,aDC,0,0,SRCCOPY"));

                result = BitBlt((HDC) 0xBAD, test.left, test.top, test.right, test.bottom, aDC, 0, 0, SRCCOPY);
                passCheck(result, 0, TEXT("NULL,test.left,test.top,test.right,test.bottom,aDC,0,0,SRCCOPY"));
                
#ifdef UNDER_CE
                // invalid bitblt rop, we differ from NT, which seems to just accept this
                result = BitBlt(aDC, test.left, test.top, test.right, test.bottom, aDC, 0, 0, MAKEROP4(PATCOPY, PATINVERT));
                passCheck(result, 0, TEXT("aDC,test.left,test.top,test.right,test.bottom,aDC,0,0,MAKEROP4(PATCOPY, PATINVERT)"));
#endif
            }
            break;
        case ECreateDIBSection:
            result = (int) CreateDIBSection((HDC) 0xBAD, (LPBITMAPINFO) &bmi, DIB_RGB_COLORS, NULL, NULL, 0);
            passCheck(result,0,TEXT("(HDC) 0xBAD, (LPBITMAPINFO) &bmi, DIB_RGB_COLORS, NULL, NULL, 0"));
            result = (int) CreateDIBSection(aDC, (LPBITMAPINFO) NULL, DIB_RGB_COLORS, NULL, NULL, 0);
            passCheck(result,0,TEXT("aDC, (LPBITMAPINFO) NULL, DIB_RGB_COLORS, NULL, NULL, 0"));
            break;
        case EStretchBlt:
            result = StretchBlt((TDC) NULL, test.left, test.top, test.right, test.bottom, (TDC) NULL, 0, 0, test.right, test.bottom, SRCCOPY);
            passCheck(result, 0, TEXT("NULL,test.left,test.top,test.right,test.bottom,NULL,0,0,test.right,test.bottom,SRCCOPY"));
            result = StretchBlt(aDC, test.left, test.top, test.right, test.bottom, (TDC) NULL, 0, 0, test.right, test.bottom, SRCCOPY);
            passCheck(result, 0, TEXT("aDC,test.left,test.top,test.right,test.bottom,NULL,0,0,test.right,test.bottom,SRCCOPY"));
            if (!g_fPrinterDC)
            {
                result = StretchBlt((TDC) NULL, test.left, test.top, test.right, test.bottom, aDC, 0, 0, test.right, test.bottom, SRCCOPY);
                passCheck(result, 0, TEXT("NULL,test.left,test.top,test.right,test.bottom,aDC,0,0,test.right,test.bottom,SRCCOPY"));
                result = StretchBlt(aDC, test.left, test.top, test.right, test.bottom, aDC, 0, 0, test.right, test.bottom, MAKEROP4(PATCOPY, PATINVERT));
                passCheck(result, 0, TEXT("aDC,test.left,test.top,test.right,test.bottom,aDC,0,0,test.right,test.bottom,MAKEROP4(PATCOPY, PATINVERT)"));
            }
            break;
        case ETransparentImage:
            result = TransparentBlt((TDC) NULL, test.left, test.top, test.right, test.bottom, (TDC) NULL, 0, 0, test.right, test.bottom, RGB(0,0,0));
            passCheck(result, 0, TEXT("NULL,test.left,test.top,test.right,test.bottom,NULL,0,0,test.right,test.bottom,RGB(0,0,0)"));
            result = TransparentBlt(aDC, test.left, test.top, test.right, test.bottom, (TDC) NULL, 0, 0, test.right, test.bottom, RGB(0,0,0));
            passCheck(result, 0, TEXT("aDC,test.left,test.top,test.right,test.bottom,NULL,0,0,test.right,test.bottom,RGB(0,0,0)"));
            if (!g_fPrinterDC)
            {
                result = TransparentBlt((TDC) NULL, test.left, test.top, test.right, test.bottom, aDC, 0, 0, test.right, test.bottom, RGB(0,0,0));
                passCheck(result, 0, TEXT("NULL,test.left,test.top,test.right,test.bottom,aDC,0,0,test.right,test.bottom,RGB(0,0,0)"));
            }
            break;            
        case EStretchDIBits:
            
            result = StretchDIBits((TDC) NULL, test.left , test.top, test.right, test.bottom,
                                        test.left, test.top, test.right, test.bottom, NULL, NULL, DIB_RGB_COLORS, SRCCOPY);
            passCheck(result, 0, TEXT("NULL, test.left , test.top, test.right, test.bottom,test.left, test.top, test.right, test.bottom, NULL, NULL, DIB_RGB_COLORS, SRCCOPY"));
            result = StretchDIBits(aDC, test.left , test.top, test.right, test.bottom,
                                        test.left, test.top, test.right, test.bottom, NULL, NULL, DIB_RGB_COLORS, SRCCOPY);
            passCheck(result, 0, TEXT("aDC, test.left , test.top, test.right, test.bottom,test.left, test.top, test.right, test.bottom, NULL, NULL, DIB_RGB_COLORS, SRCCOPY"));

            // The desktop says this succeeds, but a ROP4 passed in makes no sense, and the only intelligent thing for StretchDIBits to do is to fail this.
            if(hBmp)
            {
                result = StretchDIBits(aDC, test.left , test.top, test.right, test.bottom,
                                        test.left, test.top, test.right, test.bottom, lpBits, pBmi, DIB_RGB_COLORS, MAKEROP4(PATCOPY, PATINVERT));
                #ifdef UNDER_CE
                passCheck(result, 0, TEXT("aDC, test.left , test.top, test.right, test.bottom,test.left, test.top, test.right, test.bottom, lpBits, pBmi, DIB_RGB_COLORS, MAKEROP4(PATCOPY, PATINVERT)"));
                #else
                passCheck(result, 1, TEXT("aDC, test.left , test.top, test.right, test.bottom,test.left, test.top, test.right, test.bottom, lpBits, pBmi, DIB_RGB_COLORS, MAKEROP4(PATCOPY, PATINVERT)"));
                #endif
            }
            else info(FAIL, TEXT("Failed to create bitmap for StretchDIBits bad parameter checking"));

            break;
        case ESetDIBitsToDevice:
            result = SetDIBitsToDevice((TDC) NULL, test.left, test.right, test.right, test.top, test.left, test.bottom, 0, zy, NULL, NULL, DIB_RGB_COLORS);
            passCheck(result, 0, TEXT("(TDC) NULL, test.left, test.y, test.right, test.height, test.left, test.bottom, 0, zy, NULL, NULL, DIB_RGB_COLORS"));
            result = SetDIBitsToDevice(aDC, test.left, test.left, test.right, test.top, test.left, test.bottom, 0, zy, NULL, NULL, DIB_RGB_COLORS);
            passCheck(result, 0, TEXT("aDC, test.left, test.y, test.right, test.height, test.left, test.bottom, 0, zy, NULL, NULL, DIB_RGB_COLORS"));
            break;
        case EMaskBlt:
            result = MaskBlt((HDC) NULL, test.left, test.top, test.right, test.bottom, (HDC) NULL, 0, 0, (HBITMAP) NULL, 0, 0, MAKEROP4(BLACKNESS, SRCCOPY));
            passCheck(result, 0, TEXT("NULL,test.left,test.top,test.right,test.bottom,NULL,0,0,NULL,0,0,MAKEROP4(BLACKNESS, SRCCOPY)"));
            result = MaskBlt(aDC, test.left, test.top, test.right, test.bottom, (HDC) NULL, 0, 0, (HBITMAP) NULL, 0, 0, MAKEROP4(BLACKNESS, SRCCOPY));
            passCheck(result, 0, TEXT("aDC,test.left,test.top,test.right,test.bottom,NULL,0,0,NULL,0,0,MAKEROP4(BLACKNESS, SRCCOPY)"));
            result = MaskBlt((HDC) NULL, test.left, test.top, test.right, test.bottom, aDC, 0, 0, (HBITMAP) NULL, 0, 0, MAKEROP4(BLACKNESS, SRCCOPY));
            passCheck(result, 0, TEXT("aDC,test.left,test.top,test.right,test.bottom,NULL,0,0,NULL,0,0,MAKEROP4(BLACKNESS, SRCCOPY)"));  
            result = MaskBlt((HDC) 0xBAD, test.left, test.top, test.right, test.bottom, (HDC) 0xBAD, 0, 0, (HBITMAP) 0xBAD, 0, 0, MAKEROP4(BLACKNESS, SRCCOPY));
            passCheck(result, 0, TEXT("0xBAD,test.left,test.top,test.right,test.bottom,0xBAD,0,0,0xBAD,0,0,MAKEROP4(BLACKNESS, SRCCOPY)"));
            result = MaskBlt(aDC, test.left, test.top, test.right, test.bottom, (HDC) 0xBAD, 0, 0, (HBITMAP) 0xBAD, 0, 0, MAKEROP4(BLACKNESS, SRCCOPY));
            passCheck(result, 0, TEXT("aDC,test.left,test.top,test.right,test.bottom,NULL,0,0,NULL,0,0,MAKEROP4(BLACKNESS, SRCCOPY)"));
            result = MaskBlt((HDC) 0xBAD, test.left, test.top, test.right, test.bottom, aDC, 0, 0, (HBITMAP) 0xBAD, 0, 0, MAKEROP4(BLACKNESS, SRCCOPY));
            passCheck(result, 0, TEXT("aDC,test.left,test.top,test.right,test.bottom,NULL,0,0,NULL,0,0,MAKEROP4(BLACKNESS, SRCCOPY)"));
            result = MaskBlt(aDC, test.left, test.top, test.right, test.bottom, aDC, 0, 0, (HBITMAP) 0xBAD, 0, 0, MAKEROP4(BLACKNESS, SRCCOPY));
            passCheck(result, 0, TEXT("aDC,test.left,test.top,test.right,test.bottom,NULL,0,0,NULL,0,0,MAKEROP4(BLACKNESS, SRCCOPY)"));
            break;
        case EGetPixel:
            result = GetPixel((HDC) NULL, test.left, test.left);
            passCheck(result, -1, TEXT("NULL,test.left, test.left"));
            result = GetPixel((HDC) 0xBAD, test.left, test.left);
            passCheck(result, -1, TEXT("NULL,test.left, test.left"));
            break;
        case EPatBlt:
            result = PatBlt((TDC) NULL, test.left, test.top, test.right, test.bottom, BLACKNESS);
            passCheck(result, 0, TEXT("NULL,test.left,test.top,test.right,test.bottom,BLACKNESS"));
            result = PatBlt((TDC) NULL, 50, 50, 100, 100, PATINVERT);
            passCheck(result, 0, TEXT("NULL, 50, 50,  100,  100, PATINVERT"));
            result = PatBlt((HDC) 0x777, 50, 50, 100, 100, PATINVERT);
            passCheck(result, 0, TEXT("50, 50,  100,  100, PATINVERT"));
#ifdef UNDER_CE
            // we differ from XP.  A rop4 doesn't make sense in PatBlt.
            result = PatBlt(aDC, 50, 50, 100, 100, MAKEROP4(PATCOPY, PATINVERT));
            passCheck(result, 0, TEXT("50, 50,  100,  100, MAKEROP4(PATCOPY, PATINVERT)"));
#endif
            break;
        case ESetPixel:
            result = SetPixel((TDC) NULL, test.left, test.left, RGB(0,0,0));
            passCheck(result, -1, TEXT("NULL,test.left, test.left"));
            break;
        case EClientToScreen:
            result   = ClientToScreen(NULL,NULL);
            passCheck(result,0,TEXT("NULL,NULL"));
            result = ClientToScreen(NULL, &point);
            passCheck(result, 0, TEXT("NULL,&point"));
            break;
        case EScreenToClient:
            result   = ScreenToClient(NULL,NULL);
            passCheck(result,0,TEXT("NULL,NULL"));
            result = ScreenToClient(NULL, &point);
            passCheck(result, 0, TEXT("NULL,&point"));
            break;
        case EPolygon:
            mySetLastError();
            result = Polygon((TDC) NULL, p, 20);
            if (myGetLastError(0x6))
                info(ECHO, TEXT("NULL,p,20"));
            passCheck(result, 0, TEXT("NULL,p,20"));
            mySetLastError();
            result = Polygon(aDC, NULL, 20);
            if (myGetLastError(NADA))
                info(ECHO, TEXT("aDC,NULL,20"));
            passCheck(result, 0, TEXT("hdc,NULL,20"));
            mySetLastError();
            result = Polygon(aDC, p, -1);
            if (myGetLastError(NADA))
                info(ECHO, TEXT("aDC,p,-1"));
            passCheck(result, 0, TEXT("hdc,p,-1"));
            break;
        case EPolyline:
            mySetLastError();
            result = Polyline((TDC) NULL, p, 20);
            if (myGetLastError(0x6))
                info(ECHO, TEXT("NULL,p,20"));
            passCheck(result, 0, TEXT("NULL,p,20"));
            mySetLastError();
            result = Polyline(aDC, NULL, 20);
            if (myGetLastError(NADA))
                info(ECHO, TEXT("aDC,NULL,20"));
            passCheck(result, 0, TEXT("hdc,NULL,20"));
            mySetLastError();
            result = Polyline(aDC, p, -1);
            if (myGetLastError(NADA))
                info(ECHO, TEXT("aDC,p,-1"));
            passCheck(result, 0, TEXT("hdc,p,-1"));
            break;
        case EFillRect:
            hBrush = (HBRUSH) GetStockObject(BLACK_BRUSH);
            result = FillRect((TDC) NULL, &r, hBrush);
            passCheck(result, 0, TEXT("NULL,&r,hBrush"));
            result = FillRect((TDC) NULL, &r, NULL);
            passCheck(result, 0, TEXT("NULL,&r,NULL"));
            result = FillRect(aDC, &test, NULL);
            passCheck(result, 1, TEXT("aDC,&test,NULL"));
            break;
        case EDrawEdge:
            #ifdef UNDER_CE
            result = DrawEdge((TDC) NULL, NULL, 0, 0);
            passCheck(result, 0, TEXT("NULL, NULL, 0, 0"));
            result = DrawEdge(aDC, NULL, 0, 0);
            passCheck(result, 0, TEXT("NULL, NULL, 0, 0"));
            result = DrawEdge((TDC) NULL, &test, 0, 0);
            passCheck(result, 0, TEXT("NULL, NULL, 0, 0"));
            #endif
            break;
        case EMoveToEx:
            result = MoveToEx((TDC) NULL, 1, 1, NULL);
            passCheck(result, 0, TEXT("NULL, 1, 1, NULL"));
            break;
        case ELineTo:
            result = LineTo((TDC) NULL, 1, 1);
            passCheck(result, 0, TEXT("NULL, 1, 1"));
            break;
        case EGetDIBColorTable:
            result = GetDIBColorTable((TDC) NULL, 0, 1, NULL);
            passCheck(result, 0, TEXT("NULL, 0, 1, NULL"));
            result = GetDIBColorTable((TDC) NULL, 0, 1, &rgbqTmp);
            passCheck(result, 0, TEXT("NULL, 0, 1, rgbqTmp"));
            result = GetDIBColorTable(aDC, 0, 1, NULL);
            passCheck(result, 0, TEXT("aDC, 0, 1, NULL"));

            // if aDC is less than 16bpp, then this may succeed
            if(myGetBitDepth() > 8)
            {
                result = GetDIBColorTable(aDC, 0, 1, &rgbqTmp);
                passCheck(result, 0, TEXT("aDC, 0, 1, rgbqTmp"));
            }
            result = GetDIBColorTable((HDC) 0xBAD, 0, 1, &rgbqTmp);
            passCheck(result, 0, TEXT("0xBAD, 0, 1, rgbqTmp"));

            aBmp = myCreateDIBSection(aDC, (VOID **) & lpBits, 1, zx/2, zy, DIB_RGB_COLORS);

            if(aBmp)
            {
                abmpStock = (TBITMAP) SelectObject(aCompDC, aBmp);

                result = GetDIBColorTable(aCompDC, 0, 1, NULL);
                passCheck(result, 0, TEXT("aCompDC, 0, 1, NULL"));
                result = GetDIBColorTable(aCompDC, 2, 1, &rgbqTmp);
                passCheck(result, 0, TEXT("aCompDC, 2, 1, rgbqTmp"));

                SelectObject(aCompDC, abmpStock);
                DeleteObject(aBmp);
            }
            else info(DETAIL, TEXT("Failed to create a DIB for bad param testing, portion of the test skipped"));

            aBmp = myCreateDIBSection(aDC, (VOID **) & lpBits, 16, zx/2, zy, DIB_PAL_COLORS);

            if(aBmp)
            {
                abmpStock = (TBITMAP) SelectObject(aCompDC, aBmp);

                result = GetDIBColorTable(aCompDC, 2, 1, &rgbqTmp);
                passCheck(result, 0, TEXT("aCompDC, 2, 1, rgbqTmp (16bpp DIB_PAL_COLORS)"));

                SelectObject(aCompDC, abmpStock);
                DeleteObject(aBmp);
            }
            else info(DETAIL, TEXT("Failed to create a DIB for bad param testing, portion of the test skipped"));
            break;
        case ESetDIBColorTable:
            result = SetDIBColorTable((TDC) NULL, 0, 1, NULL);
            passCheck(result, 0, TEXT("NULL, 0, 1, NULL"));
            result = SetDIBColorTable(aDC, 0, 1, NULL);
            passCheck(result, 0, TEXT("aDC, 0, 1, NULL"));
            result = SetDIBColorTable(aDC, 0, 1, &rgbqTmp);
            passCheck(result, 0, TEXT("aDC, 0, 1, rgbqTmp"));
            result = SetDIBColorTable((HDC) 0xBAD, 0, 1, &rgbqTmp);
            passCheck(result, 0, TEXT("0xBAD, 0, 1, rgbqTmp"));
#ifdef UNDER_CE // causes a stop error on XP, reported 12/3/02 to be fixed in XP SP2
            aBmp = myCreateDIBSection(aDC, (VOID **) & lpBits, 1, zx/2, zy, DIB_RGB_COLORS);

            if(aBmp)
            {
                abmpStock = (TBITMAP) SelectObject(aCompDC, aBmp);

                result = SetDIBColorTable(aCompDC, 0, 1, NULL);
                passCheck(result, 0, TEXT("aCompDC, 0, 1, NULL"));
                result = SetDIBColorTable(aCompDC, 2, 1, &rgbqTmp);
                passCheck(result, 0, TEXT("aCompDC, 2, 1, rgbqTmp"));

                SelectObject(aCompDC, abmpStock);
                DeleteObject(aBmp);
            }
            else info(DETAIL, TEXT("Failed to create a DIB for bad param testing, portion of the test skipped"));
#endif
            break;
        case EGradientFill:

            gRect[0].UpperLeft = 0;
            gRect[0].LowerRight = 1;
                        
            vert [0] .x      = zx/4;
            vert [0] .y      = zy/4;
            vert [0] .Red    = 0xFF00;
            vert [0] .Green  = 0x0000;
            vert [0] .Blue   = 0x0000;
            vert [0] .Alpha  = 0x0000;
                
            vert [1] .x      = 3*zx/4;
            vert [1] .y      = 3*zy/4;
            vert [1] .Red    = 0xFF00;
            vert [1] .Green  = 0x0000;
            vert [1] .Blue   = 0x0000;
            vert [1] .Alpha  = 0x0000;
    
            result = gpfnGradientFill((TDC) NULL, NULL, 0, NULL, 0, NULL);
            passCheck(result, 0, TEXT("NULL, NULL, 0, NULL, 0, NULL"));
            result = gpfnGradientFill((TDC) NULL, NULL, 0, NULL, 0, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("NULL, NULL, 0, NULL, 0, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill((TDC) NULL, NULL, 0, NULL, 1, NULL);
            passCheck(result, 0, TEXT("NULL, NULL, 0, NULL, 1, NULL"));
            result = gpfnGradientFill((TDC) NULL, NULL, 0, NULL, 1, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("NULL, NULL, 0, NULL, 1, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill((TDC) NULL, NULL, 0, gRect, 0, NULL);
            passCheck(result, 0, TEXT("NULL, NULL, 0, gRect, 0, NULL"));
            result = gpfnGradientFill((TDC) NULL, NULL, 0, gRect, 0, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("NULL, NULL, 0, gRect, 0, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill((TDC) NULL, NULL, 0, gRect, 1, NULL);
            passCheck(result, 0, TEXT("NULL, NULL, 0, gRect, 1, NULL"));
            result = gpfnGradientFill((TDC) NULL, NULL, 0, gRect, 1, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("NULL, NULL, 0, gRect, 1, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill((TDC) NULL, NULL, 2, NULL, 0, NULL);
            passCheck(result, 0, TEXT("NULL, NULL, 2, NULL, 0, NULL"));
            result = gpfnGradientFill((TDC) NULL, NULL, 2, NULL, 0, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("NULL, NULL, 2, NULL, 0, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill((TDC) NULL, NULL, 2, NULL, 1, NULL);
            passCheck(result, 0, TEXT("NULL, NULL, 2, NULL, 1, NULL"));
            result = gpfnGradientFill((TDC) NULL, NULL, 2, NULL, 1, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("NULL, NULL, 2, NULL, 1, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill((TDC) NULL, NULL, 2, gRect, 0, NULL);
            passCheck(result, 0, TEXT("NULL, NULL, 2, gRect, 0, NULL"));
            result = gpfnGradientFill((TDC) NULL, NULL, 2, gRect, 0, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("NULL, NULL, 2, gRect, 0, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill((TDC) NULL, NULL, 2, gRect, 1, NULL);
            passCheck(result, 0, TEXT("NULL, NULL, 2, gRect, 1, NULL"));
            result = gpfnGradientFill((TDC) NULL, NULL, 2, gRect, 1, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("NULL, NULL, 2, gRect, 1, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill((TDC) NULL, vert, 0, NULL, 0, NULL);
            passCheck(result, 0, TEXT("NULL, vert, 0, NULL, 0, NULL"));
            result = gpfnGradientFill((TDC) NULL, vert, 0, NULL, 0, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("NULL, vert, 0, NULL, 0, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill((TDC) NULL, vert, 0, NULL, 1, NULL);
            passCheck(result, 0, TEXT("NULL, vert, 0, NULL, 1, NULL"));
            result = gpfnGradientFill((TDC) NULL, vert, 0, NULL, 1, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("NULL, vert, 0, NULL, 1, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill((TDC) NULL, vert, 0, gRect, 0, NULL);
            passCheck(result, 0, TEXT("NULL, vert, 0, gRect, 0, NULL"));
            result = gpfnGradientFill((TDC) NULL, vert, 0, gRect, 0, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("NULL, vert, 0, gRect, 0, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill((TDC) NULL, vert, 0, gRect, 1, NULL);
            passCheck(result, 0, TEXT("NULL, vert, 0, gRect, 1, NULL"));
            result = gpfnGradientFill((TDC) NULL, vert, 0, gRect, 1, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("NULL, vert, 0, gRect, 1, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill((TDC) NULL, vert, 2, NULL, 0, NULL);
            passCheck(result, 0, TEXT("NULL, vert, 2, NULL, 0, NULL"));
            result = gpfnGradientFill((TDC) NULL, vert, 2, NULL, 0, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("NULL, vert, 2, NULL, 0, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill((TDC) NULL, vert, 2, NULL, 1, NULL);
            passCheck(result, 0, TEXT("NULL, vert, 2, NULL, 1, NULL"));
            result = gpfnGradientFill((TDC) NULL, vert, 2, NULL, 1, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("NULL, vert, 2, NULL, 1, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill((TDC) NULL, vert, 2, gRect, 0, NULL);
            passCheck(result, 0, TEXT("NULL, vert, 2, gRect, 0, NULL"));
            result = gpfnGradientFill((TDC) NULL, vert, 2, gRect, 0, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("NULL, vert, 2, gRect, 0, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill((TDC) NULL, vert, 2, gRect, 1, NULL);
            passCheck(result, 0, TEXT("NULL, vert, 2, gRect, 1, NULL"));
            result = gpfnGradientFill((TDC) NULL, vert, 2, gRect, 1, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("NULL, vert, 2, gRect, 1, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill(aDC, NULL, 0, NULL, 0, NULL);
            passCheck(result, 0, TEXT("aDC, NULL, 0, NULL, 0, NULL"));
            result = gpfnGradientFill(aDC, NULL, 0, NULL, 0, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("aDC, NULL, 0, NULL, 0, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill(aDC, NULL, 0, NULL, 1, NULL);
            passCheck(result, 0, TEXT("aDC, NULL, 0, NULL, 1, NULL"));
            result = gpfnGradientFill(aDC, NULL, 0, NULL, 1, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("aDC, NULL, 0, NULL, 1, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill(aDC, NULL, 0, gRect, 0, NULL);
            passCheck(result, 0, TEXT("aDC, NULL, 0, gRect, 0, NULL"));
            result = gpfnGradientFill(aDC, NULL, 0, gRect, 0, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("aDC, NULL, 0, gRect, 0, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill(aDC, NULL, 0, gRect, 1, NULL);
            passCheck(result, 0, TEXT("aDC, NULL, 0, gRect, 1, NULL"));
            result = gpfnGradientFill(aDC, NULL, 0, gRect, 1, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("aDC, NULL, 0, gRect, 1, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill(aDC, NULL, 2, NULL, 0, NULL);
            passCheck(result, 0, TEXT("aDC, NULL, 2, NULL, 0, NULL"));
            result = gpfnGradientFill(aDC, NULL, 2, NULL, 0, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("aDC, NULL, 2, NULL, 0, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill(aDC, NULL, 2, NULL, 1, NULL);
            passCheck(result, 0, TEXT("aDC, NULL, 2, NULL, 1, NULL"));
            result = gpfnGradientFill(aDC, NULL, 2, NULL, 1, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("aDC, NULL, 2, NULL, 1, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill(aDC, NULL, 2, gRect, 0, NULL);
            passCheck(result, 0, TEXT("aDC, NULL, 2, gRect, 0, NULL"));
            result = gpfnGradientFill(aDC, NULL, 2, gRect, 0, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("aDC, NULL, 2, gRect, 0, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill(aDC, NULL, 2, gRect, 1, NULL);
            passCheck(result, 0, TEXT("aDC, NULL, 2, gRect, 1, NULL"));
            result = gpfnGradientFill(aDC, NULL, 2, gRect, 1, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("aDC, NULL, 2, gRect, 1, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill(aDC, vert, 0, NULL, 0, NULL);
            passCheck(result, 0, TEXT("aDC, vert, 0, NULL, 0, NULL"));
            result = gpfnGradientFill(aDC, vert, 0, NULL, 0, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("aDC, vert, 0, NULL, 0, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill(aDC, vert, 0, NULL, 1, NULL);
            passCheck(result, 0, TEXT("aDC, vert, 0, NULL, 1, NULL"));
            result = gpfnGradientFill(aDC, vert, 0, NULL, 1, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("aDC, vert, 0, NULL, 1, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill(aDC, vert, 0, gRect, 0, NULL);
            passCheck(result, 0, TEXT("aDC, vert, 0, gRect, 0, NULL"));
            result = gpfnGradientFill(aDC, vert, 0, gRect, 0, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("aDC, vert, 0, gRect, 0, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill(aDC, vert, 0, gRect, 1, NULL);
            passCheck(result, 0, TEXT("aDC, vert, 0, gRect, 1, NULL"));
            result = gpfnGradientFill(aDC, vert, 0, gRect, 1, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("aDC, vert, 0, gRect, 1, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill(aDC, vert, 2, NULL, 0, NULL);
            passCheck(result, 0, TEXT("aDC, vert, 2, NULL, 0, NULL"));
            result = gpfnGradientFill(aDC, vert, 2, NULL, 0, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("aDC, vert, 2, NULL, 0, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill(aDC, vert, 2, NULL, 1, NULL);
            passCheck(result, 0, TEXT("aDC, vert, 2, NULL, 1, NULL"));
            result = gpfnGradientFill(aDC, vert, 2, NULL, 1, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("aDC, vert, 2, NULL, 1, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill(aDC, vert, 2, gRect, 0, NULL);
            passCheck(result, 0, TEXT("aDC, vert, 2, gRect, 0, NULL"));
            result = gpfnGradientFill(aDC, vert, 2, gRect, 0, GRADIENT_FILL_RECT_H);
            passCheck(result, 0, TEXT("aDC, vert, 2, gRect, 0, GRADIENT_FILL_RECT_H"));
            result = gpfnGradientFill(aDC, vert, 2, gRect, 1, 0x0BAD);
            passCheck(result, 0, TEXT("aDC, vert, 2, gRect, 1, 0x0BAD"));
            #ifdef UNDER_CE
            result = gpfnGradientFill((TDC) NULL, NULL, 0, NULL, 0, GRADIENT_FILL_TRIANGLE);
            passCheck(result, 0, TEXT("NULL, NULL, 0, NULL, 0, GRADIENT_FILL_TRIANGLE")); 
            result = gpfnGradientFill(aDC, vert, 2, gRect, 1, GRADIENT_FILL_TRIANGLE);
            passCheck(result, 0, TEXT("aDC, vert, 2, gRect, 1, GRADIENT_FILL_TRIANGLE"));            
            #endif
            break;
            default:
                info(FAIL, TEXT("PassNull called on an invalid case"));
             break;
    }
    switch (testFunc)
    {
        case EGetPixel:
        case ESetPixel:
            myGetLastError(NADA);
            break;
        case EClientToScreen:
        case EScreenToClient:
            myGetLastError(0x578);  // REVIEW: what is this NT 1314 code?
            break;
        case EPolygon:          // GLE testing done above
        case EPolyline:
            break;
        default:
            myGetLastError(ERROR_INVALID_HANDLE);
            break;
    }

    DeleteObject(hBmp);
    DeleteDC(aCompDC);
    myReleaseDC(aDC);
}


/***********************************************************************************
***
***   BitBlt Functional Test
***
************************************************************************************/
#define cell      RESIZE(80)
#define trans(x)  (x*cell)

POINT   blank,
        newBlank,
        oldBlank;
int     pastMoves[testCycles];

//***********************************************************************************
BOOL validMove(int i, int move)
{
    POINT   board = { zx / 2 / cell, zy / cell };
    const POINT fixPos[4] = { {0, -1}, {1, 0}, {0, 1}, {-1, 0} };   // N E S W

    if (gSanityChecks)
        info(FAIL, TEXT("What? i:%d m:%d nb:(%d %d) b:(%d %d) o:(%d %d)"), i, move, newBlank.x, newBlank.y, blank.x, blank.y,
             oldBlank.x, oldBlank.y);

    newBlank.x = blank.x + fixPos[move].x;
    newBlank.y = blank.y + fixPos[move].y;
    pastMoves[i] = move;

    return (newBlank.x != oldBlank.x && newBlank.y != oldBlank.y && newBlank.x >= 0 && newBlank.y >= 0 && newBlank.x < board.x
            && newBlank.y < board.y);
}

//***********************************************************************************
void
doMove(TDC hdc, int testFunc)
{
    POINT   s,
            d,
            temp,
            o;

    o.x = s.x = trans(newBlank.x);
    o.y = s.y = trans(newBlank.y);
    d.x = trans(blank.x);
    d.y = trans(blank.y);

    while (s.x != d.x || s.y != d.y)
    {
        temp = s;
        if (s.x == d.x)         // translate y
            s.y += (s.y > d.y) ? -1 : 1;
        else                    // translate x
            s.x += (s.x > d.x) ? -1 : 1;

        switch (testFunc)
        {
            case EBitBlt:
                myBitBlt(hdc, s.x, s.y, cell, cell, hdc, temp.x, temp.y, SRCCOPY);
                break;
            case EStretchBlt:
                myStretchBlt(hdc, s.x, s.y, cell, cell, hdc, temp.x, temp.y, cell, cell, SRCCOPY);
                break;
            case EMaskBlt:
                myMaskBlt(hdc, s.x, s.y, cell, cell, hdc, temp.x, temp.y, NULL, 0, 0, SRCCOPY);
                break;
            case ETransparentImage:
                TransparentBlt(hdc, s.x, s.y, cell, cell, hdc, temp.x, temp.y, cell, cell, RGB(255,255,255));
                break;
        }

        if (o.x < d.x)
            myPatBlt(hdc, o.x, o.y, s.x - o.x, cell, BLACKNESS);
        else if (o.x > d.x)
            myPatBlt(hdc, s.x + cell, o.y, o.x - s.x, cell, BLACKNESS);
        else if (o.y < d.y)
            myPatBlt(hdc, o.x, o.y, cell, s.y - o.y, BLACKNESS);
        else if (o.y > d.y)
            myPatBlt(hdc, o.x, s.y + cell, cell, o.y - s.y, BLACKNESS);

        //      Sleep(1);
    }
    oldBlank = blank;
    blank = newBlank;
    Sleep(1);
}

//***********************************************************************************
void
decorateScreen(TDC hdc)
{

    HFONT   hFont,
            hFontOld = NULL;
    LOGFONT lFont;
    RECT    bounding = { 0, 0, zx / 2, zy };
    TCHAR   word[50];
    UINT    len;
    HBRUSH  hBrush = CreateSolidBrush(RGB(0, 0, 128)),
            stockBr = (HBRUSH) SelectObject(hdc, hBrush);
    HRGN    hRgn = CreateRectRgnIndirect(&bounding);
    int     colors[2] = { RGB(192, 192, 192), RGB(128, 128, 128) }, peg = 0;

    lFont;
    if (!hBrush || !stockBr || !hRgn)
    {
        info(FAIL, TEXT("Important resource failed: Brush:0%x0 stockBr:0%x0 hRgn::0%x0"), hBrush, stockBr, hRgn);
    }

    blank.x = goodRandomNum(zx / 2 / cell);
    blank.y = goodRandomNum(zy / cell);
    oldBlank.x = oldBlank.y = -1;

    FillRgn(hdc, hRgn, hBrush);

    _tcscpy(word, TEXT("mmm")); //Microsoft Logo       
    hFont = setupFont(1, MSLOGO, hdc, 65, 0, /*450 */ 0, 0, FW_BOLD, 1, 0, 0);

    SetTextAlign(hdc, TA_CENTER | TA_TOP);

    len = _tcslen(word);

    SetBkMode(hdc, TRANSPARENT);
    SetBkColor(hdc, RGB(255, 0, 0));

    int     i = RESIZE(zx / 4 - RESIZE(25));

    for (int x = bounding.top - RESIZE(200); x < bounding.bottom + RESIZE(300); x += RESIZE(100))
    {
        SetTextColor(hdc, colors[0]);
        if (!ExtTextOut(hdc, i - peg, x, ETO_CLIPPED, &bounding, word, len, NULL))
            info(FAIL, TEXT("ExtTextOut blasting: %d %d"), i, x);

        SetTextColor(hdc, colors[1]);
        if (!ExtTextOut(hdc, i + RESIZE(2) - peg, x + RESIZE(4), ETO_CLIPPED, &bounding, word, len, NULL))
            info(FAIL, TEXT("ExtTextOut blasting: %d %d"), i, x);
    }

#ifndef UNDER_CE
    cleanupFont(1, aFont, hdc, hFont);
#else // UNDER_CE
    SelectObject(hdc, hFontOld ? hFontOld : GetStockObject(SYSTEM_FONT));
    DeleteObject(hFont);
#endif // UNDER_CE

    SelectObject(hdc, stockBr);
    DeleteObject(hBrush);
    DeleteObject(hRgn);
}

//***********************************************************************************
void
BitBltFunctionalTest(int testFunc)
{

    info(ECHO, TEXT("%s: Sliding Squares"), funcName[testFunc]);

    if (g_fPrinterDC)
    {
        info(ECHO, TEXT("Test invalid for printer DC"));
        return;
    }

    float	  OldMaxErrors = SetMaxErrorPercentage((float) .15);
    TDC     hdc = myGetDC();
    int     i;
    const int invertMove[4] = { 2, 3, 0, 1 };

    decorateScreen(hdc);
    myPatBlt(hdc, trans(blank.x), trans(blank.y), cell, cell, BLACKNESS);
    myBitBlt(hdc, zx / 2, 0, zx / 2, zy, hdc, 0, 0, SRCCOPY);

    // mix up screen
    for (i = 0; i < testCycles; i++)
    {
        while (!validMove(i, goodRandomNum(4)))
            ;                   // do nothing
        doMove(hdc, testFunc);
    }
    // do everything in reverse
    for (i = testCycles - 1; i >= 0; i--)
    {
        validMove(i, invertMove[pastMoves[i]]);
        doMove(hdc, testFunc);
    }
    CheckScreenHalves(hdc);
    myReleaseDC(hdc);
    SetMaxErrorPercentage(OldMaxErrors);
}

/***********************************************************************************
***
***   Blt off screen
***
************************************************************************************/
//***********************************************************************************
void
BitBltOffScreen(int testFunc, int pass)
{

    info(ECHO, TEXT("%s - BitBlt offscreen: pass=%d"), funcName[testFunc], pass);

    // set up vars
    TDC     hdc = myGetDC();
    TDC     offDC;
    TBITMAP hBmp,
            stockBmp;
    DWORD   dwError;

    if ((offDC = myCreateCompatibleDC(hdc)) == NULL)
    {
        dwError = GetLastError();
        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("BitBltOffScreen: CreateCompatibleDC() failed: out of memory: GLE:%d"), dwError);
        else
            info(FAIL, TEXT("BitBltOffScreen: CreateCompatibleDC() failed:  GLE:%d"), dwError);
        goto LReleaseDC;
    }

    if ((hBmp = CreateCompatibleBitmap(hdc, zx / 2, zy)) == NULL)
    {
        dwError = GetLastError();
        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("BitBltOffScreen: CreateCompBitmap() failed: out of memory: GLE:%d"), dwError);
        else
            info(FAIL, TEXT("BitBltOffScreen: CreateCompBitmap() failed:  GLE:%d"), dwError);
        goto LDeleteDC;
    }

    stockBmp = (TBITMAP) SelectObject(offDC, hBmp);

    if (!stockBmp)
    {
        dwError = GetLastError();
        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("BitBltOffScreen: SelectOBject(hBmp) failed: out of memory: GLE:%d"), dwError);
        else
            info(FAIL, TEXT("BitBltOffScreen: SelectOBject(hBmp)) failed:  GLE:%d"), dwError);
        goto LDeleteBMP;

    }
    // make the offscreen dc background match the source
    BitBlt(offDC, 0, 0, zx/2, zy, hdc, 0, 0, SRCCOPY);

    // draw win logo on top screen
    drawLogo(hdc, 150);

    if (pass == 0 && !g_fPrinterDC)
        myBitBlt(offDC, 0, 0, zx / 2, zy, hdc, 0, 0, SRCCOPY);  // copy entire half of screen to offscreen
    else
        drawLogo(offDC, 150);   // draw win logo on top screen

    // copy squares from offscreen to top screen
    switch (testFunc)
    {
        case EBitBlt:
            myBitBlt(hdc, zx / 2, 0, zx / 2, zy, offDC, 0, 0, SRCCOPY);
            break;
        case EStretchBlt:
            myStretchBlt(hdc, zx / 2, 0, zx / 2, zy, offDC, 0, 0, zx / 2, zy, SRCCOPY);
            break;
        case EMaskBlt:
            myMaskBlt(hdc, zx / 2, 0, zx / 2, zy, offDC, 0, 0, NULL, 0, 0, SRCCOPY);
            break;
    }

    Sleep(500);
    CheckScreenHalves(hdc);
    SelectObject(offDC, stockBmp);

  LDeleteBMP:
    DeleteObject(hBmp);
  LDeleteDC:
    myDeleteDC(offDC);
  LReleaseDC:
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Get/Set Pixel on & off screen
***
************************************************************************************/

//***********************************************************************************
void
PixelCopy(TDC src, TDC dest, int testFunc)
{
    COLORREF aColor;
    int     i, // width
            j, // height
            x, // fade
            y; // fade
    int     a20 = 0,
            a150 = 0;
    
    BOOL bOnePixel = testFunc == EGetPixel || testFunc == ESetPixel 
                                  || testFunc == EPatBlt ||testFunc == ERoundRect || testFunc == ERectangle;
    int    StepX = bOnePixel? 2 : ((rand() % 5)+1), // step in x drawing larger blocks
            StepY = bOnePixel ? 2 : ((rand() % 5)+1); // step in y for drawing larger blocks

    // create a mask bitmap for the MaskBlt call, we're doing a SRCCOPY for both foreground and background,
    // so the actual pattern shouldn't make any difference.
    TDC hdcCompat = CreateCompatibleDC(src);
    TBITMAP hbmp = myCreateBitmap(zx, zy, 1, 1, NULL), hbmpStock;

    hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp);

    // fill the entire surface in 1 call, so we don't get invalid driver verification failures.
    PatBlt(hdcCompat, 0, 0, zx, zy, WHITENESS);
    PatBlt(hdcCompat, zx/4, 0, zx/4, zy, BLACKNESS);
    PatBlt(hdcCompat, 3*zx/4, 0, zx/4, zy, BLACKNESS);

    SelectObject(hdcCompat, hbmpStock);
    DeleteDC(hdcCompat);
    
    // if we're doing getpixel or setpixel, then we're stepping by 1, and fading
    if(bOnePixel)
    { 
        // for small device 240x320:  or 208*240: 
        // Win95 logo (width = 200) passes the boundary of half screen 
        if (zx <= 240)
        {
            a20 = 0;
            a150 = RESIZE(100);
        }
        else
        {
            a20 = RESIZE(20);
            a150 = RESIZE(100);
        }
    }
    else myPatBlt(dest, zx / 2, 0, zx, zy, BLACKNESS);
    
    info(ECHO, TEXT("a20=%d  a150=%d"), a20, a150);

    x = 0;
    do  // fade
    {
        y = 0;
        do // fade
        { 
            for (i = a20 + x; i < zx / 2 - a20; i += (StepX)) // pixel copy
            {
                StepX = bOnePixel? 2 : ((rand() % 10)+1);
                for (j = a150 + y; j < zy - a150 / 2; j += (StepY))
                {
                    StepY = bOnePixel ? 2 : ((rand() % 10)+1);
                    switch (testFunc)
                    {
                        case EGetPixel:
                        case ESetPixel:
                            mySetLastError();
                            aColor = GetPixel(src, i, j);
                            myGetLastError(NADA);
                            mySetLastError();
                            SetPixel(dest, i + zx / 2, j, aColor);
                            myGetLastError(NADA);
                            break;
                        case EBitBlt:
                            mySetLastError();
                            BitBlt(dest, i + zx / 2, j, StepX, StepY, src, i, j, SRCCOPY);
                            myGetLastError(NADA);
                            break;
                        case EMaskBlt:
                            mySetLastError();
                            MaskBlt(dest, i + zx / 2, j, StepX, StepY, src, i, j, hbmp, 0, 0, MAKEROP4(SRCCOPY, SRCCOPY));
                            myGetLastError(NADA);
                            break;
                        case EStretchBlt:
                            mySetLastError();
                            StretchBlt(dest, i + zx / 2, j, StepX, StepY, src, i, j, StepX, StepY, SRCCOPY);
                            myGetLastError(NADA);
                            break;
                        case EPatBlt:
                            mySetLastError();
                            aColor = GetPixel(src, i, j);
                            myGetLastError(NADA);
                            mySetLastError();
                            blastRect(dest, i + zx / 2, j, 1, 1, aColor, EPatBlt);
                            myGetLastError(NADA);
                            break;
                        case ERectangle:
                            mySetLastError();
                            aColor = GetPixel(src, i, j);
                            myGetLastError(NADA);
                            mySetLastError();
                            blastRect(dest, i + zx / 2, j, 1, 1, aColor, ERectangle);
                            myGetLastError(NADA);
                            break;
                        case EEllipse:
                            mySetLastError();
                            aColor = GetPixel(src, i, j);
                            myGetLastError(NADA);
                            mySetLastError();
                            blastRect(dest, i + zx / 2, j, 2, 2, aColor, EEllipse);
                            myGetLastError(NADA);
                            break;
                        case ERoundRect:
                            mySetLastError();
                            aColor = GetPixel(src, i, j);
                            myGetLastError(NADA);
                            mySetLastError();
                            blastRect(dest, i + zx / 2, j, 2, 2, aColor, ERoundRect);
                            myGetLastError(NADA);
                            break;
                        case ETransparentImage:
                            mySetLastError();
                            // make black transparent, so blitting the black portions of the logo onto the black
                            // background appears to have no effect.
                            TransparentBlt(dest, i + zx / 2, j, StepX, StepY, src, i, j, StepX, StepY, RGB(0,0,0));
                            myGetLastError(NADA);
                            break;
                    }
                }
            }
            // fade in width, only if doing 1 pixel copy
            y++;
        }while(y <= 1 && bOnePixel);
        // fade in height, only if doing 1 pixel copy
        x++;
    }while(x < 2 && bOnePixel);

    DeleteObject(hbmp);
}

//***********************************************************************************
void
GetSetOffScreen(int testFunc)
{

    info(ECHO, TEXT("%s - GetSet Off Screen"), funcName[testFunc]);
    if (g_fPrinterDC)
    {
        info(ECHO, TEXT("Test invalid on Printer DC"));
        return;
    }

    // set up vars
    TDC     hdc = myGetDC();
    TDC     offDC;
    TBITMAP hBmp,
            stockBmp;
    DWORD   dwError;
    
    if ((offDC = myCreateCompatibleDC(hdc)) == NULL)
    {
        dwError = GetLastError();
        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("GetSetOffScreen: CreateCompatibleDC() failed: out of memory: GLE:%d"), dwError);
        else
            info(FAIL, TEXT("GetSetOffScreen: CreateCompatibleDC() failed:  GLE:%d"), dwError);
        goto LReleaseDC;
    }

    if ((hBmp = CreateCompatibleBitmap(hdc, zx / 2, zy)) == NULL)
    {
        dwError = GetLastError();
        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("GetSetOffScreen: CreateCompBitmap() failed: out of memory: GLE:%d"), dwError);
        else
            info(FAIL, TEXT("GetSetOffScreen: CreateCompBitmap() failed:  GLE:%d"), dwError);
        goto LDeleteDC;
    }

    stockBmp = (TBITMAP) SelectObject(offDC, hBmp);

    if (!stockBmp)
    {
        dwError = GetLastError();
        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("GetSetOffScreen: SelectOBject(hBmp) failed: out of memory: GLE:%d"), dwError);
        else
            info(FAIL, TEXT("GetSetOffScreen: SelectOBject(hBmp)) failed:  GLE:%d"), dwError);
        goto LDeleteBMP;

    }

    // make the offscreen dc background match the source
    BitBlt(offDC, 0, 0, zx/2, zy, hdc, 0, 0, SRCCOPY);

    // draw win logo on top screen
    drawLogo(hdc, 150);

    // copy entire half of screen to offscreen
    myBitBlt(offDC, 0, 0, zx / 2, zy, hdc, 0, 0, SRCCOPY);

    PixelCopy(offDC, hdc, testFunc);

    CheckScreenHalves(hdc);

    SelectObject(offDC, stockBmp);

  LDeleteBMP:
    DeleteObject(hBmp);
  LDeleteDC:
    myDeleteDC(offDC);
  LReleaseDC:
    myReleaseDC(hdc);
}

//***********************************************************************************
void
GetSetOnScreen(int testFunc)
{

    info(ECHO, TEXT("%s - GetSet On Screen"), funcName[testFunc]);
    if (g_fPrinterDC)
    {
        info(ECHO, TEXT("Test invalid with printer DC"));
        return;
    }
    
    // set up vars
    TDC     hdc = myGetDC();

    drawLogo(hdc, 150 /* offset */ );   // draw win logo on top screen: x: zx/2 - offset
    PixelCopy(hdc, hdc, testFunc);
    CheckScreenHalves(hdc);
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Draw APIs
***
************************************************************************************/

//***********************************************************************************
void
checkRect(int testFunc)
{

    info(ECHO, TEXT("%s - checkRect"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    int     result, expected = 0;

    // clear the surface for CheckAllWhite
    PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
    SelectObject(hdc, (HBRUSH) GetStockObject(BLACK_BRUSH));
    SelectObject(hdc, (HPEN) GetStockObject(NULL_PEN));
    switch (testFunc)
    {
        case ERectangle:
            Rectangle(hdc, 10, 10, 10 + 1, 10 + 1);
            expected = 1;
            break;
        case EEllipse:
            Ellipse(hdc, 10, 10, 10 + 1, 10 + 1);
            expected = 0;
            break;
        case ERoundRect:
            RoundRect(hdc, 10, 10, 10 + 1, 10 + 1, 0, 0);
            expected = 0;
            break;
    }
    result = CheckAllWhite(hdc, 1, 1, 0, expected);

    if (result != expected)
    {
        info(FAIL, TEXT("%s - #pixels expected: %d; actual: %d"), funcName[testFunc], expected, result);
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Negative Size
***
************************************************************************************/

//***********************************************************************************
void
NegativeSize(int testFunc)
{

    info(ECHO, TEXT("%s - Negative Size"), funcName[testFunc]);

    // myGetDC now modifies zy/zx, so all tests must get them as the first thing.
    TDC     hdc = myGetDC(),
            compDC,
            hdcTemp;
    TBITMAP hBmp,
            stockBmp;
    RECT    srcRect = { (zx / 2 - UPSIZE) / 2, (zy - UPSIZE) / 2,
        (zx / 2 - UPSIZE) / 2 + UPSIZE, (zy - UPSIZE) / 2 + UPSIZE
    };
    int     result;

    int     tA[4][6] = {
        {zx / 2, 0, zx / 2, zy, 0, 0},
        {zx, 0, -zx / 2, zy, zx / 2, 0},
        {zx / 2, zy, zx / 2, -zy, 0, zy},
        {zx, zy, -zx / 2, -zy, zx / 2, zy},
    };

    // rectangle for testing inverted StretchBlt's
    int SR[2][16] = { {   zx, zy, -zx/2, -zy, 0,  0, zx/2,  zy,
                               zx/2, zy, -zx/2, -zy, 0,  0, zx/2,   zy },
                          {   zx/2,   0,   zx/2,   zy, 0, zy, zx/2, -zy,
                                    0,  zy,   zx/2, -zy, 0,  0, zx/2,   zy}};

    hBmp = myLoadBitmap(globalInst, TEXT("UP"));
    compDC = CreateCompatibleDC(hdc);
    stockBmp = (TBITMAP) SelectObject(compDC, hBmp);

    // hdcTemp is the source hdc for the blt functions. Since a printer DC cannot be a source, 
    // we substitute the compDC as the source when hdc is a printer DC.
    if (g_fPrinterDC)
    {
        hdcTemp = compDC;
    }
    else
    {
        hdcTemp = hdc;
    }

    for (int i = 0; i < 4; i++)
    {
        myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);
        myBitBlt(hdc, srcRect.left, srcRect.top, UPSIZE, UPSIZE, compDC, 0, 0, SRCCOPY);
        switch (testFunc)
        {
            case EBitBlt:
                result = myBitBlt(hdc, tA[i][0], tA[i][1], tA[i][2], tA[i][3], hdcTemp, tA[i][4], tA[i][5], SRCCOPY);
                info(ECHO, TEXT("%s(hdc, %d, %d, %d, %d, hdc, %d %d, SRCCOPY) returned:%d"), funcName[testFunc], tA[i][0],
                     tA[i][1], tA[i][2], tA[i][3], tA[i][4], tA[i][5], result);
                break;
            case EMaskBlt:
                result = myMaskBlt(hdc, tA[i][0], tA[i][1], tA[i][2], tA[i][3], hdcTemp, tA[i][4], tA[i][5], NULL, 0, 0, SRCCOPY);
                info(ECHO, TEXT("%s(hdc, %d, %d, %d, %d, hdc, %d %d, NULL, 0, 0, SRCCOPY) returned:%d"), funcName[testFunc],
                     tA[i][0], tA[i][1], tA[i][2], tA[i][3], tA[i][4], tA[i][5], result);
                break;
            case EStretchBlt:
                result =
                    myStretchBlt(hdc, tA[i][0], tA[i][1], tA[i][2], tA[i][3], hdcTemp, 0, 0, abs(tA[i][2]), abs(tA[i][3]), SRCCOPY);
                info(ECHO, TEXT("%s(hdc, %d, %d, %d, %d, hdc, 0, 0, %d, %d, SRCCOPY) returned:%d"), funcName[testFunc],
                     tA[i][0], tA[i][1], tA[i][2], tA[i][3], abs(tA[i][2]), abs(tA[i][3]), result);
                break;
            case ETransparentImage:
                result =
                    TransparentBlt(hdc, tA[i][0], tA[i][1], tA[i][2], tA[i][3], hdcTemp, 0, 0, abs(tA[i][2]), abs(tA[i][3]), RGB(0,0,0));
                info(ECHO, TEXT("%s(hdc, %d, %d, %d, %d, hdc, 0, 0, %d, %d, SRCCOPY) returned:%d"), funcName[testFunc],
                     tA[i][0], tA[i][1], tA[i][2], tA[i][3], abs(tA[i][2]), abs(tA[i][3]), result);
                break;
        }
        if (testFunc != EStretchBlt && testFunc != ETransparentImage)
            CheckScreenHalves(hdc);
    }

    if(testFunc == EStretchBlt || testFunc == ETransparentImage)
    {
        for(int j=0; j<2; j++)
        {
            myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);
            // put the up label on the left half
            myBitBlt(hdc, srcRect.left, srcRect.top, UPSIZE, UPSIZE, compDC, 0, 0, SRCCOPY);
            switch(testFunc)
            {
                case EStretchBlt:
                    // flip the left half onto the right half
                    StretchBlt(hdc, SR[j][0], SR[j][1], SR[j][2], SR[j][3], hdcTemp, SR[j][4], SR[j][5], SR[j][6], SR[j][7], SRCCOPY);
                    // flip the left half over itself
                    StretchBlt(hdc, SR[j][8], SR[j][9], SR[j][10], SR[j][11], hdcTemp, SR[j][12], SR[j][13], SR[j][14], SR[j][15], SRCCOPY);
                    break;
                case ETransparentImage:
                    // put the up label on the right half and do the flip having white transparent
                    myBitBlt(hdc, srcRect.left + zx/2, srcRect.top, UPSIZE, UPSIZE, compDC, 0, 0, SRCCOPY);
                    // flip the left half onto the right half
                    TransparentBlt(hdc, SR[j][0], SR[j][1], SR[j][2], SR[j][3], hdcTemp, SR[j][4], SR[j][5], SR[j][6], SR[j][7], RGB(255,255,255));
                    // flip the left half over itself
                    TransparentBlt(hdc, SR[j][8], SR[j][9], SR[j][10], SR[j][11], hdcTemp, SR[j][12], SR[j][13], SR[j][14], SR[j][15], RGB(255,255,255));
                    break;
            }
            // the halves should match
            CheckScreenHalves(hdc);
        }
    }
    // don't need to do anything with hdcTemp, since it is just a copy of either compDC or hdc
    SelectObject(compDC, stockBmp);
    DeleteDC(compDC);
    DeleteObject(hBmp);
    myReleaseDC(hdc);
}

void
StretchBltFlipMirrorTest(int testFunc)
{

    info(ECHO, TEXT("%s - StretchBltFlipMirrorTest"), funcName[testFunc]);

    // myGetDC now modifies zy/zx, so all tests must get them as the first thing.
    TDC     hdc = myGetDC();
    int     result;
    COLORREF crExpected = RGB(0,0,0);
    COLORREF crResult;

    // rectangle for testing inverted StretchBlt's
    int SR[][10] = {  {  0,  0,  zx,  zy,  0,  0,  zx,  zy,      0,     0},
                           {  0,  0,  zx,  zy,  0, zy,  zx, -zy,      0, zy-1},
                           {  0,  0,  zx,  zy, zx,  0, -zx,  zy, zx-1,      0},
                           {  0,  0,  zx,  zy, zx, zy, -zx, -zy, zx-1, zy-1},
                           {  0, zy,  zx, -zy,  0,  0,  zx,  zy,       0, zy-1 },
                           {  0, zy,  zx, -zy,  0, zy,  zx, -zy,     0,       0},
                           {  0, zy,  zx, -zy, zx,  0, -zx,  zy, zx-1, zy-1},
                           {  0, zy,  zx, -zy, zx, zy, -zx, -zy, zx-1,     0},
                           { zx,  0, -zx,  zy,  0,  0,  zx,  zy, zx-1,        0},
                           { zx,  0, -zx,  zy,  0, zy,  zx, -zy, zx-1, zy-1},
                           { zx,  0, -zx,  zy, zx,  0, -zx,  zy,      0,      0},
                           { zx,  0, -zx,  zy, zx, zy, -zx, -zy,     0, zy-1},
                           { zx, zy, -zx, -zy,  0,  0,  zx,  zy, zx-1, zy-1},
                           { zx, zy, -zx, -zy,  0, zy,  zx, -zy, zx-1,     0},
                           { zx, zy, -zx, -zy, zx,  0, -zx,  zy,      0, zy-1},
                           { zx, zy, -zx, -zy, zx, zy, -zx, -zy,     0,      0}};

    // hdcTemp is the source hdc for the blt functions. Since a printer DC cannot be a source, 
    // we substitute the compDC as the source when hdc is a printer DC.
    if (!g_fPrinterDC)
    {
        if(testFunc == EStretchBlt || testFunc == ETransparentImage)
        {
            for(int j=0; j<16; j++)
            {
                result = TRUE;
                result &= PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
                SetPixel(hdc, 0,0, RGB(0,0,0));
                switch(testFunc)
                {
                    case EStretchBlt:
                        // flip the left half onto the right half
                        result &= StretchBlt(hdc, SR[j][0], SR[j][1], SR[j][2], SR[j][3], hdc, SR[j][4], SR[j][5], SR[j][6], SR[j][7], SRCCOPY);
                        break;
                    case ETransparentImage:
                        // flip the left half onto the right half, make green transparent because there shouldn't be any green anywhere in this test.
                        result &= TransparentBlt(hdc, SR[j][0], SR[j][1], SR[j][2], SR[j][3], hdc, SR[j][4], SR[j][5], SR[j][6], SR[j][7], RGB(0,255,0));
                        break;
                }
                crResult = GetPixel(hdc, SR[j][8], SR[j][9]);

                if(crExpected != crResult)
                    info(FAIL, TEXT("Pixel mismatch, expected %d != %d for StretchBlt or TransparentBlt(hdc, %d, %d, %d, %d, hdc, %d, %d, %d, %d"), crExpected, crResult, SR[j][0], SR[j][1], SR[j][2], SR[j][3], SR[j][4], SR[j][5], SR[j][6], SR[j][7]);
                else if(!result)
                    info(FAIL, TEXT("One or more api calls failed during test."));

            }
        }
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Try bad ROPs
***
************************************************************************************/

DWORD   badROPs[11] = {
    0x00550009,                 //DSTINVERT,
    0x00c000ca,                 //MERGECOPY,
    0x00bb0226,                 //MERGEPAINT,
    0x00330008,                 //NOTSRCCOPY,
    0x001100a6,                 //NOTSRCERASE,
    0x005a0049,                 //PATINVERT,
    0x00fb0a09,                 //PATPAINT,
    0x008800c6,                 //SRCAND,
    0x00440328,                 //SRCERASE,
    0x00660046,                 //SRCINVERT,
    0x00ee0086,                 //SRCPAINT,
};

//***********************************************************************************
void
BadROPs(int testFunc)
{

    info(ECHO, TEXT("%s - Try Bad ROPs"), funcName[testFunc]);
    TDC     hdc = myGetDC();
    int     result;

    for (int i = 0; i < countof(badROPs); i++)
    {
        result = BitBlt(hdc, 0, 0, 50, 50, hdc, 0, 0, badROPs[i]);
        if (result)
            info(FAIL, TEXT("BitBlt should have failed given undefined ROP %x"), badROPs[i]);
    }
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Bogus Source DC
***
************************************************************************************/

//***********************************************************************************
void
BogusSrcDC(int testFunc)
{
    // because we're using bogus source dc's, we fall through to HDC'S, so we cannot verify.
    BOOL bOldVerify = SetSurfVerify(FALSE);
    info(ECHO, TEXT("%s - Bogus Source DC"), funcName[testFunc]);
    TDC     hdc = myGetDC();
    int     result = 0,
            expected = 16,
            Bpixels;
    DWORD   dwError;

    // clear the surface for the CheckAllWhite called after the test.
    myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);

    switch (testFunc)
    {
        case EBitBlt:
            result = BitBlt(hdc, 50, 50, 4, 4, (HDC) 0x777, 0, 0, BLACKNESS);
            break;
        case EStretchBlt:
            result = StretchBlt(hdc, 50, 50, 4, 4, (HDC) 0x777, 0, 0, 4, 4, BLACKNESS);
            break;
        case EMaskBlt:
            result = MaskBlt(hdc, 50, 50, 4, 4, (HDC) 0x777, 0, 0, (HBITMAP) NULL, 0, 0, MAKEROP4(BLACKNESS, WHITENESS));
            break;
    }

    if (!result)
        info(FAIL, TEXT("%s should not have failed given bogus srcDC"), funcName[testFunc]);

    Bpixels = CheckAllWhite(hdc, 1, 1, 1, expected);

    // Failure Could be due to running out of memory.
    // Expect screen at 50,50,4,4, (4*4 = 16) should painted as  black.
    if (Bpixels != expected)
    {
        dwError = GetLastError();
        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("CheckAllWhite fail: Out of memory: err=%ld"), dwError);
        else
            info(FAIL, TEXT("Blt(50,50,4,4,.BLACKNESS):  %d pixel is found: Expect 16 pixel black"), Bpixels);
    }

    myReleaseDC(hdc);
    SetSurfVerify(bOldVerify);
}

//***********************************************************************************
void
BogusSrcDCMaskBlt(int testFunc)
{
    info(ECHO, TEXT("%s - Bogus Source DC MaskBlt test"), funcName[testFunc]);
    TDC     hdc = myGetDC();
    TDC     hdcCompat = CreateCompatibleDC(hdc);
    TBITMAP hbmp = myCreateBitmap(10, 10, 1, 1, NULL), hbmpStock;
    int     result = 0,
            expected = 4,
            Bpixels;
    DWORD   dwError;

    hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp);

    // initialize the entire bitmap in 1 call so we don't get false driver verification failures.
    PatBlt(hdcCompat, 0, 0, 10, 10, WHITENESS);
    PatBlt(hdcCompat, 0, 0, 4, 4, BLACKNESS);
    PatBlt(hdcCompat, 0, 0, 2, 2, WHITENESS);

    SelectObject(hdcCompat, hbmpStock);
    DeleteDC(hdcCompat);

    // clear the surface for the CheckAllWhite called after the test.
    myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);

    result = MaskBlt(hdc, 50, 50, 4, 4, (TDC) NULL, 0, 0, hbmp, 0, 0, MAKEROP4(BLACKNESS, WHITENESS));
    // the mask is 4x4 with a 2x2 block as the foreground, and the rest as background.
    // we're making the forground black, and the background white, thus we expect 4 pixels to be black.

    if (!result)
        info(FAIL, TEXT("%s should not have failed given bogus srcDC"), funcName[testFunc]);

    Bpixels = CheckAllWhite(hdc, 1, 1, 1, expected);

    // Failure Could be due to running out of memory.
    // Expect screen at 50,50,4,4, (2*2 = 4) should painted as  black.
    if (Bpixels != expected)
    {
        dwError = GetLastError();
        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("CheckAllWhite fail: Out of memory: err=%ld"), dwError);
        else
            info(FAIL, TEXT("MaskBlt(50,50,4,4,.BLACKNESS):  %d pixel is found: Expect 4 pixel black"), Bpixels);
    }

    DeleteObject(hbmp);
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Try Odd Shapes
***
************************************************************************************/

//***********************************************************************************
void
TryShapes(int testFunc)
{

    info(ECHO, TEXT("%s - Try Shapes"), funcName[testFunc]);
    TDC     hdc = myGetDC();
    int     result,
            Bpixels;

    int     OddShapes[4][4] = {
        {0, 4, 1, 0},
        {4, 0, 1, 0},
        {-1, -1, 1, 1},
        {0, 0, 1, 0},
    };

    for (int i = 0; i < 4; i++)
    {
        myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);
        result = PatBlt(hdc, 50, 50, OddShapes[i][0], OddShapes[i][1], BLACKNESS);
        if (result != OddShapes[i][2])
            info(FAIL, TEXT("PatBlt(50,50,%d,%d,BLACKNESS) returned:%d, expected:%d"), OddShapes[i][0], OddShapes[i][1], result,
                 OddShapes[i][2]);

        Bpixels = CheckAllWhite(hdc, 1, 1, 0, OddShapes[i][3]);

        if (Bpixels != OddShapes[i][3])
            info(FAIL, TEXT("PatBlt(50,50,%d,%d,BLACKNESS) painted pixels:%d, expected:%d"), OddShapes[i][0], OddShapes[i][1],
                 Bpixels, OddShapes[i][3]);
    }
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Clip BitBlt
***
************************************************************************************/

//***********************************************************************************
void
ClipBitBlt(int testFunc)
{

    info(ECHO, TEXT("%s - Test BitBlt with clip region:"), funcName[testFunc]);

    // if the screen/surface is smaller than the bitmaps we're testing, don't test, not enough room.
    if(SetWindowConstraint(3*PARIS_X, PARIS_Y))
    {
        
        TDC     hdc = myGetDC(),
                compDC = NULL;
        HRGN    hRgn = NULL;
        TBITMAP hOrigBitmap = NULL,
                hBitmap = myLoadBitmap(globalInst, TEXT("PARIS0"));
        int     i,
                shift[9][2] = { {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}, {1, 0},
        {0, 0}
        };
        RECT    rc;
        int     delta;
        int     x,
                y,
                nRet;

        // make sure our width is divisable by 4
        zx = (zx/4)*4;
        
        if (!hBitmap)
            info(FAIL, TEXT("hBitmap NULL"));

        PatBlt(hdc, 0, 0, zx, zy, BLACKNESS);
        // set up region
        hRgn = CreateRectRgn(5, 5, zx - 5, zy - 5);

        // set up bitmaps
        compDC = CreateCompatibleDC(hdc);
        hOrigBitmap = (TBITMAP) SelectObject(compDC, hBitmap);

        delta = 20;
        info(DETAIL, TEXT("-RESIZE(20) * shift[0][0]=%d: PARIS_X=%d PARIS_Y=%d\n"), -RESIZE(20) * shift[0][0], PARIS_X, PARIS_Y);

        for (i = 0; i < 9; i++)
        {
            info(DETAIL, TEXT("Testing shift [%d,%d]"), shift[i][0], shift[i][1]);
            SelectClipRgn(hdc, NULL);
            myPatBlt(hdc, 0, 0, zx, zy, BLACKNESS);
            switch (testFunc)
            {
                case EBitBlt:
                    x = (zx / 2 - PARIS_X) / 2 - RESIZE(delta) * shift[i][0];
                    y = (zy - PARIS_Y) / 2 - RESIZE(delta) * shift[i][1];
                    info(DETAIL, TEXT("left start: [%d %d]"), x, y);
                    myBitBlt(hdc, x, y, PARIS_X, PARIS_Y, compDC, 0, 0, SRCCOPY);

                    x = (zx / 2 - PARIS_X) / 2 + zx / 2;
                    y = (zy - PARIS_Y) / 2;
                    info(DETAIL, TEXT("right start: [%d %d]"), x, y);
                    myBitBlt(hdc, (zx / 2 - PARIS_X) / 2 + zx / 2, (zy - PARIS_Y) / 2, PARIS_X, PARIS_Y, compDC, 0, 0, SRCCOPY);
                    break;
                case EStretchBlt:
                    myStretchBlt(hdc, (zx / 2 - PARIS_X) / 2 - RESIZE(delta) * shift[i][0],
                                 (zy - PARIS_Y) / 2 - RESIZE(delta) * shift[i][1], PARIS_X, PARIS_Y, compDC, 0, 0, PARIS_X, PARIS_Y,
                                 SRCCOPY);
                    myStretchBlt(hdc, (zx / 2 - PARIS_X) / 2 + zx / 2, (zy - PARIS_Y) / 2, PARIS_X, PARIS_Y, compDC, 0, 0, PARIS_X,
                                 PARIS_Y, SRCCOPY);
                    break;
                case EMaskBlt:
                    myMaskBlt(hdc, (zx / 2 - PARIS_X) / 2 - RESIZE(delta) * shift[i][0],
                              (zy - PARIS_Y) / 2 - RESIZE(delta) * shift[i][1], PARIS_X, PARIS_Y, compDC, 0, 0, NULL, 0, 0,
                              SRCCOPY);
                    myMaskBlt(hdc, (zx / 2 - PARIS_X) / 2 + zx / 2, (zy - PARIS_Y) / 2, PARIS_X, PARIS_Y, compDC, 0, 0, NULL, 0, 0,
                              SRCCOPY);
                    break;
            }

            nRet = SelectClipRgn(hdc, hRgn);
            GetClipBox(hdc, &rc);
            info(DETAIL, TEXT("cliprect = [%d,%d,%d,%d]: nRet =%d"), rc.left, rc.top, rc.right, rc.bottom, nRet);

            switch (testFunc)
            {
                case EBitBlt:
                    x = RESIZE(delta) * shift[i][0];
                    y = RESIZE(delta) * shift[i][1];
                    info(DETAIL, TEXT("BitBlt(x=%d y=%d  cx=%d  cy=%d)"), x, y, zx / 2, zy);
                    myBitBlt(hdc, x, y, zx / 2, zy, hdc, 0, 0, SRCCOPY);
                    break;
                case EStretchBlt:
                    myStretchBlt(hdc, RESIZE(delta) * shift[i][0], RESIZE(delta) * shift[i][1], zx / 2, zy, hdc, 0, 0, zx / 2, zy,
                                 SRCCOPY);
                    break;
                case EMaskBlt:
                    myMaskBlt(hdc, RESIZE(delta) * shift[i][0], RESIZE(delta) * shift[i][1], zx / 2, zy, hdc, 0, 0, NULL, 0, 0,
                              SRCCOPY);
                    break;
            }
            CheckScreenHalves(hdc);
        }

        SelectClipRgn(hdc, NULL);
        DeleteObject(hRgn);
        SelectObject(compDC, hOrigBitmap);
        DeleteDC(compDC);
        DeleteObject(hBitmap);
        myReleaseDC(hdc);
        SetWindowConstraint(0,0);
    }
    else info(DETAIL, TEXT("Screen too small for test"));
}

/***********************************************************************************
***
***   ClientScreenTest
***
************************************************************************************/

//***********************************************************************************
void
checkScreen(int offsetX, int offsetY)
{

    POINT   point0,
            point1;
    int     result0,
            result1,
            errors = 0,
            maxerrors = 20;
    RECT    rc;

    GetWindowRect(ghwndTopMain, &rc);
    MoveWindow(ghwndTopMain, offsetX, offsetY, zx, zy, FALSE);

    for (int y = 0; y < zy && errors < maxerrors; y += 20)
        for (int x = 0; x < zx && errors < maxerrors; x += 20)
        {
            point0.x = point1.x = x;
            point0.y = point1.y = y;
            result0 = ScreenToClient(ghwndTopMain, &point0);
            result1 = ClientToScreen(ghwndTopMain, &point1);
            if (!result0 || !result1)
            {
                info(FAIL, TEXT("one call failed One:%d Two:%d"), result0, result1);
                errors++;
            }
            if (point0.x != x - offsetX || point0.y != y - offsetY)
            {
                info(FAIL, TEXT("StoC (%d %d) != (%d %d) - (%d %d)"), point0.x, point0.y, x, y, offsetX, offsetY);
                errors++;
            }
            if (point1.x != x + offsetX || point1.y != y + offsetY)
            {
                info(FAIL, TEXT("CtoS (%d %d) != (%d %d) + (%d %d)"), point0.x, point0.y, x, y, offsetX, offsetY);
                errors++;
            }
        }
    if (errors >= maxerrors)
        info(ECHO, TEXT("Excessive number of errors - part of test skipped"));

    // Move back to original position
    MoveWindow(ghwndTopMain, rc.left, rc.top, zx, zy, TRUE);
}

//***********************************************************************************
void
ClientScreenTest(int testFunc)
{

    info(ECHO, TEXT("%s - Client Screen Test"), funcName[testFunc]);

    for (int y = -10; y <= 10; y += 10)
        for (int x = -10; x <= 10; x += 10)
            checkScreen(x, y);

}

/***********************************************************************************
***
***   Simple Polygon Test
***
************************************************************************************/
void
SimpleClipRgnTest0(int testFunc)
{
#define MAX_FIXED_POINTS 150

    TDC     hdc = myGetDC();
    POINT   p[MAX_FIXED_POINTS],
            p2[MAX_FIXED_POINTS];
    HRGN    hRgn;
    int     result,
            i;
    COLORREF clr;
    RECT    r;

    r.left = zx / 2 - 50;
    r.top = zy / 2 - 50;
    r.right = zx / 2 + 50;
    r.bottom = zy / 2 + 50;

    info(ECHO, TEXT("%s - SimpleClipRgnTest0:  fixed points: Set ClipRect=[%d %d %d %d]"), funcName[testFunc], r.left, r.top,
         r.right, r.bottom);
    hRgn = CreateRectRgnIndirect(&r);

    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    SelectObject(hdc, GetStockObject(BLACK_PEN));

    PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
    SelectClipRgn(hdc, hRgn);
    GetClipBox(hdc, &r);
    info(DETAIL, TEXT("Get ClipRect = [%d %d %d %d]"), r.left, r.top, r.right, r.bottom);

    result = 0;
    for (i = 0; i < MAX_FIXED_POINTS; i += 2)
    {
        // one pixel outside of the clipbox: horizontal
        p[i].x = r.left - 1;    //69 om 240x320 device;      off one pixel in left
        p[i].y = r.top + i * 2;
        p[i + 1].x = r.right;   // end of the clipbox right
        p[i + 1].y = r.top + i * 2;

        // one pixel output clipbox: vertical
        p2[i].x = r.right + 1 - result;
        p2[i].y = r.top - 1;    // one pixel outside of the top 

        p2[i + 1].x = r.right + 1 - result;
        p2[i + 1].y = r.bottom + 1; // one pixel outside of the bottom of the clipbox
        result++;
    }
    
    for (i = 0; i < 10; i++)
    {
        SetLastError(0);
        switch (testFunc)
        {
            case EPolygon:
                result = Polygon(hdc, p, MAX_FIXED_POINTS);
                result &= Polygon(hdc, p2, MAX_FIXED_POINTS);
                break;
            case EPolyline:
                result = Polyline(hdc, p, MAX_FIXED_POINTS);
                result &= Polyline(hdc, p2, MAX_FIXED_POINTS);
                break;
            case EFillRect:
                r.left -= 1;
                r.top -= 1;
                r.right += 1;
                r.bottom += 1;
                result = FillRect(hdc, &r, (HBRUSH) (GetStockObject(BLACK_BRUSH)));
                break;
        }

        if (!result)
            info(FAIL, TEXT("%s: iLoop=%d: failed: err=%ld "), funcName[testFunc], i, GetLastError());
        else if (g_fPrinterDC)
        {
            info(ECHO, TEXT("Skipping checks in SimpleClipRgnTest0 on printer DC"));
        }
        else
        {
            Sleep(1000);
            if ((clr = GetPixel(hdc, r.left - 1, r.top)) != CLR_INVALID)
                info(FAIL, TEXT("iLoop=%d: [%d,%d]=0x%lX: expect outside cliprgn "), i, r.left - 1, r.top, clr);

            if ((clr = GetPixel(hdc, r.left, r.top - 1)) != CLR_INVALID)
                info(FAIL, TEXT("iLoop=%d: [%d,%d]=0x%lX: expect outside cliprgn "), i, r.left, r.top - 1, clr);

            if ((clr = GetPixel(hdc, r.right, r.bottom - 1)) != CLR_INVALID)
                info(FAIL, TEXT("iLoop=%d: [%d,%d]=0x%lX: expect outside cliprgn "), i, r.right, r.bottom - 1, clr);

            if ((clr = GetPixel(hdc, r.right + 1, r.bottom)) != CLR_INVALID)
                info(FAIL, TEXT("iLoop=%d: [%d,%d]=0x%lX: expect outside cliprgn "), i, r.right + 1, r.bottom, clr);

            PatBlt(hdc, r.left, r.top, r.right, r.bottom, WHITENESS);
            Sleep(1000);
            if (CheckAllWhite(hdc, 0, 0, 0, 0))
            {
                info(FAIL, TEXT("%s: iLoop=%d: failed"), funcName[testFunc], i);
                Sleep(5000);
            }
        }
    }
    SelectClipRgn(hdc, NULL);
    DeleteObject(hRgn);
    myReleaseDC(hdc);
}

//***********************************************************************************
void
SimplePolyTest(int testFunc)
{

    info(ECHO, TEXT("%s - Simple Poly Test: 500 points"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    POINT   p[500],
           *point = p;
    HRGN    hRgn = CreateRectRgn(zx / 2 - 50, zy / 2 - 50, zx / 2 + 50, zy / 2 + 50);
    int     result;
    float   OldMaxErrors = SetMaxErrorPercentage((float) .0001);

    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    SelectObject(hdc, GetStockObject(BLACK_PEN));

    for (int pass = 0; pass < 2; pass++)
    {
        PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
        if (pass == 1)
        {
            SelectClipRgn(hdc, hRgn);
            RECT    r;

            GetClipBox(hdc, &r);
            info(DETAIL, TEXT("ClipRect = [%d %d %d %d]"), r.left, r.top, r.right, r.bottom);
        }

        for (int count = 0; count < 500; count += (count < 5) ? 1 : 100)
        {
            PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
            for (int n = 0; n < count; n++)
            {
                p[n].x = goodRandomNum(zx);
                p[n].y = goodRandomNum(zy);
            }
            if (testFunc == EPolygon)
                result = Polygon(hdc, point, count);
            else
                result = Polyline(hdc, point, count);
            if (!result && count > 1)
                info(FAIL, TEXT("%s failed %d vertexes"), funcName[testFunc], count);
            else if (result && count <= 1 && !(testFunc == EPolyline && count == 0))
                info(FAIL, TEXT("%s should have failed %d vertexes"), funcName[testFunc], count);
            if (pass == 1)
            {
                PatBlt(hdc, zx / 2 - 50, zy / 2 - 50, zx / 2 + 50, zy / 2 + 50, WHITENESS);
                if (0 != (result = CheckAllWhite(hdc, 0, 0, 0, 0)))
                {
                    info(DETAIL, TEXT("Pass==1: CheckAllWhite return error pixels=%d: points=%d "), result, count);
                    Sleep(5000);
                }
                else
                    info(DETAIL, TEXT("Pass==1: CheckAllWhite succeeded:"));
            }
        }
    }
    SelectClipRgn(hdc, NULL);
    DeleteObject(hRgn);
    myReleaseDC(hdc);
    SetMaxErrorPercentage(OldMaxErrors);
}

/***********************************************************************************
***
***   Simple Polygon Test
***
************************************************************************************/

//***********************************************************************************
void
StressPolyTest(int testFunc)
{

    info(ECHO, TEXT("%s - Stress Poly Test: 1000 loops"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    POINT   p[500],
           *point = p;
    HRGN    hRgn = CreateRectRgn(zx / 2 - 50, zy / 2 - 50, zx / 2 + 50, zy / 2 + 50);
    int     result;
    RECT    rc;
    float OldMaxErrors = SetMaxErrorPercentage((float) .00005);

    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    SelectObject(hdc, GetStockObject(BLACK_PEN));

    // clear the screen for CheckAllWhite
    PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
    
    SelectClipRgn(hdc, hRgn);
    // Check Region:
    GetClipBox(hdc, &rc);
    info(ECHO, TEXT("Clip Region = [%d %d %d %d]"), rc.left, rc.top, rc.right, rc.bottom);

    for (int z = 0; z < 1000; z++)
    {
        for (int count = 0; count < 500; count += (count < 5) ? 1 : 100)
        {
            for (int n = 0; n < count; n++)
            {
                p[n].x = goodRandomNum(zx);
                p[n].y = goodRandomNum(zy);
            }
            if (testFunc == EPolygon)
                result = Polygon(hdc, point, count);
            else
                result = Polyline(hdc, point, count);
            if (!result && count > 1)
                info(FAIL, TEXT("%s failed %d vertexes"), funcName[testFunc], count);
            else if (result && count <= 1 && !(testFunc == EPolyline && count == 0))
                info(FAIL, TEXT("%s should have failed %d vertexes"), funcName[testFunc], count);
        }
    }

    PatBlt(hdc, rc.left, rc.top, rc.right, rc.bottom, WHITENESS);
    CheckAllWhite(hdc, 0, 0, 0, 0);

    Sleep(10);
    SelectClipRgn(hdc, NULL);
    DeleteObject(hRgn);
    myReleaseDC(hdc);
    SetMaxErrorPercentage(OldMaxErrors);
}

/***********************************************************************************
***
***   Simple Polyline Test
***
************************************************************************************/

void
PolylineTest()                  // this one need visually check the drawing on the screen***************************/
{
    info(ECHO, TEXT("PolyLineTest"));

    int     x,
            y,
            iPen;
    TDC     hdc = myGetDC();
    HPEN    hpen;
    POINT   points[2],
           *p = points;

    for (iPen = 1; iPen < 10; iPen += 2)
    {
        myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);
        hpen = CreatePen(PS_SOLID, iPen, RGB(0, 0, 0));
        hpen = (HPEN) SelectObject(hdc, hpen);
        // Check Hotizontal line drawing
        points[0].x = 0;
        points[1].x = zx;
        for (y = 0; y < zy; y += 50)
        {
            points[0].y = points[1].y = y;
            Polyline(hdc, p, 2);
        }

        // Check vertical line drawing
        points[0].y = 0;
        points[1].y = zy;
        for (x = 0; x < zx; x += 50)
        {
            points[0].x = points[1].x = x;
            Polyline(hdc, p, 2);
        }

        // Check diagonal line drawing
        points[0].x = 0;
        points[1].y = 0;
        for (y = 0; y < zy; y += 50)
        {
            points[0].y = y;
            points[1].x = y;
            Polyline(hdc, p, 2);
        }

        points[0].y = zy;
        points[1].x = zx;
        for (y = 0; y < zy; y += 50)
        {
            points[0].x = zx - y;
            points[1].y = zy - y;
            Polyline(hdc, p, 2);
        }

#ifdef VISUAL_CHECK
        MessageBox(NULL, TEXT(""), TEXT("line"), MB_OK | MB_SETFOREGROUND);
#endif
        hpen = (HPEN) SelectObject(hdc, hpen);
        DeleteObject(hpen);
    }
    myReleaseDC(hdc);
}

//***********************************************************************************
void
FillRectPolyline(TDC hdc, RECT * r, int offset)
{

    POINT   points[2],
           *p = points;

    points[0].y = (*r).top;
    points[1].y = (*r).bottom;

    for (int x = (*r).left; x < (*r).right; x++)
    {
        points[0].x = points[1].x = x + offset;
        Polyline(hdc, p, 2);
    }
}

//***********************************************************************************
void
SimplePolylineTest(int testFunc)
{

    info(ECHO, TEXT("%s - Simple Polyline Test"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HBRUSH  hBrush = (HBRUSH) GetStockObject(BLACK_BRUSH);
    RECT    aRect;

    setTestRectRgn();

    SelectObject(hdc, (HPEN) GetStockObject(BLACK_PEN));

    for (int test = 0; test < NUMREGIONTESTS; test++)
    {
        aRect.left = RESIZE(rTests[test].left);
        aRect.top = RESIZE(rTests[test].top);
        aRect.right = RESIZE(rTests[test].right);
        aRect.bottom = RESIZE(rTests[test].bottom);

        info(ECHO, TEXT("aRect=[%d %d %d %d]"), aRect.left, aRect.top, aRect.right, aRect.bottom);

        PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
        FillRect(hdc, &aRect, hBrush);

        //240x320  or 208x240: samll deice: aRect sometime is passing the border of zx/2
        // Clean out the other side of the screen
        myPatBlt(hdc, zx / 2, 0, zx, zy, WHITENESS);
        FillRectPolyline(hdc, &aRect, zx / 2);
        CheckScreenHalves(hdc);
    }
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Poly Test
***
************************************************************************************/

//***********************************************************************************
void
PolyTest(int testFunc)
{

    info(ECHO, TEXT("%s - Poly Test"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HPEN hpen = NULL, hpenStock = NULL;
    POINT   *p0 = NULL,
            *p1 = NULL;
    
    float OldMaxVerify = SetMaxErrorPercentage((float) .005);

    p0 = new POINT[500];
    p1 = new POINT[500];

    if(p0 && p1)
    {
        for (int nPenWidth = 0; nPenWidth < 10; nPenWidth++)
        {
            hpen = CreatePen(PS_SOLID, nPenWidth, randColorRef());

            // if pen creation fails, log an abort and continue the test with the default pen.
            if(hpen)
            {
                // pen creation succeeded, select it and test
                hpenStock = (HPEN) SelectObject(hdc, hpen);
                if(!hpenStock)
                    info(ABORT, TEXT("Pen selection failed"));
                else info(DETAIL, TEXT("Testing with a width of %d"), nPenWidth);
            }
            else info(ABORT, TEXT("Pen Creation failed, using default pen."));
            
            for (int count = 0; count < 500; count += (count < 5) ? 1 : 100)
            {
                info(ECHO, TEXT("Testing #points:%d"), count);
                PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
                for (int n = 0; n < count; n++)
                {
                    // don't let the line go off the left or right edge because of it's width, 
                    // compensate x for widths.
                    p0[n].x = goodRandomNum(zx / 2 - nPenWidth * 4) + nPenWidth * 2;
                    p0[n].y = p1[n].y = goodRandomNum(zy);
                    p1[n].x = p0[n].x + zx / 2;
                }
                p1[count].x = p1[0].x;
                p1[count].y = p1[0].y;
                Polygon(hdc, p0, count);
                Polyline(hdc, p1, count + 1);
                // for pen widths > 1 polygon and polyline don't match exactly at corners.
                // same behavior on nt
                if(nPenWidth <= 1)
                    CheckScreenHalves(hdc);
            }

            // if pen creation earlier succeeded, do nothing if it failed, error previously logged.
            if(hpen)
            {
                hpenStock = (HPEN) SelectObject(hdc, hpenStock);
                // hpenStock should now be the previous pen selected, if it isn't, it's an error.
                if(hpenStock != hpen)
                    info(FAIL, TEXT("Pen returned did not match pen selected"));

                if(!DeleteObject(hpen))
                    info(FAIL, TEXT("Deletion of previously created pen failed"));
            }
        }
    }
    else info(ABORT, TEXT("Failed to allocate memory for point arrays"));

    if(p0)
        delete[] p0;
    if(p1)
        delete[] p1;
    
    myReleaseDC(hdc);
    // error tolerances must be left on until after myReleaseDC
    SetMaxErrorPercentage(OldMaxVerify);
}

/***********************************************************************************
***
***   Simple Create Bitmap Test
***
************************************************************************************/

//***********************************************************************************
void
SimpleCreateBitmapTest(int testFunc)
{

    info(ECHO, TEXT("%s - Simple Create Bitmap Test"), funcName[testFunc]);

    BYTE   *lpBits = NULL;
    TBITMAP hBmp;
    BITMAP  bmp;
    int     hh,
            ww,
            cc;

    for (int h = 0; h < 30; h++)
        for (int w = 0; w < 30; w++)
            for (int c = 0; c < countof(gBitDepths); c++)
            {
                hBmp = myCreateBitmap(w, h, 1, gBitDepths[c], lpBits);
                if(!hBmp)
                    info(FAIL, TEXT("creation of a bitmap w:%d h:%d d:%d failed."), w, h, gBitDepths[c]);
                
                if(!GetObject(hBmp, sizeof (BITMAP), &bmp))
                    info(FAIL, TEXT("Failed to retrieve BITMAP object"));

                // varaibles
                ww = w;
                hh = h;
                cc = gBitDepths[c];

                if (w == 0 || h == 0)
                    ww = hh = cc = 1;
                if (bmp.bmWidth != ww || bmp.bmHeight != hh || bmp.bmBitsPixel != cc)
                    info(FAIL, TEXT("[h%d w%d c%d] One of following occured: w%d != bmp.w%d  h%d != bmp.h%d  c%d != bmp.c%d"),
                         h, w, c, ww, bmp.bmWidth, hh, bmp.bmHeight, cc, bmp.bmBitsPixel);
                // defaults
                if (bmp.bmType != 0 || bmp.bmWidthBytes == 0 || bmp.bmPlanes != 1 || bmp.bmBits != NULL)
                    info(FAIL,
                         TEXT
                         ("[h%d w%d c%d] One of following occured: bmType%d!=0  bmWidthBytes%d==0  bmPlanes%d!=1  bmBits%X!=NULL"),
                         h, w, c, bmp.bmType, bmp.bmWidthBytes, bmp.bmPlanes, bmp.bmBits);
                DeleteObject(hBmp);
            }
}

/***********************************************************************************
***
***   draw Logo Blit
***
************************************************************************************/

//***********************************************************************************
void
drawLogoBlit(int testFunc)
{

    info(ECHO, TEXT("%s - draw Logo Blit"), funcName[testFunc]);

    if (g_fPrinterDC)
    {
        info(ECHO,TEXT("Test invalid on Printer DC"));
        return;
    }

    int     i;

    TDC     hdc = myGetDC();

    for (i = 0; i < blockSize; i++)
        blastRect(hdc, RESIZE(blocks[i][0]), RESIZE(blocks[i][1]), RESIZE(blocks[i][2]), RESIZE(blocks[i][3]), blocks[i][4],
                  EPatBlt);

    switch (testFunc)
    {
        case EBitBlt:
            for (i = 0; i < curveSize; i++)
                myBitBlt(hdc, RESIZE(curves[i][0]), RESIZE(curves[i][1]), RESIZE(curves[i][2]), RESIZE(curves[i][3]), hdc,
                         RESIZE(curves[i][4]), RESIZE(curves[i][5]), SRCCOPY);
            break;
        case EStretchBlt:
            for (i = 0; i < curveSize; i++)
                myStretchBlt(hdc, RESIZE(curves[i][0]), RESIZE(curves[i][1]), RESIZE(curves[i][2]), RESIZE(curves[i][3]), hdc,
                             RESIZE(curves[i][4]), RESIZE(curves[i][5]), RESIZE(curves[i][2]), RESIZE(curves[i][3]), SRCCOPY);
            break;
        case EMaskBlt:
            for (i = 0; i < curveSize; i++)
                myMaskBlt(hdc, RESIZE(curves[i][0]), RESIZE(curves[i][1]), RESIZE(curves[i][2]), RESIZE(curves[i][3]), hdc,
                          RESIZE(curves[i][4]), RESIZE(curves[i][5]), NULL, 0, 0, SRCCOPY);
            break;
    }

    Sleep(10);
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   MaskBlt Test
***
************************************************************************************/
#define aSquare   10

//***********************************************************************************
void
MaskBltTest(int testFunc)
{

    info(ECHO, TEXT("%s - MaskBlt Test"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
            maskDC = CreateCompatibleDC(hdc),
            srcDC = CreateCompatibleDC(hdc);
    TBITMAP hMask = myCreateBitmap(zx / 2, zy, 1, 1, NULL),
            stock0,
            hSrc = CreateCompatibleBitmap(hdc, zx / 2, zy),
            stock1;
    POINT   squares = { zx / 2 / aSquare, zy / aSquare };
    int     x,
            y,
            xx,
            yy,
            wx,
            wy,
            result;
    HRGN    hRgn;
    DWORD   BlitMask = MAKEROP4(0x00AA0000, SRCCOPY);

    if (!hMask || !hSrc)
    {
        info(FAIL, TEXT("Important bitmap failed - can go no further"));
        return;
    }

    // set up mask
    stock0 = (TBITMAP) SelectObject(maskDC, hMask);
    PatBlt(maskDC, 0, 0, zx / 2, zy, BLACKNESS);

    // set up src
    stock1 = (TBITMAP) SelectObject(srcDC, hSrc);
    myPatBlt(srcDC, 0, 0, zx / 2, zy, WHITENESS);
    for (x = 0; x < squares.x; x++)
        for (y = 0; y < squares.y; y++)
            if ((x + y) % 2 == 0)
                myPatBlt(srcDC, x * aSquare, y * aSquare, aSquare, aSquare, BLACKNESS);

    for (int n = 0; n < 10; n++)
    {
        x = goodRandomNum(zx / 2);
        y = goodRandomNum(zy);
        wx = goodRandomNum(zx / 2 - x);
        wy = goodRandomNum(zy - y);
        myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);
        result = MaskBlt(hdc, x, y, wx, wy, srcDC, x, y, hMask, x, y, BlitMask);
        info(ECHO, TEXT("MaskBlt(%X,%d,%d,%d,%d,%X,%d,%d,%X,%d,%d)"), hdc, x, y, wx, wy, srcDC, x, y, hMask, x, y);
        if (!result)
            info(FAIL, TEXT("Failed with GLE:%d"), GetLastError());
        hRgn = CreateRectRgn(x + zx / 2, y, x + wx + zx / 2, y + wy);
        SelectClipRgn(hdc, hRgn);
        for (xx = 0; xx < squares.x; xx++)
            for (yy = 0; yy < squares.y; yy++)
                if ((xx + yy) % 2 == 0)
                    myPatBlt(hdc, xx * aSquare + zx / 2, yy * aSquare, aSquare, aSquare, BLACKNESS);
        SelectClipRgn(hdc, NULL);
        DeleteObject(hRgn);
        if (result)
            CheckScreenHalves(hdc);
    }

    SelectObject(maskDC, stock0);
    SelectObject(srcDC, stock1);
    DeleteObject(hMask);
    DeleteObject(hSrc);
    myReleaseDC(hdc);
    DeleteDC(maskDC);
    DeleteDC(srcDC);
}

/***********************************************************************************
***
***   FillRectTest
***
************************************************************************************/

//***********************************************************************************
void
FillRectTest(int testFunc)
{

    TDC     hdc = myGetDC();
    HBRUSH  hBrush = (HBRUSH) GetStockObject(WHITE_BRUSH);
    RECT    r = { zx, zy, -1, -1 };
    int     result;

    info(ECHO, TEXT("%s - Using negative coords [%d %d -1 -1]"), funcName[testFunc], zx, zy);

    PatBlt(hdc, 0, 0, zx, zy, BLACKNESS);
    if(!FillRect(hdc, &r, hBrush))
        info(FAIL, TEXT("FillRect call failed."));
    result = CheckAllWhite(hdc, 1, 1, 1, 0);
    if (result != 0)
        info(FAIL, TEXT("FillRect failed draw with negative coords"));

    myReleaseDC(hdc);
}

//***********************************************************************************
void
SimpleFillRect(int testFunc)
{

    info(ECHO, TEXT("%s - SimpleFillRect"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HBRUSH  hBrush = (HBRUSH) GetStockObject(BLACK_BRUSH);
    RECT    r = { 10, 10, 20, 0 };  // top > bottom

    myPatBlt(hdc, 10 + zx / 2, 0, 10, 10, BLACKNESS);
    FillRect(hdc, &r, hBrush);
    CheckScreenHalves(hdc);
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   CreateBitmap Bad Param
***
************************************************************************************/

//***********************************************************************************
void
CreateBitmapBadParam(int testFunc)
{

    info(ECHO, TEXT("%s - CreateBitmap: using Bad Param"), funcName[testFunc]);

    TBITMAP hBmp;
    BOOL    result;

    hBmp = myCreateBitmap(0, 0, 1, 2, NULL);
    passCheck(hBmp != NULL, 1, TEXT("hBmp NULL"));
    result = DeleteObject(hBmp);
    if (!result)
        info(FAIL, TEXT("DeleteObject failed GLE:%d"), GetLastError());
}

//***********************************************************************************
void
SimpleCreateBitmap2(int testFunc)
{

    info(ECHO, TEXT("%s - CreateBitmap: check stock bmp"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
            compDC = CreateCompatibleDC(hdc);
    TBITMAP hBmp,
            hBmpStock;

    hBmpStock = (TBITMAP) GetCurrentObject(compDC, OBJ_BITMAP);

    hBmp = myCreateBitmap(0, 10, 1, 2, NULL);
    passCheck(hBmp == hBmpStock, 1, TEXT("CreateBitmap(0, 10,...) didn't return stock Bitmap"));
    DeleteObject(hBmp);

    hBmp = myCreateBitmap(10, 0, 1, 2, NULL);
    passCheck(hBmp == hBmpStock, 1, TEXT("CreateBitmap(10, 0,...) didn't return stock Bitmap"));
    DeleteObject(hBmp);
    
    DeleteDC(compDC);
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   CreateBitmap Squares
***
************************************************************************************/

//***********************************************************************************
void
CreateBitmapSquares1bpp(int testFunc)
{

    info(ECHO, TEXT("%s - CreateBitmap Squares1bpp"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
            compDC = CreateCompatibleDC(hdc);
    POINT   mid = { zx / 4, zy / 2 };
    TBITMAP hBmp,
            stockBmp;
    // 8 pixels on, 8 pixels off, 16 pixels high for an 8x8 checkerboard
    BYTE    wSquare[16][2] = {
        {0xFF, 0x00}, 
        {0xFF, 0x00},
        {0xFF, 0x00}, 
        {0xFF, 0x00},
        {0xFF, 0x00}, 
        {0xFF, 0x00},
        {0xFF, 0x00}, 
        {0xFF, 0x00},
        {0x00, 0xFF}, 
        {0x00, 0xFF},
        {0x00, 0xFF}, 
        {0x00, 0xFF},
        {0x00, 0xFF}, 
        {0x00, 0xFF},
        {0x00, 0xFF}, 
        {0x00, 0xFF}
    };

    myPatBlt(hdc, 0, 0, zx, zy, BLACKNESS);
    myPatBlt(hdc, zx/2 + mid.x, mid.y, 8, 8, WHITENESS);
    myPatBlt(hdc, zx/2 + mid.x + 8, mid.y + 8, 8, 8, WHITENESS);
    
    hBmp = myCreateBitmap(16, 16, 1, 1, (LPVOID)wSquare);
    if(hBmp == NULL)
        info(FAIL, TEXT("CreateBitmap(16, 16,  1, 1, (LPVOID)wSquare)"));
    else
    {
        stockBmp = (TBITMAP) SelectObject(compDC, hBmp);
        myBitBlt(hdc, mid.x, mid.y, 16, 16, compDC, 0, 0, SRCCOPY);

        CheckScreenHalves(hdc);

        SelectObject(compDC, stockBmp);
        DeleteObject(hBmp);
    }

    DeleteDC(compDC);
    myReleaseDC(hdc);
}

//***********************************************************************************
void
CreateBitmapSquares2bpp(int testFunc)
{
#ifdef UNDER_CE
    info(ECHO, TEXT("%s - CreateBitmap Squares2bpp"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
            compDC = CreateCompatibleDC(hdc);
    POINT   mid = { zx / 4, zy / 2 };
    TBITMAP hBmp,
            stockBmp;
    // 8 pixels on, 8 pixels off, 16 pixels high for an 8x8 checkerboard
    WORD    wSquare[16][2] = {
        {0xFFFF, 0x0000}, 
        {0xFFFF, 0x0000},
        {0xFFFF, 0x0000}, 
        {0xFFFF, 0x0000},
        {0xFFFF, 0x0000}, 
        {0xFFFF, 0x0000},
        {0xFFFF, 0x0000}, 
        {0xFFFF, 0x0000},
        {0x0000, 0xFFFF}, 
        {0x0000, 0xFFFF},
        {0x0000, 0xFFFF}, 
        {0x0000, 0xFFFF},
        {0x0000, 0xFFFF}, 
        {0x0000, 0xFFFF},
        {0x0000, 0xFFFF}, 
        {0x0000, 0xFFFF}
    };

    myPatBlt(hdc, 0, 0, zx, zy, BLACKNESS);
    myPatBlt(hdc, zx/2 + mid.x, mid.y, 8, 8, WHITENESS);
    myPatBlt(hdc, zx/2 + mid.x + 8, mid.y + 8, 8, 8, WHITENESS);
    
    hBmp = myCreateBitmap(16, 16, 1, 2, (LPVOID)wSquare);
    if(hBmp == NULL)
        info(FAIL, TEXT("CreateBitmap(16, 16,  1, 2, (LPVOID)wSquare)"));
    else
    {
        stockBmp = (TBITMAP) SelectObject(compDC, hBmp);
        myBitBlt(hdc, mid.x, mid.y, 16, 16, compDC, 0, 0, SRCCOPY);

        CheckScreenHalves(hdc);

        SelectObject(compDC, stockBmp);
        DeleteObject(hBmp);
    }

    DeleteDC(compDC);
    myReleaseDC(hdc);
#endif
}

//***********************************************************************************
void
CreateBitmapSquares4bpp(int testFunc)
{
    info(ECHO, TEXT("%s - CreateBitmap Squares4bpp"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
            compDC = CreateCompatibleDC(hdc);
    POINT   mid = { zx / 4, zy / 2 };
    TBITMAP hBmp,
            stockBmp;
    // 8 pixels on, 8 pixels off, 16 pixels high for an 8x8 checkerboard
    BYTE    wSquare[16][8] = {
        {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00},
        {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00},
        {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00},
        {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00},
        {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00},
        {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00},
        {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00},
        {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF},
        {0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF},
        {0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF},
        {0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF},
        {0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF},
        {0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF},
        {0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF},
        {0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF}
    };

    myPatBlt(hdc, 0, 0, zx, zy, BLACKNESS);
    myPatBlt(hdc, zx/2 + mid.x, mid.y, 8, 8, WHITENESS);
    myPatBlt(hdc, zx/2 + mid.x + 8, mid.y + 8, 8, 8, WHITENESS);
    
    hBmp = myCreateBitmap(16, 16, 1, 4, (LPVOID)wSquare);
    if(hBmp == NULL)
        info(FAIL, TEXT("CreateBitmap(16, 16,  1, 4, (LPVOID)wSquare)"));
    else
    {
        stockBmp = (TBITMAP) SelectObject(compDC, hBmp);
        myBitBlt(hdc, mid.x, mid.y, 16, 16, compDC, 0, 0, SRCCOPY);

        CheckScreenHalves(hdc);

        SelectObject(compDC, stockBmp);
        DeleteObject(hBmp);
    }

    DeleteDC(compDC);
    myReleaseDC(hdc);
}

//***********************************************************************************
void
CreateBitmapSquares8bpp(int testFunc)
{
    info(ECHO, TEXT("%s - CreateBitmap Squares8bpp"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
            compDC = CreateCompatibleDC(hdc);
    POINT   mid = { zx / 4, zy / 2 };
    TBITMAP hBmp,
            stockBmp;
    // 8 pixels on, 8 pixels off, 16 pixels high for an 8x8 checkerboard
    BYTE    wSquare[16][16] = {
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
    };

    myPatBlt(hdc, 0, 0, zx, zy, BLACKNESS);
    myPatBlt(hdc, zx/2 + mid.x, mid.y, 8, 8, WHITENESS);
    myPatBlt(hdc, zx/2 + mid.x + 8, mid.y + 8, 8, 8, WHITENESS);
    
    hBmp = myCreateBitmap(16, 16, 1, 8, (LPVOID)wSquare);
    if(hBmp == NULL)
        info(FAIL, TEXT("CreateBitmap(16, 16,  1, 8, (LPVOID)wSquare)"));
    else
    {
        stockBmp = (TBITMAP) SelectObject(compDC, hBmp);
        myBitBlt(hdc, mid.x, mid.y, 16, 16, compDC, 0, 0, SRCCOPY);

        CheckScreenHalves(hdc);

        SelectObject(compDC, stockBmp);
        DeleteObject(hBmp);
    }

    DeleteDC(compDC);
    myReleaseDC(hdc);
}

//***********************************************************************************
void
CreateBitmapSquares2432bpp(int testFunc, int depth)
{
    info(ECHO, TEXT("%s - CreateBitmap Squares %dbpp"), funcName[testFunc], depth);

    TDC     hdc = myGetDC(),
            compDC = CreateCompatibleDC(hdc);
    POINT   mid = { zx / 4, zy / 2 };
    TBITMAP hBmp,
            stockBmp;
    // 8 pixels on, 8 pixels off, 16 pixels high for an 8x8 checkerboard
    DWORD    wSquare[16][16] = {
        {0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 
          0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
        {0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 
          0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
        {0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 
          0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
        {0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 
          0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
        {0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 
          0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
        {0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 
          0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
        {0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 
          0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
        {0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 
          0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
          0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF},
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
          0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF},
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
          0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF},
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
          0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF},
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
          0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF},
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
          0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF},
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
          0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF},
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
          0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF}
    };

    myPatBlt(hdc, 0, 0, zx, zy, BLACKNESS);
    myPatBlt(hdc, zx/2 + mid.x, mid.y, 8, 8, WHITENESS);
    myPatBlt(hdc, zx/2 + mid.x + 8, mid.y + 8, 8, 8, WHITENESS);
    
    hBmp = myCreateBitmap(16, 16, 1, depth, (LPVOID)wSquare);
    if(hBmp == NULL)
        info(FAIL, TEXT("CreateBitmap(16, 16,  1, %d, (LPVOID)wSquare)"), depth);
    else
    {
        stockBmp = (TBITMAP) SelectObject(compDC, hBmp);
        myBitBlt(hdc, mid.x, mid.y, 16, 16, compDC, 0, 0, SRCCOPY);

        CheckScreenHalves(hdc);

        SelectObject(compDC, stockBmp);
        DeleteObject(hBmp);
    }

    DeleteDC(compDC);
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   PatBlt Simple
***
************************************************************************************/

//***********************************************************************************
void
PatBltSimple(int testFunc)
{

    info(ECHO, TEXT("%s - PatBlt Simple"), funcName[testFunc]);

    TDC     hdc = myGetDC();

    BOOL    result;
    DWORD   code[5] = {
        PATCOPY,
        PATINVERT,
        DSTINVERT,
        BLACKNESS,
        WHITENESS
    };

    info(DETAIL, TEXT("case 1"));

    for (int i = 0; i < 5; i++)
    {
        mySetLastError();
        result = PatBlt(hdc, 400, 400, 200, 200, code[i]);
        myGetLastError(NADA);
        passCheck(result, 1, TEXT("PatBlt(hdc,  400,400, 200, 200, code[i])"));
    }

    info(DETAIL, TEXT("case 2"));
    PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
    PatBlt(hdc, zx / 2, 0, 100, 100, BLACKNESS);
    SelectObject(hdc, GetStockObject(BLACK_BRUSH));
    mySetLastError();
    result = PatBlt(hdc, 100, 100, -100, -100, PATCOPY);
    myGetLastError(NADA);
    passCheck(result, 1, TEXT("PatBlt(hdc,  400,400, 200, 200, PATCOPY)"));
    CheckScreenHalves(hdc);

    myReleaseDC(hdc);
}

//***********************************************************************************
void
PatBltSimple2(int testFunc)
{

    info(ECHO, TEXT("%s - PATINVERT"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    int     result;
    
    // clear the screen for CheckAllWhite
    myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    myPatBlt(hdc, 50, 50, 100, 100, PATINVERT);
    SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    myPatBlt(hdc, 100, 100, 200, 200, PATINVERT);

    result = CheckAllWhite(hdc, 1, 1, 1, 0);
    if (result != 0)
        info(FAIL, TEXT("PatBlt drew something with a NULL_BRUSH"));

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Set Get Pixel GLE Test
***
************************************************************************************/

//***********************************************************************************
void
SetGetPixelGLETest(int testFunc)
{

    info(ECHO, TEXT("%s - Set Get Pixel GLE Test"), funcName[testFunc]);

    TDC     hdc = myGetDC();

    // set the pixel
    mySetLastError();
    SetPixel(hdc, -100, -100, RGB(0, 0, 0));
    myGetLastError(NADA);
    info(ECHO, TEXT("SetPixel(%d %d)"), -100, -100);

    mySetLastError();
    GetPixel(hdc, -100, -100);
    myGetLastError(NADA);
    info(ECHO, TEXT("GetPixel(%d %d)"), -100, -100);

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   BitBlt Negative Source
***
************************************************************************************/
void
BitBltNegSrc(int testFunc)
{

    info(ECHO, TEXT("%s - Negative Source coordinates"), funcName[testFunc]);
    
    // we're pulling unknown data from a negative portion of the dc,
    // when running on a window dc, this is part of the title bar.
    // we can't verify this.
    BOOL bOldVerify = SetSurfVerify(FALSE);

    TDC     hdc = myGetDC();
    BOOL    result;

    result = BitBlt(hdc, 50, 50, 20, 20, hdc, -10, -10, SRCCOPY);

    if (!result)
        info(FAIL, TEXT("BitBlt returned %d expected 1"), result);

    myReleaseDC(hdc);
    SetSurfVerify(bOldVerify);
}

/***********************************************************************************
***
***   CreateDIBSection - DIBmemDC test
***
************************************************************************************/
void
DIBmemDC(int testFunc, int pass)
{

    info(ECHO, TEXT("%s - DIBmemDC test"), funcName[testFunc]);
    TDC     hdc = myGetDC(),
            compDC = CreateCompatibleDC(hdc);
    TBITMAP hBmp,
            stockBmp;

    struct
    {
        BITMAPINFOHEADER bmih;
        RGBQUAD ct[4];
    }
    bmi;

    memset(&bmi.bmih, 0, sizeof (BITMAPINFOHEADER));
    bmi.bmih.biSize = sizeof (BITMAPINFOHEADER);
    bmi.bmih.biWidth = 10;
    bmi.bmih.biHeight = -10;
    bmi.bmih.biPlanes = 1;
#ifdef UNDER_CE
    bmi.bmih.biBitCount = 2;
#else
    bmi.bmih.biBitCount = 1;
#endif
    bmi.bmih.biCompression = BI_RGB;
    setRGBQUAD(&bmi.ct[0], 0x0, 0x0, 0x0);
    setRGBQUAD(&bmi.ct[1], 0x55, 0x55, 0x55);
    setRGBQUAD(&bmi.ct[2], 0xaa, 0xaa, 0xaa);
    setRGBQUAD(&bmi.ct[3], 0xff, 0xff, 0xff);

    BYTE   *lpBits = NULL;

    hBmp = CreateDIBSection(hdc, (LPBITMAPINFO) & bmi, DIB_RGB_COLORS, (VOID **) & lpBits, NULL, NULL);

    stockBmp = (TBITMAP) SelectObject(compDC, hBmp);

    info(ECHO, TEXT("%s"), (pass) ? TEXT("using ExcludeClipRect") : TEXT("IntersectClipRect"));
    if (pass)
        ExcludeClipRect(compDC, 0, 0, 9, 9);
    else
        IntersectClipRect(compDC, 0, 0, 9, 9);

    SelectObject(compDC, stockBmp);
    DeleteObject(hBmp);
    myReleaseDC(hdc);
    DeleteDC(compDC);
}

void
SimpleRoundRectTest(int testFunc)
{
    float OldMaxErrors = SetMaxErrorPercentage((float) .00005);
    info(ECHO, TEXT("%s - RoundRect: dash pen"), funcName[testFunc]);
    TDC     hdc = myGetDC();
    HPEN    hPenDash,
            hPenTemp;
    LOGPEN  logDash;
    int     result;

    logDash.lopnStyle = PS_DASH;
    logDash.lopnWidth.x = 1;
    logDash.lopnWidth.y = 1;
    logDash.lopnColor = RGB(0, 0, 0);

    hPenDash = CreatePenIndirect(&logDash);

    hPenTemp = (HPEN) SelectObject(hdc, hPenDash);
    if(!RoundRect(hdc, 50, 50, -150, 150, 90, 90))
        info(FAIL, TEXT("RoundRect call failed"));

    result = CheckAllWhite(hdc, 1, 1, 1, 0);
    if (!result)
        info(ECHO, TEXT("RoundRect(PS_DASH,1,0) was not drawn"));

    hPenTemp = (HPEN) SelectObject(hdc, hPenTemp);
    passCheck(hPenTemp == hPenDash, TRUE, TEXT("Incorrect pen returned"));

    DeleteObject(hPenDash);
    myReleaseDC(hdc);
    SetMaxErrorPercentage(OldMaxErrors);
}

void
CreateBitmapNegSize(int testFunc)
{

    info(ECHO, TEXT("%s - Negtive sizes"), funcName[testFunc]);
    TBITMAP hBmp;
    int nDepth, nWidth, nHeight;
    
    for(nDepth=0; nDepth < countof(gBitDepths); nDepth++)
    {
        // pick a random width, between -1 and -100
        nWidth = -((rand() % 100) + 1);
        nHeight = ((rand() % 100) +1);
        mySetLastError();
        hBmp = myCreateBitmap(nWidth , nHeight, 1, gBitDepths[nDepth], NULL);
        myGetLastError(ERROR_INVALID_PARAMETER);
        if (hBmp)
        {
            info(FAIL, TEXT("hBmp should be NULL for width %d, height %d, and bitdepth %d"), nWidth, nHeight, gBitDepths[nDepth]);
            DeleteObject(hBmp);
        }
    }

    for(nDepth=0; nDepth < countof(gBitDepths); nDepth++)
    {
        // pick a random width, between -1 and -100
        nWidth = ((rand() % 100) + 1);
        nHeight = -((rand() % 100) +1);
        mySetLastError();
        hBmp = myCreateBitmap(nWidth , nHeight, 1, gBitDepths[nDepth], NULL);
        myGetLastError(ERROR_INVALID_PARAMETER);
        if (hBmp)
        {
            info(FAIL, TEXT("hBmp should be NULL for width %d, height %d, and bitdepth %d"), nWidth, nHeight, gBitDepths[nDepth]);

            DeleteObject(hBmp);
        }
    }

    for(nDepth=0; nDepth < countof(gBitDepths); nDepth++)
    {
        // pick a random width, between -1 and -100
        nWidth = -((rand() % 100) + 1);
        nHeight = -((rand() % 100) +1);
        mySetLastError();
        hBmp = myCreateBitmap(nWidth , nHeight, 1, gBitDepths[nDepth], NULL);
        myGetLastError(ERROR_INVALID_PARAMETER);
        if (hBmp)
        {
            info(FAIL, TEXT("hBmp should be NULL for width %d, height %d, and bitdepth %d"), nWidth, nHeight, gBitDepths[nDepth]);

            DeleteObject(hBmp);
        }
    }

}

void
CreateBitmapBitsTest(int testFunc)
{

    info(ECHO, TEXT("%s -Bitmap Bits test"), funcName[testFunc]);
    TDC     hdc = myGetDC(),
            memDC;
    TBITMAP hBmp,
            hBmpRet;
    BITMAP  bmp;
    UINT    uBmSize;
    int     result;
    WORD    wSquare[16][2] = {

        {0xFF, 0x00}, {0xFF, 0x00}, {0xFF, 0x00}, {0xFF, 0x00}, {0xFF, 0x00},
        {0xFF, 0x00}, {0xFF, 0x00}, {0xFF, 0x00},
        {0x00, 0xFF}, {0x00, 0xFF}, {0x00, 0xFF}, {0x00, 0xFF}, {0x00, 0xFF},
        {0x00, 0xFF}, {0x00, 0xFF}, {0x00, 0xFF}
    };

    hBmp = myCreateBitmap(16, 16, 1, 1, (LPVOID)wSquare);
    memDC = CreateCompatibleDC(hdc);
    SelectObject(memDC, hBmp);
    hBmpRet = (TBITMAP) GetCurrentObject(memDC, OBJ_BITMAP);
    GetObject(hBmpRet, sizeof (BITMAP), &bmp);
    uBmSize = bmp.bmWidthBytes * bmp.bmHeight * bmp.bmPlanes;
    result = IsBadReadPtr(bmp.bmBits, uBmSize);
    if (!result)
        info(FAIL, TEXT("IsBadReadPtr returned true for bmp.bmBits"));

    DeleteDC(memDC);
    DeleteObject(hBmp);
    myReleaseDC(hdc);
}

void
CreateBitmap0SizeTest(int testFunc)
{
    info(ECHO, TEXT("%s - CreateBitmap0SizeTest"), funcName[testFunc]);
    TDC     hdc = myGetDC();
    TBITMAP hBmp;
    BITMAP  bmp;
    int nDepth;

    for(nDepth = 0; nDepth < countof(gBitDepths); nDepth++)
    {
        hBmp = myCreateBitmap(0, 100, 1, gBitDepths[nDepth], NULL);

        if(!hBmp)
            info(FAIL, TEXT("CreateBitmap w:0 h:100 p:1 d:%d b:NULL failed."), gBitDepths[nDepth]);

        memset(&bmp, 0xBAD, sizeof(bmp));
        if(!GetObject(hBmp, sizeof (bmp), &bmp))
            info(FAIL, TEXT("GetObject failed"));
 
        if (bmp.bmType != 0 || bmp.bmWidth != 1 || bmp.bmHeight != 1 || bmp.bmPlanes != 1 || bmp.bmBitsPixel != 1)
            info(FAIL, TEXT("expected: bmType:0 bmWidth:1 bmHeight:1 bmPlanes:1 bmBitsPixel:1  returned:%d %d %d %d %d"), bmp.bmType, bmp.bmWidth,
                 bmp.bmHeight, bmp.bmPlanes, bmp.bmBitsPixel);

        DeleteObject(hBmp);

        
        hBmp = myCreateBitmap(100, 0, 1, gBitDepths[nDepth], NULL);

        if(!hBmp)
            info(FAIL, TEXT("CreateBitmap w:100 h:0 p:1 d:%d b:NULL failed."), gBitDepths[nDepth]);

        memset(&bmp, 0xBAD, sizeof(bmp));
        if(!GetObject(hBmp, sizeof (bmp), &bmp))
            info(FAIL, TEXT("GetObject failed"));

        if (bmp.bmType != 0 || bmp.bmWidth != 1 || bmp.bmHeight != 1 || bmp.bmPlanes != 1 || bmp.bmBitsPixel != 1)
            info(FAIL, TEXT("expected: bmType:0 bmWidth:1 bmHeight:1 bmPlanes:1 bmBitsPixel:1  returned:%d %d %d %d %d"), bmp.bmType, bmp.bmWidth,
                 bmp.bmHeight, bmp.bmPlanes, bmp.bmBitsPixel);

        DeleteObject(hBmp);
    }
    myReleaseDC(hdc);
}

//***********************************************************************************
void
BitBltTest2(int testFunc)
{

    info(ECHO, TEXT("%s - Complex test"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
            compDC;
    TBITMAP hBmp,
            stockBmp;
    RECT    srcRect = { (zx / 2 - UPSIZE) / 2, (zy - UPSIZE) / 2,
        (zx / 2 - UPSIZE) / 2 + UPSIZE, (zy - UPSIZE) / 2 + UPSIZE
    };
    int     result;

    int     tA[4][6] = {
        {zx / 2, 0, zx / 2, zy, 0, 0},
        {zx, 0, -zx / 2, zy, zx / 2, 0},
        {zx / 2, zy, zx / 2, -zy, 0, zy},
        {zx, zy, -zx / 2, -zy, zx / 2, zy},
    };

    hBmp = myLoadBitmap(globalInst, TEXT("UP"));
    compDC = CreateCompatibleDC(hdc);
    stockBmp = (TBITMAP) SelectObject(compDC, hBmp);

    for (int i = 0; i < 4; i++)
    {
        info(DETAIL, TEXT("BitBlt(%d,%d,%d,%d,%d,%d)"), tA[i][0], tA[i][1], tA[i][2], tA[i][3], tA[i][4], tA[i][5]);
        PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
        BitBlt(hdc, srcRect.left, srcRect.top, UPSIZE, UPSIZE, compDC, 0, 0, SRCCOPY);
        result = BitBlt(hdc, tA[i][0], tA[i][1], tA[i][2], tA[i][3], hdc, tA[i][4], tA[i][5], SRCCOPY);
        if (!result)
            info(FAIL, TEXT("returned:%d expected:1"), result);
        CheckScreenHalves(hdc);
        //      Sleep(10);
    }

    SelectObject(compDC, stockBmp);
    DeleteObject(hBmp);
    DeleteDC(compDC);
    myReleaseDC(hdc);
}

void
SimpleEllipseTest(void)
{

    info(ECHO, TEXT("Ellipse - SimpleEllipseTest()"));
    float OldMaxErrors = SetMaxErrorPercentage((float) .00005);
    TDC     hdc = myGetDC();
    int     result;

    SelectObject(hdc, GetStockObject(BLACK_BRUSH));
    SelectObject(hdc, GetStockObject(BLACK_PEN));

    info(ECHO, TEXT("Case1: (30, 50, 30, 100)"));
    result = Ellipse(hdc, 30, 50, 30, 100);
    passCheck(result, 1, TEXT("Ellipse(hdc, 30, 50, 30, 100)"));
    Sleep(1000);
    PatBlt(hdc, 0, 0, zx, zy, WHITENESS);

    info(ECHO, TEXT("Case2: (30, 50, 80, 100)"));
    result = Ellipse(hdc, 30, 50, 80, 100);
    passCheck(result, 1, TEXT("Ellipse(hdc, 30, 50, 80, 100)"));
    Sleep(1000);
    PatBlt(hdc, 0, 0, zx, zy, WHITENESS);

#if 0
    // This is an interesting test, but it just takes a while to do
    // an ellipse so large, so don't run regularlly.
    info(ECHO, TEXT("Case3: (0x4,0x8000004,0x8,0x8000008)"));
    DWORD   dw = GetTickCount();

    result = Ellipse(hdc, 0x4, 0x8000004, 0x8, 0x8000008);
    passCheck(result, 1, TEXT("Ellipse(hdc, 0x4,0x8000004,0x8,0x8000008)"));
    dw = GetTickCount() - dw;
    info(ECHO, TEXT("Case3: (0x4,0x8000004,0x8,0x8000008):  used time = %u seconds"), dw / 1000);
#endif

    info(ECHO, TEXT("Ellipse - SimpleEllipseTest() Done."));
    myReleaseDC(hdc);
    SetMaxErrorPercentage(OldMaxErrors);

}

void
StressShapeTest(int testFunc)
{

    info(ECHO, TEXT("StressShapeTest"), funcName[testFunc]);

    float     OldMaxErrors = SetMaxErrorPercentage((testFunc == EEllipse || testFunc == ERoundRect)? (float) .005 : (float) 0);

    TDC     hdc = myGetDC();
    HBRUSH hbr; 
    HPEN hpn;
    int left, top, right, bottom, width, height;
    int result = 0;


    for(int i=0; i< 30; i++)
    {
        if(!(i%10))
            PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
        hpn = CreatePen(PS_SOLID, 1, randColorRef());
        hbr= CreateSolidBrush(randColorRef());

        SetROP2(hdc, gnvRop2Array[rand() % countof(gnvRop2Array)].dwVal);

        // delete the previous objects and select in the new one
        DeleteObject(SelectObject(hdc, hbr));
        DeleteObject(SelectObject(hdc, hpn));

        // make up some random coordinates to draw the ellipse in
        left = (rand() % (zx + 40)) - 20;
        right = (rand() % (zx + 40)) - 20;
        width = (rand() % (zx + 40)) - 20;

        top = (rand() % (zy + 40)) - 20;
        bottom = (rand() % (zy + 40)) - 20;
        height = (rand() % (zy + 40)) - 20;

        switch(testFunc)
        {
            case EEllipse:
                result = Ellipse(hdc, left, top, right, bottom);
                break;
            case ERectangle:
                result = Rectangle(hdc, left, top, right, bottom);
                break;
            case ERoundRect:
                result = RoundRect(hdc, left, top, right, bottom, width, height);
                break;
        }
        if(!result)
            info(FAIL, TEXT("%s failed, (%d, %d, %d, %d, %d, %d) GetLastError: %d"), funcName[testFunc], left, top, right, bottom, width, height);

    }

    SetROP2(hdc, R2_COPYPEN);

    // delete the last pen/brush selected in.
    DeleteObject(SelectObject(hdc, (HBRUSH) GetStockObject(BLACK_BRUSH)));
    DeleteObject(SelectObject(hdc, (HPEN) GetStockObject(NULL_PEN)));
    
    myReleaseDC(hdc);
    SetMaxErrorPercentage(OldMaxErrors);

}

void
SysmemShapeTest(int testFunc)
{
    info(ECHO, TEXT("%s - SysShapeTest"), funcName[testFunc]);

    float OldMaxErrors = SetMaxErrorPercentage((float) .005);

    TDC hdc = myGetDC();
    TDC hdcCompat = CreateCompatibleDC(hdc);
    TBITMAP hbmp = myCreateBitmap(zx, zy, 1, myGetBitDepth(), NULL);
    TBITMAP hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp);
    HBRUSH hbr; 
    HPEN hpn;
    
    int left, right, top, bottom, width, height;
    int result1 = 0, result2 = 0;

    for(int i=0; i<20; i++)
    {
        hpn = CreatePen(PS_SOLID, 1, randColorRef());
        hbr= CreateSolidBrush(randColorRef());

        int rop2 = gnvRop2Array[rand() % countof(gnvRop2Array)].dwVal;
        SetROP2(hdc, rop2);
        SetROP2(hdcCompat, rop2);

        // delete the previous objects and select in the new one

        // when they're deselected from hdc, they're still selected into hdcCompat
        SelectObject(hdc, hbr);
        SelectObject(hdc, hpn);

        // now they're actually freed and we can delete them.
        DeleteObject(SelectObject(hdcCompat, hbr));
        DeleteObject(SelectObject(hdcCompat, hpn));

        // reset the dc's to white for drawing.
        PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
        PatBlt(hdcCompat, 0, 0, zx, zy, WHITENESS);

        left = (rand() % (zx/2 + 40)) - 20;
        right = (rand() % (zx/2 + 40)) - 20;
        width = (rand() % (zx/2 + 40)) - 20;

        top = (rand() % (zy + 40)) - 20;
        bottom = (rand() % (zy + 40)) - 20;
        height = (rand() % (zy + 40)) - 20;
      
        switch(testFunc)
        {
            case ERectangle:
                result1 = Rectangle(hdc, left, top, right, bottom);
                result2 = Rectangle(hdcCompat, left, top, right, bottom);
            break;
            case ERoundRect:
                result1 = RoundRect(hdc, left, top, right, bottom, width, height);
                result2 = RoundRect(hdcCompat, left, top, right, bottom, width, height);
            break;
            case EEllipse:
                result1 = Ellipse(hdc, left, top, right, bottom);
                result2 = Ellipse(hdcCompat, left, top, right, bottom);
        }
        BitBlt(hdc, zx/2, 0, zx/2, zy, hdcCompat, 0, 0, SRCCOPY);

        if(result1 == NULL)
            info(FAIL, TEXT("%s to the primary left side failed.  GLE:%d"), funcName[testFunc], GetLastError());
        else if(result1 == NULL)
            info(FAIL, TEXT("%s to the offscreen left side failed.  GLE:%d"), funcName[testFunc], GetLastError());
        else CheckScreenHalves(hdc);
    }

    SetROP2(hdc, R2_COPYPEN);
    SetROP2(hdcCompat, R2_COPYPEN);

    SelectObject(hdc, (HBRUSH) GetStockObject(BLACK_BRUSH));
    SelectObject(hdc, (HPEN) GetStockObject(NULL_PEN));
    DeleteObject(SelectObject(hdcCompat, (HBRUSH) GetStockObject(BLACK_BRUSH)));
    DeleteObject(SelectObject(hdcCompat, (HPEN) GetStockObject(NULL_PEN)));
    
    SelectObject(hdcCompat, hbmpStock);
    DeleteObject(hbmp);
    DeleteDC(hdcCompat);
    myReleaseDC(hdc);
    SetMaxErrorPercentage(OldMaxErrors);
}


//***********************************************************************************
void
SimpleRectTest(int testFunc)
{

    info(ECHO, TEXT("%s - Rect and Round"), funcName[testFunc]);
    TDC     hdc = myGetDC();
    int     result;

    // clear the screen for CheckAllWhite
    PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
    
    result = Rectangle(hdc, 50, 50, 50, 100);
    if (!result)
        info(FAIL, TEXT("Rectangle(hdc,  50, 50, 50, 100) returned:%d expected:1"), result);
    result = CheckAllWhite(hdc, 1, 1, 1, 0);
    if (result)
        info(FAIL, TEXT("Rectangle(hdc,  50, 50, 50, 100) expected no drawing"));

    result = Rectangle(hdc, 50, 50, 100, 50);
    if (!result)
        info(FAIL, TEXT("Rectangle(hdc,  50, 50, 100, 50) returned:%d expected:1"), result);
    result = CheckAllWhite(hdc, 1, 1, 1, 0);
    if (result)
        info(FAIL, TEXT("Rectangle(hdc,  50, 50, 100, 50) expected no drawing"));

    result = Rectangle(hdc, 50, 50, 50, 50);
    if (!result)
        info(FAIL, TEXT("Rectangle(hdc,  50, 50, 50, 50) returned:%d expected:1"), result);
    result = CheckAllWhite(hdc, 1, 1, 1, 0);
    if (result)
        info(FAIL, TEXT("Rectangle(hdc,  50, 50, 50, 50) expected no drawing"));
/*
    PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
    result = RoundRect(hdc, 50, 50, 50, 100, 70, 70);
    if (!result)
        info(FAIL, TEXT("RoundRect(hdc,  50, 50, 50, 100, 70, 70) returned:%d expected:1"), result);
    result = CheckAllWhite(hdc, 1, 1, 1, 0);
    if (result)
        info(FAIL, TEXT("RoundRect(hdc,  50, 50, 50, 100, 70, 70) expected no drawing"));
*/
    myReleaseDC(hdc);
}

void
SimpleRectTest2(int testFunc)
{

    info(ECHO, TEXT("%s - right=50 < left=100"), funcName[testFunc]);
    TDC     hdc = myGetDC();
    int     result,
            expected = 50*50;

    result = Rectangle(hdc, 100, 100, 50, 50);
    if (!result)
        info(FAIL, TEXT("Rectangle(hdc,  100, 100, 50, 50) returned:%d expected:1"), result);
    result = CheckAllWhite(hdc, 1, 1, 1, expected);
    if (!result)
        info(FAIL, TEXT("Rectangle(hdc,  100, 100, 50, 50) expected drawing"));

    myReleaseDC(hdc);
}

//***********************************************************************************
void
SimpleBltTest(int testFunc)
{

    info(ECHO, TEXT("%s - SimpleBltTest"), funcName[testFunc]);
    TDC     hdc = myGetDC(),
            memDC = CreateCompatibleDC(hdc);

    TBITMAP hBmp = CreateCompatibleBitmap(hdc, 30, 30),
            stockBmp = (TBITMAP) SelectObject(memDC, hBmp);
    int     result = 0;

    // clear the screen for CheckAllWhite
    PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
    
    switch (testFunc)
    {
        case EBitBlt:
            result = myBitBlt(hdc, 0, 0, 0, 50, memDC, 0, 0, SRCCOPY);
            break;
        case EMaskBlt:
            result = myMaskBlt(hdc, 0, 0, 0, 50, memDC, 0, 0, NULL, 0, 0, SRCCOPY);
            break;
        case EStretchBlt:
            result = myStretchBlt(hdc, 0, 0, 0, 50, hdc, 0, 0, 0, 50, SRCCOPY);
            break;
    }

    if (!result)
        info(ECHO, TEXT("%s - failed given dest size(0,50)"), funcName[testFunc]);

    CheckAllWhite(hdc, 0, 0, 0, 0);

    SelectObject(memDC, stockBmp);
    DeleteObject(hBmp);
    DeleteDC(memDC);
    myReleaseDC(hdc);
}

//***********************************************************************************
void
SimpleCreateDIBSectionTest2(int testFunc)
{

    info(ECHO, TEXT("%s - test2"), funcName[testFunc]);
    TDC     hdc = myGetDC();
    TBITMAP hBmp0,
            hBmp1;

    struct
    {
        BITMAPINFOHEADER bmih;
        RGBQUAD ct[4];
    }
    bmi;

    memset(&bmi.bmih, 0, sizeof (BITMAPINFOHEADER));
    bmi.bmih.biSize = sizeof (BITMAPINFOHEADER);
    bmi.bmih.biWidth = 50;
    bmi.bmih.biHeight = -40;
    bmi.bmih.biPlanes = 1;
#ifdef UNDER_CE
    bmi.bmih.biBitCount = 2;
#else
    bmi.bmih.biBitCount = 1;
#endif
    bmi.bmih.biCompression = BI_RGB;
    setRGBQUAD(&bmi.ct[0], 0x0, 0x0, 0x0);
    setRGBQUAD(&bmi.ct[1], 0x55, 0x55, 0x55);
    setRGBQUAD(&bmi.ct[2], 0xaa, 0xaa, 0xaa);
    setRGBQUAD(&bmi.ct[3], 0xff, 0xff, 0xff);

    BYTE   *lpBits = NULL;

    hBmp0 = CreateDIBSection(hdc, (LPBITMAPINFO) & bmi, DIB_RGB_COLORS, (VOID **) & lpBits, NULL, NULL);
    hBmp1 = CreateDIBSection(hdc, (LPBITMAPINFO) & bmi, DIB_RGB_COLORS, NULL, NULL, NULL);

    if(!hBmp0)
        info(FAIL, TEXT("Failed to create a 2bpp bitmap with lpBits=NULL passed in."));

    if(!hBmp1)
        info(FAIL, TEXT("Failed to create a 2bpp bitmap with NULL lpBits passed in."));


    DeleteObject(hBmp0);
    DeleteObject(hBmp1);
    myReleaseDC(hdc);
}

void
DrawEdgeTest1(int testFunc)
{

    info(ECHO, TEXT("%s - passing Invalid Edge type:"), funcName[testFunc]);
    TDC     hdc = myGetDC();
    RECT    rc = { 50, 50, 100, 100 };

    mySetLastError();
    DrawEdge(hdc, &rc, 0xFFFFFFFF, BF_RECT);
    myGetLastError(NADA);       // Added for NT1381 ERROR_INVALID_PARAMETER);

    myReleaseDC(hdc);
}

void
DrawEdgeTest2(int testFunc)
{

    info(ECHO, TEXT("%s - EDGE_RASIED: BF_MIDDLE: expect button color"), funcName[testFunc]);
    int     x,
            y;
    TDC     hdc = myGetDC();
    RECT    rc = { 50, 50, 125, 125 };
    COLORREF color = (COLORREF)-1,
            expected;

    expected = (COLORREF)GetSysColor(COLOR_BTNFACE);
    PatBlt(hdc, 0, 0, zx, zy, BLACKNESS);
    mySetLastError();
    if(!DrawEdge(hdc, &rc, EDGE_RAISED, BF_MIDDLE | BF_RECT))
        info(FAIL, TEXT("DrawEdge failed for hdc, &rc, EDGE_RAISED, BF_MIDDLE | BF_RECT"));
    myGetLastError(NADA);

    Sleep(2000);
    x = 75, y = 75;
    if (!g_fPrinterDC)
    {
        color = GetPixel(hdc, x, y);
    }
    else
    {
        // Since GetPixel does not work on printer DC's, just substitute the expected.
        color = expected;
    }
    info(ECHO, TEXT("GetPixel(hdc,%d,%d) returned:0x%X"), x, y, color);

    if (color != expected)
    {
        COLORREF color2,
                expected2;

        // check again:

        color2 = GetNearestColor(hdc, color);
        expected2 = GetNearestColor(hdc, expected);

        if (color2 != expected2)
            info(FAIL, TEXT("Returned:%X Expected:%X ---Nearestcolor: Returned:%X Expected:%X  "), color, expected, color2,
                 expected2);
    }

    myReleaseDC(hdc);

}

void
DrawEdgeTest3(int testFunc)
{

    info(ECHO, TEXT("%s - EDGE_RAISED: BF_FLAT: expect white color"), funcName[testFunc]);
    TDC     hdc = myGetDC();
    RECT    rc = { 50, 50, 100, 100 };
    COLORREF color;

    // clear the surface to check for a white pixel
    PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
    mySetLastError();
    if(!DrawEdge(hdc, &rc, EDGE_RAISED, BF_FLAT | BF_RECT))
        info(FAIL, TEXT("DrawEdge failed for hdc, &rc, EDGE_RAISED, BF_FLAT | BF_RECT"));

    myGetLastError(NADA);

    color = GetPixel(hdc, 75, 75);
    info(ECHO, TEXT("GetPixel(hdc,75,75) returned:0x%X"), color);

    // Expected white center:
    if (!g_fPrinterDC && !isWhitePixel(hdc, color, 75, 75, 1))
        info(FAIL, TEXT("returned 0x%lX expected 0x%lX when Expected white center"), color, RGB(255, 255, 255));

    myReleaseDC(hdc);

#ifdef VISUAL_CHECK
    MessageBox(NULL, TEXT("Check"), TEXT("Check"), MB_OK | MB_SETFOREGROUND);
#endif
}

void
IterateDrawEdge()
{
    info(ECHO, TEXT("IterateDrawEdge"));

    TDC hdc = myGetDC();
    
    NameValPair InnerEdge[] = { NAMEVALENTRY(BDR_RAISEDINNER), 
                                                  NAMEVALENTRY(BDR_SUNKENINNER) };
    
    NameValPair OuterEdge[] = { NAMEVALENTRY(BDR_RAISEDOUTER), 
                                                  NAMEVALENTRY(BDR_SUNKENOUTER) };
    
    NameValPair Type1[] = {  NAMEVALENTRY(BF_TOP),
                                            NAMEVALENTRY(BF_BOTTOM),
                                            NAMEVALENTRY(BF_LEFT),
                                            NAMEVALENTRY(BF_RIGHT),
                                            NAMEVALENTRY(BF_RECT) };

    NameValPair Type2[] = { NAMEVALENTRY(BF_MIDDLE),
                                           NAMEVALENTRY(BF_DIAGONAL), 
                                           NAMEVALENTRY(BF_MIDDLE | BF_DIAGONAL) };
                            
    NameValPair Type3[] = { NAMEVALENTRY(BF_SOFT),
                                           NAMEVALENTRY(BF_ADJUST),
                                           NAMEVALENTRY(BF_FLAT),
                                           NAMEVALENTRY(BF_MONO),
                                           NAMEVALENTRY(BF_SOFT | BF_ADJUST | BF_FLAT | BF_MONO) };
    
    RECT rc;
    RECT rctmp;

    PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
    
    //TODO: some form of verification that the edges look right.
    for(int i = 0; i < countof(InnerEdge); i++)
        for(int j = 0; j < countof(OuterEdge); j++)
        {
            info(DETAIL, TEXT("Testing inner edge:%s outer edge:%s"), InnerEdge[i].szName, OuterEdge[j].szName);
            for(int k = 0; k < countof(Type1); k++)
                for(int l = 0; l < countof(Type2); l++)
                    for(int m = 0; m < countof(Type3); m++)
                    {
                        // get our width/height
                        rc.right = (rand() % (zx - 10)) + 20;
                        rc.bottom = (rand() % (zy - 10)) + 20;
                        // get our top left
                        rc.left = (rand()%(zx + 10 - rc.right)) - 5;
                        rc.top = (rand()%(zy + 10 - rc.bottom)) - 5;
                        // adjust width/height by the new upper left
                        rc.right += rc.left;
                        rc.bottom += rc.top;
                        
                        rctmp = rc;
                        if(!DrawEdge(hdc, &rctmp, InnerEdge[i].dwVal | OuterEdge[j].dwVal, Type1[k].dwVal | Type2[l].dwVal | Type3[m].dwVal))
                            info(FAIL, TEXT("DrawEdge failed"));

                        PatBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, WHITENESS);
                        if(CheckAllWhite(hdc, 0, 1, 1, 0))  // quiet CheckAllWhite, we'll handle the error here.
                        {
                            info(FAIL, TEXT("Rectangle drawn too large %s | %s, %s | %s | %s"), InnerEdge[i].szName, OuterEdge[j].szName, Type1[k].szName, Type2[l].szName, Type3[m].szName);
                            info(FAIL, TEXT("Expected to be within top:%d left:%d bottom:%d right:%d"), rc.top, rc.left, rc.bottom, rc.right);
                            PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
                        }
                    }
        }
    myReleaseDC(hdc);
}

void
TestBltInRegion(void)
{

    info(ECHO, TEXT("TestBltInRegion"));

    // if the screen/surface is smaller than the bitmaps we're testing, don't test, not enough room.
    if(SetWindowConstraint(3*PARIS_X, PARIS_Y))
    {
        
        TDC     hdc = myGetDC(),
                compDC;
        HRGN    hRgn;
        TBITMAP hOrigBitmap,
                hBitmap = myLoadBitmap(globalInst, TEXT("PARIS0"));
        int     i,
                shift[9][2] = { {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}, {1, 0},
        {0, 0}
        };
        RECT    r;

        // make sure our width is divisable by 4
        zx = (zx/4)*4;

        if (!hBitmap)
            info(FAIL, TEXT("hBitmap NULL"));

        PatBlt(hdc, 0, 0, zx, zy, BLACKNESS);

        // set up region
        hRgn = CreateRectRgn(5, 5, zx - 5, zy - 5);;
        GetRgnBox(hRgn, &r);
        info(ECHO, TEXT("ClipBox=[%d %d %d %d]"), r.left, r.top, r.right, r.bottom);

        // set up bitmaps
        compDC = CreateCompatibleDC(hdc);
        hOrigBitmap = (TBITMAP) SelectObject(compDC, hBitmap);

        for (i = 0; i < 1; i++)
        {
            info(DETAIL, TEXT("Testing shift [%d,%d]"), shift[i][0], shift[i][1]);
            SelectClipRgn(hdc, NULL);
            PatBlt(hdc, 0, 0, zx, zy, BLACKNESS);
            BitBlt(hdc, (zx / 2 - PARIS_X) / 2 - RESIZE(20) * shift[i][0], (zy - PARIS_Y) / 2 - RESIZE(20) * shift[i][1], PARIS_X,
                   PARIS_Y, compDC, 0, 0, SRCCOPY);
            BitBlt(hdc, (zx / 2 - PARIS_X) / 2 + zx / 2, (zy - PARIS_Y) / 2, PARIS_X, PARIS_Y, compDC, 0, 0, SRCCOPY);
            SelectClipRgn(hdc, hRgn);
            BitBlt(hdc, RESIZE(20) * shift[i][0], RESIZE(20) * shift[i][1], zx / 2, zy, hdc, 0, 0, SRCCOPY);
            CheckScreenHalves(hdc);
        }

        SelectClipRgn(hdc, NULL);
        DeleteObject(hRgn);
        SelectObject(compDC, hOrigBitmap);
        DeleteDC(compDC);
        DeleteObject(hBitmap);
        myReleaseDC(hdc);
        
        // restore the default constraint
        SetWindowConstraint(0,0);
    }
    else info(DETAIL, TEXT("Screen too small for test"));
}

void
TestSBltCase10(void)
{
    if (g_fPrinterDC)
    {
        info(ECHO, TEXT("TestSBltCase10 test invalid on printer DC"));
        return;
    }

    // top is the top of the rectangle being copied
    const int top = 85;
    TDC     hdc = myGetDC();
    // height is the height of the rectangle being copied
    int height = min(200, zy - top);
    info(ECHO, TEXT("TestSBltCase10: StretchBlt(hdc,100,84,10,%d,hdc,100,85,10,%d,SRCCOPY"), height, height);
    // using the height as min(200, zy - top) keeps the test from pulling unknown data from below the bottom of the box
    if (!StretchBlt(hdc, 100, top - 1, 10, height, hdc, 100, top, 10, height, SRCCOPY))
        info(FAIL, TEXT("StretchBlt call Failed: err=%ld"), GetLastError());
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   StretchBlt Test
***
************************************************************************************/
#define microSquare    1

//***********************************************************************************
void
StretchBltTest(int testFunc)
{

    info(ECHO, TEXT("%s - StretchBlt1 Test"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
            srcDC = CreateCompatibleDC(hdc);
    TBITMAP hOrigBitmap;
    TBITMAP hSrc = CreateCompatibleBitmap(hdc, zx / 4, zy / 2);
    int     x,
            y,
            m,
            result;

    // set up src
    hOrigBitmap = (TBITMAP) SelectObject(srcDC, hSrc);
    myPatBlt(srcDC, 0, 0, zx, zy, WHITENESS);
    for (x = 0; x < 10; x++)
        for (y = 0; y < 10; y++)
            if ((x + y) % 2 == 0)
                myPatBlt(srcDC, x * microSquare, y * microSquare, microSquare, microSquare, BLACKNESS);

    for (m = 1; m < 15; m++)
    {
        myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);
        result =
            StretchBlt(hdc, 0, 0, microSquare * 10 * m, microSquare * 10 * m, srcDC, 0, 0, microSquare * 10, microSquare * 10,
                       SRCCOPY);

        // for small device: 240*320 or 208 x240:  StretchBlt passes the middle line of the screen
        myPatBlt(hdc, zx / 2, 0, zx, zy, WHITENESS);

        for (x = 0; x < 10; x++)
            for (y = 0; y < 10; y++)
                if ((x + y) % 2 == 0)
                {
                    myPatBlt(hdc, x * microSquare * m + zx / 2, y * microSquare * m, microSquare * m, microSquare * m,
                             BLACKNESS);
                }
        //            Sleep(1);
        CheckScreenHalves(hdc);
    }

    SelectObject(srcDC, hOrigBitmap);
    DeleteObject(hSrc);
    myReleaseDC(hdc);
    DeleteDC(srcDC);
}

//***********************************************************************************
void
StretchBltTest2(int testFunc)
{

    info(ECHO, TEXT("%s - StretchBlt2 Test"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
            srcDC = CreateCompatibleDC(hdc);
    TBITMAP hOrigBitmap;
    TBITMAP hSrc = CreateCompatibleBitmap(hdc, zx / 4, zy / 2);
    int     x,
            y,
            result;

    // set up src
    hOrigBitmap = (TBITMAP) SelectObject(srcDC, hSrc);
    myPatBlt(srcDC, 0, 0, zx, zy, WHITENESS);
    for (x = 0; x < 10; x++)
        for (y = 0; y < 10; y++)
            if ((x + y) % 2 == 0)
                myPatBlt(srcDC, x * microSquare, y * microSquare, microSquare, microSquare, BLACKNESS);

    myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);
    Rectangle(hdc, 10 - 1, 10 - 1, 110 + 1, 110 + 1);
    result =
        StretchBlt(hdc, 10, 10, microSquare * 10 * 10, microSquare * 10 * 10, srcDC, 0, 0, microSquare * 10, microSquare * 10,
                   SRCCOPY);
    //    Sleep(3000);

    SelectObject(srcDC, hOrigBitmap);
    DeleteObject(hSrc);
    myReleaseDC(hdc);
    DeleteDC(srcDC);
}

//***********************************************************************************
void
StretchBltTest3(int testFunc)
{

    info(ECHO, TEXT("%s - StretchBlt3 Test"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
            srcDC = CreateCompatibleDC(hdc);
    TBITMAP hOrigBitmap;
    TBITMAP hSrc = CreateCompatibleBitmap(hdc, zx / 4, zy / 2);
    POINT   squares = { zx / 4 / aSquare, zy / 2 / aSquare };
    int     x,
            y,
            result;

    // set up src
    hOrigBitmap = (TBITMAP) SelectObject(srcDC, hSrc);
    myPatBlt(srcDC, 0, 0, zx / 2, zy, WHITENESS);
    for (x = 0; x < squares.x; x++)
        for (y = 0; y < squares.y; y++)
            if ((x + y) % 2 == 0)
                myPatBlt(srcDC, x * aSquare, y * aSquare, aSquare, aSquare, BLACKNESS);

    for (x = 0; x <= zx; x += 40)
        for (y = 0; y <= zy; y += 40)
        {
            result = StretchBlt(hdc, 0, 0, x, y, srcDC, 0, 0, zx / 4, zy / 2, SRCCOPY);
            if (!result)
                info(FAIL, TEXT("StretchBlt(%X,%d,%d,%d,%d,%X,%d,%d,%d,%d)"), hdc, 0, 0, x, y, srcDC, 0, 0, zx / 4, zy / 2);
        }
    SelectObject(srcDC, hOrigBitmap);
    DeleteObject(hSrc);
    myReleaseDC(hdc);
    DeleteDC(srcDC);
}

//***********************************************************************************
void
BlitOffScreen(void)
{

    info(ECHO, TEXT("OffScreen Blit"));
    // this test blt's from the corners of the screen for testing clipping, which
    // grabs portions of the taskbar and puts them on the screen.

    if (g_fPrinterDC)
    {
        info(ECHO, TEXT("Test invalid on printer DC"));
        return;
    }
    
    BOOL OldSurfVerif = SetSurfVerify(FALSE);

    TDC     hdc = myGetDC();
    int     result;

    setTestRects();

    for (int t = 0; t < NUMRECTS; t++)
    {
        decorateScreen(hdc);
        myBitBlt(hdc, zx / 2, 0, zx / 2, zy, hdc, 0, 0, SRCCOPY);
        result =
            StretchBlt(hdc, rTest[8].left, rTest[8].top, zx / 2, zy / 2, hdc, rTest[t].left, rTest[t].top, zx / 2, zy / 2,
                       SRCCOPY);
        if (!result)
            info(FAIL, TEXT("rect[%d]: result:0 expected:1"), t);
        //        Sleep(1);
    }

    myReleaseDC(hdc);
    // verification must be turned off until after myReleaseDC
    SetSurfVerify(OldSurfVerif);
}

/***********************************************************************************
***
***   Check ViewPort Change
***
************************************************************************************/

//***********************************************************************************
void
ViewPort(int testFunc)
{
    // REVIEW add other drawing routines
    info(ECHO, TEXT("%s - Check ViewPort Change"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    // ellipse can have a small number of differences due to hardware differences.
    float OldMaxErrors = SetMaxErrorPercentage(testFunc == EEllipse ? (float) .0005 : (float) 0);
    
    SetViewportOrgEx(hdc, 0, 0, NULL);
    SelectObject(hdc, (HBRUSH) GetStockObject(BLACK_BRUSH));
    SelectObject(hdc, (HPEN) GetStockObject(BLACK_PEN));
    switch (testFunc)
    {
        case ERectangle:
            Rectangle(hdc, 30, 30, 60, 60);
            break;
        case EEllipse:
            Ellipse(hdc, 30, 30, 60, 60);
            break;
        case ERoundRect:
            RoundRect(hdc, 30, 30, 60, 60, 0, 0);
            break;
    }
    SetViewportOrgEx(hdc, zx / 2, 0, NULL);

    switch (testFunc)
    {
        case ERectangle:
            Rectangle(hdc, 30, 30, 60, 60);
            break;
        case EEllipse:
            Ellipse(hdc, 30, 30, 60, 60);
            break;
        case ERoundRect:
            RoundRect(hdc, 30, 30, 60, 60, 0, 0);
            break;
    }
    SetViewportOrgEx(hdc, 0, 0, NULL);
    CheckScreenHalves(hdc);
    myReleaseDC(hdc);
    // error tolerances must be left active until after myReleaseDC
    SetMaxErrorPercentage(OldMaxErrors);
}

//***********************************************************************************
void
BitBltTest3(int testFunc)
{

    info(ECHO, TEXT("%s: DestDC: width = 0"), funcName[testFunc]);
    TDC     hdc = myGetDC(),
            memDC = CreateCompatibleDC(hdc);

    TBITMAP hBmp = CreateCompatibleBitmap(hdc, 30, 30),
            stockBmp = (TBITMAP) SelectObject(memDC, hBmp);
    int     result = 0;

    result = BitBlt(hdc, 0, 0, 0, 50, memDC, 0, 0, SRCCOPY);

    if (!result)
        info(ECHO, TEXT("failed given dest size(0,50)"));

    CheckScreenHalves(hdc);

    SelectObject(memDC, stockBmp);
    DeleteObject(hBmp);
    DeleteDC(memDC);
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   ROP Tests
***
************************************************************************************/

//***********************************************************************************
void
SetRop2Test(void)
{

    info(ECHO, TEXT("SetRop2Test"));

    TDC     hdc = myGetDC();
    DWORD result;
    int         numRops = countof(gnvRop2Array);

    SetROP2(hdc, R2_COPYPEN);
    // good rops
    for (int r = 0; r < numRops; r++)
    {
        result = SetROP2(hdc, gnvRop2Array[r].dwVal);
        if (r > 0 && result != gnvRop2Array[r - 1].dwVal)
            info(FAIL, TEXT("SetROP2 failed to return previous set: result:%d expected:%d (%s) [%d]"), result,
                 gnvRop2Array[r - 1].dwVal, gnvRop2Array[r - 1].szName, r);
    }

    SetROP2(hdc, R2_COPYPEN);
    myReleaseDC(hdc);
}

void FAR PASCAL
SimpleTransparentImageTest(void)
{

#ifndef UNDER_CE
    info(ECHO, TEXT("SimpleTransparentImageTest --- CE only"));
#else
    info(ECHO, TEXT("SimpleTransparentImageTest ---"));

#define MYBLACKRGB RGB(0,0,0)
#define MYWHITERGB RGB(255,255,255)
#define MYGREENRGB RGB(0,255,0)
#define MYREDRGB RGB(255,0,0)
    TDC     hdc, comphdc = NULL;
    TBITMAP bmp = NULL, stockbmp = NULL;
    int     nRet;
    HBRUSH  hbrush,
            hredbrush;
    COLORREF cl,
            cl1;
    RECT    rc;
    RECT    greenRect,
            redRect;
    DWORD dwError = 0;        

    hdc = myGetDC();
    if (g_fPrinterDC)
    {
        comphdc = CreateCompatibleDC(hdc);
        if (!comphdc)
        {
            dwError = GetLastError();
            if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
                info(DETAIL, TEXT("BitBltOffScreen: CreateCompatibleDC() failed: out of memory: GLE:%d"), dwError);
            else
                info(FAIL, TEXT("BitBltOffScreen: CreateCompatibleDC() failed:  GLE:%d"), dwError);
            myReleaseDC(hdc);
            return;
        }
        bmp = CreateCompatibleBitmap(hdc, zx, zy);
        if (!bmp)
        {
            dwError = GetLastError();
            if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
                info(DETAIL, TEXT("BitBltOffScreen: CreateCompatibleDC() failed: out of memory: GLE:%d"), dwError);
            else
                info(FAIL, TEXT("BitBltOffScreen: CreateCompatibleDC() failed:  GLE:%d"), dwError);
            DeleteDC(comphdc);    
            myReleaseDC(hdc);
            return;
        }
        stockbmp = SelectObject(comphdc, bmp);
    }
    else
        comphdc = hdc;
        
    rc.left = 0;
    rc.top = 0;
    rc.bottom = zy;
    rc.right = zx;

    // There appears to be an issue where this test fails when using mygetdc(ghwndtopmain), and this test
    // is run after some previous tests in the suite.
    //hdc = myGetDC(ghwndTopMain);
    //GetClientRect(ghwndTopMain, &rc);

    if (FillRect(comphdc, &rc, (HBRUSH) GetStockObject(WHITE_BRUSH)) == 0)
    {
        info(FAIL, TEXT("FillRect (white) failed: err=%ld: "), GetLastError());
    }
    Sleep(200);

    //rc.right -= 20;
    //rc.bottom -= 20;
    if (FillRect(comphdc, &rc, (HBRUSH) GetStockObject(BLACK_BRUSH)) == 0)
    {
        info(FAIL, TEXT("FillRect (black) failed: err=%ld: "), GetLastError());
    }
    Sleep(200);

    int     qWidth = (rc.right - rc.left) / 2;
    int     qHeight = (rc.bottom - rc.top) / 2;

    hbrush = CreateSolidBrush(MYGREENRGB);
    greenRect = rc;
    greenRect.left += qWidth;
    greenRect.top += qHeight;
    if (FillRect(comphdc, &greenRect, hbrush) == 0)
    {
        info(FAIL, TEXT("FillRect (green) failed: err=%ld: "), GetLastError());
    }
    Sleep(200);

    hredbrush = CreateSolidBrush(MYREDRGB);
    redRect = greenRect;
    redRect.left += qWidth / 2;
    redRect.top += qHeight / 2;
    if (FillRect(comphdc, &redRect, hredbrush) == 0)
    {
        info(FAIL, TEXT("FillRect (red) failed: err=%ld: "), GetLastError());
    }
    Sleep(200);

    SetLastError(0);
    nRet =
        TransparentImage(hdc, rc.left, rc.top, qWidth, qHeight, comphdc, greenRect.left, greenRect.top, qWidth, qHeight,
                         MYGREENRGB);
    if (0 == nRet)
    {
        info(FAIL, TEXT("TransparentImage() failed: err=%ld: "), GetLastError());
    }
    else if (g_fPrinterDC)
    {
        // Since GetPixel will not work on a printer DC, we skip checking if TransparentImage did the right thing.
        info(ECHO, TEXT("TransparentImage check skipped on printer DC"));
    }
    else
    {
        int     x = rc.left + qWidth / 2 + 2,
                y = rc.top + qHeight / 2 + 2;

        cl1 = GetPixel(hdc, x, y);

        // compare to the red portion of gcrWhite, which is the red value taking masks into account
        if (GetRValue(cl1) != GetRValue(gcrWhite) || GetGValue(cl1) != 0 || GetBValue(cl1) != 0)
        {
            info(FAIL, TEXT("TransparentImage: at [%d,%d] expect color = RED: but get 0x%08X"), x, y, cl1);
        }
        else
        {
            info(ECHO, TEXT("TransparentImage check succeeded: at [%d,%d] expect color=RED= 0x%lX "), x, y, cl1);
        }
    }
    Sleep(400);

    // Check result
    int     x = rc.left + 2,
            y = rc.top + 2;

    if (!g_fPrinterDC)
    {
        cl = GetPixel(hdc, x, y);
        if (cl != MYBLACKRGB)       // An RGB value of 0 is Black
        {
            info(FAIL, TEXT("TransparentImage: at [%d,%d] expect color = Black: but get 0x%08X"), x, y, cl);
        }
        else
        {
            info(ECHO, TEXT("TransparentImage check succeeded: at [%d,%d] expect color=Black=0x%lX "), x, y, cl);
        }
    }

    myReleaseDC(hdc);
    if (comphdc != hdc)
    {
        SelectObject(comphdc, stockbmp);
        DeleteDC(comphdc);
        DeleteObject(bmp);
    }
    DeleteObject(hbrush);
    DeleteObject(hredbrush);
#endif // UNDER_CE
}

void
TransparentBltErrorTest(void)
{
    info(ECHO, TEXT("TransparentImageErrorTest"));

    TDC hdc = myGetDC();
    int r, g, b;

    // When paletted, there's no guarantee what colors will/won't be available.  Solid colors
    // should be different enough to not color key.
    if(myGetBitDepth() < 16)
    {
        r = 255;
        g = 255;
        b = 255;
    }
    else
    {
        // plus 8 to take into account 24bpp to 16bpp conversions, 
        // and the fact that we don't want r, g, or b, to be 0, because we're testing with them
        // at 0
        // 0 - 247 to take into account the + 8.
        r = (rand() % 247) + 8,
        g = (rand() % 247) + 8,
        b = (rand() % 247) + 8;
    }

    RECT rc = { 0, 0, zx/2, zy};
    HBRUSH hbr = CreateSolidBrush(RGB(r, g, b));
    DWORD rgb[] = { {RGB(r, 0, 0)},
                               {RGB(0, g, 0)},
                               {RGB(0, 0, b)},
                               {RGB(r, g, 0)},
                               {RGB(0, g, b)},
                               {RGB(r, 0, b)}};
    if(!hbr)
        info(ABORT, TEXT("Failed to create brush for test"));
    
    FillRect(hdc, &rc, hbr);

    for(int i = 0; i < countof(rgb); i++)
    {
        PatBlt(hdc, zx/2, 0, zx/2, zy, BLACKNESS);
        TransparentBlt(hdc, zx/2, 0, zx/2, zy, hdc, 0, 0, zx/2, zy, rgb[i]);
        if(!CheckScreenHalves(hdc))
            info(FAIL, TEXT("color incorrectly made transparent.  Source color (0x%x, 0x%x, 0x%x), transparent color (0x%x, 0x%x, 0x%x)."), 
                                                                                                r, g, b, rgb[i] & 0xFF, (rgb[i] >> 8) & 0xFF, (rgb[i] >> 16) & 0xFF);
    }
    
    DeleteObject(hbr);
    myReleaseDC(hdc);
}

void
TransparentBltSetPixelTest()
{
    info(ECHO, TEXT("TransparentBlt SetPixel Transparency test"));

    if (!g_fPrinterDC)
    {
        TDC hdc = myGetDC();
        COLORREF cr, crWhite, crBlack;

        // ****  from pixel 0 to pixel 1 with white as transparent
        SetPixel(hdc, 0, 0, RGB(255, 255, 255));
        SetPixel(hdc, 0, 1, RGB(0, 0, 0));

        // set up our black and white pixels
        crWhite = GetPixel(hdc, 0, 0);
        crBlack = GetPixel(hdc, 0, 1);
        
        TransparentBlt(hdc, 0, 1, 1, 1, hdc, 0, 0, 1, 1, crWhite);
        cr = GetPixel(hdc, 0, 1);
        if(cr != crBlack)
            info(FAIL, TEXT("white incorrectly made opaq"));

        // **** from pixel 0 to pixel 1 with black as transparent

        SetPixel(hdc, 0, 0, RGB(255, 255, 255));
        SetPixel(hdc, 0, 1, RGB(0, 0, 0));

        // set up our black and white pixels
        crWhite = GetPixel(hdc, 0, 0);
        crBlack = GetPixel(hdc, 0, 1);
        
        TransparentBlt(hdc, 0, 1, 1, 1, hdc, 0, 0, 1, 1, crBlack);
        cr = GetPixel(hdc, 0, 1);
        if(cr != crWhite)
            info(FAIL, TEXT("white incorrectly made transparent"));

        // **** from pixel 1 to pixel 0 with black as transparent
        
        SetPixel(hdc, 0, 0, RGB(255, 255, 255));
        SetPixel(hdc, 0, 1, RGB(0, 0, 0));

        TransparentBlt(hdc, 0, 0, 1, 1, hdc, 0, 1, 1, 1, crBlack);
        cr = GetPixel(hdc, 0, 0);
        if(cr != crWhite)
            info(FAIL, TEXT("black incorrectly made opaq"));

        // **** from pixel 1 to pixel 0 with white as transparent

        SetPixel(hdc, 0, 0, RGB(255, 255, 255));
        SetPixel(hdc, 0, 1, RGB(0, 0, 0));

        TransparentBlt(hdc, 0, 0, 1, 1, hdc, 0, 1, 1, 1, crWhite);
        cr = GetPixel(hdc, 0, 0);
        if(cr != crBlack)
            info(FAIL, TEXT("black incorrectly made transparent"));
        
        myReleaseDC(hdc);
    }
}

void
TransparentBltBitmapTest()
{
    info(ECHO, TEXT("TransparentBltBitmapTest"));
    
    TDC hdc = myGetDC();
    TDC hdcCompat = CreateCompatibleDC(hdc);
    TBITMAP tbmp = NULL, tbmpStock = NULL;
    BOOL result;
    
    drawLogo(hdc, 150);

    for(int i = 0; i < countof(gBitDepths); i++)
    {
        result = TRUE;
        
        tbmp = myCreateBitmap(zx/2, zy, 1, gBitDepths[i], NULL);
        tbmpStock = (TBITMAP) SelectObject(hdcCompat, tbmp);
        result &= BitBlt(hdcCompat, 0, 0, zx/2, zy, hdc, 0, 0, SRCCOPY);

#ifndef UNDER_CE
        // desktop doesn't support a transparent blt from a bitmap to a dc, so just use the dc for this test
        result &= TransparentBlt(hdc, zx/2, 0, zx/2, zy, hdcCompat, 0, 0, zx/2, zy, RGB(0,0,0));
#endif
        SelectObject(hdcCompat, tbmpStock);
#ifdef UNDER_CE
        // ce supports a transparent blt from a bitmap to a dc, so let's hit that code path.
        result &= TransparentBlt(hdc, zx/2, 0, zx/2, zy, tbmp, 0, 0, zx/2, zy, RGB(0,0,0));
 #endif
        DeleteObject(tbmp);

        if(!result)
            info(FAIL, TEXT("A BitBlt or TransparentBlt call failed."));
    }

    DeleteDC(hdcCompat);
    myReleaseDC(hdc);
}

// number of entrys in the array of rectangles to use for invert testing
#define INVTESTCOUNT 5
// number of raster op's in the rop arrays.
#define NUMROPS 14

void FillSurface(TDC hdc)
{
    int nWidth = 3;
    int nHeight = 3;
    int nBlockWidth = 9;
    int nBlockHeight = 9;
    TDC hdcTmp = CreateCompatibleDC(hdc);
    TBITMAP hbmp = CreateCompatibleBitmap(hdc, nWidth * nBlockWidth, nHeight * nBlockHeight);
    TBITMAP hbmpOld = (TBITMAP) SelectObject(hdcTmp, hbmp);
    HBRUSH hbrPattern, hbrOld;
    
    PatBlt(hdcTmp, 0, 0, nWidth * nBlockWidth, nHeight * nBlockHeight, WHITENESS);
    
    for(int i=0; i< nWidth; i++)
        for(int j=0; j< nHeight; j++)
        {
            COLORREF cr = randColorRef();
            for(int k=0; k< nBlockWidth; k++)
                for(int l=0; l< nBlockHeight; l++)
                    SetPixel(hdcTmp, (i * nBlockWidth) + k, (j * nBlockHeight) + l, cr);
        }

    SelectObject(hdcTmp, hbmpOld);

    hbrPattern = CreatePatternBrush(hbmp);

    hbrOld = (HBRUSH) SelectObject(hdc, hbrPattern);

    PatBlt(hdc, 0, 0, zx/2, zy, PATCOPY);
    
    SelectObject(hdc, hbrOld);
    DeleteObject(hbrPattern);
    DeleteObject(hbmp);
    DeleteDC(hdcTmp);
}

// this function attempts to be a comprehensive blitting suite of overlapped and clipped operations.
// it tests blt clipping in 8 directions by 1 pixel, 10 pixels, completely off, and 1 pixel left
// it tests overlapped blts that are not clipped in 8 directions
// it tests overlapped blt's that are only overlapped by 1 pixel, which also results in a portion of the blt clipped

void
BitBltSuite(int testFunc)
{
    info(ECHO, TEXT("BitBltSuite - %s"), funcName[testFunc]);
    TDC hdc = myGetDC(), hdcOffscreen = CreateCompatibleDC(hdc);
    TBITMAP hbmp = CreateCompatibleBitmap(hdc, zx/2, zy), hbmpOld, hbmpMask;
    
    // block size to blt
    int nBlockWidth = zx/4;
    int nBlockHeight = zy/2;
    // location to pull the cube from the left side
    int nCubex = zx/8;
    int nCubey = zy/4;
    DWORD dwRop = 0;
    BOOL bResult = TRUE;
    HBRUSH hBrush = NULL, hBrushStock1 = NULL, hBrushStock2 = NULL;
    int depth = 0;
    // no brush used for now.
    int type = 0;
    // 8x8 is the most common brush, so only use that.
    int brushSize = 8;
    POINT pt;

    // these variables are used in MaskBlt for calculating the mask position.
    int nMaskx = 0, nMasky = 0;
#ifndef UNDER_CE // used for modifying mask position based on rop when running on XP.  possible issue with CE MaskBlt not behaving the same, under investigation.
    int nForegroundRop = 0, nBackgroundRop = 0;
    BOOL bForeNeedSource = FALSE;
    BOOL bBackNeedSource = FALSE;
#endif
    // set up our mask
    hbmpMask = myCreateBitmap(nBlockWidth * 2, nBlockHeight * 2, 1, 1, NULL);
    hbmpOld = (TBITMAP) SelectObject(hdcOffscreen, hbmpMask);

    // white on black mask, so the diagonal lines drawn below show up
    bResult &= PatBlt(hdcOffscreen, 0, 0, nBlockWidth*2, nBlockHeight*2, WHITENESS);
    bResult &= PatBlt(hdcOffscreen, nBlockWidth/4, nBlockHeight/4, nBlockWidth/2, nBlockHeight/2, BLACKNESS);
    bResult &= PatBlt(hdcOffscreen, 3* (nBlockWidth/8), 3*(nBlockHeight/8), nBlockWidth/4, nBlockHeight/4, WHITENESS);

    // draw a diagonal stripe across the mask, useful for getting an idea of the locations of the mask being used.
    bResult &= DrawDiagonals(hdcOffscreen, nBlockWidth, nBlockHeight);

    bResult &= BitBlt(hdcOffscreen, 0, nBlockHeight, nBlockWidth, nBlockHeight, hdcOffscreen, 0, 0, SRCCOPY);
    bResult &= BitBlt(hdcOffscreen, nBlockWidth, 0, nBlockWidth, nBlockHeight * 2, hdcOffscreen, 0, 0, SRCCOPY);

    if(!bResult)
            info(FAIL, TEXT("Initialization of the mask failed"));

    SelectObject(hdcOffscreen, hbmpOld);

    hbmpOld = (TBITMAP) SelectObject(hdcOffscreen, hbmp);

    FillSurface(hdcOffscreen);
    drawLogo(hdc, 150);
    // make whatever random color the surface was filled with initially the transparent color.
    bResult &= TransparentBlt(hdcOffscreen, 0, 0, zx/2, zy, hdc, 0, 0, zx/2, zy, GetPixel(hdc, 0, 0));

                              // left, top, left side offset, top offset, width offset, height offset, cliptop, clipleft, masktop, maskleft
    int nLeftBltCoordinates[][8] = {
                                // test the top clipping plane
                                // 0-3
                                 {   nCubex, -10, zx/2 + nCubex, 0, 0, 10, 0, 10 }, // clip into the mid top by 10
                                 {   nCubex, -1, zx/2 + nCubex, 0, 0, 1, 0, 1 }, // clip into the mid top by 1
                                 {   nCubex, -nBlockHeight, zx/2 + nCubex, 0, 0, nBlockHeight, 0, nBlockHeight }, // completely clip off of the top
                                 {   nCubex, -nBlockHeight+1, zx/2 + nCubex, 0, 0, nBlockHeight-1, 0, nBlockHeight-1 }, // 1 pixel from completely off the top

                                // test the topleft clipping plane
                                // 4-7
                                 {   -10, -10, zx/2 + 0, 0, 10, 10, 10, 10 }, // clip into the top left corner by 10
                                 {  -1, -1, zx/2 + 0, 0, 1, 1, 1, 1 }, // clip into the top left corner by 1
                                 {   -nBlockWidth, -nBlockHeight, zx/2 + 0, 0, nBlockWidth, nBlockHeight, nBlockWidth, nBlockHeight }, // completely off the top left corner
                                 {  -nBlockWidth +1, -nBlockHeight +1, zx/2 + 0, 0, nBlockWidth -1, nBlockHeight -1, nBlockWidth -1, nBlockHeight -1 }, // 1 pixel from completely off the top left corner

                                // test the left clipping plane
                                // 8-11
                                 {   -10, nCubey, zx/2 + 0, nCubey, 10, 0, 10, 0 }, // clip into the mid left by 10
                                 {   -1, nCubey, zx/2 + 0, nCubey, 1, 0, 1, 0 }, // clip into the mid left by 10
                                 {   -nBlockWidth, nCubey, zx/2 + 0, nCubey, nBlockWidth, 0, nBlockWidth, 0 }, // clip completely off the left
                                 {   -nBlockWidth +1, nCubey, zx/2 + 0, nCubey, nBlockWidth - 1, 0, nBlockWidth - 1, 0 }, // clip into the mid left by 10

                                // test the bottom right clipping plane
                                // 12-15
                                 {   -10, zy - (nBlockHeight-10), zx/2 + 0, zy - (nBlockHeight-10), 10, 10, 10, 0 }, // clip into the bottom left corner by 10
                                 {   -1, zy - (nBlockHeight-1), zx/2 + 0, zy - (nBlockHeight-1), 1, 1, 1, 0 }, // clip into the bottom left corner by 1
                                 {   -nBlockWidth, zy, zx/2 + 0, zy, nBlockWidth, nBlockHeight, nBlockWidth, 0 }, // completely clip off off the bottom left corner
                                 {   -nBlockWidth + 1, zy - 1, zx/2 + 0, zy - 1, nBlockWidth - 1, nBlockHeight - 1, nBlockWidth - 1, 0 }, // almost completely clip off the bottom left corner

                                // test the bottom center clipping plane
                                // 16-19
                                 {   nCubex, zy - (nBlockHeight-10), zx/2 + nCubex, zy - (nBlockHeight-10), 0, 10, 0, 0 }, // clip into the mid bottom by 10 
                                 {   nCubex, zy - (nBlockHeight-1), zx/2 + nCubex, zy - (nBlockHeight-1), 0, 1, 0, 0 }, // clip into the mid bottom by 1
                                 {   nCubex, zy, zx/2 + nCubex, zy, 0, nBlockHeight, 0, 0 }, // clip completely into the mid bottom
                                 {   nCubex, zy-1, zx/2 + nCubex, zy-1, 0, nBlockHeight-1, 0, 0 }, // almost completely clip into the bottom

                                // bottom right corner
                                // 20-23
                                 {   zx - (nBlockWidth - 10), zy - (nBlockHeight - 10), zx/2 - (nBlockWidth - 10), zy - (nBlockHeight - 10), 10, 10, 0, 0 }, // clip by 10 into the bottom right corner
                                 {   zx - (nBlockWidth - 1), zy - (nBlockHeight - 1), zx/2 - (nBlockWidth - 1), zy - (nBlockHeight - 1), 1, 1, 0, 0 }, // clip by 1 into the bottom right corner
                                 {   zx, zy, zx/2, zy, nBlockWidth, nBlockHeight, 0, 0 }, // completely clip into the bottom right corner
                                 {   zx - 1, zy - 1, zx/2 - 1, zy - 1, nBlockWidth-1, nBlockHeight-1, 0, 0 }, // clip all but 1 pixel into the bottom right corner 

                                // right middle clipping plane
                                // 24-27
                                 {   zx - (nBlockWidth - 10), nCubey, zx/2 - (nBlockWidth - 10), nCubey, 10, 0, 0, 0 }, // clip 10 pixels into the right clipping plane
                                 {   zx - (nBlockWidth - 1), nCubey, zx/2 - (nBlockWidth - 1), nCubey, 1, 0, 0, 0 }, // clip 1 pixel into the right
                                 {   zx, nCubey, zx/2, nCubey, nBlockWidth, 0, 0, 0 }, // completely clip into the right
                                 {   zx - 1, nCubey, zx/2 - 1, nCubey, nBlockWidth - 1, 0, 0, 0 }, // clip all but 1 pixel into the right
                                
                                // top right clipping plane
                                // 28-31
                                 {   zx - (nBlockWidth - 10), -10, zx/2 - (nBlockWidth - 10), 0, 10, 10, 0, 10 }, // clip 10 into the top right clipping plane
                                 {   zx - (nBlockWidth - 1), -1, zx/2 - (nBlockWidth - 1), 0, 1, 1, 0, 1 }, // clip 1 into the top right clipping plane
                                 {   zx, -nBlockHeight, zx/2, 0, nBlockWidth, nBlockHeight, 0, nBlockHeight }, // clip completely into the top right clipping plane
                                 {   zx - 1, -nBlockHeight + 1, zx/2 - 1, 0, nBlockWidth - 1, nBlockHeight - 1, 0, nBlockHeight - 1 }, // clip all but 1 pixel into the top right clipping plane

                                // overlapped blt's by 1
                                // 32-40
                                 { nCubex, nCubey, nCubex + zx/2, nCubey, 0, 0, 0, 0 }, // exactly on top
                                 { nCubex, nCubey-1, nCubex + zx/2, nCubey-1, 0, 0, 0, 0 }, // up
                                 { nCubex + 1, nCubey-1, nCubex + zx/2 + 1, nCubey-1, 0, 0, 0, 0 }, // up right
                                 { nCubex + 1, nCubey, nCubex + zx/2 + 1, nCubey, 0, 0, 0, 0 }, // right
                                 { nCubex + 1, nCubey + 1, nCubex + zx/2 + 1, nCubey + 1, 0, 0, 0, 0 }, // down right
                                 { nCubex, nCubey + 1, nCubex + zx/2, nCubey + 1, 0, 0, 0, 0 }, // down
                                 { nCubex - 1, nCubey + 1, nCubex + zx/2 - 1, nCubey + 1, 0, 0, 0, 0 }, // down left
                                 { nCubex - 1, nCubey, nCubex + zx/2 - 1, nCubey, 0, 0, 0, 0 }, // left                                
                                 { nCubex-1, nCubey-1, nCubex + zx/2-1, nCubey-1, 0, 0, 0, 0 }, // up left

                                // 1 pixel/row/column left overlapping
                                // 41-48
                                 { nCubex, nCubey-nBlockHeight, nCubex + zx/2, nCubey-nBlockHeight, 0, 0, 0, 0 }, // up
                                 { nCubex + nBlockWidth + zx/2, nCubey-nBlockHeight, nCubex + nBlockWidth, nCubey-nBlockHeight, nBlockWidth - (zx - (nCubex + nBlockWidth + zx/2)), 0, 0, 0 }, // up right
                                 { nCubex + nBlockWidth + zx/2, nCubey, nCubex + nBlockWidth, nCubey, nBlockWidth - (zx - (nCubex + nBlockWidth + zx/2)), 0, 0, 0 }, // right
                                 { nCubex + nBlockWidth + zx/2, nCubey + nBlockHeight, nCubex + nBlockWidth, nCubey + nBlockHeight, nBlockWidth - (zx - (nCubex + nBlockWidth + zx/2)), 0, 0, 0}, // down right
                                 { nCubex, nCubey + nBlockHeight, nCubex + zx/2, nCubey + nBlockHeight, 0, 0, 0, 0 }, // down
                                 { nCubex - nBlockWidth, nCubey + nBlockHeight, zx/2, nCubey + nBlockHeight, -(nCubex - nBlockWidth), 0, -(nCubex - nBlockWidth), 0 }, // down left
                                 { nCubex - nBlockWidth, nCubey, zx/2, nCubey, -(nCubex - nBlockWidth), 0, -(nCubex - nBlockWidth), 0 }, // left
                                 { nCubex - nBlockWidth, nCubey-nBlockHeight, zx/2, nCubey-nBlockHeight, -(nCubex - nBlockWidth), 0, -(nCubex - nBlockWidth), 0 }, // up left
                              };
    int nEntries = sizeof(nLeftBltCoordinates) / (sizeof(int) * 8);
    int nTestCount;

    if(testFunc == ETransparentImage)
        nTestCount = 1;
    else if(testFunc == EMaskBlt)
        nTestCount = countof(gnvRop4Array);
    else nTestCount = countof(gnvRop3Array);

    switch(type)
    {
        case 0:
            depth = 0;
            info(DETAIL, TEXT("No brush attached"));
            break;
        case 1:
            depth = 0;
            info(DETAIL, TEXT("Solid brush selected"));
            hBrush = CreateSolidBrush(randColorRef());
        case 2:
            depth = myGetBitDepth();
            info(DETAIL, TEXT("Using a compatible bitmap, d:%d, s:%d"), depth, brushSize);
            hBrush = setupBrush(hdc, type, depth, brushSize, FALSE, TRUE);
            break;
        case 3:
            depth = gBitDepths[rand() % countof(gBitDepths)];
            info(DETAIL, TEXT("Using a DIB w/RGB, d:%d, s:%d"), depth, brushSize);
            hBrush = setupBrush(hdc, type, depth, brushSize, FALSE, TRUE);
            break;
        case 4:
            // rand % 3 to get just paletted depths
            depth = gBitDepths[rand() % 3];
            info(DETAIL, TEXT("Using a DIB w/ PAL, d:%d, s:%d"), depth, brushSize);
            hBrush = setupBrush(hdc, type, depth, brushSize, FALSE, TRUE);
            break;
    }

    if(type >= 1)
    {
        if(!hBrush)
            info(ABORT, TEXT("Failed to create brush to select into DC for BitBlt suite"));
        else 
        {
            SetBrushOrgEx(hdc, 0, 0, NULL);
            SetBrushOrgEx(hdcOffscreen, 0, 0, NULL);
            hBrushStock1 = (HBRUSH) SelectObject(hdc, hBrush);
            hBrushStock2 = (HBRUSH) SelectObject(hdcOffscreen, hBrush);
        }
    }

    // step through all of the raster op's, or only go once for TransparentBlt because it doesn't do ROP's.
    // Go through the Rop4 array for MaskBlt, rop3's for everything else.
    for(int RopCount = 0; RopCount < nTestCount; RopCount++)
    {
        if(testFunc == ETransparentImage)
            info(DETAIL,TEXT("Using Transparent Color RGB(0,0,0)"));
        else if(testFunc == EMaskBlt)
        {
            info(DETAIL,TEXT("Using ROP %s"), gnvRop4Array[RopCount].szName);
            dwRop = gnvRop4Array[RopCount].dwVal;
        }
        else 
        {
            info(DETAIL,TEXT("Using ROP %s"), gnvRop3Array[RopCount].szName);
            dwRop = gnvRop3Array[RopCount].dwVal;
        }

        for(int i = 0; i < nEntries; i++)
        {
            // initialize our flag for failures to true for this entry.  If any of the calls fail, then fail the case.
            bResult = TRUE;
            bResult &= BitBlt(hdc, 0, 0, zx/2, zy, hdcOffscreen, 0, 0, SRCCOPY);
            bResult &= BitBlt(hdc, zx/2, 0, zx/2, zy, hdc, 0, 0, SRCCOPY);

            pt.x = 0;
            pt.y = 0;
            // if our destination is on the right half, then the brush origin is in the center top, 
            // otherwise it's in the top left.
            if(nLeftBltCoordinates[i][0] >= zx/2)
                pt.x = zx/2;

            if(!SetBrushOrgEx(hdc, pt.x, pt.y, NULL))
                info(FAIL, TEXT("Failed to set brush origin for testing pattern rop's"));

            // reselect the brush to set the new origin in effect.
            SelectObject(hdc, hBrush);

            switch(testFunc)
            {
                case EBitBlt:
                    // always blt the same sized block for comparing to the other side
                    bResult &= BitBlt(hdc, nLeftBltCoordinates[i][0], nLeftBltCoordinates[i][1], nBlockWidth, nBlockHeight, hdc, nCubex, nCubey, dwRop);
                    break;
                case EStretchBlt:
                    bResult &= StretchBlt(hdc, nLeftBltCoordinates[i][0], nLeftBltCoordinates[i][1], nBlockWidth, nBlockHeight, hdc, nCubex, nCubey, nBlockWidth, nBlockHeight, dwRop);
                    break;
                case EMaskBlt:
                    bResult &= MaskBlt(hdc, nLeftBltCoordinates[i][0], nLeftBltCoordinates[i][1], nBlockWidth, nBlockHeight, hdc, nCubex, nCubey, hbmpMask, 0, 0, dwRop);
                    break;
                case ETransparentImage:
                    // make black transparent, so only the center of the flag shows.
                    bResult &= TransparentBlt(hdc, nLeftBltCoordinates[i][0], nLeftBltCoordinates[i][1], nBlockWidth, nBlockHeight, hdc, nCubex, nCubey, nBlockWidth, nBlockHeight, RGB(0,0,0));
                    break;
            }

            pt.x = 0;
            pt.y = 0;
            // if our destination is on the right half, then the brush origin is in the center top, 
            // otherwise it's in the top left.
            if(nLeftBltCoordinates[i][2] >= zx/2)
                pt.x = zx/2;
            if(!SetBrushOrgEx(hdc, pt.x, pt.y, NULL))
                info(FAIL, TEXT("Failed to set brush origin for testing pattern rop's"));

            // reselect the brush to set the new origin in effect.
            SelectObject(hdc, hBrush);

            switch(testFunc)
            {
                case ETransparentImage:
                    bResult &= TransparentBlt(hdc, nLeftBltCoordinates[i][2], nLeftBltCoordinates[i][3], nBlockWidth - nLeftBltCoordinates[i][4], nBlockHeight - nLeftBltCoordinates[i][5], 
                                                hdcOffscreen, nCubex + nLeftBltCoordinates[i][6], nCubey + nLeftBltCoordinates[i][7], nBlockWidth - nLeftBltCoordinates[i][4], nBlockHeight - nLeftBltCoordinates[i][5], RGB(0,0,0));
                    break;
                case EMaskBlt:
                    nMaskx = nLeftBltCoordinates[i][6];
                    nMasky = nLeftBltCoordinates[i][7];
#ifndef UNDER_CE 
                    // the desktop seems to move the mask in by 2*the amount outside of the primary, and wraps the mask around to compensate for the lacking height/width
                    // this code is used to determine whether or not the source is needed, and how to modify the coordinates for the
                    // mask's top-left corner to compensate for the desktop behavior.
                    nForegroundRop = dwRop >> 16;
                    nBackgroundRop = dwRop >> 24;
                    bForeNeedSource = ((((nForegroundRop >> 2) ^ nForegroundRop) & 0x33) != 0);
                    bBackNeedSource = ((((nBackgroundRop >> 2) ^ nBackgroundRop) & 0x33) != 0);
        
                    // if either the forground or the background rop's need the source, then we need to modify the coordinates into the mask to compensate for clipping.
                    // if neither need the source, then we do not need to compensate for clipping, since the offscreen HDC isn't used.
                    if(bForeNeedSource == FALSE && bBackNeedSource == FALSE)
                    {
                        nMaskx = 2*nLeftBltCoordinates[i][6];
                        nMasky = 2*nLeftBltCoordinates[i][7];
                    }
#endif
                    bResult &= MaskBlt(hdc, nLeftBltCoordinates[i][2], nLeftBltCoordinates[i][3], nBlockWidth - nLeftBltCoordinates[i][4], nBlockHeight - nLeftBltCoordinates[i][5], 
                                                hdcOffscreen, nCubex + nLeftBltCoordinates[i][6], nCubey + nLeftBltCoordinates[i][7], hbmpMask, nMaskx, nMasky, dwRop);
                    break;
                default:
                    // grab the same block from the offscreen surface and put it on the other side
                    bResult &= BitBlt(hdc, nLeftBltCoordinates[i][2], nLeftBltCoordinates[i][3], nBlockWidth - nLeftBltCoordinates[i][4], nBlockHeight - nLeftBltCoordinates[i][5], 
                                                                            hdcOffscreen, nCubex + nLeftBltCoordinates[i][6], nCubey + nLeftBltCoordinates[i][7], dwRop);
                    break;
            }
            if(!CheckScreenHalves(hdc))
            {
                    if(testFunc == ETransparentImage)
                        info(DETAIL,TEXT("Failure on Transparent Image test %d"), i);
                    else if(testFunc == EMaskBlt)
                        info(DETAIL,TEXT("Failure on test %d, using ROP %s"), i, gnvRop4Array[RopCount].szName);
                    else 
                        info(FAIL, TEXT("Failure on test %d, rop %s"), i, gnvRop3Array[RopCount].szName);
            }
            if(!bResult)
                info(FAIL, TEXT("A Blt call failed for rop %s"), gnvRop3Array[RopCount].szName);
        }
    }

    if(type >= -1)
    {
        SelectObject(hdc, hBrushStock1);
        SelectObject(hdcOffscreen, hBrushStock2);
        DeleteObject(hBrush);  
    }
    SelectObject(hdcOffscreen, hbmpOld);
    DeleteObject(hbmp);
    DeleteObject(hbmpMask);
    DeleteDC(hdcOffscreen);
    myReleaseDC(hdc);
}

// this is a stretching version of the bitBlt suite.  Since most drivers will be intelligent and use the same code path for a StretchBlt as a
// BitBlt if there isn't any stretching, we need to do the overlapped blt's in the situation where there is some stretching.
// This suite behaves similar to the BitBlt suite execept it stretches the blt's by a random small amount on both sides.
// because of that stretching and the resulting clipping that will be needed, it needs a completely different data set to handle
// the blt's to both sides. 

void
StretchBltSuite(int testFunc)
{
    info(ECHO, TEXT("StretchBltSuite - %s"), funcName[testFunc]);
    TDC hdc = myGetDC(), hdcOffscreen = CreateCompatibleDC(hdc);
    TBITMAP hbmp = CreateCompatibleBitmap(hdc, zx/2, zy), hbmpOld;
    
    // block size to blt
    int nBlockWidth = zx/4;
    int nBlockHeight = zy/2;
    // location to pull the cube from the left side
    int nCubex = zx/8;
    int nCubey = zy/4;
    int nSrcStretchx, nSrcStretchy, nDstStretchx, nDstStretchy;

    DWORD dwRop = 0;
    BOOL bResult = TRUE;
    HBRUSH hBrush = NULL, hBrushStock1 = NULL, hBrushStock2 = NULL;
    int depth = 0;
    // no brush for now.
    int type = 0;
    // 8x8 is the most common brush size.
    int brushSize = 8;

    // setup the offscreen source for the non-overlapped blt's
    hbmpOld = (TBITMAP) SelectObject(hdcOffscreen, hbmp);

    FillSurface(hdcOffscreen);
    drawLogo(hdc, 150);
    // make whatever random color the surface was filled with the transparent color.
    bResult &= TransparentBlt(hdcOffscreen, 0, 0, zx/2, zy, hdc, 0, 0, zx/2, zy, GetPixel(hdc, 0, 0));

    // the source stretch must be negative (smaller than the available space) to keep from grabbing uninitialized data from the screen.
    nSrcStretchx = (rand() % zx/4) - zx/8;
    nSrcStretchy = (rand() % zy/4) - zy/8;
    nDstStretchx = (rand() % zx/4) - zx/8;
    nDstStretchy = (rand() % zy/4) - zy/8;

                              // left, top, dest stretch x, dest stretch y, src stretch x, src stretch y
    int nLeftBltCoordinates[][6] = {                                 
                               // overlapped blt's by 1
                                 { nCubex, nCubey, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // exactly on top
                                 { nCubex, nCubey-1, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // up
                                 { nCubex + 1, nCubey-1, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // up right
                                 { nCubex + 1, nCubey, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // right
                                 { nCubex + 1, nCubey + 1, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // down right
                                 { nCubex, nCubey + 1, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // down
                                 { nCubex - 1, nCubey + 1, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // down left
                                 { nCubex - 1, nCubey, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // left                                
                                 { nCubex-1, nCubey-1, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // up left
                              };

    int nEntries = sizeof(nLeftBltCoordinates) / (sizeof(int) * 6);
    int nTestCount;

    if(testFunc == ETransparentImage)
        nTestCount = 1;
    else nTestCount = countof(gnvRop3Array);

    switch(type)
    {
        case 0:
            depth = 0;
            info(DETAIL, TEXT("No brush attached"));
            break;
        case 1:
            depth = 0;
            info(DETAIL, TEXT("Solid brush selected"));
            hBrush = CreateSolidBrush(randColorRef());
        case 2:
            depth = myGetBitDepth();
            info(DETAIL, TEXT("Using a compatible bitmap, d:%d, s:%d"), depth, brushSize);
            hBrush = setupBrush(hdc, type, depth, brushSize, FALSE, TRUE);
            break;
        case 3:
            depth = gBitDepths[rand() % countof(gBitDepths)];
            info(DETAIL, TEXT("Using a DIB w/RGB, d:%d, s:%d"), depth, brushSize);
            hBrush = setupBrush(hdc, type, depth, brushSize, FALSE, TRUE);
            break;
        case 4:
            // rand % 3 to get just paletted depths
            depth = gBitDepths[rand() % 3];
            info(DETAIL, TEXT("Using a DIB w/ PAL, d:%d, s:%d"), depth, brushSize);
            hBrush = setupBrush(hdc, type, depth, brushSize, FALSE, TRUE);
            break;
    }

    if(type >= 1)
    {
        if(!hBrush)
            info(ABORT, TEXT("Failed to create brush to select into DC for BitBlt suite"));
        else 
        {
            SetBrushOrgEx(hdc, 0, 0, NULL);
            SetBrushOrgEx(hdcOffscreen, 0, 0, NULL);
            hBrushStock1 = (HBRUSH) SelectObject(hdc, hBrush);
            hBrushStock2 = (HBRUSH) SelectObject(hdcOffscreen, hBrush);
        }
    }
    // step through all of the raster op's, or only go once for TransparentBlt because it doesn't do ROP's.
    for(int RopCount = 0; RopCount < nTestCount; RopCount++)
    {
        if(testFunc == ETransparentImage)
            info(DETAIL,TEXT("Using Transparent Color RGB(0,0,0)"));
        else 
        {
            info(DETAIL,TEXT("Using ROP %s"), gnvRop3Array[RopCount].szName);
            dwRop = gnvRop3Array[RopCount].dwVal;
        }
            
        for(int i = 0; i < nEntries; i++)
        {
            // initialize our flag for failures to true for this entry.  If any of the calls fail, then fail the case.
            bResult = TRUE;

            bResult &= BitBlt(hdc, 0, 0, zx/2, zy, hdcOffscreen, 0, 0, SRCCOPY);
            bResult &= BitBlt(hdc, zx/2, 0, zx/2, zy, hdc, 0, 0, SRCCOPY);

            if(!SetBrushOrgEx(hdc, 0, 0, NULL))
                info(FAIL, TEXT("Failed to set brush origin for testing pattern rop's"));

            // reselect the brush to set the new origin in effect.
            SelectObject(hdc, hBrush);
            
            switch(testFunc)
            {
                case EStretchBlt:
                    SelectObject(hdc, hBrush);
                    bResult &= StretchBlt(hdc, nLeftBltCoordinates[i][0], nLeftBltCoordinates[i][1], nBlockWidth + nLeftBltCoordinates[i][2], nBlockHeight + nLeftBltCoordinates[i][3], 
                                                    hdc, nCubex, nCubey, nBlockWidth + nLeftBltCoordinates[i][4], nBlockHeight + nLeftBltCoordinates[i][5], dwRop);
                    break;

                case ETransparentImage:
                     // make black transparent, so only the center of the flag shows.
                    bResult &= TransparentBlt(hdc, nLeftBltCoordinates[i][0], nLeftBltCoordinates[i][1], nBlockWidth + nLeftBltCoordinates[i][2], nBlockHeight + nLeftBltCoordinates[i][3], 
                                                           hdc, nCubex, nCubey, nBlockWidth + nLeftBltCoordinates[i][4], nBlockHeight + nLeftBltCoordinates[i][5], RGB(0,0,0));
                    break;
            }

            if(!SetBrushOrgEx(hdc, zx/2, 0, NULL))
                info(FAIL, TEXT("Failed to set brush origin for testing pattern rop's"));

            // reselect the brush to set the new origin in effect.
            SelectObject(hdc, hBrush);
            
            switch(testFunc)
            {
                case EStretchBlt:
                    bResult &= StretchBlt(hdc, nLeftBltCoordinates[i][0] + zx/2, nLeftBltCoordinates[i][1], nBlockWidth + nLeftBltCoordinates[i][2], nBlockHeight + nLeftBltCoordinates[i][3], 
                                                    hdcOffscreen, nCubex, nCubey, nBlockWidth + nLeftBltCoordinates[i][4], nBlockHeight + nLeftBltCoordinates[i][5], dwRop);
                    break;
                case ETransparentImage:
                    bResult &= TransparentBlt(hdc, nLeftBltCoordinates[i][0] + zx/2, nLeftBltCoordinates[i][1], nBlockWidth + nLeftBltCoordinates[i][2], nBlockHeight + nLeftBltCoordinates[i][3], 
                                                           hdcOffscreen, nCubex, nCubey, nBlockWidth + nLeftBltCoordinates[i][4], nBlockHeight + nLeftBltCoordinates[i][5], RGB(0,0,0));
                    break;
            }
            
            if(!CheckScreenHalves(hdc))
            {
                    if(testFunc == ETransparentImage)
                        info(DETAIL,TEXT("Failure on Transparent Image test %d"), i);
                    else 
                        info(FAIL, TEXT("Failure on test %d, rop %s"), i, gnvRop3Array[RopCount].szName);
            }
            if(!bResult)
                info(FAIL, TEXT("A Blt call failed for rop %s"), gnvRop3Array[RopCount].szName);
        }
    }

    if(type >= 1)
    {
        SelectObject(hdc, hBrushStock1);
        SelectObject(hdcOffscreen, hBrushStock2);
        DeleteObject(hBrush);
    }
    SelectObject(hdcOffscreen, hbmpOld);
    DeleteObject(hbmp);
    DeleteDC(hdcOffscreen);
    myReleaseDC(hdc);
}

/* this test does a fullscreen overlapped blt in 8 directions, using all of the rops in gnvRop3Array.
    This test does not do any form of verification */
// TODO: write a function which takes two dc's and does a comparision, so this function has a form of verification.
void 
BitBltSuiteFullscreen(int testFunc)
{
    info(ECHO, TEXT("BitBltSuiteFullscreen - %s"), funcName[testFunc]);
    
    TDC hdc = myGetDC(), hdcOffscreen = CreateCompatibleDC(hdc);
    TBITMAP hbmp = CreateCompatibleBitmap(hdc, zx/2, zy), hbmpOld, hbmpMask;

    DWORD dwRop = 0;

    BOOL result = FALSE;
    int nSrcStretchx, nSrcStretchy, nDstStretchx, nDstStretchy;    
    HBRUSH hBrush = NULL, hBrushStock = NULL;
    int depth = 0;
    int type = 0;
    int brushSize = 8;
    
    // set up our mask
    hbmpMask = myCreateBitmap(zx, zy, 1, 1, NULL);
    hbmpOld = (TBITMAP) SelectObject(hdcOffscreen, hbmpMask);

    // fill the mask with something interesting
    myPatBlt(hdcOffscreen, 0, 0, zx, zy, WHITENESS);
    // draw a diagonal stripe across the mask, useful for getting an idea of the locations of the mask being used.
    DrawDiagonals(hdcOffscreen, zx, zy);
    SelectObject(hdcOffscreen, hbmpOld);

    hbmpOld = (TBITMAP) SelectObject(hdcOffscreen, hbmp);

    FillSurface(hdcOffscreen);
    drawLogo(hdc, 150);
    // make whatever random color the surface was filled with the transparent color.
    TransparentBlt(hdcOffscreen, 0, 0, zx/2, zy, hdc, 0, 0, zx/2, zy, GetPixel(hdc, 0, 0));

    // the source stretch must be negative (smaller than the available space) to keep from grabbing uninitialized data from the screen.

    // perm3 stretching doesn't match GPE for this.
    nSrcStretchx = 0;
    nSrcStretchy = 0;
    nDstStretchx = 0;
    nDstStretchy = 0;
/*
    nSrcStretchx = -(rand() % zx/4);
    nSrcStretchy = -(rand() % zy/4);
    nDstStretchx = (rand() % zx/4) - zy/8;
    nDstStretchy = (rand() % zy/4) - zy/8;
*/
                              // left, top, left side offset, top offset, width offset, height offset, cliptop, clipleft
    int nLeftBltCoordinates[][6] = {
                                 // overlapped blt's by 1
                                 { 0, 0, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // exactly on top
                                 { 0, -1, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // up
                                 {  1, -1, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // up right
                                 {  1, 0, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // right
                                 {  1,  1, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // down right
                                 { 0,  1, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // down
                                 { -1,  1, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // down left
                                 { -1, 0, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // left                                
                                 { -1, -1, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // up left

                                 // 1 pixel/row/column left overlapping
                                 { 0, -(zy-1), nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // up
                                 { zx - 1, -(zy-1), nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // up right
                                 { zx - 1, 0, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // right
                                 { zx - 1, zy - 1, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // down right
                                 { 0, zy - 1, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // down
                                 { -(zx - 1), zy - 1, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // down left
                                 { -(zx - 1), 0, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // left
                                 { -(zx - 1), -zy - 1, nDstStretchx, nDstStretchy, nSrcStretchx, nSrcStretchy }, // up left
                              };
    int nEntries = sizeof(nLeftBltCoordinates) / (sizeof(int) * 6);
    int nTestCount;

    if(testFunc == ETransparentImage)
        nTestCount = 1;
    else if(testFunc == EMaskBlt)
        nTestCount = countof(gnvRop4Array);
    else nTestCount = countof(gnvRop3Array);

    switch(type)
    {
        case 0:
            depth = 0;
            info(DETAIL, TEXT("No brush attached"));
            break;
        case 1:
            depth = 0;
            info(DETAIL, TEXT("Solid brush selected"));
            hBrush = CreateSolidBrush(randColorRef());
        case 2:
            depth = myGetBitDepth();
            info(DETAIL, TEXT("Using a compatible bitmap, d:%d, s:%d"), depth, brushSize);
            hBrush = setupBrush(hdc, compatibleBitmap, depth, brushSize, FALSE, TRUE);
            break;
        case 3:
            depth = gBitDepths[rand() % countof(gBitDepths)];
            info(DETAIL, TEXT("Using a DIB w/RGB, d:%d, s:%d"), depth, brushSize);
            hBrush = setupBrush(hdc, DibRGB, depth, brushSize, FALSE, TRUE);
            break;
        case 4:
            // rand % 3 to get just paletted depths
            depth = gBitDepths[rand() % 3];
            info(DETAIL, TEXT("Using a DIB w/ PAL, d:%d, s:%d"), depth, brushSize);
            hBrush = setupBrush(hdc, DibPal, depth, brushSize, FALSE, TRUE);
            break;
    }

    if(type >= 1)
    {
        SetBrushOrgEx(hdc, 0, 0, NULL);
        if(!hBrush)
            info(ABORT, TEXT("Failed to create brush to select into DC for BitBlt suite"));
        else 
            hBrushStock = (HBRUSH) SelectObject(hdc, hBrush);
    }
    // step through all of the raster op's, or only go once for TransparentBlt because it doesn't do ROP's.
    // Go through the Rop4 array for MaskBlt, rop3's for everything else.
    for(int RopCount = 0; RopCount < nTestCount; RopCount++)
    {
        if(testFunc == ETransparentImage)
            info(DETAIL,TEXT("Using Transparent Color RGB(0,0,0)"));
        else if(testFunc == EMaskBlt)
        {
            info(DETAIL,TEXT("Using ROP %s"), gnvRop4Array[RopCount].szName);
            dwRop = gnvRop4Array[RopCount].dwVal;
        }
        else 
        {
            info(DETAIL,TEXT("Using ROP %s"), gnvRop3Array[RopCount].szName);
            dwRop = gnvRop3Array[RopCount].dwVal;
        }

        for(int i=0; i < nEntries; i++)
        {
            myBitBlt(hdc, 0, 0, zx/2, zy, hdcOffscreen, 0, 0, SRCCOPY);
            myBitBlt(hdc, zx/2, 0, zx/2, zy, hdc, 0, 0, SRCCOPY);

            switch(testFunc)
            {
                case EBitBlt:
                    // always blt the same sized block for comparing to the other side
                    result = myBitBlt(hdc, nLeftBltCoordinates[i][0], nLeftBltCoordinates[i][1], zx, zy, hdc, 0, 0, dwRop);
                    break;
                case EStretchBlt:
                    result = myStretchBlt(hdc, nLeftBltCoordinates[i][0], nLeftBltCoordinates[i][1], zx + nLeftBltCoordinates[i][2], zy + nLeftBltCoordinates[i][3], 
                                                    hdc, 0, 0, zx + nLeftBltCoordinates[i][4], zy + nLeftBltCoordinates[i][5], dwRop);
                    break;
                case EMaskBlt:
                    result = myMaskBlt(hdc, nLeftBltCoordinates[i][0], nLeftBltCoordinates[i][1], zx, zy, hdc, 0, 0, hbmpMask, 0, 0, dwRop);
                    break;
                case ETransparentImage:
                    // grab the color of a random pixel on the screen, and make it transparent
                    result = TransparentBlt(hdc, nLeftBltCoordinates[i][0], nLeftBltCoordinates[i][1], zx + nLeftBltCoordinates[i][2], zy + nLeftBltCoordinates[i][3], 
                                                       hdc, 0, 0, zx + nLeftBltCoordinates[i][4], zy + nLeftBltCoordinates[i][5], GetPixel(hdc, rand() % zx, rand() % zy));
                    break;
            }
            if(!result)
            {
                if(testFunc == EMaskBlt)
                    info(FAIL, TEXT("fullscreen Blt failed (%d, %d) test id:%d rop:%s"), nLeftBltCoordinates[i][0], nLeftBltCoordinates[i][1], i, gnvRop4Array[RopCount].szName);
                else if(testFunc == ETransparentImage)
                    info(FAIL, TEXT("fullscreen Blt failed (%d, %d) test id:%d"), nLeftBltCoordinates[i][0], nLeftBltCoordinates[i][1], i);
                else 
                    info(FAIL, TEXT("fullscreen Blt failed (%d, %d) test id:%d rop:%s"), nLeftBltCoordinates[i][0], nLeftBltCoordinates[i][1], i, gnvRop3Array[RopCount].szName);

            }
        }
    }

    if(type >= 1)
    {
        SelectObject(hdc, hBrushStock);
        DeleteObject(hBrush);
    }
    SelectObject(hdcOffscreen, hbmpOld);
    DeleteObject(hbmp);
    DeleteObject(hbmpMask);
    DeleteDC(hdcOffscreen);
    myReleaseDC(hdc);
}

void
BltFromMonochromeTest(int testFunc, int testMethod)
{
    info(ECHO, TEXT("%s - BltFromMonochromeTest"), funcName[testFunc]);

    TDC hdc = myGetDC();
    TDC hdcCompat = CreateCompatibleDC(hdc);
    TDC hdcMonoCompat = CreateCompatibleDC(hdc);
    TBITMAP hbmpMono = NULL, hbmpOffscreen = NULL;
    TBITMAP hbmpMonoStock = NULL, hbmpOffscreenStock = NULL;
    TBITMAP hbmpMask;
    BOOL bResult = TRUE;
    COLORREF FgColor = randColorRef(), BkColor = randColorRef();
    COLORREF FgColorStock, BkColorStock;
    HBRUSH hbrFgColor, hbrBkColor;
    RECT rc = {0};
    BYTE *lpBits;

    FgColorStock = SetTextColor(hdc, FgColor);
    BkColorStock = SetBkColor(hdc, BkColor);

    if(testMethod != ECreateDIBSection)
    {
        hbrFgColor = CreateSolidBrush(FgColor);
        hbrBkColor = CreateSolidBrush(BkColor);
    }
    else
    {
        // the DIB will be created with this palette, so look for these colors in the conversion.
        hbrFgColor = CreateSolidBrush(RGB(0,0,0));
        hbrBkColor = CreateSolidBrush(RGB(0x55, 0x55, 0x55));
    }

    if(hdc && hdcCompat && hdcMonoCompat && hbrFgColor && hbrBkColor && FgColorStock != CLR_INVALID && BkColorStock != CLR_INVALID)
    {
        hbmpOffscreen = CreateCompatibleBitmap(hdc, zx/2, zy);
        hbmpMask = myCreateBitmap(zx/2, zy, 1, 1, NULL);

        // test with both a 1bpp monochrome from myCreateBitmap, and also by
        // creating a 1bpp monochrome using the 1bpp monochrome attached to
        // a compatible dc by default.
        switch(testMethod)
        {
            case ECreateBitmap:
                info(DETAIL, TEXT("Using a bitmap from CreateBitmap"));
                hbmpMono = myCreateBitmap(zx/2, zy, 1, 1, NULL);
                break;
            case ECreateCompatibleBitmap:
                info(DETAIL, TEXT("Using a bitmap from CreateCompatibleBitmap"));
                hbmpMono = CreateCompatibleBitmap(hdcMonoCompat, zx/2, zy);
                break;
            case ECreateDIBSection:
                info(DETAIL, TEXT("Using a bitmap from CreateDIBSection"));
                hbmpMono = myCreateDIBSection(hdc, (VOID **) &lpBits, 1, zx/2, zy, DIB_RGB_COLORS);
                break;
        }

        if(hbmpOffscreen && hbmpMono && hbmpMask)
        {
            bResult = TRUE;

            hbmpOffscreenStock = (TBITMAP) SelectObject(hdcCompat, hbmpOffscreen);
            hbmpMonoStock = (TBITMAP) SelectObject(hdcMonoCompat, hbmpMask);

            // draw diagonals on the mask, useful for identifying locations
            bResult &= PatBlt(hdcMonoCompat, 0, 0, zx/2, zy, WHITENESS);
            bResult &= DrawDiagonals(hdcMonoCompat, zx/2, zy);
            if(!bResult)
                info(FAIL, TEXT("Initization of the mask failed."));
                        
            // select in our mono bitmap for testing.  the stock bitmap was saved when we selected in the mask for initialization.
            SelectObject(hdcMonoCompat, hbmpMono);

            bResult = TRUE;
            // initialize the test bitmap, this initialization must match the shape drawn on the color surface.
            bResult &= PatBlt(hdcMonoCompat, 0, 0, zx/2, zy, BLACKNESS);
            bResult &= PatBlt(hdcMonoCompat, zx/8, zy/4, zx/4, zy/2, WHITENESS);
            if(!bResult)
                info(FAIL, TEXT("Initization of the mono bitmap failed."));

            bResult = TRUE;
            // fill the foreground color into the color dc
            SetRect(&rc, 0, 0, zx/2, zy);
            bResult &= FillRect(hdcCompat, &rc, hbrFgColor);
            // use the same calculation for the coordinates as used in the PatBlts, to avoid rounding problems.
            SetRect(&rc, zx/8, zy/4, (zx/8 + zx/4), (zy/4 + zy/2));
            bResult &= FillRect(hdcCompat, &rc, hbrBkColor);
            if(!bResult)
                info(FAIL, TEXT("Initization of the color bitmap failed."));
         
            bResult = TRUE;
            switch(testFunc)
            {
                case EBitBlt:
                    bResult &= BitBlt(hdc, 0, 0, zx/2, zy, hdcMonoCompat, 0, 0, SRCCOPY);
                    bResult &= BitBlt(hdc, zx/2, 0, zx/2, zy, hdcCompat, 0, 0, SRCCOPY);
                    break;
                case EStretchBlt:
                    bResult &= StretchBlt(hdc, 0, 0, zx/2, zy, hdcMonoCompat, 0, 0, zx/2 - 1, zy - 1, SRCCOPY);
                    bResult &= StretchBlt(hdc, zx/2, 0, zx/2, zy, hdcCompat, 0, 0, zx/2 - 1, zy - 1, SRCCOPY);
                    break;
                case EMaskBlt:
                    bResult &= MaskBlt(hdc, 0, 0, zx/2, zy, hdcMonoCompat, 0, 0, hbmpMask, 0, 0, MAKEROP4(SRCCOPY, BLACKNESS));
                    bResult &= MaskBlt(hdc, zx/2, 0, zx/2, zy, hdcCompat, 0, 0, hbmpMask, 0, 0, MAKEROP4(SRCCOPY, BLACKNESS));
                    break;
            }

            if(!bResult)
                info(FAIL, TEXT("Blitting from the offscreen DC's to the primary failed."));
            if(!CheckScreenHalves(hdc))
                info(FAIL, TEXT("Blt from a monochrome bitmap to the primary didn't behave correctly.  bkColor:%d fgColor: %d"), BkColor, FgColor);

            SelectObject(hdcCompat, hbmpOffscreenStock);
            SelectObject(hdcMonoCompat, hbmpMonoStock);
            DeleteObject(hbmpOffscreen);
            DeleteObject(hbmpMono);
        }
        else info(ABORT, TEXT("Failed to create necessary bitmaps for testing"));
    }
    else info(ABORT, TEXT("Failed to create necessary DC's and brushes for testing or failed to set foreground/background colors for testing"));


    SetTextColor(hdc, FgColorStock);
    SetBkColor(hdc, BkColorStock);
    DeleteObject(hbrFgColor);
    DeleteObject(hbrBkColor);
    DeleteDC(hdcCompat);
    DeleteDC(hdcMonoCompat);
    myReleaseDC(hdc);
}

void
BltToMonochromeTest(int testFunc, int testMethod)
{
    info(ECHO, TEXT("%s - BltToMonochromeTest"), funcName[testFunc]);

    TDC hdc = myGetDC();
    TDC hdcMonoCompat = CreateCompatibleDC(hdc);
    TBITMAP hbmpMono = NULL, hbmpMask = myCreateBitmap(zx/2, zy, 1, 1, NULL);
    TBITMAP hbmpMonoStock = NULL;
    COLORREF FgColor = randColorRef(), BkColor = randColorRef();
    COLORREF FgColorStock, BkColorStock;
    HBRUSH hbrFgColor, hbrBkColor;
    BOOL bResult = TRUE;
    RECT rc;
    BYTE *lpBits = NULL;

    if(testMethod != ECreateDIBSection)
    {
        hbrFgColor = CreateSolidBrush(FgColor);
        hbrBkColor = CreateSolidBrush(BkColor);
    }
    else
    {
        // the DIB will be created with this palette, so look for these colors in the conversion.
        hbrFgColor = CreateSolidBrush(RGB(0,0,0));
        hbrBkColor = CreateSolidBrush(RGB(0x55, 0x55, 0x55));
    }
    
    if(hdc && hdcMonoCompat && hbmpMask)
    {
        hbmpMonoStock = (TBITMAP) SelectObject(hdcMonoCompat, hbmpMask);
        PatBlt(hdcMonoCompat, 0, 0, zx/2, zy, WHITENESS);
        DrawDiagonals(hdcMonoCompat, zx/2, zy);
        
        // test with both a 1bpp monochrome from myCreateBitmap, and also by
        // creating a 1bpp monochrome using the 1bpp monochrome attached to
        // a compatible dc by default.
        switch(testMethod)
        {
            case ECreateBitmap:
                info(DETAIL, TEXT("Using a bitmap from CreateBitmap"));
                hbmpMono = myCreateBitmap(zx/2, zy, 1, 1, NULL);
                break;
            case ECreateCompatibleBitmap:
                info(DETAIL, TEXT("Using a bitmap from CreateCompatibleBitmap"));
                hbmpMono = CreateCompatibleBitmap(hdcMonoCompat, zx/2, zy);
                break;
            case ECreateDIBSection:
                info(DETAIL, TEXT("Using a bitmap from CreateDIBSection"));
                hbmpMono = myCreateDIBSection(hdc, (VOID **) &lpBits, 1, zx/2, zy, DIB_RGB_COLORS);
                break;
        }

        if(hbmpMono)
        {
            SelectObject(hdcMonoCompat, hbmpMono);

            SetRect(&rc, 0, 0, zx/2, zy);
            bResult &= FillRect(hdc, &rc, hbrFgColor);
            SetRect(&rc, zx/8, zy/4, 3*zx/8, 3*zy/4);
            FillRect(hdc, &rc, hbrBkColor);

            BkColorStock = SetBkColor(hdc, BkColor);
            bResult &= BitBlt(hdcMonoCompat, 0, 0, zx/2, zy, hdc, 0, 0, SRCCOPY);
            FgColorStock = SetTextColor(hdc, FgColor);

            switch(testFunc)
            {
                case EBitBlt:
                    bResult &= BitBlt(hdc, zx/2, 0, zx/2, zy, hdcMonoCompat, 0, 0, SRCCOPY);
                    break;
                case EMaskBlt:
                    bResult &= MaskBlt(hdc, 0, 0, zx/2, zy, hdc, 0, 0, hbmpMask, 0, 0, MAKEROP4(SRCCOPY, BLACKNESS));
                    bResult &= MaskBlt(hdc, zx/2, 0, zx/2, zy, hdcMonoCompat, 0, 0, hbmpMask, 0, 0, MAKEROP4(SRCCOPY, BLACKNESS));
                    break;
                case EStretchBlt:
                    bResult &= StretchBlt(hdc, 0, 0, zx/2, zy, hdc, 0, 0, zx/2 - 1, zy - 1, SRCCOPY);
                    bResult &= StretchBlt(hdc, zx/2, 0, zx/2, zy, hdcMonoCompat, 0, 0, zx/2 - 1, zy - 1, SRCCOPY);
                    break;
            }
            SetTextColor(hdc, FgColorStock);
            SetBkColor(hdc, BkColorStock);

            if(!bResult)
                info(FAIL, TEXT("One more drawing operations failed."));
            
            if(!CheckScreenHalves(hdc))
                info(FAIL, TEXT("Blt to a monochrome bitmap from the primary didn't behave properly"));

            SelectObject(hdcMonoCompat, hbmpMonoStock);
            DeleteObject(hbmpMonoStock);
        }
        else info(ABORT, TEXT("Creation of a 1bpp bitmap failed."));

        DeleteObject(hbmpMask);
        DeleteObject(hbrFgColor);
        DeleteObject(hbrBkColor);
        DeleteDC(hdcMonoCompat);
        myReleaseDC(hdc);
    
    }
    else info(ABORT, TEXT("Creation of DC's required for testing failed."));
}

void
SolidGradientFill()
{
    info(ECHO, TEXT("SolidGradientFill test"));
    
    TDC hdc = myGetDC();
    TRIVERTEX        vert[2];
    GRADIENT_RECT    gRect[1]; 
    ULONG operation;
    COLORREF clr;
    DWORD dwRColor, dwGColor, dwBColor;

    if(myGetBitDepth() != 2)
    {
        gRect[0].UpperLeft = 0;
        gRect[0].LowerRight = 1;
                
        vert [0] .x      = zx/4;
        vert [0] .y      = zy/4;
        vert [0] .Red    = 0xFF00;
        vert [0] .Green  = 0x0000;
        vert [0] .Blue   = 0x0000;
        vert [0] .Alpha  = 0x0000;
        
        vert [1] .x      = 3*zx/4;
        vert [1] .y      = 3*zy/4;
        vert [1] .Red    = 0xFF00;
        vert [1] .Green  = 0x0000;
        vert [1] .Blue   = 0x0000;
        vert [1] .Alpha  = 0x0000;

        for(int i = 0; i < 2; i++)
        {
            myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);
            
            if(i % 2)
                operation = GRADIENT_FILL_RECT_H;
            else operation = GRADIENT_FILL_RECT_V;
                
            myGradientFill(hdc,vert,2,gRect,1, operation);

            clr = GetPixel(hdc, vert[0].x, vert[0].y);
            dwRColor = GetRValue(clr);
            dwGColor = GetGValue(clr);
            dwBColor = GetBValue(clr);
            if((dwRColor & GetRValue(gcrWhite)) != GetRValue(gcrWhite) || dwGColor != 0x00 || dwBColor != 0x00)
                info(FAIL, TEXT("Expected R:0x%x G:0x00 B:0x00 at (%d,%d) got R:0x%x G:0x%x B:0x%x"), GetRValue(gcrWhite), vert[0].x, vert[0].y, dwRColor, dwGColor, dwBColor);

            clr = GetPixel(hdc, vert[1].x - 1, vert[1].y - 1);
            dwRColor = GetRValue(clr);
            dwGColor = GetGValue(clr);
            dwBColor = GetBValue(clr);
            if((dwRColor & GetRValue(gcrWhite)) != GetRValue(gcrWhite) || dwGColor != 0x00 || dwBColor != 0x00)
                info(FAIL, TEXT("Expected R:0x%x G:0x00 B:0x00 at (%d,%d) got R:0x%x G:0x%x B:0x%x"), GetRValue(gcrWhite), vert[0].x, vert[0].y, dwRColor, dwGColor, dwBColor);

            clr = GetPixel(hdc, (vert[1].x - vert[0].x)/2, (vert[1].y - vert[0].y)/2);
            dwRColor = GetRValue(clr);
            dwGColor = GetGValue(clr);
            dwBColor = GetBValue(clr);
            if((dwRColor & GetRValue(gcrWhite)) != GetRValue(gcrWhite) || dwGColor != 0x00 || dwBColor != 0x00)
                info(FAIL, TEXT("Expected R:0x%x G:0x00 B:0x00 at (%d,%d) got R:0x%x G:0x%x B:0x%x"), GetRValue(gcrWhite), vert[0].x, vert[0].y, dwRColor, dwGColor, dwBColor);
        }
    }
    myReleaseDC(hdc);
}

void
SimpleGradientFill()
{
    info(ECHO, TEXT("SimpleGradientFill test"));
    
    TDC hdc = myGetDC();
    TRIVERTEX        vert[2];
    GRADIENT_RECT    gRect[1]; 
    ULONG operation;
    COLORREF clr;
    DWORD dwRColor, dwGColor, dwBColor;

    if(myGetBitDepth() != 2)
    {
        gRect[0].UpperLeft = 0;
        gRect[0].LowerRight = 1;
                
        vert [0] .x      = zx/4;
        vert [0] .y      = zy/4;
        vert [0] .Red    = 0xFF00;
        vert [0] .Green  = 0x0000;
        vert [0] .Blue   = 0x0000;
        vert [0] .Alpha  = 0x0000;
        
        vert [1] .x      = 3*zx/4;
        vert [1] .y      = 3*zy/4;
        vert [1] .Red    = 0x0000;
        vert [1] .Green  = 0x0000;
        vert [1] .Blue   = 0xFF00;
        vert [1] .Alpha  = 0x0000;

        for(int i = 0; i < 2; i++)
        {
            myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);
            
            if(i % 2)
                operation = GRADIENT_FILL_RECT_V;
            else operation = GRADIENT_FILL_RECT_H;

            info(DETAIL, TEXT("Gradient fill (%d, %d) to (%d, %d) op %d"), vert[0].x, vert[0].y, vert[1].x, vert[1].y, operation);            
            myGradientFill(hdc,vert,2,gRect,1, operation);

            if(operation == GRADIENT_FILL_RECT_H)
            {
                clr = GetPixel(hdc, vert[0].x, (vert[1].y - vert[0].y) / 2);
                dwRColor = GetRValue(clr);
                dwGColor = GetGValue(clr);
                dwBColor = GetBValue(clr);

                // make sure it's red, allow some flexability because of dithering and 16bpp color conversions.
                if(dwRColor < 0xF0 || dwGColor > 0x08 || dwBColor > 0x08)
                    info(FAIL, TEXT("Expected R:0x%x G:0x00 B:0x00 at (%d,%d) got R:0x%x G:0x%x B:0x%x"), GetRValue(gcrWhite), vert[0].x, (vert[1].y - vert[0].y) / 2, dwRColor, dwGColor, dwBColor);

                clr = GetPixel(hdc, vert[1].x - 1, (vert[1].y - vert[0].y) / 2);
                dwRColor = GetRValue(clr);
                dwGColor = GetGValue(clr);
                dwBColor = GetBValue(clr);

                // make sure it's blue, allow some flexability because of dithering and 16bpp color conversions.
                if(dwRColor > 0x08 || dwGColor > 0x08 || dwBColor < 0xF0)
                    info(FAIL, TEXT("Expected R:0x00 G:0x00 B:0x%x at (%d,%d) got R:0x%x G:0x%x B:0x%x"), GetBValue(gcrWhite), vert[1].x - 1, (vert[1].y - vert[0].y) / 2, dwRColor, dwGColor, dwBColor);
            }
            // vertical gradient fill
            else 
            {
                clr = GetPixel(hdc, (vert[1].x - vert[0].x) / 2, vert[0].y);
                dwRColor = GetRValue(clr);
                dwGColor = GetGValue(clr);
                dwBColor = GetBValue(clr);

                // make sure it's red, allow some flexability because of dithering and 16bpp color conversions.
                if(dwRColor < 0xF0 || dwGColor > 0x08 || dwBColor > 0x08)
                    info(FAIL, TEXT("Expected R:0x%x G:0x00 B:0x00 at (%d,%d) got R:0x%x G:0x%x B:0x%x"),GetRValue(gcrWhite), (vert[1].x - vert[0].x) / 2, vert[0].y, dwRColor, dwGColor, dwBColor);

                clr = GetPixel(hdc, (vert[1].x - vert[0].x) / 2, vert[1].y - 1);
                dwRColor = GetRValue(clr);
                dwGColor = GetGValue(clr);
                dwBColor = GetBValue(clr);

                // make sure it's blue, allow some flexability because of dithering and 16bpp color conversions.
                if(dwRColor > 0x08 || dwGColor > 0x08 || dwBColor < 0xF0)
                    info(FAIL, TEXT("Expected R:0x00 G:0x00 B:0x%x at (%d,%d) got R:0x%x G:0x%x B:0x%x"), GetBValue(gcrWhite), (vert[1].x - vert[0].x) / 2, vert[1].y - 1, dwRColor, dwGColor, dwBColor);
            }
        }
    }

    myReleaseDC(hdc);
}

void
SingleGradientFillDirection(TDC hdc)
{
    TRIVERTEX        vert[2], vert2[2];
    GRADIENT_RECT    gRect[1]; 
    ULONG operation;

    int nBox[][4] = { { 0, 0, zx/2, zy},
                   { zx/2, 0, 0, zy},
                   { 0, zy, zx/2, 0},
                   { zx/2, zy, 0, 0} };
    
    gRect[0].UpperLeft = 0;
    gRect[0].LowerRight = 1;
    vert [0] .Red    = (unsigned short) (rand() % 0xFF00);
    vert [0] .Green  = (unsigned short) (rand() % 0xFF00);
    vert [0] .Blue   = (unsigned short) (rand() % 0xFF00);
    vert [0] .Alpha  = (unsigned short) (0x0000);
    vert [1] .Red    = (unsigned short) (rand() % 0xFF00);
    vert [1] .Green  = (unsigned short) (rand() % 0xFF00);
    vert [1] .Blue   = (unsigned short) (rand() % 0xFF00);
    vert [1] .Alpha  = (unsigned short) (0x0000);

    vert2[0] = vert[0];
    vert2[1] = vert[1];    
    vert2[0].x = 0;
    vert2[0].y = 0;
    vert2[1].x = zx/2;
    vert2[1].y = zy;

    
    // step through horizontal and vertical gradients
    for(int i = 0; i < 2; i++)
    {
        if(i % 2)
            operation = GRADIENT_FILL_RECT_H;
        else operation = GRADIENT_FILL_RECT_V;

        for(int j = 0; j < 4; j++)
        {
            myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);

            // in some cases, the colors are switched when the coords are switched
            if((operation == GRADIENT_FILL_RECT_V && (j == 2 || j == 3)) || (operation == GRADIENT_FILL_RECT_H && (j == 1 || j == 3)))
            {
                vert2[0].Red = vert[1].Red;
                vert2[0].Green = vert[1].Green;
                vert2[0].Blue = vert[1].Blue;
                vert2[1].Red = vert[0].Red;
                vert2[1].Green = vert[0].Green;
                vert2[1].Blue = vert[0].Blue;
            }
            else 
            {
                vert2[0].Red = vert[0].Red;
                vert2[0].Green = vert[0].Green;
                vert2[0].Blue = vert[0].Blue;
                vert2[1].Red = vert[1].Red;
                vert2[1].Green = vert[1].Green;
                vert2[1].Blue = vert[1].Blue;
            }

            
            // gradient fill the half and copy it to the right half (so they have the same dither origin)
            // a gradient fill of the exact same rectangle on a different location on the screen relative to the dither origin (0,0 normally)
            // will have slightly different dithers.
            myGradientFill(hdc, vert2, 2, gRect, 1, operation);
            BitBlt(hdc, zx/2, 0, zx/2, zy, hdc, 0, 0, SRCCOPY);
            PatBlt(hdc, 0, 0, zx/2, zy, WHITENESS);

            vert[0].x = nBox[j][0];
            vert[0].y = nBox[j][1];
            vert[1].x = nBox[j][2];
            vert[1].y = nBox[j][3];
                
            myGradientFill(hdc, vert, 2, gRect, 1, operation);

            info(DETAIL, TEXT("Gradient fill (%d, %d) to (%d, %d) op %d"), vert[0].x, vert[0].y, vert[1].x, vert[1].y, operation);
            if(!CheckScreenHalves(hdc))
                info(FAIL, TEXT("Gradient fill (%d, %d) to (%d, %d) op %d failed"), vert[0].x, vert[0].y, vert[1].x, vert[1].y, operation);
        }
    }
}

void
GradientFillDirection()
{
    info(ECHO, TEXT("GradientFillDirection test"));
    
    TDC hdc = myGetDC();
    if(myGetBitDepth() != 2)
        SingleGradientFillDirection(hdc);
    myReleaseDC(hdc);
}

void
GradientFillDirection2()
{
    info(ECHO, TEXT("GradientFillDirection2 test"));
    TDC hdc = myGetDC();
    TDC hdcCompat = CreateCompatibleDC(hdc);
    TBITMAP hbmp = NULL, hbmpStock = NULL;
    
    for(int i = 0; i < countof(gBitDepths); i++)
    {
        if(gBitDepths[i] == 2)
            continue;

        info(DETAIL, TEXT("Testing on a bitmap of depth %d"), gBitDepths[i]);
        hbmp = myCreateBitmap(zx, zy, 1, gBitDepths[i], NULL);
        hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp);
        SingleGradientFillDirection(hdcCompat);
        SelectObject(hdcCompat, hbmpStock);
        DeleteObject(hbmp);
    }
    myReleaseDC(hdc);
    DeleteDC(hdcCompat);
}

void
SingleStressGradientFill(TDC hdc)
{
    TRIVERTEX        vert[100];
    GRADIENT_RECT    gRect[100];

    int numverticies = (rand() % countof(vert))+1;
    int numrects = (rand() % countof(gRect))+1;
    int j;
    ULONG operation;

    for(j=0; j< (numverticies); j++)
    {
        // +20 - 40 to allow the rectangles to go into clipping regions
        vert [j] .x      = (unsigned short) ((rand() % zx + 20) - 40);
        vert [j] .y      = (unsigned short) ((rand() % zy + 20) - 40);
        vert [j] .Red    = (unsigned short) (rand() % 0xFF00);
        vert [j] .Green  = (unsigned short) (rand() % 0xFF00);
        vert [j] .Blue   = (unsigned short) (rand() % 0xFF00);
        vert [j] .Alpha  = (unsigned short) (0x0000);
    }

    for(j=0; j < (numrects); j++)
    {
        gRect[j].UpperLeft = rand() % numverticies;
        gRect[j].LowerRight = rand() % numverticies;
    }

    if(rand() % 2)
        operation = GRADIENT_FILL_RECT_H;
    else operation = GRADIENT_FILL_RECT_V;

    myGradientFill(hdc,vert,numverticies,gRect,numrects, operation);
}

void
StressGradientFill()
{
    info(ECHO, TEXT("StressGradientFill test"));

    TDC hdc = myGetDC();
    if(myGetBitDepth() != 2)
    {
        for(int i =0; i < 100; i++)
            SingleStressGradientFill(hdc);
    }
    myReleaseDC(hdc);
}

void
StressGradientFill2()
{
    info(ECHO, TEXT("StressGradientFill2 test"));
    TDC hdc = myGetDC();
    TDC hdcCompat = CreateCompatibleDC(hdc);
    TBITMAP hbmp = NULL, hbmpStock = NULL;

    for(int i = 0; i < countof(gBitDepths); i++)
    {
        if(gBitDepths[i] == 2)
            continue;

        info(DETAIL, TEXT("Testing on a bitmap of depth %d"), gBitDepths[i]);
        hbmp = myCreateBitmap(zx, zy, 1, gBitDepths[i], NULL);
        hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp);

        myPatBlt(hdcCompat, 0, 0, zx, zy, WHITENESS);

        for(int j=0; j < 100; j++)
            SingleStressGradientFill(hdcCompat);
        
        SelectObject(hdcCompat, hbmpStock);
        DeleteObject(hbmp);
    }
    myReleaseDC(hdc);
    DeleteDC(hdcCompat);
}

void
SingleFullScreenGradientFill(TDC hdc)
{
    TRIVERTEX        vert[2];
    GRADIENT_RECT    gRect[1];
    ULONG            operation;

    int entries[][4] = { 
                     { 0, 0, zx, zy},
                     {-1, 0, zx, zy},
                     { 0, -1, zx, zy},
                     {-1, -1, zx, zy},
                     { 0, 0, zx, zy + 1},
                     {-1, 0, zx, zy + 1},
                     { 0, -1, zx, zy + 1},
                     {-1, -1, zx, zy + 1},
                     { 0, 0, zx + 1, zy},
                     {-1, 0, zx + 1, zy},
                     { 0, -1, zx + 1, zy},
                     {-1, -1, zx + 1, zy},
                     { 0, 0, zx + 1, zy + 1},
                     {-1, 0, zx + 1, zy + 1},
                     { 0, -1, zx + 1, zy + 1},
                     {-1, -1, zx + 1, zy + 1},
                     { 0, 0, zx, zy},
                     {-10, 0, zx, zy},
                     { 0, -10, zx, zy},
                     {-10, -10, zx, zy},
                     { 0, 0, zx, zy + 10},
                     {-10, 0, zx, zy + 10},
                     { 0, -10, zx, zy + 10},
                     {-10, -10, zx, zy + 10},
                     { 0, 0, zx + 10, zy},
                     {-10, 0, zx + 10, zy},
                     { 0, -10, zx + 10, zy},
                     {-10, -10, zx + 10, zy},
                     { 0, 0, zx + 10, zy + 10},
                     {-10, 0, zx + 10, zy + 10},
                     { 0, -10, zx + 10, zy + 10},
                     {-10, -10, zx + 10, zy + 10}
                    };
    
    gRect[0].UpperLeft = 0;
    gRect[0].LowerRight = 1;

    vert [0] .Red    = (unsigned short) (rand() % 0xFF00);
    vert [0] .Green  = (unsigned short) (rand() % 0xFF00);
    vert [0] .Blue   = (unsigned short) (rand() % 0xFF00);
    vert [0] .Alpha  = (unsigned short) (0x0000);
    vert [1] .Red    = (unsigned short) (rand() % 0xFF00);
    vert [1] .Green  = (unsigned short) (rand() % 0xFF00);
    vert [1] .Blue   = (unsigned short) (rand() % 0xFF00);
    vert [1] .Alpha  = (unsigned short) (0x0000);

    for(int i = 0; i < 2; i++)
    {
        if(i % 2)
            operation = GRADIENT_FILL_RECT_V;
        else operation = GRADIENT_FILL_RECT_H;
        
        for(int j = 0; j < 32; j++)
        {  
            vert [0].x = entries[j][0];
            vert [0].y = entries[j][1];
            vert [1].x = entries[j][2];
            vert [1].y = entries[j][3];
            myGradientFill(hdc, vert, 2, gRect, 1, operation);

            vert [0].x = entries[j][2];
            vert [0].y = entries[j][1];
            vert [1].x = entries[j][0];
            vert [1].y = entries[j][3];
            myGradientFill(hdc, vert, 2, gRect, 1, operation);

            vert [0].x = entries[j][0];
            vert [0].y = entries[j][3];
            vert [1].x = entries[j][2];
            vert [1].y = entries[j][1];
            myGradientFill(hdc, vert, 2, gRect, 1, operation);

            vert [0].x = entries[j][2];
            vert [0].y = entries[j][3];
            vert [1].x = entries[j][0];
            vert [1].y = entries[j][1];
            myGradientFill(hdc, vert, 2, gRect, 1, operation);
        }
    }
}


void
FullScreenGradientFill()
{
    info(ECHO, TEXT("FullScreenGradientFill test"));
    TDC hdc = myGetDC();
    if(myGetBitDepth() != 2)
        SingleFullScreenGradientFill(hdc);
    myReleaseDC(hdc);
}

void
FullScreenGradientFill2()
{
    info(ECHO, TEXT("FullScreenGradientFill2 test"));
    TDC hdc = myGetDC();
    TDC hdcCompat = CreateCompatibleDC(hdc);
    TBITMAP hbmp = NULL, hbmpStock = NULL;
    
    for(int i = 0; i < countof(gBitDepths); i++)
    {
        if(gBitDepths[i] == 2)
            continue;

        info(DETAIL, TEXT("Testing on a bitmap of depth %d"), gBitDepths[i]);
        hbmp = myCreateBitmap(zx, zy, 1, gBitDepths[i], NULL);
        hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp);
        SingleFullScreenGradientFill(hdcCompat);
        SelectObject(hdcCompat, hbmpStock);
        DeleteObject(hbmp);
    }
    myReleaseDC(hdc);
    DeleteDC(hdcCompat);
}

void
SingleComplexGradientFill(TDC hdc)
{
    TRIVERTEX vert[64];
    GRADIENT_RECT gRect[32];

    TRIVERTEX vert2[2];
    GRADIENT_RECT gRect2[1];
    gRect2[0].UpperLeft = 0;
    gRect2[0].LowerRight = 1;
    int entry;
    int maxWidth, maxHeight, left, top, width, height;
    UINT operation;

    // do it a few times for fun
    for(int k = 0; k < 10; k++)
        // do it horizontally and vertically
        for(int j=0; j<2; j++)
        {
            if(j % 2)
                operation = GRADIENT_FILL_RECT_H;
            else operation = GRADIENT_FILL_RECT_V;

            entry = 0;
            maxHeight = zy;
            top = 0;
            
            while(maxHeight > 0)
            {
                if(maxHeight > zy/8)
                    height = (rand() % (maxHeight- zy/8)) + zy/8;
                else height = maxHeight;
                
                maxWidth = zx/2;
                left = 0;
                while(maxWidth > 0)
                {
                    if(maxWidth > zx/8)
                        width = (rand() % (maxWidth - zx/8)) + zx/8;
                    else width = maxWidth;

                    vert[entry].x = left;
                    vert[entry].y = top;
                    vert[entry] .Red = (unsigned short) (rand() % 0xFF00);
                    vert[entry] .Green = (unsigned short) (rand() % 0xFF00);
                    vert[entry] .Blue = (unsigned short) (rand() % 0xFF00);
                    vert[entry] .Alpha = (unsigned short) (0x0000);

                    vert[entry + 1].x = left + width;
                    vert[entry + 1].y = top + height;
                    vert[entry + 1] .Red = (unsigned short) (rand() % 0xFF00);
                    vert[entry + 1] .Green = (unsigned short) (rand() % 0xFF00);
                    vert[entry + 1] .Blue = (unsigned short) (rand() % 0xFF00);
                    vert[entry + 1] .Alpha = (unsigned short) (0x0000);

                    gRect[entry / 2 ].UpperLeft = entry;
                    gRect[entry / 2 ].LowerRight = entry + 1;

                    left += width;
                    maxWidth -= width;
                    entry += 2;
                }
                top += height;
                maxHeight -= height;
            }

            // gradient fill the half and copy it to the right half (so they have the same dither)
            // a gradient fill of the exact same rectangle on a different location on the screen relative to the dither origin (0,0 normally)
            // will have slightly different dithers.
            myGradientFill(hdc, vert, entry, gRect, entry/2, operation);
            BitBlt(hdc, zx/2, 0, zx/2, zy, hdc, 0, 0, SRCCOPY);
            PatBlt(hdc, 0, 0, zx/2, zy, BLACKNESS);

            for(int i = 0; i < entry/2; i++)
            {
                vert2[0] = vert[gRect[i].UpperLeft];
                vert2[1] = vert[gRect[i].LowerRight];

                myGradientFill(hdc, vert2, 2, gRect2, 1, operation);
            }

            if(!CheckScreenHalves(hdc))
               info(FAIL, TEXT("functionality with a large array doesn't match functionality of single fill"));
        }
}

void
ComplexGradientFill()
{
    info(ECHO, TEXT("ComplexGradientFill test"));

    TDC hdc = myGetDC();
    if(myGetBitDepth() != 2)
        SingleComplexGradientFill(hdc);
    myReleaseDC(hdc);
}

void
ComplexGradientFill2()
{
    info(ECHO, TEXT("ComplexGradientFill2 test"));
    
    TDC hdc = myGetDC();
    TDC hdcCompat = CreateCompatibleDC(hdc);
    TBITMAP hbmp = NULL, hbmpStock = NULL;
    
    for(int i = 0; i < countof(gBitDepths); i++)
    {
        if(gBitDepths[i] == 2)
            continue;

        info(DETAIL, TEXT("Testing on a bitmap of depth %d"), gBitDepths[i]);
        hbmp = myCreateBitmap(zx, zy, 1, gBitDepths[i], NULL);
        hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp);
        SingleComplexGradientFill(hdcCompat);
        SelectObject(hdcCompat, hbmpStock);
        DeleteObject(hbmp);
    }
    myReleaseDC(hdc);
    DeleteDC(hdcCompat);
}

void
GradientFill2bpp()
{
    info(ECHO, TEXT("GradientFill 2bpp test"));
    #ifdef UNDER_CE
    TDC hdc = myGetDC();
    TDC hdcCompat = CreateCompatibleDC(hdc);
    TBITMAP hbmp = NULL, hbmpStock = NULL;
    BYTE   *lpBits = NULL;
    int result;
    TRIVERTEX        vert[2] = {0};
    GRADIENT_RECT    gRect[1] = {0};

    gRect[0].UpperLeft = 0;
    gRect[0].LowerRight = 1;
                        
    vert [0] .x      = zx/4;
    vert [0] .y      = zy/4;
    vert [0] .Red    = 0xFF00;
    vert [0] .Green  = 0x0000;
    vert [0] .Blue   = 0x0000;
    vert [0] .Alpha  = 0x0000;
                
    vert [1] .x      = 3*zx/4;
    vert [1] .y      = 3*zy/4;
    vert [1] .Red    = 0xFF00;
    vert [1] .Green  = 0x0000;
    vert [1] .Blue   = 0x0000;
    vert [1] .Alpha  = 0x0000;

    hbmp = myCreateBitmap(zx, zy, 1, 2/*bpp*/, NULL);
    if(hbmp)
    {
        hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp);
        myPatBlt(hdcCompat, 0, 0, zx, zy, WHITENESS);
        result = gpfnGradientFill(hdcCompat, vert, 2, gRect, 1, GRADIENT_FILL_RECT_H);
        passCheck(result, 0, TEXT("aDC, vert, 2, gRect, 1, GRADIENT_FILL_RECT_H using 2bpp bitmap"));
        myGetLastError(ERROR_INVALID_PARAMETER);
        SelectObject(hdcCompat, hbmpStock);
        DeleteObject(hbmp);
    }
    else info(FAIL, TEXT("Unable to create 2bpp bitmap"));

    hbmp = myCreateDIBSection(hdc, (VOID **) & lpBits, 2, zx, zy, DIB_RGB_COLORS);
    if(hbmp)
    {
        hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp);
        myPatBlt(hdcCompat, 0, 0, zx, zy, WHITENESS);
        result = gpfnGradientFill(hdcCompat, vert, 2, gRect, 1, GRADIENT_FILL_RECT_H);
        passCheck(result, 0, TEXT("aDC, vert, 2, gRect, 1, GRADIENT_FILL_RECT_H using 2bpp DIB"));
        myGetLastError(ERROR_INVALID_PARAMETER);
        SelectObject(hdcCompat, hbmpStock);
        DeleteObject(hbmp);
    }
    else info(FAIL, TEXT("Unable to create 2bpp DIB"));

    hbmp = myCreateDIBSection(hdc, (VOID **) & lpBits, 2, zx, zy, DIB_PAL_COLORS);
    if(hbmp)
    {
        hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp);
        myPatBlt(hdcCompat, 0, 0, zx, zy, WHITENESS);
        result = gpfnGradientFill(hdcCompat, vert, 2, gRect, 1, GRADIENT_FILL_RECT_H);
        passCheck(result, 0, TEXT("aDC, vert, 2, gRect, 1, GRADIENT_FILL_RECT_H using 2bpp DIB"));
        myGetLastError(ERROR_INVALID_PARAMETER);
        SelectObject(hdcCompat, hbmpStock);
        DeleteObject(hbmp);
    }
    else info(FAIL, TEXT("Unable to create 2bpp DIB"));

    DeleteDC(hdcCompat);
    myReleaseDC(hdc);
    #endif
}

// InvertRect is implemented using BitBlt, comprehensive testing isn't needed, just verify that the wrapper is correct.
void
InvertRectTest()
{
    info(ECHO, TEXT("InvertRectTest"));
    TDC hdc = myGetDC();
    RECT rc1 = {zx/2, zy, 0, 0};
    RECT rc2 = {zx/2, 0, zx, zy};

    drawLogo(hdc, 150);
    BitBlt(hdc, zx/2, 0, zx, zy, hdc, 0, 0, SRCCOPY);
    InvertRect(hdc, &rc1);
    BitBlt(hdc, rc2.left, rc2.top, rc2.right - rc2.left, rc2.bottom - rc2.top, hdc, rc2.left, rc2.top, DSTINVERT);
    CheckScreenHalves(hdc);
    
    myReleaseDC(hdc);
}

// picks random points and moves to them.
void
RandomMoveToExTest()
{
    info(ECHO, TEXT("RandomMoveToExTest"));
    TDC hdc = myGetDC();

    const int nMaxPoints = 1000;
    int x=0, y=0, xOld = 0, yOld = 0;
    POINT pnt;
    
    if(!MoveToEx(hdc, 0, 0, NULL))
        info(FAIL, TEXT("Failed to set initial point to 0, 0"));

    for(int i = 0; i < nMaxPoints; i++)
    {
        // a random point, somewhere around the surface, not necessarily on it.
        x = (rand() % (zx + 50)) - 25;
        y = (rand() % (zy + 50)) - 25;

        SetLastError(0);
        // move to the new point
        if(!MoveToEx(hdc, x, y, &pnt))
            info(FAIL, TEXT("MoveToEx failed when expected to succeed. (%d, %d)  GLE:%d"), x, y, GetLastError());
         // if the point returned wasn't what we set it to the last time we succeeded, log a failure
        else if(xOld != pnt.x || yOld != pnt.y)
                info(FAIL, TEXT("Old point (%d, %d) not equal to point returned (%d, %d)"), xOld, yOld, pnt.x, pnt.y);

        xOld = x;
        yOld = y;

    }
    myReleaseDC(hdc);
}

// picks a random point, moves to it, and draws a line to another random point.
void
RandomMoveToLineToTest()
{
    info(ECHO, TEXT("RandomMoveToLineToTest"));
    TDC hdc = myGetDC();
    float OldMaxVerify = SetMaxErrorPercentage((float) .05);
    const int nMaxLines = 100;
    int x=0, y=0, xOld = rand() % zx, yOld = rand() % zy;
    POINT pnt;
    // make the clip region on the primary match the
    // verification driver, since lines need to match.
    HRGN hrgn = CreateRectRgn(0, 0, zx, zy);
    SelectClipRgn(hdc, hrgn);
    DeleteObject(hrgn);
    
    if(!MoveToEx(hdc, xOld, yOld, NULL))
        info(FAIL, TEXT("Failed to set initial point to (%d, %d)"), xOld, yOld);

    for(int i = 0; i < nMaxLines; i++)
    {
        // a random point, somewhere around the surface, not necessarily on it.
        x = (rand() % (zx + 50)) - 25;
        y = (rand() % (zy + 50)) - 25;

        SetLastError(0);
        // move to the new point
        if(!MoveToEx(hdc, x, y, &pnt))
            info(FAIL, TEXT("MoveToEx failed when expected to succeed. (%d, %d)  GLE:%d"), x, y, GetLastError());
         // if the point returned wasn't what we set it to the last time we succeeded, log a failure
        else if(xOld != pnt.x || yOld != pnt.y)
                info(FAIL, TEXT("Old point (%d, %d) not equal to point returned (%d, %d)"), xOld, yOld, pnt.x, pnt.y);

        x = (rand() % (zx + 50)) - 25;
        y = (rand() % (zy + 50)) - 25;

        if(!LineTo(hdc, x, y))
            info(FAIL, TEXT("LineTo Failed.  GLE:%d"), GetLastError());

        // lineto sets the new position, so we set the old point to this point, which is what MoveTo should return
        // on the next call.
        xOld = x;
        yOld = y;

    }
    myReleaseDC(hdc);
    SetMaxErrorPercentage(OldMaxVerify);
}

void
RandomLineToTest()
{
    info(ECHO, TEXT("RandomLineToTest"));
    TDC hdc = myGetDC();
    float OldMaxVerify = SetMaxErrorPercentage((float) .05);
    const int nMaxPoints = 100;
    POINT pnt[nMaxPoints];
    // make the clip region on the primary match the
    // verification driver, since lines need to match.
    HRGN hrgn = CreateRectRgn(0, 0, zx, zy);
    SelectClipRgn(hdc, hrgn);
    DeleteObject(hrgn);
    
    if(!MoveToEx(hdc, 0, 0, NULL))
        info(FAIL, TEXT("Failed to set initial point to 0, 0"));

    pnt[0].x = zx/2;
    pnt[0].y = 0;
    
    for(int i = 1; i < nMaxPoints; i++)
    {
        // a random point, somewhere around the surface, not necessarily on it.
        pnt[i].x = rand() % zx/2;
        pnt[i].y = (rand() % (zy + 50)) - 25;
        
        SetLastError(0);
        if(!LineTo(hdc, pnt[i].x, pnt[i].y))
            info(FAIL, TEXT("LineTo Failed.  GLE:%d"), GetLastError());
        pnt[i].x += zx/2;
    }
    if(!Polyline(hdc, pnt, nMaxPoints))
        info(FAIL, TEXT("PolyLine failed when creating match"));

    CheckScreenHalves(hdc);
    
    myReleaseDC(hdc);
    SetMaxErrorPercentage(OldMaxVerify);
}

void
GetSetDIBColorTable()
{
    info(ECHO, TEXT("GetSetDIBColorTable"));
    TDC hdc = myGetDC();
    TDC hdcCompat;
    TBITMAP hbmp;
    TBITMAP hbmpStock;
    BYTE * lpBits = NULL;
    RGBQUAD rgbqSet[256];
    RGBQUAD rgbqRetrieve[256];
    int bitDepth[] = {1, 2, 4, 8 };
    int nNumEntries[] = { 2, 4, 16, 256 };
    UINT nType[] = {DIB_RGB_COLORS, DIB_PAL_COLORS};
    int i, j;

    hdcCompat = CreateCompatibleDC(hdc);
    if(hdcCompat)
    {
        for(i = 0; i < countof(bitDepth); i++)
        {
            for(int k=0; k < countof(nType); k++)
            {
                if(bitDepth[i] > 8 && nType[k] == DIB_PAL_COLORS)
                    continue;

                hbmp = myCreateDIBSection(hdcCompat, (VOID **) & lpBits, bitDepth[i], zx, zy, nType[k]);
                if(hbmp)
                {
                    hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp);
                    for(j=0; j < nNumEntries[i]; j++)
                    {
                        rgbqSet[j].rgbRed = (BYTE) (rand() % 255);
                        rgbqSet[j].rgbGreen = (BYTE) (rand() % 255);
                        rgbqSet[j].rgbBlue = (BYTE) (rand() % 255);
                    }
                    SetLastError(0);
                    if(!SetDIBColorTable(hdcCompat, 0, nNumEntries[i], rgbqSet))
                        info(FAIL, TEXT("Failed to SetDIBColorTable.  GLE:%d"), GetLastError());

                    SetLastError(0);
                    if(!GetDIBColorTable(hdcCompat, 0, nNumEntries[i], rgbqRetrieve))
                        info(FAIL, TEXT("Failed to GetDIBColorTAble. GLE:%d"), GetLastError());
                    
                    for(j=0; j < nNumEntries[i]; j++)
                    {
                        if(rgbqRetrieve[j].rgbRed != rgbqSet[j].rgbRed || 
                            rgbqRetrieve[j].rgbGreen != rgbqSet[j].rgbGreen ||
                            rgbqRetrieve[j].rgbBlue != rgbqSet[j].rgbBlue)
                        {
                            info(FAIL, TEXT("Color table retrieved does not match color table set."));
                            info(FAIL, TEXT("RGB Set(%d, %d, %d), Retrieved(%d, %d, %d)"),
                                rgbqSet[j].rgbRed, rgbqSet[j].rgbGreen, rgbqSet[j].rgbBlue,
                                rgbqRetrieve[j].rgbRed, rgbqRetrieve[j].rgbGreen, rgbqRetrieve[j].rgbBlue);
                        }
                    }
                    SelectObject(hdcCompat, hbmpStock);
                    DeleteObject(hbmp);
                }
                else info(ABORT, TEXT("Failed to create DIB for testing"));
            }
        }
        DeleteDC(hdcCompat);
    }
    else info(ABORT, TEXT("Failed to create compatible DC for testing"));
    
    myReleaseDC(hdc);
}

void
LoadBitmapGetDIBColorTable()
{
    info(ECHO, TEXT("LoadBitmapGetDIBColorTable"));
    TDC hdc = myGetDC();
    TDC hdcCompat;
    TBITMAP hbmp;
    TBITMAP hbmpStock;
    RGBQUAD rgbqRetrieve[256];
    int nBitDepth[] = { 1, 4, 8 };
    TCHAR szBuf[16];

    hdcCompat = CreateCompatibleDC(hdc);
    if(hdcCompat)
    {
        for(int i = 0; i < countof(nBitDepth); i++)
        {
            _stprintf(szBuf, TEXT("BMP_GDI%d"), nBitDepth[i]);
            hbmp = myLoadBitmap(globalInst, szBuf);

            if(hbmp)
            {
                hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp);

                SetLastError(0);
                if(!GetDIBColorTable(hdcCompat, 0, (int) pow(2, nBitDepth[i]), rgbqRetrieve))
                    info(FAIL, TEXT("Failed to GetDIBColorTAble, BitDepth:%d. GLE:%d"), nBitDepth[i], GetLastError());

                SetLastError(0);
                if(SetDIBColorTable(hdcCompat, 0, (int) pow(2, nBitDepth[i]), rgbqRetrieve))
                    info(FAIL, TEXT("Able to SetDIBColorTable on a non-client bitmap."));

                SelectObject(hdcCompat, hbmpStock);
                DeleteObject(hbmp);
            }
            else info(ABORT, TEXT("Failed to load bitmap for testing"));
        }
        DeleteDC(hdcCompat);
    }
    else info(ABORT, TEXT("Failed to create compatible DC for testing"));

    myReleaseDC(hdc);
}

void
CompatBitmapGetDIBColorTable()
{
    info(ECHO, TEXT("CompatBitmapGetDIBColorTable"));

    if(myGetBitDepth() <= 8)
    {
        TDC hdc = myGetDC();
        TDC hdcCompat;
        TBITMAP hbmp;
        TBITMAP hbmpStock;
        RGBQUAD rgbqRetrieve[256];

        hdcCompat = CreateCompatibleDC(hdc);
        if(hdcCompat)
        {
                hbmp = CreateCompatibleBitmap(hdc, 200, 200);

                if(hbmp)
                {
                    hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp);

                    SetLastError(0);
                    if(!GetDIBColorTable(hdcCompat, 0, (int) pow(2, myGetBitDepth()), rgbqRetrieve))
                        info(FAIL, TEXT("Failed to GetDIBColorTAble, BitDepth:%d. GLE:%d"), myGetBitDepth(), GetLastError());

                    SetLastError(0);
                    if(SetDIBColorTable(hdcCompat, 0,  (int) pow(2, myGetBitDepth()), rgbqRetrieve))
                        info(FAIL, TEXT("Able to SetDIBColorTable on a non-client bitmap."));

                    SelectObject(hdcCompat, hbmpStock);
                    DeleteObject(hbmp);
                }
                else info(ABORT, TEXT("Failed to load bitmap for testing"));
            DeleteDC(hdcCompat);
        }
        else info(ABORT, TEXT("Failed to create compatible DC for testing"));

        myReleaseDC(hdc);
    }
}

void
DIBGetDIBColorTable()
{
    info(ECHO, TEXT("DIBGetDIBColorTable"));
    TDC hdc = myGetDC();
    TDC hdcCompat;
    TBITMAP hbmp;
    TBITMAP hbmpStock;
    RGBQUAD rgbqRetrieve[256];
    int nBitDepth[] = { 1, 
#ifdef UNDER_CE
                                  2,
#endif
                                  4, 8 };
    int nType[] = { DIB_PAL_COLORS, DIB_RGB_COLORS };
    BYTE *lpBits = NULL;
    BOOL bResult;

    hdcCompat = CreateCompatibleDC(hdc);
    if(hdcCompat)
    {
        for(int i = 0; i < countof(nBitDepth); i++)
        {
            for(int j=0; j < countof(nType); j++)
            {
                // all error reporting is done in myCreateDIBSection
                hbmp = myCreateDIBSection(hdc, (VOID**) &lpBits, nBitDepth[i], 200, 200, nType[j]);

                if(hbmp)
                {
                    hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp);

                    SetLastError(0);
                    if(!GetDIBColorTable(hdcCompat, 0, (int) pow(2, nBitDepth[i]), rgbqRetrieve))
                        info(FAIL, TEXT("Failed to GetDIBColorTAble, BitDepth:%d. GLE:%d"), nBitDepth[i], GetLastError());

                    SetLastError(0);
                    bResult = SetDIBColorTable(hdcCompat, 0, (int) pow(2, nBitDepth[i]), rgbqRetrieve);
                    if(!SetDIBColorTable(hdcCompat, 0, (int) pow(2, nBitDepth[i]), rgbqRetrieve))
                        info(FAIL, TEXT("Unable to SetDIBColorTable on a user-paletted dib."));
                    
                    SelectObject(hdcCompat, hbmpStock);
                    DeleteObject(hbmp);
                }
            }
        }
        DeleteDC(hdcCompat);
    }
    else info(ABORT, TEXT("Failed to create compatible DC for testing"));

    myReleaseDC(hdc);
}

void
BitmapGetDIBColorTable()
{
    info(ECHO, TEXT("BitmapGetDIBColorTable"));
    TDC hdc = myGetDC();
    TDC hdcCompat;
    TBITMAP hbmp;
    TBITMAP hbmpStock;
    RGBQUAD rgbqRetrieve[256];
    int nBitDepth[] = { 1, 2, 4, 8 };
    int nNumEntries[] = { 2, 4, 16, 256 };

    hdcCompat = CreateCompatibleDC(hdc);
    if(hdcCompat)
    {
        for(int i = 0; i < countof(nBitDepth); i++)
        {
            hbmp = myCreateBitmap(200, 200, 1, nBitDepth[i], NULL);

            if(hbmp)
            {
                hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp);

                SetLastError(0);
                if(!GetDIBColorTable(hdcCompat, 0, nNumEntries[i], rgbqRetrieve))
                    info(FAIL, TEXT("Failed to GetDIBColorTAble, BitDepth:%d. GLE:%d"), nBitDepth[i], GetLastError());

                SetLastError(0);
                if(SetDIBColorTable(hdcCompat, 0, nNumEntries[i], rgbqRetrieve))
                    info(FAIL, TEXT("Able to SetDIBColorTable on a system palette bitmap."));
                    
                SelectObject(hdcCompat, hbmpStock);
                DeleteObject(hbmp);
            }
            else info(ABORT, TEXT("Failed to create DIB for testing"));
        }
        DeleteDC(hdcCompat);
    }
    else info(ABORT, TEXT("Failed to create compatible DC for testing"));

    myReleaseDC(hdc);
}

// returns true if gradient fill is implemented in the display driver, false if it isn't.
BOOL
GradientFillSupported()
{
    BOOL bOldVerify = SetSurfVerify(FALSE);
    TDC hdc = myGetDC();
    BOOL bRval = TRUE;

    TRIVERTEX        vert[2];
    GRADIENT_RECT    gRect[1]; 
    
    gRect[0].UpperLeft = 0;
    gRect[0].LowerRight = 1;

    vert [0] .x      = 0;
    vert [0] .y      = 0;
    vert [0] .Red    = 0x0000;
    vert [0] .Green  = 0x0000;
    vert [0] .Blue   = 0x0000;
    vert [0] .Alpha  = 0x0000;

    vert [1] .x      = zx;
    vert [1] .y      = zy;
    vert [1] .Red    = 0x0000;
    vert [1] .Green  = 0x0000;
    vert [1] .Blue   = 0x0000;
    vert [1] .Alpha  = 0x0000;

    // if the call failes, and the error is E_NOTIMPL, then we don't have gradient fill in the display driver
    if(!gpfnGradientFill(hdc, vert, 2, gRect, 1, GRADIENT_FILL_RECT_H))
        if(ERROR_NOT_SUPPORTED == GetLastError())
            bRval = FALSE;
        
    myReleaseDC(hdc);
    SetSurfVerify(bOldVerify);
    return bRval;    
}

/***********************************************************************************
***
***   Draw APIs
***
************************************************************************************/

//***********************************************************************************
TESTPROCAPI BitBlt_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(EBitBlt);
    BogusSrcDC(EBitBlt);
    BitBltFunctionalTest(EBitBlt);
    BitBltOffScreen(EBitBlt, 0);
    BitBltOffScreen(EBitBlt, 1);
    NegativeSize(EBitBlt);
    GetSetOffScreen(EBitBlt);
    GetSetOnScreen(EBitBlt);
    ClipBitBlt(EBitBlt);
    drawLogoBlit(EBitBlt);
    SimpleBltTest(EBitBlt);
    BltFromMonochromeTest(EBitBlt, ECreateBitmap);
    BltFromMonochromeTest(EBitBlt, ECreateDIBSection);
    BltToMonochromeTest(EBitBlt, ECreateBitmap);
    BitBltNegSrc(EBitBlt);
    BitBltTest2(EBitBlt);
    BitBltTest3(EBitBlt);

    // depth
    BitBltSuite(EBitBlt);
    BitBltSuiteFullscreen(EBitBlt);
    TestBltInRegion();

    return getCode();
}

//***********************************************************************************
TESTPROCAPI ClientToScreen_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(EClientToScreen);
    ClientScreenTest(EClientToScreen);

    // depth
    // NONE

    return getCode();
}

//***********************************************************************************
TESTPROCAPI CreateBitmap_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    CreateBitmapBadParam(ECreateBitmap);
    CreateBitmapNegSize(ECreateBitmap);
    CreateBitmapBitsTest(ECreateBitmap);
    CreateBitmap0SizeTest(ECreateBitmap);
    SimpleCreateBitmapTest(ECreateBitmap);
    SimpleCreateBitmap2(ECreateBitmap);

    // depth
    CreateBitmapSquares1bpp(ECreateBitmap);
    CreateBitmapSquares2bpp(ECreateBitmap);
    CreateBitmapSquares4bpp(ECreateBitmap);
    CreateBitmapSquares8bpp(ECreateBitmap);
    CreateBitmapSquares2432bpp(ECreateBitmap, 32);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI CreateCompatibleBitmap_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    OddCreatecompatibleBitmapTest(ECreateCompatibleBitmap);

    // depth
    SimpleCreatecompatibleBitmapTest(ECreateCompatibleBitmap, 0);
    SimpleCreatecompatibleBitmapTest(ECreateCompatibleBitmap, 1);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI CreateDIBSection_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(ECreateDIBSection);
    DIBmemDC(ECreateDIBSection, 0);
    DIBmemDC(ECreateDIBSection, 1);
    SimpleCreateDIBSectionTest2(ECreateDIBSection);
    CreateDIBSectionBadiUsage(ECreateDIBSection);

    // depth
    SimpleCreateDIBSectionTest(ECreateDIBSection, 0);
    SimpleCreateDIBSectionTest(ECreateDIBSection, 1);
    CreateDIBSectionPalBadUsage(ECreateDIBSection);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI Ellipse_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(EEllipse);
    SimpleEllipseTest();
    FillCheck(EEllipse);
    checkRect(EEllipse);
    ViewPort(EEllipse);

    // depth
    StressShapeTest(EEllipse);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI DrawEdge_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(EDrawEdge);
    DrawEdgeTest1(EDrawEdge);
    DrawEdgeTest2(EDrawEdge);
    DrawEdgeTest3(EDrawEdge);

    // depth
    IterateDrawEdge();

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetPixel_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(EGetPixel);
    GetSetPixelBounderies(EGetPixel);
    SetGetPixelGLETest(EGetPixel);

    // depth
    GetSetPixelPairs(EGetPixel);
    GetSetOnScreen(EGetPixel);
    GetSetOffScreen(EGetPixel);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI MaskBlt_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth 
    passNull2Draw(EMaskBlt);
    BogusSrcDC(EMaskBlt);
    BitBltFunctionalTest(EMaskBlt);
    BitBltOffScreen(EMaskBlt, 0);
    BitBltOffScreen(EMaskBlt, 1);
    NegativeSize(EMaskBlt);
    GetSetOffScreen(EMaskBlt);
    GetSetOnScreen(EMaskBlt);
    ClipBitBlt(EMaskBlt);
    drawLogoBlit(EMaskBlt);
    SimpleBltTest(EMaskBlt);
    BltFromMonochromeTest(EMaskBlt, ECreateBitmap);
    BltFromMonochromeTest(EMaskBlt, ECreateDIBSection);
    BltToMonochromeTest(EMaskBlt, ECreateBitmap);
#ifdef UNDER_CE 
    BogusSrcDCMaskBlt(EMaskBlt);
#endif

    // depth
    BitBltSuite(EMaskBlt);
    BitBltSuiteFullscreen(EMaskBlt);
    MaskBltTest(EMaskBlt);
    
    return getCode();
}

//***********************************************************************************
TESTPROCAPI PatBlt_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(EPatBlt);
    GetSetOnScreen(EPatBlt);
    GetSetOffScreen(EPatBlt);
    PatBltSimple(EPatBlt);
    PatBltSimple2(EPatBlt);
    TryShapes(EPatBlt);
    PatternLines(EPatBlt);
    FillCheck(EPatBlt);
    DrawSquareWinLogo(EPatBlt, 0, 0);

    // depth
    // covered in BitBlt

    return getCode();
}

//***********************************************************************************
TESTPROCAPI Polygon_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(EPolygon);
    SimpleClipRgnTest0(EPolygon);
    SimplePolyTest(EPolygon);
    StressPolyTest(EPolygon);

    // depth
    PolyTest(EPolygon);
    return getCode();
}

//***********************************************************************************
TESTPROCAPI Polyline_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(EPolyline);
    SimpleClipRgnTest0(EPolygon);
    SimplePolyTest(EPolyline);
    SimplePolylineTest(EPolyline);

    // depth
    PolyTest(EPolyline);
    PolylineTest();
    
    return getCode();
}

//***********************************************************************************
TESTPROCAPI Rectangle_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(ERectangle);
    SimpleRectTest(ERectangle);
    SimpleRectTest2(ERectangle);
    checkRect(ERectangle);
    checkRectangles(ERectangle);
    FillCheck(ERectangle);
    GetSetOnScreen(ERectangle);
    GetSetOffScreen(ERectangle);
    ViewPort(ERectangle);

    // depth
    StressShapeTest(ERectangle);
    SysmemShapeTest(ERectangle);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI FillRect_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(EFillRect);
    SimpleClipRgnTest0(EFillRect);
    FillRectTest(EFillRect);
    SimpleFillRect(EFillRect);

    // depth
    // none

    return getCode();
}

//***********************************************************************************
TESTPROCAPI RectVisible_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(ERectVisible);
    VisibleCheck(ERectVisible);

    // depth
    // none

    return getCode();
}

//***********************************************************************************
TESTPROCAPI RoundRect_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(ERoundRect);
    SimpleRoundRectTest(ERoundRect);
    FillCheck(ERoundRect);
    checkRect(ERoundRect);
    GetSetOnScreen(ERoundRect);
    GetSetOffScreen(ERoundRect);
    ViewPort(ERoundRect);

    // Depth
    StressShapeTest(ERoundRect);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI ScreenToClient_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(EScreenToClient);
    ClientScreenTest(EScreenToClient);

    // depth
    // none

    return getCode();
}

//***********************************************************************************
TESTPROCAPI SetPixel_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(ESetPixel);

    // depth
    // none

    return getCode();
}

//***********************************************************************************
TESTPROCAPI StretchBlt_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(EStretchBlt);
    BogusSrcDC(EStretchBlt);
    BitBltFunctionalTest(EStretchBlt);
    BitBltOffScreen(EStretchBlt, 0);
    BitBltOffScreen(EStretchBlt, 1);
    NegativeSize(EStretchBlt);
    GetSetOffScreen(EStretchBlt);
    GetSetOnScreen(EStretchBlt);
    StretchBltFlipMirrorTest(EStretchBlt);
    ClipBitBlt(EStretchBlt);
    drawLogoBlit(EStretchBlt);
    BltFromMonochromeTest(EStretchBlt, ECreateBitmap);
    BltFromMonochromeTest(EStretchBlt, ECreateDIBSection);
    BltToMonochromeTest(EStretchBlt, ECreateBitmap);
    SimpleBltTest(EStretchBlt);
    BlitOffScreen();
    StretchBltTest(EStretchBlt);
    StretchBltTest2(EStretchBlt);
    StretchBltTest3(EStretchBlt);
    TestSBltCase10();

    // depth
    BitBltSuite(EStretchBlt);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI TransparentImage_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(ETransparentImage);
    NegativeSize(ETransparentImage);
    BitBltFunctionalTest(ETransparentImage);
    GetSetOffScreen(ETransparentImage);
    GetSetOnScreen(ETransparentImage);
    StretchBltFlipMirrorTest(ETransparentImage);
    SimpleTransparentImageTest();
    TransparentBltErrorTest();
    TransparentBltSetPixelTest();
    TransparentBltBitmapTest();

    // depth
    BitBltSuite(ETransparentImage);
    BitBltSuiteFullscreen(ETransparentImage);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI StretchDIBits_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(EStretchDIBits);


    // depth
    // none

    return getCode();
}

//***********************************************************************************
TESTPROCAPI SetDIBitsToDevice_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(ESetDIBitsToDevice);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GradientFill_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    if(gpfnGradientFill)
    {
        if(GradientFillSupported())
        {
            // breadth
            passNull2Draw(EGradientFill);
            SimpleGradientFill();
            SolidGradientFill();
            FullScreenGradientFill();
            FullScreenGradientFill2();
            GradientFillDirection();
            GradientFillDirection2();
            GradientFill2bpp();

            // depth
            ComplexGradientFill();
            ComplexGradientFill2();
            StressGradientFill();
            StressGradientFill2();
        }
        else info(SKIP, TEXT("Gradient Fill unsupported in the display driver"));
    }
    else info(SKIP, TEXT("Gradient Fill unsupported on this image"));

    return getCode();
}

//***********************************************************************************
TESTPROCAPI InvertRect_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    InvertRectTest();

    // depth
    // none

    return getCode();
}

//***********************************************************************************
TESTPROCAPI MoveToEx_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(EMoveToEx);
    RandomMoveToExTest();

    // depth
    RandomMoveToLineToTest();

    return getCode();
}

//***********************************************************************************
TESTPROCAPI LineTo_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(ELineTo);

    // depth
    RandomLineToTest();

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetDIBColorTable_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(EGetDIBColorTable);

    // depth
    LoadBitmapGetDIBColorTable();
    BitmapGetDIBColorTable();
    DIBGetDIBColorTable();
    CompatBitmapGetDIBColorTable();

    return getCode();
}

//***********************************************************************************
TESTPROCAPI SetDIBColorTable_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(ESetDIBColorTable);

    // depth
    GetSetDIBColorTable();

    return getCode();
}

// This should be renamed to SetRop2_T and moved into the Drawing Suite.
//***********************************************************************************
TESTPROCAPI Rop2_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    SetRop2Test();

    // depth
    // none

    return getCode();
}
