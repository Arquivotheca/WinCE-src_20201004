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
#include "stdafx.h"
#include "CPinMapping.h"
#include "..\\HResultError.h"

using namespace MSDvr;

bool SPinMappingCompare::operator () (const TPIPin &arg1, const TPIPin &arg2) const
{
	const IPin *piPin1 = arg1;
	const IPin *piPin2 = arg2;
	return (piPin1 < piPin2);
}

CPinMappings::CPinMappings(CBaseFilter &rcBaseFilter,
						   IMediaTypeAnalyzer *piMediaTypeAnalyzer,
						   IPin *piPinPrimary,
						   bool fMapUnconnectedPins)
	: m_bPrimaryPin(255)
	, m_mapPinToIndex()
	, m_vectorPinStates()
	, m_bConnectedPins(0)
{
	IPin *          piPin;
    IEnumPins *     piEnumPins = NULL;
	UCHAR           bPinIndex = 0;
	ULONG           uFetched;

    HRESULT hr = rcBaseFilter.EnumPins( &piEnumPins );
	if (FAILED(hr))
		throw hr;

	CComPtr<IEnumPins> cComPtrIEnumPins(piEnumPins);
	piEnumPins->Release();

    // Step through every pin -- this routine only has to work
	// with source and sink filters so we don't need to whittle
	// the list down to any particular direction:

	PRIMARY_PIN_QUALITY ePrimaryPinQualityBest = PRIMARY_PIN_QUALITY_NOT_KNOWN;
	CComPtr<IPin> cComPtrIPin = NULL;
    while (S_OK == (hr = piEnumPins->Next( 1L, &piPin, &uFetched)))
    {
		cComPtrIPin.Attach(piPin);
		CComPtr<IPin> piConnectedPin;
		if (FAILED(piPin->ConnectedTo(&piConnectedPin)) ||
			!piConnectedPin)
		{
			piConnectedPin = 0;
			if (!fMapUnconnectedPins)
				continue;  // This pin isn't connected -- don't use it
		}
		else
			++m_bConnectedPins;
		m_mapPinToIndex[piPin] = bPinIndex;
		if (piPinPrimary)
		{
			if (piPin == piPinPrimary)
				m_bPrimaryPin = bPinIndex;
		}
		else if (piConnectedPin)
		{
			PRIMARY_PIN_QUALITY ePrimaryPinQualityThis = IsPrimaryPin(*piPin);
			if (ePrimaryPinQualityThis > ePrimaryPinQualityBest)
			{
				ePrimaryPinQualityBest = ePrimaryPinQualityThis;
				m_bPrimaryPin = bPinIndex;
			}
		}
		SPinState sPinState;
		sPinState.fPinFlushed = false;
		sPinState.fPinFlushing = false;
		sPinState.pcMediaTypeDescription = NULL;
		if (piMediaTypeAnalyzer)
		{
			CMediaType cMediaType;

			if (piConnectedPin)
			{
				if (SUCCEEDED(piPin->ConnectionMediaType(&cMediaType)))
					sPinState.pcMediaTypeDescription = piMediaTypeAnalyzer->AnalyzeMediaType(cMediaType);
			}
			else
			{
				CComPtr<IEnumMediaTypes> piEnumMediaTypes;

				if (SUCCEEDED(piPin->EnumMediaTypes(&piEnumMediaTypes)))
				{
					ULONG cFetched;
					AM_MEDIA_TYPE *pmt[1];

					if (SUCCEEDED(piEnumMediaTypes->Next(1, pmt, &cFetched)) &&
						(cFetched == 1))
					{
						cMediaType = *pmt[0];
						DeleteMediaType(pmt[0]);
						sPinState.pcMediaTypeDescription = piMediaTypeAnalyzer->AnalyzeMediaType(cMediaType);
					}
				}
			}
			if (!sPinState.pcMediaTypeDescription)
				throw CHResultError(E_FAIL);
		};
		m_vectorPinStates.push_back(sPinState);
		++bPinIndex;
	}

	if (m_bPrimaryPin == 255)
		m_bPrimaryPin = 0;
}

CPinMappings::~CPinMappings()
{
	std::vector<SPinState>::iterator iteratorVectorSPinState;

	for (iteratorVectorSPinState = m_vectorPinStates.begin();
		 iteratorVectorSPinState != m_vectorPinStates.end();
		 ++iteratorVectorSPinState)
	{
		SPinState &sPinState = *iteratorVectorSPinState;
		if (sPinState.pcMediaTypeDescription)
		{
			delete sPinState.pcMediaTypeDescription;
			sPinState.pcMediaTypeDescription = NULL;
		}
	}
}

UCHAR CPinMappings::FindPinPos(IPin *piPin) const
{
	CComPtr<IPin> cComPtrIPin(piPin);

	t_PinMapping::const_iterator cItfPos;
	UCHAR pinPos = (cItfPos = m_mapPinToIndex.find(cComPtrIPin)) == m_mapPinToIndex.end() ?
		NULL : cItfPos->second;
	return pinPos;
}

UCHAR CPinMappings::FindPinPos(const CMediaType &rcMediaType) const
{
	t_PinMapping::const_iterator cItfPos;

	for (cItfPos = m_mapPinToIndex.begin();
		 cItfPos != m_mapPinToIndex.end();
		 ++cItfPos)
	{
		CComPtr<IPin> cComPtrIPin = cItfPos->first;

		HRESULT hr;
		AM_MEDIA_TYPE mt;

	    hr = cComPtrIPin->ConnectionMediaType(&mt);
		if (SUCCEEDED(hr))
		{
			CMediaType cMediaType(mt);
			FreeMediaType(mt);
			if (cMediaType == rcMediaType)
			{
				return cItfPos->second;
			}
		}
		// TODO:  Remove this work around once the tests have been upgraded:
		else
			return cItfPos->second;
	}
	throw CHResultError(E_FAIL, "CPinMappings::FindPinPos() called with unknown media type");
}

IPin &CPinMappings::GetPin(UCHAR bPinPos)
{
	t_PinMapping::const_iterator cItfPos;

	for (cItfPos = m_mapPinToIndex.begin();
		 cItfPos != m_mapPinToIndex.end();
		 ++cItfPos)
	{
		if (cItfPos->second == bPinPos)
		{
			return *cItfPos->first;
		}
	}
	throw CHResultError(E_FAIL, "CPinMappings::GetPin() given invalid pin index");
}

CPinMappings::PRIMARY_PIN_QUALITY CPinMappings::IsPrimaryPin(IPin &riPin)
{
	PRIMARY_PIN_QUALITY ePrimaryPinQuality = PRIMARY_PIN_QUALITY_LAST_RESORT;
	
	try {
		HRESULT hr;
		AM_MEDIA_TYPE mt;

	    hr = riPin.ConnectionMediaType(&mt);
		if (SUCCEEDED(hr))
		{
			if (mt.majortype == MEDIATYPE_Video)
				ePrimaryPinQuality = PRIMARY_PIN_QUALITY_IDEAL;
			else if (mt.majortype == MEDIATYPE_Stream)
				ePrimaryPinQuality = PRIMARY_PIN_QUALITY_GOOD;
			else if ((mt.majortype == MEDIATYPE_Audio) ||
					 (mt.majortype == MEDIATYPE_Midi))
				ePrimaryPinQuality = PRIMARY_PIN_QUALITY_POOR;
			FreeMediaType(mt);
		}
	}
	catch (const std::exception& rcException)
	{
		UNUSED (rcException);  // suppress release build warning
#ifdef UNICODE
		DbgLog((LOG_ENGINE_OTHER, 3, _T("CPinMappings::IsPrimaryPin():  caught exception %S"), rcException.what()));
#else
		DbgLog((LOG_ENGINE_OTHER, 3, _T("CPinMappings::IsPrimaryPin():  caught exception %s"), rcException.what()));
#endif
	}
	return ePrimaryPinQuality;
}

SPinState &CPinMappings::GetPinState(UCHAR bPinPos)
{
	return m_vectorPinStates[bPinPos];
}

CMediaTypeDescription::CMediaTypeDescription(CMediaType &rcMediaType)
	: m_cMediaType(rcMediaType)
	, m_uMinimumMediaSampleBufferSize(0)
	, m_uMaximumSamplesPerSecond(0)
	, m_fIsAudio(false)
	, m_fIsVideo(false)
	, m_fIsAVStream(false)
	, m_hyMaxTimeUnitsBetweenKeyFrames(500 * 10000)  // by default, 2 key frames/sec = 500 ms interval
{
}
