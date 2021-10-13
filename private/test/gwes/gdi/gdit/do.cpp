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

   do.cpp

Abstract:

   Gdi Tests: Device Object APIs

***************************************************************************/

#include "global.h"

enum
{
    oHDC = 0,
    omemHDC,
    oHRGN,
    oHBRUSH,
    oHBITMAP,
    oHFONT,
    oHPALETTE,
    oHGDIOBJ,
    oNULLOBJ,
};

static TCHAR *objName[] = {
    TEXT("HDC"),
    TEXT("memHDC"),
    TEXT("HRGN"),
    TEXT("HBRUSH"),
    TEXT("HBITMAP"),
    TEXT("HFONT"),
    TEXT("HPALETTE"),
    TEXT("HGDIOBJ"),
    TEXT("NULLOBJ"),
};

/***********************************************************************************
***
***   Support Functions
***
************************************************************************************/
// all of the supported objects
static TDC g_myHdc;
static TDC g_myMemDC;
static HRGN g_hRgn, g_stockRgn;
static HBRUSH g_hBrush, g_stockBrush;
static TBITMAP g_hBmp, g_stockBmp;
static HFONT g_hFont, g_stockFont;
static HGDIOBJ g_hNullThing = NULL;

//**********************************************************************************
void
initObjects(void)
{
    if (gSanityChecks)
        info(DETAIL, TEXT("Init Objects"));

    // create all supported objects
    g_myHdc = myGetDC();

    CheckNotRESULT(NULL, g_myMemDC = CreateCompatibleDC(g_myHdc));
    CheckNotRESULT(NULL, g_hRgn    = CreateRectRgn(0, 0, 50, 50));
    CheckNotRESULT(NULL, g_hBrush  = CreateSolidBrush(RGB(255, 0, 0)));

    BYTE   *lpBits = NULL;

    CheckNotRESULT(NULL, g_hBmp = myCreateDIBSection(g_myHdc, (VOID **) & lpBits, myGetBitDepth(), 200, 200, DIB_RGB_COLORS, NULL, FALSE));

    LOGFONT myFont;

    myFont.lfHeight = myFont.lfWidth = myFont.lfEscapement = myFont.lfOrientation = myFont.lfWeight = 10;
    myFont.lfItalic = myFont.lfUnderline = myFont.lfStrikeOut = 0;
    myFont.lfCharSet = ANSI_CHARSET;
    myFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
    myFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    myFont.lfQuality = DEFAULT_QUALITY;
    myFont.lfPitchAndFamily = DEFAULT_PITCH;
    myFont.lfFaceName[0] = NULL;

    CheckNotRESULT(NULL, g_hFont = CreateFontIndirect(&myFont));
    // get stock objects
    CheckNotRESULT(NULL, g_stockRgn = (HRGN) SelectObject(g_myHdc, g_hRgn));
    CheckNotRESULT(NULL, g_stockBrush = (HBRUSH) SelectObject(g_myHdc, g_hBrush));
    // This selection should fail on a primary DC.
    if(isMemDC())
    {
        CheckNotRESULT(NULL, g_stockBmp = (TBITMAP) SelectObject(g_myHdc, g_hBmp));
    }
    else
    {
        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(NULL, g_stockBmp = (TBITMAP) SelectObject(g_myHdc, g_hBmp));
        CheckForLastError(ERROR_INVALID_PARAMETER);
    }
    CheckNotRESULT(NULL, g_stockFont = (HFONT) SelectObject(g_myHdc, g_hFont));
}

//**********************************************************************************
void
destroyObjects(void)
{

    if (gSanityChecks)
        info(DETAIL, TEXT("Delete Objects"));

    // XP fails this call.
    SelectObject(g_myHdc, (HRGN) g_stockRgn);
    CheckNotRESULT(NULL, SelectObject(g_myHdc, (HBRUSH) g_stockBrush));
    if(isMemDC())
    {
        CheckNotRESULT(NULL, SelectObject(g_myHdc, (TBITMAP) g_stockBmp));
    }
    else
    {
        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(NULL, SelectObject(g_myHdc, (TBITMAP) g_stockBmp));
        CheckForLastError(ERROR_INVALID_HANDLE);
    }
    CheckNotRESULT(NULL, SelectObject(g_myHdc, (HFONT) g_stockFont));

    // get rid of any created object
    if (DeleteObject(g_hNullThing))
        info(FAIL, TEXT("NULL OBJ destruction did not return 0"));
    if (g_hRgn && !DeleteObject(g_hRgn))
        info(FAIL, TEXT("HRGN destruction returned 0"));
    if (g_hBrush && !DeleteObject(g_hBrush))
        info(FAIL, TEXT("HBRUSH destruction returned 0"));
    if (g_hBmp && !DeleteObject(g_hBmp))
        info(FAIL, TEXT("HBMP destruction returned 0"));
    if (g_hFont && !DeleteObject(g_hFont))
        info(FAIL, TEXT("HFONT destruction returned 0"));

    CheckNotRESULT(FALSE, DeleteDC(g_myMemDC));
    myReleaseDC(g_myHdc);

    g_hRgn = NULL;
    g_hBrush = NULL;
    g_hBmp = NULL;
    g_hFont = NULL;
    g_myMemDC = NULL;
    g_myHdc = NULL;
}

/***********************************************************************************
***
***   Trys to get the currently selected objects
***
************************************************************************************/
void
GetCurrentobjectCheckReturns(HGDIOBJ s, HGDIOBJ r, HGDIOBJ theObj, int index)
{

    info(DETAIL, TEXT("%s Testing"), objName[index]);

    if (!s)
        info(FAIL, TEXT("SelectObject on %s returned 0"), objName[index]);

    if (r != theObj)
        info(FAIL, TEXT("GetCurrentObject on %s returned 0x%x not 0x%x"), objName[index], r, theObj);
}

//**********************************************************************************
void
GetCurrentObjectTest(int testFunc)
{

    info(ECHO, TEXT("%s: Testing with all OBJs"), funcName[testFunc]);

    initObjects();
    HGDIOBJ sResult,
            rResult;

    // brush
    CheckNotRESULT(NULL, sResult = SelectObject(g_myHdc, g_hBrush));
    CheckNotRESULT(NULL, rResult = GetCurrentObject(g_myHdc, OBJ_BRUSH));

    GetCurrentobjectCheckReturns(sResult, rResult, g_hBrush, oHBRUSH);

    // font
    CheckNotRESULT(NULL, sResult = SelectObject(g_myHdc, g_hFont));
    CheckNotRESULT(NULL, rResult = GetCurrentObject(g_myHdc, OBJ_FONT));
    GetCurrentobjectCheckReturns(sResult, rResult, g_hFont, oHFONT);

    //bitmaps - checked in other places suite
    CheckNotRESULT(NULL, rResult = GetCurrentObject(g_myHdc, OBJ_PAL));

    destroyObjects();
}

/***********************************************************************************
***
***   Trys to getobject on all of the objects
***
************************************************************************************/
void
GetObjectCheckReturns(int r0, int r1, int index)
{

    info(DETAIL, TEXT("%s Testing"), objName[index]);

    if (r0 == 0)
        info(FAIL, TEXT("First call to GetObject to get size returned %d"), r0);

    if (r1 == 0)
        info(FAIL, TEXT("Second call to GetObject returned %d"), r1);

    if (r0 != r1)
        info(FAIL, TEXT("First and second call sizes not equal 0:%d  1:%d"), r0, r1);
}

//**********************************************************************************
void
GetObjectTest(int testFunc)
{

    info(ECHO, TEXT("%s: Testing with all OBJs"), funcName[testFunc]);

    initObjects();
    DIBSECTION logBmp;
    LOGFONT logFont;
    LOGBRUSH logBrush;
    int     result0,
            result1;

    // brush
    CheckNotRESULT(0, result0 = GetObject(g_hBrush, 0, NULL));

    CheckNotRESULT(0, result1 = GetObject(g_hBrush, result0, (LPVOID) & logBrush));
    GetObjectCheckReturns(result0, result1, oHBRUSH);

    // font
    CheckNotRESULT(0, result0 = GetObject(g_hFont, 0, NULL));

    CheckNotRESULT(0, result1 = GetObject(g_hFont, result0, (LPVOID) & logFont));
    GetObjectCheckReturns(result0, result1, oHFONT);

    // bitmap
    CheckNotRESULT(0, result0 = GetObject(g_hBmp, 0, NULL));

    CheckNotRESULT(0, result1 = GetObject(g_hBmp, result0, (LPVOID) & logBmp));
    GetObjectCheckReturns(result0, result1, oHBITMAP);
    if (!logBmp.dsBm.bmBits)
        info(FAIL, TEXT("GetObject returned NULL bmBits"));
    destroyObjects();
}

/***********************************************************************************
***
***   Checks the return of getobjecttype
***
************************************************************************************/
void
GetObjectTypeCheckReturns(DWORD actual, DWORD expected, int index)
{

    info(DETAIL, TEXT("%s Testing"), objName[index]);

    if (actual == 0 || actual != expected)
        info(FAIL, TEXT("GetObjectType failed return:%d expected:%d"), actual, expected);
}

//**********************************************************************************
void
GetObjectTypeTest(int testFunc)
{

    info(ECHO, TEXT("%s: Testing with all OBJs"), funcName[testFunc]);

    initObjects();
    TDC     hdcScreen = NULL;
    TDC     hdcMem    = NULL;
    DWORD   result    = 0x00;

    CheckNotRESULT(NULL, hdcScreen = myGetDC(NULL));
    CheckNotRESULT(NULL, hdcMem = CreateCompatibleDC((TDC) NULL));

    CheckNotRESULT(0, result = GetObjectType(hdcScreen));
    GetObjectTypeCheckReturns(result, OBJ_DC, oHDC);

    CheckNotRESULT(0, result = GetObjectType(hdcMem));
    GetObjectTypeCheckReturns(result, OBJ_MEMDC, omemHDC);

    CheckNotRESULT(0, result = GetObjectType(g_hRgn));
    GetObjectTypeCheckReturns(result, OBJ_REGION, oHRGN);

    CheckNotRESULT(0, result = GetObjectType(g_hBrush));
    GetObjectTypeCheckReturns(result, OBJ_BRUSH, oHBRUSH);

    CheckNotRESULT(0, result = GetObjectType(g_hBmp));
    GetObjectTypeCheckReturns(result, OBJ_BITMAP, oHBITMAP);

    CheckNotRESULT(0, result = GetObjectType(g_hFont));
    GetObjectTypeCheckReturns(result, OBJ_FONT, oHFONT);

    CheckNotRESULT(FALSE, myReleaseDC(NULL, hdcScreen));
    CheckNotRESULT(FALSE, DeleteDC(hdcMem));
    destroyObjects();
}

/***********************************************************************************
***
***   Attempts to selectobject on all supported objects
***
************************************************************************************/
void
SelectObjectCheckReturns(HGDIOBJ s, HGDIOBJ theObj, int index)
{

    info(DETAIL, TEXT("%s Testing"), objName[index]);

    if (s != theObj)
        info(FAIL, TEXT("SelectObject on %s returned 0x%x not 0x%x"), objName[index], s, theObj);
}

//***********************************************************************************

void
SimpleSelectObjectTest(int testFunc)
{

    info(ECHO, TEXT("%s: using Bitmap"), funcName[testFunc]);

    TDC     hdc;
    TBITMAP hBitmap,
            stockBitmap;
    HGDIOBJ sResult;

    CheckNotRESULT(NULL, hdc = CreateCompatibleDC((TDC) NULL));
    CheckNotRESULT(NULL, hBitmap = CreateCompatibleBitmap(hdc, 10, 10));
    CheckNotRESULT(NULL, stockBitmap = (TBITMAP) SelectObject(hdc, hBitmap));
    CheckNotRESULT(NULL, sResult = SelectObject(hdc, hBitmap));

    if (sResult != hBitmap)
        info(FAIL, TEXT("SelectObject:  return 0x%X !=  expected 0x%X"), hBitmap, sResult);

    CheckNotRESULT(NULL, SelectObject(hdc, stockBitmap));
    CheckNotRESULT(FALSE, DeleteObject(hBitmap));
    CheckNotRESULT(FALSE, DeleteDC(hdc));
}

//**********************************************************************************
void
SelectObjectTest(int testFunc)
{

    info(ECHO, TEXT("%s: Testing with all OBJs"), funcName[testFunc]);

    initObjects();
    TDC     hdc;
    HGDIOBJ sResult;
    TBITMAP stockBitmap,
            hBitmap;

    CheckNotRESULT(NULL, hdc = CreateCompatibleDC((TDC) NULL));

    // brush
    CheckNotRESULT(NULL, SelectObject(hdc, g_hBrush));
    CheckNotRESULT(NULL, sResult = SelectObject(hdc, g_hBrush));
    SelectObjectCheckReturns(sResult, g_hBrush, oHBRUSH);

    // region
    CheckNotRESULT(NULL, SelectObject(hdc, g_hRgn));
    CheckNotRESULT(NULL, sResult = SelectObject(hdc, g_hRgn));
    info(DETAIL, TEXT("region Testing"));
    if ((int) sResult != SIMPLEREGION)
        info(FAIL, TEXT("SelectObject returned %d instead of %d"), (int) sResult, SIMPLEREGION);

    // font
    CheckNotRESULT(NULL, SelectObject(hdc, g_hFont));
    CheckNotRESULT(NULL, sResult = SelectObject(hdc, g_hFont));
    SelectObjectCheckReturns(sResult, g_hFont, oHFONT);

    // bitmap
    // can't have bitmap selected into 2 diff DCs
    CheckNotRESULT(NULL, hBitmap = CreateCompatibleBitmap(hdc, 10, 10));

    CheckNotRESULT(NULL, stockBitmap = (TBITMAP) SelectObject(hdc, hBitmap));
    CheckNotRESULT(NULL, sResult = SelectObject(hdc, hBitmap));
    SelectObjectCheckReturns(sResult, hBitmap, oHBITMAP);
    CheckNotRESULT(NULL, SelectObject(hdc, stockBitmap));

    CheckNotRESULT(FALSE, DeleteObject(hBitmap));
    CheckNotRESULT(FALSE, DeleteDC(hdc));
    destroyObjects();
}

/***********************************************************************************
***
***   Passes Null or invalid input to every possible parameter
***
************************************************************************************/

//***********************************************************************************
void
passNull2DObject(int testFunc)
{

    info(ECHO, TEXT("%s - Passing NULL"), funcName[testFunc]);

    TDC     aDC = myGetDC();
    HRGN    hRgn;
    TBITMAP hbmp;
    HPEN hPen;
    HBRUSH hbr;
    HPALETTE hPal;
    HFONT hFont;
    HGDIOBJ hObjBad;

    BITMAP bmp;
    WORD wd;
    LOGPEN lp;
#ifdef UNDER_CE
    LOGBRUSH lb;
    LOGFONT lf;
#endif

    CheckNotRESULT(NULL, hRgn = CreateRectRgn(0, 0, 50, 50));
    CheckNotRESULT(NULL, hbmp = CreateCompatibleBitmap(aDC, 10, 10));
    CheckNotRESULT(NULL, hPen = CreatePen(PS_SOLID, 10, RGB(10, 10, 10)));
    CheckNotRESULT(NULL, hbr = CreateSolidBrush(RGB(10, 10, 10)));
    CheckNotRESULT(NULL, hPal = myCreatePal(aDC, 0));
    CheckNotRESULT(NULL, hFont = setupKnownFont(aFont, 10, 10, 10, 0, FALSE, FALSE));

    // create an invalid GDI object
    CheckNotRESULT(NULL, hObjBad = (HGDIOBJ)CreateRectRgn(0, 0, 20, 20));
    CheckNotRESULT(FALSE, DeleteObject(hObjBad));

    switch (testFunc)
    {
        case EDeleteObject:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, DeleteObject((HGDIOBJ) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, DeleteObject(hObjBad));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EGetCurrentObject:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetCurrentObject((TDC) NULL, 777));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetCurrentObject(g_hdcBAD, 777));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetCurrentObject(g_hdcBAD2, 777));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetCurrentObject(aDC, 777));
            CheckForLastError(ERROR_INVALID_PARAMETER);
            break;
        case EGetObject:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetObject((HGDIOBJ) NULL, 0, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetObject((HGDIOBJ) NULL, sizeof(HPEN), NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetObject(hbmp, 1, &bmp));

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetObject(hPal, 1, &wd));

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetObject(hPen, 1, &lp));

            // desktop treates these differently than the others, and returns
            // valid sizes.
#ifdef UNDER_CE
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetObject(hbr, 1, &lb));

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetObject(hFont, 1, &lf));
#endif
            break;
        case EGetObjectType:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(NULL, GetObjectType((TDC) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(NULL, GetObjectType(g_hdcBAD));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EGetStockObject:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetStockObject(-1));
            CheckForLastError(ERROR_INVALID_PARAMETER);
            break;
        case ESelectObject:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SelectObject((TDC) NULL, (HGDIOBJ) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SelectObject(aDC, (HGDIOBJ) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SelectObject((TDC) NULL, hRgn));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SelectObject(aDC, hObjBad));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SelectObject(g_hdcBAD, hRgn));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        default:
            info(FAIL, TEXT("PassNull called on an invalid case"));
            break;
    }

    myDeletePal(aDC, hPal);
    CheckNotRESULT(FALSE, DeleteObject(hbr));
    CheckNotRESULT(FALSE, DeleteObject(hPen));
    CheckNotRESULT(FALSE, DeleteObject(hRgn));
    CheckNotRESULT(FALSE, DeleteObject(hbmp));
    DeleteObject(hFont);
    myReleaseDC(aDC);
}

/***********************************************************************************
***
***   Trys to getstockobjects on all supported objects
***
************************************************************************************/

//**********************************************************************************
void
GetStockObjectTest(int testFunc)
{

    info(ECHO, TEXT("%s - Getting all stock objects"), funcName[testFunc]);

    HGDIOBJ result0,
            result1;

    for (int i = 0; i < numStockObj; i++)
    {
        CheckNotRESULT(NULL, result0 = GetStockObject(myStockObj[i]));
        CheckNotRESULT(NULL, result1 = GetStockObject(myStockObj[i]));

        if (!(result0 && result0 == result1))
            info(FAIL, TEXT("Two consecutive calls didn't jibe call1:0x%x call2:0x%x  obj:%d"), result0, result1,
                 myStockObj[i]);
    }
}

/***********************************************************************************
***
***   Stress
***
************************************************************************************/

//**********************************************************************************
void
ObjectMemoryLeakTest(int testFunc)
{

    info(ECHO, TEXT("%s - %d Memory Leak Tests for Objects"), funcName[testFunc], 250);

    TDC     hdc;
    TBITMAP hBmp;
    HRGN    hRgn;
    HBRUSH  hBrush;
    HFONT   hFont;
    BYTE   *lpBits = NULL;
    TDC     tempDC = myGetDC(NULL);
    int     pixelN = myGetBitDepth();

    LOGFONT myFont;

    myFont.lfHeight = 10;
    myFont.lfWidth = 10;
    myFont.lfEscapement = 10;
    myFont.lfOrientation = 10;
    myFont.lfWeight = 10;
    myFont.lfItalic = 0;
    myFont.lfUnderline = 0;
    myFont.lfStrikeOut = 0;
    myFont.lfCharSet = ANSI_CHARSET;
    myFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
    myFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    myFont.lfQuality = DEFAULT_QUALITY;
    myFont.lfPitchAndFamily = DEFAULT_PITCH;
    myFont.lfFaceName[0] = NULL;

    // check bounds
    for (int objType = oHDC; objType <= oNULLOBJ; objType++)
    {
        info(DETAIL, TEXT("%s - Crash Test Dummy"), objName[objType]);

        for (int i = 0; i < 250; i++)
        {
            switch (objType)
            {
                case oHDC:
                    CheckNotRESULT(NULL, hdc = myGetDC(NULL));
                    CheckNotRESULT(FALSE, myReleaseDC(NULL, hdc));
                    break;
                case omemHDC:
                    CheckNotRESULT(NULL, hdc = CreateCompatibleDC((TDC) NULL));
                    CheckNotRESULT(FALSE, DeleteDC(hdc));
                    break;
                case oHRGN:
                    CheckNotRESULT(NULL, hRgn = CreateRectRgn(0, 0, 50, 50));
                    CheckNotRESULT(FALSE, DeleteObject(hRgn));
                    break;
                case oHBRUSH:
                    CheckNotRESULT(NULL, hBrush = CreateSolidBrush(RGB(255, 0, 0)));
                    CheckNotRESULT(FALSE, DeleteObject(hBrush));
                    break;
                case oHFONT:
                    CheckNotRESULT(NULL, hFont = CreateFontIndirect(&myFont));
                    CheckNotRESULT(FALSE, DeleteObject(hFont));
                    break;
                case oHBITMAP:
                    CheckNotRESULT(NULL, hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, pixelN, 200, 200, DIB_RGB_COLORS, NULL, FALSE));
                    CheckNotRESULT(FALSE, DeleteObject(hBmp));
                    break;
            }
        }
    }

    myReleaseDC(NULL, tempDC);

}

/***********************************************************************************
***
***   DeleteObjectTest
***
************************************************************************************/

//**********************************************************************************
void
DeleteObjectTest(int testFunc)
{

    info(ECHO, TEXT("%s - Testing"), funcName[testFunc]);

    initObjects();
    destroyObjects();
}

/***********************************************************************************
***
***   DeleteObjectTest2
***
************************************************************************************/

//**********************************************************************************
void
DeleteObjectTest2(int testFunc)
{

    info(ECHO, TEXT("%s - Delete Selected Test"), funcName[testFunc]);

    if (isMemDC())
        return;

    initObjects();

    CheckNotRESULT(FALSE, DeleteObject(g_hRgn));
    CheckNotRESULT(FALSE, DeleteObject(g_hBmp));

    if (GetObjectType(g_hRgn))
        info(FAIL, TEXT("HRGN GetObjectType returned non-0 after the object was deleted."));
    if (GetObjectType(g_hBmp))
        info(FAIL, TEXT("HBMP GetObjectType returned non-0 after the object was deleted."));

    g_hRgn = NULL;
    g_hBmp = NULL;

    destroyObjects();
}

/***********************************************************************************
***
***   Delete an object aquired using GetStockObject
***
************************************************************************************/

//**********************************************************************************
void
DeleteGetStockObjectTest(int testFunc)
{

    info(ECHO, TEXT("%s - Delete objects from GetStockObject"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HGDIOBJ resultObj0,
            resultObj1;
    BOOL    result0;
    int     noPass = DEFAULT_PALETTE;

    for (int i = 0; i < numStockObj; i++)
    {
        CheckNotRESULT(NULL, resultObj0 = GetStockObject(myStockObj[i]));

        if(myStockObj[i] != DEFAULT_PALETTE)
        {
            CheckNotRESULT(NULL, SelectObject(hdc, resultObj0));
        }
        else
        {
            CheckForRESULT(NULL, SelectObject(hdc, resultObj0));
        }

        CheckNotRESULT(FALSE, result0 = DeleteObject(resultObj0));

        if(myStockObj[i] != DEFAULT_PALETTE)
        {
            CheckNotRESULT(NULL, resultObj1 = SelectObject(hdc, resultObj0));
        }
        else
        {
            CheckForRESULT(NULL, resultObj1 = SelectObject(hdc, resultObj0));
        }

        if (!result0)
            info(FAIL, TEXT("Delete Failed on obj:%d"), i);

        if (myStockObj[i] != noPass && !(resultObj0 && resultObj0 == resultObj1))
            info(FAIL, TEXT("Two consecutive calls didn't jibe call1:0x%x call2:0x%x  obj:%d"),
                           resultObj0, resultObj1, i);

        if (myStockObj[i] == noPass && (resultObj0 && resultObj0 == resultObj1))
            info(FAIL, TEXT("Two consecutive calls shouldn't have jibed call1:0x%x call2:0x%x  obj:%d"),
                            resultObj0, resultObj1, i);
    }
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   GetObject DIB Test
***
************************************************************************************/

//***********************************************************************************
void
GetObjectDIBTest(int testFunc)
{
    info(ECHO, TEXT("%s - GetObject DIB Test"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    BYTE   *lpBits = NULL;
    TBITMAP hBmp;
    BITMAP  bmp;
    int     result;

#ifdef UNDER_CE
    // use a DIBSECTION2 instead of DIBSECTION since these are larger objects
    DIBSECTION2 bm;
#else
    DIBSECTION bm;
#endif

    struct
    {
        BITMAPINFOHEADER bmih;
        RGBQUAD ct[4];
    }
    bmi;

    // create our test scenarios in a matrix
    const struct 
    {
        // common BITMAPINFOHEADER fields
        INT input_width;
        INT input_height;
        INT input_cbBuffer;
        INT input_biBitCount;
        INT input_biCompression;

        // param to CreateDIBSection
        UINT input_iUsage;

        // expected results
        INT expect_width;           // always match input
        INT expect_height;          // always positive unless DIBSECTION2 specified, then match input
        INT expect_cbBuffer;        // always DIBSECTION unless 16bpp\32bpp and DIBSECTION2 are specified
        INT expect_biClrUsed;       // varies based on biBitCount
        INT expect_biCompression;   // varies based on biBitCount
    }
    TestMatrix[] = {
        // verify positive and negative retrieval for biHeight
        50, -40, sizeof (DIBSECTION),  16, BI_RGB, DIB_RGB_COLORS, 50, 40,  sizeof (DIBSECTION),  3, BI_BITFIELDS,
        50, 40,  sizeof (DIBSECTION),  16, BI_RGB, DIB_RGB_COLORS, 50, 40,  sizeof (DIBSECTION),  3, BI_BITFIELDS,
#ifdef UNDER_CE
        50, -40, sizeof (DIBSECTION2), 16, BI_RGB, DIB_RGB_COLORS, 50, -40, sizeof (DIBSECTION2), 3, BI_BITFIELDS,
        50, 40,  sizeof (DIBSECTION2), 16, BI_RGB, DIB_RGB_COLORS, 50, 40,  sizeof (DIBSECTION2), 3, BI_BITFIELDS,
#endif

        // verify DIBSECTION results with biCompression at supported biBitCounts (16bpp and 32bpp)
        50, -40, sizeof (DIBSECTION),  16, BI_RGB,            DIB_RGB_COLORS, 50, 40,  sizeof (DIBSECTION),  3, BI_BITFIELDS,
        50, -40, sizeof (DIBSECTION),  16, BI_BITFIELDS,      DIB_RGB_COLORS, 50, 40,  sizeof (DIBSECTION),  3, BI_BITFIELDS,
        50, -40, sizeof (DIBSECTION),  16, BI_ALPHABITFIELDS, DIB_RGB_COLORS, 50, 40,  sizeof (DIBSECTION),  3, BI_BITFIELDS,
        50, -40, sizeof (DIBSECTION),  32, BI_RGB,            DIB_RGB_COLORS, 50, 40,  sizeof (DIBSECTION),  0, BI_RGB,
        50, -40, sizeof (DIBSECTION),  32, BI_BITFIELDS,      DIB_RGB_COLORS, 50, 40,  sizeof (DIBSECTION),  3, BI_BITFIELDS,
        50, -40, sizeof (DIBSECTION),  32, BI_ALPHABITFIELDS, DIB_RGB_COLORS, 50, 40,  sizeof (DIBSECTION),  3, BI_BITFIELDS,

        // verify DIBSECTION2 results with biCompression at supported biBitCounts (16bpp and 32bpp)
#ifdef UNDER_CE
        50, -40, sizeof (DIBSECTION2), 16, BI_RGB,            DIB_RGB_COLORS, 50, -40, sizeof (DIBSECTION2), 3, BI_BITFIELDS,
        50, -40, sizeof (DIBSECTION2), 16, BI_BITFIELDS,      DIB_RGB_COLORS, 50, -40, sizeof (DIBSECTION2), 3, BI_BITFIELDS,
        50, -40, sizeof (DIBSECTION2), 16, BI_ALPHABITFIELDS, DIB_RGB_COLORS, 50, -40, sizeof (DIBSECTION2), 4, BI_ALPHABITFIELDS,
        50, -40, sizeof (DIBSECTION2), 32, BI_RGB,            DIB_RGB_COLORS, 50, -40, sizeof (DIBSECTION),  0, BI_RGB,
        50, -40, sizeof (DIBSECTION2), 32, BI_BITFIELDS,      DIB_RGB_COLORS, 50, -40, sizeof (DIBSECTION2), 3, BI_BITFIELDS,
        50, -40, sizeof (DIBSECTION2), 32, BI_ALPHABITFIELDS, DIB_RGB_COLORS, 50, -40, sizeof (DIBSECTION2), 4, BI_ALPHABITFIELDS,
#endif

        // verify biCompression at 24bpp biBitCount is treated as BI_RGB
        50, -40, sizeof (DIBSECTION),  24, BI_RGB,            DIB_RGB_COLORS, 50, 40,  sizeof (DIBSECTION),  0, BI_RGB,
        50, -40, sizeof (DIBSECTION),  24, BI_BITFIELDS,      DIB_RGB_COLORS, 50, 40,  sizeof (DIBSECTION),  0, BI_RGB,
        50, -40, sizeof (DIBSECTION),  24, BI_ALPHABITFIELDS, DIB_RGB_COLORS, 50, 40,  sizeof (DIBSECTION),  0, BI_RGB,
#ifdef UNDER_CE
        50, -40, sizeof (DIBSECTION2), 24, BI_RGB,            DIB_RGB_COLORS, 50, -40, sizeof (DIBSECTION),  0, BI_RGB,
        50, -40, sizeof (DIBSECTION2), 24, BI_BITFIELDS,      DIB_RGB_COLORS, 50, -40, sizeof (DIBSECTION),  0, BI_RGB,
        50, -40, sizeof (DIBSECTION2), 24, BI_ALPHABITFIELDS, DIB_RGB_COLORS, 50, -40, sizeof (DIBSECTION),  0, BI_RGB,
#endif

        // verify DIBSECTION result with varying biBitCount
        50, -40, sizeof (DIBSECTION),   1, BI_RGB, DIB_RGB_COLORS, 50, 40,  sizeof (DIBSECTION),    2, BI_RGB,
        50, -40, sizeof (DIBSECTION),   2, BI_RGB, DIB_RGB_COLORS, 50, 40,  sizeof (DIBSECTION),    4, BI_RGB,
        50, -40, sizeof (DIBSECTION),   4, BI_RGB, DIB_RGB_COLORS, 50, 40,  sizeof (DIBSECTION),   16, BI_RGB,
        50, -40, sizeof (DIBSECTION),   8, BI_RGB, DIB_RGB_COLORS, 50, 40,  sizeof (DIBSECTION),  256, BI_RGB,
        50, -40, sizeof (DIBSECTION),  16, BI_RGB, DIB_RGB_COLORS, 50, 40,  sizeof (DIBSECTION),    3, BI_BITFIELDS,
        50, -40, sizeof (DIBSECTION),  24, BI_RGB, DIB_RGB_COLORS, 50, 40,  sizeof (DIBSECTION),    0, BI_RGB,
        50, -40, sizeof (DIBSECTION),  32, BI_RGB, DIB_RGB_COLORS, 50, 40,  sizeof (DIBSECTION),    0, BI_RGB,

        // verify DIBSECTION2 result with varying biBitCount
#ifdef UNDER_CE
        50, -40, sizeof (DIBSECTION2),  1, BI_RGB, DIB_RGB_COLORS, 50, -40, sizeof (DIBSECTION),    2, BI_RGB,
        50, -40, sizeof (DIBSECTION2),  2, BI_RGB, DIB_RGB_COLORS, 50, -40, sizeof (DIBSECTION),    4, BI_RGB,
        50, -40, sizeof (DIBSECTION2),  4, BI_RGB, DIB_RGB_COLORS, 50, -40, sizeof (DIBSECTION),   16, BI_RGB,
        50, -40, sizeof (DIBSECTION2),  8, BI_RGB, DIB_RGB_COLORS, 50, -40, sizeof (DIBSECTION),  256, BI_RGB,
        50, -40, sizeof (DIBSECTION2), 16, BI_RGB, DIB_RGB_COLORS, 50, -40, sizeof (DIBSECTION2),   3, BI_BITFIELDS,
        50, -40, sizeof (DIBSECTION2), 24, BI_RGB, DIB_RGB_COLORS, 50, -40, sizeof (DIBSECTION),    0, BI_RGB,
        50, -40, sizeof (DIBSECTION2), 32, BI_RGB, DIB_RGB_COLORS, 50, -40, sizeof (DIBSECTION),    0, BI_RGB,
#endif

        // verify biClrUsed based on palettized iUsage (only 8bpp is supported; less than 8bpp is up converted)
        50, -40, sizeof (DIBSECTION),   1, BI_RGB, DIB_PAL_COLORS, 50, 40,  sizeof (DIBSECTION),  256, BI_RGB,
        50, -40, sizeof (DIBSECTION),   2, BI_RGB, DIB_PAL_COLORS, 50, 40,  sizeof (DIBSECTION),  256, BI_RGB,
        50, -40, sizeof (DIBSECTION),   4, BI_RGB, DIB_PAL_COLORS, 50, 40,  sizeof (DIBSECTION),  256, BI_RGB,
        50, -40, sizeof (DIBSECTION),   8, BI_RGB, DIB_PAL_COLORS, 50, 40,  sizeof (DIBSECTION),  256, BI_RGB,
#ifdef UNDER_CE
        50, -40, sizeof (DIBSECTION2),  8, BI_RGB, DIB_PAL_COLORS, 50, -40, sizeof (DIBSECTION),  256, BI_RGB,
#endif
    };
    const UINT nTestCases = countof(TestMatrix);

    for (INT i=0; i<nTestCases; ++i)
    {
        memset(&bmi.bmih, 0, sizeof (BITMAPINFOHEADER));
        bmi.bmih.biSize        = sizeof (BITMAPINFOHEADER);
        bmi.bmih.biPlanes      = 1;
        bmi.bmih.biWidth       = TestMatrix[i].input_width;
        bmi.bmih.biHeight      = TestMatrix[i].input_height;
        bmi.bmih.biBitCount    = TestMatrix[i].input_biBitCount;
        bmi.bmih.biCompression = TestMatrix[i].input_biCompression;

        setRGBQUAD(&bmi.ct[0], 0x0, 0x0, 0x0);
        setRGBQUAD(&bmi.ct[1], 0x55, 0x55, 0x55);
        setRGBQUAD(&bmi.ct[2], 0xaa, 0xaa, 0xaa);
        setRGBQUAD(&bmi.ct[3], 0xff, 0xff, 0xff);

        CheckNotRESULT(NULL, hBmp = CreateDIBSection(hdc, (LPBITMAPINFO) &bmi, TestMatrix[i].input_iUsage, (VOID **) &lpBits, NULL, NULL));

        // GetObject using a DIBSECTION or DIBSECTION2 size
        CheckForRESULT(TestMatrix[i].expect_cbBuffer, result = GetObject(hBmp, TestMatrix[i].input_cbBuffer, (LPBYTE) &bm));

        // verify the BITMAP and BITMAPINFOHEADER results
        CheckForRESULT(lpBits,                             bm.dsBm.bmBits);
        CheckForRESULT(TestMatrix[i].input_biBitCount,     bm.dsBm.bmBitsPixel);
        CheckForRESULT(TestMatrix[i].input_biBitCount,     bm.dsBmih.biBitCount);
        CheckForRESULT(TestMatrix[i].expect_biClrUsed,     bm.dsBmih.biClrUsed);
        CheckForRESULT(TestMatrix[i].expect_biCompression, bm.dsBmih.biCompression);

        // verify the BITMAP (width and height must always be greater than zero)
        if (bm.dsBm.bmWidth != abs(TestMatrix[i].expect_width) || bm.dsBm.bmHeight != abs(TestMatrix[i].expect_height))
            info(FAIL, TEXT("GetObject returned bm.dsBm: h:%d w:%d  expected: h:%d w:%d"), bm.dsBm.bmWidth, bm.dsBm.bmHeight,
                abs(TestMatrix[i].expect_width), abs(TestMatrix[i].expect_height));

        // verify the BITMAPINFOHEADER (height can be positive or negative)
        if (bm.dsBmih.biWidth != TestMatrix[i].expect_width || bm.dsBmih.biHeight != TestMatrix[i].expect_height)
            info(FAIL, TEXT("GetObject returned bm.dsBmih: h:%d w:%d  expected: h:%d w:%d"), bm.dsBmih.biWidth, bm.dsBmih.biHeight,
                TestMatrix[i].expect_width, TestMatrix[i].expect_height);

        // GetObject using a BITMAP size
        CheckForRESULT(sizeof (BITMAP), result = GetObject(hBmp, sizeof (BITMAP), (LPBYTE) & bmp));

        // verify the BITMAP (width and height must always be greater than zero)
        CheckForRESULT(lpBits, bmp.bmBits);
        CheckForRESULT(TestMatrix[i].input_biBitCount, bmp.bmBitsPixel);
        if (bmp.bmWidth != abs(TestMatrix[i].expect_width) || bmp.bmHeight != abs(TestMatrix[i].expect_height))
            info(FAIL, TEXT("GetObject returned bmp: h:%d w:%d  expected: h:%d w:%d"), bmp.bmWidth, bmp.bmHeight,
                abs(TestMatrix[i].expect_width), abs(TestMatrix[i].expect_height));

        CheckNotRESULT(FALSE, DeleteObject(hBmp));
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   GetObject BMP Test
***
************************************************************************************/
// this test creates system memory bitmaps of assorted bit depths and a video memory bitmap,
// it then retrieves the BITMAP structure from the resulting hbmp and verifies that it's correct.
//***********************************************************************************
void
GetObjectBMPTest(int testFunc)
{

    info(ECHO, TEXT("%s - GetObject BMP Test"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    TBITMAP hBmp;
    BITMAP  bmp;
    int     result;
    UINT bitDepths[] = { 1, 2, 4, 8, 16, 24, 32 };

    for(int i=0; i < countof(bitDepths); i++)
    {
        CheckNotRESULT(NULL, hBmp = myCreateBitmap(50, 40, 1, bitDepths[i], NULL));
        CheckNotRESULT(0, result = GetObject(hBmp, sizeof (BITMAP), (LPBYTE) & bmp));

        if (bmp.bmWidth != 50 || bmp.bmHeight != 40 || bmp.bmBitsPixel != bitDepths[i] || bmp.bmBits != NULL || bmp.bmPlanes != 1)
            info(ECHO, TEXT("GetObject returned: h:%d w:%d d:%d bits:%d planes:%d  expected: h:40 w:50 d:%d bits:0 planes:1"), bmp.bmWidth, bmp.bmHeight, bmp.bmBitsPixel, bmp.bmBits, bmp.bmPlanes, bitDepths[i]);
        CheckNotRESULT(FALSE, DeleteObject(hBmp));
    }

    CheckNotRESULT(NULL, hBmp = CreateCompatibleBitmap(hdc, 50, 40));
    CheckNotRESULT(0, result = GetObject(hBmp, sizeof (BITMAP), (LPBYTE) & bmp));

    if (bmp.bmWidth != 50 || bmp.bmHeight != 40 || bmp.bmBitsPixel != myGetBitDepth() || bmp.bmBits != NULL || bmp.bmPlanes != 1)
        info(ECHO, TEXT("GetObject returned: h:%d w:%d d:%d bits:%d planes:%d  expected: h:40 w:50 d:%d bits:0 planes:1"), bmp.bmWidth, bmp.bmHeight, bmp.bmBitsPixel, bmp.bmBits, bmp.bmPlanes, myGetBitDepth());

    CheckNotRESULT(FALSE, DeleteObject(hBmp));
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   GetStockObject Param Test
***
************************************************************************************/

//***********************************************************************************
void
GetStockObjectParamTest(int testFunc)
{

    info(ECHO, TEXT("GetStockObject Param Test"));
    TDC     hdc;
    HGDIOBJ hgdiObj;
    int     type[] = {
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
        DEFAULT_PALETTE,
#ifndef UNDER_CE
        ANSI_FIXED_FONT,
        ANSI_VAR_FONT,
        DEVICE_DEFAULT_FONT,
        DEFAULT_GUI_FONT,
        OEM_FIXED_FONT,
        SYSTEM_FIXED_FONT,
#endif                          // UNDER_CE
    };
    int     numTypes = countof(type);

    for (int i = 0; i < numTypes; i++)
    {
        hdc = myGetDC();
        CheckNotRESULT(0, hgdiObj = GetStockObject(type[i]));

        if(type[i] != DEFAULT_PALETTE)
        {
            CheckNotRESULT(NULL, SelectObject(hdc, hgdiObj));
        }
        else
        {
            CheckForRESULT(NULL, SelectObject(hdc, hgdiObj));
        }
        CheckNotRESULT(FALSE, DeleteObject(hgdiObj));
        myReleaseDC(hdc);
    }
}

void
DeleteObjectSelectedObjectTest(int testFunc)
{
    info(ECHO, TEXT("%s - DeleteObjectSelectedObjectTest"), funcName[testFunc]);
    TDC hdc = myGetDC();
    TDC hdcCompat;
    TBITMAP hbmp, hbmpStock;
    HBRUSH hbr, hbrStock;
    HPEN hpn, hpnStock;
    HFONT hfnt, hfntStock;
    LOGFONT lf;

    memset(&lf, 0, sizeof(LOGFONT));
    lf.lfUnderline = TRUE;

    // test 1, deleteing a selected bitmap, then deleting the dc, then the bitmap again.
    info(DETAIL, TEXT("Select, DeleteObject, DeleteDC, DeleteObject test."));
    CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
    CheckNotRESULT(NULL, hbmp = CreateCompatibleBitmap(hdc, 10, 10));
    CheckNotRESULT(NULL, hbr = CreateSolidBrush(randColorRef()));
    CheckNotRESULT(NULL, hpn = CreatePen(PS_DASH, 10, randColorRef()));
    CheckNotRESULT(NULL, hfnt = CreateFontIndirect(&lf));

    CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));
    CheckNotRESULT(NULL, hbrStock = (HBRUSH) SelectObject(hdcCompat, hbr));
    CheckNotRESULT(NULL, hpnStock = (HPEN) SelectObject(hdcCompat, hpn));
    CheckNotRESULT(NULL, hfntStock = (HFONT) SelectObject(hdcCompat, hfnt));

    if(DeleteObject(hbmp))
        info(FAIL, TEXT("Succeeded deleting a selected compatible bitmap"));

    if(DeleteObject(hbr))
        info(FAIL, TEXT("Succeeded deleting a selected compatible bitmap"));

    if(DeleteObject(hpn))
        info(FAIL, TEXT("Succeeded deleting a selected compatible bitmap"));

    if(DeleteObject(hfnt))
        info(FAIL, TEXT("Succeeded deleting a selected compatible bitmap"));

    CheckNotRESULT(FALSE, DeleteDC(hdcCompat));
    CheckNotRESULT(FALSE, DeleteObject(hbmp));
    CheckNotRESULT(FALSE, DeleteObject(hbr));
    CheckNotRESULT(FALSE, DeleteObject(hpn));
    CheckNotRESULT(FALSE, DeleteObject(hfnt));

    // test 2, deleting a selected bitmap, then deselecting the bitmap and deleting it.
    info(DETAIL, TEXT("Select, DeleteObject, Deselect, DeleteObject, DeleteDC test."));
    CheckNotRESULT(NULL, hdcCompat = CreateCompatibleDC(hdc));
    CheckNotRESULT(NULL, hbmp = CreateCompatibleBitmap(hdc, 10, 10));
    CheckNotRESULT(NULL, hbr = CreateSolidBrush(randColorRef()));
    CheckNotRESULT(NULL, hpn = CreatePen(PS_DASH, 10, randColorRef()));
    CheckNotRESULT(NULL, hfnt = CreateFontIndirect(&lf));

    CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcCompat, hbmp));
    CheckNotRESULT(NULL, hbrStock = (HBRUSH) SelectObject(hdcCompat, hbr));
    CheckNotRESULT(NULL, hpnStock = (HPEN) SelectObject(hdcCompat, hpn));
    CheckNotRESULT(NULL, hfntStock = (HFONT) SelectObject(hdcCompat, hfnt));


    if(DeleteObject(hbmp))
        info(FAIL, TEXT("Succeeded deleting a selected compatible bitmap"));

    if(DeleteObject(hbr))
        info(FAIL, TEXT("Succeeded deleting a selected compatible bitmap"));

    if(DeleteObject(hpn))
        info(FAIL, TEXT("Succeeded deleting a selected compatible bitmap"));

    if(DeleteObject(hfnt))
        info(FAIL, TEXT("Succeeded deleting a selected compatible bitmap"));

    CheckNotRESULT(NULL, SelectObject(hdcCompat, hbmpStock));
    CheckNotRESULT(NULL, SelectObject(hdcCompat, hbrStock));
    CheckNotRESULT(NULL, SelectObject(hdcCompat, hpnStock));
    CheckNotRESULT(NULL, SelectObject(hdcCompat, hfntStock));

    CheckNotRESULT(FALSE, DeleteObject(hbmp));
    CheckNotRESULT(FALSE, DeleteObject(hbr));
    CheckNotRESULT(FALSE, DeleteObject(hpn));
    CheckNotRESULT(FALSE, DeleteObject(hfnt));
    CheckNotRESULT(FALSE, DeleteDC(hdcCompat));

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   APIs
***
************************************************************************************/

//**********************************************************************************
TESTPROCAPI DeleteObject_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    BOOL OldVerify = SetSurfVerify(FALSE);

    // Breadth
    passNull2DObject(EDeleteObject);
    DeleteObjectTest(EDeleteObject);
    DeleteObjectTest2(EDeleteObject);
    ObjectMemoryLeakTest(EDeleteObject);
    // desktop succeeds deletion of selected objects.
#ifdef UNDER_CE
    DeleteObjectSelectedObjectTest(EDeleteObject);
#endif

    // Depth
    // None

    SetSurfVerify(OldVerify);

    return getCode();
}

//**********************************************************************************
TESTPROCAPI GetCurrentObject_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    BOOL OldVerify = SetSurfVerify(FALSE);

    // Breadth
    passNull2DObject(EGetCurrentObject);
    GetCurrentObjectTest(EGetCurrentObject);

    // Depth
    // None

    SetSurfVerify(OldVerify);

    return getCode();
}

//**********************************************************************************
TESTPROCAPI GetObject_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    BOOL OldVerify = SetSurfVerify(FALSE);

    // Breadth
    passNull2DObject(EGetObject);
    GetObjectTest(EGetObject);

    // Depth
    GetObjectDIBTest(EGetObject);
    GetObjectBMPTest(EGetObject);

    SetSurfVerify(OldVerify);

    return getCode();
}

//**********************************************************************************
TESTPROCAPI GetObjectType_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    BOOL OldVerify = SetSurfVerify(FALSE);

    // Breadth
    passNull2DObject(EGetObjectType);
    GetObjectTypeTest(EGetObjectType);

    // Depth
    // None

    SetSurfVerify(OldVerify);

    return getCode();
}

//**********************************************************************************
TESTPROCAPI GetStockObject_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2DObject(EGetStockObject);
    DeleteGetStockObjectTest(EGetStockObject);
    GetStockObjectTest(EGetStockObject);
    GetStockObjectParamTest(EGetStockObject);

    // Depth
    // None

    return getCode();
}

//**********************************************************************************
TESTPROCAPI SelectObject_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    BOOL OldVerify = SetSurfVerify(FALSE);

    // Breadth
    passNull2DObject(ESelectObject);
    SimpleSelectObjectTest(ESelectObject);
    SelectObjectTest(ESelectObject);

    // Depth
    // None

    SetSurfVerify(OldVerify);

    return getCode();
}
