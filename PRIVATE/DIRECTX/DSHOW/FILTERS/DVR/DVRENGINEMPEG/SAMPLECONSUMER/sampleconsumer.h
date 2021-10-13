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

#include "..\\PipelineInterfaces.h"
#include "DvrInterfaces_private.h"
#include "..\\SampleProducer\\ProducerConsumerUtilities.h"
#include "..\\SampleProducer\\CPauseBufferHistory.h"
#include "..\\Plumbing\\PipelineManagers.h"

#ifdef DEBUG
#define SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT
#endif

namespace MSDvr
{
	class CSampleConsumerTestHooks;
	class CPinMappings;
	class CConsumerNotifyOnPosition;

	namespace SampleProducer {
		class CPauseBufferSegment;
		class CPauseBufferHistory;
	}

	enum SAMPLE_CONSUMER_FILTER_STATE {
		SC_FILTER_NEVER_STARTED,
		SC_FILTER_STOPPED,
		SC_FILTER_FIRST_ACTIVATION,
		SC_FILTER_ACTIVATING,
		SC_FILTER_PAUSED,
		SC_FILTER_ABOUT_TO_RUN,
		SC_FILTER_RUNNING,
		SC_FILTER_PAUSING,
		SC_FILTER_DEACTIVATING
	};

	enum SAMPLE_CONSUMER_LOAD_STATE {
		SC_LOAD_NEEDED,
		SC_LOAD_IN_PROGRESS,
		SC_LOAD_COMPLETE
	};

	enum SAMPLE_CONSUMER_FLUSH_STATE {
		SC_FLUSH_NONE,
		SC_FLUSH_STARTING,
		SC_FLUSHING,
		SC_FLUSH_ENDING
	};

	enum SAMPLE_CONSUMER_SAMPLE_SOURCE {
		SC_SAMPLE_SOURCE_NONE,
		SC_SAMPLE_SOURCE_CONVERTING_TO_PRODUCER,
		SC_SAMPLE_SOURCE_PRODUCER,
		SC_SAMPLE_SOURCE_CONVERTING_TO_READER,
		SC_SAMPLE_SOURCE_READER
	};

	enum SAMPLE_CONSUMER_PIPELINE_STATE {
		SC_PIPELINE_NONE,
		SC_PIPELINE_ADDING,
		SC_PIPELINE_NORMAL,
		SC_PIPELINE_REMOVING
	};

	enum SAMPLE_CONSUMER_STOP_MODE {
		SC_STOP_MODE_NONE,
		SC_STOP_MODE_END_SEGMENT,
		SC_STOP_MODE_END_STREAM
	};

	enum SAMPLE_CONSUMER_POSITION_MODE {
		SC_POSITION_UNKNOWN,
		SC_POSITION_AWAITING_FIRST_SAMPLE,
		SC_POSITION_SEEKING_KEY,
		SC_POSITION_SEEKING_ANY,
		SC_POSITION_ALIGNING,
		SC_POSITION_NORMAL,
		SC_POSITION_AT_STOP_POSITION,
		SC_POSITION_PENDING_SET_POSITION,
		SC_POSITION_PENDING_RUN,
		SC_POSITION_PENDING_NORMAL_RATE
	};

	enum SAMPLE_CONSUMER_RECORDING_STATE {
		SC_RECORDING_VIEWING_NONE,
		SC_RECORDING_VIEWING_TEMP_IN_PROGRESS,
		SC_RECORDING_VIEWING_TEMP,
		SC_RECORDING_VIEWING_PERM_IN_PROGRESS,
		SC_RECORDING_VIEWING_PERM
	};

	enum SAMPLE_CONSUMER_INTERNAL_EVENT_TYPE {
		SC_INTERNAL_EVENT_LOAD,
		SC_INTERNAL_EVENT_ACTIVATE_SOURCE,
		SC_INTERNAL_EVENT_DEACTIVATE_SOURCE,
		SC_INTERNAL_EVENT_REQUESTED_SET_RATE_NORMAL,
		SC_INTERNAL_EVENT_REQUESTED_SET_RATE_RESUMING,
		SC_INTERNAL_EVENT_RUN_SOURCE,
		SC_INTERNAL_EVENT_SINK_STATE,
		SC_INTERNAL_EVENT_SINK_BIND,
		SC_INTERNAL_EVENT_SINK_UNBIND,
		SC_INTERNAL_EVENT_READER_SAMPLE,
		SC_INTERNAL_EVENT_PRODUCER_SAMPLE,
		SC_INTERNAL_EVENT_PAUSE_BUF,
		SC_INTERNAL_EVENT_LOST_PIPELINE,
		SC_INTERNAL_EVENT_PLAYED_TO_SOURCE,
		SC_INTERNAL_EVENT_DESTRUCTOR,
		SC_INTERNAL_EVENT_END_OF_STREAM,
		SC_INTERNAL_EVENT_GRAPH_ERROR_STOP,
		SC_INTERNAL_EVENT_CONSUMER_GRAPH_STOP,
		SC_INTERNAL_EVENT_POSITION_FLUSH,
		SC_INTERNAL_EVENT_AVOIDING_TRUNCATION,
		SC_INTERNAL_EVENT_FLUSH_DONE,
		SC_INTERNAL_EVENT_FAILED_TO_GET_PRODUCER_SAMPLE
	};

	class CSampleConsumer;

	class CConsumerNotifyOnPosition : public CDecoderDriverNotifyOnPosition
	{
	public:
		CConsumerNotifyOnPosition(CSampleConsumer *pcSampleConsumer, IPipelineComponent *piDecoderDriver,
									LONGLONG hyPosition, double dblPriorRate, unsigned uPositionCount,
									long lEventId, long lParam1, long lParam2)
			: CDecoderDriverNotifyOnPosition(piDecoderDriver, hyPosition)
			, m_pcSampleConsumer(pcSampleConsumer)
			, m_uPositionCount(uPositionCount)
			, m_dblPriorRate(dblPriorRate)
			, m_lEventId(lEventId)
			, m_lParam1(lParam1)
			, m_lParam2(lParam2)
		{
		}

		virtual void OnPosition();

		CSampleConsumer *m_pcSampleConsumer;
		unsigned m_uPositionCount;
		double m_dblPriorRate;
		long m_lEventId, m_lParam1, m_lParam2;
	};

	class CSampleConsumer:
		public IPlaybackPipelineComponent,
		public TRegisterableCOMInterface<IFileSourceFilter>,
//		public TRegisterableCOMInterface<IMediaSeeking>,
		public TRegisterableCOMInterface<IStreamBufferMediaSeeking>,
		public TRegisterableCOMInterface<IStreamBufferPlayback>,
		public ISampleConsumer,
		public CPauseBufferMgrImpl,
		public SampleProducer::ISampleSource,
		public IPipelineComponentIdleAction
	{
	public:
		CSampleConsumer(void);
		~CSampleConsumer(void);

		/* IPipelineComponent: */
		virtual void RemoveFromPipeline();
		virtual ROUTE GetPrivateInterface(REFIID riid, void *&rpInterface);
		virtual ROUTE NotifyFilterStateChange(FILTER_STATE eFilterState);
		virtual ROUTE ConfigurePipeline(
			UCHAR bNumPins,
			CMediaType cMediaTypes[],
			UINT uSizeCustom,
			BYTE bCustom[]);
		virtual ROUTE DispatchExtension(
						// @parm The extension to be dispatched.
						CExtendedRequest &rcExtendedRequest);

		/* IPlaybackPipelineComponent: */
		virtual unsigned char AddToPipeline(IPlaybackPipelineManager &rcManager);
		virtual ROUTE ProcessOutputSample(
			IMediaSample &riMediaSample,
			CDVROutputPin &rcDVROutputPin);
		virtual ROUTE	EndOfStream();

		/* ISampleConsumer: */
		virtual void ProcessProducerSample(
			ISampleProducer *piSampleProducer, LONGLONG hyMediaSampleID,
			ULONG uModeVersionCount,
			LONGLONG hyMediaStartPos, LONGLONG hyMediaEndPos,
			UCHAR bPinIndex);
		virtual void ProcessProducerTune(ISampleProducer *piSampleProducer,
			LONGLONG hyTuneSampleStartPos);
		virtual void SinkFlushNotify(ISampleProducer *piSampleProducer,
			UCHAR bPinIndex, bool fIsFlushing);
		virtual void NotifyBound(ISampleProducer *piSampleProducer);
		virtual void SinkStateChanged (ISampleProducer *piSampleProducer,
			PRODUCER_STATE eProducerState);
		virtual void PauseBufferUpdated(ISampleProducer *piSampleProducer,
			CPauseBufferData *pcPauseBufferData);
		virtual void NotifyProducerCacheDone(ISampleProducer *piSampleProducer,
			ULONG uModeVersionCount);
		virtual void SetPositionFlushComplete(LONGLONG hyPosition, bool fSeekToKeyFrame);
		virtual void NotifyCachePruned(ISampleProducer *piSampleProducer, LONGLONG hyFirstSampleIDRetained);
		virtual void SeekToTunePosition(LONGLONG hyChannelStartPos);
		virtual void NotifyRecordingWillStart(ISampleProducer *piSampleProducer,
			LONGLONG hyRecordingStartPosition, LONGLONG hyPriorRecordingEnd,
			const std::wstring pwszCompletedRecordingName);
		virtual LONGLONG GetMostRecentSampleTime() const;
		virtual DWORD GetLoadIncarnation() const;
		virtual void NotifyEvent(long lEvent, long lParam1, long lParam2);

		/* IPauseBufferMgr: */
		virtual CPauseBufferData* GetPauseBuffer();
		virtual void RegisterForNotifications(IPauseBufferCallback *pCallback);
		virtual void CancelNotifications(IPauseBufferCallback *pCallback);
		virtual HRESULT ExitRecordingIfOrphan(LPCOLESTR pszRecordingName);

		/* IPipelineComponentIdleAction */
		virtual void DoAction();

		/* IFileSourceFilter: */
		virtual HRESULT STDMETHODCALLTYPE Load(
			/* [in] */ LPCOLESTR pszFileName,
			/* [unique][in] */ const AM_MEDIA_TYPE *pmt);
		virtual HRESULT STDMETHODCALLTYPE GetCurFile(
			/* [out] */ LPOLESTR *ppszFileName,
			/* [out] */ AM_MEDIA_TYPE *pmt);

		/* IStreamBufferMediaSeeking: */
		virtual HRESULT STDMETHODCALLTYPE GetCapabilities(
			/* [out] */ DWORD *pdwCapabilities);
		virtual HRESULT STDMETHODCALLTYPE CheckCapabilities(
			/* [out][in] */ DWORD *pdwCapabilities);
		virtual HRESULT STDMETHODCALLTYPE IsFormatSupported(
			/* [in] */ const GUID *pguidFormat);
		virtual HRESULT STDMETHODCALLTYPE QueryPreferredFormat(
			/* [out] */ GUID *pguidFormat);
		virtual HRESULT STDMETHODCALLTYPE GetTimeFormat(
			/* [out] */ GUID *pguidFormat);
		virtual HRESULT STDMETHODCALLTYPE IsUsingTimeFormat(
			/* [in] */ const GUID *pguidFormat);
		virtual HRESULT STDMETHODCALLTYPE SetTimeFormat(
			/* [in] */ const GUID *pguidFormat);
		virtual HRESULT STDMETHODCALLTYPE GetDuration(
			/* [out] */ LONGLONG *pllDuration);
		virtual HRESULT STDMETHODCALLTYPE GetStopPosition(
			/* [out] */ LONGLONG *pllStop);
		virtual HRESULT STDMETHODCALLTYPE GetCurrentPosition(
			/* [out] */ LONGLONG *pllCurrent);
		virtual HRESULT STDMETHODCALLTYPE ConvertTimeFormat(
			/* [out] */ LONGLONG *pllTarget,
			/* [in] */ const GUID *pguidTargetFormat,
			/* [in] */ LONGLONG llSource,
			/* [in] */ const GUID *pguidSourceFormat);
		virtual HRESULT STDMETHODCALLTYPE SetPositions(
			/* [out][in] */ LONGLONG *pllCurrent,
			/* [in] */ DWORD dwCurrentFlags,
			/* [out][in] */ LONGLONG *pllStop,
			/* [in] */ DWORD dwStopFlags);
		virtual HRESULT STDMETHODCALLTYPE GetPositions(
			/* [out] */ LONGLONG *pllCurrent,
			/* [out] */ LONGLONG *pllStop);
		virtual HRESULT STDMETHODCALLTYPE GetAvailable(
			/* [out] */ LONGLONG *pllEarliest,
			/* [out] */ LONGLONG *pllLatest);
		virtual HRESULT STDMETHODCALLTYPE SetRate(
			/* [in] */ double dblRate);
		virtual HRESULT STDMETHODCALLTYPE GetRate(
			/* [out] */ double *pdblRate);
		virtual HRESULT STDMETHODCALLTYPE GetPreroll(
			/* [out] */ LONGLONG *pllPreroll);

		// IStreamBufferPlayback:
		STDMETHOD(SetTunePolicy)(STRMBUF_PLAYBACK_TUNE_POLICY eStrmbufPlaybackTunePolicy);
		STDMETHOD(GetTunePolicy)(STRMBUF_PLAYBACK_TUNE_POLICY *peStrmbufPlaybackTunePolicy);
		STDMETHOD(SetThrottlingEnabled)(BOOL fThrottle);
		STDMETHOD(GetThrottlingEnabled)(PBOOL pfThrottle);
		STDMETHOD(NotifyGraphIsConnected)();
		STDMETHOD(IsAtLive)(BOOL *pfAtLive);
		STDMETHOD(GetOffsetFromLive)(LONGLONG *pllOffsetFromLive);
		STDMETHOD(SetDesiredOffsetFromLive)(LONGLONG llOffsetFromLive);
		STDMETHOD(GetLoadIncarnation)(DWORD *pdwLoadIncarnation);
		STDMETHOD(EnableBackgroundPriority)(BOOL fEnableBackgroundPriority);

		// ISampleSource:
		virtual LONGLONG GetLatestSampleTime() const;
		virtual LONGLONG GetProducerTime() const;

	protected:
		struct CProducerSampleState  // Short form: cpsstate
		{
			CProducerSampleState(void);
			~CProducerSampleState(void);

			bool m_fProducerInFlush;
			CDVROutputPin *m_pcDVROutputPin;
			LONGLONG m_llLatestMediaStart, m_llLatestMediaEnd;

			CProducerSampleState(const CProducerSampleState &rcProducerSampleState);
			CProducerSampleState &operator =(const CProducerSampleState &);
		};
		typedef std::vector<CProducerSampleState> VectorCProducerSampleState;
			// Short form:  vcpss

		friend class CSampleConsumerTestHooks;
		friend class CConsumerStreamingThread;
		friend class CEmulatedSourcePipelineTestHooks;

		void Cleanup(SAMPLE_CONSUMER_INTERNAL_EVENT_TYPE eSCInternalEventType);
		ROUTE UpdateMediaSource(SAMPLE_CONSUMER_INTERNAL_EVENT_TYPE eSCInternalEventType);
		void DisableMediaSource(SAMPLE_CONSUMER_INTERNAL_EVENT_TYPE eSCInternalEventType);
		void CleanupProducer();
		IReader *GetReader();
		bool ViewingRecordingInProgress() const;
		void RebindToProducer();
		void UpdateRecordingName(bool fIsLiveTv, LPCOLESTR pszFileName);
		void UpdateProducerState(PRODUCER_STATE eProducerState);
		bool CanSupportRate(double dRate);
		ROUTE DetermineSampleRoute(IMediaSample &riSample,
			SAMPLE_CONSUMER_INTERNAL_EVENT_TYPE eConsumerSampleEvent,
			CDVROutputPin &rcdvrOutputPin);
		bool SeekInProducerRange(LONGLONG &rhyCurPosition);
		void SetCurrentPosition(SampleProducer::CPauseBufferSegment *pcDesiredRecording,
			LONGLONG hyDesiredPosition, bool fSeekToKeyFrame,
			bool fNoFlush, bool fNotifyDecoder, bool fOnAppThread);
		HRESULT GetConsumerPositions(LONGLONG &rhyMinPos,
											LONGLONG &rhyCurPos,
											LONGLONG &rhyMaxPos);
		void MapPins(void);
		CProducerSampleState &MapToSourcePin(UCHAR bProducerPinIndex);
		UCHAR MapToSinkPin(UCHAR bSourcePinIndex);
		void SetGraphRate(double dblRate, bool fDeferUntilRendered = false,
				LONGLONG hyTargetPosition = -1LL,
				long lEventId = 0, long lParam1 = 0, long lParam2 = 0);
		void SetGraphPosition(LONGLONG hyPosition, bool fNoFlush, bool fSkipTimeHole,
			bool fSeekKeyFrame, bool fOnAppThread);
		void EnsureRecordingIsSelected();
		void ClearRecordingHistory();
		void ClearPlaybackPosition(SAMPLE_CONSUMER_INTERNAL_EVENT_TYPE eSCInternalEventType);
		IDecoderDriver *GetDecoderDriver(void);
		void NoteBadGraphState(HRESULT hr);
		void StopGraph(PLAYBACK_END_MODE ePlaybackEndMode);
		void StartStreamingThread();
		void StopStreamingThread();
		void DoProcessProducerSample(UCHAR bPinIndex, LONGLONG hySampleId,
									ULONG uModeVersionCount,
								   	LONGLONG hyMediaStartPos, LONGLONG hyMediaEndPos);
		void DoNotifyProducerCacheDone(ULONG uModeVersionCount);
		void DoUpdateProducerMode();
		void DoNotifyPauseBuffer(CPauseBufferData *pData);
		bool CanDeliverMediaSamples(bool fOkayIfEndOfStream);
		bool DeliveringMediaSamples();
		bool DeliveringReaderSamples();
		bool DeliveringProducerSamples();
		void Pause();
		void Run();
		void Activate();
		void Deactivate();
		inline bool Stopped() const
			{
				return (m_eSCFilterState == SC_FILTER_NEVER_STARTED) ||
					(m_eSCFilterState == SC_FILTER_STOPPED) ||
					(m_eSCFilterState == SC_FILTER_NEVER_STARTED) ||
					(m_eSCFilterState == SC_FILTER_DEACTIVATING);
			}
		inline bool Paused() const
			{
				return (m_eSCFilterState == SC_FILTER_FIRST_ACTIVATION) ||
					(m_eSCFilterState == SC_FILTER_ACTIVATING) ||
					(m_eSCFilterState == SC_FILTER_PAUSED) ||
					(m_eSCFilterState == SC_FILTER_PAUSING);
			}
		inline bool Running() const
			{
				return (m_eSCFilterState == SC_FILTER_ABOUT_TO_RUN) ||
					(m_eSCFilterState == SC_FILTER_RUNNING);
			}
		static FILTER_STATE MapToSourceFilterState(SAMPLE_CONSUMER_FILTER_STATE eSampleConsumerFilterState);
		bool EnforceStopPosition();
		bool EnsureNotAtStopPosition(SAMPLE_CONSUMER_INTERNAL_EVENT_TYPE eSCInternalEventType);
		void ImportPauseBuffer(CPauseBufferData *pcPauseBufferData, LONGLONG hyLastRecordingPos);
		inline bool IsSourceProducer(SAMPLE_CONSUMER_SAMPLE_SOURCE eSCSampleSource)
			{
				return (eSCSampleSource == SC_SAMPLE_SOURCE_CONVERTING_TO_PRODUCER) ||
					(eSCSampleSource == SC_SAMPLE_SOURCE_PRODUCER);
			}
		inline bool IsSourceReader(SAMPLE_CONSUMER_SAMPLE_SOURCE eSCSampleSource)
			{
				return (eSCSampleSource == SC_SAMPLE_SOURCE_CONVERTING_TO_READER) ||
					(eSCSampleSource == SC_SAMPLE_SOURCE_READER);
			}
		inline bool IsSourceProducer()
			{
				return IsSourceProducer(m_eSampleSource);
			}
		inline bool IsSourceReader()
			{
				return IsSourceReader(m_eSampleSource);
			}
		void LeaveOrphanedRecording(SampleProducer::CPauseBufferSegment *&pcPauseBufferSegmentNext);
		HRESULT CSampleConsumer::DoLoad(
					bool fExpectLiveTv,
					LPCOLESTR pszFileName,
					const AM_MEDIA_TYPE *pmt);
		virtual void ResumeNormalPlayIfNeeded();
		void ProcessPauseBufferUpdates();
		void CompensateForWhackyClock();
		void BeginFlush(bool fFromStreamingThread = false);
		void EndFlush(bool fStopping, bool fFromStreamingThread = false);
		HRESULT GetConsumerPositionsExternal(LONGLONG &hyMinPos, 
											  LONGLONG &hyCurPos, LONGLONG &hyMaxPos,
											  bool fLieAboutEndOfMedia = true);
		CPauseBufferData *MergeWithOrphans(LONGLONG &hyPosRecordingPos, CPauseBufferData *pData, bool fOldDataIsValid = true);
		void SetCurrentPositionToTrueSegmentEnd(SampleProducer::CPauseBufferSegment *pcPauseBufferSegment,
			bool fImplicitlyASeek);
		LONGLONG GetPauseBufferSafetyMargin(bool fPadForSeek)
			{
				LONGLONG hySafetyMargin = s_hyUnsafelyCloseToTruncationConstant;
				if (fPadForSeek)
					hySafetyMargin += s_hySafetyMarginSeekVersusPlay;
				if (m_dblRate > 0.0)
					hySafetyMargin += (LONGLONG) (((double) s_hyUnsafelyCloseToTruncationRate) * m_dblRate);
				else if (m_dblRate < 0.0)
					hySafetyMargin += (LONGLONG) (((double) s_hyUnsafelyCloseToTruncationRate) * -m_dblRate);
				return hySafetyMargin;
			}
		void OnPosition(LONGLONG hyTargetPosition, double dblPriorRate, unsigned uPositionNotifyId,
						long lEventId, long lParam1, long lParam2);
		double GetTrueRate() const
			{  return (m_eSCPositionMode == SC_POSITION_PENDING_NORMAL_RATE) ? m_dblPreDeferNormalRate : m_dblRate; }
		void SetPositionMode(SAMPLE_CONSUMER_POSITION_MODE eNewMode)
			{
				if (m_eSCPositionMode == SC_POSITION_PENDING_NORMAL_RATE)
				{
					m_dblRate = m_dblPreDeferNormalRate;
					++m_uPositionNotifyId;
				}
				m_eSCPositionMode = eNewMode;
			}

		bool IsLiveOrInProgress() const
			{
				return (NULL != m_piSampleProducer) &&
					(!m_pcPauseBufferHistory || m_pcPauseBufferHistory->IsInProgress());
			}

		friend class CConsumerNotifyOnPosition;

		CSmartRefPtr<SampleProducer::CPauseBufferHistory> m_pcPauseBufferHistory;
		CSmartRefPtr<SampleProducer::CPauseBufferSegment> m_pcPauseBufferSegment;
		CSmartRefPtr<SampleProducer::CPauseBufferSegment> m_pcPauseBufferSegmentBound;
		CPinMappings *m_pcPinMappings;
		FILTER_STATE m_eFilterStateProducer;
		ISampleProducer *m_piSampleProducer;
		SAMPLE_PRODUCER_MODE m_eSampleProducerMode;
		SAMPLE_PRODUCER_MODE m_eSampleProducerActiveMode;
		ULONG m_uSampleProducerModeIncarnation;
		UCHAR m_bOutputPins;
		VectorCProducerSampleState m_vcpssOutputPinMapping;
		IPlaybackPipelineManager *m_pippmgr;
		std::wstring m_pszFileName;
		GUID m_pguidCurTimeFormat;
		double m_dblRate, m_dblPreDeferNormalRate;
		IPauseBufferMgr * m_pipbmgrNext;
		IFileSourceFilter *m_pifsrcfNext;
		IStreamBufferMediaSeeking *m_pisbmsNext;
		IStreamBufferPlayback *m_piStreamBufferPlayback;
		STRMBUF_PLAYBACK_TRANSITION_POLICY m_esbptp;
		STRMBUF_PLAYBACK_TUNE_POLICY m_eStrmBufPlaybackTunePolicy;
		IReader *m_piReader;
		IDecoderDriver *m_piDecoderDriver;
		LONGLONG m_hyStopPosition;
		LONGLONG m_hyCurrentPosition;
		LONGLONG m_hyTargetPosition;
		LONGLONG m_hyDesiredProducerPos;
		CConsumerStreamingThread *m_pcConsumerStreamingThread;
		CCritSec m_cCritSecThread, m_cCritSecState;
		bool m_fSampleMightBeDiscontiguous;
		static LONGLONG s_hyUnsafelyCloseToWriter;
		static LONGLONG s_hyUnsafelyCloseToTruncationConstant;
		static LONGLONG s_hySafetyMarginSeekVersusPlay;
		static LONGLONG s_hyUnsafelyCloseToTruncationRate;
		static LONGLONG s_hyOmitSeekOptimizationWindow;
		bool m_fFlushing;
		bool m_fThrottling;
		bool m_fResumingNormalPlay;
		bool m_fDroppingToNormalPlayAtBeginning;
		bool m_fPendingFailedRunEOS;
		bool m_fConsumerSetGraphRate;
		DWORD m_dwResumePlayTimeoutTicks;
		CPipelineRouter m_cPipelineRouterForSamples;
		LONGLONG m_hyPostFlushPosition;
		LONGLONG m_hyPostTunePosition;
		unsigned m_uPositionNotifyId;
		bool m_fInRateChange;
		BOOL m_fEnableBackgroundPriority;
		LONGLONG m_hyTargetOffsetFromLive;
		CCritSec m_cCritSecPauseBuffer;
		DWORD m_dwLoadIncarnation;

		struct SPendingPauseBufferUpdate
		{
			CSmartRefPtr<CPauseBufferData> pcPauseBufferData;
		};

		std::list<SPendingPauseBufferUpdate> m_listPauseBufferUpdates;

		SAMPLE_CONSUMER_FILTER_STATE m_eSCFilterState;
		SAMPLE_CONSUMER_LOAD_STATE m_eSCLoadState;
		SAMPLE_CONSUMER_SAMPLE_SOURCE m_eSampleSource;
		SAMPLE_CONSUMER_PIPELINE_STATE m_eSCPipelineState;
		SAMPLE_CONSUMER_STOP_MODE m_eSCStopMode;
		SAMPLE_CONSUMER_POSITION_MODE m_eSCPositionMode;
		SAMPLE_CONSUMER_RECORDING_STATE m_eSCRecordingState;

#if defined(SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT)
		REFERENCE_TIME m_rtProducerToConsumerClockOffset;
		IReferenceClock *m_piGraphClock;
#endif // SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT

	};

}
