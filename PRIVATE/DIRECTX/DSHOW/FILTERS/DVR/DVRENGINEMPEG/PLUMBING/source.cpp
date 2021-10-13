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
// Source.cpp : media format-invariant source filter and pin infrastructure.
//

#include "stdafx.h"
#include "Source.h"

#include "BootstrapPipelineManagers.h"
#include "..\\SampleConsumer\\SampleConsumer.h"
#include "AVGlitchEvents.h"
#include "DVREngine.h"

namespace MSDvr
{

//////////////////////////////////////////////////////////////////////////////
//	Source filter.															//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Static data.															//
//////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG

// Debug builds struggle with nearly pegged CPU -- use wider tolerances to
// avoid inappropriate recovery attempts:

#define MIN_ACCEPTABLE_CLOCK_RATE	g_dwMinAcceptableClockDebug
#define MAX_ACCEPTABLE_CLOCK_RATE	g_dwMaxAcceptableClockDebug

#else // !DEBUG

#define MIN_ACCEPTABLE_CLOCK_RATE	g_dwMinAcceptableClockRetail
#define MAX_ACCEPTABLE_CLOCK_RATE	g_dwMaxAcceptableClockRetail

#endif // !DEBUG

const LPCTSTR	CDVRSourceFilter::s_wzName = _T("DVR MPEG Branch Source Filter");

const CLSID		CDVRSourceFilter::s_clsid =
	// {A791E35D-DCD7-46cc-A8F5-99D3899FF921}
	{ 0xa791e35d, 0xdcd7, 0x46cc, { 0xa8, 0xf5, 0x99, 0xd3, 0x89, 0x9f, 0xf9, 0x21 } };

static const AMOVIESETUP_MEDIATYPE g_sudMediaTypes[] = {
	{ &MEDIATYPE_Stream, &MEDIASUBTYPE_Asf }
};

static const AMOVIESETUP_PIN g_sudOutputPins[] = {
	{	L"",									// obsolete
		FALSE,									// not rendered
		TRUE,									// output pin
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

const AMOVIESETUP_FILTER CDVRSourceFilter::s_sudFilterReg =
	{	&s_clsid,								// filter clsid
		s_wzName,								// filter name
		MERIT_DO_NOT_USE,						// merit
		sizeof(g_sudOutputPins) / sizeof(g_sudOutputPins[0]),
												// number of pin types
		g_sudOutputPins	};						// output pin array

//////////////////////////////////////////////////////////////////////////////
//	Instance creation.														//
//////////////////////////////////////////////////////////////////////////////

CDVRSourceFilter::CDVRSourceFilter(LPUNKNOWN piAggUnk, HRESULT *pHR) :
	CBaseFilter (s_wzName, piAggUnk, &m_cStateChangeLock, s_clsid),
	m_cClock(static_cast<IBaseFilter*>(this), pHR)
{
	// Initialize with the bootstrap pipeline manager; this will initialize
	// m_piPipelineManager
	new CBootstrapPlaybackPipelineManager(*this, m_piPipelineManager);
}

CDVRSourceFilter::~CDVRSourceFilter()
{
	DbgLog((LOG_ENGINE_OTHER, 2, _T("DVR source filter: destroying instance\n")));

	// Stop the filter, if necessary, to cease streaming activity
	if (State_Stopped != m_State)
		EXECUTE_ASSERT (SUCCEEDED(Stop()));

	// Delete the pins
	for (std::vector<CDVROutputPin*>::iterator Iterator = m_rgPins.begin();
		 Iterator != m_rgPins.end(); ++Iterator)

		delete *Iterator;

	// Clean up whatever pipeline manager has installed itself by now
	delete m_piPipelineManager;
}

#ifdef DEBUG
STDMETHODIMP_(ULONG) CDVRSourceFilter::NonDelegatingAddRef()
{
	return CBaseFilter::NonDelegatingAddRef();
}

STDMETHODIMP_(ULONG) CDVRSourceFilter::NonDelegatingRelease()
{
	return CBaseFilter::NonDelegatingRelease();
}
#endif // DEBUG


CUnknown* CDVRSourceFilter::CreateInstance(LPUNKNOWN piAggUnk, HRESULT* phr)
{
	// Initialize result code
	if (phr)
		*phr = S_OK;

	// Attempt creation of a new filter object
	try
	{
		DbgLog((LOG_ENGINE_OTHER, 2, _T("DVR source filter: creating new instance\n")));
		return new CDVRSourceFilter(piAggUnk, phr);
	}
	catch (std::bad_alloc& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: creation failed due to ")
				_T("resource constraint: %S\n"), rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: creation failed due to ")
				_T("resource constraint: %s\n"), rcException.what()));
#endif
		if (phr)
			*phr = E_OUTOFMEMORY;
	}

	return NULL;
}

template<class T> CComPtr<T> CDVRSourceFilter::GetComponentInterface(REFIID riid)
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
//	External filter management. Such management is performed by a playback	//
//	pipeline manager.														//
//																			//
//	Note: throws exceptions.												//
//////////////////////////////////////////////////////////////////////////////

void CDVRSourceFilter::AddPin(LPCWSTR wzPinName)
{
#ifdef UNICODE
	DbgLog((LOG_ENGINE_OTHER, 3, _T("DVR source filter: adding pin: %s\n"), wzPinName));
#else
	DbgLog((LOG_ENGINE_OTHER, 3, _T("DVR source filter: adding pin: %S\n"), wzPinName));
#endif
	m_rgPins.push_back(new CDVROutputPin(	*this, m_cStateChangeLock,
											wzPinName, (DWORD)m_rgPins.size()));
}

//////////////////////////////////////////////////////////////////////////////
//	Filter overrides.														//
//////////////////////////////////////////////////////////////////////////////

int CDVRSourceFilter::GetPinCount()
{
	DbgLog((LOG_ENGINE_OTHER, 4, _T("DVR source filter: the pin count is being requested, ")
			_T("and a value of %d is being returned\n"), m_rgPins.size()));
	return static_cast <int> (m_rgPins.size());
}

CBasePin* CDVRSourceFilter::GetPin(int n)
{
	DbgLog((LOG_ENGINE_OTHER, 4, _T("DVR source filter: pin %d is being requested\n"), n));
	ASSERT (static_cast <size_t> (n) < m_rgPins.size());

	return m_rgPins.at(n);
}

STDMETHODIMP
CDVRSourceFilter::NonDelegatingQueryInterface(REFIID riid, LPVOID* ppv)
{
#	ifdef DEBUG
	OLECHAR wszIID[39];
	ASSERT (StringFromGUID2(riid, wszIID, sizeof(wszIID) / sizeof(wszIID[0])));
	DbgLog((LOG_ENGINE_OTHER, 3, _T("DVR source filter: interface %s is being queried\n"), wszIID));
#	endif

	ASSERT (ppv);

	// Protect against state changes that could lead to registration changes
	CAutoLock cLock(m_pLock);

	// Handle the known interfaces for which we've provided stub implementations
	if (riid == IID_IFileSourceFilter)
		return GetInterface(static_cast <IFileSourceFilter *> (this), ppv);
	else if (riid == IID_IStreamBufferMediaSeeking)
		return GetInterface(static_cast <IStreamBufferMediaSeeking *> (this), ppv);
	else if (riid == IID_IStreamBufferPlayback)
		return GetInterface(static_cast <IStreamBufferPlayback *> (this), ppv);
	else if (riid == IID_IDVREngineHelpers)
		return GetInterface(static_cast <IDVREngineHelpers *> (this), ppv);
	else if (riid == IID_IReferenceClock)
		return GetInterface(static_cast <IReferenceClock *> (&m_cClock), ppv);

	// Check next whether the interface implementation is provided by or
	// overridden by a pipeline component
	m_piPipelineManager->NonDelegatingQueryInterface(riid, *ppv);

	// If the interface has not been registered, let the base class handle it
	if (*ppv)
		return S_OK;

	return CBaseFilter::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP CDVRSourceFilter::Run(REFERENCE_TIME tStart)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: Run; ")
			_T("tStart: %I64d\n"), tStart));

	HRESULT hrResult;

	CAutoLock cLock(m_pLock);

	// Give base class a chance to fail first
	hrResult = CBaseFilter::Run(tStart);

	if (FAILED(hrResult))
	{
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: CBaseFilter::Run ")
				_T("failed: %X\n"), hrResult));
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
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: routing error: ")
				_T("NotifyFilterStateChange: %S\n"), rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: routing error: ")
				_T("NotifyFilterStateChange: %s\n"), rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: Run dispatch completed\n")));
	return hrResult;
}

STDMETHODIMP CDVRSourceFilter::Pause()
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: Pause\n")));

	CAutoLock cLock(m_pLock);

	// Save filter state, so that we can reconstruct where we're
	// transitioning from
	const FILTER_STATE SavState = m_State;

	// Give base class a chance to fail first; in addition, output pins must
	// be activated so their allocators won't fail the streaming threads that
	// are about to be started
	HRESULT hrResult = CBaseFilter::Pause();

	if (FAILED(hrResult))
	{
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: CBaseFilter::Pause ")
				_T("failed: %X\n"), hrResult));
		return hrResult;
	}

	// Route message, with visibility of the state of origin
	m_State = SavState;
	try
	{
		m_piPipelineManager->GetRouter(NULL, true, false).
			NotifyFilterStateChange(State_Paused);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: routing error: ")
				_T("NotifyFilterStateChange: %S\n"), rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: routing error: ")
				_T("NotifyFilterStateChange: %s\n"), rcException.what()));
#endif
		return E_FAIL;
	}

	m_State = State_Paused;
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: Pause dispatch completed\n")));
	return hrResult;
}

STDMETHODIMP CDVRSourceFilter::Stop()
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: Stop\n")));

	HRESULT hrResult;

	// Windows/CE's DirectX does allow Stop() to go directly from state running:
	if (m_State == State_Running)
	{
		DbgLog((LOG_SOURCE_STATE, 3, _T("DVR source filter: dispatching: Pause to compensate for Win/CE\n") ));
		hrResult = Pause();
		if (FAILED(hrResult))
		{
			DbgLog((LOG_ERROR, 1, _T("DVR source filter: CBaseFilter::Stop()'s call to Pause  ")
				_T("failed: 0x%X\n"), hrResult));
			return hrResult;
		}
	}

	CAutoLock cLock(m_pLock);

	// Give base class a chance to fail first
	hrResult = CBaseFilter::Stop();

	if (FAILED(hrResult))
	{
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: CBaseFilter::Stop ")
				_T("failed: 0x%X\n"), hrResult));
		return hrResult;
	}

	// Advise all streaming threads to exit.
	// Bug #128004:  StartSync() can throw an exception so be careful
	// here to handle exceptions:

	try {
		m_piPipelineManager->StartSync();
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: StartSync failed: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: StartSync failed: %s\n"),
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
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: EndSync failed: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: EndSync failed: %s\n"),
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
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: routing error: ")
				_T("NotifyFilterStateChange: %S\n"), rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: routing error: ")
				_T("NotifyFilterStateChange: %s\n"), rcException.what()));
#endif
		return E_FAIL;
	}

#if 0
	// Release our clock
	m_cClock.SetExternalClock(NULL);
#endif

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: Stop dispatch completed\n")));
	return hrResult;
}

STDMETHODIMP CDVRSourceFilter::JoinFilterGraph(IFilterGraph *pGraph, LPCWSTR pName)
{
	// Let the base class handle it first
	HRESULT hr = CBaseFilter::JoinFilterGraph(pGraph, pName);
	if (FAILED(hr))
		return hr;

	// Tell the graph to use our clock
	CComQIPtr<IMediaFilter> pGraphFilter = pGraph;
	if (!pGraphFilter)
	{
		// Error - continue on though we might not be the clock
		return S_FALSE;
	}
	return pGraphFilter->SetSyncSource(&m_cClock);
}

//////////////////////////////////////////////////////////////////////////////
//	Output pin.																//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Instance creation.														//
//////////////////////////////////////////////////////////////////////////////

CDVROutputPin::CDVROutputPin(CDVRSourceFilter& rcFilter, CCritSec& rcLock,
							 LPCWSTR wzPinName, DWORD dwPinNum) :
	CBaseOutputPin (_T("DVR Output Pin"), &rcFilter, &rcLock,
					&m_hrConstruction, wzPinName),
	m_dwPinNum(dwPinNum)
{
}

//////////////////////////////////////////////////////////////////////////////
//	Output pin public interface.											//
//////////////////////////////////////////////////////////////////////////////

CDVRSourceFilter& CDVROutputPin::GetFilter()
{
	ASSERT (m_pFilter);
	return *static_cast <CDVRSourceFilter*> (m_pFilter);
}

//////////////////////////////////////////////////////////////////////////////
//	Pin overrides.															//
//////////////////////////////////////////////////////////////////////////////

HRESULT CDVROutputPin::CheckMediaType(const CMediaType* pmt)
{
	DbgLog((LOG_ENGINE_OTHER, 3, _T("DVR source filter: dispatching: CheckMediaType\n")));
	ASSERT (pmt);

	// Do not acquire the filter lock here, since this is an internal method
	// used by both application and streaming threads
	// (e.g.: CBasePin::QueryAccept)

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
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: routing error: ")
				_T("CheckMediaType: %S\n"), rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: routing error: ")
				_T("CheckMediaType: %s\n"), rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_ENGINE_OTHER, 3, _T("DVR source filter: CheckMediaType dispatch completed\n")));
	return hrResult;
}

HRESULT CDVROutputPin::
DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* ppropInputRequest)
{
	DbgLog((LOG_ENGINE_OTHER, 3, _T("DVR source filter: dispatching: DecideBufferSize\n")));
	ASSERT (pAlloc);
	ASSERT (ppropInputRequest);
	ASSERT (CritCheckIn(m_pLock));

	// Route message
	try
	{
		GetFilter().m_piPipelineManager->GetRouter(NULL, false, false).
			DecideBufferSize(*pAlloc, *ppropInputRequest, *this);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: routing error: ")
				_T("DecideBufferSize: %S\n"), rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: routing error: ")
				_T("DecideBufferSize: %s\n"), rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_ENGINE_OTHER, 3, _T("DVR source filter: DecideBufferSize dispatch completed\n")));
	return S_OK;
}

HRESULT CDVROutputPin::GetMediaType(int iPosition, CMediaType* pMediaType)
{
	if (iPosition < 0)
		return E_INVALIDARG;

	if (iPosition >= 1)
		return VFW_S_NO_MORE_ITEMS;

	return GetFilter().m_piPipelineManager->GetMediaType(m_dwPinNum, pMediaType, *this);
}

//////////////////////////////////////////////////////////////////////////////
//	IFileSourceFilter Implementation.										//
//////////////////////////////////////////////////////////////////////////////

HRESULT CDVRSourceFilter::Load(LPCOLESTR pszFileName, const AM_MEDIA_TYPE* pmt)
{
	if (! pszFileName)
		return E_POINTER;

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: Load: ")
			_T("%s\n"), pszFileName));
	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		IFileSourceFilter* piFilter =
			GetComponentInterface<IFileSourceFilter>();
		  // Release early, before making the call, since the bootstrap pipeline
		  // manager deletes itself before returning from the call
		if (piFilter)
		{
			HRESULT hr = piFilter->Load(pszFileName, pmt);
			if (SUCCEEDED(hr))
			{
				if (!m_cClock.HasSampleConsumer())
				{
					CPipelineRouter cRouter = m_piPipelineManager->GetRouter(NULL, false, false);
					void *p;
					cRouter.GetPrivateInterface(IID_ISampleConsumer, p);
					ISampleConsumer *pConsumer = (ISampleConsumer *) p;
					m_cClock.SetSampleConsumer(pConsumer);
				}

				m_cClock.ResetRateAdjustment();
			}
			return hr;
		}
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: Load error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: Load error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: Load dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::GetCurFile(LPOLESTR* ppszFileName, AM_MEDIA_TYPE* pmt)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: GetCurFile\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IFileSourceFilter> pFilter 
			= GetComponentInterface<IFileSourceFilter>();
		if (pFilter)
			return pFilter->GetCurFile(ppszFileName, pmt);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetCurFile error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetCurFile error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: GetCurFile dispatch complete\n")));
	return E_NOTIMPL;
}

//////////////////////////////////////////////////////////////////////////////
//	IStreamBufferMediaSeeking Implementation.								//
//////////////////////////////////////////////////////////////////////////////

HRESULT CDVRSourceFilter::GetCapabilities(DWORD *pCapabilities)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: GetCapabilities\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferMediaSeeking> pFilter 
			= GetComponentInterface<IStreamBufferMediaSeeking>();
		if (pFilter)
			return pFilter->GetCapabilities(pCapabilities);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetCapabilities error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetCapabilities error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: GetCapabilities dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::CheckCapabilities(DWORD *pCapabilities)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: CheckCapabilities")
			_T("%d\n"), *pCapabilities));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferMediaSeeking> pFilter 
			= GetComponentInterface<IStreamBufferMediaSeeking>();
		if (pFilter)
			return pFilter->CheckCapabilities(pCapabilities);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: CheckCapabilities error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: CheckCapabilities error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: CheckCapabilities dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::IsFormatSupported(const GUID *pFormat)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: IsFormatSupported\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferMediaSeeking> pFilter 
			= GetComponentInterface<IStreamBufferMediaSeeking>();
		if (pFilter)
			return pFilter->IsFormatSupported(pFormat);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: IsFormatSupported error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: IsFormatSupported error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: IsFormatSupported dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::QueryPreferredFormat(GUID *pFormat)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: QueryPreferredFormat\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferMediaSeeking> pFilter 
			= GetComponentInterface<IStreamBufferMediaSeeking>();
		if (pFilter)
			return pFilter->QueryPreferredFormat(pFormat);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: QueryPreferredFormat error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: QueryPreferredFormat error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: QueryPreferredFormat dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::GetTimeFormat(GUID *pFormat)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: GetTimeFormat\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferMediaSeeking> pFilter 
			= GetComponentInterface<IStreamBufferMediaSeeking>();
		if (pFilter)
			return pFilter->GetTimeFormat(pFormat);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetTimeFormat error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetTimeFormat error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: GetTimeFormat dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::IsUsingTimeFormat(const GUID *pFormat)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: IsUsingTimeFormat\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferMediaSeeking> pFilter 
			= GetComponentInterface<IStreamBufferMediaSeeking>();
		if (pFilter)
			return pFilter->IsUsingTimeFormat(pFormat);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: IsUsingTimeFormat error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: IsUsingTimeFormat error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: IsUsingTimeFormat dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::SetTimeFormat(const GUID *pFormat)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: SetTimeFormat\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferMediaSeeking> pFilter 
			= GetComponentInterface<IStreamBufferMediaSeeking>();
		if (pFilter)
			return pFilter->SetTimeFormat(pFormat);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: SetTimeFormat error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: SetTimeFormat error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: SetTimeFormat dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::GetDuration(LONGLONG *pDuration)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: GetDuration\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferMediaSeeking> pFilter 
			= GetComponentInterface<IStreamBufferMediaSeeking>();
		if (pFilter)
			return pFilter->GetDuration(pDuration);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetDuration error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetDuration error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: GetDuration dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::GetStopPosition(LONGLONG *pStop)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: GetStopPosition\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferMediaSeeking> pFilter 
			= GetComponentInterface<IStreamBufferMediaSeeking>();
		if (pFilter)
			return pFilter->GetStopPosition(pStop);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetStopPosition error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetStopPosition error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: GetStopPosition dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::GetCurrentPosition(LONGLONG *pCurrent)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: GetCurrentPosition\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferMediaSeeking> pFilter 
			= GetComponentInterface<IStreamBufferMediaSeeking>();
		if (pFilter)
			return pFilter->GetCurrentPosition(pCurrent);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetCurrentPosition error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetCurrentPosition error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: GetCurrentPosition dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::SetRate(double dRate)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: SetRate %lf\n"), dRate));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferMediaSeeking> pFilter 
			= GetComponentInterface<IStreamBufferMediaSeeking>();
		if (pFilter)
			return pFilter->SetRate(dRate);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: SetRate error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: SetRate error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: SetRate dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::GetRate(double *pdRate)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: GetRate\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferMediaSeeking> pFilter 
			= GetComponentInterface<IStreamBufferMediaSeeking>();
		if (pFilter)
			return pFilter->GetRate(pdRate);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetRate error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetRate error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: GetRate dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::GetPreroll(LONGLONG *pllPreroll)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: GetPreroll\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferMediaSeeking> pFilter 
			= GetComponentInterface<IStreamBufferMediaSeeking>();
		if (pFilter)
			return pFilter->GetPreroll(pllPreroll);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetPreroll error: %\nS"),
				rcException.what()));
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: GetPreroll dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::ConvertTimeFormat(LONGLONG *pTarget,
											const GUID *pTargetFormat,
											LONGLONG Source,
											const GUID *pSourceFormat)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: ConvertTimeFormat\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferMediaSeeking> pFilter 
			= GetComponentInterface<IStreamBufferMediaSeeking>();
		if (pFilter)
			return pFilter->ConvertTimeFormat(pTarget, pTargetFormat, Source, pSourceFormat);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: ConvertTimeFormat error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: ConvertTimeFormat error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: ConvertTimeFormat dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::SetPositions(	LONGLONG *pCurrent,
										DWORD dwCurrentFlags,
										LONGLONG *pStop,
										DWORD dwStopFlags)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: SetPositions\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferMediaSeeking> pFilter 
			= GetComponentInterface<IStreamBufferMediaSeeking>();
		if (pFilter)
			return pFilter->SetPositions(pCurrent, dwCurrentFlags, pStop, dwStopFlags);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: SetPositions error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: SetPositions error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: SetPositions dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::GetPositions(LONGLONG *pCurrent, LONGLONG *pStop)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: GetPositions\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferMediaSeeking> pFilter 
			= GetComponentInterface<IStreamBufferMediaSeeking>();
		if (pFilter)
			return pFilter->GetPositions(pCurrent, pStop);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetPositions error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetPositions error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: GetPositions dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::GetAvailable(LONGLONG *pEarliest, LONGLONG *pLatest)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: GetAvailable\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferMediaSeeking> pFilter 
			= GetComponentInterface<IStreamBufferMediaSeeking>();
		if (pFilter)
			return pFilter->GetAvailable(pEarliest, pLatest);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetAvailable error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetAvailable error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: GetAvailable dispatch complete\n")));
	return E_NOTIMPL;
}

//////////////////////////////////////////////////////////////////////////////
//	IStreamBufferPlayback Implementation.								//
//////////////////////////////////////////////////////////////////////////////

HRESULT CDVRSourceFilter::SetTunePolicy(STRMBUF_PLAYBACK_TUNE_POLICY eStrmbufPlaybackTunePolicy)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: SetTunePolicy %d\n"), eStrmbufPlaybackTunePolicy));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferPlayback> pFilter 
			= GetComponentInterface<IStreamBufferPlayback>();
		if (pFilter)
			return pFilter->SetTunePolicy(eStrmbufPlaybackTunePolicy);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: SetTunePolicy error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: SetTunePolicy error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: SetTunePolicy dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::GetTunePolicy(STRMBUF_PLAYBACK_TUNE_POLICY *peStrmbufPlaybackTunePolicy)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: GetTunePolicy\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferPlayback> pFilter 
			= GetComponentInterface<IStreamBufferPlayback>();
		if (pFilter)
			return pFilter->GetTunePolicy(peStrmbufPlaybackTunePolicy);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetTunePolicy error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetTunePolicy error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: GetTunePolicy dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::SetThrottlingEnabled(BOOL fThrottle)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: SetThrottlingEnabled(%u)\n"), (unsigned) fThrottle));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferPlayback> pFilter 
			= GetComponentInterface<IStreamBufferPlayback>();
		if (pFilter)
			return pFilter->SetThrottlingEnabled(fThrottle);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: SetThrottlingEnabled error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: SetThrottlingEnabled error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: SetThrottlingEnabled dispatch complete\n")));
	return E_NOTIMPL;
} // CDVRSourceFilter::SetThrottlingEnabled

HRESULT CDVRSourceFilter::GetThrottlingEnabled(BOOL *pfThrottle)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: GetThrottlingEnabled\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferPlayback> pFilter 
			= GetComponentInterface<IStreamBufferPlayback>();
		if (pFilter)
			return pFilter->GetThrottlingEnabled(pfThrottle);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetThrottlingEnabled error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetThrottlingEnabled error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: GetThrottlingEnabled dispatch complete\n")));
	return E_NOTIMPL;
} // CDVRSourceFilter::GetThrottlingEnabled

HRESULT CDVRSourceFilter::NotifyGraphIsConnected()
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: NotifyGraphIsConnected\n")));
	HRESULT hr = S_OK;

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		CComPtr<IReferenceClock> pClock;
		GetPreferredClock(&pClock);
		m_cClock.SetExternalClock(pClock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferPlayback> pFilter 
			= GetComponentInterface<IStreamBufferPlayback>();
		if (pFilter)
			hr = pFilter->NotifyGraphIsConnected();
		if (hr == E_NOTIMPL)
			hr = S_OK;
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: NotifyGraphIsConnected error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: NotifyGraphIsConnected error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: NotifyGraphIsConnected dispatch complete\n")));
	return hr;
} // CDVRSourceFilter::NotifyGraphIsConnected

HRESULT CDVRSourceFilter::IsAtLive(BOOL *pfAtLive)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: IsAtLive\n")));
	HRESULT hr = S_OK;

	try
	{
		// This method needs to be called from streaming threads so do **NOT**
		// grab the filter lock.
#if 0
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);
#endif // 0

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferPlayback> pFilter 
			= GetComponentInterface<IStreamBufferPlayback>();
		if (pFilter)
			hr = pFilter->IsAtLive(pfAtLive);
		if (hr == E_NOTIMPL)
			hr = S_OK;
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: IsAtLive error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: IsAtLive error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: IsAtLive dispatch complete\n")));
	return hr;
} // CDVRSourceFilter::IsAtLive

HRESULT CDVRSourceFilter::GetOffsetFromLive(LONGLONG *pllOffsetFromLive)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: GetOffsetFromLive\n")));
	HRESULT hr = S_OK;

	try
	{
		// This method needs to be called from streaming threads so do **NOT**
		// grab the filter lock.
#if 0
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);
#endif // 0

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferPlayback> pFilter 
			= GetComponentInterface<IStreamBufferPlayback>();
		if (pFilter)
			hr = pFilter->GetOffsetFromLive(pllOffsetFromLive);
		if (hr == E_NOTIMPL)
			hr = S_OK;
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetOffsetFromLive error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetOffsetFromLive error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: GetOffsetFromLive dispatch complete\n")));
	return hr;
} // CDVRSourceFilter::GetOffsetFromLive

HRESULT CDVRSourceFilter::SetDesiredOffsetFromLive(LONGLONG llOffsetFromLive)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: SetDesiredOffsetFromLive\n")));
	HRESULT hr = S_OK;

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferPlayback> pFilter 
			= GetComponentInterface<IStreamBufferPlayback>();
		if (pFilter)
			hr = pFilter->SetDesiredOffsetFromLive(llOffsetFromLive);
		if (hr == E_NOTIMPL)
			hr = S_OK;
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: SetDesiredOffsetFromLive error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: SetDesiredOffsetFromLive error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: SetDesiredOffsetFromLive dispatch complete\n")));
	return hr;
} // CDVRSourceFilter::SetDesiredOffsetFromLive

HRESULT CDVRSourceFilter::GetLoadIncarnation(DWORD *pdwLoadIncarnation)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: GetLoadIncarnation\n")));
	HRESULT hr = S_OK;

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferPlayback> pFilter 
			= GetComponentInterface<IStreamBufferPlayback>();
		if (pFilter)
			hr = pFilter->GetLoadIncarnation(pdwLoadIncarnation);
		if (hr == E_NOTIMPL)
			hr = S_OK;
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetLoadIncarnation error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetLoadIncarnation error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: GetLoadIncarnation dispatch complete\n")));
	return hr;
} // CDVRSourceFilter::GetLoadIncarnation

HRESULT CDVRSourceFilter::EnableBackgroundPriority(BOOL fEnableBackgroundPriority)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: EnableBackgroundPriority\n")));
	HRESULT hr = S_OK;

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<IStreamBufferPlayback> pFilter 
			= GetComponentInterface<IStreamBufferPlayback>();
		if (pFilter)
			hr = pFilter->EnableBackgroundPriority(fEnableBackgroundPriority);
		if (hr == E_NOTIMPL)
			hr = S_OK;
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: EnableBackgroundPriority error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: EnableBackgroundPriority error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: EnableBackgroundPriority dispatch complete\n")));
	return hr;
} // CDVRSourceFilter::EnableBackgroundPriority

//////////////////////////////////////////////////////////////////////////////
//	Private methods for ISourceVOBUInfo.									//
//////////////////////////////////////////////////////////////////////////////

HRESULT CDVRSourceFilter::GetVOBUOffsets(	LONGLONG tPresentation,
											LONGLONG tSearchStartBound,
											LONGLONG tSearchEndBound,
											VOBU_SRI_TABLE *pVobuTable)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: GetVOBUOffsets\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<ISourceVOBUInfo> pFilter 
			= GetComponentInterface<ISourceVOBUInfo>();
		if (pFilter)
			return pFilter->GetVOBUOffsets(tPresentation, tSearchStartBound, tSearchEndBound, pVobuTable);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetVOBUOffsets error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetVOBUOffsets error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: GetVOBUOffsets dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::GetRecordingSize(	LONGLONG tStart,
											LONGLONG tEnd,
											ULONGLONG* pcbSize)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: GetRecordingSize\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<ISourceVOBUInfo> pFilter 
			= GetComponentInterface<ISourceVOBUInfo>();
		if (pFilter)
			return pFilter->GetRecordingSize(tStart, tEnd, pcbSize);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetRecordingSize error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetRecordingSize error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: GetRecordingSize dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::GetRecordingDurationMax(	LONGLONG tStart,
											        ULONGLONG cbSizeMax,
											        LONGLONG* ptDurationMax)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: GetRecordingDurationMax\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<ISourceVOBUInfo> pFilter 
			= GetComponentInterface<ISourceVOBUInfo>();
		if (pFilter)
			return pFilter->GetRecordingDurationMax(tStart, cbSizeMax, ptDurationMax);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetRecordingDurationMax error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetRecordingDurationMax error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: GetRecordingDurationMax dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::GetKeyFrameTime(	LONGLONG tTime,
											LONG nSkipCount,
											LONGLONG* ptKeyFrame)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: GetKeyFrameTime\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<ISourceVOBUInfo> pFilter 
			= GetComponentInterface<ISourceVOBUInfo>();
		if (pFilter)
			return pFilter->GetKeyFrameTime(tTime, nSkipCount, ptKeyFrame);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetKeyFrameTime error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetKeyFrameTime error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: GetKeyFrameTime dispatch complete\n")));
	return E_NOTIMPL;
}

HRESULT CDVRSourceFilter::GetRecordingData(	LONGLONG tStart,
											BYTE* rgbBuffer,
											ULONG cbBuffer,
											ULONG* pcbRead,
											LONGLONG* ptEnd)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: dispatching: GetRecordingData\n")));

	try
	{
		// Ensure that only 1 app thread is in the filter at a time
		CAutoLock cLock(m_pLock);

		// Use a pipeline component's implementation if possible.
		CComPtr<ISourceVOBUInfo> pFilter 
			= GetComponentInterface<ISourceVOBUInfo>();
		if (pFilter)
			return pFilter->GetRecordingData(tStart, rgbBuffer, cbBuffer, pcbRead, ptEnd);
	}
	catch (const std::exception& rcException)
	{
		UNUSED(rcException);
#ifdef UNICODE
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetRecordingData error: %S\n"),
				rcException.what()));
#else
		DbgLog((LOG_ERROR, 1, _T("DVR source filter: GetRecordingData error: %s\n"),
				rcException.what()));
#endif
		return E_FAIL;
	}

	DbgLog((LOG_USER_REQUEST, 3, _T("DVR source filter: GetRecordingData dispatch complete\n")));
	return E_NOTIMPL;
}

//////////////////////////////////////////////////////////////////////////////
//	Private methods for X Graph Clock Mgr.									//
//////////////////////////////////////////////////////////////////////////////

CDVRClock::CDVRClock(LPUNKNOWN pUnk, HRESULT *phr) 
	:	 CBaseReferenceClock(	TEXT("CDVRClock"),
								pUnk,
								phr),
		m_llOffsetFromCurrentClock(0),
		m_iClockRate(10000),
		m_llTimeAtLastRateChange(0),
		m_llTrueTimeAtLastRateChange(0),
		m_iCounter(0),
		m_llLastMTDelta_mSec(0),
		m_llMTDeltaAccumulator(0),
		m_llNextCheck(-1),
		m_llInitialPTSDelta(0),
		m_pConsumer(0),
		m_fHitClockGlitch(false)
{
	// As of the constructor, we are using the system clock with no offset
}

void CDVRClock::SetSampleConsumer(ISampleConsumer *pSampleConsumer)
{
	m_pConsumer = pSampleConsumer;
}

void CDVRClock::SetExternalClock(IReferenceClock *pClock)
{
	DEBUGMSG(1, (L"** CDVRClock::SetExternalClock 0x%08x.\n", pClock));
	CAutoLock cObjectLock(this);

	if (pClock != m_pExternalClock)
	{
		m_fHitClockGlitch = false;
		LONGLONG llLastOldClockTime;
		if (m_pExternalClock)
			m_pExternalClock->GetTime(&llLastOldClockTime);
		else
			llLastOldClockTime = CBaseReferenceClock::GetPrivateTime();

		m_pExternalClock.Release();
		m_pExternalClock = pClock;

		LONGLONG llFirstNewClockTime;
		if (m_pExternalClock)
			m_pExternalClock->GetTime(&llFirstNewClockTime);
		else
			llFirstNewClockTime = CBaseReferenceClock::GetPrivateTime();

		m_llOffsetFromCurrentClock = (llLastOldClockTime + m_llOffsetFromCurrentClock) - llFirstNewClockTime;
	}
}

void CDVRClock::SetClockRate(int iRate)
{
    // Caution:  don't notify the clock-rate event -- it can cause a deadlock due to
    // out-of-order acquisition of the CReferenceClock lock and the decoder driver state
    // lock.

	if (m_iClockRate != iRate)
	{
		CAutoLock cObjectLock(this);
		m_llTimeAtLastRateChange = GetPrivateTime();
		m_llTrueTimeAtLastRateChange = GetTruePrivateTime();
		m_iClockRate = iRate;
		GetPrivateTime();

		// A clock rate outside the range of 1.00 +/- 1% leads to
		// bad playback.  An external assist in correcting the situation
		// is advisable.

		if ((iRate < MIN_ACCEPTABLE_CLOCK_RATE) || (iRate > MAX_ACCEPTABLE_CLOCK_RATE))
		{
			if (!m_fHitClockGlitch)
			{
				DbgLog((LOG_WARNING, 3, TEXT("CDVRClock::SetClockRate():  clock is outside desirable range [%lf]\n"),
							((double)iRate) / 10000.0 ));
				m_fHitClockGlitch = true;
			}
		}
		else if (m_fHitClockGlitch)
		{
			DbgLog((LOG_WARNING, 3, TEXT("CDVRClock::SetClockRate():  clock appears to have recovered\n") ));
			m_fHitClockGlitch = false;
		}
	}

	// Reset our couter and totals
	m_iCounter = 0;
	m_llMTDeltaAccumulator = 0;
}

void CDVRClock::ResetRateAdjustment()
{
	CAutoLock cObjectLock(this);

	if (!m_pConsumer)
	{
		ASSERT(FALSE);
		return;
	}

	RETAILMSG(LOG_CUSTOM1, (L"* CDVRClock rate adjustment resetting.\n"));

	// Make sure we have the latest clock times
	m_llTimeAtLastRateChange = GetPrivateTime();
	m_llTrueTimeAtLastRateChange = GetTruePrivateTime();

	// Reset our comparison state
	m_iClockRate = 10000;
	m_iCounter = 0;
	m_llLastMTDelta_mSec = 0;
	m_llMTDeltaAccumulator = 0;
	m_fHitClockGlitch = false;
}

REFERENCE_TIME CDVRClock::GetTruePrivateTime()
{
	LONGLONG llClockTime;

 	if (m_pExternalClock)
		m_pExternalClock->GetTime(&llClockTime);
	else
		llClockTime	= CBaseReferenceClock::GetPrivateTime();
	
	return llClockTime + m_llOffsetFromCurrentClock;
}

REFERENCE_TIME CDVRClock::GetPrivateTime()
{
    CAutoLock cObjectLock(this);

	ASSERT(m_pConsumer);

	// Apply our rate factor - I just did the math and we're not in danger of an overflow
	// caused by '* m_iClockRate' until after about 2100 days.  If we can get that kind
	// of uptime then we're in good shape :)
	LONGLONG llClockTime = m_llTimeAtLastRateChange + 
			((GetTruePrivateTime() - m_llTrueTimeAtLastRateChange) * m_iClockRate / 10000);

	// Our interval between checks doesn't go through clock slaving -- we need a steady
	// clock whose time isn't going to fluctuate due to the rendering or capture issues
	// of the clock we may be slaving to. Feedback from the fluctuations into both the
	// slaving rate and the frequency at which we correct the slaving rate can prevent
	// timely sync'ing up against the current drift rate:
	LONGLONG llClockCheckTime = CBaseReferenceClock::GetPrivateTime();

	// How often we should check the PTS delta (1/4 second)
	static const LONGLONG llUpdateInterval = UNITS/4;
	static const int iNumIntervalsToTest = 40;
	static const int iTestingDurationSecs = (int) (llUpdateInterval * iNumIntervalsToTest / UNITS);

	// Keep the time when the next check should occur
	if (m_llNextCheck == -1)
		m_llNextCheck = llClockCheckTime + llUpdateInterval;

	if (llClockCheckTime > m_llNextCheck)
	{
		// Increment our counter and the next time we check
		m_iCounter++;
		m_llNextCheck = llClockCheckTime + llUpdateInterval;

		// Update our total delta
		ASSERT(m_pConsumer);
		LONGLONG llMostRecentSampleTime = m_pConsumer->GetMostRecentSampleTime();

		if (llMostRecentSampleTime == -1)
		{
			// We don't have a clock source to sync to.  So reset everything
			// and schedule the next time to check in 10 seconds.
			m_iClockRate = 10000;
			m_iCounter = 0;
			m_llLastMTDelta_mSec = 0;
			m_llMTDeltaAccumulator = 0;
			m_llInitialPTSDelta = 0;
			m_llNextCheck = llClockTime + UNITS * 10;
			m_fHitClockGlitch = false;
			RETAILMSG(LOG_CUSTOM1, (L"* DVR Source clock 1.000 since no clock to sync to.\n"));
		}
		else
		{
			m_llMTDeltaAccumulator += llClockTime - llMostRecentSampleTime;

			if (m_iCounter == iNumIntervalsToTest)
			{
				// Turn our total delta into an average
				m_llMTDeltaAccumulator /= m_iCounter;

				// Turn our average into millisecs
				m_llMTDeltaAccumulator /= 10000LL;

				// If this is the first set of data then set our
				// initial delta
				if (m_llLastMTDelta_mSec == 0)
				{
					m_llInitialPTSDelta = m_llMTDeltaAccumulator;
					m_llLastMTDelta_mSec = m_llMTDeltaAccumulator;
				}

				LONGLONG llDrift = m_llMTDeltaAccumulator - m_llInitialPTSDelta;

				// Calculate the new rate as follows
				// This is the rate that we run at to eliminate all the drift over
				// the next testing loop
				int m_iNewRate = (int) (10000 - 10 * llDrift / iTestingDurationSecs);
				// Add a bit of smoothing:
				m_iNewRate = (m_iNewRate + m_iClockRate) / 2;

				// Don't let our clock stop or *gasp* run backwards
				m_iNewRate = max(100, m_iNewRate);

				RETAILMSG(1, (L"Old Rate:%01d.%04d New Rate:%01d.%04d Rebased:\t%d\tDelta:\t%d\n",
					m_iClockRate / 10000,
					m_iClockRate % 10000,
					m_iNewRate / 10000,
					m_iNewRate % 10000,
					(int) (llDrift),
					(int) (m_llMTDeltaAccumulator - m_llLastMTDelta_mSec)));

				ASSERT(m_iNewRate > 1000);

				// Save the average for the next run
				m_llLastMTDelta_mSec = m_llMTDeltaAccumulator;

				SetClockRate(m_iNewRate);
			}
			// FYI - The call to SetClockRate cleared our counter and totals.

		} // endif (llMostRecentSampleTime != -1)

	} // endif (llClockTime > m_llNextCheck)

	return llClockTime;
}

template<class T> HRESULT CDVRSourceFilter::FindFilterInterface(
	T **ppInterface,
	REFIID riid)
{
	// Code shamelessly ganked from the DXSDK (though the templatizing was mine :)

	if (!ppInterface)
		return E_POINTER;

	// Get the graph builder from the filter graph
	CComQIPtr<IGraphBuilder> pGraph = m_pGraph;
	if (!pGraph)
		return E_FAIL;

	HRESULT hr = E_FAIL;
	IEnumFilters *pEnum = NULL;
	IBaseFilter *pF = NULL;
	if (FAILED(pGraph->EnumFilters(&pEnum)))
		return E_FAIL;

	// Query every filter for the interface.
	while (S_OK == pEnum->Next(1, &pF, 0))
	{
		// Don't find ourself
		if (pF != (IBaseFilter *) this)
			hr = pF->QueryInterface(riid, (void **) ppInterface);
		pF->Release();
		if (SUCCEEDED(hr))
			break;
	}

	pEnum->Release();
	return hr;
}

HRESULT CDVRSourceFilter::GetPreferredClock(IReferenceClock** ppIReferenceClock)
{
	CComQIPtr<IReferenceClock> pClock;

	// Validate input
	if (ppIReferenceClock == NULL)
	{
		return E_INVALIDARG;
	}
	*ppIReferenceClock = NULL;

	// Look for the audio renderer in this graph
#ifdef UNDER_CE
	CComPtr<IAudioRenderer> pAudioRenderer;
	if (SUCCEEDED(FindFilterInterface<IAudioRenderer>(&pAudioRenderer)))
#else
	CComPtr<IAMAudioRendererStats> pAudioRenderer;
	if (SUCCEEDED(FindFilterInterface<IAMAudioRendererStats>(&pAudioRenderer)))
#endif
	{
		// Fantastic!  Now hopefully the audio renderer should have a clock
		pClock = pAudioRenderer;
		if (pClock)
		{
			*ppIReferenceClock = pClock;
			(*ppIReferenceClock)->AddRef();
			return S_OK;
		}
	}

	// Problem, no audio renderer or else it didn't have a clock. 
	// So try and find another clock in the graph.
	if (SUCCEEDED(FindFilterInterface<IReferenceClock>(&pClock)))
	{
		*ppIReferenceClock = pClock;
		(*ppIReferenceClock)->AddRef();
		return S_OK;
	}

	// Dang - no suitable clock found.
	return E_FAIL;
}

HRESULT CDVRSourceFilter::GetTime(REFERENCE_TIME *pRT)
{
	IReferenceClock *pClock = m_pClock;
	if (!pClock)
		return E_FAIL;
	return pClock->GetTime(pRT);
}

}
