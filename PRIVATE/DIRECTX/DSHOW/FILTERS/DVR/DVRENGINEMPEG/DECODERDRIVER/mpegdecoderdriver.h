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

#include "CDecoderDriver.h"
#include "../SampleProducer/MPEGPinUtils.h"

// #define CHATTER_ABOUT_POST_FLUSH_ACTIONS

// Add a sanity check to suppress duplicate samples [which would be a consumer bug]:
#define SUPPRESS_DUPLICATE_SAMPLES

namespace MSDvr
{
	class CMPEGDecoderDriver : public CDecoderDriver
	{
	public:
		CMPEGDecoderDriver(void);
		~CMPEGDecoderDriver(void);

		// Pipeline:

		ROUTE	ConfigurePipeline(
						UCHAR			iNumPins,
						CMediaType		cMediaTypes[],
						UINT			iSizeCustom,
						BYTE			Custom[]);
		virtual ROUTE NotifyFilterStateChange(FILTER_STATE eFilterState);

		// IDecoderDriver:
		virtual HRESULT IsSeekRecommendedForRate(double dblRate, LONGLONG &hyRecommendedPosition);
		virtual BOOL IsFlushNeededForRateChange() { return TRUE; }

	protected:
		virtual void Cleanup();
		virtual void RefreshDecoderCapabilities();
		virtual void ExtractPositions(IMediaSample &riMediaSample, 
				CDVROutputPin &rcDVROutputPin, 
				LONGLONG &hyEarliestAudioPosition, 
				LONGLONG &hyEarliestVideoPosition, 
				LONGLONG &hyNominalStartPosition, 
				LONGLONG &hyNominalEndPosition,
				LONGLONG &hyLatestAudioPosition,
				LONGLONG &hyLatestVideoPosition);
		virtual void ResetLatestTimes();
		virtual bool IsSampleWanted(CDecoderDriverQueueItem &rcDecoderDriverQueueItem);
		virtual ROUTE ProcessQueuedSample(CDecoderDriverQueueItem &rcDecoderDriverQueueItem);
		virtual void EnactRateChange(double dblRate);
		virtual void InitPinMappings();
		virtual bool ThrottleDeliveryWhenRunning() { return false; }
		virtual void UpdateXInterceptAsNeeded(const CDecoderDriverQueueItem &rcDecoderDriverQueueItem);
		virtual HRESULT SetPresentationTime(IMediaSample &riMediaSample, 
			REFERENCE_TIME &rtStartTime, REFERENCE_TIME &rtEndTime);
		virtual LONGLONG EstimatePlaybackPosition(bool fExemptEndOfStream = false, bool fAdviseIfEndOfMedia = false);
		virtual void QueryGraphForCapabilities();
		virtual bool ThrottleDeliveryWhenPaused();
		virtual REFERENCE_TIME GetFinalSamplePosition();
		virtual DWORD ComputeTimeUntilFinalSample(REFERENCE_TIME rtAVPosition, 
			REFERENCE_TIME rtDispatchStreamTime, REFERENCE_TIME &rtAVPositionLastKnownWhileRunning,
			REFERENCE_TIME &rtLastPollTime);
		virtual void PrePositionChange();
		virtual void PostPositionChange();

		void DoRateChangeNotification(AM_SimpleRateChange &sAMSimpleRateChange);
		void NotifyInterfaceOfRate(IKsPropertySet *pIKsPropertySet, AM_SimpleRateChange &sAMSimpleRateChange);
		void CacheRateChangePropertySetListeners();
		void RememberIfSupportsRateChangeNotification(IUnknown *pUnknown);

		CMPEGOutputMediaTypeAnalyzer m_cMPEGMediaTypeAnalyzer;
		CComPtr<IKsPropertySet> m_piPrimaryPinPropertySet;
		std::list<IKsPropertySet*> m_lRateChangePropertySetListeners;
		bool m_fDiscardingToRateChange;
		bool m_fFirstSampleAtNewRate;
		bool m_fDiscardingToSyncPointForDiscontinuity;
		bool m_fDiscardingPostQueueUntilSyncPoint;
#ifdef SUPPRESS_DUPLICATE_SAMPLES
		LONGLONG m_hyLastPositionSentDownstream;
#endif // SUPPRESS_DUPLICATE_SAMPLES
#ifdef CHATTER_ABOUT_POST_FLUSH_ACTIONS
		DWORD m_dwKeyFramesUntilChatterStops;

		virtual void ImplementEndFlush();

#endif // CHATTER_ABOUT_POST_FLUSH_ACTIONS
	};

}
