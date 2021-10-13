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
#include "Index.h"
#include "StringConversions.h"
#include "utility.h"
#include "FrameRecognizer.h"
#include "BasicVerifiers.h"

HRESULT CreateSampleDeliveryVerifier(VerificationType type, void* pVerificationData, TestGraph* pTestGraph, IGraphVerifier** ppGraphVerifier)
{
	HRESULT hr = S_OK;
	IGraphVerifier* pVerifier = NULL;

	switch(type)
	{
	case VerifySampleDelivered:
		pVerifier = new SampleDeliveryVerifier();
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


// Decoder output timestamp verifier
SampleDeliveryVerifier::SampleDeliveryVerifier() : 
	// Set the type to something not valid
	m_VerificationType(VerificationEndMarker),
	m_Tolerance(0),
	m_pTestGraph(NULL),
	m_pTapFilter(NULL),
	m_bExpectSample(false),
	m_hr(S_OK)
{
	memset(&m_mt, 0, sizeof(m_mt));
}
	

SampleDeliveryVerifier::~SampleDeliveryVerifier()
{
}

HRESULT SampleDeliveryVerifier::Init(TestGraph* pTestGraph, VerificationType type, void* pVerificationData)
{
	// Cast the the verification data
	SampleDeliveryVerifierData *pData = (SampleDeliveryVerifierData*)pVerificationData;

	// Insert the tap filter at the specified location
	HRESULT hr = pTestGraph->InsertTapFilter(pData->filterType, pData->pindir, pData->mediaMajorType, &m_pTapFilter);
	if (FAILED_F(hr))
		return hr;

	// Save the media type
	m_pTapFilter->GetMediaType(&m_mt, PINDIR_INPUT);

	// Register a callback
	hr = m_pTapFilter->RegisterEventCallback(GenericTapFilterCallback, pVerificationData, (void*)this);

	// Store the verification requested
	m_VerificationType = type;

	// Store the tolerance
	m_Tolerance = pData->tolerance;

	// Init the event we are interested in
	m_WaitForEvent = pData->event;

	// Store the test graph
	m_pTestGraph = pTestGraph;

	return hr;
}

HRESULT SampleDeliveryVerifier::GetResult(VerificationType type, void *pVerificationData, TCHAR* pResultStr)
{
	if (m_VerificationType & VerifySampleDelivered)
	{
		// If we are still expecting a frame then return error
		return (m_bExpectSample) ? E_FAIL : S_OK;
	}
	else return E_NOTIMPL;
}

HRESULT SampleDeliveryVerifier::ProcessEvent(GraphEvent event, void* pEventData)
{
	HRESULT hr = S_OK;

	if (m_VerificationType & VerifySampleDelivered)
	{
		// If this is a sample and we are expecting one
		if ((event == SAMPLE) && m_bExpectSample)
		{
			// Expected a sample and we got one
			LOG(TEXT("%S: expected sample and got one"), __FUNCTION__);
			m_bExpectSample = false;
		}
		else {
			// If this event is one we are waiting for - set the sample expected to true
			if ((DWORD)event & m_WaitForEvent)
			{
				LOG(TEXT("%S: waited for events (0x%x) and got event (0x%x). Now waiting for sample."), __FUNCTION__, m_WaitForEvent, event);
				m_bExpectSample = true;
			}
		}
	}

	return m_hr;
}

HRESULT SampleDeliveryVerifier::Start()
{
	return S_OK;
}

void SampleDeliveryVerifier::Stop()
{
	// BUGBUG: Removing the tap filter causes an exception. Not doing so for now until figuring out why.
	// if (m_pTapFilter)
	//	m_pTapFilter->Disconnect();
}

void SampleDeliveryVerifier::Reset()
{
	// Result our result for a fresh capture
	m_bExpectSample = false;
}


