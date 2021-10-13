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
// Sink.cpp : media format-invariant sink filter and pin infrastructure.
//

#include "stdafx.h"
#include "Sink.h"

#include "BootstrapPipelineManagers.h"

namespace MSDvr
{

//////////////////////////////////////////////////////////////////////////////
//	Sink filter.															//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Static data.															//
//////////////////////////////////////////////////////////////////////////////

const LPCTSTR	CDVRSinkFilter::s_wzName = _T("DVR MPEG Branch Sink Filter");

const CLSID		CDVRSinkFilter::s_clsid =
	// {07BD5A67-25B8-4738-8CFC-66316CE6799C}
	{ 0x7bd5a67, 0x25b8, 0x4738, { 0x8c, 0xfc, 0x66, 0x31, 0x6c, 0xe6, 0x79, 0x9c } };


static const AMOVIESETUP_MEDIATYPE g_sudMediaTypes[] = {
	{ &MEDIATYPE_Stream, &MEDIASUBTYPE_Asf }
};

static const AMOVIESETUP_PIN g_sudInputPins[] = {
	{	L"",									// obsolete
		FALSE,									// not rendered
		FALSE,									// input pin
		FALSE,									// for now, this is the only
												// supported pin
		FALSE,									// can have only one streaming
												// pin
		&GUID_NULL,								// obsolete
		NULL,									// obsolete
		sizeof(g_sudMediaTypes) / sizeof(g_sudMediaTypes[0]),
												// number of media types
		g_sudMediaTypes	}						// media type array
};

const AMOVIESETUP_FILTER CDVRSinkFilter::s_sudFilterReg =
	{	&s_clsid,								// filter clsid
		s_wzName,								// filter name
		MERIT_DO_NOT_USE,						// merit
		sizeof(g_sudInputPins) / sizeof(g_sudInputPins[0]),
												// number of pin types
		g_sudInputPins	};						// input pin array

//////////////////////////////////////////////////////////////////////////////
//	Instance creation.														//
//////////////////////////////////////////////////////////////////////////////

CDVRSinkFilter::CDVRSinkFilter(LPUNKNOWN piAggUnk) :
	CBaseFilter (s_wzName, piAggUnk, &m_cStateChangeLock, s_clsid)
{
	// Initialize with the bootstrap pipeline manager; this will initialize
	// m_piPipelineManager
	new CBootstrapCapturePipelineManager(*this, m_piPipelineManager);
}

CDVRSinkFilter::~CDVRSinkFilter()
{
	DbgLog((LOG_ENGINE_OTHER, 2, _T("DVR sink filter: destroying instance")));

	// Stop the filter, if necessary, to cease streaming activity
	if (State_Stopped != m_State)
		EXECUTE_ASSERT (SUCCEEDED(Stop()));

	// Delete the pins
	for (std::vector<CDVRInputPin*>::iterator Iterator = m_rgPins.begin();
		 Iterator != m_rgPins.end(); ++Iterator)

		delete *Iterator;

	// Clean up whatever pipeline manager has installed itself by now
	delete m_piPipelineManager;
}

#ifdef DEBUG
STDMETHODIMP_(ULONG) CDVRSinkFilter::NonDelegatingAddRef()
{
	return CBaseFilter::NonDelegatingAddRef();
}

STDMETHODIMP_(ULONG) CDVRSinkFilter::NonDelegatingRelease()
{
	return CBaseFilter::NonDelegatingRelease();
}
#endif // DEBUG

CUnknown* CDVRSinkFilter::CreateInstance(LPUNKNOWN piAggUnk, HRESULT* phr)
{
	// Initialize result code
	if (phr)
		*phr = S_OK;

	// Attempt creation of a new filter object
	try
	{
		DbgLog((LOG_ENGINE_OTHER, 2, _T("DVR sink filter: creating new instance")));
		return new CDVRSinkFilter(piAggUnk);
	}
	catch (std::bad_alloc& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: creation failed due to ")
				_T("resource constraint: %S"), rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: creation failed due to ")
				_T("resource constraint: %s"), rcException.what()));
#endif
		if (phr)
			*phr = E_OUTOFMEMORY;
	}

	return NULL;
}

template<class T> CComPtr<T> CDVRSinkFilter::GetComponentInterface(REFIID riid)
{
	// Query for our interface
	LPVOID p;
	m_piPipelineManager->NonDelegatingQueryInterface(riid, p);
	if (!p)
		return NULL;

	// The refcount on p was already incremented, so attach to it and return.
	CComPtr<T> pI;
	pI.Attach(static_cast<T *>(p));
	return pI;
}

//////////////////////////////////////////////////////////////////////////////
//	External filter management. Such management is performed by a capture	//
//	pipeline manager.														//
//																			//
//	Note: throws exceptions.												//
//////////////////////////////////////////////////////////////////////////////

void CDVRSinkFilter::AddPin(LPCWSTR wzPinName)
{
#ifdef UNICODE
	DbgLog((LOG_ENGINE_OTHER, 3, _T("DVR sink filter: adding pin: %s"), wzPinName));
#else
	DbgLog((LOG_ENGINE_OTHER, 3, _T("DVR sink filter: adding pin: %S"), wzPinName));
#endif
	m_rgPins.push_back(new CDVRInputPin(*this, m_cStateChangeLock, wzPinName,
										static_cast<DWORD>(m_rgPins.size())));
}

//////////////////////////////////////////////////////////////////////////////
//	Filter overrides.														//
//////////////////////////////////////////////////////////////////////////////

int CDVRSinkFilter::GetPinCount()
{
	DbgLog((LOG_ENGINE_OTHER, 4, _T("DVR sink filter: the pin count is being requested, ")
			_T("and a value of %d is being returned"), m_rgPins.size()));
	return static_cast <int> (m_rgPins.size());
}

CBasePin* CDVRSinkFilter::GetPin(int n)
{
	DbgLog((LOG_ENGINE_OTHER, 4, _T("DVR sink filter: pin %d is being requested"), n));
	ASSERT (static_cast <size_t> (n) < m_rgPins.size());

	return m_rgPins.at(n);
}

STDMETHODIMP
CDVRSinkFilter::NonDelegatingQueryInterface(REFIID riid, LPVOID* ppv)
{
#	ifdef DEBUG
	OLECHAR wszIID[39];
	ASSERT (StringFromGUID2(riid, wszIID, sizeof(wszIID) / sizeof(wszIID[0])));
	DbgLog((LOG_ENGINE_OTHER, 3, _T("DVR sink filter: interface %s is being queried"), wszIID));
#	endif

	ASSERT (ppv);

	// Protect against state changes that could lead to registration changes
	CAutoLock cLock(m_pLock);

	// Handle the known interfaces for which we've provided stub implementations
	if (riid == IID_IFileSinkFilter)
		return GetInterface((IFileSinkFilter *) this, ppv);
	else if (riid == IID_IFileSinkFilter2)
		return GetInterface((IFileSinkFilter2 *) this, ppv);
	else if (riid == IID_IStreamBufferCapture)
		return GetInterface((IStreamBufferCapture *) this, ppv);
	else if (riid == IID_IDVREngineHelpers)
		return GetInterface(static_cast <IDVREngineHelpers *> (this), ppv);
	else if (riid == IID_IContentRestrictionCapture)
		return GetInterface((IContentRestrictionCapture *) this, ppv);
	if (riid == IID_IXDSCodec)
		return GetInterface((IXDSCodec *) this, ppv);

	// Check first whether the interface implementation is provided by or
	// overridden by a pipeline component
	m_piPipelineManager->NonDelegatingQueryInterface(riid, *ppv);

	// If the interface has not been registered, let the base class handle it
	if (*ppv)
		return S_OK;

	return CBaseFilter::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP CDVRSinkFilter::Run(REFERENCE_TIME tStart)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: Run; ")
			_T("tStart: %I64d"), tStart));

	CAutoLock cLock(m_pLock);

	// Give base class a chance to fail first
	HRESULT hrResult = CBaseFilter::Run(tStart);

	if (FAILED(hrResult))
	{
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: CBaseFilter::Run ")
				_T("failed: %X"), hrResult));
		return hrResult;
	}

	// Route message
	try
	{
		m_piPipelineManager->GetRouter(NULL, true, false).
			NotifyFilterStateChange(State_Running);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("NotifyFilterStateChange: %S"), rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("NotifyFilterStateChange: %s"), rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: Run dispatch completed")));
	return hrResult;
}

STDMETHODIMP CDVRSinkFilter::Pause()
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: Pause")));

	CAutoLock cLock(m_pLock);

	// Route message before calling the base class, so that all components
	// will have been notified and the notification process completed before
	// asynchronously queued media samples can reach pipeline components
	try
	{
		m_piPipelineManager->GetRouter(NULL, true, false).
			NotifyFilterStateChange(State_Paused);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("NotifyFilterStateChange: %S"), rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("NotifyFilterStateChange: %s"), rcException.what()));
#endif
		return E_FAIL;
	}

	// Let the base class change filter state now
	HRESULT hrResult = CBaseFilter::Pause();

	if (FAILED(hrResult))
	{
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: CBaseFilter::Pause ")
				_T("failed: %X"), hrResult));
		return hrResult;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: Pause dispatch completed")));
	return hrResult;
}

STDMETHODIMP CDVRSinkFilter::Stop()
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: Stop")));

	CAutoLock cLock(m_pLock);

	// Give base class a chance to fail first
	HRESULT hrResult = CBaseFilter::Stop();

	if (FAILED(hrResult))
	{
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: CBaseFilter::Stop ")
				_T("failed: 0x%X"), hrResult));
		return hrResult;
	}

	// Advise all streaming threads to exit
	// Bug #128004:  StartSync() can throw an exception so be careful
	// here to handle exceptions:

	try {
		m_piPipelineManager->StartSync();
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: StartSync failed: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: StartSync failed: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	try {
		m_piPipelineManager->EndSync();
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: EndSync failed: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: EndSync failed: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	// Route message
	try
	{
		m_piPipelineManager->GetRouter(NULL, true, false).
			NotifyFilterStateChange(State_Stopped);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("NotifyFilterStateChange: %S"), rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("NotifyFilterStateChange: %s"), rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: Stop dispatch completed")));
	return hrResult;
}

//////////////////////////////////////////////////////////////////////////////
//	Input pin.																//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Instance creation.														//
//////////////////////////////////////////////////////////////////////////////

CDVRInputPin::CDVRInputPin(CDVRSinkFilter& rcFilter, CCritSec& rcLock,
						   LPCWSTR wzPinName, DWORD dwPinNum) :
	CBaseInputPin (_T("DVR Input Pin"), &rcFilter, &rcLock, &m_hrConstruction,
				   wzPinName),
	m_dwPinNum(dwPinNum)
{
	// Allocate a new allocator and keep a refcount
	m_pPrivateAllocator = new CDVRAllocator(&m_hrConstruction);
	if (m_pPrivateAllocator)
		m_pPrivateAllocator->AddRef();

	int x= 0 ;
}

CDVRInputPin::~CDVRInputPin()
{
	if (m_pPrivateAllocator)
		m_pPrivateAllocator->Release();
}

//////////////////////////////////////////////////////////////////////////////
//	Input pin public interface.												//
//////////////////////////////////////////////////////////////////////////////

CDVRSinkFilter& CDVRInputPin::GetFilter()
{
	ASSERT (m_pFilter);
	return *static_cast <CDVRSinkFilter*> (m_pFilter);
}

//////////////////////////////////////////////////////////////////////////////
//	Pin overrides.															//
//////////////////////////////////////////////////////////////////////////////

HRESULT CDVRInputPin::CheckMediaType(const CMediaType* pmt)
{
	DbgLog((LOG_ENGINE_OTHER, 3, _T("DVR sink filter: dispatching: CheckMediaType")));
	ASSERT (pmt);

	// Do not acquire the filter lock here, since this is an internal method
	// used by both application and streaming threads
	// (e.g.: CBaseInputPin::Receive)

	HRESULT hrResult = E_NOTIMPL;

	// Route message
	try
	{
		GetFilter().m_piPipelineManager->GetRouter(NULL, false, false).
			CheckMediaType(*pmt, *this, hrResult);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("CheckMediaType: %S"), rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("CheckMediaType: %s"), rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_ENGINE_OTHER, 3, _T("DVR sink filter: CheckMediaType dispatch completed")));
	return hrResult;
}

HRESULT CDVRInputPin::CompleteConnect(IPin* pReceivePin)
{
	DbgLog((LOG_ENGINE_OTHER, 3, _T("DVR sink filter: dispatching: CompleteConnect")));
	ASSERT (pReceivePin);
	ASSERT (CritCheckIn(m_pLock));

	// Give base class a chance to fail first
	HRESULT hrResult = CBaseInputPin::CompleteConnect(pReceivePin);

	if (FAILED(hrResult))
	{
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: CBaseInputPin::CompleteConnect ")
				_T("failed: %X"), hrResult));
		return hrResult;
	}

	// Route message
	try
	{
		GetFilter().m_piPipelineManager->GetRouter(NULL, false, false).
			CompleteConnect(*pReceivePin, *this);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("CompleteConnect: %S"), rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("CompleteConnect: %s"), rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_ENGINE_OTHER, 3, _T("DVR sink filter: CompleteConnect dispatch completed")));
	return hrResult;
}

HRESULT CDVRInputPin::NewSegment(REFERENCE_TIME rtStart, REFERENCE_TIME rtEnd, double dblRate)
{
	DbgLog((LOG_EVENT_DETECTED, 3, _T("DVR sink filter: dispatching: NewSegment")));

	// Give base class a chance to fail first
	HRESULT hrResult = CBaseInputPin::NewSegment(rtStart, rtEnd, dblRate);

	if (FAILED(hrResult))
	{
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: CBaseInputPin::NewSegment ")
				_T("failed: %X"), hrResult));
		return hrResult;
	}

	// Route message
	try
	{
		GetFilter().m_piPipelineManager->GetRouter(NULL, false, true).
			NotifyNewSegment(*this, rtStart, rtEnd, dblRate);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("NewSegment: %S"), rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("NewSegment: %s"), rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_EVENT_DETECTED, 3, _T("DVR sink filter: NewSegment dispatch completed")));
	return hrResult;
}


STDMETHODIMP CDVRInputPin::Receive(IMediaSample* pSample)
{
	DbgLog((LOG_SINK_DISPATCH, 3, _T("DVR sink filter: dispatching: Receive")));

	// No locking!!! This is a streaming thread.

	// Give base class a chance to fail first
	HRESULT hrResult = CBaseInputPin::Receive(pSample);

	if (FAILED(hrResult))
	{
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: CBaseInputPin::Receive ")
				_T("failed: %X"), hrResult));
		return hrResult;
	}

	// Route message asynchronously
	try
	{
		if (UNHANDLED_STOP == GetFilter().m_piPipelineManager->
			GetRouter(NULL, false, true).ProcessInputSample(*pSample, *this))

			hrResult = S_FALSE;  // handle internal flushing case
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("ProcessInputSample: %S"), rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("ProcessInputSample: %s"), rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_SINK_DISPATCH, 3, _T("DVR sink filter: Receive dispatch completed: 0x%X"), hrResult));
	return hrResult;
}

STDMETHODIMP CDVRInputPin::ReceiveCanBlock()
{
	DbgLog((LOG_SINK_DISPATCH, 3, _T("DVR sink filter: received: ReceiveCanBlock")));
	return S_FALSE;
}

STDMETHODIMP CDVRInputPin::BeginFlush()
{
	DbgLog((LOG_EVENT_DETECTED, 3, _T("DVR sink filter: dispatching: BeginFlush")));

	CAutoLock cLock(m_pLock);

	// Give base class a chance to fail first, and halt further sample
	// processing
	HRESULT hrResult = CBaseInputPin::BeginFlush();

	if (FAILED(hrResult))
	{
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: CBaseInputPin::BeginFlush ")
				_T("failed: %X"), hrResult));
		return hrResult;
	}

	// Synchronize streaming threads
	// Route message
	try
	{
		GetFilter().m_piPipelineManager->StartSync();
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: StartSync error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	// Route message
	try
	{
		// Now distribute to components
		GetFilter().m_piPipelineManager->GetRouter(NULL, false, false).
			NotifyBeginFlush(*this);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("NotifyBeginFlush: %S"), rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("NotifyBeginFlush: %s"), rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_EVENT_DETECTED, 3, _T("DVR sink filter: BeginFlush dispatch completed")));
	return hrResult;
}

STDMETHODIMP CDVRInputPin::EndFlush()
{
	DbgLog((LOG_EVENT_DETECTED, 3, _T("DVR sink filter: dispatching: EndFlush")));

	CAutoLock cLock(m_pLock);

	// Route message before calling the base class, so that no new samples will
	// be processed until all components have completed flushing
	try
	{
		GetFilter().m_piPipelineManager->GetRouter(NULL, false, false).
			NotifyEndFlush(*this);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("NotifyEndFlush: %S"), rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("NotifyEndFlush: %s"), rcException.what()));
#endif
		return E_FAIL;
	}

	// Release streaming threads
	try
	{
		GetFilter().m_piPipelineManager->EndSync();
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: EndSync error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: EndSync error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	// Reenable accepting samples now
	HRESULT hrResult = CBaseInputPin::EndFlush();

	if (FAILED(hrResult))
	{
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: CBaseInputPin::EndFlush ")
				_T("failed: %X"), hrResult));
		return hrResult;
	}

	DbgLog((LOG_EVENT_DETECTED, 3, _T("DVR sink filter: EndFlush dispatch completed")));
	return hrResult;
}

STDMETHODIMP CDVRInputPin::GetAllocator(IMemAllocator **ppAllocator)
{
	if( m_pPrivateAllocator )
	{
		// Addref out allocator and return it.

		if (!ppAllocator)
			return E_POINTER;

		*ppAllocator = m_pPrivateAllocator;
		m_pPrivateAllocator->AddRef();

		return S_OK;
	}

	// Fall through to the base class
	return CBaseInputPin::GetAllocator(ppAllocator);
}

STDMETHODIMP CDVRInputPin::GetAllocatorRequirements(ALLOCATOR_PROPERTIES* pProps)
{
	DbgLog((LOG_ENGINE_OTHER, 3, _T("DVR sink filter: dispatching: GetAllocatorRequirements")));
	ASSERT (pProps);

	CAutoLock cLock(m_pLock);

	bool fHandled;

	// Route message
	try
	{
		const ROUTE RouteResult = GetFilter().m_piPipelineManager->
			GetRouter(NULL, false, false).GetAllocatorRequirements(*pProps, *this);

		fHandled = HANDLED_CONTINUE == RouteResult || HANDLED_STOP == RouteResult;
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("GetAllocatorRequirements: %S"), rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("GetAllocatorRequirements: %s"), rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_ENGINE_OTHER, 3, _T("DVR sink filter: GetAllocatorRequirements dispatch completed")));
	return fHandled ? S_OK : E_NOTIMPL;
}

STDMETHODIMP CDVRInputPin::NotifyAllocator(IMemAllocator* pAllocator, BOOL bReadOnly)
{
	DbgLog((LOG_ENGINE_OTHER, 3, _T("DVR sink filter: dispatching: NotifyAllocator")));
	ASSERT (pAllocator);

	CAutoLock cLock(m_pLock);

	// Give base class a chance to fail first
	HRESULT hrResult = CBaseInputPin::NotifyAllocator(pAllocator, bReadOnly);

	if (FAILED(hrResult))
	{
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: CBaseInputPin::NotifyAllocator ")
				_T("failed: 0x%X"), hrResult));
		return hrResult;
	}

	// Route message
	try
	{
		GetFilter().m_piPipelineManager->GetRouter(NULL, false, false).
			NotifyAllocator(*pAllocator, bReadOnly ? true : false, *this);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("NotifyAllocator: %S"), rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: routing error: ")
				_T("NotifyAllocator: %s"), rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_ENGINE_OTHER, 3, _T("DVR sink filter: NotifyAllocator dispatch completed")));
	return hrResult;
}

//////////////////////////////////////////////////////////////////////////////
//	IFileSinkFilter2 Implementation.										//
//////////////////////////////////////////////////////////////////////////////

HRESULT CDVRSinkFilter::GetCurFile(LPOLESTR *ppszFileName, AM_MEDIA_TYPE *pmt)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: GetCurFile")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IFileSinkFilter> pFilter
			= GetComponentInterface<IFileSinkFilter>();
		if (pFilter)
			return pFilter->GetCurFile(ppszFileName, pmt);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: GetCurFile error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: GetCurFile error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	// Stub implementation to make graphedit happy

	if (ppszFileName)
	{
		static WCHAR wsz[] = L"DVR_Sink_Filter";
		*ppszFileName = static_cast<LPOLESTR> (CoTaskMemAlloc(sizeof(wsz)));
		if (!*ppszFileName)
			return E_OUTOFMEMORY;
		memcpy(*ppszFileName, wsz, sizeof(wsz));
	}

	if (pmt)
		memset(pmt, 0, sizeof(AM_MEDIA_TYPE));

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: GetCurFile dispatch complete")));
	return S_OK;
}

HRESULT CDVRSinkFilter::SetFileName(LPCOLESTR wszFileName, const AM_MEDIA_TYPE *pmt)
{
	if (!wszFileName)
		return E_POINTER;

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: SetFileName")
			_T("%s"), wszFileName));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IFileSinkFilter> pFilter
			= GetComponentInterface<IFileSinkFilter>();
		if (pFilter)
			return pFilter->SetFileName(wszFileName, pmt);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: SetFileName error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: SetFileName error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	// Stub implementation to make graphedit happy
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: SetFileName dispatch complete")));
	return S_OK;
}

HRESULT CDVRSinkFilter::GetMode(DWORD *pdwMode)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: GetMode")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IFileSinkFilter2> pFilter
			= GetComponentInterface<IFileSinkFilter2>();
		if (pFilter)
			return pFilter->GetMode(pdwMode);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: GetMode error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: GetMode error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	// Stub implementation to make graphedit happy
	*pdwMode = 0;
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: GetMode dispatch complete")));
	return S_OK;
}

HRESULT CDVRSinkFilter::SetMode(DWORD dwMode)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: SetMode:")
			_T("%d"), dwMode));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IFileSinkFilter2> pFilter
			= GetComponentInterface<IFileSinkFilter2>();
		if (pFilter)
			return pFilter->SetMode(dwMode);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: SetMode error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: SetMode error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	// Stub implementation to make graphedit happy
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: SetMode dispatch complete")));
	return S_OK;
}

//////////////////////////////////////////////////////////////////////////////
//	IStreamBufferCapture Implementation.									//
//////////////////////////////////////////////////////////////////////////////
HRESULT CDVRSinkFilter::GetCaptureMode(	STRMBUF_CAPTURE_MODE *pMode,
										LONGLONG *pllMaxBuf)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: GetCaptureMode")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferCapture> pFilter
			= GetComponentInterface<IStreamBufferCapture>();
		if (pFilter)
			return pFilter->GetCaptureMode(pMode, pllMaxBuf);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: GetCaptureMode error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: GetCaptureMode error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: GetCaptureMode dispatch complete")));
	return E_NOTIMPL;
}

HRESULT CDVRSinkFilter::BeginTemporaryRecording(LONGLONG llBufSize)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: BeginTemporaryRecording")
			_T("%d"), (int) llBufSize));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferCapture> pFilter
			= GetComponentInterface<IStreamBufferCapture>();
		if (pFilter)
			return pFilter->BeginTemporaryRecording(llBufSize);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: BeginTemporaryRecording error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: BeginTemporaryRecording error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: BeginTemporaryRecording dispatch complete")));
	return E_NOTIMPL;
}

HRESULT CDVRSinkFilter::BeginPermanentRecording(LONGLONG llRetainedSize,
												LONGLONG *pllActualRetainedSize)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: BeginPermanentRecording")
			_T("%d"), (int) llRetainedSize));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferCapture> pFilter
			= GetComponentInterface<IStreamBufferCapture>();
		if (pFilter)
			return pFilter->BeginPermanentRecording(llRetainedSize, pllActualRetainedSize);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: BeginPermanentRecording error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: BeginPermanentRecording error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: BeginPermanentRecording dispatch complete")));
	return E_NOTIMPL;
}

HRESULT CDVRSinkFilter::ConvertToTemporaryRecording(LPCOLESTR pszFileName)
{
	if (!pszFileName)
		return E_POINTER;

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: ConvertToTemporaryRecording")
			_T("%s"), pszFileName));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferCapture> pFilter
			= GetComponentInterface<IStreamBufferCapture>();
		if (pFilter)
			return pFilter->ConvertToTemporaryRecording(pszFileName);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: ConvertToTemporaryRecording error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: ConvertToTemporaryRecording error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: ConvertToTemporaryRecording dispatch complete")));
	return E_NOTIMPL;
}

HRESULT CDVRSinkFilter::SetRecordingPath(LPCOLESTR pszPath)
{
	if (!pszPath)
		return E_POINTER;

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: SetRecordingPath")
			_T("%s"), pszPath));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferCapture> pFilter
			= GetComponentInterface<IStreamBufferCapture>();
		if (pFilter)
			return pFilter->SetRecordingPath(pszPath);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: SetRecordingPath error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: SetRecordingPath error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: SetRecordingPath dispatch complete")));
	return E_NOTIMPL;
}

HRESULT CDVRSinkFilter::GetRecordingPath(LPOLESTR *ppszPath)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: GetRecordingPath")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferCapture> pFilter
			= GetComponentInterface<IStreamBufferCapture>();
		if (pFilter)
			return pFilter->GetRecordingPath(ppszPath);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: GetRecordingPath error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: GetRecordingPath error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: GetRecordingPath dispatch complete")));
	return E_NOTIMPL;
}

STDMETHODIMP CDVRSinkFilter::GetBoundToLiveToken(/* [out] */ LPOLESTR *ppszToken)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: GetBoundToLiveToken")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferCapture> pFilter
			= GetComponentInterface<IStreamBufferCapture>();
		if (pFilter)
			return pFilter->GetBoundToLiveToken(ppszToken);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: GetBoundToLiveToken error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: GetBoundToLiveToken error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: GetBoundToLiveToken dispatch complete")));
	return E_NOTIMPL;
}

STDMETHODIMP CDVRSinkFilter::GetCurrentPosition(/* [out] */ LONGLONG *hyCurrentPosition)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: GetCurrentPosition")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferCapture> pFilter
			= GetComponentInterface<IStreamBufferCapture>();
		if (pFilter)
			return pFilter->GetCurrentPosition(hyCurrentPosition);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: GetCurrentPosition error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: GetCurrentPosition error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: GetCurrentPosition dispatch complete")));
	return E_NOTIMPL;
}

//////////////////////////////////////////////////////////////////////////////
//	IXDSCodec Implementation.									//
//////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CDVRSinkFilter::get_XDSToRatObjOK(/* [retval][out] */ HRESULT *pHrCoCreateRetVal)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: get_XDSToRatObjOK")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IXDSCodec> pFilter
			= GetComponentInterface<IXDSCodec>();
		if (pFilter)
			return pFilter->get_XDSToRatObjOK(pHrCoCreateRetVal);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: get_XDSToRatObjOK error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: get_XDSToRatObjOK error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: get_XDSToRatObjOK dispatch complete")));
	return E_NOTIMPL;
} // CDVRSinkFilter::get_XDSToRatObjOK

STDMETHODIMP CDVRSinkFilter::put_CCSubstreamService(/* [in] */ long SubstreamMask)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: put_CCSubstreamService")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IXDSCodec> pFilter
			= GetComponentInterface<IXDSCodec>();
		if (pFilter)
			return pFilter->put_CCSubstreamService(SubstreamMask);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: put_CCSubstreamService error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: put_CCSubstreamService error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: put_CCSubstreamService dispatch complete")));
	return E_NOTIMPL;
} // CDVRSinkFilter::put_CCSubstreamService

STDMETHODIMP CDVRSinkFilter::get_CCSubstreamService(/* [retval][out] */ long *pSubstreamMask)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: get_CCSubstreamService")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IXDSCodec> pFilter
			= GetComponentInterface<IXDSCodec>();
		if (pFilter)
			return pFilter->get_CCSubstreamService(pSubstreamMask);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: get_CCSubstreamService error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: get_CCSubstreamService error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: get_CCSubstreamService dispatch complete")));
	return E_NOTIMPL;
} // CDVRSinkFilter::get_CCSubstreamService

STDMETHODIMP CDVRSinkFilter::GetContentAdvisoryRating(
            /* [out] */ PackedTvRating *pRat,
            /* [out] */ long *pPktSeqID,
            /* [out] */ long *pCallSeqID,
            /* [out] */ REFERENCE_TIME *pTimeStart,
            /* [out] */ REFERENCE_TIME *pTimeEnd)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: GetContentAdvisoryRating")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IXDSCodec> pFilter
			= GetComponentInterface<IXDSCodec>();
		if (pFilter)
			return pFilter->GetContentAdvisoryRating(pRat, pPktSeqID, pCallSeqID, pTimeStart, pTimeEnd);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: GetContentAdvisoryRating error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: GetContentAdvisoryRating error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: GetContentAdvisoryRating dispatch complete")));
	return E_NOTIMPL;
} // CDVRSinkFilter::GetContentAdvisoryRating

STDMETHODIMP CDVRSinkFilter::GetXDSPacket(
            /* [out] */ long *pXDSClassPkt,
            /* [out] */ long *pXDSTypePkt,
            /* [out] */ BSTR *pBstrXDSPkt,
            /* [out] */ long *pPktSeqID,
            /* [out] */ long *pCallSeqID,
            /* [out] */ REFERENCE_TIME *pTimeStart,
            /* [out] */ REFERENCE_TIME *pTimeEnd)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: GetXDSPacket")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IXDSCodec> pFilter
			= GetComponentInterface<IXDSCodec>();
		if (pFilter)
			return pFilter->GetXDSPacket(pXDSClassPkt, pXDSTypePkt, pBstrXDSPkt,
					pPktSeqID, pCallSeqID, pTimeStart, pTimeEnd);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1,_T("DVR sink filter: GetXDSPacket error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: GetXDSPacket error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: GetXDSPacket dispatch complete")));
	return E_NOTIMPL;
} // CDVRSinkFilter::GetXDSPacket

//////////////////////////////////////////////////////////////////////////////
//	IContentRestrictionCapture Implementation.									//
//////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CDVRSinkFilter::SetEncryptionEnabled(/* [in] */ BOOL fEncrypt)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: SetEncryptionEnabled")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IContentRestrictionCapture> pFilter
			= GetComponentInterface<IContentRestrictionCapture>();
		if (pFilter)
			return pFilter->SetEncryptionEnabled(fEncrypt);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: SetEncryptionEnabled error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: SetEncryptionEnabled error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: SetEncryptionEnabled dispatch complete")));
	return E_NOTIMPL;
} // CDVRSinkFilter::SetEncryptionEnabled

STDMETHODIMP CDVRSinkFilter::GetEncryptionEnabled(/* [out] */ BOOL *pfEncrypt)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: GetEncryptionEnabled")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IContentRestrictionCapture> pFilter
			= GetComponentInterface<IContentRestrictionCapture>();
		if (pFilter)
			return pFilter->GetEncryptionEnabled(pfEncrypt);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: GetEncryptionEnabled error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: GetEncryptionEnabled error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: GetEncryptionEnabled dispatch complete")));
	return E_NOTIMPL;
} // CDVRSinkFilter::GetEncryptionEnabled

STDMETHODIMP CDVRSinkFilter::ExpireVBICopyProtection(/* [in] */ ULONG uExpiredPolicy)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: dispatching: ExpireVBICopyProtection")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IContentRestrictionCapture> pFilter
			= GetComponentInterface<IContentRestrictionCapture>();
		if (pFilter)
			return pFilter->ExpireVBICopyProtection(uExpiredPolicy);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: ExpireVBICopyProtection error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink filter: ExpireVBICopyProtection error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink filter: ExpireVBICopyProtection dispatch complete")));
	return E_NOTIMPL;
} // CDVRSinkFilter::ExpireVBICopyProtection

//////////////////////////////////////////////////////////////////////////////
//	Input Pin's IKsPropertySet Implementation.									//
//////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CDVRInputPin::Set(
            /* [in] */ REFGUID guidPropSet,
            /* [in] */ DWORD dwPropID,
            /* [size_is][in] */ LPVOID pInstanceData,
            /* [in] */ DWORD cbInstanceData,
            /* [size_is][in] */ LPVOID pPropData,
            /* [in] */ DWORD cbPropData)
{
	DbgLog((LOG_COPY_PROTECTION, 3, _T("DVR sink pin: dispatching: Set")));	// too chatty for the user-request zone

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IKsPropertySet> pPin
			= GetComponentInterface<IKsPropertySet>();
		if (pPin)
			return pPin->Set(guidPropSet, dwPropID, pInstanceData,
				cbInstanceData, pPropData, cbPropData);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink pin: Set error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink pin: Set error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink pin: Set dispatch complete")));
	return E_NOTIMPL;
} // CDVRInputPin::Set

STDMETHODIMP CDVRInputPin::Get(
            /* [in] */ REFGUID guidPropSet,
            /* [in] */ DWORD dwPropID,
            /* [size_is][in] */ LPVOID pInstanceData,
            /* [in] */ DWORD cbInstanceData,
            /* [size_is][out] */ LPVOID pPropData,
            /* [in] */ DWORD cbPropData,
            /* [out] */ DWORD *pcbReturned)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink pin: dispatching: Get")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IKsPropertySet> pPin
			= GetComponentInterface<IKsPropertySet>();
		if (pPin)
			return pPin->Get(guidPropSet, dwPropID, pInstanceData,
				cbInstanceData, pPropData, cbPropData, pcbReturned);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink pin: Get error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink pin: Get error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink pin: Get dispatch complete")));
	return E_NOTIMPL;
} // CDVRInputPin::Get

STDMETHODIMP CDVRInputPin::QuerySupported(
            /* [in] */ REFGUID guidPropSet,
            /* [in] */ DWORD dwPropID,
            /* [out] */ DWORD *pTypeSupport)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink pin: dispatching: QuerySupported")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IKsPropertySet> pPin
			= GetComponentInterface<IKsPropertySet>();
		if (pPin)
			return pPin->QuerySupported(guidPropSet, dwPropID, pTypeSupport);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR sink pin: QuerySupported error: %S"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR sink pin: QuerySupported error: %s"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR sink pin: QuerySupported dispatch complete")));
	return E_NOTIMPL;
} // CDVRInputPin::QuerySupported

STDMETHODIMP CDVRInputPin::NonDelegatingQueryInterface(REFIID riid, LPVOID* ppv)
{
#	ifdef DEBUG
	OLECHAR wszIID[39];
	ASSERT (StringFromGUID2(riid, wszIID, sizeof(wszIID) / sizeof(wszIID[0])));
	DbgLog((LOG_ENGINE_OTHER, 3, _T("DVR sink pin: interface %s is being queried"), wszIID));
#	endif

	ASSERT (ppv);

	// Protect against state changes that could lead to registration changes
	CAutoLock cLock(m_pLock);

	// Handle the known interfaces for which we've provided stub implementations
	if (riid == IID_IKsPropertySet)
		return GetInterface((IKsPropertySet *) this, ppv);

	// Check first whether the interface implementation is provided by or
	// overridden by a pipeline component
	GetFilter().m_piPipelineManager->NonDelegatingQueryPinInterface(*this, riid, *ppv);

	// If the interface has not been registered, let the base class handle it
	if (*ppv)
		return S_OK;

	return CBaseInputPin::NonDelegatingQueryInterface(riid, ppv);
} // CDVRInputPin::NonDelegatingQueryInterface

template<class T> CComPtr<T> CDVRInputPin::GetComponentInterface(REFIID riid)
{
	// Query for our interface
	LPVOID p;
	GetFilter().m_piPipelineManager->NonDelegatingQueryPinInterface(*this, riid, p);
	if (!p)
		return NULL;

	// The refcount on p was already incremented, so attach to it and return.
	CComPtr<T> pI;
	pI.Attach(static_cast<T *>(p));
	return pI;
}

}
