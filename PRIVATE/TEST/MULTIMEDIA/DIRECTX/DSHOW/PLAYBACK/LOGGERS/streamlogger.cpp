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
//  Logger 
//
//
////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>
#include "globals.h"
#include "logging.h"
#include "EventLogger.h"
#include "GraphEvent.h"
#include "FilterDesc.h"
#include "TestGraph.h"
#include "StreamLogger.h"
#include "TapFilter.h"

// Stream Logger factory implementation
HRESULT CreateStreamLogger(EventLoggerType type, void* pEventLoggerData, TestGraph* pTestGraph, IEventLogger** ppEventLogger)
{
	HRESULT hr = S_OK;
	IEventLogger* pEventLogger = NULL;

	switch(type)
	{
	case StreamData:
		pEventLogger = new StreamLogger();
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

// Constructor
StreamLogger::StreamLogger()
{

}

// Destructor
StreamLogger::~StreamLogger()
{
	SAFE_CLOSEHANDLE(m_hLogFile);
	SAFE_CLOSEHANDLE(m_hCheckSumFile);
}

// Init: Sets up the tap filters and adds them to the graph
HRESULT StreamLogger::Init(TestGraph* pTestGraph, EventLoggerType type, void* pEventLoggerData)
{
	HRESULT hr = E_FAIL;
	ITapFilter* pTapFilter = NULL;
	StreamLoggerData* pStreamLoggerData = (StreamLoggerData*)pEventLoggerData;
	StreamLoggerData* pTapFilterData = NULL;
	FilterDesc* pFilterDesc = NULL;
	IBaseFilter* pDataFilter = NULL;
	IEnumPins* pEnumPins = NULL;
	IPin* pPin = NULL;
	PIN_DIRECTION pinDir;
	TCHAR *szToken = NULL;
	DWORD dwStreams = 0;
	

	if((NULL == pTestGraph) || NULL == pEventLoggerData)
	{
		LOG(_T("StreamLogger::Init(): pTestGraph and/or pEventLoggerData passed in as a null value.\n"));
		goto cleanup;
	}

	// Find the filter that we are going to add the tap filter to
	if(NULL == (pFilterDesc = pTestGraph->GetFilterDesc(pStreamLoggerData->primaryType)))
	{
		if(NULL == (pFilterDesc = pTestGraph->GetFilterDesc(pStreamLoggerData->secondaryType)))
		{
			hr = E_FAIL;
			LOG(_T("StreamLogger::Init(): Unable to find matching filter to tap.\n"));
			goto cleanup;
		}
	}

	if(NULL == (pDataFilter = pFilterDesc->GetFilterInstance()))
	{
		hr = E_FAIL;
		LOG(_T("StreamLogger::Init(): Unable to retrieve instance of filter to tap.\n"));
		goto cleanup;
	}

	// Enumerate all of the pins and insert tap filters on outputs
	hr = pDataFilter->EnumPins(&pEnumPins);
	if(FAILED_F(hr))
	{
		LOG(_T("StreamLogger::Init(): Failed to retrieve IEnumPins interface\n"));
		goto cleanup;
	}

	while(pEnumPins->Next(1, &pPin, 0) == S_OK)
	{
		memset(&pinDir, 0, sizeof(PIN_DIRECTION));
		hr = pPin->QueryDirection(&pinDir);
		if( FAILED_F(hr) )
		{
			LOG(_T("StreamLogger::Init(): Failed to obtain the direction of an output pin\n"));
			goto cleanup;
		}
		if(PINDIR_OUTPUT == pinDir)
		{	
			pTapFilter = NULL;
			pTestGraph->InsertTapFilter(pPin, &pTapFilter);
			if( FAILED_F(hr) || NULL == pTapFilter )
			{
				LOG(_T("StreamLogger::Init(): Failed to insert a tap filter into the test graph\n"));
				goto cleanup;
			}

			// Make a seperate logger data structure for each pin so that they know what file to output to.
			pTapFilterData = new StreamLoggerData(pStreamLoggerData);
            if(NULL == pTapFilterData)
            {
                LOG(_T("StreamLogger::Init(): Failed to create a copy of the logger data.\n"));
                hr = E_OUTOFMEMORY;
                goto cleanup;
            }
			szToken = pTapFilterData->szOutFile + _tcslen(pTapFilterData->szOutFile);
			_itot(++dwStreams, szToken, 10);
			szToken = pTapFilterData->szOutFile + _tcslen(pTapFilterData->szOutFile);

			if(pStreamLoggerData->bLogStream)
			{
				_tcscpy(szToken, _T(".stm"));
				pTapFilterData->hStreamFile = CreateFile(pTapFilterData->szOutFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
				if (INVALID_HANDLE_VALUE == pTapFilterData->hStreamFile)
				{
					hr = E_FAIL;
					LOG(_T("StreamLogger::Init(): Error when trying to create stream output file %s \n"), pTapFilterData->szOutFile);
					SAFE_DELETE(pTapFilterData);
					goto cleanup;
				}
			}

			if(pStreamLoggerData->bLogChecksum)
			{
				_tcscpy(szToken, _T(".csm"));
				pTapFilterData->hChecksumFile = CreateFile(pTapFilterData->szOutFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
				if (INVALID_HANDLE_VALUE == pTapFilterData->hChecksumFile)
				{
					hr = E_FAIL;
					LOG(_T("StreamLogger::Init(): Error when trying to create checksum output file %s \n"), pTapFilterData->szOutFile);
					SAFE_DELETE(pTapFilterData);
					goto cleanup;
				}
			}
			// Register a callback
			hr = pTapFilter->RegisterEventCallback(GenericLoggerCallback, (void*)pTapFilterData, (void*)this);
			if (FAILED_F(hr))
			{
				LOG(_T("StreamLogger::Init(): Failed to register callback\n"));
				SAFE_DELETE(pTapFilterData);
				goto cleanup;
			}

			// SAFE_RELEASE(pTapFilter);
		}	
	}

cleanup:

	SAFE_RELEASE(pDataFilter);
	SAFE_RELEASE(pEnumPins);
	SAFE_RELEASE(pPin);

	return S_OK;
}

// Callback method called by the tap filter
HRESULT StreamLogger::ProcessEvent(GraphEvent event, void* pEventData, void* pTapCallbackData)
{
	HRESULT hr = S_OK;
	StreamLoggerData* pLoggerData = (StreamLoggerData *)pTapCallbackData; 


	if(NULL == pLoggerData)
	{
		LOG(_T("StreamLogger::ProcessEvent(): pLoggerData is null.\n"));
		hr = E_FAIL;
		goto cleanup;
	}

	if (event == SAMPLE)
	{
		IMediaSample* pMediaSample = ((GraphSample *)pEventData)->pMediaSample;	
		DWORD dwBytesWritten = 0;
		DWORD dwCheckSum = 0;
		BYTE* pBits = NULL;

		hr = pMediaSample->GetPointer(&pBits);
		if(FAILED_F(hr))
		{
			LOG(_T("StreamLogger::ProcessEvent(): Unable to retrieve buffer pointer.\n"));
			hr = E_FAIL;
			goto cleanup;
		}

		// Write the sample to a file
		if(pLoggerData->bLogStream)
		{
			if(!WriteFile(pLoggerData->hStreamFile, pBits, pMediaSample->GetSize(), &dwBytesWritten, NULL))
			{
				LOG(_T("StreamLogger::ProcessEvent(): Sample WriteFile failed.\n"));
				hr = E_FAIL;
				goto cleanup;
			}
		}

		// Compute and write the checksum
		if(pLoggerData->bLogChecksum)
		{
			for(DWORD * pCurr = (DWORD *)pBits; pCurr < (DWORD *)(pBits + pMediaSample->GetSize() + 3); pCurr++)
				dwCheckSum += *pCurr;

			if(!WriteFile(pLoggerData->hChecksumFile, &dwCheckSum, sizeof(DWORD), &dwBytesWritten, NULL))
			{
				LOG(_T("StreamLogger::ProcessEvent(): CheckSum WriteFile failed.\n"));
				hr = E_FAIL;
				goto cleanup;
			}
		}
	
		// Tell the tap filter to drop the sample if not rendering
		if(!FAILED_F(hr) && !pLoggerData->bRenderStream)
		{
			hr = TF_S_DROPSAMPLE;
		}
	}
cleanup:

	return hr;
}


HRESULT StreamLogger::Start()
{
	return S_OK;
}

void StreamLogger::Stop()
{
	// BUGBUG: Remove the tap filters
}

void StreamLogger::Reset()
{
	// Flush the log so far
	// Reset for a fresh capture
}
