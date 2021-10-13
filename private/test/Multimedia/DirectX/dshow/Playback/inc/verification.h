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

#pragma once

#ifndef VERIFICATION_H
#define VERIFICATION_H

#include <map>
#include "DxXML.h"
#include "Position.h"
#include "GraphEvent.h"
#include "Index.h"
#include "Timer.h"
#include "globals.h"
#include "Streams.h"

const DWORD DLLNAME_LEN = 256;
const DWORD VERIFICATION_NAME_LEN = 128;
const DWORD    MAX_VERIFICATIONS = 128;

#define MAX_VERIFICATION_MESSAGE_SIZE         8192
#define VERIFICATION_START_TIMEOUT_MS        5000
#define VERIFICATION_EXIT_TIMEOUT_MS         5000
#define VERIFICATION_OPERATION_TIMEOUT_MS 60000            

class TestGraph;
class CTapFilter;
class CGraphVerifier;

#define VLOG

#define CHECKHR( hr,msg )    \
    if ( FAILED(hr) )    \
    {                    \
        LOG( msg );        \
        LOG( TEXT("%S: ERROR %d@%S - error code: 0x%08x "),    \
                    __FUNCTION__, __LINE__, __FILE__, hr);    \
        goto cleanup;    \
    }                    \
    else                \
    {                    \
        LOG( msg );        \
        LOG( TEXT("Succeeded.") );        \
    }    \

// The specific quantities or behavior to be verified
enum VerificationType 
{
        // Graph Building
    CorrectGraph,
    BuildGraphLatency,
    ChannelChangeLatency,

    // State change
    VerifyStateChangeLatency,

    // Playback, Seeking and Trick-modes
    VerifySampleDelivered,
    VerifyPlaybackDuration,
    StartupLatency,
    VerifyRate,
    VerifyCurrentPosition,
    VerifyStoppedPosition,
    VerifySetPositionLatency,
    VerifyPositionAndRate,

    // Scaling, video window changes
    VerifyScaleChange,
    VerifyRepaintEvent,
    VerifyRepaintedImage,

    // Video
    DecodedVideoFrameData,
    DecodedVideoFirstFrameData,
    DecodedVideoEndFrameData,
    DecodedVideoFrameDataAfterFlush,
    
    DecodedVideoPlaybackTimeStamps,
    DecodedVideoPlaybackFirstFrameTimeStamp,
    DecodedVideoPlaybackEndFrameTimeStamp,
    DecodedVideoSeekAccuracy,
    DecodedVideoTrickModeFirstFrameTimeStamp,
    DecodedVideoTrickModeEndFrameTimeStamp,

    RenderedVideoData,
    RenderedVideoFirstFrameData,
    RenderedVideoEndFrameData,
    RenderedVideoFrameDataAfterFlush,
    
    RenderedVideoTimeStamps,
    RenderedVideoFirstFrameTimeStamp,
    RenderedVideoEndFrameTimeStamp,
    RenderedVideoSeekAccuracy,

    NumDecodedVideoFrames,
    NumRenderedVideoFrames,
    NumDecoderDroppedVideoFrames,
    NumRendererDroppedVideoFrames,
    NumDroppedVideoFrames,
    NumDecodedTrickModeVideoFrames,
    NumRenderedTrickModeVideoFrames,

    DecodedVideoFirstFrameLatency,
    DecodedVideoLatencyRunToFirstSample,
    DecodedVideoLatencyPauseToFirstSample,
    DecodedVideoLatencyStopToPause,
    DecodedVideoLatencyStopToRun,
    DecodedVideoLatencyAfterFlush,
    DecodedVideoLatencyAfterSeek,
    
    RenderedVideoLatencyStopToPause,
    RenderedVideoLatencyStopToRun,
    RenderedVideoLatencyAfterFlush,
    RenderedVideoLatencyAfterSeek,

    VerifyVideoRect,
    VerifyVideoSourceRect,
    VerifyVideoDestRect,
    VerifyBackBuffers,
    VerifyDDrawSamples,
    VerifyGDISamples,
    VerifyCodecScale,
    VerifyCodecNotScale,
    VerifyCodecRotation,

    // Audio
    DecodedAudioFrameData,
    DecodedAudioFirstFrameData,
    DecodedAudioEndFrameData,
    DecodedAudioFrameDataAfterFlush,
    DecodedAudiooTimeStampsAfterSeek,

    
    DecodedAudioTimeStamps,
    DecodedAudioFirstFrameTimeStamp,
    DecodedAudioEndFrameTimeStamp,
    DecodedAudioSeekAccuracy,

    RenderedAudioData,
    RenderedAudioFirstFrameData,
    RenderedAudioEndFrameData,
    RenderedAudioFrameDataAfterFlush,
    
    RenderedAudioTimeStamps,
    RenderedAudioFirstFrameTimeStamp,
    RenderedAudioEndFrameTimeStamp,
    RenderedAudioSeekAccuracy,

    NumRenderedAudioGlitches,
    NumRenderedAudioDiscontinuities,
    DecodedAudioDataTime,
    NumDecodedAudioFrames,
    NumRenderedAudioFrames,
    NumDecoderDroppedAudioFrames,
    NumRendererDroppedAudioFrames,
    NumDroppedAudioFrames,

    DecodedAudioFirstFrameLatency,
    RenderedAudioFirstFrameLatency,
    DecodedAudioLatencyAfterFlush,
    DecodedAudioLatencyAfterSeek,
    RenderedAudioLatencyAfterFlush,

    // IReferenceClock latency verificaiton
    VerifyAdvisePastTimeLatency,
    VerifyAdviseTimeLatency,

    // Renderer verifications
    VerifyVideoRendererMode,
    VerifyVideoRendererSurfaceType,

    // End marker
    VerificationEndMarker

};

// Verification Location
// VLOC_LOCAL     -    The verifier and the filter graph it is monitoring are in the same process as the test application.
//
// VLOC_CLIENT    -    This verifier object is local to the test application, but its sole purpose is to marshal data to
//                    and from a remote verifier that is attached to a filter graph  running in a separate process.
//
// VLOC_REMOTE    -    This verifier object is directly attached to a filter graph that is running in a separate process from the test application.
//                    This verifier object receives commands from a separate verifier object running in the same process as the test application.
//                    These commands are sent via a message queue and monitored via a message pump thread.

enum VerificationLocation
{
    VLOC_LOCAL,                        
    VLOC_CLIENT,                    
    VLOC_SERVER
};

enum FrameAccuracy 
{
    FrameAccurate,
    KeyFrameAccurate
};

enum VerificationCommand
{
    VCMD_GETRESULT,
    VCMD_START,
    VCMD_STOP,
    VCMD_RESET,
    VCMD_UPDATE
};

struct VerificationRequest
{
    VerificationCommand command;    // Command to send to the verifier
    DWORD cbDataSize;                // Size in bytes of the data accompanying the command
    union
    {
        ULONGLONG upadding;          // guarantee that pointer is 8-byte-aligned
        BYTE* pData;                 // Data accompanying the request
    };
};

struct VerificationResponse
{
    HRESULT hr;                        // HRESULT return value of the remote command
    DWORD cbDataSize;                // Size in bytes of the data accompanying the result
    union
    {
        ULONGLONG upadding;          // guarantee that pointer is 8-byte-aligned
        BYTE* pData;                 // Data accompanying the result
    };
};

struct VerificationInitInfo
{
    VerificationType type;                    // Type of the verifier to initialize
    VerificationLocation location;            // Where to initialize the verifier.
    CGraphVerifier** ppVerifier;            // Where to return a verifier pointer to (only used in client process space)    
    TCHAR szName[VERIFICATION_NAME_LEN];    // Name given to this verifier
    DWORD cbDataSize;                        // Size of the data used to initialize the verifier in bytes                       
    union
    {
        ULONGLONG upadding;          // guarantee that pointer is 8-byte-aligned
        BYTE* pData;                 // Data used to initialize the verifier
    };
};

struct ProcessEventData
{
    GraphEvent event;
    void* pGraphEventData;
};

typedef std::vector<VerificationInitInfo*>        InitInfoList;
typedef InitInfoList::iterator                itInitInfo;

// The verifier interface
class CGraphVerifier
{
public:
    CGraphVerifier();
    virtual ~CGraphVerifier();
    virtual HRESULT Init(void* pOwner, VerificationType type, VerificationLocation location, TCHAR* szName, void* pVerificationData);
    virtual HRESULT GetResult(VerificationType type, void* pVerificationData = NULL, DWORD* pdwSize = NULL);
    virtual HRESULT ProcessEvent(GraphEvent event, void* pGraphEventData);
    virtual HRESULT Start();
    virtual HRESULT Stop();
    virtual HRESULT Reset();
    virtual HRESULT SignalExit();
    virtual HRESULT SignalStarted();
    virtual HRESULT Update(void* pData, DWORD* pdwSize);
    

    TCHAR* GetName();
    HANDLE GetExitEvent();                // Fetch a handle to the existing event object used to signal the message pump to exit.
    VerificationType GetType();

protected:
    bool m_bEOS;                        // Have we encountered an End Of Stream event?
    bool m_bFirstSample;                // Have we received a sample?
    LONGLONG m_llFirstSampleTime;        // Time stamp recorded within the event data of the first sample received.
    LONGLONG m_llEOSTime;                // Time stamp recorded within the event data of the first End of Stream Event.
    LONGLONG m_llSetStart;                // Start time stamp value from last New Segment event
    LONGLONG m_llSetStop;                // Stop time stamp value from last New Segment event
    double m_dSetRate;                    // Rate value from last New Segment event
    HRESULT m_hr;                        // Overall HRESULT indicating verification passed or failed. 

    TCHAR* m_szName;                    // Name given to this verifier
    VerificationType m_Type;            // Type of verification that this verifier will provide
    VerificationLocation m_Location;    // Is this verifier in local to the test process, acting as a client, or acting as a server?
    CCritSec m_CritSec;                    // Sychronization object between the message pump thread and the tap filter
    
private:
    HANDLE m_hThread;                    // Handle to the message pump thread
#ifdef UNDER_CE
    HANDLE m_hMQIN;                        // Handle to the incoming message queue if this verifier is acting as a client
    HANDLE m_hMQOUT;                    // Handle to the outgoing message queue if this verifier is acting as a client
#endif
    HANDLE m_hExit;                        // Handle to the event signalling that the message pump thread should exit
    HANDLE m_hStarted;                    // Handle to the event signalling that the message pump thread has started
};

DWORD WINAPI VerifierThreadProc( __in LPVOID lpParameter);

// Verification list is a list of <VerificationType, verification data> pairs
typedef std::map<DWORD, void*> VerificationList;
typedef std::pair<DWORD, void*> VerificationPair;
typedef std::vector<CGraphVerifier*> VerifierList;

// Factory method to create verifiers given a pointer to verification data
typedef HRESULT (*VerifierFactoryFunc)( VerificationType type,
                                        VerificationLocation location,
                                        TCHAR* pszName,
                                        void* pVerificationData, 
                                        void* pOwner, 
                                        CGraphVerifier** ppGraphVerifier );

// Prototype of a verification parser function
typedef HRESULT (*VerificationParserFunc)( CXMLElement *pVerification, 
                                               VerificationType type, 
                                               VerificationList* pVerificationList );


HRESULT GenericTapFilterCallback( GraphEvent event, void* pGraphEventData, void* pTapCallbackData, void* pObject );


//
// This defines everything we need to know about verification. For each verification type, we need the 
// equivalent string name (used for xml parsing too), the parser we need to use to parse the verification
// information from the xml file and the Verifier factory function that can create a verifier for the 
// verificaiton and pass back the verifier pointer.
//
struct VerificationInfo
{
    VerificationType    enVerificationType;
    TCHAR    szVerificationName[VERIFICATION_NAME_LEN];
    void    *pVerificationData;
    CGraphVerifier            *pVerifier;
    VerifierFactoryFunc        fpVerifierFactory;
    VerificationParserFunc    fpVerificationParser;

    // ctor
    VerificationInfo( VerificationType type, 
                      const TCHAR *pName, 
                      void    *pData, 
                      CGraphVerifier *pGraphVerifier, 
                      VerifierFactoryFunc pFactory, 
                      VerificationParserFunc pParser)
    {
        enVerificationType  = type;
        errno_t  Error;
        ZeroMemory( szVerificationName, VERIFICATION_NAME_LEN );
        Error = _tcsncpy_s( szVerificationName,countof(szVerificationName),pName, _TRUNCATE ); 
        pVerificationData = pData;
        pVerifier = pGraphVerifier;
        fpVerifierFactory = pFactory;
        fpVerificationParser = pParser;
    }

    // dtor
    // assignment operator

};

typedef std::map<DWORD, VerificationInfo *>        VerificationMap;
typedef VerificationMap::iterator                itVerification;

typedef std::map<DWORD, CGraphVerifier*>    VerifierMap;
typedef VerifierMap::iterator                itVerifier;


class MutexWait
{
private:
    HANDLE m_hM;
    
public:
    MutexWait(){};
    MutexWait(HANDLE hMutex)
    {
        if(WAIT_OBJECT_0 == WaitForSingleObject(hMutex, INFINITE))
            m_hM = hMutex;
        else
            m_hM = NULL;    
    }

    ~MutexWait()
    {
        if(m_hM)
            ReleaseMutex(m_hM);
    }
};

//This is the verifier manager class. It has a list of all verifiers available to this test module
//there is only one singleton instance of this class, defined in Verification.cpp. Everyone can
//use that to query
class CVerificationMgr
{
public:
    static CVerificationMgr *Instance();
    static HRESULT Release();
    virtual ~CVerificationMgr();
    static void GlobalInit();
    static void GlobalRelease();

public:
    BOOL    IsInitialized() { return m_bInitialized; }
    BOOL    InitializeVerifications();
    VerificationInfo *GetVerificationInfo( VerificationType type );
    VerificationInfo *GetVerificationInfo( PCSTR verificationName );
    DWORD   GetNumberOfVerifications();

    // Add a custom verifier
    HRESULT AddVerifier( VerificationType type, CGraphVerifier* pVerifier );
    // Enable verification
    HRESULT EnableVerification( VerificationType type, 
                                 VerificationLocation location,
                                 void *pVerificationData,
                                 TestGraph *pTestGraph,
                                 CGraphVerifier** ppVerifier );

    // Release the verifiers
    void ReleaseVerifiers();
    // Start the verification
    HRESULT StartVerification();
    // Stop the verification
    void StopVerification();
    // Reset the verification and make it ready for another time
    void ResetVerification();
    // Get the result of the verification
    HRESULT GetVerificationResult( VerificationType type, 
                                   void *pVerificationResult,
                                   DWORD cbResultSize);

    // Get a cumulative result from all the verifiers
    HRESULT GetVerificationResults();

    // Send an update so that the verifier can update its state during verification
    HRESULT UpdateVerifier( VerificationType type,
                        void *pData,
                        DWORD cbDataSize);

    // Add Queued Verifiers to the filter graph
    HRESULT AddQueuedVerifiers(TestGraph* pTestGraph);

private:
    CVerificationMgr();
    static CVerificationMgr *m_VerificationMgr;

    BOOL    PopulateMap( VerificationMap *pMap );

    DWORD                m_dwCurrentIndex;
    VerificationMap        m_TestVerifications;
    VerifierMap            m_TestVerifiers;
    static DWORD           m_dwRefCount;        
    static HANDLE       m_hMutex;

    InitInfoList        m_QueuedVerifiers;        // List of verifiers to be added when AddQueuedVerifiers() is called
    BOOL                m_bInitialized;
};

// Verification data strcutures
// This contains the actual sample that was tapped
struct SampleVerificationData
{
    LONGLONG currpos;
    LONGLONG pts;
    LONGLONG dts;
    LONGLONG ats;
    DWORD buflen;
    BYTE* pBuffer;
};

// This contains the sample time-stamps
struct TimeStampVerificationData
{
    LONGLONG currpos;
    LONGLONG pts;
    LONGLONG dts;
    LONGLONG ats;
};

// SampleDeliveryVerifier 
struct SampleDeliveryVerifierData
{
    DWORD filterType;
    PIN_DIRECTION pindir;
    GUID mediaMajorType;
    DWORD tolerance;
    GraphEvent event;
};

struct PlaybackDurationData
{
    LONGLONG firstSampleTime;
    LONGLONG eosTime;
};

struct TimeStampInitData
{
    DWORD dwTolerance;
    TCHAR pszIndexFile[TEST_MAX_PATH];
};

struct GraphBuildInitData
{
    DWORD dwNumItems;
    TCHAR* pszFilterNames;
};


// This describes the verification request to to the server
struct ServerVerificationRequest
{
    VerificationType type;
    void* pVerificationData;
};

const TCHAR *
GetVerificationName( VerificationType type );

const TCHAR* 
GetVerificationString( VerificationType type );

// Is this verification possible
bool 
IsVerifiable( VerificationType type );


// Factory method for creating verifiers
HRESULT 
CreateGraphVerifier( VerificationType type,
                       VerificationLocation location,
                       void *pVerificationData, 
                       void *pOwner,                     // Either a pointer to a testgraph or a tap filter, depending on whether it's a remote filter graph
                       CGraphVerifier **ppGraphVerifier );


#endif
