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
// AudioSync.cpp : Defines the entry point for the DLL application.
//

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <DShow.h>
#include <Streams.h>
#include <tchar.h>
#include <amfilter.h>
#include <transfrm.h>
#include <transip.h>
#include <initguid.h>
#include <atlbase.h>
#include <AOvMixer.h>
#include <dvdmedia.h>
#include <DvrInterfaces.h>
#include <AVGlitchEvents.h>

#include "AudioSync.h"

#include <initguid.h>
#include <AudioSyncFrameAdvProp.h>

#define RECOVER_FROM_WEDGED_STOP
#define SAMPLES_BETWEEN_INCREMENTS	10
#define LIVE_OFFSET_CHECK_INTERVAL	30 * 1000	/* check every minutes = 60 seconds = 60,000 ms */

// Thresholds for reporting quality events -- measured in milliseconds:

#define AV_GLITCH_BAD_TIMESTAMP_THRESHOLD	60 * 60 * 1000		/* buggy time-stamp if >= 1 hour off */
#define AV_GLITCH_LIPSYNC_LOST_THRESHOLD	2 * 1000			/* lip sync is totally lost if >= 2 seconds off */
#define AV_GLITCH_LIPSYNC_POOR_THRESHOLD	100					/* lip sync is noticably bad if >= 100 ms off */
#define AV_GLITCH_THRESHOLD_FOR_REPEATED_NOTICE 1000				/* repeat continuing problems every 1 second */
#define AV_NO_GLITCH_THRESHOLD_FOR_REPEATED_NOTICE	10000			/* report all-is-well only once every 10 seconds */

// {50845279-8C98-4e1f-A16A-8C48CE35EC12}
DEFINE_GUID(IID_IStreamBufferPlayback,
0x50845279, 0x8c98, 0x4e1f, 0xa1, 0x6a, 0x8c, 0x48, 0xce, 0x35, 0xec, 0x12);

#define VOLUME_MUTED	-10000

static REFERENCE_TIME g_rtDesiredLeadTimeMillisec;
	// The desired number of milliseconds of lead time between a newly arrived sample's rendering
	// time and the (adjusted) presentation clock.
static double g_rtMuteBeginThresholdRate;
	// If the rate needed to re-sync with the target rendering position is less than
	// or equal to this value, mute audio and play slowly.
static double g_rtMuteEndThresholdRate;
	// Continue muting until the rate needed to re-sync exceeds this value.
static double g_rtMuteThresholdRate;
	// Requests for playback rates this slow or slower warrant muting audio
static double g_rtMinimumAudibleRate;
	// The slowest playback rate to ever be requested of the audio renderer.
static double g_rtVeryHeavyDecimationRate;
	// The threshold at which we not only play at our maximum speed but also start
	// doing very strong decimation of samples.
static double g_rtHeavyDecimationRate;
	// A somewhat lesser but still fast playback rate that is the threshold at
	// which we start doing stronger than normal decimation of samples.
static double g_rtMaximumAudibleRate;
	// The fastest playback rate for which the playback is to be audible
static double g_rtMaximumDesirableRate;
	// The fastest playback rate considered good.  Above this, we will decimate samples.
static unsigned g_uMinimumAcceptableBackPressure;
	// The threshold for deciding whether there is sufficient back pressure to have
	// acceptable control over the playback rate.  This number or fewer free buffers
	// is acceptable.
static unsigned g_ExcessiveSamplesWithoutBackpressure;
	// The number of consecutive samples seen in normal playback mode in which the back pressure
	// is insuffient for adequate playback rate control.
static unsigned g_uSampleDropFrequency;
	// Controls the decimation rate at which samples are dropped.  When the weighted
	// count of samples arriving late reaches this value, one sample is dropped and the
	// count is reset to zero.
static REFERENCE_TIME g_rtInitialOffsetMillisecNormal;
static REFERENCE_TIME g_rtInitialOffsetMillisecModerate;
static REFERENCE_TIME g_rtInitialOffsetMillisecSevere;
	// Between the time that a flush happens and the time that the audio sync filter
	// receives a sample, the video renderer may be displaying samples. If we are in
	// a problematic case, the audio sync filter will receive its first post-flush sample
	// somewhere between half a second and a full second after the first video sample.
	// In this case, the audio sync filter will need to force the long-term offset to
	// be on the order of half a second. It looks really ugly for the video to play at
	// normal speed for that first half-second and the normal video play tends to set up
	// a nasty oscillation when the audio does kick in.  In this scenario, it is important
	// to set up the video renderer with an initial crude estimate of half-a-second lag.
	// In the easy scenarios, audio and video samples arrive about the same time.  So
	// the audio renderer can correct the presumptive half-second lag before the video
	// renderer has a chance to phase it in. This value is the presumptive offset.
static double g_dblMaxAdjust;
	// The rate computation gets more aggressive about speeding up or slowing down the
	// longer the arrival time is outside the desired range.  This is the maximum value
	// for the running longevity-of-problem factor.
static double g_dblMinAdjust;
	// This is the minimum value for the running longevity-of-problem factor.
static double g_dblMillisecToSpreadOutCorrection;
	// How many milliseconds we want to spread out the correction over.
static REFERENCE_TIME g_rtMillisecLagToForceBuilding;
	// If the lag ever reaches this value, go back to state BUILDING
	// even if there is back-pressure.  This is a last-ditch recovery
	// do deal with a serious CPU hog glitch.
static unsigned g_uMinimumSamplesWhileBuilding;
	// The minimum number of samples to process while in state BUILDING
	// before going back to state NORMAL.
static unsigned g_MinimumLateSamplesToForceBuilding;
	// The minimum number of very late samples to process while in
	// state NORMAL before switching to state BUILDING purely to
	// re-float the offset.
static int g_VideoLeadMillisec;
	// The number of milliseconds difference between the target audio
	// offset and the companion video offset.  Add this to the audio
	// offset to obtain the video offset.
static unsigned g_PenaltyForEmptyBuffers;
	// The number of milliseconds to penalize as being extra late
	// when computing the playback speed to put pressure on to fill
	// those buffers.
static int g_AdjustmentWhenFixingOffsetNormal;
	// The number of milliseconds between the bleeding edge of what
	// live tv is spitting out of the MPEG encoder and a safe, long
	// term playback position. This flavor applies for normal encoding
	// modes.
static int g_AdjustmentWhenFixingOffsetSevere;
	// As above, but applies when the jitter is known to be severe.
static int g_AdjustmentWhenFixingOffsetModerate;
	// As above, but applies when the jitter is known to be severe.
static DWORD g_TypicalSampleDuration;
	// The number of milliseconds required to play a typical sample.
static LONGLONG g_rtMinThresholdForIncreasingSafetyMargin = -100;
	// The number of milliseconds late to warrant increasing the
	// safety margin if at live.
static LONGLONG g_rtMaxThresholdForIncreasingSafetyMargin = -400;
	// The number of milliseconds late to cease increasing the
	// safety margin if at live. [If this late or worse, then the
	// problem is not simple instability at live.]

#if 0 // moved to common idl header
// {04AA8285-3C64-4cc8-8C83-326DA71576F0}
DEFINE_GUID(CLSID_AudioSync,
0x4aa8285, 0x3c64, 0x4cc8, 0x8c, 0x83, 0x32, 0x6d, 0xa7, 0x15, 0x76, 0xf0);
#endif
// {C5B1A87A-086F-4aaa-93DC-2648A554E2F7}
DEFINE_GUID(IID_IAudioQA,
    0xC5B1A87A, 0x086F, 0x4aaa, 0x93, 0xDC, 0x26, 0x48, 0xA5, 0x54, 0xE2, 0xF7);

// Setup data
const AMOVIESETUP_MEDIATYPE sudPinTypes =
{
	&MEDIATYPE_Audio,				// Major type
	&MEDIASUBTYPE_NULL				// Minor type
};

const AMOVIESETUP_PIN sudPins[2] =
{
	{
		L"Audio In",                // Pin string name
		FALSE,                      // Is it rendered
		FALSE,                      // Is it an output
		FALSE,                      // Allowed none
		FALSE,                      // Likewise many
		&CLSID_NULL,                // Connects to filter
		L"",						// Connects to pin
		1,                          // Number of types
		&sudPinTypes                // Pin information
	},
	{
		L"Audio Out",               // Pin string name
		FALSE,                      // Is it rendered
		TRUE,                       // Is it an output
		FALSE,                      // Allowed none
		FALSE,                      // Likewise many
		&CLSID_NULL,                // Connects to filter
		L"",						// Connects to pin
		1,                          // Number of types
		&sudPinTypes                // Pin information
	}
};

const AMOVIESETUP_FILTER sudAudioSync =
{
	&CLSID_AudioSync,           // Filter CLSID
	L"Audio Sync Filter",		// String name
	MERIT_DO_NOT_USE,           // Filter merit
	2,                          // Number pins
	sudPins                    // Pin details
};


//
//  Object creation stuff
//
CFactoryTemplate g_Templates[]= {
	L"Audio Sync Filter", &CLSID_AudioSync, AudioSyncFilter::CreateInstance, NULL, &sudAudioSync
};
int g_cTemplates = 1;

//
//  For now, we need retail logging support for this module; delete
//  this section once this is no longer required  -- RB
//
#ifdef ENABLE_DIAGNOSTIC_OUTPUT

#undef DbgLog
#define DbgLog(_x_) RetailMsgConvert _x_

static void RetailMsgConvert(BOOL fCond, DWORD /*Level*/, const TCHAR *pFormat, ...)
{
    va_list va;
    va_start(va, pFormat);

    TCHAR rgchBuf[MAX_DEBUG_STR];
    _vsntprintf (rgchBuf, (sizeof(rgchBuf)/sizeof(rgchBuf[0])), pFormat, va);
    rgchBuf[MAX_DEBUG_STR-1] = '\0';

	if (fCond)
		OutputDebugStringW(rgchBuf);

    va_end(va);
}

#define ZONEID_ERROR		0
#define ZONEID_WARNING		1
#define ZONEID_INFO_MIN		2
#define ZONEID_INFO_MAX		3
#define ZONEID_INIT			4
#define ZONEID_ENTER		5
#define ZONEID_TIMING		6
#define ZONEID_MEMORY		7
#define ZONEID_LOCKING		8
#define ZONEID_MTYPES		9
#define ZONEID_LIPSYNCSTATS	10
#define ZONEID_REFCOUNT		11
#define ZONEID_PERF     	12
#define ZONEID_INFO1		13
#define ZONEID_HEURISTIC	14
#define ZONEID_PAUSEBUG		15

#define ZONEMASK_ERROR		(1 << ZONEID_ERROR)
#define ZONEMASK_WARNING	(1 << ZONEID_WARNING)
#define ZONEMASK_INFO_MIN   (1 << ZONEID_INFO_MIN)
#define ZONEMASK_INFO_MAX   (1 << ZONEID_INFO_MAX)
#define ZONEMASK_INIT		(1 << ZONEID_INIT)
#define ZONEMASK_ENTER		(1 << ZONEID_ENTER)
#define ZONEMASK_TIMING		(1 << ZONEID_TIMING)
#define ZONEMASK_MEMORY		(1 << ZONEID_MEMORY)
#define ZONEMASK_LOCKING	(1 << ZONEID_LOCKING)
#define ZONEMASK_MTYPES		(1 << ZONEID_MTYPES)
#define ZONEMASK_LIPSYNCSTATS	(1 << ZONEID_LIPSYNCSTATS)
#define ZONEMASK_REFCOUNT	(1 << ZONEID_REFCOUNT)
#define ZONEMASK_PERF		(1 << ZONEID_PERF)
#define ZONEMASK_INFO1		(1 << ZONEID_INFO1)
#define ZONEMASK_HEURISTIC	(1 << ZONEID_HEURISTIC)
#define ZONEMASK_PAUSEBUG	(1 << ZONEID_PAUSEBUG)

#define ZONE_LIPSYNCSTATS		DEBUGZONE(ZONEID_LIPSYNCSTATS)
#define ZONE_HEURISTIC			DEBUGZONE(ZONEID_HEURISTIC)
#define ZONE_PAUSEBUG			DEBUGZONE(ZONEID_PAUSEBUG)

#define LOG_LIPSYNCSTATS	ZONE_LIPSYNCSTATS
#define LOG_HEURISTIC		ZONE_HEURISTIC
#define LOG_PAUSEBUG		ZONE_PAUSEBUG

#define ADUIOZONE_NAMES            \
		TEXT("Errors"),			\
		TEXT("Warnings"),		\
		TEXT("Info (min)"),		\
		TEXT("Info (max)"),		\
		TEXT("Initialize"),		\
		TEXT("Enter,Exit"),		\
		TEXT("Timing"),		    \
		TEXT("Memory"), 		\
		TEXT("Locking"), 		\
		TEXT("Media Types"),	\
		TEXT("Lip Sync Stats"),			\
		TEXT("RefCount"),		\
		TEXT("Performance"),	\
		TEXT("Info 1"),	        \
		TEXT("Lip Sync Heuristic"),			\
		TEXT("Pause/Resume")

static const DWORD Mask = 0; // ZONEMASK_ERROR | ZONEMASK_LIPSYNCSTATS | ZONEMASK_HEURISTIC;

DBGPARAM dpCurSettings =
{
	TEXT("audiosync"),
	{
	    ADUIOZONE_NAMES
	},
	Mask
};

#endif  // ENABLE_DIAGNOSTIC_OUTPUT
//  End of retail logging configuration

////////////////////////////////////////////////////////////////////////
//
// Exported entry points for registration and unregistration
// (in this case they only call through to default implementations).
//
////////////////////////////////////////////////////////////////////////

//
// DllRegisterSever
//
// Handle the registration of this filter
//
STDAPI DllRegisterServer()
{
	return AMovieDllRegisterServer2( TRUE );
} // DllRegisterServer


//
// DllUnregisterServer
//
STDAPI DllUnregisterServer()
{
	return AMovieDllRegisterServer2( FALSE );
} // DllUnregisterServer


//
// DllEntryPoint
//
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule,
					DWORD  dwReason,
					LPVOID lpReserved)
{
	if (DLL_PROCESS_ATTACH == dwReason)
	{
		AudioSyncFilter::InitHeuristicParameters();
	}
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}


////////////////////////////////////////////////////////////////////////
//
// Filter implementation:
//
////////////////////////////////////////////////////////////////////////

static inline int GetSafetyMargin(PVR_LIVE_TV_AUDIO_JITTER_CATEGORY eLiveTVJitterCategory)
{
	int iOffset = g_AdjustmentWhenFixingOffsetNormal;

	switch (eLiveTVJitterCategory)
	{
	case PVR_LIVE_TV_AUDIO_JITTER_SEVERE:
		iOffset = g_AdjustmentWhenFixingOffsetSevere;
		break;

	case PVR_LIVE_TV_AUDIO_JITTER_MODERATE:
		iOffset = g_AdjustmentWhenFixingOffsetModerate;
		break;

	case PVR_LIVE_TV_AUDIO_JITTER_NORMAL:
	default:
		iOffset = g_AdjustmentWhenFixingOffsetNormal;
		break;
	}
	return iOffset;
}

static inline REFERENCE_TIME GetInitialSafetyMargin(PVR_LIVE_TV_AUDIO_JITTER_CATEGORY eLiveTVJitterCategory)
{
	REFERENCE_TIME iOffset = g_rtInitialOffsetMillisecNormal;

	switch (eLiveTVJitterCategory)
	{
	case PVR_LIVE_TV_AUDIO_JITTER_SEVERE:
		iOffset = g_rtInitialOffsetMillisecSevere;
		break;

	case PVR_LIVE_TV_AUDIO_JITTER_MODERATE:
		iOffset = g_rtInitialOffsetMillisecModerate;
		break;

	case PVR_LIVE_TV_AUDIO_JITTER_NORMAL:
	default:
		iOffset = g_rtInitialOffsetMillisecNormal;
		break;
	}
	return iOffset;
}

static bool GetRegistryOverride(const TCHAR *pszRegistryProperyName, DWORD &dwValue)
{
	bool fGotValue = false;
	HKEY hkeyAudioSync = NULL;

    if ( ( ERROR_SUCCESS == RegOpenKeyEx(
					HKEY_LOCAL_MACHINE,
					TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\MediaCenter\\MCShell\\AudioSyncFilter"), 0, 0, &hkeyAudioSync ) ) &&
         hkeyAudioSync )
    {
        // Get the desired property:

        DWORD cbData = sizeof( dwValue );
		DWORD dwType = 0;

        if ( ( ERROR_SUCCESS == RegQueryValueEx(
               hkeyAudioSync, pszRegistryProperyName, NULL, &dwType, (PBYTE)&dwValue, &cbData ) ) &&
             (REG_DWORD == dwType) )
        {
			fGotValue = true;
        }
        RegCloseKey( hkeyAudioSync );
    }
	return fGotValue;
} // GetRegistryValue

void AudioSyncFilter::InitHeuristicParameters()
{
	DWORD dwValue;
	
	// This had been set to 230LL, but that would require keeping the buffer pool on average only
	// 1/3 of the time with an empty buffer. That much backpressure was triggering arrival time
	// oscillation -- probably due to sleeping the upstream code while waiting for a buffer.
	if (GetRegistryOverride(TEXT("DesiredLeadTimeMillisec"), dwValue))
		g_rtDesiredLeadTimeMillisec = (REFERENCE_TIME) dwValue;
	else
		g_rtDesiredLeadTimeMillisec = 170LL;

	if (GetRegistryOverride(TEXT("BeginMuteDueToSlowRateThreshold"), dwValue))
		g_rtMuteBeginThresholdRate = ((double) dwValue) / 10000.0;
	else
		g_rtMuteBeginThresholdRate = 0.97;

	if (GetRegistryOverride(TEXT("EndMuteDueToSlowRateThreshold"), dwValue))
		g_rtMuteEndThresholdRate = ((double) dwValue) / 10000.0;
	else
		g_rtMuteEndThresholdRate = 0.99;

	if (GetRegistryOverride(TEXT("MinimumAudiblePlaybackRate"), dwValue))
		g_rtMinimumAudibleRate = ((double) dwValue) / 10000.0;
	else
		g_rtMinimumAudibleRate = 0.98;

	if (GetRegistryOverride(TEXT("SevereSampleDecimationRateThreshold"), dwValue))
		g_rtVeryHeavyDecimationRate = ((double) dwValue) / 10000.0;
	else
		g_rtVeryHeavyDecimationRate = 2.0;

	if (GetRegistryOverride(TEXT("MaximumAudiblePlaybackRate"), dwValue))
		g_rtMaximumAudibleRate = ((double) dwValue) / 10000.0;
	else
		g_rtMaximumAudibleRate = 1.030;

	if (GetRegistryOverride(TEXT("MaximumGoodPlaybackRate"), dwValue))
		g_rtMaximumDesirableRate = ((double) dwValue) / 10000.0;
	else
		g_rtMaximumDesirableRate = 1.005;

	if (GetRegistryOverride(TEXT("ModerateSampleDecimationRateThreshold"), dwValue))
		g_rtHeavyDecimationRate = ((double) dwValue) / 10000.0;
	else
		g_rtHeavyDecimationRate = 1.010;

	if (GetRegistryOverride(TEXT("MinimumAcceptableBackPressure"), dwValue))
		g_uMinimumAcceptableBackPressure = (unsigned) dwValue;
	else
		g_uMinimumAcceptableBackPressure = 3;

	if (GetRegistryOverride(TEXT("MaximumSamplesWithoutBackPressure"), dwValue))
		g_ExcessiveSamplesWithoutBackpressure = (unsigned) dwValue;
	else
		g_ExcessiveSamplesWithoutBackpressure = 30;

	if (GetRegistryOverride(TEXT("SampleDecimationThreshold"), dwValue))
		g_uSampleDropFrequency = (unsigned) dwValue;
	else
		g_uSampleDropFrequency = 2500;

	if (GetRegistryOverride(TEXT("MaximumOngoingTroubleRateAdjust"), dwValue))
		g_dblMaxAdjust = ((double) dwValue) - 1000.0;
	else
		g_dblMaxAdjust = 100.0;

	if (GetRegistryOverride(TEXT("MinimumOngoingTroubleRateAdjust"), dwValue))
		g_dblMinAdjust = ((double) dwValue) - 1000.0;
	else
		g_dblMinAdjust = -50.0;

	if (GetRegistryOverride(TEXT("MillisecToCorrectLeadLag"), dwValue))
		g_dblMillisecToSpreadOutCorrection = ((double) dwValue);
	else
		g_dblMillisecToSpreadOutCorrection = 20000.0;

	if (GetRegistryOverride(TEXT("MillisecLagToForceBuilding"), dwValue))
		g_rtMillisecLagToForceBuilding = -1LL * (REFERENCE_TIME) dwValue;
	else
		g_rtMillisecLagToForceBuilding = -2500;

	if (GetRegistryOverride(TEXT("MinimumSampleCountWhileBuilding"), dwValue))
		g_uMinimumSamplesWhileBuilding = (unsigned) dwValue;
	else
		g_uMinimumSamplesWhileBuilding = 60;

	if (GetRegistryOverride(TEXT("MinimumVeryLateSamplesToForceBuilding"), dwValue))
		g_MinimumLateSamplesToForceBuilding = (unsigned) dwValue;
	else
		g_MinimumLateSamplesToForceBuilding = 30;

	if (GetRegistryOverride(TEXT("AdditionalMillisecVideoLead"), dwValue))
		g_VideoLeadMillisec = ((int) dwValue) - 1000;
	else
		g_VideoLeadMillisec = 0;

	if (GetRegistryOverride(TEXT("MillisecPenaltyPerEmptyBuffer"), dwValue))
		g_PenaltyForEmptyBuffers = (unsigned) dwValue;
	else
		g_PenaltyForEmptyBuffers = 8;

	if (GetRegistryOverride(TEXT("MillisecAdjustWhenFixingNormal"), dwValue))
		g_AdjustmentWhenFixingOffsetNormal = ((int) dwValue) - 1000;
	else
		g_AdjustmentWhenFixingOffsetNormal = 0;

	if (GetRegistryOverride(TEXT("MillisecAdjustWhenFixingSevere"), dwValue))
		g_AdjustmentWhenFixingOffsetSevere = ((int) dwValue) - 1000;
	else
		g_AdjustmentWhenFixingOffsetSevere = 0;

	g_AdjustmentWhenFixingOffsetModerate = (g_AdjustmentWhenFixingOffsetNormal + g_AdjustmentWhenFixingOffsetSevere) /2 ;

	if (GetRegistryOverride(TEXT("MillisecTypicalSamplePlayTime"), dwValue))
		g_TypicalSampleDuration = dwValue;
	else
		g_TypicalSampleDuration = 32;
} // AudioSyncFilter::InitHeuristicParameters

AudioSyncFilter::AudioSyncFilter(TCHAR *pwszFilterName, LPUNKNOWN pUnknown, REFCLSID clsid, HRESULT *pHr)
		: CTransInPlaceFilter(pwszFilterName, pUnknown, clsid, pHr
#ifndef _WIN32_WCE
			, false
#endif
			)
		, m_fMuteDueToSpeed(false)
		, m_dblTargetRate ( 1.0 )
		, m_lVolume ( 1 )  // volume is not set yet]
		, m_dwFlagsToHandleBadResume(BAD_RESUME_FLAG_NEED_SAMPLE_DOWNSTREAM)
		, m_cCritSecBadResumeState()
		, m_hAdviseEvent(TRUE, TRUE)
		, m_hInRecoveryEvent(TRUE, TRUE)
		, m_hBadResumeRecoveryThread(NULL)
		, m_fStartedFromStop(true)
		, m_lInReceiveCount(0)
		, m_dwReceiveEntrySysTicks(0)
		, m_eBackPressureMode(BUILDING_BACKPRESSURE)
		, m_uSamplesWhileBuilding(0)
		, m_uConsecutiveSamplesWithNoBackpressure(0)
		, m_uConsecutiveSamplesToForceBuilding(0)
		, m_rtRenderingOffsetTarget(0)
		, m_rtRenderingOffsetCurrent(0)
		, m_piAlphaMixer(0)
		, m_pRendererPropertySet(NULL)
		, m_piStreamBufferPlayback(0)
		, m_eLiveVersusBufferedState(UNKNOWN_DATA_BUFFERING)
		, m_piMemAllocator2()
		, m_uLastBackPressure(0)
		, m_dblRunningAvgLead(0.0)
		, m_lBufferCount(0)
		, m_dblAvgSampleInterval(0.0)
		, m_rtLastSampleStartTime(0LL)
		, m_dblAdjust(0.0)
		, m_dwSampleDropCountdown(0)
		, m_rtPauseStartStreamTime(0LL)
		, m_iSamplesSinceMuteChange(0)
		, m_uSamplesUntilCanIncrement(0)
		, m_rtOffsetFromLive(-1)
		, m_eAudioSyncFrameStepMode(AUDIO_SYNC_FRAME_STEP_NONE)
		, m_fFirstSampleSinceFlush(true)
		, m_cCritSecTransform()
		, m_dwBadResumeSampleCountdown(0)
		, m_fSleepingInReceive(FALSE)
		, m_dblRenderingRate(1.0)
		, m_eLiveTVJitterCategory(PVR_LIVE_TV_AUDIO_JITTER_NORMAL)
		, m_dwLastOffsetFromLiveCheck(0)
		, m_rtLiveSafetyMargin(g_AdjustmentWhenFixingOffsetNormal)
#ifdef ENABLE_DIAGNOSTIC_OUTPUT
		, m_dwSamplesDelivered ( 0 )
		, m_dwSamplesDropped ( 0 )
		, m_dwSamplesError ( 0 )
		, m_dwPotentiallyDry ( 0 )
		, m_dwFrameStepSamples( 0 )
		, m_dwPausedOrFlushing( 0 )
		, m_dwCritialSamples( 0 )
		, m_llLastDeliveryReport ( 0 )
		, m_llCumulativeDrift (0)
		, m_llCumAdjDrift(0)
		, m_llMaximumDrift(-0x7fffffffffffffffLL)
		, m_llMinimumDrift(0x7fffffffffffffffLL)
		, m_llNumDriftSamples(0)
		, m_uCumulativeBackPressure(0)
		, m_dwMaxBackPressure(0)
		, m_dwMinBackPressure(0xffffffff)
#endif
		, m_piMediaEventSink()
		, m_dwDiscardEventTicks(0)
		, m_dwBadTSEventTicks(0)
		, m_dwNoSyncEventTicks(0)
		, m_dwAudioReceiptEventTicks(0)
{
	ClearTimeDeltas();
#ifdef CTV_ENABLE_TESTABILITY
	InitQAStats();
	OnQAStatEvent(QA_EVENT_CREATE_FILTER);
#endif // CTV_ENABLE_TESTABILITY
} // AudioSyncFilter::AudioSyncFilter()

HRESULT AudioSyncFilter::CheckInputType(const CMediaType* mtIn)
{
	return (mtIn && (mtIn->majortype == MEDIATYPE_Audio)) ? S_OK : S_FALSE;
} // AudioSyncFilter::CheckInputType

CBasePin* AudioSyncFilter::GetPin(int n)
{
	if (! m_pOutput)
	{
		HRESULT hr = S_OK;

		m_pOutput = new CAudioSyncOutputPin(NAME("AudioSync output pin"),
											this,		// owner filter
											&hr,		// result code
											L"Output");	// pin name

		if (! m_pOutput)
			return NULL;
	}
	if (! m_pInput)
	{
		HRESULT hr = S_OK;

		m_pInput = new CAudioSyncInputPin(NAME("AudioSync input pin"),
											this,		// owner filter
											&hr,		// result code
											L"Input");	// pin name

		if (! m_pInput)
			return NULL;
	}

	return CTransInPlaceFilter::GetPin(n);
}

HRESULT AudioSyncFilter::GetAudioRenderer(IBaseFilter** ppiAudioRenderer)
{
	HRESULT hr;
	CComPtr<IPin> ciDownStreamInputPin;

	if (FAILED(hr = OutputPin()->ConnectedTo(&ciDownStreamInputPin)))
		return hr;

	PIN_INFO sPinInfo;
	if (FAILED(hr = ciDownStreamInputPin->QueryPinInfo(&sPinInfo)))
		return hr;

	if (! sPinInfo.pFilter)
		return E_UNEXPECTED;

	*ppiAudioRenderer = sPinInfo.pFilter;
	return S_OK;
}

// AdjustSpeed() updates the audio render's playback rate. It also implements the heuristics
// for deciding when to mute and/or drop samples based on the requested playback rate.
//
// The general theory is to cap the rate if it is outside of the acceptable bounds.  Being
// outside of the acceptable bounds is a good sign that something needs to be done (e.g., mute
// if the playback will be unpleasant or drop samples if playing at an acceptable rate is insufficient
// to catch up).  More detail:
//
//	* if you ask to play more slowly than the mininum rate we're willing to order the renderer to play,
//	  cap at the minimum rate.
//  * if you ask to play more slowly than the threshold for muting audio due to super slow playback
//    rate (i.e., way ahead of the playback curve), mute.
//
//  * if you ask to play more quickly than our threshold for very aggressive sample decimation,
//    cap at the maximum halfway decent playback rate and be very aggressive about decimation of samples
//  * otherwise if you ask to play faster than sounds halfway decent, cap at the maximum rate that sounds
//    halfway decent. Also, if we are not building backpressure, be moderately agressive about decimation.
//  * if you ask to play more quickly than our threshold for moderately aggressive
//	  sample decimation, be more aggressive.
//
//  * if we are not trying to play catchup (i.e., the playback rate is normal or slower than normal),
//    back off on the urgency of decimating another sample.
//
//  Always examine our accumulated measure of the need to decimate a sample against the threshold.  If
//  we're over the threshold and have a least two buffers in the queue, this is a good time to decimate
//  a sample.  If we're over twice the threshold, we'd better decimate a sample even if the queue is empty.
//
//  When doing frame advance, the AOvMixer is allowed to provide smooth video behavior by rendering
//  when convenient. The AOvMixer keeps the audio sync filter informed about what is going on during
//  frame-stepping. The audio sync filter adopts a rendering offset to match what the AOvMixer is doing.
//  Since audio is muted during the frame-step and paused in between, the audio sync filter further
//  acts to both minimize back-pressure that would delay arrival of video during frame-stepping and to
//  ensure smooth resumption of playback. Specifically, it drops late audio and, when resuming playback,
//  waits until the first post-pause-with-frame-stepping sample is on-time. This minimizes the a/v glitch.

HRESULT AudioSyncFilter::AdjustSpeed(double dblTargetRate,
									 unsigned uBackPressure, REFERENCE_TIME rtDelta,
									 bool &fDropRecommended, bool fRestoringToDefaultState)
{
	fDropRecommended = false;

	// Check whether any action needs to be taken
	if (m_dblTargetRate == dblTargetRate)
		return S_OK;

	// Retrieve audio renderer filter
	HRESULT hr;
	CComPtr<IBaseFilter> ciDownstreamAudio;

	hr = GetAudioRenderer(&ciDownstreamAudio);
	if (FAILED(hr))
	{
		DbgLog((LOG_ERROR, 1, TEXT("AudioSyncFilter::AdjustSpeed():  GetAudioRenderer() failed\n") ));
		return hr;
	}

	CComQIPtr<IAudioRenderer> ciAudioRen(ciDownstreamAudio);
	if (! ciAudioRen)
	{
		DbgLog((LOG_ERROR, 1, TEXT("AudioSyncFilter::AdjustSpeed():  QueryInterface IAudioRenderer failed\n") ));
		return E_UNEXPECTED;
	}

	bool fMute = false;
	if (fRestoringToDefaultState)
	{
		m_iSamplesSinceMuteChange = 0;
		m_dwSampleDropCountdown = 0;
	}
	else
	{
		if ((dblTargetRate <= g_rtMuteBeginThresholdRate) ||
			(m_fMuteDueToSpeed && (dblTargetRate < g_rtMuteEndThresholdRate)))
		{
			fMute = true;
			m_iSamplesSinceMuteChange = -3;
			dblTargetRate =
					((double)((m_lBufferCount - uBackPressure) * g_TypicalSampleDuration)) /
					((double)(m_lBufferCount * g_TypicalSampleDuration + rtDelta));
				DbgLog((LOG_TIMING, 1, TEXT("AudioSyncFilter::AdjustSpeed():  speed while muted is %f\n"), dblTargetRate));
		}
		else
		{
			++m_iSamplesSinceMuteChange;
			if (m_iSamplesSinceMuteChange <= 0)
				fMute = true;
		}
	}

	if (dblTargetRate < g_rtMinimumAudibleRate)
	{
		if (!fMute)
			dblTargetRate = g_rtMinimumAudibleRate;
	}
	else if (dblTargetRate > g_rtVeryHeavyDecimationRate)
	{
		dblTargetRate = g_rtMaximumAudibleRate;
		m_dwSampleDropCountdown += 3;
	}
	else if (dblTargetRate > g_rtMaximumAudibleRate)
	{
		if ((m_eBackPressureMode != BUILDING_BACKPRESSURE) &&
			(g_uMinimumAcceptableBackPressure >= uBackPressure))
		{
			dblTargetRate = g_rtMaximumAudibleRate;
			m_dwSampleDropCountdown += 2;
		}
		else
			dblTargetRate = g_rtMaximumAudibleRate;
	}
	if ((dblTargetRate <= 1.0) && (m_dwSampleDropCountdown > 1))
		m_dwSampleDropCountdown -=2;
	if (dblTargetRate > g_rtMaximumDesirableRate)
	{
		if ((m_eBackPressureMode != BUILDING_BACKPRESSURE) &&
			(g_uMinimumAcceptableBackPressure >= uBackPressure))
		{
			++m_dwSampleDropCountdown;
			if (dblTargetRate > g_rtHeavyDecimationRate)
				++m_dwSampleDropCountdown;
		}
		if ((m_lVolume == VOLUME_MUTED) ||
			(m_dwSampleDropCountdown >= 2 * g_uSampleDropFrequency) ||
		    ((m_lBufferCount - uBackPressure > 2) && (m_dwSampleDropCountdown >= g_uSampleDropFrequency)))
		{
			m_dwSampleDropCountdown = 0;
			fDropRecommended = true;
		}
	}

	// Mute volume if speed is below thresholded
	CComQIPtr<IBasicAudio> ciVolumeCtrl(ciAudioRen);

	if ((fMute != m_fMuteDueToSpeed) && !ciVolumeCtrl)
	{
		ASSERT (FALSE);  // very strange - audio will have problems now...
		return S_FALSE;  // still continue and deliver the sample
	}

	// If we're going to crank the rate up to something unpleasant,
	// mute before setting the rate:

	if (fMute && !m_fMuteDueToSpeed)
	{
		ReportAVGlitch(AV_GLITCH_AUDIO_BLOCK, 1 /* muted */);

		CAutoLock cLockVolume(&m_cVolumeLeafLock);

		// If volume has never been set and never been retrieved, then get the
		// volume first, so that the effective setting is not lost
		if (1 == m_lVolume)
		{
			hr = ciVolumeCtrl->get_Volume(&m_lVolume);
			ASSERT(SUCCEEDED(hr));
		}

		DbgLog((LOG_HEURISTIC, 1, TEXT("AudioSyncFilter::AdjustSpeed() ... muting audio for sample %I64d ms early\n"), rtDelta));
		hr = ciVolumeCtrl->put_Volume(VOLUME_MUTED);  // mute
		if (SUCCEEDED(hr))
		{
			m_fMuteDueToSpeed = true;
#ifdef CTV_ENABLE_TESTABILITY
			OnQAStatEvent(QA_EVENT_BEGIN_INTERNAL_MUTE);
#endif // CTV_ENABLE_TESTABILITY
		}
		else
		{
			DbgLog((LOG_ERROR, 1, TEXT("AudioSyncFilter::AdjustSpeed():  put_Volume() failed\n") ));
		}
	}

	// Make the rate adjustment
	hr = ciAudioRen->SetDriftRate(dblTargetRate);
	if (SUCCEEDED(hr))
	{
		m_dblTargetRate = dblTargetRate;

		DbgLog((LOG_TIMING, 2, TEXT("AudioSyncFilter::AdjustSpeed(): %lf\n"), dblTargetRate));
	}
	else
	{
		DbgLog((LOG_TIMING, 2, TEXT("AudioSyncFilter::AdjustSpeed(): failed, hr=0x%x\n"), hr));
	}


	if (!fMute && m_fMuteDueToSpeed)
	{
		ReportAVGlitch(AV_GLITCH_AUDIO_BLOCK, 0 /* unmuted */);

		CAutoLock cLockVolume(&m_cVolumeLeafLock);

		DbgLog((LOG_HEURISTIC, 1, TEXT("AudioSyncFilter::AdjustSpeed() ... unmuting audio\n")));
		hr = ciVolumeCtrl->put_Volume(m_lVolume);  // unmute
		if (SUCCEEDED(hr))
			m_fMuteDueToSpeed = false;
		if (FAILED(hr))
		{
			DbgLog((LOG_ERROR, 1, TEXT("AudioSyncFilter::AdjustSpeed():  put_Volume() failed\n") ));
		}
#ifdef CTV_ENABLE_TESTABILITY
		else
		{
			OnQAStatEvent(QA_EVENT_END_INTERNAL_MUTE);
		}
#endif // CTV_ENABLE_TESTABILITY
	}

	return S_OK;
}

// In Transform(), we strip out the presentation time (IMediaSample::GetTime()/IMediaSample::SetTime())
// so that the audio renderer won't be confused by the fact that we are slaved to a non-audio clock.
//
// We also implement a strategy for regulating the playback to keep as close as we can to a desirable
// rendering time versus the time at which video is rendered (i.e., maintain lip sync). Basically,
// the strategy is to exert backpressure on the upstream filter by making sure that the buffer pool
// stays roughly full and that latest sample's arrival time (time put into the buffer pool) roughly
// matches the current adjusted stream clock plus the time the audio renderer will need to consume
// all of the samples ahead of it in the pool.
//
// The strategy operates in two modes:
//
// "BUILDING":  the buffer pool is not full enough to exert backpressure
// "NORMAL":    the buffer pool has or recently had enough backpressure to exert control
//
// In "NORMAL" mode, if samples are arriving too early we slow the playback rate some and, if things
// are way out of whack, mute audio until the slow playback rate brings samples up-to-par. If samples
// are arriving late, we will speed up playback. If the samples are more than just a little late, we
// will also decimate the incoming samples to help catch up.
//
// NOTE:  because the incoming samples are somewhat bursty, we look at a running average measure of
//        arrival time that adds a lot of hysteresis to the reaction time.  Otherwise, the correction
//        algorithm is prone to get into an over-correcting feedback loop that oscillates out of control.

struct SSleepHelper
{
	SSleepHelper(BOOL &rfInSleep)
		: m_rfInSleep(rfInSleep)
	{
		m_rfInSleep = TRUE;
	}

	~SSleepHelper()
	{
		m_rfInSleep = FALSE;
	}

private:
	BOOL &m_rfInSleep;
};

HRESULT AudioSyncFilter::Transform(IMediaSample *pSample)
{
	REFERENCE_TIME rtSampleStart;
	REFERENCE_TIME rtSampleEnd;
	CRefTime rtStream;
	bool fDropRecommended;
#ifdef ENABLE_DIAGNOSTIC_OUTPUT
	bool fAchievedBackPressure = false;
#endif
	CBaseInputPin *pInputPin = InputPin();
	ASSERT(pInputPin);

	RecheckOffsetFromLive();

	// Extract the presentation time from the sample
	HRESULT hr = pSample->GetTime(&rtSampleStart, &rtSampleEnd);
	if (FAILED(hr))
	{
		DbgLog((LOG_ERROR, 1, TEXT("AudioSyncFilter::Transform():  IMediaSample::GetTime() failed\n") ));
		goto Exit;
	}

	// Maintain a running average of the typical time-span covered by a sample so that
	// we can do computation of how long it will take to drain the already queued samples:
	if (m_rtLastSampleStartTime != 0)
	{
		m_dblAvgSampleInterval = ((double) (rtSampleStart - m_rtLastSampleStartTime)/10000LL);
	}
	m_rtLastSampleStartTime = rtSampleStart;

	if (m_piStreamBufferPlayback)
	{
		// Remove that presentation time from the sample, so the non-slaving capable
		// audio renderer does not get confused, and will render only according to
		// drift rate, and as soon as samples arrive
		hr = pSample->SetTime(NULL, NULL);
		if (FAILED(hr))
		{
			DbgLog((LOG_ERROR, 1, TEXT("AudioSyncFilter::Transform():  IMediaSample::SetTime() failed\n") ));
			goto Exit;
		}
	}

	WaitForSingleObject(m_hInRecoveryEvent, INFINITE);

	// Prepare to compare sample's presentation time to graph time [in a way that is valid
	// even if paused]:

	rtStream = GetStreamTime();
	bool fOkayToClearOnTimeSinceFrameStep = (m_eAudioSyncFrameStepMode == AUDIO_SYNC_FRAME_STEP_WAITING_FOR_ON_TIME_SAMPLE);
		// Don't clear if concurrent with ending frame-step or during frame-step

	if (m_eAudioSyncFrameStepMode != AUDIO_SYNC_FRAME_STEP_NONE)
	{
		ASSERT((m_eAudioSyncFrameStepMode != AUDIO_SYNC_FRAME_STEP_ACTIVE) || (m_lVolume == VOLUME_MUTED));

		REFERENCE_TIME rtCurStream = rtStream;
		REFERENCE_TIME rtDeltaRaw = (rtSampleStart + m_rtRenderingOffsetCurrent * 10000LL - rtStream);
		REFERENCE_TIME rtDeltaForHeuristic = (rtSampleStart - rtCurStream) / 10000LL 
					- g_rtDesiredLeadTimeMillisec
					+ m_rtRenderingOffsetCurrent;

		if ((NORMAL_BACKPRESSURE == m_eBackPressureMode) &&
			((rtDeltaForHeuristic > AV_GLITCH_LIPSYNC_POOR_THRESHOLD) ||
			 (rtDeltaForHeuristic < - AV_GLITCH_LIPSYNC_POOR_THRESHOLD)))
		{
			 if ((rtDeltaForHeuristic > AV_GLITCH_BAD_TIMESTAMP_THRESHOLD) ||
				 (rtDeltaForHeuristic < - AV_GLITCH_BAD_TIMESTAMP_THRESHOLD))
			 {
				 ReportAVGlitch(AV_GLITCH_AUDIO_BAD_TIMESTAMP, (long) rtDeltaForHeuristic);
			 }
			 else if ((rtDeltaForHeuristic > AV_GLITCH_LIPSYNC_LOST_THRESHOLD) ||
					  (rtDeltaForHeuristic < - AV_GLITCH_LIPSYNC_LOST_THRESHOLD))
			 {
				 ReportAVGlitch(AV_GLITCH_AUDIO_NO_LIPSYNC, (long) rtDeltaForHeuristic);
			 }
			 else
			 {
				 ReportAVGlitch(AV_GLITCH_AUDIO_LIPSYNC_DRIFT, (long) rtDeltaForHeuristic);
			 }
		}

		if ((rtDeltaRaw >= 0) || (rtDeltaForHeuristic >= 0))
		{
			// We are doing or trying to recover after frame stepping.  Since audio is supposed to be
			// muted while frame stepping, we don't have to worry about creating an audio glitch
			// if we are a little slow pushing samples downstream. We want to avoid messing up
			// the video by changing the rendering offset or holding up decoding by rendering
			// stale audio. We also don't want to run ahead and push audio downstream early
			// because then we won't be able to play it when we need it for correct lip sync.
			// So we fall back to a simple heuristic:  drop stale samples and delay sending down
			// (non-critical) early samples:

			if ((rtDeltaRaw > 0) && (rtDeltaForHeuristic > g_TypicalSampleDuration) &&
			    (m_State != State_Stopped) &&
				m_fGivenSampleSinceStop &&
				!(m_dwFlagsToHandleBadResume & BAD_RESUME_FLAG_NEED_SAMPLE_DOWNSTREAM) &&
				(!pInputPin || !pInputPin->IsFlushing()))
			{

				DbgLog((LOG_HEURISTIC, 3, TEXT("AudioSyncFilter::Transform() -- will sleep until frame-advance or post-frame-advance sample is on-time\n") ));
				SSleepHelper sSleepHelper(m_fSleepingInReceive);

				while ((rtDeltaRaw > 0) && (rtDeltaForHeuristic > g_TypicalSampleDuration) &&
					   (m_State != State_Stopped) &&
					   m_fGivenSampleSinceStop &&
					   !(m_dwFlagsToHandleBadResume & BAD_RESUME_FLAG_NEED_SAMPLE_DOWNSTREAM) &&
					   (!pInputPin || !pInputPin->IsFlushing()))
				{
					// Hold up this sample until it is not ridiculously early:
					m_dwReceiveEntrySysTicks = GetTickCount();
					Sleep(16);
					rtCurStream = GetStreamTime();
					rtDeltaForHeuristic = (rtSampleStart - rtCurStream) / 10000LL
									- g_rtDesiredLeadTimeMillisec
									+ m_rtRenderingOffsetCurrent;
					rtDeltaRaw = (rtSampleStart + m_rtRenderingOffsetCurrent - rtCurStream);
				}
				DbgLog((LOG_HEURISTIC, 3, TEXT("AudioSyncFilter::Transform() -- done sleeping until sample is on-time\n") ));

				WaitForSingleObject(m_hInRecoveryEvent, INFINITE);
			}

			if (m_eAudioSyncFrameStepMode != AUDIO_SYNC_FRAME_STEP_NONE)
			{
				hr = S_OK;
	#ifdef ENABLE_DIAGNOSTIC_OUTPUT
				++m_dwFrameStepSamples;
				AddLipSyncDataPoint(rtSampleStart, rtCurStream, MeasureBackPressure());
	#endif // ENABLE_DIAGNOSTIC_OUTPUT
				goto Exit;
			}
			// else fall through, clear !m_eAudioSyncFrameStepMode and re-establish lip sync
		}
		else if (!m_fGivenSampleSinceStop ||
				 (m_dwFlagsToHandleBadResume & BAD_RESUME_FLAG_NEED_SAMPLE_DOWNSTREAM))
		{
			// We're handling the special case of samples delivered during or immediately
			// after a frame-advance.  Since we need a sample downstream asap, we'll send this one
			// on down. However,

			DbgLog((LOG_HEURISTIC, 2, TEXT("AudioSyncFilter::Transform(): delivering a needed late audio sample during frame-advance period\n")));
			hr = S_OK;
			fOkayToClearOnTimeSinceFrameStep = false;
	#ifdef ENABLE_DIAGNOSTIC_OUTPUT
			++m_dwCritialSamples;
			AddLipSyncDataPoint(rtSampleStart, rtStream, MeasureBackPressure());
	#endif // ENABLE_DIAGNOSTIC_OUTPUT
			goto Exit;
		}
		else
		{
			// We're running late and since frame-advance is either muting audio or introduced an audio
			// gap that has not yet ended, we are not going to be introducing an audio glitch by
			// dropping this late sample.

#ifdef ENABLE_DIAGNOSTIC_OUTPUT
			++m_dwSamplesDropped;
#endif

			DbgLog((LOG_HEURISTIC, 2, TEXT("AudioSyncFilter::Transform(): dropping late audio sample during frame-advance period\n")));
			hr = E_FAIL;
			fOkayToClearOnTimeSinceFrameStep = false;
			goto Report;
		}
	}

	ASSERT(m_eAudioSyncFrameStepMode == AUDIO_SYNC_FRAME_STEP_NONE);

	REFERENCE_TIME rtDelta = 0;

	{
		if (m_fFirstSampleSinceFlush)
		{
			REFERENCE_TIME rtDeltaRaw = (rtSampleStart + m_rtRenderingOffsetCurrent - rtStream);
			REFERENCE_TIME rtDeltaForHeuristic = (rtSampleStart - rtStream) / 10000LL - g_rtDesiredLeadTimeMillisec;
			rtDeltaForHeuristic += + m_rtRenderingOffsetCurrent;

			// We just flushed. That means that there is no particular hurry to get
			// early audio downstream. Consider waiting instead ...

			if ((rtDeltaRaw > 0) && (rtDeltaForHeuristic > g_TypicalSampleDuration) &&
				(m_State != State_Stopped) &&
				m_fGivenSampleSinceStop &&
				!(m_dwFlagsToHandleBadResume & BAD_RESUME_FLAG_NEED_SAMPLE_DOWNSTREAM) &&
				(!pInputPin || !pInputPin->IsFlushing()))
			{
				DbgLog((LOG_HEURISTIC, 3, TEXT("AudioSyncFilter::Transform() -- will sleep until post-flush sample is on-time\n") ));
				SSleepHelper sSleepHelper(m_fSleepingInReceive);

				while ((rtDeltaRaw > 0) && (rtDeltaForHeuristic > g_TypicalSampleDuration) &&
						(m_State != State_Stopped) &&
						m_fGivenSampleSinceStop &&
						!(m_dwFlagsToHandleBadResume & BAD_RESUME_FLAG_NEED_SAMPLE_DOWNSTREAM) &&
						(!pInputPin || !pInputPin->IsFlushing()))
				{
					// Hold up this sample until it is not ridiculously early:
					m_dwReceiveEntrySysTicks = GetTickCount();
					Sleep(16);
					rtStream = GetStreamTime();
					rtDeltaForHeuristic = (rtSampleStart - rtStream) / 10000LL - g_rtDesiredLeadTimeMillisec;
					rtDeltaForHeuristic += + m_rtRenderingOffsetCurrent;
					rtDeltaRaw = (rtSampleStart + m_rtRenderingOffsetCurrent - rtStream);
				}
				DbgLog((LOG_HEURISTIC, 3, TEXT("AudioSyncFilter::Transform() -- done sleeping until post-flush sample is on-time\n") ));

				WaitForSingleObject(m_hInRecoveryEvent, INFINITE);
			}
		}

		if ((m_State != State_Running) ||
			(pInputPin && pInputPin->IsFlushing()))
		{
			hr = S_OK;
	#ifdef ENABLE_DIAGNOSTIC_OUTPUT
			++m_dwPausedOrFlushing;
			AddLipSyncDataPoint(rtSampleStart, rtStream, MeasureBackPressure());
	#endif // ENABLE_DIAGNOSTIC_OUTPUT
			goto Exit;
		}

		// Compute observed delta from the target, in milliseconds. Also find out how many
		// buffers are empty (our back pressure measure is the number of empty buffers):

		unsigned uBackPressureMeasure = MeasureBackPressure();
		rtDelta = (rtSampleStart - rtStream) / 10000LL - g_rtDesiredLeadTimeMillisec;

	#ifdef CTV_ENABLE_TESTABILITY
		if (!m_fFirstSampleSinceFlush)
		{
			if (((long)uBackPressureMeasure) >= m_lBufferCount - 1)
			{
				OnQAStatEvent(QA_EVENT_BUFFERS_RAN_DRY);
			}
		}
	#endif // CTV_ENABLE_TESTABILITY
#ifdef ENABLE_DIAGNOSTIC_OUTPUT
		if (!m_fFirstSampleSinceFlush)
		{
			if (((long) uBackPressureMeasure) >= m_lBufferCount - 1)
			{
				++m_dwPotentiallyDry;
			}
		}
#endif // ENABLE_DIAGNOSTIC_OUTPUT

	#ifdef ENABLE_DIAGNOSTIC_OUTPUT
		AddLipSyncDataPoint(rtSampleStart, rtStream, uBackPressureMeasure);
	#endif // ENABLE_DIAGNOSTIC_OUTPUT

		DbgLog((LOG_TIMING, 2,
			TEXT("AudioSyncFilter::Transform():  %I64d lead [sample %I64d - stream %I64d ms - %I64d ms fudge + offset %I64d of %I64d], %u empty buffers\n"),
			rtDelta + m_rtRenderingOffsetCurrent,
			rtSampleStart/ 10000LL, rtStream / 10000LL,
			g_rtDesiredLeadTimeMillisec, m_rtRenderingOffsetCurrent, m_rtRenderingOffsetTarget,
			uBackPressureMeasure ));

		if (!m_piStreamBufferPlayback)
		{
			hr = S_OK;
			goto Exit;
		}

		// Update our policy for what mode we are in -- build backpressure or normal operation:

		UpdateBackPressurePolicy(uBackPressureMeasure, rtDelta);

		// Update our running average of how far we are deviating from our desired lead time
		// (to smooth out the bursty nature of arrivals):

		m_dblRunningAvgLead = ((double) rtDelta);

	#ifdef ENABLE_DIAGNOSTIC_OUTPUT
		{
			m_uCumulativeBackPressure += uBackPressureMeasure;
			if (uBackPressureMeasure > m_dwMaxBackPressure)
				m_dwMaxBackPressure = uBackPressureMeasure;
			if (uBackPressureMeasure < m_dwMinBackPressure)
				m_dwMinBackPressure = uBackPressureMeasure;
		}
	#endif // ENABLE_DIAGNOSTIC_OUTPUT

		if (m_eBackPressureMode == BUILDING_BACKPRESSURE)
		{
			// If we are playing slowly when samples are arriving late, we'll
			// need to coordinate with the AOvMixer to let it know to play
			// video late to match:

			if ((KNOWN_TO_BE_LIVE_DATA == m_eLiveVersusBufferedState) && (m_rtOffsetFromLive >= 0))
			{
				// It seems as if we're getting way too close to live
				// during the start-up phase, esp. for good quality.
				// Maybe we should re-check the distance from live?
				RecheckOffsetFromLive(true);
			}

			hr = ComputeVideoOffset(uBackPressureMeasure, rtDelta);
			if (FAILED(hr))
			{
				DbgLog((LOG_ERROR, 1, TEXT("AudioSyncFilter::Transform():  ComputeVideoOffset failed\n") ));
			}

			// There is not enough backpressure on the buffers yet to enable us
			// to control lip sync. We're going to take measures to increase
			// how quickly the back pressure builds.

			double dblTargetSpeed = ComputeTargetSpeed(rtDelta, uBackPressureMeasure);
			hr = AdjustSpeed(dblTargetSpeed, uBackPressureMeasure, rtDelta + m_rtRenderingOffsetCurrent, fDropRecommended);
			if (FAILED(hr))
			{
				DbgLog((LOG_ERROR, 1, TEXT("AudioSyncFilter::Transform() [2]:  AdjustSpeed failed\n") ));
			}
		}
		else
		{
			// We have enough backpressure to enable us to control lip sync.
			// In steady state operation, we will alter the playback drift
			// rate in order to maintain roughly the optimal lead time.  If
			// lip sync edges way out of control, we may mute (if way early)
			// or discard (way late).

			if (m_eBackPressureMode == ACHIEVED_BACKPRESSURE)
			{
				// We just achieved normal backpressure.  Set the clock versus
				// actual playback time offset that we'll use for as long as
				// we can maintain normal mode:

	#ifdef ENABLE_DIAGNOSTIC_OUTPUT
				fAchievedBackPressure = true;
	#endif
				hr = FixVideoOffset(rtDelta);
				m_eBackPressureMode = NORMAL_BACKPRESSURE;
				if (FAILED(hr))
				{
					DbgLog((LOG_ERROR, 1, TEXT("AudioSyncFilter::Transform():  FixVideoOffset failed\n") ));
				}
			}
			else
				SetRenderingOffset(m_rtRenderingOffsetCurrent, true);

			// Feed the current lag and back pressure into our heuristic for figuring
			// out how fast we need to play to keep in sync. Then adjust our playback
			// speed. As a side-effect, mute and decide whether it is wise to drop a
			// sample.
			double dblTargetSpeed = ComputeTargetSpeed(rtDelta, uBackPressureMeasure);
			REFERENCE_TIME rtPositionDelta = GetWeightedTimeDelta() + m_rtRenderingOffsetCurrent;

			if ((rtPositionDelta <= g_rtMinThresholdForIncreasingSafetyMargin) &&
				(rtPositionDelta >= g_rtMaxThresholdForIncreasingSafetyMargin) &&
				(KNOWN_TO_BE_LIVE_DATA == m_eLiveVersusBufferedState)
				&& (m_rtRenderingOffsetTarget == m_rtRenderingOffsetCurrent))
			{
				if (m_uSamplesUntilCanIncrement > 0)
					--m_uSamplesUntilCanIncrement;
				if (m_uSamplesUntilCanIncrement == 0)
				{
					LONGLONG rtOldTarget = m_rtRenderingOffsetTarget;
					RecheckOffsetFromLive(true);
					if (rtOldTarget != m_rtRenderingOffsetTarget)
					{
						DbgLog((LOG_HEURISTIC, 1, TEXT("AudioSyncFilter::Transform():  updated offset from live in lieu of updating safety margin\n") ));
					}
					else
					{
						DbgLog((LOG_HEURISTIC, 1, TEXT("AudioSyncFilter::Transform():  increasing safety margin from %I64d to %I64d\n"),
								m_rtLiveSafetyMargin, m_rtLiveSafetyMargin + 1));

						++m_rtLiveSafetyMargin;
					}
				}
			}
			hr = AdjustSpeed(dblTargetSpeed, uBackPressureMeasure, rtPositionDelta, fDropRecommended);
			if (FAILED(hr))
			{
				DbgLog((LOG_ERROR, 1, TEXT("AudioSyncFilter::Transform() [12]:  AdjustSpeed failed\n") ));
			}

			if (fDropRecommended)
			{
				// Signal to drop this sample because we're running late
	#ifdef ENABLE_DIAGNOSTIC_OUTPUT
				++m_dwSamplesDropped;
	#endif

				ReportAVGlitch(AV_GLITCH_AUDIO_DISCARD, 1);
				DbgLog((LOG_TIMING, 2, TEXT("AudioSyncFilter::Transform(): dropping audio sample\n")));
				hr = E_FAIL;
				goto Report;
			}
	}
	}

Exit:
	if (SUCCEEDED(hr) && pInputPin && pInputPin->IsFlushing())
		hr = E_FAIL;
	if (FAILED(hr))
	{
#ifdef ENABLE_DIAGNOSTIC_OUTPUT
		++m_dwSamplesDropped;
		++m_dwSamplesError;
#		endif
		ReportAVGlitch(AV_GLITCH_AUDIO_DISCARD, 1);
		DbgLog((LOG_ERROR, 1, TEXT("AudioSyncFilter::Transform(): unexpected error: 0x%X"), hr));
	}
#ifdef ENABLE_DIAGNOSTIC_OUTPUT
	else
	{
		++m_dwSamplesDelivered;
	}
#	endif

Report:
#ifdef ENABLE_DIAGNOSTIC_OUTPUT
	// Report on the number of samples pushed downstream
	const LONGLONG llCurTicks = (LONGLONG) GetTickCount();
	if ((m_llLastDeliveryReport > llCurTicks) || (llCurTicks >= m_llLastDeliveryReport + 10000LL) ||
		fAchievedBackPressure)
	{
		ReportLipSyncStats(TEXT("Transform"));
		m_llLastDeliveryReport = llCurTicks;
	}
#endif // ENABLE_DIAGNOSTIC_OUTPUT

#ifdef CTV_ENABLE_TESTABILITY
	if (m_dwQAStatsInterval)
		CollectQAStats(hr, rtDelta);
#endif // CTV_ENABLE_TESTABILITY

	if (SUCCEEDED(hr))
	{
		if (fOkayToClearOnTimeSinceFrameStep)
		{
			CAutoLock cTransformLock(&m_cCritSecTransform);

			if (m_eAudioSyncFrameStepMode == AUDIO_SYNC_FRAME_STEP_WAITING_FOR_ON_TIME_SAMPLE)
			{
				m_eAudioSyncFrameStepMode = AUDIO_SYNC_FRAME_STEP_NONE;
				if (m_eBackPressureMode == BUILDING_BACKPRESSURE)
				{
					DbgLog((LOG_HEURISTIC, 3, TEXT("AudioSyncFilter::Transform() -- recovering from frame-step, assuming normal backpressure\n") ));
					m_eBackPressureMode = NORMAL_BACKPRESSURE;
				}
			}
		}
		m_fGivenSampleSinceStop = true;
		m_fFirstSampleSinceFlush = false;
	}
	return hr;
} // AudioSyncFilter::Transform

STDMETHODIMP AudioSyncFilter::NonDelegatingQueryInterface(REFIID riid, LPVOID* ppv)
{
	if (IID_IBasicAudio == riid)
		return m_cBasicAudioImpl.NonDelegatingQueryInterface(riid, ppv);
#ifdef CTV_ENABLE_TESTABILITY
	else if (IID_IAudioQA == riid)
		return GetInterface((IAudioQA *) this, ppv);
#endif // CTV_ENABLE_TESTABILITY
	else if (riid == IID_IKsPropertySet )
    {
        return GetInterface((IKsPropertySet *) this, ppv);
    }

	return CTransInPlaceFilter::NonDelegatingQueryInterface(riid, ppv);
} // AudioSyncFilter::NonDelegatingQueryInterface

CUnknown* WINAPI AudioSyncFilter::CreateInstance(LPUNKNOWN pUnknown, HRESULT *pHr)
{
	HRESULT hr;
	if (!pHr)
		pHr = &hr;

	AudioSyncFilter *pNewObject = new AudioSyncFilter(TEXT("Audio Sync Filter"), pUnknown, CLSID_AudioSync, pHr);
	if (pNewObject == NULL)
		*pHr = E_OUTOFMEMORY;

	return pNewObject;
} // AudioSyncFilter::CreateInstance

HRESULT AudioSyncFilter::get_Volume(long* plVolume)
{
	if (! plVolume)
		return E_POINTER;

	CAutoLock cLockVolume(&m_cVolumeLeafLock);

	// If volume has never been set and never been retrieved, then get it now
	if (1 == m_lVolume)
	{
		HRESULT hr;
		CComPtr<IBaseFilter> ciDownstreamAudio;

		hr = GetAudioRenderer(&ciDownstreamAudio);
		if (FAILED(hr))
			return hr;

		CComQIPtr<IBasicAudio> ciAudioRen(ciDownstreamAudio);
		if (! ciAudioRen)
			return E_UNEXPECTED;

		hr = ciAudioRen->get_Volume(&m_lVolume);
		if (FAILED(hr))
			return hr;
	}

	// Retrieve from internally saved setting, which is different from actual
	// setting if internal muting is in effect
	*plVolume = m_lVolume;
	return S_OK;
} // AudioSyncFilter::get_Volume

HRESULT AudioSyncFilter::put_Volume(long lVolume)
{
	if (lVolume > 0 || lVolume < VOLUME_MUTED)
		return E_INVALIDARG;

	ReportAVGlitch(AV_GLITCH_AUDIO_MUTE, (VOLUME_MUTED == lVolume) ? 1 : 0);

	CAutoLock cLockVolume(&m_cVolumeLeafLock);

	// Attempt to pass through only when internal muting is not in effect
	HRESULT hr = S_OK;

	if (!m_fMuteDueToSpeed)
	{
		CComPtr<IBaseFilter> ciDownstreamAudio;

		hr = GetAudioRenderer(&ciDownstreamAudio);
		if (FAILED(hr))
			return hr;

		CComQIPtr<IBasicAudio> ciAudioRen(ciDownstreamAudio);
		if (! ciAudioRen)
			return E_UNEXPECTED;

		hr = ciAudioRen->put_Volume(lVolume);
	}

	// If it succeeded or if we are muted, then save the value
	if (SUCCEEDED(hr))
	{
#ifdef CTV_ENABLE_TESTABILITY
		if ((lVolume == VOLUME_MUTED) && (m_lVolume != VOLUME_MUTED))
			OnQAStatEvent(QA_EVENT_BEGIN_EXTERNAL_MUTE);
		else if ((m_lVolume == VOLUME_MUTED) && (lVolume != VOLUME_MUTED))
			OnQAStatEvent(QA_EVENT_END_EXTERNAL_MUTE);
#endif // CTV_ENABLE_TESTABILITY


		m_lVolume = lVolume;
	}

	return hr;
} // AudioSyncFilter::put_Volume

STDMETHODIMP AudioSyncFilter::CBasicAudioImpl::get_Volume(long* plVolume)
{
	return static_cast <AudioSyncFilter*>
		   (static_cast <IBaseFilter*> (GetOwner()))->get_Volume(plVolume);
}

STDMETHODIMP AudioSyncFilter::CBasicAudioImpl::put_Volume(long lVolume)
{
	return static_cast <AudioSyncFilter*>
		   (static_cast <IBaseFilter*> (GetOwner()))->put_Volume(lVolume);
}

STDMETHODIMP AudioSyncFilter::CBasicAudioImpl::get_Balance(long* /*plBalance*/)
{
	return E_NOTIMPL;  // must be called on the filter graph manager
} // AudioSyncFilter::get_Balance

STDMETHODIMP AudioSyncFilter::CBasicAudioImpl::put_Balance(long /*lBalance*/)
{
	return E_NOTIMPL;  // must be called on the filter graph manager
} // AudioSyncFilter::put_Balance

AudioSyncFilter::CBasicAudioImpl::CBasicAudioImpl() :
	CBasicAudio ( TEXT("AudioSync Basic Audio"),
				  static_cast <IBaseFilter*> (reinterpret_cast <AudioSyncFilter*>
				  (reinterpret_cast <char*> (this) - offsetof(AudioSyncFilter, m_cBasicAudioImpl))) )
{
}

HRESULT AudioSyncFilter::Receive(IMediaSample *pSample)
{
	ReportAVGlitch(AV_NO_GLITCH_AUDIO_IS_ARRIVING, 0);

	HRESULT hr = CarryOutReceive(pSample);
	if (SUCCEEDED(hr))
	{
		CAutoLock cStateLock(&m_cCritSecBadResumeState);

		if (m_dwFlagsToHandleBadResume & BAD_RESUME_FLAG_NEED_SAMPLE_DOWNSTREAM)
		{
			DbgLog((LOG_PAUSEBUG, 3, TEXT("AudioSyncFilter::Receive():  sending first sample since running\r\n") ));
		}
		m_dwFlagsToHandleBadResume &= ~BAD_RESUME_FLAG_NEED_SAMPLE_DOWNSTREAM;
		if (0 != m_dwBadResumeSampleCountdown)
			--m_dwBadResumeSampleCountdown;
	}
	return hr;
} // AudioSyncFilter::Receive

HRESULT AudioSyncFilter::EndOfStream(void)
{
	CComPtr<IMediaSample> pSampleToDeliver;

	HRESULT hr = CTransInPlaceFilter::EndOfStream();
	{
		CAutoLock cStateLock(&m_cCritSecBadResumeState);

		m_dwFlagsToHandleBadResume |= BAD_RESUME_FLAG_NEED_SAMPLE_DOWNSTREAM;
	}
	return hr;
}

HRESULT AudioSyncFilter::BeginFlush(void)
{
	DbgLog((LOG_PAUSEBUG, 3, TEXT("AudioSyncFilter::BeginFlush() entry\r\n") ));
	HRESULT hr = CTransInPlaceFilter::BeginFlush();

#ifdef CTV_ENABLE_TESTABILITY
	OnQAStatEvent(QA_EVENT_BEGIN_FLUSH);
#endif // CTV_ENABLE_TESTABILITY

	{
		CAutoLock cStateLock(&m_cCritSecBadResumeState);

		m_dwFlagsToHandleBadResume |= BAD_RESUME_FLAG_NEED_SAMPLE_DOWNSTREAM;
		m_eBackPressureMode = BUILDING_BACKPRESSURE;
		m_uConsecutiveSamplesWithNoBackpressure = 0;
		m_uConsecutiveSamplesToForceBuilding = 0;
		m_uSamplesWhileBuilding = 0;
		m_eLiveVersusBufferedState = UNKNOWN_DATA_BUFFERING;
		m_rtOffsetFromLive = -1;
		m_rtRenderingOffsetCurrent = 0;
		m_rtRenderingOffsetTarget = 0;
		SetRenderingOffset(0);
		m_uLastBackPressure = 0;
		m_fFirstSampleSinceFlush = true;
		m_eAudioSyncFrameStepMode = AUDIO_SYNC_FRAME_STEP_NONE;
		m_rtLiveSafetyMargin = GetSafetyMargin(m_eLiveTVJitterCategory);
	}
#ifdef ENABLE_DIAGNOSTIC_OUTPUT
	ReportLipSyncStats(TEXT("BeginFlush"));
#endif // ENABLE_DIAGNOSTIC_OUTPUT
	DbgLog((LOG_PAUSEBUG, 3, TEXT("AudioSyncFilter::BeginFlush() exit\r\n") ));
	return hr;
}

HRESULT AudioSyncFilter::EndFlush(void)
{
	DbgLog((LOG_PAUSEBUG, 3, TEXT("AudioSyncFilter::EndFlush() entry\r\n") ));

#ifdef CTV_ENABLE_TESTABILITY
	OnQAStatEvent(QA_EVENT_END_FLUSH);
#endif // CTV_ENABLE_TESTABILITY

	{
		CAutoLock cStateLock(&m_cCritSecBadResumeState);
		m_dwFlagsToHandleBadResume |= BAD_RESUME_FLAG_NEED_SAMPLE_DOWNSTREAM;
		m_eBackPressureMode = BUILDING_BACKPRESSURE;
		m_eLiveVersusBufferedState = UNKNOWN_DATA_BUFFERING;
		m_rtOffsetFromLive = -1;
		m_uSamplesWhileBuilding = 0;
		m_uConsecutiveSamplesWithNoBackpressure = 0;
		m_uConsecutiveSamplesToForceBuilding = 0;
		m_uLastBackPressure = 0;
		ClearTimeDeltas();
		m_eAudioSyncFrameStepMode = AUDIO_SYNC_FRAME_STEP_NONE;

		DetermineIfBuffered();
		m_rtRenderingOffsetCurrent = 0;
		m_rtRenderingOffsetTarget = 0;
		SetRenderingOffset(GetRecommendedOffset());
		m_fFirstSampleSinceFlush = true;
		m_rtLiveSafetyMargin = GetSafetyMargin(m_eLiveTVJitterCategory);
	}
#ifdef ENABLE_DIAGNOSTIC_OUTPUT
	ReportLipSyncStats(TEXT("BeginFlush"));
#endif // ENABLE_DIAGNOSTIC_OUTPUT
	DbgLog((LOG_PAUSEBUG, 3, TEXT("AudioSyncFilter::EndFlush() exit\r\n") ));
	return CTransInPlaceFilter::EndFlush();
}

STDMETHODIMP AudioSyncFilter::Run(REFERENCE_TIME tStart)
{
	DbgLog((LOG_HEURISTIC, 3, TEXT("AudioSyncFilter::Run(%I64d) entry\n"), tStart ));

#ifdef CTV_ENABLE_TESTABILITY
	OnQAStatEvent(QA_EVENT_RUN_FILTER);
#endif // CTV_ENABLE_TESTABILITY

	HRESULT hr;
#ifndef RECOVER_FROM_WEDGED_STOP
	if (!m_fStartedFromStop)
#endif // !RECOVER_FROM_WEDGED_STOP
	{
		hr = CreateBadResumeRecoveryThread();
		if (FAILED(hr))
			return hr;
	}

	if (!m_piAlphaMixer  &&  !m_pRendererPropertySet)
	{
		CacheAlphaMixer();	// also caches the position reporter associated with the same filter
	}
	if (!m_piStreamBufferPlayback)
	{
		CacheStreamBufferPlayback();
		m_eLiveVersusBufferedState = UNKNOWN_DATA_BUFFERING;
		m_rtOffsetFromLive = -1;
	}
	ASSERT(m_State == State_Paused);

	hr = CTransInPlaceFilter::Run(tStart);

#ifndef RECOVER_FROM_WEDGED_STOP
	if (!m_fStartedFromStop && FAILED(hr))
#else // RECOVER_FROM_WEDGED_STOP
	if (FAILED(hr))
#endif // RECOVER_FROM_WEDGED_STOP
	{
		DestroyBadResumeRecoveryThread();
	}
	if (SUCCEEDED(hr))
	{
		CAutoLock cLockTransform(&m_cCritSecTransform);

		if (m_fStartedFromStop)
		{
			m_eBackPressureMode = BUILDING_BACKPRESSURE;
			m_uSamplesWhileBuilding = 0;
			m_fStartedFromStop = false;
		}
		m_uConsecutiveSamplesWithNoBackpressure = 0;
		m_uConsecutiveSamplesToForceBuilding = 0;
		m_uLastBackPressure = 0;

		switch (m_eAudioSyncFrameStepMode)
		{
		case AUDIO_SYNC_FRAME_STEP_NONE:
		case AUDIO_SYNC_FRAME_STEP_ACTIVE:
		case AUDIO_SYNC_FRAME_STEP_WAITING_FOR_RUN_TO_END:
			break;

		case AUDIO_SYNC_FRAME_STEP_WAITING_FOR_PAUSE_TO_END:
			m_eAudioSyncFrameStepMode = AUDIO_SYNC_FRAME_STEP_WAITING_FOR_ON_TIME_SAMPLE;
			DbgLog((LOG_HEURISTIC, 3, TEXT("AUDIO:  frame-step state now waiting for on-time-sample\n") ));
			break;

		case AUDIO_SYNC_FRAME_STEP_WAITING_FOR_ON_TIME_SAMPLE:
			break;

		default:
			ASSERT(0);
			m_eAudioSyncFrameStepMode = AUDIO_SYNC_FRAME_STEP_NONE;
			DbgLog((LOG_HEURISTIC, 3, TEXT("AUDIO:  frame-step state now done\n") ));
			break;
		}
		ClearTimeDeltas();
		m_rtLiveSafetyMargin = GetSafetyMargin(m_eLiveTVJitterCategory);
	}

	DbgLog((LOG_HEURISTIC, 3, TEXT("AudioSyncFilter::Run(%I64d) exit\n"), tStart ));
	return hr;
}

STDMETHODIMP AudioSyncFilter::Pause()
{
	DbgLog((LOG_HEURISTIC, 3, TEXT("AudioSyncFilter::Pause() entry\n") ));
#ifdef ENABLE_DIAGNOSTIC_OUTPUT
	ReportLipSyncStats(TEXT("Pause"));
#endif // ENABLE_DIAGNOSTIC_OUTPUT

	if (!m_piAlphaMixer  &&  !m_pRendererPropertySet)
	{
		CacheAlphaMixer();	// also caches the position reporter associated with the same filter
	}
	if (!m_piStreamBufferPlayback)
		CacheStreamBufferPlayback();
	m_eLiveVersusBufferedState = UNKNOWN_DATA_BUFFERING;
	m_rtOffsetFromLive = -1;
	if (!m_piMemAllocator2)
		CacheMemAllocator();
	if (m_State == State_Running)
		m_rtPauseStartStreamTime = GetStreamTime();
	bool fWasRunning = (m_State == State_Running);
	bool fWasStopped = (m_State == State_Stopped);
	ASSERT(m_State != State_Paused);
	HRESULT hr = CTransInPlaceFilter::Pause();
	DestroyBadResumeRecoveryThread();

#ifdef CTV_ENABLE_TESTABILITY
	OnQAStatEvent(QA_EVENT_PAUSE_FILTER);
#endif // CTV_ENABLE_TESTABILITY

	if (SUCCEEDED(hr))
	{
		CAutoLock cTransformLock(&m_cCritSecTransform);

		bool fIgnored;
		hr = AdjustSpeed(1.0, 0, 0, fIgnored, true);
		if (FAILED(hr))
		{
			DbgLog((LOG_ERROR, 1, TEXT("AudioSyncFilter::Pause():  failed in AdjustSpeed()\n") ));
		}
		if (m_eBackPressureMode != BUILDING_BACKPRESSURE)
		{
			m_eBackPressureMode = BUILDING_BACKPRESSURE;
			m_uSamplesWhileBuilding = 0;
		}
		// else inherit old can-exceed
		m_eLiveVersusBufferedState = UNKNOWN_DATA_BUFFERING;
		m_rtOffsetFromLive = -1;
		hr = S_OK;
		if (fWasStopped)
		{
			DetermineIfBuffered();
			m_rtRenderingOffsetCurrent = 0;
			m_rtRenderingOffsetTarget = 0;
			SetRenderingOffset(GetRecommendedOffset());
		}
		switch (m_eAudioSyncFrameStepMode)
		{
		case AUDIO_SYNC_FRAME_STEP_NONE:
			break;

		case AUDIO_SYNC_FRAME_STEP_ACTIVE:
			DbgLog((LOG_LIPSYNCSTATS, 3, TEXT("AUDIO / Pause:  actively frame-stepping, assuming lost end-advance\n") ));
			m_eAudioSyncFrameStepMode = AUDIO_SYNC_FRAME_STEP_WAITING_FOR_PAUSE_TO_END;
			break;

		case AUDIO_SYNC_FRAME_STEP_WAITING_FOR_RUN_TO_END:
			m_eAudioSyncFrameStepMode = AUDIO_SYNC_FRAME_STEP_WAITING_FOR_PAUSE_TO_END;
			DbgLog((LOG_HEURISTIC, 3, TEXT("AUDIO:  frame-step state now waiting for pause to end\n") ));
			break;

		case AUDIO_SYNC_FRAME_STEP_WAITING_FOR_PAUSE_TO_END:
			// ?? Something is odd -- but it might be a race against setting State_Paused
			// and ending the frame advance.
			break;

		case AUDIO_SYNC_FRAME_STEP_WAITING_FOR_ON_TIME_SAMPLE:
			break;

		default:
			ASSERT(0);
			m_eAudioSyncFrameStepMode = AUDIO_SYNC_FRAME_STEP_NONE;
		}
		m_rtLiveSafetyMargin = GetSafetyMargin(m_eLiveTVJitterCategory);
		if (!m_piMediaEventSink)
		{
			IFilterGraph *piFilterGraph = GetFilterGraph();
			if (piFilterGraph)
			{
				HRESULT hrQI = piFilterGraph->QueryInterface(IID_IMediaEventSink, (void **)&m_piMediaEventSink.p);
				if (FAILED(hrQI))
				{
					m_piMediaEventSink.p = NULL;
				}
			}
		}

   	}
	DbgLog((LOG_HEURISTIC, 3, TEXT("AudioSyncFilter::Pause() exit\n") ));
	return hr;
}

STDMETHODIMP AudioSyncFilter::Stop()
{
	DbgLog((LOG_HEURISTIC, 3, TEXT("AudioSyncFilter::Stop() entry\n") ));

#ifdef CTV_ENABLE_TESTABILITY
	OnQAStatEvent(QA_EVENT_STOP_FILTER);
#endif // CTV_ENABLE_TESTABILITY

#ifdef ENABLE_DIAGNOSTIC_OUTPUT
	ReportLipSyncStats(TEXT("Stop"));
#endif // ENABLE_DIAGNOSTIC_OUTPUT

	m_fGivenSampleSinceStop = FALSE;		// to force exit of loop if sleeping post-mute
	m_fFirstSampleSinceFlush = true;
	m_piMediaEventSink.Release();

	HRESULT hr = CTransInPlaceFilter::Stop();
	if (SUCCEEDED(hr))
	{
		DestroyBadResumeRecoveryThread();

		CAutoLock cStateLock(&m_cCritSecBadResumeState);

		m_rtPauseStartStreamTime = 0;
		m_dwFlagsToHandleBadResume = BAD_RESUME_FLAG_NEED_SAMPLE_DOWNSTREAM;
		m_dwBadResumeSampleCountdown = 0;
		m_fStartedFromStop = true;
		m_lInReceiveCount = 0;
		m_dwReceiveEntrySysTicks = 0;
		m_eBackPressureMode = BUILDING_BACKPRESSURE;
 		m_uSamplesWhileBuilding = 0;
		ClearTimeDeltas(false);
		m_rtRenderingOffsetCurrent = 0;
		m_rtRenderingOffsetTarget = 0;
		SetRenderingOffset(0);
		m_uConsecutiveSamplesWithNoBackpressure = 0;
		m_uConsecutiveSamplesToForceBuilding = 0;
		m_uLastBackPressure = 0;
		m_piAlphaMixer.Release();
		if (m_pRendererPropertySet)
		{
			m_pRendererPropertySet->Release();
			m_pRendererPropertySet = NULL;
		}
		m_piStreamBufferPlayback.Release();
		m_eLiveVersusBufferedState = UNKNOWN_DATA_BUFFERING;
		m_rtOffsetFromLive = -1;
		m_piMemAllocator2.Release();
		m_iSamplesSinceMuteChange = 0;
		m_uSamplesUntilCanIncrement = 0;
		m_fGivenSampleSinceStop = FALSE;
		m_eAudioSyncFrameStepMode = AUDIO_SYNC_FRAME_STEP_NONE;
		m_dblRenderingRate = 1.0;
		m_rtLiveSafetyMargin = GetSafetyMargin(m_eLiveTVJitterCategory);
	}
	DbgLog((LOG_HEURISTIC, 3, TEXT("AudioSyncFilter::Stop() exit\n") ));
	return hr;
}

HRESULT AudioSyncFilter::CarryOutReceive(IMediaSample *pSample)
{
	HRESULT hr;
	{
		InReceiveTracker sInReceiveTracker(*this);
		hr = CTransInPlaceFilter::Receive(pSample);
	}
	return hr;
}

DWORD AudioSyncFilter::BadResumeThreadMain(LPVOID pvAudioSyncFilter)
{
	static_cast<AudioSyncFilter*>(pvAudioSyncFilter)->BadResumeMain();
	return 0;
} // AudioSyncFilter::BadResumeThreadMain

void AudioSyncFilter::BadResumeMain()
{
#ifdef _WIN32_WCE
	// It helps tuning and other trick modes to make this normally-sleepy thread
	// have a little higher priority so that it can wake up and respond quickly
	// if there is a need to flush, change rate, or seek.
	CeSetThreadPriority(GetCurrentThread(), 250);
#endif // _WIN32_WCE

	DbgLog((LOG_PAUSEBUG, 1, TEXT("AudioSyncFilter::BadResumeMain() - entry with %s, %s\n"),
		m_fGivenSampleSinceStop ? TEXT("saw-sample") : TEXT("no-sample"),
		(m_dwFlagsToHandleBadResume & BAD_RESUME_FLAG_ADVISE_PENDING) ? TEXT("advise-pending") : TEXT("no-advise-pending") ));

	DbgLog((LOG_PAUSEBUG, 1, TEXT("AudioSyncFilter::BadResumeMain() - enterring main loop\n") ));

	// Our initial sleep is long enough that if a sample had been received while paused,
	// it should've had time to render while we are running.  Thereafter we check more
	// frequently to catch the problem before the wedge + recovery has gone on long
	// enough to make the user really unhappy:

	DWORD dwSleepTime = MAX_TIME_IN_RECEIVE + (MAX_TIME_IN_RECEIVE >> 2);

	while (m_dwBadResumeSampleCountdown != 0)
	{
		WaitForSingleObject(m_hAdviseEvent, dwSleepTime);
		dwSleepTime = MAX_TIME_IN_RECEIVE >> 2;

		bool fNeedFlush = false;

		{
			// Note that we only hold the bad resume state lock while deciding
			// whether or not to flush. It would not be safe to hold it
			// while we grab the filter lock:

			CAutoLock cStateLock(&m_cCritSecBadResumeState);

			if ((m_dwFlagsToHandleBadResume & BAD_RESUME_FLAG_ADVISE_PENDING) &&
				NeedToFlushToRecover())
			{
				fNeedFlush = true;
			}
		}
		if (fNeedFlush)
		{
			// To avoid an assert caused by concurrent flushes, grab the filter lock
			// long enough do the flush -- and test if we are already flushing before
			// doing the flush:

			DbgLog((LOG_ERROR, 1, TEXT("### AudioSyncFilter::BadResumeMain() - flushing to compensate for audio renderer wedge ###\n") ));

			CAutoLock lck(m_pLock);
			
			CTransInPlaceOutputPin *pOutputPin = OutputPin();
			CBaseInputPin *pInputPin = InputPin();
			ASSERT(pOutputPin);
			if (pOutputPin && (!pInputPin || !pInputPin->IsFlushing()))
			{
				DbgLog((LOG_PAUSEBUG, 1, TEXT("AudioSyncFilter::BadResumeMain() - beginning flush with %u empty buffers ...\n"),
							MeasureBackPressure() ));

				ResetEvent(m_hInRecoveryEvent);
				if (SUCCEEDED(pOutputPin->DeliverBeginFlush()))
				{
					pOutputPin->DeliverEndFlush();
					m_fFirstSampleSinceFlush = true;
				}
#ifdef CTV_ENABLE_TESTABILITY
				OnQAStatEvent(QA_EVENT_PAUSE_RECOVERY);
#endif // CTV_ENABLE_TESTABILITY
				SetEvent(m_hInRecoveryEvent);
				DbgLog((LOG_PAUSEBUG, 1, TEXT("AudioSyncFilter::BadResumeMain() - done with flush ...\n") ));
				break;
			}
			else
			{
				DbgLog((LOG_PAUSEBUG, 1, TEXT("### AudioSyncFilter::BadResumeMain() - can't flush now ###\n") ));
			}
		}
		if (!(m_dwFlagsToHandleBadResume & BAD_RESUME_FLAG_ADVISE_PENDING))
			break;
	}
	m_dwFlagsToHandleBadResume &= ~BAD_RESUME_FLAG_ADVISE_PENDING;
} // AudioSyncFilter::BadResumeMain

HRESULT AudioSyncFilter::CreateBadResumeRecoveryThread()
{
	CAutoLock cStateLock(&m_cCritSecBadResumeState);

	m_dwFlagsToHandleBadResume |= BAD_RESUME_FLAG_NEED_SAMPLE_DOWNSTREAM;

	// Set up the AdviseTime() request plus
	ASSERT(!m_hBadResumeRecoveryThread);

	if (m_pClock)
	{
		m_dwFlagsToHandleBadResume |= BAD_RESUME_FLAG_ADVISE_PENDING;
		ResetEvent(m_hAdviseEvent);
	}
	else
	{
		DbgLog((LOG_TRACE, 2, TEXT("AudioSyncFilter::CreateBadResumeRecoveryThread() -- exit [no clock]\n") ));
		return S_FALSE;	// no clock
	}

	m_dwFlagsToHandleBadResume |= BAD_RESUME_FLAG_ADVISE_PENDING;
	m_dwBadResumeSampleCountdown = (DWORD) (m_lBufferCount + 2);
	m_hBadResumeRecoveryThread = CreateThread(NULL, 0, BadResumeThreadMain, this, NULL, 0);
	if (!m_hBadResumeRecoveryThread)
	{
		DbgLog((LOG_ERROR, 1, TEXT("### AudioSyncFilter::CreateBadResumeRecoveryThread() -- failed to create BadResumeThreadMain thread ###\n") ));

		// The send-delayed-sample-thread will exit shortly barring the wedged-resume-from-flush bug.
		SetEvent(m_hAdviseEvent);
		m_dwFlagsToHandleBadResume &= ~BAD_RESUME_FLAG_ADVISE_PENDING;
		m_dwBadResumeSampleCountdown = 0;
		return E_FAIL;
	}
	return S_OK;
} // AudioSyncFilter::CreateBadResumeRecoveryThread

void AudioSyncFilter::DestroyBadResumeRecoveryThread()
{
	// Issue the wake-up call to the bad-resume thread -- so hopefully it will be gone
	// by the time we're done with the send-delayed-sample thread:

	if (m_hBadResumeRecoveryThread)
	{
		CAutoLock cStateLock(&m_cCritSecBadResumeState);

		DbgLog((LOG_PAUSEBUG, 1, TEXT("### AudioSyncFilter::DestroyBadResumeRecoveryThread() -- setting wake-up event ###\n") ));

		if (m_dwFlagsToHandleBadResume & BAD_RESUME_FLAG_ADVISE_PENDING)
		{
			m_dwFlagsToHandleBadResume &= ~BAD_RESUME_FLAG_ADVISE_PENDING;
			m_dwBadResumeSampleCountdown = 0;
		}

		SetEvent(m_hAdviseEvent);
	}

	// Now, on to the bad-resume recovery thread.  It listens to an event, and if
	// the event is signalled when the flag asking for an advise and need-a-sample
	// are both still set, it will flush.  So we clear the advise flag first, make
	// sure the event is set, then wait.

	if (m_hBadResumeRecoveryThread)
	{
		WaitForSingleObject(m_hBadResumeRecoveryThread, INFINITE);

		CAutoLock cStateLock(&m_cCritSecBadResumeState);
		CloseHandle(m_hBadResumeRecoveryThread);
		m_hBadResumeRecoveryThread = NULL;
	}
} // AudioSyncFilter::DestroyBadResumeRecoveryThread

bool AudioSyncFilter::NeedToFlushToRecover()
{
	unsigned uEmptyBufferCount = MeasureBackPressure();

	if (m_fSleepingInReceive || (uEmptyBufferCount != 0))
	{
		// Either sleeping until sample is deliverable or there are
		// available buffers so the problem can't be that upstream
		// is blocked due to downstream failing to release buffers.

#ifdef ENABLE_DIAGNOSTIC_OUTPUT
		if (m_fSleepingInReceive)
		{
			DbgLog((LOG_PAUSEBUG, 1, TEXT("AudioSyncFilter::NeedFlushToRecover() -- sleeping in receive\n") ));
		}
		else
		{
			DbgLog((LOG_PAUSEBUG, 1, TEXT("AudioSyncFilter::NeedFlushToRecover() -- still %u empty buffers\n"),
				uEmptyBufferCount ));
		}
#endif // ENABLE_DIAGNOSTIC_OUTPUT
		return false;
	}

	if (m_dwFlagsToHandleBadResume & BAD_RESUME_FLAG_NEED_SAMPLE_DOWNSTREAM)
	{
		DbgLog((LOG_ERROR, 1, TEXT("### AudioSyncFilter: NeedToFlushToRecover() -- no sample downstream yet, assuming audio driver is wedged ###\n") ));
		return true;
	}
	DWORD dwTicksNow = GetTickCount();
	DWORD dwInReceive = 0;
	if (dwTicksNow > m_dwReceiveEntrySysTicks)
	{
		dwInReceive = (dwTicksNow - m_dwReceiveEntrySysTicks);
	}
	else
	{
		// The system time wrapped around.  Here we go, taking advantage
		// of integer overflow:

		dwInReceive = dwTicksNow + 1 + (0xffffffff - m_dwReceiveEntrySysTicks);
	}
	if (dwInReceive > MAX_TIME_IN_RECEIVE)
	{
		DbgLog((LOG_ERROR, 1, TEXT("### AudioSyncFilter: NeedToFlushToRecover() -- Receive() seems to be stuck [%u ms], assuming audio driver is wedged ###\n"),
			dwInReceive));
	}
	else
	{
		DbgLog((LOG_PAUSEBUG, 1, TEXT("AudioSyncFilter::NeedFlushToRecover() -- Receive() seems fine [last sample %u ms ago]\n"),
			dwInReceive));
	}
	return (dwInReceive > MAX_TIME_IN_RECEIVE);
} // AudioSyncFilter::NeedToFlushToRecover

// MeasureBackPressure() computes how many buffers are available by attempting to
// grab a buffer with no wait. If a buffer is obtains, recursively compute how
// many additional buffers are also available.

unsigned AudioSyncFilter::MeasureBackPressure()
{
	CComPtr<IMediaSample> piMediaSample;
	unsigned uBackPressureMeasure = 0;

	if (m_piMemAllocator2)
	{
		DWORD dwFreeBuffers = 0;
		if (SUCCEEDED(m_piMemAllocator2->GetFreeCount(&dwFreeBuffers)))
		{
			return dwFreeBuffers;
		}
	}
	if (m_pOutput && m_pOutput->IsConnected())
	{
		HRESULT hr = m_pOutput->GetDeliveryBuffer(&piMediaSample, NULL, NULL, AM_GBF_NOWAIT);
		if (SUCCEEDED(hr) && piMediaSample)
		{
			uBackPressureMeasure = 1 + MeasureBackPressure();
		}
	}
	else
	{
		ASSERT(0);
		uBackPressureMeasure = 1;
	}
	if (((long)uBackPressureMeasure) > m_lBufferCount)
		m_lBufferCount = (long) uBackPressureMeasure;
	return uBackPressureMeasure;
} // AudioSyncFilter::MeasureBackPressure

// ComputeVideoOffset() is called when we are in "BUILDING" mode to decide what
// are current presentation time offset should be.  We use an average of our
// current offset and -1 times our current averaged lead/lag.  All of these
// measures are in milliseconds.

HRESULT AudioSyncFilter::ComputeVideoOffset(unsigned uBackPressureMeasure, REFERENCE_TIME rtDelta)
{
	REFERENCE_TIME rtOffset = 0;
	REFERENCE_TIME rtToBeExactlyOnTime = - GetWeightedTimeDelta();

	if (rtToBeExactlyOnTime <= m_rtRenderingOffsetTarget)
	{
		if (rtToBeExactlyOnTime < m_rtRenderingOffsetCurrent)
			rtOffset = m_rtRenderingOffsetCurrent - 1;
		else
			rtOffset = rtToBeExactlyOnTime;
	}
	else
	{
		rtOffset = (m_rtRenderingOffsetTarget + rtToBeExactlyOnTime) / 2;
	}
	return SetRenderingOffset(rtOffset);
} // AudioSyncFilter::ComputeVideoOffset

// UpdateBackPressurePolicy() looks at the current lead/lag and back pressure measures
// to decide whether we should continue our current mode or switch between NORMAL and
// BUILDING modes.  The switch from BUILDING to NORMAL goes via an intermediate ACHIEVED
// state so that the caller will know of the transition. The caller is responsible for
// changing ACHIEVED to NORMAL.

void AudioSyncFilter::UpdateBackPressurePolicy(unsigned uBackPressureMeasure, REFERENCE_TIME rtDelta)
{
	switch (m_eBackPressureMode)
	{
	case BUILDING_BACKPRESSURE:
		// We were trying to build backpressure.  If we have enough now,
		// switch to the transient state that indicates we just achieved
		// enough backpressure:

		++m_uSamplesWhileBuilding;
		if ((uBackPressureMeasure <= g_uMinimumAcceptableBackPressure) &&
			(m_uSamplesWhileBuilding >= g_uMinimumSamplesWhileBuilding))
		{
			DbgLog((LOG_HEURISTIC, 1, TEXT("AudioSyncFilter::UpdateBackPressurePolicy(%u) .. BUILDING -> ACHIEVED\n"),
				uBackPressureMeasure ));
			m_eBackPressureMode = ACHIEVED_BACKPRESSURE;
 			m_uConsecutiveSamplesWithNoBackpressure = 0;
			m_uConsecutiveSamplesToForceBuilding = 0;
			m_dwSampleDropCountdown = 0;
		}
		else
		{
			DbgLog((LOG_TIMING, 1, TEXT("AudioSyncFilter::UpdateBackPressurePolicy(%u) .. BUILDING (still)\n"),
				uBackPressureMeasure ));
		}
		break;

	case ACHIEVED_BACKPRESSURE:
		// Yikes!  This is supposed to be a transient state that
		// does not survive exit of Transform().  Assert then
		// continue in normal backpressure mode.  Yes, this
		// means falling through to the next case.
		ASSERT(0);

	case NORMAL_BACKPRESSURE:
		// We need to watch for too many consecutive samples without
		// sufficient backpressure.  If that happens, drop back to
		// the no backpressure state.
		if (uBackPressureMeasure <= g_uMinimumAcceptableBackPressure)
		{
			DbgLog((LOG_TIMING, 1, TEXT("AudioSyncFilter::UpdateBackPressurePolicy(%u) .. NORMAL (still)\n"),
				uBackPressureMeasure ));
			m_uConsecutiveSamplesWithNoBackpressure = 0;
		}
		else
		{
			++m_uConsecutiveSamplesWithNoBackpressure;
		}
		if ((m_rtRenderingOffsetCurrent == m_rtRenderingOffsetTarget) &&
			(m_rtRenderingOffsetCurrent + GetWeightedTimeDelta() <= g_rtMillisecLagToForceBuilding))
			++m_uConsecutiveSamplesToForceBuilding;
		else
			m_uConsecutiveSamplesToForceBuilding = 0;
		if ((m_uConsecutiveSamplesWithNoBackpressure >= g_ExcessiveSamplesWithoutBackpressure) ||
			(m_uConsecutiveSamplesToForceBuilding >= g_MinimumLateSamplesToForceBuilding))
		{
			if (m_rtRenderingOffsetCurrent == m_rtRenderingOffsetTarget)
			{
				DbgLog((LOG_HEURISTIC, 1, TEXT("AudioSyncFilter::UpdateBackPressurePolicy(%u) .. NORMAL -> BUILDING\n"),
					uBackPressureMeasure ));
				m_eBackPressureMode = BUILDING_BACKPRESSURE;
				m_uSamplesWhileBuilding = 0;
			}
		}
		break;

	default:
		ASSERT(0);
		break;
	}
	m_uLastBackPressure = uBackPressureMeasure;
} // AudioSyncFilter::UpdateBackPressurePolicy

// FixVideoOffset() computes the stream clock versus presentation time
// offset that we're going to hold onto as long as we stay in normal
// rendering mode.

HRESULT AudioSyncFilter::FixVideoOffset(REFERENCE_TIME rtDelta)
{
	REFERENCE_TIME rtOffset = 0;

	DbgLog((LOG_HEURISTIC, 1, TEXT("AudioSyncFilter::FixVideoOffset(%I64d) -- prior offset %I64d of %I64d, worst recent offset %I64d\n"),
		rtDelta, m_rtRenderingOffsetCurrent, m_rtRenderingOffsetTarget, GetWeightedTimeDelta() ));

	rtOffset = - GetWeightedTimeDelta(); // -rtDelta;
	if ((rtOffset < m_rtRenderingOffsetCurrent) &&
		(m_rtRenderingOffsetCurrent <= m_rtRenderingOffsetTarget))
	{
		// Adopting the current delta pushes us farther from our target, 
		// which is likely to accentuate a/v instability. Stick with
		// the current value:
		rtOffset = m_rtRenderingOffsetCurrent - 1;
	}
	else if ((rtOffset > m_rtRenderingOffsetCurrent) &&
			 (m_rtRenderingOffsetCurrent >= m_rtRenderingOffsetTarget))
	{
		// Adopting the current delta pushes us farther from our target, 
		// which is likely to accentuate a/v instability. Stick with
		// the current value:
		rtOffset = m_rtRenderingOffsetCurrent + 1;
	}

	DbgLog((LOG_LIPSYNCSTATS, 1, TEXT("### AUDIO:  fixing offset from stream at %I64d ms [recommended] ###\n"), rtOffset ));
	return SetRenderingOffset(rtOffset);
} // AudioSyncFilter::FixVideoOffset

// SetRenderingOffset() stores our stream versus actual rendering time
// offset (measured in milliseconds), both locally and at the AOvMixer

HRESULT AudioSyncFilter::SetRenderingOffset(REFERENCE_TIME rtOffset, bool fUpdateCurrentOnly)
{
	// Update the rendering offset locally and tell the AOvMixer (if known)
	// about the new offset:

	HRESULT hr = S_OK;

	DbgLog((LOG_TIMING, 1, TEXT("AudioSyncFilter::SetRenderingOffset(%I64d recommended) -- was %I64d of %I64d, initial mode %s, offset from live %I64d\n"),
		rtOffset, m_rtRenderingOffsetCurrent, m_rtRenderingOffsetTarget,
			(UNKNOWN_DATA_BUFFERING == m_eLiveVersusBufferedState) ? TEXT("unknown")
			: ((KNOWN_TO_BE_LIVE_DATA == m_eLiveVersusBufferedState) ? TEXT("live") : TEXT("buffered") ),
			m_rtOffsetFromLive ));

	DetermineIfBuffered();
	LONGLONG rtRecommendedCurrentOffset = GetRecommendedOffset(true, true, rtOffset);
	LONGLONG rtRecommendedTargetOffset = fUpdateCurrentOnly ? m_rtRenderingOffsetTarget
		: GetRecommendedOffset(true, false, rtOffset);

	DbgLog((LOG_TIMING, 1, TEXT("AudioSyncFilter::SetRenderingOffset() -- decided on %I64d of %I64d, new mode %s\n"),
			rtRecommendedCurrentOffset, rtRecommendedTargetOffset,
			(UNKNOWN_DATA_BUFFERING == m_eLiveVersusBufferedState) ? TEXT("unknown")
				: ((KNOWN_TO_BE_LIVE_DATA == m_eLiveVersusBufferedState) ? TEXT("live") : TEXT("buffered")) ));

	m_rtRenderingOffsetCurrent = rtRecommendedCurrentOffset;
	if (!fUpdateCurrentOnly)
		m_rtRenderingOffsetTarget = rtRecommendedTargetOffset; 
	rtRecommendedCurrentOffset += g_VideoLeadMillisec;
	LONGLONG llDelay = rtRecommendedCurrentOffset  * 10000LL;		// rtOffset is in milliseconds, argument is in 100 ns units
	if (m_piAlphaMixer)
	{
		hr = m_piAlphaMixer->InsertDelay(llDelay);		// rtOffset is in milliseconds, argument is in 100 ns units
	}
	else if (m_pRendererPropertySet)
	{
		hr = m_pRendererPropertySet->Set(AM_KSPROPSETID_RendererPosition, AM_PROPERTY_Delay, 
			NULL, 0, &llDelay, sizeof(LONGLONG));
	}
	else
	{
		DbgLog((LOG_ERROR, 1, TEXT("AudioSyncFilter::SetRenderingOffset() ... ?? unexpected -- no AlphaMixer interface or AM_PROPERTY_Delay!!\n") ));
	}
	if (FAILED(hr))
	{
		DbgLog((LOG_ERROR, 1, TEXT("AudioSyncFilter::SetRenderingOffset():  IAlphaMixer::InsertDelay() failed\n") ));
	}
	return hr;
} // AudioSyncFilter::SetRenderingOffset


HRESULT AudioSyncFilter::QueryRendererPropertySet(IUnknown *pUnknown, IKsPropertySet **piRendererPropertySet)
{
	IKsPropertySet *piKsPropertySet;
	
	if (!piRendererPropertySet)
	{
		return E_INVALIDARG;
	}
	
	if (FAILED(pUnknown->QueryInterface(IID_IKsPropertySet, (void **) &piKsPropertySet)))
	{
		return E_FAIL;
	}
	
	DWORD dwSupportType = 0;
	
	if ((S_OK != piKsPropertySet->QuerySupported(AM_KSPROPSETID_RendererPosition, AM_PROPERTY_Delay, &dwSupportType)) ||
		!(dwSupportType & KSPROPERTY_SUPPORT_SET))
	{
		piKsPropertySet->Release();
		return E_FAIL;
	}
	
	*piRendererPropertySet = piKsPropertySet;
	return S_OK;
}


// Cache a handle to the overlay mixer interface that we use to keep it
// informed of the stream versus rendering time offset:

void AudioSyncFilter::CacheAlphaMixer()
{
	// We need to rummage over the filter graph, looking for a filter
	// with an input pin supporting IAlphaMixer, and/or a filter supporting the delay property

	if (m_pRendererPropertySet)
	{
		m_pRendererPropertySet->Release();
		m_pRendererPropertySet = NULL;
	}

	IFilterGraph *pFilterGraph = GetFilterGraph();
	if (pFilterGraph)
	{
		IEnumFilters *pFilterIter;

		if (SUCCEEDED(pFilterGraph->EnumFilters(&pFilterIter)))
		{
			CComPtr<IEnumFilters> cComPtrIEnumFilters = NULL;
			cComPtrIEnumFilters.Attach(pFilterIter);

			IBaseFilter *pFilter;
			ULONG iFiltersFound;

			while (SUCCEEDED(pFilterIter->Next(1, &pFilter, &iFiltersFound)) &&
				(iFiltersFound > 0))
			{
				CComPtr<IBaseFilter> cComPtrIBaseFilter = NULL;
				cComPtrIBaseFilter.Attach(pFilter);

				if (SUCCEEDED(QueryRendererPropertySet (pFilter, &m_pRendererPropertySet)))
				{
					break;
				}

				IEnumPins *piEnumPins;
				HRESULT hr = pFilter->EnumPins(&piEnumPins);
				if (SUCCEEDED(hr))
				{
					CComPtr<IEnumPins> piEnumPinsHolder = piEnumPins;
					piEnumPins->Release();

					ULONG uPinCount;
					IPin *piPin;
					CComPtr<IPin> piPinHolder;

					while ((S_OK == piEnumPins->Next(1, &piPin, &uPinCount)) && (uPinCount != 0))
					{
						piPinHolder.Attach(piPin);

						PIN_INFO sPinInfo;
						if (SUCCEEDED(piPin->QueryPinInfo(&sPinInfo)))
						{
							if (sPinInfo.pFilter)
								sPinInfo.pFilter->Release();
							if (sPinInfo.dir == PINDIR_INPUT)
							{
								if (SUCCEEDED(piPin->QueryInterface(IID_IAlphaMixer, (void**)(IAlphaMixer*)&m_piAlphaMixer)))
								{
									piPinHolder.Release();
									break;
								}
							}
						}
						piPinHolder.Release();

					} // while more pins to check
				} // if able to enumerate pins

				if (m_piAlphaMixer)
				{
					break;
				}
			} // while more filters to check
		}  // if able to enumerate filters
	} // if have pointer to filter graph

	ASSERT(m_piAlphaMixer  ||  m_pRendererPropertySet);
} // AudioSyncFilter::CacheAlphaMixer

// Cache a handle to the first filter we find that supports
// IStreamBufferPlayback:

void AudioSyncFilter::CacheStreamBufferPlayback()
{
	// We need to rummage over the filter graph, looking for a filter
	// with an input pin supporting IAlphaMixer.

	IFilterGraph *pFilterGraph = GetFilterGraph();
	if (pFilterGraph)
	{
		IEnumFilters *pFilterIter;

		if (SUCCEEDED(pFilterGraph->EnumFilters(&pFilterIter)))
		{
			CComPtr<IEnumFilters> cComPtrIEnumFilters = NULL;
			cComPtrIEnumFilters.Attach(pFilterIter);

			IBaseFilter *pFilter;
			ULONG iFiltersFound;

			while (SUCCEEDED(pFilterIter->Next(1, &pFilter, &iFiltersFound)) &&
				(iFiltersFound > 0))
			{
				if (SUCCEEDED(pFilter->QueryInterface(IID_IStreamBufferPlayback, (void**)(IStreamBufferPlayback*)&m_piStreamBufferPlayback)))
				{
					pFilter->Release();
					break;
				}
				pFilter->Release();
			} // while more filters to check
		}  // if able to enumerate filters
	} // if have pointer to filter graph
	ASSERT(m_piStreamBufferPlayback);
} // AudioSyncFilter::CacheStreamBufferPlayback

void AudioSyncFilter::CacheMemAllocator()
{
	if (m_piMemAllocator2)
		m_piMemAllocator2.Release();
	if (m_pOutput && m_pOutput->IsConnected())
	{
		m_piMemAllocator2 = static_cast<CAudioSyncOutputPin*>(m_pOutput)->GetAllocator();
	}
} // AudioSyncFilter::CacheMemAllocator

REFERENCE_TIME AudioSyncFilter::GetStreamTime()
{
	REFERENCE_TIME rtNow;

	switch(m_State)
	{
	case State_Running:
		{
			HRESULT hr = m_pClock ? m_pClock->GetTime(&rtNow) : E_FAIL;
			ASSERT(SUCCEEDED(hr));
			if (FAILED(hr))
			{
				DbgLog((LOG_ERROR, 1, TEXT("AudioSyncFilter::GetStreamTime() -- error 0x%x getting time in state running\n"), hr ));
				rtNow = 0;
				break;
			}
			rtNow -= m_tStart;
		}
		break;

	case State_Paused:
		rtNow = m_rtPauseStartStreamTime;
		break;

	case State_Stopped:
	default:
		rtNow = 0;
		break;
	}
	return rtNow;
} // AudioSyncFilter::GetStreamTime

// Clears out the state we use to measure how far we are from
// normal rendering.  Also put us back to the normal playback
// at 1x.
void AudioSyncFilter::ClearTimeDeltas(bool fRestoreNormalSpeed)
{
	m_dblAvgSampleInterval = 0.0;
	m_rtLastSampleStartTime = 0LL;
	m_dblAdjust = 0.0;
	m_dwSampleDropCountdown = 0;
	if (fRestoreNormalSpeed)
	{
		bool fIgnored;
		AdjustSpeed(1.0, 0, 0, fIgnored, true);
	}
} // AudioSyncFilter::ClearTimeDeltas

// ComputeTargetSpeed() is the heart of the heuristic.  We translate the current
// lead/lag and back pressure measured plus the updated running averages kept in
// data members into a playback rate designed to bring us back to our ideal
// steady-state situation.

double AudioSyncFilter::ComputeTargetSpeed(REFERENCE_TIME rtDelta, unsigned uBackPressureMeasure)
{
	// We are going to compute a rate that will get the lead/lag down to zero in "g_dblMillisecToSpreadOutCorrection"
	// milliseconds assuming that samples from here on arrive at their normal presentation time interval.
	// In our ideal situation, all buffers are full and our lead/lag is zero.  So the basic amount we need
	// to overcome is a combination of our current lead/lag (adjusted by rendering versus stream offset)
	// and how long it will take (at normal sample arrival rate) to fill all of the empty buffers.
	//
	// Empirically, some systems suffer from bouts of background activity making future samples arrive faster or
	// slower than they ought to. So we also fold in a fudge factor, "m_dblAdjust", that swells and diminishes
	// as we persist in being in a leading or lagging state.

	double dblMillisecToCompensateFor = (((double) m_rtRenderingOffsetCurrent) + ((double)GetWeightedTimeDelta()));

	// Minor tweak -- increase pressure to play slower than normal if we have
	// buffers to fill:
	double dblMillisecLeadAdjustedForBuffers = dblMillisecToCompensateFor
			- ((double)(uBackPressureMeasure * g_PenaltyForEmptyBuffers));

	double dblMillisecToFillBuffersAndCloseGap = m_dblAdjust +
		(g_dblMillisecToSpreadOutCorrection - dblMillisecLeadAdjustedForBuffers);

	DbgLog((LOG_TIMING, 1, TEXT("AudioSyncFilter::ComputeTargetSpeed():  %u of %ld buffers are empty @ %lf ms interval\n"),
		uBackPressureMeasure, m_lBufferCount, m_dblAvgSampleInterval));
	DbgLog((LOG_TIMING, 1, TEXT("AudioSyncFilter::ComputeTargetSpeed(%I64d):  gap=%lf [%lf adjusted for empty buffers], speed=%lf\n"),
		rtDelta + m_rtRenderingOffsetCurrent,
		dblMillisecToCompensateFor,
		dblMillisecLeadAdjustedForBuffers,
		dblMillisecToFillBuffersAndCloseGap / g_dblMillisecToSpreadOutCorrection));

	if (dblMillisecLeadAdjustedForBuffers < - m_dblAvgSampleInterval)
		m_dblAdjust += 2.0;
	else if (dblMillisecLeadAdjustedForBuffers > m_dblAvgSampleInterval)
		m_dblAdjust -= 1.0;
	else if ((m_dblAdjust < 0.0) && (dblMillisecLeadAdjustedForBuffers < 0.0))
		m_dblAdjust += 2.0;
	else if ((m_dblAdjust > 0.0) && (dblMillisecLeadAdjustedForBuffers > 0.0))
		m_dblAdjust -= 1.0;
	if (m_dblAdjust > g_dblMaxAdjust)
		m_dblAdjust = g_dblMaxAdjust;
	else if (m_dblAdjust < g_dblMinAdjust)
		m_dblAdjust = g_dblMinAdjust;

	if (rtDelta >= 0)
		m_dwSampleDropCountdown = 0;

	double dblSpeed = dblMillisecToFillBuffersAndCloseGap / g_dblMillisecToSpreadOutCorrection;
#if 0
	double dblDeltaSpeed = dblSpeed - 0.97;
	double dblSpeedDeltaSquare = dblDeltaSpeed * dblDeltaSpeed;
	return dblSpeed + dblSpeedDeltaSquare * dblSpeedDeltaSquare * ((dblDeltaSpeed >= 0) ? 1.0 : -1.0);
#else
	double dblSpeedSquare = dblSpeed * dblSpeed;
	return dblSpeedSquare * dblSpeedSquare;
#endif
} // AudioSyncFilter::ComputeTargetSpeed

// SetBufferCount() caches our memory of how many buffers there are in the buffer
// pool:

void AudioSyncFilter::SetBufferCount(long lBufferCount)
{
	if (lBufferCount > 0)
		m_lBufferCount = lBufferCount;
}

CAudioSyncOutputPin::CAudioSyncOutputPin(TCHAR *pObjectName,
										 CTransInPlaceFilter *pFilter,
										 HRESULT *phr,
										 LPCWSTR pName) :
	CTransInPlaceOutputPin (pObjectName, pFilter, phr, pName)
{
}

// The base class implementation of this method will prefer the input pin's
// allocator if available. However, the audio renderer must internally manage
// waveout API buffers, and is therefore forced to perform buffer copies when
// the upstream output pin forces its own allocator. We override here to prefer
// the renderer's input pin's allocator, to avoid such copying. The input pin's
// allocator will eventually be set to that of the output pin once the graph
// manager disconnects and reconnects the decoder's output pin to this filter's
// input pin.
HRESULT CAudioSyncOutputPin::DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc)
{
	// Note that *ppAlloc is almost certainly identical to m_Allocator

	HRESULT hr;
	*ppAlloc = NULL;

	if (! IsConnected())
		return VFW_E_NO_ALLOCATOR;

	// Get an addreffed allocator from the downstream input pin.
	hr = m_pInputPin->GetAllocator(ppAlloc);

	if (FAILED(hr))
		return hr;

	ASSERT(*ppAlloc);

	ALLOCATOR_PROPERTIES prop;
	ZeroMemory(&prop, sizeof(prop));

	// Try to get requirements from downstream
	pPin->GetAllocatorRequirements(&prop);

	// If he doesn't care about alignment, then set it to 1
	if (prop.cbAlign == 0)
		prop.cbAlign = 1;

	hr = DecideBufferSize(*ppAlloc, &prop);

	if (FAILED(hr))
	{
		(*ppAlloc)->Release();
		*ppAlloc = NULL;
		return hr;
	}

	// Tell the downstream input pin

	static_cast<AudioSyncFilter*>(m_pFilter)->SetBufferCount(prop.cBuffers);

	// Tell the downstream input pin
	hr = pPin->NotifyAllocator(*ppAlloc, FALSE);
	if (SUCCEEDED(hr) && SUCCEEDED((*ppAlloc)->GetProperties(&prop)))
	{
		static_cast<AudioSyncFilter*>(m_pFilter)->SetBufferCount(prop.cBuffers);
	}
	return hr;
}

#ifdef ENABLE_DIAGNOSTIC_OUTPUT

void AudioSyncFilter::ReportLipSyncStats(const TCHAR *)
{
	if (!m_llNumDriftSamples && !m_dwSamplesDelivered && !m_dwSamplesDropped && !m_dwPotentiallyDry &&
		!m_dwSamplesError && !m_dwPausedOrFlushing && !m_dwFrameStepSamples && !m_dwCritialSamples)
	{
		DbgLog((LOG_LIPSYNCSTATS, 1, TEXT("### AUDIO - no audio received ###\n") ));
		return;
	}

	if (m_llNumDriftSamples)
	{
		DbgLog((LOG_LIPSYNCSTATS, 1, 
			TEXT("### AUDIO Deviation from ideal lip sync:  %I64d (%I64d) ms average [min: %I64d, max: %I64d] @ offset %I64d of %I64d ###\n"),
			m_llCumulativeDrift / (10000LL * m_llNumDriftSamples),
			m_llCumAdjDrift / m_llNumDriftSamples,
			m_llMinimumDrift / 10000LL, 
			m_llMaximumDrift / 10000LL,
			m_rtRenderingOffsetCurrent, m_rtRenderingOffsetTarget ));
	}
	if (m_dwSamplesDropped || m_dwSamplesError || m_dwPotentiallyDry)
	{
		DbgLog((LOG_LIPSYNCSTATS, 2,
				TEXT("Audio: %u samples delivered, %u dropped [%u error], %u possible pops due to no-data\n"),
						m_dwSamplesDelivered, m_dwSamplesDropped, m_dwSamplesError, m_dwPotentiallyDry));
		m_dwSamplesDelivered = 0;
		m_dwSamplesDropped = 0;
		m_dwSamplesError = 0;
		m_dwPotentiallyDry = 0;
	}
	if (m_dwPausedOrFlushing || m_dwFrameStepSamples || m_dwCritialSamples)
	{
		DbgLog((LOG_LIPSYNCSTATS, 2,
				TEXT("Audio: %u samples delivered while paused or flushing, %u while frame-stepping, %u to avoid wedge\n"),
						m_dwPausedOrFlushing, m_dwFrameStepSamples, m_dwCritialSamples));
		m_dwPausedOrFlushing = 0;
		m_dwFrameStepSamples = 0;
		m_dwCritialSamples = 0;
	}

	m_llCumulativeDrift = 0;
	m_llCumAdjDrift = 0;
	m_llMaximumDrift = -0x7fffffffffffffffLL;
	m_llMinimumDrift = 0x7fffffffffffffffLL;

	if (m_llNumDriftSamples)
	{
		DbgLog((LOG_HEURISTIC, 2,
			TEXT("Audio:  backpressure min %u, max %u, average %lf\n"),
			m_dwMinBackPressure, m_dwMaxBackPressure,
			(double) m_uCumulativeBackPressure / (double) m_llNumDriftSamples));
	}

	m_uCumulativeBackPressure = 0;
	m_dwMaxBackPressure = 0;
	m_dwMinBackPressure = 0x7fffffff;

	m_llNumDriftSamples = 0;
} // AudioSyncFilter::ReportLipSyncStats

void AudioSyncFilter::AddLipSyncDataPoint(REFERENCE_TIME rtSampleStart, REFERENCE_TIME rtStream, unsigned uBackPressureMeasure)
{
	// First compare to our target:

	LONGLONG rtMillisecToCompensateFor = m_rtRenderingOffsetCurrent + GetWeightedTimeDelta();
	LONGLONG rtMillisecLeadAdjustedForBuffers = rtMillisecToCompensateFor 
			- ((LONGLONG)(uBackPressureMeasure * 32));
	m_llCumAdjDrift += rtMillisecLeadAdjustedForBuffers;

	LONGLONG llSampleDrift = (rtSampleStart + m_rtRenderingOffsetCurrent * 10000LL - rtStream - 10000LL * g_rtDesiredLeadTimeMillisec);
	m_llCumulativeDrift += llSampleDrift;
	if (llSampleDrift > m_llMaximumDrift)
		m_llMaximumDrift = llSampleDrift;
	if (llSampleDrift < m_llMinimumDrift)
		m_llMinimumDrift = llSampleDrift;

	++m_llNumDriftSamples;
} // AudioSyncFilter::AddLipSyncDataPoint

#endif ENABLE_DIAGNOSTIC_OUTPUT

#ifdef CTV_ENABLE_TESTABILITY

/* IAudioQA implementation: */

#define MAX_SAMPLES_UNTIL_STABLE	10

inline static DWORD TICK_INTERVAL(DWORD dwTimeNow, DWORD dwStartTime)
{
	DWORD dwInterval;

	if (dwTimeNow < dwStartTime)
		dwInterval = 0xffffffff - (dwStartTime - dwTimeNow - 1);
	else
		dwInterval = dwTimeNow - dwStartTime;
	return dwInterval;
} // TICK_INTERVAL

HRESULT AudioSyncFilter::GetAudioStats(/* [out] */AUDIO_QA_STATS *pAudioQAStats)
{
	if (!pAudioQAStats)
		return E_POINTER;
	CheckForIntervalEnd(GetTickCount());
	*pAudioQAStats = m_qaStatsLastInterval;
	return S_OK;
} // AudioSyncFilter::GetAudioStats

HRESULT AudioSyncFilter::SetAudioSampleInterval(/* [in] */ DWORD dwSystemTicksAccumulateData)
{
	m_dwQAStatsInterval = dwSystemTicksAccumulateData;
	return S_OK;
} // AudioSyncFilter::SetAudioSampleInterval

HRESULT AudioSyncFilter::GetAudioSampleInterval(/* [out] */ DWORD *pdwSystemTicksAccumulateData)
{
	if (!pdwSystemTicksAccumulateData)
		return E_POINTER;
	*pdwSystemTicksAccumulateData = m_dwQAStatsInterval;
	return S_OK;
} // AudioSyncFilter::GetAudioSampleInterval

HRESULT AudioSyncFilter::SetAudioQualityCallback(/* [in] */ PFN_AUDIO_QUALITY_CALLBACK pfnAudioQualityCallback)
{
	m_pfnAudioQualityCallback = pfnAudioQualityCallback;
	return S_OK;
} // AudioSyncFilter::SetAudioQualityCallback

void AudioSyncFilter::CollectQAStats(HRESULT hr, REFERENCE_TIME rtDelta)
{
	AUDIO_QA_EVENT_TYPE eAudioQAEventType = AUDIO_QA_EVENT_QUALITY_CHANGE;

	// Step 1:  factor the newly arrived sample into the statistics:

	++m_dwAudioSampleCount;

	bool fInvokeCallback = false;
	DWORD dwNow = GetTickCount();

	if (m_qaStatsInProgress.fExternalMute ||
		m_qaStatsInProgress.fPaused ||
		m_qaStatsInProgress.fStopped ||
		m_qaStatsInProgress.fFlushing ||
		(m_dblRenderingRate != 1.0))
	{
		++m_qaStatsInProgress.uSamplesNotPlayable;
	}
	else if (m_qaStatsInProgress.fInternalMute)
	{
		if ((m_eBackPressureMode == BUILDING_BACKPRESSURE) ||
			(m_dwCountDownToExpectedStability > 0))
		{
			++m_qaStatsInProgress.uSamplesMutedDuringTransition;
		}
		else
		{
			++m_qaStatsInProgress.uSamplesMutedOtherTimes;
			fInvokeCallback = true;
			eAudioQAEventType = AUDIO_QA_EVENT_ABNORMAL_MUTE;
		}
	}
	else
	{
		if (FAILED(hr))
		{
			++m_qaStatsInProgress.uSamplesDropped;
			fInvokeCallback = true;
			eAudioQAEventType = AUDIO_QA_EVENT_DROPPED_SAMPLE;
		}
		else
		{
			++m_qaStatsInProgress.uSamplesRendered;
		}
	}

	m_dwSampleStartTick = dwNow;
	INT32 iLeadLag = (INT32) (rtDelta + m_rtRenderingOffsetCurrent);
	if (m_qaStatsInProgress.iMillisecMinimumLeadLag > iLeadLag)
		m_qaStatsInProgress.iMillisecMinimumLeadLag = iLeadLag;
	if (m_qaStatsInProgress.iMillisecMaximumLeadLag < iLeadLag)
		m_qaStatsInProgress.iMillisecMaximumLeadLag = iLeadLag;
	m_llMillisecAccumulatedLeadLag += iLeadLag;

	if (fInvokeCallback && m_pfnAudioQualityCallback)
	{
		(*m_pfnAudioQualityCallback)(eAudioQAEventType, &m_qaStatsInProgress);
	}

	// Step 2:  decide if it is time to flush the statistics into
	//          saved data
	CheckForIntervalEnd(dwNow);
} // AudioSyncFilter::CollectQAStats

void AudioSyncFilter::CheckForIntervalEnd(DWORD dwNow)
{
	DWORD dwTimeSinceIntervalStart = TICK_INTERVAL(dwNow, m_dwQAStatsIntervalStartTick);
	if (dwTimeSinceIntervalStart >= m_dwQAStatsInterval)
	{
		if (m_dwAudioSampleCount)
			m_qaStatsInProgress.iMillisecAverageLeadLag = (INT32)
				(m_llMillisecAccumulatedLeadLag / m_dwAudioSampleCount);
		else
			m_qaStatsInProgress.iMillisecAverageLeadLag = 0;

		if (m_fDoingNormalPlay)
		{
			m_qaStatsInProgress.uMillisecNormalPlay += TICK_INTERVAL(dwNow, m_dwNormalPlayStartTick);
			m_dwNormalPlayStartTick = dwNow;
		}

		m_qaStatsInProgress.eQualitySummary = AUDIO_QA_QUALITY_UNKNOWN;

		bool fIsEarly = false;
		bool fIsLate = false;
		bool fIsVeryEarly = false;
		bool fIsVeryLate = false;

		if (m_qaStatsInProgress.iMillisecMinimumLeadLag < -350)
			fIsVeryLate = true;
		else if (m_qaStatsInProgress.iMillisecMinimumLeadLag < -275)
			fIsLate = true;

		if (m_qaStatsInProgress.iMillisecMaximumLeadLag > 150)
			fIsVeryEarly = true;
		else if (m_qaStatsInProgress.iMillisecMaximumLeadLag > 75)
			fIsEarly = true;

		if (m_qaStatsInProgress.iMillisecAverageLeadLag < -150)
			fIsVeryLate = true;
		else if (m_qaStatsInProgress.iMillisecAverageLeadLag < -50)
			fIsLate = true;
		else if (m_qaStatsInProgress.iMillisecAverageLeadLag > 150)
			fIsVeryEarly = true;
		else if (m_qaStatsInProgress.iMillisecAverageLeadLag > 50)
			fIsEarly = true;

		if (m_fDoingNormalPlay &&
			(TICK_INTERVAL(dwNow, m_dwSampleStartTick) > 100))
		{
			m_qaStatsInProgress.eQualitySummary = AUDIO_QA_QUALITY_WEDGED;
		}
		else if ((m_qaStatsInProgress.uMillisecNormalPlay < 500) ||
				 (m_dwAudioSampleCount < 5))
		{
			// We don't have enough data to accurately evaluate quality:
			m_qaStatsInProgress.eQualitySummary = AUDIO_QA_QUALITY_UNKNOWN;
		}
		else if (fIsVeryLate && fIsVeryEarly)
		{
			m_qaStatsInProgress.eQualitySummary = AUDIO_QA_QUALITY_VERY_UNSTABLE;
		}
		else if (fIsVeryLate)
		{
			m_qaStatsInProgress.eQualitySummary = AUDIO_QA_QUALITY_VERY_LATE;
		}
		else if (fIsVeryEarly)
		{
			m_qaStatsInProgress.eQualitySummary = AUDIO_QA_QUALITY_VERY_EARLY;
		}
		else if (fIsLate && fIsEarly)
		{
			m_qaStatsInProgress.eQualitySummary = AUDIO_QA_QUALITY_UNSTABLE;
		}
		else if (fIsLate)
		{
			m_qaStatsInProgress.eQualitySummary = AUDIO_QA_QUALITY_LATE;
		}
		else if (fIsEarly)
		{
			m_qaStatsInProgress.eQualitySummary = AUDIO_QA_QUALITY_EARLY;
		}
		else
		{
			m_qaStatsInProgress.eQualitySummary = AUDIO_QA_QUALITY_GOOD;
		}

		if (m_qaStatsInProgress.eQualitySummary != m_qaStatsLastInterval.eQualitySummary)
		{
			if (m_pfnAudioQualityCallback)
			{
				(*m_pfnAudioQualityCallback)(AUDIO_QA_EVENT_QUALITY_CHANGE, &m_qaStatsInProgress);
			}
		}

		m_qaStatsLastInterval = m_qaStatsInProgress;
		m_dwQAStatsIntervalStartTick = dwNow;

		ClearQAStats();
	}
} // AudioSyncFilter::CheckForIntervalEnd

void AudioSyncFilter::ClearQAStats()
{
	m_dwAudioSampleCount = 0;
	m_llMillisecAccumulatedLeadLag = 0LL;
	m_qaStatsInProgress.uMillisecNormalPlay = 0;
	m_qaStatsInProgress.uSamplesRendered = 0;
	m_qaStatsInProgress.uSamplesDropped = 0;
	m_qaStatsInProgress.uSamplesNotPlayable = 0;
	m_qaStatsInProgress.uSamplesMutedDuringTransition = 0;
	m_qaStatsInProgress.uSamplesMutedOtherTimes = 0;
	m_qaStatsInProgress.uPossibleAudioPops = 0;
	m_qaStatsInProgress.iMillisecMinimumLeadLag = INT_MAX;
	m_qaStatsInProgress.iMillisecMaximumLeadLag = INT_MIN;
	m_qaStatsInProgress.iMillisecAverageLeadLag = 0;
	m_qaStatsInProgress.uMillisecSinceLastSample = 0;
	m_qaStatsInProgress.uEventFlags = 0;

} // AudioSyncFilter::ClearQAStats

void AudioSyncFilter::OnQAStatEvent(QAStatEvent eQAStatEvent)
{
	bool fInvokeCallback = false;
	AUDIO_QA_EVENT_TYPE eAudioQAEventType = AUDIO_QA_EVENT_QUALITY_CHANGE;
	DWORD dwNow = GetTickCount();

	switch (eQAStatEvent)
	{
	case QA_EVENT_CREATE_FILTER:
		m_qaStatsInProgress.uEventFlags |= AUDIO_QA_EVENT_GRAPH_BUILT;
		m_qaStatsInProgress.fStopped = true;
		m_fDoingNormalPlay = false;
		m_dwCountDownToExpectedStability = MAX_SAMPLES_UNTIL_STABLE;
		break;

	case QA_EVENT_STOP_FILTER:
		m_qaStatsInProgress.uEventFlags |= AUDIO_QA_EVENT_STOPPED;
		m_qaStatsInProgress.fStopped = true;
		m_qaStatsInProgress.fPaused = false;
		m_dwCountDownToExpectedStability = MAX_SAMPLES_UNTIL_STABLE;
		break;

	case QA_EVENT_PAUSE_FILTER:
		m_qaStatsInProgress.uEventFlags |= AUDIO_QA_EVENT_PAUSED;
		m_qaStatsInProgress.fStopped = true;
		m_qaStatsInProgress.fPaused = false;
		m_dwCountDownToExpectedStability = MAX_SAMPLES_UNTIL_STABLE;
		break;

	case QA_EVENT_RUN_FILTER:
		m_qaStatsInProgress.fStopped = false;
		m_qaStatsInProgress.fPaused = false;
		break;

	case QA_EVENT_RATE_CHANGE:
		// Nothing special here -- UpdateNormalPlay() will take care of everything
		break;

	case QA_EVENT_BEGIN_FLUSH:
		m_dwCountDownToExpectedStability = MAX_SAMPLES_UNTIL_STABLE;
		m_qaStatsInProgress.fFlushing = true;
		m_qaStatsInProgress.uEventFlags |= AUDIO_QA_EVENT_FLUSHED;
		break;

	case QA_EVENT_END_FLUSH:
		m_dwCountDownToExpectedStability = MAX_SAMPLES_UNTIL_STABLE;
		m_qaStatsInProgress.fFlushing = false;
		m_qaStatsInProgress.uEventFlags |= AUDIO_QA_EVENT_FLUSHED;
		break;

	case QA_EVENT_BEGIN_EXTERNAL_MUTE:
		m_dwCountDownToExpectedStability = MAX_SAMPLES_UNTIL_STABLE;
		m_qaStatsInProgress.fExternalMute = true;
		break;

	case QA_EVENT_END_EXTERNAL_MUTE:
		m_dwCountDownToExpectedStability = MAX_SAMPLES_UNTIL_STABLE;
		m_qaStatsInProgress.fExternalMute = false;
		break;

	case QA_EVENT_BEGIN_INTERNAL_MUTE:
		fInvokeCallback = ((m_eBackPressureMode != BUILDING_BACKPRESSURE) &&
							(m_dwCountDownToExpectedStability == 0));
		eAudioQAEventType = AUDIO_QA_EVENT_ABNORMAL_MUTE;
		m_qaStatsInProgress.fInternalMute = true;
		break;

	case QA_EVENT_END_INTERNAL_MUTE:
		m_qaStatsInProgress.fInternalMute = false;
		break;

	case QA_EVENT_BUFFERS_RAN_DRY:
		if (!m_qaStatsInProgress.fExternalMute &&
			!m_qaStatsInProgress.fInternalMute &&
			!m_qaStatsInProgress.fPaused &&
			!m_qaStatsInProgress.fStopped &&
			!m_qaStatsInProgress.fFlushing &&
			(m_dblRenderingRate == 1.0) &&
			!m_fFirstSampleSinceFlush)
		{
			++m_qaStatsInProgress.uPossibleAudioPops;
			fInvokeCallback = true;
			eAudioQAEventType = AUDIO_QA_EVENT_AUDIO_POP;
		}
		break;

	case QA_EVENT_PAUSE_RECOVERY:
		m_qaStatsInProgress.uEventFlags |= AUDIO_QA_EVENT_PAUSE_RECOVERY;
		break;

	default:
		ASSERT(0);
		break;
	}

	bool fDoingNormalPlay =
			!m_qaStatsInProgress.fExternalMute &&
			!m_qaStatsInProgress.fInternalMute &&
			!m_qaStatsInProgress.fPaused &&
			!m_qaStatsInProgress.fStopped &&
			!m_qaStatsInProgress.fFlushing &&
			(m_dblRenderingRate == 1.0);
	if (m_fDoingNormalPlay && !fDoingNormalPlay)
	{
		m_qaStatsInProgress.uMillisecNormalPlay += TICK_INTERVAL(dwNow, m_dwNormalPlayStartTick);
	}
	else if (!m_fDoingNormalPlay && fDoingNormalPlay)
	{
		m_dwNormalPlayStartTick = dwNow;
	}
	m_fDoingNormalPlay = fDoingNormalPlay;

	if (fInvokeCallback && m_pfnAudioQualityCallback)
	{
		(*m_pfnAudioQualityCallback)(eAudioQAEventType, &m_qaStatsInProgress);
	}
} // AudioSyncFilter::OnQAStatEvent

void AudioSyncFilter::InitQAStats()
{
	m_dwQAStatsInterval = 0;
	m_dwSampleStartTick = GetTickCount();
	m_dwAudioSampleCount = 0;
	m_llMillisecAccumulatedLeadLag = 0LL;
	m_dwCountDownToExpectedStability = MAX_SAMPLES_UNTIL_STABLE;
	m_fDoingNormalPlay = false;
	m_dwNormalPlayStartTick = 0;
	m_pfnAudioQualityCallback;

	memset(&m_qaStatsLastInterval, 0, sizeof(m_qaStatsLastInterval));
	m_qaStatsLastInterval.eQualitySummary = AUDIO_QA_QUALITY_UNKNOWN;
	m_qaStatsLastInterval.fStopped = true;
	m_qaStatsLastInterval.iMillisecMinimumLeadLag = INT_MAX;
	m_qaStatsLastInterval.iMillisecMaximumLeadLag = INT_MIN;

	m_qaStatsInProgress = m_qaStatsLastInterval;
	m_dwQAStatsIntervalStartTick = GetTickCount();
} // AudioSyncFilter::InitQAStats

#endif // CTV_ENABLE_TESTABILITY

CAudioSyncInputPin::CAudioSyncInputPin(TCHAR *pObjectName,
						CTransInPlaceFilter *pFilter,
						HRESULT *phr,
						LPCWSTR pName)
	: CTransInPlaceInputPin(pObjectName, pFilter, phr, pName)
	, m_lInFlushCount(0)
{
}

STDMETHODIMP CAudioSyncInputPin::BeginFlush(void)
{
	HRESULT hr = S_OK;

    CAutoLock lck(m_pLock);
	if (m_lInFlushCount == 0)
	{
		hr = CTransInPlaceInputPin::BeginFlush();
	}
	if (SUCCEEDED(hr))
		++m_lInFlushCount;
	return hr;
} // CAudioSyncOutputPin::BeginFlush

STDMETHODIMP CAudioSyncInputPin::EndFlush(void)
{
	HRESULT hr = S_OK;

    CAutoLock lck(m_pLock);
	ASSERT(m_lInFlushCount > 0);
	if (m_lInFlushCount > 0)
	{
		--m_lInFlushCount;
		if (m_lInFlushCount == 0)
		{
			hr = CTransInPlaceInputPin::EndFlush();
		}
	}
	return hr;
} // CAudioSyncOutputPin::EndFlush

// IKsPropertySet implementation
STDMETHODIMP AudioSyncFilter::Set(REFGUID guidPropSet, DWORD dwPropID,
								  LPVOID pInstanceData, DWORD cbInstanceData,
								  LPVOID pPropData, DWORD cbPropData)
{
    // Don't lock if we have no particular worries about race conditions here

    if (PVR_AUDIO_PROPSETID_LipSyncNegotiation == guidPropSet)
    {
		// But here we do have particular worries:

		CAutoLock cTransformLock(&m_cCritSecTransform);

        switch (dwPropID)
        {
		case PVR_AUDIO_PROPID_BeginFrameAdvance:
			DbgLog((LOG_HEURISTIC, 0, TEXT("AUDIO:  beginning frame-advance\n") ));
#ifdef ENABLE_DIAGNOSTIC_OUTPUT
			if (m_eAudioSyncFrameStepMode == AUDIO_SYNC_FRAME_STEP_ACTIVE)
			{
				OutputDebugString(TEXT("AUDIO:  fyi - beginning frame-advance before the prior frame-advance ends\n"));
			}
#endif // ENABLE_DIAGNOSTIC_OUTPUT
			if ((m_State != State_Paused) || (m_eAudioSyncFrameStepMode == AUDIO_SYNC_FRAME_STEP_ACTIVE))
			{
				DbgLog((LOG_HEURISTIC, 2, TEXT("AUDIO:  refusing frame-advance\n" )));
				return E_FAIL;
			}

			m_eAudioSyncFrameStepMode = AUDIO_SYNC_FRAME_STEP_ACTIVE;
			break;

		case PVR_AUDIO_PROPID_RenderedFrameWithOffset:
			{
				// rate change command, check params
				if ((pPropData == NULL) || (sizeof(LONGLONG) != cbPropData))
				{
					return E_POINTER;
				}
				ASSERT(m_eAudioSyncFrameStepMode == AUDIO_SYNC_FRAME_STEP_ACTIVE);
				if (m_eAudioSyncFrameStepMode != AUDIO_SYNC_FRAME_STEP_ACTIVE)
					return E_FAIL;

				LONGLONG llSampleStartTime = *((LONGLONG *) pPropData);

				DbgLog((LOG_HEURISTIC, 0, TEXT("AUDIO:  video received frame at %I64d during frame-advance\n"),
								llSampleStartTime ));

				// The video sample time needs to be transformed into a lead/lag figure that is computed
				// versus the current stream time:

				REFERENCE_TIME rtStream = GetStreamTime();
				LONGLONG hyVideoRenderingLeadLag = llSampleStartTime - rtStream;

				// Now compute what the offset would have to be to make an audio sample with the
				// sample time-stamp arriving right now be on-time:

				LONGLONG hyNewRenderingOffset = g_rtDesiredLeadTimeMillisec - (hyVideoRenderingLeadLag) / 10000LL;

				if (hyNewRenderingOffset != m_rtRenderingOffsetCurrent)
				{
					DbgLog((LOG_LIPSYNCSTATS, 2, TEXT("AudioSyncFilter::Set() -- tracking video rendering (%I64d), offset %I64d -> %I64d\n"),
								llSampleStartTime, m_rtRenderingOffsetCurrent, hyNewRenderingOffset ));

					// We're going to fib about the mode because we want to let the offset float freely:

					BackPressureMode eBackPressureModeOriginal = m_eBackPressureMode;
					m_eBackPressureMode = BUILDING_BACKPRESSURE;
					SetRenderingOffset(hyNewRenderingOffset);  // we want to update both target and current
					m_eBackPressureMode = eBackPressureModeOriginal;
				}
			}
			break;

		case PVR_AUDIO_PROPID_EndFrameAdvance:
			DbgLog((LOG_HEURISTIC, 0, TEXT("AUDIO:  ending frame-advance\n") ));
			ASSERT(m_eAudioSyncFrameStepMode == AUDIO_SYNC_FRAME_STEP_ACTIVE);
			if (m_eAudioSyncFrameStepMode == AUDIO_SYNC_FRAME_STEP_ACTIVE)
			{
				switch (m_State)
				{
				case State_Running:
					m_eAudioSyncFrameStepMode = AUDIO_SYNC_FRAME_STEP_WAITING_FOR_RUN_TO_END;
					DbgLog((LOG_HEURISTIC, 0, TEXT("AUDIO:  new state is waiting for run to end\n") ));
					break;
				case State_Paused:
					m_eAudioSyncFrameStepMode = AUDIO_SYNC_FRAME_STEP_WAITING_FOR_PAUSE_TO_END;
					DbgLog((LOG_HEURISTIC, 0, TEXT("AUDIO:  new state is waiting for pause to end\n") ));
					break;
				default:
					m_eAudioSyncFrameStepMode = AUDIO_SYNC_FRAME_STEP_NONE;
					DbgLog((LOG_HEURISTIC, 0, TEXT("AUDIO:  new state is no frame stepping\n") ));
					break;
				}
			}
			break;

		default:
			return E_PROP_ID_UNSUPPORTED;
		}
    }
	else if (PVR_AUDIO_PROPSETID_LiveTV == guidPropSet)
    {
		// We also need to lock here because we may update the offset
		// from live and target rendering offset:

		CAutoLock cTransformLock(&m_cCritSecTransform);

        switch (dwPropID)
        {
		case PVR_AUDIO_PROPID_LiveTVSourceJitter:
			{
				if ((pPropData == NULL) ||
					(sizeof(PVR_LIVE_TV_AUDIO_JITTER_CATEGORY) != cbPropData))
				{
					return E_POINTER;
				}
				PVR_LIVE_TV_AUDIO_JITTER_CATEGORY eLiveTVJitterCategory =
					*((PVR_LIVE_TV_AUDIO_JITTER_CATEGORY *)pPropData);
				LONGLONG hySafetyMargin = 0LL;

				switch (eLiveTVJitterCategory)
				{
				case PVR_LIVE_TV_AUDIO_JITTER_NORMAL:
					DbgLog((LOG_LIPSYNCSTATS, 3,
							TEXT("AudioSyncFilter::Set():  setting live tv jitter category to normal\n") ));

					hySafetyMargin = g_AdjustmentWhenFixingOffsetNormal;
					break;

				case PVR_LIVE_TV_AUDIO_JITTER_SEVERE:
					DbgLog((LOG_LIPSYNCSTATS, 3,
							TEXT("AudioSyncFilter::Set():  setting live tv jitter category to severe\n") ));

					hySafetyMargin = g_AdjustmentWhenFixingOffsetSevere;
					break;

				case PVR_LIVE_TV_AUDIO_JITTER_MODERATE:
					DbgLog((LOG_LIPSYNCSTATS, 3, 
							TEXT("AudioSyncFilter::Set():  setting live tv jitter category to moderate\n") ));

					hySafetyMargin = g_AdjustmentWhenFixingOffsetModerate;
					break;

				default:
					return E_INVALIDARG;
				}
				if (m_piStreamBufferPlayback)
				{
					m_piStreamBufferPlayback->SetDesiredOffsetFromLive(hySafetyMargin * 10000LL); // hySafetyMargin is measured in milliseconds
				}
				if (eLiveTVJitterCategory != m_eLiveTVJitterCategory)
				{
					m_eLiveTVJitterCategory = eLiveTVJitterCategory;
					m_rtLiveSafetyMargin = GetSafetyMargin(m_eLiveTVJitterCategory);
					if ((m_eBackPressureMode == NORMAL_BACKPRESSURE) &&
						(m_eLiveVersusBufferedState == KNOWN_TO_BE_LIVE_DATA))
					{
						// We are in normal operation mode so this set routine is responsible
						// for updating the target offset

						// Compute our minimum safe offset:

						LONGLONG hyMinimumOffset = hySafetyMargin;
						if (m_rtOffsetFromLive >= 0)
						{
							hyMinimumOffset = hySafetyMargin - m_rtOffsetFromLive;
						}
						if (m_rtRenderingOffsetTarget < hyMinimumOffset)
						{
							DbgLog((LOG_HEURISTIC, 3,
								TEXT("AudioSyncFilter::Set():  updating target offset from %I64d to %I64d\n"),
								m_rtRenderingOffsetTarget, hyMinimumOffset ));

							m_rtRenderingOffsetTarget = hyMinimumOffset;
						}
					}
				}
			}
			break;

		default:
			return E_PROP_ID_UNSUPPORTED;
		}
    }
    else if (AM_KSPROPSETID_TSRateChange == guidPropSet)
    {
        if (AM_RATE_SimpleRateChange == dwPropID)
        {
            // rate change command, check params
            if ((pPropData == NULL) || (sizeof(AM_SimpleRateChange) != cbPropData))
            {
                return E_POINTER;
            }

            AM_SimpleRateChange *pProp = (AM_SimpleRateChange *) pPropData;
            double              dSpeed = (double)10000 / (double)pProp->Rate;

            DbgLog((LOG_LIPSYNCSTATS, 0, TEXT("#### AudioSync got rate change command, rate: %d ####"), pProp->Rate));

			// Empirically:  it is possible for the downstream rendering to wedge
			// when the rate returns to 1.0 so start up the watchdog when enterring
			// rate 1.0 and kill it when leaving rate 1.0:

			if ((dSpeed != m_dblRenderingRate) && (m_State == State_Running))
			{
				DestroyBadResumeRecoveryThread();
				if (dSpeed == 1.0)
				{
					HRESULT hrStartWatchdog = CreateBadResumeRecoveryThread();
					if (FAILED(hrStartWatchdog))
					{
						DbgLog((LOG_ERROR, 1, TEXT("#### AudioSync failed to start monitoring for downstream audio wedges, error 0x%x ####\n"),
							hrStartWatchdog ));
					}
				}
			}
            m_dblRenderingRate = dSpeed;
#ifdef CTV_ENABLE_TESTABILITY
			OnQAStatEvent(QA_EVENT_RATE_CHANGE);
#endif // CTV_ENABLE_TESTABILITY
        }
		else
		{
            return E_PROP_ID_UNSUPPORTED;
		}
    }
    else
    {
        return E_PROP_SET_UNSUPPORTED;
    }

    return S_OK ;
} // AudioSyncFilter::Set

STDMETHODIMP AudioSyncFilter::Get(REFGUID guidPropSet, DWORD dwPropID,
								  LPVOID pInstanceData, DWORD cbInstanceData,
								  LPVOID pPropData, DWORD cbPropData, DWORD *pcbReturned)
{
    HRESULT hr = E_PROP_SET_UNSUPPORTED;

	if (PVR_AUDIO_PROPSETID_LiveTV == guidPropSet)
    {
        switch (dwPropID)
        {
		case PVR_AUDIO_PROPID_LiveTVSourceJitter:
			if (pcbReturned)
				*pcbReturned = sizeof(PVR_LIVE_TV_AUDIO_JITTER_CATEGORY);
            hr = S_OK;
			if (pPropData && (cbPropData < sizeof(PVR_LIVE_TV_AUDIO_JITTER_CATEGORY)))
			{
				hr = E_INVALIDARG;
			}
			else if (pPropData)
			{
				*((PVR_LIVE_TV_AUDIO_JITTER_CATEGORY *)pPropData) = m_eLiveTVJitterCategory;
			}
			break;

		default:
			hr = E_PROP_ID_UNSUPPORTED;
		}
    }
	return hr;
} // AudioSyncFilter::Get

STDMETHODIMP AudioSyncFilter::QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport)
{
    HRESULT hr = E_PROP_SET_UNSUPPORTED;

    if (PVR_AUDIO_PROPSETID_LipSyncNegotiation == guidPropSet)
    {
        switch (dwPropID)
        {
		case PVR_AUDIO_PROPID_BeginFrameAdvance:
		case PVR_AUDIO_PROPID_RenderedFrameWithOffset:
		case PVR_AUDIO_PROPID_EndFrameAdvance:
            if (pTypeSupport)
            {
                *pTypeSupport = KSPROPERTY_SUPPORT_SET;
            }
            hr = S_OK;
			break;

		default:
			hr = E_PROP_ID_UNSUPPORTED;
		}
    }
    else if (PVR_AUDIO_PROPSETID_LiveTV == guidPropSet)
    {
        switch (dwPropID)
        {
		case PVR_AUDIO_PROPID_LiveTVSourceJitter:
            if (pTypeSupport)
            {
                *pTypeSupport = KSPROPERTY_SUPPORT_SET | KSPROPERTY_SUPPORT_GET;
            }
            hr = S_OK;
			break;

		default:
			hr = E_PROP_ID_UNSUPPORTED;
		}
    }
    else if (AM_KSPROPSETID_TSRateChange == guidPropSet)
    {
        if (AM_RATE_SimpleRateChange == dwPropID)
        {
            if (pTypeSupport)
            {
                *pTypeSupport = KSPROPERTY_SUPPORT_SET;
            }
            hr = S_OK;
        }
        else
        {
            hr = E_PROP_ID_UNSUPPORTED;
        }
    }
	return hr;
} // AudioSyncFilter::QuerySupported

LONGLONG AudioSyncFilter::GetRecommendedOffset(bool fCalledFromSetOffset, bool fComputeCurrent, LONGLONG hyInputOffset)
{
	// If we are done building, then the only thing we are allowed to do is bump the current
	// offset towards the target offset:

	if (m_eBackPressureMode == NORMAL_BACKPRESSURE)
	{
		if (fCalledFromSetOffset && fComputeCurrent && (m_rtRenderingOffsetCurrent != m_rtRenderingOffsetTarget))
		{
			if (m_uSamplesUntilCanIncrement > 0)
				--m_uSamplesUntilCanIncrement;

			if (!m_uSamplesUntilCanIncrement)
			{
				m_uSamplesUntilCanIncrement = SAMPLES_BETWEEN_INCREMENTS;
				if (m_rtRenderingOffsetCurrent > m_rtRenderingOffsetTarget)
				{
					--m_rtRenderingOffsetCurrent;
				}
				else if (m_rtRenderingOffsetCurrent < m_rtRenderingOffsetTarget)
				{
					++m_rtRenderingOffsetCurrent;
				}
			}
		}
		return fComputeCurrent ? m_rtRenderingOffsetCurrent : m_rtRenderingOffsetTarget;
	}

	// We are either building or transitioning from building to normal.

	// Step 1:  establish the default offset --
	//		if not live:  0
	//
	//		if live and computing current:
	//			g_rtInitialOffsetMillisecNormal (to be reduced by the offset from live)
	//		if live and computing the target:
	//			g_AdjustmentWhenFixingOffset (to be reduced by the offset from live)

	LONGLONG hyDefaultOffset = 0;
	if (m_eLiveVersusBufferedState == KNOWN_TO_BE_LIVE_DATA)
	{
		hyDefaultOffset = (fCalledFromSetOffset && !fComputeCurrent) ? 
			m_rtLiveSafetyMargin : GetInitialSafetyMargin(m_eLiveTVJitterCategory);
#ifdef ENABLE_DIAGNOSTIC_OUTPUT
		if (!fComputeCurrent && (m_eLiveTVJitterCategory != PVR_LIVE_TV_AUDIO_JITTER_NORMAL))
		{
			DbgLog((LOG_HEURISTIC, 3, TEXT("AudioSync::GetRecommendedOffset(%s, TARGET, %I64d) will use default offset %I64d\n"),
				fCalledFromSetOffset ? TEXT("From-Set-Offset") : TEXT("Not-From-Set-Offset"),
				hyInputOffset,
				hyDefaultOffset ));
		}
#endif // ENABLE_DIAGNOSTIC_OUTPUT
	}

	LONGLONG rtOffset = 0;

	// Step 2:  override the default if needed --
	//		if not live:  override if called from SetRenderingOffset() -- use the value suggested
	//
	//		if live:
	//			if called from SetRenderingOffset() and we're allowed to bypass the usual guidance,
	//				adopt the value given by SetRenderingOffset()
	//			compute the minimum offset we can allow and still be in the safe rendering window
	//			if the value is too low, raise it to the safe rendering value

	if (m_eLiveVersusBufferedState == KNOWN_TO_BE_LIVE_DATA)
	{
		// Compute our minimum safe offset:

		LONGLONG hyMinimumOffset = hyDefaultOffset;
		if (m_rtOffsetFromLive >= 0)
		{
			hyMinimumOffset = hyDefaultOffset - m_rtOffsetFromLive;
		}

#ifdef ENABLE_DIAGNOSTIC_OUTPUT
		if (!fComputeCurrent && (m_eLiveTVJitterCategory != PVR_LIVE_TV_AUDIO_JITTER_NORMAL))
		{
			DbgLog((LOG_HEURISTIC, 3, TEXT("AudioSync::GetRecommendedOffset(%s, TARGET, %I64d) will use minimum offset %I64d ms [%I64d ms from live]\n"),
				fCalledFromSetOffset ? TEXT("From-Set-Offset") : TEXT("Not-From-Set-Offset"),
				hyInputOffset,
				hyMinimumOffset, m_rtOffsetFromLive ));
		}
#endif // ENABLE_DIAGNOSTIC_OUTPUT

		if (fCalledFromSetOffset)
		{
			rtOffset = hyInputOffset;
		}
		if (rtOffset < hyMinimumOffset)
			rtOffset = hyMinimumOffset;

#ifdef ENABLE_DIAGNOSTIC_OUTPUT
		if (!fComputeCurrent && (m_eLiveTVJitterCategory != PVR_LIVE_TV_AUDIO_JITTER_NORMAL))
		{
			DbgLog((LOG_HEURISTIC, 3, TEXT("AudioSync::GetRecommendedOffset(%s, TARGET, %I64d) will recommend target offset %I64d ms\n"),
				fCalledFromSetOffset ? TEXT("From-Set-Offset") : TEXT("Not-From-Set-Offset"),
				hyInputOffset,
				rtOffset ));
		}
#endif // ENABLE_DIAGNOSTIC_OUTPUT


		// If we are called from SetRenderingOffset() and are going to
		// set the current offset, top off the count of samples until
		// we can increment:

		if (m_uSamplesUntilCanIncrement > 0)
			--m_uSamplesUntilCanIncrement;

		if (fCalledFromSetOffset && fComputeCurrent)
		{
			m_uSamplesUntilCanIncrement = SAMPLES_BETWEEN_INCREMENTS;
		}
	}
	else
	{
		if (fCalledFromSetOffset)
			rtOffset = hyInputOffset;
	}

	return rtOffset;
} // AudioSyncFilter::GetRecommendedOffset

void AudioSyncFilter::DetermineIfBuffered()
{
	if (m_eLiveVersusBufferedState != UNKNOWN_DATA_BUFFERING)
		return;

	BOOL fIsAtLive = FALSE;
	if (!m_piStreamBufferPlayback)
	{
		m_eLiveVersusBufferedState = KNOWN_TO_BE_BUFFERED_DATA;
		m_rtOffsetFromLive = -1;
		DbgLog((LOG_HEURISTIC, 1, TEXT("AudioSyncFilter::DetermineIfBuffered() ... assuming buffered since no IStreamBufferPlayback\n") ));
	}
	else if (SUCCEEDED(m_piStreamBufferPlayback->IsAtLive(&fIsAtLive)))
	{
		m_eLiveVersusBufferedState = fIsAtLive ? KNOWN_TO_BE_LIVE_DATA : KNOWN_TO_BE_BUFFERED_DATA;

		DbgLog((LOG_HEURISTIC, 1, TEXT("AudioSyncFilter::DetermineIfBuffered() ... known to be %s\n"),
				fIsAtLive ? TEXT("live") : TEXT("buffered") ));
		if (fIsAtLive)
		{
			if (FAILED(m_piStreamBufferPlayback->GetOffsetFromLive(&m_rtOffsetFromLive)))
				m_rtOffsetFromLive = -1;
			else
			{
				m_rtOffsetFromLive = m_rtOffsetFromLive / 10000LL;  // we want this in milliseconds
			}

			DbgLog((LOG_HEURISTIC, 1, TEXT("AudioSyncFilter::DetermineIfBuffered() ... pre-existing offset from live %I64d ms\n"),
				m_rtOffsetFromLive ));
		}
		else
			m_rtOffsetFromLive = -1;
	}
	m_dwLastOffsetFromLiveCheck = GetTickCount();
} // AudioSyncFilter::DetermineIfBuffered


void AudioSyncFilter::RecheckOffsetFromLive(bool fForceCheck)
{
	DWORD dwTicksNow = GetTickCount();

	if (m_rtRenderingOffsetCurrent != m_rtRenderingOffsetTarget)
	{
		// We are not in steady state yet -- wait.

		m_dwLastOffsetFromLiveCheck = dwTicksNow;
		return;
	};

	if (fForceCheck)
	{
	}
	else if ((dwTicksNow >= m_dwLastOffsetFromLiveCheck) &&
		(dwTicksNow < m_dwLastOffsetFromLiveCheck + LIVE_OFFSET_CHECK_INTERVAL))
	{
		// Not time to check yet

		return;
	}

	BOOL fIsAtLive = FALSE;
	if (!m_piStreamBufferPlayback)
	{
		m_eLiveVersusBufferedState = KNOWN_TO_BE_BUFFERED_DATA;
		m_rtOffsetFromLive = -1;
		DbgLog((LOG_HEURISTIC, 1, TEXT("AudioSyncFilter::RecheckOffsetFromLive() ... assuming buffered since no IStreamBufferPlayback\n") ));
	}
	else if (SUCCEEDED(m_piStreamBufferPlayback->IsAtLive(&fIsAtLive)))
	{
		bool fWasAtLive = (m_eLiveVersusBufferedState == KNOWN_TO_BE_LIVE_DATA);
		m_eLiveVersusBufferedState = fIsAtLive ? KNOWN_TO_BE_LIVE_DATA : KNOWN_TO_BE_BUFFERED_DATA;

		DbgLog((LOG_HEURISTIC, 1, TEXT("AudioSyncFilter::RecheckOffsetFromLive() ... known to be %s [was %s]\n"),
				fIsAtLive ? TEXT("live") : TEXT("buffered"),
				fWasAtLive ? TEXT("live") : TEXT("buffered") ));
		if (fIsAtLive)
		{
			LONGLONG hyOldOffsetFromLive = m_rtOffsetFromLive;
			if (FAILED(m_piStreamBufferPlayback->GetOffsetFromLive(&m_rtOffsetFromLive)))
				m_rtOffsetFromLive = -1;
			else
			{
				m_rtOffsetFromLive = m_rtOffsetFromLive / 10000LL;  // we want this in milliseconds
			}

			DbgLog((LOG_HEURISTIC, 1, TEXT("AudioSyncFilter::RecheckOffsetFromLive() ... pre-existing offset from live now %I64d ms, was %I64d ms\n"),
				m_rtOffsetFromLive, hyOldOffsetFromLive ));

			if ((m_rtOffsetFromLive >= 0) &&
				(m_eBackPressureMode == NORMAL_BACKPRESSURE))
			{
				DbgLog((LOG_HEURISTIC, 1, TEXT("AudioSyncFilter::RecheckOffsetFromLive() ... need to adjust our target if now unsafe or excessive\n") ));

				LONGLONG hySafetyMargin = m_rtLiveSafetyMargin;
				LONGLONG hyMinimumOffset = hySafetyMargin - m_rtOffsetFromLive;

				if (hyMinimumOffset > 0)
				{
					LONGLONG hyDeltaTarget = hyMinimumOffset;
					if (hyDeltaTarget > 50)
						hyDeltaTarget = 50;
					DbgLog((LOG_LIPSYNCSTATS, 1, TEXT("AudioSyncFilter::RecheckOffsetFromLive() ... target offset %I64d -> %I64d [at offset %I64d, need %I64d]\n"),
						m_rtRenderingOffsetTarget, m_rtRenderingOffsetTarget + hyDeltaTarget, m_rtOffsetFromLive, hySafetyMargin));

					m_rtRenderingOffsetTarget += hyDeltaTarget;
				}
				else
				{
					DbgLog((LOG_LIPSYNCSTATS, 1, TEXT("AudioSyncFilter::RecheckOffsetFromLive() ... all is well -- padding %I64d [at offset %I64d, need %I64d]\n"),
						- hyMinimumOffset, m_rtOffsetFromLive, hySafetyMargin ));
					if (m_State == State_Running)
					{
						LONGLONG hyDeltaTarget = - hyMinimumOffset;
						if (hyDeltaTarget > 500)
						{
							hyDeltaTarget = (hyDeltaTarget > 550) ? 50 : (hyDeltaTarget - 500);

							DbgLog((LOG_HEURISTIC, 1, TEXT("AudioSyncFilter::RecheckOffsetFromLive() ... reining in the offset by %I64d\n"),
									hyDeltaTarget ));
							m_rtRenderingOffsetTarget -= hyDeltaTarget;
						}
					}
				}
			}
		}
		else
			m_rtOffsetFromLive = -1;
	}
	m_dwLastOffsetFromLiveCheck = GetTickCount();
} // AudioSyncFilter::RecheckOffsetFromLive

void AudioSyncFilter::ReportAVGlitch(AV_GLITCH_EVENT eAVGlitchEvent, long dwParam2)
{
	CComPtr<IMediaEventSink> piMediaEventSink = m_piMediaEventSink;

	if (piMediaEventSink)
	{
		DWORD dwTimeNow = GetTickCount();
		DWORD *pdwLastOccurrance;
		DWORD dwThreshold = AV_GLITCH_THRESHOLD_FOR_REPEATED_NOTICE;

		// Squelch overly frequent complaints == as in repeating in less than 1 second:

		switch (eAVGlitchEvent)
		{
		case AV_GLITCH_AUDIO_DISCARD:
			pdwLastOccurrance = &m_dwDiscardEventTicks;
			break;

		case AV_GLITCH_AUDIO_BLOCK:
			pdwLastOccurrance = NULL;
			break;

		case AV_GLITCH_AUDIO_BAD_TIMESTAMP:
			pdwLastOccurrance = &m_dwBadTSEventTicks;
			break;

		case AV_GLITCH_AUDIO_NO_LIPSYNC:
			pdwLastOccurrance = &m_dwNoSyncEventTicks;
			break;

		case AV_GLITCH_AUDIO_LIPSYNC_DRIFT:
			pdwLastOccurrance = &m_dwPoorSyncEventTicks;
			break;

		case AV_GLITCH_AUDIO_MUTE:
			pdwLastOccurrance = NULL;
			break;

		case AV_NO_GLITCH_AUDIO_IS_ARRIVING:
			pdwLastOccurrance = &m_dwAudioReceiptEventTicks;
			dwThreshold = AV_NO_GLITCH_THRESHOLD_FOR_REPEATED_NOTICE;
			break;

		default:
			ASSERT(0);
			return;
		}

		if (!pdwLastOccurrance)
		{
			// This is an on/off event -- always sent
		}
		else if ((dwTimeNow <= *pdwLastOccurrance) || (dwTimeNow - *pdwLastOccurrance >= dwThreshold))
		{
			*pdwLastOccurrance = dwTimeNow;
		}
		else
		{
			return;
		}

		piMediaEventSink->Notify(eAVGlitchEvent, dwTimeNow, dwParam2);
	}
} // AudioSyncFilter::ReportAVGlitch

