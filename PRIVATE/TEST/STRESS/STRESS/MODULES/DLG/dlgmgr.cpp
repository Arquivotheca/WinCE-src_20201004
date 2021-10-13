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


Module Name: dlgmgr.cpp

Abstract: dialog manager APIs stress module

Notes: none

________________________________________________________________________________
*/

#include <windows.h>
#include <stressutils.h>
#include "resource.h"
#include "stressrun.h"
#include "common.h"


#define	BUTTON				0x0080
#define	EDIT				0x0081
#define	STATIC				0x0082
#define	LISTBOX				0x0083
#define	SCROLLBAR			0x0084
#define	COMBOBOX			0x0085

// Control IDs

const CHECK_BOX	= 100;
const RADIO_ONE	= 101;
const RADIO_TWO	= 102;
const RADIO_THREE = 103;
const RADIO_FOUR  = 104;
const STATIC_CTRL = 105;
const BUTTON_CTRL = 106;

#define	ALIGN_DWORD(lpb)	((LPBYTE)(((DWORD)lpb + 3) & ~3))   // make the pointer DWORD aligned


extern HINSTANCE g_hInstance;
extern _stressrun_t g_objDlgProc_Test;

#if 0
inline BYTE* lpwAlignByte(BYTE* lpIn)
{
	BYTE* lpByte = lpIn;

	BYTE* pByte = (BYTE*)(((DWORD)lpByte + 3) & ~3);  // make the pointer DWORD aligned

	return pByte;
}
#endif /* 0 */


struct _dlgtmpl_t
{
	LPDLGTEMPLATE	_pDlgStartTemplate;
	WORD			_wTotalSize;
	HLOCAL			_hLocalMem;
	BOOL			_fValidControl;
	_stressrun_t 	*_psr;

	_dlgtmpl_t(
		_stressrun_t *psr,
		TCHAR* pCaption,
		DWORD dwStyle,
		WORD x,
		WORD y,
		WORD cx,
		WORD cy,
		UINT ResID = 0,
		WORD wPointSize = 0,
		TCHAR* pFontName = NULL,
		TCHAR* pClassName = NULL)
	{
		if (psr)
		{
			_psr = psr;
		}

		_pDlgStartTemplate = NULL;

		WORD wCaptionLength = 0;
		WORD wFontNameLength = 0;
		WORD wClassNameLength = 0;

		_wTotalSize = 0;
		_hLocalMem = NULL;
		_fValidControl = FALSE;

		if (wPointSize > 0)
		{
			dwStyle |= DS_SETFONT;
		}
		else
		{
			dwStyle &= ~DS_SETFONT;
		}

		_wTotalSize += sizeof(DLGTEMPLATE);
		_wTotalSize += sizeof(WORD);		// menu

		if (ResID)
		{
			_wTotalSize += sizeof(WORD);	// menu
		}

		if (pClassName != NULL)
		{
			wClassNameLength = _tcslen(pClassName) + 1;
			wClassNameLength *= 2;
			_wTotalSize += wClassNameLength;
		}
		else
		{
			_wTotalSize += sizeof(WORD);
		}

		if (pCaption != NULL)
		{
			wCaptionLength = _tcslen(pCaption) + 1;
			wCaptionLength *= sizeof TCHAR;
			_wTotalSize += wCaptionLength;
		}

		if (wPointSize > 0)
		{
			_wTotalSize += sizeof(WORD);
			wFontNameLength = _tcslen(pFontName) + 1;
			wFontNameLength *= sizeof TCHAR;
			_wTotalSize += wFontNameLength;
		}

		_wTotalSize = ((_wTotalSize + 3) & ~3);

		_hLocalMem = LocalAlloc (LMEM_MOVEABLE | LMEM_ZEROINIT, _wTotalSize);

		if (!_hLocalMem)
		{
			LogFail(_T("#### _dlgtmpl_t::_dlgtmpl_t - LocalAlloc failed - Error: 0x%x ####\r\n"),
				GetLastError());
			_psr->fail();
			return;
		}

		_fValidControl = TRUE;

		LPBYTE pDlgTemplate = (LPBYTE)LocalLock(_hLocalMem);
		_pDlgStartTemplate = (LPDLGTEMPLATE)pDlgTemplate;

		DLGTEMPLATE DlgTemplate;

		DlgTemplate.style			= dwStyle;
		DlgTemplate.dwExtendedStyle = 0;
		DlgTemplate.cdit			= 0;
		DlgTemplate.x				= x;
		DlgTemplate.y				= y;
		DlgTemplate.cx				= cx;
		DlgTemplate.cy				= cy;

		memcpy(pDlgTemplate, &DlgTemplate, sizeof DLGTEMPLATE);
		pDlgTemplate += sizeof DLGTEMPLATE;

		if (ResID)
		{
			*(WORD*)pDlgTemplate = 0xFFFF;		// Menu
			pDlgTemplate += sizeof(WORD);
			*(WORD*)pDlgTemplate = (WORD) ResID;
			pDlgTemplate += sizeof(WORD);

		}
		else
		{
			*(WORD*)pDlgTemplate = 0x0000;		// Menu
			pDlgTemplate += sizeof(WORD);
		}

		if (pClassName)
		{
			_tcscpy((WORD*) pDlgTemplate, pClassName);
			pDlgTemplate += wClassNameLength;
		}
		else
		{
			*(WORD*)pDlgTemplate = (WORD)0x0000;       // Class
			pDlgTemplate += sizeof(WORD);
		}

		if (pCaption)
		{
			_tcscpy((WORD*) pDlgTemplate, pCaption);
			pDlgTemplate += wCaptionLength;
		}
		else
		{
			*(WORD*)pDlgTemplate = (WORD)0x0000;		// Caption
			pDlgTemplate += sizeof(WORD);

		}

		if (wPointSize > 0)
		{
			*((WORD*)pDlgTemplate) = (WORD) wPointSize;
			pDlgTemplate += sizeof(WORD);
			_tcscpy((WORD*) pDlgTemplate,pFontName);
			pDlgTemplate += wFontNameLength;
		}

		pDlgTemplate = ALIGN_DWORD(pDlgTemplate);

		return;
	}

	~_dlgtmpl_t()
	{
		if (LocalFree(_hLocalMem))
		{
			LogFail(_T("#### _dlgtmpl_t::~_dlgtmpl_t - LocalFree failed - Error: 0x%x ####\r\n"),
				GetLastError());
			_psr->fail();
		}
	}

	BOOL AddControl(
		WORD wClass,
		DWORD dwStyle,
		WORD x,
		WORD y,
		WORD cx,
		WORD cy,
		WORD CtrlID,
		TCHAR* pCaption = NULL)
	{
		WORD wCaptionLength = 0;
		UINT PreviousDataSize = _wTotalSize;

		if (pCaption)
		{
			wCaptionLength = _tcslen(pCaption) + 1;
			wCaptionLength *= sizeof TCHAR;
			_wTotalSize += wCaptionLength;
		}

		_wTotalSize += sizeof (DLGITEMTEMPLATE);

		_wTotalSize += 3 * sizeof(WORD); // (DWORD ButtonClass + nExtraStuff WORD)

		_wTotalSize = ((_wTotalSize + 3) & ~3);

		HLOCAL hMemTmp = (LPBYTE)LocalReAlloc((HLOCAL)_hLocalMem,_wTotalSize, LMEM_MOVEABLE | LMEM_ZEROINIT);

		if (!hMemTmp)
		{
			LogFail(_T("#### _dlgtmpl_t::~_dlgtmpl_t - LocalReAlloc failed - Error: 0x%x ####\r\n"),
				GetLastError());
			_psr->fail();
			return FALSE;
		}

		_hLocalMem = hMemTmp;

		LPBYTE pDlgTemplate = (LPBYTE)LocalLock(_hLocalMem);
		_pDlgStartTemplate = (LPDLGTEMPLATE)pDlgTemplate;

		_pDlgStartTemplate -> cdit++;

		pDlgTemplate += PreviousDataSize;

		pDlgTemplate = ALIGN_DWORD(pDlgTemplate);

		DLGITEMTEMPLATE DlgItemTemplate;

		DlgItemTemplate.style			= dwStyle;
		DlgItemTemplate.dwExtendedStyle	= 0;
		DlgItemTemplate.x				= x;
		DlgItemTemplate.y				= y;
		DlgItemTemplate.cx				= cx;
		DlgItemTemplate.cy				= cy;
		DlgItemTemplate.id				= CtrlID;

		memcpy(pDlgTemplate, &DlgItemTemplate, sizeof DLGITEMTEMPLATE);

		pDlgTemplate += sizeof DLGITEMTEMPLATE;

	  	// fill in class i.d. Button in this case

		*(WORD*)pDlgTemplate = 0xFFFF;
		pDlgTemplate += sizeof(WORD);
		*(WORD*)pDlgTemplate = (WORD)wClass;
		pDlgTemplate += sizeof(WORD);

	  	// copy the text of the first item

		if (pCaption)
		{
			_tcscpy((WORD*) pDlgTemplate, pCaption);
			pDlgTemplate += wCaptionLength;
		}

		*(WORD*)pDlgTemplate = (WORD) 0;  // advance pointer over nExtraStuff WORD

	  	pDlgTemplate += sizeof(WORD);

		pDlgTemplate = ALIGN_DWORD(pDlgTemplate);

		LocalUnlock(_hLocalMem);

		return TRUE;
	}
};


static LRESULT CALLBACK TestDlgProc(HWND hDlg, UINT uMsg,WPARAM wParam, LPARAM lParam)
{
	switch( uMsg )
	{
		case WM_INITDIALOG:
			break;
		case WM_COMMAND :
			switch (LOWORD(wParam))
			{
				case BUTTON_CTRL:
					LogVerbose(_T("TestDlgProc: Calling EndDialog...\r\n"));
					if (!EndDialog(hDlg, TRUE))
					{
						LogFail(_T("#### TestDlgProc: EndDialog failed - Error: 0x%x ####\r\n"),
							GetLastError());
						g_objDlgProc_Test.fail();
					}
					else
					{
						LogVerbose(_T("TestDlgProc: EndDialog successful\r\n"));
					}
					break;
			}
			break;
		default:
			return FALSE;
	}

   return TRUE;
}


DWORD Test_DialogManager(HWND hWnd)
{
	_stressrun_t obj(L"Test_DialogManager");

	//__________________________________________________________________________

	DWORD dwDialogStyle = DS_MODALFRAME | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | DS_3DLOOK ;
	DWORD dwStyle = NULL;

	LONG lBaseUnits = 0;
	const TABCTRL_COUNT = 3;
	HWND rghwndTabStopCtrl[TABCTRL_COUNT];
	const GROUPCTRL_COUNT = 4;
	HWND rghwndGroupCtrl[GROUPCTRL_COUNT];
	INT i = 0;
	const LOOPCOUNT_RANDOM = RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN);

	INT rgnCtrlIDs[] = {CHECK_BOX, RADIO_ONE, RADIO_TWO, RADIO_THREE, RADIO_FOUR, BUTTON_CTRL, STATIC_CTRL};

	HWND hDialogWnd = NULL;
	HWND hParentWnd = NULL;

	_dlgtmpl_t dlgtmpl(&obj, _T("Test Dialog Manager"), dwDialogStyle, 20, 20, 180, 70);

	LPDLGTEMPLATE lpDlgTemplate = (LPDLGTEMPLATE) dlgtmpl._pDlgStartTemplate;

	if (CESTRESS_FAIL == obj.status())
	{
		goto cleanup;
	}

	// check box

	dwStyle = WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX;
	dlgtmpl.AddControl(BUTTON, dwStyle, 9, 7, 70, 10, CHECK_BOX, _T("Check Box"));

	// radio buttons

	dwStyle = BS_AUTORADIOBUTTON  | WS_VISIBLE | WS_CHILD | WS_GROUP | WS_TABSTOP;
	dlgtmpl.AddControl(BUTTON, dwStyle, 13, 32, 37, 10, RADIO_ONE, _T("First"));

	dwStyle = BS_AUTORADIOBUTTON  | WS_VISIBLE | WS_CHILD;
	dlgtmpl.AddControl(BUTTON, dwStyle, 13, 45, 39, 10, RADIO_TWO, _T("Second"));

	dwStyle = BS_AUTORADIOBUTTON  | WS_VISIBLE | WS_CHILD;
	dlgtmpl.AddControl(BUTTON, dwStyle, 116, 32, 37, 10, RADIO_THREE, _T("Third"));

	dwStyle = BS_AUTORADIOBUTTON  | WS_VISIBLE | WS_CHILD ;
	dlgtmpl.AddControl(BUTTON, dwStyle, 116, 45, 39, 10, RADIO_FOUR, _T("Fourth"));

	dwStyle = BS_DEFPUSHBUTTON  | WS_VISIBLE | WS_TABSTOP | WS_CHILD | WS_GROUP;
	dlgtmpl.AddControl(BUTTON, dwStyle, 116, 8, 50, 14, BUTTON_CTRL, _T("Cancel"));

	dwStyle = BS_GROUPBOX | WS_VISIBLE | WS_CHILD;
	dlgtmpl.AddControl(BUTTON, dwStyle, 7, 21, 86, 39, STATIC_CTRL, _T("Group Box (Radio)"));

	if (RANDOM_CHOICE)
	{
		LogVerbose(_T("Test_DialogManager: Using parent window (0x%x)...\r\n"),
			hWnd);
		hParentWnd = hWnd;
	}
	else
	{
		LogVerbose(_T("Test_DialogManager: Not using a parent window...\r\n"));
		hParentWnd = NULL;
	}

	// CreateDialogIndirectParam

	LogVerbose(_T("Test_DialogManager: Calling CreateDialogIndirectParam...\r\n"));

	hDialogWnd = CreateDialogIndirectParam(
		g_hInstance,
		lpDlgTemplate,
		hParentWnd,
		(DLGPROC)TestDlgProc,
		(LPARAM)NULL
	);

	if (!hDialogWnd)
	{
		LogFail(_T("#### CreateDialogIndirectParam failed = Error: 0x%x ####\r\n"),
			GetLastError());
		obj.fail();
		goto cleanup;
	}

	SetForegroundWindow(hDialogWnd);
	ShowWindow(hDialogWnd, SW_SHOWNORMAL);
	UpdateWindow(hDialogWnd);
	MessagePump();

	// GetDialogBaseUnits

	LogVerbose(_T("Test_DialogManager: Calling GetDialogBaseUnits...\r\n"));

	lBaseUnits = GetDialogBaseUnits();

	LogVerbose(_T("Test_DialogManager: Calling GetDlgItem to store control HWNDs...\r\n"));

	rghwndTabStopCtrl[0] = GetDlgItem(hDialogWnd, CHECK_BOX);
	rghwndTabStopCtrl[1] = GetDlgItem(hDialogWnd, RADIO_ONE);
	rghwndTabStopCtrl[2] = GetDlgItem(hDialogWnd, BUTTON_CTRL);

	rghwndGroupCtrl[0] = GetDlgItem(hDialogWnd, RADIO_ONE);
	rghwndGroupCtrl[1] = GetDlgItem(hDialogWnd, RADIO_TWO);
	rghwndGroupCtrl[2] = GetDlgItem(hDialogWnd, RADIO_THREE);
	rghwndGroupCtrl[3] = GetDlgItem(hDialogWnd, RADIO_FOUR);

	// mixed controls

	for (i = 0; i < LOOPCOUNT_RANDOM; i++)
	{

		INT nRand = rand() % (TABCTRL_COUNT + GROUPCTRL_COUNT);
		INT nTempCtrlID = rgnCtrlIDs[nRand];

		LogVerbose(_T("Test_DialogManager: [%i] Calling GetDlgItem...\r\n"), i);

		HWND hCtrlWnd = GetDlgItem(hDialogWnd, rgnCtrlIDs[nRand]);

		DWORD dwError = GetLastError();

		if (!IsWindow(hCtrlWnd))
		{
			LogFail(_T("#### Test_DialogManager: GetDlgItem failed - Error: 0x%x ####\r\n"),
				dwError);
		}

		LogVerbose(_T("Test_DialogManager: [%i] Calling GetDlgCtrlID...\r\n"), i);

		INT nCtrlID  = GetDlgCtrlID(hCtrlWnd);

		if (nCtrlID != rgnCtrlIDs[nRand])
		{
			LogFail(_T("#### Test_DialogManager: GetDlgItem or GetDlgCtrlID failed ####\r\n"));
			obj.fail();
			goto cleanup;
		}
	}

	// tabstop Controls

	for (i = 0; i < LOOPCOUNT_RANDOM; i++)
	{
		INT nRand = rand() % TABCTRL_COUNT;
		HWND hwndNextCtrl = rghwndTabStopCtrl[nRand];

		for (INT j = 0; j < TABCTRL_COUNT; j++)
		{
			LogVerbose(_T("Test_DialogManager: [%i, %i] Calling GetNextDlgTabItem...\r\n"), i, j);
			hwndNextCtrl = GetNextDlgTabItem(hDialogWnd, hwndNextCtrl, TRUE);
		}

		if (hwndNextCtrl != rghwndTabStopCtrl[nRand])
		{
			LogFail(_T("#### Test_DialogManager: GetNextDlgTabItem failed ####\r\n"));
			obj.fail();
			goto cleanup;
		}
	}

	// group box controls

	for (i = 0; i < LOOPCOUNT_RANDOM; i++)
	{
		INT nRand = rand() % GROUPCTRL_COUNT;
		HWND hwndNextCtrl = rghwndGroupCtrl[nRand];

		for (INT j = 0; j < GROUPCTRL_COUNT; j++)
		{
			LogVerbose(_T("Test_DialogManager: [%i, %i] Calling GetNextDlgGroupItem...\r\n"), i, j);
			hwndNextCtrl = GetNextDlgGroupItem(hDialogWnd, hwndNextCtrl, TRUE);
		}

		if (hwndNextCtrl != rghwndGroupCtrl[nRand])
		{
			LogFail(_T("#### Test_DialogManager: GetNextDlgGroupItem failed ####\r\n"));
			obj.fail();
			goto cleanup;
		}
	}

	MessagePump();
	LogVerbose(_T("Test_DialogManager: Calling SendMessage(WM_COMMAND, BUTTON_CTRL)...\r\n"));
	SendMessage(hDialogWnd, WM_COMMAND, BUTTON_CTRL, 0);
	MessagePump();

	//__________________________________________________________________________

cleanup:
	if (IsWindow(hDialogWnd))
	{
		LogVerbose(_T("Test_DialogManager: Calling DestroyWindow...\r\n"));

		if (!DestroyWindow(hDialogWnd))
		{
			LogFail(_T("#### Test_DialogManager: DestroyWindow failed - Error: 0x%x ####\r\n"),
				GetLastError());
			obj.fail();
		}
	}

	return obj.status();
}
