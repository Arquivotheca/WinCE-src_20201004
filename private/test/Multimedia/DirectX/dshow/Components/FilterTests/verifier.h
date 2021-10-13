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
#ifndef _VERIFIER_H
#define _VERIFIER_H

// This is a dumb verifier. It will tell you if you got a flush, new segment, end of stream at the sink. 
// It will also tell you how many samples got sourced and sinked. But that's about it. 
// If you are implementing a filter test, you must implement a verifier if you need one.
class CVerifier : public IVerifier
{
public:
	CVerifier();
	virtual ~CVerifier();

	// Notification from test filters
	virtual void SourceSample(IMediaSample* pSample, AM_MEDIA_TYPE* pMediaType, int outpin = 0);
	virtual void SinkSample(IMediaSample* pSample, AM_MEDIA_TYPE* pMediaType, int inpin = 0);

	virtual void SourceBeginFlush(int pin = 0);
	virtual void SinkBeginFlush(int pin = 0);
	
	virtual void SourceEndFlush(int pin = 0);
	virtual void SinkEndFlush(int pin = 0);
	
	virtual void SourceNewSegment(int pin = 0);
	virtual void SinkNewSegment(int pin = 0);
	
	virtual void SourceEndOfStream(int pin = 0);
	virtual void SinkEndOfStream(int pin = 0);

	// Preparation cue from test code
	virtual HRESULT StreamingBegin();
	virtual HRESULT StreamingEnd();
	virtual void Reset();

	// Querying the verifier
	virtual bool GotSourceBeginFlush(int pin = -1);
	virtual bool GotSourceEndFlush(int pin = -1);
	virtual bool GotSourceNewSegment(int pin = -1);
	virtual bool GotSourceEndOfStream(int pin = -1);
	virtual bool GotSinkBeginFlush(int pin = -1);
	virtual bool GotSinkEndFlush(int pin = -1);	
	virtual bool GotSinkNewSegment(int pin = -1);
	virtual bool GotSinkEndOfStream(int pin = -1);
	virtual int GetSamplesSourced(int pin = -1);
	virtual int GetSamplesSinked(int pin = -1);
	virtual int GetSamplesFailed(int pin = -1);

protected:
	// Are we currently streaming?
	bool m_bStreaming;
	
	// Number of source samples got so far.
	int m_nSourceSamples;

	// Number of sink samples got so far.
	int m_nSinkSamples;
	
	// Did we get a source/sink begin flush notification.
	bool m_bSourceBeginFlush;
	bool m_bSinkBeginFlush;

	// Did we get a source/sink end flush notification.
	bool m_bSourceEndFlush;
	bool m_bSinkEndFlush;

	// Did we get a source/sink new segment notification.
	bool m_bSourceNewSegment;
	bool m_bSinkNewSegment;

	// Did we get a source/sink end of stream notification.
	bool m_bSourceEndOfStream;
	bool m_bSinkEndOfStream;
};

#endif
