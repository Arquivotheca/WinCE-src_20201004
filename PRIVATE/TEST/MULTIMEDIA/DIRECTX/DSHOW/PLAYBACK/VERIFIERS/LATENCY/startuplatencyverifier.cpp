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

HRESULT CreateStartupLatencyVerifier(VerificationType type, void* pVerificationData, TestGraph* pTestGraph, IGraphVerifier** ppGraphVerifier)
{
	HRESULT hr = S_OK;
	IGraphVerifier* pVerifier = NULL;

	switch(type)
	{
	case StartupLatency:
		pVerifier = new StartupLatencyVerifier();
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

StartupLatencyVerifier::StartupLatencyVerifier() :
	// Set the type to something not valid
	m_VerificationType(VerificationEndMarker),
	m_pTestGraph(NULL),
	m_Tolerance(0),
	m_bWaitingForFirstSample(false)
{
}

StartupLatencyVerifier::~StartupLatencyVerifier()
{
}

HRESULT StartupLatencyVerifier::Init(TestGraph* pTestGraph, VerificationType type, void* pVerificationData)
{
	if (type != StartupLatency)
		return E_INVALIDARG;

	// Store the verification requested
	m_VerificationType = type;

	// Store the test graph
	m_pTestGraph = pTestGraph;

	// Store the playback duration tolerance
	if (pVerificationData)
		m_Tolerance = *(DWORD*)pVerificationData;
	
	// Init the first sample flag
	m_bWaitingForFirstSample = true;

	// Set the latency to 0
	m_Latency = 0;

	// Init the tap filter object
	m_pTapFilter = NULL;
	
	// Insert a tap filter at the input to the decoder
	// If we can't insert at the video decoder, insert at the audio decoder
	HRESULT hr = S_OK;
	if (pTestGraph->GetNumVideoStreams())
	{
		hr = pTestGraph->InsertTapFilter(ToClass(Decoder)|ToSubClass(VideoDecoder), PINDIR_INPUT, GUID_NULL, &m_pTapFilter);
	}
	else if (pTestGraph->GetNumAudioStreams())
	{
		hr = pTestGraph->InsertTapFilter(ToClass(Decoder)|ToSubClass(AudioDecoder), PINDIR_INPUT, GUID_NULL, &m_pTapFilter);
	}
	else hr = E_FAIL;

	if (FAILED_F(hr) || !m_pTapFilter)
		return hr;

	// Register a callback
	hr = m_pTapFilter->RegisterEventCallback(GenericTapFilterCallback, pVerificationData, (void*)this);

	return hr;
}

HRESULT StartupLatencyVerifier::GetResult(VerificationType type, void *pVerificationData, TCHAR* pResultStr)
{
	HRESULT hr = S_OK;

	// Have we received a sample at all?
	if (m_bWaitingForFirstSample)
		return E_FAIL;

	// Return the latency
	if (pVerificationData)
		*(LONGLONG*)pVerificationData = m_Latency;

	// Compare the startup latency against the tolerance
	if (TICKS_TO_MSEC(m_Latency) < m_Tolerance)
	{
		//LOG(TEXT("%S: startup latency (%u) msec was within tolerance (%u) msec"), __FUNCTION__, TICKS_TO_MSEC(m_Latency), m_Tolerance);
		hr = S_OK;
	}
	else {
		LOG(TEXT("%S: ERROR %d@%S - startup latency (%d) msec not within tolerance (%d) msec"), __FUNCTION__, __LINE__, __FILE__, TICKS_TO_MSEC(m_Latency), m_Tolerance);
		hr = E_FAIL;
	}			

	return hr;
}

HRESULT StartupLatencyVerifier::ProcessEvent(GraphEvent event, void* pEventData)
{
	if (m_bWaitingForFirstSample)
	{
		if (event == SAMPLE)
		{
			m_bWaitingForFirstSample = false;
			m_Latency = m_Timer.Stop();
			LOG(TEXT("%S: got sample latency (%d) msec"), __FUNCTION__, TICKS_TO_MSEC(m_Latency));
		}
	}

	return S_OK;
}

HRESULT StartupLatencyVerifier::Start()
{
	Reset();
	return S_OK;
}

void StartupLatencyVerifier::Stop()
{
	// BUGBUG: Disconnecting the tap filter causing a crash? 

	return;
}

void StartupLatencyVerifier::Reset()
{
	LOG(TEXT("%S: starting timer"), __FUNCTION__);
	m_bWaitingForFirstSample = true;
	m_Timer.Start();
}
