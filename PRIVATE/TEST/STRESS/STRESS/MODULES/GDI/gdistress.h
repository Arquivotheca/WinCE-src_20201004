//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __GDISTRESS_H__


#define	ARRAY_SIZE(rg)			((sizeof rg) / (sizeof rg[0]))
#define	RANDOM_INT(max, min)	((INT)((rand() % ((INT)max - (INT)min + 1)) + (INT)min))
#define	RANDOM_CHOICE			((BOOL)RANDOM_INT(TRUE, FALSE))
#define	NAME_VALUE_PAIR(tok)	{tok, L#tok}
#define	RECT_WIDTH(rc)			(abs((LONG)rc.right - (LONG)rc.left))
#define	RECT_HEIGHT(rc)			(abs((LONG)rc.bottom - (LONG)rc.top))
#define	DumpRECT(rc)			LogVerbose(L#rc L".left = %d " L#rc L".top = %d " L#rc L".right = %d " L#rc L".bottom = %d\r\n", rc.left, rc.top, rc.right, rc.bottom)
#define	DumpFailRECT(rc)		LogFail(L#rc L".left = %d " L#rc L".top = %d " L#rc L".right = %d " L#rc L".bottom = %d\r\n", rc.left, rc.top, rc.right, rc.bottom)
#define	DumpWarn1RECT(rc)		LogWarn1(L#rc L".left = %d " L#rc L".top = %d " L#rc L".right = %d " L#rc L".bottom = %d\r\n", rc.left, rc.top, rc.right, rc.bottom)
#define	DumpWarn2RECT(rc)		LogWarn2(L#rc L".left = %d " L#rc L".top = %d " L#rc L".right = %d " L#rc L".bottom = %d\r\n", rc.left, rc.top, rc.right, rc.bottom)
#define	DumpPOINT(pt)			LogVerbose(L#pt L".x = %d " L#pt L".y = %d\r\n", pt.x, pt.y)
#define	DumpFailPOINT(pt)		LogFail(L#pt L".x = %d " L#pt L".y = %d\r\n", pt.x, pt.y)
#define	DumpRGB(cr)				LogVerbose(L#cr L":Red = %d " L#cr L":Green = %d " L#cr L":Blue = %d\r\n", GetRValue(cr), GetGValue(cr), GetBValue(cr))
#define	DumpFailRGB(cr)			LogFail(L#cr L":Red = %d " L#cr L":Green = %d " L#cr L":Blue = %d\r\n", GetRValue(cr), GetGValue(cr), GetBValue(cr))
#define	DumpWarn1RGB(cr)		LogWarn1(L#cr L":Red = %d " L#cr L":Green = %d " L#cr L":Blue = %d\r\n", GetRValue(cr), GetGValue(cr), GetBValue(cr))
#define	DumpWarn2RGB(cr)		LogWarn2(L#cr L":Red = %d " L#cr L":Green = %d " L#cr L":Blue = %d\r\n", GetRValue(cr), GetGValue(cr), GetBValue(cr))

#define	_COLORREF_RANDOM_		RGB(rand() % 256, rand() % 256, rand() % 256)
#define	_LINE_WIDTH_RANDOM_		(RANDOM_INT(0x10, 0x01))
#define	_DELTA_RANDOM_			(RANDOM_INT((0x80), (-0x80)))
#define	_ELLIPSE_WIDTH_RANDOM_	(RANDOM_INT(0x20, 0x04))
#define	_ELLIPSE_HEIGHT_RANDOM_	(RANDOM_INT(0x20, 0x04))

#define	LOOPCOUNT_MIN			(0x10)
#define	LOOPCOUNT_MAX			(0x20)


struct ROP_GUIDE
{
	DWORD dwROP;
	LPTSTR lpszROP;
};


struct _stressblt_t
{
protected:
	HWND _hWnd;
	RECT _rc;
	HINSTANCE _hInstance;

public:
	_stressrun_t *_psr;
	HDC hDC;
	HDC hSrcDC;
	HBITMAP hBitmap;
	WORD wBitmapID;
	HBITMAP hMask;
	WORD wMaskID;
	HBRUSH hBrush;

public:
	_stressblt_t(HINSTANCE hInstance, HWND hWnd, _stressrun_t *psr);
	~_stressblt_t();
	DWORD bitblt();
	DWORD stretchblt();
	DWORD transparentimage();
	DWORD maskblt();
	DWORD patblt();

private:
	_stressblt_t();									// default constructor NOT implemented
	_stressblt_t(const _stressblt_t&);				// default copy constructor NOT implemented
	_stressblt_t& operator= (const _stressblt_t&);	// default assignment operator NOT
};


struct _stressdraw_t
{
protected:
	HWND _hWnd;
	RECT _rc;
	HINSTANCE _hInstance;

public:
	_stressrun_t *_psr;
	HDC hDC;
	HBITMAP hBitmap;
	WORD wBitmapID;
	HBRUSH hPatternBrush;

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

private:
	_stressdraw_t();									// default constructor NOT implemented
	_stressdraw_t(const _stressdraw_t&);				// default copy constructor NOT implemented
	_stressdraw_t& operator= (const _stressdraw_t&);	// default assignment operator NOT
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
	HDC hDC;
	HFONT hFont;
	TEXTMETRIC _tm;
	LOGFONT _lf;

public:
	_stressfont_t(HINSTANCE hInstance, HWND hWnd, _stressrun_t *psr);
	~_stressfont_t();
	DWORD exttextout();
	DWORD drawtext();

public:
	static INT CALLBACK _stressfont_t::EnumFontsProc(const LOGFONT *lplf, const TEXTMETRIC *lptm, DWORD dwType, LPARAM lParam);

private:
	_stressfont_t();									// default constructor NOT implemented
	_stressfont_t(const _stressfont_t&);				// default copy constructor NOT implemented
	_stressfont_t& operator= (const _stressfont_t&);	// default assignment operator NOT
};


struct _stressrgnclip_t
{
protected:
	HWND _hWnd;
	RECT _rc;
	HINSTANCE _hInstance;

public:
	_stressrun_t *_psr;
	HDC hDC;
	HBITMAP hBitmap;
	WORD wBitmapID;
	HBRUSH hPatternBrush;

public:
	_stressrgnclip_t(HINSTANCE hInstance, HWND hWnd, _stressrun_t *psr);
	~_stressrgnclip_t();
	DWORD test();

private:
	_stressrgnclip_t();										// default constructor NOT implemented
	_stressrgnclip_t(const _stressrgnclip_t&);				// default copy constructor NOT implemented
	_stressrgnclip_t& operator= (const _stressrgnclip_t&);	// default assignment operator NOT
	DWORD combinergn(HRGN &hrgnDest, HRGN hrgnSrc);
};


BOOL RandomRect(LPRECT prc, const LPRECT prcContainer);
POINT RandomPoint(const RECT *prcContainer);


#define __GDISTRESS_H__
#endif /* __GDISTRESS_H__ */
