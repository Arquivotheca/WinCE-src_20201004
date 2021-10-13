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

   region.cpp

Abstract:

   Gdi Tests: Region APIs

***************************************************************************/

#include "global.h"
#include "rgnData.h"

/***********************************************************************************
***
***         Support Functions
***
************************************************************************************/

//***********************************************************************************
// turns a rect into a region
void
Rect2Rgn(RECT * prR)
{

    RECT    temp;

    temp.top = (*prR).top;
    temp.bottom = (*prR).bottom;
    temp.left = (*prR).left;
    temp.right = (*prR).right;

    if (temp.top - temp.bottom == 0 || temp.left - temp.right == 0)
        (*prR).top = (*prR).bottom = (*prR).left = (*prR).right = 0;
    else
    {
        (*prR).bottom = (temp.bottom < temp.top) ? temp.top : temp.bottom;
        (*prR).top = (temp.bottom < temp.top) ? temp.bottom : temp.top;
        (*prR).left = (temp.right < temp.left) ? temp.right : temp.left;
        (*prR).right = (temp.right < temp.left) ? temp.left : temp.right;
    }
}

//***********************************************************************************
// creates a random region
void
wellFormedRect(RECT * prR)
{
    randomRect(prR);
    Rect2Rgn(prR);
}

//***********************************************************************************
// looks for a point in a rect
BOOL inside(int x, int y, RECT * r)
{
    // Note: in a RECT the right and bottom are not inclusive - 
    // the rect is [left, right) and [top, bottom)
    return x < (*r).right && x >= (*r).left && y < (*r).bottom && y >= (*r).top;
}

//***********************************************************************************
// looks for area sharing between two rects
BOOL areaShare(RECT * a, RECT * b)
{
    return (*a).top >= (*b).top && (*a).bottom <= (*b).bottom && (*a).left <= (*b).left && (*a).right >= (*b).right;
}

//***********************************************************************************
// checks for overlap of rects
BOOL overlap(RECT * a, RECT * b, BOOL isPoint)
{
    RECT    temp;
    BOOL    result;

    // no intersection if b is an empty RECT
    if ((*b).left == (*b).right || (*b).top == (*b).bottom)
    {
        return FALSE;
    }

    // is this a point or a rect?
    if (isPoint)
    {
        result = (inside((*a).left, (*a).top, b));
    }
    else
    {
        // empty RECTs do not overlap
        if ((*a).right == (*a).left || (*a).top == (*a).bottom)
        {
            return FALSE;
        }

        // make sure the RECT is well-formed (left < right, top < bottom)
        temp.top = (*a).top;
        temp.bottom = (*a).bottom;
        temp.left = (*a).left;
        temp.right = (*a).right;

        (*a).bottom = (temp.bottom < temp.top) ? temp.top : temp.bottom;
        (*a).top = (temp.bottom < temp.top) ? temp.bottom : temp.top;
        (*a).left = (temp.right < temp.left) ? temp.right : temp.left;
        (*a).right = (temp.right < temp.left) ? temp.left : temp.right;

        // check for all possible overlaps
        // Note: subtracting 1 from bottom and right values is required, since bottom and right are not inclusive. Using original
        // value will give us the point just to the right or just below the edge of the rect.
        result = (inside((*a).left, (*a).top, b) || inside((*a).left, (*a).bottom - 1, b) || inside((*a).right - 1, (*a).top, b) ||
                  inside((*a).right - 1, (*a).bottom - 1, b) || inside((*b).left, (*b).top, a) || inside((*b).left, (*b).bottom - 1, a) ||
                  inside((*b).right - 1, (*b).top, a) || inside((*b).right - 1, (*b).bottom - 1, a) || areaShare(a, b) || areaShare(b, a));
    }

    return result;
}

/***********************************************************************************
***
***   Uses a random rect to check if regions are being created, set, etc. correctly
***
************************************************************************************/

//***********************************************************************************
void
CreateRectRgnRandom(int testFunc)
{

    info(ECHO, TEXT("%s - %d Random Region Tests"), funcName[testFunc], testCycles);

    HRGN    hRgn = 0;
    RECT    inRect,
            outRect;
    int     counter = 0;
    BOOL    result = 1;

    while (counter++ < testCycles)
    {
        //randomRect(&inRect);
        wellFormedRect(&inRect);

        // check all possible create region functions
        switch (testFunc)
        {
            case EGetRgnBox:
            case ECreateRectRgnIndirect:
                CheckNotRESULT(NULL, hRgn = CreateRectRgnIndirect(&inRect));
                Rect2Rgn(&inRect);
                break;
            case ECreateRectRgn:
                CheckNotRESULT(NULL, hRgn = CreateRectRgn(inRect.left, inRect.top, inRect.right, inRect.bottom));
                Rect2Rgn(&inRect);
                break;
            case ESetRectRgn:
                CheckNotRESULT(NULL, hRgn = CreateRectRgn(0, 0, 0, 0));
                CheckNotRESULT(0, SetRectRgn(hRgn, inRect.left, inRect.top, inRect.right, inRect.bottom));
                Rect2Rgn(&inRect);
                break;
            case EOffsetRgn:
                CheckNotRESULT(NULL, hRgn = CreateRectRgnIndirect(&inRect));

                wellFormedRect(&outRect);   // used as temp

                CheckNotRESULT(ERROR, result = OffsetRgn(hRgn, outRect.left / 2, outRect.top / 2));
                // make sure the offset was right
                inRect.left += outRect.left / 2;
                inRect.top += outRect.top / 2;
                inRect.right += outRect.left / 2;
                inRect.bottom += outRect.top / 2;
                Rect2Rgn(&inRect);

                if ((inRect.left - inRect.right == 0 && result != NULLREGION) ||
                    (inRect.left - inRect.right != 0 && result != SIMPLEREGION))
                {
                    info(FAIL, TEXT("OffsetRgn: count=%d: 1. hRgn=[%d %d %d %d]\r\n"), counter, inRect.left, inRect.top,
                        inRect.right, inRect.bottom);
                    info(FAIL, TEXT("OffsetRgn: count=%d: 2. OffsetRgn=[%d %d]\r\n"), counter, outRect.left / 2, outRect.top / 2);
                    info(FAIL, TEXT("OffsetRgn: count=%d: 3. inRect=[%d %d %d %d]\r\n"), counter, inRect.left, inRect.top,
                        inRect.right, inRect.bottom);
                    info(FAIL, TEXT("OffsetRgn: %s(%d, %d, %d, %d) did not returned %d"), funcName[testFunc], inRect.left, inRect.top,
                         inRect.right, inRect.bottom, result);
                }
                break;
            case EGetRegionData:
                DWORD   dwCount;
                LPRGNDATA lpRgnData;

                CheckNotRESULT(NULL, hRgn = CreateRectRgnIndirect(&inRect));
                Rect2Rgn(&inRect);
                CheckNotRESULT(NULL, dwCount = GetRegionData(hRgn, 0, NULL));

                lpRgnData = (LPRGNDATA) LocalAlloc(LPTR, dwCount);

                // get the data out and check it
                CheckNotRESULT(0, result = (int) GetRegionData(hRgn, dwCount, lpRgnData));

                if (result != (signed) dwCount)
                    info(FAIL, TEXT("%s: returned %d, but needs %d bytes"), funcName[testFunc], result, dwCount);

                //manditory
                if (lpRgnData->rdh.iType != RDH_RECTANGLES)
                    info(FAIL, TEXT("%s: lpRgnData->rdh.iType not RDH_RECTANGLES"), funcName[testFunc]);

                // is the count right?
                if ((inRect.left - inRect.right == 0 && lpRgnData->rdh.nCount != 0) ||
                    (inRect.left - inRect.right != 0 && lpRgnData->rdh.nCount != 1))
                    info(FAIL, TEXT("GetRegionData(%d, %d, %d, %d): lpRgnData->rdh.nCount = %d"), lpRgnData->rdh.nCount,
                         inRect.left, inRect.top, inRect.right, inRect.bottom);

                // is created rect is not the same as data flag error
                if (lpRgnData->rdh.nCount > 0)
                {
                    RECT   *temp = (RECT *) lpRgnData->Buffer;

                    if (!isEqualRect(&inRect, temp))
                        info(FAIL, TEXT("%s(%d, %d, %d, %d) lpRgnData->Buffer:(%d, %d, %d, %d)"), funcName[testFunc],
                             inRect.left, inRect.top, inRect.right, inRect.bottom, (*temp).left, (*temp).top, (*temp).right,
                             (*temp).bottom);
                }

                // is the bounding rect correct?
                if (!isEqualRect(&inRect, &lpRgnData->rdh.rcBound))
                    info(FAIL, TEXT("%s(%d, %d, %d, %d) lpRgnData->rdh.rcBound:(%d, %d, %d, %d)"), funcName[testFunc],
                         inRect.left, inRect.top, inRect.right, inRect.bottom, lpRgnData->rdh.rcBound.left,
                         lpRgnData->rdh.rcBound.top, lpRgnData->rdh.rcBound.right, lpRgnData->rdh.rcBound.bottom);

                LocalFree(lpRgnData);
                break;
        }

        // getting a rect out
        if (!GetRgnBox(hRgn, &outRect))
            info(FAIL, TEXT("GetRgnBox(%d, %d, %d, %d) returned 0"), outRect.left, outRect.top, outRect.right, outRect.bottom);

        CheckNotRESULT(FALSE, DeleteObject(hRgn));

        // make sure in==out
        if (!isEqualRect(&inRect, &outRect))
        {
            info(FAIL, TEXT("%s count=%d: returns NT:(%d, %d, %d, %d) => OS:(%d, %d, %d, %d)"), funcName[testFunc], counter,
                 inRect.left, inRect.top, inRect.right, inRect.bottom, outRect.left, outRect.top, outRect.right,
                 outRect.bottom);

            MyDelay(1);
        }
    }
}

/***********************************************************************************
***
***   Passes Null as every possible parameter
***
************************************************************************************/

//***********************************************************************************
void
passNull2Region(int testFunc)
{

    info(ECHO, TEXT("%s - Passing NULL"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    RECT    test;
    LPRGNDATA lpRgnData = (LPRGNDATA) LocalAlloc(LPTR, 10);
    HRGN    hRgn0,
            hRgn1,
            hRgn2,
            hRgnBad;
    HBRUSH hbr,
           hBrushBad;

    CheckNotRESULT(NULL, hbr = (HBRUSH) GetStockObject(BLACK_BRUSH));
    CheckNotRESULT(NULL, hRgn0 = CreateRectRgn(0, 0, 10, 10));
    CheckNotRESULT(NULL, hRgn1 = CreateRectRgn(10, 10, 20, 20));
    CheckNotRESULT(NULL, hRgn2 = CreateRectRgn(0, 0, 20, 20));
    
    // create an invalid region
    CheckNotRESULT(NULL, hRgnBad = CreateRectRgn(0, 0, 20, 20));
    CheckNotRESULT(FALSE, DeleteObject(hRgnBad));

    // create an invalid brush
    CheckNotRESULT(NULL, hBrushBad = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF)));
    CheckNotRESULT(FALSE, DeleteObject(hBrushBad));

    // randomRect(&test);
    wellFormedRect(&test);
    switch (testFunc)
    {
        case EGetRgnBox:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetRgnBox((HRGN) NULL, &test));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetRgnBox(hRgnBad, &test));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetRgnBox((HRGN) g_hpenStock, &test));
            CheckForLastError(ERROR_INVALID_HANDLE);

#ifdef UNDER_CE
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetRgnBox(hRgn0, NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);
#endif
            break;
        case ECreateRectRgnIndirect:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, CreateRectRgnIndirect(NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);
            break;
        case ESetRectRgn:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetRectRgn(NULL, test.left, test.top, test.right, test.bottom));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetRectRgn(hRgnBad, test.left, test.top, test.right, test.bottom));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetRectRgn((HRGN) g_hbrushStock, test.left, test.top, test.right, test.bottom));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EFrameRgn:
// not supported on CE.
#ifndef UNDER_CE
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FrameRgn(NULL, NULL, NULL, 0, 0));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FrameRgn(NULL, NULL, NULL, -1, -1));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FrameRgn(hdc, NULL, NULL, 5, 5));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FrameRgn(hdc, hRgn0, NULL, 5, 5));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FrameRgn(g_hdcBAD, hRgn0, (HBRUSH) GetStockObject(BLACK_BRUSH), 5, 5));
            CheckForLastError(ERROR_INVALID_HANDLE);
            
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FrameRgn(g_hdcBAD2, hRgn0, (HBRUSH) GetStockObject(BLACK_BRUSH), 5, 5));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FrameRgn(NULL, hRgn0, (HBRUSH) GetStockObject(BLACK_BRUSH), 5, 5));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FrameRgn(hdc, hRgn0, (HBRUSH) GetStockObject(BLACK_BRUSH), 0, 0));
            CheckForLastError(ERROR_INVALID_PARAMETER);
#endif // UNDER_CE
            break;
        case EEqualRgn:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, EqualRgn((HRGN) NULL,(HRGN) NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, EqualRgn(hRgn0,(HRGN) NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, EqualRgn((HRGN)NULL, hRgn0));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, EqualRgn(hRgn0, hRgnBad));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, EqualRgn(hRgn0, (HRGN) g_hpenStock));
            CheckForLastError(ERROR_INVALID_HANDLE);
 
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, EqualRgn(hRgnBad, hRgn0));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, EqualRgn((HRGN)g_hpalStock, hRgn0));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EOffsetRgn:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, OffsetRgn((HRGN) NULL, test.left, test.top));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, OffsetRgn(hRgnBad, test.left, test.top));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, OffsetRgn((HRGN) g_hpalStock, test.left, test.top));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EGetRegionData:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, GetRegionData((HRGN) NULL, sizeof(lpRgnData), lpRgnData));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, GetRegionData(hRgnBad, sizeof(lpRgnData), lpRgnData));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(ERROR, GetRegionData((HRGN) g_hpenStock, sizeof(lpRgnData), lpRgnData));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EFillRgn:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FillRgn((TDC) NULL,(HRGN) NULL,(HBRUSH) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FillRgn(hdc, hRgn0, (HBRUSH) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FillRgn(hdc, (HRGN) NULL, hbr));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FillRgn((TDC) NULL, hRgn0, hbr));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FillRgn(hdc, hRgn0, hBrushBad));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FillRgn(hdc, hRgn0, (HBRUSH) g_hpenStock));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FillRgn(hdc, hRgnBad, hbr));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FillRgn(hdc, (HRGN) g_hpenStock, hbr));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FillRgn(g_hdcBAD, hRgn0, hbr));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FillRgn(g_hdcBAD2, hRgn0, hbr));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FillRgn(g_hdcBAD, hRgnBad, hBrushBad));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, FillRgn(g_hdcBAD2, hRgnBad, hBrushBad));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EPtInRegion:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, PtInRegion((HRGN) NULL, test.left, test.top));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, PtInRegion(hRgnBad, test.left, test.top));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, PtInRegion((HRGN) g_hpenStock, test.left, test.top));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case ERectInRegion:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, RectInRegion((HRGN) NULL, &test));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, RectInRegion(hRgnBad, &test));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, RectInRegion((HRGN) g_hbrushStock, &test));
            CheckForLastError(ERROR_INVALID_HANDLE);

#ifdef UNDER_CE
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, RectInRegion(hRgn0, NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);
#endif
            break;
        case ECombineRgn:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, CombineRgn(hRgn0, hRgn1, hRgn2, -1));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, CombineRgn((HRGN) NULL,(HRGN) NULL, NULL, RGN_XOR));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, CombineRgn((HRGN) NULL, hRgn1, hRgn2, RGN_XOR));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, CombineRgn(hRgn0, (HRGN) NULL, hRgn2, RGN_XOR));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, CombineRgn(hRgn0, hRgn1, (HRGN) NULL, RGN_XOR));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, CombineRgn(hRgnBad, hRgn1, hRgn2, RGN_XOR));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, CombineRgn((HRGN) g_hfontStock, hRgn1, hRgn2, RGN_XOR));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, CombineRgn(hRgn0, hRgnBad, hRgn2, RGN_XOR));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, CombineRgn(hRgn0, (HRGN) g_hbrushStock, hRgn2, RGN_XOR));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, CombineRgn(hRgn0, hRgn1, hRgnBad, RGN_XOR));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, CombineRgn(hRgn0, hRgn1, (HRGN) g_hfontStock, RGN_XOR));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, CombineRgn(hRgnBad, hRgnBad, hRgnBad, RGN_XOR));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
    }

    LocalFree(lpRgnData);
    CheckNotRESULT(FALSE, DeleteObject(hRgn0));
    CheckNotRESULT(FALSE, DeleteObject(hRgn1));
    CheckNotRESULT(FALSE, DeleteObject(hRgn2));
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Checks region API return values against precompiled NT results
***
************************************************************************************/

//***********************************************************************************
void
checkReturnValues(int testFunc)
{

    info(ECHO, TEXT("%s - Checking Return Values"), funcName[testFunc]);
    setTestRectRgn();

    RECT    test = { 0, 0, 0, 0 };  // null region
    BOOL    result = 0,
            known;
    int     pass;

    HRGN    hRgn0,
            hRgn1,
            destRgn;

    CheckNotRESULT(NULL, hRgn1 = CreateRectRgnIndirect(&center));
    CheckNotRESULT(NULL, destRgn = CreateRectRgnIndirect(&center));

    // look at all three elements - indexes into an array of region combos
    for (pass = 0; pass < 3; pass++)
    {
        CheckNotRESULT(NULL, hRgn0 = CreateRectRgnIndirect(&rTests[passArray[pass][0]]));
        known = passArray[pass][2]; // one of the region enum types
        CheckNotRESULT(ERROR, CombineRgn(destRgn, hRgn0, hRgn1, unionMode[passArray[pass][1]]));

        switch (testFunc)
        {
            case EGetRgnBox:
                result = GetRgnBox(destRgn, &test);
                break;
            case EOffsetRgn:
                result = OffsetRgn(destRgn, test.left, test.top);
                break;
            case ECombineRgn:
                result = CombineRgn(destRgn, destRgn, destRgn, RGN_AND);
                break;
        }

        if (result != known)
            info(FAIL, TEXT("%s returned %d instead of %d: using(rect:center & #%d, patt:%d)"), funcName[testFunc], result,
                 known, passArray[pass][0], unionMode[passArray[pass][1]]);
        CheckNotRESULT(FALSE, DeleteObject(hRgn0));
    }
    CheckNotRESULT(FALSE, DeleteObject(hRgn1));
    CheckNotRESULT(FALSE, DeleteObject(destRgn));
}

/***********************************************************************************
***
***   Plays darts using a random target rgn and random shot rgn.
***
************************************************************************************/

//***********************************************************************************
void
inRegionRandom(int testFunc)
{

    info(ECHO, TEXT("%s - %d Random In-Region Tests"), funcName[testFunc], testCycles);

    HRGN    hRgn;
    RECT    targetRect,
            shotRect;
    BOOL    result = 0;
    int     counter = 0;

    while (counter++ < testCycles)
    {
        // create random rects
        //randomRect(&shotRect);
        wellFormedRect(&shotRect);
        //randomRect(&targetRect);
        wellFormedRect(&targetRect);
        // create region based on random rect
        CheckNotRESULT(NULL, hRgn = CreateRectRgnIndirect(&targetRect));

        if (!hRgn)
            info(FAIL, TEXT("CreateRectRgnIndirect(%d, %d, %d, %d) returned NULL"), targetRect.left, targetRect.top,
                 targetRect.right, targetRect.bottom);

        switch (testFunc)
        {
            case EPtInRegion:   // turn rect into point
                shotRect.right = shotRect.left;
                shotRect.bottom = shotRect.top;
                result = PtInRegion(hRgn, shotRect.left, shotRect.top);
                break;
            case ERectInRegion:
                result = RectInRegion(hRgn, &shotRect);
                break;
        }

        CheckNotRESULT(FALSE, DeleteObject(hRgn));
        Rect2Rgn(&targetRect);

        if (result != overlap(&shotRect, &targetRect, testFunc == EPtInRegion))
            info(FAIL, TEXT("%s returned %d for shot:(%d %d %d %d) target:(%d %d %d %d)"), funcName[testFunc], result,
                 shotRect.left, shotRect.top, shotRect.right, shotRect.bottom, targetRect.left, targetRect.top,
                 targetRect.right, targetRect.bottom);
    }
}

/***********************************************************************************
***
***   Checks OS against NT suite
***
************************************************************************************/

//***********************************************************************************
void
combinePair(int testFunc)
{

    info(ECHO, TEXT("%s - %d Combine Region Tests Checking against NT Hardcoded"), funcName[testFunc], uTests * NUMREGIONTESTS);

    setTestRectRgn();
    int     result1 = 0,
            result3 = 0,
            i,
            curIndex;
    RECT   *temp;
    HRGN    hRgn1;
    HRGN    destRgn;
    int     maxerrors = 0;

    CheckNotRESULT(NULL, hRgn1 = CreateRectRgnIndirect(&center));
    CheckNotRESULT(NULL, destRgn = CreateRectRgnIndirect(&center));

    // try every mode with every test
    for (int mode = 0; mode < uTests; mode++)
        for (int test = 0; test < NUMREGIONTESTS; test++)
        {
            HRGN hRgn0;
            DWORD   dwCount;
            LPRGNDATA lpRgnData;
            
            CheckNotRESULT(NULL, hRgn0 = CreateRectRgnIndirect(&rTests[test]));

            CheckNotRESULT(ERROR, CombineRgn(destRgn, hRgn0, hRgn1, unionMode[mode]));

            CheckNotRESULT(0, dwCount = GetRegionData(destRgn, 0, NULL));
            lpRgnData = (LPRGNDATA) LocalAlloc(LPTR, dwCount);

            CheckNotRESULT(0, result1 = (int) GetRegionData(destRgn, dwCount, lpRgnData));
            curIndex = test + mode * (NUMREGIONTESTS);

            // Verify CombineRgn
            if (NTResults[curIndex][0].left == (signed) lpRgnData->rdh.nCount)
            {
                result3 = 0;
                for (i = 1; i <= NTResults[curIndex][0].left; i++)
                {
                    temp = (RECT *) (lpRgnData->Buffer + sizeof (RECT) * (i - 1));
                    result3 |= !isEqualRect(temp, &NTResults[curIndex][i]);
                }
                if (result3)
                {
                    for (i = 1; i <= NTResults[curIndex][0].left; i++)
                    {
                        temp = (RECT *) (lpRgnData->Buffer + sizeof (RECT) * (i - 1));
                        if (maxerrors < 10)
                        {
                            info(DETAIL,
                                 TEXT
                                 ("%s: * mode:%d, test:%d OS:(%d, %d, %d, %d) NT HardCode:(%d, %d, %d, %d): diff is OK"),
                                 funcName[testFunc], mode, test, (*temp).left, (*temp).top, (*temp).right, (*temp).bottom,
                                 NTResults[curIndex][i].left, NTResults[curIndex][i].top, NTResults[curIndex][i].right,
                                 NTResults[curIndex][i].bottom);
                            maxerrors++;
                        }
                    }
                }
            }
            else if (maxerrors < 10)
            {
                info(DETAIL, TEXT("%s: * (%d, %d, %d, %d) w/ center mode:%d, test:%d lpRgnData->rdh.nCount=> OS:%d NT:%d"),
                     funcName[testFunc], rTests[test].left, rTests[test].top, rTests[test].right, rTests[test].bottom, mode,
                     test, lpRgnData->rdh.nCount, NTResults[curIndex][0].left);
                maxerrors++;
            }
            if (lpRgnData->rdh.iType != RDH_RECTANGLES)
                info(FAIL, TEXT("GetRegionData: lpRgnData->rdh.iType not RDH_RECTANGLES"));

            CheckNotRESULT(FALSE, DeleteObject(hRgn0));
            LocalFree(lpRgnData);
        }
    if (maxerrors >= 10)
        info(DETAIL, TEXT("More than 10 errors found. Part of test skipped."));

    CheckNotRESULT(FALSE, DeleteObject(hRgn1));
    CheckNotRESULT(FALSE, DeleteObject(destRgn));
}

/***********************************************************************************
***
***   Blasts OS results to screen (rgn same as combinePair - above)
***
************************************************************************************/

//***********************************************************************************
void
FillPair(int testFunc, int pass, int clipping)
{
    info(ECHO, TEXT("%s - %d Combine Region Tests Checking against NT Hardcoded - %s %s"), funcName[testFunc], testCycles,
         (pass == 0) ? TEXT("Visual") : TEXT("Graphical"), (clipping == 0) ? TEXT("No Clipping") : TEXT("Clipping"));

    // also setup center
    setTestRectRgn();
    TDC      hdc = myGetDC();
    HBRUSH hbr0,
               hbr1;
    RECT    myCenter = { RESIZE(center.left), RESIZE(center.top), RESIZE(center.right), RESIZE(center.bottom) }, temp0;
    HRGN    hRgn1,
               destRgn,
               hRgnClip;
    int        leftside = zx / 2;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hbr0 = CreateSolidBrush(PALETTERGB(0, 0, 0))); // black
        CheckNotRESULT(NULL, hbr1 = CreateSolidBrush(PALETTERGB(255, 0, 0))); // red
        CheckNotRESULT(NULL, hRgn1 = CreateRectRgnIndirect(&myCenter));
        CheckNotRESULT(NULL, destRgn = CreateRectRgnIndirect(&myCenter));

        CheckNotRESULT(NULL, hRgnClip = CreateRectRgn(0, RESIZE(center.top + 20), zx, RESIZE(center.bottom - 20)));

        CheckNotRESULT(0, GetRgnBox(hRgnClip, &temp0));
        if (clipping)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            CheckNotRESULT(NULL, SelectClipRgn(hdc, hRgnClip));
        }

        // is anything null?
        if (!hbr0 || !hbr1 || !hRgn1 || !destRgn)
            info(FAIL, TEXT("Failure to initialize Brush or Rgn"));

        // try all modes and tests
        for (int mode = 0; mode < uTests; mode++)
            for (int test = 0; test < NUMREGIONTESTS; test++)
            {
                HRGN hRgn0;
                temp0.left = RESIZE(rTests[test].left);
                temp0.top = RESIZE(rTests[test].top);
                temp0.right = RESIZE(rTests[test].right);
                temp0.bottom = RESIZE(rTests[test].bottom);

                CheckNotRESULT(NULL, hRgn0 = CreateRectRgnIndirect(&temp0));
                CheckNotRESULT(ERROR, CombineRgn(destRgn, hRgn0, hRgn1, unionMode[mode]));
                CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
                // visual testing only?
                if (pass == 0)
                {
                    CheckNotRESULT(FALSE, FillRgn(hdc, hRgn0, hbr1));
                    CheckNotRESULT(FALSE, FillRgn(hdc, hRgn1, hbr1));
                    MyDelay(1);
                }
                CheckNotRESULT(FALSE, FillRgn(hdc, destRgn, hbr0));
                MyDelay(2);

                // visual testing only?
                if (pass == 0)
                    Sleep(100);
                else
                {
                    if (gSanityChecks)
                        info(DETAIL, TEXT("Testing Rect (%d %d %d %d) index:%d mode:%d"), temp0.left, temp0.top, temp0.right,
                             temp0.bottom, test, mode);

                    RECT   *temp;
                    DWORD   dwCount;
                    LPRGNDATA lpRgnData;

                    CheckNotRESULT(0, dwCount = GetRegionData(destRgn, 0, NULL));

                    lpRgnData = (LPRGNDATA) LocalAlloc(LPTR, dwCount);

                    CheckNotRESULT(0, GetRegionData(destRgn, dwCount, lpRgnData));

                    // for small device:240x320,208x240, destRgn sometime are over the middle line of the screen
                    // so need to clean out the right half before blt
                    CheckNotRESULT(FALSE, PatBlt(hdc, zx / 2, 0, zx, zy, WHITENESS));

                    // cover over all rects with white
                    for (int i = 1; i <= (signed) lpRgnData->rdh.nCount; i++)
                    {
                        temp = (RECT *) (lpRgnData->Buffer + sizeof (RECT) * (i - 1));
                        CheckNotRESULT(FALSE, PatBlt(hdc, (*temp).left + leftside, (*temp).top, (*temp).right - (*temp).left,
                                                         (*temp).bottom - (*temp).top, BLACKNESS));
                    }

                    // is the screen equal?
                    CheckScreenHalves(hdc);
                    LocalFree(lpRgnData);
                    lpRgnData = NULL;
                    MyDelay(3);

                }
                CheckNotRESULT(FALSE, DeleteObject(hRgn0));
            }

        CheckNotRESULT(NULL, SelectClipRgn(hdc, NULL));
        CheckNotRESULT(FALSE, DeleteObject(hRgn1));
        CheckNotRESULT(FALSE, DeleteObject(hbr0));
        CheckNotRESULT(FALSE, DeleteObject(hbr1));
        CheckNotRESULT(FALSE, DeleteObject(destRgn));
        CheckNotRESULT(FALSE, DeleteObject(hRgnClip));
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
FillCheckRgn(int testFunc)
{
    info(ECHO, TEXT("%s - %d Checks for correct drawing"), funcName[testFunc], NUMREGIONTESTS);

    TDC     hdc = myGetDC();
    HBRUSH  hbr;    // black

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hbr = CreateSolidBrush(RGB(0, 0, 0)));    // black

        if (!hbr)
            info(FAIL, TEXT("Failure to initialize Brush"));

        setTestRectRgn();

        for (int test = 0; test < NUMREGIONTESTS; test++)
        {
            HRGN hRgn;
            RECT    hRgnRect;

            CheckNotRESULT(NULL, hRgn = CreateRectRgnIndirect(&rTests[test]));

            CheckNotRESULT(0, GetRgnBox(hRgn, &hRgnRect));
            // cover screen with white
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            // fill region with black
            CheckNotRESULT(FALSE, FillRgn(hdc, hRgn, hbr));
            // do checks for correct drawing
            goodBlackRect(hdc, &hRgnRect);
            CheckNotRESULT(FALSE, DeleteObject(hRgn));
        }
        CheckNotRESULT(FALSE, DeleteObject(hbr));
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Expects for GetRgnData to return a 1 for success
***
************************************************************************************/

//***********************************************************************************
void
GetRegionDataCheckReturn(int testFunc)
{

    info(ECHO, TEXT("%s - Expecting Success == dwCount"), funcName[testFunc]);

    int     result;
    RECT    inRect = { 0, 0, 10, 10 };
    HRGN    hRgn;
    DWORD   dwCount;
    LPRGNDATA lpRgnData;

    CheckNotRESULT(NULL, hRgn = CreateRectRgnIndirect(&inRect));
    CheckNotRESULT(0, dwCount = GetRegionData(hRgn, 0, NULL));
    lpRgnData = (LPRGNDATA) LocalAlloc(LPTR, dwCount);

    // get the data out and check it
    CheckNotRESULT(0, result = (int) GetRegionData(hRgn, dwCount, lpRgnData));
    if (result != (int) dwCount)
        info(FAIL, TEXT("%s: returned %d, expected success==%d"), funcName[testFunc], result, dwCount);

    CheckNotRESULT(FALSE, DeleteObject(hRgn));
    LocalFree(lpRgnData);
}

/***********************************************************************************
***
***   Passing NULL to all functions
***
************************************************************************************/

//***********************************************************************************
void
CheckingNullRegion(int testFunc)
{

    info(ECHO, TEXT("%s - Checks for Null Region"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    RECT    lineRectH = { 50, 5, 100, 5 }, 
            lineRectV ={ 5, 50, 5, 100},
            outRectH, outRectV;
    HRGN    resultH = 0,
            resultV = 0;

    switch (testFunc)
    {
        case ECreateRectRgnIndirect:
            CheckNotRESULT(NULL, resultH = CreateRectRgnIndirect(&lineRectH));
            CheckNotRESULT(NULL, resultV = CreateRectRgnIndirect(&lineRectV));
            break;
        case ECreateRectRgn:
            CheckNotRESULT(NULL, resultH = CreateRectRgn(lineRectH.left, lineRectH.top, lineRectH.right, lineRectH.bottom));
            CheckNotRESULT(NULL, resultV = CreateRectRgn(lineRectV.left, lineRectV.top, lineRectV.right, lineRectV.bottom));
            break;
    }

    CheckNotRESULT(0, GetRgnBox(resultH, &outRectH));
    CheckNotRESULT(0, GetRgnBox(resultV, &outRectV));
    // check for change to nullrgn
    if (outRectH.left == 50)
        info(FAIL, TEXT("Horizontal:(%d, %d, %d, %d) didn't collapse to NULL rgn"), funcName[testFunc], lineRectH.left,
             lineRectH.top, lineRectH.right, lineRectH.bottom);
    if (outRectV.left == 5)
        info(FAIL, TEXT("Vertical:(%d, %d, %d, %d) didn't collapse to NULL rgn"), funcName[testFunc], lineRectV.left,
             lineRectV.top, lineRectV.right, lineRectV.bottom);

    CheckNotRESULT(FALSE, DeleteObject(resultH));
    CheckNotRESULT(FALSE, DeleteObject(resultV));
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Strange Params CombineRgn
***
************************************************************************************/

//***********************************************************************************
void
StrangeCombineRgn(int testFunc)
{

    info(ECHO, TEXT("%s - Checks for Null Region"), funcName[testFunc]);
    int     result;
    HRGN    hRgnDest,
            hRgnSrc;
    RECT    aRect,
            oldRect = { 10, 10, 100, 100 };

    CheckNotRESULT(NULL, hRgnDest = CreateRectRgn(10, 10, 100, 100));
    CheckNotRESULT(NULL, hRgnSrc = CreateRectRgn(10, 10, 100, 100));

    CheckNotRESULT(0, result = CombineRgn(hRgnSrc, hRgnSrc, hRgnSrc, RGN_AND));
    CheckNotRESULT(0, GetRgnBox(hRgnSrc, &aRect));

    if (!isEqualRect(&aRect, &oldRect))
        info(FAIL, TEXT("(hRgnSrc,hRgnSrc,hRgnSrc,RGN_AND) hRgnSrc:(%d %d %d %d) expected:(10,10,100,100)"), aRect.left,
             aRect.top, aRect.right, aRect.bottom);

    CheckNotRESULT(FALSE, DeleteObject(hRgnDest));
    CheckNotRESULT(FALSE, DeleteObject(hRgnSrc));
}

/***********************************************************************************
***
***   Combine Or Complex Test
***
************************************************************************************/
#define RCube RESIZE(6)

//***********************************************************************************
void
CombineOrComplexTest(int testFunc)
{
    info(ECHO, TEXT("%s - Select Complex Test"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HRGN    hRgnTemp = NULL,
            hRgnS0,
            hRgnS1;
    int     x,
            y,
            result;
    POINT   RBoard = { zx / 2 / RCube, zy / RCube };

    // this test case requires a complex region which verification is unable to handle.
    BOOL bOldVerify = GetSurfVerify();

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hRgnS0 = CreateRectRgn(0, 0, RCube, RCube));
        CheckNotRESULT(NULL, hRgnS1 = CreateRectRgn(0, 0, RCube, RCube));
        CheckNotRESULT(NULL, SelectObject(hdc, (HBRUSH) GetStockObject(BLACK_BRUSH)));
        
        // clear the surface to draw the checkerboard
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

        for (x = 0; x < RBoard.x; x++)
            for (y = 0; y < RBoard.y; y++)
            {
                if ((x + y) % 4 == 0)
                {
                    CheckNotRESULT(NULL, hRgnTemp = CreateRectRgn(x * RCube, y * RCube, x * RCube + RCube, y * RCube + RCube));
                    CheckNotRESULT(ERROR, CombineRgn(hRgnS0, hRgnS0, hRgnTemp, RGN_OR));
                    CheckNotRESULT(FALSE, DeleteObject(hRgnTemp));
                    CheckNotRESULT(FALSE, PatBlt(hdc, x * RCube + zx / 2, y * RCube, RCube, RCube, PATCOPY));
                }
            }
        for (x = 0; x < RBoard.x; x++)
            for (y = 0; y < RBoard.y; y++)
            {
                if ((x + y) % 4 == 2)
                {
                    CheckNotRESULT(NULL, hRgnTemp = CreateRectRgn(x * RCube, y * RCube, x * RCube + RCube, y * RCube + RCube));
                    CheckNotRESULT(ERROR, CombineRgn(hRgnS1, hRgnS1, hRgnTemp, RGN_OR));
                    CheckNotRESULT(FALSE, DeleteObject(hRgnTemp));
                    CheckNotRESULT(FALSE, PatBlt(hdc, x * RCube + zx / 2, y * RCube, RCube, RCube, PATCOPY));
                }
            }
        // this makes the surface a complex region, verification doesn't know how to handle
        // complex regions when comparing surfaces, don't verify.
        bOldVerify = SetSurfVerify(FALSE);
        CheckNotRESULT(NULL, hRgnTemp = CreateRectRgn(-300, -300, -310, -310));
        CheckNotRESULT(ERROR, result = CombineRgn(hRgnTemp, hRgnS0, hRgnTemp, RGN_DIFF));
        if (result != COMPLEXREGION)
            info(FAIL, TEXT("CombineRgn(RGN_DIFF) returned:%d expected:%d"), result, COMPLEXREGION);

        CheckNotRESULT(ERROR, result = CombineRgn(hRgnTemp, hRgnTemp, hRgnS1, RGN_OR));
        if (result != COMPLEXREGION)
            info(FAIL, TEXT("CombineRgn(RGN_OR) returned:%d expected:%d"), result, COMPLEXREGION);

        CheckNotRESULT(NULL, SelectObject(hdc, hRgnTemp));
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx / 2, zy, PATCOPY));
        CheckScreenHalves(hdc);
        // clean up
        CheckNotRESULT(FALSE, DeleteObject(hRgnS0));
        CheckNotRESULT(FALSE, DeleteObject(hRgnS1));
        CheckNotRESULT(ERROR, SelectClipRgn(hdc, NULL));
        CheckNotRESULT(FALSE, DeleteObject(hRgnTemp));
    }
    
    myReleaseDC(hdc);

    // verification tolerances must be left active until after myReleaseDC does verification.
    SetSurfVerify(bOldVerify);
}

/***********************************************************************************
***
***   Combine Or Complex Test
***
************************************************************************************/
#define RCube2 22

//***********************************************************************************
void
CombineOrComplexTest2(int testFunc)
{
    info(ECHO, TEXT("%s - Select Complex Test"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HRGN    hRgnTemp = NULL,
            hRgnS0,
            hRgnS1;
    int     x,
            y,
            result;
    POINT   RBoard = { zx / 2 / RCube2, zy / RCube2 };

    // this test case requires a complex region which verification is unable to handle.
    BOOL bOldVerify = GetSurfVerify();

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hRgnS0 = CreateRectRgn(0, 0, RCube2, RCube2));
        CheckNotRESULT(NULL, hRgnS1 = CreateRectRgn(0, 0, RCube2, RCube2));

        CheckNotRESULT(NULL, SelectObject(hdc, (HBRUSH) GetStockObject(BLACK_BRUSH)));

        // clear the surface to draw the checkerboard
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

        for (x = 0; x < RBoard.x; x++)
            for (y = 0; y < RBoard.y; y++)
            {
                if ((x + y) % 4 == 0)
                {
                    CheckNotRESULT(NULL, hRgnTemp = CreateRectRgn(x * RCube2, y * RCube2, x * RCube2 + RCube2, y * RCube2 + RCube2));
                    CheckNotRESULT(ERROR, CombineRgn(hRgnS0, hRgnS0, hRgnTemp, RGN_OR));
                    CheckNotRESULT(FALSE, DeleteObject(hRgnTemp));
                    CheckNotRESULT(FALSE, PatBlt(hdc, x * RCube2 + zx / 2, y * RCube2, RCube2, RCube2, PATCOPY));
                }
            }
        for (x = 0; x < RBoard.x; x++)
            for (y = 0; y < RBoard.y; y++)
            {
                if ((x + y) % 4 == 2)
                {
                    CheckNotRESULT(NULL, hRgnTemp = CreateRectRgn(x * RCube2, y * RCube2, x * RCube2 + RCube2, y * RCube2 + RCube2));
                    CheckNotRESULT(ERROR, CombineRgn(hRgnS1, hRgnS1, hRgnTemp, RGN_OR));
                    CheckNotRESULT(FALSE, DeleteObject(hRgnTemp));
                    CheckNotRESULT(FALSE, PatBlt(hdc, x * RCube2 + zx / 2, y * RCube2, RCube2, RCube2, PATCOPY));
                }
            }

        // this makes the region complex, and verification is unable to handle complex regions.
        bOldVerify = SetSurfVerify(FALSE);

        CheckNotRESULT(NULL, hRgnTemp = CreateRectRgn(-300, -300, -310, -310));
        CheckNotRESULT(ERROR, result = CombineRgn(hRgnTemp, hRgnS0, hRgnTemp, RGN_DIFF));
        if (result != COMPLEXREGION)
            info(FAIL, TEXT("CombineRgn returned:%d expected:%d"), result, COMPLEXREGION);
        CheckNotRESULT(ERROR, result = CombineRgn(hRgnTemp, hRgnTemp, hRgnS1, RGN_OR));
        if (result != COMPLEXREGION)
            info(FAIL, TEXT("CombineRgn returned:%d expected:%d"), result, COMPLEXREGION);

        CheckNotRESULT(NULL, SelectObject(hdc, hRgnTemp));
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx / 2, zy, PATCOPY));
        CheckScreenHalves(hdc);
        // clean up
        CheckNotRESULT(FALSE, DeleteObject(hRgnS0));
        CheckNotRESULT(FALSE, DeleteObject(hRgnS1));
        CheckNotRESULT(ERROR, SelectClipRgn(hdc, NULL));
        CheckNotRESULT(FALSE, DeleteObject(hRgnTemp));
    }
    
    myReleaseDC(hdc);

    // verification tolerances must be left active until after myReleaseDC does verification.
    SetSurfVerify(bOldVerify);
}

/***********************************************************************************
***
***   Combine Rand Complex Test
***
************************************************************************************/
//***********************************************************************************
void
CombineRandComplexTest(int testFunc)
{
    info(ECHO, TEXT("%s - Combine Rand Complex Crash Test"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    RECT    aRect = { (zx - 200) / 2, (zy - 200) / 2, (zx + 200) / 2, (zy + 200) / 2 };
    HRGN    hRgnTemp,
            hPattern,
            hScreen;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        for (int i = 0; i < 5; i++)
        {
            hRgnTemp = NULL;
            CheckNotRESULT(NULL, hPattern = CreateRectRgn((zx - 100) / 2, (zy - 100) / 2, (zx + 100) / 2, (zy + 100) / 2));
            CheckNotRESULT(NULL, hScreen = CreateRectRgn(0, 0, zx, zy));
            for (int x = 0; x < 60; x++)
            {
                aRect.left = goodRandomNum(zx);
                aRect.top = goodRandomNum(zy);
                aRect.right = goodRandomNum(zx);
                aRect.bottom = goodRandomNum(zy);
                Rect2Rgn(&aRect);
                CheckNotRESULT(NULL, hRgnTemp = CreateRectRgnIndirect(&aRect));
                CheckNotRESULT(ERROR, CombineRgn(hPattern, hPattern, hRgnTemp, RGN_XOR));
                CheckNotRESULT(FALSE, DeleteObject(hRgnTemp));
            }
            CheckNotRESULT(ERROR, CombineRgn(hPattern, hScreen, hPattern, RGN_DIFF));
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            CheckNotRESULT(FALSE, FillRgn(hdc, hPattern, (HBRUSH) GetStockObject(BLACK_BRUSH)));
            // clean up
            CheckNotRESULT(FALSE, DeleteObject(hPattern));
            CheckNotRESULT(FALSE, DeleteObject(hScreen));
        }
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Combine Rand Complex Test 2
***
************************************************************************************/
//***********************************************************************************
void
CombineRandComplexTest2(int testFunc)
{
    info(ECHO, TEXT("%s - Combine Rand Complex Crash Test 2"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    RECT    aRect = { (zx - 200) / 2, (zy - 200) / 2, (zx + 200) / 2, (zy + 200) / 2 };
    HRGN    hRgnTemp,
            hPattern,
            hScreen;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        for (int i = 0; i < testCycles; i++)
        {
            hRgnTemp = NULL;
            CheckNotRESULT(NULL, hPattern = CreateRectRgn((zx - 100) / 2, (zy - 100) / 2, (zx + 100) / 2, (zy + 100) / 2));
            CheckNotRESULT(NULL, hScreen = CreateRectRgn(0, 0, zx, zy));
            for (int x = 0; x < 6; x++)
            {
                aRect.left = goodRandomNum(zx);
                aRect.top = goodRandomNum(zy);
                aRect.right = aRect.left + goodRandomNum(zx / 6);
                aRect.bottom = aRect.top + goodRandomNum(zy / 6);
                Rect2Rgn(&aRect);
                CheckNotRESULT(NULL, hRgnTemp = CreateRectRgnIndirect(&aRect));
                CheckNotRESULT(ERROR, CombineRgn(hPattern, hPattern, hRgnTemp, RGN_OR));
                CheckNotRESULT(FALSE, DeleteObject(hRgnTemp));
            }
            CheckNotRESULT(ERROR, CombineRgn(hPattern, hScreen, hPattern, RGN_DIFF));
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            CheckNotRESULT(FALSE, FillRgn(hdc, hPattern, (HBRUSH) GetStockObject(BLACK_BRUSH)));
            // clean up
            CheckNotRESULT(FALSE, DeleteObject(hPattern));
            CheckNotRESULT(FALSE, DeleteObject(hScreen));
        }
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   EqualRgn Test
***
************************************************************************************/
//***********************************************************************************
void
EqualRgnTest(int testFunc)
{

    info(ECHO, TEXT("%s - EqualRgn Test"), funcName[testFunc]);

    setTestRectRgn();
    HRGN    hRgn1,
            destRgn0,
            destRgn1;
    int     result;
    HRGN    hRgn0;

    CheckNotRESULT(NULL, hRgn1 = CreateRectRgnIndirect(&center));
    CheckNotRESULT(NULL, destRgn0 = CreateRectRgnIndirect(&center));
    CheckNotRESULT(NULL, destRgn1 = CreateRectRgnIndirect(&center));

    // try every mode with every test
    for (int mode = 0; mode < uTests; mode++)
        for (int test = 0; test < NUMREGIONTESTS; test++)
        {
            CheckNotRESULT(NULL, hRgn0 = CreateRectRgnIndirect(&rTests[test]));
            CheckNotRESULT(ERROR, CombineRgn(destRgn0, hRgn0, hRgn1, unionMode[mode]));
            CheckNotRESULT(ERROR, CombineRgn(destRgn1, hRgn0, hRgn1, unionMode[mode]));
            CheckNotRESULT(ERROR, result = EqualRgn(destRgn0, destRgn1));
            if (result != 1)
                info(FAIL, TEXT("[%d][%d] equal region found unequal"), mode, test);
            CheckNotRESULT(FALSE, DeleteObject(hRgn0));
        }
    CheckNotRESULT(FALSE, DeleteObject(hRgn1));
    CheckNotRESULT(FALSE, DeleteObject(destRgn0));
    CheckNotRESULT(FALSE, DeleteObject(destRgn1));
}

/***********************************************************************************
***
***   FrameRgn Test
***
************************************************************************************/

//***********************************************************************************
void
FrameRgnTest(int testFunc)
{
#ifndef UNDER_CE
    info(ECHO, TEXT("%s - FrameRgn Test"), funcName[testFunc]);

    setTestRectRgn();
    TDC     hdc = myGetDC();
    HRGN    hRgn1,
            destRgn,
            hRgn0;
    HBRUSH  hBrush;
    int     result;

    CheckNotRESULT(NULL, hRgn1 = CreateRectRgnIndirect(&center));
    CheckNotRESULT(NULL, destRgn = CreateRectRgnIndirect(&center));
    CheckNotRESULT(NULL, hBrush = (HBRUSH) GetStockObject(BLACK_BRUSH));

    // try every mode with every test
    for (int mode = 0; mode < uTests; mode++)
        for (int test = 0; test < NUMREGIONTESTS; test++)
        {
            CheckNotRESULT(NULL, hRgn0 = CreateRectRgnIndirect(&rTests[test]));

            CheckNotRESULT(ERROR, CombineRgn(destRgn, hRgn0, hRgn1, unionMode[mode]));
            CheckNotRESULT(ERROR, result = FrameRgn(hdc, destRgn, hBrush, 5, 5));
            if (result != 1)
                info(FAIL, TEXT("[%d][%d] FrameRgn Failed"), mode, test);
            CheckNotRESULT(FALSE, DeleteObject(hRgn0));
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
        }
    CheckNotRESULT(FALSE, DeleteObject(hRgn1));
    CheckNotRESULT(FALSE, DeleteObject(destRgn));
    myReleaseDC(hdc);
#endif // UNDER_CE
}

/***********************************************************************************
***
***   Rgn Regression Test
***
************************************************************************************/

//***********************************************************************************
void
RgnRegressionTest(int testFunc)
{

    info(ECHO, TEXT("%s - Rgn Regression Test"), funcName[testFunc]);

    RECT    r;
    HRGN    hRgn,
            hRgnTest,
            hRgn2;
    int     result;
    LPRGNDATA lpRgnData;
    RECT   *temp;
    DWORD dwCount;

    CheckNotRESULT(NULL, hRgn = CreateRectRgn(0, 0, 1, 1));
    CheckNotRESULT(NULL, hRgnTest = CreateRectRgn(50, 20, 150, 60));
    CheckNotRESULT(NULL, hRgn2 = CreateRectRgn(0, 0, 1, 1));

    switch (testFunc)
    {
        case EGetRgnBox:
            CheckNotRESULT(ERROR, CombineRgn(hRgn, hRgnTest, hRgnTest, RGN_OR));
            CheckNotRESULT(0, GetRgnBox(hRgn, &r));

            if (r.left != 50 || r.top != 20 || r.right != 150 || r.bottom != 60)
                info(FAIL, TEXT("expected:%d,%d,%d,%d  returned:%d,%d,%d,%d"), 50, 20, 150, 60, r.left, r.top, r.right,
                     r.bottom);
            break;
        case ECombineRgn:
            CheckForRESULT(SIMPLEREGION, result = CombineRgn(hRgn2, hRgn, NULL, RGN_COPY));
            break;
        case EGetRegionData:
            lpRgnData = (LPRGNDATA) LocalAlloc(LPTR, 256);
            CheckNotRESULT(ERROR, CombineRgn(hRgn, hRgnTest, hRgnTest, RGN_OR));
            CheckNotRESULT(0, GetRegionData(hRgn, 256, lpRgnData));

            temp = (RECT *) lpRgnData->Buffer;
            if ((*temp).left != 50 || (*temp).top != 20 || (*temp).right != 150 || (*temp).bottom != 60)
                info(FAIL, TEXT("expected:%d,%d,%d,%d  returned:%d,%d,%d,%d"), 50, 20, 150, 60, (*temp).left, (*temp).top,
                     (*temp).right, (*temp).bottom);
            LocalFree(lpRgnData);
            break;
        case EOffsetRgn:
            CheckNotRESULT(ERROR, OffsetRgn(hRgnTest, 30, 40));
            CheckNotRESULT(0, dwCount = GetRegionData(hRgnTest, 0, NULL));
            lpRgnData = (LPRGNDATA) LocalAlloc(LPTR, dwCount);

            CheckNotRESULT(0, GetRegionData(hRgnTest, dwCount, lpRgnData));

            if (lpRgnData->rdh.rcBound.left != 50 + 30 || lpRgnData->rdh.rcBound.top != 20 + 40 ||
                lpRgnData->rdh.rcBound.right != 150 + 30 || lpRgnData->rdh.rcBound.bottom != 60 + 40)
                info(FAIL, TEXT("returned: %d %d %d %d expected: %d %d %d %d"), lpRgnData->rdh.rcBound.left,
                     lpRgnData->rdh.rcBound.top, lpRgnData->rdh.rcBound.right, lpRgnData->rdh.rcBound.bottom, 50 + 30, 20 + 40,
                     150 + 30, 60 + 40);
            LocalFree(lpRgnData);
            break;
    }
    CheckNotRESULT(FALSE, DeleteObject(hRgn));
    CheckNotRESULT(FALSE, DeleteObject(hRgnTest));
    CheckNotRESULT(FALSE, DeleteObject(hRgn2));
}

/***********************************************************************************
***
***   GetRgnData Diff Test
***
************************************************************************************/

//***********************************************************************************
void
GetRgnDataDiffTest(int testFunc)
{

    info(ECHO, TEXT("%s - GetRgnData Diff Test"), funcName[testFunc]);

    HRGN    hrgnClip,
            hrgnTest1a,
            hrgnTest1b,
            hrgnTest2;
    DWORD   dwCount;
    LPRGNDATA lpRgnData;

    CheckNotRESULT(NULL, hrgnClip = CreateRectRgn(0, 0, 1, 1));
    CheckNotRESULT(NULL, hrgnTest1a = CreateRectRgn(25, 100, 175, 120));
    CheckNotRESULT(NULL, hrgnTest1b = CreateRectRgn(50, 20, 150, 60));
    CheckNotRESULT(NULL, hrgnTest2 = CreateRectRgn(0, 120, 75, 200));

    CombineRgn(hrgnTest1a, hrgnTest1a, hrgnTest1b, RGN_OR);
    CombineRgn(hrgnClip, hrgnTest1a, hrgnTest2, RGN_DIFF);

    CheckNotRESULT(0, dwCount = GetRegionData(hrgnClip, 0, NULL));
    lpRgnData = (LPRGNDATA) LocalAlloc(LPTR, dwCount);

    CheckNotRESULT(0, GetRegionData(hrgnClip, dwCount, lpRgnData));

    if (dwCount != 64 ||
        lpRgnData->rdh.nRgnSize != 32 ||
        lpRgnData->rdh.dwSize != 32 || lpRgnData->rdh.iType != 1 || lpRgnData->rdh.nCount != 2)
        info(FAIL, TEXT("expected: %d %d %d %d %d returned: %d %d %d %d %d"), dwCount, lpRgnData->rdh.nRgnSize,
             lpRgnData->rdh.dwSize, lpRgnData->rdh.iType, lpRgnData->rdh.nCount, 64, 32, 32, 1, 2);

    CheckNotRESULT(FALSE, DeleteObject(hrgnClip));
    CheckNotRESULT(FALSE, DeleteObject(hrgnTest1a));
    CheckNotRESULT(FALSE, DeleteObject(hrgnTest1b));
    CheckNotRESULT(FALSE, DeleteObject(hrgnTest2));
    LocalFree(lpRgnData);
}

/***********************************************************************************
***
***   GetRgnData Diff Test2
***
************************************************************************************/

//***********************************************************************************
void
GetRgnDataDiffTest2(int testFunc)
{
    info(ECHO, TEXT("%s - GetRgnData Diff Test2"), funcName[testFunc]);

    HRGN    hrgn;

    DWORD   dwCount;
    LPRGNDATA lpRgnData;

    CheckNotRESULT(NULL, hrgn = CreateRectRgn(10, 20, 30, 40));
    CheckNotRESULT(0, dwCount = GetRegionData(hrgn, 0, NULL));
    lpRgnData = (LPRGNDATA) LocalAlloc(LPTR, dwCount);

    CheckNotRESULT(ERROR, GetRegionData(hrgn, dwCount, lpRgnData));

    if (lpRgnData->rdh.nRgnSize != 16)
        info(FAIL, TEXT("expected: %d returned: %d"), 16, lpRgnData->rdh.nRgnSize);

    CheckNotRESULT(FALSE, DeleteObject(hrgn));
    LocalFree(lpRgnData);
}

void
RepeatCreateAndDeleteRgn(int testFunc)
{

    info(ECHO, TEXT("%s - repeat create and delete rgn: 10 times"), funcName[testFunc]);
    HRGN    hRgn;
    LPRGNDATA lpRgnData = (LPRGNDATA) LocalAlloc(LPTR, 256);

    for (int i = 0; i < 10000; i += 1000)
    {
        CheckNotRESULT(NULL, hRgn = CreateRectRgn(i, i, i, i));
        CheckNotRESULT(ERROR, GetRegionData(hRgn, 256, lpRgnData));
        if (lpRgnData->rdh.nCount != 0)
            info(FAIL, TEXT("GetRegionData expecting nCount:0 returned:%d"), lpRgnData->rdh.nCount);
        CheckNotRESULT(FALSE, DeleteObject(hRgn));
    }
    LocalFree(lpRgnData);
}

//***********************************************************************************
void
GetRgnDataTest(int testFunc)
{
    info(ECHO, TEXT("%s - Combine Region"), funcName[testFunc]);

    RECT    center = { 50, 50, 150, 150 }, rTest0 =
    {
    75, 25, 125, 75};

    HRGN    hRgn0,
            hRgn1,
            destRgn;
    DWORD   dwCount;
    LPRGNDATA lpRgnData;

    CheckNotRESULT(NULL, hRgn0 = CreateRectRgnIndirect(&rTest0));
    CheckNotRESULT(NULL, hRgn1 = CreateRectRgnIndirect(&center));
    CheckNotRESULT(NULL, destRgn = CreateRectRgnIndirect(&center));

    CheckNotRESULT(ERROR, CombineRgn(destRgn, hRgn0, hRgn1, RGN_OR));

    CheckNotRESULT(0, dwCount = GetRegionData(destRgn, 0, NULL));
    lpRgnData = (LPRGNDATA) LocalAlloc(LPTR, dwCount);

    CheckNotRESULT(0, GetRegionData(destRgn, dwCount, lpRgnData));
    if (lpRgnData->rdh.nCount != 2)
        info(DETAIL, TEXT("expected:2 returned:%d"), lpRgnData->rdh.nCount);

    LocalFree(lpRgnData);

    CheckNotRESULT(FALSE, DeleteObject(hRgn0));
    CheckNotRESULT(FALSE, DeleteObject(hRgn1));
    CheckNotRESULT(FALSE, DeleteObject(destRgn));
}

void
SimpleGetRgnBox(int testFunc)
{

    info(ECHO, TEXT("%s - rgn rect=[100,100,100,100]"), funcName[testFunc]);
    RECT    rc = { 100, 100, 100, 100 }, rcOut = { 100, 100, 100, 100};
    HRGN    hRgn;

    CheckNotRESULT(NULL, hRgn = CreateRectRgnIndirect(&rc));

    CheckForRESULT(NULLREGION, GetRgnBox(hRgn, &rcOut));

    if (rcOut.left + rcOut.top + rcOut.right + rcOut.bottom != 0)
        info(FAIL, TEXT("GetRgnBox expected NULL Rect result:%d %d %d %d"), rcOut.left, rcOut.top, rcOut.right, rcOut.bottom);

    CheckNotRESULT(FALSE, DeleteObject(hRgn));
}

// Using random rectangles with a width or height of 0, this verifies that CreateRectRgn and CreateRectRgnIndirect
// creates a NULL region (top = bottom = left = right = 0).
void
CreateNullRegionTest(int testFunc)
{
    info(ECHO, TEXT("%s - CreateNullRegionTest"), funcName[testFunc]);

    HRGN hrgn = NULL;
    RECT rc, rectExpected = {0, 0, 0, 0}, rcResult;

    // generate a random rectangle
    wellFormedRect(&rc);
    // give it a height of 0
    rc.top = rc.bottom;

    switch(testFunc)
    {
        case ECreateRectRgnIndirect:
            CheckNotRESULT(NULL, hrgn = CreateRectRgnIndirect(&rc));
            break;
        case ECreateRectRgn:
            CheckNotRESULT(NULL, hrgn = CreateRectRgn(rc.left, rc.top, rc.right, rc.bottom));
            break;
    }

    if(hrgn != NULL)
    {
        // get the resulting region
        CheckForRESULT(NULLREGION, GetRgnBox(hrgn, &rcResult));

        // if it's not a NULL region as expected it's a failure.
        if(!isEqualRect(&rcResult, &rectExpected))
            info(FAIL, TEXT("with rect (%d, %d, %d, %d) expect NULL region of (0,0,0,0), got (%d, %d, %d, %d)"), 
                rc.top,rc.left,rc.bottom,rc.right,
                rcResult.top,rcResult.left,rcResult.bottom,rcResult.right);

        CheckNotRESULT(FALSE, DeleteObject(hrgn));
        hrgn = NULL;
    }

    // generate a random rectangle
    wellFormedRect(&rc);
    // give it a width of 0.
    rc.left = rc.right;

    switch(testFunc)
    {
        case ECreateRectRgnIndirect:
            CheckNotRESULT(NULL, hrgn = CreateRectRgnIndirect(&rc));
            break;
        case ECreateRectRgn:
            CheckNotRESULT(NULL, hrgn = CreateRectRgn(rc.left, rc.top, rc.right, rc.bottom));
            break;
    }

    if(hrgn != NULL)
    {
        // get the resulting region
        CheckForRESULT(NULLREGION, GetRgnBox(hrgn, &rcResult));

        // if it's not a NULL region as expected it's a failure.
        if(!isEqualRect(&rcResult, &rectExpected))
            info(FAIL, TEXT("with rect (%d, %d, %d, %d) expect NULL region of (0,0,0,0), got (%d, %d, %d, %d)"), 
                rc.top,rc.left,rc.bottom,rc.right,
                rcResult.top,rcResult.left,rcResult.bottom,rcResult.right);

        CheckNotRESULT(FALSE, DeleteObject(hrgn));
        hrgn = NULL;
    }

}

// take a NULL region and try moving it around using OffsetRgn
void
OffsetRgnNULLRegionTest(int testFunc)
{
    info(ECHO, TEXT("%s - OffsetRgnNULLRegionTest"), funcName[testFunc]);

    HRGN hrgn;
    RECT rc = {0, 0, 0, 0}, rcResult = {0, 0, 0, 0};
    POINT offsetRgns[] = { {0, 0},
                                        { -1, 0},
                                        {0, -1},
                                        { -1, -1},
                                        { 1, 0},
                                        {0, 1},
                                        { 1, 1} };

    for(int i=0; i < countof(offsetRgns); i++)
    {
        // create a NULL region
        CheckNotRESULT(NULL, hrgn = CreateRectRgnIndirect(&rc));

        if(hrgn != NULL)
        {
            // move the region
            CheckForRESULT(NULLREGION, OffsetRgn(hrgn, offsetRgns[i].x, offsetRgns[i].y));

            // make sure the region still has top=bottom=left=right=0
            CheckForRESULT(NULLREGION, GetRgnBox(hrgn, &rcResult));
               
            if(!isEqualRect(&rc, &rcResult))
                info(FAIL, TEXT("expect NULL region of (0,0,0,0), got (%d, %d, %d, %d)"), rcResult.top, rcResult.left, rcResult.bottom, rcResult.right);

            CheckNotRESULT(FALSE, DeleteObject(hrgn));
        }
    }
}

// This test verifies that SetRectRgn creates the proper NULL region given a width or height of 0.
void
SetRectRgnNULLRgnTest(int testFunc)
{
    info(ECHO, TEXT("%s - SetRectRgnNULLRgnTest"), funcName[testFunc]);

    HRGN hrgn;
    RECT rcZero = {0, 0, 0, 0}, rc, rcResult;

    // create a random region
    wellFormedRect(&rc);
    CheckNotRESULT(NULL, hrgn = CreateRectRgnIndirect(&rc));

    // move the region to be a NULL region
    CheckNotRESULT(FALSE, SetRectRgn(hrgn, 0, 0, 0, 0));

    // verify it's a NULL region
    CheckForRESULT(NULLREGION, GetRgnBox(hrgn, &rcResult));
    
    if(!isEqualRect(&rcZero, &rcResult))
        info(FAIL, TEXT("expect NULL region of (0,0,0,0), got (%d,%d,%d,%d)"), rcResult.top, rcResult.left, rcResult.bottom, rcResult.right);

    CheckNotRESULT(FALSE, DeleteObject(hrgn));

    // create a new random region
    wellFormedRect(&rc);
    CheckNotRESULT(NULL, hrgn = CreateRectRgnIndirect(&rc));

    // create a random rectangle for a region
    wellFormedRect(&rcResult);
    // make it a NULL region rectangle
    rcResult.top = rcResult.bottom;

    // move the random region to the new random NULL region    
    CheckNotRESULT(FALSE, SetRectRgn(hrgn, rcResult.left, rcResult.top, rcResult.right, rcResult.bottom));

    // verify it's a NULL region
    CheckForRESULT(NULLREGION, GetRgnBox(hrgn, &rcResult));

    if(!isEqualRect(&rcZero, &rcResult))
        info(FAIL, TEXT("expect NULL region of (0,0,0,0), got (%d,%d,%d,%d)"), rcResult.top, rcResult.left, rcResult.bottom, rcResult.right);

    CheckNotRESULT(FALSE, DeleteObject(hrgn));

    // create a new random region
    wellFormedRect(&rc);
    CheckNotRESULT(NULL, hrgn = CreateRectRgnIndirect(&rc));

    // create a new random rectangle
    wellFormedRect(&rcResult);
    // make the new rectangle a NULL region rectangle
    rcResult.left = rcResult.right;

    // set the new region to the new null region rectangle
    CheckNotRESULT(FALSE, SetRectRgn(hrgn, rcResult.left, rcResult.top, rcResult.right, rcResult.bottom));

    // verify it's a NULL region.
    CheckForRESULT(NULLREGION, GetRgnBox(hrgn, &rcResult));

    if(!isEqualRect(&rcZero, &rcResult))
        info(FAIL, TEXT("expect NULL region of (0,0,0,0), got (%d,%d,%d,%d)"), rcResult.top, rcResult.left, rcResult.bottom, rcResult.right);

    CheckNotRESULT(FALSE, DeleteObject(hrgn));
}

/***********************************************************************************
***
***   APIs
***
************************************************************************************/

//***********************************************************************************
TESTPROCAPI PtInRegion_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Region(EPtInRegion);
    inRegionRandom(EPtInRegion);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI RectInRegion_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Region(ERectInRegion);
    inRegionRandom(ERectInRegion);
    passEmptyRect(ERectInRegion, funcName[ERectInRegion]);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI CreateRectRgn_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    CreateRectRgnRandom(ECreateRectRgn);
    CheckingNullRegion(ECreateRectRgn);
    CreateNullRegionTest(ECreateRectRgn);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI CreateRectRgnIndirect_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    CreateRectRgnRandom(ECreateRectRgnIndirect);
    CheckingNullRegion(ECreateRectRgnIndirect);
    CreateNullRegionTest(ECreateRectRgnIndirect);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI SetRectRgn_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Region(ESetRectRgn);
    CreateRectRgnRandom(ESetRectRgn);
    SetRectRgnNULLRgnTest(ESetRectRgn);
    
    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI OffsetRgn_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Region(EOffsetRgn);
    CreateRectRgnRandom(EOffsetRgn);
    checkReturnValues(EOffsetRgn);
    RgnRegressionTest(EOffsetRgn);
    OffsetRgnNULLRegionTest(EOffsetRgn);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetRegionData_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Region(EGetRegionData);
    CreateRectRgnRandom(EGetRegionData);
    RgnRegressionTest(EGetRegionData);
    RepeatCreateAndDeleteRgn(EGetRegionData);
    GetRgnDataDiffTest(EGetRegionData);
    GetRgnDataDiffTest2(EGetRegionData);
    GetRgnDataTest(EGetRegionData);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetRgnBox_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Region(EGetRgnBox);
    CreateRectRgnRandom(EGetRgnBox);
    checkReturnValues(EGetRgnBox);
    RgnRegressionTest(EGetRgnBox);
    SimpleGetRgnBox(EGetRgnBox);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI CombineRgn_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Region(ECombineRgn);
    RgnRegressionTest(ECombineRgn);
    checkReturnValues(ECombineRgn);
    StrangeCombineRgn(ECombineRgn);
    combinePair(ECombineRgn);
    CombineRandComplexTest(ECombineRgn);
    CombineRandComplexTest2(ECombineRgn);
    CombineOrComplexTest(ECombineRgn);
    CombineOrComplexTest2(ECombineRgn);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI FillRgn_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Region(EFillRgn);
    FillPair(EFillRgn, 0, 0);
    FillPair(EFillRgn, 1, 0);
    FillPair(EFillRgn, 1, 1);
    FillCheckRgn(EFillRgn);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI EqualRgn_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Region(EEqualRgn);
    EqualRgnTest(EEqualRgn);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI ExtCreateRegion_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    // None

    // Depth
    // None

    return getCode();
}

/***********************************************************************************
***
***   Incomplete - and not needed for UNDER_CE
***
************************************************************************************/

//***********************************************************************************
TESTPROCAPI FrameRgn_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;
    
#ifdef UNDER_CE
    info(ECHO, TEXT("Currently not implented in Windows CE, placeholder for future test development."));
#else
    // Breadth
    passNull2Region(EFrameRgn);
    FrameRgnTest(EFrameRgn);

    // Depth
    // None
#endif

    return getCode();
}
