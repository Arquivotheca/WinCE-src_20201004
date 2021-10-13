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
#include "utility.h"

/////////////////////////////////////////////////////////////////////////////
// Create the filter graph and save the interface pointers for post process
//

HRESULT CreateFilterGraph(TCHAR* url[], FilterGraph *pFG, PerfTestConfig* pConfig)
{
    CheckPointer( url, E_POINTER );
    CheckPointer( pFG, E_POINTER );
    CheckPointer( pConfig, E_POINTER );

	LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);
	
    if ( pConfig->IsAlphaBlend() )
    {
        for ( int i = 0; i < pConfig->GetNumClipIDs(); i++ )
            LOG(_T("%S: %s"), __FUNCTION__, url[i] ? url[i] : TEXT("") );
    }
    else
        LOG(_T("%S: %s"), __FUNCTION__, url[0] ? url[0] : TEXT("") );

	// Create interfaces needed
	HRESULT hr = S_OK;
	IEnumFilters* pEnumFilter = NULL;
    IBaseFilter* pVMR = NULL;
    IVMRFilterConfig *pVMRConfig = NULL;
    IVMRMixerControl *pMixerControl = NULL;

	// Zero out FG
	memset(pFG, 0, sizeof(FilterGraph));

	// Create the graph builder
	if (SUCCEEDED(hr))
	{
		if (FAILED(hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**) &pFG->pGraph)))
			LOG(_T("%S: ERROR %d@%S - CoCreateInstance failed: (%x)") ,__FUNCTION__, __LINE__, __FILE__, hr);
	}

    // BUGBUG: for simplicity, we just render the same clip twice so that we have the same rate for both streams.
    if ( pConfig->IsAlphaBlend() )
    {
	    hr = CoCreateInstance( CLSID_VideoMixingRenderer,
						       NULL, CLSCTX_INPROC,
						       IID_IBaseFilter,
						       (LPVOID *)&pVMR );
        if ( FAILED(hr) )
        {
            LOG(_T("%S: ERROR %d@%S - Create VMR failed: (%x)") ,__FUNCTION__, __LINE__, __FILE__, hr);
            goto error;
        }

        hr = pFG->pGraph->AddFilter( pVMR, L"Video Mixing Renderer" );
        if ( FAILED(hr) )
        {
            LOG(_T("%S: ERROR %d@%S - Add VMR failed: (%x)") ,__FUNCTION__, __LINE__, __FILE__, hr);
            goto error;
        }

        hr = pVMR->QueryInterface( IID_IVMRFilterConfig, (LPVOID *)&pVMRConfig );
        if ( FAILED(hr) )
        {
            LOG(_T("%S: ERROR %d@%S - Query IVMRFilterConfig failed: (%x)") ,__FUNCTION__, __LINE__, __FILE__, hr);
            goto error;
        }
        // render the same clip twice.
        hr = pVMRConfig->SetNumberOfStreams( 2 );
        if ( FAILED(hr) )
        {
            LOG(_T("%S: ERROR %d@%S - SetNumberOfStreams failed: (%x)") ,__FUNCTION__, __LINE__, __FILE__, hr);
            goto error;
        }

	    hr = pVMR->QueryInterface( IID_IVMRMixerControl, (LPVOID *)&pMixerControl );
        if ( FAILED(hr) )
        {
            LOG(_T("%S: ERROR %d@%S - Query IVMRMixerControl failed: (%x)") ,__FUNCTION__, __LINE__, __FILE__, hr);
            goto error;
        }
    }

	//set MaxBackBuffers at this time before the video renderer loads.
	// If max back buffers is specified, set the regkey before adding the VR
	if (SUCCEEDED(hr) && (pConfig->GetMaxbackBuffers()!= -1))
	{
		LOG(TEXT("%S: - setting max back buffers to %d"), __FUNCTION__, pConfig->GetMaxbackBuffers());
		hr = SetMaxVRBackBuffers(pConfig->GetMaxbackBuffers());
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to set the max back buffers %d (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pConfig->GetMaxbackBuffers(), hr);
		}
	}

	// Render the file
    NORMALIZEDRECT rcNormalized;
    rcNormalized.left = 0.25f;
    rcNormalized.top = 0.25f;
    rcNormalized.right = 0.75f;
    rcNormalized.bottom = 0.75f;
	if (SUCCEEDED(hr))
	{
        if ( pConfig->IsAlphaBlend() )
        {
            hr = pFG->pGraph->RenderFile( url[0], NULL);                
            if ( FAILED(hr) )
            {
                LOG(TEXT("%S: ERROR %d@%S - 1st time: renderFile failed for %s: (0x%x)"), __FUNCTION__, __LINE__, __FILE__, url[0], hr);
            }
            hr = pFG->pGraph->RenderFile( url[0], NULL);                
            if ( FAILED(hr) )
            {
                LOG(TEXT("%S: ERROR %d@%S - 2nd time: renderFile failed for %s: (0x%x)"), __FUNCTION__, __LINE__, __FILE__, url[0], hr);
            }
            // Hard code the alpha for every stream.
            hr = pMixerControl->SetAlpha( 0, pConfig->GetAlpha() );
            if ( FAILED(hr) )
            {
                LOG(TEXT("%S: ERROR %d@%S - SetAlpha failed for stream 0: (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            }
            hr = pMixerControl->SetAlpha( 1, pConfig->GetAlpha() );
            if ( FAILED(hr) )
            {
                LOG(TEXT("%S: ERROR %d@%S - SetAlpha failed for stream 1: (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            }
            // Put the 2nd stream at the center.
            hr = pMixerControl->SetOutputRect( 1, &rcNormalized );
            if ( FAILED(hr) )
            {
                LOG(TEXT("%S: ERROR %d@%S - SetOutputRect failed for stream 1: (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            }
        }
        else
        {
		    if (FAILED(hr = pFG->pGraph->RenderFile(url[0], NULL)))
			    LOG(TEXT("%S: ERROR %d@%S - RenderFile failed: (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        }
	}

	//set VRMode 
	if (SUCCEEDED(hr))
	{
		if (pConfig->GetVRMode()== -1)
		{
			// No need to set the mode, the VR will pick the mode on it's own
		}
		else {
			LOG(TEXT("%S: - setting video renderer mode (0x%x)"), __FUNCTION__, pConfig->GetVRMode());
			hr = SetVideoRendererMode(pFG, pConfig->GetVRMode());
			if (FAILED(hr))
			{
				LOG(TEXT("%S: ERROR %d@%S - failed to set the video renderer mode(0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
			}
		}
	}


	// Get the media event interface
	if (SUCCEEDED(hr))
	{
		if (FAILED(hr = pFG->pGraph->QueryInterface(IID_IMediaEvent, (void**) &pFG->pMediaEvent)))
			LOG(TEXT("%S: ERROR %d@%S - IMediaEvent query failed(0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
	}
	
	// Get the media control interface
	if (SUCCEEDED(hr))
	{
		if (FAILED(hr = pFG->pGraph->QueryInterface(IID_IMediaControl, (void**) &pFG->pMediaControl)))
			LOG(TEXT("%S: ERROR %d@%S - IMediaControl query failed(0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
	}

	// Check the seeking interface
	if (SUCCEEDED(hr))
	{
		if (FAILED(hr = pFG->pGraph->QueryInterface(IID_IMediaSeeking, (void**) &pFG->pMediaSeeking)))
			LOG(TEXT("%S: ERROR %d@%S - IMediaSeeking query failed(0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);			
	}

	// Above interfaces are mandatory: remaining are not
	if (SUCCEEDED(hr))
	{
		if (FAILED((hr = FindQualProp(IID_IBasicVideo, pFG->pGraph, &pFG->pQualProp))))
			LOG(TEXT("%S: WARNING %d@%S - could not find IQualProp interface(0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);			
		if (FAILED((hr = FindDroppedFrames(IID_IAMDroppedFrames, pFG->pGraph,  &pFG->pDroppedFrames))))
			LOG(TEXT("%S: WARNING %d@%S - could not find IAMDroppedFrames interface(0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);						
		if (FAILED((hr = FindAudioGlitch(IID_IAMAudioRendererStats, pFG->pGraph, &pFG->pAudioRendererStats))))
			LOG(TEXT("%S: WARNING %d@%S - could not find IAMAudioRendererStats interface(0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);			
		if (FAILED((hr = pFG->pGraph->EnumFilters( &pEnumFilter))))
			LOG(TEXT("%S: WARNING %d@%S - could not find filter enumerato(0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);						
		else {
			if (FAILED((hr = FindInterfaceOnGraph( pFG->pGraph, IID_IAMNetworkStatus, (void **)&pFG->pNetworkStatus, pEnumFilter, TRUE ))))
				LOG(TEXT("%S: WARNING %d@%S - could not find IAMNetworkStatus interface(0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);						
		}
		if (pEnumFilter)
			pEnumFilter->Release();
		hr = S_OK;
	}

	//clean up if something went wrong
	if (FAILED(hr))
	{
		if (pFG->pNetworkStatus) {
			pFG->pNetworkStatus->Release();
			pFG->pNetworkStatus = NULL;
		}
		if (pFG->pDroppedFrames) {
			pFG->pDroppedFrames->Release();
			pFG->pDroppedFrames = NULL;
		}
		if (pFG->pAudioRendererStats) {
			pFG->pAudioRendererStats->Release();
			pFG->pAudioRendererStats = NULL;
		}
		if (pFG->pQualProp){
			pFG->pQualProp->Release();
			pFG->pQualProp = NULL;
		}
		if (pFG->pMediaSeeking){
			pFG->pMediaSeeking->Release();
			pFG->pMediaSeeking = NULL;
		}
		if (pFG->pMediaEvent){
			pFG->pMediaEvent->Release();
			pFG->pMediaEvent = NULL;
		}
		if (pFG->pMediaControl) {
			pFG->pMediaControl->Release();
			pFG->pMediaControl = NULL;
		}
		if (pFG->pGraph) {
			pFG->pGraph->Release();
			pFG->pGraph = NULL;
		}
		
	}
	else {
        StringCchCopy( pFG->url, MAX_PATH, url[0] );
	}

error:
    // release VMR interfaces
    if ( pVMR )
        pVMR->Release();
    if ( pVMRConfig )
        pVMRConfig->Release();
    if ( pMixerControl )
        pMixerControl->Release();

	LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
	
	return hr;
}

void ReleaseFilterGraph(FilterGraph *pFG)
{
	LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);
	LOG(_T("%S: %s"), __FUNCTION__, pFG->url);
	if (pFG)
	{
		if (pFG->m_pVRMode)
			pFG->m_pVRMode->Release();
		if (pFG->pNetworkStatus)
			pFG->pNetworkStatus->Release();
		if (pFG->pDroppedFrames)
			pFG->pDroppedFrames->Release();
		if (pFG->pAudioRendererStats)
			pFG->pAudioRendererStats->Release();
		if (pFG->pQualProp)
			pFG->pQualProp->Release();
		if (pFG->pMediaSeeking)
			pFG->pMediaSeeking->Release();
		if (pFG->pMediaEvent)
			pFG->pMediaEvent->Release();
		if (pFG->pMediaControl)
			pFG->pMediaControl->Release();
		if (pFG->pGraph)
			pFG->pGraph->Release();
	}
	
	LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
}

HRESULT FindInterfaceOnGraph(IGraphBuilder *pFilterGraph, REFIID riid, void **ppInterface, 
									IEnumFilters* pEnumFilter, BOOL begin )
{
    HRESULT hr = E_NOINTERFACE;
    IBaseFilter*   pFilter;
    ULONG ulFetched = 0;

    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);
    
    if(!ppInterface)
        return E_FAIL;
    *ppInterface= NULL;
    
    hr = E_NOINTERFACE;
    
    if( begin )
		pEnumFilter->Reset();
    
    // find the first filter in the graph that supports riid interface
    while(!*ppInterface && pEnumFilter->Next(1, &pFilter, NULL) == S_OK)
    {
        hr = pFilter->QueryInterface(riid, ppInterface);
        pFilter->Release();
    }

    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
	
    return hr;
}

HRESULT FindQualProp(REFIID refiid, IGraphBuilder* pGraph, IQualProp** pQualProp)
{
    HRESULT hr = E_NOINTERFACE;
    IBaseFilter *pBF = NULL;
    IEnumFilters* pEnumFilter = NULL;

    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);
    
    pGraph->EnumFilters( &pEnumFilter );

    if( FindInterfaceOnGraph( pGraph, refiid, (void **)&pBF, pEnumFilter, TRUE ) == NOERROR )
    {
        // Get the QualProp from the renderer which matches our requested type
        if(refiid == IID_IBasicAudio || refiid == IID_IBasicVideo)
        {
            hr = pBF->QueryInterface(IID_IQualProp, (void **)pQualProp);
        }
        pBF->Release();
    }

    if(NULL != pEnumFilter)
        pEnumFilter->Release();

    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    return hr;
}

HRESULT FindDroppedFrames(REFIID refiid, IGraphBuilder* pGraph, IAMDroppedFrames** pDroppedFrames)
{
    HRESULT hr = E_NOINTERFACE;
    IBaseFilter *pBF = NULL;
    IEnumFilters* pEnumFilter = NULL;

    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);
	
    pGraph->EnumFilters( &pEnumFilter );

    if( FindInterfaceOnGraph( pGraph, refiid, (void **)&pBF, pEnumFilter, TRUE ) == NOERROR )
    {
        // Get the IAMDroppedFrames interface from the renderer which matches our requested type
        if(refiid == IID_IAMDroppedFrames)
        {
            hr = pBF->QueryInterface(IID_IAMDroppedFrames, (void **)pDroppedFrames);
        }
        pBF->Release();
    }

    if(NULL != pEnumFilter)
        pEnumFilter->Release();
	
    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
	
    return hr;
}


HRESULT FindAudioGlitch(REFIID refiid, IGraphBuilder* pGraph, IAMAudioRendererStats** pDroppedFrames)
{
    HRESULT hr = E_NOINTERFACE;
    IBaseFilter *pBF = NULL;
    IEnumFilters* pEnumFilter = NULL;

    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);	
    
    pGraph->EnumFilters( &pEnumFilter );

    if( FindInterfaceOnGraph( pGraph, refiid, (void **)&pBF, pEnumFilter, TRUE ) == NOERROR )
    {
        // Get the IAMAudioRendererStats from the renderer which matches our requested type
        if(refiid == IID_IAMAudioRendererStats)
        {
            hr = pBF->QueryInterface(IID_IAMAudioRendererStats, (void **)pDroppedFrames);
        }
        pBF->Release();
    }

    if(NULL != pEnumFilter)
        pEnumFilter->Release();

    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    return hr;
}

string LocationToPlay(const TCHAR* szProtocolName,  const CMediaContent* pMediaContent)
{
	string retValue = "";

	LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

	if(0 == _wcsnicmp(szProtocolName, PROTO_LOCAL, MAX_PATH))
	{
		retValue = pMediaContent->m_Locations.m_Local;
	} 
	else if(0 == _wcsnicmp(szProtocolName, PROTO_HTTP, MAX_PATH))
	{
		retValue = pMediaContent->m_Locations.m_HTTP;
	}
	else if(0 == _wcsnicmp(szProtocolName, PROTO_MMS, MAX_PATH))
	{
		retValue = pMediaContent->m_Locations.m_MMS;
	}
	else if(0 == _wcsnicmp(szProtocolName, PROTO_MMSU, MAX_PATH))
	{
		retValue = pMediaContent->m_Locations.m_MMSU;
	}
	else if(0 == _wcsnicmp(szProtocolName, PROTO_MMST, MAX_PATH))
	{
		retValue = pMediaContent->m_Locations.m_MMST;
	}
	else
	{
		retValue = "";
	}

	if (retValue != "")
		retValue	= retValue +  (pMediaContent->m_bDRM ? pMediaContent->m_DRMClipName : pMediaContent->m_ClipName);

	LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);

	return retValue;
}

void LocationToPlay(TCHAR* fileName, const TCHAR* szProtocolName,  const CMediaContent* pMediaContent)
{
	LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);
	
	string location = LocationToPlay(szProtocolName, pMediaContent);
	MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, location.c_str(), -1, fileName, MAX_PATH);

	LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
}

bool EnableBWSwitch(bool bwswitch)
{
	HKEY hKey;
	DWORD disposition;
	LONG result = ERROR_SUCCESS;

	LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

	LOGZONES(_T("%S: bwswitch = %d"), __FUNCTION__, bwswitch);	
	
	if (ERROR_SUCCESS != (result = RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\NetShow\\Player"),
					0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &disposition)))
	{
		LOGZONES(TEXT("%S: WARNING %d@%S - Open key failed: (0x%x))"), __FUNCTION__, __LINE__, __FILE__, result);	
		return false;
	}

	// Increase the min receive buffer to a large enough size for the tests to run.
	DWORD value = !bwswitch;

	if (ERROR_SUCCESS != (result = RegSetValueEx(hKey, _T("disablebandwidthswitch"), 0, REG_DWORD, (const BYTE*)&value, sizeof(value))))
	{
		LOGZONES(TEXT("%S: WARNING %d@%S - Set key failed: (0x%x))"), __FUNCTION__, __LINE__, __FILE__, result);	
		return false;
	}

	RegCloseKey(hKey);

	LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);	

	return 0;
}

/**
  * Finds tokens destructively, replacing the delimiter with a '\0'.
  */
TCHAR* GetNextToken(TCHAR* string, int* pos, TCHAR delimiter)
{
	LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);
	
	if (*pos == -1)
		return NULL;

	TCHAR* pStart = string + *pos;
	TCHAR* pEnd = _tcschr(pStart, delimiter);
	if (pEnd)
	{
		*pos = pEnd - string + 1;
		*pEnd = TEXT('\0');
	}
	else {
		*pos = -1;
	}

	LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
	
	return pStart;
}

HRESULT SetMaxVRBackBuffers(int dwMaxBackBuffers)
{
    HRESULT hr = S_OK;
    HKEY hKey;
    DWORD disposition;
    LONG result = ERROR_SUCCESS;

    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    //get to the correct registry key	
    if (ERROR_SUCCESS != (result = RegCreateKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\DirectX\\DirectShow\\Video Renderer"), 
                    0, NULL, 0, 0, NULL, &hKey, &disposition)))
    {
        LOG(TEXT("%S: ERROR %d@%S - opening key failed (0x%x)"), __FUNCTION__, __LINE__, __FILE__, result);
        return E_FAIL;
    }

    // Set the flag
    if (ERROR_SUCCESS != (result = RegSetValueEx(hKey, _T("MaxBackBuffers"), 0, REG_DWORD, (const BYTE*)&dwMaxBackBuffers, sizeof(dwMaxBackBuffers))))
    {
        LOG(TEXT("%S: ERROR %d@%S - setting max back buffers reg key failed (0x%x)"), __FUNCTION__, __LINE__, __FILE__, result);    
        hr = E_FAIL;
    }

    RegCloseKey(hKey);

    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    return hr;
}

HRESULT AcquireVRModeIface(FilterGraph *pFG)
{

	LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

	// Get the interface for setting renderer mode
	HRESULT hr = S_OK;
	IEnumFilters* pEnumFilter = NULL;

	IGraphBuilder* pGraph = pFG->pGraph;

	//get a list of all filters in the graph
	pGraph->EnumFilters( &pEnumFilter );

	//find out if any of the filters in the graph expose this interface
	if(FindInterfaceOnGraph( pGraph, IID_IAMVideoRendererMode, (void **)&(pFG->m_pVRMode), pEnumFilter, TRUE ) != NOERROR )
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to QueryInterface for IAMVideoRendererMode (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
		hr = E_NOINTERFACE;
	}

	//make sure we release the enumeration
	if(NULL != pEnumFilter)
		pEnumFilter->Release();

	LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);

	return hr;
}


HRESULT SetVideoRendererMode(FilterGraph *pFG, DWORD vrMode)
{
	HRESULT hr = S_OK;

	LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

	//get the interface we need to set renderer mode
	hr = AcquireVRModeIface(pFG);
	if (FAILED(hr)) {
		LOG(TEXT("%S: ERROR %d@%S - Call to AcquireVRModeIface() failed with (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
	}
	
	if(SUCCEEDED(hr)) {

		//call into the renderer and set the mode
		hr = pFG->m_pVRMode->SetMode(vrMode);
		if(FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - SetMode() failed with (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
	}

	LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);

	return hr;
}


