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
#include "DummySource.h"

CTypedOutputPin::CTypedOutputPin( CBaseFilter* pFilter, CCritSec* pLoc, HRESULT* phr, WCHAR* wzPinName ) :
    CBaseOutputPin( wzPinName, pFilter, pLoc, phr, wzPinName )
{
	m_pMediaTypeArray = NULL;
	m_nMediaTypes = 0;
	m_bWildCard = false;
}


CTypedOutputPin::~CTypedOutputPin()
{
	if (m_pMediaTypeArray)
		delete [] m_pMediaTypeArray;
}


STDMETHODIMP 
CTypedOutputPin::NonDelegatingQueryInterface(REFGUID riid, void **ppv)
{
    return CBaseOutputPin::NonDelegatingQueryInterface( riid, ppv );
}


HRESULT CTypedOutputPin::DecideBufferSize(IMemAllocator * pAlloc, ALLOCATOR_PROPERTIES * pProperties)
{
    CheckPointer(pAlloc,E_POINTER);
    CheckPointer(pProperties,E_POINTER);
    HRESULT hr = NOERROR;

	// The properties we get may have been filled in by the input pin. If he does not have any preferences, we will set our default preference.
	// If he does have preferences, then we will accept his preference.
	if (!pProperties->cBuffers)
        pProperties->cBuffers = 1;

	// Set this to an aribitrary value
	if (!pProperties->cbBuffer)
        pProperties->cbBuffer = m_mt.GetSampleSize();

    // Ask the allocator to reserve us some sample memory, NOTE the function
    // can succeed (that is return NOERROR) but still not have allocated the
    // memory that we requested, so we must check we got whatever we wanted
    ALLOCATOR_PROPERTIES actual;
    hr = pAlloc->SetProperties(pProperties,&actual);
    if (FAILED(hr)) 
        return hr;

    // Is this allocator unsuitable
    if (actual.cbBuffer < pProperties->cbBuffer) 
        return E_FAIL;
	if (actual.cBuffers != pProperties->cBuffers)
		return E_FAIL;

    return NOERROR;
}


HRESULT 
CTypedOutputPin::SetSupportedTypes(AM_MEDIA_TYPE* pMediaTypes, int nMediaTypes, bool bWildCard)
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
CTypedOutputPin::CheckMediaType(const CMediaType* pmt)
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
CTypedOutputPin::SetMediaType(const CMediaType *pmt)
{
	if (FAILED(CheckMediaType(pmt)))
		return E_FAIL;
	return CBaseOutputPin::SetMediaType(pmt);
}


HRESULT 
CTypedOutputPin::GetMediaType(int iPosition,CMediaType *pMediaType)
{
    if ((iPosition < 0) || (iPosition >= (int) m_nMediaTypes))
    {
        return VFW_S_NO_MORE_ITEMS;
    }
	*pMediaType = m_pMediaTypeArray[iPosition];
    return S_OK;
}

CDummySource::CDummySource( HRESULT *phr, int nPins) :
    CBaseFilter( L"Dummy Source filter", NULL, &m_csFilter, IID_IUnknown )
{
	m_nPins = nPins;
	for(int i = 0; i < nPins; i++)
	{
		m_pOutputPin[i] = new CTypedOutputPin( this, &m_csFilter, phr, L"OutputPin" );

		if( m_pOutputPin[i] == NULL ) {
			*phr = E_OUTOFMEMORY;
		}
	}
}

CDummySource::~CDummySource()
{
	for(int i = 0; i < m_nPins; i++)
	{
		if (m_pOutputPin[i])
			delete m_pOutputPin[i];
		m_pOutputPin[i] = 0;
	}
}


CUnknown* 
CDummySource::CreateInstance( LPUNKNOWN pUnkOuter, HRESULT* phr )
{
    if( phr == NULL )
    {
        return NULL;
    }


    *phr = S_OK;

    CDummySource* pDummySource = new CDummySource(phr, 1);
    if( pDummySource == NULL )
    {
        *phr = E_OUTOFMEMORY;
    }

    return pDummySource;
}


STDMETHODIMP
CDummySource::NonDelegatingQueryInterface( REFIID riid, void ** ppv )
{
    return CBaseFilter::NonDelegatingQueryInterface( riid, ppv );
}


int  
CDummySource::GetPinCount()
{
    return m_nPins;
}


CBasePin*
CDummySource::GetPin( int n )
{
    if( (n >= 0) && (n < m_nPins))
    {
        return m_pOutputPin[n];
    }

    return NULL;
}

HRESULT CDummySource::SetSupportedTypes(int pin, AM_MEDIA_TYPE* mtArray, int nMediaTypes, bool bWildCard)
{
	if ((pin < 0) || (pin >= m_nPins))
		return E_UNEXPECTED;
	else
		return m_pOutputPin[pin]->SetSupportedTypes(mtArray, nMediaTypes, bWildCard);
}