//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _EVENTIMPL_H
#define _EVENTIMPL_H

//	includes
#include <windows.h>
#include "rtccore.h"
#include "rtcerr.h"


//	forward decl
DWORD WINAPI EventThread( LPVOID pVoid );

//	defines
#define WM_EVENT (WM_USER + 47)

//	inline functions
template < class T > inline void SafeRelease( T& pObj )
{
	if( pObj )
	{
		pObj->Release();
		pObj = NULL;
	}
}

template < class T > inline void SafeFreeString( T& pString )
{
	if( pString )
	{
		SysFreeString( pString );
		pString = NULL;
	}
}


//	Abstract base class.  User must derive a class an implement all functions.
//	Those functions that are not implemented must return E_NOTIMPL.
//	The derived class is passed to CRTCEvents::Advise, and it is used
//	when events are fired.
class CRTCEventsImpl
{
public:
	virtual HRESULT OnRTCClientEvent( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCRegistrationStateChangeEvent( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCSessionStateChangeEvent( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCSessionOperationCompleteEvent( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCParticipantStateChangeEvent( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCMediaEvent( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCIntensityEvent( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCMessagingEvent( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCBuddyEvent( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCWatcherEvent( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCProfileEvent( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCUserSearchResultsEvent( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCInfoEvent( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCBuddyGroupEvent ( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCMediaRequestEvent( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCRoamingEvent( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCPresencePropertyEvent( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCPresenceDataEvent( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCPresenceStatusEvent( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCSessionReferStatusEvent( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCSessionReferredEvent( IDispatch* pDisp ) = 0;
	virtual HRESULT OnRTCReInviteEvent( IDispatch* pDisp ) = 0;

};



// Class to sink RTC events
class CRTCEvents :
	public IRTCEventNotification
{
private:
    DWORD	m_dwRefCount;
    DWORD	m_dwCookie;
	HANDLE	m_hHandlerThread;
	DWORD	m_dwHandlerThreadId;
			

public:
    CRTCEvents() : m_dwRefCount(0),
                   m_dwCookie(0),
				   m_hHandlerThread(NULL),
				   m_dwHandlerThreadId(0)
                   
    {}

    /////////////////////////////////////////////
    //
    // QueryInterface
    // 

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject)
    {
        if (iid == __uuidof(IRTCEventNotification))
        {
            *ppvObject = (void *)this;
            AddRef();
            return S_OK;
        }

        if (iid == IID_IUnknown)
        {
            *ppvObject = (void *)this;
            AddRef();
            return S_OK;
        }

        return E_NOINTERFACE;
    }

    /////////////////////////////////////////////
    //
    // AddRef
    // 

	ULONG STDMETHODCALLTYPE AddRef()
    {
        m_dwRefCount++;
        return m_dwRefCount;
    }
    
    /////////////////////////////////////////////
    //
    // Release
    // 

	ULONG STDMETHODCALLTYPE Release()
    {
        m_dwRefCount--;

        if ( 0 == m_dwRefCount)
        {
            delete this;
        }

        return 1;
    }

    /////////////////////////////////////////////
    //
    // Advise
    // 

    HRESULT Advise(IRTCClient * pClient, CRTCEventsImpl* pEventImpl )
    {    
	    IConnectionPointContainer * pCPC = NULL;
	    IConnectionPoint * pCP = NULL;
        HRESULT hr;

        // Find the connection point container
	    hr = pClient->QueryInterface(IID_IConnectionPointContainer, (void**)&pCPC);

	    if (SUCCEEDED(hr))
        {
            // Find the connection point
		    hr = pCPC->FindConnectionPoint(__uuidof(IRTCEventNotification), &pCP);

            SafeRelease(pCPC);

            if (SUCCEEDED(hr))
            {
                // Advise the connection
		        hr = pCP->Advise(this, &m_dwCookie);
                SafeRelease(pCP);

				if( m_hHandlerThread )
				{
					PostThreadMessage( m_dwHandlerThreadId, WM_QUIT, 0, 0 );
					WaitForSingleObject( m_hHandlerThread, 30000 );

					CloseHandle( m_hHandlerThread );
					m_hHandlerThread = NULL;
					m_dwHandlerThreadId = 0;
				}

				m_hHandlerThread = CreateThread( NULL, 0, EventThread, (LPVOID)pEventImpl, 0, &m_dwHandlerThreadId );
				if( m_hHandlerThread == NULL )
				{
					//	ERROR
				}
            }
        }

	    return hr;
    }

    /////////////////////////////////////////////
    //
    // Unadvise
    // 

    HRESULT Unadvise(IRTCClient * pClient)
    {
	    IConnectionPointContainer * pCPC = NULL;
	    IConnectionPoint * pCP = NULL;
        HRESULT hr = S_OK;

		if( m_hHandlerThread )
		{
			PostThreadMessage( m_dwHandlerThreadId, WM_QUIT, 0, 0 );
			WaitForSingleObject( m_hHandlerThread, 30000 );

			CloseHandle( m_hHandlerThread );
			m_hHandlerThread = NULL;
			m_dwHandlerThreadId = 0;
		}

        if (m_dwCookie)
        {
            // Find the connection point container
	        hr = pClient->QueryInterface(IID_IConnectionPointContainer, (void**)&pCPC);

	        if (SUCCEEDED(hr))
            {
                // Find the connection point
		        hr = pCPC->FindConnectionPoint(__uuidof(IRTCEventNotification), &pCP);

                SafeRelease(pCPC);

                if (SUCCEEDED(hr))
                {
                    // Unadvise this connection
		            hr = pCP->Unadvise(m_dwCookie);

                    SafeRelease(pCP);
                }
            }
        }

	    return hr;
    }

    /////////////////////////////////////////////
    //
    // Event
    // 

	HRESULT STDMETHODCALLTYPE Event(
        RTC_EVENT enEvent,
        IDispatch * pDisp
        )
    {
        // We will post a message containing the event to the
        // handler thread.
        
        // Add a reference to the event so we can hold onto it while
        // the event is in the message queue.

		//	DO WE REALLY NEED TO DO THIS???
        pDisp->AddRef();

        // Post the message
		PostThreadMessage( m_dwHandlerThreadId, WM_EVENT, (WPARAM)enEvent, (LPARAM)pDisp );

        return S_OK;
    }
};


#endif//_EVENTIMPL_H