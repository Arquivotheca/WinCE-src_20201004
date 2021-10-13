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

// AVTransport.cpp : Implementation of CAVTransport

#include "stdafx.h"


/////////////////////////////////////////////////////////////////////////////
// CAVTransport


CAVTransport::CAVTransport(ce::smart_ptr<IConnection> pConnection)
    : m_pSubscriber(NULL)
    , m_pConnection(NULL)
{
    if (pConnection == NULL)
    {
        assert(0);
        return;
    }
    m_pConnection = pConnection;
    m_pConnection->PrepareEvent(static_cast<ITransportCallback*>(this));

}

#define WSTRING_ASSIGN(str, val)    if ( !str.assign(val) ) return false;

bool CAVTransport::Init()
{
    // Initialize most strings
    ClearMetadata();
    m_bNewURI = false;

    // MediaInfo
    m_MediaInfo.nNrTracks = 0;
    WSTRING_ASSIGN(m_MediaInfo.strRecordMedium ,av::NotImpl);
    WSTRING_ASSIGN(m_MediaInfo.strWriteStatus ,av::NotImpl);
    WSTRING_ASSIGN(m_MediaInfo.strPlayMedium ,av::Medium::None);
    
    // TransportSettings
    WSTRING_ASSIGN(m_TransportSettings.strPlayMode ,av::PlayMode::Normal);
    WSTRING_ASSIGN(m_TransportSettings.strRecQualityMode ,av::NotImpl);
    
    // DeviceCapabilities
    WSTRING_ASSIGN(m_DeviceCapabilities.strPossiblePlayMedia ,L"NONE,NETWORK");
    WSTRING_ASSIGN(m_DeviceCapabilities.strPossibleRecMedia ,av::NotImpl);
    WSTRING_ASSIGN(m_DeviceCapabilities.strPossibleRecQualityModes ,av::NotImpl);

    // PositionInfo
    m_PositionInfo.nTrack = 0;
    m_PositionInfo.nRelCount = -1;
    m_PositionInfo.nAbsCount = -1;
    WSTRING_ASSIGN(m_PositionInfo.strAbsTime ,av::NotImpl);
    
    // TransportInfo
    WSTRING_ASSIGN(m_TransportInfo.strCurrentTransportState ,av::TransportState::NoMedia);
    WSTRING_ASSIGN(m_TransportInfo.strCurrentTransportStatus ,av::OK);
    WSTRING_ASSIGN(m_TransportInfo.strCurrentSpeed, L"1");

    //TransportActions
    m_TransportActionBits.bPlay     = FALSE;
    m_TransportActionBits.bStop     = FALSE;
    m_TransportActionBits.bPause    = FALSE;
    m_TransportActionBits.bSeek     = FALSE;
    m_TransportActionBits.bNext     = FALSE;
    m_TransportActionBits.bPrevious = FALSE;
    m_TransportActionBits.bRecord   = FALSE;

    return true;
}

CAVTransport::~CAVTransport()
{
    Uninit();
}

void CAVTransport::Uninit()
{
    // Set the player to a clean state
    ClearAVTransport();
    m_pSubscriber = NULL;
}


//
// ServiceInstanceLifetime
//
void CAVTransport::OnFinalRelease()
{
    delete this;
}


//
// IEventSource
//

inline  bool CAVTransport::IsInState(LPCWSTR state)
{
    return wcscmp( m_TransportInfo.strCurrentTransportState, state) == 0
           ? true : false;
}

inline  bool CAVTransport::IsNotInState(LPCWSTR state)
{
    return wcscmp( m_TransportInfo.strCurrentTransportState, state) == 0
           ? false : true;
}

DWORD CAVTransport::Advise( av::IEventSink* pSubscriber )
{
    DEBUGMSG(ZONE_AVTRANSPORT, (L"Advise"));
    ASSERT(m_pSubscriber == NULL);
    
    m_pSubscriber = pSubscriber;

    if(m_pSubscriber)
    {
        // Call OnStateChange for each state variable
        LPCWSTR vpszValues[] =
        {
            // TransportInfo
            m_TransportInfo.strCurrentTransportState,
            m_TransportInfo.strCurrentTransportStatus,
            m_TransportInfo.strCurrentSpeed,
            m_TransportActionString,

            // PositionInfo
            m_PositionInfo.strTrackDuration,
            m_PositionInfo.strTrackURI,
            m_PositionInfo.strTrackMetaData,

            // DeviceCapabilities
            m_DeviceCapabilities.strPossiblePlayMedia,
            m_DeviceCapabilities.strPossibleRecMedia,
            m_DeviceCapabilities.strPossibleRecQualityModes,

            // TransportSettings
            m_TransportSettings.strPlayMode,
            m_TransportSettings.strRecQualityMode,

            // MediaInfo
            m_MediaInfo.strMediaDuration,
            m_MediaInfo.strCurrentURI,
            m_MediaInfo.strCurrentURIMetaData,
            m_MediaInfo.strNextURI,
            m_MediaInfo.strNextURIMetaData,
            m_MediaInfo.strPlayMedium,
            m_MediaInfo.strRecordMedium,
            m_MediaInfo.strWriteStatus
        };

        MultiOnStateChanged(m_pSubscriber, c_vpszStateVariables, vpszValues);

        // TODO: need way of giving unsigned long's to subscriber. convert to strings?

        long vnValues[] =
        {
            m_PositionInfo.nTrack,
            m_MediaInfo.nNrTracks
        };

        MultiOnStateChanged(m_pSubscriber, c_vNnumericStateVariables, vnValues);
    }

    return av::SUCCESS_AV;
}

DWORD CAVTransport::Unadvise( av::IEventSink* pSubscriber)
{
    DEBUGMSG(ZONE_AVTRANSPORT, (L"UnAdvise"));
    m_pSubscriber = NULL;
    return av::SUCCESS_AV;
}

HRESULT CAVTransport::EventNotify(ETransportCallbackEvent event)
{
    HRESULT hr = S_OK;
    if (event == ETransportCallbackEvent_EOS)
    {
        Chk( Stop() );
    }
    else if (event == ETransportCallbackEvent_BOS)
    {
        Chk( Play(L"1") );
    }
    else
    {
        hr = E_FAIL;
    }
Cleanup:
    return hr;
}



//
// SetTransportState
//
void CAVTransport::SetTransportState(LPCWSTR pszState, LPCWSTR pszStatus)
{
    // Change the state, as appropriate
    if (m_TransportInfo.strCurrentTransportState != pszState)
    {
        m_TransportInfo.strCurrentTransportState = pszState;
        OnStateChanged(m_pSubscriber, av::AVTransportState::TransportState, m_TransportInfo.strCurrentTransportState);
    }

    // Change the status, as appropriate
    if (m_TransportInfo.strCurrentTransportStatus != pszStatus)
    {
        m_TransportInfo.strCurrentTransportStatus = pszStatus;
        OnStateChanged(m_pSubscriber, av::AVTransportState::TransportStatus, m_TransportInfo.strCurrentTransportStatus);
    }

    // Determine the new available actions - set the defaults
    LPCWSTR newTransportActionString = L"";
    av::TransportActions  newTransportActionBits;
    newTransportActionBits.bNext     = FALSE;
    newTransportActionBits.bPrevious = FALSE;
    newTransportActionBits.bRecord   = FALSE;
    newTransportActionBits.bStop     = FALSE;
    newTransportActionBits.bPlay     = FALSE;
    newTransportActionBits.bPause    = FALSE;
    newTransportActionBits.bSeek     = FALSE;

    if( wcscmp( pszState, av::TransportState::NoMedia ) == 0 )
    {
        ;
    }
    else if( wcscmp( pszState, av::TransportState::Stopped ) == 0 )
    {
        newTransportActionBits.bStop = TRUE;
        newTransportActionBits.bPlay = TRUE;
        newTransportActionBits.bSeek = m_fCanSeek ? TRUE : FALSE;
        newTransportActionString = (m_fCanSeek)
                      ? L"Play,Stop,Seek,X_DLNA_SeekTime"
                      : L"Play,Stop";
    }
    else if( wcscmp(pszState, av::TransportState::Playing) == 0 )
    {
        newTransportActionBits.bStop = TRUE;
        newTransportActionBits.bPlay = TRUE;
        newTransportActionBits.bPause= TRUE;
        newTransportActionBits.bSeek = m_fCanSeek ? TRUE : FALSE;
        newTransportActionString = (m_fCanSeek)
                      ? L"Pause,Stop,Seek,X_DLNA_SeekTime"
                      : L"Pause,Stop";
    }
    else if( wcscmp( pszState, av::TransportState::PausedPlayback ) == 0 )
    {
        newTransportActionBits.bStop = TRUE;
        newTransportActionBits.bPlay = TRUE;
        newTransportActionString = L"Play,Stop";
    }
    else if( wcscmp( pszState, av::TransportState::Transitioning ) == 0 )
    {
        newTransportActionBits.bStop = TRUE;
        newTransportActionString = L"Stop";
    }

    // Change the available actions, as appropriate
    if (wcscmp(m_TransportActionString, newTransportActionString) != 0)
    {
        m_TransportActionBits = newTransportActionBits;
        m_TransportActionString = newTransportActionString;
        OnStateChanged(m_pSubscriber, av::AVTransportState::CurrentTransportActions, m_TransportActionString);
    }
}




//
// IAVTransportImpl
//


void 
CAVTransport::ClearAVTransport(void)
{
    // Set the player to a clean state
    ClearMetadata();
    if ( m_pConnection != NULL )
    {
        m_pConnection->Cleanup();
    }
    SetTransportState( av::TransportState::NoMedia, av::OK );
}

DWORD 
CAVTransport::SetAVTransportURI( LPCWSTR pszCurrentURI, LPCWSTR pszCurrentURIMetaData )
{
    DEBUGMSG(ZONE_AVTRANSPORT, (L"+SetAVTransportURI"));
    DWORD hr = av::SUCCESS_AV; 
    bool  bForcedPlay = false;

    // Check input
    ChkBool( pszCurrentURI != NULL, av::ERROR_AV_POINTER );
    ChkBool( pszCurrentURIMetaData != NULL, av::ERROR_AV_POINTER );

    // Make sure that this command is permitted
    if ( IsNotInState(av::TransportState::Stopped) 
    &&   IsNotInState(av::TransportState::NoMedia) )
    {
        return av::ERROR_AV_UPNP_AVT_TRANSPORT_LOCKED;
    }

    // If the params are empty strings, then clear the info
    if ( pszCurrentURI[0] == 0 && pszCurrentURIMetaData[0] == 0)
    {
        ClearAVTransport();
        return av::SUCCESS_AV;
    }

    // process all metadata
    Chk(SetMetadata(pszCurrentURI, pszCurrentURIMetaData));

    // Clean out the connection object
    ChkBool( m_pConnection != NULL, av::ERROR_AV_UPNP_ACTION_FAILED );
    ChkUPnP( m_pConnection->Cleanup( ), av::ERROR_AV_UPNP_ACTION_FAILED );

    // Load the media
    ChkUPnP( m_pConnection->SetProtocolInfo( m_MetaDataProtocolInfo ),
                  av::ERROR_AV_UPNP_ACTION_FAILED );
    m_bNewURI = true;

Cleanup:
    // At this point we are committed to going to the stop state 
    SetTransportState( av::TransportState::Stopped, av::OK );

    // Send notifies
    if(m_pSubscriber)
    {
        LPCWSTR vNewVals[] = 
        {
            m_PositionInfo.strTrackURI,
            m_MediaInfo.strCurrentURI,
            m_MetaDataForNotify,  // uri metadata
            m_MetaDataForNotify,  // track metadata
            m_PositionInfo.strTrackDuration,
            m_MediaInfo.strMediaDuration, 
            0
        };

        long vnValues[] = 
        {
            m_PositionInfo.nTrack,
            m_MediaInfo.nNrTracks,
            0}
        ;

        MultiOnStateChanged(m_pSubscriber, c_vpszChangedStateVars, vNewVals);
        MultiOnStateChanged(m_pSubscriber, c_vnNumericStateVariables, vnValues);
    }

    DEBUGMSG(ZONE_AVTRANSPORT, (L"-SetAVTransportURI (%x)", hr));
    return hr;
}

DWORD CAVTransport::Play( LPCWSTR pszSpeed )
{
    DWORD hr = av::SUCCESS_AV;

    // Check state transition
    if( IsNotInState(av::TransportState::Playing)
    &&  IsNotInState(av::TransportState::Stopped)
    &&  IsNotInState(av::TransportState::PausedPlayback) )
    {
         return av::ERROR_AV_UPNP_AVT_INVALID_TRANSITION;
    }

    // Check play speed argument
    ChkBool(pszSpeed != NULL, av::ERROR_AV_POINTER);
    ChkBool( wcscmp( pszSpeed, m_TransportInfo.strCurrentSpeed ) == 0, av::ERROR_AV_UPNP_AVT_UNSUPPORTED_PLAY_SPEED );

    // Build the filter graph and start playing
    ChkBool(m_pConnection != NULL, av::ERROR_AV_UPNP_ACTION_FAILED);
    if (m_bNewURI)
    {
        ChkUPnP( m_pConnection->SetURI( m_NewURI, m_pIMM, m_MediaClass ), 
                                    av::ERROR_AV_UPNP_AVT_RESOURCE_NOT_FOUND);
        m_bNewURI = false;    // successful start.  Do not build the graph again.
    }
    ChkUPnP(m_pConnection->Play(), av::ERROR_AV_UPNP_ACTION_FAILED);

Cleanup:
    if (hr == av::SUCCESS_AV)
    {
        SetTransportState(av::TransportState::Playing, av::OK);
    }
    else
    {
        SetTransportState(av::TransportState::Stopped, av::OK);
    }
    DEBUGMSG(ZONE_AVTRANSPORT, (L"%cPlay", hr == av::SUCCESS_AV ? L'-' : L'#'));
    return hr;
}


DWORD CAVTransport::Pause()
{
    DEBUGMSG(ZONE_AVTRANSPORT, (L"+Pause"));
    DWORD hr = av::SUCCESS_AV;

    // Note that Pause is a valid transition even for images. 
    if ( IsNotInState(av::TransportState::Playing) )
    {
        return av::ERROR_AV_UPNP_AVT_INVALID_TRANSITION;
    }

    ChkBool( m_pConnection != NULL, av::ERROR_AV_UPNP_ACTION_FAILED );
    ChkUPnP( m_pConnection->Pause(), av::ERROR_AV_UPNP_ACTION_FAILED );

Cleanup:
    if (SUCCEEDED(hr))
    {
        SetTransportState(av::TransportState::PausedPlayback, av::OK);
    }
    else
    {
        SetTransportState(av::TransportState::Stopped, av::OK);
    }
    DEBUGMSG(ZONE_AVTRANSPORT, (L"-Pause"));
    return hr;
}


DWORD CAVTransport::Stop()
{
    DEBUGMSG(ZONE_AVTRANSPORT, (L"+Stop"));
    DWORD hr = av::SUCCESS_AV;

    if( IsInState(av::TransportState::NoMedia) )
    {
        return av::ERROR_AV_UPNP_AVT_INVALID_TRANSITION;
    }
    ChkBool( m_pConnection != NULL, av::ERROR_AV_UPNP_ACTION_FAILED );
    ChkUPnP( m_pConnection->Stop(), av::ERROR_AV_UPNP_ACTION_FAILED );
    if( m_pSubscriber )
    {
        m_pSubscriber->OnStateChanged(av::AVTransportState::RelTime, m_PositionInfo.strRelTime);
    }

Cleanup:
    SetTransportState(av::TransportState::Stopped, av::OK);
    DEBUGMSG(ZONE_AVTRANSPORT, (L"-Stop"));
    return hr;
}


DWORD CAVTransport::Seek( LPCWSTR pszUnit, LPCWSTR pszTarget)
{
    DWORD       hr = av::SUCCESS_AV;
    LONGLONG    lPosition;
    DEBUGMSG(ZONE_AVTRANSPORT, (L"+Seek"));
    DWORD       dwSeekMode;

    // Make sure we are in a state that allows us to seek
    if (  IsNotInState(av::TransportState::Playing)
       && IsNotInState(av::TransportState::Stopped) )
    {
        return av::ERROR_AV_UPNP_AVT_INVALID_TRANSITION;
    }

    ChkBool( pszUnit != NULL, av::ERROR_AV_POINTER );
    ChkBool( pszTarget!= NULL, av::ERROR_AV_POINTER );

    // We support Seek by absolute and Rel time (no playlist support)
    if ( _wcsicmp( pszUnit, av::SeekMode::RelTime ) == 0 )
    {
        dwSeekMode = AM_SEEKING_AbsolutePositioning;
    }
    else if ( _wcsicmp( pszUnit, av::SeekMode::AbsTime ) == 0 )
    {
        dwSeekMode = AM_SEEKING_AbsolutePositioning;
    }
    else if ( _wcsicmp( pszUnit, av::SeekMode::TrackNr ) == 0 )
    {
        ChkBool( 0, av::ERROR_AV_UPNP_AVT_ILLEGAL_SEEK_TARGET );
    }
    else
    {
        ChkBool( 0, av::ERROR_AV_UPNP_AVT_UNSUPPORTED_SEEK_MODE );
    }

    // Extract the time
    ChkUPnP( ParseMediaTime( pszTarget, &lPosition), av::ERROR_AV_UPNP_AVT_ILLEGAL_SEEK_TARGET );

    // Verify that we can seek. and that the seek is valid.
    ChkBool( (m_fCanSeek || m_bLimitedRange), av::ERROR_AV_UPNP_AVT_UNSUPPORTED_SEEK_MODE );
    ChkBool(m_llDuration >= lPosition, av::ERROR_AV_UPNP_AVT_ILLEGAL_SEEK_TARGET);

    // Perform seek operation
    ChkBool( m_pConnection != NULL, av::ERROR_AV_UPNP_ACTION_FAILED );
    ChkUPnP( m_pConnection->SetPositions(&lPosition, dwSeekMode, NULL, AM_SEEKING_NoPositioning ), av::ERROR_AV_UPNP_AVT_ILLEGAL_SEEK_TARGET );
    
Cleanup:
    DEBUGMSG(ZONE_AVTRANSPORT, (L"%cSeek", hr == av::SUCCESS_AV ? L'-' : L'#'));
    return hr;
}

                        
DWORD CAVTransport::SetPlayMode( LPCWSTR pszPlayMode )
{
    // Whatever the playmode set, we don't care. We don't support playlists
    return av::SUCCESS_AV;
}


DWORD CAVTransport::Next()
{
    // We do NOT support playlists
    return av::ERROR_AV_UPNP_AVT_ILLEGAL_SEEK_TARGET;
}


DWORD CAVTransport::Previous()
{
    // We do NOT support playlists
    return av::ERROR_AV_UPNP_AVT_ILLEGAL_SEEK_TARGET;
}


DWORD CAVTransport::GetMediaInfo( av::MediaInfo* pMediaInfo )
{
    DWORD hr = av::SUCCESS_AV;
    DEBUGMSG(ZONE_AVTRANSPORT, (L"GetMediaInfo"));

    ChkBool( pMediaInfo != NULL, av::ERROR_AV_POINTER );
    *pMediaInfo = m_MediaInfo;

Cleanup:
    return hr;
}


DWORD 
CAVTransport::GetTransportInfo( av::TransportInfo* pTransportInfo )
{
    DWORD hr = av::SUCCESS_AV;
    DEBUGMSG(ZONE_AVTRANSPORT, (L"GetTransportInfo"));

    ChkBool( pTransportInfo != NULL, av::ERROR_AV_POINTER );
    *pTransportInfo = m_TransportInfo;

Cleanup:
    return hr;
}


DWORD 
CAVTransport::GetPositionInfo( av::PositionInfo *pPositionInfo )
{
    DWORD    hr = av::SUCCESS_AV;
    LONGLONG lPosition = 0;
    DEBUGMSG(ZONE_AVTRANSPORT, (L"GetPositionInfo"));

    ChkBool( pPositionInfo != NULL, av::ERROR_AV_POINTER );

    if( m_pConnection && SUCCEEDED( m_pConnection->GetCurrentPosition( &lPosition )))
    {
        FormatMediaTime( lPosition, &m_PositionInfo.strRelTime );
    }
    else
    {
        m_PositionInfo.strRelTime = L"";
    }

    *pPositionInfo = m_PositionInfo;

Cleanup:
    return hr;
}


DWORD 
CAVTransport::GetDeviceCapabilities( av::DeviceCapabilities* pDeviceCapabilities )
{
    DWORD hr = av::SUCCESS_AV;
    DEBUGMSG(ZONE_AVTRANSPORT, (L"GetDevCap"));

    ChkBool( pDeviceCapabilities != NULL, av::ERROR_AV_POINTER );
    *pDeviceCapabilities = m_DeviceCapabilities;

Cleanup:
    return hr;
}


DWORD 
CAVTransport::GetTransportSettings( av::TransportSettings* pTransportSettings )
{
    DWORD hr = av::SUCCESS_AV;
    DEBUGMSG(ZONE_AVTRANSPORT, (L"GetTransportSettings"));

    ChkBool( pTransportSettings != NULL, av::ERROR_AV_POINTER );
    *pTransportSettings = m_TransportSettings;

Cleanup:
    return hr;
}


// GetCurrentTransportActions
DWORD 
CAVTransport::GetCurrentTransportActions( av::TransportActions* pActions )
{
    DWORD hr = av::SUCCESS_AV;
    DEBUGMSG(ZONE_AVTRANSPORT, (L"GetTransportActions"));

    ChkBool( pActions != NULL, av::ERROR_AV_POINTER );
    *pActions = m_TransportActionBits;

Cleanup:
    return hr;
}





// InvokeVendorAction
DWORD CAVTransport::InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult)
{
    if(0 == wcscmp(L"X_VendorExtensionExample", pszActionName))
    {
        ce::variant varInput;
        UINT        uArgErr;
        
        if(pdispparams->cArgs != 3)
        {
            return av::ERROR_AV_UPNP_ACTION_FAILED;
        }
        
        // ignore the first argument (InstanceID)
        
        // get second [in] argument
        if(FAILED(DispGetParam(pdispparams, 1, VT_BSTR, &varInput, &uArgErr)))
        {
            return av::ERROR_AV_UPNP_ACTION_FAILED;
        }
        
        // named arguments are not supported
        assert(pdispparams->rgdispidNamedArgs == NULL);
        assert(0 == pdispparams->cNamedArgs);
        
        // verify type of [out] argument
        if(pdispparams->rgvarg[0].vt != (VT_BSTR | VT_BYREF))
        {
            return av::ERROR_AV_UPNP_ACTION_FAILED;
        }
        
        assert(*pdispparams->rgvarg[0].pbstrVal == NULL);
            
        av::wstring strResult;
        
        strResult = L"Input was \"";
        strResult += varInput.bstrVal;
        strResult += L"\"";
        
        // set result to [out] argument
        *pdispparams->rgvarg[0].pbstrVal = SysAllocString(strResult);
        
        return av::SUCCESS_AV;
    }
    
    return av::ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD CAVTransport::SetCurrentTrack( long nTrack )
{
    DWORD hr = av::SUCCESS_AV;

    ChkBool( nTrack == 1, av::ERROR_AV_UPNP_AVT_ILLEGAL_SEEK_TARGET );

Cleanup:
    return hr;
}
