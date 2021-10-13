//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __WND_H__


#define	DEFAULT_WNDCLS		_T("default_wnd_t_wndcls")
#define	SECOND				(1000)
#define	MINUTE				(60 * SECOND)
#define	HOUR				(60 * MINUTE)
#define	MAX_WAIT			(INFINITE)
#define	DebugDump			(NKDbgPrintfW)
#define	MSG_EXITTHRD		(WM_USER)
#define	RECT_WIDTH(rc)		(abs((LONG)rc.right - (LONG)rc.left))
#define	RECT_HEIGHT(rc)		(abs((LONG)rc.bottom - (LONG)rc.top))


/* WIN_DATA is used to save this pointer that it can be retrieved it in the
WNDPROC. Use this struct so that GWL_USERDATA can still be used for other
things. */

class _wnd_t;

typedef struct _THREADINFO_CW_DATA
{
	HANDLE hCreated;
	HANDLE hParentThread;
	HANDLE hThread;
	HWND hParentWnd;
	DWORD dwExStyle;
	DWORD dwStyle;
	TCHAR szWndTitle[MAX_PATH];
	RECT rc;
	_wnd_t *pwnd;
	_THREADINFO_CW_DATA() { memset(this, 0, sizeof THREADINFO_CW_DATA); }
} THREADINFO_CW_DATA, *PTHREADINFO_CW_DATA;


class _wnd_t
{
	#ifdef NEVER
	struct _critical_section_t
	{
		LPCRITICAL_SECTION _lpcs;

		_critical_section_t(LPCRITICAL_SECTION lpCriticalSection)
		{
			_lpcs = lpCriticalSection;
			EnterCriticalSection(_lpcs);
		}

		~_critical_section_t()
		{
			LeaveCriticalSection(_lpcs);
		}
	};

	CRITICAL_SECTION _cs;
	#endif /* NEVER */

	HANDLE _hThread;
	DWORD _dwThreadID;

	HINSTANCE _hInstance;

	HWND _hWnd;
	TCHAR _szWndCls[MAX_PATH];
	COLORREF _crBackground;

public:
    _wnd_t(HINSTANCE hInstance, LPTSTR lpszWndTitle = _T(""), DWORD dwStyle = WS_OVERLAPPED,
    	DWORD dwExStyle = NULL, HWND hParentWnd = NULL, COLORREF crBack= 0xFFFFFF,
		LPTSTR lpszWndCls = NULL, BOOL fThread = FALSE, LPRECT lprc = NULL);
	~_wnd_t();

    inline LONG GetStyle() { return GetWindowLong(_hWnd, GWL_STYLE); }
    inline LONG GetExStyle() { return GetWindowLong(_hWnd, GWL_EXSTYLE); }
	inline LONG SetStyle(DWORD dwStyle) { return SetWindowLong(_hWnd, GWL_STYLE, dwStyle); }
	inline LONG SetExStyle(DWORD dwExStyle) { return SetWindowLong(_hWnd, GWL_EXSTYLE, dwExStyle); }
	inline LONG AddStyle(DWORD dwStyle) { return SetStyle(dwStyle | GetStyle()); }
	inline LONG AddExStyle(DWORD dwExStyle) { return SetExStyle(dwExStyle | GetExStyle()); }
	inline LONG RemoveStyle(DWORD dwStyle) { return SetStyle(dwStyle ^ GetStyle()); }
	inline LONG RemoveExStyle(DWORD dwExStyle) { return SetStyle(dwExStyle ^ GetExStyle()); }

	inline HWND hwnd() { return _hWnd; }
	inline HWND Attach(HWND hWnd) { return (_hThread || _hWnd) ? NULL : (_hWnd = hWnd); }

	inline BOOL Destroy() { return DestroyWindow(_hWnd); }
    inline BOOL IsTopMost() { return GetWindowLong(_hWnd, GWL_EXSTYLE) & WS_EX_TOPMOST ? TRUE : FALSE; }
	inline BOOL Foreground() { return ::SetForegroundWindow(_hWnd); }
	VOID Refresh(BOOL fEraseBackground = FALSE);
	inline BOOL Show(INT nCmdShow = SW_SHOWNORMAL) { return ShowWindow(_hWnd, nCmdShow); }
	inline BOOL Exists() { return IsWindow(_hWnd); }

	VOID MessagePump(HWND hWnd = NULL);
	LONG SetWndProc(WNDPROC pWndProc) { return SetWindowLong(_hWnd, GWL_WNDPROC, (LONG)pWndProc); }
	BOOL CatchThreadMessage(MSG *pmsg);

	inline HANDLE GetThreadHandle() { return _hThread; }
	inline DWORD GetThreadID() { return _dwThreadID; }
	DWORD CloseThread();

protected:
	friend LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	friend DWORD WINAPI ThreadProc(LPVOID lpParameter);

private:
	BOOL RegisterClass();
	static INT _nObjCount;
private:

};


#define __WND_H__
#endif /* __WND_H__ */
