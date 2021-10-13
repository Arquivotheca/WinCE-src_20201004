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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
////////////////////////////////////////////////////////////////////////////////

#include <streams.h>
#include "PinEnumerator.h"

CPinEnumerator::CPinEnumerator(IEnumPins* pEnumPins, PIN_DIRECTION dir, GUID majortype, GUID subtype)
{
	pEnumPins->AddRef();
	pEnumPins->Reset();
	m_pEnumPins = pEnumPins;
	m_dir = dir;
	m_guidMajorType = majortype;
	m_guidSubType = subtype;
}

CPinEnumerator::~CPinEnumerator()
{
	if (m_pEnumPins)
		m_pEnumPins->Release();
}

HRESULT CPinEnumerator::IsMatching(AM_MEDIA_TYPE* pMediaType, GUID majortype, GUID subtype)
{
	bool bMatch = true;
	
	if (majortype != GUID_NULL)
		bMatch = (pMediaType->majortype == majortype) ? true : false;
	
	if (bMatch && (subtype != GUID_NULL))
		bMatch = (pMediaType->subtype == subtype) ? true : false;
	
	return (bMatch) ? S_OK : S_FALSE;
}

HRESULT CPinEnumerator::QueryMatchPinType(IPin* pPin)
{
	HRESULT hr = S_OK;

	AM_MEDIA_TYPE mt;
	AM_MEDIA_TYPE *pMediaType = NULL;
	bool bConnected = false;

	// Get the connected media type
	hr = pPin->ConnectionMediaType(&mt);
	if (FAILED(hr) || (hr == S_FALSE))
	{
		// We may not be connected so query the pin for its media type enumerator
		IEnumMediaTypes* pEnumMediaTypes = NULL;
		hr = pPin->EnumMediaTypes(&pEnumMediaTypes);
		if (FAILED(hr) || (hr == S_FALSE))
			return E_FAIL;

		hr = pEnumMediaTypes->Next(1, &pMediaType, 0);
		if (FAILED(hr) || (hr == S_FALSE))
		{
			pEnumMediaTypes->Release();
			return E_FAIL;
		}
	}
	else {
		// We are connected, so use the connection media type
		bConnected = true;
		pMediaType = &mt;
	}
	
	// We have a media type from the pin to check againt
	hr = IsMatching(pMediaType, m_guidMajorType, m_guidSubType);
	
	if (bConnected)
		FreeMediaType(*pMediaType);
	else 
		DeleteMediaType(pMediaType);

	return hr;
}

HRESULT CPinEnumerator::Next(IPin** ppPin)
{
	IPin* pPin;
	ULONG fetched;
	
	HRESULT hr = S_OK;
	
	while (true)
	{
		hr = m_pEnumPins->Next(1, &pPin, &fetched);
		if (FAILED(hr) || (hr == S_FALSE))
			break;
		// We got a pin - let's see if this matches our direction and media type
		
		// Check direction
		PIN_DIRECTION dir;
		pPin->QueryDirection(&dir);
		if (dir != m_dir)
			continue;

		// Check if the major or sub type has the potential to match the pin's media type
		hr = QueryMatchPinType(pPin);
		if (FAILED(hr) || (hr == S_FALSE))
			continue;
	}
	return hr;
}

HRESULT CPinEnumerator::Reset()
{
	if (!m_pEnumPins)
		return E_POINTER;
	return m_pEnumPins->Reset();
}
