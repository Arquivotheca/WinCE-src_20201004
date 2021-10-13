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
#include <windows.h>
#include <stressutils.h>
#include "resource.h"
#include "stressrun.h"
#include "gdistress.h"


_stressblt_t::_stressblt_t(HINSTANCE hInstance, HWND hWnd, _stressrun_t *psr) : m_Surfaces(NULL), m_hinstCoreDLL(NULL), hMask(NULL)
{
    _hInstance = hInstance;

    if (psr)
    {
        _psr = psr;
    }

    // window

    if (!IsWindow(hWnd))
    {
        LogFail(_T("#### _stressblt_t::_stressblt_t - IsWindow failed - Error: 0x%x ####\r\n"),
            GetLastError());
        _psr->fail();
        return;
    }

    _hWnd = hWnd;

    if (!GetClientRect(hWnd, &_rc))
    {
        LogFail(_T("#### _stressblt_t::_stressblt_t - GetClientRect failed - Error: 0x%x ####\r\n"),
            GetLastError());
        _psr->fail();
        return;
    }

    // Verify a window with proper dimensions was created. Quit early to avoid divide by zero
    // exceptions caused by RANDOM_WIDTH(_rc, 0) and RANDOM_POSWIDTH(_rc, 0) calls.
    if (RECT_WIDTH(_rc) == 0 || RECT_HEIGHT(_rc) == 0)
    {
        LogFail(_T("#### _stressblt_t::_stressblt_t - window size is too small to continue ####\r\n"));
        _psr->fail();
        return;
    }

    // DC's
    m_Surfaces = new _surfaces(hWnd, _rc, hInstance, psr);

    if (!m_Surfaces)
    {
        LogFail(_T("#### _stressblt_t::_stressblt_t - Surface handler allocation failed 0x%x ####\r\n"),
            GetLastError());
        _psr->fail();
        return;
    }

    // Mask Bitmap

    wMaskID = RANDOM_INT(MASK_LAST, MASK_FIRST);

    LogVerbose(_T("_stressblt_t::_stressblt_t - Calling LoadBitmap(Resource ID: %d)...\r\n"), wMaskID);

    hMask = LoadBitmap(_hInstance, MAKEINTRESOURCE(wMaskID));

    if (!hMask)
    {
        LogFail(_T("#### _stressblt_t::_stressblt_t - LoadBitmap(%d) failed - Error: 0x%x ####\r\n"),
            wMaskID, GetLastError());
        _psr->fail();
        return;
    }

    // function pointers
    m_hinstCoreDLL = LoadLibrary(TEXT("\\coredll.dll"));

    if(!m_hinstCoreDLL)
    {
        LogFail(_T("#### _stressblt_t::_stressblt_t - LoadLibrary for Coredll.dll failed - Error: 0x%x ####\r\n"),
            GetLastError());
        _psr->fail();
        return;
    }

    // this may fail, indicating that GWES doesn't support alpha blending.
    pfnAlphaBlend = (PFNALPHABLEND) GetProcAddress(m_hinstCoreDLL, TEXT("AlphaBlend"));
}

_stressblt_t::~_stressblt_t()
{
    // only delete objects if the allocations succeeded, otherwise the failed allocations were flagged
    // at the allocation.
    if(m_Surfaces)
        delete m_Surfaces;

    if(m_hinstCoreDLL)
        FreeLibrary(m_hinstCoreDLL);

    if(hMask)
    {
        if (!DeleteObject(hMask))
        {
            LogFail(_T("#### _stressblt_t::~_stressblt_t - DeleteObject(hMask: 0x%x) failed - Error: 0x%x ####\r\n"),
                hMask, GetLastError());
            _psr->fail();
        }
    }
}


DWORD _stressblt_t::bitblt()
{
    _stressrun_t obj(L"_stressblt_t::bitblt");
    INT i = RANDOM_ROP3INDEX;
    HDC hSrc, hDest;

    hSrc = m_Surfaces->GetRandomSurface(FALSE);
    hDest = m_Surfaces->GetRandomSurface(TRUE);

    LogVerbose(_T("_stressblt_t::bitblt - Calling BitBlt(%s)...\r\n"),
        g_nvRop3Array[i].lpszROP);

    for(int loop = 0; loop < LOOPCOUNT_MAX; loop++)
    {
        SetLastError(ERROR_SUCCESS);

        if (!::BitBlt(hDest, RANDOM_WIDTH(_rc, 20), RANDOM_HEIGHT(_rc, 20), RANDOM_WIDTH(_rc, 20), RANDOM_HEIGHT(_rc, 20),
            hSrc, RANDOM_WIDTH(_rc, 20), RANDOM_HEIGHT(_rc, 20), g_nvRop3Array[i].dwROP))
        {
            if(GetLastError() == ERROR_NOT_ENOUGH_MEMORY)
            {
                LogWarn1(_T("_stressblt_t::bitblt - insufficient memory.\r\n"));
                obj.warning1();
            }
            else
            {
                LogFail(_T("#### _stressblt_t::bitblt - BitBlt(%s) failed - Error: 0x%x ####\r\n"),
                    g_nvRop3Array[i].lpszROP, GetLastError());
                obj.fail();
            }
        }
    }
    m_Surfaces->ReleaseSurface(hSrc);
    m_Surfaces->ReleaseSurface(hDest);

    return obj.status();
}


DWORD _stressblt_t::stretchblt()
{
    _stressrun_t obj(L"_stressblt_t::StretchBlt");
    INT i = RANDOM_ROP3INDEX;
    HDC hSrc, hDest;

    hSrc = m_Surfaces->GetRandomSurface(FALSE);
    hDest = m_Surfaces->GetRandomSurface(TRUE);

    LogVerbose(_T("_stressblt_t::bitblt - Calling StretchBlt(%s)...\r\n"),
        g_nvRop3Array[i].lpszROP);

    for(int loop = 0; loop < LOOPCOUNT_MAX; loop++)
    {
        SetLastError(ERROR_SUCCESS);

        SetStretchBltMode(hDest, RANDOM_CHOICE?BLACKONWHITE:BILINEAR);
        if (!::StretchBlt(hDest, RANDOM_WIDTH(_rc, 20), RANDOM_HEIGHT(_rc, 20), RANDOM_WIDTH(_rc, 20), RANDOM_HEIGHT(_rc, 20),
            hSrc, RANDOM_WIDTH(_rc, 20), RANDOM_HEIGHT(_rc, 20), RANDOM_WIDTH(_rc, 20), RANDOM_HEIGHT(_rc, 20), g_nvRop3Array[i].dwROP))
        {
            if(GetLastError() == ERROR_NOT_ENOUGH_MEMORY)
            {
                LogWarn1(_T("_stressblt_t::stretchblt - insufficient memory.\r\n"));
                obj.warning1();
            }
            else
            {
                LogFail(_T("#### _stressblt_t::stretchblt - StretchBlt(%s) failed - Error: 0x%x ####\r\n"),
                    g_nvRop3Array[i].lpszROP, GetLastError());
                obj.fail();
            }
        }
    }
    m_Surfaces->ReleaseSurface(hSrc);
    m_Surfaces->ReleaseSurface(hDest);

    return obj.status();
}


DWORD _stressblt_t::transparentimage()

{
    _stressrun_t obj(L"_stressblt_t::transparentimage");

    COLORREF crTransparentColor;
    HDC hSrc, hDest;

    hSrc = m_Surfaces->GetRandomSurface(FALSE);
    hDest = m_Surfaces->GetRandomSurface(TRUE);

    crTransparentColor = _COLORREF_RANDOM_;
    DumpRGB(crTransparentColor);

    LogVerbose(_T("_stressblt_t::transparentimage - Calling TransparentImage...\r\n"));

    for(int loop = 0; loop < LOOPCOUNT_MAX; loop++)
    {
        SetLastError(ERROR_SUCCESS);

        if (!::TransparentImage(hDest, RANDOM_WIDTH(_rc, 20), RANDOM_HEIGHT(_rc, 20), RANDOM_WIDTH(_rc, 20), RANDOM_HEIGHT(_rc, 20),
            hSrc, RANDOM_WIDTH(_rc, 20), RANDOM_HEIGHT(_rc, 20), RANDOM_WIDTH(_rc, 20), RANDOM_HEIGHT(_rc, 20), crTransparentColor))
        {
            if(GetLastError() == ERROR_NOT_ENOUGH_MEMORY)
            {
                LogWarn1(_T("_stressblt_t::transparentimage - insufficient memory.\r\n"));
                obj.warning1();
            }
            else
            {
                LogFail(_T("#### _stressblt_t::transparentimage - TransparentImage failed - Error: 0x%x ####\r\n"),
                    GetLastError());
                DumpFailRGB(crTransparentColor);
                obj.fail();
            }
        }
    }
    m_Surfaces->ReleaseSurface(hSrc);
    m_Surfaces->ReleaseSurface(hDest);

    return obj.status();
}

DWORD _stressblt_t::maskblt()
{
    _stressrun_t obj(L"_stressblt_t::maskblt");
    INT i = RANDOM_ROP3INDEX;
    INT j = RANDOM_ROP3INDEX;
    HDC hSrc, hDest;
    BITMAP bm;

    if (!GetObject(hMask, sizeof BITMAP, &bm))
    {
        LogFail(_T("#### _stressblt_t::maskblt - GetObject failed - Error: 0x%x ####\r\n"),
        GetLastError());
        obj.fail();
        goto done;
    }

    hSrc = m_Surfaces->GetRandomSurface(FALSE);
    hDest = m_Surfaces->GetRandomSurface(TRUE);

    LogVerbose(_T("_stressblt_t::maskblt - Calling MaskBlt(MAKEROP4(%s,%s))...\r\n"),
        g_nvRop3Array[i].lpszROP, g_nvRop3Array[j].lpszROP);

    SetLastError(ERROR_SUCCESS);

    if (!::MaskBlt(hDest, 0, 0, bm.bmWidth, bm.bmHeight,
        hSrc, 0, 0, hMask, 0, 0, MAKEROP4(g_nvRop3Array[i].dwROP, g_nvRop3Array[j].dwROP)))
    {
        if(GetLastError() == ERROR_NOT_ENOUGH_MEMORY)
        {
                LogWarn1(_T("_stressblt_t::maskblt - insufficient memory.\r\n"));
                obj.warning1();
        }
        else
        {
            LogFail(_T("#### _stressblt_t::maskblt - MaskBlt(MAKEROP4(%s,%s)) failed - Error: 0x%x ####\r\n"),
                g_nvRop3Array[i].lpszROP, g_nvRop3Array[j].lpszROP, GetLastError());
            obj.fail();
        }
    }

    m_Surfaces->ReleaseSurface(hSrc);
    m_Surfaces->ReleaseSurface(hDest);

done:
    return obj.status();
}


DWORD _stressblt_t::patblt()
{
    _stressrun_t obj(L"_stressblt_t::patblt");

    // PatBlt has it's own ROP codes

    ROP_GUIDE rgROP[] = {
        NAMEVALENTRY(PATCOPY),    /* copies the specified pattern into the
        destination bitmap */
        NAMEVALENTRY(PATINVERT),    /* combines the colors of the specified
        pattern with the  colors of the destination rectangle by using the
        boolean OR operator */
        NAMEVALENTRY(DSTINVERT),    // inverts the destination rectangle
        NAMEVALENTRY(BLACKNESS),    /* fills the destination rectangle using the
        color associated with index 0 in the physical palette. (black for the
        default physical palette.) */
        NAMEVALENTRY(WHITENESS) /* fills the destination rectangle using the
        color associated with index 1 in the physical palette. (white for the
        default physical palette.) */
    };

    INT i = rand() % ARRAY_SIZE(rgROP);
    HDC hDest;

    hDest = m_Surfaces->GetRandomSurface(TRUE);

    LogVerbose(_T("_stressblt_t::patblt - Calling PatBlt(%s)...\r\n"),
        rgROP[i].lpszROP);

    for(int loop = 0; loop < LOOPCOUNT_MAX; loop++)
    {
        SetLastError(ERROR_SUCCESS);

        if (!::PatBlt(hDest, RANDOM_WIDTH(_rc, 0), RANDOM_HEIGHT(_rc, 0),
            RANDOM_WIDTH(_rc, 0), RANDOM_HEIGHT(_rc, 0), (DWORD)rgROP[i].dwROP))
        {
            if(GetLastError() == ERROR_NOT_ENOUGH_MEMORY)
            {
                LogWarn1(_T("_stressblt_t::patblt - insufficient memory.\r\n"));
                obj.warning1();
            }
            else
            {
                LogFail(_T("#### _stressblt_t::patblt - PatBlt(%s) failed - Error: 0x%x ####\r\n"),
                    rgROP[i].lpszROP, GetLastError());
                obj.fail();
            }
        }
    }
    m_Surfaces->ReleaseSurface(hDest);

    return obj.status();
}

DWORD _stressblt_t::alphablend()
{
    _stressrun_t obj(L"_stressblt_t::alphablend");

    if(pfnAlphaBlend)
    {
        BLENDFUNCTION bf;
        HDC hSrc, hDest;
        HBITMAP hbmp;
        BITMAP bmp;
        int nWidthAvail, nHeightAvail;
        int nSrcTopCoordinate, nSrcLeftCoordinate, nSrcWidth, nSrcHeight;

        hSrc = m_Surfaces->GetRandomSurface(FALSE);
        hDest = m_Surfaces->GetRandomSurface(TRUE);

        memset(&bmp, 0, sizeof(BITMAP));
        hbmp = (HBITMAP) GetCurrentObject(hSrc, OBJ_BITMAP);
        if(hbmp)
        {
            // if there's a bitmap associated with the source,
            // retrieve the size of it also because AlphaBlend fails
            // if you specify a source rectangle that's outside of the surface.
            GetObject(hbmp, sizeof(BITMAP), &bmp);
            nWidthAvail = bmp.bmWidth;
            nHeightAvail = bmp.bmHeight;
        }
        else
        {
            nWidthAvail = RECT_WIDTH(_rc);
            nHeightAvail = RECT_HEIGHT(_rc);
        }

        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = RANDOM_INT(0xFF, 0x0);
        bf.AlphaFormat = RANDOM_CHOICE?0:AC_SRC_ALPHA;

        LogVerbose(_T("_stressblt_t::alphablend - Calling AlphaBlend...\r\n"));

        for(int loop = 0; loop < LOOPCOUNT_MAX; loop++)
        {
            // somewhere within the source surface
            nSrcTopCoordinate = rand() % (nHeightAvail - 1);
            nSrcLeftCoordinate = rand() % (nWidthAvail - 1);
            nSrcWidth = rand() % (nWidthAvail - nSrcLeftCoordinate);
            nSrcHeight = rand() % (nHeightAvail - nSrcTopCoordinate);

            // this call may fail indicating that alphablend isn't supported in the display driver.
            // Source must be completely on the surface, source and dest must both have a positive width/height.
            // the destination coordinates don't matter since they'll just get clipped
            SetLastError(ERROR_SUCCESS);

            if (!pfnAlphaBlend(hDest, RANDOM_WIDTH(_rc, 20), RANDOM_HEIGHT(_rc, 20),
                                        RANDOM_POSWIDTH(_rc, 0), RANDOM_POSHEIGHT(_rc, 0),
                                        hSrc, nSrcLeftCoordinate, nSrcTopCoordinate, nSrcWidth, nSrcHeight, bf))
                if(ERROR_NOT_SUPPORTED != GetLastError())
                {
                    if(GetLastError() == ERROR_NOT_ENOUGH_MEMORY)
                    {
                        LogWarn1(_T("_stressblt_t::alphablend - insufficient memory.\r\n"));
                        obj.warning1();
                    }
                    else
                    {
                        // source alpha is only valid on 32bpp
                        // if we have a source alpha and are non 32bpp, we expect an ERROR_INVALID_PARAMETER error.
                        if(!(bf.AlphaFormat == AC_SRC_ALPHA && 32 != bmp.bmBitsPixel && ERROR_INVALID_PARAMETER == GetLastError()))
                        {
                            LogFail(_T("#### _stressblt_t::alphablend - AlphaBlend failed - Error: 0x%x ####\r\n"),
                            GetLastError());
                            obj.fail();
                        }
                    }
                }
        }
        m_Surfaces->ReleaseSurface(hSrc);
        m_Surfaces->ReleaseSurface(hDest);
    }

    return obj.status();
}

