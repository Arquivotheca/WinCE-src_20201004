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
#pragma once

#include "SampleProducer.h"
#include "MPEGPinUtils.h"

namespace MSDvr
{
	class CMPEGSampleProducer: public CSampleProducer 
	{
	public:
		CMPEGSampleProducer();

		virtual LONGLONG GetProducerTime() const;

	protected:
		virtual bool IsSampledWanted(UCHAR bPinIndex, IMediaSample &riMediaSample);
		virtual void Cleanup();
		virtual bool IsPostFlushSampleAfterTune(UCHAR bPinIndex, IMediaSample &riMediaSample) const;
		virtual bool IsNewSegmentSampleAfterTune(UCHAR bPinIndex, IMediaSample &riMediaSample) const;

		CMPEGInputMediaTypeAnalyzer m_cMPEGMediaTypeAnalyzer;
		LONGLONG m_hyLastSyncPointPosition;
		REFERENCE_TIME m_rtLastSyncPointArrival;
		mutable LONGLONG m_rtLastReportedTime; // this does change even for const objects in GetProducerTime()
	};
}

