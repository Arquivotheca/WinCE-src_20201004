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


_stressrgnclip_t::_stressrgnclip_t(HINSTANCE hInstance, HWND hWnd, _stressrun_t *psr)
{
	_hInstance = hInstance;

	if (psr)
	{
		_psr = psr;
	}

	// window

	if (!IsWindow(hWnd))
	{
		LogFail(_T("#### _stressrgnclip_t::_stressrgnclip_t - IsWindow failed - Error: 0x%x ####\r\n"),
			GetLastError());
		_psr->fail();
		return;
	}

	_hWnd = hWnd;

	if (!GetClientRect(hWnd, &_rc))
	{
		LogFail(_T("#### _stressrgnclip_t::_stressrgnclip_t - GetClientRect failed - Error: 0x%x ####\r\n"),
			GetLastError());
		_psr->fail();
		return;
	}

	// DC

	hDC = GetDC(hWnd);

	if (!hDC)
	{
		LogFail(_T("#### _stressrgnclip_t::_stressrgnclip_t - GetDC failed - Error: 0x%x ####\r\n"),
			GetLastError());
		_psr->fail();
		return;
	}

	// Bitmap

	wBitmapID = RANDOM_INT(BMP_LAST, BMP_FIRST);

	LogVerbose(_T("_stressrgnclip_t::_stressrgnclip_t - Calling LoadBitmap(Resource ID: %d)...\r\n"), wBitmapID);

	hBitmap = LoadBitmap(_hInstance, MAKEINTRESOURCE(wBitmapID));

	if (!hBitmap)
	{
		LogFail(_T("#### _stressrgnclip_t::_stressrgnclip_t - LoadBitmap(%d) failed - Error: 0x%x ####\r\n"),
			wBitmapID, GetLastError());
		_psr->fail();
		return;
	}

	// Pattern Brush

	hPatternBrush = CreatePatternBrush(hBitmap);

	if (!hPatternBrush)
	{
		LogFail(_T("#### _stressrgnclip_t::_stressrgnclip_t - CreatePatternBrush failed - Error: 0x%x ####\r\n"),
			GetLastError());
		_psr->fail();
		return;
	}
}


_stressrgnclip_t::~_stressrgnclip_t()
{
	if (!DeleteObject(hPatternBrush))
	{
		LogFail(_T("#### _stressrgnclip_t::~_stressrgnclip_t - DeleteObject(hPatternBrush: 0x%x) failed - Error: 0x%x ####\r\n"),
			hPatternBrush, GetLastError());
		_psr->fail();
	}

	if (!DeleteObject(hBitmap))
	{
		LogFail(_T("#### _stressrgnclip_t::~_stressrgnclip_t - DeleteObject(hBitmap: 0x%x) failed - Error: 0x%x ####\r\n"),
			hBitmap, GetLastError());
		_psr->fail();
	}

	if (!ReleaseDC(_hWnd, hDC))
	{
		LogFail(_T("#### _stressrgnclip_t::~_stressrgnclip_t - ReleaseDC failed - Error: 0x%x ####\r\n"),
			GetLastError());
		_psr->fail();
	}
}


DWORD _stressrgnclip_t::test()
{
	_stressrun_t obj(L"_stressrgnclip_t::test");
	INT i =0, j = 0;
	HRGN hRegion = NULL;
	HRGN hClipRegion = NULL;
	HRGN hRgnTmp = NULL;
	HRGN hRegion2 = NULL;
	RECT rcRgn;
	RECT rcClip;
	RECT rc;
	RECT rcIntersectClip;
	RECT rcExcludeClip;
	RECT rcRandom;
	const INT MAX_SIZE = 1024;
	BYTE rgb[MAX_SIZE];
	INT nX = _DELTA_RANDOM_, nY = _DELTA_RANDOM_;
	HBRUSH hBrush = NULL;
	bool fCombinedRegionAvailable = false;

	const INT MAIN_LOOP_COUNT = RANDOM_INT(LOOPCOUNT_MIN, 0);

	for (i = 0; i < MAIN_LOOP_COUNT; i++)
	{
		hRegion = NULL;
		hRegion2 = NULL;
		fCombinedRegionAvailable = false;

		SetRectEmpty(&rcRgn);
		SetRectEmpty(&rcClip);
		SetRectEmpty(&rc);

		// generate random rect

		SetRectEmpty(&rcRgn);

		LogVerbose(_T("_stressrgnclip_t::test - Calling RandomRect(rcRgn)...\r\n"));

		if (!RandomRect(&rcRgn, &_rc))
		{
			LogWarn2(_T("#### _stressrgnclip_t::test - RandomRect(rcRgn) failed ####\r\n"));
			obj.warning2();
			goto next;
		}

		// CreateRectRgn OR CreateRectRgnIndirect

		if (RANDOM_CHOICE)
		{
			LogVerbose(_T("_stressrgnclip_t::test - Calling CreateRectRgn...\r\n"));

			hRegion = CreateRectRgn(rcRgn.left, rcRgn.top, rcRgn.right, rcRgn.bottom);

			if (!hRegion)
			{
				LogFail(_T("#### _stressrgnclip_t::test - CreateRectRgn failed - Error: 0x%x ####\r\n"),
					GetLastError());
				DumpFailRECT(rcRgn);
				obj.fail();
				goto next;
			}
		}
		else
		{
			LogVerbose(_T("_stressrgnclip_t::test - Calling CreateRectRgnIndirect...\r\n"));

			hRegion = CreateRectRgnIndirect(&rcRgn);

			if (!hRegion)
			{
				LogFail(_T("#### _stressrgnclip_t::test - CreateRectRgnIndirect failed - Error: 0x%x ####\r\n"),
					GetLastError());
				DumpFailRECT(rcRgn);
				obj.fail();
				goto next;
			}
		}

		// SetRectRgn

		if (RANDOM_CHOICE)
		{
			// generate random rect

			SetRectEmpty(&rcRgn);

			LogVerbose(_T("_stressrgnclip_t::test - Calling RandomRect(rcRgn)...\r\n"));

			if (!RandomRect(&rcRgn, &_rc))
			{
				LogWarn2(_T("#### _stressrgnclip_t::test - RandomRect(rcRgn) failed ####\r\n"));
				obj.warning2();
				goto next;
			}

			LogVerbose(_T("_stressrgnclip_t::test - Calling SetRectRgn...\r\n"));

			if (!SetRectRgn(hRegion, rcRgn.left, rcRgn.top, rcRgn.right, rcRgn.bottom))
			{
				LogFail(_T("#### _stressrgnclip_t::test - SetRectRgn failed - Error: 0x%x ####\r\n"),
					GetLastError());
				DumpFailRECT(rcRgn);
				obj.fail();
				goto next;
			}
		}

		// SelectClipRgn

		// generate random rect

		SetRectEmpty(&rcClip);

		LogVerbose(_T("_stressrgnclip_t::test - Calling RandomRect(rcClip)...\r\n"));

		if (!RandomRect(&rcClip, &_rc))
		{
			LogWarn2(_T("#### _stressrgnclip_t::test - RandomRect(rcClip) failed ####\r\n"));
			obj.warning2();
			goto next;
		}

		hClipRegion = NULL;

		LogVerbose(_T("_stressrgnclip_t::test - Calling CreateRectRgnIndirect...\r\n"));

		hClipRegion = CreateRectRgnIndirect(&rcClip);

		if (!hClipRegion)
		{
			LogFail(_T("#### _stressrgnclip_t::test - CreateRectRgnIndirect(rcClip) failed - Error: 0x%x ####\r\n"),
				GetLastError());
			DumpFailRECT(rcClip);
			obj.fail();
			goto next;
		}

		LogVerbose(_T("_stressrgnclip_t::test - Calling SelectClipRgn...\r\n"));

		if (SIMPLEREGION != SelectClipRgn(hDC, hClipRegion))
		{
			LogWarn1(_T("#### _stressrgnclip_t::test - SelectClipRgn failed - Error: 0x%x ####\r\n"),
				GetLastError());
			DumpWarn1RECT(rcClip);
			obj.warning1();
		}

		// GetClipBox

		SetRectEmpty(&rc);

		if (SIMPLEREGION != GetClipBox(hDC, &rc))
		{
			LogWarn1(_T("#### _stressrgnclip_t::test - GetClipBox failed - Error: 0x%x ####\r\n"),
				GetLastError());
			DumpWarn1RECT(rcClip);
			obj.warning1();
		}
		else
		{
			if (!EqualRect(&rc, &rcClip))
			{
				LogWarn1(_T("#### _stressrgnclip_t::test - EqualRect failed - Error: 0x%x ####\r\n"),
					GetLastError());
				DumpWarn1RECT(rc);
				DumpWarn1RECT(rcClip);
				obj.warning1();
			}
		}

		// GetClipRgn

		hRgnTmp = NULL;

		LogVerbose(_T("_stressrgnclip_t::test - Calling CreateRectRgn...\r\n"));

		hRgnTmp = CreateRectRgn(0, 0, 0, 0);

		if (!hRegion)
		{
			LogFail(_T("#### _stressrgnclip_t::test - CreateRectRgn(0, 0, 0, 0) failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto next;
		}

		LogVerbose(_T("_stressrgnclip_t::test - Calling GetClipRgn...\r\n"));

		if (-1 ==  GetClipRgn(hDC, hRgnTmp))
		{
			LogFail(_T("#### _stressrgnclip_t::test - GetClipRgn failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto next;
		}
		else
		{
			SetRectEmpty(&rc);

			// GetRgnBox

			LogVerbose(_T("_stressrgnclip_t::test - Calling GetRgnBox...\r\n"));

			if (SIMPLEREGION != GetRgnBox(hClipRegion, &rc))
			{
				LogWarn1(_T("#### _stressrgnclip_t::test - GetRgnBox failed - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.warning1();
			}

			// EqualRgn

			LogVerbose(_T("_stressrgnclip_t::test - Calling EqualRgn...\r\n"));

			if (!EqualRgn(hClipRegion, hRgnTmp))
			{
				LogWarn1(_T("#### _stressrgnclip_t::test - EqualRgn failed - Error: 0x%x ####\r\n"),
					GetLastError());
				DumpWarn1RECT(rc);
				DumpWarn1RECT(rcClip);
				obj.warning1();
			}
		}

		if (RANDOM_CHOICE)	// ExcludeClipRect
		{
			SetRectEmpty(&rcExcludeClip);

			if (!RandomRect(&rcExcludeClip, &rcClip))	// rcClip is the selected clip region for hDC
			{
				LogWarn2(_T("#### _stressrgnclip_t::test - RandomRect(rcExcludeClip) failed ####\r\n"));
				obj.warning2();
				goto next;
			}

			LogVerbose(_T("_stressrgnclip_t::test - Calling ExcludeClipRect...\r\n"));

			if (ERROR == ExcludeClipRect(hDC, rcExcludeClip.left,
				rcExcludeClip.top, rcExcludeClip.right, rcExcludeClip.bottom))
			{
				LogFail(_T("#### _stressrgnclip_t::test - ExcludeClipRect failed - Error: 0x%x ####\r\n"),
					GetLastError());
				DumpFailRECT(rcClip);
				DumpWarn1RECT(rcExcludeClip);
				obj.fail();
				goto next;
			}
		}
		else	// IntersectClipRect
		{
			SetRectEmpty(&rcIntersectClip);

			if (!RandomRect(&rcIntersectClip, &_rc))
			{
				LogWarn2(_T("#### _stressrgnclip_t::test - RandomRect(rcIntersectClip) failed ####\r\n"));
				obj.warning2();
				goto next;
			}

			LogVerbose(_T("_stressrgnclip_t::test - Calling IntersectClipRect...\r\n"));

			if (ERROR == IntersectClipRect(hDC, rcIntersectClip.left,
				rcIntersectClip.top, rcIntersectClip.right, rcIntersectClip.bottom))
			{
				LogFail(_T("#### _stressrgnclip_t::test - IntersectClipRect failed - Error: 0x%x ####\r\n"),
					GetLastError());
				DumpFailRECT(rcClip);
				DumpFailRECT(rcIntersectClip);
				obj.fail();
				goto next;
			}
		}

		// DEVNOTE: rcClip no longer contains the clipping area of the hDC

		// RectVisible

		if (RANDOM_CHOICE)
		{
			SetRectEmpty(&rcRandom);

			if (!RandomRect(&rcRandom, &_rc))
			{
				LogWarn2(_T("#### _stressrgnclip_t::test - RandomRect(rcRandom) failed ####\r\n"));
				obj.warning2();
				goto next;
			}

			LogVerbose(_T("_stressrgnclip_t::test - Calling RectVisible...\r\n"));

			if (RectVisible(hDC, &rcRandom))
			{
				LogVerbose(_T("_stressrgnclip_t::test - Following RECT is visible\r\n"));
				DumpRECT(rcRandom);
			}
			else
			{
				LogVerbose(_T("_stressrgnclip_t::test - Following RECT is NOT visible\r\n"));
				DumpRECT(rcRandom);
			}
		}

		// CombineRgn

		if (RANDOM_CHOICE)
		{
			LogVerbose(_T("_stressrgnclip_t::test - Calling _stressrgnclip_t::combinergn...\r\n"));

			if (CESTRESS_PASS != obj.test(combinergn(hRegion2, hRegion)))
			{
				LogFail(_T("#### _stressrgnclip_t::test - _stressrgnclip_t::combinergn failed - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
				goto next;
			}
			else
			{
				fCombinedRegionAvailable = true;
			}
		}


		// GetRegionData

		LogVerbose(_T("_stressrgnclip_t::test - Calling GetRegionData...\r\n"));

		memset(rgb, 0, sizeof rgb);

		if (!GetRegionData(hRegion, MAX_SIZE, (LPRGNDATA)rgb))
		{
			LogFail(_T("#### _stressrgnclip_t::test - GetRegionData failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
		}

		for (j = 0; j < RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN); j++)
		{
			// OffsetRgn

			nX = _DELTA_RANDOM_, nY = _DELTA_RANDOM_;

			LogVerbose(_T("_stressrgnclip_t::test - Calling OffsetRgn(%d, %d)...\r\n"), nX, nY);

			if (ERROR == OffsetRgn(hRegion, nX, nY))
			{
				LogFail(_T("#### _stressrgnclip_t::test - OffsetRgn(%d, %d) failed - Error: 0x%x ####\r\n"),
					nX, nY, GetLastError());
				obj.fail();
			}

			// FillRgn

			hBrush = NULL;

			if (RANDOM_CHOICE)
			{
				hBrush = hPatternBrush;
			}
			else
			{
				hBrush = CreateSolidBrush(_COLORREF_RANDOM_);
			}

			if (fCombinedRegionAvailable)	// combined region available
			{
				if (RANDOM_CHOICE)	// combined region
				{
					LogVerbose(_T("_stressrgnclip_t::test - Calling FillRgn with combined region...\r\n"));

					if (!FillRgn(hDC, hRegion2, hBrush))
					{
						LogFail(_T("#### _stressrgnclip_t::test - FillRgn(hRegion2) failed - Error: 0x%x ####\r\n"),
							GetLastError());
						obj.fail();
					}
				}
				else	// normal region
				{
					LogVerbose(_T("_stressrgnclip_t::test - Calling FillRgn...\r\n"));

					if (!FillRgn(hDC, hRegion, hBrush))
					{
						LogFail(_T("#### _stressrgnclip_t::test - FillRgn(hRegion) failed - Error: 0x%x ####\r\n"),
							GetLastError());
						obj.fail();
					}
				}
			}
			else
			{
				LogVerbose(_T("_stressrgnclip_t::test - Calling FillRgn...\r\n"));

				if (!FillRgn(hDC, hRegion, hBrush))
				{
					LogFail(_T("#### _stressrgnclip_t::test - FillRgn(hRegion) failed - Error: 0x%x ####\r\n"),
						GetLastError());
					obj.fail();
				}
			}
		}

		next:
			if (hRegion)
			{
				DeleteObject(hRegion);
			}

			if (hRegion2)
			{
				DeleteObject(hRegion2);
			}

			if (hClipRegion)
			{
				DeleteObject(hClipRegion);
			}

			if (hRgnTmp)
			{
				DeleteObject(hRgnTmp);
			}
	}


	return obj.status();
}


/*

@func:	_stressrgnclip_t.combinergn, randomly combines a random region with the
given region and puts the result in the destination

@rdesc:	stress return status

@param:	[out] HRGN &hrgnDest: Handle to a new region with dimensions defined by
combining two other regions. (This region must NOT exist before and CALLER is
responsible for calling DeleteObject)

@param:	[in] HRGN hrgnSrc: Handle to the given region to be combined with a
randomly generated one

@fault:	None

@pre:	The destination region (hrgnDest) must NOT exist before

@post:	The CALLER is responsible for calling DeleteObject on the destination
region

@note:	Private function, to be called only from _stressrgnclip_t::test

*/

DWORD _stressrgnclip_t::combinergn(HRGN &hrgnDest, HRGN hrgnSrc)
{
	_stressrun_t obj(L"_stressrgnclip_t::combinergn");

	FLAG_DETAILS rgfd[] = {
		NAME_VALUE_PAIR(RGN_AND),
		NAME_VALUE_PAIR(RGN_COPY),
		NAME_VALUE_PAIR(RGN_DIFF),
		NAME_VALUE_PAIR(RGN_OR),
		NAME_VALUE_PAIR(RGN_XOR)
	};

	INT i = rand() % ARRAY_SIZE(rgfd);
	RECT rc;
	HRGN hrgnTmpSrc = NULL;;

	// generate random rect

	SetRectEmpty(&rc);

	LogVerbose(_T("_stressrgnclip_t::combinergn - Calling RandomRect(rc)...\r\n"));

	if (!RandomRect(&rc, &_rc))
	{
		LogWarn2(_T("#### _stressrgnclip_t::combinergn - RandomRect(rc) failed ####\r\n"));
		obj.warning2();
		goto done;
	}

	hrgnTmpSrc = NULL;
	hrgnTmpSrc = CreateRectRgnIndirect(&rc);

	if (!hrgnTmpSrc)
	{
		LogFail(_T("#### _stressrgnclip_t::combinergn - CreateRectRgnIndirect(hrgnTmpSrc) failed - Error: 0x%x ####\r\n"),
			GetLastError());
		obj.fail();
		goto done;
	}


	SetRect(&rc, 0, 0, 1, 1);
	hrgnDest = NULL;

	hrgnDest = CreateRectRgnIndirect(&rc);

	if (!hrgnDest)
	{
		LogFail(_T("#### _stressrgnclip_t::combinergn - CreateRectRgnIndirect(hrgnDest) failed - Error: 0x%x ####\r\n"),
			GetLastError());
		obj.fail();
		goto done;
	}

	LogVerbose(_T("_stressrgnclip_t::combinergn - Calling CombineRgn(%s)...\r\n"),
		rgfd[i].lpszFlag);

	if (ERROR == CombineRgn(hrgnDest, hrgnSrc, hrgnTmpSrc,  rgfd[i].dwFlag))
	{
		LogFail(_T("#### _stressrgnclip_t::combinergn - CombineRgn(%s) failed - Error: 0x%x ####\r\n"),
			rgfd[i].lpszFlag, GetLastError());
		obj.fail();

		if (hrgnDest)
		{
			DeleteObject(hrgnDest);
		}
	}

done:
	if (hrgnTmpSrc)
	{
		DeleteObject(hrgnTmpSrc);
	}

	return obj.status();
}
