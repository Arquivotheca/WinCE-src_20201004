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


Module Name: winmgr.cpp

Abstract: stress module for window manager APIs

Notes: none

________________________________________________________________________________
*/

#include <windows.h>
#include <stressutils.h>
#include <stdlib.h>
#include "stressrun.h"
#include "common.h"
#include "wnd.h"


#define	WNDCLASS_WNDMGR_STRESS		_T("WNDCLASS_WNDMGR_STRESS")
#define	WNDTITLE_WNDMGR_STRESS		(_T("WNDMGR__")_T(TARGET_CPU)_T("_")_T(_TGTPLAT))
#define	CHANGED_WNDTITLE			(_T("CHANGED__")_T(TARGET_CPU)_T("_")_T(_TGTPLAT))


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


FLAG_DETAILS g_rgfdWndStyle[] = {
	NAME_VALUE_PAIR(NULL),
	NAME_VALUE_PAIR(WS_BORDER),
	NAME_VALUE_PAIR(WS_CAPTION),
	NAME_VALUE_PAIR(WS_CLIPCHILDREN),
	NAME_VALUE_PAIR(WS_CLIPSIBLINGS),
	NAME_VALUE_PAIR(WS_DISABLED),
	NAME_VALUE_PAIR(WS_DLGFRAME),
	NAME_VALUE_PAIR(WS_GROUP),				// first control of a group of controls
	NAME_VALUE_PAIR(WS_HSCROLL),
	NAME_VALUE_PAIR(WS_MAXIMIZEBOX),		// cannot be used with WS_EX_CONTEXTHELP
	NAME_VALUE_PAIR(WS_MINIMIZEBOX),		// cannot be used with WS_EX_CONTEXTHELP
	NAME_VALUE_PAIR(WS_OVERLAPPED),
	NAME_VALUE_PAIR(WS_SIZEBOX),
	NAME_VALUE_PAIR(WS_SYSMENU),
	NAME_VALUE_PAIR(WS_TABSTOP),
	NAME_VALUE_PAIR(WS_THICKFRAME),
	NAME_VALUE_PAIR(WS_VISIBLE),
	NAME_VALUE_PAIR(WS_VSCROLL)
};

FLAG_DETAILS g_rgfdWndStyleExclusive[] =
{
	NAME_VALUE_PAIR(WS_CHILD),				// cannot be used with WS_POPUP
	NAME_VALUE_PAIR(WS_POPUP)				// cannot be used with WS_CHILD
};

FLAG_DETAILS g_rgfdWndExStyle[] = {
	NAME_VALUE_PAIR(NULL),
	NAME_VALUE_PAIR(WS_EX_CLIENTEDGE),
	NAME_VALUE_PAIR(WS_EX_DLGMODALFRAME),
	NAME_VALUE_PAIR(WS_EX_NOACTIVATE),
	NAME_VALUE_PAIR(WS_EX_NOANIMATION),
	NAME_VALUE_PAIR(WS_EX_OVERLAPPEDWINDOW),
	NAME_VALUE_PAIR(WS_EX_STATICEDGE),
	NAME_VALUE_PAIR(WS_EX_TOOLWINDOW),
	NAME_VALUE_PAIR(WS_EX_TOPMOST),
	NAME_VALUE_PAIR(WS_EX_WINDOWEDGE)
};


UINT DoStressIteration (void);
DWORD Test_Windows(LPRECT lprc);
DWORD Test_WindowPos(LPRECT lprc);


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
							L"s2_winmgr", 	// Module name to be used in logging
							LOGZONE(SLOG_SPACE_GWES, SLOG_WINMGR),
							&mp         // Module params passed on the cmd line
							);

	// Note on Logging Zones:
	//
	// Logging is filtered at two different levels: by "logging space" and
	// by "logging zones".  The 16 logging spaces roughly corresponds to the major
	// feature areas in the system (including apps).  Each space has 16 sub-zones
	// for a finer filtering granularity.
	//
	// Use the LOGZONE(space, zones) macro to indicate the logging space
	// that your module will log under (only one allowed) and the zones
	// in that space that you will log under when the space is enabled
	// (may be multiple OR'd values).
	//
	// See \test\stress\stress\inc\logging.h for a list of available
	// logging spaces and the zones that are defined under each space


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

	//__________________________________________________________________________

	obj.test(Test_Windows(&rcWorkArea));

	obj.test(Test_WindowPos(&rcWorkArea));

	//__________________________________________________________________________

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


static void DumpStyle(DWORD dwStyle, DWORD dwExStyle)
{
	const BUFFER_SIZE = MAX_PATH * 4;
	TCHAR sz[BUFFER_SIZE];
	INT i = 0;
	bool fFirst = true;

	// window styles

	memset(sz, 0, sizeof sz);

	// normal styles

	for (i = 0; i < ARRAY_SIZE(g_rgfdWndStyle); i++)
	{
		if (g_rgfdWndStyle[i].dwFlag && (dwStyle & g_rgfdWndStyle[i].dwFlag) == g_rgfdWndStyle[i].dwFlag)
		{
			if (fFirst)
			{
				_tcscat(sz, g_rgfdWndStyle[i].lpszFlag);
				fFirst = false;
			}
			else
			{
				_tcscat(sz, _T(" | "));
				_tcscat(sz, g_rgfdWndStyle[i].lpszFlag);
			}
		}
	}

	// exclusive styles

	for (i = 0; i < ARRAY_SIZE(g_rgfdWndStyleExclusive); i++)
	{
		if (g_rgfdWndStyleExclusive[i].dwFlag && (dwStyle & g_rgfdWndStyleExclusive[i].dwFlag) == g_rgfdWndStyleExclusive[i].dwFlag)
		{
			if (fFirst)
			{
				_tcscat(sz, g_rgfdWndStyleExclusive[i].lpszFlag);
				fFirst = false;
			}
			else
			{
				_tcscat(sz, _T(" | "));
				_tcscat(sz, g_rgfdWndStyleExclusive[i].lpszFlag);
			}
		}
	}

	if (!_tcslen(sz))
	{
		_tcscat(sz, _T("NULL"));
	}

	LogComment(_T("Style: %s\r\n"), sz);

	// extend window styles

	fFirst = true;
	memset(sz, 0, sizeof sz);

	for (i = 0; i < ARRAY_SIZE(g_rgfdWndExStyle); i++)
	{
		if (g_rgfdWndExStyle[i].dwFlag && (dwStyle & g_rgfdWndExStyle[i].dwFlag) == g_rgfdWndExStyle[i].dwFlag)
		{
			if (fFirst)
			{
				_tcscat(sz, g_rgfdWndExStyle[i].lpszFlag);
				fFirst = false;
			}
			else
			{
				_tcscat(sz, _T(" | "));
				_tcscat(sz, g_rgfdWndExStyle[i].lpszFlag);
			}
		}
	}

	if (!_tcslen(sz))
	{
		_tcscat(sz, _T("NULL"));
	}

	LogComment(_T("Extended Style: %s\r\n"), sz);

	return;
}


static void DumpStyle(HWND hWnd)
{
	if (IsWindow(hWnd))
	{
		DumpStyle(GetWindowLong(hWnd, GWL_STYLE), GetWindowLong(hWnd, GWL_EXSTYLE));
	}

	return;
}


static BOOL GetRandomRedrawWindowFlag()
{
	FLAG_DETAILS rgfdInvalidation[] =
	{
		NAME_VALUE_PAIR(NULL),
		NAME_VALUE_PAIR(RDW_ERASE),
		NAME_VALUE_PAIR(RDW_INTERNALPAINT),
		NAME_VALUE_PAIR(RDW_INVALIDATE)
	};

	FLAG_DETAILS rgfdValidation[] =
	{
		NAME_VALUE_PAIR(NULL),
		NAME_VALUE_PAIR(RDW_NOERASE),
		NAME_VALUE_PAIR(RDW_VALIDATE)
	};

	FLAG_DETAILS rgfdRepaint[] =
	{
		NAME_VALUE_PAIR(NULL),
		NAME_VALUE_PAIR(RDW_ERASENOW),
		NAME_VALUE_PAIR(RDW_UPDATENOW)
	};

	FLAG_DETAILS rgfdChild[] =	// use in a mutually exclusive manner
	{
		NAME_VALUE_PAIR(NULL),
		NAME_VALUE_PAIR(RDW_ALLCHILDREN),
		NAME_VALUE_PAIR(RDW_NOCHILDREN)
	};

	const BUFFER_SIZE = MAX_PATH * 4;
	TCHAR sz[BUFFER_SIZE];
	bool fFirst = true;

	memset(sz, 0, sizeof sz);

	UINT uFlags = NULL;

	// cook up random flags

	if (RANDOM_CHOICE)	// invalidation
	{
		for (INT j = 0; j < (INT)(rand() % ARRAY_SIZE(rgfdInvalidation)); j++)
		{
			if (RANDOM_CHOICE)	// set various styles randomly
			{
				INT i = rand() % ARRAY_SIZE(rgfdInvalidation);
				uFlags |= rgfdInvalidation[i].dwFlag;

				if (rgfdInvalidation[i].dwFlag)
				{
					if (fFirst)
					{
						_tcscpy(sz, rgfdInvalidation[i].lpszFlag);
						fFirst = false;
					}
					else
					{
						_tcscat(sz, _T(" | "));
						_tcscat(sz, rgfdInvalidation[i].lpszFlag);
					}
				}
			}
		}
	}
	else	// validation
	{
		for (INT j = 0; j < (INT)(rand() % ARRAY_SIZE(rgfdValidation)); j++)
		{
			if (RANDOM_CHOICE)	// set various styles randomly
			{
				INT i = rand() % ARRAY_SIZE(rgfdValidation);
				uFlags |= rgfdValidation[i].dwFlag;

				if (rgfdValidation[i].dwFlag)
				{
					if (fFirst)
					{
						_tcscpy(sz, rgfdValidation[i].lpszFlag);
						fFirst = false;
					}
					else
					{
						_tcscat(sz, _T(" | "));
						_tcscat(sz, rgfdValidation[i].lpszFlag);
					}
				}
			}
		}
	}

	if (RANDOM_CHOICE)	// repaint
	{
		for (INT j = 0; j < (INT)(rand() % ARRAY_SIZE(rgfdRepaint)); j++)
		{
			if (RANDOM_CHOICE)	// set various styles randomly
			{
				INT i = rand() % ARRAY_SIZE(rgfdRepaint);
				uFlags |= rgfdRepaint[i].dwFlag;

				if (rgfdRepaint[i].dwFlag)
				{
					if (fFirst)
					{
						_tcscpy(sz, rgfdRepaint[i].lpszFlag);
						fFirst = false;
					}
					else
					{
						_tcscat(sz, _T(" | "));
						_tcscat(sz, rgfdRepaint[i].lpszFlag);
					}
				}
			}
		}
	}


	if (RANDOM_CHOICE)	// child (use in a mutually exclusive manner)
	{
		INT i = rand() % ARRAY_SIZE(rgfdChild);
		uFlags |= rgfdChild[i].dwFlag;

		if (rgfdChild[i].dwFlag)
		{
			if (fFirst)
			{
				_tcscpy(sz, rgfdChild[i].lpszFlag);
				fFirst = false;
			}
			else
			{
				_tcscat(sz, _T(" | "));
				_tcscat(sz, rgfdChild[i].lpszFlag);
			}
		}
	}

	if (!_tcslen(sz))
	{
		_tcscat(sz, _T("NULL"));
	}

	LogComment(_T("Style: %s\r\n"), sz);

	return uFlags;
}


/*

@func	RandomAreaLite, generates a random rectangle or region, satisfying
minimum width and height requirements within the given parent window's client
area.

@rdesc	TRUE if successful, FALSE otherwise

@parm	[out] PVOID pArea: pointer to the generated rectangle or region, the
caller is responsible for allocation

@parm	[in] HWND *prghWnd: array of windows, prghWnd[0] being the parent (root)

@parm	[in] BOOL fRegion: TRUE value causes the generated area to be a region,
FALSE value generates a rectangle

@note	**** generated region is always rectangular ***
		no overflow check
		leak alert: caller must call DeleteObject(hrgn) on the returned region
		**** Internally calls RandomRect ****

*/

BOOL RandomAreaLite(PVOID pArea, HWND hWnd, BOOL fRegion)
{
	BOOL fOk = TRUE;
	HRGN hrgnOccupied = NULL;
	RECT rc, rcParent;

	if (!SetRectEmpty(&rcParent) || !SetRectEmpty(&rc) || !IsWindow(hWnd))
	{
		fOk &= FALSE;
		goto CleanUp;
	}

	fOk &= GetClientRect(hWnd, &rcParent);

	/* generate a random rect within the rect from region data after
	satisfying minimum height and width requirements */

	RandomRect(&rc, &rcParent);

	// depending upon the flag, convert to region

	if (fRegion)
	{
		hrgnOccupied = CreateRectRgnIndirect(&rc);

		if (!hrgnOccupied)
		{
			fOk = FALSE;
			goto CleanUp;
		}

		memcpy(pArea, &hrgnOccupied, sizeof hrgnOccupied);
	}
	else
	{
		fOk &= CopyRect((PRECT)pArea, &rc);
	}

CleanUp:
	return fOk;
}


/*

@func:	Test_Windows, tests various windowing APIs

@rdesc:	stress return status

@param:	[in] LPRECT lprc: rectangle in screen coordinates within which new
windows are to be created

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

DWORD Test_Windows(LPRECT lprc)
{
	_stressrun_t obj(L"Test_Windows");

	const LOOP_COUNT = LOOPCOUNT_MIN;
	_wnd_t *rgpwnd[LOOP_COUNT];
	TCHAR szWndTitle[MAX_PATH];
	TCHAR szWndCls[MAX_PATH];
	TCHAR szTemp[MAX_PATH];
	RECT rc, rcClient, rcGet;
	HWND hWndFound = NULL;
	DWORD dwStyle, dwExStyle;
	HWND hParentWnd = NULL;
	const BOOL fSpawnThread = TRUE;

	memset(rgpwnd, 0, sizeof rgpwnd);

	for (INT i = 0; i < LOOP_COUNT; i++)
	{
		LogVerbose(_T("Test_Windows: [%i]: Generating random rectangle...\r\n"), i);

		SetRectEmpty(&rc);
		RandomRect(&rc, lprc);

		SetRectEmpty(&rcClient);	// important: GetClientRect test depends on this initialization

		// window style

		LogVerbose(_T("Test_Windows: [%i]: Generating random window styles...\r\n"), i);

		dwStyle = 0;
		dwExStyle = 0;

		for (INT j = 0; j < (INT)(rand() % ARRAY_SIZE(g_rgfdWndStyle)); j++)
		{
			if (RANDOM_CHOICE)	// set various styles randomly
			{
				dwStyle |= g_rgfdWndStyle[rand() % ARRAY_SIZE(g_rgfdWndStyle)].dwFlag;
			}

			if (RANDOM_CHOICE)	// set various extended styles randomly
			{
				dwExStyle |= g_rgfdWndExStyle[rand() % ARRAY_SIZE(g_rgfdWndExStyle)].dwFlag;
			}
		}

		// sometimes randomly choose  WS_POPUP or WS_CHILD (only one or none)

		dwStyle |= (RANDOM_CHOICE ? g_rgfdWndStyleExclusive[rand() % ARRAY_SIZE(g_rgfdWndStyleExclusive)].dwFlag : 0);

		// pick a parent window in random

		LogVerbose(_T("Test_Windows: [%i]: Picking a parent window in random...\r\n"), i);

		if (i)
		{
			if (dwStyle & WS_CHILD)
			{
				hParentWnd = rgpwnd[rand() % i]->hwnd();
			}
			else
			{
				hParentWnd = NULL;
			}
		}
		else
		{
			dwStyle &= ~WS_CHILD;	// first window must not be a child
		}

		// dump generated random window styles

		DumpStyle(dwStyle, dwExStyle);

		//______________________________________________________________________
		// AdjustWindowRectEx

		if (RANDOM_CHOICE)
		{
			LogVerbose(_T("Test_Windows: [%i]: Calling AdjustWindowRectEx...\r\n"), i);

			CopyRect(&rcClient, &rc);	// considering generated random rect rc as the client rectangle

			SetLastError(0);
			if (!AdjustWindowRectEx(&rc, dwStyle, FALSE, dwExStyle))
			{
				LogFail(_T("#### Test_Windows: AdjustWindowRectEx failed - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
				goto next;
			}
		}

		//______________________________________________________________________

		LogVerbose(_T("Test_Windows: [%i]: Generating window title...\r\n"), i);

		memset(szWndTitle, 0, sizeof szWndTitle);
		_stprintf(szWndTitle, _T("%u_0x%08x_%s"), GetTickCount(),
			(DWORD)GetCurrentThread(), WNDTITLE_WNDMGR_STRESS);

		LogVerbose(_T("Test_Windows: [%i]: Generating window class...\r\n"), i);

		memset(szWndCls, 0, sizeof szWndCls);
		_stprintf(szWndCls, _T("%s_0x%x"), WNDCLASS_WNDMGR_STRESS,
			(DWORD)GetCurrentThread());

		// randomly create windows always in spawned thread (with proper message pump)

		LogVerbose(_T("Test_Windows: [%i]: Creating random window - fSpawnThread: %d, Parent: 0x%x, Class: %s, Title: %s...\r\n"),
			i, fSpawnThread, hParentWnd, szWndCls, szWndTitle);

		DumpRECT(rc);

		SetLastError(0);
		rgpwnd[i] = new _wnd_t(g_hInstance, szWndTitle, dwStyle, dwExStyle, hParentWnd,
			_COLORREF_RANDOM_, szWndCls, fSpawnThread, &rc);

		if (!rgpwnd[i]->Exists())	// internally calls IsWindow
		{
			LogFail(_T("#### Test_Windows: Unable to create window - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto next;
		}

		LogVerbose(_T("Test_Windows: [%i]: Created 0x%x...\r\n"), i, rgpwnd[i]->hwnd());

		//______________________________________________________________________
		// IsWindowVisible (at this point, window is not visible)

		if ((!(rgpwnd[i]->GetStyle() & WS_VISIBLE)) && RANDOM_CHOICE)
		{
			LogVerbose(_T("Test_Windows: [%i]: Calling IsWindowVisible(invisible window)...\r\n"), i);

			if (IsWindowVisible(rgpwnd[i]->hwnd()))
			{
				LogFail(_T("#### Test_Windows: IsWindowVisible(invisible window) returned TRUE ####\r\n"));
				obj.fail();
				goto next;
			}
		}

		// show the window

		rgpwnd[i]->Show();			// internally calls ShowWindow
		rgpwnd[i]->Foreground();	// internally calls SetForegroundWindow
		rgpwnd[i]->Refresh();
		rgpwnd[i]->MessagePump();

		if (RANDOM_CHOICE)
		{
			LogVerbose(_T("Test_Windows: [%i]: Calling IsWindowVisible(invisible)...\r\n"), i);

			if (!IsWindowVisible(rgpwnd[i]->hwnd()))
			{
				LogFail(_T("#### Test_Windows: IsWindowVisible(invisible) returned FALSE ####\r\n"));
				obj.fail();
				goto next;
			}
		}

		//______________________________________________________________________
		// FindWindow only searches top-level windows & does not search child windows

		if (hParentWnd && !(rgpwnd[i]->GetStyle() & WS_CHILD))
		{
			// FindWindow(lpClassName, NULL)

			LogVerbose(_T("Test_Windows: [%i]: Calling FindWindow(lpClassName, NULL)...\r\n"), i);

			hWndFound = NULL;
			SetLastError(0);
			hWndFound = FindWindow(szWndCls, NULL);

			if (!IsWindow(hWndFound))
			{
				LogFail(_T("#### Test_Windows: FindWindow(lpClassName, NULL) failed - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
				goto next;
			}
			else
			{
				if (hWndFound != rgpwnd[i]->hwnd())
				{
					LogFail(_T("#### Test_Windows: FindWindow(lpClassName, NULL) returned invalid window handle ####\r\n"));
					obj.fail();
					goto next;

				}
			}

			// FindWindow(NULL, lpWindowName)

			LogVerbose(_T("Test_Windows: [%i]: Calling FindWindow(NULL, lpWindowName)...\r\n"), i);

			hWndFound = NULL;
			SetLastError(0);
			hWndFound = FindWindow(NULL, szWndTitle);

			if (!IsWindow(hWndFound))
			{
				LogFail(_T("#### Test_Windows: FindWindow(NULL, lpWindowName) failed - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
				goto next;
			}
			else
			{
				if (hWndFound != rgpwnd[i]->hwnd())
				{
					LogFail(_T("#### Test_Windows: FindWindow(NULL, lpWindowName) returned invalid window handle ####\r\n"));
					obj.fail();
					goto next;

				}
			}

			// FindWindow(lpClassName, lpWindowName)

			LogVerbose(_T("Test_Windows: [%i]: Calling FindWindow(lpClassName, lpWindowName)...\r\n"), i);

			hWndFound = NULL;
			SetLastError(0);
			hWndFound = FindWindow(szWndCls, szWndTitle);

			if (!IsWindow(hWndFound))
			{
				LogFail(_T("#### Test_Windows: FindWindow(lpClassName, lpWindowName) failed - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
				goto next;
			}
			else
			{
				if (hWndFound != rgpwnd[i]->hwnd())
				{
					LogFail(_T("#### Test_Windows: FindWindow(lpClassName, lpWindowName) returned invalid window handle ####\r\n"));
					obj.fail();
					goto next;

				}
			}
		}

		//______________________________________________________________________
		// GetWindowRect

		if (!(rgpwnd[i]->GetStyle() & WS_CHILD) && RANDOM_CHOICE)
		{
			LogVerbose(_T("Calling GetWindowRect...\r\n"));

			SetRectEmpty(&rcGet);
			SetLastError(0);
			if (!GetWindowRect(rgpwnd[i]->hwnd(), &rcGet))
			{
				LogFail(_T("#### Test_Windows: GetWindowRect failed - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
				goto next;
			}

			if (!EqualRect(&rcGet, &rc))
			{
				LogFail(_T("#### Test_Windows: GetWindowRect returned invalid window size ####\r\n"));
				LogFail(_T("____Test_Windows: Expected RECT____\r\n"));
				DumpFailRECT(rc);
				LogFail(_T("____Test_Windows: Actual RECT____\r\n"));
				DumpFailRECT(rcGet);
				obj.fail();
				goto next;
			}
		}


		//______________________________________________________________________
		// GetClientRect

		if (RANDOM_CHOICE)
		{
			LogVerbose(_T("Calling GetClientRect...\r\n"));

			SetRectEmpty(&rcGet);
			SetLastError(0);
			if (!GetClientRect(rgpwnd[i]->hwnd(), &rcGet))
			{
				LogFail(_T("#### Test_Windows: GetClientRect failed - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
				goto next;
			}

			if (dwStyle && !IsRectEmpty(&rcClient) && !(rgpwnd[i]->GetStyle() & WS_CHILD) &&
				!(rgpwnd[i]->GetExStyle() & WS_EX_STATICEDGE) && IsWindowVisible(rgpwnd[i]->hwnd()) &&
				!(rgpwnd[i]->GetStyle() & WS_VSCROLL) && !(rgpwnd[i]->GetStyle() & WS_HSCROLL))
			{
				// rcClient is in screen coordinates but rcGet is in client coordinates

				if ((RECT_WIDTH(rcClient) != RECT_WIDTH(rcGet)) || (RECT_HEIGHT(rcClient) != RECT_HEIGHT(rcGet)))
				{
					LogFail(_T("#### Test_Windows: GetClientRect returned invalid window size ####\r\n"));
					LogFail(_T("____Test_Windows: Expected RECT (in screen coordinates)____\r\n"));
					DumpFailRECT(rcClient);
					LogFail(_T("____Test_Windows: Actual RECT (in client coordinates)____\r\n"));
					DumpFailRECT(rcGet);
					obj.fail();
					DumpStyle(rgpwnd[i]->hwnd());
					goto next;
				}
			}
		}

		//______________________________________________________________________
		// GetWindowThreadProcessId

		if (RANDOM_CHOICE)
		{
			LogVerbose(_T("Calling GetWindowThreadProcessId...\r\n"));

			DWORD dwProcessID = 0;
			DWORD dwThreadID = 0;

			dwThreadID = GetWindowThreadProcessId(rgpwnd[i]->hwnd(), &dwProcessID);

			if (fSpawnThread)
			{
				if (rgpwnd[i]->GetThreadID() != dwThreadID)
				{
					LogFail(_T("#### Test_Windows: GetWindowThreadProcessId returned incorrect thread identifier ####\r\n"));
					obj.fail();
					goto next;
				}
			}
			else
			{
				if (GetCurrentThreadId() != dwThreadID)
				{
					LogFail(_T("#### Test_Windows: GetWindowThreadProcessId returned incorrect thread identifier ####\r\n"));
					obj.fail();
					goto next;
				}
			}

			if (GetCurrentProcessId() != dwProcessID)
			{
				LogFail(_T("#### Test_Windows: GetWindowThreadProcessId returned incorrect process identifier ####\r\n"));
				obj.fail();
				goto next;
			}
		}

		//______________________________________________________________________
		// IsChild

		if ((rgpwnd[i]->GetStyle() & WS_CHILD) && hParentWnd && RANDOM_CHOICE)
		{
			LogVerbose(_T("Calling IsChild...\r\n"));

			if (!IsChild(hParentWnd, rgpwnd[i]->hwnd()))
			{
				LogFail(_T("#### Test_Windows: IsChild failed to recognize child window ####\r\n"));
				obj.fail();
				goto next;
			}
		}

		//______________________________________________________________________
		// GetParent & SetParent

		if (!i && (rgpwnd[i]->GetStyle() & WS_CHILD) && RANDOM_CHOICE)
		{
			// GetParent

			LogVerbose(_T("Test_Windows: [%i]: Calling GetParent...\r\n"), i);

			SetLastError(0);
			if (hParentWnd != GetParent(rgpwnd[i]->hwnd()))
			{
				LogFail(_T("#### Test_Windows: GetParent returned incorrect parent - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
				goto next;
			}

			// SetParent

			LogVerbose(_T("Test_Windows: [%i]: Calling SetParent...\r\n"), i);

			INT iWnd = rand() % i;

			SetLastError(0);
			if (!SetParent(rgpwnd[i]->hwnd(), rgpwnd[iWnd]->hwnd()))
			{
				LogFail(_T("#### Test_Windows: SetParent failed - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
				goto next;
			}
			else
			{
				hParentWnd = rgpwnd[iWnd]->hwnd();
			}

			// GetParent again

			LogVerbose(_T("Test_Windows: [%i]: Calling GetParent again...\r\n"), i);

			SetLastError(0);
			if (hParentWnd != GetParent(rgpwnd[i]->hwnd()))
			{
				LogFail(_T("#### Test_Windows: GetParent returned incorrect parent - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
				goto next;
			}
		}

		//______________________________________________________________________
		// GetWindowTextLength, GetWindowText and SetWindowText

		if (RANDOM_CHOICE)
		{
			LogVerbose(_T("Test_Windows: [%i]: Calling GetWindowTextLength...\r\n"), i);

			SetLastError(0);
			if (_tcslen(szWndTitle) != (UINT)GetWindowTextLength(rgpwnd[i]->hwnd()))
			{
				LogFail(_T("#### Test_Windows: GetWindowTextLength returned incorrect length - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
				goto next;
			}
		}

		if (RANDOM_CHOICE)
		{
			// GetWindowText

			memset(szTemp, 0, sizeof szTemp);

			LogVerbose(_T("Test_Windows: [%i]: Calling GetWindowText...\r\n"), i);

			SetLastError(0);
			if (!GetWindowText(rgpwnd[i]->hwnd(), szTemp, MAX_PATH))
			{
				LogFail(_T("#### Test_Windows: GetWindowText failed - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
				goto next;
			}

			if (_tcscmp(szTemp, szWndTitle))
			{
				LogFail(_T("#### Test_Windows: GetWindowText returned invalid text - Expected: %s, Got: %s  ####\r\n"),
					szWndTitle, szTemp);
				obj.fail();
				goto next;
			}

			// SetWindowText

			memset(szWndTitle, 0, sizeof szWndTitle);
			_stprintf(szWndTitle, _T("%u_0x%08x_%s"), GetTickCount(),
				(DWORD)GetCurrentThread(), CHANGED_WNDTITLE);

			LogVerbose(_T("Test_Windows: [%i]: Calling SetWindowText...\r\n"), i);

			SetLastError(0);
			if (!SetWindowText(rgpwnd[i]->hwnd(), szWndTitle))
			{
				LogFail(_T("#### Test_Windows: SetWindowText failed - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
				goto next;
			}

			// GetWindowText again

			memset(szTemp, 0, sizeof szTemp);

			LogVerbose(_T("Test_Windows: [%i]: Calling GetWindowText again...\r\n"), i);

			SetLastError(0);
			if (!GetWindowText(rgpwnd[i]->hwnd(), szTemp, MAX_PATH))
			{
				LogFail(_T("#### Test_Windows: GetWindowText failed - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
				goto next;
			}

			if (_tcscmp(szTemp, szWndTitle))
			{
				LogFail(_T("#### Test_Windows: GetWindowText failed to return new text - Expected: %s, Got: %s  ####\r\n"),
					szWndTitle, szTemp);
				obj.fail();
				goto next;
			}
		}

		// ChildWindowFromPoint

		if (RANDOM_CHOICE)
		{
			RECT rcTmp;

			GetClientRect(rgpwnd[i]->hwnd(), &rcTmp);
			POINT ptRandom = RandomPoint(&rcTmp);

			LogVerbose(_T("Test_Windows: Calling ChildWindowFromPoint...\r\n"));
			DumpPOINT(ptRandom);

			HWND hWndChild = ChildWindowFromPoint(rgpwnd[i]->hwnd(), ptRandom);

			if (IsWindow(hWndChild) && (rgpwnd[i]->hwnd() != hWndChild))
			{
				if (rgpwnd[i]->hwnd() != GetParent(hWndChild))
				{
					LogWarn2(_T("#### Test_Windows: ChildWindowFromPoint returned different window ####\r\n"));
					DumpWarn2POINT(ptRandom);
				}
			}
		}

		// WindowFromPoint

		/* WindowFromPoint function does not retrieve a handle to a hidden or
		disabled window, even if the point is within the window. */

		if (RANDOM_CHOICE)
		{
			RECT rcTmp;

			GetWindowRect(rgpwnd[i]->hwnd(), &rcTmp);	// screen coordinates
			POINT ptRandom = RandomPoint(&rcTmp);

			LogVerbose(_T("Test_Windows: Calling WindowFromPoint...\r\n"));
			DumpPOINT(ptRandom);

			HWND hWnd = WindowFromPoint(ptRandom);

			if (!IsWindowVisible(rgpwnd[i]->hwnd()) || (rgpwnd[i]->GetStyle() & WS_DISABLED))
			{
				if (NULL != hWnd)
				{
					LogWarn2(_T("#### Test_Windows: WindowFromPoint returned non-null window handle ####\r\n"));
					DumpWarn2POINT(ptRandom);
				}
			}
			else
			{
				if (rgpwnd[i]->hwnd() != hWnd)
				{
					LogWarn2(_T("#### Test_Windows: WindowFromPoint returned different window handle ####\r\n"));
					DumpWarn2POINT(ptRandom);
				}
			}
		}

		// GetForegroundWindow

		if (RANDOM_CHOICE)
		{
			LogVerbose(_T("Test_Windows: Calling GetForegroundWindow...\r\n"));

			if (!IsWindow(GetForegroundWindow()))
			{
				LogFail(_T("#### Test_Windows: GetForegroundWindow returned invalid window handle ####\r\n"));
				obj.fail();
				goto next;
			}
		}

		// RedrawWindow

		if (RANDOM_CHOICE)
		{
			UINT uFlags = GetRandomRedrawWindowFlag();

			if (rgpwnd[i]->GetThreadHandle())	// proceed only when the window has it's own thread
			{
				RECT rcUpdate;
				if (!RandomAreaLite(&rcUpdate, rgpwnd[i]->hwnd(), FALSE))
				{
					LogFail(_T("#### Test_Windows: RandomAreaLite failed to generate random rect ####\r\n"));
					obj.fail();
					goto next;
				}

				HRGN hrgnUpdate = NULL;
				if (!RandomAreaLite(&hrgnUpdate, rgpwnd[i]->hwnd(), TRUE))
				{
					LogFail(_T("#### Test_Windows: RandomAreaLite failed to generate random region ####\r\n"));
					goto next;
				}

				if (RANDOM_CHOICE)	// use regions only
				{
					LogVerbose(_T("Test_Windows: Calling RedrawWindow(region only)...\r\n"), j);

					SetLastError(0);
					if (!RedrawWindow(rgpwnd[i]->hwnd(), NULL, hrgnUpdate, uFlags))
					{
						LogFail(_T("#### Test_Windows: RedrawWindow(region only) failed - Error: 0x%x ####\r\n"),
							GetLastError());
						obj.fail();
						goto RedrawWindow_CleanUp;
					}
				}
				else	// use either rect or both
				{
					if (RANDOM_CHOICE)	// use rect only
					{
						LogVerbose(_T("Test_Windows: Calling RedrawWindow(rect only)...\r\n"), j);

						SetLastError(0);
						if (!RedrawWindow(rgpwnd[i]->hwnd(), &rcUpdate, NULL, uFlags))
						{
							LogFail(_T("#### Test_Windows: RedrawWindow(rect only) failed - Error: 0x%x ####\r\n"),
								GetLastError());
							obj.fail();
							goto RedrawWindow_CleanUp;
						}
					}
					else	// use both rect and region
					{
						LogVerbose(_T("Test_Windows: Calling RedrawWindow(rect & region)...\r\n"), j);

						SetLastError(0);
						if (!RedrawWindow(rgpwnd[i]->hwnd(), &rcUpdate, hrgnUpdate, uFlags))
						{
							LogFail(_T("#### Test_Windows: RedrawWindow(rect & region) failed - Error: 0x%x ####\r\n"),
								GetLastError());
							obj.fail();
							goto RedrawWindow_CleanUp;
						}
					}
				}

			RedrawWindow_CleanUp:
				if (hrgnUpdate)
				{
					DeleteObject(hrgnUpdate);
				}
				goto next;
			}
		}

		rgpwnd[i]->MessagePump();
	next:;
	}

	//______________________________________________________________________
	// Destroy created windows in reverse order (important)

	for (INT j = (LOOP_COUNT - 1); j >= 0 ; j--)
	{
		LogVerbose(_T("Test_Windows: Destroying window #%i...\r\n"), j);

		delete rgpwnd[j];
		rgpwnd[j] = NULL;
	}

	return obj.status();
}


static BOOL GetRandomSetWindowPosFlag()
{
	FLAG_DETAILS rgfd[] =
	{
		NAME_VALUE_PAIR(NULL),
		NAME_VALUE_PAIR(SWP_DRAWFRAME),
		NAME_VALUE_PAIR(SWP_FRAMECHANGED),
		NAME_VALUE_PAIR(SWP_HIDEWINDOW),
		NAME_VALUE_PAIR(SWP_NOACTIVATE),
		NAME_VALUE_PAIR(SWP_NOMOVE),
		NAME_VALUE_PAIR(SWP_NOOWNERZORDER),
		NAME_VALUE_PAIR(SWP_NOREPOSITION),
		NAME_VALUE_PAIR(SWP_NOSIZE),
		NAME_VALUE_PAIR(SWP_NOZORDER),
		NAME_VALUE_PAIR(SWP_SHOWWINDOW)
	};

	const BUFFER_SIZE = MAX_PATH * 4;
	TCHAR sz[BUFFER_SIZE];
	bool fFirst = true;

	memset(sz, 0, sizeof sz);

	UINT uFlags = NULL;

	// cook up random flags

	if (RANDOM_CHOICE)
	{
		for (INT j = 0; j < (INT)(rand() % ARRAY_SIZE(rgfd)); j++)
		{
			if (RANDOM_CHOICE)	// set various styles randomly
			{
				INT i = rand() % ARRAY_SIZE(rgfd);
				uFlags |= rgfd[i].dwFlag;

				if (rgfd[i].dwFlag)
				{
					if (fFirst)
					{
						_tcscpy(sz, rgfd[i].lpszFlag);
						fFirst = false;
					}
					else
					{
						_tcscat(sz, _T(" | "));
						_tcscat(sz, rgfd[i].lpszFlag);
					}
				}
			}
		}
	}

	if (!_tcslen(sz))
	{
		_tcscat(sz, _T("NULL"));
	}

	LogComment(_T("Style: %s\r\n"), sz);

	return uFlags;
}


/*

@func:	Test_WindowPos, tests various positioning APIs

@rdesc:	stress return status

@param:	[in] LPRECT lprc: rectangle in screen coordinates within which new
windows are to be created

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

DWORD Test_WindowPos(LPRECT lprc)
{
	_stressrun_t obj(L"Test_WindowPos");

	struct SPECIAL_HWND
	{
		HWND _hWnd;
		LPTSTR _lpszHWND;
	};

	const LOOP_COUNT = LOOPCOUNT_MIN;
	_wnd_t *rgpwnd[LOOP_COUNT];
	TCHAR szWndTitle[MAX_PATH];
	TCHAR szWndCls[MAX_PATH];
	RECT rc;
	DWORD dwStyle, dwExStyle;
	HWND hParentWnd = NULL;
	const BOOL fSpawnThread = TRUE;

	memset(rgpwnd, 0, sizeof rgpwnd);

	for (INT i = 0; i < LOOP_COUNT; i++)
	{
		LogVerbose(_T("Test_WindowPos: [%i]: Generating random rectangle...\r\n"), i);

		SetRectEmpty(&rc);
		RandomRect(&rc, lprc);

		// window style

		LogVerbose(_T("Test_WindowPos: [%i]: Generating random window styles...\r\n"), i);

		dwStyle = 0;
		dwExStyle = 0;

		for (INT j = 0; j < (INT)(rand() % ARRAY_SIZE(g_rgfdWndStyle)); j++)
		{
			if (RANDOM_CHOICE)	// set various styles randomly
			{
				dwStyle |= g_rgfdWndStyle[rand() % ARRAY_SIZE(g_rgfdWndStyle)].dwFlag;
			}

			if (RANDOM_CHOICE)	// set various extended styles randomly
			{
				dwExStyle |= g_rgfdWndExStyle[rand() % ARRAY_SIZE(g_rgfdWndExStyle)].dwFlag;
			}
		}

		// sometimes randomly choose  WS_POPUP or WS_CHILD (only one or none)

		dwStyle |= (RANDOM_CHOICE ? g_rgfdWndStyleExclusive[rand() % ARRAY_SIZE(g_rgfdWndStyleExclusive)].dwFlag : 0);

		// pick a parent window in random

		LogVerbose(_T("Test_WindowPos: [%i]: Picking a parent window in random...\r\n"), i);

		if (i)
		{
			if (dwStyle & WS_CHILD)
			{
				hParentWnd = rgpwnd[rand() % i]->hwnd();
			}
			else
			{
				hParentWnd = NULL;
			}
		}
		else
		{
			dwStyle &= ~WS_CHILD;	// first window must not be a child
		}

		// dump generated random window styles

		DumpStyle(dwStyle, dwExStyle);

		//______________________________________________________________________

		LogVerbose(_T("Test_WindowPos: [%i]: Generating window title...\r\n"), i);

		memset(szWndTitle, 0, sizeof szWndTitle);
		_stprintf(szWndTitle, _T("%u_0x%08x_%s"), GetTickCount(),
			(DWORD)GetCurrentThread(), WNDTITLE_WNDMGR_STRESS);

		LogVerbose(_T("Test_WindowPos: [%i]: Generating window class...\r\n"), i);

		memset(szWndCls, 0, sizeof szWndCls);
		_stprintf(szWndCls, _T("%s_0x%x"), WNDCLASS_WNDMGR_STRESS,
			(DWORD)GetCurrentThread());

		// randomly create windows always in spawned thread (with proper message pump)

		LogVerbose(_T("Test_WindowPos: [%i]: Creating random window - fSpawnThread: %d, Parent: 0x%x, Class: %s, Title: %s...\r\n"),
			i, fSpawnThread, hParentWnd, szWndCls, szWndTitle);

		DumpRECT(rc);

		SetLastError(0);
		rgpwnd[i] = new _wnd_t(g_hInstance, szWndTitle, dwStyle, dwExStyle, hParentWnd,
			_COLORREF_RANDOM_, szWndCls, fSpawnThread, &rc);

		if (!rgpwnd[i]->Exists())	// internally calls IsWindow
		{
			LogFail(_T("#### Test_WindowPos: Unable to create window - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto next;
		}

		LogVerbose(_T("Test_WindowPos: [%i]: Created 0x%x...\r\n"), i, rgpwnd[i]->hwnd());

		// show the window

		rgpwnd[i]->Show();			// internally calls ShowWindow
		rgpwnd[i]->Foreground();	// internally calls SetForegroundWindow
		rgpwnd[i]->Refresh();
		rgpwnd[i]->MessagePump();

		//______________________________________________________________________
		// MoveWindow

		if (RANDOM_CHOICE)
		{
			LogVerbose(_T("Test_WindowPos: [%i]: Calling MoveWindow...\r\n"), i);

			RECT rcRandom, rcGet;

			SetRectEmpty(&rcRandom);
			RandomRect(&rcRandom, lprc);

			if (!MoveWindow(rgpwnd[i]->hwnd(), rcRandom.left, rcRandom.top,
				RECT_WIDTH(rcRandom), RECT_HEIGHT(rcRandom), RANDOM_CHOICE))
			{
				LogFail(_T("#### Test_WindowPos: [%i]: MoveWindow failed - Error: 0x%x ####\r\n"),
					i, GetLastError());
				obj.fail();
				goto next;
			}

			rgpwnd[i]->MessagePump();

			if (!(rgpwnd[i]->GetStyle() & WS_CHILD) && !(rgpwnd[i]->GetStyle() & WS_POPUP))
			{
				SetRectEmpty(&rcGet);

				GetWindowRect(rgpwnd[i]->hwnd(), &rcGet);

				if (!EqualRect(&rcGet, &rcRandom))
				{
					LogFail(_T("#### Test_WindowPos: [%i]: MoveWindow : Mismatched WindowRect ####\r\n"), i);
					LogFail(_T("____EXPECTED WINDOW RECT____\r\n"));
					DumpFailRECT(rcRandom);
					LogFail(_T("____ACTUAL WINDOW RECT____\r\n"));
					DumpFailRECT(rcGet);
					DumpStyle(rgpwnd[i]->hwnd());
					obj.fail();
					goto next;
				}
			}
		}

		//______________________________________________________________________
		// SetWindowPos

		if (RANDOM_CHOICE)
		{
			SPECIAL_HWND rgsh[] = {
				NAME_VALUE_PAIR(NULL),
				NAME_VALUE_PAIR(HWND_BOTTOM),
				NAME_VALUE_PAIR(HWND_NOTOPMOST),
				NAME_VALUE_PAIR(HWND_TOP),
				NAME_VALUE_PAIR(HWND_TOPMOST)
			};

			HWND hWndInsertAfter = (i) ? rgpwnd[rand() % i]->hwnd() : NULL;

			UINT uFlags = GetRandomSetWindowPosFlag();

			if (RANDOM_CHOICE)
			{
				INT iHWND = rand() % ARRAY_SIZE(rgsh);
				hWndInsertAfter = rgsh[iHWND]._hWnd;
				LogVerbose(_T("Test_WindowPos: [%i]: Calling SetWindowPos(%s)...\r\n"), i, rgsh[iHWND]._lpszHWND);
			}
			else
			{
				hWndInsertAfter = (i) ? rgpwnd[rand() % i]->hwnd() : NULL;
				LogVerbose(_T("Test_WindowPos: [%i]: Calling SetWindowPos(0x%x)...\r\n"), i, hWndInsertAfter);
			}

			RECT rcRandom;

			SetRectEmpty(&rcRandom);
			RandomRect(&rcRandom, lprc);

			if (!SetWindowPos(rgpwnd[i]->hwnd(), hWndInsertAfter, rcRandom.left,
				rcRandom.top, RECT_WIDTH(rcRandom), RECT_HEIGHT(rcRandom), uFlags))
			{
				LogFail(_T("#### Test_WindowPos: [%i]: SetWindowPos failed - Error: 0x%x ####\r\n"),
					i, GetLastError());
				obj.fail();
				goto next;
			}

			rgpwnd[i]->MessagePump();
		}

		//______________________________________________________________________
		// BringWindowToTop

		if (RANDOM_CHOICE)
		{
			LogVerbose(_T("Test_WindowPos: [%i]: Calling BringWindowToTop...\r\n"), i);

			if (!BringWindowToTop(rgpwnd[i]->hwnd()))
			{
				LogFail(_T("#### Test_WindowPos: [%i]: BringWindowToTop failed - Error: 0x%x ####\r\n"),
					i, GetLastError());
				DumpStyle(rgpwnd[i]->hwnd());
				obj.fail();
				goto next;
			}

			rgpwnd[i]->MessagePump();
		}

		//______________________________________________________________________
		// BeginDeferWindowPos, DeferWindowPos & EndDeferWindowPos

		if (i && RANDOM_CHOICE)
		{
			INT nNumWindows =  i + 1;

			// BeginDeferWindowPos

			LogVerbose(_T("Test_WindowPos: [%i]: Calling BeginDeferWindowPos...\r\n"), i);

			HDWP hdwp = BeginDeferWindowPos(nNumWindows);

			if (!hdwp)
			{
				LogFail(_T("#### Test_WindowPos: [%i]: BeginDeferWindowPos failed - Error: 0x%x ####\r\n"),
					i, GetLastError());
				DumpStyle(rgpwnd[i]->hwnd());
				obj.fail();
				goto next;
			}

			// DeferWindowPos

			SPECIAL_HWND rgshDeferWndPos[] = {
				NAME_VALUE_PAIR(NULL),
				NAME_VALUE_PAIR(HWND_BOTTOM),
				NAME_VALUE_PAIR(HWND_NOTOPMOST),
				NAME_VALUE_PAIR(HWND_TOP),
				NAME_VALUE_PAIR(HWND_TOPMOST)
			};

			for (INT j = 0; j < nNumWindows; j++)
			{
				LogVerbose(_T("Test_WindowPos: [%i, %i]: Calling DeferWindowPos...\r\n"), i, j);

				INT iHWND = rand() % ARRAY_SIZE(rgshDeferWndPos);

				UINT uFlags = GetRandomSetWindowPosFlag();

				RECT rcRandom;
				SetRectEmpty(&rcRandom);
				RandomRect(&rcRandom, &rc);

				hdwp = DeferWindowPos(hdwp, rgpwnd[j]->hwnd(),
							rgshDeferWndPos[iHWND]._hWnd, rcRandom.left,
							rcRandom.top, RECT_WIDTH(rcRandom),
							RECT_HEIGHT(rcRandom), uFlags);
				if (!hdwp)
				{
					LogFail(_T("#### Test_WindowPos: [%i, %i]: DeferWindowPos(%s) failed - Error: 0x%x ####\r\n"),
						i, j, rgshDeferWndPos[iHWND]._lpszHWND, GetLastError());
					DumpStyle(rgpwnd[i]->hwnd());
					obj.fail();

					/* if a call to DeferWindowPos fails, the application should
					abandon the window-positioning operation and not call
					EndDeferWindowPos. */

					goto next;
				}
			}

			// EndDeferWindowPos

			LogVerbose(_T("Test_WindowPos: [%i]: Calling EndDeferWindowPos...\r\n"), i );

			if (!EndDeferWindowPos(hdwp))
			{
					LogFail(_T("#### Test_WindowPos: [%i, %i]: EndDeferWindowPos failed - Error: 0x%x ####\r\n"),
						i, j, GetLastError());
					DumpStyle(rgpwnd[i]->hwnd());
					obj.fail();
					goto next;
			}

			rgpwnd[i]->MessagePump();
		}

		next:;
	}

	//______________________________________________________________________
	// Destroy created windows in reverse order (important)

	for (INT j = (LOOP_COUNT - 1); j >= 0 ; j--)
	{
		LogVerbose(_T("Test_WindowPos: Destroying window #%i...\r\n"), j);

		delete rgpwnd[j];
		rgpwnd[j] = NULL;
	}

	return obj.status();
}


static LRESULT CALLBACK TestWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return CallWindowProc(DefWindowProc, hWnd, uMsg, wParam, lParam);
}


/*

@func:	Test_WindowCls, tests various window class APIs

@rdesc:	stress return status

@param:	[in] LPRECT lprc: rectangle in screen coordinates within which new
windows are to be created

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

DWORD Test_WindowCls(LPRECT lprc)
{
	_stressrun_t obj(L"Test_WindowCls");

	TCHAR szWndTitle[MAX_PATH];
	TCHAR szWndCls[MAX_PATH];
	WNDCLASS wc;
	ATOM atom = 0;
	HWND hWnd = NULL;

	LogVerbose(_T("Test_WindowCls: Generating window title...\r\n"));

	memset(szWndTitle, 0, sizeof szWndTitle);
	_stprintf(szWndTitle, _T("%u_0x%08x_%s"), GetTickCount(),
		(DWORD)GetCurrentThread(), WNDTITLE_WNDMGR_STRESS);

	LogVerbose(_T("Test_WindowCls: Generating window class...\r\n"));

	memset(szWndCls, 0, sizeof szWndCls);
	_stprintf(szWndCls, _T("%s_0x%x"), WNDCLASS_WNDMGR_STRESS,
		(DWORD)GetCurrentThread());


	//______________________________________________________________________
	// RegisterClass

	LogVerbose(_T("Calling RegisterClass...\r\n"));

	memset(&wc, 0, sizeof wc);
    wc.style = 0;
	wc.lpfnWndProc = (WNDPROC)DefWindowProc;
    wc.cbClsExtra = RANDOM_INT(16, 0);
    wc.cbWndExtra = RANDOM_INT(16, 0);
	wc.hInstance = g_hInstance;
    wc.hIcon = 0;
	wc.hCursor = 0;
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = szWndCls;

	SetLastError(0);
	atom = RegisterClass(&wc);
	if (!atom)
	{
		LogFail(_T("#### Test_WindowCls: RegisterClass failed - Error: 0x%x ####\r\n"),
			GetLastError());
		obj.fail();
		goto done;
	}

	// create the window

	LogVerbose(_T("Calling CreateWindowEx...\r\n"));

	hWnd = CreateWindowEx(
     	0,
		szWndCls,
		szWndTitle,
		WS_OVERLAPPED | WS_CAPTION | WS_BORDER | WS_VISIBLE | WS_SYSMENU,
		lprc->left,
		lprc->top,
		RECT_WIDTH((*lprc)),
		RECT_HEIGHT((*lprc)),
		NULL,
		NULL,
		g_hInstance,
		NULL
	);

	if (!hWnd)
	{
		LogFail(_T("#### Test_WindowCls: CreateWindowEx failed - Error: 0x%x ####\r\n"),
			GetLastError());
		obj.fail();
		goto cleanup;
	}

	SetForegroundWindow(hWnd);
	ShowWindow(hWnd, SW_SHOWNORMAL);
	UpdateWindow(hWnd);

	MSG msg;
	INT i;

	for (i = 0; i < 10; i++)
	{
		while (PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		Sleep(50);
	}

	//______________________________________________________________________
	// GetClassInfo

	if (RANDOM_CHOICE)
	{
		LogVerbose(_T("Calling GetClassInfo...\r\n"));

		WNDCLASS wcTmp;
		memset(&wcTmp, 0, sizeof wcTmp);

		SetLastError(0);
		if (!GetClassInfo(g_hInstance, RANDOM_CHOICE ? szWndCls : (LPTSTR)atom, &wcTmp))
		{
			LogFail(_T("#### Test_WindowCls: GetClassInfo failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto cleanup;

		}

		if (memcmp(&wc, &wcTmp, sizeof wc))
		{
			LogFail(_T("#### Test_WindowCls: GetClassInfo returned mismatched WNDCLASS ####\r\n"));
			obj.fail();
			goto cleanup;
		}
	}


	//______________________________________________________________________
	// GetClassLong

	if (RANDOM_CHOICE)
	{
		LogVerbose(_T("Test_WindowCls: Calling GetClassLong(GCL_HCURSOR)...\r\n"));

		SetLastError(0);
		if ((HCURSOR)GetClassLong(hWnd, GCL_HCURSOR) != wc.hCursor)
		{
			LogFail(_T("#### Test_WindowCls: GetClassLong(GCL_HCURSOR) failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto cleanup;
		}
	}

	if (RANDOM_CHOICE)
	{
		LogVerbose(_T("Test_WindowCls: Calling GetClassLong(GCL_HICON)...\r\n"));

		SetLastError(0);
		if ((HICON)GetClassLong(hWnd, GCL_HICON) != wc.hIcon)
		{
			LogFail(_T("#### Test_WindowCls: GetClassLong(GCL_HICON) failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto cleanup;
		}
	}

	if (RANDOM_CHOICE)
	{
		LogVerbose(_T("Test_WindowCls: Calling GetClassLong(GCL_STYLE)...\r\n"));

		SetLastError(0);
		if (GetClassLong(hWnd, GCL_STYLE) != wc.style)
		{
			LogFail(_T("#### Test_WindowCls: GetClassLong(GCL_STYLE) failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto cleanup;
		}
	}

	//______________________________________________________________________
	// SetClassLong & GetClassLong

	if (RANDOM_CHOICE)
	{
		HCURSOR hCursor = LoadCursor(NULL, IDC_ARROW);

		LogVerbose(_T("Test_WindowCls: Calling SetClassLong(GCL_HCURSOR)...\r\n"));

		SetLastError(0);
		if ((HCURSOR)SetClassLong(hWnd, GCL_HCURSOR, (LONG)hCursor) != wc.hCursor)
		{
			LogFail(_T("#### Test_WindowCls: SetClassLong(GCL_HCURSOR) failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto cleanup;
		}

		LogVerbose(_T("Test_WindowCls: Calling GetClassLong(GCL_HCURSOR)...\r\n"));

		SetLastError(0);
		if ((HCURSOR)GetClassLong(hWnd, GCL_HCURSOR) != hCursor)
		{
			LogFail(_T("#### Test_WindowCls: GetClassLong(GCL_HCURSOR) failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto cleanup;
		}

	}

	//______________________________________________________________________
	// GetClassName

	if (RANDOM_CHOICE)
	{
		TCHAR szTemp[MAX_PATH];

		memset(szTemp, 0, sizeof szTemp);

		LogVerbose(_T("Test_WindowCls: Calling GetClassName...\r\n"));

		SetLastError(0);
		if (!GetClassName(hWnd, szTemp, MAX_PATH))
		{
			LogFail(_T("#### Test_WindowCls: GetClassName failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto cleanup;
		}

		if (_tcsicmp(szTemp, szWndCls))
		{
			LogFail(_T("#### Test_WindowCls: Mismatched GetClassName return - Expected: %s, Received: %s ####\r\n"),
				szWndCls, szTemp);
			obj.fail();
			goto cleanup;
		}
	}

cleanup:

	// destroy created window

	if (hWnd)
	{
		if (!DestroyWindow(hWnd))
		{
			LogFail(_T("#### Test_WindowCls: DestroyWindow failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
		}
	}


	//______________________________________________________________________
	// UnregisterClass

	LogVerbose(_T("Test_WindowCls: Calling UnregisterClass...\r\n"));

	SetLastError(0);
	if (!UnregisterClass((LPTSTR)atom, g_hInstance))
	{
		LogFail(_T("#### Test_WindowCls: UnregisterClass failed - Error: 0x%x ####\r\n"),
			GetLastError());
		obj.fail();
		goto done;
	}

done:
	return obj.status();
}


/*

@func:	Test_Other, tests other window manager APIs

@rdesc:	stress return status

@param:	[in] LPRECT lprc: rectangle in screen coordinates within which new
windows are to be created

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

DWORD Test_Other(LPRECT lprc)
{
	_stressrun_t obj(L"Test_Other");

	const LOOP_COUNT = LOOPCOUNT_MIN;
	_wnd_t *rgpwnd[LOOP_COUNT];
	TCHAR szWndTitle[MAX_PATH];
	TCHAR szWndCls[MAX_PATH];
	RECT rc;
	DWORD dwStyle, dwExStyle;
	HWND hParentWnd = NULL;

	memset(rgpwnd, 0, sizeof rgpwnd);

	for (INT i = 0; i < LOOP_COUNT; i++)
	{
		LogVerbose(_T("Test_Other: [%i]: Generating random rectangle...\r\n"), i);

		SetRectEmpty(&rc);
		RandomRect(&rc, lprc);

		// window style

		LogVerbose(_T("Test_Other: [%i]: Generating random window styles...\r\n"), i);

		dwStyle = 0;
		dwExStyle = 0;

		for (INT j = 0; j < (INT)(rand() % ARRAY_SIZE(g_rgfdWndStyle)); j++)
		{
			if (RANDOM_CHOICE)	// set various styles randomly
			{
				dwStyle |= g_rgfdWndStyle[rand() % ARRAY_SIZE(g_rgfdWndStyle)].dwFlag;
			}

			if (RANDOM_CHOICE)	// set various extended styles randomly
			{
				dwExStyle |= g_rgfdWndExStyle[rand() % ARRAY_SIZE(g_rgfdWndExStyle)].dwFlag;
			}
		}

		// sometimes randomly choose  WS_POPUP or WS_CHILD (only one or none)

		dwStyle |= (RANDOM_CHOICE ? g_rgfdWndStyleExclusive[rand() % ARRAY_SIZE(g_rgfdWndStyleExclusive)].dwFlag : 0);

		// pick a parent window in random

		LogVerbose(_T("Test_Other: [%i]: Picking a parent window in random...\r\n"), i);

		if (i)
		{
			if (dwStyle & WS_CHILD)
			{
				hParentWnd = rgpwnd[rand() % i]->hwnd();
			}
			else
			{
				hParentWnd = NULL;
			}
		}
		else
		{
			dwStyle &= ~WS_CHILD;	// first window must not be a child
		}

		// dump generated random window styles

		DumpStyle(dwStyle, dwExStyle);

		//______________________________________________________________________

		LogVerbose(_T("Test_Other: [%i]: Generating window title...\r\n"), i);

		memset(szWndTitle, 0, sizeof szWndTitle);
		_stprintf(szWndTitle, _T("%u_0x%08x_%s"), GetTickCount(),
			(DWORD)GetCurrentThread(), WNDTITLE_WNDMGR_STRESS);

		LogVerbose(_T("Test_Other: [%i]: Generating window class...\r\n"), i);

		memset(szWndCls, 0, sizeof szWndCls);
		_stprintf(szWndCls, _T("%s_0x%x"), WNDCLASS_WNDMGR_STRESS,
			(DWORD)GetCurrentThread());

		LogVerbose(_T("Test_Other: [%i]: Creating random window - Parent: 0x%x, Class: %s, Title: %s...\r\n"),
			i, hParentWnd, szWndCls, szWndTitle);

		DumpRECT(rc);

		SetLastError(0);
		rgpwnd[i] = new _wnd_t(g_hInstance, szWndTitle, dwStyle, dwExStyle, hParentWnd,
			_COLORREF_RANDOM_, szWndCls, TRUE, &rc);

		if (!rgpwnd[i]->Exists())	// internally calls IsWindow
		{
			LogFail(_T("#### Test_Other: Unable to create window - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto next;
		}

		LogVerbose(_T("Test_Other: [%i]: Created 0x%x...\r\n"), i, rgpwnd[i]->hwnd());

		// show the window

		rgpwnd[i]->Show();			// internally calls ShowWindow
		rgpwnd[i]->Foreground();	// internally calls SetForegroundWindow
		rgpwnd[i]->Refresh();

		//______________________________________________________________________
		// InvalidateRect

		if (RANDOM_CHOICE)
		{
			LogVerbose(_T("Test_Other: [%i]: Calling InvalidateRect...\r\n"), i);

			RECT rcClient, rcRandom;

			GetClientRect(rgpwnd[i]->hwnd(), &rcClient);
			SetRectEmpty(&rcRandom);
			RandomRect(&rcRandom, &rcClient);

			if (!InvalidateRect(rgpwnd[i]->hwnd(), &rcRandom, RANDOM_CHOICE))
			{
				LogFail(_T("#### Test_Other: [%i]: InvalidateRect failed - Error: 0x%x ####\r\n"),
					i, GetLastError());
				obj.fail();
				goto next;
			}
		}

		//______________________________________________________________________
		// SetActiveWindow

		if (RANDOM_CHOICE)
		{
			LogVerbose(_T("Test_Other: [%i]: Calling SetActiveWindow...\r\n"), i);

			if (!SetActiveWindow(rgpwnd[i]->hwnd()))
			{
				LogFail(_T("#### Test_Other: [%i]: SetActiveWindow failed - Error: 0x%x ####\r\n"),
					i, GetLastError());
				obj.fail();
				goto next;
			}
		}

		next:;
	}

	//______________________________________________________________________
	// Destroy created windows in reverse order (important)

	for (INT j = (LOOP_COUNT - 1); j >= 0 ; j--)
	{
		LogVerbose(_T("Test_Other: Destroying window #%i...\r\n"), j);

		delete rgpwnd[j];
		rgpwnd[j] = NULL;
	}

	return obj.status();
}
