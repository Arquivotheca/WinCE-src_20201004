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
#include <nserror.h>
#include <dshow.h>
#ifdef UNDER_CE
// ATL headers
//#include <atlbase.h>
#include <atlcomcli.h>
#include <initguid.h>
#include <bdaiface.h>
#include "RenderMp2Ts.h"
#endif

#include "logging.h"
#include "globals.h"
#include "TestDesc.h"
#include "TestGraph.h"
#include "PinEnumerator.h"
#include "StringConversions.h"
#include "ValidType.h"
#include "utility.h"
#include "strsafe.h"


#ifdef UNDER_CE
using namespace RenderMpeg2TransportStream;
#endif

BOOL TestGraph::m_bSaveGraph = NULL;
DWORD TestGraph::m_dwGraphID = 0;

DWORD TestGraph::m_dwGraphSubID = 0;
//EXTERN_GUID(CLSID_PullToPushFilter, 0x7d07ae87, 0x2e1a, 0x4e8a, 0x99, 0x6f, 0x8f, 0x1f, 0x80, 0x51, 0xfd, 0x1f);
EXTERN_GUID(CLSID_MFMPEG2Demultiplexer, 0xf792beee, 0xaeaf, 0x4ebb, 0xab, 0x14, 0x8b, 0xc5, 0xc8, 0xc6, 0x95, 0xa8);

// static function to repair graph after inserting tap filter.
static bool RepairGraphAfterInsertTapFilter( IPin *pFrom, IPin *pTo, IGraphBuilder* pGraphBuilder );

TestGraph::TestGraph() : 
    m_pGraphBuilder(NULL),
    m_pMediaControl(NULL),
    m_pMediaSeeking(NULL),
    m_pMediaPosition(NULL),
    m_pMediaEvent(NULL),
    m_pMediaEventEx(NULL),
    m_pBasicVideo(NULL),
    m_pVideoWindow(NULL),
    m_pVMRWindowlessControl(NULL),
    m_pBasicAudio(NULL),

    m_pSourceFilter(NULL),
    m_pAudioRenderer(NULL),
    m_pVideoRenderer(NULL),
    m_pVideoDecoder(NULL),
    m_pAudioDecoder(NULL),
    m_pColorConverter(NULL),
#ifdef UNDER_CE
    m_pVRMode(NULL),
    m_hOutQueue(NULL),
#endif
    // The current media object
    m_pMedia(NULL),

    m_Duration(0),
    m_SeekCaps(0),
    m_Preroll(0),
    m_bMultiLanguageAudio(false),
    m_bMultiBitRateVideo(false),
    m_bDRM(false),

    // NULL the counts
    m_nVideoStreams(0),
    m_nAudioStreams(0),
    m_nTextStreams(0),
    m_nUndeterminedStreams(0),

    // Init the flags
    m_bGraphBuilt(false),
    m_bUseVMR(FALSE),
    m_bRemoteGB(FALSE),

    // Init the positions
    m_StartPosSet(0),
    m_StopPosSet(0),
    // The initial rate
    m_RateSet(1.0),
    m_VideoWindowWidthSet(-1),
    m_VideoWindowHeightSet(-1),
    m_RotateAngle(-1),
    m_pVerifMgr(NULL)
{

    // Init COM
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr))
        LOG(TEXT("%S: ERROR %d@%S - failed to init COM (0x%x) \n"), __FUNCTION__, __LINE__, __FILE__, hr);

    // Init the tap filter list
    m_TapFilters.clear();

    // Get the verification manager
    m_pVerifMgr = CVerificationMgr::Instance();
}

TestGraph::~TestGraph()
{
    Reset();
    if(m_pVerifMgr)
    {
        m_pVerifMgr->Release();
        m_pVerifMgr = NULL;
    }
    //CoUninitialize();
}

void TestGraph::SetFirstSample( bool bFirstSample, LONGLONG firstSampleTime )
{ 
    m_bFirstSample = bFirstSample; 
    m_llFirstSampleTime = firstSampleTime;
}

void TestGraph::SetEOS( bool bEOSDelivered, LONGLONG eosTime )
{ 
    m_bEOS = bEOSDelivered; 
    m_llEOSTime = eosTime;
}

int TestGraph::GetNumVideoStreams()
{
    return m_nVideoStreams;
}

int TestGraph::GetNumAudioStreams()
{
    return m_nAudioStreams;
}

int TestGraph::GetNumTextStreams()
{
    return m_nTextStreams;
}

bool TestGraph::IsAddedSourceFilter()
{
    return (m_pSourceFilter?true:false);
}

UINT TestGraph::GetRotateAngle()
{
    return m_RotateAngle;
}

void TestGraph::SetRotateAngle(int angle)
{
    if(m_RotateAngle == -1)
    {
        //1st time set this angle, since we set -1 for initial value
        m_RotateAngle++;
    }
    m_RotateAngle = (m_RotateAngle + angle)%360;

    // Update verifier state
    m_pVerifMgr->UpdateVerifier(VerifyCodecRotation, &m_RotateAngle, sizeof(m_RotateAngle));
}

#if 0
HRESULT TestGraph::AreFiltersInGraph(FilterDescList filterDescList)
{
    HRESULT hr = S_OK;

    int nFiltersToCheck = filterDescList.size();
    int nFiltersInGraph = m_FilterList.size();

    for(int i = 0; i < nFiltersToCheck; i++)
    {    
        FilterDesc* pFilter = filterDescList[i];
        
        bool bFilterInGraph = false;

        for(int j = 0; j < nFiltersInGraph; j++)
        {
            FilterDesc* pFilterInGraph = m_FilterList[j];
            bFilterInGraph = pFilterInGraph->IsSameFilter(pFilter);
            if (bFilterInGraph)
                break;
        }

        if (!bFilterInGraph)
        {
            hr = S_FALSE;
            break;
        }
    }

    return hr;
}
#endif

HRESULT TestGraph::AreFiltersInGraph(StringList filterNameList)
{
    HRESULT hr = S_OK;

    // In secure playback, m_FilterList will be empty since we couldn't enumerate
    // the graph and populate the list.
    if ( m_bRemoteGB )
        return S_FALSE;

    int nFiltersToCheck = (int)filterNameList.size();
    int nFiltersInGraph = (int)m_FilterList.size();

    for(int i = 0; i < nFiltersToCheck; i++)
    {    
        TCHAR* szFilterName = filterNameList[i];
        
        bool bFilterInGraph = false;

        for(int j = 0; j < nFiltersInGraph; j++)
        {
            FilterDesc* pFilterInGraph = m_FilterList[j];
            bFilterInGraph = pFilterInGraph->IsSameFilter(szFilterName);
            if (bFilterInGraph)
                break;
        }

        if (!bFilterInGraph)
        {
            hr = S_FALSE;
            break;
        }
    }

    return hr;
}

HRESULT TestGraph::AreConnectionsInGraph(ConnectionDescList* pConnectionList)
{
    // TBD: Now two pins that are specified as connected may have an intermediate filter - what to do?
    return E_NOTIMPL;
}


HRESULT TestGraph::IsGraphEquivalent(GraphDesc* pGraphDesc)
{
    HRESULT hr = S_OK;

    // Make sure that the appropriate filters are in the graph
    hr = AreFiltersInGraph(pGraphDesc->filterList);

    if (FAILED_F(hr))
        return hr;


    // Now make sure that the appropriate connections are in the graph

    return hr;
}

void TestGraph::Reset()
{
    // Set the state back to stopped - don't check for errors
    SetState(State_Stopped, TestGraph::SYNCHRONOUS);

    // Reset the verifiers
    ReleaseVerifiers();

    // Release the filter descriptions
    ReleaseFilterList();

    // Release the interfaces acquired
    ReleaseInterfaces();
    
    // reset the built flag
    m_bGraphBuilt = false;

    // reset EOS event flag
    m_bEOS = false;
}

void TestGraph::ReleaseFilterList()
{
    FilterDescList::iterator iterator = m_FilterList.begin();
    while(iterator != m_FilterList.end())
    {
        FilterDesc* pFilterDesc = *iterator;
        delete pFilterDesc;
        iterator++;
    }
    m_FilterList.clear();

    if (m_pSourceFilter)
    {
        m_pSourceFilter->Release();
        m_pSourceFilter = NULL;
    }

    if (m_pAudioRenderer)
    {
        m_pAudioRenderer->Release();
        m_pAudioRenderer = NULL;
    }

    if (m_pVideoRenderer)
    {
        m_pVideoRenderer->Release();
        m_pVideoRenderer = NULL;
    }

    if (m_pAudioDecoder)
    {
        m_pAudioDecoder->Release();
        m_pAudioDecoder = NULL;
    }

    if (m_pVideoDecoder)
    {
        m_pVideoDecoder->Release();
        m_pVideoDecoder = NULL;
    }
    
    if (m_pColorConverter)
    {
        m_pColorConverter->Release();
        m_pColorConverter = NULL;
    }
}


HRESULT TestGraph::SetMedia( PlaybackMedia* pMedia)
{
    m_pMedia = pMedia;
    return S_OK;
}

PlaybackMedia* TestGraph::GetCurrentMedia()
{
    // before returning the media, get the media information
    // 
    HRESULT hr = S_OK;
    hr = this->GetMediaInfo( m_pMedia );
    if ( FAILED(hr) )
    {
        LOG( TEXT("GetMediaInfo failed for %s!"), m_pMedia->GetUrl() );
    }
    return m_pMedia;
}

HRESULT TestGraph::CreateEmptyGraph()
{
    HRESULT hr = S_OK;
    if (!m_pGraphBuilder)
    {

#if UNDER_CE
        if(m_bRemoteGB)
            hr = CoCreateInstance(CLSID_RestrictedFilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder2, (void**)&m_pGraphBuilder);
        else
            hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder2, (void**)&m_pGraphBuilder);
#else
        //Desktop has no notion of CLSID_RestrictedFilterGraph or IID_IGraphBuilder2
        hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&m_pGraphBuilder);
#endif

    }
    return hr;
}

HRESULT TestGraph::AddSourceFilter()
{
    // If source filter already exists or the media has not been set then return error
    if (m_pSourceFilter || !m_pMedia)
        return E_FAIL;

    // Create an empty graph if one doesn't exist already
    HRESULT hr = CreateEmptyGraph();
    if (FAILED(hr) || (hr == S_FALSE))
        return hr;
    
    // Add a source filter to the empty graph
    WCHAR wszUrl[TEST_MAX_PATH];
    TCharToWChar(m_pMedia->GetUrl(), wszUrl, countof(wszUrl));
    hr = m_pGraphBuilder->AddSourceFilter(wszUrl, WSTR_SourceFilter, &m_pSourceFilter);
    if (FAILED(hr) || (hr == S_FALSE))
        return hr;
    
    return hr;
}

HRESULT TestGraph::BuildEmptyGraph()
{
    return CreateEmptyGraph();
}

IGraphBuilder* TestGraph::GetGraph()
{
    if (m_pGraphBuilder)
        m_pGraphBuilder->AddRef();
    return m_pGraphBuilder;
}

HRESULT TestGraph::FindInterfaceOnGraph(REFIID riid, void **ppInterface, TCHAR* szName)
{
    HRESULT hr = S_OK;
    IUnknown* pIUnknown = NULL;
    
#ifdef UNDER_CE
    WCHAR wszFilterName[FILTER_NAME_LENGTH];
    IEnumFilterInterfaces* pIEnumFilterInterfaces = NULL;
 
    if(FAILED(hr = ((IGraphBuilder2*)m_pGraphBuilder)->FindInterfacesOnGraph(riid, &pIEnumFilterInterfaces)))
        goto cleanup;
        
    while(S_OK == (hr = pIEnumFilterInterfaces->Next(&pIUnknown, FILTER_NAME_LENGTH, wszFilterName)))
    {
        // Return the first instance of the interface found unless the caller gives us a filter name to match
        if(!szName)
            break;
        else
        {
            if(!_wcsnicmp(szName, wszFilterName, FILTER_NAME_LENGTH))
                break;
        }
        
        if(pIUnknown)
        {
            pIUnknown->Release();
            pIUnknown = NULL;
        }
    }

cleanup:
    if(S_OK != hr)
    {
        if(pIUnknown)
            pIUnknown->Release();
    }
    else
    {
        // Use the base filter pointer to QI the wanted interface, not IBaseFilter pointer
        // itself
        if(pIUnknown)
        {
            hr = pIUnknown->QueryInterface(riid, ppInterface);
            pIUnknown->Release();
        }
    }
    
    if(pIEnumFilterInterfaces)
        pIEnumFilterInterfaces->Release();

#else
    hr = E_NOTIMPL;
#endif
    
    return hr;

}

HRESULT TestGraph::FindInterfaceOnFilter(REFIID riid, void **ppInterface)
{
    CheckPointer( ppInterface,E_POINTER );
    HRESULT hr = E_NOINTERFACE;
    IBaseFilter* pFilter;
    ULONG ulFetched = 0;    
    IEnumFilters* pEnumFilter = NULL;
    
    if(!ppInterface)
        return E_POINTER;

    if (!m_pGraphBuilder)
        return E_FAIL;

    // Enumerate the filters
    hr = m_pGraphBuilder->EnumFilters(&pEnumFilter);
    if (FAILED(hr))
        return hr;

    // Find the first filter in the graph that supports riid interface
    pEnumFilter->Reset();
    
    while(pEnumFilter->Next(1, &pFilter, NULL) == S_OK)
    {
        hr = pFilter->QueryInterface(riid, ppInterface);
        pFilter->Release();
        if (SUCCEEDED(hr))
            break;
    }

    pEnumFilter->Release();
    
    return hr;
}

HRESULT TestGraph::BuildGraph()
{
    HRESULT hr = S_OK;

    // If the graph has already been built, then return an error
    if ( !m_bUseVMR && m_bGraphBuilt )
        return E_FAIL;

    // If the media has not been set, then return an error as well
    if (!m_pMedia)
        return E_FAIL;
    
    if(m_bRemoteGB)
    {
        // Look to see if there is a .GRF file to load and use that if we find it.
        FILE* pFile = NULL;
        TCHAR szFileName[TEST_MAX_PATH];
        TCHAR szFileID[32];
        errno_t error;
        szFileName[0] = _T('\0');            
        StringCchCopy(szFileName, TEST_MAX_PATH, _T("\\Temp\\"));    
        error = _itot_s(m_dwGraphID, szFileID,_tcslen(szFileID),10);
        if(error != 0)
        {
          LOG(TEXT("ERROR:TestGraph::BuildGraph -_itot_s"));
          return E_FAIL;
        }
        StringCchCat(szFileName, TEST_MAX_PATH,szFileID);
        StringCchCat(szFileName, TEST_MAX_PATH, _T("-"));
        error = _itot_s(m_dwGraphSubID, szFileID,_tcslen(szFileID), 10);
        if(error != 0)
        {
          LOG(TEXT("ERROR:TestGraph::BuildGraph -_itot_s"));
          return E_FAIL;
        }
        StringCchCat(szFileName, TEST_MAX_PATH,szFileID);
        StringCchCat(szFileName, TEST_MAX_PATH, _T(".GRF"));
        error = _tfopen_s(&pFile,szFileName, _T("r"));
        if( (error != 0) || (pFile == NULL))
        {
            fclose(pFile);

            // We found a matching .GRF file, so let's commit to using that
            if(FAILED(hr = BuildGraph(szFileName)))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to construct filter graph from saved GRF file %s (0x%x) \n"), __FUNCTION__, __LINE__, __FILE__, szFileName, hr);
            }
            else
            {
                LOG(TEXT("%S: successfully built filter graph from saved GRF file \n"), __FUNCTION__, szFileName);
            }

            m_dwGraphSubID++;       // Up the Sub ID in case we are creating multiple graphs / VMR streams in this test.
            goto cleanup;            
        }
    }

    // We either are working locally or didn't find a GRF file, so let's try building a graph
    // from scratch.

    if ( !m_pGraphBuilder )
        hr = CreateEmptyGraph();


#ifdef UNDER_CE
    if (SUCCEEDED_F(hr))
    {
        WCHAR wszUrl[TEST_MAX_PATH] = {0};
        TCharToWChar(m_pMedia->GetUrl(), wszUrl, countof(wszUrl));        
        Graph graphRender; 
        hr = graphRender.RenderFile(m_pGraphBuilder,wszUrl);
        // We need to try other local storage media if one fails due to FILE_NOT_FOUND  error.
        if ( NS_E_FILE_NOT_FOUND == hr )
        {
            LOG( TEXT("Render %s failed. (0x%08x)"), m_pMedia->GetUrl(), hr );
            WCHAR wszFileName[TEST_MAX_PATH] = {0};
            TCharToWChar( m_pMedia->m_szFileName, wszFileName, countof(wszFileName) );
            if ( wcsstr( wszUrl, L"\\Hard Disk\\") )
            {
                _tcscpy_s( wszUrl, countof(wszUrl), L"\\Storage Card\\" );
                _tcscat_s( wszUrl, countof(wszUrl), wszFileName );                             
                hr = graphRender.RenderFile(m_pGraphBuilder,wszUrl); 
                LOG( TEXT("Try to render %s"),  wszUrl );

            }
            else if ( wcsstr( wszUrl, L"\\Storage Card\\") )
            {
                _tcscpy_s( wszUrl, countof(wszUrl), L"\\Hard Disk\\" );
                _tcscat_s( wszUrl, countof(wszUrl), wszFileName );                             
                hr = graphRender.RenderFile(m_pGraphBuilder,wszUrl); 
                LOG( TEXT("Try to render %s"),  wszUrl );

            }

        }

    }
#endif

#ifdef UNDER_NT
    if (SUCCEEDED_F(hr))
    {
        WCHAR wszUrl[TEST_MAX_PATH] = {0};
        TCharToWChar(m_pMedia->GetUrl(), wszUrl, countof(wszUrl));        
        hr = m_pGraphBuilder->RenderFile(wszUrl, NULL);
        // We need to try other local storage media if one fails due to FILE_NOT_FOUND  error.
        if ( NS_E_FILE_NOT_FOUND == hr )
        {
            LOG( TEXT("Render %s failed. (0x%08x)"), m_pMedia->GetUrl(), hr );
            WCHAR wszFileName[TEST_MAX_PATH] = {0};
            TCharToWChar( m_pMedia->m_szFileName, wszFileName, countof(wszFileName) );
            if ( wcsstr( wszUrl, L"\\Hard Disk\\") )
            {
                _tcscpy_s( wszUrl, countof(wszUrl), L"\\Storage Card\\" );
                _tcscat_s( wszUrl, countof(wszUrl), wszFileName );                             
                hr = m_pGraphBuilder->RenderFile(wszUrl, NULL); 
                LOG( TEXT("Try to render %s"),  wszUrl );

            }
            else if ( wcsstr( wszUrl, L"\\Storage Card\\") )
            {
                _tcscpy_s( wszUrl, countof(wszUrl), L"\\Hard Disk\\" );
                _tcscat_s( wszUrl, countof(wszUrl), wszFileName );                             
                hr = m_pGraphBuilder->RenderFile(wszUrl, NULL); 
                LOG( TEXT("Try to render %s"),  wszUrl );

            }
        }
    }
#endif

    // Set as built
    if (SUCCEEDED_F(hr))
        hr = SetBuilt();

cleanup:

    return hr;
}


// Build a graph from an existing .GRF file that contains prebuilt filter graph data.
HRESULT TestGraph::BuildGraph(TCHAR* szGRFFile)
{
    HRESULT hr = S_OK;
    WCHAR wszName[TEST_MAX_PATH];

    // If the graph has already been built, then return an error
    if ( !m_bUseVMR && m_bGraphBuilt || !szGRFFile )
        return E_FAIL;

    // See if we need to clear out any residual state.  (We wipe out everything when we build from a GRF file)
    if ( m_pGraphBuilder )
    {
        LOG(TEXT("Resetting Filter Graph to clear existing data before construction from GRF file: %s ."), szGRFFile);
        
        Reset();
    }

    if(FAILED(hr = CreateEmptyGraph()))
        goto cleanup;

    TCharToWChar(szGRFFile, wszName, countof(wszName));

    hr = m_pGraphBuilder->RenderFile(szGRFFile, NULL);
/*
    if(FAILED(hr = StgOpenStorage(wszName, 
                            0, 
                            STGM_TRANSACTED | STGM_READ | STGM_SHARE_DENY_WRITE, 
                            0,
                            0, 
                            &pStorage)))
        goto cleanup;
    
    
    if(FAILED(hr = m_pGraphBuilder->QueryInterface(IID_IPersistStream,
             reinterpret_cast<void**>(&pPersistStream))))
        goto cleanup;
    
    if(FAILED(hr = pStorage->OpenStream(L"ActiveMovieGraph", 
                            0, 
                            STGM_READ | STGM_SHARE_EXCLUSIVE, 
                            0, 
                            &pStream)))
        goto cleanup;

    if(FAILED(hr = pPersistStream->Load(pStream)))
        goto cleanup;
*/  
    // Set as built 
    hr = SetBuilt();
    
cleanup:
    
    return hr;
}


HRESULT TestGraph::DetermineStreams()
{
    // Query the media object for the number of video streams
    // m_nVideoStreams = m_pMedia->GetNumVideoStreams();
    // m_nAudioStreams = m_pMedia->GetNumAudioStreams();
    //m_nTextStreams = m_pMedia->GetNumTextStreams();

    // TBD: Parse the structure of the graph
    // 1. Enumerate the filters in teh graph
    // 2. Determine the connections

    // BUGBUG: hack
    AcquireInterfaces();

    if (m_pAudioRenderer)
        m_nAudioStreams = 1;    
    
    if (m_pVideoRenderer)
        m_nVideoStreams = 1;

    return S_OK;
}


HRESULT TestGraph::DetermineFilterList()
{
    if ( !m_pGraphBuilder)
        return E_FAIL;

    HRESULT hr = S_OK;
    IEnumFilters *pEnumFilters = NULL;
    hr = m_pGraphBuilder->EnumFilters(&pEnumFilters);
    if (FAILED(hr))
        return hr;

    IBaseFilter* pFilter = NULL;
    DWORD nFetched = 0;
    pEnumFilters->Reset();
    
    // Enumerate the first filter
    hr = pEnumFilters->Next(1, &pFilter, &nFetched);
    while (SUCCEEDED_F(hr))
    {
        TCHAR szURL[TEST_MAX_PATH];
        _tcscpy_s(szURL, TEST_MAX_PATH, m_pMedia->GetUrl());
        FilterDesc* pFilterDesc = new FilterDesc(pFilter, szURL, &hr);
        // if the ctor returns S_FALSE, that should be valid for unknown or third party
        // filters.
        if (!pFilterDesc || FAILED(hr))
        {
            if (!pFilterDesc)
                hr = E_OUTOFMEMORY;
            else delete pFilterDesc;
            break;
        }
        
        // Add the filter desc object to our list
        m_FilterList.push_back(pFilterDesc);
        
        // Release the ref on the filter
        pFilter->Release();

        // Enumerate the next filter
        hr = pEnumFilters->Next(1, &pFilter, &nFetched);
    }

    if (FAILED(hr))
    {
        ReleaseFilterList();
    }

    pEnumFilters->Release();
    
    return hr;
}

HRESULT TestGraph::IdentifyFilters()
{
    return E_NOTIMPL;
}

HRESULT TestGraph::AcquireInterfaces()
{
    HRESULT hr = S_OK;

    if ( !m_pGraphBuilder)
        return E_FAIL;

    if (!m_pMediaControl)
    {
        // Get all common interfaces - if an interface is mandatory and not obtained that will be an error
        hr = m_pGraphBuilder->QueryInterface(IID_IMediaControl, (void**)&m_pMediaControl);
        if (FAILED(hr) || (hr == S_FALSE) || !m_pMediaControl)
        {
            return hr;
        }
    }

    if (!m_pMediaSeeking)
    {
        // Get all common interfaces - if an interface is mandatory and not obtained that will be an error
        hr = m_pGraphBuilder->QueryInterface(IID_IMediaSeeking, (void**)&m_pMediaSeeking);
        if (FAILED(hr) || (hr == S_FALSE) || !m_pMediaSeeking)
        {
            return hr;
        }
    }

    if ( !m_pMediaPosition )
    {       
        hr = m_pGraphBuilder->QueryInterface(IID_IMediaPosition, (void**)&m_pMediaPosition);
        if ( FAILED(hr) || (hr == S_FALSE) || !m_pMediaPosition )
        {
            // The remote graph builder may not expose this interface
            if(E_NOINTERFACE == hr && m_bRemoteGB)
                hr = S_OK;
            else
                return hr;
        }
    }

    if (!m_pMediaEvent)
    {
        // Get all common interfaces - if an interface is mandatory and not obtained that will be an error
        hr = m_pGraphBuilder->QueryInterface(IID_IMediaEvent, (void**)&m_pMediaEvent);
        if (FAILED(hr) || (hr == S_FALSE) || !m_pMediaEvent)
        {
            return hr;
        }
    }

    if ( !m_pMediaEventEx )
    {
        // Get all common interfaces - if an interface is mandatory and not obtained that will be an error
        hr = m_pGraphBuilder->QueryInterface(IID_IMediaEventEx, (void**)&m_pMediaEventEx);
        if (FAILED(hr) || (hr == S_FALSE) || !m_pMediaEventEx)
        {
            return hr;
        }
    }


    if(!m_pVMRWindowlessControl)
    {
        // BUGBUG: No error if we don't get this
        FindInterfaceOnGraph(IID_IVMRWindowlessControl, (void**)&m_pVMRWindowlessControl, NULL);   
    }

    if (!m_pBasicVideo)
    {
        // BUGBUG: No error if we don't get this
        m_pGraphBuilder->QueryInterface(IID_IBasicVideo, (void**)&m_pBasicVideo);
    }

    if (!m_pVideoWindow)
    {
        // BUGBUG: No error if we don't get this
        m_pGraphBuilder->QueryInterface(IID_IVideoWindow, (void**)&m_pVideoWindow);
    }

    if (!m_pBasicAudio)
    {
        // BUGBUG: No error if we don't get this 
        m_pGraphBuilder->QueryInterface(IID_IBasicAudio, (void**)&m_pBasicAudio);
    }

    return hr;
}

void TestGraph::ReleaseInterfaces()
{
    if (m_pGraphBuilder)
    {
        m_pGraphBuilder->Release();
        m_pGraphBuilder = NULL;
    }

    if (m_pMediaControl)
    {
        m_pMediaControl->Release();
        m_pMediaControl = NULL;
    }

    if (m_pMediaSeeking)
    {
        m_pMediaSeeking->Release();
        m_pMediaSeeking = NULL;
    }

    if ( m_pMediaPosition )
    {
        m_pMediaPosition->Release();
        m_pMediaPosition = NULL;
    }

    if (m_pMediaEvent)
    {
        m_pMediaEvent->Release();
        m_pMediaEvent = NULL;
    }

    if ( m_pMediaEventEx )
    {
        m_pMediaEventEx->Release();
        m_pMediaEventEx = NULL;
    }

    if (m_pVideoWindow)
    {
        m_pVideoWindow->Release();
        m_pVideoWindow = NULL;
    }

    if(m_pVMRWindowlessControl)
    {
        m_pVMRWindowlessControl->Release();
        m_pVMRWindowlessControl = NULL;
    }

    if (m_pBasicVideo)
    {
        m_pBasicVideo->Release();
        m_pBasicVideo = NULL;
    }

    if (m_pBasicAudio)
    {
        m_pBasicAudio->Release();
        m_pBasicAudio = NULL;
    }

#ifdef UNDER_CE
    if (m_pVRMode)
    {
        m_pVRMode->Release();
        m_pVRMode = NULL;
    }
#endif

    if (m_pSourceFilter)
    {
        m_pSourceFilter->Release();
        m_pSourceFilter = NULL;
    }

}

HRESULT TestGraph::RenderUnconnectedPins()
{
    HRESULT hr = S_OK;

    // We need atleast the source filter
    if (!IsAddedSourceFilter())
        return E_FAIL;

    // Find the first unconnected pin starting from the source filter
    IPin* pPin = NULL;
    IEnumPins* pEnumPins = NULL;
    hr = m_pSourceFilter->EnumPins(&pEnumPins);
    if (FAILED(hr) || (hr == S_FALSE))
        return hr;

    bool bNoMorePins = false;
    bool bAtleastOnePinRendered = false;

    // We'll try to Render all pins audio or video    
    while(true)
    {
        hr = pEnumPins->Next(1, &pPin, 0);
        if (hr==S_FALSE)
        {
            // There were no more pins
            bNoMorePins = true;
            break;
        }
        else if (FAILED(hr)) {
            break;
        }
        else {
            // Render the pin found
            hr = m_pGraphBuilder->Render(pPin);
            
            if (SUCCEEDED(hr) && (hr != S_FALSE))
                bAtleastOnePinRendered = true;

            // Release reference on the pin found
            pPin->Release();
        }
    }

    pEnumPins->Release();

    // Return success if we have no more pins to render but we rendered atleast one pin
    return (bNoMorePins && bAtleastOnePinRendered) ? S_OK : hr;
}

int GetNumFilterInstances(FilterDescList &filterList, TCHAR* wszFilterName)
{
    // BUGBUG: Need to implement this function
    int fCnt = 0;
    FilterDescList::iterator iterator = filterList.begin();

    while(iterator != filterList.end())
    {
        FilterDesc* pFilterDesc = *iterator;
        if (!_tcscmp(pFilterDesc->GetName(), wszFilterName))
            fCnt++;

        iterator++;
    }

    return fCnt;
}

HRESULT TestGraph::AddFilter(FilterId id, TCHAR* szFilterName, int length)
{
    HRESULT hr = S_OK;

    if (!m_pGraphBuilder)
        return E_FAIL;

#ifdef UNDER_CE
    if ( m_bRemoteGB )
    {
        FilterInfo  tmpFilterInfo;
        hr = FilterInfoMapper::GetFilterInfo( id, &tmpFilterInfo );
        if ( FAILED(hr) )
            return hr;

        WCHAR wszFilterName[TEST_MAX_PATH] = {0};
        hr = ((IGraphBuilder2*)m_pGraphBuilder)->AddFilterByCLSID( tmpFilterInfo.guid, wszFilterName );
        if ( FAILED(hr) )
            return hr;
    }
    else
    {
#endif

    TCHAR szTmpFilterName[MAX_FILTER_NAME];
    ZeroMemory( szTmpFilterName, MAX_FILTER_NAME );


    if (szFilterName && (length <= 0))
        return E_FAIL;

    FilterDesc* pFilterDesc = NULL;
    
    // Create an instance of the filter
    if (SUCCEEDED(hr) && (hr != S_FALSE)) 
    {
        // Create the object representing the filter to be added
        pFilterDesc = new FilterDesc(id);
        if (!pFilterDesc)
            return E_OUTOFMEMORY;

        // BUGBUG: the following #ifdef code is when there are multiple instances of the same filter 
        // and we want to assign differentiating names to them
#if 0
        // Find out the number of filters with the same name
        int fcount = GetNumFilterInstances(m_FilterList, szFilterName);

        // Append the count to the filter name
        if (fcount)
        {
            StringCchPrintf( szTmpFilterName, MAX_FILTER_NAME, TEXT("%s:%d"), szFilterName, fcount ); 
            // Set the name of the filter
            pFilterDesc->SetName(szTmpFilterName);
        }
#endif        
        // Create the filter instance
        hr = pFilterDesc->CreateFilterInstance();
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR - Failed to create filter instance %s"), __FUNCTION__, pFilterDesc->GetName());
        }
    }

    // Add the filter instance to the graph
    if (SUCCEEDED_F(hr))
    {
        hr = pFilterDesc->AddToGraph(m_pGraphBuilder);
        if (FAILED(hr))
        {
            LOG(TEXT("Failed to add filter %s to the graph"), pFilterDesc->GetName());
        }
    }

    // Add the filter instance to the maintained list of filters
    if (SUCCEEDED(hr) && (hr != S_FALSE))
    {
        m_FilterList.push_back(pFilterDesc);
        
        DeterminePrimaryFilters();
    }

    // Copy the new name to the name to be returned
    if (szFilterName && SUCCEEDED_F(hr))
    {
        _tcscpy_s(szFilterName, length, pFilterDesc->GetName() );
    }

    // Release resources
    if (FAILED_F(hr))
    {
        // If a filter object was allocated
        if (pFilterDesc)
            delete pFilterDesc;
    }
#ifdef UNDER_CE
    }
#endif

    return hr;
}

HRESULT TestGraph::AddFilterByCLSID( CLSID rclsid, wchar_t *pwszFilterName )
{
    CheckPointer( pwszFilterName, E_POINTER );
    HRESULT hr = S_OK;

    if ( !m_pGraphBuilder )
        return E_FAIL;

#ifdef UNDER_CE
    hr = ( (IGraphBuilder2 *)m_pGraphBuilder )->AddFilterByCLSID( rclsid, pwszFilterName );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to add the filter %S. (0x%x)" ), 
                        __FUNCTION__, __LINE__, __FILE__, pwszFilterName, hr );
        goto cleanup;
    }
#else
    hr = E_NOTIMPL;
#endif

cleanup:
    return hr;
}

bool TestGraph::IsBuilt()
{
    return m_bGraphBuilt;
}

HRESULT TestGraph::SetBuilt( bool built )
{
    HRESULT hr = S_OK;
    errno_t error;
    // Create the list of filters in the graph
    if(SUCCEEDED(hr = DetermineFilterList()))
        // Determine the renderers, decoders etc
        if(SUCCEEDED(hr = DeterminePrimaryFilters()))
            // Determine the streams
            hr = DetermineStreams();

    // Instantiate any verifiers that need to be created & added to the graph.
    if(FAILED(hr = m_pVerifMgr->AddQueuedVerifiers(this)))
        goto cleanup;

    if ( built )
    {
        // Look to see if we are just saving this graph as a GRF file.  
        if(m_bSaveGraph)
        {
            TCHAR szFileName[TEST_MAX_PATH];
            TCHAR szFileID[32];

            szFileName[0] = _T('\0');            
            StringCchCopy(szFileName, TEST_MAX_PATH, _T("\\Temp\\"));
            error = _itot_s(m_dwGraphID, szFileID,_tcslen(szFileID), 10);
            if(error != 0)
            { 
               LOG(TEXT("ERROR:TestGraph::SetBuilt -_itot_s"));
               return E_FAIL;
            }
            StringCchCat(szFileName, TEST_MAX_PATH,szFileID);
            StringCchCat(szFileName, TEST_MAX_PATH, _T("-"));
            error =_itot_s(m_dwGraphSubID, szFileID,_countof(szFileID), 10);
            if(error != 0)
            { 
               LOG(TEXT("ERROR:TestGraph::SetBuilt -_itot_s"));
               return E_FAIL;
            }
            StringCchCat(szFileName, TEST_MAX_PATH,szFileID);
            StringCchCat(szFileName, TEST_MAX_PATH, _T(".GRF"));

            m_dwGraphSubID++;  // Increment this in case this test uses multiple filter graphs / VMR streams

            SaveGraphFile(m_pGraphBuilder, szFileName);

            // We don't want the test to execute in this case (since we just care about saving the graph).
            // Therefore, short-circuit to a failure value
            hr = E_FAIL;
            goto cleanup;
        }
        
        // Set as built
        m_bGraphBuilt = built;   

        // Get the duration and set the stop pos as already being set
        GetDuration(&m_StopPosSet);
        m_StartPosSet = 0;
        m_RateSet = 1.0;
    }
    else
        m_bGraphBuilt = false;

cleanup:

    return hr;
}

HRESULT TestGraph::SaveGraphFile(IGraphBuilder *pGraph, WCHAR *wszPath) 
{
    const WCHAR wszStreamName[] = L"ActiveMovieGraph"; 
    HRESULT hr;
    
    IStorage *pStorage = NULL;
    hr = StgCreateDocfile(
        wszPath,
        STGM_CREATE | STGM_TRANSACTED | STGM_READWRITE | STGM_SHARE_EXCLUSIVE,
        0, &pStorage);
    if(FAILED(hr)) 
    {
        return hr;
    }

    IStream *pStream;
    hr = pStorage->CreateStream(
        wszStreamName,
        STGM_WRITE | STGM_CREATE | STGM_SHARE_EXCLUSIVE,
        0, 0, &pStream);
    if (FAILED(hr)) 
    {
        pStorage->Release();    
        return hr;
    }

    IPersistStream *pPersist = NULL;
    pGraph->QueryInterface(IID_IPersistStream, reinterpret_cast<void**>(&pPersist));
    hr = pPersist->Save(pStream, TRUE);
    pStream->Release();
    pPersist->Release();
    if (SUCCEEDED(hr)) 
    {
        hr = pStorage->Commit(STGC_DEFAULT);
    }
    pStorage->Release();
    return hr;
}



FilterDesc* TestGraph::GetFilter(const TCHAR* szFromFilter)
{
    FilterDescList::iterator iterator = m_FilterList.begin();

    while(iterator != m_FilterList.end())
    {
        FilterDesc* pFilterDesc = *iterator;
        if (!_tcscmp(pFilterDesc->GetName(), szFromFilter))
            return pFilterDesc;
        iterator++;
    }

    return NULL;
}

IBaseFilter* TestGraph::GetFilter(FilterId id)
{
    FilterDescList::iterator iterator = m_FilterList.begin();

    while(iterator != m_FilterList.end())
    {
        FilterDesc* pFilterDesc = *iterator;
        if (pFilterDesc->GetId() == id)
            return pFilterDesc->GetFilterInstance();
        iterator++;
    }

    return NULL;
}

FilterDescList* TestGraph::GetFilterDescList()
{
    return &m_FilterList;
}

HRESULT TestGraph::LoadURL(FilterDesc* pFromFilter)
{
    HRESULT hr = S_OK;
    IFileSourceFilter* pFileFilter;

    if(IsSourceFilter(pFromFilter->GetType())) 
    {
        // AddRef'd here so make sure we release
        IBaseFilter* pFilter = pFromFilter->GetFilterInstance();
        
        //load the file we want
        hr = pFilter->QueryInterface(IID_IFileSourceFilter, (void**)&pFileFilter);
        if (SUCCEEDED(hr)) {
            //Load a URL, without this the filter will not expose a PIN. 
            hr = pFileFilter->Load(m_pMedia->GetUrl(), NULL);                
            if(FAILED(hr)) {
                LOG(TEXT("%S: ERROR %d@%S - Failed while trying to Load URL %s in source filter %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, 
                    pFromFilter->GetName(), m_pMedia->GetUrl(), hr);    
            }
            pFileFilter->Release();
        }    
        pFilter->Release();    
    }
    return hr;
}

HRESULT TestGraph::Connect(const TCHAR* szFromFilter, int iFromPin, const TCHAR* szToFilter, int iToPin, AM_MEDIA_TYPE * pMediaType)
{
    HRESULT hr = S_OK;

    if ( m_bRemoteGB )
        return E_NOTIMPL;

    if (!m_pGraphBuilder)
        return E_FAIL;

    // From the filter list find the filters and pins 
    // (assume that the filter name has the instance appended)
    FilterDesc* pFromFilter = GetFilter(szFromFilter);
    if (!pFromFilter)
        return E_FAIL;

    // Get the filter desc object from the filter name
    FilterDesc* pToFilter = GetFilter(szToFilter);
    if (!pToFilter)
        return E_FAIL;

    //before getting pins make sure we load a URL into the filter. Source filters will not expose a pin until you tell them what url to stream
    LoadURL(pFromFilter);

    // Get the pins
    IPin* pFromPin = NULL;
    IPin* pToPin = NULL;

    pFromPin = pFromFilter->GetPin(iFromPin, PINDIR_OUTPUT);
    if (!pFromPin)
    {
        // Unable to find output pin
        return E_FAIL;
    }

    pToPin = pToFilter->GetPin(iToPin, PINDIR_INPUT);
    if (!pToPin)
    {
        // Unable to find input pin
        pFromPin->Release();
        return E_FAIL;
    }

    // Now connect the pins intelligently
    hr = m_pGraphBuilder->Connect(pFromPin, pToPin);
    
    // Decrease reference count
    if ( pFromPin )
        pFromPin->Release();
    if ( pToPin )
        pToPin->Release();

    return hr;
}

HRESULT TestGraph::ConnectDirect(const TCHAR* szFromFilter, const TCHAR* szToFilter, int iFromPin, int iToPin, AM_MEDIA_TYPE * pMediaType)
{
    HRESULT hr = S_OK;
    
    if ( m_bRemoteGB )
        return E_NOTIMPL;

    if (!m_pGraphBuilder)
        return E_FAIL;

    // From the filter list find the filters and pins 
    // (assume that the filter name has the instance appended)
    FilterDesc* pFromFilter = GetFilter(szFromFilter);
    if (!pFromFilter)
        return E_FAIL;

    FilterDesc* pToFilter = GetFilter(szToFilter);
    if (!pToFilter)
        return E_FAIL;

    //before getting pins make sure we load a URL into the filter. Source filters will not expose a pin until you tell them what url to stream
    LoadURL(pFromFilter);

    // Get the pins
    IPin* pFromPin = NULL;
    IPin* pToPin = NULL;

    pFromPin = pFromFilter->GetPin(iFromPin, PINDIR_OUTPUT);
    if (!pFromPin)
    {
        // Unable to find output pin
        return E_FAIL;
    }

    pToPin = pToFilter->GetPin(iToPin, PINDIR_INPUT);
    if (!pToPin)
    {
        // Unable to find input pin
        pFromPin->Release();
        return E_FAIL;
    }

    // Now connect the pins directly
    hr = m_pGraphBuilder->ConnectDirect(pFromPin, pToPin, pMediaType);
    
    // Decrease reference count
    if ( pFromPin )
        pFromPin->Release();
    if ( pToPin )
        pToPin->Release();

    return hr;
}

HRESULT TestGraph::DisconnectFilter(const TCHAR* szFilter)
{
    CheckPointer( szFilter, E_POINTER );
    HRESULT hr = S_OK;

    if ( m_bRemoteGB )
        return E_NOTIMPL;

    if ( !m_pGraphBuilder ) 
        return E_FAIL;

    IBaseFilter *pFilter = NULL;
    IEnumPins* pEnumPins = NULL;
    IPin *pTmpPin = NULL;
    WCHAR wszFilter[TEST_MAX_PATH] = {0};
    TCharToWChar( szFilter, wszFilter, TEST_MAX_PATH );

    hr = m_pGraphBuilder->FindFilterByName( wszFilter, &pFilter );
    if ( FAILED(hr) )
    {
        LOG(  TEXT("%S: ERROR %d@%S - Can't find the filter: %s. (0x%08x)"), 
                __FUNCTION__, __LINE__, __FILE__, szFilter, hr );
        goto error;
    }

    hr = pFilter->EnumPins( &pEnumPins );
    if ( FAILED(hr) )
    {
        LOG(  TEXT("%S: ERROR %d@%S - EnumPins failed. (0x%08x)"), 
                __FUNCTION__, __LINE__, __FILE__, hr );
        goto error;
    }    

    hr = pEnumPins->Reset();
    if ( FAILED(hr) ) 
    {
        LOG( TEXT("%S: ERROR %d@%S - EnumPins::Reset failed.(0x%08x)"), 
                __FUNCTION__, __LINE__, __FILE__, hr );
        goto error;
    }

    while ( (hr = pEnumPins->Next(1, &pTmpPin, 0)) == S_OK ) {
        hr = m_pGraphBuilder->Disconnect( pTmpPin );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Disconnecting the pin failed.(0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
            goto error;
        }
        pTmpPin->Release();
        pTmpPin = NULL;
    }

error:
    if (pEnumPins)
        pEnumPins->Release();

    if ( pTmpPin )
        pTmpPin->Release();

    if(pFilter)
        pFilter->Release();

    return hr;
}

static void CheckFilterStates(const FilterDescList& filterList, FILTER_STATE toState)
{
    for (FilterDescList::const_iterator iterator = filterList.begin(), end = filterList.end();
         iterator != end; iterator++)
    {
        FilterDesc* pFilterDesc = *iterator;
        FILTER_STATE state;
        IMediaFilter* pFilter = (IMediaFilter*)pFilterDesc->GetFilterInstance();
        if (pFilter)
        {
            HRESULT hr = pFilter->GetState(0xFFFFFFFF, &state);
            if (SUCCEEDED(hr))
            {
                if (state != toState)
                {
                    LOG(L"Filter: %s yet to transition to state: %s.  Current state: %s", pFilterDesc->GetName(),
                        GetStateName(toState), GetStateName(state));
                }
            }
            else
            {
                LOG(L"CheckFilterStates: GetState failed for filter: %s, hr: 0x%08x", pFilterDesc->GetName(), hr);
            }
        }
    }
}

HRESULT TestGraph::SetState(FILTER_STATE toState, DWORD flags, DWORD *pTime)
{
    HRESULT hr = S_OK;
    
    if (!m_pMediaControl)
    {
        AcquireInterfaces();
        if (!m_pMediaControl)
            return E_FAIL;
    }

    // We have to know what state we are in. If we are transitioning then we can't really set the new state?
    switch(toState)
    {
    case State_Stopped:
        hr = m_pMediaControl->Stop();
        break;
    case State_Paused:
        hr = m_pMediaControl->Pause();
        break;
    case State_Running:
        hr = m_pMediaControl->Run();
        break;
    }

    if (SUCCEEDED(hr) || (hr == VFW_S_CANT_CUE) || (hr == VFW_S_STATE_INTERMEDIATE))
    {
        if (flags & TestGraph::SYNCHRONOUS)
        {
            FILTER_STATE state;
            Timer timer;
            timer.Start();
            
            while (timer.Elapsed() < MSEC_TO_TICKS(STATE_CHANGE_TIMEOUT_MS) ) {
                hr = m_pMediaControl->GetState(100, (OAFilterState*)&state);
           
                if (((state == toState) && (hr == S_OK)) || 
                    ((state == State_Paused) && (hr == VFW_S_CANT_CUE)))
                {
                    return S_OK;
                }
                else if (hr != VFW_S_STATE_INTERMEDIATE)
                {
                    LOG(TEXT("%S: ERROR %d@%S - failed to change state to %s (0x%x) \n"), __FUNCTION__, __LINE__, __FILE__, GetStateName(toState), hr);
                    return E_FAIL;
                }
            }
        }
    }
    if (hr == VFW_S_STATE_INTERMEDIATE)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to change state to %s (0x%x), it takes too much time to be in intermediate state \n"),__FUNCTION__, __LINE__, __FILE__, GetStateName(toState), hr);
        return E_FAIL;
    }

    return hr;
}

HRESULT TestGraph::StopWhenReady()
{
    HRESULT hr = S_OK;
    
    if (!m_pMediaControl)
    {
        AcquireInterfaces();
        if (!m_pMediaControl)
            return E_FAIL;
    }

    hr = m_pMediaControl->StopWhenReady();

    return hr;
}

HRESULT TestGraph::WaitForCompletion(DWORD timeout, LONG *eventCode )
{
    HRESULT hr = S_OK;
    if (!m_pMediaEvent)
    {
        AcquireInterfaces();
        if (!m_pMediaEvent)
            return E_FAIL;
    }

    long evCode;
    hr = m_pMediaEvent->WaitForCompletion(timeout, &evCode);
    if (SUCCEEDED(hr))
    {
        if (evCode != EC_COMPLETE )
        {   
            LOG(TEXT("%S: ERROR %d@%S - WaitForCompletion return succeed with evCode %d \n"), __FUNCTION__, __LINE__, __FILE__, evCode);
            hr = E_FAIL;
        }
        // return the event code.
        if ( eventCode )
            *eventCode = evCode;
    }else
        LOG(TEXT("%S: ERROR %d@%S - m_pMediaEvent->WaitForCompletion failed (0x%x) \n"), __FUNCTION__, __LINE__, __FILE__,  hr);

    return hr;
}

HRESULT TestGraph::WaitForEvent(long event, DWORD timeout)
{
    HRESULT hr = S_OK;
    if (!m_pMediaEvent)
    {
        AcquireInterfaces();
        if (!m_pMediaEvent)
            return E_FAIL;
    }

    while (true)
    {
        long evCode;
        long param1;
        long param2;
        
        // Get the next event
        hr = m_pMediaEvent->GetEvent(&evCode, &param1, &param2, timeout);
        if (SUCCEEDED(hr))
        {
            // Free the event resources
            m_pMediaEvent->FreeEventParams(evCode, param1, param2);

            // If we found the event we were waiting for, break out
            if (evCode == event)
                break;
        }
        else if (FAILED(hr))
        {
            // If there was a timeout return S_FALSE
            if (hr == E_ABORT)
            {
                hr = S_FALSE;
                LOG(TEXT("%S: ERROR %d@%S - WaitForEvent failed due to timeout(0x%x) \n"), __FUNCTION__, __LINE__, __FILE__,  hr);
                break;
            }
        }
    }

    return hr;
}

HRESULT TestGraph::GetEventHandle(HANDLE* pEvent)
{
    if (!pEvent)
        return E_INVALIDARG;

    if (!m_pMediaEvent)
        return E_FAIL;

    return m_pMediaEvent->GetEventHandle((OAEVENT*)pEvent);
}

FILTER_STATE TestGraph::GetRandomState()
{
    UINT i=0;
    rand_s(&i);
    switch(i%3)
    {
    case State_Stopped: return State_Stopped;
    case State_Paused: return State_Paused;
    case State_Running: return State_Running;
    }
    return State_Running;
}

HRESULT TestGraph::SetAbsolutePositions(LONGLONG start, LONGLONG stop)
{
    HRESULT hr = S_OK;
    if (!m_pMediaSeeking)
    {
        AcquireInterfaces();
        if (!m_pMediaSeeking)
            return E_FAIL;
    }

    hr = m_pMediaSeeking->SetPositions(&start, AM_SEEKING_AbsolutePositioning, &stop, AM_SEEKING_AbsolutePositioning);
    if (SUCCEEDED(hr))
    {
        m_StartPosSet = start;
        m_StopPosSet = stop;
    }

    return hr;
}

HRESULT TestGraph::SetRate(double rate)
{
    HRESULT hr = S_OK;
    if (!m_pMediaSeeking)
    {
        AcquireInterfaces();
        if (!m_pMediaSeeking)
            return E_FAIL;
    }
    hr = m_pMediaSeeking->SetRate(rate);
    if (SUCCEEDED(hr))
    {
        m_RateSet = rate;
    }

    return hr;
}

HRESULT TestGraph::GetRate(double* pRate)
{
    HRESULT hr = S_OK;
    
    if (!pRate)
        return E_INVALIDARG;

    if (!m_pMediaSeeking)
    {
        AcquireInterfaces();
        if (!m_pMediaSeeking)
            return E_FAIL;
    }
    hr = m_pMediaSeeking->GetRate(pRate);

    return hr;
}

HRESULT TestGraph::GetRateSet(double* pRate)
{
    if (!pRate)
        return E_FAIL;
    *pRate = m_RateSet;
    return S_OK;
}

HRESULT TestGraph::SetPosition( LONGLONG posToSet, bool bStartPos )
{
    HRESULT hr = S_OK;

    if (!m_pMediaSeeking)
    {
        AcquireInterfaces();
        if (!m_pMediaSeeking)
            return E_FAIL;
    }

    if ( bStartPos )
    {
        hr = m_pMediaSeeking->SetPositions(&posToSet, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
        if (SUCCEEDED(hr))
            m_StartPosSet = posToSet;
    }
    else    // set the stop position only.
    {
        hr = m_pMediaSeeking->SetPositions(NULL, AM_SEEKING_NoPositioning, &posToSet, AM_SEEKING_AbsolutePositioning );
        if ( SUCCEEDED(hr) )
            m_StopPosSet = posToSet;
    }

    return hr;
}

HRESULT TestGraph::GetCurrentPosition(LONGLONG* pCurrent)
{
    HRESULT hr = S_OK;
    
    if (!pCurrent)
        return E_INVALIDARG;

    if (!m_pMediaSeeking)
    {
        AcquireInterfaces();
        if (!m_pMediaSeeking)
            return E_FAIL;
    }

    hr = m_pMediaSeeking->GetCurrentPosition(pCurrent);
    return hr;
}

void TestGraph::GetPositionSet(LONGLONG* pStart, LONGLONG* pStop)
{
    if (pStart)
        *pStart = m_StartPosSet;

    if (pStop)
        *pStop = m_StopPosSet;
}

HRESULT TestGraph::GetPositions(LONGLONG* pCurrent, LONGLONG *pStop)
{
    HRESULT hr = S_OK;
    if (!m_pMediaSeeking)
    {
        AcquireInterfaces();
        if (!m_pMediaSeeking)
            return E_FAIL;
    }

    LONGLONG current, stop;
    hr = m_pMediaSeeking->GetPositions(&current, &stop);

    if (SUCCEEDED_F(hr))
    {
        if (pCurrent)
            *pCurrent = current;
        if (pStop)
            *pStop = stop;
    }

    return hr;
}

HRESULT TestGraph::GetDuration(LONGLONG* pDuration)
{
    HRESULT hr = S_OK;
    
    if (!pDuration)
        return E_INVALIDARG;

    if (!m_pMediaSeeking)
    {
        AcquireInterfaces();
        if (!m_pMediaSeeking)
            return E_FAIL;
    }

    hr = m_pMediaSeeking->GetDuration(pDuration);
    return hr;
}

HRESULT TestGraph::DeterminePrimaryFilters()
{
    HRESULT hr = S_OK;

    if ( m_bRemoteGB )
        return S_FALSE;

    // Look for the source filter
    if (!m_pSourceFilter)
    {
        FilterDesc* pSourceDesc = GetFilterDesc(SourceFilter, 0);
        if (pSourceDesc)
            m_pSourceFilter = pSourceDesc->GetFilterInstance();
    }

    // Look for the video decoder
    if (!m_pVideoDecoder)
    {
        FilterDesc* pVDDesc = GetFilterDesc(Decoder, VideoDecoder);
        if (pVDDesc)
            m_pVideoDecoder = pVDDesc->GetFilterInstance();
    }

    // Look for the audio decoder
    if (!m_pAudioDecoder)
    {
        FilterDesc* pADDesc = GetFilterDesc(Decoder, AudioDecoder);
        if (pADDesc)
            m_pAudioDecoder = pADDesc->GetFilterInstance();
    }


    // Look for the video mixing renderer
    if (!m_pVideoRenderer)
    {
        FilterDesc* pVRDesc = GetFilterDesc(Renderer, VideoMixingRenderer);
        if (pVRDesc)
            m_pVideoRenderer = pVRDesc->GetFilterInstance();
    }

    // Look for the audio renderer
    if (!m_pAudioRenderer)
    {
        FilterDesc* pARDesc = GetFilterDesc(Renderer, AudioRenderer);
        if (pARDesc)
            m_pAudioRenderer = pARDesc->GetFilterInstance();
    }

    return hr;
}


HRESULT TestGraph::AcquireVRModeIface()
{
#ifdef UNDER_CE
    HRESULT hr = S_OK;

    // Get the interface for setting renderer mode
    if (m_pVRMode)
        return S_OK;

    if (!m_pVideoRenderer)
        return E_FAIL;

    hr = m_pVideoRenderer->QueryInterface(IID_IAMVideoRendererMode, (void**)&m_pVRMode);
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to QueryInterface for IAMVideoRendererMode (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
    }

    return hr;
#else
    return E_FAIL;
#endif
}

HRESULT TestGraph::SetVideoRendererMode(VideoRendererMode vrMode)
{
#ifdef UNDER_CE
    HRESULT hr = S_OK;

    hr = AcquireVRModeIface();
    if (FAILED(hr))
        return hr;
    
    if (vrMode & VIDEO_RENDERER_MODE_GDI)
    {
        // Set GDI mode
        hr = m_pVRMode->SetMode(AM_VIDEO_RENDERER_MODE_GDI);
    }
    else if (vrMode & VIDEO_RENDERER_MODE_DDRAW)
    {
        // Set DDraw mode
        hr = m_pVRMode->SetMode(AM_VIDEO_RENDERER_MODE_DDRAW);
    }

    return hr;
#else
    return E_FAIL;
#endif
}

HRESULT TestGraph::GetVideoRendererMode(VideoRendererMode* pVRMode)
{
    if (!pVRMode)
        return E_INVALIDARG;

#ifdef UNDER_CE
    HRESULT hr = S_OK;

    hr = AcquireVRModeIface();
    if (FAILED(hr))
        return hr;
    
    DWORD dwMode = 0;
    hr = m_pVRMode->GetMode(&dwMode);
    if (FAILED(hr))
        return hr;

    if (dwMode == AM_VIDEO_RENDERER_MODE_GDI)
    {
        // Set GDI mode
        *pVRMode = VIDEO_RENDERER_MODE_GDI;
    }
    else if (dwMode == AM_VIDEO_RENDERER_MODE_DDRAW)
    {
        // Set DDraw mode
        *pVRMode = VIDEO_RENDERER_MODE_DDRAW;
    }
    else
    {
        // Set as 0
        *pVRMode = (VideoRendererMode)0;
    }
    
    return hr;
#else
    return E_FAIL;
#endif
}

HRESULT TestGraph::SetMaxBackBuffers(DWORD dwMaxBackBuffers)
{
    return ::SetMaxBackBuffers(dwMaxBackBuffers);
}

HRESULT TestGraph::SetVRPrimaryFlipping(bool bAllowPrimaryFlipping)
{
    return ::SetVRPrimaryFlipping(bAllowPrimaryFlipping);
}

void TestGraph::SetGraphID(DWORD dwGraphID) 
{ 
    m_dwGraphID = dwGraphID;
    m_dwGraphSubID = 0;
}

// The caller needs to set the remote verification server if needed
// The test graph will create a RemoteVerifier object to help communicate with the server and set the verification server url
// Verification works by having the caller specify the type of verification/measurement needed and the verification structure (containing data such as tolerance etc) cast to void*
// The test graph creates the appropriate verifier/measurer and maintains a list of such verifiers
// The verifier is responsible for adding the callbacks / tap-filters needed using TestGrpah::InsertTapFilter
// The verifier registers a call-back with the tap-filters directly
// So each tap filter might have callbacks to several verifiers
// The verifier is responsible for monitoring/receiving callbacks and doing it's operations
// When the caller calls GetVerificationResult, the test graph calls the appropriate verifier and retrieves a pointer to the verification result
// There is casting involved with the verification data (Could we do better?)

// Is verification possible?
bool TestGraph::IsCapable(VerificationType type)
{
    return ::IsVerifiable(type);
}

HRESULT 
TestGraph::AddVerifier(VerificationType type, CGraphVerifier* pVerifier)
{
    // Add to our list of verifiers
    return m_pVerifMgr->AddVerifier( type, pVerifier );
}

HRESULT 
TestGraph::EnableVerification( VerificationType type, 
                               void *pVerificationData,
                               CGraphVerifier** ppVerifier )
{
    HRESULT hr = S_OK;
    DWORD dwLength = 0;
    VerificationInitInfo* pInitInfo = NULL;
    TimeStampInitData* pTimeStampInitData = NULL;
    GraphBuildInitData* pGraphBuildInitData = NULL;
    WCHAR* pszOutName = NULL;
    size_t pcch = 0;
    DWORD dwMsgFlags = 0;
    StringList filterNameList;
    StringList::iterator itList;


    // Serialize the verification data and package it in case we are sending this data across a message queue.
    // Add in additional configuration data for selected verifiers (most of this is related to the source media)
    if((pInitInfo = (VerificationInitInfo*) new BYTE[MAX_VERIFICATION_MESSAGE_SIZE]) == 0)
    {
        hr = E_OUTOFMEMORY;
        goto cleanup;
    }
    pInitInfo->type = type;
    pInitInfo->pData = (BYTE*)pInitInfo + sizeof(VerificationInitInfo);
    pInitInfo->cbDataSize = 0;

    switch(type)
    {
        case VerifyBackBuffers:
        case NumDecodedTrickModeVideoFrames:
        case NumRenderedTrickModeVideoFrames:
        case NumDecodedVideoFrames:
        case NumRenderedVideoFrames:
        case NumDecoderDroppedVideoFrames:
        case NumRendererDroppedVideoFrames:
        case NumDroppedVideoFrames:
        case VerifyCurrentPosition:
        case VerifyStoppedPosition:
        case VerifyAdvisePastTimeLatency:
        case VerifyAdviseTimeLatency:
        case StartupLatency:
        case DecodedVideoLatencyPauseToFirstSample:
        case DecodedVideoLatencyRunToFirstSample:        
        case NumRenderedAudioDiscontinuities:
        case DecodedAudioDataTime:
            if(pVerificationData)
            {
                pInitInfo->cbDataSize = sizeof(DWORD);
                memcpy(pInitInfo->pData, pVerificationData, pInitInfo->cbDataSize);
            }
            break;

        case VerifySampleDelivered:
            if(pVerificationData)
            {
                pInitInfo->cbDataSize = sizeof(SampleDeliveryVerifierData);
                memcpy(pInitInfo->pData, pVerificationData, pInitInfo->cbDataSize);
            }
            break;

        case VerifyVideoSourceRect:
        case VerifyVideoDestRect:
            if(pVerificationData)
            {
                pInitInfo->cbDataSize = sizeof(RECT);
                memcpy(pInitInfo->pData, pVerificationData, pInitInfo->cbDataSize);
            }
            break;

        case DecodedVideoSeekAccuracy:
        case DecodedVideoTrickModeFirstFrameTimeStamp:
        case DecodedVideoTrickModeEndFrameTimeStamp:

            pInitInfo->cbDataSize = sizeof(TimeStampInitData);
            pTimeStampInitData = (TimeStampInitData*)(pInitInfo->pData);
            pTimeStampInitData->dwTolerance = 0;        // default tolerance value if pVerificationData is NULL

            if(m_pMedia)
            {
                if(FAILED(hr = StringCchCopy(pTimeStampInitData->pszIndexFile, TEST_MAX_PATH, m_pMedia->videoInfo.m_szIndexFile)))
                    goto cleanup;
            }
            else
                pTimeStampInitData->pszIndexFile[0] = _T('\0');    
            
            if(pVerificationData)
                pTimeStampInitData->dwTolerance = *((DWORD*)pVerificationData);
                    
            break;

        case CorrectGraph:
            // Convert the vector of filter names into a serialized 2d TCHAR array that's num items x FILTER_NAME_LENGTH
            filterNameList = ( (GraphDesc*)pVerificationData )->GetFilterList();
            pGraphBuildInitData = (GraphBuildInitData*)pInitInfo->pData;
            pGraphBuildInitData->dwNumItems = filterNameList.size();
            pGraphBuildInitData->pszFilterNames = (TCHAR*)(pGraphBuildInitData + sizeof(GraphBuildInitData));
            pInitInfo->cbDataSize = sizeof(VerificationInitInfo) + 
                                 sizeof(GraphBuildInitData) +
                                 (FILTER_NAME_LENGTH * pGraphBuildInitData->dwNumItems * sizeof(TCHAR));

            if(pInitInfo->cbDataSize > MAX_VERIFICATION_MESSAGE_SIZE)
            {
                // Bail out if our message is going to be too big.
                hr = E_INVALIDARG;
                goto cleanup;
            }

            itList = filterNameList.begin();

            for(DWORD dwIndex = 0; dwIndex < pGraphBuildInitData->dwNumItems && itList != filterNameList.end(); dwIndex++)
            {
                if(FAILED(hr = StringCchCopy(pGraphBuildInitData->pszFilterNames + dwIndex * FILTER_NAME_LENGTH,
                                          FILTER_NAME_LENGTH,
                                          *itList)))
                    goto cleanup;
                
                itList++;
            }
            
            break;
     
        default:
            if(pVerificationData)
            {
                // This means that the caller is giving us init data but we aren't currently serializing it. Since we are sending
                // this data across a queue, it MUST be serialized.  It is up to developers of new verifiers to add in serialization
                // code within this function for their verifier-specific init data formats!
                if(m_bRemoteGB)
                {
                    hr = E_INVALIDARG;
                    goto cleanup;
                }
                else
                    // Not using serialized data, but we are okay because aren't using a restricted filter graph so everything 
                    // is running within the same address space.
                    pInitInfo->pData = (BYTE*)pVerificationData;
                    pInitInfo->cbDataSize = 0;
            }          
            break;
    }

    if(m_bRemoteGB)
    {
        // Create the verifier running in the test app process. This verifier is used solely to route verifier calls across
        // the the verifier that is directly attached to the Tap Filter.  The attached verifer is running in a separate process.
        if(FAILED(hr = m_pVerifMgr->EnableVerification(type, VLOC_CLIENT, pInitInfo, this, ppVerifier)))
            goto cleanup;

#ifdef UNDER_CE
        // Send init info over the message queue
        if(!WriteMsgQueue(m_hOutQueue, 
                        (void*)pInitInfo, 
                        sizeof(VerificationInitInfo) + pInitInfo->cbDataSize,
                        0,
                        dwMsgFlags))
        {
            hr = HRESULT_FROM_WIN32 (GetLastError());
            goto cleanup;
        }
#endif                                                       
    }
    else
        hr = m_pVerifMgr->EnableVerification( type, VLOC_LOCAL, pInitInfo, this, ppVerifier );

cleanup:
    if(pInitInfo)
        delete[] pInitInfo;
    
    return hr;
    
}

void 
TestGraph::ReleaseVerifiers()
{
    m_pVerifMgr->ReleaseVerifiers();
}

HRESULT 
TestGraph::StartVerification()
{
    return m_pVerifMgr->StartVerification();
}

void 
TestGraph::StopVerification()
{
    m_pVerifMgr->StopVerification();
}

void 
TestGraph::ResetVerification()
{
    m_pVerifMgr->ResetVerification();
}

HRESULT 
TestGraph::GetVerificationResult( VerificationType type, 
                                  void *pVerificationResult,
                                  DWORD cbDataSize)
{
    return m_pVerifMgr->GetVerificationResult( type, pVerificationResult, cbDataSize);
}

HRESULT 
TestGraph::GetVerificationResults()
{
    return m_pVerifMgr->GetVerificationResults();
}

HRESULT TestGraph::EnableLogging(EventLoggerType type, void* pEventLoggerData)
{
    HRESULT hr = S_OK;
    IEventLogger* pLogger = NULL;
    
    // Create the verifier needed for this type
    hr = CreateEventLogger(type, pEventLoggerData, this, &pLogger);
    if (FAILED(hr))
        return hr;

    // Add to our list of loggers
    m_EventLoggerList.insert(EventLoggerPair(type, pLogger));

    return hr;
}

void TestGraph::ReleaseLoggers()
{
    EventLoggerList::iterator iterator = m_EventLoggerList.begin();
    while (iterator != m_EventLoggerList.end())
    {
        IEventLogger* pEventLogger = iterator->second;
        pEventLogger->Stop();
        delete pEventLogger;
        iterator++;
    }
    m_EventLoggerList.clear();
}

HRESULT TestGraph::StartLogging()
{
    HRESULT hr = S_OK;
    EventLoggerList::iterator iterator = m_EventLoggerList.begin();
    while (iterator != m_EventLoggerList.end())
    {
        EventLoggerType type = (EventLoggerType)iterator->first;
        IEventLogger* pEventLogger = iterator->second;
        hr = pEventLogger->Start();
        if (FAILED_F(hr))
            break;
        iterator++;
    }
    return hr;
}

void TestGraph::StopLogging()
{
    EventLoggerList::iterator iterator = m_EventLoggerList.begin();
    while (iterator != m_EventLoggerList.end())
    {
        IEventLogger* pEventLogger = iterator->second;
        pEventLogger->Stop();
        iterator++;
    }
}

void TestGraph::ResetLogging()
{
    EventLoggerList::iterator iterator = m_EventLoggerList.begin();
    while (iterator != m_EventLoggerList.end())
    {
        IEventLogger* pEventLogger = iterator->second;
        pEventLogger->Reset();
        iterator++;
    }
}

FilterDesc* TestGraph::GetFilterDesc(DWORD filterClass, DWORD filterSubClass)
{
    // Go through the list of filters and find the first filter that fits the requested class and sub-class
    FilterDescList::iterator iterator = m_FilterList.begin();
    FilterDesc* pSelectedFilter = NULL;
    while(iterator != m_FilterList.end())
    {
        FilterDesc* pFilterDesc = *iterator;
        DWORD filterType = pFilterDesc->GetType();

        // Either the class is not specified or is also set in the filter
        if ((!filterSubClass || (filterSubClass & SubClass(filterType))) &&
            (!filterClass || (filterClass & Class(filterType))))
        {
            pSelectedFilter = pFilterDesc;
            break;
        }

        iterator++;
    }

    return pSelectedFilter;
}

FilterDesc* TestGraph::GetFilterDesc(DWORD filterType)
{
    return GetFilterDesc(Class(filterType), SubClass(filterType));
}

FilterDesc* TestGraph::GetFilter(IBaseFilter* pBaseFilter)
{
    // Go through the list of filters and find the filter that has the same base filter
    FilterDescList::iterator iterator = m_FilterList.begin();
    while(iterator != m_FilterList.end())
    {
        FilterDesc* pFilterDesc = *iterator;
        if (pFilterDesc->GetFilterInstance() == pBaseFilter)
            return pFilterDesc;
        iterator++;
    }

    return NULL;
}

HRESULT TestGraph::InsertTapFilter(IPin* pPin, ITapFilter** ppTapFilter)
{
    HRESULT hr = S_OK;

    // Create the tap filter
    ITapFilter* pTapFilter = NULL;
    IGraphBuilder* pGraphBuilder = NULL;
    // hr = CTapFilter::CreateInstance(&pTapFilter);

    hr = CoCreateInstance( CLSID_TapFilter, NULL, CLSCTX_INPROC_SERVER, IID_ITapFilter, (void**) &pTapFilter );
    if(FAILED(hr) || pTapFilter == NULL)
    {
        // the tap filter wasn't found, so we assume it's not registered and register it.
        if(S_OK != RegisterDll(TEXT("TapFilter.dll")))
            if(S_OK != RegisterDll(TEXT("\\hard disk\\TapFilter.dll")))
                if(S_OK != RegisterDll(TEXT("\\release\\TapFilter.dll")))
                    if(S_OK != RegisterDll(TEXT("\\Storage Card\\TapFilter.dll")))
                    {
                        LOG(TEXT("%S: ERROR %d@%S - Unable to register TapFilter.dll (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
                        goto cleanup;
                    }

        // try again now that the filter should be properly registered
        hr = CoCreateInstance( CLSID_TapFilter, NULL, CLSCTX_INPROC_SERVER, IID_ITapFilter, (void**) &pTapFilter );
        if(FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - CoCreateInstance failed when acquiring a TapFilter instance (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            goto cleanup;
        }
    }


    if (!pTapFilter)
    {
        hr = E_FAIL;
        goto cleanup;
    }
    
    // Get the graph
    if ((pGraphBuilder = GetGraph()) == 0)
    {
        hr = E_FAIL;
        goto cleanup;
    }

    // Insert the tap filter at the output of the specified pin
    hr = pTapFilter->Insert(pGraphBuilder, pPin);
    
    if(FAILED_F(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to insert tap filter (0x%x) \n"), __FUNCTION__, __LINE__, __FILE__, hr);
    }
    else 
    {
        LOG(TEXT("%S: inserted tap filter \n"), __FUNCTION__);
        
        // Return the tap filter
        *ppTapFilter = pTapFilter;        
    }

cleanup:
    if(FAILED_F(hr))
    {
        if(pTapFilter)
            pTapFilter->Release();
    }

    if(pGraphBuilder)
        pGraphBuilder->Release();

    return hr;
}

HRESULT TestGraph::InsertTapFilter(DWORD filterType, PIN_DIRECTION pindir, GUID majortype, ITapFilter** ppTapFilter)
{
    HRESULT hr = S_OK;

    if (!ppTapFilter)
        return E_POINTER;

    FilterDesc* pFilterDesc = GetFilterDesc(filterType);
    IPin* pPin = NULL;
    IPin *pTmpOutputPin = NULL;

    if (pFilterDesc)
    {
        // BUGBUG: ignore the media type for now and just get the 1st output pin
        int i = 0;
        while (true)
        {
            // Get the requested pin from the filter
            pPin = pFilterDesc->GetPin(i, pindir);
            if (!pPin)
                break;
            
            // If there was no media type specified, then just choose the first pin
            if (majortype == GUID_NULL)
                break;

            // Determine if the media type matches the pin's media type
            AM_MEDIA_TYPE mt;
            pPin->ConnectionMediaType(&mt);
            // Free the format block
            FreeMediaType(mt);
            if (mt.majortype == majortype)
                break;

            // Release the acquired pins
            pPin->Release();
            pPin = NULL;

            // Go to the next pin
            i++;
        }        
    }

    // We have the pin where we have to insert the tap
    if (pPin)
    {
        IPin* pTo   = NULL;
        IPin* pTemp = NULL;
    
        pPin->ConnectedTo( &pTemp );
        if ( PINDIR_INPUT == pindir )
        {
            pTo = pPin;
            pPin = pTemp;
        }
        else if ( PINDIR_OUTPUT == pindir )
            pTo = pTemp;

        if(FAILED(hr = InsertTapFilter(pPin, ppTapFilter)))
            goto cleanup;

        // Fix the problems introduced by disconnecting and connecting pins.
        bool repair = RepairGraphAfterInsertTapFilter( pPin, pTo, m_pGraphBuilder );
        if ( !repair )
            LOG( TEXT("%S: WARNING %d@%S - Can not repaire the graph after inserting tap filter."), 
                        __FUNCTION__, __LINE__, __FILE__);

        pPin->Release();
        pTo->Release();
    }
    else 
    {
        hr = E_FAIL;
        goto cleanup;
    }

    // specifically put the graph in the stop state.
    hr = SetState(State_Stopped, TestGraph::SYNCHRONOUS);

cleanup:

    return hr;
}

HRESULT TestGraph::DisconnectTapFilter(DWORD filter, GUID majortype)
{
    // BUGBUG: remove the filter from the graph
    return S_OK;
}

HRESULT 
TestGraph::GetMPEG1WaveFormat( PlaybackAudioInfo* pAudio, 
                               MPEG1WAVEFORMAT* pMPEGWfex )
{
    HRESULT hr = S_OK;

    if ( !pAudio || !pMPEGWfex )
        return E_INVALIDARG;

    DWORD dwBitRate = pMPEGWfex->dwHeadBitrate;
    pAudio->decSpecificInfo.mpeg1AudioInfo.mode = pMPEGWfex->fwHeadMode;
    pAudio->decSpecificInfo.mpeg1AudioInfo.layer = pMPEGWfex->fwHeadLayer;
    DWORD nChannels = pMPEGWfex->wfx.nChannels;
    DWORD nSamplesPerSec = pMPEGWfex->wfx.nSamplesPerSec;

    return hr;
}

HRESULT TestGraph::GetWaveFormatEx( PlaybackAudioInfo* pAudio, WAVEFORMATEX* pWfex )
{
    HRESULT hr = S_OK;

    if ( !pAudio || !pWfex )
        return E_INVALIDARG;

    DWORD nChannels = pWfex->nChannels;
    DWORD nSamplesPerSec = pWfex->nSamplesPerSec;

    return hr;
}

HRESULT TestGraph::GetAudioFormat(PlaybackAudioInfo* pAudio, AM_MEDIA_TYPE* pMediaType)
{
    HRESULT hr = S_OK;

    if (!pAudio || !pMediaType)
        return E_INVALIDARG;

    pAudio->bCompressed = (pMediaType->bTemporalCompression) ? true : false;
    pAudio->m_dwSampleSize = (pMediaType->bFixedSizeSamples) ? 0 : pMediaType->lSampleSize;

    if (pMediaType->formattype == FORMAT_WaveFormatEx)
    {
        hr = GetWaveFormatEx(pAudio, (WAVEFORMATEX*)pMediaType->pbFormat);
    }
    //else if this is MPEG audio
    //{
    //    hr = GetMPEG1WaveFormat(pAudio, (MPEG1WAVEFORMAT*)pMediaType->pbFormat);
    //}
    else hr = E_FAIL;

    return hr;
}

HRESULT TestGraph::GetAudioInfo(PlaybackMedia* pMedia)
{
    HRESULT hr = S_OK;
    AM_MEDIA_TYPE mt;
    const char* szAudioType = NULL;
    bool bDone = false;

    if (!pMedia)
        return E_INVALIDARG;

    // Check if there is a audio renderer 
    FilterDesc* pRendererDesc = GetFilterDesc(Renderer, AudioRenderer);
    if (!pRendererDesc)
    {
        pMedia->nAudioStreams = 0;
        return S_OK;
    }

    // BUGBUG: there is atleast one audio stream because there is a audio renderer
    pMedia->nAudioStreams = 1;

    FilterDesc* pDecoderDesc = GetFilterDesc(Decoder, AudioDecoder);
    if (pDecoderDesc)
    {
        // BUGBUG: There is a decoder - check the first pin
        IPin* pInputPin = pDecoderDesc->GetPin(0, PINDIR_INPUT);
        if (!pInputPin)
            return hr;

        // Get the input pin's media type
        hr = pInputPin->ConnectionMediaType(&mt);
        if (FAILED(hr))
        {
            // pInputPin has been addRef in GetPin, need to release it
            pInputPin->Release();
            return hr;
        }

        // Check the media type - make sure it's audio
        if (!IsAudioType(&mt))
        {
            pInputPin->Release();
            return E_FAIL;
        }

        // Get the type
        szAudioType = GetTypeName(&mt);
        
        if (!szAudioType)
            szAudioType = "Unknown";

        // release the input pin
        pInputPin->Release();
    }
    
    // If we still haven't figured out the audio type - check if there is a splitter filter
    FilterDesc* pSplitterDesc = NULL;
    if (!szAudioType)
    {
        pSplitterDesc = GetFilterDesc(Splitter, 0);

        if (pSplitterDesc)
        {
        }
    }

    // Now get the audio format
    if (szAudioType)
    {
        // Copy the type into the media object
        CharToTChar(szAudioType, pMedia->audioInfo.m_szType, countof(pMedia->audioInfo.m_szType));

        hr = GetAudioFormat(&pMedia->audioInfo, &mt);
    }


    return (bDone) ? S_OK : E_FAIL;
}

HRESULT 
TestGraph::GetBitmapInfoHeader( PlaybackVideoInfo* pVideo, 
                                BITMAPINFOHEADER* pBitmapInfoHeader )
{
    if ( !pVideo || !pBitmapInfoHeader )
        return E_INVALIDARG;

    pVideo->m_nWidth = pBitmapInfoHeader->biWidth;
    pVideo->m_nHeight = pBitmapInfoHeader->biHeight;
    return S_OK;
}


HRESULT 
TestGraph::GetMPEG1VideoInfo( PlaybackVideoInfo* pVideo, 
                              MPEG1VIDEOINFO* pMPEGVideoInfo )
{
    HRESULT hr = S_OK;

    if ( !pVideo || !pMPEGVideoInfo )
        return E_INVALIDARG;

    hr = GetVideoInfoFormat( pVideo, &pMPEGVideoInfo->hdr );
    return hr;
}

HRESULT 
TestGraph::GetMPEG2VideoInfo( PlaybackVideoInfo* pVideo, 
                              MPEG2VIDEOINFO* pMPEGVideoInfo )
{
    HRESULT hr = S_OK;

    if ( !pVideo || !pMPEGVideoInfo )
        return E_INVALIDARG;

    pVideo->decSpecificInfo.mpeg2VideoInfo.dwProfile = pMPEGVideoInfo->dwProfile;
    pVideo->decSpecificInfo.mpeg2VideoInfo.dwLevel = pMPEGVideoInfo->dwLevel;
    hr = GetVideoInfoFormat2(pVideo, &pMPEGVideoInfo->hdr);
    return hr;
}

HRESULT 
TestGraph::GetVideoInfoFormat( PlaybackVideoInfo* pVideo, 
                               VIDEOINFOHEADER* pVideoInfoHeader )
{
    HRESULT hr = S_OK;

    if ( !pVideo || !pVideoInfoHeader )
        return E_INVALIDARG;

    pVideo->dwBitRate = pVideoInfoHeader->dwBitRate;
    pVideo->dwAvgTimePerFrame = pVideoInfoHeader->AvgTimePerFrame;

    hr = GetBitmapInfoHeader(pVideo, &pVideoInfoHeader->bmiHeader);
    return hr;
}

HRESULT 
TestGraph::GetVideoInfoFormat2( PlaybackVideoInfo* pVideo, 
                                VIDEOINFOHEADER2* pVideoInfoHeader )
{
    HRESULT hr = S_OK;

    if ( !pVideo || !pVideoInfoHeader )
        return E_INVALIDARG;

    pVideo->m_dwBitRate = pVideoInfoHeader->dwBitRate;
    pVideo->dwAvgTimePerFrame = pVideoInfoHeader->AvgTimePerFrame;
    pVideo->m_bInterlaced = (pVideoInfoHeader->dwInterlaceFlags == 0) ? false : true;
    pVideo->m_fXAspectRatio = (FLOAT)pVideoInfoHeader->dwPictAspectRatioX;
    pVideo->m_fYAspectRatio = (FLOAT)pVideoInfoHeader->dwPictAspectRatioY;

    hr = GetBitmapInfoHeader(pVideo, &pVideoInfoHeader->bmiHeader);
    return hr;
}

HRESULT 
TestGraph::GetVideoFormat( PlaybackVideoInfo* pVideo, 
                           AM_MEDIA_TYPE* pMediaType )
{
    HRESULT hr = S_OK;

    if (!pVideo || !pMediaType)
        return E_INVALIDARG;

    pVideo->m_bCompressed = (pMediaType->bTemporalCompression) ? true : false;
    pVideo->m_dwFrameSize = (pMediaType->bFixedSizeSamples) ? 0 : pMediaType->lSampleSize;

    if (pMediaType->formattype == FORMAT_VideoInfo)
    {
        hr = GetVideoInfoFormat(pVideo, (VIDEOINFOHEADER*)pMediaType->pbFormat);
    }
    else if (pMediaType->formattype == FORMAT_VideoInfo2)
    {
        hr = GetVideoInfoFormat2(pVideo, (VIDEOINFOHEADER2*)pMediaType->pbFormat);
    }
    else if (pMediaType->formattype == FORMAT_MPEGVideo)
    {
        hr = GetMPEG1VideoInfo(pVideo, (MPEG1VIDEOINFO*)pMediaType->pbFormat);
    }
    else if (pMediaType->formattype == FORMAT_MPEG2Video)
    {
        hr = GetMPEG2VideoInfo(pVideo, (MPEG2VIDEOINFO*)pMediaType->pbFormat);
    }
    else hr = E_FAIL;

    return hr;
}

HRESULT 
TestGraph::DetermineSystemStreamFromSplitter( FilterDesc* pFilterDesc, 
                                              PlaybackMedia* pMedia )
{
    return E_NOTIMPL;
}

HRESULT TestGraph::GetSystemInfo(PlaybackMedia* pMedia)
{
    HRESULT hr = S_OK;

    if (!pMedia)
        return E_INVALIDARG;

    // To determine if there is a system stream and it's characteristics
    // - look for a splitter. If found, then this is a system stream.
    // - then check the input type to the splitter and try to map that to a system type name
    FilterDesc* pSplitterDesc = GetFilterDesc(Splitter, 0);

    // BUGBUG: assumign that there won't be more than one system stream by definition
    pMedia->nSystemStreams = (NULL == pSplitterDesc) ? 0 : 1;

    if (!pMedia->nSystemStreams)
        return S_FALSE;

    bool bSourceIsSplitter = (IsSourceFilter(pSplitterDesc->GetType())) ? true : false;

    if (bSourceIsSplitter)
    {
        // If the splitter filter and source filters are the same
        // Determine the system stream type from the actual filter itself
        hr = DetermineSystemStreamFromSplitter(pSplitterDesc, pMedia);
    }
    else {
        // Else the splitter filter has an input type - analyze that
        AM_MEDIA_TYPE mt;
        const char* szSystemType = NULL;

        // BUGBUG: assuming that there cannot be more than one input to the splitter filter, get the first input pin
        IPin* pPin = pSplitterDesc->GetPin(0, PINDIR_INPUT);

        if (pPin)
        {
            // Get the input pin's media type
            hr = pPin->ConnectionMediaType(&mt);
            if (SUCCEEDED(hr))
                // Get the type name from the type
                szSystemType = GetTypeName(&mt);
            
            if (!szSystemType)        
                szSystemType = "Unknown";

            // Release obtained pin
            pPin->Release();
        }
        else {
            LOG(TEXT("%S: ERROR %d@%S - failed to get input pin from splitter\n"), __FUNCTION__, __LINE__, __FILE__);
            hr = E_FAIL;
        }

        if (SUCCEEDED(hr))
        {
            // Copy to the system info type
            CharToTChar(szSystemType, pMedia->systemInfo.szType, countof(pMedia->systemInfo.szType));
        }
    }
    
    return hr;
}

HRESULT TestGraph::GetVideoInfo(PlaybackMedia* pMedia)
{
    HRESULT hr = S_OK;

    if (!pMedia)
        return E_INVALIDARG;

    AM_MEDIA_TYPE mt;
    const char* szVideoType = NULL;
    bool bDone = false;

    // Determining number of video streams?
    // Just look at the source filter and count the video output types
    // If not, look for the splitter and see if it has a video output type
    // BUGBUG: If MPEG Splitter, we can use IAMStreamSelect. If ASF - this interface is not supported - and so use ILanguageSelection for multi-language streams

    // Check if there is a video mixing renderer 
    FilterDesc* pRendererDesc = GetFilterDesc(Renderer, VideoMixingRenderer);
    if (!pRendererDesc)
    {
        pMedia->nVideoStreams = 0;
        return S_OK;
    }

    // BUGBUG: there is atleast one video stream because there is a video renderer
    pMedia->nVideoStreams = 1;

    FilterDesc* pDecoderDesc = GetFilterDesc(Decoder, VideoDecoder);
    if (pDecoderDesc)
    {
        // BUGBUG: There is a decoder - check the first pin
        IPin* pInputPin = pDecoderDesc->GetPin(0, PINDIR_INPUT);
        if (!pInputPin)
        {
            // pInputPin has been addRef in GetPin, need to release it
            pInputPin->Release();
            return hr;
        }

        // Get the input pin's media type
        hr = pInputPin->ConnectionMediaType(&mt);
        if (FAILED(hr))
        {
            // pInputPin has been addRef in GetPin, need to release it
            pInputPin->Release();
            return hr;
        }

        // Check the media type - make sure it's video
        if (!IsVideoType(&mt))
            return E_FAIL;

        // Get the type
        szVideoType = GetTypeName(&mt);
        
        if (!szVideoType)
            szVideoType = "Unknown";

        // pInputPin has been addRef in GetPin, need to release it
        pInputPin->Release();
    }
    
    // If we still haven't figured out the video type - check if there is a splitter filter
    FilterDesc* pSplitterDesc = NULL;
    if (!szVideoType)
    {
        pSplitterDesc = GetFilterDesc(Splitter, 0);

        if (pSplitterDesc)
        {
        }
    }

    // Now get the video format
    if (szVideoType)
    {
        // Copy the type into the media object
        CharToTChar(szVideoType, pMedia->videoInfo.m_szType, countof(pMedia->videoInfo.m_szType));

        hr = GetVideoFormat(&pMedia->videoInfo, &mt);
    }

    return (bDone) ? S_OK : E_FAIL;
}

HRESULT TestGraph::GetMediaInfo(PlaybackMedia* pMedia)
{
    HRESULT hr = S_OK;

    if (!pMedia)
        return E_INVALIDARG;

    if (!m_pGraphBuilder)
        return E_FAIL;

    // Acquire some interfaces from the graph
    hr = AcquireInterfaces();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: Acquiring common interfaces failed (0x%x) \n"), __FUNCTION__, hr);
        return hr;
    }

    // Get the duration
    hr = m_pMediaSeeking->GetDuration(&pMedia->duration);
    if (FAILED(hr))
    {
        LOG(TEXT("%S: failed to get duration (0x%x) \n"), __FUNCTION__, hr);
        return hr;
    }
    
    // Preroll
    hr = m_pMediaSeeking->GetPreroll(&pMedia->preroll);
    if (FAILED(hr))
    {
        LOG(TEXT("%S: failed to get preroll (0x%x) \n"), __FUNCTION__, hr);
        // Set the preroll as 0 - don't have to exit
        pMedia->preroll = 0;
        hr = S_OK;
    }

    // Determine if this is a system stream and it's characteristic
    GetSystemInfo(pMedia);
    
    // Determine the video streams and their characteristics
    GetVideoInfo(pMedia);

    // Determine the audio streams and their characteristics
    GetAudioInfo(pMedia);

    return S_OK;
}

HRESULT TestGraph::GetVideoWidth(long *pWidth)
{
    HRESULT hr = E_FAIL;
    LONG nHeight = 0;
    LONG arWidth = 0;
    LONG arHeight = 0;

    if (!pWidth)
    {
        hr = E_INVALIDARG;
        goto cleanup;
    }

    if (!m_pBasicVideo && !m_pVMRWindowlessControl)
        AcquireInterfaces();

    if(m_pBasicVideo)
        hr = m_pBasicVideo->get_VideoWidth(pWidth);
    else if(m_pVMRWindowlessControl)
        hr = m_pVMRWindowlessControl->GetNativeVideoSize(pWidth, &nHeight, &arWidth, &arHeight);

cleanup:
    
    return hr;
}

HRESULT TestGraph::GetVideoHeight(long *pHeight)
{
    HRESULT hr = E_FAIL;
    LONG nWidth = 0;
    LONG arWidth = 0;
    LONG arHeight = 0;

    if (!pHeight)
    {
        hr = E_INVALIDARG;
        goto cleanup;
    }

    if (!m_pBasicVideo && !m_pVMRWindowlessControl)
        AcquireInterfaces();

    if(m_pBasicVideo)
        hr = m_pBasicVideo->get_VideoHeight(pHeight);
    else if(m_pVMRWindowlessControl)
        hr = m_pVMRWindowlessControl->GetNativeVideoSize(&nWidth, pHeight, &arWidth, &arHeight);

cleanup:
    
    return hr;
        
}

HRESULT TestGraph::SetVRImageSize(VRImageSize imageSize)
{
    HRESULT hr = S_OK;
    if (!m_pVideoWindow)
    {
        AcquireInterfaces();
        if (!m_pVideoWindow)
            return E_FAIL;
    }

    if (imageSize == MIN_IMAGE_SIZE)
    {
        long width, height;
        hr = m_pVideoWindow->GetMinIdealImageSize(&width, &height);
        if (SUCCEEDED(hr))
            hr = SetVideoWindowSize(width, height);
    }
    else if (imageSize == MAX_IMAGE_SIZE)
    {
        long width, height;
        hr = m_pVideoWindow->GetMaxIdealImageSize(&width, &height);
        if (SUCCEEDED(hr))
            hr = SetVideoWindowSize(width, height);
    }
    else if (imageSize == FULL_SCREEN)
    {
        hr = m_pVideoWindow->put_FullScreenMode(OATRUE);
        hr = m_pVideoWindow->SetWindowForeground(OATRUE);
        // It is expected.  We need use this call to bring window foreground.
        if ( VFW_E_IN_FULLSCREEN_MODE == hr )
            hr = S_OK;
    }

    return hr;
}

HRESULT TestGraph::SetFullScreenMode(bool bFullScreen)
{
    HRESULT hr = m_pVideoWindow->put_FullScreenMode((bFullScreen) ? OATRUE : OAFALSE);
    return hr;
}

HRESULT TestGraph::SetSourceRect(long left, long top, long width, long height)
{
    HRESULT hr = S_OK;
    if (!m_pBasicVideo)
    {
        AcquireInterfaces();
        if (!m_pBasicVideo)
            return E_FAIL;
    }

    hr = m_pBasicVideo->SetSourcePosition(left, top, width, height);

    return hr;
}

HRESULT TestGraph::SetDestinationRect(long left, long top, long width, long height)
{
    HRESULT hr = S_OK;
    if (!m_pBasicVideo)
    {
        AcquireInterfaces();
        if (!m_pBasicVideo)
            return E_FAIL;
    }

    hr = m_pBasicVideo->SetDestinationPosition(left, top, width, height);

    return hr;
}

HRESULT TestGraph::GetVideoWindowPosition(long* pLeft, long* pTop, long* pWidth, long* pHeight)
{
    HRESULT hr = S_OK;
    if (!m_pVideoWindow)
    {
        AcquireInterfaces();
        if (!m_pVideoWindow)
            return E_FAIL;
    }

    hr = m_pVideoWindow->GetWindowPosition(pLeft, pTop, pWidth, pHeight);

    return hr;
}

HRESULT TestGraph::GetVideoWindowSize(long* pWidth, long* pHeight)
{
    HRESULT hr = S_OK;
    if (!m_pVideoWindow)
    {
        AcquireInterfaces();
        if (!m_pVideoWindow)
            return E_FAIL;
    }

    hr = m_pVideoWindow->get_Width(pWidth);
    if (SUCCEEDED(hr))
        hr = m_pVideoWindow->get_Height(pHeight);

    return hr;
}

HRESULT TestGraph::SetVideoWindowPosition(long left, long top, long width, long height)
{
    HRESULT hr = S_OK;
    if (!m_pVideoWindow)
    {
        AcquireInterfaces();
        if (!m_pVideoWindow)
            return E_FAIL;
    }

    hr = m_pVideoWindow->SetWindowPosition(left, top, width, height);

    return hr;
}

HRESULT TestGraph::SetVideoWindowSize(long width, long height)
{
    HRESULT hr = S_OK;
    if (!m_pVideoWindow)
    {
        AcquireInterfaces();
        if (!m_pVideoWindow)
        {
            hr = E_FAIL;
            goto cleanup;
        }
    }

    long prevWidthSet = m_VideoWindowWidthSet;
    long prevHeightSet = m_VideoWindowHeightSet;

    // put_Width generates a synchronous redraw at the renderer.
    // if there are verifiers that depend on the width being set already in the test graph, those might fail
    // so set the width being set currently
    // this does require that no verification is going on when this method is called

    m_VideoWindowWidthSet = width;

    m_pVerifMgr->UpdateVerifier(VerifyCodecScale, (void*)&width, sizeof(long));
    m_pVerifMgr->UpdateVerifier(VerifyCodecNotScale, (void*)&width, sizeof(long));
    
    hr = m_pVideoWindow->put_Width(width);
    if (SUCCEEDED(hr))
    {
        m_VideoWindowHeightSet = height;

        m_pVerifMgr->UpdateVerifier(VerifyCodecScale, (void*)&height, sizeof(long));
        m_pVerifMgr->UpdateVerifier(VerifyCodecNotScale, (void*)&height, sizeof(long));
        
        hr = m_pVideoWindow->put_Height(height);

        if (FAILED(hr))
        {
            m_VideoWindowHeightSet = prevHeightSet;
            m_pVerifMgr->UpdateVerifier(VerifyCodecScale, (void*)&prevHeightSet, sizeof(long));
            m_pVerifMgr->UpdateVerifier(VerifyCodecNotScale, (void*)&prevHeightSet, sizeof(long));
        }
    }
    else 
    {
        m_VideoWindowWidthSet = prevWidthSet;
        m_pVerifMgr->UpdateVerifier(VerifyCodecScale, (void*)&prevWidthSet, sizeof(long));
        m_pVerifMgr->UpdateVerifier(VerifyCodecNotScale, (void*)&prevWidthSet, sizeof(long));
    }

cleanup:

    return hr;
}

HRESULT TestGraph::GetVideoWindowSizeSet(long* pWidth, long* pHeight)
{
    HRESULT hr = S_OK;

    *pWidth = m_VideoWindowWidthSet;
    *pHeight = m_VideoWindowHeightSet;

    return hr;
}

HRESULT TestGraph::GetBasicVideo( IBasicVideo **ppBasicVideo )
{
    HRESULT hr = S_OK;
    
    if (!m_pBasicVideo)
    {
        AcquireInterfaces();
        if (!m_pBasicVideo)
            return E_FAIL;
    }

    *ppBasicVideo = m_pBasicVideo;
    return hr;
}

HRESULT TestGraph::GetBasicAudio( IBasicAudio **ppBasicAudio )
{
    HRESULT hr = S_OK;
    
    if (!m_pBasicAudio)
    {
        AcquireInterfaces();
        if (!m_pBasicAudio)
            return E_FAIL;
    }

    *ppBasicAudio = m_pBasicAudio;
    return hr;
}

HRESULT TestGraph::GetMediaControl(IMediaControl** ppMediaControl)
{
    HRESULT hr = S_OK;
    
    if (!m_pMediaControl)
    {
        AcquireInterfaces();
        if (!m_pMediaControl)
            return E_FAIL;
    }

    *ppMediaControl = m_pMediaControl;
    return hr;
}

HRESULT TestGraph::GetMediaSeeking(IMediaSeeking** ppMediaSeeking)
{
    HRESULT hr = S_OK;
    
    if (!m_pMediaSeeking)
    {
        AcquireInterfaces();
        if (!m_pMediaSeeking)
            return E_FAIL;
    }

    *ppMediaSeeking = m_pMediaSeeking;
    return hr;
}

HRESULT TestGraph::GetMediaPosition(IMediaPosition** ppMediaPosition)
{
    HRESULT hr = S_OK;
    
    if (!m_pMediaPosition)
    {
        AcquireInterfaces();
        if (!m_pMediaPosition)
            return E_FAIL;
    }

    *ppMediaPosition = m_pMediaPosition;
    return hr;
}


HRESULT TestGraph::GetMediaEvent( IMediaEvent** ppMediaEvent )
{
    HRESULT hr = S_OK;
    
    if (!m_pMediaEvent)
    {
        AcquireInterfaces();
        if (!m_pMediaEvent)
            return E_FAIL;
    }

    *ppMediaEvent = m_pMediaEvent;
    return hr;
}

HRESULT TestGraph::GetMediaEventEx( IMediaEventEx** ppMediaEventEx )
{
    HRESULT hr = S_OK;
    
    if (!m_pMediaEventEx)
    {
        AcquireInterfaces();
        if (!m_pMediaEventEx)
            return E_FAIL;
    }

    *ppMediaEventEx = m_pMediaEventEx;
    return hr;
}

HRESULT TestGraph::GetVideoWindow( IVideoWindow** ppVideoWindow )
{
    HRESULT hr = S_OK;
    
    if ( !m_pVideoWindow )
    {
        AcquireInterfaces();
        if (!m_pVideoWindow)
            return E_FAIL;
    }

    *ppVideoWindow = m_pVideoWindow;
    return hr;
}

HRESULT TestGraph::UseRemoteGB( BOOL bRemote )
{ 
    HRESULT hr = S_OK;
    WCHAR* pszOutName = NULL;
    size_t pcch = 0;

    // if we are saving to .GRF files, we ALWAYS want to run using the non-restricted graph
    // to ensure all filters make it into the filter graph.  Therefore, we ignore the incoming value.
    if(m_bSaveGraph)
    {
        bRemote = false;
        goto cleanup;
    }

#ifdef UNDER_CE
    if(bRemote)
    {
        MSGQUEUEOPTIONS mqo_out;
        // We already have a queue established for verifier information and shouldn't make another one.
        if(m_hOutQueue)
            goto cleanup;

        // Create the message queue used to send verifier init data to the tap filter
        mqo_out.dwSize = sizeof(MSGQUEUEOPTIONS);
        mqo_out.dwFlags = MSGQUEUE_ALLOW_BROKEN | MSGQUEUE_NOPRECOMMIT;      // Allows queue to be written to even if there is no attached reader & vice versa
        mqo_out.dwMaxMessages = 0;                   // No max number of messages in the queue at one time
        mqo_out.cbMaxMessage = MAX_VERIFICATION_MESSAGE_SIZE;           
        mqo_out.bReadAccess = FALSE;  
    
        if(FAILED(hr = StringCchLength(DEFAULT_INIT_QUEUE_NAME, TEST_MAX_PATH - 16, &pcch)))
            goto cleanup;

        if((pszOutName = new WCHAR[pcch + 16]) == 0)
        {
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }

        if(FAILED(hr = StringCbCopy(pszOutName, (pcch + 16) * sizeof(WCHAR) ,  DEFAULT_INIT_QUEUE_NAME)))
            goto cleanup;

        if(FAILED(hr = StringCbCatW(pszOutName, (pcch + 16) * sizeof(WCHAR), L"-to tapfilter")))
            goto cleanup;

        if((m_hOutQueue = CreateMsgQueue(pszOutName, &mqo_out)) == 0)
        {
            hr = HRESULT_FROM_WIN32 (GetLastError());
            LOG(TEXT("%S: failed to create Tap Filter Init Message Queue! (0x%x) \n"), __FUNCTION__, hr);
            goto cleanup;
        }
            
    }
    else
    {
        // Most of the time this failure code isn't monitored, but we don't need to call this function if we weren't in a remote state before
        if(m_hOutQueue)
        {
            CloseMsgQueue(m_hOutQueue);
            hr = HRESULT_FROM_WIN32 (GetLastError());
            m_hOutQueue = NULL;
        }
    }
#endif
    
cleanup:
    m_bRemoteGB = bRemote;

    if(pszOutName)
        delete[] pszOutName;

    return hr;
}

//
// pFrom: original output pin before inserting the tap filter
// pTo:   original receive pin before inserting the tap filter
// 
bool RepairGraphAfterInsertTapFilter( IPin *pFrom, IPin *pTo, IGraphBuilder* pGraphBuilder )
{
    
    ASSERT( pFrom );
    ASSERT( pTo );
    ASSERT( pGraphBuilder );

    HRESULT hr = S_OK;
    IPin *pTmpPin = NULL;
    IBaseFilter *pToFilter = NULL;

    PIN_INFO toPinInfo;
    ZeroMemory( &toPinInfo, sizeof(PIN_INFO) );

    // Retreive the pin info of the downstream filter of tap filter.  PIN_INFO contains
    // a pointer to the filter.  We need to use the filter pointer to get the output pin
    // since its connection and properties could be changed during the process of inserting
    // tap filter.
    hr = pTo->QueryPinInfo( &toPinInfo );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - QueryPinInfo failed. (0x%08x)"), 
                __FUNCTION__, __LINE__, __FILE__, hr );
        return false;
    }
    pToFilter = toPinInfo.pFilter;

    IEnumPins* pEnumPins = NULL;
    int nPins = 0;

    // Get the output pin of the downstream filter so that we can render and reconnect
    // that pin to repaire any defects caused by inserting the tap filter.
    hr = pToFilter->EnumPins(&pEnumPins);
    if ( FAILED(hr) )
    {
        LOG(  TEXT("%S: ERROR %d@%S - RepairGraphAfterInsertTapFilter: EnumPins failed. (0x%08x)"), 
                __FUNCTION__, __LINE__, __FILE__, hr );
        return false;
    }    

    hr = pEnumPins->Reset();
    if ( FAILED(hr) ) 
    {
        LOG( TEXT("%S: ERROR %d@%S - RepairGraphAfterInsertTapFilter: EnumPins::Reset failed.(0x%08x)"), 
                __FUNCTION__, __LINE__, __FILE__, hr );
        pEnumPins->Release();
        return false;
    }

    PIN_DIRECTION dir;
    UINT nOutputPins = 0;
    while ( (hr = pEnumPins->Next(1, &pTmpPin, 0)) == S_OK ) {
        pTmpPin->QueryDirection(&dir);
        if ( PINDIR_OUTPUT == dir )
        {
            nOutputPins++;
            break;
        }
        pTmpPin->Release();
    }

    if ( 0 != nOutputPins ) // we got an output pin, otherwise it is a renderer
    {
        // Reconnects the files in the graph staring from the specified output pin
        // because during the insertion, the output pin of the downstream filter may
        // be disconnected or some pin properties may be reset.  Need to use both 
        // functions below to cover all media types.
        hr = pGraphBuilder->Render( pTmpPin );
        if ( FAILED(hr) && VFW_E_ALREADY_CONNECTED != hr )
        {
            LOG(TEXT("%S: ERROR %d@%S - Render the output pin failed. (0x%x) \n"), 
                    __FUNCTION__, __LINE__, __FILE__, hr);
            pTmpPin->Release();
            pTmpPin = NULL;
            return false;
        }

        hr = pGraphBuilder->Reconnect( pTmpPin );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Reconnect output pin of %S failed.(0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        }
        pTmpPin->Release();
        pTmpPin = NULL;
    }
    
    if (pEnumPins)
        pEnumPins->Release();

    if(pToFilter)
        pToFilter->Release();

    return true;
}
