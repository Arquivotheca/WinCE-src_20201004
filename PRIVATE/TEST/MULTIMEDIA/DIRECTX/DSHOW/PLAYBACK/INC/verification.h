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
////////////////////////////////////////////////////////////////////////////////

#ifndef VERIFICATION_H
#define VERIFICATION_H

#include <map>
#include "Position.h"
#include "GraphEvent.h"
#include "TapFilter.h"
#include "Index.h"
#include "Timer.h"
#include "FrameRecognizer.h"
#include "xmlif.h"

class TestGraph;

// The specific quantities or behavior to be verified
enum VerificationType {
		// Graph Building
	CorrectGraph,
	BuildGraphLatency,
	ChannelChangeLatency,

	// State change
	VerifyStateChangeLatency,

	// Playback, Seeking and Trick-modes
	VerifySampleDelivered,
	VerifyPlaybackDuration,
	StartupLatency,
	VerifyRate,
	VerifyCurrentPosition,
	VerifyStoppedPosition,
	VerifySetPositionLatency,
	VerifyPositionAndRate,

	// Scaling, video window changes
	VerifyScaleChange,
	VerifyRepaintEvent,
	VerifyRepaintedImage,

	// Video
	DecodedVideoFrameData,
	DecodedVideoFirstFrameData,
	DecodedVideoEndFrameData,
	DecodedVideoFrameDataAfterFlush,
	
	DecodedVideoPlaybackTimeStamps,
	DecodedVideoPlaybackFirstFrameTimeStamp,
	DecodedVideoPlaybackEndFrameTimeStamp,
	DecodedVideoSeekAccuracy,
	DecodedVideoTrickModeFirstFrameTimeStamp,
	DecodedVideoTrickModeEndFrameTimeStamp,

	RenderedVideoData,
	RenderedVideoFirstFrameData,
	RenderedVideoEndFrameData,
	RenderedVideoFrameDataAfterFlush,
	
	RenderedVideoTimeStamps,
	RenderedVideoFirstFrameTimeStamp,
	RenderedVideoEndFrameTimeStamp,
	RenderedVideoSeekAccuracy,

	NumDecodedVideoFrames,
	NumRenderedVideoFrames,
	NumDecoderDroppedVideoFrames,
	NumRendererDroppedVideoFrames,
	NumDroppedVideoFrames,
	NumDecodedTrickModeVideoFrames,
	NumRenderedTrickModeVideoFrames,

	DecodedVideoFirstFrameLatency,
	DecodedVideoLatencyRunToFirstSample,
	DecodedVideoLatencyPauseToFirstSample,
	DecodedVideoLatencyStopToPause,
	DecodedVideoLatencyStopToRun,
	DecodedVideoLatencyAfterFlush,
	DecodedVideoLatencyAfterSeek,
	
	RenderedVideoLatencyStopToPause,
	RenderedVideoLatencyStopToRun,
	RenderedVideoLatencyAfterFlush,
	RenderedVideoLatencyAfterSeek,

	VerifyVideoSourceRect,
	VerifyVideoDestRect,
	VerifyBackBuffers,
	VerifyDDrawSamples,

	// Audio
	DecodedAudioFrameData,
	DecodedAudioFirstFrameData,
	DecodedAudioEndFrameData,
	DecodedAudioFrameDataAfterFlush,
	DecodedAudiooTimeStampsAfterSeek,

	
	DecodedAudioTimeStamps,
	DecodedAudioFirstFrameTimeStamp,
	DecodedAudioEndFrameTimeStamp,
	DecodedAudioSeekAccuracy,

	RenderedAudioData,
	RenderedAudioFirstFrameData,
	RenderedAudioEndFrameData,
	RenderedAudioFrameDataAfterFlush,
	
	RenderedAudioTimeStamps,
	RenderedAudioFirstFrameTimeStamp,
	RenderedAudioEndFrameTimeStamp,
	RenderedAudioSeekAccuracy,

	NumRenderedAudioGlitches,
	NumRenderedAudioDiscontinuities,
	DecodedAudioDataTime,
	NumDecodedAudioFrames,
	NumRenderedAudioFrames,
	NumDecoderDroppedAudioFrames,
	NumRendererDroppedAudioFrames,
	NumDroppedAudioFrames,

	DecodedAudioFirstFrameLatency,
	RenderedAudioFirstFrameLatency,
	DecodedAudioLatencyAfterFlush,
	DecodedAudioLatencyAfterSeek,
	RenderedAudioLatencyAfterFlush,

	// IReferenceClock latency verificaiton
	VerifyAdvisePastTimeLatency,
	VerifyAdviseTimeLatency,

	// Renderer verifications
	VerifyVideoRendererMode,
	VerifyVideoRendererSurfaceType,

	// End marker
	VerificationEndMarker

};

// Verification list is a list of <VerificationType, verification data> pairs
typedef map<int, void*> VerificationList;
typedef pair <int, void*> VerificationPair;


enum FrameAccuracy {
	FrameAccurate,
	KeyFrameAccurate
};

// The verifier interface
class IGraphVerifier
{
public:
	virtual ~IGraphVerifier() {};
	virtual HRESULT Init(TestGraph* pTestGraph, VerificationType type, void *pVerificationData) = 0;
	virtual HRESULT GetResult(VerificationType type, void *pVerificationData = NULL, TCHAR* pResultStr = NULL) = 0;
	virtual HRESULT ProcessEvent(GraphEvent event, void* pGraphEventData) = 0;
	virtual HRESULT Start() = 0;
	virtual void Stop() = 0;
	virtual void Reset() = 0;
};


// Factory method to create verifiers given a pointer to verification data
typedef HRESULT (*VerifierFactory)(VerificationType type, void* pVerificationData, TestGraph* pTestGraph, IGraphVerifier** ppGraphVerifier);
HRESULT GenericTapFilterCallback(GraphEvent event, void* pGraphEventData, void* pTapCallbackData, void* pObject);

// Prototype of a verification parser function
typedef HRESULT (*VerificationParserFunction)(HELEMENT hVerification, VerificationType type, VerificationList* pVerificationList);

// Factory method to create verifiers given a pointer to verification data
typedef HRESULT (*VerifierFactory)(VerificationType type, void* pVerificationData, TestGraph* pTestGraph, IGraphVerifier** ppGraphVerifier);

//this is the basic Verifier object. For each verification type, we need the equivalent string name (used for xml parsing too)
// the parser we need to use to parse the verifier information from the xml file
// and the Verifier factory function that can create a new instance of the verifier and give it to us
struct VerifierObj
{
	VerificationType verificationType;
	const char* szVerificationType;
	VerificationParserFunction pVerificationParserFunction;
	VerifierFactory fn;	
};

// Verification data strcutures
// This contains the actual sample that was tapped
struct SampleVerificationData
{
	LONGLONG currpos;
	LONGLONG pts;
	LONGLONG dts;
	LONGLONG ats;
	DWORD buflen;
	BYTE* pBuffer;
};

// This contains the sample time-stamps
struct TimeStampVerificationData
{
	LONGLONG currpos;
	LONGLONG pts;
	LONGLONG dts;
	LONGLONG ats;
};

// This describes the verification request to to the server
struct ServerVerificationRequest
{
	VerificationType type;
	void* pVerificationData;
};

const CHAR* GetVerificationString(VerificationType type);

// Is this verification possible
bool IsVerifiable(VerificationType type);

//This is the verifier manager class. It has a list of all verifiers available to this test module
//there is only one singleton instance of this class, defined in Verification.cpp. Everyone can
//use that to query
class VerifierMgr 
{
	public:
		VerifierMgr() {}
		~VerifierMgr() {}
		static int WINAPI GetNumVerifiers();
		static VerifierObj* GetVerifier(int index);
};

// Factory method for creating verifiers
HRESULT CreateGraphVerifier(VerificationType type, void* pVerificationData, TestGraph* pTestGraph, IGraphVerifier** ppGraphVerifier);

// SampleDeliveryVerifier 
struct SampleDeliveryVerifierData
{
	DWORD filterType;
	PIN_DIRECTION pindir;
	GUID mediaMajorType;
	DWORD tolerance;
	GraphEvent event;
};

struct PlaybackDurationData
{
	LONGLONG firstSampleTime;
	LONGLONG eosTime;
};

#endif
