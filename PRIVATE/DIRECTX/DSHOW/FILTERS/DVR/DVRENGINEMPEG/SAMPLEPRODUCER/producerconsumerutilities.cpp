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
#include <stdafx.h>
#include "ProducerConsumerUtilities.h"

HRESULT MSDvr::WStringToOleString(const std::wstring &wstringSource, LPOLESTR *ppszDestinationString)
{
	HRESULT hr = S_OK;
	const wchar_t *pwszSourceString = wstringSource.c_str();

	*ppszDestinationString = NULL;
	if (!*pwszSourceString)
		hr = S_OK;
	else
	{
		size_t uStrLen = 1 + wcslen(pwszSourceString);
		*ppszDestinationString = (LPOLESTR) CoTaskMemAlloc(uStrLen * sizeof(OLECHAR));
		if (! *ppszDestinationString)
			hr = E_OUTOFMEMORY;
		else
			memcpy(*ppszDestinationString, pwszSourceString, uStrLen * sizeof(OLECHAR));
	}
	return hr;
} // WStringToOleString

////////////////////////////////////////////////
// Now back to good code:
////////////////////////////////////////////////

void MSDvr::CopySample(IMediaSample *pSrcSample, IMediaSample *pDestSample)
{
	REFERENCE_TIME rtStartTime, rtEndTime;
	HRESULT hr;
	
	if (SUCCEEDED(pSrcSample->GetTime(&rtStartTime, &rtEndTime)))
	{
		if (rtEndTime < rtStartTime)
			rtEndTime = rtStartTime;
#ifdef DEBUG
		hr =
#endif
			pDestSample->SetTime(&rtStartTime, &rtEndTime);
#ifdef DEBUG
		if (FAILED(hr))
			throw CHResultError(hr);
#endif
	}

	LONGLONG hyStartTime, hyEndTime;
	if (SUCCEEDED(pSrcSample->GetMediaTime(&hyStartTime, &hyEndTime)))
	{
		if (hyEndTime < hyStartTime)
			hyEndTime = hyStartTime;
#ifdef DEBUG
		hr = 
#endif
			pDestSample->SetMediaTime(&hyStartTime, &hyEndTime);
#ifdef DEBUG
		if (FAILED(hr))
			throw CHResultError(hr);
#endif
	}
#ifdef DEBUG
	hr = 
#endif
		pDestSample->SetPreroll(pSrcSample->IsPreroll() == S_OK);
#ifdef DEBUG
	if (SUCCEEDED(hr))
		hr = 
#endif
			pDestSample->SetDiscontinuity(pSrcSample->IsDiscontinuity() == S_OK);
#ifdef DEBUG
	if (SUCCEEDED(hr))
		hr =
#endif
			pDestSample->SetSyncPoint(pSrcSample->IsSyncPoint() == S_OK);
#ifdef DEBUG
	if (FAILED(hr))
		throw CHResultError(hr);
#endif

	long cbBytes = pSrcSample->GetActualDataLength();
	BYTE *pSrcBytes = NULL;
	hr = pSrcSample->GetPointer(&pSrcBytes);
	BYTE *pDestBytes = NULL;
	if (SUCCEEDED(hr))
	{
		hr = pDestSample->GetPointer(&pDestBytes);
		if (SUCCEEDED(hr))
		{
			hr = pDestSample->SetActualDataLength(cbBytes);
			if (SUCCEEDED(hr))
				memcpy(pDestBytes, pSrcBytes, cbBytes);
		}
	}

#if 0 // Yes, we really ought to do this, but it costs CPU and in practice, we don't use this info
	if (FAILED(hr))
		throw CHResultError(hr);

	AM_MEDIA_TYPE *pmt = NULL;
	if (S_OK == pSrcSample->GetMediaType(&pmt))  // S_FALSE is also possible but pmt will be NULL
	{
		hr = pDestSample->SetMediaType(pmt);
		DeleteMediaType(pmt);
		if (FAILED(hr))
			throw CHResultError(hr);
	}

	IMediaSample2 *piMediaSample2Src = NULL;
	IMediaSample2 *piMediaSample2Dest = NULL;
	if (SUCCEEDED(pSrcSample->QueryInterface(IID_IMediaSample2, (void**) &piMediaSample2Src)))
	{
		hr = pDestSample->QueryInterface(IID_IMediaSample2, (void**) &piMediaSample2Dest);
		if (SUCCEEDED(hr))
		{
			AM_SAMPLE2_PROPERTIES amSample2Properties;
			if (SUCCEEDED(piMediaSample2Src->GetProperties(sizeof(amSample2Properties), (BYTE*)&amSample2Properties)))
			{
				DWORD uNonDataPropSize = (DWORD)(((BYTE*) (&amSample2Properties.pMediaType)) - ((BYTE*)&amSample2Properties));
				amSample2Properties.cbData = uNonDataPropSize;
				hr = piMediaSample2Dest->SetProperties(uNonDataPropSize, (BYTE*)&amSample2Properties);
			}
			piMediaSample2Dest->Release();
		}
		piMediaSample2Src->Release();
	}
#endif // 0

#ifdef DEBUG
	if (FAILED(hr))
		throw CHResultError(hr);
#endif
}
