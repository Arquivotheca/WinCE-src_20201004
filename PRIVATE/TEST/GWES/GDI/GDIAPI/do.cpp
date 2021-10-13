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
static TDC g_ScreenDC;
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
    g_ScreenDC = myGetDC(NULL);
    g_myMemDC = myCreateCompatibleDC(g_myHdc);

    g_hRgn = CreateRectRgn(0, 0, 50, 50);
    g_hBrush = CreateSolidBrush(RGB(255, 0, 0));

    PatBlt(g_myHdc, 0, 0, zx, zy, WHITENESS);
    PatBlt(g_ScreenDC, 0, 0, zx, zy, WHITENESS);
    PatBlt(g_myMemDC, 0, 0, zx, zy, WHITENESS);

    BYTE   *lpBits = NULL;

    g_hBmp = myCreateDIBSection(g_myHdc, (VOID **) & lpBits, myGetBitDepth(), 200, 200, DIB_RGB_COLORS);

    LOGFONT myFont;

    myFont.lfHeight = myFont.lfWidth = myFont.lfEscapement = myFont.lfOrientation = myFont.lfWeight = 10;
    myFont.lfItalic = myFont.lfUnderline = myFont.lfStrikeOut = 0;
    myFont.lfCharSet = ANSI_CHARSET;
    myFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
    myFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    myFont.lfQuality = DEFAULT_QUALITY;
    myFont.lfPitchAndFamily = DEFAULT_PITCH;
    myFont.lfFaceName[0] = NULL;

    g_hFont = CreateFontIndirect(&myFont);
    // get stock objects
    g_stockRgn = (HRGN) SelectObject(g_myHdc, g_hRgn);
    g_stockBrush = (HBRUSH) SelectObject(g_myHdc, g_hBrush);
    g_stockBmp = (TBITMAP) SelectObject(g_myHdc, g_hBmp);
    g_stockFont = (HFONT) SelectObject(g_myHdc, g_hFont);

    // check for valid creation
    if (!g_myHdc)
        info(FAIL, TEXT("HDC creation returned NULL"));
    if (!g_myMemDC)
        info(FAIL, TEXT("MemDC creation returned NULL"));
    if (!g_hRgn)
        info(FAIL, TEXT("HRGN creation returned NULL"));
    if (!g_hBrush)
        info(FAIL, TEXT("HBRUSH creation returned NULL"));
    if (!g_hBmp)
        info(FAIL, TEXT("HBMP creation returned NULL"));
    if (!g_hFont)
        info(FAIL, TEXT("HFONT creation returned NULL"));
}

//**********************************************************************************
void
destroyObjects(void)
{

    if (gSanityChecks)
        info(DETAIL, TEXT("Delete Objects"));

    SelectObject(g_myHdc, (HRGN) g_stockRgn);
    SelectObject(g_myHdc, (HBRUSH) g_stockBrush);
    SelectObject(g_myHdc, (TBITMAP) g_stockBmp);
    SelectObject(g_myHdc, (HFONT) g_stockFont);

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
    if (g_ScreenDC && !myReleaseDC(NULL, g_ScreenDC))
        info(FAIL, TEXT("Screen HDC destruction returned 0"));
    
    myDeleteDC(g_myMemDC);
    myReleaseDC(g_myHdc);

    g_hRgn = NULL;
    g_hBrush = NULL;
    g_hBmp = NULL;
    g_hFont = NULL;
    g_ScreenDC = NULL;
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

    PatBlt(g_myHdc, 0, 0, zx, zy, WHITENESS);

    // brush
    sResult = SelectObject(g_myHdc, g_hBrush);
    mySetLastError();
    rResult = GetCurrentObject(g_myHdc, OBJ_BRUSH);
    myGetLastError(NADA);
    GetCurrentobjectCheckReturns(sResult, rResult, g_hBrush, oHBRUSH);

    // font
    sResult = SelectObject(g_myHdc, g_hFont);
    mySetLastError();
    rResult = GetCurrentObject(g_myHdc, OBJ_FONT);
    myGetLastError(NADA);
    GetCurrentobjectCheckReturns(sResult, rResult, g_hFont, oHFONT);

    //bitmaps - checked in other places suite
    mySetLastError();
    rResult = GetCurrentObject(g_myHdc, OBJ_PAL);
    myGetLastError(NADA);

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
    mySetLastError();
    result0 = GetObject(g_hBrush, 0, NULL);
    myGetLastError(NADA);

    mySetLastError();
    result1 = GetObject(g_hBrush, result0, (LPVOID) & logBrush);
    myGetLastError(NADA);
    GetObjectCheckReturns(result0, result1, oHBRUSH);

    // font
    mySetLastError();
    result0 = GetObject(g_hFont, 0, NULL);
    myGetLastError(NADA);

    mySetLastError();
    result1 = GetObject(g_hFont, result0, (LPVOID) & logFont);
    myGetLastError(NADA);
    GetObjectCheckReturns(result0, result1, oHFONT);

    // bitmap
    mySetLastError();
    result0 = GetObject(g_hBmp, 0, NULL);
    myGetLastError(NADA);

    mySetLastError();
    result1 = GetObject(g_hBmp, result0, (LPVOID) & logBmp);
    myGetLastError(NADA);
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
    TDC     hdc = myCreateCompatibleDC(NULL);
    DWORD   result;

    mySetLastError();
    result = GetObjectType(g_ScreenDC);
    myGetLastError(NADA);
    GetObjectTypeCheckReturns(result, OBJ_DC, oHDC);

    mySetLastError();
    result = GetObjectType(hdc);
    myGetLastError(NADA);
    GetObjectTypeCheckReturns(result, OBJ_MEMDC, omemHDC);

    mySetLastError();
    result = GetObjectType(g_hRgn);
    myGetLastError(NADA);
    GetObjectTypeCheckReturns(result, OBJ_REGION, oHRGN);

    mySetLastError();
    result = GetObjectType(g_hBrush);
    myGetLastError(NADA);
    GetObjectTypeCheckReturns(result, OBJ_BRUSH, oHBRUSH);

    mySetLastError();
    result = GetObjectType(g_hBmp);
    myGetLastError(NADA);
    GetObjectTypeCheckReturns(result, OBJ_BITMAP, oHBITMAP);

    mySetLastError();
    result = GetObjectType(g_hFont);
    myGetLastError(NADA);
    GetObjectTypeCheckReturns(result, OBJ_FONT, oHFONT);

    myDeleteDC(hdc);
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

    TDC     hdc = CreateCompatibleDC((TDC) NULL);
    TBITMAP hBitmap = CreateCompatibleBitmap(hdc, 10, 10),
            stockBitmap = (TBITMAP) SelectObject(hdc, hBitmap);
    HGDIOBJ sResult = SelectObject(hdc, hBitmap);

    if (sResult != hBitmap)
        info(FAIL, TEXT("SelectObject:  return 0x%X !=  expected 0x%X"), hBitmap, sResult);

    SelectObject(hdc, stockBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdc);
}

//**********************************************************************************
void
SelectObjectTest(int testFunc)
{

    info(ECHO, TEXT("%s: Testing with all OBJs"), funcName[testFunc]);

    initObjects();
    TDC     hdc = myCreateCompatibleDC(NULL);
    HGDIOBJ sResult;

    // brush
    mySetLastError();
    SelectObject(hdc, g_hBrush);
    myGetLastError(NADA);
    sResult = SelectObject(hdc, g_hBrush);
    SelectObjectCheckReturns(sResult, g_hBrush, oHBRUSH);

    // region
    mySetLastError();
    SelectObject(hdc, g_hRgn);
    myGetLastError(NADA);
    sResult = SelectObject(hdc, g_hRgn);
    info(DETAIL, TEXT("region Testing"));
    if ((int) sResult != SIMPLEREGION)
        info(FAIL, TEXT("SelectObject returned %d instead of %d"), (int) sResult, SIMPLEREGION);
    // font
    mySetLastError();
    SelectObject(hdc, g_hFont);
    myGetLastError(NADA);
    sResult = SelectObject(hdc, g_hFont);
    SelectObjectCheckReturns(sResult, g_hFont, oHFONT);

    // bitmap
    // can't have bitmap selected into 2 diff DCs
    TBITMAP stockBitmap,
            hBitmap = CreateCompatibleBitmap(hdc, 10, 10);

    mySetLastError();
    stockBitmap = (TBITMAP) SelectObject(hdc, hBitmap);
    myGetLastError(NADA);
    sResult = SelectObject(hdc, hBitmap);
    SelectObjectCheckReturns(sResult, hBitmap, oHBITMAP);
    SelectObject(hdc, stockBitmap);
    DeleteObject(hBitmap);

    myDeleteDC(hdc);
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
    HRGN    hRgn = CreateRectRgn(0, 0, 50, 50);
    int     result;

    mySetLastError();
    switch (testFunc)
    {
        case EDeleteObject:
            result = DeleteObject((HGDIOBJ) NULL);
            passCheck(result, 0, TEXT("NULL"));
            break;
        case EGetCurrentObject:
            result = (int) GetCurrentObject((TDC) NULL, 777);
            passCheck(result, 0, TEXT("NULL,-1"));
            result = (int) GetCurrentObject(aDC, 777);
            passCheck(result, 0, TEXT("aDC,-1"));
            break;
        case EGetObject:
            result = (BOOL) GetObject((HGDIOBJ) NULL, 0, NULL);
            passCheck(result, 0, TEXT("NULL,0,NULL"));
            result = (BOOL) GetObject((HGDIOBJ) NULL, 10, NULL);
            passCheck(result, 0, TEXT("NULL,10,NULL"));
            break;
        case EGetObjectType:
            result = (BOOL) GetObjectType((TDC) NULL);
            passCheck(result, 0, TEXT("NULL"));
            break;
        case EGetStockObject:
            result = (BOOL) GetStockObject(-1);
            passCheck(result, 0, TEXT("-1"));
            break;
        case ESelectObject:
            result = (BOOL) SelectObject((TDC) NULL, (HGDIOBJ) NULL);
            passCheck(result, 0, TEXT("NULL,NULL"));
            result = (BOOL) SelectObject(aDC, (HGDIOBJ) NULL);
            passCheck(result, 0, TEXT("aDC,NULL"));
            result = (BOOL) SelectObject((TDC) NULL, hRgn);
            passCheck(result, 0, TEXT("NULL,hRgn"));
            break;
    }
    switch (testFunc)
    {
        case EGetStockObject:
        case EDeleteObject:
        case EGetObject:
            myGetLastError(NADA);
            break;
        case EGetCurrentObject:
            myGetLastError(ERROR_INVALID_PARAMETER);
            break;
        default:
            myGetLastError(ERROR_INVALID_HANDLE);
            break;
    }
    DeleteObject(hRgn);
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
        mySetLastError();
        result0 = GetStockObject(myStockObj[i]);
        myGetLastError(NADA);

        mySetLastError();
        result1 = GetStockObject(myStockObj[i]);
        myGetLastError(NADA);

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
                    hdc = myGetDC(NULL);
                    if (!hdc)
                        info(FAIL, TEXT("HDC creation returned NULL"));
                    if (!myReleaseDC(NULL, hdc))
                        info(FAIL, TEXT("HDC destruction returned 0"));
                    break;
                case omemHDC:
                    hdc = CreateCompatibleDC((TDC) NULL);
                    if (!hdc)
                        info(FAIL, TEXT("HDC creation returned NULL"));
                    if (!DeleteDC(hdc))
                        info(FAIL, TEXT("HDC destruction returned 0"));
                    break;
                case oHRGN:
                    hRgn = CreateRectRgn(0, 0, 50, 50);
                    if (!hRgn)
                        info(FAIL, TEXT("HRGN creation returned NULL"));
                    if (!DeleteObject(hRgn))
                        info(FAIL, TEXT("HRGN destruction returned 0"));
                    break;
                case oHBRUSH:
                    hBrush = CreateSolidBrush(RGB(255, 0, 0));
                    if (!hBrush)
                        info(FAIL, TEXT("HBRUSH creation returned NULL"));
                    if (!DeleteObject(hBrush))
                        info(FAIL, TEXT("HBRUSH destruction returned 0"));
                    break;
                case oHFONT:
                    hFont = CreateFontIndirect(&myFont);
                    if (!hFont)
                        info(FAIL, TEXT("HFONT creation returned NULL"));
                    if (!DeleteObject(hFont))
                        info(FAIL, TEXT("HFONT destruction returned 0"));
                    break;
                case oHBITMAP:
                    hBmp = myCreateDIBSection(tempDC, (VOID **) & lpBits, pixelN, 200, 200, DIB_RGB_COLORS);
                    if (!hBmp)
                        info(FAIL, TEXT("HBMP creation returned NULL"));
                    if (!DeleteObject(hBmp))
                        info(FAIL, TEXT("HBMP destruction returned 0"));
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

    if (!DeleteObject(g_hRgn))
        info(FAIL, TEXT("HRGN destruction returned 0"));
    if (!DeleteObject(g_hBmp))
        info(FAIL, TEXT("HBMP destruction returned 0"));

    if (GetObjectType(g_hRgn))
        info(FAIL, TEXT("HRGN GetObjectType returned non-0"));
    if (GetObjectType(g_hBmp))
        info(FAIL, TEXT("HBMP GetObjectType returned non-0"));

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
        resultObj0 = GetStockObject(myStockObj[i]);
        SelectObject(hdc, resultObj0);
        result0 = DeleteObject(resultObj0);
        resultObj1 = SelectObject(hdc, resultObj0);

        if (!result0)
            info(FAIL, TEXT("Delete Failed on obj:%d"), i);

        if (myStockObj[i] != noPass && !(resultObj0 && resultObj0 == resultObj1))
            info(FAIL, TEXT("Two consecutive calls didn't jibe call1:0x%x call2:0x%x  obj:%d"), resultObj0, resultObj1, i);

        if (myStockObj[i] == noPass && (resultObj0 && resultObj0 == resultObj1))
            info(FAIL, TEXT("Two consecutive calls shouldn't have jibed call1:0x%x call2:0x%x  obj:%d"), resultObj0, resultObj1,
                 i);
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
    DIBSECTION bm;
    BITMAP  bmp;
    int     result;

    //TBITMAP  hBmp  = myCreateDIBSection(hdc,(VOID**)&lpBits,1,50,-40, DIB_RGB_COLORS);

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
    bmi.bmih.biBitCount = 1;

    bmi.bmih.biCompression = BI_RGB;
    setRGBQUAD(&bmi.ct[0], 0x0, 0x0, 0x0);
    setRGBQUAD(&bmi.ct[1], 0x55, 0x55, 0x55);
    setRGBQUAD(&bmi.ct[2], 0xaa, 0xaa, 0xaa);
    setRGBQUAD(&bmi.ct[3], 0xff, 0xff, 0xff);
    hBmp = CreateDIBSection(hdc, (LPBITMAPINFO) & bmi, DIB_RGB_COLORS, (VOID **) & lpBits, NULL, NULL);
    passCheck(hBmp == NULL, 0, TEXT("CreateDIBSection returned NULL"));

    result = GetObject(hBmp, sizeof (DIBSECTION), (LPBYTE) & bm);
    passCheck(result > 0, 1, TEXT("GetObject(hBmp, sizeof(DIBSECTION), (LPBYTE)&bm)"));

    if (bm.dsBm.bmWidth != 50 || bm.dsBm.bmHeight != 40)
        info(ECHO, TEXT("GetObject returned: h:%d w:%d  expected: h:40 w:50"), bm.dsBm.bmWidth, bm.dsBm.bmHeight);

    result = GetObject(hBmp, sizeof (BITMAP), (LPBYTE) & bmp);
    passCheck(result > 0, 1, TEXT("GetObject(hBmp, sizeof(BITMAP), (LPBYTE)&bmp)"));

    if (bmp.bmBits == NULL)
        info(FAIL, TEXT("GetObject returned NULL for bitmap.bmBits"));

    DeleteObject(hBmp);
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
        hBmp = myCreateBitmap(50, 40, 1, bitDepths[i], NULL);
        result = GetObject(hBmp, sizeof (BITMAP), (LPBYTE) & bmp);
        passCheck(result > 0, 1, TEXT("GetObject(hBmp, sizeof(BITMAP), (LPBYTE)&bmp)"));
        if (bmp.bmWidth != 50 || bmp.bmHeight != 40 || bmp.bmBitsPixel != bitDepths[i] || bmp.bmBits != NULL || bmp.bmPlanes != 1)
            info(ECHO, TEXT("GetObject returned: h:%d w:%d d:%d bits:%d planes:%d  expected: h:40 w:50 d:%d bits:0 planes:1"), bmp.bmWidth, bmp.bmHeight, bmp.bmBitsPixel, bmp.bmBits, bmp.bmPlanes, bitDepths[i]);
        DeleteObject(hBmp);
    }

    hBmp = CreateCompatibleBitmap(hdc, 50, 40);
    result = GetObject(hBmp, sizeof (BITMAP), (LPBYTE) & bmp);
    passCheck(result > 0, 1, TEXT("GetObject(hBmp, sizeof(BITMAP), (LPBYTE)&bmp)"));
    if (bmp.bmWidth != 50 || bmp.bmHeight != 40 || bmp.bmBitsPixel != myGetBitDepth() || bmp.bmBits != NULL || bmp.bmPlanes != 1)
        info(ECHO, TEXT("GetObject returned: h:%d w:%d d:%d bits:%d planes:%d  expected: h:40 w:50 d:%d bits:0 planes:1"), bmp.bmWidth, bmp.bmHeight, bmp.bmBitsPixel, bmp.bmBits, bmp.bmPlanes, myGetBitDepth());
    DeleteObject(hBmp);
    
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
        PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
        hgdiObj = GetStockObject(type[i]);
        if (!hgdiObj)
            info(ECHO, TEXT("GetStockObject Failed on obj[%d]"), i);
        SelectObject(hdc, hgdiObj);
        DeleteObject(hgdiObj);
        myReleaseDC(hdc);
    }
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

    // Depth
    // None

    SetSurfVerify(OldVerify);

    return getCode();
}

//**********************************************************************************
TESTPROCAPI GetCurrentObject_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2DObject(EGetCurrentObject);
    GetCurrentObjectTest(EGetCurrentObject);

    // Depth
    // None

    return getCode();
}

//**********************************************************************************
TESTPROCAPI GetObject_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2DObject(EGetObject);
    GetObjectTest(EGetObject);

    // Depth
    GetObjectDIBTest(EGetObject);
    GetObjectBMPTest(EGetObject);

    return getCode();
}

//**********************************************************************************
TESTPROCAPI GetObjectType_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2DObject(EGetObjectType);
    GetObjectTypeTest(EGetObjectType);

    // Depth
    // None

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

    // Breadth
    passNull2DObject(ESelectObject);
    SimpleSelectObjectTest(ESelectObject);
    SelectObjectTest(ESelectObject);

    // Depth
    // None

    return getCode();
}
