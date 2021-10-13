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

   text.cpp

Abstract:

   Gdi Tests: Text APIs

***************************************************************************/
#include "global.h"
#include "fontdata.h"

#define MAXLEN 128
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
    int     maxerrors = 0;

    while (counter++ < 10)
    {
        // create a random mode
        alignmode = -(rand() % 10000);
        alignmode &= (TA_BASELINE | TA_BOTTOM | TA_TOP | TA_CENTER | TA_LEFT | TA_RIGHT | TA_NOUPDATECP | TA_UPDATECP);
        // try to set and then get this random mode
        mySetLastError();
        getResult0 = GetTextAlign(hdc);
        myGetLastError(NADA);

        mySetLastError();
        setResult0 = SetTextAlign(hdc, alignmode);
        myGetLastError(NADA);

        mySetLastError();
        setResult1 = SetTextAlign(hdc, alignmode);
        myGetLastError(NADA);

        mySetLastError();
        getResult1 = GetTextAlign(hdc);
        myGetLastError(NADA);

        SetTextAlign(hdc, getResult0);
        // The modes created were valid so this would be an error
        if (maxerrors < 10)
        {

            if (getResult0 != setResult0)
                info(FAIL, TEXT("Get/Set Text Align != before mode change. getResult:%d  setResult:%d"), getResult0,
                     setResult0);

            if (getResult1 != alignmode)
            {
                info(FAIL, TEXT("* GetTextAlign did not return correct mode:%d  return:%d"), mode, getResult1);
                maxerrors++;
            }

            if (setResult1 != alignmode)
            {
                info(FAIL, TEXT("* SetTextAlign did not return correct mode:%d  return:%d"), mode, setResult1);
                maxerrors++;
            }

            if (getResult1 != setResult1)
                info(FAIL, TEXT("Get/Set TextAlign != after mode change. getResult:%d  setResult:%d"), getResult1, setResult1);

        }
    }
    if (maxerrors >= 10)
        info(ECHO, TEXT("More than 10 errors found. Part of test skipped."));
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
        mySetLastError();
        getResult0 = GetTextAlign(hdc);
        myGetLastError(NADA);

        mySetLastError();
        setResult0 = SetTextAlign(hdc, mode[i]);
        myGetLastError(NADA);

        mySetLastError();
        setResult1 = SetTextAlign(hdc, mode[i]);
        myGetLastError(NADA);

        mySetLastError();
        getResult1 = GetTextAlign(hdc);
        myGetLastError(NADA);

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
    HFONT   hFont;
    TCHAR   outFaceName[256];
    int     wResult,
            gResult;
    UINT    uLCID = GetSystemDefaultLCID();

    for (int i = 0; i < numFonts; i++)
    {
        if (fontArray[i].hardcoded && fontArray[i].type != EBad)
        {
            memset(outFaceName, 0, sizeof(outFaceName));
            hFont = setupFontMetrics(1, i, hdc, 20, 0, 0, 0, 400, 0, 0, 0);
            // get the face
            mySetLastError();
            gResult = GetTextFace(hdc, countof(outFaceName), (LPTSTR)outFaceName);
            myGetLastError(NADA);
            if (!gResult)
                info(FAIL, TEXT("GetTextFace failed on %s #:%d GLE():%d"), fontArray[i].fileName, i, GetLastError());
            // is the output == input?
            wResult = _tcscmp(fontArray[i].faceName, outFaceName);
            if (hFont && wResult != 0)
            {
                // only check USA version
                if (uLCID == 0x409)
                    info(FAIL, TEXT("wcscmp failed on %s != %s  #:%d"), fontArray[i].faceName, outFaceName, i);
                else
                    info(ECHO, TEXT("NON USA version: wcscmp failed on %s != %s  #:%d"), fontArray[i].faceName, outFaceName, i);
            }

            if (hFont)
                cleanupFont(1, i, hdc, hFont);
        }
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
    HFONT   hFont;
    TEXTMETRIC outTM;
    int     gResult,
            temp[20];
    

    for (int i = 0; i < fontCount; i++)
    {
        if (fontList[i] != -1 && fontArray[fontList[i]].type != EBad)
        {
            hFont = setupFontMetrics(1, fontList[i], hdc, 20, 0, 0, 0, 400, 0, 0, 0);
            // get the face
            mySetLastError();
            gResult = GetTextMetrics(hdc, &outTM);
            myGetLastError(NADA);
            if (!gResult)
                info(FAIL, TEXT("GetTextMetrics failed on %s #:%d err=%d"), fontArray[i].fileName, i, GetLastError());

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
            temp[9] = outTM.tmDigitizedAspectX;
            temp[10] = outTM.tmDigitizedAspectY;
            temp[11] = outTM.tmFirstChar;
            temp[12] = outTM.tmLastChar;
            temp[13] = outTM.tmDefaultChar;
            temp[14] = outTM.tmBreakChar;
            temp[15] = outTM.tmItalic;
            temp[16] = outTM.tmUnderlined;
            temp[17] = outTM.tmStruckOut;
            temp[18] = outTM.tmPitchAndFamily;
            temp[19] = outTM.tmCharSet;

            if (!hFont)
                continue;

            // cleck against NT hard coded array
            for (int j = 0; j < 20; j++)
            {
#ifdef UNDER_CE                 // CE no promise to match NTs values
                TCHAR   szFaceName[64];
                if (temp[j] != NTFontMetrics[i][j] 
                    && (j != 1 && j != 2 && j != 3 && j != 5 && j != 6 && j != 11 && j != 12)
                    && !(g_fPrinterDC && (9 == j || 10 == j)))
                    info(FAIL, TEXT("* GetTextMetrics font:%d param:%d  NT:%d != OS:%d"), i, j, NTFontMetrics[i][j], temp[j]);

                GetTextFace(hdc, 60, szFaceName);
                info(ECHO, TEXT("font:%d: Font want = %s:  Get %s"), i, (LPTSTR) fontArray[i].faceName, (LPTSTR) szFaceName);

#else
                if (temp[j] != NTFontMetrics[i][j])
                    info(FAIL, TEXT("* GetTextMetrics font:%d param:%d  NT:%d != OS:%d"), i, j, NTFontMetrics[i][j], temp[j]);
#endif
            }
            cleanupFont(1, fontList[i], hdc, hFont);
        }
    }
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   Tries different strings and lengths for extent
***
************************************************************************************/

//***********************************************************************************
void
GetTextExtentPoint32Test(int testFunc)
{
    info(ECHO, TEXT("%s - checking extents against NT Hardcoded"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HFONT   hFont;
    SIZE    size;
    BOOL    result;
    TCHAR   myStr[2];
    TCHAR   aCh;
    int     maxerrors = 0;
    int     fontList[] = {tahomaFont, courierFont, /*symbolFont,*/ timesFont, wingdingFont, verdanaFont, arialRasterFont};
    
    myStr[1] = NULL;
    for (int f = 0; f < countof(fontList); f++)
    {
        if (fontList[f] != -1 && fontArray[fontList[f]].type != EBad)
        {
            // for all of the tests and chars
            hFont = setupFontMetrics(1, fontList[f], hdc, 16, 0, 0, 0, 400, 0, 0, 0);
            for (int j = 0; j <= 'z' - '!'; j++)
            {
                aCh = (TCHAR)(j + TEXT('!'));
                myStr[0] = aCh;

                mySetLastError();
                result = GetTextExtentPoint32(hdc, myStr, 1, &size);
                myGetLastError(NADA);
                
                if (maxerrors < 16 && (size.cy != 16 || size.cx != NTExtentResults[f][j]))
                {
                    info(FAIL, TEXT("* font #%d:<%s> char:<%x> x:(%d != NT:%d) y:(%d != NT:16)"), f, fontArray[fontList[f]].fileName,
                         aCh, size.cx, NTExtentResults[f][j], size.cy);
                    maxerrors++;
                }

            }
            if (hFont)
                cleanupFont(1, fontList[f], hdc, hFont);
        }
    }
    if (maxerrors >= 10)
        info(ECHO, TEXT("More than 10 errors found. Part of test skipped."));
    myReleaseDC(hdc);
}

/***********************************************************************************
***
***   GetTextExtentPoint32ImpactTest
***
************************************************************************************/

//***********************************************************************************
void
GetTextExtentPoint32ImpactTest(int testFunc)
{
    // this done with Symbol font now 
    info(ECHO, TEXT("%s - checking extents against NT Hardcoded"), funcName[testFunc]);

    TDC     hdc = myGetDC();
    HFONT   hFont;
    SIZE    size;
    int     strLen;
    BOOL    result,
            expect;
    TCHAR   myStr[500];

    TCHAR   myChar = 'Z';
    int     fontList[] = {tahomaFont, courierFont, symbolFont, timesFont, wingdingFont, verdanaFont, arialRasterFont};

    myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);
            
    for (int l = 0; l < 100; l++)
        myStr[l] = myChar;
    myStr[l] = NULL;

    strLen = _tcslen(myStr);

    for(int f=0; f<countof(fontList); f++)
    {
        if (fontList[f] != -1 && fontArray[fontList[f]].type != EBad)
        {
            expect = NTExtentZResults[f];
            
            // for all of the tests and chars
            hFont = setupFontMetrics(1, fontList[f], hdc, 16, 0, 0, 0, 400, 0, 0, 0);

            mySetLastError();
            result = GetTextExtentPoint32(hdc, myStr, strLen, &size);
            myGetLastError(NADA);

            if (hFont && (size.cx != expect))
                info(FAIL, TEXT("#:%d of char:<%c> (%d != NT:%d)"), 100, myChar, size.cx, expect);

            if (hFont)
                cleanupFont(1, fontList[f], hdc, hFont);
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
    HFONT   hFont;
    HBRUSH  hBrush = CreateSolidBrush(RGB(255, 0, 0)),
            stockBr = (HBRUSH) SelectObject(hdc, hBrush);
    HRGN    hRgn = CreateRectRgnIndirect(&r);
    int     numCalls = ((500 * 640) / zx);

    if (!hBrush || !stockBr || !hRgn)
    {
        info(FAIL, TEXT("Important resource failed: Brush:0%x0 stockBr:0%x0 hRgn::0%x0"), hBrush, stockBr, hRgn);
    }

    // set up
    FillRgn(hdc, hRgn, hBrush);
    _tcscpy(word, TEXT("m"));   //Microsoft Logo
    len = _tcslen(word);
    SetTextAlign(hdc, TA_CENTER);
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    SetBkColor(hdc, RGB(255, 0, 0));
    hFont = setupFont(1, MSLOGO, hdc, 52, 0, 0, 0, 700, 1, 0, 0);

    start = GetTickCount();

    for (int i = 0; i < numCalls; i++)
        ExtTextOut(hdc, r.right / 2, r.bottom / 2 - 26, ETO_OPAQUE, &r, word, len, NULL);

    end = GetTickCount();
    totalTime = (float) ((float) end - (float) start) / 1000;
    percent = (float) 100 *(((float) i / totalTime) / performNT);

    if (hFont)
        cleanupFont(1, MSLOGO, hdc, hFont);

    if (percent < (float) 80)
        info(ECHO, TEXT("* %.2f percent of NT Performance: %d calls took secs:%.2f, average calls/sec:%.2f"), percent, i,
             totalTime, (float) i / totalTime);

    SelectObject(hdc, stockBr);
    myReleaseDC(hdc);
    DeleteObject(hBrush);
    DeleteObject(hRgn);
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
    HFONT   hFont,
            hFontLogo;
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
    hBitmap = CreateCompatibleBitmap(hdc, offRect.right, offRect.bottom);
    compDC = CreateCompatibleDC(hdc);
    hBitmapStock = (TBITMAP) SelectObject(compDC, hBitmap);

    // set up GDI modes and states
    myPatBlt(hdc, 0, 0, zx, zy, BLACKNESS);
    SetBkMode(compDC, OPAQUE);
    SetBkColor(compDC, RGB(0, 0, 0));
    SetTextAlign(compDC, TA_CENTER | TA_TOP);

    hFont = setupFont(1, aFont, compDC, 45, 0, 0, 0, 400, 0, 0, 0);
    hFontLogo = setupFont(1, MSLOGO, compDC, 45, 0, 0, 0, 700, 1, 0, 0);

    while (nextPerson != numPeople - 1)
    {
        for (i = 0; i <= Divisions; i++)
        {
            // if the entry is for the mslogo, and we have the mslogo font
            if(!_tcscmp(people[peopleIndex[i]], TEXT("Microsoft")) && MSLOGO)
            {
                // use the logo, and set the i in Microsoft to NULL, so it's not printed
                SelectObject(compDC, (HFONT) ((people[peopleIndex[i]][0] == 'M') ? hFontLogo : hFont));
                len = 1;
            }
            // not the microsoft special case, use the standard font.
            else 
            {
                SelectObject(compDC, (HFONT) hFont);
                len = _tcslen(people[peopleIndex[i]]);
            }
            cent = (people[peopleIndex[i]][0] == ' ' ||
                    people[peopleIndex[i]][0] == 'M') ? (zx - offRect.right) / 2 : RectCycles[i].left + offset[i][1];
            SetTextColor(compDC, pColor[i]);
            if (!ExtTextOut(compDC, offRect.right / 2, 0, ETO_OPAQUE, &offRect, people[peopleIndex[i]], len, NULL))
                info(FAIL, TEXT("ExtTextOut failed on output at: (%d %d)"), cent, RectCycles[i].top);
            BitBlt(hdc, cent, RectCycles[i].top, offRect.right, offRect.bottom, compDC, 0, 0, SRCCOPY);

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

    if (hFont)
        cleanupFont(1, aFont, compDC, hFont);
    if (hFontLogo)
        cleanupFont(1, MSLOGO, compDC, hFontLogo);

    SelectObject(compDC, hBitmapStock);
    DeleteObject(hBitmap);
    DeleteDC(compDC);
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

    int     result = 1;
    SIZE    size;

    mySetLastError();
    switch (testFunc)
    {
        case EDrawTextW:
            result = DrawText((TDC) NULL, NULL, NULL, NULL, NULL);
            break;
        case EExtTextOut:
            result = ExtTextOut((TDC) NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            passCheck(result, 0, TEXT("NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL"));
            break;
        case EGetTextExtentPoint32:
            result = GetTextExtentPoint32((TDC) NULL, NULL, NULL, &size);
            break;
        case EGetTextExtentPoint:
            result = GetTextExtentPoint((TDC) NULL, NULL, NULL, &size);
            break;
        case EGetTextFace:
            mySetLastError();
            result = GetTextFace((TDC) NULL, NULL, NULL);
            passCheck(result, 0, TEXT("NULL,NULL,NULL"));
            myGetLastError(NADA);

            mySetLastError();
            result = GetTextFace((TDC) NULL, 10, NULL);
            passCheck(result, 0, TEXT("NULL,10,NULL"));
            myGetLastError(NADA);

            mySetLastError();
            result = GetTextFace((TDC) NULL, NULL, TEXT("Hello"));
            passCheck(result, 0, TEXT("NULL,NULL,Hello"));
            myGetLastError(ERROR_INVALID_PARAMETER);

            mySetLastError();
            break;
        case EGetTextMetrics:
            result = GetTextMetrics((TDC) NULL, NULL);
            passCheck(result, 0, TEXT("NULL,NULL"));
            break;
        case EGetTextAlign:
            result = GetTextAlign((TDC) NULL);
            passCheck(result, GDI_ERROR, TEXT("NULL"));
            break;
        case ESetTextAlign:
            result = SetTextAlign((TDC) NULL, 777);
            passCheck(result, -1, TEXT("NULL,777"));
            break;
    }

    switch (testFunc)
    {
        case EExtTextOut:
        case ESetTextAlign:
            myGetLastError(ERROR_INVALID_HANDLE);
            break;
        case EDrawTextW:
#ifdef UNDER_NT
            myGetLastError(NADA);
#else // UNDER_CE
            myGetLastError(ERROR_INVALID_HANDLE);
#endif
            break;
        default:
            myGetLastError(NADA);
            break;
    }
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
    int     result = 1;
    RECT    rc = { 0, 0, 10, 10 };

    mySetLastError();
    result = ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rc, NULL, 3000, NULL);
    myGetLastError(NADA);
    passCheck(result, 0, TEXT("hdc, 0, 0, ETO_OPAQUE, &rc, NULL, 3000, NULL"));

    // crashed client ITV588
    info(ECHO, TEXT("%s - Part 2 *"), funcName[testFunc]);
    rc.left = rc.top = 0;
    rc.right = rc.bottom = 100;
    SetTextAlign(hdc, TA_RIGHT);
    SetBkMode(hdc, OPAQUE);
    result = ExtTextOut(hdc, 10, 10, 0, NULL, NULL, 0, (INT *) & rc);
    passCheck(result, 1, TEXT("hdc, 10, 10, 0, NULL, NULL, 0, (INT *)&rc"));

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
    int     result;
    RECT    rc = { 0, 0, 10, 10 }, rc2 =
    {
    0, 0, 0, 0};

    result = DrawText(hdc, TEXT("Hello World"), -1, &rc, 1);
    passCheck(result != 0, 1, TEXT("hdc,Hello World,-1,&rc,1"));

    result = DrawText(hdc, TEXT("Hello World"), 30, &rc2, 99999999);
    passCheck(result != 0, 1, TEXT("hdc,Hello World,30,&rc2,99999999"));

    result = DrawText(hdc, TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ"), 26, &rc, DT_CALCRECT);
    passCheck(result != 0, 1, TEXT("hdc,ABCDEFGHIJKLMNOPQRSTUVWXYZ, 26, &rc, DT_CALCRECT"));
    if (rc.top != 0 || rc.bottom <= 0 || rc.right <= 10)
        info(FAIL, TEXT("rc rect (%d %d %d %d)didn't get adjusted when DT_CALCRECT passed"), rc.left, rc.top,
             rc.right, rc.bottom);

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
    int     result0,
            result1;
    RECT    rc = { 0, 0, zx + 1, zy + 1 };
    COLORREF c[2] = { RGB(0, 0, 0), RGB(255, 255, 255) };
    int     expect[2] = { zx * (zy - 1), 0 };
    DWORD   op[2] = { WHITENESS, BLACKNESS };

    for (int i = 0; i < 2; i++)
    {
        PatBlt(hdc, 0, 0, zx, zy, op[i]);
        SetBkColor(hdc, c[i]);
        SetTextColor(hdc, c[i]);
        result0 = ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rc, TEXT("Should not see this"), 19, NULL);
        result1 = CheckAllWhite(hdc, 1, 1, 1, expect[i]);
        passCheck(result0, 1, TEXT("hdc, 0, 0, ETO_OPAQUE, &rc,..."));
        if (result1 != expect[i])
            info(FAIL, TEXT("pass:%d colored pixels expected:%d != result:%d"), i, expect[i], result1);
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

    info(ECHO, TEXT("%s - using <%s>"), funcName[testFunc], fontArray[aFont].faceName);

    TDC     hdc = myGetDC();
    HFONT   hFont = setupFont(1, aFont, hdc, 52, 0, 0, 0, 400, 0, 0, 0);
    int     len = wcslen(fontArray[aFont].faceName) + 1;
    int     result;

    result = GetTextFace(hdc, 128, NULL);
    if (hFont)
    {
        passCheck(result, len, TEXT("hdc, 128, NULL) "));
        cleanupFont(1, aFont, hdc, hFont);
    }
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
    HFONT   hFont = setupFontMetrics(1, aFont, hdc, 12, 0, 0, 0, 0, 0, 1, 0);
    RECT    r = { 0, 0, zx, zy };
    int     result,
            expected = 120;
    DWORD   dw;

    // clear the screen for CheckAllWhite
    PatBlt(hdc, 0, 0, zx, zy, WHITENESS);
    SetTextColor(hdc, RGB(0, 0, 0));
    SetBkColor(hdc, RGB(255, 255, 255));

    DrawText(hdc, TEXT("                        "), -1, &r, DT_SINGLELINE);
    result = CheckAllWhite(hdc, 1, 1, 1, expected);

    if (!hFont)                 // font is missing: don't need check
        goto LReleaseDC;

    if (result < expected - 60 || result > expected + 60)
    {
        dw = GetLastError();
        if (dw == ERROR_NOT_ENOUGH_MEMORY || dw == ERROR_OUTOFMEMORY)
            info(DETAIL, TEXT("Found %d Colored Pixels: Fail due to Out Of Memory"), result);
        else
            info(FAIL, TEXT("Found %d Colored Pixels NT draw:%d: err=%ld"), result, expected, dw);
    }

    cleanupFont(1, aFont, hdc, hFont);

  LReleaseDC:
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

    SelectObject(hdc, (HFONT) GetStockObject(SYSTEM_FONT));
    SetTextColor(hdc, RGB(0, 0, 0));
    SetBkColor(hdc, RGB(255, 255, 255));

    DrawText(hdc, TEXT("\t\t\t\t\t\t\t\t\t"), -1, &r, DT_EXPANDTABS);
    result = CheckAllWhite(hdc, 1, 1, 1, 0);
    if (result < expected)
        info(FAIL, TEXT("Found %d Colored Pixels NT draw:%d"), result, expected);

    myReleaseDC(hdc);
}

void
DrawText_DT_XXXX(int testFunc)
{

    info(ECHO, TEXT("%s - All DT_XXXX options"), funcName[testFunc]);

    TDC     hdc = myGetDC();

    RECT    r = { 0, 0, zx, zy };
    int     flag[] = {
        DT_BOTTOM,
        DT_CALCRECT,
        DT_CENTER,
        DT_EXPANDTABS,
        DT_LEFT,
        DT_NOCLIP,
        DT_NOPREFIX,
        DT_RIGHT,
        DT_SINGLELINE,
        DT_TABSTOP,
        DT_TOP,
        DT_VCENTER,
        DT_WORDBREAK,
    };
    int     flagSize = sizeof (flag) / sizeof (int);

    for (int i = 0; i < flagSize; i++)
        DrawText(hdc, TEXT("This is a DT_Option test."), -1, &r, flag[i]);

    myReleaseDC(hdc);
}

void
DrawTextUnderline(int testFunc)
{

    info(ECHO, TEXT("Check Underline Color of DrawText"));

    LOGFONT logfont;
    HFONT   hFont,
            hFontTemp;
    RECT    rect = { 0, 0, zx, zy };
    int     c = 0;

    TDC     hdc = myGetDC();

    SelectObject(hdc, GetStockObject(BLACK_PEN));
    SelectObject(hdc, GetStockObject(BLACK_BRUSH));
    memset(&logfont, 0, sizeof (LOGFONT));
    logfont.lfHeight = 18;
    logfont.lfUnderline = 1;
    hFont = CreateFontIndirect(&logfont);
    hFontTemp = (HFONT) SelectObject(hdc, hFont);

    for (int y = 0; y < zy; c += 32, y += 30)
    {
        rect.top = y;
        rect.bottom = y + 30;
        SetTextColor(hdc, RGB(c % 256, c % 256, c % 256));
        DrawText(hdc, TEXT("Check for color change in underline"), -1, (LPRECT) & rect, 0);
    }
    Sleep(10);

    hFontTemp = (HFONT) SelectObject(hdc, hFontTemp);
    passCheck(hFontTemp == hFont, TRUE, TEXT("Incorrect font returned"));

    DeleteObject(hFont);
    myReleaseDC(hdc);
}

void
DrawTextClipRegion()
{
#ifdef UNDER_NT
#define CLEARTYPE_QUALITY  5
// nt doesn't support cleartype_compat_quality it appears, so just test cleartype quality instead
#define CLEARTYPE_COMPAT_QUALITY  5
#endif
    TDC     hdc = myGetDC();
    // allow up to a 1% difference with fonts.
    float     OldMaxErrors = SetMaxErrorPercentage((float) .15);
    BOOL   OldVerifySetting;
    HFONT   hfont,
            hfontSav;
    int     iQuality,
            iRgn,
            i,
            nRet;
    UINT    wFormat = DT_LEFT | DT_WORDBREAK;
    HRGN    hrgn;
    DWORD   dw;
    RECT    rc,
            rcRgn;
    LOGFONT lgFont;
    TCHAR   szText[1024];
    TCHAR   szTmp[64];
    TCHAR  *rgszFontName[5] =
        { TEXT("Times New Roman"), TEXT("Courier New"), TEXT("Arial"), TEXT("Tahoma"), TEXT("Times New Roman") };
    int     rgnFontHeight[] = { 11, 14, 25, 49, 58 };   // countof rgnFontHeight == rgnFontWeight
    int     rgnFontWeight[] = { 900, 700, 400, 300, 400 };
    BYTE    rgbFontQuality[] = { ANTIALIASED_QUALITY, CLEARTYPE_QUALITY, CLEARTYPE_COMPAT_QUALITY };
    
    memset((LPVOID) & lgFont, 0, sizeof (LOGFONT));
    lgFont.lfCharSet = DEFAULT_CHARSET;

    nRet = LoadString(globalInst, IDS_String_9962, szText, 1024);
    info(DETAIL, TEXT("DrawTextClipRegion: LoadString:  nRet=%ld\r\n"), nRet);

    rc.left = rc.top = 0;
    rc.right = zx;
    rc.bottom = zy;

    for (iQuality = 0; iQuality < countof(rgbFontQuality); iQuality++)
    {
        lgFont.lfQuality = rgbFontQuality[iQuality];
        
        // if we're testing cleartype quality, turn off verification because the test driver doesn't
        // support cleartype so the character positions will be off.
        OldVerifySetting = SetSurfVerify(!(rgbFontQuality[iQuality] == CLEARTYPE_QUALITY));
        
        for (iRgn = 0; iRgn < 4; iRgn++)
        {
            // set up screen 
            myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);
            // clip regsion 
            rcRgn.left = rand() % 200 + 1;
            rcRgn.top = rand() % 200 + 1;

            while ((nRet = rand() % 200) < 120)
                ;
            rcRgn.right = nRet + rcRgn.left;

            while ((nRet = rand() % 200) < 120)
                ;
            rcRgn.bottom = nRet + rcRgn.top;
            hrgn = CreateRectRgn(rcRgn.left, rcRgn.top, rcRgn.right, rcRgn.bottom);
            if (!hrgn)
            {
                dw = GetLastError();
                if (dw == ERROR_NOT_ENOUGH_MEMORY || dw == ERROR_OUTOFMEMORY)
                    info(DETAIL, TEXT("CreateRectRgn() failed: Out of Memory:"));
                else
                {
                    info(FAIL, TEXT("CreateRectRgn() failed: err=%ld"), dw);
                }
                continue;
            }

            nRet = SelectClipRgn(hdc, hrgn);
            if (nRet == ERROR)
            {
                info(FAIL, TEXT("iQuality=%d: iRgn=%d:  SelectClipRgn() fail: err=%ld\r\n"), iQuality, iRgn, GetLastError());
                goto LDeleteRgn;
            }

            info(DETAIL, TEXT("iQuality=%d: iRgn=%d: clip rect = [%d %d %d %d]\r\n"), iQuality, iRgn, rcRgn.left, rcRgn.top,
                 rcRgn.right, rcRgn.bottom);

            for (i = 0; i < countof(rgnFontHeight); i++)
            {
                _tcscpy(lgFont.lfFaceName, rgszFontName[i]);
                lgFont.lfHeight = rgnFontHeight[i];
                lgFont.lfWeight = rgnFontWeight[i];
                lgFont.lfItalic = TRUE;
                lgFont.lfUnderline = TRUE;

                hfont = CreateFontIndirect(&lgFont);
                hfontSav = (HFONT) SelectObject(hdc, hfont);
                GetTextFace(hdc, 32, szTmp);
                info(DETAIL, TEXT("i=%d  FontFace=%s: %s\n"), i, (LPTSTR) szTmp,
                     (LPTSTR) (iQuality == 0 ? TEXT("ANTIALIASED_QUALITY") : TEXT("ANTIALIASED_QUALITY")));
                nRet = DrawText(hdc, szText, -1, &rc, wFormat);
                if (nRet == 0)
                {
                    dw = GetLastError();
                    if (dw == ERROR_NOT_ENOUGH_MEMORY || dw == ERROR_OUTOFMEMORY)
                        info(DETAIL, TEXT("DrawText() failed: Out of Memory:"));
                    else
                    {
                        info(FAIL, TEXT("i=%d DrawText() failed: err=%ld"), i, GetLastError());
                    }
                }
                info(DETAIL, TEXT("-------------------------> Check:  DrawText() return %d"), nRet);

                SelectObject(hdc, hfontSav);
                DeleteObject(hfont);
                Sleep(100);

                myPatBlt(hdc, 0, 0, zx, zy, WHITENESS);
                nRet = CheckAllWhite(hdc, 1, 1, 1, 0);
                if (nRet != 0)  // found some error: DrawText() draws outside of the clip rec
                {
                    dw = GetLastError();
                    if (dw == ERROR_NOT_ENOUGH_MEMORY || dw == ERROR_OUTOFMEMORY)
                        info(DETAIL, TEXT("CheckAllWhite() failed: Out of Memory:"));
                    else
                    {
                        info(FAIL, TEXT("CheckAllWhite() failed: found %d pixels are not white"), nRet);
                    }
                }
            }

            nRet = SelectClipRgn(hdc, (HRGN) NULL);
          LDeleteRgn:
            if (!DeleteObject(hrgn))
            {
                info(FAIL, TEXT("iQuality=%d: iRgn=%d:  DeleteObject(hRgn) fail: err=%ld\r\n"), iQuality, iRgn, GetLastError());
            }
        }

        SetSurfVerify(OldVerifySetting);
    }

    myReleaseDC(hdc);
    // have to restore the max error percentage after releasing, so it's still in effect when doing the final verification.
   SetMaxErrorPercentage(OldMaxErrors);
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
    HFONT   hFont;
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

    //setup 
    _tcscpy(word, TEXT("The Quick Brown Fox jumps right over the lazy dog"));
    len = _tcslen(word);
    SetTextColor(hdc, RGB(0, 0, 0));
    SetBkMode(hdc, TRANSPARENT);
    SetBkColor(hdc, RGB(255, 0, 0));
    hFont = setupFont(1, aFont, hdc, 25, 0, 0, 0, 400, 0, 0, 0);

    for (i = 0; i < 100; i++)
    {
        index = i % countof(nvAlignments);
        result = SetTextAlign(hdc, nvAlignments[index].dwVal);
        if(result == GDI_ERROR)
            info(FAIL, TEXT("SetTextAlign failed %s GLE: %s"), nvAlignments[index].szName, GetLastError());
        else 
            info(DETAIL, TEXT(" SetTextAlign Successful %s"), nvAlignments[index].szName);
        
        if (!ExtTextOut(hdc, zx/2, zy/2, ETO_OPAQUE, NULL, word, len, NULL))
        {
            info(FAIL, TEXT("AlignExtTestOut Failed for %s"), nvAlignments[index].szName, _T("where the last error was %s"), GetLastError());
            break;
        }

    }

    if (hFont)
        cleanupFont(1, aFont, hdc, hFont);
    myReleaseDC(hdc);

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

    // Breadth
    passNull2Text(EExtTextOut);
    passNull2ExtTextOut(EExtTextOut);
    CreditsExtTestOut(EExtTextOut);
    SpeedExtTestOut(EExtTextOut);
    AlignExtTestOut(EExtTextOut);
    ColorFillingExtTextOut(EExtTextOut);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetTextAlign_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Text(EGetTextAlign);
    randGetSetTextAlign(EGetTextAlign);
    realGetSetTextAlign(EGetTextAlign);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetTextExtentPoint32_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Text(EGetTextExtentPoint32);
    GetTextExtentPoint32ImpactTest(EGetTextExtentPoint32);
    GetTextExtentPoint32Test(EGetTextExtentPoint32);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetTextFace_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Text(EGetTextFace);
    GetTextFaceTest(EGetTextFace);
    SpecialParams(EGetTextFace);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetTextMetrics_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Text(EGetTextMetrics);
    GetTextMetricsTest(EGetTextMetrics);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI SetTextAlign_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Text(ESetTextAlign);
    randGetSetTextAlign(ESetTextAlign);
    realGetSetTextAlign(ESetTextAlign);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI DrawTextW_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Text(EDrawTextW);
    passNull2DrawText(EDrawTextW);
    DrawText_DT_XXXX(EDrawTextW);
    DrawTextCheckUnderline(EDrawTextW);
    DrawTextExpandTabs(EDrawTextW);
    DrawTextUnderline(EDrawTextW);

    // Depth
    DrawTextClipRegion();

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetTextExtentPoint_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Text(EGetTextExtentPoint);

    // Depth
    // None

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GetTextExtentExPoint_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
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

static int myFlag = OPAQUE,
        myFont = aFont;
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
        _tcscpy(TestStr, str);
}

void
setFont(int newFont)
{
    myFont = newFont;
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
    myPatBlt(hdc, 0, 0, zx, zy, BLACKNESS);
    SetBkMode(hdc, myFlag);
    SelectObject(hdc, GetStockObject(DKGRAY_BRUSH));
    SetBkColor(hdc, RGB(0, 0, 255));
    SetTextColor(hdc, RGB(255, 255, 0));
    setNewCharSpace(0, 0);
}

//***********************************************************************************
void
BlastStr(TDC hdc, TCHAR * str, int line)
{
    RECT    aRect = { 0, 0, 300, 200 };

    if (gSanityChecks)
        info(DETAIL, TEXT("%s"), str);
    ExtTextOut(hdc, 10, 10 + line * 20, ETO_CLIPPED, &aRect, str, _tcslen(str), NULL);
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
    GetObject(tempFont, sizeof (LOGFONT), &fontInfo);

    _tcsncpy(temp, TestStr, countof(temp));
    temp[countof(temp)-1] = TEXT('\0');
    BlastStr(hdc, temp, i++);

    _sntprintf(temp, countof(temp), TEXT("FName:<%s>"), fontInfo.lfFaceName);
    temp[countof(temp)-1] = TEXT('\0');
    BlastStr(hdc, temp, i++);

    _sntprintf(temp, countof(temp), TEXT("H:%ld W:%ld Wei:%ld"), fontInfo.lfHeight, fontInfo.lfWidth, fontInfo.lfWeight);
    temp[countof(temp)-1] = TEXT('\0');
    BlastStr(hdc, temp, i++);

    _sntprintf(temp, countof(temp), TEXT("Esc:%ld Ori:%ld"), fontInfo.lfEscapement, fontInfo.lfOrientation);
    temp[countof(temp)-1] = TEXT('\0');
    BlastStr(hdc, temp, i++);

    _sntprintf(temp, countof(temp), TEXT("Ita:%d Und:%d Str:%d"), fontInfo.lfItalic, fontInfo.lfUnderline, fontInfo.lfStrikeOut);
    temp[countof(temp)-1] = TEXT('\0');
    BlastStr(hdc, temp, i++);

    _sntprintf(temp, countof(temp), TEXT("BkM:%d Clip:%d %d %d %d"), myFlag, clipRect.left, clipRect.top, clipRect.right, clipRect.bottom);
    temp[countof(temp)-1] = TEXT('\0');
    BlastStr(hdc, temp, i++);

    _sntprintf(temp, countof(temp), TEXT("ChSpace(%s): %d"), (charSpace) ? TEXT("ON") : TEXT("OFF"), (charSpace) ? charSpaceValues[0] : 0);
    temp[countof(temp)-1] = TEXT('\0');
    BlastStr(hdc, temp, i++);

    SelectObject(hdc, (HFONT) tempFont);
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

    SetTextColor(hdc, RGB(0, 0, 0));
    SetBkColor(hdc, RGB(0, 0, 0));
    SetBkMode(hdc, oldMode);
    SetTextAlign(hdc, oldAlign);

    if (hOldFont)
        SelectObject(hdc, hOldFont);
    if (oldFlags != -1)
        ExtTextOut(hdc, zx / 2, 3 * zy / 4, oldFlags, &clipRect, oldStr, _tcslen(oldStr), oldCharSpace);
    if (hCurrFont)
        SelectObject(hdc, hCurrFont);

    SetTextColor(hdc, RGB(128, 128, 128));
    SetBkColor(hdc, RGB(0, 0, 0));
    SetBkMode(hdc, TRANSPARENT);
    SetTextAlign(hdc, TA_BASELINE | TA_LEFT);

    if (DirtyFlag)
        myPatBlt(hdc, 0, 0, zx, zy, BLACKNESS);
    else
        PatBlt(hdc, 10, 10, 280, 190, BLACKNESS);

    for (int x = 0; x < zx; x += RESIZE(40))
    {
        myPatBlt(hdc, x, 0, 1, zy, PATCOPY);
        myPatBlt(hdc, 0, x, zx, 1, PATCOPY);
    }

    myPatBlt(hdc, zx / 2, 0, 2, zy, WHITENESS);
    myPatBlt(hdc, 0, 3 * zy / 4, zx, 2, WHITENESS);

    DebugInfo(hdc);

    SetTextColor(hdc, tempColor);
    SetBkColor(hdc, tempBkColor);
    SetBkMode(hdc, tempMode);
    SetTextAlign(hdc, tempAlign);
}

//***********************************************************************************
void
myExtTextOut(TDC hdc, HFONT hFont, HFONT oldFont, UINT Flags, TCHAR * str0, TCHAR * str1, TCHAR * str2)
{
    unsigned int nLengthAvail = MAXLEN - 1;
    TCHAR   str[MAXLEN];

    str[0] = NULL;

    // copy the entire string if there's room, copy nothing if there isn't.
    // nLengthAvail leaves 1 space for the NULL, and wcslen returns the length excluding the null, so 
    // nLenghAvail is the number of non-null spaces available, hence the check for nLenghtAvail > wcslen instead of
    // nLenghtAvail >= wcslen.
    if (str0 && nLengthAvail > _tcslen(str0))
    {
        _tcscat(str, str0);
        nLengthAvail -= _tcslen(str0);
    }
    if (str1 && nLengthAvail > _tcslen(str1))
    {
        _tcscat(str, str1);
        nLengthAvail -= _tcslen(str1);
    }
    if (str2 && nLengthAvail > _tcslen(str2))
    {
        _tcscat(str, str2);
        nLengthAvail -= _tcslen(str2);
    }

    DrawHash(hdc, hFont, oldFont);
    ExtTextOut(hdc, zx / 2, 3 * zy / 4, Flags, &clipRect, str, _tcslen(str), charSpace);

    oldFlags = Flags;
    _tcscpy(oldStr, str);
    oldMode = GetBkMode(hdc);
    oldAlign = GetTextAlign(hdc);
    setOldCharSpace();
    Sleep(1);
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
    HFONT   hFont = setupFont(0, myFont, hdc, 45, 0, 0, 0, 400, 0, 0, 0);
    TCHAR   str[256];

    initManualFonts(hdc);

    for (int i = 0; i < 3; i++)
        for (int j = 3; j < numA; j++)
        {
            SetTextAlign(hdc, AlignsFlag[i] | AlignsFlag[j]);
            myExtTextOut(hdc, hFont, hFont, ETO_CLIPPED, AlignsStr[i], TEXT(" | "), AlignsStr[j]);
            _stprintf(str, TEXT("Text with the attributes: %s %s"), AlignsStr[i], AlignsStr[j]);
            AskMessage(TestStr, TEXT("AlignTest"), str);
        }
    SelectObject(hdc, hFont);
    DrawHash(hdc, NULL, NULL);
    cleanupFont(0, myFont, hdc, hFont);
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
    HFONT   hFont,
            oldFont = NULL;

    initManualFonts(hdc);
    for (int j = 0; j < 1000; j += 100)
    {
        hFont = setupFont(0, myFont, hdc, 45, 0, 0, 0, j, 0, 0, 0);
        SetTextAlign(hdc, TA_TOP | TA_CENTER);
        myExtTextOut(hdc, hFont, oldFont, ETO_CLIPPED, TEXT("Gaining Weight"), NULL, NULL);
        if (oldFont)
            cleanupFont(0, myFont, hdc, oldFont);
        oldFont = hFont;
        Sleep(10);
    }
    AskMessage(TestStr, TEXT("WeightTest"), TEXT("the font get fatter"));
    SelectObject(hdc, oldFont);
    DrawHash(hdc, NULL, NULL);
    cleanupFont(0, myFont, hdc, oldFont);
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

    initManualFonts(hdc);
    SetTextAlign(hdc, TA_TOP | TA_CENTER);

    for (i = 120; i <= 3600; i += 120)
    {
        hFont = setupFont(0, myFont, hdc, (long) ((float) (i + 80) / (float) 80), 0, 0, i % 3600, 400, 0, typeFlag, typeFlag);
        myExtTextOut(hdc, hFont, oldFont, ETO_CLIPPED, TEXT("Orientation"), NULL, NULL);
        if (oldFont)
            cleanupFont(0, myFont, hdc, oldFont);
        oldFont = hFont;
        Sleep(10);
    }
    AskMessage(TestStr, TEXT("OriEscTest"), TEXT("the Orientation stay the same"));
    SelectObject(hdc, oldFont);
    DrawHash(hdc, NULL, NULL);
    cleanupFont(0, myFont, hdc, oldFont);

    oldFont = NULL;
    for (i = 120; i <= 3600; i += 120)
    {
        hFont = setupFont(0, myFont, hdc, (long) ((float) (i + 80) / (float) 80), 0, i % 3600, 0, 400, 0, typeFlag, typeFlag);
        myExtTextOut(hdc, hFont, oldFont, ETO_CLIPPED, TEXT("Escapement"), NULL, NULL);
        if (oldFont)
            cleanupFont(0, myFont, hdc, oldFont);
        oldFont = hFont;
        Sleep(10);
    }
    AskMessage(TestStr, TEXT("OriEscTest"), TEXT("the Escapement change"));
    SelectObject(hdc, oldFont);
    DrawHash(hdc, NULL, NULL);
    cleanupFont(0, myFont, hdc, oldFont);
    myReleaseDC(hdc);
}

//***********************************************************************************
void
TestFontAndText(void)
{

    setTestName(TEXT("TestFontAndText"));

    TDC     hdc = myGetDC();
    int     i;
    HFONT   hFont;

    initManualFonts(hdc);
    SetTextAlign(hdc, TA_TOP | TA_CENTER);

    i = 2040;
    hFont = setupFont(0, myFont, hdc, 20, 0, i % 3600, 0, 400, 0, 1, 1);
    myExtTextOut(hdc, hFont, NULL, ETO_CLIPPED, TEXT("Escapement"), NULL, NULL);

    //   Sleep(3000);
    cleanupFont(0, myFont, hdc, hFont);
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
    TCHAR   str[256];

    initManualFonts(hdc);

    for (i = 0; i < 7; i++)
    {
        hFont = setupFont(0, myFont, hdc, 55, 0, 0, 0, 400, attrib[i][0], attrib[i][1], attrib[i][2]);
        SetTextAlign(hdc, TA_TOP | TA_CENTER);

        myExtTextOut(hdc, hFont, oldFont, ETO_CLIPPED,
                     attrib[i][0] ? AttribStr[0] : NULL,
                     attrib[i][1] ? AttribStr[1] : NULL,
                     attrib[i][2] ? AttribStr[2] : NULL);
        if (oldFont)
            cleanupFont(0, myFont, hdc, oldFont);
        oldFont = hFont;
        
        _stprintf(str, TEXT("Text with the attributes: Italic:%d Underline:%d Strike:%d"),
                  attrib[i][0], attrib[i][1], attrib[i][2]);
        AskMessage(TestStr, TEXT("AttribTest"), str);
    }
    SelectObject(hdc, oldFont);
    DrawHash(hdc, NULL, NULL);
    cleanupFont(0, myFont, hdc, oldFont);
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

    initManualFonts(hdc);
    SetTextAlign(hdc, TA_TOP | TA_CENTER);

    for (i = 0; i < 80; i += 15)
    {
        hFont = setupFont(0, myFont, hdc, i, 0, 0, 0, 400, 0, 0, 0);
        myExtTextOut(hdc, hFont, oldFont, ETO_CLIPPED, TEXT("Scale All"), NULL, NULL);
        if (oldFont)
            cleanupFont(0, myFont, hdc, oldFont);
        oldFont = hFont;
        Sleep(10);
    }
    AskMessage(TestStr, TEXT("ScaleTest"), TEXT("the font scale up"));
    SelectObject(hdc, oldFont);
    DrawHash(hdc, NULL, NULL);
    cleanupFont(0, myFont, hdc, oldFont);

    oldFont = NULL;
    for (i = 0; i < 80; i += 15)
    {
        hFont = setupFont(0, myFont, hdc, 0, i, 0, 0, 400, 0, 0, 0);
        myExtTextOut(hdc, hFont, oldFont, ETO_CLIPPED, TEXT("Scale Width"), NULL, NULL);
        if (oldFont)
            cleanupFont(0, myFont, hdc, oldFont);
        oldFont = hFont;
        Sleep(10);
    }
    AskMessage(TestStr, TEXT("ScaleTest"), TEXT("the font scale the width"));
    SelectObject(hdc, oldFont);
    DrawHash(hdc, NULL, NULL);
    cleanupFont(0, myFont, hdc, oldFont);

    oldFont = NULL;
    for (i = 0; i < 200; i += 30)
    {
        hFont = setupFont(0, myFont, hdc, i, 30, 0, 0, 400, 0, 0, 0);
        myExtTextOut(hdc, hFont, oldFont, ETO_CLIPPED, TEXT("Scale Height"), NULL, NULL);
        if (oldFont)
            cleanupFont(0, myFont, hdc, oldFont);
        oldFont = hFont;
        Sleep(10);
    }
    AskMessage(TestStr, TEXT("ScaleTest"), TEXT("the font scale the height"));
    SelectObject(hdc, oldFont);
    DrawHash(hdc, NULL, NULL);
    cleanupFont(0, myFont, hdc, oldFont);
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

    initManualFonts(hdc);

    for (i = 0; i < 40; i += 4)
    {
        hFont = setupFont(0, myFont, hdc, 55, 0, 0, 0, 400, 0, 0, 0);
        SetTextAlign(hdc, TA_TOP | TA_CENTER);
        setNewCharSpace(i, 50);
        myExtTextOut(hdc, hFont, oldFont, ETO_CLIPPED, TEXT("Character Space"), NULL, NULL);
        if (oldFont)
            cleanupFont(0, myFont, hdc, oldFont);
        oldFont = hFont;
        Sleep(10);
    }
    AskMessage(TestStr, TEXT("CharSpaceTest"), TEXT("the Intercharacter Spacing Increase"));
    SelectObject(hdc, oldFont);
    DrawHash(hdc, NULL, NULL);
    cleanupFont(0, myFont, hdc, oldFont);
    myReleaseDC(hdc);
}

//***********************************************************************************
TESTPROCAPI ManualFont_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    setFont(aFont);
    RemoveAllFonts();
    if (myAddFontResource(fontArray[myFont].fileName) == 0 && (fontArray[myFont].type != EBad))
        return getCode();

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

    RemoveFontResource(fontArray[myFont].fileName);
    return getCode();
}

//***********************************************************************************
TESTPROCAPI ManualFontClipped_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    setFont(aFont);
    RemoveAllFonts();
    if (myAddFontResource(fontArray[myFont].fileName) == 0 && (fontArray[myFont].type != EBad))
        return getCode();

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
    RemoveFontResource(fontArray[myFont].fileName);
    return getCode();
}
