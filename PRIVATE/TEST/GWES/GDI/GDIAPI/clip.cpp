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
    int     result = 0;
    RECT    rTemp = { 0, 0, 20, 20 };
    HRGN    tempRgn = CreateRectRgnIndirect(&rTemp);
    TDC     hdc = myGetDC();

    mySetLastError();
    switch (testFunc)
    {
        case EExcludeClipRect:
            result = ExcludeClipRect((HDC) NULL, test.left, test.top, test.right, test.bottom);
            passCheck(result, 0, TEXT("(hdc=NULL,x,x,x,x"));
            result = ExcludeClipRect((HDC) 0xBAD, test.left, test.top, test.right, test.bottom);
            passCheck(result, 0, TEXT("(hdc=0xBAD,x,x,x,x"));
            break;
        case EGetClipBox:
            result = GetClipBox((HDC) NULL, &test);
            passCheck(result, 0, TEXT("NULL,x,x,x,x"));
            result = GetClipBox(hdc, NULL);
            passCheck(result, 0, TEXT("hdc,lprect=NULL"));
            break;
        case EGetClipRgn:
#ifdef UNDER_CE
            result = GetClipRgn(hdc, NULL);
            // For a printer DC on CE after start page/start doc clip region becomes non-null.
            // So we expect an error return val if this is a printer dc, 0 if it isn't,
            // Clip region is non-null on XP, so this test isn't valid.
            passCheck(result, g_fPrinterDC?-1:0, TEXT("hdc,NULL"));
#endif
            SelectClipRgn(hdc, tempRgn);
            result = GetClipRgn(hdc, NULL);
            // the HDC has a non-null clip region, we expect this call to fail
            passCheck(result, -1, TEXT("hdc,NULL"));
            result = GetClipRgn(hdc, (HRGN) 0xBAD);
            passCheck(result, -1, TEXT("hdc,0xBAD"));
            SelectClipRgn(hdc, NULL);
            result = GetClipRgn((TDC) NULL, tempRgn);
            passCheck(result, -1, TEXT("NULL,tempRgn"));
            result = GetClipRgn((HDC) 0xBAD, tempRgn);
            passCheck(result, -1, TEXT("0xBAD,tempRgn"));
            result = GetClipRgn((TDC) NULL, NULL);
            passCheck(result, -1, TEXT("NULL,NULL"));
            break;
        case EIntersectClipRect:
            result = IntersectClipRect((TDC) NULL, test.left, test.top, test.right, test.bottom);
            passCheck(result, 0, TEXT("NULL,x,x,x,x"));
            result = IntersectClipRect((HDC) 0xBAD, test.left, test.top, test.right, test.bottom);
            passCheck(result, 0, TEXT("0xBAD,x,x,x,x"));
            break;

        case ESelectClipRgn:
            result = SelectClipRgn(hdc, NULL);
            passCheck(result, SIMPLEREGION, TEXT("hdc,NULL"));
            result = SelectClipRgn(hdc, (HRGN) 0xBAD);
            passCheck(result, 0, TEXT("hdc,0xBAD"));
            result = SelectClipRgn((TDC) NULL, tempRgn);
            passCheck(result, 0, TEXT("NULL,tempRgn"));
            result = SelectClipRgn((HDC) 0xBAD, tempRgn);
            passCheck(result, 0, TEXT("0xBAD,tempRgn"));
            result = SelectClipRgn((TDC) NULL, NULL);
            passCheck(result, 0, TEXT("NULL,NULL"));
            break;
    }
    myGetLastError(ERROR_INVALID_HANDLE);

    DeleteObject(tempRgn);
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
        HRGN    hRgn1 = CreateRectRgnIndirect(&center);
        HRGN    destRgn = CreateRectRgnIndirect(&center);
        RECT    aRect;

        // try every mode with every test
        for (int mode = 0; mode < uTests; mode++)
            for (int test = 0; test < NUMREGIONTESTS; test++)
            {
                if(rTests[test].left < zx && rTests[test].top < zy)
                {
                    HRGN    hRgn0 = CreateRectRgnIndirect(&rTests[test]);

                    result0 = CombineRgn(destRgn, hRgn0, hRgn1, unionMode[mode]);

                    mySetLastError();
                    result1 = SelectClipRgn(hdc, destRgn);
                    myGetLastError(NADA);

                    mySetLastError();
                    result2 = GetClipBox(hdc, &aRect);
                    myGetLastError(NADA);

                    if (!(result0 == result1 && result1 == result2))
                        info(FAIL, TEXT("Same RGN diff returns CombineRgn:%d SelectClipRgn%d GetClipBox:%d"), result0, result1, result2,
                            mode, test);

                    DeleteObject(hRgn0);
                }
            }
        DeleteObject(hRgn1);
        DeleteObject(destRgn);
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

    TDC     hdc = myGetDC();
    int     result1 = 0,
            result3 = 0,
            i,
            curIndex,
            numEnt;
    RECT   *temp,
           *expected;

    HRGN    hRgn1 = CreateRectRgnIndirect(&center);
    HRGN    destRgn = CreateRectRgnIndirect(&center);

    // try every test
    for (int test = 0; test < NUMREGIONTESTS; test++)
    {

        SelectClipRgn(hdc, hRgn1);

        mySetLastError();
        switch (testFunc)
        {
            case EIntersectClipRect:
                IntersectClipRect(hdc, rTests[test].left, rTests[test].top, rTests[test].right, rTests[test].bottom);
                break;

            case EExcludeClipRect:
                ExcludeClipRect(hdc, rTests[test].left, rTests[test].top, rTests[test].right, rTests[test].bottom);
                break;
        }
        myGetLastError(NADA);

        GetClipRgn(hdc, destRgn);

        DWORD   dwCount = GetRegionData(destRgn, 0, NULL);
        LPRGNDATA lpRgnData = (LPRGNDATA) LocalAlloc(LPTR, dwCount);

        result1 = (int) GetRegionData(destRgn, dwCount, lpRgnData);
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

        LocalFree(lpRgnData);
    }
    DeleteObject(hRgn1);
    DeleteObject(destRgn);
    myReleaseDC(hdc);

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

    TDC     hdc = myGetDC();
    int     result1,
            result2;
    HRGN    hRgn1 = CreateRectRgnIndirect(&center),
            rRgn = CreateRectRgnIndirect(&center);

    // try every mode with every test
    for (int test = 0; test < NUMREGIONTESTS; test++)
    {
        SelectClipRgn(hdc, NULL);
        HRGN    hRgn0 = CreateRectRgnIndirect(&rTests[test]);

        result1 = SelectClipRgn(hdc, hRgn0);
        result2 = GetClipRgn(hdc, rRgn);

        if (!EqualRgn(rRgn, hRgn0))
            info(FAIL, TEXT("Equal Rgn !Equal test:%d"), test);

        DeleteObject(hRgn0);
    }
    DeleteObject(hRgn1);
    DeleteObject(rRgn);
    myReleaseDC(hdc);

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

    TDC     hdc = myGetDC();
    int     edge = RESIZE(80);

    HRGN    hRgnC = CreateRectRgn(edge, edge, zx / 2 - edge, zy - edge),
            hRgnS = CreateRectRgn(0, 0, zx / 2, zy);
    HBRUSH  hBrushR = (HBRUSH) GetStockObject(LTGRAY_BRUSH),
            hBrushB = (HBRUSH) GetStockObject(BLACK_BRUSH);

    FillRgn(hdc, hRgnS, hBrushR);
    FillRgn(hdc, hRgnC, hBrushB);
    myBitBlt(hdc, zx / 2, 0, zx / 2, zy, hdc, 0, 0, SRCCOPY);

    SelectClipRgn(hdc, hRgnC);
    for (int i = 0; i < 5; i++)
    {
        PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
        Sleep(100);
        PatBlt(hdc, 0, 0, zx, zy, BLACKNESS);
        Sleep(100);
    }

    CheckScreenHalves(hdc);

    SelectClipRgn(hdc, NULL);

    DeleteObject(hRgnS);
    DeleteObject(hRgnC);
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

    TDC     hdc = myGetDC();

    HRGN    hRgnTemp = NULL,
            hRgnS = CreateRectRgn(0, 0, SCube, SCube);

    POINT   SBoard = { zx / 2 / SCube, zy / SCube };

    SelectClipRgn(hdc, NULL);

    SelectObject(hdc, (HBRUSH) GetStockObject(BLACK_BRUSH));

    for (int x = 0; x < SBoard.x; x++)
        for (int y = 0; y < SBoard.y; y++)
        {
            if ((x + y) % 2 == 0)
            {
                hRgnTemp = CreateRectRgn(x * SCube, y * SCube, x * SCube + SCube, y * SCube + SCube);
                CombineRgn(hRgnS, hRgnS, hRgnTemp, RGN_OR);
                DeleteObject(hRgnTemp);
                myPatBlt(hdc, x * SCube + zx / 2, y * SCube, SCube, SCube, PATCOPY);
            }
        }

    SelectClipRgn(hdc, hRgnS);
    myPatBlt(hdc, 0, 0, zx / 2, zy, PATCOPY);

    CheckScreenHalves(hdc);

    // clean up
    DeleteObject(hRgnS);
    SelectClipRgn(hdc, NULL);
    myReleaseDC(hdc);
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

    TDC     hdc = myGetDC();
    HRGN    hRgnS = CreateRectRgn(0, 0, zx / 2, zy);
    POINT   SBoard = { zx / 2 / SCube, zy / SCube };
    int     x,
            y;
    // test uses a complex region to build up an image to verify w/ CheckScreenHalves, can't do driver verification
    BOOL bOldVerify = SetSurfVerify(FALSE);
    
    SelectClipRgn(hdc, NULL);
    SelectObject(hdc, (HBRUSH) GetStockObject(BLACK_BRUSH));

    for (x = 0; x < SBoard.x; x++)
        for (y = 0; y < SBoard.y; y++)
        {
            if ((x + y) % 2 == 0)
                myPatBlt(hdc, x * SCube + zx / 2, y * SCube, SCube, SCube, PATCOPY);
        }

    SelectClipRgn(hdc, hRgnS);
    for (x = 0; x < SBoard.x; x++)
        for (y = 0; y < SBoard.y; y++)
        {
            if ((x + y) % 2 == 1)
                ExcludeClipRect(hdc, x * SCube, y * SCube, x * SCube + SCube, y * SCube + SCube);
        }

    //for areas not covered by cubes
    ExcludeClipRect(hdc, 0, SBoard.y * SCube, zx, zy);
    ExcludeClipRect(hdc, SBoard.x * SCube, 0, zx, zy);

    myPatBlt(hdc, 0, 0, zx / 2, zy, PATCOPY);

    CheckScreenHalves(hdc);
    
    // clean up
    DeleteObject(hRgnS);
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

    result = IntersectClipRect(hdc, 50, 50, 50, 100);   // left = right
    if (result != NULLREGION)
        info(FAIL, TEXT("IntersectClipRect returned %d != expected %d"), result, NULLREGION);

    result = GetClipBox(hdc, &rc);
    if (result != NULLREGION)
        info(FAIL, TEXT("GetClipBox: returned %d != expected %d"), result, NULLREGION);

    if (rc.left + rc.top + rc.right + rc.bottom != 0)
        info(FAIL, TEXT("GetClipBox expected NULL Rect result:%d %d %d %d"), rc.left, rc.top, rc.right, rc.bottom);

    myReleaseDC(hdc);
}

void
TestBadClipRgn(int testFunc)
{

    info(ECHO, TEXT("%s - No bitmap attached to DC"), funcName[testFunc]);
    TDC     hdc = myGetDC(),
            aDC = CreateCompatibleDC(hdc);
    int     result;
    HRGN    hRgn = CreateRectRgn(0, 0, 10, 10);

    result = GetClipRgn(aDC, hRgn);
    if (result != 0)
        info(FAIL, TEXT("GetClipRgn expected:%d returned:%d"), 0, result);

    myReleaseDC(hdc);
    DeleteDC(aDC);
    DeleteObject(hRgn);
}

void
TestNullClipRgn(int testFunc)
{

    info(ECHO, TEXT("%s - No Clip Region in DC"), funcName[testFunc]);
    TDC     hdc = myGetDC(),
            aDC = CreateCompatibleDC(hdc);
    int     result;
    HRGN    hRgn = CreateRectRgn(0, 0, 10, 10);
    TBITMAP hbmp = CreateCompatibleBitmap(hdc, 10, 10), hbmpStock = NULL;

    hbmpStock = (TBITMAP) SelectObject(aDC, hbmp);
    
    SelectClipRgn(aDC, hRgn);
    result = GetClipRgn(aDC, hRgn);
    if (result != NULLREGION)
        info(FAIL, TEXT("GetClipRgn expected:%d returned:%d"), NULLREGION, result);

    SelectObject(aDC, hbmpStock);
    myReleaseDC(hdc);
    DeleteDC(aDC);
    DeleteObject(hRgn);
    DeleteObject(hbmp);
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
    info(ECHO, TEXT("TestGetClipBox2"));
    TDC     hdc = myGetDC();;
    RECT    rc;
    int nRgnRight = (rand() % (zx - 1)) +1;
    int nRgnBottom = (rand() % (zy - 1)) +1;
    HRGN    hRgn = CreateRectRgn(0, 0, nRgnRight, nRgnBottom);
    
    info(ECHO, TEXT("%s - clip Region = [0,0,%d,%d]"), funcName[testFunc], nRgnRight, nRgnBottom);

    if (!hRgn)
    {
        info(FAIL, TEXT("CreateRectRgn() failed: err=%ld"), GetLastError());
        return;
    }

    // paint the inside of the window black
    PatBlt(hdc, 0, 0, zx, zy, BLACKNESS);

    if (SelectClipRgn(hdc, hRgn) == ERROR)
        info(FAIL, TEXT("SelectClipRgn(hdc, hRgn) failed: err=%ld"), GetLastError());

    // paint the section that's left after the clip region white
    PatBlt(hdc, 0, 0, zx, zy, WHITENESS);

    GetClipBox(hdc, &rc);

    if (rc.left != 0 || rc.top != 0 || rc.right != nRgnRight || rc.bottom != nRgnBottom)
        info(FAIL, TEXT("GetClipBox expected: 0,0,%d,%d returned:%d,%d,%d,%d"), nRgnRight, nRgnBottom, rc.left, rc.top, rc.right, rc.bottom);
    Sleep(500);
    DeleteObject(hRgn);
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
    TestBadClipRgn(EGetClipRgn);
    TestNullClipRgn(EGetClipRgn);

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
