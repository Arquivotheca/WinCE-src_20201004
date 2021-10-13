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

   pal.cpp 

Abstract:

   Gdi Tests: Palette APIs

***************************************************************************/

#include "global.h"

/***********************************************************************************
***
***   GetNearestPaletteIndexTest       
***
************************************************************************************/

//***********************************************************************************
void
SimpleGetNearestPaletteIndexTest(int testFunc)
{
    info(ECHO, TEXT("%s: Testing with special Pal"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HPALETTE hPal;
    PALETTEENTRY outPal[256];
    COLORREF aColor;
    int     i,
            result;

    CheckNotRESULT(NULL, hPal = myCreatePal(hdc, 0));

    CheckNotRESULT(0, GetPaletteEntries(hPal, 0, 256, (LPPALETTEENTRY)outPal));
    for (i = 0; i < 256; i++)
    {
        aColor = PALETTERGB(outPal[i].peRed, outPal[i].peGreen, outPal[i].peBlue);
        result = GetNearestPaletteIndex(hPal, aColor);
        if (i > 9 && i < 246 && !(result >= 0 && result < 10) && result != i)
            info(FAIL, TEXT("GetNearestPaletteIndex returned %d not expected %d"), result, i);
    }

    myDeletePal(hdc, hPal);
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   GetPaletteEntriesTest       
***
************************************************************************************/

//***********************************************************************************
void
GetPaletteEntriesTest(int testFunc)
{
    info(ECHO, TEXT("%s: Testing with special Pal"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HPALETTE hPal;
    int     i;
    PALETTEENTRY outPal[256];

    CheckNotRESULT(NULL, hPal = myCreatePal(hdc, 0));
    CheckNotRESULT(0, GetPaletteEntries(hPal, 0, 256, (LPPALETTEENTRY)outPal));

    // verify the changes
    for (i = 10; i < 246; i++)
        if (outPal[i].peRed != i || outPal[i].peGreen != i || outPal[i].peBlue != i)
            info(FAIL, TEXT("palPalEntry index:%d not equal in:(%d %d %d) out:(%d %d %d)"), i, i, i, i, outPal[i].peRed,
                 outPal[i].peGreen, outPal[i].peBlue);

    myDeletePal(hdc, hPal);
    myReleaseDC(hdc);
}

//***********************************************************************************
void
SetGetPaletteEntriesTest(int testFunc)
{
    info(ECHO, TEXT("SetGetPaletteEntries"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HPALETTE hPal;
    int     i;
    PALETTEENTRY outPal[256];
    PALETTEENTRY inPal[256];
    
    CheckNotRESULT(NULL, hPal = myCreatePal(hdc, 0));

    for (i = 0; i < 256; i++)
    {
        inPal[i].peRed = (BYTE) (GenRand() % 256);
        inPal[i].peGreen = (BYTE) (GenRand() % 256);
        inPal[i].peBlue = (BYTE) (GenRand() % 256);
    }

    CheckForRESULT(256, SetPaletteEntries(hPal, 0, 256, (LPPALETTEENTRY)inPal));

    CheckForRESULT(256, GetPaletteEntries(hPal, 0, 256, (LPPALETTEENTRY)outPal));

    // verify the changes
    for (i = 0; i < 256; i++)
        if (outPal[i].peRed != inPal[i].peRed || outPal[i].peGreen != inPal[i].peGreen || outPal[i].peBlue != inPal[i].peBlue)
            info(FAIL, TEXT("palPalEntry index:%d not equal in:(%d %d %d) out:(%d %d %d)"), i, 
                                                                                        inPal[i].peRed, inPal[i].peGreen, inPal[i].peBlue, 
                                                                                        outPal[i].peRed, outPal[i].peGreen, outPal[i].peBlue);

    myDeletePal(hdc, hPal);
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   GetNearestColorTest        
***
************************************************************************************/

//***********************************************************************************
void
SimpleGetNearestColorTest(int testFunc)
{
    info(ECHO, TEXT("%s: Testing with special Pal"), funcName[testFunc]);

    if(myGetBitDepth() == 8 && (isSysPalDIBDC() || !isPalDIBDC()))
    {
        TDC     hdc = myGetDC();
        HPALETTE hPal,
                hPal0;
        PALETTEENTRY outPal[256];
        COLORREF aColor,
                result = 0;
        int     i,
                index;
        int bpp = myGetBitDepth();
        
        CheckNotRESULT(NULL, hPal = myCreatePal(hdc, 0));
        CheckNotRESULT(NULL, hPal0 = (HPALETTE) GetCurrentObject(hdc, OBJ_PAL));

        if (hPal0 != hPal)
            info(FAIL, TEXT("Didn't get same pal"));

        CheckNotRESULT(0, GetPaletteEntries(hPal, 0, 256, (LPPALETTEENTRY)outPal));

        for (i = 0; i < 256; i++)
        {
            aColor = RGB(outPal[i].peRed, outPal[i].peGreen, outPal[i].peBlue);
            if (bpp <= 8)
            {
                CheckNotRESULT(CLR_INVALID, result = GetNearestColor(hdc, aColor));
            }

            CheckNotRESULT(CLR_INVALID, index = GetNearestPaletteIndex(hPal, aColor));

            if (bpp <= 8)
            {
                if ((index < 10 || index > 245) && result != (aColor & 0x00FFFFFF))
                    info(FAIL, TEXT("GetNearestColor returned [%d](%d %d %d) not a system pal entry: index=%d color[%d %d %d] "), i,
                         GetRValue(result), GetGValue(result), GetBValue(result), index, outPal[i].peRed, outPal[i].peGreen,
                         outPal[i].peBlue);
            }
            else if ((index < 10 || index > 245) && (index != i))
                info(FAIL, TEXT("GetNearestPaletteIndex returned [%d]; not a system pal entry: index=%d "), i, index);

            /*
               else
               info(ECHO,TEXT("GetNearestColor returned [%d](%d %d %d) is a system pal entry: index=%d color[%d %d %d] "),
               i,GetRValue(result),GetGValue(result),GetBValue(result), index, outPal[i].peRed, outPal[i].peGreen, outPal[i].peBlue);
             */

        }

        myDeletePal(hdc, hPal);
        myReleaseDC(hdc);
    }
    else info(DETAIL, TEXT("Test requires 8bpp paletted surface with system palette to run"));
}

/***********************************************************************************
***
***   Passes Null as every possible parameter     
***
************************************************************************************/

//***********************************************************************************
void
passNull2Pal(int testFunc)
{

    info(ECHO, TEXT("%s - Passing NULL"), funcName[testFunc]);

    TDC     aDC = myGetDC();
    HPALETTE hpal;
    HPALETTE hPalBad;
    PALETTEENTRY lppe[256];

    CheckNotRESULT(NULL, hpal = myCreatePal(aDC, 0));

    // create an invalid Palette
    CheckNotRESULT(NULL, hPalBad = myCreatePal(aDC, 0));
    myDeletePal(aDC, hPalBad);

    switch (testFunc)
    {
        case EGetNearestColor:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(CLR_INVALID, GetNearestColor((TDC) NULL, 0));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(CLR_INVALID, GetNearestColor(g_hdcBAD, 0));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(CLR_INVALID, GetNearestColor(g_hdcBAD2, 0));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EGetNearestPaletteIndex:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(CLR_INVALID, GetNearestPaletteIndex(NULL, 0));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(CLR_INVALID, GetNearestPaletteIndex(hPalBad, 0));
            CheckForLastError(ERROR_INVALID_HANDLE);


            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(CLR_INVALID, GetNearestPaletteIndex((HPALETTE) g_hpenStock, 0));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EGetPaletteEntries:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetPaletteEntries(NULL, 0, 1, lppe));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetPaletteEntries(hPalBad, 0, 1, lppe));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetPaletteEntries((HPALETTE) g_hfontStock, 0, 1, lppe));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetPaletteEntries(hpal, 0, 0, lppe));
            break;
        case EGetSystemPaletteEntries:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetSystemPaletteEntries((TDC) NULL, 0, 1, lppe));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetSystemPaletteEntries(g_hdcBAD, 0, 1, lppe));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetSystemPaletteEntries(g_hdcBAD2, 0, 1, lppe));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case ESetPaletteEntries:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetPaletteEntries((HPALETTE) NULL, 0, 1, lppe));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetPaletteEntries(hPalBad, 0, 1, lppe));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetPaletteEntries((HPALETTE) g_hbrushStock, 0, 1, lppe));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetPaletteEntries(hpal, 0, 1, NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetPaletteEntries(hpal, 0, 0, lppe));
            CheckForLastError(ERROR_INVALID_PARAMETER);
            break;
    }

    myDeletePal(aDC, hpal);

    //LReleaseDC:
    myReleaseDC(aDC);

}

/***********************************************************************************
***
***   GetSystemPaletteEntriesTest
***
************************************************************************************/

//***********************************************************************************
void
GetSystemPaletteEntriesTest(int testFunc)
{
    info(ECHO, TEXT("%s - BVT Check"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    int     result0;
    PALETTEENTRY pal[256];
    int     i,
            start,
            num,
            errorCount = 0;
    // if the surfaces is paletted with the system paletted, and the
    // palette is settable, then verify the entries.
    BOOL    eightBit = GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE;

    for (start = 0; start < 256 && errorCount < 200; start++)
        for (num = start; num < 256 - start && errorCount < 200; num++)
        {
            // clean palette entry
            for (i = 0; i < 256; i++)
                pal[i].peRed = pal[i].peGreen = pal[i].peBlue = 7;

            // get real entires
            result0 = (int) GetSystemPaletteEntries(hdc, start, num, (LPPALETTEENTRY)pal);

            if (result0 != num && eightBit)
            {
                info(FAIL, TEXT("%s returned %d not the expected num:%d start:%d"), funcName[testFunc], result0, num, start);
                errorCount++;
            }

            // did something get changed?
            for (i = 0; i < result0 && eightBit; i++)
                if (pal[i].peRed == 7 && pal[i].peGreen == 7 && pal[i].peBlue == 7)
                {
                    info(FAIL, TEXT("1st Error Only: pal index:%d did not changed %s(start:%d num:%d)"), i, funcName[testFunc],
                         start, num);
                    errorCount++;
                    break;
                }
        }
    if (errorCount == 200)
        info(FAIL, TEXT("Terminating Test: At least 50 errors found"));

    //LReleaseDC:
    myReleaseDC(hdc);
}

void
GetSystemPaletteEntriesTest2(int testFunc)
{
    int     result0;

    info(ECHO, TEXT("%s - Get 10 entries"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    PALETTEENTRY pal[256];

    CheckForRESULT((myGetBitDepth() == 8 && !isMemDC())?10:0, result0 = (int) GetSystemPaletteEntries(hdc, 0, 10, (LPPALETTEENTRY)pal));

    //LReleaseDC:
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Palette APIs
***
************************************************************************************/

//***********************************************************************************
TESTPROCAPI GetNearestPaletteIndex_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    passNull2Pal(EGetNearestPaletteIndex);
    SimpleGetNearestPaletteIndexTest(EGetNearestPaletteIndex);

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetNearestColor_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Pal(EGetNearestColor);

    if (myGetBitDepth() == 8 && (isSysPalDIBDC() || !isPalDIBDC()))
        SimpleGetNearestColorTest(EGetNearestColor);

        // Depth
        // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetSystemPaletteEntries_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Pal(EGetSystemPaletteEntries);
    GetSystemPaletteEntriesTest2(EGetSystemPaletteEntries);

    // Depth
    GetSystemPaletteEntriesTest(EGetSystemPaletteEntries);
    
    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetPaletteEntries_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Pal(EGetPaletteEntries);
    GetPaletteEntriesTest(EGetPaletteEntries);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI SetPaletteEntries_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Pal(ESetPaletteEntries);
    SetGetPaletteEntriesTest(ESetPaletteEntries);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI CreatePalette_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    // None

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI SelectPalette_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    // None

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI RealizePalette_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    // None

    // Depth
    // None

    return getCode();
}
