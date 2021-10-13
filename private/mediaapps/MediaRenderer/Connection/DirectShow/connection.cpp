//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//


#include "stdafx.h"
#include "control.h"
#include "RenderMp2Ts.h"

CConnection::CConnection()
{
    m_pProtocolInfo = NULL;
    m_pStatus       = NULL;
    m_Volume        = MAX_VOLUME_RANGE * 2 / 3;
    m_pITCB         = NULL;
}


CConnection::~CConnection()
{
    Stop();
    Cleanup();

    if ( m_hShutdownEvent.valid() )
    {
        SetEvent(m_hShutdownEvent);
    }

    if ( m_hEventThread.valid() )
    {
        WaitForSingleObject(m_hEventThread, INFINITE);
    }
}

HRESULT 
CConnection::PrepareEvent(ITransportCallback *pITCB)
{
    ce::gate<ce::critical_section> _gate(m_cs);
    HRESULT hr = S_OK;

    ChkBool(pITCB != 0, E_POINTER);
    m_pITCB = pITCB;

    if ( !m_hEventThread.valid() )
    {
        // Prepare the shutdown event if necessary
        if ( !m_hStartEvent.valid() )
        {
            m_hShutdownEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        }
        ChkBool( m_hShutdownEvent.valid(), E_FAIL );

        // Prepare the start event if necessary
        if ( !m_hStartEvent.valid() )
        {
            m_hStartEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        }
        ChkBool( m_hStartEvent.valid(), E_FAIL );

        // Prepare the stop event if necessary
        if ( !m_hStopEvent.valid() )
        {
            m_hStopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        }
        ChkBool( m_hStopEvent.valid(), E_FAIL );

        // Prepare the EOS thread if necessary
        if ( !m_hEventThread.valid() )
        {
            m_hEventThread = CreateThread(NULL, 0, EventThreadProc, this, 0, NULL);
        }
        ChkBool( m_hEventThread.valid(), E_FAIL );
    }
Cleanup:
    return hr;
}


// Start a dshow graph for the indicated media.
// The URI is required and has Host, Port, and Identifier.
// The IMediaMetadata has all the details about how to play.
HRESULT 
CConnection::SetURI( LPCWSTR pszURI, IMediaMetadata *pIMM, EMediaClass MediaClass )
{
    ce::gate<ce::critical_section> _gate(m_cs);

    HRESULT hr = S_OK;
    CComPtr<IEnumFilters>   pEnum   = NULL;
    CComPtr<IBaseFilter>    pFilter = NULL;
    RenderMpeg2TransportStream::Graph graphWrapper;

    // Validate some input parameters
    ChkBool( pIMM != NULL, E_POINTER );
    ChkBool( pszURI != NULL, E_POINTER );
    ChkBool( pszURI[0] != 0, E_INVALIDARG );

    // Load the Metadata
    m_MediaClass = MediaClass;
    Chk(AcquireInterfaces());
    Chk(m_pPersistMediaMetadata->Load(pIMM));

    // Load the content
    Chk( graphWrapper.RenderFile( m_pTrackGraph, pszURI ));
    Chk( SetVideoWindow( pIMM ));
    Chk( m_pMediaControl->Pause( ));

Cleanup:
    return hr;
}

static HRESULT GetMetadataResolution( IMediaMetadata *pIMM, LONG *pH, LONG *pW )
{
    HRESULT hr = S_OK;
    BSTR key = SysAllocString(L"resolution");
    BSTR val = NULL;
    LONG high = 0, wide = 0;

    // see if the metadata spelled out the resolution
    ChkBool(pIMM != NULL, E_POINTER);
    ChkBool(key != NULL, E_OUTOFMEMORY);
    Chk(pIMM->Get(key, &val));
    ChkBool(val != NULL, E_FAIL);
    ChkBool(swscanf(val, L"%ldx%ld", pH, pW) == 2, E_FAIL);

Cleanup:
    SysFreeString(key);
    SysFreeString(val);
    return hr;
}

HRESULT 
CConnection::SetVideoWindow( IMediaMetadata *pIMM )
{
    HRESULT hr = S_OK;
    LONG lVideoWidth = 0, lVideoHeight = 0;
    LONG lScreenWidth = 0, lScreenHeight = 0;

    // This does not apply to video and images
    ChkBool( m_MediaClass == MediaClass_Image || m_MediaClass == MediaClass_Video, S_OK);

    // Determine the screen size
    lScreenWidth = (LONG)GetSystemMetrics(SM_CXSCREEN);
    lScreenHeight = (LONG)GetSystemMetrics(SM_CYSCREEN);

    // Get the video sizings from DShow
    Chk( m_pBasicVideo->get_SourceWidth( &lVideoWidth ));
    Chk( m_pBasicVideo->get_SourceHeight( &lVideoHeight ));

    // If Dshow does not know the dimensions, try the metadata
    if (lVideoWidth == 0 && lVideoHeight == 0)
    {
        // Try for dimensions from the metadata
        if (FAILED(GetMetadataResolution(pIMM, &lVideoHeight, &lVideoWidth)))
        {
            // No metadata info.  Default to screen size.
            lVideoWidth  = lScreenWidth;
            lVideoHeight = lScreenHeight;
        }
    }

    // Downsize video window if necessary
    if (lVideoWidth > lScreenWidth)
    {
        // Downscale image to fit screen horizontally; Scale H & V to preserve aspect ration.
        lVideoHeight = (lVideoHeight * lScreenWidth) / lVideoWidth;  // Do this first
        lVideoWidth  = (lVideoWidth  * lScreenWidth) / lVideoWidth;  // Do this second
    }
    if (lVideoHeight > lScreenHeight)
    {
        // Downscale image to fit screen vertically; Scale H & V to preserve aspect ration.
        lVideoWidth  = (lVideoWidth  * lScreenHeight)  / lVideoHeight;  // Do this first
        lVideoHeight = (lVideoHeight * lScreenHeight)  / lVideoHeight;  // Do this second
    }

    // Center video window
    LONG lPositionHoriz = (lScreenWidth - lVideoWidth) / 2;
    LONG lPositionVert  = (lScreenHeight - lVideoHeight) / 2;


    // Set the video window
    ChkBool( m_pVideoWindow != NULL, E_FAIL );
    Chk( m_pVideoWindow->SetWindowPosition( lPositionHoriz, lPositionVert, lVideoWidth, lVideoHeight ));
    Chk( m_pVideoWindow->SetWindowForeground( -1 ));

Cleanup:

    // Convert successes to S_OK and failures to S_FALSE
    hr = SUCCEEDED(hr) ? S_OK : S_FALSE;

    return hr;
}


HRESULT 
CConnection::SetCurrentTrack(unsigned long nTrack, av::PositionInfo* pPositionInfo)
{
    if( nTrack == 1 )
    {
        return S_OK;
    }

    return E_FAIL;
}


HRESULT 
CConnection::AcquireInterfaces()
{
    HRESULT hr = S_OK;
    
    ReleaseInterfaces();
    Chk( CoCreateInstance( CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void **) &m_pTrackGraph ));
    Chk( m_pTrackGraph->QueryInterface( IID_IMediaControl, (void**) &m_pMediaControl ));
    Chk( m_pTrackGraph->QueryInterface( IID_IMediaEvent, (void**) &m_pMediaEvent ));
    Chk( m_pTrackGraph->QueryInterface( IID_IMediaSeeking, (void**) &m_pMediaSeeking ));
    Chk( m_pTrackGraph->QueryInterface( IID_IBasicAudio, (void**) &m_pBasicAudio ));
    Chk( m_pTrackGraph->QueryInterface( IID_IBasicVideo, (void**) &m_pBasicVideo ));
    Chk( m_pTrackGraph->QueryInterface( IID_IVideoWindow, (void**) &m_pVideoWindow ));
    Chk( m_pTrackGraph->QueryInterface( IID_IPersistMediaMetadata, (void**) &m_pPersistMediaMetadata));

Cleanup:
    if( FAILED( hr ))
    {
        ReleaseInterfaces();
    }
    return hr;
}


void 
CConnection::ReleaseInterfaces(void)
{
    // Release all references
    m_pTrackGraph.Release();
    m_pMediaControl.Release();
    m_pMediaEvent.Release();
    m_pMediaSeeking.Release();
    m_pBasicAudio.Release();
    m_pBasicVideo.Release();
    m_pVideoWindow.Release();
    m_pPersistMediaMetadata.Release();
}


HRESULT 
CConnection::Play()
{
    ce::gate<ce::critical_section> _gate(m_cs);
    HRESULT hr = S_OK;
    LONGLONG lasttime;
    
    // Start the show running
    ChkBool( m_pMediaControl != NULL, E_FAIL );
    Chk( m_pMediaControl->Run());

    // Set volume if applicable
    if (m_MediaClass == MediaClass_Video || m_MediaClass == MediaClass_Audio)
    {
        Chk( SetVolume( m_Volume ));
    }

    // Make video and images visible
    if (m_MediaClass == MediaClass_Video || m_MediaClass == MediaClass_Image)
    {
        ChkBool( m_pVideoWindow != NULL, E_FAIL );
        Chk(m_pVideoWindow->put_Visible( OATRUE ));
        Chk(m_pVideoWindow->SetWindowForeground( -1 ));
    }

    // Prepare for A/V ending and Ensure that A/V has started.
    if (m_MediaClass == MediaClass_Video || m_MediaClass == MediaClass_Audio)
    {

        // Tell the event thread that we are started.
        if (m_hStartEvent.valid())
        {
            SetEvent(m_hStartEvent);
        }

        // Ensure that A/V is running 
        ChkBool( m_pMediaSeeking != NULL, hr);
        Chk( m_pMediaSeeking->GetCurrentPosition( &lasttime ));
        for (int i = 0; i < 50; i++)
        {
            LONGLONG time;
            Chk( m_pMediaSeeking->GetCurrentPosition( &time ));
            if (time != lasttime)
            {
                break;
            }
            lasttime = time;
            Sleep(100);
        }
    }

Cleanup:
    return hr;
}

HRESULT
CConnection::Pause()
{
    ce::gate<ce::critical_section> _gate(m_cs);
    HRESULT hr = S_OK;
    ChkBool( m_pMediaControl != NULL, E_FAIL );
    Chk( m_pMediaControl->Pause());
Cleanup:
    return hr;
}

HRESULT 
CConnection::Stop()
{
    ce::gate<ce::critical_section> _gate(m_cs);
    HRESULT hr = S_OK;
    LONGLONG llPos = 0;

    // The event thread no longer waits for this media to end
    if (m_hStopEvent.valid())
    {
        SetEvent(m_hStopEvent);
    }

    // Stop
    ChkBool( m_pMediaControl != NULL, E_FAIL );
    Chk( m_pMediaControl->Stop());

    // Reset position to the beginning
    if (m_MediaClass == MediaClass_Video || m_MediaClass == MediaClass_Audio)
    {
        Chk(m_pMediaSeeking->SetPositions( &llPos, AM_SEEKING_AbsolutePositioning, 
                                           NULL, AM_SEEKING_NoPositioning ));
    }

    // Make media invisible 
    if (m_MediaClass == MediaClass_Video || m_MediaClass == MediaClass_Image)
    {
        ChkBool( m_pVideoWindow != NULL, E_FAIL );
        Chk(m_pVideoWindow->put_Visible( OAFALSE ));
    }

Cleanup:
    return hr;
}


HRESULT 
CConnection::Cleanup()
{
    ce::gate<ce::critical_section> _gate(m_cs);

    delete [] m_pProtocolInfo;
    m_pProtocolInfo = NULL;

    delete[] m_pStatus;
    m_pStatus = NULL;

    ReleaseInterfaces();
    return S_OK;
}


HRESULT 
CConnection::SetVolume( unsigned short Volume )
{
    ce::gate<ce::critical_section> _gate(m_cs);
    HRESULT hr = S_OK;
    m_Volume = Volume;
    
    if( m_pBasicAudio != NULL )
    {
        Chk( m_pBasicAudio->put_Volume( VolumeLinToLog( Volume )));
    }

Cleanup:
    return hr;
}


HRESULT 
CConnection::GetVolume( unsigned short* pVolume )
{
    ce::gate<ce::critical_section> _gate(m_cs);
    HRESULT hr = S_OK;
    
    ChkBool( pVolume != NULL, E_INVALIDARG );
    *pVolume = m_Volume;

Cleanup:
    return hr;
}


HRESULT 
CConnection::SetProtocolInfo( LPCWSTR pszProtocolInfo )
{
    ce::gate<ce::critical_section> _gate(m_cs);
    HRESULT hr = S_OK;
    DWORD dwChars = 0;

    ChkBool( pszProtocolInfo != NULL, E_INVALIDARG );
    Chk( StringCchLength( pszProtocolInfo, STRSAFE_MAX_CCH, (size_t*) &dwChars ));

    delete[] m_pProtocolInfo;
    m_pProtocolInfo = NULL;

    m_pProtocolInfo = new WCHAR[ ++dwChars ];
    ChkBool( m_pProtocolInfo != NULL, E_OUTOFMEMORY );
    memcpy( m_pProtocolInfo, pszProtocolInfo, dwChars * sizeof( WCHAR ) );

Cleanup:
    return hr;
}


HRESULT 
CConnection::GetProtocolInfo( LPWSTR* ppszProtocolInfo )
{
    ce::gate<ce::critical_section> _gate(m_cs);
    HRESULT hr = E_FAIL;

    ChkBool( ppszProtocolInfo != NULL, E_INVALIDARG );
    *ppszProtocolInfo = L"";

    if( m_pProtocolInfo)
    {
        // BUGBUG: This is trash. We should be copyying the string
        *ppszProtocolInfo = m_pProtocolInfo;
        hr = S_OK;
    }

Cleanup:
    return hr;
}


HRESULT
CConnection::SetStatus( LPWSTR pszStatus )
{
    ce::gate<ce::critical_section> _gate(m_cs);
    HRESULT hr = S_OK;
    DWORD dwChars = 0;
    
    ChkBool( pszStatus != NULL, E_INVALIDARG );
    Chk( StringCchLength( pszStatus, STRSAFE_MAX_CCH, (size_t*)&dwChars ));

    delete[] m_pStatus;
    m_pStatus = NULL;

    m_pStatus = new WCHAR[ ++dwChars ];
    memcpy( m_pStatus, pszStatus, dwChars * sizeof( WCHAR ) );

Cleanup:
    return hr;
}


HRESULT 
CConnection::GetStatus( LPWSTR* ppszStatus )
{
    ce::gate<ce::critical_section> _gate(m_cs);
    HRESULT hr = E_FAIL;

    ChkBool( ppszStatus != NULL, E_INVALIDARG );
    *ppszStatus = NULL;

    if( m_pStatus )
    {
        // BUGBUG: This is trash. We should be copyying the string
        *ppszStatus = m_pStatus;
        hr = S_OK;
    }

Cleanup:
    return hr;
}


HRESULT 
CConnection::SetPositions( LONGLONG *pllStart, DWORD dwStartFlags, LONGLONG *pllStop, DWORD dwStoptFlags )
{
    ce::gate<ce::critical_section> _gate(m_cs);
    HRESULT hr = S_OK;
    LONGLONG llStartLocal, llStopLocal;

    ChkBool( m_pMediaSeeking != NULL, E_FAIL );

    // Convert from ms to 100ns, and do so locally.
    if (pllStart != NULL)
    {
        llStartLocal = *pllStart * 10000;
        pllStart = &llStartLocal;
    }
    if (pllStop != NULL)
    {
        llStopLocal = *pllStop * 10000;
        pllStop = &llStopLocal;
    }

    Chk( m_pMediaSeeking->SetPositions( pllStart, dwStartFlags, pllStop, dwStoptFlags ));

Cleanup:
    return hr;
}


HRESULT
CConnection::GetCurrentPosition( LONGLONG *pllPosition )
{
    ce::gate<ce::critical_section> _gate(m_cs);
    HRESULT hr = S_OK;

    ChkBool( m_pMediaSeeking != NULL, E_FAIL );
    ChkBool( pllPosition != NULL, E_INVALIDARG );
    Chk( m_pMediaSeeking->GetCurrentPosition( pllPosition ));
    *pllPosition /= 10000;   // convert from 100ns to ms

Cleanup:
    return hr;
}


DWORD WINAPI CConnection::EventThreadProc(LPVOID lpParameter)
{
    HRESULT         hr = S_OK;
    BOOL            fContinue = TRUE;
    BOOL            fPlaying = FALSE;
    LONG            lEvent, lParam1, lParam2;
    DWORD           dwResult;
    HANDLE          hEvents[4];
    DWORD           nEvents = 0;
    OAEVENT         hEOSEvent = NULL;

    CConnection* pThis = reinterpret_cast<CConnection*>(lpParameter);
    assert(pThis);


    while (fContinue)
    {
        // wait for start stream, stop stream, or shutdown indications
        hEvents[0] = pThis->m_hShutdownEvent;    // shut down event
        hEvents[1] = pThis->m_hStartEvent;       // the play has started event
        hEvents[2] = pThis->m_hStopEvent;        // the play is stopped
        hEvents[3] = (HANDLE)hEOSEvent;          // the play is played out.
        nEvents    = fPlaying ? 4 : 3;
        dwResult = WaitForMultipleObjects( nEvents, hEvents, FALSE, INFINITE );

        // Take the lock - but scope it to the switch statement
        { 
            ce::gate<ce::critical_section> _gate(pThis->m_cs);

            // Process the event 
            switch( dwResult )
            {
            case WAIT_OBJECT_0+0:
                // We are shutting down
                fContinue = FALSE;
                break;

            case WAIT_OBJECT_0+1:
                // The play has started, Find the EOS event
                hEOSEvent = NULL;
                if (pThis->m_pMediaEvent != NULL)
                {
                    if (FAILED(pThis->m_pMediaEvent->GetEventHandle(&hEOSEvent)))
                    {
                        hEOSEvent = NULL;
                    }
                }
                if (hEOSEvent != NULL)
                {
                    fPlaying = TRUE;
                }
                break;

            case WAIT_OBJECT_0+2:
                // The play has been stopped.
                hEOSEvent = NULL;
                fPlaying = FALSE;
                break;

            case WAIT_OBJECT_0+3:
                // Get the next event
                if (pThis->m_pMediaEvent == NULL
                || FAILED( pThis->m_pMediaEvent->GetEvent( &lEvent, &lParam1, &lParam2, 0 )))
                {
                    break;
                }


                // process the enxt event
                if(lEvent == EC_COMPLETE || lEvent == EC_ERRORABORT || lEvent == EC_USERABORT)
                {
                    // The play has ended.  verify and tell someone
                    hEOSEvent = NULL;
                    fPlaying = FALSE;
                    if (pThis->m_pITCB != NULL)
                    {
                        pThis->m_pITCB->EventNotify(ETransportCallbackEvent_EOS);
                    }
                }

                pThis->m_pMediaEvent->FreeEventParams( lEvent, lParam1, lParam2 );
                break;

            default:
                // Error waiting? 
                hEOSEvent = NULL;
                fPlaying = FALSE;
                break;
            }  // end switch
        }  // end lock
    } // end while 

    return 0;
}

extern "C" HRESULT GetConnection(IConnection** ppConnection)
{
    HRESULT hr = S_OK;
    CPREx(ppConnection, E_FAIL);
    *ppConnection = new CConnection;
Error:
    return hr;
}
