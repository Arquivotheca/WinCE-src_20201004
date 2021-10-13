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
//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

/*-------------------------------------------------------------------------------


  Module Name:

	ft.h

  Description:
	Declare mini thread object

  -------------------------------------------------------------------------------*/
#include "SyncObj.h"
#ifndef MTHREAD_H
#define MTHREAD_H

class MiniThread  {

public :
	MiniThread (DWORD dwStackSize=0, int iPriority=THREAD_PRIORITY_NORMAL, BOOL bSuspended=FALSE);
	~MiniThread();
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
	CMutex syncObject;
private:
	virtual DWORD ThreadRun()=0;
	static DWORD WINAPI ThreadProc( LPVOID dParam);

	BOOL bTerminated;
	HANDLE threadHandle;
	DWORD  threadId;
	BOOL suspendFlag;
	DWORD exitCode;
};

#endif
