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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include <stdafx.h>
#include "ComponentWorkerThread.h"
#include "HResultError.h"

using namespace MSDvr;

CComponentWorkerThread::CComponentWorkerThread(HANDLE hStopEvent)
: m_lRef(1)
, m_hThread(0)
, m_hStopEvent(hStopEvent)
, m_dwThreadID(0)
, m_fShutdownSignal(false)
{
} // CComponentWorkerThread::CComponentWorkerThread

CComponentWorkerThread::~CComponentWorkerThread()
{
	ASSERT(m_lRef == 0);
	ASSERT(m_hThread == 0);
	ASSERT(m_dwThreadID == 0);
} // CComponentWorkerThread::~CComponentWorkerThread

void CComponentWorkerThread::StartThread()
{
	// Heads up:  parental control needs to be able to run a sequence of
	// actions in a fashion that is effectively atomic with respect to
	// a/v reaching the renderers. Right now, parental control is doing
	// so by raising the priority of the thread in PC to a registry-supplied
	// value (PAL priority 1 = CE priority 249). If you change the code
	// here to raise the priority of the DVR engine playback graph 
	// streaming threads, be sure to check that either these threads or
	// the DVR splitter output pin threads are still running at a priority
	// less urgent than parental control.

	if (m_hThread)
		throw CHResultError(E_FAIL);

	m_fShutdownSignal = false;
	AddRef();
	try {
		m_hThread = CreateThread(	NULL,
								0,
								StreamingThreadProc,
								this,
								0,
								&m_dwThreadID);
	}
	catch (const std::exception&)
	{
		m_hThread = NULL;
	}

	if (!m_hThread)
	{
		// Problem!  We should be able to create a new thread.
		ASSERT(FALSE);
		SetIsDone();
		throw CHResultError(E_FAIL);
	}
} // CComponentWorkerThread::StartThread

void CComponentWorkerThread::SignalThreadToStop()
{
	m_fShutdownSignal = true;
} // CComponentWorkerThread::SignalThreadToStop

void CComponentWorkerThread::SleepUntilStopped()
{
	while (m_hThread != NULL)
	{
		Sleep(100);
	}
} // CComponentWorkerThread::BlockUntilStopped

bool CComponentWorkerThread::IsDone()
{
	return (m_hThread == NULL);
} // CComponentWorkerThread::IsDone

void CComponentWorkerThread::SetIsDone()
{
	if (m_hThread != NULL)
		CloseHandle(m_hThread);
	m_hThread = NULL;
	m_dwThreadID = 0;
	Release();
} // CComponentWorkerThread::SetIsDone

void CComponentWorkerThread::OnThreadExit(DWORD &dwOutcome)
{
} // CComponentWorkerThread::OnThreadExit

DWORD WINAPI CComponentWorkerThread::StreamingThreadProc(void *p)
{
	DWORD dwOutcome = 0;

	CComponentWorkerThread *pcComponentWorkerThread =
		static_cast<CComponentWorkerThread*>(p);

	try {
		dwOutcome = pcComponentWorkerThread->ThreadMain();
	} 
	catch (const std::exception&)
	{
		dwOutcome = E_FAIL;
	};
	try {
		pcComponentWorkerThread->OnThreadExit(dwOutcome);
	}
	catch (const std::exception& )
	{
	};
	try {
		pcComponentWorkerThread->SetIsDone();
	}
	catch (const std::exception& )
	{
	};
	return dwOutcome;
} // CComponentWorkerThread::StreamingThreadProc
