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
#include <streams.h>
#include <windows.h>
#include <dshow.h>
#include "TestInterfaces.h"
#include "VidCapVerifier.h"
#include "tuxmain.h"

#define MAXFRAMEDELAY 4

CVidCapVerifier::CVidCapVerifier()
{
    for(int i = 0; i < NUM_VCAP_PINS; i++)
    {
        memset(&m_SinkMediaType[i], 0, sizeof(m_SinkMediaType[i]));
        m_bStoredSinkMediaType[i] = false;
    }
    Reset();
}

CVidCapVerifier::~CVidCapVerifier()
{
    Reset();

    for(int i = 0; i < NUM_VCAP_PINS; i++)
    {
        FreeMediaType(m_SinkMediaType[i]);
        memset(&m_SinkMediaType[i], 0, sizeof(m_SinkMediaType[i]));
        m_bStoredSinkMediaType[i] = false;
    }
}

void CVidCapVerifier::Reset()
{
    m_bStreaming = false;
    
    for(int i = 0; i < NUM_VCAP_PINS; i++)
    {
        m_nSamplesSinked[i] = 0;
        m_nSamplesFailed[i] = 0;
        m_bSinkBeginFlush[i] = false;
        m_bSinkEndFlush[i] = false;    
        m_bSinkNewSegment[i] = false;
        m_bSinkEndOfStream[i] = false;

        m_rtFirstFrameStartTime[i] = 0;
        m_rtFirstFrameStopTime[i] = 0;
        m_rtLastFrameStartTime[i] = 0;
        m_rtLastFrameStopTime[i] = 0;
    }
}

HRESULT CVidCapVerifier::StreamingBegin()
{
    if (m_bStreaming)
        return E_FAIL;

    Reset();

    m_bStreaming = true;

    return NOERROR;
}

HRESULT CVidCapVerifier::StreamingEnd()
{
    if (!m_bStreaming)
        return E_FAIL;

    m_bStreaming = false;

    return NOERROR;
}

void CVidCapVerifier::SourceSample(IMediaSample* pSample, AM_MEDIA_TYPE* pMediaType, int nOutputPin)
{
    // This should never be called
    SetAccumResult(m_AccumHR, E_UNEXPECTED);
    return;
}

void CVidCapVerifier::SinkSample(IMediaSample* pSample, AM_MEDIA_TYPE* pSinkMediaType, int pin)
{
    HRESULT hr = NOERROR;


    if ((pin < 0) || (pin >= NUM_VCAP_PINS)) {
        SetAccumResult(m_AccumHR, E_UNEXPECTED);
        LOG(TEXT("SinkSample: pin number is less than 0, or greater than %d"), NUM_VCAP_PINS);
        m_nSamplesFailed[pin]++;
        return;
    }

    m_nSamplesSinked[pin]++;

    // 1. Null check
    if (!pSample)
    {
        hr = E_UNEXPECTED;
        LOG(TEXT("SinkSample: no sample pointer passed."));
        m_nSamplesFailed[pin]++;
        return;
    }

    // 2. Get the media type
    AM_MEDIA_TYPE *pMediaType = NULL;

    hr = pSample->GetMediaType(&pMediaType);
    // GetMediaType returns a media type only if it has changed, this is indicated by S_FALSE
    if (FAILED(hr) || ((hr == S_OK) && !pMediaType)) {
        SetAccumResult(m_AccumHR, E_UNEXPECTED);
        LOG(TEXT("SinkSample: unexpected return value from GetMediaType."));
        m_nSamplesFailed[pin]++;
        return;
    }
    // Set hr back to NOERROR in case we got S_FALSE from GetMediaType
    hr = NOERROR;

    // If the sample did not provide one, check if the source gave us one
    if (!pMediaType && pSinkMediaType)
        pMediaType = pSinkMediaType;

    if (!pMediaType && !m_bStoredSinkMediaType[pin]) {
        SetAccumResult(m_AccumHR, E_UNEXPECTED);
        LOG(TEXT("SinkSample: recieved a sample, but no media type known."));
        m_nSamplesFailed[pin]++;
        return;
    }

    // If a new media type was recieved, check it and Verify and store the media type
    if (pMediaType) {
        if (!VerifySinkMediaType(pMediaType, (m_bStoredSinkMediaType[pin] ? &m_SinkMediaType[pin] : 0), pin))
        {
            hr = S_FALSE;
            LOG(TEXT("SinkSample: invalid media type given for the sample."));
            m_nSamplesFailed[pin]++;
        }

        // Free the format block if we had one
        FreeMediaType(m_SinkMediaType[pin]);    
        CopyMediaType(&m_SinkMediaType[pin], pMediaType);

        m_bStoredSinkMediaType[pin] = true;

        if (!pSinkMediaType)
            DeleteMediaType(pMediaType);
    }
    
    // 3. Verify the sample itself, this takes care of setting the failures for the frame number and such.
    VerifySinkMediaSample(pSample, pin);

    SetAccumResult(m_AccumHR, hr);
    return;
}

void CVidCapVerifier::SourceBeginFlush(int pin)
{
    // This should never be called
    SetAccumResult(m_AccumHR, E_UNEXPECTED);
    return;
}

void CVidCapVerifier::SourceEndFlush(int pin)
{
    // This should never be called
    SetAccumResult(m_AccumHR, E_UNEXPECTED);
    return;
}

void CVidCapVerifier::SourceNewSegment(int pin)
{
    // This should never be called
    SetAccumResult(m_AccumHR, E_UNEXPECTED);
    return;
}

void CVidCapVerifier::SourceEndOfStream(int pin)
{
    // This should never be called
    SetAccumResult(m_AccumHR, E_UNEXPECTED);
    return;
}

void CVidCapVerifier::SinkBeginFlush(int pin)
{
    m_bSinkBeginFlush[pin] = true;
}
    
void CVidCapVerifier::SinkEndFlush(int pin)
{
    m_bSinkEndFlush[pin] = true;
}
    
void CVidCapVerifier::SinkNewSegment(int pin)
{
    m_bSinkNewSegment[pin] = true;
}

void CVidCapVerifier::SinkEndOfStream(int pin)
{
    m_bSinkEndOfStream[pin] = true;
}

bool CVidCapVerifier::GotSourceBeginFlush(int pin)
{
    if(pin == -1)
        return (m_bSourceBeginFlush[0] && m_bSourceBeginFlush[1] && m_bSourceBeginFlush[2]);
    return m_bSourceBeginFlush[pin];
}

bool CVidCapVerifier::GotSourceEndFlush(int pin)
{
    if(pin == -1)
        return (m_bSourceEndFlush[0] && m_bSourceEndFlush[1] && m_bSourceEndFlush[2]);
    return m_bSourceEndFlush[pin];
}

bool CVidCapVerifier::GotSourceEndOfStream(int pin)
{
    if(pin == -1)
        return (m_bSourceEndOfStream[0] && m_bSourceEndOfStream[1] && m_bSourceEndOfStream[2]);
    return m_bSourceEndOfStream[pin];
}

bool CVidCapVerifier::GotSourceNewSegment(int pin)
{
    if(pin == -1)
        return (m_bSourceNewSegment[0] && m_bSourceNewSegment[1] && m_bSourceNewSegment[2]);
    return m_bSourceNewSegment[pin];
}

bool CVidCapVerifier::GotSinkBeginFlush(int pin)
{
    if(pin == -1)
        return (m_bSinkBeginFlush[0] && m_bSinkBeginFlush[1] && m_bSinkBeginFlush[2]);
    return m_bSinkBeginFlush[pin];
}

bool CVidCapVerifier::GotSinkEndFlush(int pin)
{
    if(pin == -1)
        return (m_bSinkEndFlush[0] && m_bSinkEndFlush[1] && m_bSinkEndFlush[2]);
    return m_bSinkEndFlush[pin];
}

bool CVidCapVerifier::GotSinkEndOfStream(int pin)
{
    if(pin == -1)
        return (m_bSinkEndOfStream[0] && m_bSinkEndOfStream[1] && m_bSinkEndOfStream[2]);
    return m_bSinkEndOfStream[pin];
}

bool CVidCapVerifier::GotSinkNewSegment(int pin)
{
    if(pin == -1)
        return (m_bSinkNewSegment[0] && m_bSinkNewSegment[1] && m_bSinkNewSegment[2]);
    return m_bSinkNewSegment[pin];
}

bool CVidCapVerifier::VerifySinkMediaType(AM_MEDIA_TYPE* pMediaType, AM_MEDIA_TYPE* pPrevMediaType, int pin)
{
    if (!pMediaType->cbFormat || !pMediaType->pbFormat)
        return false;

    bool isvalid = true;
    
    // The format could be one of the following.
    if ((pMediaType->formattype == FORMAT_VideoInfo2) || 
        (pMediaType->formattype == FORMAT_VideoInfo))
    {
    }
    else if (pMediaType->formattype == FORMAT_MPEGVideo)
    {
        // MPEG1 ?
    }
    return isvalid;
}

bool CVidCapVerifier::VerifySinkMediaSample(IMediaSample* pMediaSample, int pin)
{
    bool bSampleFailed = false;

    // grab the timestamp and store it off if its our first frame
    if(m_nSamplesSinked[pin] == 1)
    {
        LOG(TEXT("    VerifySinkMediaSample::Received the first sample of a new segment."));

        // save off the timestamp of the first frame
        if(pMediaSample->GetTime(&m_rtFirstFrameStartTime[pin], &m_rtFirstFrameStopTime[pin]) != S_OK)
        {
            LOG(TEXT("VerifySinkMediaSample::GetTime returned something other than S_OK."));
            bSampleFailed = true;
        }

        // this is the first frame, so it's also the last frame right now
        if(pMediaSample->GetTime(&m_rtLastFrameStartTime[pin], &m_rtLastFrameStopTime[pin]) != S_OK)
        {
            LOG(TEXT("VerifySinkMediaSample::GetTime returned something other than S_OK."));
            bSampleFailed = true;
        }

        // verify the first frame has a discontinuity flag set
        if(S_FALSE == pMediaSample->IsDiscontinuity())
        {
            LOG(TEXT("VerifySinkMediaSample::First frame is not a Discontinuity frame."));
            bSampleFailed = true;
        }
    }
    else
    {
        LOG(TEXT("    VerifySinkMediaSample::Received an additional sample."));

        REFERENCE_TIME rtAvgTimePerFrame = 0;
        REFERENCE_TIME rtCurrentStartTime = 0, rtCurrentStopTime = 0;

        // verify that the previous timestamp is no more than <n> frames in the future from
        // the current frame
        if(IsEqualGUID(m_SinkMediaType[pin].formattype, FORMAT_VideoInfo))
            rtAvgTimePerFrame = ((VIDEOINFOHEADER *) m_SinkMediaType[pin].pbFormat)->AvgTimePerFrame;
        else if(IsEqualGUID(m_SinkMediaType[pin].formattype,FORMAT_VideoInfo2))
            rtAvgTimePerFrame = ((VIDEOINFOHEADER2 *) m_SinkMediaType[pin].pbFormat)->AvgTimePerFrame;

        // grab the timestamp and store it off as the current frame
        if(pMediaSample->GetTime(&rtCurrentStartTime, &rtCurrentStopTime) != S_OK)
        {
            LOG(TEXT("VerifySinkMediaSample::GetTime returned something other than S_OK."));
            bSampleFailed = true;
        }

        // if the sample is not a discontinuity then we can check the timestamp, if
        // it is a discontinuity we cannot because we have skipped atleast one but
        // potentially more than one samples.
        if(S_FALSE == pMediaSample->IsDiscontinuity())
        {
            if(rtCurrentStartTime >= (m_rtLastFrameStartTime[pin] + (MAXFRAMEDELAY * rtAvgTimePerFrame)))
            {
                LOG(TEXT("VerifySinkMediaSample::The current frame start time is greater than the previous frame plus the time per frame."));
                bSampleFailed = true;
            }

            if(rtCurrentStopTime >= (m_rtLastFrameStopTime[pin] + (MAXFRAMEDELAY * rtAvgTimePerFrame)))
            {
                LOG(TEXT("VerifySinkMediaSample::The current frame stop time is greater than the previous frame plus the time per frame."));
                bSampleFailed = true;
            }
        }

        // track that the timestamps are always increasing
        if(rtCurrentStartTime < m_rtLastFrameStartTime[pin])
        {
            LOG(TEXT("VerifySinkMediaSample::The frame start timestamp went backwards."));
            bSampleFailed = true;
        }

        if(rtCurrentStopTime < m_rtLastFrameStopTime[pin])
        {
            LOG(TEXT("VerifySinkMediaSample::The frame stop timestamp went backwards."));
            bSampleFailed = true;
        }

        // the camera driver will drop samples if the process load becomes too high while testing.
        // samples after the drop will be tagged as a discontinuity. This is OK.
#if 0
        if(S_OK == pMediaSample->IsDiscontinuity())
        {
            LOG(TEXT("VerifySinkMediaSample::Recieved a frame other than the first that's tagged as being a discontinuity."));
            bSampleFailed = true;
        }
#endif

        // now that we're done checking the previous timestamp compared to the current,
        // set the current to the previous for the next frame
        m_rtLastFrameStartTime[pin] = rtCurrentStartTime;
        m_rtLastFrameStopTime[pin] = rtCurrentStopTime;
    }

    if(bSampleFailed)
        m_nSamplesFailed[pin]++;

    return true;
}

int CVidCapVerifier::GetSamplesSourced(int pin)
{
    // should never be called as the video capture filter is the source.
    return -1;
}

int CVidCapVerifier::GetSamplesSinked(int pin)
{
    if(pin == -1)
        return (m_nSamplesSinked[0] + m_nSamplesSinked[1] + m_nSamplesSinked[2]);
    return m_nSamplesSinked[pin];
}

int CVidCapVerifier::GetSamplesFailed(int pin)
{
    if(pin == -1)
        return (m_nSamplesFailed[0] + m_nSamplesFailed[1] + m_nSamplesFailed[2]);
    return m_nSamplesFailed[pin];
}

HRESULT
CVidCapVerifier::GetTimestampOfFirstFrame(REFERENCE_TIME *rtStart, REFERENCE_TIME *rtStop, int pin)
{
    if(!rtStart || !rtStop)
        return E_POINTER;

    if(pin < 0 || pin > NUM_VCAP_PINS)
        return E_INVALIDARG;

    if(m_nSamplesSinked[pin] == 0)
        return VFW_E_SAMPLE_TIME_NOT_SET;

    *rtStart = m_rtFirstFrameStartTime[pin];
    *rtStop = m_rtFirstFrameStopTime[pin];

    if(m_rtFirstFrameStopTime[pin] == 0)
        return VFW_S_NO_STOP_TIME;

    return S_OK;
}

HRESULT CVidCapVerifier::GetTimestampOfLastFrame(REFERENCE_TIME *rtStart, REFERENCE_TIME *rtStop, int pin)
{
    if(!rtStart || !rtStop)
        return E_POINTER;

    if(pin < 0 || pin > NUM_VCAP_PINS)
        return E_INVALIDARG;

    if(m_nSamplesSinked[pin] == 0)
        return VFW_E_SAMPLE_TIME_NOT_SET;

    *rtStart = m_rtLastFrameStartTime[pin];
    *rtStop = m_rtLastFrameStopTime[pin];

    if(m_rtLastFrameStopTime[pin] == 0)
        return VFW_S_NO_STOP_TIME;

    return S_OK;
}


