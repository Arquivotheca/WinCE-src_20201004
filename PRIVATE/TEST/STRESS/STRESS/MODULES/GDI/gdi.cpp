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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*
________________________________________________________________________________
THIS  CODE AND  INFORMATION IS  PROVIDED "AS IS"  WITHOUT WARRANTY  OF ANY KIND,
EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.


Module Name: gdi.cpp

Abstract: stress module for GDI APIs

Notes: none

________________________________________________________________________________
*/

#include <windows.h>
#include <stressutils.h>
#include "resource.h"
#include "stressrun.h"
#include "gdistress.h"


#define    WNDCLASS_MAIN            _T("GDI_MAIN_QA")

const int MAX_INSTANCES = 2;

HINSTANCE g_hInstance = NULL;
LPTSTR g_rgszStr[] = {
    _T("alpha"),
    _T("beta"),
    _T("gamma"),
    _T("delta"),
    _T("epsilon"),
    _T("zeta"),
    _T("eta"),
    _T("theta"),
    _T("iota"),
    _T("kappa"),
    _T("lamda"),
    _T("mu"),
    _T("nu"),
    _T("xi"),
    _T("omikron"),
    _T("pi"),
    _T("rho"),
    _T("sigma"),
    _T("tau"),
    _T("upsilon"),
    _T("phi"),
    _T("chi"),
    _T("psi"),
    _T("omega")
};
INT STR_COUNT = ARRAY_SIZE(g_rgszStr);

INT g_nBitDepths[] = { 1, 2, 4, 8, 16, 24, 32 };
INT BITDEPTHS_COUNT = ARRAY_SIZE(g_nBitDepths);

INT g_nTextAlign[] = {
                           TA_BASELINE,
                           TA_BOTTOM,
                           TA_TOP,
                           TA_CENTER,
                           TA_LEFT,
                           TA_RIGHT,
                           TA_NOUPDATECP,
                           TA_UPDATECP
                        };
INT TEXTALIGN_COUNT = ARRAY_SIZE(g_nTextAlign);

INT g_nBKModes[] = {
                           OPAQUE,
                           TRANSPARENT
                         };
INT BKMODES_COUNT = ARRAY_SIZE(g_nBKModes);

INT g_nPenStyles[] = {
                                PS_SOLID,
                                PS_DASH,
                                PS_NULL
                                };
INT PENSTYLES_COUNT = ARRAY_SIZE(g_nPenStyles);

ROP_GUIDE g_nvRop3Array[] = {
                                NAMEVALENTRY(DSTINVERT), //5555
                                NAMEVALENTRY(SRCINVERT), // 6666
                                NAMEVALENTRY(SRCCOPY), //cccc
                                NAMEVALENTRY(SRCPAINT), //eeee
                                NAMEVALENTRY(SRCAND), //8888
                                NAMEVALENTRY(SRCERASE), //4444
                                NAMEVALENTRY(MERGECOPY), //c0c0
                                NAMEVALENTRY(MERGEPAINT), //bbbb
                                NAMEVALENTRY(NOTSRCCOPY), //3333
                                NAMEVALENTRY(NOTSRCERASE), //1111
                                NAMEVALENTRY(PATCOPY), //f0f0
                                NAMEVALENTRY(PATINVERT), //5a5a
                                NAMEVALENTRY(PATPAINT), //fbfb
                                NAMEVALENTRY(BLACKNESS), //0000
                                NAMEVALENTRY(WHITENESS), //ffff
                                NAMEVALENTRY(0x00AA0000), // NOP
                                NAMEVALENTRY(0x00B80000), // PDSPxax
                                NAMEVALENTRY(0x00A00000), // DPa
                                NAMEVALENTRY(0x00220000), //DSna
                                NAMEVALENTRY(0x00BA0000), // DPSnao
                                NAMEVALENTRY(0x00990000), // DSxn
                                NAMEVALENTRY(0x00690000), // PDSxxn
                                NAMEVALENTRY(0x00E20000), // DSPDxax
                                NAMEVALENTRY(0x00050000), // DPon
                                NAMEVALENTRY(0x000A0000), // DPna
                                NAMEVALENTRY(0x000F0000), // Pn
                                NAMEVALENTRY(0x00500000), // PDna
                                NAMEVALENTRY(0x005F0000), // DPan
                                NAMEVALENTRY(0x00A50000), // PDxn
                                NAMEVALENTRY(0x00AF0000), // DPno
                                NAMEVALENTRY(0x00F50000), // PDno
                                NAMEVALENTRY(0x00FA0000), // DPo
                                NAMEVALENTRY(0x00A00000) // DPa
                                };

INT ROP3_COUNT = ARRAY_SIZE(g_nvRop3Array);

ROP_GUIDE g_nvRop2Array[] = {
                                NAMEVALENTRY(R2_BLACK),
                                NAMEVALENTRY(R2_COPYPEN),
                                NAMEVALENTRY(R2_MASKNOTPEN),
                                NAMEVALENTRY(R2_MASKPEN),
                                NAMEVALENTRY(R2_MASKPENNOT),
                                NAMEVALENTRY(R2_MERGENOTPEN),
                                NAMEVALENTRY(R2_MERGEPEN),
                                NAMEVALENTRY(R2_MERGEPENNOT),
                                NAMEVALENTRY(R2_NOP),
                                NAMEVALENTRY(R2_NOT),
                                NAMEVALENTRY(R2_NOTCOPYPEN),
                                NAMEVALENTRY(R2_NOTMASKPEN),
                                NAMEVALENTRY(R2_NOTMERGEPEN),
                                NAMEVALENTRY(R2_NOTXORPEN),
                                NAMEVALENTRY(R2_WHITE),
                                NAMEVALENTRY(R2_XORPEN),
                                };

INT ROP2_COUNT = ARRAY_SIZE(g_nvRop2Array);

void FAR PASCAL MessagePump();
UINT DoStressIteration (void);
DWORD Test_Blt(HWND hWnd);
DWORD Test_Drawing(HWND hWnd);
DWORD Test_Text(HWND hWnd);
DWORD Test_Region_Clip(HWND hWnd);
DWORD Test_Miscellaneous(HWND hWnd);


#ifdef UNDER_CE
INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hInstPrev, LPWSTR wszCmdLine, INT nCmdShow)
#else /* UNDER_CE */
INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hInstPrev, LPSTR szCmdLine, INT nCmdShow)
#endif /* UNDER_CE */
{
    DWORD            dwStartTime = GetTickCount();
    UINT            ret;
    LPTSTR            tszCmdLine;
    int                retValue = 0;  // Module return value.  You should return 0 unless you had to abort.
    MODULE_PARAMS    mp;            // Cmd line args arranged here.  Used to init the stress utils.
    STRESS_RESULTS    results;       // Cumulative test results for this module.

    ZeroMemory((void*) &mp, sizeof(MODULE_PARAMS));
    ZeroMemory((void*) &results, sizeof(STRESS_RESULTS));


    // NOTE:  Modules should be runnable on desktop whenever possible,
    //        so we need to convert to a common cmd line char width:

#ifdef UNDER_NT
    tszCmdLine =  GetCommandLine();
#else
    tszCmdLine  = wszCmdLine;
#endif



    ///////////////////////////////////////////////////////
    // Initialization
    //

    // Read the cmd line args into a module params struct.
    // You must call this before InitializeStressUtils().

    // Be sure to use the correct version, depending on your
    // module entry point.

    if ( !ParseCmdLine_WinMain (tszCmdLine, &mp) )
    {
        LogFail (L"Failure parsing the command line!  exiting ...");

        // Use the abort return code
        return CESTRESS_ABORT;
    }


    // You must call this before using any stress utils

    InitializeStressUtils (
                            L"s2_gdi",     // Module name to be used in logging
                            LOGZONE(SLOG_SPACE_GDI, (SLOG_FONT | SLOG_BITMAP | SLOG_RGN | SLOG_TEXT | SLOG_CLIP | SLOG_DRAW)),
                            &mp         // Module params passed on the cmd line
                            );


    // save application instance

    g_hInstance = hInstance;

    // Module-specific parsing of tszUser ...

    // retrieve random seed from the command line (if specified)

    long lRandomSeed = 0;

    if (mp.tszUser)
    {
        lRandomSeed = _ttol(mp.tszUser);
    }

    if (!mp.tszUser || !lRandomSeed)
    {
        lRandomSeed = GetTickCount();
    }

    LogComment(_T("==== Random Seed: %d ====\r\n"), lRandomSeed);

    srand(lRandomSeed); // use command line

    LogVerbose (L"Starting at %d", dwStartTime);


    ///////////////////////////////////////////////////////
    // Main test loop
    //

    while ( CheckTime(mp.dwDuration, dwStartTime) )
    {
        ret = DoStressIteration();

        if (ret == CESTRESS_ABORT)
        {
            LogFail (L"Aborting on test iteration %i!", results.nIterations);

            retValue = CESTRESS_ABORT;
            break;
        }

        RecordIterationResults (&results, ret);
    }



    ///////////////////////////////////////////////////////
    // Clean-up, report results, and exit
    //

    DWORD dwEnd = GetTickCount();
    LogVerbose (L"Leaving at %d, duration (ms) = %d, (sec) = %d, (min) = %d",
                    dwEnd,
                    dwEnd - dwStartTime,
                    (dwEnd - dwStartTime) / 1000,
                    (dwEnd - dwStartTime) / 60000
                    );

    // DON'T FORGET:  You must report your results to the harness before exiting
    ReportResults (&results);

    return retValue;
}


UINT DoStressIteration (void)
{
    _stressrun_t obj(L"DoStressIteration");

    HWND hWnd = NULL;
    WNDCLASS wc;
    RECT rcWorkArea;

     // SystemParametersInfo with SPI_GETWORKAREA

    LogVerbose(_T("SystemParametersInfo with SPI_GETWORKAREA...\r\n"));

    if (!SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0))
    {
        LogWarn1(_T("#### SystemParametersInfo(SPI_GETWORKAREA) failed - Error: 0x%x ####\r\n"),
            GetLastError());
        obj.warning1();
        SetRect(&rcWorkArea, 0, 0, GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN));
    }

    LogVerbose(_T("Registering main window class...\r\n"));

    wc.style = 0;
    wc.lpfnWndProc = (WNDPROC)DefWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = g_hInstance;
    wc.hIcon = 0;
    wc.hCursor = 0;
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = WNDCLASS_MAIN;

    if (!RegisterClass(&wc))
    {
        LogWarn2(_T("RegisterClass failed - Error: 0x%x\r\n"),
            GetLastError());
        obj.warning2();
    }

    INT iModCount = GetRunningModuleCount(g_hInstance);
    if (iModCount > MAX_INSTANCES)
    {
        LogWarn1(TEXT("No more than %d modules allowed, currently %d, aborting"), MAX_INSTANCES, iModCount);
        obj.abort();
        goto cleanup;
    }

    LogVerbose(_T("Creating main window...\r\n"));

    hWnd = CreateWindowEx(
         0,
        WNDCLASS_MAIN,
        _T("GDI"),
        WS_OVERLAPPED | WS_CAPTION | WS_BORDER | WS_VISIBLE | WS_SYSMENU,
        rcWorkArea.left,
        rcWorkArea.top,
        RECT_WIDTH(rcWorkArea),
        RECT_HEIGHT(rcWorkArea),
        NULL,
        NULL,
        g_hInstance,
        NULL
    );

    if (!hWnd)
    {
        LogFail(_T("#### CreateWindowEx failed - Error: 0x%x ####\r\n"),
            GetLastError());
        obj.fail();
        goto cleanup;
    }

    SetForegroundWindow(hWnd);
    ShowWindow(hWnd, SW_SHOWNORMAL);
    UpdateWindow(hWnd);

    MessagePump();

    //__________________________________________________________________________

    obj.test(Test_Blt(hWnd));

    obj.test(Test_Drawing(hWnd));

    obj.test(Test_Text(hWnd));

    obj.test(Test_Region_Clip(hWnd));

    obj.test(Test_Miscellaneous(hWnd));

    //__________________________________________________________________________

    if (hWnd)
    {
        if (!DestroyWindow(hWnd))
        {
            LogFail(_T("#### DestroyWindow failed - Error: 0x%x ####\r\n"),
                GetLastError());
            obj.fail();
        }
    }

cleanup:
    if (!UnregisterClass(WNDCLASS_MAIN, g_hInstance))
    {
        LogWarn2(_T("UnregisterClass failed - Error: 0x%x\r\n"),
            GetLastError());
        obj.warning2();
    }

    return obj.status();
}


/*

@func:    RandomRect, generates a valid random rect within specified rect

@rdesc:    true if successful, false otherwise

@param:    [in] LPRECT prc: generated random rect

@param:    [in] const LPRECT prcContainer: generated random rect is inside this rect

@fault:    none

@pre:    none

@post:    none

@note:    none

*/

BOOL RandomRect(LPRECT prc, const LPRECT prcContainer)
{
    BOOL fMinDimensionNotSatisfied = TRUE;
    LONG lTemp = 0x00;

    // minimum width and height of the rectangle to be generated

    const MIN_X = 5, MIN_Y = 5;


    if ((RECT_WIDTH((*prcContainer)) < MIN_X) || (RECT_HEIGHT((*prcContainer)) < MIN_Y))
    {
        return FALSE;
    }

    while (fMinDimensionNotSatisfied)
    {
        // generate a set of random points within the container

        if (!SetRect(prc,
            ((prcContainer->right - MIN_X) - prcContainer->left) ? RANDOM_INT((prcContainer->right - MIN_X), prcContainer->left) : prcContainer->left,
            ((prcContainer->bottom - MIN_Y) - prcContainer->top) ? RANDOM_INT((prcContainer->bottom - MIN_Y), prcContainer->top) : prcContainer->top,
            (prcContainer->right - (prcContainer->left + MIN_X)) ? RANDOM_INT(prcContainer->right, (prcContainer->left + MIN_X)) : prcContainer->right,
            (prcContainer->bottom - (prcContainer->top + MIN_Y)) ? RANDOM_INT(prcContainer->bottom, (prcContainer->top + MIN_Y)) : prcContainer->bottom))
        {
            return FALSE;
        }

        // cannonize the generated rectangle

        if (prc->left > prc->right)
        {
            lTemp = prc->left;
            prc->left = prc->right;
            prc->right = lTemp;
        }

        if (prc->bottom < prc->top)
        {
            lTemp = prc->bottom;
            prc->bottom = prc->top;
            prc->top = lTemp;
        }

        // minimum width and height requirements must be satisfied

        if ((RECT_WIDTH((*prc)) >= MIN_X) && (RECT_HEIGHT((*prc)) >= MIN_Y))
        {
            fMinDimensionNotSatisfied = FALSE;
        }
    }

    return TRUE;
}


/*

@func:    RandomPoint, generates a random point inside given rect

@rdesc:    generated random point as a POINT structure

@param:    [in] const RECT *prcContainer: generated random point lies within this rect

@fault:    none

@pre:    none

@post:    none

@note:    none

*/

POINT RandomPoint(const RECT *prcContainer)
{
    INT nRangeX = 0;
    INT nRangeY = 0;
    POINT ptRandom;

    memset(&ptRandom, 0, sizeof ptRandom);

    nRangeX = RECT_WIDTH((*prcContainer));
    nRangeY = RECT_HEIGHT((*prcContainer));

    if (nRangeX)
    {
        ptRandom.x = (rand() % nRangeX) + prcContainer->left;
    }

    if (nRangeY)
    {
        ptRandom.y = (rand() % nRangeY) + prcContainer->top;
    }

    return ptRandom;
}


/*

@func:    Test_Blt, tests various Blt APIs

@rdesc:    stress return status

@param:    [in] HWND hWnd: handle to the main window

@fault:    none

@pre:    none

@post:    none

@note:    none

*/

DWORD Test_Blt(HWND hWnd)
{
    _stressrun_t obj(L"Test_Blt");
    _stressblt_t blt(g_hInstance, hWnd, &obj);

    if (CESTRESS_FAIL == obj.status())
    {
        goto done;
    }

    if (RANDOM_CHOICE)
    {
        LogComment(_T("Test_Blt: Testing BitBlt...\r\n"));
        obj.test(blt.bitblt());
    }

    if (RANDOM_CHOICE)
    {
        LogComment(_T("Test_Blt: Testing StretchBlt...\r\n"));
        obj.test(blt.stretchblt());
    }

    if (RANDOM_CHOICE)
    {
        LogComment(_T("Test_Blt: Testing MaskBlt...\r\n"));
        obj.test(blt.maskblt());
    }

    if (RANDOM_CHOICE)
    {
        LogComment(_T("Test_Blt: Testing PatBlt...\r\n"));
        obj.test(blt.patblt());
    }

    if (RANDOM_CHOICE)
    {
        LogComment(_T("Test_Blt: Testing TransparentImage...\r\n"));
        obj.test(blt.transparentimage());
    }

    if (RANDOM_CHOICE)
    {
        LogComment(_T("Test_Blt: Testing AlphaBlend...\r\n"));
        obj.test(blt.alphablend());
    }

done:
    return obj.status();
}


/*

@func:    Test_Drawing, tests various drawing APIs

@rdesc:    stress return status

@param:    [in] HWND hWnd: handle to the main window

@fault:    none

@pre:    none

@post:    none

@note:    none

*/

DWORD Test_Drawing(HWND hWnd)
{
    _stressrun_t obj(L"Test_Drawing");
    _stressdraw_t draw(g_hInstance, hWnd, &obj);

    if (CESTRESS_FAIL == obj.status())
    {
        goto done;
    }

    if (RANDOM_CHOICE)
    {
        LogComment(_T("Test_Drawing: Testing Rectangle...\r\n"));
        obj.test(draw.rectangle());
    }

    if (RANDOM_CHOICE)
    {
        LogComment(_T("Test_Drawing: Testing FillRect...\r\n"));
        obj.test(draw.fillrect());
    }

    if (RANDOM_CHOICE)
    {
        LogComment(_T("Test_Drawing: Testing Ellipse...\r\n"));
        obj.test(draw.ellipse());
    }

    if (RANDOM_CHOICE)
    {
        LogComment(_T("Test_Drawing: Testing RoundRect...\r\n"));
        obj.test(draw.roundrect());
    }

    if (RANDOM_CHOICE)
    {
        LogComment(_T("Test_Drawing: Testing Polygon...\r\n"));
        obj.test(draw.polygon());
    }

    if (RANDOM_CHOICE)
    {
        LogComment(_T("Test_Drawing: Testing Pixel...\r\n"));
        obj.test(draw.pixel());
    }

    if (RANDOM_CHOICE)
    {
        LogComment(_T("Test_Drawing: Testing Icon...\r\n"));
        obj.test(draw.icon());
    }

    if (RANDOM_CHOICE)
    {
        LogComment(_T("Test_Drawing: Testing GradientFill...\r\n"));
        obj.test(draw.gradientfill());
    }

done:
    return obj.status();
}


/*

@func:    Test_Text, tests various text APIs

@rdesc:    stress return status

@param:    [in] HWND hWnd: handle to the main window

@fault:    none

@pre:    none

@post:    none

@note:    none

*/

DWORD Test_Text(HWND hWnd)
{
    _stressrun_t obj(L"Test_Text");
    _stressfont_t text(g_hInstance, hWnd, &obj);

    if (CESTRESS_FAIL == obj.status())
    {
        goto done;
    }

    if (RANDOM_CHOICE)
    {
        LogComment(_T("Test_Text: Testing DrawText...\r\n"));
        obj.test(text.drawtext());
    }

    if (RANDOM_CHOICE)
    {
        LogComment(_T("Test_Text: Testing ExtTextOut...\r\n"));
        obj.test(text.exttextout());
    }

done:
    return obj.status();
}


/*

@func:    Test_Region_Clip, tests various region and clipping APIs

@rdesc:    stress return status

@param:    [in] HWND hWnd: handle to the main window

@fault:    none

@pre:    none

@post:    none

@note:    none

*/

DWORD Test_Region_Clip(HWND hWnd)
{
    _stressrun_t obj(L"Test_Region_Clip");
    _stressrgnclip_t rgnclip(g_hInstance, hWnd, &obj);

    if (CESTRESS_FAIL == obj.status())
    {
        goto done;
    }

    if (RANDOM_CHOICE)
    {
        LogComment(_T("Test_Region_Clip: Testing...\r\n"));
        obj.test(rgnclip.test());
    }

done:
    return obj.status();
}


/*

@func:    Test_Miscellaneous, tests other GDI APIs not covered in the other tests

@rdesc:    stress return status

@param:    [in] HWND hWnd: handle to the main window

@fault:    none

@pre:    none

@post:    none

@note:    none

*/

DWORD Test_Miscellaneous(HWND hWnd)
{
    _stressrun_t obj(L"Test_Miscellaneous");

    HDC hDC = GetDC(hWnd);

    if (!hDC)
    {
        LogFail(_T("#### Test_Miscellaneous - GetDC failed - Error: 0x%x ####\r\n"),
            GetLastError());
        obj.fail();
        return obj.status();
    }

    //__________________________________________________________________________

    if (RANDOM_CHOICE)    // DC APIs
    {
        COLORREF cr;
        INT nDeltaX = 0, nDeltaY = 0;
        HBRUSH hBrush = NULL;
        RECT rcFilled;
        RECT rcRandom;
        RECT rc;

        if (!SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0))
        {
            LogWarn1(_T("#### Test_Miscellaneous - SystemParametersInfo(SPI_GETWORKAREA) failed - Error: 0x%x ####\r\n"),
                GetLastError());
            obj.warning1();
            SetRect(&rc, 0, 0, GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN));
        }

        // generate a random rectangle

        if (!RandomRect(&rcRandom, &rc))
        {
            LogFail(_T("#### Test_Miscellaneous - RandomRect failed - Error: 0x%x ####\r\n"),
                GetLastError());
            obj.fail();
            return obj.status();
        }

        const INT MAX_ITERATION = RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN);

        for (INT i = 0; i < MAX_ITERATION; i++)
        {
            cr = _COLORREF_RANDOM_;
            DumpRGB(cr);
            nDeltaX = _DELTA_RANDOM_;
            nDeltaY = _DELTA_RANDOM_;

            hBrush = NULL;

            LogVerbose(_T("Test_Miscellaneous - Using solid brush...\r\n"));

            hBrush = CreateSolidBrush(cr);

            if (!hBrush)
            {
                LogFail(_T("#### Test_Miscellaneous - CreateSolidBrush failed - Error: 0x%x ####\r\n"),
                    GetLastError());
                obj.fail();
                continue;
            }

            if (!SaveDC(hDC))
            {
                LogFail(_T("#### Test_Miscellaneous - SaveDC failed - Error: 0x%x ####\r\n"),
                    GetLastError());
                obj.fail();
            }

            SelectObject(hDC, hBrush);

            SetRect(&rcFilled, (rcRandom.left + nDeltaX), (rcRandom.top + nDeltaY),
                (rcRandom.right + nDeltaX), (rcRandom.bottom + nDeltaY));

            LogVerbose(_T("Test_Miscellaneous - Calling FillRect...\r\n"));

            if (!FillRect(hDC, &rcFilled, hBrush))
            {
                LogFail(_T("#### Test_Miscellaneous - FillRect #%i failed - Error: 0x%x ####\r\n"),
                    i, GetLastError());
                DumpFailRECT(rcFilled);
                DumpFailRGB(cr);
                LogFail(_T("Test_Miscellaneous - nDeltaX: %d, nDeltaY: %d\r\n"),
                    nDeltaX, nDeltaY);
                obj.fail();
            }

            if (!RestoreDC(hDC, -1))
            {
                LogFail(_T("#### Test_Miscellaneous - RestoreDC failed - Error: 0x%x ####\r\n"),
                    GetLastError());
                obj.fail();
            }

            if (!DeleteObject(hBrush))
            {
                LogFail(_T("#### Test_Miscellaneous - DeleteObject failed - Error: 0x%x ####\r\n"),
                    GetLastError());
                obj.fail();
            }
        }
    }

    //__________________________________________________________________________

    if (RANDOM_CHOICE)    // Other APIs
    {
        const INT BMP_CX = RANDOM_INT( 0x10, 0x1), BMP_CY = RANDOM_INT( 0x10, 0x1);

        // CreateBitmap

        LogVerbose(_T("Test_Miscellaneous - Calling CreateBitmap...\r\n"));

        HBITMAP hBmp1 = CreateBitmap(BMP_CX, BMP_CY, 1, RANDOM_BITDEPTH, NULL);

        if (!hBmp1)
        {
            LogFail(_T("#### Test_Miscellaneous - CreateBitmap failed - Error: 0x%x ####\r\n"),
                GetLastError());
            obj.fail();
        }
        else
        {
            if (!DeleteObject(hBmp1))
            {
                LogFail(_T("#### Test_Miscellaneous - DeleteObject(hBmp1) failed - Error: 0x%x ####\r\n"),
                    GetLastError());
                obj.fail();
            }
        }

        // CreateCompatibleBitmap

        LogVerbose(_T("Test_Miscellaneous - Calling CreateCompatibleBitmap...\r\n"));

        HBITMAP hBmp2 = CreateCompatibleBitmap(hDC, BMP_CX, BMP_CY);

        if (!hBmp2)
        {
            LogFail(_T("#### Test_Miscellaneous - CreateCompatibleBitmap failed - Error: 0x%x ####\r\n"),
                GetLastError());
            obj.fail();
        }
        else
        {
            if (!DeleteObject(hBmp2))
            {
                LogFail(_T("#### Test_Miscellaneous - DeleteObject(hBmp2) failed - Error: 0x%x ####\r\n"),
                    GetLastError());
                obj.fail();
            }
        }
    }

    if (hDC && !ReleaseDC(hWnd, hDC))
    {
        LogFail(_T("#### Test_Miscellaneous - ReleaseDC(hDC) failed - Error: 0x%x ####\r\n"),
            GetLastError());
        obj.fail();
    }

    return obj.status();
}



/*

@func	Validates that the given window handle is valid and belongs to the
current process.

*/

static inline bool IsValidLocalWindow(HWND hwnd)
{
    if (IsWindow(hwnd))
    {
        DWORD dwWndProcessID = 0;

        if(GetWindowThreadProcessId(hwnd, &dwWndProcessID))
        {
            if (GetCurrentProcessId() == dwWndProcessID)
            {
                return true;
            }
        }
    }

    return false;
}


/*

@func Pulls pending messages off of the current thread's message queue and
dispatches them. This function is useful for UI automation or other synchronous
apps that create windows or cause message traffic to be generated on the same
thread in which results of these actions are to be observed. MessagePump() will
allow those messages to be dispatched and processed, allowing the thread to
verify the results of that processing.

*/

void FAR PASCAL MessagePump()
{
    for (INT iLoop = 0; iLoop < 5; iLoop++)
    {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            // Do not pump WM_PAINT messages for invalid and non-local windows
            if ((WM_PAINT == msg.message) && !IsValidLocalWindow(msg.hwnd))
            {
                LogWarn2(_T("### MessagePump - WARNING - WM_PAINT for invalid window %x in our queue\r\n"), msg.hwnd);
                return;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        Sleep(10);
    }

    return;
}

