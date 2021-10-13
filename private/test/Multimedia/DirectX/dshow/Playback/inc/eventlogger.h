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
////////////////////////////////////////////////////////////////////////////////

#ifndef EVENTLOGGER_H
#define EVENTLOGGER_H

#include <tchar.h>
#include "globals.h"
#include "Position.h"
#include "GraphEvent.h"
#include "TapFilter.h"
#include "RemoteEvent.h"
#include "videostamp.h"
#include "FrameDiff.h"

class ILogger;
class TestGraph;

#include <map>

using namespace std;

enum EventLoggerType {
    // Generic sample logger
    SampleProperties,

    // Audio/Video specific loggers
    VideoFrameData,
    VideoTimeStamps,    
    AudioFrameData,
    AudioTimeStamps,

    LogFrameNumbers,
    StreamData,
    GraphEvents,
    RemoteEvents,
    DiffFrames,

    // Instrumeters
    InsertFrameNumbers,


    // End marker
    EventLoggerEndMarker
};

// The event logger interface
class IEventLogger
{
public:
    virtual ~IEventLogger() {};
    virtual HRESULT Init(TestGraph* pTestGraph, EventLoggerType type, void *pEventLoggerData) = 0;
    virtual HRESULT ProcessEvent(GraphEvent event, void* pEventData, void* pTapCallbackData) = 0;
    virtual HRESULT Start() = 0;
    virtual void Stop() = 0;
    virtual void Reset() = 0;
};

// EventLogger list is a list of <EventLoggerType, > pairs
typedef map<int, IEventLogger*> EventLoggerList;
typedef pair <int, IEventLogger*> EventLoggerPair;

// Factory method to create verifiers given a pointer to verification data
typedef HRESULT (*EventLoggerFactory)(EventLoggerType type, void* pEventLoggerData, TestGraph* pTestGraph, IEventLogger** ppEventLogger);
HRESULT GenericLoggerCallback(GraphEvent event, void* pGraphEventData, void* pTapCallbackData, void *pObject);

struct EventLoggerFactoryTableEntry
{
    EventLoggerType type;
    EventLoggerFactory fn;
};

// EventLogger data strcutures
// This contains the actual sample that was tapped
struct SampleEventData
{
    LONGLONG currpos;
    LONGLONG pts;
    LONGLONG dts;
    LONGLONG ats;
    DWORD buflen;
    BYTE* pBuffer;
};

// This contains the sample time-stamps
struct TimeStampEventData
{
    LONGLONG currpos;
    LONGLONG pts;
    LONGLONG dts;
    LONGLONG ats;
};

const TCHAR* GetEventLoggerString(EventLoggerType type);

// Is this verification possible
bool IsLoggable(EventLoggerType type);

// Factory method for verifiers
HRESULT CreateEventLogger(EventLoggerType type, void* pVerificationData, TestGraph* pTestGraph, IEventLogger** ppGraphVerifier);
HRESULT CreateVideoDecoderOutputLogger(EventLoggerType type, void* pEventLoggerData, TestGraph* pTestGraph, IEventLogger** ppEventLogger);
HRESULT CreateSampleLogger(EventLoggerType type, void* pEventLoggerData, TestGraph* pTestGraph, IEventLogger** ppEventLogger);
HRESULT CreateGraphEventLogger(EventLoggerType type, void* pEventLoggerData, TestGraph* pTestGraph, IEventLogger** ppEventLogger);
HRESULT CreateRemoteEventLogger(EventLoggerType type, void* pEventLoggerData, TestGraph* pTestGraph, IEventLogger** ppEventLogger);
HRESULT CreateFrameNumberInstrumenter(EventLoggerType type, void* pEventLoggerData, TestGraph* pTestGraph, IEventLogger** ppEventLogger);
HRESULT CreateFrameNumberLogger(EventLoggerType type, void* pEventLoggerData, TestGraph* pTestGraph, IEventLogger** ppEventLogger);
HRESULT CreateStreamLogger(EventLoggerType type, void* pEventLoggerData, TestGraph* pTestGraph, IEventLogger** ppEventLogger);
HRESULT CreateFrameDiffLogger(EventLoggerType type, void* pEventLoggerData, TestGraph* pTestGraph, IEventLogger** ppEventLogger);

// Loggers
class VideoDecoderOutputLogger : public IEventLogger
{
public:
    VideoDecoderOutputLogger();
    virtual ~VideoDecoderOutputLogger();
    virtual HRESULT Init(TestGraph* pTestGraph, EventLoggerType type, void *pEventLoggerData);
    virtual void ProcessEvent(GraphEvent event, void* pGraphEventData);
    virtual HRESULT Start();
    virtual void Stop();
    virtual void Reset();

private:
    EventLoggerType m_EventLoggerType;
    TCHAR m_szLogFile[MAX_PATH];
    HANDLE m_hFile;
};

struct SampleLoggerData
{
    DWORD filterType;
    GUID mediaType;
    TCHAR szLogFile[MAX_PATH];
    PIN_DIRECTION pindir;
    errno_t  Error;
    SampleLoggerData(DWORD filterType, PIN_DIRECTION pindir, GUID mediaType, TCHAR* szLogFile)
    {
        this->filterType = filterType;
        this->mediaType = mediaType;
        this->pindir = pindir;
        Error = _tcsncpy_s (this->szLogFile,countof(this->szLogFile),szLogFile, _TRUNCATE);
    }
};


class SampleLogger : public IEventLogger
{
public:
    SampleLogger();
    virtual ~SampleLogger();
    virtual HRESULT Init(TestGraph* pTestGraph, EventLoggerType type, void *pEventLoggerData);
    virtual HRESULT ProcessEvent(GraphEvent event, void* pEventData, void* pTapCallbackData);
    virtual HRESULT Start();
    virtual void Stop();
    virtual void Reset();

private:
    int m_nSamples;
    AM_MEDIA_TYPE m_mt;
    EventLoggerType m_EventLoggerType;
    TCHAR m_szLogFile[MAX_PATH];
    HANDLE m_hFile;
    HRESULT m_hr;
};


struct GraphEventLoggerData
{
    DWORD filterType;
    GUID mediaType;
    TCHAR szLogFile[MAX_PATH];
    PIN_DIRECTION pindir;
    errno_t  Error;
    GraphEventLoggerData(DWORD filterType, PIN_DIRECTION pindir, GUID mediaType, TCHAR* szLogFile)
    {
        this->filterType = filterType;
        this->mediaType = mediaType;
        this->pindir = pindir;
        Error = _tcsncpy_s (this->szLogFile,countof(this->szLogFile), szLogFile, _TRUNCATE);
    }
};

class GraphEventLogger : public IEventLogger
{
public:
    GraphEventLogger();
    virtual ~GraphEventLogger();
    virtual HRESULT Init(TestGraph* pTestGraph, EventLoggerType type, void *pEventLoggerData);
    virtual HRESULT ProcessEvent(GraphEvent event, void* pEventData, void* pTapCallbackData);
    virtual HRESULT Start();
    virtual void Stop();
    virtual void Reset();

private:
    int m_nSamples;
    AM_MEDIA_TYPE m_mt;
    EventLoggerType m_EventLoggerType;
    TCHAR m_szLogFile[MAX_PATH];
    HANDLE m_hFile;
    HRESULT m_hr;
};

struct RemoteEventLoggerData
{
    TCHAR szLoggerName[FILTER_NAME_LEN];
    DWORD filterType;
    GUID mediaType;
    PIN_DIRECTION pindir;
    IPin* pTapPin;
    ILogger* pLogger;
    errno_t  Error;
    RemoteEventLoggerData(TCHAR* szLoggerName, DWORD filterType, PIN_DIRECTION pindir, GUID mediaType, ILogger* pLogger)
    {
        this->filterType = filterType;
        this->mediaType = mediaType;
        this->pindir = pindir;
        this->pTapPin = NULL;
        this->pLogger = pLogger;
        Error = _tcsncpy_s (this->szLoggerName,countof(this->szLoggerName),szLoggerName,_TRUNCATE );
    }

    RemoteEventLoggerData(TCHAR* szLoggerName, IPin* pTapPin, ILogger* pLogger)
    {
        this->pTapPin = pTapPin;
        this->pLogger = pLogger;
        Error = _tcsncpy_s (this->szLoggerName,countof(this->szLoggerName),szLoggerName,_TRUNCATE);
    }
};

class RemoteEventLogger : public IEventLogger
{
public:
    RemoteEventLogger();
    virtual ~RemoteEventLogger();
    virtual HRESULT Init(TestGraph* pTestGraph, EventLoggerType type, void *pEventLoggerData);
    virtual HRESULT ProcessEvent(GraphEvent event, void* pEventData, void* pTapCallbackData);
    virtual HRESULT Start();
    virtual void Stop();
    virtual void Reset();

private:
    enum {
        GRAPH_EVENT_BUFFER_SIZE = 512,
        REMOTE_EVENT_BUFFER_SIZE = 512
    };

    TCHAR szLoggerName[FILTER_NAME_LEN];
    int m_nSamples;
    AM_MEDIA_TYPE m_mt;
    EventLoggerType m_EventLoggerType;
    ILogger* m_pLogger;
    HRESULT m_hr;
    BYTE m_GraphEventBuffer[GRAPH_EVENT_BUFFER_SIZE];
    BYTE m_RemoteEventBuffer[REMOTE_EVENT_BUFFER_SIZE];
};


struct FrameNumberInstrumenterData
{
    DWORD filterType;
    GUID mediaType;
    PIN_DIRECTION pindir;

    FrameNumberInstrumenterData(DWORD filterType, PIN_DIRECTION pindir, GUID mediaType)
    {
        this->filterType = filterType;
        this->mediaType = mediaType;
        this->pindir = pindir;
    }
};

class FrameNumberInstrumenter : public IEventLogger
{
public:
    FrameNumberInstrumenter();
    virtual ~FrameNumberInstrumenter();
    virtual HRESULT Init(TestGraph* pTestGraph, EventLoggerType type, void *pEventLoggerData);
    virtual HRESULT ProcessEvent(GraphEvent event, void* pEventData, void* pTapCallbackData);
    virtual HRESULT Start();
    virtual void Stop();
    virtual void Reset();

private:
    int m_nSamples;
    AM_MEDIA_TYPE m_mt;
    EventLoggerType m_EventLoggerType;
    TCHAR m_szLogFile[MAX_PATH];
    HRESULT m_hr;
    VideoStampWriter m_Instrumenter;
};

struct FrameNumberLoggerData
{
    DWORD filterType;
    GUID mediaType;
    PIN_DIRECTION pindir;

    FrameNumberLoggerData(DWORD filterType, PIN_DIRECTION pindir, GUID mediaType)
    {
        this->filterType = filterType;
        this->mediaType = mediaType;
        this->pindir = pindir;
    }
};

class FrameNumberLogger : public IEventLogger
{
public:
    FrameNumberLogger();
    virtual ~FrameNumberLogger();
    virtual HRESULT Init(TestGraph* pTestGraph, EventLoggerType type, void *pEventLoggerData);
    virtual HRESULT ProcessEvent(GraphEvent event, void* pEventData, void* pTapCallbackData);
    virtual HRESULT Start();
    virtual void Stop();
    virtual void Reset();
    

private:
    int m_nSamples;
    AM_MEDIA_TYPE m_mt;
    EventLoggerType m_EventLoggerType;
    TCHAR m_szLogFile[MAX_PATH];
    HRESULT m_hr;
    VideoStampReader m_Recognizer;
};

class StreamLogger : public IEventLogger
    {
    public:
    StreamLogger();
    virtual ~StreamLogger();
    virtual HRESULT Init(TestGraph* pTestGraph, EventLoggerType type, void *pEventLoggerData);
    virtual HRESULT ProcessEvent(GraphEvent event, void* pEventData, void* pTapCallbackData);
    virtual HRESULT Start();
    virtual void Stop();
    virtual void Reset();
  
    private:
    TCHAR m_szCheckSumFile[MAX_PATH];
    TCHAR m_szLogFile[MAX_PATH];
    HANDLE m_hLogFile;
    HANDLE m_hCheckSumFile;
    };
  
    // Since there will be a tap filter for each output pin, there will also be a corresponding StreamLoggerData object for each as well.
    class StreamLoggerData
    {
    public:
        INT primaryType;            // Attempt to connect to log output of filter of this type first
        INT secondaryType;      // Fall back to logging this filter's output if primaryType is not found.
        TCHAR szOutFile[MAX_PATH];  // File name to use as a basis for output file names
        HANDLE hStreamFile;         // handle to the file to write stream data to
        HANDLE hChecksumFile;       // handle to the file to write checksum data to
        BOOL    bRenderStream;      // Drop the samples after logging the data or keep and render them?
        BOOL    bLogStream;         // Log the stream data to a file?
        BOOL    bLogChecksum;       // Log the checksum data to a file?
  
    StreamLoggerData()
    {
        this->primaryType = -1;
        this->secondaryType = -1;
        memset(szOutFile, 0, sizeof(szOutFile));
        hStreamFile = INVALID_HANDLE_VALUE;
        hChecksumFile = INVALID_HANDLE_VALUE;
        bRenderStream = true;
        bLogStream = false;
        bLogChecksum = false;
    }

    StreamLoggerData(StreamLoggerData * pData)
    {
        this->primaryType = pData->primaryType;
        this->secondaryType = pData->secondaryType;
        memcpy(szOutFile,pData->szOutFile, sizeof(szOutFile));
        hStreamFile = INVALID_HANDLE_VALUE;
        hChecksumFile = INVALID_HANDLE_VALUE;
        bRenderStream = pData->bRenderStream;
        bLogStream = pData->bLogStream;
        bLogChecksum = pData->bLogChecksum;
    }

    ~StreamLoggerData()
    {
        if(INVALID_HANDLE_VALUE != hStreamFile && NULL != hStreamFile)
            CloseHandle(hStreamFile);
        if(INVALID_HANDLE_VALUE != hChecksumFile && NULL != hChecksumFile)
            CloseHandle(hChecksumFile);
    }
        
    };

class FrameDiffLogger : public IEventLogger
{
    public:
        FrameDiffLogger();
        virtual ~FrameDiffLogger();
        virtual HRESULT Init(TestGraph* pTestGraph, EventLoggerType type, void *pEventLoggerData);
        virtual HRESULT ProcessEvent(GraphEvent event, void* pEventData, void* pTapCallbackData);
        virtual HRESULT Start();
        virtual void Stop();
        virtual void Reset();
    

    private:
        AM_MEDIA_TYPE m_mt;
        EventLoggerType m_EventLoggerType;
        TCHAR m_szLogFile[MAX_PATH];
        HRESULT m_hr;
        FrameDiff m_Differ;
        CVideoStampReader m_VideoStampReader;
};
#endif