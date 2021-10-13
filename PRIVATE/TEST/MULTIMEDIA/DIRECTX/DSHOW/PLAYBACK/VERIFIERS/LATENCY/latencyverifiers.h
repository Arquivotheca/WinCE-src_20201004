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

#ifndef LATENCY_VERIFIERS_H
#define LATENCY_VERIFIERS_H

#include "Position.h"
#include "GraphEvent.h"
#include "TapFilter.h"
#include "Index.h"
#include "Timer.h"

const char* const STR_VerifyStateChangeLatency = "VerifyStateChangeLatency";
const char* const STR_StartupLatency = "StartupLatency";
const char* const STR_BuildGraphLatency = "BuildGraphLatency";
const char* const STR_VerifySetPositionLatency = "VerifySetPositionLatency";
const char* const STR_DecodedVideoFirstFrameLatency = "DecodedVideoPlaybackFirstFrameLatency";
const char* const STR_DecodedVideoLatencyAfterSeek = "DecodedVideoLatencyAfterSeek";
const char* const STR_DecodedVideoLatencyStopToPause = "DecodedVideoLatencyStopToPause";
const char* const STR_DecodedVideoLatencyRunToFirstSample = "DecodedVideoLatencyRunToFirstSample";
const char* const STR_DecodedVideoLatencyPauseToFirstSample = "DecodedVideoLatencyPauseToFirstSample";

// Factory method for verifiers
HRESULT CreateDecoderOutputLatencyVerifier(VerificationType type, void* pVerificationData, TestGraph* pTestGraph, IGraphVerifier** ppGraphVerifier);
HRESULT CreateStartupLatencyVerifier(VerificationType type, void* pVerificationData, TestGraph* pTestGraph, IGraphVerifier** ppGraphVerifier);

class DecoderOutputLatencyVerifier : public IGraphVerifier
{
public:
	DecoderOutputLatencyVerifier();
	virtual ~DecoderOutputLatencyVerifier();
	virtual HRESULT Init(TestGraph* pTestGraph, VerificationType type, void *pVerificationData);
	virtual HRESULT GetResult(VerificationType type, void *pVerificationData = NULL, TCHAR* pResultStr = NULL);
	virtual HRESULT ProcessEvent(GraphEvent event, void* pGraphEventData);
	virtual HRESULT Start();
	virtual void Stop();
	virtual void Reset();

private:
	TestGraph* m_pTestGraph;
	VerificationType m_VerificationType;
	HRESULT m_hr;
	Timer m_Timer;
	DWORD m_Tolerance;
	DWORD m_Latency;
	bool m_bStarted;
	bool m_bWaitingForSample;
	bool m_bWaitingForTrigger;
};

class StartupLatencyVerifier : public IGraphVerifier
{
public:
	StartupLatencyVerifier();
	virtual ~StartupLatencyVerifier();
	virtual HRESULT Init(TestGraph* pTestGraph, VerificationType type, void *pVerificationData);
	virtual HRESULT ProcessEvent(GraphEvent event, void* pGraphEventData);
	virtual HRESULT Start();
	virtual void Stop();
	virtual void Reset();
	virtual HRESULT GetResult(VerificationType type, void *pVerificationData = NULL, TCHAR* pResultStr = NULL);
private:
	VerificationType m_VerificationType;
	TestGraph* m_pTestGraph;
	ITapFilter* m_pTapFilter;
	LONGLONG m_Tolerance;
	LONGLONG m_Latency;
	bool m_bWaitingForFirstSample;
	Timer m_Timer;
};


#endif
