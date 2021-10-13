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


Module Name: dlg.cpp

Abstract: dialog manager and window controls stress module

Notes: none

________________________________________________________________________________
*/

#include <windows.h>
#include <stressutils.h>
#include "resource.h"
#include "stressrun.h"
#include "common.h"


#define	WNDCLASS_PARENT			_T("DLG_PARENT_QA")


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


struct DLG_INFO
{
	HWND hwndDlg;
	HWND rghwndCtrls[MAX_PATH];
	INT nCtrlCount;
	BOOL fExitingThread;
};


_stressrun_t g_objDlgProc_DIALOG_APP0;
_stressrun_t g_objDlgProc_DIALOG_APP1;
_stressrun_t g_objDlgProc_Test;


UINT DoStressIteration (void);
BOOL CALLBACK DlgProc_DIALOG_APP0(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
DWORD Test_DIALOG_APP0(HWND hDlg);
DWORD Test_Dialog(HWND hWndParent, LPTSTR lpszDlgName, UINT uBlinkTime, DLGPROC lpDialogFunc);
DWORD Test_ListBox_MultiSel(HWND hWndList);
DWORD Test_EditBox_MultiLine(HWND hWndEdit);
BOOL CALLBACK DlgProc_DIALOG_APP1(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
DWORD Test_DIALOG_APP1(HWND hDlg);
DWORD Test_ComboBox(HWND hWndCombo);
DWORD Test_ListBox_SingleSel(HWND hWndList);
DWORD Test_Static(HWND hWndStaticBitmap, HWND hWndStaticIcon);
DWORD ThreadKeyProc(DLG_INFO *di);

extern DWORD Test_DialogManager(HWND hWnd);


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
							L"s2_dlg", 	// Module name to be used in logging
							LOGZONE(SLOG_SPACE_GWES, SLOG_DLGMGR | SLOG_CONTROLS),
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

	HWND hWnd = NULL;
    WNDCLASS wc;
    RECT rcWorkArea;
    BOOL fParentWindow = RANDOM_BOOL(TRUE, FALSE);

	if (fParentWindow)
	{
	 	// SystemParametersInfo with SPI_GETWORKAREA

		LogVerbose(_T("SystemParametersInfo with SPI_GETWORKAREA...\r\n"));

		if (!SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0))
		{
			LogWarn1(_T("#### SystemParametersInfo(SPI_GETWORKAREA) failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.warning1();
			SetRect(&rcWorkArea, 0, 0, GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN));
		}

		LogVerbose(_T("Registering parent window class...\r\n"));

	    wc.style = 0;
    	wc.lpfnWndProc = (WNDPROC)DefWindowProc;
	    wc.cbClsExtra = 0;
	    wc.cbWndExtra = 0;
    	wc.hInstance = g_hInstance;
	    wc.hIcon = 0;
    	wc.hCursor = 0;
	    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	    wc.lpszMenuName = 0;
	    wc.lpszClassName = WNDCLASS_PARENT;

		if (!RegisterClass(&wc))
		{
			LogWarn2(_T("RegisterClass failed - Error: 0x%x\r\n"),
				GetLastError());
			obj.warning2();
		}

		LogComment(_T("Creating parent window...\r\n"));

		hWnd = CreateWindowEx(
	     	0,
			WNDCLASS_PARENT,
			_T("PARENT WINDOW"),
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

		ShowWindow(hWnd, SW_SHOWNORMAL);
		UpdateWindow(hWnd);
		MessagePump();
	}


	if (IsWindow(hWnd))
	{
		obj.test(Test_Dialog(hWnd, _T("DIALOG_APP0"), rand(), DlgProc_DIALOG_APP0));
	}

	if (IsWindow(hWnd))
	{
		obj.test(Test_Dialog(hWnd, _T("DIALOG_APP1"), rand(), DlgProc_DIALOG_APP1));
	}

	if (IsWindow(hWnd))
	{
		obj.test(Test_DialogManager(hWnd));
	}


	// dlgproc details

	obj.test(g_objDlgProc_DIALOG_APP0.status());
	g_objDlgProc_DIALOG_APP0.reset();

	obj.test(g_objDlgProc_DIALOG_APP1.status());
	g_objDlgProc_DIALOG_APP1.reset();

	obj.test(g_objDlgProc_Test.status());
	g_objDlgProc_Test.reset();

	if (fParentWindow)
	{
		if (hWnd)
		{
			if (!DestroyWindow(hWnd))
			{
				LogFail(_T("#### DestroyWindow failed - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
			}
		}
	}

cleanup:
	if (fParentWindow)
	{
		if (!UnregisterClass(WNDCLASS_PARENT, g_hInstance))
		{
			LogWarn2(_T("UnregisterClass failed - Error: 0x%x\r\n"),
				GetLastError());
			obj.warning2();
		}
	}

	return obj.status();
}


/*

@func:	Test_Dialog, main test function to accept different dialog resource
names, dlgproc etc.

@rdesc:	stress return status

@param:	[in] HWND hWndParent: handle to the parent window, null value indicates
no parent window available

@param:	[in] LPTSTR lpszDlgName: dialog resource name to use

@param:	[in] UINT uBlinkTime: caret blink time

@param:	[in] DLGPROC lpDialogFunc: pointer to the dialog procedure

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

DWORD Test_Dialog(HWND hWndParent, LPTSTR lpszDlgName, UINT uBlinkTime, DLGPROC lpDialogFunc)
{
	_stressrun_t obj(L"Test_Dialog");

	LogVerbose(_T("Calling CreateDialog...\r\n"));

	HWND hDlgMain = CreateDialog(g_hInstance, lpszDlgName, hWndParent, lpDialogFunc);

	if (!hDlgMain) // dlg creation failed
	{
		LogFail(_T("#### CreateDialog failed - Error: 0x%x ####\r\n"), GetLastError());
		obj.fail();
		goto cleanup;
	}
	else
	{
		LogVerbose(_T("CreateDialog successful\r\n"));
	}

	SetForegroundWindow(hDlgMain);
	ShowWindow(hDlgMain, SW_SHOW);

	LogVerbose(_T("Calling SetCaretBlinkTime...\r\n"));

	if (SetCaretBlinkTime(uBlinkTime))
	{
		LogVerbose(_T("SetCaretBlinkTime successful\r\n"));
		LogVerbose(_T("Calling GetCaretBlinkTime...\r\n"));

		UINT uTime = GetCaretBlinkTime();

		if (uTime != uBlinkTime)
		{
			LogWarn1(_T("Blink Time:: Set: %d, Get: %d\r\n"),
				uBlinkTime, uTime);
			obj.warning1();
		}
		else
		{
			LogVerbose(_T("Blink times matched\r\n"));
		}
	}
	else
	{
		LogWarn2(_T("SetCaretBlinkTime failed - Error: 0x%x\r\n"), GetLastError());
		obj.warning2();
	}

	MessagePump();

	// this is where the operations specific to the visual structure of the dialog starts

	// route operation to dlg specific functions

	if (!_tcsicmp(lpszDlgName, _T("DIALOG_APP0")))
	{
		obj.test(Test_DIALOG_APP0(hDlgMain));
	}
	else if (!_tcsicmp(lpszDlgName, _T("DIALOG_APP1")))
	{
		obj.test(Test_DIALOG_APP1(hDlgMain));
	}

cleanup:

	if (hDlgMain)
	{
		LogVerbose(_T("Calling DestroyWindow...\r\n"));

		if (!DestroyWindow(hDlgMain))
		{
			LogFail(_T("#### DestoryWindow failed - Error: 0x%x ####\r\n"), GetLastError());
			obj.fail();
		}
	}

	return obj.status();
}


/*

@func:	Test_DIALOG_APP0, test function for dialog named DIALOG_APP0 (resource)

@rdesc:	stress return status

@param:	[in] HWND hDlg: handle to the already created dialog box

@fault:	none

@pre:	none

@post:	none

@note:	the caller is responsible to creating and destroying dialogs

*/

DWORD Test_DIALOG_APP0(HWND hDlg)
{
	_stressrun_t obj(L"Test_DIALOG_APP0");

	// dialog DIALOG_APP0 has only a multiline edit control and a listbox

	TCHAR szDebug[MAX_PATH];
	LONG lReturn = 0;
	HWND hWndEdit = NULL;
	HWND hWndList = NULL;
	HANDLE hThreadKey = NULL;
	DWORD dwID = 0;
	DLG_INFO di;
	MSG msg;
	INT i = 0;

	// initialize listbox (IDC_LIST1)

	LogVerbose(_T("Calling GetDlgItem(IDC_LIST1)...\r\n"));

	hWndList = GetDlgItem(hDlg, IDC_LIST1);

	if (!hWndList)
	{
		LogFail(_T("#### GetDlgItem(IDC_LIST1) failed - Error: 0x%x ####\r\n"), GetLastError());
		obj.fail();
		goto cleanup;
	}

	LogComment(_T("hWndList = 0x%x, hDlg=0x%x\r\n"), hWndList, hDlg);

	LogVerbose(_T("Initializing IDC_LIST1...\r\n"));

	for (i = 0; i < LISTBOX_TOTAL_STRINGS; i++) // initialize listbox
	{
		memset(szDebug, 0, sizeof szDebug);
		_stprintf(szDebug, _T("%d: %s"), i, g_rgszStr[rand() % ARRAY_SIZE(g_rgszStr)]);
		lReturn = SendMessage(hWndList, LB_ADDSTRING, 0,  (LPARAM)szDebug);

		if (LB_ERR == lReturn)
		{
			LogFail(_T("#### LB_ADDSTRING returned LB_ERR - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto cleanup;
		}
		else if (LB_ERRSPACE == lReturn)
		{
			LogFail(_T("#### LB_ADDSTRING returned LB_ERRSPACE - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto cleanup;
		}
	}

	DLGITEM_ACTION_WAIT;
	MessagePump();

	LogVerbose(_T("IDC_LIST1 Initialized\r\n"));

	// initialize multiline edit control (IDC_EDIT1)

	LogVerbose(_T("Initializing IDC_EDIT1...\r\n"));

	LogVerbose(_T("Calling GetDlgItem(IDC_EDIT1)...\r\n"));

	hWndEdit = GetDlgItem(hDlg, IDC_EDIT1);

	if (!hWndEdit)
	{
		LogFail(_T("#### GetDlgItem(IDC_EDIT1) failed - Error: 0x%x ####\r\n"), GetLastError());
		obj.fail();
		goto cleanup;
	}

	LogComment(_T("hWndEdit = 0x%x, hDlg=0x%x\r\n"), hWndEdit, hDlg);

	memset(szDebug, 0, sizeof szDebug);
	_tcscat(szDebug, g_rgszStr[rand() % ARRAY_SIZE(g_rgszStr)]);
	_tcscat(szDebug, _T("\r\n"));
	_tcscat(szDebug, g_rgszStr[rand() % ARRAY_SIZE(g_rgszStr)]);
	_tcscat(szDebug, _T("\r\n"));
	_tcscat(szDebug, g_rgszStr[rand() % ARRAY_SIZE(g_rgszStr)]);

	LogVerbose(_T("Calling SetWindowText...\r\n"));
	LogVerbose(_T("------------------------------------------\r\n"));
	LogVerbose(_T("%s\r\n"), szDebug);
	LogVerbose(_T("------------------------------------------\r\n"));

	if (!SetWindowText(hWndEdit, szDebug))
	{
		LogFail(_T("#### SetWindowText failed - Error: 0x%x ####\r\n"),
			GetLastError());
		obj.fail();
		goto cleanup;
	}

	DLGITEM_ACTION_WAIT;
	MessagePump();

	LogVerbose(_T("IDC_EDIT1 Initialized\r\n"));

	if (RANDOM_BOOL(TRUE, FALSE))
	{
		// fill DLG_ITEM structure with dialog control hwnds

		LogVerbose(_T("Filling DLG_ITEM structure...\r\n"));

		memset(&di, 0, sizeof di);

		di.hwndDlg = hDlg;

		di.rghwndCtrls[di.nCtrlCount++] = hWndEdit;
		di.rghwndCtrls[di.nCtrlCount++] = hWndList;
		di.rghwndCtrls[di.nCtrlCount++] = GetDlgItem(hDlg, IDOK);
		di.rghwndCtrls[di.nCtrlCount++] = GetDlgItem(hDlg, IDCANCEL);

		// spawn key generator thread

		LogComment(_T("Spawning key generator thread...\r\n"));

		hThreadKey = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadKeyProc,
			(LPVOID)&di, 0, &dwID);

		if (!hThreadKey)
		{
	   		LogFail(_T("#### CreateThread failed - Error:0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto cleanup;
		}

		LogVerbose(_T("CreateThread successful\r\n"));

		// message pump

		LogVerbose(_T("Enter message pump...\r\n"));

		while (true)
		{
			if (di.fExitingThread)
			{
				break;
			}

			memset(&msg, 0, sizeof msg);
			if (PeekMessage(&msg, NULL, 0, 0,  PM_REMOVE))
			{
				if (IsWindow(hDlg))
				{
					if (!IsDialogMessage(hDlg, &msg))
					 {
						 TranslateMessage(&msg);
						 DispatchMessage(&msg);
					 }
				}
			}
			else
			{
				THREADPOLL_WAIT;
			}
		}

		LogVerbose(_T("Exited message pump\r\n"));

		// wait for the thread to complete

		WaitForSingleObject(hThreadKey, THREAD_COMPLETION_DELAY);

		LogVerbose(_T("Closing key generator thread handle...\r\n"));

		if (!CloseHandle(hThreadKey))
		{
	   		LogFail(_T("#### CloseHandle failed - Error:0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto cleanup;
		}
	}

	if (RANDOM_BOOL(TRUE, FALSE))
	{
		LogComment(_T("Testing multiple selection list box...\r\n"));

		// test listbox

		obj.test(Test_ListBox_MultiSel(hWndList));
	}

	if (RANDOM_BOOL(TRUE, FALSE))
	{
		LogComment(_T("Testing multi-line edit control...\r\n"));

		// test edit control

		obj.test(Test_EditBox_MultiLine(hWndEdit));
	}

cleanup:
	return obj.status();
}


/*

@func:	DlgProc_DIALOG_APP0, dialog procedure for for dialog named DIALOG_APP0
(resource)

@rdesc:	true if it processed the message, false otherwise

@param:	default

@fault:	none

@pre:	none

@post:	none

@note:	global object g_objDlgProc_DIALOG_APP0 holds correcponding stress status
information

*/

BOOL CALLBACK DlgProc_DIALOG_APP0(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
		return FALSE;   // let dlgmgr process

	case WM_CLOSE:
		LogVerbose(_T("DlgProc_DIALOG_APP0: Receive WM_CLOSE\r\n"));

		LogVerbose(_T("DlgProc_DIALOG_APP0: Calling EndDialog...\r\n"));

		if (!EndDialog(hwndDlg, TRUE))
		{
			LogFail(_T("#### DlgProc_DIALOG_APP0: EndDialog failed - Error: 0x%x ####\r\n"),
				GetLastError());
			g_objDlgProc_DIALOG_APP0.fail();
		}
		else
		{
			LogVerbose(_T("DlgProc_DIALOG_APP0: EndDialog successful\r\n"));
		}

		PostQuitMessage(0);
		return TRUE; // message processed

		default:
			break;
	}

	return FALSE;
}


/*

@func:	Test_ListBox_MultiSel, tests a multi-select listbox

@rdesc:	stress return status

@param:	[in] HWND hWndList: handle to the listbox

@fault:	none

@pre:	none

@post:	none

@note:	assumes listbox is already initialized

*/

DWORD Test_ListBox_MultiSel(HWND hWndList)
{
	_stressrun_t obj(L"Test_ListBox_MultiSel");

	LONG lReturn = 0;
	INT nItemCount = 0;
	bool rgfSelSet[LISTBOX_TOTAL_STRINGS];
	INT nItem = 0;
	INT i = 0;
	INT nMaxLoopCount = 0;
	INT rgnSelGet[LISTBOX_TOTAL_STRINGS];
	INT nSelGetCount = 0;

	//__________________________________________________________________________
	// multiple items selection

	// reset listbox selection

	SendMessage(hWndList, LB_SETSEL, (WPARAM)(BOOL)false, (LPARAM)-1);

	// get item count

	lReturn = SendMessage(hWndList, LB_GETCOUNT, 0,  0);

	if (LB_ERR == lReturn)
	{
		LogFail(_T("#### LB_GETCOUNT returned LB_ERR - Error: 0x%x ####\r\n"),
			GetLastError());
		obj.fail();
		goto cleanup;
	}

	nItemCount = lReturn;

	memset(rgfSelSet, false, (sizeof rgfSelSet) / (sizeof bool));

	// select multiple items

	nMaxLoopCount = RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN);

	INT nSelectedItemCount;
	nSelectedItemCount = 0;

	for (i = 0; i < nMaxLoopCount; i++)
	{
		if (!nItemCount)
		{
			LogWarn2(_T("No items in the list box\r\n"));
			obj.warning2();
			goto cleanup;
		}

		nItem = rand() % nItemCount;

		if (!rgfSelSet[nItem])	// if not already selected
		{
			lReturn = SendMessage(hWndList, LB_SETSEL, (WPARAM)(BOOL)true, (LPARAM)nItem);

			if (LB_ERR == lReturn)
			{
				LogFail(_T("#### LB_SETSEL returned LB_ERR - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.fail();
				goto cleanup;
			}
			else
			{
				rgfSelSet[nItem] = true;
				nSelectedItemCount++;
				LogVerbose(_T("LB_SETSEL selected item #%i (Total: %d)\r\n"), nItem, nSelectedItemCount);
			}
		}

		MessagePump();
		DLGITEM_ACTION_WAIT;
	}

	// validate selection

	memset(rgnSelGet, 0, sizeof rgnSelGet);
	lReturn = SendMessage(hWndList, LB_GETSELITEMS, (WPARAM)LISTBOX_TOTAL_STRINGS, (LPARAM)rgnSelGet);

	if (LB_ERR == lReturn)
	{
		LogFail(_T("#### LB_GETSELITEMS returned LB_ERR - Error: 0x%x ####\r\n"),
			GetLastError());
		obj.fail();
		goto cleanup;
	}

	nSelGetCount = lReturn;

	if (nSelectedItemCount != nSelGetCount)
	{
		LogWarn1(_T("#### Mismatched selection count: Expected: %d, Got %d ####\r\n"),
			nSelectedItemCount, nSelGetCount);
		obj.warning1();
	}
	else
	{
		if (nSelGetCount)
		{
			for (i = 0; i < nSelGetCount; i++)
			{
				if (!rgfSelSet[rgnSelGet[i]])
				{
					LogWarn1(_T("#### Item #%i not selected: Multiline listbox returned mismatched selections ####\r\n"), i);
					obj.warning1();
				}
				else
				{
					rgfSelSet[rgnSelGet[i]] = false;
				}
			}
		}

		// make sure the test didn't select any other item(s)

		for (i = 0; i < LISTBOX_TOTAL_STRINGS; i++)
		{
			if (rgfSelSet[i])
			{
				LogWarn1(_T("#### Multiline listbox didn't register selection ####\r\n"));
				obj.warning1();
			}
		}
	}

	//__________________________________________________________________________
	// delete item in random

	nMaxLoopCount = RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN);

	for (i = 0; i < nMaxLoopCount; i++)
	{
		// get item count

		lReturn = SendMessage(hWndList, LB_GETCOUNT, 0,  0);

		if (LB_ERR == lReturn)
		{
			LogFail(_T("#### LB_GETCOUNT returned LB_ERR - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto cleanup;
		}

		if (!lReturn)
		{
			LogWarn2(_T("No items in the list box\r\n"));
			obj.warning2();
			goto cleanup;
		}

		nItem = rand() % lReturn;

		lReturn = SendMessage(hWndList, LB_DELETESTRING, (WPARAM)nItem, 0);

		if (LB_ERR == lReturn)
		{
			LogFail(_T("#### LB_DELETESTRING returned LB_ERR - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto cleanup;
		}

		DLGITEM_ACTION_WAIT;
		MessagePump();
	}

cleanup:
	return obj.status();
}


/*

@func:	Test_EditBox_MultiLine, tests a multi-select edit control

@rdesc:	stress return status

@param:	[in] HWND hWndEdit: handle to the edit control

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

DWORD Test_EditBox_MultiLine(HWND hWndEdit)
{
	_stressrun_t obj(L"Test_EditBox_MultiLine");

	LONG lReturn = 0;
	const INT BUF_SIZE = 0x400;
	TCHAR szText[BUF_SIZE];
	INT nLineCount = 0;
	INT i = 0;
   	INT nCharCount = 0;
	INT nEndSet = 0;
	INT nStartSet = 0;
	INT nEndGet = 0;
	INT nStartGet = 0;
	bool fCopyPaste = true;

	//__________________________________________________________________________
	// initialize edit control with own text

	nLineCount = RANDOM_INT(EDITBOX_MAX_LINES, EDITBOX_MIN_LINES);

	memset(szText, 0, sizeof szText);

	for (i = 0; i < nLineCount; i++)
	{
		_tcscat(szText, g_rgszStr[rand() % ARRAY_SIZE(g_rgszStr)]);

		if (i < (nLineCount - 1)) // ignore newline at the last line
		{
			_tcscat(szText, _T("\r\n"));
		}
	}

	LogVerbose(_T("Calling SetWindowText...\r\n"));
	LogVerbose(_T("------------------------------------------\r\n"));
	LogVerbose(_T("%s\r\n"), szText);
	LogVerbose(_T("------------------------------------------\r\n"));

	if (!SetWindowText(hWndEdit, szText))
	{
		LogFail(_T("#### SetWindowText failed - Error: 0x%x ####\r\n"),
			GetLastError());
		obj.fail();
		goto cleanup;
	}

	DLGITEM_ACTION_WAIT;

	//__________________________________________________________________________
	// select random text from the edit control

	// first get count of chars in the edit control

   	lReturn = SendMessage(hWndEdit, WM_GETTEXTLENGTH, 0, 0);

   	nCharCount = lReturn;

	if (!lReturn)
	{
		LogWarn2(_T("WM_GETTEXTLENGTH returned '0'\r\n"));
		obj.warning2();
		goto cleanup;
	}

	nEndSet = rand() % lReturn;

	if (!nEndSet)
	{
		nEndSet++;
	}

	nStartSet = rand() % nEndSet;

	lReturn = SendMessage(hWndEdit, EM_SETSEL, (WPARAM)nStartSet, (LPARAM)nEndSet);

	if (FALSE == lReturn)
	{
		LogWarn2(_T("EM_SETSEL returned FALSE - Error: 0x%x\r\n"),
			GetLastError());
		obj.warning2();
	}

	DLGITEM_ACTION_WAIT;
	MessagePump();

	lReturn = SendMessage(hWndEdit, EM_GETSEL, (WPARAM)&nStartGet, (LPARAM)&nEndGet);

	if (-1 == lReturn)
	{
		LogWarn1(_T("EM_GETSEL returned -1 - Error: 0x%x\r\n"),
			GetLastError());
		obj.warning1();
	}

	if (nStartSet < nEndSet) // normal case
	{
		if (nStartGet != nStartSet || nEndGet != nEndSet)
		{
			LogWarn1(_T("#### (start < end): Expected: (%d, %d), Got: (%d, %d) ####\r\n"),
				nStartSet, nEndSet, nStartGet, nEndGet);
			obj.warning1();
			goto cleanup;
		}
	}
	else // reverse
	{
		if (nStartGet != nEndSet || nEndGet != nStartSet)
		{
			LogWarn1(_T("#### (start >= end): Expected: (%d, %d), Got: (%d, %d) ####\r\n"),
				nEndSet, nStartSet, nStartGet, nEndGet);
			obj.warning1();
			goto cleanup;
		}
	}

	DLGITEM_ACTION_WAIT;
	MessagePump();

	fCopyPaste = (rand() % 2) ? true : false;

	if (fCopyPaste)
	{
		//__________________________________________________________________________
		// copy

		LogVerbose(_T("Sending WM_COPY...\r\n"));

		lReturn = SendMessage(hWndEdit, WM_COPY, 0, 0);
		MessagePump();
	}
	else
	{
		//__________________________________________________________________________
		// cut

		LogVerbose(_T("Sending WM_CUT...\r\n"));

		lReturn = SendMessage(hWndEdit, WM_CUT, 0x00, 0x00);
		MessagePump();
	}

	DLGITEM_ACTION_WAIT;
	MessagePump();

	//__________________________________________________________________________
	// paste

	LogVerbose(_T("Sending WM_PASTE...\r\n"));

	lReturn = SendMessage(hWndEdit, WM_PASTE, 0x00, 0x00);

	DLGITEM_ACTION_WAIT;
	MessagePump();

	if (fCopyPaste) // verify char count
	{
	   	lReturn = SendMessage(hWndEdit, WM_GETTEXTLENGTH, 0, 0);

	   	if (nCharCount != lReturn)
	   	{
			LogWarn2(_T("#### Character count changed after WM_CUT, WM_PASTE operations ####\r\n"));
			LogWarn2(_T("#### Expected: %d, Got: %d ####\r\n"), nCharCount, lReturn);
			obj.warning2();
			goto cleanup;
	   	}
	}

cleanup:
	return obj.status();
}


/*

@func:	Test_DIALOG_APP1, test function for dialog named DIALOG_APP1 (resource)

@rdesc:	stress return status

@param:	[in] HWND hDlg: handle to the already created dialog box

@fault:	none

@pre:	none

@post:	none

@note:	the caller is responsible to creating and destroying dialogs

*/

DWORD Test_DIALOG_APP1(HWND hDlg)
{
	_stressrun_t obj(L"Test_DIALOG_APP1");

	/*
	dialog DIALOG_APP1 has following controls:

		- checkbox
		- combobox
		- command button
		- single select listbox
		- radio button
		- scrollbar
		- static
		- groupbox
	*/

	TCHAR szDebug[MAX_PATH];
	LONG lReturn = 0;
	HWND hWndEdit = NULL;
	HWND hWndList = NULL;
	HWND hWndCombo = NULL;
	HWND hWndStaticBitmap = NULL;
	HWND hWndStaticIcon = NULL;
	HANDLE hThreadKey = NULL;
	DWORD dwID = 0;
	DLG_INFO di;
	MSG msg;
	INT i = 0;

	// initialize listbox (IDC_LIST1)

	LogVerbose(_T("Calling GetDlgItem(IDC_LIST1)...\r\n"));

	hWndList = GetDlgItem(hDlg, IDC_LIST1);

	if (!hWndList)
	{
		LogFail(_T("#### GetDlgItem(IDC_LIST1) failed - Error: 0x%x ####\r\n"), GetLastError());
		obj.fail();
		goto cleanup;
	}

	LogComment(_T("hWndList = 0x%x, hDlg=0x%x\r\n"), hWndList, hDlg);

	LogVerbose(_T("Initializing IDC_LIST1...\r\n"));

	for (i = 0; i < LISTBOX_TOTAL_STRINGS; i++) // initialize listbox
	{
		memset(szDebug, 0, sizeof szDebug);
		_stprintf(szDebug, _T("%d: %s"), i, g_rgszStr[rand() % ARRAY_SIZE(g_rgszStr)]);
		lReturn = SendMessage(hWndList, LB_ADDSTRING, 0,  (LPARAM)szDebug);

		if (LB_ERR == lReturn)
		{
			LogFail(_T("#### LB_ADDSTRING returned LB_ERR - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto cleanup;
		}
		else if (LB_ERRSPACE == lReturn)
		{
			LogFail(_T("#### LB_ADDSTRING returned LB_ERRSPACE - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto cleanup;
		}
	}

	DLGITEM_ACTION_WAIT;
	MessagePump();

	LogVerbose(_T("IDC_LIST1 Initialized\r\n"));

	// initialize combo box

	LogVerbose(_T("Initializing IDC_COMBO1...\r\n"));

	LogVerbose(_T("Calling GetDlgItem(IDC_COMBO1)...\r\n"));

	hWndCombo = GetDlgItem(hDlg, IDC_COMBO1);

	if (!hWndCombo)
	{
		LogFail(_T("#### GetDlgItem(IDC_COMBO1) failed - Error: 0x%x ####\r\n"), GetLastError());
		obj.fail();
		goto cleanup;
	}

	LogComment(_T("hWndCombo = 0x%x, hDlg=0x%x\r\n"), hWndCombo, hDlg);

	for (i = 0; i < COMBOBOX_TOTAL_STRINGS; i++) // initialize listbox
	{
		memset(szDebug, 0, sizeof szDebug);
		_stprintf(szDebug, _T("%d: %s"), i, g_rgszStr[rand() % ARRAY_SIZE(g_rgszStr)]);
		lReturn = SendMessage(hWndCombo, CB_ADDSTRING, 0,  (LPARAM)szDebug);

		if (CB_ERR == lReturn)
		{
			LogFail(_T("#### CB_ADDSTRING returned CB_ERR - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto cleanup;
		}
		else if (CB_ERRSPACE == lReturn)
		{
			LogFail(_T("#### CB_ADDSTRING returned CB_ERRSPACE - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto cleanup;
		}
	}

	DLGITEM_ACTION_WAIT;
	MessagePump();

	LogVerbose(_T("IDC_COMBO1 Initialized\r\n"));

	// initialize multiline edit control (IDC_EDIT1)

	LogVerbose(_T("Initializing IDC_EDIT1...\r\n"));

	LogVerbose(_T("Calling GetDlgItem(IDC_EDIT1)...\r\n"));

	hWndEdit = GetDlgItem(hDlg, IDC_EDIT1);

	if (!hWndEdit)
	{
		LogFail(_T("#### GetDlgItem(IDC_EDIT1) failed - Error: 0x%x ####\r\n"), GetLastError());
		obj.fail();
		goto cleanup;
	}

	LogComment(_T("hWndEdit = 0x%x, hDlg=0x%x\r\n"), hWndEdit, hDlg);

	memset(szDebug, 0, sizeof szDebug);
	_tcscat(szDebug, g_rgszStr[rand() % ARRAY_SIZE(g_rgszStr)]);
	_tcscat(szDebug, _T("\r\n"));
	_tcscat(szDebug, g_rgszStr[rand() % ARRAY_SIZE(g_rgszStr)]);
	_tcscat(szDebug, _T("\r\n"));
	_tcscat(szDebug, g_rgszStr[rand() % ARRAY_SIZE(g_rgszStr)]);

	LogVerbose(_T("Calling SetWindowText...\r\n"));
	LogVerbose(_T("------------------------------------------\r\n"));
	LogVerbose(_T("%s\r\n"), szDebug);
	LogVerbose(_T("------------------------------------------\r\n"));

	if (!SetWindowText(hWndEdit, szDebug))
	{
		LogFail(_T("#### SetWindowText failed - Error: 0x%x ####\r\n"),
			GetLastError());
		obj.fail();
		goto cleanup;
	}

	DLGITEM_ACTION_WAIT;
	MessagePump();

	LogVerbose(_T("IDC_EDIT11 Initialized\r\n"));

	// retrieve static control handle

	LogVerbose(_T("Calling GetDlgItem(IDC_BMP1)...\r\n"));

	hWndStaticBitmap = GetDlgItem(hDlg, IDC_BMP1);

	if (!hWndStaticBitmap)
	{
		LogFail(_T("#### GetDlgItem(IDC_BMP1) failed - Error: 0x%x ####\r\n"), GetLastError());
		obj.fail();
		goto cleanup;
	}

	LogComment(_T("hWndStaticBitmap = 0x%x, hDlg=0x%x\r\n"), hWndStaticBitmap, hDlg);

	LogVerbose(_T("Calling GetDlgItem(IDC_ICON1)...\r\n"));

	hWndStaticIcon = GetDlgItem(hDlg, IDC_ICON1);

	if (!hWndStaticIcon)
	{
		LogFail(_T("#### GetDlgItem(IDC_ICON1) failed - Error: 0x%x ####\r\n"), GetLastError());
		obj.fail();
		goto cleanup;
	}

	LogComment(_T("hWndStaticIcon = 0x%x, hDlg=0x%x\r\n"), hWndStaticIcon, hDlg);

	if (RANDOM_BOOL(TRUE, FALSE))
	{
		// fill DLG_ITEM structure with dialog control hwnds

		LogVerbose(_T("Filling DLG_ITEM structure...\r\n"));

		memset(&di, 0, sizeof di);

		di.hwndDlg = hDlg;

		di.rghwndCtrls[di.nCtrlCount++] = hWndEdit;
		di.rghwndCtrls[di.nCtrlCount++] = hWndList;
		di.rghwndCtrls[di.nCtrlCount++] = hWndCombo;
		di.rghwndCtrls[di.nCtrlCount++] = hWndStaticBitmap;
		di.rghwndCtrls[di.nCtrlCount++] = hWndStaticIcon;
		di.rghwndCtrls[di.nCtrlCount++] = GetDlgItem(hDlg, IDC_CHECK1);
		di.rghwndCtrls[di.nCtrlCount++] = GetDlgItem(hDlg, IDC_RADIO1);
		di.rghwndCtrls[di.nCtrlCount++] = GetDlgItem(hDlg, IDC_RADIO2);
		di.rghwndCtrls[di.nCtrlCount++] = GetDlgItem(hDlg, IDC_RADIO3);
		di.rghwndCtrls[di.nCtrlCount++] = GetDlgItem(hDlg, IDC_SCROLLBAR1);
		di.rghwndCtrls[di.nCtrlCount++] = GetDlgItem(hDlg, IDC_SCROLLBAR2);
		di.rghwndCtrls[di.nCtrlCount++] = GetDlgItem(hDlg, IDOK);
		di.rghwndCtrls[di.nCtrlCount++] = GetDlgItem(hDlg, IDCANCEL);

		// spawn key generator thread

		LogComment(_T("Spawning key generator thread...\r\n"));

		hThreadKey = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadKeyProc,
			(LPVOID)&di, 0, &dwID);

		if (!hThreadKey)
		{
	   		LogFail(_T("#### CreateThread failed - Error:0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto cleanup;
		}

		LogVerbose(_T("CreateThread successful\r\n"));

		// message pump

		LogVerbose(_T("Enter message pump...\r\n"));

		while (true)
		{
			if (di.fExitingThread)
			{
				break;
			}

			memset(&msg, 0, sizeof msg);
			if (PeekMessage(&msg, NULL, 0, 0,  PM_REMOVE))
			{
				if (IsWindow(hDlg))
				{
					if (!IsDialogMessage(hDlg, &msg))
					 {
						 TranslateMessage(&msg);
						 DispatchMessage(&msg);
					 }
				}
			}
			else
			{
				THREADPOLL_WAIT;
			}
		}

		LogVerbose(_T("Exited message pump\r\n"));

		// wait for the thread to complete

		WaitForSingleObject(hThreadKey, THREAD_COMPLETION_DELAY);

		LogVerbose(_T("Closing key generator thread handle...\r\n"));

		if (!CloseHandle(hThreadKey))
		{
	   		LogFail(_T("#### CloseHandle failed - Error:0x%x ####\r\n"),
				GetLastError());
			obj.fail();
			goto cleanup;
		}
	}

	if (RANDOM_BOOL(TRUE, FALSE))
	{
		LogComment(_T("Testing single selection list box...\r\n"));

		// test listbox

		obj.test(Test_ListBox_SingleSel(hWndList));
	}

	if (RANDOM_BOOL(TRUE, FALSE))
	{
		LogComment(_T("Testing multi-line edit control...\r\n"));

		// test edit control

		obj.test(Test_EditBox_MultiLine(hWndEdit));
	}

	if (RANDOM_BOOL(TRUE, FALSE))
	{
		LogComment(_T("Testing combo box...\r\n"));

		// test combobox

		obj.test(Test_ComboBox(hWndCombo));
	}

	if (RANDOM_BOOL(TRUE, FALSE))
	{
		LogComment(_T("Testing static controls...\r\n"));

		// test static control

		obj.test(Test_Static(hWndStaticBitmap, hWndStaticIcon));
	}

cleanup:
	return obj.status();
}


/*

@func:	DlgProc_DIALOG_APP1, dialog procedure for for dialog named DIALOG_APP1
(resource)

@rdesc:	true if it processed the message, false otherwise

@param:	default

@fault:	none

@pre:	none

@post:	none

@note:	global object g_objDlgProc_DIALOG_APP1 holds correcponding stress status
information

*/

BOOL CALLBACK DlgProc_DIALOG_APP1(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	SCROLLINFO si;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		PostMessage(hwndDlg, QA_INIT_DLG, 0, 0);
		return FALSE;   // let dlgmgr process

	case QA_INIT_DLG: // after initdialog

		memset(&si, 0, sizeof si);

		si.cbSize = sizeof si;
		si.fMask = SIF_PAGE | SIF_RANGE;
		si.nMin = SCROLLBAR_RANGE_MIN;
		si.nMax = SCROLLBAR_RANGE_MAX;
		si.nPage = SCROLLBAR_SCROLLPAGE;

		// vertical

		SetScrollInfo(
			GetDlgItem(hwndDlg, IDC_SCROLLBAR1),
			SB_CTL,
			&si,
			TRUE
		);

		// horizontal

		SetScrollInfo(
			GetDlgItem(hwndDlg, IDC_SCROLLBAR2),
			SB_CTL,
			&si,
			TRUE
		);

		return TRUE;

	case WM_CLOSE:
		LogVerbose(_T("DlgProc_DIALOG_APP0: Receive WM_CLOSE\r\n"));

		LogVerbose(_T("DlgProc_DIALOG_APP0: Calling EndDialog...\r\n"));

		if (!EndDialog(hwndDlg, TRUE))
		{
			LogFail(_T("#### DlgProc_DIALOG_APP0: EndDialog failed - Error: 0x%x ####\r\n"),
				GetLastError());
			g_objDlgProc_DIALOG_APP1.fail();
		}
		else
		{
			LogVerbose(_T("DlgProc_DIALOG_APP0: EndDialog successful\r\n"));
		}

		PostQuitMessage(0);
		return TRUE; // message processed

	case WM_COMMAND:
		LogVerbose(_T("DlgProc_DIALOG_APP1: WM_COMMAND: hwnd=%08X msg=%08x wP=%08X lP=%08X\r\n"),
			hwndDlg, uMsg, wParam, lParam );

		switch (wParam)
		{
			default:
				break;

	         case IDC_RADIO1:
	         case IDC_RADIO2:
	         case IDC_RADIO3:
	            if (!CheckRadioButton(hwndDlg, IDC_RADIO1, IDC_RADIO3, wParam))
	            {
					LogFail(_T("#### DlgProc_DIALOG_APP0: CheckRadioButton failed - Error: 0x%x ####\r\n"),
						GetLastError());
	            	g_objDlgProc_DIALOG_APP1.fail();
	            }
				break;

			case IDOK:
			case IDCANCEL:
				break; // dlgmgr
		}
		break;

	case WM_VSCROLL:
	case WM_HSCROLL:

		memset(&si, 0, sizeof si);

		si.cbSize = sizeof si;
		si.fMask = SIF_ALL;

		if (!GetScrollInfo((HWND)lParam, SB_CTL, &si))
		{
			LogFail(_T("#### DlgProc_DIALOG_APP1: GetScrollInfo failed - Error: 0x%x ####\r\n"),
				GetLastError());
			g_objDlgProc_DIALOG_APP1.fail();
			break;
		}

		switch(LOWORD(wParam))
		{
			case SB_BOTTOM: // horiz: SB_RIGHT
				si.nPos = SCROLLBAR_RANGE_MAX;
				break;
			case SB_ENDSCROLL:
				break;
			case SB_LINEDOWN: // horiz: SB_LINERIGHT
				si.nPos = min((si.nPos + SCROLLBAR_SCROLLSTEP), SCROLLBAR_RANGE_MAX);
				break;
			case SB_LINEUP: // horiz: SB_LINELEFT
				si.nPos = max((si.nPos - SCROLLBAR_SCROLLSTEP), SCROLLBAR_RANGE_MIN);
				break;
			case SB_PAGEDOWN: // horiz: SB_PAGERIGHT
				si.nPos = min((si.nPos + SCROLLBAR_SCROLLPAGE), SCROLLBAR_RANGE_MAX);
				break;
			case SB_PAGEUP: // horiz: SB_PAGELEFT
				si.nPos = max((si.nPos - SCROLLBAR_SCROLLPAGE), SCROLLBAR_RANGE_MIN);
				break;
			case SB_THUMBPOSITION:
			case SB_THUMBTRACK:
				si.nPos = HIWORD(wParam);
				break;
			case SB_TOP: // horiz: SB_LEFT
				si.nPos = SCROLLBAR_RANGE_MIN;
				break;
		}
		SetScrollPos((HWND)lParam, SB_CTL, si.nPos, TRUE);
		break;

		default:
			break;
	}

	return FALSE;
}


/*

@func:	Test_ComboOrSingleList, tests either a combobox or a single selection
listbox

@rdesc:	stress return status

@param:	[in] HWND hWnd: handle to the control

@param:	[] bool fComboBox: true value runs tests for combobox, false value runs
tests for single selection listbox

@fault:	none

@pre:	none

@post:	none

@note:	only used by Test_ComboBox and Test_ListBox_SingleSel

*/

static DWORD Test_ComboOrSingleList(HWND hWnd, bool fComboBox)
{
	_stressrun_t obj;

	LONG lReturn = 0;
	INT nItemCount = 0;
	INT nItem = 0;
	INT i = 0;
	INT nMaxLoopCount = 0;

	TCHAR szMsg[MAX_PATH];
	UINT uMsg;
	LONG lErrorCode;
	TCHAR szErrorCode[MAX_PATH];

	memset(szMsg, 0, sizeof szMsg);
	memset(szErrorCode, 0, sizeof szErrorCode);

	//__________________________________________________________________________
	// items selection

	// get item count

	if (fComboBox) // combo box
	{
		uMsg = CB_GETCOUNT;
		_tcscpy(szMsg, _T("CB_GETCOUNT"));

		lErrorCode = CB_ERR;
		_tcscpy(szErrorCode, _T("CB_ERR"));
	}
	else // list box
	{
		uMsg = LB_GETCOUNT;
		_tcscpy(szMsg, _T("LB_GETCOUNT"));

		lErrorCode = LB_ERR;
		_tcscpy(szErrorCode, _T("LB_ERR"));
	}

	lReturn = SendMessage(hWnd, uMsg, 0,  0);

	if (lErrorCode == lReturn)
	{
		LogFail(_T("#### %s returned %s - Error: 0x%x ####\r\n"),
			szMsg, szErrorCode, GetLastError());
		obj.fail();
		goto cleanup;
	}

	nItemCount = lReturn;

	// select and validate different items in a loop

	nMaxLoopCount = RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN);

	for (i = 0; i < nMaxLoopCount; i++)
	{
		// select a random item

		if (!nItemCount)
		{
			LogWarn2(_T("No items in the list/combo box\r\n"));
			obj.warning2();
			goto cleanup;
		}

		nItem = rand() % nItemCount;

		if (fComboBox) // combo box
		{
			uMsg = CB_SETCURSEL;
			_tcscpy(szMsg, _T("CB_SETCURSEL"));

			lErrorCode = CB_ERR;
			_tcscpy(szErrorCode, _T("CB_ERR"));
		}
		else // list box
		{
			uMsg = LB_SETCURSEL;
			_tcscpy(szMsg, _T("LB_SETCURSEL"));

			lErrorCode = LB_ERR;
			_tcscpy(szErrorCode, _T("LB_ERR"));
		}

		lReturn = SendMessage(hWnd, uMsg, (WPARAM)nItem, (LPARAM)0);

		if (lErrorCode == lReturn)
		{
			LogFail(_T("#### %s returned %s - Error: 0x%x ####\r\n"),
				szMsg, szErrorCode, GetLastError());
			obj.fail();
			goto cleanup;
		}

		DLGITEM_ACTION_WAIT;
		MessagePump();

		// validate the selection

		if (fComboBox) // combo box
		{
			uMsg = CB_GETCURSEL;
			_tcscpy(szMsg, _T("CB_GETCURSEL"));

			lErrorCode = CB_ERR;
			_tcscpy(szErrorCode, _T("CB_ERR"));
		}
		else // list box
		{
			uMsg = LB_GETCURSEL;
			_tcscpy(szMsg, _T("LB_GETCURSEL"));

			lErrorCode = LB_ERR;
			_tcscpy(szErrorCode, _T("LB_ERR"));
		}


		lReturn = SendMessage(hWnd, uMsg, (WPARAM)0, (LPARAM)0);

		if (lErrorCode == lReturn)
		{
			LogFail(_T("#### %s returned %s - Error: 0x%x ####\r\n"),
				szMsg, szErrorCode, GetLastError());
			obj.fail();
			goto cleanup;
		}
		else
		{
			if (lReturn != nItem)
			{
				LogWarn2(_T("#### Mismatched XB_SETCURSEL/XB_GETCURSEL - Expected: %d, Got: %d ####\r\n"),
					nItem, lReturn);
				obj.warning2();
				goto cleanup;
			}
			else
			{
				LogVerbose(_T("Matched XB_SETCURSEL/XB_GETCURSEL\r\n"));
			}
		}
	}

	//__________________________________________________________________________
	// delete item in random

	nMaxLoopCount = RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN);

	for (i = 0; i < nMaxLoopCount; i++)
	{
		// get item count

		if (fComboBox) // combo box
		{
			uMsg = CB_GETCOUNT;
			_tcscpy(szMsg, _T("CB_GETCOUNT"));

			lErrorCode = CB_ERR;
			_tcscpy(szErrorCode, _T("CB_ERR"));
		}
		else // list box
		{
			uMsg = LB_GETCOUNT;
			_tcscpy(szMsg, _T("LB_GETCOUNT"));

			lErrorCode = LB_ERR;
			_tcscpy(szErrorCode, _T("LB_ERR"));
		}


		lReturn = SendMessage(hWnd, uMsg, 0,  0);

		if (lErrorCode == lReturn)
		{
			LogFail(_T("#### %s returned %s - Error: 0x%x ####\r\n"),
				szMsg, szErrorCode, GetLastError());
			obj.fail();
			goto cleanup;
		}

		if (!lReturn)
		{
			LogWarn2(_T("No items in the list/combo box\r\n"));
			obj.warning2();
			goto cleanup;
		}

		nItem = rand() % lReturn;

		if (fComboBox) // combo box
		{
			uMsg = CB_DELETESTRING;
			_tcscpy(szMsg, _T("CB_DELETESTRING"));

			lErrorCode = CB_ERR;
			_tcscpy(szErrorCode, _T("CB_ERR"));
		}
		else // list box
		{
			uMsg = LB_DELETESTRING;
			_tcscpy(szMsg, _T("LB_DELETESTRING"));

			lErrorCode = LB_ERR;
			_tcscpy(szErrorCode, _T("LB_ERR"));
		}

		lReturn = SendMessage(hWnd, uMsg, (WPARAM)nItem, 0);

		if (lErrorCode == lReturn)
		{
			LogFail(_T("#### CB_DELETESTRING returned CB_ERR - Error: 0x%x ####\r\n"),
				szMsg, szErrorCode, GetLastError());
			obj.fail();
			goto cleanup;
		}

		DLGITEM_ACTION_WAIT;
		MessagePump();
	}

cleanup:
	return obj.status();
}


/*

@func:	Test_ComboBox, tests a combobox

@rdesc:	stress return status

@param:	[in] HWND hWndCombo: handle to the combobox

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

DWORD Test_ComboBox(HWND hWndCombo)
{
	_stressrun_t obj(L"Test_ComboBox");

	obj.test(Test_ComboOrSingleList(hWndCombo, true));

	return obj.status();
}


/*

@func:	Test_ListBox_SingleSel, tests a single selection listbox

@rdesc:	stress return status

@param:	[in] HWND hWndList: handle to the single selection listbox

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

DWORD Test_ListBox_SingleSel(HWND hWndList)
{
	_stressrun_t obj(L"Test_ListBox_SingleSel");

	obj.test(Test_ComboOrSingleList(hWndList, false));

	return obj.status();
}


/*

@func:	Test_Static, tests SS_BITMAP and SS_ICON static controls

@rdesc:	stress return status

@param:	[in] HWND hWndStaticBitmap: handle to the SS_BITMAP static control

@param:	[in] HWND hWndStaticIcon: handle to the SS_ICON static control

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

DWORD Test_Static(HWND hWndStaticBitmap, HWND hWndStaticIcon)
{
	_stressrun_t obj(L"Test_Static");

	const INT BMP_COUNT = BMP_LAST - BMP_FIRST + 1;
	HBITMAP rghBmp[BMP_COUNT];

	const INT ICON_COUNT = ICON_LAST - ICON_FIRST + 1;
	HICON rghIcon[ICON_COUNT];

	INT i = 0;
	LONG lReturn = 0;

	// load bitmaps

	for (i = 0; i < BMP_COUNT; i++)
	{
		rghBmp[i] = LoadBitmap(g_hInstance,  MAKEINTRESOURCE(i + BMP_FIRST));

		if (!rghBmp[i])
		{
			LogFail(_T("#### LoadBitmap(%i) failed - Error: 0x%x ####\r\n"),
				(i + BMP_FIRST), GetLastError());
			obj.fail();
			goto cleanup;
		}
	}

	// load icons

	for (i = 0; i < ICON_COUNT; i++)
	{
		rghIcon[i] = LoadIcon(g_hInstance,  MAKEINTRESOURCE(i + ICON_FIRST));

		if (!rghIcon[i])
		{
			LogFail(_T("#### LoadIcon(%i) failed - Error: 0x%x ####\r\n"),
				(i + ICON_FIRST), GetLastError());
			obj.fail();
			goto cleanup;
		}
	}


	// test with bitmaps

	for (i = 0; i < BMP_COUNT; i++)
	{
		LogVerbose(_T("[%i] Attempting STM_SETIMAGE(IMAGE_BITMAP)...\r\n"), i);

		lReturn = SendMessage(hWndStaticBitmap, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)rghBmp[i]);

		LogVerbose(_T("[%i] Attempting STM_GETIMAGE(IMAGE_BITMAP)...\r\n"), i);

		lReturn = SendMessage(hWndStaticBitmap, STM_GETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)0);

		if (!lReturn || (HBITMAP)lReturn != rghBmp[i])
		{
			LogFail(_T("#### STM_SETIMAGE/STM_GETIMAGE(IMAGE_BITMAP) mismatch - Error: 0x%x ####\r\n"),
				GetLastError());
			LogFail(_T("#### STM_SETIMAGE/STM_GETIMAGE(IMAGE_BITMAP) - Handle Expected: 0x%x, Got: 0x%x ####\r\n"),
				rghBmp[i], lReturn);
			obj.fail();
			goto cleanup;
		}

		UpdateWindow(hWndStaticIcon);
		MessagePump();
		DLGITEM_ACTION_WAIT;
	}

	// test with icons

	for (i = 0; i < ICON_COUNT; i++)
	{
		LogVerbose(_T("[%i] Attempting STM_SETIMAGE(IMAGE_ICON)...\r\n"), i);

		lReturn = SendMessage(hWndStaticIcon, STM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)rghIcon[i]);

		LogVerbose(_T("[%i] Attempting STM_GETIMAGE(IMAGE_ICON)...\r\n"), i);

		lReturn = SendMessage(hWndStaticIcon, STM_GETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)0);

		if (!lReturn || (HICON)lReturn != rghIcon[i])
		{
			LogFail(_T("#### STM_SETIMAGE/STM_GETIMAGE(IMAGE_ICON) mismatch - Error: 0x%x ####\r\n"),
				GetLastError());
			LogFail(_T("#### STM_SETIMAGE/STM_GETIMAGE(IMAGE_ICON) - Handle Expected: 0x%x, Got: 0x%x ####\r\n"),
				rghIcon[i], lReturn);
			obj.fail();
			goto cleanup;
		}

		UpdateWindow(hWndStaticIcon);
		MessagePump();
		DLGITEM_ACTION_WAIT;
	}

cleanup:
	for (i = 0; i < BMP_COUNT; i++)
	{
		if (!DeleteObject(rghBmp[i]))
		{
			LogFail(_T("#### DeleteObject(rghBmp[%i]) failed - Error: 0x%x ####\r\n"),
				i, GetLastError());
			obj.fail();
		}
	}


	#ifdef NEVER
	for (i = 0; i < ICON_COUNT; i++)
	{
		if (!DeleteObject(rghIcon[i]))
		{
			LogFail(_T("#### DeleteObject(rghIcon[%i]) failed - Error: 0x%x ####\r\n"),
				i, GetLastError());
			obj.fail();
		}
	}
	#endif /* NEVER */

	return obj.status();
}


/*

@func:	FillRandomKeys, fills a buffer with specified number of random keys
(chars)

@rdesc:	void

@param:	[in/out] LPUINT rgChars: buffer to hold generated chars

@param:	[in] INT nChars: number of chars to be generated

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

static inline VOID FillRandomKeys(LPUINT rgChars, INT nChars)
{
	const INT ASCII_MIN_VALID = 8;
	const INT ASCII_MAX_VALID = 126;

	for (INT i = 0; i < nChars;  i++)
	{
		rgChars[i] = (TCHAR)RANDOM_INT(ASCII_MAX_VALID, ASCII_MIN_VALID);
	}

	return;
}


/*

@func:	RandomPostKeybdMessage, posts random/sends random keystrokes to the
specified window

@rdesc:	stress return status

@param:	[in] HWND hWnd: handle to the window

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

static DWORD RandomPostKeybdMessage(HWND hwnd)
{
	_stressrun_t obj(L"RandomPostKeybdMessage");

	const INT MAX_KEYCOUNT = 4;
	const INT MIN_KEYCOUNT = 1;
	const INT MAX_REPEAT = 4;
	const INT MIN_REPEAT = 1;

	INT nChars = RANDOM_INT(MAX_KEYCOUNT, MIN_KEYCOUNT);

	UINT rgChars[MAX_KEYCOUNT];

	INT i = 0;
	LONG lReturn = 0;

	FillRandomKeys(rgChars, nChars);

	LogVerbose(_T("RandomPostKeybdMessage: Posting %d chars\r\n"), nChars);
	SetLastError(0);

	for (i = 0; (i < nChars) && IsWindow(hwnd); i++)
	{
		INT nRepeat = RANDOM_INT(MAX_REPEAT, MIN_REPEAT);

		BOOL fUseSendMessage = RANDOM_BOOL(TRUE, FALSE);

		if (fUseSendMessage)
		{
			LogVerbose(_T("RandomPostKeybdMessage: Using SendMessage...\r\n"));

    		lReturn = SendMessage(hwnd, WM_CHAR, rgChars[i], nRepeat);

			LogVerbose(_T("RandomPostKeybdMessage: SendMessage(hwnd: 0x%x, WM_CHAR, WPARAM: %d, LPARAM: %d Return =%ld\r\n"),
				hwnd, rgChars[i], lReturn);
		}
		else
		{
			LogVerbose(_T("RandomPostKeybdMessage: Using PostMessage...\r\n"));

    		lReturn = PostMessage(hwnd, WM_CHAR, rgChars[i], nRepeat);

    		if (!lReturn)
    		{
				LogWarn2(_T("#### ThreadKeyProc: PostMessage failed - Error: 0x%x ####\r\n"),
					GetLastError());
				obj.warning2();
    		}

			LogVerbose(_T("RandomPostKeybdMessage: PostMessage(hwnd: 0x%x, WM_CHAR, WPARAM: %d, LPARAM: %d Return =%ld\r\n"),
				hwnd, rgChars[i], lReturn);
		}
	}

	MessagePump();

	return obj.status();
}


/*

@func:	ThreadKeyProc, threadproc to post keystrokes to control windows

@rdesc:	stress return status

@param:	[in] DLG_INFO *pdi: basically a thread info structure containing handle
to all control windows in the dialog

@fault:	none

@pre:	none

@post:	none

@note:	none

*/

DWORD ThreadKeyProc(DLG_INFO *pdi)
{
	_stressrun_t obj(L"ThreadKeyProc");
	const INT KEYPOSTCOUNT_MIN = 2;
	const INT KEYPOSTCOUNT_MAX = 8;
	INT nLoop;
	INT  i = 0;
	RECT rc;
	WORD xpos, ypos;

	pdi->fExitingThread = FALSE;

	nLoop = RANDOM_INT(KEYPOSTCOUNT_MAX, KEYPOSTCOUNT_MIN);

	while (nLoop--)
	{
		for (i = 0; i < pdi->nCtrlCount; i++)
		{
			if (RANDOM_BOOL(TRUE, FALSE))
			{
				LogComment(_T("Posting random keystrokesto the window...\r\n"));

				obj.test(RandomPostKeybdMessage(pdi->rghwndCtrls[i]));

				MessagePump();
				DLGITEM_KEYPOST_WAIT;
			}

			if (RANDOM_BOOL(TRUE, FALSE))
			{
				SetRectEmpty(&rc);
				GetClientRect(pdi->rghwndCtrls[i], &rc);

				xpos = RANDOM_INT(rc.right, rc.left);
				ypos = RANDOM_INT(rc.bottom, rc.top);

				LogComment(_T("Posting mouse-click (xpos: %d, ypos: %d) to the window...\r\n"),
					xpos, ypos);

				if (!PostMessage(pdi->rghwndCtrls[i], WM_LBUTTONDOWN, 0, MAKELPARAM(xpos, ypos)))
				{
					LogWarn2(_T("#### ThreadKeyProc: PostMessage(WM_LBUTTONDOWN) failed - Error: 0x%x ####\r\n"),
						GetLastError());
					obj.warning2();
				}

				DLGITEM_MOUSECLICK_WAIT;

				if (!PostMessage(pdi->rghwndCtrls[i], WM_LBUTTONUP, 0, MAKELPARAM(xpos, ypos)))
				{
					LogWarn2(_T("#### ThreadKeyProc: PostMessage(WM_LBUTTONUP) failed - Error: 0x%x ####\r\n"),
						GetLastError());
					obj.warning2();
				}
			}
		}
	}

	// post a char to the main window

	if (!PostMessage(pdi->hwndDlg, WM_CHAR, _T('A'),  0xFFFFFFFF))
	{
		LogWarn2(_T("#### ThreadKeyProc: PostMessage failed - Error: 0x%x ####\r\n"),
			GetLastError());
		obj.warning2();
	}

	MessagePump();

	pdi->fExitingThread = TRUE;

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
