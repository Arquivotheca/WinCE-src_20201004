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

#ifndef BASIC_VERIFIERS_H
#define BASIC_VERIFIERS_H

#include "Verification.h"
#include "TestGraph.h"

const char* const STR_CorrectGraph = "CorrectGraph";
const char* const STR_VerifySampleDelivered = "VerifySampleDelivered";
const char* const STR_VerifyNumComponents = "VerifyNumComponents";
const char* const STR_PlaybackDuration = "PlaybackDuration";

// Factory method for verifiers
HRESULT CreateSampleDeliveryVerifier(VerificationType type, void* pVerificationData, TestGraph* pTestGraph, IGraphVerifier** ppGraphVerifier);
HRESULT CreatePlaybackDurationVerifier(VerificationType type, void* pVerificationData, TestGraph* pTestGraph, IGraphVerifier** ppGraphVerifier);
HRESULT CreateGraphBuildVerifier(VerificationType type, void* pVerificationData, TestGraph* pTestGraph, IGraphVerifier** ppGraphVerifier);

class SampleDeliveryVerifier : public IGraphVerifier
{
public:
	SampleDeliveryVerifier();
	virtual ~SampleDeliveryVerifier ();
	virtual HRESULT Init(TestGraph* pTestGraph, VerificationType type, void *pVerificationData);
	virtual HRESULT GetResult(VerificationType type, void *pVerificationData = NULL, TCHAR* pResultStr = NULL);
	virtual HRESULT ProcessEvent(GraphEvent event, void* pGraphEventData);
	virtual HRESULT Start();
	virtual void Stop();
	virtual void Reset();

private:
	VerificationType m_VerificationType;
	ITapFilter* m_pTapFilter;
	TestGraph* m_pTestGraph;
	DWORD m_Tolerance;
	GraphEvent m_WaitForEvent;
	bool m_bExpectSample;
	AM_MEDIA_TYPE m_mt;
	HRESULT m_hr;
};

class GraphBuildVerifier : public IGraphVerifier
{
public:
	GraphBuildVerifier();
	virtual ~GraphBuildVerifier();
	virtual HRESULT Init(TestGraph* pTestGraph, VerificationType type, void *pVerificationData);
	virtual HRESULT ProcessEvent(GraphEvent event, void* pGraphEventData);
	virtual HRESULT Start();
	virtual void Stop();
	virtual void Reset();
	virtual HRESULT GetResult(VerificationType type, void *pVerificationData = NULL, TCHAR* pResultStr = NULL);
private:
	VerificationType m_VerificationType;
	TestGraph* m_pTestGraph;
	StringList m_FilterList;
	HRESULT m_hr;
};

class PlaybackDurationVerifier : public IGraphVerifier
{
public:
	PlaybackDurationVerifier();
	virtual ~PlaybackDurationVerifier();
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
	GUID m_MediaType;
	LONGLONG m_FirstSampleTime;
	LONGLONG m_EOSTime;
	bool m_bFirstSample;
};

#endif
