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
//
//  Verifier objects
//
//
////////////////////////////////////////////////////////////////////////////////

#include "logging.h"
#include "globals.h"
#include "TestDesc.h"
#include "TestGraph.h"
#include "PinEnumerator.h"
#include "StringConversions.h"
#include "utility.h"
#include "LatencyVerifiers.h"
#include "TestDescParser.h"

extern "C" {VerifierObj g_LatencyVerifierObjTable[] = 
{
	{VerifyStateChangeLatency, STR_VerifyStateChangeLatency, ParseVerifyDWORD, NULL},
	{VerifySetPositionLatency, STR_VerifySetPositionLatency, ParseVerifyDWORD, NULL},
	{StartupLatency, STR_StartupLatency, ParseVerifyDWORD, CreateStartupLatencyVerifier},
	{BuildGraphLatency, STR_BuildGraphLatency, ParseVerifyDWORD, NULL},	

	//Video Latency

	{DecodedVideoFirstFrameLatency, STR_DecodedVideoFirstFrameLatency, ParseVerifyDWORD, NULL},
	{DecodedVideoLatencyStopToPause, STR_DecodedVideoLatencyStopToPause, ParseVerifyDWORD, CreateDecoderOutputLatencyVerifier},
	{DecodedVideoLatencyStopToRun, "", NULL, CreateDecoderOutputLatencyVerifier},
	{DecodedVideoLatencyAfterFlush, "", NULL, CreateDecoderOutputLatencyVerifier},
	{DecodedVideoLatencyAfterSeek, STR_DecodedVideoLatencyAfterSeek, ParseVerifyDWORD, CreateDecoderOutputLatencyVerifier},
	{DecodedVideoLatencyRunToFirstSample, STR_DecodedVideoLatencyRunToFirstSample, ParseVerifyDWORD, CreateDecoderOutputLatencyVerifier},
	{DecodedVideoLatencyPauseToFirstSample, STR_DecodedVideoLatencyPauseToFirstSample, ParseVerifyDWORD, CreateDecoderOutputLatencyVerifier},

	// Audio Latency

	{DecodedAudioFirstFrameLatency, "", NULL, NULL},
	{RenderedAudioFirstFrameLatency, "", NULL, NULL},
	{DecodedAudioLatencyAfterFlush, "", NULL, NULL},
	{RenderedAudioLatencyAfterFlush, "", NULL, NULL}
};}

int g_LatencyVerifierObjTableSize = sizeof(g_LatencyVerifierObjTable)/sizeof(VerifierObj);
