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

    CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
    CheckNotRESULT(FALSE, PatBlt(hdc, 20, 20, 3, 3, BLACKNESS));
    CheckNotRESULT(FALSE, PatBlt(hdc, 20 + zx / 2, 20, 3, 3, BLACKNESS));

    info(DETAIL, TEXT("Pixel Depth: %d x:%d y:%d"), myGetBitDepth(), zx, zy);
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
void
Thrash_SimpleCreateSolidBrushTest(void)
{
    info(ECHO, TEXT("Thrash_SimpleCreateSolidBrushTest"));

    HBRUSH  hBrush;
    COLORREF color = RGB(255, 0, 0);
    int     counter = 0;

    while (counter++ < cycles)
    {
        CheckNotRESULT(NULL, hBrush = CreateSolidBrush(color));
        CheckNotRESULT(FALSE, DeleteObject(hBrush));
    }
}

//***********************************************************************************
void
Thrash_SimpleCreatePatternBrushTest(void)
{
    info(ECHO, TEXT("Thrash_SimpleCreatePatternBrushTest"));

    TDC     hdc = myGetDC();
    HBRUSH  hBrush;
    TBITMAP hBmp;
    int     counter = 0;

    CheckNotRESULT(NULL, hBmp = CreateCompatibleBitmap(hdc, 8, 8));


    while (counter++ < cycles)
    {
        CheckNotRESULT(NULL, hBrush = CreatePatternBrush(hBmp));
        CheckNotRESULT(FALSE, DeleteObject(hBrush));
    }

    CheckNotRESULT(FALSE, DeleteObject(hBmp));
    myReleaseDC(hdc);
}

//***********************************************************************************
void
Thrash_checkGetSetBrushOrgEx(void)
{
    info(ECHO, TEXT("Thrash_checkGetSetBrushOrgEx"));
// GetBrushOrgEx unsupported on CE.
#ifndef UNDER_CE
    TDC     hdc = myGetDC();
    int     counter = 0;
    BOOL    result = TRUE;
    POINT   prev0 = { 50, 50 };

    while (counter++ < cycles)
    {
        CheckNotRESULT(0, GetBrushOrgEx(hdc, &prev0));
        CheckNotRESULT(0, SetBrushOrgEx(hdc, 10, 10, &prev0));
    }
    myReleaseDC(hdc);
#endif // UNDER_CE
}

/***********************************************************************************
***
***   Clip Tests
***
************************************************************************************/

void
Thrash_passNull2ClipRegion(void)
{
    info(ECHO, TEXT("Thrash_passNull2ClipRegion"));

    TDC     hdc = myGetDC();
    HRGN    tempRgn;
    RECT    test = { 0, 0, 100, 100 };
    int counter = 0;

    while(counter++ < cycles)
    {
        CheckNotRESULT(NULL, tempRgn = CreateRectRgn(0, 0, 20, 20));
        CheckNotRESULT(ERROR, ExcludeClipRect(hdc, test.left, test.top, test.right, test.bottom));
        CheckNotRESULT(ERROR, GetClipBox(hdc, &test));
        CheckNotRESULT(ERROR, IntersectClipRect(hdc, test.left, test.top, test.right, test.bottom));
        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(-1, GetClipRgn(hdc, NULL));
        CheckForLastError(ERROR_INVALID_HANDLE);
        CheckNotRESULT(-1, GetClipRgn(hdc, tempRgn));
        CheckNotRESULT(ERROR, SelectClipRgn(hdc, tempRgn));
        CheckNotRESULT(ERROR, SelectClipRgn(hdc, NULL));
        CheckNotRESULT(FALSE, DeleteObject(tempRgn));
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Device Attribute Tests
***
************************************************************************************/

//***********************************************************************************
void
Thrash_checkGetSetModes(void)
{
    info(ECHO, TEXT("Thrash_checkGetSetModes"));

    TDC     hdc = myGetDC();
    int     counter = 0;

    while (counter++ < cycles)
    {
        CheckNotRESULT(0, SetBkMode(hdc, OPAQUE));
        CheckNotRESULT(0, GetBkMode(hdc));
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Device Context Tests
***
************************************************************************************/
void
Thrash_StressPairs(void)
{
    info(ECHO, TEXT("Thrash_StressPairs"));

    TDC     hdc;
    int     counter = 0;
    BOOL    bOldVerify = SetSurfVerify(FALSE);

    while(counter++ < cycles)
    {
        CheckNotRESULT(NULL, hdc = CreateCompatibleDC((TDC) NULL));
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
        CheckNotRESULT(FALSE, DeleteDC(hdc));
    }

    counter = 0;
    while(counter++ < cycles)
    {
        hdc = myGetDC();
        myReleaseDC(hdc);
    }
    
    SetSurfVerify(bOldVerify);
}

/***********************************************************************************
***
***   Device Object Tests
***
************************************************************************************/
void
Thrash_GetStockObjectTest(void)
{
    info(ECHO, TEXT("Thrash_GetStockObjectTest"));

    for (int i = 0; i < numStockObj; i++)
    {
        CheckNotRESULT(NULL, GetStockObject(myStockObj[i]));
    }
}

/***********************************************************************************
***
***   Draw Tests
***
************************************************************************************/
void
Thrash_blastRect(void)
{
    info(ECHO, TEXT("Thrash_blastRect"));

    TDC     hdc = myGetDC();
    COLORREF color = RGB(255, 0, 0);
    HBRUSH  hBrush,
            lastBrush;
    int counter = 0;

    while(counter++ < cycles)
    {
        CheckNotRESULT(NULL, hBrush = CreateSolidBrush(color));
        CheckNotRESULT(NULL, lastBrush = (HBRUSH) SelectObject(hdc, hBrush));

        CheckNotRESULT(FALSE, PatBlt(hdc, 20, 20, 60, 60, PATCOPY));

        CheckNotRESULT(NULL, SelectObject(hdc, lastBrush));
        CheckNotRESULT(FALSE, DeleteObject(hBrush));
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Region Tests
***
************************************************************************************/
void
Thrash_CreateRectRgnRandom(void)
{
    info(ECHO, TEXT("Thrash_CreateRectRgnRandom"));

    HRGN    hRgn;
    RECT    inRect = { 0, 0, 100, 100 };
    int counter = 0;

    while(counter++ < cycles)
    {
        CheckNotRESULT(NULL, hRgn = CreateRectRgnIndirect(&inRect));
        CheckNotRESULT(FALSE, DeleteObject(hRgn));
        CheckNotRESULT(NULL, hRgn = CreateRectRgn(inRect.left, inRect.top, inRect.right, inRect.bottom));
        CheckNotRESULT(FALSE, DeleteObject(hRgn));
    }

}

/***********************************************************************************
***
***   Text Tests
***
************************************************************************************/
void
Thrash_SpeedExtTestOut(void)
{
    info(ECHO, TEXT("Thrash_SpeedExtTestOut"));

    TDC     hdc = myGetDC();
    RECT    r = { 0, 0, 100, 50 };
    int counter = 0;

    while(counter++ < cycles)
        CheckNotRESULT(0, ExtTextOut(hdc, 40, 40, ETO_OPAQUE, &r, TEXT("Hello World"), 11, NULL));

    myReleaseDC(hdc);
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
    int expected = 18;
    
    setupScreen(hdc, ECheckAllWhite);
    result = CheckAllWhite(hdc, FALSE, expected);
    if (result != expected)
    {
        dw = GetLastError();
        if (dw == ERROR_NOT_ENOUGH_MEMORY || dw == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("Expected %d pixel are black, found: %d: Failed due to Out of Memory"), expected, result);
        else
            info(FAIL, TEXT("Expected %d pixel are black, found: %d: GLE=%ld"), expected, result, dw);
    }
    else
        info(DETAIL, TEXT("Expected %d pixel are black, found %d"), expected, result);

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

    Thrash_SpeedExtTestOut();
    Thrash_CreateRectRgnRandom();
    Thrash_blastRect();
    Thrash_GetStockObjectTest();
    Thrash_StressPairs();
    Thrash_checkGetSetModes();
    Thrash_passNull2ClipRegion();
    Thrash_checkGetSetBrushOrgEx();
    Thrash_SimpleCreatePatternBrushTest();
    Thrash_SimpleCreateSolidBrushTest();

    return getCode();
}
