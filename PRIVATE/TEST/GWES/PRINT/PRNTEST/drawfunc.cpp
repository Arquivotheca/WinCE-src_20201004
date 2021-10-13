//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#define DEBUG_OUTPUT  1

#include <windows.h>
#include <tchar.h>
#include <string.h>
#include <memory.h>
#ifndef UNDER_CE
#include <winspool.h>
#else
#include <winddi.h>
#endif
#include <tux.h>
#include "drawfunc.h"

extern HINSTANCE g_hInst;
extern HFONT g_hfont;
extern int g_nTotalPages;
extern RECT g_rcPrint;
extern BOOL g_fDraftMode;
extern BOOL g_fUserAbort;

#define HIGHEST_BITMAP		     520

VOID LogResult (int nLevel, LPCTSTR szFormat, ...);
BOOL PASCAL CallEndAndStartNewPage (HDC hdcPrint, LPDWORD lpdwError);


//=========================================================================
//         Draw functions:
//=========================================================================
int PASCAL DrawPageXXXBlt (HDC hdcPrint, int iDoc, int iPage, int nCallAbort)
{
    int i, nBitmaps, iLoop;
    HDC hdcMem;
    HBITMAP rghbm[MAX_BITMAPS], hbmMask, hbmPrev;
    int nRetCode = 0, yDest = 0, xDest = 0, yMax, nRet, nSpace = 10;
    RECT rc, rcTmp;
    BITMAP bmp;
    DWORD dwError, dwRop;
    HFONT hfontPrev = NULL;
    TCHAR szTmp[256], ropName[64];
    MEMORYSTATUS mem;

    dwRop = grgdwRop[iDoc].dwRop;
    _tcscpy (ropName, grgdwRop[iDoc].ropName);
    wsprintf (szTmp, TEXT ("iDoc=%d iPage=%d ROP=%s"), iDoc + 1, iPage + 1, (LPTSTR) ropName);
    LogResult (1, TEXT ("------------------ DrawPageXXXBlt() Starts!   %s"), (LPTSTR) szTmp);

    rc = g_rcPrint;
    SetLastError (0);
    SetViewportOrgEx (hdcPrint, 0, 0, NULL);
    hdcMem = CreateCompatibleDC (hdcPrint);
    if (!hdcMem)
    {
        dwError = GetLastError ();
        LogResult (1, TEXT ("DrawPageXXXBlt(): CreateCompatibleDC() failed: err=%ld"), dwError);
        return dwError;
    }
    else
    {
        LogResult (DEBUG_OUTPUT, TEXT ("DrawPageXXXBlt(): CreateCompatibleDC() succeeded; hdcMem=0x%LX"), hdcMem);
    }

    nBitmaps = 6;

//=============================
// BitBlt  one row
//=============================
    nRet = DrawText (hdcPrint, szTmp, -1, &rc, DT_WORD_ELLIPSIS);
    rc.top += nRet + nSpace;
    wsprintf (szTmp, TEXT ("BitBlt:"));
    nRet = DrawText (hdcPrint, szTmp, -1, &rc, DT_WORD_ELLIPSIS);
    yDest = rc.top + nRet + nSpace;
    yMax = 400;
    for (i = 0; i < nBitmaps; i++)
    {
        SetLastError (0);
        rghbm[i] = LoadBitmap (g_hInst, szBitmap[i]);
        if (!rghbm[i])
        {
            nRetCode = GetLastError ();
            LogResult (1, TEXT ("LoadBitmap(%s) Failed: error=%ld"), (LPTSTR) szBitmap[i], nRetCode);
        }
        GetObject (rghbm[i], sizeof (BITMAP), &bmp);

        SetLastError (0);
        hbmPrev = (HBITMAP) SelectObject (hdcMem, rghbm[i]);
        if (!hbmPrev)
        {
            LogResult (1, TEXT ("BitBlt: i=%d: SelectObject() failed: err=%ld\r\n"), i, GetLastError ());
        }

        if (!BitBlt (hdcPrint, xDest, yDest, bmp.bmWidth, bmp.bmHeight, hdcMem, 0, 0, dwRop))
        {
            wsprintf (szTmp, TEXT ("BBlt %d: err=%ld"), i, GetLastError ());
            LogResult (DEBUG_OUTPUT, szTmp);
            rcTmp.left = xDest;
            rcTmp.top = yDest;
            rcTmp.right = xDest + bmp.bmWidth;
            rcTmp.bottom = yDest + bmp.bmHeight;
            DrawText (hdcPrint, szTmp, -1, &rcTmp, DT_WORD_ELLIPSIS);
        }

        xDest += bmp.bmWidth + 20;
        if (bmp.bmHeight > yMax)
            yMax = bmp.bmHeight;
        SelectObject (hdcMem, hbmPrev);
        DeleteObject (rghbm[i]);
    }

    // Now do MaskBlt():  2 rows
    rc.top = yDest + yMax + nSpace;
    for (iLoop = 0; iLoop < 2; iLoop++)
    {
        switch (iLoop)
        {
            case 0:
                wsprintf (szTmp, TEXT ("MaskBlt: Balloons mask"));
                break;
            case 1:
                wsprintf (szTmp, TEXT ("MaskBlt: Small white mask:  Cut part of the large bitmap"));
                break;
        }
        nRet = DrawText (hdcPrint, szTmp, -1, &rc, DT_WORD_ELLIPSIS);
        yDest = rc.top + nRet + nSpace;
        xDest = 0;
        yMax = 100;

        hbmMask = LoadBitmap (g_hInst, szMaskBitmap[iLoop]);
        if (!hbmMask)
        {
            nRetCode = GetLastError ();
            LogResult (1, TEXT ("i=%d LoadBitmap(MASK: %s) Failed: error=%ld"), iLoop, (LPTSTR) szMaskBitmap[iLoop], nRetCode);
        }
        // First print: put them at the same line as Shrink print
        for (i = 0; i < nBitmaps; i++)
        {
            rghbm[i] = LoadBitmap (g_hInst, szBitmap[i]);
            if (!rghbm[i])
            {
                nRetCode = GetLastError ();
                LogResult (1, TEXT ("i=%d LoadBitmap(%s) Failed: error=%ld"), i, (LPTSTR) szBitmap[i], nRetCode);
            }
            GetObject (rghbm[i], sizeof (BITMAP), &bmp);

            hbmPrev = (HBITMAP) SelectObject (hdcMem, rghbm[i]);

            if (!MaskBlt (hdcPrint, xDest, yDest, bmp.bmWidth, bmp.bmHeight, hdcMem, 0, 0, hbmMask, 0, 0, dwRop))
            {
                dwError = GetLastError ();
                wsprintf (szTmp, TEXT ("MBlt %d: err=%ld"), i, dwError);
                rcTmp.left = xDest;
                rcTmp.top = yDest;
                rcTmp.right = xDest + bmp.bmWidth;
                rcTmp.bottom = yDest + bmp.bmHeight;
                DrawText (hdcPrint, szTmp, -1, &rcTmp, DT_WORD_ELLIPSIS);

                if (dwError == ERROR_NOT_ENOUGH_MEMORY || dwError == ERROR_OUTOFMEMORY)
                {
                    GlobalMemoryStatus (&mem);
                    wsprintf (szTmp, TEXT ("MBlt %d: err=%ld: dwAvailPhys=%u bytes = %u K"), i, dwError, mem.dwAvailPhys,
                        mem.dwAvailPhys/1024);
                }
                LogResult (DEBUG_OUTPUT, szTmp);
            }

            xDest += bmp.bmWidth + 20;
            if (bmp.bmHeight > yMax)
                yMax = bmp.bmHeight;

            SelectObject (hdcMem, hbmPrev);
            if (!DeleteObject (rghbm[i]))
            {
                LogResult (DEBUG_OUTPUT, TEXT ("DrawPageXXXBlt(: iLoop=%d delete hbm Failed: error=%ld"), GetLastError ());
            }
        }

        if (hbmMask && !DeleteObject (hbmMask))
        {
            LogResult (DEBUG_OUTPUT,
                TEXT ("DrawPageXXXBlt(: iLoop=%d delete hbmMask Failed: error=%ld"), iLoop, GetLastError ());
        }

        rc.top = yDest + yMax + nSpace;
        if ((rc.top + yMax / 2) > rc.bottom)
        {
            LogResult (DEBUG_OUTPUT, TEXT ("DrawPageXXXBlt(): CallEndAndStartNewPage()"));
            if (!CallEndAndStartNewPage (hdcPrint, &dwError))
            {
                if (g_fUserAbort)
                    LogResult (1, TEXT ("User aborted printing."));
                else
                    nRetCode = dwError;
                goto LRelease;
            }
            rc = g_rcPrint;
        }
    }

//===============================
// StretchBlt:   one row  Shrink
//===============================
    rc.top = yDest + yMax + nSpace;
    wsprintf (szTmp, TEXT ("StretchBlt:  Shrink 1/2"));
    nRet = DrawText (hdcPrint, szTmp, -1, &rc, DT_WORD_ELLIPSIS);
    yDest = rc.top + nRet + 20;
    xDest = 0;
    yMax = 100;
    for (i = 0; i < nBitmaps; i++)
    {
        rghbm[i] = LoadBitmap (g_hInst, szBitmap[i]);
        if (!rghbm[i])
        {
            nRetCode = GetLastError ();
            LogResult (DEBUG_OUTPUT, TEXT ("LoadBitmap(%s) Failed: error=%ld"), (LPTSTR) szBitmap[i], nRetCode);
        }
        GetObject (rghbm[i], sizeof (BITMAP), &bmp);
        hbmPrev = (HBITMAP) SelectObject (hdcMem, rghbm[i]);

        // Shrink
        if (!StretchBlt
            (hdcPrint, xDest, yDest, bmp.bmWidth / 2, bmp.bmHeight / 2, hdcMem, 0, 0, bmp.bmWidth, bmp.bmHeight, dwRop))
        {
            wsprintf (szTmp, TEXT ("SBlt %d: err=%ld"), i, GetLastError ());
            LogResult (DEBUG_OUTPUT, szTmp);
            rcTmp.left = xDest;
            rcTmp.top = yDest;
            rcTmp.right = xDest + bmp.bmWidth;
            rcTmp.bottom = yDest + bmp.bmHeight;
            DrawText (hdcPrint, szTmp, -1, &rcTmp, DT_WORD_ELLIPSIS);
        }

        xDest += (bmp.bmWidth / 2 + 20);
        if ((bmp.bmHeight / 2) > yMax)
            yMax = bmp.bmHeight / 2;
        SelectObject (hdcMem, hbmPrev);
        DeleteObject (rghbm[i]);
    }

//===============================
// patBlt:   one row
//===============================
    rc.top = yDest + yMax + nSpace * 2;
    rc.right = g_rcPrint.right;
    wsprintf (szTmp, TEXT ("PatBlt:  Gray Scale"));
    nRet = DrawText (hdcPrint, szTmp, -1, &rc, DT_WORD_ELLIPSIS);
    rc.top += nRet + nSpace * 2;

    nSpace = rc.right / 255;    // width of each small rectangle
    yMax = GetDeviceCaps (hdcPrint, LOGPIXELSX);
    xDest = rc.left;
    if (iDoc % 2)
        dwRop = PATINVERT;
    else
        dwRop = PATCOPY;
    for (i = 0; i < 256; i++, xDest += nSpace)
    {
        HBRUSH hOldbrush, hBrush = CreateSolidBrush (RGB (i, i, i));

        if (hBrush == NULL)
            LogResult (1, TEXT ("i=%d: CreateSolidBrush failed. err=%ld"), i, GetLastError ());
        hOldbrush = (HBRUSH) SelectObject (hdcPrint, hBrush);
        if (hOldbrush == NULL)
            LogResult (1, TEXT ("i=%d: Select Brush failed. err=%ld"), i, GetLastError ());
        if (!(PatBlt (hdcPrint, xDest, rc.top, nSpace, yMax, dwRop)))
            LogResult (1, TEXT ("i=%d: PatBlt failed. err=%ld"), i, GetLastError ());
        SelectObject (hdcPrint, hOldbrush);
        if (DeleteObject (hBrush) == 0)
            LogResult (1, TEXT ("i=%d: Delete brush failed. err=%ld"), i, GetLastError ());
    }

  LRelease:
    if (!DeleteObject (hdcMem))
        LogResult (1, TEXT ("DrawPageXXXBlt(: delete hdcMem Failed: error=%ld"), GetLastError ());
    SelectObject (hdcPrint, hfontPrev);
    return nRetCode;
}

int PASCAL DrawPageFonts (HDC hdcPrint, int iDoc, int iPage, int nCallAbort)
{
    int i, nRet, nDPI, nScreenPix, nRet360 = 0;
    HDC hdc;
    HFONT hfont = NULL, hfontPrev;

    int nRetCode = 0, nTests = MAX_SZTEST;
    LOGFONT lf;
    RECT rc, rcSave, rcTmp = {0};
    POINT pt;
    TEXTMETRIC tm;
    TCHAR szTmp[64];

    LogResult (1, TEXT ("------------------ DrawPageFonts() Starts!  iDoc=%d  iPage=%d"), iDoc + 1, iPage + 1);

    GetClipBox (hdcPrint, &rc);
    rcSave.left = rc.left;
    rcSave.top = rc.top;
    rcSave.right = rc.right;
    rcSave.bottom = rc.bottom;

    Rectangle (hdcPrint, rc.left, rc.top, rc.right, rc.bottom);

    SetBkMode (hdcPrint, OPAQUE);
    nDPI = GetDeviceCaps (hdcPrint, LOGPIXELSY);
    hdc = GetDC (NULL);
    nScreenPix = GetDeviceCaps (hdc, LOGPIXELSY);
    ReleaseDC (NULL, hdc);

    memset ((LPVOID) & lf, 0, sizeof (LOGFONT));
    for (i = 0; i < nTests; i++)
    {

        switch (GetSystemDefaultLCID ())
        {
            case 0x411:        // Japan
                lf.lfHeight = rgFontInfo_Jpn[i].Height * nDPI / nScreenPix;
                lf.lfWeight = rgFontInfo_Jpn[i].Weight;
                lf.lfItalic = rgFontInfo_Jpn[i].Italic;
                lf.lfUnderline = rgFontInfo_Jpn[i].Underline;
                lf.lfStrikeOut = rgFontInfo_Jpn[i].StrikeOut;
                lf.lfEscapement = rgFontInfo_Jpn[i].Escapement;
                lf.lfOrientation = rgFontInfo_Jpn[i].Orientation;
                lf.lfCharSet = SHIFTJIS_CHARSET;
                _tcscpy (lf.lfFaceName, rgFontInfo_Jpn[i].szFontName);
                break;

            case 0x409:        // US

            default:
                lf.lfHeight = rgFontInfo[i].Height * nDPI / nScreenPix;
                lf.lfWeight = rgFontInfo[i].Weight;
                lf.lfItalic = rgFontInfo[i].Italic;
                lf.lfUnderline = rgFontInfo[i].Underline;
                lf.lfStrikeOut = rgFontInfo[i].StrikeOut;
                lf.lfEscapement = rgFontInfo[i].Escapement;
                lf.lfOrientation = rgFontInfo[i].Orientation;

#ifdef UNDER_CE
                if (iPage > 0)
                    lf.lfQuality |= CLEARTYPE_QUALITY; // CLEARTYPE_QUALITY == 5
#endif

                _tcscpy (lf.lfFaceName, rgFontInfo[i].szFontName);
                break;
        }

        hfont = CreateFontIndirect (&lf);
        hfontPrev = (HFONT) SelectObject (hdcPrint, hfont);

        GetTextFace (hdcPrint, 64, szTmp);
        GetTextMetrics (hdcPrint, &tm);

        switch (GetSystemDefaultLCID ())
        {
            case 0x411:        // Japan
                LogResult (DEBUG_OUTPUT,
                    TEXT ("i=%d: want: Name=%s  Height=%d\r\n"), i, (LPTSTR) rgFontInfo_Jpn[i].szFontName, lf.lfHeight);
                break;

            case 0x409:        // US

            default:
                LogResult (DEBUG_OUTPUT,
                    TEXT ("i=%d: want: Name=%s  Height=%d\r\n"), i, (LPTSTR) rgFontInfo[i].szFontName, lf.lfHeight);
                break;
        }

        LogResult (DEBUG_OUTPUT, TEXT ("i=%d: get : Name=%s  Height=%d\r\n"), i, (LPTSTR) szTmp, tm.tmHeight);

        switch (GetSystemDefaultLCID ())
        {
            case 0x411:        // Japan
#ifdef UNICODE
                nRet = DrawText (hdcPrint, rgszTestInfo_Jpn[i].sz, -1, &rc, rgszTestInfo_Jpn[i].uFormat);
#else
                nRet = 0;
#endif
                break;

            case 0x409:        // US

            default:
                nRet = DrawText (hdcPrint, rgszTestInfo[i].sz, -1, &rc, rgszTestInfo[i].uFormat);
                break;
        }

        if (i == 6)             // string rotate 360 degree
            nRet360 = nRet;

        SelectObject (hdcPrint, hfontPrev);
        DeleteObject (hfont);

        // default:
        SetTextColor (hdcPrint, RGB (0, 0, 0));
        SetBkColor (hdcPrint, RGB (0xFF, 0xFF, 0xFF));

        switch (i)
        {
            case 3:            // prepare for 4th string
                memcpy (&rcTmp, &rc, sizeof (RECT));
                LogResult (DEBUG_OUTPUT, TEXT ("i=3: DT_CALCRECT: Beefore rcTmp= [%d %d %d %d]"), rcTmp);

                switch (GetSystemDefaultLCID ())
                {
                    case 0x411: // Japan
#ifdef UNICODE
                        nRet = DrawText (hdcPrint, rgszTestInfo_Jpn[i].sz, -1, &rcTmp, DT_CALCRECT | rgszTestInfo_Jpn[i].uFormat);
#else
                        nRet = 0;
#endif
                        break;

                    case 0x409: // US

                    default:
                        nRet = DrawText (hdcPrint, rgszTestInfo[i].sz, -1, &rcTmp, DT_CALCRECT | rgszTestInfo[i].uFormat);
                        break;
                }

                LogResult (DEBUG_OUTPUT, TEXT ("i=3: DT_CALCRECT: retuned rcTmp= [%d %d %d %d]"), rcTmp);
                // For 4th string: will draw it at the save level as string 3
                rc.right -= (rcTmp.right + 10);
                break;
            case 4:            //  Adjust for 5th string: Show at the center: don't update rc.top
                // text is "At the Center!\r\nAt the Center!"
                rc.right += (rcTmp.right + 10);
                SetTextColor (hdcPrint, RGB (0xFF, 0, 0)); // RED
                break;

            case 5:            // Prepage for show rotaing string: 6 [360 degree] and ,7,8,9 strings
                SetViewportOrgEx (hdcPrint, rcSave.right / 2, rcSave.bottom / 2, &pt);
                rc.left = 0;
                rc.top = 0;
                rc.right = rcSave.right / 2;
                rc.bottom = rcSave.bottom / 2;
                SetTextColor (hdcPrint, RGB (0xFF, 0, 0)); // Red
                SetBkColor (hdcPrint, RGB (0, 0, 0)); //  background
                break;

            case 6:            //  prepare for 7th string: Rotate 270.
                rc.left = -10;
                rc.top = 10;
                rc.right = -rcSave.right / 2;
                rc.bottom = 3000; //  exceeds
                SetBkColor (hdcPrint, RGB (255, 255, 0)); //background=YELLOW
                break;

            case 7:            //prepare for 8th: Rotate string 90  degree:
                rc.left = 0;
                rc.top = -10;
                rc.right = rcSave.right / 2;
                rc.bottom = -rcSave.bottom / 2;
                SetTextColor (hdcPrint, RGB (0, 0xFF, 0)); // Green
                break;
            case 8:            // Prepare for 9th string: rotate 180 degree
                rc.left = -10;
                rc.top = 0;
                rc.right = -rcSave.right / 2;
                rc.bottom = -rcSave.bottom / 2;
                SetTextColor (hdcPrint, RGB (0, 0, 0xFF)); // Blue
                break;


            case 9:            //  Prepare for 10th string: same height as string rotate 360: at left
                SetViewportOrgEx (hdcPrint, pt.x, pt.y, &pt);
                rc.left = 0;
                rc.top = rcSave.bottom / 2;
                rc.right = rcSave.right / 2 - 100;
                rc.bottom = rcSave.bottom;
                SetTextColor (hdcPrint, RGB (0, 0xFF, 0xFF)); // CYAN
                break;

            case 10:           //  Prepare for 11th string: Under string rotate 360: at right
                rc.left = rcSave.right / 2 + 100;
                rc.top = rcSave.bottom / 2 + nRet360;
                rc.right = rcSave.right;
                rc.bottom = rcSave.bottom;
                SetTextColor (hdcPrint, RGB (0xFF, 0x0, 0xFF)); // Magenta
                break;

            case 11:           // Prepare for 12th string:  Europen characters
                rc.left = 0;
                rc.top = rcSave.bottom * 7 / 9;
                rc.right = rcSave.right;
                rc.bottom = rcSave.bottom;
                break;

            case 12:           // Prepare for 13th: Last string:  BOTTOM
                SetBkMode (hdcPrint, OPAQUE);
                SetBkColor (hdcPrint, RGB (0xDF, 0xDF, 0xDF));
                // Fall Through

            default:
                rc.top += nRet;
        }

    }

    if (hfont)
        DeleteObject (hfont);
    return nRetCode;
}


//=======================================
// Rotate more:
//=======================================
int PASCAL DrawPageFonts2 (HDC hdcPrint, int iDoc, int iPage, int nCallAbort)
{
    int iLoop, index, width, height, angle, nStartSize = 8, offset = 200, nRet;
    HFONT hfont = NULL, hfontPrev;
    int nRetCode = 0, nDPI;
    LOGFONT lf;
    RECT rc;
    TCHAR szRotate[100] = TEXT ("abdfghjklmpqstwyz");
    TCHAR *rgFontName[2] = { TEXT ("Times New Roman"), TEXT ("Tahoma") };
    LPBYTE lpByte;

    LogResult (1, TEXT ("------------------ DrawPageFonts2() Starts!  iDoc=%d  iPage=%d"), iDoc + 1, iPage + 1);

    SetBkMode (hdcPrint, OPAQUE);
    width = g_rcPrint.right / 2 + 250;
    height = g_rcPrint.bottom / 4 + 250;
    nDPI = GetDeviceCaps (hdcPrint, LOGPIXELSY);

    for (iLoop = 0; iLoop < 2; iLoop++)
    {
        rc = g_rcPrint;
        rc.bottom /= 2;
        if (iLoop == 0)         // using Times New Roman: which link to test1.tte
        {
            SetViewportOrgEx (hdcPrint, g_rcPrint.right * 3 / 10, g_rcPrint.bottom / 7, NULL);
            SetBkColor (hdcPrint, RGB (0x99, 0xBB, 0xDD));
            SetTextColor (hdcPrint, RGB (0, 0, 0)); // BLACK TEXT

            lpByte = (LPBYTE) (&szRotate[17]);
            *lpByte++ = 0x00;   // first char in test1.tte
            *lpByte++ = 0xE0;
            *lpByte++ = 0x00;   // test1.tte only contains 1 EUDC char at 0xE000.
            *lpByte++ = 0xE0;

            *lpByte++ = 0x00;   // set NULL terminator
            *lpByte++ = 0x00;

            nStartSize = 8;
            offset = 200;
        }
        else
        {
            switch (GetSystemDefaultLCID ())
            {
                case 0x411:    // Japan
                    // use Japanese Chars
                    nRet = LoadString (g_hInst, IDS_String_2_JPN, szRotate, 25);
                    LogResult (DEBUG_OUTPUT, TEXT ("Fonts2: Load Japanese string return %d chars"), nRet);
                    break;

                case 0x409:    // US

                default:
                    // use Japanese Chars: using Tahoma: which link to test3.tte
                    nRet = LoadString (g_hInst, IDS_String_2, szRotate, 25);
                    LogResult (DEBUG_OUTPUT, TEXT ("Fonts2: Load Japanese string return %d chars"), nRet);
                    break;
            }
            lpByte = (LPBYTE) (&szRotate[9]);
            *lpByte++ = 0x00;   // first char in test3.tte
            *lpByte++ = 0xE0;

            *lpByte++ = 0x57;   // last char in test3.tte
            *lpByte++ = 0xE7;

            SetViewportOrgEx (hdcPrint, width, g_rcPrint.bottom * 5 / 8, NULL);
            SetBkColor (hdcPrint, RGB (0xDD, 0xEE, 0xFF));
            SetTextColor (hdcPrint, RGB (0xFF, 0, 0)); // RED      TEXT
            nStartSize = 9;
            offset = 168;
        }

        memset ((LPVOID) & lf, 0, sizeof (LOGFONT));
        _tcscpy (lf.lfFaceName, rgFontName[iLoop]);


        for (angle = 0, index = 0; angle < 3600; angle += offset, index++)
        {
            if (iLoop == 0)
            {
                lf.lfHeight = (nStartSize + index * 2) * nDPI / 96;
                lf.lfWeight = FW_BOLD;
            }
            else
            {
                lf.lfHeight = (nStartSize + index * 3 / 2) * nDPI / 96;
            }

            lf.lfEscapement = angle;
            lf.lfOrientation = angle;

            hfont = CreateFontIndirect (&lf);
            hfontPrev = (HFONT) SelectObject (hdcPrint, hfont);
            if (angle <= 900)
            {
                rc.bottom = -height;
            }
            else if (angle > 900 && angle <= 1800)
            {
                rc.right = -width;
                rc.bottom = -height;
            }
            else if (angle > 1800 && angle <= 2700)
            {
                rc.right = -width;
                rc.bottom = height;
            }
            else
            {
                rc.right = width;
                rc.bottom = height;
            }
            // if using DT_BOTTOM: NT shows nothing.
            DrawText (hdcPrint, szRotate, -1, &rc, DT_SINGLELINE);
            SelectObject (hdcPrint, hfontPrev);
            DeleteObject (hfont);
        }
    }

    return nRetCode;
}

//==============================================
// Full page of text
//==============================================
int PASCAL DrawPageFullText (HDC hdcPrint, int iDoc, int iPage, int nCallAbort)
{
#define   MAX_TEXT_LENGTH  1024
    int i, j, nRet, nTests, nDPI;
    HFONT hfont = NULL, hfontPrev;

    int nRetCode = 0;
    UINT uFormat;
    LOGFONT lf;
    RECT rc;
    TEXTMETRIC tm;
    UINT size;
    TCHAR szText[MAX_TEXT_LENGTH + 10], szFace[64];
    BYTE szTest[sizeof(szText)];

    LogResult (1, TEXT ("------------------ DrawPageFullText() Starts!  iDoc=%d  iPage=%d"), iDoc + 1, iPage + 1);

#ifdef UNDER_CE
    EnableEUDC (TRUE);
#endif
    rc = g_rcPrint;

    // select clip region first:  done in caller routine
    // SetViewportOrgEx() second
    SetViewportOrgEx (hdcPrint, rc.right / 2, rc.bottom / 2, NULL);
    rc.left = -rc.right / 2;
    rc.top = -rc.bottom / 2;
    rc.right = rc.right / 2;
    rc.bottom = rc.bottom / 2;

    nDPI = GetDeviceCaps (hdcPrint, LOGPIXELSY);
    nTests = 5;
    memset ((LPVOID) & lf, 0, sizeof (LOGFONT));

    switch (GetSystemDefaultLCID ())
    {
        case 0x411:            // Japan
            lf.lfCharSet = SHIFTJIS_CHARSET;

            switch (iPage)
            {
                case 0:
                    _tcscpy (lf.lfFaceName, TEXT ("‚l‚r ‚oƒSƒVƒbƒN"));
                    break;
                case 1:
                    _tcscpy (lf.lfFaceName, TEXT ("MS Gothic"));
                    break;
            }
            break;

        case 0x409:            // US

        default:
            switch (iPage)
            {
                case 0:
                    _tcscpy (lf.lfFaceName, TEXT ("Times New Roman"));
                    break;
                case 1:
                    _tcscpy (lf.lfFaceName, TEXT ("Tahoma"));
                    break;
                case 2:
                    _tcscpy (lf.lfFaceName, TEXT ("Arial"));
                    break;
                case 3:
                    _tcscpy (lf.lfFaceName, TEXT ("Courier New"));
                    break;
                case 4:
                    _tcscpy (lf.lfFaceName, TEXT ("MS Sans Serif"));
                    lf.lfHeight = 0;
                    lf.lfWeight = FW_BOLD;
                    break;
            }
            break;
    }


    szTest[0] = 0x01;
    szTest[1] = 0xE0;
    szTest[2] = 0x00;
    szTest[3] = 0xE0;

    SetBkMode (hdcPrint, OPAQUE);
    for (i = 0; i < nTests; i++) // total 6 string: each draw 3 times
    {
        szTest[4] = 0;
        szTest[5] = 0;

        memset ((LPVOID) szText, 0, sizeof (szText));
        
        switch (GetSystemDefaultLCID ())
        {
            case 0x411:        // Japan
                nRet = LoadString (g_hInst, IDS_String_FIRST_JPN + (i % 6), szText, MAX_TEXT_LENGTH);
                break;

            case 0x409:        // US

            default:
                nRet = LoadString (g_hInst, IDS_String_FIRST + (i % 6), szText, MAX_TEXT_LENGTH);
                break;
        }

        szText[nRet] = 0;

        LogResult (DEBUG_OUTPUT, TEXT ("LoadString(i=%d) return %d: <%s>"), i, nRet, (LPTSTR) szText);
        
        // append an EUDC font at the front
        _tcscat ((LPTSTR) & (szTest[4]), szText);

        size = 10 + i * 4;
        lf.lfHeight = size * nDPI / 96;
        for (j = 0; j < 3; j++)
        {
            uFormat = DT_WORDBREAK | DT_EXPANDTABS;
            SetBkColor (hdcPrint, RGB (0xFF, 0xFF, 0xFF));
            SetTextColor (hdcPrint, RGB (0, 0, 0));

            switch (j)
            {
                case 0:
                    uFormat |= DT_LEFT;
                    lf.lfQuality = DRAFT_QUALITY; // == 0
                    lf.lfItalic = TRUE;
                    lf.lfWeight = 300;
                    lf.lfUnderline = FALSE;
                    break;
                case 1:
                    lf.lfQuality = 2; // == PROOF_QUALITY: CE Won't support
                    uFormat |= DT_CENTER;
                    lf.lfItalic = FALSE;
                    lf.lfWeight = 900;
                    lf.lfUnderline = FALSE;

                    if (i == 3)
                    {
                        SetBkColor (hdcPrint, RGB (0, 0, 0));
                        SetTextColor (hdcPrint, RGB (0xFF, 0xFF, 0xFF));
                    }
                    break;
                case 2:
                    lf.lfQuality = ANTIALIASED_QUALITY; //== 4
                    uFormat |= DT_RIGHT;
                    lf.lfItalic = FALSE;
                    lf.lfWeight = 400;
                    lf.lfUnderline = TRUE;

                    lf.lfHeight = -lf.lfHeight; // Change height to negative
                    break;
            }
            
            hfont = CreateFontIndirect (&lf);
            hfontPrev = (HFONT) SelectObject (hdcPrint, hfont);

            GetTextFace (hdcPrint, 64, szFace);
            GetTextMetrics (hdcPrint, &tm);
            LogResult (DEBUG_OUTPUT,
                TEXT("FullText: Face=%s: want Ht=%d  get Ht=%d  Bold=%d"), (LPTSTR) szFace, lf.lfHeight, tm.tmHeight, tm.tmWeight);

            nRet = DrawText (hdcPrint, (LPTSTR) szTest, -1, &rc, uFormat);
            hfont = (HFONT) SelectObject (hdcPrint, hfontPrev);
            DeleteObject (hfont);
            hfont = NULL;
            rc.top += nRet;
        }

        // test abort doc
        if (iPage == 1 && i == 2 && nCallAbort)
        {
            nRet = AbortDoc (hdcPrint);
            if (nRet <= 0)
            {
                nRetCode = GetLastError ();
                LogResult (1, TEXT ("AbortDoc(hdcPrint) FAIL: nRet=%d:  err=%ld\r\n"), nRet, nRetCode);
            }
            g_fUserAbort = TRUE;
            goto LRet;
        }

    }

  LRet:
    return nRetCode;
}


int PASCAL DrawPageBrushes (HDC hdcPrint, int iDoc, int iPage, int nCallAbort)
{
    int FIXED_WIDTH = 700;      // in 300 dpi mode: for first 2 polygon figures
    int ROW_HEIGHT = 420;
    int RECTANGLE_WIDTH = 300;
    int ELLIPSE_WIDTH = 350;
    int xMax = 700;
    POINT rgTriangleSave[5];
    POINT rgConcaveSave[5];

    int i, index, iRow, iPen, nTests, xOrg, yOrg, bmWidth, bmHeight;
    HDC hdcMem;
    HBITMAP hbm = NULL;
    HPEN hpen = NULL, hpenPrev;
    HBRUSH hbrush = NULL, hbrushPrev;
    int nRetCode = 0, offset;
    HFONT hfontPrev;
    DWORD dwError;
    RECT rc, rcT;
    POINT rgTriangle[5];
    POINT rgConcave[5];
    TCHAR szError[128];
    BITMAP bmp;

    LogResult (1, TEXT ("------------------ DrawPageBrushes() Starts!  iDoc=%d  iPage=%d"), iDoc + 1, iPage + 1);

    rc = g_rcPrint;
    SetLastError (0);
    SetViewportOrgEx (hdcPrint, 0, 0, NULL);
    hdcMem = CreateCompatibleDC (hdcPrint);
    if (!hdcMem)
    {
        dwError = GetLastError ();
        LogResult (DEBUG_OUTPUT, TEXT ("DrawPageBrushes(): CreateCompatibleDC() failed: err=%ld"), dwError);
        return dwError;
    }
    hfontPrev = (HFONT) SelectObject (hdcPrint, g_hfont);

    wsprintf (szError, TEXT ("BRUSHES: Page %d: ROP=%s"), iPage + 1, (LPTSTR) grgdwRop2[iPage].ropName);
    LogResult (DEBUG_OUTPUT, szError);
    rcT.left = 0;
    rcT.top = 0;
    rcT.right = 2400;
    rcT.bottom = 40;
    offset = DrawText (hdcPrint, szError, -1, &rcT, DT_WORD_ELLIPSIS);
    offset *= 2;

    SetLastError (0);
    if (!SetROP2 (hdcPrint, grgdwRop2[iPage].dwRop))
    {
        LogResult (DEBUG_OUTPUT, TEXT ("SetROP2(0x%lx = %s) fail: err=%ld)"),
            grgdwRop2[iPage].dwRop, (LPTSTR) grgdwRop2[iPage].ropName, GetLastError ());
    }

    if (g_fDraftMode)           // 150 dpi
    {
        FIXED_WIDTH /= 2;       // in 300 dpi mode: for first 2 polygon figures
        ROW_HEIGHT /= 2;
        RECTANGLE_WIDTH /= 2;
        ELLIPSE_WIDTH /= 2;
        xMax /= 2;

        memcpy (rgTriangleSave, grgTriangleSave, sizeof (grgTriangleSave));
        memcpy (rgConcaveSave, grgConcaveSave, sizeof (grgConcaveSave));

        for (i = 0; i < 5; i++)
        {
            rgTriangleSave[i].x /= 2;
            rgTriangleSave[i].y /= 2;

            rgConcaveSave[i].x /= 2;
            rgConcaveSave[i].y /= 2;
        }
    }

    nTests = 11;
    for (iRow = 0, index = 0; index < nTests; index++, iRow++)
    {
        // Calculate first: then index == 8: rcFill can use the value
        rcT.left = FIXED_WIDTH;
        rcT.top = iRow * ROW_HEIGHT + offset;
        rcT.right = rcT.left + RECTANGLE_WIDTH;
        rcT.bottom = rcT.top + RECTANGLE_WIDTH;

        iPen = index % MAX_PENS;
        hpen = CreatePen (rgPenInfo[iPen].nStyle, rgPenInfo[iPen].nWidth, rgPenInfo[iPen].color);

        SetLastError (0);
        if (index <= 5)
        {
            hbm = LoadBitmap (g_hInst, szBitmap[index]);
            if (!hbm)
            {
                nRetCode = GetLastError ();
                LogResult (DEBUG_OUTPUT, TEXT ("LoadBitmap(%s) Failed: error=%ld"), (LPTSTR) szBitmap[index], nRetCode);
                goto LRelease;
            }

            GetObject (hbm, sizeof (BITMAP), &bmp);
            bmWidth = bmp.bmWidth;
            bmHeight = bmp.bmHeight;
            SetLastError (0);
            hbrush = CreatePatternBrush (hbm);
        }
        else
        {
            hbm = NULL;
            bmWidth = 0;
            bmHeight = 0;
            SetLastError (0);
            switch (index)
            {
                case 6:
                    hbrush = (HBRUSH) GetStockObject (NULL_BRUSH);
                    hpen = CreatePen (PS_SOLID, 2, RGB (0xAA, 0xAA, 0));
                    break;

                case 7:
                    hbrush = (HBRUSH) GetStockObject (BLACK_BRUSH);
                    hpen = CreatePen (PS_SOLID, 15, RGB (0xFF, 0xFF, 0xFF));
                    break;
                case 8:
                    hbrush = (HBRUSH) GetStockObject (WHITE_BRUSH);
                    hpen = CreatePen (PS_SOLID, 15, RGB (0, 0, 0xFF));
                    break;
                case 9:
                    hbrush = (HBRUSH) GetStockObject (LTGRAY_BRUSH);
                    hpen = CreatePen (PS_SOLID, 15, RGB (0xFF, 0, 0));
                    break;
                case 10:
                    hbrush = (HBRUSH) GetStockObject (DKGRAY_BRUSH);
                    hpen = CreatePen (PS_SOLID, 15, RGB (0, 0xFF, 0));
            }
        }

        if (!hbrush || !hpen)
        {
            nRetCode = GetLastError ();
            if (nRetCode == ERROR_OUTOFMEMORY || nRetCode == ERROR_NOT_ENOUGH_MEMORY)
            {
                TCHAR szError[128];

                wsprintf (szError, 
                    TEXT("index=%d: Brush(0x%lX) or Pen(0x%lX) failed: error=%ld: Ou of memory!!!"), index, hbrush, hpen, nRetCode);
                LogResult (DEBUG_OUTPUT, szError);

                rcT.left = 0;
                rcT.top = iRow * ROW_HEIGHT + offset;
                rcT.right = 24000;
                rcT.bottom = rcT.top + RECTANGLE_WIDTH;
                DrawText (hdcPrint, szError, -1, &rcT, DT_WORD_ELLIPSIS);
                goto LAdjustForNextDraw;
            }
            else
            {
                LogResult (DEBUG_OUTPUT,
                    TEXT ("index=%d:  Create Brush (0x%lX) or Pen(0x%lX) failed: error=%ld"), index, hbrush, hpen, nRetCode);
                goto LRelease;
            }
        }

        for (i = 0; i < 5; i++)
        {
            rgTriangle[i].x = rgTriangleSave[i].x;
            rgTriangle[i].y = rgTriangleSave[i].y;

            rgConcave[i].x = rgConcaveSave[i].x;
            rgConcave[i].y = rgConcaveSave[i].y;

            // only adjust y position
            rgTriangle[i].y += (iRow * ROW_HEIGHT) + offset;
            rgConcave[i].y += (iRow * ROW_HEIGHT) + offset;
        }


        if (index == 7)
        {
            RECT rcFill;

            rcFill.left = 0;
            rcFill.top = rcT.top - 20;
            rcFill.right = rcT.right + 50 + ELLIPSE_WIDTH; // for ellipse
            rcFill.bottom = rcT.bottom + 50;
            FillRect (hdcPrint, &rcFill, (HBRUSH) GetStockObject (DKGRAY_BRUSH));
        }
        // paint the rectangle first even it's position is after triangle:  need to test SetBrushOrgEx()
        // MGDI: Must call SetBrushOrgEx() first, then Call SelectObject(hbrush)
        if (hbm)
        {
            xOrg = rcT.left % bmWidth;
            yOrg = rcT.top % bmHeight;
            SetBrushOrgEx (hdcPrint, xOrg, yOrg, NULL);
        }

        hpenPrev = (HPEN) SelectObject (hdcPrint, hpen);
        hbrushPrev = (HBRUSH) SelectObject (hdcPrint, hbrush);

        Rectangle (hdcPrint, rcT.left, rcT.top, rcT.right, rcT.bottom);
        rcT.left += ELLIPSE_WIDTH + 25;
        rcT.right += ELLIPSE_WIDTH + 25;
        Ellipse (hdcPrint, rcT.left, rcT.top + 25, rcT.right, rcT.bottom - 25);

        Polygon (hdcPrint, rgTriangle, 4);
        Polygon (hdcPrint, rgConcave, 5);

        SelectObject (hdcPrint, hpenPrev);
        SelectObject (hdcPrint, hbrushPrev);
        DeleteObject (hpen);
        if (!DeleteObject (hbrush))
            LogResult (DEBUG_OUTPUT, TEXT ("Brushes: Delete hbrush failed: err=%ld"), GetLastError ());

      LAdjustForNextDraw:
        if ((rcT.bottom + ROW_HEIGHT > g_rcPrint.bottom) && (index + 1 < nTests))
        {
            LogResult (DEBUG_OUTPUT, TEXT ("DrawPageBrushes(): CallEndAndStartNewPage()"));
            if (!CallEndAndStartNewPage (hdcPrint, &dwError))
            {
                if (g_fUserAbort)
                    LogResult (1, TEXT ("User aborted printing."));
                else
                    nRetCode = dwError;
                goto LRelease;
            }

            if (!SetROP2 (hdcPrint, grgdwRop2[iPage].dwRop))
            {
                LogResult (DEBUG_OUTPUT,
                    TEXT ("SetROP2(0x%lx = %s) fail: err=%ld)"),
                    grgdwRop2[iPage].dwRop, (LPTSTR) grgdwRop2[iPage].ropName, GetLastError ());
            }

            iRow = -1;          // for loop will update it to 0
            hfontPrev = (HFONT) SelectObject (hdcPrint, g_hfont);
        }
    }                           // for (iRow=0, index = 0....)


  LRelease:
    if (!DeleteObject (hdcMem))
    {
        LogResult (DEBUG_OUTPUT, TEXT ("Brushes: Delete hdcMem failed: err=%ld"), GetLastError ());
    }

    DeleteObject (hpen);
    DeleteObject (hbrush);
    DeleteObject (hbm);

    SelectObject (hdcPrint, hfontPrev);
    return nRetCode;
}

//======================================
// show the 8, 16, 24 bit bitmap
//======================================
int PASCAL DrawPageBitmap2 (HDC hdcPrint, int iDoc, int iPage, int nCallAbort)
{
    HDC hdcMem;
    HBITMAP hbm, hbmPrev;
    BITMAP bmp;
    int i, nRetCode = 0, xDest = 0, yDest = 0, fRet, x, y;
    HFONT hfontPrev;
    RECT rcTmp, rcGet;
    POINT points[2];
    TCHAR szError[128];
    HRGN hrgn;
    DWORD dwError;

    LogResult (1, TEXT ("------------------ DrawPageBitMap2() Starts!  iDoc=%d  iPage=%d"), iDoc + 1, iPage + 1);

    hdcMem = CreateCompatibleDC (hdcPrint);
    if (!hdcMem)
    {
        dwError = GetLastError ();
        LogResult (DEBUG_OUTPUT, TEXT ("DrawPageBitmap2(): CreateCompatibleDC() failed: err=%ld"), dwError);
        return dwError;
    }

    SetLastError (0);
    if (iDoc <= 2)              // doc 0, doc 1, doc2
    {
        x = g_rcPrint.right / 2;
        y = g_rcPrint.bottom / 2;
        SetViewportOrgEx (hdcPrint, x, y, &points[0]);
        GetClipBox (hdcPrint, &rcTmp);
        LogResult (DEBUG_OUTPUT,
            TEXT ("After SetViewport(%d %d): rcClipGet=[%d %d %d %d]"), x, y, rcTmp.left, rcTmp.top, rcTmp.right, rcTmp.bottom);

        points[0].x = 0;
        points[0].y = -y;
        points[1].x = 0;
        points[1].y = y;
        Polyline (hdcPrint, points, 2); // vertical line

        points[0].x = -x;
        points[0].y = 0;
        points[1].x = x;
        points[1].y = 0;
        Polyline (hdcPrint, points, 2); // horizontal line

        IntersectClipRect (hdcPrint, -3000, -3000, 3000, 3000);
        GetClipBox (hdcPrint, &rcGet);
        LogResult (DEBUG_OUTPUT,
            TEXT ("1. After InterClipRect: rcGet=[%d %d %d %d]"), rcGet.left, rcGet.top, rcGet.right, rcGet.bottom);

        if (memcmp (&rcTmp, &rcGet, sizeof (RECT)))
        {
            LogResult (1,
                TEXT
                ("After InterClipRect: rcGet=[%d %d %d %d]  != rc Original [%d %d %d %d]"),
                rcGet.left, rcGet.top, rcGet.right, rcGet.bottom, rcTmp.left, rcTmp.top, rcTmp.right, rcTmp.bottom);
        }

        if (iDoc == 0)
        {
            rcTmp.left = x / 2;
            rcTmp.top = y / 2;
            rcTmp.right = g_rcPrint.right - x / 2;
            rcTmp.bottom = g_rcPrint.bottom - y / 2;

            LogResult (DEBUG_OUTPUT,
                TEXT ("Clip Region: rcTmp=[%d %d %d %d]"), rcTmp.left, rcTmp.top, rcTmp.right, rcTmp.bottom);

            // select clipregion into DC
            hrgn = CreateRectRgn (rcTmp.left, rcTmp.top, rcTmp.right, rcTmp.bottom);
            fRet = SelectClipRgn (hdcPrint, hrgn);
            if (fRet == ERROR)
            {
                LogResult (1, TEXT ("iDoc=0: SelectCliprgn() failed:  err=%ld"), GetLastError ());
            }
            DeleteObject (hrgn);
        }
        else if (iDoc == 1)
        {
            rcTmp.left = -x / 4 * 3;
            rcTmp.top = -y / 4 * 3;
            rcTmp.right = x / 4 * 3;
            rcTmp.bottom = y / 4 * 3;
            IntersectClipRect (hdcPrint, rcTmp.left, rcTmp.top, rcTmp.right, rcTmp.bottom);
        }
        // Verify
        GetClipBox (hdcPrint, &rcGet);
        LogResult (DEBUG_OUTPUT,
            TEXT ("2. After set clip regsion: rcGet=[%d %d %d %d]"), rcGet.left, rcGet.top, rcGet.right, rcGet.bottom);

        if ((iDoc == 1) && memcmp (&rcTmp, &rcGet, sizeof (RECT)))
        {
            LogResult (DEBUG_OUTPUT,
                TEXT
                ("After set clip regsion: rcGet=[%d %d %d %d]  != rcSet [%d %d %d %d]"),
                rcGet.left, rcGet.top, rcGet.right, rcGet.bottom, rcTmp.left, rcTmp.top, rcTmp.right, rcTmp.bottom);
        }
    }
    else
        SetViewportOrgEx (hdcPrint, 0, 0, NULL);

    // Only  Load the one Large bitmap:  16 bits, 24 bits  and 8 bits
    //     5: is 16biti4.bmp  16 bits;   6: is bently.bmp:  24 bits:  6: cmdBar2.bmp 8 bits
    hfontPrev = (HFONT) SelectObject (hdcPrint, g_hfont);

    xDest = 0;
    yDest = 0;

    for (i = 0; i < 4; i++)
    {
        if (i == 3)             // load cmdbar2. again
            hbm = LoadBitmap (g_hInst, szBitmap[BITMAP_8_BITS]);
        else
            hbm = LoadBitmap (g_hInst, szBitmap[i + BITMAP_16_BITS]);

        if (!hbm)
        {
            nRetCode = GetLastError ();
            LogResult (1, TEXT ("i=%d: LoadBitmap(%s) Failed: error=%ld"), i, (LPTSTR) szBitmap[i + BITMAP_16_BITS], nRetCode);
            continue;
        }
        GetObject (hbm, sizeof (BITMAP), &bmp);
        if ((hbmPrev = (HBITMAP) SelectObject (hdcMem, hbm)) == NULL)
        {
            LogResult (1, TEXT ("i=%d: SelectObject(hbm=0x%lX) Failed: error=%ld"), i, hbm, GetLastError ());
        }

        rcTmp.left = xDest;
        rcTmp.top = yDest;
        rcTmp.right = xDest + bmp.bmWidth;
        rcTmp.bottom = yDest + bmp.bmHeight;

        if (iDoc <= 2)
        {
            int nDWidth = 0, nDHeight = 0;

            switch (i)
            {
                case 0:        // 16 bits:
                    nDWidth = -(bmp.bmWidth * 3 / 2); // mirror image: along Y axis
                    nDHeight = bmp.bmHeight * 3 / 2;
#ifdef UNDER_NT
                    xDest = 0;
                    yDest = 0;
#else
                    xDest = -40;
                    yDest = 40;
#endif // UNDER_CE
                    break;
                case 1:        // 24 bits
                    nDWidth = bmp.bmWidth * 3 / 2;; // mirror image: along X axis
                    nDHeight = -(bmp.bmHeight * 3 / 2);

#ifdef UNDER_NT
                    xDest = 0;
                    yDest = 0;
#else
                    xDest = 40;
                    yDest = -40;
#endif
                    break;
                case 2:
                    nDWidth = -(bmp.bmWidth * 3 / 2); // mirror image: along X and Y axis
                    nDHeight = -(bmp.bmHeight * 3 / 2);

#ifdef UNDER_NT
                    xDest = 0;
                    yDest = 0;
#else
                    xDest = -40;
                    yDest = -40;
#endif
                    break;

                case 3:
                    nDWidth = (bmp.bmWidth * 3 / 2); // mirror image: along X and Y axis
                    nDHeight = (bmp.bmHeight * 3 / 2);
#ifdef UNDER_NT
                    xDest = 0;
                    yDest = 0;
#else
                    xDest = 40;
                    yDest = 40;
#endif
                    break;
            }

            if (i == 3)
            {
                fRet = StretchBlt (hdcPrint,
                    xDest, yDest, nDWidth, nDHeight, hdcMem, 0, 0, -bmp.bmWidth, -bmp.bmHeight, grgdwRop[iDoc % 2].dwRop);
            }
            else
            {
                fRet = StretchBlt (hdcPrint,
                    xDest, yDest, nDWidth, nDHeight, hdcMem, 0, 0, bmp.bmWidth, bmp.bmHeight, grgdwRop[iDoc % 2].dwRop);
            }

            if (!fRet)
            {
                wsprintf (szError, TEXT ("i=%d Bitmap3: SBlt: err=%ld"), i, GetLastError ());
                DrawText (hdcPrint, szError, -1, &rcTmp, DT_WORD_ELLIPSIS);
                LogResult (1, szError);
            }
        }
        else
        {
            if (!BitBlt (hdcPrint, xDest, yDest, bmp.bmWidth, bmp.bmHeight, hdcMem, 0, 0, grgdwRop[iDoc % 2].dwRop))
            {
                wsprintf (szError, TEXT ("Bitmap3: BBlt %d: err=%ld"), i, GetLastError ());
                DrawText (hdcPrint, szError, -1, &rcTmp, DT_WORD_ELLIPSIS);
                LogResult (DEBUG_OUTPUT, szError);
            }

            xDest += bmp.bmWidth + 20;
        }

        SelectObject (hdcMem, hbmPrev);
        if (!DeleteObject (hbm))
        {
            LogResult (1, TEXT ("Bitmap1:  Delete hbm failed: err=%ld"), GetLastError ());
        }
    }

    if (!DeleteObject (hdcMem))
    {
        LogResult (1, TEXT ("Bitmap1:  Delete hddcMem failed: err=%ld"), GetLastError ());
    }
    SelectObject (hdcPrint, hfontPrev);
    return nRetCode;
}


int PASCAL DrawPageTransparentI (HDC hdcPrint, int iDoc, int iPage, int nCallAbort)
{
#define  WIDTH  300
    HDC hdcMem;
    HBITMAP hbm, hbmPrev;
    int i, nObjects, nWidth = 40;
    int nRetCode = 0, yDest = 0, xDest = 0;
    int DEST_WIDTH;
    DWORD dwError;
    RECT rc, rcMem;
    HFONT hfontPrev;
    HBRUSH hbrush;
    COLORREF colorTrans;
    TCHAR sz[128];
    int rgBits[MAX_TEST_BITS] = { 1, 2, 4, 8, 16, 24, 32 };


    LogResult (1, TEXT ("------------------ DrawPageTransparentI() Starts!  iDoc=%d  iPage=%d"), iDoc + 1, iPage + 1);

    SetLastError (0);
    SetViewportOrgEx (hdcPrint, 0, 0, NULL);
    hdcMem = CreateCompatibleDC (hdcPrint);
    if (!hdcMem)
    {
        dwError = GetLastError ();
        LogResult (DEBUG_OUTPUT, TEXT ("DrawPageTransparentI(): CreateCompatibleDC() failed: err=%ld"), dwError);
        return dwError;
    }

    if (g_fDraftMode)
        nObjects = 4;
    else
        nObjects = MAX_TEST_BITS;
    hfontPrev = (HFONT) SelectObject (hdcPrint, g_hfont);

    DEST_WIDTH = (iPage % 2) ? WIDTH * 2 : WIDTH;

    for (i = 0; i < nObjects; i++)
    {
        rc.left = xDest;
        rc.top = yDest;
        rc.right = xDest + DEST_WIDTH;
        rc.bottom = yDest + DEST_WIDTH;
        FillRect (hdcPrint, &rc, (HBRUSH) GetStockObject (WHITE_BRUSH));

        wsprintf (sz, TEXT ("Obj=%d: bits=%d:"), i, rgBits[i]);
        rc.left += nWidth * 2 + 5;
        rc.top += nWidth * 2 + 5;
        rc.right -= nWidth * 2;
        rc.bottom -= nWidth * 2;
        DrawText (hdcPrint, sz, -1, &rc, DT_WORDBREAK);

        hbm = CreateBitmap (WIDTH, WIDTH, 1, rgBits[i], NULL);
        hbmPrev = (HBITMAP) SelectObject (hdcMem, hbm);

        rcMem.left = 0;
        rcMem.top = 0;
        rcMem.right = WIDTH;
        rcMem.bottom = WIDTH;
        FillRect (hdcMem, &rcMem, (HBRUSH) GetStockObject (LTGRAY_BRUSH));

        hbrush = CreateSolidBrush (RGB (0xFF, 0, 0)); // RED
        rcMem.left += nWidth;
        rcMem.top += nWidth;
        rcMem.right -= nWidth;
        rcMem.bottom -= nWidth;
        FillRect (hdcMem, &rcMem, hbrush);
        DeleteObject (hbrush);


        // middle of the memory DC: fill the transparent color
        colorTrans = RGB (0x55, 0xFF, 0xFF); //     TransColor
        rcMem.left += nWidth;
        rcMem.top += nWidth;
        rcMem.right -= nWidth;
        rcMem.bottom -= nWidth;
        hbrush = CreateSolidBrush (colorTrans);
        FillRect (hdcMem, &rcMem, hbrush);
        DeleteObject (hbrush);

#ifdef UNDER_CE
        TransparentImage (hdcPrint, xDest, yDest, DEST_WIDTH, DEST_WIDTH, hdcMem, 0, 0, WIDTH, WIDTH, colorTrans);
#endif

        xDest += DEST_WIDTH + 25;
        if ((xDest + DEST_WIDTH) > g_rcPrint.right)
        {
            xDest = 0;
            yDest += DEST_WIDTH + 25;

            if ((yDest + DEST_WIDTH) > g_rcPrint.bottom)
            {
                LogResult (DEBUG_OUTPUT, TEXT ("DrawPageTI(): CallEndAndStartNewPage()"));
                if (!CallEndAndStartNewPage (hdcPrint, &dwError))
                {
                    SelectObject (hdcMem, hbmPrev);
                    DeleteObject (hbm);
                    if (g_fUserAbort)
                        LogResult (1, TEXT ("User aborted printing."));
                    else
                        nRetCode = dwError;
                    goto LRelease;
                }
                hfontPrev = (HFONT) SelectObject (hdcPrint, g_hfont);
                yDest = 0;
            }
        }
        SelectObject (hdcMem, hbmPrev);
        DeleteObject (hbm);
    }

  LRelease:
    if (hdcMem)
        DeleteObject (hdcMem);
    SelectObject (hdcPrint, hfontPrev);
    return nRetCode;
}


int PASCAL DrawPagePens (HDC hdcPrint, int iDoc, int iPage, int nCallAbort)
{
    HDC hdcMem;
    HBITMAP hbm, hbmOld;
    int iLoop, nLoop, iRow, index, x = 0, y = 0, nWidth = 150, nHeight = 150;
    int nRetCode = 0, nBits;
    RECT rc, rcTmp;
    HPEN hpen, hpenOld, hpenStock;
    HBRUSH hbrush, hbrushOld;
    DWORD dwRop;
    float fMul = (float)1.1;
    TCHAR sz[32];
    int rgBits[MAX_TEST_BITS] = { 32, 24, 1, 16, 4, 2, 8 };
    PENINFO rgpInfo[3] = {
        {PS_SOLID, 35, RGB (0, 0, 0xFF)},
        {PS_SOLID, 105, RGB (0, 0xFF, 0)},
        {PS_DASH, 1, RGB (0xFF, 0, 0)},
    };

    LogResult (1, TEXT ("------------------ DrawPagePens() Starts!  iDoc=%d  iPage=%d"), iDoc + 1, iPage + 1);
    rc = g_rcPrint;

    nLoop = MAX_TEST_BITS;
    hdcMem = CreateCompatibleDC (hdcPrint);
    rcTmp.left = rcTmp.top = 0;

    for (iLoop = 0, iRow = 0; iLoop < nLoop; iLoop++, iRow++)
    {
        nWidth += (int)(iRow * 3.5);
        rcTmp.right = nWidth;
        rcTmp.bottom = nHeight;

        nBits = rgBits[iLoop];
        wsprintf (sz, TEXT ("b = %d"), nBits);

        hbm = CreateBitmap (nWidth, nHeight, 1, nBits, NULL);
        hbmOld = (HBITMAP) SelectObject (hdcMem, hbm);

        if (iPage % 2)
        {
            hbrush = CreateSolidBrush (RGB (0xAA, 0xCC, 0xEE));
        }
        else
        {
            hbrush = (HBRUSH) GetStockObject (WHITE_BRUSH);
        }

        hbrushOld = (HBRUSH) SelectObject (hdcMem, hbrush);

        dwRop = SRCCOPY;
        for (index = 0; index < 3; index++)
        {
            dwRop = SRCCOPY;
            hpen = CreatePen (rgpInfo[index].nStyle, rgpInfo[index].nWidth, rgpInfo[index].color);

            hpenStock = (HPEN) SelectObject (hdcMem, hpen);
            FillRect (hdcMem, &rcTmp, (HBRUSH) GetStockObject (WHITE_BRUSH));

            RoundRect (hdcMem, 0, 0, nWidth, nHeight, 40 + index * 10, 25 + index * 10);
            DrawText (hdcMem, sz, -1, &rcTmp, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            LogResult (DEBUG_OUTPUT,
                TEXT ("DrawPen: iRow=%d: index=%d  nWidth=%d  strech width=%ld"), iRow, index, nWidth, (int)(nWidth * fMul));
            StretchBlt (hdcPrint, x, y, (int)(nWidth * fMul), (int)(nHeight * fMul), hdcMem, 0, 0, nWidth, nHeight, dwRop);
            x += (int)(nWidth * fMul + 20);


            FillRect (hdcMem, &rcTmp, (HBRUSH) GetStockObject (WHITE_BRUSH));
            RoundRect (hdcMem, rgpInfo[index].nWidth / 2,
                rgpInfo[index].nWidth / 2,
                nWidth - rgpInfo[index].nWidth / 2, nHeight - rgpInfo[index].nWidth / 2, 26 + index * 12, 51 + index * 8);

            hpenOld = (HPEN) SelectObject (hdcMem, hpenStock);
            if (hpenOld != hpen)
                LogResult (1,
                    TEXT ("FAIL: index=%d: henOld(0x%lX)  !=  hpen(0x%lX): something is wrong\n"), index, hpenOld, hpen);
            Polyline (hdcMem, (LPPOINT) & rcTmp, 2);

            BitBlt (hdcPrint, x, y, (int)nWidth, (int)nHeight, hdcMem, 0, 0, dwRop);
            x += (int)(nWidth + 20);
            if (!DeleteObject (hpen))
            {
                LogResult (1, TEXT ("FAIL: index=%d: Delete(hpen=0x%lX): err=%ld"), index, hpen, nRetCode = GetLastError ());
            }
        }

        SelectObject (hdcMem, hbrushOld);
        SelectObject (hdcMem, hbmOld);

        if (!DeleteObject (hbm))
        {
            LogResult (1, TEXT ("FAIL: iLoop=%d: Delete(hbm=0x%lX): err=%ld"), iLoop, hbm, nRetCode = GetLastError ());
        }

        if (!DeleteObject (hbrush))
        {
            LogResult (1, TEXT ("FAIL: iLoop=%d: Delete(hbrush=0x%lX): err=%ld"), iLoop, hbrush, nRetCode = GetLastError ());
        }

        x = 0;
        y += (int)(fMul * nHeight) + 20;
    }

    DeleteDC (hdcMem);
    return nRetCode;
}


int PASCAL DrawPageStretchBlt (HDC hdcPrint, int iDoc, int iPage, int nCallAbort)
{
    int nRetCode = 0;
    int dpi;
    RECT rc1;
    LPRECT lpRect1;
    int nTextHeight, i, iLoop, nLoops;
    HFONT hfont, hOldfont;
    HPEN hpen;
    LOGFONT logfont;
    DWORD dwRop[15] = { SRCCOPY, SRCPAINT, SRCAND, SRCINVERT, NOTSRCCOPY, SRCERASE,
        NOTSRCERASE,
        MERGECOPY,
        MERGEPAINT, PATCOPY, PATPAINT, PATINVERT, DSTINVERT, BLACKNESS,
        WHITENESS
    };
    TCHAR Rops[][15] = { TEXT ("SRCCOPY"), TEXT ("SRCPAINT"), TEXT ("SRCAND"),
        TEXT ("SRCINVERT"), TEXT ("NOTSRCCOPY"),
        TEXT ("SRCERASE"), TEXT ("NOTSRCERASE"), TEXT ("MERGECOPY"),
        TEXT ("MERGEPAINT"), TEXT ("PATCOPY"),
        TEXT ("PATPAINT"), TEXT ("PATINVERT"), TEXT ("DSTINVERT"),
        TEXT ("BLACKNESS"), TEXT ("WHITENESS")
    };
    float ratio[2] = { (float)1.35, (float)0.75 };
    DWORD dwError;


    LogResult (1, TEXT ("------------------ DrawPageStretchBlt() Starts!  iDoc=%d  iPage=%d"), iDoc + 1, iPage + 1);

    dpi = GetDeviceCaps (hdcPrint, LOGPIXELSX);
    hfont = (HFONT) GetStockObject (SYSTEM_FONT);
    GetObject (hfont, sizeof (logfont), &logfont);
    nTextHeight = logfont.lfHeight = dpi / 6;
    logfont.lfWidth = 0;

    hpen = (HPEN) SelectObject (hdcPrint, GetStockObject (BLACK_PEN));
    hfont = CreateFontIndirect (&logfont);
    hOldfont = (HFONT) SelectObject (hdcPrint, hfont);


    // only print one page
    nLoops = 1;
    for (iLoop = 0; iLoop < nLoops; iLoop++)
    {
        SetRect (&rc1, 0, 0, GetDeviceCaps (hdcPrint, HORZRES), GetDeviceCaps (hdcPrint, VERTRES));
        lpRect1 = &rc1;
        if (iLoop == 1)
            DrawText (hdcPrint, TEXT ("StretchBlt: Shrink: ratio=0.75"), -1, lpRect1, DT_WORDBREAK | DT_LEFT);
        else
            DrawText (hdcPrint, TEXT ("StretchBlt: Enlarge: ratio=1.35"), -1, lpRect1, DT_WORDBREAK | DT_LEFT);

        lpRect1->top += 2 * nTextHeight;

        for (i = 0; i < 15; i++)
        {
            DrawText (hdcPrint, Rops[i], -1, lpRect1, DT_WORDBREAK | DT_LEFT);
            lpRect1->top += 2 * nTextHeight;
            MyStretchBlt (hdcPrint, lpRect1, dwRop[i], ratio[iLoop]);

            if ((i + 1) % 4 == 0 && i != 0)
            {
                lpRect1->left = 0;
                lpRect1->top += (int)(GetDeviceCaps (hdcPrint, LOGPIXELSX) * ratio[iLoop]);
                lpRect1->top += 2 * nTextHeight;
            }

            else
            {
                lpRect1->left += (int)(GetDeviceCaps (hdcPrint, LOGPIXELSX) * ratio[iLoop]);
                lpRect1->left += 2 * nTextHeight;
                lpRect1->top -= 2 * nTextHeight;
            }
        }
        if (iLoop == 0 && nLoops > 1)
        {
            if (!CallEndAndStartNewPage (hdcPrint, &dwError))
            {
                if (g_fUserAbort)
                    LogResult (1, TEXT ("User aborted printing."));
                else
                    nRetCode = dwError;
                goto LReturn;
            }
        }
    }

  LReturn:
    SelectObject (hdcPrint, hpen);
    SelectObject (hdcPrint, hOldfont);
    DeleteObject (hfont);
    return nRetCode;
}

BOOL PASCAL MyStretchBlt (HDC hdcPrint, LPRECT lpRect, DWORD dwRop, float ratio)
{
    HDC hdcmem;
    HBITMAP hbitmap, hOldbitmap;
    RECT rc;
    HBRUSH hBrush;
    HPEN hpen;
    int nLength = (int)(GetDeviceCaps (hdcPrint, LOGPIXELSX) * ratio);
    int nHeight = (int)(GetDeviceCaps (hdcPrint, LOGPIXELSY) * ratio);
    int nDX = (int)nLength * 3 / 5;
    int nDY = (int)nHeight * 3 / 5;
    int iResult;

    hdcmem = CreateCompatibleDC (hdcPrint);
    if (hdcmem == NULL)
    {
        LogResult (1, TEXT ("CreateCompatibleDC failed, err=%ld"), GetLastError ());
        DrawText (hdcPrint, TEXT ("OUT OF MEMORY"), -1, lpRect, DT_WORDBREAK | DT_LEFT);
        return FALSE;
    }

    hbitmap = CreateCompatibleBitmap (hdcPrint, nLength, nHeight);
    if (hbitmap == NULL)
    {
        LogResult (1, TEXT ("CreateCompatibleBMP failed, err=%ld"), GetLastError ());
        DrawText (hdcPrint, TEXT ("OUT OF MEMORY"), -1, lpRect, DT_WORDBREAK | DT_LEFT);
        DeleteDC (hdcmem);
        return FALSE;
    }

    if ((hOldbitmap = (HBITMAP) SelectObject (hdcmem, hbitmap)) == NULL)
    {
        LogResult (1, TEXT ("SelectObject(hbitmap) failed, err=%ld"), GetLastError ());
    }
    // Paint grey back drop.
    SetRect (&rc, 0, 0, nLength, nHeight);
    hBrush = CreateSolidBrush (RGB (188, 188, 188));
    if (hBrush == NULL)
        LogResult (1, TEXT ("CreateSolidBrush failed, err=%ld"), GetLastError ());
    if (FillRect (hdcmem, &rc, hBrush) == FALSE)
        LogResult (1, TEXT ("FillRect failed, err=%ld"), GetLastError ());
    DeleteObject (hBrush);

    iResult = BitBlt (hdcPrint, lpRect->left, lpRect->top, nLength, nHeight, hdcmem, 0, 0, SRCCOPY);
    if (iResult == FALSE)
        LogResult (1, TEXT ("LRGRAY: BitBlt failed, err=%ld"), GetLastError ());

    // Initialize 3/5 inch square rect.
    SetRect (&rc, 0, 0, (int)(GetDeviceCaps (hdcPrint, LOGPIXELSX) * 0.6), (int)(GetDeviceCaps (hdcPrint, LOGPIXELSX) * 0.6));

    // Put red square in memory DC.
    hBrush = CreateSolidBrush (RGB (255, 0, 0));
    if (FillRect (hdcmem, &rc, hBrush) == FALSE)
        LogResult (1, TEXT ("FillRect failed, err=%ld"), GetLastError ());
    DeleteObject (hBrush);
    // StretchBlt Red square into Printer DC.
    iResult = StretchBlt (hdcPrint, lpRect->left + nDX / 3, lpRect->top, nDX, nDY, hdcmem, 0, 0, rc.right, rc.bottom, dwRop);
    if (iResult == FALSE)
        LogResult (1, TEXT ("RED: StretchBlt failed, err=%ld"), GetLastError ());


    // Put BLUE square in memory DC.
    hBrush = CreateSolidBrush (RGB (0, 0, 255));
    if (FillRect (hdcmem, &rc, hBrush) == FALSE)
        LogResult (1, TEXT ("FillRect failed, err=%ld"), GetLastError ());
    DeleteObject (hBrush);
    // StretchBlt bule square into Printer DC.
    iResult = StretchBlt (hdcPrint, lpRect->left, lpRect->top + nDY / 3, nDX, nDY, hdcmem, 0, 0, rc.right, rc.bottom, dwRop);
    if (iResult == FALSE)
        LogResult (1, TEXT ("BLUE: StretchBlt failed, err=%ld"), GetLastError ());

    // Put GREEN  square in memory DC.
    hBrush = CreateSolidBrush (RGB (0, 255, 0));
    if (FillRect (hdcmem, &rc, hBrush) == FALSE)
        LogResult (1, TEXT ("FillRect failed, err=%ld"), GetLastError ());
    DeleteObject (hBrush);
    // StretchBlt green square into Printer DC.
    iResult =
        StretchBlt (hdcPrint, lpRect->left + 2 * nDX / 3,
        lpRect->top + 2 * nDY / 3, nDX, nDY, hdcmem, 0, 0, rc.right, rc.bottom, dwRop);
    if (iResult == FALSE)
        LogResult (1, TEXT ("BLUE: StretchBlt failed, err=%ld"), GetLastError ());


    if (!DeleteDC (hdcmem))
        LogResult (1, TEXT ("StretchBlt: DeleteDC(hdcmem) failed, err=%ld"), GetLastError ());

    if (!DeleteObject (hbitmap))
        LogResult (1, TEXT ("StretchBlt: DeleteDC(hbitmap) failed, err=%ld"), GetLastError ());

    hBrush = (HBRUSH) SelectObject (hdcPrint, GetStockObject (NULL_BRUSH));
    hpen = (HPEN) SelectObject (hdcPrint, GetStockObject (BLACK_PEN));
    if (!Rectangle (hdcPrint, lpRect->left, lpRect->top, lpRect->left + nLength, lpRect->top + nHeight))
        LogResult (1, TEXT ("StretchBlt: Rectangle failed, err=%ld"), GetLastError ());
    SelectObject (hdcPrint, hpen);
    SelectObject (hdcPrint, hBrush);
    return TRUE;
}



//==============================================
// Only Selcet Clip Regsion once. Print 2 pages
//==============================================
int PASCAL DrawPageClipRegion (HDC hdcPrint, int iDoc, int iPage, int nCallAbort)
{
    int nRetCode = 0, x, y;
    HPEN hpen, hpenOld;
    HBRUSH hbrush, hbrushOld;
    HFONT hfontOld;
    RECT rcTmp, rc;
    POINT points[2];
    HRGN hrgn;
    DWORD dwError;

    LogResult (1, TEXT ("------------------ DrawPageClipRegion() Starts!  iDoc=%d  iPage=%d"), iDoc + 1, iPage + 1);

    SetLastError (0);
    x = g_rcPrint.right / 2;
    y = g_rcPrint.bottom / 2;
    SetViewportOrgEx (hdcPrint, x, y, &points[0]);
    GetClipBox (hdcPrint, &rcTmp);
    LogResult (DEBUG_OUTPUT,
        TEXT
        ("Page 1. SetViewport(%d %d): Clip Rectangle=[%d %d %d %d]"), x, y, rcTmp.left, rcTmp.top, rcTmp.right, rcTmp.bottom);

    rcTmp.left = x / 2;
    rcTmp.top = y / 2;
    rcTmp.right = g_rcPrint.right - x / 2;
    rcTmp.bottom = g_rcPrint.bottom - y / 2;

    LogResult (DEBUG_OUTPUT, TEXT ("Set Clip Region = [%d %d %d %d]"), rcTmp.left, rcTmp.top, rcTmp.right, rcTmp.bottom);

    // select clipregion into DC
    hrgn = CreateRectRgn (rcTmp.left, rcTmp.top, rcTmp.right, rcTmp.bottom);
    SelectClipRgn (hdcPrint, hrgn);
    DeleteObject (hrgn);

    // Verify
    GetClipBox (hdcPrint, &rcTmp);
    LogResult (DEBUG_OUTPUT,
        TEXT ("Page 1. After set, Clip Rectangle=[%d %d %d %d]"), rcTmp.left, rcTmp.top, rcTmp.right, rcTmp.bottom);

    // Select font, pen and brush into DC
    hfontOld = (HFONT) SelectObject (hdcPrint, g_hfont);

    SetLastError (0);

    // Create the new red Pen;
    hpen = CreatePen (PS_SOLID, 30, RGB (255, 0, 0));
    hpenOld = (HPEN) SelectObject (hdcPrint, hpen);

    if (hpen == NULL || hpenOld == NULL)
    {
        LogResult (1, TEXT ("DrawPageClipRegion: Create RED Pen or Select Pen failed: err=%ld."), GetLastError ());
    }

    SetLastError (0);
    hbrush = CreateSolidBrush (RGB (0, 0, 255)); // blue
    hbrushOld = (HBRUSH) SelectObject (hdcPrint, hbrush);
    if (hbrush == NULL || hbrushOld == NULL)
    {
        LogResult (1, TEXT ("DrawPageClipRegion: Create BLUE brush or Select brush failed: err=%ld."), GetLastError ());
    }

    // page 1
    rc.left = -x / 4;
    rc.top = -y / 4;
    rc.right = x / 4;
    rc.bottom = y / 4;

    Rectangle (hdcPrint, -x / 4, -y / 4, x / 4, y / 4);
    DrawText (hdcPrint, TEXT ("Page 1"), -1, &rc, DT_VCENTER | DT_CENTER | DT_SINGLELINE);

    // page 2
    if (!CallEndAndStartNewPage (hdcPrint, &dwError))
    {
        if (g_fUserAbort)
            LogResult (1, TEXT ("User aborted printing."));
        else
            nRetCode = dwError;
        goto LRet;
    }

    // View origin and Clip region both are lost lost:  this is the same as NT:
    SetViewportOrgEx (hdcPrint, x, y, &points[0]);
    GetClipBox (hdcPrint, &rcTmp);
    LogResult (DEBUG_OUTPUT,
        TEXT
        ("Page 2. After EndPage() calling, Clip Rectangle=[%d %d %d %d]"),
        x, y, rcTmp.left, rcTmp.top, rcTmp.right, rcTmp.bottom);

    Rectangle (hdcPrint, -x / 4, -y / 4, x / 4, y / 4);
    DrawText (hdcPrint, TEXT ("Page 2"), -1, &rc, DT_VCENTER | DT_CENTER | DT_SINGLELINE);

  LRet:
    SelectObject (hdcPrint, hfontOld);
    SelectObject (hdcPrint, hpenOld);
    SelectObject (hdcPrint, hbrushOld);
    DeleteObject (hpen);
    DeleteObject (hbrush);
    return nRetCode;
}

int PASCAL DrawPageColorText (HDC hdcPrint, int iDoc, int iPage, int nCallAbort)
{
    int nRetCode = 0, iResult, i, j;
    RECT rc;
    LPRECT lpRect;
    int dpi;
    HFONT hfont, hOldfont;
    LOGFONT logfont;
    TCHAR szTmp[63];

    LogResult (1, TEXT ("------------------ DrawPageColorText() Starts!  iDoc=%d  iPage=%d"), iDoc + 1, iPage + 1);

    dpi = GetDeviceCaps (hdcPrint, LOGPIXELSX);
    hfont = (HFONT) GetStockObject (SYSTEM_FONT);
    GetObject (hfont, sizeof (logfont), &logfont);
    logfont.lfHeight = dpi / 5;
    logfont.lfWidth = 0;
    logfont.lfItalic = TRUE;
    _tcscpy (logfont.lfFaceName, TEXT ("Times New Roman"));
    hfont = CreateFontIndirect (&logfont);
    if (hfont == NULL)
        LogResult (1, TEXT ("CreateFontIndirect failed. err=%ld"), GetLastError ());
    hOldfont = (HFONT) SelectObject (hdcPrint, hfont);
    if (hOldfont == NULL)
        LogResult (1, TEXT ("SelectObject failed. err=%ld"), GetLastError ());

    // Do OPAQUE TEXT
    //====================
    SetRect (&rc, 0, 0, GetDeviceCaps (hdcPrint, HORZRES), GetDeviceCaps (hdcPrint, VERTRES));
    lpRect = &rc;
    if (SetBkMode (hdcPrint, OPAQUE) == 0)
        LogResult (1, TEXT ("SetBkMode(OPAQUE) failed. err=%ld"), GetLastError ());
    iResult =
        ExtTextOut (hdcPrint, lpRect->left, lpRect->top, NULL, NULL,
        TEXT ("Background mode is OPAQUE, text is clipped on"), 45, NULL);
    if (iResult <= 0)
        LogResult (1, TEXT ("ExtTextOut failed. err=%ld"), GetLastError ());
    for (i = 0; i < 3; i++)
    {
        if (SetBkColor (hdcPrint, rgColor[i].clr) == CLR_INVALID)
            LogResult (1, TEXT ("SetBkColor(%s) failed. err=%ld"), (LPTSTR) rgColor[i].clrName, GetLastError ());

        for (j = 0; j < 5; j++)
        {
            SetTextColor (hdcPrint, rgColor[i + 1 + j].clr);
            lpRect->top += dpi / 4;
            wsprintf (szTmp, TEXT ("This is %s text on %s background"),
                (LPTSTR) rgColor[i + j + 1].clrName, (LPTSTR) rgColor[i].clrName);

            iResult =
                ExtTextOut (hdcPrint, lpRect->left, lpRect->top, ETO_CLIPPED, lpRect, (LPTSTR) szTmp, _tcslen (szTmp), NULL);
            if (iResult <= 0)
                LogResult (1, TEXT ("OPAQUE:i=%d j=%d ExtTextOut failed. err=%ld"), i, j, GetLastError ());
        }
    }

    // DO Tranparent TEXT
    //====================
    SetBkColor (hdcPrint, RGB (255, 255, 255));
    SetTextColor (hdcPrint, RGB (0, 0, 0));
    lpRect->top += dpi / 4;
    lpRect->right = (int)dpi *5 / 2;

    ExtTextOut (hdcPrint, lpRect->left, lpRect->top, ETO_OPAQUE, NULL,
        TEXT ("Background mode is TRANSPARENT, text is NOT clipped on"), 54, NULL);
    if (SetBkMode (hdcPrint, TRANSPARENT) == 0)
        LogResult (1, TEXT ("SetBkMode(TRANSPARENT) failed. err=%ld"), GetLastError ());

    for (i = 3; i <= 7; i++)
    {
        if (SetBkColor (hdcPrint, rgColor[i].clr) == CLR_INVALID)
            LogResult (1, TEXT ("SetBkColor(%s) failed. err=%ld"), (LPTSTR) rgColor[i].clrName, GetLastError ());
        for (j = 1; j < 6; j++)
        {
            SetTextColor (hdcPrint, rgColor[j].clr);
            lpRect->top += dpi / 4;
            if (lpRect->top >= lpRect->bottom)
                goto LDone;
            wsprintf (szTmp, TEXT ("This is %s text on %s background"),
                (LPTSTR) rgColor[j].clrName, (LPTSTR) rgColor[i].clrName);
            iResult =
                ExtTextOut (hdcPrint, lpRect->left, lpRect->top, ETO_OPAQUE, lpRect, (LPTSTR) szTmp, _tcslen (szTmp), NULL);
            if (iResult <= 0)
                LogResult (1, TEXT ("TRANSPARENT:i=%d j=%d ExtTextOut failed. err=%ld"), i, j, GetLastError ());
        }
    }

  LDone:
    if (SelectObject (hdcPrint, hOldfont) == NULL)
        LogResult (1, TEXT ("SelectObject failed. GetLastError returned 0x%x"), GetLastError ());
    if (DeleteObject (hfont) == 0)
        LogResult (1, TEXT ("DelectObject failed. GetLastError returned 0x%x"), GetLastError ());
    return nRetCode;
}
