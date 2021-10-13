//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <stressutils.h>
#include "resource.h"
#include "stressrun.h"
#include "gdistress.h"


_stressdraw_t::_stressdraw_t(HINSTANCE hInstance, HWND hWnd, _stressrun_t *psr) : m_Surfaces(NULL), m_hinstCoreDLL(NULL)
{
    _hInstance = hInstance;

    if (psr)
    {
        _psr = psr;
    }

    // window

    if (!IsWindow(hWnd))
    {
        LogFail(_T("#### _stressdraw_t::_stressdraw_t - IsWindow failed - Error: 0x%x ####\r\n"),
            GetLastError());
        _psr->fail();
        return;
    }

    _hWnd = hWnd;

    if (!GetClientRect(hWnd, &_rc))
    {
        LogFail(_T("#### _stressdraw_t::_stressdraw_t - GetClientRect failed - Error: 0x%x ####\r\n"),
            GetLastError());
        _psr->fail();
        return;
    }

    m_Surfaces = new _surfaces(hWnd, _rc, hInstance, psr);

    if(!m_Surfaces)
    {
        LogFail(_T("#### _stressdraw_t::_stressdraw_t - Failed to allocate surfaces class - Error: 0x%x ####\r\n"),
            GetLastError());
        _psr->fail();
        return;
    }

    // function pointers
    m_hinstCoreDLL = LoadLibrary(TEXT("\\coredll.dll"));

    if(!m_hinstCoreDLL)
    {
        LogFail(_T("#### _stressdraw_t::_stressdraw_t - LoadLibrary for Coredll.dll failed - Error: 0x%x ####\r\n"),
            GetLastError());
        _psr->fail();
        return;
    }

    // this may fail, indicating that GWES doesn't support gradient fill.
    pfnGradientFill = (PFNGRADIENTFILL) GetProcAddress(m_hinstCoreDLL, TEXT("GradientFill"));
}


_stressdraw_t::~_stressdraw_t()
{
    if(m_hinstCoreDLL)
        FreeLibrary(m_hinstCoreDLL);

    if(m_Surfaces)
        delete m_Surfaces;
}


DWORD _stressdraw_t::rectangle()
{
    _stressrun_t obj(L"_stressdraw_t::rectangle");
    INT nDeltaX = 0, nDeltaY = 0;
    RECT rcRandom;
    HDC hDest;

    if (!RandomRect(&rcRandom, &_rc))
    {
        LogFail(_T("#### _stressdraw_t::rectangle - RandomRect failed - Error: 0x%x ####\r\n"),
            GetLastError());
        obj.fail();
        return obj.status();
    }
        
    // generate a random rectangle
    const INT MAX_ITERATION = RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN);

    for (INT i = 0; i < MAX_ITERATION; i++)
    {
        // the surface created 
        hDest = m_Surfaces->GetRandomSurface(TRUE);

        nDeltaX = _DELTA_RANDOM_;
        nDeltaY = _DELTA_RANDOM_;

        LogVerbose(_T("_stressdraw_t::rectangle - Calling Rectangle...\r\n"));

        if (!Rectangle(hDest, (rcRandom.left + nDeltaX), (rcRandom.top + nDeltaY),
            (rcRandom.right + nDeltaX), (rcRandom.bottom + nDeltaY)))
        {
            LogFail(_T("#### _stressdraw_t::rectangle - Rectangle #%i failed - Error: 0x%x ####\r\n"),
                i, GetLastError());
            DumpFailRECT(rcRandom);
            LogFail(_T("_stressdraw_t::rectangle - nDeltaX: %d, nDeltaY: %d\r\n"),
                nDeltaX, nDeltaY);
            obj.fail();
        }

        m_Surfaces->ReleaseSurface(hDest);
    }

    return obj.status();
}


DWORD _stressdraw_t::fillrect()
{
    _stressrun_t obj(L"_stressdraw_t::fillrect");
    INT nDeltaX = 0, nDeltaY = 0;
    RECT rcFilled;
    RECT rcRandom;
    HDC hDest;
    HBRUSH hBrush;

    // generate a random rectangle

    if (!RandomRect(&rcRandom, &_rc))
    {
        LogFail(_T("#### _stressdraw_t::fillrect - RandomRect failed - Error: 0x%x ####\r\n"),
            GetLastError());
        obj.fail();
        return obj.status();
    }

    const INT MAX_ITERATION = RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN);

    for (INT i = 0; i < MAX_ITERATION; i++)
    {
        hDest = m_Surfaces->GetRandomSurface(TRUE);
        int nTimeout = 100;
        do
        {
            nTimeout--;
            hBrush = m_Surfaces->CreateRandomBrush();
        }while(!hBrush && nTimeout);

        if(hBrush)
        {
            nDeltaX = _DELTA_RANDOM_;
            nDeltaY = _DELTA_RANDOM_;

            SetRect(&rcFilled, (rcRandom.left + nDeltaX), (rcRandom.top + nDeltaY),
                (rcRandom.right + nDeltaX), (rcRandom.bottom + nDeltaY));

            LogVerbose(_T("_stressdraw_t::fillrect - Calling FillRect...\r\n"));

            SetLastError(ERROR_SUCCESS);

            if (!FillRect(hDest, &rcFilled, hBrush))
            {
                if(GetLastError() == ERROR_NOT_ENOUGH_MEMORY)
                {
                    LogWarn1(_T("_stressdraw_t::fillrect - insufficient memory.\r\n"));
                    obj.warning1();
                }
                else
                {
                    LogFail(_T("#### _stressdraw_t::fillrect - FillRect #%i failed - Error: 0x%x ####\r\n"),
                        i, GetLastError());
                    DumpFailRECT(rcFilled);
                    LogFail(_T("_stressdraw_t::fillrect - nDeltaX: %d, nDeltaY: %d\r\n"),
                        nDeltaX, nDeltaY);
                    obj.fail();
                }
            }

            DeleteObject(hBrush);
        }
        // appropriate brush creation failure messages handle by CreateRandomBrush.

        m_Surfaces->ReleaseSurface(hDest);
    }

    return obj.status();
}


DWORD _stressdraw_t::ellipse()
{
    _stressrun_t obj(L"_stressdraw_t::ellipse");
    INT nDeltaX = 0, nDeltaY = 0;
    RECT rcRandom;
    HDC hDest;

    // generate a random rectangle

    if (!RandomRect(&rcRandom, &_rc))
    {
        LogFail(_T("#### _stressdraw_t::ellipse - RandomRect failed - Error: 0x%x ####\r\n"),
            GetLastError());
        obj.fail();
        return obj.status();
    }

    const INT MAX_ITERATION = RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN);

    for (INT i = 0; i < MAX_ITERATION; i++)
    {
        hDest = m_Surfaces->GetRandomSurface(TRUE);

        nDeltaX = _DELTA_RANDOM_;
        nDeltaY = _DELTA_RANDOM_;

        LogVerbose(_T("_stressdraw_t::ellipse - Calling Ellipse...\r\n"));

        SetLastError(ERROR_SUCCESS);

        if (!Ellipse(hDest, (rcRandom.left + nDeltaX), (rcRandom.top + nDeltaY),
            (rcRandom.right + nDeltaX), (rcRandom.bottom + nDeltaY)))
        {
            if(GetLastError() == ERROR_NOT_ENOUGH_MEMORY)
            {
                LogWarn1(_T("_stressdraw_t::ellipse - insufficient memory.\r\n"));
                obj.warning1();
            }
            else
            {
                LogFail(_T("#### _stressdraw_t::ellipse - Ellipse #%i failed - Error: 0x%x ####\r\n"),
                    i, GetLastError());
                DumpFailRECT(rcRandom);
                LogFail(_T("_stressdraw_t::ellipse - nDeltaX: %d, nDeltaY: %d\r\n"),
                    nDeltaX, nDeltaY);
                obj.fail();
            }
        }

        m_Surfaces->ReleaseSurface(hDest);
    }

    return obj.status();
}


DWORD _stressdraw_t::roundrect()
{
    _stressrun_t obj(L"_stressdraw_t::roundrect");

    INT nDeltaX = 0, nDeltaY = 0;
    RECT rcRandom;
    INT nEllipseWidth = 0, nEllipseHeight = 0;
    HDC hDest;

    // generate a random rectangle

    if (!RandomRect(&rcRandom, &_rc))
    {
        LogFail(_T("#### _stressdraw_t::roundrect - RandomRect failed - Error: 0x%x ####\r\n"),
            GetLastError());
        obj.fail();
        return obj.status();
    }

    const INT MAX_ITERATION = RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN);

    for (INT i = 0; i < MAX_ITERATION; i++)
    {
        hDest = m_Surfaces->GetRandomSurface(TRUE);

        nDeltaX = _DELTA_RANDOM_;
        nDeltaY = _DELTA_RANDOM_;
        nEllipseWidth = _ELLIPSE_WIDTH_RANDOM_;
        nEllipseHeight = _ELLIPSE_HEIGHT_RANDOM_;


        LogVerbose(_T("_stressdraw_t::roundrect - Calling RoundRect...\r\n"));

        SetLastError(ERROR_SUCCESS);

        if (!RoundRect(hDest, (rcRandom.left + nDeltaX), (rcRandom.top + nDeltaY),
            (rcRandom.right + nDeltaX), (rcRandom.bottom + nDeltaY), nEllipseWidth,
            nEllipseHeight))
        {
            if(GetLastError() == ERROR_NOT_ENOUGH_MEMORY)
            {
                LogWarn1(_T("_stressdraw_t::roundrect - insufficient memory.\r\n"));
                obj.warning1();
            }
            else
            {
                LogFail(_T("#### _stressdraw_t::roundrect - RoundRect #%i failed - Error: 0x%x ####\r\n"),
                    i, GetLastError());
                DumpFailRECT(rcRandom);
                LogFail(_T("_stressdraw_t::roundrect - nDeltaX: %d, nDeltaY: %d, nEllipseWidth: %d, nEllipseHeight: %d\r\n"),
                    nDeltaX, nDeltaY, nEllipseWidth, nEllipseHeight);
                obj.fail();
            }
        }
        m_Surfaces->ReleaseSurface(hDest);
    }

    return obj.status();
}


DWORD _stressdraw_t::polygon()
{
    _stressrun_t obj(L"_stressdraw_t::polygon");

    INT nDeltaX = 0, nDeltaY = 0;
    POINT rgpt[LOOPCOUNT_MAX];
    POINT rgptShaken[LOOPCOUNT_MAX];
    HDC hDest;
    const INT POINT_COUNT = RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN);

    memset(rgpt, 0, sizeof rgpt);

    // generate random points for the polygon

    for (INT iPoint = 0; iPoint < POINT_COUNT; iPoint++)
    {
        rgpt[iPoint] = RandomPoint(&_rc);
    }

    const INT MAX_ITERATION = RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN);

    for (INT i = 0; i < MAX_ITERATION; i++)
    {
        hDest = m_Surfaces->GetRandomSurface(TRUE);

        nDeltaX = _DELTA_RANDOM_;
        nDeltaY = _DELTA_RANDOM_;

        memset(rgptShaken, 0, sizeof rgptShaken);

        for (iPoint = 0; iPoint < POINT_COUNT; iPoint++)
        {
            rgptShaken[iPoint] = rgpt[iPoint];
            rgptShaken[iPoint].x += nDeltaX;
            rgptShaken[iPoint].y += nDeltaY;
        }

        LogVerbose(_T("_stressdraw_t::polygon - Calling Polygon...\r\n"));

        SetLastError(ERROR_SUCCESS);

        if (!Polygon(hDest, rgptShaken, POINT_COUNT))
        {
            if(GetLastError() == ERROR_NOT_ENOUGH_MEMORY)
            {
                LogWarn1(_T("_stressdraw_t::polygon - insufficient memory.\r\n"));
                obj.warning1();
            }
            else
            {
                LogFail(_T("#### _stressdraw_t::polygon - Polygon #%i failed - Error: 0x%x ####\r\n"),
                    i, GetLastError());

                for (iPoint = 0; iPoint < POINT_COUNT; iPoint++)
                {
                    LogFail(_T("_stressdraw_t::polygon - pt.x: %d, pt.y: %d\r\n"),
                        rgptShaken[iPoint].x, rgptShaken[iPoint].y);
                }
                LogFail(_T("_stressdraw_t::polygon - nDeltaX: %d, nDeltaY: %d\r\n"),
                    nDeltaX, nDeltaY);
                obj.fail();
            }
        }
        m_Surfaces->ReleaseSurface(hDest);
    }

    return obj.status();
}


DWORD _stressdraw_t::pixel()
{
    _stressrun_t obj(L"_stressdraw_t::pixel");

    POINT pt;
    COLORREF cr;
    HDC hDest;

    hDest = m_Surfaces->GetRandomSurface(TRUE);

    const INT MAX_ITERATION = RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN) *
        RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN);

    for (INT i = 0; i < MAX_ITERATION; i++)
    {
        cr = _COLORREF_RANDOM_;
        DumpRGB(cr);

        pt = RandomPoint(&_rc);

        LogVerbose(_T("_stressdraw_t::pixel - Calling SetPixel...\r\n"));

        SetLastError(ERROR_SUCCESS);

        if (-1 == SetPixel(hDest, pt.x, pt.y, cr))
        {
            if(GetLastError() == ERROR_NOT_ENOUGH_MEMORY)
            {
                LogWarn1(_T("_stressdraw_t::SetPixel - insufficient memory.\r\n"));
                obj.warning1();
            }
            else
            {
                LogWarn2(_T("_stressdraw_t::SetPixel - failed %d.\r\n"), GetLastError());
                obj.warning2();
            }
        }
    }

    m_Surfaces->ReleaseSurface(hDest);
    return obj.status();
}


DWORD _stressdraw_t::icon()
{
    _stressrun_t obj(L"_stressdraw_t::icon");

    const MAX_ICON_CX = 48;
    const MAX_ICON_CY = 48;

    HICON hIcon = 0;

    WORD wIconID = ICON_FIRST;

    INT    x = 0;
    INT    y = 0;
    HDC hDest;

    hDest = m_Surfaces->GetRandomSurface(TRUE);

    while (y < RECT_HEIGHT(_rc))
    {
        while(x < RECT_WIDTH(_rc))
        {
            hIcon = NULL;
            wIconID = RANDOM_INT(ICON_LAST, ICON_FIRST);

            // load the icon

            LogVerbose(_T("_stressdraw_t::icon - Calling LoadIcon(Resource ID: %d)...\r\n"), wIconID);

            hIcon = LoadIcon(_hInstance, MAKEINTRESOURCE(wIconID));

            if (!hIcon)
            {
                LogFail(_T("#### _stressdraw_t::icon - LoadIcon(%d) failed - Error: 0x%x ####\r\n"),
                    wIconID, GetLastError());
                obj.fail();
                goto cleanup;
            }

            // draw the icon: DrawIconEx

            LogVerbose(_T("_stressdraw_t::icon - Calling DrawIconEx(Resource ID: %d)...\r\n"), wIconID);

            if (!DrawIconEx(hDest, x, y, hIcon, 0, 0, NULL, 0, DI_NORMAL))
            {
                LogFail(_T("#### _stressdraw_t::icon - DrawIconEx(%d) failed - Error: 0x%x ####\r\n"),
                    wIconID, GetLastError());
                obj.fail();
                goto cleanup;
            }

            LogVerbose(_T("_stressdraw_t::icon - Calling DestroyIcon(Resource ID: %d)...\r\n"), wIconID);

            if (!DestroyIcon(hIcon))
            {
                LogFail(_T("#### _stressdraw_t::icon - DestroyIcon(%d) failed - Error: 0x%x ####\r\n"),
                    wIconID, GetLastError());
                obj.fail();
                goto cleanup;
            }
            hIcon = NULL;

            x += MAX_ICON_CX; // advance x by icon size
        }

        x = 0; // reset x
        y += MAX_ICON_CY; // advance y by the biggest icon width
    }

cleanup:
    if (hIcon)
    {
        if (!DestroyIcon(hIcon))
        {
            LogFail(_T("#### _stressdraw_t::icon - DestroyIcon(%d) failed - Error: 0x%x ####\r\n"),
                wIconID, GetLastError());
            obj.fail();
        }
    }
    m_Surfaces->ReleaseSurface(hDest);

    return obj.status();
}

DWORD _stressdraw_t::gradientfill()
{
    _stressrun_t obj(L"_stressdraw_t::GradientFill");

    HDC hDest;
    HBITMAP hbmp;
    BITMAP bmp;
    BOOL bExpectedReturnValue = TRUE;

    memset(&bmp, 0, sizeof(BITMAP));

    hDest = m_Surfaces->GetRandomSurface(TRUE);

    hbmp = (HBITMAP) GetCurrentObject(hDest, OBJ_BITMAP);

    if(hbmp)
        GetObject(hbmp, sizeof(BITMAP), &bmp);

    for(int j = 0; j < LOOPCOUNT_MAX; j ++)
    {
        TRIVERTEX        vert[LOOPCOUNT_MAX];
        GRADIENT_RECT    gRect[LOOPCOUNT_MAX]; 
        const INT VERTEX_COUNT = RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN);
        const INT RECT_COUNT = RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN);
        int i;

        for(i = 0; i < LOOPCOUNT_MAX; i++)
        {
            gRect[i].UpperLeft = RANDOM_INT(VERTEX_COUNT-1, 0);
            gRect[i].LowerRight = RANDOM_INT(VERTEX_COUNT-1, 0);
        }

        for(i = 0; i < LOOPCOUNT_MAX; i++)
        {
            vert [i] .x      = RANDOM_INT(_rc.right + 20, _rc.left - 20);
            vert [i] .y      = RANDOM_INT(_rc.bottom + 20, _rc.top - 20);
            vert [i] .Red    = RANDOM_BYTE << 8;
            vert [i] .Green  = RANDOM_BYTE << 8;
            vert [i] .Blue   = RANDOM_BYTE << 8;
            vert [i] .Alpha  = RANDOM_BYTE << 8;
        }

        SetLastError(ERROR_SUCCESS);

        // This call may fail, indicating that gradient fill isn't supported in the display driver.
        if(bExpectedReturnValue != pfnGradientFill(hDest, vert, VERTEX_COUNT, gRect, RECT_COUNT, RANDOM_CHOICE?GRADIENT_FILL_RECT_H:GRADIENT_FILL_RECT_V))
            if(!((bmp.bmBitsPixel == 2 && ERROR_INVALID_PARAMETER == GetLastError()) || (ERROR_NOT_SUPPORTED == GetLastError())))
            {
                if(GetLastError() == ERROR_NOT_ENOUGH_MEMORY)
                {
                    LogWarn1(_T("_stressdraw_t::SetPixel - insufficient memory.\r\n"));
                    obj.warning1();
                }
                else
                {
                    LogFail(_T("#### _stressdraw_t::gradientfill - GradientFill failed - Error: 0x%x ####\r\n"),
                        GetLastError());
                    obj.fail();
                    goto done;
                }
            }
    }

done:
    m_Surfaces->ReleaseSurface(hDest);
    return obj.status();
}

