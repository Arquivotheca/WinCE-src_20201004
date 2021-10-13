//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _SERVER_H
#define _SERVER_H

//	includes
#include "eventimpl.h"

class CServerEventImpl : public CRTCEventsImpl
{
public:
	CServerEventImpl();
	~CServerEventImpl();
	HANDLE		GetShutdownEvent();

	void		SetClient( IRTCClient* pClient );
	IRTCClient*	GetClient();

	void		SetEvents( CRTCEvents* pEvents );
	CRTCEvents*	GetEvents();

	HRESULT 	OnRTCClientEvent( IDispatch* pDisp );
	HRESULT 	OnRTCRegistrationStateChangeEvent( IDispatch* pDisp );
	HRESULT 	OnRTCSessionStateChangeEvent( IDispatch* pDisp );
	HRESULT 	OnRTCSessionOperationCompleteEvent( IDispatch* pDisp );
	HRESULT 	OnRTCParticipantStateChangeEvent( IDispatch* pDisp );
	HRESULT 	OnRTCMediaEvent( IDispatch* pDisp );
	HRESULT 	OnRTCIntensityEvent( IDispatch* pDisp );
	HRESULT 	OnRTCMessagingEvent( IDispatch* pDisp );
	HRESULT 	OnRTCBuddyEvent( IDispatch* pDisp );
	HRESULT 	OnRTCWatcherEvent( IDispatch* pDisp );
	HRESULT 	OnRTCProfileEvent( IDispatch* pDisp );
	HRESULT 	OnRTCUserSearchResultsEvent( IDispatch* pDisp );
	HRESULT 	OnRTCInfoEvent( IDispatch* pDisp );
	HRESULT 	OnRTCBuddyGroupEvent ( IDispatch* pDisp );
	HRESULT 	OnRTCMediaRequestEvent( IDispatch* pDisp );
	HRESULT 	OnRTCRoamingEvent( IDispatch* pDisp );
	HRESULT 	OnRTCPresencePropertyEvent( IDispatch* pDisp );
	HRESULT 	OnRTCPresenceDataEvent( IDispatch* pDisp );
	HRESULT 	OnRTCPresenceStatusEvent( IDispatch* pDisp );
	HRESULT 	OnRTCSessionReferStatusEvent( IDispatch* pDisp );
	HRESULT 	OnRTCSessionReferredEvent( IDispatch* pDisp );
	HRESULT 	OnRTCReInviteEvent( IDispatch* pDisp );

private:
	HANDLE		m_hShutdownComplete;
	IRTCClient*	m_pClient;
	CRTCEvents* m_pEvents;
	long		m_nMsgCookie;


};
#endif//_SERVER_H