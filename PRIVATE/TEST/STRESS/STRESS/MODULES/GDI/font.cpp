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


extern LPTSTR g_rgszStr[];
extern INT STR_COUNT;


_stressfont_t::_stressfont_t(HINSTANCE hInstance, HWND hWnd, _stressrun_t *psr)
{
	_hInstance = hInstance;

	if (psr)
	{
		_psr = psr;
	}

	// window

	if (!IsWindow(hWnd))
	{
		LogFail(_T("#### _stressfont_t::_stressfont_t - IsWindow failed - Error: 0x%x ####\r\n"),
			GetLastError());
		_psr->fail();
		return;
	}

	_hWnd = hWnd;

	if (!GetClientRect(hWnd, &_rc))
	{
		LogFail(_T("#### _stressfont_t::_stressfont_t - GetClientRect failed - Error: 0x%x ####\r\n"),
			GetLastError());
		_psr->fail();
		return;
	}

	// DC

	hDC = GetDC(hWnd);

	if (!hDC)
	{
		LogFail(_T("#### _stressfont_t::_stressfont_t - GetDC failed - Error: 0x%x ####\r\n"),
			GetLastError());
		_psr->fail();
		return;
	}

	// Font

	memset(&_lf,  0,  sizeof _lf);
	memset(&_tm,  0,  sizeof _lf);

	// enumerate all available fonts in the system

	EnumFonts(hDC,  NULL, _stressfont_t::EnumFontsProc,  (LPARAM)this);

	// _stressfont_t::EnumFontsProc must have already populated LOGFONT & TEXTMETRIC

	if (_lf.lfFaceName && _lf.lfFaceName[0] && _tcslen(_lf.lfFaceName))
	{
		hFont =  CreateFontIndirect(&_lf);

		if (!hFont)
		{
			LogFail(_T("#### _stressfont_t::_stressfont_t - CreateFontIndirect(%s) failed - Error: 0x%x ####\r\n"),
				_lf.lfFaceName, GetLastError());
			_psr->fail();
		}
	}
}


_stressfont_t::~_stressfont_t()
{
	if (!DeleteObject(hFont))
	{
		LogFail(_T("#### _stressfont_t::~_stressfont_t - DeleteObject(hFont: 0x%x) failed - Error: 0x%x ####\r\n"),
			hFont, GetLastError());
		_psr->fail();
	}

	if (!ReleaseDC(_hWnd, hDC))
	{
		LogFail(_T("#### _stressfont_t::~_stressfont_t - ReleaseDC failed - Error: 0x%x ####\r\n"),
			GetLastError());
		_psr->fail();
	}
}


INT CALLBACK _stressfont_t::EnumFontsProc(const LOGFONT *lplf, const TEXTMETRIC *lptm, DWORD dwType, LPARAM lParam)
{
	_stressfont_t *psf = (_stressfont_t *)lParam;

	memcpy(&psf->_lf, lplf, sizeof psf->_lf);
	memcpy(&psf->_tm, lptm, sizeof psf->_tm);

	// effect: random font is selected

	if (RANDOM_CHOICE)
	{
		return FALSE;
	}

	return TRUE;
};


DWORD _stressfont_t::drawtext()
{
	_stressrun_t obj(L"_stressfont_t::drawtext");

	FLAG_DETAILS rgfd[] = {
		NAME_VALUE_PAIR(DT_BOTTOM),
		NAME_VALUE_PAIR(DT_CALCRECT),
		NAME_VALUE_PAIR(DT_CENTER),
		NAME_VALUE_PAIR(DT_EXPANDTABS),
		NAME_VALUE_PAIR(DT_INTERNAL),
		NAME_VALUE_PAIR(DT_LEFT),
		NAME_VALUE_PAIR(DT_NOCLIP),
		NAME_VALUE_PAIR(DT_NOPREFIX),
		NAME_VALUE_PAIR(DT_RIGHT),
		NAME_VALUE_PAIR(DT_SINGLELINE),
		NAME_VALUE_PAIR(DT_TABSTOP),
		NAME_VALUE_PAIR(DT_TOP),
		NAME_VALUE_PAIR(DT_VCENTER),
		NAME_VALUE_PAIR(DT_WORDBREAK)
	};

	HGDIOBJ hOld = SelectObject(hDC, hFont);



	INT LOOP_COUNT = RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN);

	for (INT i = 0; i < LOOP_COUNT; i++)
	{
		INT j = rand() % ARRAY_SIZE(rgfd);
		INT isz = rand() % STR_COUNT;
		RECT rc;

		SetRectEmpty(&rc);

		LogVerbose(_T("_stressfont_t::drawtext - Calling RandomRect...\r\n"));

		if (!RandomRect(&rc, &_rc))
		{
			LogWarn2(_T("#### _stressfont_t::drawtext - RandomRect failed ####\r\n"));
			continue;
		}

		LogVerbose(_T("_stressfont_t::drawtext - Calling DrawText(%s, %s)...\r\n"),
			g_rgszStr[isz], rgfd[j].lpszFlag);

		if (!DrawText(hDC, (LPTSTR)g_rgszStr[isz], -1, &rc, rgfd[j].dwFlag))
		{
			LogFail(_T("#### _stressfont_t::drawtext - DrawText(%s, %s) failed - Error: 0x%x ####\r\n"),
				g_rgszStr[isz], rgfd[j].lpszFlag, GetLastError());
			DumpFailRECT(rc);
			obj.fail();
		}
	}

	SelectObject(hDC, hOld);

	return obj.status();
}


DWORD _stressfont_t::exttextout()
{
	_stressrun_t obj(L"_stressfont_t::exttextout");

	FLAG_DETAILS rgfd[] = {
		NAME_VALUE_PAIR(ETO_CLIPPED),
		NAME_VALUE_PAIR(ETO_OPAQUE),
		NAME_VALUE_PAIR(ETO_OPAQUE | ETO_CLIPPED)
	};

	HGDIOBJ hOld = SelectObject(hDC, hFont);

	const INT LOOP_COUNT = RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN);

	for (INT i = 0; i < LOOP_COUNT; i++)
	{
		INT j = rand() % ARRAY_SIZE(rgfd);
		INT isz = rand() % STR_COUNT;
		RECT rc;
		POINT pt;

		SetRectEmpty(&rc);
		memset(&pt, 0, sizeof pt);

		LogVerbose(_T("_stressfont_t::exttextout - Calling RandomRect...\r\n"));

		if (!RandomRect(&rc, &_rc))
		{
			LogWarn2(_T("#### _stressfont_t::drawtext - RandomRect failed ####\r\n"));
			continue;
		}
		else
		{
			if (RANDOM_CHOICE)
			{
				pt = RandomPoint(&rc);	// within random rect only
			}
			else
			{
				pt = RandomPoint(&_rc);	// within window client area
			}
		}

		LogVerbose(_T("_stressfont_t::exttextout - Calling ExtTextOut(%s, %s)...\r\n"),
			g_rgszStr[isz], rgfd[j].lpszFlag);

		if (!ExtTextOut(hDC, pt.x, pt.y, rgfd[j].dwFlag, &rc, (LPTSTR)g_rgszStr[isz],
			_tcslen(g_rgszStr[isz]), NULL))
		{
			LogFail(_T("#### _stressfont_t::exttextout - ExtTextOut(%s, %s) failed - Error: 0x%x ####\r\n"),
				g_rgszStr[isz], rgfd[j].lpszFlag, GetLastError());
			DumpFailPOINT(pt);
			DumpFailRECT(rc);
			obj.fail();
		}
	}

	SelectObject(hDC, hOld);

	return obj.status();
}

