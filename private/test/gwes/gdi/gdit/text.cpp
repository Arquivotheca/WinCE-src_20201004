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

   text.cpp

Abstract:

   Gdi Tests: Text APIs

***************************************************************************/
#include "global.h"
#include "fontdata.h"
#include <winuser.h>

#define MAXLEN 128

extern BOOL  CheckRotation();
extern BOOL  RandRotate(int nDirection = -1);
extern DWORD gdwCurrentAngle;
extern BOOL  IsUniscribeImage(void);

/***********************************************************************************
***
***   Uses random mode to check return values of Get/Set testalign pairs
***
************************************************************************************/

//***********************************************************************************
void
randGetSetTextAlign(int testFunc)
{
    info(ECHO, TEXT("%s - %d Set/Get TextAlign Random Modes"), funcName[testFunc], testCycles);

    TDC     hdc = myGetDC();
    int     counter = 0,
            getResult0,
            getResult1,
            setResult0,
            setResult1,
            alignmode;

    while (counter++ < 10)
    {
        // create a random mode
        alignmode = -(GenRand() % 10000);
        alignmode &= (TA_BASELINE | TA_BOTTOM | TA_TOP | TA_CENTER | TA_LEFT | TA_RIGHT | TA_NOUPDATECP | TA_UPDATECP);

        // not valid to have bit 2 set and not bit 1
        if ((alignmode & TA_CENTER) == (TA_CENTER ^ TA_RIGHT))
            alignmode |= TA_CENTER;

        // try to set and then get this random mode

        CheckNotRESULT(GDI_ERROR, getResult0 = GetTextAlign(hdc));
        CheckNotRESULT(GDI_ERROR, setResult0 = SetTextAlign(hdc, alignmode));
        CheckNotRESULT(GDI_ERROR, setResult1 = SetTextAlign(hdc, alignmode));
        CheckNotRESULT(GDI_ERROR, getResult1 = GetTextAlign(hdc));
        CheckNotRESULT(GDI_ERROR, SetTextAlign(hdc, getResult0));

        // The modes created were valid so this would be an error

        if (getResult0 != setResult0)
            info(FAIL, TEXT("Get/Set Text Align != before mode change. getResult:%x  setResult:%x"), getResult0, setResult0);

        if (getResult1 != alignmode)
            info(FAIL, TEXT("* GetTextAlign did not return correct mode:%x  return: %x"), alignmode, getResult1);

        if (setResult1 != alignmode)
            info(FAIL, TEXT("* SetTextAlign did not return correct mode:%x  return:%x"), alignmode, setResult1);

        if (getResult1 != setResult1)
            info(FAIL, TEXT("Get/Set TextAlign != after mode change. getResult:%x  setResult:%x"), getResult1, setResult1);
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Tries all alignment possibilities
***
************************************************************************************/

//***********************************************************************************
void
realGetSetTextAlign(int testFunc)
{
    info(ECHO, TEXT("%s - Set/Get TextAlign Real Modes"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    int     getResult0,
            getResult1,
            setResult0,
            setResult1;

    // try all modes
    for (int i = 0; i < numModes; i++)
    {
        // try to set and then get this valid mode
        CheckNotRESULT(GDI_ERROR, getResult0 = GetTextAlign(hdc));
        CheckNotRESULT(GDI_ERROR, setResult0 = SetTextAlign(hdc, mode[i]));
        CheckNotRESULT(GDI_ERROR, setResult1 = SetTextAlign(hdc, mode[i]));
        CheckNotRESULT(GDI_ERROR, getResult1 = GetTextAlign(hdc));

        if (getResult0 != setResult0)
            info(FAIL, TEXT("Get/Set Text Align != before mode change. getResult:%d  setResult:%d"), getResult0, setResult0);

        if (getResult1 != mode[i])
            info(FAIL, TEXT("GetTextAlign did not return correct mode:%d  return:%d"), mode, getResult1);

        if (setResult1 != mode[i])
            info(FAIL, TEXT("SetTextAlign did not return correct mode:%d  return:%d"), mode, setResult1);

        if (getResult1 != setResult1)
            info(FAIL, TEXT("Get/Set TextAlign != after mode change. getResult:%d  setResult:%d"), getResult1, setResult1);
    }
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Tries GetTextFace with all fonts
***
************************************************************************************/

//***********************************************************************************
void
GetTextFaceTest(int testFunc)
{
    info(ECHO, TEXT("%s - Changing and getting Face"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HFONT   hFont, hFontStock;
    TCHAR   outFaceName[256];
    int     wResult;
    UINT    uLCID = GetSystemDefaultLCID();

    for (int i = 0; i < numFonts; i++)
    {
        memset(outFaceName, 0, sizeof(outFaceName));
        CheckNotRESULT(NULL, hFont = setupKnownFont(i, 20, 0, 0, 0, 0, 0));
        CheckNotRESULT(NULL, hFontStock = (HFONT) SelectObject(hdc, hFont));
        // get the face
        CheckNotRESULT(0, GetTextFace(hdc, countof(outFaceName), (LPTSTR)outFaceName));

        // is the output == input?
        wResult = _tcscmp(fontArray[i].tszFaceName, outFaceName);
        if (hFont && wResult != 0)
        {
            // only check USA version
            if (uLCID == 0x409)
                info(FAIL, TEXT("_tcscmp failed on %s != %s  #:%d"), fontArray[i].tszFaceName, outFaceName, i);
            else
                info(DETAIL, TEXT("NON USA version: tcscmp failed on %s != %s  #:%d"), fontArray[i].tszFaceName, outFaceName, i);
        }

        CheckNotRESULT(NULL, SelectObject(hdc, hFontStock));
        CheckNotRESULT(FALSE, DeleteObject(hFont));
    }
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Tries GetTextMetrics with all fonts compares to NT
***
************************************************************************************/

//***********************************************************************************
void
GetTextMetricsTest(int testFunc)
{
    info(ECHO, TEXT("%s - check against stored values"), funcName[testFunc]);

    TDC     hdc = myGetDC();
                        // font order must match entries in NTFontMetrics[][] in fontdata.h
    int     fontList[] = {tahomaFont, courierFont, symbolFont, timesFont, wingdingFont, verdanaFont};
    int     fontCount = countof(fontList);
    HFONT   hFont, hFontStock;
    TEXTMETRIC outTM;
    int     temp[18];

    // SetTextCharacterExtra not implemented on raster fonts.
    if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
        CheckNotRESULT(0x8000000, SetTextCharacterExtra(hdc, 50));

    for (int i = 0; i < fontCount; i++)
    {
        if (fontList[i] != -1)
        {
            CheckNotRESULT(NULL, hFont = setupKnownFont(fontList[i], 20, 0, 0, 0, 0, 0));
            CheckNotRESULT(NULL, hFontStock = (HFONT) SelectObject(hdc, hFont));
            // get the face
            CheckNotRESULT(0, GetTextMetrics(hdc, &outTM));

            // fill in values for this font
            temp[0] = outTM.tmHeight;
            temp[1] = outTM.tmAscent;
            temp[2] = outTM.tmDescent;
            temp[3] = outTM.tmInternalLeading;
            temp[4] = outTM.tmExternalLeading;
            temp[5] = outTM.tmAveCharWidth;
            temp[6] = outTM.tmMaxCharWidth;
            temp[7] = outTM.tmWeight;
            temp[8] = outTM.tmOverhang;
            temp[9] = outTM.tmFirstChar;
            temp[10] = outTM.tmLastChar;
            temp[11] = outTM.tmDefaultChar;
            temp[12] = outTM.tmBreakChar;
            temp[13] = outTM.tmItalic;
            temp[14] = outTM.tmUnderlined;
            temp[15] = outTM.tmStruckOut;
            temp[16] = outTM.tmPitchAndFamily;
            temp[17] = outTM.tmCharSet;

            if (!hFont)
                continue;

            // cleck against NT hard coded array
            for (int j = 0; j < countof(temp); j++)
            {
#ifdef UNDER_CE                 // CE no promise to match NTs values
                TCHAR   szFaceName[64];
                if (temp[j] != NTFontMetrics[i][j]
                    && (j != 3 && j != 5 && j != 6 && j != 9 && j != 10)
                    && !(isPrinterDC(hdc)))
                    info(FAIL, TEXT("* GetTextMetrics font:%d param:%d  NT:%d != OS:%d"), i, j, NTFontMetrics[i][j], temp[j]);

                CheckNotRESULT(0, GetTextFace(hdc, 60, szFaceName));

                if(_tcscmp(fontArray[fontList[i]].tszFaceName, szFaceName))
                    info(FAIL, TEXT("Wanted font %s, recieved font %s"), fontArray[fontList[i]].tszFaceName, szFaceName);

#else
                if (temp[j] != NTFontMetrics[i][j])
                    info(FAIL, TEXT("* GetTextMetrics font:%d param:%d  NT:%d != OS:%d"), i, j, NTFontMetrics[i][j], temp[j]);
#endif
            }
            CheckNotRESULT(NULL, SelectObject(hdc, hFontStock));
            CheckNotRESULT(FALSE, DeleteObject(hFont));
        }
    }

    // SetTextCharacterExtra not implemented on raster fonts.
    if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
        CheckNotRESULT(0x8000000, SetTextCharacterExtra(hdc, 0));
    myReleaseDC(hdc);
}

//***********************************************************************************
void
GetTextMetricsDPITest(int testFunc)
{
    info(ECHO, TEXT("%s - GetTextMetricsDPI test"), funcName[testFunc]);

    TDC     hdc = myGetDC();
                        // font order must match entries in NTFontMetrics[][] in fontdata.h
    int     fontList[] = {tahomaFont, courierFont, symbolFont, timesFont, wingdingFont, verdanaFont};
    int     fontCount = countof(fontList);
    HFONT   hFont, hFontStock;
    TEXTMETRIC outTM;

    // SetTextCharacterExtra not implemented on raster fonts.
    if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
        CheckNotRESULT(0x8000000, SetTextCharacterExtra(hdc, 50));

    for (int i = 0; i < fontCount; i++)
    {
        if (fontList[i] != -1)
        {
            CheckNotRESULT(NULL, hFont = setupKnownFont(fontList[i], 20, 0, 0, 0, 0, 0));
            CheckNotRESULT(NULL, hFontStock = (HFONT) SelectObject(hdc, hFont));
            if(!hFont)
                continue;

            // get the face
            CheckNotRESULT(0, GetTextMetrics(hdc, &outTM));

            CheckForRESULT(outTM.tmDigitizedAspectX, GetDeviceCaps(hdc, LOGPIXELSX));
            CheckForRESULT(outTM.tmDigitizedAspectY, GetDeviceCaps(hdc, LOGPIXELSY));

            CheckNotRESULT(NULL, SelectObject(hdc, hFontStock));
            CheckNotRESULT(FALSE, DeleteObject(hFont));
        }
    }

    // SetTextCharacterExtra not implemented on raster fonts.
    if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
        CheckNotRESULT(0x8000000, SetTextCharacterExtra(hdc, 0));

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Tries different strings and lengths for extent
***
************************************************************************************/

//***********************************************************************************
void
GetTextExtentPointTest(int testFunc)
{
    info(ECHO, TEXT("%s - checking extents against NT Hardcoded"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HFONT   hFont, hFontStock;
    SIZE    size;
    TCHAR   myStr[2];
    TCHAR   aCh;
    int     maxerrors = 0;
    int     fontList[] = {tahomaFont, courierFont, /*symbolFont,*/ timesFont, wingdingFont, verdanaFont, arialRasterFont};

    myStr[1] = NULL;
    for (int f = 0; f < countof(fontList); f++)
    {
        if (fontList[f] != -1)
        {
            // for all of the tests and chars
            CheckNotRESULT(NULL, hFont = setupKnownFont(fontList[f], 16, 0, 0, 0, 0, 0));
            CheckNotRESULT(NULL, hFontStock = (HFONT) SelectObject(hdc, hFont));

            for (int j = 0; j <= 'z' - '!'; j++)
            {
                aCh = (TCHAR)(j + TEXT('!'));
                myStr[0] = aCh;

                switch(testFunc)
                {
                    case EGetTextExtentPoint32:
                        CheckNotRESULT(0, GetTextExtentPoint32(hdc, myStr, 1, &size));
                        break;
                    case EGetTextExtentPoint:
                        CheckNotRESULT(0, GetTextExtentPoint(hdc, myStr, 1, &size));
                        break;
                    case EGetTextExtentExPoint:
                        CheckNotRESULT(0, GetTextExtentExPoint(hdc, myStr, 1, 0, NULL, NULL, &size));
                        break;
                }

                if(fontArray[fontList[f]].nLinkedFont == -1 || !fontArray[fontList[f]].st.IsInSkipTable(aCh))
                {
                    if(maxerrors < 16 && (size.cy != 16 || size.cx != NTExtentResults[f][j]))
                    {
                        info(FAIL, TEXT("* font #%d:<%s> char:<%x> x:(%d != NT:%d) y:(%d != NT:16)"), f, fontArray[fontList[f]].tszFaceName,
                             aCh, size.cx, NTExtentResults[f][j], size.cy);
                        maxerrors++;
                    }
                }
                else info(DETAIL, TEXT("<%s> char:<%x> is in the skip table."), fontArray[fontList[f]].tszFaceName, aCh);
            }
            CheckNotRESULT(NULL, SelectObject(hdc, hFontStock));
            CheckNotRESULT(FALSE, DeleteObject(hFont));
        }
    }
    if (maxerrors >= 10)
        info(DETAIL, TEXT("More than 10 errors found. Part of test skipped."));
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   GetTextExtentPoint32ImpactTest
***
************************************************************************************/

//***********************************************************************************
void
GetTextExtentPointImpactTest(int testFunc)
{
    // this done with Symbol font now
    info(ECHO, TEXT("%s - checking extents against NT Hardcoded"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HFONT   hFont, hFontStock;
    SIZE    size;
    int     strLen;
    BOOL    expect;
    TCHAR   myStr[500];

    TCHAR   myChar = 'Z';
    int     fontList[] = {tahomaFont, courierFont, symbolFont, timesFont, wingdingFont, verdanaFont, arialRasterFont};

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

        for (int l = 0; l < 100; l++)
            myStr[l] = myChar;
        myStr[l] = NULL;

        strLen = _tcslen(myStr);

        for(int f=0; f<countof(fontList); f++)
        {
            if (fontList[f] != -1)
            {
                expect = NTExtentZResults[f];

                // for all of the tests and chars
                CheckNotRESULT(NULL, hFont = setupKnownFont(fontList[f], 16, 0, 0, 0, 0, 0));
                CheckNotRESULT(NULL, hFontStock = (HFONT) SelectObject(hdc, hFont));

                switch(testFunc)
                {
                    case EGetTextExtentPoint32:
                        CheckNotRESULT(0, GetTextExtentPoint32(hdc, myStr, strLen, &size));
                        break;
                    case EGetTextExtentPoint:
                        CheckNotRESULT(0, GetTextExtentPoint(hdc, myStr, strLen, &size));
                        break;
                    case EGetTextExtentExPoint:
                        CheckNotRESULT(0, GetTextExtentExPoint(hdc, myStr, strLen, 0, NULL, NULL, &size));
                        break;
                }

                    if(fontArray[fontList[f]].nLinkedFont == -1 || !fontArray[fontList[f]].st.IsInSkipTable(myChar))
                    {
                        if (hFont && (size.cx != expect))
                            info(FAIL, TEXT("#:%d of char:<%c> (%d != NT:%d)"), 100, myChar, size.cx, expect);
                    }
                    else info(DETAIL, TEXT("<%s> char:<%x> is in the skip table."), fontArray[fontList[f]].tszFaceName, myChar);

                CheckNotRESULT(NULL, SelectObject(hdc, hFontStock));
                CheckNotRESULT(FALSE, DeleteObject(hFont));
            }
        }
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Speed test
***
************************************************************************************/

//***********************************************************************************
void
SpeedExtTestOut(int testFunc)
{

    info(ECHO, TEXT("%s - Speed Check"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    RECT    r = { 0, 0, zx, zy };
    TCHAR   word[50];
    UINT    len;
    DWORD   start,
            end;
    float   totalTime,
            performNT = (float) 400,
            percent;
    HFONT   hFont, hFontStock;
    HBRUSH  hBrush,
            stockBr;
    HRGN    hRgn;
    int     numCalls = 200;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hBrush = CreateSolidBrush(RGB(255, 0, 0)));
        CheckNotRESULT(NULL, stockBr = (HBRUSH) SelectObject(hdc, hBrush));
        CheckNotRESULT(NULL, hRgn = CreateRectRgnIndirect(&r));

        // set up
        CheckNotRESULT(FALSE, FillRgn(hdc, hRgn, hBrush));
        _tcscpy_s(word, TEXT("m"));   //Microsoft Logo
        len = _tcslen(word);

        CheckNotRESULT(GDI_ERROR, SetTextAlign(hdc, TA_CENTER));
        CheckNotRESULT(CLR_INVALID, SetTextColor(hdc, RGB(255, 255, 255)));
        CheckNotRESULT(0, SetBkMode(hdc, TRANSPARENT));
        CheckNotRESULT(CLR_INVALID, SetBkColor(hdc, RGB(255, 0, 0)));

        CheckNotRESULT(NULL, hFont = setupKnownFont(MSLOGO, 52, 0, 0, 0, 0, 0));
        CheckNotRESULT(NULL, hFontStock = (HFONT) SelectObject(hdc, hFont));

        start = GetTickCount();

        for (int i = 0; i < numCalls; i++)
            CheckNotRESULT(0, ExtTextOut(hdc, r.right / 2, r.bottom / 2 - 26, ETO_OPAQUE | ETO_IGNORELANGUAGE, &r, word, len, NULL));

        end = GetTickCount();
        totalTime = (float) ((float) end - (float) start) / 1000;
        percent = (float) 100 *(((float) i / totalTime) / performNT);

        CheckNotRESULT(NULL, SelectObject(hdc, hFontStock));
        CheckNotRESULT(FALSE, DeleteObject(hFont));

        if (percent < (float) 80)
            info(ECHO, TEXT("* %.2f percent of NT Performance: %d calls took secs:%.2f, average calls/sec:%.2f"), percent, i,
                 totalTime, (float) i / totalTime);

        CheckNotRESULT(NULL, SelectObject(hdc, stockBr));

        CheckNotRESULT(FALSE, DeleteObject(hBrush));
        CheckNotRESULT(FALSE, DeleteObject(hRgn));
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Credits test
***
************************************************************************************/
#define  Divisions 8

//***********************************************************************************
void
CreditsExtTestOut(int testFunc)
{

    info(ECHO, TEXT("%s - Credits Check"), funcName[testFunc]);

    TDC     hdc = myGetDC(),
            compDC;
    UINT    len;
    HFONT   hFont, hFontStock,
            hFontLogo, hFontLogoStock;
    int     i,
            cent,
            nextPerson = Divisions,
            steps,
            swag;
    RECT    offRect;
    TBITMAP hBitmap, hBitmapStock;

    RECT    RectCycles[20];
    int     peopleIndex[20];
    int     offset[20][2];
    COLORREF pColor[20],
            aColor[4] = { RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(255, 255, 0) };

    // set up RECTs
    steps = zy / Divisions;
    swag = steps * 2;
    for (i = 0; i < 20; i++)
    {
        RectCycles[i].left = (long) ((float) steps * (float) 1.5);
        RectCycles[i].right = zx - RectCycles[i].left;
        RectCycles[i].top = i * steps - steps;
        RectCycles[i].bottom = RectCycles[i].top + steps;
        peopleIndex[i] = i;
        pColor[i] = aColor[goodRandomNum(4)];
        offset[i][0] = offset[i][1] = 1;
    }

    offRect.left = offRect.top = 0;
    offRect.right = RectCycles[0].right - RectCycles[0].left;
    offRect.bottom = RectCycles[0].bottom - RectCycles[0].top;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hBitmap = CreateCompatibleBitmap(hdc, offRect.right, offRect.bottom));
        CheckNotRESULT(NULL, compDC = CreateCompatibleDC(hdc));
        CheckNotRESULT(NULL, hBitmapStock = (TBITMAP) SelectObject(compDC, hBitmap));

        // set up GDI modes and states
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
        CheckNotRESULT(FALSE, PatBlt(compDC, 0, 0, zx, zy, WHITENESS));
        CheckNotRESULT(0, SetBkMode(compDC, OPAQUE));
        CheckNotRESULT(CLR_INVALID, SetBkColor(compDC, RGB(0, 0, 0)));
        CheckNotRESULT(GDI_ERROR, SetTextAlign(compDC, TA_CENTER | TA_TOP));

        CheckNotRESULT(NULL, hFont = setupKnownFont(aFont, 45, 0, 0, 0, 0, 0));
        CheckNotRESULT(NULL, hFontStock = (HFONT) SelectObject(hdc,hFont));
        CheckNotRESULT(NULL, hFontLogo = setupKnownFont(MSLOGO, 45, 0, 0, 0, 0, 0));
        CheckNotRESULT(NULL, hFontLogoStock = (HFONT) SelectObject(compDC,hFontLogo));

        __analysis_assume(numPeople > 0);
        while (nextPerson != numPeople - 1)
        {
            for (i = 0; i <= Divisions; i++)
            {
                // if the entry is for the mslogo, and we have the mslogo font
                if(!_tcscmp(people[peopleIndex[i]], TEXT("Microsoft")) && MSLOGO)
                {
                    // use the logo, and set the i in Microsoft to NULL, so it's not printed
                    CheckNotRESULT(NULL, SelectObject(compDC, (HFONT) ((people[peopleIndex[i]][0] == 'M') ? hFontLogo : hFont)));
                    len = 1;
                }
                // not the microsoft special case, use the standard font.
                else
                {
                    CheckNotRESULT(NULL, SelectObject(compDC, (HFONT) hFont));
                    len = _tcslen(people[peopleIndex[i]]);
                }
                cent = (people[peopleIndex[i]][0] == ' ' ||
                        people[peopleIndex[i]][0] == 'M') ? (zx - offRect.right) / 2 : RectCycles[i].left + offset[i][1];
                CheckNotRESULT(CLR_INVALID, SetTextColor(compDC, pColor[i]));
                CheckNotRESULT(0, ExtTextOut(compDC, offRect.right / 2, 0, ETO_OPAQUE | ETO_IGNORELANGUAGE, &offRect, people[peopleIndex[i]], len, NULL));

                CheckNotRESULT(FALSE, BitBlt(hdc, cent, RectCycles[i].top, offRect.right, offRect.bottom, compDC, 0, 0, SRCCOPY));

                if (offset[i][1] > swag || offset[i][1] < -swag)
                    offset[i][0] *= -1;
                offset[i][1] += offset[i][0];
                RectCycles[i].top-=10;
                RectCycles[i].bottom-=10;

                if (RectCycles[i].top <= -steps)
                {
                    RectCycles[i].top = (Divisions + 1) * steps - steps;
                    RectCycles[i].bottom = RectCycles[i].top + steps;
                    peopleIndex[i] = ++nextPerson;
                    pColor[i] = aColor[goodRandomNum(4)];
                    offset[i][0] = offset[i][1] = 1;
                }
            }
        }

        CheckNotRESULT(NULL, SelectObject(hdc, hFontStock));
        CheckNotRESULT(NULL, SelectObject(compDC, hFontLogoStock));
        CheckNotRESULT(FALSE, DeleteObject(hFont));
        CheckNotRESULT(FALSE, DeleteObject(hFontLogo));

        CheckNotRESULT(NULL, SelectObject(compDC, hBitmapStock));
        CheckNotRESULT(FALSE, DeleteObject(hBitmap));
        CheckNotRESULT(FALSE, DeleteDC(compDC));
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Passes Null as every possible parameter
***
************************************************************************************/

//***********************************************************************************
void
passNull2Text(int testFunc)
{

    info(ECHO, TEXT("%s - Passing NULL"), funcName[testFunc]);

    TDC hdc = myGetDC();
    SIZE    size;
    TCHAR string[] = TEXT("This is a test string");
    RECT rcRect = {0, 0, 10, 10};
    TEXTMETRIC tm;

    switch (testFunc)
    {
        case EDrawTextW:
#ifdef UNDER_CE // userspace exception on XP for this, ce handles it.
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, DrawText(hdc, string, _tcslen(string), NULL, DT_BOTTOM));
            CheckForLastError(ERROR_INVALID_PARAMETER);
#endif
            // this should return 1 but not draw anything.
            CheckForRESULT(1, DrawText(hdc, string, 0, &rcRect, DT_BOTTOM));

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, DrawText(g_hdcBAD, string, _tcslen(string), &rcRect, DT_BOTTOM));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, DrawText(hdc, NULL, _tcslen(string), &rcRect, DT_BOTTOM));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, DrawText((TDC) NULL, string, _tcslen(string), &rcRect, DT_BOTTOM));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EExtTextOut:
#ifdef UNDER_CE // XP behaves differently for these
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, ExtTextOut(hdc, 0, 0, ETO_CLIPPED | ETO_IGNORELANGUAGE, NULL, NULL, _tcslen(string), NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);
#endif

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, ExtTextOut((TDC) NULL, 0, 0, ETO_CLIPPED | ETO_IGNORELANGUAGE, NULL, string, _tcslen(string), NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, ExtTextOut(g_hdcBAD, 0, 0, ETO_CLIPPED | ETO_IGNORELANGUAGE, NULL, string, _tcslen(string), NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EGetTextExtentPoint32:
// causes test exception on XP.
#ifdef UNDER_CE
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextExtentPoint32(hdc, string, _tcslen(string), NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextExtentPoint32(hdc, NULL, _tcslen(string), &size));
            CheckForLastError(ERROR_INVALID_PARAMETER);
#endif

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextExtentPoint32(hdc, string, -1, &size));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextExtentPoint32((TDC) NULL, string, _tcslen(string), &size));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextExtentPoint32(g_hdcBAD, string, _tcslen(string), &size));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EGetTextExtentPoint:
// Causes test exception on XP.
#ifdef UNDER_CE
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextExtentPoint(hdc, string, _tcslen(string), NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextExtentPoint(hdc, NULL, _tcslen(string), &size));
            CheckForLastError(ERROR_INVALID_PARAMETER);
#endif

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextExtentPoint(hdc, string, -1, &size));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextExtentPoint((TDC) NULL, string, _tcslen(string), &size));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextExtentPoint(g_hdcBAD, string, _tcslen(string), &size));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EGetTextExtentExPoint:
// Causes test exception on XP.
#ifdef UNDER_CE
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextExtentExPoint(hdc, string, _tcslen(string), 0, NULL, NULL, NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextExtentExPoint(hdc, NULL, _tcslen(string), 0, NULL, NULL, &size));
            CheckForLastError(ERROR_INVALID_PARAMETER);
#endif

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextExtentExPoint(hdc, string, -1, 0, NULL, NULL, &size));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextExtentExPoint((TDC) NULL, string, _tcslen(string), 0, NULL, NULL, &size));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextExtentExPoint(g_hdcBAD, string, _tcslen(string), 0, NULL, NULL, &size));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EGetTextFace:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextFace(hdc, -1, string));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextFace((TDC) NULL, countof(string), string));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextFace(g_hdcBAD, countof(string), string));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EGetTextMetrics:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextMetrics(hdc, NULL));
            CheckForLastError(ERROR_INVALID_PARAMETER);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextMetrics((TDC) NULL, &tm));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(0, GetTextMetrics(g_hdcBAD, &tm));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EGetTextAlign:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(GDI_ERROR, GetTextAlign((TDC) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(GDI_ERROR, GetTextAlign(g_hdcBAD));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case ESetTextAlign:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(GDI_ERROR, SetTextAlign((TDC) NULL, TA_BASELINE));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(GDI_ERROR, SetTextAlign(g_hdcBAD, TA_BASELINE));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   ExtTextOut Param Checking
***
************************************************************************************/

void
passNull2ExtTextOut(int testFunc)
{

    info(ECHO, TEXT("%s - Passing NULL 2*"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    RECT    rc = { 0, 0, 10, 10 };

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        SetLastError(ERROR_SUCCESS);
        CheckForRESULT(0, ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rc, NULL, 3000, NULL));
        CheckForLastError(ERROR_INVALID_PARAMETER);

        // crashed client ITV588
        info(ECHO, TEXT("%s - Part 2 *"), funcName[testFunc]);
        rc.left = rc.top = 0;
        rc.right = rc.bottom = 100;
        CheckNotRESULT(GDI_ERROR, SetTextAlign(hdc, TA_RIGHT));
        CheckNotRESULT(GDI_ERROR, SetBkMode(hdc, OPAQUE));
        CheckNotRESULT(0, ExtTextOut(hdc, 10, 10, 0, NULL, NULL, 0, (INT *) & rc));
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***    DrawText Param Checking
***
************************************************************************************/

void
passNull2DrawText(int testFunc)
{

    info(ECHO, TEXT("%s - Passing NULL *"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    RECT    rc = { 0, 0, 10, 10 }, rc2 = { 0, 0, 0, 0};

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(0, DrawText(hdc, TEXT("Hello World"), -1, &rc, 1));
        CheckNotRESULT(0, DrawText(hdc, TEXT("Hello World"), 30, &rc2, 99999999));
        CheckNotRESULT(0, DrawText(hdc, TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ"), 26, &rc, DT_CALCRECT));

        if (rc.top != 0 || rc.left != 0 || rc.bottom <= 10 || rc.right <= 10)
            info(FAIL, TEXT("rc rect (%d %d %d %d)didn't get adjusted when DT_CALCRECT passed"), rc.left, rc.top,
                 rc.right, rc.bottom);
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Color Filling
***
************************************************************************************/

void
ColorFillingExtTextOut(int testFunc)
{

    info(ECHO, TEXT("%s - Color Filling"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    int     result1;
    RECT    rc = { 0, 0, zx + 1, zy + 1 };
    COLORREF c[2] = { RGB(0, 0, 0), RGB(255, 255, 255) };
    // when we do a PatBlt to whiteness, we expect 0 non-white pixels,
    // when we pat blt to blackness, we expect all black pixels.
    int     expect[2] = { zx * zy, 0 };
    DWORD   op[2] = { WHITENESS, BLACKNESS };

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        for (int i = 0; i < 2; i++)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, op[i]));
            CheckNotRESULT(CLR_INVALID, SetBkColor(hdc, c[i]));
            CheckNotRESULT(CLR_INVALID, SetTextColor(hdc, c[i]));
            CheckNotRESULT(0, ExtTextOut(hdc, 0, 0, ETO_OPAQUE | ETO_IGNORELANGUAGE, &rc, TEXT("Should not see this"), 19, NULL));

            // 1bpp brushes (which ExtTextOut uses to fill) are always the bkcolor, thus this isn't
            // a valid test for 1bpp.
            if(myGetBitDepth() != 1)
            {
                result1 = CheckAllWhite(hdc, FALSE, expect[i]);
                if (result1 != expect[i])
                {
                    info(FAIL, TEXT("pass:%d colored pixels expected:%d != result:%d"), i, expect[i], result1);
                    OutputBitmap(hdc, zx, zy);
                }
            }
        }
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Special Params
***
************************************************************************************/

void
SpecialParams(int testFunc)
{

    info(ECHO, TEXT("%s - using <%s>"), funcName[testFunc], fontArray[aFont].tszFaceName);

    TDC     hdc = myGetDC();
    HFONT   hFont, hFontStock;
    int     len;

    CheckNotRESULT(NULL, hFont = setupKnownFont(aFont, 52, 0, 0, 0, 0, 0));
    CheckNotRESULT(NULL, hFontStock = (HFONT) SelectObject(hdc, hFont));

    len = _tcslen(fontArray[aFont].tszFaceName) + 1;

    CheckForRESULT(len, GetTextFace(hdc, 128, NULL));

    CheckNotRESULT(NULL, SelectObject(hdc, hFontStock));
    CheckNotRESULT(FALSE, DeleteObject(hFont));

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   DrawText CheckUnderline
***
************************************************************************************/

void
DrawTextCheckUnderline(int testFunc)
{
    info(ECHO, TEXT("%s - DrawText CheckUnderline"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HFONT   hFont, hFontStock;
    RECT    r = { 0, 0, zx, zy };
    int     result,
            expected = 120;
    int    FontToUse = (tahomaFont != -1) ? tahomaFont : (legacyTahomaFont != -1 ? legacyTahomaFont : BigTahomaFont);

    if(FontToUse == -1)
        info(DETAIL, TEXT("This test required the Tahoma or Big Tahoma font to be available"));
    else
    {
        if (!IsWritableHDC(hdc))
            info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
        else
        {
            CheckNotRESULT(NULL, hFont = setupKnownFont(FontToUse, 12, 0, 0, 0, 1, 0));
            CheckNotRESULT(NULL, hFontStock = (HFONT) SelectObject(hdc, hFont));

            // clear the screen for CheckAllWhite
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            CheckNotRESULT(CLR_INVALID, SetTextColor(hdc, RGB(0, 0, 0)));
            CheckNotRESULT(CLR_INVALID, SetBkColor(hdc, RGB(255, 255, 255)));

            CheckNotRESULT(0, DrawText(hdc, TEXT("                        "), -1, &r, DT_SINGLELINE));
            result = CheckAllWhite(hdc, FALSE, expected);

            if(hFont)                 // if font is missing: don't need to check further
            {
                if (result < expected - 60 || result > expected + 60)
                {
                    info(FAIL, TEXT("Found %d Colored Pixels NT draw:%d"), result, expected);
                }

                CheckNotRESULT(NULL, SelectObject(hdc, hFontStock));
                CheckNotRESULT(FALSE, DeleteObject(hFont));
            }
        }
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   DrawText Expand Tabs
***
************************************************************************************/

void
DrawTextExpandTabs(int testFunc)
{
    info(ECHO, TEXT("%s - DrawText Check Expand Tabs"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    RECT    r = { 0, 0, zx, zy };
    int     result,
            expected = 0;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

        CheckNotRESULT(NULL, SelectObject(hdc, (HFONT) GetStockObject(SYSTEM_FONT)));
        CheckNotRESULT(CLR_INVALID, SetTextColor(hdc, RGB(0, 0, 0)));
        CheckNotRESULT(CLR_INVALID, SetBkColor(hdc, RGB(255, 255, 255)));

        CheckNotRESULT(0, DrawText(hdc, TEXT("\t\t\t\t\t\t\t\t\t"), -1, &r, DT_EXPANDTABS));
        result = CheckAllWhite(hdc, FALSE, expected);
        if (result != expected)
            info(FAIL, TEXT("Found %d Colored Pixels NT draw:%d"), result, expected);
    }

    myReleaseDC(hdc);
}

void
DrawText_DT_XXXX(int testFunc)
{
    info(ECHO, TEXT("%s - All DT_XXXX options"), funcName[testFunc]);

    TDC     hdc = myGetDC();

    RECT    r = { 0, 0, zx, zy };
    int     flag[] = {
        DT_TOP,
        DT_LEFT,
        DT_CENTER,
        DT_RIGHT,
        DT_VCENTER,
        DT_BOTTOM,
        DT_WORDBREAK,
        DT_SINGLELINE,
        DT_EXPANDTABS,
        DT_TABSTOP,
        DT_NOCLIP,
        DT_EXTERNALLEADING,
        DT_CALCRECT,
        DT_NOPREFIX,
        DT_INTERNAL,
        DT_EDITCONTROL,
        DT_RTLREADING,
        DT_WORD_ELLIPSIS,
        DT_END_ELLIPSIS
    };
    int     flagSize = sizeof (flag) / sizeof (int);

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
        for (int i = 0; i < flagSize; i++)
            CheckNotRESULT(0, DrawText(hdc, TEXT("This is a DT_Option test."), -1, &r, flag[i]));

    myReleaseDC(hdc);
}

void
DrawTextUnderline(int testFunc)
{
    info(ECHO, TEXT("%s - Check Underline Color of DrawText"), funcName[testFunc]);

    HFONT   hFont,
            hFontTemp;
    RECT    rect = { 0, 0, zx, zy };
    int     c = 0;

    TDC     hdc = myGetDC();

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, SelectObject(hdc, GetStockObject(BLACK_PEN)));
        CheckNotRESULT(NULL, SelectObject(hdc, GetStockObject(BLACK_BRUSH)));

        CheckNotRESULT(NULL, hFont = setupKnownFont(aFont, 18, 0, 0, 0, 1, 0));
        CheckNotRESULT(NULL, hFontTemp = (HFONT) SelectObject(hdc, hFont));

        for (int y = 0; y < zy; c += 32, y += 30)
        {
            rect.top = y;
            rect.bottom = y + 30;
            CheckNotRESULT(CLR_INVALID, SetTextColor(hdc, RGB(c % 256, c % 256, c % 256)));
            CheckNotRESULT(NULL, DrawText(hdc, TEXT("Check for color change in underline"), -1, (LPRECT) & rect, 0));
        }

        CheckNotRESULT(NULL, hFontTemp = (HFONT) SelectObject(hdc, hFontTemp));
        passCheck(hFontTemp == hFont, TRUE, TEXT("Incorrect font returned"));
        CheckNotRESULT(FALSE, DeleteObject(hFont));
    }

    myReleaseDC(hdc);
}

void
DrawTextClipRegion()
{
    info(ECHO, TEXT("DrawTextClipRegion"));
    TDC     hdc = myGetDC();
    BOOL   OldVerifySetting;
    HFONT   hfont,
            hfontSav;
    int     iQuality,
            iRgn,
            i,
            nRet;
    UINT    wFormat = DT_LEFT | DT_WORDBREAK;
    HRGN    hrgn;
    RECT    rc,
            rcRgn;
    LOGFONT lgFont;
    TCHAR   szText[1024];
    TCHAR   szTmp[64];
    TCHAR  *rgszFontName[5] =
        { TEXT("Times New Roman"), TEXT("Courier New"), TEXT("Arial"), TEXT("Tahoma"), TEXT("Times New Roman") };
    int     rgnFontHeight[] = { 11, 14, 25, 49, 58 };   // countof rgnFontHeight == rgnFontWeight
    int     rgnFontWeight[] = { 900, 700, 400, 300, 400 };
    BYTE    rgbFontQuality[] = { ANTIALIASED_QUALITY,
                                                 CLEARTYPE_QUALITY,
                                                 //CLEARTYPE_COMPAT_QUALITY,
                                                 NONANTIALIASED_QUALITY,
                                                 //DEFAULT_QUALITY,
                                                 //DRAFT_QUALITY
                                                };

    memset((LPVOID) & lgFont, 0, sizeof (LOGFONT));
    lgFont.lfCharSet = DEFAULT_CHARSET;

    nRet = LoadString(globalInst, IDS_String_9962, szText, 1024);
    info(DETAIL, TEXT("DrawTextClipRegion: LoadString:  nRet=%ld\r\n"), nRet);

    rc.left = rc.top = 0;
    rc.right = zx;
    rc.bottom = zy;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        for (iQuality = 0; iQuality < countof(rgbFontQuality); iQuality++)
        {
            lgFont.lfQuality = rgbFontQuality[iQuality];

            // if we're testing cleartype quality, turn off verification because the test driver doesn't
            // support cleartype so the character positions will be off.
            // if verification is already turned off, don't turn it on.
            OldVerifySetting = SetSurfVerify(!(rgbFontQuality[iQuality] == CLEARTYPE_QUALITY) && GetSurfVerify());

            for (iRgn = 0; iRgn < 2; iRgn++)
            {
                // set up screen
                CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
                // clip regsion
                rcRgn.left = GenRand() % 200 + 1;
                rcRgn.top = GenRand() % 200 + 1;

                nRet = (GenRand() % 80) + 120;
                rcRgn.right = nRet + rcRgn.left;

                nRet = (GenRand() % 80) + 120;
                rcRgn.bottom = nRet + rcRgn.top;
                CheckNotRESULT(NULL, hrgn = CreateRectRgn(rcRgn.left, rcRgn.top, rcRgn.right, rcRgn.bottom));
                if (!hrgn)
                    continue;

                CheckNotRESULT(ERROR, nRet = SelectClipRgn(hdc, hrgn));
                if (nRet == ERROR)
                {
                    info(FAIL, TEXT("iQuality=%d: iRgn=%d:  SelectClipRgn() fail: err=%ld\r\n"), iQuality, iRgn, GetLastError());
                    goto LDeleteRgn;
                }

                for (i = 0; i < countof(rgnFontHeight); i++)
                {
                    _tcscpy_s(lgFont.lfFaceName, rgszFontName[i]);
                    lgFont.lfHeight = rgnFontHeight[i];
                    lgFont.lfWeight = rgnFontWeight[i];
                    lgFont.lfItalic = TRUE;
                    lgFont.lfUnderline = TRUE;

                    CheckNotRESULT(NULL, hfont = CreateFontIndirect(&lgFont));
                    CheckNotRESULT(NULL, hfontSav = (HFONT) SelectObject(hdc, hfont));
                    CheckNotRESULT(0, GetTextFace(hdc, 32, szTmp));
                    CheckNotRESULT(0, DrawText(hdc, szText, -1, &rc, wFormat));

                    CheckNotRESULT(NULL, SelectObject(hdc, hfontSav));
                    CheckNotRESULT(FALSE, DeleteObject(hfont));

                    CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

                    nRet = CheckAllWhite(hdc, FALSE, 0);
                    if (nRet != 0)
                    {
                        // found some error: DrawText() draws outside of the clip rec
                        info(FAIL, TEXT("DrawText draws outside clip rect: iQuality=%d: iRgn=%d: clip rect = [%d %d %d %d]\r\n"), iQuality, iRgn, rcRgn.left, rcRgn.top,
                             rcRgn.right, rcRgn.bottom);
                        info(FAIL, TEXT("DrawText draws outside clip rect: i=%d  FontFace=%s"), i, (LPTSTR) szTmp);
                        info(FAIL, TEXT("DrawText draws outside clip rect: CheckAllWhite() failed: found %d pixels are not white"), nRet);
                    }
                }

                CheckNotRESULT(ERROR, SelectClipRgn(hdc, (HRGN) NULL));

              LDeleteRgn:
                CheckNotRESULT(FALSE, DeleteObject(hrgn));
            }

            SetSurfVerify(OldVerifySetting);
        }
    }

    myReleaseDC(hdc);
}

void
AlignExtTestOut(int testFunc)
{
    info(ECHO, TEXT("%s - Alignment checks"), funcName[testFunc]);
    TDC     hdc = myGetDC();

    TCHAR   word[50];
    UINT    len = 0;
    UINT    i = 0, index = 0;
    UINT    result;
    HFONT   hFont, hFontStock;
    NameValPair nvAlignments[] = {
            NAMEVALENTRY(TA_BASELINE),
            NAMEVALENTRY(TA_BOTTOM),
            NAMEVALENTRY(TA_TOP),
            NAMEVALENTRY(TA_LEFT),
            NAMEVALENTRY(TA_RIGHT),
            NAMEVALENTRY(TA_LEFT | TA_TOP),
            NAMEVALENTRY(TA_LEFT | TA_BOTTOM),
            NAMEVALENTRY(TA_LEFT | TA_BASELINE),
            NAMEVALENTRY(TA_RIGHT | TA_TOP),
            NAMEVALENTRY(TA_RIGHT |TA_BOTTOM),
            NAMEVALENTRY(TA_RIGHT |TA_BASELINE),
            NAMEVALENTRY(TA_CENTER | TA_TOP),
            NAMEVALENTRY(TA_CENTER | TA_BOTTOM),
            NAMEVALENTRY(TA_CENTER | TA_BASELINE),
            NAMEVALENTRY(0),
    };

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        //setup
        _tcscpy_s(word, TEXT("The Quick Brown Fox jumps right over the lazy dog"));
        len = _tcslen(word);
        CheckNotRESULT(CLR_INVALID, SetTextColor(hdc, RGB(0, 0, 0)));
        CheckNotRESULT(0, SetBkMode(hdc, TRANSPARENT));
        CheckNotRESULT(CLR_INVALID, SetBkColor(hdc, RGB(255, 0, 0)));

        CheckNotRESULT(NULL, hFont = setupKnownFont(aFont, 25, 0, 0, 0, 0, 0));
        CheckNotRESULT(NULL, hFontStock = (HFONT) SelectObject(hdc, hFont));
        for (i = 0; i < 100; i++)
        {
            index = i % countof(nvAlignments);
            CheckNotRESULT(GDI_ERROR, result = SetTextAlign(hdc, nvAlignments[index].dwVal));

            if (!ExtTextOut(hdc, zx/2, zy/2, ETO_OPAQUE | ETO_IGNORELANGUAGE, NULL, word, len, NULL))
            {
                info(FAIL, TEXT("AlignExtTestOut Failed for %s"), nvAlignments[index].szName, _T("where the last error was %s"), GetLastError());
                break;
            }
        }

        CheckNotRESULT(NULL, SelectObject(hdc, hFontStock));
        CheckNotRESULT(FALSE, DeleteObject(hFont));
    }
    
    myReleaseDC(hdc);
}

void
FontSmoothingChangeTest(int testFunc, BOOL bChangeBefore, BOOL bChangeBeforeSelect)
{
    info(ECHO, TEXT("%s - FontSmoothingChangeTest, %d, %d"), funcName[testFunc], bChangeBefore, bChangeBeforeSelect);

    // base registry key for ClearTypeSettings
    TCHAR szGPERegKey[] = TEXT("System\\GDI\\ClearTypeSettings");

    DWORD dwSize  = sizeof(DWORD);
    DWORD dwType  = 0x00;
    DWORD dwValue = 0x00;
    HKEY  hKey;

    // retreive the key.
    if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, szGPERegKey, 0, 0, &hKey))
    {
        if(ERROR_SUCCESS == RegQueryValueEx(hKey, TEXT("OffOnRotation"), NULL, &dwType, (BYTE*) &dwValue, &dwSize))
        {
            // the registry key type should be DWORD
            CheckForRESULT(REG_DWORD, dwType);
            // the size should be a DWORD
            CheckForRESULT(sizeof(DWORD), dwSize);
            // close the open reg key.
            CheckForRESULT(ERROR_SUCCESS, RegCloseKey(hKey));
        }
    }

    // If the regkey to disable ClearType on rotation is set and rotation is supported,
    // disable rotation for this test only.
    BOOL bOldRotateAvail = gRotateAvail;
    if (dwValue == 0x01 && CheckRotation())
    {
        // return screen to standard orientation
        if (gdwCurrentAngle != 0)
            RandRotate(DMDO_DEFAULT);

        gRotateAvail = FALSE;
    }

    TDC hdc = myGetDC();
    // return original rotation setting
    gRotateAvail = bOldRotateAvail;

    LOGFONT lf;
    BOOL bOriginalFontSmoothing, bNewFontSmoothing;
    int naQualities[] = {
                                 ANTIALIASED_QUALITY,
                                 NONANTIALIASED_QUALITY,
                                 CLEARTYPE_COMPAT_QUALITY,
                                 CLEARTYPE_QUALITY,
                                 DEFAULT_QUALITY,
                                 DRAFT_QUALITY
                                };
    int nQuality;
    HFONT hFontToTest, hFontStockFont;
    TCHAR tcTestString[] = TEXT("Test String");
    RECT rc = {0, 0, zx, zy};
    RECT rcClip = { 0, 0, zx/2, zy };
    int nLeadingSpace = 10;
    HRGN hrgn;

    memset(&lf, 0, sizeof(LOGFONT));

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hrgn = CreateRectRgnIndirect(&rcClip));

        // retrieve the current fontsmoothing
        CheckNotRESULT(FALSE, SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &bOriginalFontSmoothing, 0));

        // turn off fontsmoothing
        bNewFontSmoothing = FALSE;
        CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, bNewFontSmoothing, 0, 0));

        // set our base font to cleartype quality on the left side of the screen and blit it to the right so
        // we're always drawing at the same location (possible adjustment by gdi for pixel locations).
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
        CheckNotRESULT(NULL, hFontToTest = (HFONT) GetCurrentObject(hdc, OBJ_FONT));
        CheckNotRESULT(0, GetObject(hFontToTest, sizeof(LOGFONT), &lf));
        lf.lfQuality = CLEARTYPE_COMPAT_QUALITY;
        CheckNotRESULT(NULL, hFontToTest = CreateFontIndirect(&lf));
        CheckNotRESULT(NULL, hFontStockFont = (HFONT) SelectObject(hdc, hFontToTest));
        CheckNotRESULT(FALSE, ExtTextOut(hdc, 0 + nLeadingSpace, 0, ETO_IGNORELANGUAGE, &rc, tcTestString, _tcslen(tcTestString), NULL));
        CheckNotRESULT(FALSE, BitBlt(hdc, zx/2, 0, zx/2, zy, hdc, 0, 0, SRCCOPY));
        CheckNotRESULT(NULL, SelectObject(hdc, hFontStockFont));
        CheckNotRESULT(FALSE, DeleteObject(hFontToTest));

        // if bChangeBefore, set the font type prior to creating the font.
        if(bChangeBefore)
        {
            // turn on font smoothing to verify that everything now is cleartype quality.
            bNewFontSmoothing = TRUE;
            CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, bNewFontSmoothing, 0, 0));
        }

        for(nQuality = 0; nQuality < countof(naQualities); nQuality++)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx/2, zy, WHITENESS));

            // if !bChangeBefore, set the font type after creating the font.
            if(!bChangeBefore)
            {
                // turn on font smoothing to verify that everything now is cleartype quality.
                bNewFontSmoothing = FALSE;
                CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, bNewFontSmoothing, 0, 0));
            }

            // due to global font smoothing, everything should be cleartype and match the base font.
            lf.lfQuality = naQualities[nQuality];
            CheckNotRESULT(NULL, hFontToTest = CreateFontIndirect(&lf));

            // if !bChangeBefore and bChangeBeforeSelect, set the font type after creating the font but before the selection
            if(!bChangeBefore && bChangeBeforeSelect)
            {
                // turn on font smoothing to verify that everything now is cleartype quality.
                bNewFontSmoothing = TRUE;
                CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, bNewFontSmoothing, 0, 0));
            }

            CheckNotRESULT(NULL, hFontStockFont = (HFONT) SelectObject(hdc, hFontToTest));

            // if !bChangeBefore, set the font type after creating the font and after the selection.
            if(!bChangeBefore && !bChangeBeforeSelect)
            {
                // turn on font smoothing to verify that everything now is cleartype quality.
                bNewFontSmoothing = TRUE;
                CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, bNewFontSmoothing, 0, 0));
            }

            // draw to the left side of the screen so the location matches base for comparison.
            CheckNotRESULT(ERROR, SelectClipRgn(hdc, hrgn));
            CheckNotRESULT(FALSE, ExtTextOut(hdc, nLeadingSpace, 0, ETO_IGNORELANGUAGE, &rc, tcTestString, _tcslen(tcTestString), NULL));
            CheckNotRESULT(ERROR, SelectClipRgn(hdc, NULL));

            CheckNotRESULT(NULL, SelectObject(hdc, hFontStockFont));
            CheckNotRESULT(FALSE, DeleteObject(hFontToTest));

            if(NONANTIALIASED_QUALITY == naQualities[nQuality])
            {
                info(DETAIL, TEXT("CheckScreenHalves expected to fail with NONANTIALIASED_QUALITY, skipping check"));
            }
            else
            {
                if(!CheckScreenHalves(hdc))
                    info(FAIL, TEXT("Failed on case %d"), nQuality);
            }
        }

        DeleteObject(hrgn);
        CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, bOriginalFontSmoothing, 0, 0));
    }

    myReleaseDC(hdc);
}

// buffer size for small strings.
#define  COMMON_DRWTXT_LEN 32

void
DrawTextStringTest(int testFunc)
{
    info(ECHO, TEXT("%s - DrawTextStringTest"), funcName[testFunc]);
    TDC hdc = myGetDC();

    TCHAR tcTestString[COMMON_DRWTXT_LEN * 2];
    // rectangles for the left and right sides of the screen.
    RECT rcL = {0, 0, zx/2, zy};
    RECT rcR = {zx/2, 0, zx, zy};
    int entry, i;
    // interesting string lengths to pass to DrawText.
    int nStringLengths[] = {
                                        0,
                                        1,
                                        2,
                                        COMMON_DRWTXT_LEN - 2,
                                        COMMON_DRWTXT_LEN - 1,
                                        COMMON_DRWTXT_LEN,
                                        COMMON_DRWTXT_LEN + 1,
                                        COMMON_DRWTXT_LEN + 2,
                                        countof(tcTestString) - 1
                                        };

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        for(entry = 0; entry < countof(nStringLengths); entry++)
        {
            // clear the screen
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

            // fill the string
            for(i = 0; i < nStringLengths[entry]; i++)
                tcTestString[i] = 'a';
            tcTestString[i] = NULL;

            // draw the left telling DrawText to calculate the string length
            if(!DrawText(hdc, tcTestString, -1, &rcL, 0))
                info(FAIL, TEXT("DrawText failed on string length %d"), nStringLengths[entry]);

            // draw it again giving it the string length
            if(!DrawText(hdc, tcTestString, nStringLengths[entry], &rcR, 0))
                info(FAIL, TEXT("DrawText failed on string length %d"), nStringLengths[entry]);

            // verify it drew correctly.
            CheckScreenHalves(hdc);
        }
    }

    myReleaseDC(hdc);
}

#define DRAW_NOBLEED 2  // eliminate bleed over on ends

void
DrawTextPrefixTest(int testFunc)
{
#ifdef UNDER_CE
    // BUGBUG: Need to check presence of keyboard first PS#100337
    if(GetKeyboardStatus() == 0)
    {
        info(DETAIL, TEXT("Keyboard not present, skipping DrawTextPrefixTest"));
        return;
    }
#endif

    info(ECHO, TEXT("%s - DrawTextPrefixTest"), funcName[testFunc]);
    TDC hdc = myGetDC();
    RECT rcL = {DRAW_NOBLEED, 0, zx/2, zy};
    RECT rcR = {zx/2 + DRAW_NOBLEED, 0, zx, zy};
    TCHAR tcTestString[] = TEXT("&&&&&&&&&&&&&&&&");
    TCHAR tcTestStringPrefixed[] = TEXT("&X&b&c&d&e&f&g&h");
    TCHAR tcTestStringNoPrefix[] = TEXT("Xbcdefgh");

    // For the Hanging Test Strings, we'll draw the given
    // string on one side and replace the ' ' with '&' and
    // draw on the other side.
    // This is deliberately not const - we'll be modifying in place
    TCHAR rgHangingTestStrings[][32] = {
        TEXT("Xbcdefgh "),
        TEXT("Xbcdefgh "),
        TEXT("Xbcdefg "),
        TEXT("Xbcdefg "),
        TEXT("&&Xbcdefgh "),
        TEXT("X&&bcdefgh "),
        TEXT("&&Xbcdefg "),
        TEXT("X&&bcdefg "),
        TEXT("Xbcdefg&&h "),
        TEXT("Xbcdefgh&& "),
        TEXT("Xbcdef&&g "),
        TEXT("Xbcdefg&& "),
        TEXT("Display && Themes "),
        TEXT("Display Themes "),
    };

    HFONT hfontNormal, hfontStock;
    HFONT hfontUnderline;

    // Use a random character as our prefix base
    TCHAR Randy = '!' + GenRand() % ('z' - '!');
    if(Randy == '&')        // no no..use a character that won't effect our tests
        Randy = 'a';
    tcTestStringPrefixed[1] = Randy;
    tcTestStringNoPrefix[0] = Randy;

    // some fonts don't succeed with this, the spacing isn't the same for both runs.
    int    FontToUse = (tahomaFont != -1) ? tahomaFont : (legacyTahomaFont != -1 ? legacyTahomaFont : BigTahomaFont);
    if(FontToUse == -1)
        info(DETAIL, TEXT("This test required the Tahoma or Big Tahoma font to be available"));
    else
    {
        if (!IsWritableHDC(hdc))
            info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
        else
        {
            CheckNotRESULT(NULL, hfontNormal = setupKnownFont(FontToUse, 0, 0, 0, 0, 0, 0));
            CheckNotRESULT(NULL, hfontUnderline = setupKnownFont(FontToUse, 0, 0, 0, 0, 1, 0));

            // a single prefix & with no following character.
            PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
            CheckNotRESULT(FALSE, DrawText(hdc, tcTestString, 1, &rcL, 0));
            CheckAllWhite(hdc, TRUE, 0);

            // a whole bunch of prefix characters, even's and odds
            CheckNotRESULT(FALSE, DrawText(hdc, tcTestString, -1, &rcL, 0));
            CheckNotRESULT(FALSE, DrawText(hdc, tcTestString, 2, &rcL, 0));
            CheckNotRESULT(FALSE, DrawText(hdc, tcTestString, 3, &rcL, 0));
            CheckNotRESULT(FALSE, DrawText(hdc, tcTestString, 4, &rcL, 0));
            CheckNotRESULT(FALSE, DrawText(hdc, tcTestString, 5, &rcL, 0));

            // && prefixed compared to &
            info(DETAIL, TEXT("Testing that the prefix escape draws without an underline"));
            CheckNotRESULT(NULL, hfontStock = (HFONT) SelectObject(hdc, hfontNormal));
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            CheckNotRESULT(FALSE, DrawText(hdc, tcTestString, 2, &rcL, 0));
            CheckNotRESULT(FALSE, DrawText(hdc, tcTestString, 1, &rcR, DT_NOPREFIX));
            CheckNotRESULT(NULL, SelectObject(hdc, hfontStock));
            CheckScreenHalves(hdc);

            // && prefixed compared to & with an underlined font
            info(DETAIL, TEXT("Testing that prefix escape draws with an underline when using an underlined font"));
            CheckNotRESULT(NULL, hfontStock = (HFONT) SelectObject(hdc, hfontUnderline));
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            CheckNotRESULT(FALSE, DrawText(hdc, tcTestString, 2, &rcL, 0));
            CheckNotRESULT(FALSE, DrawText(hdc, tcTestString, 1, &rcR, DT_NOPREFIX));
            CheckNotRESULT(NULL, SelectObject(hdc, hfontStock));
            CheckScreenHalves(hdc);

#ifdef UNDER_CE
            // desktop's underline for prefixed characters is thicker than the normal underline,
            // so this test isn't valid.
            info(DETAIL, TEXT("Testing that prefixed character draws with an underline"));

            // If we're in RTL, we need to add an allowable error
            if (g_bRTL)
            {
                // Passing a negative number indicates the allowable error is in absolute pixels
                SetMaxScreenHalvesErrorPercentage(-2.0);
            }
            
            // &a vs a w/ underlined font
            CheckNotRESULT(NULL, hfontStock = (HFONT) SelectObject(hdc, hfontNormal));
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            CheckNotRESULT(FALSE, DrawText(hdc, tcTestStringPrefixed, 2, &rcL, 0));
            CheckNotRESULT(NULL, SelectObject(hdc, hfontUnderline));
            CheckNotRESULT(FALSE, DrawText(hdc, tcTestStringNoPrefix, 1, &rcR, DT_NOPREFIX));
            CheckNotRESULT(NULL, SelectObject(hdc, hfontStock));
            CheckScreenHalves(hdc);

            CheckNotRESULT(NULL, hfontStock = (HFONT) SelectObject(hdc, hfontNormal));
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            CheckNotRESULT(0, SetROP2(hdc, R2_NOP));
            CheckNotRESULT(FALSE, DrawText(hdc, tcTestStringPrefixed, 2, &rcL, 0));
            CheckNotRESULT(0, SetROP2(hdc, R2_COPYPEN));
            CheckNotRESULT(NULL, SelectObject(hdc, hfontStock));
            CheckNotRESULT(NULL, hfontStock = (HFONT) SelectObject(hdc, hfontUnderline));
            CheckNotRESULT(FALSE, DrawText(hdc, tcTestStringNoPrefix, 1, &rcR, DT_NOPREFIX));
            CheckNotRESULT(NULL, SelectObject(hdc, hfontStock));
            CheckScreenHalves(hdc);     // off by 1 pixel

            // &a vs a w/ underlined font, w/ underlined font with a no op rop2
            CheckNotRESULT(NULL, hfontStock = (HFONT) SelectObject(hdc, hfontNormal));
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            CheckNotRESULT(FALSE, DrawText(hdc, tcTestStringPrefixed, 2, &rcL, 0));
            CheckNotRESULT(NULL, SelectObject(hdc, hfontStock));

            CheckNotRESULT(NULL, hfontStock = (HFONT) SelectObject(hdc, hfontUnderline));
            CheckNotRESULT(0, SetROP2(hdc, R2_NOP));
            CheckNotRESULT(FALSE, DrawText(hdc, tcTestStringNoPrefix, 1, &rcR, DT_NOPREFIX));
            CheckNotRESULT(0, SetROP2(hdc, R2_COPYPEN));
            CheckNotRESULT(NULL, SelectObject(hdc, hfontStock));
            
            CheckScreenHalves(hdc);     // off by 1 pixel

            if (g_bRTL)
            {
                ResetScreenHalvesCompareConstraints();
            }
#endif

            info(DETAIL, TEXT("Testing with hanging prefix"));
            for (int i = 0; i < _countof(rgHangingTestStrings); ++i)
            {
                int iTrailingSpace = _tcslen(rgHangingTestStrings[i]) - 1;
                if (rgHangingTestStrings[i][iTrailingSpace] != _T(' '))
                {
                    info(ABORT, TEXT("Invalid test condition: rgHangingTestStrings values must have trailing ' '"));
                }

                // && prefixed compared to &
                CheckNotRESULT(NULL, hfontStock = (HFONT) SelectObject(hdc, hfontNormal));
                CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

                // Draw with no trailing characters (using the trailing ' ' in RTL throws it off)
                rgHangingTestStrings[i][iTrailingSpace] = 0;
                CheckNotRESULT(FALSE, DrawText(hdc, rgHangingTestStrings[i], -1, &rcL, 0));
                
                // Draw with ' ' changed to '&'
                rgHangingTestStrings[i][iTrailingSpace] = _T('&');
                CheckNotRESULT(FALSE, DrawText(hdc, rgHangingTestStrings[i], -1, &rcR, 0));
                
                CheckNotRESULT(NULL, SelectObject(hdc, hfontStock));
                if (!CheckScreenHalves(hdc))
                {
                    info(FAIL, TEXT("Failure above with string \"%s\""), rgHangingTestStrings[i]);
                }
            }

            info(DETAIL, TEXT("Testing a set of prefixed characters"));
            CheckNotRESULT(NULL, hfontStock = (HFONT) SelectObject(hdc, hfontNormal));
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            // desktop behavior : last character gets the prefix, others don't.
            CheckNotRESULT(FALSE, DrawText(hdc, tcTestStringPrefixed, -1, &rcL, 0));
            CheckNotRESULT(NULL, SelectObject(hdc, hfontStock));

            // this deselects the font and cleans up the allocations.
            CheckNotRESULT(FALSE, DeleteObject(hfontNormal));
            CheckNotRESULT(FALSE, DeleteObject(hfontUnderline));
        }
    }
    myReleaseDC(hdc);
}


void
DrawTextEllipsisTest(int testFunc)
{
    info(ECHO, TEXT("%s - DrawTextEllipsisTest"), funcName[testFunc]);
    #define STRING_LENGTH 256

    TDC hdc = myGetDC();
    RECT rc;
    TCHAR tcString[STRING_LENGTH] = { NULL };
    int nWidthToRemove;
    nWidthToRemove = (zx/2) / ((zy/16) - 2*DRAW_NOBLEED);

    #define NUMFLAGS 12
    UINT nFlags[][2] = {
                                    {DT_WORD_ELLIPSIS, DT_WORD_ELLIPSIS},
                                    {DT_END_ELLIPSIS, DT_END_ELLIPSIS},
                                    {DT_END_ELLIPSIS, DT_END_ELLIPSIS | DT_SINGLELINE},
                                    {DT_END_ELLIPSIS, DT_END_ELLIPSIS | DT_NOPREFIX},
                                    {DT_END_ELLIPSIS, DT_END_ELLIPSIS | DT_LEFT},
                                    {DT_END_ELLIPSIS, DT_END_ELLIPSIS | DT_TOP},
                                    {DT_END_ELLIPSIS | DT_RIGHT, DT_END_ELLIPSIS | DT_RIGHT},
                                    {DT_END_ELLIPSIS | DT_CENTER, DT_END_ELLIPSIS | DT_CENTER},
                                    {DT_END_ELLIPSIS | DT_NOCLIP, DT_END_ELLIPSIS | DT_NOCLIP},
                                    {DT_END_ELLIPSIS, DT_WORD_ELLIPSIS},
                                    {DT_WORD_ELLIPSIS, DT_END_ELLIPSIS},
                                    {DT_WORD_ELLIPSIS | DT_SINGLELINE, DT_END_ELLIPSIS}
                                 };
    UINT Index;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        for(Index = 0; Index < NUMFLAGS; Index++)
        {
            PatBlt(hdc, 0, 0, zx, zy, WHITENESS);

            for(int i = 0; i < STRING_LENGTH -1; i++)
            {
                if((GenRand() % 7) == 0)
                    tcString[i] = ' ';
                else
                    tcString[i] = 'a';
            }

            rc.left = DRAW_NOBLEED;
            rc.top = 0;
            rc.bottom = 16;

            for(rc.right = zx/2; rc.right  > 0; rc.right -= nWidthToRemove)
            {
                CheckNotRESULT(FALSE, DrawText(hdc, tcString, -1, &rc, nFlags[Index][0]));
                rc.top +=16;
                rc.bottom += 16;
            }

            rc.left = zx/2 + DRAW_NOBLEED;
            rc.top = 0;
            rc.bottom = 16;

            for(rc.right = zx; rc.right  > zx/2; rc.right -= nWidthToRemove)
            {
                CheckNotRESULT(FALSE, DrawText(hdc, tcString, -1, &rc, nFlags[Index][1]));
                rc.top +=16;
                rc.bottom +=16;
            }

            CheckScreenHalves(hdc);
        }
    }

    myReleaseDC(hdc);
    #undef STRING_LENGTH
}

#define CFC_QUALITY 0x0001

HFONT
CreateFontCopy(TDC hdc, HFONT hfOriginal, DWORD dwFlags, LOGFONT * plf)
{
    HFONT hf = NULL;
    HFONT hfFromDC = NULL;
    LOGFONT lfNewFont = {0};

    if (!plf)
    {
        return NULL;
    }

    // First get the hfont if we don't have one
    if (NULL == hfOriginal)
    {
        CheckNotRESULT(NULL, hfFromDC = (HFONT)GetStockObject(SYSTEM_FONT));
        CheckNotRESULT(NULL, hfFromDC = (HFONT)SelectObject(hdc, (HGDIOBJ)hfFromDC));
        // Now hf contains the HFONT that was in the DC

        // Get the information for the original test font
        CheckNotRESULT(FALSE, GetObject((HGDIOBJ)hfFromDC, sizeof(lfNewFont), &lfNewFont));
    }
    else
    {
        // Get the information for the original test font
        CheckNotRESULT(FALSE, GetObject((HGDIOBJ)hfOriginal, sizeof(lfNewFont), &lfNewFont));
    }


    if (dwFlags & CFC_QUALITY)
    {
        // Update the quality
        lfNewFont.lfQuality = plf->lfQuality;
    }

    // Create the updated font
    CheckNotRESULT(NULL, hf = CreateFontIndirect(&lfNewFont));

    if (hfFromDC)
    {
        // Select the original font back in.
        CheckNotRESULT(NULL, hfFromDC = (HFONT)SelectObject(hdc, (HGDIOBJ)hfFromDC));
        // Now hf is the stock system font and the original font is in the DC
        CheckNotRESULT(FALSE, DeleteObject(hfFromDC));
    }

    return hf;
}
void
TextDrawSpacingTest(int testFunc, BOOL bUseDX, BOOL bUseSetTextCharacterExtra, BOOL bTestSmoothedFont)
{
    if (IsUniscribeImage())
    {
        info(ECHO, TEXT("%s - Skipping TextDrawSpacingTest. Test not supported on Uniscribe images."), funcName[testFunc]);
        return;
    }

    info(ECHO, TEXT("%s - TextDrawSpacingTest"), funcName[testFunc]);
    TDC hdc = myGetDC();
    TCHAR tcTextString[MAXSTRINGLENGTH];

    int nCharacterWidth;
    int *pnDxAdjustment = NULL, nDxAdjustment[MAXSTRINGLENGTH];
    int nOldTextCharacterExtra = 0, nNewTextCharacterExtra;
    int nOldBKMode;
    int index;
    int nMaxCharacterCountToUse;
    // warning 22019 indicates that zy might be < 0. We know it isn't.
#pragma warning(suppress:22019)
    RECT rcBase = { zx/4, 0, zx/2, zy };
    RECT rcCurrent;
    // our right half rectangle to use.
    RECT rcComparisonRect = {3*zx/4, 0, zx, zy };
    RECT rcLeft = { 0, 0, zx/2, zy };
    RECT rcRight = { zx/2, 0, zx, zy };
    SIZE sz;
    POINT point;        // draw alignment marks for inspecting results
    HRGN hrgnClip;
    LOGFONT lfNonAntiAliased = {0};
    HFONT hf = NULL;

    if (!bTestSmoothedFont)
    {
        //
        // We need to create a font that is nonantialiased... (otherwise the pixel-perfect comparison will fail
        // due to differences in how overlapped characters are drawn singly versus all together)
        //

        // Update the quality
        lfNonAntiAliased.lfQuality = NONANTIALIASED_QUALITY;
        CheckNotRESULT(NULL, hf = CreateFontCopy(hdc, NULL, CFC_QUALITY, &lfNonAntiAliased));

        // Select the new nonantialiased font into our DC
        CheckNotRESULT(NULL, hf = (HFONT)SelectObject(hdc, (HGDIOBJ)hf));
        // Now hf is holding the original test font's handle
    }

    // SetTextCharacterExtra not available on raster font systems.
    if(fontArray[aFont].dwType != TRUETYPE_FONTTYPE)
        bUseSetTextCharacterExtra = FALSE;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
        if(!g_bRTL)
        {
            _tcscpy_s(tcTextString, TEXT("A a1 !Bb 2@Cc Ww Ii 3#Dd4 $Ee5%F f6^."));
        }
        else
        {
            rcBase.right = zx/4;

            // When going RTL we don't want symbol/space reordering so use dif. string
            _tcscpy_s(tcTextString, TEXT("ABa1CcBbD2wCcdWwEIie3QDd4wMEe5tFSf6mn"));
        }

        nMaxCharacterCountToUse = _tcslen(tcTextString);

        // Using lpDX parameter
        if(bUseDX)
            pnDxAdjustment = nDxAdjustment;

        CheckNotRESULT(0, nOldBKMode = SetBkMode(hdc, TRANSPARENT));

        // the extents to move down a line (grab these here because we'll be printing things one line at
        // a time and copying from one side to the other to ensure that cleartype differences from
        // location to location don't affect us).
        CheckNotRESULT(FALSE, GetTextExtentPoint(hdc, tcTextString, _tcslen(tcTextString), &sz));
        rcBase.bottom = rcBase.top + sz.cy;
        rcComparisonRect.bottom = rcBase.bottom;

        while(rcBase.top < zy)
        {
            nNewTextCharacterExtra = 0;
            // for !dx, rccurrent = rcbase at the start.
            rcCurrent = rcBase;

            // if use character extra, set the extra space we're going to use.
            // don't allow the characters to touch, or they'll end up blending into eachother when using antialiasing.
            if(bUseSetTextCharacterExtra)
                nNewTextCharacterExtra = (GenRand() % 10);

            // if used DX
            if(bUseDX)
            {
                // initialize our random dx adjustment
                for(index = 0; index < (int) _tcslen(tcTextString); index++)
                {
                    nDxAdjustment[index] = (GenRand() % 20) - 10;
                }
            }

            DWORD dwFlags = DT_TOP | DT_NOCLIP | DT_NOPREFIX | DT_SINGLELINE;
            if(!g_bRTL)
                dwFlags |= DT_LEFT;
            else
                dwFlags |= DT_RIGHT;

            // select the clip region for the near side of the screen
            // DrawText output will end up on the Right side of the screen with the
            // ExtTextOut output on the Left side of the screen. (The
            // DrawText output goes to the Left side and is then copied over to the right).
            CheckNotRESULT(NULL, hrgnClip = CreateRectRgnIndirect(&rcLeft));
            CheckNotRESULT(ERROR, SelectClipRgn(hdc, hrgnClip));
            CheckNotRESULT(FALSE, DeleteObject(hrgnClip));

            for(index = 0; index < nMaxCharacterCountToUse; index++)
            {
                // draw the current character at the requested rectangle.
                CheckNotRESULT(0, DrawText(hdc, &(tcTextString[index]), 1, &rcCurrent, dwFlags));

                // update the current rectangle for the next character.
                if(bUseDX)
                {
                    // draw at base plus lpdx plus the extra for current character
                    if(!g_bRTL)
                        rcCurrent.left += (nDxAdjustment[index] + nNewTextCharacterExtra);
                    else
                        rcCurrent.right -= (nDxAdjustment[index] + nNewTextCharacterExtra);
                }
                else
                {
                    int nAppliedWidth;

                    // get the width for the current character
                    CheckNotRESULT(FALSE, GetCharWidth32(hdc, tcTextString[index], tcTextString[index], &nCharacterWidth));

                    // if extra width > individual character width, it's applied
                    // if extra width - individual width > 0, it's applied
                    // if extra width - individual width < 0, it's ignored.
                    if(nNewTextCharacterExtra + nCharacterWidth > 0)
                        nAppliedWidth = nNewTextCharacterExtra + nCharacterWidth;
                    else
                        nAppliedWidth = nCharacterWidth;

                    //Update the current position for the next character.
                    if(!g_bRTL)
                        rcCurrent.left += nAppliedWidth;
                    else
                        rcCurrent.right -= nAppliedWidth;
                }
            }

            // Copy DrawText result to the right half of the screen
            CheckNotRESULT(ERROR, SelectClipRgn(hdc, NULL));
            CheckNotRESULT(FALSE, BitBlt(hdc, rcRight.left, rcBase.top, rcLeft.right - rcLeft.left, sz.cy, hdc, rcLeft.left, rcBase.top, SRCCOPY));
            CheckNotRESULT(FALSE, PatBlt(hdc, rcLeft.left, rcBase.top, rcLeft.right - rcLeft.left, sz.cy, WHITENESS));

            // select the clip region for the same side of the screen again.
            CheckNotRESULT(NULL, hrgnClip = CreateRectRgnIndirect(&rcLeft));
            CheckNotRESULT(ERROR, SelectClipRgn(hdc, hrgnClip));
            CheckNotRESULT(FALSE, DeleteObject(hrgnClip));

            // Enable the text character extra for drawing the comparison string.
            // SetTextCharacterExtra not implemented on raster fonts.
            if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
                CheckNotRESULT(0x80000000, nOldTextCharacterExtra = SetTextCharacterExtra(hdc, nNewTextCharacterExtra));

            if(!g_bRTL)
            {
                CheckNotRESULT(FALSE, ExtTextOut(hdc, rcBase.left, rcBase.top, 0, NULL, tcTextString, nMaxCharacterCountToUse, pnDxAdjustment));
            }
            else
            {
                int old = SetTextAlign(hdc, TA_RIGHT);
                int right = rcBase.left;

                CheckNotRESULT(FALSE, ExtTextOut(hdc, right, rcBase.top, 0, NULL, tcTextString, nMaxCharacterCountToUse, pnDxAdjustment));
                SetTextAlign(hdc, old);
            }

            // restore the character spacing.
            // SetTextCharacterExtra not implemented on raster fonts.
            if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
                CheckNotRESULT(0x80000000, SetTextCharacterExtra(hdc, nOldTextCharacterExtra));

            // clear out the clip region now that we're done with it.
            CheckNotRESULT(ERROR, SelectClipRgn(hdc, NULL));

            // move down a line
            rcBase.top += sz.cy;
            rcBase.bottom += sz.cy;
            rcComparisonRect.top = rcBase.top;
            rcComparisonRect.bottom = rcBase.bottom;
        }

        // Since we're copying from left to right and clearing the left before drawing to the left,
        // we need to draw the reference lines after drawing the text)
        if(!g_bRTL)
        {
            CheckNotRESULT(FALSE, MoveToEx(hdc, rcBase.left, rcLeft.top, &point));
            CheckNotRESULT(FALSE, LineTo(hdc, rcBase.left, rcLeft.bottom));
            CheckNotRESULT(FALSE, MoveToEx(hdc, rcComparisonRect.left, rcLeft.top, &point));
            CheckNotRESULT(FALSE, LineTo(hdc, rcComparisonRect.left, rcLeft.bottom));
        }
        else
        {
            CheckNotRESULT(FALSE, MoveToEx(hdc, rcBase.right, rcLeft.top, &point));
            CheckNotRESULT(FALSE, LineTo(hdc, rcBase.right, rcLeft.bottom));
            CheckNotRESULT(FALSE, MoveToEx(hdc, rcComparisonRect.left, rcLeft.top, &point));
            CheckNotRESULT(FALSE, LineTo(hdc, rcComparisonRect.left, rcLeft.bottom));
        }

        if (bTestSmoothedFont)
        {
            // With smoothed fonts, the error is going to be fairly large. The below
            // options will ensure there aren't complete mismatches while still allowing
            // pretty significant differences. The values were determined experimentally
            // by increasing the tolerances until known good results were consistently passing.
            // The standard fonts pass with 205 and 75. Use larger values here to account for
            // different fonts
            SetScreenHalvesRMSPercentage(225, 95);
            CheckScreenHalves(hdc);
            ResetScreenHalvesCompareConstraints();
        }
        else
        {
            CheckScreenHalves(hdc);
        }
        CheckNotRESULT(0, SetBkMode(hdc, nOldBKMode));
    }

    if (!bTestSmoothedFont)
    {
        // hf currently is holding the original test font (with our nonantialiased font in the DC)
        CheckNotRESULT(NULL, hf = (HFONT)SelectObject(hdc, (HGDIOBJ)hf));

        // Cleanup the font
        CheckNotRESULT(FALSE, DeleteObject(hf));
    }
    myReleaseDC(hdc);
}

void
DrawTextCalcRect(int testFunc)
{
    info(ECHO, TEXT("%s - DrawTextCalcRect"), funcName[testFunc]);
    TDC hdc = myGetDC();
    HRGN hrgnClip = NULL;
    
    DWORD dwSingleLineFlag = 0;

    DWORD rgSingleLineFlags[] = {
        0,
        DT_LEFT,
        DT_TOP,
        DT_VCENTER,
        DT_BOTTOM,
        DT_EXPANDTABS,
        DT_NOPREFIX,
        DT_END_ELLIPSIS,
    };

    DWORD rgMultiLineFlags[] = {
        0,
        DT_CENTER,
        DT_LEFT,
        DT_RIGHT,
        DT_EXPANDTABS,
        DT_WORDBREAK,
        DT_NOPREFIX,
    };

    const DWORD * pCurrentFlagSet = NULL;
    int cCurrentFlags = 0;
    int iCurrentCharExtra = 0;
    const int CHAREXTRA_RANGE = 20;
    int iOriginalCharExtra = 0x80000000;
    
    const TCHAR * rgStrings[] = {
        // Simple text string with lots of words
        _T("a b c d e f g h i j k l m n o p q r s t u v w x y z"),

        // Longer text string (the opening of Dante's Commedia: Dante Alighieri, c. 1310)
        _T("Racconta il divin Poeta, siccome ritrovo; smarrito in una orrida selva, \
e sul mattino giunse ad un colle, a cui volendo salire, fu da alcune \
fiere impetlilo; e che mentre fuggiva da una \
di quelle, vide Virgilio, il quale gli disse che \
lo avrebbe guidato ali' Inferno ed al Purgatorio, \
e di poi sarebbe condotto al Paradiso; ed egli \
con la scorta di lui intraprende il gran viaggio."),

        // No wordbreaks in this one
        _T("asldkfjaiuyasuiyasduifywehjirjhwerjhxcvxiucvyiuyeriuhjhdfasdfmncvyiuiysihwerjhdf"),

        // Tabs in this one
        _T("kjlasd\tasdkjfadf\t asldkfj asdlkfj \t"),

        // Newlines in this one
        _T("laksjdfkj\nlaksjdhf\nsakjhfalk jhdflakjsh dfalkjhfasd asdfhkljashdf\nasldkjfhlajkhsf\n"),

        // Prefixes here
        _T("&alksdf&&alksdf&alskdjf aljksdf&h asdj&asdf &&asdf&s"),

        // Short versions of the above
        _T("a b c d"),
        _T("asldfkj"),
        _T("&akd&&as"),
        _T("abc\tdef"),
        _T("abc\ndef"),
    };

    RECT rcCalcRect = {0};
    RECT rcAdjusted = {0};
    RECT rcLeft = {0, 0, zx/2, zy};
    RECT rcRight = {zx/2, 0, zx, zy};
    RECT rcLastPixelsWorkaround = {0};

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        for (int i = 0; i < 2; ++i)
        {
            if (i)
            {
                dwSingleLineFlag = DT_SINGLELINE;
                pCurrentFlagSet = rgSingleLineFlags;
                cCurrentFlags = _countof(rgSingleLineFlags);
            }
            else
            {
                dwSingleLineFlag = 0;
                pCurrentFlagSet = rgMultiLineFlags;
                cCurrentFlags = _countof(rgMultiLineFlags);
            }

            for (int iFlag = 0; iFlag < cCurrentFlags; ++iFlag)
            {
                
                for (int iString = 0; iString < _countof(rgStrings); ++iString)
                {
                    UINT uFormat = dwSingleLineFlag | pCurrentFlagSet[iFlag];

                    // Methodology: for each flag, draw one half with the calc determined by CALCRECT
                    // and draw the other half with a much larger (2x) rect (either in vert or horz, 
                    // depending on the DT_SINGLELINE flag).

                    iCurrentCharExtra = (GenRand() % CHAREXTRA_RANGE) - (CHAREXTRA_RANGE)/2;
                    iOriginalCharExtra = SetTextCharacterExtra(hdc, iCurrentCharExtra);

                    // calculate the rect
                    rcCalcRect.left = rcLeft.left;
                    rcCalcRect.top = rcLeft.top;
                    rcCalcRect.bottom = (rcLeft.top + rcLeft.bottom) / 2;
                    rcCalcRect.right = (rcLeft.left + rcLeft.right) / 2;
                    CheckNotRESULT(0, DrawText(hdc, rgStrings[iString], -1, &rcCalcRect, uFormat | DT_CALCRECT));

                    // DrawText will not apply the extra between the first and second chars drawn even though
                    // CalcRect will. Add the right amount to take care of this.
                    if (iCurrentCharExtra < 0 && dwSingleLineFlag)
                    {
                        rcCalcRect.right += -iCurrentCharExtra;
                    }

                    SetRect(&rcLastPixelsWorkaround, rcCalcRect.right, rcCalcRect.top, rcCalcRect.right + 1, rcCalcRect.bottom);

                    // select the clip region for the near side of the screen
                    CheckNotRESULT(NULL, hrgnClip = CreateRectRgnIndirect(&rcLeft));
                    CheckNotRESULT(ERROR, SelectClipRgn(hdc, hrgnClip));
                    CheckNotRESULT(FALSE, DeleteObject(hrgnClip));

                    // draw to the left side
                    CheckNotRESULT(FALSE, PatBlt(hdc, rcLeft.left, rcLeft.top, rcLeft.right - rcLeft.left, rcLeft.bottom - rcLeft.top, WHITENESS));
                    CheckNotRESULT(0, DrawText(hdc, rgStrings[iString], -1, &rcCalcRect, uFormat));

                    // Copy to the right side
                    CheckNotRESULT(ERROR, SelectClipRgn(hdc, NULL));
                    CheckNotRESULT(FALSE, BitBlt(hdc, rcRight.left, rcRight.top, rcLeft.right - rcLeft.left, rcLeft.bottom - rcLeft.top, hdc, rcLeft.left, rcLeft.top, SRCCOPY));
                    CheckNotRESULT(FALSE, PatBlt(hdc, rcLeft.left, rcLeft.top, rcLeft.right - rcLeft.left, rcLeft.bottom - rcLeft.top, WHITENESS));

                    // select the clip region for the near side of the screen again
                    CheckNotRESULT(NULL, hrgnClip = CreateRectRgnIndirect(&rcLeft));
                    CheckNotRESULT(ERROR, SelectClipRgn(hdc, hrgnClip));
                    CheckNotRESULT(FALSE, DeleteObject(hrgnClip));

                    // adjust the default rect
                    if (dwSingleLineFlag)
                    {
                        rcCalcRect.right *= 2;
                    }
                    else
                    {
                        rcCalcRect.bottom *= 2;
                    }
                    // draw to the left side again
                    CheckNotRESULT(0, DrawText(hdc, rgStrings[iString], -1, &rcCalcRect, uFormat));

                    if (dwSingleLineFlag)
                    {
                        // CALCRECT does not account for blending of cleartext and is off by one. As a result,
                        // there will be a few pixels that are almost white hanging off the end that are clipped
                        // by the rect. Clear those pixels. 
                        CheckNotRESULT(FALSE, PatBlt(hdc, rcLastPixelsWorkaround.left, rcLastPixelsWorkaround.top, 1, rcLastPixelsWorkaround.bottom - rcLastPixelsWorkaround.top, WHITENESS));
                    }

                    CheckNotRESULT(ERROR, SelectClipRgn(hdc, NULL));

                    if (!CheckScreenHalves(hdc))
                    {
                        info(FAIL, _T("Failure Data: SingleLine (%d), Flags (%#x), CharExtra (%d), String (%s)"), 
                            dwSingleLineFlag,
                            uFormat,
                            iCurrentCharExtra,
                            rgStrings[iString]);
                    }
                    SetTextCharacterExtra(hdc, iOriginalCharExtra);
                }
            }
        }
    }

    myReleaseDC(hdc);
}

void
TextNonAntiAliasedTest(int testFunc, BOOL fSmoothingEnabled)
{
    info(ECHO, TEXT("%s - TextNonAntiAliasedTest"), funcName[testFunc]);
    TDC hdc = myGetDC();
    TCHAR tcTextString[MAXSTRINGLENGTH] = _T("Test String @.?");

    // warning 22019 indicates that zy might be < 0. We know it isn't.
    LOGFONT lfNonAntiAliased = {0};
    HFONT hf = NULL;
#pragma warning(suppress:22019)
    RECT rcCurrent = {0, 0, zx, zy};


    // Update the quality - Create a font with the same parameters as the current
    // test font, but use NONANTIALIASED_QUALITY.
    lfNonAntiAliased.lfQuality = NONANTIALIASED_QUALITY;
    CheckNotRESULT(NULL, hf = CreateFontCopy(hdc, NULL, CFC_QUALITY, &lfNonAntiAliased));

    // Select the new nonantialiased font into our DC
    CheckNotRESULT(NULL, hf = (HFONT)SelectObject(hdc, (HGDIOBJ)hf));
    // Now hf is holding the original test font's handle

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // Clear the screen to white
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

        // Draw some text to the screen
        if (EDrawTextW == testFunc)
        {
            CheckNotRESULT(0, DrawText(hdc, tcTextString, -1, &rcCurrent, 0));
        }
        else
        {
            CheckNotRESULT(FALSE, ExtTextOut(hdc, 0, 0, 0, NULL, tcTextString, _tcslen(tcTextString), NULL));
        }

        // Verify that every pixel is either black or white

        // Only print out 10 failing pixels at most, to keep output clean
        int iErrorCount = 10;
        for (int i = 0; i < zx && iErrorCount > 0; ++i)
        {
            for (int j = 0; j < zy && iErrorCount > 0; ++j)
            {
                BOOL fIsBlackOrWhite = FALSE;
                COLORREF cr = GetPixel(hdc, i, j);
                fIsBlackOrWhite = isBlackPixel(hdc, cr, i, j, TRUE) || isWhitePixel(hdc, cr, i, j, TRUE);
                if (!fIsBlackOrWhite)
                {
                    --iErrorCount;
                    info(FAIL, 
                        TEXT("Found pixel that wasn't black or white - nonantialiased text should not blend: %d, %d, %#x"),
                        i, j, cr);

                }
            }
        }
    }

    // hf currently is holding the original test font (with our nonantialiased font in the DC)
    CheckNotRESULT(NULL, hf = (HFONT)SelectObject(hdc, (HGDIOBJ)hf));

    // Cleanup the font
    CheckNotRESULT(FALSE, DeleteObject(hf));

    myReleaseDC(hdc);
}


// GetTextExtent point behavior changed (again).
// if extra width > individual character width, it's applied
// if extra width - individual width > 0, it's applied
// if extra width - individual width < 0, the extent is 0.
void
GetTextExtentPointWidthTest(int testFunc)
{
    info(ECHO, TEXT("%s - GetTextExtentPointWidthTest"), funcName[testFunc]);
    TDC hdc = myGetDC();
    TCHAR tcTextString[MAXSTRINGLENGTH] = TEXT("A a1 !Bb 2@Cc Ww Ii 3#Dd4 $Ee5%F f6^.");
    int nCharWidth = 0, nCharWidthPrev = 0;
    int nOldTextCharacterExtra = 0,
        nNewTextCharacterExtra;
    int index, nStringWidth = 0;
    SIZE sz;
    int nAppliedWidth;
    int nStringLength;

    // this test verifies SetTexthCharacterExtra, which isn't available on raster font systems
    if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
    {
        for(nStringLength = 0; nStringLength <= (int) _tcslen(tcTextString); nStringLength++)
        {
// XP behavior for negative width calculations is odd
#ifdef UNDER_CE
            nNewTextCharacterExtra = -20;
#else
            nNewTextCharacterExtra = 0;
#endif
            for(; nNewTextCharacterExtra <= 20; nNewTextCharacterExtra++)

            {
                nStringWidth = 0;
                nCharWidthPrev = 0;

                CheckNotRESULT(0x80000000, nOldTextCharacterExtra = SetTextCharacterExtra(hdc, nNewTextCharacterExtra));
                switch(testFunc)
                {
                    case EGetTextExtentExPoint:
                        CheckNotRESULT(FALSE, GetTextExtentExPoint(hdc, tcTextString, nStringLength, 0, NULL, NULL, &sz));
                        break;
                    case EGetTextExtentPoint32:
                        CheckNotRESULT(FALSE, GetTextExtentPoint32(hdc, tcTextString, nStringLength, &sz));
                        break;
                    case EGetTextExtentPoint:
                        CheckNotRESULT(FALSE, GetTextExtentPoint32(hdc, tcTextString, nStringLength, &sz));
                        break;
                }

                for(index = 0; index < nStringLength; index++)
                {
                    CheckNotRESULT(FALSE, GetCharWidth32(hdc, tcTextString[index], tcTextString[index], &nCharWidth));

                    if((nNewTextCharacterExtra >= 0) ||
                       (nNewTextCharacterExtra + nCharWidth >= 0))
                    {
                        // If the CharacterExtra is not negative or if
                        // the CharacterExtra plus the CharWidth is still positive,
                        // the resulting width is the sum of them.
                        nAppliedWidth = nCharWidth + nNewTextCharacterExtra;
                    }
                    else
                    {
                        // If the CharacterExtra plus the CharWidth is negative, keep at 0 instead
                        // of ignoring.
                        nAppliedWidth = 0;
                    }

                    //Add the current character to the width
                    nStringWidth += (nAppliedWidth);

                     // get the abc for the current character (next in the loop)
                    nCharWidthPrev = nCharWidth;
                }

                // verify the final width manually calculated with abc widths and character extras equals the width that GetTextExtentPoint calculates.
                if(nStringWidth != sz.cx)
                    info(FAIL, TEXT("ExpectedStringWidth %d, returned %d, for Extra width %d, string length %d"), nStringWidth, sz.cx, nNewTextCharacterExtra, nStringLength);

                CheckNotRESULT(0x80000000, SetTextCharacterExtra(hdc, nOldTextCharacterExtra));
            }
        }
    }
    myReleaseDC(hdc);
}


void
GetTextExtentExPointalpDxTest(int testFunc)
{
    info(ECHO, TEXT("%s - GetTextExtentExPointalpDxTest"), funcName[testFunc]);
    TDC hdc = myGetDC();
    TCHAR tcTextString[MAXSTRINGLENGTH];
    int aDx[MAXSTRINGLENGTH];
    SIZE sz = {0, 0};

    // Warning 22019 tells us that zy might be < 0 due to integer underflow
#pragma warning(suppress:22019)
    RECT rcBase = { 0, 0, zx/2, zy };
    // our right half rectangle to use.
    RECT rcComparisonRect = {zx/2, 0, zx, zy };
    int nCurrentStringLength = 0;
    int nOldBKMode;
    int nOldAlign =0;
    HRGN hrgnClip;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        if(!g_bRTL)
            _tcscpy_s(tcTextString, TEXT("A a1 !Bb 2@Cc Ww Ii 3#Dd4 $Ee5%F f6^."));
        else
        {
            CheckNotRESULT(GDI_ERROR, nOldAlign = SetTextAlign(hdc, TA_RIGHT));
            _tcscpy_s(tcTextString, TEXT("ABa1CcBbD2wCcdWwEIie3QDd4wMEe5tFSf6mn"));
        }

        // preinitilize the surface so the background color won't effect the drawing.
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));

        CheckNotRESULT(0, nOldBKMode = SetBkMode(hdc, TRANSPARENT));

        while(nCurrentStringLength < (int) _tcslen(tcTextString) && rcBase.top < zy && sz.cx < (zx/2 - 4))
        {

            CheckNotRESULT(FALSE, GetTextExtentExPoint(hdc, tcTextString, nCurrentStringLength, 0, NULL, aDx, &sz));

            // select the clip region for the left side of the screen, because of antialiasing we can bleed past the middle of the screen.
            CheckNotRESULT(NULL, hrgnClip = CreateRectRgnIndirect(&rcBase));
            CheckNotRESULT(ERROR, SelectClipRgn(hdc, hrgnClip));
            CheckNotRESULT(FALSE, DeleteObject(hrgnClip));

            for(int index = 0; index < nCurrentStringLength; index++)
            {
                // aDX[i] is the width for the i+1 character.
                int nAdjustment = index > 0?aDx[index - 1]:0;
                if(!g_bRTL)
                {
                    CheckNotRESULT(FALSE, ExtTextOut(hdc, rcBase.left + nAdjustment, rcBase.top, ETO_IGNORELANGUAGE, NULL, &(tcTextString[index]), 1, NULL));
                }
                else
                {
                    CheckNotRESULT(FALSE, ExtTextOut(hdc, rcBase.right - nAdjustment, rcBase.top, ETO_IGNORELANGUAGE, NULL, &(tcTextString[index]), 1, NULL));
                }
            }

            CheckNotRESULT(NULL, hrgnClip = CreateRectRgnIndirect(&rcComparisonRect));
            CheckNotRESULT(ERROR, SelectClipRgn(hdc, hrgnClip));
            CheckNotRESULT(FALSE, DeleteObject(hrgnClip));

            if(!g_bRTL)
            {
                CheckNotRESULT(FALSE, ExtTextOut(hdc, rcComparisonRect.left, rcComparisonRect.top, ETO_IGNORELANGUAGE, NULL, tcTextString, nCurrentStringLength, NULL));
            }
            else
            {
                CheckNotRESULT(FALSE, ExtTextOut(hdc, rcComparisonRect.right, rcComparisonRect.top, ETO_IGNORELANGUAGE, NULL, tcTextString, nCurrentStringLength, NULL));
            }

            CheckNotRESULT(ERROR, SelectClipRgn(hdc, NULL));

            rcBase.top += sz.cy;
            rcComparisonRect.top = rcBase.top;
            nCurrentStringLength++;
        }


        CheckScreenHalves(hdc);

        CheckNotRESULT(0, SetBkMode(hdc, nOldBKMode));
        if(g_bRTL)
            CheckNotRESULT(0, SetTextAlign(hdc, nOldAlign));
    }

    myReleaseDC(hdc);
}

void
GetTextExtentExPointSIZETest(int testFunc)
{
    info(ECHO, TEXT("%s - GetTextExtentExPointSIZETest"), funcName[testFunc]);
    TDC hdc = myGetDC();
    TCHAR tcTextString[MAXSTRINGLENGTH] = TEXT("A a1 !Bb 2@Cc Ww Ii 3#Dd4 $Ee5%F f6^.");
    int nStringLength;
    int nMaxExtent;
    int nFit;
    SIZE sz, szWholeString;

    CheckNotRESULT(FALSE, GetTextExtentExPoint(hdc, tcTextString, _tcslen(tcTextString), 0, 0, NULL, &szWholeString));

    // for a length of 0, CE and desktop differ.
    for(nStringLength = 1; nStringLength < (int) _tcslen(tcTextString); nStringLength++)
    {
        // test not applicable on raster fonts.
        if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
        {
            // TEST 1
            // if nMaxFit = sz.cx and the length increases
            CheckNotRESULT(FALSE, GetTextExtentExPoint(hdc, tcTextString, nStringLength, 0, 0, NULL, &sz));
            CheckNotRESULT(FALSE, GetTextExtentExPoint(hdc, tcTextString, _tcslen(tcTextString), nMaxExtent = sz.cx, &nFit, NULL, &sz));

            if(nFit != nStringLength)
                info(FAIL, TEXT("For the width needed, GetTextExtentPoint didn't return the same number of characters"));

            if(sz.cx != szWholeString.cx || sz.cy != szWholeString.cy)
                info(FAIL, TEXT("Expected the full rectangle width/height (%d, %d), but recieved something different (%d, %d)"), szWholeString.cx, szWholeString.cy, sz.cx, sz.cy);
        }

        // TEST 2
        // if the width available decreases
        CheckNotRESULT(FALSE, GetTextExtentExPoint(hdc, tcTextString, nStringLength, 0, 0, NULL, &sz));
        CheckNotRESULT(FALSE, GetTextExtentExPoint(hdc, tcTextString, _tcslen(tcTextString), nMaxExtent = sz.cx - 1, &nFit, NULL, &sz));

        if(nStringLength > 0 && nFit != nStringLength - 1)
            info(FAIL, TEXT("for 1 pixel less than needed for a given length, GetTextExtentExPoint returned the same number of chacters"));

        if(sz.cy != szWholeString.cy)
            info(FAIL, TEXT("The string height (%d) didn't match the original string height(%d), test 2."), sz.cy, szWholeString.cy);


        // TEST 3
        // if the width available decreases
        CheckNotRESULT(FALSE, GetTextExtentExPoint(hdc, tcTextString, nStringLength, 0, 0, NULL, &sz));
        CheckNotRESULT(FALSE, GetTextExtentExPoint(hdc, tcTextString, nStringLength, nMaxExtent = sz.cx, &nFit, NULL, &sz));

        if(nFit != nStringLength)
            info(FAIL, TEXT("For the same number of characters, the function didn't return the same number usable"));

        if(sz.cx != nMaxExtent)
            info(FAIL, TEXT("for the same number of characters, the width required changed between calls."));

        if(sz.cy != szWholeString.cy)
            info(FAIL, TEXT("The string height (%d) didn't match the original string height(%d), test 3."), sz.cy, szWholeString.cy);


        // TEST 4
        // if the width available decreases
        CheckNotRESULT(FALSE, GetTextExtentExPoint(hdc, tcTextString, nStringLength, 0, 0, NULL, &sz));
        CheckNotRESULT(FALSE, GetTextExtentExPoint(hdc, tcTextString, nStringLength, nMaxExtent = sz.cx + 1, &nFit, NULL, &sz));

        if(nFit != nStringLength)
            info(FAIL, TEXT("For more width available, the max number of characters changed."));

        if(sz.cx != nMaxExtent - 1)
            info(FAIL, TEXT("For more width available but the same number of characters, the width required changed."));

        if(sz.cy != szWholeString.cy)
            info(FAIL, TEXT("The string height (%d) didn't match the original string height(%d), test 4."), sz.cy, szWholeString.cy);

    }

    myReleaseDC(hdc);
}

void
SetTextCharacterExtraEscapementTest(int testFunc, BOOL bNegative = FALSE, BOOL bUseDX = FALSE)
{
    info(ECHO, TEXT("%s - SetTextCharacterExtraEscapementTest"), funcName[testFunc]);
    TDC hdc = myGetDC();
    TCHAR tcTextString[MAXSTRINGLENGTH] = TEXT("A a1 !Bb 2@Cc Ww Ii 3#Dd4 $Ee5%F f6^.");
    RECT rc = { zx/2, zy/2, zx, zy };
    int nOldExtra;
    LOGFONT lf;
    HFONT hfont, hfontStock;
    int *pnDxAdjustment = NULL, nDxAdjustment[MAXSTRINGLENGTH];

    int nDegreesToStep = 3600;
    int nDegreeStep = 20;
    int nCurrentAngle = 0;

    // so the start isn't _always_ at 0 escapement with 0 extra
    int nCharacterExtraStart = bNegative?(GenRand() % 10):-(GenRand() % 10);
    int nCharacterExtraEnd = bNegative?-50:50;
    FLOAT nCharacterExtraStep = (FLOAT) (bNegative?-1.:1.) * (((FLOAT) (abs(nCharacterExtraEnd) + abs(nCharacterExtraStart)))/ ((FLOAT) (nDegreesToStep / nDegreeStep)));
    FLOAT nCurrentExtra;
    BYTE    rgbFontQuality[] = { ANTIALIASED_QUALITY,
                                                 CLEARTYPE_QUALITY,
                                                 //CLEARTYPE_COMPAT_QUALITY,
                                                 NONANTIALIASED_QUALITY,
                                                 //DEFAULT_QUALITY,
                                                 //DRAFT_QUALITY
                                                };

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // test using the system font.
        memset(&lf, 0, sizeof(LOGFONT));
        hfontStock = (HFONT) GetStockObject(SYSTEM_FONT);
        GetObject(hfontStock, sizeof(LOGFONT), &lf);

        // SetTextCharacterExtra not implemented on raster fonts.
        if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
        {
            for(int nQuality = 0; nQuality < countof(rgbFontQuality); nQuality++)
            {
                lf.lfQuality = rgbFontQuality[nQuality];

                CheckNotRESULT(0x80000000, nOldExtra = GetTextCharacterExtra(hdc));

                for(nCurrentAngle = 0, nCurrentExtra = (FLOAT) nCharacterExtraStart;
                    nCurrentAngle < nDegreesToStep;
                    nCurrentAngle+=nDegreeStep, nCurrentExtra += nCharacterExtraStep)
                {
                    lf.lfEscapement = lf.lfOrientation = nCurrentAngle;
                    // create a font with some escapement

                    CheckNotRESULT(0x80000000, SetTextCharacterExtra(hdc, (int) nCurrentExtra));

                    // if used DX
                    if(bUseDX)
                    {
                        pnDxAdjustment = nDxAdjustment;
                        for(int i = 0; i < (int) _tcslen(tcTextString); i++)
                        {
                            nDxAdjustment[i] = (int) nCurrentExtra;
                        }
                    }

                    hfont = CreateFontIndirect(&lf);
                    SelectObject(hdc, hfont);

                    // draw the text
                    switch(testFunc)
                    {
                        case EExtTextOut:
                            CheckNotRESULT(FALSE, ExtTextOut(hdc, zx/2, zy/2, ETO_IGNORELANGUAGE, NULL, tcTextString, _tcslen(tcTextString), pnDxAdjustment));
                            break;
                        case EDrawTextW:
                            CheckNotRESULT(0, DrawText(hdc, tcTextString, -1, &rc, DT_NOCLIP));
                            break;
                    }

                    CheckNotRESULT(NULL, SelectObject(hdc, hfontStock));
                    CheckNotRESULT(FALSE, DeleteObject(hfont));
                }
            }
            CheckNotRESULT(0x80000000, SetTextCharacterExtra(hdc, nOldExtra));
        }
    }

    myReleaseDC(hdc);
}

void
TextCharExtraCleartypeTest(int testFunc)
{
    info(ECHO, TEXT("%s - TextCharExtraCleartypeTest"), funcName[testFunc]);

    TDC hdc = myGetDC();
    BOOL bOriginalSmoothing;
    int nOldCharacterExtra;
    TCHAR tcTextString[MAXSTRINGLENGTH] = TEXT("A a1 !Bb 2@Cc Ww Ii 3#Dd4 $Ee5%F f6^.");

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        // SetTextCharacterExtra not implemented on raster fonts.
        if(fontArray[aFont].dwType == TRUETYPE_FONTTYPE)
        {
            CheckNotRESULT(0x80000000, nOldCharacterExtra = GetTextCharacterExtra(hdc));
            CheckNotRESULT(FALSE, SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &bOriginalSmoothing, 0));
            CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, TRUE, 0, 0));
            CheckNotRESULT(0x80000000, SetTextCharacterExtra(hdc, -10));

            CheckNotRESULT(FALSE, ExtTextOut(hdc, 0, 0, ETO_IGNORELANGUAGE, NULL, tcTextString, _tcslen(tcTextString), NULL));

            CheckNotRESULT(0x80000000, SetTextCharacterExtra(hdc, nOldCharacterExtra));
            CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, bOriginalSmoothing, 0, 0));
        }
    }

    myReleaseDC(hdc);
}

void
ExtTextOutFillRectTest(int testFunc)
{
    info(ECHO, TEXT("%s - ExtTextOutFillRectTest"), funcName[testFunc]);
    TDC hdc = myGetDC();
    COLORREF crTestColors[] = { 0x00000000, 0x00FFFFFF, 0x00777777, randColorRef() };
    HBRUSH hbrSolid;
    RECT rcLeft = { 10, 10, zx/8, zx/8 };
    RECT rcRight = { 10 + zx/2, 10, zx/8 + zx/2, zx/8 };

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        for(int i = 0; i < countof(crTestColors); i++)
        {
            CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
            CheckNotRESULT(NULL, hbrSolid = CreateSolidBrush(crTestColors[i]));
            CheckNotRESULT(FALSE, FillRect(hdc, &rcRight, hbrSolid));

            CheckNotRESULT(CLR_INVALID, SetBkColor(hdc, crTestColors[i]));
            CheckNotRESULT(FALSE, ExtTextOut(hdc, 0, 0, ETO_OPAQUE | ETO_IGNORELANGUAGE, &rcLeft, NULL, 0, NULL));

            CheckNotRESULT(FALSE, DeleteObject(hbrSolid));

            // a brushes always use the bkcolor for realization.
            if(myGetBitDepth() != 1)
                CheckScreenHalves(hdc);
        }
    }

    myReleaseDC(hdc);
}

void
CleartypeSurfacesTest(int testFunc)
{
    info(ECHO, TEXT("%s - CleartypeSurfacesTest"), funcName[testFunc]);

    // only valid for >=8bpp surfaces.
    if(gBpp >= 8)
    {
        // turn off verification because the test driver doesn't support cleartype.
        BOOL OldVerifySetting = SetSurfVerify(FALSE);

        TCHAR tcTextString[MAXSTRINGLENGTH] = TEXT("Cleartype Test String.");
        TDC hdc = myGetDC();
        TDC hdcPrimary = NULL;
        TDC hdcPrimaryCompat = NULL;
        TBITMAP hbmpCompat = NULL;
        TBITMAP hbmpStock = NULL;
        RECT rcRight = { zx/2, 0, zx, zy };
        RECT rcLeft = { 0, 0, zx/2, zy/2 };
        LOGFONT lf;
        HFONT hfnt = NULL;
        HFONT hfntStock = NULL;

        if (!IsWritableHDC(hdc))
        {
            info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
        }
        else
        {
            memset(&lf, 0, sizeof(LOGFONT));

            lf.lfQuality = CLEARTYPE_QUALITY;

            CheckNotRESULT(NULL, hdcPrimary = myGetDC(NULL));
            CheckNotRESULT(NULL, hdcPrimaryCompat = CreateCompatibleDC(hdcPrimary));
            CheckNotRESULT(NULL, hbmpCompat = CreateCompatibleBitmap(hdcPrimary, zx, zy));
            CheckNotRESULT(NULL, hbmpStock = (TBITMAP) SelectObject(hdcPrimaryCompat, hbmpCompat));

            CheckNotRESULT(NULL, hfnt = CreateFontIndirect(&lf));
            // the stock font is the stock font, no need to save it twice
            CheckNotRESULT(NULL, hfntStock = (HFONT) SelectObject(hdc, hfnt));
            CheckNotRESULT(NULL, hfntStock = (HFONT) SelectObject(hdcPrimaryCompat, hfnt));

            // prep the primary compatible surface to match the surface under test
            CheckNotRESULT(FALSE, BitBlt(hdcPrimaryCompat, 0, 0, zx/2, zx/2, hdc, 0, 0, SRCCOPY));

            // first, we draw a text string onto the primary compat, that gives us our cleartyped baseline.
            switch(testFunc)
            {
                case EExtTextOut:
                    CheckNotRESULT(FALSE, ExtTextOut(hdcPrimaryCompat, 0, 0, NULL, NULL, tcTextString, _tcslen(tcTextString), NULL));
                    break;
                case EDrawTextW:
                    CheckNotRESULT(0, DrawText(hdcPrimaryCompat, tcTextString, -1, &rcLeft, DT_NOCLIP));
                    break;
            }

            // now blit what was drawn onto the primary compat back onto the surface under test. This gives us the baseline
            // for what cleartype on this surface should look like.
            CheckNotRESULT(FALSE, BitBlt(hdc, 0, 0, zx/2, zx/2, hdcPrimaryCompat, 0, 0, SRCCOPY));

            // now draw cleartype text on the surface under test and compare it.
            switch(testFunc)
            {
                case EExtTextOut:
                    CheckNotRESULT(FALSE, ExtTextOut(hdc, zx/2, 0, NULL, NULL, tcTextString, _tcslen(tcTextString), NULL));
                    break;
                case EDrawTextW:
                    CheckNotRESULT(0, DrawText(hdc, tcTextString, -1, &rcRight, DT_NOCLIP));
                    break;
            }

            // compare. NOTE: these surfaces aren't going to be identical, they're drawn using different code paths
            // the primary difference is shading, so the variation allowed is small.
            SetScreenHalvesRMSPercentage(60, 30);
            CheckScreenHalves(hdc);
            ResetScreenHalvesCompareConstraints();

            CheckNotRESULT(NULL, SelectObject(hdcPrimaryCompat, hfntStock));
            CheckNotRESULT(NULL, SelectObject(hdc, hfntStock));
            CheckNotRESULT(NULL, SelectObject(hdcPrimaryCompat, hbmpStock));
            CheckNotRESULT(NULL, DeleteDC(hdcPrimaryCompat));
            CheckNotRESULT(NULL, myReleaseDC(NULL, hdcPrimary));
            CheckNotRESULT(FALSE, DeleteObject(hbmpCompat));
            CheckNotRESULT(FALSE, DeleteObject(hfnt));
        }

        myReleaseDC(hdc);

        SetSurfVerify(OldVerifySetting);
    }
    else
        info(DETAIL, TEXT("Test is only valid with >8bpp surfaces, portion skipped."));

}

/***********************************************************************************
***
***   APIs
***
************************************************************************************/

//***********************************************************************************
TESTPROCAPI ExtTextOut_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;
    BOOL bOriginalSmoothing;
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &bOriginalSmoothing, 0));

    for(int i = 0; i < 2; i++)
    {
        CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, i, 0, 0));

        // Breadth
        passNull2Text(EExtTextOut);
        passNull2ExtTextOut(EExtTextOut);
        CreditsExtTestOut(EExtTextOut);
        SpeedExtTestOut(EExtTextOut);
        AlignExtTestOut(EExtTextOut);
        ColorFillingExtTextOut(EExtTextOut);
        // run the test with the override set prior to creation.
        FontSmoothingChangeTest(EExtTextOut, TRUE, FALSE);
        // run the test with the override set after creation and after the selection.
        FontSmoothingChangeTest(EExtTextOut, FALSE, FALSE);
        // run the test with the override set after creation and before selection.
        FontSmoothingChangeTest(EExtTextOut, FALSE, TRUE);
        // run the test positive and negative, without lpdx
        SetTextCharacterExtraEscapementTest(EExtTextOut, FALSE, FALSE);
        SetTextCharacterExtraEscapementTest(EExtTextOut, TRUE, FALSE);
        // run the test positive and negative with lpdx
        SetTextCharacterExtraEscapementTest(EExtTextOut, FALSE, TRUE);
        SetTextCharacterExtraEscapementTest(EExtTextOut, TRUE, TRUE);
        TextCharExtraCleartypeTest(EExtTextOut);
        ExtTextOutFillRectTest(EExtTextOut);
        // from draw.cpp
        WritableBitmapTest(EExtTextOut);
        CleartypeSurfacesTest(EExtTextOut);

        // Depth
        // This test will use NonAntiAliased when font smoothing is disabled and
        // it will use the ComparePixelsRMS algorithm when font smoothing is 
        // enabled (since manually drawn glyphs bleed together compared to ExtTextOut
        // drawn glyphs).
        TextDrawSpacingTest(EExtTextOut, TRUE, FALSE, i);
        TextDrawSpacingTest(EExtTextOut, TRUE, TRUE, i);

        TextNonAntiAliasedTest(EExtTextOut, i);
    }

    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, bOriginalSmoothing, 0, 0));
    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetTextAlign_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;
    BOOL bOriginalSmoothing;
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &bOriginalSmoothing, 0));

    for(int i = 0; i < 2; i++)
    {
        CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, i, 0, 0));

        // Breadth
        passNull2Text(EGetTextAlign);
        randGetSetTextAlign(EGetTextAlign);
        realGetSetTextAlign(EGetTextAlign);

        // Depth
        // None
    }
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, bOriginalSmoothing, 0, 0));
    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetTextExtentPoint32_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;
    BOOL bOriginalSmoothing;
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &bOriginalSmoothing, 0));

    for(int i = 0; i < 2; i++)
    {
        CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, i, 0, 0));

        // Breadth
        passNull2Text(EGetTextExtentPoint32);
        GetTextExtentPointImpactTest(EGetTextExtentPoint32);
        GetTextExtentPointTest(EGetTextExtentPoint32);

        // Depth
        GetTextExtentPointWidthTest(EGetTextExtentPoint32);
    }
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, bOriginalSmoothing, 0, 0));
    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetTextFace_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;
    BOOL bOriginalSmoothing;
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &bOriginalSmoothing, 0));

    for(int i = 0; i < 2; i++)
    {
        CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, i, 0, 0));

        // Breadth
        passNull2Text(EGetTextFace);
        GetTextFaceTest(EGetTextFace);
        SpecialParams(EGetTextFace);

        // Depth
        // None
    }
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, bOriginalSmoothing, 0, 0));
    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetTextMetrics_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;
    BOOL bOriginalSmoothing;
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &bOriginalSmoothing, 0));

    for(int i = 0; i < 2; i++)
    {
        CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, i, 0, 0));

        // Breadth
        passNull2Text(EGetTextMetrics);
        GetTextMetricsTest(EGetTextMetrics);
        GetTextMetricsDPITest(EGetTextMetrics);

        // Depth
        // None
    }
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, bOriginalSmoothing, 0, 0));
    return getCode();
}

//***********************************************************************************
TESTPROCAPI SetTextAlign_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;
    BOOL bOriginalSmoothing;
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &bOriginalSmoothing, 0));

    for(int i = 0; i < 2; i++)
    {
        CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, i, 0, 0));

        // Breadth
        passNull2Text(ESetTextAlign);
        randGetSetTextAlign(ESetTextAlign);
        realGetSetTextAlign(ESetTextAlign);

        // Depth
        // None
    }
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, bOriginalSmoothing, 0, 0));
    return getCode();
}

//***********************************************************************************
TESTPROCAPI DrawTextW_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;
    BOOL bOriginalSmoothing;
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &bOriginalSmoothing, 0));

    for(int i = 0; i < 2; i++)
    {
        CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, i, 0, 0));

        // Breadth
        passNull2Text(EDrawTextW);
        passNull2DrawText(EDrawTextW);
        DrawText_DT_XXXX(EDrawTextW);
        DrawTextCheckUnderline(EDrawTextW);
        DrawTextExpandTabs(EDrawTextW);
        DrawTextUnderline(EDrawTextW);
        DrawTextStringTest(EDrawTextW);
        DrawTextPrefixTest(EDrawTextW);
        DrawTextEllipsisTest(EDrawTextW);
        // run the test positive and negative
        SetTextCharacterExtraEscapementTest(EDrawTextW, FALSE);
        SetTextCharacterExtraEscapementTest(EDrawTextW, TRUE);
        // from draw.cpp
        WritableBitmapTest(EDrawTextW);
        CleartypeSurfacesTest(EDrawTextW);

        // Depth
        // Note that the spacing test will use a fuzzy comparison
        // when font smoothing is enabled.
        TextDrawSpacingTest(EDrawTextW, FALSE, FALSE, i);
        TextDrawSpacingTest(EDrawTextW, FALSE, TRUE, i);

        DrawTextCalcRect(EDrawTextW);

        DrawTextClipRegion();
        TextNonAntiAliasedTest(EDrawTextW, i);
    }

    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, bOriginalSmoothing, 0, 0));
    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetTextExtentPoint_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;
    BOOL bOriginalSmoothing;
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &bOriginalSmoothing, 0));

    for(int i = 0; i < 2; i++)
    {
        CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, i, 0, 0));

        // Breadth
        passNull2Text(EGetTextExtentPoint);
        GetTextExtentPointImpactTest(EGetTextExtentPoint);
        GetTextExtentPointTest(EGetTextExtentPoint);

        // Depth
        GetTextExtentPointWidthTest(EGetTextExtentPoint);
    }
   CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, bOriginalSmoothing, 0, 0));
   return getCode();
}

//***********************************************************************************
TESTPROCAPI GetTextExtentExPoint_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;
    BOOL bOriginalSmoothing;
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &bOriginalSmoothing, 0));

    for(int i = 0; i < 2; i++)
    {
        CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, i, 0, 0));

        // Breadth
        GetTextExtentPointImpactTest(EGetTextExtentExPoint);
        GetTextExtentPointTest(EGetTextExtentExPoint);
        // because this test is comparing manually drawn glyphs to glyphs drawn by
        // ExtTextOut, when antialiasing is enabled the antialiasing can bleed between glyphs, mismatching
        // the os function.
        if(i == 0)
            GetTextExtentExPointalpDxTest(EGetTextExtentExPoint);
        GetTextExtentExPointSIZETest(EGetTextExtentExPoint);

        // Depth
        GetTextExtentPointWidthTest(EGetTextExtentExPoint);
    }
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, bOriginalSmoothing, 0, 0));
    return getCode();
}

/***********************************************************************************
***
***   Manual Font Tests
***
************************************************************************************/


/***********************************************************************************
***
***   Support Functions/Structs
***
************************************************************************************/

static int AlignsFlag[] = {
    TA_BASELINE,
    TA_BOTTOM,
    TA_TOP,
    TA_CENTER,
    TA_LEFT,
    TA_RIGHT,
};

static TCHAR *AlignsStr[] = {
    TEXT("TA_BASELINE"),
    TEXT("TA_BOTTOM"),
    TEXT("TA_TOP"),
    TEXT("TA_CENTER"),
    TEXT("TA_LEFT"),
    TEXT("TA_RIGHT"),
};

static TCHAR oldStr[MAXLEN];
static int oldFlags,
        oldMode,
        oldAlign;

TCHAR  *AttribStr[3] = {
    TEXT("Italic"),
    TEXT("Underline"),
    TEXT("Strike"),
};

static BYTE attrib[7][3] = { {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 0}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1} };

static int myFlag = OPAQUE;
static TCHAR TestStr[80];
static int     m[2] = { OPAQUE, TRANSPARENT };
static RECT clipRect = { 0, 0, zx - 1, zy - 1 };

static int *charSpace,
       *oldCharSpace,
        charSpaceValues[256],
        oldCharSpaceValues[256];

void
setTestName(TCHAR * str)
{
    if(str)
    {
        _tcscpy_s(TestStr, str);
        info(ECHO, str);
    }
}

void
setSetBkMode(int newFlag)
{
    myFlag = newFlag;
}

void
setClipRect(int l, int t, int r, int b)
{
    clipRect.left = l;
    clipRect.top = t;
    clipRect.right = r;
    clipRect.bottom = b;
}

void
setNewCharSpace(int x, int n)
{
    charSpace = (n == 0) ? NULL : charSpaceValues;

    for (int i = 0; i < n; i++)
        charSpaceValues[i] = x;
}

void
setOldCharSpace(void)
{
    if (!charSpace)
        oldCharSpace = NULL;
    else
    {
        oldCharSpace = oldCharSpaceValues;
        for (int i = 0; i < 256; i++)
            oldCharSpaceValues[i] = charSpaceValues[i];
    }
}

//***********************************************************************************
void
initManualFonts(TDC hdc)
{
    pegGdiSetBatchLimit(1);
    CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
    CheckNotRESULT(0, SetBkMode(hdc, myFlag));
    CheckNotRESULT(NULL, SelectObject(hdc, GetStockObject(DKGRAY_BRUSH)));
    CheckNotRESULT(CLR_INVALID, SetBkColor(hdc, RGB(0, 0, 255)));
    CheckNotRESULT(CLR_INVALID, SetTextColor(hdc, RGB(255, 255, 0)));
    setNewCharSpace(0, 0);
}

//***********************************************************************************
void
BlastStr(TDC hdc, TCHAR * str, int line)
{
    RECT    aRect = { 0, 0, 300, 200 };

    if (gSanityChecks)
        info(DETAIL, TEXT("%s"), str);

    CheckNotRESULT(FALSE, ExtTextOut(hdc, 10, 10 + line * 20, ETO_CLIPPED | ETO_IGNORELANGUAGE, &aRect, str, _tcslen(str), NULL));
}

//***********************************************************************************
void
DebugInfo(TDC hdc)
{

    TCHAR   temp[256];
    LOGFONT fontInfo;
    HFONT   tempFont;
    int     i = 0;

    tempFont = (HFONT) SelectObject(hdc, (HFONT) GetStockObject(SYSTEM_FONT));
    CheckNotRESULT(0, GetObject(tempFont, sizeof (LOGFONT), &fontInfo));

    _tcsncpy_s(temp, TestStr, (countof(temp)- 1));
    temp[countof(temp)-1] = TEXT('\0');
    BlastStr(hdc, temp, i++);

    _sntprintf_s(temp, countof(temp)-1, TEXT("FName:<%s>"), fontInfo.lfFaceName);
    temp[countof(temp)-1] = TEXT('\0');
    BlastStr(hdc, temp, i++);

    _sntprintf_s(temp, countof(temp)-1, TEXT("H:%ld W:%ld Wei:%ld"), fontInfo.lfHeight, fontInfo.lfWidth, fontInfo.lfWeight);
    temp[countof(temp)-1] = TEXT('\0');
    BlastStr(hdc, temp, i++);

    _sntprintf_s(temp, countof(temp)-1, TEXT("Esc:%ld Ori:%ld"), fontInfo.lfEscapement, fontInfo.lfOrientation);
    temp[countof(temp)-1] = TEXT('\0');
    BlastStr(hdc, temp, i++);

    _sntprintf_s(temp, countof(temp)-1, TEXT("Ita:%d Und:%d Str:%d"), fontInfo.lfItalic, fontInfo.lfUnderline, fontInfo.lfStrikeOut);
    temp[countof(temp)-1] = TEXT('\0');
    BlastStr(hdc, temp, i++);

    _sntprintf_s(temp, countof(temp)-1, TEXT("BkM:%d Clip:%d %d %d %d"), myFlag, clipRect.left, clipRect.top, clipRect.right, clipRect.bottom);
    temp[countof(temp)-1] = TEXT('\0');
    BlastStr(hdc, temp, i++);

    _sntprintf_s(temp, countof(temp)-1, TEXT("ChSpace(%s): %d"), (charSpace) ? TEXT("ON") : TEXT("OFF"), (charSpace) ? charSpaceValues[0] : 0);
    temp[countof(temp)-1] = TEXT('\0');
    BlastStr(hdc, temp, i++);

    CheckNotRESULT(NULL, SelectObject(hdc, (HFONT) tempFont));
}

//***********************************************************************************
void
DrawHash(TDC hdc, HFONT hCurrFont, HFONT hOldFont)
{

    extern int DirtyFlag;
    COLORREF tempColor,
            tempBkColor;
    int     tempMode;
    int     tempAlign;

    tempAlign;
    tempColor = GetTextColor(hdc);
    tempBkColor = GetBkColor(hdc);
    tempMode = GetBkMode(hdc);
    tempAlign = GetTextAlign(hdc);

    CheckNotRESULT(CLR_INVALID, SetTextColor(hdc, RGB(0, 0, 0)));
    CheckNotRESULT(CLR_INVALID, SetBkColor(hdc, RGB(0, 0, 0)));
    CheckForRESULT(tempMode, SetBkMode(hdc, oldMode));
    CheckNotRESULT(GDI_ERROR, SetTextAlign(hdc, oldAlign));

    if (hOldFont)
        CheckNotRESULT(NULL, SelectObject(hdc, hOldFont));
    if (oldFlags != -1)
        CheckNotRESULT(FALSE, ExtTextOut(hdc, zx / 2, 3 * zy / 4, oldFlags | ETO_IGNORELANGUAGE, &clipRect, oldStr, _tcslen(oldStr), oldCharSpace));
    if (hCurrFont)
        CheckNotRESULT(NULL, SelectObject(hdc, hCurrFont));

    CheckNotRESULT(CLR_INVALID, SetTextColor(hdc, RGB(128, 128, 128)));
    CheckNotRESULT(CLR_INVALID, SetBkColor(hdc, RGB(0, 0, 0)));
    CheckForRESULT(oldMode, SetBkMode(hdc, TRANSPARENT));
    CheckNotRESULT(GDI_ERROR, SetTextAlign(hdc, TA_BASELINE | TA_LEFT));

    if (DirtyFlag)
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, BLACKNESS));
    else
        CheckNotRESULT(FALSE, PatBlt(hdc, 10, 10, 280, 190, BLACKNESS));

    for (int x = 0; x < zx; x += RESIZE(40))
    {
        CheckNotRESULT(FALSE, PatBlt(hdc, x, 0, 1, zy, PATCOPY));
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, x, zx, 1, PATCOPY));
    }

    CheckNotRESULT(FALSE, PatBlt(hdc, zx / 2, 0, 2, zy, WHITENESS));
    CheckNotRESULT(FALSE, PatBlt(hdc, 0, 3 * zy / 4, zx, 2, WHITENESS));

    DebugInfo(hdc);

    CheckNotRESULT(CLR_INVALID, SetTextColor(hdc, tempColor));
    CheckNotRESULT(CLR_INVALID, SetBkColor(hdc, tempBkColor));
    CheckForRESULT(TRANSPARENT, SetBkMode(hdc, tempMode));
    CheckNotRESULT(GDI_ERROR, SetTextAlign(hdc, tempAlign));
}

//***********************************************************************************
void
myExtTextOut(TDC hdc, HFONT hFont, HFONT oldFont, UINT Flags, TCHAR * str0, TCHAR * str1, TCHAR * str2)
{
    unsigned int nLengthAvail = MAXLEN - 1;
    TCHAR   str[MAXLEN];

    str[0] = NULL;

    // copy the entire string if there's room, copy nothing if there isn't.
    // nLengthAvail leaves 1 space for the NULL, and tcslen returns the length excluding the null, so
    // nLenghAvail is the number of non-null spaces available, hence the check for nLenghtAvail > tcslen instead of
    // nLenghtAvail >= tcslen.
    if (str0 && nLengthAvail > _tcslen(str0))
    {
        _tcsncat_s(str, str0, nLengthAvail);
        nLengthAvail -= _tcslen(str0);
    }
    if (str1 && nLengthAvail > _tcslen(str1))
    {
        _tcsncat_s(str, str1, nLengthAvail);
        nLengthAvail -= _tcslen(str1);
    }
    if (str2 && nLengthAvail > _tcslen(str2))
    {
        _tcsncat_s(str, str2, nLengthAvail);
        nLengthAvail -= _tcslen(str2);
    }

    DrawHash(hdc, hFont, oldFont);
    CheckNotRESULT(FALSE, ExtTextOut(hdc, zx / 2, 3 * zy / 4, Flags, &clipRect, str, _tcslen(str), charSpace));

    oldFlags = Flags;
    _tcscpy_s(oldStr, str);
    oldMode = GetBkMode(hdc);
    oldAlign = GetTextAlign(hdc);
    setOldCharSpace();

}

/***********************************************************************************
***
***   Alignment
***
************************************************************************************/
//***********************************************************************************
void
AlignTest(void)
{
    setTestName(TEXT("Alignment - Manual Test"));

    TDC     hdc = myGetDC();
    int     numA = sizeof (AlignsFlag) / sizeof (int);
    HFONT   hFont, hFontStock;
    TCHAR   str[256] = {NULL};

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        CheckNotRESULT(NULL, hFont = setupKnownFont(aFont, 45, 0, 0, 0, 0, 0));
        CheckNotRESULT(NULL, hFontStock = (HFONT) SelectObject(hdc, hFont));

        initManualFonts(hdc);

        for (int i = 0; i < 3; i++)
            for (int j = 3; j < numA; j++)
            {
                CheckNotRESULT(GDI_ERROR, SetTextAlign(hdc, AlignsFlag[i] | AlignsFlag[j]));
                myExtTextOut(hdc, hFont, hFont, ETO_CLIPPED | ETO_IGNORELANGUAGE, AlignsStr[i], TEXT(" | "), AlignsStr[j]);
                _sntprintf_s(str, countof(str) - 1, TEXT("Text with the attributes: %s %s"), AlignsStr[i], AlignsStr[j]);
                AskMessage(TestStr, TEXT("AlignTest"), str);
            }
        DrawHash(hdc, NULL, NULL);
        CheckNotRESULT(NULL, SelectObject(hdc, hFontStock));
        CheckNotRESULT(FALSE, DeleteObject(hFont));
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Weight
***
************************************************************************************/
//***********************************************************************************
void
WeightTest(void)
{
    setTestName(TEXT("Weight - Manual Test"));

    TDC     hdc = myGetDC();
    HFONT   hFont, hFontStock,
            oldFont = NULL;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        initManualFonts(hdc);
        for (int j = 0; j < 1000; j += 100)
        {
            CheckNotRESULT(NULL, hFont = setupFont(fontArray[aFont].tszFullName, 45, 0, 0, 0, j, 0, 0, 0));
            CheckNotRESULT(NULL, hFontStock = (HFONT) SelectObject(hdc, hFont));
            CheckNotRESULT(GDI_ERROR, SetTextAlign(hdc, TA_TOP | TA_CENTER));
            myExtTextOut(hdc, hFont, oldFont, ETO_CLIPPED | ETO_IGNORELANGUAGE, TEXT("Gaining Weight"), NULL, NULL);
            if(oldFont)
                CheckNotRESULT(FALSE, DeleteObject(oldFont));

            oldFont = hFont;
        }
        AskMessage(TestStr, TEXT("WeightTest"), TEXT("the font get fatter"));
        CheckNotRESULT(NULL, SelectObject(hdc, oldFont));
        DrawHash(hdc, NULL, NULL);
        CheckNotRESULT(NULL, SelectObject(hdc, GetStockObject(SYSTEM_FONT)));
        CheckNotRESULT(FALSE, DeleteObject(oldFont));
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Orientation & Escapement
***
************************************************************************************/
//***********************************************************************************
void
OriEscTest(BYTE typeFlag)
{
    setTestName(TEXT("Orientation - Manual Test"));

    TDC     hdc = myGetDC();
    int     i;
    HFONT   hFont,
            oldFont = NULL;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        initManualFonts(hdc);
        CheckNotRESULT(GDI_ERROR, SetTextAlign(hdc, TA_TOP | TA_CENTER));

        for (i = 120; i <= 3600; i += 120)
        {
            CheckNotRESULT(NULL, hFont = setupKnownFont(aFont, (long) ((float) (i + 80) / (float) 80), 0, 0, i % 3600, typeFlag, typeFlag));
            CheckNotRESULT(NULL, SelectObject(hdc, hFont));

            myExtTextOut(hdc, hFont, oldFont, ETO_CLIPPED | ETO_IGNORELANGUAGE, TEXT("Orientation"), NULL, NULL);
            if(oldFont)
                CheckNotRESULT(FALSE, DeleteObject(oldFont));
            oldFont = hFont;
        }
        AskMessage(TestStr, TEXT("OriEscTest"), TEXT("the Orientation stay the same"));
        CheckNotRESULT(NULL, SelectObject(hdc, GetStockObject(SYSTEM_FONT)));
        CheckNotRESULT(FALSE, DeleteObject(oldFont));
        DrawHash(hdc, NULL, NULL);

        oldFont = NULL;
        for (i = 120; i <= 3600; i += 120)
        {
            hFont = setupKnownFont(aFont, (long) ((float) (i + 80) / (float) 80), 0, i % 3600, 0, typeFlag, typeFlag);
            CheckNotRESULT(NULL, SelectObject(hdc, hFont));
            myExtTextOut(hdc, hFont, oldFont, ETO_CLIPPED | ETO_IGNORELANGUAGE, TEXT("Escapement"), NULL, NULL);
            if (oldFont)
                CheckNotRESULT(FALSE, DeleteObject(oldFont));
            oldFont = hFont;
        }
        AskMessage(TestStr, TEXT("OriEscTest"), TEXT("the Escapement change"));
        CheckNotRESULT(NULL, SelectObject(hdc, oldFont));
        DrawHash(hdc, NULL, NULL);
        CheckNotRESULT(NULL, SelectObject(hdc, GetStockObject(SYSTEM_FONT)));
        CheckNotRESULT(FALSE, DeleteObject(oldFont));
    }

    myReleaseDC(hdc);
}

//***********************************************************************************
void
TestFontAndText(void)
{
    setTestName(TEXT("TestFontAndText"));

    TDC     hdc = myGetDC();
    int     i;
    HFONT   hFont, hFontStock;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        initManualFonts(hdc);
        CheckNotRESULT(GDI_ERROR, SetTextAlign(hdc, TA_TOP | TA_CENTER));

        i = 2040;
        CheckNotRESULT(NULL, hFont = setupKnownFont(aFont, 20, 0, i % 3600, 0, 1, 1));
        CheckNotRESULT(NULL, hFontStock = (HFONT) SelectObject(hdc, hFont));

        myExtTextOut(hdc, hFont, NULL, ETO_CLIPPED | ETO_IGNORELANGUAGE, TEXT("Escapement"), NULL, NULL);

        CheckNotRESULT(NULL, SelectObject(hdc, hFontStock));
        CheckNotRESULT(FALSE, DeleteObject(hFont));
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Attributes
***
************************************************************************************/
//***********************************************************************************
void
AttribTest(void)
{
    setTestName(TEXT("Attrib - Manual Test"));

    TDC     hdc = myGetDC();
    int     i;
    HFONT   hFont,
            oldFont = NULL;
    TCHAR   str[256] = {NULL};

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        initManualFonts(hdc);

        for (i = 0; i < 7; i++)
        {
            CheckNotRESULT(NULL, hFont = setupFont(fontArray[aFont].tszFullName, 55, 0, 0, 0, 400, attrib[i][0], attrib[i][1], attrib[i][2]));
            CheckNotRESULT(NULL, SelectObject(hdc, hFont));
            CheckNotRESULT(GDI_ERROR, SetTextAlign(hdc, TA_TOP | TA_CENTER));

            myExtTextOut(hdc, hFont, oldFont, ETO_CLIPPED | ETO_IGNORELANGUAGE,
                         attrib[i][0] ? AttribStr[0] : NULL,
                         attrib[i][1] ? AttribStr[1] : NULL,
                         attrib[i][2] ? AttribStr[2] : NULL);
            if (oldFont)
                CheckNotRESULT(FALSE, DeleteObject(oldFont));
            oldFont = hFont;

            _sntprintf_s(str, countof(str) - 1, TEXT("Text with the attributes: Italic:%d Underline:%d Strike:%d"),
                      attrib[i][0], attrib[i][1], attrib[i][2]);
            AskMessage(TestStr, TEXT("AttribTest"), str);
        }
        CheckNotRESULT(NULL, SelectObject(hdc, oldFont));
        DrawHash(hdc, NULL, NULL);
        CheckNotRESULT(NULL, SelectObject(hdc, GetStockObject(SYSTEM_FONT)));
        CheckNotRESULT(FALSE, DeleteObject(oldFont));
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Scale
***
************************************************************************************/
//***********************************************************************************
void
ScaleTest(void)
{
    setTestName(TEXT("Scale - Manual Test"));

    TDC     hdc = myGetDC();
    int     i;
    HFONT   hFont,
            oldFont = NULL;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        initManualFonts(hdc);
        CheckNotRESULT(GDI_ERROR, SetTextAlign(hdc, TA_TOP | TA_CENTER));

        for (i = 0; i < 80; i += 15)
        {
            CheckNotRESULT(NULL, hFont = setupKnownFont(aFont, i, 0, 0, 0, 0, 0));
            CheckNotRESULT(NULL, SelectObject(hdc, hFont));
            myExtTextOut(hdc, hFont, oldFont, ETO_CLIPPED | ETO_IGNORELANGUAGE, TEXT("Scale All"), NULL, NULL);
            if (oldFont)
                CheckNotRESULT(FALSE, DeleteObject(oldFont));
            oldFont = hFont;
        }
        AskMessage(TestStr, TEXT("ScaleTest"), TEXT("the font scale up"));
        CheckNotRESULT(NULL, SelectObject(hdc, oldFont));
        DrawHash(hdc, NULL, NULL);
        CheckNotRESULT(NULL, SelectObject(hdc, GetStockObject(SYSTEM_FONT)));
        CheckNotRESULT(FALSE, DeleteObject(oldFont));

        oldFont = NULL;
        for (i = 0; i < 80; i += 15)
        {
            CheckNotRESULT(NULL, hFont = setupKnownFont(aFont, 0, i, 0, 0, 0, 0));
            CheckNotRESULT(NULL, SelectObject(hdc, hFont));
            myExtTextOut(hdc, hFont, oldFont, ETO_CLIPPED | ETO_IGNORELANGUAGE, TEXT("Scale Width"), NULL, NULL);
            if (oldFont)
                CheckNotRESULT(FALSE, DeleteObject(oldFont));
            oldFont = hFont;
        }
        AskMessage(TestStr, TEXT("ScaleTest"), TEXT("the font scale the width"));
        CheckNotRESULT(NULL, SelectObject(hdc, oldFont));
        DrawHash(hdc, NULL, NULL);
        CheckNotRESULT(NULL, SelectObject(hdc, GetStockObject(SYSTEM_FONT)));
        CheckNotRESULT(FALSE, DeleteObject(oldFont));

        oldFont = NULL;
        for (i = 0; i < 200; i += 30)
        {
            hFont = setupKnownFont(aFont, i, 30, 0, 0, 0, 0);
            CheckNotRESULT(NULL, SelectObject(hdc, hFont));
            myExtTextOut(hdc, hFont, oldFont, ETO_CLIPPED | ETO_IGNORELANGUAGE, TEXT("Scale Height"), NULL, NULL);
            if (oldFont)
                CheckNotRESULT(FALSE, DeleteObject(oldFont));
            oldFont = hFont;
        }
        AskMessage(TestStr, TEXT("ScaleTest"), TEXT("the font scale the height"));
        CheckNotRESULT(NULL, SelectObject(hdc, oldFont));
        DrawHash(hdc, NULL, NULL);
        CheckNotRESULT(NULL, SelectObject(hdc, GetStockObject(SYSTEM_FONT)));
        CheckNotRESULT(FALSE, DeleteObject(oldFont));
    }

    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   CharSpace
***
************************************************************************************/
//***********************************************************************************
void
CharSpaceTest(void)
{
    setTestName(TEXT("Character Space - Manual Test"));

    TDC     hdc = myGetDC();
    int     i;
    HFONT   hFont,
            oldFont = NULL;

    if (!IsWritableHDC(hdc))
        info(DETAIL, TEXT("Test invalid with write-protected HDC, portion skipped."));
    else
    {
        initManualFonts(hdc);

        for (i = 0; i < 40; i += 4)
        {
            CheckNotRESULT(NULL, hFont = setupKnownFont(aFont, 55, 0, 0, 0, 0, 0));
            CheckNotRESULT(NULL, SelectObject(hdc, hFont));
            CheckNotRESULT(GDI_ERROR, SetTextAlign(hdc, TA_TOP | TA_CENTER));
            setNewCharSpace(i, 50);
            myExtTextOut(hdc, hFont, oldFont, ETO_CLIPPED | ETO_IGNORELANGUAGE, TEXT("Character Space"), NULL, NULL);
            if (oldFont)
                CheckNotRESULT(FALSE, DeleteObject(oldFont));
            oldFont = hFont;
        }
        AskMessage(TestStr, TEXT("CharSpaceTest"), TEXT("the Intercharacter Spacing Increase"));
        CheckNotRESULT(NULL, SelectObject(hdc, oldFont));
        DrawHash(hdc, NULL, NULL);
        CheckNotRESULT(NULL, SelectObject(hdc, GetStockObject(SYSTEM_FONT)));
        CheckNotRESULT(FALSE, DeleteObject(oldFont));
    }

    myReleaseDC(hdc);
}

//***********************************************************************************
TESTPROCAPI ManualFont_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;
    BOOL bOriginalSmoothing;
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &bOriginalSmoothing, 0));

    for(int i = 0; i < 2; i++)
    {
        CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, i, 0, 0));

        setClipRect(0, 0, zx - 1, zy - 1);
        setSetBkMode(m[0]);
        TestFontAndText();

        for (int j = 0; j < 2; j++)
        {
            setSetBkMode(m[j]);
            AlignTest();
            OriEscTest(0);
            OriEscTest(1);
            AttribTest();
            WeightTest();
            ScaleTest();
            CharSpaceTest();
        }
    }
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, bOriginalSmoothing, 0, 0));
    return getCode();
}

//***********************************************************************************
TESTPROCAPI ManualFontClipped_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;
    BOOL bOriginalSmoothing;
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &bOriginalSmoothing, 0));

    for(int i = 0; i < 2; i++)
    {
        CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, i, 0, 0));

        setClipRect(zx / 2 + RESIZE(100), 3 * zy / 4 + RESIZE(60), zx / 2 - RESIZE(100), 3 * zy / 4 - RESIZE(60));

        for (int j = 0; j < 2; j++)
        {
            setSetBkMode(m[j]);
            AlignTest();
            OriEscTest(0);
            OriEscTest(1);
            AttribTest();
            WeightTest();
            ScaleTest();
            CharSpaceTest();
        }
    }
    CheckNotRESULT(FALSE, SystemParametersInfo(SPI_SETFONTSMOOTHING, bOriginalSmoothing, 0, 0));
    return getCode();
}
