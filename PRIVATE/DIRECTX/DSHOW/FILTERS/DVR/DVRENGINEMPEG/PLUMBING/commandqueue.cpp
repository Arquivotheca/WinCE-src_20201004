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
// CommandQueue.h : Class for queueing asynchronous pipeline commands.
//

#include "stdafx.h"
#include "PipelineManagers.h"

#include "..\\HResultError.h"

namespace MSDvr
{

//////////////////////////////////////////////////////////////////////////////
//	CCommandQueue.															//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Construction and destruction.											//
//////////////////////////////////////////////////////////////////////////////

CCommandQueue::CCommandQueue(HANDLE hFlushEvent) :
	m_hFlushEvent					(hFlushEvent),
	m_hCommandsAvailableSemaphore	(CreateSemaphore(NULL, 0, LONG_MAX, NULL))
{
	ASSERT (hFlushEvent);

	if (! m_hCommandsAvailableSemaphore)
		throw std::bad_alloc("Command queue: failed to create semaphore");
}

CCommandQueue::~CCommandQueue()
{
	// Clean up the event created by this class
	EXECUTE_ASSERT (CloseHandle(m_hCommandsAvailableSemaphore));
}

//////////////////////////////////////////////////////////////////////////////
//	Implementation.															//
//////////////////////////////////////////////////////////////////////////////

CQueuedPipelineCommand CCommandQueue::GetQueueObject()
{
	// Wait against both handles
	const HANDLE rghHandles[] = { m_hCommandsAvailableSemaphore, m_hFlushEvent };
	const DWORD dwWaitResult = WaitForMultipleObjects(
		sizeof(rghHandles) / sizeof(rghHandles[0]), rghHandles, FALSE, INFINITE);

	if (WAIT_FAILED == dwWaitResult)
		throw CHResultError(HRESULT_FROM_WIN32(GetLastError()), "Command queue: get wait failed");

	// If no command is available, return by manufacturing the
	// null-command
	if (WAIT_OBJECT_0 + 1 == dwWaitResult)
		return CQueuedPipelineCommand();

	// If there is a command available, then return it, regardless of the
	// status of the flush event
	CAutoLock cLock(&m_cQueueLock);
	  // Protect container from both concurrent readers and writers

	CQueuedPipelineCommand cResult(m_cQueue.front());
	m_cQueue.pop_front();

	return cResult;
}

bool CCommandQueue::PutQueueObject(CQueuedPipelineCommand& rcCommand)
{
	// Protect from concurrent read and write access
	CAutoLock cLock(&m_cQueueLock);

	// Test whether we should be accepting queue objects now
	if (rcCommand.DiscardWhileFlushing())
	{
		const DWORD dwWaitResult = WaitForSingleObject(m_hFlushEvent, 0);

		if (WAIT_FAILED == dwWaitResult)
			throw CHResultError(HRESULT_FROM_WIN32(GetLastError()), "Command queue: put wait failed");

		if (WAIT_OBJECT_0 == dwWaitResult)
			return false;
	}

	// Copy the new command into a new queue slot
	m_cQueue.push_back(rcCommand);

	// No exception has occurred; we can therefore adjust the semaphore now
	EXECUTE_ASSERT (ReleaseSemaphore(m_hCommandsAvailableSemaphore, 1, NULL));
	return true;
}

void CCommandQueue::LockForFlush()
{
	// Ensure no command is in the process of being added now
	CAutoLock cLock(&m_cQueueLock);

	EXECUTE_ASSERT (SetEvent(m_hFlushEvent));
}

//////////////////////////////////////////////////////////////////////////////
//	CQueuedPipelineCommand.													//
//////////////////////////////////////////////////////////////////////////////

CQueuedPipelineCommand::CQueuedPipelineCommand()
{
}

CQueuedPipelineCommand::CQueuedPipelineCommand(
			const CQueuedPipelineCommand& rcCommand) :
	m_cRouter			(rcCommand.m_cRouter),
	m_psRouterInvoker	(rcCommand.m_psRouterInvoker)
{
}

CQueuedPipelineCommand::CQueuedPipelineCommand(
			const CPipelineRouter& rcQueuingRouter,
			CPipelineRouter::SInvoker* psRouterInvoker) :
	m_cRouter			(rcQueuingRouter),
	m_psRouterInvoker	(psRouterInvoker)
{
	ASSERT (psRouterInvoker);
}

bool CQueuedPipelineCommand::operator () ()
{
	// This is the asynchronous invocation on the queued object; initiate
	// routing across the pipeline now
	if (! m_psRouterInvoker.get())
		// Not having a router invoker is the flushing signal
		return false;

	m_cRouter.PipelineInvoke(*m_psRouterInvoker, true);

	return true;  // continue asynchronous invocation
}

bool CQueuedPipelineCommand::DiscardWhileFlushing() const
{
	// Don't discard the flushing signal
	if (! m_psRouterInvoker.get())
		return false;

	return m_psRouterInvoker->DiscardWhileFlushing();
}

}
