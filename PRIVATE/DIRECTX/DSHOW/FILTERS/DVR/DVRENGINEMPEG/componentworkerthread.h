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
#pragma once

namespace MSDvr {

	// @interface CComponentWorkerThread | 
	// The class CComponentWorkerThread is designed to be the parent class
	// of all classes that provide a "main" for a streaming or application
	// thread. The class CComponentWorkerThread implements the mechanics
	// of creating and shutting down the thread. Each subclass is just
	// responsible for implementing ThreadMain() in a way that monitors
	// either an external stop event (m_hStopEvent) or a boolean data
	// member (m_fShutdownSignal). The creator of a CComponentWorkerThread
	// instance gets a reference to the object. A second reference is held
	// on behalf of the thread, for as long as the thread is running.
	// Class CComponentWorkerThread takes care of managing the reference
	// held on behalf of the thread and calling SetIsDone() when the
	// thread exits ThreadMain(). The class also handles the situation in
	// which ThreadMain() exits due to an exception.
	class CComponentWorkerThread
	{
	public:
		// @cmember
		// CComponentWorkerThread() sets up the data members but does
		// not actually start the thread.
		CComponentWorkerThread(HANDLE hStopEvent = NULL);

		// @cmember
		// StartThread() creates a thread and starts it running. If there
		// is a problem starting the thread, StartThread() will throw an
		// exception (typically E_FAIL).
		void StartThread();

		// @cmember
		// If the CComponentWorkerThread() was passed a non-null event, you should
		// signal that event instead of calling SignalThreadToStop(). ThreadMain()
		// should do waits on the stop event. If you do not pass a non-null event
		// to the constructor, call SignalThreadToStop(). ThreadMain() must, in this
		// usage pattern, monitor the protected data member m_fShutdownSignal to
		// decide when to exit. The value true means exit has been requested.
		void SignalThreadToStop();

		// @cmember
		// SleepUntilStopped() will not return until the worker thread has exited
		// ThreadMain(). 
		void SleepUntilStopped();

		// @cmember
		// IsDone() returns true if and only if the worker thread has either
		// exited ThreadMain() or has not yet been started.
		bool IsDone();

		// @cmember
		// AddRef() increments the reference count on the CComponentWorkerThread()
		// instance.
		long AddRef() { return InterlockedIncrement(&m_lRef); }

		// @cmember
		// Release() decrements the reference on the CComponentWorkerThread
		// instance. When the reference count drops to zero, the instance
		// is deleted. Use Release() instead of directly deleting th instance.
		long Release() { long lRef = InterlockedDecrement(&m_lRef); if (lRef == 0) delete this; return lRef; }

	protected:
		// @cmember
		// ~CComponentWorkerThread() assumes that the thread has already
		// been stopped. The destructor doesn't do much -- but subclasses
		// may add their own actions.
		virtual ~CComponentWorkerThread();

		// @cmember
		// ThreadMain() must be implemented -- this routine supplies the
		// "main" for the worker thread.
		virtual DWORD ThreadMain() = 0;

		// @cmember
		// OnThreadExit() is called by CComponentWorkerThread() whenever
		// ThreadMain() is left -- either by exception or normal return.
		// Subclasses can supply their own implementation if they need to
		// reliably implement some soft of cleanup before the CComponentWorkerThread
		// structure is torn down.
		virtual void OnThreadExit(DWORD &dwOutcome);

		// @cmember m_hThread is NULL if no thread is running. If a thread is
		// running, it is a handle to the thread.
		HANDLE m_hThread;

		// @cmember m_dwThreadID is 0 if no thread is running. If a thread is
		// running, it is the thread's identifier.
		DWORD m_dwThreadID;

		// @cmember m_hStopEvent is the event handle passed in to the constructor
		// of this CComponentWorker instance.
		HANDLE m_hStopEvent;

		// @cmember m_fShutdownSignal is true if and only if SignalThreadToStop()
		// has been called.
		bool m_fShutdownSignal;

	private:
		// @cmember
		// SetIsDone() is used internally to clear out the handles to the thread
		// handle and identifier.
		void SetIsDone();

		// @cmember
		// StreamingThreadProc() is the thread-main as far as Windows is concerned.
		// It forwards to ThreadMain() and oversees cleanup after ThreadMain()
		// exits.
		static DWORD WINAPI StreamingThreadProc(void *p);


		// @cmember m_lRef is the reference count of this CComponentWorker instance.
		long m_lRef;
	};

}
