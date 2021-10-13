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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#include "stdafx.h"
#include "..\\PauseBuffer.h"
#include "SampleConsumer.h"
#include "..\\SampleProducer\\CPauseBufferHistory.h"
#include "..\\DVREngine.h"
#include "..\\SampleProducer\\CPinMapping.h"
#include "..\\SampleProducer\\ProducerConsumerUtilities.h"
#include "..\\ComponentWorkerThread.h"
#include "..\\Plumbing\\Source.h"
#include "..\\HResultError.h"
#include "..\\Writer\\DVREngineHelpers.h"
#include <dsthread.h>

#ifndef UNICODE
Like much of the rest of the DVR engine, this code must be built with UNICODE enabled.
That is, OLE chars and tchar's must be wide characters.
#endif

// #define HACK_TO_TEST_IF_IFRAME_ISSUE

// If you want Win/CE kernel tracker events to help sort out what is
// going on with A/V glitches, uncomment the define below:

#ifdef _WIN32_WCE
// #define WIN_CE_KERNEL_TRACKING_EVENTS
#endif

#ifdef WIN_CE_KERNEL_TRACKING_EVENTS
#include "Celog.h"
#endif

using namespace MSDvr;

#define OLESTRLEN(x)        wcslen(x)
#define OLESTRCMP(s1,s2)    wcscmp(s1,s2)

#define STOP_POSITION_NULL_VALUE  0x7fffffffffffffff
#define CURRENT_POSITION_NULL_VALUE -1

#define POST_FLUSH_POSITION_PENDING -2
#define POST_FLUSH_POSITION_INVALID -1

#define MINIMUM_REWIND_DISTANCE     10000000LL

#define ALLOWANCE_FOR_CARRYING_OUT_GO_LIVE  6000000LL
    // When asked to go to the live position, we know how far behind the MPEG encoder
    // we must wind up after the trick mode completes. Trick modes always go to positions
    // aligned with the next I-Frame after the target position. So we will wind up
    // 0 to 500 ms after the target. There's also some variation in position due to
    // the time needed to get things going again nicely when implementing the trick
    // mode. Empirically, that variation is roughly 100-150 ms.  So put together, we need
    // to aim for a target about 500+100=600 ms before the boundary to unsafe playback
    // in order to give us a high probability of landing at a good position up-front.
    // If we miss, there is downstream code to correct the matter, but correction
    // involves subtle a/v playback games [playing at non-1x rates] that are best
    // avoided.

#define MAX_LIVE_PLAYBACK_RANGE     4LL * 1000LL * 10000LL

LONGLONG CSampleConsumer::s_hyUnsafelyCloseToWriter = -1;
LONGLONG CSampleConsumer::s_hyUnsafelyCloseToTruncationConstant = -1;   // Default value to force init during constructor
LONGLONG CSampleConsumer::s_hySafetyMarginSeekVersusPlay = 10000000;    // Add 1 more second if seeking
LONGLONG CSampleConsumer::s_hyUnsafelyCloseToTruncationRate = 2000000;  // Allow an extra margin for faster rates -- empirically 200 ms per each 1x
LONGLONG CSampleConsumer::s_hyOmitSeekOptimizationWindow = 20000000;   // seeking to 2 seconds before 'now' is more of an a/v glitch than useful as going-to-live

#if 0
// Only enable this if you are debugging a system too unstable to run a long time:
#define CHECK_POSITION(x)   ASSERT(x < (LONGLONG)0x80000000000)
#else
#define CHECK_POSITION(x)   
#endif

///////////////////////////////////////////////////////////////////////
//
// Helper class CConsumerStreamingThread:
//
///////////////////////////////////////////////////////////////////////

namespace MSDvr
{
    template<typename T>
    class StateManager {
    public:
        StateManager(T *pT)
            : m_pT(pT)
            , m_OrigState(*pT)
            , m_fConfirmed(false)
        {
        }

        ~StateManager()
        {
            if (!m_fConfirmed)
                *m_pT = m_OrigState;
        }

        void SetState(T newState)
        {
            *m_pT = newState;
        }

        void ConfirmState(T finalState)
        {
            *m_pT = finalState;
            m_fConfirmed = true;
        }

    protected:
        T *m_pT;
        T m_OrigState;
        bool m_fConfirmed;
    };

    class CConsumerStreamingThread : public CComponentWorkerThread
    {
    public:
        enum CONSUMER_STREAM_EVENT_TYPE {
            CONSUMER_EVENT_PRODUCER_SAMPLE,
            CONSUMER_EVENT_PRODUCER_CACHE_DONE,
            CONSUMER_EVENT_PRODUCER_MODE_CHANGE
        };

        struct SConsumerStreamEvent
        {
            CONSUMER_STREAM_EVENT_TYPE eConsumerStreamEventType;
            UCHAR bPinIndex;
            LONGLONG hyMediaSampleId;
            LONGLONG hyMediaStartPos;
            LONGLONG hyMediaEndPos;
            ULONG uModeVersionCount;
            CSmartRefPtr<CPauseBufferData> pPauseBuffer;
        };

        CConsumerStreamingThread(CSampleConsumer &rcSampleConsumer);

        void SendCacheDoneEvent(ULONG uModeVersionCount);
        void SendSampleEvent(UCHAR bPinIndex, LONGLONG hyMediaSampleId,
                ULONG uModeVersionCount,
                LONGLONG hyMediaStartPos, LONGLONG hyMediaEndPos);
        void SendProducerModeEvent();
        void FlushEvents();
        void FlushOldSamples(LONGLONG hyFirstSampleRetained);

    protected:
        ~CConsumerStreamingThread();
        virtual DWORD ThreadMain();
        virtual void OnThreadExit(DWORD &dwOutcome);

        CSampleConsumer &m_rcSampleConsumer;
        std::list<SConsumerStreamEvent> m_listSConsumerStreamEvents;
        CEventHandle m_hEventHandle;
        CCritSec m_cCritSecThreadQueue;
        bool m_fNeedPauseAck;
    };
}
CConsumerStreamingThread::CConsumerStreamingThread(CSampleConsumer &rcSampleConsumer)
    : CComponentWorkerThread(NULL)
    , m_rcSampleConsumer(rcSampleConsumer)
    , m_listSConsumerStreamEvents()
    , m_hEventHandle(TRUE, FALSE)
    , m_cCritSecThreadQueue()
    , m_fNeedPauseAck(false)
{
    if (!m_hEventHandle)
    {
        DbgLog((LOG_ERROR, 2, _T("CConsumerStreamingThread(%p)::CConsumerStreamingThread() CreateEvent() failed\n"), this));
        throw CHResultError(GetLastError(), "CreateEvent() failed");
    }
} // CConsumerStreamingThread::CConsumerStreamingThread

CConsumerStreamingThread::~CConsumerStreamingThread()
{
    FlushEvents();
}

void CConsumerStreamingThread::SendCacheDoneEvent(ULONG uModeVersionCount)
{
    SConsumerStreamEvent sConsumerStreamEvent;
    sConsumerStreamEvent.eConsumerStreamEventType = CONSUMER_EVENT_PRODUCER_CACHE_DONE;
    sConsumerStreamEvent.uModeVersionCount = uModeVersionCount;

    CAutoLock cAutoLock(&m_cCritSecThreadQueue);
    m_listSConsumerStreamEvents.push_back(sConsumerStreamEvent);
    SetEvent(m_hEventHandle);
}

void CConsumerStreamingThread::SendProducerModeEvent()
{
    SConsumerStreamEvent sConsumerStreamEvent;
    sConsumerStreamEvent.eConsumerStreamEventType = CONSUMER_EVENT_PRODUCER_MODE_CHANGE;

    CAutoLock cAutoLock(&m_cCritSecThreadQueue);
    m_listSConsumerStreamEvents.push_back(sConsumerStreamEvent);
    SetEvent(m_hEventHandle);
}

void CConsumerStreamingThread::SendSampleEvent(UCHAR bPinIndex, LONGLONG hyMediaSampleId,
                                               ULONG uModeVersionCount,
                                                LONGLONG hyMediaStartPos, LONGLONG hyMediaEndPos)
{
    SConsumerStreamEvent sConsumerStreamEvent;
    sConsumerStreamEvent.eConsumerStreamEventType = CONSUMER_EVENT_PRODUCER_SAMPLE;
    sConsumerStreamEvent.bPinIndex = bPinIndex;
    sConsumerStreamEvent.hyMediaSampleId = hyMediaSampleId;
    sConsumerStreamEvent.uModeVersionCount = uModeVersionCount;
    sConsumerStreamEvent.hyMediaStartPos = hyMediaStartPos;
    sConsumerStreamEvent.hyMediaEndPos = hyMediaEndPos;

    CAutoLock cAutoLock(&m_cCritSecThreadQueue);
    m_listSConsumerStreamEvents.push_back(sConsumerStreamEvent);
    SetEvent(m_hEventHandle);
}

void CConsumerStreamingThread::FlushEvents()
{
    CAutoLock cAutoLock(&m_cCritSecThreadQueue);
    m_listSConsumerStreamEvents.clear();
}

void CConsumerStreamingThread::FlushOldSamples(LONGLONG hyFirstSampleRetained)
{
    CAutoLock cAutoLock(&m_cCritSecThreadQueue);
    std::list<SConsumerStreamEvent>::iterator iter;

    for (iter = m_listSConsumerStreamEvents.begin();
         iter != m_listSConsumerStreamEvents.end();
         )
    {
        SConsumerStreamEvent sConsumerStreamEvent = *iter;
        if ((sConsumerStreamEvent.eConsumerStreamEventType == CONSUMER_EVENT_PRODUCER_SAMPLE) &&
            ((hyFirstSampleRetained < 0) || (sConsumerStreamEvent.hyMediaSampleId < hyFirstSampleRetained)))
        {
            iter = m_listSConsumerStreamEvents.erase(iter);
        }
        else
            ++iter;
    }
}

DWORD CConsumerStreamingThread::ThreadMain()
{
    DWORD dwWaitResult;
    HANDLE phEvents[2];
    phEvents[0] = m_rcSampleConsumer.m_pippmgr->GetFlushEvent();
    phEvents[1] = m_hEventHandle;
    BOOL fBackgroundMode = FALSE;
    
    while (!m_fShutdownSignal)
    {
        if (fBackgroundMode != m_rcSampleConsumer.m_fEnableBackgroundPriority)
        {
            fBackgroundMode = m_rcSampleConsumer.m_fEnableBackgroundPriority;
            _internal_SetThreadPriorityEx(GetCurrentThread(), fBackgroundMode ? THREAD_PRIORITY_BELOW_NORMAL : THREAD_PRIORITY_NORMAL);
        }

        dwWaitResult = WaitForMultipleObjects(2, phEvents, FALSE, 500);
        m_rcSampleConsumer.ProcessPauseBufferUpdates();
        if (dwWaitResult == WAIT_OBJECT_0 + 1)
        {
            std::list<SConsumerStreamEvent>::iterator iter;

            CAutoLock cAutoLock(&m_cCritSecThreadQueue);
            ResetEvent(m_hEventHandle);
            while (!m_fShutdownSignal && !m_rcSampleConsumer.m_fFlushing &&
                   ((iter = m_listSConsumerStreamEvents.begin()) != m_listSConsumerStreamEvents.end()))
            {
                try {
                    SConsumerStreamEvent sConsumerStreamEvent = *iter;
                    iter = m_listSConsumerStreamEvents.erase(iter);
                    CAutoUnlock cAutoUnlock(m_cCritSecThreadQueue);

                    switch (sConsumerStreamEvent.eConsumerStreamEventType)
                    {
                    case CONSUMER_EVENT_PRODUCER_SAMPLE:
                        // Don't lock here -- establish the lock inside
                        // DoProcessProducerSample() after we've finished
                        // the calls that would require the producer to
                        // establish its lock.
                        m_rcSampleConsumer.DoProcessProducerSample(
                            sConsumerStreamEvent.bPinIndex,
                            sConsumerStreamEvent.hyMediaSampleId,
                            sConsumerStreamEvent.uModeVersionCount,
                            sConsumerStreamEvent.hyMediaStartPos,
                            sConsumerStreamEvent.hyMediaEndPos);
                        break;

                    case CONSUMER_EVENT_PRODUCER_CACHE_DONE:
                        m_rcSampleConsumer.DoNotifyProducerCacheDone(
                            sConsumerStreamEvent.uModeVersionCount);
                        break;

                    case CONSUMER_EVENT_PRODUCER_MODE_CHANGE:
                        m_rcSampleConsumer.DoUpdateProducerMode();
                        break;
                    }
                }
                catch (const std::exception& rcException)
                {
                    UNUSED (rcException);  // suppress release build warning
#ifdef UNICODE
                    DbgLog((LOG_ERROR, 2,
                        _T("CConsumerStreamingThread(%p)::ThreadMain() caught exception %S\n"),
                        this, rcException.what()));
#else
                    DbgLog((LOG_ERROR, 2,
                        _T("CConsumerStreamingThread(%p)::ThreadMain() caught exception %s\n"),
                        this, rcException.what()));
#endif
                };
            }
        }
        else if (dwWaitResult == WAIT_OBJECT_0)
        {
            DbgLog((LOG_SOURCE_STATE, 3, _T("CConsumerStreamingThread(%p)::ThreadMain() detected flush, will block\n"), this));
            m_rcSampleConsumer.BeginFlush(true);
            bool fContinueAfterFlush = m_rcSampleConsumer.m_pippmgr->WaitEndFlush();
            SetEvent(m_hEventHandle);
            if (!fContinueAfterFlush)
            {
                DbgLog((LOG_SOURCE_STATE, 3, _T("CConsumerStreamingThread(%p)::ThreadMain() exiting because WaitEndFlush() returned false\n"), this));
                m_fShutdownSignal = true;
                m_rcSampleConsumer.EndFlush(true, true);
                break;
            }
            m_rcSampleConsumer.EndFlush(false, true);
            DbgLog((LOG_SOURCE_STATE, 3, _T("CConsumerStreamingThread(%p)::ThreadMain() flush done, resuming\n"), this));
        }
        m_rcSampleConsumer.ResumeNormalPlayIfNeeded();
    }
    return 0;
}

void CConsumerStreamingThread::OnThreadExit(DWORD &dwOutcome)
{
    CAutoLock cAutoLock(&m_rcSampleConsumer.m_cCritSecThread);
    if (m_rcSampleConsumer.m_pcConsumerStreamingThread == this)
    {
        Release();
        m_rcSampleConsumer.m_pcConsumerStreamingThread = NULL;
    }
}

///////////////////////////////////////////////////////////////////////
//
// Helper class CProducerSampleState:
//
///////////////////////////////////////////////////////////////////////

CSampleConsumer::CProducerSampleState::CProducerSampleState(void)
    : m_pcDVROutputPin(0)
    , m_llLatestMediaStart(-1)
    , m_llLatestMediaEnd(-1)
    , m_fProducerInFlush(false)
{
} // CSampleConsumer::CProducerSampleState::CProducerSampleState

CSampleConsumer::CProducerSampleState::~ CProducerSampleState(void)
{
} // CSampleConsumer::CProducerSampleState::~ CProducerSampleState

CSampleConsumer::CProducerSampleState::CProducerSampleState(const CProducerSampleState &rcProducerSampleState)
    : m_pcDVROutputPin(rcProducerSampleState.m_pcDVROutputPin)
    , m_llLatestMediaStart(rcProducerSampleState.m_llLatestMediaStart)
    , m_llLatestMediaEnd(rcProducerSampleState.m_llLatestMediaEnd)
    , m_fProducerInFlush(rcProducerSampleState.m_fProducerInFlush)
{
}

CSampleConsumer::CProducerSampleState &
    CSampleConsumer::CProducerSampleState::operator =(const CProducerSampleState &rcProducerSampleState)
{
    m_pcDVROutputPin = rcProducerSampleState.m_pcDVROutputPin;
    m_llLatestMediaStart = rcProducerSampleState.m_llLatestMediaStart;
    m_llLatestMediaEnd = rcProducerSampleState.m_llLatestMediaEnd;
    m_fProducerInFlush = rcProducerSampleState.m_fProducerInFlush;
    return *this;
}

///////////////////////////////////////////////////////////////////////
//
//  Class CConsumerNotifyOnPosition
//
///////////////////////////////////////////////////////////////////////

void CConsumerNotifyOnPosition::OnPosition()
{
    m_pcSampleConsumer->OnPosition(m_hyTargetPosition, m_dblPriorRate, m_uPositionCount,
                                    m_lEventId, m_lParam1, m_lParam2);
}

///////////////////////////////////////////////////////////////////////
//
//  Class CSampleConsumer -- constructor and destructor
//
///////////////////////////////////////////////////////////////////////
CSampleConsumer::CSampleConsumer(void)
    : m_pcPinMappings(0)
    , m_pcPauseBufferHistory()
    , m_pcPauseBufferSegment()
    , m_pcPauseBufferSegmentBound()
    , m_piSampleProducer(0)
    , m_bOutputPins(0)
    , m_vcpssOutputPinMapping()
    , m_pippmgr(0)
    , m_pszFileName(L"")
    , m_pguidCurTimeFormat(TIME_FORMAT_MEDIA_TIME)
    , m_dblRate(1.0)
    , m_dblPreDeferNormalRate(1.0)
    , m_pipbmgrNext(0)
    , m_pifsrcfNext(0)
    , m_pisbmsNext(0)
    , m_esbptp(STRMBUF_STAY_WITH_RECORDING)
    , m_eStrmBufPlaybackTunePolicy(STRMBUF_PLAYBACK_TUNE_FLUSH_AND_GO_TO_LIVE)
    , m_piStreamBufferPlayback(0)
    , m_piReader(0)
    , m_piDecoderDriver(0)
    , m_hyStopPosition(STOP_POSITION_NULL_VALUE)
    , m_hyCurrentPosition(CURRENT_POSITION_NULL_VALUE)
    , m_hyTargetPosition(CURRENT_POSITION_NULL_VALUE)
    , m_pcConsumerStreamingThread(0)
    , m_eSampleProducerMode(PRODUCER_SUPPRESS_SAMPLES)
    , m_eSampleProducerActiveMode(PRODUCER_SUPPRESS_SAMPLES)
    , m_uSampleProducerModeIncarnation(0)
    , m_cCritSecThread()
    , m_cCritSecState()
    , m_fInRateChange(false)
    , m_eSCFilterState(SC_FILTER_NEVER_STARTED)
    , m_eSCLoadState(SC_LOAD_NEEDED)
    , m_eSampleSource(SC_SAMPLE_SOURCE_NONE)
    , m_eSCPipelineState(SC_PIPELINE_NONE)
    , m_eSCStopMode(SC_STOP_MODE_NONE)
    , m_eSCPositionMode(SC_POSITION_UNKNOWN)
    , m_eSCRecordingState(SC_RECORDING_VIEWING_NONE)
    , m_hyDesiredProducerPos(-1)
    , m_fSampleMightBeDiscontiguous(false)
    , m_fFlushing(false)
    , m_fThrottling(true)
    , m_fResumingNormalPlay(false)
    , m_fDroppingToNormalPlayAtBeginning(false)
    , m_fConsumerSetGraphRate(false)
    , m_fPendingFailedRunEOS(false)
    , m_dwResumePlayTimeoutTicks(0)
    , m_hyPostFlushPosition(POST_FLUSH_POSITION_INVALID)
    , m_hyPostTunePosition(POST_FLUSH_POSITION_INVALID)
    , m_uPositionNotifyId(0)
    , m_hyTargetOffsetFromLive(g_rtMinimumBufferingUpstreamOfLive)
    , m_cCritSecPauseBuffer()
    , m_listPauseBufferUpdates()
    , m_dwLoadIncarnation(0)
    , m_fEnableBackgroundPriority(FALSE)
#if defined(SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT)
    , m_rtProducerToConsumerClockOffset(0)
    , m_piGraphClock(NULL)
#endif // SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT

{
    CHECK_POSITION(m_hyCurrentPosition);
    g_cDVREngineHelpersMgr.RegisterConsumer(this);

    if (-1 == s_hyUnsafelyCloseToTruncationConstant)
        {
        // Read value in msec
        DWORD dwValue;
        extern bool GetRegistryOverride(const TCHAR *pszRegistryProperyName, DWORD &dwValue);
        if (GetRegistryOverride(TEXT("PauseBufferSafetyMargin"), dwValue))
            {
            s_hyUnsafelyCloseToTruncationConstant = (LONGLONG)dwValue * (LONGLONG)10000;
            }
        else
            {
            s_hyUnsafelyCloseToTruncationConstant = 50000000;  // 5 seconds is dangerously close due to clock corrections
            }
        }

    DbgLog((LOG_ENGINE_OTHER, 2, _T("CSampleConsumer: constructed %p\n"), this));
} // CSampleConsumer::CSampleConsumer

CSampleConsumer::~CSampleConsumer(void)
{
    DbgLog((LOG_ENGINE_OTHER, 2, _T("CSampleConsumer: destroying %p\n"), this));

    g_cDVREngineHelpersMgr.UnregisterConsumer(this);

    {
        CAutoLock cAutoLock(&m_cCritSecState);

        Cleanup(SC_INTERNAL_EVENT_DESTRUCTOR);
    }
} // CSampleConsumer::~CSampleConsumer

///////////////////////////////////////////////////////////////////////
//
//  Class CSampleConsumer -- ISampleSource
//
///////////////////////////////////////////////////////////////////////

LONGLONG CSampleConsumer::GetLatestSampleTime() const
{
    LONGLONG hySampleTime = -1;
    if (m_piSampleProducer)
    {
        LONGLONG hyMinPosition;
        m_piSampleProducer->GetPositionRange(hyMinPosition, hySampleTime);
    }
    return hySampleTime;
} // CSampleConsumer::GetLatestSampleTime()

LONGLONG CSampleConsumer::GetProducerTime() const
{
    LONGLONG hyTimeNow = 0;
    if (m_piSampleProducer)
    {
        hyTimeNow = m_piSampleProducer->GetProducerTime();
    }
    return hyTimeNow;
} // CSampleConsumer::GetProducerTime()

///////////////////////////////////////////////////////////////////////
//
//  Class CSampleConsumer -- IPipelineComponent
//
///////////////////////////////////////////////////////////////////////

ROUTE CSampleConsumer::DispatchExtension(
                        // @parm The extension to be dispatched.
                        CExtendedRequest &rcExtendedRequest)
{
    // Redispatch for correct interleaving:

    if (m_pippmgr)
    {
        m_pippmgr->GetRouter(this, false, true).DispatchExtension(rcExtendedRequest);
        return HANDLED_STOP;
    }
    return UNHANDLED_CONTINUE;
}

void CSampleConsumer::RemoveFromPipeline()
{
    DbgLog((LOG_ENGINE_OTHER, 3, _T("CSampleConsumer(%p)::RemoveFromPipeline\n"), this));

    // We need to make sure that the pipeline is stopped before we hold onto
    // m_cCritSecState. Since Cleanup() will try to stop the streaming thread,
    // do it now first (preemptively):
    StopStreamingThread();

    CAutoLock cAutoLock(&m_cCritSecState);

    ASSERT(m_eSCPipelineState == SC_PIPELINE_NORMAL);
    StateManager<SAMPLE_CONSUMER_PIPELINE_STATE> tStateManager(&m_eSCPipelineState);
    tStateManager.SetState(SC_PIPELINE_REMOVING);

    Cleanup(SC_INTERNAL_EVENT_LOST_PIPELINE);
    
    tStateManager.ConfirmState(SC_PIPELINE_NONE);
} // CSampleConsumer::RemoveFromPipeline

ROUTE CSampleConsumer::GetPrivateInterface(REFIID riid, void *&rpInterface)
{
    ROUTE result = UNHANDLED_CONTINUE;

    if (riid == IID_ISampleConsumer)
    {
        rpInterface = ((ISampleConsumer *)this);
        result = HANDLED_STOP;
    }
    else if (riid == IID_IPauseBufferMgr)
    {
        rpInterface = ((IPauseBufferMgr*) this);
        result = HANDLED_STOP;
    }
    return result;
} // CSampleConsumer::GetPrivateInterface

ROUTE CSampleConsumer::NotifyFilterStateChange(FILTER_STATE eState)
{
    DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer(%p)::NotifyFilterStateChange(%d)\n"), this, eState));

    switch (eState)
    {
    case State_Stopped:
        {
            CAutoLock cAutoLock(&m_cCritSecState);

            Deactivate();
        }
        StopStreamingThread();
        break;

    case State_Paused:
        {
            CAutoLock cAutoLock(&m_cCritSecState);

            if (Stopped())
                Activate();
            else if (Running())
                Pause();
#if defined(SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT)
            m_rtProducerToConsumerClockOffset = 0;
            m_piGraphClock = NULL;
#endif // SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT
        }
        break;

    case State_Running:
        {
            CAutoLock cAutoLock(&m_cCritSecState);

            Run();
        }
        break;
    }
    return HANDLED_CONTINUE;
} // CSampleConsumer::NotifyFilterStateChange

ROUTE CSampleConsumer::ConfigurePipeline(
    UCHAR iNumPins,
    CMediaType cMediaTypes[],
    UINT iSizeCustom,
    BYTE Custom[])
{
    DbgLog((LOG_ENGINE_OTHER, 3, _T("CSampleConsumer(%p)::ConfigurePipeline()\n"), this));

    // This is the point that we grab references to other pipeline and
    // filter components -- the pipeline has been fully configured and we're
    // still safely running on an app thread.
    //
    CAutoLock cAutoLock(&m_cCritSecState);

    IReader *piReader = GetReader();
    if (!piReader)
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::ConfigurePipeline() -- no reader\n"), this));
        throw CHResultError(E_FAIL, "No reader found");
    }

    // fyi:  the reader starts off in a streaming state. We've got to stop it
    // before it sends us unwanted samples. We don't know yet if we're going
    // to want samples from the producer or the consumer. This is as early as
    // we have the all of the right information and as late as we can wait.
    //
    if (!DeliveringReaderSamples())
        piReader->SetIsStreaming(false);

    if (!GetDecoderDriver())
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::ConfigurePipeline() -- no decoder driver\n"), this));
        throw CHResultError(E_FAIL, "No decoder driver found");
    }

    return HANDLED_CONTINUE;
} // CSampleConsumer::ConfigurePipeline

/* IPlaybackPipelineComponent: */
unsigned char CSampleConsumer::AddToPipeline(
    IPlaybackPipelineManager &rcManager)
{
    DbgLog((LOG_ENGINE_OTHER, 3, _T("CSampleConsumer(%p)::AddToPipeline()\n"), this));
    CAutoLock cAutoLock(&m_cCritSecState);

    ASSERT(m_eSCPipelineState == SC_PIPELINE_NONE);
    StateManager<SAMPLE_CONSUMER_PIPELINE_STATE> tStateManager(&m_eSCPipelineState);
    tStateManager.SetState(SC_PIPELINE_ADDING);

    IPipelineManager *rpManager = (IPipelineManager *) &rcManager;
    m_pippmgr = &rcManager;
    m_cPipelineRouterForSamples= m_pippmgr->GetRouter(this, false, true);
    m_pifsrcfNext = (IFileSourceFilter *)
        rpManager->RegisterCOMInterface(static_cast<TRegisterableCOMInterface<IFileSourceFilter> &>(*this));
    m_pisbmsNext = (IStreamBufferMediaSeeking *)
        rpManager->RegisterCOMInterface(static_cast<TRegisterableCOMInterface<IStreamBufferMediaSeeking> &>(*this));
    m_piStreamBufferPlayback = (IStreamBufferPlayback *)
        rpManager->RegisterCOMInterface(static_cast<TRegisterableCOMInterface<IStreamBufferPlayback> &>(*this));
//  rpManager->RegisterCOMInterface(static_cast<TRegisterableCOMInterface<IMediaSeeking> &>(*this));
    // Private interfaces -- not explicitly registered

    tStateManager.ConfirmState(SC_PIPELINE_NORMAL);

    return 1;
} // CSampleConsumer::AddToPipeline

ROUTE CSampleConsumer::ProcessOutputSample(
    IMediaSample &riSample,
    CDVROutputPin &rcOutputPin)
{
    ROUTE eRoute = UNHANDLED_STOP;

    ProcessPauseBufferUpdates();

    {
        CAutoLock cAutoLock(&m_cCritSecState);

        eRoute = DetermineSampleRoute(riSample, SC_INTERNAL_EVENT_READER_SAMPLE, rcOutputPin);
        // TODO:  Figure out how the below can happen and fix it correctly:
        if ((eRoute == HANDLED_STOP) || (eRoute == UNHANDLED_STOP))
        {
            if ((m_eSampleProducerMode != PRODUCER_SUPPRESS_SAMPLES) &&
                (m_eSampleProducerActiveMode != PRODUCER_SUPPRESS_SAMPLES))
            {
                if (m_piReader)
                    m_piReader->SetIsStreaming(false);
            }
        }
    }

    if ((eRoute == HANDLED_CONTINUE) || (eRoute == UNHANDLED_CONTINUE))
    {
        // Requeue the sample for proper interleaving:

        eRoute = HANDLED_STOP;
        ASSERT(m_pippmgr);
        m_pippmgr->GetRouter(this, false, true).ProcessOutputSample(riSample, rcOutputPin);
    }
    return eRoute;
} // CSampleConsumer::ProcessOutputSample

ROUTE CSampleConsumer::EndOfStream()
{
    ROUTE eRoute = HANDLED_CONTINUE;

    DbgLog((LOG_EVENT_DETECTED, 1, _T("### DVR Source Filter:  reader hit end-of-stream ###\n")));

    ProcessPauseBufferUpdates();
    {
        CAutoLock cAutoLock(&m_cCritSecState);
        if (IsSourceReader() || m_fPendingFailedRunEOS)
        {
            eRoute = UpdateMediaSource(SC_INTERNAL_EVENT_END_OF_STREAM);
        }
        else
        {
            eRoute = HANDLED_STOP;
            if ((m_eSampleProducerMode != PRODUCER_SUPPRESS_SAMPLES) &&
                (m_eSampleProducerActiveMode != PRODUCER_SUPPRESS_SAMPLES))
            {
                if (m_piReader)
                    m_piReader->SetIsStreaming(false);
            }
        }
    }
    if (m_fPendingFailedRunEOS)
    {
        eRoute = HANDLED_CONTINUE;
        m_fPendingFailedRunEOS = false;
    }
    if ((eRoute == HANDLED_CONTINUE) || (eRoute == UNHANDLED_CONTINUE))
    {
        // Redispatch to get correct interleaving:

        m_hyPostFlushPosition = POST_FLUSH_POSITION_INVALID;
        m_hyPostTunePosition = POST_FLUSH_POSITION_INVALID;

        m_pippmgr->GetRouter(this, false, true).EndOfStream();
        eRoute = HANDLED_STOP;
        if (m_piDecoderDriver)
        {
            CDecoderDriverSendNotification *pcDecoderDriverSendNotification =
                new CDecoderDriverSendNotification(
                m_piDecoderDriver->GetPipelineComponent(),
                DVRENGINE_EVENT_RECORDING_END_OF_STREAM,
                (GetTrueRate() < 0) ? DVRENGINE_BEGINNING_OF_RECORDING : DVRENGINE_END_OF_RECORDING,
                m_dwLoadIncarnation,
                true);
            try {
                m_pippmgr->GetRouter(this, false, true).DispatchExtension(*pcDecoderDriverSendNotification);
            } catch (const std::exception &) {};
            pcDecoderDriverSendNotification->Release();
        }
    }
    return eRoute;
} // CSampleConsumer::EndOfStream

///////////////////////////////////////////////////////////////////////
//
//  Class CSampleConsumer -- IPipelineComponentIdleAction
//
///////////////////////////////////////////////////////////////////////

void CSampleConsumer::DoAction()
{
    ProcessPauseBufferUpdates();
    ResumeNormalPlayIfNeeded();
    CompensateForWhackyClock();     // hastens the recovery time if something yanked hard on the clock
} // CSampleConsumer::DoAction

///////////////////////////////////////////////////////////////////////
//
//  Class CSampleConsumer -- ISampleConsumer
//
///////////////////////////////////////////////////////////////////////

void CSampleConsumer::NotifyBound(ISampleProducer *pProducer)
{
    DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer(%p)::NotifyBound(%p)\n"), this, pProducer));
    // Note:  This routine is a callback that is only invoked when the
    // consumer is in the middle of a call to bind to or unbind from a
    // sample producer. Do not bother grabbing the lock again.

    CSmartRefPtr<CPauseBufferData> pcPauseBufferData;
    CSmartRefPtr<CPauseBufferData> pDataWithOrphans;

    m_piSampleProducer = pProducer;
    if (pProducer)
    {
        UpdateProducerState(pProducer->GetSinkState());
        MapPins();

        ClearRecordingHistory();
        pcPauseBufferData.Attach(pProducer->GetPauseBuffer());
        
        LONGLONG hyBoundRecordingEndPos = -1LL;
        pDataWithOrphans.Attach(MergeWithOrphans(hyBoundRecordingEndPos, pcPauseBufferData, false));

        ImportPauseBuffer(pDataWithOrphans, hyBoundRecordingEndPos);
        s_hyUnsafelyCloseToWriter = pProducer->GetReaderWriterGapIn100Ns();
    }
    else
    {
        if (m_pcPauseBufferHistory)
            m_pcPauseBufferHistory->MarkRecordingDone();
        m_vcpssOutputPinMapping.clear();
        m_bOutputPins = 0;
        s_hyUnsafelyCloseToWriter = -1;
    }
    UpdateMediaSource(pProducer ? SC_INTERNAL_EVENT_SINK_BIND : SC_INTERNAL_EVENT_SINK_UNBIND);


    if (m_esbptp == STRMBUF_STAY_WITH_SINK)
    {
        try {
            FirePauseBufferUpdateEvent(pDataWithOrphans);
        }
        catch (const std::exception& ) {};
    }
} // CSampleConsumer::NotifyBound

void CSampleConsumer::SinkFlushNotify(ISampleProducer *piSampleProducer,
                                    UCHAR bPinIndex, bool fIsFlushing)
{
    DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer(%p)::SinkFlushNotify(%p, %u, %u)\n"),
        this, piSampleProducer, (unsigned) bPinIndex, (unsigned) fIsFlushing));
    {
        CAutoLock cAutoLock(&m_cCritSecState);

        if (piSampleProducer != m_piSampleProducer)
            return;

        // Should we ever hold onto samples, now is the time to flush them!

        CProducerSampleState &rcState = MapToSourcePin(bPinIndex);
        rcState.m_fProducerInFlush = fIsFlushing;
    }

}

void CSampleConsumer::ProcessProducerSample(
    ISampleProducer *piSampleProducer,
    LONGLONG hyMediaSampleId,
    ULONG uModeVersionCount,
    LONGLONG hyMediaStartPos,
    LONGLONG hyMediaEndPos,
    UCHAR bPinIndex)
{
    // Note:  because we now change m_piSampleProducer once the graph has been stopped
    //        [because Load() is only allowed while the graph is stopped], we don't
    //        have to worry about races between this routine and changing the producer
    //        while there is a streaming thread. Performance is critical, so take
    //        advantage of this situation by not claiming m_cCritSecState.

    if (piSampleProducer != m_piSampleProducer)
        return;

    CAutoLock cAutoLock2(&m_cCritSecThread);

    if (m_pcConsumerStreamingThread)
        m_pcConsumerStreamingThread->SendSampleEvent(bPinIndex, hyMediaSampleId,
            uModeVersionCount, hyMediaStartPos, hyMediaEndPos);
} // CSampleConsumer::ProcessProducerSample

void CSampleConsumer::ProcessProducerTune(ISampleProducer *piSampleProducer,
        LONGLONG hyTuneSampleStartPos)
{
    DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer(%p)::ProcessProducerTune(%p, %I64d)\n"),
        this, piSampleProducer, hyTuneSampleStartPos));

    // Note:  because we now change m_piSampleProducer once the graph has been stopped
    //        [because Load() is only allowed while the graph is stopped], we don't
    //        have to worry about races between this routine and changing the producer
    //        while there is a streaming thread. Performance is critical, so take
    //        advantage of this situation by not claiming m_cCritSecState.

    if (piSampleProducer != m_piSampleProducer)
        return;


    switch (m_esbptp)
    {
    case STRMBUF_STAY_WITH_RECORDING:
        // We are viewing a recording. If we are indeed bound to the recording
        // we do not need to do anything. If we want to be bound to a recording
        // but haven't picked one to be bound to, we don't do anything here either.
        break;

    case STRMBUF_STAY_WITH_SINK:
        if (m_hyCurrentPosition >= 0){
            IDecoderDriver *piDecoderDriver = GetDecoderDriver();
            if (piDecoderDriver)
            {
                // If we were to lock m_cCritSecState, we would have to unlock here
                // to avoid deadlock:
                //
                // CAutoUnlock cAutoUnlock(m_cCritSecState);

                if (m_eStrmBufPlaybackTunePolicy == STRMBUF_PLAYBACK_TUNE_FLUSH_AND_GO_TO_LIVE)
                    m_hyPostTunePosition = hyTuneSampleStartPos;
                piDecoderDriver->ImplementTuneEnd(
                    (m_eStrmBufPlaybackTunePolicy == STRMBUF_PLAYBACK_TUNE_FLUSH_AND_GO_TO_LIVE),
                    hyTuneSampleStartPos, true);
            }
        }
        break;
    }
} // CSampleConsumer::ProcessProducerTune

void CSampleConsumer::SeekToTunePosition(LONGLONG hyChannelStartPos)
{
    CAutoLock cAutoLock(&m_cCritSecState);

    LONGLONG hyTrueStartPos = hyChannelStartPos;
    LONGLONG hyMinPos, hyCurPos, hyMaxPos;
    HRESULT hr = GetConsumerPositionsExternal(hyMinPos, hyCurPos, hyMaxPos, false);
    if (SUCCEEDED(hr))
    {
        // We need to be sure that we don't get unsafely close to the start
        // of the buffer:

        m_hyPostTunePosition = hyTrueStartPos;

        if (hyChannelStartPos > hyMaxPos - m_hyTargetOffsetFromLive)
            hyChannelStartPos = hyMaxPos - m_hyTargetOffsetFromLive;

        // We don't want to seek if we're already sufficiently close to the
        // seek point:

        if (hyCurPos >= hyMaxPos - (m_hyTargetOffsetFromLive + ALLOWANCE_FOR_CARRYING_OUT_GO_LIVE))
        {
            DbgLog((LOG_SOURCE_STATE, 1, TEXT("CSampleConsumer::SeekToTunePosition() ... no-op'ing, saying safe\n") ));
            return;  // don't bother
        }
    }

    SetCurrentPosition(NULL, hyChannelStartPos, true, false, true, true);
    m_hyPostTunePosition = hyTrueStartPos;
} // CSampleConsumer::SeekToTunePosition

void CSampleConsumer::SinkStateChanged(
        ISampleProducer *piSampleProducer, PRODUCER_STATE eProducerState)
{
    DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer(%p)::SinkStateChanged(%p, %d)\n"),
        this, piSampleProducer, (int) eProducerState));
    
    CAutoLock cAutoLock(&m_cCritSecState);

    if (piSampleProducer != m_piSampleProducer)
        return;

    if (eProducerState == PRODUCER_IS_DEAD)
        CleanupProducer();
    else
        UpdateProducerState(eProducerState);

    UpdateMediaSource(SC_INTERNAL_EVENT_SINK_STATE);
} // CSampleConsumer::SinkStateChanged

void CSampleConsumer::PauseBufferUpdated(
    ISampleProducer *piSampleProducer,
    CPauseBufferData *pData)
{
    DbgLog((LOG_PAUSE_BUFFER, 3, _T("CSampleConsumer(%p)::PauseBufferUpdated(%p, %p)\n"),
        this, piSampleProducer, pData));

    if (piSampleProducer != m_piSampleProducer)
        return;

    try
    {
        CAutoLock cAutoLock(&m_cCritSecPauseBuffer);

        SPendingPauseBufferUpdate sPendingPauseBufferUpdate;
        sPendingPauseBufferUpdate.pcPauseBufferData = pData;
        m_listPauseBufferUpdates.push_back(sPendingPauseBufferUpdate);
    }
    catch (const std::exception& )
    {
    }
} // CSampleConsumer::PauseBufferUpdated

void CSampleConsumer::NotifyProducerCacheDone(ISampleProducer *piSampleProducer,
                                              ULONG uModeVersionCount)
{
    DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer(%p)::NotifyProducerCacheDone(%p, %u)\n"),
        this, piSampleProducer, uModeVersionCount));

    // Note:  because we now change m_piSampleProducer once the graph has been stopped
    //        [because Load() is only allowed while the graph is stopped], we don't
    //        have to worry about races between this routine and changing the producer
    //        while there is a streaming thread. Performance is critical, so take
    //        advantage of this situation by not claiming m_cCritSecState.

    if (piSampleProducer != m_piSampleProducer)
        return;

    CAutoLock cAutoLock2(&m_cCritSecThread);

    if (m_pcConsumerStreamingThread)
        m_pcConsumerStreamingThread->SendCacheDoneEvent(uModeVersionCount);
} // CSampleConsumer::NotifyProducerCacheDone

void CSampleConsumer::SetPositionFlushComplete(LONGLONG hyPosition, bool fSeekToKeyFrame)
{
//  m_hyCurrentPosition = m_hyTargetPosition;
    DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetPositionFlushComplete(%I64d, %u)\n"),
        hyPosition, (unsigned) fSeekToKeyFrame));
    m_fSampleMightBeDiscontiguous = true;
    m_fDroppingToNormalPlayAtBeginning = false;
    SetCurrentPosition(NULL, hyPosition, fSeekToKeyFrame, true, false, true);
    DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetPositionFlushComplete():  now at %I64d using %s, producer mode %d (%d active)\n"),
        m_hyCurrentPosition,
        IsSourceReader() ? _T("reader") : ( IsSourceProducer() ? _T("producer") : _T("nothing") ),
        m_eSampleProducerActiveMode, m_eSampleProducerMode ));
}

void CSampleConsumer::NotifyCachePruned(ISampleProducer *piSampleProducer, LONGLONG hyFirstSampleIDRetained)
{
    if (piSampleProducer != m_piSampleProducer)
        return;

    CAutoLock cAutoLock(&m_cCritSecThread);

    if (m_pcConsumerStreamingThread)
        m_pcConsumerStreamingThread->FlushOldSamples(hyFirstSampleIDRetained);
}

void CSampleConsumer::NotifyRecordingWillStart(ISampleProducer *piSampleProducer,
    LONGLONG hyRecordingStartPosition, LONGLONG hyPriorRecordingEnd,
    const std::wstring pwszCompletedRecordingName)
{
#ifdef UNICODE
    DbgLog((LOG_PAUSE_BUFFER, 3,
        _T("CSampleConsumer::NotifyRecordingWillStart(%p, %I64d, %I64d, %s)\n"),
        piSampleProducer, hyRecordingStartPosition, hyPriorRecordingEnd, pwszCompletedRecordingName.c_str()));
#else
    DbgLog((LOG_PAUSE_BUFFER, 3,
        _T("CSampleConsumer::NotifyRecordingWillStart(%p, %I64d, %I64d, %S)\n"),
        piSampleProducer, hyRecordingStartPosition, hyPriorRecordingEnd, pwszCompletedRecordingName.c_str()));
#endif

    CAutoLock cAutoLock(&m_cCritSecState);

    // FYI:  It is possible that the producer guessed wrong -- if the producer
    // has just ok'd a sample that is a key frame, it would've told the consumer
    // but the writer may not have seen the sample when the Begin...Recording()
    // call came in. Thus, the recording might start with the sample already
    // in the hands of the consumer and m_hyEndTime would've been updated to
    // that value.  We need to back-track on the current position in this case.
    if ((STRMBUF_STAY_WITH_RECORDING == m_esbptp) &&
        (piSampleProducer == m_piSampleProducer) &&
        (SC_LOAD_NEEDED != m_eSCLoadState) &&
        m_pcPauseBufferSegmentBound &&
        (pwszCompletedRecordingName.compare(m_pcPauseBufferSegmentBound->GetRecordingName()) == 0) &&
        (m_pcPauseBufferSegmentBound->m_fInProgress ||
        (m_pcPauseBufferSegmentBound->m_hyEndTime > hyPriorRecordingEnd)))
    {
        DbgLog((LOG_PAUSE_BUFFER, 3,
            _T("CSampleConsumer::NotifyRecordingWillStart():  noting the end of the recording-in-progress, currently at %I64d\n"),
            m_hyCurrentPosition));
        m_pcPauseBufferSegmentBound->m_hyEndTime = hyPriorRecordingEnd;
        if (m_hyCurrentPosition > hyPriorRecordingEnd)
        {
            CHECK_POSITION(hyPriorRecordingEnd);
            m_hyCurrentPosition = hyPriorRecordingEnd;
        }
        m_pcPauseBufferSegmentBound->m_fInProgress = false;
        if (STOP_POSITION_NULL_VALUE == m_hyStopPosition)
        {
            m_hyStopPosition = hyPriorRecordingEnd;
            if ((m_hyCurrentPosition >= m_hyStopPosition) && (GetTrueRate() > 0.0))
            {
                StopGraph(PLAYBACK_AT_END);
            }
        }
    }
} // CSampleConsumer::NotifyRecordingWillStart

void CSampleConsumer::NotifyEvent(long lEvent, long lParam1, long lParam2)
{
    try {
        IDecoderDriver *piDecoderDriver = GetDecoderDriver();
        if (piDecoderDriver)
        {
            piDecoderDriver->IssueNotification(this, lEvent, lParam1, lParam2, true, false);
        }
    } catch (const std::exception &) {};
} // CSampleConsumer::NotifyEvent

///////////////////////////////////////////////////////////////////////
//
//  Class CSampleConsumer -- IPauseBufferMgr
//
///////////////////////////////////////////////////////////////////////

CPauseBufferData* CSampleConsumer::GetPauseBuffer()
{
    DbgLog((LOG_PAUSE_BUFFER, 3, _T("CSampleConsumer(%p)::GetPauseBuffer()\n"), this));
    CAutoLock cAutoLock(&m_cCritSecState);

    if (!m_piSampleProducer)
        return 0;

    CSmartRefPtr<CPauseBufferData> pcPauseBufferData;
    pcPauseBufferData.Attach(m_piSampleProducer->GetPauseBuffer());

    CSmartRefPtr<CPauseBufferData> pDataWithOrphans;
    LONGLONG hyBoundRecordingEndPos = -1LL;
    pDataWithOrphans.Attach(MergeWithOrphans(hyBoundRecordingEndPos, pcPauseBufferData));

    ImportPauseBuffer(pDataWithOrphans, hyBoundRecordingEndPos);
    CPauseBufferData *answer = m_pcPauseBufferHistory ? m_pcPauseBufferHistory->GetPauseBufferData() : 0;
    return answer;
} // CSampleConsumer::GetPauseBuffer

HRESULT CSampleConsumer::ExitRecordingIfOrphan(LPCOLESTR pszRecordingName)
{
    HRESULT hr = S_FALSE;
    bool fGoToLive = false;
    CComPtr<IStreamBufferMediaSeeking> piMediaSeeking;
    CComPtr<IMediaControl> piMediaControl;

    DbgLog((LOG_FILE_OTHER, 3, TEXT("CSampleConsumer::ExitRecordingIfOrphan(%s)\n"), pszRecordingName ));

    {
        CAutoLock cAutoLock(&m_cCritSecState);
        if (m_pcPauseBufferHistory)
        {
            CSmartRefPtr<SampleProducer::CPauseBufferSegment>  pcPauseBufferSegment;
            pcPauseBufferSegment.Attach(m_pcPauseBufferHistory->FindRecording(pszRecordingName));
            if (pcPauseBufferSegment != NULL)
            {
                if (m_esbptp != STRMBUF_STAY_WITH_SINK)
                {
                    DbgLog((LOG_FILE_OTHER, 3, TEXT("CSampleConsumer::ExitRecordingIfOrphan(%s) -- bound to this recording -- rejecting the request\n"), pszRecordingName ));

                    return E_FAIL;
                }
                else if (!pcPauseBufferSegment->m_fIsOrphaned || !pcPauseBufferSegment->m_fIsPermanent)
                {
                    DbgLog((LOG_FILE_OTHER, 3, TEXT("CSampleConsumer::ExitRecordingIfOrphan(%s) -- not a permanent, orphaned recording -- rejecting the request\n"), pszRecordingName ));

                    return E_FAIL;
                }
#ifdef DEBUG
                DbgLog((LOG_PAUSE_BUFFER, 3,
                    _T("CPauseBufferHistory::ExitRecordingIfOrphan(%s):  about to modify the pause buffer\n"),
                    pszRecordingName,
                    pcPauseBufferSegment->GetRecordingName().c_str() ));

                DumpPauseBufferHistory(m_pcPauseBufferHistory);
#endif // DEBUG
                hr = S_OK;
                if (m_pcPauseBufferSegment && (pcPauseBufferSegment == m_pcPauseBufferSegment))
                {
                    fGoToLive = true;
                    CBaseFilter *pcBaseFilter = m_pippmgr ? &m_pippmgr->GetSourceFilter() : NULL;

                    if (!pcBaseFilter)
                    {
                        DbgLog((LOG_FILE_OTHER, 3, TEXT("CSampleConsumer::ExitRecordingIfOrphan(%s) -- can't get the base filter\n"), pszRecordingName ));

                        hr = E_FAIL;
                    }
                    else
                    {
                        hr = pcBaseFilter->QueryInterface(__uuidof(IStreamBufferMediaSeeking), (void**) &piMediaSeeking.p);
                        if (SUCCEEDED(hr) && !piMediaSeeking)
                        {
                            hr = E_FAIL;
                        }
                        if (SUCCEEDED(hr) && !Stopped())
                        {
                            IFilterGraph *piFilterGraph = pcBaseFilter->GetFilterGraph();
                            if (piFilterGraph)
                            {
                                hr = piFilterGraph->QueryInterface(IID_IMediaControl, (void **)&piMediaControl.p);
                                if (SUCCEEDED(hr) && !piMediaControl)
                                {
                                    hr = E_FAIL;
                                }
                                if (FAILED(hr))
                                {
                                    DbgLog((LOG_FILE_OTHER, 3, TEXT("CSampleConsumer::ExitRecordingIfOrphan(%s) -- can't get IMediaControl\n"), pszRecordingName ));
                                }
                            }
                        }
                        else
                        {
                            DbgLog((LOG_FILE_OTHER, 3, TEXT("CSampleConsumer::ExitRecordingIfOrphan(%s) -- can't get IStreamBufferMediaSeeking\n"), pszRecordingName ));
                        }
                    }
                }
                else
                {
                    DbgLog((LOG_FILE_OTHER, 3, TEXT("CSampleConsumer::ExitRecordingIfOrphan(%s) -- discarding an orphaned recording that isn't being played\n"), pszRecordingName ));
                    try {
                        hr = m_pcPauseBufferHistory->DiscardOrphanRecording(pcPauseBufferSegment);
                    } catch (const std::exception &)
                    {
                        hr = E_FAIL;
                    }
                    CSmartRefPtr<CPauseBufferData> pcPauseBufferData;
                    pcPauseBufferData.Attach(m_pcPauseBufferHistory->GetPauseBufferData());
                    try {
                        FirePauseBufferUpdateEvent(pcPauseBufferData);
                    }
                    catch (const std::exception& ) {};
                }
#ifdef DEBUG
                DbgLog((LOG_PAUSE_BUFFER, 3,
                    _T("CPauseBufferHistory::ExitRecordingIfOrphan(%s):  done modifying the pause buffer\n"),
                    pszRecordingName,
                    pcPauseBufferSegment->GetRecordingName().c_str() ));

                DumpPauseBufferHistory(m_pcPauseBufferHistory);
#endif // DEBUG
            }
        }
    }


    if (fGoToLive && SUCCEEDED(hr))
    {
        DbgLog((LOG_FILE_OTHER, 3, TEXT("CSampleConsumer::ExitRecordingIfOrphan(%s) -- trying to go-live\n"), pszRecordingName ));

        piMediaSeeking->SetRate(1.0);
        LONGLONG hyMaxPosition = 0x7fffffffffffffffLL;
        piMediaSeeking->SetPositions(&hyMaxPosition, AM_SEEKING_AbsolutePositioning | AM_SEEKING_SeekToKeyFrame, NULL, AM_SEEKING_NoPositioning);
        if (piMediaControl)
        {
            piMediaControl->Run();
        }
    }
    if (FAILED(hr))
    {
        DbgLog((LOG_FILE_OTHER, 3, TEXT("CSampleConsumer::ExitRecordingIfOrphan(%s) -- failed, error 0x%x\n"), pszRecordingName, hr ));
    }

    return hr;
} // CSampleConsumer::ExitRecordingIfOrphan

void CSampleConsumer::RegisterForNotifications(IPauseBufferCallback *pCallback)
{
    CAutoLock cAutoLock(&m_cCritSecState);

    CPauseBufferMgrImpl::RegisterForNotifications(pCallback);
    if (pCallback)
    {
        CSmartRefPtr<CPauseBufferData> pcPauseBufferData;
        pcPauseBufferData.Attach(GetPauseBuffer());
        if (pcPauseBufferData)
        {
            try {
                pCallback->PauseBufferUpdated(pcPauseBufferData);
            }
            catch (const std::exception& )
            {
            }
        }
    }
} // CSampleConsumer::RegisterForNotifications

void CSampleConsumer::CancelNotifications(IPauseBufferCallback *pCallback)
{
    CAutoLock cAutoLock(&m_cCritSecState);

    CPauseBufferMgrImpl::CancelNotifications(pCallback);
} // CSampleConsumer::CancelNotifications

///////////////////////////////////////////////////////////////////////
//
//  Class CSampleConsumer -- IFileSourceFilter
//
///////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE CSampleConsumer::Load(
    /* [in] */ LPCOLESTR pszFileName,
    /* [unique][in] */ const AM_MEDIA_TYPE *pmt)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::Load()\n"), this));
    HRESULT hr = S_OK;
    HRESULT hrTmp = S_OK;

    {
        CAutoLock cAutoLock(&m_cCritSecState);
        StateManager<SAMPLE_CONSUMER_LOAD_STATE> cStateManager(&m_eSCLoadState);

        cStateManager.SetState(SC_LOAD_IN_PROGRESS);

        if (!Stopped())
            hr = E_FAIL;
        else if (pszFileName && CSampleProducerLocator::IsLiveTvToken(pszFileName))
        {
            // We are authoritative for live-tv loads. So we must load first and
            // then forward:

            hr = DoLoad(true, pszFileName, pmt);
            hrTmp = hr;

            if (SUCCEEDED(hr) && m_pifsrcfNext)
            {
                hrTmp = m_pifsrcfNext->Load(pszFileName, pmt);
                if (hrTmp == E_NOTIMPL)
                    hrTmp = S_OK;
                ASSERT(SUCCEEDED(hrTmp));
            }
        }
        else
        {
            // The reader is authoritative for live-tv loads. So we must forward
            // and then if there no error, mirror the load:

            if (m_pifsrcfNext)
            {
                hr = m_pifsrcfNext->Load(pszFileName, pmt);
                if (hr == E_NOTIMPL)
                    hr = S_OK;
            }
            hrTmp = hr;
            if (SUCCEEDED(hr))
                hrTmp = DoLoad(false, pszFileName, pmt);
        }

        if (SUCCEEDED(hr) && FAILED(hrTmp))
        {
            DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::Load() authoritative load worked, secondary load notification failed with error %d\n"), this, hrTmp));
            ASSERT(0);
        }
        if (SUCCEEDED(hrTmp))
        {
            ++m_dwLoadIncarnation;
            cStateManager.ConfirmState((pszFileName && *pszFileName) ? SC_LOAD_COMPLETE : SC_LOAD_NEEDED);
        }
    }

    return hrTmp;
} // CSampleConsumer::Load

HRESULT STDMETHODCALLTYPE CSampleConsumer::GetCurFile(
    /* [out] */ LPOLESTR *ppszFileName,
    /* [out] */ AM_MEDIA_TYPE *pmt)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::GetCurFile()\n"), this));
    CAutoLock cAutoLock(&m_cCritSecState);

    HRESULT hr = E_NOTIMPL;

    if (m_pifsrcfNext)
        hr = m_pifsrcfNext->GetCurFile(ppszFileName, pmt);

    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetCurFile() returning error HRESULT %d\n"), this, hr));
    }
    return hr;
} // CSampleConsumer::GetCurFile

///////////////////////////////////////////////////////////////////////
//
//  Class CSampleConsumer -- IStreamBufferPlayback
//
///////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE CSampleConsumer::SetTunePolicy(
    STRMBUF_PLAYBACK_TUNE_POLICY eStrmbufPlaybackTunePolicy)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::SetTunePolicy(%d)\n"), this, (int) eStrmbufPlaybackTunePolicy));
    CAutoLock cAutoLock(&m_cCritSecState);

    HRESULT hr = S_OK;
    m_eStrmBufPlaybackTunePolicy = eStrmbufPlaybackTunePolicy;

    if (m_piStreamBufferPlayback)
    {
        HRESULT hrFwd = m_piStreamBufferPlayback->SetTunePolicy(eStrmbufPlaybackTunePolicy);
        if (FAILED(hrFwd) && (hrFwd != E_NOTIMPL))
            hr = hrFwd;
    }


    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::SetTunePolicy() returning error HRESULT %d\n"), this, hr));
    }
    return hr;
} // CSampleConsumer::SetTunePolicy

HRESULT STDMETHODCALLTYPE CSampleConsumer::GetTunePolicy(
    STRMBUF_PLAYBACK_TUNE_POLICY *peStrmbufPlaybackTunePolicy)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::GetTunePolicy()\n"), this));
    CAutoLock cAutoLock(&m_cCritSecState);

    HRESULT hr = S_OK;

    if (!peStrmbufPlaybackTunePolicy)
        hr = E_POINTER;
    else
    {
        if (m_piStreamBufferPlayback)
        {
            hr = m_piStreamBufferPlayback->GetTunePolicy(peStrmbufPlaybackTunePolicy);
            if (hr == E_NOTIMPL)
                hr = S_OK;
            else if (SUCCEEDED(hr))
                ASSERT(*peStrmbufPlaybackTunePolicy == m_eStrmBufPlaybackTunePolicy);
        }
        if (SUCCEEDED(hr))
            *peStrmbufPlaybackTunePolicy = m_eStrmBufPlaybackTunePolicy;
    }

    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetTunePolicy() returning error HRESULT %d\n"), this, hr));
    }
    return hr;
} // CSampleConsumer::GetTunePolicy

HRESULT STDMETHODCALLTYPE CSampleConsumer::SetThrottlingEnabled(BOOL fThrottle)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::SetThrottlingEnabled(%u)\n"), this, (unsigned) fThrottle));
    CAutoLock cAutoLock(&m_cCritSecState);

    HRESULT hr = S_OK;

    if (!fThrottle && (GetTrueRate() != 1.0))
    {
        hr = XACT_E_WRONGSTATE;
    }
    else
    {
        if (m_piStreamBufferPlayback)
        {
            hr = m_piStreamBufferPlayback->SetThrottlingEnabled(fThrottle);
        }
        if (SUCCEEDED(hr) || (hr == E_NOTIMPL))
        {
            if (m_piDecoderDriver)
                m_piDecoderDriver->ImplementThrottling(fThrottle ? true : false);
            m_fThrottling = fThrottle ? true : false;
            hr = S_OK;
        }
    }
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::SetThrottlingEnabled() returning error HRESULT %d\n"), this, hr));
    }

    return hr;
} // CSampleConsumer::SetThrottlingEnabled

HRESULT STDMETHODCALLTYPE CSampleConsumer::GetThrottlingEnabled(PBOOL pfThrottle)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::GetThrottlingEnabled(%p)\n"), this, pfThrottle));
    CAutoLock cAutoLock(&m_cCritSecState);

    HRESULT hr = S_OK;

    if (pfThrottle)
        *pfThrottle = m_fThrottling ? TRUE : FALSE;
    else
        hr = E_POINTER;

    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetThrottlingEnabled() returning error HRESULT %d\n"), this, hr));
    }
    else
    {
    }
    return hr;
}// CSampleConsumer::GetThrottlingEnabled

HRESULT STDMETHODCALLTYPE CSampleConsumer::NotifyGraphIsConnected()
{
    return E_NOTIMPL;
} // CSampleConsumer::NotifyGraphIsConnected

HRESULT CSampleConsumer::IsAtLive(BOOL *pfAtLive)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::IsAtLive(%p)\n"), this, pfAtLive));
    CAutoLock cAutoLock(&m_cCritSecState);

    HRESULT hr = S_OK;

    if (pfAtLive)
    {
        if (m_dblRate < 0)
        {
            DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::IsAtLive():  playing backward, not trying to stay with live\n") ));

            *pfAtLive = FALSE;
        }
        else if (!IsLiveOrInProgress())
        {
            DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::IsAtLive():  not bound to a sample producer\n") ));

            *pfAtLive = FALSE;
        }
        else if (IsSourceReader())
        {
            DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::IsAtLive():  using the reader\n") ));

            *pfAtLive = FALSE;
        }
        else if (IsSourceProducer())
        {
            DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::IsAtLive():  using the sample producer\n") ));

            *pfAtLive = TRUE;
        }
        else if (!m_pcPinMappings)
        {
            DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::IsAtLive():  no pin mappings so producer range unknown\n") ));

            *pfAtLive = FALSE;
            hr = E_FAIL;
        }
        else
        {
            LONGLONG hyCurPos = ((m_eSCPositionMode == SC_POSITION_PENDING_RUN) ||
                                  (m_eSCPositionMode == SC_POSITION_PENDING_SET_POSITION)) ?
                                        m_hyTargetPosition : m_hyCurrentPosition;
            if (hyCurPos == -1)
            {
                DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::IsAtLive():  deciding based on stay-with-sink setting\n") ));

                *pfAtLive = (STRMBUF_STAY_WITH_SINK == m_esbptp) ? TRUE : FALSE;
            }
            else
            {
                LONGLONG hyProducerMin, hyProducerMax;
                m_piSampleProducer->GetPositionRange(hyProducerMin, hyProducerMax);

                if ((hyProducerMin == -1) && (hyProducerMax == -1))
                {
                    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::IsAtLive():  producer has no range yet, bailing\n") ));

                    *pfAtLive = FALSE;
                    hr = E_FAIL;
                }
                else
                {
                    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::IsAtLive():  deciding based on current position %I64d vs producer range [%I64d,%I64d]\n"),
                                hyCurPos, hyProducerMin, hyProducerMax ));

                    if (hyProducerMin == -1)
                    {
                        // We're going to assume that any position within 4 seconds of
                        // the latest is good enough for 'live'.

                        *pfAtLive = (hyCurPos >= hyProducerMax - MAX_LIVE_PLAYBACK_RANGE) ? TRUE : FALSE;
                    }
                    else
                    {
                        *pfAtLive = (hyCurPos >= hyProducerMin) ? TRUE : FALSE;
                    }
                }
            }
        }
        if (*pfAtLive && SUCCEEDED(hr))
        {
            // At this, the source filter, we appear to be at live.  However, it could be that
            // there is so much buffered downstream that the user is not viewing anywhere near
            // live.

            LONGLONG hyMinPos, hyCurPos, hyMaxPos;

            if (SUCCEEDED(GetConsumerPositionsExternal(hyMinPos, hyCurPos, hyMaxPos)))
            {
                if (hyMaxPos - hyCurPos > MAX_LIVE_PLAYBACK_RANGE)
                {
                    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::IsAtLive():  source vs rendering delta is large [%I64d ms] -- not live after all\n"),
                                (hyMaxPos - hyCurPos) / 10000LL ));

                    *pfAtLive = FALSE;
                }
            }
        }
    }
    else
        hr = E_POINTER;

    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::IsAtLive() returning error HRESULT %d\n"), this, hr));
    }
    else
    {
        DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::IsAtLive() returning %s\n"),
            this, *pfAtLive ? _T("live") : _T("not-live") ));
    }
    return hr;
} // CSampleConsumer::IsAtLive

HRESULT CSampleConsumer::GetOffsetFromLive(LONGLONG *pllOffsetFromLive)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::GetOffsetFromLive(%p)\n"), this, pllOffsetFromLive));
    CAutoLock cAutoLock(&m_cCritSecState);

    HRESULT hr = S_OK;

    if (!pllOffsetFromLive)
        hr = E_POINTER;
    else if (!IsLiveOrInProgress())
        hr = E_FAIL;
    else
    {
        LONGLONG hyMinPos, hyCurPos, hyMaxPos;

        hr = GetConsumerPositionsExternal(hyMinPos, hyCurPos, hyMaxPos, false);
        if (SUCCEEDED(hr))
        {
            *pllOffsetFromLive = hyMaxPos - hyCurPos;
        }
    }

    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetOffsetFromLive() returning error HRESULT %d\n"), this, hr));
    }
    else
    {
        DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::GetOffsetFromLive() returning %I64d\n"),
            this, *pllOffsetFromLive ));
    }
    return hr;
} // CSampleConsumer::GetOffsetFromLive

HRESULT CSampleConsumer::SetDesiredOffsetFromLive(LONGLONG llOffsetFromLive)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::SetDesiredOffsetFromLive(%I64d)\n"), this, llOffsetFromLive));
    CAutoLock cAutoLock(&m_cCritSecState);

    HRESULT hr = S_OK;

    if (llOffsetFromLive == 0x7fffffffffffffff)
        m_hyTargetOffsetFromLive = g_rtMinimumBufferingUpstreamOfLive;
    else
        m_hyTargetOffsetFromLive = llOffsetFromLive + ALLOWANCE_FOR_CARRYING_OUT_GO_LIVE;
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::SetDesiredOffsetFromLive() returning success\n") ));
    return hr;
} // CSampleConsumer::GetOffsetFromLive

HRESULT CSampleConsumer::GetLoadIncarnation(DWORD *pdwLoadIncarnation)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::GetLoadIncarnation(%p)\n"), this, pdwLoadIncarnation));
    CAutoLock cAutoLock(&m_cCritSecState);

    if (!pdwLoadIncarnation)
        return E_POINTER;

    *pdwLoadIncarnation = m_dwLoadIncarnation;
    return S_OK;
} // CSampleConsumer::GetLoadIncarnation

HRESULT CSampleConsumer::EnableBackgroundPriority(BOOL fEnableBackgroundPriority)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::EnableBackgroundPriority(%u)\n"), this, (unsigned) fEnableBackgroundPriority));
    CAutoLock cAutoLock(&m_cCritSecState);

    m_fEnableBackgroundPriority = fEnableBackgroundPriority;
    IReader *piReader = GetReader();
    if (piReader)
    {
        piReader->SetBackgroundPriorityMode(fEnableBackgroundPriority);
    }
    IDecoderDriver *piDecoderDriver = GetDecoderDriver();
    if (piDecoderDriver)
    {
        piDecoderDriver->SetBackgroundPriorityMode(fEnableBackgroundPriority);
    }
    return S_OK;
} // CSampleConsumer::EnableBackgroundPriority

///////////////////////////////////////////////////////////////////////
//
//  Class CSampleConsumer -- IStreamBufferMediaSeeking
//
///////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE CSampleConsumer::GetCapabilities(
    /* [out] */ DWORD *pCapabilities)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::GetCapabilities(%p)\n"), this, pCapabilities));
    CAutoLock cAutoLock(&m_cCritSecState);

    HRESULT hr = E_NOTIMPL;
    if (!pCapabilities)
        hr = E_POINTER;
    else if (m_pisbmsNext)
        hr = m_pisbmsNext->GetCapabilities(pCapabilities);

    if (SUCCEEDED(hr) || (hr == E_NOTIMPL))
    {
        DWORD dwCapabilities = AM_SEEKING_CanSeekAbsolute
                            | AM_SEEKING_CanSeekForwards
                            | AM_SEEKING_CanSeekBackwards
                            | AM_SEEKING_CanGetCurrentPos
                            | AM_SEEKING_CanPlayBackwards
                            | AM_SEEKING_CanGetStopPos
                            | AM_SEEKING_CanDoSegments
                            ;
        if (SUCCEEDED(hr))
        {
            *pCapabilities &= dwCapabilities;
        }
        else
            *pCapabilities = dwCapabilities;
        hr = S_OK;
    }
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetCapabilities() returning error HRESULT %d\n"), this, hr));
    }
    return hr;
} // CSampleConsumer::GetCapabilities

HRESULT STDMETHODCALLTYPE CSampleConsumer::CheckCapabilities(
    /* [out][in] */ DWORD *pCapabilities)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::CheckCapabilities()\n"), this));
    CAutoLock cAutoLock(&m_cCritSecState);

    if (!pCapabilities)
    {
        return E_POINTER;
    }

    HRESULT hr = E_NOTIMPL;
    if (m_pisbmsNext)
    {
        hr = m_pisbmsNext->CheckCapabilities(pCapabilities);
    }
    if (hr == E_NOTIMPL)
    {
        hr = S_OK;
    }
    if (SUCCEEDED(hr))
    {
        DWORD dwCapabilities = AM_SEEKING_CanSeekAbsolute
                            | AM_SEEKING_CanSeekForwards
                            | AM_SEEKING_CanSeekBackwards
                            | AM_SEEKING_CanGetCurrentPos
                            | AM_SEEKING_CanPlayBackwards
                            | AM_SEEKING_CanGetStopPos
                            | AM_SEEKING_CanDoSegments
            & *pCapabilities;

        if (dwCapabilities == 0)
        {
            hr = E_FAIL;  // no capabilities are supported
        }
        else if ((hr == S_OK) && (*pCapabilities != dwCapabilities))
        {
            hr = S_FALSE;  // some but not all capabilities are supported.
        }

        *pCapabilities = dwCapabilities;
    }
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::CheckCapabilities() returning error HRESULT %d\n"), this, hr));
    }
    return hr;
} // CSampleConsumer::CheckCapabilities

HRESULT STDMETHODCALLTYPE CSampleConsumer::IsFormatSupported(
    /* [in] */ const GUID *pFormat)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::IsFormatSupported()\n"), this));
    CAutoLock cAutoLock(&m_cCritSecState);

    if (!pFormat)
    {
        return E_POINTER;
    }

    HRESULT hr = E_NOTIMPL;
    if (m_pisbmsNext)
    {
        hr = m_pisbmsNext->IsFormatSupported(pFormat);
    }
    if (hr == E_NOTIMPL)
    {
        hr = S_OK;
    }
    if (FAILED(hr) || (hr == S_FALSE))
    {
        if (hr != S_FALSE)
        {
            DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::IsFormatSupported() returning error HRESULT %d\n"), this, hr));
        }
        return hr;
    }

    return (*pFormat == TIME_FORMAT_MEDIA_TIME) ? S_OK : S_FALSE;
} // CSampleConsumer::IsFormatSupported

HRESULT STDMETHODCALLTYPE CSampleConsumer::QueryPreferredFormat(
    /* [out] */ GUID *pFormat)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::QueryPreferredFormat(%p)\n"), this, pFormat));
    CAutoLock cAutoLock(&m_cCritSecState);

    if (!pFormat)
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::QueryPreferredFormat() returning error E_POINTER\n"), this));
        return E_POINTER;
    }

#ifdef DEBUG
    if (m_pisbmsNext)
    {
        HRESULT hr = m_pisbmsNext->QueryPreferredFormat(pFormat);
        ASSERT((hr == S_OK) || (hr == E_NOTIMPL));
    }
#endif
    *pFormat = TIME_FORMAT_MEDIA_TIME;
    return S_OK;
} // CSampleConsumer::QueryPreferredFormat

HRESULT STDMETHODCALLTYPE CSampleConsumer::GetTimeFormat(
    /* [out] */ GUID *pFormat)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::GetTimeFormat()\n"), this));
    CAutoLock cAutoLock(&m_cCritSecState);

    if (!pFormat)
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetTimeFormat() returning error E_POINTER\n"), this));
        return E_POINTER;
    }

#ifdef DEBUG
    if (m_pisbmsNext)
    {
        HRESULT hr = m_pisbmsNext->GetTimeFormat(pFormat);
        ASSERT(((hr == S_OK) && (*pFormat == m_pguidCurTimeFormat)) || (hr == E_NOTIMPL));
    }
#endif
    *pFormat = m_pguidCurTimeFormat;
    return S_OK;
} // CSampleConsumer::GetTimeFormat

HRESULT STDMETHODCALLTYPE CSampleConsumer::IsUsingTimeFormat(
    /* [in] */ const GUID *pFormat)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::IsUsingTimeFormat()\n"), this));
    CAutoLock cAutoLock(&m_cCritSecState);

    if (!pFormat)
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::IsUsingTimeFormat() returning error E_POINTER\n"), this));
        return E_POINTER;
    }

    HRESULT hr = (*pFormat == m_pguidCurTimeFormat) ? S_OK : S_FALSE;
#ifdef DEBUG
    if (m_pisbmsNext)
    {
        HRESULT hr2 = m_pisbmsNext->IsUsingTimeFormat(pFormat);
        ASSERT(FAILED(hr2) || (hr2 == hr));
    }
#endif
    return hr;
}  // CSampleConsumer::IsUsingTimeFormat

HRESULT STDMETHODCALLTYPE CSampleConsumer::SetTimeFormat(
    /* [in] */ const GUID *pFormat)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::SetTimeFormat()\n"), this));

    HRESULT hr = S_OK;

    {
        CAutoLock cAutoLock(&m_cCritSecState);
        if (!pFormat)
            hr = E_POINTER;
        else if (*pFormat != TIME_FORMAT_MEDIA_TIME)
            hr = E_INVALIDARG;
        else if (!Stopped())
        {
            hr = VFW_E_WRONG_STATE;
        }
        else if (m_pisbmsNext)
        {
            hr = m_pisbmsNext->SetTimeFormat(pFormat);
            if (hr == E_NOTIMPL)
                hr = S_OK;
        }
        if (SUCCEEDED(hr))
            m_pguidCurTimeFormat = *pFormat;
        else
            DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::SetTiemFormat() returning error HRESULT %d\n"), this, hr));
    }

    return hr;
} // CSampleConsumer::SetTimeFormat

HRESULT STDMETHODCALLTYPE CSampleConsumer::GetDuration(
    /* [out] */ LONGLONG *pDuration)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::GetDuration()\n"), this));
    ProcessPauseBufferUpdates();
    CAutoLock cAutoLock(&m_cCritSecState);

    if (!pDuration)
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetDuration() returning error E_POINTER\n"), this));
        return E_POINTER;
    }

    IReader *pReader = GetReader();
    if (!pReader || ViewingRecordingInProgress())
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetDuration() returning error E_NOTIMPL due to no reader or completed recording\n"), this));
        return E_NOTIMPL;
    }

    LONGLONG llDuration = pReader->GetMaxSeekPosition() - pReader->GetMinSeekPosition();
#ifdef DEBUG
    if (m_pisbmsNext)
    {
        HRESULT hr = m_pisbmsNext->GetDuration(pDuration);
        ASSERT(FAILED(hr) || (*pDuration == llDuration));
    }
#endif
    *pDuration = llDuration;
    return S_OK;
} // CSampleConsumer::GetDuration

HRESULT STDMETHODCALLTYPE CSampleConsumer::GetStopPosition(
    /* [out] */ LONGLONG *pStop)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::GetStopPosition()\n"), this));
    ProcessPauseBufferUpdates();
    CAutoLock cAutoLock(&m_cCritSecState);

    if (!pStop)
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetStopPosition() returning error E_POINTER\n"), this));
        return E_POINTER;
    }
    else if (SC_LOAD_COMPLETE != m_eSCLoadState)
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetStopPosition() returning error E_FAIL because nothing is loaded\n"), this));
        return E_FAIL;
    }

#ifdef DEBUG
    if (m_pisbmsNext)
    {
        // We're the authoritative source for stop info,
        // so any error or mismatch is some other pipeline
        // component's problem.
        HRESULT hr = m_pisbmsNext->GetStopPosition(pStop);
        ASSERT(FAILED(hr) || (*pStop == m_hyStopPosition));
    }
#endif
    *pStop = m_hyStopPosition;
    return S_OK;
} // CSampleConsumer::GetStopPosition

HRESULT STDMETHODCALLTYPE CSampleConsumer::GetCurrentPosition(
    /* [out] */ LONGLONG *pCurrent)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::GetCurrentPosition()\n"), this));
    CAutoLock cAutoLock(&m_cCritSecState);

    if (!pCurrent)
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetCurrentPosition() returning error E_POINTER\n"), this));
        return E_POINTER;
    }
    else if (SC_LOAD_COMPLETE != m_eSCLoadState)
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetCurrentPosition() returning error E_FAIL because nothing is loaded\n"), this));
        return E_FAIL;
    }

    LONGLONG hyMinPos, hyCurPos, hyMaxPos;
    HRESULT hr = GetConsumerPositionsExternal(hyMinPos, hyCurPos, hyMaxPos);
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetCurrentPosition() returning error HRESULT %d\n"), this, hr));
        return hr;
    }

#ifdef DEBUG
    if (m_pisbmsNext)
    {
        HRESULT hrTmp = m_pisbmsNext->GetCurrentPosition(pCurrent);
        ASSERT(FAILED(hrTmp) || (*pCurrent == hyCurPos));
    }
#endif
    *pCurrent = hyCurPos;
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::GetCurrentPosition() returning %I64d\n"), this, hyCurPos));
    return S_OK;
} // CSampleConsumer::GetCurrentPosition

HRESULT STDMETHODCALLTYPE CSampleConsumer::ConvertTimeFormat(
    /* [out] */ LONGLONG *pTarget,
    /* [in] */ const GUID *pTargetFormat,
    /* [in] */ LONGLONG Source,
    /* [in] */ const GUID *pSourceFormat)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::ConvertTimeFormat()\n"), this));

    DbgLog((LOG_ERROR, 2, _T("CSa:mpleConsumer(%p)::ConvertTimeFormat() returning error E_NOTIMPL\n"), this));
    return E_NOTIMPL;
} // CSampleConsumer::ConvertTimeFormat

HRESULT STDMETHODCALLTYPE CSampleConsumer::SetPositions(
    /* [out][in] */ LONGLONG *pCurrent,
    /* [in] */ DWORD dwCurrentFlags,
    /* [out][in] */ LONGLONG *pStop,
    /* [in] */ DWORD dwStopFlags)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::SetPositions()\n"), this));

    HRESULT hr = S_OK;
    // TODO:  Safeguard against integer overflow!!!
    //
    // TODO:  positions are relative to the duration
    bool changeCurPos = false;
    bool changeStopPos = false;
    LONGLONG llDesiredCurPosition = pCurrent ? *pCurrent : 0;
    LONGLONG llDesiredStopPosition = pStop ? *pStop : 0;
    LONGLONG llOrigCurrent = pCurrent ? *pCurrent : 0;
    LONGLONG llOrigStop = pStop ? *pStop : 0;

    ProcessPauseBufferUpdates();

    {
        CAutoLock cAutoLock(&m_cCritSecState);

        if (SC_LOAD_COMPLETE != m_eSCLoadState)
        {
            DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::SetPositions() returning error E_FAIL because nothing is loaded\n"), this));
            return E_FAIL;
        }
        m_fDroppingToNormalPlayAtBeginning = false;
        if (!m_fThrottling)
        {
            DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::SetPositions() returning error E_INVALIDSTATE because throttling is disabled\n"), this));
            return XACT_E_WRONGSTATE;
        }


        LONGLONG llCurPos;
        DWORD dwPositionMode = (dwCurrentFlags & AM_SEEKING_PositioningBitsMask);

        DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer()::SetPositions():  current position mode = %u\n"), dwPositionMode));
        if (dwPositionMode != AM_SEEKING_NoPositioning)
        {
            LONGLONG llMinPos = -1;
            LONGLONG llMaxPos = -1;
            LONGLONG llOrigMaxPos = -1;
            if (!pCurrent)
                hr = E_POINTER;
            else
            {
                changeCurPos = true;

                hr = GetConsumerPositionsExternal(llMinPos, llCurPos, llMaxPos, false);
                llOrigMaxPos = llMaxPos;
                if (SUCCEEDED(hr) && m_pcPauseBufferHistory)
                {
                    LONGLONG llSafeMin = m_pcPauseBufferHistory->GetSafePlaybackStart(GetPauseBufferSafetyMargin(true));
                    if (llMinPos < llSafeMin)
                    {
                        llMinPos = llSafeMin;
                        if (llMinPos > llMaxPos)
                            llMinPos = llMaxPos;
                    }
                    if (IsLiveOrInProgress())
                    {
                        if (llMaxPos - m_hyTargetOffsetFromLive >= llMinPos)
                            llMaxPos -= m_hyTargetOffsetFromLive;
                        else
                            llMaxPos = llMinPos;
                    }
                }
            }
            if (FAILED(hr))
            {
                DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::SetPositions():  bailing -- pCurrent = %p, hr = %d\n"),
                    pCurrent, hr));
                goto SetPositionsExit;
            }
            DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetPositions():  asked for position %I64d, currently %I64d, range [%I64d, %I64d]\n"),
                llDesiredCurPosition, llCurPos, llMinPos, llMaxPos));
            if (dwPositionMode == AM_SEEKING_RelativePositioning)
            {
                llDesiredCurPosition += llCurPos;
            }
            if (llDesiredCurPosition < llMinPos)
                llDesiredCurPosition = llMinPos;
            if (llDesiredCurPosition > llMaxPos)
            {
                if (IsLiveOrInProgress())
                {
                    DbgLog((LOG_SAMPLE_CONSUMER, 3, TEXT("CSampleConsumer::SetPositions():  lagging from 'now' to ensure downstream buffering\n") ));
                }

                llDesiredCurPosition = llMaxPos;
                if ((m_eSCPositionMode == SC_POSITION_NORMAL) &&
                    (m_hyCurrentPosition != CURRENT_POSITION_NULL_VALUE) &&
                    m_piSampleProducer)
                {
                    // We are in the normal sort of situation -- playing where we know
                    // the current position.  In this case, we're just going to create
                    // an a/v glitch if we do a tiny seek to catch up to now (due to
                    // the flush). Skip the seek in that case:
                    ASSERT((m_hyCurrentPosition <= llDesiredCurPosition) ||
                           ((llDesiredCurPosition == llMaxPos) && m_piSampleProducer));

                    if ((m_hyCurrentPosition <= llOrigMaxPos) &&
                        (llOrigMaxPos - m_hyCurrentPosition < s_hyOmitSeekOptimizationWindow))
                    {
                        DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetPositions():  corrected position to current (%I64d) rather than latest (%I64d)\n"),
                            m_hyCurrentPosition, llDesiredCurPosition));
                        llDesiredCurPosition = m_hyCurrentPosition;
                    }
                }
            }
            DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetPositions():  request corrected to position %I64d\n"),
                llDesiredCurPosition));
        }

        dwPositionMode = (dwStopFlags & AM_SEEKING_PositioningBitsMask);
        if (dwPositionMode != AM_SEEKING_NoPositioning)
        {
            if (!pStop)
                hr = E_POINTER;

            if (dwPositionMode == AM_SEEKING_IncrementalPositioning)
            {
                if (!pCurrent)
                    hr = E_INVALIDARG;
                else
                    llDesiredStopPosition += llDesiredCurPosition;
            }
            else if (dwPositionMode == AM_SEEKING_RelativePositioning)
            {
                if (m_hyStopPosition == STOP_POSITION_NULL_VALUE)
                    hr = E_INVALIDARG;
                else
                    llDesiredStopPosition += m_hyStopPosition;
            }

            changeStopPos = true;
            m_eSCStopMode = (dwStopFlags & AM_SEEKING_Segment) ?
                SC_STOP_MODE_END_SEGMENT : SC_STOP_MODE_END_STREAM;
            if (FAILED(hr))
                goto SetPositionsExit;
        }

        if (m_pisbmsNext)
        {
            DWORD dwForwardCurFlags = 0;
            DWORD dwForwardStopFlags = 0;

            if (changeCurPos)
                dwForwardCurFlags = (dwCurrentFlags &
                    (AM_SEEKING_Segment | AM_SEEKING_SeekToKeyFrame | AM_SEEKING_NoFlush));
            else
                dwForwardCurFlags = AM_SEEKING_NoPositioning;
            if (changeStopPos)
                dwForwardStopFlags = (dwStopFlags &
                    (AM_SEEKING_Segment | AM_SEEKING_SeekToKeyFrame | AM_SEEKING_NoFlush));
            else
                dwForwardStopFlags = AM_SEEKING_NoPositioning;

            hr = m_pisbmsNext->SetPositions(
                &llDesiredCurPosition, dwForwardCurFlags,
                &llDesiredStopPosition, dwForwardStopFlags);
            if (hr == E_NOTIMPL)
                hr = S_OK;
        }

        if (SUCCEEDED(hr) && changeCurPos)
        {
            if (llCurPos == llDesiredCurPosition)
                changeCurPos = FALSE;
            else
            {
                try {
                    SetCurrentPosition(NULL, llDesiredCurPosition,
                        (dwCurrentFlags & AM_SEEKING_SeekToKeyFrame) != 0,
                        (dwCurrentFlags & AM_SEEKING_NoFlush) != 0,
                        true, true);
                } catch (const CHResultError &cHResultError) {
                    hr = cHResultError.m_hrError;
                } catch (const std::exception &) {
                    hr = E_FAIL;
                }
            }
        }
        if (SUCCEEDED(hr) && changeStopPos)
        {
            if (llDesiredStopPosition == m_hyStopPosition)
                changeStopPos = false;
            else
                m_hyStopPosition = llDesiredStopPosition;
        }
        if (SUCCEEDED(hr))
        {
            if (pCurrent && (dwCurrentFlags & AM_SEEKING_ReturnTime))
                *pCurrent = llDesiredCurPosition;
            if (pStop && (dwStopFlags & AM_SEEKING_ReturnTime))
                *pStop = llDesiredStopPosition;
        }

SetPositionsExit:
        if (SUCCEEDED(hr))
            hr = (changeCurPos || changeStopPos) ? S_OK : S_FALSE;
        else
            DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::SetPositions() returning error HRESULT %d\n"), this, hr));
    }


    return hr;
} // CSampleConsumer::SetPositions

HRESULT STDMETHODCALLTYPE CSampleConsumer::GetPositions(
    /* [out] */ LONGLONG *pCurrent,
    /* [out] */ LONGLONG *pStop)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::GetPositions(%p, %p)\n"), this, pCurrent, pStop));
    ProcessPauseBufferUpdates();
    CAutoLock cAutoLock(&m_cCritSecState);

    if (!pCurrent || !pStop)
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetPositions() returning error E_POINTER\n"), this));
        return E_POINTER;
    }
    else if (SC_LOAD_COMPLETE != m_eSCLoadState)
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetPositions() returning error E_FAIL because nothing is loaded\n"), this));
        return E_FAIL;
    }

    LONGLONG hyMinPos, hyCurPos, hyMaxPos;
    HRESULT hr = GetConsumerPositionsExternal(hyMinPos, hyCurPos, hyMaxPos);
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetPositions() returning error HRESULT %d\n"), this, hr));
        return hr;
    }

    if (m_pisbmsNext)
    {
        hr = m_pisbmsNext->GetPositions(pCurrent, pStop);
        ASSERT(FAILED(hr) || (
            (*pStop == m_hyStopPosition) &&
            (*pCurrent == hyCurPos)));
    }
    *pCurrent = hyCurPos;
    *pStop = m_hyStopPosition;
    return S_OK;
} // CSampleConsumer::GetPositions

HRESULT STDMETHODCALLTYPE CSampleConsumer::GetAvailable(
    /* [out] */ LONGLONG *pEarliest,
    /* [out] */ LONGLONG *pLatest)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::GetAvailable()\n"), this));
    ProcessPauseBufferUpdates();
    CAutoLock cAutoLock(&m_cCritSecState);

    if (!pEarliest || !pLatest)
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetAvailable() returning error E_POINTER\n"), this));
        return E_POINTER;
    }
    else if (SC_LOAD_COMPLETE != m_eSCLoadState)
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetAvailable() returning error E_FAIL because nothing is loaded\n"), this));
        return E_FAIL;
    }

    LONGLONG hyMinPos, hyCurPos, hyMaxPos;
    HRESULT hr = GetConsumerPositionsExternal(hyMinPos, hyCurPos, hyMaxPos);
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetPositions() returning error %d\n"), this, hr));
        return hr;
    }

    if (m_pisbmsNext)
    {
        hr = m_pisbmsNext->GetAvailable(pEarliest, pLatest);
        if (FAILED(hr) && (hr != E_NOTIMPL))
        {
            DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetPositions() returning error HRESULT %d\n"), this, hr));
            return hr;
        }
        if (*pEarliest > hyMinPos)
            hyMinPos = *pEarliest;
        if (*pLatest < hyMaxPos)
            hyMaxPos = *pLatest;
    }
    *pEarliest = hyMinPos;
    *pLatest = hyMaxPos;

    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::GetAvailable() -- earliest %I64d, latest %I64d\n"),
        this, hyMinPos, hyMaxPos));

    return S_OK;
} // CSampleConsumer::GetAvailable

struct BoolHelper
{
    BoolHelper(bool &rfBool)
        : m_rfBool(rfBool)
        , m_fOriginalValue(rfBool)
    {
    }

    ~BoolHelper()
    {
        m_rfBool = m_fOriginalValue;
    }

private:
    bool &m_rfBool;
    bool m_fOriginalValue;
};

HRESULT STDMETHODCALLTYPE CSampleConsumer::SetRate(
    /* [in] */ double dRate)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::SetRate(%lf)\n"), this, dRate));

    HRESULT hr = S_OK;
    bool fRunGraph = false;
    IDecoderDriver *piDecoderDriver = NULL;
    double dblOldRate;
    bool fRecommendFlushingSeek = false;
    LONGLONG hyRecommendedSeekPosition = -1;
    bool fAtDecoderEOS = false;
    BOOL fDoingMasterFlush = FALSE;
    bool fRestoringNormalRate = false;
    BoolHelper fInRateChange(m_fInRateChange);

    {
        CAutoLock cAutoLock(&m_cCritSecState);

        piDecoderDriver = GetDecoderDriver();
    }

    ProcessPauseBufferUpdates();

    CDecoderDriverFlushRequest cDecoderDriverFlushRequest(piDecoderDriver);

    {
        CAutoLock cAutoLock(&m_cCritSecState);

        // Look for the sentinel value indicating that we are now at end/beginning
        // and trying to restore our rate to 1.0:

        if ((dRate == 0.0) && (m_eSCPositionMode == SC_POSITION_PENDING_NORMAL_RATE))
        {
            DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetRate():  implementing boundary-of-pause-buffer/recording, set-rate-normal\n") ));
            fRestoringNormalRate = true;
            m_dblPreDeferNormalRate = 1.0;
            dRate = 1.0;
        }
        m_fDroppingToNormalPlayAtBeginning = false;
        if (SC_LOAD_COMPLETE != m_eSCLoadState)
            hr = E_FAIL;
        else if (!CanSupportRate(dRate))
            hr = E_INVALIDARG;
        else if (!m_fThrottling)
            hr = XACT_E_WRONGSTATE;
        else if ((m_dblRate >= 1.0) && Running() && (dRate > m_dblRate) && DeliveringProducerSamples())
            hr = E_FAIL;
        else if (m_pisbmsNext)
        {
            hr = m_pisbmsNext->SetRate(dRate);
            if (hr == E_NOTIMPL)
                hr = S_OK;
        }
        if (SUCCEEDED(hr))
        {
            if (!m_fConsumerSetGraphRate && (dRate == m_dblRate) &&
                !fRestoringNormalRate &&
                CanDeliverMediaSamples(false))
            {
                DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetRate() ... treating as a no-op since the rate is already in effect\n") ));
                hr = S_FALSE;
                goto doNoopSetRate;
            }
            if (!fRestoringNormalRate &&
                (m_eSCPositionMode == SC_POSITION_PENDING_NORMAL_RATE))
            {
                if (dRate == m_dblPreDeferNormalRate)
                {
                    DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetRate() ... treating as a no-op since the rate is (for the moment) in effect\n") ));
                    hr = S_FALSE;
                    goto doNoopSetRate;
                }
                DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetRate() ... new rate %lf will clear pending-revert-to-normal from old rate %lf\n"),
                    dRate, m_dblPreDeferNormalRate));
            }

            // Be sure to ask questions related to position now, before we
            // do a possible flush as part of a rate change:

            if (piDecoderDriver)
            {
                fDoingMasterFlush = piDecoderDriver->IsFlushNeededForRateChange();
                if (S_OK == piDecoderDriver->IsSeekRecommendedForRate(dRate, hyRecommendedSeekPosition))
                    fRecommendFlushingSeek = true;
                fAtDecoderEOS = piDecoderDriver->IsAtEOS();
            }

            if (dRate == GetTrueRate())
                fRecommendFlushingSeek = false;

            // In case a seek is recommended:
            // Decide whether we want to override the decoder's recommendation based on
            // consumer state about whether we are seeking to a different position or
            // crossing between recordings:

            DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetRate():  decoder driver recommended seeking to %I64d\n"),
                hyRecommendedSeekPosition));

            switch (m_eSCPositionMode)
            {
            case SC_POSITION_UNKNOWN:
            case SC_POSITION_AWAITING_FIRST_SAMPLE:
                hyRecommendedSeekPosition = -1;
                break;

            case SC_POSITION_SEEKING_KEY:
            case SC_POSITION_SEEKING_ANY:
            case SC_POSITION_ALIGNING:
                hyRecommendedSeekPosition = m_hyCurrentPosition;
                break;

            case SC_POSITION_NORMAL:
            case SC_POSITION_AT_STOP_POSITION:
                break;

            case SC_POSITION_PENDING_NORMAL_RATE:
                if (!fRestoringNormalRate)
                {
                    // if we aren't restoring the normal rate, we're effectively
                    // at a stop position. See above. We will need a flushing
                    // seek since we cannot resume playback at our current
                    // position.

                    DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetRate():  aborting the wait-until-beginning/end is visible mode\n") ));

                    fRecommendFlushingSeek = true;
                    break;  
                }

                // deliberately fall through to handle the go-to-normal-rate case:
                fRecommendFlushingSeek = true;

            case SC_POSITION_PENDING_SET_POSITION:
            case SC_POSITION_PENDING_RUN:
                hyRecommendedSeekPosition = m_hyTargetPosition;
                break;

            default:
                ASSERT(0);
                break;
            }
            LONGLONG hyMinPos, hyCurPos, hyMaxPos;
            LONGLONG hyMinSafePosition = (m_pcPauseBufferHistory ? m_pcPauseBufferHistory->GetSafePlaybackStart(GetPauseBufferSafetyMargin(true)) : -1);
            GetConsumerPositions(hyMinPos, hyCurPos, hyMaxPos);
            LONGLONG hyMaxSafePosition = hyMaxPos;
            if (IsLiveOrInProgress())
                hyMaxSafePosition -= m_hyTargetOffsetFromLive;

            DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetRate():  comparing tentative %s to %I64d versus min safe %I64d, max safe %I64d\n"),
                fRecommendFlushingSeek ? TEXT("seek") : TEXT("no-seek"),
                hyRecommendedSeekPosition,
                hyMinSafePosition,
                hyMaxSafePosition ));

            if ((!fRecommendFlushingSeek || (hyRecommendedSeekPosition < 0)) &&
                (m_hyCurrentPosition >= 0) &&
                (hyMinSafePosition >= 0) &&
                (hyMinSafePosition > m_hyCurrentPosition))
            {
                hyRecommendedSeekPosition = hyMinSafePosition;
                fRecommendFlushingSeek = true;
            }
            else if (fRecommendFlushingSeek &&
                    (hyMinSafePosition >= 0) &&
                    (hyRecommendedSeekPosition >= 0) &&
                    (hyMinSafePosition > hyRecommendedSeekPosition))
            {
                hyRecommendedSeekPosition = hyMinSafePosition;
            }
            if ((!fRecommendFlushingSeek || (hyRecommendedSeekPosition < 0)) &&
                (m_hyCurrentPosition >= 0) &&
                (hyMaxSafePosition >= 0) &&
                (hyMaxSafePosition < m_hyCurrentPosition))
            {
                hyRecommendedSeekPosition = hyMaxSafePosition;
                fRecommendFlushingSeek = true;
            }
            else if (fRecommendFlushingSeek &&
                    (hyMaxSafePosition >= 0) &&
                    (hyRecommendedSeekPosition >= 0) &&
                    (hyMaxSafePosition < hyRecommendedSeekPosition))
            {
                hyRecommendedSeekPosition = hyMaxSafePosition;
            }
            
            DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetRate():  consumer decided to override the recommendation to %I64d\n"),
                hyRecommendedSeekPosition));

            if ((dRate < 0.0) &&
                ((fRecommendFlushingSeek &&
                 (hyRecommendedSeekPosition >= 0) &&
                 (hyRecommendedSeekPosition - MINIMUM_REWIND_DISTANCE <= hyMinSafePosition)) ||
                 (!fRecommendFlushingSeek && (m_hyCurrentPosition >= 0) && (m_hyCurrentPosition - MINIMUM_REWIND_DISTANCE <= hyMinSafePosition))))
            {
                DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetRate():  consumer decided to refuse rewind starting at/close to the beginning\n") ));

                hr = E_FAIL;
            }
        }

        if (SUCCEEDED(hr))
        {
            if (m_piSampleProducer &&
                ((dRate != GetTrueRate()) || m_fConsumerSetGraphRate) &&
                (m_hyTargetOffsetFromLive != 0) &&
                (m_esbptp == STRMBUF_STAY_WITH_SINK))
            {
                LONGLONG hyProducerMin, hyProducerMax;
                m_piSampleProducer->GetPositionRange(hyProducerMin, hyProducerMax);
                LONGLONG hyProposedPostRatePosition =
                    (!fRecommendFlushingSeek || (hyRecommendedSeekPosition == -1)) ?
                        m_hyCurrentPosition : hyRecommendedSeekPosition;
                if ((hyProposedPostRatePosition >= 0) && (hyProducerMax >= 0) &&
                    (hyProposedPostRatePosition > hyProducerMax - m_hyTargetOffsetFromLive))
                {
                    fRecommendFlushingSeek = true;
                    hyRecommendedSeekPosition = (hyProducerMin > hyProducerMax - m_hyTargetOffsetFromLive) ?
                        hyProducerMin : hyProducerMax - m_hyTargetOffsetFromLive;
                }
            }

            dblOldRate = GetTrueRate();
            m_dblRate = dRate;
            m_dblPreDeferNormalRate = dRate;
            ++m_uPositionNotifyId;
            m_fInRateChange = true;
            if (((dblOldRate >= 0) && (dRate < 0)) ||
                ((dblOldRate < 0) && (dRate >= 0)))
            {
                // We've changed directions so our old bound on position is no longer valid
                // unless we have yet to render anything since the last flush and are not
                // doing a seek.

                LONGLONG hyRenderedPos = -1;
                if (m_piDecoderDriver && !m_fFlushing)
                    hyRenderedPos = m_piDecoderDriver->EstimatePlaybackPosition(true, true);

                if (fDoingMasterFlush &&
                    ((hyRenderedPos >=0) || (m_hyPostFlushPosition >= 0)) &&
                    (!fRecommendFlushingSeek || (hyRecommendedSeekPosition < 0)))
                {
                    DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetRate():  seeking to where we were seeking before\n") ));

                    fRecommendFlushingSeek = true;
                    hyRecommendedSeekPosition = (hyRenderedPos >=0) ? hyRenderedPos : m_hyPostFlushPosition;
                }
                else
                {
                    DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetRate():  invalidating flush position bound\n") ));
                    m_hyPostFlushPosition = POST_FLUSH_POSITION_INVALID;
                    m_hyPostTunePosition = POST_FLUSH_POSITION_INVALID;
                }
            }
        }
    }

    // Need to not hold the lock because implementing a new rate may need to flush:

    if (piDecoderDriver && SUCCEEDED(hr))
    {
        if (fDoingMasterFlush)
            fDoingMasterFlush = cDecoderDriverFlushRequest.BeginFlush();
        piDecoderDriver->ImplementNewRate(this, dRate, fDoingMasterFlush);
    }
    else
        fDoingMasterFlush = FALSE;

    if (SUCCEEDED(hr))
    {
        CAutoLock cAutoLock(&m_cCritSecState);

        if (fRecommendFlushingSeek)
        {
            if ((hyRecommendedSeekPosition < 0) || (m_hyCurrentPosition == hyRecommendedSeekPosition))
            {
                // All we want to do is flush. Continue on our way otherwise.

                if (!fDoingMasterFlush)
                {
                    DbgLog((LOG_SOURCE_STATE, 4, _T("CSampleConsumer::SetRate():  Flushing before the seek\n")));

                    CAutoUnlock cAutoUnlock(m_cCritSecState);

                    if (cDecoderDriverFlushRequest.BeginFlush())
                        cDecoderDriverFlushRequest.EndFlush();
                }
            }
            else
            {
                DbgLog((LOG_SOURCE_STATE, 4, _T("CSampleConsumer::SetRate():: Doing a flushing seek to %I64d\n"),
                    hyRecommendedSeekPosition));
                // We want a flushing seek:
                SetCurrentPosition(NULL, hyRecommendedSeekPosition, TRUE,
                    fDoingMasterFlush ? true : false, true, true);
            }
        }
        if (fDoingMasterFlush)
        {
            cDecoderDriverFlushRequest.EndFlush();
            fDoingMasterFlush = FALSE;
        }
        if (SUCCEEDED(hr))
        {
            if (!Stopped() &&
                (m_eSCLoadState != SC_LOAD_NEEDED) &&
                (((m_eSCPositionMode == SC_POSITION_AT_STOP_POSITION) &&
                (m_eSCStopMode == SC_STOP_MODE_NONE)) ||
                (m_eSCPositionMode == SC_POSITION_PENDING_NORMAL_RATE)))
            {
                if (((m_dblRate < 0.0) && (dblOldRate > 0.0)) ||
                    ((m_dblRate > 0.0) && (dblOldRate < 0.0)))
                {
                    if (fAtDecoderEOS)
                        fRunGraph = true;
                    EnsureNotAtStopPosition(SC_INTERNAL_EVENT_REQUESTED_SET_RATE_RESUMING);
                }
                else if (m_pcPauseBufferSegmentBound &&
                        (((m_dblRate >= 0.0) && (m_hyCurrentPosition >= m_pcPauseBufferSegmentBound->CurrentEndTime())) ||
                        ((m_dblRate < 0.0) && (m_hyCurrentPosition <= m_pcPauseBufferSegmentBound->CurrentStartTime()))))
                {
                    // We are at end of stream and are merely changing the rate to
                    // something that will keep us at end-of-stream.  We'd better
                    // trigger end-of-stream notification.

                    DbgLog((LOG_SOURCE_STATE, 3,
                        _T("CSampleConsumer::Run() -- dispatching end-of-stream since we are sitting at a stop position\n") ));
                    m_fPendingFailedRunEOS = true;
                    m_pippmgr->GetRouter(NULL, false, true).EndOfStream();
                }
                else
                {
                    // Hmm.  If we are playing forward, more content should be arriving into
                    // the pause buffer. If we are playing backward, other code should be
                    // waking us up eventually to avoid the truncation point.

                    if (fAtDecoderEOS && CanDeliverMediaSamples(true))
                        fRunGraph = true;
                    EnsureNotAtStopPosition(fRunGraph ? SC_INTERNAL_EVENT_REQUESTED_SET_RATE_RESUMING
                        : SC_INTERNAL_EVENT_REQUESTED_SET_RATE_NORMAL);
                    UpdateMediaSource(fRunGraph ? SC_INTERNAL_EVENT_REQUESTED_SET_RATE_RESUMING
                        : SC_INTERNAL_EVENT_REQUESTED_SET_RATE_NORMAL);
                }
            }
            else
            {
                if (fAtDecoderEOS && CanDeliverMediaSamples(true))
                    fRunGraph = true;
                UpdateMediaSource(fRunGraph ? SC_INTERNAL_EVENT_REQUESTED_SET_RATE_RESUMING
                    : SC_INTERNAL_EVENT_REQUESTED_SET_RATE_NORMAL);
            }
        }
    }
    else
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::SetRate() returning error HRESULT %d\n"), this, hr));

    cDecoderDriverFlushRequest.EndFlush();

    if (fRunGraph)
        piDecoderDriver->ImplementRun(true);

doNoopSetRate:
    m_fConsumerSetGraphRate = false;

    return hr;
} // CSampleConsumer::SetRate

HRESULT STDMETHODCALLTYPE CSampleConsumer::GetRate(
    /* [out] */ double *pdRate)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::GetRate() will return %lf\n"), this, m_dblRate));
    CAutoLock cAutoLock(&m_cCritSecState);

    if (!pdRate)
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetRate() returning error E_POINTER\n"), this));
        return E_POINTER;
    }
    else if (SC_LOAD_COMPLETE != m_eSCLoadState)
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetRate() returning error E_FAIL because nothing is loaded\n"), this));
        return E_FAIL;
    }

    if (m_pisbmsNext)
    {
        HRESULT hr = m_pisbmsNext->GetRate(pdRate);
        ASSERT(FAILED(hr) || (*pdRate == m_dblRate));

        if (SUCCEEDED(hr) && (*pdRate != m_dblRate))
        {
            // This is a scenario that shouldn't happen. If the authoritative
            // party wrt to rates (i.e., the DecoderDriver) has a different
            // belief, get back in sync:

            m_dblRate = *pdRate;
            m_dblPreDeferNormalRate = *pdRate;
            UpdateMediaSource(SC_INTERNAL_EVENT_REQUESTED_SET_RATE_NORMAL);
        }
    }

    *pdRate = GetTrueRate();
    return S_OK;
} // CSampleConsumer::GetRate

HRESULT STDMETHODCALLTYPE CSampleConsumer::GetPreroll(
    /* [out] */ LONGLONG *pllPreroll)
{
    DbgLog((LOG_USER_REQUEST, 3, _T("CSampleConsumer(%p)::GetPreroll()\n"), this));
    CAutoLock cAutoLock(&m_cCritSecState);

    HRESULT hr = S_OK;

    if (!pllPreroll)
        hr = E_POINTER;
    else if (SC_LOAD_COMPLETE != m_eSCLoadState)
        hr = E_FAIL;
    else if (m_pisbmsNext)
    {
        hr = m_pisbmsNext->GetPreroll(pllPreroll);
        if (hr == E_NOTIMPL)
            hr = S_OK;
    }

    if (SUCCEEDED(hr))
    {
        LONGLONG hyPreroll = 0;
        IDecoderDriver *piDecoderDriver = GetDecoderDriver();
        if (piDecoderDriver)
            hr = piDecoderDriver->GetPreroll(hyPreroll);
        *pllPreroll = hyPreroll;
    }

    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetPreroll() returning error HRESULT %d\n"), this, hr));
    }
    return hr;
} // CSampleConsumer::GetPreroll

///////////////////////////////////////////////////////////////////////
//
//  Class CSampleConsumer -- Protected methods
//
///////////////////////////////////////////////////////////////////////

void CSampleConsumer::Cleanup(SAMPLE_CONSUMER_INTERNAL_EVENT_TYPE eSCInternalEventType)
{
    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer(%p)::Cleanup()\n"), this));
    try {
        ClearPlaybackPosition(eSCInternalEventType);
        ClearRecordingHistory();
        m_fInRateChange = false;
        if (m_pifsrcfNext)
        {
            m_pifsrcfNext = 0;
        }
        if (m_pisbmsNext)
        {
            m_pisbmsNext = 0;
        }
        if (m_piStreamBufferPlayback)
            m_piStreamBufferPlayback = 0;
        m_bOutputPins = 0;
        m_vcpssOutputPinMapping.clear();
        m_pippmgr = 0;
        m_pguidCurTimeFormat = TIME_FORMAT_MEDIA_TIME;
        m_eStrmBufPlaybackTunePolicy = STRMBUF_PLAYBACK_TUNE_FLUSH_AND_GO_TO_LIVE;
        m_dblRate = 1.0;
        m_dblPreDeferNormalRate = 1.0;
        m_pipbmgrNext = 0;
        m_eFilterStateProducer = State_Stopped;
        m_uSampleProducerModeIncarnation = 0;
        ClearPauseBufferCallbacks();
        m_piReader = 0;
        if (m_piDecoderDriver)
            m_piDecoderDriver->UnregisterIdleAction(this);
        m_piDecoderDriver = 0;
        m_eSCLoadState = SC_LOAD_NEEDED;
        m_fSampleMightBeDiscontiguous = false;
        m_fFlushing = false;
        m_fThrottling = true;
        m_hyPostFlushPosition = POST_FLUSH_POSITION_INVALID;
        m_hyPostTunePosition = POST_FLUSH_POSITION_INVALID;
        if (m_pcPinMappings)
        {
            delete m_pcPinMappings;
            m_pcPinMappings = NULL;
        }
#if defined(SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT)
        m_rtProducerToConsumerClockOffset = 0;
        m_piGraphClock = NULL;
#endif // SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT
    }
    catch (const std::exception& rcException)
    {
        UNUSED (rcException);  // suppress release build warning
#ifdef UNICODE
        DbgLog((LOG_ERROR, 3,
            _T("CSampleConsumer(%p)::Cleanup() caught exception %s\n"),
            this, rcException.what()));
#else
        DbgLog((LOG_ERROR, 3,
            _T("CSampleConsumer(%p)::Cleanup() caught exception %S\n"),
            this, rcException.what()));
#endif
    }
} // CSampleConsumer::Cleanup

#ifndef SHIP_BUILD
static const wchar_t *GetEventName(SAMPLE_CONSUMER_INTERNAL_EVENT_TYPE eEventType)
{
    const wchar_t *pwszEventName = L"????";

    switch (eEventType)
    {
    case SC_INTERNAL_EVENT_LOAD:
        pwszEventName = L"SC_INTERNAL_EVENT_LOAD";
        break;

    case SC_INTERNAL_EVENT_ACTIVATE_SOURCE:
        pwszEventName = L"SC_INTERNAL_EVENT_ACTIVATE_SOURCE";
        break;

    case SC_INTERNAL_EVENT_DEACTIVATE_SOURCE:
        pwszEventName = L"SC_INTERNAL_EVENT_DEACTIVATE_SOURCE";
        break;

    case SC_INTERNAL_EVENT_REQUESTED_SET_RATE_NORMAL:
        pwszEventName = L"SC_INTERNAL_EVENT_REQUESTED_SET_RATE_NORMAL";
        break;

    case SC_INTERNAL_EVENT_REQUESTED_SET_RATE_RESUMING:
        pwszEventName = L"SC_INTERNAL_EVENT_REQUESTED_SET_RATE_RESUMING";
        break;

    case SC_INTERNAL_EVENT_RUN_SOURCE:
        pwszEventName = L"SC_INTERNAL_EVENT_RUN_SOURCE";
        break;

    case SC_INTERNAL_EVENT_SINK_STATE:
        pwszEventName = L"SC_INTERNAL_EVENT_SINK_STATE";
        break;

    case SC_INTERNAL_EVENT_SINK_BIND:
        pwszEventName = L"SC_INTERNAL_EVENT_SINK_BIND";
        break;

    case SC_INTERNAL_EVENT_SINK_UNBIND:
        pwszEventName = L"SC_INTERNAL_EVENT_SINK_UNBIND";
        break;

    case SC_INTERNAL_EVENT_READER_SAMPLE:
        pwszEventName = L"SC_INTERNAL_EVENT_READER_SAMPLE";
        break;

    case SC_INTERNAL_EVENT_PRODUCER_SAMPLE:
        pwszEventName = L"SC_INTERNAL_EVENT_PRODUCER_SAMPLE";
        break;

    case SC_INTERNAL_EVENT_PAUSE_BUF:
        pwszEventName = L"SC_INTERNAL_EVENT_PAUSE_BUF";
        break;

    case SC_INTERNAL_EVENT_LOST_PIPELINE:
        pwszEventName = L"SC_INTERNAL_EVENT_LOST_PIPELINE";
        break;

    case SC_INTERNAL_EVENT_PLAYED_TO_SOURCE:
        pwszEventName = L"SC_INTERNAL_EVENT_PLAYED_TO_SOURCE";
        break;

    case SC_INTERNAL_EVENT_DESTRUCTOR:
        pwszEventName = L"SC_INTERNAL_EVENT_DESTRUCTOR";
        break;

    case SC_INTERNAL_EVENT_END_OF_STREAM:
        pwszEventName = L"SC_INTERNAL_EVENT_END_OF_STREAM";
        break;

    case SC_INTERNAL_EVENT_GRAPH_ERROR_STOP:
        pwszEventName = L"SC_INTERNAL_EVENT_GRAPH_ERROR_STOP";
        break;

    case SC_INTERNAL_EVENT_CONSUMER_GRAPH_STOP:
        pwszEventName = L"SC_INTERNAL_EVENT_CONSUMER_GRAPH_STOP";
        break;

    case SC_INTERNAL_EVENT_POSITION_FLUSH:
        pwszEventName = L"SC_INTERNAL_EVENT_POSITION_FLUSH";
        break;

    case SC_INTERNAL_EVENT_AVOIDING_TRUNCATION:
        pwszEventName = L"SC_INTERNAL_EVENT_AVOIDING_TRUNCATION";
        break;

    case SC_INTERNAL_EVENT_FLUSH_DONE:
        pwszEventName = L"SC_INTERNAL_EVENT_FLUSH_DONE";
        break;
    }
    return pwszEventName;
}
#endif // !SHIP_BUILD

ROUTE CSampleConsumer::UpdateMediaSource(SAMPLE_CONSUMER_INTERNAL_EVENT_TYPE eSCInternalEventType)
{
#ifndef SHIP_BUILD
#ifdef UNICODE
    DbgLog((LOG_SAMPLE_CONSUMER, 4,
        _T("CSampleConsumer::UpdateMediaSource(%s):  state %d, position mode %d, position %I64d, source %d, producer mode %d [%d], rate %lf [%lf]\n"),
        GetEventName(eSCInternalEventType), m_eSCFilterState, m_eSCPositionMode, m_hyCurrentPosition,
        m_eSampleSource, m_eSampleProducerMode, m_eSampleProducerActiveMode, m_dblRate, GetTrueRate() ));
#else
    DbgLog((LOG_SAMPLE_CONSUMER, 4,
        _T("CSampleConsumer::UpdateMediaSource(%S):  state %d, position mode %d, position %I64d, source %d, producer mode %d [%d], rate %lf [%lf]\n"),
        GetEventName(eSCInternalEventType), m_eSCFilterState, m_eSCPositionMode, m_hyCurrentPosition,
        m_eSampleSource, m_eSampleProducerMode, m_eSampleProducerActiveMode, m_dblRate, GetTrueRate()));
#endif
#endif // !SHIP_BUILD
    bool fMustKeepNewPosition = false;
    ROUTE eRouteDefault = HANDLED_CONTINUE;

    // Step 1:  bail if we are either not supposed to run (e.g., because we
    //          just reached our stop position) or if we have no source of
    //          media samples:

    if (m_eSCPositionMode == SC_POSITION_PENDING_RUN)
    {
        if (eSCInternalEventType != SC_INTERNAL_EVENT_ACTIVATE_SOURCE)
            return HANDLED_STOP;
        m_hyCurrentPosition = m_hyTargetPosition;
        fMustKeepNewPosition = true;
        SetPositionMode(SC_POSITION_ALIGNING);
    }
    else if ((m_eSCPositionMode == SC_POSITION_PENDING_SET_POSITION) ||
             (m_eSCPositionMode == SC_POSITION_PENDING_NORMAL_RATE))
        return HANDLED_STOP;

    if (EnforceStopPosition() ||
        !CanDeliverMediaSamples(eSCInternalEventType == SC_INTERNAL_EVENT_REQUESTED_SET_RATE_RESUMING) ||
        (eSCInternalEventType == SC_INTERNAL_EVENT_DEACTIVATE_SOURCE) ||
        (eSCInternalEventType == SC_INTERNAL_EVENT_LOST_PIPELINE) ||
        (eSCInternalEventType == SC_INTERNAL_EVENT_DESTRUCTOR))
    {
        DisableMediaSource(eSCInternalEventType);
        eRouteDefault = HANDLED_STOP;
        goto exitUpdateMediaSource;
    }

    // Phase 2:  Make sure out data structures are in place for
    //           doing routing:

    if ((eSCInternalEventType == SC_INTERNAL_EVENT_ACTIVATE_SOURCE) ||
        (eSCInternalEventType == SC_INTERNAL_EVENT_LOAD) ||
        (eSCInternalEventType == SC_INTERNAL_EVENT_SINK_BIND) ||
        (eSCInternalEventType == SC_INTERNAL_EVENT_SINK_UNBIND))
    {
        MapPins();
        EnsureRecordingIsSelected();
    }

    // Phase 3: Figure out if we need to move to a different recording and, if so,
    //          update the recording selection state and possibly position:

    bool seekOverOrphan = false;

    // Check for hints of program logic bugs:
    ASSERT((m_hyCurrentPosition >= 0) ||
           (m_eSCPositionMode == SC_POSITION_UNKNOWN) ||
           (m_eSCPositionMode == SC_POSITION_AWAITING_FIRST_SAMPLE) ||
           (m_eSCPositionMode == SC_POSITION_ALIGNING) ||
           (eSCInternalEventType == SC_INTERNAL_EVENT_END_OF_STREAM));
    ASSERT((STRMBUF_STAY_WITH_RECORDING == m_esbptp) || !m_pcPauseBufferSegmentBound);
    ASSERT((STRMBUF_STAY_WITH_SINK == m_esbptp) || !m_pcPauseBufferSegmentBound || (m_pcPauseBufferSegmentBound == m_pcPauseBufferSegment));

    // If we know enough about the recording to set our current position, do so now:
    if (m_pcPauseBufferSegment &&
        (m_pcPauseBufferSegment->CurrentEndTime() >= 0))
    {
        if ((m_hyCurrentPosition < 0) ||
            (m_eSCPositionMode == SC_POSITION_UNKNOWN))
        {
            // We haven't seen a media sample yet and haven't been told any particular
            // position. However we do have information on the time range of this
            // recording. We'll pick our position based on the recording's time range.
            // If we are bound to this recording or it is not in-progress, we'll pick
            // its starting time. If we are not bound to a recording and the current
            // recording is in progress, we'll pick its 'live' position.

            if ((m_pcPauseBufferSegmentBound == m_pcPauseBufferSegment) ||
                !m_pcPauseBufferSegment->m_fInProgress)
            {
                m_hyCurrentPosition = m_pcPauseBufferSegment->CurrentStartTime();
                if (!m_pcPauseBufferSegment->IsPermanent() &&
                    (m_hyCurrentPosition < m_pcPauseBufferHistory->GetSafePlaybackStart(GetPauseBufferSafetyMargin(true))))
                    m_hyCurrentPosition =  m_pcPauseBufferHistory->GetSafePlaybackStart(GetPauseBufferSafetyMargin(true));
                CHECK_POSITION(m_hyCurrentPosition);
                SetPositionMode(SC_POSITION_AWAITING_FIRST_SAMPLE);
                fMustKeepNewPosition = true;
            }
            else
            {
                SetCurrentPositionToTrueSegmentEnd(m_pcPauseBufferSegment, (m_hyTargetOffsetFromLive != 0));
                SetPositionMode(SC_POSITION_AWAITING_FIRST_SAMPLE);
                fMustKeepNewPosition = true;
            }
            DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  initialized current position to %I64d from pause buffer info\n"),
                    m_hyCurrentPosition));
        }
        else if (m_hyCurrentPosition > m_pcPauseBufferSegment->CurrentEndTime())
        {
            // Hmm. We've moved outside of the range of the current recording.
            if (m_pcPauseBufferSegment->m_fInProgress && (GetTrueRate() > 0.0))
            {
                // Okay, we could've plausibly seen a sample beyond the old
                // end-of-recording because we're playing forward in a recording
                // in progress. Update the end time and make sure our rate is
                // no more than 1.0.

                ASSERT(m_pcPauseBufferSegment->m_hyEndTime < m_hyCurrentPosition);
                m_pcPauseBufferSegment->m_hyEndTime = m_hyCurrentPosition;
            }
            else if (GetTrueRate() > 0.0)
            {
                // We could plausibly be playing forward into another pause
                // buffer segment. Stop forward playback if we've reached the
                // first sample after the recording to which we are bound.
                // Otherwise advance to the next pause buffer. If there is not
                // one, something is very wrong [doubtless a programming ].

                if ((STRMBUF_STAY_WITH_RECORDING == m_esbptp) && m_pcPauseBufferSegmentBound)
                {
                    DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::UpdateMediaSource() -- stopping playback AFTER the end of a bound permanent recording (at %I64d, ends at %I64d)\n"),
                            this, m_hyCurrentPosition, m_pcPauseBufferSegment->CurrentEndTime()));
                    SetPositionMode(SC_POSITION_AT_STOP_POSITION);
                    StopGraph(PLAYBACK_AT_END);
                    SetCurrentPositionToTrueSegmentEnd(m_pcPauseBufferSegmentBound, false);
                    fMustKeepNewPosition = true;
                    goto exitUpdateMediaSource;
                }
                else
                {
                    CSmartRefPtr<SampleProducer::CPauseBufferSegment> pNewSegment;
                    if (m_pcPauseBufferHistory)
                        pNewSegment.Attach(m_pcPauseBufferHistory->FindSegmentByTime(m_hyCurrentPosition,
                            SampleProducer::CPauseBufferHistory::PREFER_EARLIER_TIME,
                            m_pcPauseBufferSegment));
                    if (pNewSegment)
                    {
                        DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  played into the next recording, current is %s)\n"),
                            m_pcPauseBufferSegment->m_fIsOrphaned ? _T("orphaned") : _T("normal") ));
                        if (m_pcPauseBufferSegment->m_fIsOrphaned)
                        {
                            seekOverOrphan = true;
                            SampleProducer::CPauseBufferSegment *pNextRecording = pNewSegment;
                            LeaveOrphanedRecording(pNextRecording);
                            fMustKeepNewPosition = true;
                            pNewSegment = pNextRecording;
                            if (pNewSegment && !pNewSegment->m_fIsOrphaned)
                                SetCurrentPositionToTrueSegmentEnd(pNewSegment, true);
                        }
                        m_pcPauseBufferSegment = pNewSegment;
#ifdef UNICODE
                        DbgLog((LOG_PAUSE_BUFFER, 3,
                            _T("CONSUMER:  updated current recording to %s\n"),
                            m_pcPauseBufferSegment ? m_pcPauseBufferSegment->GetRecordingName().c_str() : L"[none]" ));
#else
                        DbgLog((LOG_PAUSE_BUFFER, 3,
                            _T("CONSUMER:  updated current recording to %S\n"),
                            m_pcPauseBufferSegment ? m_pcPauseBufferSegment->GetRecordingName().c_str() : L"[none]" ));
#endif
                    }
                    else
                    {
                        ASSERT(0);
                        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::UpdateMediaSource() -- played beyond end-of-recording but no recording there (@%I64d, ends %I64d)\n"),
                            this, m_hyCurrentPosition, m_pcPauseBufferSegment->CurrentEndTime() ));
                        SetPositionMode(SC_POSITION_AT_STOP_POSITION);
                        NoteBadGraphState(E_FAIL);
                        SetCurrentPositionToTrueSegmentEnd(m_pcPauseBufferSegment, false);
                        fMustKeepNewPosition = true;
                        goto exitUpdateMediaSource;
                    }
                } /* end not bound to a recording */
            } /* end playing forward */
            else /* m_dblRate <= 0.0 */
            {
                // We played backward and wound up before the current recording. This
                // sounds really wierd but potentially the sink filter did a transition
                // that converted some of this (temporary) recording into a newly created
                // permanent recording and we should now be within the permanent recording.
                //
                // It is also possible that we were playing forward, hit end-of-recording,
                // and then were told to switch to rewinding. In that case either there was
                // a (as above),
                // the recording was temporary and got truncated by conversion.

                ASSERT(!m_pcPauseBufferSegment->IsPermanent() &&
                       (m_pcPauseBufferSegment != m_pcPauseBufferSegmentBound));

                if ((STRMBUF_STAY_WITH_RECORDING == m_esbptp) && m_pcPauseBufferSegmentBound)
                {
                    // We don't get here unless there was an earlier error. Chatter
                    // and add a safe-guard. We can't play outside of the bounds of a
                    // permanent recording.
                    DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::UpdateMediaSource() -- stopping rewind AFTER the end of a bound permanent recording (at %I64d, ends at %I64d)\n"),
                            this, m_hyCurrentPosition, m_pcPauseBufferSegment->CurrentEndTime()));
                    SetPositionMode(SC_POSITION_AT_STOP_POSITION);
                    StopGraph(PLAYBACK_AT_END);
                    eRouteDefault = HANDLED_STOP;
                    goto exitUpdateMediaSource;
                }

                CSmartRefPtr<SampleProducer::CPauseBufferSegment> pNewSegment;
                if (m_pcPauseBufferHistory)
                    pNewSegment.Attach(m_pcPauseBufferHistory->FindSegmentByTime(m_hyCurrentPosition,
                        SampleProducer::CPauseBufferHistory::PREFER_LATER_TIME,
                        m_pcPauseBufferSegment));
                if (pNewSegment)
                {
                    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  played backward(!) into the next recording, current is %s)\n"),
                            m_pcPauseBufferSegment->m_fIsOrphaned ? _T("orphaned") : _T("normal") ));
                    if (m_pcPauseBufferSegment->m_fIsOrphaned)
                    {
                        seekOverOrphan = true;
                        SampleProducer::CPauseBufferSegment *pNextRecording = pNewSegment;
                        LeaveOrphanedRecording(pNextRecording);
                        fMustKeepNewPosition = true;
                        pNewSegment = pNextRecording;
                        if (pNewSegment && !pNewSegment->m_fIsOrphaned)
                            SetCurrentPositionToTrueSegmentEnd(pNewSegment, true);
                    }
                    m_pcPauseBufferSegment = pNewSegment;
#ifdef UNICODE
                    DbgLog((LOG_PAUSE_BUFFER, 3,
                        _T("CONSUMER:  updated current recording to %s\n"),
                            m_pcPauseBufferSegment ? m_pcPauseBufferSegment->GetRecordingName().c_str() : L"[none]"));
#else
                    DbgLog((LOG_PAUSE_BUFFER, 3,
                        _T("CONSUMER:  updated current recording to %S\n"),
                            m_pcPauseBufferSegment ? m_pcPauseBufferSegment->GetRecordingName().c_str() : L"[none]"));
#endif
                }
                else
                {
                    DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::UpdateMediaSource() -- current position %I64d is after the current recording (%I64d) but no recording exists\n"),
                            this, m_hyCurrentPosition, m_pcPauseBufferSegment->CurrentEndTime()));
                    ASSERT(0);
                    SetPositionMode(SC_POSITION_AT_STOP_POSITION);
                    NoteBadGraphState(E_FAIL);
                    SetCurrentPositionToTrueSegmentEnd(m_pcPauseBufferSegment, false);
                    fMustKeepNewPosition= true;
                    goto exitUpdateMediaSource;
                }
            } /* played backward and the world changed out from under us to put us after the recording */
        } /* end played beyond the end of the current recording */
        else if (m_hyCurrentPosition < m_pcPauseBufferSegment->CurrentStartTime())
        {
            // Hmm. We've moved outside of the range of the current recording
            // but this time off the early end.

            if ((eSCInternalEventType == SC_INTERNAL_EVENT_ACTIVATE_SOURCE) &&
                !m_pcPauseBufferSegment->IsPermanent() &&
                (m_hyCurrentPosition <=
                    m_pcPauseBufferHistory->GetSafePlaybackStart(GetPauseBufferSafetyMargin(true))))
            {
                // What happened is that we were inactive or slow long enough for
                // the pause buffer to truncate the part of the temporary
                // recording containing our prior position.  We need to
                // jump to a saner position and rate.

                DbgLog((LOG_EVENT_DETECTED, 3, _T("CONSUMER:  paused long enough to fall outside recording\n") ));

                // Heads up:  we need to report the new rate first, so that the event processing of end-of-buffer
                //            sees the new rate (at least, so the WinPVR guide app expects).
                if (GetTrueRate() < 1.0)
                {
                    m_fDroppingToNormalPlayAtBeginning = true;
                    SetGraphRate(1.0);
                }
                if (m_piDecoderDriver)
                    m_piDecoderDriver->IssueNotification(this, DVRENGINE_EVENT_BEGINNING_OF_PAUSE_BUFFER,
                            DVRENGINE_PAUSED_UNTIL_TRUNCATION, m_dwLoadIncarnation);

                DbgLog((LOG_SAMPLE_CONSUMER, 4, _T("CSampleConsumer::UpdateMediaSource() [3]:  setting position from %I64d to %I64d\n"),
                    m_hyCurrentPosition, m_pcPauseBufferHistory->GetSafePlaybackStart(GetPauseBufferSafetyMargin(true)) ));
                m_hyCurrentPosition = m_pcPauseBufferHistory->GetSafePlaybackStart(GetPauseBufferSafetyMargin(true));
                fMustKeepNewPosition = true;
                CHECK_POSITION(m_hyCurrentPosition);
                m_fSampleMightBeDiscontiguous = true;
                if (m_hyCurrentPosition < m_pcPauseBufferSegment->CurrentStartTime())
                {
                    m_hyCurrentPosition = m_pcPauseBufferSegment->CurrentStartTime();
                    DbgLog((LOG_SAMPLE_CONSUMER, 4, _T("CSampleConsumer::UpdateMediaSource() [3]:  correcting position to start time, now %I64d\n"),
                        m_hyCurrentPosition ));
                    CHECK_POSITION(m_hyCurrentPosition);
                }
            }
            else if (GetTrueRate() < 0.0)
            {
                // We could plausibly be playing backward into another pause
                // buffer segment. Stop backward playback if we've reached the
                // first sample before the recording to which we are bound.
                // Otherwise advance to the previous recording in the pause buffer.
                // If there is not one, something is very wrong [doubtless a programming ].

                if ((STRMBUF_STAY_WITH_RECORDING == m_esbptp) && m_pcPauseBufferSegmentBound)
                {
                    DbgLog((LOG_EVENT_DETECTED, 3, _T("CONSUMER:  Stopping at bound-recording start (@%I64d, begins %I64d)\n"),
                            m_hyCurrentPosition, m_pcPauseBufferSegment->CurrentStartTime()));

                    SetPositionMode(SC_POSITION_AT_STOP_POSITION);
                    StopGraph(PLAYBACK_AT_BEGINNING);
                    DbgLog((LOG_SAMPLE_CONSUMER, 4, _T("CSampleConsumer::UpdateMediaSource() [5]:  setting position from %I64d to %I64d\n"),
                        m_hyCurrentPosition, m_pcPauseBufferSegmentBound->CurrentStartTime() ));
                    m_hyCurrentPosition = m_pcPauseBufferSegmentBound->CurrentStartTime();
                    fMustKeepNewPosition = true;
                    eRouteDefault = HANDLED_STOP;
                    goto exitUpdateMediaSource;
                }
                else
                {
                    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  played backward into the previous recording\n") ));
                    CSmartRefPtr<SampleProducer::CPauseBufferSegment> pNewSegment;
                    if (m_pcPauseBufferHistory)
                        pNewSegment.Attach(m_pcPauseBufferHistory->FindSegmentByTime(m_hyCurrentPosition,
                            SampleProducer::CPauseBufferHistory::PREFER_LATER_TIME,
                            m_pcPauseBufferSegment));
                    if (pNewSegment)
                    {
                        ASSERT(!m_pcPauseBufferSegment->m_fIsOrphaned);
                        ASSERT(!pNewSegment->m_fIsOrphaned);
                        m_pcPauseBufferSegment = pNewSegment;
#ifdef UNICODE
                        DbgLog((LOG_PAUSE_BUFFER, 3,
                            _T("CONSUMER:  updated current recording to %s\n"),
                            m_pcPauseBufferSegment ? m_pcPauseBufferSegment->GetRecordingName().c_str() : L"[none]"));
#else
                        DbgLog((LOG_PAUSE_BUFFER, 3,
                            _T("CONSUMER:  updated current recording to %S\n"),
                            m_pcPauseBufferSegment ? m_pcPauseBufferSegment->GetRecordingName().c_str() : L"[none]"));
#endif
                    }
                    else
                    {
                        DbgLog((LOG_EVENT_DETECTED, 3, _T("CONSUMER:  Stopping at last recording beginning (@%I64d, begins %I64d)\n"),
                                m_hyCurrentPosition, m_pcPauseBufferSegment->CurrentStartTime()));
                        ASSERT(0);
                        SetPositionMode(SC_POSITION_AT_STOP_POSITION);
                        NoteBadGraphState(E_FAIL);
                        DbgLog((LOG_SAMPLE_CONSUMER, 4, _T("CSampleConsumer::UpdateMediaSource() [5]:  setting position from %I64d to %I64d\n"),
                            m_hyCurrentPosition, m_pcPauseBufferSegment->CurrentStartTime() ));
                        m_hyCurrentPosition = m_pcPauseBufferSegment->CurrentStartTime();
                        fMustKeepNewPosition = true;
                        eRouteDefault = HANDLED_STOP;
                        goto exitUpdateMediaSource;
                    }
                } /* end not bound to a recording */
            } /* end playing forward */
            else /* m_dblRate >= 0.0 */
            {
                // We played forward and wound up before the current recording. This
                // sounds really wierd but potentially the sink filter did a transition
                // that converted some of this (temporary) recording into a newly created
                // permanent recording and we should now be within the permanent recording.

                if ((STRMBUF_STAY_WITH_RECORDING == m_esbptp) && m_pcPauseBufferSegmentBound)
                {
                    DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::UpdateMediaSource() -- current position %I64d is BEFORE the beginning of the bound recording (%I64d), stopping to avoid more trouble\n"),
                            this, m_hyCurrentPosition, m_pcPauseBufferSegment->CurrentStartTime()));
                    SetPositionMode(SC_POSITION_AT_STOP_POSITION);
                    StopGraph(PLAYBACK_AT_BEGINNING);
                    eRouteDefault = HANDLED_STOP;
                    goto exitUpdateMediaSource;
                }
                CSmartRefPtr<SampleProducer::CPauseBufferSegment> pNewSegment;
                if (m_pcPauseBufferHistory)
                    pNewSegment.Attach(m_pcPauseBufferHistory->FindSegmentByTime(m_hyCurrentPosition,
                        SampleProducer::CPauseBufferHistory::PREFER_EARLIER_TIME,
                        m_pcPauseBufferSegment));
                if (pNewSegment)
                {
                    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  played forward (!) into the previous recording, current is %s)\n"),
                            m_pcPauseBufferSegment->m_fIsOrphaned ? _T("orphaned") : _T("normal") ));
                    if (m_pcPauseBufferSegment->m_fIsOrphaned)
                    {
                        seekOverOrphan = true;
                        SampleProducer::CPauseBufferSegment *pNextRecording = pNewSegment;
                        LeaveOrphanedRecording(pNextRecording);
                        fMustKeepNewPosition = true;
                        pNewSegment = pNextRecording;
                        if (pNewSegment && !pNewSegment->m_fIsOrphaned)
                            SetCurrentPositionToTrueSegmentEnd(pNewSegment, true);
                    }
                    m_pcPauseBufferSegment = pNewSegment;
#ifdef UNICODE
                    DbgLog((LOG_PAUSE_BUFFER, 3,
                        _T("CONSUMER:  updated current recording to %s\n"),
                            m_pcPauseBufferSegment ? m_pcPauseBufferSegment->GetRecordingName().c_str() : L"[none]"));
#else
                    DbgLog((LOG_PAUSE_BUFFER, 3,
                        _T("CONSUMER:  updated current recording to %S\n"),
                            m_pcPauseBufferSegment ? m_pcPauseBufferSegment->GetRecordingName().c_str() : L"[none]"));
#endif
                }
                else
                {
                    DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::UpdateMediaSource() -- current position %I64d is before the beginning of the current recording (%I64d) no recording before\n"),
                            this, m_hyCurrentPosition, m_pcPauseBufferSegment->CurrentStartTime()));
                    ASSERT(0);
                    SetPositionMode(SC_POSITION_AT_STOP_POSITION);
                    NoteBadGraphState(E_FAIL);
                    DbgLog((LOG_SAMPLE_CONSUMER, 4, _T("CSampleConsumer::UpdateMediaSource() [6]:  setting position from %I64d to %I64d\n"),
                        m_hyCurrentPosition, m_pcPauseBufferSegment->CurrentStartTime() ));
                    m_hyCurrentPosition = m_pcPauseBufferSegment->CurrentStartTime();
                    fMustKeepNewPosition = true;
                    eRouteDefault = HANDLED_STOP;
                }
            } /* played backward and the world changed out from under us to put us after the recording */
        }
        else if (m_hyCurrentPosition == m_pcPauseBufferSegment->CurrentEndTime())
        {
            // We have reached the exact end position of the recording. If we're
            // playing backwards, there is nothing special to do -- we'll just
            // play into the new recording. If we are playing forward, what
            // we don't know based on position is whether there are non-primary
            // pin samples still to come that aren't tagged with this position
            // or whether this position reflects a non-key-frame that is split
            // among multiple media samples. EndOfStream() will be called if
            // this is the end of a completed recording that is not part of
            // the pause buffer. Otherwise, we'll get another sample later. So
            // for now, do nothing.
        }
        else /* must be positioned exactly at the beginning of the recording */
        {
            // Again, we have reached the exact beginning position of a recording.
            // If we're playing forwards, we'll just play into the recording.
            // If we're playing backwards, either we will eventually get and
            // end-of-stream notification (handled elsewhere) or we will get
            // another sample with an earlier time-stamp, which will drive us
            // to the next recording.
        }
    }


    // Phase 4:  We know that the position and recording choice correspond.
    //           We know that if we would've returned already if this sample
    //           or event had pushed us outside of the range of the bound
    //           recording or pause buffer.  Now we need to evaluate whether
    //           we need to go to 1x forward play to avoid falling out of
    //           the safe range of a temporary recording or pace the creation
    //           of a recording-in-progress.

    if (((m_dblRate != 1.0) || Paused()) &&
        m_pcPauseBufferSegment &&
        (m_pcPauseBufferSegment->CurrentEndTime() >= 0) &&
        (m_hyCurrentPosition >= 0) &&
        (m_eSCPositionMode != SC_POSITION_UNKNOWN))
    {
        if ((m_dblRate > 1.0) &&
            !Paused() &&
            m_pcPauseBufferSegment->m_fInProgress &&
            (m_hyCurrentPosition >= m_pcPauseBufferSegment->CurrentEndTime()))
        {
            // We're reached the forward end of a recording-in-progress.
            // We should drop back to the rate 1.0.

            DbgLog((LOG_EVENT_DETECTED, 3, _T("CONSUMER:  Dropping to 1x for in-progress (@%I64d, ends %I64d)\n"),
                    m_hyCurrentPosition, m_pcPauseBufferSegment->CurrentEndTime()));

            // Set the graph rate first to support the expectations of the guide app and nslav about
            // whether the rate is corrected before the end is set.

            long lEndEventSubtype = ((STRMBUF_STAY_WITH_RECORDING == m_esbptp) &&
                    (m_pcPauseBufferSegment == m_pcPauseBufferSegmentBound)) ?
                        DVRENGINE_LIVE_POSITION_IN_BOUND_RECORDING :
                        DVRENGINE_LIVE_POSITION_IN_LIVE_TV;
            SetGraphRate(1.0, true, -1LL, DVRENGINE_EVENT_END_OF_PAUSE_BUFFER, lEndEventSubtype, m_dwLoadIncarnation);
            if (m_eSCPositionMode == SC_POSITION_PENDING_NORMAL_RATE)
            {
                goto exitUpdateMediaSource;
            }
            if (m_piDecoderDriver)
                m_piDecoderDriver->IssueNotification(this, DVRENGINE_EVENT_END_OF_PAUSE_BUFFER, lEndEventSubtype, m_dwLoadIncarnation);
        }
        else if (((m_dblRate < 1.0) || Paused()) &&
                 !m_pcPauseBufferSegment->IsPermanent() &&
                 (m_hyCurrentPosition <=
                    m_pcPauseBufferSegment->CurrentStartTime() + GetPauseBufferSafetyMargin(false)))
        {
            // We are close to the beginning of this temporary recording. If it so
            // happens that we are safely away from the truncation point and there
            // is a recording before this one, we can continue as-is.
            bool fIsSafe = false;

            if ((m_dblRate >= 0.0) &&
                (m_hyCurrentPosition >= m_pcPauseBufferSegment->CurrentStartTime()) &&
                (m_hyCurrentPosition >= m_pcPauseBufferHistory->GetSafePlaybackStart(GetPauseBufferSafetyMargin(false))) &&
                (m_pcPauseBufferSegment->m_fInProgress ||
                (m_hyCurrentPosition <= m_pcPauseBufferSegment->CurrentEndTime())))
            {
                // We are safely away from the truncation point and correctly in the current recording. All is well.

                DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  will not report paused-until-start-of-media because we are safely in the recording, ahead of truncation, and not rewinding\n") ));
                fIsSafe = true;
            }
            else if (m_hyCurrentPosition > m_pcPauseBufferHistory->GetSafePlaybackStart(GetPauseBufferSafetyMargin(false)))
            {
                // We're safely away from the truncation point.  Is there another recording?

                DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  played backward/paused almost to the beginning of a temporary recording ...\n") ));
                CSmartRefPtr<SampleProducer::CPauseBufferSegment> pNewSegment;
                if (m_pcPauseBufferHistory)
                    pNewSegment.Attach(m_pcPauseBufferHistory->FindSegmentByTime(
                        m_pcPauseBufferSegment->CurrentStartTime() - 1,
                        SampleProducer::CPauseBufferHistory::PREFER_LATER_TIME,
                        m_pcPauseBufferSegment));
                if (pNewSegment)
                {
                    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  ... but there is a recording before this one so continue on\n") ));
                    fIsSafe = true;
                }
            }

            if (!fIsSafe)
            {
                // We're dangerously close to the about-to-be-deleted end of
                // a recording. Run at a rate of 1.0:
                DbgLog((LOG_EVENT_DETECTED, 3, _T("CONSUMER:  Dropping to 1x for temp beginning (@%I64d, begins %I64d)\n"),
                        m_hyCurrentPosition, m_pcPauseBufferSegment->CurrentStartTime()));
                m_fDroppingToNormalPlayAtBeginning = true;

                // Set the graph rate first to support the expectations of the guide app and nslav about
                // whether the rate is corrected before the end is set.

                long lBeginningEventSubtype = (GetTrueRate() < 0.0) ? DVRENGINE_REWOUND_TO_BEGINNING : DVRENGINE_SLOW_PLAY_UNTIL_TRUNCATION;
                SetGraphRate(1.0, true, -1, DVRENGINE_EVENT_BEGINNING_OF_PAUSE_BUFFER, lBeginningEventSubtype, m_dwLoadIncarnation);
                if (m_eSCPositionMode == SC_POSITION_PENDING_NORMAL_RATE)
                {
                    goto exitUpdateMediaSource;
                }
                if (m_piDecoderDriver)
                    m_piDecoderDriver->IssueNotification(this, DVRENGINE_EVENT_BEGINNING_OF_PAUSE_BUFFER, lBeginningEventSubtype, m_dwLoadIncarnation);
            }
        }
    }

    // Phase 5: We need to (re-)consider whether we want to pull samples
    //          from the reader versus the producer.

    SAMPLE_PRODUCER_MODE eSampleProducerModeNew = m_eSampleProducerMode;

    if (eSCInternalEventType == SC_INTERNAL_EVENT_READER_SAMPLE)
    {
        // A new sample never changes us to the producer unless we're
        // playing forward at a rate greater than 1x and we're in a
        // recording-in-progress or a very newly ended recording. Even
        // then, we only change if we are
        // sufficiently close to the beginning of the recording-in-porgress

        if (m_piSampleProducer &&
            (GetTrueRate() > 0.0) &&
            m_pcPauseBufferSegment)
        {
            LONGLONG hyProducerMin, hyProducerMax;
            m_piSampleProducer->GetPositionRange(hyProducerMin, hyProducerMax);
            if ((m_hyCurrentPosition >= hyProducerMax) ||
                ((m_hyCurrentPosition >= hyProducerMin) &&
                 (m_hyCurrentPosition >= hyProducerMax - s_hyUnsafelyCloseToWriter)))
            {
                eSampleProducerModeNew = (m_hyCurrentPosition >= hyProducerMax) ?
                        PRODUCER_SEND_SAMPLES_LIVE : PRODUCER_SEND_SAMPLES_FORWARD;
                m_hyDesiredProducerPos = m_hyCurrentPosition;
                eRouteDefault = HANDLED_STOP;
                DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  Switching from Reader to Producer %s (@%I64d in [%I64d,%I64d])\n"),
                        (eSampleProducerModeNew == PRODUCER_SEND_SAMPLES_LIVE) ? _T("Live") : _T("Forward"),
                        m_hyCurrentPosition, hyProducerMin, hyProducerMax));
            }
        }
    }
    else if ((eSCInternalEventType == SC_INTERNAL_EVENT_PRODUCER_SAMPLE) ||
             (eSCInternalEventType == SC_INTERNAL_EVENT_FAILED_TO_GET_PRODUCER_SAMPLE))
    {
        // A new sample never changes us from the producer back to the
        // reader unless we are playing at a rate of less than 1x. A failed
        // attempt to get a sample also won't push us back to the reader
        // unless playing at less than 1x. If
        // we are playing in live mode, we need to watch out for lagging
        // behind and so falling outside the producer's range. If we are
        // scanning forward or backward through the cache, we'll get a
        // notification. So we just need to handle the live scenario here:

        if (PRODUCER_SEND_SAMPLES_LIVE == m_eSampleProducerMode)
        {
            ASSERT(m_hyCurrentPosition >= 0);  // we're supposed to know where we are -- we just got a sample to tell us

            LONGLONG hyProducerMin, hyProducerMax;
            m_piSampleProducer->GetPositionRange(hyProducerMin, hyProducerMax);
            ASSERT((hyProducerMax >= 0) && (hyProducerMax >= 0));  // The producer should know about a position since it just sent out a sample

            if (((m_hyCurrentPosition <= hyProducerMin) ||
                 ((GetTrueRate() < 1.0) && (m_hyCurrentPosition < hyProducerMax - s_hyUnsafelyCloseToWriter))) &&
                (SC_SAMPLE_SOURCE_PRODUCER == m_eSampleSource))
            {
                // We can't render any more from the producer -- let's see about
                // switching to the reader:
                LONGLONG hyMaxReaderPos = m_piReader ? m_piReader->GetMaxSeekPosition() : -1;
                if (m_piReader &&
                    (m_hyCurrentPosition <= hyMaxReaderPos))
                {
                    eSampleProducerModeNew = PRODUCER_SUPPRESS_SAMPLES;
                    eRouteDefault = HANDLED_STOP;
                    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  Falling back from the producer to the reader (@%I64d)\n"),
                            m_hyCurrentPosition));
                }
                else
                {
                    // Yikes -- we are not able to keep up rendering at 1x.
                    DbgLog((LOG_ERROR, 3, _T("CSampleConsumer(%p)::UpdateMediaSource(): Unable to fall back from the producer to the reader (@%I64d, reader ends @%I64d, producer [%I64d,%I64d])\n"),
                            this, m_hyCurrentPosition, hyMaxReaderPos, hyProducerMin, hyProducerMax));
                }
            }
        }
        if (eSCInternalEventType == SC_INTERNAL_EVENT_PRODUCER_SAMPLE)
            m_eSampleSource = SC_SAMPLE_SOURCE_PRODUCER;
    }
    else if (eSCInternalEventType == SC_INTERNAL_EVENT_PLAYED_TO_SOURCE)
    {
        // We've exhausted the producer's cache. If we're playing forward
        // at a rate greater than 1.0, drop back to 1.0. If we are playing
        // at a forward rate less than 1.0, we might as well keep using
        // producer samples until our position drops outside of the the
        // producer's range. If we are playing backward, switch to the
        // reader.
        eSampleProducerModeNew = PRODUCER_SEND_SAMPLES_LIVE;
        if (!m_piSampleProducer || (GetTrueRate() < 0.0))
        {
            eSampleProducerModeNew = PRODUCER_SUPPRESS_SAMPLES;
            DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  Producer end, playing backward (@%I64d)\n"),
                        m_hyCurrentPosition));
        }
        else if (GetTrueRate() < 1.0)
        {
            LONGLONG hyProducerMin, hyProducerMax;
            m_piSampleProducer->GetPositionRange(hyProducerMin, hyProducerMax);
            if ((hyProducerMin < 0) ||
                (m_hyCurrentPosition <= hyProducerMin))
                eSampleProducerModeNew = PRODUCER_SUPPRESS_SAMPLES;
            DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  End of Producer, playing slow (@%I64d in [%I64d,%I64d])\n"),
                    m_hyCurrentPosition, hyProducerMin, hyProducerMax));
        }
        else if (GetTrueRate() > 1.0)
        {
            DbgLog((LOG_EVENT_DETECTED, 3, _T("CONSUMER:  End of Producer, assumed live, dropping to 1x\n")));

            // Set the graph rate first to support the expectations of the guide app and nslav about
            // whether the rate is corrected before the end is set.

            long lEndEventSubtype = ((STRMBUF_STAY_WITH_RECORDING == m_esbptp) &&
                    (m_pcPauseBufferSegment == m_pcPauseBufferSegmentBound)) ?
                        DVRENGINE_LIVE_POSITION_IN_BOUND_RECORDING :
                        DVRENGINE_LIVE_POSITION_IN_LIVE_TV;
            SetGraphRate(1.0, true, -1, DVRENGINE_EVENT_END_OF_PAUSE_BUFFER, lEndEventSubtype, m_dwLoadIncarnation);
            if (m_eSCPositionMode == SC_POSITION_PENDING_NORMAL_RATE)
            {
                goto exitUpdateMediaSource;
            }
            if (m_piDecoderDriver)
                m_piDecoderDriver->IssueNotification(this, DVRENGINE_EVENT_END_OF_PAUSE_BUFFER, lEndEventSubtype, m_dwLoadIncarnation);
        }
        else
        {
            DbgLog((LOG_SAMPLE_CONSUMER, 4, _T("CONSUMER:  End of Producer, no special action\n")));
        }
    }
    else if (eSCInternalEventType == SC_INTERNAL_EVENT_END_OF_STREAM)
    {
        // We've exhausted the reader's range. If we are playing backward,
        // treat this as hitting a stop position. If we are playing forward
        // in a recording-in-progress, switch to the producer. If we are
        // playing forward in a completed-recording, treat this as hitting
        // a stop position.

        DbgLog((LOG_SAMPLE_CONSUMER, 4, _T("CONSUMER:  rate %lf, %s%s\n"),
            GetTrueRate(),
            m_fDroppingToNormalPlayAtBeginning ? TEXT("Dropping to normal play at beginning, ") : TEXT(""),
            (m_pcPauseBufferSegment && m_pcPauseBufferSegment->m_fInProgress && m_piSampleProducer) ?
            TEXT("live-tv w/segment in progress") : TEXT("either not live-tv or segment not in progress") ));

        if (((GetTrueRate() > 0.0) && !m_fDroppingToNormalPlayAtBeginning) &&
            m_pcPauseBufferSegment &&
            m_pcPauseBufferSegment->m_fInProgress &&
            m_piSampleProducer)
        {
            LONGLONG hyProducerMin, hyProducerMax;
            m_piSampleProducer->GetPositionRange(hyProducerMin, hyProducerMax);
            eSampleProducerModeNew = PRODUCER_SEND_SAMPLES_FORWARD;
            m_hyDesiredProducerPos = m_hyCurrentPosition;
            eRouteDefault = HANDLED_STOP;
            DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  End of Stream, switching to producer/forward (@%I64d in [%I64d,%I64d])\n"),
                    m_hyCurrentPosition, hyProducerMin, hyProducerMax));
        }
        else if (m_pcPauseBufferSegmentBound &&
                 (STRMBUF_STAY_WITH_RECORDING == m_esbptp))
        {
            DbgLog((LOG_EVENT_DETECTED, 3, _T("CONSUMER:  End-of-stream, stopping\n")));
            SetPositionMode(SC_POSITION_AT_STOP_POSITION);
            StopGraph((GetTrueRate() < 0.0) ? PLAYBACK_AT_BEGINNING : PLAYBACK_AT_END);
            eRouteDefault = HANDLED_STOP;
            goto exitUpdateMediaSource;
        }
        else
        {
            // We rewound to the beginning of a temporary buffer. We need to switch
            // to 1x forward in order to stay ahead of the truncation point.
            eRouteDefault = HANDLED_STOP;

            // Set the graph rate first to support the expectations of the guide app and nslav about
            // whether the rate is corrected before the end is set.

            long lBeginEventSubtype = (GetTrueRate() > 0.0) && !m_fDroppingToNormalPlayAtBeginning ? DVRENGINE_SLOW_PLAY_UNTIL_TRUNCATION : DVRENGINE_REWOUND_TO_BEGINNING;
            m_fDroppingToNormalPlayAtBeginning = true;
            SetGraphRate(1.0, true, -1, DVRENGINE_EVENT_BEGINNING_OF_PAUSE_BUFFER, lBeginEventSubtype, m_dwLoadIncarnation);
            if (m_eSCPositionMode == SC_POSITION_PENDING_NORMAL_RATE)
            {
                goto exitUpdateMediaSource;
            }

            DisableMediaSource(eSCInternalEventType);
            if (m_piDecoderDriver)
                m_piDecoderDriver->IssueNotification(this, DVRENGINE_EVENT_BEGINNING_OF_PAUSE_BUFFER, lBeginEventSubtype, m_dwLoadIncarnation);

            DbgLog((LOG_EVENT_DETECTED, 3, _T("CONSUMER:  End of Stream, rewind hit beginning of pause buffer [%I64d])\n"),
                    m_hyCurrentPosition));

            // We do **not** want to switch to the producer just because we cannot continue
            // rewinding using the reader.
            goto exitUpdateMediaSource;
        }
    }
    else
    {
        // The pause buffer or position or presence/absense of a producer
        // forces use to completely reevaluate:

        if (!m_piSampleProducer)
            eSampleProducerModeNew = PRODUCER_SUPPRESS_SAMPLES;
        else {
            LONGLONG hyProducerMin, hyProducerMax;
            m_piSampleProducer->GetPositionRange(hyProducerMin, hyProducerMax);
            if (m_hyCurrentPosition < 0)
            {
                // We don't know where we should play. If we are bound
                // to a recording, we would've know at least where we want
                // to be. So we must be bound to live. Default to the 'now'
                // position via the producer. If playing forward, that
                // means we want samples live. If playing backward and not
                // already grabbing the backward samples, grab them enmasse.

                DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  Dunno where we are but there is a producer, using it\n") ));
                ASSERT(!((STRMBUF_STAY_WITH_RECORDING == m_esbptp) && m_pcPauseBufferSegmentBound && !m_pcPauseBufferSegmentBound->m_fInProgress));
                if (GetTrueRate() > 0.0)
                {
                    eSampleProducerModeNew = PRODUCER_SEND_SAMPLES_LIVE;
                    DbgLog((LOG_SAMPLE_CONSUMER, 3, TEXT("CONSUMER:  adopting producer-live, assuming unsafe\n") ));
                    if ((hyProducerMax >= 0) && (hyProducerMin >= 0))
                    {
                        DbgLog((LOG_SAMPLE_CONSUMER, 3, TEXT("CONSUMER:  Starting a little before live to ensure 2 I-Frames\n") ));
                        eSampleProducerModeNew = PRODUCER_SEND_SAMPLES_FORWARD;
                        if (hyProducerMin + m_hyTargetOffsetFromLive > hyProducerMax)
                            m_hyDesiredProducerPos = hyProducerMin;
                        else
                        {
                            m_hyDesiredProducerPos = hyProducerMax - m_hyTargetOffsetFromLive;
                            DbgLog((LOG_SAMPLE_CONSUMER, 3, TEXT("CONSUMER:  capping position at safely behind live\n") ));
                        }
                    }

                }
                else if (GetTrueRate() < 0.0)
                {
                    eSampleProducerModeNew = PRODUCER_SEND_SAMPLES_BACKWARD;
                    m_hyDesiredProducerPos = -1;
                }
            }
            else if (hyProducerMax < 0)
            {
                // We know where we want to be but the producer is clueless.
                // This state could arise because we created the filter graphs
                // and did a seek here but haven't started either sink or source.
                // If the reader can handle this position, great. Otherwise let's
                // go with the live position in preference to no input:

                if (m_piReader &&
                    (m_hyCurrentPosition <= m_piReader->GetMaxSeekPosition()))
                {
                    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  producer has nothing but we're in range for the reader\n") ));
                    eSampleProducerModeNew = PRODUCER_SUPPRESS_SAMPLES;
                }
                else if (eSampleProducerModeNew == PRODUCER_SUPPRESS_SAMPLES)
                {
                    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  producer has nothing, not in reader range, will wait for live samples\n") ));
                    eSampleProducerModeNew = PRODUCER_SEND_SAMPLES_LIVE;
                }
            }
            else if (hyProducerMin < 0)
            {
                DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  Producer must've flushed, will try the reader\n") ));
                eSampleProducerModeNew = PRODUCER_SUPPRESS_SAMPLES;
            }
            else if (m_hyCurrentPosition < hyProducerMin)
            {
                DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  fell behind the producer (at %I64d, producer [%I64d,%I64d]\n"),
                    m_hyCurrentPosition, hyProducerMin, hyProducerMax));
                eSampleProducerModeNew = PRODUCER_SUPPRESS_SAMPLES;
            }
            else {
                // We want to use the sample producer. We need to decide if
                // we should pull en-masse or not.
                if (GetTrueRate() < 0.0)
                {
                    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  playback backward, in producer land (at %I64d, producer [%I64d,%I64d]\n"),
                        m_hyCurrentPosition, hyProducerMin, hyProducerMax));
                    eSampleProducerModeNew =
                        (m_hyCurrentPosition <= hyProducerMin) ?
                            PRODUCER_SUPPRESS_SAMPLES : PRODUCER_SEND_SAMPLES_BACKWARD;
                    m_hyDesiredProducerPos = m_hyCurrentPosition;
                    eRouteDefault = HANDLED_STOP;
                }
                else if (m_hyCurrentPosition >= hyProducerMax)
                {
                    if ((m_eSampleProducerMode == PRODUCER_SEND_SAMPLES_FORWARD) &&
                        (SC_INTERNAL_EVENT_PLAYED_TO_SOURCE != eSCInternalEventType))
                    {
                        // We're going forward already -- don't interrupt this because we might
                        // lose a critical sample:
                        DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  getting samples forward, at or ahead of the producer (at %I64d, producer [%I64d,%I64d]\n"),
                            m_hyCurrentPosition, hyProducerMin, hyProducerMax));
                    }
                    else
                    {
                        DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  at or ahead of the producer (at %I64d, producer [%I64d,%I64d]\n"),
                            m_hyCurrentPosition, hyProducerMin, hyProducerMax));
                        eSampleProducerModeNew = PRODUCER_SEND_SAMPLES_LIVE;
                        eRouteDefault = HANDLED_STOP;
                    }
                }
                else if (eSampleProducerModeNew != PRODUCER_SEND_SAMPLES_LIVE)
                {
                    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CONSUMER:  in producer range playing forward (at %I64d, producer [%I64d,%I64d]\n"),
                        m_hyCurrentPosition, hyProducerMin, hyProducerMax));
                    eSampleProducerModeNew = PRODUCER_SEND_SAMPLES_FORWARD;
                    m_hyDesiredProducerPos = m_hyCurrentPosition;
                    eRouteDefault = HANDLED_STOP;
                }
            }
        }
    }  /* end complete re-evaluation of reader vs producer */

    if (eSampleProducerModeNew == PRODUCER_SUPPRESS_SAMPLES)
    {
        // We know that we can't use the producer. See if we
        // can use the reader:

        DbgLog((LOG_SAMPLE_CONSUMER, 5, _T("CSampleConsumer::UpdateMediaSource():  cannot use producer\n")));

        if (!m_piReader ||
            (m_hyCurrentPosition < 0) ||
            (m_eSCPositionMode == SC_POSITION_UNKNOWN) ||
            !m_pcPauseBufferSegment ||
            (!m_piSampleProducer && (m_esbptp == STRMBUF_STAY_WITH_SINK)))
        {
            // There either is no reader or we don't know enough about what
            // we are to display to be able to tell the reader what to do.

            DbgLog((LOG_SOURCE_STATE, 3, _T("CONSUMER: disabling all inputs (@%I64d)\n"),
                    m_hyCurrentPosition));
            DisableMediaSource(eSCInternalEventType);
            return HANDLED_CONTINUE;
        }

        bool wereUsingReader = IsSourceReader();
        if (!wereUsingReader)
        {
            DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer(%p)::UpdateMediaSource() selecting the reader\n"), this));
        }
        bool fFormerlyUsingProducer = false;
        if (m_piSampleProducer &&
            (IsSourceProducer() ||
             (!wereUsingReader &&
              ((m_eSampleProducerMode != PRODUCER_SUPPRESS_SAMPLES) ||
               (m_eSampleProducerActiveMode != PRODUCER_SUPPRESS_SAMPLES)))))
        {
            m_eSampleProducerMode = PRODUCER_SUPPRESS_SAMPLES;
            {
                CAutoLock cAutoLock(&m_cCritSecThread);

                if (m_pcConsumerStreamingThread)
                    m_pcConsumerStreamingThread->SendProducerModeEvent();
            }
            fFormerlyUsingProducer = (m_hyCurrentPosition >= 0);
            DbgLog((LOG_SOURCE_STATE, 3, _T("CONSUMER: switching from producer to reader (@%I64d)\n"),
                    m_hyCurrentPosition));
        };
        if (!wereUsingReader &&
            (!m_piSampleProducer || !IsSourceProducer()))
        {
            DbgLog((LOG_SOURCE_STATE, 4, _T("CSampleConsumer::UpdateMediaSource() -- noting this might be a discontinuity\n")));
            m_fSampleMightBeDiscontiguous = true;
        }

        if (!wereUsingReader)
        {
            m_eSampleSource = SC_SAMPLE_SOURCE_CONVERTING_TO_READER;
        }
        else
        {
            m_eSampleSource = SC_SAMPLE_SOURCE_READER;
        }
        DWORD dwReaderSeekStatus = ERROR_SUCCESS;
        if (seekOverOrphan)
        {
            m_fSampleMightBeDiscontiguous = true;
            dwReaderSeekStatus = m_piReader->SetCurrentPosition(m_hyCurrentPosition, true);
        }
        else if ((eSCInternalEventType == SC_INTERNAL_EVENT_PRODUCER_SAMPLE) ||
                 (eSCInternalEventType == SC_INTERNAL_EVENT_PLAYED_TO_SOURCE))
        {
            dwReaderSeekStatus = m_piReader->SetCurrentPosition(m_hyCurrentPosition, false);
            eRouteDefault = HANDLED_STOP;
        }
        else if ((eSCInternalEventType != SC_INTERNAL_EVENT_READER_SAMPLE) &&
                 (eSCInternalEventType != SC_INTERNAL_EVENT_PAUSE_BUF) &&
                 (eSCInternalEventType != SC_INTERNAL_EVENT_RUN_SOURCE) &&
                 (eSCInternalEventType != SC_INTERNAL_EVENT_REQUESTED_SET_RATE_NORMAL))
            dwReaderSeekStatus = m_piReader->SetCurrentPosition(m_hyCurrentPosition, true);
        else if (fFormerlyUsingProducer)
        {
            dwReaderSeekStatus = m_piReader->SetCurrentPosition(m_hyCurrentPosition, false);
            eRouteDefault = HANDLED_STOP;
        }
        else if (!wereUsingReader)
        {
            DbgLog((LOG_SOURCE_STATE, 3, _T("CONSUMER: enabling reader with no seek (@%I64d)\n"),
                    m_hyCurrentPosition));
        }

        if (dwReaderSeekStatus != ERROR_SUCCESS)
        {
            DisableMediaSource(eSCInternalEventType);
            DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::UpdateMediaSource() -- IReader::SetCurrentPosition(%I64d) failed, legit range is [%I64d, %I64d]\n"),
                            this, m_hyCurrentPosition,
                            m_piReader->GetMinSeekPosition(),
                            m_piReader->GetMaxSeekPosition() ));
            SetPositionMode(SC_POSITION_AT_STOP_POSITION);
            NoteBadGraphState(E_FAIL);
            eRouteDefault = HANDLED_STOP;
            goto exitUpdateMediaSource;
        }
        if ((!wereUsingReader || (eSCInternalEventType == SC_INTERNAL_EVENT_END_OF_STREAM)) &&
            !m_fFlushing)
            m_piReader->SetIsStreaming(true);
        if ((eRouteDefault == HANDLED_STOP) && m_piDecoderDriver &&
            ((eSCInternalEventType == SC_INTERNAL_EVENT_PRODUCER_SAMPLE) ||
             (eSCInternalEventType == SC_INTERNAL_EVENT_END_OF_STREAM) ||
             (eSCInternalEventType == SC_INTERNAL_EVENT_READER_SAMPLE)))
        {
            DbgLog((LOG_SOURCE_DISPATCH, 3, _T("CONSUMER:  Discarding current sample (@%I64d)\n"),
                m_hyCurrentPosition));
        }
        return ((eSCInternalEventType == SC_INTERNAL_EVENT_PRODUCER_SAMPLE) ||
                (eSCInternalEventType == SC_INTERNAL_EVENT_END_OF_STREAM) ||
                (eSCInternalEventType == SC_INTERNAL_EVENT_READER_SAMPLE)) ? eRouteDefault : HANDLED_CONTINUE;
    }

    // We know that we are going to use the producer. We just have
    // to do the right magic to switch modes:

    DbgLog((LOG_SOURCE_DISPATCH, 5, _T("CSampleConsumer::UpdateMediaSource():  will use producer\n")));

    if (IsSourceReader() && m_piReader)
        m_piReader->SetIsStreaming(false);
    if (!IsSourceProducer() || (eSampleProducerModeNew != m_eSampleProducerMode))
    {
        DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer(%p)::UpdateMediaSource() switching to producer\n"), this));
        m_eSampleSource = SC_SAMPLE_SOURCE_CONVERTING_TO_PRODUCER;
        m_eSampleProducerMode = eSampleProducerModeNew;
        {
            CAutoLock cAutoLock(&m_cCritSecThread);
                if (m_pcConsumerStreamingThread)
                    m_pcConsumerStreamingThread->SendProducerModeEvent();
        }
        if ((m_eSampleProducerMode == PRODUCER_SEND_SAMPLES_LIVE) && (GetTrueRate() > 1.0))
        {
            DbgLog((LOG_EVENT_DETECTED, 3, _T("CONSUMER:  failsafe -- drop to 1x for Producer/Live\n")));

            // Change the rate first so that the guide app and TVPAL expectation about the arrivial
            // of rate change notification before end-of-buffer.

            long lEndEventSubtype = ((STRMBUF_STAY_WITH_RECORDING == m_esbptp) &&
                    (m_pcPauseBufferSegment == m_pcPauseBufferSegmentBound)) ?
                        DVRENGINE_LIVE_POSITION_IN_BOUND_RECORDING :
                        DVRENGINE_LIVE_POSITION_IN_LIVE_TV;
            SetGraphRate(1.0, true, -1, DVRENGINE_EVENT_END_OF_PAUSE_BUFFER, lEndEventSubtype, m_dwLoadIncarnation);
            if (m_eSCPositionMode == SC_POSITION_PENDING_NORMAL_RATE)
            {
                goto exitUpdateMediaSource;
            }
            if (m_piDecoderDriver)
                m_piDecoderDriver->IssueNotification(this, DVRENGINE_EVENT_END_OF_PAUSE_BUFFER, lEndEventSubtype, m_dwLoadIncarnation);
        }

        if (!m_fSampleMightBeDiscontiguous && (m_hyCurrentPosition >= 0))
        {
            LONGLONG hyProducerMin, hyProducerMax;
            m_piSampleProducer->GetPositionRange(hyProducerMin, hyProducerMax);
            if ((m_hyCurrentPosition < hyProducerMin) || (m_hyCurrentPosition > hyProducerMax))
                m_fSampleMightBeDiscontiguous = true;
        }
    }

exitUpdateMediaSource:
    if ((eRouteDefault == HANDLED_STOP) && m_piDecoderDriver &&
        ((eSCInternalEventType == SC_INTERNAL_EVENT_PRODUCER_SAMPLE) ||
            (eSCInternalEventType == SC_INTERNAL_EVENT_END_OF_STREAM) ||
            (eSCInternalEventType == SC_INTERNAL_EVENT_READER_SAMPLE)))
    {
        DbgLog((LOG_SOURCE_DISPATCH, 3, _T("CONSUMER:  Discarding current sample (@%I64d)\n"),
            m_hyCurrentPosition));
    }
    if (!fMustKeepNewPosition && (eRouteDefault == HANDLED_STOP))
        eRouteDefault = UNHANDLED_STOP;

#ifndef SHIP_BUILD
#ifdef UNICODE
    DbgLog((LOG_SAMPLE_CONSUMER, 4,
        _T("CSampleConsumer::UpdateMediaSource(%s) exit:  state %d, position mode %d, position %I64d, source %d, producer mode %d [%d], rate %lf [%lf]\n"),
        GetEventName(eSCInternalEventType), m_eSCFilterState, m_eSCPositionMode, m_hyCurrentPosition,
        m_eSampleSource, m_eSampleProducerMode, m_eSampleProducerActiveMode, m_dblRate, GetTrueRate() ));
#else
    DbgLog((LOG_SAMPLE_CONSUMER, 4,
        _T("CSampleConsumer::UpdateMediaSource(%S) exit:  state %d, position mode %d, position %I64d, source %d, producer mode %d [%d], rate %lf [%lf]\n"),
        GetEventName(eSCInternalEventType), m_eSCFilterState, m_eSCPositionMode, m_hyCurrentPosition,
        m_eSampleSource, m_eSampleProducerMode, m_eSampleProducerActiveMode, m_dblRate, GetTrueRate()));
#endif
#endif // !SHIP_BUILD

    return ((eSCInternalEventType == SC_INTERNAL_EVENT_PRODUCER_SAMPLE) ||
            (eSCInternalEventType == SC_INTERNAL_EVENT_END_OF_STREAM) ||
            (eSCInternalEventType == SC_INTERNAL_EVENT_READER_SAMPLE)) ? eRouteDefault : HANDLED_CONTINUE;
} // CSampleConsumer::UpdateMediaSource

void CSampleConsumer::DisableMediaSource(SAMPLE_CONSUMER_INTERNAL_EVENT_TYPE eSCInternalEventType)
{
    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer(%p)::DisableMediaSource()\n"), this));

    m_eSampleSource = SC_SAMPLE_SOURCE_NONE;
    m_fDroppingToNormalPlayAtBeginning = false;
    if (m_piSampleProducer && (m_eSampleProducerMode != PRODUCER_SUPPRESS_SAMPLES))
    {
        m_eSampleProducerMode = PRODUCER_SUPPRESS_SAMPLES;
        {
            CAutoLock cAutoLock(&m_cCritSecThread);

            if (m_pcConsumerStreamingThread)
                m_pcConsumerStreamingThread->SendProducerModeEvent();
        }
    }
    if (m_piReader)
        m_piReader->SetIsStreaming(false);
    if (GetDecoderDriver())
        GetDecoderDriver()->ImplementDisabledInputs(
            (eSCInternalEventType == SC_INTERNAL_EVENT_PRODUCER_SAMPLE) ||
            (eSCInternalEventType == SC_INTERNAL_EVENT_READER_SAMPLE));
}

void CSampleConsumer::CleanupProducer(void)
{
    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer(%p)::CleanupProducer()\n"), this));

    if (m_piSampleProducer)
    {
        try {
            m_piSampleProducer->UnbindConsumer(this);
        }
        catch (const std::exception& rcException)
        {
            UNUSED (rcException);  // suppress release build warning
#ifdef UNICODE
            DbgLog((LOG_ERROR, 2,
                _T("CSampleConsumer(%p)::CleanupProducer() caught exception %S\n"),
                this, rcException.what()));
#else
            DbgLog((LOG_ERROR, 2,
                _T("CSampleConsumer(%p)::CleanupProducer() caught exception %s\n"),
                this, rcException.what()));
#endif
        }
        m_piSampleProducer = NULL;
    }
#if defined(SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT)
    m_rtProducerToConsumerClockOffset = 0;
    m_piGraphClock = NULL;
#endif // SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT
}

IReader *CSampleConsumer::GetReader(void)
{
    if (!m_piReader && m_pippmgr)
    {
        CPipelineRouter cPipelineRouter = m_pippmgr->GetRouter(NULL, false, false);
        void *pvReader = NULL;
        cPipelineRouter.GetPrivateInterface(IID_IReader, pvReader);
        m_piReader = static_cast<IReader*>(pvReader);
    }
    return m_piReader;
}

IDecoderDriver *CSampleConsumer::GetDecoderDriver(void)
{
    if (!m_piDecoderDriver && m_pippmgr)
    {
        CPipelineRouter cPipelineRouter = m_pippmgr->GetRouter(NULL, false, false);
        void *pvDecoderDriver = NULL;
        cPipelineRouter.GetPrivateInterface(IID_IDecoderDriver, pvDecoderDriver);
        m_piDecoderDriver = static_cast<IDecoderDriver*>(pvDecoderDriver);
        if (m_piDecoderDriver)
            m_piDecoderDriver->RegisterIdleAction(this);
    }
    return m_piDecoderDriver;
}

bool CSampleConsumer::ViewingRecordingInProgress(void) const
{
    return (m_pcPauseBufferSegment && m_pcPauseBufferSegment->m_fInProgress);
}

void CSampleConsumer::RebindToProducer()
{
    DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer(%p)::RebindToProducer()\n"), this));

    if (m_pippmgr && *m_pszFileName.c_str())
    {
        if (!m_piSampleProducer ||
            !m_piSampleProducer->IsHandlingFile(m_pszFileName.c_str()))
        {
            if (m_piSampleProducer)
            {
                HRESULT hr = m_piSampleProducer->UnbindConsumer(this);
                m_piSampleProducer = NULL;
                if (FAILED(hr))
                    throw CHResultError(hr);
            }
            CSampleProducerLocator::BindToSampleProducer(this, m_pszFileName.c_str());
        }
        else if (m_piSampleProducer)
        {
            // If we are rebinding after a flush, we need to refresh our memory
            // of what the producer thinks the mode is. The message from the producer
            // changing the mode might've been lost during the flush:

            DbgLog((LOG_SOURCE_STATE, 4, _T("CSampleConsumer::RebindToProducer():  old producer mode %d desired, %d actual\n"),
                m_eSampleProducerMode, m_eSampleProducerActiveMode));
            m_piSampleProducer->GetModeForConsumer(this, m_eSampleProducerActiveMode, m_uSampleProducerModeIncarnation);
            m_eSampleProducerMode = m_eSampleProducerActiveMode;
            DbgLog((LOG_SOURCE_STATE, 4, _T("CSampleConsumer::RebindToProducer():  new producer mode %d desired, %d actual\n"),
                m_eSampleProducerMode, m_eSampleProducerActiveMode));

            // If we are bound to a recording-in-progress, it is possible that
            // the Load() call was issued before the writer has written the
            // first sample to the new recording. If so, we will not have gotten
            // a notification about the recording. We can explicitly ask for
            // the pause buffer content and, during this window, the Writer
            // will add an interim entry for the recording to tell us about it.
            // Hence the code below:

            CSmartRefPtr<CPauseBufferData> pcPauseBufferData;
            pcPauseBufferData.Attach(m_piSampleProducer->GetPauseBuffer());

            CSmartRefPtr<CPauseBufferData> pDataWithOrphans;
            LONGLONG hyBoundRecordingEndPos = -1LL;
            // Note:  I was under time-pressure to fix a 
            //        pruning down a recording-in-progress to just the description
            //        of the recording-in-progress when I added the call to MergeWithOrphans().
            //        I suspect that "true" may be the correct second argument, but "false"
            //        mirrors the old behavior for live-tv. I don't have the time now to
            //        think through true vs false here for live-tv. For a recording-in-progress,
            //        true and false have the same effect. So, pending further thought, I'm
            //        checking in false (for the old -- dunno
            //        which).
            pDataWithOrphans.Attach(MergeWithOrphans(hyBoundRecordingEndPos, pcPauseBufferData, false));

            ImportPauseBuffer(pDataWithOrphans, hyBoundRecordingEndPos);
        }
    }
    else if (m_piSampleProducer)
    {
        HRESULT hr = m_piSampleProducer->UnbindConsumer(this);
        m_piSampleProducer = NULL;
        if (FAILED(hr))
            throw CHResultError(hr);
    }
}

void CSampleConsumer::UpdateRecordingName(bool fIsLiveTv, LPCOLESTR pszRecordingName)
{
    DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer(%p)::UpdateRecordingName()\n"), this));
    OleMemHolder<LPOLESTR> trueRecordingName;

    if (fIsLiveTv)
    {
        m_esbptp = STRMBUF_STAY_WITH_SINK;
    }
    else
    {
        m_esbptp = STRMBUF_STAY_WITH_RECORDING;

        if (m_pippmgr)
        {
            IFileSourceFilter *piFileSourceFilter = NULL;
            LPVOID pvFileSourceFilter = NULL;
            m_pippmgr->NonDelegatingQueryInterface(
                            IID_IFileSourceFilter, pvFileSourceFilter);
            piFileSourceFilter = static_cast<IFileSourceFilter*>(pvFileSourceFilter);
            if (piFileSourceFilter)
            {
                CComPtr<IFileSourceFilter> ccomPtrIFileSourceFilter = 0;
                ccomPtrIFileSourceFilter.Attach(piFileSourceFilter);

                AM_MEDIA_TYPE mt;
                if (SUCCEEDED(piFileSourceFilter->GetCurFile(&trueRecordingName.m_pMem, &mt)))
                {
                    FreeMediaType(mt);
                    pszRecordingName = trueRecordingName.m_pMem;
                }
            }
        }
    }

    if (pszRecordingName &&
        (OLESTRCMP(pszRecordingName, m_pszFileName.c_str()) != 0))
    {
        m_pszFileName = pszRecordingName;
    }
    RebindToProducer();
}

void CSampleConsumer::UpdateProducerState(PRODUCER_STATE eProducerState)
{
    switch (eProducerState)
    {
    case PRODUCER_IS_STOPPED:
        m_eFilterStateProducer = State_Stopped;
        if (IsSourceProducer() && m_piDecoderDriver)
            m_piDecoderDriver->ImplementDisabledInputs(false);
        break;
    case PRODUCER_IS_PAUSED:
        m_eFilterStateProducer = State_Paused;
        if (IsSourceProducer() && m_piDecoderDriver)
            m_piDecoderDriver->ImplementDisabledInputs(false);
#if defined(SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT)
        m_rtProducerToConsumerClockOffset = 0;
        m_piGraphClock = NULL;
#endif // SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT
        break;
    case PRODUCER_IS_RUNNING:
        m_eFilterStateProducer = State_Running;
#if defined(SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT)
        if (Running() && !m_piGraphClock)
        {
            CDVRSourceFilter &rcDVRSourceFilter = m_pippmgr->GetSourceFilter();
            if (SUCCEEDED(rcDVRSourceFilter.GetSyncSource(&m_piGraphClock)) && m_piGraphClock)
            {
                REFERENCE_TIME rtCurrentTime;

                m_piGraphClock->Release();
                if (SUCCEEDED(m_piGraphClock->GetTime(&rtCurrentTime)))
                    m_rtProducerToConsumerClockOffset =
                        rtCurrentTime - m_piSampleProducer->GetProducerTime();
                else
                    m_piGraphClock = NULL;
            }
            else
                m_piGraphClock = NULL;
        }
#endif // SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT
        break;
    default:
        ASSERT(0);  // this is a programming 
        break;
    }
}

bool CSampleConsumer::CanSupportRate(double dRate)
{
    // TODO:  determine if we're wedged at the beginning of a temporary recording
    //                 or at the 'live' side of a recording-in-progress
    //                 or at the end of a completed recording that we are bound to
    IDecoderDriver *piDecoderDriver = GetDecoderDriver();
    if (piDecoderDriver && !piDecoderDriver->IsRateSupported(dRate))
        return false;
    return true;
} // CSampleConsumer::CanSupportRate

ROUTE CSampleConsumer::DetermineSampleRoute(
    IMediaSample &riSample,
    SAMPLE_CONSUMER_INTERNAL_EVENT_TYPE eConsumerSampleEvent,
    CDVROutputPin &cDVROutputPin)
{
#if defined(SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT)

    if (Running() && m_piSampleProducer &&
        (m_eFilterStateProducer == State_Running) && m_piGraphClock
        && (riSample.IsSyncPoint() == S_OK))
    {
        REFERENCE_TIME rtCurrentTime;
        REFERENCE_TIME rtSinkTime = m_piSampleProducer->GetProducerTime();
        REFERENCE_TIME rtAdjustedSinkTime = rtSinkTime + m_rtProducerToConsumerClockOffset;

        if (SUCCEEDED(m_piGraphClock->GetTime(&rtCurrentTime)))
        {
            LONGLONG hyClockDrift = (rtCurrentTime - rtAdjustedSinkTime);
#ifdef SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT
            static REFERENCE_TIME rtLastCurrentTime = 0;
            if ((rtLastCurrentTime != 0) && (rtLastCurrentTime == rtCurrentTime))
            {
                const wchar_t *pwszMsg = L"\n\n####\nCSampleConsumer:  clock froze\n###\n\n";

#ifdef WIN_CE_KERNEL_TRACKING_EVENTS
            CELOGDATA(TRUE,
                CELID_RAW_WCHAR,
                (PVOID)pwszMsg,
                (1 + wcslen(pwszMsg)) * sizeof(wchar_t),
                1,
                CELZONE_MISC);

#endif // WIN_CE_KERNEL_TRACKING_EVENTS
                OutputDebugStringW(pwszMsg);
            }

            rtLastCurrentTime = rtCurrentTime;

            if ((hyClockDrift < - 10000LL) ||
                (hyClockDrift > 10000LL))
            {
                DbgLog((LOG_CLOCK, 1, _T("CSampleConsumer:  clock drift of %I64d milliseconds [source clock (%I64d) - sink clock (%I64d) - initial delta (%I64d)]\n"),
                    hyClockDrift / (REFERENCE_TIME) 10000,
                    rtCurrentTime,
                    rtSinkTime,
                    m_rtProducerToConsumerClockOffset));
            }
#endif // SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT

        }
    }

#endif // SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT

    UCHAR bPrimaryPinIndex = m_pcPinMappings->GetPrimaryPin();
    IPin *piPin = static_cast<IPin*>(&cDVROutputPin);
    UCHAR bPinIndex = m_pcPinMappings->FindPinPos(piPin);

    if (!((eConsumerSampleEvent == SC_INTERNAL_EVENT_READER_SAMPLE) && DeliveringReaderSamples()) &&
        !((eConsumerSampleEvent == SC_INTERNAL_EVENT_PRODUCER_SAMPLE) && DeliveringProducerSamples()))
        return HANDLED_STOP;

    LONGLONG hyMediaStart = -1;
    LONGLONG hyMediaEnd = -1;
    if (FAILED(riSample.GetMediaTime(&hyMediaStart, &hyMediaEnd)) ||
        (hyMediaStart < 0))
    {
        ASSERT(0);
        throw CHResultError(E_FAIL);
    }

recheckMode:
    switch (m_eSCPositionMode)
    {
    case SC_POSITION_UNKNOWN:
    case SC_POSITION_AWAITING_FIRST_SAMPLE:
        SetPositionMode(SC_POSITION_ALIGNING);
        // continue into the next case:

    case SC_POSITION_ALIGNING:
        if ((bPinIndex != bPrimaryPinIndex) ||
            (riSample.IsSyncPoint() != S_OK))
            return HANDLED_STOP;

        SetPositionMode(SC_POSITION_NORMAL);
        break;

    case SC_POSITION_NORMAL:
        break;

    case SC_POSITION_SEEKING_KEY:
    case SC_POSITION_SEEKING_ANY:
        if (bPinIndex != bPrimaryPinIndex)
            return HANDLED_STOP;

        if (((GetTrueRate() < 0.0) &&
            (hyMediaStart > m_hyTargetPosition)) ||
            ((GetTrueRate() > 0.0) &&
            (hyMediaStart < m_hyTargetPosition)))
            return HANDLED_STOP;
        SetPositionMode((m_eSCPositionMode == SC_POSITION_SEEKING_KEY) ? SC_POSITION_NORMAL : SC_POSITION_ALIGNING);
        goto recheckMode;

    case SC_POSITION_AT_STOP_POSITION:
    case SC_POSITION_PENDING_SET_POSITION:
    case SC_POSITION_PENDING_RUN:
    case SC_POSITION_PENDING_NORMAL_RATE:
        return HANDLED_STOP;
    }

    StateManager<LONGLONG> tStateManagerCurrentPosition(&m_hyCurrentPosition);

    if (bPinIndex == bPrimaryPinIndex)
    {
        m_hyCurrentPosition = hyMediaStart;
        CHECK_POSITION(m_hyCurrentPosition);
    }

    ROUTE eRoute = UpdateMediaSource(eConsumerSampleEvent);
    if ((eRoute == HANDLED_CONTINUE) || (eRoute == UNHANDLED_CONTINUE))
    {
        if (m_hyPostFlushPosition == POST_FLUSH_POSITION_PENDING)
        {
            m_hyPostFlushPosition = m_hyCurrentPosition;
            DbgLog((LOG_SOURCE_STATE, 4,
                _T("CSampleConsumer::DetermineSampleRoute():  updated post-flush position limit to be %I64d\n"),
                m_hyCurrentPosition));
        }

        if (m_fSampleMightBeDiscontiguous)
        {
            DbgLog((LOG_SOURCE_DISPATCH, 4,
                _T("CSampleConsumer::DetermineSampleRoute():  marking sample @ %I64d as discontiguous\n"),
                m_hyCurrentPosition));
            riSample.SetDiscontinuity(TRUE);
        }
        else if (riSample.IsDiscontinuity() == S_OK)
        {
#if defined(DEBUG) || defined(_DEBUG)
            DbgLog((LOG_SOURCE_DISPATCH, 4,
            _T("CSampleConsumer::DetermineSampleRoute():  received discontiguous sample @ %I64d\n"),
            m_hyCurrentPosition));
#endif
        }
    }
    if (eRoute != UNHANDLED_STOP)
    {
        tStateManagerCurrentPosition.ConfirmState(m_hyCurrentPosition);
    }
    m_fSampleMightBeDiscontiguous = false;
    return eRoute;
} // CSampleConsumer::DetermineSampleRoute

void CSampleConsumer::MapPins(void)
{
    if (!m_pippmgr)
        return;

    if (!m_pcPinMappings)
    {
        CDVRSourceFilter &rcDVRSourceFilter = m_pippmgr->GetSourceFilter();
        m_pcPinMappings = new CPinMappings(rcDVRSourceFilter, NULL,
            (rcDVRSourceFilter.GetPinCount() == 0) ? NULL : &m_pippmgr->GetPrimaryOutput(), true);
        if (m_pcPinMappings && (m_pcPinMappings->GetConnectedPinCount() == 0))
        {
            // It must be too early to correctly establish the pin mappings.
            delete m_pcPinMappings;
            m_pcPinMappings = NULL;
            return;
        }
    }

    if (m_piSampleProducer && !m_bOutputPins)
    {
        UCHAR bNumPins = m_piSampleProducer->QueryNumInputPins();
        UCHAR bPinIndex;

        m_vcpssOutputPinMapping.clear();
        for (bPinIndex = 0; bPinIndex < bNumPins; bPinIndex++)
        {
            CMediaType cMediaType = m_piSampleProducer->QueryInputPinMediaType(bPinIndex);
            CProducerSampleState cProducerSampleState;
            try {
            cProducerSampleState.m_pcDVROutputPin =
                static_cast<CDVROutputPin*>(
                    &m_pcPinMappings->GetPin(m_pcPinMappings->FindPinPos(cMediaType))
                    );
            } catch (const std::exception &)
            {
                // This pin has no mapping:

                cProducerSampleState.m_pcDVROutputPin = NULL;
            }
            m_vcpssOutputPinMapping.push_back(cProducerSampleState);
        }
        m_bOutputPins = bNumPins;
    }
} // CSampleConsumer::MapPins

CSampleConsumer::CProducerSampleState &CSampleConsumer::MapToSourcePin(UCHAR bProducerPinIndex)
{
    MapPins();

    ASSERT(bProducerPinIndex < m_bOutputPins);
    if (bProducerPinIndex >= m_bOutputPins)
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::MapToSourcePin(%u) throwing E_INVALIDARG\n"), this, (unsigned) bProducerPinIndex));
        throw CHResultError(E_INVALIDARG, "Mismatched producer/consumer pins");
    }

    return m_vcpssOutputPinMapping[bProducerPinIndex];
} // CSampleConsumer::MapToSourcePin

UCHAR CSampleConsumer::MapToSinkPin(UCHAR bSourcePinIndex)
{
    IPin *piPin = &m_pcPinMappings->GetPin(bSourcePinIndex);

    UCHAR bPinIndex;
    for (bPinIndex = 0; bPinIndex < m_bOutputPins; bPinIndex++)
    {
        IPin *piPin2 = static_cast<IPin*>(m_vcpssOutputPinMapping[bPinIndex].m_pcDVROutputPin);
        if (piPin2 && (piPin == piPin2))
            return bPinIndex;
    }
    DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::MapToSinkPin(%u) throwing E_INVALIDARG\n"), this, (unsigned) bSourcePinIndex));
    throw CHResultError(E_INVALIDARG, "Unknown consumer/producer pin mapping");
} // CSampleConsumer::MapToSinkPin

void CSampleConsumer::SetCurrentPosition(SampleProducer::CPauseBufferSegment *pcDesiredRecording,
                                         LONGLONG hyDesiredPosition,
                                         bool fSeekToKeyFrame,
                                         bool fNoFlush, bool fNotifyDecoder, bool fOnAppThread)
{
    StateManager<LONGLONG> tStateManagerPosition(&m_hyCurrentPosition);
    CSmartRefPtr<SampleProducer::CPauseBufferSegment> pSelectedRecording = pcDesiredRecording;

    DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetCurrentPosition(%p, %I64d, %u [key frame], %u [no flush], %u [tell decoder]\n"),
        pcDesiredRecording, hyDesiredPosition, (unsigned) fSeekToKeyFrame,
        (unsigned) fNoFlush, (unsigned) fNotifyDecoder));

    ++m_uPositionNotifyId;

    if (m_pcPauseBufferHistory && !pcDesiredRecording)
        pSelectedRecording.Attach(m_pcPauseBufferHistory->FindSegmentByTime(hyDesiredPosition,
                        SampleProducer::CPauseBufferHistory::PREFER_LATER_TIME, NULL));

    if (!pSelectedRecording)
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer::SetCurrentPosition():  throwing exception because no recording has been selected\n")));
        throw CHResultError(E_INVALIDARG);
    }
    if ((m_esbptp == STRMBUF_STAY_WITH_RECORDING) &&
        m_pcPauseBufferSegmentBound &&
        (pSelectedRecording != m_pcPauseBufferSegmentBound))
    {
        // Oops -- this seek lies outside our bound recording.  Force the seek
        // to be within the bound recording:
        pSelectedRecording = m_pcPauseBufferSegmentBound;
    }

    // Round off the desired position to a value within the recording:

    LONGLONG hyRecStart = pSelectedRecording->CurrentStartTime();
    LONGLONG hyRecEnd = pSelectedRecording->CurrentEndTime();
    if (hyRecStart < 0)
    {
        DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::SetCurrentPosition():  recording start not known\n")));
        hyDesiredPosition = CURRENT_POSITION_NULL_VALUE;
    }
    else if (hyDesiredPosition < hyRecStart)
    {
        DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::SetCurrentPosition():  rounding up to recording start position %I64d\n"),
            hyRecStart));
        hyDesiredPosition = hyRecStart;
    }
    else if (hyDesiredPosition > hyRecEnd)
    {
        DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::SetCurrentPosition():  rounding down to recording end position %I64d\n"),
            hyRecEnd));
        hyDesiredPosition = hyRecEnd;
    }
    if (hyDesiredPosition == CURRENT_POSITION_NULL_VALUE)
    {
        // We don't know where we want to go. Give up
        DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::SetCurrentPosition():  dunno where to seek to, giving up\n")));
        return;
    }
    bool fCanUseProducer = SeekInProducerRange(hyDesiredPosition);
    bool fAtPositionNow = (((pSelectedRecording == m_pcPauseBufferSegment) ||
                             !m_pcPauseBufferSegment) &&
                           ((m_hyCurrentPosition == hyDesiredPosition) ||
                            (m_hyCurrentPosition < 0) ||
                            (m_eSCPositionMode == SC_POSITION_UNKNOWN) ||
                            (m_eSCPositionMode == SC_POSITION_AWAITING_FIRST_SAMPLE)));
    bool fJumpsTimeHole = false;
    if (m_pcPauseBufferSegment &&
        m_pcPauseBufferSegment->m_fIsOrphaned &&
        (m_pcPauseBufferSegment != pSelectedRecording))
    {
        // We are moving from an orphaned recording (i.e., a permanent recording
        // that is not part of the pause buffer).
        fJumpsTimeHole = true;
        pcDesiredRecording = pSelectedRecording;
        LeaveOrphanedRecording(pcDesiredRecording);
        pSelectedRecording = pcDesiredRecording;
        if (pSelectedRecording && !pSelectedRecording->m_fIsOrphaned)
            hyDesiredPosition = pSelectedRecording->CurrentEndTime();
    }

    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::SetCurrentPosition():  can use producer=%u, at position=%u, jumps hole=%u\n"),
        (unsigned) fCanUseProducer, (unsigned) fAtPositionNow, (unsigned) fJumpsTimeHole));

    m_hyTargetPosition = hyDesiredPosition;
    m_pcPauseBufferSegment = pSelectedRecording;
#ifdef UNICODE
    DbgLog((LOG_PAUSE_BUFFER, 3,
        _T("CONSUMER:  updated current recording to %s\n"),
        m_pcPauseBufferSegment ? m_pcPauseBufferSegment->GetRecordingName().c_str() : L"[none]"));
#else
    DbgLog((LOG_PAUSE_BUFFER, 3,
        _T("CONSUMER:  updated current recording to %S\n"),
        m_pcPauseBufferSegment ? m_pcPauseBufferSegment->GetRecordingName().c_str() : L"[none]"));
#endif

    if (fAtPositionNow)
        SetPositionMode((m_hyCurrentPosition < 0) ? SC_POSITION_AWAITING_FIRST_SAMPLE : SC_POSITION_NORMAL);
    else if (Stopped())
    {
        SetPositionMode(SC_POSITION_PENDING_RUN);
        fNoFlush = true;
    }
    else if (!fNoFlush)
        SetPositionMode(SC_POSITION_PENDING_SET_POSITION);
    else
        SetPositionMode(fSeekToKeyFrame ? SC_POSITION_SEEKING_KEY : SC_POSITION_SEEKING_ANY);
    EnsureRecordingIsSelected();  // this will update how we are viewing the recording

    DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetCurrentPosition():  position mode %d\n"), m_eSCPositionMode));

    if (m_eSCPositionMode == SC_POSITION_PENDING_SET_POSITION)
    {
        DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetCurrentPosition(): deferring seek until after flush, stopping all inputs\n")));
        DisableMediaSource(SC_INTERNAL_EVENT_POSITION_FLUSH);
    }
    else if (m_eSCPositionMode != SC_POSITION_PENDING_RUN)
    {
        DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetCurrentPosition():  updating now\n")));
        m_hyCurrentPosition = hyDesiredPosition;

        CHECK_POSITION(m_hyCurrentPosition);
        if (Stopped() ||
            (m_piDecoderDriver && m_piDecoderDriver->IsAtEOS()) ||
            (!fCanUseProducer && !m_piReader))
        {
            DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetCurrentPosition():  stopped or end-of-stream or no usable sample source\n")));

            m_eSampleSource = SC_SAMPLE_SOURCE_NONE;
            m_eSampleProducerMode = PRODUCER_SUPPRESS_SAMPLES;
            {
                CAutoLock cAutoLock(&m_cCritSecThread);

                if (m_pcConsumerStreamingThread)
                    m_pcConsumerStreamingThread->SendProducerModeEvent();
            }
            m_piReader->SetIsStreaming(false);
            if (!fCanUseProducer && m_piReader &&
                (hyDesiredPosition >= 0) &&
                (m_piReader->GetCurrentPosition() != hyDesiredPosition))
            {
                DWORD dwStatus = m_piReader->SetCurrentPosition(hyDesiredPosition, fSeekToKeyFrame);
                if (dwStatus != ERROR_SUCCESS)
                    throw CHResultError(E_FAIL);
            }
        }
        else if (fCanUseProducer)
        {
            DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetCurrentPosition():  will use producer\n")));

            if (m_piReader)
                m_piReader->SetIsStreaming(false);
            if (fAtPositionNow)
                m_eSampleProducerMode = PRODUCER_SEND_SAMPLES_LIVE;
            else
            {
                m_eSampleProducerMode = (GetTrueRate() < 0.0) ?
                    PRODUCER_SEND_SAMPLES_BACKWARD : PRODUCER_SEND_SAMPLES_FORWARD;
                m_hyDesiredProducerPos = m_hyCurrentPosition;
            }
            {
                CAutoLock cAutoLock(&m_cCritSecThread);

                if (m_pcConsumerStreamingThread)
                    m_pcConsumerStreamingThread->SendProducerModeEvent();
            }
            m_eSampleSource = fAtPositionNow ? SC_SAMPLE_SOURCE_PRODUCER : SC_SAMPLE_SOURCE_CONVERTING_TO_PRODUCER;
        }
        else // must be that m_piReader != NULL
        {
            DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetCurrentPosition():  will use reader\n")));

            m_eSampleProducerMode = PRODUCER_SUPPRESS_SAMPLES;
            {
                CAutoLock cAutoLock(&m_cCritSecThread);

                if (m_pcConsumerStreamingThread)
                    m_pcConsumerStreamingThread->SendProducerModeEvent();
            }
            if ((hyDesiredPosition >= 0) &&
                (m_piReader->GetCurrentPosition() != hyDesiredPosition))
            {
                DWORD dwStatus = m_piReader->SetCurrentPosition(hyDesiredPosition, fSeekToKeyFrame);
                if (dwStatus != ERROR_SUCCESS)
                    throw CHResultError(E_FAIL);
            }
            m_eSampleSource = SC_SAMPLE_SOURCE_READER;
            if (!m_fFlushing)
                m_piReader->SetIsStreaming(true);
        }
    }

    if (!fAtPositionNow)
    {
        DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetCurrentPosition():  setting flush position bound to %I64d\n"), hyDesiredPosition ));

        m_hyPostFlushPosition = hyDesiredPosition;
        m_hyPostTunePosition = POST_FLUSH_POSITION_INVALID;
        m_fSampleMightBeDiscontiguous = true;
        if (fNotifyDecoder)
        {
            if (fOnAppThread && !fNoFlush)
            {
                CAutoUnlock cAutoUnlock(m_cCritSecState);

                SetGraphPosition(hyDesiredPosition, fNoFlush, fJumpsTimeHole, fSeekToKeyFrame, fOnAppThread);
            }
            else
            {
                SetGraphPosition(hyDesiredPosition, fNoFlush, fJumpsTimeHole, fSeekToKeyFrame, fOnAppThread);
            }
        }
    }
    tStateManagerPosition.ConfirmState(m_hyCurrentPosition);
} // CSampleConsumer::SetCurrentPosition

HRESULT CSampleConsumer::GetConsumerPositions(LONGLONG &rhyMinPos,
                                            LONGLONG &rhyCurPos,
                                            LONGLONG &rhyMaxPos)
{
    rhyCurPos = ((m_eSCPositionMode == SC_POSITION_PENDING_RUN) ||
                 (m_eSCPositionMode == SC_POSITION_PENDING_SET_POSITION)) ?
                    m_hyTargetPosition : m_hyCurrentPosition;

    rhyMinPos = -1;
    rhyMaxPos = -1;

    if (!m_piReader && !(m_piSampleProducer && m_pcPinMappings))
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::GetConsumerPositions() returning E_FAIL due to no reader and no producer w/known pins\n"), this));
        return E_FAIL;
    }

    LONGLONG hyProducerMin = -1;
    LONGLONG hyProducerMax = -1;
    if (m_pcPauseBufferSegmentBound)
    {
        rhyMinPos = m_pcPauseBufferSegmentBound->CurrentStartTime();
        if (m_piSampleProducer && m_pcPauseBufferSegmentBound->m_fInProgress)
        {
            m_piSampleProducer->GetPositionRange(hyProducerMin, hyProducerMax);
            rhyMaxPos = hyProducerMax;
        }
        else
        {
            rhyMaxPos = m_pcPauseBufferSegmentBound->m_hyEndTime;
            if (m_piSampleProducer && (rhyCurPos > rhyMaxPos))
            {
                // There is a window during which completion of the recording
                // is racing playback. Safeguard against outward confusion by
                // correcting the reported current position:
                rhyCurPos = rhyMaxPos;
            }
        }
    }
    else
    {
        if (m_piReader)
        {
            rhyMinPos = m_piReader->GetMinSeekPosition();
            rhyMaxPos = m_piReader->GetMaxSeekPosition();
        }
        if (m_piSampleProducer && m_pcPinMappings)
        {
//          LONGLONG hyProducerMin, hyProducerMax;
            m_piSampleProducer->GetPositionRange(hyProducerMin, hyProducerMax);
            rhyMaxPos = hyProducerMax;
            if (!m_piReader)
                rhyMinPos = hyProducerMin;
        }
    }

    ASSERT((rhyMinPos <= rhyCurPos) ||
           (rhyCurPos == CURRENT_POSITION_NULL_VALUE) ||
           (m_eSCPositionMode == SC_POSITION_UNKNOWN) ||
           (!m_pcPauseBufferSegmentBound && (m_eSCPositionMode == SC_POSITION_PENDING_NORMAL_RATE)) ||
           (m_eSCPositionMode == SC_POSITION_AWAITING_FIRST_SAMPLE));
    ASSERT((rhyCurPos <= rhyMaxPos) ||
           (rhyCurPos == CURRENT_POSITION_NULL_VALUE) ||
           (m_eSCPositionMode == SC_POSITION_UNKNOWN) ||
           (m_eSCPositionMode == SC_POSITION_AWAITING_FIRST_SAMPLE));
    return S_OK;
} // CSampleConsumer::GetConsumerPositions

bool CSampleConsumer::SeekInProducerRange(LONGLONG &rhyCurPosition)
{
    if (!m_piSampleProducer || !m_pcPinMappings)
        return false;

    try {
        LONGLONG hyProducerMin, hyProducerMax;
        m_piSampleProducer->GetPositionRange(hyProducerMin, hyProducerMax);
        if ((hyProducerMin >= 0) && (rhyCurPosition >= hyProducerMin))
        {
            if (rhyCurPosition > hyProducerMax)
                rhyCurPosition = hyProducerMax + 1;
            return true;
        }
    }
    catch (const std::exception& rcException)
    {
        UNUSED (rcException);  // suppress release build warning
#ifdef UNICODE
        DbgLog((LOG_ERROR, 2,
            _T("CSampleConsumer(%p)::SeekInProducerRange() ignoring exception %S\n"),
            this, rcException.what()));
#else
        DbgLog((LOG_ERROR, 2,
            _T("CSampleConsumer(%p)::SeekInProducerRange() ignoring exception %s\n"),
            this, rcException.what()));
#endif
    }
    return false;
} // CSampleConsumer::SeekInProducerRange

void CSampleConsumer::SetGraphRate(double dblRate, bool fDeferUntilRendered,
                                   LONGLONG hyTargetPosition,
                                   long lEventId, long lParam1, long lParam2)
{
    DbgLog((LOG_SOURCE_STATE, 4, _T("CSampleConsumer::SetGraphRate(%lf,%s,%I64d,0x%x,%d,%d)\n"),
        dblRate, fDeferUntilRendered ? TEXT("defer-until-rendered") : TEXT("immediately"),
        hyTargetPosition, lEventId, lParam1, lParam2));

    if (m_eSCPositionMode == SC_POSITION_PENDING_NORMAL_RATE)
        return;

    IDecoderDriver *piDecoderDriver = GetDecoderDriver();
    if (fDeferUntilRendered)
    {
        if (!piDecoderDriver)
            fDeferUntilRendered = false;
        else if (!piDecoderDriver->IsFlushNeededForRateChange())
            fDeferUntilRendered = false;
        else if (dblRate == m_dblRate)
            fDeferUntilRendered = false;
        else if ((m_hyCurrentPosition == CURRENT_POSITION_NULL_VALUE) ||
                 (m_eSCPositionMode != SC_POSITION_NORMAL))
            fDeferUntilRendered = false;
    }
    if (fDeferUntilRendered && m_fInRateChange)
    {
        // Ugh -- we are in the middle of a rate change and were forced to release
        //        our state lock in order to do a flush. The streaming codepaths
        //        don't hold the filter lock (and must not do so) so it is possible
        //        for streaming to discover the need for a rate change. We bail
        //        after the deferral so it is likely to be safe to release the
        //        the lock and sleep until either it is safe to proceed or it looks
        //        like something is holding us up.
    
        unsigned uNumSleeps = 0;

        do
        {
            CAutoUnlock cAutoUnlock(m_cCritSecState);

            ++uNumSleeps;
            Sleep(25);
        }
        while (m_fInRateChange && (uNumSleeps < 20));
    }

    double dblOldRate = m_dblRate;
    m_dblRate = dblRate;
    if (fDeferUntilRendered)
        m_dblPreDeferNormalRate = dblOldRate;
    else
    {
        m_dblPreDeferNormalRate = dblRate;
        if (-1LL != hyTargetPosition)
        {
            SetCurrentPosition(NULL, hyTargetPosition, true, false, true, true);
        }
        else if (((dblOldRate > 0.0) && (dblRate < 0.0)) ||
                 ((dblOldRate < 0.0) && (dblRate > 0.0)))
        {
            if ((m_eSCPositionMode == SC_POSITION_NORMAL) && (m_hyCurrentPosition != CURRENT_POSITION_NULL_VALUE))
            {
                // It is ugly to switch directions where you start painting
                // partial screens on top of a key frame generated while
                // going in the other direction.
                SetPositionMode(SC_POSITION_SEEKING_KEY);
                m_hyTargetPosition = m_hyCurrentPosition;
            }

            // We've changed directions or are going to do a seek so our
            // old bound on position is no longer valid:

            DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::SetGraphRate():  invalidating flush position bound\n") ));
            m_hyPostFlushPosition = POST_FLUSH_POSITION_INVALID;
            m_hyPostTunePosition = POST_FLUSH_POSITION_INVALID;
        }
    }

    if (piDecoderDriver)
    {
        m_fConsumerSetGraphRate = true;
        if (fDeferUntilRendered)
        {
            CConsumerNotifyOnPosition *pcConsumerNotifyOnPosition = NULL;

            try {
                pcConsumerNotifyOnPosition = new CConsumerNotifyOnPosition(this, piDecoderDriver->GetPipelineComponent(),
                            m_hyCurrentPosition, dblOldRate, ++m_uPositionNotifyId, lEventId, lParam1, lParam2);
                m_pippmgr->GetRouter(this, false, true).DispatchExtension(*pcConsumerNotifyOnPosition);

                // Don't use SetPositionMode() here -- we're doing something special.
                m_eSCPositionMode = SC_POSITION_PENDING_NORMAL_RATE;
                m_hyTargetPosition = m_hyCurrentPosition;
                DisableMediaSource(SC_INTERNAL_EVENT_REQUESTED_SET_RATE_NORMAL);
            }
            catch (const std::exception &)
            {
                fDeferUntilRendered = false;
            }
            if (pcConsumerNotifyOnPosition)
                pcConsumerNotifyOnPosition->Release();
        }
        if (!fDeferUntilRendered)
        {
            piDecoderDriver->SetNewRate(dblRate);
            piDecoderDriver->ImplementRun(false);
        }
    }
    else
        m_fDroppingToNormalPlayAtBeginning = false;
} // CSampleConsumer::SetGraphRate

void CSampleConsumer::SetGraphPosition(LONGLONG hyPosition, bool fNoFlush,
                                       bool fSkipTimeHole, bool fSeekKeyFrame, bool fOnAppThread)
{
    ++m_uPositionNotifyId;
    m_fDroppingToNormalPlayAtBeginning = false;
    IDecoderDriver *piDecoderDriver = GetDecoderDriver();
    if (piDecoderDriver)
    {
        piDecoderDriver->ImplementNewPosition(this, hyPosition,
                fNoFlush, fSkipTimeHole, fSeekKeyFrame, fOnAppThread);
    }
    else if (!fNoFlush)
        SetPositionFlushComplete(hyPosition, fSeekKeyFrame);
} // CSampleConsumer::SetGraphPosition

void CSampleConsumer::NoteBadGraphState(HRESULT hr)
{
    DisableMediaSource(SC_INTERNAL_EVENT_GRAPH_ERROR_STOP);
    SetPositionMode(SC_POSITION_AT_STOP_POSITION);
    IDecoderDriver *piDecoderDriver = GetDecoderDriver();
    if (piDecoderDriver)
        piDecoderDriver->ImplementGraphConfused(hr);
} // CSampleConsumer::NoteBadGraphState

void CSampleConsumer::StopGraph(PLAYBACK_END_MODE ePlaybackEndMode)
{
    SetPositionMode(SC_POSITION_AT_STOP_POSITION);
    DisableMediaSource(SC_INTERNAL_EVENT_CONSUMER_GRAPH_STOP);

    m_hyPostFlushPosition = POST_FLUSH_POSITION_INVALID;
    m_hyPostTunePosition = POST_FLUSH_POSITION_INVALID;

    IDecoderDriver *piDecoderDriver = GetDecoderDriver();
    if (piDecoderDriver)
    {
        piDecoderDriver->ImplementEndPlayback(ePlaybackEndMode, this);

        CDecoderDriverSendNotification *pcDecoderDriverSendNotification = NULL;
        switch (ePlaybackEndMode)
        {
        case PLAYBACK_AT_STOP_POSITION:
        case PLAYBACK_AT_STOP_POSITION_NEW_SEGMENT:
        default:
            break;
        case PLAYBACK_AT_BEGINNING:
        case PLAYBACK_AT_END:
            pcDecoderDriverSendNotification = new CDecoderDriverSendNotification(
                m_piDecoderDriver->GetPipelineComponent(),
                DVRENGINE_EVENT_RECORDING_END_OF_STREAM,
                (ePlaybackEndMode == PLAYBACK_AT_BEGINNING) ? DVRENGINE_BEGINNING_OF_RECORDING : DVRENGINE_END_OF_RECORDING,
                m_dwLoadIncarnation,
                true);
            try {
                m_pippmgr->GetRouter(this, false, true).DispatchExtension(*pcDecoderDriverSendNotification);
            } catch (const std::exception &) {};
            break;
        }
        if (pcDecoderDriverSendNotification)
            pcDecoderDriverSendNotification->Release();
    }
} // CSampleConsumer::StopGraph

void CSampleConsumer::EnsureRecordingIsSelected()
{
    if (m_pcPauseBufferHistory && !m_pcPauseBufferSegment)
    {
        m_pcPauseBufferSegment.Attach(m_pcPauseBufferHistory->GetCurrentSegment());
#ifdef UNICODE
        DbgLog((LOG_PAUSE_BUFFER, 3,
            _T("CONSUMER:  updated current recording to %s\n"),
            m_pcPauseBufferSegment ? m_pcPauseBufferSegment->GetRecordingName().c_str() : L"[none]"));
#else
        DbgLog((LOG_PAUSE_BUFFER, 3,
            _T("CONSUMER:  updated current recording to %S\n"),
            m_pcPauseBufferSegment ? m_pcPauseBufferSegment->GetRecordingName().c_str() : L"[none]"));
#endif
    }
    else if (m_pcPauseBufferHistory)
    {
        // It could be that the recording we are viewing has changed from
        // temporary to permanent. This is implemented by deleting the
        // recording for the recording as a temporary and creating a new
        // one for it as permanent. To handle cases in which the old
        // recording is no longer in the history, we need to re-establish
        // our current segment:

        bool fFoundCurSegment = false;
        SampleProducer::CPauseBufferSegment *pCurSeg = m_pcPauseBufferSegment;
        std::vector<SampleProducer::CPauseBufferSegment*>::iterator segIter;
        for (segIter = m_pcPauseBufferHistory->m_listpcPauseBufferSegments.begin();
             segIter != m_pcPauseBufferHistory->m_listpcPauseBufferSegments.end();
             ++segIter)
        {
            if (*segIter == pCurSeg)
            {
                fFoundCurSegment = true;
                break;
            }
        }
        if (!fFoundCurSegment)
        {
#ifdef UNICODE
            DbgLog((LOG_PAUSE_BUFFER, 3,
                _T("CONSUMER:  EnsureRecordingIsSelected() found cur recording %s @ %p (%s) vanished\n"),
                m_pcPauseBufferSegment->GetRecordingName().c_str(),
                pCurSeg,
                m_pcPauseBufferSegment->IsPermanent() ? _T("permanent") : _T("temporary")));
#else
            DbgLog((LOG_PAUSE_BUFFER, 3,
                _T("CONSUMER:  EnsureRecordingIsSelected() found cur recording %S @ %p (%s) vanished\n"),
                m_pcPauseBufferSegment->GetRecordingName().c_str(),
                pCurSeg,
                m_pcPauseBufferSegment->IsPermanent() ? _T("permanent") : _T("temporary")));
#endif

            LONGLONG hyRefPos = m_hyCurrentPosition;
            switch (m_eSCPositionMode)
            {
            case SC_POSITION_UNKNOWN:
                hyRefPos = m_pcPauseBufferSegment->CurrentStartTime();
                break;
    
            case SC_POSITION_PENDING_SET_POSITION:
            case SC_POSITION_PENDING_RUN:
                hyRefPos = m_hyTargetPosition;
                break;

            case SC_POSITION_AWAITING_FIRST_SAMPLE:
            default:
                if (hyRefPos < 0)
                    hyRefPos = m_pcPauseBufferSegment->CurrentStartTime();
                break;
            }
            if (!fFoundCurSegment)
            {
                m_pcPauseBufferSegment.Attach(m_pcPauseBufferHistory->FindSegmentByTime(hyRefPos,
                    (GetTrueRate() > 0) ?
                        SampleProducer::CPauseBufferHistory::PREFER_EARLIER_TIME :
                        SampleProducer::CPauseBufferHistory::PREFER_LATER_TIME));
            }

            if (m_pcPauseBufferSegment)
            {
                pCurSeg = m_pcPauseBufferSegment;
#ifdef UNICODE
                DbgLog((LOG_PAUSE_BUFFER, 3,
                    _T("CONSUMER:  EnsureRecordingIsSelected() new cur recording %s @ %p (%s)\n"),
                    m_pcPauseBufferSegment->GetRecordingName().c_str(),
                    pCurSeg,
                    m_pcPauseBufferSegment->IsPermanent() ? _T("permanent") : _T("temporary")));
#else
                DbgLog((LOG_PAUSE_BUFFER, 3,
                    _T("CONSUMER:  EnsureRecordingIsSelected() new cur recording %S @ %p (%s)\n"),
                    m_pcPauseBufferSegment->GetRecordingName().c_str(),
                    pCurSeg,
                    m_pcPauseBufferSegment->IsPermanent() ? _T("permanent") : _T("temporary")));
#endif
            }
            else
            {
                DbgLog((LOG_PAUSE_BUFFER, 3, _T("CONSUMER:  EnsureRecordingIsSelected() no new recording!\n")));
                ASSERT(0);
            }
        }
    }

    switch (m_esbptp)
    {
    case STRMBUF_STAY_WITH_RECORDING:
        if (!m_pcPauseBufferSegmentBound && m_pcPauseBufferSegment &&
            m_pcPauseBufferSegment->IsPermanent())
        {
            m_pcPauseBufferSegmentBound = m_pcPauseBufferSegment;
            if (m_piSampleProducer)
            {
                IReader *piReader = GetReader();
                if (piReader)
                {
                    LONGLONG hyTrueMinSeek = piReader->GetMinSeekPosition();
                    if (hyTrueMinSeek >= 0)
                        m_pcPauseBufferSegmentBound->SetMinimumSeekPosition(hyTrueMinSeek);
                }
            }
        }
        break;

    case STRMBUF_STAY_WITH_SINK:
        m_pcPauseBufferSegmentBound = NULL;
        break;
    }

    if (m_pcPauseBufferSegment)
    {
        if (m_pcPauseBufferSegment->IsPermanent())
            m_eSCRecordingState = m_pcPauseBufferSegment->m_fInProgress ?
                    SC_RECORDING_VIEWING_PERM_IN_PROGRESS : SC_RECORDING_VIEWING_PERM;
        else
            m_eSCRecordingState = m_pcPauseBufferSegment->m_fInProgress ?
                    SC_RECORDING_VIEWING_TEMP_IN_PROGRESS : SC_RECORDING_VIEWING_TEMP;
    }
    else
        m_eSCRecordingState = SC_RECORDING_VIEWING_NONE;
} // CSampleConsumer::EnsureRecordingIsSelected

void CSampleConsumer::ClearRecordingHistory()
{
    m_pcPauseBufferSegment = 0;
    m_pcPauseBufferSegmentBound = 0;
    m_pcPauseBufferHistory = 0;
    m_eSCRecordingState = SC_RECORDING_VIEWING_NONE;
    
    CAutoLock cAutoLock(&m_cCritSecPauseBuffer);
    m_listPauseBufferUpdates.clear();

#ifdef UNICODE
    DbgLog((LOG_PAUSE_BUFFER, 3,
        _T("CONSUMER:  cleared current recording to %s\n"),
        m_pcPauseBufferSegment ? m_pcPauseBufferSegment->GetRecordingName().c_str() : L"[none]"));
#else
    DbgLog((LOG_PAUSE_BUFFER, 3,
        _T("CONSUMER:  cleared current recording to %S\n"),
        m_pcPauseBufferSegment ? m_pcPauseBufferSegment->GetRecordingName().c_str() : L"[none]"));
#endif
} // CSampleConsumer::ClearRecordingHistory

void CSampleConsumer::ClearPlaybackPosition(SAMPLE_CONSUMER_INTERNAL_EVENT_TYPE eSCInternalEventType)
{
    DisableMediaSource(eSCInternalEventType);
    m_pszFileName = L"";
    if (m_piSampleProducer)
    {
        m_piSampleProducer->UnbindConsumer(this);
        m_piSampleProducer = 0;
    }
    StopStreamingThread();

    m_hyCurrentPosition = CURRENT_POSITION_NULL_VALUE;
    CHECK_POSITION(m_hyCurrentPosition);
    m_hyDesiredProducerPos = -1;
    m_hyStopPosition = STOP_POSITION_NULL_VALUE;
    m_eSCPositionMode = SC_POSITION_UNKNOWN;
    m_eSampleSource = SC_SAMPLE_SOURCE_NONE;
    m_eSCStopMode = SC_STOP_MODE_NONE;
    m_fResumingNormalPlay = false;
    m_fDroppingToNormalPlayAtBeginning = false;
    m_fPendingFailedRunEOS = false;
    m_fConsumerSetGraphRate = false;
    m_dwResumePlayTimeoutTicks = 0;
    ++m_uPositionNotifyId;
} // CSampleConsumer::ClearPlaybackPosition

void CSampleConsumer::StartStreamingThread()
{
    CAutoLock cAutoLock(&m_cCritSecThread);

    ASSERT(m_pcConsumerStreamingThread == NULL);

    // Heads up:  parental control needs to be able to run a sequence of
    // actions in a fashion that is effectively atomic with respect to
    // a/v reaching the renderers. Right now, parental control is doing
    // so by raising the priority of the thread in PC to a registry-supplied
    // value (PAL priority 1 = CE priority 249). If you change the code
    // here to raise the priority of the DVR engine playback graph
    // streaming threads, be sure to check that either these threads or
    // the DVR splitter output pin threads are still running at a priority
    // less urgent than parental control.

    m_pcConsumerStreamingThread = new CConsumerStreamingThread(*this);
    if (m_pcConsumerStreamingThread)
    {
        try {
            m_pcConsumerStreamingThread->StartThread();
        }
        catch (const std::exception& )
        {
            m_pcConsumerStreamingThread->Release();
            m_pcConsumerStreamingThread = NULL;
            throw;
        }
    }
} // CSampleConsumer::StartStreamingThread

void CSampleConsumer::StopStreamingThread()
{
    CConsumerStreamingThread *pcConsumerStreamingThread;

    {
        CAutoLock cAutoLock(&m_cCritSecThread);
        pcConsumerStreamingThread = m_pcConsumerStreamingThread;
        if (pcConsumerStreamingThread)
        {
            pcConsumerStreamingThread->AddRef();
            pcConsumerStreamingThread->SignalThreadToStop();
        }
    }
    if (pcConsumerStreamingThread)
    {
        pcConsumerStreamingThread->SleepUntilStopped();
        pcConsumerStreamingThread->Release();
    }
} // CSampleConsumer::StopStreamingThread

void CSampleConsumer::DoProcessProducerSample(UCHAR bPinIndex, LONGLONG hySampleId,
                                              ULONG uModeVersionCount,
                                            LONGLONG hyMediaStartPos, LONGLONG hyMediaEndPos)
{
    // We generally need the consumer lock, but we cannot afford to hold
    // it when calling into the producer or waiting for a buffer. So we
    // grab the lock then unlock at need.

    CAutoLock cAutoLock(&m_cCritSecState);

    if (!DeliveringProducerSamples())
        return;
    if (uModeVersionCount != m_uSampleProducerModeIncarnation)
    {
        DbgLog((LOG_SOURCE_DISPATCH, 4, _T("CSampleConsumer::DoProcessProducerSample() ... ignoring sample from old producer mode setting (%u sample vs %u now)\n"),
            uModeVersionCount, m_uSampleProducerModeIncarnation));
        return;
    }
    CProducerSampleState &rcState = MapToSourcePin(bPinIndex);
    if (rcState.m_fProducerInFlush || !rcState.m_pcDVROutputPin)
        return; // don't want this sample -- we're either flushing or there's no corresponding output pin

    rcState.m_llLatestMediaStart = hyMediaStartPos;
    rcState.m_llLatestMediaEnd = hyMediaEndPos;

    if (!DeliveringProducerSamples())
        return;
    if (rcState.m_fProducerInFlush)
        return;

    CBaseOutputPin *pcBaseOutputPin = static_cast<CBaseOutputPin*>(rcState.m_pcDVROutputPin);
    IMediaSample *piMediaSampleCopy = NULL;
    HRESULT hr;

    {
        CAutoUnlock cAutoUnlock(m_cCritSecState);
#ifdef DEBUG
        DWORD dwStartTicks = GetTickCount();
#endif // DEBUG
        hr = pcBaseOutputPin->GetDeliveryBuffer(&piMediaSampleCopy, NULL, NULL, 0);
#ifdef DEBUG
        DWORD dwEndTicks = GetTickCount();
        if (dwEndTicks - dwStartTicks > 1)
        {
            DbgLog((LOG_SOURCE_DISPATCH, 5, _T("CSampleConsumer():  GetDeliveryBuffer() took %ld ticks\n"),
                dwEndTicks - dwStartTicks));
        }
#endif
    }
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 2, _T("CSampleConsumer(%p)::DoProcessProducerSample() -- unable to get delivery buffer, error %d\n"),
            this, hr));
        return;
    }

    CComPtr<IMediaSample> cComPtrIMediaSample;
    cComPtrIMediaSample.Attach(piMediaSampleCopy);

    if (!DeliveringProducerSamples())
        return;
    if (rcState.m_fProducerInFlush)
        return;

    switch (m_eSampleProducerActiveMode)
    {
    case PRODUCER_SEND_SAMPLES_FORWARD:
        if ((m_hyCurrentPosition >= 0) && (rcState.m_llLatestMediaStart < m_hyCurrentPosition))
        {
            DbgLog((LOG_SOURCE_DISPATCH, 3, _T("CONSUMER:  Discarding unwanted forward sample (@%I64d, sample %I64d)\n"),
                    m_hyCurrentPosition, rcState.m_llLatestMediaStart));
            return;
        }
        break;

    case PRODUCER_SEND_SAMPLES_BACKWARD:
        if ((m_hyCurrentPosition >= 0) && (rcState.m_llLatestMediaStart > m_hyCurrentPosition))
        {
            DbgLog((LOG_SOURCE_DISPATCH, 3, _T("CONSUMER:  Discarding unwanted backward sample (@%I64d, sample %I64d)\n"),
                    m_hyCurrentPosition, rcState.m_llLatestMediaStart));
            return;
        }
        break;

    default:
        break;
    }

    // We cannot afford to hold the consumer lock while asking for the producer
    // lock here -- we will deadlock. So we use a try/catch(...) to guard against
    // a race condition in which m_piSampleProducer gets null'd out concurrently:

    try {
        CAutoUnlock cAutoUnlock(m_cCritSecState);

        if (!m_piSampleProducer ||
            !m_piSampleProducer->GetSample(bPinIndex, hySampleId, *piMediaSampleCopy))
        {
            DbgLog((LOG_SOURCE_DISPATCH, 3, _T("CSampleConsumer(%p)::DoProcessProducerSample() -- failed to obtain the sample data\n"), this));
            m_fSampleMightBeDiscontiguous = true;
            if (GetTrueRate() < 1.0)
            {
                UpdateMediaSource(SC_INTERNAL_EVENT_FAILED_TO_GET_PRODUCER_SAMPLE);
            }
            return;
        }
    } catch (...) {
        m_fSampleMightBeDiscontiguous = true;
        if (GetTrueRate() < 1.0)
        {
            UpdateMediaSource(SC_INTERNAL_EVENT_FAILED_TO_GET_PRODUCER_SAMPLE);
        }
        return;
    }


    CDVROutputPin &rcDVROutputPin = *rcState.m_pcDVROutputPin;
    ROUTE eRoute = DetermineSampleRoute(*piMediaSampleCopy, SC_INTERNAL_EVENT_PRODUCER_SAMPLE,
                        rcDVROutputPin);
    if ((eRoute == HANDLED_CONTINUE) || (eRoute == UNHANDLED_CONTINUE))
    {
        // Requeue for correct interleaving with other actions:
        ASSERT(m_pippmgr);
        m_cPipelineRouterForSamples.ProcessOutputSample(*piMediaSampleCopy, rcDVROutputPin);
    }
} // CSampleConsumer::DoProcessProducerSample

void CSampleConsumer::DoUpdateProducerMode()
{
    // We cannot safely hold the consumer lock while calling to the producer
    // here. So we update the producer with just a try/catch(...) to guard
    // against a concurrent action that sets m_piProducer to NULL. Then we
    // grab the lock to update the producer state.

    ISampleProducer *piSampleProducer = NULL;
    SAMPLE_PRODUCER_MODE desiredMode;
    ULONG uModeIncarnation;
    {
        CAutoLock cAutoLock(&m_cCritSecState);
        piSampleProducer = m_piSampleProducer;
        desiredMode = m_eSampleProducerMode;
        if (!piSampleProducer || (desiredMode == m_eSampleProducerActiveMode) ||
            (m_fFlushing && (desiredMode != PRODUCER_SUPPRESS_SAMPLES)))
            return;  // nothing to do
        DbgLog((LOG_SOURCE_STATE, 4, _T("CSampleConsumer::DoUpdateProducerMode():  setting mode %d\n"),
                desiredMode));
        m_eSampleProducerActiveMode = desiredMode;
        uModeIncarnation = ++m_uSampleProducerModeIncarnation;

        DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::DoUpdateProducerMode():  %d\n"), desiredMode));
    }
    try {
        piSampleProducer->UpdateMode(desiredMode, uModeIncarnation, m_hyDesiredProducerPos, this);
    } catch (...) {};
    DbgLog((LOG_SOURCE_STATE, 4, _T("CSampleConsumer::DoUpdateProducerMode():  done setting mode %d\n"), desiredMode));
}

void CSampleConsumer::DoNotifyProducerCacheDone(ULONG uModeVersionCount)
{
    DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer(%p)::DoNotifyProducerCacheDone(%u) in mode %d\n"),
        this, uModeVersionCount, m_eSampleProducerActiveMode));
    CAutoLock cAutoLock(&m_cCritSecState);

    if (!DeliveringProducerSamples())
    {
        DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer(%p)::DoNotifyProducerCacheDone() bailing early due to recent stop or unbind\n"), this));
        return;
    }
    if (uModeVersionCount != m_uSampleProducerModeIncarnation)
    {
        DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer(%p)::DoNotifyProducerCacheDone() ignoring stale notification (%u notification, %u consumer)\n"),
            this, uModeVersionCount, m_uSampleProducerModeIncarnation));
        return;
    }

    switch (m_eSampleProducerActiveMode)
    {
    case PRODUCER_SEND_SAMPLES_FORWARD:
        // The producer finished sending its samples going forward
        // and has automatically converted to live mode:
        m_eSampleSource = SC_SAMPLE_SOURCE_PRODUCER;
        m_eSampleProducerActiveMode = PRODUCER_SEND_SAMPLES_LIVE;
        if (m_eSampleProducerMode == PRODUCER_SEND_SAMPLES_FORWARD)
            m_eSampleProducerMode = PRODUCER_SEND_SAMPLES_LIVE;
        break;

    case PRODUCER_SEND_SAMPLES_BACKWARD:
        // The producer finished sending its samples going backward
        // and has automatically converted to suppress sample mode.
        // We'll need to pick a new source.
        m_eSampleSource = SC_SAMPLE_SOURCE_NONE;
        m_eSampleProducerActiveMode = PRODUCER_SUPPRESS_SAMPLES;
        if (m_eSampleProducerMode == PRODUCER_SEND_SAMPLES_BACKWARD)
            m_eSampleProducerMode = PRODUCER_SUPPRESS_SAMPLES;
        break;

    default:
        // Huh?  Our record of what we asked the producer to do
        // is out of sync with reality. Give up.
        DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::DoNotifyProducerCacheDone() -- unexpected state %d active, %d desired\n"),
                m_eSampleProducerActiveMode, m_eSampleProducerMode));
        ASSERT(0);
        return;
    }

    LONGLONG hyProducerMin, hyProducerMax;
    m_piSampleProducer->GetPositionRange(hyProducerMin, hyProducerMax);

    if (hyProducerMax < 0)
    {
        // the producer has never seen a sample:

        ASSERT(m_eSCPositionMode != SC_POSITION_PENDING_RUN);
        SetPositionMode(SC_POSITION_AWAITING_FIRST_SAMPLE);
        m_hyCurrentPosition = CURRENT_POSITION_NULL_VALUE;
        CHECK_POSITION(m_hyCurrentPosition);
    }
    else
    {
        // If we are playing backward, we might find hyProducerMax >= 0 because
        // samples were received but hyProducerMin < 0 because no samples have
        // been received since the latest flush. If we are playing backward
        // and the producer has flushed everything, treat our last known position
        // as the latest sample ever seen by the producer.

        m_hyCurrentPosition = ((GetTrueRate() > 0.0) || (hyProducerMin < 0)) ? hyProducerMax : hyProducerMin;
        CHECK_POSITION(m_hyCurrentPosition);

        ASSERT(m_eSCPositionMode != SC_POSITION_PENDING_RUN);
        SetPositionMode((m_eSCPositionMode == SC_POSITION_SEEKING_KEY) ?
            SC_POSITION_ALIGNING : SC_POSITION_NORMAL);
    }

    UpdateMediaSource(SC_INTERNAL_EVENT_PLAYED_TO_SOURCE);
} // CSampleConsumer::DoNotifyProducerCacheDone

bool CSampleConsumer::CanDeliverMediaSamples(bool fOkayIfEndOfStream)
{
    return  m_pippmgr &&
            (m_piReader || m_piSampleProducer) &&
            !Stopped() &&
            (!m_piDecoderDriver || !m_piDecoderDriver->IsAtEOS() || fOkayIfEndOfStream) &&
            !m_fFlushing &&
            (m_eSCPipelineState == SC_PIPELINE_NORMAL) &&
            (m_eSCPositionMode != SC_POSITION_AT_STOP_POSITION) &&
            (m_eSCPositionMode != SC_POSITION_PENDING_RUN) &&
            (m_eSCPositionMode != SC_POSITION_PENDING_SET_POSITION);
} // CSampleConsumer::CanDeliverMediaSamples

bool CSampleConsumer::DeliveringMediaSamples()
{
    return  m_pippmgr &&
            (m_piReader || m_piSampleProducer) &&
            !Stopped() &&
            !m_fFlushing &&
            (m_eSampleSource != SC_SAMPLE_SOURCE_NONE) &&
            (m_eSCPipelineState == SC_PIPELINE_NORMAL) &&
            (m_eSCPositionMode != SC_POSITION_AT_STOP_POSITION) &&
            (m_eSCPositionMode != SC_POSITION_PENDING_SET_POSITION) &&
            (m_eSCPositionMode != SC_POSITION_PENDING_RUN) &&
            (m_eSCRecordingState != SC_RECORDING_VIEWING_NONE);
} // CSampleConsumer::DeliveringMediaSamples

bool CSampleConsumer::DeliveringReaderSamples()
{
    return DeliveringMediaSamples() && m_piReader &&
        ( (m_eSampleSource == SC_SAMPLE_SOURCE_CONVERTING_TO_READER) ||
          (m_eSampleSource == SC_SAMPLE_SOURCE_READER) );
} // CSampleConsumer::DeliveringReaderSamples

bool CSampleConsumer::DeliveringProducerSamples()
{
    return DeliveringMediaSamples() && m_piSampleProducer &&
        ( (m_eSampleSource == SC_SAMPLE_SOURCE_CONVERTING_TO_PRODUCER) ||
          (m_eSampleSource == SC_SAMPLE_SOURCE_PRODUCER) );
} // CSampleConsumer::DeliveringProducerSamples

void CSampleConsumer::Pause()
{
    ASSERT(Running());

    StateManager<SAMPLE_CONSUMER_FILTER_STATE> tStateManager(&m_eSCFilterState);
    tStateManager.SetState(SC_FILTER_PAUSING);

    // TODO:  is there anything that needs to be done here?

    tStateManager.ConfirmState(SC_FILTER_PAUSED);
} // CSampleConsumer::Pause

void CSampleConsumer::Run()
{
    if (Running())
        return;

    ASSERT(!Stopped());

    StateManager<SAMPLE_CONSUMER_FILTER_STATE> tStateManager(&m_eSCFilterState);
    tStateManager.SetState(SC_FILTER_ABOUT_TO_RUN);

    if ((m_eSCPositionMode == SC_POSITION_AT_STOP_POSITION) &&
        m_pcPauseBufferSegmentBound &&
        (m_hyCurrentPosition >= 0) &&
        (/* (SC_STOP_MODE_NONE == m_eSCStopMode) || */
         ((GetTrueRate() >= 0.0) && (m_hyCurrentPosition >= m_pcPauseBufferSegmentBound->CurrentEndTime())) ||
         ((GetTrueRate() < 0.0) && (m_hyCurrentPosition <= m_pcPauseBufferSegmentBound->CurrentStartTime()))))
    {
        DbgLog((LOG_SOURCE_STATE, 3,
            _T("CSampleConsumer::Run() -- dispatching end-of-stream since we are sitting at a stop position\n") ));
        m_fPendingFailedRunEOS = true;
        m_pippmgr->GetRouter(NULL, false, true).EndOfStream();
        return;
    }

    if (!EnsureNotAtStopPosition(SC_INTERNAL_EVENT_RUN_SOURCE))
        UpdateMediaSource(SC_INTERNAL_EVENT_RUN_SOURCE);

#if defined(SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT)
    if (m_piSampleProducer && (m_eFilterStateProducer == State_Running))
    {
        if (!m_piGraphClock)
        {
            CDVRSourceFilter &rcDVRSourceFilter = m_pippmgr->GetSourceFilter();
            if (SUCCEEDED(rcDVRSourceFilter.GetSyncSource(&m_piGraphClock)) && m_piGraphClock)
            {
                REFERENCE_TIME rtCurrentTime;

                m_piGraphClock->Release();
                if (SUCCEEDED(m_piGraphClock->GetTime(&rtCurrentTime)))
                    m_rtProducerToConsumerClockOffset =
                        rtCurrentTime - m_piSampleProducer->GetProducerTime();
                else
                    m_piGraphClock = NULL;
            }
            else
                m_piGraphClock = NULL;
        }
    }
#endif // SAMPLE_CONSUMER_MONITOR_CLOCK_DRIFT

    tStateManager.ConfirmState(SC_FILTER_RUNNING);
} // CSampleConsumer::Run

void CSampleConsumer::Activate()
{
    ASSERT(Stopped());

    StateManager<SAMPLE_CONSUMER_FILTER_STATE> tStateManager(&m_eSCFilterState);
    tStateManager.SetState(SC_FILTER_NEVER_STARTED ? SC_FILTER_FIRST_ACTIVATION : SC_FILTER_ACTIVATING);

    StartStreamingThread();
    RebindToProducer();
    {
        CAutoLock cAutoLock(&m_cCritSecThread);

        if (m_pcConsumerStreamingThread)
            m_pcConsumerStreamingThread->SendProducerModeEvent();
    }

    UpdateMediaSource(SC_INTERNAL_EVENT_ACTIVATE_SOURCE);
    EnsureNotAtStopPosition(SC_INTERNAL_EVENT_ACTIVATE_SOURCE);
    tStateManager.ConfirmState(SC_FILTER_PAUSED);
} // CSampleConsumer::Activate

void CSampleConsumer::Deactivate()
{
    if (Stopped())
        return;

    ASSERT(!Running());

    StateManager<SAMPLE_CONSUMER_FILTER_STATE> tStateManager(&m_eSCFilterState);
    tStateManager.SetState(SC_FILTER_DEACTIVATING);

    DisableMediaSource(SC_INTERNAL_EVENT_DEACTIVATE_SOURCE);
    m_dblRate = 1.0;
    m_dblPreDeferNormalRate = 1.0;
    m_hyTargetOffsetFromLive = g_rtMinimumBufferingUpstreamOfLive;
    tStateManager.ConfirmState(SC_FILTER_STOPPED);
} // CSampleConsumer::Deactivate

FILTER_STATE CSampleConsumer::MapToSourceFilterState(SAMPLE_CONSUMER_FILTER_STATE eSampleConsumerFilterState)
{
    FILTER_STATE eFilterState;

    switch (eSampleConsumerFilterState)
    {
    case SC_FILTER_NEVER_STARTED:
    case SC_FILTER_STOPPED:
    case SC_FILTER_DEACTIVATING:
        eFilterState = State_Stopped;
        break;

    case SC_FILTER_FIRST_ACTIVATION:
    case SC_FILTER_ACTIVATING:
    case SC_FILTER_PAUSED:
    case SC_FILTER_PAUSING:
        eFilterState = State_Paused;
        break;

    case SC_FILTER_ABOUT_TO_RUN:
    case SC_FILTER_RUNNING:
        eFilterState = State_Running;
        break;
    }
    return eFilterState;
} // CSampleConsumer::MapToSourceFilterState

bool CSampleConsumer::EnforceStopPosition()
{
    if ((m_hyCurrentPosition < 0) ||
        (m_eSCStopMode == SC_STOP_MODE_NONE) ||
        (m_eSCPositionMode != SC_POSITION_NORMAL) ||
        (m_hyStopPosition == STOP_POSITION_NULL_VALUE))
    {
        // We don't know where we are yet so we can't possibly know
        // if we've hit a stop position. Or we weren't explicitly
        // told to stop at any particular position.
        return false;
    }
    if (((GetTrueRate() > 0.0) && (m_hyCurrentPosition < m_hyStopPosition)) ||
        ((GetTrueRate() < 0.0) && (m_hyCurrentPosition > m_hyStopPosition)))
    {
        return false;
    }

    SetPositionMode(SC_POSITION_AT_STOP_POSITION);
    bool fUseSegmentMode = (m_eSCStopMode == SC_STOP_MODE_END_SEGMENT);
    m_eSCStopMode = SC_STOP_MODE_NONE;
    if ((m_esbptp == STRMBUF_STAY_WITH_RECORDING) && m_pcPauseBufferSegmentBound &&
        !m_pcPauseBufferSegmentBound->m_fInProgress)
    {
        m_hyStopPosition = m_pcPauseBufferSegmentBound->m_hyEndTime;
    }
    else
        m_hyStopPosition = STOP_POSITION_NULL_VALUE;
    StopGraph(fUseSegmentMode ?
        PLAYBACK_AT_STOP_POSITION_NEW_SEGMENT : PLAYBACK_AT_STOP_POSITION);
    return true;
} // CSampleConsumer::EnforceStopPosition

void CSampleConsumer::ImportPauseBuffer(CPauseBufferData *pcPauseBufferData, LONGLONG hyLastRecordingPos)
{
    if (!m_pcPauseBufferHistory)
        m_pcPauseBufferHistory.Attach(new SampleProducer::CPauseBufferHistory(*this));
    if (pcPauseBufferData)
    {
#ifdef DEBUG
        DbgLog((LOG_PAUSE_BUFFER, 3, _T("CSampleConsumer::ImportPauseBuffer():\n")));
        size_t uNumRecordings = pcPauseBufferData->GetCount();
        for (size_t uRecordingIdx = 0; uRecordingIdx < uNumRecordings; ++uRecordingIdx)
        {
            const SPauseBufferEntry &rsPauseBufferEntry = (*pcPauseBufferData)[uRecordingIdx];
#ifdef UNICODE
            DbgLog((LOG_PAUSE_BUFFER, 4,
                _T("    file %s, recording %s, %s, starts at %I64d\n"),
                rsPauseBufferEntry.strFilename.c_str(),
                rsPauseBufferEntry.strRecordingName.c_str(),
                rsPauseBufferEntry.fPermanent ? _T("permanent") : _T("temporary"),
                rsPauseBufferEntry.tStart));
#else
            DbgLog((LOG_PAUSE_BUFFER, 4,
                _T("    file %S, recording %S, %s, starts at %I64d\n"),
                rsPauseBufferEntry.strFilename.c_str(),
                rsPauseBufferEntry.strRecordingName.c_str(),
                rsPauseBufferEntry.fPermanent ? _T("permanent") : _T("temporary"),
                rsPauseBufferEntry.tStart));
#endif
        }
#endif
        m_pcPauseBufferHistory->Import(*pcPauseBufferData);
    }

    EnsureRecordingIsSelected();

    if ((m_esbptp == STRMBUF_STAY_WITH_RECORDING) && (hyLastRecordingPos >= 0) &&
        m_pcPauseBufferSegmentBound)
    {
        m_pcPauseBufferSegmentBound->m_fInProgress = false;
        m_pcPauseBufferSegmentBound->m_hyEndTime = hyLastRecordingPos;
    }

    if ((m_esbptp == STRMBUF_STAY_WITH_RECORDING) && m_pcPauseBufferSegmentBound &&
        !m_pcPauseBufferSegmentBound->m_fInProgress)
    {
        // We're bound to a now-completed recording. Make sure that we have
        // the correct position for the ending of the recording (possibly this
        // is a newly ended recording):

        LONGLONG hyRecordingEnd = m_pcPauseBufferSegmentBound->m_hyEndTime;
        if (m_hyStopPosition > hyRecordingEnd)
            m_hyStopPosition = hyRecordingEnd;
    }

    // If deletion of a permanent recording has moved us from being safely
    // within the pause buffer to being outside, jump to live:

    if (m_pcPauseBufferHistory && m_pcPauseBufferSegment &&
        (m_esbptp == STRMBUF_STAY_WITH_SINK) && !Stopped())
    {
        LONGLONG hyMinimumSafePosition = m_pcPauseBufferHistory->GetSafePlaybackStart(GetPauseBufferSafetyMargin(false));
        bool fNeedJumpToLive = false;

        if ((m_hyTargetPosition >= 0) &&
            (m_hyTargetPosition < hyMinimumSafePosition) &&
            ((m_eSCPositionMode == SC_POSITION_PENDING_SET_POSITION) ||
             (m_eSCPositionMode == SC_POSITION_PENDING_RUN) ||
             (m_eSCPositionMode == SC_POSITION_PENDING_NORMAL_RATE)))
        {
            DbgLog((LOG_SOURCE_STATE, 3,
                _T("CSampleConsumer::ImportPauseBuffer() ... deleted where we were (%I64d), changing seek target\n"),
                m_hyCurrentPosition ));

            // Hmm ... we're in a messy state jumping to a position that isn't safe.
            // Better try to go to a different target.

            fNeedJumpToLive = true;
        }
        else if ((m_hyCurrentPosition >= 0) && (m_hyCurrentPosition < hyMinimumSafePosition))
        {
            DbgLog((LOG_SOURCE_STATE, 3,
                _T("CSampleConsumer::ImportPauseBuffer() ... deleted where we were (%I64d), jumping to live\n"),
                m_hyCurrentPosition ));

            // Easy case -- a seek will work cleanly.

            fNeedJumpToLive = true;
        };
        if (fNeedJumpToLive)
        {
            // Bad news -- we are now outside of the pause buffer.  Jump to live after blocking receipt
            // of samples until we can get there:

            LONGLONG hyMinPos, hyCurPos, hyMaxPos;
            HRESULT hr = GetConsumerPositions(hyMinPos, hyCurPos, hyMaxPos);
            if (FAILED(hr))
            {
                DbgLog((LOG_SOURCE_STATE, 3,
                    _T("CSampleConsumer::ImportPauseBuffer() ... unable to determine position bounds, bailing [hr=0x%x\\n"),
                    hr ));

                NoteBadGraphState(hr);
            }
            else
            {
                LONGLONG hyLivePosition = hyMaxPos - m_hyTargetOffsetFromLive;
                if (hyLivePosition < hyMinPos)
                    hyLivePosition = hyMinPos;

                DbgLog((LOG_SOURCE_STATE, 3,
                    _T("CSampleConsumer::ImportPauseBuffer() ... going to live at %I64d\n"),
                    hyLivePosition ));

                DisableMediaSource(SC_INTERNAL_EVENT_PAUSE_BUF);
                SetCurrentPosition(NULL, hyLivePosition,
                            true /* seek to key frame */, false /* do not prohibit a flush */,
                            true /* decoder driver needs to know */, false /* not calling from an app thread */);
            }
        }
    }

    UpdateMediaSource(SC_INTERNAL_EVENT_PAUSE_BUF);
} // CSampleConsumer::ImportPauseBuffer

bool CSampleConsumer::EnsureNotAtStopPosition(SAMPLE_CONSUMER_INTERNAL_EVENT_TYPE eSCInternalEventType)
{
    if ((m_eSCPositionMode == SC_POSITION_AT_STOP_POSITION) ||
        (m_eSCPositionMode == SC_POSITION_PENDING_NORMAL_RATE))
    {
        SetPositionMode((m_hyCurrentPosition < 0) ? SC_POSITION_UNKNOWN : SC_POSITION_ALIGNING);
        EnsureRecordingIsSelected();
        UpdateMediaSource(eSCInternalEventType);
        return true;
    }
    return false;
} // CSampleConsumer::EnsureNotAtStopPosition

void CSampleConsumer::LeaveOrphanedRecording(SampleProducer::CPauseBufferSegment *&pcPauseBufferSegmentNext)
{
    // There is a code logic 
    // being something other than a bound recording or null.
    ASSERT(m_pcPauseBufferHistory);
    ASSERT(m_pcPauseBufferSegment);
    ASSERT(m_pcPauseBufferSegment != m_pcPauseBufferSegmentBound);
    ASSERT(m_pcPauseBufferSegment != pcPauseBufferSegmentNext);
    
    // We need to do three things:
    // 1) eliminate all recordings up to and including the one we are leaving from the pause buffer
    // 2) if the proposed next segment is not itself an orphaned recording, jump to 'now'
    // 3) make sure we are positioned in a recording that survives pruning (we may be at the last
    //    sample of the pruned recording and not yet at the first sample of the next one)

    // Step 1:  prune the pause buffer to eliminate this recording (and any previous ones,
    //          though there shouldn't be any) and broadcast knowledge of the modified
    //          pause buffer:

    std::vector<SampleProducer::CPauseBufferSegment*>::iterator recIter;
    bool fSawTargetRecording = false;
    size_t uFilesDropped = 0;
    CSmartRefPtr<CPauseBufferData> pcPauseBufferData;
    pcPauseBufferData.Attach(m_pcPauseBufferHistory->GetPauseBufferData());

    for (recIter = m_pcPauseBufferHistory->m_listpcPauseBufferSegments.begin();
         recIter != m_pcPauseBufferHistory->m_listpcPauseBufferSegments.end();
        )
    {
        SampleProducer::CPauseBufferSegment *aRec = *recIter;

        if (!fSawTargetRecording && ((aRec == pcPauseBufferSegmentNext) || !aRec->m_fIsOrphaned))
        {
#ifdef UNICODE
            DbgLog((LOG_PAUSE_BUFFER, 3,
                _T("CSampleConsumer::LeaveOrphanedRecording():  stopping the prune with %s [%u files lost]\n"),
                aRec->GetRecordingName().c_str(),
                uFilesDropped));
#else
            DbgLog((LOG_PAUSE_BUFFER, 3,
                _T("CSampleConsumer::LeaveOrphanedRecording():  stopping the prune with %S [%u files lost]\n"),
                aRec->GetRecordingName().c_str(),
                uFilesDropped));
#endif
            
            ASSERT(aRec->m_uFirstRecordingFile == uFilesDropped);
            fSawTargetRecording = true;

            size_t uFileIndex;
            std::vector<SPauseBufferEntry>::iterator iter;

            for (uFileIndex = 0; uFileIndex < uFilesDropped; ++uFileIndex)
            {
                iter = pcPauseBufferData->m_vecFiles.begin();

#ifdef UNICODE
                DbgLog((LOG_PAUSE_BUFFER, 3,
                    _T("CSampleConsumer::LeaveOrphanedRecording():  forgetting file %s\n"),
                    iter->strFilename.c_str()));
#else
                DbgLog((LOG_PAUSE_BUFFER, 3,
                    _T("CSampleConsumer::LeaveOrphanedRecording():  forgetting file %S\n"),
                    iter->strFilename.c_str()));
#endif

                iter = pcPauseBufferData->m_vecFiles.erase(iter);
            }
        };
        if (fSawTargetRecording)
        {
            ASSERT(aRec->m_uFirstRecordingFile >= uFilesDropped);

#ifdef UNICODE
            DbgLog((LOG_PAUSE_BUFFER, 3,
                _T("CSampleConsumer::LeaveOrphanedRecording(): %s first recording %u -> %u\n"),
                aRec->GetRecordingName().c_str(),
                aRec->m_uFirstRecordingFile,
                aRec->m_uFirstRecordingFile - uFilesDropped));
#else
            DbgLog((LOG_PAUSE_BUFFER, 3,
                _T("CSampleConsumer::LeaveOrphanedRecording(): %S first recording %u -> %u\n"),
                aRec->GetRecordingName().c_str(),
                aRec->m_uFirstRecordingFile,
                aRec->m_uFirstRecordingFile - uFilesDropped));
#endif

            aRec->m_uFirstRecordingFile -= uFilesDropped;
            ++recIter;
        }
        else
        {
#ifdef UNICODE
            DbgLog((LOG_PAUSE_BUFFER, 3,
                _T("CSampleConsumer::LeaveOrphanedRecording():  forgetting recording %s [%u files]\n"),
                aRec->GetRecordingName().c_str(),
                aRec->m_uNumRecordingFiles));
#else
            DbgLog((LOG_PAUSE_BUFFER, 3,
                _T("CSampleConsumer::LeaveOrphanedRecording():  forgetting recording %S [%u files]\n"),
                aRec->GetRecordingName().c_str(),
                aRec->m_uNumRecordingFiles));
#endif

            uFilesDropped += aRec->m_uNumRecordingFiles;
            recIter = m_pcPauseBufferHistory->m_listpcPauseBufferSegments.erase(recIter);
        }
    }

    ASSERT(fSawTargetRecording);
    if (!fSawTargetRecording)
    {
        // Hmm. It looks like the pause buffer contained only orphaned recordings. This is truly
        // wierd. We'll clean out the pause buffer data anyway.

        pcPauseBufferData->m_vecFiles.clear();
    }

    m_pcPauseBufferSegment = NULL;
#ifdef UNICODE
    DbgLog((LOG_PAUSE_BUFFER, 3,
        _T("CONSUMER:  cleared current recording to %s\n"),
        m_pcPauseBufferSegment ? m_pcPauseBufferSegment->GetRecordingName().c_str() : L"[none]" ));
#else
    DbgLog((LOG_PAUSE_BUFFER, 3,
        _T("CONSUMER:  cleared current recording to %S\n"),
        m_pcPauseBufferSegment ? m_pcPauseBufferSegment->GetRecordingName().c_str() : L"[none]" ));
#endif

    try {
        FirePauseBufferUpdateEvent(pcPauseBufferData);
    } catch (const std::exception &) {};

    // Step 2:  change the target recording to the in-progress one
    //          unless the proposed target is also an orphan

    bool fPositionAtNow = false;
    if (pcPauseBufferSegmentNext && !pcPauseBufferSegmentNext->m_fIsOrphaned)
    {
        pcPauseBufferSegmentNext = m_pcPauseBufferHistory->GetCurrentSegment();
        fPositionAtNow = true;
        ASSERT(pcPauseBufferSegmentNext);
    }

    // Step 3:  make sure that our position is reported as being within the
    //          recording we are going to:

    if (pcPauseBufferSegmentNext)
    {
        switch (m_eSCPositionMode)
        {
        case SC_POSITION_UNKNOWN:
            break;

        case SC_POSITION_AWAITING_FIRST_SAMPLE:
        case SC_POSITION_SEEKING_KEY:
        case SC_POSITION_SEEKING_ANY:
        case SC_POSITION_ALIGNING:
        case SC_POSITION_NORMAL:
            if (fPositionAtNow)
            {
                m_hyCurrentPosition = pcPauseBufferSegmentNext->CurrentEndTime();
                SetPositionMode(SC_POSITION_AWAITING_FIRST_SAMPLE);
            }
            else if (m_hyCurrentPosition < pcPauseBufferSegmentNext->CurrentStartTime())
            {
                m_hyCurrentPosition = pcPauseBufferSegmentNext->CurrentStartTime();
                if (!pcPauseBufferSegmentNext->IsPermanent() &&
                    (m_hyCurrentPosition < m_pcPauseBufferHistory->GetSafePlaybackStart(GetPauseBufferSafetyMargin(true))))
                    m_hyCurrentPosition =  m_pcPauseBufferHistory->GetSafePlaybackStart(GetPauseBufferSafetyMargin(true));
                SetPositionMode(SC_POSITION_AWAITING_FIRST_SAMPLE);
            }
            break;

        case SC_POSITION_AT_STOP_POSITION:
            if (fPositionAtNow)
                m_hyCurrentPosition = pcPauseBufferSegmentNext->CurrentEndTime();
            else if (m_hyCurrentPosition < pcPauseBufferSegmentNext->CurrentStartTime())
            {
                m_hyCurrentPosition = pcPauseBufferSegmentNext->CurrentStartTime();
                if (!pcPauseBufferSegmentNext->IsPermanent() &&
                    (m_hyCurrentPosition < m_pcPauseBufferHistory->GetSafePlaybackStart(GetPauseBufferSafetyMargin(true))))
                    m_hyCurrentPosition =  m_pcPauseBufferHistory->GetSafePlaybackStart(GetPauseBufferSafetyMargin(true));
            }
            break;

        case SC_POSITION_PENDING_SET_POSITION:
        case SC_POSITION_PENDING_RUN:
            if (fPositionAtNow)
                m_hyCurrentPosition = pcPauseBufferSegmentNext->CurrentEndTime();
            else if (m_hyTargetPosition < pcPauseBufferSegmentNext->CurrentStartTime())
            {
                m_hyTargetPosition = pcPauseBufferSegmentNext->CurrentStartTime();
                if (!pcPauseBufferSegmentNext->IsPermanent() &&
                    (m_hyCurrentPosition < m_pcPauseBufferHistory->GetSafePlaybackStart(GetPauseBufferSafetyMargin(true))))
                    m_hyCurrentPosition =  m_pcPauseBufferHistory->GetSafePlaybackStart(GetPauseBufferSafetyMargin(true));
            }
            break;

        default:
            ASSERT(0);
            break;
        }
    }
    else
    {
        // We shouldn't be here unless the graph is pretty confused. If there are
        // orphans, we're in live TV so there should've been a current recording.

        ASSERT(0);
        switch (m_eSCPositionMode)
        {
        case SC_POSITION_UNKNOWN:
            break;

        case SC_POSITION_AWAITING_FIRST_SAMPLE:
        case SC_POSITION_SEEKING_KEY:
        case SC_POSITION_SEEKING_ANY:
        case SC_POSITION_ALIGNING:
        case SC_POSITION_NORMAL:
            m_hyCurrentPosition = CURRENT_POSITION_NULL_VALUE;
            SetPositionMode(SC_POSITION_UNKNOWN);
            break;

        case SC_POSITION_AT_STOP_POSITION:
            m_hyCurrentPosition = CURRENT_POSITION_NULL_VALUE;
            break;

        case SC_POSITION_PENDING_SET_POSITION:
            // Yikes!  Hopefully, we in the middle of seeking to a valid position.
            // Otherwise we are likely to stop the graph due to confusion about
            // the position.
            break;

        case SC_POSITION_PENDING_RUN:
            m_hyCurrentPosition = CURRENT_POSITION_NULL_VALUE;
            break;

        default:
            ASSERT(0);
            break;
        }
    }

#ifdef UNICODE
    DbgLog((LOG_PAUSE_BUFFER, 3,
        _T("CSampleConsumer::LeaveOrphanedRecording():  now at 0x%I64d (pos mode %d) in %s\n"),
        ((m_eSCPositionMode == SC_POSITION_PENDING_SET_POSITION) ||
         (m_eSCPositionMode == SC_POSITION_PENDING_RUN)) ? m_hyTargetPosition : m_hyCurrentPosition,
        m_eSCPositionMode,
        pcPauseBufferSegmentNext ? pcPauseBufferSegmentNext->GetRecordingName().c_str() : L"[null]"));
#else
    DbgLog((LOG_PAUSE_BUFFER, 3,
        _T("CSampleConsumer::LeaveOrphanedRecording():  now at 0x%I64d (pos mode %d) in %S\n"),
        ((m_eSCPositionMode == SC_POSITION_PENDING_SET_POSITION) ||
         (m_eSCPositionMode == SC_POSITION_PENDING_RUN)) ? m_hyTargetPosition : m_hyCurrentPosition,
        m_eSCPositionMode,
        pcPauseBufferSegmentNext ? pcPauseBufferSegmentNext->GetRecordingName().c_str() : L"[null]"));
#endif
} // CSampleConsumer::LeaveOrphanedRecording

HRESULT CSampleConsumer::DoLoad(
    /* [in] */ bool fIsLiveTv,
    /* [in] */ LPCOLESTR pszFileName,
    /* [unique][in] */ const AM_MEDIA_TYPE *pmt)
{
    HRESULT hr = S_OK;

    try {
        ClearPlaybackPosition(SC_INTERNAL_EVENT_LOAD);
        ClearRecordingHistory();
        m_hyPostFlushPosition = POST_FLUSH_POSITION_INVALID;
        m_hyPostTunePosition = POST_FLUSH_POSITION_INVALID;

        IDecoderDriver *piDecoderDriver = GetDecoderDriver();
        if (piDecoderDriver)
            piDecoderDriver->ImplementLoad();
        if (!pszFileName || (*pszFileName == 0))
        {
            m_pszFileName = L"";
            return hr;
        }
        UpdateRecordingName(fIsLiveTv, pszFileName);
        if (fIsLiveTv)
        {
            if (!m_piSampleProducer)
                hr = E_INVALIDARG;
            else if (m_esbptp != STRMBUF_STAY_WITH_SINK)
                hr = E_INVALIDARG;
        };
        if (SUCCEEDED(hr))
        {
            m_hyStopPosition = STOP_POSITION_NULL_VALUE;
            if (!m_piSampleProducer)
            {
                ASSERT(!m_pcPauseBufferHistory);
                IReader *piReader = GetReader();
                ASSERT(piReader);  // we should've blocked configure pipeline if no reader
                LONGLONG hyRecordingEnd = piReader->GetMaxSeekPosition();
                m_pcPauseBufferHistory.Attach(new SampleProducer::CPauseBufferHistory(
                                            *this, m_pszFileName,
                                            piReader->GetMinSeekPosition(),
                                            hyRecordingEnd));
                if (!m_pcPauseBufferHistory->WasSuccessfullyConstructed())
                {
                    m_pcPauseBufferHistory = NULL;
                    hr = E_FAIL;
                }
                else
                    m_hyStopPosition = hyRecordingEnd;
            }
            else if (!m_pcPauseBufferHistory)
                m_pcPauseBufferHistory.Attach(new SampleProducer::CPauseBufferHistory(*this));
        }

        if (SUCCEEDED(hr) && (m_esbptp == STRMBUF_STAY_WITH_RECORDING))
        {
            EnsureRecordingIsSelected();
            if (!m_pcPauseBufferSegmentBound)
            {
                DbgLog((LOG_ERROR, 3, _T("CSampleConsumer::Load():  either the recording is temporary or the handshake with the writer is bad\n")));
                hr = E_INVALIDARG;
            }
        }
    }
    catch (const CHResultError& rcHResultError)
    {
#ifdef UNICODE
        DbgLog((LOG_ERROR, 2,
            _T("CSampleConsumer(%p)::Load() caught exception %S\n"),
            this, rcHResultError.what()));
#else
        DbgLog((LOG_ERROR, 2,
            _T("CSampleConsumer(%p)::Load() caught exception %s\n"),
            this, rcHResultError.what()));
#endif
        hr = rcHResultError.m_hrError;
    }
    catch (const std::exception& rcException)
    {
        UNUSED (rcException);  // suppress release build warning
#ifdef UNICODE
        DbgLog((LOG_ERROR, 2,
            _T("CSampleConsumer(%p)::Load() caught exception %S\n"),
            this, rcException.what()));
#else
        DbgLog((LOG_ERROR, 2,
            _T("CSampleConsumer(%p)::Load() caught exception %s\n"),
            this, rcException.what()));
#endif
        hr = E_FAIL;
    }
    return hr;
}

void CSampleConsumer::ResumeNormalPlayIfNeeded()
{
    try
    {
        IDecoderDriver *piDecoderDriver = NULL;

        {
            // If we are playing at a rate less than 1.0 and the truncation
            // point is close, we need to get the graph running at 1x.
            // Remember that only temporary recordings have truncation
            // points.

            CAutoLock cAutoLock(&m_cCritSecState);

            DWORD dwSysTicksNow = GetTickCount();
            if (m_fResumingNormalPlay &&
                ((dwSysTicksNow - m_dwResumePlayTimeoutTicks) < 2000))
                return;  // too soon to try again
            piDecoderDriver = m_piDecoderDriver;
            if (!piDecoderDriver || (SC_LOAD_COMPLETE != m_eSCLoadState) || (m_hyCurrentPosition < 0))
            {
                m_fResumingNormalPlay = false;
                return;
                        // there is no point in this -- we have not loaded a recording
                        // or we have no decoder driver to do the work of setting
                        // things aright.
            }

            if (Running() && (m_eSCPositionMode == SC_POSITION_PENDING_NORMAL_RATE))
            {
                // We'll resume once playback reaches the expected position (or is frozen for too long)

                m_fResumingNormalPlay = false;
                return;
            }

            if (Running() && (GetTrueRate() >= 1.0) && (m_fFlushing || DeliveringMediaSamples()))
            {
                m_fResumingNormalPlay = false;
                return; // We are already playing fast enough to stay ahead
            }

            if (!m_pcPauseBufferSegment || m_pcPauseBufferSegment->IsPermanent())
            {
                m_fResumingNormalPlay = false;
                return; // We are not in a temporary recording
            }

            // Because we are paused, we'll need extra time to turn the graph around. So do
            // add the wider position margin here even though we are checking if we need to
            // do a change to playback state:

            if (m_hyCurrentPosition >
                    m_pcPauseBufferHistory->GetSafePlaybackStart(GetPauseBufferSafetyMargin(true)))
            {
                m_fResumingNormalPlay = false;
                return; // We are safely ahead for the moment
            }

            // We are dangerously close to the truncation point:

            DbgLog((LOG_EVENT_DETECTED, 3, _T("CSampleConsumer::ResumeNormalPlayIfNeeded() .. resuming to avoid truncation\n")));
            piDecoderDriver->ImplementRun(false);
            m_piDecoderDriver->IssueNotification(this, DVRENGINE_EVENT_BEGINNING_OF_PAUSE_BUFFER,
                        DVRENGINE_PAUSED_UNTIL_TRUNCATION, m_dwLoadIncarnation);
            if (m_dblRate < 1.0)
                SetGraphRate(1.0);
            EnsureNotAtStopPosition(SC_INTERNAL_EVENT_AVOIDING_TRUNCATION);
            m_fResumingNormalPlay = true;
            m_dwResumePlayTimeoutTicks = dwSysTicksNow;
        }
    }
    catch (const std::exception& rcException)
    {
        UNUSED (rcException);  // suppress release build warning
#ifdef UNICODE
        DbgLog((LOG_ERROR, 2,
            _T("CSampleConsumer(%p)::ResumeNormalPlayIfNeeded() caught exception %S\n"),
            this, rcException.what()));
#else
        DbgLog((LOG_ERROR, 2,
            _T("CSampleConsumer(%p)::ResumeNormalPlayIfNeeded() caught exception %s\n"),
            this, rcException.what()));
#endif
    };
}

void CSampleConsumer::CompensateForWhackyClock()
{
    // Sometimes, the MediaCenter DVR product is so overwhelmed that it just
    // can't record or capture at anywhere near 1x.  For the 1.5 release, we
    // did lots of tuning to minimize the cases in which things fall apart but
    // for the remaining cases (esp peak disk demand during EPG translation),
    // we need to do something to accelerate recovery. The idea is to keep an
    // eye out for the clock slaving code running at an unmanagably fast or slow
    // rate. If that is observed, determine where playback is and do a flushing
    // seek to that position.  The decoder driver will take care of restoring
    // order during the flush.

    if (m_pippmgr && IsLiveOrInProgress() && Running() &&
        (SC_SAMPLE_SOURCE_NONE != m_eSampleSource) &&
        (SC_POSITION_NORMAL == m_eSCPositionMode))
    {
        IDecoderDriver *piDecoderDriver = NULL;
        CDVRSourceFilter &rcDVRSourceFilter = m_pippmgr->GetSourceFilter();
        CDVRClock &rcDVRClock = rcDVRSourceFilter.GetDVRClock();
        LONGLONG hySeekPos = -1LL;

        if (rcDVRClock.SawClockGlitch())
        {
            CAutoLock cAutoLock(&m_cCritSecState);

            DbgLog((LOG_ERROR, 3, TEXT("CSampleConsumer::CompensateForWhackyClock() -- clock needs assist to correct quickly\n") ));

            LONGLONG hyMinPos, hyMaxPos;
            if (FAILED(GetConsumerPositionsExternal(hyMinPos, hySeekPos, hyMaxPos)))
            {
                DbgLog((LOG_ERROR, 3, TEXT("CSampleConsumer::CompensateForWhackyClock() -- failed to get current position, giving up\n") ));
                return;
            }
            piDecoderDriver = GetDecoderDriver();
        }
        if (hySeekPos != -1LL)
        {
            DbgLog((LOG_WARNING, 3, TEXT("CSampleConsumer::CompensateForWhackyClock() -- attempting recovery\n")));

            HRESULT hrSeekResult = E_FAIL;
            CComQIPtr<IStreamBufferMediaSeeking> piStreamBufferMediaSeeking = &rcDVRSourceFilter;
            if (piStreamBufferMediaSeeking)
            {
                hrSeekResult = piStreamBufferMediaSeeking->SetPositions(&hySeekPos, AM_SEEKING_AbsolutePositioning | AM_SEEKING_SeekToKeyFrame,
                                                NULL, AM_SEEKING_NoPositioning);
                piStreamBufferMediaSeeking.Release();
            }

            if (piDecoderDriver && (S_OK != hrSeekResult))
            {
                CAutoLock cAutoLockApp(rcDVRSourceFilter.GetAppLock());

                if (piDecoderDriver->ImplementBeginFlush())
                {
                    piDecoderDriver->ImplementEndFlush();
                }
            }
            rcDVRClock.ClearClockGlitch();
        }
    }
}

void CSampleConsumer::BeginFlush(bool fFromStreamingThread)
{
    // Remember -- never hold a consumer lock when calling into the producer's
    // update mode:
    ISampleProducer *piSampleProducer = NULL;

    {
        CAutoLock cAutoLock(&m_cCritSecState);
        m_fFlushing = true;
        if (m_piReader)
            m_piReader->SetIsStreaming(false);
        m_eSampleProducerActiveMode = PRODUCER_SUPPRESS_SAMPLES;
        piSampleProducer = m_piSampleProducer;
    }
    if (piSampleProducer)
        piSampleProducer->UpdateMode(PRODUCER_SUPPRESS_SAMPLES, ++m_uSampleProducerModeIncarnation, 0, this);

    if (!fFromStreamingThread)
    {
        CAutoLock cAutoLock(&m_cCritSecThread);
        if (m_pcConsumerStreamingThread)
        {
            m_pcConsumerStreamingThread->FlushEvents();
        }
    }
    else
    {
        // In this case, we dare not grab m_cCritSecThread -- this can deadlock if we are trying
        // to stop the filter graph. We know that the thread won't go away because the thread
        // is making the call. FlushEvents() will take care of locking the queue only.
        m_pcConsumerStreamingThread->FlushEvents();
    }

    DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::BeginFlush():  at %I64d using %s, producer mode %d (%d active)\n"),
        m_hyCurrentPosition,
        IsSourceReader() ? _T("reader") : ( IsSourceProducer() ? _T("producer") : _T("nothing") ),
        m_eSampleProducerActiveMode, m_eSampleProducerMode ));
} // CSampleConsumer::BeginFlush

void CSampleConsumer::EndFlush(bool fStopping, bool fFromStreamingThread)
{
    // Also remember to never hold consumer locks while telling the
    // producer to update its state:

    bool fUpdateProducerMode = false;

    {
        CAutoLock cAutoLock(&m_cCritSecState);

        m_fFlushing = false;
        m_hyPostTunePosition = POST_FLUSH_POSITION_INVALID;
        switch (m_eSCPositionMode)
        {
        case SC_POSITION_UNKNOWN:
            m_hyPostFlushPosition = POST_FLUSH_POSITION_PENDING;
            break;

        case SC_POSITION_AWAITING_FIRST_SAMPLE:
        case SC_POSITION_SEEKING_KEY:
        case SC_POSITION_SEEKING_ANY:
        case SC_POSITION_ALIGNING:
        case SC_POSITION_PENDING_NORMAL_RATE:
            m_hyPostFlushPosition = (m_hyCurrentPosition >= 0) ? m_hyCurrentPosition : POST_FLUSH_POSITION_PENDING;
            break;

        case SC_POSITION_NORMAL:
        case SC_POSITION_AT_STOP_POSITION:
            m_hyPostFlushPosition = POST_FLUSH_POSITION_PENDING;
            break;

        case SC_POSITION_PENDING_SET_POSITION:
        case SC_POSITION_PENDING_RUN:
            m_hyPostFlushPosition = m_hyTargetPosition;
            break;

        default:
            ASSERT(0);
            break;
        }
        DbgLog((LOG_SOURCE_STATE, 3, _T("SampleConsumer::EndFlush():  set post-flush position reporting bound to %I64d\n"),
                m_hyPostFlushPosition));

        if (fStopping || !CanDeliverMediaSamples(false))
        {
            DbgLog((LOG_SOURCE_STATE, 3, _T("SampleConsumer::EndFlush():  unable to deliver samples, not activating anything\n")));
        }
        else if (IsSourceReader())
        {
            DbgLog((LOG_SOURCE_STATE, 3, _T("SampleConsumer::EndFlush():  source was reader, enabling streaming\n")));
            m_piReader->SetIsStreaming(true);
        }
        else if (IsSourceProducer())
        {
            DbgLog((LOG_SOURCE_STATE, 3, _T("SampleConsumer::EndFlush():  source was producer, will update mode from %d to %d\n"),
                m_eSampleProducerActiveMode, m_eSampleProducerMode));

            fUpdateProducerMode = true;
        }
        else
        {
            DbgLog((LOG_SOURCE_STATE, 3, _T("SampleConsumer::EndFlush():  source not known, calling UpdateMediaSource()\n")));
            UpdateMediaSource(SC_INTERNAL_EVENT_FLUSH_DONE);
        }
    }
    if (fUpdateProducerMode && !fStopping)
        DoUpdateProducerMode();

    DbgLog((LOG_SOURCE_STATE, 3, _T("CSampleConsumer::EndFlush(%u):  at %I64d using %s, producer mode %d (%d active)\n"),
        (unsigned) fStopping, m_hyCurrentPosition,
        IsSourceReader() ? _T("reader") : ( IsSourceProducer() ? _T("producer") : _T("nothing") ),
        m_eSampleProducerActiveMode, m_eSampleProducerMode));
}

HRESULT CSampleConsumer::GetConsumerPositionsExternal(LONGLONG &hyMinPos,
                                                      LONGLONG &hyCurPos, LONGLONG &hyMaxPos,
                                                      bool fLieAboutEndOfMedia)
{
    HRESULT hr = GetConsumerPositions(hyMinPos, hyCurPos, hyMaxPos);
    if (SUCCEEDED(hr))
    {
        DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::GetConsumerPositionsExternal():  internal positions %I64d min, %I64d now, %I64d max\n"),
            hyMinPos, hyCurPos, hyMaxPos ));

        LONGLONG hyOrigCurPos = hyCurPos;
        if (m_piDecoderDriver && !m_fFlushing)
        {
            LONGLONG hyDecoderCurPos = m_piDecoderDriver->EstimatePlaybackPosition(true, true);
            DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::GetConsumerPositionsExternal():  estimated rendered position %I64d\n"),
                hyDecoderCurPos));
            if (fLieAboutEndOfMedia && !m_piSampleProducer &&
                (DECODER_DRIVER_PLAYBACK_POSITION_END_OF_MEDIA == hyDecoderCurPos))
                hyDecoderCurPos = hyMaxPos;
            else if (fLieAboutEndOfMedia && m_pcPauseBufferSegmentBound &&
                     (DECODER_DRIVER_PLAYBACK_POSITION_START_OF_MEDIA == hyDecoderCurPos))
                hyDecoderCurPos = hyMinPos;

            if ((hyDecoderCurPos >= 0) && (hyDecoderCurPos <= hyMaxPos) && (hyDecoderCurPos >= hyMinPos))
            {
                if (m_hyPostFlushPosition == POST_FLUSH_POSITION_PENDING)
                {
                    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::GetConsumerPositionsExternal() ignoring decoder value because no post-flush sample yet\n") ));
                }
                else if ((m_hyPostFlushPosition != POST_FLUSH_POSITION_INVALID) &&
                    (((GetTrueRate() >= 0) && (hyDecoderCurPos < m_hyPostFlushPosition)) ||
                    ((GetTrueRate() < 0) && (hyDecoderCurPos > m_hyPostFlushPosition))))
                {
                    ASSERT((hyMinPos <= m_hyPostFlushPosition) && (hyMaxPos >= m_hyPostFlushPosition));

                    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::GetConsumerPositionsExternal() ignoring decoder value in favor of flush value %I64d\n"),
                        m_hyPostFlushPosition));
                    hyCurPos = m_hyPostFlushPosition;
                }
                else
                {
                    hyCurPos = hyDecoderCurPos;
                    m_hyPostFlushPosition = POST_FLUSH_POSITION_INVALID;
                }
            }
            else if (DECODER_DRIVER_PLAYBACK_POSITION_UNKNOWN == hyDecoderCurPos)
            {
                if (m_hyPostFlushPosition == POST_FLUSH_POSITION_PENDING)
                {
                    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::GetConsumerPositionsExternal() no decoder info, no post-flush sample yet\n") ));
                }
                else if (m_hyPostFlushPosition != POST_FLUSH_POSITION_INVALID)
                {
                    ASSERT((hyMinPos <= m_hyPostFlushPosition) && (hyMaxPos >= m_hyPostFlushPosition));

                    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::GetConsumerPositionsExternal() no decoder info, using flush value %I64d\n"),
                        m_hyPostFlushPosition));
                    hyCurPos = m_hyPostFlushPosition;
                }
                else
                {
                    hyCurPos = hyDecoderCurPos;
                }
            }
            else if ((hyDecoderCurPos >= 0) && (hyDecoderCurPos <= hyMinPos))
            {
                DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::GetConsumerPositionsExternal() decoder pos is smaller than minimum one, reporting minimum\n")));
                hyCurPos = hyMinPos;
            }
        }

        if ((m_eSCPositionMode == SC_POSITION_PENDING_SET_POSITION) ||
            (m_eSCPositionMode == SC_POSITION_PENDING_RUN))
        {
            hyCurPos = m_hyTargetPosition;
        }
        if ((hyCurPos == CURRENT_POSITION_NULL_VALUE) ||
            (m_eSCPositionMode == SC_POSITION_UNKNOWN) ||
            (m_eSCPositionMode == SC_POSITION_AWAITING_FIRST_SAMPLE))
        {
            DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::GetConsumerPositionsExternal() predicting position since no valid position yet\n")));

            if ((hyCurPos == CURRENT_POSITION_NULL_VALUE) &&
                (hyOrigCurPos != -1) &&
                (m_eSCPositionMode != SC_POSITION_UNKNOWN))
            {
                // We already know were we want to go. So it makes sense to report that position.
                // Just make sure that if we are bound to live tv, that we back the position
                // up if needed to be safely before 'now':

                hyCurPos = hyOrigCurPos;
                if ((STRMBUF_STAY_WITH_SINK == m_esbptp) &&
                    (m_hyTargetOffsetFromLive != 0) &&
                    (hyMinPos >= 0) && (hyMaxPos >= 0) &&
                    (hyCurPos > hyMaxPos - m_hyTargetOffsetFromLive))
                {
                    hyCurPos = hyMaxPos - m_hyTargetOffsetFromLive;
                    if (hyCurPos < hyMinPos)
                        hyCurPos = hyMinPos;
                    m_hyPostFlushPosition = hyCurPos;
                }
            }
            else
            {
                switch (m_esbptp)
                {
                case STRMBUF_STAY_WITH_RECORDING:
                    hyCurPos = hyMinPos;
                    break;

                case STRMBUF_STAY_WITH_SINK:
                    hyCurPos = hyMaxPos;
                    if (hyCurPos == -1)
                        hyMinPos = -1;
                    else if (m_hyTargetOffsetFromLive != 0)
                    {
                        // Our current position will start a safe distance back from live:
                        if ((hyMinPos >= 0) && (hyMaxPos >= 0))
                        {
                            if (hyMinPos + m_hyTargetOffsetFromLive > hyMaxPos)
                                hyCurPos = hyMinPos;
                            else
                            {
                                hyCurPos = hyMaxPos - m_hyTargetOffsetFromLive;
                            }
                            m_hyCurrentPosition = hyCurPos;
                            SetPositionMode(SC_POSITION_AWAITING_FIRST_SAMPLE);
                            m_hyPostFlushPosition = hyCurPos;
                        }
                    }
                    break;

                }
            }
        }

        // Be prepared to lie about being at/beyond a channel change as part of the
        // lazy [but low-glitch] fast-tune change:
        if ((m_hyTargetOffsetFromLive !=0) &&
            fLieAboutEndOfMedia &&
            (m_hyPostTunePosition != POST_FLUSH_POSITION_INVALID) &&
            (GetTrueRate() > 0) && (hyCurPos < m_hyPostTunePosition))
            hyCurPos = m_hyPostTunePosition;

        if ((m_esbptp == STRMBUF_STAY_WITH_SINK) &&
            m_pcPauseBufferHistory)
        {
            // The reader may know about a pause buffer file in a temporary recording
            // that lies partly outside the logical pause buffer. We don't want to
            // use that part of the pause buffer because it may vanish at any time
            // and cause an A/V hiccup -- as well as because the pause buffer size
            // 'bonus' is not per spec.  See if we're in the truncation window:

            LONGLONG hyTruncationPos = m_pcPauseBufferHistory->GetTruncationPosition();
            if (hyTruncationPos > hyMinPos)
            {
                // We are in the truncation window. If the oldest recording is a permanent
                // recording, do nothing -- this is the one legit 'bonus' to the pause
                // buffer. If the oldest recording is temporary, our earliest legit
                // seek point is the truncation point.

                SampleProducer::CPauseBufferSegment *pcPauseBufferSegment =
                    m_pcPauseBufferHistory->FindSegmentByTime(0,
                    SampleProducer::CPauseBufferHistory::PREFER_EARLIER_TIME);
                if (!pcPauseBufferSegment || !pcPauseBufferSegment->IsPermanent())
                {
                    hyMinPos = hyTruncationPos;
                    if (hyCurPos < hyMinPos)
                        hyMinPos = hyCurPos;
                }
                if (pcPauseBufferSegment)
                    pcPauseBufferSegment->Release();
            }
        }

        if ((hyMinPos > hyCurPos) && fLieAboutEndOfMedia &&
            !m_pcPauseBufferSegmentBound &&
            (m_eSCPositionMode == SC_POSITION_PENDING_NORMAL_RATE))
        {
            // While we have halted processing of data waiting for rendering
            // to work its way to the beginning of the pause buffer, the
            // beginning of the pause buffer is moving forward it time. It is
            // perfectly plausible that the beginning could move to a point
            // later in time that what we'd earlier sent downstream. We're
            // going to resume 1x playback at the beginning of the pause
            // buffer (shortly).  Tell the lie:

            hyCurPos = hyMinPos + GetPauseBufferSafetyMargin(true);
        }

        ASSERT(fLieAboutEndOfMedia || ((hyMinPos <= hyCurPos) && (hyCurPos <= hyMaxPos)));
    }
    DbgLog((LOG_SAMPLE_CONSUMER, 3, _T("CSampleConsumer::GetConsumerPositionsExternal():  returning %I64d min, %I64d now, %I64d max\n"),
            hyMinPos, hyCurPos, hyMaxPos ));
    return hr;
}

CPauseBufferData *CSampleConsumer::MergeWithOrphans(LONGLONG &hyBoundRecordingEndPos, CPauseBufferData *pDataArg, bool fOldDataIsValid)
{
    // The caller is holding onto a reference to pDataArg. We are responsible for either returning
    // pData after incrementing its reference or returning a new CPauseBufferData with a reference
    // already dedicated to the caller. We use a smart pointer to keep alive whatever we are
    // considering as a return value.

    hyBoundRecordingEndPos = -1LL;

    CSmartRefPtr<CPauseBufferData> pData = pDataArg;
    LONGLONG hyTruncationPoint = m_pcPauseBufferHistory ? m_pcPauseBufferHistory->GetTruncationPosition() : -1;

    DbgLog((LOG_PAUSE_BUFFER, 3, _T("CSampleConsumer::MergeWithOrphans(%p) -- position %I64d, truncation %I64d\n"),
        pData, m_hyCurrentPosition, hyTruncationPoint));

#ifdef DEBUG
    if (pData)
    {
        DbgLog((LOG_PAUSE_BUFFER, 4, _T("\nPause buffer content [reported from the capture graph]:\n")));

        size_t uFileIndex;
        size_t uNumFiles = pData->GetCount();
        for (uFileIndex = 0;
             uFileIndex < uNumFiles;
             ++uFileIndex)
        {
            SPauseBufferEntry sPauseBufferEntry;

            sPauseBufferEntry = (*pData)[uFileIndex];
#ifdef UNICODE
            DbgLog((LOG_PAUSE_BUFFER, 4,
                _T("        %s:  %I64d (%s)\n"),
                sPauseBufferEntry.strFilename.c_str(),
                sPauseBufferEntry.tStart,
                sPauseBufferEntry.fPermanent ? _T("permanent") : _T("temporary") ));
#else
            DbgLog((LOG_PAUSE_BUFFER, 4,
                _T("        %S:  %I64d (%s)\n"),
                sPauseBufferEntry.strFilename.c_str(),
                sPauseBufferEntry.tStart,
                sPauseBufferEntry.fPermanent ? _T("permanent") : _T("temporary") ));
#endif
        }
    }
#endif // DEBUG

    if (pData && (m_esbptp == STRMBUF_STAY_WITH_RECORDING) && !m_pszFileName.empty())
    {
        // Discard pause buffer entries that are not part of the recording we
        // are playing:

        // CreateNew() returns a CPauseBufferData with its creation reference
        // already set to 1. Hold onto that without further increments.

        CSmartRefPtr<CPauseBufferData> pNewData;
        pNewData.Attach(pData->CreateNew());

        size_t uNumFiles2 = pData->GetCount();
        size_t uFileIndex2;
        bool fSawRecording = false;

        for (uFileIndex2 = 0; uFileIndex2 < uNumFiles2; ++uFileIndex2)
        {
            SPauseBufferEntry sPauseBufferEntry = (*pData)[uFileIndex2];

            if (!sPauseBufferEntry.fPermanent)
            {
                // Do not include this one -- it is not part of a permanent recording
                
                if (fSawRecording && (hyBoundRecordingEndPos == -1LL))
                {
                    hyBoundRecordingEndPos = sPauseBufferEntry.tStart - 1LL;
                }
                continue;
            }

            if (m_pszFileName.compare(sPauseBufferEntry.strRecordingName) != 0)
            {
                // Do not include this one -- it is not part of the recording we are
                // bound to:
                if (fSawRecording && (hyBoundRecordingEndPos == -1LL))
                {
                    hyBoundRecordingEndPos = sPauseBufferEntry.tStart - 1LL;
                }
                continue;
            }

            // Okay, we've found a file that is part of the permanent recording that
            // we are bound to:

            fSawRecording = true;
            (void) pNewData->SortedInsert(sPauseBufferEntry);
        }

        if ((pNewData->GetCount() == 0) && fOldDataIsValid && m_pcPauseBufferHistory)
        {
            // The recording either hasn't gotten underway yet or has completed
            // and fallen out of the pause buffer. We need to add the files we
            // already know about:

            DbgLog((LOG_PAUSE_BUFFER, 3, _T("CSampleConsumer::MergeWithOrphans() -- our bound recording is no longer part of the pause buffer, using old data\n") ));

            CSmartRefPtr<CPauseBufferData> pOldData;

            pOldData.Attach(m_pcPauseBufferHistory->GetPauseBufferData());

            size_t uNumFiles3 = pOldData->GetCount();
            size_t uFileIndex3;

            for (uFileIndex3 = 0; uFileIndex3 < uNumFiles3; ++uFileIndex3)
            {
                SPauseBufferEntry sPauseBufferEntry = (*pOldData)[uFileIndex3];

                if (!sPauseBufferEntry.fPermanent)
                {
                    // Do not include this one -- it is not part of a permanent recording
                    
                    continue;
                }

                if (m_pszFileName.compare(sPauseBufferEntry.strRecordingName) != 0)
                {
                    // Do not include this one -- it is not part of the recording we are
                    // bound to:
                    continue;
                }

                // Okay, we've found a file that is part of the permanent recording that
                // we are bound to:

                (void) pNewData->SortedInsert(sPauseBufferEntry);
            }
        }

        // Now we can do an assignment -- the smart pointer code will ensure that
        // the old pData gets its ref count dropped, pData holds onto a ref for as
        // long as we are in this routine, and scoping of pNewData will release the
        // creation ref.
        pData = pNewData;
    }
    // TODO someday -- if pData is NULL, we are likely disconnecting from the sample producer.
    //                 If so, we want to freeze our snapshot based on the current pause buffer
    //                 and mark the last as complete -- if bound to a recording. In practice
    //                 right now, we don't disconnect from the sample producer upon completion
    //                 and so there is way to test this future scenario. I might be wrong about
    //                 disconnecting being the only NULL case.

    if (!fOldDataIsValid)
    {
        DbgLog((LOG_PAUSE_BUFFER, 3, _T("CSampleConsumer::MergeWithOrphans() -- old data is not valid\n") ));

        return pData.Detach();
    }

    if (!pData ||
        !m_pcPauseBufferHistory ||
        (m_esbptp == STRMBUF_STAY_WITH_RECORDING) ||
        (m_hyCurrentPosition < 0) ||
        (m_eSCPositionMode == SC_POSITION_UNKNOWN) ||
        (m_eSCPositionMode == SC_POSITION_PENDING_SET_POSITION) ||
        (hyTruncationPoint <= m_hyCurrentPosition) ||
        !m_pcPauseBufferSegment)
    {
        // We do not have any orphans in these situations. Just add a reference
        // to the data and return it.

        DbgLog((LOG_PAUSE_BUFFER, 3, _T("CSampleConsumer::MergeWithOrphans() -- early exit (%s, %d)\n"),
            (m_esbptp == STRMBUF_STAY_WITH_RECORDING) ? _T("bound-to-recording") : _T("live-tv"),
            (int) m_eSCPositionMode));

        return pData.Detach();
    }

    // It might be that the recording we are in was a permanent recording but is
    // now temporary. Check for that and bring the info on the current recording
    // in-sync first.  We don't want to use stale permanent vs temporary info when
    // computing orphaned recordings:

    if (m_pcPauseBufferSegment->IsPermanent())
    {
        size_t uNewFileCount = pData->GetCount();
        size_t uFileIdx;
        for (uFileIdx = 0; uFileIdx < uNewFileCount; ++uFileIdx)
        {
            SPauseBufferEntry continuedFile = (*pData)[uFileIdx];
            if (continuedFile.strRecordingName.compare(m_pcPauseBufferSegment->GetRecordingName()) == 0)
            {
                // We've found the start of the series of names corresponding to our
                // current recording.  See if we can find one in our recording that
                // is now temporary. That is a sign of a deletion-by-conversion-to-temporary:

                if (!continuedFile.fPermanent)
                {
                    if (m_pcPauseBufferSegment->ContainsFile(continuedFile.strFilename))
                    {
                        m_pcPauseBufferSegment->RevertToTemporary();
                        break;
                    }
                }
            }
        }
    }

    // We might have orphans.  Look for permanent recordings on/after our A/V position
    // that lie wholly outside the contiguous portion of the pause buffer:

    if (!m_pcPauseBufferSegment->IsPermanent() ||
        (m_pcPauseBufferSegment->CurrentEndTime() < 0) ||
        (m_pcPauseBufferSegment->CurrentEndTime() > hyTruncationPoint))
    {
        // We appear to be positioned in a temporary recording or a permanent recording that is partly
        // wholly within the pause buffer window. We don't need to keep any orphans if that is
        // really the case. However, the various portions of the DVR engine are not perfectly
        // synchronized. Double check that the Writer has not already currently ended our
        // permanent recording and dropped it from the new list of recordings + files.

        bool fNotInOrphan = true;
        if (m_pcPauseBufferSegment->IsPermanent())
        {
            fNotInOrphan = false;
            size_t uNewFileCount = pData->GetCount();
            size_t uFileIdx;
            for (uFileIdx = 0; uFileIdx < uNewFileCount; ++uFileIdx)
            {
                SPauseBufferEntry continuedFile = (*pData)[uFileIdx];
                if (continuedFile.fPermanent &&
                    (continuedFile.strRecordingName.compare(m_pcPauseBufferSegment->GetRecordingName()) == 0))
                {
                    fNotInOrphan = true;
                    break;
                }
            }
        }

        if (fNotInOrphan)
        {
#ifdef UNICODE
            DbgLog((LOG_PAUSE_BUFFER, 3,
                _T("CSampleConsumer::MergeWithOrphans() -- exit (%s @ %p: %s, %I64d > %I64d)\n"),
                m_pcPauseBufferSegment->GetRecordingName().c_str(),
                m_pcPauseBufferSegment,
                m_pcPauseBufferSegment->IsPermanent() ? _T("permanent") : _T("temporary"),
                m_pcPauseBufferSegment->CurrentEndTime(),
                hyTruncationPoint));
#else
            DbgLog((LOG_PAUSE_BUFFER, 3,
                _T("CSampleConsumer::MergeWithOrphans() -- exit (%S @ %p: %s, %I64d > %I64d)\n"),
                m_pcPauseBufferSegment->GetRecordingName().c_str(),
                m_pcPauseBufferSegment,
                m_pcPauseBufferSegment->IsPermanent() ? _T("permanent") : _T("temporary"),
                m_pcPauseBufferSegment->CurrentEndTime(),
                hyTruncationPoint));
#endif

            return pData.Detach();
        }

        SPauseBufferEntry firstRetainedFile = (*pData)[0];
        hyTruncationPoint = firstRetainedFile.tStart - 1;

        DbgLog((LOG_PAUSE_BUFFER, 3, _T("CSampleConsumer::MergeWithOrphans() -- not an orphan after all, truncating at %I64d\n"),
            hyTruncationPoint));
    }

    // We've got orphans.  Identify them and add them to the pause buffer.

    DbgLog((LOG_PAUSE_BUFFER, 3, _T("CSampleConsumer::MergeWithOrphans() -- about to merge in orphans ...\n")));

    // Again, hold onto a ref for the new data safely with a smart pointer
    // (in case of exit by exception):

    CSmartRefPtr<CPauseBufferData> pNewData;
    pNewData.Attach(pData->Duplicate());

    std::vector<SampleProducer::CPauseBufferSegment*>::iterator iter;
    bool fFoundStartingPoint = false;
    SampleProducer::CPauseBufferSegment *pcPauseBufferSegmentLatestOrphan = NULL;

    for (iter = m_pcPauseBufferHistory->m_listpcPauseBufferSegments.begin();
         iter != m_pcPauseBufferHistory->m_listpcPauseBufferSegments.end();
         ++iter)
    {
        SampleProducer::CPauseBufferSegment *pcPauseBufferSegment = *iter;

        // Easy checks first -- we skip until we get to our current recording.
        // We break out of the loop as soon as we find something that is
        // partly in the current pause buffer.

        if (pcPauseBufferSegment == m_pcPauseBufferSegment)
        {
            fFoundStartingPoint = true;
            pcPauseBufferSegmentLatestOrphan = m_pcPauseBufferSegment;
        }
        if (!fFoundStartingPoint)
            continue;       // recording before our current recording cannot be orphans

        if (pcPauseBufferSegment->CurrentEndTime() > hyTruncationPoint)
        {
            bool fNotAnOrphan = true;
            if (pcPauseBufferSegment->IsPermanent())
            {
                fNotAnOrphan = false;
                size_t uNewFileCount = pData->GetCount();
                size_t uFileIdx;
                for (uFileIdx = 0; uFileIdx < uNewFileCount; ++uFileIdx)
                {
                    SPauseBufferEntry continuedFile = (*pData)[uFileIdx];
                    if (continuedFile.fPermanent &&
                        (continuedFile.strRecordingName.compare(pcPauseBufferSegment->GetRecordingName()) == 0))
                    {
                        fNotAnOrphan = true;
                        break;
                    }
                }
            }
            if (fNotAnOrphan)
            {
#ifdef UNICODE
                DbgLog((LOG_PAUSE_BUFFER, 3,
                    _T("CSampleConsumer::MergeWithOrphans() -- bailing at %s (%I64d end > %I64d truncation)\n"),
                    pcPauseBufferSegment->GetRecordingName().c_str(),
                    pcPauseBufferSegment->CurrentEndTime(),
                    hyTruncationPoint));
#else
                DbgLog((LOG_PAUSE_BUFFER, 3,
                    _T("CSampleConsumer::MergeWithOrphans() -- bailing at %S (%I64d end > %I64d truncation)\n"),
                    pcPauseBufferSegment->GetRecordingName().c_str(),
                    pcPauseBufferSegment->CurrentEndTime(),
                    hyTruncationPoint));
#endif
                break;
            }
        }

        if (fFoundStartingPoint && pcPauseBufferSegment->IsPermanent())
        {
            if ((pcPauseBufferSegment != m_pcPauseBufferSegment) &&
                (pcPauseBufferSegment != pcPauseBufferSegmentLatestOrphan) &&
                (pcPauseBufferSegment->CurrentStartTime() > pcPauseBufferSegmentLatestOrphan->CurrentEndTime() + 10000000LL))
            {
                // This is a permanent recording outside the pause buffer window but it
                // is not back-to-back with the recording that we are playing (or some recording
                // that is back-to-back).  It is not an orphan that we want to keep:

#ifdef UNICODE
                DbgLog((LOG_PAUSE_BUFFER, 3,
                    _T("CSampleConsumer::MergeWithOrphans() -- discontiguous permanent record %s [%I64d, %I64d] is not an orphan\n"),
                    pcPauseBufferSegment->GetRecordingName().c_str(),
                    pcPauseBufferSegment->CurrentStartTime(),
                    pcPauseBufferSegment->CurrentEndTime()));

#else
                DbgLog((LOG_PAUSE_BUFFER, 3,
                    _T("CSampleConsumer::MergeWithOrphans() -- discontiguous permanent record %S [%I64d, %I64d] is not an orphan\n"),
                    pcPauseBufferSegment->GetRecordingName().c_str(),
                    pcPauseBufferSegment->CurrentStartTime(),
                    pcPauseBufferSegment->CurrentEndTime()));

#endif
            }
            else
            {
                // This is an orphan -- add its file chunks in reverse order:

#ifdef UNICODE
                DbgLog((LOG_PAUSE_BUFFER, 3,
                    _T("CSampleConsumer::MergeWithOrphans() -- marking %s [%I64d, %I64d] as an orphan\n"),
                    pcPauseBufferSegment->GetRecordingName().c_str(),
                    pcPauseBufferSegment->CurrentStartTime(),
                    pcPauseBufferSegment->CurrentEndTime()));

#else
                DbgLog((LOG_PAUSE_BUFFER, 3,
                    _T("CSampleConsumer::MergeWithOrphans() -- marking %S [%I64d, %I64d] as an orphan\n"),
                    pcPauseBufferSegment->GetRecordingName().c_str(),
                    pcPauseBufferSegment->CurrentStartTime(),
                    pcPauseBufferSegment->CurrentEndTime()));

#endif
                pcPauseBufferSegment->m_fIsOrphaned = true;
                pcPauseBufferSegmentLatestOrphan = pcPauseBufferSegment;

                size_t uLastFile = pcPauseBufferSegment->m_uFirstRecordingFile
                    + pcPauseBufferSegment->m_uNumRecordingFiles;
                size_t uIdxFile;
                for (uIdxFile = pcPauseBufferSegment->m_uFirstRecordingFile;
                    uIdxFile < uLastFile;
                    ++uIdxFile)
                {
                    SPauseBufferEntry orphanedFile = (*pcPauseBufferSegment->m_pcPauseBufferData)[uIdxFile];
#ifdef UNICODE
                    DbgLog((LOG_PAUSE_BUFFER, 4,
                        _T("    orphan file %s [%I64d]\n"),
                        orphanedFile.strFilename.c_str(), orphanedFile.tStart));
#else
                    DbgLog((LOG_PAUSE_BUFFER, 4,
                        _T("    orphan file %S [%I64d]\n"),
                        orphanedFile.strFilename.c_str(), orphanedFile.tStart));
#endif
                    pNewData->SortedInsert(orphanedFile);
                }
            }
        }
    }

    return pNewData.Detach();
} // CSampleConsumer::MergeWithOrphans

void CSampleConsumer::SetCurrentPositionToTrueSegmentEnd(
    SampleProducer::CPauseBufferSegment *pcPauseBufferSegment,
    bool fImplicitlyASeek)
{
    LONGLONG hyLivePosition = pcPauseBufferSegment->CurrentEndTime();
    if (pcPauseBufferSegment->m_fInProgress && m_piSampleProducer)
    {
        LONGLONG hyMinPos, hyMaxPos;

        m_piSampleProducer->GetPositionRange(hyMinPos, hyMaxPos);
        if (hyMaxPos >= 0)
        {
            hyLivePosition = hyMaxPos;
            if (hyMinPos >= 0)
            {
                if (hyMinPos + m_hyTargetOffsetFromLive > hyMaxPos)
                    hyLivePosition = hyMinPos;
                else
                {
                    hyLivePosition = hyMaxPos - m_hyTargetOffsetFromLive;
                }
            }
            pcPauseBufferSegment->m_hyEndTime = hyMaxPos;
        }
    }
    if (fImplicitlyASeek)
    {
        switch (m_eSCPositionMode)
        {
        case SC_POSITION_UNKNOWN:
            m_hyCurrentPosition = hyLivePosition;
            if (hyLivePosition >= 0)
            {
                SetPositionMode(SC_POSITION_AWAITING_FIRST_SAMPLE);
                m_hyPostFlushPosition = hyLivePosition;
            }
            CHECK_POSITION(m_hyCurrentPosition);
            break;

        case SC_POSITION_AWAITING_FIRST_SAMPLE:
            m_hyCurrentPosition = hyLivePosition;
            if (hyLivePosition >= 0)
                m_hyPostFlushPosition = hyLivePosition;
            CHECK_POSITION(m_hyCurrentPosition);
            break;

        case SC_POSITION_SEEKING_KEY:
        case SC_POSITION_SEEKING_ANY:
        case SC_POSITION_ALIGNING:
        case SC_POSITION_NORMAL:
        default:
            m_hyCurrentPosition = hyLivePosition;
            m_fSampleMightBeDiscontiguous = true;
            SetPositionMode(SC_POSITION_SEEKING_KEY);
            CHECK_POSITION(m_hyCurrentPosition);
            if (hyLivePosition >= 0)
                m_hyPostFlushPosition = hyLivePosition;
            break;

        case SC_POSITION_AT_STOP_POSITION:
            m_hyCurrentPosition = hyLivePosition;
            m_fSampleMightBeDiscontiguous = true;
            CHECK_POSITION(m_hyCurrentPosition);
            break;

        case SC_POSITION_PENDING_SET_POSITION:
        case SC_POSITION_PENDING_RUN:
            m_fSampleMightBeDiscontiguous = true;
            m_hyTargetPosition = hyLivePosition;
            if (hyLivePosition >= 0)
                m_hyPostFlushPosition = hyLivePosition;
            break;
        }
    }
}

LONGLONG CSampleConsumer::GetMostRecentSampleTime() const
{
    // We are really looking for a clock tied to samples bearing reliable times at the Sample Producer.
    LONGLONG hySampleTime = -1;
    if (m_piSampleProducer && IsLiveOrInProgress())
    {
        hySampleTime = m_piSampleProducer->GetProducerTime();
    }
    return hySampleTime;
}

DWORD CSampleConsumer::GetLoadIncarnation() const
{
    return m_dwLoadIncarnation;
}

void CSampleConsumer::OnPosition(LONGLONG hyTargetPosition, double dblPriorRate, unsigned uPositionNotifyId,
                                long lEventId, long lParam1, long lParam2)
{
    IPlaybackPipelineManager *pippmgr = NULL;

    {
        CAutoLock cAutoLock(&m_cCritSecState);

        pippmgr = m_pippmgr;
        if (!pippmgr || (m_uPositionNotifyId != uPositionNotifyId))
            return;

        LONGLONG hyBoundaryPosition = -1;
        LONGLONG hyMinPos, hyCurPos, hyMaxPos;
        if (SUCCEEDED(GetConsumerPositions(hyMinPos, hyCurPos, hyMaxPos)))
        {
            if (dblPriorRate < 1.0)
            {
                // We automatically reverse direction at the beginning
                // if the rate is < 1.0.

                hyBoundaryPosition = hyMinPos;
                if (!m_pcPauseBufferSegmentBound && m_pcPauseBufferHistory)
                {
                    hyBoundaryPosition = m_pcPauseBufferHistory->GetSafePlaybackStart(GetPauseBufferSafetyMargin(true));
                }
            }
            else
            {
                // We automatically drop down to 1x at the end if the
                // rate is > 1.0.

                hyBoundaryPosition = hyMaxPos;
                if (m_piSampleProducer)
                {
                    hyBoundaryPosition = hyMaxPos - m_hyTargetOffsetFromLive;
                    if (hyBoundaryPosition < hyMinPos)
                        hyBoundaryPosition = hyMinPos;
                }
            }
            if (hyBoundaryPosition != -1)
                hyTargetPosition = hyBoundaryPosition;
        }
        m_hyTargetPosition = hyTargetPosition;

        // Do not use SetPositionMode() here -- we're doing something special:
        m_eSCPositionMode = SC_POSITION_PENDING_NORMAL_RATE;
    }

    HRESULT hr = E_NOTIMPL;
    CDVRSourceFilter &rcBaseFilter = m_pippmgr->GetSourceFilter();
    IStreamBufferMediaSeeking *piStreamBufferMediaSeeking = NULL;

    hr = rcBaseFilter.QueryInterface(__uuidof(IStreamBufferMediaSeeking), (void**) &piStreamBufferMediaSeeking);
    if (SUCCEEDED(hr))
    {
        CComPtr<IStreamBufferMediaSeeking> ccomPtrIStreamBufferMediaSeeking = NULL;
        ccomPtrIStreamBufferMediaSeeking.Attach(piStreamBufferMediaSeeking);

        // Heads up:  we use the combination of rate 0.0 and position mode
        // SC_POSITION_PENDING_NORMAL_RATE as a sentinel that we want to
        // restore the rate. If someone else comes in and tries to change
        // the rate or position for some other reason since we've released
        // the lock, the position mode will have been changed and since 0.0
        // is a normally unsupported rate, the rate change will be rejected
        // (as we wish it to be in that case):
        hr = ccomPtrIStreamBufferMediaSeeking->SetRate(0.0);
    }

    {
        CAutoLock cAutoLock(&m_cCritSecState);

        if (m_piDecoderDriver && lEventId)
            m_piDecoderDriver->IssueNotification(this, lEventId, lParam1, lParam2, true);
    }

    // TODO:  is there anything useful we can do if we can't set the
    // rate to 1.0?  The current state doesn't seem all that bad -- it is
    // effectively paused until someone gives the state machinery a kick.
} // CSampleConsumer::OnPosition

void CSampleConsumer::DoNotifyPauseBuffer(CPauseBufferData *pData)
{
    CAutoLock cAutoLock(&m_cCritSecState);

    CSmartRefPtr<CPauseBufferData> pDataWithOrphans;
    LONGLONG hyBoundRecordingEndPos = -1LL;
    pDataWithOrphans.Attach(MergeWithOrphans(hyBoundRecordingEndPos, pData));

    try {
        FirePauseBufferUpdateEvent(pDataWithOrphans);
    } catch (const std::exception &) {};

    ImportPauseBuffer(pDataWithOrphans, hyBoundRecordingEndPos);
} // CSampleConsumer::DoNotifyPauseBuffer

void CSampleConsumer::ProcessPauseBufferUpdates()
{
    while (1)
    {
        SPendingPauseBufferUpdate sPendingPauseBufferUpdate;

        {
            CAutoLock cAutoLock(&m_cCritSecPauseBuffer);
        
            std::list<SPendingPauseBufferUpdate>::iterator iter;
            iter = m_listPauseBufferUpdates.begin();
            if (iter == m_listPauseBufferUpdates.end())
            {
                // nothing more to do:

                return;
            }
            sPendingPauseBufferUpdate = *iter;
            m_listPauseBufferUpdates.erase(iter);
        }

        ASSERT(sPendingPauseBufferUpdate.pcPauseBufferData != (CPauseBufferData *)NULL);
        DoNotifyPauseBuffer(sPendingPauseBufferUpdate.pcPauseBufferData);
    }
} // CSampleConsumer::ProcessPauseBufferUpdates

