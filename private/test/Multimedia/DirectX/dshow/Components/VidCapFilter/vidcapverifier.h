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
#ifndef _VIDCAP_VERIFIER_H
#define _VIDCAP_VERIFIER_H

#define NUM_VCAP_PINS 3

class CVidCapVerifier : public IVerifier
{
public:
    CVidCapVerifier();
    virtual ~CVidCapVerifier();

    // Overriding IVerifier interface
    virtual void SourceSample(IMediaSample* pSample, AM_MEDIA_TYPE* pMediaType, int pin = 0);
    virtual void SinkSample(IMediaSample* pSample, AM_MEDIA_TYPE* pMediaType, int pin = 0);
    virtual void Reset();

    virtual void SourceBeginFlush(int pin = 0);
    virtual void SinkBeginFlush(int pin = 0);

    virtual void SourceEndFlush(int pin = 0);
    virtual void SinkEndFlush(int pin = 0);

    virtual void SourceNewSegment(int pin = 0);
    virtual void SinkNewSegment(int pin = 0);

    virtual void SourceEndOfStream(int pin = 0);
    virtual void SinkEndOfStream(int pin = 0);

    virtual bool GotSourceBeginFlush(int pin = 0);
    virtual bool GotSourceEndFlush(int pin = 0);
    virtual bool GotSourceNewSegment(int pin = 0);
    virtual bool GotSourceEndOfStream(int pin = 0);
    virtual bool GotSinkBeginFlush(int pin = 0);
    virtual bool GotSinkEndFlush(int pin = 0);
    virtual bool GotSinkNewSegment(int pin = 0);
    virtual bool GotSinkEndOfStream(int pin = 0);
    virtual int GetSamplesSourced(int pin = 0);
    virtual int GetSamplesSinked(int pin = 0);
    virtual int GetSamplesFailed(int pin = 0);


    virtual HRESULT StreamingBegin();
    virtual HRESULT StreamingEnd();

    virtual HRESULT GetTimestampOfFirstFrame(REFERENCE_TIME *rtStart, REFERENCE_TIME *rtStop, int pin = 0);
    virtual HRESULT GetTimestampOfLastFrame(REFERENCE_TIME *rtStart, REFERENCE_TIME *rtStop, int pin = 0);

protected:
    virtual bool VerifySinkMediaType(AM_MEDIA_TYPE* pMediaType, AM_MEDIA_TYPE* pReferenceMediaType = NULL, int pin = 0);
    virtual bool VerifySinkMediaSample(IMediaSample* pMediaSample, int pin = 0);

protected:
    // Have we stored the current sample type?
    bool m_bStoredSinkMediaType[NUM_VCAP_PINS];

    // Propagates the first error encountered.
    HRESULT m_AccumHR;

    // How many samples did we get?
    int m_nSamplesSinked[NUM_VCAP_PINS];
    int m_nSamplesFailed[NUM_VCAP_PINS];

    REFERENCE_TIME m_rtFirstFrameStartTime[NUM_VCAP_PINS];
    REFERENCE_TIME m_rtFirstFrameStopTime[NUM_VCAP_PINS];

    REFERENCE_TIME m_rtLastFrameStartTime[NUM_VCAP_PINS];
    REFERENCE_TIME m_rtLastFrameStopTime[NUM_VCAP_PINS];

    // The media type of the first sinked sample
    AM_MEDIA_TYPE m_SinkMediaType[NUM_VCAP_PINS];

    // Are we currently streaming?
    bool m_bStreaming;
    
    // Did we get a begin flush notification.
    bool m_bSourceBeginFlush[NUM_VCAP_PINS];

    // Did we get a end flush notification.
    bool m_bSourceEndFlush[NUM_VCAP_PINS];

    // Did we get a new segment notification.
    bool m_bSourceNewSegment[NUM_VCAP_PINS];

    // Did we get a end of stream notification.
    bool m_bSourceEndOfStream[NUM_VCAP_PINS];

    // Did we get a begin flush notification.
    bool m_bSinkBeginFlush[NUM_VCAP_PINS];

    // Did we get a end flush notification.
    bool m_bSinkEndFlush[NUM_VCAP_PINS];

    // Did we get a new segment notification.
    bool m_bSinkNewSegment[NUM_VCAP_PINS];

    // Did we get a end of stream notification.
    bool m_bSinkEndOfStream[NUM_VCAP_PINS];
};

#endif
