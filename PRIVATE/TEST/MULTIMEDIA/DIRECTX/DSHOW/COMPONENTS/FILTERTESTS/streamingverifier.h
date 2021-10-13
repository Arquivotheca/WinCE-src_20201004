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
#ifndef _STREAMING_VERIFIER_H
#define _STREAMING_VERIFIER_H

#include "Verifier.h"

class CStreamingVerifier : public CVerifier
{
public:
	CStreamingVerifier();
	virtual ~CStreamingVerifier();
	
	// Overriding IVerifier interface
	virtual void SourceSample(IMediaSample* pSample, AM_MEDIA_TYPE* pMediaType, int outpin = 0);
	virtual void SinkSample(IMediaSample* pSample, AM_MEDIA_TYPE* pMediaType, int inpin = 0);
	virtual void Reset();
	virtual HRESULT StreamingBegin();
	virtual int GetSamplesFailed(int pin);

protected:
	virtual bool VerifySinkMediaType(AM_MEDIA_TYPE* pMediaType, AM_MEDIA_TYPE* pReferenceMediaType = NULL) = 0;
	virtual bool VerifySinkMediaSample(IMediaSample* pMediaSample) = 0;

protected:
	// Have we stored the current sample type?
	bool m_bStoredSourceMediaType;
	bool m_bStoredSinkMediaType;

	// Propagates the first error encountered.
	HRESULT m_AccumHR;

	// How many samples did we get?
	int m_nSamplesSourced;
	int m_nSamplesSinked;
	int m_nSamplesFailed;

	// The media type of the first sinked sample
	AM_MEDIA_TYPE m_SourceMediaType;
	AM_MEDIA_TYPE m_SinkMediaType;
};

#endif
