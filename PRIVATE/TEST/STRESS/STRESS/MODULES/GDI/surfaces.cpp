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

HPEN _surfaces::CreateRandomPen()
{
    HPEN hpenToReturn = NULL;
    LOGPEN lp;

    lp.lopnStyle = RANDOM_PENSTYLE;
    lp.lopnWidth.x = _LINE_WIDTH_RANDOM_;
    lp.lopnWidth.y = _LINE_WIDTH_RANDOM_;
    lp.lopnColor = _COLORREF_RANDOM_;
    int nCase = RANDOM_INT(5, 0);

    switch(nCase)
    {
        case 0:
            LogVerbose(_T("s2_gdi _surfaces selecting no pen...\r\n"));
            break;
        case 1:
            LogVerbose(_T("s2_gdi _surfaces selecting CreatePenIndirect pen ...\r\n"));
            hpenToReturn = CreatePenIndirect(&lp);
            break;
        case 2:
            LogVerbose(_T("s2_gdi _surfaces Selecting CreatePen pen...\r\n"));
            hpenToReturn = CreatePen(lp.lopnStyle, lp.lopnWidth.x, lp.lopnColor);
            break;
        case 3:
            LogVerbose(_T("s2_gdi _surfaces Selecting NULL_PEN stock object...\r\n"));
            hpenToReturn = (HPEN) GetStockObject(NULL_PEN);
            break;
        case 4:
            LogVerbose(_T("s2_gdi _surfaces Selecting BLACK_PEN stock object...\r\n"));
            hpenToReturn = (HPEN) GetStockObject(BLACK_PEN);
            break;
        case 5:
            LogVerbose(_T("s2_gdi _surfaces Selecting WHITE_PEN stock object...\r\n"));
            hpenToReturn = (HPEN) GetStockObject(WHITE_PEN);
            break;
        default:
            DebugBreak();
            break;
    }

    if(nCase > 0 && !hpenToReturn)
    {
        LogFail(_T("#### s2:gdi _surfaces Pen creation failed Error: 0x%x ####\r\n"), GetLastError());
        if(m_psr)
            m_psr->fail();
    }

    return hpenToReturn;
}

INT CALLBACK _surfaces::EnumFontsProc(const LOGFONT *lplf, const TEXTMETRIC *lptm, DWORD dwType, LPARAM lParam)
{
    LOGFONT *lf = (LOGFONT *)lParam;

    memcpy(lf, lplf, sizeof(LOGFONT));

    // effect: random font is selected
    if (RANDOM_CHOICE)
        return FALSE;

    return TRUE;
}

HFONT _surfaces::CreateRandomFont()
{
    HFONT hfontToReturn = NULL;
    LOGFONT lf;

    int nCase = RANDOM_INT(2, 0);
    memset(&lf, 0, sizeof(LOGFONT));

    switch(nCase)
    {
        case 0:
            LogVerbose(_T("s2_gdi _surfaces Selecting no font...\r\n"));
            break;
        case 1:
            LogVerbose(_T("s2_gdi _surfaces Selecting SYSTEM_FONT stock object...\r\n"));
            hfontToReturn = (HFONT) GetStockObject(SYSTEM_FONT);
            break;
        case 2:
            LogVerbose(_T("s2_gdi _surfaces Selecting random font...\r\n"));
            // EnumFonts stops at a random font and returns it's parameters.
            if(RANDOM_CHOICE)
                EnumFonts(m_hdcPrimary,  NULL, _surfaces::EnumFontsProc,  (LPARAM)&lf);
            else EnumFontFamiliesEx(m_hdcPrimary, &lf, _surfaces::EnumFontsProc, (LPARAM)&lf, NULL);

            hfontToReturn = CreateFontIndirect(&lf);
            break;
        default:
            DebugBreak();
            break;
    }

    if(nCase > 0 && !hfontToReturn)
    {
        LogFail(_T("#### s2_gdi _surfaces Font creation failed Error: 0x%x ####\r\n"), GetLastError());
        if(m_psr)
            m_psr->fail();
    }

    return hfontToReturn;
}

HBRUSH _surfaces::CreateRandomBrush()
{
    HBRUSH hbrushToReturn = NULL;
    int wBitmapID = RANDOM_INT(BMP_LAST, BMP_FIRST);
    HBITMAP hBitmap = LoadBitmap(m_hInstance, MAKEINTRESOURCE(wBitmapID));
    int nCase = RANDOM_INT(8, 0);

    SetLastError(ERROR_SUCCESS);

    switch(nCase)
    {
        case 0:
            LogVerbose(_T("s2_gdi _surfaces Selecting no brush...\r\n"));
            break;
        case 1:
            LogVerbose(_T("s2_gdi _surfaces Selecting pattern brush...\r\n"));
            hbrushToReturn = CreatePatternBrush(hBitmap);
            break;
        case 2:
            LogVerbose(_T("s2_gdi _surfaces Selecting solid brush...\r\n"));
            hbrushToReturn = CreateSolidBrush(_COLORREF_RANDOM_);
            break;
        case 3:
            LogVerbose(_T("s2_gdi _surfaces Selecting BLACK_BRUSH...\r\n"));
            hbrushToReturn = (HBRUSH) GetStockObject(BLACK_BRUSH);
            break;
        case 4:
            LogVerbose(_T("s2_gdi _surfaces Selecting DKGRAY_BRUSH...\r\n"));
            hbrushToReturn = (HBRUSH) GetStockObject(DKGRAY_BRUSH);
            break;
        case 5:
            LogVerbose(_T("s2_gdi _surfaces Selecting HOLLOW_BRUSH...\r\n"));
            hbrushToReturn = (HBRUSH) GetStockObject(HOLLOW_BRUSH);
            break;
        case 6:
            LogVerbose(_T("s2_gdi _surfaces Selecting LTGRAY_BRUSH...\r\n"));
            hbrushToReturn = (HBRUSH) GetStockObject(LTGRAY_BRUSH);
            break;
        case 7:
            LogVerbose(_T("s2_gdi _surfaces Selecting NULL_BRUSH brush...\r\n"));
            hbrushToReturn = (HBRUSH) GetStockObject(NULL_BRUSH);
            break;
        case 8:
            LogVerbose(_T("s2_gdi _surfaces Selecting WHITE_BRUSH brush...\r\n"));
            hbrushToReturn = (HBRUSH) GetStockObject(WHITE_BRUSH);
            break;
        default:
            DebugBreak();
            break;
    }

    if(nCase > 0 && !hbrushToReturn)
    {
        if(GetLastError() == ERROR_NOT_ENOUGH_MEMORY)
        {
            LogWarn1(_T("Insufficient memory for brush creation.\r\n"));
            if(m_psr)
                m_psr->warning1();
        }
        else
        {
            LogFail(_T("#### s2_gdi _surfaces Brush creation failed Error: 0x%x ####\r\n"), GetLastError());
            if(m_psr)
                m_psr->fail();
        }
    }

    DeleteObject(hBitmap);
    return hbrushToReturn;
}

typedef union tagMYRGBQUAD { 
    struct {
      BYTE rgbBlue;
      BYTE rgbGreen;
      BYTE rgbRed;
      BYTE rgbReserved;
    };
    UINT rgbMask;
} MYRGBQUAD;

struct MYBITMAPINFO {
    BITMAPINFOHEADER bmih;
    MYRGBQUAD ct[256];
};

HBITMAP _surfaces::CreateRandomDIB()
{
    struct MYBITMAPINFO bmi;
    UINT usage = DIB_RGB_COLORS;
    DWORD *ppvBits;

    memset(&bmi, 0, sizeof(MYBITMAPINFO));

    bmi.bmih.biSize = sizeof (BITMAPINFOHEADER);
    bmi.bmih.biWidth = RECT_WIDTH(m_rc);
    bmi.bmih.biHeight = RANDOM_CHOICE?RECT_HEIGHT(m_rc):(-1*RECT_HEIGHT(m_rc));
    bmi.bmih.biPlanes = 1;
    bmi.bmih.biBitCount = RANDOM_BITDEPTH;
    bmi.bmih.biSizeImage = bmi.bmih.biXPelsPerMeter = bmi.bmih.biYPelsPerMeter = 0;
    bmi.bmih.biClrUsed = bmi.bmih.biClrImportant = 0;

    if(bmi.bmih.biBitCount <= 8)
    {
        LogVerbose(_T("s2_gdi _surfaces using <=8bpp DIB...\r\n"));

        bmi.bmih.biCompression = BI_RGB;
        for (int i = 0; i < 256; i++)
        {
            bmi.ct[i].rgbRed = RANDOM_BYTE;
            bmi.ct[i].rgbGreen = RANDOM_BYTE;
            bmi.ct[i].rgbBlue = RANDOM_BYTE;
            bmi.ct[i].rgbReserved = 0;
        }

        // 8bpp can be DIB_PAL_COLORS or DIB_RGB_COLORS, everything else is DIB_PAL_COLORS
        if(bmi.bmih.biBitCount == 8 && RANDOM_CHOICE)
        {
            LogVerbose(_T("s2_gdi _surfaces using 8bpp DIB_PAL_COLORS...\r\n"));
            usage = DIB_PAL_COLORS;
        }
    }
    else if(bmi.bmih.biBitCount == 16)
    {
        bmi.bmih.biCompression = BI_BITFIELDS;
        int nCase = RANDOM_INT(8, 0);

        switch(nCase)
        {
            case 0:
                LogVerbose(_T("s2_gdi _surfaces using 16bpp BI_RGB DIB...\r\n"));
                bmi.bmih.biCompression = BI_RGB;
                break;
            case 1:
                LogVerbose(_T("s2_gdi _surfaces using 16bpp ARGB4444 DIB...\r\n"));
                bmi.bmih.biCompression = BI_ALPHABITFIELDS;
                bmi.ct[3].rgbMask = 0xF000;
                bmi.ct[2].rgbMask = 0x000F;
                bmi.ct[1].rgbMask = 0x00F0;
                bmi.ct[0].rgbMask = 0x0F00;
                break;
            case 2:
                LogVerbose(_T("s2_gdi _surfaces using 16bpp RGB565 DIB...\r\n"));
                bmi.ct[2].rgbMask = 0x001F;
                bmi.ct[1].rgbMask = 0x07E0;
                bmi.ct[0].rgbMask = 0xF800;
                break;
            case 3:
                LogVerbose(_T("s2_gdi _surfaces using 16bpp ARGB555 DIB...\r\n"));
                bmi.ct[2].rgbMask = 0x001F;
                bmi.ct[1].rgbMask = 0x03E0;
                bmi.ct[0].rgbMask = 0x7C00;
                break;
            case 4:
                LogVerbose(_T("s2_gdi _surfaces using 16bpp ARGB1555 DIB...\r\n"));
                bmi.bmih.biCompression = BI_ALPHABITFIELDS;
                bmi.ct[3].rgbMask = 0x8000;
                bmi.ct[2].rgbMask = 0x001F;
                bmi.ct[1].rgbMask = 0x03E0;
                bmi.ct[0].rgbMask = 0x7C00;
                break;
            case 5:
                LogVerbose(_T("s2_gdi _surfaces using 16bpp ABGR4444 DIB...\r\n"));
                bmi.bmih.biCompression = BI_ALPHABITFIELDS;
                bmi.ct[3].rgbMask = 0xF000;
                bmi.ct[0].rgbMask = 0x000F;
                bmi.ct[1].rgbMask = 0x00F0;
                bmi.ct[2].rgbMask = 0x0F00;
                break;
            case 6:
                LogVerbose(_T("s2_gdi _surfaces using 16bpp BGR565 DIB...\r\n"));
                bmi.ct[0].rgbMask = 0x001F;
                bmi.ct[1].rgbMask = 0x07E0;
                bmi.ct[2].rgbMask = 0xF800;
                break;
            case 7:
                LogVerbose(_T("s2_gdi _surfaces using 16bpp BGR555 DIB...\r\n"));
                bmi.ct[0].rgbMask = 0x001F;
                bmi.ct[1].rgbMask = 0x03E0;
                bmi.ct[2].rgbMask = 0x7C00;
                break;
            case 8:
                LogVerbose(_T("s2_gdi _surfaces using 16bpp ABGR1555 DIB...\r\n"));
                bmi.bmih.biCompression = BI_ALPHABITFIELDS;
                bmi.ct[3].rgbMask = 0x8000;
                bmi.ct[0].rgbMask = 0x001F;
                bmi.ct[1].rgbMask = 0x03E0;
                bmi.ct[2].rgbMask = 0x7C00;
                break;
            default:
                DebugBreak();
                break;
        }
    }
    else if(bmi.bmih.biBitCount == 24)
    {
        bmi.bmih.biCompression = BI_RGB;
    }
    else if(bmi.bmih.biBitCount == 32)
    {
        bmi.bmih.biCompression = BI_BITFIELDS;
        int nCase = RANDOM_INT(4, 0);

        switch(nCase)
        {
            case 0:
                LogVerbose(_T("s2_gdi _surfaces using 32bpp BI_RGB DIB...\r\n"));
                bmi.bmih.biCompression = BI_RGB;
                break;
            case 1:
                LogVerbose(_T("s2_gdi _surfaces using 32bpp ARGB8888 DIB...\r\n"));
                bmi.bmih.biCompression = BI_ALPHABITFIELDS;
                bmi.ct[3].rgbMask = 0xFF000000;
                bmi.ct[2].rgbMask = 0x000000FF;
                bmi.ct[1].rgbMask = 0x0000FF00;
                bmi.ct[0].rgbMask = 0x00FF0000;
                break;
            case 2:
                LogVerbose(_T("s2_gdi _surfaces using 32bpp RGB888 DIB...\r\n"));
                bmi.ct[2].rgbMask = 0x000000FF;
                bmi.ct[1].rgbMask = 0x0000FF00;
                bmi.ct[0].rgbMask = 0x00FF0000;
                break;
            case 3:
                LogVerbose(_T("s2_gdi _surfaces using 32bpp ABGR8888 DIB...\r\n"));
                bmi.bmih.biCompression = BI_ALPHABITFIELDS;
                bmi.ct[3].rgbMask = 0xFF000000;
                bmi.ct[0].rgbMask = 0x000000FF;
                bmi.ct[1].rgbMask = 0x0000FF00;
                bmi.ct[2].rgbMask = 0x00FF0000;
                break;
            case 4:
                LogVerbose(_T("s2_gdi _surfaces using 32bpp BGR888 DIB...\r\n"));
                bmi.ct[0].rgbMask = 0x000000FF;
                bmi.ct[1].rgbMask = 0x0000FF00;
                bmi.ct[2].rgbMask = 0x00FF0000;
                break;
            default:
                DebugBreak();
                break;
        }
    }

    return(CreateDIBSection(m_hdcPrimary, (BITMAPINFO *) &bmi, usage, (VOID **) &ppvBits, NULL, NULL));
}

HBITMAP _surfaces::CreateRandomBitmap(BOOL bWritable)
{
    HBITMAP hbmpToReturn = NULL;
    int nBpp;
    int nCase = RANDOM_INT(bWritable?3:4, 0);

    SetLastError(ERROR_SUCCESS);

    switch(nCase)
    {
        case 0:
            LogVerbose(_T("s2_gdi _surfaces using the primary...\r\n"));
            break;
        case 1:
            nBpp = RANDOM_BITDEPTH;
            LogVerbose(_T("s2_gdi _surfaces using a %dbpp bitmap from CreateBitmap...\r\n"), nBpp);
            hbmpToReturn = CreateBitmap(RECT_WIDTH(m_rc), RECT_HEIGHT(m_rc), 1, nBpp, NULL);
            break;
        case 2:
            LogVerbose(_T("s2_gdi _surfaces using a bitmap from CreateCompatibleBitmap...\r\n"));
            hbmpToReturn = CreateCompatibleBitmap(m_hdcPrimary, RECT_WIDTH(m_rc), RECT_HEIGHT(m_rc));
            break;
        case 3:
            LogVerbose(_T("s2_gdi _surfaces using a random DIB bitmap...\r\n"));
            hbmpToReturn = CreateRandomDIB();
            break;
        case 4:
            LogVerbose(_T("s2_gdi _surfaces using a bitmap from LoadBitmap...\r\n"));
            hbmpToReturn = LoadBitmap(m_hInstance, MAKEINTRESOURCE(RANDOM_INT(BMP_LAST, BMP_FIRST)));
            break;
        default:
            DebugBreak();
            break;
    }

    if(nCase > 0 && !hbmpToReturn)
    {
        if(GetLastError() == ERROR_NOT_ENOUGH_MEMORY)
        {
            LogWarn1(_T("Insufficient memory for bitmap creation.\r\n"));
            if(m_psr)
                m_psr->warning1();
        }
        else
        {
            LogFail(_T("#### s2_gdi _surfaces BitmapCreation failed Error: 0x%x ####\r\n "), GetLastError());
            if(m_psr)
                m_psr->fail();
        }
    }

    return hbmpToReturn;
}

HRGN _surfaces::CreateRandomRegion()
{
    RECT rc;
    HRGN hrgnFinal;
    HRGN hrgnTmp;

    RandomRect(&rc, &m_rc);
    hrgnFinal = CreateRectRgnIndirect(&rc);

    for(int i = 0; i < RGN_COMPLEXITY; i++)
    {
        RandomRect(&rc, &m_rc);
        hrgnTmp = CreateRectRgnIndirect(&rc);
        CombineRgn(hrgnFinal, hrgnFinal, hrgnTmp, RGN_XOR);
        DeleteObject(hrgnTmp);
    }

    return hrgnFinal;
}

_surfaces::_surfaces(HWND hWnd, RECT rc, HINSTANCE hInstance, _stressrun_t * _psr) : m_hWnd(hWnd), m_rc(rc), m_hInstance(hInstance), m_psr(_psr)
{
    m_hdcPrimary = GetDC(hWnd);
    m_hbmpStock = CreateBitmap(1, 1, 1, 1, NULL);
    m_hbrushStock = (HBRUSH) GetStockObject(NULL_BRUSH);
    m_hpenStock = (HPEN) GetStockObject(NULL_PEN);
    m_hFontStock = (HFONT) GetStockObject(SYSTEM_FONT);

    HDC hdcTmp;
    HBITMAP hbmp, hbmpOld;
    int nWidth = 3;
    int nHeight = 3;
    int nBlockWidth = 9;
    int nBlockHeight = 9;

    hdcTmp = CreateCompatibleDC(m_hdcPrimary);
    hbmp = CreateCompatibleBitmap(m_hdcPrimary, nWidth * nBlockWidth, nHeight * nBlockHeight);
    hbmpOld = (HBITMAP) SelectObject(hdcTmp, hbmp);
    PatBlt(hdcTmp, 0, 0, nWidth * nBlockWidth, nHeight * nBlockHeight, WHITENESS);
        
    for(int i=0; i< nWidth; i++)
        for(int j=0; j< nHeight; j++)
        {
            COLORREF cr = _COLORREF_RANDOM_;
            for(int k=0; k< nBlockWidth; k++)
                for(int l=0; l< nBlockHeight; l++)
                {
                    SetPixel(hdcTmp, (i * nBlockWidth) + k, (j * nBlockHeight) + l, cr);
                }
        }

    SelectObject(hdcTmp, hbmpOld);

    m_hbrPattern = CreatePatternBrush(hbmp);

    DeleteDC(hdcTmp);
    DeleteObject(hbmp);
}

_surfaces::~_surfaces()
{
    ReleaseDC(m_hWnd, m_hdcPrimary);
    DeleteObject(m_hbmpStock);
    DeleteObject(m_hbrPattern);
}

HDC _surfaces::GetRandomSurface(BOOL bWriteable)
{
    //pick a random surface
    HDC hdcToReturn;
    HBITMAP hbmp;
    HBRUSH hbrush;
    HPEN hpen;
    HFONT hfont;
    HRGN hrgn;

    // When selecting a new object, delete the previous object
    // just in case.  The worst that happens is "stock" objects are
    // deleted, which is harmless.
    hbmp = CreateRandomBitmap(bWriteable);
    if(hbmp)
    {
        hdcToReturn = CreateCompatibleDC(m_hdcPrimary);
        DeleteObject(SelectObject(hdcToReturn, hbmp));
    }
    else hdcToReturn = m_hdcPrimary;

    FillRect(hdcToReturn, &m_rc, m_hbrPattern);

    // pick a random brush
    hbrush = CreateRandomBrush();

    if(hbrush)
        DeleteObject(SelectObject(hdcToReturn, hbrush));

    // pick a random pen
    hpen = CreateRandomPen();

    if(hpen)
        DeleteObject(SelectObject(hdcToReturn, hpen));

    // pick a random font
    hfont = CreateRandomFont();

    if(hfont)
        DeleteObject(SelectObject(hdcToReturn, hfont));

    // pick a setup a random region
    hrgn = CreateRandomRegion();

    if(hrgn)
    {
        SelectObject(hdcToReturn, hrgn);
        DeleteObject(hrgn);
    }

    // pick a random rop2
    SetROP2(hdcToReturn, g_nvRop2Array[RANDOM_ROP2INDEX].dwROP);

    // set a random foreground color
    SetTextColor(hdcToReturn, _COLORREF_RANDOM_);

    // set a random background color
    SetBkColor(hdcToReturn, _COLORREF_RANDOM_);

    // set a random bkmode
    SetBkMode(hdcToReturn, RANDOM_BKMODE);

    // set a random text align
    SetTextAlign(hdcToReturn, RANDOM_TEXTALIGN);

    return hdcToReturn;
}

BOOL _surfaces::ReleaseSurface(HDC hdc)
{
    BOOL bPassed = TRUE;

    // the worst that happens here is we delete "stock" objects which have no effect.
    bPassed &= DeleteObject(SelectObject(hdc, m_hbrushStock));
    bPassed &= DeleteObject(SelectObject(hdc, m_hFontStock));
    bPassed &= DeleteObject(SelectObject(hdc, m_hpenStock));

    if(hdc != m_hdcPrimary)
    {
        bPassed &= DeleteObject(SelectObject(hdc, m_hbmpStock));
        bPassed &= DeleteDC(hdc);
    }

    return bPassed;
}

