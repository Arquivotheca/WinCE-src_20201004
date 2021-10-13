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
#include "..\\PipelineInterfaces.h"
#include "DvrInterfaces.h"
#include "..\\DVREngine.h"
#include "MPEGDecoderDriver.h"
#include "..\\HResultError.h"
#include <InitGuid.h>
#include <decloc.h>
#include "..\\SampleProducer\\ProducerConsumerUtilities.h"
#include "..\\Plumbing\\Source.h"

using namespace MSDvr;

#define MPEG_DECODER_MAX_READER_FRAME_RATE	g_dblMPEGReaderFullFrameRate
#define MPEG_DECODER_MIN_READER_FRAME_RATE	g_dblMPEGReaderFullFrameRate

// #define CHATTER_ABOUT_GRAPH_SETUP

///////////////////////////////////////////////////////////////////////
//
//  Class CMPEGDecoderDriver -- overrides for handling MPEG
//
///////////////////////////////////////////////////////////////////////

CMPEGDecoderDriver::CMPEGDecoderDriver(void)
	: CDecoderDriver()
	, m_cMPEGMediaTypeAnalyzer()
	, m_piPrimaryPinPropertySet(0)
	, m_lRateChangePropertySetListeners()
	, m_fDiscardingToRateChange(false)
	, m_fDiscardingToSyncPointForDiscontinuity(false)
	, m_fFirstSampleAtNewRate(false)
	, m_fDiscardingPostQueueUntilSyncPoint(false)
#ifdef SUPPRESS_DUPLICATE_SAMPLES
	, m_hyLastPositionSentDownstream(-1)
#endif // SUPPRESS_DUPLICATE_SAMPLES
#ifdef CHATTER_ABOUT_POST_FLUSH_ACTIONS
	, m_dwKeyFramesUntilChatterStops(2)
#endif // CHATTER_ABOUT_POST_FLUSH_ACTIONS
{
	DbgLog((ZONE_DECODER_DRIVER, 2, _T("CMPEGDecoderDriver: constructed %p\n"), this));

	m_piMediaTypeAnalyzer = &m_cMPEGMediaTypeAnalyzer;
}

CMPEGDecoderDriver::~CMPEGDecoderDriver(void)
{
	DbgLog((ZONE_DECODER_DRIVER, 2, _T("CMPEGDecoderDriver: destroying %p\n"), this));
	Cleanup();
}

ROUTE CMPEGDecoderDriver::ProcessQueuedSample(CDecoderDriverQueueItem &rcDecoderDriverQueueItem)
{
	ROUTE eRoute = HANDLED_STOP;

#ifdef SUPPRESS_DUPLICATE_SAMPLES
	if (m_hyLastPositionSentDownstream >= 0)
	{
		if (m_hyLastPositionSentDownstream == rcDecoderDriverQueueItem.m_hyNominalStartPosition)
		{
			DbgLog((LOG_WARNING, 1, _T("### DVR Source Filter:  chucking a duplicate sample [consumer has a bug] ###\n") ));
			goto DoneWithSample;
		}
		else if ((m_dblRate >= 0) &&
				 (m_hyLastPositionSentDownstream > rcDecoderDriverQueueItem.m_hyNominalStartPosition))
		{
			DbgLog((LOG_WARNING, 1, _T("### DVR Source Filter:  chucking a backwards sample while playing forward ###\n") ));
			goto DoneWithSample;
		}
		else if ((m_dblRate < 0) &&
				 (m_hyLastPositionSentDownstream < rcDecoderDriverQueueItem.m_hyNominalStartPosition))
		{
			DbgLog((LOG_WARNING, 1, _T("### DVR Source Filter:  chucking a forwards sample while playing backwards ###\n")));
			goto DoneWithSample;
		}
	}
	m_hyLastPositionSentDownstream = rcDecoderDriverQueueItem.m_hyNominalStartPosition;
#endif // SUPPRESS_DUPLICATE_SAMPLES
	eRoute = CDecoderDriver::ProcessQueuedSample(rcDecoderDriverQueueItem);

DoneWithSample:

	if (m_fDiscardingPostQueueUntilSyncPoint)
	{
		if ((eRoute != HANDLED_STOP) && (eRoute != UNHANDLED_STOP))
		{
			if (rcDecoderDriverQueueItem.m_piMediaSample->IsSyncPoint() == S_OK)
			{
				// We're going to allow this sync point downstream.  It is the first
				// after a discontinuity, so since we suppresses the earlier non-sync
				// discontinuities, this sample needs to be marked as a discontinuity.

				DbgLog((LOG_SOURCE_STATE, 3, TEXT("MPEGDecoderDriver::ProcessQueuedSample() -- resuming transmission of queued samples\n") ));
				m_fDiscardingPostQueueUntilSyncPoint = false;
				rcDecoderDriverQueueItem.m_piMediaSample->SetDiscontinuity(TRUE);
			}
			else
			{
				// We're still looking for a sync point
				DbgLog((LOG_SOURCE_STATE, 3, TEXT("MPEGDecoderDriver::ProcessQueuedSample() -- discarding another non-sync-point sample\n") ));
				eRoute = HANDLED_STOP;
			}
		}
	}
	else if (rcDecoderDriverQueueItem.m_piMediaSample->IsDiscontinuity() == S_OK)
	{
		if ((eRoute == HANDLED_STOP) || (eRoute == UNHANDLED_STOP))
		{
			// We crossed a discontinuity. We're not going to send it downstream.
			// We will start sending samples back downstream once we get a sync-point.

			DbgLog((LOG_SOURCE_STATE, 3, TEXT("MPEGDecoderDriver::ProcessQueuedSample() -- non-deliverable discontinuity, beginning post-queue discard\n") ));
			m_fDiscardingPostQueueUntilSyncPoint = true;
		}
		else if (rcDecoderDriverQueueItem.m_piMediaSample->IsSyncPoint() != S_OK)
		{
			// We are looking at a non-sync-point discontinuity. The sync-point
			// discontinuity must've been discarded due to some odd codepath.
			// Discard this and all subsequent non-sync-point samples in the queue.

			DbgLog((LOG_SOURCE_STATE, 3, TEXT("MPEGDecoderDriver::ProcessQueuedSample() -- found queued non-sync discontinuity, beginning post-queue discard\n") ));
			m_fDiscardingPostQueueUntilSyncPoint = true;
			eRoute = HANDLED_STOP;
		}
	}
	if (m_fFirstSampleAtNewRate)
	{
		if ((eRoute == HANDLED_STOP) || (eRoute == UNHANDLED_STOP))
		{
			DbgLog((LOG_SOURCE_STATE, 3, TEXT("MPEGDecoderDriver::ProcessQueuedSample() -- discarding first sample at new rate, beginning post-queue discard\n") ));
			m_fFirstSampleAtNewRate = false;
			m_fDiscardingPostQueueUntilSyncPoint = true;
		}
		else
			m_fFirstSampleAtNewRate = false;
	}
#ifdef CHATTER_ABOUT_POST_FLUSH_ACTIONS
	if (m_dwKeyFramesUntilChatterStops)
	{
		TCHAR pwszMsg[128];

		_stprintf(pwszMsg, TEXT("MPEG Decoder Driver:  %s %s%ssample at a/v position %I64d\n"),
			((eRoute == HANDLED_STOP) || (eRoute == UNHANDLED_STOP)) ? TEXT("discarded") : TEXT("dispatched"),
			(rcDecoderDriverQueueItem.m_piMediaSample->IsDiscontinuity() == S_OK) ? TEXT("discontinuity ") : TEXT(""),
			(rcDecoderDriverQueueItem.m_piMediaSample->IsSyncPoint () == S_OK) ? TEXT("sync-point ") : TEXT(""),
			rcDecoderDriverQueueItem.m_hyNominalStartPosition);
		OutputDebugString(pwszMsg);

		if (rcDecoderDriverQueueItem.m_piMediaSample->IsSyncPoint () == S_OK)
			--m_dwKeyFramesUntilChatterStops;
	}
#endif // CHATTER_ABOUT_POST_FLUSH_ACTIONS

	return eRoute;
}

ROUTE CMPEGDecoderDriver::ConfigurePipeline(
						UCHAR			iNumPins,
						CMediaType		cMediaTypes[],
						UINT			iSizeCustom,
						BYTE			Custom[])
{
	return CDecoderDriver::ConfigurePipeline(iNumPins, cMediaTypes, iSizeCustom, Custom);
}

void CMPEGDecoderDriver::RefreshDecoderCapabilities()
{
	if (m_fKnowDecoderCapabilities)
		return;

	CDecoderDriver::RefreshDecoderCapabilities();

	m_hyPreroll = 0;

	// Magic values:  tune these downward to increase the CPU load and frame rate
	//				  during fast forward and rewind. Tune these download to reduce.
	m_dblFrameToKeyFrameRatio = g_dblMPEGDecoderDriverKeyFrameRatio;
	m_dblSecondsToKeyFrameRatio = g_dblMPEGDecoderDriverKeyFramesPerSecond;

	if (m_eFilterState == State_Stopped)
		m_fKnowDecoderCapabilities = false;
}

void CMPEGDecoderDriver::Cleanup()
{
	CDecoderDriver::Cleanup();
	m_piPrimaryPinPropertySet = NULL;

	std::list<IKsPropertySet*>::iterator iter;
	for (iter = m_lRateChangePropertySetListeners.begin();
		iter != m_lRateChangePropertySetListeners.end();
		++iter)
	{
		IKsPropertySet *piKsPropertySet = *iter;
		piKsPropertySet->Release();
	}
	m_lRateChangePropertySetListeners.clear();

	m_fDiscardingToRateChange = false;
	m_fDiscardingToSyncPointForDiscontinuity = false;
	m_fDiscardingPostQueueUntilSyncPoint = false;
	m_fFirstSampleAtNewRate = false;
#ifdef SUPPRESS_DUPLICATE_SAMPLES
	m_hyLastPositionSentDownstream = -1;
#endif // SUPPRESS_DUPLICATE_SAMPLES
}

void CMPEGDecoderDriver::ExtractPositions(IMediaSample &riMediaSample,
				CDVROutputPin &rcDVROutputPin,
				LONGLONG &hyEarliestAudioPosition,
				LONGLONG &hyEarliestVideoPosition,
				LONGLONG &hyNominalStartPosition,
				LONGLONG &hyNominalEndPosition,
				LONGLONG &hyLatestAudioPosition,
				LONGLONG &hyLatestVideoPosition)
{
	CDecoderDriver::ExtractPositions(riMediaSample, rcDVROutputPin,
		hyEarliestAudioPosition, hyEarliestVideoPosition,
		hyNominalStartPosition, hyNominalEndPosition,
		hyLatestAudioPosition, hyLatestVideoPosition);
}

void CMPEGDecoderDriver::ResetLatestTimes()
{
	CDecoderDriver::ResetLatestTimes();
#ifdef SUPPRESS_DUPLICATE_SAMPLES
	m_hyLastPositionSentDownstream = -1;
#endif // SUPPRESS_DUPLICATE_SAMPLES
}

void CMPEGDecoderDriver::EnactRateChange(double dblRate)
{
	DbgLog((LOG_SOURCE_STATE, 3, _T("CMPEGDecoderDriver(%p)::EnactRateChange(%lf) -- entry\n"), this, dblRate));

	CDecoderDriver::EnactRateChange(dblRate);

	// Make sure we have our pin mappings, including a reference to the IKsPropertySet
	// interface of the input pin connected to our primary output pin:

	if (!m_pcPinMappings)
		InitPinMappings();

	m_fDiscardingToRateChange = true;
	m_fFirstSampleAtNewRate = false;
	DbgLog((LOG_SOURCE_STATE, 3, _T("CMPEGDecoderDriver(%p)::EnactRateChange(%lf) -- exit\n"), this, dblRate));
} // CMPEGDecoderDriver::EnactRateChange

void CMPEGDecoderDriver::InitPinMappings()
{
	if (m_pcPinMappings)
		return;

	ASSERT(!m_piPrimaryPinPropertySet);

	if (!m_pippmgr)
		return;

	CDecoderDriver::InitPinMappings();
	if (!m_pcPinMappings)
		return;

	CDVROutputPin &rcPrimaryPin = m_pippmgr->GetPrimaryOutput();
	CComPtr<IPin> piPrimaryPinConnectedTo;

    if (FAILED(rcPrimaryPin.ConnectedTo(&piPrimaryPinConnectedTo)) ||
		!piPrimaryPinConnectedTo)
	{
		DbgLog((LOG_ERROR, 2, _T("CMPEGDecoderDriver::InitPinMappings() failed to find connected pin\n")));
		return;
	}

  if (FAILED(piPrimaryPinConnectedTo->QueryInterface(IID_IKsPropertySet, (void **)&m_piPrimaryPinPropertySet)) ||
	  !m_piPrimaryPinPropertySet)
  {
		DbgLog((LOG_ERROR, 2, _T("CMPEGDecoderDriver::InitPinMappings() failed to find IKsPropertySet on connected pin\n")));
		return;
  }

  CacheRateChangePropertySetListeners();
} // CMPEGDecoderDriver::InitPinMappings

HRESULT CMPEGDecoderDriver::IsSeekRecommendedForRate(double dblRate, LONGLONG &hyRecommendedPosition)
{
	CAutoLock cAutoLock(&m_cCritSec);

	// Ask the downstream filter for information about where we are decoding if
	// it makes sense to do so (i.e., we don't know of any pending reason to change
	// the rate etc.):

	hyRecommendedPosition = -1;
	if (!m_fSawSample || !m_dblRate ||
		(m_fThrottling && (m_fPositionDiscontinuity || m_fPendingRateChange)))
		return S_OK;

	LONGLONG hyRenderingPosition = EstimatePlaybackPosition(true);
	if (hyRenderingPosition < 0)
		return S_OK;

	// The downstream filter gave us info about where it is rendering now. Use that info plus
	// the current stream time and the A/V position of the latest sample sent downstream (or
	// in our queue) to estimate when we will be done rendering what is in the queue:

	REFERENCE_TIME rtNow = m_cClockState.GetStreamTime();
	LONGLONG hyLatestPositionQueuedNow = m_rtLatestRecdStreamEndTime;

	std::list<CDecoderDriverQueueItem>::iterator iter;
	bool fFoundReasonToStop = false;
	
	for (iter = m_listCDecoderDriverQueueItem.begin();
		!fFoundReasonToStop && (iter != m_listCDecoderDriverQueueItem.end());
		++iter)
	{
		CDecoderDriverQueueItem &cDecoderDriverQueueItem = *iter;
		switch (cDecoderDriverQueueItem.m_eQueueItemType)
		{
		case CDecoderDriverQueueItem::DECODER_DRIVER_ITEM_SAMPLE:
			if ((m_dblRate > 0) &&
				(hyLatestPositionQueuedNow < cDecoderDriverQueueItem.m_hyNominalEndPosition))
				hyLatestPositionQueuedNow = cDecoderDriverQueueItem.m_hyNominalEndPosition;
			else if ((m_dblRate < 0) &&
				(hyLatestPositionQueuedNow > cDecoderDriverQueueItem.m_hyNominalStartPosition))
				hyLatestPositionQueuedNow = cDecoderDriverQueueItem.m_hyNominalStartPosition;
			break;

		case CDecoderDriverQueueItem::DECODER_DRIVER_ITEM_END_OF_STREAM:
			// This is a pending stop -- we'll need to do a flushing seek to clear it
			return S_OK;

		case CDecoderDriverQueueItem::DECODER_DRIVER_ITEM_EXTENSION:
			if ((cDecoderDriverQueueItem.m_pcExtendedRequest->m_iPipelineComponentPrimaryTarget == this) &&
				(cDecoderDriverQueueItem.m_pcExtendedRequest->m_eExtendedRequestType ==
				CExtendedRequest::DECODER_DRIVER_SEEK_COMPLETE))
			{
				// This is a pending seek -- don't trust previously sent samples
				return S_OK;
			}
			break;

		default:
			ASSERT(0);
			break;
		}
	}

	if (hyLatestPositionQueuedNow < 0)
		return S_OK;		// We don't know where we are so we can't make a recommendation.

	LONGLONG hyEstimatedRenderingTime = rtNow +
		(LONGLONG) (((double)(hyLatestPositionQueuedNow - hyRenderingPosition)) / m_dblRate);
	if ((hyEstimatedRenderingTime >= rtNow) &&
		(hyEstimatedRenderingTime > rtNow + s_hyMaximumWaitForRate))
	{
		hyRecommendedPosition = hyRenderingPosition;
		// The MPEG filter graph always needs a flush when changing rate:
		return S_OK;
	}

	return S_OK;
}

void CMPEGDecoderDriver::UpdateXInterceptAsNeeded(const CDecoderDriverQueueItem &rcDecoderDriverQueueItem)
{
	// The downstream filter takes care of the mapping -- optimize this down to keeping the intercept
	// always 0.

	if (!m_fSawSample || (m_fThrottling && (m_fPositionDiscontinuity || m_fPendingRateChange)))
	{
		m_rtXInterceptTime = 0;

		if (!m_fSawSample || !m_fPendingRateChange)
			m_rtXInterceptTimePrior = m_rtXInterceptTime;

		DbgLog((ZONE_DECODER_DRIVER, 3,
			_T("CMPEGDecoderDriver::UpdateXInterceptAsNeeded(): x-intercept is always 0\n")));

		m_fSawSample = true;
		m_fPositionDiscontinuity = false;
		m_fPendingRateChange = false;
	}
}

HRESULT CMPEGDecoderDriver::SetPresentationTime(IMediaSample &, REFERENCE_TIME &, REFERENCE_TIME &)
{
	// The downstream filter handles the remapping for MPEG. We do nothing.
	return S_OK;
} // CMPEGDecoderDriver::SetPresentationTime

LONGLONG CMPEGDecoderDriver::EstimatePlaybackPosition(bool fExemptEndOfStream, bool fAdviseIfEndOfMedia)
{
	CAutoLock cAutoLock(&m_cCritSec);

	// Step 0:  if we have sent an end-of-media event and are asked to return a sentinel
	//          in such a scenario, do so.

	if (fAdviseIfEndOfMedia)
	{
		switch (m_eDecoderDriverMediaEndStatus)
		{
		case DECODER_DRIVER_START_OF_MEDIA_SENT:
			return DECODER_DRIVER_PLAYBACK_POSITION_START_OF_MEDIA;
		
		case DECODER_DRIVER_END_OF_MEDIA_SENT:
			return DECODER_DRIVER_PLAYBACK_POSITION_END_OF_MEDIA;

		default:
			break;
		}
	}

	// Step 1:  verify that the situation allows us to reliably convert
	// between A/V positions and stream times. If not, return the
	// sentinel -1:

	if ((m_fAtEndOfStream && !fExemptEndOfStream) ||
		!m_fSawSample ||
		m_fFlushing ||
		m_fPositionDiscontinuity)
	{
		DbgLog((ZONE_DECODER_DRIVER, 3,
			_T("CMPEGDecoderDriver::EstimatePlaybackPosition():  return -1 due to %s\n"),
			(m_fAtEndOfStream  && !fExemptEndOfStream) ? _T("end of stream") :
				(!m_fSawSample ? _T("no sample seen") : (m_fFlushing ? _T("flushing") : _T("position discontinuity"))) ));
		return -1;
	}

	// Step 2: ask downstream to find out the current A/V position:

	if (!m_piPrimaryPinPropertySet)
	{
		DbgLog((ZONE_DECODER_DRIVER, 3,
			_T("CMPEGDecoderDriver::EstimatePlaybackPosition():  return -1 due to no IKsPropertySet to query\n") ));
		return -1;
	}

	LONGLONG hyEstimatedPos;
	DWORD dwRet;
	HRESULT hr = m_piPrimaryPinPropertySet->Get(AM_KSPROPSETID_DVR_DecoderLocation, AM_RATE_DecoderPosition,
		NULL, 0, &hyEstimatedPos, sizeof(LONGLONG), &dwRet);

	if (FAILED(hr))
	{
		hr = m_piPrimaryPinPropertySet->Get(AM_KSPROPSETID_RendererPosition, AM_PROPERTY_CurrentPosition,
							NULL, 0, &hyEstimatedPos, sizeof(LONGLONG), &dwRet);
        if (FAILED(hr))
       	{
		    DbgLog((ZONE_DECODER_DRIVER, 3,
			    _T("CMPEGDecoderDriver::EstimatePlaybackPosition():  return -1 due to failure attempting to query IKsPropertySet\n") ));
		    return -1;
       	}
	}

	// Step 3:  validate the result by capping it with the A/V position
	// of the next sample in the (known) queue:

	std::list<CDecoderDriverQueueItem>::iterator iter;
	bool fFoundReasonToStop = false;
	
	for (iter = m_listCDecoderDriverQueueItem.begin();
		!fFoundReasonToStop && (iter != m_listCDecoderDriverQueueItem.end());
		++iter)
	{
		CDecoderDriverQueueItem &cDecoderDriverQueueItem = *iter;
		switch (cDecoderDriverQueueItem.m_eQueueItemType)
		{
		case CDecoderDriverQueueItem::DECODER_DRIVER_ITEM_SAMPLE:
			if ((m_dblRate > 0) &&
				(hyEstimatedPos > cDecoderDriverQueueItem.m_hyNominalStartPosition))
				hyEstimatedPos = cDecoderDriverQueueItem.m_hyNominalStartPosition - 1;
			else if ((m_dblRate < 0) &&
				(hyEstimatedPos < cDecoderDriverQueueItem.m_hyNominalEndPosition))
				hyEstimatedPos = cDecoderDriverQueueItem.m_hyNominalEndPosition + 1;
			fFoundReasonToStop = true;
			break;

		case CDecoderDriverQueueItem::DECODER_DRIVER_ITEM_END_OF_STREAM:
			fFoundReasonToStop = true;
			break;

		case CDecoderDriverQueueItem::DECODER_DRIVER_ITEM_EXTENSION:
			if ((cDecoderDriverQueueItem.m_pcExtendedRequest->m_iPipelineComponentPrimaryTarget == this) &&
				(cDecoderDriverQueueItem.m_pcExtendedRequest->m_eExtendedRequestType ==
				CExtendedRequest::DECODER_DRIVER_SEEK_COMPLETE))
			{
				// This is a pending seek -- don't trust previously sent samples

				DbgLog((ZONE_DECODER_DRIVER, 3,
					_T("CMPEGDecoderDriver::EstimatePlaybackPosition():  return -1 due to seek-in-progress\n") ));
				return -1;
			}
			break;

		default:
			ASSERT(0);
			break;
		}
	}
	ASSERT(hyEstimatedPos >= 0);
	return (hyEstimatedPos >= 0) ? hyEstimatedPos : -1;
} // CMPEGDecoderDriver::EstimatePlaybackPosition

void CMPEGDecoderDriver::QueryGraphForCapabilities()
{
	if (!m_pippmgr)
		return;

	InitPinMappings();

	if (!m_piPrimaryPinPropertySet)
		return;

	/* Ask for the full-frame, maximum forward and backward rates, rate change protocol */
	DWORD cbReturned;
	AM_MaxFullDataRate lMaxDataRate;
	if (SUCCEEDED(m_piPrimaryPinPropertySet->Get(AM_KSPROPSETID_TSRateChange,
									AM_RATE_MaxFullDataRate, NULL, 0,
									&lMaxDataRate, sizeof(lMaxDataRate), &cbReturned)))
	{
		m_sAMMaxFullDataRate = lMaxDataRate;
		double dblMaxForwardOrBackward = (((double) lMaxDataRate / (double)g_uRateScaleFactor));
		m_dblMaxFrameRateForward = dblMaxForwardOrBackward;
		m_dblMaxFrameRateBackward = dblMaxForwardOrBackward;
	}

	// Enforce an empiric limit on how fast the Reader can churn out data
	// -- the Reader might be the bottleneck:
	if (m_dblMaxFrameRateForward > MPEG_DECODER_MAX_READER_FRAME_RATE)
		m_dblMaxFrameRateForward = MPEG_DECODER_MAX_READER_FRAME_RATE;
	if (m_dblMaxFrameRateBackward > MPEG_DECODER_MIN_READER_FRAME_RATE)
		m_dblMaxFrameRateBackward = MPEG_DECODER_MIN_READER_FRAME_RATE;

#ifndef _WIN32_WCE
	AM_QueryRate frameRateReturns;
	if (SUCCEEDED(m_piPrimaryPinPropertySet->Get(AM_KSPROPSETID_TSRateChange,
									AM_RATE_QueryFullFrameRate, NULL, 0,
									&frameRateReturns, sizeof(frameRateReturns), &cbReturned)))
	{
		double dblMaxForward = (((double) frameRateReturns.lMaxForwardFullFrame / (double)g_uRateScaleFactor));
		if (dblMaxForward < m_dblMaxFrameRateForward)
			m_dblMaxFrameRateForward = dblMaxForward;
		double dblMaxBackward = (((double) frameRateReturns.lMaxReverseFullFrame / (double)g_uRateScaleFactor));
		if (dblMaxBackward < (double)m_dblMaxFrameRateBackward)
			m_dblMaxFrameRateBackward = dblMaxBackward;
	}
#endif _WIN32_WCE

#ifdef CHATTER_ABOUT_GRAPH_SETUP
	{
		// Chatter about the setup of the graph:

		CBaseFilter &rcBaseFilter = m_pippmgr->GetFilter();

		// Iterate over the filter graph looking for IAMStreamSelect on a filter
		// that has 1+ audio input pins.

		IFilterGraph *pFilterGraph = rcBaseFilter.GetFilterGraph();
		if (!pFilterGraph)
		{
			OutputDebugStringW(L"CMPEGDecoderDriver::QueryGraphForCapabilities() -- no filter graph, giving up\n");
			return;
		}

		IEnumFilters *pFilterIter;

		if (FAILED(pFilterGraph->EnumFilters(&pFilterIter)))
		{
			OutputDebugStringW(L"CMPEGDecoderDriver::QueryGraphForCapabilities() -- no filter graph, giving up\n");
			return;
		}

		CComPtr<IEnumFilters> cComPtrIEnumFilters = NULL;
		cComPtrIEnumFilters.Attach(pFilterIter);

		IBaseFilter *pFilter;
		ULONG iFiltersFound;
		wchar_t pwszMsg[1024];

		OutputDebugStringW(L"\n#### FILTERS AND THEIR CONNECTED PINS ####\n\n");
		while (SUCCEEDED(pFilterIter->Next(1, &pFilter, &iFiltersFound)) &&
			(iFiltersFound > 0))
		{
			CComPtr<IBaseFilter> cComPtrIBaseFilter = NULL;
			cComPtrIBaseFilter.Attach(pFilter);

			FILTER_INFO sFilterInfo;
			if (SUCCEEDED(pFilter->QueryFilterInfo(&sFilterInfo)))
			{
				swprintf(pwszMsg, L"FILTER:  %s @ %p\n", sFilterInfo.achName, pFilter);
				OutputDebugStringW(pwszMsg);
				if (sFilterInfo.pGraph)
					sFilterInfo.pGraph->Release();
			}
			else
			{
				swprintf(pwszMsg, L"FILTER:  @ %p\n", pFilter);
				OutputDebugStringW(pwszMsg);
			}

			IEnumPins *piEnumPins;
			HRESULT hr = pFilter->EnumPins(&piEnumPins);
			if (SUCCEEDED(hr))
			{
				CComPtr<IEnumPins> piEnumPinsHolder = piEnumPins;
				piEnumPins->Release();

				ULONG uPinCount;
				IPin *piPin;

				while ((S_OK == piEnumPins->Next(1, &piPin, &uPinCount)) && (uPinCount != 0))
				{
					CComPtr<IPin> piPinHolder = piPin;
					piPin->Release();

					PIN_INFO sPinInfo;
					if (SUCCEEDED(piPin->QueryPinInfo(&sPinInfo)))
					{
						if (sPinInfo.pFilter)
							sPinInfo.pFilter->Release();
						if (sPinInfo.dir != PINDIR_OUTPUT)
							continue;
					}
					else
						sPinInfo.achName[0] = 0;
					IPin *pConnectedPin = NULL;
					if (SUCCEEDED(piPin->ConnectedTo(&pConnectedPin)) && pConnectedPin)
					{
						PIN_INFO sConnectedPinInfo;
						if (SUCCEEDED(pConnectedPin->QueryPinInfo(&sConnectedPinInfo)))
						{
							swprintf(pwszMsg, L"  Connects pin %s @ %p to pin %s @ %p of filter @ %p\n",
								sPinInfo.achName, piPin,
								sConnectedPinInfo.achName, pConnectedPin,
								sConnectedPinInfo.pFilter);
							OutputDebugStringW(pwszMsg);

							if (sConnectedPinInfo.pFilter)
								sConnectedPinInfo.pFilter->Release();
						}
						pConnectedPin->Release();
					}
					else
					{
						swprintf(pwszMsg, L"  Connects pin %s @ %p to pin %p of unknown filter\n",
							sPinInfo.achName, piPin, pConnectedPin);
						OutputDebugStringW(pwszMsg);
					}
				}
			}
		}
	}
#endif // CHATTER_ABOUT_GRAPH_SETUP
} // CMPEGDecoderDriver::QueryGraphForCapabilities

bool CMPEGDecoderDriver::IsSampleWanted(CDecoderDriverQueueItem &rcDecoderDriverQueueItem)
{
	bool fWanted = false;

	if (!CDecoderDriver::IsSampleWanted(rcDecoderDriverQueueItem))
	    goto returnIsWanted;
	if (m_fDiscardingToRateChange)
	{
		if (rcDecoderDriverQueueItem.m_piMediaSample->IsSyncPoint() != S_OK)
		{
			DbgLog((LOG_SOURCE_STATE, 3, _T("CMPEGDecoderDriver::IsSampleWanted():  discarding non-sync sample leading up to rate change\n") ));
            goto returnIsWanted;
		}
		rcDecoderDriverQueueItem.m_piMediaSample->SetDiscontinuity(TRUE);
		m_fDiscardingToRateChange = false;
		m_fFirstSampleAtNewRate = true;
		m_fDiscardingToSyncPointForDiscontinuity = false;

		REFERENCE_TIME  rtStart = 0;
		AM_SimpleRateChange prop;

		prop.StartTime = rtStart;
		double dNewRate = 10000 / m_dblRate;
		prop.Rate = LONG(dNewRate);

		if (dNewRate >= LONG_MAX)
		{
			// take care of positive overflow
			prop.Rate = LONG_MAX;
		}
		else if (dNewRate <= LONG_MIN)
		{
			// take care of negative overflow
			prop.Rate = LONG_MIN;
		}
		else if (prop.Rate == 0)
		{
			// take care of underflow (i.e. setting to zero)
			if (dNewRate >= 0)
			{
				prop.Rate = 1;
			}
			else
			{
				prop.Rate = -1;
			}
		}
		else
		{
			// common (normal) case
		}
		DbgLog((LOG_SOURCE_STATE, 3, _T("CMPEGDecoderDriver::IsSampleWanted() -- setting rate property to %d\n"), prop.Rate));
		DoRateChangeNotification(prop);
	}
	else if (m_fDiscardingToSyncPointForDiscontinuity)
	{
		if (rcDecoderDriverQueueItem.m_piMediaSample->IsSyncPoint() != S_OK)
		{
			DbgLog((LOG_SOURCE_STATE, 3, _T("CMPEGDecoderDriver::IsSampleWanted():  discarding non-sync sample after discontinuity\n") ));
            goto returnIsWanted;
		}
		DbgLog((LOG_SOURCE_STATE, 3, _T("CMPEGDecoderDriver::IsSampleWanted():  found sync-point after discontinuity\n") ));
		m_fDiscardingToSyncPointForDiscontinuity = false;
		rcDecoderDriverQueueItem.m_piMediaSample->SetDiscontinuity(TRUE);
	}
	if (SKIP_MODE_NORMAL != m_eFrameSkipModeEffective)
	{
		if (rcDecoderDriverQueueItem.m_piMediaSample->IsSyncPoint() != S_OK)
		{
			// Huh?  Either the Reader or the sample consumer goofed -- get rid of this
			// non-sync-point sample:
			DbgLog((LOG_SOURCE_STATE, 1, _T("CMPEGDecoderDriver::IsSampleWanted():  found non-sync while in key-frame-only rate, will discard\n") ));
            goto returnIsWanted;
		}
		rcDecoderDriverQueueItem.m_piMediaSample->SetDiscontinuity(TRUE);
	}
	fWanted = true;

returnIsWanted:

	if (rcDecoderDriverQueueItem.m_piMediaSample->IsDiscontinuity() == S_OK)
	{
		if (!fWanted)
		{
			if (!m_fDiscardingToSyncPointForDiscontinuity)
			{
				DbgLog((LOG_SOURCE_STATE, 3, _T("CMPEGDecoderDriver::IsSampleWanted():  found to-be-discarded discontinuity, will discard until sync point\n") ));
				m_fDiscardingToSyncPointForDiscontinuity = true;
			}
		}
		else if (rcDecoderDriverQueueItem.m_piMediaSample->IsSyncPoint() != S_OK)
		{
			DbgLog((LOG_SOURCE_STATE, 3, _T("CMPEGDecoderDriver::IsSampleWanted():  found non-sync discontinuity, will discard\n") ));
			m_fDiscardingToSyncPointForDiscontinuity = true;
			fWanted = false;
		}
	}

#ifdef CHATTER_ABOUT_POST_FLUSH_ACTIONS
	if (m_dwKeyFramesUntilChatterStops)
	{
		TCHAR pwszMsg[128];

		_stprintf(pwszMsg, TEXT("MPEG Decoder Driver:  %s %s%ssample at a/v position %I64d\n"),
			fWanted ? TEXT("considering delivery of") : TEXT("discarded"),
			(rcDecoderDriverQueueItem.m_piMediaSample->IsDiscontinuity() == S_OK) ? TEXT("discontinuity ") : TEXT(""),
			(rcDecoderDriverQueueItem.m_piMediaSample->IsSyncPoint () == S_OK) ? TEXT("sync-point ") : TEXT(""),
			rcDecoderDriverQueueItem.m_hyNominalStartPosition);
		OutputDebugString(pwszMsg);
	}
#endif // CHATTER_ABOUT_POST_FLUSH_ACTIONS
	return fWanted;
} // CMPEGDecoderDriver::IsSampleWanted

bool CMPEGDecoderDriver::ThrottleDeliveryWhenPaused()
{
	return false;
} // CMPEGDecoderDriver::ThrottleDeliveryWhenPaused

#ifdef CHATTER_ABOUT_POST_FLUSH_ACTIONS

void CMPEGDecoderDriver::ImplementEndFlush()
{
	m_dwKeyFramesUntilChatterStops = 2;
	CDecoderDriver::ImplementEndFlush();
}

#endif // CHATTER_ABOUT_POST_FLUSH_ACTIONS

ROUTE CMPEGDecoderDriver::NotifyFilterStateChange(FILTER_STATE eFilterState)
{
	// Do not hold the lock while escalating because doing so has the
	// potential for deadlock.

	double origRate = m_dblRate;
	ROUTE eRoute = CDecoderDriver::NotifyFilterStateChange(eFilterState);

#ifdef CHATTER_ABOUT_POST_FLUSH_ACTIONS
	m_dwKeyFramesUntilChatterStops = 2;
#endif // CHATTER_ABOUT_POST_FLUSH_ACTIONS

	CAutoLock cAutoLock(&m_cCritSec);

	if ((eFilterState == State_Stopped) && (origRate != m_dblRate) && (m_dblRate == 1.0))
	{
		AM_SimpleRateChange prop;

		prop.StartTime = 0;
		prop.Rate = 10000;  // We need to go to rate 1.0, which converteed to 10000/rate means 10000

		DbgLog((LOG_SOURCE_STATE, 3, _T("CMPEGDecoderDriver::NotifyFilterStateChange() -- setting rate property to %d\n"), prop.Rate));
		DoRateChangeNotification(prop);
	}

	return eRoute;
} // CMPEGDecoderDriver::NotifyFilterStateChange

void CMPEGDecoderDriver::DoRateChangeNotification(AM_SimpleRateChange &sAMSimpleRateChange)
{
	// We need to use IKsPropertySet::Set() to notify all interested input pins and filters
	// about the rate change.

	// The DVRNav filter should be notified first so that it can take care of
	// pushing remaining samples downstream first and doing the mute / unmute:

	if (m_piPrimaryPinPropertySet)
		NotifyInterfaceOfRate(m_piPrimaryPinPropertySet, sAMSimpleRateChange);

	std::list<IKsPropertySet*>::iterator iter;

	for (iter = m_lRateChangePropertySetListeners.begin();
		iter != m_lRateChangePropertySetListeners.end();
		++iter)
	{
		NotifyInterfaceOfRate(*iter, sAMSimpleRateChange);
	}
} // CMPEGDecoderDriver::DoRateChangeNotification

void CMPEGDecoderDriver::NotifyInterfaceOfRate(IKsPropertySet *piKsPropertySet, AM_SimpleRateChange &sAMSimpleRateChange)
{
	HRESULT hr = piKsPropertySet->Set(AM_KSPROPSETID_TSRateChange,
								AM_RATE_SimpleRateChange,
								NULL,
								0,
								(BYTE *)&sAMSimpleRateChange,
								sizeof(AM_SimpleRateChange));
	if (FAILED(hr))
	{
#ifdef DEBUG
		DWORD dwSupportType = 0;

		if ((S_OK != piKsPropertySet->QuerySupported(AM_KSPROPSETID_TSRateChange,
					AM_RATE_SimpleRateChange, &dwSupportType)) ||
			!(dwSupportType & KSPROPERTY_SUPPORT_SET))
			return;		// don't chatter about failure to do change rate notification on something that doesn't care
#endif
		DbgLog((LOG_ERROR, 2, _T("CMPEGDecoderDriver::NotifyInterfaceOfRate() -- error %d setting rate property to %d on i/f @ %p\n"),
					hr, sAMSimpleRateChange.Rate, piKsPropertySet));
	}
	else
	{
		DbgLog((LOG_SOURCE_STATE, 3, _T("CMPEGDecoderDriver::NotifyInterfaceOfRate() -- done setting rate property to %d on i/f @ %p\n"),
			sAMSimpleRateChange.Rate, piKsPropertySet ));
	}
} // CMPEGDecoderDriver::NotifyInterfaceOfRate

void CMPEGDecoderDriver::CacheRateChangePropertySetListeners()
{
	CComPtr<IEnumFilters> pFilterIter;
	if (FAILED(m_piFilterGraph->EnumFilters(&pFilterIter)))
	{
		DbgLog((LOG_SOURCE_STATE, 3, _T("CMPEGDecoderDriver::CacheRateChangePropertySetListeners() -- unable to enumerate filters, giving up\n"), this));
		return;
	}

	CComPtr<IBaseFilter> pFilter;
	ULONG iFiltersFound;

	for ( ; SUCCEEDED(pFilterIter->Next(1, &pFilter.p, &iFiltersFound)) &&
			(iFiltersFound > 0); pFilter.Release())
	{
		RememberIfSupportsRateChangeNotification(pFilter);

		CComPtr<IEnumPins> piEnumPins;
		if (SUCCEEDED(pFilter->EnumPins(&piEnumPins)) && piEnumPins)
		{
			CComPtr<IPin> piPin;
			ULONG cPinCount;

			for ( ;
				 SUCCEEDED(piEnumPins->Next(1, &piPin.p, &cPinCount)) && (cPinCount > 0);
				 piPin.Release())
			{
				// We've found a pin. We're only interested in it if it is an input pin
				// that is connected to something:

				PIN_INFO sPinInfo;

				if (FAILED(piPin->QueryPinInfo(&sPinInfo)))
					continue;
				if (sPinInfo.pFilter)
					sPinInfo.pFilter->Release();
				if (sPinInfo.dir == PINDIR_INPUT)
					RememberIfSupportsRateChangeNotification(piPin);
			}
		}
	}
} // CMPEGDecoderDriver::CacheRateChangePropertySetListeners()

void CMPEGDecoderDriver::RememberIfSupportsRateChangeNotification(IUnknown *pUnknown)
{
	CComPtr<IKsPropertySet> piKsPropertySet;

	if (FAILED(pUnknown->QueryInterface(IID_IKsPropertySet, (void **) &piKsPropertySet.p)))
		return;
	if (piKsPropertySet.p == m_piPrimaryPinPropertySet.p)
		return;  // don't want duplicate notification

	DWORD dwSupportType = 0;

	if ((S_OK != piKsPropertySet->QuerySupported(AM_KSPROPSETID_TSRateChange,
					AM_RATE_SimpleRateChange, &dwSupportType)) ||
		!(dwSupportType & KSPROPERTY_SUPPORT_SET))
		return;

	m_lRateChangePropertySetListeners.push_back(piKsPropertySet);

	// Assuming we didn't throw an exception out of push_back, we want to leave the ref
	// to piKsPropertySet.p in the hands of the list:
	piKsPropertySet.Detach();
} // CMPEGDecoderDriver::RememberIfSupportsRateChangeNotification

REFERENCE_TIME CMPEGDecoderDriver::GetFinalSamplePosition()
{
	return m_fSawSample ? m_rtLatestRecdStreamEndTime : -1;
} // CMPEGDecoderDriver::GetFinalSamplePosition

DWORD CMPEGDecoderDriver::ComputeTimeUntilFinalSample(REFERENCE_TIME rtAVPosition,
													  REFERENCE_TIME rtDispatchStreamTime,
													  REFERENCE_TIME &rtAVPositionLastKnownWhileRunning,
													  REFERENCE_TIME &rtLastPollTime)
{
	LONGLONG hyRenderingPosition = EstimatePlaybackPosition(true);
	REFERENCE_TIME rtNow = m_cClockState.GetStreamTime();

	DbgLog((LOG_DECODER_DRIVER, 1, _T("CMPEGDecoderDriver::ComputeTimeUntilFinalSample(%I64d, %I64d) @ now %I64d, position %I64d\n"),
			rtAVPosition, rtDispatchStreamTime, rtNow, hyRenderingPosition ));

	if  ((rtAVPosition == -1) || (hyRenderingPosition < 0) || (m_dblRate == 0.0))
	{
		// Either we don't know what the a/v position of the last sample is or we haven't a clue
		// what we're rendering now.  It could be that the EOF was issued at an awkward time
		// (e.g., recently after a flush, so the first post-flush a/v hadn't hit the overlay
		// mixer) -- or the current time could be awkward for the same reason:

		if ((m_dblRate != 0.0) && (m_eFilterState == State_Running))
		{
			if (rtNow - rtLastPollTime > 1000LL * 10000LL)
			{
				DbgLog((LOG_DECODER_DRIVER, 1, _T("CMPEGDecoderDriver::ComputeTimeUntilFinalSample(%I64d, %I64d) returning 0 due to position and/or rate\n"),
					rtAVPosition, rtDispatchStreamTime ));
				return 0;  // stuck
			}
		}
		else
		{
			DbgLog((LOG_DECODER_DRIVER, 1, _T("CMPEGDecoderDriver::ComputeTimeUntilFinalSample(%I64d, %I64d) not running, keep on sleeping\n"),
				rtAVPosition, rtDispatchStreamTime ));

			rtLastPollTime = rtNow;
		}
		// Dunno how long to wait (insufficient data), so just wait a little while:

		return 500;
	}

	if (m_eFilterState == State_Running)
	{
		if (rtAVPositionLastKnownWhileRunning == hyRenderingPosition)
		{
			// We haven't made progress.  Did we expect to?  Heuristic:  at least 4 frames/sec should
			// render so we should see progress in 1/4 second.
			//
			// Grumble:  I've seen successive samples given to AOvMixer Receive() with time-stamps 730 ms apart.
			// Increasing to 1000 ms.

			if (rtNow - rtLastPollTime > 1000LL * 10000LL)
			{
				DbgLog((LOG_DECODER_DRIVER, 1, _T("CMPEGDecoderDriver::ComputeTimeUntilFinalSample(%I64d, %I64d) returning 0 because no progress\n"),
					rtAVPosition, rtDispatchStreamTime ));
				return 0;  // stuck
			}
		}
		else
			rtLastPollTime = rtNow;
		rtAVPositionLastKnownWhileRunning = hyRenderingPosition;
	}
	else
	{
		rtLastPollTime = rtNow;
	}
	REFERENCE_TIME rtDShowUnitsUntilShouldRender = (rtAVPosition - hyRenderingPosition) / m_dblRate;
	if (rtDShowUnitsUntilShouldRender <= 0)
	{
		DbgLog((LOG_DECODER_DRIVER, 1, _T("CMPEGDecoderDriver::ComputeTimeUntilFinalSample(%I64d, %I64d) returning 0 being at/beyond rendering time\n"),
			rtAVPosition, rtDispatchStreamTime ));
		return 0;
	}
	return (rtDShowUnitsUntilShouldRender / 10000LL);
} // CMPEGDecoderDriver::ComputeTimeUntilFinalSample

void CMPEGDecoderDriver::PrePositionChange()
{
	if (m_dblRate != 1.0)
	{
		REFERENCE_TIME  rtStart = 0;
		AM_SimpleRateChange prop;

		prop.StartTime = rtStart;
		prop.Rate = 10000;  // We need to temporarily go to rate 1.0, which converteed to 10000/rate means 10000

		DbgLog((LOG_SOURCE_STATE, 3, _T("CMPEGDecoderDriver::PrePositionChange() -- setting rate property to %d\n"), prop.Rate));
		DoRateChangeNotification(prop);
	}
} // CMPEGDecoderDriver::PrePositionChange

void CMPEGDecoderDriver::PostPositionChange()
{
	if (m_dblRate != 1.0)
	{
		REFERENCE_TIME  rtStart = 0;
		AM_SimpleRateChange prop;

		prop.StartTime = rtStart;
		double dNewRate = 10000 / m_dblRate;
		prop.Rate = LONG(dNewRate);

		if (dNewRate >= LONG_MAX)
		{
			// take care of positive overflow
			prop.Rate = LONG_MAX;
		}
		else if (dNewRate <= LONG_MIN)
		{
			// take care of negative overflow
			prop.Rate = LONG_MIN;
		}
		else if (prop.Rate == 0)
		{
			// take care of underflow (i.e. setting to zero)
			if (dNewRate >= 0)
			{
				prop.Rate = 1;
			}
			else
			{
				prop.Rate = -1;
			}
		}
		else
		{
			// common (normal) case
		}
		DbgLog((LOG_SOURCE_STATE, 3, _T("CMPEGDecoderDriver::PostPositionChange() -- setting rate property to %d\n"), prop.Rate));
		DoRateChangeNotification(prop);
	}
} // CMPEGDecoderDriver::PostPositionChange

