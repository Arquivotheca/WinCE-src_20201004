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
#include "BasicVerifiers.h"

HRESULT CreatePlaybackDurationVerifier(VerificationType type, void* pVerificationData, TestGraph* pTestGraph, IGraphVerifier** ppGraphVerifier)
{
	HRESULT hr = S_OK;
	IGraphVerifier* pVerifier = NULL;

	switch(type)
	{
	case VerifyPlaybackDuration:
		pVerifier = new PlaybackDurationVerifier();
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

PlaybackDurationVerifier::PlaybackDurationVerifier()
{
	// Set the type to something not valid
	m_VerificationType = VerificationEndMarker;
	m_pTestGraph = NULL;
}

PlaybackDurationVerifier::~PlaybackDurationVerifier()
{
}

HRESULT PlaybackDurationVerifier::Init(TestGraph* pTestGraph, VerificationType type, void* pVerificationData)
{
	if (type != VerifyPlaybackDuration)
		return E_INVALIDARG;

	// Store the verification requested
	m_VerificationType = type;

	// Store the test graph
	m_pTestGraph = pTestGraph;

	// Init the first sample flag
	m_bFirstSample = true;

	// Set the first sample timestamp to 0
	m_FirstSampleTime = 0;

	// Set the EOS timestamp to 0
	m_EOSTime = 0;

	// Init the tap filter object
	m_pTapFilter = NULL;
	
	// Insert a tap filter at the input of the renderer filter
	// If the stream has both audio and video, then choose the audio stream
	HRESULT hr = S_OK;
	if (pTestGraph->GetNumAudioStreams())
		m_MediaType = MEDIATYPE_Audio;
	else if (pTestGraph->GetNumVideoStreams())
		m_MediaType = MEDIATYPE_Video;
	else hr = E_FAIL;

	if (FAILED_F(hr))
		return hr;

	// Insert the tap filter
	hr = pTestGraph->InsertTapFilter(ToClass(Renderer), PINDIR_INPUT, m_MediaType, &m_pTapFilter);

	if (FAILED_F(hr))
		return hr;

	// Register a callback
	hr = m_pTapFilter->RegisterEventCallback(GenericTapFilterCallback, pVerificationData, (void*)this);

	return hr;
}

HRESULT PlaybackDurationVerifier::GetResult(VerificationType type, void *pVerificationData, TCHAR* pResultStr)
{
	PlaybackDurationData* pData = (PlaybackDurationData*)pVerificationData;
	
	// Return the recorded time-stamps
	if (pData)
	{
		pData->firstSampleTime = m_FirstSampleTime;
		pData->eosTime = m_EOSTime;
	}

	return S_OK;
}

HRESULT PlaybackDurationVerifier::ProcessEvent(GraphEvent event, void* pEventData)
{
	// If this is the first sample stop the timer
	if ((m_bFirstSample) && (event == SAMPLE))
	{
		// Start the timer
		m_FirstSampleTime = MSEC_TO_TICKS(GetTickCount());
		m_bFirstSample = false;
	}
	else if (event == EOS)
	{
		m_EOSTime = MSEC_TO_TICKS(GetTickCount());
	}

	return S_OK;
}

HRESULT PlaybackDurationVerifier::Start()
{
	return S_OK;
}

void PlaybackDurationVerifier::Stop()
{
	// BUGBUG: Removing the tap filter causes an exception. Not doing so for now until figuring out why.
	// if (m_pTapFilter)
	//	m_pTapFilter->Disconnect();

	return;
}

void PlaybackDurationVerifier::Reset()
{
}
