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

   global.cpp

Abstract:

   Code used by the entire GDI test suite

***************************************************************************/

//#define VISUAL_CHECK
#ifndef UNDER_CE
#define _CRT_RAND_S
#include <stdlib.h>
#endif

#include "global.h"

int g_iPrinterCntr = 0;

// These bitmap parameters are set in the ShellProc's Begin Test Case
DWORD g_dwCurrentTestCase = 0xffffffff;

// see global.h to adjust size
int     myStockObj[numStockObj] = {
    BLACK_BRUSH,
    DKGRAY_BRUSH,
    GRAY_BRUSH,
    HOLLOW_BRUSH,
    LTGRAY_BRUSH,
    NULL_BRUSH,
    WHITE_BRUSH,
    BLACK_PEN,
    NULL_PEN,
    WHITE_PEN,
    SYSTEM_FONT,
    DEFAULT_PALETTE
};

NameValPair rgBitmapTypes[numBitMapTypes] = {
    {compatibleBitmap, TEXT("CreateCompatibleBitmap")},
    {lb, TEXT("LoadBitmap")},
    {DibRGB, TEXT("CreateDIBSection RGB")},
    {DibPal, TEXT("CreateDIBSection Pal")}
};

#ifdef UNDER_CE
#define WINDOWSDIRECTORY TEXT("\\windows\\")
#define WINDOWSFONTSDIRECTORY TEXT("\\windows\\fonts\\")
#else
// this may not be true in all circumstances, however it is sufficient for
// the scenarios where this test is run on XP.
#define WINDOWSDIRECTORY TEXT("c:\\windows\\")
#define WINDOWSFONTSDIRECTORY TEXT("c:\\windows\\fonts\\")
#endif

static  DOUBLE   g_ScreenHalvesMaxErrors = 0;
RMSCOMPAREDATA g_ScreenHalvesRMSCompareData;
// the comparison function takes a byte pointer and casts it to what it expects.
BYTE *g_ScreenHalvesCompareParameter = (BYTE *) &g_ScreenHalvesMaxErrors;
PFNCOMPAREPIXELS gpfnScreenHalvesPixelCompare = (PFNCOMPAREPIXELS) &ComparePixels;

// LOCK_SESSION and HOME_SESSION won't be available with certain builds.
#ifndef LOCK_SESSION
#define LOCK_SESSION 1
#endif
#ifndef HOME_SESSION
#define HOME_SESSION 2
#endif

void
ResetIdleTimer()
{
    if (gpfnSHIdleTimerResetEx)
    {
        gpfnSHIdleTimerResetEx(LOCK_SESSION | HOME_SESSION);
    }
}

//**********************************************************************************
void
SetMaxScreenHalvesErrorPercentage(DOUBLE MaxError)
{
    g_ScreenHalvesMaxErrors = MaxError;
    g_ScreenHalvesCompareParameter = (BYTE *) &g_ScreenHalvesMaxErrors;
    gpfnScreenHalvesPixelCompare = (PFNCOMPAREPIXELS) &ComparePixels;
}

//**********************************************************************************
void
SetScreenHalvesRMSPercentage(UINT Threshold, UINT AvgThreshold)
{
    g_ScreenHalvesRMSCompareData.uiThreshold = Threshold;
    g_ScreenHalvesRMSCompareData.uiAvgThreshold = AvgThreshold;

    g_ScreenHalvesCompareParameter = (BYTE *) &g_ScreenHalvesRMSCompareData;
    gpfnScreenHalvesPixelCompare = (PFNCOMPAREPIXELS) &ComparePixelsRMS;
}

//**********************************************************************************
void
ResetScreenHalvesCompareConstraints()
{
    g_ScreenHalvesMaxErrors = 0;
    g_ScreenHalvesCompareParameter = (BYTE *) &g_ScreenHalvesMaxErrors;
    gpfnScreenHalvesPixelCompare = (PFNCOMPAREPIXELS) &ComparePixels;
}

/***********************************************************************************
***
***   UNDER_CE Work Arounds
***
************************************************************************************/

//**********************************************************************************
// UNDER_CE Pending GdiSetBatchLimit
void
pegGdiSetBatchLimit(int i)
{
#ifndef UNDER_CE
    GdiSetBatchLimit(i);
#endif // UNDER_CE
}

//**********************************************************************************
// UNDER_CE Pending GdiSetBatchLimit
void
pegGdiFlush(void)
{
#ifndef UNDER_CE
    GdiFlush();
#endif // UNDER_CE
}

/***********************************************************************************
***
***   myGetBitDepth
***
************************************************************************************/

//***********************************************************************************
inline int myGetBitDepth()
{
    return gBpp;
}

/***********************************************************************************
***
***   Checks pixel for in bounds and whiteness
***
************************************************************************************/

//**********************************************************************************
BOOL isWhitePixel(TDC hdc, DWORD color, int x, int y, int pass)
{
    int     nBits = myGetBitDepth();

    if (((x >= zx || y >= zy || x < 0 || y < 0)) && (color == CLR_INVALID))
        return pass;

    return((color & 0xFFFFFF) == 0xFFFFFF);
}

//**********************************************************************************
BOOL isBlackPixel(TDC hdc, DWORD color, int x, int y, int pass)
{
    if (((x >= zx || y >= zy || x < 0 || y < 0)) && (color == CLR_INVALID))
        return pass;

    // otherwise check against black

    return color == 0x0;
}

void
ShiftBitMasks(DWORD *dwRed, DWORD *dwGreen, DWORD *dwBlue)
{
    while(*dwRed && (*dwRed & 1) != 1)
        *dwRed = *dwRed >> 1;

    while(*dwGreen && (*dwGreen & 1) != 1)
        *dwGreen = *dwGreen >> 1;

    while(*dwBlue && (*dwBlue & 1) != 1)
        *dwBlue = *dwBlue >> 1;

    while(*dwRed && (*dwRed & 0x00800000) != 0x00800000)
        *dwRed = *dwRed << 1;

    while(*dwGreen && (*dwGreen & 0x00008000) != 0x00008000)
        *dwGreen = *dwGreen << 1;

    while(*dwBlue && (*dwBlue & 0x0000080) != 0x00000080)
        *dwBlue = *dwBlue << 1;
}

/***********************************************************************************
***
***   Error checking macro's
***
************************************************************************************/
void
InternalCheckNotRESULT(DWORD dwExpectedResult, DWORD dwResult, const TCHAR *FuncText, const TCHAR *File, int LineNum)
{
    if(dwExpectedResult == dwResult)
    {
        DWORD dwError = GetLastError();
        LPVOID lpMsgBuf  = NULL;
        info(FAIL, TEXT("%s returned %d expected not to be %d, File - %s, Line - %d"), FuncText, dwResult, dwExpectedResult, File, LineNum);
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
                        NULL, dwError, 0, (LPTSTR) &lpMsgBuf, 0, NULL);
        if(!lpMsgBuf)
            lpMsgBuf = NOERRORFOUND;
        info(FAIL, TEXT("GLE returned %d - %s"), dwError, lpMsgBuf);
        if(lpMsgBuf != NOERRORFOUND)
            LocalFree(lpMsgBuf);
        SetLastError(dwError);
    }
}

void
InternalCheckForRESULT(DWORD dwExpectedResult, DWORD dwResult, const TCHAR *FuncText, const TCHAR *File, int LineNum)
{
    if(dwExpectedResult != dwResult)
    {
        DWORD dwError = GetLastError();
        LPVOID lpMsgBuf  = NULL;
        info(FAIL, TEXT("%s returned %d expected to be %d, File - %s, Line - %d"), FuncText, dwResult, dwExpectedResult, File, LineNum);
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
                        NULL, dwError, 0, (LPTSTR) &lpMsgBuf, 0, NULL);
        if(!lpMsgBuf)
            lpMsgBuf = NOERRORFOUND;
        info(FAIL, TEXT("GLE returned %d - %s"), dwError, lpMsgBuf);
        if(lpMsgBuf != NOERRORFOUND)
            LocalFree(lpMsgBuf);
        SetLastError(dwError);
    }
}

void
InternalCheckForLastError(DWORD dwExpectedResult, const TCHAR *File, int LineNum)
{
    DWORD dwResult = GetLastError();
    if(dwExpectedResult != dwResult)
    {
        LPVOID lpMsgBuf = NULL;
        info(FAIL, TEXT("GetLastError() returned %d expected to be %d, File - %s, Line - %d"), dwResult, dwExpectedResult, File, LineNum);
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
                        NULL, dwResult, 0, (LPTSTR) &lpMsgBuf, 0, NULL);
        if(!lpMsgBuf)
            lpMsgBuf = NOERRORFOUND;
        info(FAIL, TEXT("Last Error Value %d = %s"), dwResult, lpMsgBuf);
        if(lpMsgBuf != NOERRORFOUND)
            LocalFree(lpMsgBuf);
    }
}

/***********************************************************************************
***
***   Verification Support
***
************************************************************************************/

//***********************************************************************************
void
setRGBQUAD(MYRGBQUAD * rgbq, int r, int g, int b)
{
    rgbq->rgbBlue = (BYTE)b;
    rgbq->rgbGreen = (BYTE)g;
    rgbq->rgbRed = (BYTE)r;
    rgbq->rgbReserved = 0;
}

void
setRGBQUAD(RGBQUAD * rgbq, int r, int g, int b)
{
    rgbq->rgbBlue = (BYTE)b;
    rgbq->rgbGreen = (BYTE)g;
    rgbq->rgbRed = (BYTE)r;
    rgbq->rgbReserved = 0;
}

COLORREF
GetColorRef(RGBQUAD * rgbq)
{
    return RGB(rgbq->rgbRed, rgbq->rgbGreen, rgbq->rgbBlue);
}

COLORREF
GetColorRef(MYRGBQUAD * rgbq)
{
    return RGB(rgbq->rgbRed, rgbq->rgbGreen, rgbq->rgbBlue);
}

// take a 32bpp BGR value and change it into a 32bpp RGB
// and vice versa.
COLORREF
SwapRGB(COLORREF cr)
{
    return ((cr & 0xFF000000) | ((cr & 0x00FF0000) >> 16) |(cr & 0x0000FF00) | ((cr & 0x000000FF) << 16));
}

int roundUp(double number)
{
    if((number - ((int) number)) >= .5)
        return((int) number + 1);
    else
        return((int) number);
}

//**********************************************************************************
BOOL
CheckScreenHalves(TDC hdc)
{
    ResetIdleTimer();
    NOPRINTERDC(hdc, TRUE);

    BOOL    bRval = TRUE;
    int     bpp = 24,
            step = (bpp / 8);
    DWORD   maxErrors = 20;
    BYTE   *lpBits = NULL;
    TDC     myHdc;
    TBITMAP hBmp,
            stockBmp;
    DIBSECTION ds;
    DWORD dwError;
    cCompareResults cCr;
    DOUBLE dData = 0;
    HRESULT hr;

    if (NULL == (myHdc = CreateCompatibleDC(hdc)))
    {
        dwError = GetLastError();
        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("CreateCompatibleDC(hdc) fail: Out of memory:  bpp=%d err=%ld"), bpp, dwError);
        else
        {
            info(FAIL, TEXT("CreateCompatibleDC(hdc) fail: bpp=%d err=%ld"), bpp, dwError);
            bRval = FALSE;
        }
        return bRval;
    }

    // BUGBUG: because of accessing by DWORD, access violation on last pixel, increase
    // BUGBUG: DIB height by 1 to avoid violation.
    if (NULL == (hBmp = myCreateDIBSection(hdc, (VOID **) & lpBits, bpp, zx, -(zy+1), DIB_RGB_COLORS, NULL, FALSE)))
    {
        dwError = GetLastError();
        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("myCreateDIBSection() fail: Out of memory: bpp=%d err=%ld"), bpp, dwError);
        else
        {
            info(FAIL, TEXT("myCreateDIBSection() fail: bpp=%d err=%ld"), bpp, dwError);
            bRval = FALSE;
        }
        CheckNotRESULT(FALSE, DeleteDC(myHdc));
        return bRval;
    }

    // copy the bits
    CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(myHdc, hBmp));
    CheckNotRESULT(0, GetObject(hBmp, sizeof (DIBSECTION), &ds));
    if (ds.dsBm.bmHeight < 0)
    {
       info(FAIL, TEXT("CheckScreenHalves - GetObject returned bmp with negative height"));
       bRval = FALSE;
    }

    CheckNotRESULT(FALSE, BitBlt(myHdc, 0, 0, zx, zy, hdc, grcInUse.left, grcInUse.top, SRCCOPY));

    cCr.Reset();
    cCr.SetMaxResults(maxErrors);
    // negative height because this is a top down dib.
    hr = gpfnScreenHalvesPixelCompare(lpBits, zx/2, -zy, ds.dsBm.bmWidthBytes, lpBits + (step * zx/2), zx/2, -zy, ds.dsBm.bmWidthBytes, &cCr, g_ScreenHalvesCompareParameter);

    if(FAILED(hr) && hr != E_FAIL)
    {
        info(FAIL, TEXT("CheckScreenHalves: Screen comparison call failed"));
        bRval = FALSE;
    }
    else if(cCr.GetPassResult() == FALSE)
    {
        info(FAIL, TEXT("CheckScreenHalves: Screen halves mismatch detected."));
        OutputScreenHalvesBitmap(hdc, zx, zy);
        bRval = FALSE;
    }

    OutputLogInfo(&cCr);

    CheckNotRESULT(NULL, SelectObject(myHdc, stockBmp));
    CheckNotRESULT(FALSE, DeleteObject(hBmp));
    CheckNotRESULT(FALSE, DeleteDC(myHdc));
    return bRval;
}

/***********************************************************************************
***
***   Make sure entire screen is white
***
************************************************************************************/

//**********************************************************************************
int
CheckAllWhite(TDC hdc, BOOL quiet, DWORD dwExpected)
{
    ResetIdleTimer();
    NOPRINTERDC(hdc, dwExpected);

    int     bpp = 24,
            step;
    DWORD   maxErrors = 20,
            dwErrors = 0,
            dwWhite = 0x00FFFFFF;
    BOOL    bMatch = FALSE;
    BYTE   *lpBits = NULL,
           *pBitsExpected;
    TDC     myHdc;
    TBITMAP hBmp,
            stockBmp;
    DIBSECTION ds;

    // set the step to be the step for the DIB we're creating to do the test on.
    step = (bpp / 8);

    if (NULL == (myHdc = CreateCompatibleDC(hdc)))
    {
        DWORD dwError = GetLastError();
        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("CreateCompatibleDC(hdc) fail: Out of memory: err=%ld"), dwError);
        else
            info(FAIL, TEXT("CreateCompatibleDC(hdc) fail: err=%ld"), dwError);
        return dwExpected;
    }

    // BUGBUG: because of accessing by DWORD, access violation on last pixel, increase
    // BUGBUG: DIB height by 1 to avoid violation.
    // Warning 22019 implies that arithmetic operations my underflow.
    // Since we're test code and these are all pretty-well-known window sizes, suppress the warning.
#pragma warning(suppress:22019)
    if (NULL == (hBmp = myCreateDIBSection(hdc, (VOID **) & lpBits, bpp, zx, -(zy + 1), DIB_RGB_COLORS, NULL, FALSE)))
    {
        DWORD dwError = GetLastError();
        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("myCreateDIBSection() fail: Out of memory: err=%ld"), dwError);
        else
            info(FAIL, TEXT("myCreateDIBSection() fail: err=%ld"), dwError);
        CheckNotRESULT(FALSE, DeleteDC(myHdc));
        return dwExpected;
    }

    // copy the bits
    CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(myHdc, hBmp));
    CheckNotRESULT(0, GetObject(hBmp, sizeof (DIBSECTION), &ds));
    if (ds.dsBm.bmHeight < 0)
        info(FAIL, TEXT("CheckAllWhite - GetObject returned bmp with negative height"));

    CheckNotRESULT(FALSE, BitBlt(myHdc, 0, 0, zx, zy, hdc, grcInUse.left, grcInUse.top, SRCCOPY));

    int bmpStep;
    for (int y = 0; y < zy; y++)
    {
        // point the expected and actual pixel pointers to the beginning of the scanline
        if(!g_bRTL)
        {
            pBitsExpected = lpBits + y * ds.dsBm.bmWidthBytes;
            bmpStep = step;
        }
        else
        {
            pBitsExpected = lpBits + (y+1)*ds.dsBm.bmWidthBytes - step;
            bmpStep = -step;
        }

        for (int x = 0; x < zx; x++, pBitsExpected += bmpStep)
        {
            DWORD dwPixelValue = *(DWORD UNALIGNED *) pBitsExpected;
            bMatch = ((dwPixelValue & dwWhite) == dwWhite);
            if (!bMatch)
            {
                dwErrors++;
                // if we expect 0 non-white pixels, then output any we find that aren't white up to maxerrors,
                // otherwise there's no telling where we are in the list of mismatches, so don't output anything.
                if (dwErrors < maxErrors  && dwExpected == 0 && !quiet)
                    info( DETAIL, TEXT("(%d, %d) Found (0x%06X)"), x, y, dwPixelValue);
            }
        }
    }

    // if there's more failures than we expected and we weren't asked to be quiet, log a failure.
    if(dwErrors > dwExpected && !quiet)
    {
        info(FAIL, TEXT("%d pixels found, expected %d"), dwErrors, dwExpected);
        OutputBitmap(myHdc, zx, zy);
    }

    CheckNotRESULT(NULL, SelectObject(myHdc, stockBmp));
    CheckNotRESULT(FALSE, DeleteObject(hBmp));
    CheckNotRESULT(FALSE, DeleteDC(myHdc));

    return (int) dwErrors;
}

/***********************************************************************************
***
***   Looks for black on inside of rect and white around outside
***
************************************************************************************/

//**********************************************************************************
void
goodBlackRect(TDC hdc, RECT * r)
{
    NOPRINTERDCV(hdc);

    int     midX = ((*r).right + (*r).left) / 2,    // midpoints on rect

            midY = ((*r).bottom + (*r).top) / 2;
    POINT   white[4],
            black[4];
    DWORD   result;
    BOOL Pass = true;

    // pixels that are expected white
    white[0].x = (*r).left - 1;
    white[0].y = midY;
    white[1].x = midX;
    white[1].y = (*r).top - 1;
    white[2].x = (*r).right + 1;
    white[2].y = midY;
    white[3].x = midX;
    white[3].y = (*r).bottom + 1;

    // pixels that are expected black
    black[0].x = (*r).left + 1;
    black[0].y = midY;
    black[1].x = midX;
    black[1].y = (*r).top + 1;
    black[2].x = (*r).right - 2;
    black[2].y = midY;
    black[3].x = midX;
    black[3].y = (*r).bottom - 2;

    for (int i = 0; i < 4; i++)
    {
        // looks on outside edges for white
        result = GetPixel(hdc, white[i].x, white[i].y);

        if (!isWhitePixel(hdc, result, white[i].x, white[i].y, 1))
        {
            info(FAIL, TEXT("On RECT (%d %d %d %d) found non-white Pixel:0x%x at %d %d"), (*r).left, (*r).top, (*r).right,
                 (*r).bottom, result, white[i].x, white[i].y);
            Pass = FALSE;
        }

        // looks on inside for black
        result = GetPixel(hdc, black[i].x, black[i].y);

        if (!isBlackPixel(hdc, result, black[i].x, black[i].y, 1))
        {
            info(FAIL, TEXT("On RECT (%d %d %d %d) found white Pixel:0x%x at %d %d"), (*r).left, (*r).top, (*r).right,
                 (*r).bottom, result, black[i].x, black[i].y);
           Pass = FALSE;
        }
    }

    if(!Pass)
        OutputBitmap(hdc, zx, zy);
}

/***********************************************************************************
***
***   Random number generators
***
************************************************************************************/
static int initialPassFlag = 0;

//**********************************************************************************
int
goodRandomNum(int range)
{                               // return 0 to (num-1)
    return abs(GenRand()) % range;
}

//**********************************************************************************
int
badRandomNum(void)
{
    return GenRand() * ((GenRand() % 2) ? -1 : 1);
}

//**********************************************************************************
 int GenRand()
{
    #ifdef UNDER_CE 
    int buffer1, buffer2;
    /*generates random number*/
    BCryptGenRandom(NULL, (PUCHAR) &buffer1, sizeof(int),BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    buffer2 = buffer1 & RAND_BIT_MASK;
    return buffer2;
    #else
    UINT uNumber = 0;
    rand_s(&uNumber);
    return (uNumber & RAND_BIT_MASK);
    #endif 
}

//**********************************************************************************
COLORREF randColorRef(void)
{
    return RGB(GenRand() % 256, GenRand() % 256, GenRand() % 256);
}

//**********************************************************************************
COLORREF randColorRefA(void)
{
    return RGBA(GenRand() % 256, GenRand() % 256, GenRand() % 256, GenRand() % 256);
}

/***********************************************************************************
***
***   RECT functions
***
************************************************************************************/
void
setRect(RECT * aRect, int l, int t, int r, int b)
{
    (*aRect).left = l;
    (*aRect).top = t;
    (*aRect).right = r;
    (*aRect).bottom = b;
}

//***********************************************************************************
BOOL isEqualRect(RECT * a, RECT * b)
{
    return (*a).top == (*b).top && (*a).left == (*b).left && (*a).right == (*b).right && (*a).bottom == (*b).bottom;
}

void
fixNum(long *n)
{
#ifdef UNDER_CE
    if (abs(*n) > 0xFFF)
    {                           // only can hold 6bit values
        *n = *n % 0xFFF;
        //info(ECHO,TEXT("pre:%X post:%X post:%d"),x, *n, *n);
    }
#endif // UNDER_CE
}

//***********************************************************************************
void
randomRect(RECT * temp)
{
    (*temp).left = GenRand() * ((GenRand() % 2) ? -1 : 1);
    (*temp).top = GenRand() * ((GenRand() % 2) ? -1 : 1);
    (*temp).right = GenRand() * ((GenRand() % 2) ? -1 : 1);
    (*temp).bottom = GenRand() * ((GenRand() % 2) ? -1 : 1);

    fixNum(&((*temp).left));
    fixNum(&((*temp).top));
    fixNum(&((*temp).right));
    fixNum(&((*temp).bottom));
}

void
OrderRect(RECT * rc)
{
    int nSwapTmp;

    if(rc->top > rc->bottom)
    {
        nSwapTmp = rc->top;
        rc->top = rc->bottom;
        rc->bottom = nSwapTmp;
    }

    if(rc->left > rc->right)
    {
        nSwapTmp = rc->left;
        rc->left = rc->right;
        rc->right = nSwapTmp;
    }
}

//***********************************************************************************
void
randomRectWithinArea(int ntop, int nleft, int nright, int nbottom, RECT * rcResult)
{
    int nHeight = nbottom - ntop;
    int nWidth = nright - nleft;

    rcResult->top = GenRand() % nHeight;
    rcResult->bottom = GenRand() % nHeight;
    rcResult->left = GenRand() % nWidth;
    rcResult->right = GenRand() % nWidth;

    rcResult->top += ntop;
    rcResult->bottom += ntop;
    rcResult->left += nleft;
    rcResult->right += nleft;
}

//***********************************************************************************
void
randomWORectWithinArea(int ntop, int nleft, int nright, int nbottom, RECT * rcResult)
{
    randomRectWithinArea(ntop, nleft, nbottom, nright, rcResult);
    OrderRect(rcResult);
}

/***********************************************************************************
***
***   setTestRectRgn
***
************************************************************************************/
RECT    rTests[NUMREGIONTESTS];
RECT    center;

//***********************************************************************************
void
setTestRectRgn(void)
{

    int     sx = 50,
            sy = 50,
            gx = 100 + sx,
            gy = 100 + sy,
            ox = (gx - sx) / 4,
            oy = (gy - sy) / 4,
            x1 = ox,
            y1 = oy,
            x2 = x1 + ox * 2,
            y2 = y1 + oy * 2,
            x3 = x2 + ox * 2,
            y3 = y2 + oy * 2,
            x4 = x3 + ox * 2,
            y4 = y3 + oy * 2;

    setRect(&center, sx, sy, gx, gy);

    // interesting rect positions
    setRect(&rTests[0], x1, y1, x2, y2);
    setRect(&rTests[1], x2, y1, x3, y2);
    setRect(&rTests[2], x3, y1, x4, y2);
    setRect(&rTests[3], x1, y2, x2, y3);
    setRect(&rTests[4], x2, y2, x3, y3);
    setRect(&rTests[5], x3, y2, x4, y3);
    setRect(&rTests[6], x1, y3, x2, y4);
    setRect(&rTests[7], x2, y3, x3, y4);
    setRect(&rTests[8], x3, y3, x4, y4);
    setRect(&rTests[9], gx + ox, sy, gx * 2 - sx + ox, gy);
    setRect(&rTests[10], x1, y1, sx + 1, sy + 1);
    setRect(&rTests[11], gx - 1, y1, x4, sy + 1);
    setRect(&rTests[12], x1, gy - 1, sx + 1, y4);
    setRect(&rTests[13], gx - 1, gy - 1, x4, y4);
    setRect(&rTests[14], sx, sy, gx, gy);
}

/***********************************************************************************
***
***   Palette Functions
***
************************************************************************************/

//**********************************************************************************
HPALETTE myCreatePal(TDC hdc, int code)
{
    HPALETTE hPal,
            stockPal;
    WORD    nEntries;
    int     i;

    struct
    {
        WORD    Version;
        WORD    NumberOfEntries;
        PALETTEENTRY aEntries[256];
    }
    Palette =
    {
        0x300,                  // version number
        256                     // number of colors
    };

    // set up palette
    CheckNotRESULT(NULL, hPal = (HPALETTE) GetCurrentObject(hdc, OBJ_PAL));
    CheckNotRESULT(0, GetObject(hPal, sizeof (nEntries), &nEntries));
    CheckNotRESULT(0, GetPaletteEntries(hPal, 0, nEntries, (LPPALETTEENTRY) & (Palette.aEntries)));

    // copy bottom half of def sys-pal to bottom of new pal
    for (i = 246; i < 256; i++)
        Palette.aEntries[i] = Palette.aEntries[i - 236];

    for (i = 10; i < 246; i++)
        Palette.aEntries[i].peRed = Palette.aEntries[i].peGreen = Palette.aEntries[i].peBlue = (BYTE)(i + code);

    CheckNotRESULT(NULL, hPal = CreatePalette((LPLOGPALETTE) & Palette));

    CheckNotRESULT(NULL, stockPal = SelectPalette(hdc, hPal, 0));

    if (!hPal || !stockPal)
        info(FAIL, TEXT("myCreatePal: [%s%s] Invalid"), (!hPal) ? L"hPal" : L"", (!stockPal) ? L" stockPal" : L"");

    CheckNotRESULT(GDI_ERROR, RealizePalette(hdc));

    return hPal;
}

//**********************************************************************************
HPALETTE myCreateNaturalPal(TDC hdc)
{
    HPALETTE hPal;

    // set up palette
    CheckNotRESULT(NULL, hPal = (HPALETTE) GetStockObject(DEFAULT_PALETTE));

    return hPal;
}

//**********************************************************************************
void
myDeletePal(TDC hdc, HPALETTE hPal)
{

    HPALETTE hPalRet;

    CheckNotRESULT(NULL, hPalRet = SelectPalette(hdc, (HPALETTE) GetStockObject(DEFAULT_PALETTE), 0 /* CE ignore this */ ));
    CheckNotRESULT(FALSE, DeleteObject(hPal));

    RealizePalette(hdc);
}

//***********************************************************************************
HFONT setupFont(TCHAR *tszFaceName, long Hei, long Wid, long Esc, long Ori, long Wei, BYTE Ita, BYTE Und, BYTE Str)
{
    LOGFONT lFont;
    HFONT   hFont;

    // set up log info
    lFont.lfHeight = Hei;
    lFont.lfWidth = Wid;
    lFont.lfEscapement = Esc;
    lFont.lfOrientation = Ori;
    lFont.lfWeight = Wei;
    lFont.lfItalic = Ita;
    lFont.lfUnderline = Und;
    lFont.lfStrikeOut = Str;

    lFont.lfCharSet = DEFAULT_CHARSET;
    lFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lFont.lfQuality = DEFAULT_QUALITY;
    lFont.lfPitchAndFamily = DEFAULT_PITCH;

    _tcsncpy_s(lFont.lfFaceName, tszFaceName, LF_FACESIZE-1);
    // create and select in font
    CheckNotRESULT(NULL, hFont = CreateFontIndirect(&lFont));

    return hFont;
}

//**********************************************************************************

/* font management functions */
void
CleanupFontArray(fileData *fdFontArray[], int nEntries)
{

    for(int i = 0; i < nEntries; i++)
    {
        if((*fdFontArray)[i].tszFileName)
            delete[] (*fdFontArray)[i].tszFileName;
        (*fdFontArray)[i].tszFileName = NULL;

        if((*fdFontArray)[i].tszFullPath)
            delete[] (*fdFontArray)[i].tszFullPath;
        (*fdFontArray)[i].tszFullPath = NULL;
    }

    if(*fdFontArray)
        delete[] *fdFontArray;

    *fdFontArray = NULL;
}

// figure out what fonts are already in a specified directory, and fill the buffer given
bool
FindAllFonts(TCHAR *tszFontPath, fileData *fdFontArray[], int *nEntries)
{
    WIN32_FIND_DATA wfd;
    HANDLE hFindFile;
    int nFilesFound = 0;
    TCHAR tszLocation[MAX_PATH];
    bool bRval = true;
    TCHAR *tszFileWildCard[] = {
        TEXT("*.ttf"),
        TEXT("*.ttc"),
        TEXT("*.ac3"),
        TEXT("*.fon"),
        TEXT("*.fnt"),
        };

    DWORD dwFontType[] = {
        TRUETYPE_FONTTYPE,
        TRUETYPE_FONTTYPE,
        TRUETYPE_FONTTYPE,
        RASTER_FONTTYPE,
        RASTER_FONTTYPE,
        };

    DWORD dwFileType[] = {
        TRUETYPE_FILETYPE,
        TRUETYPECOLLECTION_FILETYPE,
        TRUETYPEAGFA_FILETYPE,
        RASTERFON_FILETYPE,
        RASTERFNT_FILETYPE
        };
    int nLocationEnd = _tcslen(tszFontPath) + 1;
    int nOpenLength = MAX_PATH - nLocationEnd - 1;
    int nWildCardIndex;

    // only proceed if a valid path was given
    if(!nEntries || !fdFontArray || !tszFontPath || !tszFontPath[0])
        return false;

    *nEntries = 0;
    memset(tszLocation, 0, sizeof(tszLocation));
    _tcsncpy_s(tszLocation, tszFontPath, MAX_PATH-1);

    // count of how many fonts are in a particular location, for allocating the array.
    for(nWildCardIndex = 0; nWildCardIndex < countof(tszFileWildCard); nWildCardIndex++)
    {
        _tcsncat_s(tszLocation, tszFileWildCard[nWildCardIndex], nOpenLength);
        hFindFile = FindFirstFile(tszLocation, &wfd);
        if(hFindFile != INVALID_HANDLE_VALUE)
        {
            do
            {
                nFilesFound++;
            }while(FindNextFile(hFindFile, &wfd));

            FindClose(hFindFile);
        }
        tszLocation[nLocationEnd] = NULL;
    }

    // if files were found, allocate the array and fill it in.
    if(nFilesFound > 0)
    {
        *fdFontArray = new(fileData[nFilesFound]);

        if(*fdFontArray)
        {
            for(int i = 0; i < nFilesFound; i++)
                memset(&((*fdFontArray)[i]), 0, sizeof(fileData));

            for(nWildCardIndex = 0; nWildCardIndex < countof(tszFileWildCard); nWildCardIndex++)
            {
                _tcsncat_s(tszLocation, tszFileWildCard[nWildCardIndex], nOpenLength);
                hFindFile = FindFirstFile(tszLocation, &wfd);
                if(hFindFile != INVALID_HANDLE_VALUE && bRval)
                {
                    do
                    {
                        if(*nEntries < nFilesFound)
                        {
                            (*fdFontArray)[*nEntries].tszFileName = new(TCHAR[_tcslen(wfd.cFileName) + 1]);
                            (*fdFontArray)[*nEntries].tszFullPath = new(TCHAR[_tcslen(wfd.cFileName) + _tcslen(tszFontPath) + 1]);
                            if((*fdFontArray)[*nEntries].tszFileName != NULL && (*fdFontArray)[*nEntries].tszFullPath != NULL)
                            {
                                // copy over the full path
                                _tcscpy_s((*fdFontArray)[*nEntries].tszFullPath, (_tcslen(wfd.cFileName) + _tcslen(tszFontPath) + 1), tszFontPath);
                                _tcscat_s((*fdFontArray)[*nEntries].tszFullPath, (_tcslen(wfd.cFileName) + _tcslen(tszFontPath) + 1), wfd.cFileName);

                                // now just the file name
                                _tcscpy_s((*fdFontArray)[*nEntries].tszFileName, (_tcslen(wfd.cFileName) + 1), wfd.cFileName);

                                wfd.cFileName[i] = NULL;
                                (*fdFontArray)[*nEntries].dwFileSizeHigh = wfd.nFileSizeHigh;
                                (*fdFontArray)[*nEntries].dwFileSizeHigh = wfd.nFileSizeLow;
                                memcpy(&((*fdFontArray)[*nEntries].ft), &wfd.ftCreationTime, sizeof(FILETIME));

                                // each wildcard is associated to a specific font type, .ttf/ttc/ac3 are truetype, .fon/fnt are raster fonts.
                                // set this fonts type based on it's file name wildcard.
                                (*fdFontArray)[*nEntries].dwType = dwFontType[nWildCardIndex];
                                (*fdFontArray)[*nEntries].dwFileType = dwFileType[nWildCardIndex];

                                (*nEntries)++;
                            }
                            else
                            {
                                if((*fdFontArray)[*nEntries].tszFileName)
                                    delete[] (*fdFontArray)[*nEntries].tszFileName;
                                if((*fdFontArray)[*nEntries].tszFullPath)
                                    delete[] (*fdFontArray)[*nEntries].tszFullPath;
                                bRval = false;
                            }
                        }
                    }while(FindNextFile(hFindFile, &wfd) && bRval);

                    FindClose(hFindFile);
                }
                tszLocation[nLocationEnd] = NULL;
            }
        }
    }

    if(*nEntries != nFilesFound)
    {
        CleanupFontArray(fdFontArray, *nEntries);
        *nEntries = 0;
        bRval = false;
    }
    return bRval;
}

/***********************************************************************************
***
***   Font Support Functions
***
************************************************************************************/
// indecies into the global font array
// entry 0 must always be valid, otherwise the array is emtpy...
int     aFont = 0;

// fonts which have known metrics (the framework handles font versions), no font is known by default
int     tahomaFont = -1;
int     legacyTahomaFont = -1;
int     BigTahomaFont = -1;     // used for Complex Script Images
int     courierFont = -1;
int     timesFont = -1;
int     verdanaFont = -1;
int     symbolFont = -1;
int     wingdingFont = -1;
int     MSLOGO = -1;
int     arialRasterFont = -1;

// all of the font file data available on the system
fileData *gfdWindowsFonts = NULL;
int     gnWindowsFontsEntries = 0;
fileData *gfdWindowsFontsFonts = NULL;
int     gnWindowsFontsFontsEntries = 0;
fileData *gfdUserFonts = NULL;
int     gnUserFontsEntries = 0;

// our global font data that everyone can access
fontInfo *fontArray = NULL;
int numFonts = 0;

// all of our known font information
// There are two tahomas - first is the normal US small font and
// the last one is Big Tahoma used for complex script images
static KnownfontInfo privateFontArray[] = {
    TEXT("Tahoma"), TEXT("Tahoma"), TEXT("tahoma.ttf"), &tahomaFont, FW_NORMAL, 0, 0x4a58e19c, 0x01c377de, 0x00000000, 0x0001f74c, TRUETYPE_FONTTYPE,
    TEXT("Legacy_PPC_Tahoma"), TEXT("Legacy_PPC_Tahoma"), TEXT("Legacy_PPC_Tahoma.ttf"), &legacyTahomaFont, FW_NORMAL, 0, 0x4a58e19c, 0x01c377de, 0x00000000, 0x0001fd98, TRUETYPE_FONTTYPE,
    TEXT("Courier New"), TEXT("Courier New"), TEXT("cour.ttf"), &courierFont, FW_NORMAL, 0, 0x2ed2b4be, 0x01c377de, 0x00000000, 0x00027a9c, TRUETYPE_FONTTYPE,
    TEXT("Times New Roman"), TEXT("Times New Roman"), TEXT("times.ttf"), &timesFont, FW_NORMAL, 0, 0x4a70bbfa, 0x01c377de, 0x00000000, 0x0002d290, TRUETYPE_FONTTYPE,
    TEXT("Verdana"), TEXT("Verdana"), TEXT("verdana.ttf"), &verdanaFont, FW_NORMAL, 0, 0x4afb14e8, 0x01c377de, 0x00000000, 0x000248f8, TRUETYPE_FONTTYPE,
    TEXT("Symbol"), TEXT("Symbol"), TEXT("symbol.ttf"), &symbolFont, FW_NORMAL, 0, 0x4a4a91ca, 0x01c377de, 0x00000000, 0x00010f58, TRUETYPE_FONTTYPE,
    TEXT("Wingdings"), TEXT("Wingdings"), TEXT("wingding.ttf"), &wingdingFont, FW_NORMAL, 0, 0x4b0bc75d, 0x01c377de, 0x00000000, 0x00013c68, TRUETYPE_FONTTYPE,
    TEXT("Microsoft Logo"), TEXT("Microsoft Logo Bold Italic"), TEXT("mslogo.ttf"), &MSLOGO, FW_BOLD, 0xFF, 0x3e09e681, 0x01c377de, 0x00000000, 0x000009c4, TRUETYPE_FONTTYPE,
    TEXT("Arial"), TEXT("Arial"), TEXT("arial.fnt"), &arialRasterFont, FW_NORMAL, 0, 0x2e675e17, 0x01c377de, 0x00000000, 0x00001788, RASTER_FONTTYPE,
    TEXT("Tahoma"), TEXT("Tahoma"), TEXT("tahoma.ttf"), &BigTahomaFont, FW_NORMAL, 0, 0x4a58e19c, 0x01c377de, 0x00000000, 0x0004aa10, TRUETYPE_FONTTYPE,
};

static const int privateNumFonts = sizeof(privateFontArray)/sizeof(KnownfontInfo);

int
FindFileInArray(TCHAR *tszFileName, fileData *fdFontArray[], int nEntries)
{
    // if the font array is invalid, or the font isn't there, return false.
    if(tszFileName && fdFontArray && *fdFontArray)
    {
        for(int i = 0; i < nEntries; i++)
        {
            // when the font is found, return true
            if(_tcsicmp(tszFileName, (*fdFontArray)[i].tszFileName) == 0)
                return i;
        }
    }
    return -1;
}

int
FindFontInArray(KnownfontInfo *kfi, fontInfo *FontArray[], int nEntries)
{
    int index = -1;
    int MatchingEntryCount = 0;

    // if the font array is invalid, or the font isn't there, return false.
    if(kfi->tszFaceName && kfi->tszFullName && FontArray && *FontArray)
    {
        for(int i = 0; i < nEntries; i++)
        {
            // when the font is found, return true
            if(_tcsicmp(kfi->tszFaceName, (*FontArray)[i].tszFaceName) == 0 &&
                _tcsicmp(kfi->tszFullName, (*FontArray)[i].tszFullName) == 0 &&
                kfi->dwType == (*FontArray)[i].dwType &&
                kfi->lWeight == (*FontArray)[i].lWeight &&
                kfi->bItalics == (*FontArray)[i].bItalics)
            {
                index = i;
                MatchingEntryCount++;
            }
        }
    }

    // if we have 0 matches, then the index is still -1.
    // if we have more than 1 index, then we don't have a solid match,
    // the index is -1.
    // if we have 1 match, then we return the index of it.
    if(MatchingEntryCount > 1)
        index = -1;

    return index;
}

int
FindFontFaceInArray(TCHAR * tszFaceName, fontInfo *FontArray[], int nEntries)
{
    int index = -1;
    int MatchingEntryCount = 0;

    // if the font array is invalid, or the font isn't there, return false.
    if(tszFaceName)
    {
        for(int i = 0; i < nEntries; i++)
        {
            // when the font is found, return true
            if(_tcsicmp(tszFaceName, (*FontArray)[i].tszFaceName) == 0)
            {
                index = i;
                MatchingEntryCount++;
            }
        }
    }

    // if we have 0 matches, then the index is still -1.
    // if we have more than 1 index, then we don't have a solid match,
    // the index is -1.
    // if we have 1 match, then we return the index of it.
    if(MatchingEntryCount > 1)
        index = -1;

    return index;
}

int
FileDataMatchesKnownFont(KnownfontInfo *kfi, fileData *fdFontArray[], int nEntries, BOOL bSystemFont)
{
    // if the font array is invalid, or the font isn't there, return false.
    if(kfi && fdFontArray && *fdFontArray)
    {
        for(int i = 0; i < nEntries; i++)
        {
            // if this is a user added font, and it wasn't added, then ignore it.
            if(!bSystemFont && !((*fdFontArray)[i].bFontAdded))
                continue;

            // when the font is found, return true if everything matches,
            if(_tcsicmp(kfi->tszFileName, (*fdFontArray)[i].tszFileName) == 0 &&
                    kfi->dwFileSizeHigh == (*fdFontArray)[i].dwFileSizeHigh &&
                    kfi->dwFileSizeLow == (*fdFontArray)[i].dwFileSizeLow)
                    return i;
        }
    }
    return -1;
}

void
OutputFileInfo(fileData *fdFontArray[], int nEntries,  bool bSystemFont)
{
    if(fdFontArray && *fdFontArray)
    {
        for(int i = 0; i < nEntries; i++)
        {
            TCHAR *tszFontAdded;

            if(bSystemFont)
                tszFontAdded = TEXT("");
            else if((*fdFontArray)[i].bFontAdded)
                tszFontAdded = TEXT(", Added.");
            else if((*fdFontArray)[i].dwErrorCode==ERROR_SUCCESS)
                tszFontAdded = TEXT(", Not added. Already loaded or present in ROM.");
            else if((*fdFontArray)[i].dwErrorCode==ERROR_INVALID_DATA)
                tszFontAdded = TEXT(", Not added. Unsupported font type.");
            else tszFontAdded = TEXT(", Not added.");

            info(DETAIL, TEXT("    %s, 0x%08x, 0x%08x, 0x%08x, 0x%08x%s"), (*fdFontArray)[i].tszFullPath,
                (*fdFontArray)[i].ft.dwLowDateTime,
                (*fdFontArray)[i].ft.dwHighDateTime,
                (*fdFontArray)[i].dwFileSizeLow,
                (*fdFontArray)[i].dwFileSizeHigh,
                tszFontAdded);
        }
    }
}

void
OutputFontInfo()
{
    if(fontArray)
    {
        for(int i = 0; i < numFonts; i++)
        {
            TCHAR *tszType;
            switch(fontArray[i].dwType)
            {
                case RASTER_FONTTYPE:
                    tszType = TEXT("RASTER_FONTTYPE");
                    break;
                case TRUETYPE_FONTTYPE:
                    tszType = TEXT("TRUETYPE_FONTTYPE");
                    break;
                default:
                    tszType = TEXT("VECTOR_FONTTYPE");
                    break;

            }
            info(DETAIL, TEXT("    \"%s\" \"%s\" %s.  Skiptable is %s."), fontArray[i].tszFaceName, fontArray[i].tszFullName, tszType, fontArray[i].st.SkipString());
            if(fontArray[i].nLinkedFont >= 0)
                info(DETAIL, TEXT("        Linked to font %s, method %d."), fontArray[fontArray[i].nLinkedFont].tszFaceName, fontArray[i].nFontLinkMethod);
        }
    }
}

void
AddAllUserFonts()
{
    // step through all of the fonts in the user fonts array
    // if the font file isn't in one of the other two directories, then addfontresource it and mark it as added.
    for(int i = 0; i < gnUserFontsEntries; i++)
    {
        if( -1 == FindFileInArray(gfdUserFonts[i].tszFileName, &gfdWindowsFonts, gnWindowsFontsEntries) &&
            -1 == FindFileInArray(gfdUserFonts[i].tszFileName, &gfdWindowsFontsFonts, gnWindowsFontsFontsEntries))
        {
            // add font resource
            SetLastError(ERROR_SUCCESS);
            if(AddFontResource(gfdUserFonts[i].tszFullPath) > 0)
            {
                gfdUserFonts[i].bFontAdded = TRUE;
            }
            else
            {
                gfdUserFonts[i].dwErrorCode = GetLastError();
            }
        }
    }
}

void
RemoveAllUserFonts()
{
    // step through all of the fonts in the user array and removefontresource them if they were added.
    for(int i = 0; i < gnUserFontsEntries; i++)
    {
        if(gfdUserFonts[i].bFontAdded)
            RemoveFontResource(gfdUserFonts[i].tszFullPath);
    }
}

// This callback updates a structure with the counts of certain types of fonts
// like how many times a particular fontface was enumerated, how many of each
// charset, and how many of each family.
int CALLBACK
CountEnumsProc(ENUMLOGFONTEX *lpelfe, NEWTEXTMETRICEX *lpntme, DWORD dwFontType, LPARAM lParam)
{
    (* (int *) lParam)++;
    return TRUE;
}

int CALLBACK
FillEnumsProc(ENUMLOGFONTEX *lpelfe, NEWTEXTMETRICEX *lpntme, DWORD dwFontType, LPARAM lParam)
{
    if(numFonts < (* (int *) lParam) && numFonts >= 0)
    {
        // by default, this font isn't linked to anything and doesn't have any font link method.
        fontArray[numFonts].nLinkedFont = -1;
        fontArray[numFonts].nFontLinkMethod = -1;

        fontArray[numFonts].dwType = dwFontType;
        fontArray[numFonts].tszFaceName = new(TCHAR[_tcslen(lpelfe->elfLogFont.lfFaceName) + 1]);
        if(fontArray[numFonts].tszFaceName)
            _tcsncpy_s(fontArray[numFonts].tszFaceName, (_tcslen(lpelfe->elfLogFont.lfFaceName) + 1), lpelfe->elfLogFont.lfFaceName,_TRUNCATE);

        fontArray[numFonts].tszFullName = new(TCHAR[_tcslen(lpelfe->elfFullName) + 1]);
        if(fontArray[numFonts].tszFullName)
            _tcsncpy_s(fontArray[numFonts].tszFullName, (_tcslen(lpelfe->elfFullName) + 1), lpelfe->elfFullName, _TRUNCATE);

        fontArray[numFonts].bItalics = lpelfe->elfLogFont.lfItalic;
        fontArray[numFonts].lWeight = lpelfe->elfLogFont.lfWeight;

        numFonts++;
    }

    return TRUE;
}

void
EnumerateAndAddSystemFonts()
{
    int count = 0;
    HDC hdc = GetDC(NULL);
    LOGFONT lf;

    memset(&lf, 0, sizeof(LOGFONT));
    lf.lfCharSet = DEFAULT_CHARSET;

    // enumerate all of the fonts on the system, and put them in the public font array
    CheckNotRESULT(FALSE, EnumFontFamiliesEx(hdc, &lf, (FONTENUMPROC) CountEnumsProc, (LPARAM) &count, 0));

    if(count)
    {
        fontArray = new(fontInfo[count]);

        if(fontArray)
            CheckNotRESULT(FALSE, EnumFontFamiliesEx(hdc, &lf, (FONTENUMPROC) FillEnumsProc, (LPARAM) &count, 0));
    }
    ReleaseDC(NULL, hdc);
}

void
CleanupSystemFonts()
{
    if(fontArray)
    {
        for(int i = 0; i < numFonts; i++)
        {
            if(fontArray[i].tszFaceName)
                delete[] fontArray[i].tszFaceName;
            if(fontArray[i].tszFullName)
                delete[] fontArray[i].tszFullName;
        }
        delete[] fontArray;
        fontArray = NULL;
        numFonts=0;
    }
}

void
SetupFontIndicies()
{
    int i;
    int nKnownFonts = 0;

    // for each of the fonts in the private array, see if it's available in the system
    for(i = 0; i < privateNumFonts; i++)
    {
        // first we check to see if exactly 1 font matches our requirements
        int index = FindFontInArray(&(privateFontArray[i]), &fontArray, numFonts);
        // if the font was found in the system (exact match for face name, full name, and type
        if(index != -1)
        {
            // now, is the file found in the system
            int nValue1 = FileDataMatchesKnownFont(&(privateFontArray[i]), &gfdUserFonts, gnUserFontsEntries, false);
            int nValue2 = FileDataMatchesKnownFont(&(privateFontArray[i]), &gfdWindowsFonts, gnWindowsFontsEntries, true);
            int nValue3 = FileDataMatchesKnownFont(&(privateFontArray[i]), &gfdWindowsFontsFonts, gnWindowsFontsFontsEntries, true);

            // if any of the three match then set the index bacause we have match for the enumarated characteristics and the
            // file is in the system
            if(nValue1 >= 0 || nValue2 >= 0 || nValue3 >= 0)
            {
                *(privateFontArray[i].pnFontIndex) = index;
                nKnownFonts++;
            }
        }
    }

    // initialize aFont to 0, but LoopPrimarySurface should select the font to use.
    aFont = 0;

    if(nKnownFonts > 0)
    {
        info(DETAIL, TEXT("The known fonts found in the system are"));
        for(i = 0; i < privateNumFonts; i++)
        {
            if(*(privateFontArray[i].pnFontIndex) != -1)
                info(DETAIL, TEXT("  \"%s\" \"%s\""), privateFontArray[i].tszFaceName, privateFontArray[i].tszFullName);
        }
    }
    else info(DETAIL, TEXT("The test was unable to identify any known fonts in the system."));

    // mslogo is a special case, it's just used to draw the logo occationally and always needs to be available.
    if(MSLOGO == -1)
    {
        MSLOGO = aFont;
    }
}

void
InitializeFontLinkingData()
{
    HKEY hkeySystemLink = NULL;
    HKEY hkeySkipTable = NULL;
    HKEY hkeyGDI = NULL;

    TCHAR tszSkipString[MAX_FONTLINKSTRINGLENGTH];
    DWORD dwSkipTableType, dwSkipTableSize;
    DWORD dwFontLinkMethod = 0, dwFontLinkMethodSize;

    RegOpenKeyEx( HKEY_LOCAL_MACHINE ,TEXT("SYSTEM\\GDI"), 0, KEY_QUERY_VALUE, &hkeyGDI);
    if(hkeyGDI)
    {
        dwFontLinkMethodSize = sizeof(DWORD);
        RegQueryValueEx(hkeyGDI, TEXT("FontLinkMethods"), NULL, NULL, (BYTE *) &dwFontLinkMethod, &dwFontLinkMethodSize);
        RegCloseKey(hkeyGDI);
    }

    RegOpenKeyEx( HKEY_LOCAL_MACHINE ,TEXT("SOFTWARE\\Microsoft\\FontLink\\SystemLink"), 0, KEY_QUERY_VALUE, &hkeySystemLink);

    if(hkeySystemLink)
    {
        LONG lEnumReturnValue;
        int nRegIndex = 0;
        TCHAR tszBaseFontName[LF_FACESIZE];
        TCHAR tszLinkedName[MAX_PATH + LF_FACESIZE];
        DWORD dwBaseFontNameCount, dwType, dwLinkedFontSize;

        do
        {
            int nBaseFontIndex;
            int nLinkedFontIndex;
            int nLinkedNameStart;

            tszBaseFontName[0] = NULL;
            tszLinkedName[0] = NULL;
            dwBaseFontNameCount = countof(tszBaseFontName);
            dwLinkedFontSize = sizeof(tszLinkedName);
            lEnumReturnValue = RegEnumValue(hkeySystemLink, nRegIndex, tszBaseFontName, &dwBaseFontNameCount, NULL, &dwType, (LPBYTE) tszLinkedName, &dwLinkedFontSize);

            // if there was a value enumerated
            if(lEnumReturnValue == ERROR_SUCCESS)
            {
                nBaseFontIndex = FindFontFaceInArray(tszBaseFontName, &fontArray, numFonts);

                // get the index for the linked font, step through the value returned above until we hit the comma, which indicates the start
                // of the font name.
                for(nLinkedNameStart =0; tszLinkedName[nLinkedNameStart] != ',' && tszLinkedName[nLinkedNameStart]; nLinkedNameStart++);
                if(tszLinkedName[nLinkedNameStart] == ',')
                    nLinkedNameStart++;

                nLinkedFontIndex = FindFontFaceInArray(&(tszLinkedName[nLinkedNameStart]), &fontArray, numFonts);
                if(nBaseFontIndex >= 0 && nLinkedFontIndex >= 0)
                {
                    fontArray[nBaseFontIndex].nLinkedFont = nLinkedFontIndex;
                    fontArray[nBaseFontIndex].nFontLinkMethod = dwFontLinkMethod;
                }
            }

            nRegIndex++;
        }while(lEnumReturnValue == ERROR_SUCCESS);

        // setup the skip table for the font.
        RegOpenKeyEx( HKEY_LOCAL_MACHINE ,TEXT("SOFTWARE\\Microsoft\\FontLink\\SkipTable"), 0, KEY_QUERY_VALUE, &hkeySkipTable);
        if(hkeySkipTable)
        {
            if(fontArray)
            {
                for(int i = 0; i < numFonts; i++)
                {
                    dwSkipTableSize = sizeof(tszSkipString);
                    tszSkipString[0] = NULL;
                    RegQueryValueEx(hkeySkipTable, fontArray[i].tszFaceName, NULL, &dwSkipTableType, (LPBYTE) tszSkipString, &dwSkipTableSize);
                    fontArray[i].st.InitializeSkipTable(tszSkipString);
                }
            }
            RegCloseKey(hkeySkipTable);

        }
        RegCloseKey(hkeySystemLink);
    }
}

//***********************************************************************************
HFONT setupKnownFont(int index, long Hei, long Wid, long Esc, long Ori, BYTE Und, BYTE Str)
{
    LOGFONT lFont;
    HFONT   hFont;

    // set up log info
    lFont.lfHeight = Hei;
    lFont.lfWidth = Wid;
    lFont.lfEscapement = Esc;
    lFont.lfOrientation = Ori;
    lFont.lfWeight = fontArray[index].lWeight;
    lFont.lfItalic = fontArray[index].bItalics;
    lFont.lfUnderline = Und;
    lFont.lfStrikeOut = Str;

    lFont.lfCharSet = DEFAULT_CHARSET;
    lFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lFont.lfQuality = DEFAULT_QUALITY;
    lFont.lfPitchAndFamily = DEFAULT_PITCH;

    _tcsncpy_s(lFont.lfFaceName, fontArray[index].tszFaceName, LF_FACESIZE-1);
    // create and select in font
    CheckNotRESULT(NULL, hFont = CreateFontIndirect(&lFont));

    return hFont;
}

//***********************************************************************************
void
initFonts(void)
{
    FindAllFonts(WINDOWSDIRECTORY, &gfdWindowsFonts, &gnWindowsFontsEntries);
    FindAllFonts(WINDOWSFONTSDIRECTORY, &gfdWindowsFontsFonts, &gnWindowsFontsFontsEntries);
    FindAllFonts(gszFontLocation, &gfdUserFonts, &gnUserFontsEntries);
    AddAllUserFonts();
    EnumerateAndAddSystemFonts();
    SetupFontIndicies();

    InitializeFontLinkingData();

    info(DETAIL, TEXT("The fonts in use are:"));
    OutputFontInfo();

    if(gfdWindowsFonts && gnWindowsFontsEntries > 0)
    {
        info(DETAIL, TEXT("The fonts in \\windows are:"));
        OutputFileInfo(&gfdWindowsFonts, gnWindowsFontsEntries, true);
    }
    else info(DETAIL, TEXT("No fonts found in \\windows."));

    if(gfdWindowsFontsFonts && gnWindowsFontsFontsEntries > 0)
    {
        info(DETAIL, TEXT("The fonts in \\windows\\fonts are:"));
        OutputFileInfo(&gfdWindowsFontsFonts, gnWindowsFontsFontsEntries, true);
    }
    else info(DETAIL, TEXT("No fonts found in \\windows\\fonts."));

    if(gfdUserFonts && gnUserFontsEntries > 0)
    {
        info(DETAIL, TEXT("The fonts in the user specified directory %s are:"), gszFontLocation);
        OutputFileInfo(&gfdUserFonts, gnUserFontsEntries, false);
    }
    else
    {
        if (gszFontLocation && gszFontLocation[0])
            info(DETAIL, TEXT("No user fonts added from %s."), gszFontLocation);
        else
            info(DETAIL, TEXT("No user fonts added. Use the /FontLocation command line to add additional fonts."));
    }

}

//***********************************************************************************
void
freeFonts(void)
{
    // remove the user fonts first.
    RemoveAllUserFonts();

    CleanupFontArray(&gfdWindowsFonts, gnWindowsFontsEntries);
    CleanupFontArray(&gfdWindowsFontsFonts, gnWindowsFontsFontsEntries);
    CleanupFontArray(&gfdUserFonts, gnUserFontsEntries);
    CleanupSystemFonts();
}

/***********************************************************************************
***
***   passCheck
***
************************************************************************************/

//***********************************************************************************
int
passCheck(int result, int expected, TCHAR * args)
{

    if (result != expected)
    {
        DWORD   dwError = GetLastError();

        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
        {
            info(DETAIL, TEXT("returned 0x%lX expected 0x%lX when passed %s:  caused by out of memory: GLE:%d"), result,
                 expected, args, dwError);
            return 1;
        }
        else
        {
            info(FAIL, TEXT("returned 0x%lX expected 0x%lX when passed %s"), result, expected, args);
            return 0;
        }
    }

    return 1;
}

/***********************************************************************************
***
***   pass Empty Rect
***
************************************************************************************/

//***********************************************************************************
void
passEmptyRect(int testFunc, TCHAR * name)
{
    info(ECHO, TEXT("%s - Testing with empty Rect"), name);

    TDC     hdc = myGetDC();
    RECT    emptyRect = { 5, 10, 5, 20 };
    HRGN    hRgn,
            areaRgn;

    int     expected = 0;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hRgn = CreateRectRgn(0, 0, 4, 4));
        CheckNotRESULT(NULL, areaRgn = CreateRectRgn(0, 0, 499, 499));

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

        CheckNotRESULT(ERROR, SelectClipRgn(hdc, areaRgn));

        switch (testFunc)
        {
            case ERectInRegion:
                CheckForRESULT(0, RectInRegion(hRgn, &emptyRect));
                break;
            case EIntersectClipRect:
                CheckForRESULT(NULLREGION, IntersectClipRect(hdc, emptyRect.left, emptyRect.top, emptyRect.right, emptyRect.bottom));
                break;
            case EExcludeClipRect:
    #ifdef UNDER_CE
                expected = SIMPLEREGION;
    #else // UNDER_CE
                expected = COMPLEXREGION;
    #endif // UNDER_CE
                CheckForRESULT(expected, ExcludeClipRect(hdc, emptyRect.left, emptyRect.top, emptyRect.right, emptyRect.bottom));
                break;
        }
        CheckNotRESULT(FALSE, DeleteObject(areaRgn));
        CheckNotRESULT(FALSE, DeleteObject(hRgn));
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Ask User
***
************************************************************************************/

int     DirtyFlag;

//***********************************************************************************
void
AskMessage(TCHAR * APIname, TCHAR * FuncName, TCHAR * outStr)
{

    int     result = 1;
    TCHAR   questionStr[256] = {NULL};

    DirtyFlag = 0;

    if (verifyFlag != EVerifyManual || isMemDC())
        return;

    _sntprintf_s(questionStr, countof(questionStr) - 1, TEXT("Did you see %s?"), outStr);

    result = MessageBox(NULL, questionStr, TEXT("Manual Test Verification"), MB_YESNOCANCEL | MB_DEFBUTTON1);

    if (result == IDNO)
        info(FAIL, TEXT("%s in Manual Test<%s> User Replied <NO>:%s"), APIname, FuncName, questionStr);
    else if (result == IDCANCEL)
        verifyFlag = EVerifyAuto;

    DirtyFlag = (verifyFlag == EVerifyAuto) || (result == IDCANCEL);
}

/***********************************************************************************
***
***   Remove All Fonts
***
************************************************************************************/

void
MyDelay(int i)
{
#ifdef VISUAL_CHECK
    TCHAR   sz[32];

    _sntprintf_s(sz, countof(sz)-1, TEXT("i=%d"), i);
    MessageBox(NULL, sz, TEXT("Check"), MB_OK);
#endif
}

TCHAR  *
cMapClass::operator[] (int index)
{
    // initialize it to entry 0's string, which is "skip"
    TCHAR  *
        tcTmp = m_funcEntryStruct[0].szName;

    for (int i = 0; NULL != m_funcEntryStruct[i].szName; i++)
        if (m_funcEntryStruct[i].dwVal == (DWORD) index)
            tcTmp = m_funcEntryStruct[i].szName;

    return tcTmp;
}

// Search through the list and find the entry ID for the string given.
int
cMapClass::operator() (TCHAR * tcString)
{
    for(int i = 0; NULL != m_funcEntryStruct[i].szName; i++)
        if(!_tcsicmp(tcString, m_funcEntryStruct[i].szName))
            return m_funcEntryStruct[i].dwVal;
    return 0;
}

#define ENTRY(MSG, TestID) {TestID, MSG}

NameValPair funcEntryStruct[] =
{
    // inits
    ENTRY(TEXT("String Not Found"), EInvalidEntry),
    // verifcation
    ENTRY(TEXT("VerifyManual"), EVerifyManual),
    ENTRY(TEXT("VerifyAuto"), EVerifyAuto),
    // extended verification
    ENTRY(TEXT("DDI_Test.dll verification"), EVerifyDDITEST),
    ENTRY(TEXT("Never use DDI_Test.dll verification"), EVerifyNone),
    ENTRY(TEXT("Always use DDI_Test.dll verification"), EVerifyAlways),

    // ***********************
    // test start
    // clip
    ENTRY(TEXT("ExcludeClipRect"), EExcludeClipRect),
    ENTRY(TEXT("GetClipBox"), EGetClipBox),
    ENTRY(TEXT("GetClipRgn"),EGetClipRgn),
    ENTRY(TEXT("IntersectClipRect"), EIntersectClipRect),
    ENTRY(TEXT("SelectClipRgn"), ESelectClipRgn),
    // draw
    ENTRY(TEXT("BitBlt"), EBitBlt),
    ENTRY(TEXT("ClientToScreen"), EClientToScreen),
    ENTRY(TEXT("CreateBitmap"), ECreateBitmap),
    ENTRY(TEXT("CreateCompatibleBitmap"), ECreateCompatibleBitmap),
    ENTRY(TEXT("CreateDIBSection"), ECreateDIBSection),
    ENTRY(TEXT("Ellipse"), EEllipse),
    ENTRY(TEXT("DrawEdge"), EDrawEdge),
    ENTRY(TEXT("GetPixel"), EGetPixel),
    ENTRY(TEXT("MaskBlt"), EMaskBlt),
    ENTRY(TEXT("PatBlt"), EPatBlt),
    ENTRY(TEXT("Polygon"), EPolygon),
    ENTRY(TEXT("Polyline"), EPolyline),
    ENTRY(TEXT("Rectangle"), ERectangle),
    ENTRY(TEXT("FillRect"), EFillRect),
    ENTRY(TEXT("RectVisible"), ERectVisible),
    ENTRY(TEXT("RoundRect"), ERoundRect),
    ENTRY(TEXT("ScreenToClient"), EScreenToClient),
    ENTRY(TEXT("SetPixel"), ESetPixel),
    ENTRY(TEXT("StretchBlt"), EStretchBlt),
    ENTRY(TEXT("Transparent Image"), ETransparentImage),
    ENTRY(TEXT("StretchDIBits"), EStretchDIBits),
    ENTRY(TEXT("SetDIBitsToDevice"), ESetDIBitsToDevice),
    ENTRY(TEXT("GradientFill"), EGradientFill),
    ENTRY(TEXT("InvertRect"), EInvertRect),
    ENTRY(TEXT("MoveToEx"), EMoveToEx),
    ENTRY(TEXT("LineTo"), ELineTo),
    ENTRY(TEXT("GetDIBColorTable"), EGetDIBColorTable),
    ENTRY(TEXT("SetDIBColorTAble"), ESetDIBColorTable),
    ENTRY(TEXT("SetBitmapBits"), ESetBitmapBits),
    ENTRY(TEXT("DrawFocusRect"), EDrawFocusRect),
    ENTRY(TEXT("SetROP2"), ESetROP2),
    ENTRY(TEXT("AlphaBlend"), EAlphaBlend),
    ENTRY(TEXT("MapWindowPoints"), EMapWindowPoints),
    ENTRY(TEXT("GetROP2"), EGetROP2),
    // pal
    ENTRY(TEXT("GetNearestColor"), EGetNearestColor),
    ENTRY(TEXT("GetNearestPaletteIndex"), EGetNearestPaletteIndex),
    ENTRY(TEXT("GetSystemPaletteEntries"), EGetSystemPaletteEntries),
    ENTRY(TEXT("GetPaletteEntries"), EGetPaletteEntries),
    ENTRY(TEXT("SetPaletteEntries"), ESetPaletteEntries),
    ENTRY(TEXT("CreatePalette"), ECreatePalette),
    ENTRY(TEXT("SelectPalette"), ESelectPalette),
    ENTRY(TEXT("RealizePalette"), ERealizePalette),
    // GDI
    ENTRY(TEXT("GdiFlush"), EGdiFlush),
    ENTRY(TEXT("GdiSetBatchLimit"), EGdiSetBatchLimit),
    ENTRY(TEXT("ChangeDisplaySettingsEx"), EChangeDisplaySettingsEx),
    // region
    ENTRY(TEXT("CombineRgn"), ECombineRgn),
    ENTRY(TEXT("CreateRectRgn"), ECreateRectRgn),
    ENTRY(TEXT("CreateRectRgnIndirect"), ECreateRectRgnIndirect),
    ENTRY(TEXT("EqualRgn"), EEqualRgn),
    ENTRY(TEXT("FillRgn"), EFillRgn),
    ENTRY(TEXT("FrameRgn"), EFrameRgn),
    ENTRY(TEXT("GetRegionData"), EGetRegionData),
    ENTRY(TEXT("GetRgnBox"), EGetRgnBox),
    ENTRY(TEXT("OffsetRgn"), EOffsetRgn),
    ENTRY(TEXT("PtInRegion"), EPtInRegion),
    ENTRY(TEXT("RectInRegion"), ERectInRegion),
    ENTRY(TEXT("SetRectRgn"), ESetRectRgn),
    ENTRY(TEXT("ExtCreateRegion"), EExtCreateRegion),
    // brush & pens
    ENTRY(TEXT("CreateDIBPatternBrushPt"), ECreateDIBPatternBrushPt),
    ENTRY(TEXT("CreatePatternBrush"), ECreatePatternBrush),
    ENTRY(TEXT("CreatSolidBrush"), ECreateSolidBrush),
    ENTRY(TEXT("GetBrushOrgEx"), EGetBrushOrgEx),
    ENTRY(TEXT("GetSysColorBrush"), EGetSysColorBrush),
    ENTRY(TEXT("SetBrushOrgEx"), ESetBrushOrgEx),
    ENTRY(TEXT("CreatePenIndirect"), ECreatePenIndirect),
    ENTRY(TEXT("SystemStockBrush"), ESystemStockBrush),
    ENTRY(TEXT("CreatePen"), ECreatePen),
    // da
    ENTRY(TEXT("GetBkColor"), EGetBkColor),
    ENTRY(TEXT("GetBkMode"), EGetBkMode),
    ENTRY(TEXT("GetTextColor"), EGetTextColor),
    ENTRY(TEXT("SetBkColor"), ESetBkColor),
    ENTRY(TEXT("SetBkMode"), ESetBkMode),
    ENTRY(TEXT("SetTextColor"), ESetTextColor),
    ENTRY(TEXT("SetViewportOrgEx"), ESetViewportOrgEx),
    ENTRY(TEXT("SetStretchBltMode"), ESetStretchBltMode),
    ENTRY(TEXT("GetStretchBltMode"), EGetStretchBltMode),
    ENTRY(TEXT("SetTextCharacterExtra"), ESetTextCharacterExtra),
    ENTRY(TEXT("GetTextCharacterExtra"), EGetTextCharacterExtra),
    ENTRY(TEXT("SetLayout"), ESetLayout),
    ENTRY(TEXT("GetLayout"), EGetLayout),
    ENTRY(TEXT("GetViewportOrgEx"), EGetViewportOrgEx),
    ENTRY(TEXT("GetViewportExtEx"), EGetViewportExtEx),
    ENTRY(TEXT("SetWindowOrgEx"), ESetWindowOrgEx),
    ENTRY(TEXT("GetWindowOrgEx"), EGetWindowOrgEx),
    ENTRY(TEXT("OffsetViewportOrgEx"), EOffsetViewportOrgEx),
    ENTRY(TEXT("GetWindowExtEx"), EGetWindowExtEx),
    // dc
    ENTRY(TEXT("CreateCompatibleDC"), ECreateCompatibleDC),
    ENTRY(TEXT("DeleteDC"), EDeleteDC),
    ENTRY(TEXT("GetDCOrgEx"), EGetDCOrgEx),
    ENTRY(TEXT("GetDeviceCaps"), EGetDeviceCaps),
    ENTRY(TEXT("RestoreDC"), ERestoreDC),
    ENTRY(TEXT("SaveDC"), ESaveDC),
    ENTRY(TEXT("ScrollDC"), EScrollDC),
    ENTRY(TEXT("CreateDC"), ECreateDC),
    ENTRY(TEXT("ExtEscape"), EExtEscape),
    ENTRY(TEXT("BitmapEscape"), EBitmapEscape),
    // do
    ENTRY(TEXT("DeleteObject"), EDeleteObject),
    ENTRY(TEXT("GetCurrentObject"), EGetCurrentObject),
    ENTRY(TEXT("GetObject"), EGetObject),
    ENTRY(TEXT("GetObjectType"), EGetObjectType),
    ENTRY(TEXT("GetStockObject"), EGetStockObject),
    ENTRY(TEXT("SelectObject"), ESelectObject),
    ENTRY(TEXT("UnrealizeObject"), EUnrealizeObject),
    // font
    ENTRY(TEXT("AddFontResource"), EAddFontResource),
    ENTRY(TEXT("CreateFontIndirect"), ECreateFontIndirect),
    ENTRY(TEXT("EnumFonts"), EEnumFonts),
    ENTRY(TEXT("EnumFontFamilies"), EEnumFontFamilies),
    ENTRY(TEXT("EnumFontFamProc"), EEnumFontFamProc),
    ENTRY(TEXT("GetCharABCWidths"), EGetCharABCWidths),
    ENTRY(TEXT("GetCharWidth"), EGetCharWidth),
    ENTRY(TEXT("GetCharWidth32"), EGetCharWidth32),
    ENTRY(TEXT("RemoveFontResource"), ERemoveFontResource),
    ENTRY(TEXT("EnableEUDC"), EEnableEUDC),
    ENTRY(TEXT("TranslateCharsetInfo"), ETranslateCharsetInfo),
    ENTRY(TEXT("EnumFontFamiliesEx"), EEnumFontFamiliesEx),
    // text
    ENTRY(TEXT("DrawTextW"), EDrawTextW),
    ENTRY(TEXT("ExtTextOut"), EExtTextOut),
    ENTRY(TEXT("GetTextAlign"), EGetTextAlign),
    ENTRY(TEXT("GetTextExtentPoint"), EGetTextExtentPoint),
    ENTRY(TEXT("GetTextExtentPoint32"), EGetTextExtentPoint32),
    ENTRY(TEXT("GetTextExtentExPoint"), EGetTextExtentExPoint),
    ENTRY(TEXT("GetTextFace"), EGetTextFace),
    ENTRY(TEXT("GetTextMetrics"), EGetTextMetrics),
    ENTRY(TEXT("SetTextAlign"), ESetTextAlign),
    // print
    ENTRY(TEXT("StartDoc"), EStartDoc),
    ENTRY(TEXT("EndDoc"), EEndDoc),
    ENTRY(TEXT("StartPage"), EStartPage),
    ENTRY(TEXT("EndPage"), EEndPage),
    ENTRY(TEXT("AbortDoc"), EAbortDoc),
    ENTRY(TEXT("SetAbortProc"), ESetAbortProc),
    // testchk
    ENTRY(TEXT("CheckAllWhite"), ECheckAllWhite),
    ENTRY(TEXT("CheckScreenHalves"), ECheckScreenHalves),
    ENTRY(TEXT("GetReleaseDC"), EGetReleaseDC),
    ENTRY(TEXT("Thrash"), EThrash),
    ENTRY(TEXT("CheckDriverVerify"), ECheckDriverVerify),
    // Manual
    ENTRY(TEXT("ManualFont"), EManualFont),
    ENTRY(TEXT("ManualFontClip"), EManualFontClip),
    ENTRY(NULL, NULL)
};

NameValPair DCEntryCommonStruct[] =
{
    ENTRY(TEXT("String Not Found"), EInvalidEntry),
    // gdi surfaces
    ENTRY(TEXT("Win_Primary"), EWin_Primary),
    ENTRY(TEXT("GDI_VidMemory"), EGDI_VidMemory),
    ENTRY(TEXT("GDI_SysMemory"), EGDI_SysMemory),
    ENTRY(NULL, NULL)
};

NameValPair DCEntryExtendedStruct[] =
{
    ENTRY(TEXT("String Not Found"), EInvalidEntry),
    // gdi surfaces
    ENTRY(TEXT("Win_Primary"), EWin_Primary),
    ENTRY(TEXT("GDI_VidMemory"), EGDI_VidMemory),
    ENTRY(TEXT("GDI_SysMemory"), EGDI_SysMemory),
    ENTRY(TEXT("1BPP_BITMAP"), EGDI_1bppBMP),
    ENTRY(TEXT("1BPP_DIB_RGB"), EGDI_1bppRGBDIB),
#ifdef UNDER_CE
    // The desktop doesn't support 2bpp DIB's at all.
    ENTRY(TEXT("2BPP_DIB_RGB"), EGDI_2bppRGBDIB),
#endif
    ENTRY(TEXT("4BPP_DIB_RGB"), EGDI_4bppRGBDIB),
    ENTRY(TEXT("8BPP_DIB_RGB"), EGDI_8bppRGBDIB),
    ENTRY(TEXT("16BPP_DIB_RGB565"), EGDI_16bppRGB565DIB),
    ENTRY(TEXT("24BPP_DIB_RGB"), EGDI_24bppRGBDIB),
    ENTRY(TEXT("32BPP_DIB_RGB8888"), EGDI_32bppRGB8888DIB),
    ENTRY(NULL, NULL)
};

NameValPair DCEntryAllStruct[] =
{
    ENTRY(TEXT("String Not Found"), EInvalidEntry),
    // gdi surfaces
    ENTRY(TEXT("Win_Primary"), EWin_Primary),
    ENTRY(TEXT("Full_Screen"), EFull_Screen),
    ENTRY(TEXT("GDI_VidMemory"), EGDI_VidMemory),
    ENTRY(TEXT("GDI_SysMemory"), EGDI_SysMemory),
    ENTRY(TEXT("CreateDC_Primary"), ECreateDC_Primary),
// On the desktop this requires having a printer to print everything tested,
// which requires a significant amount of paper.
#ifdef UNDER_CE
    ENTRY(TEXT("GDI_Printer"), EGDI_Printer),
#endif
    ENTRY(TEXT("1BPP_BITMAP"), EGDI_1bppBMP),
#ifdef UNDER_CE
    // the desktop does not allow bitmaps of a bit depth other than the primary to be
    // selected into a dc compatible to the primary.
    ENTRY(TEXT("2BPP_BITMAP"), EGDI_2bppBMP),
    ENTRY(TEXT("4BPP_BITMAP"), EGDI_4bppBMP),
    ENTRY(TEXT("8BPP_BITMAP"), EGDI_8bppBMP),
    ENTRY(TEXT("16BPP_BITMAP"), EGDI_16bppBMP),
    ENTRY(TEXT("24BPP_BITMAP"), EGDI_24bppBMP),
    ENTRY(TEXT("32BPP_BITMAP"), EGDI_32bppBMP),
    ENTRY(TEXT("8BPP_DIB_Pal"), EGDI_8bppPalDIB),
#endif
    ENTRY(TEXT("1BPP_DIB_RGB"), EGDI_1bppRGBDIB),
#ifdef UNDER_CE
    // The desktop doesn't support 2bpp DIB's at all.
    ENTRY(TEXT("2BPP_DIB_RGB"), EGDI_2bppRGBDIB),
#endif
    ENTRY(TEXT("4BPP_DIB_RGB"), EGDI_4bppRGBDIB),
    ENTRY(TEXT("8BPP_DIB_RGB"), EGDI_8bppRGBDIB),
    ENTRY(TEXT("16BPP_DIB_RGB"), EGDI_16bppRGBDIB),
    ENTRY(TEXT("16BPP_DIB_RGB4444"), EGDI_16bppRGB4444DIB),
    ENTRY(TEXT("16BPP_DIB_RGB1555"), EGDI_16bppRGB1555DIB),
    ENTRY(TEXT("16BPP_DIB_RGB555"), EGDI_16bppRGB555DIB),
    ENTRY(TEXT("16BPP_DIB_RGB565"), EGDI_16bppRGB565DIB),
    ENTRY(TEXT("16BPP_DIB_BGR4444"), EGDI_16bppBGR4444DIB),
    ENTRY(TEXT("16BPP_DIB_BGR1555"), EGDI_16bppBGR1555DIB),
    ENTRY(TEXT("16BPP_DIB_BGR555"), EGDI_16bppBGR555DIB),
    ENTRY(TEXT("16BPP_DIB_BGR565"), EGDI_16bppBGR565DIB),
    ENTRY(TEXT("24BPP_DIB_RGB"), EGDI_24bppRGBDIB),
    ENTRY(TEXT("32BPP_DIB_RGB"), EGDI_32bppRGBDIB),
    ENTRY(TEXT("32BPP_DIB_RGB8888"), EGDI_32bppRGB8888DIB),
    ENTRY(TEXT("32BPP_DIB_RGB888"), EGDI_32bppRGB888DIB),
    ENTRY(TEXT("32BPP_DIB_BGR8888"), EGDI_32bppBGR8888DIB),
    ENTRY(TEXT("32BPP_DIB_BGR888"), EGDI_32bppBGR888DIB),
#ifdef UNDER_CE
    // 8bpp DIB's with DIB_PAL_COLORS on the desktop doesn't have
    // a white pixel, just light gray.
    ENTRY(TEXT("8BPP_DIB_Pal_BUB"), EGDI_8bppPalDIBBUB),
#endif
    ENTRY(TEXT("1BPP_DIB_RGB_BUB"), EGDI_1bppRGBDIBBUB),
#ifdef UNDER_CE
    // The desktop doesn't support 2bpp DIB's at all.
    ENTRY(TEXT("2BPP_DIB_RGB_BUB"), EGDI_2bppRGBDIBBUB),
#endif
    ENTRY(TEXT("4BPP_DIB_RGB_BUB"), EGDI_4bppRGBDIBBUB),
    ENTRY(TEXT("8BPP_DIB_RGB_BUB"), EGDI_8bppRGBDIBBUB),
    ENTRY(TEXT("16BPP_DIB_RGB_BUB"), EGDI_16bppRGBDIBBUB),
    ENTRY(TEXT("16BPP_DIB_RGB4444_BUB"), EGDI_16bppRGB4444DIBBUB),
    ENTRY(TEXT("16BPP_DIB_RGB1555_BUB"), EGDI_16bppRGB1555DIBBUB),
    ENTRY(TEXT("16BPP_DIB_RGB555_BUB"), EGDI_16bppRGB555DIBBUB),
    ENTRY(TEXT("16BPP_DIB_RGB565_BUB"), EGDI_16bppRGB565DIBBUB),
    ENTRY(TEXT("16BPP_DIB_BGR4444_BUB"), EGDI_16bppBGR4444DIBBUB),
    ENTRY(TEXT("16BPP_DIB_BGR1555_BUB"), EGDI_16bppBGR1555DIBBUB),
    ENTRY(TEXT("16BPP_DIB_BGR555_BUB"), EGDI_16bppBGR555DIBBUB),
    ENTRY(TEXT("16BPP_DIB_BGR565_BUB"), EGDI_16bppBGR565DIBBUB),
    ENTRY(TEXT("24BPP_DIB_RGB_BUB"), EGDI_24bppRGBDIBBUB),
    ENTRY(TEXT("32BPP_DIB_RGB_BUB"), EGDI_32bppRGBDIBBUB),
    ENTRY(TEXT("32BPP_DIB_RGB8888_BUB"), EGDI_32bppRGB8888DIBBUB),
    ENTRY(TEXT("32BPP_DIB_RGB888_BUB"), EGDI_32bppRGB888DIBBUB),
    ENTRY(TEXT("32BPP_DIB_BGR8888_BUB"), EGDI_32bppBGR8888DIBBUB),
    ENTRY(TEXT("32BPP_DIB_BGR888_BUB"), EGDI_32bppBGR888DIBBUB),
    ENTRY(NULL, NULL)
};

#undef ENTRY

MapClass funcName(&funcEntryStruct[0]);
MapClass DCName(&DCEntryAllStruct[0]);

BYTE GetROP2ChannelValue(BYTE Dest, BYTE Pen, DWORD dwRop)
{
    BYTE bResultingColor = 0;

    switch(dwRop)
    {
        case(R2_BLACK):
            bResultingColor = 0; // 0
            break;
        case(R2_COPYPEN):
            bResultingColor = Pen; // P
            break;
        case(R2_MASKNOTPEN):
            bResultingColor = Dest & ~Pen; // DPna
            break;
        case(R2_MASKPEN):
            bResultingColor = Dest & Pen; // DPa
            break;
        case(R2_MASKPENNOT):
            bResultingColor = Pen & ~Dest; // PDna
            break;
        case(R2_MERGENOTPEN):
            bResultingColor = Dest | ~Pen; // DPno
            break;
        case(R2_MERGEPEN):
            bResultingColor = Dest | Pen; // DPo
            break;
        case(R2_MERGEPENNOT):
            bResultingColor = Pen | ~Dest; // PDno
            break;
        case(R2_NOP):
            bResultingColor = Dest; // D
            break;
        case(R2_NOT):
            bResultingColor = ~Dest; // Dn
            break;
        case(R2_NOTCOPYPEN):
            bResultingColor = ~Pen; // Pn
            break;
        case(R2_NOTMASKPEN):
            bResultingColor = ~(Dest & Pen); // DPan
            break;
        case(R2_NOTMERGEPEN):
            bResultingColor = ~(Dest | Pen); // DPon
            break;
        case(R2_NOTXORPEN):
            bResultingColor = ~(Dest ^ Pen); // DPxn
            break;
        case(R2_WHITE):
            bResultingColor = 0xFF; // 1
            break;
        case(R2_XORPEN):
            bResultingColor = (Dest ^ Pen); // DPx
            break;
    }
    return bResultingColor;
}

// this function takes in the byte values for a source, dest, and pattern and combines
// them using the definition of each ROP value.
BYTE GetChannelValue(BYTE Source, BYTE Dest, BYTE Pattern, DWORD dwRop)
{
    // get rid of the lower 16 bits, and make sure the upper 16 are 0's.
    // This gives us a value between 0 & 255, one of the 256 raster operations.
    dwRop = dwRop >> 16;
    dwRop &= 0xFF;

    BYTE bResultingColor = 0;

    switch(dwRop)
    {
        case 0x00:
            bResultingColor = 0x0; // 0 (BLACKNESS)
            break;
        case 0x01:
            bResultingColor = ~(Dest | (Pattern | Source)); // DPSoon
            break;
        case 0x02:
            bResultingColor = Dest & ~(Pattern | Source); // DPSona
            break;
        case 0x03:
            bResultingColor = ~(Pattern | Source); // PSon
            break;
        case 0x04:
            bResultingColor = Source & ~(Dest | Pattern); // SDPona
            break;
        case 0x05:
            bResultingColor = ~(Dest | Pattern); // DPon
            break;
        case 0x06:
            bResultingColor = ~(Pattern | ~(Dest ^ Source)); // PDSxnon
            break;
        case 0x07:
            bResultingColor = ~(Pattern | (Dest & Source)); // PDSaon
            break;
        case 0x08:
            bResultingColor = Source & (Dest & ~Pattern); // SDPnaa
            break;
        case 0x09:
            bResultingColor = ~(Pattern | (Dest ^ Source)); // PDSxon
            break;
        case 0x0A:
            bResultingColor = Dest & ~Pattern; // DPna
            break;
        case 0x0B:
            bResultingColor = ~(Pattern | (Source & ~Dest)); // PSDnaon
            break;
        case 0x0C:
            bResultingColor = Source & ~Pattern; // SPna
            break;
        case 0x0D:
            bResultingColor = ~(Pattern | (Dest & ~Source)); // PDSnaon
            break;
        case 0x0E:
            bResultingColor = ~(Pattern | ~(Dest | Source)); // PDSonon
            break;
        case 0x0F:
            bResultingColor = ~Pattern; // Pn
            break;
        case 0x10:
            bResultingColor = Pattern & ~(Dest | Source); // PDSona
            break;
        case 0x11:
            bResultingColor = ~(Dest | Source); // DSon (NOTSRCERASE)
            break;
        case 0x12:
            bResultingColor = ~(Source | ~(Dest ^ Pattern)); // SDPxnon
            break;
        case 0x13:
            bResultingColor = ~(Source | (Dest & Pattern)); // SDPaon
            break;
        case 0x14:
            bResultingColor = ~(Dest | ~(Pattern ^ Source)); // DPSxnon
            break;
        case 0x15:
            bResultingColor = ~(Dest | (Pattern & Source)); // DPSaon
            break;
        case 0x16:
            bResultingColor = Pattern ^ (Source ^ (Dest & ~(Pattern & Source))); // PSDPSanaxx
            break;
        case 0x17:
            bResultingColor = ~(Source ^ ((Source ^ Pattern) & (Dest ^ Source))); // SSPxDSxaxn
            break;
        case 0x18:
            bResultingColor = (Source ^ Pattern) & (Pattern ^ Dest); // SPxPDxa
            break;
        case 0x19:
            bResultingColor = ~(Source ^ (Dest & ~(Pattern & Source))); // SDPSanaxn
            break;
        case 0x1A:
            bResultingColor = Pattern ^ (Dest | (Source & Pattern)); // PDSPaox
            break;
        case 0x1B:
            bResultingColor = ~(Source ^ (Dest & (Pattern ^ Source))); // SDPSxaxn
            break;
        case 0x1C:
            bResultingColor = Pattern ^ (Source | (Dest & Pattern)); // PSDPaox
            break;
        case 0x1D:
            bResultingColor = ~(Dest ^ (Source & (Pattern ^ Dest))); // DSPDxaxn
            break;
        case 0x1E:
            bResultingColor = Pattern ^ (Dest | Source); // PDSox
            break;
        case 0x1F:
            bResultingColor = ~(Pattern & (Dest | Source)); // PDSoan
            break;
        case 0x20:
            bResultingColor = Dest & (Pattern & ~Source); // DPSnaa
            break;
        case 0x21:
            bResultingColor = ~(Source | (Dest ^ Pattern)); // SDPxon
            break;
        case 0x22:
            bResultingColor = Dest & ~Source; // DSna
            break;
        case 0x23:
            bResultingColor = ~(Source | ( Pattern & ~Dest)); // SPDnaon
            break;
        case 0x24:
            bResultingColor = (Source ^ Pattern) & (Dest ^ Source); // SPxDSxa
            break;
        case 0x25:
            bResultingColor = ~(Pattern ^ (Dest & ~(Source & Pattern))); // PDSPanaxn
            break;
        case 0x26:
            bResultingColor = Source ^ (Dest | (Pattern & Source)); // SDPSaox
            break;
        case 0x27:
            bResultingColor = Source ^ (Dest | ~(Pattern ^ Source)); // SDPSxnox
            break;
        case 0x28:
            bResultingColor = Dest & (Pattern ^ Source); //DPSxa
            break;
        case 0x29:
            bResultingColor = ~(Pattern ^ (Source ^ (Dest | (Pattern & Source)))); //PSDPSaoxxn
            break;
        case 0x2A:
            bResultingColor = Dest & ~(Pattern & Source); // DPSana
            break;
        case 0x2B:
            bResultingColor =~(Source ^ ((Source ^ Pattern) & (Pattern ^ Dest))); //SSPxPDxaxn
            break;
        case 0x2C:
            bResultingColor = Source ^ (Pattern & (Dest | Source)); //SPDSoax
            break;
        case 0x2D:
            bResultingColor = Pattern ^ (Source | ~Dest); // PSDnox
            break;
        case 0x2E:
            bResultingColor = Pattern ^ (Source | (Dest ^ Pattern)); // PSDPxox
            break;
        case 0x2F:
            bResultingColor = ~(Pattern & (Source | ~Dest)); // PSDnoan
            break;
        case 0x30:
            bResultingColor = Pattern & ~Source; // PSna
            break;
        case 0x31:
            bResultingColor = ~(Source | (Dest & ~Pattern)); // SDPnaon
            break;
        case 0x32:
            bResultingColor = Source ^ (Dest | (Pattern | Source)); // SDPSoox
            break;
        case 0x33:
            bResultingColor = ~Source; // Sn (NOTSRCCOPY)
            break;
        case 0x34:
            bResultingColor = Source ^ (Pattern | (Dest & Source)); // SPDSaox
            break;
        case 0x35:
            bResultingColor = Source ^ ( Pattern | ~(Dest ^ Source)); // SPDSxnox
            break;
        case 0x36:
            bResultingColor = Source ^ (Dest | Pattern); // SDPox
            break;
        case 0x37:
            bResultingColor = ~(Source & (Dest | Pattern)); // SDPoan
            break;
        case 0x38:
            bResultingColor = Pattern ^ (Source & (Dest | Pattern)); // PSDPoax
            break;
        case 0x39:
            bResultingColor = Source ^ (Pattern | ~Dest); // SPDnox
            break;
        case 0x3A:
            bResultingColor = Source ^ (Pattern | (Dest ^ Source)); // SPDSxox
            break;
        case 0x3B:
            bResultingColor = ~(Source & (Pattern | ~Dest)); // SPDnoan
            break;
        case 0x3C:
            bResultingColor = Pattern ^ Source; // PSx
            break;
        case 0x3D:
            bResultingColor = Source ^ (Pattern | ~(Dest | Source)); // SPDSonox
            break;
        case 0x3E:
            bResultingColor = Source ^ (Pattern | (Dest & ~Source)); // SPDSnaox
            break;
        case 0x3F:
            bResultingColor = ~(Pattern & Source); // PSan
            break;
        case 0x40:
            bResultingColor = Pattern & (Source & ~Dest); // PSDnaa
            break;
        case 0x41:
            bResultingColor = ~(Dest | (Pattern ^ Source)); // DPSxon
            break;
        case 0x42:
            bResultingColor = (Source ^ Dest) & (Pattern ^ Dest); // SDxPDxa
            break;
        case 0x43:
            bResultingColor = ~(Source ^ (Pattern & ~(Dest & Source))); // SPDSanaxn
            break;
        case 0x44:
            bResultingColor = (Source & ~Dest); // SDna (SRCERASE)
            break;
        case 0x45:
            bResultingColor = ~(Dest | (Pattern & ~Source)); // DPSnaon
            break;
        case 0x46:
            bResultingColor = Dest ^ (Source | (Pattern & Dest)); // DSPDaox
            break;
        case 0x47:
            bResultingColor = ~(Pattern ^ (Source & (Dest ^ Pattern))); // PSDPxaxn
            break;
        case 0x48:
            bResultingColor = Source & (Dest ^ Pattern); // SDPxa
            break;
        case 0x49:
            bResultingColor = ~(Pattern ^ (Dest ^ (Source | (Pattern & Dest)))); // PDSPDaoxxn
            break;
        case 0x4A:
            bResultingColor = Dest ^ (Pattern & (Source | Dest)); // DPSDoax
            break;
        case 0x4B:
            bResultingColor = Pattern ^ (Dest | ~Source); // PDSnox
            break;
        case 0x4C:
            bResultingColor = Source & ~(Dest & Pattern); // SDPana
            break;
        case 0x4D:
            bResultingColor = ~(Source ^ ((Source ^ Pattern) | (Dest ^ Source))); // SSPxDSxoxn
            break;
        case 0x4E:
            bResultingColor = Pattern ^ (Dest | (Source ^ Pattern)); // PDSPxox
            break;
        case 0x4F:
            bResultingColor = ~(Pattern & (Dest | ~Source)); // PDSnoan
            break;
        case 0x50:
            bResultingColor = Pattern & ~Dest; // PDna
            break;
        case 0x51:
            bResultingColor = ~(Dest | (Source & ~Pattern)); // DSPnaon
            break;
        case 0x52:
            bResultingColor = Dest ^ (Pattern | (Source & Dest)); // DPSDaox
            break;
        case 0x53:
            bResultingColor = ~(Source ^ (Pattern & (Dest ^ Source))); // SPDSxaxn
            break;
        case 0x54:
            bResultingColor = ~(Dest | ~(Pattern | Source)); // DPSonon
            break;
        case 0x55:
            bResultingColor = ~Dest; // Dn (DSTINVERT)
            break;
        case 0x56:
            bResultingColor =Dest ^ (Pattern | Source) ; // DPSox
            break;
        case 0x57:
            bResultingColor = ~(Dest & (Pattern | Source)); // DPSoan
            break;
        case 0x58:
            bResultingColor = Pattern ^ (Dest & (Source | Pattern)); // PDSPoax
            break;
        case 0x59:
            bResultingColor = Dest ^ (Pattern | ~Source); // DPSnox
            break;
        case 0x5A:
            bResultingColor = Dest ^ Pattern; // DPx (PATINVERT)
            break;
        case 0x5B:
            bResultingColor = Dest ^ (Pattern | ~(Source | Dest)); // DPSDonox
            break;
        case 0x5C:
            bResultingColor = Dest ^ (Pattern | (Source ^ Dest)); // DPSDxox
            break;
        case 0x5D:
            bResultingColor = ~(Dest & (Pattern | ~Source)); // DPSnoan
            break;
        case 0x5E:
            bResultingColor = Dest ^ (Pattern | (Source & ~Dest)); // DPSDnaox
            break;
        case 0x5F:
            bResultingColor = ~(Dest & Pattern); // DPan
            break;
        case 0x60:
            bResultingColor = Pattern & (Dest ^ Source); // PDSxa
            break;
        case 0x61:
            bResultingColor = ~(Dest ^ (Source ^ (Pattern | (Dest & Source)))); // DSPDSaoxxn
            break;
        case 0x62:
            bResultingColor = Dest ^ (Source & (Pattern | Dest)); // DSPDoax
            break;
        case 0x63:
            bResultingColor = Source ^ (Dest | ~Pattern); // SDPnox
            break;
        case 0x64:
            bResultingColor = Source ^ (Dest & (Pattern | Source)); // SDPSoax
            break;
        case 0x65:
            bResultingColor = Dest ^ (Source | ~Pattern); // DSPnox
            break;
        case 0x66:
            bResultingColor = Dest ^ Source; // DSx (SRCINVERT)
            break;
        case 0x67:
            bResultingColor = Source ^ (Dest | ~(Pattern | Source)); //SDPSonox
            break;
        case 0x68:
            bResultingColor = ~(Dest ^ (Source ^ (Pattern | ~(Dest | Source)))); // DSPDSonoxxn
            break;
        case 0x69:
            bResultingColor = ~(Pattern ^ (Dest ^ Source)); // PDSxxn
            break;
        case 0x6A:
            bResultingColor = Dest ^ (Pattern & Source); // DPSax
            break;
        case 0x6B:
            bResultingColor = ~(Pattern ^ (Source ^ (Dest & (Pattern | Source)))); // PSDPSoaxxn
            break;
        case 0x6C:
            bResultingColor = Source ^ (Dest & Pattern); // SDPax
            break;
        case 0x6D:
            bResultingColor = ~(Pattern ^ (Dest ^ (Source & (Pattern | Dest)))); // PDSPDoaxxn
            break;
        case 0x6E:
            bResultingColor = Source ^ (Dest & (Pattern | ~Source)); // SDPSnoax
            break;
        case 0x6F:
            bResultingColor = ~(Pattern & ~(Dest ^ Source)); // PDSxnan
            break;
        case 0x70:
            bResultingColor = Pattern & ~(Dest & Source); // PDSana
            break;
        case 0x71:
            bResultingColor = ~(Source ^ ((Source ^ Dest) & (Pattern ^ Dest))); // SSDxPDxaxn
            break;
        case 0x72:
            bResultingColor = Source ^ (Dest | (Pattern ^ Source)); // SDPSxox
            break;
        case 0x73:
            bResultingColor = ~(Source & (Dest | ~Pattern)); // SDPnoan
            break;
        case 0x74:
            bResultingColor = Dest ^ (Source | (Pattern ^ Dest)); // DSPDxox
            break;
        case 0x75:
            bResultingColor = ~(Dest & (Source | ~Pattern)); // DSPnoan
            break;
        case 0x76:
            bResultingColor = Source ^ (Dest | (Pattern & ~Source)); // SDPSnaox
            break;
        case 0x77:
            bResultingColor = ~(Source & Dest); // SDan
            break;
        case 0x78:
            bResultingColor = Pattern ^ (Dest & Source); // PDSax
            break;
        case 0x79:
            bResultingColor = ~(Dest ^ (Source ^ (Pattern & (Dest | Source)))); // DSPDSoaxxn
            break;
        case 0x7A:
            bResultingColor = Dest ^ (Pattern & (Source | ~Dest)); // DPSDnoax
            break;
        case 0x7B:
            bResultingColor = ~(Source & ~(Dest ^ Pattern)); // SDPxnan
            break;
        case 0x7C:
            bResultingColor = Source ^ (Pattern & (Dest | ~Source)); // SPDSnoax
            break;
        case 0x7D:
            bResultingColor = ~(Dest & ~(Pattern ^ Source)); // DPSxnan
            break;
        case 0x7E:
            bResultingColor = (Source ^ Pattern) | (Dest ^ Source); // SPxDSxo
            break;
        case 0x7F:
            bResultingColor = ~(Dest & (Pattern & Source)); // DPSaan
            break;
        case 0x80:
            bResultingColor = Dest & (Pattern & Source); // DPSaa
            break;
        case 0x81:
            bResultingColor = ~((Source ^ Pattern) | (Dest ^ Source)); // SPxDSxon
            break;
        case 0x82:
            bResultingColor = Dest & ~(Pattern ^ Source); // DPSxna
            break;
        case 0x83:
            bResultingColor = ~(Source ^ (Pattern & (Dest | ~Source))); // SPDSnoaxn
            break;
        case 0x84:
            bResultingColor = Source & ~(Dest ^ Pattern); // SDPxna
            break;
        case 0x85:
            bResultingColor = ~(Pattern ^ (Dest & (Source | ~Pattern))); // PDSPnoaxn
            break;
        case 0x86:
            bResultingColor = Dest ^ (Source ^ (Pattern & (Dest | Source))); // DSPDSoaxx
            break;
        case 0x87:
            bResultingColor = ~(Pattern ^ (Dest & Source)); // PDSaxn
            break;
        case 0x88:
            bResultingColor = Dest & Source; // DSa (SRCAND)
            break;
        case 0x89:
            bResultingColor = ~(Source ^ (Dest | (Pattern & ~Source))); // SDPSnaoxn
            break;
        case 0x8A:
            bResultingColor = Dest & (Source | ~Pattern); // DSPnoa
            break;
        case 0x8B:
            bResultingColor = ~(Dest ^ (Source | (Pattern ^ Dest))); // DSPDxoxn
            break;
        case 0x8C:
            bResultingColor = Source & (Dest | ~Pattern); // SDPnoa
            break;
        case 0x8D:
            bResultingColor = ~(Source ^ (Dest | (Pattern ^ Source))); // SDPSxoxn
            break;
        case 0x8E:
            bResultingColor = Source ^ ((Source ^ Dest) & (Pattern ^ Dest)); // SSDxPDxax
            break;
        case 0x8F:
            bResultingColor = ~(Pattern & ~(Dest & Source)); // PDSanan
            break;
        case 0x90:
            bResultingColor = Pattern & ~(Dest ^ Source); // PDSxna
            break;
        case 0x91:
            bResultingColor = ~(Source ^ (Dest & (Pattern | ~Source))); // SDPSnoaxn
            break;
        case 0x92:
            bResultingColor = Dest ^ (Pattern ^ (Source & (Dest | Pattern))); // DPSDPoaxx
            break;
        case 0x93:
            bResultingColor = ~(Source ^ (Pattern & Dest)); // SPDaxn
            break;
        case 0x94:
            bResultingColor = Pattern ^ (Source ^ (Dest & (Pattern | Source))); // PSDPSoaxx
            break;
        case 0x95:
            bResultingColor = ~(Dest ^ (Pattern & Source)); // DPSaxn
            break;
        case 0x96:
            bResultingColor = Dest ^ (Pattern ^ Source); // DPSxx
            break;
        case 0x97:
            bResultingColor = Pattern ^ (Source ^ (Dest | ~(Pattern | Source))); // PSDPSonoxx
            break;
        case 0x98:
            bResultingColor = ~(Source ^ (Dest | ~(Pattern | Source))); // SDPSonoxn
            break;
        case 0x99:
            bResultingColor = ~(Dest ^ Source); // DSxn
            break;
        case 0x9A:
            bResultingColor = Dest ^ (Pattern & ~Source); //DPSnax
            break;
        case 0x9B:
            bResultingColor = ~(Source ^ Dest & (Pattern | Source)); // SDPSoaxn
            break;
        case 0x9C:
            bResultingColor = Source ^ (Pattern & ~Dest); // SPDnax
            break;
        case 0x9D:
            bResultingColor = ~(Dest ^ (Source & (Pattern | Dest))); // DSPDoaxn
            break;
        case 0x9E:
            bResultingColor = Dest ^ (Source ^ (Pattern | (Dest & Source))); // DSPDSaoxx
            break;
        case 0x9F:
            bResultingColor = ~(Pattern & (Dest ^ Source)); // PDSxan
            break;
        case 0xA0:
            bResultingColor = Dest & Pattern; // DPa
            break;
        case 0xA1:
            bResultingColor = ~(Pattern ^ (Dest | (Source & (~Pattern)))); // PDSPnaoxn
            break;
        case 0xA2:
            bResultingColor = Dest & (Pattern | ~Source); // DPSnoa
            break;
        case 0xA3:
            bResultingColor = ~(Dest ^ (Pattern | (Source ^ Dest))); //DPSDxoxn
            break;
        case 0xA4:
            bResultingColor = ~(Pattern ^ (Dest | ~(Source | Pattern))); // PDSPonoxn
            break;
        case 0xA5:
            bResultingColor = ~(Pattern ^ Dest); // PDxn
            break;
        case 0xA6:
            bResultingColor = Dest ^ (Source & ~Pattern); // DSPnax
            break;
        case 0xA7:
            bResultingColor = ~(Pattern ^ (Dest & (Source | Pattern))); // PDSPoaxn
            break;
        case 0xA8:
            bResultingColor = Dest & (Pattern | Source); // DPSoa
            break;
        case 0xA9:
            bResultingColor = ~(Dest ^ (Pattern | Source)); // DPSoxn
            break;
        case 0xAA:
            bResultingColor = Dest; // D
            break;
        case 0xAB:
            bResultingColor = Dest | ~(Pattern | Source); // DPSono
            break;
        case 0xAC:
            bResultingColor = Source ^ (Pattern & (Dest ^ Source)); // SPDSxax
            break;
        case 0xAD:
            bResultingColor = ~(Dest ^ (Pattern | (Source & Dest))); // DPSDaoxn
            break;
        case 0xAE:
            bResultingColor = Dest | (Source & ~Pattern); // DSPnao
            break;
        case 0xAF:
            bResultingColor = Dest | ~Pattern; // DPno
            break;
        case 0xB0:
            bResultingColor = Pattern & (Dest | ~Source); // PDSnoa
            break;
        case 0xB1:
            bResultingColor = ~(Pattern ^ (Dest | (Source ^ Pattern))); // PDSPxoxn
            break;
        case 0xB2:
            bResultingColor = Source ^ ((Source ^ Pattern) | (Dest ^ Source)); // SSPxDSxox
            break;
        case 0xB3:
            bResultingColor = ~(Source & ~(Dest & Pattern)); // SDPanan
            break;
        case 0xB4:
            bResultingColor = Pattern ^ (Source & ~Dest); // PSDnax
            break;
        case 0xB5:
            bResultingColor = ~(Dest ^ (Pattern & (Source | Dest))); // DPSDoaxn
            break;
        case 0xB6:
            bResultingColor = Dest ^ (Pattern ^ (Source | (Dest & Pattern))); // DPSDPaoxx
            break;
        case 0xB7:
            bResultingColor = ~(Source & (Dest ^ Pattern)); // SDPxan
            break;
        case 0xB8:
            bResultingColor = Pattern ^ (Source & (Dest ^ Pattern)); // PSDPxax
            break;
        case 0xB9:
            bResultingColor = ~(Dest ^ (Source | (Pattern & Dest))); // DSPDaoxn
            break;
        case 0xBA:
            bResultingColor = Dest | (Pattern & ~Source); // DPSnao
            break;
        case 0xBB:
            bResultingColor = Dest | ~Source; // DSno (MERGEPAINT)
            break;
        case 0xBC:
            bResultingColor = Source ^ (Pattern & ~(Dest & Source)); // SPDSanax
            break;
        case 0xBD:
            bResultingColor = ~((Source ^ Dest) & (Pattern ^ Dest)); // SDxPDxan
            break;
        case 0xBE:
            bResultingColor = Dest | (Pattern ^ Source); // DPSxo
            break;
        case 0xBF:
            bResultingColor = Dest | ~(Pattern & Source); // DPSano
            break;
        case 0xC0:
            bResultingColor = Pattern & Source; // PSa (MERGECOPY)
            break;
        case 0xC1:
            bResultingColor = ~(Source ^ (Pattern | (Dest & ~Source))); // SPDSnaoxn
            break;
        case 0xC2:
            bResultingColor = ~(Source ^ (Pattern | ~(Dest | Source))); // SPDSonoxn
            break;
        case 0xC3:
            bResultingColor = ~(Pattern ^ Source); // PSxn
            break;
        case 0xC4:
            bResultingColor = Source & (Pattern | ~Dest); // SPDnoa
            break;
        case 0xC5:
            bResultingColor = ~(Source ^ (Pattern | (Dest ^ Source))); // SPDSxoxn
            break;
        case 0xC6:
            bResultingColor = Source ^ (Dest & ~Pattern); // SDPnax
            break;
        case 0xC7:
            bResultingColor = ~(Pattern ^ (Source & (Dest | Pattern))); // PSDPoaxn
            break;
        case 0xC8:
            bResultingColor = Source & (Dest | Pattern); // SDPoa
            break;
        case 0xC9:
            bResultingColor = ~(Source ^ (Pattern | Dest)); // SPDoxn
            break;
        case 0xCA:
            bResultingColor = Dest ^ (Pattern & (Source ^ Dest)); // DPSDxax
            break;
        case 0xCB:
            bResultingColor = ~(Source ^ (Pattern | (Dest & Source))); // SPDSaoxn
            break;
        case 0xCC:
            bResultingColor = Source; //  S (SRCCOPY)
            break;
        case 0xCD:
            bResultingColor = Source | ~(Dest | Pattern); // SDPono
            break;
        case 0xCE:
            bResultingColor = Source | (Dest & ~Pattern); // SDPnao
            break;
        case 0xCF:
            bResultingColor = Source | ~Pattern; // SPno
            break;
        case 0xD0:
            bResultingColor = Pattern & (Source | ~Dest); // PSDnoa
            break;
        case 0xD1:
            bResultingColor = ~(Pattern ^ (Source | (Dest ^ Pattern))); // PSDPxoxn
            break;
        case 0xD2:
            bResultingColor = Pattern ^ (Dest & ~Source); // PDSnax
            break;
        case 0xD3:
            bResultingColor = ~(Source ^ (Pattern & (Dest | Source))); // SPDSoaxn
            break;
        case 0xD4:
            bResultingColor = Source ^ ((Source ^ Pattern) & (Pattern ^ Dest)); // SSPxPDxax
            break;
        case 0xD5:
            bResultingColor = ~(Dest & ~(Pattern & Source)); // DPSanan
            break;
        case 0xD6:
            bResultingColor = Pattern ^ (Source ^ (Dest | (Pattern & Source))); // PSDPSaoxx
            break;
        case 0xD7:
            bResultingColor = ~(Dest & (Pattern ^ Source)); // DPSxan
            break;
        case 0xD8:
            bResultingColor = Pattern ^ (Dest & (Source ^ Pattern)); // PDSPxax
            break;
        case 0xD9:
            bResultingColor = ~(Source ^ (Dest | (Pattern & Source))); // SDPSaoxn
            break;
        case 0xDA:
            bResultingColor = Dest ^ (Pattern & ~(Source & Dest)); // DPSDanax
            break;
        case 0xDB:
            bResultingColor = ~((Source ^ Pattern) & (Dest ^ Source)); // SPxDSxan
            break;
        case 0xDC:
            bResultingColor = Source | (Pattern & ~Dest); // SPDnao
            break;
        case 0xDD:
            bResultingColor = Source | ~Dest; // SDno
            break;
        case 0xDE:
            bResultingColor = Source | (Dest ^ Pattern); // SDPxo
            break;
        case 0xDF:
            bResultingColor = Source | ~(Dest & Pattern); // SDPano
            break;
        case 0xE0:
            bResultingColor = Pattern & (Dest | Source); // PDSoa
            break;
        case 0xE1:
            bResultingColor = ~(Pattern ^ (Dest | Source)); // PDSoxn
            break;
        case 0xE2:
            bResultingColor = Dest ^ (Source & (Pattern ^ Dest)); // DSPDxax
            break;
        case 0xE3:
            bResultingColor = ~(Pattern ^ (Source | (Dest & Pattern))); // PSDPaoxn
            break;
        case 0xE4:
            bResultingColor = Source ^ (Dest & (Pattern ^ Source)); // SDPSxax
            break;
        case 0xE5:
            bResultingColor = ~(Pattern ^ (Dest | (Source & Pattern))); // PDSPaoxn
            break;
        case 0xE6:
            bResultingColor = Source ^ (Dest & ~(Pattern & Source)); // SDPSanax
            break;
        case 0xE7:
            bResultingColor = ~((Source ^ Pattern) & (Pattern ^ Dest)); // SPxPDxan
            break;
        case 0xE8:
            bResultingColor = Source ^ ((Source ^ Pattern) & (Dest ^ Source)); // SSPxDSxax
            break;
        case 0xE9:
            bResultingColor = ~(Dest ^ (Source ^ (Pattern & ~(Dest & Source)))); // DSPDSanaxxn
            break;
        case 0xEA:
            bResultingColor = Dest | (Pattern & Source); // DPSao
            break;
        case 0xEB:
            bResultingColor = Dest | ~(Pattern ^ Source); // DPSxno
            break;
        case 0xEC:
            bResultingColor = Source | (Dest & Pattern); // SDPao
            break;
        case 0xED:
            bResultingColor = Source | ~(Dest ^ Pattern); // SDPxno
            break;
        case 0xEE:
            bResultingColor = Dest | Source; // DSo (SRCPAINT)
            break;
        case 0xEF:
            bResultingColor = Source | (Dest | ~Pattern); // SDPnoo
            break;
        case 0xF0:
            bResultingColor = Pattern; // P (PATCOPY)
            break;
        case 0xF1:
            bResultingColor = Pattern | ~(Dest | Source); // PDSono
            break;
        case 0xF2:
            bResultingColor = Pattern | (Dest & ~Source); // PDSnao
            break;
        case 0xF3:
            bResultingColor = Pattern | ~Source; // PSno
            break;
        case 0xF4:
            bResultingColor = Pattern | (Source & ~Dest); // PSDnao
            break;
        case 0xF5:
            bResultingColor = Pattern | ~Dest; // PDno
            break;
        case 0xF6:
            bResultingColor = Pattern | (Dest ^ Source); // PDSxo
            break;
        case 0xF7:
            bResultingColor = Pattern | ~(Dest & Source); // PDSano
            break;
        case 0xF8:
            bResultingColor = Pattern | (Dest & Source); // PDSao
            break;
        case 0xF9:
            bResultingColor = Pattern | ~(Dest ^ Source); // PDSxno
            break;
        case 0xFA:
            bResultingColor = Dest | Pattern; // DPo
            break;
        case 0xFB:
            bResultingColor = Dest | (Pattern | ~Source); // DPSnoo (PATPAINT)
            break;
        case 0xFC:
            bResultingColor = Pattern | Source; // PSo
            break;
        case 0xFD:
            bResultingColor = Pattern | (Source | ~Dest); // PSDnoo
            break;
        case 0xFE:
            bResultingColor = Dest | (Pattern | Source); // DPSoo
            break;
        case 0xFF:
            bResultingColor = 0xFF; // 1 (WHITENESS)
            break;
    }
    return bResultingColor;
}

