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

#include "logging.h"
#include "globals.h"
#include "TestGraph.h"
#include "TestDesc.h"
#include "Utility.h"

extern TestConfig* g_pConfig;

// retrieve the filter description based on the well known filter name. 
void 
TestConfig::GetFilterDescFromName( TCHAR* szFilterName, FilterId &id )
{
	if (!szFilterName)
		return;
	
	int i;
	for( i = 0; i < FilterInfoMapper::nFilters; i++ )
	{
		if ( FilterInfoMapper::map[i] && 
			 FilterInfoMapper::map[i]->szWellKnownName && 
			 !_tcscmp( FilterInfoMapper::map[i]->szWellKnownName, szFilterName) ) 
		{
			break;
		}
	}

	if ( i == FilterInfoMapper::nFilters ) 
	{
		// didn't find matching filter, return null
		id = UnknownFilter;
	}
	
	id =  FilterInfoMapper::map[i]->id;
}

// String cleanup
void EmptyStringList(StringList stringList)
{
	StringList::iterator iterator = stringList.begin();
	while(iterator != stringList.end())
	{
		TCHAR* szString = *iterator;
		delete szString;
		iterator++;
	}
	stringList.clear();
}

TCHAR* ConnectionDesc::GetFromFilter()
{
	return NULL;
}

TCHAR* ConnectionDesc::GetToFilter()
{
	return NULL;
}

int ConnectionDesc::GetFromPin()
{
	return 0;
}

int ConnectionDesc::GetToPin()
{
	return 0;
}

GraphDesc::GraphDesc()
{
}

GraphDesc::~GraphDesc()
{
}

int GraphDesc::GetNumFilters()
{
	return (int)filterList.size();
}

StringList GraphDesc::GetFilterList()
{
	return filterList;
}

int GraphDesc::GetNumConnections()
{
	return 0;
}

ConnectionDescList* GraphDesc::GetConnectionList()
{
	return NULL;
}


TestDesc::TestDesc() : 
	testId(-1),
	testProc(NULL)
{
	memset(szTestName, 0, sizeof(szTestName));
	memset(szTestDesc, 0, sizeof(szTestDesc));
	memset(szDownloadLocation, 0, sizeof(szDownloadLocation));
}

TestDesc::~TestDesc()
{
	// Cleanup the media list
	EmptyStringList(mediaList);

	// Cleanup the verification list
	VerificationList::iterator viterator = verificationList.begin();
	while(viterator != verificationList.end())
	{
		void* pVerificationData = viterator->second;
		delete pVerificationData;
		viterator++;
	}
	verificationList.clear();
}

int TestDesc::GetNumMedia()
{
	return (int)mediaList.size();
}

Media* TestDesc::GetMedia(int i)
{
	HRESULT hr = S_OK;

	// From the media list - get the name of the media object
	TCHAR*& szMediaProtocol = mediaList[i];
	TCHAR szMedia[MEDIA_NAME_LENGTH];
	TCHAR szProtocol[PROTOCOL_NAME_LENGTH];

	// Get the well known name from the string containing media name and protocol
	hr = ::GetMediaName(szMediaProtocol, szMedia, countof(szMedia));
	if (FAILED(hr))
		return NULL;
    
	// Get the media object corresponding to the well known name
	Media* pMedia = ::FindMedia(g_pConfig->GetMediaList(), szMedia, countof(szMedia));

	if (!pMedia)
		return NULL;

	// Get the protocol from the string containing media name and protocol
	hr = ::GetProtocolName(szMediaProtocol, szProtocol, countof(szProtocol));
	if (FAILED(hr))
		return NULL;

	// Set the protocol in the media object
	hr = pMedia->SetProtocol(szProtocol);
	if (FAILED(hr))
	{

		LOG(TEXT("%S: ERROR %d@%S - failed to set protocol %s."), __FUNCTION__, __LINE__, __FILE__, szProtocol);
		return NULL;
	}


	// Set the download location in the media if needed
	if (_tcscmp(szDownloadLocation, TEXT("")))
		hr = pMedia->SetDownloadLocation(szDownloadLocation);
	
	return (FAILED_F(hr)) ? NULL : pMedia;
}

bool TestDesc::GetVerification(VerificationType type, void** ppVerificationData)
{
	VerificationList::iterator iterator = verificationList.find(type);

	// If a match was found
	if (iterator == verificationList.end())
		return false;

	if (ppVerificationData)
		*ppVerificationData = iterator->second;

	return true;
}

VerificationList TestDesc::GetVerificationList()
{
	return verificationList;
}


void TestDesc::Reset()
{
	StringList::iterator iterator = mediaList.begin();
	TCHAR szMedia[MEDIA_NAME_LENGTH];
	while(iterator != mediaList.end())
	{
		TCHAR* szMediaProtocol = *iterator;

		// Get the well known name from the string containing media name and protocol
		HRESULT hr = ::GetMediaName(szMediaProtocol, szMedia, countof(szMedia));
		if (SUCCEEDED(hr))
		{
			// Get the media object corresponding to the well known name
			Media* pMedia = ::FindMedia(g_pConfig->GetMediaList(), szMedia, countof(szMedia));
			if (pMedia)
				pMedia->Reset();
		}
		else {
			LOG(TEXT("%S: ERROR %d@%S - failed to reset media %s."), __FUNCTION__, __LINE__, __FILE__, szMedia);
		}
		iterator++;
	}

}

PlaceholderTestDesc::PlaceholderTestDesc()
{
}

PlaceholderTestDesc::~PlaceholderTestDesc()
{
}

// BuildGraphTestDesc methods
BuildGraphTestDesc::BuildGraphTestDesc() : TestDesc()
{
	// BUGBUG: seems to erase virtual fn table if any
	memset(this, sizeof(BuildGraphTestDesc), 0);
}

BuildGraphTestDesc::~BuildGraphTestDesc()
{
	EmptyStringList(filterList);
}

// PlaybackTestDesc methods
PlaybackTestDesc::PlaybackTestDesc() :
	TestDesc(),
	start(0),
	stop(0),
	durationTolerance(0)
{
}

PlaybackTestDesc::~PlaybackTestDesc()
{
}

// SetPositionTestDesc methods
SetPosRateTestDesc::SetPosRateTestDesc()
{
	// BUGBUG: seems to erase virtual fn table if any
	memset(this, sizeof(SetPosRateTestDesc), 0);
}

SetPosRateTestDesc::~SetPosRateTestDesc()
{
	positionList.clear();
	posrateList.clear();
}

// StateChangeTestDesc methods
StateChangeTestDesc::StateChangeTestDesc()
{
	memset(this, sizeof(StateChangeTestDesc), 0);
}

StateChangeTestDesc::~StateChangeTestDesc()
{
}

// VRTestDesc methods
VRTestDesc::VRTestDesc()
{
	// Default values

	// Set back buffers to -1 to indicate that it has not been set
	dwMaxBackBuffers = -1;

	// Set the mode to neither GDI nor DDraw
	vrMode = -1;
	vrMode2 = -1;

	// Set the image size to none
	imageSize = IMAGE_SIZE_NONE;

	// Set the flags to 0
	dwVRFlags = 0;

	// Indicate that the surface mode is also not set
	surfaceType = AMDDS_DEFAULT;
	surfaceType2 = AMDDS_DEFAULT;

	// Init the source and dest rectangles
	bSrcRect = false;
	bDstRect = false;
	memset(&srcRect, 0, sizeof(RECT));
	memset(&dstRect, 0, sizeof(RECT));

	// The default state
	bStatePresent = false;
	state = State_Stopped;
}

VRTestDesc::~VRTestDesc()
{
	wndPosList.clear();
	wndScaleList.clear();
	wndRectList.clear();
}

#if 0
TCHAR* TestDesc::GetTestName()
{
	return NULL;
}

// Get the test id
DWORD TestDesc::GetTestId()
{
	return -1;
}

// Get the test method
TESTPROC TestDesc::GetTestProc()
{
	return NULL;
}

// Set the test method
TESTPROC SetTestProc()
{
	return NULL;
}

// The number of media files
int TestDesc::GetNumMedia()
{
	return 0;
}

// Interface to retrieve each of the media
Media* TestDesc::GetMedia(int i)
{
	return NULL;
}

// Interface to retrieve the media list
MediaList* TestDesc::GetMediaList()
{
	return NULL;
}


// BuildGraphTestDesc methods
int BuildGraphTestDesc::GetNumFilters()
{
	return 0;
}

FilterDesc* BuildGraphTestDesc::GetFilter(int i)
{
	return NULL;
}
FilterDescList* BuildGraphTestDesc::GetFilterList()
{
	return NULL;
}

GraphDesc* BuildGraphTestDesc::GetGraph()
{
	return NULL;
}

LONGLONG BuildGraphTestDesc::GetGraphBuildTolerance()
{
	return 0;
}

#endif