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

#include "../SampleProducer/CClockState.h"
#include "../SampleProducer/ProducerConsumerUtilities.h"

namespace MSDvr
{
	class CDecoderDriverAppThread;
	class CDecoderDriverStreamThread;
	class CPinMappings;
	class IMediaTypeAnalyzer;

	class CDecoderDriverQueueItem
	{
	public:
		enum QUEUE_ITEM_TYPE
		{
			DECODER_DRIVER_ITEM_SAMPLE,
			DECODER_DRIVER_ITEM_END_OF_STREAM,
			DECODER_DRIVER_ITEM_EXTENSION
		};

		QUEUE_ITEM_TYPE m_eQueueItemType;
		CComPtr<IMediaSample> m_piMediaSample;
		CSmartRefPtr<CExtendedRequest> m_pcExtendedRequest;
		union {
			struct {
				CDVROutputPin *m_pcDVROutputPin;
				LONGLONG m_hyEarliestAudioPosition, m_hyEarliestVideoPosition,
					m_hyNominalStartPosition, m_hyNominalEndPosition,
					m_hyLatestAudioPosition, m_hyLatestVideoPosition;
			};
		};
	};

	class CDecoderDriver :
		public IPlaybackPipelineComponent,
		public IDecoderDriver
	{
	public:
		enum KEY_FRAME_STATUS
		{
			KEY_FRAME_NEEDED,
			PARTIAL_KEY_FRAME,
			COMPLETE_KEY_FRAME
		};

		CDecoderDriver();
		~CDecoderDriver();

		/* IPipelineComponent: */
		virtual void RemoveFromPipeline();
		virtual ROUTE GetPrivateInterface(REFIID riid, void *&rpInterface);
		virtual ROUTE NotifyFilterStateChange(FILTER_STATE eFilterState);
		virtual ROUTE ConfigurePipeline(
						UCHAR			iNumPins,
						CMediaType		cMediaTypes[],
						UINT			iSizeCustom,
						BYTE			Custom[]);
		virtual ROUTE DispatchExtension(
						// @parm The extension to be dispatched.
						CExtendedRequest &rcExtendedRequest);

		/* IPlaybackPipelineComponent: */
		virtual unsigned char AddToPipeline(IPlaybackPipelineManager &rIPlaybackPipelineManager);
		virtual ROUTE ProcessOutputSample(
			IMediaSample &riMediaSample,
			CDVROutputPin &rcDVROutputPin);
		virtual ROUTE	EndOfStream();

		/* IDecoderDriver: */
		virtual HRESULT GetPreroll(LONGLONG &rhyPreroll);
		virtual bool IsRateSupported(double dblRate);
		virtual void ImplementNewRate(IPipelineComponent *piPipelineComponentOrigin, double dblRate, BOOL fFlushInProgress);
		virtual void SetNewRate(double dblRate);
		virtual void ImplementNewPosition(IPipelineComponent *piPipelineComponentOrigin, LONGLONG hyPosition,
			bool fNoFlushRequested, bool fSkippingTimeHole,
			bool fSeekToKeyFrame, bool fOnAppThread);
		virtual void ImplementEndPlayback(PLAYBACK_END_MODE ePlaybackEndMode, IPipelineComponent *piRequester);
		virtual void ImplementTuneEnd(bool fTuneIsLive,
			LONGLONG hyChannelStartPos, bool fCalledFromAppThread);
		virtual bool ImplementBeginFlush();
		virtual void ImplementEndFlush();
		virtual void ImplementRun(bool fOnAppThread);
		virtual void ImplementGraphConfused(HRESULT hr);
		virtual void ImplementDisabledInputs(bool fSampleComingFirst);
		virtual void ImplementLoad();
		virtual bool IsAtEOS();
		virtual HRESULT IsSeekRecommendedForRate(double dblRate, LONGLONG &hyRecommendedPosition);
		virtual void IssueNotification(IPipelineComponent *piPipelineComponent,
			long lEventID, long lParam1 = 0, long lParam2 = 0,
			bool fDeliverNow = false, bool fDeliverOnlyIfStreaming = false);
		virtual void DeferToAppThread(CAppThreadAction *pcAppThreadAction);
		virtual IPipelineComponent *GetPipelineComponent();
		virtual void SetEndOfStreamPending();
		virtual void ImplementThrottling(bool fThrottle);
		virtual void RegisterIdleAction(IPipelineComponentIdleAction *piPipelineComponentIdleAction);
		virtual void UnregisterIdleAction(IPipelineComponentIdleAction *piPipelineComponentIdleAction);
		virtual BOOL IsFlushNeededForRateChange() { return FALSE; }
		virtual void SetBackgroundPriorityMode(BOOL fUseBackgroundPriority);

		static const double s_dblFrameToKeyFrameRatio;
		static const double s_dblSecondsToKeyFrameRatio;
		static const REFERENCE_TIME s_rtMaxEarlyDeliveryMargin;
		static const REFERENCE_TIME s_rtMaxLateDeliveryDrift;
		static const REFERENCE_TIME s_rtMaxEarlyDeliveryDrift;

	protected:
		virtual void Cleanup();
		virtual void RefreshDecoderCapabilities();
		virtual void InitPinMappings();
		virtual void UpdateXInterceptAsNeeded(const CDecoderDriverQueueItem &rcDecoderDriverQueueItem);
		virtual void EnactRateChange(double dblRate);
		virtual HRESULT SetPresentationTime(IMediaSample &riMediaSample,
			REFERENCE_TIME &rtStartTime, REFERENCE_TIME &rtEndTime);
		void GetGraphInterfaces();

		IReader *GetReader();
		ISampleConsumer *GetSampleConsumer();
		IMediaEventSink *GetMediaEventSink();  // caller must release the ref if returned value is non-null
		void StopAppThread();
		void StartAppThread();
		void StopStreamThread();
		void StartStreamThread();
		void AllPinsEndOfStream();
		void SetEnableAudio(bool fEnable);
		void StartNewSegment();
		void EndCurrentSegment();
		void FlushSamples();
		void SendNotification(long lEventID, long lParam1, long lParam2);
		void IdleAction();
		bool SkipModeChanged(FRAME_SKIP_MODE eFrameSkipMode1, FRAME_SKIP_MODE eFrameSkipMode2,
							 DWORD dwFrameSkipModeSeconds1, DWORD dwFrameSkipModeSeconds2);

		virtual void HandleSetGraphRate(double dblRate);
		virtual void HandleGraphSetPosition(IPipelineComponent *piPipelineComponentOrigin,
			LONGLONG hyPosition,
							bool fNoFlushRequested, bool fSkippingTimeHole,
							bool fSeekToKeyFrame);
		virtual void HandleGraphTune(LONGLONG hyChannelStartPos);
		virtual void HandleGraphRun();
		virtual void HandleGraphError(HRESULT hr);
		virtual bool BeginFlush();  // returns true iff there is need to call EndFlush()
		virtual void EndFlush();
		virtual void NoteDiscontinuity();
		virtual bool ReadyForSample(const CDecoderDriverQueueItem &rcDecoderDriverQueueItem);
		virtual bool IsSampleWanted(CDecoderDriverQueueItem &rcDecoderDriverQueueItem);
		virtual void ExtractPositions(IMediaSample &riMediaSample,
				CDVROutputPin &rcDVROutputPin,
				LONGLONG &hyEarliestAudioPosition,
				LONGLONG &hyEarliestVideoPosition,
				LONGLONG &hyNominalStartPosition,
				LONGLONG &hyNominalEndPosition,
				LONGLONG &hyLatestAudioPosition,
				LONGLONG &hyLatestVideoPosition);
		virtual void ResetLatestTimes();
		virtual ROUTE ProcessQueuedSample(CDecoderDriverQueueItem &rcDecoderDriverQueueItem);
		virtual ROUTE DoDispatchExtension(CExtendedRequest &rcExtendedRequest);
		virtual bool ThrottleDeliveryWhenRunning() { return m_fThrottling; }
		virtual LONGLONG EstimatePlaybackPosition(bool fExemptEndOfStream = false, bool fAdviseIfEndOfMedia = false);
		virtual void QueryGraphForCapabilities();
		virtual void ComputeModeForRate(double dblRate, FRAME_SKIP_MODE &eFrameSkipMode, DWORD &dwFrameSkipModeSeconds);
		virtual bool ThrottleDeliveryWhenPaused() { return true; }
		virtual REFERENCE_TIME GetFinalSamplePosition();
		virtual DWORD ComputeTimeUntilFinalSample(
			REFERENCE_TIME rtAVPosition,
			REFERENCE_TIME rtDispatchStreamTime,
			REFERENCE_TIME &rtAVPositionLastKnownWhileRunning,
			REFERENCE_TIME &rtLastPollTime);
		virtual void PrePositionChange() {}
		virtual void PostPositionChange() {}

		friend class CDecoderDriverTestHooks;
		friend class CDecoderDriverAppThread;
		friend class CEmulatedSourcePipelineTestHooks;

		CPinMappings *m_pcPinMappings;
		FILTER_STATE m_eFilterState;
		FILTER_STATE m_eFilterStatePrior;
		IPlaybackPipelineManager *m_pippmgr;
		GUID m_guidCurTimeFormat;
		double m_dblRate, m_dblAbsoluteRate;
		double m_dblRatePrior, m_dblAbsoluteRatePrior;
		IReader *m_piReader;
		ISampleConsumer *m_piSampleConsumer;
		LONGLONG m_hyLatestMediaTime;
		REFERENCE_TIME m_rtXInterceptTime;
		REFERENCE_TIME m_rtXInterceptTimePrior;
		REFERENCE_TIME m_rtLatestRecdStreamEndTime;
		REFERENCE_TIME m_rtLatestSentStreamEndTime;
		REFERENCE_TIME m_rtLatestRateChange;
		REFERENCE_TIME m_rtSegmentStartTime;
		REFERENCE_TIME m_rtLatestKeyDownstream;
		double m_dblMaxFrameRateForward;
		double m_dblMaxFrameRateBackward;
		double m_dblFrameToKeyFrameRatio;
		double m_dblSecondsToKeyFrameRatio;
		AM_MaxFullDataRate m_sAMMaxFullDataRate;
		FRAME_SKIP_MODE m_eFrameSkipMode;
		DWORD m_dwFrameSkipModeSeconds;
		FRAME_SKIP_MODE m_eFrameSkipModeEffective;
		DWORD m_dwFrameSkipModeSecondsEffective;
		LONGLONG m_hyPreroll;
		bool m_fKnowDecoderCapabilities;
		bool m_fSegmentStartNeeded;
		bool m_fSawSample;
		bool m_fFlushing;
		DWORD m_dwSegmentNumber;
		CDecoderDriverAppThread *m_pcDecoderDriverAppThread;
		CDecoderDriverStreamThread *m_pcDecoderDriverStreamThread;
		CDVRSourceFilter *m_pcBaseFilter;
		CComPtr<IFilterGraph> m_piFilterGraph;
		CComPtr<IMediaControl> m_piMediaControl;
		CComPtr<IMediaEventSink> m_piMediaEventSink;
		CCritSec m_cCritSec;
		bool m_fAtEndOfStream;
		bool m_fPositionDiscontinuity;
		bool m_fThrottling;
		CCritSec *m_pcCritSecApp;
		IMediaTypeAnalyzer *m_piMediaTypeAnalyzer;
		CClockState m_cClockState;
		std::list<CDecoderDriverQueueItem> m_listCDecoderDriverQueueItem;
		LONGLONG m_hyLatestVideoTime;
		LONGLONG m_hyLatestAudioTime;
		LONGLONG m_hyMinimumLegalTime;
		KEY_FRAME_STATUS m_fSentKeyFrameSinceStop;
		bool m_fSentAudioSinceStop;
		bool m_fSentVideoSinceStop;
		bool m_fPendingRateChange;
		bool m_fEndOfStreamPending;
		DWORD m_dwEcCompleteCount;
		unsigned m_uPendingDiscontinuities;
		// It is such a nasty thing to deal with locking of the list of callbacks versus
		// invocation of them. The decoder driver isn't allowed to hold locks going
		// backward so I can't safeguard a fine std::list or std::vector from concurrent
		// actions easily. The "list" is going to be a fixed size vector:
		enum {
			MAX_IDLE_ACTIONS = 8
		};

		PIPPipelineComponentIdleAction m_listIPipelineComponentIdleAction[MAX_IDLE_ACTIONS];

		enum DECODER_DRIVER_MEDIA_END_STATUS {
			DECODER_DRIVER_NORMAL_PLAY,
			DECODER_DRIVER_END_OF_MEDIA_SENT,
			DECODER_DRIVER_START_OF_MEDIA_SENT
		};
		
		DECODER_DRIVER_MEDIA_END_STATUS m_eDecoderDriverMediaEndStatus;

		BOOL m_fEnableBackgroundPriority;

		static const LONGLONG s_hyMaxMediaTime;
		static const LONGLONG s_hyAllowanceForDownstreamProcessing;
		static const LONGLONG s_hyMinAllowance;
		static const LONGLONG s_hyMaximumWaitForRate;
		static const ULONG g_uRateScaleFactor;

		friend class CDecoderDriverStreamThread;
	};

}

