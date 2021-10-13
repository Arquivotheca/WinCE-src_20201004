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

#include "logging.h"
#include "globals.h"
#include "TuxGraphTest.h"
#include "TestGraph.h"
#include "TestDesc.h"
#include "Utility.h"

PlaybackMedia *
FindPlaybackMedia( PBMediaList* pMediaList, TCHAR* szMediaName, int len )
{
    for ( DWORD i = 0; i < pMediaList->size(); i++ )
    {
        PlaybackMedia* pMedia = pMediaList->at(i);
        if (!_tcsncmp(pMedia->GetName(), szMediaName, len))
            return pMedia;
    }
    return NULL;
}

// ctor of TestConfig
TestConfig::TestConfig()
{
    bGenerateId = false;
    // Get the verification manager
    m_pVerifMgr = CVerificationMgr::Instance();
};

// dtor of TestConfig
TestConfig::~TestConfig()
{        
    // Cleanup the media list
    PBMediaList::iterator miterator = mediaList.begin();
    while(miterator != mediaList.end())
    {
        PlaybackMedia* pMedia = *miterator;
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

    // clean up the verification map
    if ( m_pVerifMgr )
    {
        m_pVerifMgr->Release();
        m_pVerifMgr = NULL;
    }
}


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
    // if filterList is not empty, we need to free strings 
    // it holds
    for ( DWORD i = 0; i < filterList.size(); i++ )
    {
        TCHAR *pTmp = filterList[i];
        if ( pTmp ) delete pTmp;
        pTmp = 0x0;
    }
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

    m_bDRMContent = FALSE;
    m_bChamber = FALSE;
    m_hrExpected = S_OK;
    m_bRemoteGB = FALSE;
}

TestDesc::~TestDesc()
{
    // Cleanup the media list
    EmptyStringList(mediaList);

    // Cleanup the verification list
    VerificationList::iterator viterator = verificationList.begin();
    while(viterator != verificationList.end())
    {
        // BUGBUG: we have to cast the void * back to certain type before delete.
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

PlaybackMedia* TestDesc::GetMedia(int i)
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
    PlaybackMedia* pMedia = FindPlaybackMedia( GraphTest::m_pTestConfig->GetMediaList(), 
                                               szMedia, 
                                               countof(szMedia) );

    if (!pMedia)
        return NULL;

    // First, try an IIS Download of the file
    hr = pMedia->SetProtocol(TEXT("IIS_HTTP"));
    if ( ( SUCCEEDED(hr) ) && ( _tcscmp(szDownloadLocation, TEXT("")) ) )
    {
        hr = pMedia->SetDownloadLocation(szDownloadLocation);
        if ( SUCCEEDED(hr) )
        {
            return pMedia;
        }
    }

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
    {
        // Download the test clip to the local machine.  Try different places, the default
        // is Hard Disk
        hr = pMedia->SetDownloadLocation( szDownloadLocation );
        if ( FAILED(hr) )
        {
            // Set Temp as the secondary location to attempt
            StringCchCopyEx( szDownloadLocation, 
                 TEST_MAX_PATH,
                 TEXT("\\Temp\\"),
                 NULL,
                 NULL,
                 STRSAFE_FILL_BEHIND_NULL );


            hr = pMedia->SetDownloadLocation( szDownloadLocation );
        }
        // Last chance for if Download fails is to check if the download source is an
        // UNC path.  If it is, just set our local playback location to the UNC address
        if ( FAILED(hr) )
        {
            hr = pMedia->SetProtocol(TEXT("Disk"));
            if ( SUCCEEDED(hr) )
            {
                hr = StringCchCopyEx( szDownloadLocation, 
                     TEST_MAX_PATH,
                     pMedia->GetUrl(TEXT("Disk")),
                     NULL,
                     NULL,
                     STRSAFE_FILL_BEHIND_NULL );
                LOG(TEXT("%S: Will attempt playback from path %s."), __FUNCTION__, szDownloadLocation);
            }
        }
    }

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
            PlaybackMedia* pMedia = FindPlaybackMedia( GraphTest::m_pTestConfig->GetMediaList(), 
                                                       szMedia, 
                                                       countof(szMedia) );
            if ( pMedia )
                pMedia->Reset();
        }
        else {
            LOG(TEXT("%S: ERROR %d@%S - failed to reset media %s."), __FUNCTION__, __LINE__, __FILE__, szMedia);
        }
        iterator++;
    }

}

// Adjustment of test result based on current value, current HRESULT, & expected HRESULT values
DWORD TestDesc::AdjustTestResult(HRESULT hr, DWORD currentRetVal)
{
    DWORD dwRetVal = currentRetVal;

    //BUGBUG: to avoid false negative results, check S_FALSE.
    if(dwRetVal == TPR_PASS && hr != TestExpectedHR() && hr != S_FALSE )
    {
        LOG(TEXT("%S: Changing PASSING result to FAILED. Expected HResult (0x%x) does not match returned HResult (0x%x)."), __FUNCTION__, TestExpectedHR(), hr );   
        dwRetVal = TPR_FAIL;
    }
    else if(dwRetVal == TPR_FAIL && FAILED(hr) && hr == TestExpectedHR())
    {
        LOG(TEXT("%S: Changing FAILED result to PASSING. Expected HResult and Returned HResult match (0x%x). Failure Expected."), __FUNCTION__, hr);
        dwRetVal = TPR_PASS;
    }

    return dwRetVal;    
}

PlaceholderTestDesc::PlaceholderTestDesc()
{
}

PlaceholderTestDesc::~PlaceholderTestDesc()
{
}


//
// Use state change sequence in the test config and get the next state in the sequence.
//
HRESULT GetNextState(StateChangeSequence sequence, FILTER_STATE current, FILTER_STATE* pNextState)
{
    FILTER_STATE state=State_Stopped;
    
    if ( !pNextState || None == sequence )
        return E_FAIL;
    
    unsigned int randVal=0;

    switch(sequence)
    {
    case StopPause:
        state = (current != State_Stopped) ? State_Stopped : State_Paused;
        break;
    case StopPlay:
        state = (current != State_Stopped) ? State_Stopped : State_Running;
        break;
    case PauseStop:
        state = (current != State_Paused) ? State_Paused : State_Stopped;
        break;
    case PausePlay:
        state = (current != State_Paused) ? State_Paused : State_Running;
        break;
    case PlayPause:
        state = (current != State_Running) ? State_Running : State_Paused;
        break;
    case PlayStop:
        state = (current != State_Running) ? State_Running : State_Stopped;
        break;
    case PausePlayStop:
        break;        
    case RandomSequence:
        rand_s(&randVal);
        state = (FILTER_STATE)(randVal%3);
        break;
    default:
        return E_FAIL;
    }
    
    *pNextState = state;
    return S_OK;
    
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