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


_stressblt_t::_stressblt_t(HINSTANCE hInstance, HWND hWnd, _stressrun_t *psr)
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

	// DC

	hDC = GetDC(hWnd);

	if (!hDC)
	{
		LogFail(_T("#### _stressblt_t::_stressblt_t - GetDC failed - Error: 0x%x ####\r\n"),
			GetLastError());
		_psr->fail();
		return;
	}

	// Bitmap

	wBitmapID = RANDOM_INT(BMP_LAST, BMP_FIRST);

	LogVerbose(_T("_stressblt_t::_stressblt_t - Calling LoadBitmap(Resource ID: %d)...\r\n"), wBitmapID);

	hBitmap = LoadBitmap(_hInstance, MAKEINTRESOURCE(wBitmapID));

	if (!hBitmap)
	{
		LogFail(_T("#### _stressblt_t::_stressblt_t - LoadBitmap(%d) failed - Error: 0x%x ####\r\n"),
			wBitmapID, GetLastError());
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

	// Pattern Brush

	hBrush = CreatePatternBrush(hBitmap);

	if (!hBrush)
	{
		LogFail(_T("#### _stressblt_t::_stressblt_t - CreatePatternBrush failed - Error: 0x%x ####\r\n"),
			GetLastError());
		_psr->fail();
		return;
	}

	// Source DC

	hSrcDC = CreateCompatibleDC(hDC);

	if (!hSrcDC)
	{
		LogFail(_T("#### _stressblt_t::_stressblt_t - CreateCompatibleDC failed - Error: 0x%x ####\r\n"),
			GetLastError());
		_psr->fail();
		return;
	}

	SelectObject(hSrcDC, hBitmap);
}


_stressblt_t::~_stressblt_t()
{
	if (!DeleteDC(hSrcDC))
	{
		LogFail(_T("#### _stressblt_t::~_stressblt_t - DeleteDC failed - Error: 0x%x ####\r\n"),
			GetLastError());
		_psr->fail();
	}

	if (!DeleteObject(hBrush))
	{
		LogFail(_T("#### _stressblt_t::~_stressblt_t - DeleteObject(hBrush: 0x%x) failed - Error: 0x%x ####\r\n"),
			hBrush, GetLastError());
		_psr->fail();
	}

	if (!DeleteObject(hBitmap))
	{
		LogFail(_T("#### _stressblt_t::~_stressblt_t - DeleteObject(hBitmap: 0x%x) failed - Error: 0x%x ####\r\n"),
			hBitmap, GetLastError());
		_psr->fail();
	}

	if (!ReleaseDC(_hWnd, hDC))
	{
		LogFail(_T("#### _stressblt_t::~_stressblt_t - ReleaseDC failed - Error: 0x%x ####\r\n"),
			GetLastError());
		_psr->fail();
	}
}


DWORD _stressblt_t::bitblt()
{
	_stressrun_t obj(L"_stressblt_t::bitblt");

	ROP_GUIDE rgROP[] = {
		NAME_VALUE_PAIR(SRCCOPY),		// dest = source
		NAME_VALUE_PAIR(SRCPAINT),		// dest = source OR dest
		NAME_VALUE_PAIR(SRCAND),		// dest = source AND dest
		NAME_VALUE_PAIR(SRCINVERT),		// dest = source XOR dest
		NAME_VALUE_PAIR(SRCERASE),		// dest = source AND (NOT dest )
		NAME_VALUE_PAIR(NOTSRCCOPY),	// dest = (NOT source)
		NAME_VALUE_PAIR(NOTSRCERASE), 	// dest = (NOT src) AND (NOT dest)
		NAME_VALUE_PAIR(MERGECOPY), 	// dest = (source AND pattern)
		NAME_VALUE_PAIR(MERGEPAINT), 	// dest = (NOT source) OR dest
		NAME_VALUE_PAIR(PATCOPY), 		// dest = pattern
		NAME_VALUE_PAIR(PATPAINT), 		// dest = DPSnoo
		NAME_VALUE_PAIR(PATINVERT), 	// dest = pattern XOR dest
		NAME_VALUE_PAIR(DSTINVERT), 	// dest = (NOT dest)
		NAME_VALUE_PAIR(BLACKNESS), 	// dest = BLACK
		NAME_VALUE_PAIR(WHITENESS) 		// dest = WHITE
	};

	BITMAP bm;
	INT i = rand() % ARRAY_SIZE(rgROP);

	if (!GetObject(hBitmap, sizeof BITMAP, &bm))
	{
		LogFail(_T("#### _stressblt_t::bitblt - GetObject failed - Error: 0x%x ####\r\n"),
			GetLastError());
		obj.fail();
		goto done;
	}

	LogVerbose(_T("_stressblt_t::bitblt - Calling BitBlt(%s)...\r\n"),
		rgROP[i].lpszROP);

	if (!::BitBlt(hDC, 0, 0, bm.bmWidth , bm.bmHeight, hSrcDC, 0, 0, rgROP[i].dwROP))
	{
		LogFail(_T("#### _stressblt_t::bitblt - BitBlt(%s) failed - Error: 0x%x ####\r\n"),
			rgROP[i].lpszROP, GetLastError());
		obj.fail();
	}

done:
	return obj.status();
}


DWORD _stressblt_t::stretchblt()
{
	_stressrun_t obj(L"_stressblt_t::stretchblt");

	ROP_GUIDE rgROP[] = {
		NAME_VALUE_PAIR(SRCCOPY),		// dest = source
		NAME_VALUE_PAIR(SRCPAINT),		// dest = source OR dest
		NAME_VALUE_PAIR(SRCAND),		// dest = source AND dest
		NAME_VALUE_PAIR(SRCINVERT),		// dest = source XOR dest
		NAME_VALUE_PAIR(SRCERASE),		// dest = source AND (NOT dest )
		NAME_VALUE_PAIR(NOTSRCCOPY),	// dest = (NOT source)
		NAME_VALUE_PAIR(NOTSRCERASE), 	// dest = (NOT src) AND (NOT dest)
		NAME_VALUE_PAIR(MERGECOPY), 	// dest = (source AND pattern)
		NAME_VALUE_PAIR(MERGEPAINT), 	// dest = (NOT source) OR dest
		NAME_VALUE_PAIR(PATCOPY), 		// dest = pattern
		NAME_VALUE_PAIR(PATPAINT), 		// dest = DPSnoo
		NAME_VALUE_PAIR(PATINVERT), 	// dest = pattern XOR dest
		NAME_VALUE_PAIR(DSTINVERT), 	// dest = (NOT dest)
		NAME_VALUE_PAIR(BLACKNESS), 	// dest = BLACK
		NAME_VALUE_PAIR(WHITENESS) 		// dest = WHITE
	};

	BITMAP bm;
	INT i = rand() % ARRAY_SIZE(rgROP);

	if (!GetObject(hBitmap, sizeof BITMAP, &bm))
	{
		LogFail(_T("#### _stressblt_t::stretchblt - GetObject failed - Error: 0x%x ####\r\n"),
			GetLastError());
		obj.fail();
		goto done;
	}

	LogVerbose(_T("_stressblt_t::stretchblt - Calling StretchBlt(%s)...\r\n"),
		rgROP[i].lpszROP);


	if (!::StretchBlt(hDC, _rc.left, _rc.top, RECT_WIDTH(_rc), RECT_HEIGHT(_rc),
		hSrcDC, 0, 0, bm.bmWidth, bm.bmHeight, (DWORD)rgROP[i].dwROP))
	{
		LogFail(_T("#### _stressblt_t::stretchblt - StretchBlt(%s) failed - Error: 0x%x ####\r\n"),
			rgROP[i].lpszROP, GetLastError());
		obj.fail();
	}

done:
	return obj.status();
}


DWORD _stressblt_t::transparentimage()
{
	_stressrun_t obj(L"_stressblt_t::transparentimage");

	COLORREF crTransparentColor;
	BITMAP bm;

	if (!GetObject(hBitmap, sizeof BITMAP, &bm))
	{
		LogFail(_T("#### _stressblt_t::transparentimage - GetObject failed - Error: 0x%x ####\r\n"),
			GetLastError());
		obj.fail();
		goto done;
	}

	crTransparentColor = _COLORREF_RANDOM_;
	DumpRGB(crTransparentColor);

	LogVerbose(_T("_stressblt_t::transparentimage - Calling TransparentImage...\r\n"));

	if (!::TransparentImage(hDC, _rc.left, _rc.top, RECT_WIDTH(_rc), RECT_HEIGHT(_rc),
		hSrcDC, 0, 0, bm.bmWidth, bm.bmHeight, crTransparentColor))
	{
		LogFail(_T("#### _stressblt_t::transparentimage - TransparentImage failed - Error: 0x%x ####\r\n"),
			GetLastError());
		DumpFailRGB(crTransparentColor);
		obj.fail();
	}

done:
	return obj.status();
}


DWORD _stressblt_t::maskblt()
{
	_stressrun_t obj(L"_stressblt_t::maskblt");

	ROP_GUIDE rgROP[] = {
		NAME_VALUE_PAIR(SRCCOPY),		// dest = source
		NAME_VALUE_PAIR(SRCPAINT),		// dest = source OR dest
		NAME_VALUE_PAIR(SRCAND),		// dest = source AND dest
		NAME_VALUE_PAIR(SRCINVERT),		// dest = source XOR dest
		NAME_VALUE_PAIR(SRCERASE),		// dest = source AND (NOT dest )
		NAME_VALUE_PAIR(NOTSRCCOPY),	// dest = (NOT source)
		NAME_VALUE_PAIR(NOTSRCERASE), 	// dest = (NOT src) AND (NOT dest)
		NAME_VALUE_PAIR(MERGECOPY), 	// dest = (source AND pattern)
		NAME_VALUE_PAIR(MERGEPAINT), 	// dest = (NOT source) OR dest
		NAME_VALUE_PAIR(PATCOPY), 		// dest = pattern
		NAME_VALUE_PAIR(PATPAINT), 		// dest = DPSnoo
		NAME_VALUE_PAIR(PATINVERT), 	// dest = pattern XOR dest
		NAME_VALUE_PAIR(DSTINVERT), 	// dest = (NOT dest)
		NAME_VALUE_PAIR(BLACKNESS), 	// dest = BLACK
		NAME_VALUE_PAIR(WHITENESS) 		// dest = WHITE
	};

	BITMAP bm;
	INT i = rand() % ARRAY_SIZE(rgROP);

	if (!GetObject(hBitmap, sizeof BITMAP, &bm))
	{
		LogFail(_T("#### _stressblt_t::maskblt - GetObject failed - Error: 0x%x ####\r\n"),
			GetLastError());
		obj.fail();
		goto done;
	}

	LogVerbose(_T("_stressblt_t::maskblt - Calling MaskBlt(%s)...\r\n"),
		rgROP[i].lpszROP);


	if (!::MaskBlt(hDC, 0, 0, bm.bmWidth, bm.bmHeight, hSrcDC, 0, 0, hMask,
		0, 0, (DWORD)rgROP[i].dwROP))
	{
		LogFail(_T("#### _stressblt_t::maskblt - MaskBlt(%s) failed - Error: 0x%x ####\r\n"),
			rgROP[i].lpszROP, GetLastError());
		obj.fail();
	}

done:
	return obj.status();
}


DWORD _stressblt_t::patblt()
{
	_stressrun_t obj(L"_stressblt_t::patblt");

	// PatBlt has it's own ROP codes

	ROP_GUIDE rgROP[] = {
		NAME_VALUE_PAIR(PATCOPY),	/* copies the specified pattern into the
		destination bitmap */
		NAME_VALUE_PAIR(PATINVERT),	/* combines the colors of the specified
		pattern with the  colors of the destination rectangle by using the
		boolean OR operator */
		NAME_VALUE_PAIR(DSTINVERT),	// inverts the destination rectangle
		NAME_VALUE_PAIR(BLACKNESS),	/* fills the destination rectangle using the
		color associated with index 0 in the physical palette. (black for the
		default physical palette.) */
		NAME_VALUE_PAIR(WHITENESS) /* fills the destination rectangle using the
		color associated with index 1 in the physical palette. (white for the
		default physical palette.) */
	};

	INT i = rand() % ARRAY_SIZE(rgROP);

	HBRUSH hDefaultBrush = (HBRUSH)SelectObject(hDC, hBrush);

	LogVerbose(_T("_stressblt_t::patblt - Calling PatBlt(%s)...\r\n"),
		rgROP[i].lpszROP);

	if (!::PatBlt(hDC, _rc.left, _rc.top, RECT_WIDTH(_rc), RECT_HEIGHT(_rc), (DWORD)rgROP[i].dwROP))
	{
		LogFail(_T("#### _stressblt_t::patblt - PatBlt(%s) failed - Error: 0x%x ####\r\n"),
			rgROP[i].lpszROP, GetLastError());
		obj.fail();
	}

	SelectObject(hDC, hDefaultBrush);
	return obj.status();
}
