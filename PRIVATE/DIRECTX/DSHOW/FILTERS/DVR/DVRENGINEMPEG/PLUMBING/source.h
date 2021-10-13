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
// Source.h : media format-invariant source filter and pin infrastructure.
//

#pragma once

#include "DvrInterfaces_private.h"
#include "..\\writer\\DvrEngineHelpers.h"
#include "..\\PipelineInterfaces.h"

namespace MSDvr
{

class		CDVROutputPin;
interface	IPlaybackPipelineManager;
class		CSampleConsumer;


//////////////////////////////////////////////////////////////////////////////
//	Clock exposed by source filter.											//
//////////////////////////////////////////////////////////////////////////////

class CDVRClock : public CBaseReferenceClock
{
public:
	CDVRClock(LPUNKNOWN pUnk, HRESULT *phr);

	// Sets the external clock which drives this.  NULL will cause us to
	// use the system clock.   GetTruePrivateTime will keep the reported times
	// continuous across external clock changes via an internal offset value.
	void SetExternalClock(IReferenceClock *pClock);

	// Returns a rate adjusted version of GetTruePrivateTime.
	REFERENCE_TIME GetPrivateTime();

	// This function exposes the time backed by m_pExternalClock.  No rate
	// adjustment occurs at all.
	REFERENCE_TIME GetTruePrivateTime();

	// Resets our rate adjustment totals.  This should be called every time
	// what we're playing back is changed.
	void ResetRateAdjustment();

	// Queries and updates saved state about whether the clock slaving
	// is working as expected:

	bool SawClockGlitch() { return m_fHitClockGlitch; }
	void ClearClockGlitch() { m_fHitClockGlitch = false; }

	// Stores the a sample consumer pointer.  The clock uses the sample
	// consumer for finding out what the most recent available media time is.
	void SetSampleConsumer(ISampleConsumer *pSampleConsumer);

	// Returns iff the clock has a reference to the sample consumer
	bool HasSampleConsumer() { return m_pConsumer != NULL; }

private:
	// Sets the rate at which the clock times are returned relative to
	// the true clock in 10 thousandths.
	// A value of 10000 == 1x,  9570 == .957x, and so on.
	// When the rate is set, the current time does not change, only the rate
	// at which future times are returned in relation to the true clock.
	void SetClockRate(int iRate);

	// Resets rate related members back to default values
	void ResetMembers();

	// The 'true' clock that we're using to drive this one.  NULL means we use
	// the system clock.
	CComPtr<IReferenceClock> m_pExternalClock;

	// Offset to apply to all times returned from m_pExternalClock
	LONGLONG m_llOffsetFromCurrentClock;

	// Time returned by m_pExternalClock at the last rate change
	LONGLONG m_llTrueTimeAtLastRateChange;

	// Rate adjusted time at the last rate change
	LONGLONG m_llTimeAtLastRateChange;

	// Counter that tracks how many comparisons we've made
	int m_iCounter;

	// The average delta of the media sample times and m_pExternalClock's
	// time in milliSecs.  The actual value here doesn't matter.  This is what
	// we correct for to make sure it's not drifting over time.
	LONGLONG m_llLastMTDelta_mSec;

	// Accumulator for adding up the deltas during the comparison phase
	LONGLONG m_llMTDeltaAccumulator;

	// Rate that the clock is running at in 1/10000ths.  See SetClockRate
	// for more info.
	int m_iClockRate;

	// Pointer to the sample consumer - used for retrieving the latest media time
	// that we're aware of.
	ISampleConsumer *m_pConsumer;

	// Time our next drift measurement should occur.
	LONGLONG m_llNextCheck;

	// Stores the average PTS delta from the first set of comparisons.  Drift
	// is determined by comparing future data against this value
	LONGLONG m_llInitialPTSDelta;

	// Remembers whether something has happened that looks like a clock slaving
	// issue that would result in a playback glitch:
	bool m_fHitClockGlitch;
};

//////////////////////////////////////////////////////////////////////////////
//	Source filter.															//
//////////////////////////////////////////////////////////////////////////////

class CDVRSourceFilter :	public CBaseFilter,
							public IFileSourceFilter,
							public IStreamBufferMediaSeeking,
							public IStreamBufferPlayback,
							public CDVREngineHelpersImpl,
							public ISourceVOBUInfo
{
	// Private constructor for use by the creation function only
	CDVRSourceFilter(LPUNKNOWN piAggUnk, HRESULT *pHR);
	virtual ~CDVRSourceFilter();

public:
	DECLARE_IUNKNOWN;

#ifdef DEBUG
	STDMETHODIMP_(ULONG) NonDelegatingAddRef();
	STDMETHODIMP_(ULONG) NonDelegatingRelease();
	void AssertFilterLock() const
	{
		ASSERT (CritCheckIn(m_pLock));
	}
#endif // DEBUG


	// Creation function for the factory template
	static CUnknown* CreateInstance(LPUNKNOWN piAggUnk, HRESULT* phr);

	// Add a pin to the filter (for pipeline manager only)
	void AddPin(LPCWSTR wzPinName);

	// Filter name
	static const LPCTSTR		s_wzName;

	// Filter CLSID
	static const CLSID			s_clsid;

	// Filter registration data
	static const AMOVIESETUP_FILTER s_sudFilterReg;

	// Override
	virtual int GetPinCount();

	// Override
	virtual CBasePin* GetPin(int n);

	// Override
	STDMETHOD(NonDelegatingQueryInterface) (REFIID riid, LPVOID* ppv);

	// Override
	STDMETHOD(Run)(REFERENCE_TIME tStart);

	// Override
	STDMETHOD(Pause)();

	// Override
	STDMETHOD(Stop)();

	// Override
	STDMETHOD(JoinFilterGraph)(IFilterGraph *pGraph, LPCWSTR pName);

	// IFileSourceFilter
	STDMETHOD(Load)(LPCOLESTR pszFileName, const AM_MEDIA_TYPE* pmt);
	STDMETHOD(GetCurFile)(LPOLESTR* ppszFileName, AM_MEDIA_TYPE* pmt);

	// IStreamBufferMediaSeeking
	STDMETHOD(GetCapabilities)(DWORD *pCapabilities);
	STDMETHOD(CheckCapabilities)(DWORD *pCapabilities);
	STDMETHOD(IsFormatSupported)(const GUID *pFormat);
	STDMETHOD(QueryPreferredFormat)(GUID *pFormat);
	STDMETHOD(GetTimeFormat)(GUID *pFormat);
	STDMETHOD(IsUsingTimeFormat)(const GUID *pFormat);
	STDMETHOD(SetTimeFormat)(const GUID *pFormat);
	STDMETHOD(GetDuration)(LONGLONG *pDuration);
	STDMETHOD(GetStopPosition)(LONGLONG *pStop);
	STDMETHOD(GetCurrentPosition)(LONGLONG *pCurrent);
	STDMETHOD(ConvertTimeFormat)(	LONGLONG *pTarget,
									const GUID *pTargetFormat,
									LONGLONG Source,
									const GUID *pSourceFormat);
	STDMETHOD(SetPositions)(LONGLONG *pCurrent,
							DWORD dwCurrentFlags,
							LONGLONG *pStop,
							DWORD dwStopFlags);
	STDMETHOD(GetPositions)(LONGLONG *pCurrent, LONGLONG *pStop);
	STDMETHOD(GetAvailable)(LONGLONG *pEarliest, LONGLONG *pLatest);
	STDMETHOD(SetRate)(double dRate);
	STDMETHOD(GetRate)(double *pdRate);
	STDMETHOD(GetPreroll)(LONGLONG *pllPreroll);

	// IStreamBufferPlayback:
	STDMETHOD(SetTunePolicy)(STRMBUF_PLAYBACK_TUNE_POLICY eStrmbufPlaybackTunePolicy);
	STDMETHOD(GetTunePolicy)(STRMBUF_PLAYBACK_TUNE_POLICY *peStrmbufPlaybackTunePolicy);
	STDMETHOD(SetThrottlingEnabled)(BOOL fThrottle);
	STDMETHOD(GetThrottlingEnabled)(BOOL *pfThrottle);
	STDMETHOD(NotifyGraphIsConnected)();
	STDMETHOD(IsAtLive)(BOOL *pfAtLive);
	STDMETHOD(GetOffsetFromLive)(LONGLONG *pllOffsetFromLive);
	STDMETHOD(SetDesiredOffsetFromLive)(LONGLONG llOffsetFromLive);
	STDMETHOD(GetLoadIncarnation)(DWORD *pdwLoadIncarnation);
	STDMETHOD(EnableBackgroundPriority)(BOOL fEnableBackgroundPriority);

	// ISourceVOBUInfo
	STDMETHOD(GetVOBUOffsets)(	LONGLONG tPresentation,
								LONGLONG tSearchStartBound,
								LONGLONG tSearchEndBound,
								VOBU_SRI_TABLE *pVobuTable);
	STDMETHOD(GetRecordingSize)(LONGLONG tStart,
								LONGLONG tEnd,
								ULONGLONG* pcbSize);
	STDMETHOD(GetRecordingDurationMax)( LONGLONG tStart,
								        ULONGLONG cbSizeMax,
								        LONGLONG* ptDurationMax);
	STDMETHOD(GetKeyFrameTime)(	LONGLONG tTime,
								LONG nSkipCount,
								LONGLONG* ptKeyFrame);
	STDMETHOD(GetRecordingData)(	LONGLONG tStart,
									BYTE* rgbBuffer,
									ULONG cbBuffer,
									ULONG* pcbRead,
									LONGLONG* ptEnd);

	// Gets the time from the graph clock
	HRESULT GetTime(REFERENCE_TIME *pRT);

	// So that the pipeline components can do computations based on
	// clock vs stream time:
	REFERENCE_TIME GetStreamBase() { return m_tStart; }

	// The decoder driver needs to get the lock for its app thread:
	CCritSec *GetAppLock() { return m_pLock; }

	// The decoder driver also wants to get the clock in order to detect
	// and work around the unavoidable a/v glitches during very high stress:
	CDVRClock &GetDVRClock() { return m_cClock; }

private:
	CCritSec					m_cStateChangeLock;
	std::vector<CDVROutputPin*>	m_rgPins;
	IPlaybackPipelineManager*	m_piPipelineManager;
	CDVRClock					m_cClock;

	friend class CDVROutputPin;
	CDVRSourceFilter(const CDVRSourceFilter&);					// not implemented
	CDVRSourceFilter& operator = (const CDVRSourceFilter&);		// not implemented

	// Function to retrieve an interface implemented by a pipeline component.
	// Iff no component implements riid, then NULL is returned.
	template<class T> CComPtr<T> GetComponentInterface(REFIID riid = __uuidof(T));

	// Function to find an interface on some filter in the graph.  The first
	// filter, OTHER than this one, which implements T is returned.  If the
	// interface can't be found, E_NOINTERFACE is returned.
	template<class T> HRESULT FindFilterInterface(	T **ppInterface,
													REFIID riid = __uuidof(T));

	// Returns a pointer to the clock which should be used by m_cClock for
	// generating the stream time.  This decision is based on to what this filter
	// is currently bound.  Returns NULL if no preferred clock was found or on
	// an error.  The returned pointer has already been AddRef'ed and must be
	// released.
	HRESULT GetPreferredClock(IReferenceClock** ppIReferenceClock);
};

//////////////////////////////////////////////////////////////////////////////
//	Output pin for source filter.											//
//////////////////////////////////////////////////////////////////////////////

class CDVROutputPin :		public CBaseOutputPin
{
	// Private constructor for use by the filter only
	CDVROutputPin(CDVRSourceFilter& rcFilter, CCritSec& rcLock, LPCWSTR wzPinName, DWORD dwPinNum);

public:

	// Owner filter access
	CDVRSourceFilter& GetFilter();

	// Override
	virtual HRESULT CheckMediaType(const CMediaType* pmt);

	// Override
	virtual HRESULT DecideBufferSize(IMemAllocator* pAlloc,
									 ALLOCATOR_PROPERTIES* ppropInputRequest);

	// Override
	virtual HRESULT GetMediaType(int iPosition, CMediaType* pMediaType);

private:
	HRESULT						m_hrConstruction;
	DWORD						m_dwPinNum;

	friend class CDVRSourceFilter;
	CDVROutputPin(const CDVROutputPin&);						// not implemented
	CDVROutputPin& operator = (const CDVROutputPin&);			// not implemented
};

}
