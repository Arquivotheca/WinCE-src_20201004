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
#include "SampleProducer.h"
#include "..\\PauseBuffer.h"
#include "..\\Plumbing\\Sink.h"
#include "..\\Plumbing\\PipelineManagers.h"
#include "ProducerConsumerUtilities.h"
#include "..\\HResultError.h"

// To enable Win/CE kernel tracker events that help sort out A/V
// glitches, uncomment the following define -- or otherwise define
// it for all MPEG DVR Engine sources.

#ifdef _WIN32_WCE
// #define WIN_CE_KERNEL_TRACKING_EVENTS
#endif // __WIN32_WCE

#ifdef WIN_CE_KERNEL_TRACKING_EVENTS
#include "celog.h"
#endif // WIN_CE_KERNEL_TRACKING_EVENTS

using namespace MSDvr;

inline size_t OLESTRLEN(LPCOLESTR pszOleStr)
{
	return wcslen(pszOleStr);
}

inline int OLESTRCMP(LPCOLESTR pszOleStr1, LPCOLESTR pszOleStr2)
{
	return wcscmp(pszOleStr1,pszOleStr2);
}

const LONGLONG CSampleProducer::s_hy100NsPerSecond = 10000000;
//const LONGLONG CSampleProducer::s_hyOverlapIn100NsUnits = 20000000; // TODO:  tune this!
const LONGLONG CSampleProducer::s_hyOverlapIn100NsUnits = 5000000; // TODO:  tune this!
const long CSampleProducer::s_lMinimumBufferQuota = 2;
const LONGLONG CSampleProducer::s_hySleepTimeAs100Ns = 500000;

#ifdef DEBUG
unsigned long CSampleProducer::s_uSinkIDCounter = 0;
#endif

// #define GATHER_STATS
#ifdef GATHER_STATS
static unsigned long gSyncPoints = 0;
static unsigned long gSamples = 0;
#endif

#ifdef DEBUG
static LONGLONG g_hyMinimumTimeSpanInQueue = 30000000; // typically about a 3 second requirement
#endif // DEBUG


///////////////////////////////////////////////////////////////////////
//
// Helper class CSampleConsumerState:
//
///////////////////////////////////////////////////////////////////////

CSampleConsumerState::CSampleConsumerState(void)
	: m_piSampleConsumer(0)
	, m_eSampleProducerMode(PRODUCER_SUPPRESS_SAMPLES)
	, m_fHasMediaType(false)
	, m_uModeVersionCount(0)
{
} // CSampleConsumerState::CSampleConsumerState

CSampleConsumerState::CSampleConsumerState(const CSampleConsumerState &rcSampleConsumerState)
	: m_piSampleConsumer(rcSampleConsumerState.m_piSampleConsumer)
	, m_eSampleProducerMode(rcSampleConsumerState.m_eSampleProducerMode)
	, m_fHasMediaType(rcSampleConsumerState.m_fHasMediaType)
	, m_uModeVersionCount(rcSampleConsumerState.m_uModeVersionCount)
{
} // CSampleConsumerState::CSampleConsumerState

CSampleConsumerState &CSampleConsumerState::operator =(const CSampleConsumerState &rcSampleConsumerState)
{
	if (this != &rcSampleConsumerState)
	{
		m_piSampleConsumer = rcSampleConsumerState.m_piSampleConsumer;
		m_eSampleProducerMode = rcSampleConsumerState.m_eSampleProducerMode;
		m_fHasMediaType = rcSampleConsumerState.m_fHasMediaType;
		m_uModeVersionCount = rcSampleConsumerState.m_uModeVersionCount;
	}
	return *this;
} // CSampleConsumerState::operator =

CSampleConsumerState::~CSampleConsumerState(void)
{
} // CSampleConsumerState::~CSampleConsumerState

///////////////////////////////////////////////////////////////////////
//
// Helper class CProducerSample:
//
///////////////////////////////////////////////////////////////////////

LONGLONG CProducerSample::s_hyNextMediaSampleID = 0;

CProducerSample::CProducerSample(UCHAR bPinIndex, LONGLONG hyMediaStartTime,
								 LONGLONG hyMediaEndTime, IMediaSample &riMediaSample)
	: m_bPinIndex(bPinIndex)
	, m_hyMediaStartTime(hyMediaStartTime)
	, m_hyMediaEndTime(hyMediaEndTime)
	, m_hyMediaSampleID(++s_hyNextMediaSampleID)
	, m_piMediaSample(&riMediaSample)
{
} // CProducerSample::CProducerSample

CProducerSample::~CProducerSample()
{
} // CProducerSample::~CProducerSample

CProducerSample &CProducerSample::operator = (const CProducerSample &rcProducerSample)
{
	if (this != &rcProducerSample)
	{
		m_bPinIndex = rcProducerSample.m_bPinIndex;
		m_hyMediaStartTime= rcProducerSample.m_hyMediaStartTime;
		m_hyMediaEndTime= rcProducerSample.m_hyMediaEndTime;
		m_piMediaSample = rcProducerSample.m_piMediaSample;
		m_hyMediaSampleID = rcProducerSample.m_hyMediaSampleID;
	}
	return *this;
} // CProducerSample::operator =

CProducerSample::CProducerSample(const CProducerSample &rcProducerSample)
	: m_bPinIndex(rcProducerSample.m_bPinIndex)
	, m_hyMediaStartTime(rcProducerSample.m_hyMediaStartTime)
	, m_hyMediaEndTime(rcProducerSample.m_hyMediaEndTime)
	, m_piMediaSample(rcProducerSample.m_piMediaSample)
	, m_hyMediaSampleID(rcProducerSample.m_hyMediaSampleID)
{
} // CProducerSample::CProducerSample

///////////////////////////////////////////////////////////////////////
//
// Helper class CSampleProducerPinRecord:
//
///////////////////////////////////////////////////////////////////////

CSampleProducerPinRecord::CSampleProducerPinRecord(CPinMappings &rcPinMapping, UCHAR bPinIndex)
	: m_rsPinState(rcPinMapping.GetPinState(bPinIndex))
	, m_uMaxCachedSamples(0)
	, m_uCurCachedSamples(0)
{
} // CSampleProducerPinRecord::CSampleProducerPinRecord

CSampleProducerPinRecord &CSampleProducerPinRecord::operator =(const CSampleProducerPinRecord &rcSampleProducerPinRecord)
{
	if (this != &rcSampleProducerPinRecord)
	{
		m_rsPinState = rcSampleProducerPinRecord.m_rsPinState;
		m_uMaxCachedSamples = rcSampleProducerPinRecord.m_uMaxCachedSamples;
		m_uCurCachedSamples = rcSampleProducerPinRecord.m_uCurCachedSamples;

	}
	return *this;
} // CSampleProducerPinRecord::operator =

CSampleProducerPinRecord::CSampleProducerPinRecord(const CSampleProducerPinRecord &rcSampleProducerPinRecord)
	: m_rsPinState(rcSampleProducerPinRecord.m_rsPinState)
	, m_uMaxCachedSamples(rcSampleProducerPinRecord.m_uMaxCachedSamples)
	, m_uCurCachedSamples(rcSampleProducerPinRecord.m_uCurCachedSamples)
{
} // CSampleProducerPinRecord::CSampleProducerPinRecord

CSampleProducerPinRecord::~CSampleProducerPinRecord(void)
{
} // CSampleProducerPinRecord::~CSampleProducerPinRecord

///////////////////////////////////////////////////////////////////////
//
// Helper class CSampleCache
//
///////////////////////////////////////////////////////////////////////

CSampleCache::CSampleCache()
	: m_pcProducerSample(NULL)
	, m_uIdxOldest(0)
	, m_uIdxToFill(0)
	, m_uMaxSamples(0)
{
}

CSampleCache::~CSampleCache()
{
	if (m_pcProducerSample)
	{
		delete[] m_pcProducerSample;
		m_pcProducerSample = NULL;
	}
}

void CSampleCache::Init(VectorCSampleProducerPinRecord &rvectorCSampleProducerPinRecord)
{
	if (m_pcProducerSample)
	{
		delete[] m_pcProducerSample;
		m_pcProducerSample = NULL;
	}
	m_uIdxOldest = 0;
	m_uIdxToFill = 0;
	m_uMaxSamples = 0;

	unsigned uTotalSamples = 0;
	VectorCSampleProducerPinRecordIterator pinIterator;
	for (pinIterator = rvectorCSampleProducerPinRecord.begin();
		 pinIterator != rvectorCSampleProducerPinRecord.end();
		 ++pinIterator)
	{
		uTotalSamples += pinIterator->m_uMaxCachedSamples;
	}
	if (uTotalSamples > 0)
	{
		m_pcProducerSample = new CProducerSample[uTotalSamples + 1];
		m_uMaxSamples = uTotalSamples + 1;
	}
}

void CSampleCache::Append(CProducerSample &rcProducerSample)
{
	if ((m_uMaxSamples == 0) || (Increment(m_uIdxToFill) == m_uIdxOldest))
		throw CHResultError(E_FAIL);

	m_pcProducerSample[m_uIdxToFill] = rcProducerSample;
	m_uIdxToFill = Increment(m_uIdxToFill);
}

void CSampleCache::Clear()
{
	if (m_pcProducerSample)
	{
		unsigned uIdx;

		for (uIdx = 0; uIdx < m_uMaxSamples; ++uIdx)
		{
			m_pcProducerSample[uIdx].Clear();
		}
	}
	m_uIdxOldest = 0;
	m_uIdxToFill = 0;
}

LONGLONG CSampleCache::GetOldestSampleStart()
{
	if (m_uIdxOldest == m_uIdxToFill)
		return 0;
	return m_pcProducerSample[m_uIdxOldest].m_hyMediaStartTime;
}

void CSampleCache::PruneTo(unsigned uFirstKeep)
{
	ASSERT(m_pcProducerSample && (uFirstKeep < m_uMaxSamples));

	unsigned uIdx = m_uIdxOldest;

	while (uIdx != uFirstKeep)
	{
		m_pcProducerSample[uIdx].Clear();
		uIdx = Increment(uIdx);
	}
	m_uIdxOldest = uFirstKeep;
}

CProducerSample &CSampleCache::Find(LONGLONG hySampleID)
{
	if (m_uIdxOldest == m_uIdxToFill)
	{
		DbgLog((LOG_ERROR, 2, _T("Sample Producer -- about to throw an exception that will be caught to speed an unusual but not error case\n")));
		throw CHResultError(E_INVALIDARG);
	}

	LONGLONG hyLowestID = m_pcProducerSample[m_uIdxOldest].m_hyMediaSampleID;
	if ((hySampleID < hyLowestID) || (hySampleID >= hyLowestID + m_uMaxSamples))
	{
		DbgLog((LOG_ERROR, 2, _T("Sample Producer -- about to throw an exception that will be caught to speed an unusual but not error case\n")));
		throw CHResultError(E_INVALIDARG);
	}

	unsigned uExpectedIdx = (unsigned) (m_uIdxOldest + (hySampleID - hyLowestID));
	if (uExpectedIdx >= m_uMaxSamples)
		uExpectedIdx -= m_uMaxSamples;
	if (m_pcProducerSample[uExpectedIdx].m_hyMediaSampleID != hySampleID)
		throw CHResultError(E_FAIL);
	return m_pcProducerSample[uExpectedIdx];
}

///////////////////////////////////////////////////////////////////////
//
// Helper class CSampleProducerSampleHistory
//
///////////////////////////////////////////////////////////////////////

CSampleProducerSampleHistory::CSampleProducerSampleHistory(CSampleProducer &rcSampleProducer)
	: m_rcSampleProducer(rcSampleProducer)
	, m_bPinsInFlush(0)
	, m_bPrimaryPinIndex(255)
	, m_fFlushEnded(false)
	, m_fNewSegmentStarted(false)
	, m_hyMinPosition(-1)
	, m_hyMaxPosition(-1)
	, m_hyLastRawMediaStart(0)
	, m_hyLastDeltaMediaStart(0)
	, m_vcsppr()
	, m_hyClockOffset(0)
	, m_fSawSample(false)
	, m_cSampleCache()
{
}

CSampleProducerSampleHistory::~CSampleProducerSampleHistory()
{
	Clear();
}

void CSampleProducerSampleHistory::Init(CPinMappings &rcPinMappings)
{
	Clear();
	m_bPrimaryPinIndex = rcPinMappings.GetPrimaryPin();

	UCHAR bNumPins = rcPinMappings.GetPinCount();
	for (UCHAR i = 0; i < bNumPins; ++i)
	{
		CSampleProducerPinRecord pinRec(rcPinMappings, i);
		IPin &riPin = rcPinMappings.GetPin(i);

		ASSERT(pinRec.m_rsPinState.pcMediaTypeDescription);
		pinRec.m_uCurCachedSamples = 0;
		ULONG ignored;
		m_rcSampleProducer.ComputeBufferQuota(&pinRec.m_rsPinState.pcMediaTypeDescription->m_cMediaType,
				pinRec.m_uMaxCachedSamples, ignored);
		m_vcsppr.push_back(pinRec);
	}
	m_cSampleCache.Init(m_vcsppr);
}

void CSampleProducerSampleHistory::Clear()
{
	m_bPinsInFlush = 0;
	m_bPrimaryPinIndex = 255;  // big value -- should trigger assert if no Init()
	m_fFlushEnded = false;
	m_fNewSegmentStarted = false;
	m_hyMinPosition = -1;
	m_hyMaxPosition = -1;
	m_cSampleCache.Clear();
	m_vcsppr.clear();
}

ROUTE CSampleProducerSampleHistory::OnNewSample(UCHAR bPinIndex,
												IMediaSample &riMediaSample)
{
	// ASSUMPTION:  caller holds m_cCritSecStream exactly one!!!

	if (0 != m_bPinsInFlush)
		return HANDLED_STOP;
	bool fIsTune = false;

	if (m_fFlushEnded)
	{
		m_fFlushEnded = false;
		if (m_rcSampleProducer.IsPostFlushSampleAfterTune(bPinIndex, riMediaSample))
		{
			// This is a tune event!
			fIsTune = true;
		}
	}
	if (m_fNewSegmentStarted)
	{
		m_fNewSegmentStarted = false;
		if (m_rcSampleProducer.IsNewSegmentSampleAfterTune(bPinIndex, riMediaSample))
		{
			// This is a tune event!
			fIsTune = true;
		}
	}


	LONGLONG sampleStart;
	BYTE *pbSampleData;
	long lDataLeft;
	HRESULT hr = riMediaSample.GetPointer(&pbSampleData);
	if (SUCCEEDED(hr))
		lDataLeft = riMediaSample.GetActualDataLength();
	if (FAILED(hr))
		return HANDLED_STOP;

	LONGLONG hyOrigMaxPosition = m_hyMaxPosition;
	m_rcSampleProducer.ExtractMediaTime(bPinIndex, riMediaSample, sampleStart, pbSampleData, lDataLeft);
#ifdef WIN_CE_KERNEL_TRACKING_EVENTS
	{
		if (fIsTune)
		{
			wchar_t pwszMsg[256];
			swprintf(pwszMsg, L"CSampleProducerSampleHistory::OnNewSample() ... TUNE at %I64d (%I64d + %I64d)\n",
				sampleStart + m_hyClockOffset, sampleStart, m_hyClockOffset);
			CELOGDATA(TRUE,
				CELID_RAW_WCHAR,
				(PVOID)pwszMsg,
				(1 + wcslen(pwszMsg)) * sizeof(wchar_t),
				1,
				CELZONE_MISC);
			}
	}
#endif // WIN_CE_KERNEL_TRACKING_EVENTS

	LONGLONG hyMediaSampleID = -1;
	LONGLONG hyAdjustedMediaStart;

	m_hyLastDeltaMediaStart = sampleStart - m_hyLastRawMediaStart;
	m_hyLastRawMediaStart = sampleStart;
	hyAdjustedMediaStart = sampleStart + m_hyClockOffset;

	m_rcSampleProducer.SetMediaTime(bPinIndex, riMediaSample,
			hyAdjustedMediaStart, hyAdjustedMediaStart, pbSampleData, lDataLeft);
	if (m_hyMaxPosition < hyAdjustedMediaStart)
	{
		m_hyMaxPosition = hyAdjustedMediaStart;
	}
	if (m_hyMinPosition < 0)
		m_hyMinPosition = m_hyMaxPosition;
	sampleStart = hyAdjustedMediaStart;

	if (m_rcSampleProducer.m_piWriter)
	{
		LONGLONG hyWriterPos = m_rcSampleProducer.m_piWriter->GetLatestSampleTime();
		if (hyWriterPos >= 0)
		{
			static LONGLONG hyHighWaterMark = 0;

			ASSERT(hyWriterPos < sampleStart);
			LONGLONG delta = (sampleStart - hyWriterPos);
			if (delta > hyHighWaterMark)
			{
				hyHighWaterMark = delta;
				if (delta > 2000000)
				{
					wchar_t pwszMsg[128];
					wsprintf(pwszMsg, L"\n**** DVR ENGINE BUFFERING INFO: PRODUCER/WRITER GAP = %I64d 100ns units\n\n",
						delta);
					OutputDebugStringW(pwszMsg);
				}
			}
		}
	}

	bool fCacheForConsumer;
	ROUTE eRouteForWriter;
	m_rcSampleProducer.GetSampleDeliveryPolicy(bPinIndex, riMediaSample,
			fCacheForConsumer, eRouteForWriter);

	if (fCacheForConsumer)
	{
		CSampleProducerPinRecord &rcSampleProducerPinRecord = m_vcsppr[bPinIndex];
		if (rcSampleProducerPinRecord.m_uMaxCachedSamples ==
				rcSampleProducerPinRecord.m_uCurCachedSamples)
		{
			DbgLog((LOG_SINK_DISPATCH, 4, _T("CSampleProducer:  pruning sample %I64d 100ns units old\n"),
					hyAdjustedMediaStart - m_cSampleCache.GetOldestSampleStart()));
#ifdef DEBUG
			ASSERT(hyAdjustedMediaStart - m_cSampleCache.GetOldestSampleStart() >= g_hyMinimumTimeSpanInQueue);
#endif
			Prune(bPinIndex);
		}
		CProducerSample cProducerMediaSample(bPinIndex, sampleStart, sampleStart, riMediaSample);
		m_cSampleCache.Append(cProducerMediaSample);
		++rcSampleProducerPinRecord.m_uCurCachedSamples;
		hyMediaSampleID = cProducerMediaSample.m_hyMediaSampleID;
	}

	if (bPinIndex == m_bPrimaryPinIndex)
	{
		m_rcSampleProducer.UpdateLatestTime(sampleStart);
	}
	m_fSawSample = true;

	if ((bPinIndex == m_bPrimaryPinIndex) &&
		m_rcSampleProducer.m_fRecordingRequested &&
		(riMediaSample.IsSyncPoint() == S_OK))
	{
		m_rcSampleProducer.m_fRecordingRequested = false;
		{
			CAutoUnlock cAutoUnlock(m_rcSampleProducer.m_cCritSecStream);
			m_rcSampleProducer.NotifyConsumersOfRecordingStart(hyOrigMaxPosition, sampleStart);
		}
	}
	{
		CAutoUnlock cAutoUnlock(m_rcSampleProducer.m_cCritSecStream);
		if (fIsTune)
			m_rcSampleProducer.NotifyConsumersOfTune(sampleStart);

		if (fCacheForConsumer)
			m_rcSampleProducer.NotifyConsumersOfSample(hyMediaSampleID, sampleStart, sampleStart, bPinIndex);
	}
	return eRouteForWriter;
}

void CSampleProducerSampleHistory::OnBeginFlush(UCHAR bPinIndex)
{
#ifdef WIN_CE_KERNEL_TRACKING_EVENTS
	{
		wchar_t pwszMsg[256] = L"CSampleProducerSampleHistory::OnBeginFlush()\n";
		CELOGDATA(TRUE,
			CELID_RAW_WCHAR,
			(PVOID)pwszMsg,
			(1 + wcslen(pwszMsg)) * sizeof(wchar_t),
			1,
			CELZONE_MISC);
	}
#endif // WIN_CE_KERNEL_TRACKING_EVENTS
	CSampleProducerPinRecord &rcSampleProducerPinRecord = m_vcsppr[bPinIndex];
	if (rcSampleProducerPinRecord.m_rsPinState.fPinFlushing)
		return;

	rcSampleProducerPinRecord.m_rsPinState.fPinFlushing = true;

	if (1 == ++m_bPinsInFlush)
	{
		m_fFlushEnded = false;
		m_cSampleCache.Clear();

		VectorCSampleProducerPinRecordIterator iterator;
		for (iterator = m_vcsppr.begin(); iterator != m_vcsppr.end(); ++iterator)
			iterator->m_uCurCachedSamples = 0;

		m_hyMinPosition = -1;
		m_rcSampleProducer.NotifyConsumerOfCachePrune(-1);
	}
}

void CSampleProducerSampleHistory::OnEndFlush(UCHAR bPinIndex)
{
#ifdef WIN_CE_KERNEL_TRACKING_EVENTS
	{
		wchar_t pwszMsg[256] = L"CSampleProducerSampleHistory::OnEndFlush()\n";
		CELOGDATA(TRUE,
			CELID_RAW_WCHAR,
			(PVOID)pwszMsg,
			(1 + wcslen(pwszMsg)) * sizeof(wchar_t),
			1,
			CELZONE_MISC);
	}
#endif // WIN_CE_KERNEL_TRACKING_EVENTS
	CSampleProducerPinRecord &rcSampleProducerPinRecord = m_vcsppr[bPinIndex];
	if (!rcSampleProducerPinRecord.m_rsPinState.fPinFlushing)
		return;
	rcSampleProducerPinRecord.m_rsPinState.fPinFlushing = false;

	if (0 == --m_bPinsInFlush)
	{
		m_fFlushEnded = true;
	}
}

void CSampleProducerSampleHistory::OnNewSegment(UCHAR bPinIndex,
												REFERENCE_TIME rtStart, REFERENCE_TIME rtEnd,
												double dblRate)
{
	if (-1 != m_hyMaxPosition)
		m_fNewSegmentStarted = true;
} // CSampleProducerSampleHistory::OnNewSegment

void CSampleProducerSampleHistory::Prune(UCHAR bPinIndex)
{
	CSampleCacheFwdIterator iterator(m_cSampleCache);
	enum PRUNE_PHASE { SEEKING_PIN_SAMPLE, SEEKING_NEW_TIMESTAMP, SEEKING_KEYFRAME, PRUNE_COMPLETE }
		ePrunePhase = SEEKING_PIN_SAMPLE;
	LONGLONG hyDiscardedTime = -1;
	LONGLONG hyPruneEnd = -1;
	unsigned uNumSamplesPruned = 0;
	unsigned uNumMaxPruningDesired = m_cSampleCache.GetMax() / 3;

	m_hyMinPosition = -1;
	bool fNeedPrune = false;
	while ((ePrunePhase != PRUNE_COMPLETE) && iterator.HasMore())
	{
		CProducerSample &rcProducerSample = iterator.Current();

		switch (ePrunePhase)
		{
		case SEEKING_PIN_SAMPLE:
			if (rcProducerSample.m_bPinIndex == bPinIndex)
			{
				ePrunePhase = SEEKING_NEW_TIMESTAMP;
				hyDiscardedTime = rcProducerSample.m_hyMediaStartTime;
			}
			--m_vcsppr[rcProducerSample.m_bPinIndex].m_uCurCachedSamples;
			fNeedPrune = true;
			++uNumSamplesPruned;
			break;

		case SEEKING_NEW_TIMESTAMP:
			if (rcProducerSample.m_hyMediaStartTime != hyDiscardedTime)
				ePrunePhase = SEEKING_KEYFRAME;
				// FALL THROUGH INTO NEXT CASE -- to decide if this is a key frame
			else
			{
				--m_vcsppr[rcProducerSample.m_bPinIndex].m_uCurCachedSamples;
				fNeedPrune = true;
				++uNumSamplesPruned;
				break;
			}
		case SEEKING_KEYFRAME:
			if (((rcProducerSample.m_bPinIndex == m_bPrimaryPinIndex) &&
				 (rcProducerSample.m_piMediaSample->IsSyncPoint() == S_OK)) ||
			    (uNumSamplesPruned >= uNumMaxPruningDesired))
			{
				ePrunePhase = PRUNE_COMPLETE;
				m_hyMinPosition = rcProducerSample.m_hyMediaStartTime;
				hyPruneEnd = rcProducerSample.m_hyMediaSampleID;
				m_cSampleCache.PruneTo(iterator.GetPosition());
				fNeedPrune = false;
			}
			else
			{
				--m_vcsppr[rcProducerSample.m_bPinIndex].m_uCurCachedSamples;
				fNeedPrune = true;
				++uNumSamplesPruned;
			}
			break;
		case PRUNE_COMPLETE:
			ASSERT(0);
			break;
		}
		iterator.Next();
	}
	if (fNeedPrune)
		m_cSampleCache.Clear();
	m_rcSampleProducer.NotifyConsumerOfCachePrune(hyPruneEnd);
}

///////////////////////////////////////////////////////////////////////
//
// Helper class CSampleConsumerBindings
//
///////////////////////////////////////////////////////////////////////

CSampleConsumerBindings::CSampleConsumerBindings()
	: m_listCSampleConsumerState()
	, m_lNumListIterators(0)
	, m_lNumPendingUnbinds(0)
{
} // CSampleConsumerBindings::CSampleConsumerBindings

CSampleConsumerBindings::~CSampleConsumerBindings()
{
} // CSampleConsumerBindings::~CSampleConsumerBindings

CSampleConsumerState &CSampleConsumerBindings::GetState(ISampleConsumer *piSampleConsumer)
{
	CSampleConsumerBindingIterator iterator(*this);

	while (iterator.MoveToNext())
	{
		CSampleConsumerState &rcSampleConsumerState = iterator.GetCurrent();
		if (rcSampleConsumerState.m_piSampleConsumer == piSampleConsumer)
			return rcSampleConsumerState;
	}

	DbgLog((LOG_ERROR, 2, _T("CSampleConsumerBindings::GetState(%p) failed, throwing E_FAIL\n"),
			piSampleConsumer));
	throw CHResultError(E_FAIL);
} // CSampleConsumerBindings::GetState

bool CSampleConsumerBindings::IsBound(ISampleConsumer *piSampleConsumer)
{
	if (!piSampleConsumer)
		return false;

	CSampleConsumerBindingIterator iterator(*this);

	while (iterator.MoveToNext())
	{
		CSampleConsumerState &rcSampleConsumerState = iterator.GetCurrent();
		if (rcSampleConsumerState.m_piSampleConsumer == piSampleConsumer)
			return true;
	}
	return false;
} // CSampleConsumerBindings::IsBound

void CSampleConsumerBindings::Add(CSampleConsumerState &rcSampleConsumerState)
{
	if (!IsBound(rcSampleConsumerState.m_piSampleConsumer))
		m_listCSampleConsumerState.push_back(rcSampleConsumerState);
} // CSampleConsumerBindings::Add

bool CSampleConsumerBindings::Unbind(ISampleConsumer *piSampleConsumer)
{
	if (!piSampleConsumer)
		return false;

	CSampleConsumerBindingIterator iterator(*this);

	while (iterator.MoveToNext())
	{
		CSampleConsumerState &rcSampleConsumerState = iterator.GetCurrent();
		if (rcSampleConsumerState.m_piSampleConsumer == piSampleConsumer)
		{
			rcSampleConsumerState.m_piSampleConsumer = NULL;
			++m_lNumPendingUnbinds;
			return true;
		}
	}
	return false;
} // CSampleConsumerBindings::Unbind

void CSampleConsumerBindings::Clear()
{
	m_listCSampleConsumerState.clear();
	ASSERT(m_lNumListIterators == 0);
}

///////////////////////////////////////////////////////////////////////
//
// Helper class CSampleConsumerBindingIterator
//
///////////////////////////////////////////////////////////////////////

CSampleConsumerBindingIterator::CSampleConsumerBindingIterator(CSampleConsumerBindings &rcSampleConsumerBindings)
	: m_rcSampleConsumerBindings(rcSampleConsumerBindings)
	, m_iteratorCSampleConsumerState()
	, m_fNewlyInited(true)
{
	++rcSampleConsumerBindings.m_lNumListIterators;
} // CSampleConsumerBindingIterator::CSampleConsumerBindingIterator

CSampleConsumerBindingIterator::~CSampleConsumerBindingIterator()
{
	if ((0 == --m_rcSampleConsumerBindings.m_lNumListIterators) &&
		(0 < m_rcSampleConsumerBindings.m_lNumPendingUnbinds))
	{
		m_rcSampleConsumerBindings.m_lNumPendingUnbinds = 0;
		std::list<CSampleConsumerState>::iterator iteratorCSampleConsumerState;
	
		for (iteratorCSampleConsumerState = m_rcSampleConsumerBindings.m_listCSampleConsumerState.begin();
			iteratorCSampleConsumerState != m_rcSampleConsumerBindings.m_listCSampleConsumerState.end();
			iteratorCSampleConsumerState)
		{
			CSampleConsumerState &rcSampleConsumerState = *iteratorCSampleConsumerState;

			if (!rcSampleConsumerState.m_piSampleConsumer)
				iteratorCSampleConsumerState = m_rcSampleConsumerBindings.m_listCSampleConsumerState.erase(iteratorCSampleConsumerState);
			else
				++iteratorCSampleConsumerState;
		}
	}
} // CSampleConsumerBindingIterator::~CSampleConsumerBindingIterator

CSampleConsumerState &CSampleConsumerBindingIterator::GetCurrent()
{
	while (m_iteratorCSampleConsumerState != m_rcSampleConsumerBindings.m_listCSampleConsumerState.end())
	{
		if (m_iteratorCSampleConsumerState->m_piSampleConsumer)
			return *m_iteratorCSampleConsumerState;
		++m_iteratorCSampleConsumerState;
	}
	DbgLog((LOG_ERROR, 2, _T("CSampleConsumerBindingIterator::GetCurrent() failed, throwing E_FAIL\n"),
			E_FAIL));
	throw CHResultError(E_FAIL);
} // CSampleConsumerBindingIterator::GetCurrent

bool CSampleConsumerBindingIterator::MoveToNext()
{
	if (m_fNewlyInited)
	{
		m_iteratorCSampleConsumerState = m_rcSampleConsumerBindings.m_listCSampleConsumerState.begin();
		m_fNewlyInited = false;
	}
	else if (m_iteratorCSampleConsumerState != m_rcSampleConsumerBindings.m_listCSampleConsumerState.end())
		++m_iteratorCSampleConsumerState;

	while (m_iteratorCSampleConsumerState != m_rcSampleConsumerBindings.m_listCSampleConsumerState.end())
	{
		if (m_iteratorCSampleConsumerState->m_piSampleConsumer)
			return true;
		++m_iteratorCSampleConsumerState;
	}
	return false;
} // CSampleConsumerBindingIterator::MoveToNext

///////////////////////////////////////////////////////////////////////
//
// Class CSampleProducer:   constructor and destructor
//
///////////////////////////////////////////////////////////////////////

CSampleProducer::CSampleProducer(void)
	: m_cSampleConsumerBindings()
	, m_cSampleProducerSampleHistory(*this)
	, m_pcPauseBufferData(0)
	, m_pifsinkf2Next(0)
	, m_pisbcNext(0)
	, m_pisbcFilter(0)
	, m_eFilterState(State_Stopped)
	, m_picpmgr(0)
	, m_pipbmgr(0)
	, m_pifsinkf2Writer(0)
	, m_pcPinMappings(0)
	, m_piWriter(0)
	, m_cCritSecStream()
	, m_pwszSinkID()
	, m_piMediaTypeAnalyzer(NULL)
	, m_fRecordingRequested(false)
	, m_cClockState()
	, m_pwszCurrentPermRecording()
	, m_pwszCompletedPermRecording()
	, m_listCSampleProducerPendingEvent()
	, m_piMediaEventSink(0)
	, m_hyTimeToPositionOffset(0)
{
	CAutoLock cAutoLock(&m_cCritSecStream);

	wchar_t pwszTmpID[64];

#ifdef DEBUG
	_snwprintf(pwszTmpID, 64, L"live%d", s_uSinkIDCounter);
	++s_uSinkIDCounter;
#else
	GUID guidSink;
	HRESULT hr = CoCreateGuid(&guidSink);
	if (FAILED(hr))
		throw CHResultError(hr);
	
	_snwprintf(pwszTmpID, 64, L"live:%X-%X-%X-%X-%X-%X-%X-%X-%X-%X-%X",
		guidSink.Data1,
		(unsigned long) guidSink.Data2,
		(unsigned long) guidSink.Data3,
		(unsigned long) guidSink.Data4[0],
		(unsigned long) guidSink.Data4[1],
		(unsigned long) guidSink.Data4[2],
		(unsigned long) guidSink.Data4[3],
		(unsigned long) guidSink.Data4[4],
		(unsigned long) guidSink.Data4[5],
		(unsigned long) guidSink.Data4[6],
		(unsigned long) guidSink.Data4[7]);
#endif
	m_pwszSinkID = pwszTmpID;

	DbgLog((LOG_ENGINE_OTHER, 2, _T("CSampleProducer: constructed %p\n"), this));

}  // CSampleProducer::CSampleProducer

CSampleProducer::~CSampleProducer(void)
{
	CAutoLock cAutoLock(&m_cCritSecStream);

	DbgLog((LOG_ENGINE_OTHER, 2, _T("CSampleProducer: destroying %p\n"), this));

	Cleanup();
} // CSampleProducer::~CSampleProducer

///////////////////////////////////////////////////////////////////////
//
// Class CSampleProducer:   ISampleProducer
//
///////////////////////////////////////////////////////////////////////

IPipelineComponent *CSampleProducer::GetPipelineComponent()
{
	return this;
} // CSampleProducer::GetPipelineComponent()

LONGLONG CSampleProducer::GetProducerTime() const
{
	return m_cClockState.GetStreamTime() + m_cSampleProducerSampleHistory.m_hyClockOffset
		 + m_hyTimeToPositionOffset;
}

bool CSampleProducer::IsHandlingFile(LPCOLESTR pszFileName)
{
	if (IsLiveTvToken(pszFileName))
		return true;

	CAutoLock cAutoLock(&m_cCritSecStream);
	return IsRecordingKnown(pszFileName);
} // CSampleProducer::IsHandlingFile

bool CSampleProducer::IsLiveTvToken(LPCOLESTR pszLiveTvToken) const
{
	if (!pszLiveTvToken)
		return false;
#ifdef DEBUG
	// In order to make our life easier and be able to test using a tool that,
	// like GraphEdit, uses a normal file dialog to prompt for the recording
	// name, accept any recording name that ends with the live tv token.
	size_t uProposedTokenLength = wcslen(pszLiveTvToken);
	size_t uActualTokenLength = m_pwszSinkID.length();
	if ((uActualTokenLength <= uProposedTokenLength) &&
		(wcscmp(pszLiveTvToken + uProposedTokenLength - uActualTokenLength,
				m_pwszSinkID.c_str()) == 0))
		return true;
	return false;
#else
	return (OLESTRCMP(pszLiveTvToken, m_pwszSinkID.c_str()) == 0);
#endif
}

HRESULT CSampleProducer::BindConsumer(ISampleConsumer *piSampleConsumer, LPCOLESTR pszFileName)
{
	CAutoLock cAutoLock(&m_cCritSecStream);

	HRESULT hr = S_OK;

	DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleProducer(%p)::BindConsumer(%p)\n"), this, piSampleConsumer));

	if (!piSampleConsumer || !pszFileName)
		hr = E_POINTER;
	else
	{
		if (!IsLiveTvToken(pszFileName) &&
			!IsRecordingKnown(pszFileName))
			hr = E_FAIL;
		else if (!m_cSampleConsumerBindings.IsBound(piSampleConsumer))
		{
			try {
				CSampleConsumerState cSampleConsumerState;
				cSampleConsumerState.m_eSampleProducerMode = PRODUCER_SUPPRESS_SAMPLES;
				cSampleConsumerState.m_piSampleConsumer = piSampleConsumer;
				m_cSampleConsumerBindings.Add(cSampleConsumerState);

				piSampleConsumer->NotifyBound(this);
			}
			catch (const CHResultError &rcHResultError)
			{
				hr = rcHResultError.m_hrError;
			}
			catch (const std::exception& )
			{
				hr = E_FAIL;
			}
		}
	}
	if (FAILED(hr))
	{
		DbgLog((LOG_ERROR, 2, _T("CSampleProducer(%p)::BindConsumer() failed with HRESULT %d\n"),
			this, hr));
	}
	return hr;
} // CSampleProducer::BindConsumer

HRESULT CSampleProducer::UnbindConsumer(ISampleConsumer *piSampleConsumer)
{
	CAutoLock cAutoLock(&m_cCritSecStream);

	DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleProducer(%p)::UnbindConsumer(%p)\n"), this, piSampleConsumer));

	HRESULT hr = S_OK;

	if (!piSampleConsumer)
		hr = E_POINTER;
	else
	{
		try {
			m_cSampleConsumerBindings.Unbind(piSampleConsumer);
			piSampleConsumer->NotifyBound(NULL);
		}
		catch (const CHResultError & rcHResultError)
		{
			hr = rcHResultError.m_hrError;
		}
		catch (const std::exception& )
		{
			hr = E_FAIL;
		}
	}
	if (FAILED(hr))
	{
		DbgLog((LOG_ERROR, 2, _T("CSampleProducer(%p)::UnbindConsumer() failed with HRESULT %d\n"),
			this, hr));
	}
	return hr;
} // CSampleProducer::UnbindConsumer

HRESULT CSampleProducer::UpdateMode(
	SAMPLE_PRODUCER_MODE eSampleProducerMode,
	ULONG uModeVersionCount,
	LONGLONG hyDesiredPos,
	ISampleConsumer *piSampleConsumer)
{
	CAutoLock cAutoLock(&m_cCritSecStream);

	DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleProducer(%p)::UpdateMode(%d, %u, %I64d, %p)\n"),
		this, (int) eSampleProducerMode, uModeVersionCount, hyDesiredPos, piSampleConsumer));

	HRESULT hr = S_OK;

	if (!piSampleConsumer)
		hr = E_POINTER;
	else
		try {
			CSampleConsumerState &rcSampleConsumerState = m_cSampleConsumerBindings.GetState(piSampleConsumer);
		
			rcSampleConsumerState.m_eSampleProducerMode = eSampleProducerMode;
			rcSampleConsumerState.m_uModeVersionCount= uModeVersionCount;
			switch (eSampleProducerMode)
			{
			case PRODUCER_SEND_SAMPLES_FORWARD:
				{
					try {
						EnsureConsumerHasMediaType(rcSampleConsumerState);
						CSampleCacheFwdIterator fwdIterator(m_cSampleProducerSampleHistory.m_cSampleCache);
						while (fwdIterator.HasMore())
						{
							CProducerSample &rcProducerSample = fwdIterator.Current();
							if ((hyDesiredPos < 0) ||
								(rcProducerSample.m_hyMediaStartTime >= hyDesiredPos))
								rcSampleConsumerState.m_piSampleConsumer->ProcessProducerSample(
									this, rcProducerSample.m_hyMediaSampleID,
									rcSampleConsumerState.m_uModeVersionCount,
									rcProducerSample.m_hyMediaStartTime,
									rcProducerSample.m_hyMediaEndTime,
									rcProducerSample.m_bPinIndex);
							fwdIterator.Next();
						}
					} catch (const std::exception &) {
						rcSampleConsumerState.m_piSampleConsumer->NotifyProducerCacheDone(this, uModeVersionCount);
						rcSampleConsumerState.m_eSampleProducerMode = PRODUCER_SEND_SAMPLES_LIVE;
						throw;
					};
					rcSampleConsumerState.m_piSampleConsumer->NotifyProducerCacheDone(this, uModeVersionCount);
					rcSampleConsumerState.m_eSampleProducerMode = PRODUCER_SEND_SAMPLES_LIVE;
				}
				break;

			case PRODUCER_SEND_SAMPLES_BACKWARD:
				{
					try {
						EnsureConsumerHasMediaType(rcSampleConsumerState);
						CSampleCacheRevIterator revIterator(m_cSampleProducerSampleHistory.m_cSampleCache);
						while (revIterator.HasMore())
						{
							CProducerSample &rcProducerSample = revIterator.Current();
							if ((hyDesiredPos < 0) ||
								(rcProducerSample.m_hyMediaStartTime <= hyDesiredPos))
								rcSampleConsumerState.m_piSampleConsumer->ProcessProducerSample(
									this, rcProducerSample.m_hyMediaSampleID,
									rcSampleConsumerState.m_uModeVersionCount,
									rcProducerSample.m_hyMediaStartTime,
									rcProducerSample.m_hyMediaEndTime,
									rcProducerSample.m_bPinIndex);
							revIterator.Previous();
						}
					} catch (const std::exception &) {
						rcSampleConsumerState.m_piSampleConsumer->NotifyProducerCacheDone(this,
							uModeVersionCount);
						rcSampleConsumerState.m_eSampleProducerMode = PRODUCER_SEND_SAMPLES_LIVE;
						throw;
					};
					rcSampleConsumerState.m_piSampleConsumer->NotifyProducerCacheDone(this,
						uModeVersionCount);
					rcSampleConsumerState.m_eSampleProducerMode = PRODUCER_SUPPRESS_SAMPLES;
				}
				break;

			case PRODUCER_SEND_SAMPLES_LIVE:
				EnsureConsumerHasMediaType(rcSampleConsumerState);
				break;

			case PRODUCER_SUPPRESS_SAMPLES:
				break;
			}
		}
		catch (const CHResultError & rcHResultError)
		{
			hr = rcHResultError.m_hrError;
		}
		catch (const std::exception& )
		{
			hr = E_FAIL;
		}

	if (FAILED(hr))
	{
		DbgLog((LOG_ERROR, 2, _T("CSampleProducer(%p)::UpdateMode() failed with HRESULT %d\n"),
			this, hr));
	}
	return hr;
} // CSampleProducer::UpdateMode

void CSampleProducer::GetModeForConsumer(ISampleConsumer *piSampleConsumer,
		SAMPLE_PRODUCER_MODE &reSampleProducerMode, ULONG &ruModeVersionCount)
{
	CAutoLock cAutoLock(&m_cCritSecStream);

	DbgLog((LOG_ENGINE_OTHER, 3, _T("CSampleProducer(%p)::GetModeForConsumer(%p)\n"),
		this, piSampleConsumer));

	reSampleProducerMode = PRODUCER_SUPPRESS_SAMPLES;
	ruModeVersionCount = 0;

	if (piSampleConsumer)
	{
		try {
			CSampleConsumerState &rcSampleConsumerState = m_cSampleConsumerBindings.GetState(piSampleConsumer);
		
			reSampleProducerMode = rcSampleConsumerState.m_eSampleProducerMode;
			ruModeVersionCount = rcSampleConsumerState.m_uModeVersionCount;
		}
		catch (const std::exception &) {};
	}
} // CSampleProducer::GetModeForConsumer

CPauseBufferData* CSampleProducer::GetPauseBuffer(void)
{
	IPauseBufferMgr *ppbmgr = NULL;

	{
		CAutoLock cAutoLock(&m_cCritSecStream);

		ppbmgr = GetPauseBufferMgr();
	}
	CPauseBufferData *pcPauseBufferData = (ppbmgr ? ppbmgr->GetPauseBuffer() : 0);
	m_pcPauseBufferData = pcPauseBufferData;
	return pcPauseBufferData;
} // CSampleProducer::GetPauseBuffer

PRODUCER_STATE CSampleProducer::GetSinkState()
{
	PRODUCER_STATE uSinkState;

	switch (m_eFilterState)
	{
	case State_Running:
		uSinkState = PRODUCER_IS_RUNNING;
		break;
	case State_Stopped:
		uSinkState = PRODUCER_IS_STOPPED;
		break;
	case State_Paused:
		uSinkState = PRODUCER_IS_PAUSED;
		break;
	}
	return uSinkState;
}

UCHAR CSampleProducer::QueryNumInputPins()
{
	return m_pcPinMappings ? m_pcPinMappings->GetPinCount() : 0;
} // CSampleProducer::QueryNumInputPins

CMediaType CSampleProducer::QueryInputPinMediaType(UCHAR bPinIndex)
{
	return m_cSampleProducerSampleHistory.m_vcsppr[bPinIndex].m_rsPinState.pcMediaTypeDescription->m_cMediaType;
} // CSampleProducer::QueryInputPinMediaType

void CSampleProducer::GetPositionRange(LONGLONG &rhyMinPosition,
										LONGLONG &rhyMaxPosition) const
{
	// No locking here -- this is one of the routines that the consumer
	// must be able to call into while holding the consumer lock.

	rhyMinPosition = m_cSampleProducerSampleHistory.m_hyMinPosition;
	rhyMaxPosition = m_cSampleProducerSampleHistory.m_hyMaxPosition;
} // CSampleProducer::GetPositionRange

bool CSampleProducer::GetSample(UCHAR bPinIndex, LONGLONG hyMediaSampleID,
								IMediaSample &riMediaSample)
{
	CAutoLock cAutoLock(&m_cCritSecStream);

	try {
		CProducerSample &cProducerSample = m_cSampleProducerSampleHistory.m_cSampleCache.Find(hyMediaSampleID);
		if (cProducerSample.m_hyMediaSampleID == hyMediaSampleID)
		{
			ASSERT(cProducerSample.m_bPinIndex == bPinIndex);
			MSDvr::CopySample(cProducerSample.m_piMediaSample, &riMediaSample);
			return true;
		}
	} catch (const std::exception &) {
	}
	return false;
} // CSampleProducer::GetSample

void CSampleProducer::UpdateLatestTime(LONGLONG sampleStart)
{
	CAutoLock cAutoLock(&m_cCritSecStream);

	if (m_pcPauseBufferData)
		m_pcPauseBufferData->m_llLastSampleTime_100nsec = sampleStart;

	if (!m_listCSampleProducerPendingEvent.empty())
	{
		LONG lAVPosition = (LONG) (sampleStart / 10000LL);  // convert 100 ns time units to milliseconds
		std::list<CSampleProducerPendingEvent>::const_iterator iter;
		for (iter = m_listCSampleProducerPendingEvent.begin();
			iter != m_listCSampleProducerPendingEvent.end();
			++iter)
		{
			if (m_piMediaEventSink)
			{
#ifdef DEBUG
				HRESULT hr =
#endif
					m_piMediaEventSink->Notify(iter->lEventId, iter->lEventParam1, lAVPosition);
#ifdef DEBUG
				if (FAILED(hr))
				{
					DbgLog((LOG_ERROR, 2, _T("CSampleProducer::UpdateLatestTime() -- IMediaEventSink::Notify() returned error %d\n"),
						hr));
				}
#endif
			}
		}
		m_listCSampleProducerPendingEvent.clear();
	}
}

LONGLONG CSampleProducer::GetReaderWriterGapIn100Ns() const
{
	return m_piWriter ? m_piWriter->GetMaxSampleLatency() : -1;
}

HRESULT CSampleProducer::GetGraphClock(IReferenceClock** ppIReferenceClock) const
{
	return m_cClockState.GetClock(ppIReferenceClock);
}

bool CSampleProducer::IndicatesFlushWasTune(IMediaSample &riMediaSample, CDVRInputPin &rcInputPin)
{
	bool fIsTune = false;

	try {
		UCHAR bPinIndex = MapInputPin(rcInputPin);
		fIsTune = IsPostFlushSampleAfterTune(bPinIndex, riMediaSample);
	} catch (const std::exception &) {};

	return fIsTune;
} // CSampleProducer::IndicatesFlushWasTune

bool CSampleProducer::IndicatesNewSegmentWasTune(IMediaSample &riMediaSample, CDVRInputPin &rcInputPin)
{
	bool fIsTune = false;

	try {
		UCHAR bPinIndex = MapInputPin(rcInputPin);
		fIsTune = IsNewSegmentSampleAfterTune(bPinIndex, riMediaSample);
	} catch (const std::exception &) {};

	return fIsTune;
} // CSampleProducer::IndicatesFlushWasTune

///////////////////////////////////////////////////////////////////////
//
// Class CSampleProducer:   IFileSinkFilter2
//
///////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE CSampleProducer::SetFileName(
	/* [in] */ LPCOLESTR pszFileName,
	/* [unique][in] */ const AM_MEDIA_TYPE *pmt)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("CSampleProducer(%p)::SetFileName()\n"), this));

	HRESULT hr = E_NOTIMPL;

	{
	CAutoLock cAutoLock(&m_cCritSecStream);

	if (m_pifsinkf2Next)
		hr = m_pifsinkf2Next->SetFileName(pszFileName, pmt);
	}

	if (FAILED(hr))
	{
		if (!m_pifsinkf2Next && (hr == E_NOTIMPL))
		{
			DbgLog((LOG_ENGINE_OTHER, 3, _T("CSampleProducer(%p)::SetFileName(): no component to forward to, returning E_NOTIMPL\n"),
				this));
		}
		else
		{
			DbgLog((LOG_ERROR, 2, _T("CSampleProducer(%p)::SetFileName() failed with HRESULT %d\n"),
				this, hr));
		}
	}
	return hr;
} // CSampleProducer::SetFileName

HRESULT STDMETHODCALLTYPE CSampleProducer::GetCurFile(
	/* [out] */ LPOLESTR *ppszFileName,
	/* [out] */ AM_MEDIA_TYPE *pmt)
{
	CAutoLock cAutoLock(&m_cCritSecStream);

	HRESULT hr = E_NOTIMPL;

	if (m_pifsinkf2Next)
	{
		hr = m_pifsinkf2Next->GetCurFile(ppszFileName, pmt);
	}

	if (FAILED(hr))
	{
		if (!m_pifsinkf2Next && (hr == E_NOTIMPL))
		{
			DbgLog((LOG_ENGINE_OTHER, 3, _T("CSampleProducer(%p)::GetCurFile(): no component to forward to, returning E_NOTIMPL\n"),
				this));
		}
		else
		{
			DbgLog((LOG_ERROR, 2, _T("CSampleProducer(%p)::GetCurFile() failed with HRESULT %d\n"),
				this, hr));
		}
	}
	return hr;
} // CSampleProducer::GetCurFile

HRESULT STDMETHODCALLTYPE CSampleProducer::SetMode(
	/* [in] */ DWORD dwFlags)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("CSampleProducer(%p)::SetMode(%lu)\n"), this, dwFlags));

	HRESULT hr = E_NOTIMPL;

	if (m_pifsinkf2Next)
		hr = m_pifsinkf2Next->SetMode(dwFlags);

	if (FAILED(hr))
	{
		if (!m_pifsinkf2Next && (hr == E_NOTIMPL))
		{
			DbgLog((LOG_ENGINE_OTHER, 3, _T("CSampleProducer(%p)::SetMode(): no component to forward to, returning E_NOTIMPL\n"),
				this));
		}
		else
		{
			DbgLog((LOG_ERROR, 2, _T("CSampleProducer(%p)::SetMode() failed with HRESULT %d\n"),
				this, hr));
		}
	}
	return hr;
} // CSampleProducer::SetMode

HRESULT STDMETHODCALLTYPE CSampleProducer::GetMode(
	/* [out] */ DWORD *pdwFlags)
{
	CAutoLock cAutoLock(&m_cCritSecStream);

	HRESULT hr = E_NOTIMPL;

	if (m_pifsinkf2Next)
		hr = m_pifsinkf2Next->GetMode(pdwFlags);

	if (FAILED(hr))
	{
		if (!m_pifsinkf2Next && (hr == E_NOTIMPL))
		{
			DbgLog((LOG_ENGINE_OTHER, 3, _T("CSampleProducer(%p)::GetMode(): no component to forward to, returning E_NOTIMPL\n"),
				this));
		}
		else
		{
			DbgLog((LOG_ERROR, 2, _T("CSampleProducer(%p)::GetMode() failed with HRESULT %d\n"),
				this, hr));
		}
	}
	return hr;
} // CSampleProducer::GetMode
	
///////////////////////////////////////////////////////////////////////
//
// Class CSampleProducer:   IStreamBufferCapture
//
///////////////////////////////////////////////////////////////////////

STDMETHODIMP CSampleProducer::GetCurrentPosition(/* [out] */ LONGLONG *phyCurrentPosition)
{
	if (!phyCurrentPosition)
	{
		return E_POINTER;
	}
	*phyCurrentPosition = m_cSampleProducerSampleHistory.m_hyMaxPosition;
	DbgLog((LOG_SINK_DISPATCH, 3, _T("CSampleProducer::GetCurrentPosition() returning %I64d\n"),
		*phyCurrentPosition));
	return S_OK;
} // CSampleProducer::GetCurrentPosition

HRESULT STDMETHODCALLTYPE CSampleProducer::GetCaptureMode(
	/* [out] */ STRMBUF_CAPTURE_MODE *psbcapturemode,
	/* [out] */ LONGLONG *pllMaxBufferMilliseconds)
{
	CAutoLock cAutoLock(&m_cCritSecStream);

	if (!psbcapturemode || !pllMaxBufferMilliseconds)
		return E_POINTER;

	HRESULT hr = E_NOTIMPL;

	if (m_pisbcNext)
		hr = m_pisbcNext->GetCaptureMode(psbcapturemode, pllMaxBufferMilliseconds);
	if (FAILED(hr))
	{
		if (!m_pifsinkf2Next && (hr == E_NOTIMPL))
		{
			DbgLog((LOG_ENGINE_OTHER, 3, _T("CSampleProducer(%p)::GetCaptureMode(): no component to forward to, returning E_NOTIMPL\n"),
				this));
		}
		else
		{
			DbgLog((LOG_ERROR, 2, _T("CSampleProducer(%p)::GetCaptureMode() failed with HRESULT %d\n"),
				this, hr));
		}
	}
	return hr;
} // CSampleProducer::GetCaptureMode

HRESULT STDMETHODCALLTYPE CSampleProducer::BeginTemporaryRecording(
	/* [in] */ LONGLONG llBufferSizeInMilliseconds)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("CSampleProducer(%p)::BeginTemporaryRecording(%I64d)\n"),
		this, llBufferSizeInMilliseconds));

	HRESULT hr = E_NOTIMPL;

	{
		CAutoLock cAutoLock(&m_cCritSecStream);

		if (m_pisbcNext)
		{
			hr = m_pisbcNext->BeginTemporaryRecording(llBufferSizeInMilliseconds);
		}
	}


	if (SUCCEEDED(hr) || (hr == E_NOTIMPL))
	{
		if (m_cSampleProducerSampleHistory.m_fSawSample &&
			!m_pwszCurrentPermRecording.empty())
		{
			m_fRecordingRequested = true;
			m_pwszCompletedPermRecording = m_pwszCurrentPermRecording;
		}
		m_pwszCurrentPermRecording = L"";
	}
	if (FAILED(hr))
	{
		if (!m_pifsinkf2Next && (hr == E_NOTIMPL))
		{
			DbgLog((LOG_ENGINE_OTHER, 3, _T("CSampleProducer(%p)::BeginTemporaryRecording(): no component to forward to, returning E_NOTIMPL\n"),
				this));
		}
		else
		{
			DbgLog((LOG_ERROR, 2, _T("CSampleProducer(%p)::BeginTemporaryRecording() failed with HRESULT %d\n"),
				this, hr));
		}
	}
	return hr;
} // CSampleProducer::BeginTemporaryRecording

HRESULT STDMETHODCALLTYPE CSampleProducer::BeginPermanentRecording(
	/* [in] */ LONGLONG llRetainedSizeInMilliseconds,
	/* [out] */ LONGLONG *pllActualRetainedSize)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("CSampleProducer(%p)::BeginPermanentRecording(%I64d)\n"),
		this, llRetainedSizeInMilliseconds));

	// Store a default value which really gets set by m_pisbcNext
	if (pllActualRetainedSize)
	{
		*pllActualRetainedSize = 0;
	}

	HRESULT hr = E_NOTIMPL;

	{
		CAutoLock cAutoLock(&m_cCritSecStream);

		if (m_pisbcNext)
		{
			hr = m_pisbcNext->BeginPermanentRecording(llRetainedSizeInMilliseconds, pllActualRetainedSize);
		}
	}

	if (SUCCEEDED(hr) || (hr == E_NOTIMPL))
	{
		if (m_cSampleProducerSampleHistory.m_fSawSample &&
			!m_pwszCurrentPermRecording.empty())
		{
			m_fRecordingRequested = true;
			m_pwszCompletedPermRecording = m_pwszCurrentPermRecording;
		}
		if (!GetRecordingNameFromWriter(m_pwszCurrentPermRecording))
		{
			m_pwszCurrentPermRecording = L"";
			DbgLog((LOG_ERROR, 2, _T("CSampleProducer::BeginPermanentRecording():  failed to cache recording name, might confuse a consumer\n")));
		}
	}
	if (FAILED(hr))
	{
		if (!m_pifsinkf2Next && (hr == E_NOTIMPL))
		{
			DbgLog((LOG_ENGINE_OTHER, 3, _T("CSampleProducer(%p)::BeginPermanentRecording(): no component to forward to, returning E_NOTIMPL\n"),
				this));
		}
		else
		{
			DbgLog((LOG_ERROR, 2, _T("CSampleProducer(%p)::BeginPermanentRecording() failed with HRESULT %d\n"),
				this, hr));
		}
	}
	return hr;
}  // CSampleProducer::BeginPermanentRecording

HRESULT STDMETHODCALLTYPE CSampleProducer::ConvertToTemporaryRecording(
			/* [in] */ LPCOLESTR pszFileName)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("CSampleProducer(%p)::ConvertToTemporaryRecording()\n"), this));

	HRESULT hr = E_NOTIMPL;

	{
		CAutoLock cAutoLock(&m_cCritSecStream);

		if (m_pisbcNext)
		{
			hr = m_pisbcNext->ConvertToTemporaryRecording(pszFileName);
		}
	}

	if (FAILED(hr))
	{
		if (!m_pifsinkf2Next && (hr == E_NOTIMPL))
		{
			DbgLog((LOG_ENGINE_OTHER, 3, _T("CSampleProducer(%p)::ConvertToTemporaryRecording(): no component to forward to, returning E_NOTIMPL\n"),
				this));
		}
		else
		{
			DbgLog((LOG_ERROR, 2, _T("CSampleProducer(%p)::ConvertToTemporaryRecording() failed with HRESULT %d\n"),
				this, hr));
		}
	}
	return hr;
}  // CSampleProducer::ConvertToTemporaryRecording

HRESULT STDMETHODCALLTYPE CSampleProducer::SetRecordingPath(
			/* [in] */ LPCOLESTR pszPath)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("CSampleProducer(%p)::SetRecordingPath()\n"), this));

	HRESULT hr = E_NOTIMPL;

	if (m_pisbcNext)
	{
		hr = m_pisbcNext->SetRecordingPath(pszPath);
	}

	if (FAILED(hr))
	{
		if (!m_pifsinkf2Next && (hr == E_NOTIMPL))
		{
			DbgLog((LOG_ENGINE_OTHER, 3, _T("CSampleProducer(%p)::SetRecordingPath(): no component to forward to, returning E_NOTIMPL\n"),
				this));
		}
		else
		{
			DbgLog((LOG_ERROR, 2, _T("CSampleProducer(%p)::SetRecordingPath() failed with HRESULT %d\n"),
				this, hr));
		}
	}
	return hr;
} // CSampleProducer::SetRecordingPath

HRESULT STDMETHODCALLTYPE CSampleProducer::GetRecordingPath(
			/* [out] */ LPOLESTR *ppszPath)
{
	CAutoLock cAutoLock(&m_cCritSecStream);

	HRESULT hr = E_NOTIMPL;

	if (m_pisbcNext)
	{
		hr = m_pisbcNext->GetRecordingPath(ppszPath);
	}

	if (FAILED(hr))
	{
		if (!m_pifsinkf2Next && (hr == E_NOTIMPL))
		{
			DbgLog((LOG_ENGINE_OTHER, 3, _T("CSampleProducer(%p)::GetRecordingPath(): no component to forward to, returning E_NOTIMPL\n"),
				this));
		}
		else
		{
			DbgLog((LOG_ERROR, 2, _T("CSampleProducer(%p)::GetRecordingPath() failed with HRESULT %d\n"),
				this, hr));
		}
	}
	return hr;
} // CSampleProducer::GetRecordingPath

HRESULT CSampleProducer::GetBoundToLiveToken(
			/* [out] */ LPOLESTR *ppszToken)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("CSampleProducer(%p)::GetBoundToLiveToken()\n"), this));

	CAutoLock cAutoLock(&m_cCritSecStream);
	HRESULT hr = S_OK;

	// The producer is responsible for returning this -- it won't bother
	// to forward the call.

	if (!ppszToken)
		hr = E_POINTER;
	else
	{
		const WCHAR *szLiveToken = m_pwszSinkID.c_str();
		*ppszToken = (LPOLESTR) CoTaskMemAlloc(sizeof(WCHAR) *
							(1+wcslen(szLiveToken)));
		if (*ppszToken)
		{
			wcscpy(*ppszToken, szLiveToken);
		}
		else
		{
			hr = E_OUTOFMEMORY;
		}
	}
	return hr;
} // CSampleProducer::GetBoundToLiveToken

///////////////////////////////////////////////////////////////////////
//
// Class CSampleProducer:   IPipelineComponent
//
///////////////////////////////////////////////////////////////////////

void CSampleProducer::RemoveFromPipeline()
{
	CAutoLock cAutoLock(&m_cCritSecStream);

	DbgLog((LOG_ENGINE_OTHER, 3, _T("CSampleProducer(%p)::RemoveFromPipeline()\n"), this));

	Cleanup();
} // CSampleProducer::RemoveFromPipeline

ROUTE CSampleProducer::GetPrivateInterface(REFIID riid, void *&rpInterface)
{
	ROUTE result = UNHANDLED_CONTINUE;

	if (riid == IID_ISampleProducer)
	{
		rpInterface = ((ISampleProducer *)this);
		result = HANDLED_STOP;
	}
	return result;
} // CSampleProducer::GetPrivateInterface

ROUTE CSampleProducer::NotifyFilterStateChange(FILTER_STATE eFilterState)
{
	CAutoLock cAutoLock(&m_cCritSecStream);

	DbgLog((LOG_ENGINE_OTHER, 3, _T("CSampleProducer(%p)::NotifyFilterStateChange(%d)\n"),
		this, (int) eFilterState));

	CDVRSinkFilter &rcBaseFilter = m_picpmgr->GetSinkFilter();
	m_cClockState.CacheFilterClock(rcBaseFilter);
	switch (eFilterState)
	{
	case State_Stopped:
		m_cSampleProducerSampleHistory.m_hyClockOffset += m_cClockState.GetStreamTime() + GetAverageSampleInterval();
		m_cClockState.Stop();
		break;

	case State_Paused:
		{
			m_cClockState.CacheFilterClock(rcBaseFilter);
			m_cClockState.Pause();

			switch (m_eFilterState)
			{
			case State_Stopped:
				ASSERT(m_picpmgr);
				ASSERT(m_eFilterState != State_Running);
				m_eFilterState = eFilterState;

				if (m_pcPinMappings)
				{
					delete m_pcPinMappings;
					m_pcPinMappings = NULL;
				}
				m_pcPinMappings = new CPinMappings(rcBaseFilter, m_piMediaTypeAnalyzer);
				m_cSampleProducerSampleHistory.Init(*m_pcPinMappings);
				NotifyPinUpdate();
				{
					HRESULT hr = rcBaseFilter.QueryInterface(IID_IStreamBufferCapture, (void**) &m_pisbcFilter);
					if (FAILED(hr))
					{
						DbgLog((LOG_ERROR, 2, _T("CSampleProducer(%p)::NotifyFilterStateChange() unable to get the filter's IStreamBufferCapture i/f\n"), this));
						throw CHResultError(hr);
					}
					// We are part of the filter so we can safely hold onto a pointer to our
					// filter without a reference count -- and keeping a reference count
					// would lock the filter into existence forever.
					m_pisbcFilter->Release();
				}

				/* For the side-effect of registering for notifications: */
				GetPauseBufferMgr();
				break;
	
			case State_Running:
				break;

			case State_Paused:
				// Huh?  Why are we pausing while in state paused?  Something
				// wierd must've happened -- flag it and then continue on.
				ASSERT(m_eFilterState != State_Paused);
				break;
			}
		}
		break;

	case State_Running:
		m_cClockState.Run(rcBaseFilter.GetStreamBase());
		break;
	}

	m_eFilterState = eFilterState;

	NotifyConsumersOfState();

	return HANDLED_CONTINUE;
} // CSampleProducer::NotifyFilterStateChange

ROUTE CSampleProducer::ConfigurePipeline(
	UCHAR bNumPins,
	CMediaType cMediaTypes[],
	UINT uSizeCustom,
	BYTE bCustom[])
{
	DbgLog((LOG_ENGINE_OTHER, 3, _T("CSampleProducer(%p)::ConfigurePipeline()\n"), this));

	if (m_piMediaTypeAnalyzer)
	{
		UCHAR bPinIndex;
		for (bPinIndex = 0; bPinIndex < bNumPins; ++bPinIndex)
		{
			CMediaTypeDescription *pcMediaTypeDescription =
				m_piMediaTypeAnalyzer->AnalyzeMediaType(cMediaTypes[bPinIndex]);
			if (!pcMediaTypeDescription)
				throw CHResultError(E_INVALIDARG);
			delete pcMediaTypeDescription;
		}
	}
	return HANDLED_CONTINUE;
} // CSampleProducer::ConfigurePipeline

ROUTE CSampleProducer::DispatchExtension(
						CExtendedRequest &rcExtendedRequest)
{
	ROUTE eRoute = UNHANDLED_CONTINUE;

	DbgLog((LOG_SINK_DISPATCH, 3, _T("CSampleProducer(%p)::DispatchExtension()\n"), this));

	switch (rcExtendedRequest.m_eExtendedRequestType)
	{
	case CExtendedRequest::SAMPLE_PRODUCER_SEND_EVENT_WITH_POSITION:
		if (rcExtendedRequest.m_iPipelineComponentPrimaryTarget == this)
		{
			CAutoLock cAutoLock(&m_cCritSecStream);

			CEventWithAVPosition *pcEventWithAVPosition =
				static_cast<CEventWithAVPosition*>(&rcExtendedRequest);
			DbgLog((LOG_SINK_DISPATCH, 3, _T("CSampleProducer(%p)::DispatchExtension() -- queueing event %d, param1 %d\n"),
				this, pcEventWithAVPosition->m_lEventId, pcEventWithAVPosition->m_lParam1));

			CSampleProducerPendingEvent cSampleProducerPendingEvent;
			cSampleProducerPendingEvent.lEventId = pcEventWithAVPosition->m_lEventId;
			cSampleProducerPendingEvent.lEventParam1 = pcEventWithAVPosition->m_lParam1;
			m_listCSampleProducerPendingEvent.push_back(cSampleProducerPendingEvent);
			eRoute = HANDLED_STOP;
		}
		break;
	}
	return eRoute;
} // CSampleProducer::DispatchExtension

///////////////////////////////////////////////////////////////////////
//
// Class CSampleProducer:   ICapturePipelineComponent
//
///////////////////////////////////////////////////////////////////////

unsigned char CSampleProducer::AddToPipeline(ICapturePipelineManager& ricpmgr)
{
	CAutoLock cAutoLock(&m_cCritSecStream);

	DbgLog((LOG_ENGINE_OTHER, 3, _T("CSampleProducer(%p)::AddToPipeline()\n"), this));

	if (m_pcPauseBufferData)
		m_pcPauseBufferData = NULL;

	m_picpmgr = &ricpmgr;
	IPipelineManager *pManager = &ricpmgr;
	m_pisbcNext =
		static_cast<IStreamBufferCapture*>
			(pManager->RegisterCOMInterface(
				static_cast<TRegisterableCOMInterface<IStreamBufferCapture> &>(*this)));
	pManager->RegisterCOMInterface(static_cast<TRegisterableCOMInterface<IFileSinkFilter> &>(*this));
	m_pifsinkf2Next =
		static_cast<IFileSinkFilter2*>
			(pManager->RegisterCOMInterface(
				static_cast<TRegisterableCOMInterface<IFileSinkFilter2> &>(*this)));

	HRESULT hr = CSampleProducerLocator::RegisterSampleProducer(this);
	if (FAILED(hr))
	{
		DbgLog((LOG_ERROR, 2, _T("CSampleProducer(%p)::AddToPipeline() throw HRESULT %d as an exception\n"),
			this, hr));
		throw CHResultError(hr);
	}

	GetPauseBufferMgr();  // Do this for the side-effects

	CBaseFilter &rcBaseFilter = pManager->GetFilter();
	IFilterGraph *piFilterGraph = rcBaseFilter.GetFilterGraph();
	if (piFilterGraph)
	{
		// We cannot keep a reference on the filter graph because the
		// filter graph holds a reference to the sink filter and hence
		// sample producer. Ditto for the media control and media event
		// sink.  However, GetFilterGraph does not AddRef the filter graph
		// so we just hold onto the returned value as is.  We must release
		// the media events though since QI does AddRef.
		hr = piFilterGraph->QueryInterface(IID_IMediaEventSink, (void **)&m_piMediaEventSink);
		if (SUCCEEDED(hr))
		{
			m_piMediaEventSink->Release();
		}
		else
			m_piMediaEventSink = NULL;
	}

	return 0;
} // CSampleProducer::AddToPipeline

ROUTE CSampleProducer::ProcessInputSample(
	IMediaSample &riMediaSample,
	CDVRInputPin &rcDVRInputPin)
{
#ifdef GATHER_STATS
	if (S_OK == riMediaSample.IsSyncPoint())
		++gSyncPoints;
	++gSamples;
#endif
	ROUTE eRoute = HANDLED_CONTINUE;

#ifdef DEBUG
	LONGLONG hyMediaStart, hyMediaEnd;
	if (FAILED(riMediaSample.GetMediaTime(&hyMediaStart, &hyMediaEnd)))
	{
		hyMediaStart = -1;
		hyMediaEnd = -1;
	}
	DbgLog((LOG_SINK_DISPATCH, 5, _T("CSampleProducer::ProcessInputSample():  pin #%d, [%I64d, %I64d] %s %s\n"),
		rcDVRInputPin.GetPinNum(), hyMediaStart, hyMediaEnd,
		(riMediaSample.IsSyncPoint() == S_OK) ? _T("sync") : _T(""),
		(riMediaSample.IsDiscontinuity() == S_OK) ? _T("discontinuity") : _T("") ));
#endif

	CAutoLock cAutoLock(&m_cCritSecStream);

	if ((m_eFilterState == State_Stopped) ||
		(m_cSampleProducerSampleHistory.m_bPinsInFlush > 0))
		return UNHANDLED_STOP;

	try {
		UCHAR bPinIndex = MapInputPin(rcDVRInputPin);

		if (IsSampledWanted(bPinIndex, riMediaSample))
			eRoute = m_cSampleProducerSampleHistory.OnNewSample(bPinIndex, riMediaSample);
		else
			eRoute = HANDLED_STOP;
	}
	catch (const std::exception& rcException)
	{
		UNUSED (rcException);  // suppress release build warning
#ifdef UNICODE
		DbgLog((LOG_ERROR, 2,
			_T("CSampleProducer(%p)::ProcessInputSample() caught exception %S\n"),
			this, rcException.what()));
#else
		DbgLog((LOG_ERROR, 2,
			_T("CSampleProducer(%p)::ProcessInputSample() caught exception %s\n"),
			this, rcException.what()));
#endif
		return HANDLED_STOP;
	};
	
	return eRoute;
} // CSampleProducer::ProcessInputSample

ROUTE CSampleProducer::NotifyBeginFlush(CDVRInputPin &rcDVRInputPin)
{
	CAutoLock cAutoLock(&m_cCritSecStream);

	DbgLog((LOG_EVENT_DETECTED, 3, _T("CSampleProducer(%p)::NotifyBeginFlush(%p)\n"), this, &rcDVRInputPin));

	UCHAR bPinIndex = MapInputPin(rcDVRInputPin);

	// Handle the flush locally:  that means taking note of the flush
	// and then releasing the buffers held:

	m_cSampleProducerSampleHistory.OnBeginFlush(bPinIndex);

	return HANDLED_CONTINUE;
} // CSampleProducer::NotifyBeginFlush

ROUTE CSampleProducer::NotifyEndFlush(CDVRInputPin &rcDVRInputPin)
{
	CAutoLock cAutoLock(&m_cCritSecStream);

	DbgLog((LOG_EVENT_DETECTED, 3, _T("CSampleProducer(%p)::NotifyEndFlush(%p)\n"), this, &rcDVRInputPin));

	UCHAR bPinIndex = MapInputPin(rcDVRInputPin);
	m_cSampleProducerSampleHistory.OnEndFlush(bPinIndex);

	return HANDLED_CONTINUE;
} // CSampleProducer::NotifyEndFlush

ROUTE CSampleProducer::NotifyNewSegment(
						CDVRInputPin&	rcDVRInputPin,
						REFERENCE_TIME rtStart,
						REFERENCE_TIME rtEnd,
						double dblRate)
{
	CAutoLock cAutoLock(&m_cCritSecStream);

	DbgLog((LOG_EVENT_DETECTED, 3, _T("CSampleProducer(%p)::NotifyNewSegment(%p)\n"), this, &rcDVRInputPin));

	UCHAR bPinIndex = MapInputPin(rcDVRInputPin);

	// Record the fact that a segment was started

	m_cSampleProducerSampleHistory.OnNewSegment(bPinIndex, rtStart, rtEnd, dblRate);

	return HANDLED_CONTINUE;
} // CSampleProducer::NotifyNewSegment


ROUTE CSampleProducer::GetAllocatorRequirements(
	ALLOCATOR_PROPERTIES &rProperties,
	CDVRInputPin &rcDVRInputPin)
{
	AM_MEDIA_TYPE mt;
	HRESULT hr = rcDVRInputPin.ConnectionMediaType(&mt);
	if (rProperties.cBuffers == 0)
		rProperties.cBuffers = 2;
	if (rProperties.cbAlign == 0)
		rProperties.cbAlign = 1;

    ULONG lAddedBuffers, lMinimumBufferSize;
	ComputeBufferQuota(SUCCEEDED(hr) ? &mt : (AM_MEDIA_TYPE *) NULL,
						(ULONG)lAddedBuffers, lMinimumBufferSize);
	rProperties.cBuffers += lAddedBuffers + g_dwExtraSampleProducerBuffers;
	if (rProperties.cbBuffer < (LONG)lMinimumBufferSize)
		rProperties.cbBuffer = (LONG)lMinimumBufferSize;
	if (SUCCEEDED(hr))
		FreeMediaType(mt);

	DbgLog((LOG_ENGINE_OTHER, 3, _T("CSampleProducer::GetAllocatorRequirements():  %d buffers of size %d\n"),
		rProperties.cBuffers, rProperties.cbBuffer));

	return HANDLED_CONTINUE;
} // CSampleProducer::GetAllocatorRequirements

///////////////////////////////////////////////////////////////////////
//
// Class CSampleProducer:   IPauseBufferCallback
//
///////////////////////////////////////////////////////////////////////

void CSampleProducer::PauseBufferUpdated(CPauseBufferData *pData)
{
	NotifyConsumersOfPauseBufferChange(pData);
} // CSampleProducer::PauseBufferUpdated

///////////////////////////////////////////////////////////////////////
//
// Class CSampleProducer:   protected methods
//
///////////////////////////////////////////////////////////////////////

void CSampleProducer::Cleanup(void)
{
	try {
		DisassociateFromConsumers();

		m_pwszCurrentPermRecording = L"";
		m_pwszCompletedPermRecording = L"";
		m_cSampleConsumerBindings.Clear();
		m_listCSampleProducerPendingEvent.clear();
		m_piMediaEventSink = 0;
		m_cClockState.Clear();
		m_pifsinkf2Next = 0;
		m_pisbcNext = 0;
		m_pisbcFilter = NULL;
		m_eFilterState = State_Stopped;
		m_cSampleProducerSampleHistory.Clear();
		m_picpmgr = 0;
		m_fRecordingRequested = false;
		m_hyTimeToPositionOffset = 0;
		if (m_pipbmgr)
		{
			try {
				m_pipbmgr->CancelNotifications(this);
			}
			catch (const std::exception& rcException)
			{
				UNUSED (rcException);  // suppress release build warning
#ifdef UNICODE
				DbgLog((LOG_ERROR, 2,
					_T("CSampleProducer(%p)::Cleanup() caught exception %S from CancelNotifications()\n"),
					this, rcException.what()));
#else
				DbgLog((LOG_ERROR, 2,
					_T("CSampleProducer(%p)::Cleanup() caught exception %s from CancelNotifications()\n"),
					this, rcException.what()));
#endif
			}
			m_pipbmgr = 0;
		}
		m_piWriter = 0;
		m_pifsinkf2Writer = 0;
		if (m_pcPauseBufferData)
			m_pcPauseBufferData = NULL;
		if (m_pcPinMappings)
		{
			delete m_pcPinMappings;
			m_pcPinMappings = NULL;
		}
	}
	catch (const std::exception& rcException)
	{
		UNUSED (rcException);  // suppress release build warning
#ifdef UNICODE
		DbgLog((LOG_ERROR, 2,
			_T("CSampleProducer(%p)::Cleanup() caught exception %S\n"),
			this, rcException.what()));
#else
		DbgLog((LOG_ERROR, 2,
			_T("CSampleProducer(%p)::Cleanup() caught exception %s\n"),
			this, rcException.what()));
#endif
	}
} // CSampleProducer::Cleanup

IPauseBufferMgr *CSampleProducer::GetPauseBufferMgr(void)
{
	// todo - Is the following state check necessary?
	if (!m_pipbmgr && m_picpmgr /*&& (m_eFilterState != State_Stopped)*/)
	{
		CPipelineRouter cPipelineRouter = m_picpmgr->GetRouter(NULL, false, false);
		void *pvTmp = 0;
		ROUTE eRoute = cPipelineRouter.GetPrivateInterface(IID_IPauseBufferMgr, pvTmp);
		if ((eRoute == HANDLED_STOP) || (eRoute == HANDLED_CONTINUE))
		{
			m_pipbmgr = static_cast<IPauseBufferMgr*>(pvTmp);
			m_pipbmgr->RegisterForNotifications(this);
		}
	}
	return m_pipbmgr;
} // CSampleProducer::GetPauseBufferMgr

bool CSampleProducer::GetRecordingNameFromWriter(std::wstring &recordingName)
{
	bool gotRecording = true;
	recordingName = L"";

	if (!m_pifsinkf2Writer && m_picpmgr)
	{
		CBaseFilter &rcBaseFilter = m_picpmgr->GetFilter();

		IFileSinkFilter2 *piFileSinkFilter2 = NULL;
		if (SUCCEEDED(rcBaseFilter.QueryInterface(IID_IFileSinkFilter2, (void**)&piFileSinkFilter2)))
		{
			m_pifsinkf2Writer = piFileSinkFilter2;
			piFileSinkFilter2->Release();
		}
	}
	if (m_pifsinkf2Writer)
	{
		AM_MEDIA_TYPE mtCurrent;
		LPOLESTR pszCurFileName = 0;
		memset(&mtCurrent, 0, sizeof(mtCurrent));
		if (SUCCEEDED(m_pifsinkf2Writer->GetCurFile(&pszCurFileName, &mtCurrent)))
		{
			if (pszCurFileName && *pszCurFileName)
			{
				FreeMediaType(mtCurrent);
				try {
					recordingName = pszCurFileName;
				}
				catch (const std::exception& )
				{
					gotRecording = false;
				};
			}
			else
				gotRecording = false;
			CoTaskMemFree(pszCurFileName);
		}
		else
			gotRecording = false;
	}
	// else if there is no writer, we must be in a funky test environment. Claim success.
	return gotRecording;
} // CSampleProducer::GetRecordingNameFromWriter

void CSampleProducer::NotifyConsumersOfState(void)
{
	PRODUCER_STATE uSinkState;

	switch (m_eFilterState)
	{
	case State_Stopped:
		uSinkState = PRODUCER_IS_STOPPED;
		break;

	case State_Paused:
		uSinkState = PRODUCER_IS_PAUSED;
		break;

	case State_Running:
		uSinkState = PRODUCER_IS_RUNNING;
		break;
	}

	CSampleConsumerBindingIterator iter(m_cSampleConsumerBindings);
	while (iter.MoveToNext())
	{
		CSampleConsumerState &rcSampleConsumerState = iter.GetCurrent();
		try {
			rcSampleConsumerState.m_piSampleConsumer->SinkStateChanged(this, uSinkState);
		}
		catch (const std::exception& rcException)
		{
			UNUSED (rcException);  // suppress release build warning
#ifdef UNICODE
			DbgLog((LOG_ERROR, 2,
				_T("CSampleProducer(%p)::NotifyConsumersOfState() caught exception %S\n"),
					this, rcException.what()));
#else
			DbgLog((LOG_ERROR, 2,
				_T("CSampleProducer(%p)::NotifyConsumersOfState() caught exception %s\n"),
					this, rcException.what()));
#endif
		};
	}
} // CSampleProducer::NotifyConsumersOfState

void CSampleProducer::NotifyConsumersOfSample(LONGLONG hyMediaSampleID,
											  LONGLONG hyMediaStartPos,
											  LONGLONG hyMediaEndPos, UCHAR bPinIndex)
{
	CSampleConsumerBindingIterator iter(m_cSampleConsumerBindings);

	while (iter.MoveToNext())
	{
		CSampleConsumerState &rcSampleConsumerState = iter.GetCurrent();

		if (PRODUCER_SEND_SAMPLES_LIVE == rcSampleConsumerState.m_eSampleProducerMode)
		{
			try {
				rcSampleConsumerState.m_piSampleConsumer->ProcessProducerSample(
					this, hyMediaSampleID, rcSampleConsumerState.m_uModeVersionCount,
					hyMediaStartPos, hyMediaEndPos, bPinIndex);
			}
			catch (const std::exception& rcException)
			{
				UNUSED (rcException);  // suppress release build warning
#ifdef UNICODE
				DbgLog((LOG_ERROR, 2,
					_T("CSampleProducer(%p)::NotifyConsumersOfSample() caught exception %S\n"),
					this, rcException.what()));
#else
				DbgLog((LOG_ERROR, 2,
					_T("CSampleProducer(%p)::NotifyConsumersOfSample() caught exception %s\n"),
					this, rcException.what()));
#endif
			}
		}
	}
}

void CSampleProducer::NotifyConsumersOfTune(LONGLONG hyTuneSampleStartPos)
{
	DbgLog((LOG_EVENT_DETECTED, 2,
					_T("CSampleProducer(%p)::NotifyConsumersOfTune(%I64d)\n"),
					this, hyTuneSampleStartPos ));
	CSampleConsumerBindingIterator iter(m_cSampleConsumerBindings);

	while (iter.MoveToNext())
	{
		CSampleConsumerState &rcSampleConsumerState = iter.GetCurrent();

		try {
			rcSampleConsumerState.m_piSampleConsumer->ProcessProducerTune(this, hyTuneSampleStartPos);
		}
		catch (const std::exception& rcException)
		{
			UNUSED (rcException);  // suppress release build warning
#ifdef UNICODE
			DbgLog((LOG_ERROR, 2,
				_T("CSampleProducer(%p)::NotifyConsumersOfTune() caught exception %S\n"),
				this, rcException.what()));
#else
			DbgLog((LOG_ERROR, 2,
				_T("CSampleProducer(%p)::NotifyConsumersOfTune() caught exception %s\n"),
				this, rcException.what()));
#endif
		};
	}

	// Also notify anyone monitoring the filter graph that the tune has been handled
	// (on the capture side):

	if (m_piMediaEventSink)
	{
#ifdef DEBUG
		HRESULT hr =
#endif
			m_piMediaEventSink->Notify(DVRENGINE_EVENT_CAPTURE_TUNE_DETECTED, 0, 0);
#ifdef DEBUG
		if (FAILED(hr))
		{
			DbgLog((LOG_ERROR, 2, _T("CSampleProducer::NotifyConsumersOfTune() -- IMediaEventSink::Notify() returned error %d\n"),
				hr));
		}
#endif
	}
}

void CSampleProducer::NotifyConsumerOfCachePrune(LONGLONG hyFirstSampleIDRetained)
{
	CSampleConsumerBindingIterator iter(m_cSampleConsumerBindings);

	while (iter.MoveToNext())
	{
		CSampleConsumerState &rcSampleConsumerState = iter.GetCurrent();

		try {
			rcSampleConsumerState.m_piSampleConsumer->NotifyCachePruned(this, hyFirstSampleIDRetained);
		}
		catch (const std::exception& rcException)
		{
			UNUSED (rcException);  // suppress release build warning
#ifdef UNICODE
			DbgLog((LOG_ERROR, 2,
				_T("CSampleProducer(%p)::NotifyConsumerOfCachePrune() caught exception %S\n"),
				this, rcException.what()));
#else
			DbgLog((LOG_ERROR, 2,
				_T("CSampleProducer(%p)::NotifyConsumerOfCachePrune() caught exception %s\n"),
				this, rcException.what()));
#endif
		};
	}
} // CSampleProducer::NotifyConsumerOfCachePrune

void CSampleProducer::DisassociateFromConsumers()
{
	try {
		CSampleProducerLocator::UnregisterSampleProducer(this);
	}
	catch (const std::exception& rcException)
	{
		UNUSED (rcException);  // suppress release build warning
#ifdef UNICODE
		DbgLog((LOG_ERROR, 2,
			_T("CSampleProducer(%p)::DisassociateFromConsumers() caught exception %S from UnregisterSampleProducer()\n"),
					this, rcException.what()));
#else
		DbgLog((LOG_ERROR, 2,
			_T("CSampleProducer(%p)::DisassociateFromConsumers() caught exception %s from UnregisterSampleProducer()\n"),
					this, rcException.what()));
#endif
	};

	CSampleConsumerBindingIterator iter(m_cSampleConsumerBindings);

	while (iter.MoveToNext())
	{
		try {
			CSampleConsumerState &rcSampleConsumerState = iter.GetCurrent();
			rcSampleConsumerState.m_piSampleConsumer->SinkStateChanged(this, PRODUCER_IS_DEAD);
		}
		catch (const std::exception& rcException)
		{
			UNUSED (rcException);  // suppress release build warning
#ifdef UNICODE
			DbgLog((LOG_ERROR, 2,
				_T("CSampleProducer(%p)::DisassociateFromConsumers() caught exception %S from SinkStateChanged()\n"),
					this, rcException.what()));
#else
			DbgLog((LOG_ERROR, 2,
				_T("CSampleProducer(%p)::DisassociateFromConsumers() caught exception %s from SinkStateChanged()\n"),
					this, rcException.what()));
#endif
		};
	}
}

LONGLONG CSampleProducer::GetFilterGraphClockTime()
{
	return m_cClockState.GetClockTime();
}

bool CSampleProducer::IsPrimaryPin(UCHAR bPinIndex)
{
	ASSERT(m_picpmgr);  // programming bug, not runtime error, would cause this
	if (!m_pcPinMappings)
		m_pcPinMappings = new CPinMappings(m_picpmgr->GetFilter(), m_piMediaTypeAnalyzer);
	return m_pcPinMappings->IsPrimaryPin(bPinIndex);
} // CSampleProducer::IsPrimaryPin

UCHAR CSampleProducer::MapInputPin(CDVRInputPin &rcDVRInputPin)
{
	ASSERT(m_picpmgr);  // programming bug, not runtime error, would cause this
	if (!m_pcPinMappings)
		m_pcPinMappings = new CPinMappings(m_picpmgr->GetFilter(), m_piMediaTypeAnalyzer);
	IPin *piPin = &rcDVRInputPin;
	return m_pcPinMappings->FindPinPos(piPin);
} // CSampleProducer::MapInputPin

void CSampleProducer::EnsureConsumerHasMediaType(CSampleConsumerState &rcSampleConsumerState)
{
	CAutoLock cAutoLock(&m_cCritSecStream);

	if (!rcSampleConsumerState.m_fHasMediaType)
	{
		// TODO:  rework this to attach a media type to a media sample
		rcSampleConsumerState.m_fHasMediaType = true;
	}
} // CSampleProducer::EnsureConsumerHasMediaType

void CSampleProducer::NotifyConsumersOfPauseBufferChange(CPauseBufferData *pcPauseBufferData)
{
	CSampleConsumerBindingIterator iter(m_cSampleConsumerBindings);

	while (iter.MoveToNext())
	{
		try {
			CSampleConsumerState &rcSampleConsumerState = iter.GetCurrent();
			rcSampleConsumerState.m_piSampleConsumer->PauseBufferUpdated(this, pcPauseBufferData);
		}
		catch (const std::exception& rcException)
		{
			UNUSED (rcException);  // suppress release build warning
#ifdef UNICODE
			DbgLog((LOG_ERROR, 2,
				_T("CSampleProducer(%p)::NotifyConsumersOfPauseBufferChange() caught exception %S\n"),
					this, rcException.what()));
#else
			DbgLog((LOG_ERROR, 2,
				_T("CSampleProducer(%p)::NotifyConsumersOfPauseBufferChange() caught exception %s\n"),
					this, rcException.what()));
#endif
		};
	}
} // CSampleProducer::NotifyConsumersOfPauseBufferChange

void CSampleProducer::ExtractMediaTime(UCHAR bPinIndex, IMediaSample &riMediaSample,
									   LONGLONG &sampleStart,
									   BYTE *pbSampleData, long lSampleDataSize)
{
	LONGLONG sampleEnd;
	HRESULT hr = riMediaSample.GetMediaTime(&sampleStart, &sampleEnd);
	if (hr == VFW_E_MEDIA_TIME_NOT_SET)
		sampleStart = GetFilterGraphClockTime();
	else if (FAILED(hr))
		throw CHResultError(hr);
}

void CSampleProducer::SetMediaTime(UCHAR bPinIndex, IMediaSample &riMediaSample,
			LONGLONG sampleStart, LONGLONG sampleEnd,
			BYTE *pbSampleData, long lSampleDataSize)
{
	HRESULT hr = riMediaSample.SetMediaTime(&sampleStart, &sampleEnd);
	if (FAILED(hr))
		throw CHResultError(hr);
} // CSampleProducer::SetMediaTime

bool CSampleProducer::IsSampledWanted(UCHAR bPinIndex, IMediaSample &riMediaSample)
{
	return true;
} // CSampleProducer::IsSampledWanted

void CSampleProducer::GetSampleDeliveryPolicy(UCHAR bPinIndex, IMediaSample &riMediaSample,
			bool &fCacheForConsumer, ROUTE &eRouteForWriter)
{
	fCacheForConsumer = true;
	eRouteForWriter = HANDLED_CONTINUE;
} // CSampleProducer::GetSampleDeliveryPolicy

bool CSampleProducer::IsPostFlushSampleAfterTune(UCHAR bPinIndex, IMediaSample &riMediaSample) const
{
	return ((bPinIndex == m_cSampleProducerSampleHistory.m_bPrimaryPinIndex) &&
			(riMediaSample.IsDiscontinuity() == S_OK) &&
			(riMediaSample.IsSyncPoint() == S_OK));
} // CSampleProducer::IsPostFlushSampleAfterTune

bool CSampleProducer::IsNewSegmentSampleAfterTune(UCHAR bPinIndex, IMediaSample &riMediaSample) const
{
	return false;
} // CSampleProducer::IsNewSegmentSampleAfterTune

bool CSampleProducer::IsRecordingKnown(const wchar_t *pwszRecordingName)
{
	if (!pwszRecordingName || !*pwszRecordingName)
		return false;  // no (valid) recording name argument
	std::wstring recordingName = L"";
	if (!GetRecordingNameFromWriter(recordingName))
		return false;
	return (*(recordingName.c_str()) &&
			(OLESTRCMP(pwszRecordingName, recordingName.c_str()) == 0));
} // CSampleProducer::IsRecordingKnown

void CSampleProducer::ComputeBufferQuota(AM_MEDIA_TYPE *pmt, ULONG &lBufferQuota, ULONG &lMinimumSize)
{
	lBufferQuota = s_lMinimumBufferQuota;
	lMinimumSize = 0;

	try
	{
		std::auto_ptr<CMediaTypeDescription> pcMediaTypeDescription;
		if (pmt && m_piMediaTypeAnalyzer)
		{
			CMediaType cMediaType(*pmt);
			std::auto_ptr<CMediaTypeDescription> pcMediaDescriptionTemp(m_piMediaTypeAnalyzer->AnalyzeMediaType(cMediaType));
			pcMediaTypeDescription = pcMediaDescriptionTemp;
		}
		if (pcMediaTypeDescription.get())
			lMinimumSize = pcMediaTypeDescription->m_uMinimumMediaSampleBufferSize;
		OverrideMinimumSize(pmt, lMinimumSize);
		if (!m_piWriter && m_picpmgr)
		{
			CPipelineRouter	cPipelineRouter = m_picpmgr->GetRouter(NULL, false, false);
			void *pviWriter = NULL;
			ROUTE eRoute = cPipelineRouter.GetPrivateInterface(IID_IWriter, pviWriter);
			if ((eRoute == HANDLED_STOP) || (eRoute == HANDLED_CONTINUE))
				m_piWriter = static_cast<IWriter*>(pviWriter);
		}
		if (m_piWriter)
		{
			LONGLONG hyWriterLatency = m_piWriter->GetMaxSampleLatency();
			if (pcMediaTypeDescription.get())
			{
				LONGLONG hyLatencyIn100NsUnits = hyWriterLatency + s_hyOverlapIn100NsUnits
					+ pcMediaTypeDescription->m_hyMaxTimeUnitsBetweenKeyFrames;
				lBufferQuota = (ULONG) ((hyLatencyIn100NsUnits * pcMediaTypeDescription->m_uMaximumSamplesPerSecond)
								/ s_hy100NsPerSecond);
#ifdef DEBUG
				g_hyMinimumTimeSpanInQueue = hyLatencyIn100NsUnits;
#endif // DEBUG
			}
			OverrideWriterLatencyToBufferCount(pmt, hyWriterLatency, lBufferQuota);
			if (lBufferQuota < s_lMinimumBufferQuota)
				lBufferQuota = s_lMinimumBufferQuota;
		}
	}
	catch (const std::exception& rcException)
	{
		UNUSED (rcException);  // suppress release build warning
#ifdef UNICODE
		DbgLog((LOG_ERROR, 2,
			_T("CSampleProducer(%p)::ComputeBufferQuota() caught exception %S\n"),
					this, rcException.what()));
#else
		DbgLog((LOG_ERROR, 2,
			_T("CSampleProducer(%p)::ComputeBufferQuota() caught exception %s\n"),
					this, rcException.what()));
#endif
	}
} // CSampleProducer::ComputeBufferQuota

void CSampleProducer::NotifyPinUpdate()
{
}

void CSampleProducer::OverrideMinimumSize(AM_MEDIA_TYPE *pmt, ULONG &uProposedMinimumSize)
{
}

void CSampleProducer::OverrideWriterLatencyToBufferCount(
				AM_MEDIA_TYPE *pmt,
				LONGLONG hyWriterLatencyIn100NsUnits,
				ULONG uProposedBufferCount)
{
}

void CSampleProducer::NotifyConsumersOfRecordingStart(LONGLONG hyPreRecordingPos, LONGLONG hyRecordingStart)
{
	if (m_pwszCompletedPermRecording.empty())
		return;

	CSampleConsumerBindingIterator iter(m_cSampleConsumerBindings);

	while (iter.MoveToNext())
	{
		try {
			CSampleConsumerState &rcSampleConsumerState = iter.GetCurrent();
			rcSampleConsumerState.m_piSampleConsumer->NotifyRecordingWillStart(this, hyRecordingStart,
				hyPreRecordingPos, m_pwszCompletedPermRecording);
		}
		catch (const std::exception& rcException)
		{
			UNUSED (rcException);  // suppress release build warning
#ifdef UNICODE
			DbgLog((LOG_ERROR, 2,
				_T("CSampleProducer(%p)::NotifyConsumersOfPauseBufferChange() caught exception %S\n"),
					this, rcException.what()));
#else
			DbgLog((LOG_ERROR, 2,
				_T("CSampleProducer(%p)::NotifyConsumersOfPauseBufferChange() caught exception %s\n"),
					this, rcException.what()));
#endif
		};
	}
}

LONGLONG CSampleProducer::GetAverageSampleInterval()
{
	return m_cSampleProducerSampleHistory.m_hyLastDeltaMediaStart;
}
