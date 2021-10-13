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

   draw.cpp

Abstract:

   Gdi Tests:  Palette & Drawing APIs

***************************************************************************/

#include "global.h"
#include "fontdata.h"
#include "drawdata.h"
#include "rgnData.h"
#include "alphablend.h"

/***********************************************************************************
***
***   Universal Data used in this file
***
************************************************************************************/
#define  NUMRECTS  9
// #define MAXTEST
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
                                NAMEVALENTRY(MAKEROP4(0x00E20000, BLACKNESS)),
                                NAMEVALENTRY(MAKEROP4(BLACKNESS, SRCCOPY)),
                                NAMEVALENTRY(MAKEROP4(BLACKNESS, SRCPAINT)),
                                NAMEVALENTRY(MAKEROP4(WHITENESS, SRCCOPY)),
                                NAMEVALENTRY(MAKEROP4(SRCCOPY, 0X00AA0029)),
                                NAMEVALENTRY(MAKEROP4(SRCAND, SRCCOPY)),
                                NAMEVALENTRY(MAKEROP4(SRCCOPY, ~DSTINVERT)),
                                NAMEVALENTRY(MAKEROP4(SRCCOPY, PATINVERT)),
                                NAMEVALENTRY(MAKEROP4(0X00AA0000, SRCCOPY)),
                                NAMEVALENTRY(MAKEROP4(0X00AA0000, PATCOPY))
                                };
#ifndef MAXTEST
static NameValPair gnvRop3Array[] = {
                                NAMEVALENTRY(DSTINVERT), //5555
                                NAMEVALENTRY(SRCINVERT), // 6666
                                NAMEVALENTRY(SRCCOPY), //cccc
                                NAMEVALENTRY(SRCPAINT), //eeee
                                NAMEVALENTRY(SRCAND), //8888
                                NAMEVALENTRY(SRCERASE), //4444
                                NAMEVALENTRY(MERGECOPY), //c0c0
                                NAMEVALENTRY(MERGEPAINT), //bbbb
                                NAMEVALENTRY(NOTSRCCOPY), //3333
                                NAMEVALENTRY(NOTSRCERASE), //1111
                                NAMEVALENTRY(PATCOPY), //f0f0
                                NAMEVALENTRY(PATINVERT), //5a5a
                                NAMEVALENTRY(PATPAINT), //fbfb
                                NAMEVALENTRY(BLACKNESS), //0000
                                NAMEVALENTRY(WHITENESS), //ffff
                                NAMEVALENTRY(0x00AA0000), // NOP
                                NAMEVALENTRY(0x00B80000), // PDSPxax
                                NAMEVALENTRY(0x00A00000), // DPa
                                NAMEVALENTRY(0x00290000), // PSDPSaoxxn
                                NAMEVALENTRY(0x00490000), // PDSPDaoxxn
                                NAMEVALENTRY(0x006D0000), // PDSPDoaxxn
                                };
#else
// every possible rop3.  This takes a while and doesn't gain us mutch.
static NameValPair gnvRop3Array[] = {
                                NAMEVALENTRY(0x00000000),
                                NAMEVALENTRY(0x00010000),
                                NAMEVALENTRY(0x00020000),
                                NAMEVALENTRY(0x00020000),
                                NAMEVALENTRY(0x00030000),
                                NAMEVALENTRY(0x00040000),
                                NAMEVALENTRY(0x00050000),
                                NAMEVALENTRY(0x00060000),
                                NAMEVALENTRY(0x00070000),
                                NAMEVALENTRY(0x00080000),
                                NAMEVALENTRY(0x00090000),
                                NAMEVALENTRY(0x000a0000),
                                NAMEVALENTRY(0x000b0000),
                                NAMEVALENTRY(0x000c0000),
                                NAMEVALENTRY(0x000d0000),
                                NAMEVALENTRY(0x000e0000),
                                NAMEVALENTRY(0x000f0000),
                                NAMEVALENTRY(0x00100000),
                                NAMEVALENTRY(0x00110000),
                                NAMEVALENTRY(0x00120000),
                                NAMEVALENTRY(0x00130000),
                                NAMEVALENTRY(0x00140000),
                                NAMEVALENTRY(0x00150000),
                                NAMEVALENTRY(0x00160000),
                                NAMEVALENTRY(0x00170000),
                                NAMEVALENTRY(0x00180000),
                                NAMEVALENTRY(0x00190000),
                                NAMEVALENTRY(0x001a0000),
                                NAMEVALENTRY(0x001b0000),
                                NAMEVALENTRY(0x001c0000),
                                NAMEVALENTRY(0x001d0000),
                                NAMEVALENTRY(0x001e0000),
                                NAMEVALENTRY(0x001f0000),
                                NAMEVALENTRY(0x00200000),
                                NAMEVALENTRY(0x00210000),
                                NAMEVALENTRY(0x00220000),
                                NAMEVALENTRY(0x00220000),
                                NAMEVALENTRY(0x00230000),
                                NAMEVALENTRY(0x00240000),
                                NAMEVALENTRY(0x00250000),
                                NAMEVALENTRY(0x00260000),
                                NAMEVALENTRY(0x00270000),
                                NAMEVALENTRY(0x00280000),
                                NAMEVALENTRY(0x00290000),
                                NAMEVALENTRY(0x002a0000),
                                NAMEVALENTRY(0x002b0000),
                                NAMEVALENTRY(0x002c0000),
                                NAMEVALENTRY(0x002d0000),
                                NAMEVALENTRY(0x002e0000),
                                NAMEVALENTRY(0x002f0000),
                                NAMEVALENTRY(0x00300000),
                                NAMEVALENTRY(0x00310000),
                                NAMEVALENTRY(0x00320000),
                                NAMEVALENTRY(0x00330000),
                                NAMEVALENTRY(0x00340000),
                                NAMEVALENTRY(0x00350000),
                                NAMEVALENTRY(0x00360000),
                                NAMEVALENTRY(0x00370000),
                                NAMEVALENTRY(0x00380000),
                                NAMEVALENTRY(0x00390000),
                                NAMEVALENTRY(0x003a0000),
                                NAMEVALENTRY(0x003b0000),
                                NAMEVALENTRY(0x003c0000),
                                NAMEVALENTRY(0x003d0000),
                                NAMEVALENTRY(0x003e0000),
                                NAMEVALENTRY(0x003f0000),
                                NAMEVALENTRY(0x00400000),
                                NAMEVALENTRY(0x00410000),
                                NAMEVALENTRY(0x00420000),
                                NAMEVALENTRY(0x00430000),
                                NAMEVALENTRY(0x00440000),
                                NAMEVALENTRY(0x00450000),
                                NAMEVALENTRY(0x00460000),
                                NAMEVALENTRY(0x00470000),
                                NAMEVALENTRY(0x00480000),
                                NAMEVALENTRY(0x00490000),
                                NAMEVALENTRY(0x004a0000),
                                NAMEVALENTRY(0x004b0000),
                                NAMEVALENTRY(0x004c0000),
                                NAMEVALENTRY(0x004d0000),
                                NAMEVALENTRY(0x004e0000),
                                NAMEVALENTRY(0x004f0000),
                                NAMEVALENTRY(0x00500000),
                                NAMEVALENTRY(0x00510000),
                                NAMEVALENTRY(0x00520000),
                                NAMEVALENTRY(0x00530000),
                                NAMEVALENTRY(0x00540000),
                                NAMEVALENTRY(0x00550000),
                                NAMEVALENTRY(0x00560000),
                                NAMEVALENTRY(0x00570000),
                                NAMEVALENTRY(0x00580000),
                                NAMEVALENTRY(0x00590000),
                                NAMEVALENTRY(0x005a0000),
                                NAMEVALENTRY(0x005b0000),
                                NAMEVALENTRY(0x005c0000),
                                NAMEVALENTRY(0x005d0000),
                                NAMEVALENTRY(0x005e0000),
                                NAMEVALENTRY(0x005f0000),
                                NAMEVALENTRY(0x00600000),
                                NAMEVALENTRY(0x00610000),
                                NAMEVALENTRY(0x00620000),
                                NAMEVALENTRY(0x00630000),
                                NAMEVALENTRY(0x00640000),
                                NAMEVALENTRY(0x00650000),
                                NAMEVALENTRY(0x00660000),
                                NAMEVALENTRY(0x00670000),
                                NAMEVALENTRY(0x00680000),
                                NAMEVALENTRY(0x00690000),
                                NAMEVALENTRY(0x006a0000),
                                NAMEVALENTRY(0x006b0000),
                                NAMEVALENTRY(0x006c0000),
                                NAMEVALENTRY(0x006d0000),
                                NAMEVALENTRY(0x006e0000),
                                NAMEVALENTRY(0x006f0000),
                                NAMEVALENTRY(0x00700000),
                                NAMEVALENTRY(0x00710000),
                                NAMEVALENTRY(0x00720000),
                                NAMEVALENTRY(0x00730000),
                                NAMEVALENTRY(0x00740000),
                                NAMEVALENTRY(0x00750000),
                                NAMEVALENTRY(0x00760000),
                                NAMEVALENTRY(0x00770000),
                                NAMEVALENTRY(0x00780000),
                                NAMEVALENTRY(0x00790000),
                                NAMEVALENTRY(0x007a0000),
                                NAMEVALENTRY(0x007b0000),
                                NAMEVALENTRY(0x007c0000),
                                NAMEVALENTRY(0x007d0000),
                                NAMEVALENTRY(0x007e0000),
                                NAMEVALENTRY(0x007f0000),
                                NAMEVALENTRY(0x00800000),
                                NAMEVALENTRY(0x00810000),
                                NAMEVALENTRY(0x00820000),
                                NAMEVALENTRY(0x00830000),
                                NAMEVALENTRY(0x00840000),
                                NAMEVALENTRY(0x00850000),
                                NAMEVALENTRY(0x00860000),
                                NAMEVALENTRY(0x00870000),
                                NAMEVALENTRY(0x00880000),
                                NAMEVALENTRY(0x00890000),
                                NAMEVALENTRY(0x008a0000),
                                NAMEVALENTRY(0x008b0000),
                                NAMEVALENTRY(0x008c0000),
                                NAMEVALENTRY(0x008d0000),
                                NAMEVALENTRY(0x008e0000),
                                NAMEVALENTRY(0x008f0000),
                                NAMEVALENTRY(0x00900000),
                                NAMEVALENTRY(0x00910000),
                                NAMEVALENTRY(0x00920000),
                                NAMEVALENTRY(0x00930000),
                                NAMEVALENTRY(0x00940000),
                                NAMEVALENTRY(0x00950000),
                                NAMEVALENTRY(0x00960000),
                                NAMEVALENTRY(0x00970000),
                                NAMEVALENTRY(0x00980000),
                                NAMEVALENTRY(0x00990000),
                                NAMEVALENTRY(0x009a0000),
                                NAMEVALENTRY(0x009b0000),
                                NAMEVALENTRY(0x009c0000),
                                NAMEVALENTRY(0x009d0000),
                                NAMEVALENTRY(0x009e0000),
                                NAMEVALENTRY(0x009f0000),
                                NAMEVALENTRY(0x00a00000),
                                NAMEVALENTRY(0x00a10000),
                                NAMEVALENTRY(0x00a20000),
                                NAMEVALENTRY(0x00a30000),
                                NAMEVALENTRY(0x00a40000),
                                NAMEVALENTRY(0x00a50000),
                                NAMEVALENTRY(0x00a60000),
                                NAMEVALENTRY(0x00a70000),
                                NAMEVALENTRY(0x00a80000),
                                NAMEVALENTRY(0x00a90000),
                                NAMEVALENTRY(0x00aa0000),
                                NAMEVALENTRY(0x00ab0000),
                                NAMEVALENTRY(0x00ac0000),
                                NAMEVALENTRY(0x00ad0000),
                                NAMEVALENTRY(0x00ae0000),
                                NAMEVALENTRY(0x00af0000),
                                NAMEVALENTRY(0x00b00000),
                                NAMEVALENTRY(0x00b10000),
                                NAMEVALENTRY(0x00b20000),
                                NAMEVALENTRY(0x00b30000),
                                NAMEVALENTRY(0x00b40000),
                                NAMEVALENTRY(0x00b50000),
                                NAMEVALENTRY(0x00b60000),
                                NAMEVALENTRY(0x00b70000),
                                NAMEVALENTRY(0x00b80000),
                                NAMEVALENTRY(0x00b90000),
                                NAMEVALENTRY(0x00ba0000),
                                NAMEVALENTRY(0x00bb0000),
                                NAMEVALENTRY(0x00bc0000),
                                NAMEVALENTRY(0x00bd0000),
                                NAMEVALENTRY(0x00be0000),
                                NAMEVALENTRY(0x00bf0000),
                                NAMEVALENTRY(0x00c00000),
                                NAMEVALENTRY(0x00c10000),
                                NAMEVALENTRY(0x00c20000),
                                NAMEVALENTRY(0x00c30000),
                                NAMEVALENTRY(0x00c40000),
                                NAMEVALENTRY(0x00c50000),
                                NAMEVALENTRY(0x00c60000),
                                NAMEVALENTRY(0x00c70000),
                                NAMEVALENTRY(0x00c80000),
                                NAMEVALENTRY(0x00c90000),
                                NAMEVALENTRY(0x00ca0000),
                                NAMEVALENTRY(0x00cb0000),
                                NAMEVALENTRY(0x00cc0000),
                                NAMEVALENTRY(0x00cd0000),
                                NAMEVALENTRY(0x00ce0000),
                                NAMEVALENTRY(0x00cf0000),
                                NAMEVALENTRY(0x00d00000),
                                NAMEVALENTRY(0x00d10000),
                                NAMEVALENTRY(0x00d20000),
                                NAMEVALENTRY(0x00d30000),
                                NAMEVALENTRY(0x00d40000),
                                NAMEVALENTRY(0x00d50000),
                                NAMEVALENTRY(0x00d60000),
                                NAMEVALENTRY(0x00d70000),
                                NAMEVALENTRY(0x00d80000),
                                NAMEVALENTRY(0x00d90000),
                                NAMEVALENTRY(0x00da0000),
                                NAMEVALENTRY(0x00db0000),
                                NAMEVALENTRY(0x00dc0000),
                                NAMEVALENTRY(0x00dd0000),
                                NAMEVALENTRY(0x00de0000),
                                NAMEVALENTRY(0x00df0000),
                                NAMEVALENTRY(0x00e00000),
                                NAMEVALENTRY(0x00e10000),
                                NAMEVALENTRY(0x00e20000),
                                NAMEVALENTRY(0x00e30000),
                                NAMEVALENTRY(0x00e40000),
                                NAMEVALENTRY(0x00e50000),
                                NAMEVALENTRY(0x00e60000),
                                NAMEVALENTRY(0x00e70000),
                                NAMEVALENTRY(0x00e80000),
                                NAMEVALENTRY(0x00e90000),
                                NAMEVALENTRY(0x00ea0000),
                                NAMEVALENTRY(0x00eb0000),
                                NAMEVALENTRY(0x00ec0000),
                                NAMEVALENTRY(0x00ed0000),
                                NAMEVALENTRY(0x00ee0000),
                                NAMEVALENTRY(0x00ef0000),
                                NAMEVALENTRY(0x00f00000),
                                NAMEVALENTRY(0x00f10000),
                                NAMEVALENTRY(0x00f20000),
                                NAMEVALENTRY(0x00f30000),
                                NAMEVALENTRY(0x00f40000),
                                NAMEVALENTRY(0x00f50000),
                                NAMEVALENTRY(0x00f60000),
                                NAMEVALENTRY(0x00f70000),
                                NAMEVALENTRY(0x00f80000),
                                NAMEVALENTRY(0x00f90000),
                                NAMEVALENTRY(0x00fa0000),
                                NAMEVALENTRY(0x00fb0000),
                                NAMEVALENTRY(0x00fc0000),
                                NAMEVALENTRY(0x00fd0000),
                                NAMEVALENTRY(0x00fe0000),
                                NAMEVALENTRY(0x00ff0000)
                                };
#endif

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

    HBRUSH  hBrush,
            stockBrush;
    HPEN stockHPen;
    RECT  r = { x, y, x + sizeX, y + sizeY };

    CheckNotRESULT(NULL, hBrush = CreateSolidBrush(color));
    CheckNotRESULT(NULL, stockBrush = (HBRUSH) SelectObject(hdc, hBrush));
    CheckNotRESULT(NULL, stockHPen = (HPEN) SelectObject(hdc, (HPEN) GetStockObject(NULL_PEN)));

    switch (testFunc)
    {
        case EPatBlt:
            CheckNotRESULT(FALSE, PatBlt(hdc, x, y, sizeX, sizeY, PATCOPY));
            break;
        case ERectangle:
            CheckNotRESULT(FALSE, Rectangle(hdc, x, y, x + sizeX, y + sizeY));    // fill interior only
            break;
        case EEllipse:
            CheckNotRESULT(FALSE, Ellipse(hdc, x, y, x + sizeX, y + sizeY));
            break;
        case ERoundRect:
            CheckNotRESULT(FALSE, RoundRect(hdc, x, y, x + sizeX, y + sizeY, 0, 0));
            break;
    }

    if (sizeX < 1 || sizeY < 1)
        goodBlackRect(hdc, &r);

    CheckNotRESULT(NULL, SelectObject(hdc, stockHPen));
    CheckNotRESULT(NULL, SelectObject(hdc, stockBrush));
    CheckNotRESULT(FALSE, DeleteObject(hBrush));
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
    HBRUSH stockHBrush;
    HPEN stockHPen;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        setTestRects();

        CheckNotRESULT(NULL, stockHBrush = (HBRUSH) SelectObject(hdc, GetStockObject(BLACK_BRUSH)));
        CheckNotRESULT(NULL, stockHPen = (HPEN) SelectObject(hdc, (HPEN) GetStockObject(NULL_PEN)));

        // look at all of the tests
        while (--test >= 0)
        {
            info(DETAIL, TEXT("Testing Rect (%d %d %d %d) index:%d"), rTest[test].left, rTest[test].top, rTest[test].right,
                 rTest[test].bottom, test);

            // clear the screen
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

            // blast a rectangle
            CheckNotRESULT(FALSE, Rectangle(hdc, rTest[test].left, rTest[test].top, rTest[test].right, rTest[test].bottom));
            // cover over the rectangle
            CheckNotRESULT(FALSE, PatBlt(hdc, rTest[test].left, rTest[test].top, rTest[test].right - rTest[test].left,
                                                              rTest[test].bottom - rTest[test].top, WHITENESS));
            // is the screen clear?
            CheckAllWhite(hdc, FALSE, 0);
        }

        CheckNotRESULT(NULL, SelectObject(hdc, stockHBrush));
        CheckNotRESULT(NULL, SelectObject(hdc, stockHPen));
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

// fills a surface with an interesting pattern of blocks
void FillSurface(TDC hdc, BOOL bSolid = FALSE)
{
    int nWidth = 3;
    int nHeight = 3;
    int nBlockWidth = 9;
    int nBlockHeight = 9;
    TDC hdcTmp = NULL;
    TBITMAP hbmp = NULL;
    TBITMAP hbmpOld = NULL;
    HBRUSH hbrPattern = NULL, hbrOld = NULL;

    if(!bSolid)
    {
        CheckNotRESULT(NULL, hdcTmp = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hbmp = CreateCompatibleBitmap(hdc, nWidth * nBlockWidth, nHeight * nBlockHeight));
        CheckNotRESULT(NULL, hbmpOld = (TBITMAP) SelectObject(hdcTmp, hbmp));

        CheckNotRESULT(FALSE, PatBlt(hdcTmp, 0, 0, nWidth * nBlockWidth, nHeight * nBlockHeight, WHITENESS));

        for(int i=0; i< nWidth; i++)
            for(int j=0; j< nHeight; j++)
            {
                COLORREF cr = randColorRef();
                for(int k=0; k< nBlockWidth; k++)
                    for(int l=0; l< nBlockHeight; l++)
                    {
                        CheckNotRESULT(CLR_INVALID, SetPixel(hdcTmp, (i * nBlockWidth) + k, (j * nBlockHeight) + l, cr));
                    }
            }

        CheckNotRESULT(NULL, SelectObject(hdcTmp, hbmpOld));


        CheckNotRESULT(NULL, hbrPattern = CreatePatternBrush(hbmp));
    }
    else
    {
        CheckNotRESULT(NULL, hbrPattern = CreateSolidBrush(randColorRef()));
    }

    CheckNotRESULT(NULL, hbrOld = (HBRUSH) SelectObject(hdc, hbrPattern));
    CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx/2, zy, PATCOPY));
    CheckNotRESULT(NULL, SelectObject(hdc, hbrOld));

    if(hbrPattern)
        CheckNotRESULT(FALSE, DeleteObject(hbrPattern));
    if(hbmp)
        CheckNotRESULT(FALSE, DeleteObject(hbmp));
    if(hdcTmp)
        CheckNotRESULT(FALSE, DeleteDC(hdcTmp));
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

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        setTestRects();

        while (--test >= 0)
        {
            info(DETAIL, TEXT("Testing Rect (%d %d %d %d) index:%d"), rTest[test].left, rTest[test].top, rTest[test].right,
                 rTest[test].bottom, test);

            // clear the screen
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            // draw a black rect
            CheckNotRESULT(FALSE, PatBlt(hdc, rTest[test].left, rTest[test].top,
                                                              rTest[test].right - rTest[test].left, rTest[test].bottom - rTest[test].top, BLACKNESS));

            // fill in temps
            temp.left = rTest[test].left;
            temp.top = rTest[test].top;
            w = temp.right = rTest[test].right - rTest[test].left;  // the width
            h = temp.bottom = rTest[test].bottom - rTest[test].top; // the height

            // draw concentric white squares using patblt as a line function
            for (int i = 0; temp.right >= 0 && temp.bottom >= 0; i++)
            {
                CheckNotRESULT(FALSE, PatBlt(hdc, temp.left, temp.top, 1, temp.bottom, WHITENESS));  // L
                CheckNotRESULT(FALSE, PatBlt(hdc, temp.left, temp.top, temp.right, 1, WHITENESS));   // T
                CheckNotRESULT(FALSE, PatBlt(hdc, temp.left + temp.right, temp.top, 1, temp.bottom + 1, WHITENESS)); // R
                CheckNotRESULT(FALSE, PatBlt(hdc, temp.left, temp.top + temp.bottom, temp.right + 1, 1, WHITENESS)); // B

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
                CheckNotRESULT(FALSE, PatBlt(hdc, temp.left, temp.top, 1, temp.bottom, WHITENESS));  // L
                CheckNotRESULT(FALSE, PatBlt(hdc, temp.left + 1, temp.top, temp.right, 1, WHITENESS));   // T
                CheckNotRESULT(FALSE, PatBlt(hdc, temp.left + temp.right, temp.top + 1, 1, temp.bottom - 1, WHITENESS)); // R
                CheckNotRESULT(FALSE, PatBlt(hdc, temp.left, temp.top + temp.bottom, temp.right + 1, 1, WHITENESS)); // B
                // move outward covering missed black lines
                temp.left -= 2;
                temp.top -= 2;
                if (temp.right <= w)
                    temp.right += 4;
                if (temp.bottom <= h)
                    temp.bottom += 4;
            }
            // is the screen clear?
            CheckAllWhite(hdc, FALSE, 0);
        }
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
    SetMaxErrorPercentage((testFunc == EEllipse || testFunc == ERoundRect)? .005 : 0);
    TDC     hdc = myGetDC();
    setTestRects();
    HBRUSH stockHBrush;
    HPEN stockHPen;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, stockHBrush = (HBRUSH) SelectObject(hdc, GetStockObject(BLACK_BRUSH)));

        for (int test = 0; test < NUMRECTS; test++)
        {
            info(DETAIL, TEXT("Drawing %d %d %d %d"), rTest[test].left, rTest[test].top, rTest[test].right, rTest[test].bottom);
            // cover screen with white
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

            // blast a black rect using rectangle or a patblt
            switch (testFunc)
            {
                case EPatBlt:
                    CheckNotRESULT(FALSE, PatBlt(hdc, rTest[test].left, rTest[test].top, rTest[test].right - rTest[test].left,
                                                                     rTest[test].bottom - rTest[test].top, PATCOPY));
                    break;
                case ERectangle:
                    CheckNotRESULT(NULL, stockHPen = (HPEN) SelectObject(hdc, (HPEN) GetStockObject(NULL_PEN)));
                    CheckNotRESULT(FALSE, Rectangle(hdc, rTest[test].left, rTest[test].top, rTest[test].right, rTest[test].bottom));
                    CheckNotRESULT(NULL, SelectObject(hdc, stockHPen));
                    break;
                case EEllipse:
                    CheckNotRESULT(FALSE, Ellipse(hdc, rTest[test].left, rTest[test].top, rTest[test].right, rTest[test].bottom));
                    break;
                case ERoundRect:
                    CheckNotRESULT(FALSE, RoundRect(hdc, rTest[test].left, rTest[test].top, rTest[test].right, rTest[test].bottom, 5, 5));
                    break;
            }
            // make sure the rect was black and borders are correct
            goodBlackRect(hdc, &rTest[test]);
        }

        CheckNotRESULT(NULL, SelectObject(hdc, stockHBrush));
    }

    myReleaseDC(hdc);
    // verification tolerances must be left active until after myReleaseDC does verification.
    ResetCompareConstraints();
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
        if(gSanityChecks)
            info(ECHO,TEXT("[%d] %d %d %d %d"),test,rTest[0].top,rTest[0].left,rTest[0].right,rTest[0].bottom);

        result = RectVisible(hdc, &rTest[test]);
        if (!result)
            info(FAIL, TEXT("Visible Rect(%d %d %d %d) returned False"), rTest[test].left, rTest[test].top,
                 rTest[test].right, rTest[test].bottom);
    }

    // look for rects outside of hdc
    for (test = 0; test < NUMRECTS; test++)
    {
        // off the screen to the right
        temp.left = rTest[test].left + 5000;
        temp.top = rTest[test].top;
        temp.right = rTest[test].right + 5000;
        temp.bottom = rTest[test].bottom;
        result = RectVisible(hdc, &temp);
        if (result)
            info(FAIL, TEXT("Not visible Rect(%d %d %d %d) returned True"), temp.left, temp.top, temp.right, temp.bottom);

        // off the screen to the bottom
        temp.left = rTest[test].left;
        temp.top = rTest[test].top + 5000;
        temp.right = rTest[test].right;
        temp.bottom = rTest[test].bottom + 5000;
        result = RectVisible(hdc, &temp);
        if (result)
            info(FAIL, TEXT("Not visible Rect(%d %d %d %d) returned True"), temp.left, temp.top, temp.right, temp.bottom);

        // off the screen to the left
        temp.left = rTest[test].left - 5000;
        temp.top = rTest[test].top;
        temp.right = rTest[test].right - 5000;
        temp.bottom = rTest[test].bottom;
        result = RectVisible(hdc, &temp);
        if (result)
            info(FAIL, TEXT("Not visible Rect(%d %d %d %d) returned True"), temp.left, temp.top, temp.right, temp.bottom);

        // off the screen to the top
        temp.left = rTest[test].left;
        temp.top = rTest[test].top - 5000;
        temp.right = rTest[test].right;
        temp.bottom = rTest[test].bottom - 5000;
        result = RectVisible(hdc, &temp);
        if (result)
            info(FAIL, TEXT("Not visible Rect(%d %d %d %d) returned True"), temp.left, temp.top, temp.right, temp.bottom);

        // off the screen to the top left
        temp.left = rTest[test].left - 5000;
        temp.top = rTest[test].top - 5000;
        temp.right = rTest[test].right - 5000;
        temp.bottom = rTest[test].bottom - 5000;
        result = RectVisible(hdc, &temp);
        if (result)
            info(FAIL, TEXT("Not visible Rect(%d %d %d %d) returned True"), temp.left, temp.top, temp.right, temp.bottom);

        // off the screen to the top right
        temp.left = rTest[test].left + 5000;
        temp.top = rTest[test].top - 5000;
        temp.right = rTest[test].right + 5000;
        temp.bottom = rTest[test].bottom - 5000;
        result = RectVisible(hdc, &temp);
        if (result)
            info(FAIL, TEXT("Not visible Rect(%d %d %d %d) returned True"), temp.left, temp.top, temp.right, temp.bottom);

        // off the screen to the bottom left
        temp.left = rTest[test].left - 5000;
        temp.top = rTest[test].top + 5000;
        temp.right = rTest[test].right - 5000;
        temp.bottom = rTest[test].bottom + 5000;
        result = RectVisible(hdc, &temp);
        if (result)
            info(FAIL, TEXT("Not visible Rect(%d %d %d %d) returned True"), temp.left, temp.top, temp.right, temp.bottom);

        // off the screen to the bottom right
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

    // if we're doing a getdc(null) or createdc(null), then the task bar is in bounds, so count it.
    if (DCFlag == EFull_Screen || DCFlag == ECreateDC_Primary)
        lzy = GetDeviceCaps(hdc, VERTRES);

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // clear the screen
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, lzy, WHITENESS));

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
                    result = SetPixel(hdc, x, y, color);

                    if(gSanityChecks)
                        info(ECHO, TEXT("SetPixel (%d %d): pass=%d"), x, y, pass);

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
                    result = GetPixel(hdc, x, y);

                    if(gSanityChecks)
                        info(ECHO, TEXT("GetPixel (%d %d %d)"), x, y, pass);

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
        CheckAllWhite(hdc, FALSE, 0);
    }

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

    if (DCFlag == EFull_Screen || DCFlag == ECreateDC_Primary)
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

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        for (int i = 0; i < 8; i++)
        {
            x = (int) spot[i][0];
            y = (int) spot[i][1];
            expected = (int) spot[i][2];

            // set a pixel on a boundery using the palrgb color
            result = SetPixel(hdc, x, y, WhiteColorPal);
            if (expected == 1 && !isWhitePixel(hdc, result, x, y, 1))
            {
                info(FAIL, TEXT("SetPixel(%d,%d,0x%x): returned 0x%x: expect WHITE"), x, y, WhiteColorPal, result);
            }
            else if (expected == 0 && result != -1)
                info(FAIL, TEXT("SetPixel(%d,%d,0x%x):returned 0x%x: expect -1"), x, y, WhiteColorPal, result);

            // set a pixel on a boundery using the pal color
            result = SetPixel(hdc, x, y, WhiteColorRGB);
            if (expected == 1 && !isWhitePixel(hdc, result, x, y, 1))
            {
                info(FAIL, TEXT("SetPixel(%d,%d,0x%x):returned 0x%x: expect WHITE"), x, y, WhiteColorRGB, result);
            }
            else if (expected == 0 && result != -1)
                info(FAIL, TEXT("SetPixel(%d,%d,0x%x):returned 0x%x: expect -1"), x, y, WhiteColorRGB, result);

            // try to get this pixel
            result = GetPixel(hdc, x, y);
            if (expected == 1 && !isWhitePixel(hdc, result, x, y, 1))
            {
                info(FAIL, TEXT("GetPixel(%d,%d):returned 0x%x: expect WHITE"), x, y, result);
            }
            else if (expected == 0 && result != -1)
                info(FAIL, TEXT("GetPixel(%d,%d):returned 0x%x: expect -1"), x, y, result);
        }

        // cover up last pixel
        CheckForRESULT(CLR_INVALID, result = SetPixel(hdc, x, y, WhiteColorPal));
        if (result != -1)
           info(FAIL, TEXT("SetPixel(NULL,%d,%d,0x%x):returned 0x%x: expect -1"), x, y, WhiteColorPal, result);
        // check if it was cleared
        CheckForRESULT(CLR_INVALID, result = GetPixel(hdc, x, y));
        if (result != -1)
            info(FAIL, TEXT("GetPixel(NULL,%d,%d):returned 0x%x: expect -1"), x, y, result);
    }

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

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        for (int i = 0; i < blockSize; i++)
            blastRect(hdc, RESIZE(blocks[i][0] - offset), RESIZE(blocks[i][1]), RESIZE(blocks[i][2]),
                RESIZE(blocks[i][3]), blocks[i][4], EPatBlt);
    }

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

    NOPRINTERDCV(hdc);

    if (zy > 240 && zx > 240)
    {
        for (i = 0; i < curveSize; i++)
        {
            CheckNotRESULT(FALSE, BitBlt(hdc, RESIZE(curves[i][0] - offset), RESIZE(curves[i][1]), RESIZE(curves[i][2]), RESIZE(curves[i][3]),
                                                       hdc, RESIZE(curves[i][4] - offset), RESIZE(curves[i][5]), SRCCOPY));
        }
    }

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
                hBmp = myCreateDIBSection(hdc, (VOID **) & lpBits, gBitDepths[d], j, j, nType[i], NULL, FALSE);
                // all checks done in myCreateDIBSection
                if (hBmp)
                {
                    CheckNotRESULT(0, GetObject(hBmp, sizeof (BITMAP), &myBmp));
                    if (myBmp.bmBitsPixel != gBitDepths[d] || myBmp.bmWidth != j || myBmp.bmHeight != j)
                        info(FAIL, TEXT("expected bpp:%d, W:%d, H:%d, got bpp:%d W:%d H:%d "),
                                                gBitDepths[d], j, j, myBmp.bmBitsPixel, myBmp.bmBitsPixel, myBmp.bmWidth, myBmp.bmHeight);
                    CheckNotRESULT(FALSE, DeleteObject(hBmp));
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
        SetLastError(ERROR_SUCCESS);
        hBmp = CreateDIBSection(hdc, (LPBITMAPINFO) & bmi, num, (VOID **) & lpBits, NULL, NULL);
        if (hBmp)
        {
            info(FAIL, TEXT("CreateDIBSection didn't fail given bogus iUsage:%d"), num);
            CheckNotRESULT(FALSE, DeleteObject(hBmp));
        }
        else
        {
            CheckForLastError(ERROR_INVALID_PARAMETER);
        }
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
        setRGBQUAD(&bmi.ct[i], (BYTE) GenRand() % 0xFF, (BYTE) GenRand() % 0xFF, (BYTE) GenRand() % 0xFF);
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
                        // BUGBUG: by design, we differ from the desktop.  no point in flagging the differences as failures.
#ifdef UNDER_CE
                            if(!(planes == 1 &&
                                    (bmi.bmih.biClrUsed <= 256 || bpp >= 16 || (bpp < 16 && biCompression[comp].dwVal == BI_RGB && Options[opt].dwVal == DIB_PAL_COLORS)) &&
                                    (((bpp == 1 || bpp == 2 || bpp == 4 || bpp == 8) && biCompression[comp].dwVal == BI_RGB) ||((bpp == 16 || bpp == 24 || bpp == 32) && Options[opt].dwVal == DIB_RGB_COLORS))))
                                info(FAIL, TEXT("creation of a surface succeed when expected to fail.  planes:%d, bpp:%d, compression:%s, ClrsUsed:%d, Options:%s"), planes, bpp, biCompression[comp].szName, biClrsUsed[clrs], Options[opt].szName);
#endif

                            GetObject(hBmp, sizeof(BITMAP), &bmpData);

#ifdef UNDER_CE
                            if(bmpData.bmPlanes != planes || bmpData.bmType != 0 || bmpData.bmWidth != 10 || bmpData.bmHeight != 10 || bmpData.bmBitsPixel != bpp || bmpData.bmBits == NULL)
#else
                            // on the desktop, planes isn't necessairly obeyed.
                            if(bmpData.bmType != 0 || bmpData.bmWidth != 10 || bmpData.bmHeight != 10 || bmpData.bmBitsPixel != bpp || bmpData.bmBits == NULL)
#endif
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

        hBmp = CreateCompatibleBitmap(hdc, j, j);

        if (j > 0 && !hBmp && !nullHDC)
            info(FAIL, TEXT("hBmp(%dx%d) failed returned NULL"), j, j);
        else if (hBmp && nullHDC)
        {
            info(FAIL, TEXT("hBmp should have failed %dx%d"), j, j);
            CheckNotRESULT(FALSE, DeleteObject(hBmp));
        }
        else if(hBmp)
            CheckNotRESULT(FALSE, DeleteObject(hBmp));
    }
    if (hdc)
        myReleaseDC(hdc);
}

/***********************************************************************************
***
***   create a compatible bitmap from a call to create compatible dc w/o selecting a bitmap
***   which results in a bitmap being created compatible to the stock bitmap
***
************************************************************************************/

//***********************************************************************************
void
CreateCompatibleBitmapFromCreateCompatibleDC()
{
    info(ECHO, TEXT("Creating a compatible bitmap from a compatible dc"));

    TDC hdc            = myGetDC();
    TBITMAP hbmpStock1 = NULL;
    TBITMAP hbmpStock2 = NULL;

    // hdcCompat1 is a compatible dc w/ the stock bitmap selected into it
    TDC hdcCompat1     = NULL;
    // hbmp is a bitmap compatible to the stock bitmap
    TBITMAP hbmpCompat = NULL;
    // hdcCompat2 is a compatible dc w/ the stock bitmap selected into it
    TDC hdcCompat2     = NULL;
    // hbmp is a 1bpp bitmap (which should match the stock bitmap)
    TBITMAP hbmp       = NULL;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hdcCompat1 = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hbmpCompat = CreateCompatibleBitmap(hdcCompat1, zx/2, zy));
        CheckNotRESULT(NULL, hbmpStock1 = (TBITMAP) SelectObject(hdcCompat1, hbmpCompat));

        CheckNotRESULT(NULL, hdcCompat2 = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hbmp = myCreateBitmap(zx/2, zy, 1, 1, NULL));
        CheckNotRESULT(NULL, hbmpStock2 = (TBITMAP) SelectObject(hdcCompat2, hbmp));

        CheckNotRESULT(FALSE, PatBlt(hdcCompat1, 0, 0, zx/2, zy, WHITENESS));
        CheckNotRESULT(FALSE, PatBlt(hdcCompat2, 0, 0, zx/2, zy, WHITENESS));

        // draw the logo onto the dc with the bitmap compatible to the stock bitmap
        drawLogo(hdcCompat1, 150);
        // draw the logo onto the dc with the bitmap that's 1bpp, which should match the stock bitmap
        drawLogo(hdcCompat2, 150);

        // copy them to the primary, compatbile to the stock on the left
        CheckNotRESULT(FALSE, BitBlt(hdc, 0, 0, zx/2, zy, hdcCompat1, 0, 0, SRCCOPY));
        // 1bpp bitmap on the right
        CheckNotRESULT(FALSE, BitBlt(hdc, zx/2, 0, zx/2, zy, hdcCompat2, 0, 0, SRCCOPY));

        // they should match
        CheckScreenHalves(hdc);

        CheckNotRESULT(NULL, SelectObject(hdcCompat1, hbmpStock1));
        CheckNotRESULT(NULL, SelectObject(hdcCompat2, hbmpStock2));
        CheckNotRESULT(FALSE, DeleteObject(hbmpCompat));
        CheckNotRESULT(FALSE, DeleteObject(hbmp));
        CheckNotRESULT(FALSE, DeleteDC(hdcCompat1));
        CheckNotRESULT(FALSE, DeleteDC(hdcCompat2));
    }

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

    hdc[0] = myGetDC();

    if (!IsWritableHDC(hdc[0]))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hdc[1] = CreateCompatibleDC(hdc[0]));
        CheckNotRESULT(NULL, hdc[2] = CreateCompatibleDC(hdc[1]));

        bpp = myGetBitDepth();

        CheckNotRESULT(FALSE, PatBlt(hdc[0], 0, 0, zx, zy, WHITENESS));
        CheckNotRESULT(FALSE, PatBlt(hdc[1], 0, 0, zx, zy, WHITENESS));
        CheckNotRESULT(FALSE, PatBlt(hdc[2], 0, 0, zx, zy, WHITENESS));

        // Creating a bitmap compatible to the test DC.
        CheckNotRESULT(NULL, hBmp = CreateCompatibleBitmap(hdc[0], 40, 40));
        CheckNotRESULT(0, GetObject(hBmp, sizeof (BITMAP), &myBmp));
        // a bitmap compatible to the dc retrieved from myGetDC() should be it's bit depth and size.
        if (myBmp.bmBitsPixel != bpp || myBmp.bmWidth != 40 || myBmp.bmHeight != 40)
        {
            info(FAIL, TEXT("creating a bitmap compatible to the test DC."));
            info(FAIL, TEXT("expected bpp:%d, W:%d, H:%d, got bpp:%d W:%d H:%d "), bpp, 40, 40, myBmp.bmBitsPixel, myBmp.bmWidth, myBmp.bmHeight);
        }
        CheckNotRESULT(FALSE, DeleteObject(hBmp));

        // Creating a bitmap compatible to a dc compatible to the test dc.
        CheckNotRESULT(NULL, hBmp = CreateCompatibleBitmap(hdc[1], 40, 40));
        CheckNotRESULT(0, GetObject(hBmp, sizeof (BITMAP), &myBmp));
        // the default bitmap of a compatible DC is a 1x1 monochrome
        if (myBmp.bmBitsPixel != 1 || myBmp.bmWidth != 40 || myBmp.bmHeight != 40)
        {
            info(FAIL, TEXT("Creating a bitmap compatible to a dc compatible to the test dc."));
            info(FAIL, TEXT("expected bpp:1, w:1, d:1, got  bpp:%d W:%d H:%d "), myBmp.bmBitsPixel, myBmp.bmWidth, myBmp.bmHeight);
        }
        CheckNotRESULT(FALSE, DeleteObject(hBmp));

        // Creating a bitmap compatible to a dc compatible to a compatible dc.
        CheckNotRESULT(NULL, hBmp = CreateCompatibleBitmap(hdc[2], 40, 40));
        CheckNotRESULT(0, GetObject(hBmp, sizeof (BITMAP), &myBmp));
        // the default bitmap of a compatible DC is a 1x1 monochrome
        if (myBmp.bmBitsPixel != 1 || myBmp.bmWidth != 40 || myBmp.bmHeight != 40)
        {
            info(FAIL, TEXT("Creating a bitmap compatible to a dc compatible to a compatible dc."));
            info(FAIL, TEXT("expected bpp:1, w:1, d:1, got  bpp:%d W:%d H:%d "), myBmp.bmBitsPixel, myBmp.bmWidth, myBmp.bmHeight);
        }
        CheckNotRESULT(FALSE, DeleteObject(hBmp));

        // Creating a bitmap compatible to the test dc with a width of 0.
        CheckNotRESULT(NULL, hBmp = CreateCompatibleBitmap(hdc[0], 0, 40));
        CheckNotRESULT(0, GetObject(hBmp, sizeof (BITMAP), &myBmp));
        // the default bitmap of a compatible DC is a 1x1 monochrome
        if (myBmp.bmBitsPixel != 1 || myBmp.bmWidth != 1 || myBmp.bmHeight != 1)
        {
            info(FAIL, TEXT("Creating a bitmap compatible to the test dc with a width of 0"));
            info(FAIL, TEXT("expected bpp:1, w:1, d:1, got  bpp:%d W:%d H:%d "), myBmp.bmBitsPixel, myBmp.bmWidth, myBmp.bmHeight);
        }
        CheckNotRESULT(FALSE, DeleteObject(hBmp));

        // Creating a bitmap compatible to the test dc with a height of 0.
        CheckNotRESULT(NULL, hBmp = CreateCompatibleBitmap(hdc[0], 40, 0));
        CheckNotRESULT(0, GetObject(hBmp, sizeof (BITMAP), &myBmp));
        // the default bitmap of a compatible DC is a 1x1 monochrome
        if (myBmp.bmBitsPixel != 1 || myBmp.bmWidth != 1 || myBmp.bmHeight != 1)
        {
            info(FAIL, TEXT("Creating a bitmap compatible to the test dc with a height of 0"));
            info(FAIL, TEXT("expected bpp:1, w:1, d:1, got  bpp:%d W:%d H:%d "), myBmp.bmBitsPixel, myBmp.bmWidth, myBmp.bmHeight);
        }
        CheckNotRESULT(FALSE, DeleteObject(hBmp));

        // Creating a bitmap compatible to the test dc, with a width of 40, height of 40.
        CheckNotRESULT(NULL, hBmp = CreateCompatibleBitmap(hdc[0], 40, 40));

        if (isMemDC())
        {
            // if we're testing on a memory dc, this will succeed.
            CheckNotRESULT(NULL, hBmpTemp = (TBITMAP) SelectObject(hdc[0], hBmp));
            CheckNotRESULT(NULL, hBmpTemp = (TBITMAP) SelectObject(hdc[0], hBmpTemp));
            passCheck(hBmpTemp == hBmp, TRUE, TEXT("Incorrect bitmap returned"));
        }
        else
        {
            SetLastError(ERROR_SUCCESS);
            // if we're testing on a primary dc, this will fail.
            CheckForRESULT(NULL, hBmpTemp = (TBITMAP) SelectObject(hdc[0], hBmp));
            if(!isPrinterDC(hdc[0]))
                CheckForLastError(ERROR_INVALID_PARAMETER);
        }

        CheckNotRESULT(FALSE, DeleteObject(hBmp));
        CheckNotRESULT(FALSE, DeleteDC(hdc[1]));
        CheckNotRESULT(FALSE, DeleteDC(hdc[2]));
    }

    myReleaseDC(hdc[0]);

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
    TDC     aCompDC;
    TBITMAP hBmp, hbmpMono, hbmpColor;
    RECT    test = { 10, 10, 50, 50 };
    POINT   point,
            points[20],
           *p = points;
    RECT    r;
    HBRUSH  hBrush = NULL;
    HBRUSH  hBrushBad;
    BYTE   *lpBits = NULL;
    TBITMAP aBmp = NULL;
    TBITMAP abmpStock = NULL;
    RGBQUAD rgbqTmp;
    TRIVERTEX        vert[2] = {0};
    GRADIENT_RECT    gRect[1] = {0};
    LPBITMAPINFO pBmi;
    // used for a pointer for SetBitmapBits.
    BYTE tmp = 0;
    WORD tmp2[2] = {0};
    BLENDFUNCTION bf;

    CheckNotRESULT(NULL, aCompDC = CreateCompatibleDC(aDC));

    // create an invalid brush
    CheckNotRESULT(NULL, hBrushBad = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF)));
    CheckNotRESULT(FALSE, DeleteObject(hBrushBad));

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

    CheckNotRESULT(NULL, hBmp = CreateDIBSection(aDC, (LPBITMAPINFO) & bmi, DIB_RGB_COLORS, (VOID **) & lpBits, NULL, NULL));

    CheckNotRESULT(NULL, hbmpMono = myCreateBitmap(1, 1, 1, 1, NULL));
    CheckNotRESULT(NULL, hbmpColor = myCreateBitmap(1, 1, 1, 16, NULL));

    switch (testFunc)
    {
        case ERectangle:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, Rectangle((TDC) NULL, test.left, test.top, test.right, test.bottom));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, Rectangle(g_hdcBAD, test.left, test.top, test.right, test.bottom));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, Rectangle(g_hdcBAD2, test.left, test.top, test.right, test.bottom));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EEllipse:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, Ellipse((TDC) NULL, test.left, test.top, test.right, test.bottom));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, Ellipse(g_hdcBAD, test.left, test.top, test.right, test.bottom));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, Ellipse(g_hdcBAD2, test.left, test.top, test.right, test.bottom));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case ERoundRect:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, RoundRect((TDC) NULL, test.left, test.top, test.right, test.bottom, 10, 10));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, RoundRect(g_hdcBAD, test.left, test.top, test.right, test.bottom, 10, 10));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, RoundRect(g_hdcBAD2, test.left, test.top, test.right, test.bottom, 10, 10));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case ERectVisible:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(-1, RectVisible((TDC) NULL, &test));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(-1, RectVisible(g_hdcBAD, &test));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(-1, RectVisible(g_hdcBAD2, &test));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, RectVisible(aDC, NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);
            break;
        case EBitBlt:
            if (!isPrinterDC(NULL))
            {

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, BitBlt((TDC) NULL, test.left, test.top, test.right, test.bottom, (TDC) NULL, 0, 0, SRCCOPY));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, BitBlt(aDC, test.left, test.top, test.right, test.bottom, (TDC) NULL, 0, 0, SRCCOPY));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, BitBlt((TDC) NULL, test.left, test.top, test.right, test.bottom, g_hdcBAD, 0, 0, SRCCOPY));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, BitBlt(aDC, test.left, test.top, test.right, test.bottom, g_hdcBAD, 0, 0, SRCCOPY));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, BitBlt((TDC) NULL, test.left, test.top, test.right, test.bottom, g_hdcBAD2, 0, 0, SRCCOPY));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, BitBlt(aDC, test.left, test.top, test.right, test.bottom, g_hdcBAD2, 0, 0, SRCCOPY));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, BitBlt((TDC) NULL, test.left, test.top, test.right, test.bottom, aDC, 0, 0, SRCCOPY));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, BitBlt(g_hdcBAD, test.left, test.top, test.right, test.bottom, aDC, 0, 0, SRCCOPY));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, BitBlt(g_hdcBAD2, test.left, test.top, test.right, test.bottom, aDC, 0, 0, SRCCOPY));
                CheckForLastError(ERROR_INVALID_HANDLE);

#ifdef UNDER_CE
                // BUGBUG: invalid bitblt rop, we differ from NT, which seems to just accept this
                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, BitBlt(aDC, test.left, test.top, test.right, test.bottom, aDC, 0, 0, MAKEROP4(PATCOPY, PATINVERT)));
                CheckForLastError(ERROR_INVALID_PARAMETER);
#endif
            }
            break;
        case ECreateDIBSection:
#ifdef UNDER_CE  // Desktop succeeds this call.  Shouldn't because the bitmap is needed for the palette.
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, CreateDIBSection(g_hdcBAD, (LPBITMAPINFO) &bmi, DIB_RGB_COLORS, NULL, NULL, 0));
            CheckForLastError(ERROR_INVALID_HANDLE);
#endif
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, CreateDIBSection(g_hdcBAD2, (LPBITMAPINFO) &bmi, DIB_RGB_COLORS, NULL, NULL, 0));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, CreateDIBSection(aDC, (LPBITMAPINFO) NULL, DIB_RGB_COLORS, NULL, NULL, 0));
            CheckForLastError(ERROR_INVALID_PARAMETER);
            break;
        case EStretchBlt:
            // printer DC source results in the wrapped API calls returning true.
            if(!isPrinterDC(aDC))
            {
                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, StretchBlt((TDC) NULL, test.left, test.top, test.right, test.bottom, (TDC) NULL, 0, 0, test.right, test.bottom, SRCCOPY));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, StretchBlt(aDC, test.left, test.top, test.right, test.bottom, (TDC) NULL, 0, 0, test.right, test.bottom, SRCCOPY));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, StretchBlt((TDC) NULL, test.left, test.top, test.right, test.bottom, aDC, 0, 0, test.right, test.bottom, SRCCOPY));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, StretchBlt(g_hdcBAD, test.left, test.top, test.right, test.bottom, aDC, 0, 0, test.right, test.bottom, SRCCOPY));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, StretchBlt(g_hdcBAD2, test.left, test.top, test.right, test.bottom, aDC, 0, 0, test.right, test.bottom, SRCCOPY));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, StretchBlt(aDC, test.left, test.top, test.right, test.bottom, g_hdcBAD2, 0, 0, test.right, test.bottom, SRCCOPY));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, StretchBlt(aDC, test.left, test.top, test.right, test.bottom, g_hdcBAD2, 0, 0, test.right, test.bottom, SRCCOPY));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, StretchBlt(aDC, test.left, test.top, test.right, test.bottom, aDC, 0, 0, test.right, test.bottom, MAKEROP4(PATCOPY, PATINVERT)));
                CheckForLastError(ERROR_INVALID_PARAMETER);
            }
            break;
        case ETransparentImage:
            // printer DC source results in the wrapped API calls returning true.
            if(!isPrinterDC(aDC))
            {
                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, TransparentBlt((TDC) NULL, test.left, test.top, test.right, test.bottom, (TDC) NULL, 0, 0, test.right, test.bottom, RGB(0,0,0)));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, TransparentBlt(aDC, test.left, test.top, test.right, test.bottom, (TDC) NULL, 0, 0, test.right, test.bottom, RGB(0,0,0)));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, TransparentBlt(aDC, test.left, test.top, test.right, test.bottom, g_hdcBAD, 0, 0, test.right, test.bottom, RGB(0,0,0)));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, TransparentBlt(aDC, test.left, test.top, test.right, test.bottom, g_hdcBAD2, 0, 0, test.right, test.bottom, RGB(0,0,0)));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, TransparentBlt(aDC, test.left, test.top, test.right, test.bottom, g_hdcBAD, 0, 0, test.right, test.bottom, RGB(0,0,0)));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, TransparentBlt(aDC, test.left, test.top, test.right, test.bottom, g_hdcBAD2, 0, 0, test.right, test.bottom, RGB(0,0,0)));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, TransparentBlt(g_hdcBAD, test.left, test.top, test.right, test.bottom, aDC, 0, 0, test.right, test.bottom, RGB(0,0,0)));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, TransparentBlt(g_hdcBAD2, test.left, test.top, test.right, test.bottom, aDC, 0, 0, test.right, test.bottom, RGB(0,0,0)));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, TransparentBlt(g_hdcBAD, test.left, test.top, test.right, test.bottom, aDC, 0, 0, test.right, test.bottom, RGB(0,0,0)));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, TransparentBlt(g_hdcBAD2, test.left, test.top, test.right, test.bottom, aDC, 0, 0, test.right, test.bottom, RGB(0,0,0)));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, TransparentBlt((TDC) NULL, test.left, test.top, test.right, test.bottom, aDC, 0, 0, test.right, test.bottom, RGB(0,0,0)));
                CheckForLastError(ERROR_INVALID_HANDLE);
            }
            break;
        case EStretchDIBits:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, StretchDIBits((TDC) NULL, test.left , test.top, test.right, test.bottom,
                                        test.left, test.top, test.right, test.bottom, lpBits, pBmi, DIB_RGB_COLORS, SRCCOPY));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, StretchDIBits(g_hdcBAD, test.left , test.top, test.right, test.bottom,
                                        test.left, test.top, test.right, test.bottom, lpBits, pBmi, DIB_RGB_COLORS, SRCCOPY));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, StretchDIBits(g_hdcBAD2, test.left , test.top, test.right, test.bottom,
                                        test.left, test.top, test.right, test.bottom, lpBits, pBmi, DIB_RGB_COLORS, SRCCOPY));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, StretchDIBits(aDC, test.left , test.top, test.right, test.bottom,
                                        test.left, test.top, test.right, test.bottom, lpBits, NULL, DIB_RGB_COLORS, SRCCOPY));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, StretchDIBits(aDC, test.left , test.top, test.right, test.bottom,
                                        test.left, test.top, test.right, test.bottom, NULL, pBmi, DIB_RGB_COLORS, SRCCOPY));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            // BUGBUG: The desktop says this succeeds, but a ROP4 passed in makes no sense, and the only intelligent thing for StretchDIBits to do is to fail this.
            #ifdef UNDER_CE
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, StretchDIBits(aDC, test.left , test.top, test.right, test.bottom,
                                    test.left, test.top, test.right, test.bottom, lpBits, pBmi, DIB_RGB_COLORS, MAKEROP4(PATCOPY, PATINVERT)));
            CheckForLastError(ERROR_INVALID_PARAMETER);
            #endif
            break;
        case ESetDIBitsToDevice:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetDIBitsToDevice((TDC) NULL, test.left, test.right, test.right, test.top, test.left, test.bottom, 0, zy, NULL, NULL, DIB_RGB_COLORS));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetDIBitsToDevice(aDC, test.left, test.left, test.right, test.top, test.left, test.bottom, 0, zy, NULL, NULL, DIB_RGB_COLORS));
            CheckForLastError(ERROR_INVALID_PARAMETER);
            break;
        case EMaskBlt:
            // printer DC source results in the wrapped API calls returning true.
            if(!isPrinterDC(aDC))
            {
                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, MaskBlt((TDC) NULL, test.left, test.top, test.right, test.bottom, aDC, 0, 0, hbmpMono, 0, 0, MAKEROP4(BLACKNESS, SRCCOPY)));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, MaskBlt(aDC, test.left, test.top, test.right, test.bottom, (TDC) NULL, 0, 0, hbmpMono, 0, 0, MAKEROP4(BLACKNESS, SRCCOPY)));
                CheckForLastError(ERROR_INVALID_PARAMETER);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, MaskBlt(g_hdcBAD, test.left, test.top, test.right, test.bottom, aDC, 0, 0, hbmpMono, 0, 0, MAKEROP4(BLACKNESS, SRCCOPY)));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, MaskBlt(aDC, test.left, test.top, test.right, test.bottom, g_hdcBAD, 0, 0, hbmpMono, 0, 0, MAKEROP4(BLACKNESS, SRCCOPY)));
                CheckForLastError(ERROR_INVALID_PARAMETER);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, MaskBlt(aDC, test.left, test.top, test.right, test.bottom, aDC, 0, 0, g_hbmpBAD, 0, 0, MAKEROP4(BLACKNESS, SRCCOPY)));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, MaskBlt(g_hdcBAD2, test.left, test.top, test.right, test.bottom, aDC, 0, 0, hbmpMono, 0, 0, MAKEROP4(BLACKNESS, SRCCOPY)));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, MaskBlt(aDC, test.left, test.top, test.right, test.bottom, g_hdcBAD2, 0, 0, hbmpMono, 0, 0, MAKEROP4(BLACKNESS, SRCCOPY)));
                CheckForLastError(ERROR_INVALID_PARAMETER);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, MaskBlt(aDC, test.left, test.top, test.right, test.bottom, aDC, 0, 0, g_hbmpBAD2, 0, 0, MAKEROP4(BLACKNESS, SRCCOPY)));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, MaskBlt(aDC, test.left, test.top, test.right, test.bottom, aDC, 0, 0, hbmpColor, 0, 0, MAKEROP4(BLACKNESS, SRCCOPY)));
                CheckForLastError(ERROR_INVALID_PARAMETER);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, MaskBlt(aDC, test.left, test.top, test.right, test.bottom, aDC, 0, 0, hbmpMono, -1, 0, MAKEROP4(BLACKNESS, SRCCOPY)));
                CheckForLastError(ERROR_INVALID_PARAMETER);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, MaskBlt(aDC, test.left, test.top, test.right, test.bottom, aDC, 0, 0, hbmpMono, 0, -1, MAKEROP4(BLACKNESS, SRCCOPY)));
                CheckForLastError(ERROR_INVALID_PARAMETER);
            }
            break;
        case EGetPixel:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(-1, GetPixel((TDC) NULL, test.left, test.left));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(-1, GetPixel(g_hdcBAD, test.left, test.left));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(-1, GetPixel(g_hdcBAD2, test.left, test.left));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EPatBlt:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, PatBlt((TDC) NULL, test.left, test.top, test.right, test.bottom, BLACKNESS));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, PatBlt((TDC) NULL, 50, 50, 100, 100, PATINVERT));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, PatBlt(g_hdcBAD, 50, 50, 100, 100, PATINVERT));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, PatBlt(g_hdcBAD2, 50, 50, 100, 100, PATINVERT));
            CheckForLastError(ERROR_INVALID_HANDLE);
#ifdef UNDER_CE
            // BUGBUG: we differ from XP.  A rop4 doesn't make sense in PatBlt.
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, PatBlt(aDC, 50, 50, 100, 100, MAKEROP4(PATCOPY, PATINVERT)));
            CheckForLastError(ERROR_INVALID_PARAMETER);
#endif
            break;
        case ESetPixel:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(-1, SetPixel((TDC) NULL, test.left, test.left, RGB(0,0,0)));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(-1, SetPixel(g_hdcBAD, test.left, test.left, RGB(0,0,0)));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(-1, SetPixel(g_hdcBAD2, test.left, test.left, RGB(0,0,0)));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EClientToScreen:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, ClientToScreen(NULL,NULL));
            CheckForLastError(ERROR_INVALID_WINDOW_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, ClientToScreen(NULL, &point));
            CheckForLastError(ERROR_INVALID_WINDOW_HANDLE);
            break;
        case EScreenToClient:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, ScreenToClient(NULL,NULL));
            CheckForLastError(ERROR_INVALID_WINDOW_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, ScreenToClient(NULL, &point));
            CheckForLastError(ERROR_INVALID_WINDOW_HANDLE);
            break;
        case EPolygon:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, Polygon((TDC) NULL, p, 20));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, Polygon(g_hdcBAD, p, 20));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, Polygon(g_hdcBAD2, p, 20));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, Polygon(aDC, NULL, 20));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            // no last error expected for the following 3 scenarios.
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, Polygon(aDC, p, -1));

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, Polygon(aDC, p, 0));

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, Polygon(aDC, p, 1));
            break;
        case EPolyline:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, Polyline((TDC) NULL, p, 20));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, Polyline(g_hdcBAD, p, 20));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, Polyline(g_hdcBAD2, p, 20));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, Polyline(aDC, NULL, 20));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, Polyline(aDC, p, -1));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, Polyline(aDC, p, 1));
            CheckForLastError(ERROR_INVALID_PARAMETER);
            break;
        case EFillRect:
            CheckNotRESULT(NULL, hBrush = (HBRUSH) GetStockObject(BLACK_BRUSH));
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FillRect((TDC) NULL, &r, hBrush));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FillRect(g_hdcBAD, &r, hBrush));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FillRect(g_hdcBAD2, &r, hBrush));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FillRect((TDC) NULL, &r, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

#ifdef UNDER_CE
            // printer DC source results in the wrapped API calls returning true.
            if(!isPrinterDC(aDC))
            {
                // xp doesn't detect the bad handle
                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(0, FillRect(aDC, &test, hBrushBad));
                CheckForLastError(ERROR_INVALID_HANDLE);

                // causes an exception.
                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(0, FillRect(aDC, NULL, hBrush));
                CheckForLastError(ERROR_INVALID_PARAMETER);
            }
#endif
            break;
        case EDrawEdge:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, DrawEdge((TDC) NULL, &r, BDR_RAISEDINNER, BF_ADJUST));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, DrawEdge(g_hdcBAD, &r, BDR_RAISEDINNER, BF_ADJUST));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, DrawEdge(g_hdcBAD2, &r, BDR_RAISEDINNER, BF_ADJUST));
            CheckForLastError(ERROR_INVALID_HANDLE);

// Excepts on XP.
#ifdef UNDER_CE
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, DrawEdge(aDC, NULL, BDR_RAISEDINNER, BF_ADJUST));
            CheckForLastError(ERROR_INVALID_PARAMETER);
#endif
            break;
        case EMoveToEx:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, MoveToEx((TDC) NULL, 1, 1, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, MoveToEx(g_hdcBAD, 1, 1, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, MoveToEx(g_hdcBAD2, 1, 1, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case ELineTo:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, LineTo((TDC) NULL, 1, 1));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, LineTo(g_hdcBAD, 1, 1));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, LineTo(g_hdcBAD2, 1, 1));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EGetDIBColorTable:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetDIBColorTable((TDC) NULL, 0, 1, &rgbqTmp));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetDIBColorTable(aDC, 0, 1, NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            // if aDC is less than 16bpp, then this may succeed
            if(myGetBitDepth() > 8)
            {
                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(0, GetDIBColorTable(aDC, 0, 1, &rgbqTmp));
                CheckForLastError(ERROR_INVALID_HANDLE);
            }
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetDIBColorTable(g_hdcBAD, 0, 1, &rgbqTmp));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetDIBColorTable(g_hdcBAD2, 0, 1, &rgbqTmp));
            CheckForLastError(ERROR_INVALID_HANDLE);

            CheckNotRESULT(NULL, aBmp = myCreateDIBSection(aDC, (VOID **) & lpBits, 1, zx/2, zy, DIB_RGB_COLORS, NULL, FALSE));

            if(aBmp)
            {
                CheckNotRESULT(NULL, abmpStock = (TBITMAP) SelectObject(aCompDC, aBmp));

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(0, GetDIBColorTable(aCompDC, 0, 1, NULL));
                CheckForLastError(ERROR_INVALID_PARAMETER);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(0, GetDIBColorTable(aCompDC, 2, 1, &rgbqTmp));
                CheckForLastError(ERROR_INVALID_HANDLE);

                CheckNotRESULT(NULL, SelectObject(aCompDC, abmpStock));
                CheckNotRESULT(FALSE, DeleteObject(aBmp));
            }

            CheckNotRESULT(NULL, aBmp = myCreateDIBSection(aDC, (VOID **) & lpBits, 16, zx/2, zy, DIB_RGB_COLORS, NULL, FALSE));

            if(aBmp)
            {
                CheckNotRESULT(NULL, abmpStock = (TBITMAP) SelectObject(aCompDC, aBmp));

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(0, GetDIBColorTable(aCompDC, 2, 1, &rgbqTmp));
                CheckForLastError(ERROR_INVALID_HANDLE);

                CheckNotRESULT(NULL, SelectObject(aCompDC, abmpStock));
                CheckNotRESULT(FALSE, DeleteObject(aBmp));
            }
            break;
        case ESetDIBColorTable:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetDIBColorTable(aDC, 0, 1, NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetDIBColorTable(g_hdcBAD, 0, 1, &rgbqTmp));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetDIBColorTable(g_hdcBAD2, 0, 1, &rgbqTmp));
            CheckForLastError(ERROR_INVALID_HANDLE);

#ifdef UNDER_CE // BUGBUG: causes a stop error on XP, reported 12/3/02 to be fixed in XP SP2
            CheckNotRESULT(NULL, aBmp = myCreateDIBSection(aDC, (VOID **) & lpBits, 1, zx/2, zy, DIB_RGB_COLORS, NULL, FALSE));

            if(aBmp)
            {
                CheckNotRESULT(NULL, abmpStock = (TBITMAP) SelectObject(aCompDC, aBmp));

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(0, SetDIBColorTable(aCompDC, 0, 1, NULL));
                CheckForLastError(ERROR_INVALID_PARAMETER);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(0, SetDIBColorTable(aCompDC, 2, 1, &rgbqTmp));
                CheckForLastError(ERROR_INVALID_HANDLE);

                CheckNotRESULT(NULL, SelectObject(aCompDC, abmpStock));
                CheckNotRESULT(FALSE, DeleteObject(aBmp));
            }
            else info(DETAIL, TEXT("Failed to create a DIB for bad param testing, portion of the test skipped"));
#endif
            break;
        case ESetBitmapBits:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetBitmapBits((TBITMAP) NULL, 0, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetBitmapBits(hBmp, 10, NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetBitmapBits(hBmp, 0, &tmp));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetBitmapBits((TBITMAP) NULL, 2, tmp2));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetBitmapBits(g_hbmpBAD, 1, tmp2));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetBitmapBits(g_hbmpBAD2, 1, tmp2));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EDrawFocusRect:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, DrawFocusRect((TDC) NULL, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

#ifdef UNDER_CE
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, DrawFocusRect(aDC, NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);
#endif

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, DrawFocusRect((TDC) NULL, &r));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, DrawFocusRect(g_hdcBAD, &r));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, DrawFocusRect(g_hdcBAD2, &r));
            CheckForLastError(ERROR_INVALID_HANDLE);
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


            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, gpfnGradientFill((TDC) NULL, vert, 2, gRect, 1, GRADIENT_FILL_RECT_H));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, gpfnGradientFill(g_hdcBAD, vert, 2, gRect, 1, GRADIENT_FILL_RECT_H));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, gpfnGradientFill(g_hdcBAD2, vert, 2, gRect, 1, GRADIENT_FILL_RECT_H));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, gpfnGradientFill(aDC, NULL, 2, gRect, 1, GRADIENT_FILL_RECT_H));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, gpfnGradientFill(aDC, vert, 2, NULL, 1, GRADIENT_FILL_RECT_H));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, gpfnGradientFill(aDC, vert, 2, gRect, 0, NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, gpfnGradientFill(aDC, vert, 2, gRect, 0, GRADIENT_FILL_RECT_H));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, gpfnGradientFill(aDC, vert, 2, gRect, 1, 0x0BAD));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            // XP supports triangular gradient fills.
#ifdef UNDER_CE
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, gpfnGradientFill((TDC) NULL, NULL, 0, NULL, 0, GRADIENT_FILL_TRIANGLE));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, gpfnGradientFill(aDC, vert, 2, gRect, 1, GRADIENT_FILL_TRIANGLE));
            CheckForLastError(ERROR_INVALID_PARAMETER);
#endif
            break;
        case EAlphaBlend:
            // printer dc sources always succeed for the wrapper since it can cause problems in the tests.
            if (!isPrinterDC(NULL))
            {
                memset(&bf, 0, sizeof(BLENDFUNCTION));
                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, gpfnAlphaBlend((TDC) NULL, 0, 0, 10, 10, aDC, 20, 20, 10, 10, bf));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, gpfnAlphaBlend(g_hdcBAD, 0, 0, 10, 10, aDC, 20, 20, 10, 10, bf));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, gpfnAlphaBlend(g_hdcBAD2, 0, 0, 10, 10, aDC, 20, 20, 10, 10, bf));
                CheckForLastError(ERROR_INVALID_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(FALSE, gpfnAlphaBlend(aDC, 0, 0, 10, 10, (TDC) NULL, 20, 20, 10, 10, bf));
                CheckForLastError(ERROR_INVALID_HANDLE);
            }

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, gpfnAlphaBlend(aDC, 0, 0, 10, 10, g_hdcBAD, 20, 20, 10, 10, bf));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, gpfnAlphaBlend(aDC, 0, 0, 10, 10, g_hdcBAD2, 20, 20, 10, 10, bf));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EMapWindowPoints:
            {
                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(0, MapWindowPoints(NULL, NULL, &point, 0));

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(0, MapWindowPoints(NULL, (HWND) -1, &point, 1));
                CheckForLastError(ERROR_INVALID_WINDOW_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(0, MapWindowPoints(NULL, (HWND) g_hdcBAD, &point, 1));
                CheckForLastError(ERROR_INVALID_WINDOW_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(0, MapWindowPoints((HWND) -1, NULL, &point, 1));
                CheckForLastError(ERROR_INVALID_WINDOW_HANDLE);

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(0, MapWindowPoints((HWND) g_hdcBAD, NULL, &point, 1));
                CheckForLastError(ERROR_INVALID_WINDOW_HANDLE);

                point.x = 10;
                point.y = 20;
                if(DCFlag == EWin_Primary)
                {
                    SetLastError(ERROR_SUCCESS);
                    MapWindowPoints(NULL, myHwnd, &point, 1);

                    SetLastError(ERROR_SUCCESS);
                    MapWindowPoints(myHwnd, NULL, &point, 0);
                }
                else
                {
                    SetLastError(ERROR_SUCCESS);
                    CheckForRESULT(0, MapWindowPoints(NULL, myHwnd, &point, 1));

                    SetLastError(ERROR_SUCCESS);
                    CheckForRESULT(0, MapWindowPoints(myHwnd, NULL, &point, 0));
                }
            }
            break;
        case EGetROP2:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetROP2((HDC) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetROP2((HDC) g_hdcBAD));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetROP2((HDC) g_hdcBAD2));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;

        case ESetROP2:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetROP2((HDC) NULL, 0));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetROP2((HDC) g_hdcBAD, 0));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetROP2((HDC) g_hdcBAD2, 0));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;

        default:
            info(FAIL, TEXT("PassNull called on an invalid case"));
            break;
    }

    CheckNotRESULT(FALSE, DeleteObject(hbmpColor));
    CheckNotRESULT(FALSE, DeleteObject(hbmpMono));
    CheckNotRESULT(FALSE, DeleteObject(hBmp));
    CheckNotRESULT(FALSE, DeleteDC(aCompDC));
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
                CheckNotRESULT(FALSE, BitBlt(hdc, s.x, s.y, cell, cell, hdc, temp.x, temp.y, SRCCOPY));
                break;
            case EStretchBlt:
                CheckNotRESULT(FALSE, StretchBlt(hdc, s.x, s.y, cell, cell, hdc, temp.x, temp.y, cell, cell, SRCCOPY));
                break;
            case EMaskBlt:
                CheckNotRESULT(FALSE, MaskBlt(hdc, s.x, s.y, cell, cell, hdc, temp.x, temp.y, (TBITMAP) NULL, 0, 0, SRCCOPY));
                break;
            case ETransparentImage:
                CheckNotRESULT(FALSE, TransparentBlt(hdc, s.x, s.y, cell, cell, hdc, temp.x, temp.y, cell, cell, RGB(255,255,255)));
                break;
        }

        if (o.x < d.x)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, o.x, o.y, s.x - o.x, cell, BLACKNESS));
        }
        else if (o.x > d.x)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, s.x + cell, o.y, o.x - s.x, cell, BLACKNESS));
        }
        else if (o.y < d.y)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, o.x, o.y, cell, s.y - o.y, BLACKNESS));
        }
        else if (o.y > d.y)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, o.x, s.y + cell, cell, o.y - s.y, BLACKNESS));
        }

    }
    oldBlank = blank;
    blank = newBlank;
}

//***********************************************************************************
void
decorateScreen(TDC hdc)
{
    HFONT   hFont,
            hFontStock;
    LOGFONT lFont;
    RECT    bounding = { 0, 0, zx / 2, zy };
    TCHAR   word[50];
    UINT    len;
    HBRUSH  hBrush,
            stockBr;
    HRGN    hRgn;
    int     colors[2] = { RGB(192, 192, 192), RGB(128, 128, 128) }, peg = 0;

    CheckNotRESULT(NULL, hBrush = CreateSolidBrush(RGB(0, 0, 128)));
    CheckNotRESULT(NULL, stockBr = (HBRUSH) SelectObject(hdc, hBrush));
    CheckNotRESULT(NULL, hRgn = CreateRectRgnIndirect(&bounding));

    lFont;
    if (!hBrush || !stockBr || !hRgn)
    {
        info(FAIL, TEXT("Important resource failed: Brush:0%x0 stockBr:0%x0 hRgn::0%x0"), hBrush, stockBr, hRgn);
    }

    blank.x = goodRandomNum(zx / 2 / cell);
    blank.y = goodRandomNum(zy / cell);
    oldBlank.x = oldBlank.y = -1;

    CheckNotRESULT(FALSE, FillRgn(hdc, hRgn, hBrush));

    _tcscpy_s(word, TEXT("mmm")); //Microsoft Logo
    CheckNotRESULT(NULL, hFont = setupKnownFont(MSLOGO, 65, 0, 0, 0, 0, 0));
    CheckNotRESULT(NULL, hFontStock = (HFONT) SelectObject(hdc, hFont));

    CheckNotRESULT(GDI_ERROR, SetTextAlign(hdc, TA_CENTER | TA_TOP));

    len = _tcslen(word);

    CheckNotRESULT(0, SetBkMode(hdc, TRANSPARENT));
    CheckNotRESULT(CLR_INVALID, SetBkColor(hdc, RGB(255, 0, 0)));

    int     i = RESIZE(zx / 4 - RESIZE(25));

    // Warning 22019 implies that bounding.bottom + RESIZE(300) could overflow or underflow.
    // Since we're test code and these are all pretty-well-known window sizes, suppress the warning.
#pragma warning(suppress:22019)
    for (int x = bounding.top - RESIZE(200); x < bounding.bottom + RESIZE(300); x += RESIZE(100))
    {
        CheckNotRESULT(CLR_INVALID, SetTextColor(hdc, colors[0]));
        CheckNotRESULT(FALSE, ExtTextOut(hdc, i - peg, x, ETO_CLIPPED, &bounding, word, len, NULL));

        CheckNotRESULT(GDI_ERROR, SetTextColor(hdc, colors[1]));
        CheckNotRESULT(FALSE, ExtTextOut(hdc, i + RESIZE(2) - peg, x + RESIZE(4), ETO_CLIPPED, &bounding, word, len, NULL));
    }

    CheckNotRESULT(NULL, SelectObject(hdc, hFontStock));
    CheckNotRESULT(FALSE, DeleteObject(hFont));

    CheckNotRESULT(NULL, SelectObject(hdc, stockBr));
    CheckNotRESULT(FALSE, DeleteObject(hBrush));
    CheckNotRESULT(FALSE, DeleteObject(hRgn));
}

//***********************************************************************************
void
BitBltFunctionalTest(int testFunc)
{
    info(ECHO, TEXT("%s: Sliding Squares (BitBltFunctionalTest)"), funcName[testFunc]);
    NOPRINTERDCV(NULL);
    SetMaxErrorPercentage( .15);
    TDC     hdc = myGetDC();

    // for transparent image, we make the transparent color black, so only the blank
    // block is overwritten, however with 1bpp black is used other places in the image,
    // which makes this test invalid.
    if(testFunc == ETransparentImage && myGetBitDepth() == 1)
        info(DETAIL, TEXT("Test invalid with TransparentImage and 1BPP, portion skipped. %d %d"), testFunc, myGetBitDepth());
    else if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        int     i;
        const int invertMove[4] = { 2, 3, 0, 1 };

        decorateScreen(hdc);
        CheckNotRESULT(FALSE, PatBlt(hdc, trans(blank.x), trans(blank.y), cell, cell, BLACKNESS));
        CheckNotRESULT(FALSE, BitBlt(hdc, zx / 2, 0, zx / 2, zy, hdc, 0, 0, SRCCOPY));

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
    }

    myReleaseDC(hdc);
    ResetCompareConstraints();
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
    BLENDFUNCTION bf;

    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 0xFF;
    bf.AlphaFormat = 0;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, offDC = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hBmp = CreateCompatibleBitmap(hdc, zx / 2, zy));
        CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(offDC, hBmp));

        // make the offscreen dc background match the source
        CheckNotRESULT(FALSE, BitBlt(offDC, 0, 0, zx/2, zy, hdc, 0, 0, SRCCOPY));

        // draw win logo on top screen
        drawLogo(hdc, 150);

        if (pass == 0 && !isPrinterDC(hdc))
        {
            CheckNotRESULT(FALSE, BitBlt(offDC, 0, 0, zx / 2, zy, hdc, 0, 0, SRCCOPY));  // copy entire half of screen to offscreen
        }
        else
            drawLogo(offDC, 150);   // draw win logo on top screen

        // copy squares from offscreen to top screen
        switch (testFunc)
        {
            case EBitBlt:
                CheckNotRESULT(FALSE, BitBlt(hdc, zx / 2, 0, zx / 2, zy, offDC, 0, 0, SRCCOPY));
                break;
            case EStretchBlt:
                CheckNotRESULT(FALSE, StretchBlt(hdc, zx / 2, 0, zx / 2, zy, offDC, 0, 0, zx / 2, zy, SRCCOPY));
                break;
            case EMaskBlt:
                CheckNotRESULT(FALSE, MaskBlt(hdc, zx / 2, 0, zx / 2, zy, offDC, 0, 0, (TBITMAP) NULL, 0, 0, SRCCOPY));
                break;
            case EAlphaBlend:
                CheckNotRESULT(FALSE, gpfnAlphaBlend(hdc, zx / 2, 0, zx / 2, zy, offDC, 0, 0, zx / 2, zy, bf));
                break;
        }

        CheckScreenHalves(hdc);

        CheckNotRESULT(NULL, SelectObject(offDC, stockBmp));
        CheckNotRESULT(FALSE, DeleteObject(hBmp));
        CheckNotRESULT(FALSE, DeleteDC(offDC));
    }

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
    int    StepX = bOnePixel? 2 : ((GenRand() % 5)+1), // step in x drawing larger blocks
            StepY = bOnePixel ? 2 : ((GenRand() % 5)+1); // step in y for drawing larger blocks

    // create a mask bitmap for the MaskBlt call, we're doing a SRCCOPY for both foreground and background,
    // so the actual pattern shouldn't make any difference.
    TDC hdcCompat;
    TBITMAP hbmp, hbmpStock;
    BLENDFUNCTION bf;

    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 0xFF;
    bf.AlphaFormat = 0;

    if (!IsWritableHDC(dest))
    {
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
        return;
    }

    CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(src));
    CheckNotRESULT(NULL, hbmp = myCreateBitmap(zx, zy, 1, 1, NULL));
    CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));

    // fill the entire surface in 1 call, so we don't get invalid driver verification failures.
    CheckNotRESULT(FALSE, PatBlt(hdcCompat, 0, 0, zx, zy, WHITENESS));
    CheckNotRESULT(FALSE, PatBlt(hdcCompat, zx/4, 0, zx/4, zy, BLACKNESS));
    CheckNotRESULT(FALSE, PatBlt(hdcCompat, 3*zx/4, 0, zx/4, zy, BLACKNESS));

    CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
    CheckNotRESULT(FALSE, DeleteDC(hdcCompat));

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
            // BUGBUG: because the logo is randomly taller because of a bug in drawlogo, we expand the area copied by 50.
            // BUGBUG: Drawlogo to be fixed later.
            a150 = RESIZE(100);
        }
    }
    else
    {
        CheckNotRESULT(FALSE, PatBlt(dest, zx / 2, 0, zx, zy, BLACKNESS));
    }

    x = 0;
    do  // fade
    {
        y = 0;
        do // fade
        {
            for (i = a20 + x; i < zx / 2 - a20; i += (StepX)) // pixel copy
            {
                StepX = bOnePixel? 2 : ((GenRand() % 10)+1);
                for (j = a150 + y; j < zy - a150 / 2; j += (StepY))
                {
                    StepY = bOnePixel ? 2 : ((GenRand() % 10)+1);
                    switch (testFunc)
                    {
                        case EGetPixel:
                        case ESetPixel:
                            aColor = GetPixel(src, i, j);
                            CheckNotRESULT(-1, SetPixel(dest, i + zx / 2, j, aColor));
                            break;
                        case EBitBlt:
                            CheckNotRESULT(FALSE, BitBlt(dest, i + zx / 2, j, StepX, StepY, src, i, j, SRCCOPY));
                            break;
                        case EMaskBlt:
                            CheckNotRESULT(FALSE, MaskBlt(dest, i + zx / 2, j, StepX, StepY, src, i, j, hbmp, 0, 0, MAKEROP4(SRCCOPY, SRCCOPY)));
                            break;
                        case EStretchBlt:
                            CheckNotRESULT(FALSE, StretchBlt(dest, i + zx / 2, j, StepX, StepY, src, i, j, StepX, StepY, SRCCOPY));
                            break;
                        case EPatBlt:
                            CheckNotRESULT(CLR_INVALID, aColor = GetPixel(src, i, j));
                            blastRect(dest, i + zx / 2, j, 1, 1, aColor, EPatBlt);
                            break;
                        case ERectangle:
                            CheckNotRESULT(CLR_INVALID, aColor = GetPixel(src, i, j));
                            if(!g_bRTL)
                            {
                                blastRect(dest, i + zx / 2, j, 2, 2, aColor, ERectangle);
                            }
                            else
                            {
                                //2x2 drawing is off by one pixel from origin on RTL compared to LTR.
                                blastRect(dest, i + zx / 2 - 1, j, 2, 2, aColor, ERectangle);
                            }
                            break;
                        case EEllipse:
                            CheckNotRESULT(CLR_INVALID, aColor = GetPixel(src, i, j));
                            blastRect(dest, i + zx / 2, j, 2, 2, aColor, EEllipse);
                            break;
                        case ERoundRect:
                            CheckNotRESULT(CLR_INVALID, aColor = GetPixel(src, i, j));
                            if(!g_bRTL)
                                blastRect(dest, i + zx / 2 , j, 2, 2, aColor, ERoundRect);
                            else
                            {
                                //2x2 drawing is off by one pixel from origin on RTL compared to LTR.
                                blastRect(dest, i + zx / 2 - 1, j, 2, 2, aColor, ERoundRect);
                            }
                            break;
                        case ETransparentImage:
                            // make black transparent, so blitting the black portions of the logo onto the black
                            // background appears to have no effect.
                            CheckNotRESULT(FALSE, TransparentBlt(dest, i + zx / 2, j, StepX, StepY, src, i, j, StepX, StepY, RGB(0,0,0)));
                            break;
                        case EAlphaBlend:
                            if((i+zx/2 + StepX) > zx)
                            {
                                StepX = zx - (i + zx/2);
                                if(StepX < 0)
                                    StepX = 0;
                            }

                            if((j + StepY) > zy)
                            {
                                StepY = zy - j;
                                if(StepY < 0)
                                    StepY = 0;
                            }

                            // Check against integer overflow
                            if ((i+zx/2) > i)
                            {
                                CheckNotRESULT(FALSE, gpfnAlphaBlend(dest, i + zx / 2, j, StepX, StepY, src, i, j, StepX, StepY, bf));
                            }
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

    CheckNotRESULT(FALSE, DeleteObject(hbmp));
}

//***********************************************************************************
void
GetSetOffScreen(int testFunc)
{
    info(ECHO, TEXT("%s - GetSet Off Screen"), funcName[testFunc]);
    NOPRINTERDCV(NULL);

    // set up vars
    TDC     hdc   = myGetDC();
    TDC     offDC = NULL;
    TBITMAP hBmp     = NULL,
            stockBmp = NULL;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, offDC = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hBmp = CreateCompatibleBitmap(hdc, zx / 2, zy));
        CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(offDC, hBmp));

        // make the offscreen dc background match the source
        CheckNotRESULT(FALSE, BitBlt(offDC, 0, 0, zx/2, zy, hdc, 0, 0, SRCCOPY));

        // draw win logo on top screen
        drawLogo(hdc, 150);

        // copy entire half of screen to offscreen
        CheckNotRESULT(FALSE, BitBlt(offDC, 0, 0, zx / 2, zy, hdc, 0, 0, SRCCOPY));

        PixelCopy(offDC, hdc, testFunc);

        CheckScreenHalves(hdc);

        CheckNotRESULT(NULL, SelectObject(offDC, stockBmp));
        CheckNotRESULT(FALSE, DeleteObject(hBmp));
        CheckNotRESULT(FALSE, DeleteDC(offDC));
    }

    myReleaseDC(hdc);
}

//***********************************************************************************
void
GetSetOnScreen(int testFunc)
{
    info(ECHO, TEXT("%s - GetSet On Screen"), funcName[testFunc]);
    NOPRINTERDCV(NULL);

    // set up vars
    TDC     hdc = myGetDC();

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        drawLogo(hdc, 150 /* offset */ );   // draw win logo on top screen: x: zx/2 - offset
        PixelCopy(hdc, hdc, testFunc);
        CheckScreenHalves(hdc);
    }

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
    HBRUSH stockHBrush;
    HPEN stockHPen;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // clear the surface for CheckAllWhite
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
        CheckNotRESULT(NULL, stockHBrush = (HBRUSH) SelectObject(hdc, (HBRUSH) GetStockObject(BLACK_BRUSH)));
        CheckNotRESULT(NULL, stockHPen = (HPEN) SelectObject(hdc, (HPEN) GetStockObject(NULL_PEN)));
        switch (testFunc)
        {
            case ERectangle:
                // a rectangle with a null pen can never be drawn 1 pixel wide it's always 2...
                CheckNotRESULT(FALSE, Rectangle(hdc, 10, 10, 10 + 2, 10 + 2));
                expected = 1;
                break;
            case EEllipse:
                CheckNotRESULT(FALSE, Ellipse(hdc, 10, 10, 10 + 1, 10 + 1));
                expected = 0;
                break;
            case ERoundRect:
                CheckNotRESULT(FALSE, RoundRect(hdc, 10, 10, 10 + 1, 10 + 1, 0, 0));
                expected = 0;
                break;
        }
        result = CheckAllWhite(hdc, FALSE, expected);

        if (result != expected)
        {
            info(FAIL, TEXT("%s - #pixels expected: %d; actual: %d"), funcName[testFunc], expected, result);
        }

        CheckNotRESULT(NULL, SelectObject(hdc, stockHBrush));
        CheckNotRESULT(NULL, SelectObject(hdc, stockHPen));
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
    TDC     hdc      = myGetDC(),
            compDC   = NULL,
            hdcTemp  = NULL;
    TBITMAP hBmp     = NULL,
            stockBmp = NULL;
    RECT    srcRect  = { (zx / 2 - UPSIZE) / 2, (zy - UPSIZE) / 2,
                         (zx / 2 - UPSIZE) / 2 + UPSIZE, (zy - UPSIZE) / 2 + UPSIZE
                       };
    int     result   = 0;

    int     tA[4][6] = {
        {zx / 2, 0, zx / 2, zy, 0, 0},
        {zx, 0, -zx / 2, zy, zx / 2, 0},
        {zx / 2, zy, zx / 2, -zy, 0, zy},
        {zx, zy, -zx / 2, -zy, zx / 2, zy},
    };

// Windows XP uses the bottom right pixel to specify the source/destination when flipped/mirrored and
// the ROP requires a source.  XP uses the bottom/right +1 pixel to specify the destination when flipped/mirrored
// and using a sourceless ROP.  Windows CE uses the bottom/right +1 pixel for all flipped mirrored operations regardless
// of ROP.
#ifdef UNDER_CE
    // rectangle for testing inverted StretchBlt's
    int SR[2][16] = { { zx, zy, -zx/2, -zy, 0,  0, zx/2,  zy,
                        zx/2, zy, -zx/2, -zy, 0,  0, zx/2,   zy },
                      { zx/2,   0,   zx/2,   zy, 0, zy, zx/2, -zy,
                        0,  zy,   zx/2, -zy, 0,  0, zx/2,   zy}};
#else
    // rectangle for testing inverted StretchBlt's
    int SR[2][16] = { { zx - 1, zy - 1, -zx/2, -zy, 0,  0, zx/2,  zy,
                        zx/2 - 1, zy - 1, -zx/2, -zy, 0,  0, zx/2,   zy },
                      { zx/2,   0,   zx/2,   zy, 0, zy - 1, zx/2, -zy,
                        0,  zy - 1,   zx/2, -zy, 0,  0, zx/2,   zy}};
#endif

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hBmp = myLoadBitmap(globalInst, TEXT("UP")));
        CheckNotRESULT(NULL, compDC = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(compDC, hBmp));

        // hdcTemp is the source hdc for the blt functions. Since a printer DC cannot be a source,
        // we substitute the compDC as the source when hdc is a printer DC.
        if (isPrinterDC(hdc))
        {
            hdcTemp = compDC;
        }
        else
        {
            hdcTemp = hdc;
        }

        for (int i = 0; i < 4; i++)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            CheckNotRESULT(FALSE, BitBlt(hdc, srcRect.left, srcRect.top, UPSIZE, UPSIZE, compDC, 0, 0, SRCCOPY));
            switch (testFunc)
            {
                case EBitBlt:
                    CheckNotRESULT(FALSE, result = BitBlt(hdc, tA[i][0], tA[i][1], tA[i][2], tA[i][3], hdcTemp, tA[i][4], tA[i][5], SRCCOPY));
                    break;
                case EMaskBlt:
                    CheckNotRESULT(FALSE, result = MaskBlt(hdc, tA[i][0], tA[i][1], tA[i][2], tA[i][3], hdcTemp, tA[i][4], tA[i][5], (TBITMAP) NULL, 0, 0, SRCCOPY));
                    break;
                case EStretchBlt:
                    CheckNotRESULT(FALSE, result = StretchBlt(hdc, tA[i][0], tA[i][1], tA[i][2], tA[i][3], hdcTemp, 0, 0, abs(tA[i][2]), abs(tA[i][3]), SRCCOPY));
                    break;
                case ETransparentImage:
                    CheckNotRESULT(FALSE, result = TransparentBlt(hdc, tA[i][0], tA[i][1], tA[i][2], tA[i][3], hdcTemp, 0, 0, abs(tA[i][2]), abs(tA[i][3]), RGB(0,0,0)));
                    break;
            }
            if (testFunc != EStretchBlt && testFunc != ETransparentImage)
                CheckScreenHalves(hdc);
        }

        if(testFunc == EStretchBlt || testFunc == ETransparentImage)
        {
            for(int j = 0; j < 2; j++)
            {
                CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
                // put the up label on the left half
                CheckNotRESULT(FALSE, BitBlt(hdc, srcRect.left, srcRect.top, UPSIZE, UPSIZE, compDC, 0, 0, SRCCOPY));
                switch(testFunc)
                {
                    case EStretchBlt:
                        // flip the left half onto the right half
                        CheckNotRESULT(FALSE, StretchBlt(hdc, SR[j][0], SR[j][1], SR[j][2], SR[j][3], hdcTemp, SR[j][4], SR[j][5], SR[j][6], SR[j][7], SRCCOPY));
                        // flip the left half over itself
                        CheckNotRESULT(FALSE, StretchBlt(hdc, SR[j][8], SR[j][9], SR[j][10], SR[j][11], hdcTemp, SR[j][12], SR[j][13], SR[j][14], SR[j][15], SRCCOPY));
                        break;
                    case ETransparentImage:
                        // put the up label on the right half and do the flip having white transparent
                        CheckNotRESULT(FALSE, BitBlt(hdc, srcRect.left + zx/2, srcRect.top, UPSIZE, UPSIZE, compDC, 0, 0, SRCCOPY));

                        // flip the left half onto the right half, using a blue/red color as transparent, as that shouldn't exist on the surface.
                        CheckNotRESULT(FALSE, TransparentBlt(hdc, SR[j][0], SR[j][1], SR[j][2], SR[j][3], hdcTemp, SR[j][4], SR[j][5], SR[j][6], SR[j][7], RGB(255,0,255)));
                        // flip the left half over itself, using a blue/red color as transparent, as that shouldn't exist on the surface.
                        CheckNotRESULT(FALSE, TransparentBlt(hdc, SR[j][8], SR[j][9], SR[j][10], SR[j][11], hdcTemp, SR[j][12], SR[j][13], SR[j][14], SR[j][15], RGB(255,0,255)));
                        break;
                }
                // on the desktop the top row gets blended a little causing failures.
                CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, 2, WHITENESS));

                // the halves should match
                CheckScreenHalves(hdc);
            }
        }
        // don't need to do anything with hdcTemp, since it is just a copy of either compDC or hdc
        CheckNotRESULT(NULL, SelectObject(compDC, stockBmp));
        CheckNotRESULT(FALSE, DeleteDC(compDC));
        CheckNotRESULT(FALSE, DeleteObject(hBmp));
    }

    myReleaseDC(hdc);
}

void
StretchBltFlipMirrorTest(int testFunc)
{
    info(ECHO, TEXT("%s - StretchBltFlipMirrorTest"), funcName[testFunc]);

    // myGetDC now modifies zy/zx, so all tests must get them as the first thing.
    TDC     hdc = myGetDC();
    // This requires more color options than black and white, because
    // if either black or white are made transparent, the test will fail.
    // 2bpp+ is needed so the transparent color maps to something between
    // black and white.
    if(myGetBitDepth() > 1)
    {
        COLORREF crExpected = RGB(0,0,0);
        COLORREF crResult;

// Windows XP uses the bottom right pixel to specify the source/destination when flipped/mirrored and
// the ROP requires a source.  XP uses the bottom/right +1 pixel to specify the destination when flipped/mirrored
// and using a sourceless ROP.  Windows CE uses the bottom/right +1 pixel for all flipped mirrored operations regardless
// of ROP.
#ifdef UNDER_CE
        // rectangle for testing inverted StretchBlt's
        int SR[][10] = {  {  0,  0,  zx,  zy,  0,  0,  zx,  zy, 0, 0}, // 0
                               {  0,  0,  zx,  zy,  0, zy,  zx, -zy, 0, zy-1}, // 1
                               {  0,  0,  zx,  zy, zx,  0, -zx,  zy, zx-1, 0}, // 2
                               {  0,  0,  zx,  zy, zx, zy, -zx, -zy, zx-1, zy-1}, // 3
                               {  0, zy,  zx, -zy,  0,  0,  zx,  zy, 0, zy-1 }, // 4
                               {  0, zy,  zx, -zy,  0, zy,  zx, -zy, 0, 0}, // 5
                               {  0, zy,  zx, -zy, zx,  0, -zx,  zy, zx-1, zy-1}, // 6
                               {  0, zy,  zx, -zy, zx, zy, -zx, -zy, zx-1, 0}, // 7
                               { zx,  0, -zx,  zy,  0,  0,  zx,  zy, zx-1, 0}, // 8
                               { zx,  0, -zx,  zy,  0, zy,  zx, -zy, zx-1, zy-1}, // 9
                               { zx,  0, -zx,  zy, zx,  0, -zx,  zy, 0, 0}, // 10
                               { zx,  0, -zx,  zy, zx, zy, -zx, -zy, 0, zy-1}, // 11
                               { zx, zy, -zx, -zy,  0,  0,  zx,  zy, zx-1, zy-1}, // 12
                               { zx, zy, -zx, -zy,  0, zy,  zx, -zy, zx-1, 0}, // 13
                               { zx, zy, -zx, -zy, zx,  0, -zx,  zy, 0, zy-1}, // 14
                               { zx, zy, -zx, -zy, zx, zy, -zx, -zy, 0, 0}}; // 15
#else
        // rectangle for testing inverted StretchBlt's
        int SR[][10] = {  {  0,  0,  zx,  zy,  0,  0,  zx,  zy, 0, 0}, // 0
                               {  0,  0,  zx,  zy,  0, zy-1,  zx, -zy, 0, zy-1}, // 1
                               {  0,  0,  zx,  zy, zx-1,  0, -zx,  zy, zx-1, 0}, // 2
                               {  0,  0,  zx,  zy, zx-1, zy-1, -zx, -zy, zx-1, zy-1}, // 3
                               {  0, zy-1,  zx, -zy,  0,  0,  zx,  zy, 0, zy-1 }, // 4
                               {  0, zy-1,  zx, -zy,  0, zy-1,  zx, -zy, 0, 0}, // 5
                               {  0, zy-1,  zx, -zy, zx-1,  0, -zx,  zy, zx-1, zy-1}, // 6
                               {  0, zy-1,  zx, -zy, zx-1, zy-1, -zx, -zy, zx-1, 0}, // 7
                               { zx-1,  0, -zx,  zy,  0,  0,  zx,  zy, zx-1, 0}, // 8
                               { zx-1,  0, -zx,  zy,  0, zy-1,  zx, -zy, zx-1, zy-1}, // 9
                               { zx-1,  0, -zx,  zy, zx-1,  0, -zx,  zy, 0, 0}, // 10
                               { zx-1,  0, -zx,  zy, zx-1, zy-1, -zx, -zy, 0, zy-1}, // 11
                               { zx-1, zy-1, -zx, -zy,  0,  0,  zx,  zy, zx-1, zy-1}, // 12
                               { zx-1, zy-1, -zx, -zy,  0, zy-1,  zx, -zy, zx-1, 0}, // 13
                               { zx-1, zy-1, -zx, -zy, zx-1,  0, -zx,  zy, 0, zy-1}, // 14
                               { zx-1, zy-1, -zx, -zy, zx-1, zy-1, -zx, -zy, 0, 0}}; // 15
#endif
        if (!IsWritableHDC(hdc))
            info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
        else
        {
            if (!isPrinterDC(hdc))
            {
                if(testFunc == EStretchBlt || testFunc == ETransparentImage)
                {
                    for(int j=0; j<16; j++)
                    {
                        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
                        SetPixel(hdc, 0,0, RGB(0,0,0));

                        switch(testFunc)
                        {
                            case EStretchBlt:
                                // flip the left half onto the right half
                                CheckNotRESULT(FALSE, StretchBlt(hdc, SR[j][0], SR[j][1], SR[j][2], SR[j][3], hdc, SR[j][4], SR[j][5], SR[j][6], SR[j][7], SRCCOPY));
                                break;
                            case ETransparentImage:
                                // flip the left half onto the right half, make green transparent because there shouldn't be any green anywhere in this test.
                                CheckNotRESULT(FALSE, TransparentBlt(hdc, SR[j][0], SR[j][1], SR[j][2], SR[j][3], hdc, SR[j][4], SR[j][5], SR[j][6], SR[j][7], RGB(0,255,0)));
                                break;
                        }
                        CheckNotRESULT(CLR_INVALID, crResult = GetPixel(hdc, SR[j][8], SR[j][9]));

                        if(crExpected != crResult)
                        {
                            info(FAIL, TEXT("Pixel mismatch, expected %d != %d for StretchBlt or TransparentBlt(hdc, %d, %d, %d, %d, hdc, %d, %d, %d, %d)"), crExpected, crResult, SR[j][0], SR[j][1], SR[j][2], SR[j][3], SR[j][4], SR[j][5], SR[j][6], SR[j][7]);
                            info(FAIL, TEXT("Mismatch on test %d"), j);
                            OutputBitmap(hdc, zx, zy);
                        }

                    }
                }
            }
        }
    }
    else info(DETAIL, TEXT("Test invalid on 1bpp, skipping a portion of the test."));
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
        CheckForRESULT(FALSE, result = BitBlt(hdc, 0, 0, 50, 50, hdc, 0, 0, badROPs[i]));
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
    BOOL bOldVerify = SetSurfVerify(FALSE);
    info(ECHO, TEXT("%s - Bogus Source DC"), funcName[testFunc]);
    TDC     hdc = myGetDC();
    int     result = 0,
            expected = 16,
            Bpixels;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // clear the surface for the CheckAllWhite called after the test.
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

        switch (testFunc)
        {
            case EBitBlt:
                CheckNotRESULT(FALSE, result = BitBlt(hdc, 50, 50, 4, 4, g_hdcBAD, 0, 0, BLACKNESS));
                break;
            case EStretchBlt:
                CheckNotRESULT(FALSE, result = StretchBlt(hdc, 50, 50, 4, 4, g_hdcBAD, 0, 0, 4, 4, BLACKNESS));
                break;
            case EMaskBlt:
                CheckNotRESULT(FALSE, result = MaskBlt(hdc, 50, 50, 4, 4, g_hdcBAD, 0, 0, (TBITMAP) NULL, 0, 0, MAKEROP4(BLACKNESS, WHITENESS)));
                break;
        }

        if (!result)
            info(FAIL, TEXT("%s should not have failed given bogus srcDC"), funcName[testFunc]);

        Bpixels = CheckAllWhite(hdc, FALSE, expected);

        // Failure Could be due to running out of memory.
        // Expect screen at 50,50,4,4, (4*4 = 16) should painted as  black.
        CheckNotRESULT(FALSE, Bpixels == expected);
    }
    myReleaseDC(hdc);
    SetSurfVerify(bOldVerify);
}

//***********************************************************************************
void
BogusSrcDCMaskBlt(int testFunc)
{
    info(ECHO, TEXT("%s - Bogus Source DC MaskBlt test"), funcName[testFunc]);
    TDC     hdc       = myGetDC();
    TDC     hdcCompat = NULL;
    TBITMAP hbmp      = NULL,
            hbmpStock = NULL;
    int     result    = 0,
            expected  = 4,
            Bpixels;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hbmp = myCreateBitmap(10, 10, 1, 1, NULL));
        CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));

        // initialize the entire bitmap in 1 call so we don't get false driver verification failures.
        CheckNotRESULT(FALSE, PatBlt(hdcCompat, 0, 0, 10, 10, WHITENESS));
        CheckNotRESULT(FALSE, PatBlt(hdcCompat, 0, 0, 4, 4, BLACKNESS));
        CheckNotRESULT(FALSE, PatBlt(hdcCompat, 0, 0, 2, 2, WHITENESS));

        CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
        CheckNotRESULT(FALSE, DeleteDC(hdcCompat));

        // clear the surface for the CheckAllWhite called after the test.
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

        CheckNotRESULT(FALSE, result = MaskBlt(hdc, 50, 50, 4, 4, (TDC) NULL, 0, 0, hbmp, 0, 0, MAKEROP4(BLACKNESS, WHITENESS)));
        // the mask is 4x4 with a 2x2 block as the foreground, and the rest as background.
        // we're making the forground black, and the background white, thus we expect 4 pixels to be black.

        if (!result)
            info(FAIL, TEXT("%s should not have failed given bogus srcDC"), funcName[testFunc]);

        Bpixels = CheckAllWhite(hdc, FALSE, expected);

        // Failure Could be due to running out of memory.
        // Expect screen at 50,50,4,4, (2*2 = 4) should painted as  black.
        CheckNotRESULT(FALSE, Bpixels == expected);
        CheckNotRESULT(FALSE, DeleteObject(hbmp));
    }

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

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        for (int i = 0; i < 4; i++)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            result = PatBlt(hdc, 50, 50, OddShapes[i][0], OddShapes[i][1], BLACKNESS);
            if (result != OddShapes[i][2])
                info(FAIL, TEXT("PatBlt(50,50,%d,%d,BLACKNESS) returned:%d, expected:%d"), OddShapes[i][0], OddShapes[i][1], result,
                     OddShapes[i][2]);

            Bpixels = CheckAllWhite(hdc, FALSE, OddShapes[i][3]);

            if (Bpixels != OddShapes[i][3])
                info(FAIL, TEXT("PatBlt(50,50,%d,%d,BLACKNESS) painted pixels:%d, expected:%d"), OddShapes[i][0], OddShapes[i][1],
                     Bpixels, OddShapes[i][3]);
        }
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
                hBitmap;
        int     i,
                shift[9][2] = { {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}, {1, 0},
                                {0, 0}
                                };
        RECT    rc;
        int     delta;
        int     x,
                y,
                nRet;

        if (!IsWritableHDC(hdc))
            info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
        else
        {

            CheckNotRESULT(NULL, hBitmap = myLoadBitmap(globalInst, TEXT("PARIS0")));
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));

            // set up region
            CheckNotRESULT(NULL, hRgn = CreateRectRgn(5, 5, zx - 5, zy - 5));

            // set up bitmaps
            CheckNotRESULT(NULL, compDC = CreateCompatibleDC(hdc));
            CheckNotRESULT(NULL, hOrigBitmap = (TBITMAP) SelectObject(compDC, hBitmap));

            delta = 20;
            info(DETAIL, TEXT("-RESIZE(20) * shift[0][0]=%d: PARIS_X=%d PARIS_Y=%d\n"), -RESIZE(20) * shift[0][0], PARIS_X, PARIS_Y);

            for (i = 0; i < 9; i++)
            {
                info(DETAIL, TEXT("Testing shift [%d,%d]"), shift[i][0], shift[i][1]);
                CheckNotRESULT(ERROR, SelectClipRgn(hdc, NULL));
                CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
                switch (testFunc)
                {
                    case EBitBlt:
                        x = (zx / 2 - PARIS_X) / 2 - RESIZE(delta) * shift[i][0];
                        y = (zy - PARIS_Y) / 2 - RESIZE(delta) * shift[i][1];
                        CheckNotRESULT(FALSE, BitBlt(hdc, x, y, PARIS_X, PARIS_Y, compDC, 0, 0, SRCCOPY));

                        x = (zx / 2 - PARIS_X) / 2 + zx / 2;
                        y = (zy - PARIS_Y) / 2;
                        CheckNotRESULT(FALSE, BitBlt(hdc, (zx / 2 - PARIS_X) / 2 + zx / 2, (zy - PARIS_Y) / 2, PARIS_X, PARIS_Y, compDC, 0, 0, SRCCOPY));
                        break;
                    case EStretchBlt:
                        CheckNotRESULT(FALSE, StretchBlt(hdc, (zx / 2 - PARIS_X) / 2 - RESIZE(delta) * shift[i][0],
                                     (zy - PARIS_Y) / 2 - RESIZE(delta) * shift[i][1], PARIS_X, PARIS_Y, compDC, 0, 0, PARIS_X, PARIS_Y,
                                     SRCCOPY));
                        CheckNotRESULT(FALSE, StretchBlt(hdc, (zx / 2 - PARIS_X) / 2 + zx / 2, (zy - PARIS_Y) / 2, PARIS_X, PARIS_Y, compDC, 0, 0, PARIS_X,
                                     PARIS_Y, SRCCOPY));
                        break;
                    case EMaskBlt:
                        CheckNotRESULT(FALSE, MaskBlt(hdc, (zx / 2 - PARIS_X) / 2 - RESIZE(delta) * shift[i][0],
                                  (zy - PARIS_Y) / 2 - RESIZE(delta) * shift[i][1], PARIS_X, PARIS_Y, compDC, 0, 0, (TBITMAP) NULL, 0, 0,
                                  SRCCOPY));
                        CheckNotRESULT(FALSE, MaskBlt(hdc, (zx / 2 - PARIS_X) / 2 + zx / 2, (zy - PARIS_Y) / 2, PARIS_X, PARIS_Y, compDC, 0, 0, (TBITMAP) NULL, 0, 0,
                                  SRCCOPY));
                        break;
                }

                CheckNotRESULT(ERROR, nRet = SelectClipRgn(hdc, hRgn));
                CheckNotRESULT(NULL, GetClipBox(hdc, &rc));

                switch (testFunc)
                {
                    case EBitBlt:
                        x = RESIZE(delta) * shift[i][0];
                        y = RESIZE(delta) * shift[i][1];
                        CheckNotRESULT(FALSE, BitBlt(hdc, x, y, zx / 2, zy, hdc, 0, 0, SRCCOPY));
                        break;
                    case EStretchBlt:
                        CheckNotRESULT(FALSE, StretchBlt(hdc, RESIZE(delta) * shift[i][0], RESIZE(delta) * shift[i][1], zx / 2, zy, hdc, 0, 0, zx / 2, zy,
                                     SRCCOPY));
                        break;
                    case EMaskBlt:
                        CheckNotRESULT(FALSE, MaskBlt(hdc, RESIZE(delta) * shift[i][0], RESIZE(delta) * shift[i][1], zx / 2, zy, hdc, 0, 0, (TBITMAP) NULL, 0, 0,
                                  SRCCOPY));
                        break;
                }
                CheckScreenHalves(hdc);
            }

            CheckNotRESULT(ERROR, SelectClipRgn(hdc, NULL));
            CheckNotRESULT(FALSE, DeleteObject(hRgn));
            CheckNotRESULT(NULL, SelectObject(compDC, hOrigBitmap));
            CheckNotRESULT(FALSE, DeleteDC(compDC));
            CheckNotRESULT(FALSE, DeleteObject(hBitmap));
        }

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
            maxerrors = 20,
            MoveX;
    RECT    rc;

    // verify we are not running windowless
    if (NULL == ghwndTopMain)
        return;

    CheckNotRESULT(FALSE, GetWindowRect(ghwndTopMain, &rc));
    CheckNotRESULT(FALSE, MoveWindow(ghwndTopMain, offsetX, offsetY, zx, zy, FALSE));

    for (int y = 0; y < zy && errors < maxerrors; y += 20)
        for (int x = 0; x < zx && errors < maxerrors; x += 20)
        {
            point0.y = point1.y = y;
            if(!g_bRTL) // normal LTR window?
            {
                point0.x = point1.x = x;
                MoveX = offsetX;
                result0 = ScreenToClient(ghwndTopMain, &point0);
                result1 = ClientToScreen(ghwndTopMain, &point1);
            }
            else
            { // RTL window
                point0.x = point1.x = zx - x; // x coord walk opposite direction
                MoveX = -offsetX;
                result0 = MapWindowPoints(NULL, ghwndTopMain, &point0, 1);
                result1 = MapWindowPoints(ghwndTopMain, NULL, &point1, 1);
            }

            if(!g_bRTL && (!result0 || !result1)) // don't test MapWindowPoints results
            {
                info(FAIL, TEXT("one call failed One:%d Two:%d"), result0, result1);
                errors++;
            }
            if (point0.x != x - MoveX || point0.y != y - offsetY)
            {
                info(FAIL, TEXT("StoC (%d %d) != (%d %d) - (%d %d)"), point0.x, point0.y, x, y, MoveX, offsetY);
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
    CheckNotRESULT(FALSE, MoveWindow(ghwndTopMain, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE));
}

//***********************************************************************************
void
ClientScreenTest(int testFunc)
{

    info(ECHO, TEXT("%s - Client Screen Test"), funcName[testFunc]);

    for (int y = -10; y <= 10; y += 10)
        for (int x = -10; x <= 10; x += 10)
            checkScreen(x, y);

    // As this moves the processes top window, we need to re-initialize changes to the shell
    // for the test suite.
    RehideShell(ghwndTopMain);
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
    HBRUSH stockHBrush;
    HPEN stockHPen;

    r.left = zx / 2 - 50;
    r.top = zy / 2 - 50;
    r.right = zx / 2 + 50;
    r.bottom = zy / 2 + 50;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        info(ECHO, TEXT("%s - SimpleClipRgnTest0:  fixed points: Set ClipRect=[%d %d %d %d]"), funcName[testFunc], r.left, r.top,
             r.right, r.bottom);
        CheckNotRESULT(NULL, hRgn = CreateRectRgnIndirect(&r));

        CheckNotRESULT(NULL, stockHBrush = (HBRUSH) SelectObject(hdc, GetStockObject(NULL_BRUSH)));
        CheckNotRESULT(NULL, stockHPen = (HPEN) SelectObject(hdc, GetStockObject(BLACK_PEN)));

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
        CheckNotRESULT(ERROR, SelectClipRgn(hdc, hRgn));
        CheckNotRESULT(ERROR, GetClipBox(hdc, &r));
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
            switch (testFunc)
            {
                case EPolygon:
                    CheckNotRESULT(FALSE, Polygon(hdc, p, MAX_FIXED_POINTS));
                    CheckNotRESULT(FALSE, Polygon(hdc, p2, MAX_FIXED_POINTS));
                    break;
                case EPolyline:
                    CheckNotRESULT(FALSE, Polyline(hdc, p, MAX_FIXED_POINTS));
                    CheckNotRESULT(FALSE, Polyline(hdc, p2, MAX_FIXED_POINTS));
                    break;
                case EFillRect:
                    r.left -= 1;
                    r.top -= 1;
                    r.right += 1;
                    r.bottom += 1;
                    CheckNotRESULT(FALSE, FillRect(hdc, &r, (HBRUSH) (GetStockObject(BLACK_BRUSH))));
                    break;
            }

            if (isPrinterDC(hdc))
            {
                info(ECHO, TEXT("Skipping checks in SimpleClipRgnTest0 on printer DC"));
            }
            else
            {
                if ((clr = GetPixel(hdc, r.left - 1, r.top)) != CLR_INVALID)
                    info(FAIL, TEXT("iLoop=%d: [%d,%d]=0x%lX: expect outside cliprgn "), i, r.left - 1, r.top, clr);

                if ((clr = GetPixel(hdc, r.left, r.top - 1)) != CLR_INVALID)
                    info(FAIL, TEXT("iLoop=%d: [%d,%d]=0x%lX: expect outside cliprgn "), i, r.left, r.top - 1, clr);

                if ((clr = GetPixel(hdc, r.right, r.bottom - 1)) != CLR_INVALID)
                    info(FAIL, TEXT("iLoop=%d: [%d,%d]=0x%lX: expect outside cliprgn "), i, r.right, r.bottom - 1, clr);

                if ((clr = GetPixel(hdc, r.right + 1, r.bottom)) != CLR_INVALID)
                    info(FAIL, TEXT("iLoop=%d: [%d,%d]=0x%lX: expect outside cliprgn "), i, r.right + 1, r.bottom, clr);

                CheckNotRESULT(FALSE, PatBlt(hdc, r.left, r.top, r.right, r.bottom, WHITENESS));

                if (CheckAllWhite(hdc, FALSE, 0))
                {
                    info(FAIL, TEXT("%s: iLoop=%d: failed"), funcName[testFunc], i);
                }
            }
        }
        CheckNotRESULT(NULL, SelectClipRgn(hdc, NULL));
        CheckNotRESULT(FALSE, DeleteObject(hRgn));
        CheckNotRESULT(NULL, SelectObject(hdc, stockHBrush));
        CheckNotRESULT(NULL, SelectObject(hdc, stockHPen));
    }

    myReleaseDC(hdc);
}

//***********************************************************************************
void
SimplePolyTest(int testFunc)
{
    info(ECHO, TEXT("%s - Simple Poly Test: 500 points"), funcName[testFunc]);
    SetMaxErrorPercentage(.0001);
    TDC     hdc = myGetDC();
    POINT   p[500],
           *point = p;
    HRGN    hRgn;
    int     result;
    HBRUSH stockHBrush;
    HPEN stockHPen;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hRgn = CreateRectRgn(zx / 2 - 50, zy / 2 - 50, zx / 2 + 50, zy / 2 + 50));

        CheckNotRESULT(NULL, stockHPen = (HPEN) SelectObject(hdc, GetStockObject(NULL_BRUSH)));
        CheckNotRESULT(NULL, stockHBrush = (HBRUSH) SelectObject(hdc, GetStockObject(BLACK_PEN)));

        for (int pass = 0; pass < 2; pass++)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            if (pass == 1)
            {
                CheckNotRESULT(ERROR, SelectClipRgn(hdc, hRgn));
                RECT    r;

                CheckNotRESULT(ERROR, GetClipBox(hdc, &r));
                info(DETAIL, TEXT("ClipRect = [%d %d %d %d]"), r.left, r.top, r.right, r.bottom);
            }

            for (int count = 0; count < 500; count += (count < 5) ? 1 : 100)
            {
                CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
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
                    CheckNotRESULT(FALSE, PatBlt(hdc, zx / 2 - 50, zy / 2 - 50, zx / 2 + 50, zy / 2 + 50, WHITENESS));
                    if (0 != (result = CheckAllWhite(hdc, FALSE, 0)))
                        info(DETAIL, TEXT("Pass==1: CheckAllWhite return error pixels=%d: points=%d "), result, count);
                }
            }
        }
        CheckNotRESULT(ERROR, SelectClipRgn(hdc, NULL));
        CheckNotRESULT(FALSE, DeleteObject(hRgn));
        CheckNotRESULT(NULL, SelectObject(hdc, stockHPen));
        CheckNotRESULT(NULL, SelectObject(hdc, stockHBrush));
    }

    myReleaseDC(hdc);
    ResetCompareConstraints();
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
    SetMaxErrorPercentage(.00005);
    TDC     hdc = myGetDC();
    POINT   p[500],
           *point = p;
    HRGN    hRgn;
    int     result;
    RECT    rc;
    HPEN stockHPen;
    HBRUSH stockHBrush;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hRgn = CreateRectRgn(zx / 2 - 50, zy / 2 - 50, zx / 2 + 50, zy / 2 + 50));

        CheckNotRESULT(NULL, stockHBrush = (HBRUSH) SelectObject(hdc, GetStockObject(NULL_BRUSH)));
        CheckNotRESULT(NULL, stockHPen = (HPEN) SelectObject(hdc, GetStockObject(BLACK_PEN)));

        // clear the screen for CheckAllWhite
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

        CheckNotRESULT(ERROR, SelectClipRgn(hdc, hRgn));
        // Check Region:
        CheckNotRESULT(ERROR, GetClipBox(hdc, &rc));

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
                    info(FAIL, TEXT("%s failed %d vertexes - %d"), funcName[testFunc], count, GetLastError());
                else if (result && count <= 1 && !(testFunc == EPolyline && count == 0))
                    info(FAIL, TEXT("%s should have failed %d vertexes"), funcName[testFunc], count);
            }
        }

        CheckNotRESULT(FALSE, PatBlt(hdc, rc.left, rc.top, rc.right, rc.bottom, WHITENESS));
        CheckAllWhite(hdc, FALSE, 0);

        CheckNotRESULT(ERROR, SelectClipRgn(hdc, NULL));
        CheckNotRESULT(FALSE, DeleteObject(hRgn));
        CheckNotRESULT(NULL, SelectObject(hdc, stockHBrush));
        CheckNotRESULT(NULL, SelectObject(hdc, stockHPen));
    }

    myReleaseDC(hdc);
    ResetCompareConstraints();
}

/***********************************************************************************
***
***   Simple Polyline Test
***
************************************************************************************/

void
PolylineTest()                  // this one need visually check the drawing on the screen
{
    info(ECHO, TEXT("PolyLineTest"));

    int     x,
            y,
            iPen;
    TDC     hdc = myGetDC();
    HPEN    hpen;
    POINT   points[2],
           *p = points;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        for (iPen = 1; iPen < 10; iPen += 2)
        {
            // Warning 22019 implies that zx or zy could overflow or underflow.
            // Since we're test code and these are all pretty-well-known window sizes, suppress the warning.
#pragma warning(suppress:22019)
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            CheckNotRESULT(NULL, hpen = CreatePen(PS_SOLID, iPen, RGB(0, 0, 0)));
            CheckNotRESULT(NULL, hpen = (HPEN) SelectObject(hdc, hpen));
            // Check Hotizontal line drawing
            points[0].x = 0;
            points[1].x = zx;
            for (y = 0; y < zy; y += 50)
            {
                points[0].y = points[1].y = y;
                CheckNotRESULT(FALSE, Polyline(hdc, p, 2));
            }

            // Check vertical line drawing
            points[0].y = 0;
            points[1].y = zy;
            for (x = 0; x < zx; x += 50)
            {
                points[0].x = points[1].x = x;
                CheckNotRESULT(FALSE, Polyline(hdc, p, 2));
            }

            // Check diagonal line drawing
            points[0].x = 0;
            points[1].y = 0;
            for (y = 0; y < zy; y += 50)
            {
                points[0].y = y;
                points[1].x = y;
                CheckNotRESULT(FALSE, Polyline(hdc, p, 2));
            }

            points[0].y = zy;
            points[1].x = zx;
            for (y = 0; y < zy; y += 50)
            {
                points[0].x = zx - y;
                points[1].y = zy - y;
                CheckNotRESULT(FALSE, Polyline(hdc, p, 2));
            }

#ifdef VISUAL_CHECK
            MessageBox(NULL, TEXT(""), TEXT("line"), MB_OK | MB_SETFOREGROUND);
#endif
            CheckNotRESULT(NULL, hpen = (HPEN) SelectObject(hdc, hpen));
            CheckNotRESULT(FALSE, DeleteObject(hpen));
        }
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
        CheckNotRESULT(FALSE, Polyline(hdc, p, 2));
    }
}

//***********************************************************************************
void
SimplePolylineTest(int testFunc)
{
    info(ECHO, TEXT("%s - Simple Polyline Test"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HBRUSH  hBrush;
    RECT    aRect;
    HPEN stockHPen;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hBrush = (HBRUSH) GetStockObject(BLACK_BRUSH));

        setTestRectRgn();

        CheckNotRESULT(NULL, stockHPen = (HPEN) SelectObject(hdc, (HPEN) GetStockObject(BLACK_PEN)));

        for (int test = 0; test < NUMREGIONTESTS; test++)
        {
            aRect.left = RESIZE(rTests[test].left);
            aRect.top = RESIZE(rTests[test].top);
            aRect.right = RESIZE(rTests[test].right);
            aRect.bottom = RESIZE(rTests[test].bottom);

            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            CheckNotRESULT(FALSE, FillRect(hdc, &aRect, hBrush));

            //240x320  or 208x240: small deice: aRect sometime is passing the border of zx/2
            // Clean out the other side of the screen
            CheckNotRESULT(FALSE, PatBlt(hdc, zx / 2, 0, zx, zy, WHITENESS));
            FillRectPolyline(hdc, &aRect, zx / 2);
            CheckScreenHalves(hdc);
        }

        CheckNotRESULT(NULL, SelectObject(hdc, stockHPen));
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
    SetMaxErrorPercentage(.005);
    TDC   hdc       = myGetDC();
    HPEN  hpen      = NULL,
          hpenStock = NULL;
    POINT *p0       = NULL,
          *p1       = NULL;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        p0 = new POINT[500];
        p1 = new POINT[500];

        if(p0 && p1)
        {
            for (int nPenWidth = 0; nPenWidth < 10; nPenWidth++)
            {
                CheckNotRESULT(NULL, hpen = CreatePen(PS_SOLID, nPenWidth, randColorRef()));

                // if pen creation fails, log an abort and continue the test with the default pen.
                if(hpen)
                {
                    // pen creation succeeded, select it and test
                    CheckNotRESULT(NULL, hpenStock = (HPEN) SelectObject(hdc, hpen));
                    info(DETAIL, TEXT("Testing with a width of %d"), nPenWidth);
                }
                else info(ABORT, TEXT("Pen Creation failed, using default pen."));

                for (int count = 0; count < 500; count += (count < 5) ? 1 : 100)
                {
                    CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
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

                    // if count is less than 2, polygon/line doesn't have enough points to draw anything.
                    // so the call will fail.
                    CheckForRESULT(count >= 2, Polygon(hdc, p0, count));
                    CheckForRESULT(count >= 1, Polyline(hdc, p1, count + 1));
                    // for pen widths > 1 polygon and polyline don't match exactly at corners.
                    // same behavior on nt
                    if(nPenWidth <= 1)
                        CheckScreenHalves(hdc);
                }

                // if pen creation earlier succeeded, do nothing if it failed, error previously logged.
                if(hpen)
                {
                    CheckNotRESULT(NULL, hpenStock = (HPEN) SelectObject(hdc, hpenStock));
                    // hpenStock should now be the previous pen selected, if it isn't, it's an error.
                    if(hpenStock != hpen)
                        info(FAIL, TEXT("Pen returned did not match pen selected"));

                    CheckNotRESULT(FALSE, DeleteObject(hpen));
                }
            }
        }
        else info(ABORT, TEXT("Failed to allocate memory for point arrays"));

        if(p0)
            delete[] p0;
        if(p1)
            delete[] p1;
    }

    myReleaseDC(hdc);
    // error tolerances must be left on until after myReleaseDC
    ResetCompareConstraints();
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
                CheckNotRESULT(NULL, hBmp = myCreateBitmap(w, h, 1, gBitDepths[c], lpBits));
                if(!hBmp)
                    info(FAIL, TEXT("creation of a bitmap w:%d h:%d d:%d failed."), w, h, gBitDepths[c]);

                CheckNotRESULT(FALSE, GetObject(hBmp, sizeof (BITMAP), &bmp));

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
                CheckNotRESULT(FALSE, DeleteObject(hBmp));
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

    NOPRINTERDCV(NULL);

    int     i;
    TDC     hdc = myGetDC();

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        for (i = 0; i < blockSize; i++)
            blastRect(hdc, RESIZE(blocks[i][0]), RESIZE(blocks[i][1]), RESIZE(blocks[i][2]), RESIZE(blocks[i][3]), blocks[i][4],
                      EPatBlt);

        switch (testFunc)
        {
            case EBitBlt:
                for (i = 0; i < curveSize; i++)
                    CheckNotRESULT(FALSE, BitBlt(hdc, RESIZE(curves[i][0]), RESIZE(curves[i][1]), RESIZE(curves[i][2]), RESIZE(curves[i][3]), hdc,
                             RESIZE(curves[i][4]), RESIZE(curves[i][5]), SRCCOPY));
                break;
            case EStretchBlt:
                for (i = 0; i < curveSize; i++)
                    CheckNotRESULT(FALSE, StretchBlt(hdc, RESIZE(curves[i][0]), RESIZE(curves[i][1]), RESIZE(curves[i][2]), RESIZE(curves[i][3]), hdc,
                                 RESIZE(curves[i][4]), RESIZE(curves[i][5]), RESIZE(curves[i][2]), RESIZE(curves[i][3]), SRCCOPY));
                break;
            case EMaskBlt:
                for (i = 0; i < curveSize; i++)
                    CheckNotRESULT(FALSE, MaskBlt(hdc, RESIZE(curves[i][0]), RESIZE(curves[i][1]), RESIZE(curves[i][2]), RESIZE(curves[i][3]), hdc,
                              RESIZE(curves[i][4]), RESIZE(curves[i][5]), (TBITMAP) NULL, 0, 0, SRCCOPY));
                break;
        }
    }

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

    TDC     hdc    = myGetDC(),
            maskDC = NULL,
            srcDC  = NULL;
    TBITMAP hMask  = NULL,
            stock0 = NULL,
            hSrc   = NULL,
            stock1 = NULL;
    POINT   squares = { zx / 2 / aSquare, zy / aSquare };
    int     x,
            y,
            xx,
            yy,
            wx,
            wy;
    HRGN    hRgn = NULL;
    DWORD   BlitMask = MAKEROP4(0x00AA0000, SRCCOPY);

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, maskDC = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, srcDC = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hMask = myCreateBitmap(zx / 2, zy, 1, 1, NULL));
        CheckNotRESULT(NULL, hSrc = CreateCompatibleBitmap(hdc, zx / 2, zy));

        // set up mask
        CheckNotRESULT(NULL, stock0 = (TBITMAP) SelectObject(maskDC, hMask));
        CheckNotRESULT(FALSE, PatBlt(maskDC, 0, 0, zx / 2, zy, BLACKNESS));

        // set up src
        CheckNotRESULT(NULL, stock1 = (TBITMAP) SelectObject(srcDC, hSrc));
        CheckNotRESULT(FALSE, PatBlt(srcDC, 0, 0, zx / 2, zy, WHITENESS));
        for (x = 0; x < squares.x; x++)
            for (y = 0; y < squares.y; y++)
                if ((x + y) % 2 == 0)
                    CheckNotRESULT(FALSE, PatBlt(srcDC, x * aSquare, y * aSquare, aSquare, aSquare, BLACKNESS));

        for (int n = 0; n < 10; n++)
        {
            x = goodRandomNum(zx / 2);
            y = goodRandomNum(zy);
            wx = goodRandomNum(zx / 2 - x);
            wy = goodRandomNum(zy - y);
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            CheckNotRESULT(FALSE, MaskBlt(hdc, x, y, wx, wy, srcDC, x, y, hMask, x, y, BlitMask));
            CheckNotRESULT(NULL, hRgn = CreateRectRgn(x + zx / 2, y, x + wx + zx / 2, y + wy));
            CheckNotRESULT(ERROR, SelectClipRgn(hdc, hRgn));
            for (xx = 0; xx < squares.x; xx++)
                for (yy = 0; yy < squares.y; yy++)
                    if ((xx + yy) % 2 == 0)
                        CheckNotRESULT(FALSE, PatBlt(hdc, xx * aSquare + zx / 2, yy * aSquare, aSquare, aSquare, BLACKNESS));
            CheckNotRESULT(ERROR, SelectClipRgn(hdc, NULL));
            CheckNotRESULT(FALSE, DeleteObject(hRgn));

            CheckScreenHalves(hdc);
        }

        CheckNotRESULT(NULL, SelectObject(maskDC, stock0));
        CheckNotRESULT(NULL, SelectObject(srcDC, stock1));
        CheckNotRESULT(FALSE, DeleteObject(hMask));
        CheckNotRESULT(FALSE, DeleteObject(hSrc));
        CheckNotRESULT(FALSE, DeleteDC(maskDC));
        CheckNotRESULT(FALSE, DeleteDC(srcDC));
    }

    myReleaseDC(hdc);
}

void
MaskBltBadMaskWidth(int testFunc)
{
    info(ECHO, TEXT("%s - MaskBltBadMaskWidth Test"), funcName[testFunc]);

    NOPRINTERDCV(NULL);

    TDC     hdc = myGetDC(), hdcCompat;
    int nWidth = zx-5, nHeight = zy-5;
    TBITMAP hMask, hbmpStock;
    int result;
    // use a mask that requires a source
    DWORD   BlitMaskSrc = MAKEROP4(SRCCOPY, SRCINVERT);
    DWORD   BlitMaskDst = MAKEROP4(BLACKNESS, WHITENESS);

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hMask = myCreateBitmap(nWidth, nHeight, 1, 1, NULL));

        int nTests[][10] = {
                                // dst top, dst left, width, height, src top, src left, mask top, mask left, ROP, expected result
                                { 0, 0, nWidth, nHeight, 0, 0, -1, 0, BlitMaskSrc, 0},
                                { 0, 0, nWidth, nHeight, 0, 0, 0, -1, BlitMaskSrc, 0},
                                { 0, 0, nWidth - 1, nHeight, 0, 0, -1, 0, BlitMaskSrc, 0},
                                { 0, 0, nWidth, nHeight - 1, 0, 0, 0, -1, BlitMaskSrc, 0},
                                { 0, 0, nWidth, nHeight, 0, 0, -nWidth, 0, BlitMaskSrc, 0},
                                { 0, 0, nWidth, nHeight, 0, 0, 0, -nHeight, BlitMaskSrc, 0},
                                { 0, 0, nWidth + 1, nHeight, 0, 0, 0, 0, BlitMaskSrc, 0},
                                { 0, 0, nWidth, nHeight + 1, 0, 0, 0, 0, BlitMaskSrc, 0},
                                { 0, 0, nWidth, nHeight, 0, 0, nWidth, 0, BlitMaskSrc, 0},
                                { 0, 0, nWidth, nHeight, 0, 0, 0, nHeight, BlitMaskSrc, 0},
                                { 0, 0, nWidth, nHeight, 0, 0, nWidth + 1, 0, BlitMaskSrc, 0},
                                { 0, 0, nWidth, nHeight, 0, 0, 0, nHeight + 1, BlitMaskSrc, 0},
                                { 0, 0, nWidth, nHeight, 0, 0, nWidth - 1, 0, BlitMaskSrc, 0},
                                { 0, 0, nWidth, nHeight, 0, 0, 0, nHeight - 1, BlitMaskSrc, 0},
                                { 0, 0, nWidth, nHeight, 0, 0, 1, 0, BlitMaskSrc, 0},
                                { 0, 0, nWidth, nHeight, 0, 0, 0, 1, BlitMaskSrc, 0},
                                { 0, 0, nWidth, nHeight, 0, 0, 0, 1, BlitMaskDst, 0},
                                { 0, 0, nWidth, nHeight, 0, 0, 1, 0, BlitMaskDst, 0},
                                { 0, 0, nWidth+1, nHeight, 0, 0, 0, 0, BlitMaskDst, 0},
                                { 0, 0, nWidth, nHeight+1, 0, 0, 0, 0, BlitMaskDst, 0}
                                };

        int nCount = sizeof(nTests) / (sizeof(int) * 10);
        int i;

        if (!hMask)
        {
            info(FAIL, TEXT("Important bitmap failed - can go no further"));
            return;
        }

        CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hMask));

        CheckNotRESULT(FALSE, PatBlt(hdcCompat, 0, 0, nWidth, nHeight, WHITENESS));
        DrawDiagonals(hdcCompat, nWidth, nHeight);
        CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));

        for(i=0; i<nCount; i++)
        {
            result = MaskBlt(hdc, nTests[i][0], nTests[i][1], nTests[i][2], nTests[i][3], hdc, nTests[i][4], nTests[i][5], hMask, nTests[i][6], nTests[i][7], nTests[i][8]);
            if(result != nTests[i][9])
                info(FAIL, TEXT("Test %d - MaskBlt(hdc, %d, %d, %d, %d, hdc, %d, %d, hMask, %d, %d, %d) returned %d, expected %d"),
                                        i, nTests[i][0], nTests[i][1], nTests[i][2], nTests[i][3], nTests[i][4], nTests[i][5], nTests[i][6], nTests[i][7], nTests[i][8], result, nTests[i][9]);
        }

        CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
        CheckNotRESULT(FALSE, DeleteObject(hMask));
    }

    myReleaseDC(hdc);
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
    HBRUSH  hBrush;
    RECT    r = { zx, zy, -1, -1 };
    int     result;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hBrush = (HBRUSH) GetStockObject(WHITE_BRUSH));

        info(ECHO, TEXT("%s - Using negative coords [%d %d -1 -1]"), funcName[testFunc], zx, zy);

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
        CheckNotRESULT(FALSE, FillRect(hdc, &r, hBrush));

        result = CheckAllWhite(hdc, FALSE, 0);
        if (result != 0)
            info(FAIL, TEXT("FillRect failed draw with negative coords"));
    }

    myReleaseDC(hdc);
}

//***********************************************************************************
void
SimpleFillRect(int testFunc)
{
    info(ECHO, TEXT("%s - SimpleFillRect"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HBRUSH  hBrush;
    RECT    r = { 10, 10, 20, 0 };  // top > bottom

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hBrush = (HBRUSH) GetStockObject(BLACK_BRUSH));

        CheckNotRESULT(FALSE, PatBlt(hdc, 10 + zx / 2, 0, 10, 10, BLACKNESS));
        CheckNotRESULT(FALSE, FillRect(hdc, &r, hBrush));
        CheckScreenHalves(hdc);
    }

    myReleaseDC(hdc);
}

//***********************************************************************************
// verify that a NULL brush draws nothing with FillRect.
void
FillRectNullBrushTest(int testFunc)
{
    info(ECHO, TEXT("%s - FillRectNullBrushTest"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HBRUSH  hBrush;
    RECT    r = { 10, 10, 20, 20 };

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hBrush = (HBRUSH) GetStockObject(NULL_BRUSH));

        CheckNotRESULT(FALSE, FillRect(hdc, &r, hBrush));

        // the left and right halves should match, and both sides should be empty.
        CheckScreenHalves(hdc);
    }

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

    CheckNotRESULT(NULL, hBmp = myCreateBitmap(0, 0, 1, 2, NULL));
    passCheck(hBmp != NULL, 1, TEXT("hBmp NULL"));
    CheckNotRESULT(FALSE, DeleteObject(hBmp));
}

//***********************************************************************************
void
SimpleCreateBitmap2(int testFunc)
{

    info(ECHO, TEXT("%s - CreateBitmap: check stock bmp"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
              compDC;
    TBITMAP hBmp,
            hBmpStock;

    CheckNotRESULT(NULL, compDC = CreateCompatibleDC(hdc));

    CheckNotRESULT(NULL, hBmpStock = (TBITMAP) GetCurrentObject(compDC, OBJ_BITMAP));

    CheckNotRESULT(NULL, hBmp = myCreateBitmap(0, 10, 1, 2, NULL));
    CheckNotRESULT(FALSE, hBmp == hBmpStock);
    CheckNotRESULT(FALSE, DeleteObject(hBmp));

    CheckNotRESULT(NULL, hBmp = myCreateBitmap(10, 0, 1, 2, NULL));
    CheckNotRESULT(FALSE, hBmp == hBmpStock);
    CheckNotRESULT(FALSE, DeleteObject(hBmp));

    CheckNotRESULT(FALSE, DeleteDC(compDC));
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
               compDC;
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

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, compDC = CreateCompatibleDC(hdc));

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
        if(!g_bRTL)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x, mid.y, 8, 8, WHITENESS));
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x + 8, mid.y + 8, 8, 8, WHITENESS));
        }
        else
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x + 8, mid.y, 8, 8, WHITENESS));
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x, mid.y + 8, 8, 8, WHITENESS));
        }

        if(testFunc == ESetBitmapBits)
        {
            CheckNotRESULT(NULL, hBmp = myCreateBitmap(16, 16, 1, 1, NULL));
            CheckNotRESULT(0, SetBitmapBits(hBmp, sizeof(wSquare), (LPVOID) wSquare));
        }
        else
        {
            CheckNotRESULT(NULL, hBmp = myCreateBitmap(16, 16, 1, 1, (LPVOID)wSquare));
        }

        if(hBmp != NULL)
        {
            CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(compDC, hBmp));
            CheckNotRESULT(FALSE, BitBlt(hdc, mid.x, mid.y, 16, 16, compDC, 0, 0, SRCCOPY));

            CheckScreenHalves(hdc);

            CheckNotRESULT(NULL, SelectObject(compDC, stockBmp));
            CheckNotRESULT(FALSE, DeleteObject(hBmp));
        }

        CheckNotRESULT(FALSE, DeleteDC(compDC));
    }

    myReleaseDC(hdc);
}

//***********************************************************************************
void
CreateBitmapSquares2bpp(int testFunc)
{
#ifdef UNDER_CE
    info(ECHO, TEXT("%s - CreateBitmap Squares2bpp"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
               compDC;
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

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, compDC = CreateCompatibleDC(hdc));

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
        if(!g_bRTL)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x, mid.y, 8, 8, WHITENESS));
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x + 8, mid.y + 8, 8, 8, WHITENESS));
        }
        else
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x + 8, mid.y, 8, 8, WHITENESS));
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x, mid.y + 8, 8, 8, WHITENESS));
        }

        if(testFunc == ESetBitmapBits)
        {
            CheckNotRESULT(NULL, hBmp = myCreateBitmap(16, 16, 1, 2, NULL));
            CheckNotRESULT(0, SetBitmapBits(hBmp, sizeof(wSquare), (LPVOID) wSquare));
        }
        else
        {
            CheckNotRESULT(NULL, hBmp = myCreateBitmap(16, 16, 1, 2, (LPVOID)wSquare));
        }

        if(hBmp != NULL)
        {
            CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(compDC, hBmp));
            CheckNotRESULT(FALSE, BitBlt(hdc, mid.x, mid.y, 16, 16, compDC, 0, 0, SRCCOPY));

            CheckScreenHalves(hdc);

            CheckNotRESULT(NULL, SelectObject(compDC, stockBmp));
            CheckNotRESULT(FALSE, DeleteObject(hBmp));
        }

        CheckNotRESULT(FALSE, DeleteDC(compDC));
    }

    myReleaseDC(hdc);
#endif
}

//***********************************************************************************
void
CreateBitmapSquares4bpp(int testFunc)
{
    info(ECHO, TEXT("%s - CreateBitmap Squares4bpp"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
            compDC;
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

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, compDC = CreateCompatibleDC(hdc));
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
        if(!g_bRTL)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x, mid.y, 8, 8, WHITENESS));
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x + 8, mid.y + 8, 8, 8, WHITENESS));
        }
        else
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x + 8, mid.y, 8, 8, WHITENESS));
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x, mid.y + 8, 8, 8, WHITENESS));
        }

        if(testFunc == ESetBitmapBits)
        {
            CheckNotRESULT(NULL, hBmp = myCreateBitmap(16, 16, 1, 4, NULL));
            CheckNotRESULT(0, SetBitmapBits(hBmp, sizeof(wSquare), (LPVOID) wSquare));
        }
        else
        {
            CheckNotRESULT(NULL, hBmp = myCreateBitmap(16, 16, 1, 4, (LPVOID)wSquare));
        }

        if(hBmp != NULL)
        {
            CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(compDC, hBmp));
            CheckNotRESULT(FALSE, BitBlt(hdc, mid.x, mid.y, 16, 16, compDC, 0, 0, SRCCOPY));

            CheckScreenHalves(hdc);

            CheckNotRESULT(NULL, SelectObject(compDC, stockBmp));
            CheckNotRESULT(FALSE, DeleteObject(hBmp));
        }

        CheckNotRESULT(FALSE, DeleteDC(compDC));
    }

    myReleaseDC(hdc);
}

//***********************************************************************************
void
CreateBitmapSquares8bpp(int testFunc)
{
    info(ECHO, TEXT("%s - CreateBitmap Squares8bpp"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
            compDC;
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

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, compDC = CreateCompatibleDC(hdc));

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
        if(!g_bRTL)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x, mid.y, 8, 8, WHITENESS));
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x + 8, mid.y + 8, 8, 8, WHITENESS));
        }
        else
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x + 8, mid.y, 8, 8, WHITENESS));
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x, mid.y + 8, 8, 8, WHITENESS));
        }

        if(testFunc == ESetBitmapBits)
        {
            CheckNotRESULT(NULL, hBmp = myCreateBitmap(16, 16, 1, 8, NULL));
            CheckNotRESULT(0, SetBitmapBits(hBmp, sizeof(wSquare), (LPVOID) wSquare));
        }
        else
        {
            CheckNotRESULT(NULL, hBmp = myCreateBitmap(16, 16, 1, 8, (LPVOID)wSquare));
        }

        if(hBmp != NULL)
        {
            CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(compDC, hBmp));
            CheckNotRESULT(FALSE, BitBlt(hdc, mid.x, mid.y, 16, 16, compDC, 0, 0, SRCCOPY));

            CheckScreenHalves(hdc);

            CheckNotRESULT(NULL, SelectObject(compDC, stockBmp));
            CheckNotRESULT(FALSE, DeleteObject(hBmp));
        }

        CheckNotRESULT(FALSE, DeleteDC(compDC));
    }

    myReleaseDC(hdc);
}

//***********************************************************************************
void
CreateBitmapSquares16bpp(int testFunc)
{
    info(ECHO, TEXT("%s - CreateBitmap Squares16bpp"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
            compDC;
    POINT   mid = { zx / 4, zy / 2 };
    TBITMAP hBmp,
            stockBmp;
    // 8 pixels on, 8 pixels off, 16 pixels high for an 8x8 checkerboard
    WORD    wSquare[16][16] = {
        {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
        {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
        {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
        {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
        {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
        {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
        {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
        {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
        {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF},
        {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF},
        {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF},
        {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF},
        {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF},
        {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF},
        {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF},
        {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}
    };

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, compDC = CreateCompatibleDC(hdc));

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
        if(!g_bRTL)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x, mid.y, 8, 8, WHITENESS));
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x + 8, mid.y + 8, 8, 8, WHITENESS));
        }
        else
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x + 8, mid.y, 8, 8, WHITENESS));
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x, mid.y + 8, 8, 8, WHITENESS));
        }

        if(testFunc == ESetBitmapBits)
        {
            CheckNotRESULT(NULL, hBmp = myCreateBitmap(16, 16, 1, 16, NULL));
            CheckNotRESULT(0, SetBitmapBits(hBmp, sizeof(wSquare), (LPVOID) wSquare));
        }
        else
        {
            CheckNotRESULT(NULL, hBmp = myCreateBitmap(16, 16, 1, 16, (LPVOID)wSquare));
        }

        if(hBmp != NULL)
        {
            CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(compDC, hBmp));
            CheckNotRESULT(FALSE, BitBlt(hdc, mid.x, mid.y, 16, 16, compDC, 0, 0, SRCCOPY));

            // RGB555 set to 0xFFFF mismatches the foreground/background comparison, resulting
            // in a mismatch.  this test isn't valid if the myGetDC() bitmap is 1bpp because it's not possible to
            // determine the bit mask used for the bitmap from CreateBitmap.  If the primary bit depth
            // is 32bpp it's RGB555, if it's 16bpp it matches the primary.
            if(myGetBitDepth() != 1)
                CheckScreenHalves(hdc);

            CheckNotRESULT(NULL, SelectObject(compDC, stockBmp));
            CheckNotRESULT(FALSE, DeleteObject(hBmp));
        }

        CheckNotRESULT(FALSE, DeleteDC(compDC));
    }

    myReleaseDC(hdc);
}

//***********************************************************************************
void
CreateBitmapSquares24bpp(int testFunc)
{
    info(ECHO, TEXT("%s - CreateBitmap Squares 24bpp"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
            compDC;
    POINT   mid = { zx / 4, zy / 2 };
    TBITMAP hBmp,
            stockBmp;
    // 8 pixels on, 8 pixels off, 16 pixels high for an 8x8 checkerboard
    DWORD    wSquare[16][12] = {
        {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
          0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
        {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
          0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
        {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
          0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
        {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
          0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
        {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
          0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
        {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
          0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
        {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
          0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
        {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
          0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
          0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
          0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
          0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
          0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
          0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
          0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
          0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
          0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
    };

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, compDC = CreateCompatibleDC(hdc));

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
        if(!g_bRTL)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x, mid.y, 8, 8, WHITENESS));
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x + 8, mid.y + 8, 8, 8, WHITENESS));
        }
        else
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x + 8, mid.y, 8, 8, WHITENESS));
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x, mid.y + 8, 8, 8, WHITENESS));
        }

        if(testFunc == ESetBitmapBits)
        {
            CheckNotRESULT(NULL, hBmp = myCreateBitmap(16, 16, 1, 24, NULL));
            CheckNotRESULT(0, SetBitmapBits(hBmp, sizeof(wSquare), (LPVOID) wSquare));
        }
        else
        {
            CheckNotRESULT(NULL, hBmp = myCreateBitmap(16, 16, 1, 24, (LPVOID)wSquare));
        }

        if(hBmp != NULL)
        {
            CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(compDC, hBmp));
            CheckNotRESULT(FALSE, BitBlt(hdc, mid.x, mid.y, 16, 16, compDC, 0, 0, SRCCOPY));

            CheckScreenHalves(hdc);

            CheckNotRESULT(NULL, SelectObject(compDC, stockBmp));
            CheckNotRESULT(FALSE, DeleteObject(hBmp));
        }

        CheckNotRESULT(FALSE, DeleteDC(compDC));
    }

    myReleaseDC(hdc);
}


//***********************************************************************************
void
CreateBitmapSquares32bpp(int testFunc)
{
    info(ECHO, TEXT("%s - CreateBitmap Squares 32bpp"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
            compDC;
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

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, compDC = CreateCompatibleDC(hdc));

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
        if(!g_bRTL)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x, mid.y, 8, 8, WHITENESS));
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x + 8, mid.y + 8, 8, 8, WHITENESS));
        }
        else
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x + 8, mid.y, 8, 8, WHITENESS));
            CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x, mid.y + 8, 8, 8, WHITENESS));
        }

        if(testFunc == ESetBitmapBits)
        {
            CheckNotRESULT(NULL, hBmp = myCreateBitmap(16, 16, 1, 32, NULL));
            CheckNotRESULT(0, SetBitmapBits(hBmp, sizeof(wSquare), (LPVOID) wSquare));
        }
        else
        {
            CheckNotRESULT(NULL, hBmp = myCreateBitmap(16, 16, 1, 32, (LPVOID)wSquare));
        }

        if(hBmp != NULL)
        {
            CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(compDC, hBmp));
            CheckNotRESULT(FALSE, BitBlt(hdc, mid.x, mid.y, 16, 16, compDC, 0, 0, SRCCOPY));

            CheckScreenHalves(hdc);

            CheckNotRESULT(NULL, SelectObject(compDC, stockBmp));
            CheckNotRESULT(FALSE, DeleteObject(hBmp));
        }

        CheckNotRESULT(FALSE, DeleteDC(compDC));
    }

    myReleaseDC(hdc);
}

//***********************************************************************************
void
SetBitmapBitsOnePixel(int testFunc,  int nBitDepth, int nSurfType)
{
    if(nSurfType == ECreateCompatibleBitmap)
        nBitDepth = myGetBitDepth();

    info(ECHO, TEXT("%s - SetBitmapBitsOnePixel %dbpp - %s"), funcName[testFunc], nBitDepth, funcName[nSurfType]);

    TDC     hdc      = myGetDC(),
            compDC   = NULL;
    POINT   mid      = { zx / 4, zy / 2 };
    TBITMAP hBmp     = NULL,
            stockBmp = NULL;
    int nBitmapSize  = 16;
    int nNumberOfPixelsX = 1;
    int nNumberOfPixelsY = 1;
    // 8 pixels on, 8 pixels off, 16 pixels high for an 8x8 checkerboard
    DWORD    wSquare = 0xFFFFFFFF;
    int nBytes;
    BYTE *pbBits;

    if(nBitDepth == 32)
        wSquare = 0x00FFFFFF;

    if(nBitDepth >= 8)
        nBytes = nBitDepth / 8;
    else
    {
        nBytes = 1;
        nNumberOfPixelsX = 8 / nBitDepth;
    }

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, compDC = CreateCompatibleDC(hdc));

        // background to black
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
        // right half to white
        CheckNotRESULT(FALSE, PatBlt(hdc, zx/2 + mid.x, mid.y, nNumberOfPixelsX, nNumberOfPixelsY, WHITENESS));

        // create the bitmap
        if(nSurfType == ECreateBitmap)
        {
            CheckNotRESULT(NULL, hBmp = myCreateBitmap(nBitmapSize, nBitmapSize, 1, nBitDepth, NULL));
        }
        else if(nSurfType == ECreateDIBSection)
            hBmp = myCreateDIBSection(hdc, (VOID **) &pbBits, nBitDepth, nBitmapSize, nBitmapSize, DIB_RGB_COLORS, NULL, FALSE);
        else if(nSurfType == ECreateCompatibleBitmap)
            CheckNotRESULT(NULL, hBmp = CreateCompatibleBitmap(hdc, nBitmapSize, nBitmapSize));

        // clear out the whole bitmap
        CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(compDC, hBmp));
        CheckNotRESULT(FALSE, PatBlt(compDC, 0, 0, nBitmapSize, nBitmapSize, BLACKNESS));

        // set the pixels in the bitmap, using the number of bytes calculated above.
        CheckNotRESULT(0, SetBitmapBits(hBmp, nBytes, (LPVOID) &wSquare));

        // copy them to the primary
        if(!g_bRTL)
            CheckNotRESULT(FALSE, BitBlt(hdc, mid.x, mid.y, nBitmapSize, nBitmapSize, compDC, 0, 0, SRCCOPY));
        else
            CheckNotRESULT(FALSE, BitBlt(hdc, mid.x-nBitmapSize+nNumberOfPixelsX, mid.y, nBitmapSize, nBitmapSize, compDC, 0, 0, SRCCOPY));

        // RGB555 set to 0xFFFF mismatches the foreground/background comparison, resulting
        // in a mismatch.  this test isn't valid if the myGetDC() bitmap is 1bpp because it's not possible to
        // determine the bit mask used for the bitmap from CreateBitmap.  If the primary bit depth
        // is 32bpp it's RGB555, if it's 16bpp it matches the primary.
        if(!(myGetBitDepth() == 1 && nBitDepth == 16))
            CheckScreenHalves(hdc);

        // cleanup
        CheckNotRESULT(NULL, SelectObject(compDC, stockBmp));
        CheckNotRESULT(FALSE, DeleteObject(hBmp));
        CheckNotRESULT(FALSE, DeleteDC(compDC));
    }

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
    HBRUSH stockHBrush;
    DWORD   code[5] = {
        PATCOPY,
        PATINVERT,
        DSTINVERT,
        BLACKNESS,
        WHITENESS
    };

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        info(DETAIL, TEXT("case 1"));

        for (int i = 0; i < 5; i++)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, 400, 400, 200, 200, code[i]));
        }

        info(DETAIL, TEXT("case 2"));

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
        CheckNotRESULT(FALSE, PatBlt(hdc, zx / 2, 0, 100, 100, BLACKNESS));

        CheckNotRESULT(NULL, stockHBrush = (HBRUSH) SelectObject(hdc, GetStockObject(BLACK_BRUSH)));
        CheckNotRESULT(FALSE, PatBlt(hdc, 100, 100, -100, -100, PATCOPY));

        CheckScreenHalves(hdc);

        CheckNotRESULT(NULL, SelectObject(hdc, stockHBrush));
    }

    myReleaseDC(hdc);
}

//***********************************************************************************
void
PatBltSimple2(int testFunc)
{
    info(ECHO, TEXT("%s - PATINVERT"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    int     result;
    HBRUSH stockHBrush;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // clear the screen for CheckAllWhite
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
        CheckNotRESULT(NULL, stockHBrush = (HBRUSH) SelectObject(hdc, GetStockObject(NULL_BRUSH)));
        CheckNotRESULT(FALSE, PatBlt(hdc, 50, 50, 100, 100, PATINVERT));
        CheckNotRESULT(NULL, SelectObject(hdc, GetStockObject(HOLLOW_BRUSH)));
        CheckNotRESULT(FALSE, PatBlt(hdc, 100, 100, 200, 200, PATINVERT));

        result = CheckAllWhite(hdc, FALSE, 0);
        if (result != 0)
            info(FAIL, TEXT("PatBlt drew something with a NULL_BRUSH"));

        CheckNotRESULT(NULL, SelectObject(hdc, stockHBrush));
    }

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

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // set the pixel
        CheckForRESULT(CLR_INVALID, SetPixel(hdc, -100, -100, RGB(0, 0, 0)));
        CheckForRESULT(CLR_INVALID, GetPixel(hdc, -100, -100));
    }

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

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
        CheckNotRESULT(FALSE, BitBlt(hdc, 50, 50, 20, 20, hdc, -10, -10, SRCCOPY));

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
            compDC;
    TBITMAP hBmp,
            stockBmp;

    struct
    {
        BITMAPINFOHEADER bmih;
        RGBQUAD ct[4];
    }
    bmi;

    CheckNotRESULT(NULL, compDC = CreateCompatibleDC(hdc));

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

    CheckNotRESULT(NULL, hBmp = CreateDIBSection(hdc, (LPBITMAPINFO) & bmi, DIB_RGB_COLORS, (VOID **) & lpBits, NULL, NULL));

    CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(compDC, hBmp));

    if (pass)
        CheckNotRESULT(ERROR, ExcludeClipRect(compDC, 0, 0, 9, 9));
    else
        CheckNotRESULT(ERROR, IntersectClipRect(compDC, 0, 0, 9, 9));

    CheckNotRESULT(NULL, SelectObject(compDC, stockBmp));
    CheckNotRESULT(FALSE, DeleteObject(hBmp));
    CheckNotRESULT(FALSE, DeleteDC(compDC));
    myReleaseDC(hdc);
}

void
SimpleRoundRectTest(int testFunc)
{
    info(ECHO, TEXT("%s - RoundRect: dash pen"), funcName[testFunc]);
    SetMaxErrorPercentage(.00005);
    TDC     hdc = myGetDC();
    HPEN    hPenDash,
            hPenTemp;
    LOGPEN  logDash;
    int     result;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // Clear out the surface for the test.
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

        logDash.lopnStyle = PS_DASH;
        logDash.lopnWidth.x = 1;
        logDash.lopnWidth.y = 1;
        logDash.lopnColor = RGB(0, 0, 0);

        CheckNotRESULT(NULL, hPenDash = CreatePenIndirect(&logDash));

        CheckNotRESULT(NULL, hPenTemp = (HPEN) SelectObject(hdc, hPenDash));
        CheckNotRESULT(FALSE, RoundRect(hdc, 50, 50, -150, 150, 90, 90));

        // CheckAllWhite will return the dwExpected value if the DC
        // is not readable (e.g. if it's a printer DC). Call CheckAllWhite
        // with dwExpected=1 to account for this case (note that 
        // CheckAllWhite will print a FAIL message if it hits unexpected
        // issues).
        result = CheckAllWhite(hdc, TRUE, 1);
        if (!result)
            info(FAIL, TEXT("RoundRect(PS_DASH,1,0) was not drawn"));

        CheckNotRESULT(NULL, hPenTemp = (HPEN) SelectObject(hdc, hPenTemp));

        passCheck(hPenTemp == hPenDash, TRUE, TEXT("Incorrect pen returned"));

        CheckNotRESULT(FALSE, DeleteObject(hPenDash));
    }

    myReleaseDC(hdc);
    ResetCompareConstraints();
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
        nWidth = -((GenRand() % 100) + 1);
        nHeight = ((GenRand() % 100) +1);
        CheckForRESULT(NULL, hBmp = myCreateBitmap(nWidth , nHeight, 1, gBitDepths[nDepth], NULL));
        if (hBmp)
        {
            info(FAIL, TEXT("hBmp should be NULL for width %d, height %d, and bitdepth %d"), nWidth, nHeight, gBitDepths[nDepth]);
            CheckNotRESULT(FALSE, DeleteObject(hBmp));
        }
    }

    for(nDepth=0; nDepth < countof(gBitDepths); nDepth++)
    {
        // pick a random width, between -1 and -100
        nWidth = ((GenRand() % 100) + 1);
        nHeight = -((GenRand() % 100) +1);
        CheckForRESULT(NULL, hBmp = myCreateBitmap(nWidth , nHeight, 1, gBitDepths[nDepth], NULL));
        if (hBmp)
        {
            info(FAIL, TEXT("hBmp should be NULL for width %d, height %d, and bitdepth %d"), nWidth, nHeight, gBitDepths[nDepth]);
            CheckNotRESULT(FALSE, DeleteObject(hBmp));
        }
    }

    for(nDepth=0; nDepth < countof(gBitDepths); nDepth++)
    {
        // pick a random width, between -1 and -100
        nWidth = -((GenRand() % 100) + 1);
        nHeight = -((GenRand() % 100) +1);
        CheckForRESULT(NULL, hBmp = myCreateBitmap(nWidth , nHeight, 1, gBitDepths[nDepth], NULL));
        if (hBmp)
        {
            info(FAIL, TEXT("hBmp should be NULL for width %d, height %d, and bitdepth %d"), nWidth, nHeight, gBitDepths[nDepth]);
            CheckNotRESULT(FALSE, DeleteObject(hBmp));
        }
    }
}

void
CreateBitmapBitsTest(int testFunc)
{

    info(ECHO, TEXT("%s - Bitmap Bits test"), funcName[testFunc]);
    TDC     hdc = myGetDC(),
            memDC;
    TBITMAP hBmp,
            hBmpRet;
    BITMAP  bmp;
    UINT    uBmSize;
    WORD    wSquare[16][2] = {
        {0xFF, 0x00}, {0xFF, 0x00}, {0xFF, 0x00}, {0xFF, 0x00}, {0xFF, 0x00},
        {0xFF, 0x00}, {0xFF, 0x00}, {0xFF, 0x00},
        {0x00, 0xFF}, {0x00, 0xFF}, {0x00, 0xFF}, {0x00, 0xFF}, {0x00, 0xFF},
        {0x00, 0xFF}, {0x00, 0xFF}, {0x00, 0xFF}
    };

    CheckNotRESULT(NULL, hBmp = myCreateBitmap(16, 16, 1, 1, (LPVOID)wSquare));
    CheckNotRESULT(NULL, memDC = CreateCompatibleDC(hdc));
    CheckNotRESULT(NULL, SelectObject(memDC, hBmp));
    CheckNotRESULT(NULL, hBmpRet = (TBITMAP) GetCurrentObject(memDC, OBJ_BITMAP));
    CheckNotRESULT(NULL, GetObject(hBmpRet, sizeof (BITMAP), &bmp));
    uBmSize = bmp.bmWidthBytes * bmp.bmHeight * bmp.bmPlanes;
    CheckNotRESULT(FALSE, DeleteDC(memDC));
    CheckNotRESULT(FALSE, DeleteObject(hBmp));
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
        CheckNotRESULT(NULL, hBmp = myCreateBitmap(0, 100, 1, gBitDepths[nDepth], NULL));

        if(!hBmp)
            info(FAIL, TEXT("CreateBitmap w:0 h:100 p:1 d:%d b:NULL failed."), gBitDepths[nDepth]);

        memset(&bmp, 0xBAD, sizeof(bmp));
        CheckNotRESULT(NULL, GetObject(hBmp, sizeof (bmp), &bmp));

        if (bmp.bmType != 0 || bmp.bmWidth != 1 || bmp.bmHeight != 1 || bmp.bmPlanes != 1 || bmp.bmBitsPixel != 1)
            info(FAIL, TEXT("expected: bmType:0 bmWidth:1 bmHeight:1 bmPlanes:1 bmBitsPixel:1  returned:%d %d %d %d %d"), bmp.bmType, bmp.bmWidth,
                 bmp.bmHeight, bmp.bmPlanes, bmp.bmBitsPixel);

        CheckNotRESULT(FALSE, DeleteObject(hBmp));

        CheckNotRESULT(NULL, hBmp = myCreateBitmap(100, 0, 1, gBitDepths[nDepth], NULL));

        memset(&bmp, 0xBAD, sizeof(bmp));
        CheckNotRESULT(NULL, GetObject(hBmp, sizeof (bmp), &bmp));

        if (bmp.bmType != 0 || bmp.bmWidth != 1 || bmp.bmHeight != 1 || bmp.bmPlanes != 1 || bmp.bmBitsPixel != 1)
            info(FAIL, TEXT("expected: bmType:0 bmWidth:1 bmHeight:1 bmPlanes:1 bmBitsPixel:1  returned:%d %d %d %d %d"), bmp.bmType, bmp.bmWidth,
                 bmp.bmHeight, bmp.bmPlanes, bmp.bmBitsPixel);

        CheckNotRESULT(FALSE, DeleteObject(hBmp));
    }
    myReleaseDC(hdc);
}

//***********************************************************************************
void
BitBltTest2(int testFunc)
{
    info(ECHO, TEXT("%s - Complex test"), funcName[testFunc]);

    TDC     hdc      = myGetDC(),
            compDC   = NULL;
    TBITMAP hBmp     = NULL,
            stockBmp = NULL;
    RECT    srcRect  = { (zx / 2 - UPSIZE) / 2, (zy - UPSIZE) / 2,
                         (zx / 2 - UPSIZE) / 2 + UPSIZE, (zy - UPSIZE) / 2 + UPSIZE
                       };

    int     tA[4][6] = {
        {zx / 2, 0, zx / 2, zy, 0, 0},
        {zx, 0, -zx / 2, zy, zx / 2, 0},
        {zx / 2, zy, zx / 2, -zy, 0, zy},
        {zx, zy, -zx / 2, -zy, zx / 2, zy},
    };

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hBmp = myLoadBitmap(globalInst, TEXT("UP")));
        CheckNotRESULT(NULL, compDC = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(compDC, hBmp));

        for (int i = 0; i < 4; i++)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            CheckNotRESULT(FALSE, BitBlt(hdc, srcRect.left, srcRect.top, UPSIZE, UPSIZE, compDC, 0, 0, SRCCOPY));
            CheckNotRESULT(FALSE, BitBlt(hdc, tA[i][0], tA[i][1], tA[i][2], tA[i][3], hdc, tA[i][4], tA[i][5], SRCCOPY));
            CheckScreenHalves(hdc);
        }

        CheckNotRESULT(NULL, SelectObject(compDC, stockBmp));
        CheckNotRESULT(FALSE, DeleteObject(hBmp));
        CheckNotRESULT(FALSE, DeleteDC(compDC));
    }

    myReleaseDC(hdc);
}

void
SimpleEllipseTest(void)
{
    info(ECHO, TEXT("Ellipse - SimpleEllipseTest()"));
    SetMaxErrorPercentage(.00005);
    TDC     hdc = myGetDC();
    HBRUSH stockHBrush;
    HPEN stockHPen;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, stockHBrush = (HBRUSH) SelectObject(hdc, GetStockObject(BLACK_BRUSH)));
        CheckNotRESULT(NULL, stockHPen = (HPEN) SelectObject(hdc, GetStockObject(BLACK_PEN)));

        // Case1: (30, 50, 30, 100)
        CheckNotRESULT(FALSE, Ellipse(hdc, 30, 50, 30, 100));
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

        // Case2: (30, 50, 80, 100)
        CheckNotRESULT(FALSE, Ellipse(hdc, 30, 50, 80, 100));
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

#if MAXTEST
        // This is an interesting test, but it just takes a while to do
        // an ellipse so large, so don't run regularlly.
        info(ECHO, TEXT("Case3: (0x4,0x8000004,0x8,0x8000008)"));
        DWORD   dw = GetTickCount();

        CheckNotRESULT(FALSE, Ellipse(hdc, 0x4, 0x8000004, 0x8, 0x8000008));

        dw = GetTickCount() - dw;
        info(ECHO, TEXT("Case3: (0x4,0x8000004,0x8,0x8000008):  used time = %u seconds"), dw / 1000);
#endif

        CheckNotRESULT(NULL, SelectObject(hdc, stockHBrush));
        CheckNotRESULT(NULL, SelectObject(hdc, stockHPen));
    }

    myReleaseDC(hdc);
    ResetCompareConstraints();
}

void
StressShapeTest(int testFunc)
{
    info(ECHO, TEXT("StressShapeTest"), funcName[testFunc]);

    // disable surface verification
    BOOL bOldVerify = SetSurfVerify(FALSE);

    SetMaxErrorPercentage((testFunc == EEllipse || testFunc == ERoundRect)? .005 : 0);
    TDC     hdc = myGetDC();
    HBRUSH hbr;
    HPEN hpn;
    int left, top, right, bottom, width, height;
    int result = 0;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        for(int i=0; i< 30; i++)
        {
            if(!(i%10))
            {
                CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            }
            CheckNotRESULT(NULL, hpn = CreatePen(PS_SOLID, 1, randColorRef()));
            CheckNotRESULT(NULL, hbr= CreateSolidBrush(randColorRef()));

            CheckNotRESULT(0, SetROP2(hdc, gnvRop2Array[GenRand() % countof(gnvRop2Array)].dwVal));

            // delete the previous objects and select in the new one
            CheckNotRESULT(FALSE, DeleteObject(SelectObject(hdc, hbr)));
            CheckNotRESULT(FALSE, DeleteObject(SelectObject(hdc, hpn)));

            // make up some random coordinates to draw the ellipse in
            left = (GenRand() % (zx + 40)) - 20;
            right = (GenRand() % (zx + 40)) - 20;
            width = (GenRand() % (zx + 40)) - 20;

            top = (GenRand() % (zy + 40)) - 20;
            bottom = (GenRand() % (zy + 40)) - 20;
            height = (GenRand() % (zy + 40)) - 20;

            switch(testFunc)
            {
                case EEllipse:
                    CheckNotRESULT(FALSE, result = Ellipse(hdc, left, top, right, bottom));
                    break;
                case ERectangle:
                    CheckNotRESULT(FALSE, result = Rectangle(hdc, left, top, right, bottom));
                    break;
                case ERoundRect:
                    CheckNotRESULT(FALSE, result = RoundRect(hdc, left, top, right, bottom, width, height));
                    break;
            }
        }

        CheckNotRESULT(0, SetROP2(hdc, R2_COPYPEN));

        // delete the last pen/brush selected in.
        CheckNotRESULT(FALSE, DeleteObject(SelectObject(hdc, (HBRUSH) GetStockObject(BLACK_BRUSH))));
        CheckNotRESULT(FALSE, DeleteObject(SelectObject(hdc, (HPEN) GetStockObject(NULL_PEN))));
    }

    myReleaseDC(hdc);
    ResetCompareConstraints();
    SetSurfVerify(bOldVerify);
}

void
SysmemShapeTest(int testFunc)
{
    info(ECHO, TEXT("%s - SysShapeTest"), funcName[testFunc]);
    SetMaxErrorPercentage(.005);
    TDC hdc = myGetDC();
    TDC hdcCompat;
    TBITMAP hbmp;
    TBITMAP hbmpStock;
    HBRUSH hbr;
    HPEN hpn;
    int left, right, top, bottom, width, height;
    DWORD *pdwBits;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));

        // myCreateBitmap creates an RGB555 bitmap by default, so if we're using an RGB565 DIB, then
        // we need to match it with an RGB565 dib for this to be valid.
        if(myGetBitDepth() == 16 && (gdwRedBitMask | gdwGreenBitMask | gdwBlueBitMask) == 0x0000FFFF)
            CheckNotRESULT(NULL, hbmp = myCreateRGBDIBSection(hdc, (VOID **) &pdwBits, 16, zx, zy, RGB565, BI_BITFIELDS, NULL));
        else
            CheckNotRESULT(NULL, hbmp = myCreateBitmap(zx, zy, 1, myGetBitDepth(), NULL));

        CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));
        CheckNotRESULT(FALSE, PatBlt(hdcCompat, 0, 0, zx, zy, WHITENESS));

        for(int i=0; i<20; i++)
        {
            // if the bit depth is less than 8, and the surface being tested is a DIB,
            // it won't have the system palette so the only colors guaranteed to
            // be common between the bitmap and DIB are black and white.
            if(myGetBitDepth() > 8)
            {
                CheckNotRESULT(NULL, hpn = CreatePen(PS_SOLID, 1, randColorRef()));
                CheckNotRESULT(NULL, hbr= CreateSolidBrush(randColorRef()));
            }
            else
            {
                CheckNotRESULT(NULL, hpn = CreatePen(PS_SOLID, 1, RGB(0x0, 0x0, 0x0)));
                CheckNotRESULT(NULL, hbr= CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF)));
            }
            int rop2 = gnvRop2Array[GenRand() % countof(gnvRop2Array)].dwVal;

            CheckNotRESULT(0, SetROP2(hdc, rop2));
            CheckNotRESULT(0, SetROP2(hdcCompat, rop2));

            // delete the previous objects and select in the new one

            // when they're deselected from hdc, they're still selected into hdcCompat
            CheckNotRESULT(NULL, SelectObject(hdc, hbr));
            CheckNotRESULT(NULL, SelectObject(hdc, hpn));

            // now they're actually freed and we can delete them.
            CheckNotRESULT(FALSE, DeleteObject(SelectObject(hdcCompat, hbr)));
            CheckNotRESULT(FALSE, DeleteObject(SelectObject(hdcCompat, hpn)));

            // reset the dc's to white for drawing.
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            CheckNotRESULT(FALSE, PatBlt(hdcCompat, 0, 0, zx, zy, WHITENESS));

            left = (GenRand() % (zx/2 + 40)) - 20;
            right = (GenRand() % (zx/2 + 40)) - 20;
            width = (GenRand() % (zx/2 + 40)) - 20;

            top = (GenRand() % (zy + 40)) - 20;
            bottom = (GenRand() % (zy + 40)) - 20;
            height = (GenRand() % (zy + 40)) - 20;

            switch(testFunc)
            {
                case ERectangle:
                    CheckNotRESULT(FALSE, Rectangle(hdc, left, top, right, bottom));
                    CheckNotRESULT(FALSE, Rectangle(hdcCompat, left, top, right, bottom));
                break;
            }

            CheckNotRESULT(FALSE, BitBlt(hdc, zx/2, 0, zx/2, zy, hdcCompat, 0, 0, SRCCOPY));

            CheckScreenHalves(hdc);
        }

        CheckNotRESULT(0, SetROP2(hdc, R2_COPYPEN));
        CheckNotRESULT(0, SetROP2(hdcCompat, R2_COPYPEN));

        CheckNotRESULT(NULL, SelectObject(hdc, (HBRUSH) GetStockObject(BLACK_BRUSH)));
        CheckNotRESULT(NULL, SelectObject(hdc, (HPEN) GetStockObject(NULL_PEN)));
        CheckNotRESULT(FALSE, DeleteObject(SelectObject(hdcCompat, (HBRUSH) GetStockObject(BLACK_BRUSH))));
        CheckNotRESULT(FALSE, DeleteObject(SelectObject(hdcCompat, (HPEN) GetStockObject(NULL_PEN))));

        CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
        CheckNotRESULT(FALSE, DeleteObject(hbmp));
        CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
    }

    myReleaseDC(hdc);
    ResetCompareConstraints();
}


//***********************************************************************************
void
SimpleRectTest(int testFunc)
{
    info(ECHO, TEXT("%s - Rect and Round"), funcName[testFunc]);
    TDC     hdc = myGetDC();
    int     result;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // clear the screen for CheckAllWhite
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

        CheckNotRESULT(FALSE, Rectangle(hdc, 50, 50, 50, 100));

        result = CheckAllWhite(hdc, FALSE, 0);
        if (result)
            info(FAIL, TEXT("Rectangle(hdc,  50, 50, 50, 100) expected no drawing"));

        CheckNotRESULT(FALSE, Rectangle(hdc, 50, 50, 100, 50));

        result = CheckAllWhite(hdc, FALSE, 0);
        if (result)
            info(FAIL, TEXT("Rectangle(hdc,  50, 50, 100, 50) expected no drawing"));

        CheckNotRESULT(FALSE, Rectangle(hdc, 50, 50, 50, 50));

        result = CheckAllWhite(hdc, FALSE, 0);
        if (result)
            info(FAIL, TEXT("Rectangle(hdc,  50, 50, 50, 50) expected no drawing"));
    }

    myReleaseDC(hdc);
}

void
SimpleRectTest2(int testFunc)
{
    info(ECHO, TEXT("%s - right=50 < left=100"), funcName[testFunc]);
    TDC     hdc = myGetDC();
    int     result;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

        CheckNotRESULT(FALSE, Rectangle(hdc, 100, 100, 50, 50));

        // we expect atleast one non-white pixel, when run on a printer DC CheckAllWhite just returns expected.
        result = CheckAllWhite(hdc, TRUE, 1);
        if (!result)
            info(FAIL, TEXT("Rectangle(hdc,  100, 100, 50, 50) expected drawing, got %d pixels"), result);
    }

    myReleaseDC(hdc);
}

//***********************************************************************************
void
SimpleBltTest(int testFunc)
{
    info(ECHO, TEXT("%s - SimpleBltTest"), funcName[testFunc]);
    TDC     hdc = myGetDC(),
            memDC;

    TBITMAP hBmp,
            stockBmp;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, memDC = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hBmp = CreateCompatibleBitmap(hdc, 30, 30));
        CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(memDC, hBmp));

        // clear the screen for CheckAllWhite
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

        switch (testFunc)
        {
            case EBitBlt:
                CheckNotRESULT(FALSE, BitBlt(hdc, 0, 0, 0, 50, memDC, 0, 0, SRCCOPY));
                break;
            case EMaskBlt:
                CheckNotRESULT(FALSE, MaskBlt(hdc, 0, 0, 0, 50, memDC, 0, 0, (TBITMAP) NULL, 0, 0, SRCCOPY));
                break;
            case EStretchBlt:
                CheckNotRESULT(FALSE, StretchBlt(hdc, 0, 0, 0, 50, hdc, 0, 0, 0, 50, SRCCOPY));
                break;
        }

        CheckAllWhite(hdc, FALSE, 0);

        CheckNotRESULT(NULL, SelectObject(memDC, stockBmp));
        CheckNotRESULT(FALSE, DeleteObject(hBmp));
        CheckNotRESULT(FALSE, DeleteDC(memDC));
    }

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

    CheckNotRESULT(NULL, hBmp0 = CreateDIBSection(hdc, (LPBITMAPINFO) & bmi, DIB_RGB_COLORS, (VOID **) & lpBits, NULL, NULL));
    CheckNotRESULT(NULL, hBmp1 = CreateDIBSection(hdc, (LPBITMAPINFO) & bmi, DIB_RGB_COLORS, NULL, NULL, NULL));

    if(!hBmp0)
        info(FAIL, TEXT("Failed to create a 2bpp bitmap with lpBits=NULL passed in."));

    if(!hBmp1)
        info(FAIL, TEXT("Failed to create a 2bpp bitmap with NULL lpBits passed in."));


    CheckNotRESULT(FALSE, DeleteObject(hBmp0));
    CheckNotRESULT(FALSE, DeleteObject(hBmp1));
    myReleaseDC(hdc);
}

void
DrawEdgeTest1(int testFunc)
{
    info(ECHO, TEXT("%s - passing Invalid Edge type:"), funcName[testFunc]);
    TDC     hdc = myGetDC();
    RECT    rc = { 50, 50, 100, 100 };

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // for 1bpp, monochromicity and flatness are enforced, so there aren't any bad
        // edge types.
        if(myGetBitDepth() > 1)
        {
            SetLastError(ERROR_SUCCESS);
            // printer DC's are metafiles, and therefore do not do all of the error checking expected.
            CheckForRESULT(isPrinterDC(hdc)?TRUE:FALSE, DrawEdge(hdc, &rc, 0xFFFFFFFF, BF_RECT));
            if(!isPrinterDC(hdc))
                CheckForLastError(ERROR_INVALID_PARAMETER);
        }
    }

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

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(0, expected = (COLORREF)GetSysColor(COLOR_BTNFACE));
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
        CheckNotRESULT(FALSE, DrawEdge(hdc, &rc, EDGE_RAISED, BF_MIDDLE | BF_RECT));

        x = 75, y = 75;
        if (!isPrinterDC(hdc))
        {
            CheckNotRESULT(CLR_INVALID, color = GetPixel(hdc, x, y));
        }
        else
        {
            // Since GetPixel does not work on printer DC's, just substitute the expected.
            color = expected;
        }

        if (color != expected)
        {
            COLORREF color2,
                     expected2;

            // check again:
            CheckNotRESULT(CLR_INVALID, color2 = GetNearestColor(hdc, color));
            CheckNotRESULT(CLR_INVALID, expected2 = GetNearestColor(hdc, expected));

            if (color2 != expected2)
                info(FAIL, TEXT("Returned:%X Expected:%X ---Nearestcolor: Returned:%X Expected:%X  "),
                color, expected, color2, expected2);
        }
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

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // clear the surface to check for a white pixel
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
        CheckNotRESULT(FALSE, DrawEdge(hdc, &rc, EDGE_RAISED, BF_FLAT | BF_RECT));

        CheckNotRESULT(CLR_INVALID, color = GetPixel(hdc, 75, 75));

        // Expected white center:
        if (!isPrinterDC(hdc) && !isWhitePixel(hdc, color, 75, 75, 1))
            info(FAIL, TEXT("returned 0x%lX expected 0x%lX when Expected white center"), color, RGB(255, 255, 255));
    }

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

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

        //TODO: some form of verification that the edges look right.
        for(int i = 0; i < countof(InnerEdge); i++)
            for(int j = 0; j < countof(OuterEdge); j++)
            {
                for(int k = 0; k < countof(Type1); k++)
                    for(int l = 0; l < countof(Type2); l++)
                        for(int m = 0; m < countof(Type3); m++)
                        {
                            // get our width/height
                            rc.right = (GenRand() % (zx - 10)) + 20;
                            rc.bottom = (GenRand() % (zy - 10)) + 20;
                            // get our top left
                            rc.left = (GenRand()%(zx + 10 - rc.right)) - 5;
                            rc.top = (GenRand()%(zy + 10 - rc.bottom)) - 5;
                            // adjust width/height by the new upper left
                            rc.right += rc.left;
                            rc.bottom += rc.top;

                            rctmp = rc;
                            CheckNotRESULT(FALSE, DrawEdge(hdc, &rctmp, InnerEdge[i].dwVal | OuterEdge[j].dwVal, Type1[k].dwVal | Type2[l].dwVal | Type3[m].dwVal));

                            CheckNotRESULT(FALSE, PatBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, WHITENESS));
                            if(CheckAllWhite(hdc, FALSE, 0))
                            {
                                info(FAIL, TEXT("IterateDrawEdge: Testing inner edge:%s outer edge:%s"), InnerEdge[i].szName, OuterEdge[j].szName);
                                info(FAIL, TEXT("IterateDrawEdge: Rectangle drawn too large %s | %s, %s | %s | %s"), InnerEdge[i].szName, OuterEdge[j].szName, Type1[k].szName, Type2[l].szName, Type3[m].szName);
                                info(FAIL, TEXT("IterateDrawEdge: Expected to be within top:%d left:%d bottom:%d right:%d"), rc.top, rc.left, rc.bottom, rc.right);
                                CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
                            }
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
                hBitmap;
        int     i,
                shift[9][2] = { {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}, {1, 0},
                                {0, 0}
                              };
        RECT    r;

        if (!IsWritableHDC(hdc))
            info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
        else
        {
            CheckNotRESULT(NULL, hBitmap = myLoadBitmap(globalInst, TEXT("PARIS0")));
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));

            // set up region
            CheckNotRESULT(NULL, hRgn = CreateRectRgn(5, 5, zx - 5, zy - 5));
            CheckNotRESULT(NULL, GetRgnBox(hRgn, &r));

            // set up bitmaps
            CheckNotRESULT(NULL, compDC = CreateCompatibleDC(hdc));
            CheckNotRESULT(NULL, hOrigBitmap = (TBITMAP) SelectObject(compDC, hBitmap));

            for (i = 0; i < 1; i++)
            {
                CheckNotRESULT(ERROR, SelectClipRgn(hdc, NULL));
                CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
                CheckNotRESULT(FALSE, BitBlt(hdc, (zx / 2 - PARIS_X) / 2 - RESIZE(20) * shift[i][0], (zy - PARIS_Y) / 2 - RESIZE(20) * shift[i][1], PARIS_X, PARIS_Y, compDC, 0, 0, SRCCOPY));
                CheckNotRESULT(FALSE, BitBlt(hdc, (zx / 2 - PARIS_X) / 2 + zx / 2, (zy - PARIS_Y) / 2, PARIS_X, PARIS_Y, compDC, 0, 0, SRCCOPY));
                CheckNotRESULT(ERROR, SelectClipRgn(hdc, hRgn));
                CheckNotRESULT(FALSE, BitBlt(hdc, RESIZE(20) * shift[i][0], RESIZE(20) * shift[i][1], zx / 2, zy, hdc, 0, 0, SRCCOPY));
                CheckScreenHalves(hdc);
            }

            CheckNotRESULT(ERROR, SelectClipRgn(hdc, NULL));
            CheckNotRESULT(FALSE, DeleteObject(hRgn));
            CheckNotRESULT(NULL, SelectObject(compDC, hOrigBitmap));
            CheckNotRESULT(FALSE, DeleteDC(compDC));
            CheckNotRESULT(FALSE, DeleteObject(hBitmap));
        }

        myReleaseDC(hdc);

        // restore the default constraint
        SetWindowConstraint(0,0);
    }
    else info(DETAIL, TEXT("Screen too small for test"));
}

void
TestSBltCase10(void)
{
    NOPRINTERDCV(NULL);

    // top is the top of the rectangle being copied
    const int top = 85;
    TDC     hdc = myGetDC();
    // height is the height of the rectangle being copied
    int height = min(200, zy - top);
    info(ECHO, TEXT("TestSBltCase10: StretchBlt(hdc,100,84,10,%d,hdc,100,85,10,%d,SRCCOPY"), height, height);

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
        // using the height as min(200, zy - top) keeps the test from pulling unknown data from below the bottom of the box
        CheckNotRESULT(FALSE, StretchBlt(hdc, 100, top - 1, 10, height, hdc, 100, top, 10, height, SRCCOPY));

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
            srcDC;
    TBITMAP hOrigBitmap;
    TBITMAP hSrc;
    int     x,
            y,
            m,
            result;
    BLENDFUNCTION bf;

    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 0xFF;
    bf.AlphaFormat = 0;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, srcDC = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hSrc = CreateCompatibleBitmap(hdc, zx / 4, zy / 2));

        // set up src
        CheckNotRESULT(NULL, hOrigBitmap = (TBITMAP) SelectObject(srcDC, hSrc));
        CheckNotRESULT(FALSE, PatBlt(srcDC, 0, 0, zx, zy, WHITENESS));
        for (x = 0; x < 10; x++)
            for (y = 0; y < 10; y++)
                if ((x + y) % 2 == 0)
                {
                    CheckNotRESULT(FALSE, PatBlt(srcDC, x * microSquare, y * microSquare, microSquare, microSquare, BLACKNESS));
                }

        for (m = 1; m < 15; m++)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

            switch(testFunc)
            {
                case EStretchBlt:
                    CheckNotRESULT(FALSE, result = StretchBlt(hdc, 0, 0, microSquare * 10 * m, microSquare * 10 * m, srcDC, 0, 0, microSquare * 10, microSquare * 10, SRCCOPY));
                    break;
                case EAlphaBlend:
                    CheckNotRESULT(FALSE, result = gpfnAlphaBlend(hdc, 0, 0, microSquare * 10 * m, microSquare * 10 * m, srcDC, 0, 0, microSquare * 10, microSquare * 10, bf));
                    break;
            }
            // for small device: 240*320 or 208 x240:  StretchBlt passes the middle line of the screen
            CheckNotRESULT(FALSE, PatBlt(hdc, zx / 2, 0, zx, zy, WHITENESS));

            for (x = 0; x < 10; x++)
                for (y = 0; y < 10; y++)
                    if ((x + y) % 2 == 0)
                    {
                        CheckNotRESULT(FALSE, PatBlt(hdc, x * microSquare * m + zx / 2, y * microSquare * m, microSquare * m, microSquare * m, BLACKNESS));
                    }
            CheckScreenHalves(hdc);
        }

        CheckNotRESULT(NULL, SelectObject(srcDC, hOrigBitmap));
        CheckNotRESULT(FALSE, DeleteObject(hSrc));
        CheckNotRESULT(FALSE, DeleteDC(srcDC));
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   StretchBlt Test
***
************************************************************************************/
#define macroSquare    10

//***********************************************************************************
void
StretchBltShrinkTest(int testFunc)
{
    info(ECHO, TEXT("%s - StretchBltShrinkTest"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
            srcDC;
    TBITMAP hOrigBitmap;
    TBITMAP hSrc;
    int     x,
            y,
            m,
            result;
    BLENDFUNCTION bf;

    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 0xFF;
    bf.AlphaFormat = 0;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, srcDC = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hSrc = CreateCompatibleBitmap(hdc, zx, zy));

        // set up src
        CheckNotRESULT(NULL, hOrigBitmap = (TBITMAP) SelectObject(srcDC, hSrc));
        CheckNotRESULT(FALSE, PatBlt(srcDC, 0, 0, zx, zy, WHITENESS));
        // Draw a big square to shrink to a smaller square.
        for (x = 0; x < 10; x++)
            for (y = 0; y < 10; y++)
                if ((x + y) % 2 == 0)
                {
                    CheckNotRESULT(FALSE, PatBlt(srcDC, x * macroSquare, y * macroSquare, macroSquare, macroSquare, BLACKNESS));
                }

        for (m = 15; m > 5; m--)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

            switch(testFunc)
            {
                case EStretchBlt:
                    CheckNotRESULT(FALSE, result = StretchBlt(hdc, 0, 0, microSquare * 10 * m, microSquare * 10 * m, srcDC, 0, 0, macroSquare * 10, macroSquare * 10, SRCCOPY));
                    break;
                case EAlphaBlend:
                    CheckNotRESULT(FALSE, result = gpfnAlphaBlend(hdc, 0, 0, microSquare * 10 * m, microSquare * 10 * m, srcDC, 0, 0, macroSquare * 10, macroSquare * 10, bf));
                    break;
            }
            // for small device: 240*320 or 208 x240:  StretchBlt passes the middle line of the screen
            CheckNotRESULT(FALSE, PatBlt(hdc, zx / 2, 0, zx, zy, WHITENESS));

            for (x = 0; x < 10; x++)
                for (y = 0; y < 10; y++)
                    if ((x + y) % 2 == 0)
                    {
                        CheckNotRESULT(FALSE, PatBlt(hdc, x * microSquare * m + zx / 2, y * microSquare * m, microSquare * m, microSquare * m, BLACKNESS));
                    }
            CheckScreenHalves(hdc);
        }

        CheckNotRESULT(NULL, SelectObject(srcDC, hOrigBitmap));
        CheckNotRESULT(FALSE, DeleteObject(hSrc));
        CheckNotRESULT(FALSE, DeleteDC(srcDC));
    }

    myReleaseDC(hdc);
}

//***********************************************************************************
void
StretchBltTest2(int testFunc, int nMode)
{
    info(ECHO, TEXT("%s - StretchBlt2 Test"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
            srcDC;
    TBITMAP hOrigBitmap;
    TBITMAP hSrc;
    int     x,
            y;
    int nOldMode;
    BLENDFUNCTION bf;

    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 0xFF;
    bf.AlphaFormat = 0;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(0, nOldMode = SetStretchBltMode(hdc, nMode));
        CheckNotRESULT(NULL, srcDC = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hSrc = CreateCompatibleBitmap(hdc, zx / 4, zy / 2));

        // set up src
        CheckNotRESULT(NULL, hOrigBitmap = (TBITMAP) SelectObject(srcDC, hSrc));
        CheckNotRESULT(FALSE, BitBlt(srcDC, 0, 0, zx, zy, hdc, 0, 0, SRCCOPY));
        for (x = 0; x < 10; x++)
            for (y = 0; y < 10; y++)
                if ((x + y) % 2 == 0)
                    CheckNotRESULT(FALSE, PatBlt(srcDC, x * microSquare, y * microSquare, microSquare, microSquare, BLACKNESS));

        CheckNotRESULT(FALSE, Rectangle(hdc, 10 - 1, 10 - 1, 110 + 1, 110 + 1));
        switch(testFunc)
        {
            case EStretchBlt:
                CheckNotRESULT(FALSE, StretchBlt(hdc, 10, 10, microSquare * 10 * 10, microSquare * 10 * 10, srcDC, 0, 0, microSquare * 10, microSquare * 10, SRCCOPY));
                break;
            case EAlphaBlend:
                CheckNotRESULT(FALSE, gpfnAlphaBlend(hdc, 10, 10, microSquare * 10 * 10, microSquare * 10 * 10, srcDC, 0, 0, microSquare * 10, microSquare * 10, bf));
                break;
        }
        CheckNotRESULT(0, SetStretchBltMode(hdc, nOldMode));
        CheckNotRESULT(NULL, SelectObject(srcDC, hOrigBitmap));
        CheckNotRESULT(FALSE, DeleteObject(hSrc));
        CheckNotRESULT(FALSE, DeleteDC(srcDC));
    }

    myReleaseDC(hdc);
}

//***********************************************************************************
void
StretchBltShrinkTest2(int testFunc, int nMode)
{
    info(ECHO, TEXT("%s - StretchBltShrinkTest2 Test"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
            srcDC;
    TBITMAP hOrigBitmap;
    TBITMAP hSrc;
    int     x,
            y;
    int nOldMode;
    BLENDFUNCTION bf;

    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 0xFF;
    bf.AlphaFormat = 0;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(0, nOldMode = SetStretchBltMode(hdc, nMode));
        CheckNotRESULT(NULL, srcDC = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hSrc = CreateCompatibleBitmap(hdc, zx, zy));

        // set up src
        CheckNotRESULT(NULL, hOrigBitmap = (TBITMAP) SelectObject(srcDC, hSrc));
        CheckNotRESULT(FALSE, BitBlt(srcDC, 0, 0, zx, zy, hdc, 0, 0, SRCCOPY));
        for (x = 0; x < 10; x++)
            for (y = 0; y < 10; y++)
                if ((x + y) % 2 == 0)
                    CheckNotRESULT(FALSE, PatBlt(srcDC, x * macroSquare, y * macroSquare, macroSquare, macroSquare, BLACKNESS));

        CheckNotRESULT(FALSE, Rectangle(hdc, 10 - 1, 10 - 1, 110 + 1, 110 + 1));
        switch(testFunc)
        {
            case EStretchBlt:
                CheckNotRESULT(FALSE, StretchBlt(hdc, 10, 10, microSquare * 10 * 10, microSquare * 10 * 10, srcDC, 0, 0, macroSquare * 10, macroSquare * 10, SRCCOPY));
                break;
            case EAlphaBlend:
                CheckNotRESULT(FALSE, gpfnAlphaBlend(hdc, 10, 10, microSquare * 10 * 10, microSquare * 10 * 10, srcDC, 0, 0, macroSquare * 10, macroSquare * 10, bf));
                break;
        }
        CheckNotRESULT(0, SetStretchBltMode(hdc, nOldMode));
        CheckNotRESULT(NULL, SelectObject(srcDC, hOrigBitmap));
        CheckNotRESULT(FALSE, DeleteObject(hSrc));
        CheckNotRESULT(FALSE, DeleteDC(srcDC));
    }
    
    myReleaseDC(hdc);
}

//***********************************************************************************
void
StretchBltTest3(int testFunc, int nMode)
{
    info(ECHO, TEXT("%s - StretchBlt3 Test"), funcName[testFunc]);

    // allow some flexibilty for HW acceleration
    SetMaxErrorPercentage(5.0);

    TDC     hdc = myGetDC(),
            srcDC;
    TBITMAP hOrigBitmap;
    TBITMAP hSrc;
    POINT   squares = { zx / 4 / aSquare, zy / 2 / aSquare };
    int     x,
            y,
            result;
    int nOldMode;
    BLENDFUNCTION bf;

    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 0xFF;
    bf.AlphaFormat = 0;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(0, nOldMode = SetStretchBltMode(hdc, nMode));
        CheckNotRESULT(NULL, srcDC = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hSrc = CreateCompatibleBitmap(hdc, zx / 4, zy / 2));

        // set up src
        CheckNotRESULT(NULL, hOrigBitmap = (TBITMAP) SelectObject(srcDC, hSrc));
        CheckNotRESULT(FALSE, BitBlt(srcDC, 0, 0, zx / 2, zy, hdc, 0, 0, SRCCOPY));
        for (x = 0; x < squares.x; x++)
            for (y = 0; y < squares.y; y++)
                if ((x + y) % 2 == 0)
                {
                    CheckNotRESULT(FALSE, PatBlt(srcDC, x * aSquare, y * aSquare, aSquare, aSquare, BLACKNESS));
                }

        for (x = 0; x <= zx; x += zx/5)
            for (y = 0; y <= zy; y += zy/5)
            {
                switch(testFunc)
                {
                    case EStretchBlt:
                        CheckNotRESULT(FALSE, result = StretchBlt(hdc, 0, 0, x, y, srcDC, 0, 0, zx / 4, zy / 2, SRCCOPY));
                        break;
                    case EAlphaBlend:
                        CheckNotRESULT(FALSE, result = gpfnAlphaBlend(hdc, 0, 0, x, y, srcDC, 0, 0, zx / 4, zy / 2, bf));
                        break;
                }
                if (!result)
                    info(FAIL, TEXT("%s(0x%8x,%d,%d,%d,%d,0x%8x,%d,%d,%d,%d)"), funcName[testFunc], hdc, 0, 0, x, y, srcDC, 0, 0, zx / 4, zy / 2);
            }

        CheckNotRESULT(0, SetStretchBltMode(hdc, nOldMode));
        CheckNotRESULT(NULL, SelectObject(srcDC, hOrigBitmap));
        CheckNotRESULT(FALSE, DeleteObject(hSrc));
        CheckNotRESULT(FALSE, DeleteDC(srcDC));
    }

    myReleaseDC(hdc);
    
    // verification tolerances must be left active until after myReleaseDC does verification.
    ResetCompareConstraints();
}

//***********************************************************************************
void
StretchBltShrinkTest3(int testFunc, int nMode)
{
    info(ECHO, TEXT("%s - StretchBltShrink3 Test"), funcName[testFunc]);
    
    // allow some flexibilty for HW acceleration
    SetMaxErrorPercentage(5.0);

    TDC     hdc = myGetDC(),
            srcDC;
    TBITMAP hOrigBitmap;
    TBITMAP hSrc;
    POINT   squares = { zx / aSquare, zy / aSquare };
    int     x,
            y,
            result;
    int nOldMode;
    BLENDFUNCTION bf;

    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 0xFF;
    bf.AlphaFormat = 0;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(0, nOldMode = SetStretchBltMode(hdc, nMode));
        CheckNotRESULT(NULL, srcDC = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hSrc = CreateCompatibleBitmap(hdc, zx, zy));

        // set up src
        CheckNotRESULT(NULL, hOrigBitmap = (TBITMAP) SelectObject(srcDC, hSrc));
        CheckNotRESULT(FALSE, BitBlt(srcDC, 0, 0, zx, zy, hdc, 0, 0, SRCCOPY));
        for (x = 0; x < squares.x; x++)
            for (y = 0; y < squares.y; y++)
                if ((x + y) % 2 == 0)
                {
                    CheckNotRESULT(FALSE, PatBlt(srcDC, x * aSquare, y * aSquare, aSquare, aSquare, BLACKNESS));
                }

        for (x = zx; x >= 0; x -= zx/5)
            for (y = zy; y >= 0; y -= zy/5)
            {
                switch(testFunc)
                {
                    case EStretchBlt:
                        CheckNotRESULT(FALSE, result = StretchBlt(hdc, 0, 0, x, y, srcDC, 0, 0, zx, zy, SRCCOPY));
                        break;
                    case EAlphaBlend:
                        CheckNotRESULT(FALSE, result = gpfnAlphaBlend(hdc, 0, 0, x, y, srcDC, 0, 0, zx-1, zy-1, bf));
                        break;
                }
                if (!result)
                    info(FAIL, TEXT("%s(0x%8x,%d,%d,%d,%d,0x%8x,%d,%d,%d,%d)"), funcName[testFunc], hdc, 0, 0, x, y, srcDC, 0, 0, zx, zy);
            }

        CheckNotRESULT(0, SetStretchBltMode(hdc, nOldMode));
        CheckNotRESULT(NULL, SelectObject(srcDC, hOrigBitmap));
        CheckNotRESULT(FALSE, DeleteObject(hSrc));
        CheckNotRESULT(FALSE, DeleteDC(srcDC));
    }

    myReleaseDC(hdc);

    // verification tolerances must be left active until after myReleaseDC does verification.
    ResetCompareConstraints();
}

//***********************************************************************************
void
BlitOffScreen(void)
{
    info(ECHO, TEXT("OffScreen Blit"));
    // this test blt's from the corners of the screen for testing clipping, which
    // grabs portions of the taskbar and puts them on the screen.

    NOPRINTERDCV(NULL);

    BOOL OldSurfVerif = SetSurfVerify(FALSE);

    TDC     hdc = myGetDC();

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        setTestRects();

        for (int t = 0; t < NUMRECTS; t++)
        {
            decorateScreen(hdc);
            CheckNotRESULT(FALSE, BitBlt(hdc, zx / 2, 0, zx / 2, zy, hdc, 0, 0, SRCCOPY));
            CheckNotRESULT(FALSE, StretchBlt(hdc, rTest[8].left, rTest[8].top, zx / 2, zy / 2, hdc, rTest[t].left, rTest[t].top, zx / 2, zy / 2, SRCCOPY));
        }
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
    info(ECHO, TEXT("%s - Viewport"), funcName[testFunc]);
    // non-rectnagles can have a small number of differences due to hardware differences.
    SetMaxErrorPercentage(testFunc != ERectangle ? .0005 : 0);
    TDC     hdc = myGetDC();
    HBRUSH stockHBrush;
    HPEN stockHPen;
    SIZE  szSize;
    POINT newPoint;
    POINT oldPoint;
    POINT Points[] = {{0, 0}, {50, 50}};
    int nPointCount = countof(Points);
    int testType;
    int num=goodRandomNum(32767);

    // Test GetViewportOrgEx -- this is not supported by GDI_Printer
    if (DCFlag != EGDI_Printer)
    {
        info(DETAIL, TEXT("GetViewportOrgEx"));
        CheckNotRESULT(FALSE, SetViewportOrgEx(hdc, num, 0, NULL));
        CheckNotRESULT(FALSE, GetViewportOrgEx(hdc, &newPoint));
        CheckNotRESULT(FALSE, num==newPoint.x);
        CheckNotRESULT(FALSE, SetViewportOrgEx(hdc, 0, 0, NULL));

        CheckNotRESULT(FALSE, OffsetViewportOrgEx(hdc, num, 0, NULL));
        CheckNotRESULT(FALSE, GetViewportOrgEx(hdc, &newPoint));
        CheckNotRESULT(FALSE, num==newPoint.x);
        CheckNotRESULT(FALSE, SetViewportOrgEx(hdc, 0, 0, NULL));
    }
    // Test GetWindowOrgEx
    info(DETAIL, TEXT("GetWindowOrgEx"));
    CheckNotRESULT(FALSE, SetWindowOrgEx(hdc, num, 0, NULL));
    CheckNotRESULT(FALSE, GetWindowOrgEx(hdc, &newPoint));
    CheckNotRESULT(FALSE, num==newPoint.x);
    CheckNotRESULT(FALSE, SetWindowOrgEx(hdc, 0, 0, NULL));

    // Test GetViewportExtEx and GetWindowExtEx -- not supported by GDI_Printer
    if (DCFlag != EGDI_Printer)
    {
        BOOL doTest = FALSE;
        INT width;
        INT height;
        if (DCFlag == EFull_Screen || DCFlag == EWin_Primary || DCFlag == ECreateDC_Primary)
        {
            doTest = TRUE;
            width  = GetDeviceCaps(hdc, HORZRES);
            height = GetDeviceCaps(hdc, VERTRES);
        }
        else if (gWidth && gHeight)
        {
            doTest = TRUE;
            width  = gWidth;
            height = gHeight;
        }

        if (doTest)
        {
            info(DETAIL, TEXT("GetViewportExtEx"));
            CheckNotRESULT(FALSE, GetViewportExtEx(hdc, &szSize));
            CheckNotRESULT(FALSE, 1==szSize.cx);
            CheckNotRESULT(FALSE, 1==szSize.cy);

            info(DETAIL, TEXT("GetWindowExtEx"));
            CheckNotRESULT(FALSE, GetWindowExtEx(hdc, &szSize));
            CheckNotRESULT(FALSE, 1==szSize.cx);
            CheckNotRESULT(FALSE, 1==szSize.cy);
        }
    }

    info(DETAIL, TEXT("Viewport Adjustment"));
    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        for (testType = 0; testType < 4; testType++)
        {
            myReleaseDC(hdc);
            hdc = myGetDC();

            switch (testType)
            {
                case 0:
                case 3:
                    CheckNotRESULT(FALSE, SetViewportOrgEx(hdc, 0, 0, NULL));
                    if (DCFlag != EGDI_Printer)
                    {
                        CheckNotRESULT(FALSE, GetViewportOrgEx(hdc, &newPoint));
                        CheckNotRESULT(FALSE, 0==newPoint.x);
                        CheckNotRESULT(FALSE, 0==newPoint.y);
                    }
                    break;
                case 1:
                    CheckNotRESULT(FALSE, SetWindowOrgEx(hdc, 0, 0, NULL));
                    CheckNotRESULT(FALSE, GetWindowOrgEx(hdc, &newPoint));
                    CheckNotRESULT(FALSE, 0==newPoint.x);
                    CheckNotRESULT(FALSE, 0==newPoint.y);
                    break;
                case 2:
                    CheckNotRESULT(FALSE, SetViewportOrgEx(hdc, 0, 0, NULL));
                    if (DCFlag != EGDI_Printer)
                    {
                        CheckNotRESULT(FALSE, GetViewportOrgEx(hdc, &newPoint));
                        CheckNotRESULT(FALSE, 0==newPoint.x);
                        CheckNotRESULT(FALSE, 0==newPoint.y);
                    }
                    CheckNotRESULT(FALSE, SetWindowOrgEx(hdc, 0, 0, NULL));
                    CheckNotRESULT(FALSE, GetWindowOrgEx(hdc, &newPoint));
                    CheckNotRESULT(FALSE, 0==newPoint.x);
                    CheckNotRESULT(FALSE, 0==newPoint.y);
                    break;
            }

            CheckNotRESULT(NULL, stockHBrush = (HBRUSH) SelectObject(hdc, (HBRUSH) GetStockObject(BLACK_BRUSH)));
            CheckNotRESULT(NULL, stockHPen = (HPEN) SelectObject(hdc, (HPEN) GetStockObject(BLACK_PEN)));

            switch (testFunc)
            {
                case ERectangle:
                    CheckNotRESULT(FALSE, Rectangle(hdc, 30, 30, 60, 60));
                    break;
                case EEllipse:
                    CheckNotRESULT(FALSE, Ellipse(hdc, 30, 30, 60, 60));
                    break;
                case ERoundRect:
                    CheckNotRESULT(FALSE, RoundRect(hdc, 30, 30, 60, 60, 0, 0));
                    break;
                case EPolygon:
                    CheckNotRESULT(FALSE, Polygon(hdc, Points, nPointCount));
                    break;
                case EPolyline:
                    CheckNotRESULT(FALSE, Polyline(hdc, Points, nPointCount));
                    break;
                case ELineTo:
                    CheckNotRESULT(FALSE, MoveToEx(hdc, 30, 30, NULL));
                    CheckNotRESULT(FALSE, LineTo(hdc, 60, 60));
                    break;
            }

            switch (testType)
            {
                case 0:
                    CheckNotRESULT(FALSE, SetViewportOrgEx(hdc, zx/2, 0, &oldPoint));
                    CheckNotRESULT(FALSE, 0==oldPoint.x);
                    CheckNotRESULT(FALSE, 0==oldPoint.y);
                    if (DCFlag != EGDI_Printer)
                    {
                        CheckNotRESULT(FALSE, GetViewportOrgEx(hdc, &newPoint));
                        CheckNotRESULT(FALSE, zx/2==newPoint.x);
                        CheckNotRESULT(FALSE, 0==newPoint.y);
                    }
                    break;
                case 1:
                    CheckNotRESULT(FALSE, SetWindowOrgEx(hdc, -(zx/2), 0, &oldPoint));
                    CheckNotRESULT(FALSE, GetWindowOrgEx(hdc, &newPoint));
                    CheckNotRESULT(FALSE, 0==oldPoint.x);
                    CheckNotRESULT(FALSE, 0==oldPoint.y);
                    CheckNotRESULT(FALSE, -(zx/2)==newPoint.x);
                    CheckNotRESULT(FALSE, 0==newPoint.y);
                    break;
                case 2:
                    CheckNotRESULT(FALSE, SetViewportOrgEx(hdc, zx/4, 0, &oldPoint));
                    CheckNotRESULT(FALSE, 0==oldPoint.x);
                    CheckNotRESULT(FALSE, 0==oldPoint.y);
                    if (DCFlag != EGDI_Printer)
                    {
                        CheckNotRESULT(FALSE, GetViewportOrgEx(hdc, &newPoint));
                        CheckNotRESULT(FALSE, zx/4==newPoint.x);
                        CheckNotRESULT(FALSE, 0==newPoint.y);
                    }
                    CheckNotRESULT(FALSE, SetWindowOrgEx(hdc, -(zx/4), 0, &oldPoint));
                    CheckNotRESULT(FALSE, GetWindowOrgEx(hdc, &newPoint));
                    CheckNotRESULT(FALSE, 0==oldPoint.x);
                    CheckNotRESULT(FALSE, 0==oldPoint.y);
                    CheckNotRESULT(FALSE, -(zx/4)==newPoint.x);
                    CheckNotRESULT(FALSE, 0==newPoint.y);
                    break;
                case 3:
                    CheckNotRESULT(FALSE, OffsetViewportOrgEx(hdc, zx/2, 0, &oldPoint));
                    CheckNotRESULT(FALSE, 0==oldPoint.x);
                    CheckNotRESULT(FALSE, 0==oldPoint.y);
                    if (DCFlag != EGDI_Printer)
                    {
                        CheckNotRESULT(FALSE, GetViewportOrgEx(hdc, &newPoint));
                        CheckNotRESULT(FALSE, zx/2==newPoint.x);
                        CheckNotRESULT(FALSE, 0==newPoint.y);
                    }
                    break;
            }

            switch (testFunc)
            {
                case ERectangle:
                    CheckNotRESULT(FALSE, Rectangle(hdc, 30, 30, 60, 60));
                    break;
                case EEllipse:
                    CheckNotRESULT(FALSE, Ellipse(hdc, 30, 30, 60, 60));
                    break;
                case ERoundRect:
                    CheckNotRESULT(FALSE, RoundRect(hdc, 30, 30, 60, 60, 0, 0));
                    break;
                case EPolygon:
                    CheckNotRESULT(FALSE, Polygon(hdc, Points, nPointCount));
                    break;
                case EPolyline:
                    CheckNotRESULT(FALSE, Polyline(hdc, Points, nPointCount));
                    break;
                case ELineTo:
                    CheckNotRESULT(FALSE, MoveToEx(hdc, 30, 30, NULL));
                    CheckNotRESULT(FALSE, LineTo(hdc, 60, 60));
                    break;
            }

            switch (testType)
            {
                case 0:
                    CheckNotRESULT(FALSE, SetViewportOrgEx(hdc, 0, 0, &oldPoint));
                    CheckNotRESULT(FALSE, zx/2==oldPoint.x);
                    CheckNotRESULT(FALSE, 0==oldPoint.y);
                    if (DCFlag != EGDI_Printer)
                    {
                        CheckNotRESULT(FALSE, GetViewportOrgEx(hdc, &newPoint));
                        CheckNotRESULT(FALSE, 0==newPoint.x);
                        CheckNotRESULT(FALSE, 0==newPoint.y);
                    }
                    break;
                case 1:
                    CheckNotRESULT(FALSE, SetWindowOrgEx(hdc, 0, 0, &oldPoint));
                    CheckNotRESULT(FALSE, GetWindowOrgEx(hdc, &newPoint));
                    CheckNotRESULT(FALSE, -(zx/2)==oldPoint.x);
                    CheckNotRESULT(FALSE, 0==oldPoint.y);
                    CheckNotRESULT(FALSE, 0==newPoint.x);
                    CheckNotRESULT(FALSE, 0==newPoint.y);
                    break;
                case 2:
                    CheckNotRESULT(FALSE, SetViewportOrgEx(hdc, 0, 0, &oldPoint));
                    CheckNotRESULT(FALSE, zx/4==oldPoint.x);
                    CheckNotRESULT(FALSE, 0==oldPoint.y);
                    if (DCFlag != EGDI_Printer)
                    {
                        CheckNotRESULT(FALSE, GetViewportOrgEx(hdc, &newPoint));
                        CheckNotRESULT(FALSE, 0==newPoint.x);
                        CheckNotRESULT(FALSE, 0==newPoint.y);
                    }
                    CheckNotRESULT(FALSE, SetWindowOrgEx(hdc, 0, 0, &oldPoint));
                    CheckNotRESULT(FALSE, GetWindowOrgEx(hdc, &newPoint));
                    CheckNotRESULT(FALSE, -(zx/4)==oldPoint.x);
                    CheckNotRESULT(FALSE, 0==oldPoint.y);
                    CheckNotRESULT(FALSE, 0==newPoint.x);
                    CheckNotRESULT(FALSE, 0==newPoint.y);
                    break;
                case 3:
                    CheckNotRESULT(FALSE, OffsetViewportOrgEx(hdc, -zx/2, 0, &oldPoint));
                    CheckNotRESULT(FALSE, zx/2==oldPoint.x);
                    CheckNotRESULT(FALSE, 0==oldPoint.y);
                    if (DCFlag != EGDI_Printer)
                    {
                        CheckNotRESULT(FALSE, GetViewportOrgEx(hdc, &newPoint));
                        CheckNotRESULT(FALSE, 0==newPoint.x);
                        CheckNotRESULT(FALSE, 0==newPoint.y);
                    }
                    break;
            }

            CheckScreenHalves(hdc);
            CheckNotRESULT(NULL, SelectObject(hdc, stockHBrush));
            CheckNotRESULT(NULL, SelectObject(hdc, stockHPen));

        } //for
    }

    myReleaseDC(hdc);
    // error tolerances must be left active until after myReleaseDC
    ResetCompareConstraints();
}

//***********************************************************************************
void
BitBltTest3(int testFunc)
{
    info(ECHO, TEXT("%s: DestDC: width = 0"), funcName[testFunc]);
    TDC     hdc   = myGetDC(),
            memDC = NULL;

    TBITMAP hBmp     = NULL,
            stockBmp = NULL;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, memDC = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hBmp = CreateCompatibleBitmap(hdc, 30, 30));
        CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(memDC, hBmp));

        CheckNotRESULT(FALSE, BitBlt(hdc, 0, 0, 0, 50, memDC, 0, 0, SRCCOPY));

        CheckScreenHalves(hdc);

        CheckNotRESULT(NULL, SelectObject(memDC, stockBmp));
        CheckNotRESULT(FALSE, DeleteObject(hBmp));
        CheckNotRESULT(FALSE, DeleteDC(memDC));
    }

    myReleaseDC(hdc);
}

//***********************************************************************************
void
SimpleColorConversionTest(int testFunc, int type)
{
    info(ECHO, TEXT("SimpleColorConversionTest, %s"), type?TEXT("Bitmap"):TEXT("DIB"));
    TDC hdc = myGetDC();
    TDC hdcTmp;
    TBITMAP hbmpOld;
    TBITMAP hbmpMask;
    BYTE   *lpBits = NULL;
    int bpp = myGetBitDepth();
    UINT nType[] = {DIB_RGB_COLORS, DIB_PAL_COLORS};
    TBITMAP hBmp;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hdcTmp = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hbmpMask = myCreateBitmap(zx/2, zy, 1, 1, NULL));
        CheckNotRESULT(NULL, hbmpOld = (TBITMAP) SelectObject(hdcTmp, hbmpMask));
        CheckNotRESULT(FALSE, PatBlt(hdcTmp, 0, 0, zx/2, zy, WHITENESS));
        CheckNotRESULT(NULL, SelectObject(hdcTmp, hbmpOld));

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

        // put the logo on the primary
        drawLogo(hdc, 150);

        // step through all of the bit depths
        for (int d = 0; d < countof(gBitDepths); d++)
        {
            for(int j=0; j < countof(nType); j++)
            {
#ifndef UNDER_CE
                // xp disallows selection of bitmaps of bit depths other than 1 and the primary
                if(type == 1 && !(gBitDepths[d] == 1 || gBitDepths[d] == GetDeviceCaps(hdc, BITSPIXEL)))
                    continue;
#endif

                if((gBitDepths[d] > 8 || type == 1) && nType[j] == DIB_PAL_COLORS)
                    continue;

                info(DETAIL, TEXT("Creating %s depth:%d"), type?TEXT("Bitmap"):TEXT("DIB"), gBitDepths[d]);
                if(type == 0)
                {
                    CheckNotRESULT(NULL, hBmp = myCreateDIBSection(hdc, (VOID **) & lpBits, gBitDepths[d], zx/2, zy, nType[j], NULL, FALSE));
                }
                else
                {
                    CheckNotRESULT(NULL, hBmp = myCreateBitmap(zx/2, zy, 1, gBitDepths[d], NULL));
                }
                // all checks done in myCreateDIBSection
                if (hBmp)
                {
                    TBITMAP hbmpStock;
                    CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcTmp, hBmp));
                    CheckNotRESULT(FALSE, PatBlt(hdcTmp, 0, 0, zx/2, zy, WHITENESS));
                    CheckNotRESULT(FALSE, PatBlt(hdc, zx/2, 0, zx/2, zy, BLACKNESS));
                    drawLogo(hdcTmp, 150);
                    switch(testFunc)
                    {
                        case EBitBlt:
                            CheckNotRESULT(FALSE, BitBlt(hdc, zx/2, 0, zx/2, zy, hdcTmp, 0, 0, SRCCOPY));
                        break;
                        case EMaskBlt:
                            CheckNotRESULT(FALSE, MaskBlt(hdc, zx/2, 0, zx/2, zy, hdcTmp, 0, 0, hbmpMask, 0, 0, SRCCOPY));
                        break;
                        case EStretchBlt:
                            CheckNotRESULT(FALSE, StretchBlt(hdc, zx/2, 0, zx/2, zy, hdcTmp, 0, 0, zx/2, zy, SRCCOPY));
                        break;
                        case ETransparentImage:
                            // make black transparent, the background should be black already
                            CheckNotRESULT(FALSE, TransparentBlt(hdc, zx/2, 0, zx/2, zy, hdcTmp, 0, 0, zx/2, zy, RGB(0x00, 0x00, 0x00)));
                        break;
                        default:
                            info(FAIL, TEXT("Unknown Operation for test."));
                    }

                    // always verify if we're at or over 16bpp.
                    // the halves won't be identical if paletted
                    if(gBitDepths[d] >= 16 && myGetBitDepth() >= 16)
                        CheckScreenHalves(hdc);

                    CheckNotRESULT(NULL, SelectObject(hdcTmp, hbmpStock));
                    CheckNotRESULT(FALSE, DeleteObject(hBmp));
                }
                else
                        info(ABORT, TEXT("CreateDIBSection/CreateBitmap failed"));
            }
        }

        CheckNotRESULT(FALSE, DeleteObject(hbmpMask));
        CheckNotRESULT(FALSE, DeleteDC(hdcTmp));
    }

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

    CheckNotRESULT(0, SetROP2(hdc, R2_COPYPEN));
    // good rops
    for (int r = 0; r < numRops; r++)
    {
        result = SetROP2(hdc, gnvRop2Array[r].dwVal);
        if (r > 0 && result != gnvRop2Array[r - 1].dwVal)
            info(FAIL, TEXT("SetROP2 failed to return previous set: result:%d expected:%d (%s) [%d]"), result,
                 gnvRop2Array[r - 1].dwVal, gnvRop2Array[r - 1].szName, r);
    }

    // bad rops
    for (int r = 0; r < countof(badROPs); r++)
    {
        result = SetROP2(hdc, badROPs[r]);
        if (result)
            info(FAIL, TEXT("SetROP2 should have failed given undefined ROP %x"), badROPs[r]);
    }

    CheckNotRESULT(0, SetROP2(hdc, R2_COPYPEN));
    myReleaseDC(hdc);
}

//***********************************************************************************
void
GetRop2Test(void)
{
    info(ECHO, TEXT("GetRop2Test"));
    TDC   hdc = myGetDC();
    DWORD previous_value;
    int   numRops = countof(gnvRop2Array);

    CheckNotRESULT(0, previous_value = SetROP2(hdc, R2_COPYPEN));
    // good rops
    for (int r = 0; r < numRops; r++)
    {
        CheckNotRESULT(0, SetROP2(hdc, gnvRop2Array[r].dwVal));
        CheckForRESULT(gnvRop2Array[r].dwVal, GetROP2(hdc));
    }

    // bad rops
    CheckNotRESULT(0, SetROP2(hdc, previous_value));
    for (int r = 0; r < countof(badROPs); r++)
    {
        CheckForRESULT(0, SetROP2(hdc, badROPs[r]));
        CheckForRESULT(previous_value, GetROP2(hdc));
    }

    CheckNotRESULT(0, SetROP2(hdc, previous_value));
    myReleaseDC(hdc);
}

void
DrawObjects(TDC hdc, BOOL brush, int offset)
{

    HBRUSH  tempBrush, stockBrush;
    int nBitDepth = GenRand() % countof(gBitDepths);
    int nBitmapType = GenRand() % numBitMapTypes;

    // no DIB_PAL option for anything greater than 8, and no bitmaps for loadbitmap for 16 and 32, so just use 8bpp, otherwise use the random bit depth.
    if((nBitmapType == DibPal && gBitDepths[nBitDepth] > 8) || (nBitmapType == lb && (gBitDepths[nBitDepth] == 2 || gBitDepths[nBitDepth] == 16 || gBitDepths[nBitDepth] == 32)))
        nBitDepth = 8;
    else nBitDepth = gBitDepths[nBitDepth];

    CheckNotRESULT(NULL, tempBrush = (!brush) ? (HBRUSH) GetStockObject(NULL_BRUSH) : setupBrush(hdc, nBitmapType, nBitDepth, 8, TRUE, TRUE));

    CheckNotRESULT(FALSE, PatBlt(hdc, 0, RESIZE(offset), zx, RESIZE(90), BLACKNESS));
    CheckNotRESULT(NULL, stockBrush = (HBRUSH) SelectObject(hdc, tempBrush));

    CheckNotRESULT(FALSE, Ellipse(hdc, RESIZE(20), RESIZE(20 + offset), RESIZE(160), RESIZE(160 + offset)));

    CheckNotRESULT(FALSE, RoundRect(hdc, RESIZE(90), RESIZE(20 + offset),RESIZE( 230), RESIZE(160 + offset), RESIZE(20), RESIZE(20)));

    CheckNotRESULT(FALSE, Rectangle(hdc, RESIZE(160), RESIZE(20 + offset), RESIZE(300), RESIZE(160 + offset)));

    CheckNotRESULT(NULL, SelectObject(hdc, stockBrush));

    if(tempBrush != GetStockObject(NULL_BRUSH))
        CheckNotRESULT(FALSE, DeleteObject(tempBrush));

}

//***********************************************************************************
void
ROP2Test(void)
{

    info(ECHO, TEXT("ROP2Test - Pens narrow/wide/dashed"));
    // this test case draws ellipses and other shapes, which may differ slightly, so give them some leeway.
    SetMaxErrorPercentage(.0002);
    TDC     hdc = myGetDC();

    POINT   p[6][2] = { { {RESIZE(190), RESIZE(20)}, {RESIZE(330), RESIZE(160)} },
                      { {RESIZE(190), RESIZE( 160)}, {RESIZE(330), RESIZE(20)} },
                      { {RESIZE(360), RESIZE(20)}, {RESIZE(360), RESIZE(160)} },
                      { {RESIZE(390), RESIZE(20)}, {RESIZE(390), RESIZE(160)} },
                      { {RESIZE(420), RESIZE(20)}, {RESIZE(420), RESIZE(160)} },
                      { {RESIZE(450), RESIZE(20)}, {RESIZE(450), RESIZE(160)} } };

    COLORREF c[4] = { RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(0, 255, 255) };

    TCHAR   str[256] = {NULL};
    RECT    StrRect = { RESIZE(20), RESIZE(180), RESIZE(20)+ 380, RESIZE(180) + 20 };
    HPEN    hPen, hpenStock;
    LOGPEN  logPen;

    NameValPair styles[] = {
        NAMEVALENTRY(PS_SOLID),
        NAMEVALENTRY(PS_DASH),
        NAMEVALENTRY(PS_NULL),
        //             NAMEVALENTRY(PS_DOT),        future features
        //             NAMEVALENTRY(PS_DASHDOT),
        //             NAMEVALENTRY(PS_DASHDOTDOT),
        //             NAMEVALENTRY(PS_INSIDEFRAME)
    };

    int     numStyles = countof(styles),
            numRops = countof(gnvRop2Array);

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(0, SetROP2(hdc, R2_COPYPEN));

        for (int r = 0; r < numRops; r++)
            for (int i = 0; i < numStyles; i++)
            {
                for (int n = -1; n < 50; n += (n < 5) ? 1 : 5)
                {
                    CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
                    CheckNotRESULT(0, SetROP2(hdc, gnvRop2Array[r].dwVal));
                    logPen.lopnStyle = styles[i].dwVal;
                    logPen.lopnWidth.x = logPen.lopnWidth.y = n;
                    logPen.lopnColor = RGB(0, 0, 0);
                    CheckNotRESULT(NULL, hPen = CreatePenIndirect(&logPen));
                    CheckNotRESULT(NULL, hpenStock = (HPEN) SelectObject(hdc, hPen));
                    DrawObjects(hdc, 0, 0);
                    DrawObjects(hdc, 1, 200);
                    CheckNotRESULT(FALSE, Polyline(hdc, p[0], 2));
                    CheckNotRESULT(FALSE, Polyline(hdc, p[1], 2));
                    CheckNotRESULT(NULL, SelectObject(hdc, hpenStock));
                    CheckNotRESULT(FALSE, DeleteObject(hPen));

                    for (int j = 0; j < 4; j++)
                    {
                        logPen.lopnColor = c[j];
                        CheckNotRESULT(NULL, hPen = CreatePenIndirect(&logPen));
                        CheckNotRESULT(NULL, hpenStock = (HPEN) SelectObject(hdc, hPen));
                        CheckNotRESULT(FALSE, Polyline(hdc, p[j + 2], 2));
                        CheckNotRESULT(NULL, SelectObject(hdc, hpenStock));
                        CheckNotRESULT(FALSE, DeleteObject(hPen));
                    }

                    _sntprintf_s(str, countof(str) - 1, TEXT("Pen:(%d) Style:%s ROP2:%s (%d)"), n, styles[i].szName, gnvRop2Array[r].szName, gnvRop2Array[r].dwVal);
                    CheckNotRESULT(0, DrawText(hdc, str, -1, &StrRect, DT_LEFT));

                    SHORT   k = 0;
                    while (verifyFlag == EVerifyManual && !k)
                    {
                        k = GetAsyncKeyState(VK_LSHIFT);
                        Sleep(10); // for keyboard timing
                    }
                }
            }
        CheckNotRESULT(0, SetROP2(hdc, R2_COPYPEN));
    }

    myReleaseDC(hdc);
    ResetCompareConstraints();
}

void
DrawStuff(TDC hdc, int rop, const TCHAR * szRop, BOOL rop4)
{
    TCHAR   str[256] = {NULL};
    RECT    StrRect = { RESIZE(20), RESIZE(180), RESIZE(20)+380, RESIZE(180)+20 };
    HBRUSH  tempBrush, stockBrush;

    CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
    for(int j = 0; j < countof(gBitDepths); j++)
        for (int i = 0; i < numBitMapTypes + 1; i++)
        {
            // no DIB_PAL option for anything greater than 8, and no bitmaps for loadbitmap for 16 and 32, so just skip.
            if((i == DibPal && gBitDepths[j] > 8) || (i == lb && (gBitDepths[j] == 2 || gBitDepths[j] == 16 || gBitDepths[j] == 32)))
                continue;

            CheckNotRESULT(NULL, tempBrush = (i == numBitMapTypes) ? (HBRUSH) GetStockObject(BLACK_BRUSH) : setupBrush(hdc, i, gBitDepths[j], 8, 1, 1));
            CheckNotRESULT(NULL, stockBrush = (HBRUSH) SelectObject(hdc, tempBrush));
            // PatBlt is expected to fail for ROP's the require a source.
            PatBlt(hdc, RESIZE(20 + i * 10), RESIZE(20 + i * 10), RESIZE(60), RESIZE(60), rop);
            CheckNotRESULT(FALSE, PatBlt(hdc, RESIZE(350 + i * 10), RESIZE(20 + i * 10), RESIZE(60), RESIZE(60), PATCOPY));
            CheckNotRESULT(NULL, SelectObject(hdc, stockBrush));

            if(tempBrush != GetStockObject(BLACK_BRUSH))
                CheckNotRESULT(FALSE, DeleteObject(tempBrush));
        }

    CheckNotRESULT(NULL, stockBrush = (HBRUSH) SelectObject(hdc, GetStockObject(BLACK_BRUSH)));
    CheckNotRESULT(FALSE, Ellipse(hdc, RESIZE(200), RESIZE(20), RESIZE(250), RESIZE(70)));
    CheckNotRESULT(FALSE, RoundRect(hdc, RESIZE(240), RESIZE(40), RESIZE(290), RESIZE(90), RESIZE(10), RESIZE(10)));
    CheckNotRESULT(FALSE, Rectangle(hdc, RESIZE(280), RESIZE(60), RESIZE(330), RESIZE(110)));

    if (!rop4)
    {
        CheckNotRESULT(FALSE, BitBlt(hdc, 0, RESIZE(220), zx, RESIZE(200), hdc, 0, 0, rop));
    }
    else
    {
        TDC     compDC;
        TBITMAP hBitmap,
                hMask,
                hStock;

        CheckNotRESULT(NULL, compDC = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hBitmap = myLoadBitmap(globalInst, TEXT("PARIS0")));
        CheckNotRESULT(NULL, hMask = myLoadBitmap(globalInst, TEXT("PARISZ")));

        CheckNotRESULT(NULL, hStock = (TBITMAP) SelectObject(compDC, hBitmap));

        CheckNotRESULT(FALSE, MaskBlt(hdc, RESIZE(50), RESIZE(220), PARIS_X, PARIS_Y, compDC, 0, 0, (TBITMAP) NULL, 0, 0, rop));
        // expect this call to fail, not using a 1bpp mask/error condition
        MaskBlt(hdc, RESIZE(200), RESIZE(220), PARIS_X, PARIS_Y, compDC, 0, 0, hBitmap, 0, 0, rop);
        CheckNotRESULT(FALSE, MaskBlt(hdc, RESIZE(340), RESIZE(220), PARIS_X, PARIS_Y, compDC, 0, 0, hMask, 0, 0, rop));

        CheckNotRESULT(NULL, SelectObject(compDC, hStock));
        CheckNotRESULT(FALSE, DeleteObject(hBitmap));
        CheckNotRESULT(FALSE, DeleteObject(hMask));
        CheckNotRESULT(FALSE, DeleteDC(compDC));
    }

    // stop and wait
    _sntprintf_s(str, countof(str) - 1, TEXT("ROP:0x%x [%s]"), rop, szRop);
    CheckNotRESULT(0, DrawText(hdc, str, -1, &StrRect, DT_LEFT));
    SHORT   k = 0;

    while (verifyFlag == EVerifyManual && !k)
    {
        k = GetAsyncKeyState(VK_LSHIFT);
        Sleep(10); // for keyboard timing
    }
    Sleep(10); // for keyboard timing

    CheckNotRESULT(NULL, SelectObject(hdc, stockBrush));
}

void
ROP3Test(void)
{
    info(ECHO, TEXT("SetRop3 Test"));
    // this test case draws ellipses and other shapes, which may differ slightly, so give them some leeway.
    SetMaxErrorPercentage(.0001);
    TDC     hdc = myGetDC();

    int     numRops = countof(gnvRop3Array),
            rop;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // BitBlt+PatBlt: brush selected: pattern/solid
        for (int i = 0; i < numRops; i++)
        {
            rop = gnvRop3Array[i].dwVal;
            DrawStuff(hdc, rop, gnvRop3Array[i].szName, FALSE);
        }
    }

    myReleaseDC(hdc);
    ResetCompareConstraints();
}

//***********************************************************************************
void
ROP4Test(void)
{
    info(ECHO, TEXT("SetRop4 Test"));

    // this test case draws ellipses and other shapes, which may differ slightly, so give them some leeway.
    SetMaxErrorPercentage(.0001);
    if(SetWindowConstraint(2*PARIS_X+20, PARIS_Y))
    {
        TDC     hdc = myGetDC();
        int     numRops = countof(gnvRop4Array);

        if (!IsWritableHDC(hdc))
            info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
        else
        {
            // BitBlt+PatBlt: brush selected: pattern/solid
            for (int i = 0; i < numRops; i++)
                DrawStuff(hdc, gnvRop4Array[i].dwVal, gnvRop4Array[i].szName, TRUE);
        }

        myReleaseDC(hdc);
        SetWindowConstraint(0,0);
    }
    else info(DETAIL, TEXT("Screen too small for test"));
    ResetCompareConstraints();
}

void
SimpleTransparentImageTest(void)
{

#ifndef UNDER_CE
    info(ECHO, TEXT("SimpleTransparentImageTest --- CE only"));
#else
    info(ECHO, TEXT("SimpleTransparentImageTest"));

    TDC hdc = myGetDC(),
          comphdc = NULL;
    TBITMAP bmp = NULL,
                stockbmp = NULL;
    HBRUSH hbrush,
               hredbrush;
    COLORREF cr,
                  cr1,
                  crExpected;
    COLORREF crRGBBlack;
    COLORREF crRGBGreen;
    COLORREF crRGBRed;
    RECT rc;
    RECT greenRect,
            redRect;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        if(myGetBitDepth() > 8)
        {
            crRGBBlack = RGB(0,0,0);
            crRGBGreen = RGB(0,255,0);
            crRGBRed = RGB(255,0,0);
            crExpected = RGB(0xFF, 0x0, 0x0);
        }
        else
        {
            CheckNotRESULT(CLR_INVALID, crRGBBlack = GetNearestColor(hdc, RGB(0,0,0)));
            CheckNotRESULT(CLR_INVALID, crRGBGreen = GetNearestColor(hdc, RGB(0,255,0)));
            CheckNotRESULT(CLR_INVALID, crRGBRed = GetNearestColor(hdc, RGB(255,0,0)));

            // with shades of gray palettes, solid red and solid green
            // are the same color.  So, use some different shades of gray
            // instead.  black will be available.
            if(crRGBGreen == crRGBRed)
            {
                CheckNotRESULT(CLR_INVALID, crRGBRed = GetNearestColor(hdc, RGB(85,85,85)));
                CheckNotRESULT(CLR_INVALID, crRGBGreen = GetNearestColor(hdc, RGB(170,170,170)));
            }
            crExpected = crRGBRed;
        }

        if (isPrinterDC(hdc))
        {
            CheckNotRESULT(NULL, comphdc = CreateCompatibleDC(hdc));
            CheckNotRESULT(NULL, bmp = CreateCompatibleBitmap(hdc, zx, zy));
            CheckNotRESULT(NULL, stockbmp = (TBITMAP) SelectObject(comphdc, bmp));
        }
        else
            comphdc = hdc;

        rc.left = 0;
        rc.top = 0;
        rc.bottom = zy;
        rc.right = zx;

        CheckNotRESULT(FALSE, FillRect(comphdc, &rc, (HBRUSH) GetStockObject(WHITE_BRUSH)));

        //rc.right -= 20;
        //rc.bottom -= 20;
        CheckNotRESULT(FALSE, FillRect(comphdc, &rc, (HBRUSH) GetStockObject(BLACK_BRUSH)));

        int     qWidth = (rc.right - rc.left) / 2;
        int     qHeight = (rc.bottom - rc.top) / 2;

        CheckNotRESULT(NULL, hbrush = CreateSolidBrush(crRGBGreen));
        greenRect = rc;
        greenRect.left += qWidth;
        greenRect.top += qHeight;
        CheckNotRESULT(FALSE, FillRect(comphdc, &greenRect, hbrush));

        CheckNotRESULT(NULL, hredbrush = CreateSolidBrush(crRGBRed));
        redRect = greenRect;
        redRect.left += qWidth / 2;
        redRect.top += qHeight / 2;
        CheckNotRESULT(FALSE, FillRect(comphdc, &redRect, hredbrush));
        CheckNotRESULT(FALSE, TransparentImage(hdc, rc.left, rc.top, qWidth, qHeight, comphdc, greenRect.left, greenRect.top, qWidth, qHeight, crRGBGreen));

        if (isPrinterDC(hdc))
        {
            // Since GetPixel will not work on a printer DC, we skip checking if TransparentImage did the right thing.
            info(DETAIL, TEXT("TransparentImage check skipped on printer DC"));
        }
        else
        {
            int     x = rc.left + qWidth / 2 + 2,
                    y = rc.top + qHeight / 2 + 2;

            CheckNotRESULT(CLR_INVALID, cr1 = GetPixel(hdc, x, y));

            if (cr1 != crExpected)
            {
                info(FAIL, TEXT("TransparentImage: at [%d,%d] expect color = 0x%08X: but get 0x%08X"), x, y, crExpected, cr1);
            }
        }

        // Check result
        int     x = rc.left + 2,
                y = rc.top + 2;

        if (!isPrinterDC(hdc))
        {
            CheckNotRESULT(CLR_INVALID, cr = GetPixel(hdc, x, y));
            if (cr != crRGBBlack)       // An RGB value of 0 is Black
            {
                info(FAIL, TEXT("TransparentImage: at [%d,%d] expect color = 0x%08X: but get 0x%08X"), x, y, crRGBBlack, cr);
            }
        }

        if (comphdc != hdc)
        {
            CheckNotRESULT(NULL, SelectObject(comphdc, stockbmp));
            CheckNotRESULT(FALSE, DeleteDC(comphdc));
            CheckNotRESULT(FALSE, DeleteObject(bmp));
        }
        CheckNotRESULT(FALSE, DeleteObject(hbrush));
        CheckNotRESULT(FALSE, DeleteObject(hredbrush));
    }

    myReleaseDC(hdc);
#endif // UNDER_CE
}

void
TransparentBltErrorTest(void)
{
    info(ECHO, TEXT("TransparentBltErrorTest"));

    TDC hdc = myGetDC();

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        if(myGetBitDepth() > 1)
        {
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
                // plus 0x10 to take into account RGB4444 to 24bpp conversion
                // and the fact that we don't want r, g, or b, to be 0, because we're testing with them
                // at 0
                // 0 - 0xE0 to take into account the + 0x10.
                r = (GenRand() % 0xE0) + 0x10,
                g = (GenRand() % 0xE0) + 0x10,
                b = (GenRand() % 0xE0) + 0x10;
            }

            RECT rc = { 0, 0, zx/2, zy};
            HBRUSH hbr;
            DWORD rgb[] = { {RGB(r, 0, 0)},
                                       {RGB(0, g, 0)},
                                       {RGB(0, 0, b)},
                                       {RGB(r, g, 0)},
                                       {RGB(0, g, b)},
                                       {RGB(r, 0, b)}};

            CheckNotRESULT(NULL, hbr = CreateSolidBrush(RGB(r, g, b)));

            CheckNotRESULT(FALSE, FillRect(hdc, &rc, hbr));

            for(int i = 0; i < countof(rgb); i++)
            {
                CheckNotRESULT(FALSE, PatBlt(hdc, zx/2, 0, zx/2, zy, BLACKNESS));
                CheckNotRESULT(FALSE, TransparentBlt(hdc, zx/2, 0, zx/2, zy, hdc, 0, 0, zx/2, zy, rgb[i]));
                if(!CheckScreenHalves(hdc))
                    info(FAIL, TEXT("color incorrectly made transparent.  Source color (0x%x, 0x%x, 0x%x), transparent color (0x%x, 0x%x, 0x%x)."),
                                                                                                        r, g, b, rgb[i] & 0xFF, (rgb[i] >> 8) & 0xFF, (rgb[i] >> 16) & 0xFF);
            }

            CheckNotRESULT(FALSE, DeleteObject(hbr));
        }
        else info(DETAIL, TEXT("Test requires more than 2 colors to test tranparency, skipping a portion of the test."));
    }

    myReleaseDC(hdc);
}

void
TransparentBltPatBltTest()
{
    info(ECHO, TEXT("TransparentBlt PatBlt Transparency test"));
    NOPRINTERDCV(NULL);

    TDC hdc = myGetDC();
    COLORREF cr, crWhite, crBlack;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // ****  from pixel 0 to pixel 1 with white as transparent
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, 1, 1, WHITENESS));
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 1, 1, 1, BLACKNESS));

        // set up our black and white pixels
        CheckNotRESULT(CLR_INVALID, crWhite = GetPixel(hdc, 0, 0));
        CheckNotRESULT(CLR_INVALID, crBlack = GetPixel(hdc, 0, 1));

        CheckNotRESULT(FALSE, TransparentBlt(hdc, 0, 1, 1, 1, hdc, 0, 0, 1, 1, crWhite));
        CheckNotRESULT(CLR_INVALID, cr = GetPixel(hdc, 0, 1));
        if(cr != crBlack)
            info(FAIL, TEXT("white incorrectly made opaq"));

        // **** from pixel 0 to pixel 1 with black as transparent

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, 1, 1, WHITENESS));
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 1, 1, 1, BLACKNESS));

        // set up our black and white pixels
        CheckNotRESULT(CLR_INVALID, crWhite = GetPixel(hdc, 0, 0));
        CheckNotRESULT(CLR_INVALID, crBlack = GetPixel(hdc, 0, 1));

        CheckNotRESULT(FALSE, TransparentBlt(hdc, 0, 1, 1, 1, hdc, 0, 0, 1, 1, crBlack));
        CheckNotRESULT(CLR_INVALID, cr = GetPixel(hdc, 0, 1));
        if(cr != crWhite)
            info(FAIL, TEXT("white incorrectly made transparent"));

        // **** from pixel 1 to pixel 0 with black as transparent

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, 1, 1, WHITENESS));
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 1, 1, 1, BLACKNESS));

        CheckNotRESULT(FALSE, TransparentBlt(hdc, 0, 0, 1, 1, hdc, 0, 1, 1, 1, crBlack));
        CheckNotRESULT(CLR_INVALID, cr = GetPixel(hdc, 0, 0));
        if(cr != crWhite)
            info(FAIL, TEXT("black incorrectly made opaq"));

        // **** from pixel 1 to pixel 0 with white as transparent

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, 1, 1, WHITENESS));
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 1, 1, 1, BLACKNESS));

        CheckNotRESULT(FALSE, TransparentBlt(hdc, 0, 0, 1, 1, hdc, 0, 1, 1, 1, crWhite));
        CheckNotRESULT(CLR_INVALID, cr = GetPixel(hdc, 0, 0));
        if(cr != crBlack)
            info(FAIL, TEXT("black incorrectly made transparent"));
    }

    myReleaseDC(hdc);

}

void
TransparentBltSetPixelTest()
{
    info(ECHO, TEXT("TransparentBlt SetPixel Transparency test"));
    NOPRINTERDCV(NULL);

    TDC hdc = myGetDC();
    COLORREF cr, crWhite, crBlack;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // ****  from pixel 0 to pixel 1 with white as transparent
        CheckNotRESULT(-1, SetPixel(hdc, 0, 0, RGB(255, 255, 255)));
        CheckNotRESULT(-1, SetPixel(hdc, 0, 1, RGB(0, 0, 0)));

        // set up our black and white pixels
        CheckNotRESULT(CLR_INVALID, crWhite = GetPixel(hdc, 0, 0));
        CheckNotRESULT(CLR_INVALID, crBlack = GetPixel(hdc, 0, 1));

        CheckNotRESULT(FALSE, TransparentBlt(hdc, 0, 1, 1, 1, hdc, 0, 0, 1, 1, crWhite));
        CheckNotRESULT(CLR_INVALID, cr = GetPixel(hdc, 0, 1));
        if(cr != crBlack)
            info(FAIL, TEXT("white incorrectly made opaq"));

        // **** from pixel 0 to pixel 1 with black as transparent

        CheckNotRESULT(-1, SetPixel(hdc, 0, 0, RGB(255, 255, 255)));
        CheckNotRESULT(-1, SetPixel(hdc, 0, 1, RGB(0, 0, 0)));

        // set up our black and white pixels
        CheckNotRESULT(CLR_INVALID, crWhite = GetPixel(hdc, 0, 0));
        CheckNotRESULT(CLR_INVALID, crBlack = GetPixel(hdc, 0, 1));

        CheckNotRESULT(FALSE, TransparentBlt(hdc, 0, 1, 1, 1, hdc, 0, 0, 1, 1, crBlack));
        CheckNotRESULT(CLR_INVALID, cr = GetPixel(hdc, 0, 1));
        if(cr != crWhite)
            info(FAIL, TEXT("white incorrectly made transparent"));

        // **** from pixel 1 to pixel 0 with black as transparent

        CheckNotRESULT(-1, SetPixel(hdc, 0, 0, RGB(255, 255, 255)));
        CheckNotRESULT(-1, SetPixel(hdc, 0, 1, RGB(0, 0, 0)));

        CheckNotRESULT(FALSE, TransparentBlt(hdc, 0, 0, 1, 1, hdc, 0, 1, 1, 1, crBlack));
        CheckNotRESULT(CLR_INVALID, cr = GetPixel(hdc, 0, 0));
        if(cr != crWhite)
            info(FAIL, TEXT("black incorrectly made opaq"));

        // **** from pixel 1 to pixel 0 with white as transparent

        CheckNotRESULT(-1, SetPixel(hdc, 0, 0, RGB(255, 255, 255)));
        CheckNotRESULT(-1, SetPixel(hdc, 0, 1, RGB(0, 0, 0)));

        CheckNotRESULT(FALSE, TransparentBlt(hdc, 0, 0, 1, 1, hdc, 0, 1, 1, 1, crWhite));
        CheckNotRESULT(CLR_INVALID, cr = GetPixel(hdc, 0, 0));
        if(cr != crBlack)
            info(FAIL, TEXT("black incorrectly made transparent"));
    }

    myReleaseDC(hdc);

}

void
TransparentBltBitmapTest()
{
    info(ECHO, TEXT("TransparentBltBitmapTest"));

    TDC hdc = myGetDC();
    TDC hdcCompat;
    TBITMAP tbmp = NULL, tbmpStock = NULL;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));

        drawLogo(hdc, 150);

        for(int i = 0; i < countof(gBitDepths); i++)
        {
#ifndef UNDER_CE
            // xp disallows selection of bitmaps of bit depths other than 1 and the primary
            if(!(gBitDepths[i] == 1 || gBitDepths[i] == GetDeviceCaps(hdcCompat, BITSPIXEL)))
                continue;
#endif

            CheckNotRESULT(NULL, tbmp = myCreateBitmap(zx/2, zy, 1, gBitDepths[i], NULL));
            CheckNotRESULT(NULL, tbmpStock = (TBITMAP) SelectObject(hdcCompat, tbmp));
            CheckNotRESULT(FALSE, BitBlt(hdcCompat, 0, 0, zx/2, zy, hdc, 0, 0, SRCCOPY));

#ifndef UNDER_CE
            // desktop doesn't support a transparent blt from a bitmap to a dc, so just use the dc for this test
            CheckNotRESULT(FALSE, TransparentBlt(hdc, zx/2, 0, zx/2, zy, hdcCompat, 0, 0, zx/2, zy, RGB(0,0,0)));
#endif
            CheckNotRESULT(NULL, SelectObject(hdcCompat, tbmpStock));
#ifdef UNDER_CE
            // ce supports a transparent blt from a bitmap to a dc, so let's hit that code path.
            CheckNotRESULT(FALSE, TransparentBlt(hdc, zx/2, 0, zx/2, zy, tbmp, 0, 0, zx/2, zy, RGB(0,0,0)));
 #endif
            CheckNotRESULT(FALSE, DeleteObject(tbmp));
        }

        CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
    }

    myReleaseDC(hdc);
}

// compares transparent image from a system memory surface to transparent image from a
// video memory surface.
void
TransparentBltSysmemTest()
{
    info(ECHO, TEXT("TransparentBltSysmemTest"));
    TDC hdc = myGetDC();
    TDC hdcCompat1;
    TDC hdcCompat2;
    TBITMAP hbmp1, hbmp1Stock;
    TBITMAP hbmp2, hbmp2Stock;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // a bitmap compatible to a paletted DIB won't
        // behave the same as a bitmap from create bitmap, because they'll have different palettes.
        if(!isPalDIBDC())
        {
            CheckNotRESULT(NULL, hdcCompat1 = CreateCompatibleDC(hdc));
            CheckNotRESULT(NULL, hdcCompat2 = CreateCompatibleDC(hdc));
            CheckNotRESULT(NULL, hbmp1 = CreateCompatibleBitmap(hdc, zx/2, zy));
            CheckNotRESULT(NULL, hbmp1Stock = (TBITMAP) SelectObject(hdcCompat1, hbmp1));
            CheckNotRESULT(NULL, hbmp2 = myCreateBitmap(zx/2, zy, 1, myGetBitDepth(), NULL));
            CheckNotRESULT(NULL, hbmp2Stock = (TBITMAP) SelectObject(hdcCompat2, hbmp2));

            CheckNotRESULT(FALSE, BitBlt(hdcCompat1, 0, 0, zx, zy, hdc, 0, 0, SRCCOPY));
            CheckNotRESULT(FALSE, BitBlt(hdcCompat2, 0, 0, zx, zy, hdc, 0, 0, SRCCOPY));

            FillSurface(hdc); // draw an interesting pattern on the primary's left side
            CheckNotRESULT(FALSE, BitBlt(hdc, zx/2, 0, zx/2, zy, hdc, 0, 0, SRCCOPY)); // copy that pattern to the right side

            drawLogo(hdcCompat1, 150); // draw a windows logo on the video memory surface
            drawLogo(hdcCompat2, 150); // draw a logo on the video memory surface

            CheckNotRESULT(FALSE, TransparentBlt(hdc, 0, 0, zx/2, zy, hdcCompat1, 0, 0, zx/2, zy, GetPixel(hdcCompat1, 0, 0))); // the logo should show up on top of the pattern on the left half of the screen
            CheckNotRESULT(FALSE, TransparentBlt(hdc, zx/2, 0, zx/2, zy, hdcCompat2, 0, 0, zx/2, zy, GetPixel(hdcCompat2, 0, 0))); // the logo should show up on the right half of the screen

            CheckScreenHalves(hdc);

            CheckNotRESULT(NULL, SelectObject(hdcCompat1, hbmp1Stock));
            CheckNotRESULT(NULL, SelectObject(hdcCompat2, hbmp2Stock));
            CheckNotRESULT(FALSE, DeleteObject(hbmp1));
            CheckNotRESULT(FALSE, DeleteObject(hbmp2));
            CheckNotRESULT(FALSE, DeleteDC(hdcCompat1));
            CheckNotRESULT(FALSE, DeleteDC(hdcCompat2));
        }
        else info(DETAIL, TEXT("This test isn't valid on paletted DIB's, skipping a portion of the test."));
    }

    myReleaseDC(hdc);
}

// make sure that the random color selected is made transparent as expected.  It does it by setting
// a destination surface to a random color and then copying from a source with a random color to the destination
// while making the color of the source the transparent color, thus expecting the TransparentBlt to do nothing.
void
TransparentBltTransparencyTest(int testFunc)
{
    info(ECHO, TEXT("TransparentBltTransparencyTest"));
    TDC hdc = myGetDC();
    TDC hdcCompat;
    TBITMAP hBmp, hBmpStock;
    COLORREF crSrc, crDest;
    HBRUSH hbr;
    RECT rcQuarter = { zx/6, zy/3, 2*zx/6, 2*zy/3};
    RECT rcHalf = { 0, 0, zx/2, zy};
    RECT rcWhole = { 0, 0, zx, zy};

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
        for(int i = 0; i < countof(gBitDepths); i++)
        {
            info(DETAIL, TEXT("Testing bit depth %d"), gBitDepths[i]);
            CheckNotRESULT(NULL, hBmp = myCreateBitmap(zx/2, zy, 1, gBitDepths[i], NULL));
            CheckNotRESULT(NULL, hBmpStock = (TBITMAP) SelectObject(hdcCompat, hBmp));

            // Fill the offscreen surface with a random color.
            // paletted surfaces have a possibility of getting a color close enough to black
            // that it's realized as black, which results in failure
            if(gBitDepths[i] <= 8)
                crSrc = RGB(0xFF, 0xFF, 0xFF);
            else
            {
                // get a random color that isn't black.
                do{
                    crSrc = randColorRef();
                }while(crSrc == RGB(0x0, 0x0, 0x0));
            }

            CheckNotRESULT(NULL, hbr = CreateSolidBrush(crSrc));
            CheckNotRESULT(FALSE, FillRect(hdcCompat, &rcHalf, hbr));
            CheckNotRESULT(FALSE, DeleteObject(hbr));

            // fill the primary with a random color.
            // paletted surfaces have a possibility of getting a color close enough to black
            // that it's realized as black, which results in failure
            if(myGetBitDepth() <= 8)
                crDest = RGB(0xFF, 0xFF, 0xFF);
            else
            {
                // get a random color that isn't black.
                do{
                    crDest = randColorRef();
                }while(crDest == RGB(0x0, 0x0, 0x0));

            }

            CheckNotRESULT(NULL, hbr = CreateSolidBrush(crDest));
            CheckNotRESULT(FALSE, FillRect(hdc, &rcWhole, hbr));
            CheckNotRESULT(FALSE, DeleteObject(hbr));

            // put a black box in the center of the offscreen surface.
            CheckNotRESULT(NULL, hbr = CreateSolidBrush(RGB(0x00, 0x00, 0x00)));
            CheckNotRESULT(FALSE, FillRect(hdc, &rcQuarter, hbr));
            CheckNotRESULT(FALSE, FillRect(hdcCompat, &rcQuarter, hbr));
            CheckNotRESULT(FALSE, DeleteObject(hbr));

            CheckNotRESULT(FALSE, TransparentBlt(hdc, zx/2, 0, zx/2, zy, hdcCompat, 0, 0, zx/2, zy, GetPixel(hdcCompat, 0, 0)));

            CheckScreenHalves(hdc);

            CheckNotRESULT(NULL, SelectObject(hdcCompat, hBmpStock));
            CheckNotRESULT(FALSE, DeleteObject(hBmp));
        }

        CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
    }

    myReleaseDC(hdc);
}

// number of entrys in the array of rectangles to use for invert testing
#define INVTESTCOUNT 5

void
SimpleStretchDIBitsTest(int nDirection)
{
    info(ECHO, TEXT("SimpleStretchDIBitsTest"));

    BYTE *lpBits = NULL;

    TDC hdc = myGetDC();
    TDC hdcMem;
    TDC hdcLogo;
    TBITMAP hbmpStock;
    TBITMAP hBmp;
    TBITMAP hbmpLogo;
    TBITMAP hbmpStockForLogo = NULL;
    DWORD dwRop = NOTSRCCOPY;
    int bpp = myGetBitDepth();
    int i, k;
    struct MYBITMAPINFO bmi;

    UINT nType[] = {DIB_RGB_COLORS, DIB_PAL_COLORS};

    // entry 16 is the x coord of the upper left corner used for clearing the dest if needed. (patblt doesn't understand negative widths/heights)
    int dwInvCoords[INVTESTCOUNT][17] = { {    zx,  zy,  -zx/2, -zy,    0,    0,   zx/2,   zy,
                                                0,   0,   zx/2,  zy, zx/2,   zy,  -zx/2,  -zy, 0 },   // flip and mirror, neg dest for stretchblt, neg src for stretchdibits
                                          {  zx/2,   0,   zx/2,  zy, zx/2,   zy,  -zx/2,  -zy,
                                             zx/2,  zy,  -zx/2, -zy,    0,    0,   zx/2,   zy, 0 },   // flip and mirror, neg src for stretchblt, neg dest for stretchdibits
                                          {    zx,  zy,  -zx/2, -zy,    0,    0,   zx/2,   zy,
                                             zx/2,  zy,  -zx/2, -zy,    0,    0,   zx/2,   zy, 0 },   // flip and mirror, neg dest for stretchblt, neg dest for stretchdibits
                                          {  zx/2,  zy,   zx/2, -zy,    0,    0,   zx/2,   zy,
                                                0,  zy,   zx/2, -zy,    0,    0,   zx/2,   zy, 0 },   // mirror, neg dest for stretchblt, neg dest for stretchdibits
                                          {    zx,   0,  -zx/2,  zy,    0,    0,   zx/2,   zy,
                                             zx/2,   0,  -zx/2,  zy,    0,    0,   zx/2,   zy, 0 } }; // flip, neg dest for stretchblt, neg dest for stretchdibits

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hdcMem = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hdcLogo = CreateCompatibleDC(hdc));

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx/2, zy, WHITENESS));
        CheckNotRESULT(NULL, hbmpLogo = CreateCompatibleBitmap(hdc, zx/2, zy));

        CheckNotRESULT(NULL, hbmpStockForLogo = (TBITMAP) SelectObject(hdcLogo, hbmpLogo));
        CheckNotRESULT(FALSE, PatBlt(hdcLogo, 0, 0, zx/2, zy, WHITENESS));
        drawLogo(hdcLogo, 150);

        for(k = 0; k<countof(nType); k++)
        {
            if(bpp != 8 && nType[k] == DIB_PAL_COLORS)
               continue;

            // myCreateDIBSection handles any error conditions.
            // nDirection determines whether it's a top down or bottom up DIB.
            hBmp = myCreateDIBSection(hdc, (void **) &lpBits, bpp, zx, nDirection * zy, nType[k], &bmi, FALSE);

            // Use the memory DC temporarilly to draw something interesting on the bitmap
            CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcMem, hBmp));

            CheckNotRESULT(FALSE, PatBlt(hdcMem, 0, 0, zx, zy, WHITENESS));
            drawLogo(hdcMem, 150);
            CheckNotRESULT(NULL, SelectObject(hdcMem, hbmpStock));

            // step through all of the raster op's
            for(int RopCount = 0; RopCount < countof(gnvRop3Array); RopCount++)
            {
                info(DETAIL,TEXT("Using ROP %s"), gnvRop3Array[RopCount].szName);
                dwRop = gnvRop3Array[RopCount].dwVal;

                // StretchDIBits compared to stretchblt.
                for(i=0; i<INVTESTCOUNT; i++)
                {
                    // copy the logo onto the primary, if we could create the extra bitmap
                    if(hbmpLogo)
                    {
                        CheckNotRESULT(FALSE, BitBlt(hdc, 0, 0, zx/2, zy, hdcLogo, 0, 0, SRCCOPY));
                    }
                    else
                    {
                        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx/2, zy, WHITENESS));
                        drawLogo(hdc, 150);
                    }

                    // make the left and right match, so we can duplicate the results for dest dependant actions.
                    if(!(dwRop == BLACKNESS || dwRop == WHITENESS || dwRop == NOTSRCCOPY || dwRop == PATCOPY || dwRop == SRCCOPY))
                        CheckNotRESULT(FALSE, BitBlt(hdc, zx/2, 0, zx/2, zy, hdc, 0, 0, SRCCOPY));

                    CheckNotRESULT(FALSE, StretchBlt(hdc, dwInvCoords[i][0], dwInvCoords[i][1], dwInvCoords[i][2], dwInvCoords[i][3],
                               hdc, dwInvCoords[i][4], dwInvCoords[i][5], dwInvCoords[i][6], dwInvCoords[i][7], dwRop));

                    // if we're not doing a dest dependant action, then clear the destination.
                    if(dwRop == BLACKNESS || dwRop == WHITENESS || dwRop == NOTSRCCOPY || dwRop == PATCOPY || dwRop == SRCCOPY)
                        CheckNotRESULT(FALSE, PatBlt(hdc, dwInvCoords[i][16], 0, zx/2, zy, WHITENESS));

                    CheckNotRESULT(FALSE, StretchDIBits(hdc, dwInvCoords[i][8],  dwInvCoords[i][9],  dwInvCoords[i][10], dwInvCoords[i][11],
                                               dwInvCoords[i][12], dwInvCoords[i][13], dwInvCoords[i][14], dwInvCoords[i][15], lpBits, (LPBITMAPINFO) &bmi, nType[k], dwRop));

                    // if we're not paletted, the screen will match.
                    // if we are paletted and the primary isn't an 8bpp DIB using DIB_PAL_COLORS, then we'll match.
                    // if the primary used DIB_PAL_COLORS, and the dib we created here used it also, then it will match.
                    // if it's the standard 8bpp bitmap from CreateBitmap, then we'll match.
                    if(bpp > 8 || (isPalDIBDC() && !isSysPalDIBDC()) || nType[k] == DIB_PAL_COLORS)
                        CheckScreenHalves(hdc);
                }

                // Stretching from a DIB section to the primary.
                int x;
                for(x=2; x < zx*2; x+=x)
                {
                    CheckNotRESULT(FALSE, StretchDIBits(hdc, zx/2-x/2 ,zy/2 - x/2, x, x,
                                          RESIZE(20), RESIZE(120), (zx/2) - RESIZE(40), zy - RESIZE(240), lpBits, (LPBITMAPINFO) &bmi, nType[k], dwRop));
                }

                for(x=2; x < zx*2; x+=x)
                {
                    CheckNotRESULT(FALSE, StretchDIBits(hdc, zx/2 + x/2 ,zy/2 - x/2, -x, x,
                                          RESIZE(20), RESIZE(120), (zx/2) - RESIZE(40), zy - RESIZE(240), lpBits, (LPBITMAPINFO) &bmi, nType[k], dwRop));
                }

                for(x=2; x < zx*2; x+=x)
                {
                    CheckNotRESULT(FALSE, StretchDIBits(hdc, zx/2-x/2 ,zy/2 + x/2, x, -x,
                                          RESIZE(20), RESIZE(120), (zx/2) - RESIZE(40), zy - RESIZE(240), lpBits, (LPBITMAPINFO) &bmi, nType[k], dwRop));
                }

                for(x=2; x < zx*2; x+=x)
                {
                    CheckNotRESULT(FALSE, StretchDIBits(hdc, zx/2 + x/2 ,zy/2 + x/2, -x, -x,
                                          RESIZE(20), RESIZE(120), (zx/2) - RESIZE(40), zy - RESIZE(240), lpBits, (LPBITMAPINFO) &bmi, nType[k], dwRop));
                }

            }

            CheckNotRESULT(FALSE, DeleteObject(hBmp));
        }

        CheckNotRESULT(NULL, SelectObject(hdcLogo, hbmpStockForLogo));
        CheckNotRESULT(FALSE, DeleteObject(hbmpLogo));
        CheckNotRESULT(FALSE, DeleteDC(hdcLogo));
        CheckNotRESULT(FALSE, DeleteDC(hdcMem));
    }

    myReleaseDC(hdc);
}

//****************************************************************************************
void
DoRotateBitmap(TDC hdc, TDC hdcSrc, int width, int depth, BOOL bRotate90)
{
    TDC hdcDest      = NULL;
    TBITMAP hbmpDest = NULL;
    TBITMAP hbmpStck = NULL;
    COLORREF cr      = NULL;

    CheckNotRESULT(NULL, hdcDest = CreateCompatibleDC(hdcSrc));
    CheckNotRESULT(NULL, hbmpDest = CreateCompatibleBitmap(hdcDest, depth, width));
    CheckNotRESULT(NULL, hbmpStck = (TBITMAP)SelectObject(hdcDest, hbmpDest));

    for (int y=0; y<depth; y++)
    {
        for (int x=0; x<width; x++)
        {
            // Rotate given bitmap 90 degrees
            if (bRotate90)
            {
                CheckNotRESULT(CLR_INVALID, cr = GetPixel(hdcSrc, x, y));
                CheckNotRESULT(-1, SetPixel(hdcDest, y, width-x-1, cr));
            }
            // Rotate given bitmap 270 degrees
            else
            {
                CheckNotRESULT(CLR_INVALID, cr = GetPixel(hdcSrc, x, y));
                CheckNotRESULT(-1, SetPixel(hdcDest, depth-y-1, x, cr));
            }
        }
    }

    // Blt result to screen
    CheckNotRESULT(FALSE, BitBlt(hdc, 0, 0, depth, width, hdcDest, 0, 0, SRCCOPY));

    // Cleanup
    CheckNotRESULT(NULL,  SelectObject(hdcDest, hbmpStck));
    CheckNotRESULT(FALSE, DeleteObject(hbmpDest));
    CheckNotRESULT(FALSE, DeleteDC(hdcDest));
}

//****************************************************************************************
void
SimpleSetDIBitsToDeviceTest(int nDirection)
{
    info(ECHO, TEXT("SimpleSetDIBitsToDeviceTest"));

    BYTE   *lpBits = NULL;

    TDC hdc = myGetDC();
    TDC hdcMem;
    TBITMAP hbmpMem;
    TBITMAP hbmpStock;
    TBITMAP hBmp;
    struct MYBITMAPINFO bmi;

    int bpp = myGetBitDepth();
    int k;
    UINT nType[] = {DIB_RGB_COLORS, DIB_PAL_COLORS};

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hdcMem = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hbmpMem = CreateCompatibleBitmap(hdc, zx/2, zy));

        for(k =0; k < countof(nType); k++)
        {
            // DIB_PAL_COLORS is only valid on 8bpp, since the system palette is an 8bpp palette.
            if(myGetBitDepth() != 8 && nType[k] == DIB_PAL_COLORS)
                continue;

            hBmp = myCreateDIBSection(hdc, (void **) &lpBits, bpp, zx, nDirection * zy, nType[k], &bmi, FALSE);

            // use the memory DC temporarilly to draw something interesting on the bitmap
            CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcMem, hBmp));
            CheckNotRESULT(FALSE, PatBlt(hdcMem, 0, 0, zx, zy, WHITENESS));
            drawLogo(hdcMem, 150);
            CheckNotRESULT(NULL, SelectObject(hdcMem, hbmpStock));

            // draw something on the primary DC
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            drawLogo(hdc, 150);

            // setup the secondary source surface to hold the unmodified picture to blt to the primary.
            CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcMem, hbmpMem));

            CheckNotRESULT(FALSE, PatBlt(hdcMem, 0, 0, zx, zy, WHITENESS));
            drawLogo(hdcMem, 150);

            if(!g_bRTL)
                CheckNotRESULT(FALSE, SetDIBitsToDevice(hdc, zx/2, 0, zx/2, zy, 0, 0, 0, zy, lpBits, (LPBITMAPINFO) &bmi, nType[k]));
            else
                CheckNotRESULT(FALSE, SetDIBitsToDevice(hdc, zx/2, 0, zx/2, zy, zx/2, 0, 0, zy, lpBits, (LPBITMAPINFO) &bmi, nType[k]));

            // if we're not paletted, the screen will match.
            // if we are paletted and the primary isn't an 8bpp DIB using DIB_PAL_COLORS, then we'll match.
            // if the primary used DIB_PAL_COLORS, and the dib we created here used it also, then it will match.
            // if it's the standard 8bpp bitmap from CreateBitmap, then we'll match.
            if(bpp > 8 || (isPalDIBDC() && !isSysPalDIBDC()) || nType[k] == DIB_PAL_COLORS)
                CheckScreenHalves(hdc);

            int left, top, srcleft, srctop, width, height;
            int nMaxErrors = MAXERRORMESSAGES;

            for(int i=0; i<50 && nMaxErrors > 0; i++)
            {
                // setup the coordinates for the rectangles to copy.
                width  = GenRand() % (zx/2);
                height = GenRand() % (zy);
                left   = GenRand() % ((zx/2) - width);
                top    = GenRand() % (zy - height);

                srcleft = GenRand() % ((zx/2) - width);
                srctop  = GenRand() % (zy - height);

                // blt from the unmodified picture to the primary at position, (left, top)
                CheckNotRESULT(FALSE, BitBlt(hdc, left, top, width, height, hdcMem, srcleft, srctop, SRCCOPY));

                // blt from the dib section to the primary at position, (left + width/2, top)

                // Because we're using a bottom up bitmap, the bottom left corner of the bitmap is actually (zy - (srctop+height)).
                if(!g_bRTL)
                    CheckNotRESULT(FALSE, SetDIBitsToDevice(hdc, left + zx/2, top, width , height, srcleft, zy - (srctop + height), 0, zy, lpBits, (LPBITMAPINFO) &bmi, nType[k]));
                else
                    CheckNotRESULT(FALSE, SetDIBitsToDevice(hdc, left + zx/2, top, width , height, zx - (srcleft + width), zy - (srctop + height), 0, zy, lpBits, (LPBITMAPINFO) &bmi, nType[k]));

                // if we're not paletted, the screen will match.
                // if we are paletted and the primary isn't an 8bpp DIB using DIB_PAL_COLORS, then we'll match.
                // if the primary used DIB_PAL_COLORS, and the dib we created here used it also, then it will match.
                // if it's the standard 8bpp bitmap from CreateBitmap, then we'll match.
                if(bpp > 8 || (isPalDIBDC() && !isSysPalDIBDC()) || nType[k] == DIB_PAL_COLORS)
                    if(!CheckScreenHalves(hdc))
                        nMaxErrors--;
           }

            CheckNotRESULT(FALSE, DeleteObject(hBmp));
            CheckNotRESULT(NULL, SelectObject(hdcMem, hbmpStock));
        }
        CheckNotRESULT(FALSE, DeleteObject(hbmpMem));
        CheckNotRESULT(FALSE, DeleteDC(hdcMem));
    }

    myReleaseDC(hdc);
}

//
// These structures are used to setup the test matrix for the
// BitBltSuite.  BltCall1 contains value and coordinates for
// first Blit and then BltClall2 contains values and coordinates
// needed to create an identical resultant Blit operation but
// with a different set of offsets.  Generally the first blit
// clips based on the Dest window and the second blit clips
// based on the source DC.
//
// RTL_mask_left contains the masking offset for when we are
// in an RTL window.
//

struct BltCall1 {
    int dst_left, dst_top;      // 0-1
    int src_left, src_top;      // 2-3
};

struct BltCall2 {
    int dst_left, dst_top;      // 4-5
    int width, height;          // 6-7
    int src_left, src_top;      // 8-9
    int msk_left, msk_top;      // 10-11
};

struct BltStruct {
    struct BltCall1 Clip1;
    struct BltCall2 Clip2;
    int RTL_msk_left;
};

// this function attempts to be a comprehensive blitting suite of overlapped and clipped operations.
// it tests blt clipping in 8 directions by 1 pixel, 10 pixels, completely off, and 1 pixel left
// it tests overlapped blts that are not clipped in 8 directions
// it tests overlapped blt's that are only overlapped by 1 pixel, which also results in a portion of the blt clipped

void
BitBltSuite(int testFunc)
{
    info(ECHO, TEXT("BitBltSuite - %s"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
            hdcOffscreen;
    TBITMAP hbmp,
            hbmpOld,
            hbmpMask;

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
    // for 1, 2, and 4bpp the pixels are packed, so on non-byte boundaries when images are overlapping,
    // there can be issues as bytes are manipulated, not pixels.  Desktop behaves similarly, thus this is
    // the designed behavior.
    int type = (myGetBitDepth() > 4)?GenRand() % 3:GenRand() % 2;
    // 8x8 is the most common brush, so only use that.
    int brushSize = 8;
    POINT pt;

    // these variables are used in MaskBlt for calculating the mask position.
    int nMaskx = 0, nMasky = 0;
#ifndef UNDER_CE // used for modifying mask position based on rop when running on XP.
    int nForegroundRop = 0, nBackgroundRop = 0;
    BOOL bForeNeedSource = FALSE;
    BOOL bBackNeedSource = FALSE;
#endif

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hdcOffscreen = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hbmp = CreateCompatibleBitmap(hdc, zx/2, zy));

        // set up our mask
        CheckNotRESULT(NULL, hbmpMask = myCreateBitmap(nBlockWidth * 2, nBlockHeight * 2, 1, 1, NULL));
        CheckNotRESULT(NULL, hbmpOld = (TBITMAP) SelectObject(hdcOffscreen, hbmpMask));

        // white on black mask, so the diagonal lines drawn below show up
        CheckNotRESULT(FALSE, PatBlt(hdcOffscreen, 0, 0, nBlockWidth*2, nBlockHeight*2, WHITENESS));
        CheckNotRESULT(FALSE, PatBlt(hdcOffscreen, nBlockWidth/4, nBlockHeight/4, nBlockWidth/2, nBlockHeight/2, BLACKNESS));
        CheckNotRESULT(FALSE, PatBlt(hdcOffscreen, 3* (nBlockWidth/8), 3*(nBlockHeight/8), nBlockWidth/4, nBlockHeight/4, WHITENESS));

        // draw a diagonal stripe across the mask, useful for getting an idea of the locations of the mask being used.
        CheckNotRESULT(FALSE, DrawDiagonals(hdcOffscreen, nBlockWidth, nBlockHeight));

        CheckNotRESULT(FALSE, BitBlt(hdcOffscreen, 0, nBlockHeight, nBlockWidth, nBlockHeight, hdcOffscreen, 0, 0, SRCCOPY));
        CheckNotRESULT(FALSE, BitBlt(hdcOffscreen, nBlockWidth, 0, nBlockWidth, nBlockHeight * 2, hdcOffscreen, 0, 0, SRCCOPY));

        CheckNotRESULT(NULL, SelectObject(hdcOffscreen, hbmpOld));

        CheckNotRESULT(NULL, hbmpOld = (TBITMAP) SelectObject(hdcOffscreen, hbmp));

        FillSurface(hdcOffscreen);
        drawLogo(hdc, 150);
        // make whatever random color the surface was filled with initially the transparent color.
        CheckNotRESULT(FALSE, TransparentBlt(hdcOffscreen, 0, 0, zx/2, zy, hdc, 0, 0, zx/2, zy, GetPixel(hdc, 0, 0)));

        // Matrix for all our clipping operations
        struct BltStruct nLeftBltCoordinates[] = {
                // coordinates that can be tested on all surfaces.
                // test the top clipping plane with the dest
                // 0-5
            {{ nCubex, -10, nCubex, nCubey}, {zx/2 + nCubex, 0, 0, 10, 0, 10, 0, 10}, nBlockWidth}, // clip into the mid top by 10
            {{ nCubex, -1, nCubex, nCubey}, {zx/2 + nCubex, 0, 0, 1, 0, 1, 0, 1}, nBlockWidth }, // clip into the mid top by 1
            {{ nCubex, -nBlockHeight, nCubex, nCubey}, {zx/2 + nCubex, 0, 0, nBlockHeight, 0, nBlockHeight, 0, nBlockHeight}, nBlockWidth}, // completely clip off of the top
            {{ nCubex, -nBlockHeight+1, nCubex, nCubey}, {zx/2 + nCubex, 0, 0, nBlockHeight-1, 0, nBlockHeight-1, 0, nBlockHeight-1}, nBlockWidth}, // 1 pixel from completely off the top
            {{ nCubex, -nBlockHeight - 1, nCubex, nCubey}, {zx/2 + nCubex, 0, 0, nBlockHeight, 0, nBlockHeight + 1, 0, nBlockHeight + 1}, nBlockWidth}, // completely clip off the top with 1 pixel of space
            {{ nCubex, -nBlockHeight - 10, nCubex, nCubey}, {zx/2 + nCubex, 0, 0, nBlockHeight, 0, nBlockHeight + 10, 0, nBlockHeight + 10}, nBlockWidth}, // completely clip off of the top with 10 pixels of space

                // test the top left clipping plane with the dest
                // 6-11
            {{ -10, -10, nCubex, nCubey}, {zx/2 + 0, 0, 10, 10, 10, 10, 10, 10}, nBlockWidth}, // clip into the top left corner by 10
            {{ -1, -1, nCubex, nCubey}, {zx/2 + 0, 0, 1, 1, 1, 1, 1, 1}, nBlockWidth}, // clip into the top left corner by 1
            {{ -nBlockWidth, -nBlockHeight, nCubex, nCubey}, {zx/2 + 0, 0, nBlockWidth, nBlockHeight, nBlockWidth, nBlockHeight, nBlockWidth, nBlockHeight}, nBlockWidth}, // completely off the top left corner
            {{ -nBlockWidth +1, -nBlockHeight +1, nCubex, nCubey}, {zx/2 + 0, 0, nBlockWidth -1, nBlockHeight -1, nBlockWidth -1, nBlockHeight -1, nBlockWidth -1, nBlockHeight -1}, nBlockWidth}, // 1 pixel from completely off the top left corner
            {{ -nBlockWidth -1, -nBlockHeight -1, nCubex, nCubey}, {zx/2 + 0, 0, nBlockWidth +1, nBlockHeight +1, nBlockWidth +1, nBlockHeight +1, nBlockWidth +1, nBlockHeight +1}, nBlockWidth}, // completely off the top left corner with 1 pixel of space
            {{ -nBlockWidth -10, -nBlockHeight -10, nCubex, nCubey}, {zx/2 + 0, 0, nBlockWidth +10, nBlockHeight +10, nBlockWidth +10, nBlockHeight +10, nBlockWidth +10, nBlockHeight +10}, nBlockWidth}, // completely off the top left corner with 10 pixels of space

                // test the left clipping plane with the dest
                // 12-17
            {{ -10, nCubey, nCubex, nCubey}, {zx/2, nCubey, 10, 0, 10, 0, 10, 0}, nBlockWidth}, // clip into the mid left by 10
            {{ -1, nCubey, nCubex, nCubey}, {zx/2, nCubey, 1, 0, 1, 0, 1, 0}, nBlockWidth}, // clip into the mid left by 1
            {{ -nBlockWidth, nCubey, nCubex, nCubey}, {zx/2, nCubey, nBlockWidth, 0, nBlockWidth, 0, nBlockWidth, 0}, nBlockWidth}, // clip completely off the left
            {{ -nBlockWidth +1, nCubey, nCubex, nCubey}, {zx/2, nCubey, nBlockWidth - 1, 0, nBlockWidth - 1, 0, nBlockWidth - 1, 0}, nBlockWidth}, // clip into the mid left by 10
            {{ -nBlockWidth -1, nCubey, nCubex, nCubey}, {zx/2, nCubey, nBlockWidth, 0, nBlockWidth, 0, nBlockWidth, 0}, nBlockWidth}, // clip completely off the left with 1 pixel of space
            {{ -nBlockWidth -10, nCubey, nCubex, nCubey}, {zx/2, nCubey, nBlockWidth, 0, nBlockWidth, 0, nBlockWidth, 0}, nBlockWidth}, // clip completely off the left with 10 pixels of space

                // test the bottom left clipping plane with the dest
                // 18-23
            {{ -10, zy - (nBlockHeight-10), nCubex, nCubey}, {zx/2, zy - (nBlockHeight-10), 10, 10, 10, 0, 10, 0}, nBlockWidth}, // clip into the bottom left corner by 10
            {{ -1, zy - (nBlockHeight-1), nCubex, nCubey}, {zx/2, zy - (nBlockHeight-1), 1, 1, 1, 0, 1, 0}, nBlockWidth}, // clip into the bottom left corner by 1
            {{ -nBlockWidth, zy, nCubex, nCubey}, {zx/2, zy, nBlockWidth, nBlockHeight, nBlockWidth, 0, nBlockWidth, 0}, nBlockWidth}, // completely clip off off the bottom left corner
            {{ -nBlockWidth + 1, zy - 1, nCubex, nCubey}, {zx/2, zy - 1, nBlockWidth - 1, nBlockHeight - 1, nBlockWidth - 1, 0, nBlockWidth - 1, 0}, nBlockWidth}, // almost completely clip off the bottom left corner
            {{ -nBlockWidth - 1, zy + 1, nCubex, nCubey}, {zx/2, zy + 1, nBlockWidth + 1, nBlockHeight + 1, nBlockWidth + 1, 0, nBlockWidth + 1, 0}, nBlockWidth}, // completely clipped off the bottom left corner with 1 pixel of space
            {{ -nBlockWidth - 10, zy + 10, nCubex, nCubey}, {zx/2, zy + 10, nBlockWidth + 10, nBlockHeight + 10, nBlockWidth + 10, 0, nBlockWidth + 10, 0}, nBlockWidth}, // completely clipped off the bottom left corner with 10 pixels of space

                // test the bottom center clipping plane with the dest
                // 24-29
            {{ nCubex, zy - (nBlockHeight-10), nCubex, nCubey}, {zx/2 + nCubex, zy - (nBlockHeight-10), 0, 10, 0, 0, 0, 0}, nBlockWidth}, // clip into the mid bottom by 10
            {{ nCubex, zy - (nBlockHeight-1), nCubex, nCubey}, {zx/2 + nCubex, zy - (nBlockHeight-1), 0, 1, 0, 0, 0, 0}, nBlockWidth}, // clip into the mid bottom by 1
            {{ nCubex, zy, nCubex, nCubey}, {zx/2 + nCubex, zy, 0, nBlockHeight, 0, 0, 0, 0}, nBlockWidth}, // clip completely into the mid bottom
            {{ nCubex, zy-1, nCubex, nCubey}, {zx/2 + nCubex, zy-1, 0, nBlockHeight-1, 0, 0, 0, 0}, nBlockWidth}, // almost completely clip into the bottom
            {{ nCubex, zy+1, nCubex, nCubey}, {zx/2 + nCubex, zy+1, 0, nBlockHeight+1, 0, 0, 0, 0}, nBlockWidth}, // completely clipped with 1 pixel of space.
            {{ nCubex, zy+10, nCubex, nCubey}, {zx/2 + nCubex, zy+10, 0, nBlockHeight+10, 0, 0, 0, 0}, nBlockWidth}, // completely clipped with 10 pixels of space

                // bottom right corner with the dest
                // 30-35
            {{ zx - (nBlockWidth - 10), zy - (nBlockHeight - 10), nCubex, nCubey}, {zx/2 - (nBlockWidth - 10), zy - (nBlockHeight - 10), 10, 10, 0, 0, 0, 0}, 10}, // clip by 10 into the bottom right corner
            {{ zx - (nBlockWidth - 1), zy - (nBlockHeight - 1), nCubex, nCubey}, {zx/2 - (nBlockWidth - 1), zy - (nBlockHeight - 1), 1, 1, 0, 0, 0, 0}, 1}, // clip by 1 into the bottom right corner
            {{ zx, zy, nCubex, nCubey}, {zx/2, zy, nBlockWidth, nBlockHeight, 0, 0, 0, 0}, nBlockWidth}, // completely clip into the bottom right corner
            {{ zx - 1, zy - 1, nCubex, nCubey}, {zx/2 - 1, zy - 1, nBlockWidth-1, nBlockHeight-1, 0, 0, 0, 0}, nBlockWidth-1}, // clip all but 1 pixel into the bottom right corner
            {{ zx + 1, zy + 1, nCubex, nCubey}, {zx/2 + 1, zy + 1, nBlockWidth+1, nBlockHeight+1, 0, 0, 0, 0}, nBlockWidth+1}, // clip all but 1 pixel into the bottom right corner
            {{ zx + 10, zy + 10, nCubex, nCubey}, {zx/2 + 10, zy + 10, nBlockWidth+10, nBlockHeight+10, 0, 0, 0, 0},nBlockWidth+10}, // clip all but 1 pixel into the bottom right corner

                // right middle clipping plane with the dest
                // 36-41
            {{ zx - (nBlockWidth - 10), nCubey, nCubex, nCubey}, {zx/2 - (nBlockWidth - 10), nCubey, 10, 0, 0, 0, 0, 0}, 10}, // clip 10 pixels into the right clipping plane
            {{ zx - (nBlockWidth - 1), nCubey, nCubex, nCubey}, {zx/2 - (nBlockWidth - 1), nCubey, 1, 0, 0, 0, 0, 0}, 1}, // clip 1 pixel into the right
            {{ zx, nCubey, nCubex, nCubey}, {zx/2, nCubey, nBlockWidth, 0, 0, 0, 0, 0}, nBlockWidth}, // completely clip into the right
            {{ zx - 1, nCubey, nCubex, nCubey}, {zx/2 - 1, nCubey, nBlockWidth - 1, 0, 0, 0, 0, 0}, nBlockWidth - 1 }, // clip all but 1 pixel into the right
            {{ zx + 1, nCubey, nCubex, nCubey}, {zx/2, nCubey, nBlockWidth, 0, 0, 0, 0, 0}, nBlockWidth}, // clip all but 1 pixel into the right
            {{ zx + 10, nCubey, nCubex, nCubey}, {zx/2, nCubey, nBlockWidth, 0, 0, 0, 0, 0}, nBlockWidth}, // clip all but 1 pixel into the right

                // top right clipping plane with the dest
                // 42-47
            {{ zx - (nBlockWidth - 10), -10, nCubex, nCubey}, {zx/2 - (nBlockWidth - 10), 0, 10, 10, 0, 10, 0, 10}, 10}, // clip 10 into the top right clipping plane
            {{ zx - (nBlockWidth - 1), -1, nCubex, nCubey}, {zx/2 - (nBlockWidth - 1), 0, 1, 1, 0, 1, 0, 1}, 1}, // clip 1 into the top right clipping plane
            {{ zx, -nBlockHeight, nCubex, nCubey}, {zx/2, 0, nBlockWidth, nBlockHeight, 0, nBlockHeight, 0, nBlockHeight}, nBlockWidth }, // clip completely into the top right clipping plane
            {{ zx - 1, -nBlockHeight + 1, nCubex, nCubey}, {zx/2 - 1, 0, nBlockWidth - 1, nBlockHeight - 1, 0, nBlockHeight - 1, 0, nBlockHeight - 1}, nBlockWidth - 1}, // clip all but 1 pixel into the top right clipping plane
            {{ zx + 1, -nBlockHeight - 1, nCubex, nCubey}, {zx/2 + 1, 0, nBlockWidth + 1, nBlockHeight + 1, 0, nBlockHeight + 1, 0, nBlockHeight + 1}, nBlockWidth + 1}, // clip all but 1 pixel into the top right clipping plane
            {{ zx + 10, -nBlockHeight - 10, nCubex, nCubey}, {zx/2 + 10, 0, nBlockWidth + 10, nBlockHeight + 10, 0, nBlockHeight + 10,  0, nBlockHeight + 10}, nBlockWidth + 10}, // clip all but 1 pixel into the top right clipping plane

                // overlapped blt's by 1 with dest
                // 48-56
            {{ nCubex, nCubey, nCubex, nCubey}, {nCubex + zx/2, nCubey, 0, 0, 0, 0, 0, 0}, 0}, // exactly on top
            {{ nCubex, nCubey-1, nCubex, nCubey}, {nCubex + zx/2, nCubey-1, 0, 0, 0, 0, 0, 0}, 0}, // up
            {{ nCubex + 1, nCubey-1, nCubex, nCubey}, {nCubex + zx/2 + 1, nCubey-1, 0, 0, 0, 0, 0, 0}, 0}, // up right
            {{ nCubex + 1, nCubey, nCubex, nCubey}, {nCubex + zx/2 + 1, nCubey, 0, 0, 0, 0, 0, 0}, 0}, // right
            {{ nCubex + 1, nCubey + 1, nCubex, nCubey}, {nCubex + zx/2 + 1, nCubey + 1, 0, 0, 0, 0, 0, 0}, 0}, // down right
            {{ nCubex, nCubey + 1, nCubex, nCubey}, {nCubex + zx/2, nCubey + 1, 0, 0, 0, 0, 0, 0}, 0}, // down
            {{ nCubex - 1, nCubey + 1, nCubex, nCubey}, {nCubex + zx/2 - 1, nCubey + 1, 0, 0, 0, 0, 0, 0}, 0}, // down left
            {{ nCubex - 1, nCubey, nCubex, nCubey}, {nCubex + zx/2 - 1, nCubey, 0, 0, 0, 0, 0, 0}, 0}, // left
            {{ nCubex-1, nCubey-1, nCubex, nCubey}, {nCubex + zx/2-1, nCubey-1, 0, 0, 0, 0, 0, 0}, 0}, // up left

                // overlapped blt's by 1 with source
                // 57-64
            {{ nCubex, nCubey, nCubex, nCubey - 1}, {nCubex + zx/2, nCubey, 0, 0, 0, -1, 0, 0}, 0}, // up
            {{ nCubex, nCubey, nCubex + 1, nCubey - 1}, {nCubex + zx/2, nCubey, 0, 0, 1, -1, 0, 0}, 0}, // up right
            {{ nCubex, nCubey, nCubex + 1, nCubey}, {nCubex + zx/2, nCubey, 0, 0, 1, 0, 0, 0}, 0}, // right
            {{ nCubex, nCubey, nCubex + 1, nCubey + 1}, {nCubex + zx/2, nCubey, 0, 0, 1, 1, 0, 0}, 0}, // down right
            {{ nCubex, nCubey, nCubex, nCubey + 1}, {nCubex + zx/2, nCubey, 0, 0, 0, 1, 0, 0}, 0}, // down
            {{ nCubex, nCubey, nCubex - 1, nCubey - 1}, {nCubex + zx/2, nCubey, 0, 0, -1, -1, 0, 0}, 0}, // down left
            {{ nCubex, nCubey, nCubex - 1, nCubey}, {nCubex + zx/2, nCubey, 0, 0, -1, 0, 0, 0}, 0}, // left
            {{ nCubex, nCubey, nCubex - 1, nCubey - 1}, {nCubex + zx/2, nCubey, 0, 0, -1, -1, 0, 0}, 0}, // up left

                // 1 pixel/row/column left overlapping
                // 65-72
            {{ nCubex, nCubey-nBlockHeight, nCubex, nCubey}, {nCubex + zx/2, nCubey-nBlockHeight, 0, 0, 0, 0, 0, 0}, 0}, // up
            {{ nCubex + nBlockWidth + zx/2, nCubey-nBlockHeight, nCubex, nCubey}, {nCubex + nBlockWidth, nCubey-nBlockHeight, nBlockWidth - (zx - (nCubex + nBlockWidth + zx/2)), 0, 0, 0, 0, 0}, nBlockWidth - (zx - (nCubex + nBlockWidth + zx/2))}, // up right
            {{ nCubex + nBlockWidth + zx/2, nCubey, nCubex, nCubey}, {nCubex + nBlockWidth, nCubey, nBlockWidth - (zx - (nCubex + nBlockWidth + zx/2)), 0, 0, 0, 0, 0}, nBlockWidth - (zx - (nCubex + nBlockWidth + zx/2))}, // right
            {{ nCubex + nBlockWidth + zx/2, nCubey + nBlockHeight, nCubex, nCubey}, {nCubex + nBlockWidth, nCubey + nBlockHeight, nBlockWidth - (zx - (nCubex + nBlockWidth + zx/2)), 0, 0, 0, 0, 0}, nBlockWidth - (zx - (nCubex + nBlockWidth + zx/2)) }, // down right
            {{ nCubex, nCubey + nBlockHeight, nCubex, nCubey}, {nCubex + zx/2, nCubey + nBlockHeight, 0, 0, 0, 0, 0, 0}, 0}, // down
            {{ nCubex - nBlockWidth, nCubey + nBlockHeight, nCubex, nCubey}, {zx/2, nCubey + nBlockHeight, -(nCubex - nBlockWidth), 0, -(nCubex - nBlockWidth), 0, -(nCubex - nBlockWidth), 0}, nBlockWidth}, // down left
            {{ nCubex - nBlockWidth, nCubey, nCubex, nCubey}, {zx/2, nCubey, -(nCubex - nBlockWidth), 0, -(nCubex - nBlockWidth), 0, -(nCubex - nBlockWidth), 0}, nBlockWidth}, // left
            {{ nCubex - nBlockWidth, nCubey-nBlockHeight, nCubex, nCubey}, {zx/2, nCubey-nBlockHeight, -(nCubex - nBlockWidth), 0, -(nCubex - nBlockWidth), 0, -(nCubex - nBlockWidth), 0}, nBlockWidth}, // up left

                // tests that can only be run on a memory DC.
                // right middle clipping plane with the source
                // 73-78
            {{ nCubex + zx/2, nCubey, zx - (nBlockWidth - 10), nCubey}, {nCubex, nCubey, 0, 0, -nCubex + zx/2 - (nBlockWidth - 10), 0, 0, 0}, nBlockWidth}, // clip 10 pixels into the right clipping plane
            {{ nCubex + zx/2, nCubey, zx - (nBlockWidth - 1), nCubey}, {nCubex, nCubey, 0, 0, -nCubex + zx/2 - (nBlockWidth - 1), 0, 0, 0}, nBlockWidth}, // clip 1 pixel into the right
            {{ nCubex + zx/2, nCubey, zx, nCubey}, {nCubex, nCubey, 0, 0, -nCubex + zx/2, 0, 0, 0}, nBlockWidth}, // completely clip into the right
            {{ nCubex + zx/2, nCubey, zx - 1, nCubey}, {nCubex, nCubey, 0, 0, -nCubex + zx/2 - 1, 0, 0, 0}, nBlockWidth}, // clip all but 1 pixel into the right
            {{ nCubex + zx/2, nCubey, zx + 1, nCubey}, {nCubex, nCubey, 0, 0, -nCubex + zx/2 + 1, 0, 0, 0}, nBlockWidth}, // clip all but 1 pixel into the right
            {{ nCubex + zx/2, nCubey, zx + 10, nCubey}, {nCubex, nCubey, 0, 0, -nCubex + zx/2 + 10, 0, 0, 0}, nBlockWidth}, // clip all but 1 pixel into the right

                // left middle clipping plane with the source
                // 79-84
            {{ nCubex, nCubey, -10, nCubey}, {nCubex + zx/2, nCubey, 0, 0, -nCubex - 10, 0, 0, 0}, nBlockWidth}, // clip into the left by 10
            {{ nCubex, nCubey, -1, nCubey}, {nCubex + zx/2, nCubey, 0, 0, -nCubex - 1, 0, 0, 0}, nBlockWidth}, // clip into the left by 1
            {{ nCubex, nCubey, -nBlockWidth, nCubey}, {nCubex + zx/2, nCubey, 0, 0, -nCubex - nBlockWidth, 0, 0, 0}, nBlockWidth}, // completely clip off the left clipping plane
            {{ nCubex, nCubey, -nBlockWidth + 1, nCubey}, {nCubex + zx/2, nCubey, 0, 0, -nCubex - nBlockWidth + 1, 0, 0, 0}, nBlockWidth}, // clip into the left by 10
            {{ nCubex, nCubey, -nBlockWidth - 1 , nCubey}, {nCubex + zx/2, nCubey, 0, 0, -nCubex - nBlockWidth -1, 0, 0, 0}, nBlockWidth}, // clip completely off the left with 1 pixel of space
            {{ nCubex, nCubey, -nBlockWidth - 10 , nCubey}, {nCubex + zx/2, nCubey, 0, 0, -nCubex - nBlockWidth -10, 0, 0, 0}, nBlockWidth}, // clip completely off the left with 10 pixels of space

                // test the top clipping plane with the source.
                // 85-90
            {{ nCubex, nCubey, nCubex, -10}, {nCubex + zx/2, nCubey, 0, 0, 0, -nCubey - 10, 0, 0}, nBlockWidth}, // clip into the mid top by 10
            {{ nCubex, nCubey, nCubex, -1}, {nCubex + zx/2, nCubey, 0, 0, 0, -nCubey - 1, 0, 0}, nBlockWidth}, // clip into the mid top by 1
            {{ nCubex, nCubey, nCubex, -nBlockHeight}, {nCubex + zx/2, nCubey, 0, 0, 0, -nCubey - nBlockHeight, 0, 0}, nBlockWidth}, // completely clip off of the top
            {{ nCubex, nCubey, nCubex, -nBlockHeight + 1}, {nCubex + zx/2, nCubey, 0, 0, 0, -nCubey - nBlockHeight + 1, 0, 0}, nBlockWidth}, // 1 pixel from completely off the top
            {{ nCubex, nCubey, nCubex, -nBlockHeight - 1}, {nCubex + zx/2, nCubey, 0, 0, 0, -nCubey - nBlockHeight - 1, 0, 0}, nBlockWidth}, // completely clip off of the top with 1 pixel of space
            {{ nCubex, nCubey, nCubex, -nBlockHeight - 10}, {nCubex + zx/2, nCubey, 0, 0, 0, -nCubey - nBlockHeight - 10, 0, 0}, nBlockWidth}, // completely clip off of the top with 10 pixels of space

                // test the top left clipping plane with the source.
                // 91-96
            {{ nCubex, nCubey, -10, -10}, {nCubex + zx/2, nCubey, 0, 0, -nCubex - 10, -nCubey - 10, 0, 0}, nBlockWidth}, // clip into the top left corner by 10
            {{ nCubex, nCubey, -1, -1}, {nCubex + zx/2, nCubey, 0, 0, -nCubex - 1, -nCubey - 1, 0, 0}, nBlockWidth}, // clip into the top left corner by 1
            {{ nCubex, nCubey, -nBlockWidth, -nBlockHeight}, {nCubex + zx/2, nCubey, 0, 0, -nCubex - nBlockWidth, -nCubey - nBlockHeight, 0, 0}, nBlockWidth}, // completely clip off the top left corner
            {{ nCubex, nCubey, -nBlockWidth + 1, -nBlockHeight + 1}, {nCubex + zx/2, nCubey, 0, 0, -nCubex - nBlockWidth + 1, -nCubey - nBlockHeight + 1, 0, 0}, nBlockWidth}, // clip into the top left corner by 10
            {{ nCubex, nCubey, -nBlockWidth - 1 , -nBlockHeight -1}, {nCubex + zx/2, nCubey, 0, 0, -nCubex - nBlockWidth -1, -nCubey - nBlockHeight -1, 0, 0}, nBlockWidth}, // completely clipped off the top left corner with 1 pixel of space
            {{ nCubex, nCubey, -nBlockWidth - 10 , -nBlockHeight -10}, {nCubex + zx/2, nCubey, 0, 0, -nCubex - nBlockWidth -10, -nCubey - nBlockHeight -10, 0, 0}, nBlockWidth}, // completely clipped off the top left corner with 10 pixels of space

                // test the bottom left clipping plane with the source.
                // 97-102
            {{ nCubex, nCubey, -10, zy - (nBlockHeight-10)}, {nCubex + zx/2, nCubey, 0, 0, -nCubex - 10, -nCubey + zy - (nBlockHeight-10), 0, 0}, nBlockWidth}, // clip into the bottom left corner by 10
            {{ nCubex, nCubey, -1, zy - (nBlockHeight-1)}, {nCubex + zx/2, nCubey, 0, 0, -nCubex - 1, -nCubey + zy - (nBlockHeight-1), 0, 0}, nBlockWidth}, // clip into the bottom left corner by 1
            {{ nCubex, nCubey, -nBlockWidth, zy}, {nCubex + zx/2, nCubey, 0, 0, -nCubex - nBlockWidth, -nCubey + zy, 0, 0}, nBlockWidth}, // completely clip off off the bottom left corner
            {{ nCubex, nCubey, -nBlockWidth + 1, zy - 1}, {nCubex + zx/2, nCubey, 0, 0, -nCubex - nBlockWidth + 1, -nCubey + zy - 1, 0, 0}, nBlockWidth}, // almost completely clip off the bottom left corner
            {{ nCubex, nCubey, -nBlockWidth - 1, zy + 1}, {nCubex + zx/2, nCubey, 0, 0, -nCubex - nBlockWidth - 1, -nCubey + zy + 1, 0, 0}, nBlockWidth}, // completely clipped off the bottom left corner with 1 pixel of space
            {{ nCubex, nCubey, -nBlockWidth - 10, zy + 10}, {nCubex + zx/2, nCubey, 0, 0, -nCubex - nBlockWidth - 10, -nCubey + zy + 10, 0}, nBlockWidth}, // completely clipped off the bottom left corner with 1 pixel of space

                // test the bottom center clipping plane with the source.
                // 103-108
            {{ nCubex, nCubey, nCubex, zy - (nBlockHeight-10)}, {nCubex + zx/2, nCubey, 0, 0, 0, -nCubey + zy - (nBlockHeight-10), 0, 0}, nBlockWidth}, // clip into the bottom left corner by 10
            {{ nCubex, nCubey, nCubex, zy - (nBlockHeight-1)}, {nCubex + zx/2, nCubey, 0, 0, 0, -nCubey + zy - (nBlockHeight-1), 0, 0}, nBlockWidth}, // clip into the bottom left corner by 1
            {{ nCubex, nCubey, nCubex, zy}, {nCubex + zx/2, nCubey, 0, 0, 0, -nCubey + zy, 0, 0}, nBlockWidth}, // completely clip off off the bottom left corner
            {{ nCubex, nCubey, nCubex, zy - 1}, {nCubex + zx/2, nCubey, 0, 0, 0, -nCubey + zy - 1, 0, 0}, nBlockWidth}, // almost completely clip off the bottom left corner
            {{ nCubex, nCubey, nCubex, zy + 1}, {nCubex + zx/2, nCubey, 0, 0, 0, -nCubey + zy + 1, 0, 0}, nBlockWidth}, // completely clipped off the bottom left corner with 1 pixel of space
            {{ nCubex, nCubey, nCubex, zy + 10}, {nCubex + zx/2, nCubey, 0, 0, 0, -nCubey + zy + 10, 0, 0}, nBlockWidth}, // completely clipped off the bottom left corner with 1 pixel of space

                // bottom right corner with the source.
                // 109-114
            {{ nCubex + zx/2, nCubey, zx - (nBlockWidth - 10), zy - (nBlockHeight - 10)}, {nCubex, nCubey, 0, 0, -nCubex + zx/2 - (nBlockWidth - 10), -nCubey + zy - (nBlockHeight - 10), 0, 0}, nBlockWidth}, // clip by 10 into the bottom right corner
            {{ nCubex + zx/2, nCubey, zx - (nBlockWidth - 1), zy - (nBlockHeight - 1)}, {nCubex, nCubey, 0, 0, -nCubex + zx/2 - (nBlockWidth - 1), -nCubey + zy - (nBlockHeight - 1), 0, 0}, nBlockWidth}, // clip by 1 into the bottom right corner
            {{ nCubex + zx/2, nCubey, zx, zy}, {nCubex, nCubey, 0, 0, -nCubex + zx/2, -nCubey + zy, 0, 0}, nBlockWidth}, // completely clip into the bottom right corner
            {{ nCubex + zx/2, nCubey, zx - 1, zy - 1}, {nCubex, nCubey, 0, 0, -nCubex + zx/2 -1, -nCubey + zy - 1, 0, 0}, nBlockWidth}, // clip all but 1 pixel into the bottom right corner
            {{ nCubex + zx/2, nCubey, zx + 1, zy + 1}, {nCubex, nCubey, 0, 0, -nCubex + zx/2 + 1, -nCubey + zx + 1, 0, 0}, nBlockWidth}, // clip all but 1 pixel into the bottom right corner
            {{ nCubex + zx/2, nCubey, zx + 10, zy + 10}, {nCubex, nCubey, 0, 0, -nCubex + zx/2 + 10, -nCubex + zy + 10, 0, 0}, nBlockWidth}, // clip all but 1 pixel into the bottom right corner

                // top right clipping plane with the source
                // 115-120
            {{ nCubex + zx/2, nCubey, zx - (nBlockWidth - 10), -10}, {nCubex, nCubey, 0, 0, -nCubex + zx/2 - (nBlockWidth - 10), -nCubey - 10, 0, 0}, nBlockWidth}, // clip 10 into the top right clipping plane
            {{ nCubex + zx/2, nCubey, zx - (nBlockWidth - 1), -1}, {nCubex, nCubey, 0, 0, -nCubex + zx/2 - (nBlockWidth - 1), -nCubey - 1, 0, 0}, nBlockWidth}, // clip 1 into the top right clipping plane
            {{ nCubex + zx/2, nCubey, zx, -nBlockHeight}, {nCubex, nCubey, 0, 0, -nCubex + zx/2, -nCubey - nBlockHeight,  0, 0}, nBlockWidth}, // clip completely into the top right clipping plane
            {{ nCubex + zx/2, nCubey, zx - 1, -nBlockHeight + 1}, {nCubex, nCubey, 0, 0, -nCubex + zx/2 -1, -nCubey - nBlockHeight + 1, 0, 0}, nBlockWidth}, // clip all but 1 pixel into the top right clipping plane
            {{ nCubex + zx/2, nCubey, zx + 1, -nBlockHeight - 1}, {nCubex, nCubey, 0, 0, -nCubex + zx/2 + 1, -nCubey - nBlockHeight - 1,0, 0}, nBlockWidth}, // clip all but 1 pixel into the top right clipping plane
            {{ nCubex + zx/2, nCubey, zx + 10, -nBlockHeight - 10}, {nCubex, nCubey, 0, 0, -nCubex + zx/2 + 10, -nCubey - nBlockHeight - 10, 0, 0}, nBlockWidth}, // clip all but 1 pixel into the top right clipping plane
                };

        // there are entries in the array that pull data from all possible clipping planes, which can result in using a portion of the task bar in a Blt,
        // which will cause a failure.  These cases are only valid on a Memory DC which does not contain a task bar.
        // 73 for entries 0 to 72, which are valid on all surfaces.  73-120 are only valid on memory DC's.
        int nEntries = isMemDC() ? countof(nLeftBltCoordinates) : 73;
        info(DETAIL, TEXT("Using %d testcases"), nEntries);

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
                CheckNotRESULT(NULL, hBrush = CreateSolidBrush(randColorRef()));
                break;
            case 2:
                depth = gBitDepths[GenRand() % countof(gBitDepths)];
                info(DETAIL, TEXT("Using a DIB w/RGB, d:%d, s:%d"), depth, brushSize);
                CheckNotRESULT(NULL, hBrush = setupBrush(hdc, type, depth, brushSize, FALSE, TRUE));
                break;
        }

        if(hBrush)
        {
            CheckNotRESULT(FALSE, SetBrushOrgEx(hdc, 0, 0, NULL));
            CheckNotRESULT(FALSE, SetBrushOrgEx(hdcOffscreen, 0, 0, NULL));
            CheckNotRESULT(NULL, hBrushStock1 = (HBRUSH) SelectObject(hdc, hBrush));
            CheckNotRESULT(NULL, hBrushStock2 = (HBRUSH) SelectObject(hdcOffscreen, hBrush));
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
                CheckNotRESULT(FALSE, BitBlt(hdc, 0, 0, zx/2, zy, hdcOffscreen, 0, 0, SRCCOPY));
                CheckNotRESULT(FALSE, BitBlt(hdc, zx/2, 0, zx/2, zy, hdc, 0, 0, SRCCOPY));

                pt.x = 0;
                pt.y = 0;
                // if our destination is on the right half, then the brush origin is in the center top,
                // otherwise it's in the top left.
                if(nLeftBltCoordinates[i].Clip1.dst_left >= zx/2)
                    pt.x = zx/2;

                if(hBrush)
                {
                    CheckNotRESULT(FALSE, SetBrushOrgEx(hdc, pt.x, pt.y, NULL));
                    // reselect the brush to set the new origin in effect.
                    CheckNotRESULT(NULL, SelectObject(hdc, hBrush));
                }

                switch(testFunc)
                {
                    case EBitBlt:
                        // always blt the same sized block for comparing to the other side
                        CheckNotRESULT(FALSE, BitBlt(hdc, nLeftBltCoordinates[i].Clip1.dst_left, nLeftBltCoordinates[i].Clip1.dst_top, nBlockWidth, nBlockHeight, hdc, nLeftBltCoordinates[i].Clip1.src_left, nLeftBltCoordinates[i].Clip1.src_top, dwRop));
                        break;
                    case EStretchBlt:
                        CheckNotRESULT(FALSE, StretchBlt(hdc, nLeftBltCoordinates[i].Clip1.dst_left, nLeftBltCoordinates[i].Clip1.dst_top, nBlockWidth, nBlockHeight, hdc, nLeftBltCoordinates[i].Clip1.src_left, nLeftBltCoordinates[i].Clip1.src_top, nBlockWidth, nBlockHeight, dwRop));
                        break;
                    case EMaskBlt:
                        CheckNotRESULT(FALSE, MaskBlt(hdc, nLeftBltCoordinates[i].Clip1.dst_left, nLeftBltCoordinates[i].Clip1.dst_top, nBlockWidth, nBlockHeight, hdc, nLeftBltCoordinates[i].Clip1.src_left, nLeftBltCoordinates[i].Clip1.src_top, hbmpMask, 0, 0, dwRop));
                        break;
                    case ETransparentImage:
                        // make black transparent, so only the center of the flag shows.
                        CheckNotRESULT(FALSE, TransparentBlt(hdc, nLeftBltCoordinates[i].Clip1.dst_left, nLeftBltCoordinates[i].Clip1.dst_top, nBlockWidth, nBlockHeight, hdc, nLeftBltCoordinates[i].Clip1.src_left, nLeftBltCoordinates[i].Clip1.src_top, nBlockWidth, nBlockHeight, RGB(0,0,0)));
                        break;
                }

                pt.x = 0;
                pt.y = 0;
                // if our destination is on the right half, then the brush origin is in the center top,
                // otherwise it's in the top left.
                if(nLeftBltCoordinates[i].Clip2.dst_left >= zx/2)
                    pt.x = zx/2;

                if(hBrush)
                {
                    CheckNotRESULT(FALSE, SetBrushOrgEx(hdc, pt.x, pt.y, NULL));

                    // reselect the brush to set the new origin in effect.
                    CheckNotRESULT(NULL, SelectObject(hdc, hBrush));
                }

                switch(testFunc)
                {
                    case ETransparentImage:
                        CheckNotRESULT(FALSE, TransparentBlt(hdc, nLeftBltCoordinates[i].Clip2.dst_left, nLeftBltCoordinates[i].Clip2.dst_top, nBlockWidth - nLeftBltCoordinates[i].Clip2.width, nBlockHeight - nLeftBltCoordinates[i].Clip2.height,
                                                    hdcOffscreen, nCubex + nLeftBltCoordinates[i].Clip2.src_left, nCubey + nLeftBltCoordinates[i].Clip2.src_top, nBlockWidth - nLeftBltCoordinates[i].Clip2.width, nBlockHeight - nLeftBltCoordinates[i].Clip2.height, RGB(0,0,0)));
                        break;
                    case EMaskBlt:
                        nMaskx = nLeftBltCoordinates[i].Clip2.msk_left;
                        nMasky = nLeftBltCoordinates[i].Clip2.msk_top;
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
                            nMaskx = 2*nLeftBltCoordinates[i].Clip2.msk_left;
                            nMasky = 2*nLeftBltCoordinates[i].Clip2.msk_top;
                        }
    #endif
                        if(!g_bRTL)
                            nMaskx = nLeftBltCoordinates[i].Clip2.msk_left;
                        else
                            nMaskx = nLeftBltCoordinates[i].RTL_msk_left;

                        CheckNotRESULT(FALSE, MaskBlt(hdc, nLeftBltCoordinates[i].Clip2.dst_left, nLeftBltCoordinates[i].Clip2.dst_top, nBlockWidth - nLeftBltCoordinates[i].Clip2.width, nBlockHeight - nLeftBltCoordinates[i].Clip2.height,
                                                hdcOffscreen, nCubex + nLeftBltCoordinates[i].Clip2.src_left, nCubey + nLeftBltCoordinates[i].Clip2.src_top, hbmpMask, nMaskx, nMasky, dwRop));
                        break;
                    default:
                        // grab the same block from the offscreen surface and put it on the other side
                        CheckNotRESULT(FALSE, BitBlt(hdc, nLeftBltCoordinates[i].Clip2.dst_left, nLeftBltCoordinates[i].Clip2.dst_top, nBlockWidth - nLeftBltCoordinates[i].Clip2.width, nBlockHeight - nLeftBltCoordinates[i].Clip2.height,
                                                                                hdcOffscreen, nCubex + nLeftBltCoordinates[i].Clip2.src_left, nCubey + nLeftBltCoordinates[i].Clip2.src_top, dwRop));
                        break;
                }

                if(!CheckScreenHalves(hdc))
                {
                        if(testFunc == ETransparentImage)
                            info(FAIL,TEXT("Failure on Transparent Image test %d"), i);
                        else if(testFunc == EMaskBlt)
                            info(FAIL,TEXT("Failure on test %d, using ROP %s"), i, gnvRop4Array[RopCount].szName);
                        else
                            info(FAIL, TEXT("Failure on test %d, rop %s"), i, gnvRop3Array[RopCount].szName);
                }
                if(!bResult)
                {
                    if(testFunc == ETransparentImage)
                        info(FAIL,TEXT("A call failed for test %d"), i);
                    else if(testFunc == EMaskBlt)
                        info(FAIL,TEXT("A Blt call failed for test %d, using ROP4 %s"), i, gnvRop4Array[RopCount].szName);
                    else
                        info(FAIL, TEXT("A Blt call failed for test %d, ROP3 %s"), i, gnvRop3Array[RopCount].szName);
                }
            }
        }

        if(hBrush)
        {
            CheckNotRESULT(NULL, SelectObject(hdc, hBrushStock1));
            CheckNotRESULT(NULL, SelectObject(hdcOffscreen, hBrushStock2));
            CheckNotRESULT(FALSE, DeleteObject(hBrush));
        }

        CheckNotRESULT(NULL, SelectObject(hdcOffscreen, hbmpOld));
        CheckNotRESULT(FALSE, DeleteObject(hbmp));
        CheckNotRESULT(FALSE, DeleteObject(hbmpMask));
        CheckNotRESULT(FALSE, DeleteDC(hdcOffscreen));
    }

    myReleaseDC(hdc);
}


// this is a stretching version of the bitBlt suite.  Since most drivers will be intelligent and use the same code path for a StretchBlt as a
// BitBlt if there isn't any stretching, we need to do the overlapped blt's in the situation where there is some stretching.
// This suite behaves similar to the BitBlt suite execept it stretches the blt's by a random small amount on both sides.
// because of that stretching and the resulting clipping that will be needed, it needs a completely different data set to handle
// the blt's to both sides.

void
StretchBltSuite(int testFunc, int nMode)
{
    info(ECHO, TEXT("StretchBltSuite - %s"), funcName[testFunc]);
    TDC hdc = myGetDC(),
          hdcOffscreen;
    TBITMAP hbmp,
                 hbmpOld;
    int nOldMode;
    // block size to blt
    int nBlockWidth = zx/4;
    int nBlockHeight = zy/2;
    // location to pull the cube from the left side
    int nCubex = zx/8;
    int nCubey = zy/4;
    int nSrcStretchx, nSrcStretchy, nDstStretchx, nDstStretchy;
    DWORD dwRop = 0;
    HBRUSH hBrush = NULL, hBrushStock1 = NULL, hBrushStock2 = NULL;
    int depth = 0;
    // no brush for now.
    int type = GenRand() % 3;
    // 8x8 is the most common brush size.
    int brushSize = 8;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(0, nOldMode = SetStretchBltMode(hdc, nMode));
        CheckNotRESULT(NULL, hdcOffscreen = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hbmp = CreateCompatibleBitmap(hdc, zx/2, zy)) ;

        // setup the offscreen source for the non-overlapped blt's
        CheckNotRESULT(NULL, hbmpOld = (TBITMAP) SelectObject(hdcOffscreen, hbmp));

        FillSurface(hdcOffscreen);
        drawLogo(hdc, 150);
        // make whatever random color the surface was filled with the transparent color.
        CheckNotRESULT(FALSE, TransparentBlt(hdcOffscreen, 0, 0, zx/2, zy, hdc, 0, 0, zx/2, zy, GetPixel(hdc, 0, 0)));

        // the source stretch must be negative (smaller than the available space) to keep from grabbing uninitialized data from the screen.
        nSrcStretchx = (GenRand() % zx/4) - zx/8;
        nSrcStretchy = (GenRand() % zy/4) - zy/8;
        nDstStretchx = (GenRand() % zx/4) - zx/8;
        nDstStretchy = (GenRand() % zy/4) - zy/8;

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
                CheckNotRESULT(NULL, hBrush = CreateSolidBrush(randColorRef()));
                break;
            case 2:
                depth = gBitDepths[GenRand() % countof(gBitDepths)];
                info(DETAIL, TEXT("Using a DIB w/RGB, d:%d, s:%d"), depth, brushSize);
                CheckNotRESULT(NULL, hBrush = setupBrush(hdc, type, depth, brushSize, FALSE, TRUE));
                break;
        }

        if(hBrush)
        {
            CheckNotRESULT(FALSE, SetBrushOrgEx(hdc, 0, 0, NULL));
            CheckNotRESULT(FALSE, SetBrushOrgEx(hdcOffscreen, 0, 0, NULL));
            CheckNotRESULT(NULL, hBrushStock1 = (HBRUSH) SelectObject(hdc, hBrush));
            CheckNotRESULT(NULL, hBrushStock2 = (HBRUSH) SelectObject(hdcOffscreen, hBrush));
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
                CheckNotRESULT(FALSE, BitBlt(hdc, 0, 0, zx/2, zy, hdcOffscreen, 0, 0, SRCCOPY));
                CheckNotRESULT(FALSE, BitBlt(hdc, zx/2, 0, zx/2, zy, hdc, 0, 0, SRCCOPY));

                if(hBrush)
                {
                    CheckNotRESULT(FALSE, SetBrushOrgEx(hdc, 0, 0, NULL));

                    // reselect the brush to set the new origin in effect.
                    CheckNotRESULT(NULL, SelectObject(hdc, hBrush));
                }

                switch(testFunc)
                {
                    case EStretchBlt:
                        if(hBrush)
                            CheckNotRESULT(NULL, SelectObject(hdc, hBrush));
                        CheckNotRESULT(FALSE, StretchBlt(hdc, nLeftBltCoordinates[i][0], nLeftBltCoordinates[i][1], nBlockWidth + nLeftBltCoordinates[i][2], nBlockHeight + nLeftBltCoordinates[i][3],
                                                        hdc, nCubex, nCubey, nBlockWidth + nLeftBltCoordinates[i][4], nBlockHeight + nLeftBltCoordinates[i][5], dwRop));
                        break;
                    case ETransparentImage:
                         // make black transparent, so only the center of the flag shows.
                        CheckNotRESULT(FALSE, TransparentBlt(hdc, nLeftBltCoordinates[i][0], nLeftBltCoordinates[i][1], nBlockWidth + nLeftBltCoordinates[i][2], nBlockHeight + nLeftBltCoordinates[i][3],
                                                               hdc, nCubex, nCubey, nBlockWidth + nLeftBltCoordinates[i][4], nBlockHeight + nLeftBltCoordinates[i][5], RGB(0,0,0)));
                        break;
                }

                if(hBrush)
                {
                    CheckNotRESULT(FALSE, SetBrushOrgEx(hdc, zx/2, 0, NULL));

                    // reselect the brush to set the new origin in effect.
                    CheckNotRESULT(NULL, SelectObject(hdc, hBrush));
                }

                switch(testFunc)
                {
                    case EStretchBlt:
                        CheckNotRESULT(FALSE, StretchBlt(hdc, nLeftBltCoordinates[i][0] + zx/2, nLeftBltCoordinates[i][1], nBlockWidth + nLeftBltCoordinates[i][2], nBlockHeight + nLeftBltCoordinates[i][3],
                                                        hdcOffscreen, nCubex, nCubey, nBlockWidth + nLeftBltCoordinates[i][4], nBlockHeight + nLeftBltCoordinates[i][5], dwRop));
                        break;
                    case ETransparentImage:
                        CheckNotRESULT(FALSE, TransparentBlt(hdc, nLeftBltCoordinates[i][0] + zx/2, nLeftBltCoordinates[i][1], nBlockWidth + nLeftBltCoordinates[i][2], nBlockHeight + nLeftBltCoordinates[i][3],
                                                               hdcOffscreen, nCubex, nCubey, nBlockWidth + nLeftBltCoordinates[i][4], nBlockHeight + nLeftBltCoordinates[i][5], RGB(0,0,0)));
                        break;
                }

                if(!CheckScreenHalves(hdc))
                {
                        if(testFunc == ETransparentImage)
                            info(DETAIL,TEXT("Failure on Transparent Image test %d"), i);
                        else
                            info(FAIL, TEXT("Failure on test %d, rop %s"), i, gnvRop3Array[RopCount].szName);
                }
            }
        }

        CheckNotRESULT(0, SetStretchBltMode(hdc, nOldMode));

        if(hBrush)
        {
            CheckNotRESULT(NULL, SelectObject(hdc, hBrushStock1));
            CheckNotRESULT(NULL, SelectObject(hdcOffscreen, hBrushStock2));
            CheckNotRESULT(FALSE, DeleteObject(hBrush));
        }
        CheckNotRESULT(NULL, SelectObject(hdcOffscreen, hbmpOld));
        CheckNotRESULT(FALSE, DeleteObject(hbmp));
        CheckNotRESULT(FALSE, DeleteDC(hdcOffscreen));
    }

    myReleaseDC(hdc);
}

// bilinear stretch verification test
void
BilinearAndHalftoneStretchTest(int testFunc, int nMode)
{
    info(ECHO, TEXT("BilinearAndHalftoneStretchTest - %s"), funcName[testFunc]);
    NOPRINTERDCV(NULL);

    int nMonitors = GetSystemMetrics(SM_CMONITORS);

    // bilinear and halftone stretching only works on > 8bpp, and is not available on Multimon.
    if(myGetBitDepth() > 8 && nMonitors == 1)
    {
        TDC hdc = myGetDC();
        int nOldMode;
        int nGrayPixels = 0;
        COLORREF cr;

        if (!IsWritableHDC(hdc))
            info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
        else
        {
            nOldMode = SetStretchBltMode(hdc, nMode);

            switch (nMode)
            {
            case BILINEAR:
                CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
                CheckNotRESULT(FALSE, PatBlt(hdc, zx/4, zy/4, 2*(zx/4), 2*(zy/4), BLACKNESS));
                CheckNotRESULT(FALSE, StretchBlt(hdc, 20, 20, zx-40, zy-40, hdc, zx/4-10, zy/4-10, 2*(zx/4)+20, 2*(zy/4)+20, SRCCOPY));
                break;

            case HALFTONE:
                CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
                CheckNotRESULT(FALSE, PatBlt(hdc, 25, 20, zx-50, zy-40, BLACKNESS));
                CheckNotRESULT(FALSE, PatBlt(hdc, 20, 25, zx-40, zy-50, BLACKNESS));
                CheckNotRESULT(FALSE, PatBlt(hdc, zx/4-10, zy/4-10, 2*(zx/4)+20, 2*(zy/4)+20, WHITENESS));
                CheckNotRESULT(FALSE, StretchBlt(hdc, zx/4, zy/4, 2*(zx/4), 2*(zy/4), hdc, 0, 0, zx, zy, SRCCOPY));
                break;

            default:
                info(FAIL, TEXT("Invalid SetStretchBltMode sent to BilinearAndHalftoneStretchTest."));
                break;
            }

            for(int x = 0; x < zx; x++)
                for(int y =0; y < zy; y++)
                {
                    cr = GetPixel(hdc, x, y);
                    if(cr != RGB(0,0,0) && cr != RGB(255, 255, 255))
                    {
                            nGrayPixels++;
                    }
                }

// desktop doesn't support bilinear stretching.
#ifdef UNDER_CE
            if(nGrayPixels == 0)
            {
                if (nMode == BILINEAR)
                    info(FAIL, TEXT("Expected gray pixels for bilinear stretch, no gray pixels found."));
                else
                    info(FAIL, TEXT("Expected gray pixels for halftone stretch, no gray pixels found."));
                OutputBitmap(hdc, zx, zy);
            }
#endif
        }

        myReleaseDC(hdc);
    }
}

/* this test does a fullscreen overlapped blt in 8 directions, using all of the rops in gnvRop3Array.
    This test does not do any form of verification */
// TODO: write a function which takes two dc's and does a comparision, so this function has a form of verification.
void
BitBltSuiteFullscreen(int testFunc)
{
    info(ECHO, TEXT("BitBltSuiteFullscreen - %s"), funcName[testFunc]);

    TDC hdc = myGetDC(),
          hdcOffscreen;
    TBITMAP hbmp,
                 hbmpOld,
                 hbmpMask;
    DWORD dwRop = 0;

    BOOL result = FALSE;
    int nSrcStretchx, nSrcStretchy, nDstStretchx, nDstStretchy;
    HBRUSH hBrush = NULL, hBrushStock = NULL;
    int depth = 0;
    int type = GenRand() % 3;
    int brushSize = 8;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hdcOffscreen = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hbmp = CreateCompatibleBitmap(hdc, zx/2, zy));
        // set up our mask
        CheckNotRESULT(NULL, hbmpMask = myCreateBitmap(zx, zy, 1, 1, NULL));
        CheckNotRESULT(NULL, hbmpOld = (TBITMAP) SelectObject(hdcOffscreen, hbmpMask));

        // fill the mask with something interesting
        CheckNotRESULT(FALSE, PatBlt(hdcOffscreen, 0, 0, zx, zy, WHITENESS));
        // draw a diagonal stripe across the mask, useful for getting an idea of the locations of the mask being used.
        DrawDiagonals(hdcOffscreen, zx, zy);
        CheckNotRESULT(NULL, SelectObject(hdcOffscreen, hbmpOld));

        CheckNotRESULT(NULL, hbmpOld = (TBITMAP) SelectObject(hdcOffscreen, hbmp));

        FillSurface(hdcOffscreen);
        drawLogo(hdc, 150);
        // make whatever random color the surface was filled with the transparent color.
        CheckNotRESULT(FALSE, TransparentBlt(hdcOffscreen, 0, 0, zx/2, zy, hdc, 0, 0, zx/2, zy, GetPixel(hdc, 0, 0)));

        // the source stretch must be negative (smaller than the available space) to keep from grabbing uninitialized data from the screen.
        nSrcStretchx = -(GenRand() % zx/4);
        nSrcStretchy = -(GenRand() % zy/4);
        nDstStretchx = (GenRand() % zx/4) - zx/8;
        nDstStretchy = (GenRand() % zy/4) - zy/8;

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
                CheckNotRESULT(NULL, hBrush = CreateSolidBrush(randColorRef()));
                break;
            case 2:
                depth = gBitDepths[GenRand() % countof(gBitDepths)];
                info(DETAIL, TEXT("Using a DIB w/RGB, d:%d, s:%d"), depth, brushSize);
                CheckNotRESULT(NULL, hBrush = setupBrush(hdc, DibRGB, depth, brushSize, FALSE, TRUE));
                break;
        }

        if(hBrush)
        {
            CheckNotRESULT(FALSE, SetBrushOrgEx(hdc, 0, 0, NULL));
            CheckNotRESULT(NULL, hBrushStock = (HBRUSH) SelectObject(hdc, hBrush));
        }
        // step through all of the raster op's, or only go once for TransparentBlt because it doesn't do ROP's.
        // Go through the Rop4 array for MaskBlt, rop3's for everything else.
        for(int RopCount = 0; RopCount < nTestCount; RopCount++)
        {
            for(int i=0; i < nEntries; i++)
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

                CheckNotRESULT(FALSE, BitBlt(hdc, 0, 0, zx/2, zy, hdcOffscreen, 0, 0, SRCCOPY));
                CheckNotRESULT(FALSE, BitBlt(hdc, zx/2, 0, zx/2, zy, hdc, 0, 0, SRCCOPY));

                switch(testFunc)
                {
                    case EBitBlt:
                        // always blt the same sized block for comparing to the other side
                        CheckNotRESULT(FALSE, result = BitBlt(hdc, nLeftBltCoordinates[i][0], nLeftBltCoordinates[i][1], zx, zy, hdc, 0, 0, dwRop));
                        break;
                    case EStretchBlt:
                        CheckNotRESULT(FALSE, result = StretchBlt(hdc, nLeftBltCoordinates[i][0], nLeftBltCoordinates[i][1], zx + nLeftBltCoordinates[i][2], zy + nLeftBltCoordinates[i][3],
                                                        hdc, 0, 0, zx + nLeftBltCoordinates[i][4], zy + nLeftBltCoordinates[i][5], dwRop));
                        break;
                    case EMaskBlt:
                        CheckNotRESULT(FALSE, result = MaskBlt(hdc, nLeftBltCoordinates[i][0], nLeftBltCoordinates[i][1], zx, zy, hdc, 0, 0, hbmpMask, 0, 0, dwRop));
                        break;
                    case ETransparentImage:
                        // grab the color of a random pixel on the screen, and make it transparent
                        CheckNotRESULT(FALSE, result = TransparentBlt(hdc, nLeftBltCoordinates[i][0], nLeftBltCoordinates[i][1], zx + nLeftBltCoordinates[i][2], zy + nLeftBltCoordinates[i][3],
                                                           hdc, 0, 0, zx + nLeftBltCoordinates[i][4], zy + nLeftBltCoordinates[i][5], GetPixel(hdc, GenRand() % zx, GenRand() % zy)));
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

        if(hBrush)
        {
            CheckNotRESULT(NULL, SelectObject(hdc, hBrushStock));
            CheckNotRESULT(FALSE, DeleteObject(hBrush));
        }
        CheckNotRESULT(NULL, SelectObject(hdcOffscreen, hbmpOld));
        CheckNotRESULT(FALSE, DeleteObject(hbmp));
        CheckNotRESULT(FALSE, DeleteObject(hbmpMask));
        CheckNotRESULT(FALSE, DeleteDC(hdcOffscreen));
    }

    myReleaseDC(hdc);
}

void
BltFromMonochromeTest(int testFunc, int testMethod)
{
    info(ECHO, TEXT("%s - BltFromMonochromeTest"), funcName[testFunc]);

    TDC hdc = myGetDC();
    TDC hdcCompat;
    TDC hdcMonoCompat;
    TBITMAP hbmpMono = NULL, hbmpOffscreen = NULL;
    TBITMAP hbmpMonoStock = NULL, hbmpOffscreenStock = NULL;
    TBITMAP hbmpMask;
    COLORREF FgColor = randColorRef(), BkColor = randColorRef();
    COLORREF FgColorStock, BkColorStock;
    HBRUSH hbrFgColor, hbrBkColor;
    RECT rc = {0};
    BYTE *lpBits;
    BLENDFUNCTION bf;

    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 0xFF;
    bf.AlphaFormat = 0;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hdcMonoCompat = CreateCompatibleDC(hdc));

        // the DIB will be created with this palette, so look for these colors in the conversion.
        // if the bit depth is less than 8, then random foreground/background colors may not be
        // available, so stick to black and white.
        if(testMethod == ECreateDIBSection || myGetBitDepth() < 8)
        {
           FgColor = RGB(0,0,0);
           BkColor = RGB(0xFF,0xFF,0xFF);
        }

        // if we're testing a DIB, foreground/background text setting should have no effect.
        if(testMethod != ECreateDIBSection)
        {
            FgColorStock = SetTextColor(hdc, FgColor);
            BkColorStock = SetBkColor(hdc, BkColor);
        }
        else
        {
            FgColorStock = SetTextColor(hdc, randColorRef());
            BkColorStock = SetBkColor(hdc, randColorRef());
        }

        CheckNotRESULT(NULL, hbrFgColor = CreateSolidBrush(FgColor));
        CheckNotRESULT(NULL, hbrBkColor = CreateSolidBrush(BkColor));


        if(hdc && hdcCompat && hdcMonoCompat && hbrFgColor && hbrBkColor && FgColorStock != CLR_INVALID && BkColorStock != CLR_INVALID)
        {
            CheckNotRESULT(NULL, hbmpOffscreen = CreateCompatibleBitmap(hdc, zx/2, zy));
            CheckNotRESULT(NULL, hbmpMask = myCreateBitmap(zx/2, zy, 1, 1, NULL));

            // test with both a 1bpp monochrome from myCreateBitmap, and also by
            // creating a 1bpp monochrome using the 1bpp monochrome attached to
            // a compatible dc by default.
            switch(testMethod)
            {
                case ECreateBitmap:
                    info(DETAIL, TEXT("Using a bitmap from CreateBitmap"));
                    CheckNotRESULT(NULL, hbmpMono = myCreateBitmap(zx/2, zy, 1, 1, NULL));
                    break;
                case ECreateCompatibleBitmap:
                    info(DETAIL, TEXT("Using a bitmap from CreateCompatibleBitmap"));
                    CheckNotRESULT(NULL, hbmpMono = CreateCompatibleBitmap(hdcMonoCompat, zx/2, zy));
                    break;
                case ECreateDIBSection:
                    info(DETAIL, TEXT("Using a bitmap from CreateDIBSection"));
                    CheckNotRESULT(NULL, hbmpMono = myCreateDIBSection(hdc, (VOID **) &lpBits, 1, zx/2, zy, DIB_RGB_COLORS, NULL, FALSE));
                    break;
            }

            if(hbmpOffscreen && hbmpMono && hbmpMask)
            {
                CheckNotRESULT(NULL, hbmpOffscreenStock = (TBITMAP) SelectObject(hdcCompat, hbmpOffscreen));
                CheckNotRESULT(NULL, hbmpMonoStock = (TBITMAP) SelectObject(hdcMonoCompat, hbmpMask));

                // draw diagonals on the mask, useful for identifying locations
                CheckNotRESULT(FALSE, PatBlt(hdcMonoCompat, 0, 0, zx/2, zy, WHITENESS));
                CheckNotRESULT(FALSE, DrawDiagonals(hdcMonoCompat, zx/2, zy));

                // select in our mono bitmap for testing.  the stock bitmap was saved when we selected in the mask for initialization.
                CheckNotRESULT(NULL, SelectObject(hdcMonoCompat, hbmpMono));

                // initialize the test bitmap, this initialization must match the shape drawn on the color surface.
                CheckNotRESULT(FALSE, PatBlt(hdcMonoCompat, 0, 0, zx/2, zy, BLACKNESS));
                CheckNotRESULT(FALSE, PatBlt(hdcMonoCompat, zx/8, zy/4, zx/4, zy/2, WHITENESS));

                // fill the foreground color into the color dc
                SetRect(&rc, 0, 0, zx/2, zy);
                CheckNotRESULT(FALSE, FillRect(hdcCompat, &rc, hbrFgColor));
                // use the same calculation for the coordinates as used in the PatBlts, to avoid rounding problems.
                SetRect(&rc, zx/8, zy/4, (zx/8 + zx/4), (zy/4 + zy/2));
                CheckNotRESULT(FALSE, FillRect(hdcCompat, &rc, hbrBkColor));

                switch(testFunc)
                {
                    case EBitBlt:
                        CheckNotRESULT(FALSE, BitBlt(hdc, 0, 0, zx/2, zy, hdcMonoCompat, 0, 0, SRCCOPY));
                        CheckNotRESULT(FALSE, BitBlt(hdc, zx/2, 0, zx/2, zy, hdcCompat, 0, 0, SRCCOPY));
                        break;
                    case EStretchBlt:
                        CheckNotRESULT(FALSE, StretchBlt(hdc, 0, 0, zx/2, zy, hdcMonoCompat, 0, 0, zx/2 - 1, zy - 1, SRCCOPY));
                        CheckNotRESULT(FALSE, StretchBlt(hdc, zx/2, 0, zx/2, zy, hdcCompat, 0, 0, zx/2 - 1, zy - 1, SRCCOPY));
                        break;
                    case EMaskBlt:
                        CheckNotRESULT(FALSE, MaskBlt(hdc, 0, 0, zx/2, zy, hdcMonoCompat, 0, 0, hbmpMask, 0, 0, MAKEROP4(SRCCOPY, BLACKNESS)));
                        CheckNotRESULT(FALSE, MaskBlt(hdc, zx/2, 0, zx/2, zy, hdcCompat, 0, 0, hbmpMask, 0, 0, MAKEROP4(SRCCOPY, BLACKNESS)));
                        break;
                    case EAlphaBlend:
                        CheckNotRESULT(FALSE, gpfnAlphaBlend(hdc, 0, 0, zx/2, zy, hdcMonoCompat, 0, 0, zx/2 - 1, zy - 1, bf));
                        CheckNotRESULT(FALSE, gpfnAlphaBlend(hdc, zx/2, 0, zx/2, zy, hdcCompat, 0, 0, zx/2 - 1, zy - 1, bf));
                        break;

                }

                if(!CheckScreenHalves(hdc))
                    info(FAIL, TEXT("Blt from a monochrome bitmap to the primary didn't behave correctly.  bkColor:%d fgColor: %d"), BkColor, FgColor);

                CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpOffscreenStock));
                CheckNotRESULT(NULL, SelectObject(hdcMonoCompat, hbmpMonoStock));
                CheckNotRESULT(FALSE, DeleteObject(hbmpOffscreen));
                CheckNotRESULT(FALSE, DeleteObject(hbmpMono));
                CheckNotRESULT(FALSE, DeleteObject(hbmpMask));
            }
            else info(ABORT, TEXT("Failed to create necessary bitmaps for testing"));
        }
        else info(ABORT, TEXT("Failed to create necessary DC's and brushes for testing or failed to set foreground/background colors for testing"));

        SetTextColor(hdc, FgColorStock);
        SetBkColor(hdc, BkColorStock);
        CheckNotRESULT(FALSE, DeleteObject(hbrFgColor));
        CheckNotRESULT(FALSE, DeleteObject(hbrBkColor));
        CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
        CheckNotRESULT(FALSE, DeleteDC(hdcMonoCompat));
    }
    myReleaseDC(hdc);
}

void
BltToMonochromeTest(int testFunc, int testMethod)
{
    info(ECHO, TEXT("%s - BltToMonochromeTest"), funcName[testFunc]);

    TDC hdc = myGetDC();
    TDC hdcMonoCompat;
    TBITMAP hbmpMono = NULL,
                hbmpMask;
    TBITMAP hbmpMonoStock = NULL;
    COLORREF FgColor = randColorRef(), BkColor = randColorRef();
    COLORREF FgColorStock, BkColorStock;
    HBRUSH hbrFgColor, hbrBkColor;
    RECT rc;
    BYTE *lpBits = NULL;
    BLENDFUNCTION bf;

    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 0xFF;
    bf.AlphaFormat = 0;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hdcMonoCompat = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hbmpMask = myCreateBitmap(zx/2, zy, 1, 1, NULL));

        // the DIB will be created with this palette, so look for these colors in the conversion.
        // if the bit depth is less than 8, then random foreground/background colors may not be
        // available, so stick to black and white.
        if(testMethod == ECreateDIBSection || myGetBitDepth() < 8)
        {
           FgColor = RGB(0,0,0);
           BkColor = RGB(0xFF,0xFF,0xFF);
        }

        // if we're testing a DIB, foreground/background text setting should have no effect.
        if(testMethod != ECreateDIBSection)
        {
            CheckNotRESULT(CLR_INVALID, FgColorStock = SetTextColor(hdc, FgColor));
            CheckNotRESULT(CLR_INVALID, BkColorStock = SetBkColor(hdc, BkColor));
        }
        else
        {
            CheckNotRESULT(CLR_INVALID, FgColorStock = SetTextColor(hdc, randColorRef()));
            CheckNotRESULT(CLR_INVALID, BkColorStock = SetBkColor(hdc, randColorRef()));
        }

        CheckNotRESULT(NULL, hbrFgColor = CreateSolidBrush(FgColor));
        CheckNotRESULT(NULL, hbrBkColor = CreateSolidBrush(BkColor));


        if(hdc && hdcMonoCompat && hbmpMask)
        {
            CheckNotRESULT(NULL, hbmpMonoStock = (TBITMAP) SelectObject(hdcMonoCompat, hbmpMask));
            CheckNotRESULT(FALSE, PatBlt(hdcMonoCompat, 0, 0, zx/2, zy, WHITENESS));
            DrawDiagonals(hdcMonoCompat, zx/2, zy);

            // test with both a 1bpp monochrome from myCreateBitmap, and also by
            // creating a 1bpp monochrome using the 1bpp monochrome attached to
            // a compatible dc by default.
            switch(testMethod)
            {
                case ECreateBitmap:
                    info(DETAIL, TEXT("Using a bitmap from CreateBitmap"));
                    CheckNotRESULT(NULL, hbmpMono = myCreateBitmap(zx/2, zy, 1, 1, NULL));
                    break;
                case ECreateCompatibleBitmap:
                    info(DETAIL, TEXT("Using a bitmap from CreateCompatibleBitmap"));
                    CheckNotRESULT(NULL, hbmpMono = CreateCompatibleBitmap(hdcMonoCompat, zx/2, zy));
                    break;
                case ECreateDIBSection:
                    info(DETAIL, TEXT("Using a bitmap from CreateDIBSection"));
                    CheckNotRESULT(NULL, hbmpMono = myCreateDIBSection(hdc, (VOID **) &lpBits, 1, zx/2, zy, DIB_RGB_COLORS, NULL, FALSE));
                    break;
            }

            if(hbmpMono)
            {
                CheckNotRESULT(NULL, SelectObject(hdcMonoCompat, hbmpMono));

                SetRect(&rc, 0, 0, zx/2, zy);
                CheckNotRESULT(FALSE, FillRect(hdc, &rc, hbrFgColor));
                SetRect(&rc, zx/8, zy/4, 3*zx/8, 3*zy/4);
                CheckNotRESULT(FALSE, FillRect(hdc, &rc, hbrBkColor));

                CheckNotRESULT(CLR_INVALID, BkColorStock = SetBkColor(hdc, BkColor));
                CheckNotRESULT(FALSE, BitBlt(hdcMonoCompat, 0, 0, zx/2, zy, hdc, 0, 0, SRCCOPY));
                CheckNotRESULT(CLR_INVALID, FgColorStock = SetTextColor(hdc, FgColor));

                switch(testFunc)
                {
                    case EBitBlt:
                        CheckNotRESULT(FALSE, BitBlt(hdc, zx/2, 0, zx/2, zy, hdcMonoCompat, 0, 0, SRCCOPY));
                        break;
                    case EMaskBlt:
                        CheckNotRESULT(FALSE, MaskBlt(hdc, 0, 0, zx/2, zy, hdc, 0, 0, hbmpMask, 0, 0, MAKEROP4(SRCCOPY, BLACKNESS)));
                        CheckNotRESULT(FALSE, MaskBlt(hdc, zx/2, 0, zx/2, zy, hdcMonoCompat, 0, 0, hbmpMask, 0, 0, MAKEROP4(SRCCOPY, BLACKNESS)));
                        break;
                    case EStretchBlt:
                        CheckNotRESULT(FALSE, StretchBlt(hdc, 0, 0, zx/2, zy, hdc, 0, 0, zx/2 - 1, zy - 1, SRCCOPY));
                        CheckNotRESULT(FALSE, StretchBlt(hdc, zx/2, 0, zx/2, zy, hdcMonoCompat, 0, 0, zx/2 - 1, zy - 1, SRCCOPY));
                        break;
                    case EAlphaBlend:
                        CheckNotRESULT(FALSE, gpfnAlphaBlend(hdc, zx/2, 0, zx/2, zy, hdcMonoCompat, 0, 0, zx/2, zy, bf));
                        break;
                }
                CheckNotRESULT(CLR_INVALID, SetTextColor(hdc, FgColorStock));
                CheckNotRESULT(CLR_INVALID, SetBkColor(hdc, BkColorStock));

                if(!CheckScreenHalves(hdc))
                    info(FAIL, TEXT("Blt to a monochrome bitmap from the primary didn't behave properly"));

                CheckNotRESULT(NULL, SelectObject(hdcMonoCompat, hbmpMonoStock));
                CheckNotRESULT(FALSE, DeleteObject(hbmpMono));
            }
            else info(ABORT, TEXT("Creation of a 1bpp bitmap failed."));

            CheckNotRESULT(FALSE, DeleteObject(hbmpMask));
            CheckNotRESULT(FALSE, DeleteObject(hbrFgColor));
            CheckNotRESULT(FALSE, DeleteObject(hbrBkColor));
            CheckNotRESULT(FALSE, DeleteDC(hdcMonoCompat));
        }
        else info(ABORT, TEXT("Creation of DC's required for testing failed."));
    }

    myReleaseDC(hdc);
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
    COLORREF crExpected;
    int nOldMode;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // verify that the bkmode has no effect.
        CheckNotRESULT(0, nOldMode = SetBkMode(hdc, GenRand() % 2?TRANSPARENT:OPAQUE));

        // 2bpp gradient fill's aren't supported.
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

            // get the nearest color to what the gradient fill is filling to.
            CheckNotRESULT(CLR_INVALID, crExpected = GetNearestColor(hdc, RGB(0xFF, 0x00, 0x00)));

            for(int i = 0; i < 2; i++)
            {
                CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

                if(i % 2)
                    operation = GRADIENT_FILL_RECT_H;
                else operation = GRADIENT_FILL_RECT_V;

                CheckNotRESULT(FALSE, gpfnGradientFill(hdc,vert,2,gRect,1, operation));

                // don't verify if running on a printer dc.
                if(!isPrinterDC(hdc))
                {
                    CheckNotRESULT(CLR_INVALID, clr = GetPixel(hdc, vert[0].x, vert[0].y));
                    dwRColor = GetRValue(clr);
                    dwGColor = GetGValue(clr);
                    dwBColor = GetBValue(clr);
                    if(myGetBitDepth() > 8)
                    {
                        if(dwRColor != 0xFF || dwGColor != 0x00 || dwBColor != 0x00)
                            info(FAIL, TEXT("Expected R:0xFF G:0x00 B:0x00 at (%d,%d) got R:0x%x G:0x%x B:0x%x"),
                                            vert[0].x, vert[0].y, dwRColor, dwGColor, dwBColor);
                    }
                    else if(clr != crExpected)
                        info(FAIL, TEXT("Expected R:0x%x G:0x%x B:0x%x at (%d,%d) got R:0x%x G:0x%x B:0x%x"),
                                        GetRValue(crExpected), GetGValue(crExpected), GetBValue(crExpected), vert[0].x, vert[0].y, dwRColor, dwGColor, dwBColor);

                    CheckNotRESULT(CLR_INVALID, clr = GetPixel(hdc, vert[1].x - 1, vert[1].y - 1));
                    dwRColor = GetRValue(clr);
                    dwGColor = GetGValue(clr);
                    dwBColor = GetBValue(clr);

                    if(myGetBitDepth() > 8)
                    {
                        if(dwRColor != 0xFF || dwGColor != 0x00 || dwBColor != 0x00)
                            info(FAIL, TEXT("Expected R:0xFF G:0x00 B:0x00 at (%d,%d) got R:0x%x G:0x%x B:0x%x"),
                                            vert[1].x - 1, vert[1].y - 1, dwRColor, dwGColor, dwBColor);
                    }
                    else if(clr != crExpected)
                        info(FAIL, TEXT("Expected R:0x%x G:0x%x B:0x%x at (%d,%d) got R:0x%x G:0x%x B:0x%x"),
                                        GetRValue(crExpected), GetGValue(crExpected), GetBValue(crExpected), vert[1].x - 1, vert[1].y - 1, dwRColor, dwGColor, dwBColor);

                    CheckNotRESULT(CLR_INVALID, clr = GetPixel(hdc, (vert[1].x - vert[0].x)/2, (vert[1].y - vert[0].y)/2));
                    dwRColor = GetRValue(clr);
                    dwGColor = GetGValue(clr);
                    dwBColor = GetBValue(clr);
                    if(myGetBitDepth() > 8)
                    {
                        if(dwRColor != 0xFF || dwGColor != 0x00 || dwBColor != 0x00)
                            info(FAIL, TEXT("Expected R:0xFF G:0x00 B:0x00 at (%d,%d) got R:0x%x G:0x%x B:0x%x"),
                                        (vert[1].x - vert[0].x)/2, (vert[1].y - vert[0].y)/2, dwRColor, dwGColor, dwBColor);
                    }
                    else if(clr != crExpected)
                        info(FAIL, TEXT("Expected R:0x%x G:0x%x B:0x%x at (%d,%d) got R:0x%x G:0x%x B:0x%x"),
                                        GetRValue(crExpected), GetGValue(crExpected), GetBValue(crExpected), (vert[1].x - vert[0].x)/2, (vert[1].y - vert[0].y)/2, dwRColor, dwGColor, dwBColor);
                }
            }
        }

        CheckNotRESULT(0, SetBkMode(hdc, nOldMode));
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
    COLORREF crExpected;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // 2bpp gradient fills aren't supported.
        if(myGetBitDepth() != 2  && (gdwRedBitMask | gdwGreenBitMask | gdwBlueBitMask) != 0x00000fff)
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
                CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

                if(i % 2)
                    operation = GRADIENT_FILL_RECT_V;
                else operation = GRADIENT_FILL_RECT_H;

                info(DETAIL, TEXT("Gradient fill (%d, %d) to (%d, %d) op %d"), vert[0].x, vert[0].y, vert[1].x, vert[1].y, operation);
                CheckNotRESULT(FALSE, gpfnGradientFill(hdc,vert,2,gRect,1, operation));

                // don't verify the colors if running on a printer dc.
                if(!isPrinterDC(hdc))
                {
                    if(operation == GRADIENT_FILL_RECT_H)
                    {
                        CheckNotRESULT(CLR_INVALID, clr = GetPixel(hdc, vert[0].x, (vert[1].y - vert[0].y) / 2));
                        dwRColor = GetRValue(clr);
                        dwGColor = GetGValue(clr);
                        dwBColor = GetBValue(clr);

                        if(myGetBitDepth() > 8)
                        {
                            // make sure it's red, allow some flexability because of dithering
                            if(dwRColor < 0xF0 || dwGColor > 0x08 || dwBColor > 0x08)
                            {
                                info(FAIL, TEXT("Expected R:0x%x G:0x00 B:0x00 at (%d,%d) got R:0x%x G:0x%x B:0x%x"), 0xFF, vert[0].x, (vert[1].y - vert[0].y) / 2, dwRColor, dwGColor, dwBColor);
                                OutputBitmap(hdc, zx, zy);
                            }
                        }
                        else
                        {
                            CheckNotRESULT(CLR_INVALID, crExpected = GetNearestColor(hdc, RGB(vert[0].Red >> 8,vert[0].Green >> 8,vert[0].Blue >> 8)));
                            if(crExpected != clr)
                            {
                                info(FAIL, TEXT("Expected R:0x%x G:0x%x B:0x%x on surface at (%d,%d) got R:0x%x G:0x%x B:0x%x"),
                                                    GetRValue(crExpected), GetRValue(crExpected), GetRValue(crExpected), vert[0].x, (vert[1].y - vert[0].y) / 2, dwRColor, dwGColor, dwBColor);
                                OutputBitmap(hdc, zx, zy);
                            }
                        }

                        CheckNotRESULT(CLR_INVALID, clr = GetPixel(hdc, vert[1].x - 1, (vert[1].y - vert[0].y) / 2));
                        dwRColor = GetRValue(clr);
                        dwGColor = GetGValue(clr);
                        dwBColor = GetBValue(clr);

                        if(myGetBitDepth() > 8)
                        {
                            // make sure it's blue, allow some flexability because of dithering
                            if(dwRColor > 0x08 || dwGColor > 0x08 || dwBColor < 0xF0)
                            {
                                info(FAIL, TEXT("Expected R:0x00 G:0x00 B:0x%x at (%d,%d) got R:0x%x G:0x%x B:0x%x"), 0xFF, vert[1].x - 1, (vert[1].y - vert[0].y) / 2, dwRColor, dwGColor, dwBColor);
                                OutputBitmap(hdc, zx, zy);
                            }
                        }
                        // due to dithering when the gradient fill get's to the other side, it's not possible to verify the color on the ending edge
                        // when paletted.
                        /*
                        else
                        {
                            crExpected = GetNearestColor(hdc, RGB(vert[1].Red >> 8,vert[1].Green >> 8,vert[1].Blue >> 8));
                            if(crExpected != clr)
                            {
                                info(FAIL, TEXT("Expected R:0x%x G:0x%x B:0x%x on surface at (%d,%d) got R:0x%x G:0x%x B:0x%x"),
                                                    GetRValue(crExpected), GetRValue(crExpected), GetRValue(crExpected), vert[1].x - 1, (vert[1].y - vert[0].y) / 2, dwRColor, dwGColor, dwBColor);
                                OutputBitmap(hdc, zx, zy);
                            }
                        }
                        */
                    }
                    // vertical gradient fill
                    else
                    {
                        CheckNotRESULT(CLR_INVALID, clr = GetPixel(hdc, (vert[1].x - vert[0].x) / 2, vert[0].y));
                        dwRColor = GetRValue(clr);
                        dwGColor = GetGValue(clr);
                        dwBColor = GetBValue(clr);

                        if(myGetBitDepth() > 8)
                        {
                            // make sure it's red, allow some flexability because of dithering
                            if(dwRColor < 0xF0 || dwGColor > 0x08 || dwBColor > 0x08)
                            {
                                info(FAIL, TEXT("Expected R:0x%x G:0x00 B:0x00 at (%d,%d) got R:0x%x G:0x%x B:0x%x"), 0xFF, (vert[1].x - vert[0].x) / 2, vert[0].y, dwRColor, dwGColor, dwBColor);
                                OutputBitmap(hdc, zx, zy);
                            }
                        }
                        else
                        {
                            CheckNotRESULT(CLR_INVALID, crExpected = GetNearestColor(hdc, RGB(vert[0].Red >> 8,vert[0].Green >> 8,vert[0].Blue >> 8)));
                            if(crExpected != clr)
                            {
                                info(FAIL, TEXT("Expected R:0x%x G:0x%x B:0x%x on surface at (%d,%d) got R:0x%x G:0x%x B:0x%x"),
                                                    GetRValue(crExpected), GetRValue(crExpected), GetRValue(crExpected), (vert[1].x - vert[0].x) / 2, vert[0].y, dwRColor, dwGColor, dwBColor);
                                OutputBitmap(hdc, zx, zy);
                            }
                        }

                        CheckNotRESULT(CLR_INVALID, clr = GetPixel(hdc, (vert[1].x - vert[0].x) / 2, vert[1].y - 1));
                        dwRColor = GetRValue(clr);
                        dwGColor = GetGValue(clr);
                        dwBColor = GetBValue(clr);
                        if(myGetBitDepth() > 8)
                        {
                            // make sure it's blue, allow some flexability because of dithering
                            if(dwRColor > 0x08 || dwGColor > 0x08 || dwBColor < 0xF0)
                            {
                                info(FAIL, TEXT("Expected R:0x00 G:0x00 B:0x%x at (%d,%d) got R:0x%x G:0x%x B:0x%x"), 0xFF, (vert[1].x - vert[0].x) / 2, vert[1].y - 1, dwRColor, dwGColor, dwBColor);
                                OutputBitmap(hdc, zx, zy);
                            }
                        }
                        // due to dithering when the gradient fill get's to the other side, it's not possible to verify the color on the ending edge
                        // when paletted.
                        /*
                        else
                        {
                            crExpected = GetNearestColor(hdc, RGB(vert[1].Red >> 8,vert[1].Green >> 8,vert[1].Blue >> 8));
                            if(crExpected != clr)
                            {
                                info(FAIL, TEXT("Expected R:0x%x G:0x%x B:0x%x on surface at (%d,%d) got R:0x%x G:0x%x B:0x%x"),
                                                    GetRValue(crExpected), GetRValue(crExpected), GetRValue(crExpected), (vert[1].x - vert[0].x) / 2, vert[1].y - 1, dwRColor, dwGColor, dwBColor);
                                OutputBitmap(hdc, zx, zy);
                            }
                        }
                        */
                    }
                }
            }
        }
        else info(DETAIL, TEXT("Test not applicable to RGB4444 or 2bpp surfaces"));
    }

    myReleaseDC(hdc);
}

BOOL
AlphaBlendSupported()
{
    BOOL bOldVerify = SetSurfVerify(FALSE);
    TDC hdcPrimary = myGetDC(NULL);
    TDC hdcCompat = CreateCompatibleDC(hdcPrimary);
    BLENDFUNCTION bf;
    BOOL bRval = TRUE;

    memset(&bf, 0, sizeof(bf));
    bf.SourceConstantAlpha = 0x55;

    SetLastError(ERROR_SUCCESS);

    // if the call fails and AlphaBlend isn't supported in the display driver, then
    // report that it's not supported.  Otherwise it is supported and should be tested.
    if(!gpfnAlphaBlend(hdcCompat, 0, 0, 5, 5, hdcCompat, 10, 10, 5, 5, bf))
        if(ERROR_NOT_SUPPORTED == GetLastError())
            bRval = FALSE;

    // if the call succeeds, then we can test AlphaBlend.
    DeleteDC(hdcCompat);
    myReleaseDC(NULL, hdcPrimary);
    SetSurfVerify(bOldVerify);
    return bRval;
}

void
SimpleGradientFillAlphaTest()
{
    info(ECHO, TEXT("SimpleGradientFillAlphaTest"));

    NOPRINTERDCV(NULL);

    if(myGetBitDepth() > 2)
    {
        TDC hdcPrimary = myGetDC();

        TDC hdc;
        TBITMAP hbmp, hbmpStock;
        TRIVERTEX        vert[2];
        GRADIENT_RECT    gRect[1];
        ULONG operation;
        COLORREF clr;
        DWORD *pdwBits;
        DWORD dwRColor, dwGColor, dwBColor, dwAColor;
        int startofline;
        DWORD dwExpAlpha;        // Expected Alpha result

        BLENDFUNCTION bf;

        bf.BlendOp = AC_SRC_OVER;
        bf.AlphaFormat = AC_SRC_ALPHA;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = 0xFF;

        if (!IsWritableHDC(hdcPrimary))
            info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
        else
        {
            CheckNotRESULT(NULL, hdc = CreateCompatibleDC(hdcPrimary));
            // use a top down DIB, so the coordinates are relative to the top of the pointer array.
            CheckNotRESULT(NULL, hbmp = myCreateDIBSection(hdcPrimary, (VOID **) &pdwBits, 32, zx, -zy, DIB_RGB_COLORS, NULL, FALSE));
            CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdc, hbmp));
            // match the background color of the primary and the test bitmap
            CheckNotRESULT(FALSE, BitBlt(hdc, 0, 0, zx, zy, hdcPrimary, 0, 0, SRCCOPY));

            gRect[0].UpperLeft = 0;
            gRect[0].LowerRight = 1;

            // From red to red, but with an alpha of FF to 0.
            vert[0].x      = zx/4;
            vert[0].y      = zy/4;
            vert[0].Red    = 0xFF00;
            vert[0].Green  = 0x0000;
            vert[0].Blue   = 0x0000;
            vert[0].Alpha  = 0xFF00;

            vert[1].x      = 3*zx/4;
            vert[1].y      = 3*zy/4;
            vert[1].Red    = 0xFF00;
            vert[1].Green  = 0x0000;
            vert[1].Blue   = 0x0000;
            vert[1].Alpha  = 0x0000;

            for(int i = 0; i < 2; i++)
            {
                if(i % 2)
                    operation = GRADIENT_FILL_RECT_V;
                else
                    operation = GRADIENT_FILL_RECT_H;

                info(DETAIL, TEXT("Gradient fill (%d, %d) to (%d, %d)"), vert[0].x, vert[0].y, vert[1].x, vert[1].y, operation);
                CheckNotRESULT(FALSE, gpfnGradientFill(hdc,vert,2,gRect,1, operation));

                if(gpfnAlphaBlend && AlphaBlendSupported())
                {
                    CheckNotRESULT(FALSE, gpfnAlphaBlend(hdcPrimary, 0, 0, zx, zy, hdc, 0, 0, zx, zy, bf));
                }

                if(operation == GRADIENT_FILL_RECT_H)
                {
                    info(DETAIL, TEXT("Horizontal"));

                    // each pixel takes a DWORD, so ZX is the number of DWORD's per scanline.
                    if(!g_bRTL)
                    {   // get 1st pixel of middle row
                        startofline = zx * ((vert[1].y - vert[0].y) / 2 + vert[0].y);
                        clr = pdwBits[startofline + vert[0].x];
                        dwExpAlpha = 0xFF;
                    }
                    else
                    {
                        startofline = zx * ((vert[1].y - vert[0].y) / 2 + vert[0].y);
                        startofline += zx - 1;
                        clr = pdwBits[startofline - vert[0].x];
                        dwExpAlpha = 0xFC;
                    }
                    clr = SwapRGB(clr);

                    dwRColor = GetRValue(clr);
                    dwGColor = GetGValue(clr);
                    dwBColor = GetBValue(clr);
                    dwAColor = GetAValue(clr);

                    if(g_bRTL)
                        info(DETAIL, TEXT("RTL end fill 0x%x"), dwAColor);

                    if(dwRColor < 0xFF || dwAColor < dwExpAlpha || dwGColor > 0|| dwBColor > 0)
                    {
                        info(FAIL, TEXT("Expected A:0x%x R:0xFF G:0x00 B:0x00 Actual A:0x%x R:0x%x G:0x%x B:0x%x"), dwExpAlpha, dwAColor, dwRColor, dwGColor, dwBColor);
                        OutputBitmap(hdc, zx, zy);
                    }

                    if(!g_bRTL)
                    {   // get last pixel of middle row
                        startofline = zx * ((vert[1].y - vert[0].y) / 2 + vert[0].y);
                        clr = pdwBits[startofline + vert[1].x - 1];
                        dwExpAlpha = 0x02;
                    }
                    else
                    {
                        startofline = zx * (1 + (vert[1].y - vert[0].y) / 2 + vert[0].y) - 1;
                        clr = pdwBits[startofline - vert[1].x + 1];
                        dwExpAlpha = 0x00;
                    }
                    clr = SwapRGB(clr);
                    dwRColor = GetRValue(clr);
                    dwGColor = GetGValue(clr);
                    dwBColor = GetBValue(clr);
                    dwAColor = GetAValue(clr);

                    if(dwRColor < 0xFF || dwAColor > dwExpAlpha || dwGColor > 0|| dwBColor > 0)
                    {
                        info(FAIL, TEXT("Expected A:0x%x R:0x02 G:0x00 B:0x00 Actual A:0x%x R:0x%x G:0x%x B:0x%x"), dwExpAlpha, dwAColor, dwRColor, dwGColor, dwBColor);
                        OutputBitmap(hdc, zx, zy);
                    }
                }
                // vertical gradient fill
                else
                {
                    info(DETAIL, TEXT("Vertical"));

                    if(!g_bRTL)
                    {
                        // get pixel in middle of first row
                        clr = pdwBits[(zx * vert[0].y) + ((vert[1].x - vert[0].x) / 2)];
                    }
                    else
                    {
                        startofline = zx * (vert[0].y + 1) - 1;
                        clr = pdwBits[startofline - (vert[1].x - vert[0].x) / 2];
                    }
                    clr = SwapRGB(clr);
                    dwRColor = GetRValue(clr);
                    dwGColor = GetGValue(clr);
                    dwBColor = GetBValue(clr);
                    dwAColor = GetAValue(clr);

                    if(dwRColor < 0xFF || dwAColor < 0xFF || dwGColor > 0|| dwBColor > 0)
                    {
                        info(FAIL, TEXT("Expected A:0xFF R:0xFF G:0x00 B:0x00 Actual A:0x%x R:0x%x G:0x%x B:0x%x"), dwAColor, dwRColor, dwGColor, dwBColor);
                        OutputBitmap(hdc, zx, zy);
                    }

                    if(!g_bRTL)
                    {       // get pixel in middle of next to last row
                        clr = pdwBits[(zx * (vert[1].y - 1)) + ((vert[1].x - vert[0].x) / 2)];
                    }
                    else
                    {
                        startofline = zx * vert[1].y - 1;
                        clr = pdwBits[startofline - ((vert[1].x - vert[0].x) / 2)];
                    }
                    clr = SwapRGB(clr);
                    dwRColor = GetRValue(clr);
                    dwGColor = GetGValue(clr);
                    dwBColor = GetBValue(clr);
                    dwAColor = GetAValue(clr);

                    if(dwRColor < 0xFF || dwAColor > 0x03 || dwGColor > 0|| dwBColor > 0)
                    {
                        info(FAIL, TEXT("Expected A:0x02 R:0xFF G:0x00 B:0x00 Actual A:0x%x R:0x%x G:0x%x B:0x%x"), dwAColor, dwRColor, dwGColor, dwBColor);
                        OutputBitmap(hdc, zx, zy);
                    }
                }
            }

            CheckNotRESULT(NULL, SelectObject(hdc, hbmpStock));
            CheckNotRESULT(FALSE, DeleteObject(hbmp));
            CheckNotRESULT(FALSE, DeleteDC(hdc));
        }

        myReleaseDC(hdcPrimary);
    }
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
    vert [0] .Red    = (unsigned short) (GenRand() % 0xFF00);
    vert [0] .Green  = (unsigned short) (GenRand() % 0xFF00);
    vert [0] .Blue   = (unsigned short) (GenRand() % 0xFF00);
    vert [0] .Alpha  = (unsigned short) (0x0000);
    vert [1] .Red    = (unsigned short) (GenRand() % 0xFF00);
    vert [1] .Green  = (unsigned short) (GenRand() % 0xFF00);
    vert [1] .Blue   = (unsigned short) (GenRand() % 0xFF00);
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
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

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
            CheckNotRESULT(FALSE, gpfnGradientFill(hdc, vert2, 2, gRect, 1, operation));
            CheckNotRESULT(FALSE, BitBlt(hdc, zx/2, 0, zx/2, zy, hdc, 0, 0, SRCCOPY));
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx/2, zy, WHITENESS));

            vert[0].x = nBox[j][0];
            vert[0].y = nBox[j][1];
            vert[1].x = nBox[j][2];
            vert[1].y = nBox[j][3];

            CheckNotRESULT(FALSE, gpfnGradientFill(hdc, vert, 2, gRect, 1, operation));

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

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
        if(myGetBitDepth() != 2)
            SingleGradientFillDirection(hdc);

    myReleaseDC(hdc);
}

void
SingleStressGradientFill(TDC hdc)
{
    TRIVERTEX        vert[100];
    GRADIENT_RECT    gRect[100];

    int numverticies = (GenRand() % countof(vert))+1;
    int numrects = (GenRand() % countof(gRect))+1;
    int j;
    ULONG operation;

    for(j=0; j< (numverticies); j++)
    {
        // +20 - 40 to allow the rectangles to go into clipping regions
        vert [j] .x      = (unsigned short) ((GenRand() % zx + 20) - 40);
        vert [j] .y      = (unsigned short) ((GenRand() % zy + 20) - 40);
        vert [j] .Red    = (unsigned short) (GenRand() % 0xFF00);
        vert [j] .Green  = (unsigned short) (GenRand() % 0xFF00);
        vert [j] .Blue   = (unsigned short) (GenRand() % 0xFF00);
        vert [j] .Alpha  = (unsigned short) (GenRand() % 0xFF00);
    }

    for(j=0; j < (numrects); j++)
    {
        gRect[j].UpperLeft = GenRand() % numverticies;
        gRect[j].LowerRight = GenRand() % numverticies;
    }

    if(GenRand() % 2)
        operation = GRADIENT_FILL_RECT_H;
    else operation = GRADIENT_FILL_RECT_V;

    CheckNotRESULT(FALSE, gpfnGradientFill(hdc,vert,numverticies,gRect,numrects, operation));
}

void
StressGradientFill()
{
    info(ECHO, TEXT("StressGradientFill test"));

    TDC hdc = myGetDC();

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        if(myGetBitDepth() != 2)
        {
            for(int i =0; i < 100; i++)
                SingleStressGradientFill(hdc);
        }
    }

    myReleaseDC(hdc);
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

    vert [0] .Red    = (unsigned short) (GenRand() % 0xFF00);
    vert [0] .Green  = (unsigned short) (GenRand() % 0xFF00);
    vert [0] .Blue   = (unsigned short) (GenRand() % 0xFF00);
    vert [0] .Alpha  = (unsigned short) (0x0000);
    vert [1] .Red    = (unsigned short) (GenRand() % 0xFF00);
    vert [1] .Green  = (unsigned short) (GenRand() % 0xFF00);
    vert [1] .Blue   = (unsigned short) (GenRand() % 0xFF00);
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
            CheckNotRESULT(FALSE, gpfnGradientFill(hdc, vert, 2, gRect, 1, operation));

            vert [0].x = entries[j][2];
            vert [0].y = entries[j][1];
            vert [1].x = entries[j][0];
            vert [1].y = entries[j][3];
            CheckNotRESULT(FALSE, gpfnGradientFill(hdc, vert, 2, gRect, 1, operation));

            vert [0].x = entries[j][0];
            vert [0].y = entries[j][3];
            vert [1].x = entries[j][2];
            vert [1].y = entries[j][1];
            CheckNotRESULT(FALSE, gpfnGradientFill(hdc, vert, 2, gRect, 1, operation));

            vert [0].x = entries[j][2];
            vert [0].y = entries[j][3];
            vert [1].x = entries[j][0];
            vert [1].y = entries[j][1];
            CheckNotRESULT(FALSE, gpfnGradientFill(hdc, vert, 2, gRect, 1, operation));
        }
    }
}


void
FullScreenGradientFill()
{
    info(ECHO, TEXT("FullScreenGradientFill test"));
    TDC hdc = myGetDC();

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
        if(myGetBitDepth() != 2)
            SingleFullScreenGradientFill(hdc);

    myReleaseDC(hdc);
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
                    height = (GenRand() % (maxHeight- zy/8)) + zy/8;
                else height = maxHeight;

                maxWidth = zx/2;
                left = 0;
                while(maxWidth > 0)
                {
                    if(maxWidth > zx/8)
                        width = (GenRand() % (maxWidth - zx/8)) + zx/8;
                    else width = maxWidth;

                    vert[entry].x = left;
                    vert[entry].y = top;
                    vert[entry] .Red = (unsigned short) (GenRand() % 0xFF00);
                    vert[entry] .Green = (unsigned short) (GenRand() % 0xFF00);
                    vert[entry] .Blue = (unsigned short) (GenRand() % 0xFF00);
                    vert[entry] .Alpha = (unsigned short) (0x0000);

                    vert[entry + 1].x = left + width;
                    vert[entry + 1].y = top + height;
                    vert[entry + 1] .Red = (unsigned short) (GenRand() % 0xFF00);
                    vert[entry + 1] .Green = (unsigned short) (GenRand() % 0xFF00);
                    vert[entry + 1] .Blue = (unsigned short) (GenRand() % 0xFF00);
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
            CheckNotRESULT(FALSE, gpfnGradientFill(hdc, vert, entry, gRect, entry/2, operation));
            CheckNotRESULT(FALSE, BitBlt(hdc, zx/2, 0, zx/2, zy, hdc, 0, 0, SRCCOPY));
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx/2, zy, BLACKNESS));

            for(int i = 0; i < entry/2; i++)
            {
                vert2[0] = vert[gRect[i].UpperLeft];
                vert2[1] = vert[gRect[i].LowerRight];

                CheckNotRESULT(FALSE, gpfnGradientFill(hdc, vert2, 2, gRect2, 1, operation));
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

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
        if(myGetBitDepth() != 2)
            SingleComplexGradientFill(hdc);

    myReleaseDC(hdc);
}

void
GradientFill2bpp()
{
    info(ECHO, TEXT("GradientFill 2bpp test"));
    #ifdef UNDER_CE
    TDC hdc = myGetDC();
    TDC hdcCompat;
    TBITMAP hbmp = NULL, hbmpStock = NULL;
    BYTE   *lpBits = NULL;
    TRIVERTEX        vert[2] = {0};
    GRADIENT_RECT    gRect[1] = {0};

    CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));

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

    CheckNotRESULT(NULL, hbmp = myCreateBitmap(zx, zy, 1, 2/*bpp*/, NULL));
    if(hbmp)
    {
        CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));
        CheckNotRESULT(FALSE, PatBlt(hdcCompat, 0, 0, zx, zy, WHITENESS));
        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(FALSE, gpfnGradientFill(hdcCompat, vert, 2, gRect, 1, GRADIENT_FILL_RECT_H));
        CheckForLastError(ERROR_INVALID_PARAMETER);
        CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
        CheckNotRESULT(FALSE, DeleteObject(hbmp));
    }
    else info(FAIL, TEXT("Unable to create 2bpp bitmap"));

    CheckNotRESULT(NULL, hbmp = myCreateDIBSection(hdc, (VOID **) & lpBits, 2, zx, zy, DIB_RGB_COLORS, NULL, FALSE));
    if(hbmp)
    {
        CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));
        CheckNotRESULT(FALSE, PatBlt(hdcCompat, 0, 0, zx, zy, WHITENESS));
        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(FALSE, gpfnGradientFill(hdcCompat, vert, 2, gRect, 1, GRADIENT_FILL_RECT_H));
        CheckForLastError(ERROR_INVALID_PARAMETER);
        CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
        CheckNotRESULT(FALSE, DeleteObject(hbmp));
    }
    else info(FAIL, TEXT("Unable to create 2bpp DIB"));

    CheckNotRESULT(NULL, hbmp = myCreateDIBSection(hdc, (VOID **) & lpBits, 2, zx, zy, DIB_PAL_COLORS, NULL, FALSE));
    if(hbmp)
    {
        CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));
        CheckNotRESULT(FALSE, PatBlt(hdcCompat, 0, 0, zx, zy, WHITENESS));
        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(FALSE, gpfnGradientFill(hdcCompat, vert, 2, gRect, 1, GRADIENT_FILL_RECT_H));
        CheckForLastError(ERROR_INVALID_PARAMETER);
        CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
        CheckNotRESULT(FALSE, DeleteObject(hbmp));
    }
    else info(FAIL, TEXT("Unable to create 2bpp DIB"));

    CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
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

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        drawLogo(hdc, 150);
        CheckNotRESULT(FALSE, BitBlt(hdc, zx/2, 0, zx, zy, hdc, 0, 0, SRCCOPY));
        CheckNotRESULT(FALSE, InvertRect(hdc, &rc1));
        CheckNotRESULT(FALSE, BitBlt(hdc, rc2.left, rc2.top, rc2.right - rc2.left, rc2.bottom - rc2.top, hdc, rc2.left, rc2.top, DSTINVERT));
        CheckScreenHalves(hdc);
    }

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

    CheckNotRESULT(FALSE, MoveToEx(hdc, 0, 0, NULL));

    for(int i = 0; i < nMaxPoints; i++)
    {
        // a random point, somewhere around the surface, not necessarily on it.
        x = (GenRand() % (zx + 50)) - 25;
        y = (GenRand() % (zy + 50)) - 25;

        // move to the new point
        CheckNotRESULT(FALSE, MoveToEx(hdc, x, y, &pnt));
         // if the point returned wasn't what we set it to the last time we succeeded, log a failure
        if(xOld != pnt.x || yOld != pnt.y)
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
    SetMaxErrorPercentage(.05);
    TDC hdc = myGetDC();
    const int nMaxLines = 100;
    int x=0, y=0, xOld = GenRand() % zx, yOld = GenRand() % zy;
    POINT pnt;
    // make the clip region on the primary match the
    // verification driver, since lines need to match.
    HRGN hrgn;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hrgn = CreateRectRgn(0, 0, zx, zy));
        CheckNotRESULT(ERROR, SelectClipRgn(hdc, hrgn));
        CheckNotRESULT(FALSE, DeleteObject(hrgn));

        CheckNotRESULT(FALSE, MoveToEx(hdc, xOld, yOld, NULL));

        for(int i = 0; i < nMaxLines; i++)
        {
            // a random point, somewhere around the surface, not necessarily on it.
            x = (GenRand() % (zx + 50)) - 25;
            y = (GenRand() % (zy + 50)) - 25;

            // move to the new point
            CheckNotRESULT(FALSE, MoveToEx(hdc, x, y, &pnt));
             // if the point returned wasn't what we set it to the last time we succeeded, log a failure
            if(xOld != pnt.x || yOld != pnt.y)
                info(FAIL, TEXT("Old point (%d, %d) not equal to point returned (%d, %d)"), xOld, yOld, pnt.x, pnt.y);

            x = (GenRand() % (zx + 50)) - 25;
            y = (GenRand() % (zy + 50)) - 25;

            CheckNotRESULT(FALSE, LineTo(hdc, x, y));

            // lineto sets the new position, so we set the old point to this point, which is what MoveTo should return
            // on the next call.
            xOld = x;
            yOld = y;

        }
    }

    myReleaseDC(hdc);
    ResetCompareConstraints();
}

void
RandomLineToTest()
{
    info(ECHO, TEXT("RandomLineToTest"));
    SetMaxErrorPercentage(.05);
    TDC hdc = myGetDC();
    const int nMaxPoints = 100;
    POINT pnt[nMaxPoints];
    // make the clip region on the primary match the
    // verification driver, since lines need to match.
    HRGN hrgn;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hrgn = CreateRectRgn(0, 0, zx, zy));

        CheckNotRESULT(ERROR, SelectClipRgn(hdc, hrgn));
        CheckNotRESULT(FALSE, DeleteObject(hrgn));

        CheckNotRESULT(FALSE, MoveToEx(hdc, 0, 0, NULL));

        pnt[0].x = zx/2;
        pnt[0].y = 0;

        for(int i = 1; i < nMaxPoints; i++)
        {
            // a random point, somewhere around the surface, not necessarily on it.
            pnt[i].x = GenRand() % zx/2;
            pnt[i].y = (GenRand() % (zy + 50)) - 25;

            CheckNotRESULT(FALSE, LineTo(hdc, pnt[i].x, pnt[i].y));

            pnt[i].x += zx/2;
        }
        CheckNotRESULT(FALSE, Polyline(hdc, pnt, nMaxPoints));
        CheckScreenHalves(hdc);
    }

    myReleaseDC(hdc);
    ResetCompareConstraints();
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

    CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));

    if(hdcCompat)
    {
        for(i = 0; i < countof(bitDepth); i++)
        {
            for(int k=0; k < countof(nType); k++)
            {
                if(bitDepth[i] > 8 && nType[k] == DIB_PAL_COLORS)
                    continue;

                info(DETAIL, TEXT("Testing a DIB of depth %d, %d entries, and with %s usage"),
                    bitDepth[i], nNumEntries[i], nType[k]==DIB_PAL_COLORS?TEXT("DIB_PAL_COLORS"):TEXT("DIB_RGB_COLORS"));

                // myCreateDIBSection handles the failure cases.
                hbmp = myCreateDIBSection(hdcCompat, (VOID **) & lpBits, bitDepth[i], zx, zy, nType[k], NULL, FALSE);
                if(hbmp)
                {
                    CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));

                    // preinitialize the palettes to known values, 0 and 1 are easy.
                    memset(rgbqSet, 0, sizeof(rgbqSet));
                    memset(rgbqRetrieve, 1, sizeof(rgbqRetrieve));

                    for(j=0; j < nNumEntries[i]; j++)
                    {
                        rgbqSet[j].rgbRed = (BYTE) (GenRand() % 256);
                        rgbqSet[j].rgbGreen = (BYTE) (GenRand() % 256);
                        rgbqSet[j].rgbBlue = (BYTE) (GenRand() % 256);
                    }

                    CheckNotRESULT(0, SetDIBColorTable(hdcCompat, 0, nNumEntries[i], rgbqSet));
                    CheckNotRESULT(0, GetDIBColorTable(hdcCompat, 0, nNumEntries[i], rgbqRetrieve));

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
                    CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
                    CheckNotRESULT(FALSE, DeleteObject(hbmp));
                }
            }
        }
        CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
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
    // XP only succeeds this call on 1bpp.
    int nBitDepth[] = {
                                1,
#ifdef UNDER_CE
                                4,
                                8
#endif
                                };
    TCHAR szBuf[16] = {NULL};

    CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
    if(hdcCompat)
    {
        for(int i = 0; i < countof(nBitDepth); i++)
        {
            _sntprintf_s(szBuf, countof(szBuf) - 1, TEXT("BMP_GDI%d"), nBitDepth[i]);
            CheckNotRESULT(NULL, hbmp = myLoadBitmap(globalInst, szBuf));

            if(hbmp)
            {
                CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));
                CheckNotRESULT(0, GetDIBColorTable(hdcCompat, 0, (int) pow((double)2, (double)(nBitDepth[i])), rgbqRetrieve));

                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(0, SetDIBColorTable(hdcCompat, 0, (int) pow((double)2, (double)(nBitDepth[i])), rgbqRetrieve));
                CheckForLastError(ERROR_INVALID_HANDLE);
                CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
                CheckNotRESULT(FALSE, DeleteObject(hbmp));
            }
            else info(ABORT, TEXT("Failed to load bitmap for testing"));
        }
        CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
    }
    else info(ABORT, TEXT("Failed to create compatible DC for testing"));

    myReleaseDC(hdc);
}

void
CompatBitmapGetDIBColorTable()
{
    info(ECHO, TEXT("CompatBitmapGetDIBColorTable"));

    TDC hdc = myGetDC();

    if(myGetBitDepth() <= 8)
    {
        TDC hdcCompat;
        TBITMAP hbmp;
        TBITMAP hbmpStock;
        RGBQUAD rgbqRetrieve[256];

        CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
        if(hdcCompat)
        {
                CheckNotRESULT(NULL, hbmp = CreateCompatibleBitmap(hdc, 200, 200));

                if(hbmp)
                {
                    CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));

                    CheckNotRESULT(0, GetDIBColorTable(hdcCompat, 0, (int) pow((double)2, (double)(myGetBitDepth())), rgbqRetrieve));

                    SetLastError(ERROR_SUCCESS);
                    BOOL brVal = SetDIBColorTable(hdcCompat, 0,  (int) pow((double)2, (double)(myGetBitDepth())), rgbqRetrieve);
                    if(!isPalDIBDC() && brVal)
                        info(FAIL, TEXT("Able to SetDIBColorTable on a non-client bitmap."));
                    else if(isPalDIBDC() && !brVal)
                        info(FAIL, TEXT("Unable to SetDIBColorTable on a bitmap compatible to a DIB"));

                    if(!brVal)
                        CheckForLastError(ERROR_INVALID_HANDLE);

                    CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
                    CheckNotRESULT(FALSE, DeleteObject(hbmp));
                }
                else info(ABORT, TEXT("Failed to load bitmap for testing"));
            CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
        }
        else info(ABORT, TEXT("Failed to create compatible DC for testing"));
    }
    myReleaseDC(hdc);
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

    CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
    if(hdcCompat)
    {
        for(int i = 0; i < countof(nBitDepth); i++)
        {
            for(int j=0; j < countof(nType); j++)
            {
                // all error reporting is done in myCreateDIBSection
                CheckNotRESULT(NULL, hbmp = myCreateDIBSection(hdc, (VOID**) &lpBits, nBitDepth[i], 200, 200, nType[j], NULL, FALSE));

                if(hbmp)
                {
                    CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));
                    CheckNotRESULT(0, GetDIBColorTable(hdcCompat, 0, (int) pow((double)2, (double)(nBitDepth[i])), rgbqRetrieve));
                    CheckNotRESULT(0, SetDIBColorTable(hdcCompat, 0, (int) pow((double)2, (double)(nBitDepth[i])), rgbqRetrieve));
                    CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
                    CheckNotRESULT(FALSE, DeleteObject(hbmp));
                }
            }
        }
        CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
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

    CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
    if(hdcCompat)
    {
        for(int i = 0; i < countof(nBitDepth); i++)
        {
#ifndef UNDER_CE
            if(!(nBitDepth[i] == 1 || nBitDepth[i] == GetDeviceCaps(hdcCompat, BITSPIXEL)))
                continue;
#endif
            CheckNotRESULT(NULL, hbmp = myCreateBitmap(200, 200, 1, nBitDepth[i], NULL));

            if(hbmp)
            {
                // BUGBUG: desktop behaves differently here.
                CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));
                CheckNotRESULT(0, GetDIBColorTable(hdcCompat, 0, nNumEntries[i], rgbqRetrieve));
                SetLastError(ERROR_SUCCESS);
                CheckForRESULT(0, SetDIBColorTable(hdcCompat, 0, nNumEntries[i], rgbqRetrieve));
                CheckForLastError(ERROR_INVALID_HANDLE);
                CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
                CheckNotRESULT(FALSE, DeleteObject(hbmp));
            }
            else info(ABORT, TEXT("Failed to create DIB for testing"));
        }
        CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
    }
    else info(ABORT, TEXT("Failed to create compatible DC for testing"));

    myReleaseDC(hdc);
}

// returns true if gradient fill is implemented in the display driver, false if it isn't.
BOOL
GradientFillSupported()
{
    BOOL bOldVerify = SetSurfVerify(FALSE);
    TDC hdcPrimary = myGetDC(NULL);
    TDC hdcCompat = CreateCompatibleDC(hdcPrimary);
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

    SetLastError(ERROR_SUCCESS);

    // if the call fails, and the error is E_NOTIMPL, then we don't have gradient fill in the display driver
    if(!gpfnGradientFill(hdcCompat, vert, 2, gRect, 1, GRADIENT_FILL_RECT_H))
        if(ERROR_NOT_SUPPORTED == GetLastError())
            bRval = FALSE;

    DeleteDC(hdcCompat);
    myReleaseDC(NULL, hdcPrimary);
    SetSurfVerify(bOldVerify);
    return bRval;
}

void
PatternBrushFillRectTest()
{
    info(ECHO, TEXT("PatternBrushFillRectTest test"));

    // 1bpp bitmaps use foreground/background colors when BitBlitting, but
    // pattern's on those surfaces use the nearest match, so this test isn't valid.

    TDC hdc = myGetDC();

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        if(myGetBitDepth() > 1)
        {
            TDC hdcCompat;

            HBRUSH  hBrush;
            TBITMAP hbmp, hbmpStock;
            int i, width, height;
            RECT rc;

            CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));

            // min width/height of 2 to prevent a divide by 0.
            width = (GenRand() % (zx/4 - 2)) + 2;
            height = (GenRand() %( zy/2 - 2)) + 2;

            // pick a random location, but align the pattern so it'll match on both sides.
            rc.top = (GenRand() % ((zy - (zy % height))/height)) * height;
            rc.left = (GenRand() % ((zx/2 - (zx/2 % width))/width)) * width;
            rc.bottom = rc.top + height;
            rc.right = rc.left + width;

            for(i = 0; i < countof(gBitDepths); i++)
            {
    // XP only allows bitmaps that match the primary or are 1bpp to be selected into a compatible dc.
    #ifndef UNDER_CE
                if(!(gBitDepths[i] == 1 || gBitDepths[i] == GetDeviceCaps(hdc, BITSPIXEL)))
                    continue;
    #endif
                info(DETAIL, TEXT("Testing a brush of bit depth %d"), gBitDepths[i]);
                CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
                CheckNotRESULT(NULL, hbmp = myCreateBitmap(width, height, 1, gBitDepths[i], NULL));

                CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));

                CheckNotRESULT(FALSE, PatBlt(hdcCompat, 0, 0, width, height, WHITENESS));
                FillSurface(hdcCompat);

                CheckNotRESULT(NULL, hBrush = CreatePatternBrush(hbmp));

                CheckNotRESULT(FALSE, FillRect(hdc, &rc, hBrush));

                // initialize the right side to something so we catch if the BitBlt/FillRect does nothing.
                CheckNotRESULT(FALSE, PatBlt(hdc, rc.left + zx/2, rc.top, rc.right - rc.left, rc.bottom - rc.top, BLACKNESS));
                CheckNotRESULT(FALSE, BitBlt(hdc, rc.left + zx/2, rc.top, rc.right - rc.left, rc.bottom - rc.top, hdcCompat, 0, 0, SRCCOPY));

                CheckScreenHalves(hdc);

                CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
                CheckNotRESULT(FALSE, DeleteObject(hbmp));
                CheckNotRESULT(FALSE, DeleteObject(hBrush));
            }

            CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
        }
        else info(DETAIL, TEXT("Test not valid on 1bpp bitmaps, skipping this portion"));
    }

    myReleaseDC(hdc);
}

// this test verifies that PatBlt succeeds and fails as expected based on the rop's need for a source.
void
PatBltBadRopTest()
{
    info(ECHO, TEXT("PatBltBadRopTest"));
// xp doesn't fail PatBt cases with rop's that are invalid.
#ifdef UNDER_CE
    TDC hdc = myGetDC();
    BOOL bNeedSrc;
    BOOL bPatBltResult;
    DWORD dwRop;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        for(int i = 0; i < countof(gnvRop3Array); i++)
        {
            // take a copy of the rop shifted right by 16 so we only have the upper 16 bits (all that matters).
            dwRop = gnvRop3Array[i].dwVal >> 16;
            // this is a magic formula that takes the rop as 0x00NN and tells us whether or not it needs a source.
            bNeedSrc = ((((dwRop >> 2) ^ dwRop) & 0x33) != 0);
            SetLastError(ERROR_SUCCESS);
            bPatBltResult = PatBlt(hdc, 0, 0, zx, zy, gnvRop3Array[i].dwVal);
            if(bNeedSrc && bPatBltResult)
                info(FAIL, TEXT("PatBlt succeeded for rop %s when expected to fail because rop requires a source"), gnvRop3Array[i].szName);
            if(!bNeedSrc && !bPatBltResult)
                info(FAIL, TEXT("PatBlt failed for rop %s when expected to succeed because rop doesn't require a source"), gnvRop3Array[i].szName);

            if(!bPatBltResult)
                CheckForLastError(ERROR_INVALID_HANDLE);
        }
    }

    myReleaseDC(hdc);
#endif
}

void
DrawFocusRectTest(int testfunc)
{
    info(ECHO, TEXT("DrawFocusRect test"));

    TDC hdc = myGetDC();
    RECT rc = {10, 10, zx/2 - 10, zy - 10};

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(FALSE, DrawFocusRect(hdc, &rc));
        CheckNotRESULT(FALSE, DrawFocusRect(hdc, &rc));

        CheckScreenHalves(hdc);
    }

    myReleaseDC(hdc);
}

// verifies that a bitmap compatible to a DIB behaves exactly the same as the DIB
void
CreateCompatibleToDibTest(int testfunc, int bitDepth)
{
    info(ECHO, TEXT("CreateCompatibleToDibTest, %dBPP"), bitDepth);

    TDC hdc       = myGetDC();
    TDC hdcDIB    = NULL;
    TDC hdcCompat = NULL;

    TBITMAP hbmpDIB         = NULL;
    TBITMAP hbmpDIBStock    = NULL;
    TBITMAP hbmpCompat      = NULL;
    TBITMAP hbmpCompatStock = NULL;

    BYTE *lpBits    = NULL;
    RGBQUAD rgbq[2] = { { 0x55, 0x55, 0x55 },
                        { 0xAA, 0xAA, 0xAA } };

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hdcDIB = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));

#ifndef UNDER_CE
        // Desktop doesn't support 2bpp.
        if(bitDepth == 2)
            goto cleanup;
#endif

        if(hdcDIB && hdcCompat)
        {
            CheckNotRESULT(NULL, hbmpDIB = myCreateDIBSection(hdc, (VOID **) &lpBits, bitDepth, zx/2, zy, DIB_RGB_COLORS, NULL, FALSE));
            if(hbmpDIB)
            {
                CheckNotRESULT(NULL, hbmpDIBStock = (TBITMAP) SelectObject(hdcDIB, hbmpDIB));

                // change the 1bpp color table entries so they do not match the default 1bpp color table.
                if(bitDepth == 1)
                    CheckNotRESULT(0, SetDIBColorTable(hdcDIB, 0, 2, rgbq));

                CheckNotRESULT(FALSE, PatBlt(hdcDIB, 0, 0, zx/2, zy, WHITENESS));
                CheckNotRESULT(NULL, hbmpCompat = CreateCompatibleBitmap(hdcDIB, zx/2, zy));

                if(hbmpCompat)
                {
                    CheckNotRESULT(NULL, hbmpCompatStock = (TBITMAP) SelectObject(hdcCompat, hbmpCompat));
                    CheckNotRESULT(FALSE, PatBlt(hdcCompat, 0, 0, zx/2, zy, WHITENESS));
                    drawLogo(hdcDIB, 150);
                    drawLogo(hdcCompat, 150);

                    CheckNotRESULT(FALSE, BitBlt(hdc, 0, 0, zx/2, zy, hdcDIB, 0, 0, SRCCOPY));
                    CheckNotRESULT(FALSE, BitBlt(hdc, zx/2, 0, zx/2, zy, hdcCompat, 0, 0, SRCCOPY));

                    CheckScreenHalves(hdc);
                }
                else info(ABORT, TEXT("Failed to create necessary compatible bitmap for test"));
            }
            else info(ABORT, TEXT("Failed to create necessairy DIB for test"));
        }
        else info(ABORT, TEXT("Failed to create necessairy DC's for test"));
    }

#ifndef UNDER_CE
cleanup:
#endif

    if(hbmpDIB)
        CheckNotRESULT(NULL, SelectObject(hdcDIB, hbmpDIBStock));
    if(hbmpCompat)
        CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpCompatStock));
    if(hbmpDIB)
        CheckNotRESULT(FALSE, DeleteObject(hbmpDIB));
    if(hbmpCompat)
        CheckNotRESULT(FALSE, DeleteObject(hbmpCompat));
    if(hdcDIB)
        CheckNotRESULT(FALSE, DeleteDC(hdcDIB));
    if(hdcCompat)
        CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
    myReleaseDC(hdc);
}

// verifies that a bitmap compatible to the primary behaves the same
// as the primary.
void
CreateCompatibleBitmapDrawTest()
{
    info(ECHO, TEXT("CreateCompatibleBitmapDrawTest"));

    TDC hdc           = myGetDC();
    TDC hdcCompat     = NULL;
    TBITMAP hbmp      = NULL;
    TBITMAP hbmpStock = NULL;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hbmp = CreateCompatibleBitmap(hdc, zx/2, zy));

        if(hdcCompat && hbmp)
        {
            CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));
            CheckNotRESULT(FALSE, BitBlt(hdcCompat, 0, 0, zx/2, zy, hdc, 0, 0, SRCCOPY));
            drawLogo(hdc, 150);
            drawLogo(hdcCompat, 150);
            CheckNotRESULT(FALSE, BitBlt(hdc, zx/2, 0, zx/2, zy, hdcCompat, 0, 0, SRCCOPY));

            CheckScreenHalves(hdc);

            CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
            CheckNotRESULT(FALSE, DeleteObject(hbmp));
            CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
        }
    }

    myReleaseDC(hdc);
}


// verifies that bitmaps Blt'ed between non-similar layout DCs
// are not mirrored.  Blts between similar DCs are already checked elsewhere
void
BlitMirrorTest(int testFunc)
{
    info(ECHO, TEXT("BlitMirrorTest - %s"), funcName[testFunc]);

    TDC hdc = myGetDC();
    TDC hdcCompat;
    TBITMAP hbmp;
    TBITMAP hbmpStock;
#ifdef UNDER_CE
    DWORD dwRop = SRCCOPY;     // SRCCOPY | NOMIRRORBITMAP
    DWORD dwLayout = NULL;
#else
    DWORD dwRop = SRCCOPY | NOMIRRORBITMAP;
    DWORD dwLayout = LAYOUT_BITMAPORIENTATIONPRESERVED;
#endif
    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));

        if(g_bRTL)
            CheckNotRESULT(NULL, SetLayout(hdcCompat, dwLayout));
        else
            CheckNotRESULT(LAYOUT_RTL, SetLayout(hdcCompat, LAYOUT_RTL | dwLayout));

        CheckNotRESULT(NULL, hbmp = CreateCompatibleBitmap(hdc, zx, zy));

        if(hdcCompat && hbmp)
        {
            CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));
            CheckNotRESULT(FALSE, BitBlt(hdcCompat, 0, 0, zx, zy, hdc, 0, 0, SRCCOPY));
            drawLogo(hdc, 150);
            drawLogo(hdcCompat, 150);

            SetMaxScreenHalvesErrorPercentage(0.0);
            ResetCompareConstraints();
            switch(testFunc)
            {
                case EBitBlt:
                    CheckNotRESULT(FALSE, BitBlt(hdc, zx/2, 0, zx/2, zy, hdcCompat, 0, 0, dwRop));
                    break;
                case EStretchBlt:
                    // Size of blit is one line shorter to prevent BitBlit shortcut
                    info(DETAIL, TEXT("A 5%% variance is acceptable due to stretching differences, a moderate number of mismatches listed below are expected."));
                    CheckNotRESULT(FALSE, StretchBlt(hdc, zx/2, 0, zx/2, zy, hdcCompat, 0, 0, zx/2, zy-1, dwRop));
                    SetMaxScreenHalvesErrorPercentage(5.0);     // Allow 5% difference due to stretching
                    SetMaxErrorPercentage(5.0);
                    break;
                case EMaskBlt:
                    CheckNotRESULT(FALSE, MaskBlt(hdc, zx/2, 0, zx/2, zy, hdcCompat, 0, 0, (TBITMAP) NULL, 0, 0, dwRop));
                    break;
                case EAlphaBlend:
                    {
                        BLENDFUNCTION bf;

                        bf.BlendOp = AC_SRC_OVER;
                        bf.BlendFlags = 0;
                        bf.SourceConstantAlpha = 0xFF;
                        bf.AlphaFormat = 0;

                        CheckNotRESULT(FALSE, gpfnAlphaBlend(hdc, zx/2, 0, zx/2, zy, hdcCompat, 0, 0, zx/2, zy, bf));
                    }
                    break;
                default:
                    info(FAIL, TEXT("BlitMirrorTest called on an invalid case"));
                    break;
            }

            //Flip near half of screen to match...
            StretchBlt(hdc, zx/2, 0, -zx/2, zy, hdc, 0, 0, zx/2, zy, SRCCOPY);
            CheckScreenHalves(hdc);

            CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
            CheckNotRESULT(FALSE, DeleteObject(hbmp));
            CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
        }

        ResetScreenHalvesCompareConstraints();
    }

    myReleaseDC(hdc);

    // verification tolerances must be left active until after myReleaseDC does verification.
    ResetCompareConstraints();
}


// Verifies that the center portion of an ellipse, roundrect, and rectangle
// are transparent when you use a NULL brush.
void
HollowPolyTest(int testFunc)
{
    info(ECHO, TEXT("%s - HollowPolyTest"), funcName[testFunc]);
    // this creates a window/surface and initializes it to a random color
    TDC hdc = myGetDC();
    LOGPEN lpn;
    HPEN hpn, hpnStock;
    HBRUSH hbrStock;
    UINT style[] = { PS_SOLID, PS_DASH, PS_NULL };
    COLORREF crExpected;
    int nOldRop2;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(0, nOldRop2 = SetROP2(hdc, gnvRop2Array[GenRand() % countof(gnvRop2Array)].dwVal));

        // set up a random pen to use, shouldn't have any effect on the center.
        lpn.lopnStyle = style[GenRand() % countof(style)];
        lpn.lopnWidth.x = GenRand() % 5;
        lpn.lopnWidth.y = 0;
        lpn.lopnColor = randColorRef();

        // get the color of the center of the screen, this shouldn't be changed by the drawing call.
        crExpected = GetPixel(hdc, zx/2, zy/2);

        // select the hollow brush, and create and select the pen for use
        CheckNotRESULT(NULL, hbrStock = (HBRUSH) SelectObject(hdc, GetStockObject(HOLLOW_BRUSH)));
        CheckNotRESULT(NULL, hpn = CreatePenIndirect(&lpn));
        CheckNotRESULT(NULL, hpnStock = (HPEN) SelectObject(hdc, hpn));

        // rectangle, round rect, and ellips should all leave the center alone
        switch(testFunc)
        {
            case ERectangle:
                CheckNotRESULT(FALSE, Rectangle(hdc, zx/3, zy/3, 2*zx/3, 2*zy/3));
                break;
            case ERoundRect:
                CheckNotRESULT(FALSE, RoundRect(hdc, zx/3, zy/3, 2*zx/3, 2*zy/3, GenRand() % 10, GenRand() % 10));
                break;
            case EEllipse:
                CheckNotRESULT(FALSE, Ellipse(hdc, zx/3, zy/3, 2*zx/3, 2*zy/3));
                break;
        }

        // verify that the pixel in the center of the shape didn't change.
        CheckForRESULT(crExpected, GetPixel(hdc, zx/2, zy/2));

        // restore the rop2
        CheckNotRESULT(0, SetROP2(hdc, nOldRop2));

        CheckNotRESULT(NULL, SelectObject(hdc, hbrStock));
        CheckNotRESULT(NULL, SelectObject(hdc, hpnStock));
        CheckNotRESULT(FALSE, DeleteObject(hpn));
    }

    myReleaseDC(hdc);
}


UINT FindNearestRGBQUAD(RGBQUAD * rgbq, UINT nNumEntries, COLORREF cr)
{
    int nCRValue = GetRValue(cr) + GetGValue(cr) + GetBValue(cr);
    UINT nSmallestDiff, nCurrentDiff;
    int nIndex = CLR_INVALID;

    for(UINT i = 0; i < nNumEntries; i++)
    {
        nCurrentDiff = abs((rgbq[i].rgbRed + rgbq[i].rgbGreen + rgbq[i].rgbBlue) - nCRValue);

        // if the current index entry is closer than the current smallest, then this is the new smallest.
        // if the index is -1, then this is the first time, so it's by definition the smallest so far.
        if(nIndex == CLR_INVALID || nCurrentDiff < nSmallestDiff)
        {
            nSmallestDiff = nCurrentDiff;
            nIndex = i;

            // if it's zero, it's it so return.
            if(0 == nSmallestDiff)
                break;
        }
    }

    return nIndex;
}

void TestAllRops(int testFunc)
{
    info(ECHO, TEXT("TestAllRops - %s"), funcName[testFunc]);

    // This test isn't valid on a printer dc.
    NOPRINTERDCV(NULL);

    if(!isPalDIBDC() && (2 == myGetBitDepth() || 4 == myGetBitDepth()))
    {
        // Test cannot run on non-DIB bitmaps at paletted bit depths other than 1 and 8bpp
        info(DETAIL, TEXT("Test cannot run on this surface type/bit depth."));
        return;
    }

    // turn of driver verification for this test, it does all verification.
    BOOL bOldVerify = SetSurfVerify(FALSE);
    TDC hdc = myGetDC();

    // When testing StretchDIBits, we need a dib to test with, and it needs to be
    // initialized.
    TDC hdcCompat = NULL;
    TDC hdcSource = NULL;
    TBITMAP hbmpDIB = NULL, hbmpStock = NULL;
    BYTE *lpBits = NULL;
    struct MYBITMAPINFO bmi;

    // this controls whether or not we loop for setting background colors
    // along with foreground colors.
    int nBackgroundMax = (testFunc == EMaskBlt)?0xFF:0;

    int nMaxErrors = MAXERRORMESSAGES;
    // The masked used when testing MaskBlt.  this is just a 2 pixel by 2pixel square
    // with the left pixels set to 1 and the right set to 0.  The left uses the foreground
    // ROP when combining, the right uses the background ROP when combining.
    BYTE nMaskInit[2][2] = { {0x40}, {0x40} };
    TBITMAP hbmpMask;
    COLORREF crSource, crDest, crBrush;
    HBRUSH hbr, hbrStock;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // Create the source DIB for StretchDIBits.
        if(testFunc == EStretchDIBits)
        {
            CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
            // create a 2x2 DIB for testing.
            CheckNotRESULT(NULL, hbmpDIB = myCreateDIBSection(hdc, (VOID **) &lpBits, myGetBitDepth(), 1, 1, DIB_RGB_COLORS, &bmi, FALSE));
            CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmpDIB));
            // When referring to the source, we now need to refer to hdcCompat.
            hdcSource = hdcCompat;
        }
        // if we're not testing StretchDIBits, then the source is the dest.
        else hdcSource = hdc;

        CheckNotRESULT(NULL, hbmpMask = myCreateBitmap(2, 2, 1, 1, nMaskInit));

        // step through foreground/background rop's if we're testing MaskBlt
        for(int j=0; j <= nBackgroundMax && nMaxErrors > 0; j++)
            for(int i=0; i <= 0xFF && nMaxErrors > 0; i++)
            {
                crSource = randColorRef();
                crBrush = randColorRef();
                crDest = randColorRef();

                CheckNotRESULT(NULL, hbr = CreateSolidBrush(crBrush));

                CheckNotRESULT(NULL, hbrStock = (HBRUSH) SelectObject(hdc, hbr));


                // set the brush color to the actual color used.
                // for 1bpp, we need the color prior to realization to see if it was the background color.
                if(myGetBitDepth() != 1)
                    CheckNotRESULT(CLR_INVALID, crBrush = SetPixel(hdc, 0, 0, crBrush));

                // set the source and dest colors on the surface.
                CheckNotRESULT(CLR_INVALID, crSource = SetPixel(hdcSource, 0, 0, crSource));
                CheckNotRESULT(CLR_INVALID, crDest = SetPixel(hdc, 0, 1, crDest));

                if(testFunc == EMaskBlt)
                {
                    CheckNotRESULT(CLR_INVALID, crSource = SetPixel(hdcSource, 1, 0, crSource));
                    CheckNotRESULT(CLR_INVALID, crDest = SetPixel(hdc, 1, 1, crDest));
                }

                switch(testFunc)
                {
                    case EBitBlt:
                        CheckNotRESULT(FALSE, BitBlt(hdc, 0, 1, 1, 1, hdc, 0, 0, i << 16));
                        break;
                    case EStretchBlt:
                        // take the single pixel and double it, so we're sure we're hitting the stretching path.
                        CheckNotRESULT(FALSE, StretchBlt(hdc, 0, 1, 2, 2, hdc, 0, 0, 1, 1, i << 16));
                        break;
                    case EMaskBlt:
                        CheckNotRESULT(FALSE, MaskBlt(hdc, 0, 1, 2, 1, hdc, 0, 0, hbmpMask, 0, 0, MAKEROP4(j << 16, i << 16)));
                        break;
                    case EStretchDIBits:
                        CheckNotRESULT(FALSE, StretchDIBits(hdc, 0, 1, 1, 1, 0, 0, 1, 1, lpBits, (LPBITMAPINFO) &bmi, DIB_RGB_COLORS, i << 16));
                        break;
                }

                // general case for non-paletted surfaces.
                if(myGetBitDepth() > 8)
                {
                    COLORREF crExpected, crReturned;
                    BYTE ExpectedValR = GetChannelValue(GetRValue(crSource), GetRValue(crDest), GetRValue(crBrush), i << 16);
                    BYTE ExpectedValG = GetChannelValue(GetGValue(crSource), GetGValue(crDest), GetGValue(crBrush), i << 16);
                    BYTE ExpectedValB = GetChannelValue(GetBValue(crSource), GetBValue(crDest), GetBValue(crBrush), i << 16);

                    crExpected = RGB(ExpectedValR, ExpectedValG, ExpectedValB);

                    if(!g_bRTL || !(testFunc == EMaskBlt))
                        crReturned = GetPixel(hdc, 0, 1);
                    else
                        crReturned = GetPixel(hdc, 1, 1);

                    crReturned &= 0xFFFFFF;


                    // StretchDIBits requires extra blitting, which results in the loss of accuracy of the lower bits due to color conversions.
                    if(16 == myGetBitDepth() && testFunc == EStretchDIBits)
                    {
                        DWORD dwMask = (gdwShiftedRedBitMask | gdwShiftedGreenBitMask | gdwShiftedBlueBitMask);
                        crExpected &= dwMask;
                        crReturned &= dwMask;
                    }

                    if(crReturned != crExpected)
                    {
                        nMaxErrors--;
                        info(FAIL, TEXT("Resulting pixel value 0x%08x mismatching expected value 0x%08x"), crReturned, crExpected);
                        info(FAIL, TEXT("Dest 0x%08x, Source 0x%08x, Brush 0x%08x, ROP 0x%08x"), crDest, crSource, crBrush, i << 16);
                    }

                    if(testFunc == EMaskBlt)
                    {
                        ExpectedValR = GetChannelValue(GetRValue(crSource), GetRValue(crDest), GetRValue(crBrush), j << 16);
                        ExpectedValG = GetChannelValue(GetGValue(crSource), GetGValue(crDest), GetGValue(crBrush), j << 16);
                        ExpectedValB = GetChannelValue(GetBValue(crSource), GetBValue(crDest), GetBValue(crBrush), j << 16);

                        crExpected = RGB(ExpectedValR, ExpectedValG, ExpectedValB);

                        if(!g_bRTL)
                            crReturned = GetPixel(hdc, 1, 1);
                        else
                            crReturned = GetPixel(hdc, 0, 1);

                        crReturned &= 0xFFFFFF;

                        // mask off the alpha value if it's there.
                        if(crReturned != crExpected)
                        {
                            nMaxErrors--;
                            info(FAIL, TEXT("Resulting pixel value 0x%08x mismatching expected value 0x%08x"), crReturned, crExpected);
                            info(FAIL, TEXT("Dest 0x%08x, Source 0x%08x, Brush 0x%08x, ROP 0x%08x"), crDest, crSource, crBrush, j << 16);
                        }
                    }
                }
                // if it's a paletted DIB of any bit depth then we can handle it.
                // unless it's system paletted, in which case it's handled differently.
                else if(isPalDIBDC() && !isSysPalDIBDC())
                {
                    UINT BrushIndex, SourceIndex, DestIndex;
                    UINT ExpectedIndex, ResultingIndex;
                    COLORREF crReturned;
                    RGBQUAD rgbq[256];
                    UINT nEntries;

                    CheckNotRESULT(0, nEntries = GetDIBColorTable(hdc, 0, 256, rgbq));

                    // get the palette index of the source, dest, and brush. (crSource, crDest, crBrush)
                    CheckNotRESULT(CLR_INVALID, SourceIndex = FindNearestRGBQUAD(rgbq, nEntries, crSource));
                    CheckNotRESULT(CLR_INVALID, DestIndex = FindNearestRGBQUAD(rgbq, nEntries, crDest));
                    CheckNotRESULT(CLR_INVALID, BrushIndex = FindNearestRGBQUAD(rgbq, nEntries, crBrush));

                    if(1 == myGetBitDepth())
                        BrushIndex = (crBrush == GetBkColor(hdc) ? 1 : 0);

                    ExpectedIndex = GetChannelValue(SourceIndex, DestIndex, BrushIndex, i << 16);

                    // for some ROP's the upper bits (which are irelevent for the index) will become enabled.
                    // ignore them.
                    switch(myGetBitDepth())
                    {
                        case 1:
                            ExpectedIndex &= 0x1;
                            break;
                        case 2:
                            ExpectedIndex &= 0x3;
                            break;
                        case 4:
                            ExpectedIndex &= 0xf;
                            break;
                        default:
                            break;
                   }

                    // process the rop on the palette index
                    if(!g_bRTL || !(testFunc == EMaskBlt))
                        CheckNotRESULT(CLR_INVALID, crReturned = GetPixel(hdc, 0, 1));
                    else
                        CheckNotRESULT(CLR_INVALID, crReturned = GetPixel(hdc, 1, 1));

                    // get the palette index of the resulting color
                    CheckNotRESULT(CLR_INVALID, ResultingIndex = FindNearestRGBQUAD(rgbq, nEntries, crReturned));

                    // if the palette indexes don't match, then fail.
                    if(ExpectedIndex != ResultingIndex)
                    {
                        nMaxErrors--;
                        info(FAIL, TEXT("Resulting index from the ROP is incorrect.  Expected %d, got %d"), ExpectedIndex, ResultingIndex);
                        info(FAIL, TEXT("Dest index %d, Source index %d, Brush index %d"), DestIndex, SourceIndex, BrushIndex);
                        info(FAIL, TEXT("Dest 0x%08x, Source 0x%08x, Brush 0x%08x, Result 0x%08x, ROP 0x%08x"), crDest, crSource, crBrush, crReturned, i<<16);
                    }

                    if(testFunc == EMaskBlt)
                    {
                        // get the palette index for the second dest pixel
                        if(!g_bRTL)
                            CheckNotRESULT(CLR_INVALID, crReturned = GetPixel(hdc, 1, 1));
                        else
                            CheckNotRESULT(CLR_INVALID, crReturned = GetPixel(hdc, 0, 1));
                        CheckNotRESULT(CLR_INVALID, ResultingIndex = FindNearestRGBQUAD(rgbq, nEntries, crReturned));

                        ExpectedIndex = GetChannelValue(SourceIndex, DestIndex, BrushIndex, j << 16);

                         // for some ROP's the upper bits (which are irelevent for the index) will become enabled.
                         // ignore them.
                         switch(myGetBitDepth())
                         {
                             case 1:
                                 ExpectedIndex &= 0x1;
                                 break;
                             case 2:
                                 ExpectedIndex &= 0x3;
                                 break;
                             case 4:
                                 ExpectedIndex &= 0xf;
                                 break;
                             default:
                                 break;
                        }

                        // if the palette indexes don't match, then fail.
                        if(ExpectedIndex != ResultingIndex)
                        {
                            nMaxErrors--;
                            info(FAIL, TEXT("Resulting index from the ROP is incorrect.  Expected %d, got %d"), ExpectedIndex, ResultingIndex);
                            info(FAIL, TEXT("Dest index %d, Source index %d, Brush index %d"), DestIndex, SourceIndex, BrushIndex);
                            info(FAIL, TEXT("Dest 0x%08x, Source 0x%08x, Brush 0x%08x, Result 0x%08x, ROP 0x%08x"), crDest, crSource, crBrush, crReturned, j<<16);
                        }
                    }
                }
                // it's one of the other types of surfaces we can test (1bpp, 8bpp paletted DIB using a system palette)
                else
                {
                    HPALETTE hpal = NULL;
                    UINT BrushIndex, SourceIndex, DestIndex;
                    UINT ExpectedIndex, ResultingIndex;
                    COLORREF crReturned;

                    // if we're running at 1bpp, the palette should be the foreground/background colors.
                    if(1 == myGetBitDepth())
                    {
                        BrushIndex = (crBrush == GetBkColor(hdc) ? 1 : 0);
                        DestIndex = (crDest == GetBkColor(hdc) ? 1 : 0);
                        SourceIndex = (crSource == GetBkColor(hdc) ? 1 : 0);
                    }
                    else
                    {
                        CheckNotRESULT(NULL, hpal = (HPALETTE) GetCurrentObject(hdc, OBJ_PAL));

                        // get the palette index of the source, dest, and brush. (crSource, crDest, crBrush)
                        CheckNotRESULT(CLR_INVALID, SourceIndex = GetNearestPaletteIndex(hpal, crSource));
                        CheckNotRESULT(CLR_INVALID, DestIndex = GetNearestPaletteIndex(hpal, crDest));
                        CheckNotRESULT(CLR_INVALID, BrushIndex = GetNearestPaletteIndex(hpal, crBrush));
                    }

                    // process the rop on the palette index
                    ExpectedIndex = GetChannelValue(SourceIndex, DestIndex, BrushIndex, i << 16);

                     // for some ROP's the upper bits (which are irelevent for the index) will become enabled.
                     // ignore them.
                     switch(myGetBitDepth())
                     {
                         case 1:
                             ExpectedIndex &= 0x1;
                             break;
                         case 2:
                             ExpectedIndex &= 0x3;
                             break;
                         case 4:
                             ExpectedIndex &= 0xf;
                             break;
                         default:
                             break;
                    }

                    if(!g_bRTL || !(testFunc == EMaskBlt))
                        CheckNotRESULT(CLR_INVALID, crReturned = GetPixel(hdc, 0, 1));
                    else
                        CheckNotRESULT(CLR_INVALID, crReturned = GetPixel(hdc, 1, 1));

                    if(1 == myGetBitDepth())
                    {
                        ResultingIndex = (crReturned == GetBkColor(hdc) ? 1 : 0);
                    }
                    else
                    {
                        // get the palette index of the resulting color
                        CheckNotRESULT(CLR_INVALID, ResultingIndex = GetNearestPaletteIndex(hpal, crReturned));
                    }

                    // if the palette indexes don't match, then fail.
                    if(ExpectedIndex != ResultingIndex)
                    {
                        nMaxErrors--;
                        info(FAIL, TEXT("Resulting index from the ROP is incorrect.  Expected %d, got %d"), ExpectedIndex, ResultingIndex);
                        info(FAIL, TEXT("Dest index %d, Source index %d, Brush index %d"), DestIndex, SourceIndex, BrushIndex);
                        info(FAIL, TEXT("Dest 0x%08x, Source 0x%08x, Brush 0x%08x, Result 0x%08x, ROP 0x%08x"), crDest, crSource, crBrush, crReturned, i<<16);
                    }

                    if(testFunc == EMaskBlt)
                    {
                        // get the palette index for the second dest pixel
                        if(!g_bRTL)
                            CheckNotRESULT(CLR_INVALID, crReturned = GetPixel(hdc, 1, 1));
                        else
                            CheckNotRESULT(CLR_INVALID, crReturned = GetPixel(hdc, 0, 1));

                        if(1 == myGetBitDepth())
                        {
                            ResultingIndex = (crReturned == GetBkColor(hdc) ? 1 : 0);
                        }
                        else
                        {
                            CheckNotRESULT(CLR_INVALID, ResultingIndex = GetNearestPaletteIndex(hpal, crReturned));
                        }
                        ExpectedIndex = GetChannelValue(SourceIndex, DestIndex, BrushIndex, j << 16);

                         // for some ROP's the upper bits (which are irelevent for the index) will become enabled.
                         // ignore them.
                         switch(myGetBitDepth())
                         {
                             case 1:
                                 ExpectedIndex &= 0x1;
                                 break;
                             case 2:
                                 ExpectedIndex &= 0x3;
                                 break;
                             case 4:
                                 ExpectedIndex &= 0xf;
                                 break;
                             default:
                                 break;
                        }

                        // if the palette indexes don't match, then fail.
                        if(ExpectedIndex != ResultingIndex)
                        {
                            nMaxErrors--;
                            info(FAIL, TEXT("Resulting index from the ROP is incorrect.  Expected %d, got %d"), ExpectedIndex, ResultingIndex);
                            info(FAIL, TEXT("Dest index %d, Source index %d, Brush index %d"), DestIndex, SourceIndex, BrushIndex);
                            info(FAIL, TEXT("Dest 0x%08x, Source 0x%08x, Brush 0x%08x, Result 0x%08x, ROP 0x%08x"), crDest, crSource, crBrush, crReturned, j<<16);
                        }
                    }
                    if(hpal)
                        CheckNotRESULT(FALSE, DeleteObject(hpal));
                }

                CheckNotRESULT(NULL, SelectObject(hdc, hbrStock));
                CheckNotRESULT(FALSE, DeleteObject(hbr));

                if(nMaxErrors <= 0)
                    info(FAIL, TEXT("Exceeded maximum errors printed, exiting test."));
            }

        if(hbmpDIB)
        {
            CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
            CheckNotRESULT(FALSE, DeleteObject(hbmpDIB));
        }

        if(hdcCompat)
            CheckNotRESULT(FALSE, DeleteDC(hdcCompat));

        CheckNotRESULT(FALSE, DeleteObject(hbmpMask));
    }

    myReleaseDC(hdc);
    SetSurfVerify(bOldVerify);
}

// An alpha in the upper byte results in unexpected behavior.
// Verify the system doesn't crash.
void
SetGetPixelAlphaTest()
{
    info(ECHO, TEXT("SetGetPixelAlphaTest"));

    TDC hdc = myGetDC();
    COLORREF cr[] = {
                            RGBA(0, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(1, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(2, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(127, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(128, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(129, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(253, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(254, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(255, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            randColorRefA()
                            };

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        for(int i = 0; i < countof(cr); i++)
        {
            // resulting colors are undefined.
            CheckNotRESULT(-1, SetPixel(hdc, 0, 0, cr[i]));
            CheckNotRESULT(CLR_INVALID, GetPixel(hdc, 0, 0));
        }
    }

    myReleaseDC(hdc);
}

void
GetPixelAlphaDIBTest()
{
    info(ECHO, TEXT("GetPixelAlphaDIBTest"));

    // we're directly modifying the lpBits pointer so we need to disable surface verification.
    BOOL bOldVerify = SetSurfVerify(FALSE);
    TDC hdc = myGetDC();
    TDC hdcCompat;
    TBITMAP hbmp, hbmpStock;
    COLORREF cr[] = {
                            RGBA(0, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(1, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(2, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(127, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(128, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(129, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(253, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(254, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(255, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            randColorRefA()
                            };
    DWORD *pdwBits;
    COLORREF crReturned;

    CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
    CheckNotRESULT(NULL, hbmp = myCreateDIBSection(hdc, (VOID **) &pdwBits, 32, 1, 1, DIB_RGB_COLORS, NULL, FALSE));
    CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));

    CheckNotRESULT(FALSE, PatBlt(hdcCompat, 0, 0, zx, zy, WHITENESS));

    // verify that calling GetPixel on a surface which does contain an alpha value
    for(int i = 0; i < countof(cr); i++)
    {
        // the color above is in RGB format, 32bpp DIB's store in BGR format.
        *pdwBits = SwapRGB(cr[i]);
        // Get Pixel should truncate the alpha.
        crReturned =  GetPixel(hdcCompat, 0, 0);

        // GetPixel should truncate the Alpha.
        if(crReturned != (cr[i] & 0x00FFFFFF))
        {
            info(FAIL, TEXT("ColorReturned 0x%08x not equal to the color expected 0x%08x, case %d"), crReturned, cr[i] & 0x00FFFFFF, i);
        }
    }

    CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
    CheckNotRESULT(FALSE, DeleteObject(hbmp));
    CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
    myReleaseDC(hdc);
    SetSurfVerify(bOldVerify);
}


void
SetPixelAlphaDIBTest()
{
    info(ECHO, TEXT("SetPixelAlphaDIBTest"));

    // we're directly modifying the lpBits pointer so we need to disable surface verification.
    BOOL bOldVerify = SetSurfVerify(FALSE);
    TDC hdc = myGetDC();
    TDC hdcCompat;
    TBITMAP hbmp, hbmpStock;
    COLORREF cr[] = {
                            RGBA(0, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(1, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(2, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(127, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(128, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(129, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(253, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(254, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(255, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            randColorRefA()
                            };
    DWORD * pdwBits;
    COLORREF crReturned, crSet;

    CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
    CheckNotRESULT(NULL, hbmp = myCreateDIBSection(hdc, (VOID **) &pdwBits, 32, 1, 1, DIB_RGB_COLORS, NULL, FALSE));
    CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));

    CheckNotRESULT(FALSE, PatBlt(hdcCompat, 0, 0, zx, zy, WHITENESS));

    for(int i = 0; i < countof(cr); i++)
    {
        pdwBits[0] = 0;
        crSet = SetPixel(hdcCompat, 0, 0, cr[i]);
        crReturned = SwapRGB(pdwBits[0]);

        // Desktop sets pixel values randomly to 0 if alpha is set.
        if(crSet != 0x0 && crReturned != (cr[i] & 0x00FFFFFF))
        {
            info(FAIL, TEXT("ColorSet 0x%08x not equal to the color expected 0x%08x, case %d"), crSet, cr[i] & 0x00FFFFFF, i);
        }
    }

    CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
    CheckNotRESULT(FALSE, DeleteObject(hbmp));
    CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
    myReleaseDC(hdc);
    SetSurfVerify(bOldVerify);
}

void
BltAlphaDIBTest(int testFunc)
{
    info(ECHO, TEXT("%s - BltAlphaDIBTest"), funcName[testFunc]);
    NOPRINTERDCV(NULL);

    // we're directly modifying the lpBits pointer so we need to disable surface verification.
    BOOL bOldVerify = SetSurfVerify(FALSE);
    TDC hdc = myGetDC();
    TDC hdcCompat;
    TBITMAP hbmp, hbmpStock, hbmpMask;
    HBRUSH hbr = NULL, hbrStock;
    COLORREF cr[] = {
                            RGBA(0, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(1, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(2, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(127, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(128, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(129, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(253, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(254, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            RGBA(255, GenRand() % 256, GenRand() % 256, GenRand() % 256),
                            randColorRefA()
                            };
    DWORD * pdwBits;

    // This is a 2x2 square, with every pixel set to 1 so it uses the foreground ROP (SRCCOPY)
    BYTE nMaskInit[2][2] = { {0xB0}, {0xB0} };

    CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
    CheckNotRESULT(NULL, hbmp = myCreateDIBSection(hdc, (VOID **) &pdwBits, 32, 2, -2, DIB_RGB_COLORS, NULL, FALSE));
    CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));
    CheckNotRESULT(NULL, hbmpMask = myCreateBitmap(2, 2, 1, 1, nMaskInit));

    CheckNotRESULT(FALSE, PatBlt(hdcCompat, 0, 0, zx, zy, WHITENESS));

    for(int i = 0; i < countof(cr); i++)
    {
        if(!g_bRTL)
            pdwBits[0] = cr[i];
        else
            pdwBits[1] = cr[i];

        switch(testFunc)
        {
            case EBitBlt:
                CheckNotRESULT(FALSE, BitBlt(hdcCompat, 1, 0, 1, 1, hdcCompat, 0, 0, SRCCOPY));
                break;
            case EMaskBlt:
                CheckNotRESULT(FALSE, MaskBlt(hdcCompat, 1, 0, 1, 1, hdcCompat, 0, 0, hbmpMask, 0, 0, MAKEROP4(SRCCOPY, BLACKNESS)));
                break;
            case EStretchBlt:
                CheckNotRESULT(FALSE, StretchBlt(hdcCompat, 1, 0, 1, 1, hdcCompat, 0, 0, 1, 1, SRCCOPY));
                break;
            case ETransparentImage:
                // make the transparent color anything but the color we're using.
                CheckNotRESULT(FALSE, TransparentBlt(hdcCompat, 1, 0, 1, 1, hdcCompat, 0, 0, 1, 1, ~cr[i]));
                break;
            case EPatBlt:
                // Solid and pattern brushes with alpha values have undefined behavior, so we ignore the results.

                info(DETAIL, TEXT("PatBlt AlphaDIBTest with a Solid brush"));
                // Test PatBlt with a solid brush that contains an alpha
                // when creating the brush, compensate for RGB/BGR order.
                CheckNotRESULT(NULL, hbr = CreateSolidBrush(SwapRGB(cr[i])));
                CheckNotRESULT(NULL, hbrStock = (HBRUSH) SelectObject(hdcCompat, hbr));
                CheckNotRESULT(FALSE, PatBlt(hdcCompat, 1, 0, 1, 1, PATCOPY));
                CheckNotRESULT(NULL, SelectObject(hdcCompat, hbrStock));
                CheckNotRESULT(FALSE, DeleteObject(hbr));

                info(DETAIL, TEXT("PatBlt AlphaDIBTest with a Pattern brush"));
                // Test PatBlt with a pattern brush that contains an alpha
                pdwBits[0] = cr[i];
                CheckNotRESULT(NULL, hbr = CreatePatternBrush(hbmp));
                CheckNotRESULT(NULL, hbrStock = (HBRUSH) SelectObject(hdcCompat, hbr));
                CheckNotRESULT(FALSE, PatBlt(hdcCompat, 1, 0, 1, 1, PATCOPY));
                CheckNotRESULT(NULL, SelectObject(hdcCompat, hbrStock));
                CheckNotRESULT(FALSE, DeleteObject(hbr));
                break;
        }

        // TransparentImage truncates the alpha.
        if(testFunc == ETransparentImage)
        {
            if(!g_bRTL)
            {
                if(cr[i] != pdwBits[1])
                    info(FAIL, TEXT("Expected (0,1) = 0x%08x, got 0x%08x"), cr[i] & 0x00FFFFFF, pdwBits[1]);
            }
            else
            {
                if(cr[i] != pdwBits[0])
                    info(FAIL, TEXT("Expected (0,1) = 0x%08x, got 0x%08x"), cr[i] & 0x00FFFFFF, pdwBits[0]);
            }
        }
        else if(testFunc != EPatBlt)
        {
            if(!g_bRTL)
            {
                if(cr[i] != pdwBits[1])
                    info(FAIL, TEXT("Expected (0,1) = 0x%08x, got 0x%08x"), cr[i], pdwBits[1]);
            }
            else
            {
                if(cr[i] != pdwBits[0])
                    info(FAIL, TEXT("Expected (0,1) = 0x%08x, got 0x%08x"), cr[i], pdwBits[0]);
            }
        }
    }

    CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
    CheckNotRESULT(FALSE, DeleteObject(hbmp));
    CheckNotRESULT(FALSE, DeleteObject(hbmpMask));
    CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
    myReleaseDC(hdc);
    SetSurfVerify(bOldVerify);
}

void
ShapeColorTest(int testFunc, BOOL bAlpha)
{
    info(ECHO, TEXT("%s - ShapeColorTest %s alpha"),
        funcName[testFunc],bAlpha?TEXT("with"):TEXT("without"));

    NOPRINTERDCV(NULL);

    TDC hdc = myGetDC();
    HPEN hpn, hpnStock;
    COLORREF cr = bAlpha?randColorRefA():randColorRef();
    COLORREF crBackground;
    COLORREF crResult, crExpected;
    int nOldRop2;
    BYTE ExpectedR, ExpectedG, ExpectedB;
    int nROP2Index;
    POINT Points[] = {{0, 0}, {50, 50}};
    int nPointCount = countof(Points);

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // This test is not valid when paletted, as paletted ROP's use indicies instead of colors.
        if(myGetBitDepth() > 8)
        {
            CheckNotRESULT(NULL, hpn = CreatePen(PS_SOLID, 50, cr));
            CheckNotRESULT(NULL, hpnStock = (HPEN) SelectObject(hdc, hpn));

            for(nROP2Index = 0; nROP2Index < countof(gnvRop2Array); nROP2Index++)
            {
                // realize the pen color to the surface
                CheckNotRESULT(CLR_INVALID, SetPixel(hdc, 0, 0, cr));
                CheckNotRESULT(CLR_INVALID, cr = GetPixel(hdc, 0, 0));

                FillSurface(hdc, TRUE);

                // get the Dest color.
                CheckNotRESULT(CLR_INVALID, crBackground = GetPixel(hdc, 0, 0));

                CheckNotRESULT(0, nOldRop2 = SetROP2(hdc, gnvRop2Array[nROP2Index].dwVal));

                switch(testFunc)
                {
                    case EEllipse:
                        CheckNotRESULT(FALSE, Ellipse(hdc, 0, 0, 50, 50));
                        CheckNotRESULT(CLR_INVALID, crResult = GetPixel(hdc, 10, 10));
                        break;
                    case ERectangle:
                        CheckNotRESULT(FALSE, Rectangle(hdc, 0, 0, 50, 50));
                        CheckNotRESULT(CLR_INVALID, crResult = GetPixel(hdc, 10, 10));
                        break;
                    case ERoundRect:
                        CheckNotRESULT(FALSE, RoundRect(hdc, 0, 0, 50, 50, 50, 50));
                        CheckNotRESULT(CLR_INVALID, crResult = GetPixel(hdc, 10, 10));
                        break;
                    case EPolygon:
                        CheckNotRESULT(FALSE, Polygon(hdc, Points, nPointCount));
                        CheckNotRESULT(CLR_INVALID, crResult = GetPixel(hdc, 10, 10));
                        break;
                    case EPolyline:
                        CheckNotRESULT(FALSE, Polyline(hdc, Points, nPointCount));
                        CheckNotRESULT(CLR_INVALID, crResult = GetPixel(hdc, 10, 10));
                        break;
                    case ELineTo:
                        CheckNotRESULT(FALSE, MoveToEx(hdc, Points[0].x, Points[0].y, NULL));
                        CheckNotRESULT(FALSE, LineTo(hdc, Points[1].x, Points[1].y));
                        CheckNotRESULT(CLR_INVALID, crResult = GetPixel(hdc, 10, 10));
                        break;
                }

                ExpectedR = GetROP2ChannelValue(GetRValue(crBackground), GetRValue(cr), gnvRop2Array[nROP2Index].dwVal);
                ExpectedG = GetROP2ChannelValue(GetGValue(crBackground), GetGValue(cr), gnvRop2Array[nROP2Index].dwVal);
                ExpectedB = GetROP2ChannelValue(GetBValue(crBackground), GetBValue(cr), gnvRop2Array[nROP2Index].dwVal);

                crExpected = RGB(ExpectedR, ExpectedG, ExpectedB);

                // Alpha's in pens results in the same unexpected behavior as alpha with other functions.
                // Don't fail when testing alpha.
                if(!bAlpha && crResult != crExpected)
                    info(FAIL, TEXT("Expected color 0x%08x, returned color 0x%08x"), crExpected, crResult);

                CheckNotRESULT(0, SetROP2(hdc, nOldRop2));
            }

            CheckNotRESULT(NULL, SelectObject(hdc, hpnStock));
            CheckNotRESULT(FALSE, DeleteObject(hpn));
        }
    }

    myReleaseDC(hdc);
}

void
AlphaBlendRandomTest(int testFunc)
{
    info(ECHO, TEXT("%s - AlphaBlendRandomTest"), funcName[testFunc]);
    NOPRINTERDCV(NULL);

    TDC hdc = myGetDC();
    BLENDFUNCTION bf;
    RECT rcSource, rcDest;
    BOOL bExpectedReturnValue, bActualReturnValue;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        FillSurface(hdc);
        drawLogo(hdc, 150);
        CheckNotRESULT(FALSE, BitBlt(hdc, zx/2, 0, zx/2, zy, hdc, 0, 0, SRCCOPY));

        for(int i = 0; i < (10*testCycles); i++)
        {
            // pick a random constant alpha to blend with.
            bf.SourceConstantAlpha = GenRand() % 0xFF;
            bf.BlendOp = GenRand() % 2;
            bf.BlendFlags = GenRand() % 2;
            bf.AlphaFormat = GenRand() % 3;

            randomRectWithinArea(0, 0, zx, zy, &rcSource);
            randomRectWithinArea(0, 0, zx, zy, &rcDest);

            // if either rectangle is a flip/mirror, then the call should fail
            if(rcDest.right - rcDest.left < 0 || rcDest.bottom - rcDest.top < 0 ||
               rcSource.right - rcSource.left < 0 || rcSource.bottom - rcSource.top < 0)
                bExpectedReturnValue = FALSE;
// CE allows overlapping, XP doesn't.
#ifndef UNDER_CE
            else if(overlap(&rcSource, &rcDest, FALSE))
                bExpectedReturnValue = FALSE;
#else
            // XP allows random blend flags, CE doesn't.
            else if(bf.BlendFlags != 0)
                bExpectedReturnValue = FALSE;
#endif
            else if(bf.BlendOp != AC_SRC_OVER)
                bExpectedReturnValue = FALSE;
            else if(bf.AlphaFormat != 0 && bf.AlphaFormat != AC_SRC_ALPHA)
                bExpectedReturnValue = FALSE;
            else if(bf.AlphaFormat == AC_SRC_ALPHA && myGetBitDepth() != 32)
                bExpectedReturnValue = FALSE;
            else bExpectedReturnValue = TRUE;

            SetLastError(ERROR_SUCCESS);
            bActualReturnValue = gpfnAlphaBlend(hdc, rcDest.left, rcDest.top, rcDest.right - rcDest.left, rcDest.bottom - rcDest.top,
                                                               hdc, rcSource.left, rcSource.top, rcSource.right - rcSource.left, rcSource.bottom - rcSource.top, bf);

            if(bExpectedReturnValue != bActualReturnValue)
            {
                info(FAIL, TEXT("Alphablend returned %d when expected to return %d"), bActualReturnValue, bExpectedReturnValue);
                info(FAIL, TEXT("Rectangles in use were Dest:L:%d T:%d R:%d B:%d, Source:L:%d T:%d R:%d B:%d and were %soverlapping"), rcDest.left, rcDest.top, rcDest.right, rcDest.bottom,
                    rcSource.left, rcSource.top, rcSource.right, rcSource.bottom, overlap(&rcSource, &rcDest, FALSE)?TEXT(""):TEXT("not "));
                info(FAIL, TEXT("Blend function SourceConstantAlpha: 0x%08x BlendOp: 0x%08x, BlendFlags: 0x%08x, AlphaFormat: 0x%08x"), bf.SourceConstantAlpha, bf.BlendOp, bf.BlendFlags, bf.AlphaFormat);
            }

            if(bExpectedReturnValue == FALSE)
                CheckForLastError(ERROR_INVALID_PARAMETER);
        }
    }

    myReleaseDC(hdc);
}

struct sCoordinates {
        int dleft;
        int dtop;
        int dwidth;
        int dheight;
        int sleft;
        int stop;
        int swidth;
        int sheight;
};

void
AlphaBlendGoodRectTest(int testFunc)
{
    info(ECHO, TEXT("%s - AlphaBlendGoodRectTest"), funcName[testFunc]);
    NOPRINTERDCV(NULL);

    TDC hdc = myGetDC();
    BLENDFUNCTION bf;
    BOOL bActualReturnValue;
    struct sCoordinates nCoord[] = {    // adjacent cases
                                                      { 10, 10, 10, 10, 0, 0, 10, 10 }, // 0
                                                      { 10, 10, 10, 10, 0, 0, zx, 10 }, // 1
                                                      { 10, 10, 10, 10, 0, 0, 10, zy }, // 2
                                                      { 10, 10, 10, 10, 20, 0, 10, zy }, // 3
                                                      { 10, 10, 10, 10, 0, 20, zx, 0 }, // 4
                                                      // source clipping cases
                                                      { 20, 20, 10, 10, zx-10, 0, 10, 10 }, // 5
                                                      { 20, 20, 10, 10, 0, zy-10, 10, 10 }, // 6
                                                      { 20, 20, 10, 10, zx-10, zy-10, 10, 10 }, // 7
                                                      // empty rectangles
                                                      { 20, 20, 10, 10, 0, 0, 0, 10 }, // 8
                                                      { 20, 20, 10, 10, 0, 0, 10, 0 }, // 9
                                                      { 20, 20, 0, 10, 0, 0, 10, 10 }, // 10
                                                      { 20, 20, 10, 0, 0, 0, 10, 10 }, // 11
                                                      };

    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = 0xFF;
    bf.BlendFlags = 0;
    bf.AlphaFormat = 0;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        for(int i = 0; i < countof(nCoord); i++)
        {
            bActualReturnValue = gpfnAlphaBlend(hdc, nCoord[i].dleft, nCoord[i].dtop, nCoord[i].dwidth, nCoord[i].dheight,
                                                                    hdc, nCoord[i].sleft, nCoord[i].stop, nCoord[i].swidth, nCoord[i].sheight, bf);

            if(!bActualReturnValue)
            {
                info(FAIL, TEXT("Rectangle case %d failed when expected to succeed."), i);
                info(FAIL, TEXT("dest - left: %d top: %d width: %d height: %d"), nCoord[i].dleft, nCoord[i].dtop, nCoord[i].dwidth, nCoord[i].dheight);
                info(FAIL, TEXT("source - left: %d top: %d width: %d height: %d"), nCoord[i].sleft, nCoord[i].stop, nCoord[i].swidth, nCoord[i].sheight);
            }
        }
    }

    myReleaseDC(hdc);
}

void
AlphaBlendBadRectTest(int testFunc)
{
    info(ECHO, TEXT("%s - AlphaBlendBadRectTest"), funcName[testFunc]);
    NOPRINTERDCV(NULL);

    TDC hdc = myGetDC();
    BLENDFUNCTION bf;
    BOOL bActualReturnValue;
    struct sCoordinates nCoord[] = {    // overlapping cases.  CE allows overlapping.
        /*
        { 10, 10, 10, 10, 10, 10, 10, 10 }, // 0
        { 10, 10, 10, 10, 9, 10, 10, 10 }, // 1
        { 10, 10, 10, 10, 10, 9, 10, 10 }, // 2
        { 10, 10, 10, 10, 9, 9, 10, 10 }, // 3
        { 10, 10, 10, 10, 11, 10, 10, 10 }, // 4
        { 10, 10, 10, 10, 10, 11, 10, 10 }, // 5
        { 10, 10, 10, 10, 11, 11, 10, 10 }, // 6
        { 9, 10, 10, 10, 10, 10, 10, 10 }, // 7
        { 10, 9, 10, 10, 10, 10, 10, 10 }, // 8
        { 9, 9, 10, 10, 10, 10, 10, 10 }, // 9
        { 11, 10, 10, 10, 10, 10, 10, 10 }, // 10
        { 10, 11, 10, 10, 10, 10, 10, 10 }, // 11
        { 11, 11, 10, 10, 10, 10, 10, 10 }, // 12
        { 10, 10, 10, 10, 1, 10, 10, 10 }, // 13
        { 10, 10, 10, 10, 10, 1, 10, 10 }, // 14
        { 10, 10, 10, 10, 1, 1, 10, 10 }, // 15
        { 10, 10, 10, 10, 19, 10, 10, 10 }, // 16
        { 10, 10, 10, 10, 10, 19, 10, 10 }, // 17
        { 10, 10, 10, 10, 19, 19, 10, 10 }, // 18
        { 1, 10, 10, 10, 10, 10, 10, 10 }, // 19
        { 10, 1, 10, 10, 10, 10, 10, 10 }, // 20
        { 1, 1, 10, 10, 10, 10, 10, 10 }, // 21
        { 19, 10, 10, 10, 10, 10, 10, 10 }, // 22
        { 10, 19, 10, 10, 10, 10, 10, 10 }, // 23
        { 19, 19, 10, 10, 10, 10, 10, 10 }, // 24
        { 10, 10, 10, 10, 0, 0, 11, 11 }, // 25
        { 10, 10, 10, 10, 0, 0, zx, 11 }, // 26
        { 10, 10, 10, 10, 0, 0, 11, zy }, // 27
        { 10, 10, 10, 10, 19, 0, 10, zy }, // 28
        { 10, 10, 10, 10, 0, 19, zx, 10 }, // 29
        */
        // source clipping cases
        { 20, 20, 10, 10, -1, 0, 10, 10 }, // 0 <= referred to below.
        { 20, 20, 10, 10, 0, -1, 10, 10 }, // 1
        { 20, 20, 10, 10, -1, -1, 10, 10 }, // 2
        { 20, 20, 10, 10, zx-9, 0, 10, 10 }, // 3
        { 20, 20, 10, 10, 0, zy-1, 40, 40 }, // 4 // must have the width/height increased for the task bar.
        { 20, 20, 10, 10, zx-9, zy-9, 10, 10 }, // 5
        // negative widths/heights
        { 10, 10, 10, 10, 10, 10, 10, -10 }, // 6
        { 10, 10, 10, 10, 10, 10, -10, 10 }, // 7
        { 10, 10, 10, 10, 10, 10, -10, -10 }, // 8
        { 10, 10, 10, -10, 10, 10, 10, 10 }, // 9
        { 10, 10, 10, -10, 10, 10, 10, -10 }, // 10
        { 10, 10, 10, -10, 10, 10, -10, 10 }, // 11
        { 10, 10, 10, -10, 10, 10, -10, -10 }, // 12
        { 10, 10, -10, 10, 10, 10, 10, 10 }, // 13
        { 10, 10, -10, 10, 10, 10, 10, -10 }, // 14
        { 10, 10, -10, 10, 10, 10, -10, 10 }, // 15
        { 10, 10, -10, 10, 10, 10, -10, -10 }, // 16
        { 10, 10, -10, -10, 10, 10, 10, 10 }, // 17
        { 10, 10, -10, -10, 10, 10, 10, -10 }, // 18
        { 10, 10, -10, -10, 10, 10, -10, 10 }, // 19
        { 10, 10, -10, -10, 10, 10, -10, -10 } // 20
        };

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        bf.BlendOp = AC_SRC_OVER;
        bf.SourceConstantAlpha = 0xFF;
        bf.BlendFlags = 0;
        bf.AlphaFormat = 0;

        for(int i = 0; i < countof(nCoord); i++)
        {
            // if we're testing on a window DC, the coordinates are adjusted by the window location,
            // which makes finding the true edges of the surface difficult.
            if(isWinDC() && i >= 0 && i <=5)
                continue;

            bActualReturnValue = gpfnAlphaBlend(hdc, nCoord[i].dleft, nCoord[i].dtop, nCoord[i].dwidth, nCoord[i].dheight,
                                                                    hdc, nCoord[i].sleft, nCoord[i].stop, nCoord[i].swidth, nCoord[i].sheight, bf);

            if(bActualReturnValue)
            {
                info(FAIL, TEXT("Rectangle case %d succeeded when expected to fail."), i);
                info(FAIL, TEXT("dest - left: %d top: %d width: %d height: %d"), nCoord[i].dleft, nCoord[i].dtop, nCoord[i].dwidth, nCoord[i].dheight);
                info(FAIL, TEXT("source - left: %d top: %d width: %d height: %d"), nCoord[i].sleft, nCoord[i].stop, nCoord[i].swidth, nCoord[i].sheight);
            }
            else
                CheckForLastError(ERROR_INVALID_PARAMETER);
        }
    }

    myReleaseDC(hdc);
}


// Alpha blend test values are located in "Alphablend.h"
void
AlphaBlendConstAlphaTest(int testFunc)
{
    info(ECHO, TEXT("%s - AlphaBlendConstAlphaTest"), funcName[testFunc]);

    NOPRINTERDCV(NULL);

    // non-paletted surfaces and dibs have determinstic color behavior.
    // the system palette could be anything...
    if(myGetBitDepth() > 8 || (isPalDIBDC() && !isSysPalDIBDC()))
    {
        TDC hdc = myGetDC();
        BLENDFUNCTION bf;
        COLORREF crSource, crDest, crExpected, crResult;

        if (!IsWritableHDC(hdc))
            info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
        else
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

            bf.BlendOp = AC_SRC_OVER;
            bf.BlendFlags = 0;
            bf.AlphaFormat = 0;

            for(int i = 0; i < countof(g_stcConstAlpha); i++)
            {
                crSource = g_stcConstAlpha[i].crSource;
                crDest = g_stcConstAlpha[i].crDest;
                bf.SourceConstantAlpha = g_stcConstAlpha[i].alpha;

                CheckNotRESULT(CLR_INVALID, crDest = SetPixel(hdc, 0, 0, crDest));
                CheckNotRESULT(CLR_INVALID, crSource = SetPixel(hdc, 1, 1, crSource));

                // alphablend from the source to the dest applying the constant alpha
                // this call should never fail.
                CheckNotRESULT(FALSE, gpfnAlphaBlend(hdc, 0, 0, 1, 1, hdc, 1, 1, 1, 1, bf));

                CheckNotRESULT(CLR_INVALID, crResult = GetPixel(hdc, 0, 0));

                if(myGetBitDepth() == 1)
                    crExpected = g_stcConstAlpha[i].crExpected1;
                else if(myGetBitDepth() == 2)
                    crExpected = g_stcConstAlpha[i].crExpected2;
                else if(myGetBitDepth() == 4)
                    crExpected = g_stcConstAlpha[i].crExpected4;
                else if(myGetBitDepth() == 8)
                    crExpected = g_stcConstAlpha[i].crExpected8;
                else if(myGetBitDepth() == 16)
                {
                    DWORD dwBitMask = (gdwShiftedRedBitMask | gdwShiftedGreenBitMask | gdwShiftedBlueBitMask);
                    // this test only has values for 565 and 555 pixel formats, no data for 444
                    if(dwBitMask == 0x00F8FCF8 || dwBitMask == 0x00F8F8F8)
                    {
                        crExpected =  g_stcConstAlpha[i].crExpected16 & dwBitMask;
                        crResult &= dwBitMask;
                    }
                    else
                        crExpected = crResult;
                }
                else crExpected =  g_stcConstAlpha[i].crExpected;

                if(crExpected != crResult)
                {
                    info(FAIL, TEXT("Alphablend expected 0x%08x, actual 0x%08x, case %d"), crExpected, crResult, i);
                    info(FAIL, TEXT("Source 0x%08x, dest 0x%08x, SCA 0x%08x, case: %d"), crSource, crDest, bf.SourceConstantAlpha, i);
                }
            }
        }

        myReleaseDC(hdc);
    }
}

// Alpha blend test values are located in "Alphablend.h"
void
AlphaBlendPerPixelAlphaToPrimaryTest(int testFunc)
{
    info(ECHO, TEXT("%s - AlphaBlendPerPixelAlphaToPrimaryTest"), funcName[testFunc]);

    // non-paletted surfaces and dibs have determinstic color behavior.
    // the system palette could be anything...
    // XP doesn't do saturated math for offscreen surfaces...
#ifdef UNDER_CE
    if(myGetBitDepth() > 8 || (isPalDIBDC() && !isSysPalDIBDC()))
    {
        // we're directly modifying the lpBits pointer so we need to disable surface verification.
        BOOL bOldVerify = SetSurfVerify(FALSE);
        TDC hdcPrimary = myGetDC();
        TDC hdc;
        TBITMAP hbmp, hbmpStock;
        BLENDFUNCTION bf;
        DWORD *pdwBits;
        COLORREF crExpected, crSource, crDest, crResult;

        if (!IsWritableHDC(hdcPrimary))
            info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
        else
        {
            CheckNotRESULT(FALSE, PatBlt(hdcPrimary, 0, 0, zx, zy, WHITENESS));

            bf.BlendOp = AC_SRC_OVER;
            bf.AlphaFormat = AC_SRC_ALPHA;
            bf.BlendFlags = 0;

            // Create a 32bpp DIB, which is the only surface alpha's really apply to.
            CheckNotRESULT(NULL, hdc = CreateCompatibleDC(hdcPrimary));
            CheckNotRESULT(NULL, hbmp = myCreateDIBSection(hdc, (VOID **) &pdwBits, 32, 1, 1, DIB_RGB_COLORS, NULL, FALSE));
            CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdc, hbmp));

            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

            for(int i = 0; i < countof(g_stcPPAlphaPrimary); i++)
            {
                crSource = pdwBits[0] = g_stcPPAlphaPrimary[i].crSource;
                bf.SourceConstantAlpha = g_stcPPAlphaPrimary[i].alpha;

                // SetPixel fails when alpha's are involved (as expected), so remove the alpha.
                CheckNotRESULT(CLR_INVALID, crDest = SetPixel(hdcPrimary, 0, 0, g_stcPPAlphaPrimary[i].crDest & 0x00FFFFFF));

                // alphablend from the source to the dest applying the constant alpha and the per pixel alpha
                CheckNotRESULT(FALSE, gpfnAlphaBlend(hdcPrimary, 0, 0, 1, 1, hdc, 0, 0, 1, 1, bf));

                // retrieving the pixel value on a printer DC will fail
                if (!isPrinterDC(hdcPrimary))
                {

                    CheckNotRESULT(CLR_INVALID, crResult = GetPixel(hdcPrimary, 0, 0));

                    if(myGetBitDepth() == 1)
                        crExpected = g_stcPPAlphaPrimary[i].crExpected1;
                    else if(myGetBitDepth() == 2)
                        crExpected = g_stcPPAlphaPrimary[i].crExpected2;
                    else if(myGetBitDepth() == 4)
                        crExpected = g_stcPPAlphaPrimary[i].crExpected4;
                    else if(myGetBitDepth() == 8)
                        crExpected = g_stcPPAlphaPrimary[i].crExpected8;
                    else if(myGetBitDepth() == 16)
                    {
                        DWORD dwBitMask = (gdwShiftedRedBitMask | gdwShiftedGreenBitMask | gdwShiftedBlueBitMask);
                        // this test only has values for 565 and 555 pixel formats, no data for 444
                        if(dwBitMask == 0x00F8FCF8 || dwBitMask == 0x00F8F8F8)
                        {
                            crExpected =  g_stcPPAlphaPrimary[i].crExpected16 & dwBitMask;
                            crResult &= dwBitMask;
                        }
                        else
                            crExpected =  crResult;
                    }
                    else crExpected =  g_stcPPAlphaPrimary[i].crExpected;

                    if(crExpected != crResult)
                    {
                        info(FAIL, TEXT("Alphablend expected 0x%08x, actual 0x%08x, case %d"), crExpected, crResult, i);
                        info(FAIL, TEXT("Source 0x%08x, dest 0x%08x, SCA 0x%08x, case: %d"), crSource, crDest, bf.SourceConstantAlpha, i);
                    }
                }
            }

            CheckNotRESULT(NULL, SelectObject(hdc, hbmpStock));
            CheckNotRESULT(FALSE, DeleteObject(hbmp));
            CheckNotRESULT(FALSE, DeleteDC(hdc));
        }

        myReleaseDC(hdcPrimary);
        SetSurfVerify(bOldVerify);
    }
#endif
}

// Alpha blend test values are located in "Alphablend.h"
void
AlphaBlendPerPixelAlphaTest(int testFunc, BOOL bTopDown)
{
    info(ECHO, TEXT("%s - AlphaBlendPerPixelAlphaTest %s"), funcName[testFunc], bTopDown?TEXT("Top Down"):TEXT("Bottom Up"));
    NOPRINTERDCV(NULL);
// XP doesn't do saturated math for offscreen surfaces...
#ifdef UNDER_CE
    // we're directly modifying the lpBits pointer so we need to disable surface verification.
    BOOL bOldVerify = SetSurfVerify(FALSE);
    TDC hdcPrimary = myGetDC();
    TDC hdc;
    TBITMAP hbmp, hbmpStock;
    BLENDFUNCTION bf;
    DWORD *pdwBits;
    COLORREF crExpected, crSource, crDest;

    bf.BlendOp = AC_SRC_OVER;
    bf.AlphaFormat = AC_SRC_ALPHA;
    bf.BlendFlags = 0;

    // Create a 32bpp DIB, which is the only surface alpha's really apply to.
    CheckNotRESULT(NULL, hdc  = CreateCompatibleDC(hdcPrimary));
    CheckNotRESULT(NULL, hbmp = myCreateDIBSection(hdc, (VOID **) &pdwBits, 32, 2, bTopDown?-2:2, DIB_RGB_COLORS, NULL, FALSE));
    CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdc, hbmp));

    CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

    for(int i = 0; i < countof(g_stcPPAlpha); i++)
    {
        // pdwBits[1] is the source when top down, 3 is the source when bottom up
        crSource = pdwBits[bTopDown?1:3] = g_stcPPAlpha[i].crSource;
        // pdwBits[0] is the dest when top down, 2 is the dest when bottom up
        crDest = pdwBits[bTopDown?0:2] = g_stcPPAlpha[i].crDest;
        crExpected = g_stcPPAlpha[i].crExpected;
        bf.SourceConstantAlpha = g_stcPPAlpha[i].alpha;

        // alphablend from the source to the dest applying the constant alpha
        if(!g_bRTL)
            CheckNotRESULT(FALSE, gpfnAlphaBlend(hdc, 0, 0, 1, 1, hdc, 1, 0, 1, 1, bf));
        else
            CheckNotRESULT(FALSE, gpfnAlphaBlend(hdc, 1, 0, 1, 1, hdc, 0, 0, 1, 1, bf));

        // pdwBits[0] is the destination/result color when top down
        // pdwBits[2] is the destination/result when bottom up
        if(crExpected != pdwBits[bTopDown?0:2])
        {
            info(FAIL, TEXT("Alphablend expected 0x%08x, actual 0x%08x, case: %d"), crExpected, pdwBits[bTopDown?0:2], i);
            info(FAIL, TEXT("Source 0x%08x, dest 0x%08x, SCA 0x%08x, case: %d"), crSource, crDest, bf.SourceConstantAlpha, i);
        }

        if(crSource != pdwBits[bTopDown?1:3])
            info(FAIL, TEXT("Source was modifed by the call."));
    }

    CheckNotRESULT(NULL, SelectObject(hdc, hbmpStock));
    CheckNotRESULT(FALSE, DeleteObject(hbmp));
    CheckNotRESULT(FALSE, DeleteDC(hdc));
    myReleaseDC(hdcPrimary);
    SetSurfVerify(bOldVerify);
#endif
}

void
CreateDIBSection24bppDIBTest(int testFunc)
{
    info(ECHO, TEXT("%s - CreateDIBSection24bppDIBTest"), funcName[testFunc]);
    TDC hdc = myGetDC();
    TDC hdcCompat;
    TBITMAP hbmp, hbmpStock;
    DWORD *pdwBits;

    for(int i = RGBFIRST; i < RGBLAST; i++)
    {
        CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
        if(hdcCompat)
        {
            CheckNotRESULT(NULL, hbmp = myCreateRGBDIBSection(hdc, (VOID **) &pdwBits, 24, 2, -2, i, BI_BITFIELDS, NULL));
            if(hbmp)
            {
                CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));

                // preset the whole surface to black, so pixel 0, 1 doesn't interfere with the dword data for pixel 0,0
                PatBlt(hdcCompat, 0, 0, 2, 2, BLACKNESS);

                int x_offset;
                if(!g_bRTL)
                    x_offset = 0;
                else
                    x_offset = 1;
                CheckNotRESULT(CLR_INVALID, SetPixel(hdcCompat, x_offset, 0, RGB(0xFF, 0x0, 0x0)));
                if(pdwBits[0] != 0xFF0000)
                    info(FAIL, TEXT("Case %d Expected BGR red, returned 0x%08x"), i, pdwBits[0]);

                CheckNotRESULT(CLR_INVALID, SetPixel(hdcCompat,x_offset, 0, RGB(0x0, 0xFF, 0x0)));
                if(pdwBits[0] != 0x00FF00)
                    info(FAIL, TEXT("Case %d Expected BGR green, returned 0x%08x"), i, pdwBits[0]);

                CheckNotRESULT(CLR_INVALID, SetPixel(hdcCompat, x_offset, 0, RGB(0x0, 0x0, 0xFF)));
                if(pdwBits[0] != 0x0000FF)
                    info(FAIL, TEXT("Case %d Expected BGR blue, returned 0x%08x"), i, pdwBits[0]);

                CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
                CheckNotRESULT(FALSE, DeleteObject(hbmp));

            }
            CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
        }
    }

    myReleaseDC(hdc);
}

void
BitBltPalTest(int testFunc)
{
    info(ECHO, TEXT("%s - BitBltPalTest"), funcName[testFunc]);

    TDC hdc = myGetDC();
    TDC hdcCompat;
    TBITMAP hbmp, hbmpStock;
// desktop doesn't support 2bpp.
#ifdef UNDER_CE
    int nBitDepths[] = {1, 2, 4, 8 };
#else
    int nBitDepths[] = {1, 4, 8 };
#endif
    int r=0, g=0, b=0, i;
    DWORD *pdwBits = 0;
    int nEntries;
    // 256 entries, maximum possible.
    MYRGBQUAD ct[256];
    HBRUSH hbr, hbrExpected;
    // the coordinates for the six test boxes.
    RECT rcCoordinates[5] = {{0}};
    RECT rcTmp;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // initialize the 5 rectangles.
        SetRect(&rcCoordinates[0], 1*(zx/10), 1*(zy/7), 1*(zx/10) + zx/10, 1*(zy/7) + zy/7);
        SetRect(&rcCoordinates[1], 3*(zx/10), 1*(zy/7), 3*(zx/10) + zx/10, 1*(zy/7) + zy/7);
        SetRect(&rcCoordinates[2], 1*(zx/10), 3*(zy/7), 1*(zx/10) + zx/10, 3*(zy/7) + zy/7);
        SetRect(&rcCoordinates[3], 3*(zx/10), 3*(zy/7), 3*(zx/10) + zx/10, 3*(zy/7) + zy/7);
        SetRect(&rcCoordinates[4], 1*(zx/10), 5*(zy/7), 1*(zx/10) + zx/10, 5*(zy/7) + zy/7);

        for(int j = 0; j < countof(nBitDepths); j++)
        {
            nEntries = (int) pow((double)2, (double)(nBitDepths[j]));
            int nEntriesToModify[] = {0, 1, nEntries/2 - 1, nEntries/2, nEntries -2 };

            CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
            // create a paletted DIB with the multiple entries of the same color.
            hbmp = myCreateDIBSection(hdc, (VOID **) &pdwBits, nBitDepths[j], zx/2, zy, DIB_RGB_COLORS, NULL, FALSE);
            CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));
            CheckForRESULT(nEntries, GetDIBColorTable(hdcCompat, 0, nEntries, (RGBQUAD *) ct));

            CheckNotRESULT(FALSE, PatBlt(hdcCompat, 0, 0, zx/2, zy, WHITENESS));
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

            // change the 6 entries to something random, all the same.
            r = GenRand() % 255;
            g = GenRand() % 255;
            b = GenRand() % 255;

            CheckNotRESULT(NULL, hbrExpected = CreateSolidBrush(RGB(r, g, b)));

            // draw the 5 boxes.
            for(i = 0; i < countof(rcCoordinates); i++)
            {
                rcTmp = rcCoordinates[i];
                rcTmp.left += zx/2;
                rcTmp.right += zx/2;
                CheckNotRESULT(NULL, hbr = CreateSolidBrush(GetColorRef(&ct[nEntriesToModify[i]])));
                CheckNotRESULT(FALSE, FillRect(hdcCompat, &rcCoordinates[i], hbr));
                CheckNotRESULT(FALSE, FillRect(hdc, &rcTmp, hbrExpected));
                CheckNotRESULT(FALSE, DeleteObject(hbr));
            }

            // 1bpp the entire surface is resolved to be the expected color, as expected.
            if(nBitDepths[j] == 1)
            {
                rcTmp.left = zx/2;
                rcTmp.right = zx;
                rcTmp.top = 0;
                rcTmp.bottom = zy;
                FillRect(hdc, &rcTmp, hbrExpected);
            }

            CheckNotRESULT(FALSE, DeleteObject(hbrExpected));


            for(i = 0; i < countof(nEntriesToModify); i++)
            {
                setRGBQUAD(&(ct[nEntriesToModify[i]]), r, g, b);
            }

            // change the color table to one with multiple identical entries.
            CheckForRESULT(nEntries, SetDIBColorTable(hdcCompat, 0, nEntries, (RGBQUAD *) ct));

            // do the BitBlt, the rectangles should all be changed.
            CheckNotRESULT(FALSE, BitBlt(hdc, 0, 0, zx/2, zy, hdcCompat, 0, 0, SRCCOPY));

            // verify that the image created is correct.  Unable to verify when running at 1bpp becuase more than 2
            // available colors are needed for verification.
            if(myGetBitDepth() > 1)
                CheckScreenHalves(hdc);

            CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
            DeleteObject(hbmp);
            DeleteDC(hdcCompat);
        }
    }
    myReleaseDC(hdc);
}

void
TransparentImagePalTest(int testFunc)
{
    info(ECHO, TEXT("%s - TransparentImagePalTest"), funcName[testFunc]);
    TDC hdc = myGetDC();
    TDC hdcCompat;
    TBITMAP hbmp, hbmpStock;
// desktop doesn't support 2bpp.
#ifdef UNDER_CE
    int nBitDepths[] = {1, 2, 4, 8 };
#else
    int nBitDepths[] = {1, 4, 8 };
#endif
    int r=0, g=0, b=0, i;
    DWORD *pdwBits = 0;
    int nEntries;
    // 256 entries, maximum possible.
    MYRGBQUAD ct[256];
    HBRUSH hbr, hbrExpected;
    // the coordinates for the six test boxes.
    RECT rcCoordinates[5] = {{0}};
    RECT rcTmp;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // initialize the 5 rectangles.
        SetRect(&rcCoordinates[0], 1*(zx/10), 1*(zy/7), 1*(zx/10) + zx/10, 1*(zy/7) + zy/7);
        SetRect(&rcCoordinates[1], 3*(zx/10), 1*(zy/7), 3*(zx/10) + zx/10, 1*(zy/7) + zy/7);
        SetRect(&rcCoordinates[2], 1*(zx/10), 3*(zy/7), 1*(zx/10) + zx/10, 3*(zy/7) + zy/7);
        SetRect(&rcCoordinates[3], 3*(zx/10), 3*(zy/7), 3*(zx/10) + zx/10, 3*(zy/7) + zy/7);
        SetRect(&rcCoordinates[4], 1*(zx/10), 5*(zy/7), 1*(zx/10) + zx/10, 5*(zy/7) + zy/7);

        for(int j = 0; j < countof(nBitDepths); j++)
        {
            nEntries = (int) pow((double)2, (double)(nBitDepths[j]));
            int nEntriesToModify[] = {0, 1, nEntries/2 - 1, nEntries/2, nEntries -2 };

            CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
            // create a paletted DIB with the multiple entries of the same color.
            hbmp = myCreateDIBSection(hdc, (VOID **) &pdwBits, nBitDepths[j], zx/2, zy, DIB_RGB_COLORS, NULL, FALSE);
            CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));
            CheckForRESULT(nEntries, GetDIBColorTable(hdcCompat, 0, nEntries, (RGBQUAD *) ct));

            CheckNotRESULT(FALSE, PatBlt(hdcCompat, 0, 0, zx/2, zy, WHITENESS));
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

            // change the 6 entries to something random, all the same.
            r = GenRand() % 255;
            g = GenRand() % 255;
            b = GenRand() % 255;

            CheckNotRESULT(NULL, hbrExpected = CreateSolidBrush(RGB(r, g, b)));

            // for 1bpp, we fill the whole surface with the random color, and then
            // make specific boxes white.  For all othet bit depths the pattern is the opposite.
            if(nBitDepths[j] == 1)
            {
                rcTmp.left = zx/2;
                rcTmp.right = zx;
                rcTmp.top = 0;
                rcTmp.bottom = zy;
                FillRect(hdc, &rcTmp, hbrExpected);
                CheckNotRESULT(FALSE, DeleteObject(hbrExpected));
                CheckNotRESULT(NULL, hbrExpected = (HBRUSH) GetStockObject(WHITE_BRUSH));
            }

            // draw the 5 boxes.
            for(i = 0; i < countof(rcCoordinates); i++)
            {
                rcTmp = rcCoordinates[i];
                rcTmp.left += zx/2;
                rcTmp.right += zx/2;
                CheckNotRESULT(NULL, hbr = CreateSolidBrush(GetColorRef(&ct[nEntriesToModify[i]])));
                CheckNotRESULT(FALSE, FillRect(hdcCompat, &rcCoordinates[i], hbr));
                // the first rectangle is the rectangle that will be made transparent, so don't draw it in our
                // expected result.
                if((nBitDepths[j] == 1 && (i % 2) == 0) || (nBitDepths[j] != 1 && i > 0))
                    CheckNotRESULT(FALSE, FillRect(hdc, &rcTmp, hbrExpected));
                CheckNotRESULT(FALSE, DeleteObject(hbr));
            }

            // free the brush allocated.  for 1bpp this is a stock brush which deleting is harmless.
            CheckNotRESULT(FALSE, DeleteObject(hbrExpected));

            for(i = 0; i < countof(nEntriesToModify); i++)
            {
                setRGBQUAD(&(ct[nEntriesToModify[i]]), r, g, b);
            }

            // change the color table to the one that has multiple identical entries
            CheckForRESULT(nEntries, SetDIBColorTable(hdcCompat, 0, nEntries, (RGBQUAD *) ct));

            // do the TransparentImage, only the top left rectangle should be transparent.
            CheckNotRESULT(FALSE, TransparentBlt(hdc, 0, 0, zx/2, zy, hdcCompat, 0, 0, zx/2, zy, RGB(r, g, b)));

            // there's two behaviors here.  For some hardware accelerated drivers all of the colors are
            // made transparent.  For non-hardware accelerated transparent blt's only the first color is made transparent.
            // so, if the 1st block is made white, then all of the blocks are going to be white, so clear out the right
            // half of the screen.
            COLORREF cr1 = GetPixel(hdc, rcCoordinates[2].left + (rcCoordinates[2].right - rcCoordinates[2].left)/2,
                                                            rcCoordinates[2].top + (rcCoordinates[2].bottom - rcCoordinates[2].top)/2);
            COLORREF cr2 = GetPixel(hdc, 0, 0);
            if(cr1 == cr2)
            {
                PatBlt(hdc, zx/2, 0, zx/2, zy, WHITENESS);
            }

            // verify that the image created is correct.  Unable to verify when running at 1bpp becuase more than 2
            // available colors are needed for verification.
            if(myGetBitDepth() > 1)
                CheckScreenHalves(hdc);

            CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
            DeleteObject(hbmp);
            DeleteDC(hdcCompat);
        }
    }

    myReleaseDC(hdc);
}

void
CreateCompatibleBitmapDIBSECTIONTest(int testFunc)
{
#ifdef UNDER_CE
    info(ECHO, TEXT("%s - CreateCompatibleBitmapDIBSECTIONTest"), funcName[testFunc]);

    struct MYBITMAPINFO
    {
        BITMAPINFOHEADER bmih;
        ULONG            Palette[256];
    };

    TDC hdc = myGetDC();
    TBITMAP Bmp;
    int          Size;
    DIBSECTION   DibSection;
    TBITMAP      Dib;
    MYBITMAPINFO bmi;
    BYTE *pBits = NULL;

    CheckNotRESULT(NULL, Bmp  = CreateCompatibleBitmap(hdc, 1, 1));

    if ((Size = GetObject(Bmp, sizeof (DibSection), &DibSection))
        && Size == sizeof (DibSection))
    {
        memset(&bmi, 0, sizeof (bmi));

        bmi.bmih          = DibSection.dsBmih;
        bmi.bmih.biWidth  = zx;
        bmi.bmih.biHeight = -zy;

        // The primary always BI_BITFIELDS for bpp > 8
        if (bmi.bmih.biCompression == BI_BITFIELDS)
        {
            bmi.Palette[0] = DibSection.dsBitfields[0];
            bmi.Palette[1] = DibSection.dsBitfields[1];
            bmi.Palette[2] = DibSection.dsBitfields[2];
        }

        CheckNotRESULT(NULL, Dib = CreateDIBSection(hdc, (BITMAPINFO*)&bmi, DIB_RGB_COLORS, (VOID **) &pBits, NULL, 0));
        CheckNotRESULT(FALSE, DeleteObject(Dib));
    }
    else info(FAIL, TEXT("GetObject call failed or a Dibsection wasn't returned"));

    CheckNotRESULT(FALSE, DeleteObject(Bmp));
    myReleaseDC(hdc);
#endif
}

void
FillRectSysColorTest(int testFunc)
{
    info(ECHO, TEXT("%s - FillRectSysColorTest"), funcName[testFunc]);

    NameValPair SysColors[] = {
                                                NAMEVALENTRY(COLOR_3DDKSHADOW),
                                                NAMEVALENTRY(COLOR_3DFACE),
                                                NAMEVALENTRY(COLOR_BTNFACE),
                                                NAMEVALENTRY(COLOR_3DHILIGHT),
                                                NAMEVALENTRY(COLOR_3DHIGHLIGHT),
#ifndef UNDER_CE
                                                NAMEVALENTRY(COLOR_BTNHILIGHT),
#endif
                                                NAMEVALENTRY(COLOR_BTNHIGHLIGHT),
                                                NAMEVALENTRY(COLOR_3DLIGHT),
                                                NAMEVALENTRY(COLOR_3DSHADOW),
                                                NAMEVALENTRY(COLOR_BTNSHADOW),
                                                NAMEVALENTRY(COLOR_ACTIVEBORDER),
                                                NAMEVALENTRY(COLOR_ACTIVECAPTION),
                                                NAMEVALENTRY(COLOR_APPWORKSPACE),
                                                NAMEVALENTRY(COLOR_BACKGROUND),
                                                NAMEVALENTRY(COLOR_DESKTOP),
                                                NAMEVALENTRY(COLOR_BTNTEXT),
                                                NAMEVALENTRY(COLOR_CAPTIONTEXT),
                                                NAMEVALENTRY(COLOR_GRAYTEXT),
                                                NAMEVALENTRY(COLOR_HIGHLIGHT),
                                                NAMEVALENTRY(COLOR_HIGHLIGHTTEXT),
                                                NAMEVALENTRY(COLOR_INACTIVEBORDER),
                                                NAMEVALENTRY(COLOR_INACTIVECAPTION),
                                                NAMEVALENTRY(COLOR_INACTIVECAPTIONTEXT),
                                                NAMEVALENTRY(COLOR_INFOBK),
                                                NAMEVALENTRY(COLOR_INFOTEXT),
                                                NAMEVALENTRY(COLOR_MENU),
                                                NAMEVALENTRY(COLOR_MENUTEXT),
                                                NAMEVALENTRY(COLOR_SCROLLBAR),
#ifdef UNDER_CE
                                                NAMEVALENTRY(COLOR_STATIC),
                                                NAMEVALENTRY(COLOR_STATICTEXT),
#endif
                                                NAMEVALENTRY(COLOR_WINDOW),
                                                NAMEVALENTRY(COLOR_WINDOWFRAME),
                                                NAMEVALENTRY(COLOR_WINDOWTEXT)
                                                    };
    TDC hdc = myGetDC();
    HBRUSH hbr;
    // to speed up the test we'll ony fill the top half of the rectangles to verify functionality.
    RECT rcLeft = {0, 0, zx/2, zy/2};
    RECT rcRight = {zx/2, 0, zx, zy/2};

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        for(int i = 0; i < countof(SysColors); i++)
        {
            CheckNotRESULT(NULL, hbr = CreateSolidBrush((COLORREF) GetSysColor(SysColors[i].dwVal)));

            CheckNotRESULT(FALSE, FillRect(hdc, &rcLeft, hbr));
            CheckNotRESULT(FALSE, FillRect(hdc, &rcRight, (HBRUSH) (SysColors[i].dwVal + 1)));

            CheckScreenHalves(hdc);

            CheckNotRESULT(FALSE, DeleteObject(hbr));
        }
    }

    myReleaseDC(hdc);
}

void
WritableBitmapTest(int testFunc)
{
    info(ECHO, TEXT("%s - WritableBitmapTest"), funcName[testFunc]);
// desktop allows writing to a bitmap from LoadBitmap
#ifdef UNDER_CE
    NOPRINTERDCV(NULL);

    TDC hdc = myGetDC();
    TDC hdcCompat;
    TBITMAP hbmp, hbmpMask, hbmpDIB;
    TBITMAP hbmpStock;
    // XP only succeeds this call on 1bpp.
    TCHAR *szTestBitmap[] = {
        TEXT("JADE1"),
        TEXT("JADE4"),
        TEXT("JADE8"),
        TEXT("JADE24")
        };
    RECT rc = {0, 0, zx, zy };
    POINT pt[] = {
        {0, 0},
        {zx, zy}
        };
    BYTE bytedata[20] = {GenRand()};
    BYTE *lpBits = NULL;
    struct MYBITMAPINFO bmi;

    TRIVERTEX        vert[2];
    GRADIENT_RECT    gRect[1];
    BLENDFUNCTION bf;

    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 0xFF;
    bf.AlphaFormat = 0;

    gRect[0].UpperLeft = 0;
    gRect[0].LowerRight = 1;
    vert[0].x      = 0;
    vert[0].y      = 0;
    vert[0].Red    = 0xFF00;
    vert[0].Green  = 0x0000;
    vert[0].Blue   = 0x0000;
    vert[0].Alpha  = 0x0000;
    vert[1].x      = zx/2;
    vert[1].y      = zy/2;
    vert[1].Red    = 0x0000;
    vert[1].Green  = 0xFF00;
    vert[1].Blue   = 0x0000;
    vert[1].Alpha  = 0x0000;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hbmpMask = myCreateBitmap(zx, zy, 1, 1, NULL));
        hbmpDIB = myCreateDIBSection(hdc, (void **) &lpBits, 16, zx, -zy, DIB_RGB_COLORS, &bmi, FALSE);

        for(int i = 0; i < countof(szTestBitmap); i++)
        {
            CheckNotRESULT(NULL, hbmp = myLoadBitmap(globalInst, szTestBitmap[i]));

            if(hbmp)
            {
                CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));
                // blit the test bitmap to the left half
                CheckNotRESULT(FALSE, BitBlt(hdc, 0, 0, zx/2, zy, hdcCompat, 0, 0, SRCCOPY));

                SetLastError(ERROR_SUCCESS);
                switch(testFunc)
                {
                    case EBitBlt:
                        CheckForRESULT(FALSE, BitBlt(hdcCompat, 0, 0, zx, zy, hdc, 0, 0, SRCCOPY));
                        break;
                    case EPatBlt:
                        CheckForRESULT(FALSE, PatBlt(hdcCompat, 0, 0, zx, zy, BLACKNESS));
                        break;
                    case EStretchBlt:
                        CheckForRESULT(FALSE, StretchBlt(hdcCompat, 0, 0, zx, zy, hdc, 0, 0, 10, 10, SRCCOPY));
                        break;
                    case EMaskBlt:
                        CheckForRESULT(FALSE, MaskBlt(hdcCompat, 0, 0, zx, zy, hdc, 0, 0, hbmpMask, 0, 0, MAKEROP4(SRCCOPY, SRCINVERT)));
                        break;
                    case EEllipse:
                        CheckForRESULT(FALSE, Ellipse(hdcCompat, 10, 10, 50, 50));
                        break;
                    case ERectangle:
                        CheckForRESULT(FALSE, Rectangle(hdcCompat, 10, 10, 50, 50));
                        break;
                    case ERoundRect:
                        CheckForRESULT(FALSE, RoundRect(hdcCompat, 10, 10, 50, 50, 20, 20));
                        break;
                    case ESetPixel:
                        // a 4 pixel block with random colors should trigger the failure if it's modified.
                        CheckForRESULT(-1, SetPixel(hdcCompat, 0, 0, randColorRef()));
                        CheckForRESULT(-1, SetPixel(hdcCompat, 0, 1, randColorRef()));
                        CheckForRESULT(-1, SetPixel(hdcCompat, 1, 0, randColorRef()));
                        CheckForRESULT(-1, SetPixel(hdcCompat, 1, 1, randColorRef()));
                        break;
                    case ELineTo:
                        CheckNotRESULT(FALSE, MoveToEx(hdcCompat, 0, 0, NULL));
                        CheckForRESULT(FALSE, LineTo(hdcCompat, zx, zy));
                        break;
                    case EDrawEdge:
                        CheckForRESULT(FALSE, DrawEdge(hdcCompat, &rc, BDR_RAISEDINNER, BF_MIDDLE));
                        break;
                    case EPolygon:
                        CheckForRESULT(FALSE, Polygon(hdcCompat, pt, 2));
                        break;
                    case EPolyline:
                        CheckForRESULT(FALSE, Polyline(hdcCompat, pt, 2));
                        break;
                    case ETransparentImage:
                        CheckForRESULT(FALSE, TransparentBlt(hdcCompat, 0, 0, zx, zy, hdc, 10, 10, 50, 50, RGB(0, 0, 0)));
                        break;
                    case ESetBitmapBits:
                        CheckForRESULT(0, SetBitmapBits(hbmp, 20, bytedata));
                        break;
                    case EDrawFocusRect:
                        CheckForRESULT(FALSE, DrawFocusRect(hdcCompat, &rc));
                        break;
                    case EFillRect:
                        CheckForRESULT(FALSE, FillRect(hdcCompat, &rc, (HBRUSH) (COLOR_GRAYTEXT + 1)));
                        break;
                    case EInvertRect:
                        CheckForRESULT(FALSE, InvertRect(hdcCompat, &rc));
                        break;
                    case EGradientFill:
                        CheckForRESULT(FALSE, gpfnGradientFill(hdcCompat,vert,2,gRect,1, GRADIENT_FILL_RECT_H));
                        break;
                    case EAlphaBlend:
                        CheckForRESULT(FALSE, gpfnAlphaBlend(hdcCompat, 0, 0, zx, zy, hdc, 0, 0, zx, zy, bf));
                        break;
                    case EDrawTextW:
                        CheckForRESULT(0, DrawText(hdcCompat, TEXT("Hello World"), -1, &rc, 0));
                        break;
                    case EExtTextOut:
                        CheckForRESULT(0, ExtTextOut(hdcCompat, 0, 0, ETO_OPAQUE, &rc, TEXT("Should not see this"), _tcslen(TEXT("Should not see this")), NULL));
                        break;
                    case EStretchDIBits:
                        CheckForRESULT(FALSE, StretchDIBits(hdcCompat, 0,  0,  zx, zy, 0, 0, zx, zy, lpBits, (LPBITMAPINFO) &bmi, DIB_RGB_COLORS, BLACKNESS));
                        break;
                    case ESetDIBitsToDevice:
                        CheckForRESULT(FALSE, SetDIBitsToDevice(hdcCompat, 0, 0, zx , zy, 0, 0, zx, zy, lpBits, (LPBITMAPINFO) &bmi, DIB_RGB_COLORS));
                        break;
                }

                // last errors are inconsistent
                //CheckForLastError(ERROR_INVALID_HANDLE);

                // blit it to the right half and verify it wasn't modified.
                CheckNotRESULT(FALSE, BitBlt(hdc, zx/2, 0, zx/2, zy, hdcCompat, 0, 0, SRCCOPY));
                CheckScreenHalves(hdc);

                // cleanup and release it.
                CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
                CheckNotRESULT(FALSE, DeleteObject(hbmp));
            }
            else info(ABORT, TEXT("Failed to load bitmap for testing"));
        }

        CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
        CheckNotRESULT(FALSE, DeleteObject(hbmpMask));
        CheckNotRESULT(FALSE, DeleteObject(hbmpDIB));
    }
    
    myReleaseDC(hdc);
#endif
}

void
WritableDCTest(int testFunc)
{
    info(ECHO, TEXT("%s - WritableDCTest"), funcName[testFunc]);

    NOPRINTERDCV(NULL);

    TDC hdc = myGetDC();
    TBITMAP hbmpMask = NULL;
    TBITMAP hbmpDIB = NULL;
    RECT rc = {0, 0, zx, zy};
    POINT pt[] = {
        {0, 0},
        {zx, zy}
        };
    BYTE *lpBits = NULL;
    struct MYBITMAPINFO bmi;

    TRIVERTEX vert[2];
    GRADIENT_RECT gRect[1];
    BLENDFUNCTION bf;

    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 0xFF;
    bf.AlphaFormat = 0;

    gRect[0].UpperLeft = 0;
    gRect[0].LowerRight = 1;
    vert[0].x      = 0;
    vert[0].y      = 0;
    vert[0].Red    = 0xFF00;
    vert[0].Green  = 0x0000;
    vert[0].Blue   = 0x0000;
    vert[0].Alpha  = 0x0000;
    vert[1].x      = zx/2;
    vert[1].y      = zy/2;
    vert[1].Red    = 0x0000;
    vert[1].Green  = 0xFF00;
    vert[1].Blue   = 0x0000;
    vert[1].Alpha  = 0x0000;

    CheckNotRESULT(NULL, hbmpDIB = myCreateDIBSection(hdc, (void **) &lpBits, 16, zx, -zy, DIB_RGB_COLORS, &bmi, FALSE));

    if (IsWritableHDC(hdc))
        info(DETAIL, TEXT("This test requires a write-protected HDC to continue, portion skipped."));
    else
    {
        // verify our write operation to a write-protected DC is blocked
        SetLastError(ERROR_SUCCESS);
        switch(testFunc)
        {
            case EBitBlt:
                CheckForRESULT(FALSE, BitBlt(hdc, 0, 0, zx, zy, hdc, 0, 0, SRCCOPY));
                break;
            case EPatBlt:
                CheckForRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
                break;
            case EStretchBlt:
                CheckForRESULT(FALSE, StretchBlt(hdc, 0, 0, zx, zy, hdc, 0, 0, 10, 10, SRCCOPY));
                break;
            case EMaskBlt:
                CheckForRESULT(FALSE, MaskBlt(hdc, 0, 0, zx, zy, hdc, 0, 0, hbmpMask, 0, 0, MAKEROP4(SRCCOPY, SRCINVERT)));
                break;
            case EEllipse:
                CheckForRESULT(FALSE, Ellipse(hdc, 10, 10, 50, 50));
                break;
            case ERectangle:
                CheckForRESULT(FALSE, Rectangle(hdc, 10, 10, 50, 50));
                break;
            case ERoundRect:
                CheckForRESULT(FALSE, RoundRect(hdc, 10, 10, 50, 50, 20, 20));
                break;
            case ESetPixel:
                // a 4 pixel block with random colors should trigger the failure if it's modified.
                CheckForRESULT(-1, SetPixel(hdc, 0, 0, randColorRef()));
                CheckForRESULT(-1, SetPixel(hdc, 0, 1, randColorRef()));
                CheckForRESULT(-1, SetPixel(hdc, 1, 0, randColorRef()));
                CheckForRESULT(-1, SetPixel(hdc, 1, 1, randColorRef()));
                break;
            case ELineTo:
                CheckNotRESULT(FALSE, MoveToEx(hdc, 0, 0, NULL));
                CheckForRESULT(FALSE, LineTo(hdc, zx, zy));
                break;
            case EDrawEdge:
                CheckForRESULT(FALSE, DrawEdge(hdc, &rc, BDR_RAISEDINNER, BF_MIDDLE));
                break;
            case EPolygon:
                CheckForRESULT(FALSE, Polygon(hdc, pt, 2));
                break;
            case EPolyline:
                CheckForRESULT(FALSE, Polyline(hdc, pt, 2));
                break;
            case ETransparentImage:
                CheckForRESULT(FALSE, TransparentBlt(hdc, 0, 0, zx, zy, hdc, 10, 10, 50, 50, RGB(0, 0, 0)));
                break;
            case EDrawFocusRect:
                CheckForRESULT(FALSE, DrawFocusRect(hdc, &rc));
                break;
            case EFillRect:
                CheckForRESULT(FALSE, FillRect(hdc, &rc, (HBRUSH) (COLOR_GRAYTEXT + 1)));
                break;
            case EInvertRect:
                CheckForRESULT(FALSE, InvertRect(hdc, &rc));
                break;
            case EGradientFill:
                CheckForRESULT(FALSE, gpfnGradientFill(hdc, vert, 2, gRect, 1, GRADIENT_FILL_RECT_H));
                break;
            case EAlphaBlend:
                CheckForRESULT(FALSE, gpfnAlphaBlend(hdc, 0, 0, zx, zy, hdc, 0, 0, zx, zy, bf));
                break;
            case EDrawTextW:
                CheckForRESULT(0, DrawText(hdc, TEXT("Hello World"), -1, &rc, 0));
                break;
            case EExtTextOut:
                CheckForRESULT(0, ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rc, TEXT("Should not see this"), _tcslen(TEXT("Should not see this")), NULL));
                break;
            case EStretchDIBits:
                CheckForRESULT(FALSE, StretchDIBits(hdc, 0,  0,  zx, zy, 0, 0, zx, zy, lpBits, (LPBITMAPINFO) &bmi, DIB_RGB_COLORS, BLACKNESS));
                break;
            case ESetDIBitsToDevice:
                CheckForRESULT(FALSE, SetDIBitsToDevice(hdc, 0, 0, zx , zy, 0, 0, zx, zy, lpBits, (LPBITMAPINFO) &bmi, DIB_RGB_COLORS));
                break;
        }

        CheckForLastError(ERROR_INVALID_HANDLE);
    }
    
    // cleanup
    CheckNotRESULT(FALSE, DeleteObject(hbmpDIB));
    myReleaseDC(hdc);
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
    BltFromMonochromeTest(EBitBlt, ECreateCompatibleBitmap);
    BltFromMonochromeTest(EBitBlt, ECreateDIBSection);
    BltToMonochromeTest(EBitBlt, ECreateBitmap);
    BltToMonochromeTest(EBitBlt, ECreateCompatibleBitmap);
    BltToMonochromeTest(EBitBlt, ECreateDIBSection);
    BitBltNegSrc(EBitBlt);
    BitBltTest2(EBitBlt);
    BitBltTest3(EBitBlt);
    BltAlphaDIBTest(EBitBlt);
    BitBltPalTest(EBitBlt);
    WritableBitmapTest(EBitBlt);
    WritableDCTest(EBitBlt);
    BlitMirrorTest(EBitBlt);

    // depth
    BitBltSuite(EBitBlt);
    BitBltSuiteFullscreen(EBitBlt);
    SimpleColorConversionTest(EBitBlt, 0);
    SimpleColorConversionTest(EBitBlt, 1);
    TestBltInRegion();
    TestAllRops(EBitBlt);

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
// desktop doesn't allow bitmaps that aren't the same bitdepth as the primary
// to be selected into a dc compatible to the primary.
#ifdef UNDER_CE
    CreateBitmapSquares2bpp(ECreateBitmap);
    CreateBitmapSquares4bpp(ECreateBitmap);
    CreateBitmapSquares8bpp(ECreateBitmap);
    CreateBitmapSquares16bpp(ECreateBitmap);
    CreateBitmapSquares24bpp(ECreateBitmap);
    CreateBitmapSquares32bpp(ECreateBitmap);
#endif

    return getCode();
}

//***********************************************************************************
TESTPROCAPI CreateCompatibleBitmap_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    OddCreatecompatibleBitmapTest(ECreateCompatibleBitmap);
    CreateCompatibleBitmapDIBSECTIONTest(ECreateCompatibleBitmap);

    // depth
    SimpleCreatecompatibleBitmapTest(ECreateCompatibleBitmap, 0);
    SimpleCreatecompatibleBitmapTest(ECreateCompatibleBitmap, 1);
    CreateCompatibleBitmapDrawTest();
    CreateCompatibleBitmapFromCreateCompatibleDC();

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
    CreateCompatibleToDibTest(ECreateDIBSection, 1);
    CreateCompatibleToDibTest(ECreateDIBSection, 2);
    CreateCompatibleToDibTest(ECreateDIBSection, 4);
    CreateCompatibleToDibTest(ECreateDIBSection, 8);
    CreateDIBSection24bppDIBTest(ECreateDIBSection);

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
    HollowPolyTest(EEllipse);
    ShapeColorTest(EEllipse, FALSE);
    ShapeColorTest(EEllipse, TRUE);
    WritableBitmapTest(EEllipse);
    WritableDCTest(EEllipse);

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
    WritableBitmapTest(EDrawEdge);
    WritableDCTest(EDrawEdge);

    // depth
    IterateDrawEdge();

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetPixel_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;
    NOPRINTERDC(NULL, getCode());

    // breadth
    passNull2Draw(EGetPixel);
    GetSetPixelBounderies(EGetPixel);
    SetGetPixelGLETest(EGetPixel);
    SetGetPixelAlphaTest();
    GetPixelAlphaDIBTest();

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
    BltFromMonochromeTest(EMaskBlt, ECreateCompatibleBitmap);
    BltFromMonochromeTest(EMaskBlt, ECreateDIBSection);
    BltToMonochromeTest(EMaskBlt, ECreateBitmap);
    BltToMonochromeTest(EMaskBlt, ECreateCompatibleBitmap);
    BltToMonochromeTest(EMaskBlt, ECreateDIBSection);
#ifdef UNDER_CE
    // slight behavioral difference from NT.  If we give a mask, but no source DC and a rop set that doesn't use
    // the source, then xp fails but we succeed, which is reasonable behavior for the situation.
    BogusSrcDCMaskBlt(EMaskBlt);
#endif
    MaskBltBadMaskWidth(EMaskBlt);
    BltAlphaDIBTest(EMaskBlt);
    WritableBitmapTest(EMaskBlt);
    WritableDCTest(EMaskBlt);
    BlitMirrorTest(EMaskBlt);

    // depth
    BitBltSuite(EMaskBlt);
    BitBltSuiteFullscreen(EMaskBlt);
    SimpleColorConversionTest(EMaskBlt, 0);
    SimpleColorConversionTest(EMaskBlt, 1);
    MaskBltTest(EMaskBlt);
    TestAllRops(EMaskBlt);

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
    PatBltBadRopTest();
    BltAlphaDIBTest(EPatBlt);
    WritableBitmapTest(EPatBlt);
    WritableDCTest(EPatBlt);

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
    ShapeColorTest(EPolygon, FALSE);
    ShapeColorTest(EPolygon, TRUE);
    WritableBitmapTest(EPolygon);
    WritableDCTest(EPolygon);
    ViewPort(EPolygon);

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
    SimpleClipRgnTest0(EPolyline);
    SimplePolyTest(EPolyline);
    SimplePolylineTest(EPolyline);
    ShapeColorTest(EPolyline, FALSE);
    ShapeColorTest(EPolyline, TRUE);
    WritableBitmapTest(EPolyline);
    WritableDCTest(EPolyline);
    ViewPort(EPolyline);

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
    HollowPolyTest(ERectangle);
    ShapeColorTest(ERectangle, FALSE);
    ShapeColorTest(ERectangle, TRUE);
    WritableBitmapTest(ERectangle);
    WritableDCTest(ERectangle);

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
    FillRectNullBrushTest(EFillRect);
    PatternBrushFillRectTest();
    FillRectSysColorTest(EFillRect);
    WritableBitmapTest(EFillRect);
    WritableDCTest(EFillRect);

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
    HollowPolyTest(ERoundRect);
    ShapeColorTest(ERoundRect, FALSE);
    ShapeColorTest(ERoundRect, TRUE);
    WritableBitmapTest(ERoundRect);
    WritableDCTest(ERoundRect);

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
    NOPRINTERDC(NULL, getCode());

    // breadth
    passNull2Draw(ESetPixel);
    GetSetPixelBounderies(ESetPixel);
    SetGetPixelGLETest(ESetPixel);
    SetGetPixelAlphaTest();
    SetPixelAlphaDIBTest();
    WritableBitmapTest(ESetPixel);
    WritableDCTest(ESetPixel);

    // depth
    GetSetPixelPairs(ESetPixel);
    GetSetOnScreen(ESetPixel);
    GetSetOffScreen(ESetPixel);

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
    BltFromMonochromeTest(EStretchBlt, ECreateCompatibleBitmap);
    BltFromMonochromeTest(EStretchBlt, ECreateDIBSection);
    BltToMonochromeTest(EStretchBlt, ECreateBitmap);
    BltToMonochromeTest(EStretchBlt, ECreateCompatibleBitmap);
    BltToMonochromeTest(EStretchBlt, ECreateDIBSection);
    SimpleBltTest(EStretchBlt);
    BlitOffScreen();
    StretchBltTest(EStretchBlt);
    StretchBltShrinkTest(EStretchBlt);
    StretchBltTest2(EStretchBlt, BLACKONWHITE);
    StretchBltShrinkTest2(EStretchBlt, BLACKONWHITE);
    StretchBltTest3(EStretchBlt, BLACKONWHITE);
    StretchBltShrinkTest3(EStretchBlt, BLACKONWHITE);
    StretchBltTest2(EStretchBlt, COLORONCOLOR);
    StretchBltShrinkTest2(EStretchBlt, COLORONCOLOR);
    StretchBltTest3(EStretchBlt, COLORONCOLOR);
    StretchBltShrinkTest3(EStretchBlt, COLORONCOLOR);
    TestSBltCase10();
    BltAlphaDIBTest(EStretchBlt);
    WritableBitmapTest(EStretchBlt);
    WritableDCTest(EStretchBlt);
    BlitMirrorTest(EStretchBlt);

    // depth
    BitBltSuite(EStretchBlt);
    BitBltSuiteFullscreen(EStretchBlt);
    SimpleColorConversionTest(EStretchBlt, 0);
    SimpleColorConversionTest(EStretchBlt, 1);
    StretchBltSuite(EStretchBlt, COLORONCOLOR);
    StretchBltSuite(EStretchBlt, BLACKONWHITE);
    TestAllRops(EStretchBlt);
    StretchBltTest2(EStretchBlt, BILINEAR);
    StretchBltShrinkTest2(EStretchBlt, BILINEAR);
    StretchBltTest3(EStretchBlt, BILINEAR);
    StretchBltShrinkTest3(EStretchBlt, BILINEAR);
    BilinearAndHalftoneStretchTest(EStretchBlt, BILINEAR);
    StretchBltSuite(EStretchBlt, BILINEAR);
    StretchBltTest2(EStretchBlt, HALFTONE);
    StretchBltShrinkTest2(EStretchBlt, HALFTONE);
    StretchBltTest3(EStretchBlt, HALFTONE);
    StretchBltShrinkTest3(EStretchBlt, HALFTONE);
    BilinearAndHalftoneStretchTest(EStretchBlt, HALFTONE);
    StretchBltSuite(EStretchBlt, HALFTONE);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI TransparentImage_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(ETransparentImage);
#ifdef UNDER_CE
    // CE allows flipped/mirrored transparentblt's, desktop doesn't.
    NegativeSize(ETransparentImage);
#endif
    BitBltFunctionalTest(ETransparentImage);
    GetSetOffScreen(ETransparentImage);
    GetSetOnScreen(ETransparentImage);
    StretchBltFlipMirrorTest(ETransparentImage);
    SimpleTransparentImageTest();
    TransparentBltErrorTest();
    TransparentBltSetPixelTest();
    TransparentBltBitmapTest();
    TransparentBltPatBltTest();
    TransparentBltSysmemTest();
    TransparentBltTransparencyTest(ETransparentImage);
    BltAlphaDIBTest(ETransparentImage);
    TransparentImagePalTest(ETransparentImage);
    WritableBitmapTest(ETransparentImage);
    WritableDCTest(ETransparentImage);

    // depth
    BitBltSuite(ETransparentImage);
    BitBltSuiteFullscreen(ETransparentImage);
    SimpleColorConversionTest(ETransparentImage, 0);
    SimpleColorConversionTest(ETransparentImage, 1);
    StretchBltSuite(ETransparentImage, BLACKONWHITE);
    StretchBltSuite(ETransparentImage, COLORONCOLOR);
    StretchBltSuite(ETransparentImage, BILINEAR);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI StretchDIBits_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(EStretchDIBits);
    WritableBitmapTest(EStretchDIBits);
    WritableDCTest(EStretchDIBits);

    // depth
    SimpleStretchDIBitsTest(1);
    SimpleStretchDIBitsTest(-1);
    TestAllRops(EStretchDIBits);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI SetDIBitsToDevice_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(ESetDIBitsToDevice);
    WritableBitmapTest(ESetDIBitsToDevice);
    WritableDCTest(ESetDIBitsToDevice);

    // depth
    SimpleSetDIBitsToDeviceTest(1);
    SimpleSetDIBitsToDeviceTest(-1);

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
            GradientFillDirection();
            GradientFill2bpp();
            SimpleGradientFillAlphaTest();
            WritableBitmapTest(EGradientFill);
            WritableDCTest(EGradientFill);

            // depth
            ComplexGradientFill();
            StressGradientFill();
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
    WritableBitmapTest(EInvertRect);
    WritableDCTest(EInvertRect);

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
    ShapeColorTest(ELineTo, FALSE);
    ShapeColorTest(ELineTo, TRUE);
    WritableBitmapTest(ELineTo);
    WritableDCTest(ELineTo);
    ViewPort(ELineTo);

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

//***********************************************************************************
TESTPROCAPI SetBitmapBits_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(ESetBitmapBits);

    // depth
    CreateBitmapSquares1bpp(ESetBitmapBits);
    SetBitmapBitsOnePixel(ESetBitmapBits, 1, ECreateBitmap);
    SetBitmapBitsOnePixel(ESetBitmapBits, 1, ECreateDIBSection);
    SetBitmapBitsOnePixel(ESetBitmapBits, 1, ECreateCompatibleBitmap);
    SetBitmapBitsOnePixel(ESetBitmapBits, 4, ECreateDIBSection);
    SetBitmapBitsOnePixel(ESetBitmapBits, 8, ECreateDIBSection);
    SetBitmapBitsOnePixel(ESetBitmapBits, 16, ECreateDIBSection);
    SetBitmapBitsOnePixel(ESetBitmapBits, 24, ECreateDIBSection);
    SetBitmapBitsOnePixel(ESetBitmapBits, 32, ECreateDIBSection);
    WritableBitmapTest(ESetBitmapBits);

// desktop doesn't allow bitmaps that aren't the same bitdepth as the primary
// to be selected into a dc compatible to the primary.
#ifdef UNDER_CE
    CreateBitmapSquares2bpp(ESetBitmapBits);
    CreateBitmapSquares4bpp(ESetBitmapBits);
    CreateBitmapSquares8bpp(ESetBitmapBits);
    CreateBitmapSquares16bpp(ESetBitmapBits);
    CreateBitmapSquares24bpp(ESetBitmapBits);
    CreateBitmapSquares32bpp(ESetBitmapBits);

    SetBitmapBitsOnePixel(ESetBitmapBits, 2, ECreateDIBSection);
    SetBitmapBitsOnePixel(ESetBitmapBits, 2, ECreateBitmap);
    SetBitmapBitsOnePixel(ESetBitmapBits, 4, ECreateBitmap);
    SetBitmapBitsOnePixel(ESetBitmapBits, 8, ECreateBitmap);
    SetBitmapBitsOnePixel(ESetBitmapBits, 16, ECreateBitmap);
    SetBitmapBitsOnePixel(ESetBitmapBits, 24, ECreateBitmap);
    SetBitmapBitsOnePixel(ESetBitmapBits, 32, ECreateBitmap);
#endif

    return getCode();
}

TESTPROCAPI DrawFocusRect_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(EDrawFocusRect);
    WritableBitmapTest(EDrawFocusRect);
    WritableDCTest(EDrawFocusRect);

    // depth
    DrawFocusRectTest(EDrawFocusRect);

    return getCode();

}

//***********************************************************************************
TESTPROCAPI SetROP2_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(ESetROP2);
    SetRop2Test();
    ROP2Test();
    ROP3Test();
    ROP4Test();

    // depth
    // none

    return getCode();
}


//***********************************************************************************
TESTPROCAPI GetROP2_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(EGetROP2);
    GetRop2Test();
    // depth
    // none

    return getCode();
}

//***********************************************************************************
TESTPROCAPI AlphaBlend_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    if(gpfnAlphaBlend)
    {
        if(AlphaBlendSupported())
        {
            // breadth
            passNull2Draw(EAlphaBlend);
            BitBltOffScreen(EAlphaBlend, 0);
            BitBltOffScreen(EAlphaBlend, 1);
            GetSetOffScreen(EAlphaBlend);
            GetSetOnScreen(EAlphaBlend);
            BltFromMonochromeTest(EAlphaBlend, ECreateBitmap);
            BltFromMonochromeTest(EAlphaBlend, ECreateCompatibleBitmap);
            BltFromMonochromeTest(EAlphaBlend, ECreateDIBSection);
            BltToMonochromeTest(EAlphaBlend, ECreateBitmap);
            BltToMonochromeTest(EAlphaBlend, ECreateCompatibleBitmap);
            BltToMonochromeTest(EAlphaBlend, ECreateDIBSection);
            StretchBltTest(EAlphaBlend);
            StretchBltShrinkTest(EAlphaBlend);
            StretchBltTest2(EAlphaBlend, BLACKONWHITE);
            StretchBltShrinkTest2(EAlphaBlend, BLACKONWHITE);
            StretchBltTest3(EAlphaBlend, BLACKONWHITE);
            StretchBltShrinkTest3(EAlphaBlend, BLACKONWHITE);
            StretchBltTest2(EAlphaBlend, COLORONCOLOR);
            StretchBltShrinkTest2(EAlphaBlend, COLORONCOLOR);
            StretchBltTest3(EAlphaBlend, COLORONCOLOR);
            StretchBltShrinkTest3(EAlphaBlend, COLORONCOLOR);
            AlphaBlendRandomTest(EAlphaBlend);
            AlphaBlendBadRectTest(EAlphaBlend);
            AlphaBlendGoodRectTest(EAlphaBlend);
            AlphaBlendConstAlphaTest(EAlphaBlend);
            AlphaBlendPerPixelAlphaTest(EAlphaBlend, TRUE);
            AlphaBlendPerPixelAlphaTest(EAlphaBlend, FALSE);
            AlphaBlendPerPixelAlphaToPrimaryTest(EAlphaBlend);
            WritableBitmapTest(EAlphaBlend);
            WritableDCTest(EAlphaBlend);
            BlitMirrorTest(EAlphaBlend);

             // depth
            // None
        }
        else info(SKIP, TEXT("AlphaBlend isn't supported in the display driver."));
    }
    else info(SKIP, TEXT("AlphaBlend isn't supported in the image."));

    return getCode();
}

//***********************************************************************************
TESTPROCAPI MapWindowPoints_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2Draw(EMapWindowPoints);
    ClientScreenTest(EMapWindowPoints);

    // depth
    // NONE

    return getCode();
}


