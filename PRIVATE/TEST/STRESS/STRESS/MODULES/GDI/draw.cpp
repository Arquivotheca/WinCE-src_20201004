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


_stressdraw_t::_stressdraw_t(HINSTANCE hInstance, HWND hWnd, _stressrun_t *psr)
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

	// DC

	hDC = GetDC(hWnd);

	if (!hDC)
	{
		LogFail(_T("#### _stressdraw_t::_stressdraw_t - GetDC failed - Error: 0x%x ####\r\n"),
			GetLastError());
		_psr->fail();
		return;
	}

	// Bitmap

	wBitmapID = RANDOM_INT(BMP_LAST, BMP_FIRST);

	LogVerbose(_T("_stressdraw_t::_stressdraw_t - Calling LoadBitmap(Resource ID: %d)...\r\n"), wBitmapID);

	hBitmap = LoadBitmap(_hInstance, MAKEINTRESOURCE(wBitmapID));

	if (!hBitmap)
	{
		LogFail(_T("#### _stressdraw_t::_stressdraw_t - LoadBitmap(%d) failed - Error: 0x%x ####\r\n"),
			wBitmapID, GetLastError());
		_psr->fail();
		return;
	}

	// Pattern Brush

	hPatternBrush = CreatePatternBrush(hBitmap);

	if (!hPatternBrush)
	{
		LogFail(_T("#### _stressdraw_t::_stressdraw_t - CreatePatternBrush failed - Error: 0x%x ####\r\n"),
			GetLastError());
		_psr->fail();
		return;
	}
}


_stressdraw_t::~_stressdraw_t()
{
	if (!DeleteObject(hPatternBrush))
	{
		LogFail(_T("#### _stressdraw_t::~_stressdraw_t - DeleteObject(hPatternBrush: 0x%x) failed - Error: 0x%x ####\r\n"),
			hPatternBrush, GetLastError());
		_psr->fail();
	}

	if (!DeleteObject(hBitmap))
	{
		LogFail(_T("#### _stressdraw_t::~_stressdraw_t - DeleteObject(hBitmap: 0x%x) failed - Error: 0x%x ####\r\n"),
			hBitmap, GetLastError());
		_psr->fail();
	}

	if (!ReleaseDC(_hWnd, hDC))
	{
		LogFail(_T("#### _stressdraw_t::~_stressdraw_t - ReleaseDC failed - Error: 0x%x ####\r\n"),
			GetLastError());
		_psr->fail();
	}
}


DWORD _stressdraw_t::rectangle()
{
	_stressrun_t obj(L"_stressdraw_t::rectangle");

	COLORREF cr;
	INT nLineWidth = 1;
	INT nDeltaX = 0, nDeltaY = 0;
	HPEN hPen = NULL;
	#if 0
	HGDIOBJ hDefaultGDI = SelectObject(hDC, GetStockObject(BLACK_PEN));
	#endif /* 0 */
	HBRUSH hBrush = NULL;
	RECT rcRandom;

	// generate a random rectangle

	if (!RandomRect(&rcRandom, &_rc))
	{
		LogFail(_T("#### _stressdraw_t::rectangle - RandomRect failed - Error: 0x%x ####\r\n"),
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

		if (!SaveDC(hDC))
		{
			LogFail(_T("#### _stressdraw_t::rectangle - SaveDC failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
		}

		// initialize handles

		hPen = NULL;
		hBrush = NULL;

		BOOL fSolid = RANDOM_CHOICE;
		BOOL fPatternBrush = false;

		if (fSolid)
		{
			if (RANDOM_CHOICE)
			{
				LogVerbose(_T("_stressdraw_t::rectangle - Using pattern brush...\r\n"));
				hBrush = hPatternBrush;
				fPatternBrush = true;
			}
			else
			{
				LogVerbose(_T("_stressdraw_t::rectangle - Using solid brush...\r\n"));

				hBrush = CreateSolidBrush(cr);

				if (!hBrush)
				{
					LogFail(_T("#### _stressdraw_t::rectangle - CreateSolidBrush failed - Error: 0x%x ####\r\n"),
						GetLastError());
					obj.fail();
					goto cleanup;
				}
			}

			SelectObject(hDC, hBrush);
		}
		else
		{
			nLineWidth = _LINE_WIDTH_RANDOM_;

			BOOL fDashedPen = i % 2; // PS_SOLID: 0, PS_DASH: 1

			/* if the value specified by nWidth is greater than 1, the
			fnPenStyle parameter must be PS_NULL or PS_SOLID */

			if (nLineWidth > 1)
			{
				fDashedPen = PS_SOLID; // false
			}

			if (fDashedPen)
			{
				LogVerbose(_T("_stressdraw_t::rectangle - Using PS_DASH...\r\n"));
			}
			else
			{
				LogVerbose(_T("_stressdraw_t::rectangle - Using PS_SOLID...\r\n"));
			}

			hPen = CreatePen(fDashedPen, nLineWidth, cr);

			if (!hPen)
			{
				LogFail(_T("#### _stressdraw_t::rectangle - CreatePen failed - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
				goto cleanup;
			}

			SelectObject(hDC, hPen);
		}

		LogVerbose(_T("_stressdraw_t::rectangle - Calling Rectangle...\r\n"));

		if (!Rectangle(hDC, (rcRandom.left + nDeltaX), (rcRandom.top + nDeltaY),
			(rcRandom.right + nDeltaX), (rcRandom.bottom + nDeltaY)))
		{
			LogFail(_T("#### _stressdraw_t::rectangle - Rectangle #%i failed - Error: 0x%x ####\r\n"),
				i, GetLastError());
			DumpFailRECT(rcRandom);
			DumpFailRGB(cr);
			LogFail(_T("_stressdraw_t::rectangle - nLineWidth: %d, nDeltaX: %d, nDeltaY: %d\r\n"),
				nLineWidth, nDeltaX, nDeltaY);
			obj.fail();
		}

	cleanup:
		#if 0
		SelectObject(hDC, hDefaultGDI);
		#endif /* 0 */

		if (!RestoreDC(hDC, -1))
		{
			LogFail(_T("#### _stressdraw_t::rectangle - RestoreDC failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
		}

		if (hPen)
		{
			if (!DeleteObject(hPen))
			{
				LogFail(_T("#### _stressdraw_t::rectangle - DeleteObject(hPen: 0x%x) failed - Error: 0x%x ####\r\n"),
					hPen, GetLastError());
				obj.fail();
			}
		}

		if (!fPatternBrush && hBrush)
		{
			if (!DeleteObject(hBrush))
			{
				LogFail(_T("#### _stressdraw_t::rectangle - DeleteObject(hBrush: 0x%x) failed - Error: 0x%x ####\r\n"),
					hBrush, GetLastError());
				obj.fail();
			}
		}
	}

	return obj.status();
}


DWORD _stressdraw_t::fillrect()
{
	_stressrun_t obj(L"_stressdraw_t::fillrect");

	COLORREF cr;
	INT nDeltaX = 0, nDeltaY = 0;
	HBRUSH hBrush = NULL;
	RECT rcFilled;
	RECT rcRandom;

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
		cr = _COLORREF_RANDOM_;
		DumpRGB(cr);
		nDeltaX = _DELTA_RANDOM_;
		nDeltaY = _DELTA_RANDOM_;
		hBrush = NULL;

		if (!SaveDC(hDC))
		{
			LogFail(_T("#### _stressdraw_t::fillrect - SaveDC failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
		}

		BOOL fPatternBrush = false;

		if (RANDOM_CHOICE)
		{
			LogVerbose(_T("_stressdraw_t::fillrect - Using pattern brush...\r\n"));
			hBrush = hPatternBrush;
			fPatternBrush = true;
		}
		else
		{
			LogVerbose(_T("_stressdraw_t::fillrect - Using solid brush...\r\n"));

			hBrush = CreateSolidBrush(cr);

			if (!hBrush)
			{
				LogFail(_T("#### _stressdraw_t::fillrect - CreateSolidBrush failed - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
				goto cleanup;
			}
		}

		SelectObject(hDC, hBrush);

		SetRect(&rcFilled, (rcRandom.left + nDeltaX), (rcRandom.top + nDeltaY),
			(rcRandom.right + nDeltaX), (rcRandom.bottom + nDeltaY));

		LogVerbose(_T("_stressdraw_t::fillrect - Calling FillRect...\r\n"));

		if (!FillRect(hDC, &rcFilled, hBrush))
		{
			LogFail(_T("#### _stressdraw_t::fillrect - FillRect #%i failed - Error: 0x%x ####\r\n"),
				i, GetLastError());
			DumpFailRECT(rcFilled);
			DumpFailRGB(cr);
			LogFail(_T("_stressdraw_t::fillrect - nDeltaX: %d, nDeltaY: %d\r\n"),
				nDeltaX, nDeltaY);
			obj.fail();
		}

	cleanup:
		if (!RestoreDC(hDC, -1))
		{
			LogFail(_T("#### _stressdraw_t::fillrect - RestoreDC failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
		}

		if (!fPatternBrush && hBrush)
		{
			if (!DeleteObject(hBrush))
			{
				LogFail(_T("#### _stressdraw_t::fillrect - DeleteObject(hBrush: 0x%x) failed - Error: 0x%x ####\r\n"),
					hBrush, GetLastError());
				obj.fail();
			}
		}
	}

	return obj.status();
}


DWORD _stressdraw_t::ellipse()
{
	_stressrun_t obj(L"_stressdraw_t::ellipse");

	INT nLineWidth = 1;
	COLORREF cr;
	INT nDeltaX = 0, nDeltaY = 0;
	HBRUSH hBrush = NULL;
	RECT rcRandom;
	HPEN hPen;

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
		cr = _COLORREF_RANDOM_;
		DumpRGB(cr);
		nDeltaX = _DELTA_RANDOM_;
		nDeltaY = _DELTA_RANDOM_;

		if (!SaveDC(hDC))
		{
			LogFail(_T("#### _stressdraw_t::ellipse - SaveDC failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
		}

		// initialize handles

		hPen = NULL;
		hBrush = NULL;

		BOOL fSolid = RANDOM_CHOICE;
		BOOL fPatternBrush = false;

		if (fSolid)
		{
			if (RANDOM_CHOICE)
			{
				LogVerbose(_T("_stressdraw_t::ellipse - Using pattern brush...\r\n"));
				hBrush = hPatternBrush;
				fPatternBrush = true;
			}
			else
			{
				LogVerbose(_T("_stressdraw_t::ellipse - Using solid brush...\r\n"));

				hBrush = CreateSolidBrush(cr);

				if (!hBrush)
				{
					LogFail(_T("#### _stressdraw_t::ellipse - CreateSolidBrush failed - Error: 0x%x ####\r\n"),
						GetLastError());
					obj.fail();
					goto cleanup;
				}
			}

			SelectObject(hDC, hBrush);
		}
		else
		{
			nLineWidth = _LINE_WIDTH_RANDOM_;

			BOOL fDashedPen = i % 2; // PS_SOLID: 0, PS_DASH: 1

			/* if the value specified by nWidth is greater than 1, the
			fnPenStyle parameter must be PS_NULL or PS_SOLID */

			if (nLineWidth > 1)
			{
				fDashedPen = PS_SOLID; // false
			}

			if (fDashedPen)
			{
				LogVerbose(_T("_stressdraw_t::ellipse - Using PS_DASH...\r\n"));
			}
			else
			{
				LogVerbose(_T("_stressdraw_t::ellipse - Using PS_SOLID...\r\n"));
			}

			hPen = CreatePen(fDashedPen, nLineWidth, cr);

			if (!hPen)
			{
				LogFail(_T("#### _stressdraw_t::ellipse - CreatePen failed - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
				goto cleanup;
			}

			SelectObject(hDC, hPen);
		}

		LogVerbose(_T("_stressdraw_t::ellipse - Calling Ellipse...\r\n"));

		if (!Ellipse(hDC, (rcRandom.left + nDeltaX), (rcRandom.top + nDeltaY),
			(rcRandom.right + nDeltaX), (rcRandom.bottom + nDeltaY)))
		{
			LogFail(_T("#### _stressdraw_t::ellipse - Ellipse #%i failed - Error: 0x%x ####\r\n"),
				i, GetLastError());
			DumpFailRECT(rcRandom);
			DumpFailRGB(cr);
			LogFail(_T("_stressdraw_t::ellipse - nLineWidth: %d, nDeltaX: %d, nDeltaY: %d\r\n"),
				nLineWidth, nDeltaX, nDeltaY);
			obj.fail();
		}

	cleanup:
		if (!RestoreDC(hDC, -1))
		{
			LogFail(_T("#### _stressdraw_t::ellipse - RestoreDC failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
		}

		if (hPen)
		{
			if (!DeleteObject(hPen))
			{
				LogFail(_T("#### _stressdraw_t::ellipse - DeleteObject(hPen: 0x%x) failed - Error: 0x%x ####\r\n"),
					hPen, GetLastError());
				obj.fail();
			}
		}

		if (!fPatternBrush && hBrush)
		{
			if (!DeleteObject(hBrush))
			{
				LogFail(_T("#### _stressdraw_t::ellipse - DeleteObject(hBrush: 0x%x) failed - Error: 0x%x ####\r\n"),
					hBrush, GetLastError());
				obj.fail();
			}
		}
	}

	return obj.status();
}


DWORD _stressdraw_t::roundrect()
{
	_stressrun_t obj(L"_stressdraw_t::roundrect");

	INT nLineWidth = 1;
	COLORREF cr;
	INT nDeltaX = 0, nDeltaY = 0;
	HBRUSH hBrush = NULL;
	RECT rcRandom;
	HPEN hPen;
	INT nEllipseWidth = 0, nEllipseHeight = 0;

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
		cr = _COLORREF_RANDOM_;
		DumpRGB(cr);
		nDeltaX = _DELTA_RANDOM_;
		nDeltaY = _DELTA_RANDOM_;
		nEllipseWidth = _ELLIPSE_WIDTH_RANDOM_;
		nEllipseHeight = _ELLIPSE_HEIGHT_RANDOM_;

		if (!SaveDC(hDC))
		{
			LogFail(_T("#### _stressdraw_t::roundrect - SaveDC failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
		}

		// initialize handles

		hPen = NULL;
		hBrush = NULL;

		BOOL fSolid = RANDOM_CHOICE;
		BOOL fPatternBrush = false;

		if (fSolid)
		{
			if (RANDOM_CHOICE)
			{
				LogVerbose(_T("_stressdraw_t::roundrect - Using pattern brush...\r\n"));
				hBrush = hPatternBrush;
				fPatternBrush = true;
			}
			else
			{
				LogVerbose(_T("_stressdraw_t::roundrect - Using solid brush...\r\n"));

				hBrush = CreateSolidBrush(cr);

				if (!hBrush)
				{
					LogFail(_T("#### _stressdraw_t::roundrect - CreateSolidBrush failed - Error: 0x%x ####\r\n"),
						GetLastError());
					obj.fail();
					goto cleanup;
				}
			}

			SelectObject(hDC, hBrush);
		}
		else
		{
			nLineWidth = _LINE_WIDTH_RANDOM_;

			BOOL fDashedPen = i % 2; // PS_SOLID: 0, PS_DASH: 1

			/* if the value specified by nWidth is greater than 1, the
			fnPenStyle parameter must be PS_NULL or PS_SOLID */

			if (nLineWidth > 1)
			{
				fDashedPen = PS_SOLID; // false
			}

			if (fDashedPen)
			{
				LogVerbose(_T("_stressdraw_t::roundrect - Using PS_DASH...\r\n"));
			}
			else
			{
				LogVerbose(_T("_stressdraw_t::roundrect - Using PS_SOLID...\r\n"));
			}

			hPen = CreatePen(fDashedPen, nLineWidth, cr);

			if (!hPen)
			{
				LogFail(_T("#### _stressdraw_t::roundrect - CreatePen failed - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
				goto cleanup;
			}

			SelectObject(hDC, hPen);
		}

		LogVerbose(_T("_stressdraw_t::roundrect - Calling RoundRect...\r\n"));

		if (!RoundRect(hDC, (rcRandom.left + nDeltaX), (rcRandom.top + nDeltaY),
			(rcRandom.right + nDeltaX), (rcRandom.bottom + nDeltaY), nEllipseWidth,
			nEllipseHeight))
		{
			LogFail(_T("#### _stressdraw_t::roundrect - RoundRect #%i failed - Error: 0x%x ####\r\n"),
				i, GetLastError());
			DumpFailRECT(rcRandom);
			DumpFailRGB(cr);
			LogFail(_T("_stressdraw_t::roundrect - nLineWidth: %d, nDeltaX: %d, nDeltaY: %d, nEllipseWidth: %d, nEllipseHeight: %d\r\n"),
				nLineWidth, nDeltaX, nDeltaY, nEllipseWidth, nEllipseHeight);
			obj.fail();
		}

	cleanup:
		if (!RestoreDC(hDC, -1))
		{
			LogFail(_T("#### _stressdraw_t::roundrect - RestoreDC failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
		}

		if (hPen)
		{
			if (!DeleteObject(hPen))
			{
				LogFail(_T("#### _stressdraw_t::roundrect - DeleteObject(hPen: 0x%x) failed - Error: 0x%x ####\r\n"),
					hPen, GetLastError());
				obj.fail();
			}
		}

		if (!fPatternBrush && hBrush)
		{
			if (!DeleteObject(hBrush))
			{
				LogFail(_T("#### _stressdraw_t::roundrect - DeleteObject(hBrush: 0x%x) failed - Error: 0x%x ####\r\n"),
					hBrush, GetLastError());
				obj.fail();
			}
		}
	}

	return obj.status();
}


DWORD _stressdraw_t::polygon()
{
	_stressrun_t obj(L"_stressdraw_t::polygon");

	COLORREF cr;
	INT nLineWidth = 1;
	HPEN hPen = NULL;
	INT nDeltaX = 0, nDeltaY = 0;
	HBRUSH hBrush = NULL;
	POINT rgpt[LOOPCOUNT_MAX];
	POINT rgptShaken[LOOPCOUNT_MAX];

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
		nLineWidth = _LINE_WIDTH_RANDOM_;
		cr = _COLORREF_RANDOM_;
		DumpRGB(cr);
		nDeltaX = _DELTA_RANDOM_;
		nDeltaY = _DELTA_RANDOM_;

		if (!SaveDC(hDC))
		{
			LogFail(_T("#### _stressdraw_t::polygon - SaveDC failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
		}

		// initialize handles

		hPen = NULL;
		hBrush = NULL;

		BOOL fSolid = RANDOM_CHOICE;
		BOOL fPatternBrush = false;

		if (fSolid)
		{
			if (RANDOM_CHOICE)
			{
				LogVerbose(_T("_stressdraw_t::polygon - Using pattern brush...\r\n"));
				hBrush = hPatternBrush;
				fPatternBrush = true;
			}
			else
			{
				LogVerbose(_T("_stressdraw_t::polygon - Using solid brush...\r\n"));

				hBrush = CreateSolidBrush(cr);

				if (!hBrush)
				{
					LogFail(_T("#### _stressdraw_t::polygon - CreateSolidBrush failed - Error: 0x%x ####\r\n"),
						GetLastError());
					obj.fail();
					goto cleanup;
				}
			}

			SelectObject(hDC, hBrush);
		}
		else
		{
			nLineWidth = _LINE_WIDTH_RANDOM_;

			BOOL fDashedPen = i % 2; // PS_SOLID: 0, PS_DASH: 1

			/* if the value specified by nWidth is greater than 1, the
			fnPenStyle parameter must be PS_NULL or PS_SOLID */

			if (nLineWidth > 1)
			{
				fDashedPen = PS_SOLID; // false
			}

			if (fDashedPen)
			{
				LogVerbose(_T("_stressdraw_t::polygon - Using PS_DASH...\r\n"));
			}
			else
			{
				LogVerbose(_T("_stressdraw_t::polygon - Using PS_SOLID...\r\n"));
			}

			hPen = CreatePen(fDashedPen, nLineWidth, cr);

			if (!hPen)
			{
				LogFail(_T("#### _stressdraw_t::polygon - CreatePen failed - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
				goto cleanup;
			}

			SelectObject(hDC, hPen);
		}

		memset(rgptShaken, 0, sizeof rgptShaken);

		for (iPoint = 0; iPoint < POINT_COUNT; iPoint++)
		{
			rgptShaken[iPoint] = rgpt[iPoint];
			rgptShaken[iPoint].x += nDeltaX;
			rgptShaken[iPoint].y += nDeltaY;
		}

		LogVerbose(_T("_stressdraw_t::polygon - Calling Polygon...\r\n"));

		if (!Polygon(hDC, rgptShaken, POINT_COUNT))
		{
			LogFail(_T("#### _stressdraw_t::polygon - Polygon #%i failed - Error: 0x%x ####\r\n"),
				i, GetLastError());

			for (iPoint = 0; iPoint < POINT_COUNT; iPoint++)
			{
				LogFail(_T("_stressdraw_t::polygon - pt.x: %d, pt.y: %d\r\n"),
					rgptShaken[iPoint].x, rgptShaken[iPoint].y);
			}

			DumpFailRGB(cr);
			LogFail(_T("_stressdraw_t::polygon - nLineWidth: %d, nDeltaX: %d, nDeltaY: %d\r\n"),
				nLineWidth, nDeltaX, nDeltaY);
			obj.fail();
		}

	cleanup:
		if (!RestoreDC(hDC, -1))
		{
			LogFail(_T("#### _stressdraw_t::polygon - RestoreDC failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
		}

		if (hPen)
		{
			if (!DeleteObject(hPen))
			{
				LogFail(_T("#### _stressdraw_t::polygon - DeleteObject(hPen: 0x%x) failed - Error: 0x%x ####\r\n"),
					hPen, GetLastError());
				obj.fail();
			}
		}

		if (!fPatternBrush && hBrush)
		{
			if (!DeleteObject(hBrush))
			{
				LogFail(_T("#### _stressdraw_t::polygon - DeleteObject(hBrush: 0x%x) failed - Error: 0x%x ####\r\n"),
					hBrush, GetLastError());
				obj.fail();
			}
		}
	}

	return obj.status();
}


DWORD _stressdraw_t::pixel()
{
	_stressrun_t obj(L"_stressdraw_t::pixel");

	POINT pt;
	COLORREF cr;

	const INT MAX_ITERATION = RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN) *
		RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN);

	for (INT i = 0; i < MAX_ITERATION; i++)
	{
		cr = _COLORREF_RANDOM_;
		DumpRGB(cr);

		pt = RandomPoint(&_rc);

		LogVerbose(_T("_stressdraw_t::pixel - Calling SetPixel...\r\n"));

		if (-1 == SetPixel(hDC, pt.x, pt.y, cr))
		{
			obj.warning2();
		}
	}

	return obj.status();
}


DWORD _stressdraw_t::icon()
{
	_stressrun_t obj(L"_stressdraw_t::icon");

	const MAX_ICON_CX = 48;
	const MAX_ICON_CY = 48;

	HICON hIcon = 0;

	WORD wIconID = ICON_FIRST;

	INT	x = 0;
	INT	y = 0;

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

			if (!DrawIconEx(hDC, x, y, hIcon, 0, 0, NULL, 0, DI_NORMAL))
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

	return obj.status();
}
