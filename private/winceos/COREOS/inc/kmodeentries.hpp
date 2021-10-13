//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef __KMODE_ENTRIES_HPP_INCLUDED__
#define __KMODE_ENTRIES_HPP_INCLUDED__


class KmodeEntries_t
{

public:


	typedef
	BOOL
	(*EventModify_t)(
		HANDLE	hEvent,
		DWORD	action
		);

	typedef
	BOOL
	(*ReleaseMutex_t)(
		HANDLE	hMutex
		);

	typedef
	HANDLE
	(*CreateAPIHandle_t)(
		HANDLE	hSet,
		LPVOID	pvData
		);


	typedef
	LPVOID
	(*MapViewOfFile_t)(
		HANDLE	hMap,
		DWORD	fdwAccess,
		DWORD	dwOffsetLow,
		DWORD	dwOffsetHigh,
		DWORD	cbMap
		);

	typedef
	int
	(*ThreadGetPrio_t)(
		HANDLE	hTh
		);

	typedef
	BOOL
	(*ThreadSetPrio_t)(
		HANDLE	hTh,
		DWORD	prio
		);


	typedef
	HGDIOBJ
	(*SelectObject_t)(
		HDC		hDC,
		HANDLE	hObj
		);


	typedef
	BOOL
	(*PatBlt_t)(
		HDC		hdcDest,
		int		nXLeft,
		int		nYLeft,
		int		nWidth,
		int		nHeight,
		DWORD	dwRop
		);

	typedef
	BOOL
	(*GetTextExtentExPointW_t)(
		HDC		hdc,
		LPCWSTR	lpszStr,
		int		cchString,
		int		nMaxExtent,
		LPINT	lpnFit,
		LPINT	alpDx,
		LPSIZE	lpSize
		);

	typedef
	HBRUSH
	(*GetSysColorBrush_t)(
		int	nIndex
		);

	typedef
	COLORREF
	(*SetBkColor_t)(
		HDC			hDC,
		COLORREF	dwColor
		);

	typedef
	HWND
	(*GetParent_t)(
		HWND	hwnd
		);

	typedef
	BOOL
	(*InvalidateRect_t)(
			HWND	hwnd,
	const	RECT*	prc,
			BOOL	fErase
		);

	typedef
	LONG
	(*NKRegOpenKeyExW_t)(
		HKEY	hKey,
		LPCWSTR	lpSubKey,
		DWORD	ulOptions,
		REGSAM	samDesired,
		PHKEY	phkResult
		);


	typedef
	LONG
	(*NKRegQueryValueExW_t)(
		HKEY	hKey,
		LPCWSTR	lpValueName,
		LPDWORD	lpReserved,
		LPDWORD	lpType,
		LPBYTE	lpData,
		LPDWORD	lpcbData
		);


	typedef
	HANDLE
	(*CreateFileW_t)(
		LPCWSTR					lpFileName,
		DWORD					dwDesiredAccess,
		DWORD					dwShareMode,
		LPSECURITY_ATTRIBUTES	lpSecurityAttributes,
		DWORD					dwCreationDisposition,
		DWORD					dwFlagsAndAttributes,
		HANDLE					hTemplateFile
		);

	typedef
	BOOL
	(*ReadFile_t)(
		HANDLE			hFile,
		LPVOID			lpBuffer,
		DWORD			nNumberOfBytesToRead,
		LPDWORD			lpNumberOfBytesRead,
		LPOVERLAPPED	lpOverlapped
		);

	typedef
	HANDLE
	(*OpenDatabaseEx_t)(
		PCEGUID				pguid,
		PCEOID				poid,
		LPWSTR				lpszName,
		SORTORDERSPECEX*	pSort,
		DWORD				dwFlags,
		CENOTIFYREQUEST*	pReq
		);

	typedef
	CEOID
	(*SeekDatabase_t)(
		HANDLE	hDatabase,
		DWORD	dwSeekType,
		DWORD	dwValue,
		WORD	wNumVals,
		LPDWORD	lpdwIndex
		);


	typedef
	CEOID
	(*ReadRecordPropsEx_t)(
		HANDLE		hDbase,
		DWORD		dwFlags,
		LPWORD		lpcPropID,
		CEPROPID*	rgPropID,
		LPBYTE*		lplpBuffer,
		LPDWORD		lpcbBuffer,
		HANDLE		hHeap
		);


	typedef
	LONG
	(*NKRegCreateKeyExW_t)(
		HKEY					hKey,
		LPCWSTR					lpSubKey,
		DWORD					Reserved,
		LPWSTR					lpClass,
		DWORD					dwOptions,
		REGSAM					samDesired,
		LPSECURITY_ATTRIBUTES	lpSecurityAttributes,
		PHKEY					phkResult,
		LPDWORD					lpdwDisposition
		);

	typedef
	BOOL
	(*DeviceIoControl_t)(
		HANDLE			hDevice,
		DWORD			dwIoControlCode,
		LPVOID			lpInBuf,
		DWORD			nInBufSize,
		LPVOID			lpOutBuf,
		DWORD			nOutBufSize,
		LPDWORD			lpBytesReturned,
		LPOVERLAPPED	lpOverlapped
		);


	typedef
	BOOL
	(*CloseHandle_t)(
		HANDLE	hObj
		);


	typedef
	LONG
	(*NKRegCloseKey_t)(
		HKEY	hKey
		);


	typedef
	BOOL
	(*ClientToScreen_t)(
		HWND	hwnd,
		LPPOINT	lpPoint
		);


	typedef
	LRESULT
	(*DefWindowProcW_t)(
		HWND	hwnd,
		UINT	msg,
		WPARAM	wParam,
		LPARAM	lParam
		);

	typedef
	BOOL
	(*GetClipCursor_t)(
		RECT*	lpRect
		);


	typedef
	HDC
	(*GetDC_t)(
		HWND	hwnd
		);

	typedef
	HWND
	(*GetFocus_t)(
		void
		);

	typedef
	BOOL
	(*GetMessageW_t)(
		MSG*	pMsgr,
		HWND	hwnd,
		UINT	wMsgFilterMin,
		UINT	wMsgFilterMax
		);


	typedef
	HWND
	(*GetWindow_t)(
		HWND	hwnd,
		UINT	uCmd
		);

	typedef
	BOOL
	(*PeekMessageW_t)(
		MSG*	pMsg,
		HWND	hWnd,
		UINT	wMsgFilterMin,
		UINT	wMsgFilterMax,
		UINT	wRemoveMsg
		);

	typedef
	BOOL
	(*ReleaseDC_t)(
		HWND	hwnd,
		HDC		hdc
		);

	typedef
	LRESULT
	(*SendMessageW_t)(
		HWND	hwnd,
		UINT	Msg,
		WPARAM	wParam,
		LPARAM	lParam
		);

	typedef
	int
	(*SetScrollInfo_t)(
			HWND		hwnd,
			int			fnBar,
	const	SCROLLINFO*	lpsi,
			BOOL		fRedraw
		);

	typedef
	LONG
	(*SetWindowLongW_t)(
		HWND	hwnd,
		int		nIndex,
		LONG	lNewLong
		);

	typedef
	BOOL
	(*SetWindowPos_t)(
		HWND hwnd,
		HWND hwndInsertAfter,
		int x,
		int y,
		int dx,
		int dy,
		UINT fuFlags
		);

	typedef
	HBRUSH
	(*CreateSolidBrush_t)(
		COLORREF crColor
		);

	typedef
	BOOL
	(*DeleteMenu_t)(
		HMENU hmenu,
		UINT uPosition,
		UINT uFlags
		);

	typedef
	BOOL
	(*DeleteObject_t)(
		HGDIOBJ hObject
		);

	typedef
	int
	(*DrawTextW_t)(
		HDC		hdc,
		LPCWSTR	lpszStr,
		int		cchStr,
		RECT*	lprc,
		UINT	wFormat
		);

	typedef
	BOOL
	(*ExtTextOutW_t)(
				HDC		hdc,
				int		X,
				int		Y,
				UINT	fuOptions,
		CONST	RECT*	lprc,
				LPCWSTR	lpszString,
				UINT	cbCount,
		CONST	INT*	lpDx
		);

	typedef
	int
	(*FillRect_t)(
				HDC		hdc,
		CONST	RECT*	lprc,
				HBRUSH	hbr
		);

	typedef
	SHORT
	(*GetAsyncKeyState_t)(
		INT	vKey
		);

	typedef
	int
	(*GetDlgCtrlID_t)(
		HWND	hWnd
		);

	typedef
	HGDIOBJ
	(*GetStockObject_t)(
		int	fnObject
		);

	typedef
	int
	(*GetSystemMetrics_t)(
		int	nIndex
		);

	typedef
	ATOM
	(*RegisterClassWStub_t)(
		const	WNDCLASSW*	lpWndClass,
		const	WCHAR*		lpszClassName,
				HANDLE		hprcWndProc
		);

	typedef
	UINT
	(*RegisterClipboardFormatW_t)(
		const	WCHAR*	lpszFormat
		);

	typedef
	int
	(*SetBkMode_t)(
		HDC	hdc,
		int	iBkMode
		);

	typedef
	COLORREF
	(*SetTextColor_t)(
		HDC			hdc,
		COLORREF	crColor
		);


	typedef
	BOOL
	(*TransparentImage_t)(
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
		);

	typedef
	BOOL
	(*IsDialogMessageW_t)(
		HWND	hDlg,
		MSG*	lpMsg
		);

	typedef
	BOOL
	(*PostMessageW_t)(
		HWND	hWnd,
		UINT	Msg,
		WPARAM	wParam,
		LPARAM	lParam
		);

	typedef
	BOOL
	(*IsWindowVisible_t)(
		HWND	hWnd
		);

	typedef
	SHORT
	(*GetKeyState_t)(
		int	nVirtKey
		);

	typedef
	HDC
	(*BeginPaint_t)(
		HWND			hwnd,
		PAINTSTRUCT*	pps
		);

	typedef
	BOOL
	(*EndPaint_t)(
		HWND			hwnd,
		PAINTSTRUCT*	pps
		);


	typedef
	DWORD
	(*PerformCallBack4_t)(
		CALLBACKINFO*	pcbi,
		...
		);


	typedef
	CEOID
	(*CeWriteRecordProps_t)(
		HANDLE		hDbase,
		CEOID		oidRecord,
		WORD		cPropID,
		CEPROPVAL*	rgPropVal
		);


	typedef
	BOOL
	(*ReadFileWithSeek_t)(
		HANDLE			hFile,
		LPVOID			lpBuffer,
		DWORD			nNumberOfBytesToRead,
		LPDWORD			lpNumberOfBytesRead,
		LPOVERLAPPED	lpOverlapped,
		DWORD			dwLowOffset,
		DWORD			dwHighOffset
		);

	typedef
	BOOL
	(*WriteFileWithSeek_t)(
		HANDLE			hFile,
		LPCVOID			lpBuffer,
		DWORD			nNumberOfBytesToWrite,
		LPDWORD			lpNumberOfBytesWritten,
		LPOVERLAPPED	lpOverlapped,
		DWORD			dwLowOffset,
		DWORD			dwHighOffset
		);



	EventModify_t			m_pEventModify;				//	0
	ReleaseMutex_t			m_pReleaseMutex;
	CreateAPIHandle_t		m_pCreateAPIHandle;
	MapViewOfFile_t			m_pMapViewOfFile;
	ThreadGetPrio_t     	m_pThreadGetPrio;
	ThreadSetPrio_t			m_pThreadSetPrio;
	SelectObject_t      	m_pSelectObject;
	PatBlt_t            	m_pPatBlt;
	GetTextExtentExPointW_t	m_pGetTextExtentExPointW;
	GetSysColorBrush_t		m_pGetSysColorBrush;		//	9

	SetBkColor_t            m_pSetBkColor;				// 010
    GetParent_t             m_pGetParent;
	InvalidateRect_t		m_pInvalidateRect;
	NKRegOpenKeyExW_t		m_pNKRegOpenKeyExW;
	NKRegQueryValueExW_t    m_pNKRegQueryValueExW;
	CreateFileW_t           m_pCreateFileW;
	ReadFile_t				m_pReadFile;
	OpenDatabaseEx_t        m_pOpenDatabaseEx;
	SeekDatabase_t          m_pSeekDatabase;
	ReadRecordPropsEx_t     m_pReadRecordPropsEx;		// 019

	NKRegCreateKeyExW_t     m_pNKRegCreateKeyExW;		// 020
	DeviceIoControl_t       m_pDeviceIoControl;
	CloseHandle_t           m_pCloseHandle;
	NKRegCloseKey_t         m_pNKRegCloseKey;
	ClientToScreen_t        m_pClientToScreen;
	DefWindowProcW_t        m_pDefWindowProcW;
	GetClipCursor_t         m_pGetClipCursor;
	GetDC_t                 m_pGetDC;
	GetFocus_t              m_pGetFocus;
	GetMessageW_t           m_pGetMessageW;				// 029

	GetWindow_t             m_pGetWindow;				// 030
	PeekMessageW_t          m_pPeekMessageW;
	ReleaseDC_t             m_pReleaseDC;
	SendMessageW_t          m_pSendMessageW;
	SetScrollInfo_t         m_pSetScrollInfo;
	SetWindowLongW_t        m_pSetWindowLongW;
	SetWindowPos_t          m_pSetWindowPos;
	CreateSolidBrush_t      m_pCreateSolidBrush;
	DeleteMenu_t            m_pDeleteMenu;
	DeleteObject_t          m_pDeleteObject;			// 039

	DrawTextW_t					m_pDrawTextW;			//	40
	ExtTextOutW_t           	m_pExtTextOutW;
	FillRect_t					m_pFillRect;
	GetAsyncKeyState_t      	m_pGetAsyncKeyState;
	GetDlgCtrlID_t          	m_pGetDlgCtrlID;
	GetStockObject_t        	m_pGetStockObject;
	GetSystemMetrics_t      	m_pGetSystemMetrics;
	RegisterClassWStub_t    	m_pRegisterClassWStub;
	RegisterClipboardFormatW_t  m_pRegisterClipboardFormatW;
	SetBkMode_t                 m_pSetBkMode;			//	049

	SetTextColor_t              m_pSetTextColor;		//	050
	TransparentImage_t          m_pTransparentImage;
	IsDialogMessageW_t          m_pIsDialogMessageW;
	PostMessageW_t              m_pPostMessageW;
	IsWindowVisible_t           m_pIsWindowVisible;
	GetKeyState_t               m_pGetKeyState;
	BeginPaint_t                m_pBeginPaint;
	EndPaint_t                  m_pEndPaint;
	PerformCallBack4_t          m_pPerformCallBack4;
	CeWriteRecordProps_t        m_pCeWriteRecordProps;	//	059

	ReadFileWithSeek_t          m_pReadFileWithSeek;	//	060
	WriteFileWithSeek_t         m_pWriteFileWithSeek;



};


#endif

