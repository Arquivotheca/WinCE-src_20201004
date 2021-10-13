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

   clip.cpp

Abstract:

   Gdi Tests: Clipping APIs

***************************************************************************/

#include "global.h"
#include "rgnData.h"

/***********************************************************************************
***
***   Passes Null as every possible parameter
***
************************************************************************************/

//***********************************************************************************
void
passNull2ClipRegion(int testFunc)
{

    info(ECHO, TEXT("%s - Passing NULL"), funcName[testFunc]);

    RECT    test = { 10, 10, 50, 50 };
    RECT    rTemp = { 0, 0, 20, 20 };
    HRGN    tempRgn, selectedRgn, hRgnBad;
    TDC     hdc = myGetDC();

    CheckNotRESULT(NULL, tempRgn = CreateRectRgnIndirect(&rTemp));
    CheckNotRESULT(NULL, selectedRgn = CreateRectRgnIndirect(&rTemp));
    CheckNotRESULT(ERROR, SelectClipRgn(hdc, selectedRgn));

    // create an invalid region
    CheckNotRESULT(NULL, hRgnBad = CreateRectRgn(0, 0, 20, 20));
    CheckNotRESULT(FALSE, DeleteObject(hRgnBad));

    switch (testFunc)
    {
        case EExcludeClipRect:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, ExcludeClipRect((TDC) NULL, test.left, test.top, test.right, test.bottom));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, ExcludeClipRect(g_hdcBAD, test.left, test.top, test.right, test.bottom));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, ExcludeClipRect(g_hdcBAD2, test.left, test.top, test.right, test.bottom));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EGetClipBox:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, GetClipBox((TDC) NULL, &test));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, GetClipBox(g_hdcBAD, &test));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, GetClipBox(g_hdcBAD2, &test));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(FALSE, GetClipBox(hdc, NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);
            break;
        case EGetClipRgn:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(-1, GetClipRgn(hdc, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(-1, GetClipRgn(hdc, hRgnBad));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(-1, GetClipRgn(hdc, (HRGN) g_hpenStock));
            CheckForLastError(ERROR_INVALID_HANDLE);

            CheckForRESULT(SIMPLEREGION, SelectClipRgn(hdc, NULL));

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(-1, GetClipRgn((TDC) NULL, tempRgn));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(-1, GetClipRgn(g_hdcBAD, tempRgn));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(-1, GetClipRgn(g_hdcBAD2, tempRgn));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(-1, GetClipRgn((TDC) NULL, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EIntersectClipRect:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, IntersectClipRect((TDC) NULL, test.left, test.top, test.right, test.bottom));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, IntersectClipRect(g_hdcBAD, test.left, test.top, test.right, test.bottom));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, IntersectClipRect(g_hdcBAD2, test.left, test.top, test.right, test.bottom));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case ESelectClipRgn:
            CheckForRESULT(SIMPLEREGION, SelectClipRgn(hdc, NULL));

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, SelectClipRgn(hdc, hRgnBad));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, SelectClipRgn(hdc, (HRGN) g_hbrushStock));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, SelectClipRgn((TDC) NULL, tempRgn));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, SelectClipRgn(g_hdcBAD, tempRgn));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, SelectClipRgn(g_hdcBAD2, tempRgn));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, SelectClipRgn((TDC) NULL, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
    }

    CheckNotRESULT(ERROR, SelectClipRgn(hdc, NULL));
    CheckNotRESULT(FALSE, DeleteObject(selectedRgn));
    CheckNotRESULT(FALSE, DeleteObject(tempRgn));
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Check return values
***
************************************************************************************/

//***********************************************************************************
void
returnValues0(int testFunc)
{

    info(ECHO, TEXT("%s - Checking return values for diff rgns"), funcName[testFunc]);
    setTestRectRgn();
    // if the window bottom/right match the region created then result0 != (result 0 & result 1)
    // same behavior on desktop.
    if(SetWindowConstraint(center.right + 1, center.bottom + 1))
    {
        TDC     hdc = myGetDC();
        int     result0,
                result1,
                result2;
        HRGN    hRgn1;
        HRGN    destRgn;
        HRGN    hRgn0;
        RECT    aRect;

        CheckNotRESULT(NULL, hRgn1 = CreateRectRgnIndirect(&center));
        CheckNotRESULT(NULL, destRgn = CreateRectRgnIndirect(&center));

        // try every mode with every test
        for (int mode = 0; mode < uTests; mode++)
            for (int test = 0; test < NUMREGIONTESTS; test++)
            {
                if(rTests[test].left < zx && rTests[test].top < zy)
                {
                    CheckNotRESULT(NULL, hRgn0 = CreateRectRgnIndirect(&rTests[test]));
                    CheckNotRESULT(ERROR, result0 = CombineRgn(destRgn, hRgn0, hRgn1, unionMode[mode]));
                    CheckNotRESULT(ERROR, result1 = SelectClipRgn(hdc, destRgn));
                    CheckNotRESULT(ERROR, result2 = GetClipBox(hdc, &aRect));

                    if (!(result0 == result1 && result1 == result2))
                        info(FAIL, TEXT("Same RGN diff returns CombineRgn:%d SelectClipRgn%d GetClipBox:%d"), result0, result1, result2,
                            mode, test);

                    CheckNotRESULT(FALSE, DeleteObject(hRgn0));
                }
            }
        CheckNotRESULT(FALSE, DeleteObject(hRgn1));
        CheckNotRESULT(FALSE, DeleteObject(destRgn));
        myReleaseDC(hdc);
        SetWindowConstraint(0,0);
    }
    else info(DETAIL, TEXT("Screen too small to test"));
}

/***********************************************************************************
***
***   Check return values: Exclude \ Intsersect
***
************************************************************************************/

//***********************************************************************************
void
ClipRectReturns(int testFunc)
{

    info(ECHO, TEXT("%s - Clip Rgn Tests Checking against NT Hardcoded"), funcName[testFunc]);

    setTestRectRgn();

    if(SetWindowConstraint(center.right + 1, center.bottom + 1))
    {
        TDC     hdc = myGetDC();
        int     result1 = 0,
                result3 = 0,
                i,
                curIndex,
                numEnt;
        RECT   *temp,
               *expected;

        HRGN    hRgn1;
        HRGN    destRgn;
        DWORD   dwCount;
        LPRGNDATA lpRgnData;

        CheckNotRESULT(NULL, hRgn1 = CreateRectRgnIndirect(&center));
        CheckNotRESULT(NULL, destRgn = CreateRectRgnIndirect(&center));

        // try every test
        for (int test = 0; test < NUMREGIONTESTS; test++)
        {

            CheckNotRESULT(ERROR, SelectClipRgn(hdc, hRgn1));

            switch (testFunc)
            {
                case EIntersectClipRect:
                    CheckNotRESULT(ERROR, IntersectClipRect(hdc, rTests[test].left, rTests[test].top, rTests[test].right, rTests[test].bottom));
                    break;

                case EExcludeClipRect:
                    CheckNotRESULT(ERROR, ExcludeClipRect(hdc, rTests[test].left, rTests[test].top, rTests[test].right, rTests[test].bottom));
                    break;
            }

            CheckNotRESULT(-1, GetClipRgn(hdc, destRgn));

            CheckNotRESULT(0, dwCount = GetRegionData(destRgn, 0, NULL));

            CheckNotRESULT(NULL, lpRgnData = (LPRGNDATA) LocalAlloc(LPTR, dwCount));

            CheckNotRESULT(0, result1 = (int) GetRegionData(destRgn, dwCount, lpRgnData));
            curIndex = test;

            // Verify CombineRgn
            numEnt = (testFunc == EIntersectClipRect) ? NTIntersectClip[curIndex][0].left : NTExcludeClip[curIndex][0].left;
            if (numEnt == (signed) lpRgnData->rdh.nCount)
            {
                result3 = 0;
                for (i = 1; i <= numEnt; i++)
                {
                    expected = (testFunc == EIntersectClipRect) ? &NTIntersectClip[curIndex][i] : &NTExcludeClip[curIndex][i];
                    temp = (RECT *) (lpRgnData->Buffer + sizeof (RECT) * (i - 1));
                    result3 |= !isEqualRect(temp, expected);
                }
                if (result3)
                {
                    for (i = 1; i <= numEnt; i++)
                    {
                        expected = (testFunc == EIntersectClipRect) ? &NTIntersectClip[curIndex][i] : &NTExcludeClip[curIndex][i];
                        temp = (RECT *) (lpRgnData->Buffer + sizeof (RECT) * (i - 1));
                        info(FAIL, TEXT("%s: test:%d OS:(%d, %d, %d, %d) NT HardCode:(%d, %d, %d, %d)"), funcName[testFunc], test,
                             (*temp).left, (*temp).top, (*temp).right, (*temp).bottom, (*expected).left, (*expected).top,
                             (*expected).right, (*expected).bottom);
                    }
                }
            }
            else
                info(FAIL, TEXT("%s:(%d, %d, %d, %d) w/ center test:%d lpRgnData->rdh.nCount=> OS:%d NT:%d"), funcName[testFunc],
                     rTests[test].left, rTests[test].top, rTests[test].right, rTests[test].bottom, test, lpRgnData->rdh.nCount,
                     numEnt);

            if (lpRgnData->rdh.iType != RDH_RECTANGLES)
                info(FAIL, TEXT("GetRegionData: lpRgnData->rdh.iType not RDH_RECTANGLES"));

            // NULL indicates successful free.
            CheckForRESULT(NULL, LocalFree(lpRgnData));
        }
        CheckNotRESULT(FALSE, DeleteObject(hRgn1));
        CheckNotRESULT(FALSE, DeleteObject(destRgn));
        myReleaseDC(hdc);
    }
    else info(DETAIL, TEXT("Screen too small to test"));
}

/***********************************************************************************
***
***   getClipRgnTest
***
************************************************************************************/

//***********************************************************************************
void
getClipRgnTest(int testFunc)
{

    info(ECHO, TEXT("%s - Checking return values for diff rgns"), funcName[testFunc]);
    setTestRectRgn();

    if(SetWindowConstraint(center.right + 1, center.bottom + 1))
    {
        TDC   hdc = myGetDC();
        int     result1,
                result2;
        HRGN hRgn1,
                hRgn0,
                rRgn;

        CheckNotRESULT(NULL, hRgn1 = CreateRectRgnIndirect(&center));
        CheckNotRESULT(NULL, rRgn = CreateRectRgnIndirect(&center));

        // try every mode with every test
        for (int test = 0; test < NUMREGIONTESTS; test++)
        {
            CheckNotRESULT(ERROR, SelectClipRgn(hdc, NULL));
            CheckNotRESULT(NULL, hRgn0 = CreateRectRgnIndirect(&rTests[test]));

            CheckNotRESULT(ERROR, result1 = SelectClipRgn(hdc, hRgn0));
            CheckNotRESULT(ERROR, result2 = GetClipRgn(hdc, rRgn));

            if (!EqualRgn(rRgn, hRgn0))
                info(FAIL, TEXT("Equal Rgn !Equal test:%d"), test);

            CheckNotRESULT(FALSE, DeleteObject(hRgn0));
        }
        CheckNotRESULT(FALSE, DeleteObject(hRgn1));
        CheckNotRESULT(FALSE, DeleteObject(rRgn));
        myReleaseDC(hdc);
    }
    else info(DETAIL, TEXT("Screen too small to test"));
}

/***********************************************************************************
***
***   Select Test
***
************************************************************************************/

//***********************************************************************************
void
SelectTest(int testFunc)
{
    info(ECHO, TEXT("%s - Select Test"), funcName[testFunc]);

    TDC    hdc = myGetDC();
    int    edge = RESIZE(80);
    HRGN   hRgnC,
           hRgnS;
    HBRUSH hBrushR,
           hBrushB;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hRgnC = CreateRectRgn(edge, edge, zx / 2 - edge, zy - edge));
        CheckNotRESULT(NULL, hRgnS = CreateRectRgn(0, 0, zx / 2, zy));

        CheckNotRESULT(NULL, hBrushR = (HBRUSH) GetStockObject(LTGRAY_BRUSH));
        CheckNotRESULT(NULL, hBrushB = (HBRUSH) GetStockObject(BLACK_BRUSH));

        CheckNotRESULT(FALSE, FillRgn(hdc, hRgnS, hBrushR));
        CheckNotRESULT(FALSE, FillRgn(hdc, hRgnC, hBrushB));
        CheckNotRESULT(FALSE, BitBlt(hdc, zx / 2, 0, zx / 2, zy, hdc, 0, 0, SRCCOPY));

        CheckNotRESULT(NULL, SelectClipRgn(hdc, hRgnC));
        for (int i = 0; i < 5; i++)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
        }

        CheckScreenHalves(hdc);

        CheckNotRESULT(NULL, SelectClipRgn(hdc, NULL));

        CheckNotRESULT(FALSE, DeleteObject(hRgnS));
        CheckNotRESULT(FALSE, DeleteObject(hRgnC));
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Select Complex
***
************************************************************************************/

//***********************************************************************************
void
SelectComplexTest(int testFunc, int SCube)
{

    info(ECHO, TEXT("%s - Select Complex Test"), funcName[testFunc]);
    // verification cannot handle complex clip regions.
    BOOL OldVerify = SetSurfVerify(FALSE);

    TDC     hdc = myGetDC();

    // The desktop dithers for 1bpp to 16bpp color conversions, which makes this test invalid
#ifndef UNDER_CE
    if (IsWritableHDC(hdc) && myGetBitDepth() > 1)
#else
    if (IsWritableHDC(hdc))
#endif
    {
        HRGN    hRgnTemp = NULL,
                   hRgnS;
        POINT   SBoard = { zx / 2 / SCube, zy / SCube };

        CheckNotRESULT(NULL, hRgnS = CreateRectRgn(0, 0, SCube, SCube));

        CheckNotRESULT(NULL, SelectClipRgn(hdc, NULL));

        CheckNotRESULT(NULL, SelectObject(hdc, (HBRUSH) GetStockObject(BLACK_BRUSH)));

        for (int x = 0; x < SBoard.x; x++)
            for (int y = 0; y < SBoard.y; y++)
            {
                if ((x + y) % 2 == 0)
                {
                    CheckNotRESULT(NULL, hRgnTemp = CreateRectRgn(x * SCube, y * SCube, x * SCube + SCube, y * SCube + SCube));
                    CheckNotRESULT(ERROR, CombineRgn(hRgnS, hRgnS, hRgnTemp, RGN_OR));
                    CheckNotRESULT(FALSE, DeleteObject(hRgnTemp));
                    CheckNotRESULT(FALSE, PatBlt(hdc, x * SCube + zx / 2, y * SCube, SCube, SCube, PATCOPY));
                }
            }

        CheckNotRESULT(NULL, SelectClipRgn(hdc, hRgnS));
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx / 2, zy, PATCOPY));

        CheckScreenHalves(hdc);

        // clean up
        CheckNotRESULT(NULL, SelectClipRgn(hdc, NULL));
        CheckNotRESULT(FALSE, DeleteObject(hRgnS));
    }

    myReleaseDC(hdc);

    SetSurfVerify(OldVerify);
}

/***********************************************************************************
***
***   Exclude Complex
***
************************************************************************************/

//***********************************************************************************
void
ExcludeComplexTest(int testFunc, int SCube)
{

    info(ECHO, TEXT("%s - Exclude Complex Test"), funcName[testFunc]);

    // test uses a complex region to build up an image to verify w/ CheckScreenHalves, can't do driver verification
    BOOL bOldVerify = SetSurfVerify(FALSE);
    TDC     hdc = myGetDC();

    // The desktop dithers for 1bpp to 16bpp color conversions, which makes this test invalid.
#ifndef UNDER_CE
    if (IsWritableHDC(hdc) && myGetBitDepth() > 1)
#else
    if (IsWritableHDC(hdc))
#endif
    {
        HRGN    hRgnS;
        POINT   SBoard = { zx / 2 / SCube, zy / SCube };
        int     x,
                y;

        CheckNotRESULT(NULL, hRgnS = CreateRectRgn(0, 0, zx / 2, zy));
        CheckNotRESULT(ERROR, SelectClipRgn(hdc, NULL));
        CheckNotRESULT(NULL, SelectObject(hdc, (HBRUSH) GetStockObject(BLACK_BRUSH)));

        for (x = 0; x < SBoard.x; x++)
            for (y = 0; y < SBoard.y; y++)
            {
                if ((x + y) % 2 == 0)
                    CheckNotRESULT(FALSE, PatBlt(hdc, x * SCube + zx / 2, y * SCube, SCube, SCube, PATCOPY));
            }

        CheckNotRESULT(NULL, SelectClipRgn(hdc, hRgnS));
        for (x = 0; x < SBoard.x; x++)
            for (y = 0; y < SBoard.y; y++)
            {
                if ((x + y) % 2 == 1)
                    CheckNotRESULT(ERROR, ExcludeClipRect(hdc, x * SCube, y * SCube, x * SCube + SCube, y * SCube + SCube));
            }

        //for areas not covered by cubes
        CheckNotRESULT(ERROR, ExcludeClipRect(hdc, 0, SBoard.y * SCube, zx, zy));
        CheckNotRESULT(ERROR, ExcludeClipRect(hdc, SBoard.x * SCube, 0, zx, zy));

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx / 2, zy, PATCOPY));

        CheckScreenHalves(hdc);

        // clean up
        CheckNotRESULT(FALSE, DeleteObject(hRgnS));
    }

    myReleaseDC(hdc);
    SetSurfVerify(bOldVerify);
}

void
TestGetClipBox1(int testFunc)
{

    info(ECHO, TEXT("%s - IntersectClipRect: rc:left=right"), funcName[testFunc]);
    TDC     hdc = myGetDC();
    int     result;
    RECT    rc = { 0, 0, 0, 0 };

    CheckForRESULT(NULLREGION, result = IntersectClipRect(hdc, 50, 50, 50, 100));   // left = right
    CheckForRESULT(NULLREGION, result = GetClipBox(hdc, &rc));

    if ((rc.left | rc.top | rc.right | rc.bottom) != 0)
        info(FAIL, TEXT("GetClipBox expected NULL Rect result:%d %d %d %d"), rc.left, rc.top, rc.right, rc.bottom);

    myReleaseDC(hdc);
}

void
TestNoClipRgn(int testFunc)
{

    info(ECHO, TEXT("%s - TestNoClipRgn"), funcName[testFunc]);
    TDC     hdc = myGetDC(),
            aDC;
    int     result;
    HRGN    hRgn;

    CheckNotRESULT(NULL, aDC = CreateCompatibleDC(hdc));
    CheckNotRESULT(NULL, hRgn = CreateRectRgn(0, 0, 10, 10));
    CheckForRESULT(0, result = GetClipRgn(aDC, hRgn));
    CheckForRESULT(0, result = GetClipRgn(aDC, NULL));
    CheckNotRESULT(FALSE, DeleteDC(aDC));
    CheckNotRESULT(FALSE, DeleteObject(hRgn));
    myReleaseDC(hdc);
}

void
TestNormalClipRgn(int testFunc)
{

    info(ECHO, TEXT("%s - TestNormalClipRgn"), funcName[testFunc]);
    TDC     hdc = myGetDC(),
            aDC;
    int     result;
    HRGN    hRgn;
    TBITMAP hbmp,
                hbmpStock = NULL;

    // bitmap and normal region associated with the dc, region fills the bitmap.
    CheckNotRESULT(NULL, aDC = CreateCompatibleDC(hdc));
    CheckNotRESULT(NULL, hRgn = CreateRectRgn(0, 0, 10, 10));
    CheckNotRESULT(NULL, hbmp = CreateCompatibleBitmap(hdc, 10, 10));

    CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(aDC, hbmp));
    CheckForRESULT(SIMPLEREGION, SelectClipRgn(aDC, hRgn));
    CheckForRESULT(1, result = GetClipRgn(aDC, hRgn));

    // bitmap and normal region, region doesn't fill the bitmap.
    CheckNotRESULT(0, SetRectRgn(hRgn, 1, 1, 9, 9));
    CheckForRESULT(SIMPLEREGION, SelectClipRgn(aDC, hRgn));
    CheckForRESULT(1, result = GetClipRgn(aDC, hRgn));

    CheckNotRESULT(NULL, SelectObject(aDC, hbmpStock));
    CheckNotRESULT(FALSE, DeleteDC(aDC));
    CheckNotRESULT(FALSE, DeleteObject(hRgn));
    CheckNotRESULT(FALSE, DeleteObject(hbmp));
    myReleaseDC(hdc);
}

void
TestNullClipRgn(int testFunc)
{

    info(ECHO, TEXT("%s - TestNullClipRgn"), funcName[testFunc]);
    TDC     hdc = myGetDC(),
            aDC;
    int     result;
    HRGN    hRgn;
    TBITMAP hbmp,
                hbmpStock = NULL;

    // NULLREGION associated to the DC
    CheckNotRESULT(NULL, aDC = CreateCompatibleDC(hdc));
    CheckNotRESULT(NULL, hbmp = CreateCompatibleBitmap(hdc, 10, 10));
    CheckNotRESULT(NULL, hRgn = CreateRectRgn(0, 0, 0, 0));

    CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(aDC, hbmp));
    CheckForRESULT(NULLREGION, SelectClipRgn(aDC, hRgn));

    CheckForRESULT(1, result = GetClipRgn(aDC, hRgn));

    CheckNotRESULT(NULL, SelectObject(aDC, hbmpStock));
    CheckNotRESULT(FALSE, DeleteDC(aDC));
    CheckNotRESULT(FALSE, DeleteObject(hRgn));
    CheckNotRESULT(FALSE, DeleteObject(hbmp));
    myReleaseDC(hdc);
}

//***********************************************************************************
LRESULT CALLBACK
GenericWnd455(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return (DefWindowProc(hWnd, message, wParam, lParam));
}

//***********************************************************************************
void
TestGetClipBox2(int testFunc)
{
    info(ECHO, TEXT("%s - TestGetClipBox2"), funcName[testFunc]);
    TDC     hdc = myGetDC();
    RECT    rc;
    int nRgnRight = (GenRand() % (zx - 1)) +1;
    int nRgnBottom = (GenRand() % (zy - 1)) +1;
    HRGN    hRgn;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hRgn = CreateRectRgn(0, 0, nRgnRight, nRgnBottom));

        // paint the inside of the window black
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
        CheckNotRESULT(ERROR, SelectClipRgn(hdc, hRgn));

        // paint the section that's left after the clip region white
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
        CheckNotRESULT(ERROR, GetClipBox(hdc, &rc));

        if (rc.left != 0 || rc.top != 0 || rc.right != nRgnRight || rc.bottom != nRgnBottom)
            info(FAIL, TEXT("GetClipBox expected: 0,0,%d,%d returned:%d,%d,%d,%d"), nRgnRight, nRgnBottom, rc.left, rc.top, rc.right, rc.bottom);

        CheckNotRESULT(FALSE, DeleteObject(hRgn));
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   APIs
***
************************************************************************************/

//***********************************************************************************
TESTPROCAPI ExcludeClipRect_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2ClipRegion(EExcludeClipRect);
    passEmptyRect(EExcludeClipRect, funcName[EExcludeClipRect]);
    ExcludeComplexTest(EExcludeClipRect, 10);

    // depth
    ClipRectReturns(EExcludeClipRect);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetClipBox_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2ClipRegion(EGetClipBox);
    returnValues0(EGetClipBox);
    TestGetClipBox1(EGetClipBox);
    TestGetClipBox2(EGetClipBox);

    // depth
    // NONE

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetClipRgn_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2ClipRegion(EGetClipRgn);
    TestNoClipRgn(EGetClipRgn);
    TestNullClipRgn(EGetClipRgn);
    TestNormalClipRgn(EGetClipRgn);

    // depth
    getClipRgnTest(EGetClipRgn);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI IntersectClipRect_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2ClipRegion(EIntersectClipRect);
    passEmptyRect(EIntersectClipRect, funcName[EIntersectClipRect]);

    // depth
    ClipRectReturns(EIntersectClipRect);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI SelectClipRgn_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // breadth
    passNull2ClipRegion(ESelectClipRgn);
    returnValues0(ESelectClipRgn);
    SelectTest(ESelectClipRgn);
    SelectComplexTest(ESelectClipRgn, 10);

    // depth
    // NONE

    return getCode();
}
