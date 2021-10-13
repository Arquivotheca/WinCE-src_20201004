//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "global.h"
#include "fontdata.h"

/***********************************************************************************
***
***   Test Testing Support Functions
***
************************************************************************************/
//***********************************************************************************
void
setupScreen(TDC hdc, int testFunc)
{

    info(ECHO, TEXT("%s - Test Test"), funcName[testFunc]);

    PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
    PatBlt(hdc, 20, 20, 3, 3, BLACKNESS);
    PatBlt(hdc, 20 + zx / 2, 20, 3, 3, BLACKNESS);

    info(DETAIL, TEXT("Pixel Depth: %d x:%d y:%d"), GetDeviceCaps(hdc, BITSPIXEL), zx, zy);
    info(DETAIL, TEXT("Drawing L:(%d %d)x(%d %d), R:(%d %d)x(%d %d)"), 20, 20, 21, 21, 20 + zx / 2, 20, 20 + zx / 2 + 1, 21);
}

/***********************************************************************************
************************************************************************************
***
***   Thrash Functions
***
************************************************************************************
************************************************************************************/

/*
   1) Prototype of BOOL Func(void);  Return TRUE is things worked, FALSE if they didn't.
   2) Quick to run.  1 second and under would be nice.  Under 5 seconds will be required (since we want to be able to clean up quickly when needed)
   3) Does lots of memory and state stuff.  Do stuff that creates Objects, manipulates globals, parties on shared resources, etc.  Other areas probably aren't worth much since they should be covered by your functional tests.
   4) Is THREAD SAFE! No modified globals, no static variables, no illegal access to exclusive global objects.  If you're not sure what all this entails please ask someone.
   5) CLEAN, CLEAN, CLEAN.  We can't have any memory leaks or other garbage left around because of our code.  If we do, our code will crash/leak before the system does.

   remember #includes ...
 */

const int cycles = 10;

/***********************************************************************************
***
***   Brush Tests
***
************************************************************************************/

//***********************************************************************************
BOOL Thrash_SimpleCreateSolidBrushTest(void)
{

    HBRUSH  hBrush;
    COLORREF color = RGB(255, 0, 0);
    int     counter = 0;

    while (counter++ < cycles)
    {
        hBrush = CreateSolidBrush(color);
        if (!hBrush)
            return FALSE;
        if (!DeleteObject(hBrush))
            return FALSE;
    }
    return TRUE;
}

//***********************************************************************************
BOOL Thrash_SimpleCreatePatternBrushTest(void)
{
    TDC     hdc = myGetDC();
    HBRUSH  hBrush;
    TBITMAP hBmp = CreateCompatibleBitmap(hdc, 8, 8);
    int     counter = 0;

    if (!hBmp)
        return FALSE;

    if (!hdc)
        return FALSE;

    while (counter++ < cycles)
    {
        hBrush = CreatePatternBrush(hBmp);
        if (!hBrush)
            return FALSE;
        if (!DeleteObject(hBrush))
            return FALSE;
    }
    if (!DeleteObject(hBmp))
        return FALSE;
    if (!myReleaseDC(hdc))
        return FALSE;
    return TRUE;
}

//***********************************************************************************
BOOL Thrash_checkGetSetBrushOrgEx(void)
{
#ifndef UNDER_CE
    TDC     hdc = myGetDC();
    int     counter = 0;
    BOOL    result = TRUE;
    POINT   prev0 = { 50, 50 };

    if (!hdc)
        return FALSE;

    while (counter++ < cycles)
    {
        GetBrushOrgEx(hdc, &prev0);
        SetBrushOrgEx(hdc, 10, 10, &prev0);
        if (!result)
            return FALSE;
    }
    if (!myReleaseDC(hdc))
        return FALSE;
#endif // UNDER_CE
    return TRUE;
}

/***********************************************************************************
***
***   Clip Tests
***
************************************************************************************/

BOOL Thrash_passNull2ClipRegion(void)
{

    TDC     hdc = myGetDC();
    HRGN    tempRgn = CreateRectRgn(0, 0, 20, 20);
    RECT    test = { 0, 0, 100, 100 };

    if (!hdc)
        return FALSE;

    ExcludeClipRect(hdc, test.left, test.top, test.right, test.bottom);

    GetClipBox(hdc, &test);
    IntersectClipRect(hdc, test.left, test.top, test.right, test.bottom);

    GetClipRgn(hdc, NULL);
    GetClipRgn(hdc, tempRgn);
    SelectClipRgn(hdc, tempRgn);
    SelectClipRgn(hdc, NULL);

    DeleteObject(tempRgn);

    if (!myReleaseDC(hdc))
        return FALSE;
    return TRUE;
}

/***********************************************************************************
***
***   Device Attribute Tests
***
************************************************************************************/

//***********************************************************************************
BOOL Thrash_checkGetSetModes(void)
{

    TDC     hdc = myGetDC();
    int     counter = 0;

    if (!hdc)
        return FALSE;

    while (counter++ < cycles)
    {
        SetBkMode(hdc, OPAQUE);
        GetBkMode(hdc);
    }

    if (!myReleaseDC(hdc))
        return FALSE;

    return TRUE;
}

/***********************************************************************************
***
***   Device Context Tests
***
************************************************************************************/

BOOL Thrash_StressPairs(void)
{

    TDC     hdc;
    int     i;
    BOOL    bOldVerify = SetSurfVerify(FALSE),
            bRet = TRUE;

    for (i = 0; i < cycles && bRet; i++)
    {
        hdc = CreateCompatibleDC((TDC) NULL);
        PatBlt(hdc, 0, 0, zx, zy, WHITENESS);

        if (!hdc)
            bRet = FALSE;
        if (!DeleteDC(hdc))
            bRet = FALSE;
    }

    for (i = 0; i < cycles && bRet; i++)
    {
        hdc = myGetDC();
        if (!hdc)
            bRet = FALSE;
        if (!myReleaseDC(hdc))
            bRet = FALSE;
    }
    
    SetSurfVerify(bOldVerify);
        
    return bRet;
}

/***********************************************************************************
***
***   Device Object Tests
***
************************************************************************************/
BOOL Thrash_GetStockObjectTest(void)
{

    for (int i = 0; i < numStockObj; i++)
    {
        if (!GetStockObject(myStockObj[i]))
            return FALSE;
    }
    return TRUE;
}

/***********************************************************************************
***
***   Draw Tests
***
************************************************************************************/

BOOL Thrash_blastRect(void)
{

    TDC     hdc = myGetDC();
    COLORREF color = RGB(255, 0, 0);
    HBRUSH  hBrush = CreateSolidBrush(color),
            lastBrush = (HBRUSH) SelectObject(hdc, hBrush);

    if (!hdc)
        return FALSE;

    PatBlt(hdc, 20, 20, 60, 60, PATCOPY);

    SelectObject(hdc, lastBrush);
    DeleteObject(hBrush);

    if (!myReleaseDC(hdc))
        return FALSE;

    return TRUE;
}

/***********************************************************************************
***
***   Region Tests
***
************************************************************************************/

BOOL Thrash_CreateRectRgnRandom(void)
{

    HRGN    hRgn;
    RECT    inRect = { 0, 0, 100, 100 };

    hRgn = CreateRectRgnIndirect(&inRect);
    if (!hRgn)
        return FALSE;

    if (!DeleteObject(hRgn))
        return FALSE;

    hRgn = CreateRectRgn(inRect.left, inRect.top, inRect.right, inRect.bottom);
    if (!hRgn)
        return FALSE;

    if (!DeleteObject(hRgn))
        return FALSE;

    return TRUE;
}

/***********************************************************************************
***
***   Text Tests
***
************************************************************************************/

BOOL Thrash_SpeedExtTestOut(void)
{

    TDC     hdc = myGetDC();
    RECT    r = { 0, 0, 100, 50 };

    if (!hdc)
        return FALSE;

    for (int i = 0; i < 500; i++)
    {
        if (!ExtTextOut(hdc, 40, 40, ETO_OPAQUE, &r, TEXT("Hello World"), 11, NULL))
        {
            info(FAIL, TEXT("i=%d: ExtTextOut() failed: err=%ld"), GetLastError());
        }
    }

    if (!myReleaseDC(hdc))
        return FALSE;

    return TRUE;
}

/***********************************************************************************
***
***   Test Testing Functions
***
************************************************************************************/

//***********************************************************************************
TESTPROCAPI CheckAllWhite_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    TDC     hdc = myGetDC();
    int     result;
    DWORD   dw;

    setupScreen(hdc, ECheckAllWhite);
    result = CheckAllWhite(hdc, 1, 1, 0, 0);
    if (result != 18)
    {
        dw = GetLastError();
        if (dw == ERROR_NOT_ENOUGH_MEMORY || dw == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("Expected %d pixel are black, found: %d: Failed due to Out of Memory"), 18, result);
        else
            info(FAIL, TEXT("Expected %d pixel are black, found: %d: GLE=%ld"), 18, result, dw);
    }
    else
        info(DETAIL, TEXT("Expected %d pixel are black, found %d"), 18, result);

    myReleaseDC(hdc);
    return getCode();
}

//***********************************************************************************
TESTPROCAPI CheckScreenHalves_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    TDC     hdc = myGetDC();

    setupScreen(hdc, ECheckScreenHalves);

    CheckScreenHalves(hdc);
    myReleaseDC(hdc);
    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetReleaseDC_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    info(ECHO, TEXT("%s - Test Test"), funcName[EGetReleaseDC]);

    TDC     hdc = myGetDC();

    myReleaseDC(hdc);
    return getCode();
}

//***********************************************************************************
TESTPROCAPI Thrash_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    info(ECHO, TEXT("%s - Test Test"), funcName[EThrash]);

    info(ECHO, TEXT("Thrash_SpeedExtTestOut"));
    passCheck(Thrash_SpeedExtTestOut(), 1, TEXT("Thrash_SpeedExtTestOut"));
    info(ECHO, TEXT("Thrash_CreateRectRgnRandom"));
    passCheck(Thrash_CreateRectRgnRandom(), 1, TEXT("Thrash_CreateRectRgnRandom"));
    info(ECHO, TEXT("Thrash_blastRect"));
    passCheck(Thrash_blastRect(), 1, TEXT("Thrash_blastRect"));
    info(ECHO, TEXT("Thrash_GetStockObjectTest"));
    passCheck(Thrash_GetStockObjectTest(), 1, TEXT("Thrash_GetStockObjectTest"));
    info(ECHO, TEXT("Thrash_StressPairs"));
    passCheck(Thrash_StressPairs(), 1, TEXT("Thrash_StressPairs"));
    info(ECHO, TEXT("Thrash_checkGetSetModes"));
    passCheck(Thrash_checkGetSetModes(), 1, TEXT("Thrash_checkGetSetModes"));
    info(ECHO, TEXT("Thrash_passNull2ClipRegion"));
    passCheck(Thrash_passNull2ClipRegion(), 1, TEXT("Thrash_passNull2ClipRegion"));
    info(ECHO, TEXT("Thrash_checkGetSetBrushOrgEx"));
    passCheck(Thrash_checkGetSetBrushOrgEx(), 1, TEXT("Thrash_checkGetSetBrushOrgEx"));
    info(ECHO, TEXT("Thrash_SimpleCreatePatternBrushTest"));
    passCheck(Thrash_SimpleCreatePatternBrushTest(), 1, TEXT("Thrash_SimpleCreatePatternBrushTest"));
    info(ECHO, TEXT("Thrash_SimpleCreateSolidBrushTest"));
    passCheck(Thrash_SimpleCreateSolidBrushTest(), 1, TEXT("Thrash_SimpleCreateSolidBrushTest"));

    return getCode();
}
