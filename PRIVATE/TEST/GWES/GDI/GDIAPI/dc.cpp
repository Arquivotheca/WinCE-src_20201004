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

   dc.cpp 

Abstract:

   Gdi Tests: Device Context APIs

***************************************************************************/

#include "global.h"

/***********************************************************************************
***
***   Support Functions 
***
************************************************************************************/

// all of the supported objects
static HRGN     g_rghRgn[2], g_stockRgn;
static HBRUSH   g_rghBrush[2], g_stockBrush;
static TBITMAP  g_rghBmp[2], g_stockBmp;
static HFONT    g_rghFont[2], g_stockFont;
static HPALETTE g_rghPal[2], g_stockPal;

//**********************************************************************************
void
initObjectsDC(TDC hdc, int index)
{

    if (gSanityChecks)
        info(DETAIL, TEXT("Init Objects"));

    int     code = (index + 1) * 3;
    HRGN hrgnTmp = NULL;
    HBRUSH hbrTmp = NULL;
    HFONT hfntTmp = NULL;
    TBITMAP hbmpTmp = NULL;

    g_rghRgn[index] = (HRGN) NULL;
    g_rghBrush[index] = (HBRUSH) NULL;
    g_rghBmp[index] = (TBITMAP) NULL;
    g_rghFont[index] = (HFONT) NULL;

    // create all supported objects 
    g_rghRgn[index] = CreateRectRgn(0, 0, code, code);
    g_rghBrush[index] = CreateSolidBrush(RGB(code, code, code));
    g_rghBmp[index] = CreateCompatibleBitmap(hdc, code, code);

    LOGFONT myFont;

    myFont.lfHeight = code;
    myFont.lfWidth = code;
    myFont.lfEscapement = code;
    myFont.lfOrientation = code;
    myFont.lfWeight = code;
    myFont.lfItalic = 0;
    myFont.lfUnderline = 0;
    myFont.lfStrikeOut = 0;
    myFont.lfCharSet = ANSI_CHARSET;
    myFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
    myFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    myFont.lfQuality = DEFAULT_QUALITY;
    myFont.lfPitchAndFamily = DEFAULT_PITCH;
    myFont.lfFaceName[0] = NULL;
    g_rghFont[index] = CreateFontIndirect(&myFont);
    g_rghPal[index] = myCreatePal(hdc, code);

    // check for valid creation
    if (!g_rghRgn[index])
        info(FAIL, TEXT("HRGN creation returned NULL"));
    if (!g_rghBrush[index])
        info(FAIL, TEXT("HBRUSH creation returned NULL"));
    if (!g_rghBmp[index])
        info(FAIL, TEXT("HBMP creation returned NULL"));
    if (!g_rghFont[index])
        info(FAIL, TEXT("HFONT creation returned NULL"));
    if (!g_rghPal[index])
        info(FAIL, TEXT("HPALETTE creation returned NULL"));

    if (index == 0)
    {
        // Save the stock objects on call 0
        g_stockRgn = (HRGN) SelectObject(hdc, g_rghRgn[index]);
        if(GDI_ERROR == (unsigned long) g_stockRgn)
            info(FAIL, TEXT("Selection of region failed, index 0"));
        
        g_stockBrush = (HBRUSH) SelectObject(hdc, g_rghBrush[index]);
        if(!g_stockBrush)
            info(FAIL, TEXT("Selection of brush failed, index 0"));
        
        g_stockFont = (HFONT) SelectObject(hdc, g_rghFont[index]);
        if(!g_stockFont)
            info(FAIL, TEXT("Selection of font failedm, index 0"));
        
        g_stockBmp = (TBITMAP) SelectObject(hdc, g_rghBmp[index]);
        if(!g_stockBmp && isMemDC())
            info(FAIL, TEXT("Selection of bitmap failed, index 0"));
        if(g_stockBmp && !isMemDC())
            info(FAIL, TEXT("Selection of bitmap Succeeded on a primary surface, index 0"));
    }
    else
    {
        // the values returned by the second call should match the ones selected in the first,
        // except for regions.
        hrgnTmp = (HRGN) SelectObject(hdc, g_rghRgn[index]);
        if(GDI_ERROR == (unsigned long) hrgnTmp)
            info(FAIL, TEXT("Selection of region failed, index 1"));
        
        hbrTmp = (HBRUSH) SelectObject(hdc, g_rghBrush[index]);
        if(!hbrTmp)
            info(FAIL, TEXT("Selection of brush failed, index 1"));
        if(hbrTmp != g_rghBrush[0])
            info(FAIL, TEXT("Brush returned does not match previous brush selected, index 1"));
        
        hfntTmp = (HFONT) SelectObject(hdc, g_rghFont[index]);
        if(!hfntTmp)
            info(FAIL, TEXT("Selection of font failed, index 1"));
        if(hfntTmp != g_rghFont[0])
            info(FAIL, TEXT("Font returned does not match previous font selected, index 1"));
        
        hbmpTmp = (TBITMAP) SelectObject(hdc, g_rghBmp[index]);
        if(!hbmpTmp && isMemDC())
            info(FAIL, TEXT("Selection of bitmap failed, index 1"));
        if(hbmpTmp != g_rghBmp[0] && isMemDC())
            info(FAIL, TEXT("Bitmap returned does not match previous bitmap selected, index 1"));
        if(hbmpTmp && !isMemDC())
            info(FAIL, TEXT("Selection of bitmap succeeded on a primary surface, index 1"));
    }
}

//**********************************************************************************
void
checkObjectsDC(TDC hdc, int index)
{

    if (gSanityChecks)
        info(DETAIL, TEXT("check Objects"));

    int     code = (index + 1) * 3;

    LOGBRUSH logBrush;
    BITMAP  logBmp;
    LOGFONT logFont;

    HBRUSH  tempBrush;
    HFONT   tempFont;
    TBITMAP tempBmp;
    int     result0,
            result1;

    // brush
    tempBrush = (HBRUSH) GetCurrentObject(hdc, OBJ_BRUSH);
    result0 = GetObject(tempBrush, 0, NULL);
    result1 = GetObject(tempBrush, result0, (LPVOID) & logBrush);
    if (logBrush.lbColor != RGB(code, code, code))
        info(FAIL, TEXT("Brush not correctly recovered 0x%x != 0x%x"), logBrush.lbColor, RGB(code, code, code));

    // font
    tempFont = (HFONT) GetCurrentObject(hdc, OBJ_FONT);
    result0 = GetObject(tempFont, 0, NULL);
    result1 = GetObject(tempFont, result0, (LPVOID) & logFont);
    if (logFont.lfHeight != code)
        info(FAIL, TEXT("Font not correctly recovered %d"), logFont.lfHeight);

    if (isMemDC())
    {                           // !getDC(null) 
        // bitmap
        tempBmp = (TBITMAP) GetCurrentObject(hdc, OBJ_BITMAP);
        result0 = GetObject(tempBmp, 0, NULL);
        result1 = GetObject(tempBmp, result0, (LPVOID) & logBmp);
        if (logBmp.bmWidth != code)
            info(FAIL, TEXT("Bitmap not correctly recovered %d 0x%x"), logBmp.bmWidth, tempBmp);
    }
    else
        info(DETAIL, TEXT("Skipping Bitmap check in this mode"));

    // palette
    PALETTEENTRY outPal[256];
    HPALETTE tempPal;

    tempPal = (HPALETTE) GetCurrentObject(hdc, OBJ_PAL);
    GetPaletteEntries(tempPal, 0, 235, (LPPALETTEENTRY)outPal);
    for (int i = 10; i < 235; i++)
        if (outPal[i].peRed != i + code)
        {
            info(FAIL, TEXT("Palette not correctly recovered Pal [%d](%d %d %d) != (%d)"), i, outPal[i].peRed,
                 outPal[i].peGreen, outPal[i].peBlue, i + code);
            break;
        }
}

//**********************************************************************************
void
destroyObjectsDC(TDC hdc, int index)
{

    if (gSanityChecks)
        info(DETAIL, TEXT("Delete Objects"));

    //   No need to restore regions -- the "stockRgn" isn't even a region.
    //   SelectObject(hdc,(HRGN)     stockRgn); 
    if(index == 0)
    {
        if(!SelectObject(hdc, (HBRUSH) g_stockBrush))
            info(FAIL, TEXT("Failed to reselect the stock brush into the dc"));
        if( isMemDC() && !SelectObject(hdc, (TBITMAP) g_stockBmp))
            info(FAIL, TEXT("Failed to reselect the stock bitmap into the dc"));
        if(!SelectObject(hdc, (HFONT) g_stockFont))
            info(FAIL, TEXT("Failed to reselect the stock font into the dc"));
    }
    
    myDeletePal(hdc, g_rghPal[index]);
    // get rid of any created object
    if (!DeleteObject(g_rghRgn[index]))
        info(FAIL, TEXT("HRGN destruction returned 0"));
    if (!DeleteObject(g_rghBrush[index]))
        info(FAIL, TEXT("HBRUSH destruction returned 0"));

    if (!DeleteObject(g_rghBmp[index]) && isMemDC())
        info(FAIL, TEXT("HBMP[%d] destruction returned 0"), index);
    if (!DeleteObject(g_rghFont[index]))
        info(FAIL, TEXT("HFONT destruction returned 0"));
}

/***********************************************************************************
***
***   Passing NULL to CreateCompativeDC
***
************************************************************************************/

//**********************************************************************************
void
passCreateCompatibleDCNULL(int testFunc)
{

    info(ECHO, TEXT("%s: Passing NULL hdc handle"), funcName[testFunc]);

    mySetLastError();
    TDC     tempDC = CreateCompatibleDC((TDC) NULL);

    myGetLastError(NADA);

    if (!tempDC)
        info(FAIL, TEXT("Expected good hdc returned 0x%x"), tempDC);
    else
        DeleteDC(tempDC);
}

/***********************************************************************************
***
***   
***
************************************************************************************/

//**********************************************************************************
void
TestGetDCOrgEx(int testFunc)
{
#ifndef UNDER_CE
    info(ECHO, TEXT("%s: Testing"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    POINT   p = { -10, -10 };
    BOOL    result;

    info(DETAIL, TEXT("Part 0"));
    mySetLastError();
    result = GetDCOrgEx(hdc, &p);
    if (result != 1 || p.x != 0 || p.y != 0)
        info(FAIL, TEXT("passing valid DC got offset point x:%d y:%d result:%d"), p.x, p.y, result);
    myGetLastError(NADA);

    p.x = p.y = -10;
    info(DETAIL, TEXT("Part 1"));
    mySetLastError();
    result = GetDCOrgEx(NULL, &p);

    if (result != 0 || p.x != -10 || p.y != -10)
        info(FAIL, TEXT("passing NULL DC got offset point x:%d y:%d result:%d"), p.x, p.y, result);

    if (result != 0)
        info(FAIL, TEXT("passing NULL DC got offset point result:%d"), result);
    myGetLastError(NADA);

    p.x = p.y = -10;
    info(DETAIL, TEXT("Part 2"));
    mySetLastError();
    result = GetDCOrgEx((TDC) 0x911, &p);

    if (result != 0 || p.x != -10 || p.y != -10)
        info(FAIL, TEXT("passing NULL DC got offset point x:%d y:%d result:%d"), p.x, p.y, result);

    if (result != 0)
        info(FAIL, TEXT("passing NULL DC got offset point result:%d"), result);
    myGetLastError(NADA);
    myReleaseDC(hdc);
#endif // UNDER_CE
}

/***********************************************************************************
***
***   Stress test DeleteDC
***
************************************************************************************/

//**********************************************************************************
void
StressPairs(int testFunc)
{

    BOOL    bOldVerify = SetSurfVerify(FALSE);

    info(ECHO, TEXT("%s: Doing 1 minute of create and DeleteDC"), funcName[testFunc]);

    TDC     hdc;
    // run the test for 1 minute
    DWORD EndTime = (GetTickCount() + 60000);
    int i=0;
    
    for(i =0; GetTickCount() <= EndTime; i++)
    {
        if (testFunc == ECreateCompatibleDC)
        {
            mySetLastError();
            hdc = myCreateCompatibleDC(NULL);
            myGetLastError(NADA);

            mySetLastError();
            myDeleteDC(hdc);
            myGetLastError(NADA);
        }
        else
        {
            mySetLastError();
            hdc = myGetDC();
            myGetLastError(NADA);

            mySetLastError();
            myReleaseDC(hdc);
            myGetLastError(NADA);
        }
    }

    info(DETAIL, TEXT("Did %d Get/Release pairs"), i);
    SetSurfVerify(bOldVerify);
}

/***********************************************************************************
***
***   Program Error
***
************************************************************************************/

//**********************************************************************************
void
ProgramError(int testFunc)
{
    
    info(ECHO, TEXT("%s: Program Error test."), funcName[testFunc]);

    //This test case is inproperly handling dc's, which cannot be handled properly by
    // the TDC wrapper, so we must fall back to using HDC's
    HDC     hdc = GetDC(NULL);

    if (!hdc)
        info(FAIL, TEXT("GetDC failed"));

    if (!DeleteDC(hdc))
    {
        info(FAIL, TEXT("DeleteDC failed"));
    }
    if (ReleaseDC(NULL, hdc))
        info(DETAIL, TEXT("* RelaseDC should have failed"));

}

/***********************************************************************************
***
***   Save\Restore DC Testing
***
************************************************************************************/

//**********************************************************************************
void
SaveSomeDCs(TDC hdc, int num)
{

    int     sResult0;

    while (RestoreDC(hdc, -1))
        ;                       // remove all saved DCs

    for (int i = 0; i < num; i++)
    {
        mySetLastError();
        sResult0 = SaveDC(hdc);
        myGetLastError(NADA);

        if (sResult0 != i + 1)
            info(FAIL, TEXT("SaveDC returned: %d not expected:%d"), sResult0, i + 1);
    }
}

//**********************************************************************************
void
SaveRestoreDCPairs(int testFunc)
{

    info(ECHO, TEXT("%s: Save & Restore DC Testing"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    int     i,
            rResult0;

    info(DETAIL, TEXT("Part 0"));
    // try -1 restore sequence
    SaveSomeDCs(hdc, testCycles);
    for (i = 0; i < testCycles + 1; i++)
    {
        mySetLastError();
        rResult0 = RestoreDC(hdc, -1);
        Rectangle(hdc, 0, 0, 50, 50);
        myGetLastError((i < testCycles) ? NADA : ERROR_INVALID_PARAMETER);
        if (!rResult0 && i != testCycles)
            info(FAIL, TEXT("RestoreDC (hdc,-1) returned: %d not expected: 1"), rResult0);
        else if (rResult0 && i == testCycles)
            info(FAIL, TEXT("RestoreDC (hdc,-1) past last index returned: %d expected: 0"), rResult0);
    }

    info(DETAIL, TEXT("Part 1"));
    // try numerical sequence
    SaveSomeDCs(hdc, testCycles);
    // we have to go backward here
    for (i = testCycles; i > -1; i--)
    {
        mySetLastError();
        rResult0 = RestoreDC(hdc, i);
        Rectangle(hdc, 0, 0, 50, 50);
        myGetLastError((i > 0) ? NADA : ERROR_INVALID_PARAMETER);
        if (!rResult0 && i != 0)
            info(FAIL, TEXT("RestoreDC (hdc,%d) returned: %d not expected: 1"), i, rResult0);
        else if (rResult0 && i == 0)
            info(FAIL, TEXT("RestoreDC (hdc,%d) past last index returned: %d expected: 0"), i, rResult0);
    }

    info(DETAIL, TEXT("Part 2"));
    // try -1 accessing top of stack when none exists 
    SaveSomeDCs(hdc, testCycles);
    // this should remove everything above it on stack
    mySetLastError();
    rResult0 = RestoreDC(hdc, 1);
    myGetLastError(NADA);
    // now try to get items that were deleted
    for (i = 0; i < testCycles; i++)
    {
        mySetLastError();
        rResult0 = RestoreDC(hdc, -1);
        Rectangle(hdc, 0, 0, 50, 50);
        myGetLastError(ERROR_INVALID_PARAMETER);
        if (rResult0)
            info(FAIL, TEXT("RestoreDC (hdc,%d) returned:%d after top of stack - expected failure"), i, rResult0);
    }

    info(DETAIL, TEXT("Part 3"));
    // try numerically accessing top of stack when none exists 
    SaveSomeDCs(hdc, testCycles);
    // this should remove everything above it on stack
    mySetLastError();
    rResult0 = RestoreDC(hdc, 1);
    myGetLastError(NADA);
    // now try to get items that were deleted
    for (i = testCycles; i > 0; i--)
    {
        mySetLastError();
        rResult0 = RestoreDC(hdc, i);
        myGetLastError(ERROR_INVALID_PARAMETER);
        if (rResult0)
            info(FAIL, TEXT("RestoreDC (hdc,%d) returned:%d after top of stack - expected failure"), i, rResult0);
    }
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Test Recovering Objects
***
************************************************************************************/

//**********************************************************************************
void
RecoverObjectsTest(int testFunc)
{

    if (0 != testFunc)
        info(ECHO, TEXT("%s: Recovering SaveDC Objects 1"), funcName[testFunc]);
    TDC     hdc = myGetDC();
    int     sResult,
            rResult;

    initObjectsDC(hdc, 0);
    checkObjectsDC(hdc, 0);

    sResult = SaveDC(hdc);
    if (sResult <= 0)
        info(FAIL, TEXT("SaveDC (hdc) => %d"), sResult);
    
    initObjectsDC(hdc, 1);
    checkObjectsDC(hdc, 1);

    mySetLastError();
    rResult = RestoreDC(hdc, -1);
    myGetLastError(NADA);

    if (rResult != 1)
        info(FAIL, TEXT("RestoreDC (hdc,-1) => %d"), rResult);

    checkObjectsDC(hdc, 0);

    destroyObjectsDC(hdc, 0);
    destroyObjectsDC(hdc, 1);

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Check Off Screen Drawing
***
************************************************************************************/

//**********************************************************************************
void
LogWait(LPTSTR string)
{
    info(DETAIL, TEXT("Following call: <%s>"), string);
    //Sleep(500);  No longer needed - no longer crashes
}

//**********************************************************************************
void
TestCreateCompatibleDC(int testFunc)
{

    info(ECHO, TEXT("%s - Offscreen Drawing Test"), funcName[testFunc]);

    // set up vars
    TDC     hdc = myGetDC();
    TDC     offDC = myCreateCompatibleDC(hdc);
    TBITMAP hBmp,
            stockBmp;
    DWORD   dwError;

    if (!offDC)
    {
        dwError = GetLastError();
        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("CreateCompatibleDC() failed: OUT of Memory: err=%ld"), dwError);
        else
            info(FAIL, TEXT("CreateCompatibleDC() failed: err=%ld"), dwError);
        goto LReleaseDC;
    }

    hBmp = CreateCompatibleBitmap(hdc, zx / 2, zy);
    if (!hBmp)
    {
        dwError = GetLastError();
        if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("CreateCompatibleBitmap(%d %d): failed:  OUT OF MEMORY"), zx / 2, zy);
        else
            info(FAIL, TEXT("CreateCompatibleBitmap(%d %d): failed: err=%ld"), zx / 2, zy, dwError);
        goto LDeleteDC;
    }

    stockBmp = (TBITMAP) SelectObject(offDC, hBmp);
    LogWait(TEXT("Prior to any calls"));
    myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);
    LogWait(TEXT("myPatBlt(hdc,   0, 0,zx,   zy, WHITENESS)"));
    myPatBlt(offDC, 0, 0, zx / 2, zy, WHITENESS);
    LogWait(TEXT("myPatBlt(offDC, 0, 0,zx/2, zy, WHITENESS)"));

    //BitBlt
    BitBlt(offDC, 10, 10, 10, 10, offDC, 10, 10, SRCCOPY);  // off to off
    LogWait(TEXT("BitBlt(offDC,10,10,10,10,offDC,10,10,SRCCOPY)"));
    BitBlt(hdc, 10, 20, 10, 10, offDC, 10, 10, SRCCOPY);    // off to on
    LogWait(TEXT("BitBlt(hdc,10,20,10,10,offDC,10,10,SRCCOPY)"));
    BitBlt(offDC, 10, 30, 10, 10, hdc, 10, 10, SRCCOPY);    // on to off
    LogWait(TEXT("BitBlt(offDC,10,30,10,10,hdc,10,10,SRCCOPY)"));

    //Get/SetPixel
    SetPixel(offDC, 35, 35, RGB(0, 0, 0));
    LogWait(TEXT("SetPixel(offDC,35,35,RGB(0,0,0))"));
    GetPixel(offDC, 35, 35);
    LogWait(TEXT("GetPixel(offDC,35,35)"));
    SetPixel(hdc, 35, 35, RGB(0, 0, 0));
    LogWait(TEXT("SetPixel(hdc,35,35,RGB(0,0,0))"));
    GetPixel(hdc, 35, 35);
    LogWait(TEXT("GetPixel(hdc,35,35)"));

    //PatBlt
    PatBlt(offDC, 40, 40, 10, 10, BLACKNESS);
    LogWait(TEXT("PatBlt(offDC,40,40,10,10,BLACKNESS)"));
    PatBlt(hdc, 40, 40, 10, 10, BLACKNESS);
    LogWait(TEXT("PatBlt(hdc,40,40,10,10,BLACKNESS)"));

    /* Removed from GDI 11/13/95
       //Rectangle
       SelectObject(offDC,GetStockObject(BLACK_BRUSH));
       Rectangle(offDC,50,50,60,60);
       LogWait(L"Rectangle(offDC,50,50,60,60)");
       SelectObject(hdc,GetStockObject(BLACK_BRUSH));
       Rectangle(hdc,50,50,60,60);
       LogWait(L"Rectangle(hdc,50,50,60,60)");
     */

    // copy squares from offscreen to top screen
    myBitBlt(hdc, zx / 2, 0, zx / 2, zy, offDC, 0, 0, SRCCOPY);

    CheckScreenHalves(hdc);

    SelectObject(offDC, stockBmp);
    DeleteObject(hBmp);

  LDeleteDC:
    myDeleteDC(offDC);
  LReleaseDC:
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Globe Test
***
************************************************************************************/

//**********************************************************************************
void
globeTest(int testFunc)
{

    info(ECHO, TEXT("%s - Globe Test"), funcName[testFunc]);

    TDC     aDC = myGetDC(),
            compDC[MAX_GLOBE];
    TBITMAP hBmp[MAX_GLOBE],
            stockBmp[MAX_GLOBE];
    POINT   Globes = { zx / 2 / GLOBE_SIZE, zy / GLOBE_SIZE };
    TCHAR   szBuf[30];
    int     i,
            j;
    HPALETTE hPal = myCreateNaturalPal(aDC);

    for (i = 0; i < MAX_GLOBE; i++)
    {
        _stprintf(szBuf, TEXT("GLOBE%d"), i);
        hBmp[i] = myLoadBitmap(globalInst, szBuf);
        compDC[i] = CreateCompatibleDC(aDC);
        if (!hBmp[i])
            info(FAIL, TEXT("Bmp %d failed"), i);
        if (!compDC[i])
            info(FAIL, TEXT("CompDC %d failed"), i);
        stockBmp[i] = (TBITMAP) SelectObject(compDC[i], hBmp[i]);
    }

    PatBlt(aDC, 0, 0, zx, zy, BLACKNESS);

    for (int z = 0; z <= MAX_GLOBE * 15; z++)
    {
        if (z == 1)
            myBitBlt(aDC, zx / 2, 0, zx / 2, zy, aDC, 0, 0, SRCCOPY);
        for (i = 0; i < Globes.x; i++)
            for (j = 0; j < Globes.y; j++)
            {
                if ((i + j) % 2 == 0)
                    BitBlt(aDC, i * GLOBE_SIZE, j * GLOBE_SIZE, GLOBE_SIZE, GLOBE_SIZE, compDC[(i + j + z) % MAX_GLOBE], 0, 0,
                           SRCCOPY);
            }
        Sleep(50);
    }

    CheckScreenHalves(aDC);

    for (i = 0; i < MAX_GLOBE; i++)
    {
        SelectObject(compDC[i], stockBmp[i]);
        DeleteObject(hBmp[i]);
        DeleteDC(compDC[i]);
    }
    myDeletePal(aDC, hPal);
    myReleaseDC(aDC);
}

/***********************************************************************************
***
***   Paris Test
***
************************************************************************************/

//**********************************************************************************
void
parisTest(int testFunc)
{

    info(ECHO, TEXT("%s - Paris Test"), funcName[testFunc]);

    // if the screen/surface is smaller than the bitmaps we're testing, don't test, not enough room.
    if(SetWindowConstraint(3*PARIS_X, PARIS_Y))
    {
        TDC     aDC = myGetDC(),
                compDC[MAX_PARIS];
        TBITMAP hBmp[MAX_PARIS],
                stockBmp[MAX_PARIS];
        TCHAR   szBuf[30];
        int     i,
                z;
        POINT   Faces = { zx / 2 - (zx / 2 + PARIS_X) / 2, zy - (zy + PARIS_Y) / 2 };
        HPALETTE hPal[MAX_PARIS],
                aPal = myCreateNaturalPal(aDC);

        // make sure our width is divisable by 4
        zx = (zx/4)*4;

        for (i = 0; i < MAX_PARIS; i++)
        {
            _stprintf(szBuf, TEXT("PARIS%d"), i);
            compDC[i] = CreateCompatibleDC(aDC);
            hBmp[i] = myLoadBitmap(globalInst, szBuf);
            if (!hBmp[i])
                info(FAIL, TEXT("Bmp %d failed"), i);
            if (!compDC[i])
                info(FAIL, TEXT("CompDC %d failed"), i);
            hPal[i] = myCreateNaturalPal(compDC[i]);
            stockBmp[i] = (TBITMAP) SelectObject(compDC[i], hBmp[i]);
        }

        PatBlt(aDC, 0, 0, zx, zy, BLACKNESS);

        BitBlt(aDC, Faces.x, Faces.y, PARIS_X, PARIS_Y, compDC[0], 0, 0, SRCCOPY);
        myBitBlt(aDC, zx / 2, 0, zx / 2, zy, aDC, 0, 0, SRCCOPY);

        myBitBlt(aDC, 0, 0, 30, 30, aDC, 0, 0, SRCCOPY);

        for (i = 0; i < 10; i++)
        {
            for (z = 0; z < MAX_PARIS; z++)
            {
                BitBlt(aDC, Faces.x, Faces.y, PARIS_X, PARIS_Y, compDC[z], 0, 0, SRCCOPY);
                Sleep(50);
            }
            for (z = MAX_PARIS - 2; z > 0; z--)
            {
                BitBlt(aDC, Faces.x, Faces.y, PARIS_X, PARIS_Y, compDC[z], 0, 0, SRCCOPY);
                Sleep(50);
            }
        }

        BitBlt(aDC, Faces.x, Faces.y, PARIS_X, PARIS_Y, compDC[0], 0, 0, SRCCOPY);
        Sleep(50);

        CheckScreenHalves(aDC);

        for (i = 0; i < MAX_PARIS; i++)
        {
            SelectObject(compDC[i], stockBmp[i]);
            DeleteObject(hBmp[i]);
            myDeletePal(compDC[i], hPal[i]);
            DeleteDC(compDC[i]);
        }

        myDeletePal(aDC, aPal);
        myReleaseDC(aDC);
        SetWindowConstraint(0,0);
    }
    else info(DETAIL, TEXT("Screen too small for test, skipping"));
}

/***********************************************************************************
***
***   Bad Param
***
************************************************************************************/

//**********************************************************************************
void
BadParam(int testFunc)
{

    info(ECHO, TEXT("%s - Bad Param"), funcName[testFunc]);

    HDC     badDC = (HDC) badRandomNum(),
            resultDC = NULL;

    resultDC = CreateCompatibleDC(badDC);
    if (resultDC)
        info(FAIL, TEXT("CreateCompatibleDC should have failed given bogus DC 0x%x"), (int) badDC);
}

//**********************************************************************************
void
TestRestoreDC0(int testFunc)
{

    info(ECHO, TEXT("%s - Call RestoreDC(hdc, 0)"), funcName[testFunc]);

    int     iDCSaved;
    BOOL    bDCRestored;
    TDC     hdc = myGetDC();
    DWORD   dw;

    iDCSaved = SaveDC(hdc);
    info(DETAIL, TEXT("SaveDC() return %d"), iDCSaved);
    SetLastError(0);
    bDCRestored = RestoreDC(hdc, 0);
    info(DETAIL, TEXT("RestoreDC(hdc,0) return %d"), bDCRestored);
    passCheck(bDCRestored, 0, TEXT("RestoreDC(hdc, 0) expect return 0"));
    dw = GetLastError();
    if (dw != ERROR_INVALID_PARAMETER)
        info(FAIL, TEXT("RestoreDC(hdc, 0): expect err=%ld, but GLE return %d"), ERROR_INVALID_PARAMETER, dw);
    bDCRestored = RestoreDC(hdc, iDCSaved);
    if (0 == bDCRestored)
        info(FAIL, TEXT("RestoreDC(hdc, <valid>): expect nonzero return"));
    myReleaseDC(hdc);
}

//**********************************************************************************
void
TestSaveDCNULL(int testFunc)
{

    info(ECHO, TEXT("%s - pass (hdc=NULL)"), funcName[testFunc]);
    int     iDC;
    DWORD   dw;

    SetLastError(0);
    iDC = SaveDC((TDC) NULL);
    passCheck(iDC, 0, TEXT("SaveDC(NULL) expect return 0"));
    dw = GetLastError();
    if (dw != ERROR_INVALID_HANDLE)
        info(FAIL, TEXT("SaveDC(NULL): expect err=%ld, but GLE return %d"), ERROR_INVALID_HANDLE, dw);

    iDC = RestoreDC((TDC) NULL, 0);
    passCheck(iDC, 0, TEXT("RestoreDC(hdc=NULL, 0) expect return 0"));
    dw = GetLastError();
    if (dw != ERROR_INVALID_HANDLE)
        info(FAIL, TEXT("RestoreDC(NULL,0): expect err=%ld, but GLE return %d"), ERROR_INVALID_HANDLE, dw);
}

void ScrollDCBadParam()
{
    info(ECHO, TEXT("Checking ScrollDC bad parameters"));
    
    TDC hdc = myGetDC();
    PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
    PatBlt(hdc, zx/3, zy/3, zx/3, zy/3, BLACKNESS);

    if(ScrollDC((HDC) NULL, 0, 0, NULL, NULL, NULL, NULL))
        info(FAIL, TEXT("ScrollDC succeeded with NULL handle"));
    // nt allows diagonal scrolling and doesn't fail on bad hdc's
    #ifdef UNDER_CE
    if(ScrollDC((HDC) -1, 0, 0, NULL, NULL, NULL, NULL))
        info(FAIL, TEXT("ScrollDC succeeded with -1 handle"));
    if(ScrollDC((HDC) 0x0BAD0BAD, 0, 0, NULL, NULL, NULL, NULL))
        info(FAIL, TEXT("ScrollDC succeeded with 0x0BAD0BAD handle"));

    if(ScrollDC(hdc, 1, 1, NULL, NULL, NULL, NULL))
        info(FAIL, TEXT("ScrollDC(HDC, 1, 1, NULL, NULL, NULL, NULL) succeeded"));

    if(ScrollDC(hdc, -1, 1, NULL, NULL, NULL, NULL))
        info(FAIL, TEXT("ScrollDC(HDC, -1, 1, NULL, NULL, NULL, NULL) succeeded"));

    if(ScrollDC(hdc, 1, -1, NULL, NULL, NULL, NULL))
        info(FAIL, TEXT("ScrollDC(HDC, 1, -1, NULL, NULL, NULL, NULL) succeeded"));

    if(ScrollDC(hdc, -1, -1, NULL, NULL, NULL, NULL))
        info(FAIL, TEXT("ScrollDC(HDC, -1, -1, NULL, NULL, NULL, NULL) succeeded"));
    #endif
    PatBlt(hdc, zx/3, zy/3, zx/3, zy/3, WHITENESS);
    CheckAllWhite(hdc, 0, 1, 0, 0);
    myReleaseDC(hdc);
}

void SimpleScrollDCTest()
{
    info(ECHO, TEXT("SimpleScrollDCTest"));
    TDC hdc = myGetDC();
    HRGN hrgn = CreateRectRgn(0, 0, zx, zy);

    // this test relies on the clip region to drag
    // the color around on the screen

    SelectClipRgn(hdc, hrgn);
    
    int stepy = 0;
    int stepx = 0;
    // zx/2+1 just in case we're an odd sized window, 
    // we want to go all the way to the edge
    int rcWidth = zx/2 + 1;
    int rcHeight = zy/2 + 1;
 
    if(!hdc)
    {
        info(ABORT, TEXT("dc creation failed"));
        return;
    }
    PatBlt(hdc, 0, 0, zx, zy, BLACKNESS);
    
    PatBlt(hdc, 1, 1, rcWidth, rcHeight, WHITENESS);

    info(DETAIL, TEXT("Using ScrollDc to fill the surface with white"));

    // scroll us to into the upper left corner
    if(!ScrollDC(hdc, -1, 0, NULL, NULL, NULL, NULL))
        info(FAIL, TEXT("ScrollDC call failed"));
    // scroll us to into the upper left corner
    if(!ScrollDC(hdc, 0, -1, NULL, NULL, NULL, NULL))
        info(FAIL, TEXT("ScrollDC call failed"));
    
    // scroll the rectangle to the far right 
    for(stepx=0; stepx <= 2; stepx++)
    {
       if(!ScrollDC(hdc, zx/4, 0, NULL, NULL, NULL, NULL))
            info(FAIL, TEXT("ScrollDC call failed"));
    }
    
    for(stepy=0; stepy<= 2; stepy++)
    {
       if(!ScrollDC(hdc, 0, zy/4, NULL, NULL, NULL, NULL))
            info(FAIL, TEXT("ScrollDC call failed"));
    }

    // we should have scrolled makeing the whole screen white
    CheckAllWhite(hdc, 0, 1, 0, 0);

    PatBlt(hdc, 0, 0, zx, zy, BLACKNESS);
    
    PatBlt(hdc, zx-rcWidth-1, zy-rcHeight-1, rcWidth, rcHeight, WHITENESS);

    info(DETAIL, TEXT("Using ScrollDc to fill the surface with white"));

    // scroll us to into the lower right corner
    if(!ScrollDC(hdc, 1, 0, NULL, NULL, NULL, NULL))
        info(FAIL, TEXT("ScrollDC call failed"));
    // scroll us to into the lower right corner
    if(!ScrollDC(hdc, 0, 1, NULL, NULL, NULL, NULL))
        info(FAIL, TEXT("ScrollDC call failed"));
    
    // scroll the rectangle to the far right 
    for(stepx=0; stepx <= 2; stepx++)
    {
       if(!ScrollDC(hdc, -zx/4, 0, NULL, NULL, NULL, NULL))
            info(FAIL, TEXT("ScrollDC call failed"));
    }
    
    for(stepy=0; stepy<= 2; stepy++)
    {
       if(!ScrollDC(hdc, 0, -zy/4, NULL, NULL, NULL, NULL))
            info(FAIL, TEXT("ScrollDC call failed"));
    }

    // we should have scrolled makeing the whole screen white
    CheckAllWhite(hdc, 0, 1, 0, 0);

    SelectClipRgn(hdc, NULL);
    DeleteObject(hrgn);
    myReleaseDC(hdc);
}

void ScrollDCTest()
{
    info(ECHO, TEXT("ScrollDCTest"));

    TDC hdc = myGetDC();
    int stepy = 0;
    int stepx = 0;
    int nx = 0, ny = 0;
    int nIteration = 0;
    RECT rcInvalidated = {0};
    HRGN hrgn = CreateRectRgn(0, 0, zx, zy);

    if (DCFlag == EGDI_Default)
    {
        SelectClipRgn(hdc, hrgn);
    }
    
    for(nIteration = 0; nIteration < 2; nIteration++)
    {
        RECT rcClipRect = { 0, 0, zx, zy};
        RECT *lprcClipRect = NULL;
        
        if(1 == nIteration)
        {
            info(DETAIL, TEXT("ScrollDc with a clip rectangle"));
            rcClipRect.left = rand() % zx/4;
            rcClipRect.top = rand() % zy/4;
            // 0 - 3zn/4 + zn/4 to be in the bottom/right 3/4th of the screen.
            // +-6 so bottom - top - 4 != 0
            rcClipRect.right = (rand() % (3*zx/4 - 6)) + zx/4 + 6;
            rcClipRect.bottom = (rand() % (3* zy/4 - 6)) + zy/4 + 6;
            lprcClipRect = & rcClipRect;
        }
        else info(DETAIL, TEXT("ScrollDc without a clip rectangle"));

        // -4 /+ 1 so the width/height of the box to draw isn't bigger than the
        // clip rect, 4 so it doesn't step off when we scroll, which will cause a failure.
        int rcWidth = (rand() % (rcClipRect.right - rcClipRect.left - 4)) + 1;
        int rcHeight = (rand() % (rcClipRect.bottom - rcClipRect.top - 4)) + 1;
        int rcTravelx = rcClipRect.right - rcClipRect.left - rcWidth -1;
        int rcTravely = rcClipRect.bottom - rcClipRect.top - rcHeight -1;
        HBRUSH hbr = NULL;
        
        if(!hdc)
        {
            info(ABORT, TEXT("dc creation failed"));
            return;
        }
        PatBlt(hdc, 0, 0, zx, zy, WHITENESS);

        hbr = CreateSolidBrush(randColorRef());
        if(hbr)
        {
            RECT rc = { rcClipRect.left + 1, rcClipRect.top + 1, rcClipRect.left + rcWidth, rcClipRect.top + rcHeight};
            FillRect(hdc, &rc, hbr);
            DeleteObject(hbr);
        }

        for(stepx=0, nx=0; nx + stepx + 1 < rcTravelx; nx += stepx++)
        {
           if(!ScrollDC(hdc, stepx, 0, NULL, lprcClipRect, NULL, &rcInvalidated))
               info(FAIL, TEXT("ScrollDC call failed"));
           if(!stepx && rcInvalidated.left !=0 && rcInvalidated.right !=0 && rcInvalidated.top != 0 && rcInvalidated.bottom !=0)
               info(FAIL, TEXT("ScrollDC set the invalidated when no scroll performed t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
           if(stepx && (rcInvalidated.left != rcClipRect.left || rcInvalidated.right != rcClipRect.left + stepx || rcInvalidated.top != rcClipRect.top || rcInvalidated.bottom != rcClipRect.bottom))
               info(FAIL, TEXT("ScrollDC didn't set the invalidated rect correct t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
        }

        hbr = CreateSolidBrush(randColorRef());
        if(hbr)
        {
            RECT rc = { rcClipRect.left + nx + 1, rcClipRect.top + ny + 1, rcClipRect.left + rcWidth + nx + 1, rcClipRect.top + rcHeight + ny + 1};
            FillRect(hdc, &rc, hbr);
            DeleteObject(hbr);
        }
        
        for(stepy=0, ny = 0; ny + stepy + 1 < rcTravely; ny += stepy++)
        {
           if(!ScrollDC(hdc, 0, stepy, NULL, lprcClipRect, NULL, &rcInvalidated))
                info(FAIL, TEXT("ScrollDC call failed"));
           if(!stepy && rcInvalidated.left !=0 && rcInvalidated.right !=0 && rcInvalidated.top != 0 && rcInvalidated.bottom !=0)
               info(FAIL, TEXT("ScrollDC set the invalidated when no scroll performed t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
           if(stepy && (rcInvalidated.left != rcClipRect.left || rcInvalidated.right != rcClipRect.right  || rcInvalidated.top != rcClipRect.top || rcInvalidated.bottom != rcClipRect.top + stepy))
               info(FAIL, TEXT("ScrollDC didn't set the invalidated rect correct t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);           
        }

        hbr = CreateSolidBrush(randColorRef());
        if(hbr)
        {
            RECT rc = { rcClipRect.left + nx + 1, rcClipRect.top + ny + 1, rcClipRect.left + rcWidth + nx + 1, rcClipRect.top + rcHeight + ny + 1};
            FillRect(hdc, &rc, hbr);
            DeleteObject(hbr);
        }
        
        for(stepx=0; nx - stepx >= 0; nx -= stepx++)
        {
           if(!ScrollDC(hdc, -stepx, 0, NULL, lprcClipRect, NULL, &rcInvalidated))
                info(FAIL, TEXT("ScrollDC call failed"));
           if(!stepx && rcInvalidated.left !=0 && rcInvalidated.right !=0 && rcInvalidated.top != 0 && rcInvalidated.bottom !=0)
               info(FAIL, TEXT("ScrollDC set the invalidated when no scroll performed t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
           if(stepx && (rcInvalidated.left != rcClipRect.right - stepx || rcInvalidated.right != rcClipRect.right  || rcInvalidated.top != rcClipRect.top || rcInvalidated.bottom != rcClipRect.bottom))
               info(FAIL, TEXT("ScrollDC didn't set the invalidated rect correct t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);      
        }

        hbr = CreateSolidBrush(randColorRef());
        if(hbr)
        {
            RECT rc = { rcClipRect.left + nx + 1, rcClipRect.top + ny + 1, rcClipRect.left + rcWidth + nx + 1, rcClipRect.top + rcHeight + ny + 1};
            FillRect(hdc, &rc, hbr);
            DeleteObject(hbr);
        }
        
        for(stepy=0; ny - stepy >= 0; ny -= stepy++)
        {
           if(!ScrollDC(hdc, 0, -stepy, NULL, lprcClipRect, NULL, &rcInvalidated))
                info(FAIL, TEXT("ScrollDC call failed"));
           if(!stepy && rcInvalidated.left !=0 && rcInvalidated.right !=0 && rcInvalidated.top != 0 && rcInvalidated.bottom !=0)
               info(FAIL, TEXT("ScrollDC set the invalidated when no scroll performed t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
           if(stepy && (rcInvalidated.left != rcClipRect.left || rcInvalidated.right != rcClipRect.right  || rcInvalidated.top != rcClipRect.bottom - stepy || rcInvalidated.bottom != rcClipRect.bottom))
               info(FAIL, TEXT("ScrollDC didn't set the invalidated rect correct t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);               
        }

        PatBlt(hdc, rcClipRect.left + 1, rcClipRect.top + 1, rcClipRect.left + rcWidth, rcClipRect.top + rcHeight, WHITENESS);

        CheckAllWhite(hdc, 0, 1, 0, 0);
    }

    SelectClipRgn(hdc, NULL);
    DeleteObject(hrgn);
    myReleaseDC(hdc);
    
}

void ScrollDCScrollTest()
{
    info(ECHO, TEXT("ScrollDCScrollTest"));

    TDC hdc = myGetDC();

    for(int i = 0; i < 6; i++)
    {
        RECT rcScroll = { rand() % zx/8 + zx/4, rand() % zy/8 + zy/4, rand() % zx/8 + 5*zx/8, rand() % zy/8 + 5*zy/8};
        RECT rcClip =  { rand() % zx/8 + zx/4, rand() % zy/8 + zy/4, rand() % zx/8 + 5*zx/8, rand() % zy/8 + 5*zy/8};
        RECT rcRgn = {0};
        HRGN hrgnInvalidated = NULL;
        RECT * lprcScroll = NULL;
        RECT * lprcClip = NULL;

        RECT rcBlackRect = {0};
        RECT rcClipRect = {0};
        RECT rcInvalidated = {0};
        switch(i)
        {
            case 0:
                hrgnInvalidated = CreateRectRgn(0,0,0,0);
            case 1:
                if(hrgnInvalidated)
                    info(DETAIL, TEXT("testing scroll rect w/ hrgn"));
                else info(DETAIL, TEXT("testing scroll rect"));
                lprcScroll = &rcScroll;
                rcClipRect = rcScroll;
                lprcClip = NULL;
                
                break;
            case 2:
                hrgnInvalidated = CreateRectRgn(0,0,0,0);
            case 3:
                if(hrgnInvalidated)
                    info(DETAIL, TEXT("testing clip rect w/ hrgn"));
                else info(DETAIL, TEXT("testing clip rect"));
                rcClipRect = rcClip;
                lprcClip = &rcClip;
                lprcScroll = NULL;
                break;
            case 4:
                hrgnInvalidated = CreateRectRgn(0,0,0,0);
            case 5:
                if(hrgnInvalidated)
                    info(DETAIL, TEXT("testing scroll and clip rect w/ hrgn"));
                else info(DETAIL, TEXT("testing scroll and clip rect"));
                lprcScroll = &rcScroll;
                lprcClip = &rcClip;
                rcClipRect.left = max(rcScroll.left, rcClip.left);
                rcClipRect.right = min(rcScroll.right, rcClip.right);
                rcClipRect.top = max(rcScroll.top, rcClip.top);
                rcClipRect.bottom = min(rcScroll.bottom, rcClip.bottom);
                break;
        }
        rcBlackRect.left = (rand() % (rcClipRect.right - rcClipRect.left - 1)) + (rcClipRect.left + 1);
        rcBlackRect.right = (rand() % (rcClipRect.right - rcBlackRect.left)) + rcBlackRect.left;
        rcBlackRect.top = (rand() % (rcClipRect.bottom - rcClipRect.top - 1)) + (rcClipRect.top + 1);
        rcBlackRect.bottom = (rand() % (rcClipRect.bottom - rcBlackRect.top)) + rcBlackRect.top;

        int nWidth = rcBlackRect.right - rcBlackRect.left;
        int nHeight = rcBlackRect.bottom - rcBlackRect.top;
        int step = 0;

        PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
        PatBlt(hdc, rcBlackRect.left, rcBlackRect.top, nWidth, nHeight, BLACKNESS);

        info(DETAIL, TEXT("scrolling to the right"));
        for(step=0; step <= rcClipRect.right - rcBlackRect.left; step++)
        {
            if(!ScrollDC(hdc, 1, 0, lprcScroll, lprcClip, hrgnInvalidated, &rcInvalidated))
                info(FAIL, TEXT("ScrollDC call failed"));
            if(hrgnInvalidated)
            {
                if(SIMPLEREGION == GetRgnBox(hrgnInvalidated, &rcRgn))
                {
                    if(rcRgn.left != rcInvalidated.left || rcRgn.right != rcInvalidated.right || rcRgn.top != rcInvalidated.top || rcRgn.bottom != rcInvalidated.bottom)
                        info(FAIL, TEXT("Hrgn mismatch"));
                }
                else info(DETAIL, TEXT("complex region"));
            }
            if(rcInvalidated.left != rcClipRect.left || rcInvalidated.top != rcClipRect.top || 
                    rcInvalidated.right != rcClipRect.left+1 || rcInvalidated.bottom != rcClipRect.bottom)
                info(FAIL, TEXT("ScrollDC didn't set the invalidated rect correct t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
        }
        // just scrolled off of the area
        CheckAllWhite(hdc, 0, 1, 0, 0);

        PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
        PatBlt(hdc, rcBlackRect.left, rcBlackRect.top, nWidth, nHeight, BLACKNESS);

        info(DETAIL, TEXT("scrolling to the left"));
        for(step=0; step <= rcBlackRect.right - rcClipRect.left; step++)
        {
           if(!ScrollDC(hdc, -1, 0, lprcScroll, lprcClip, hrgnInvalidated, &rcInvalidated))
                info(FAIL, TEXT("ScrollDC call failed"));
           if(hrgnInvalidated)
           {
                if(SIMPLEREGION == GetRgnBox(hrgnInvalidated, &rcRgn))
                {
                    if(rcRgn.left != rcInvalidated.left || rcRgn.right != rcInvalidated.right || rcRgn.top != rcInvalidated.top || rcRgn.bottom != rcInvalidated.bottom)
                        info(FAIL, TEXT("Hrgn mismatch"));
                }
                else info(DETAIL, TEXT("complex region"));
           }
           if(rcInvalidated.left != rcClipRect.right -1 || rcInvalidated.top != rcClipRect.top || 
                    rcInvalidated.right != rcClipRect.right || rcInvalidated.bottom != rcClipRect.bottom)
                info(FAIL, TEXT("ScrollDC didn't set the invalidated rect correct t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
        }
        // just scrolled off of the area
        CheckAllWhite(hdc, 0, 1, 0, 0);

        PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
        PatBlt(hdc, rcBlackRect.left, rcBlackRect.top, nWidth, nHeight, BLACKNESS);

        info(DETAIL, TEXT("scrolling up"));
        for(step=0; step <= rcClipRect.bottom - rcBlackRect.top; step++)
        {
           if(!ScrollDC(hdc, 0, 1, lprcScroll, lprcClip, hrgnInvalidated, &rcInvalidated))
                info(FAIL, TEXT("ScrollDC call failed"));
           if(hrgnInvalidated)
           {
                if(SIMPLEREGION == GetRgnBox(hrgnInvalidated, &rcRgn))
                {
                    if(rcRgn.left != rcInvalidated.left || rcRgn.right != rcInvalidated.right || rcRgn.top != rcInvalidated.top || rcRgn.bottom != rcInvalidated.bottom)
                        info(FAIL, TEXT("Hrgn mismatch"));
                }
                else info(DETAIL, TEXT("complex region"));
           }
           if(rcInvalidated.left != rcClipRect.left || rcInvalidated.top != rcClipRect.top || 
                    rcInvalidated.right != rcClipRect.right || rcInvalidated.bottom != rcClipRect.top + 1)
                info(FAIL, TEXT("ScrollDC didn't set the invalidated rect correct t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
        }
        // just scrolled off of the area
        CheckAllWhite(hdc, 0, 1, 0, 0);

        PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
        PatBlt(hdc, rcBlackRect.left, rcBlackRect.top, nWidth, nHeight, BLACKNESS);

        info(DETAIL, TEXT("scrolling down"));
        for(step=0; step <= rcBlackRect.bottom - rcClipRect.top; step++)
        {
           if(!ScrollDC(hdc, 0, -1, lprcScroll, lprcClip, hrgnInvalidated, &rcInvalidated))
                info(FAIL, TEXT("ScrollDC call failed"));
           if(hrgnInvalidated)
           {
                if(SIMPLEREGION == GetRgnBox(hrgnInvalidated, &rcRgn))
                {
                    if(rcRgn.left != rcInvalidated.left || rcRgn.right != rcInvalidated.right || rcRgn.top != rcInvalidated.top || rcRgn.bottom != rcInvalidated.bottom)
                        info(FAIL, TEXT("Hrgn mismatch"));
                }
                else info(DETAIL, TEXT("complex region"));
           }           
           if(rcInvalidated.left != rcClipRect.left || rcInvalidated.top != rcClipRect.bottom -1 || 
                    rcInvalidated.right != rcClipRect.right || rcInvalidated.bottom != rcClipRect.bottom)
                info(FAIL, TEXT("ScrollDC didn't set the invalidated rect correct t:%d l:%d r:%d b:%d"), rcInvalidated.top, rcInvalidated.left,
                                                                                 rcInvalidated.right, rcInvalidated.bottom);
        }
        // just scrolled off of the area
        CheckAllWhite(hdc, 0, 1, 0, 0);      

        if(hrgnInvalidated)
        {
            DeleteObject(hrgnInvalidated);
            hrgnInvalidated = NULL;
        }
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   APIs
***
************************************************************************************/

//**********************************************************************************
TESTPROCAPI CreateCompatibleDC_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    StressPairs(ECreateCompatibleDC);
    passCreateCompatibleDCNULL(ECreateCompatibleDC);
    BadParam(ECreateCompatibleDC);

    // Depth
    globeTest(ECreateCompatibleDC);
    parisTest(ECreateCompatibleDC);
    TestCreateCompatibleDC(ECreateCompatibleDC);

    return getCode();
}

//**********************************************************************************
TESTPROCAPI DeleteDC_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    StressPairs(EDeleteDC);
    ProgramError(EDeleteDC);

    // Depth
    // None

    return getCode();
}

//**********************************************************************************
TESTPROCAPI RestoreDC_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    TestRestoreDC0(ERestoreDC);
    TestSaveDCNULL(ERestoreDC);

    // Depth
    SaveRestoreDCPairs(ERestoreDC);
    RecoverObjectsTest(ERestoreDC);
    
    return getCode();
}

//**********************************************************************************
TESTPROCAPI SaveDC_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    TestRestoreDC0(ESaveDC);
    TestSaveDCNULL(ESaveDC);

    // Depth
    SaveRestoreDCPairs(ESaveDC);
    RecoverObjectsTest(ESaveDC);

    return getCode();
}

//**********************************************************************************
TESTPROCAPI ScrollDC_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    if (!g_fPrinterDC)
    {
        // Breadth
        ScrollDCBadParam();
        SimpleScrollDCTest();

        // Depth
        ScrollDCTest();
        ScrollDCScrollTest();
    }
    else
        info(ECHO, TEXT("ScrollDC Test cannot be run on printer DC"));

    return getCode();
}

