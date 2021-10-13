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
////////////////////////////////////////////////////////////////////////////////

#ifndef TEST_DESC_H
#define TEST_DESC_H

#include <vector>
#include <map>
#include "globals.h"
#include "FilterDesc.h"
#include "GraphDesc.h"
#include "Media.h"
#include "Position.h"
#include "Verification.h"
#include "VideoRenderer.h"
#include "Operation.h"

class TestDesc;
class TestGroup;
typedef vector<TestGroup*> TestGroupList;
typedef vector<TestDesc*> TestDescList;
typedef vector<TCHAR*> StringList;

enum {
	TEST_NAME_LENGTH = 512,
	TEST_DESC_LENGTH = 1024
};

class TestGroup
{
public:
	TestDescList testDescList;
	TCHAR szGroupName[TEST_NAME_LENGTH];
	DWORD testIdBase;

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
	DWORD testId;
	
	// The test function to call
	TESTPROC testProc;

	// The list of media names if there is a list
	StringList mediaList;

	TCHAR szDownloadLocation[MAX_PATH];

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
	virtual Media* GetMedia(int i);

	// Extract verification required
	virtual bool GetVerification(VerificationType type, void** pVerificationData = NULL);

	// Get the list of verifications required
	virtual VerificationList GetVerificationList();

	// Reset the test
	virtual void Reset();
};

class PlaceholderTestDesc : public TestDesc
{
public:
	PlaceholderTestDesc();
	~PlaceholderTestDesc();
};

class BuildGraphTestDesc : public TestDesc
{
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

public:
	PlaybackTestDesc();
	virtual ~PlaybackTestDesc();
};

class MultiplePlaybackTestDesc : public PlaybackTestDesc
{
public:

};

enum StateChangeSequence {
	PlayPause,
	PlayStop,
	PauseStop,
	PausePlayStop,
	RandomSequence
};

class StateChangeTestDesc : public TestDesc
{
public:
	// The initial state
	FILTER_STATE state;

	// The number of state changes required
	DWORD nStateChanges;

	// The latency tolerance
	DWORD tolerance;

	// State change sequence
	StateChangeSequence sequence;

	// Time to delay between states (ms)
	DWORD dwTimeBetweenStates;

public:
	StateChangeTestDesc();
	~StateChangeTestDesc();
};

class SetPosRateTestDesc : public TestDesc
{
public:
	// The list of positions
	PositionList positionList;

	// List of positions and rate
	PosRateList posrateList;

	// The frame accurace
	FrameAccuracy accuracy;

public:
	SetPosRateTestDesc();
	virtual ~SetPosRateTestDesc();
};

class TrickModeTestDesc : public TestDesc
{
public:
	int start;
	int end;
	double rate;
	LONGLONG transitionTime;
	double newrate;

public:
	TrickModeTestDesc();
	~TrickModeTestDesc();
};



class VRTestDesc : public TestDesc
{
public:
	// The rendering mode
	int vrMode;

	// The second rendering mode
	int vrMode2;

	// The DDraw surface type
	int surfaceType;

	// The second DDraw surface mode
	int surfaceType2;
	
	// Miscellaneous VR flags
	DWORD dwVRFlags;

	// The max no of back buffers
	DWORD dwMaxBackBuffers;

	// Recommended image size
	VRImageSize imageSize;

	// The list of video window positions to test with
	WndPosList wndPosList;

	// The list of strech/shrink/full screen/min/max to iterate the window through
	WndScaleList wndScaleList;

	// The list of client areas for the occluding window
	WndRectList wndRectList;

	// List of operations
	OpList opList;

	// The state in which to run the test
	FILTER_STATE state;
	bool bStatePresent;

	// Source rectangle
	bool bSrcRect;
	RECT srcRect;

	// Destination rectangle
	bool bDstRect;
	RECT dstRect;

public:
	// Constructor & Destructor
	VRTestDesc();
	~VRTestDesc();
};

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
	MediaList mediaList;
	TestDescList testDescList;
	TestGroupList testGroupList;
	bool bGenerateId;

public:
	TestConfig()
	{
		bGenerateId = false;
	}

	~TestConfig()
	{		
		// Cleanup the media list
		MediaList::iterator miterator = mediaList.begin();
		while(miterator != mediaList.end())
		{
			Media* pMedia = *miterator;
			delete pMedia;
			miterator++;
		}
		mediaList.clear();

		// Cleanup the test desc list
		TestDescList::iterator tditerator = testDescList.begin();
		while(tditerator != testDescList.end())
		{
			TestDesc* pTestDesc = *tditerator;
			delete pTestDesc;
			tditerator++;
		}
		testDescList.clear();

		// Cleanup the test group list
		TestGroupList::iterator tgiterator = testGroupList.begin();
		while(tgiterator != testGroupList.end())
		{
			TestGroup* pTestGroup = *tgiterator;
			delete pTestGroup;
			tgiterator++;
		}
		testGroupList.clear();
	}

	bool GenerateId()
	{
		return bGenerateId;
	}

	FilterDesc* GetFilterDescFromName(TCHAR* szFilterName)
	{
		return NULL;
	}

	void GetFilterDescFromName(TCHAR* szFilterName, FilterId &id );

	MediaList* GetMediaList()
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