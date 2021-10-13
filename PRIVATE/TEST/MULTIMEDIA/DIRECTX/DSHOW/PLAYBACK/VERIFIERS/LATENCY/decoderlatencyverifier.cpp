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
//
//  Verifier objects
//
//
////////////////////////////////////////////////////////////////////////////////

#include "logging.h"
#include "globals.h"
#include "TestDesc.h"
#include "TestGraph.h"
#include "PinEnumerator.h"
#include "StringConversions.h"
#include "utility.h"
#include "LatencyVerifiers.h"


HRESULT CreateDecoderOutputLatencyVerifier(VerificationType type, void* pVerificationData, TestGraph* pTestGraph, IGraphVerifier** ppGraphVerifier)
{
	HRESULT hr = S_OK;
	IGraphVerifier* pVerifier = NULL;

	switch(type)
	{
	case DecodedVideoLatencyPauseToFirstSample:
	case DecodedVideoLatencyRunToFirstSample:
		pVerifier = new DecoderOutputLatencyVerifier();
		if (!pVerifier)
			hr = E_OUTOFMEMORY;
		break;
	
	default:
		hr = E_NOTIMPL;
		break;
	};
	
	if (SUCCEEDED(hr))
	{
		hr = pVerifier->Init(pTestGraph, type, pVerificationData);
		if (FAILED(hr))
			delete pVerifier;
		else 
			*ppGraphVerifier = pVerifier;
	}
	
	return hr;
}

// Decoder output latency verifier
DecoderOutputLatencyVerifier::DecoderOutputLatencyVerifier() :
	m_VerificationType (VerificationEndMarker),
	m_bStarted(false),
	m_Tolerance(0),
	m_Latency(0),
	m_bWaitingForTrigger(false),
	m_bWaitingForSample(false),
	m_hr(S_OK)
{
}

DecoderOutputLatencyVerifier::~DecoderOutputLatencyVerifier()
{
}

HRESULT DecoderOutputLatencyVerifier::Init(TestGraph* pTestGraph, VerificationType type, void* pVerificationData)
{
	ITapFilter* pTapFilter = NULL;
	
	DWORD filter = FilterType(Decoder, VideoDecoder);
	HRESULT hr = pTestGraph->InsertTapFilter(filter, PINDIR_OUTPUT, MEDIATYPE_Video, &pTapFilter);
	if (FAILED_F(hr))
		return hr;

	// Register a callback
	hr = pTapFilter->RegisterEventCallback(GenericTapFilterCallback, pVerificationData, (void*)this);

	// Save the verification type
	m_VerificationType = type;

	// Store the latency tolerance
	m_Tolerance = *(DWORD*)pVerificationData;

	return hr;
}

HRESULT DecoderOutputLatencyVerifier::GetResult(VerificationType type, void *pVerificationData, TCHAR* ppResultStr)
{
	if (m_bWaitingForSample)
	{
		LOG(TEXT("%S: ERROR %d@%S - still waiting for sample to be delivered"), __FUNCTION__, __LINE__, __FILE__);
		return E_FAIL;
	}

	return m_hr;
}

HRESULT DecoderOutputLatencyVerifier::ProcessEvent(GraphEvent event, void* pEventData)
{
	// BUGBUG: What are the options?
	HRESULT hr = S_OK;
	
	if (!m_bStarted)
		return S_FALSE;

	if ((m_VerificationType == DecodedVideoLatencyRunToFirstSample) ||
		(m_VerificationType == DecodedVideoLatencyPauseToFirstSample))
	{
		// When we get the Run/Pause, start the timer.
		if ((((event == STATE_CHANGE_RUN) && (m_VerificationType == DecodedVideoLatencyRunToFirstSample)) ||
			 ((event == STATE_CHANGE_PAUSE) && (m_VerificationType == DecodedVideoLatencyPauseToFirstSample))))
			 
		{
			LOG(TEXT("%S: got trigger - Pause/Run"), __FUNCTION__);
			m_bWaitingForSample = true;
			m_Timer.Start();
		}
		//else if (m_bWaitingForSample && (event == SAMPLE))
		else if ( m_bWaitingForSample && (event == SAMPLE || event == EOS))
		{
			m_bWaitingForSample = false;
			m_Latency = m_Timer.Stop();
			if (TICKS_TO_MSEC(m_Latency) < m_Tolerance)
			{
				LOG(TEXT("%S: first sample latency (%d) msec was within tolerance (%d) msec"), __FUNCTION__, TICKS_TO_MSEC(m_Latency), m_Tolerance);
				m_hr = S_OK;
			}
			else {
				LOG(TEXT("%S: ERROR %d@%S - latency (%d) msec not within tolerance (%d) msec"), __FUNCTION__, __LINE__, __FILE__, TICKS_TO_MSEC(m_Latency), m_Tolerance);
				m_hr = E_FAIL;
			}			
		}
	}

	return S_OK;
}

HRESULT DecoderOutputLatencyVerifier::Start()
{
	Reset();
	m_bStarted = true;
	return S_OK;
}

void DecoderOutputLatencyVerifier::Stop()
{
	m_bStarted = false;
	return;
}

void DecoderOutputLatencyVerifier::Reset()
{
	m_hr = S_OK;
	m_bWaitingForSample = false;
}

