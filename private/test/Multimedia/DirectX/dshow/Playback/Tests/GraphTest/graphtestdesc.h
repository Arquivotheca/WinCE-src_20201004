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

#ifndef GRAPH_TEST_DESC_H
#define GRAPH_TEST_DESC_H

#include "TestDesc.h"

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)       do{ if ((p)!=NULL) {p->Release(); p=NULL;} }while(0)
#endif

struct MarkerInfo
{
    int markerNum;
    BSTR markerName;
    double markerTime;
};

typedef std::vector<MarkerInfo*> MediaMarkerInfo;

class BuildGraphTestDesc : public TestDesc
{
private:
    DWORD  m_dwNumOfFilters;
    GUID   m_Guid;

public:
    GUID *GetTestGuid() { return &m_Guid; }
    void SetTestGuid( GUID *pGuid );
    DWORD GetNumOfFilters() { return m_dwNumOfFilters; }
    void SetNumOfFilters( DWORD dwFilters ) { m_dwNumOfFilters = dwFilters; }
    TCHAR *GetFilterName( DWORD dwIndex );

public:
    // The structure of the expected graph
    GraphDesc graphDesc;

    // Tolerance
    DWORD dwBuildTimeTolerance;

    // List of of filters
    StringList filterList;

    // Graph build tolerance time
    LONGLONG GraphBuildTolerance;

public:
    // Constructor & Destructor
    BuildGraphTestDesc();
    ~BuildGraphTestDesc();
};

class PlaybackTestDesc : public TestDesc
{
public:
    // The start time
    LONGLONG start;

    // The stop etime
    LONGLONG stop;

    // The tolerance for playback duration
    int durationTolerance;

    // Is this an xBVT test case
    bool XBvt;
	
	//Do you want to monitor resource during multiple graph test?
	bool resMonitor;

    FILTER_STATE state;
public:
    PlaybackTestDesc();
    virtual ~PlaybackTestDesc();
};

class StateChangeTestDesc : public TestDesc
{
public:
    // State change data
    StateChangeData        m_StateChange;
    // The latency tolerance
    DWORD tolerance;
    // Is this an xBVT test case
    bool XBvt;
	//Do you want to monitor resource during multiple graph test?
	bool resMonitor;
public:
    StateChangeTestDesc();
    ~StateChangeTestDesc();
};

class PlaybackInterfaceTestDesc : public TestDesc
{
public:
    //For IBasicAudio test
    int tolerance;
    
    //For IAMNetShowExProps test
    long protocol;
    long bandWidth;
    long codecCount;    

    //For IAMDroppedFrames test
    long totalFrameCount;
    long droppedFrameRate;

    //For IAMNetworkStatus test
    int isBroadcast;
   
    //For IAMExtendedSeeking test    
    long markerCount ;
    long exCapabilities;
    MediaMarkerInfo mediaMarkerInfo;

    //for IAMNetWorkConfig test    
    BSTR pszHost;
    long testPort;
    long bufferTime;

public:
    PlaybackInterfaceTestDesc();
    virtual ~PlaybackInterfaceTestDesc();
    void clearMem();
};

class AudioControlTestDesc : public TestDesc
{
public:
    // Use bluetooth
    bool    UseBT;
    // Add an additional audio renderer to the graph
    bool    ExtraRend;
    // Which renderer to use (0 = default, 1 = 1st, 2 = 2nd)
    DWORD   SelectRend;
    // Check inputs
    bool    Input;
    // Provide onscreen prompts for manual tests
    bool    Prompt;

public:
    AudioControlTestDesc();
    ~AudioControlTestDesc();
    void SetUseBT() { UseBT = true; }
    void SetExtraRend() { ExtraRend = true; }
    void SetSelectRend( DWORD dwSelectRend ) { SelectRend = dwSelectRend; }
    void SetInput() { Input = true; }
    void SetPrompt() { Prompt = true; }
};

#endif
