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
// @doc
// CommandQueue.h : Class for queueing asynchronous pipeline commands.
// Note: Instead of including this header file directly, include
//		 PipelineManagers.h.
//

#ifndef PIPELINEMANAGERS_H
#error "Must include PipelineManagers.h instead of CommandQueue.h"
#endif

//////////////////////////////////////////////////////////////////////////////
// @class CQueuedPipelineCommand |
// The individual element queued per asynchronous pipeline command execution
// request.
//////////////////////////////////////////////////////////////////////////////
class CQueuedPipelineCommand
{
public:
	CQueuedPipelineCommand();
	CQueuedPipelineCommand(
								const CQueuedPipelineCommand& rcCommand);
	CQueuedPipelineCommand(
								const CPipelineRouter& rcQueuingRouter,
								CPipelineRouter::SInvoker* psRouterInvoker);

	bool operator () ();
	bool DiscardWhileFlushing() const;

private:
	CPipelineRouter										m_cRouter;
	mutable std::auto_ptr<CPipelineRouter::SInvoker>	m_psRouterInvoker;
	  // pointer loses ownership when command is copied
};

//////////////////////////////////////////////////////////////////////////////
// @class CCommandQueue |
// The CCommandQueue class is used to hold commands, containing media samples
// or other filter in-bound or out-bound data. It is thread-safe for
// concurrent adding and deleting of commands. It cooperates with
// CAppToStmSync by releasing a thread waiting to pick up a command when the
// BeginFlush event is signalled, even if no pipeline command is present in
// the queue.
//////////////////////////////////////////////////////////////////////////////
class CCommandQueue
{
// @access Public Interface
public:
	// @cmember Constructor
	CCommandQueue(
								// @parm BeginFlush event handle
								HANDLE hFlushEvent);

	// @cmember Destructor
	~CCommandQueue();

	// @mfunc Command retrieval
	CQueuedPipelineCommand	GetQueueObject();

	// @mfunc Command addition; returns false while the queue is locked
	bool					PutQueueObject(
								// @parm Object to be copied and queued
								CQueuedPipelineCommand& rcCommand);

	// @mfunc Atomically set the flush event and stop accepting further
	// queue objects
	void					LockForFlush();

// @access Private Members
private:
	const HANDLE			m_hFlushEvent;
	const HANDLE			m_hCommandsAvailableSemaphore;
	CCritSec				m_cQueueLock;
	std::deque<CQueuedPipelineCommand> m_cQueue;

	CCommandQueue(const CCommandQueue&);					// not implemented
	CCommandQueue& operator = (const CCommandQueue&);		// not implemented
};
