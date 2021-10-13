//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "kernel.h"
#include <KmodeEntries.hpp>
#include <GweApiSet1.hpp>

extern "C" LPVOID pGwesHandler;

extern "C"
const
CINFO*
SwitchToProc(
	CALLSTACK*	pcstk,
	DWORD		dwAPISet
	);

extern "C"
void
SwitchBack(
	void
	);


extern "C"
void
DoMapPtr(
	void**
    );

#define MapArgPtr(P) DoMapPtr((LPVOID *)&(P))


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
int
SC_GetSystemMetrics(
	int	nIndex
	)
{
			CALLSTACK	cstk;
			int			iRet;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);
	__try
	{
		iRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pGetSystemMetrics(nIndex);
//		iRet = (*(int (*)(int))(pci->ppfnMethods[MID_GetSystemMetrics]))(nIndex);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		iRet = 0;
	}
	SwitchBack();
	return iRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
ATOM
SC_RegisterClassWStub(
	const	WNDCLASSW*	lpWndClass,
	const	WCHAR*		lpszClassName,
			HANDLE		hprcWndProc
	)
{
	CALLSTACK	cstk;
	ATOM		aRet;

	//	Need to map pointers before switching to target proc.
	MapArgPtr(lpWndClass);
	MapArgPtr(lpszClassName);

	const CINFO *pci = SwitchToProc(&cstk,SH_WMGR);
	__try
	{
 		aRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pRegisterClassWApiSetEntry(lpWndClass,lpszClassName,hprcWndProc);
// 		aRet = (*(ATOM (*)(CONST WNDCLASSW *, LPCWSTR, HANDLE))(pci->ppfnMethods[MID_RegisterClassWApiSetEntry]))(lpWndClass,lpszClassName,hprcWndProc);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		aRet = 0;
	}
	SwitchBack();
	return aRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
UINT
SC_RegisterClipboardFormatW(
	const	WCHAR*	lpszFormat
    )
{
	CALLSTACK cstk;
	UINT uRet;

	//	Need to map pointers before switching to target proc.
	MapArgPtr(lpszFormat);

	const CINFO *pci = SwitchToProc(&cstk,SH_WMGR);
	__try
	{
		uRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pRegisterClipboardFormatW(lpszFormat);
//		uRet = (*(UINT (*)(LPCWSTR))(pci->ppfnMethods[MID_RegisterClipboardFormatW]))(lpszFormat);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		uRet = 0;
	}
	SwitchBack();
	return uRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
int
SC_SetBkMode(
	HDC	hdc,
	int	iBkMode
	)
{
			CALLSTACK	cstk;
			int			iRet;
	const	CINFO *pci = SwitchToProc(&cstk,SH_GDI);
	__try
		{
		iRet = ((GweApiSet2_t*)(pci->ppfnMethods)) -> m_pSetBkMode(hdc,iBkMode);
//		iRet = (*(int (*)(HDC,int))(pci->ppfnMethods[SETBKMODE]))(hdc,iBkMode);
		}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		iRet = 0;
	}
	SwitchBack();
	return iRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
COLORREF
SC_SetTextColor(
	HDC			hdc,
	COLORREF	crColor
	)
{
			CALLSTACK	cstk;
			COLORREF	cRet;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_GDI);
	__try
	{
		cRet = ((GweApiSet2_t*)(pci->ppfnMethods)) -> m_pSetTextColor(hdc,crColor);
//		cRet = (*(COLORREF (*)(HDC,COLORREF))(pci->ppfnMethods[SETTEXTCOLOR]))(hdc,crColor);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		cRet = 0;
	}
	SwitchBack();
	return cRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
BOOL
SC_InvalidateRect(
			HWND	hwnd,
	const	RECT*	prc,
			BOOL	fErase
	)
{
	CALLSTACK	cstk;
	BOOL		bRet;

	//	Need to map pointers before switching to target proc.
	MapArgPtr(prc);

	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);

	__try
	{
		bRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pInvalidateRect(hwnd,prc,fErase);
//		bRet = (*(BOOL (*)(HWND, LPCRECT, BOOL))(pci->ppfnMethods[MID_InvalidateRect]))(hwnd,prc,fErase);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		bRet = 0;
	}
	SwitchBack();
	return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
BOOL
SC_TransparentImage(
	HDC			hdcDest,
	int			nXDest,
	int			nYDest,
	int			nWidthDest,
	int			nHeightDest,
	HANDLE		hImgSrc,
	int			nXSrc,
	int			nYSrc,
	int			nWidthSrc,
	int			nHeightSrc,
	COLORREF	crTransparentColor
	)
{
			CALLSTACK	cstk;
			BOOL		bRet;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_GDI);
	__try
	{
		bRet  = ((GweApiSet2_t*)(pci->ppfnMethods)) -> m_pTransparentImage(
														hdcDest,
														nXDest,
														nYDest,
														nWidthDest,
														nHeightDest,
														hImgSrc,
														nXSrc,
														nYSrc,
														nWidthSrc,
														nHeightSrc,
														crTransparentColor
														);
//		bRet = (*(BOOL (*)(HDC,int,int,int,int,HANDLE,int,int,int,int,COLORREF))(pci->ppfnMethods[TRANSPARENTIMAGE]))
//			   (hdcDest, nXDest, nYDest, nWidthDest, nHeightDest, hImgSrc, nXSrc, nYSrc, nWidthSrc, nHeightSrc, crTransparentColor);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		bRet = 0;
	}
	SwitchBack();
	return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
BOOL
SC_IsDialogMessageW(
	HWND	hDlg,
	MSG*	lpMsg
    )
{
	CALLSTACK	cstk;
	BOOL		bRet;

	//	Need to map pointers before switching to target proc.
	MapArgPtr(lpMsg);

	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);

	__try
	{
		bRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pIsDialogMessageW(hDlg,lpMsg);
//		bRet = (*(BOOL (*)(HWND,LPMSG))(pci->ppfnMethods[MID_IsDialogMessageW]))(hDlg,lpMsg);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		bRet = 0;
	}
	SwitchBack();
	return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
BOOL
SC_PostMessageW(
	HWND	hWnd,
	UINT	Msg,
	WPARAM	wParam,
	LPARAM	lParam
	)
{
			CALLSTACK	cstk;
			BOOL		bRet;
    const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);
	__try
	{
		bRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pPostMessageW(hWnd,Msg,wParam,lParam);
//		bRet = (*(BOOL (*)(HWND,UINT,WPARAM,LPARAM))(pci->ppfnMethods[MID_PostMessageW]))(hWnd,Msg,wParam,lParam);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		bRet = 0;
	}
	SwitchBack();
	return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
BOOL
SC_IsWindowVisible(
	HWND	hWnd
	)
{
			CALLSTACK	cstk;
			BOOL		bRet;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);
	__try
	{
		bRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pIsWindowVisible(hWnd);
//		bRet = (*(BOOL (*)(HWND))(pci->ppfnMethods[MID_IsWindowVisible]))(hWnd);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		bRet = 0;
	}
	SwitchBack();
	return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
SHORT
SC_GetKeyState(
	int	nVirtKey
	)
{
			CALLSTACK	cstk;
			SHORT		sRet;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);
	__try
	{
		sRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pGetKeyState(nVirtKey);
//		sRet = (*(SHORT (*)(int))(pci->ppfnMethods[MID_GetKeyState]))(nVirtKey);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		sRet = 0;
	}
	SwitchBack();
	return sRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
HDC
SC_BeginPaint(
	HWND			hwnd,
	PAINTSTRUCT*	pps
	)
{
	CALLSTACK	cstk;
	HDC			hRet;

	//	Need to map pointers before switching to target proc.
	MapArgPtr(pps);

	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);

	__try
	{
		hRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pBeginPaint(hwnd,pps);
//		hRet = (*(HDC (*)(HWND,LPPAINTSTRUCT))(pci->ppfnMethods[MID_BeginPaint]))(hwnd,pps);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		hRet = 0;
	}
	SwitchBack();
	return hRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
BOOL
SC_EndPaint(
	HWND			hwnd,
	PAINTSTRUCT*	pps
    )
{
	CALLSTACK	cstk;
	BOOL		bRet;

	//	Need to map pointers before switching to target proc.
	MapArgPtr(pps);

	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);

	__try
	{
		bRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pEndPaint(hwnd,pps);
//		bRet = (*(BOOL (*)(HWND,LPPAINTSTRUCT))(pci->ppfnMethods[MID_EndPaint]))(hwnd,pps);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		bRet = 0;
	}
	SwitchBack();
	return bRet;
}






//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
LRESULT
SC_DefWindowProcW(
	HWND	hwnd,
	UINT	msg,
	WPARAM	wParam,
	LPARAM	lParam
	)
{
			CALLSTACK	cstk;
			LRESULT		lRet;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);

	__try
	{
		lRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pDefWindowProcW(hwnd,msg,wParam,lParam);
//		lRet = (*(LRESULT (*)(HWND, UINT, WPARAM, LPARAM))(pci->ppfnMethods[MID_DefWindowProcW]))(hwnd,msg,wParam,lParam);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		lRet = 0;
	}
	SwitchBack();
	return lRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
BOOL
SC_GetClipCursor(
	RECT*	lpRect
	)
{
	CALLSTACK	cstk;
	BOOL		bRet;

	//	Need to map pointers before switching to target proc.
	MapArgPtr(lpRect);

	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);
	__try
	{
		bRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pGetClipCursor(lpRect);
//		bRet = (*(BOOL (*)(LPRECT))(pci->ppfnMethods[MID_GetClipCursor]))(lpRect);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		bRet = 0;
	}
	SwitchBack();
	return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
HDC
SC_GetDC (
	HWND	hwnd
	)
{
			CALLSTACK	cstk;
			HDC			hRet;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);
	__try
	{
		hRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pGetDC(hwnd);
//		hRet = (*(HDC (*)(HWND))(pci->ppfnMethods[MID_GetDC]))(hwnd);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		hRet = 0;
	}
	SwitchBack();
	return hRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
HWND
SC_GetFocus(
	void
	)
{
			CALLSTACK	cstk;
			HWND		hRet;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);
	__try
	{
		hRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pGetFocus();
//		hRet = (*(HWND (*)())(pci->ppfnMethods[MID_GetFocus]))();
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		hRet = 0;
	}
	SwitchBack();
	return hRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
BOOL
SC_GetMessageW(
	MSG*	pMsgr,
	HWND	hwnd,
	UINT	wMsgFilterMin,
	UINT	wMsgFilterMax
	)
{
	CALLSTACK	cstk;
	BOOL		bRet;

	//	Need to map pointers before switching to target proc.
	MapArgPtr(pMsgr);

	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);

	__try
	{
		bRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pGetMessageW(pMsgr,hwnd,wMsgFilterMin,wMsgFilterMax);
//		bRet = (*(BOOL (*)(PMSG,HWND,UINT,UINT))(pci->ppfnMethods[MID_GetMessageW]))(pMsgr,hwnd,wMsgFilterMin,wMsgFilterMax);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		bRet = 0;
	}
	SwitchBack();
	return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
HWND
SC_GetWindow(
	HWND	hwnd,
	UINT	uCmd
	)
{
			CALLSTACK	cstk;
			HWND		hRet;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);
	__try
	{
		hRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pGetWindow(hwnd,uCmd);
//		hRet = (*(HWND (*)(HWND,UINT))(pci->ppfnMethods[MID_GetWindow]))(hwnd,uCmd);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		hRet = 0;
	}
	SwitchBack();
	return hRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
BOOL
SC_PeekMessageW(
	MSG*	pMsg,
	HWND	hWnd,
	UINT	wMsgFilterMin,
	UINT	wMsgFilterMax,
	UINT	wRemoveMsg
	)
{
	CALLSTACK	cstk;
	BOOL		bRet;

	//	Need to map pointers before switching to target proc.
	MapArgPtr(pMsg);

	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);

	__try
	{
		bRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pPeekMessageW(pMsg,hWnd,wMsgFilterMin,wMsgFilterMax,wRemoveMsg);
//		bRet = (*(BOOL (*)(PMSG,HWND,UINT,UINT,UINT))(pci->ppfnMethods[MID_PeekMessageW]))(pMsg,hWnd,wMsgFilterMin,wMsgFilterMax,wRemoveMsg);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		bRet = 0;
	}
	SwitchBack();
	return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
BOOL
SC_ReleaseDC(
	HWND	hwnd,
	HDC		hdc
	)
{
			CALLSTACK	cstk;
			BOOL		bRet;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);
	__try
	{
		bRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pReleaseDC(hwnd,hdc);
//		bRet = (*(BOOL (*)(HWND,HDC))(pci->ppfnMethods[MID_ReleaseDC]))(hwnd,hdc);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		bRet = 0;
	}
	SwitchBack();
	return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
LRESULT
SC_SendMessageW(
	HWND	hwnd,
	UINT	Msg,
	WPARAM	wParam,
	LPARAM	lParam
	)
{
			CALLSTACK	cstk;
			LRESULT		lRet;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);
	__try
	{
		lRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pSendMessageW(hwnd,Msg,wParam,lParam);
//		lRet = (*(LRESULT (*)(HWND,UINT,WPARAM,LPARAM))(pci->ppfnMethods[MID_SendMessageW]))(hwnd,Msg,wParam,lParam);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		lRet = 0;
	}
	SwitchBack();
	return lRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
int
SC_SetScrollInfo(
			HWND		hwnd,
			int			fnBar,
	const	SCROLLINFO*	lpsi,
			BOOL		fRedraw
	)
{
	CALLSTACK	cstk;
	int			iRet;

	//	Need to map pointers before switching to target proc.
	MapArgPtr(lpsi);

	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);

	__try
	{
		iRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pSetScrollInfo(hwnd,fnBar,lpsi,fRedraw);
//		iRet = (*(int (*)(HWND,int,LPCSCROLLINFO,BOOL))(pci->ppfnMethods[MID_SetScrollInfo]))(hwnd,fnBar,lpsi,fRedraw);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		iRet = 0;
	}
	SwitchBack();
	return iRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
LONG
SC_SetWindowLongW(
	HWND	hwnd,
	int		nIndex,
	LONG	lNewLong
	)
{
			CALLSTACK	cstk;
			LONG		lRet;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);
	__try
	{
		lRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pSetWindowLongW(hwnd,nIndex,lNewLong);
//		lRet = (*(LONG (*)(HWND,int,LONG))(pci->ppfnMethods[MID_SetWindowLongW]))(hwnd,nIndex,lNewLong);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		lRet = 0;
	}
	SwitchBack();
	return lRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
BOOL
SC_SetWindowPos(
    HWND hwnd,
    HWND hwndInsertAfter,
    int x,
    int y,
    int dx,
    int dy,
    UINT fuFlags
    )
{
			CALLSTACK cstk;
			BOOL bRet;
	const	CINFO*	pci = SwitchToProc(&cstk,SH_WMGR);
	__try
	{
		bRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pSetWindowPos(hwnd,hwndInsertAfter,x,y,dx,dy,fuFlags);
//		bRet = (*(BOOL (*)(HWND,HWND,int,int,int,int,UINT))(pci->ppfnMethods[MID_SetWindowPos]))(hwnd,hwndInsertAfter,x,y,dx,dy,fuFlags);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		bRet = 0;
	}
	SwitchBack();
	return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
HBRUSH
SC_CreateSolidBrush(
    COLORREF crColor
    )
{
			CALLSTACK	cstk;
			HBRUSH		hRet;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_GDI);
	__try
	{
		hRet = ((GweApiSet2_t*)(pci->ppfnMethods)) -> m_pCreateSolidBrush(crColor);
//		hRet = (*(HBRUSH (*)(COLORREF))(pci->ppfnMethods[CREATESOLIDBRUSH]))(crColor);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		hRet = 0;
	}
	SwitchBack();
	return hRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
BOOL
SC_DeleteMenu(
    HMENU hmenu,
    UINT uPosition,
    UINT uFlags
    )
{
			CALLSTACK	cstk;
			BOOL		bRet;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);
	__try
	{
		bRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pDeleteMenu(hmenu,uPosition,uFlags);
//		bRet = (*(BOOL (*)(HMENU, UINT, UINT))(pci->ppfnMethods[MID_DeleteMenu]))(hmenu,uPosition,uFlags);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		bRet = 0;
	}
	SwitchBack();
	return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
BOOL
SC_DeleteObject(
    HGDIOBJ hObject
    )
{
			CALLSTACK	cstk;
			BOOL		bRet;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_GDI);
	__try
	{
		bRet = ((GweApiSet2_t*)(pci->ppfnMethods)) -> m_pDeleteObject(hObject);
//		bRet = (*(BOOL (*)(HGDIOBJ))(pci->ppfnMethods[DELETEOBJECT]))(hObject);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		bRet = 0;
	}
	SwitchBack();
	return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
int
SC_DrawTextW(
    HDC hdc,
    LPCWSTR lpszStr,
    int cchStr,
    RECT *lprc,
    UINT wFormat
    )
{
	CALLSTACK	cstk;
	int			iRet;

	//	Need to map pointers before switching to target proc.
	MapArgPtr(lpszStr);
	MapArgPtr(lprc);

	const	CINFO*		pci = SwitchToProc(&cstk,SH_GDI);

	__try
	{
		iRet = ((GweApiSet2_t*)(pci->ppfnMethods)) -> m_pDrawTextW(hdc,lpszStr,cchStr,lprc,wFormat);
//		iRet = (*(int (*)(HDC,LPCWSTR,int,RECT *,UINT))(pci->ppfnMethods[DRAWTEXTW]))(hdc,lpszStr,cchStr,lprc,wFormat);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		iRet = 0;
	}
	SwitchBack();
	return iRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
BOOL
SC_ExtTextOutW(
    HDC hdc,
    int X,
    int Y,
    UINT fuOptions,
    CONST RECT *lprc,
    LPCWSTR lpszString,
    UINT cbCount,
    CONST INT *lpDx
    )
{
	CALLSTACK	cstk;
	BOOL		bRet;

	//	Need to map pointers before switching to target proc.
	MapArgPtr(lprc);
	MapArgPtr(lpszString);
	MapArgPtr(lpDx);

	const	CINFO*		pci = SwitchToProc(&cstk,SH_GDI);

	__try
	{
		bRet = ((GweApiSet2_t*)(pci->ppfnMethods)) -> m_pExtTextOutW(hdc,X,Y,fuOptions,lprc,lpszString,cbCount,lpDx);
//		bRet = (*(BOOL (*)(HDC,int,int,UINT,CONST RECT *, LPCWSTR, UINT, CONST INT *))(pci->ppfnMethods[EXTTEXTOUTW]))(hdc,X,Y,fuOptions,lprc,lpszString,cbCount,lpDx);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		bRet = 0;
	}
	SwitchBack();
	return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
int
SC_FillRect(
    HDC hdc,
    CONST RECT *lprc,
    HBRUSH hbr
    )
{
	CALLSTACK	cstk;
    int			iRet;

	//	Need to map pointers before switching to target proc.
	MapArgPtr(lprc);

	const	CINFO*		pci = SwitchToProc(&cstk,SH_GDI);

	__try
	{
		iRet = ((GweApiSet2_t*)(pci->ppfnMethods)) -> m_pFillRect(hdc,lprc,hbr);
//		iRet = (*(int (*)(HDC,CONST RECT *, HBRUSH))(pci->ppfnMethods[FILLRECT]))(hdc,lprc,hbr);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		iRet = 0;
	}
	SwitchBack();
	return iRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
SHORT
SC_GetAsyncKeyState(
    INT vKey
    )
{
			CALLSTACK	cstk;
			SHORT		sRet;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);
	__try
	{
		sRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pGetAsyncKeyState(vKey);
//		sRet = (*(SHORT (*)(INT))(pci->ppfnMethods[MID_GetAsyncKeyState]))(vKey);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		sRet = 0;
	}
	SwitchBack();
	return sRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
int
SC_GetDlgCtrlID(
    HWND hWnd
    )
{
			CALLSTACK	cstk;
			int			iRet;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);
	__try
	{
		iRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pGetDlgCtrlID(hWnd);
//		iRet = (*(int (*)(HWND))(pci->ppfnMethods[MID_GetDlgCtrlID]))(hWnd);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		iRet = 0;
	}
	SwitchBack();
	return iRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
HGDIOBJ
SC_GetStockObject(
    int fnObject
    )
{
			CALLSTACK	cstk;
			HGDIOBJ		hRet;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_GDI);
	__try
	{
		hRet = ((GweApiSet2_t*)(pci->ppfnMethods)) -> m_pGetStockObject(fnObject);
//		hRet = (*(HGDIOBJ (*)(int))(pci->ppfnMethods[GETSTOCKOBJECT]))(fnObject);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		hRet = 0;
	}
	SwitchBack();
	return hRet;
}





//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
BOOL
SC_ClientToScreen(
    HWND hwnd,
    LPPOINT lpPoint
    )
{
	CALLSTACK	cstk;
	BOOL		bRet;

	//	Need to map pointers before switching to target proc.
	MapArgPtr(lpPoint);

	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);

	__try
	{
		bRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pClientToScreen(hwnd,lpPoint);
//		bRet = (*(BOOL (*)(HWND, LPPOINT))(pci->ppfnMethods[MID_ClientToScreen]))(hwnd,lpPoint);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		bRet = 0;
	}
	SwitchBack();
	return bRet;
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
HBRUSH
SC_GetSysColorBrush(
    int nIndex
    )
{
	CALLSTACK cstk;
	HBRUSH hRet ;
	const CINFO *pci;
	pci = SwitchToProc(&cstk,SH_GDI);
	__try
	{
		hRet = ((GweApiSet2_t*)(pci->ppfnMethods)) -> m_pGetSysColorBrush(nIndex);
//		hRet = (*(HBRUSH (*)(int))(pci->ppfnMethods[GETSYSCOLORBRUSH]))(nIndex);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		hRet = 0;
	}
	SwitchBack();
	return hRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
HWND
SC_GetParent(
    HWND hwnd
    )
{
			CALLSTACK	cstk;
			HWND		hRet;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_WMGR);
	__try
	{
		hRet = ((GweApiSet1_t*)(pci->ppfnMethods)) -> m_pGetParent(hwnd);
//		hRet = (*(HWND (*)(HWND))(pci->ppfnMethods[MID_GetParent]))(hwnd);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		hRet = 0;
	}
	SwitchBack();
	return hRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
COLORREF
SC_SetBkColor(
    HDC hDC,
    COLORREF dwColor
    )
{
			CALLSTACK	cstk;
			COLORREF	cr;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_GDI);
	__try
	{
		cr = ((GweApiSet2_t*)(pci->ppfnMethods)) -> m_pSetBkColor(hDC,dwColor);
//		cr = (*(COLORREF (*)(HDC,COLORREF))(pci->ppfnMethods[SETBKCOLOR]))(hDC,dwColor);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		cr = 0;
	}
	SwitchBack();
	return cr;
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
BOOL
SC_GetTextExtentExPointW(
    HDC hdc,
    LPCWSTR lpszStr,
    int cchString,
    int nMaxExtent,
    LPINT lpnFit,
    LPINT alpDx,
    LPSIZE lpSize
    )
{
	CALLSTACK	cstk;
	BOOL		bRet;
			
	//	Need to map pointers before switching to target proc.
	MapArgPtr(lpszStr);
	MapArgPtr(lpnFit);
	MapArgPtr(alpDx);
	MapArgPtr(lpSize);

	const	CINFO*		pci = SwitchToProc(&cstk,SH_GDI);

	__try
	{
		bRet = ((GweApiSet2_t*)(pci->ppfnMethods)) -> m_pGetTextExtentExPointW(hdc,lpszStr,cchString,nMaxExtent,lpnFit,alpDx,lpSize);
//		bRet = (*(BOOL (*)(HDC,LPCWSTR,int,int,LPINT,LPINT,LPSIZE))(pci->ppfnMethods[GETTEXTEXTENTEXPOINTW]))(hdc,lpszStr,cchString,nMaxExtent,lpnFit,alpDx,lpSize);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		bRet = 0;
	}
	SwitchBack();
	return bRet;
}




extern "C"
HGDIOBJ
SC_SelectObject(
    HDC hDC,
    HANDLE hObj
    )
{
			CALLSTACK	cstk;
			HGDIOBJ		hRet;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_GDI);
	__try
	{
		hRet = ((GweApiSet2_t*)(pci->ppfnMethods)) -> m_pSelectObject(hDC,hObj);
//		hRet = (*(HGDIOBJ (*)(HANDLE,HANDLE))(pci->ppfnMethods[SELECTOBJECT]))(hDC,hObj);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		hRet = 0;
	}
	SwitchBack();
	return hRet;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
BOOL
SC_PatBlt(
    HDC hdcDest,
    int nXLeft,
    int nYLeft,
    int nWidth,
    int nHeight,
    DWORD dwRop
    )
{
			CALLSTACK	cstk;
			BOOL		bRet = 0;
	const	CINFO*		pci = SwitchToProc(&cstk,SH_GDI);
	__try
	{
		bRet = ((GweApiSet2_t*)(pci->ppfnMethods)) -> m_pPatBlt(hdcDest,nXLeft,nYLeft,nWidth,nHeight,dwRop);
//		bRet = (*(BOOL (*)(HDC,int,int,int,int,DWORD))(pci->ppfnMethods[PATBLT]))(hdcDest,nXLeft,nYLeft,nWidth,nHeight,dwRop);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		(*(void (*)(void))pGwesHandler)();
		bRet = 0;
	}
	SwitchBack();
	return bRet;
}



// NOTE: we do not need to validate the pointers if the following functions are called directly
//       -- the thread is in KMode already, it can do anything it wants.



static	KmodeEntries_t	g_KmodeEntries =
{
	SC_EventModify,			//	0
	SC_ReleaseMutex,
	SC_CreateAPIHandle,
	SC_MapViewOfFile,
	SC_ThreadGetPrio,
	SC_ThreadSetPrio,
	SC_SelectObject,
	SC_PatBlt,
	SC_GetTextExtentExPointW,
	SC_GetSysColorBrush,	//	9

	SC_SetBkColor,			// 010
	SC_GetParent,
	SC_InvalidateRect,
	NKRegOpenKeyExW,
	NKRegQueryValueExW,
	SC_CreateFileW,
	SC_ReadFile,
	SC_OpenDatabaseEx,
	SC_SeekDatabase,
	SC_ReadRecordPropsEx,   // 019

	NKRegCreateKeyExW,		// 020
	SC_DeviceIoControl,
	SC_CloseHandle,
	NKRegCloseKey,
	SC_ClientToScreen,
	SC_DefWindowProcW,
	SC_GetClipCursor,
	SC_GetDC,
	SC_GetFocus,
	SC_GetMessageW,			// 029

	SC_GetWindow,			// 030
	SC_PeekMessageW,
	SC_ReleaseDC,
	SC_SendMessageW,
	SC_SetScrollInfo,
	SC_SetWindowLongW,
	SC_SetWindowPos,
	SC_CreateSolidBrush,
	SC_DeleteMenu,
	SC_DeleteObject,		// 039

	SC_DrawTextW,           // 040
	SC_ExtTextOutW,
	SC_FillRect,
	SC_GetAsyncKeyState,
	SC_GetDlgCtrlID,
	SC_GetStockObject,
	SC_GetSystemMetrics,
	SC_RegisterClassWStub,
	SC_RegisterClipboardFormatW,
	SC_SetBkMode,           // 049

	SC_SetTextColor,        // 050
	SC_TransparentImage,
	SC_IsDialogMessageW,
	SC_PostMessageW,
	SC_IsWindowVisible,
	SC_GetKeyState,
	SC_BeginPaint,
	SC_EndPaint,
	SC_PerformCallBack4,
	SC_CeWriteRecordProps,  // 059

	SC_ReadFileWithSeek,    // 060
	SC_WriteFileWithSeek,   // 061



};

extern "C"
void*
KmodeEntries(
	void
	)
{
	return &g_KmodeEntries;
}



#ifdef NEVER_DEFINE
extern "C"
const PFNVOID ExtraMethods[] = {
	(PFNVOID)SC_EventModify,         // 000
	(PFNVOID)SC_ReleaseMutex,        // 001
	(PFNVOID)SC_CreateAPIHandle,     // 002
	(PFNVOID)SC_MapViewOfFile,       // 003
	(PFNVOID)SC_ThreadGetPrio,       // 004
	(PFNVOID)SC_ThreadSetPrio,       // 005
	(PFNVOID)SC_SelectObject,        // 006
	(PFNVOID)SC_PatBlt,              // 007
	(PFNVOID)SC_GetTextExtentExPointW,// 008
	(PFNVOID)SC_GetSysColorBrush,    // 009

	(PFNVOID)SC_SetBkColor,          // 010
	(PFNVOID)SC_GetParent,           // 011
	(PFNVOID)SC_InvalidateRect,      // 012
	(PFNVOID)NKRegOpenKeyExW,        // 013
	(PFNVOID)NKRegQueryValueExW,     // 014
	(PFNVOID)SC_CreateFileW,         // 015
	(PFNVOID)SC_ReadFile,            // 016
	(PFNVOID)SC_OpenDatabaseEx,      // 017
	(PFNVOID)SC_SeekDatabase,        // 018
	(PFNVOID)SC_ReadRecordPropsEx,   // 019

	(PFNVOID)NKRegCreateKeyExW,      // 020
	(PFNVOID)SC_DeviceIoControl,     // 021
	(PFNVOID)SC_CloseHandle,         // 022
	(PFNVOID)NKRegCloseKey,          // 023
	(PFNVOID)SC_ClientToScreen,      // 024
	(PFNVOID)SC_DefWindowProcW,      // 025
	(PFNVOID)SC_GetClipCursor,       // 026
	(PFNVOID)SC_GetDC,               // 027
	(PFNVOID)SC_GetFocus,            // 028
	(PFNVOID)SC_GetMessageW,         // 029

	(PFNVOID)SC_GetWindow,           // 030
	(PFNVOID)SC_PeekMessageW,        // 031
	(PFNVOID)SC_ReleaseDC,           // 032
	(PFNVOID)SC_SendMessageW,        // 033
	(PFNVOID)SC_SetScrollInfo,       // 034
	(PFNVOID)SC_SetWindowLongW,      // 035
	(PFNVOID)SC_SetWindowPos,        // 036
	(PFNVOID)SC_CreateSolidBrush,    // 037
	(PFNVOID)SC_DeleteMenu,          // 038
	(PFNVOID)SC_DeleteObject,        // 039

	(PFNVOID)SC_DrawTextW,           // 040
	(PFNVOID)SC_ExtTextOutW,         // 041
	(PFNVOID)SC_FillRect,            // 042
	(PFNVOID)SC_GetAsyncKeyState,    // 043
	(PFNVOID)SC_GetDlgCtrlID,        // 044
	(PFNVOID)SC_GetStockObject,      // 045
	(PFNVOID)SC_GetSystemMetrics,    // 046
	(PFNVOID)SC_RegisterClassWStub,  // 047
	(PFNVOID)SC_RegisterClipboardFormatW, // 048
	(PFNVOID)SC_SetBkMode,           // 049

	(PFNVOID)SC_SetTextColor,        // 050
	(PFNVOID)SC_TransparentImage,    // 051
	(PFNVOID)SC_IsDialogMessageW,    // 052
	(PFNVOID)SC_PostMessageW,        // 053
	(PFNVOID)SC_IsWindowVisible,     // 054
	(PFNVOID)SC_GetKeyState,         // 055
	(PFNVOID)SC_BeginPaint,          // 056
	(PFNVOID)SC_EndPaint,            // 057
	(PFNVOID)SC_PerformCallBack4,    // 058
	(PFNVOID)SC_CeWriteRecordProps,  // 059

	(PFNVOID)SC_ReadFileWithSeek,    // 060
	(PFNVOID)SC_WriteFileWithSeek,   // 061
};


ERRFALSE(sizeof(ExtraMethods) == sizeof(ExtraMethods[0])*62);

#endif

