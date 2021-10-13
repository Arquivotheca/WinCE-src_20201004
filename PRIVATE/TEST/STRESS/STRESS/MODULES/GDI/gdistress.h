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
#ifndef __GDISTRESS_H__

#define    ARRAY_SIZE(rg)            ((sizeof rg) / (sizeof rg[0]))
#define    RANDOM_INT(max, min)    ((INT)((rand() % ((INT)max - (INT)min + 1)) + (INT)min))
#define    RANDOM_BYTE              ((BYTE)(rand() % 0xFF))
#define    RANDOM_BITDEPTH        g_nBitDepths[RANDOM_INT(BITDEPTHS_COUNT-1, 0)]
#define    RANDOM_TEXTALIGN        g_nTextAlign[RANDOM_INT(TEXTALIGN_COUNT-1, 0)]
#define    RANDOM_BKMODE          g_nBKModes[RANDOM_INT(BKMODES_COUNT-1, 0)]
#define    RANDOM_PENSTYLE       g_nPenStyles[RANDOM_INT(PENSTYLES_COUNT-1, 0)]
#define    RANDOM_ROP3INDEX      RANDOM_INT(ROP3_COUNT - 1, 0)
#define    RANDOM_ROP2INDEX      RANDOM_INT(ROP2_COUNT - 1, 0)
#define    RANDOM_CHOICE            ((BOOL)RANDOM_INT(TRUE, FALSE))
#define    RANDOM_POSNEG            RANDOM_CHOICE?-1:1
#define    RANDOM_WIDTH(rc, range) RANDOM_POSNEG*((rand() % (RECT_WIDTH(rc) + (range * 2))) - range)
#define    RANDOM_HEIGHT(rc, range) RANDOM_POSNEG*((rand() % (RECT_HEIGHT(rc) + (range * 2))) - range)
#define    RANDOM_POSWIDTH(rc, range) ((rand() % (RECT_WIDTH(rc) + (range * 2))) - range)
#define    RANDOM_POSHEIGHT(rc, range) ((rand() % (RECT_HEIGHT(rc) + (range * 2))) - range)
#define    NAMEVALENTRY(tok)    {tok, L#tok}
#define    RECT_WIDTH(rc)            (abs((LONG)rc.right - (LONG)rc.left))
#define    RECT_HEIGHT(rc)            (abs((LONG)rc.bottom - (LONG)rc.top))
#define    DumpRECT(rc)            LogVerbose(L#rc L".left = %d " L#rc L".top = %d " L#rc L".right = %d " L#rc L".bottom = %d\r\n", rc.left, rc.top, rc.right, rc.bottom)
#define    DumpFailRECT(rc)        LogFail(L#rc L".left = %d " L#rc L".top = %d " L#rc L".right = %d " L#rc L".bottom = %d\r\n", rc.left, rc.top, rc.right, rc.bottom)
#define    DumpWarn1RECT(rc)        LogWarn1(L#rc L".left = %d " L#rc L".top = %d " L#rc L".right = %d " L#rc L".bottom = %d\r\n", rc.left, rc.top, rc.right, rc.bottom)
#define    DumpWarn2RECT(rc)        LogWarn2(L#rc L".left = %d " L#rc L".top = %d " L#rc L".right = %d " L#rc L".bottom = %d\r\n", rc.left, rc.top, rc.right, rc.bottom)
#define    DumpPOINT(pt)            LogVerbose(L#pt L".x = %d " L#pt L".y = %d\r\n", pt.x, pt.y)
#define    DumpFailPOINT(pt)        LogFail(L#pt L".x = %d " L#pt L".y = %d\r\n", pt.x, pt.y)
#define    DumpRGB(cr)                LogVerbose(L#cr L":Red = %d " L#cr L":Green = %d " L#cr L":Blue = %d\r\n", GetRValue(cr), GetGValue(cr), GetBValue(cr))
#define    DumpFailRGB(cr)            LogFail(L#cr L":Red = %d " L#cr L":Green = %d " L#cr L":Blue = %d\r\n", GetRValue(cr), GetGValue(cr), GetBValue(cr))
#define    DumpWarn1RGB(cr)        LogWarn1(L#cr L":Red = %d " L#cr L":Green = %d " L#cr L":Blue = %d\r\n", GetRValue(cr), GetGValue(cr), GetBValue(cr))
#define    DumpWarn2RGB(cr)        LogWarn2(L#cr L":Red = %d " L#cr L":Green = %d " L#cr L":Blue = %d\r\n", GetRValue(cr), GetGValue(cr), GetBValue(cr))

#define    _COLORREF_RANDOM_        RGB(rand() % 256, rand() % 256, rand() % 256)
#define    _LINE_WIDTH_RANDOM_        (RANDOM_INT(0x80, 0x0))
#define    _DELTA_RANDOM_            (RANDOM_INT((0x80), (-0x80)))
#define    _ELLIPSE_WIDTH_RANDOM_    (RANDOM_INT(0x20, 0x04))
#define    _ELLIPSE_HEIGHT_RANDOM_    (RANDOM_INT(0x20, 0x04))

#define    LOOPCOUNT_MIN            (0x10)
#define    LOOPCOUNT_MAX            (0x20)
#define    RGN_COMPLEXITY 10

typedef int (WINAPI * PFNGRADIENTFILL)(HDC hdc, PTRIVERTEX pVertex,ULONG nVertex,PVOID pMesh, ULONG nCount, ULONG ulMode);
typedef BOOL (WINAPI * PFNALPHABLEND)(HDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, HDC tdcSrc, int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, BLENDFUNCTION blendFunction);

struct ROP_GUIDE
{
    DWORD dwROP;
    LPTSTR lpszROP;
};

struct _surfaces
{
public:
    _surfaces(HWND hWnd, RECT rc, HINSTANCE hInstance, _stressrun_t *_psr);
    ~_surfaces();

    HDC GetRandomSurface(BOOL bWriteable);
    BOOL ReleaseSurface(HDC hdc);
    static INT CALLBACK EnumFontsProc(const LOGFONT *lplf, const TEXTMETRIC *lptm, DWORD dwType, LPARAM lParam);
    HPEN CreateRandomPen();
    HFONT CreateRandomFont();
    HBRUSH CreateRandomBrush();
    HBITMAP CreateRandomBitmap(BOOL bWriteable);
    HRGN CreateRandomRegion();
    // it's occasionally useful for the test to know whether or not it's on the primary, so
    // let's allow access to this.
    HDC m_hdcPrimary;

private:
    HBITMAP CreateRandomDIB();
    _stressrun_t *m_psr;
    HINSTANCE m_hInstance;
    HWND m_hWnd;
    RECT m_rc;
    HBITMAP m_hbmpStock;
    HBRUSH m_hbrushStock;
    HPEN m_hpenStock;
    HFONT m_hFontStock;
    HBRUSH m_hbrPattern;
};

struct _stressblt_t
{
protected:
    HWND _hWnd;
    RECT _rc;
    HINSTANCE _hInstance;

public:
    _stressrun_t *_psr;
    HBITMAP hMask;
    WORD wMaskID;
    PFNALPHABLEND pfnAlphaBlend;
    _surfaces *m_Surfaces;
    HINSTANCE m_hinstCoreDLL;

public:
    _stressblt_t(HINSTANCE hInstance, HWND hWnd, _stressrun_t *psr);
    ~_stressblt_t();
    DWORD bitblt();
    DWORD stretchblt();
    DWORD transparentimage();
    DWORD maskblt();
    DWORD patblt();
    DWORD alphablend();

private:
    _stressblt_t();                                    // default constructor NOT implemented
    _stressblt_t(const _stressblt_t&);                // default copy constructor NOT implemented
    _stressblt_t& operator= (const _stressblt_t&);    // default assignment operator NOT
};


struct _stressdraw_t
{
protected:
    HWND _hWnd;
    RECT _rc;
    HINSTANCE _hInstance;

public:
    _stressrun_t *_psr;
    _surfaces *m_Surfaces;
    PFNGRADIENTFILL pfnGradientFill;
    HINSTANCE m_hinstCoreDLL;

public:
    _stressdraw_t(HINSTANCE hInstance, HWND hWnd, _stressrun_t *psr);
    ~_stressdraw_t();
    DWORD rectangle();
    DWORD fillrect();
    DWORD ellipse();
    DWORD roundrect();
    DWORD polygon();
    DWORD pixel();
    DWORD icon();
    DWORD gradientfill();

private:
    _stressdraw_t();                                    // default constructor NOT implemented
    _stressdraw_t(const _stressdraw_t&);                // default copy constructor NOT implemented
    _stressdraw_t& operator= (const _stressdraw_t&);    // default assignment operator NOT
};


struct FLAG_DETAILS
{
    DWORD dwFlag;
    LPTSTR lpszFlag;
};


struct _stressfont_t
{
protected:
    HWND _hWnd;
    RECT _rc;
    HINSTANCE _hInstance;

public:
    _stressrun_t *_psr;
    _surfaces *m_Surfaces;

public:
    _stressfont_t(HINSTANCE hInstance, HWND hWnd, _stressrun_t *psr);
    ~_stressfont_t();
    DWORD exttextout();
    DWORD drawtext();

public:
    static INT CALLBACK _stressfont_t::EnumFontsProc(const LOGFONT *lplf, const TEXTMETRIC *lptm, DWORD dwType, LPARAM lParam);

private:
    _stressfont_t();                                    // default constructor NOT implemented
    _stressfont_t(const _stressfont_t&);                // default copy constructor NOT implemented
    _stressfont_t& operator= (const _stressfont_t&);    // default assignment operator NOT
};


struct _stressrgnclip_t
{
protected:
    HWND _hWnd;
    RECT _rc;
    HINSTANCE _hInstance;

public:
    _stressrun_t *_psr;
    _surfaces *m_Surfaces;

public:
    _stressrgnclip_t(HINSTANCE hInstance, HWND hWnd, _stressrun_t *psr);
    ~_stressrgnclip_t();
    DWORD test();

private:
    _stressrgnclip_t();                                        // default constructor NOT implemented
    _stressrgnclip_t(const _stressrgnclip_t&);                // default copy constructor NOT implemented
    _stressrgnclip_t& operator= (const _stressrgnclip_t&);    // default assignment operator NOT
    DWORD combinergn(HRGN &hrgnDest, HRGN hrgnSrc);
};


BOOL RandomRect(LPRECT prc, const LPRECT prcContainer);
POINT RandomPoint(const RECT *prcContainer);

extern INT g_nBitDepths[];
extern INT BITDEPTHS_COUNT;
extern INT g_nTextAlign[];
extern INT TEXTALIGN_COUNT;
extern INT g_nBKModes[];
extern INT BKMODES_COUNT;
extern INT g_nPenStyles[];
extern INT PENSTYLES_COUNT;
extern ROP_GUIDE g_nvRop3Array[];
extern INT ROP3_COUNT;
extern ROP_GUIDE g_nvRop2Array[];
extern INT ROP2_COUNT;


#define __GDISTRESS_H__
#endif /* __GDISTRESS_H__ */
