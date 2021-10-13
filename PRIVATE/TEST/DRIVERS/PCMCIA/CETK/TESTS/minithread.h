//******************************************************************************
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************
  
#ifndef _MTHREAD_H_
#define _MTHREAD_H_

class MiniThread  {

public :
	MiniThread (DWORD dwStackSize=0, int iPriority=THREAD_PRIORITY_NORMAL, BOOL bSuspended=FALSE);
	virtual ~MiniThread();
	BOOL ThreadStart();
	BOOL ThreadStop();
	BOOL ThreadTerminated(DWORD dwMilliSeconds);
	BOOL WaitThreadComplete(DWORD dwMilliSeconds);
	BOOL ForceTerminated();

	DWORD GetThreadId() { return threadId; };
	BOOL IsTerminated() { return bTerminated; };
	HANDLE GetThreadHandle() { return threadHandle; };
	BOOL GetExitCodeThread(LPDWORD lpExitCode) {
		if (!threadHandle) {
			*lpExitCode=exitCode;
			return TRUE;
		}
		else
			return FALSE;
	};

	void SetResult(BOOL bResult){bRetVal = bResult;};
	BOOL GetResult(){return bRetVal;};

private:
	virtual DWORD ThreadRun()=0;
	static DWORD WINAPI ThreadProc( LPVOID dParam);
	BOOL	bRetVal;
	BOOL bTerminated;
	HANDLE threadHandle;
	DWORD  threadId;
	BOOL suspendFlag;
	DWORD exitCode;
};

#endif

