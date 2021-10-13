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
//
//  Time stamp instrumenter 
//
//
////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>

#include <windows.h>
#include <winuser.h>
#include <wingdi.h>
#include <stdio.h>
#include <tchar.h>
#include <streams.h>
#include <ddraw.h>

#include "globals.h"
#include "logging.h"
#include "EventLogger.h"
#include "GraphEvent.h"
#include "FilterDesc.h"
#include "TestGraph.h"
#include "mmImaging.h"
#include "utility.h"

HRESULT CreateFrameNumberInstrumenter(EventLoggerType type, void* pEventLoggerData, TestGraph* pTestGraph, IEventLogger** ppEventLogger)
{
	HRESULT hr = S_OK;
	IEventLogger* pEventLogger = NULL;

	switch(type)
	{
	case InsertFrameNumbers:
		pEventLogger = new FrameNumberInstrumenter();
		if (!pEventLogger)
			hr = E_OUTOFMEMORY;
		break;
	
	default:
		hr = E_NOTIMPL;
		break;
	};
	
	if (SUCCEEDED(hr))
	{
		hr = pEventLogger->Init(pTestGraph, type, pEventLoggerData);
		if (FAILED(hr))
			delete pEventLogger;
		else 
			*ppEventLogger = pEventLogger;
	}
	
	return hr;
}


// Decoder output timestamp verifier
FrameNumberInstrumenter::FrameNumberInstrumenter() : 
	m_EventLoggerType(EventLoggerEndMarker),
	m_nSamples(0),
	m_hr(S_OK)

{
	memset(m_szLogFile, 0, sizeof(m_szLogFile));

}

FrameNumberInstrumenter::~FrameNumberInstrumenter()
{
}

HRESULT FrameNumberInstrumenter::Init(TestGraph* pTestGraph, EventLoggerType type, void* pEventLoggerData)
{
	ITapFilter* pTapFilter = NULL;
	HRESULT hr = S_OK;

	FrameNumberInstrumenterData *pInstrumenterData = (FrameNumberInstrumenterData*) pEventLoggerData;

	hr = pTestGraph->InsertTapFilter(pInstrumenterData->filterType, pInstrumenterData->pindir, pInstrumenterData->mediaType, &pTapFilter);
	if (FAILED_F(hr))
	{
		LOG(TEXT("Failed to insert tap filter\n"));
		return hr;
	}

	// Save the media type
	pTapFilter->GetMediaType(&m_mt, PINDIR_INPUT);
	LOG(TEXT("Tapped into types: \n"));
	PrintMediaType(&m_mt);
	// Register a callback
	hr = pTapFilter->RegisterEventCallback(GenericLoggerCallback, pEventLoggerData, (void*)this);
	if (FAILED_F(hr))
	{
		LOG(TEXT("Failed to register callback\n"));
		return hr;
	}

	// Store the verification requested
	m_EventLoggerType = type;

	// Init the actual instrumenter
	hr = m_Instrumenter.Init(&m_mt);

	return hr;
}

HRESULT FrameNumberInstrumenter::ProcessEvent(GraphEvent event, void* pEventData, void* pCallBackData)
{
	HRESULT hr = S_OK;

	if (FAILED_F(m_hr))
		return m_hr;

	if (m_EventLoggerType == InsertFrameNumbers)
	{
		if (event == SAMPLE)
		{
			GraphSample* pSample = (GraphSample*)pEventData;
			IMediaSample* pMediaSample = pSample->pMediaSample;

			// Instrument each frame
			hr = m_Instrumenter.ProcessMediaSample(pMediaSample);

			m_nSamples++;
		}
	}

	return hr;
}

HRESULT FrameNumberInstrumenter::Start()
{
	return S_OK;
}

void FrameNumberInstrumenter::Stop()
{
}

void FrameNumberInstrumenter::Reset()
{
}