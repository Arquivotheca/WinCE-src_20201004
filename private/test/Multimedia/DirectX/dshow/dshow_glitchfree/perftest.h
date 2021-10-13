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
#ifndef PERF_TEST_H
#define PERF_TEST_H

#include <tchar.h>
#include <vector>
#include "tuxmain.H"
#include "Performance.h"
#include "MediaContent.h"
#include "common.h"
#include "PlaylistParser.h"

using namespace std;

typedef basic_string<TCHAR> tstring;

#define ALL                     TEXT("All")

#define PLAYBACK_TEST               TEXT("Playback")

#define STATUS_CPU              TEXT("CPU")
#define STATUS_MEM              TEXT("MEM")

#define PLAYBACK_TEST_DESC          TEXT("Playback Performance Test")

extern TCHAR* SupportedStatus[];
extern TCHAR* SupportedProtocols[];

extern int nSupportedProtocols;

/**
  * Exception class that is thrown on error.
  */
class PerfTestException
{
public:
    PerfTestException(const TCHAR* msg) {
        errno_t err = _tcsncpy_s(message, sizeof(message)/sizeof(TCHAR), msg, _TRUNCATE);
    }
    TCHAR message[MAX_PATH];
    DWORD retval; 
};

/**
  * Really a placeholder class for all the command line parameters. 
  */
class PerfTestConfig
{
private:
    // The list of protocols to iterate over
    vector<TCHAR*> protocols;

    // The list of clip ids
    vector<TCHAR*> clipIDs;

    // The statuses to report
    vector<TCHAR*> statuses;

    //determines how often we sample for memory usage / CPU utalization (ms)
    int iSampleInterval;

    // The playlist file to use
    TCHAR* szPlaylist;

    bool perflog;
    TCHAR* szPerflog;

    // Specify to use DRM
    bool bDRM;

    // Alpha blending perf test
    bool bAlphaBlend;
    float fAlpha;

    // No of repetitions of the test
    int nRepeat;


    //what VR mode to run in
    DWORD dwVRMode;

    //what is the maximum back buffers you want to use while rendering
    int iBackBuffers;
    
private:
    TCHAR* Clone(const TCHAR* string);

public:
    // Constructor
    PerfTestConfig();

    // Destructor
    ~PerfTestConfig();

    // Protocols to be used
    void AddProtocol(TCHAR* protocol);
    int GetNumProtocols();
    void GetProtocol(int protocol, TCHAR* szProtocol);
    const TCHAR* GetProtocol(int test);

    // Clips to be used
    void AddClipID(TCHAR* clipid);
    int GetNumClipIDs();
    void GetClipID(int clipID, TCHAR* szClipID);
    const TCHAR* GetClipID(int test);

    // Statuses to report
    void AddStatusToReport(TCHAR* status);
    int GetNumStatusToReport();
    bool StatusToReport(const TCHAR* szStatusToReport);

    //if we are to monitor the status, how big should the sampling interval be
    void SetSampleInterval(int interval);
    int GetSampleInterval();

    // Playlist to be used
    void SetPlaylist(TCHAR* szPlaylist);
    TCHAR* GetPlaylist();

    // Perflog to be used
    void SetPerflog(TCHAR* szPerflog);
    TCHAR* GetPerflog();
    void GetPerflog(TCHAR*);
    bool UsePerflog();

    void SetDRM(bool drm);
    bool UseDRM();

    void SetAlphaBlend(bool bAlphaBlend);
    bool IsAlphaBlend();
    void SetAlpha(float fAlphaData);
    float GetAlpha();

    void SetRepeat(int  repeat);
    int GetRepeat();

    void SetVRMode(DWORD vrMode);
    DWORD GetVRMode();  

    void SetMaxbackBuffers(int maxBackBuffers);
    DWORD GetMaxbackBuffers();
    
};

/**
  * This structure contains the statistics collected during playback testing.
  */
struct PlaybackStats {
    long droppedFramesDecoder;
    int droppedFramesRenderer;
    int framesDrawn;
    int droppedFramesDelivery;
    int avgFrameRate;
    int avgSyncOffset;
    int devSyncOffset;
    int jitter;
    DWORD audioDiscontinuities;
    DWORD actualDuration;
    long receivedPackets;
    long lostPackets;
    long recoveredPackets;
    int avgCpuUtilization;
};

// The different protocols that we support
#define PROTO_LOCAL _T("Local")
#define PROTO_HTTP  _T("HTTP")
#define PROTO_MMS   _T("MMS")
#define PROTO_MMSU  _T("MMSU")
#define PROTO_MMST  _T("MMST")

//the main class executing our test

class CPlaybackTest
{
private:
    PlaybackStats pbStats;
    CPerformanceMonitor *pCPUMon;

private:
    void Init();
    HRESULT GetAudioVideoPlaybackStats(CMediaContent* pMediaContent, FilterGraph* pFG, PlaybackStats* pStats);
    TESTPROCAPI TestSystemPlaybackQuality(CMediaContent* pMediaContent, FilterGraph* pFG, PlaybackStats *pStats);
    TESTPROCAPI TestVideoPlaybackQuality(CMediaContent* pMediaContent, FilterGraph* pFG, PlaybackStats *pStats);
    TESTPROCAPI TestAudioPlaybackQuality(CMediaContent* pMediaContent, FilterGraph* pFG, PlaybackStats *pStats);
    TESTPROCAPI PlaybackPerfTest(PerfTestConfig* pConfig, CMediaContent* pMediaContent1, TCHAR *url[]);

    // test function for alpha blending
    TESTPROCAPI AlphaBlendingTest( HANDLE hLogFile, PerfTestConfig* pConfig, CPlaylistParser *pPlaylist, int rep );

public:
    CPlaybackTest();
    ~CPlaybackTest();

    TESTPROCAPI Execute(PerfTestConfig* pConfig);
};

//The actual tux test that gets called, entry point to the test
extern TESTPROCAPI SystematicPerfTest(UINT uMsg, 
                                      TPPARAM tpParam, 
                                      LPFUNCTION_TABLE_ENTRY lpFTE );

#endif // PERF_TEST_H