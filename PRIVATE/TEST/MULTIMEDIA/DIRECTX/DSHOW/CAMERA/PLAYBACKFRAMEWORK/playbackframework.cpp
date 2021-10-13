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
#include <windows.h>
#include "logging.h"
#include "PlaybackFramework.h"
#include "clparse.h"
#include <bldver.h>

//#define OUTPUT_EVENTS

CPlaybackFramework::CPlaybackFramework( )
{
    m_bCoInitialized = FALSE;
    m_bInitialized = FALSE;

    m_pGraph = NULL;
    m_pMediaControl = NULL;
    m_pMediaSeeking = NULL;
    m_pMediaEvent = NULL;
    m_pVideoRenderer = NULL;
    m_pVideoRendererMode = NULL;
    m_pVideoWindow = NULL;
    m_pVideoRendererBasicVideo = NULL;
}

CPlaybackFramework::~CPlaybackFramework( )
{
    Cleanup();
}

HRESULT
CPlaybackFramework::Cleanup()
{
    HRESULT hr = S_OK;

    if(m_bCoInitialized)
    {
        // we must be initialized to clear the media events.
        ClearMediaEvents();

        StopGraph();

        // if the video window is available, clear out it's window handle before cleanup
        // to prevent invalid use of the window after cleanup.
        if(m_pVideoWindow)
        {
            // Clear the owner window for the video window
            // this may fail depending on the owners and such, so ignore any failures.
            m_pVideoWindow->put_Owner((OAHWND) NULL);
        }

        // release all of the components
        m_pGraph = NULL;
        m_pMediaControl = NULL;
        m_pMediaSeeking = NULL;
        m_pMediaEvent = NULL;
        m_pVideoRenderer = NULL;
        m_pVideoRendererMode = NULL;
        m_pVideoWindow = NULL;
        m_pVideoRendererBasicVideo = NULL;
        // uninitialize
        CoUninitialize();

        m_bCoInitialized = FALSE;
    }

    m_bInitialized = FALSE;

    return hr;
}

void
CPlaybackFramework::OutputRuntimeParameters()
{
    Log(TEXT("*** Test runtime parameteters *** "));

    Log(TEXT("    Playback framework version %d.%d.%d.%d"), CE_MAJOR_VER, CE_MINOR_VER, CE_BUILD_VER, PLAYBACK_FRAMEWORK_VERSION);
}

BOOL
CPlaybackFramework::ParseCommandLine(LPCTSTR tszCommandLine)
{
    class CClParse cCLPparser(tszCommandLine);
    BOOL bReturnValue = TRUE;

    // if no command line was given, then there's nothing for us to do.
    if(!tszCommandLine)
    {
        return FALSE;
    }
    else if(cCLPparser.GetOpt(TEXT("?")))
    {
        Log(TEXT(""));

        return FALSE;
    }
    else
    {
        // parse
    }
    return bReturnValue;
}

HRESULT
CPlaybackFramework::Init(HWND OwnerWnd, RECT *rcCoordinates, TCHAR *tszFileName)
{
    HRESULT hr = S_OK;

    if(!OwnerWnd)
    {
        FAIL(TEXT("CPlaybackFramework: Invalid owner window given."));
        hr = E_FAIL;
        goto cleanup;
    }

    if(!tszFileName)
    {
        FAIL(TEXT("CPlaybackFramework: Invalid filename given."));
        hr = E_FAIL;
        goto cleanup;
    }

    if(FAILED(hr = CoInitialize(NULL)))
    {
        FAIL(TEXT("CPlaybackFramework: Failed to CoInitialize."));
        goto cleanup;
    }

    m_bCoInitialized = TRUE;

    hr = CoCreateInstance( CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**) &m_pGraph );
    if(FAILED(hr) || m_pGraph == NULL)
    {
        FAIL(TEXT("CPlaybackFramework: CoCreateInstance of the filter graph failed."));
        goto cleanup;
    }

    // these components always exist.
    hr = m_pGraph.QueryInterface( &m_pMediaControl );
    if(FAILED(hr) || m_pMediaControl == NULL)
    {
        FAIL(TEXT("CPlaybackFramework: Querying for the IMedia control failed."));
        goto cleanup;
    }

    hr = m_pGraph.QueryInterface( &m_pMediaEvent );
    if(FAILED(hr) || m_pMediaEvent == NULL)
    {
        FAIL(TEXT("CPlaybackFramework: Querying for the IMediaEvent failed."));
        goto cleanup;
    }

    hr = m_pGraph.QueryInterface( &m_pMediaSeeking );
    if(FAILED(hr) || m_pMediaSeeking == NULL)
    {
        FAIL(TEXT("CPlaybackFramework: Querying for the m_pMediaSeeking failed."));
        goto cleanup;
    }

    // renderfile
    hr = m_pGraph->RenderFile(tszFileName, NULL);
    if(FAILED(hr))
    {
        FAIL(TEXT("CPlaybackFramework: Failed to render the file."));
        goto cleanup;
    }

    // if the video renderer isn't found, it may be because there is no video so 
    // do nothing with it.
    m_pGraph->FindFilterByName(TEXT("Video Renderer"), &m_pVideoRenderer);

    if(m_pVideoRenderer)
    {
        hr = m_pVideoRenderer.QueryInterface( &m_pVideoWindow );
        if(FAILED(hr) || m_pVideoWindow == NULL)
        {
            FAIL(TEXT("CPlaybackFramework: Retrieving the IVideoWindow failed."));
            goto cleanup;
        }

        hr = m_pVideoRenderer.QueryInterface( &m_pVideoRendererMode );
        if(FAILED(hr) || m_pVideoRendererMode == NULL)
        {
            FAIL(TEXT("CPlaybackFramework: Unable to retrieve the video renderer mode."));
            goto cleanup;
        }

        hr = m_pVideoRenderer.QueryInterface( &m_pVideoRendererBasicVideo );
        if(FAILED(hr) || m_pVideoRendererBasicVideo == NULL)
        {
            FAIL(TEXT("CPlaybackFramework: Unable to retrieve the video renderer BasicVideo interface."));
            goto cleanup;
        }

        // Set the video window and it's position.
        hr = m_pVideoWindow->put_Owner((OAHWND) OwnerWnd);
        if(FAILED(hr))
        {
            DETAIL(TEXT("CPlaybackFramework: Setting the owner window failed."));
        }

        if(rcCoordinates)
        {
            hr = SetVideoWindowPosition(rcCoordinates);
            if(FAILED(hr))
            {
                DETAIL(TEXT("CPlaybackFramework: Setting the window position failed."));
            }
        }
    }

cleanup:
    return hr;
}

HRESULT
CPlaybackFramework::RunGraph()
{
    HRESULT hr = S_OK;
    OAFilterState fsBefore;
    OAFilterState fsAfter;

    if(m_pMediaControl == NULL)
    {
        FAIL(TEXT("CPlaybackFramework: Core components needed for RunGraph unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = m_pMediaControl->GetState(PLAYBACKMEDIAEVENT_TIMEOUT, &fsBefore);
    if(FAILED(hr))
    {
        FAIL(TEXT("CPlaybackFramework: Retrieving the graph state failed."));
        goto cleanup;
    }

    // BUGBUG: 107737 - telling a running graph to run again breaks the graph time
    if(fsBefore != State_Running)
    {
        hr = m_pMediaControl->Run();
        if(FAILED(hr))
        {
            DETAIL(TEXT("CPlaybackFramework: Starting the graph failed."));
            goto cleanup;
        }
    }

    hr = m_pMediaControl->GetState(PLAYBACKMEDIAEVENT_TIMEOUT, &fsAfter);
    if(FAILED(hr))
    {
        FAIL(TEXT("CPlaybackFramework: Retrieving the graph state failed."));
        goto cleanup;
    }

    if(fsAfter != State_Running)
    {
        FAIL(TEXT("CPlaybackFramework: Failed to change the filter state."));
        goto cleanup;
    }

cleanup:
    return hr;
}

HRESULT
CPlaybackFramework::PauseGraph()
{
    HRESULT hr = S_OK;

    if(m_pMediaControl == NULL)
    {
        FAIL(TEXT("CPlaybackFramework: Core components needed for PauseGraph unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = m_pMediaControl->Pause();
    if(FAILED(hr))
    {
        DETAIL(TEXT("CPlaybackFramework: Pausing the graph failed."));
        goto cleanup;
    }

    OAFilterState fs;
    hr = m_pMediaControl->GetState(PLAYBACKMEDIAEVENT_TIMEOUT, &fs);
    if(FAILED(hr))
    {
        FAIL(TEXT("CPlaybackFramework: Retrieving the graph state failed."));
        goto cleanup;
    }

    if(fs != State_Paused)
    {
        FAIL(TEXT("CPlaybackFramework: Failed to change the filter state."));
        goto cleanup;
    }

cleanup:
    return hr;
}

HRESULT
CPlaybackFramework::StopGraph()
{
    HRESULT hr = S_OK;

    if(m_pMediaControl == NULL)
    {
        FAIL(TEXT("CPlaybackFramework: Core components needed for StopGraph unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    // if something fails when stopping the graph, and we weren't initialized, then it's not necessarily a failure.
    // if we were initialized, then everything should succeed.

    hr = m_pMediaControl->Stop();
    if(FAILED(hr) && m_bInitialized)
    {
        FAIL(TEXT("CPlaybackFramework: Stopping the graph failed."));
        goto cleanup;
    }

    OAFilterState fs;
    hr = m_pMediaControl->GetState(PLAYBACKMEDIAEVENT_TIMEOUT, &fs);
    if(FAILED(hr) && m_bInitialized)
    {
        FAIL(TEXT("CPlaybackFramework: Retrieving the graph state failed."));
        goto cleanup;
    }

    if(fs != State_Stopped && m_bInitialized)
    {
        FAIL(TEXT("CPlaybackFramework: Failed to change the filter state."));
        goto cleanup;
    }

cleanup:
    return hr;
}

HRESULT
CPlaybackFramework::SetRate(double dRate)
{
    if(m_pMediaSeeking == NULL)
    {
        FAIL(TEXT("CPlaybackFramework: Core components needed for SetRate unavailable."));
        return E_FAIL;
    }

    return m_pMediaSeeking->SetRate(dRate);
}

HRESULT
CPlaybackFramework::GetMediaEvent(LONG *lEventCode, LONG *lParam1, LONG *lParam2, long lEventTimeout)
{
    HRESULT hrEvent;

    if(m_pMediaEvent == NULL)
    {
        FAIL(TEXT("CPlaybackFramework: Core components needed for GetMediaEvent unavailable."));
        return E_FAIL;
    }

    hrEvent =  m_pMediaEvent->GetEvent( lEventCode, lParam1, lParam2, lEventTimeout );

#ifdef OUTPUT_EVENTS
    if(SUCCEEDED(hrEvent))
        Log(TEXT("Received media event 0x%08x, param1 0x%08x, param2 0x%08x."), *lEventCode, *lParam1, *lParam2);
#endif

    return hrEvent;
}

HRESULT CPlaybackFramework::FreeMediaEvent(LONG lEventCode, LONG lParam1, LONG lParam2)
{
    HRESULT hrEvent;

    hrEvent = m_pMediaEvent->FreeEventParams( lEventCode, lParam1, lParam2 );

    return hrEvent;
}

HRESULT
CPlaybackFramework::ClearMediaEvents()
{
    HRESULT hr = S_OK;
    LONG lEventCode = 0, lParam1 = 0, lParam2 = 0;

    if(m_pMediaEvent == NULL)
    {
        FAIL(TEXT("CPlaybackFramework: Core components needed for ClearMediaEvents unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    while(SUCCEEDED(m_pMediaEvent->GetEvent( &lEventCode, &lParam1, &lParam2, 0 )))
    {
#ifdef OUTPUT_EVENTS
        Log(TEXT("Clearing media event 0x%08x, param1 0x%08x, param2 0x%08x."), lEventCode, lParam1, lParam2);
#endif
        hr = m_pMediaEvent->FreeEventParams( lEventCode, lParam1, lParam2 );
    }

cleanup:
    return hr;
}

HRESULT
CPlaybackFramework::WaitForCompletion(long lEventTimeout, long *lEvCode)
{
    if(m_pMediaEvent == NULL)
    {
        FAIL(TEXT("CPlaybackFramework: Core components needed for WaitForCompletion unavailable."));
        return E_FAIL;
    }

    return m_pMediaEvent->WaitForCompletion(lEventTimeout, lEvCode);
}

HRESULT
CPlaybackFramework::FindFilterByName(TCHAR *Name, IBaseFilter **ppFilter)
{
    if(m_pGraph == NULL)
    {
        FAIL(TEXT("CPlaybackFramework: Core components needed for FindFilterByName unavailable."));
        return E_FAIL;
    }

    return m_pGraph->FindFilterByName(Name, ppFilter);
}

HRESULT
CPlaybackFramework::SetVideoWindowPosition(RECT *rcCoordinates)
{
    HRESULT hr = S_OK;

    // If there's no video renderer in the graph, then fail the cal.
    if(m_pVideoRenderer == NULL)
    {
        hr = VFW_E_NOT_FOUND;
        goto cleanup;
    }

    if(m_pVideoWindow == NULL)
    {
        FAIL(TEXT("CPlaybackFramework: Core components needed for SetVideoWindowPosition unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    if(!rcCoordinates)
    {
        FAIL(TEXT("CPlaybackFramework: Invalid rectangle pointer given."));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = m_pVideoWindow->SetWindowPosition(rcCoordinates->left, rcCoordinates->top, rcCoordinates->right - rcCoordinates->left, rcCoordinates->bottom - rcCoordinates->top);
    if(FAILED(hr))
    {
        FAIL(TEXT("CPlaybackFramework: Setting the window position failed."));
        goto cleanup;
    }

cleanup:
    return hr;
}


HRESULT
CPlaybackFramework::GetVideoWindowPosition(RECT *rcCoordinates)
{
    HRESULT hr = S_OK;

    if(m_pVideoRenderer == NULL)
    {
        hr = VFW_E_NOT_FOUND;
        goto cleanup;
    }

    if(m_pVideoWindow == NULL)
    {
        FAIL(TEXT("CPlaybackFramework: Core components needed for SetVideoWindowPosition unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    if(!rcCoordinates)
    {
        FAIL(TEXT("CPlaybackFramework: Invalid rectangle pointer given."));
        hr = E_FAIL;
        goto cleanup;
    }

    // now that we've successfully moved the preview, update our internal location rectangle
    hr = m_pVideoWindow->GetWindowPosition(&rcCoordinates->left, &rcCoordinates->top, &rcCoordinates->right, &rcCoordinates->bottom);
    if(FAILED(hr))
    {
        DETAIL(TEXT("CPlaybackFramework: Setting the window position failed."));
    }
    rcCoordinates->right += rcCoordinates->left;
    rcCoordinates->bottom += rcCoordinates->top;

cleanup:
    return hr;
}

HRESULT
CPlaybackFramework::GetVideoRenderMode(DWORD *dwMode)
{
    HRESULT hr = S_OK;

    if(m_pVideoRenderer == NULL)
    {
        hr = VFW_E_NOT_FOUND;
        goto cleanup;
    }

    if(m_pVideoRendererMode == NULL)
    {
        FAIL(TEXT("CPlaybackFramework: Core components required for GetVideoRenderMode unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    if(NULL == dwMode)
    {
        FAIL(TEXT("CPlaybackFramework: Invalid parameter."));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = m_pVideoRendererMode->GetMode(dwMode);
    if(FAILED(hr))
    {
        FAIL(TEXT("CPlaybackFramework: Unable to retrieve the video renderer mode."));
        goto cleanup;
    }

cleanup:
    return hr;
}

HRESULT
CPlaybackFramework::SetVideoRenderMode(DWORD dwMode)
{
    HRESULT hr = S_OK;

    if(m_pVideoRenderer == NULL)
    {
        hr = VFW_E_NOT_FOUND;
        goto cleanup;
    }

    if (m_pVideoRendererMode == NULL)
    {
        FAIL(TEXT("CPlaybackFramework: Core components required for SetVideoRenderMode unavailable."));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = m_pVideoRendererMode->SetMode(dwMode);
    if(FAILED(hr))
    {
        DETAIL(TEXT("CPlaybackFramework: Unable to set the video renderer mode."));
        goto cleanup;
    }

cleanup:
    return hr;
}

HRESULT
CPlaybackFramework::GetVideoRenderStats(PLAYBACKVIDEORENDERER_STATS *statData)
{
    HRESULT hr = S_OK;
    REFTIME AvgTimeperFrame = 0;
    CComPtr<IQualProp> pQP = NULL;
    int iTemp;

    if(m_pVideoRenderer == NULL)
    {
        hr = VFW_E_NOT_FOUND;
        goto cleanup;
    }

    if(m_pVideoRendererBasicVideo == NULL)
    {
        FAIL(TEXT("GetVideoRenderStats:  VideoRendererBasicVideo is NULL"));
        hr = E_FAIL;
        goto cleanup;
    }

    if (statData)
    {
        statData->dFrameRate     = 0.0;
        statData->dActualRate    = 0.0;
        statData->lFramesDropped = 0;
        statData->lSourceWidth = 0;
        statData->lSourceHeight = 0;
        statData->lDestWidth = 0;
        statData->lDestHeight = 0;
    }
   
    hr = m_pVideoRendererBasicVideo->get_AvgTimePerFrame(&AvgTimeperFrame);
    if(FAILED(hr))
    {
        FAIL(TEXT("GetVideoRenderStats: get_AvgTimePerFrame failed."));
    }

    hr = m_pVideoRendererBasicVideo->get_SourceWidth(&statData->lSourceWidth);
    if(FAILED(hr))
    {
        FAIL(TEXT("GetVideoRenderStats: get_SourceWidth failed."));
    }

    hr = m_pVideoRendererBasicVideo->get_SourceHeight(&statData->lSourceHeight);
    if(FAILED(hr))
    {
        FAIL(TEXT("GetVideoRenderStats: get_SourceHeight failed."));
    }

    hr = m_pVideoRendererBasicVideo->get_DestinationWidth(&statData->lDestWidth);
    if(FAILED(hr))
    {
        FAIL(TEXT("GetVideoRenderStats: get_DestinationWidth failed."));
    }

    hr = m_pVideoRendererBasicVideo->get_DestinationHeight(&statData->lDestHeight);
    if(FAILED(hr))
    {
        FAIL(TEXT("GetVideoRenderStats: get_DestinationHeight failed."));
    }

    if (AvgTimeperFrame)
    {
        if (statData)
            statData->dFrameRate = (1.0/AvgTimeperFrame);
    }

    // no GUID associated in atlbase.h
    hr = m_pVideoRendererBasicVideo->QueryInterface(IID_IQualProp, (void**) &pQP);
    if(FAILED(hr))
    {
        FAIL(TEXT("GetVideoRenderStats: Querying for the IQualProp control failed."));
        goto cleanup;
    }
                
    pQP->get_FramesDroppedInRenderer(&iTemp);

    if (statData)    
        statData->lFramesDropped = iTemp;
          
    pQP->get_AvgFrameRate(&iTemp);
    if (statData)    
        statData->dActualRate = iTemp / 100.0;

    pQP->get_AvgSyncOffset(&iTemp);
    if (statData)    
        statData->lAvgSyncOffset = iTemp;

    pQP->get_DevSyncOffset(&iTemp);
    if (statData)    
        statData->lDevSyncOffset = iTemp;

    pQP->get_FramesDrawn(&iTemp);
    if (statData)    
        statData->lFramesDrawn = iTemp;

    pQP->get_Jitter(&iTemp);
    if (statData)    
        statData->lJitter = iTemp;

cleanup:
    return hr;
}

HRESULT
CPlaybackFramework::SwitchVideoRenderMode()
{
    HRESULT hr = S_OK;

    LONG BufferSize = 0;
    LONG *pDIBImage = NULL;
    DWORD dwModeSet = AM_VIDEO_RENDERER_MODE_GDI, dwModeReturned;

    if(m_pVideoRenderer == NULL)
    {
        hr = VFW_E_NOT_FOUND;
        goto cleanup;
    }

    hr = GetVideoRenderMode(&dwModeReturned);

    if(FAILED(hr))
        goto cleanup;
    else if(dwModeReturned == AM_VIDEO_RENDERER_MODE_DDRAW)
    {
        DETAIL(TEXT("CPlaybackFramework: Using ddraw, switching to GDI."));
        dwModeSet = AM_VIDEO_RENDERER_MODE_GDI;
    }
    else if(dwModeReturned == AM_VIDEO_RENDERER_MODE_GDI)
    {
        DETAIL(TEXT("CPlaybackFramework: Using gdi, switching to DDraw."));
        dwModeSet = AM_VIDEO_RENDERER_MODE_DDRAW;
    }

    hr = SetVideoRenderMode(dwModeSet);
    if(FAILED(hr))
        goto cleanup;

    hr =GetVideoRenderMode(&dwModeReturned);
    if(FAILED(hr))
        goto cleanup;

    if(dwModeSet != dwModeReturned)
    {
        FAIL(TEXT("CPlaybackFramework: The mode set doesn't match the mode returned after setting."));
        hr = E_FAIL;
        goto cleanup;
    }

cleanup:
    return hr;
}

HRESULT
CPlaybackFramework::GetScreenOrientation(DWORD *dwOrientation)
{
    DEVMODE dm;

    *dwOrientation = 0;

    memset(&dm, 0, sizeof(DEVMODE));
    dm.dmSize = sizeof(DEVMODE);
    dm.dmFields = DM_DISPLAYORIENTATION;

    if(DISP_CHANGE_SUCCESSFUL == ChangeDisplaySettingsEx(NULL, &dm, NULL, CDS_TEST, NULL))
    {
        *dwOrientation = dm.dmDisplayOrientation;
    }

    return S_OK;
}

HRESULT
CPlaybackFramework::SetScreenOrientation(DWORD dwOrientation)
{
    DEVMODE dm;
    HRESULT hr = E_FAIL;

    memset(&dm, 0, sizeof(DEVMODE));
    dm.dmSize = sizeof(DEVMODE);
    dm.dmFields = DM_DISPLAYORIENTATION;
    dm.dmDisplayOrientation = dwOrientation;

    if(DISP_CHANGE_SUCCESSFUL == ChangeDisplaySettingsEx(NULL, &dm, NULL, 0, NULL))
    {
        hr = S_OK;
    }

    return hr;
}

