//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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


#define	WNDCLASS_MAIN			_T("GDI_MAIN_QA")


HINSTANCE g_hInstance = NULL;
LPTSTR g_rgszStr[] = {
	_T("alpha"),
	_T("beta"),
	_T("gamma"),
	_T("delta"),
	_T("epsilon"),
	_T("zêta"),
	_T("êta"),
	_T("thêta"),
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
	DWORD			dwStartTime = GetTickCount();
	UINT			ret;
	LPTSTR			tszCmdLine;
	int				retValue = 0;  // Module return value.  You should return 0 unless you had to abort.
	MODULE_PARAMS	mp;            // Cmd line args arranged here.  Used to init the stress utils.
	STRESS_RESULTS	results;       // Cumulative test results for this module.

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
							L"s2_gdi", 	// Module name to be used in logging
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

@func:	RandomRect, generates a valid random rect within specified rect

@rdesc:	true if successful, false otherwise

@param:	[in] LPRECT prc: generated random rect

@param:	[in] const LPRECT prcContainer: generated random rect is inside this rect

@fault:	none

@pre:	none

@post:	none

@note:	none

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

@func:	RandomPoint, generates a random point inside given rect

@rdesc:	generated random point as a POINT structure

@param:	[in] const RECT *prcContainer: generated random point lies within this rect

@fault:	none

@pre:	none

@post:	none

@note:	none

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

@func:	Test_Blt, tests various Blt APIs

@rdesc:	stress return status

@param:	[in] HWND hWnd: handle to the main window

@fault:	none

@pre:	none

@post:	none

@note:	none

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

done:
	return obj.status();
}


/*

@func:	Test_Drawing, tests various drawing APIs

@rdesc:	stress return status

@param:	[in] HWND hWnd: handle to the main window

@fault:	none

@pre:	none

@post:	none

@note:	none

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

done:
	return obj.status();
}


/*

@func:	Test_Text, tests various text APIs

@rdesc:	stress return status

@param:	[in] HWND hWnd: handle to the main window

@fault:	none

@pre:	none

@post:	none

@note:	none

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

@func:	Test_Region_Clip, tests various region and clipping APIs

@rdesc:	stress return status

@param:	[in] HWND hWnd: handle to the main window

@fault:	none

@pre:	none

@post:	none

@note:	none

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

@func:	Test_Miscellaneous, tests other GDI APIs not covered in the other tests

@rdesc:	stress return status

@param:	[in] HWND hWnd: handle to the main window

@fault:	none

@pre:	none

@post:	none

@note:	none

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

	if (RANDOM_CHOICE)	// DC APIs
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

	if (RANDOM_CHOICE)	// Other APIs
	{
		const INT BMP_CX = 0x10, BMP_CY = 0x10;

		// CreateBitmap

		LogVerbose(_T("Test_Miscellaneous - Calling CreateBitmap...\r\n"));

		HBITMAP hBmp1 = CreateBitmap(BMP_CX, BMP_CY, 1, 2, NULL);

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
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		Sleep(10);
	}

	return;
}

