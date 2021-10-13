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
#include <streams.h>
#include "TestInterfaces.h"
#include "Verifier.h"

CVerifier::CVerifier()
{
	CVerifier::Reset();
}

CVerifier::~CVerifier()
{
}

void CVerifier::Reset()
{
	m_bStreaming = false;
	m_nSourceSamples = m_nSinkSamples = 0;
	m_bSourceBeginFlush = false;
	m_bSinkBeginFlush = false;
	m_bSourceEndFlush = false;
	m_bSinkEndFlush = false;
	m_bSourceNewSegment = false;
	m_bSinkNewSegment = false;
	m_bSourceEndOfStream = false;
	m_bSinkEndOfStream = false;
}

HRESULT CVerifier::StreamingBegin()
{
	if (m_bStreaming)
		return E_FAIL;

	m_bStreaming = true;
	m_nSourceSamples = 0;
	m_nSinkSamples = 0;

	return NOERROR;
}

HRESULT CVerifier::StreamingEnd()
{
	if (!m_bStreaming)
		return E_FAIL;
	m_bStreaming = false;
	// This verifier always returns NOERROR because it is dumb.
	return NOERROR;
}

void CVerifier::SourceSample(IMediaSample* pSample, AM_MEDIA_TYPE* pMediaType, int nOutputPin)
{
	m_nSourceSamples++;
	return;
}

void CVerifier::SinkSample(IMediaSample* pDstSample, AM_MEDIA_TYPE* pMediaType, int nInputSample)
{
	m_nSinkSamples++;
	return;
}


void CVerifier::SourceBeginFlush(int pin)
{
	m_bSourceBeginFlush = true;
}

void CVerifier::SinkBeginFlush(int pin)
{
	m_bSinkBeginFlush = true;
}
	
void CVerifier::SourceEndFlush(int pin)
{
	m_bSourceEndFlush = true;
}

void CVerifier::SinkEndFlush(int pin)
{
	m_bSinkEndFlush = true;
}
	
void CVerifier::SourceNewSegment(int pin)
{
	m_bSourceNewSegment = true;
}

void CVerifier::SinkNewSegment(int pin)
{
	m_bSinkNewSegment = true;
}
	
void CVerifier::SourceEndOfStream(int pin)
{
	m_bSourceEndOfStream = true;
}

void CVerifier::SinkEndOfStream(int pin)
{
	m_bSinkEndOfStream = true;
}

bool CVerifier::GotSourceBeginFlush(int pin)
{
	return m_bSourceBeginFlush;
}

bool CVerifier::GotSourceEndFlush(int pin)
{
	return m_bSourceEndFlush;
}

bool CVerifier::GotSourceEndOfStream(int pin)
{
	return m_bSourceEndOfStream;
}

bool CVerifier::GotSourceNewSegment(int pin)
{
	return m_bSourceNewSegment;
}

bool CVerifier::GotSinkBeginFlush(int pin)
{
	return m_bSinkBeginFlush;
}

bool CVerifier::GotSinkEndFlush(int pin)
{
	return m_bSinkEndFlush;
}

bool CVerifier::GotSinkEndOfStream(int pin)
{
	return m_bSinkEndOfStream;
}

bool CVerifier::GotSinkNewSegment(int pin)
{
	return m_bSinkNewSegment;
}

int CVerifier::GetSamplesSourced(int pin)
{
	return m_nSourceSamples;
}

int CVerifier::GetSamplesSinked(int pin)
{
	return m_nSinkSamples;
}

int CVerifier::GetSamplesFailed(int pin)
{
	// the base streaming verifier doesn't track failures, so 0 failed
	return 0;
}

