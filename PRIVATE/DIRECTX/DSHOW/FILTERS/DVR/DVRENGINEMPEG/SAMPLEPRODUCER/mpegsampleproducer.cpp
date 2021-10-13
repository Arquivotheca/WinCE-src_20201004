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
#include "MPEGSampleProducer.h"
#include "..\\HResultError.h"

using namespace MSDvr;

#define UNDEFINED_SYNC_POINT_ARRIVAL	-1

///////////////////////////////////////////////////////////////////////
//
// Class CMPEGSampleProducer:   constructor and destructor
//
///////////////////////////////////////////////////////////////////////

CMPEGSampleProducer::CMPEGSampleProducer(void)
	: CSampleProducer()
	, m_hyLastSyncPointPosition(0)
	, m_rtLastSyncPointArrival(UNDEFINED_SYNC_POINT_ARRIVAL)
	, m_rtLastReportedTime(0)
{
	m_piMediaTypeAnalyzer = &m_cMPEGMediaTypeAnalyzer;
}  // CMPEGSampleProducer::CMPEGSampleProducer

///////////////////////////////////////////////////////////////////////
//
// Class CMPEGSampleProducer:   helper methods
//
///////////////////////////////////////////////////////////////////////

bool CMPEGSampleProducer::IsSampledWanted(UCHAR bPinIndex, IMediaSample &riMediaSample)
{
	if (!IsPrimaryPin(bPinIndex))
		return false;

	if (riMediaSample.IsSyncPoint() != S_OK)
	{
		if (UNDEFINED_SYNC_POINT_ARRIVAL == m_rtLastSyncPointArrival)
		{
			DbgLog((LOG_SINK_DISPATCH, 3, 
				_T("CMPEGSampleProducer::IsSampleWanted():  discarding non-sync-point ... need to init MediaTime <-> StreamTime mapping\n") ));
			return false;
		}
		return true;
	}

	LONGLONG hyStartTime, hyEndTime;
	HRESULT hr = riMediaSample.GetMediaTime(&hyStartTime, &hyEndTime);
	if (SUCCEEDED(hr) && (hyStartTime >= m_hyLastSyncPointPosition))
	{
		m_hyLastSyncPointPosition = hyStartTime;
		m_rtLastSyncPointArrival = m_cClockState.GetStreamTime();
	}
	return true;
} // CMPEGSampleProducer::IsSampledWanted

void CMPEGSampleProducer::Cleanup()
{
	CSampleProducer::Cleanup();
	m_hyLastSyncPointPosition = 0;
	m_rtLastSyncPointArrival = UNDEFINED_SYNC_POINT_ARRIVAL;
	m_hyLastSyncPointPosition = 0;
} // CMPEGSampleProducer::Cleanup

bool CMPEGSampleProducer::IsPostFlushSampleAfterTune(UCHAR bPinIndex, IMediaSample &riMediaSample) const
{
	return false;
} // CMPEGSampleProducer::IsPostFlushSampleAfterTune

bool CMPEGSampleProducer::IsNewSegmentSampleAfterTune(UCHAR bPinIndex, IMediaSample &riMediaSample) const
{
	return (bPinIndex == m_cSampleProducerSampleHistory.m_bPrimaryPinIndex);
} // CMPEGSampleProducer::IsNewSegmentSampleAfterTune

LONGLONG CMPEGSampleProducer::GetProducerTime() const
{
	if (UNDEFINED_SYNC_POINT_ARRIVAL == m_rtLastSyncPointArrival)
		return -1;
	LONGLONG hyTimeNow = m_hyLastSyncPointPosition + (m_cClockState.GetStreamTime() - m_rtLastSyncPointArrival);
	if (hyTimeNow < m_rtLastReportedTime)
		hyTimeNow = m_rtLastReportedTime;
	m_rtLastReportedTime = hyTimeNow;
	return hyTimeNow;
} // CMPEGSampleProducer::GetProducerTime
