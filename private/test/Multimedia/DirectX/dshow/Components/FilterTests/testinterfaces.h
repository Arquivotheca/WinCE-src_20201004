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
#ifndef _TEST_INTERFACES_H
#define _TEST_INTERFACES_H

#include "TestUids.h"

typedef AM_MEDIA_TYPE* PAM_MEDIA_TYPE;

struct MediaTypeCombination
{
    int nInputs;
    int nOutputs;
    AM_MEDIA_TYPE *mtInput;
    AM_MEDIA_TYPE *mtOutput;
};

struct AudioTypeSpec
{
    GUID majortype;
    GUID subtype;
    GUID formattype;
    WORD wFormatTag;
};

struct VideoTypeSpec
{
    GUID majortype;
    GUID subtype;
    WORD bitcount;
    DWORD compression;
    REFERENCE_TIME rtAvgTimePerFrame;
    int colorTableSize;
    BYTE* pColorTable;
};

enum SampleLatenessProperties {
    RandomlyLate,
    MostlyLate,
    MostlyOnTime,
    AllLate,
    AllOnTime
};

enum StreamingFlags
{
    UntilEndOfStream = 1,
    Loop = 2,
    ForSpecifiedTime = 3
};

#define StreamingFlagName(flag) ((flag == UntilEndOfStream) ? L"UntilEndOfStream" : ((flag == Loop) ? L"Loop" : L"Invalid Flags"))

enum StreamingMode
{
    // Streaming is specified in the length of time
    Continuous,
    // Streaming is specified in the number of samples
    Discrete,
    // Discrete streaming with specified gaps of time
    Delayed
};

#define StreamingModeName(mode) ((mode == Continuous) ? L"Continuous" : ((mode == Discrete) ? L"Discrete" : ((mode == Delayed) ? L"Delayed" : L"Invalid Mode")))


#define SetAccumResult(hrAccum, hr) if (SUCCEEDED(hrAccum)) hrAccum = hr

// This module defines interfaces specifically supported by filters that form part of the filter testing framework.
class IVerifier
{
public:
    virtual void SourceSample(IMediaSample* pSample, AM_MEDIA_TYPE* pMediaType, int pin = 0) = 0;
    virtual void SinkSample(IMediaSample* pSample, AM_MEDIA_TYPE* pMediaType, int pin = 0) = 0;
    
    virtual void SourceBeginFlush(int pin = 0) = 0;
    virtual void SinkBeginFlush(int pin = 0) = 0;
    
    virtual void SourceEndFlush(int pin = 0) = 0;
    virtual void SinkEndFlush(int pin = 0) = 0;
    
    virtual void SourceNewSegment(int pin = 0) = 0;
    virtual void SinkNewSegment(int pin = 0) = 0;
    
    virtual void SourceEndOfStream(int pin = 0) = 0;
    virtual void SinkEndOfStream(int pin = 0) = 0;
    
    virtual HRESULT StreamingBegin() = 0;
    virtual HRESULT StreamingEnd() = 0;
    virtual void Reset() = 0;

    // Pin = -1 indicates "any" pin
    virtual bool GotSourceBeginFlush(int pin = -1) = 0;
    virtual bool GotSourceEndFlush(int pin = -1) = 0;
    virtual bool GotSourceNewSegment(int pin = -1) = 0;
    virtual bool GotSourceEndOfStream(int pin = -1) = 0;
    virtual bool GotSinkBeginFlush(int pin = -1) = 0;
    virtual bool GotSinkEndFlush(int pin = -1) = 0;
    virtual bool GotSinkNewSegment(int pin = -1) = 0;
    virtual bool GotSinkEndOfStream(int pin = -1) = 0;
    virtual int GetSamplesSourced(int pin = -1) = 0;
    virtual int GetSamplesSinked(int pin = -1) = 0;
    virtual int GetSamplesFailed(int pin = -1) = 0;
};

class ITestSource : public IUnknown
{
public:
    // To iterate over media type combinations
    virtual int GetNumMediaTypeCombinations() = 0;
    virtual HRESULT SelectMediaTypeCombination(int iPosition) = 0;
    virtual HRESULT SelectMediaTypeCombination(int iPosition, BOOL bRotation) = 0;
    virtual HRESULT GetMediaTypeCombination(int iPosition, AM_MEDIA_TYPE** ppMediaType) = 0;
    virtual int GetSelectedMediaTypeCombination() = 0;


    // Initiating the control signaling tests
    virtual HRESULT BeginFlushTest() = 0;
    virtual HRESULT EndFlushTest() = 0;
    virtual HRESULT NewSegmentTest() = 0;
    virtual HRESULT EndOfStreamTest() = 0;

    // Initiating streaming
    // Setting streaming modes - if continuous nUnits is time in msec, else it is the number of samples.
    // The mode flags indicate if the streaming should stop when the source stream ends before the requested number of units or if the source should loop around
    // Default is for the stream to run through the source until the end and stop. In which case the units dont matter. The filter should set to the most natural unit in this case.
    virtual HRESULT SetStreamingMode(StreamingMode mode, int nUnits, StreamingFlags flags) = 0;
    virtual HRESULT BeginStreaming() = 0;
    virtual HRESULT EndStreaming() = 0;
    virtual bool IsStreaming() = 0;
    virtual HRESULT GenerateMediaSample(IMediaSample* pSample, IPin*) = 0;
    virtual HRESULT SetSampleLatenessProbability(float p) = 0;
    virtual int GetSamplesSourced(int pin = 0) = 0;

    // Setting the verifier
    virtual HRESULT SetVerifier(IVerifier* pVerifier) = 0;
    virtual IVerifier *GetVerifier() = 0;

    // This method sets the source of data. The actual type of data depends on the filter. 
    // Test filters dont always implement this method.
    virtual HRESULT SetDataSource(const TCHAR* file) = 0;

    // This method sets the source of resolution.
    // Test filters dont always implement this method.
    virtual HRESULT SetResolution(int width, int height){return E_NOTIMPL;}

    // These methods set parameters to be invalid during the respective operations. Filters may not always implement this method.
    // If not supported, they return false.
    virtual bool SetInvalidConnectionParameters() = 0;
    virtual bool SetInvalidAllocatorParameters() = 0;
    virtual HRESULT UseOwnAllocator() = 0;
};

class ITestSink : public IUnknown
{
public:
    // To iterate over media types
    virtual int GetPinCount() = 0;
    virtual int GetNumMediaTypeCombinations() = 0;
    virtual HRESULT SelectMediaTypeCombination(int iPosition) = 0;
    virtual HRESULT GetMediaTypeCombination(int iPosition, AM_MEDIA_TYPE** ppMediaType) = 0;
    virtual int GetSelectedMediaTypeCombination() = 0;

    // Setting the verifier
    virtual HRESULT SetVerifier(IVerifier* pVerifier) = 0;
    virtual IVerifier *GetVerifier() = 0;
    
    // These methods set parameters to be invalid during the respective operations. Filters may not always implement this method.
    // If not supported, they return false.
    virtual bool SetInvalidConnectionParameters() = 0;
    virtual bool SetInvalidAllocatorParameters() = 0;
};

#ifndef UNDER_CE
typedef DWORD TESTPROCAPI;
enum TPR
{
    TPR_FAIL,
    TPR_PASS,
    TPR_SKIP
};
#endif

#endif // _TEST_INTERFACES_H
