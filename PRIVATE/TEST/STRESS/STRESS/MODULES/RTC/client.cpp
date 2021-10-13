//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <rtccore.h>
#include "client.h"
#include "stressutils.h"

CClientEventImpl::CClientEventImpl()
{
	m_hShutdownComplete = CreateEvent( NULL, FALSE, FALSE, NULL );
	m_pClient = NULL;
	m_nMsgCookie = 0;
	m_bConnected = false;
	m_bOpComplete = false;
}


CClientEventImpl::~CClientEventImpl()
{
	if( m_hShutdownComplete )
		CloseHandle( m_hShutdownComplete );
}


HANDLE CClientEventImpl::GetShutdownEvent()
{
	return m_hShutdownComplete;
}

void CClientEventImpl::SetClient( IRTCClient* pClient )
{
	m_pClient = pClient;
}

IRTCClient* CClientEventImpl::GetClient()
{
	return m_pClient;
}

void CClientEventImpl::SetEvents( CRTCEvents* pEvents )
{
	m_pEvents = pEvents;
}

CRTCEvents* CClientEventImpl::GetEvents()
{
	return m_pEvents;
}

void CClientEventImpl::SetCookie( LONG lCookie )
{
	m_nMsgCookie = lCookie;
}

LONG CClientEventImpl::GetCookie()
{
	return m_nMsgCookie;
}


//	RTCE_CLIENT
//	This type of event is fired when general Client-related events occur.
HRESULT CClientEventImpl::OnRTCClientEvent( IDispatch* pDisp )
{
	IRTCClientEvent * pEvent = NULL;
	RTC_CLIENT_EVENT_TYPE enEventType;
	HRESULT hr = S_OK;
	
	hr = pDisp->QueryInterface( __uuidof(IRTCClientEvent), (void **)&pEvent );
	if( FAILED(hr) )
	{
		LogFail( TEXT("CClientEventImpl::OnRTCClientEvent:QI for IRTCClientEvent failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	hr = pEvent->get_EventType( &enEventType );
	if( FAILED(hr) )
	{
		LogFail( TEXT("CClientEventImpl::OnRTCClientEvent:get_EventType failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	if( enEventType == RTCCET_ASYNC_CLEANUP_DONE )
	{
		LogComment( TEXT("CClientEventImpl::OnRTCClientEvent got RTCCET_ASYNC_CLEANUP_DONE.  Setting shutdown event.") );

		SetEvent( m_hShutdownComplete );
		goto cleanup;
	}


cleanup:

	SafeRelease( pEvent );

	return hr;
}


//	RTCE_REGISTRATION_STATE_CHANGE
//	This type of event is fired when a registration changes state.
HRESULT CClientEventImpl::OnRTCRegistrationStateChangeEvent( IDispatch* pDisp )
{
	IRTCRegistrationStateChangeEvent * pEvent = NULL;
	HRESULT hr = S_OK;

	hr = pDisp->QueryInterface( __uuidof(IRTCRegistrationStateChangeEvent),
		(void **)&pEvent );

	if( FAILED(hr) )
	{
		goto cleanup;
	}



cleanup:

	SafeRelease( pEvent );

	return hr;
}


//	RTCE_SESSION_STATE_CHANGE
//	This type of event is fired when a session changes state.
HRESULT CClientEventImpl::OnRTCSessionStateChangeEvent( IDispatch* pDisp )
{
	IRTCSessionStateChangeEvent*	pEvent = NULL;
	IRTCSession*					pSess = NULL;
	RTC_SESSION_STATE				enState;
	long							lStatus = 0;
	BSTR							bstrStatus = NULL;
	HRESULT hr = S_OK;
	
	
	hr = pDisp->QueryInterface( __uuidof(IRTCSessionStateChangeEvent),
		(void **)&pEvent );
	if( FAILED(hr) )
	{
		LogFail( TEXT("CClientEventImpl::OnRTCSessionStateChangeEvent:QI failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	hr = pEvent->get_Session( &pSess );
	if( FAILED(hr) )
	{
		LogFail( TEXT("CClientEventImpl::OnRTCSessionStateChangeEvent:get_Session failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	hr = pEvent->get_State( &enState );
	if( FAILED(hr) )
	{
		LogFail( TEXT("CClientEventImpl::OnRTCSessionStateChangeEvent:get_State failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	pEvent->get_StatusCode( &lStatus );
	pEvent->get_StatusText( &bstrStatus );

	switch( enState )
	{
	case RTCSS_IDLE:
		LogComment( TEXT("CClientEventImpl::OnRTCSessionStateChangeEvent:RTCSS_IDLE"), hr );
		break;

	case RTCSS_INCOMING:
		LogComment( TEXT("CClientEventImpl::OnRTCSessionStateChangeEvent:RTCSS_INCOMING"), hr );
		break;

	case RTCSS_ANSWERING:
		LogComment( TEXT("CClientEventImpl::OnRTCSessionStateChangeEvent:RTCSS_ANSWERING"), hr );
		break;

	case RTCSS_INPROGRESS:
		LogComment( TEXT("CClientEventImpl::OnRTCSessionStateChangeEvent:RTCSS_INPROGRESS"), hr );
		break;

	case RTCSS_CONNECTED:
		LogComment( TEXT("CClientEventImpl::OnRTCSessionStateChangeEvent:RTCSS_CONNECTED"), hr );
		//pSess->AddRef();
		m_bConnected = true;
		break;

	case RTCSS_DISCONNECTED:
		LogComment( TEXT("CClientEventImpl::OnRTCSessionStateChangeEvent:RTCSS_DISCONNECTED(status:0x%08x \"%s\")"), lStatus, bstrStatus );
		//pSess->Release();
		m_bConnected = false;
		break;

	case RTCSS_HOLD:
		LogComment( TEXT("CClientEventImpl::OnRTCSessionStateChangeEvent:RTCSS_HOLD"), hr );
		
		break;

	case RTCSS_REFER:
		LogComment( TEXT("CClientEventImpl::OnRTCSessionStateChangeEvent:RTCSS_REFER"), hr );
		
		break;

	}


cleanup:

	SafeRelease( pSess );
	SafeRelease( pEvent );
	SafeFreeString( bstrStatus );

	return hr;
}


//	RTCE_SESSION_OPERATION_COMPLETE
//	This type of event is fired when operations started with SendMessage, SendMessageStatus, AddStream, and RemoveStream complete.
HRESULT CClientEventImpl::OnRTCSessionOperationCompleteEvent( IDispatch* pDisp )
{
	IRTCSessionOperationCompleteEvent*	pEvent = NULL;
	IRTCSession*						pSess = NULL;
	long								lCookie = -1;
	long								lStatusCode = -1;
	BSTR								bstrStatusText = NULL;
	HRESULT hr = S_OK;
	

	hr = pDisp->QueryInterface( __uuidof(IRTCSessionOperationCompleteEvent),
		(void **)&pEvent );

	if( FAILED(hr) )
	{
		LogFail( TEXT("CClientEventImpl::OnRTCSessionOperationCompleteEvent:QI failed for IRTCSessionOperationCompleteEvent hr = 0x%08x"), hr );
		goto cleanup;
	}

	hr = pEvent->get_Session( &pSess );
	if( FAILED(hr) )
	{
		LogFail( TEXT("CClientEventImpl::OnRTCSessionOperationCompleteEvent:get_Session failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	hr = pEvent->get_Cookie( &lCookie );
	if( FAILED(hr) )
	{
		LogFail( TEXT("CClientEventImpl::OnRTCSessionOperationCompleteEvent:get_Cookie failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	hr = pEvent->get_StatusCode( &lStatusCode );
	if( FAILED(hr) )
	{
		LogFail( TEXT("CClientEventImpl::OnRTCSessionOperationCompleteEvent:get_StatusCode failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	hr = pEvent->get_StatusText( &bstrStatusText );
	if( FAILED(hr) && hr != RTC_E_NOT_EXIST )
	{
		LogFail( TEXT("CClientEventImpl::OnRTCSessionOperationCompleteEvent:get_StatusText failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	LogComment( TEXT("CClientEventImpl::OnRTCSessionOperationCompleteEvent - Session(0x%08x):Cookie(0x%08x):Status Code(0x%08x):Status Text(%ls)"), \
		pSess, lCookie, lStatusCode, bstrStatusText );

	SetOperationComplete( true );

cleanup:

	SafeFreeString( bstrStatusText );
	SafeRelease( pSess );
	SafeRelease( pEvent );
	
	return hr;
}


//	RTCE_PARTICIPANT_STATE_CHANGE
//	This type of event is fired when a participant changes state.
HRESULT CClientEventImpl::OnRTCParticipantStateChangeEvent( IDispatch* pDisp )
{
	IRTCParticipantStateChangeEvent * pEvent = NULL;
	HRESULT hr = S_OK;

	hr = pDisp->QueryInterface( __uuidof(IRTCParticipantStateChangeEvent),
		(void **)&pEvent );

	if( FAILED(hr) )
	{
		goto cleanup;
	}


cleanup:

	SafeRelease( pEvent );

	return hr;
}


//	RTCE_MEDIA
//	This type of event is fired when a media stream starts, stops, pauses, or resumes, or when the media is negotiated.
HRESULT CClientEventImpl::OnRTCMediaEvent( IDispatch* pDisp )
{
	IRTCMediaEvent * pEvent = NULL;
	HRESULT hr = S_OK;

	hr = pDisp->QueryInterface( __uuidof(IRTCMediaEvent),
		(void **)&pEvent );

	if( FAILED(hr) )
	{
		goto cleanup;
	}


cleanup:

	SafeRelease( pEvent );

	return hr;
}


//	RTCE_INTENSITY
//	This type of event is fired when the intensity level changes on one of the audio devices.
HRESULT CClientEventImpl::OnRTCIntensityEvent( IDispatch* pDisp )
{
	IRTCIntensityEvent * pEvent = NULL;
	HRESULT hr = S_OK;

	hr = pDisp->QueryInterface( __uuidof(IRTCIntensityEvent),
		(void **)&pEvent );

	if( FAILED(hr) )
	{
		goto cleanup;
	}

cleanup:

	SafeRelease( pEvent );

	return hr;
}


//	RTCE_MESSAGING
//	This type of event is fired when an incoming instant message arrives, or when an update of the remote user's keystroke status is received.
HRESULT CClientEventImpl::OnRTCMessagingEvent( IDispatch* pDisp )
{
	IRTCMessagingEvent* 		pEvent = NULL;
	IRTCSession*				pSess = NULL;
	IRTCParticipant*			pParticipant = NULL;
	RTC_MESSAGING_EVENT_TYPE	enType;
	
	BSTR						bstrMessage = NULL;
	BSTR						bstrReplyMessage =  NULL;
	BSTR						bstrMessageHeader = NULL;
	BSTR						bstrUserURI = NULL;
	BSTR						bstrName = NULL;
	RTC_PARTICIPANT_STATE		enState;				

	HRESULT hr = S_OK;
	

	hr = pDisp->QueryInterface( __uuidof(IRTCMessagingEvent),
		(void **)&pEvent );
	if( FAILED(hr) )
	{
		LogFail( TEXT("CClientEventImpl::OnRTCMessagingEvent:QI failed for IRTCSessionOperationCompleteEvent hr = 0x%08x"), hr );
		goto cleanup;
	}

	hr = pEvent->get_EventType( &enType );
	if( FAILED(hr) )
	{
		LogFail( TEXT("CClientEventImpl::OnRTCMessagingEvent:get_EventType failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	if( enType == RTCMSET_MESSAGE )
	{
		//	we got a message
		hr = pEvent->get_Session( &pSess );
		if( FAILED(hr) )
		{
			LogFail( TEXT("CClientEventImpl::OnRTCMessagingEvent:get_Session failed hr = 0x%08x"), hr );
			goto cleanup;
		}

		hr = pEvent->get_Participant( &pParticipant );
		if( FAILED(hr) )
		{
			LogFail( TEXT("CClientEventImpl::OnRTCMessagingEvent:get_Participant failed hr = 0x%08x"), hr );
			goto cleanup;
		}

		hr = pEvent->get_Message( &bstrMessage );
		if( FAILED(hr) )
		{
			LogFail( TEXT("CClientEventImpl::OnRTCMessagingEvent:get_Message failed hr = 0x%08x"), hr );
			goto cleanup;
		}

		hr = pEvent->get_MessageHeader( &bstrMessageHeader );
		if( FAILED(hr) )
		{
			LogFail( TEXT("CClientEventImpl::OnRTCMessagingEvent:get_MessageHeader failed hr = 0x%08x"), hr );
			goto cleanup;
		}

		hr = pParticipant->get_UserURI( &bstrUserURI );
		if( FAILED(hr) )
		{
			LogFail( TEXT("CClientEventImpl::OnRTCMessagingEvent:get_UserURI failed hr = 0x%08x"), hr );
			goto cleanup;
		}

		hr = pParticipant->get_Name( &bstrName );
		if( FAILED(hr) )
		{
			LogFail( TEXT("CClientEventImpl::OnRTCMessagingEvent:get_Name failed hr = 0x%08x"), hr );
			goto cleanup;
		}

		hr = pParticipant->get_State( &enState );
		if( FAILED(hr) )
		{
			LogFail( TEXT("CClientEventImpl::OnRTCMessagingEvent:get_State failed hr = 0x%08x"), hr );
			goto cleanup;
		}

		LogComment( TEXT("CClientEventImpl::OnRTCMessagingEvent: got MESSAGE:") );

		LogComment( TEXT("CClientEventImpl::OnRTCMessagingEvent:Session(0x%08x):MessageHeader(%ls):Message(%ls):UserURI(%ls):Name(%ls):State(%d)"), \
			pSess, bstrMessageHeader, bstrMessage, bstrUserURI, bstrName, enState );

		
	}
	

cleanup:

	SafeRelease( pSess );
	SafeRelease( pParticipant );
	SafeRelease( pEvent );

	SafeFreeString( bstrMessage );
	SafeFreeString( bstrMessageHeader );
	SafeFreeString( bstrReplyMessage );
	SafeFreeString( bstrUserURI );
	SafeFreeString( bstrName );

	return hr;
}


//	RTCE_BUDDY
//	This type of event is fired when the presence state of a Buddy changes.
HRESULT CClientEventImpl::OnRTCBuddyEvent( IDispatch* pDisp )
{
	IRTCBuddyEvent * pEvent = NULL;
	HRESULT hr = S_OK;

	hr = pDisp->QueryInterface( __uuidof(IRTCBuddyEvent),
		(void **)&pEvent );

	if( FAILED(hr) )
	{
		goto cleanup;
	}


cleanup:

	SafeRelease( pEvent );

	return hr;
}


//	RTCE_WATCHER
//	This type of event is fired when a request is made to monitor the local user and 
//	there is no matching entry in the list of blocked/allowed Watchers.
HRESULT CClientEventImpl::OnRTCWatcherEvent( IDispatch* pDisp )
{
	IRTCWatcherEvent * pEvent = NULL;
	HRESULT hr = S_OK;

	hr = pDisp->QueryInterface( __uuidof(IRTCWatcherEvent),
		(void **)&pEvent );

	if( FAILED(hr) )
	{
		goto cleanup;
	}


cleanup:

	SafeRelease( pEvent );

	return hr;
}


//	RTCE_PROFILE
//	This type of event is fired when in-band provisioning completes.
HRESULT CClientEventImpl::OnRTCProfileEvent( IDispatch* pDisp )
{
	IRTCProfileEvent * pEvent = NULL;
	HRESULT hr = S_OK;

	hr = pDisp->QueryInterface( __uuidof(IRTCProfileEvent),
		(void **)&pEvent );

	if( FAILED(hr) )
	{
		goto cleanup;
	}


cleanup:

	SafeRelease( pEvent );

	return hr;
}


//	RTCE_USERSEARCH
//	When a search returns from IRTCUserSearch::ExecuteSearch, the RTCE_USERSEARCH event is fired, and 
//	the IRTCUserSearchResultsEvent interface is available by calling QueryInterface on the event object.
HRESULT CClientEventImpl::OnRTCUserSearchResultsEvent( IDispatch* pDisp )
{
	IRTCUserSearchResultsEvent * pEvent = NULL;
	HRESULT hr = S_OK;

	hr = pDisp->QueryInterface( __uuidof(IRTCUserSearchResultsEvent),
		(void **)&pEvent );

	if( FAILED(hr) )
	{
		goto cleanup;
	}


cleanup:

	SafeRelease( pEvent );

	return hr;
}


//	RTCE_INFO
//	The IRTCInfoEvent interface gives access to information contained in an incoming SIP INFO message.
HRESULT CClientEventImpl::OnRTCInfoEvent( IDispatch* pDisp )
{
	IRTCInfoEvent * pEvent = NULL;
	HRESULT hr = S_OK;

	hr = pDisp->QueryInterface( __uuidof(IRTCInfoEvent),
		(void **)&pEvent );

	if( FAILED(hr) )
	{
		goto cleanup;
	}

cleanup:

	SafeRelease( pEvent );

	return hr;
}


//	RTCE_GROUP
//	This type of event is fired when roaming group information is updated.
HRESULT CClientEventImpl::OnRTCBuddyGroupEvent ( IDispatch* pDisp )
{
	IRTCBuddyGroupEvent  * pEvent = NULL;
	HRESULT hr = S_OK;

	hr = pDisp->QueryInterface( __uuidof(IRTCBuddyGroupEvent ),
		(void **)&pEvent );

	if( FAILED(hr) )
	{
		goto cleanup;
	}

cleanup:

	SafeRelease( pEvent );

	return hr;
}


//	RTCE_MEDIA_REQUEST
//	RTCE_MEDIA_REQUEST events are fired when the other party in an RTC 1.2 PC-PC session requests to add or remove media streams.
HRESULT CClientEventImpl::OnRTCMediaRequestEvent( IDispatch* pDisp )
{
	IRTCMediaRequestEvent * pEvent = NULL;
	HRESULT hr = S_OK;

	hr = pDisp->QueryInterface( __uuidof(IRTCMediaRequestEvent),
		(void **)&pEvent );

	if( FAILED(hr) )
	{
		goto cleanup;
	}



cleanup:

	SafeRelease( pEvent );

	return hr;
}


//	RTCE_ROAMING
//	This type of event is fired when a roaming session is enabled, is disabled, or encounters an error.
HRESULT CClientEventImpl::OnRTCRoamingEvent( IDispatch* pDisp )
{
	IRTCRoamingEvent * pEvent = NULL;
	HRESULT hr = S_OK;

	hr = pDisp->QueryInterface( __uuidof(IRTCRoamingEvent),
		(void **)&pEvent );

	if( FAILED(hr) )
	{
		goto cleanup;
	}


cleanup:

	SafeRelease( pEvent );

	return hr;
}


//	RTCE_PRESENCE_PROPERTY
//	The IRTCPresencePropertyEvent interface contains methods for retrieving presence properties
//	after a presence state change operation has been performed, as indicated by a RTCE_PRESENCE_PROPERTY event.
HRESULT CClientEventImpl::OnRTCPresencePropertyEvent( IDispatch* pDisp )
{
	IRTCPresencePropertyEvent  * pEvent = NULL;
	HRESULT hr = S_OK;

	hr = pDisp->QueryInterface( __uuidof(IRTCPresencePropertyEvent ),
		(void **)&pEvent );

	if( FAILED(hr) )
	{
		goto cleanup;
	}


cleanup:

	SafeRelease( pEvent );

	return hr;
}


//	RTCE_PRESENCE_DATA
//	The IRTCPresenceDataEvent interface contains methods for retrieving presence data. 
HRESULT CClientEventImpl::OnRTCPresenceDataEvent( IDispatch* pDisp )
{
	IRTCPresenceDataEvent * pEvent = NULL;
	HRESULT hr = S_OK;

	hr = pDisp->QueryInterface( __uuidof(IRTCPresenceDataEvent),
		(void **)&pEvent );

	if( FAILED(hr) )
	{
		goto cleanup;
	}


cleanup:

	SafeRelease( pEvent );

	return hr;
}


//	RTCE_PRESENCE_STATUS
//	The IRTCPresenceStatusEvent interface contains methods for retrieving presence data. 
HRESULT CClientEventImpl::OnRTCPresenceStatusEvent( IDispatch* pDisp )
{
	IRTCPresenceStatusEvent * pEvent = NULL;
	HRESULT hr = S_OK;

	hr = pDisp->QueryInterface( __uuidof(IRTCPresenceStatusEvent),
		(void **)&pEvent );

	if( FAILED(hr) )
	{
		goto cleanup;
	}

cleanup:

	SafeRelease( pEvent );

	return hr;
}


//	RTCE_SESSION_REFER_STATUS
//	The IRTCSessionReferStatusEvent interface provides methods to inform the application of the status of the referred session.
HRESULT CClientEventImpl::OnRTCSessionReferStatusEvent( IDispatch* pDisp )
{
	IRTCSessionReferStatusEvent  * pEvent = NULL;
	HRESULT hr = S_OK;

	hr = pDisp->QueryInterface( __uuidof(IRTCSessionReferStatusEvent ),
		(void **)&pEvent );

	if( FAILED(hr) )
	{
		goto cleanup;
	}

cleanup:

	SafeRelease( pEvent );

	return hr;
}


//	RTCE_SESSION_REFERRED
//	The IRTCSessionReferredEvent interface provides methods to accept or reject a referred session.
HRESULT CClientEventImpl::OnRTCSessionReferredEvent( IDispatch* pDisp )
{
	IRTCSessionReferredEvent * pEvent = NULL;
	HRESULT hr = S_OK;

	hr = pDisp->QueryInterface( __uuidof(IRTCSessionReferredEvent),
		(void **)&pEvent );

	if( FAILED(hr) )
	{
		goto cleanup;
	}

cleanup:

	SafeRelease( pEvent );

	return hr;
}


//	RTCE_REINVITE
//	The IRTCReInviteEvent interface provides methods that inform the application of a 
//	reinvite event for a session that the application is handling session negotiations for. 
//	The client application can retrieve the session and accept or reject the re-INVITE. 
//	This method initiates a reInvite for the session, not just a media stream.
HRESULT CClientEventImpl::OnRTCReInviteEvent( IDispatch* pDisp )
{
	IRTCReInviteEvent * pEvent = NULL;
	HRESULT hr = S_OK;

	hr = pDisp->QueryInterface( __uuidof(IRTCReInviteEvent),
		(void **)&pEvent );

	if( FAILED(hr) )
	{
		goto cleanup;
	}

cleanup:

	SafeRelease( pEvent );

	return hr;
}


