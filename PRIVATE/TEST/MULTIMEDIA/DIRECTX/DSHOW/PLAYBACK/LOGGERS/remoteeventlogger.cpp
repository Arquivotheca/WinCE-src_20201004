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
//  Remote Event Logger 
//
//
////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include "ILogger.h"
#include "globals.h"
#include "logging.h"
#include "EventLogger.h"
#include "TestGraph.h"
#include "ValidType.h"
#include "RemoteEvent.h"

HRESULT CreateRemoteEventLogger(EventLoggerType type, void* pEventLoggerData, TestGraph* pTestGraph, IEventLogger** ppEventLogger)
{
	HRESULT hr = S_OK;
	IEventLogger* pEventLogger = NULL;

	switch(type)
	{
	case RemoteEvents:
		pEventLogger = new RemoteEventLogger();
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


RemoteEventLogger::RemoteEventLogger()
{
	// Set the type to something not valid
	m_EventLoggerType = EventLoggerEndMarker;

	m_nSamples = 0;

	memset(&m_mt, 0, sizeof(m_mt));

	m_hr = S_OK;
}

RemoteEventLogger::~RemoteEventLogger()
{
}

HRESULT RemoteEventLogger::Init(TestGraph* pTestGraph, EventLoggerType type, void* pEventLoggerData)
{
	HRESULT hr = S_OK;
	ITapFilter* pTapFilter = NULL;
	RemoteEventLoggerData* pRemoteEventLoggerData = (RemoteEventLoggerData*)pEventLoggerData;

	// BUGBUG: We need a tap filter at the output specified
	if (pRemoteEventLoggerData->pTapPin)
	{
		hr = pTestGraph->InsertTapFilter(pRemoteEventLoggerData->pTapPin, &pTapFilter);
	}
	else {
		hr = pTestGraph->InsertTapFilter(pRemoteEventLoggerData->filterType, pRemoteEventLoggerData->pindir, pRemoteEventLoggerData->mediaType, &pTapFilter);
	}
	if (FAILED_F(hr))
	{
		LOG(TEXT("Failed to insert tap filter\n"));
		return hr;
	}

	// Save the media type
	pTapFilter->GetMediaType(&m_mt, PINDIR_INPUT);

	// Register a callback
	hr = pTapFilter->RegisterEventCallback(GenericLoggerCallback, pEventLoggerData, (void*)this);
	if (FAILED_F(hr))
	{
		LOG(TEXT("Failed to register callback\n"));
		return hr;
	}

	// Store the verification requested
	m_EventLoggerType = type;

	// Save the logger
	m_pLogger = pRemoteEventLoggerData->pLogger;

	// Save the logger name
	_tcsncpy(szLoggerName, pRemoteEventLoggerData->szLoggerName, countof(this->szLoggerName));

	return S_OK;
}

HRESULT RemoteEventLogger::ProcessEvent(GraphEvent event, void* pEventData, void* pCallBackData)
{
	HRESULT hr = S_OK;

	tstring xml = TEXT("");
	tstring prefix = TEXT("\t");

	TCHAR tsz[256];			 			

	if (FAILED_F(m_hr))
		return m_hr;

	DWORD evSize = 0;
	DWORD nBytesWritten = 0;
	if (m_EventLoggerType == RemoteEvents)
	{
		TCHAR* szEvent = TEXT("");
		if (event == SAMPLE)
		{
			GraphSample* pSample = (GraphSample*)pEventData;
			IMediaSample* pMediaSample = pSample->pMediaSample;

			// Check if the media type has changed
			AM_MEDIA_TYPE* pMediaType = NULL;
			hr = pMediaSample->GetMediaType(&pMediaType);
			if (SUCCEEDED_F(hr))
			{
				const char* szTypeName = GetTypeName(pMediaType);
				szTypeName = (NULL == szTypeName) ? "Undetermined" : szTypeName;
				
				// If there was a media type change - log the change
				_stprintf(tsz, _T("Media Type Change: %S\r\n"), szTypeName);

				// Delete the type
				FreeMediaType(*pMediaType);
				DeleteMediaType(pMediaType);
			}

			TCHAR szDiscontinuity[8] = TEXT(" ");
			TCHAR szSyncPoint[8] = TEXT(" ");
			TCHAR szPreroll[8] = TEXT(" ");

			// Log the sample itself
			// Get the start and stop time
			LONGLONG start, stop;
			hr = pMediaSample->GetTime(&start, &stop);
			if (hr != S_OK)
			{
				LOG(TEXT("Failed to get time\n"));
				m_hr = E_FAIL;
				return m_hr;
			}
			
			bool bSyncPoint = (S_OK == pMediaSample->IsSyncPoint());
			bool bPreroll = (S_OK == pMediaSample->IsPreroll());
			long datalen = pMediaSample->GetActualDataLength();
			bool bDiscontinuity = (S_OK == pMediaSample->IsDiscontinuity());

			if (bSyncPoint)
				_tcscpy(szSyncPoint, TEXT("K"));

			if (bPreroll)
				_tcscpy(szPreroll, TEXT("P"));

			if (bDiscontinuity)
				_tcscpy(szDiscontinuity, TEXT("D"));

			// Output time-stamps
			_stprintf(tsz, _T("S%5d %I64d:%I64d %I64d %d %s%s%s\r\n"), m_nSamples, start, stop, pSample->ats, datalen, szSyncPoint, szPreroll, szDiscontinuity);

#if 0
			// Save the sample itself
			BYTE* pBits = NULL;
			pMediaSample->GetPointer(&pBits);

			TCHAR szfile[64];
			_sntprintf(szfile, countof(szfile), TEXT("\\Hard Disk\\Out\\out%d.bmp"), m_nSamples);
			if (m_nSamples%20 == 0)
				SaveBitmap(szfile, &((VIDEOINFOHEADER*)m_mt.pbFormat)->bmiHeader, pBits);
#endif
			m_nSamples++;

			// Serialize the graph event 
			pSample->Serialize(m_GraphEventBuffer, sizeof(m_GraphEventBuffer), &evSize);

			szEvent = TEXT("SAMPLE");
		}
		else if (event == BEGIN_FLUSH)
		{
			GraphBeginFlush* pBeginFlush = (GraphBeginFlush*)pEventData;
			_stprintf(tsz, _T("Begin Flush @ %I64d\r\n"), pBeginFlush->ats);
			pBeginFlush->Serialize(m_GraphEventBuffer, sizeof(m_GraphEventBuffer), &evSize);
			szEvent = TEXT("BEGIN_FLUSH");
		}
		else if (event == END_FLUSH)
		{
			GraphEndFlush* pEndFlush = (GraphEndFlush*)pEventData;
			_stprintf(tsz, _T("End Flush @ %I64d\r\n"), pEndFlush->ats);
			pEndFlush->Serialize(m_GraphEventBuffer, sizeof(m_GraphEventBuffer), &evSize);
			szEvent = TEXT("END_FLUSH");
		}
		else if (event == NEW_SEG)
		{
			GraphNewSegment* pNewSeg = (GraphNewSegment*)pEventData;
			_stprintf(tsz, _T("NewSeg @ %I64d, start: %I64d, stop: %I64d, rate: %f \r\n"), pNewSeg->ats, pNewSeg->start, pNewSeg->stop, pNewSeg->rate);
			pNewSeg->Serialize(m_GraphEventBuffer, sizeof(m_GraphEventBuffer), &evSize);
			szEvent = TEXT("NEW_SEG");
		}
		else if (event == QUERY_ACCEPT)
		{
			GraphQueryAccept* pGraphQueryAccept = (GraphQueryAccept*)pEventData;
			const char* szTypeName = GetTypeName(pGraphQueryAccept->pMediaType);
			szTypeName = (NULL == szTypeName) ? "Undetermined" : szTypeName;
			_stprintf(tsz, _T("QUERY_ACCEPT @ %I64d, Type: %S, bAccept: %d\r\n"), pGraphQueryAccept->ats, szTypeName, pGraphQueryAccept->bAccept);
			pGraphQueryAccept->Serialize(m_GraphEventBuffer, sizeof(m_GraphEventBuffer), &evSize);
			szEvent = TEXT("QUERY_ACCEPT");
		}
		else if (event == EOS)
		{
			GraphEndOfStream* pEndOfStream = (GraphEndOfStream*)pEventData;
			_stprintf(tsz, _T("EOS @ %I64d\r\n"), pEndOfStream->ats);
			pEndOfStream->Serialize(m_GraphEventBuffer, sizeof(m_GraphEventBuffer), &evSize);
			szEvent = TEXT("EOS");
		}
		else if (event == STATE_CHANGE_PAUSE)
		{
			GraphPause* pPause = (GraphPause*)pEventData;
			_stprintf(tsz, _T("Pause @ %I64d\r\n"), pPause->ats);
			pPause->Serialize(m_GraphEventBuffer, sizeof(m_GraphEventBuffer), &evSize);
			szEvent = TEXT("PAUSE");
		}
		else if (event == STATE_CHANGE_RUN)
		{
			GraphRun* pRun = (GraphRun*)pEventData;
			_stprintf(tsz, _T("Run @ %I64d\r\n"), pRun->ats);
			pRun->Serialize(m_GraphEventBuffer, sizeof(m_GraphEventBuffer), &evSize);
			szEvent = TEXT("RUN");
		}
		else if (event == STATE_CHANGE_STOP)
		{
			GraphStop* pStop = (GraphStop*)pEventData;
			_stprintf(tsz, _T("Stop @ %I64d\r\n"), pStop->ats);
			pStop->Serialize(m_GraphEventBuffer, sizeof(m_GraphEventBuffer), &evSize);
			szEvent = TEXT("STOP");
		}

		
		// Serialize the remote graph event
		RemoteGraphEvent::Serialize(m_RemoteEventBuffer, sizeof(m_RemoteEventBuffer), 
									szLoggerName, countof(szLoggerName),
									event, m_GraphEventBuffer, evSize, 
									&nBytesWritten);
		
		// Log the remote event
		if (m_pLogger)
		{
			m_pLogger->Log(m_RemoteEventBuffer, nBytesWritten);
		}
		LOG(szEvent);

	}

	return hr;
}

HRESULT RemoteEventLogger::Start()
{
	return S_OK;
}

void RemoteEventLogger::Stop()
{
	// BUGBUG: Remove the tap filter
}

void RemoteEventLogger::Reset()
{
	// Flush the log so far
	// Reset for a fresh capture
}