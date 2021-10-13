#ifndef _HELPERS_H

#ifdef __cplusplus
extern "C" {
#endif

#define	OK(x)	DebugDump(TEXT("Anchor #%i\n"), x);

#ifndef SECOND	
#define	SECOND	(1000)
#endif

#ifndef MINUTE	
#define	MINUTE	(60 * SECOND)
#endif

#ifndef HOUR	
#define	HOUR	(60 * MINUTE)
#endif

#define	MSGPUMP_LOOPCOUNT			(3)
#define	MSGPUMP_LOOPDELAY			(10)		
#define	MAXWAIT_TOIDLE				(20)
#define	MAX_IDLE_TIME				(200) // waitforidle thread max return time (msec)
#define	IDLE_REPEAT					(3) // number of waitforidle returns
#define	LOOP_DELAY					(25)
#define	KBHIT_DELAY					(100)
#define	BUF_SIZE					(0x400)

#define	DM_LEFTDOWN     		0x0001
#define	DM_LEFTUP       		0x0002
#define	DM_LEFTCLICK    		0x0004
#define	DM_RELATIVE     		0x0008
#define	DM_LEFTDBLCLICK			0x0010
#define	DM_NOMOVE           	0x0020
#define	DM_SECOND_LEFTCLICK		0x0040

#define	MOUSE_DELAY				(10) // millisecond

typedef DWORD (WINAPI *PTESTTHREADPROC) ();

BOOL _RWaitForSystemIdleEx(DWORD dwMaxWaitMilliSecs);
int DebugDump(const TCHAR *tszFormat, ...);
DWORD ExecuteTestThread(PTESTTHREADPROC pTestProc, DWORD dwTimeOut);
void FAR PASCAL MessagePump();

BOOL IsWindowAvailable(TCHAR *tszWinTitle, HWND hWnd);
BOOL WaitForWindow(LPTSTR lptszWindowName, int nMaxWait);
BOOL ClickOnWindow(HWND hWnd, TCHAR *tszWndTitle, TCHAR *tszWndClass);
HWND FindWindowEx(HWND hWnd, TCHAR *tszWndTitle, TCHAR *tszWndClass);
BOOL MouseSetRelativeWindow(HWND hWnd);
HWND MouseGetRelativeWindow(void);
BOOL Click(HWND hWnd);
BOOL DoMouse(DWORD dwFlags, DWORD dwX, DWORD dwY);
HANDLE GetProcessFromWindow(HWND hWnd);


#define DWaitForSystemIdle() _RWaitForSystemIdleEx(MAXWAIT_TOIDLE)

#ifdef __cplusplus
}
#endif

#define _HELPERS_H
#endif
