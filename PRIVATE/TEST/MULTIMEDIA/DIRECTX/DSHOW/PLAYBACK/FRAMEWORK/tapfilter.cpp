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
////////////////////////////////////////////////////////////////////////////////

#include <streams.h>
#include "globals.h"
#include "logging.h"
#include "TapFilter.h"
#include "utility.h"

#define STRICTMT 0
#define ENABLE_ENTRYEXIT_LOG 0

class EntryExitLogger
{
public:
	const char* m_szFunction;
	HRESULT* m_phr;

	EntryExitLogger(const char* function, HRESULT* phr = NULL) : m_szFunction (function), m_phr(phr)
	{
#if ENABLE_ENTRYEXIT_LOG 
		LOG(TEXT("%S: entry \n"), m_szFunction);
#endif
	}

	~EntryExitLogger()
	{
#if ENABLE_ENTRYEXIT_LOG 
		if (m_phr)
            LOG(TEXT("%S: exit 0x%x \n"), m_szFunction, *m_phr);
		else LOG(TEXT("%S: exit\n"), m_szFunction);
#endif
	}
};

#define ENTRYEXITLOG(phr) EntryExitLogger entryexitlogger(__FUNCTION__, phr);

CAsyncReaderPassThru::CAsyncReaderPassThru(IAsyncReader* pPeerAsync, IPin* pPin, LPUNKNOWN pUnknown)
	: CUnknown(TEXT("Async Reader Pass Thru"), pUnknown) 
{
	ENTRYEXITLOG(NULL);
	m_pPeerAsync = pPeerAsync;
	m_pPin = pPin;
}

CAsyncReaderPassThru::~CAsyncReaderPassThru()
{
	ENTRYEXITLOG(NULL);
	if (m_pPeerAsync)
		m_pPeerAsync->Release();
}

HRESULT CAsyncReaderPassThru::BeginFlush()
{
	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);
	hr = m_pPeerAsync->BeginFlush();
	return hr;
}

HRESULT CAsyncReaderPassThru::EndFlush()
{
	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);
	hr = m_pPeerAsync->EndFlush();
	return hr;
}

HRESULT CAsyncReaderPassThru::Length(LONGLONG *pTotal, LONGLONG *pAvailable)
{
	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);
	hr = m_pPeerAsync->Length(pTotal, pAvailable);
	return hr;
}

HRESULT CAsyncReaderPassThru::RequestAllocator(IMemAllocator *pPreferred, ALLOCATOR_PROPERTIES *pProps, IMemAllocator **ppActual)
{
	HRESULT hr = S_OK;	
	ENTRYEXITLOG(&hr);
	hr = m_pPeerAsync->RequestAllocator(pPreferred, pProps, ppActual);
	return hr;
}

HRESULT CAsyncReaderPassThru::Request(IMediaSample *pSample, DWORD_PTR dwUser)
{
	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);

	// For PULL MODEL, we get startup latency here.  This is when WAVE PARSER
	// requests the first sample.
	CTapFilterOutputPin *pPin = (CTapFilterOutputPin *)m_pPin;
	if ( !pPin ) return E_FAIL;
	CTapFilter *pTapFilter = pPin->GetFilter();
	if ( !pTapFilter ) return E_FAIL;

	// Trigger the graph event
	GraphSample event;
	// Try getting the reference clock and the arrival time from that
	IReferenceClock* pRefClock = NULL;
	hr = pTapFilter->GetSyncSource(&pRefClock);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to get reference clock (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
		hr = S_OK;
	}
	if (pRefClock)
	{
		hr = pRefClock->GetTime(&event.ats);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to get the arrival time (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			hr = S_OK;
			event.ats = 0;
		}
		pRefClock->Release();
	}
	else event.ats = 0;

	// Store the sample
	event.pMediaSample = pSample;

	// Trigger the graph event
	pTapFilter->TriggerGraphEvent(SAMPLE, (void*)&event);

	hr = m_pPeerAsync->Request(pSample, dwUser);
	return hr;
}

HRESULT CAsyncReaderPassThru::SyncReadAligned(IMediaSample *pSample)
{
	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);
	hr = m_pPeerAsync->SyncReadAligned(pSample);
	return hr;
}

HRESULT CAsyncReaderPassThru::SyncRead(LONGLONG llPosition, LONG lLength, BYTE *pBuffer)
{
	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);
	hr = m_pPeerAsync->SyncRead(llPosition, lLength, pBuffer);
	return hr;
}

HRESULT CAsyncReaderPassThru::WaitForNext(DWORD dwTimeout, IMediaSample **ppSample, DWORD_PTR *pdwUser)
{
	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);
	hr = m_pPeerAsync->WaitForNext(dwTimeout, ppSample, pdwUser);
	return hr;
}

HRESULT CAsyncReaderPassThru::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);
	
	if (!ppv)
	{
		hr = E_POINTER;
		return hr;
	}

    ValidateReadWritePtr(ppv,sizeof(PVOID));

    if (riid == IID_IAsyncReader) {
		hr = GetInterface((IBaseFilter *) this, ppv);
	}
	else {
		hr = CUnknown::NonDelegatingQueryInterface(riid, ppv);
	}
	return hr;
}

CTapFilterInputPin::CTapFilterInputPin( CTapFilter* pFilter, CCritSec* pLoc, HRESULT* phr, TCHAR* szPinName ) :
    CBaseInputPin( szPinName, pFilter, pLoc, phr, L"TapFilterInputPin"),
	m_pTapFilter(pFilter),
	m_pTappedMemInputPin(NULL)
{
	ENTRYEXITLOG(NULL);
}


CTapFilterInputPin::~CTapFilterInputPin()
{
	ENTRYEXITLOG(NULL);
}

HRESULT CTapFilterInputPin::NonDelegatingQueryInterface(REFGUID riid, void **ppv)
{
	HRESULT hr = E_NOINTERFACE;
	ENTRYEXITLOG(&hr);
	
	// INonDelegatingUnknown, IPin, IQualityControl, IMemInputPin
	hr = CBaseInputPin::NonDelegatingQueryInterface(riid, ppv);
    if (SUCCEEDED_F(hr))
        return hr;

	// BUGBUG: for interfaces that we don't know about, I will forward the request to m_TappedOutputPin - is this a problem?
	// This is for the case where the m_pTappedInputPin or the filter at the TapFilter's input is QI'ng for a special interface
	// This will also have an impact if the filter graph queries the pins for an interface in which case we will forward the interface from the connected pin 
    if (m_pTappedInputPin && m_Connected)
	{
		hr = m_pTappedInputPin->QueryInterface(riid, ppv);
	}

	return hr;
}

STDMETHODIMP CTapFilterInputPin::Receive( IMediaSample* pSample )
{
	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);

	// If we are either not connected or the tapped input pin doesn't have a suitable transport
	if (!m_Connected || !m_pTappedMemInputPin)
	{
		hr = E_FAIL;
		return hr;
	}

	// Trigger the graph event
	GraphSample event;

	// Try getting the reference clock and the arrival time from that
	IReferenceClock* pRefClock = NULL;
	hr = m_pTapFilter->GetSyncSource(&pRefClock);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to get reference clock (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
		hr = S_OK;
	}
	if (pRefClock)
	{
		hr = pRefClock->GetTime(&event.ats);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to get the arrival time (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			hr = S_OK;
			event.ats = 0;
		}
		pRefClock->Release();
	}
	else event.ats = 0;

	// Store the sample
	event.pMediaSample = pSample;

	// Trigger the graph event
	hr = m_pTapFilter->TriggerGraphEvent(SAMPLE, (void*)&event);
	if((HRESULT)TF_S_DROPSAMPLE == hr)
	{
		// pSample->Release();
		hr = S_OK;
	}
	else
	{
		// forward the sample
		hr = m_pTappedMemInputPin->Receive(pSample);
	}
	return hr;
}


HRESULT CTapFilterInputPin::GetAllocatorRequirements(ALLOCATOR_PROPERTIES *pProps)
{
	HRESULT hr = S_OK;	
	ENTRYEXITLOG(&hr);
	
	if (m_pTappedMemInputPin)
		hr = m_pTappedMemInputPin->GetAllocatorRequirements(pProps);
	else hr = E_FAIL;

	return hr;
}

HRESULT CTapFilterInputPin::GetAllocator(IMemAllocator** ppAllocator)
{
	HRESULT hr = S_OK;	
	ENTRYEXITLOG(&hr);

	if (m_pTappedMemInputPin)
		hr = m_pTappedMemInputPin->GetAllocator(ppAllocator);
	else hr = E_FAIL;

	return hr;
}

HRESULT CTapFilterInputPin::NotifyAllocator(IMemAllocator *ppAllocator, BOOL bReadOnly)
{
	HRESULT hr = S_OK;	
	ENTRYEXITLOG(&hr);
	
	if (m_pTappedMemInputPin)
		hr = m_pTappedMemInputPin->NotifyAllocator(ppAllocator, bReadOnly);
	else hr = E_FAIL;

	return hr;
}

HRESULT CTapFilterInputPin::QueryAccept(const AM_MEDIA_TYPE* pmt)
{
	HRESULT hr = S_OK;	
	ENTRYEXITLOG(&hr);
#if STRICTMT
	CMediaType mt(m_mt);
	hr = (mt == *pmt) ? S_OK : E_FAIL;
#else
	// Forward call to tapped Input
	hr = m_pTappedInputPin->QueryAccept(pmt);
	if (SUCCEEDED(hr))
	{
		FreeMediaType(m_mt);
		CopyMediaType(&m_mt, pmt); 
	}
#endif
	return hr;
}


HRESULT CTapFilterInputPin::CheckMediaType(const CMediaType* pmt)
{
	HRESULT hr = S_OK;	
	ENTRYEXITLOG(&hr);

#if STRICTMT
	CMediaType mt(m_mt);
	hr = (mt == *pmt) ? S_OK : E_FAIL;
#else
	// Forward call to tapped input
	hr = m_pTappedInputPin->QueryAccept(pmt);
#endif
	return hr;
}

HRESULT CTapFilterInputPin::GetMediaType(int iPosition, CMediaType *pMediaType)
{
	HRESULT hr = S_OK;	
	ENTRYEXITLOG(&hr);

	if (!pMediaType)
	{
		hr = E_INVALIDARG;
		return hr;
	}
    
#if STRICTMT
    if (iPosition < 0)
		hr = E_INVALIDARG;
	else if (iPosition > 0)
		hr = VFW_S_NO_MORE_ITEMS;
	else 
		*pMediaType = m_mt;
#else
	// Forward this call to the tapped input
	IEnumMediaTypes *pEnumMediaTypes = NULL;
	hr = m_pTappedInputPin->EnumMediaTypes(&pEnumMediaTypes);
	if (FAILED(hr))
		return hr;
	
	// Reset the enumerator
	hr = pEnumMediaTypes->Reset();
	if (FAILED_F(hr)) {
		//if we are going to leave, make sure we clean up first
		pEnumMediaTypes->Release();		
		return hr;
	}

	// Skip over to the position
	if (iPosition)
	{
		hr = pEnumMediaTypes->Skip(iPosition);
		if (FAILED_F(hr)) {
			//if we are going to leave, make sure we clean up first
			pEnumMediaTypes->Release();			
			return hr;
		}
	}

	ULONG cFetched = 0;
	AM_MEDIA_TYPE* pMT;
	hr = pEnumMediaTypes->Next(1, &pMT, &cFetched);
	if (FAILED_F(hr)) {
		//if we are going to leave, make sure we clean up first
		pEnumMediaTypes->Release();			
		return hr;
	}

	// Assign back to callers pointer
	if (pMediaType)
		*pMediaType = CMediaType(*pMT);

	// BUGBUG: release other places
	pEnumMediaTypes->Release();
#endif
	return hr;
}

HRESULT CTapFilterInputPin::SetConnectionParameters(CTapFilterOutputPin* pTapFilterOutputPin, IPin* pFromPin, IPin* pToPin, AM_MEDIA_TYPE* pMediaType)
{
	HRESULT hr = S_OK;	
	ENTRYEXITLOG(&hr);

	// Save the pin pointers
	m_pTappedInputPin = pToPin;
	m_pTappedOutputPin = pFromPin;
	m_pTapFilterOutputPin = pTapFilterOutputPin;
	
	// Set the base class media type variable
	CopyMediaType(&m_mt, pMediaType);
	
	return S_OK;
}

HRESULT CTapFilterInputPin::UpdateMediaType(AM_MEDIA_TYPE* pmt)
{
	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);

	if (m_Connected)
	{
		FreeMediaType(m_mt);
		CopyMediaType(&m_mt, pmt);
	}
	else hr = E_FAIL;
	return hr;
}

HRESULT CTapFilterInputPin::ReceiveConnection(IPin *pConnector, const AM_MEDIA_TYPE *pmt)
{
	HRESULT hr = S_OK;	
	ENTRYEXITLOG(&hr);

	hr = m_pTappedInputPin->ReceiveConnection(m_pTapFilterOutputPin, pmt);
	if (SUCCEEDED(hr))
	{
		CompleteConnect(pmt);
	}
	return hr;
}

HRESULT CTapFilterInputPin::CompleteConnect(const AM_MEDIA_TYPE* pMediaType)
{
	HRESULT hr = S_OK;	
	ENTRYEXITLOG(&hr);

	// If the tapped input pin has the IMemInputPin interface, acquire it
	hr = m_pTappedInputPin->QueryInterface(IID_IMemInputPin, (void**)&m_pTappedMemInputPin);

	// Set the base class media type var
	CopyMediaType(&m_mt, pMediaType);

	// Set the output pin as connected since we do the connections simultaneously
	m_pTapFilterOutputPin->SetInputConnected(true, pMediaType);

	// Set the base class variable
	m_Connected = m_pTappedOutputPin;

	return hr;
}

HRESULT CTapFilterInputPin::Disconnect()
{
	HRESULT hr = S_OK;	
	ENTRYEXITLOG(&hr);

	m_Connected = NULL;
	m_pTapFilterOutputPin->SetInputConnected(false);
	
	//since we got this in CompleteConnection, this would be the place to release it
	if(m_pTappedMemInputPin)
		m_pTappedMemInputPin->Release();
	return hr;
}

HRESULT CTapFilterInputPin::BeginFlush()
{
	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);

	// Trigger the graph event
	GraphBeginFlush event;
	IReferenceClock* pRefClock = NULL;
	hr = m_pTapFilter->GetSyncSource(&pRefClock);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to get reference clock (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
		hr = S_OK;
	}

	if (pRefClock)
	{
		hr = pRefClock->GetTime(&event.ats);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to get the arrival time (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			event.ats = 0;
			hr = S_OK;
		}
	}
	else event.ats = 0;

	m_pTapFilter->TriggerGraphEvent(BEGIN_FLUSH, (void*)&event);

	// Forward the flush
	hr = m_pTappedInputPin->BeginFlush();
	CBaseInputPin::BeginFlush();

	if(pRefClock)
		pRefClock->Release();
	
	return hr;
}

HRESULT CTapFilterInputPin::EndFlush()
{
	// Trigger the graph event
	GraphEndFlush event;
	HRESULT hr = S_OK;
	IReferenceClock* pRefClock = NULL;
	
	hr = m_pTapFilter->GetSyncSource(&pRefClock);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to get reference clock (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
		hr = S_OK;
	}

	if (pRefClock)
	{
		hr = pRefClock->GetTime(&event.ats);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to get the arrival time (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			hr = S_OK;
			event.ats = 0;
		}
	}
	else event.ats = 0;

	m_pTapFilter->TriggerGraphEvent(END_FLUSH, (void*)&event);

	if(pRefClock)
		pRefClock->Release();

	// Forward the end flush
	CBaseInputPin::EndFlush();
	return m_pTappedInputPin->EndFlush();
}

HRESULT CTapFilterInputPin::EndOfStream()
{
	// Trigger the graph event
	GraphEndOfStream event;
	HRESULT hr = S_OK;
	IReferenceClock* pRefClock = NULL;

	hr = m_pTapFilter->GetSyncSource(&pRefClock);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to get reference clock (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
		hr = S_OK;
	}

	if (pRefClock)
	{
		hr = pRefClock->GetTime(&event.ats);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to get the arrival time (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			hr = S_OK;
			event.ats = 0;
		}
	}
	else event.ats = 0;

	m_pTapFilter->TriggerGraphEvent(EOS, (void*)&event);

	if(pRefClock)
		pRefClock->Release();

	// Forward the EOS
	return m_pTappedInputPin->EndOfStream();
}

HRESULT CTapFilterInputPin::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
	// Trigger the graph event
	GraphNewSegment event;
	HRESULT hr = S_OK;
	IReferenceClock* pRefClock = NULL;
	hr = m_pTapFilter->GetSyncSource(&pRefClock);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to get reference clock (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
		hr = S_OK;
	}

	if (pRefClock)
	{
		hr = pRefClock->GetTime(&event.ats);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to get the arrival time (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			hr = S_OK;
			event.ats = 0;
		}
	}
	else event.ats = 0;
	event.start = tStart;
	event.stop = tStop;
	event.rate = dRate;
	m_pTapFilter->TriggerGraphEvent(NEW_SEG, (void*)&event);

	if(pRefClock)
		pRefClock->Release();

	// Forward the new segment
	return m_pTappedInputPin->NewSegment(tStart, tStop, dRate);
}

CTapPosPassThru::CTapPosPassThru(const TCHAR *pName, LPUNKNOWN pUnk, HRESULT *phr, IPin *pPin) :
	CPosPassThru(pName, pUnk, phr, pPin)
{
}

HRESULT CTapPosPassThru::get_Rate(double *pRate)
{
	return CPosPassThru::get_Rate(pRate);
}

HRESULT CTapPosPassThru::get_Duration(REFTIME *pDuration)
{
	return CPosPassThru::get_Duration(pDuration);
}

CTapFilterOutputPin::CTapFilterOutputPin( CTapFilter* pFilter, CCritSec* pLoc, HRESULT* phr, TCHAR* szPinName ) :
    CBaseOutputPin( szPinName, pFilter, pLoc, phr, L"TapFilterOutputPin"),
	m_pTapFilter(pFilter)
{
	m_pPosPassThru = NULL;
	m_pAsyncReader = NULL;
}

CTapFilterOutputPin::~CTapFilterOutputPin()
{
	// free allocated resource.
	if ( m_pPosPassThru ) 
		delete m_pPosPassThru;
	if ( m_pAsyncReader ) 
		delete m_pAsyncReader;

	m_pPosPassThru = NULL;
	m_pAsyncReader = NULL;
}

HRESULT CTapFilterOutputPin::NonDelegatingQueryInterface(REFGUID riid, void **ppv)
{
    if (SUCCEEDED(CBaseOutputPin::NonDelegatingQueryInterface(riid, ppv)))
        return NOERROR;

	// This is a little tricky
	// If we get queried for a IMediaPosition or IMediaSeeking, defer to the CPosPassThru object
	if ((riid == IID_IMediaPosition) || (riid == IID_IMediaSeeking)) {
        CAutoLock l(&m_csPassThru);
        // Create a CPosPassThru if we don't have one already
        if (!m_pPosPassThru) {
            if (m_pTapFilterInputPin) {
                HRESULT hr = S_OK;
                m_pPosPassThru = new CTapPosPassThru(TEXT("TapFilter PosPassThru"),
                                                (IPin*)this,
                                                &hr,
                                                m_pTapFilterInputPin);
                if (m_pPosPassThru && (FAILED(hr))) {
                    delete m_pPosPassThru;
                    m_pPosPassThru = NULL;
                }
            }
        }

        if (m_pPosPassThru) {
            return m_pPosPassThru->NonDelegatingQueryInterface(riid, ppv);
        }
    }
	else if (riid == IID_IAsyncReader)
	{
		if (!m_pAsyncReader)
		{
			IAsyncReader* pPeerAsync = NULL;
			HRESULT hr = m_pTappedOutputPin->QueryInterface(riid, (void**)&pPeerAsync);
			if (FAILED(hr))
				return hr;
			// BUGBUG:
			m_pAsyncReader = new CAsyncReaderPassThru(pPeerAsync, this, reinterpret_cast<IUnknown*>(this));
		}
		return m_pAsyncReader->NonDelegatingQueryInterface(riid, ppv);;
	}
	// BUGBUG: But for any other interface, I will forward the request to m_TappedOutputPin. This is for the case where the m_pTappedInputPin or the filter at the TapFilter's output is QI'ng for a special interface such as AsyncReader/ISplitterInfo/ISplitterTiming
	// BUGBUG: I'm not even checking if we have connected because this can come during the connection. This will also have an impact if the filter graph queries the pins for an interface in which case we will forward the interface from the connected pin - is this a problem?
    else if (m_pTappedOutputPin)
	{
		return m_pTappedOutputPin->QueryInterface(riid, ppv);
	}

	return E_NOINTERFACE;
}

HRESULT CTapFilterOutputPin::QueryAccept(const AM_MEDIA_TYPE* pmt)
{
	HRESULT hr = S_OK;	
	ENTRYEXITLOG(&hr);

	// Get the arrival time-stamp
	GraphQueryAccept event;
	IReferenceClock* pRefClock = NULL;
	hr = m_pTapFilter->GetSyncSource(&pRefClock);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to get reference clock (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
		hr = S_OK;
	}

	if (pRefClock)
	{
		hr = pRefClock->GetTime(&event.ats);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to get the arrival time (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			hr = S_OK;
		}
	}
	else event.ats = 0;

	// Check if we accept this media type change
	bool bAccept = false;
#if STRICTMT
	CMediaType mt(m_mt);
	hr = (mt == *pmt) ? S_OK : E_FAIL;
	bAccept = (hr == S_OK);
#else
	// Forward call to tapped output and save the media type if successful
	// This is a special case where the VMR changes the mediatype in NotifyAllocator and communicates this upstream through a QueryAccept
	hr = m_pTappedOutputPin->QueryAccept(pmt);
	if (m_Connected && (hr == S_OK))
	{
		bAccept = true;
		FreeMediaType(m_mt);
		CopyMediaType(&m_mt, pmt);

		hr = m_pTapFilterInputPin->UpdateMediaType(&m_mt);
	}
#endif
	
	// Trigger the graph event
	event.bAccept = bAccept;
	event.pMediaType = pmt;
	m_pTapFilter->TriggerGraphEvent(QUERY_ACCEPT, &event);

	if(pRefClock)
		pRefClock->Release();

	return hr;
}

HRESULT CTapFilterOutputPin::CheckMediaType(const CMediaType* pmt)
{
	HRESULT hr = S_OK;	
	ENTRYEXITLOG(&hr);
#if STRICTMT
	CMediaType mt(m_mt);
	hr = (mt == *pmt) ? S_OK : E_FAIL;
#else
	// Forward call to tapped output
	hr = m_pTappedOutputPin->QueryAccept(pmt);
#endif
	return hr;
}

HRESULT CTapFilterOutputPin::GetMediaType(int iPosition, CMediaType *pMediaType)
{
	HRESULT hr = S_OK;	
	ENTRYEXITLOG(&hr);

	if (!pMediaType)
		return E_INVALIDARG;
    
#if STRICTMT
    if (iPosition < 0)
		hr = E_INVALIDARG;
	else if (iPosition > 0)
		hr = VFW_S_NO_MORE_ITEMS;
	else 
		*pMediaType = m_mt;
#else
	// Forward this call to the tapped output
	IEnumMediaTypes *pEnumMediaTypes = NULL;
	hr = m_pTappedOutputPin->EnumMediaTypes(&pEnumMediaTypes);
	if (FAILED(hr))
		return hr;
	
	// Reset the enumerator
	hr = pEnumMediaTypes->Reset();
	if (FAILED_F(hr))
		return hr;

	// Skip over to the position
	if (iPosition)
	{
		hr = pEnumMediaTypes->Skip(iPosition);
		if (FAILED_F(hr))
			return hr;
	}

	ULONG cFetched = 0;
	AM_MEDIA_TYPE* pMT;
	hr = pEnumMediaTypes->Next(1, &pMT, &cFetched);
	if (FAILED_F(hr))
		return hr;

	// Assign back to callers pointer
	*pMediaType = CMediaType(*pMT);

	pEnumMediaTypes->Release();
#endif
	return hr;
}
HRESULT CTapFilterOutputPin::ReceiveConnection(IPin *pConnector, const AM_MEDIA_TYPE *pmt)
{
	HRESULT hr = E_FAIL;	
	ENTRYEXITLOG(&hr);
	return hr;
}

HRESULT CTapFilterOutputPin::Connect(IPin* pReceivePin, const AM_MEDIA_TYPE* pmt)
{
	HRESULT hr = S_OK;	
	ENTRYEXITLOG(&hr);
	// Are we set as connected?
	hr = (m_Connected == NULL) ? E_FAIL : S_OK;
	return hr;
}

HRESULT CTapFilterOutputPin::Disconnect()
{
	HRESULT hr = S_OK;	
	ENTRYEXITLOG(&hr);
	// Are we set as disconnected?
	hr = (m_Connected == NULL) ? S_OK : S_FALSE;
	return hr;
}

HRESULT CTapFilterOutputPin::SetInputConnected(bool connected, const AM_MEDIA_TYPE* pMediaType)
{
	HRESULT hr = S_OK;	
	ENTRYEXITLOG(&hr);

	m_Connected = (connected) ? m_pTappedInputPin : NULL;
	if (connected)
		CopyMediaType(&m_mt, pMediaType);

	return hr;
}

HRESULT CTapFilterOutputPin::DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES *pProp)
{
	HRESULT hr = S_OK;	
	ENTRYEXITLOG(&hr);

	if (pProp && 
		((pProp->cbAlign != 0) || (pProp->cbBuffer != 0) || (pProp->cbPrefix != 0) || (pProp->cBuffers != 0)))
		hr = S_OK;
	else hr = E_NOTIMPL;
	return hr;
}

HRESULT CTapFilterOutputPin::SetConnectionParameters(CTapFilterInputPin* pTapFilterInputPin, IPin* pFromPin, IPin* pToPin, AM_MEDIA_TYPE* pMediaType)
{
	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);
	
	m_pTappedInputPin = pToPin;
	m_pTappedOutputPin = pFromPin;
	m_pTapFilterInputPin = pTapFilterInputPin;

	// Set the base class media type var
	CopyMediaType(&m_mt, pMediaType);
	return hr;
}

HRESULT CTapFilterOutputPin::Notify(IBaseFilter * pSender, Quality q)
{
	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);
	if (m_pTappedOutputPin)
	{
		IQualityControl* pQualityControl = NULL;
		hr = m_pTappedOutputPin->QueryInterface(IID_IQualityControl, (void**)&pQualityControl);
		if (SUCCEEDED(hr)) {
			hr = pQualityControl->Notify(m_pTapFilter, q);
			pQualityControl->Release();
		}
	}
	else hr = E_NOTIMPL;

	return hr;
}

CTapFilter::CTapFilter(HRESULT* phr) :
    CBaseFilter( TEXT("Tap Filter"), NULL, &m_csFilter, IID_IUnknown )
{
	ENTRYEXITLOG(NULL);
	m_pInputPin = new CTapFilterInputPin(this, &m_csFilter, phr, TEXT("Input Pin"));
	m_pOutputPin = new CTapFilterOutputPin(this, &m_csFilter, phr, TEXT("Output Pin"));
}

CTapFilter::~CTapFilter()
{
	ENTRYEXITLOG(NULL);
}

HRESULT CTapFilter::CreateInstance(CTapFilter** ppTapFilter)
{
   	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);
	CTapFilter* pTapFilter = new CTapFilter(&hr);
    if (SUCCEEDED(hr) && (pTapFilter != NULL))
		*ppTapFilter = pTapFilter;
    return hr;
}

CBasePin *CTapFilter::GetPin(int n)
{
	ENTRYEXITLOG(NULL);
	if (n == 0)
		return m_pInputPin;
	else if (n == 1)
		return m_pOutputPin;
	else
		return NULL;
}

int CTapFilter::GetPinCount(void)
{
	ENTRYEXITLOG(NULL);
	return 2;
}

HRESULT CTapFilter::Insert(IGraphBuilder* pGraph, IPin* pFromPin)
{
	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);

	IPin* pToPin = NULL;
	AM_MEDIA_TYPE mt1;
	AM_MEDIA_TYPE mt2;

	// The pin to which this is connected
	hr = pFromPin->ConnectedTo(&pToPin);
	if (FAILED(hr))
	{
		return hr;
	}

	if (SUCCEEDED(hr))
	{
		// The connection media type
		pFromPin->ConnectionMediaType(&mt1);
		pToPin->ConnectionMediaType(&mt2);
		LOG(TEXT("Tapped types: \n"));
		PrintMediaType(&mt1);
		PrintMediaType(&mt2);
	}

	if (SUCCEEDED(hr))
	{
		// Disconnect the pins
		hr = pGraph->Disconnect(pFromPin);
	}
	else {
		LOG(TEXT("%S: ERROR %d@%S - failed to disconnect tapped pins."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	if (SUCCEEDED(hr))
	{
		hr = pGraph->Disconnect(pToPin);
	}
	else {
		LOG(TEXT("%S: ERROR %d@%S - failed to disconnect tapped pins."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}


	if (SUCCEEDED(hr))
	{
		// Set the pins and media types
		m_pInputPin->SetConnectionParameters(m_pOutputPin, pFromPin, pToPin, &mt1);
		m_pOutputPin->SetConnectionParameters(m_pInputPin, pFromPin, pToPin, &mt2);
	}

	if (SUCCEEDED(hr))
	{
		// Add the tap filter to the graph
		hr = pGraph->AddFilter(this, L"TapFilter");
	}
	else {
		LOG(TEXT("%S: ERROR %d@%S - failed to add tap filter to graph."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	if (SUCCEEDED(hr))
	{
		// Now reconnect the two pins together with the tap filter in between
#if STRICTMT
		hr = pGraph->ConnectDirect(pFromPin, m_pInputPin, &mt1);
#else
		hr = pGraph->ConnectDirect(pFromPin, m_pInputPin, NULL);
#endif
	}
	else {
		LOG(TEXT("%S: ERROR %d@%S - failed to connect tap filter input pin."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}


	if (SUCCEEDED(hr))
	{
#if STRICTMT
		hr = pGraph->ConnectDirect(m_pOutputPin, pToPin, &mt1);
#else
		hr = pGraph->ConnectDirect(m_pOutputPin, pToPin, NULL);
#endif
	}
	else {
		LOG(TEXT("%S: ERROR %d@%S - failed to connect tap filter output pin."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	if (SUCCEEDED(hr))
	{
		AM_MEDIA_TYPE tmt1;
		AM_MEDIA_TYPE tmt2;
		pFromPin->ConnectionMediaType(&tmt1);
		pToPin->ConnectionMediaType(&tmt2);
		LOG(TEXT("Tap filter types: \n"));
		PrintMediaType(&tmt1);
		PrintMediaType(&tmt2);
		CMediaType tcmt1(tmt1);
		CMediaType tcmt2(tmt2);
		CMediaType cmt1(mt1);
		if ((tcmt1 != tcmt2) || (tcmt1 != cmt1))
			LOG(TEXT("%S: WARNING %d@%S - the tapped and tap filter types don't seem to be the same.\n"), __FUNCTION__, __LINE__, __FILE__);

		// Use Connect to fix the problems with setting all kinds of OutputTypeSet flags on
		// different filter. we don't care about return value as long as the flags are set 
		// after calling Connect().
		// Actually ConnnectDirect calls Connect. We will investigate why calling ConnectDirect is not enough.
		pFromPin->Connect( m_pInputPin, &tmt1 );
		m_pOutputPin->Connect( pToPin, &tmt2 );
	}

	if(pToPin)
		pToPin->Release();
	
	m_pGraph = pGraph;
	return hr;
}

HRESULT CTapFilter::GetMediaType(AM_MEDIA_TYPE* pMediaType, PIN_DIRECTION pindir)
{
	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);
	if (pindir == PINDIR_INPUT)
		hr = m_pInputPin->ConnectionMediaType(pMediaType);
	else if (pindir == PINDIR_OUTPUT)
		hr = m_pOutputPin->ConnectionMediaType(pMediaType);
	else hr = E_FAIL;
	return hr;
}

HRESULT CTapFilter::RegisterEventCallback(TapFilterCallbackFunction callbackfn, void* pCallbackData, void* pObject)
{
	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);
	callbackList.push_back(Callback(callbackfn, pCallbackData, pObject));
	return hr;
}

HRESULT CTapFilter::Disconnect()
{
	HRESULT hr = S_OK;	
	ENTRYEXITLOG(&hr);

	hr = m_pGraph->Disconnect(m_pTappedOutputPin);
	if (SUCCEEDED(hr))
		hr = m_pGraph->Disconnect(m_pOutputPin);

	return hr;
}

HRESULT CTapFilter::TriggerGraphEvent(GraphEvent event, void* pEventData)
{
    HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);

	BOOL bDropSample = false;
	CallbackList::iterator iterator = callbackList.begin();

	while(iterator != callbackList.end())
	{
        Callback callback = *iterator;
		hr = callback.callbackfn(event, pEventData, callback.pCallbackData, callback.pObject);
		
		// Check to see if the callback function wants us to drop the sample
		if((HRESULT)TF_S_DROPSAMPLE == hr && SAMPLE == event)
			bDropSample = true;
		iterator++;
	}

	if(bDropSample)
		hr = TF_S_DROPSAMPLE;

	return hr;
}

HRESULT CTapFilter::Pause()
{
	HRESULT hr = S_OK;
	// Trigger the graph event
	GraphSample event;

	// Try getting the reference clock and the arrival time from that
	IReferenceClock* pRefClock = NULL;
	hr = GetSyncSource(&pRefClock);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to get reference clock (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
		hr = S_OK;
	}
	if (pRefClock)
	{
		hr = pRefClock->GetTime(&event.ats);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to get the arrival time (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			hr = S_OK;
			event.ats = 0;
		}
		pRefClock->Release();
	}
	else event.ats = 0;

	// Trigger the graph event
	TriggerGraphEvent(STATE_CHANGE_PAUSE, (void*)&event);

	// Propagate the pause call
	return CBaseFilter::Pause();
}

HRESULT CTapFilter::Stop()
{
	HRESULT hr = S_OK;
	
	// Trigger the graph event
	GraphSample event;

	// Try getting the reference clock and the arrival time from that
	IReferenceClock* pRefClock = NULL;
	hr = GetSyncSource(&pRefClock);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to get reference clock (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
		hr = S_OK;
	}
	if (pRefClock)
	{
		hr = pRefClock->GetTime(&event.ats);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to get the arrival time (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			hr = S_OK;
			event.ats = 0;
		}
		pRefClock->Release();
	}
	else event.ats = 0;

	// Trigger the graph event
	TriggerGraphEvent(STATE_CHANGE_STOP, (void*)&event);

	// Propagate the stop call
	return CBaseFilter::Stop();
}

HRESULT CTapFilter::Run(REFERENCE_TIME tStart)
{
	HRESULT hr = S_OK;
	
	// Trigger the graph event
	GraphSample event;

	// Try getting the reference clock and the arrival time from that
	IReferenceClock* pRefClock = NULL;
	hr = GetSyncSource(&pRefClock);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to get reference clock (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
		hr = S_OK;
	}
	if (pRefClock)
	{
		hr = pRefClock->GetTime(&event.ats);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to get the arrival time (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			hr = S_OK;
			event.ats = 0;
		}
		pRefClock->Release();
	}
	else event.ats = 0;

	// Trigger the graph event
	TriggerGraphEvent(STATE_CHANGE_RUN, (void*)&event);

	// Propagate the run call
	return CBaseFilter::Run(tStart);
}

HRESULT CTapFilterInputPin::Active()
{
	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);
	return hr;
}

HRESULT CTapFilterOutputPin::Active()
{
	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);
	return hr;
}

HRESULT CTapFilterInputPin::Inactive()
{
	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);
	return hr;
}

HRESULT CTapFilterOutputPin::Inactive()
{
	HRESULT hr = S_OK;
	ENTRYEXITLOG(&hr);
	return hr;
}
