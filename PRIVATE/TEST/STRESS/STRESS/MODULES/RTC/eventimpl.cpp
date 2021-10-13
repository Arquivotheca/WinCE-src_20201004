//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "eventimpl.h"



DWORD WINAPI EventThread( LPVOID pVoid )
{
	CRTCEventsImpl* pEventImpl = (CRTCEventsImpl*)pVoid;
	MSG			sMsg;
	IDispatch*  pDisp = NULL;
	RTC_EVENT	nEvent;

	if( pEventImpl == NULL )
		return 1;

	CoInitialize( NULL );

	//	enter a message loop
	//	the IRTCEventNotification implementation will post messages to us for every event fired
	//	(that is, events we are registered to receive)

	while( GetMessage( &sMsg, NULL, 0, 0 ) )
	{
		if( sMsg.message == WM_EVENT )
		{
			nEvent = (RTC_EVENT)sMsg.wParam;
			pDisp = (IDispatch*)sMsg.lParam;

			switch( nEvent )
			{
			case RTCE_CLIENT:
				{					
					pEventImpl->OnRTCClientEvent( pDisp );
					break;
				}

			case RTCE_REGISTRATION_STATE_CHANGE:
				{					
					pEventImpl->OnRTCRegistrationStateChangeEvent( pDisp );
					break;
				}

			case RTCE_SESSION_STATE_CHANGE:
				{
					pEventImpl->OnRTCSessionStateChangeEvent( pDisp );
					break;
				}

			case RTCE_SESSION_OPERATION_COMPLETE:
				{			
					pEventImpl->OnRTCSessionOperationCompleteEvent( pDisp );
					break;
				}

			case RTCE_PARTICIPANT_STATE_CHANGE:
				{
					pEventImpl->OnRTCParticipantStateChangeEvent( pDisp );
					break;
				}

			case RTCE_MEDIA:
				{
					pEventImpl->OnRTCMediaEvent( pDisp );
					break;
				}

			case RTCE_INTENSITY:
				{
					pEventImpl->OnRTCIntensityEvent( pDisp );
					break;
				}

			case RTCE_MESSAGING:
				{
					pEventImpl->OnRTCMessagingEvent( pDisp );
					break;
				}

			case RTCE_BUDDY:
				{
					pEventImpl->OnRTCBuddyEvent( pDisp );
					break;
				}

			case RTCE_WATCHER:
				{
					pEventImpl->OnRTCWatcherEvent( pDisp );
					break;
				}

			case RTCE_PROFILE:
				{
					pEventImpl->OnRTCProfileEvent( pDisp );
					break;
				}

			case RTCE_USERSEARCH:
				{					
					pEventImpl->OnRTCUserSearchResultsEvent( pDisp );
					break;
				}

			case RTCE_INFO:
				{
					pEventImpl->OnRTCInfoEvent( pDisp );
					break;
				}

			case RTCE_GROUP:
				{
					pEventImpl->OnRTCBuddyGroupEvent( pDisp );
					break;
				}

			case RTCE_MEDIA_REQUEST:
				{
					pEventImpl->OnRTCMediaRequestEvent( pDisp );
					break;
				}

			case RTCE_ROAMING:
				{
					pEventImpl->OnRTCRoamingEvent( pDisp );
					break;
				}

			case RTCE_PRESENCE_PROPERTY:
				{
					pEventImpl->OnRTCPresencePropertyEvent( pDisp );
					break;
				}

			case RTCE_PRESENCE_DATA:
				{
					pEventImpl->OnRTCPresenceDataEvent( pDisp );
					break;
				}

			case RTCE_PRESENCE_STATUS:
				{
					pEventImpl->OnRTCPresenceStatusEvent( pDisp );
					break;
				}

			case RTCE_SESSION_REFER_STATUS:
				{
					pEventImpl->OnRTCSessionReferStatusEvent( pDisp );
					break;
				}

			case RTCE_SESSION_REFERRED:
				{
					pEventImpl->OnRTCSessionReferredEvent( pDisp );
					break;
				}

			case RTCE_REINVITE:
				{
					pEventImpl->OnRTCReInviteEvent( pDisp );
					break;
				}
			}

			SafeRelease( pDisp );            			
		}
		
	}

	CoUninitialize();
	return 0;
}
