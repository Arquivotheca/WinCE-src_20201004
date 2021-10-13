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
#include <windows.h>
#include <dshow.h>
#include "TestInterfaces.h"
#include "StreamingVerifier.h"

CStreamingVerifier::CStreamingVerifier() : CVerifier()
{
	memset((void*)&m_SourceMediaType, 0, sizeof(m_SourceMediaType));
	memset((void*)&m_SinkMediaType, 0, sizeof(m_SinkMediaType));

	Reset();
	
	m_bStoredSourceMediaType = false;
	m_bStoredSinkMediaType = false;
	
	m_nSamplesFailed = 0;
	m_nSamplesSinked = 0;
	m_nSamplesSourced = 0;

	m_AccumHR = S_OK;
}

CStreamingVerifier::~CStreamingVerifier()
{
}

void CStreamingVerifier::Reset()
{
	CVerifier::Reset();
	
	FreeMediaType(m_SourceMediaType);
	memset(&m_SourceMediaType, 0, sizeof(m_SourceMediaType));
	
	FreeMediaType(m_SinkMediaType);
	memset(&m_SinkMediaType, 0, sizeof(m_SinkMediaType));
	
	m_bStoredSourceMediaType = false;
	m_bStoredSinkMediaType = false;
	
	m_nSamplesFailed = 0;
	m_AccumHR = S_OK;
}

HRESULT CStreamingVerifier::StreamingBegin()
{
	// first, if we are streaming then fail the call
	if (m_bStreaming)
		return E_FAIL;

	// if we're not streaming, reset
		Reset();

	// now notify the base class (since it sets m_bStreaming, but reset resets it)
	HRESULT hr = CVerifier::StreamingBegin();
	return hr;
}

void CStreamingVerifier::SourceSample(IMediaSample* pSample, AM_MEDIA_TYPE* pSourceMediaType, int nOutputPin)
{
	HRESULT hr = NOERROR;

	CVerifier::SourceSample(pSample, pSourceMediaType, nOutputPin);

	// Store the media type for later use
	AM_MEDIA_TYPE *pMediaType = NULL;

	hr = pSample->GetMediaType(&pMediaType);
	// GetMediaType returns a media type only if it has changed, this is indicated by S_FALSE
	if (FAILED(hr) || ((hr == S_OK) && !pMediaType)) {
		SetAccumResult(m_AccumHR, E_UNEXPECTED);
		return;
	}
	
	// If the sample did not provide one, check if the source gave us one
	if (!pMediaType && pSourceMediaType)
		pMediaType = pSourceMediaType;

	if (!pMediaType && !m_bStoredSourceMediaType) {
		SetAccumResult(m_AccumHR, E_UNEXPECTED);
		return;
	}

	// Store the media type
	if (pMediaType) {
		// Free the format block if we had one
		FreeMediaType(m_SourceMediaType);	
		CopyMediaType(&m_SourceMediaType, pMediaType);
		m_bStoredSourceMediaType = true;
		if (!pSourceMediaType)
			DeleteMediaType(pMediaType);
	}

	return;
}

void CStreamingVerifier::SinkSample(IMediaSample* pSample, AM_MEDIA_TYPE* pSinkMediaType, int nInputPin)
{
	HRESULT hr = NOERROR;

	CVerifier::SinkSample(pSample, pSinkMediaType, nInputPin);

	if (nInputPin != 0) {
		SetAccumResult(m_AccumHR, E_INVALIDARG);
		return;
	}

	// The verifier checks the following during streaming
	// 1. We dont get a null sample
	// 2. The media type is valid
	// 3. The samples have valid bits

	// If we already detected an error, return.
	//if (FAILED(m_AccumHR) || (m_AccumHR == S_FALSE))
	//	return;

	// 1. Null check
	if (!pSample)
		hr = E_UNEXPECTED;


	// 2. Get the media type
	AM_MEDIA_TYPE *pMediaType = NULL;

	hr = pSample->GetMediaType(&pMediaType);
	// GetMediaType returns a media type only if it has changed, this is indicated by S_FALSE
	if (FAILED(hr) || ((hr == S_OK) && !pMediaType)) {
		SetAccumResult(m_AccumHR, E_UNEXPECTED);
		return;
	}
	
	// If the sample did not provide one, check if the source gave us one
	if (!pMediaType && pSinkMediaType)
		pMediaType = pSinkMediaType;

	if (!pMediaType && !m_bStoredSinkMediaType) {
		SetAccumResult(m_AccumHR, E_UNEXPECTED);
		return;
	}

	// Set hr back to S_OK in case we got S_FALSE from GetMediaType
	hr = NOERROR;

	// Verify and store the media type
	if (pMediaType) {
		if (!VerifySinkMediaType(pMediaType, (m_bStoredSinkMediaType) ? &m_SinkMediaType : 0))
			hr = S_FALSE;

		// Free the format block if we had one
		FreeMediaType(m_SinkMediaType);	
		CopyMediaType(&m_SinkMediaType, pMediaType);

		m_bStoredSinkMediaType = true;

		if (!pSinkMediaType)
			DeleteMediaType(pMediaType);
	}
	
	// 3. Verify the sample itself
	if (!VerifySinkMediaSample(pSample))
	{
		m_nSamplesFailed++;
		hr = S_FALSE;
	}

	SetAccumResult(m_AccumHR, hr);
	return;
}

int CStreamingVerifier::GetSamplesFailed(int pin)
{
	return m_nSamplesFailed;
}

