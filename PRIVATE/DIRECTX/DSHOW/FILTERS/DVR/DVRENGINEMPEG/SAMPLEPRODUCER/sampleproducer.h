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
#include "DvrInterfaces.h"
#include "CPauseBufferHistory.h"
#include "..\\DVREngine.h"
#include "CPinMapping.h"
#include "..\\HResultError.h"
#include "CClockState.h"
#include "ProducerConsumerUtilities.h"

struct BASIC_OBJECT;

namespace MSDvr
{
	class CSampleProducer;

	class CSampleConsumerState     // Short form:  cssc
	{
	public:
		CSampleConsumerState();
		~CSampleConsumerState();

		CSampleConsumerState(const CSampleConsumerState &);
		CSampleConsumerState &operator =(const CSampleConsumerState &);

		ISampleConsumer *m_piSampleConsumer;
		bool m_fHasMediaType;
		SAMPLE_PRODUCER_MODE m_eSampleProducerMode;
		ULONG m_uModeVersionCount;
	};

	class CSampleConsumerBindings
	{
	public:
		CSampleConsumerBindings();
		~CSampleConsumerBindings();

		CSampleConsumerState &GetState(ISampleConsumer *);
		bool IsBound(ISampleConsumer *);
		void Add(CSampleConsumerState &);
		bool Unbind(ISampleConsumer *);
		void Clear();

	protected:
		friend class CSampleConsumerBindingIterator;

		std::list<CSampleConsumerState> m_listCSampleConsumerState;
		int m_lNumListIterators;
		int m_lNumPendingUnbinds;

	private:
		CSampleConsumerBindings(const CSampleConsumerBindings &);
		CSampleConsumerBindings &operator =(const CSampleConsumerBindings &);
	};

	class CSampleConsumerBindingIterator
	{
	public:
		CSampleConsumerBindingIterator(CSampleConsumerBindings &);
		~CSampleConsumerBindingIterator();

		CSampleConsumerState &GetCurrent();
		bool MoveToNext();

	private:
		CSampleConsumerBindingIterator &operator =(const CSampleConsumerBindingIterator &);
		CSampleConsumerBindingIterator(const CSampleConsumerBindingIterator &);

		bool m_fNewlyInited;
		CSampleConsumerBindings &m_rcSampleConsumerBindings;
		std::list<CSampleConsumerState>::iterator m_iteratorCSampleConsumerState;
	};

	class CProducerSample
	{
	public:
		CProducerSample(UCHAR bPinIndex, LONGLONG hyMediaStartTime, LONGLONG hyMediaEndTime,
						IMediaSample &riMediaSample);
		CProducerSample()
			: m_bPinIndex(255)
			, m_hyMediaStartTime(-1)
			, m_hyMediaEndTime(-1)
			, m_hyMediaSampleID(-1)
			, m_piMediaSample(0)
			{}
		~CProducerSample();
		CProducerSample &operator = (const CProducerSample &);
		CProducerSample(const CProducerSample&);

		void Clear()
		{
			m_bPinIndex = 255;
			m_hyMediaStartTime = -1;
			m_hyMediaSampleID = 0;
			m_hyMediaEndTime = -1;
			m_piMediaSample = NULL;
		}

		UCHAR m_bPinIndex;
		LONGLONG m_hyMediaStartTime;
		LONGLONG m_hyMediaSampleID, m_hyMediaEndTime;
		CComPtr<IMediaSample> m_piMediaSample;

	private:
		static LONGLONG s_hyNextMediaSampleID;
	};

	class CSampleProducerPinRecord  // Short form:  csppr
	{
	public:
		CSampleProducerPinRecord(CPinMappings &rcPinMapping, UCHAR bPinIndex);
		~CSampleProducerPinRecord(void);

		CSampleProducerPinRecord &operator =(const CSampleProducerPinRecord &);
		CSampleProducerPinRecord(const CSampleProducerPinRecord &);

		SPinState &m_rsPinState;
		ULONG m_uMaxCachedSamples;
		ULONG m_uCurCachedSamples;
	};

	typedef std::vector<CSampleProducerPinRecord> VectorCSampleProducerPinRecord;
		// Short form: vcsppr
	typedef std::vector<CSampleProducerPinRecord>::iterator VectorCSampleProducerPinRecordIterator;

	class CSampleCache
	{
	public:
		CSampleCache();
		~CSampleCache();

		void Init(VectorCSampleProducerPinRecord &rvectorCSampleProducerPinRecord);
		void Append(CProducerSample &rcProducerSample);
		void Clear();
		void PruneTo(unsigned uFirstKeep);
		CProducerSample &Find(LONGLONG hySampleID);
		unsigned GetMax() { return m_uMaxSamples - 1; }

		// For debugging chatter:
		LONGLONG GetOldestSampleStart();

	protected:
		friend class CSampleCacheFwdIterator;
		friend class CSampleCacheRevIterator;

		unsigned Increment(unsigned uIdx)
			{
				return (uIdx + 1 >= m_uMaxSamples) ? 0 : uIdx + 1;
			}

		unsigned Decrement(unsigned uIdx)
			{
				return (uIdx == 0) ? m_uMaxSamples-1 : uIdx - 1;
			}

		CProducerSample *m_pcProducerSample;
		unsigned m_uIdxOldest, m_uIdxToFill, m_uMaxSamples;

	private:
		// Deliberately not implemented:
		CSampleCache(const CSampleCache &);
		CSampleCache & operator =(const CSampleCache &);
	};

	class CSampleCacheFwdIterator
	{
	public:
		CSampleCacheFwdIterator(CSampleCache &rcSampleCache)
			: m_rcSampleCache(rcSampleCache)
			, m_uIdxCurrent(rcSampleCache.m_uIdxOldest)
			{}

		~CSampleCacheFwdIterator() {}

		bool HasMore() { return m_uIdxCurrent != m_rcSampleCache.m_uIdxToFill; }

		unsigned GetPosition() { return m_uIdxCurrent; }

		CProducerSample &Current()
			{
				if (!HasMore())
					throw CHResultError(E_FAIL);
				return m_rcSampleCache.m_pcProducerSample[m_uIdxCurrent];
			}

		void Next()
			{
				m_uIdxCurrent = m_rcSampleCache.Increment(m_uIdxCurrent);
			}

	protected:
		unsigned m_uIdxCurrent;
		CSampleCache &m_rcSampleCache;
	};

	class CSampleCacheRevIterator
	{
	public:
		CSampleCacheRevIterator(CSampleCache &rcSampleCache)
			: m_rcSampleCache(rcSampleCache)
			, m_uIdxCurrent(rcSampleCache.m_uIdxToFill)
			{}
		~CSampleCacheRevIterator() {}

		bool HasMore() { return m_uIdxCurrent != m_rcSampleCache.m_uIdxOldest; }

		unsigned GetPosition() { return m_rcSampleCache.Decrement(m_uIdxCurrent); }

		CProducerSample &Current()
			{
				if (!HasMore())
					throw CHResultError(E_FAIL);
				return m_rcSampleCache.m_pcProducerSample[m_rcSampleCache.Decrement(m_uIdxCurrent)];
			}

		void Previous()
			{
				m_uIdxCurrent = m_rcSampleCache.Decrement(m_uIdxCurrent);
			}

	protected:
		unsigned m_uIdxCurrent;
		CSampleCache &m_rcSampleCache;
	};

	class CSampleProducerSampleHistory
	{
	public:
		CSampleProducerSampleHistory(CSampleProducer &rcSampleProducer);
		~CSampleProducerSampleHistory();

		void Init(CPinMappings &rcPinMappings);
		void Clear();

		ROUTE OnNewSample(UCHAR bPinIndex, IMediaSample &riMediaSample);
		void OnBeginFlush(UCHAR bPinIndex);
		void OnEndFlush(UCHAR bPinIndex);
		void OnNewSegment(UCHAR bPinIndex, REFERENCE_TIME rtStart, REFERENCE_TIME rtEnd, double dblRate);

		UCHAR m_bPinsInFlush;
		UCHAR m_bPrimaryPinIndex;
		bool m_fFlushEnded;
		bool m_fSawSample;
		bool m_fNewSegmentStarted;
		LONGLONG m_hyMinPosition, m_hyMaxPosition, m_hyLastRawMediaStart, m_hyLastDeltaMediaStart;
		VectorCSampleProducerPinRecord m_vcsppr;
		LONGLONG m_hyClockOffset;  // 100 ns units
		CSampleCache m_cSampleCache;

	protected:
		void Prune(UCHAR bPinIndex);

		CSampleProducer &m_rcSampleProducer;

	private:
		CSampleProducerSampleHistory(const CSampleProducerSampleHistory &);
		CSampleProducerSampleHistory & operator=(const CSampleProducerSampleHistory &);
	};

	struct CSampleProducerPendingEvent
	{
		long lEventId, lEventParam1;
	};

	class CSampleProducer :
		public ICapturePipelineComponent,
		public TRegisterableCOMInterface<IFileSinkFilter>,
		public TRegisterableCOMInterface<IFileSinkFilter2>,
		public TRegisterableCOMInterface<IStreamBufferCapture>,
		public ISampleProducer,
		public IPauseBufferCallback
	{
	public:
		CSampleProducer();
		virtual ~CSampleProducer();

		/* ISampleProducer: */
		virtual bool IsHandlingFile(LPCOLESTR);
	    virtual bool IsLiveTvToken(LPCOLESTR) const;
		virtual HRESULT BindConsumer(ISampleConsumer *, LPCOLESTR);
		virtual HRESULT UnbindConsumer(ISampleConsumer *);
		virtual HRESULT UpdateMode(SAMPLE_PRODUCER_MODE, ULONG, LONGLONG, ISampleConsumer *);
		virtual CPauseBufferData* GetPauseBuffer();
		virtual PRODUCER_STATE GetSinkState();
		virtual UCHAR QueryNumInputPins();
		virtual CMediaType QueryInputPinMediaType(UCHAR);
		virtual void GetPositionRange(LONGLONG &, LONGLONG &) const;
		virtual bool GetSample(UCHAR , LONGLONG, IMediaSample & );
		void UpdateLatestTime(LONGLONG sampleStart);
		virtual void GetModeForConsumer(ISampleConsumer *piSampleConsumer,
			SAMPLE_PRODUCER_MODE &reSampleProducerMode, ULONG &ruModeVersionCount);
		virtual LONGLONG GetReaderWriterGapIn100Ns() const;
		virtual HRESULT GetGraphClock(IReferenceClock** ppIReferenceClock) const;
		virtual LONGLONG GetProducerTime() const;
		virtual IPipelineComponent *GetPipelineComponent();
		virtual bool IndicatesFlushWasTune(IMediaSample &riMediaSample, CDVRInputPin&	rcInputPin);
		virtual bool IndicatesNewSegmentWasTune(IMediaSample &riMediaSample, CDVRInputPin &rcInputPin);

		/* ISinkFilter2: */
		virtual HRESULT STDMETHODCALLTYPE SetFileName(
			/* [in] */ LPCOLESTR ,
			/* [unique][in] */ const AM_MEDIA_TYPE *);
		virtual HRESULT STDMETHODCALLTYPE GetCurFile(
			/* [out] */ LPOLESTR *,
			/* [out] */ AM_MEDIA_TYPE *);
		virtual HRESULT STDMETHODCALLTYPE SetMode(
			/* [in] */ DWORD );
		virtual HRESULT STDMETHODCALLTYPE GetMode(
			/* [out] */ DWORD *);
	
		/* IStreamBufferCapture: */
		virtual HRESULT STDMETHODCALLTYPE GetCaptureMode(
			/* [out] */ STRMBUF_CAPTURE_MODE *,
			/* [out] */ LONGLONG *);
		virtual HRESULT STDMETHODCALLTYPE BeginTemporaryRecording(
			/* [in] */ LONGLONG );
		virtual HRESULT STDMETHODCALLTYPE BeginPermanentRecording(
			/* [in] */ LONGLONG,
			/* [out] */ LONGLONG *);
		virtual HRESULT STDMETHODCALLTYPE ConvertToTemporaryRecording(
			/* [in] */ LPCOLESTR );
		virtual HRESULT STDMETHODCALLTYPE SetRecordingPath(
			/* [in] */ LPCOLESTR );
		virtual HRESULT STDMETHODCALLTYPE GetRecordingPath(
			/* [out] */ LPOLESTR *);
		virtual HRESULT STDMETHODCALLTYPE GetBoundToLiveToken(
			/* [out] */ LPOLESTR *ppszToken);
		STDMETHODIMP GetCurrentPosition(/* [out] */ LONGLONG *phyCurrentPosition);

		/* IPipelineComponent: */
		virtual void RemoveFromPipeline();
		virtual ROUTE GetPrivateInterface(REFIID, void *&);
		virtual ROUTE NotifyFilterStateChange(FILTER_STATE );
		virtual ROUTE ConfigurePipeline(
			UCHAR ,
			CMediaType cMediaTypes[],
			UINT ,
			BYTE bCustom[]);
		virtual ROUTE DispatchExtension(
						// @parm The extension to be dispatched.
						CExtendedRequest &rcExtendedRequest);

		/* ICapturePipelineComponent: */
		virtual unsigned char AddToPipeline(ICapturePipelineManager& );
		virtual ROUTE ProcessInputSample(IMediaSample &, CDVRInputPin &);
		virtual ROUTE NotifyBeginFlush(CDVRInputPin &);
		virtual ROUTE NotifyEndFlush(CDVRInputPin &);
		virtual ROUTE GetAllocatorRequirements(
			ALLOCATOR_PROPERTIES &,	CDVRInputPin &);
		virtual ROUTE	NotifyNewSegment(
						CDVRInputPin&	rcInputPin,
						REFERENCE_TIME rtStart,
						REFERENCE_TIME rtEnd,
						double dblRate);

		/* IPauseBufferCallback: */
	    virtual void PauseBufferUpdated(CPauseBufferData *pData);

		/* For helper classes: */

		FILTER_STATE GetFilterState() { return m_eFilterState; }

	protected:
		virtual void Cleanup();
		IPauseBufferMgr *GetPauseBufferMgr();
		bool GetRecordingNameFromWriter(std::wstring &recordingName);
		void NotifyConsumersOfState();
		void NotifyConsumersOfSample(LONGLONG , LONGLONG, LONGLONG, UCHAR );
		void NotifyConsumersOfTune(LONGLONG);
		void NotifyConsumersOfPauseBufferChange(CPauseBufferData *pcPauseBufferData);
		void NotifyConsumerOfCachePrune(LONGLONG hyFirstSampleIDRetained);
		void DisassociateFromConsumers();
		LONGLONG GetFilterGraphClockTime();
		bool IsPrimaryPin(UCHAR bPinIndex);
		UCHAR MapInputPin(CDVRInputPin &);
		bool IsRecordingKnown(const wchar_t *pwszRecordingName);
		void ComputeBufferQuota(AM_MEDIA_TYPE *pmt, ULONG &, ULONG &);

		// virtuals to be overridden by format-specific subclasses
		// of CSampleProducer:
		virtual void ExtractMediaTime(UCHAR bPinIndex,
			IMediaSample &riMediaSample,
			LONGLONG &sampleStart,
			BYTE *pbSampleData, long lSampleDataSize);		// throw an exception on failure
		virtual void SetMediaTime(UCHAR bPinIndex,
			IMediaSample &riMediaSample,
			LONGLONG sampleStart, LONGLONG sampleEnd,
			BYTE *pbSampleData, long lSampleDataSize); // throw an exception on failure
		virtual bool IsSampledWanted(UCHAR bPinIndex, IMediaSample &riMediaSample);
		virtual void GetSampleDeliveryPolicy(UCHAR bPinIndex,
			IMediaSample &riMediaSample,
			bool &fCacheForConsumer, ROUTE &eRouteForWriter);
		virtual bool IsPostFlushSampleAfterTune(UCHAR bPinIndex, IMediaSample &riMediaSample) const;
		virtual bool IsNewSegmentSampleAfterTune(UCHAR bPinIndex, IMediaSample &riMediaSample) const;
		virtual void EnsureConsumerHasMediaType(CSampleConsumerState &rcSampleConsumerState);
		virtual void OverrideMinimumSize(AM_MEDIA_TYPE *pmt, ULONG &uProposedMinimumSize);
		virtual void OverrideWriterLatencyToBufferCount(
				AM_MEDIA_TYPE *pmt,
				LONGLONG hyWriterLatencyIn100NsUnits,
				ULONG uProposedBufferCount);
		virtual void NotifyPinUpdate();
		virtual void NotifyConsumersOfRecordingStart(LONGLONG hyPrerecordingPos, LONGLONG hyRecordingStart);
		virtual LONGLONG GetAverageSampleInterval();

		friend class CSampleProducerTestHooks;
		friend class CSampleProducerSampleHistory;

		CSampleConsumerBindings m_cSampleConsumerBindings;
		CSmartRefPtr<CPauseBufferData> m_pcPauseBufferData;
		IFileSinkFilter2 *m_pifsinkf2Next;
		IStreamBufferCapture *m_pisbcNext, *m_pisbcFilter;
		FILTER_STATE m_eFilterState;
		CSampleProducerSampleHistory m_cSampleProducerSampleHistory;
		ICapturePipelineManager *m_picpmgr;
		IPauseBufferMgr *m_pipbmgr;
		IFileSinkFilter2 *m_pifsinkf2Writer;
			// producer is deleted by pipeline so no ref is needed +
			// keeping a ref creates a circularity to keeps sink filter
			// from being freed.
		CPinMappings *m_pcPinMappings;
		IWriter *m_piWriter;
		CCritSec m_cCritSecStream;
		std::wstring m_pwszSinkID;
		IMediaTypeAnalyzer *m_piMediaTypeAnalyzer;
		bool m_fRecordingRequested;
		CClockState m_cClockState;
		std::wstring m_pwszCurrentPermRecording, m_pwszCompletedPermRecording;
		std::list<CSampleProducerPendingEvent> m_listCSampleProducerPendingEvent;
		IMediaEventSink *m_piMediaEventSink;
		LONGLONG m_hyTimeToPositionOffset;

		static const LONGLONG s_hy100NsPerSecond;
		static const LONGLONG s_hyOverlapIn100NsUnits;
		static const long s_lMinimumBufferQuota;
		static const LONGLONG s_hySleepTimeAs100Ns;

#ifdef DEBUG
		static unsigned long s_uSinkIDCounter;
#endif

public:
	};

	typedef std::list<CSampleConsumerState>::iterator ListCSampleConsumerStateIterator;
	typedef std::vector<CSampleProducerPinRecord>::iterator VectorCSampleProducerPinRecordIterator;
		// short form:  vcsppriterator
}
