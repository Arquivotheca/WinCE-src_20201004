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
// PipelineManagers.cpp : Base classes for capture and playback pipeline
// managers.
//

#include "stdafx.h"
#include "PipelineManagers.h"
#include "Sink.h"
#include "Source.h"
#include "DVREngine.h"

namespace MSDvr
{

//////////////////////////////////////////////////////////////////////////////
//	SGuidCompare.															//
//////////////////////////////////////////////////////////////////////////////

bool SGuidCompare::operator () (REFGUID arg1, REFGUID arg2) const
{
	return	arg1.Data1 != arg2.Data1 ? arg1.Data1 < arg2.Data1 :
			arg1.Data2 != arg2.Data2 ? arg1.Data2 < arg2.Data2 :
			arg1.Data3 != arg2.Data3 ? arg1.Data3 < arg2.Data3 :
			arg1.Data4[0] != arg2.Data4[0] ? arg1.Data4[0] < arg2.Data4[0] :
			arg1.Data4[1] != arg2.Data4[1] ? arg1.Data4[1] < arg2.Data4[1] :
			arg1.Data4[2] != arg2.Data4[2] ? arg1.Data4[2] < arg2.Data4[2] :
			arg1.Data4[3] != arg2.Data4[3] ? arg1.Data4[3] < arg2.Data4[3] :
			arg1.Data4[4] != arg2.Data4[4] ? arg1.Data4[4] < arg2.Data4[4] :
			arg1.Data4[5] != arg2.Data4[5] ? arg1.Data4[5] < arg2.Data4[5] :
			arg1.Data4[6] != arg2.Data4[6] ? arg1.Data4[6] < arg2.Data4[6] :
			arg1.Data4[7] < arg2.Data4[7];
}

//////////////////////////////////////////////////////////////////////////////
//	SPinGuidCompare.															//
//////////////////////////////////////////////////////////////////////////////

bool SPinGuidCompare::operator () (SInputPinInterface arg1, SInputPinInterface arg2) const
{
	return	(&arg1.riPin == &arg2.riPin) &&
			arg1.iid.Data1 != arg2.iid.Data1 ? arg1.iid.Data1 < arg2.iid.Data1 :
			arg1.iid.Data2 != arg2.iid.Data2 ? arg1.iid.Data2 < arg2.iid.Data2 :
			arg1.iid.Data3 != arg2.iid.Data3 ? arg1.iid.Data3 < arg2.iid.Data3 :
			arg1.iid.Data4[0] != arg2.iid.Data4[0] ? arg1.iid.Data4[0] < arg2.iid.Data4[0] :
			arg1.iid.Data4[1] != arg2.iid.Data4[1] ? arg1.iid.Data4[1] < arg2.iid.Data4[1] :
			arg1.iid.Data4[2] != arg2.iid.Data4[2] ? arg1.iid.Data4[2] < arg2.iid.Data4[2] :
			arg1.iid.Data4[3] != arg2.iid.Data4[3] ? arg1.iid.Data4[3] < arg2.iid.Data4[3] :
			arg1.iid.Data4[4] != arg2.iid.Data4[4] ? arg1.iid.Data4[4] < arg2.iid.Data4[4] :
			arg1.iid.Data4[5] != arg2.iid.Data4[5] ? arg1.iid.Data4[5] < arg2.iid.Data4[5] :
			arg1.iid.Data4[6] != arg2.iid.Data4[6] ? arg1.iid.Data4[6] < arg2.iid.Data4[6] :
			arg1.iid.Data4[7] < arg2.iid.Data4[7];
}

//////////////////////////////////////////////////////////////////////////////
//	Filter state assertion.													//
//////////////////////////////////////////////////////////////////////////////

static void AssertFilterLock(CBaseFilter& rcFilter)
{
#ifdef DEBUG
	FILTER_INFO sFilterInfo;
	EXECUTE_ASSERT (SUCCEEDED(rcFilter.QueryFilterInfo(&sFilterInfo)));
	if (sFilterInfo.pGraph)
		sFilterInfo.pGraph->Release();  // we don't need this

	if (wcscmp(sFilterInfo.achName, CDVRSourceFilter::s_wzName))
		static_cast <CDVRSinkFilter&> (rcFilter).AssertFilterLock();
	else
		static_cast <CDVRSourceFilter&> (rcFilter).AssertFilterLock();
#else // DEBUG
	UNUSED (rcFilter);
#endif  // DEBUG
}

//////////////////////////////////////////////////////////////////////////////
//	CBasePipelineManager.													//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Instance creation and destruction.										//
//////////////////////////////////////////////////////////////////////////////

CBasePipelineManager::CBasePipelineManager(CBaseFilter& rcFilter) :
	m_rcFilter					( rcFilter ),
	m_bLastNotifiedComponent	( 0 ),
	m_hStreamingThread			( NULL ),
	m_hBeginFlushEvent			(TRUE, FALSE),
	m_hEndFlushEvent			(TRUE, TRUE),
	m_wMacStreamingThreads		( 1 ),
	m_hThreadSyncEvent			(TRUE, FALSE),
	m_cQueuedCommands			( m_hBeginFlushEvent )
{
	if (! m_hBeginFlushEvent || ! m_hEndFlushEvent || ! m_hThreadSyncEvent)
	{
		throw std::bad_alloc("Pipeline manager: failed to create flush event");
	}
}

CBasePipelineManager::~CBasePipelineManager()
{
	// Clean up the pipeline, whatever state it may be in (including
	// partial notification state)
	ClearPipeline();
}

//////////////////////////////////////////////////////////////////////////////
//	IPipelineManager interface implementation.								//
//////////////////////////////////////////////////////////////////////////////

CBaseFilter& CBasePipelineManager::GetFilter()
{
	return m_rcFilter;
}

IUnknown* CBasePipelineManager::RegisterCOMInterface(CPipelineUnknown& rcPipUnk)
{
	const IID& riid = rcPipUnk.GetRegistrationIID();

	ASSERT (riid != GUID_NULL);
	ASSERT (! rcPipUnk.m_pcUnknown || rcPipUnk.m_pcUnknown == &m_rcFilter);

	t_COMRegistrants::iterator cItfPos;
	IUnknown* const piPriorRegistrant =
		(cItfPos = m_cCOMRegistrants.find(riid)) == m_cCOMRegistrants.end() ?
		NULL : &cItfPos->second->GetRegistrationInterface();

	// Insertion into the map may throw an exception
	m_cCOMRegistrants[riid] = &rcPipUnk;

	// Now that the insertion has succeeded, record the binding of this
	// interface implementation to this filter object
	rcPipUnk.m_pcUnknown = &m_rcFilter;

	// Return the prior registrant, or NULL if this is the first one
	return piPriorRegistrant;
}

void CBasePipelineManager::NonDelegatingQueryInterface(REFIID riid, LPVOID& rpv)
{
	ASSERT (riid != GUID_NULL);

	// Look up the interface id
	t_COMRegistrants::iterator cItfPos = m_cCOMRegistrants.find(riid);

	// Reference count, and return
	if (cItfPos == m_cCOMRegistrants.end())
		rpv = NULL;
	else
		GetInterface(&cItfPos->second->GetRegistrationInterface(), &rpv);
}

CPipelineRouter CBasePipelineManager::GetRouter(IPipelineComponent* piStart,
												bool fUpstream, bool fAsync)
{
	return CPipelineRouter(m_cPipelineComponents,
						   fAsync ? &m_cQueuedCommands : NULL,
						   piStart, fUpstream);
}

HANDLE CBasePipelineManager::GetFlushEvent()
{
	return m_hBeginFlushEvent;
}

bool CBasePipelineManager::WaitEndFlush()
{
	// Ensure we're expecting this call
	ASSERT (WAIT_OBJECT_0 == WaitForSingleObject(m_hBeginFlushEvent, 0));
	ASSERT (WAIT_TIMEOUT == WaitForSingleObject(m_hEndFlushEvent, 0));

	// Access filter state before it might change
	FILTER_STATE State;
	EXECUTE_ASSERT (SUCCEEDED(m_rcFilter.GetState(0, &State)));
	DbgLog((ZONE_SOURCE_STATE, 3, _T("CBasePipelineManager::WaitEndFlush() called in state %d\n"), State));

	// Register this caller
	if (! InterlockedDecrement(&m_lThreadsToSync))
	{
		EXECUTE_ASSERT (SetEvent(m_hThreadSyncEvent));
		DbgLog((ZONE_SOURCE_STATE, 3, _T("CBasePipelineManager::WaitEndFlush() set event m_hThreadSyncEvent\n")));
	}

	// Terminate this thread when stopping
	if (State_Stopped == State)
		return false;

	// Otherwise wait until the sync period ends
	EXECUTE_ASSERT (WAIT_OBJECT_0 == WaitForSingleObject(m_hEndFlushEvent, INFINITE));

	ASSERT (WAIT_TIMEOUT == WaitForSingleObject(m_hThreadSyncEvent, 0));

	// Register this thread's exiting from synchronization
	if (! InterlockedDecrement(&m_lThreadsToSync))
		EXECUTE_ASSERT (SetEvent(m_hThreadSyncEvent));

	return true;
}

void CBasePipelineManager::StartSync()
{
	// Must only be called by an application thread...
	AssertFilterLock(m_rcFilter);

	// Check that we're not already syncing
	if (WAIT_OBJECT_0 == WaitForSingleObject(m_hBeginFlushEvent, 0))
		throw std::logic_error("Sync already in progress");

	DbgLog((ZONE_SOURCE_STATE, 3, _T("CBasePipelineManager::StartSync()\n")));

	// Set number of threads we will wait for
	m_lThreadsToSync = m_wMacStreamingThreads;

	// Prepare for threads calling into WaitEndFlush and hold them
	EXECUTE_ASSERT (ResetEvent(m_hThreadSyncEvent));
	EXECUTE_ASSERT (ResetEvent(m_hEndFlushEvent));

	// Stop accepting new commands, and direct streaming threads to WaitEndFlush
	m_cQueuedCommands.LockForFlush();

	// Unblock streaming threads stuck in downstream filters
	const int lNumPins = m_rcFilter.GetPinCount();
	for (int lIndex = 0; lIndex < lNumPins; ++lIndex)
	{
		CBasePin& rcCurPin = *m_rcFilter.GetPin(lIndex);
		PIN_INFO sPinInfo;
		EXECUTE_ASSERT (SUCCEEDED(rcCurPin.QueryPinInfo(&sPinInfo)));
		if (sPinInfo.pFilter)
			sPinInfo.pFilter->Release();  // we don't need this

		if (PINDIR_OUTPUT == sPinInfo.dir)
			static_cast <CBaseOutputPin&> (rcCurPin).DeliverBeginFlush();
	}

	// Wait until all streaming threads are synchronized
	EXECUTE_ASSERT (WAIT_OBJECT_0 == WaitForSingleObject(m_hThreadSyncEvent, INFINITE));
	  // if this wait fails, we have big problems
}

void CBasePipelineManager::EndSync()
{
	// Must only be called by an application thread...
	AssertFilterLock(m_rcFilter);
	ASSERT (! m_lThreadsToSync);

	DbgLog((ZONE_SOURCE_STATE, 3, _T("CBasePipelineManager::EndSync()\n")));


	// Check that we're syncing
	if (WAIT_OBJECT_0 != WaitForSingleObject(m_hBeginFlushEvent, 0))
		throw std::logic_error("Sync not in progress");

	// Notify downstream filters that flushing is ending
	const int lNumPins = m_rcFilter.GetPinCount();
	for (int lIndex = 0; lIndex < lNumPins; ++lIndex)
	{
		CBasePin& rcCurPin = *m_rcFilter.GetPin(lIndex);
		PIN_INFO sPinInfo;
		EXECUTE_ASSERT (SUCCEEDED(rcCurPin.QueryPinInfo(&sPinInfo)));
		if (sPinInfo.pFilter)
			sPinInfo.pFilter->Release();  // we don't need this

		if (PINDIR_OUTPUT == sPinInfo.dir)
			static_cast <CBaseOutputPin&> (rcCurPin).DeliverEndFlush();
	}

	// Clear flushing state: first ensure that released threads don't end up
	// in WaitEndFlush again
	EXECUTE_ASSERT (ResetEvent(m_hBeginFlushEvent));

	// Now release blocked streaming threads and allow command queue to accept
	// samples again
	m_lThreadsToSync = m_wMacStreamingThreads;
	EXECUTE_ASSERT (ResetEvent(m_hThreadSyncEvent));
	EXECUTE_ASSERT (SetEvent(m_hEndFlushEvent));

	// Wait until all streaming threads have exited from WaitEndFlush, and thus
	// things are safe for doing another synchronization cycle
	FILTER_STATE State;
	EXECUTE_ASSERT (SUCCEEDED(m_rcFilter.GetState(0, &State)));

	DbgLog((ZONE_SOURCE_STATE, 3, _T("CBasePipelineManager::EndSync() finishing in state %d\n"), State));
	if (State_Stopped != State)
		EXECUTE_ASSERT (WAIT_OBJECT_0 == WaitForSingleObject(m_hThreadSyncEvent, INFINITE));
}

void CBasePipelineManager::AppendPipelineComponent(IPipelineComponent& riComponent,
												   bool fContainerToOwn)
{
	SPipelineElement sNewElement = { &riComponent, fContainerToOwn };

	// Adding a new node may throw an exception
	m_cPipelineComponents.push_back(sNewElement);
}

void CBasePipelineManager::TakeHeadOwnership()
{
	ASSERT (m_cPipelineComponents.size());

	m_cPipelineComponents.front().m_fContainerIsOwner = true;
}

void CBasePipelineManager::NotifyAllAddedComponents(
		bool fCapture, IPipelineManager& riPipelineManager)
{
	for (t_PipelineComponents::iterator Iterator = m_cPipelineComponents.begin();
		 Iterator != m_cPipelineComponents.end();
		 ++m_bLastNotifiedComponent, ++Iterator)

		if (fCapture)
			m_wMacStreamingThreads +=
			static_cast <ICapturePipelineComponent*> (Iterator->m_piComponent)
				->AddToPipeline(static_cast <ICapturePipelineManager&>
								(riPipelineManager));
		else
			m_wMacStreamingThreads +=
			static_cast <IPlaybackPipelineComponent*> (Iterator->m_piComponent)
				->AddToPipeline(static_cast <IPlaybackPipelineManager&>
								(riPipelineManager));
}

void CBasePipelineManager::ClearPipeline()
{
	unsigned char bIndex;
	t_PipelineComponents::iterator Iterator;

	// Distribute removal notifications first
	for (Iterator = m_cPipelineComponents.begin(), bIndex = 0;
		 bIndex < m_bLastNotifiedComponent;
		 ++bIndex, ++Iterator)

		Iterator->m_piComponent->RemoveFromPipeline();

	m_bLastNotifiedComponent = 0;  // reset

	// Delete those components that we own
	for (Iterator = m_cPipelineComponents.begin();
		 Iterator != m_cPipelineComponents.end();
		 ++Iterator)

		if (Iterator->m_fContainerIsOwner)
			delete Iterator->m_piComponent;

	// List is now clear
	m_cPipelineComponents.clear();

	// COM registrants have just been cleared
	m_cCOMRegistrants.clear();

	// No more component streaming threads exist
	m_wMacStreamingThreads = 1;  // the worker thread
}

//////////////////////////////////////////////////////////////////////////////
//	Streaming thread routines.												//
//////////////////////////////////////////////////////////////////////////////

void CBasePipelineManager::StartStreamingThread()
{
	ASSERT (! m_hStreamingThread);

	m_hStreamingThread = CreateThread(NULL, 0, GlobalStreamingThreadEntry,
									  this, 0, NULL);
	BOOL fResult = CeSetThreadPriority(m_hStreamingThread,g_dwStreamingThreadPri);
	ASSERT(fResult);

	if (! m_hStreamingThread)
		throw std::bad_alloc("Streaming thread creation failure");
}

void CBasePipelineManager::WaitStreamingThread()
{
	ASSERT (m_hStreamingThread);

	// Wait for the thread to exit
	EXECUTE_ASSERT (WAIT_OBJECT_0 == WaitForSingleObject(m_hStreamingThread, INFINITE));

	// Close out the thread handle
	EXECUTE_ASSERT (CloseHandle(m_hStreamingThread));
	m_hStreamingThread = NULL;
}

DWORD WINAPI
CBasePipelineManager::GlobalStreamingThreadEntry(LPVOID pvBasePipelineManager)
{
	ASSERT (pvBasePipelineManager);
	return reinterpret_cast <CBasePipelineManager*>
		(pvBasePipelineManager)->StreamingThreadEntry();
}

unsigned int CBasePipelineManager::StreamingThreadEntry()
{
	DbgLog((ZONE_ENGINE_OTHER, 3, _T("Pipeline manager: the worker thread is starting\n")));

	// Loop and service the queue until an exit marker is encountered
	for (;;)
	{
		// The call to retrieve the next queued command will block until
		// there is at least one command to retrieve
		CQueuedPipelineCommand cLoopCommand(m_cQueuedCommands.GetQueueObject());

		// While flushing, some commands need to be discarded
		const DWORD dwWaitResult = WaitForSingleObject(m_hBeginFlushEvent, 0);
		if (WAIT_FAILED == dwWaitResult)
		{
			ASSERT (FALSE);
			continue;
		}

		if (WAIT_OBJECT_0 == dwWaitResult
		&&	cLoopCommand.DiscardWhileFlushing())
		{
			DbgLog((ZONE_ENGINE_OTHER, 3, _T("Pipeline manager: discarding a command while flushing\n")));
			continue;
		}

		// Invoke the command
		try
		{
			if (! cLoopCommand() && ! WaitEndFlush())
				break;
		}
		catch (const std::exception& rcException)
		{
			UNUSED (rcException);  // suppress release build warning
#ifdef UNICODE
			DbgLog((LOG_ERROR, 1, 
					_T("The base pipeline manager's worker thread has encountered an exception: %S\n"),
					rcException.what()));
#else
			DbgLog((LOG_ERROR, 1, 
					_T("The base pipeline manager's worker thread has encountered an exception: %s\n"),
					rcException.what()));
#endif
		}

		// cLoopCommand destructor will now release invoker memory
	}

	DbgLog((ZONE_ENGINE_OTHER, 3, _T("Pipeline manager: the worker thread is exiting\n")));
	return 0;
}

//////////////////////////////////////////////////////////////////////////////
//	CCapturePipelineManager.												//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Instance creation.														//
//////////////////////////////////////////////////////////////////////////////

CCapturePipelineManager::CCapturePipelineManager(CDVRSinkFilter& rcFilter) :
	m_cBasePipelineManager		(rcFilter)
{
}

//////////////////////////////////////////////////////////////////////////////
//	IPipelineManager interface implementation.								//
//////////////////////////////////////////////////////////////////////////////

CBaseFilter& CCapturePipelineManager::GetFilter()
{
	return m_cBasePipelineManager.GetFilter();
}

IUnknown* CCapturePipelineManager::RegisterCOMInterface(
			CPipelineUnknown& rcPipUnk)
{
	return m_cBasePipelineManager.RegisterCOMInterface(rcPipUnk);
}

void CCapturePipelineManager::NonDelegatingQueryInterface(
			REFIID riid, LPVOID& rpv)
{
	m_cBasePipelineManager.NonDelegatingQueryInterface(riid, rpv);
}

void CCapturePipelineManager::NonDelegatingQueryPinInterface(
								CDVRInputPin&       rcDVRInputPin,
								REFIID				riid,
								LPVOID&				rpv)
{
	ASSERT (riid != GUID_NULL);

	// Look up the interface id
	SInputPinInterface sPinPlusIID(riid, rcDVRInputPin);
	t_COMPinRegistrants::iterator cItfPos = m_cCOMPinRegistrants.find(sPinPlusIID);

	// Reference count, and return
	if (cItfPos == m_cCOMPinRegistrants.end())
		rpv = NULL;
	else
		GetInterface(&cItfPos->second->GetRegistrationInterface(), &rpv);
}

CPipelineRouter CCapturePipelineManager::GetRouter(
			IPipelineComponent* piStart, bool fUpstream, bool fAsync)
{
	return m_cBasePipelineManager.GetRouter(piStart, fUpstream, fAsync);
}

HANDLE CCapturePipelineManager::GetFlushEvent()
{
	return m_cBasePipelineManager.GetFlushEvent();
}

bool CCapturePipelineManager::WaitEndFlush()
{
	return m_cBasePipelineManager.WaitEndFlush();
}

void CCapturePipelineManager::StartSync()
{
	m_cBasePipelineManager.StartSync();
}

void CCapturePipelineManager::EndSync()
{
	m_cBasePipelineManager.EndSync();
}

//////////////////////////////////////////////////////////////////////////////
//	ICapturePipelineManager interface implementation.						//
//////////////////////////////////////////////////////////////////////////////

CDVRSinkFilter&	CCapturePipelineManager::GetSinkFilter()
{
	return static_cast <CDVRSinkFilter&> (GetFilter());
}

IUnknown* CCapturePipelineManager::RegisterCOMInterface(
			CPipelineUnknown& rcPipUnk, CDVRInputPin& rcInputPin)
{
	const IID& riid = rcPipUnk.GetRegistrationIID();

	ASSERT (riid != GUID_NULL);
	ASSERT (! rcPipUnk.m_pcUnknown || rcPipUnk.m_pcUnknown == &rcInputPin);

	SInputPinInterface sPinPlusIID(riid, rcInputPin);

	t_COMPinRegistrants::iterator cItfPos;
	IUnknown* const piPriorRegistrant =
		(cItfPos = m_cCOMPinRegistrants.find(sPinPlusIID)) == m_cCOMPinRegistrants.end() ?
		NULL : &cItfPos->second->GetRegistrationInterface();

	// Insertion into the map may throw an exception
	m_cCOMPinRegistrants[sPinPlusIID] = &rcPipUnk;

	// Now that the insertion has succeeded, record the binding of this
	// interface implementation to this filter object
	rcPipUnk.m_pcUnknown = &rcInputPin;

	// Return the prior registrant, or NULL if this is the first one
	return piPriorRegistrant;
}

//////////////////////////////////////////////////////////////////////////////
//	CPlaybackPipelineManager.												//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Instance creation.														//
//////////////////////////////////////////////////////////////////////////////

CPlaybackPipelineManager::CPlaybackPipelineManager(CDVRSourceFilter& rcFilter) :
	m_cBasePipelineManager		(rcFilter)
{
}

//////////////////////////////////////////////////////////////////////////////
//	IPipelineManager interface implementation.								//
//////////////////////////////////////////////////////////////////////////////

CBaseFilter& CPlaybackPipelineManager::GetFilter()
{
	return m_cBasePipelineManager.GetFilter();
}

IUnknown* CPlaybackPipelineManager::RegisterCOMInterface(
			CPipelineUnknown& rcPipUnk)
{
	return m_cBasePipelineManager.RegisterCOMInterface(rcPipUnk);
}

void CPlaybackPipelineManager::NonDelegatingQueryInterface(
			REFIID riid, LPVOID& rpv)
{
	m_cBasePipelineManager.NonDelegatingQueryInterface(riid, rpv);
}

CPipelineRouter CPlaybackPipelineManager::GetRouter(
			IPipelineComponent* piStart, bool fUpstream, bool fAsync)
{
	return m_cBasePipelineManager.GetRouter(piStart, fUpstream, fAsync);
}

HANDLE CPlaybackPipelineManager::GetFlushEvent()
{
	return m_cBasePipelineManager.GetFlushEvent();
}

bool CPlaybackPipelineManager::WaitEndFlush()
{
	return m_cBasePipelineManager.WaitEndFlush();
}

void CPlaybackPipelineManager::StartSync()
{
	m_cBasePipelineManager.StartSync();
}

void CPlaybackPipelineManager::EndSync()
{
	m_cBasePipelineManager.EndSync();
}

//////////////////////////////////////////////////////////////////////////////
//	IPlaybackPipelineManager interface implementation.						//
//////////////////////////////////////////////////////////////////////////////

CDVRSourceFilter&	CPlaybackPipelineManager::GetSourceFilter()
{
	return static_cast <CDVRSourceFilter&> (GetFilter());
}

IUnknown* CPlaybackPipelineManager::RegisterCOMInterface(
			CPipelineUnknown& /*rcPipUnk*/, CDVROutputPin& /*rcOutputPin*/)
{
	// Not implemented (functionality of per-pin COM interface exposure not
	// (yet?) required)
	ASSERT (FALSE);
	return NULL;
}

HRESULT CPlaybackPipelineManager::GetMediaType(
			int /*iPosition*/, CMediaType* /*pMediaType*/,
			CDVROutputPin& /*rcOutputPin*/)
{
	return E_UNEXPECTED;
}

//////////////////////////////////////////////////////////////////////////////
//	CPipelineRouter.														//
//////////////////////////////////////////////////////////////////////////////

// Predicate functor used by the following constructor
class CCompareComponent
{
public:
	CCompareComponent(IPipelineComponent* piCompareTarget) :
			m_piCompareTarget	(piCompareTarget)
	{
	}

	bool operator () (const SPipelineElement& rsComareSource) const
	{
		return rsComareSource.m_piComponent == m_piCompareTarget;
	}

private:
	IPipelineComponent* const	m_piCompareTarget;
};

CPipelineRouter::CPipelineRouter(
			const t_PipelineComponents& rcComponentList,
			CCommandQueue* pcQueuedCommands,
			IPipelineComponent*	piStart,
			bool fUpstream, bool fQueueAtHead) :
	m_pcComponentList			(&rcComponentList),
	m_pcQueuedCommands			(pcQueuedCommands),
	m_fUpstream					(fUpstream)
{
	// Initialize the iterator we will be using
	if (fUpstream)
		m_ReverseIterator = piStart ?
			std::find_if(rcComponentList.rbegin(), rcComponentList.rend(),
						 CCompareComponent(piStart)) :
			rcComponentList.rend();
	else
		m_ForwardIterator = piStart ?
			std::find_if(rcComponentList.begin(), rcComponentList.end(),
						 CCompareComponent(piStart)) :
			rcComponentList.end();
}

CPipelineRouter::CPipelineRouter() :
	m_pcComponentList			(NULL),
	m_pcQueuedCommands			(NULL)
{
}

// This helper template allows for forward or reverse iteration over the list
// of pipeline components
template <class InputIterator>
ROUTE PipelineForwardReverseInvoke(
			InputIterator First,
			InputIterator Last, 
			CPipelineRouter::SInvoker& rsInvoker)
{
	ROUTE Result = UNHANDLED_CONTINUE;

	// Iterate from start position to end position, in either forward or
	// reverse direction, depending on iterator type
	for (InputIterator Iter = First; Iter != Last; ++Iter)
	{
		// Invoke the pipeline component; this could abort the iteration
		// process by throwing an exception
		const ROUTE LoopResult = rsInvoker(*Iter->m_piComponent);

		// Discontinue looping, if requested
		if (UNHANDLED_STOP == LoopResult || HANDLED_STOP == LoopResult)
		{
			// Update final iteration result
			if (UNHANDLED_STOP == LoopResult && HANDLED_CONTINUE == Result)
				Result = HANDLED_STOP;
			else
				Result = LoopResult;

			break;
		}

		// Keep track of when there's been a handler of the message
		if (HANDLED_CONTINUE == LoopResult)
			Result = HANDLED_CONTINUE;
	}

	return Result;
}

ROUTE CPipelineRouter::PipelineInvoke(SInvoker& rsInvoker, bool fForceSync /*= false*/) const
{
	// The convention is that rsInvoker refers to a newly allocated SInvoker
	// if and only if m_pcQueuedCommands is non-null. In that case, this 
	// routine is responsible for either successfully queueing the invoker or
	// for deleting it.

	ASSERT (m_pcComponentList);

	// Determine synchronous vs. asynchronous behavior
	if (! fForceSync && m_pcQueuedCommands)
	{
		if (m_pcQueuedCommands->PutQueueObject(CQueuedPipelineCommand(*this, &rsInvoker)))
			return HANDLED_CONTINUE;

		// Filter is flushing
		return UNHANDLED_STOP;
	}

	// Route according to set direction
	// Note: when routing across the entire pipeline, then reset
	//		 the start position here, so that pipeline routers
	//		 can survive pipeline composition changes
	if (m_fUpstream)
		return PipelineForwardReverseInvoke(
			m_ReverseIterator == m_pcComponentList->rend() ?
			m_pcComponentList->rbegin() : ++t_PipelineComponents::const_reverse_iterator(m_ReverseIterator),
			m_pcComponentList->rend(), rsInvoker);

	else
		return PipelineForwardReverseInvoke(
			m_ForwardIterator == m_pcComponentList->end() ?
			m_pcComponentList->begin() : ++t_PipelineComponents::const_iterator(m_ForwardIterator),
			m_pcComponentList->end(), rsInvoker);
}

struct SRemoveFromPipelineInvoker : CPipelineRouter::SInvoker
{
	virtual ROUTE operator () (IPipelineComponent& riPipelineComponent)
	{
		riPipelineComponent.RemoveFromPipeline();
		return HANDLED_CONTINUE;
	}
};

void CPipelineRouter::RemoveFromPipeline()
{
	PipelineInvoke(AllocateInvoker(SRemoveFromPipelineInvoker()));
}

struct SGetPrivateInterfaceInvoker : CPipelineRouter::SInvoker
{
	SGetPrivateInterfaceInvoker(REFIID riid, void*& rpInterface) :
		m_riid			(riid),
		m_rpInterface	(rpInterface)
	{}

	virtual ROUTE operator () (IPipelineComponent& riPipelineComponent)
	{
		return riPipelineComponent.GetPrivateInterface(m_riid, m_rpInterface);
	}

private:
	REFIID	m_riid;
	void*&	m_rpInterface;
};

ROUTE CPipelineRouter::GetPrivateInterface(REFIID riid, void*& rpInterface)
{
	return PipelineInvoke(AllocateInvoker(SGetPrivateInterfaceInvoker(riid, rpInterface)));
}

struct SNotifyFilterStateChangeInvoker : CPipelineRouter::SInvoker
{
	SNotifyFilterStateChangeInvoker(FILTER_STATE eState) :
		m_eState		(eState)
	{}

	virtual ROUTE operator () (IPipelineComponent& riPipelineComponent)
	{
		return riPipelineComponent.NotifyFilterStateChange(m_eState);
	}

private:
	const FILTER_STATE	m_eState;
};

ROUTE CPipelineRouter::NotifyFilterStateChange(FILTER_STATE eState)
{
	return PipelineInvoke(AllocateInvoker(SNotifyFilterStateChangeInvoker(eState)));
}

// Note: This method's payload is not (yet?) copied at the time of invocation,
//		 and it is therefore not (yet?) safe for asynchronous calling without
//		 protecting the input parameters.
struct SConfigurePipelineInvoker : CPipelineRouter::SInvoker
{
	SConfigurePipelineInvoker(UCHAR iNumPins, CMediaType cMediaTypes[],
							  UINT iSizeCustom, BYTE Custom[]) :
		m_iNumPins		(iNumPins),
		m_cMediaTypes	(cMediaTypes),
		m_iSizeCustom	(iSizeCustom),
		m_Custom		(Custom)
	{}

	virtual ROUTE operator () (IPipelineComponent& riPipelineComponent)
	{
		return riPipelineComponent.ConfigurePipeline(m_iNumPins, m_cMediaTypes,
			m_iSizeCustom, m_Custom);
	}

private:
	const UCHAR			m_iNumPins;
	CMediaType* const	m_cMediaTypes;
	const UINT			m_iSizeCustom;
	BYTE* const			m_Custom;
};

ROUTE CPipelineRouter::ConfigurePipeline(UCHAR iNumPins,
										 CMediaType cMediaTypes[],
										 UINT iSizeCustom, BYTE Custom[])
{
	return PipelineInvoke(AllocateInvoker(SConfigurePipelineInvoker(
		iNumPins, cMediaTypes, iSizeCustom, Custom)));
}

struct SDispatchExtensionInvoker : CPipelineRouter::SInvoker
{
	SDispatchExtensionInvoker(CExtendedRequest &rcExtendedRequest)
		: m_pcExtendedRequest		(&rcExtendedRequest)
	{
		m_pcExtendedRequest->AddRef();
	}

	~SDispatchExtensionInvoker()
	{
		m_pcExtendedRequest->Release();
	}

	SDispatchExtensionInvoker(SDispatchExtensionInvoker &rsDispatchExtensionInvoker)
		: m_pcExtendedRequest		(rsDispatchExtensionInvoker.m_pcExtendedRequest)
	{
		ASSERT(m_pcExtendedRequest);
		m_pcExtendedRequest->AddRef();
	}

	SDispatchExtensionInvoker &operator =(SDispatchExtensionInvoker &rsDispatchExtensionInvoker)
	{
		ASSERT(m_pcExtendedRequest);
		ASSERT(rsDispatchExtensionInvoker.m_pcExtendedRequest);
		rsDispatchExtensionInvoker.m_pcExtendedRequest->AddRef();
		m_pcExtendedRequest->Release();
		m_pcExtendedRequest = rsDispatchExtensionInvoker.m_pcExtendedRequest;
	}

	virtual ROUTE operator () (IPipelineComponent& riPipelineComponent)
	{
		return riPipelineComponent.DispatchExtension(*m_pcExtendedRequest);
	}

	virtual bool		DiscardWhileFlushing() const 
	{
		bool fDiscard = true;

		switch (m_pcExtendedRequest->m_eFlushAndStopBehavior)
		{
		case CExtendedRequest::DISCARD_ON_FLUSH:
			fDiscard = true;
			break;

		case CExtendedRequest::EXECUTE_ON_FLUSH:
			fDiscard = true;
			if (m_pcExtendedRequest->m_iPipelineComponentPrimaryTarget)
			{
				try {
					m_pcExtendedRequest->m_iPipelineComponentPrimaryTarget->DispatchExtension(*m_pcExtendedRequest);
				} catch (std::exception &) {};
			}
			break;

		case CExtendedRequest::RETAIN_ON_FLUSH:
			fDiscard = false;
			break;
		}
		return fDiscard;
	}

private:
	CExtendedRequest *m_pcExtendedRequest;
};

ROUTE CPipelineRouter::DispatchExtension(
						CExtendedRequest &rcExtendedRequest)
{
	return PipelineInvoke(AllocateInvoker(SDispatchExtensionInvoker(rcExtendedRequest)));
}

unsigned char CPipelineRouter::AddToPipeline(ICapturePipelineManager& rcManager)
{
	// This call is not routable
	ASSERT (FALSE);
	return 0;
}

struct SProcessInputSampleInvoker : CPipelineRouter::SInvoker
{
	SProcessInputSampleInvoker(IMediaSample& riSample,
							   CDVRInputPin& rcInputPin) :
		m_spSample		(&riSample),
		m_rcInputPin	(rcInputPin)
	{}

	virtual ROUTE operator () (IPipelineComponent& riPipelineComponent)
	{
		return static_cast <ICapturePipelineComponent&> (riPipelineComponent).
			ProcessInputSample(*m_spSample, m_rcInputPin);
	}

	virtual bool DiscardWhileFlushing() const
	{
		return true;
	}

private:
	CComPtr<IMediaSample> m_spSample;
	CDVRInputPin&		m_rcInputPin;
};

ROUTE CPipelineRouter::ProcessInputSample(IMediaSample& riSample,
										  CDVRInputPin& rcInputPin)
{
	return PipelineInvoke(AllocateInvoker(SProcessInputSampleInvoker(riSample, rcInputPin)));
}

struct SNotifyBeginFlushInvoker : CPipelineRouter::SInvoker
{
	SNotifyBeginFlushInvoker(CDVRInputPin& rcInputPin) :
		m_rcInputPin	(rcInputPin)
	{}

	virtual ROUTE operator () (IPipelineComponent& riPipelineComponent)
	{
		return static_cast <ICapturePipelineComponent&> (riPipelineComponent).
			NotifyBeginFlush(m_rcInputPin);
	}

private:
	CDVRInputPin&		m_rcInputPin;
};

ROUTE CPipelineRouter::NotifyBeginFlush(CDVRInputPin& rcInputPin)
{
	return PipelineInvoke(AllocateInvoker(SNotifyBeginFlushInvoker(rcInputPin)));
}

struct SNotifyEndFlushInvoker : CPipelineRouter::SInvoker
{
	SNotifyEndFlushInvoker(CDVRInputPin& rcInputPin) :
		m_rcInputPin	(rcInputPin)
	{}

	virtual ROUTE operator () (IPipelineComponent& riPipelineComponent)
	{
		return static_cast <ICapturePipelineComponent&> (riPipelineComponent).
			NotifyEndFlush(m_rcInputPin);
	}

private:
	CDVRInputPin&		m_rcInputPin;
};

ROUTE CPipelineRouter::NotifyEndFlush(CDVRInputPin& rcInputPin)
{
	return PipelineInvoke(AllocateInvoker(SNotifyEndFlushInvoker(rcInputPin)));
}

struct SGetAllocatorRequirementsInvoker : CPipelineRouter::SInvoker
{
	SGetAllocatorRequirementsInvoker(ALLOCATOR_PROPERTIES& rProperties,
									 CDVRInputPin& rcInputPin) :
		m_rProperties	(rProperties),
		m_rcInputPin	(rcInputPin)
	{}

	virtual ROUTE operator () (IPipelineComponent& riPipelineComponent)
	{
		return static_cast <ICapturePipelineComponent&> (riPipelineComponent).
			GetAllocatorRequirements(m_rProperties, m_rcInputPin);
	}

private:
	ALLOCATOR_PROPERTIES& m_rProperties;
	CDVRInputPin&		m_rcInputPin;
};

ROUTE CPipelineRouter::GetAllocatorRequirements(ALLOCATOR_PROPERTIES& rProperties,
												CDVRInputPin& rcInputPin)
{
	return PipelineInvoke(AllocateInvoker(SGetAllocatorRequirementsInvoker(rProperties, rcInputPin)));
}

struct SNotifyAllocatorInvoker : CPipelineRouter::SInvoker
{
	SNotifyAllocatorInvoker(IMemAllocator& riAllocator, bool fReadOnly,
							CDVRInputPin& rcInputPin) :
		m_spAllocator	(&riAllocator),
		m_fReadOnly		(fReadOnly),
		m_rcInputPin	(rcInputPin)
	{}

	virtual ROUTE operator () (IPipelineComponent& riPipelineComponent)
	{
		return static_cast <ICapturePipelineComponent&> (riPipelineComponent).
			NotifyAllocator(*m_spAllocator, m_fReadOnly, m_rcInputPin);
	}

private:
	CComPtr<IMemAllocator> m_spAllocator;
	const bool			m_fReadOnly;
	CDVRInputPin&		m_rcInputPin;
};

ROUTE CPipelineRouter::NotifyAllocator(IMemAllocator& riAllocator, bool fReadOnly,
									   CDVRInputPin& rcInputPin)
{
	return PipelineInvoke(AllocateInvoker(SNotifyAllocatorInvoker(riAllocator, fReadOnly, rcInputPin)));
}

struct SCheckInputMediaTypeInvoker : CPipelineRouter::SInvoker
{
	SCheckInputMediaTypeInvoker(const CMediaType& rcMediaType,
								CDVRInputPin& rcInputPin,
								HRESULT& rhResult) :
		m_rcMediaType	(rcMediaType),
		m_rcInputPin	(rcInputPin),
		m_rhResult		(rhResult)
	{}

	virtual ROUTE operator () (IPipelineComponent& riPipelineComponent)
	{
		return static_cast <ICapturePipelineComponent&> (riPipelineComponent).
			CheckMediaType(m_rcMediaType, m_rcInputPin, m_rhResult);
	}

private:
	const CMediaType&	m_rcMediaType;
	CDVRInputPin&		m_rcInputPin;
	HRESULT&			m_rhResult;
};

ROUTE CPipelineRouter::CheckMediaType(const CMediaType& rcMediaType,
									  CDVRInputPin& rcInputPin,
									  HRESULT& rhResult)
{
	return PipelineInvoke(AllocateInvoker(SCheckInputMediaTypeInvoker(
		rcMediaType, rcInputPin, rhResult)));
}

struct SCompleteConnectInvoker : CPipelineRouter::SInvoker
{
	SCompleteConnectInvoker(IPin& riReceivePin,
							CDVRInputPin& rcInputPin) :
		m_spReceivePin	(&riReceivePin),
		m_rcInputPin	(rcInputPin)
	{}

	virtual ROUTE operator () (IPipelineComponent& riPipelineComponent)
	{
		return static_cast <ICapturePipelineComponent&> (riPipelineComponent).
			CompleteConnect(*m_spReceivePin, m_rcInputPin);
	}

private:
	CComPtr<IPin>		m_spReceivePin;
	CDVRInputPin&		m_rcInputPin;
};

ROUTE CPipelineRouter::CompleteConnect(IPin& riReceivePin,
									   CDVRInputPin& rcInputPin)
{
	return PipelineInvoke(AllocateInvoker(SCompleteConnectInvoker(
		riReceivePin, rcInputPin)));
}

struct SNotifyNewSegmentInvoker : CPipelineRouter::SInvoker
{
	SNotifyNewSegmentInvoker(CDVRInputPin& rcInputPin, 
							 REFERENCE_TIME rtStart,
							 REFERENCE_TIME rtEnd,
							 double dblRate) :
		m_rcInputPin	(rcInputPin),
		m_rtStart		(rtStart),
		m_rtEnd			(rtEnd),
		m_dblRate		(dblRate)
	{}

	virtual ROUTE operator () (IPipelineComponent& riPipelineComponent)
	{
		return static_cast <ICapturePipelineComponent&> (riPipelineComponent).
			NotifyNewSegment(m_rcInputPin, m_rtStart, m_rtEnd, m_dblRate);
	}

private:
	CDVRInputPin&		m_rcInputPin;
	REFERENCE_TIME		m_rtStart, m_rtEnd;
	double				m_dblRate;
};


ROUTE CPipelineRouter::NotifyNewSegment(
						CDVRInputPin&	rcInputPin,
						REFERENCE_TIME rtStart,
						REFERENCE_TIME rtEnd,
						double dblRate)
{
	return PipelineInvoke(AllocateInvoker(SNotifyNewSegmentInvoker(
		rcInputPin, rtStart, rtEnd, dblRate)));
}

unsigned char CPipelineRouter::AddToPipeline(IPlaybackPipelineManager& rcManager)
{
	// This call is not routable
	ASSERT (FALSE);
	return 0;
}

struct SProcessOutputSampleInvoker : CPipelineRouter::SInvoker
{
	SProcessOutputSampleInvoker(IMediaSample& riSample,
								CDVROutputPin& rcOutputPin) :
		m_spSample		(&riSample),
		m_rcOutputPin	(rcOutputPin)
	{}

	virtual ROUTE operator () (IPipelineComponent& riPipelineComponent)
	{
		return static_cast <IPlaybackPipelineComponent&> (riPipelineComponent).
			ProcessOutputSample(*m_spSample, m_rcOutputPin);
	}

	virtual bool DiscardWhileFlushing() const
	{
		return true;
	}

private:
	CComPtr<IMediaSample> m_spSample;
	CDVROutputPin&		m_rcOutputPin;
};

ROUTE CPipelineRouter::ProcessOutputSample(IMediaSample& riSample,
										   CDVROutputPin& rcOutputPin)
{
	return PipelineInvoke(AllocateInvoker(SProcessOutputSampleInvoker(riSample, rcOutputPin)));
}

struct SDecideBufferSizeInvoker : CPipelineRouter::SInvoker
{
	SDecideBufferSizeInvoker(IMemAllocator& riAllocator,
							 ALLOCATOR_PROPERTIES& rProperties,
							 CDVROutputPin& rcOutputPin) :
		m_riAllocator	(riAllocator),
		m_rProperties	(rProperties),
		m_rcOutputPin	(rcOutputPin)
	{}

	virtual ROUTE operator () (IPipelineComponent& riPipelineComponent)
	{
		return static_cast <IPlaybackPipelineComponent&> (riPipelineComponent).
			DecideBufferSize(m_riAllocator, m_rProperties, m_rcOutputPin);
	}

private:
	IMemAllocator&		m_riAllocator;
	ALLOCATOR_PROPERTIES& m_rProperties;
	CDVROutputPin&		m_rcOutputPin;
};

ROUTE CPipelineRouter::DecideBufferSize(IMemAllocator& riAllocator,
										ALLOCATOR_PROPERTIES& rProperties,
										CDVROutputPin& rcOutputPin)
{
	return PipelineInvoke(AllocateInvoker(SDecideBufferSizeInvoker(
		riAllocator, rProperties, rcOutputPin)));
}

struct SCheckOutputMediaTypeInvoker : CPipelineRouter::SInvoker
{
	SCheckOutputMediaTypeInvoker(const CMediaType& rcMediaType,
								 CDVROutputPin& rcOutputPin,
								 HRESULT& rhResult) :
		m_rcMediaType	(rcMediaType),
		m_rcOutputPin	(rcOutputPin),
		m_rhResult		(rhResult)
	{}

	virtual ROUTE operator () (IPipelineComponent& riPipelineComponent)
	{
		return static_cast <IPlaybackPipelineComponent&> (riPipelineComponent).
			CheckMediaType(m_rcMediaType, m_rcOutputPin, m_rhResult);
	}

private:
	const CMediaType&	m_rcMediaType;
	CDVROutputPin&		m_rcOutputPin;
	HRESULT&			m_rhResult;
};

ROUTE CPipelineRouter::CheckMediaType(const CMediaType& rcMediaType,
									  CDVROutputPin& rcOutputPin,
									  HRESULT& rhResult)
{
	return PipelineInvoke(AllocateInvoker(SCheckOutputMediaTypeInvoker(
		rcMediaType, rcOutputPin, rhResult)));
}

struct SEndOfStreamInvoker : CPipelineRouter::SInvoker
{
	virtual ROUTE operator () (IPipelineComponent& riPipelineComponent)
	{
		return static_cast <IPlaybackPipelineComponent&> (riPipelineComponent).
			EndOfStream();
	}
	virtual bool		DiscardWhileFlushing() const { return true; }
};

ROUTE CPipelineRouter::EndOfStream()
{
	return PipelineInvoke(AllocateInvoker(SEndOfStreamInvoker()));
}

}
