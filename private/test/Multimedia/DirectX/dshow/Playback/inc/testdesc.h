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

#ifndef TEST_DESC_H
#define TEST_DESC_H

#include <vector>
#include <map>
#include "globals.h"
#include "FilterDesc.h"
#include "GraphDesc.h"
#include "PlaybackMedia.h"
#include "Verification.h"
#include "VideoRenderer.h"
#include "Operation.h"

class TestDesc;
class TestGroup;
typedef vector<TestGroup*> TestGroupList;
typedef vector<TestDesc*> TestDescList;
typedef vector<TCHAR*> StringList;
typedef vector<FILTER_STATE> StateList;

enum {
    TEST_NAME_LENGTH = 512,
    TEST_DESC_LENGTH = 1024
};

class TestGroup
{
public:
    TestDescList testDescList;
    TCHAR szGroupName[TEST_NAME_LENGTH];
    INT testIdBase;

public:
    TestGroup() : testIdBase(-1)
    {
        memset(szGroupName, 0, sizeof(szGroupName));
    }
    TestDescList* GetTestDescList()
    {
        return &testDescList;
    }
};

PlaybackMedia *
FindPlaybackMedia( PBMediaList* pMediaList, TCHAR* szMediaName, int len );

// TestDesc contains
// - the test func to be run, which is filled in subsequent to parsing
// - the media on which to run on
// - the Tux test id
// - a common interface to get at the media files specified
class TestDesc
{
public:
    // The name of the test
    TCHAR szTestName[TEST_NAME_LENGTH];
    
    // The description of the test
    TCHAR szTestDesc[TEST_DESC_LENGTH];

    // The numeric test id
    INT testId;
    
    // The test function to call
    TESTPROC testProc;

    // The list of media names if there is a list
    StringList mediaList;

    TCHAR szDownloadLocation[TEST_MAX_PATH];

    // The list of verifications
    VerificationList verificationList;

public:
    // Constructor
    TestDesc();

    // Destructor
    virtual ~TestDesc();

    // The number of media files
    virtual int GetNumMedia();

    // Interface to retrieve each of the media
    virtual PlaybackMedia* GetMedia(int i);

    // Extract verification required
    virtual bool GetVerification(VerificationType type, void** pVerificationData = NULL);

    // Get the list of verifications required
    virtual VerificationList GetVerificationList();

    // Reset the test
    virtual void Reset();

    // Add support for DRM tests
    BOOL DRMContent() { return m_bDRMContent; }
    void DRMContent( BOOL bDRM ) { m_bDRMContent = bDRM; }
    BOOL SecureChamber() { return m_bChamber; }
    void SecureChamber( BOOL bDRM ) { m_bChamber = bDRM; }
    BOOL RemoteGB() { return m_bRemoteGB; }
    void RemoteGB( BOOL bRemote ) { m_bRemoteGB = bRemote; }
    HRESULT TestExpectedHR() { return m_hrExpected; }
    void TestExpectedHR( HRESULT hrTest ) { m_hrExpected = hrTest; }

    // Adjustment of test result based on current value, current HRESULT, & expected HRESULT values
    DWORD AdjustTestResult(HRESULT hr, DWORD retval);

private:
    HRESULT m_hrExpected;
    BOOL m_bDRMContent;
    BOOL m_bChamber;
    BOOL m_bRemoteGB;
};

class PlaceholderTestDesc : public TestDesc
{
public:
    PlaceholderTestDesc();
    ~PlaceholderTestDesc();
};

enum StateChangeSequence {
    None,
    StopPause,
    StopPlay,
    PauseStop,
    PausePlay,
    PlayPause,
    PlayStop,
    PausePlayStop,
    RandomSequence
};

struct StateChangeData {
    // The initial state
    FILTER_STATE state;

    // The number of state changes required
    DWORD nStateChanges;

    // State change sequence
    StateChangeSequence sequence;

    // Time to delay between states (ms)
    DWORD dwTimeBetweenStates;
};


// Get the next state in the state change sequence
HRESULT 
GetNextState(StateChangeSequence sequence, FILTER_STATE current, FILTER_STATE* pNextState);
// String cleanup
void EmptyStringList(StringList stringList);

class TestList
{
public:
    TestDescList testList;
    TestList();
    ~TestList();

public:
    int GetNumTests();
    TestDesc* GetTestDesc(int i);
};

class TestConfig
{
public:
    PBMediaList mediaList;
    TestDescList testDescList;
    TestGroupList testGroupList;
    bool bGenerateId;

public:
    TestConfig();
    ~TestConfig();

    bool GenerateId()
    {
        return bGenerateId;
    }

    FilterDesc* GetFilterDescFromName(TCHAR* szFilterName)
    {
        return NULL;
    }

    void GetFilterDescFromName(TCHAR* szFilterName, FilterId &id );

    PBMediaList* GetMediaList()
    {
        return &mediaList;
    }

    TestDescList* GetTestDescList()
    {
        return &testDescList;
    }

    TestGroupList* GetTestGroupList()
    {
        return &testGroupList;
    }

private:
    CVerificationMgr    *m_pVerifMgr;
};

#endif // TEST_DESC_H

#if 0
    // The number of media files
    int GetNumMedia();

    // Interface to retrieve each of the media
    Media* GetMedia(int i);

    // Interface to retrieve the media list
    MediaList* GetMediaList();

    // Get the test method
    TESTPROC GetTestProc();

    // Set the test method
    TESTPROC SetTestProc();

    // Retrieve the actual graph structure
    GraphDesc* GetGraph();

    // Get the tolerance threshold in ms for building graphs
    LONGLONG GetGraphBuildTolerance();

    // The number of filters specified
    int GetNumFilters();
    
    // Method to retrieve each filter
    FilterDesc* GetFilter(int i);

    // Method to retrieve the filter list
    FilterDescList* GetFilterList();

    // The tolerance of the playback duration
    int GetDurationTolerance();

    // The starting position
    LONGLONG GetStartPos();

    // The stop position
    LONGLONG GetStopPos();

    // The list of positions to seek to
    PositionList* GetPositionList();

    // How accurate should the seek frame be?
    FrameAccuracy GetFrameAccuracy();

    // The alternating state changes
    void GetAlternatingStates(FILTER_STATE* pState1, FILTER_STATE* pState2);

    // Get the start position (in % of duration)
    int GetStart();

    // Get the end position (in % of duration)
    int GetEnd();

    // Get the rate
    double GetRate();

    // Get the transition time
    LONGLONG GetTransitionTime();

    // Get the new rate
    double GetNewRate();

#endif