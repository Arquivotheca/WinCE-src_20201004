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
#include <streams.h>
#include "DummySink.h"

CTypedAllocatorInputPin::CTypedAllocatorInputPin( CBaseFilter* pFilter, CCritSec* pLoc, HRESULT* phr, WCHAR* wzPinName ) :
    CBaseInputPin( wzPinName, pFilter, pLoc, phr, wzPinName )
{
	m_pMediaTypeArray = NULL;
	m_nMediaTypes = 0;
	m_bWildCard = false;
}


CTypedAllocatorInputPin::~CTypedAllocatorInputPin()
{
	if (m_pMediaTypeArray)
		delete [] m_pMediaTypeArray;
}


STDMETHODIMP 
CTypedAllocatorInputPin::NonDelegatingQueryInterface(REFGUID riid, void **ppv)
{
    return CBaseInputPin::NonDelegatingQueryInterface( riid, ppv );
}


STDMETHODIMP 
CTypedAllocatorInputPin::Receive( IMediaSample* pSample )
{
    return S_OK;
}


HRESULT 
CTypedAllocatorInputPin::SetSupportedTypes(AM_MEDIA_TYPE* pMediaTypes, int nMediaTypes, bool bWildCard)
{
	if (nMediaTypes <= 0)
		return E_FAIL;

	if (m_pMediaTypeArray) {
		delete [] m_pMediaTypeArray;
		m_pMediaTypeArray = NULL;
	}
	
	m_pMediaTypeArray = new CMediaType[nMediaTypes];
	if (!m_pMediaTypeArray)
		return E_OUTOFMEMORY;

	for(int i = 0; i < nMediaTypes; i++)
		m_pMediaTypeArray[i] = CMediaType(pMediaTypes[i]);
	
	m_nMediaTypes = nMediaTypes;
	m_bWildCard = bWildCard;
    return S_OK;
}


HRESULT 
CTypedAllocatorInputPin::CheckMediaType(const CMediaType* pmt)
{
	BOOL match = FALSE;
	for(int i = 0; i < m_nMediaTypes; i++)
	{
		match = (m_bWildCard) ? pmt->MatchesPartial(&m_pMediaTypeArray[i]) : (*pmt == m_pMediaTypeArray[i]);
		if (match)
			break;
	}
	return (match) ? S_OK : S_FALSE;
}


HRESULT 
CTypedAllocatorInputPin::SetMediaType(const CMediaType *pmt)
{
	if (FAILED(CheckMediaType(pmt)))
		return E_FAIL;
	return CBaseInputPin::SetMediaType(pmt);
}


HRESULT 
CTypedAllocatorInputPin::GetMediaType(int iPosition,CMediaType *pMediaType)
{
    if ((iPosition < 0) || (iPosition >= (int) m_nMediaTypes))
    {
        return VFW_S_NO_MORE_ITEMS;
    }
	*pMediaType = m_pMediaTypeArray[iPosition];
    return S_OK;
}

CDummySink::CDummySink( HRESULT *phr, int nPins) :
    CBaseFilter( L"Dummy Sink filter", NULL, &m_csFilter, IID_IUnknown )
{
	m_nPins = nPins;
	for(int i = 0; i < nPins; i++)
	{
		m_pInputPin[i] = new CTypedAllocatorInputPin(this, &m_csFilter, phr, L"InputPin" );

		if( m_pInputPin[i] == NULL ) {
			*phr = E_OUTOFMEMORY;
		}
	}
}

CDummySink::CDummySink( HRESULT *phr) :
    CBaseFilter( L"Dummy Sink filter", NULL, &m_csFilter, IID_IUnknown )
{
}    

CDummySink::~CDummySink()
{
	for(int i = 0; i < m_nPins; i++)
	{
		if (m_pInputPin[i])
			delete m_pInputPin[i];
		m_pInputPin[i] = 0;
	}
}


CUnknown* 
CDummySink::CreateInstance( LPUNKNOWN pUnkOuter, HRESULT* phr )
{
    if( phr == NULL )
    {
        return NULL;
    }


    *phr = S_OK;

    CDummySink* pDummySink = new CDummySink(phr, 1);
    if( pDummySink == NULL )
    {
        *phr = E_OUTOFMEMORY;
    }

    return pDummySink;
}


STDMETHODIMP
CDummySink::NonDelegatingQueryInterface( REFIID riid, void ** ppv )
{
    return CBaseFilter::NonDelegatingQueryInterface( riid, ppv );
}

int  
CDummySink::GetPinCount()
{
    return m_nPins;
}


CBasePin*
CDummySink::GetPin( int n )
{
    if( (n >= 0) && (n < m_nPins))
    {
        return m_pInputPin[n];
    }

    return NULL;
}

HRESULT CDummySink::SetSupportedTypes(int pin, AM_MEDIA_TYPE* mtArray, int nMediaTypes, bool bWildCard)
{
	if ((pin < 0) || (pin >= m_nPins))
		return E_UNEXPECTED;
	else
		return m_pInputPin[pin]->SetSupportedTypes(mtArray, nMediaTypes, bWildCard);
}