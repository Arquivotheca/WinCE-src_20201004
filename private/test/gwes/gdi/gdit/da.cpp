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

   da.cpp

Abstract:

   Gdi Tests: Device Attribute APIs
***************************************************************************/

#include "global.h"

/***********************************************************************************
***
***   Uses random color to check return values of Get/Set Color pairs
***
************************************************************************************/

//***********************************************************************************
void
goodCheckGetSetColor(int testFunc)
{
    info(ECHO, TEXT("%s - goodCheckGetSetColor"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    int     counter = 0;
    COLORREF inColor,
            outSetColor = 0,
            outGetColor = 0;

    while (counter++ < testCycles)
    {
        inColor = randColorRef();

        switch (testFunc)
        {
            case EGetBkColor:
            case ESetBkColor:
                CheckNotRESULT(CLR_INVALID, outSetColor = SetBkColor(hdc, inColor));
                CheckNotRESULT(CLR_INVALID, outGetColor = GetBkColor(hdc));
                break;
            case EGetTextColor:
            case ESetTextColor:
                CheckNotRESULT(CLR_INVALID, outSetColor = SetTextColor(hdc, inColor));
                CheckNotRESULT(CLR_INVALID, outGetColor = GetTextColor(hdc));
                break;
        }

        // The colors created were valid so this would be an error
        if (inColor != outGetColor)
            info(FAIL, TEXT("Get/Set Pairs not equal in:%X  outSet:%X  outGet:%X"), inColor, outSetColor, outGetColor);
    }
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Same as above but passes alphas
***
************************************************************************************/

//***********************************************************************************
void
AlphaCheckGetSetColor(int testFunc)
{

    info(ECHO, TEXT("%s - AlphaCheckGetSetColor"), funcName[testFunc], testCycles);
    TDC     hdc = myGetDC();
    int     counter = 0;
    COLORREF inColor,
            outSetColor,
            outGetColor = 0;

    while (counter++ < testCycles)
    {
        // create random color - this time it can be in the range +/- maxint
        inColor = randColorRefA();

        SetLastError(ERROR_SUCCESS);
        // try to set and then get this random color
        switch (testFunc)
        {
            case EGetBkColor:
            case ESetBkColor:
                outSetColor = SetBkColor(hdc, inColor);
                outGetColor = GetBkColor(hdc);
                break;
            case EGetTextColor:
            case ESetTextColor:
                outSetColor = SetTextColor(hdc, inColor);
                outGetColor = GetTextColor(hdc);
                break;
        }
        // the inColor may be CLR_INVALID
        if (inColor != outGetColor)
            info(FAIL, TEXT("Color: Set<%X>  != result<%X>"), inColor, outGetColor);
        if(outGetColor == CLR_INVALID)
            CheckForLastError(ERROR_INVALID_PARAMETER);
    }
    myReleaseDC(hdc);

}

/***********************************************************************************
***
***   Check multiple calls to SetBkColor and SetTextColor on single DC
***
************************************************************************************/

//***********************************************************************************
void
multiSetBkColor()
{
    info(ECHO, TEXT("multiSetBkColor"));

    // This test isn't valid on a printer dc.
    NOPRINTERDCV(NULL);

    if(16 > myGetBitDepth())
    {
        // Test cannot run on bit depths less than 16
        info(DETAIL, TEXT("multiSetBkColor cannot run on this surface type/bit depth."));
        return;
    }

    TDC hdc = myGetDC();

    TBITMAP hbmp;
    HBRUSH hBrush;
    HBRUSH stockHBrush;
    HPEN   hPen;
    HPEN   stockHPen;
    int    stockBkMode;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hbmp = myLoadBitmap(globalInst, TEXT("JADE1")));
        CheckNotRESULT(NULL, hBrush = CreatePatternBrush(hbmp));
        CheckNotRESULT(NULL, hPen = CreatePen(PS_SOLID, 0, RGB(0x00, 0x00, 0x00)));
        CheckNotRESULT(NULL, stockHBrush = (HBRUSH)SelectObject(hdc, hBrush));
        CheckNotRESULT(NULL, stockHPen = (HPEN)SelectObject(hdc, hPen));
        CheckNotRESULT(0, stockBkMode = SetBkMode(hdc, OPAQUE));

        for (int i=0; i<3; i++)
        {
            COLORREF crRand;
            COLORREF crStockTextColor;
            COLORREF crStockBkColor;

            switch (i)
            {
            case 0:
                crRand = RGB(0xFF, 0x00, 0x00);
                break;
            case 1:
                crRand = RGB(0x00, 0xFF, 0x00);
                break;
            case 2:
                crRand = RGB(0x00, 0x00, 0xFF);
                break;
            }

            // select our new color
            CheckNotRESULT(CLR_INVALID, crStockTextColor = SetTextColor(hdc, crRand));
            CheckNotRESULT(CLR_INVALID, crStockBkColor = SetBkColor(hdc, crRand));

            // draw using our custom brush
            CheckNotRESULT(FALSE, Rectangle(hdc, 0, 0, 1, 1));

            // return original color to dc
            CheckNotRESULT(CLR_INVALID, SetTextColor(hdc, crStockTextColor));
            CheckNotRESULT(CLR_INVALID, SetBkColor(hdc, crStockBkColor));

            // Verify the correct color was set
            CheckForRESULT(crRand, GetPixel(hdc, 0, 0));
        }

        CheckNotRESULT(0, SetBkMode(hdc, stockBkMode));
        CheckNotRESULT(NULL, SelectObject(hdc, stockHBrush));
        CheckNotRESULT(NULL, SelectObject(hdc, stockHPen));

        CheckNotRESULT(FALSE, DeleteObject(hBrush));
        CheckNotRESULT(FALSE, DeleteObject(hPen));
        CheckNotRESULT(FALSE, DeleteObject(hbmp));
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Uses random mode to check return values of Get/Set Mode pairs
***
************************************************************************************/

//***********************************************************************************
void
checkGetSetModes(int testFunc)
{

    info(ECHO, TEXT("%s - %d Set/Get Mode Pairs"), funcName[testFunc], testCycles);

    TDC     hdc = myGetDC();
    int     counter = 0,
            getResult0,
            getResult1,
            setResult0,
            setResult1,
            mode;

    while (counter++ < testCycles)
    {
        // create a random mode in the range +/- maxint
        mode = badRandomNum();
#ifdef UNDER_CE
        // REVIEW: CE: No promise of handling large (or negative) modes
        mode %= 256;
        mode *= (mode < 0 ? -1 : 1);
#endif

        // look to the return values previous to the new mode and after
        getResult0 = GetBkMode(hdc);
        setResult0 = SetBkMode(hdc, mode);

        setResult1 = SetBkMode(hdc, mode);
        getResult1 = GetBkMode(hdc);

        // should be the previous mode
        if (getResult0 != setResult0)
            info(FAIL, TEXT("Get/Set Modes != before mode change. getResult:%d  setResult:%d"), getResult0, setResult0);

        /* This is correct win95 behavior, and differs from the NT
           behavior.  On win95, any BkMode other than OPAQUE gives you
           TRANSPARENT as the value returned by Set or GetBkMode().
         */
#ifdef WIN95
        if (mode != OPAQUE)
            mode = TRANSPARENT;
#endif // WIN95

        // after setting the new mode
        if (getResult1 != mode)
            info(FAIL, TEXT("GetBkMode did not return correct mode:%d  return:%d"), mode, getResult1);

        if (setResult1 != mode)
            info(FAIL, TEXT("SetBkMode did not return correct mode:%d  return:%d"), mode, setResult1);

        if (getResult1 != setResult1)
            info(FAIL, TEXT("Get/Set Modes != after mode change. getResult:%d  setResult:%d"), getResult1, setResult1);

    }
    myReleaseDC(hdc);
}

void
GetBkModeDefault(int testFunc)
{

    info(ECHO, TEXT("%s - Check Default Mode"), funcName[testFunc]);
    TDC     hdc = myGetDC();

    CheckForRESULT(OPAQUE, GetBkMode(hdc));

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   check RGB color return
***
************************************************************************************/
void
checkRGBColor(int testFunc)
{

    info(ECHO, TEXT("%s - Check RGB Color"), funcName[testFunc]);
    BOOL bOldVerify = SetSurfVerify(FALSE);
    TDC     hdc = myGetDC();

    switch (testFunc)
    {
        case EGetBkColor:
            CheckForRESULT(0, 0xFF000000 & GetBkColor(hdc));
            break;
        case EGetTextColor:
            CheckForRESULT(0, 0xFF000000 & GetTextColor(hdc));
            break;
        case ESetBkColor:
            CheckForRESULT(0, 0xFF000000 & SetBkColor(hdc, RGB(0, 0, 0)));
            break;
        case ESetTextColor:
            CheckForRESULT(0, 0xFF000000 & SetTextColor(hdc, RGB(0, 0, 0)));
            break;
    }

    myReleaseDC(hdc);
    SetSurfVerify(bOldVerify);
}

void
TestSetGetModeOrColor(int testFunc, int pass)
{
    info(ECHO, TEXT("%s - TestSetGetModeOrColor: %s bitmap in DC"), funcName[testFunc],
         pass ? TEXT("not using") : TEXT("using"));

    // If this is not a memory DC and we're trying to select a bitmap into
    // it, then skip the case (since it's not valid).
    if (!isMemDC() && !pass)
        return;

    // Disable surface verification because the verification DC will fail to validate
    // against a memory DC when trying to select a bitmap into it.
    BOOL bOldVerify = SetSurfVerify(FALSE);

    TDC     hdc = myGetDC();
    TBITMAP hBmp0,
                 hBmp1,
                 stockBmp = NULL,
                 testBmp = NULL;
    COLORREF inColor = RGB(0x55, 0x55, 0x55),
                   outColor = 0,
                   result = 0;

    CheckNotRESULT(NULL, hBmp0 = CreateCompatibleBitmap(hdc, 10, 10));
    CheckNotRESULT(NULL, hBmp1 = CreateCompatibleBitmap(hdc, 10, 10));

    // select in first bitmap
    if (!pass)
       CheckNotRESULT(NULL, stockBmp = (TBITMAP) SelectObject(hdc, hBmp0));

    // set an attribute
    switch (testFunc)
    {
        case ESetTextColor:
        case EGetTextColor:
            CheckNotRESULT(CLR_INVALID, result = SetTextColor(hdc, inColor));
            break;
        case ESetBkColor:
        case EGetBkColor:
            CheckNotRESULT(CLR_INVALID, result = SetBkColor(hdc, inColor));
            break;
        case ESetBkMode:
        case EGetBkMode:
            inColor = TRANSPARENT;
            CheckNotRESULT(CLR_INVALID, result = SetBkMode(hdc, inColor));
            break;
    }

    // select in second bitmap
    if (!pass)
    {
        CheckNotRESULT(NULL, testBmp = (TBITMAP) SelectObject(hdc, hBmp1));
        if (testBmp != hBmp0)
            info(FAIL, TEXT("Did not recieve set Bitmap: Got=0x%08x, Expected:0x%08x"), testBmp, hBmp0);
    }

    // check the attribute
    switch (testFunc)
    {
        case ESetTextColor:
        case EGetTextColor:
            CheckNotRESULT(CLR_INVALID, outColor = GetTextColor(hdc));
            break;
        case ESetBkColor:
        case EGetBkColor:
            CheckNotRESULT(CLR_INVALID, outColor = GetBkColor(hdc));
            break;
        case ESetBkMode:
        case EGetBkMode:
            CheckNotRESULT(0, outColor = GetBkMode(hdc));
            break;
    }

    // check color
    if (inColor != outColor)
        info(FAIL, TEXT("Set Color:0x%x != Get Color:0x%x, previous:0x%x"), inColor, outColor, result);

    // clean up
    if (!pass)
    {
        CheckNotRESULT(NULL, testBmp = (TBITMAP) SelectObject(hdc, stockBmp));
        if (testBmp != hBmp1)
            info(FAIL, TEXT("Did not recieve set Bitmap: Got=0x%08x, Expected:0x%08x"), testBmp, hBmp1);
    }
    CheckNotRESULT(FALSE, DeleteObject(hBmp0));
    CheckNotRESULT(FALSE, DeleteObject(hBmp1));
    myReleaseDC(hdc);
    SetSurfVerify(bOldVerify);
}

void
TestSetBkModeRandom(void)
{
    // modes are stored as bytes, so we either truncate
    // or limit the size of the mode we pass to be a byte.
    BYTE mode = GenRand() % 0xFF;
    info(DETAIL, TEXT("TestSetBkModeRandom: mode = %d"), mode);

    TDC     hdc = myGetDC();
    int     getResult;

    CheckNotRESULT(0, SetBkMode(hdc, mode));
    // GetBkMode can return 0 if the mode set was 0.
    getResult = GetBkMode(hdc);

    // verify the mode returned is the mode requested.
    CheckForRESULT(TRUE, mode == getResult);

    myReleaseDC(hdc);
}

void
PassNull2da(int testFunc)
{
    info(ECHO, TEXT("Passing Null 2 %s"), funcName[testFunc]);

    TDC hdc = myGetDC();

    switch(testFunc)
    {
        case ESetStretchBltMode:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetStretchBltMode((TDC) NULL, BLACKONWHITE));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetStretchBltMode(g_hdcBAD, BLACKONWHITE));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetStretchBltMode(g_hdcBAD2, BLACKONWHITE));
            CheckForLastError(ERROR_INVALID_HANDLE);

// random mode set's succeed on desktop.
#ifdef UNDER_CE
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetStretchBltMode(hdc, -1));
            CheckForLastError(ERROR_INVALID_PARAMETER);
#endif
            break;
        case EGetStretchBltMode:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetStretchBltMode((TDC) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetStretchBltMode(g_hdcBAD));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetStretchBltMode(g_hdcBAD2));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case ESetTextCharacterExtra:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0x80000000, SetTextCharacterExtra((TDC) NULL, 0));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0x80000000, SetTextCharacterExtra(g_hdcBAD, 0));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0x80000000, SetTextCharacterExtra(g_hdcBAD2, 0));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0x80000000, SetTextCharacterExtra(hdc, 0x80000000));
            CheckForLastError(ERROR_INVALID_PARAMETER);
            // verify that the invalid character extra wasn't set.
            CheckNotRESULT(0x80000000, GetTextCharacterExtra(hdc));
            break;
        case EGetTextCharacterExtra:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0x80000000, GetTextCharacterExtra((TDC) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0x80000000, GetTextCharacterExtra(g_hdcBAD));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0x80000000, GetTextCharacterExtra(g_hdcBAD2));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case ESetLayout:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(GDI_ERROR, SetLayout((TDC) NULL, 0x00));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(GDI_ERROR, SetLayout((TDC) g_hdcBAD, 0x00));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(GDI_ERROR, SetLayout((TDC) g_hdcBAD2, 0x00));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EGetLayout:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(GDI_ERROR, GetLayout((TDC) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(GDI_ERROR, GetLayout((TDC) g_hdcBAD));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(GDI_ERROR, GetLayout((TDC) g_hdcBAD2));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case ESetViewportOrgEx:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetViewportOrgEx((TDC) NULL, 0, 0, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetViewportOrgEx((TDC) g_hdcBAD, 0, 0, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetViewportOrgEx((TDC) g_hdcBAD2, 0, 0, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EGetViewportOrgEx:
        {
            POINT pt;

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetViewportOrgEx((TDC) NULL, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetViewportOrgEx((TDC) NULL, &pt));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetViewportOrgEx((TDC) g_hdcBAD, &pt));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetViewportOrgEx((TDC) g_hdcBAD2, &pt));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        }
        case EGetViewportExtEx:
        {
            SIZE sz;

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetViewportExtEx((TDC) NULL, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetViewportExtEx((TDC) NULL, &sz));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetViewportExtEx((TDC) g_hdcBAD, &sz));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetViewportExtEx((TDC) g_hdcBAD2, &sz));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        }
        case ESetWindowOrgEx:
        {
            POINT pt;

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetWindowOrgEx((TDC) NULL, 0, 0, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetWindowOrgEx((TDC) NULL, 0, 0, &pt));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetWindowOrgEx((TDC) g_hdcBAD, 0, 0, &pt));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetWindowOrgEx((TDC) g_hdcBAD2, 0, 0, &pt));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        }
        case EGetWindowOrgEx:
        {
            POINT pt;

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetWindowOrgEx((TDC) NULL, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetWindowOrgEx((TDC) NULL, &pt));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetWindowOrgEx((TDC) g_hdcBAD, &pt));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetWindowOrgEx((TDC) g_hdcBAD2, &pt));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        }
        case EOffsetViewportOrgEx:
        {
            POINT pt;

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, OffsetViewportOrgEx((TDC) NULL, 0, 0, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, OffsetViewportOrgEx((TDC) NULL, 0, 0, &pt));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, OffsetViewportOrgEx((TDC) g_hdcBAD, 0, 0, &pt));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, OffsetViewportOrgEx((TDC) g_hdcBAD2, 0, 0, &pt));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        }
        case EGetWindowExtEx:
        {
            SIZE sz;

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetWindowExtEx((TDC) NULL, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetWindowExtEx((TDC) NULL, &sz));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetWindowExtEx((TDC) g_hdcBAD, &sz));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetWindowExtEx((TDC) g_hdcBAD2, &sz));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        }
        case ESetTextColor:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(CLR_INVALID, SetTextColor((TDC) NULL, RGB(0, 0, 0)));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(CLR_INVALID, SetTextColor((TDC) g_hdcBAD, RGB(0, 0, 0)));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(CLR_INVALID, SetTextColor((TDC) g_hdcBAD2, RGB(0, 0, 0)));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case ESetBkMode:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetBkMode((TDC) NULL, OPAQUE));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetBkMode((TDC) g_hdcBAD, OPAQUE));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, SetBkMode((TDC) g_hdcBAD2, OPAQUE));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case ESetBkColor:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(CLR_INVALID, SetBkColor((TDC) NULL, RGB(0, 0, 0)));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(CLR_INVALID, SetBkColor((TDC) g_hdcBAD, RGB(0, 0, 0)));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(CLR_INVALID, SetBkColor((TDC) g_hdcBAD2, RGB(0, 0, 0)));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EGetTextColor:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(CLR_INVALID, GetTextColor((TDC) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(CLR_INVALID, GetTextColor((TDC) g_hdcBAD));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(CLR_INVALID, GetTextColor((TDC) g_hdcBAD2));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EGetBkColor:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(CLR_INVALID, GetBkColor((TDC) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(CLR_INVALID, GetBkColor((TDC) g_hdcBAD));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(CLR_INVALID, GetBkColor((TDC) g_hdcBAD2));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EGetBkMode:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetBkMode((TDC) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetBkMode((TDC) g_hdcBAD));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetBkMode((TDC) g_hdcBAD2));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        default:
            info(FAIL, TEXT("PassNull2da called on an invalid case"));
        break;
}

    myReleaseDC(hdc);
}

void
CheckGetSetStretchBltModes()
{
    info(ECHO, TEXT("CheckGetSetStretchBltModes"));
    TDC hdc = myGetDC();
    int nModesKnown[] = { BLACKONWHITE,
                                      BILINEAR,
                                      COLORONCOLOR,
                                      HALFTONE,
#ifndef UNDER_CE
                                      STRETCH_ANDSCANS,
                                      STRETCH_DELETESCANS,
                                      STRETCH_HALFTONE,
                                      STRETCH_ORSCANS,
                                      WHITEONBLACK
#endif
                                    };
    int nStockMode, nModeSet, nNewMode;

    CheckNotRESULT(0, nStockMode = GetStretchBltMode(hdc));
    nModeSet = nModesKnown[GenRand() % countof(nModesKnown)];
    CheckForRESULT(nStockMode, SetStretchBltMode(hdc, nModeSet));

    // loop 512 times getting and setting the mode.
    for(int i=0; i < 512; i++)
    {
        CheckForRESULT(nModeSet, GetStretchBltMode(hdc));
        nNewMode = nModesKnown[GenRand() % countof(nModesKnown)];
        CheckForRESULT(nModeSet, SetStretchBltMode(hdc, nNewMode));
        nModeSet = nNewMode;
    }

    CheckForRESULT(nModeSet, GetStretchBltMode(hdc));
    CheckForRESULT(nModeSet, SetStretchBltMode(hdc, nStockMode));

    myReleaseDC(hdc);
}

void
randSetStretchBltMode()
{
    info(ECHO, TEXT("randSetStretchBltMode"));
    TDC hdc = myGetDC();
    int nModeReturned, nModeSet, nStockMode;
    int nModesKnown[] = { BLACKONWHITE,
                                      BILINEAR,
                                      COLORONCOLOR,
                                      HALFTONE,
#ifndef UNDER_CE
                                      STRETCH_ANDSCANS,
                                      STRETCH_DELETESCANS,
                                      STRETCH_HALFTONE,
                                      STRETCH_ORSCANS,
                                      WHITEONBLACK
#endif
                                    };
#ifdef UNDER_CE
    BOOL nModeKnown;
#endif
    int nLastValidModeSet;

    CheckNotRESULT(0, nStockMode = GetStretchBltMode(hdc));
    nLastValidModeSet = nStockMode;

    for(int i=0; i<512; i++)
    {
        nModeSet = GenRand();
        nModeReturned = SetStretchBltMode(hdc, nModeSet);

        // Desktop sets the mode, even if passed an invalid mode.
#ifdef UNDER_CE
        nModeKnown = FALSE;
        for(int j=0; j<countof(nModesKnown); j++)
            if(nModesKnown[j] == nModeSet)
            {
                nModeKnown = TRUE;
                break;
            }

        if(nModeKnown && nModeReturned != nLastValidModeSet)
            info(FAIL, TEXT("Mode was known but mode returned was not last mode set"));

        if(!nModeKnown && nModeReturned != 0)
            info(FAIL, TEXT("Mode was invalid, but call did not fail"));

        if(nModeKnown)
            nLastValidModeSet = nModeSet;

        if(nModeReturned == 0)
            CheckForLastError(ERROR_INVALID_PARAMETER);
#else
        if(nModeReturned != nLastValidModeSet)
            info(FAIL, TEXT("Mode that was set was not returned %d set %d returned"), nModeSet, nModeReturned);
        nLastValidModeSet = nModeSet;
#endif
    }

    CheckNotRESULT(0, SetStretchBltMode(hdc, nStockMode));
    myReleaseDC(hdc);
}

void
RandSetTextCharacterExtraTest()
{
    info(ECHO, TEXT("RandSetTextCharacterExtraText"));
    TDC hdc = myGetDC();
    int nPreviousExtra = 0, nExtra;

    CheckNotRESULT(0x80000000, SetTextCharacterExtra(hdc, nPreviousExtra));

    // verify that it's returned.
    CheckForRESULT(nPreviousExtra, GetTextCharacterExtra(hdc));

    for(int i = 0; i < 512; i++)
    {
        // pick random numbers and set the extra to it.
        nExtra = GenRand();
        if(nExtra != 0x80000000)
        {
            CheckForRESULT(nPreviousExtra, SetTextCharacterExtra(hdc, nExtra));
            CheckForRESULT(nExtra, GetTextCharacterExtra(hdc));
            nPreviousExtra = nExtra;
        }
    }

    // cleanup the character extra change.
    CheckForRESULT(nPreviousExtra, SetTextCharacterExtra(hdc, 0));

    myReleaseDC(hdc);
}

void
SetTextCharacterExtraMaxTest()
{
    info(ECHO, TEXT("SetTextCharacterExtraMaxTest"));
    TDC hdc = myGetDC();
    TCHAR tcTestString[] = TEXT("this is a test string");
    int nOldCharacterExtra;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(0x80000000, nOldCharacterExtra = SetTextCharacterExtra(hdc, 0));

        for(int i = 1; i < 0x80000000; i = i<<1)
        {
            CheckNotRESULT(0x80000000, SetTextCharacterExtra(hdc, i));
            CheckNotRESULT(FALSE, ExtTextOut(hdc, 0, 0, NULL, NULL, tcTestString, _tcslen(tcTestString), NULL));
        }

        CheckNotRESULT(0x80000000, SetTextCharacterExtra(hdc, nOldCharacterExtra));
    }

    myReleaseDC(hdc);
}

void
SetTextCharacterExtraNegativeMaxTest()
{
    info(ECHO, TEXT("SetTextCharacterExtraNegativeMaxTest"));
    TDC hdc = myGetDC();
    TCHAR tcTestString[] = TEXT("this is a test string");
    int nOldCharacterExtra;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(0x80000000, nOldCharacterExtra = SetTextCharacterExtra(hdc, 0));

        for(int i = 1; i < 0x80000000; i = i<<1)
        {
            CheckNotRESULT(0x80000000, SetTextCharacterExtra(hdc, -i));
            CheckNotRESULT(FALSE, ExtTextOut(hdc, 0, 0, NULL, NULL, tcTestString, _tcslen(tcTestString), NULL));
        }

        CheckNotRESULT(0x80000000, SetTextCharacterExtra(hdc, nOldCharacterExtra));
    }

    myReleaseDC(hdc);
}

void
GetSetLayoutModeTest(int loops)
{
    info(ECHO, TEXT("GetSetLayoutModeTest - testing %d random modes"), loops);
    TDC hdc = myGetDC();
    DWORD dwSavedLayout;

    CheckNotRESULT(GDI_ERROR, dwSavedLayout = GetLayout(hdc));
    CheckNotRESULT(GDI_ERROR, SetLayout(hdc, 0x0008));  //LAYOUT_BITMAPORIENTATIONPRESERVED
    CheckNotRESULT(GDI_ERROR, SetLayout(hdc, 0));

    DWORD dwGet0, dwGet1, dwGet2, dwGet3;
    DWORD dwSet0, dwSet1, dwSet2;
    DWORD dwMode;
    for(int i=0; i < 50; i++)
    {
        dwMode = GenRand() * GDI_ERROR;
        CheckNotRESULT(GDI_ERROR, dwGet0 = GetLayout(hdc));
        CheckNotRESULT(GDI_ERROR, dwSet0 = SetLayout(hdc, dwMode));
        CheckNotRESULT(GDI_ERROR, dwGet1 = GetLayout(hdc));
        CheckNotRESULT(GDI_ERROR, dwSet1 = SetLayout(hdc, dwMode));
        CheckNotRESULT(GDI_ERROR, dwGet2 = GetLayout(hdc));
        CheckNotRESULT(GDI_ERROR, dwSet2 = SetLayout(hdc, dwGet0));
        CheckNotRESULT(GDI_ERROR, dwGet3 = GetLayout(hdc));

        if(dwGet0 != dwSet0)
            info(FAIL, TEXT("Mode mismatch - expected %x found %x"), dwGet0, dwSet0);
        if(dwGet1 != dwSet1)
            info(FAIL, TEXT("Mode mismatch - expected %x found %x"), dwGet1, dwSet1);
        if(dwGet2 != dwSet2)
            info(FAIL, TEXT("Mode mismatch - expected %x found %x"), dwGet2, dwSet2);
        if(dwGet3 != dwGet0)
            info(FAIL, TEXT("Mode mismatch - expected %x found %x"), dwGet3, dwSet0);
    }

    CheckNotRESULT(GDI_ERROR, SetLayout(hdc, dwSavedLayout));

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   APIs
***
************************************************************************************/

//***********************************************************************************
TESTPROCAPI GetBkColor_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    PassNull2da(EGetBkColor);
    TestSetGetModeOrColor(EGetBkColor, 0);
    TestSetGetModeOrColor(EGetBkColor, 1);
    checkRGBColor(EGetBkColor);
    goodCheckGetSetColor(EGetBkColor);
    AlphaCheckGetSetColor(EGetBkColor);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetBkMode_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    PassNull2da(EGetBkMode);
    TestSetGetModeOrColor(EGetBkMode, 0);
    TestSetGetModeOrColor(EGetBkMode, 1);
    TestSetBkModeRandom();
    checkGetSetModes(EGetBkMode);
    GetBkModeDefault(EGetBkMode);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetTextColor_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    PassNull2da(EGetTextColor);
    TestSetGetModeOrColor(EGetTextColor, 0);
    TestSetGetModeOrColor(EGetTextColor, 1);
    checkRGBColor(EGetTextColor);
    AlphaCheckGetSetColor(EGetTextColor);
    goodCheckGetSetColor(EGetTextColor);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI SetBkColor_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    PassNull2da(ESetBkColor);
    TestSetGetModeOrColor(ESetBkColor, 0);
    TestSetGetModeOrColor(ESetBkColor, 1);
    checkRGBColor(ESetBkColor);
    goodCheckGetSetColor(ESetBkColor);
    AlphaCheckGetSetColor(ESetBkColor);
    TestSetBkModeRandom();
    multiSetBkColor();

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI SetBkMode_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    PassNull2da(ESetBkMode);
    TestSetGetModeOrColor(ESetBkMode, 0);
    TestSetGetModeOrColor(ESetBkMode, 1);
    checkGetSetModes(ESetBkColor);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI SetTextColor_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    PassNull2da(ESetTextColor);
    TestSetGetModeOrColor(ESetTextColor, 0);
    TestSetGetModeOrColor(ESetTextColor, 1);
    checkRGBColor(ESetTextColor);
    goodCheckGetSetColor(ESetTextColor);
    AlphaCheckGetSetColor(ESetTextColor);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI SetViewportOrgEx_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    PassNull2da(ESetViewportOrgEx);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetViewportOrgEx_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    PassNull2da(EGetViewportOrgEx);

    // Depth
    // None
    return getCode();
}


//***********************************************************************************
TESTPROCAPI GetViewportExtEx_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    PassNull2da(EGetViewportExtEx);

    // Depth
    // None
    return getCode();
}

//***********************************************************************************
TESTPROCAPI SetWindowOrgEx_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    PassNull2da(ESetWindowOrgEx);

    // Depth
    // None
    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetWindowOrgEx_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    PassNull2da(EGetWindowOrgEx);

    // Depth
    // None
    return getCode();
}

//***********************************************************************************
TESTPROCAPI OffsetViewportOrgEx_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    PassNull2da(EOffsetViewportOrgEx);

    // Depth
    // None
    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetWindowExtEx_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    PassNull2da(EGetWindowExtEx);

    // Depth
    // None
    return getCode();
}

//***********************************************************************************
TESTPROCAPI SetStretchBltMode_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // Breadth
    PassNull2da(ESetStretchBltMode);

    // Depth
    CheckGetSetStretchBltModes();
    randSetStretchBltMode();

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetStretchBltMode_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // Breadth
    PassNull2da(EGetStretchBltMode);

    // Depth
    CheckGetSetStretchBltModes();

    return getCode();
}

//***********************************************************************************
TESTPROCAPI SetTextCharacterExtra_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{

    // these tests are only applicable on truetype systems,
    // raster fonts don't support character extras
    if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
    {
        // Breadth
        PassNull2da(ESetTextCharacterExtra);

        // Depth
        RandSetTextCharacterExtraTest();
        SetTextCharacterExtraMaxTest();
        SetTextCharacterExtraNegativeMaxTest();
    }
    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetTextCharacterExtra_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // Breadth

    if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
    {
        PassNull2da(EGetTextCharacterExtra);

        // Depth
        // none
    }
    return getCode();
}


//***********************************************************************************
TESTPROCAPI SetLayout_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // Breadth
    PassNull2da(ESetLayout);
    GetSetLayoutModeTest(50);

    // Depth
    // none

    return getCode();
}


//***********************************************************************************
TESTPROCAPI GetLayout_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // Breadth
    PassNull2da(EGetLayout);
    GetSetLayoutModeTest(50);

    // Depth
    // none

    return getCode();
}

