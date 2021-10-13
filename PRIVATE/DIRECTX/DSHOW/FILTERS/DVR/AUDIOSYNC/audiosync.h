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
// AudioSync.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "IAudioQA.h"
#include "AudioSyncLiveTVQualityProp.h"

struct IAlphaMixer;

#ifndef SHIP_BUILD
#define ENABLE_DIAGNOSTIC_OUTPUT
#endif // !SHIP_BUILD

class AudioSyncFilter : public CTransInPlaceFilter
						, public IKsPropertySet
#ifdef CTV_ENABLE_TESTABILITY
						, public IAudioQA
#endif // CTV_ENABLE_TESTABILITY
{
public:
	AudioSyncFilter(TCHAR *pwszFilterName, LPUNKNOWN pUnknown, REFCLSID clsid, HRESULT *pHr);

	// Transform-in-place methods
	virtual HRESULT Transform(IMediaSample *pSample);

	// Tranform-filter methods
    virtual HRESULT CheckInputType(const CMediaType* mtIn);
	virtual CBasePin* GetPin(int n);

	// Override
	STDMETHOD(NonDelegatingQueryInterface) (REFIID riid, LPVOID* ppv);

	// COM glue
	DECLARE_IUNKNOWN
	static CUnknown* WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

    STDMETHODIMP Run(REFERENCE_TIME tStart);
	STDMETHODIMP Stop();
	STDMETHODIMP Pause();

    // chance to customize the transform process
    virtual HRESULT Receive(IMediaSample *pSample);

    // if you override Receive, you may need to override these three too
    virtual HRESULT EndOfStream(void);
    virtual HRESULT BeginFlush(void);
    virtual HRESULT EndFlush(void);

    // IKsPropertySet implementation
    STDMETHODIMP Set(REFGUID guidPropSet, DWORD dwPropID, LPVOID pInstanceData, DWORD cbInstanceData, LPVOID pPropData, DWORD cbPropData);
    STDMETHODIMP Get(REFGUID guidPropSet, DWORD dwPropID, LPVOID pInstanceData, DWORD cbInstanceData, LPVOID pPropData, DWORD cbPropData, DWORD *pcbReturned);
    STDMETHODIMP QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport);

#ifdef CTV_ENABLE_TESTABILITY
	// IAudioQA:
	STDMETHOD(GetAudioStats)(/* [out] */ AUDIO_QA_STATS *pAudioQAStats);
	STDMETHOD(SetAudioSampleInterval)(/* [in] */ DWORD dwSystemTicksAccumulateData);
	STDMETHOD(GetAudioSampleInterval)(/* [out] */ DWORD *pdwSystemTicksAccumulateData);
	STDMETHOD(SetAudioQualityCallback)(/* [in] */ PFN_AUDIO_QUALITY_CALLBACK pfnAudioQualityCallback);
#endif // CTV_ENABLE_TESTABILITY

	// for output pin:
	void SetBufferCount(long lBufferCount);

	// For initialization:
	static void InitHeuristicParameters();

	// For use anywhere to report a glitch:
	void ReportAVGlitch(AV_GLITCH_EVENT eAVGlitchEvent, long dwParam2);

private:
	HRESULT GetAudioRenderer(IBaseFilter** ppiAudioRenderer);
	HRESULT AdjustSpeed(double dblTargetRate,
		unsigned uBackPressure, REFERENCE_TIME rtDelta,
		bool &fDropRecommended, bool fRestoringToDefaultState = false);
	HRESULT CarryOutReceive(IMediaSample *pSample);

	// IBasicAudio internal implementation
    HRESULT get_Volume(long *plVolume);
    HRESULT put_Volume(long lVolume);

	class CBasicAudioImpl : public CBasicAudio
	{
		STDMETHOD(get_Volume) (long *plVolume);
		STDMETHOD(put_Volume) (long lVolume);
		STDMETHOD(get_Balance) (long *plBalance);
		STDMETHOD(put_Balance) (long lBalance);

	public:
		CBasicAudioImpl();
	}			m_cBasicAudioImpl;

	double		m_dblTargetRate;
	bool		m_fMuteDueToSpeed;
	long		m_lVolume;
	CCritSec	m_cVolumeLeafLock;
	INT32		m_iSamplesSinceMuteChange;  // > 0 implies heading toward possible re-mute, < 0 implies heading toward umute
	REFERENCE_TIME m_rtPauseStartStreamTime;
	REFERENCE_TIME m_rtOffsetFromLive;	// < 0 implies not at live
	REFERENCE_TIME m_rtLiveSafetyMargin;
	BOOL		m_fFirstSampleSinceFlush;
	double	    m_dblRenderingRate;

#ifdef CTV_ENABLE_TESTABILITY
	// IAudioQA statistics and supporting methods:

	AudioQAStats	m_qaStatsLastInterval;
	AudioQAStats    m_qaStatsInProgress;

	DWORD		m_dwQAStatsInterval;
	DWORD	    m_dwQAStatsIntervalStartTick;
	DWORD		m_dwAudioSampleCount;
	DWORD		m_dwSampleStartTick;
	LONGLONG	m_llMillisecAccumulatedLeadLag;
	DWORD		m_dwCountDownToExpectedStability;
	BOOL		m_fDoingNormalPlay;
	DWORD		m_dwNormalPlayStartTick;
	PFN_AUDIO_QUALITY_CALLBACK m_pfnAudioQualityCallback;

	enum QAStatEvent {
		QA_EVENT_CREATE_FILTER,
		QA_EVENT_STOP_FILTER,
		QA_EVENT_PAUSE_FILTER,
		QA_EVENT_RUN_FILTER,
		QA_EVENT_RATE_CHANGE,
		QA_EVENT_BEGIN_FLUSH,
		QA_EVENT_END_FLUSH,
		QA_EVENT_BEGIN_EXTERNAL_MUTE,
		QA_EVENT_END_EXTERNAL_MUTE,
		QA_EVENT_BEGIN_INTERNAL_MUTE,
		QA_EVENT_END_INTERNAL_MUTE,
		QA_EVENT_BUFFERS_RAN_DRY,
		QA_EVENT_PAUSE_RECOVERY
	};

	void CollectQAStats(HRESULT hr, REFERENCE_TIME rtDelta);
	void ClearQAStats();
	void OnQAStatEvent(QAStatEvent eQAStatEvent);
	void InitQAStats();
	void CheckForIntervalEnd(DWORD dwNow);

#endif // CTV_ENABLE_TESTABILITY

	class CEventHandle
	{
	public:
		CEventHandle(BOOL bManualReset, BOOL bInitialState)
			: m_hEvent(CreateEvent(NULL, bManualReset, bInitialState, NULL))
			{
			}

		~CEventHandle()
			{
				if (m_hEvent != NULL)
				{
					EXECUTE_ASSERT(CloseHandle(m_hEvent));
				}
			}

		operator HANDLE() const throw()
			{
				return m_hEvent;
			}

		operator HEVENT() const throw()
			{
				return (HEVENT)m_hEvent;
			}

			bool operator!() const throw()
		{
			return (m_hEvent == NULL);
		}

	private:
		// deliberately not implemented:
		CEventHandle & operator=(const CEventHandle &);
		CEventHandle(const CEventHandle &);

		const HANDLE m_hEvent;
	};

	enum {
		BAD_RESUME_FLAGS_NONE = 0,
		BAD_RESUME_FLAG_NEED_SAMPLE_DOWNSTREAM = 0x1,
		BAD_RESUME_FLAG_ADVISE_PENDING = 0x2
	};

	#define MAX_TIME_IN_RECEIVE					50		/* in system ticks == milliseconds */

	DWORD		 m_dwFlagsToHandleBadResume;
	DWORD		 m_dwBadResumeSampleCountdown;
	BOOL		 m_fSleepingInReceive;
	CCritSec	 m_cCritSecBadResumeState;
	CEventHandle m_hAdviseEvent;
	HANDLE		 m_hBadResumeRecoveryThread;
	bool		 m_fStartedFromStop;
	LONG		 m_lInReceiveCount;
	DWORD		 m_dwReceiveEntrySysTicks;
	CEventHandle m_hInRecoveryEvent;
	BOOL		 m_fGivenSampleSinceStop;
	CCritSec	 m_cCritSecTransform;

	enum AUDIO_SYNC_FRAME_STEP_MODE
	{
		AUDIO_SYNC_FRAME_STEP_NONE,
		AUDIO_SYNC_FRAME_STEP_ACTIVE,
		AUDIO_SYNC_FRAME_STEP_WAITING_FOR_RUN_TO_END,
		AUDIO_SYNC_FRAME_STEP_WAITING_FOR_PAUSE_TO_END,
		AUDIO_SYNC_FRAME_STEP_WAITING_FOR_ON_TIME_SAMPLE
	};

	AUDIO_SYNC_FRAME_STEP_MODE m_eAudioSyncFrameStepMode;

	struct InReceiveTracker
	{
		InReceiveTracker(AudioSyncFilter &rAudioSyncFilter)
			: m_rAudioSyncFilter(rAudioSyncFilter)
		{
			if (1 == InterlockedIncrement(&m_rAudioSyncFilter.m_lInReceiveCount))
			{
				m_rAudioSyncFilter.m_dwReceiveEntrySysTicks = GetTickCount();
			}
		}

		~InReceiveTracker()
		{
			InterlockedDecrement(&m_rAudioSyncFilter.m_lInReceiveCount);
		}

	private:
		AudioSyncFilter &m_rAudioSyncFilter;
	};

	HRESULT CreateBadResumeRecoveryThread();
	void DestroyBadResumeRecoveryThread();
	static DWORD BadResumeThreadMain(LPVOID);
	void BadResumeMain();
	bool NeedToFlushToRecover();
	LONGLONG GetRecommendedOffset(bool fCalledFromSetOffset = false, bool fComputeCurrent=true, LONGLONG hyInputOffset = 0);
	void DetermineIfBuffered();
	void RecheckOffsetFromLive(bool fForceCheck = false);

	enum BackPressureMode
	{
		BUILDING_BACKPRESSURE,
		ACHIEVED_BACKPRESSURE,
		NORMAL_BACKPRESSURE
	};

	enum LiveVersusBufferedState
	{
		KNOWN_TO_BE_LIVE_DATA,
		KNOWN_TO_BE_BUFFERED_DATA,
		UNKNOWN_DATA_BUFFERING
	};

	unsigned MeasureBackPressure();
	HRESULT ComputeVideoOffset(unsigned uBackPressureMeasure, REFERENCE_TIME rtDelta);
	void UpdateBackPressurePolicy(unsigned uBackPressureMeasure, REFERENCE_TIME rtDelta);
	HRESULT FixVideoOffset(REFERENCE_TIME rtDelta);
	HRESULT SetRenderingOffset(REFERENCE_TIME rtOffset, bool fUpdateCurrentOnly=false);
	void CacheDownstreamAllocator();
	void CacheAlphaMixer();
	void CacheStreamBufferPlayback();
	void ClearTimeDeltas(bool fRestoreNormalSpeed=true);
	inline REFERENCE_TIME GetWeightedTimeDelta()
		{ return (REFERENCE_TIME) m_dblRunningAvgLead; }
	double ComputeTargetSpeed(REFERENCE_TIME rtDelta, unsigned uBackPressureMeasure);
	void CacheMemAllocator();
	REFERENCE_TIME GetStreamTime();	// returns 0 if stopped or error computing time

	BackPressureMode m_eBackPressureMode;
	unsigned m_uConsecutiveSamplesWithNoBackpressure;
	REFERENCE_TIME m_rtRenderingOffsetTarget;
	REFERENCE_TIME m_rtRenderingOffsetCurrent;
	CComPtr<IAlphaMixer> m_piAlphaMixer;
	IKsPropertySet *m_pRendererPropertySet;
	CComPtr<IStreamBufferPlayback> m_piStreamBufferPlayback;
	LiveVersusBufferedState m_eLiveVersusBufferedState;
	unsigned m_uLastBackPressure;
	double		m_dblRunningAvgLead;
	REFERENCE_TIME m_rtLastSampleStartTime;
	double		m_dblAvgSampleInterval;
	long		m_lBufferCount;
	double		m_dblAdjust;
	DWORD		m_dwSampleDropCountdown;
	CComQIPtr<IMemAllocator2> m_piMemAllocator2;
	unsigned m_uSamplesWhileBuilding;
	unsigned m_uConsecutiveSamplesToForceBuilding;
	unsigned m_uSamplesUntilCanIncrement;
	PVR_LIVE_TV_AUDIO_JITTER_CATEGORY m_eLiveTVJitterCategory;
	DWORD    m_dwLastOffsetFromLiveCheck;

#ifdef ENABLE_DIAGNOSTIC_OUTPUT
	DWORD		m_dwSamplesDelivered;
	DWORD		m_dwSamplesDropped;
	DWORD		m_dwSamplesError;
	DWORD		m_dwPotentiallyDry;
	LONGLONG	m_llLastDeliveryReport;
	LONGLONG	m_llCumulativeDrift;
	LONGLONG	m_llMaximumDrift;
	LONGLONG	m_llMinimumDrift;
	LONGLONG	m_llCumAdjDrift;
	LONGLONG	m_llNumDriftSamples;
	double	    m_uCumulativeBackPressure;
	DWORD	    m_dwMaxBackPressure;
	DWORD	    m_dwMinBackPressure;
	DWORD		m_dwPausedOrFlushing;
	DWORD		m_dwFrameStepSamples;
	DWORD		m_dwCritialSamples;

	void ReportLipSyncStats(const TCHAR *pszWhen);
	void AddLipSyncDataPoint(REFERENCE_TIME rtSampleStart, REFERENCE_TIME rtStream, unsigned uBackPressureMeasure);
#	endif
	HRESULT QueryRendererPropertySet(IUnknown *pUnknown, IKsPropertySet **piRendererPropertySet);

	// To support notification of glitches, with suppression of too many dups:
	CComPtr<IMediaEventSink> m_piMediaEventSink;		// only held while the graph is running
	DWORD m_dwDiscardEventTicks;
	DWORD m_dwBadTSEventTicks;
	DWORD m_dwNoSyncEventTicks;
	DWORD m_dwPoorSyncEventTicks;
	DWORD m_dwAudioReceiptEventTicks;

	friend class CAudioSyncOutputPin;
};


class CAudioSyncOutputPin : CTransInPlaceOutputPin
{
	// Construction
	CAudioSyncOutputPin(TCHAR *pObjectName,
						CTransInPlaceFilter *pFilter,
						HRESULT *phr,
						LPCWSTR pName);

	// Transform-pin override
	virtual HRESULT DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc);

	// Constructor access
	friend CBasePin* AudioSyncFilter::GetPin(int);

	// Offer the allocator:
	IMemAllocator *GetAllocator() { return m_pAllocator; }
	friend void AudioSyncFilter::CacheMemAllocator();
};

class CAudioSyncInputPin : CTransInPlaceInputPin
{
	// Construction
	CAudioSyncInputPin(TCHAR *pObjectName,
						CTransInPlaceFilter *pFilter,
						HRESULT *phr,
						LPCWSTR pName);

	// Constructor access
	friend CBasePin* AudioSyncFilter::GetPin(int);

	// Transform-pin override
	STDMETHODIMP BeginFlush(void);
	STDMETHODIMP EndFlush(void);

protected:
	LONG m_lInFlushCount;
};
