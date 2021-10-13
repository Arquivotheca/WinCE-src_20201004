//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//=================================================================
//  command line:  Default is printing out Black/White
//  example:
//  s tux -o -d prntest -x101-112
//
//  In color printer:  if you want to print out colr
//  s tux -o -d prntest -x101-112  -c "color"
//=================================================================

#define DEBUG_OUTPUT 1
#define CALL_PRINTDLG 1         // for NT

#include <windows.h>
#include <tchar.h>
#include <commdlg.h>

#ifdef UNDER_NT
#include <winspool.h>
#endif
#include <kato.h>
#include <tux.h>
#include "prntest.h"

#ifndef UNDER_CE
#include <stdio.h>
#endif

typedef int (*PFN_DrawPage) (HDC hdcPrint, int iDoc, int iPage, int nCallAbort);
PFN_DrawPage rgpfnDraw[] = {
    (PFN_DrawPage) DrawPageFullText, // index 0
    (PFN_DrawPage) DrawPageXXXBlt, // index 1:
    (PFN_DrawPage) DrawPageFonts, // index 2:
    (PFN_DrawPage) DrawPagePens, // index 3:
    (PFN_DrawPage) DrawPageBrushes, // index 4:
    (PFN_DrawPage) DrawPageBitmap2, // index 5:
    (PFN_DrawPage) DrawPageStretchBlt, // index 6:
    (PFN_DrawPage) DrawPageTransparentI, // index 7:
    (PFN_DrawPage) DrawPageFonts2, // index 8:
    (PFN_DrawPage) DrawPageClipRegion, // index 9:
    (PFN_DrawPage) DrawPageColorText, // index 10:
};


TESTPROCAPI TestIrdaPrint (UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{                               //0: IRDA;  1: COM1: 57600;  2: COM1: 9600; 3: PARALLEL; 4 net work
    return TestPrint (uMsg, tpParam, lpFTE, USE_IRDA);
}

TESTPROCAPI TestSerialPrint1 (UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{                               //0: IRDA;  1: COM1: 57600;  2: COM1: 9600; 3: PARALLEL; 4 net work
    return TestPrint (uMsg, tpParam, lpFTE, USE_SERIAL_57600);
}

TESTPROCAPI TestSerialPrint2 (UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{                               //0: IRDA;  1: COM1: 57600;  2: COM1: 9600; 3: PARALLEL; 4 net work
    return TestPrint (uMsg, tpParam, lpFTE, USE_SERIAL_9600);
}

TESTPROCAPI TestParallelPrint (UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{                               //0: IRDA;  1: COM1: 57600;  2: COM1: 9600; 3: PARALLEL; 4 net work
    return TestPrint (uMsg, tpParam, lpFTE, USE_PARALLEL);
}

TESTPROCAPI TestNetWorkPrint (UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{                               //0: IRDA;  1: COM1: 57600;  2: COM1: 9600; 3: PARALLEL; 4 net work
    return TestPrint (uMsg, tpParam, lpFTE, USE_NETWORK);
}

//******************************************************************************
//
//******************************************************************************
int FAR PASCAL TestPrint (UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE, UINT uPort)
{
    BOOL bPrinterDC = TRUE;
    int nRetCode = TPR_PASS;
    int nDrawFunc;
    int iDoc, nDocs, iPage, nPages, nRet;
    DWORD dwError;
    DEVMODE dm;
    HDC hdcPrint;
    DOCINFO docinfo;
    TCHAR szDebug[128];
    TCHAR szDocName[32];

    // Check our message value to see why we have been called.
    if (uMsg == TPM_QUERY_THREAD_COUNT)
    {
        ((LPTPS_QUERY_THREAD_COUNT) tpParam)->dwThreadCount = 1;
        return SPR_HANDLED;
    }
    else if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }



    nDrawFunc = lpFTE->dwUserData & PRN_SELECT_FUNCTION;
    nPages = (lpFTE->dwUserData & PRN_SELECT_PAGE_NUM) >> 24;
    nDocs = (lpFTE->dwUserData & PRN_SELECT_DOC_NUM) >> 28;

    MySetDevMode (lpFTE->dwUserData, &dm);

  LRetryAgain:
    if ((g_nJobs == 0 || gszNetPath[0] == 0) && uPort == USE_NETWORK) // network printing: need to ask user type in the path
    {
        nRet = DialogBox (g_hInst, TEXT ("QueryDlgBox"), NULL, QueryDlgProc);
        if (nRet == 0)          // user press cancel button
        {
            LogResult (1, TEXT ("========== NetPathDlg: User has pressed Cancel button ========"));
            return TPR_PASS;
        }
        else if (nRet == -1)    // error occurs
        {
            LogResult (1, TEXT ("========== NetPathDlg: Error occurs:  GetLastError() return %ld ========"), GetLastError ());
            return TPR_FAIL;
        }
    }

    SetLastError (0);

    if (g_nJobs > 0)
    {
        LogResult (1, TEXT ("Sleep 15 seconds before calling CreateDC()"));
        Sleep (15 * 1000);
    }

    if (bPrinterDC)
    {
    #ifdef UNDER_CE
        if (uPort == USE_NETWORK)   // network printing: need to ask user type in the path
            hdcPrint = CreateDC (TEXT ("PCL.DLL"), NULL, gszNetPath, &dm);
        else if (g_szDCOutput[0])   // user specified an output device
            hdcPrint = CreateDC (TEXT ("PCL.DLL"), NULL, g_szDCOutput, &dm);
        else
            hdcPrint = CreateDC (TEXT ("PCL.DLL"), NULL, grgszOutput[uPort], &dm);
        LogResult (1, TEXT ("--------------------- CreateDC( PCL.DLL  NULL  %s  &dm)"),
            (LPTSTR) (uPort == USE_NETWORK ? gszNetPath : 
            g_szDCOutput[0] ? g_szDCOutput : grgszOutput[uPort]));

    #else // UNDER_NT
        hdcPrint = MyGetPrintDC1 (&dm);
    // hdcPrint = MyGetNTPrintDC2( &dm, grgszOutput[uPort] );
    #endif
    }
    else
    {
        hdcPrint = GetDC(NULL);
    }

    if (hdcPrint == NULL)
    {
        LogResult (1, TEXT ("CrateDC() FAIL: err=%ld"), GetLastError ());
        if (uPort == USE_NETWORK) // network printing: the path might be wrong
        {
            gszNetPath[0] = 0;  // reset the path
            gszNetPath[1] = 0;

            nRet = MessageBox (NULL, TEXT ("The printer network path might be wrong, Do you want to retry?"),
                TEXT ("Error"), MB_YESNO | MB_SETFOREGROUND | MB_APPLMODAL);

            if (nRet == IDYES)
                goto LRetryAgain;
        }

        nRetCode = TPR_FAIL;
        goto LRelease;
    }

//LogResult(DEBUG_OUTPUT,  TEXT("CrateDC() SUCCEED: hdcPrint = 0x%lX"),  hdcPrint);
    LogResult (DEBUG_OUTPUT, TEXT ("dmSpecVersion = 0x%lX"), dm.dmSpecVersion);
    LogResult (DEBUG_OUTPUT, TEXT ("dmSize        =   %d "), dm.dmSize);
    LogResult (DEBUG_OUTPUT, TEXT ("dmOrientation =   %d "), dm.dmOrientation);
    LogResult (DEBUG_OUTPUT, TEXT ("dmPaperSize   =   %d "), dm.dmPaperSize);
    LogResult (DEBUG_OUTPUT, TEXT ("dmCopies      =   %d "), dm.dmCopies);
    LogResult (DEBUG_OUTPUT, TEXT ("dmPrintQuality=   %d "), dm.dmPrintQuality);
    LogResult (DEBUG_OUTPUT, TEXT ("dmColor       =   %d "), dm.dmColor);
    LogResult (DEBUG_OUTPUT, TEXT ("dmBitsPerPel  =   %d "), dm.dmBitsPerPel);
    LogResult (DEBUG_OUTPUT, TEXT ("dmPelsWidth   =   %d "), dm.dmPelsWidth);
    LogResult (DEBUG_OUTPUT, TEXT ("dmPelsHeight  =   %d "), dm.dmPelsHeight);
    LogResult (DEBUG_OUTPUT, TEXT ("dmFields      = 0x%lX"), dm.dmFields);

    g_fDraftMode = (dm.dmPrintQuality == DMRES_DRAFT) ? TRUE : FALSE;

// Get the size of the printing area
    g_rcPrint.left = 0;
    g_rcPrint.top = 0;
    g_rcPrint.right = GetDeviceCaps (hdcPrint, HORZRES);
    g_rcPrint.bottom = GetDeviceCaps (hdcPrint, VERTRES);

    LogResult (1, TEXT ("rcPrint=[%d  %d  %d  %d]:  %s mode"),
        g_rcPrint.left, g_rcPrint.top, g_rcPrint.right, g_rcPrint.bottom,
        (LPTSTR) (g_fDraftMode ? TEXT ("DRAFT") : TEXT ("HIGH_QUALITY")));


    if (bPrinterDC)
    {
        if ((nRet = SetAbortProc (hdcPrint, AbortProc)) <= 0)
        {
            LogResult (1, TEXT ("SetAbortProc() failed. nRet=%d  err=%ld"), nRet, GetLastError ());
            nRetCode = TPR_FAIL;
            goto LRelease;
        }
    }

    docinfo.cbSize = sizeof (docinfo);
    docinfo.lpszOutput = NULL;  // default to printer

    ghDlgPrint = CreateDialog (g_hInst, TEXT ("PrintDlgBox"), NULL, PrintDlgProc);

    g_nJobs++;
    iDoc = 0;
    while (iDoc < nDocs)
    {
        g_fUserAbort = FALSE;
        wsprintf (szDocName, TEXT ("Job %d: DOC %d"), g_nJobs, iDoc + 1);
        docinfo.lpszDocName = szDocName;
        LogResult (1, TEXT ("Start printing <%s>"), (LPTSTR) szDocName);
        SetWindowText (ghDlgPrint, szDocName);

        if (bPrinterDC && !CallStartDoc (hdcPrint, &docinfo, &dwError))
        {
            if (g_fUserAbort)
            {
                LogResult (1, TEXT ("User aborted printing."));
            }
            else
                nRetCode = TPR_FAIL;
            goto LDestroyPrintDlg;
        }

        for (iPage = 0; iPage < nPages; iPage++)
        {
            // Test Clip region:
            if (lpFTE->dwUserData & PRN_SELECT_CLIPRGN)
            {
                if (!MySetClipRegion (hdcPrint, iPage))
                    nRetCode = TPR_FAIL;
            }
            if (bPrinterDC && !CallStartPage (hdcPrint, &dwError))
            {
                if (g_fUserAbort)
                {
                    LogResult (1, TEXT ("User aborted printing."));
                }
                else
                    nRetCode = TPR_FAIL;
                goto LDestroyPrintDlg;
            }

            wsprintf (szDebug, TEXT ("%s: page %d"), szDocName, iPage + 1);
            SetWindowText (ghDlgPrint, szDebug);

            LogResult (DEBUG_OUTPUT, TEXT ("============== g_nJobs=%d: iDoc=%d iPage=%d: Call Draw Function!"),
                g_nJobs, iDoc + 1, iPage + 1);

            SetLastError (0);
            nRet = (rgpfnDraw[nDrawFunc]) (hdcPrint, iDoc, iPage, bPrinterDC);

            if (g_fUserAbort)
            {
                LogResult (1, TEXT ("User aborted printing."));
                goto LDestroyPrintDlg;
            }
            else if (nRet != 0) // error occurs
            {
                if (nRet == ERROR_OUTOFMEMORY || nRet == ERROR_NOT_ENOUGH_MEMORY)
                {
                    LogResult (1, TEXT ("Call rgpfnDraw[%d] fail: return %d:  OUTOFMEMORY"), nDrawFunc, nRet);
                }
                else
                {
                    LogResult (1, TEXT ("Call rgpfnDraw[%d] fail:  return %d:"), nDrawFunc, nRet);
                }
                // continue
            }

            if (bPrinterDC && !CallEndPage (hdcPrint, &dwError))
            {
                if (g_fUserAbort)
                    LogResult (1, TEXT ("User aborted printing."));
                else
                    nRetCode = TPR_FAIL;
                goto LDestroyPrintDlg;
            }

            // ckeck abort state:
            if (g_fUserAbort)
                goto LDestroyPrintDlg;
        }                       // for (iPage = 0; ...)

        if (bPrinterDC && !CallEndDoc (hdcPrint, &dwError))
        {
            if (g_fUserAbort)
                LogResult (1, TEXT ("User aborted printing."));
            else
                nRetCode = TPR_FAIL;
            goto LDestroyPrintDlg;
        }
        iDoc++;
        // ckeck abort state:
        if (g_fUserAbort)
            break;
    }                           // for (iDoc= 0; ...)

  LDestroyPrintDlg:
    DestroyWindow (ghDlgPrint);
    ghDlgPrint = NULL;

  LRelease:
    if (bPrinterDC)
    {
        DeleteDC (hdcPrint);
    }
    else
    {
        ReleaseDC(NULL, hdcPrint);
    }
    return nRetCode;
}


VOID PASCAL MySetDevMode (DWORD dwUserData, LPDEVMODE lpdm)
{
    memset (lpdm, 0, sizeof (DEVMODE));
    lpdm->dmSize = sizeof (DEVMODE);
    lpdm->dmSpecVersion = 0x400;

    lpdm->dmFields |= DM_COPIES;
    lpdm->dmCopies = 1;         // CE only support 1: (short) ( (dwUserData & PRN_SELECT_PAGE_NUM) >> 24 );

    lpdm->dmFields |= DM_COLOR;

    if (g_fColorPrint)
    {
        LogResult (1, TEXT ("set up: DMCOLOR_COLOR"));
        lpdm->dmColor = DMCOLOR_COLOR;
    }
    else
    {
        if (dwUserData & PRN_SELECT_COLOR) // 0: black/white:  1: color
            lpdm->dmColor = DMCOLOR_COLOR;
        else
            lpdm->dmColor = DMCOLOR_MONOCHROME;
    }

    lpdm->dmFields |= DM_PAPERSIZE;

    if (g_dmPaperSize)          // use the value in comamnd line
    {
        lpdm->dmPaperSize = (SHORT)g_dmPaperSize;
    }
    else                        // use the program's  defined value
    {
        switch ((dwUserData & PRN_SELECT_PAPESIZE) >> 16)
        {
            case 0:            // letter = 0
                lpdm->dmPaperSize = DMPAPER_LETTER;
                break;
            case 1:
                lpdm->dmPaperSize = DMPAPER_A4;
                break;
            case 2:
                lpdm->dmPaperSize = DMPAPER_LEGAL;
                break;
            case 3:
                lpdm->dmPaperSize = DMPAPER_B5;
                break;
        }
    }


    lpdm->dmFields |= DM_PRINTQUALITY;
    if (dwUserData & PRN_SELECT_QUALITY) // 0: use Draft mode  1: Full QUALITY
        lpdm->dmPrintQuality = DMRES_HIGH; //DMRES_HIGH = -4
    else
        lpdm->dmPrintQuality = DMRES_DRAFT; // DMRES_DRAFT = -1


    lpdm->dmFields |= DM_ORIENTATION;
    if (dwUserData & PRN_SELECT_LANDSCAPE) //
        lpdm->dmOrientation = DMORIENT_LANDSCAPE; // 1: Landscape
    else
        lpdm->dmOrientation = DMORIENT_PORTRAIT; // 0: portrait
}

BOOL CALLBACK PrintDlgProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static BOOL fButtonClose = FALSE;
    TCHAR szText[128];

    switch (msg)
    {
        case WM_USER + 40:
            SetWindowText (GetDlgItem (hDlg, IDCANCEL), TEXT ("Close"));
            fButtonClose = TRUE;
            return TRUE;
        case WM_INITDIALOG:
            if (gfPerformance)
            {
                LoadString (g_hInst, IDS_String_PerfTest, szText, 128);
                SetWindowText (GetDlgItem (hDlg, IDC_STATIC1), szText);
            }
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD (wParam)) // WID
            {
                case IDCANCEL:
                    if (!fButtonClose)
                    {
                        g_fUserAbort = TRUE;
                        LogResult (1, TEXT ("==========  USER  press Cancel button ========="));
                    }
                    DestroyWindow (hDlg);
                    ghDlgPrint = 0;

                    if (fButtonClose)
                    {
                        PostQuitMessage (0); // TestPrintPerf()  has a message loop
                    }
                    return TRUE;
                default:
                    break;
            }
            break;
    }
    return FALSE;
}




BOOL CALLBACK QueryDlgProc (HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    TCHAR szText[128];

    switch (msg)
    {
        case WM_INITDIALOG:
            if (gfPerformance)
            {
                LoadString (g_hInst, IDS_String_PerfPages, szText, 128);
                SetWindowText (GetDlgItem (hwndDlg, IDC_STATIC1), szText);
            }
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD (wParam)) // WID
            {
                case IDOK:     // network printing dialog:  check the path ??
                    if (gfPerformance)
                    {
                        BOOL fSucceed = FALSE;

                        gnPages = GetDlgItemInt (hwndDlg, IDC_EDIT_1, &fSucceed, FALSE /* bSigned */ );
                        if (!fSucceed)
                        {
                            MessageBox (NULL, TEXT ("The input is not correct"), TEXT ("Error"), MB_OK | MB_SETFOREGROUND);
                            return FALSE;
                        }
                    }
                    else        // NetPathDlg
                    {
                        if (!GetDlgItemText (hwndDlg, IDC_EDIT_1, (LPTSTR) gszNetPath, MAX_PATH_LENGTH))
                        {
                            MessageBox (NULL, TEXT ("The path is empty!"), TEXT ("Error"), MB_OK | MB_SETFOREGROUND);
                            return FALSE;
                        }
                    }
                    EndDialog (hwndDlg, 1);
                    return TRUE;

                case IDCANCEL:
                    EndDialog (hwndDlg, 0);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}



BOOL PASCAL MySetClipRegion (HDC hdcPrint, int iPage)
{
    RECT rc, rcGet;
    int xRequestMargin, yRequestMargin, xDPI, yDPI;
    int nPhysicalW, nPhysicalH, nPhysicalOffsetX, nPhysicalOffsetY, xMinRightMargin, yMinBottomMargin;
    HRGN hrgn;

    xDPI = GetDeviceCaps (hdcPrint, LOGPIXELSX);
    yDPI = GetDeviceCaps (hdcPrint, LOGPIXELSY);

    switch (iPage)
    {
        case 2:                // page2:  margin=2.5 inches
            xRequestMargin = (int)(xDPI * 2.5);
            yRequestMargin = (int)(yDPI * 2.5);
            break;

        case 1:                // page1:  margin= 3 inches
            xRequestMargin = xDPI * 3;
            yRequestMargin = yDPI * 3;
            break;

        case 0:                // page0:  margin= 1.5 inch
            xRequestMargin = xDPI / 2;
            yRequestMargin = yDPI / 2;
            break;

        default:
            xRequestMargin = 0;
            yRequestMargin = 0;
            break;
    }

    nPhysicalW = GetDeviceCaps (hdcPrint, PHYSICALWIDTH);
    nPhysicalH = GetDeviceCaps (hdcPrint, PHYSICALHEIGHT);
    nPhysicalOffsetX = GetDeviceCaps (hdcPrint, PHYSICALOFFSETX); // left margin
    nPhysicalOffsetY = GetDeviceCaps (hdcPrint, PHYSICALOFFSETY); // top  margin

    LogResult (DEBUG_OUTPUT, TEXT ("PWidth=%d  PHeight=%d  POffsetX=%d  pOffsetY=%d xRequestMargin=%d  yRequestMargin=%d"),
        nPhysicalW, nPhysicalH, nPhysicalOffsetX, nPhysicalOffsetY, xRequestMargin, yRequestMargin);

// xMinLeftMargin =  gnPhysicalOffsetX ;
// yMinLeftMargin =  gnPhysicalOffsetY ;
// we need to calculate the right and bottom  Min Margin (un printed area)
    xMinRightMargin = nPhysicalW - g_rcPrint.right - nPhysicalOffsetX;
    yMinBottomMargin = nPhysicalH - g_rcPrint.bottom - nPhysicalOffsetY;


// Set up the clip rectangle value
    rc.left = xRequestMargin - nPhysicalOffsetX;
    rc.top = yRequestMargin - nPhysicalOffsetY;
    rc.right = g_rcPrint.right - (xRequestMargin - xMinRightMargin);
    rc.bottom = g_rcPrint.bottom - (yRequestMargin - yMinBottomMargin);

    LogResult (DEBUG_OUTPUT, TEXT ("Set ClipRc = [%d %d %d %d]"), rc.left, rc.top, rc.right, rc.bottom);
    hrgn = CreateRectRgn (rc.left, rc.top, rc.right, rc.bottom);
    SelectClipRgn (hdcPrint, hrgn);
    DeleteObject (hrgn);
    // Verify
    GetClipBox (hdcPrint, &rcGet);

    if (memcmp (&rc, &rcGet, sizeof (RECT)))
    {
        LogResult (DEBUG_OUTPUT, TEXT ("After set clip regsion: rcGet=[%d %d %d %d]  != rcSet [%d %d %d %d]"),
            rcGet.left, rcGet.top, rcGet.right, rcGet.bottom, rc.left, rc.top, rc.right, rc.bottom);
        return FALSE;
    }
    return TRUE;
}


BOOL CALLBACK AbortProc (HDC hdc, int error)
{
    MSG msg;

    while (!g_fUserAbort && PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (!ghDlgPrint || !IsDialogMessage (ghDlgPrint, &msg))
        {
            TranslateMessage (&msg);
            DispatchMessage (&msg);
        }
    }
    return !g_fUserAbort;
}



BOOL PASCAL CallStartDoc (HDC hdcPrint, DOCINFO * lpdocinfo, LPDWORD lpdwError)
{
    int nRet;

    *lpdwError = 0;
    SetLastError (0);
    nRet = StartDoc (hdcPrint, lpdocinfo);
    if (nRet <= 0)
    {
        // check first: Is this caused by USER pressing Cancel Button" ?
        if (g_fUserAbort)       // This is not a error
            return FALSE;

        *lpdwError = GetLastError ();
        LogResult (1, TEXT ("CallStartDoc() failed. nRet=%d  err=%ld:  "), nRet, *lpdwError);
        return FALSE;
    }
    return TRUE;
}

BOOL PASCAL CallStartPage (HDC hdcPrint, LPDWORD lpdwError)
{
    int nRet;

    *lpdwError = 0;
    SetLastError (0);
    nRet = StartPage (hdcPrint);
    g_nTotalPages++;
    if (nRet <= 0)
    {
        // check first: Is this caused by USER pressing Cancel Button" ?
        if (g_fUserAbort)       // This is not a error
            return FALSE;

        *lpdwError = GetLastError ();
        LogResult (1, TEXT ("CallStartPage() failed. nRet=%d  err=%ld:"), nRet, *lpdwError);
        return FALSE;
    }
    return TRUE;
}

BOOL PASCAL CallEndPage (HDC hdcPrint, LPDWORD lpdwError)
{
    int nRet;

    *lpdwError = 0;
    SetLastError (0);
    nRet = EndPage (hdcPrint);
    if (nRet <= 0)
    {
        // check first: Is this caused by USER pressing Cancel Button" ?
        if (g_fUserAbort)
            return FALSE;

        *lpdwError = GetLastError ();
        if (*lpdwError == ERROR_OUTOFMEMORY || *lpdwError == ERROR_NOT_ENOUGH_MEMORY)
        {
            LogResult (1, TEXT ("EndPage() failed.  err=%ld:  OUTOFMEMORY"), *lpdwError);
        }
        else
        {
            LogResult (1, TEXT ("EndPage() failed. nRet=%d  err=%ld:"), nRet, *lpdwError);
        }
        return FALSE;
    }
    return TRUE;
}

BOOL PASCAL CallEndDoc (HDC hdcPrint, LPDWORD lpdwError)
{
    int nRet;

    *lpdwError = 0;
    SetLastError (0);
    nRet = EndDoc (hdcPrint);
    if (nRet <= 0)
    {
        // check first: Is this caused by USER pressing Cancel Button" ?
        if (g_fUserAbort)
            return FALSE;

        *lpdwError = GetLastError ();
        LogResult (1, TEXT ("CallEndDoc() failed. nRet=%d  err=%ld:"), nRet, *lpdwError);
        return FALSE;
    }
    return TRUE;
}

BOOL PASCAL CallEndAndStartNewPage (HDC hdcPrint, LPDWORD lpdwError)
{
    if (g_fUserAbort)
        return FALSE;

    if (!CallEndPage (hdcPrint, lpdwError))
        return FALSE;
    if (!CallStartPage (hdcPrint, lpdwError))
        return FALSE;
    return TRUE;
}

TESTPROCAPI TestPrintPerf (UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UINT iPage;
    int nRet, nRetCode = TPR_PASS, nPort, hours, minutes, seconds;
    HDC hdcPrint = NULL;
    DWORD dwTime, dwTimeSave;
    MSG msg;
    DOCINFO docinfo;
    DEVMODE dm;
    TCHAR szText[512], szDebug[32];

    // Check our message value to see why we have been called.
    if (uMsg == TPM_QUERY_THREAD_COUNT)
    {
        ((LPTPS_QUERY_THREAD_COUNT) tpParam)->dwThreadCount = 1;
        return SPR_HANDLED;
    }
    else if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }
    gfPerformance = TRUE;
    nRet = DialogBox (g_hInst, TEXT ("QueryDlgBox"), NULL, QueryDlgProc);
    if (nRet == 0)              // user press cancel button
    {
        LogResult (1, TEXT ("========== QueryPagesDlg: User has pressed Cancel button ========"));
        return TPR_PASS;
    }
    else if (nRet == -1)        // error occurs
    {
        LogResult (1, TEXT ("========== QueryPagesDlg: Error occurs:  GetLastError() return %ld ========"), GetLastError ());
        return TPR_FAIL;
    }


    g_fUserAbort = FALSE;
    ghDlgPrint = CreateDialog (g_hInst, TEXT ("PrintDlgBox"), NULL, PrintDlgProc);
    ShowWindow (ghDlgPrint, SW_SHOWNORMAL);
    UpdateWindow (ghDlgPrint);

    nPort = lpFTE->dwUserData & 0xF;
    MySetDevMode (0x10000000 | lpFTE->dwUserData, &dm);
    nRet = gnPages;
// dwUserData
//0xF      F        F         F        F         F          F         F
//=========================================================================
//#Docs   #pages  0=Black  0=Letter    0=NonClip  0=Draft   0=Portrait   Func #
//                1=Color  1=A4        1=ClipRgn  1=Full    1=Landscape
//                         2=LEGAL         QUALITY
//                         3=B5

// 1 doc, n pages, COLOR or MONOCHROME,  LETTER, NonClip, DRAFT or HIGH QUALITY , PORTRAIT, FUNCTION=0

// =====================
// Now start count time
// =====================

#ifdef UNDER_NT
#ifdef CALL_PRINTDLG
    hdcPrint = MyGetPrintDC1 (&dm);
#else
    hdcPrint = MyGetNTPrintDC2 (&dm, grgszOutput[nPort]);
#endif
#else
    gdwStartTime = GetTickCount ();
    if(g_szDCOutput[0])
        hdcPrint = CreateDC (TEXT ("PCL.DLL"), NULL, g_szDCOutput, &dm);
    else
        hdcPrint = CreateDC (TEXT ("PCL.DLL"), NULL, grgszOutput[nPort], &dm);
#endif

    if (!hdcPrint)
    {
        LogResult (1, TEXT ("TestPrintPerf: CreateDC() failed: err=%ld"), (LPTSTR) (grgszOutput[nPort]), GetLastError ());
        nRetCode = TPR_FAIL;
        goto LRet0;
    }


    docinfo.cbSize = sizeof (docinfo);
    docinfo.lpszOutput = NULL;  // default to printer
    docinfo.lpszDocName = TEXT ("printing Performance Test");

    if ((nRet = SetAbortProc (hdcPrint, AbortProc)) <= 0)
    {
        LogResult (1, TEXT ("SetAbortProc() failed. nRet=%d  err=%ld"), nRet, GetLastError ());
        nRetCode = TPR_FAIL;
        goto LRet1;
    }

    if (StartDoc (hdcPrint, &docinfo) <= 0)
    {
        LogResult (1, TEXT ("StartDoc() failed. err=%ld"), GetLastError ());
        nRetCode = TPR_FAIL;
        goto LRet1;
    }

    for (iPage = 1; iPage <= gnPages; iPage++)
    {
        if (StartPage (hdcPrint) <= 0)
        {
            LogResult (1, TEXT ("Startpage( iPage=%d) failed. err=%ld"), iPage, GetLastError ());
            nRetCode = TPR_FAIL;
            goto LRet1;
        }


        LogResult (1, TEXT ("Start to printing page %d \r\n"), iPage);
        DrawPerfTestOnePage (hdcPrint, iPage, nPort);
        if (g_fUserAbort)
            goto LRet1;


        if (EndPage (hdcPrint) <= 0)
        {
            LogResult (1, TEXT ("EndPage( iPage=%d) failed. err=%ld"), iPage, GetLastError ());
            nRetCode = TPR_FAIL;
            goto LRet1;
        }
    }

    if (EndDoc (hdcPrint) <= 0)
    {
        LogResult (1, TEXT ("EndDoc() failed. err=%ld"), GetLastError ());
        nRetCode = TPR_FAIL;
        goto LRet1;
    }

// =====================
//
// =====================
    dwTime = dwTimeSave = GetTickCount () - gdwStartTime;
    wsprintf (szText, TEXT ("%d pages:\r\nTotal Running time: %u ms:"), gnPages, dwTime);
    hours = dwTime / 3600000;
    dwTime -= hours * 3600000;
    minutes = dwTime / 60000;
    dwTime -= minutes * 60000;
    seconds = dwTime / 1000;
    dwTime -= seconds * 1000;

    _tcscat (szText, TEXT ("= "));
    if (hours)
    {
        wsprintf (szDebug, TEXT ("%d hours "), hours);
        _tcscat (szText, szDebug);
    }
    if (minutes)
    {
        wsprintf (szDebug, TEXT ("%d min "), minutes);
        _tcscat (szText, szDebug);
    }
    if (seconds)
    {
        wsprintf (szDebug, TEXT ("%d sec "), seconds);
        _tcscat (szText, szDebug);
    }

    if (dwTime)
    {
        wsprintf (szDebug, TEXT ("%d ms"), dwTime);
        _tcscat (szText, szDebug);
    }


    wsprintf (szDebug, TEXT ("\r\n%d ms / per page"), dwTimeSave / gnPages);
    _tcscat (szText, szDebug);

    SetWindowText (GetDlgItem (ghDlgPrint, IDC_STATIC1), szText);
    SendMessage (ghDlgPrint, WM_USER + 40, 0, 0);
    LogResult (1, TEXT ("%s"), (LPTSTR) szText);

    while (GetMessage (&msg, (HWND) NULL, 0, 0))
    {
        TranslateMessage (&msg);
        DispatchMessage (&msg);
    }

  LRet1:
    DeleteDC (hdcPrint);
  LRet0:
    g_nJobs++;
    g_nTotalPages = gnPages;
    return nRetCode;
}


VOID DrawPerfTestOnePage (HDC hdcPrint, int iPage, int nPort)
{
#define MAX_TEXT_LENGTH  1024
    int nDPI, nScreenPix, nRet, nFit, nOffset, cx, cy, nWidth, nSpace = 14, nRatio;
    HDC hdcMem;
    HFONT hfont, hfontOld;
    HPEN hpen, hpenOld;
    HBRUSH hbrush, hbrushOld;
    HBITMAP hbm;
    LOGFONT lf;
    SIZE size;
    RECT rc, rcTmp;
    TCHAR szText[MAX_TEXT_LENGTH], szTmp[64], szTmp2[128];
    POINT pt[2];
    TEXTMETRIC tm;

    GetClipBox (hdcPrint, &rc);
    nDPI = GetDeviceCaps (hdcPrint, LOGPIXELSY);
#ifdef UNDER_NT
    if (nDPI == 150)
        rc.bottom = 1538;       // make it the same as  WINCE pcl.dll
    else
        rc.bottom = 1538 * 2;   // make it the same as  WINCE pcl.dll
#endif
    nScreenPix = GetDeviceCaps (GetDC (NULL), LOGPIXELSY);
    wsprintf (szText, TEXT ("DPI=%d: ScreenPixel=%d: %s:     width=%d height=%d"),
        nDPI, nScreenPix, (LPTSTR) grgszOutput[nPort], rc.right, rc.bottom);
#ifdef UNDER_NT
    _tcscpy (szTmp, TEXT ("Under NT"));
#else
    _tcscpy (szTmp, TEXT ("Under CE"));
#endif
    _tcscat (szText, TEXT ("     "));
    _tcscat (szText, szTmp);

    nRatio = 1;                 // 150 DPI: DRAFT mode
    if (nDPI == 300)
        nRatio = 2;
    else if (nDPI == 600)
        nRatio = 4;
    nSpace *= nRatio;

    memset ((LPVOID) & lf, 0, sizeof (LOGFONT));
    lf.lfWeight = FW_NORMAL;

    // part 1:  small text  ======================================:
    lf.lfHeight = 16 * nDPI / nScreenPix;
    _tcscpy (lf.lfFaceName, TEXT ("Tahoma"));
    hfont = CreateFontIndirect (&lf);
    if (!hfont)
        LogResult (1, TEXT ("TestPrintPerf: CreateFont(Tahoma) failed: err=%ld\r\n"), GetLastError ());
    hfontOld = (HFONT) SelectObject (hdcPrint, hfont);

    // Check  Font Name
    GetTextFace (hdcPrint, 64, szTmp);
    GetTextMetrics (hdcPrint, &tm);
    wsprintf (szTmp2, TEXT ("1: Want h=%d: Get %s h=%d"), lf.lfHeight, szTmp, tm.tmHeight);

    // Print out the DPI resolution
    rc.top += DrawText (hdcPrint, (LPTSTR) szText, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    rc.top += nSpace;           // space

    // part 1: small text
    rc.top += DrawText (hdcPrint, (LPTSTR) szTmp2, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    if (!LoadString (g_hInst, IDS_String_2, szText, MAX_TEXT_LENGTH))
        LogResult (1, TEXT ("TestPrintPerf: LoadString(IDS_String_2) failed: err=%ld"), GetLastError ());
    rc.top += DrawText (hdcPrint, (LPTSTR) szText, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    rc.top += nSpace;           // space
    SelectObject (hdcPrint, hfontOld);
    DeleteObject (hfont);


    // part 2: large text ======================================:
    lf.lfHeight = 32 * nDPI / nScreenPix;
    lf.lfWeight = FW_BOLD;      // == 700;
    lf.lfItalic = 1;
    lf.lfUnderline = 1;
    _tcscpy (lf.lfFaceName, TEXT ("Tahoma"));
    hfont = CreateFontIndirect (&lf);
    if (!hfont)
        LogResult (1, TEXT ("TestPrintPerf: CreateFont(Tahoma) failed: err=%ld"), GetLastError ());
    hfontOld = (HFONT) SelectObject (hdcPrint, hfont);

    //Check  Font Name
    GetTextFace (hdcPrint, 64, szTmp);
    GetTextMetrics (hdcPrint, &tm);
    wsprintf (szTmp2, TEXT ("2: Want h=%d: Get %s h=%d"), lf.lfHeight, szTmp, tm.tmHeight);
    _tcscat (szTmp2, TEXT (" +B+I+U"));

    lf.lfUnderline = 0;
    rc.top += DrawText (hdcPrint, (LPTSTR) szTmp2, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    // Load text string
    if (!LoadString (g_hInst, IDS_String_3, szText, MAX_TEXT_LENGTH))
        LogResult (1, TEXT ("TestPrintPerf: LoadString(IDS_String_3) failed: err=%ld"), GetLastError ());
    rc.top += DrawText (hdcPrint, (LPTSTR) szText, 100, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    rc.top += nSpace * 2;


    // part 3: 4 bits bitmap + drawing +  8 bits bitmap ======================================:
    if ((hdcMem = CreateCompatibleDC (GetDC (NULL))) == NULL)
        LogResult (1, TEXT ("CrearteCompatibleDC()  err=%ld"), GetLastError ());

    pt[0].x = rc.left;
    pt[0].y = rc.top;
    pt[1].x = rc.right;
    pt[1].y = rc.top;
    Polyline (hdcPrint, pt, 2);

    cx = 425 * nRatio;
    cy = 337 * nRatio;
    hbm = LoadBitmap (g_hInst, TEXT ("winnt4"));
    if (!hbm)
        LogResult (1, TEXT ("TestPrintPerf: LoadBitmap(winnt4) failed: err=%ld"), GetLastError ());

    cx = 425 * nRatio;
    cy = 337 * nRatio;
    SetLastError (0);
    if (!SelectObject (hdcMem, hbm))
        LogResult (1, TEXT ("TestPrintPerf: SelectObject(winnt4) failed: err=%ld"), GetLastError ());
    if (!StretchBlt (hdcPrint, 0, rc.top, cx, cy, hdcMem, 0, 0, 425, 337, SRCCOPY))
        LogResult (1, TEXT ("StretchBlt(1): failed: err=%ld"), GetLastError ());

    // Create the new red Pen;
    nWidth = 30 * nRatio;
    hpen = CreatePen (PS_SOLID, nWidth, RGB (255, 0, 0));
    hpenOld = (HPEN) SelectObject (hdcPrint, hpen);
    hbrush = CreateSolidBrush (RGB (0, 0, 255)); // blue
    hbrushOld = (HBRUSH) SelectObject (hdcPrint, hbrush);

    rcTmp.left = rc.left + cx;
    rcTmp.top = rc.top;
    cx = 200 * nRatio;
    cy = 200 * nRatio;
    rcTmp.right = rcTmp.left + cx;
    rcTmp.bottom = rcTmp.top + cy;
    Rectangle (hdcPrint, rcTmp.left, rcTmp.top, rcTmp.right, rcTmp.bottom);
    wsprintf (szText, TEXT ("Page %d"), iPage);
    DrawText (hdcPrint, szText, -1, &rcTmp, DT_VCENTER | DT_CENTER | DT_SINGLELINE);
    SelectObject (hdcPrint, hpenOld);
    SelectObject (hdcPrint, hbrushOld);
    DeleteObject (hpen);
    DeleteObject (hbrushOld);

    // Now delete font 2
    SelectObject (hdcPrint, hfontOld);
    DeleteObject (hfont);

    nWidth = rc.right - (rcTmp.right + nSpace * 2);
    hbm = LoadBitmap (g_hInst, TEXT ("globe8"));
    if (!hbm)
        LogResult (1, TEXT ("TestPrintPerf: LoadBitmap(globe8) failed: err=%ld"), GetLastError ());
    SetLastError (0);
    if (!SelectObject (hdcMem, hbm))
        LogResult (1, TEXT ("TestPrintPerf: SelectObject(globe8) failed: err=%ld"), GetLastError ());
    if (!StretchBlt (hdcPrint, rcTmp.right + nSpace * 2, rc.top, nWidth, cy * 2, hdcMem, 0, 0, 45, 45, SRCCOPY))
    {
        LogResult (1, TEXT ("StretchBlt(1): failed: err=%ld"), GetLastError ());
    }
    DeleteDC (hdcMem);

    rc.top += (cy * 2 + nSpace);

    // part 4:  small text  ======================================:
    lf.lfHeight = 18 * nDPI / nScreenPix;
    lf.lfWeight = FW_NORMAL;
    _tcscpy (lf.lfFaceName, TEXT ("Tahoma"));
    hfont = CreateFontIndirect (&lf);
    if (!hfont)
        LogResult (1, TEXT ("TestPrintPerf: CreateFont(Tahoma: 2) failed: err=%ld"), GetLastError ());
    hfontOld = (HFONT) SelectObject (hdcPrint, hfont);

    //Check  Font Name
    GetTextFace (hdcPrint, 64, szTmp);
    GetTextMetrics (hdcPrint, &tm);
    wsprintf (szTmp2, TEXT ("3: Want h=%d: Get %s h=%d"), lf.lfHeight, szTmp, tm.tmHeight);
    _tcscat (szTmp2, TEXT (" + Italic"));

    rc.top += DrawText (hdcPrint, (LPTSTR) szTmp2, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    // Load text string
    if (!LoadString (g_hInst, IDS_String_4, szText, MAX_TEXT_LENGTH))
        LogResult (1, TEXT ("TestPrintPerf: LoadString(IDS_String_4) failed: err=%ld"), GetLastError ());
    rc.top += DrawText (hdcPrint, (LPTSTR) szText, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    rc.top += nSpace;           // space
    SelectObject (hdcPrint, hfontOld);
    DeleteObject (hfont);



    // part 5: Midium text: Red color  ======================================:
    SetTextColor (hdcPrint, RGB (0xFF, 0, 0));
    lf.lfHeight = 20 * nDPI / nScreenPix;
    lf.lfWeight = FW_BOLD;
    lf.lfItalic = 0;
    _tcscpy (lf.lfFaceName, TEXT ("Tahoma"));
    hfont = CreateFontIndirect (&lf);
    if (!hfont)
        LogResult (1, TEXT ("TestPrintPerf: CreateFont(Tahoma) failed: err=%ld"), GetLastError ());
    hfontOld = (HFONT) SelectObject (hdcPrint, hfont);
    
    //Check  Font Name
    GetTextFace (hdcPrint, 64, szTmp);
    GetTextMetrics (hdcPrint, &tm);
    wsprintf (szTmp2, TEXT ("4: Want h=%d: Get %s h=%d"), lf.lfHeight, szTmp, tm.tmHeight);
    _tcscat (szTmp2, TEXT (" + Bold + RED"));

    rc.top += DrawText (hdcPrint, (LPTSTR) szTmp2, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    if (!LoadString (g_hInst, IDS_String_5, szText, MAX_TEXT_LENGTH))
        LogResult (1, TEXT ("TestPrintPerf: LoadString(IDS_String_5) failed: err=%ld"), GetLastError ());
    rc.top += DrawText (hdcPrint, (LPTSTR) szText, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    rc.top += nSpace;
    SelectObject (hdcPrint, hfontOld);
    DeleteObject (hfont);



    SetTextColor (hdcPrint, RGB (0, 0, 0));
    lf.lfHeight = 12 * nDPI / nScreenPix;
    lf.lfWeight = FW_NORMAL;
    _tcscpy (lf.lfFaceName, TEXT ("Tahoma"));
    hfont = CreateFontIndirect (&lf);
    if (!hfont)
        LogResult (1, TEXT ("TestPrintPerf: CreateFont(Tahoma) failed: err=%ld"), GetLastError ());
    hfontOld = (HFONT) SelectObject (hdcPrint, hfont);

    //Check  Font Name
    GetTextFace (hdcPrint, 64, szTmp);
    GetTextMetrics (hdcPrint, &tm);
    wsprintf (szTmp2, TEXT ("5: Want h=%d: Get %s h=%d"), lf.lfHeight, szTmp, tm.tmHeight);

    rc.top += DrawText (hdcPrint, (LPTSTR) szTmp2, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    if (!LoadString (g_hInst, IDS_String_2, szText, MAX_TEXT_LENGTH))
        LogResult (1, TEXT ("TestPrintPerf: LoadString(IDS_String_2): 2: failed: err=%ld"), GetLastError ());
    rc.top += DrawText (hdcPrint, (LPTSTR) szText, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    rc.top += nSpace;


    if ((nRet = LoadString (g_hInst, IDS_String_3, szText, MAX_TEXT_LENGTH)) == 0)
        LogResult (1, TEXT ("TestPrintPerf: LoadString(IDS_String_3): 2: failed: err=%ld"), GetLastError ());
    nRet = DrawText (hdcPrint, (LPTSTR) szText, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    rc.top += nRet;
    rc.top += nSpace;


    if ((nRet = LoadString (g_hInst, IDS_String_4, szText, MAX_TEXT_LENGTH)) == 0)
        LogResult (1, TEXT ("TestPrintPerf: LoadString(IDS_String_4): 2: failed: err=%ld"), GetLastError ());
    nRet = DrawText (hdcPrint, (LPTSTR) szText, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    rc.top += nRet;
    rc.top += nSpace;


    // nRet = number of chars in the string
    if ((nRet = LoadString (g_hInst, IDS_String_5, szText, MAX_TEXT_LENGTH)) == 0)
        LogResult (1, TEXT ("TestPrintPerf: LoadString(IDS_String_5): 2: failed: err=%ld"), GetLastError ());
    nRet = DrawText (hdcPrint, (LPTSTR) szText, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    rc.top += nRet;
    rc.top += nSpace;


    lf.lfHeight = 14 * nDPI / nScreenPix;
    _tcscpy (lf.lfFaceName, TEXT ("Tahoma"));
    hfont = CreateFontIndirect (&lf);
    if (!hfont)
        LogResult (1, TEXT ("TestPrintPerf: CreateFont(Tahoma) failed: err=%ld"), GetLastError ());
    hfontOld = (HFONT) SelectObject (hdcPrint, hfont);
    
    //Check  Font Name
    GetTextFace (hdcPrint, 64, szTmp);
    GetTextMetrics (hdcPrint, &tm);
    wsprintf (szTmp2, TEXT ("6: Want h=%d: Get %s h=%d"), lf.lfHeight, szTmp, tm.tmHeight);

    rc.top += DrawText (hdcPrint, (LPTSTR) szTmp2, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    // nRet = number of chars in the string
    if ((nRet = LoadString (g_hInst, IDS_String_2, szText, MAX_TEXT_LENGTH)) == 0)
        LogResult (1, TEXT ("TestPrintPerf: LoadString(IDS_String_2): 3: failed: err=%ld"), GetLastError ());

    nFit = 0;
    size.cx = size.cy = 0;
    nOffset = 0;
    for( ; ; )
    {
        GetTextExtentExPoint (hdcPrint, szText, nRet, rc.right, &nFit, NULL, &size);
        DrawText (hdcPrint, (LPTSTR) (szText + nOffset), nFit, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
        nOffset += nFit;
        nRet -= nFit;
        rc.top += size.cy;
        if (nRet <= 0)
            break;
        // check: if we could fit the next line
        if ((rc.top + size.cy) >= rc.bottom)
            break;
    }
    SelectObject (hdcPrint, hfontOld);
    DeleteObject (hfont);
}




#ifdef UNDER_NT
#ifdef CALL_PRINTDLG
HDC PASCAL MyGetPrintDC1 (DEVMODE * lpdmUser)
{
    BOOL bPrintDlg;             // Return code from PrintDlg function
    PRINTDLG pd;                // Printer dialog structure
    DWORD dwError;

    //  Initialize variables     hDC = NULL;
    memset (&pd, 0, sizeof (PRINTDLG));
    /* Initialize the PRINTDLG members. */
    pd.lStructSize = sizeof (PRINTDLG);
    pd.hwndOwner = NULL;
    pd.Flags = PD_RETURNDC | PD_PRINTSETUP;
    pd.hDevMode = (HANDLE) NULL;
    pd.hDevNames = (HANDLE) NULL;
    pd.nFromPage = 1;
    pd.nToPage = 1;
    pd.nMinPage = 0;
    pd.nMaxPage = 0;
    pd.nCopies = 1;
    pd.hInstance = g_hInst;

    bPrintDlg = PrintDlg (&pd);
    if (!bPrintDlg)             // Either no changes, or a common dialog error occured
    {
        dwError = CommDlgExtendedError ();
        if (dwError != 0)
        {
            LogResult (1, TEXT ("PrintDlg() failed:  err=%ld"), dwError);
            return (NULL);
        }
    }

    gdwStartTime = GetTickCount ();
    return pd.hDC;
}
#endif // CALL_PRINTDLG



HDC PASCAL MyGetNTPrintDC2 (DEVMODE * lpdmUser, LPTSTR szPort)
{
    HDC hdc = NULL;
    TCHAR szTemp[256];
    LPTSTR lpszTemp;

    // Get Default printer name.
    GetProfileString (TEXT ("windows"), TEXT ("device"), TEXT (""), szTemp, sizeof (szTemp));
    if (_tcslen (szTemp) == 0)
    {
        // INVARIANT:  no default printer.
        LogResult (1, TEXT ("GetProfileString() failed: err=%ld\r\n"), GetLastError ());
        return (NULL);
    }
    // Terminate at first comma, just want printer name.
    lpszTemp = _tcschr (szTemp, ',');
    if (lpszTemp != NULL)
        *lpszTemp = '\x0';

    lpdmUser->dmSpecVersion = 0x401;
    if (lpdmUser->dmPrintQuality == DMRES_HIGH)
        lpdmUser->dmPrintQuality = 300; // match CE standards: DMRES_HIGH = 300 dpi
    else
        lpdmUser->dmPrintQuality = 150; // match CE standards: DMRES_DRAFT = 150 dpi

    gdwStartTime = GetTickCount ();
    SetLastError (0);
    hdc = CreateDC (NULL, szTemp, szPort, lpdmUser);
    if (hdc)
    {
        LogResult (1, TEXT ("NT: CreateDC(NULL, %s  %s, &dm) succeeded.\r\n"), (LPTSTR) szTemp, (LPTSTR) szPort);
    }
    else
    {
        LogResult (1, TEXT ("NT: CreateDC(NULL, %s  %s, &dm) failed. err=%ld\r\n"),
            (LPTSTR) szTemp, (LPTSTR) szPort, GetLastError ());
    }
    return hdc;
}
#endif // UNDER_NT





VOID DrawPerfTestOnePage_2 (HDC hdcPrint, int iPage, int nPort)
{
#define MAX_TEXT_LENGTH  1024
    int nDPI, nScreenPix, nSpace = 20, nRatio;
    HFONT hfont, hfontOld;
    LOGFONT lf;
    RECT rc;
    TCHAR szText[MAX_TEXT_LENGTH], szTmp[128], szTmp2[128];
    TEXTMETRIC tm;

    GetClipBox (hdcPrint, &rc);
    nDPI = GetDeviceCaps (hdcPrint, LOGPIXELSY);
    nScreenPix = GetDeviceCaps (GetDC (NULL), LOGPIXELSY);
    wsprintf (szText, TEXT ("DPI=%d: ScreenPixel=%d: %s:     width=%d height=%d"),
        nDPI, nScreenPix, (LPTSTR) grgszOutput[nPort], rc.right, rc.bottom);

    nRatio = 1;                 // 150 DPI: DRAFT mode
    if (nDPI == 300)
        nRatio = 2;
    else if (nDPI == 600)
        nRatio = 4;
    nSpace *= nRatio;

    memset ((LPVOID) & lf, 0, sizeof (LOGFONT));
    lf.lfWeight = FW_NORMAL;
    lf.lfHeight = 20 * nDPI / nScreenPix;

    // part 1:
    _tcscpy (lf.lfFaceName, TEXT ("Times New Roman"));
    hfont = CreateFontIndirect (&lf);
    if (!hfont)
        LogResult (1, TEXT ("TestPrintPerf: CreateFont(Times New Roman) failed: err=%ld\r\n"), GetLastError ());
    hfontOld = (HFONT) SelectObject (hdcPrint, hfont);

    //Check  Font Name
    GetTextFace (hdcPrint, 64, szTmp);
    GetTextMetrics (hdcPrint, &tm);
    wsprintf (szTmp2, TEXT ("Font 1:  Face=%s  height=%d"), szTmp, tm.tmHeight);
    LogResult (1, TEXT ("%s\r\n"), (LPTSTR) szTmp2);

    // Print out the DPI resolution
    rc.top += DrawText (hdcPrint, (LPTSTR) szText, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    rc.top += nSpace;           // space

    // print font:
    rc.top += DrawText (hdcPrint, (LPTSTR) szTmp2, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    if (!LoadString (g_hInst, IDS_String_2, szText, 512))
        LogResult (1, TEXT ("TestPrintPerf: LoadString(IDS_String_2) failed: err=%ld"), GetLastError ());
    rc.top += DrawText (hdcPrint, (LPTSTR) szText, 200, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    rc.top += nSpace;           // space
    SelectObject (hdcPrint, hfontOld);
    DeleteObject (hfont);


    // part 2:
    _tcscpy (lf.lfFaceName, TEXT ("Tahoma"));
    hfont = CreateFontIndirect (&lf);
    if (!hfont)
        LogResult (1, TEXT ("TestPrintPerf: CreateFont(Tahoma) failed: err=%ld"), GetLastError ());
    hfontOld = (HFONT) SelectObject (hdcPrint, hfont);

    //Check  Font Name
    GetTextFace (hdcPrint, 64, szTmp);
    GetTextMetrics (hdcPrint, &tm);
    wsprintf (szTmp2, TEXT ("Font 2:  Face=%s  height=%d"), szTmp, tm.tmHeight);
    LogResult (1, TEXT ("%s\r\n"), (LPTSTR) szTmp2);

    rc.top += DrawText (hdcPrint, (LPTSTR) szTmp2, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    rc.top += DrawText (hdcPrint, (LPTSTR) szText, 200, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    rc.top += nSpace;
    SelectObject (hdcPrint, hfontOld);
    DeleteObject (hfont);


    // part 3:
    _tcscpy (lf.lfFaceName, TEXT ("Arial"));
    hfont = CreateFontIndirect (&lf);
    if (!hfont)
        LogResult (1, TEXT ("TestPrintPerf: CreateFont(Arial) failed: err=%ld"), GetLastError ());
    hfontOld = (HFONT) SelectObject (hdcPrint, hfont);
    
    //Check  Font Name
    GetTextFace (hdcPrint, 64, szTmp);
    GetTextMetrics (hdcPrint, &tm);
    wsprintf (szTmp2, TEXT ("Font 3:  Face=%s  height=%d"), szTmp, tm.tmHeight);
    LogResult (1, TEXT ("%s\r\n"), (LPTSTR) szTmp2);
    rc.top += DrawText (hdcPrint, (LPTSTR) szTmp2, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    rc.top += DrawText (hdcPrint, (LPTSTR) szText, 200, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    rc.top += nSpace;
    SelectObject (hdcPrint, hfontOld);
    DeleteObject (hfont);


    lf.lfCharSet = SYMBOL_CHARSET;
    _tcscpy (lf.lfFaceName, TEXT ("Symbol"));

    hfont = CreateFontIndirect (&lf);
    if (!hfont)
        LogResult (1, TEXT ("TestPrintPerf: CreateFont(symbol) failed: err=%ld"), GetLastError ());
    hfontOld = (HFONT) SelectObject (hdcPrint, hfont);
    
    //Check  Font Name
    GetTextFace (hdcPrint, 64, szTmp);
    GetTextMetrics (hdcPrint, &tm);
    wsprintf (szTmp2, TEXT ("Font 4:  Face=%s  height=%d"), szTmp, tm.tmHeight);
    LogResult (1, TEXT ("%s\r\n"), (LPTSTR) szTmp2);
    rc.top += DrawText (hdcPrint, (LPTSTR) szTmp2, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    rc.top += DrawText (hdcPrint, (LPTSTR) szText, 200, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    rc.top += nSpace;
    SelectObject (hdcPrint, hfontOld);
    DeleteObject (hfont);

    lf.lfCharSet = 0;           // turn off this flag

    lf.lfItalic = 1;
    lf.lfWeight = FW_BOLD;
    _tcscpy (lf.lfFaceName, TEXT ("Arial Bold Italic"));
    hfont = CreateFontIndirect (&lf);
    if (!hfont)
        LogResult (1, TEXT ("TestPrintPerf: CreateFont(Arial Bold Italic) failed: err=%ld"), GetLastError ());
    hfontOld = (HFONT) SelectObject (hdcPrint, hfont);
    
    //Check  Font Name
    GetTextFace (hdcPrint, 64, szTmp);
    GetTextMetrics (hdcPrint, &tm);
    wsprintf (szTmp2, TEXT ("Font 5:  Face=%s  height=%d"), szTmp, tm.tmHeight);
    LogResult (1, TEXT ("%s\r\n"), (LPTSTR) szTmp2);
    rc.top += DrawText (hdcPrint, (LPTSTR) szTmp2, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    rc.top += DrawText (hdcPrint, (LPTSTR) szText, 200, &rc, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    rc.top += nSpace;
    SelectObject (hdcPrint, hfontOld);
    DeleteObject (hfont);
}
