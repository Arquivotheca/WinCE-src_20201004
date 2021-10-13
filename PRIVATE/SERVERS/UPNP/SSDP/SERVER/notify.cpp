//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <ssdppch.h>
#pragma hdrstop

#include "http_status.h"
#include "notify.h"

notification_mgr* g_pNotificationMgr;

// EventsExtensionProc
DWORD EventsExtensionProc(LPEXTENSION_CONTROL_BLOCK pecb)
{
	pecb->dwHttpStatusCode = g_pNotificationMgr->event(pecb);
    
    HSE_SEND_HEADER_EX_INFO hse = {0};
    
    hse.pszStatus = ce::http_status_string(pecb->dwHttpStatusCode);
    hse.cchStatus = strlen(hse.pszStatus);

	// send response
	pecb->ServerSupportFunction(pecb->ConnID, HSE_REQ_SEND_RESPONSE_HEADER_EX, &hse, NULL, NULL);
	
	return HSE_STATUS_SUCCESS;
}


// NotifyAliveByebye
bool NotifyAliveByebye(PSSDP_REQUEST pSsdpRequest)
{
	return g_pNotificationMgr->alive_byebye(pSsdpRequest);
}


// RegisterNotificationSink
HANDLE RegisterNotificationSink(DWORD dwNotificationType, LPCWSTR pwszUSN, LPCWSTR pwszQueryString, LPCWSTR pwszMsgQueue, HANDLE hOwner)
{
	return g_pNotificationMgr->register_notification(dwNotificationType, pwszUSN, pwszQueryString, pwszMsgQueue, hOwner);
}


// DeregisterNotificationSink
void DeregisterNotificationSink(HANDLE hSubscription)
{
	g_pNotificationMgr->deregister_notification(hSubscription);
}


// CleanupNotifications
void CleanupNotifications(HANDLE hOwner)
{
	g_pNotificationMgr->cleanup_notifications(hOwner);
}


// CheckListNotifyForAliveByebye
VOID CheckListNotifyForAliveByebye(PSSDP_REQUEST pSsdpRequest)
{
}


////////////////////////////////////////////////////////////////////
// notification_mgr

// event
DWORD notification_mgr::event(LPEXTENSION_CONTROL_BLOCK pecb)
{
	ce::wstring strQueryString;
    DWORD       dw;
	DWORD		dwEventSEQ;
	char		pszHeaderBuf[22] = {0};
	int     	status = HTTP_STATUS_PRECOND_FAILED;

    ///////////////////////
	// verify body length
	if(pecb->cbAvailable != pecb->cbTotalBytes)
	    if(pecb->cbTotalBytes == 0)
	        // CONTENT-LENGTH header missing (required by UPnP spec)
	        return HTTP_STATUS_LENGTH_REQUIRED;
	    else
	        // event body larger than data preloaded by web server (48K by default)
	        // reject the event for security reasons
	        return HTTP_STATUS_REQUEST_TOO_LARGE;
	
	///////////////////////
	// verify headers
	
	// NT: upnp:event
	if(!pecb->GetServerVariable(pecb->ConnID, "HTTP_NT", pszHeaderBuf, &(dw = sizeof(pszHeaderBuf) - 1)))
	    if(ERROR_NO_DATA == GetLastError())
	        // missing NT header -> 400 Bad request
	        return HTTP_STATUS_BAD_REQUEST;
	    else
	        // invalid NT header -> 412 Precondition failed
	        return HTTP_STATUS_PRECOND_FAILED;
    else
	    if(strcmp(pszHeaderBuf, "upnp:event"))
	        // invalid NT header -> 412 Precondition failed
	        return HTTP_STATUS_PRECOND_FAILED;
	        
	
	// NTS: upnp:propchange
	if(!pecb->GetServerVariable(pecb->ConnID, "HTTP_NTS", pszHeaderBuf, &(dw = sizeof(pszHeaderBuf) - 1)))
	    if(ERROR_NO_DATA == GetLastError())
	        // missing NT header -> 400 Bad request
	        return HTTP_STATUS_BAD_REQUEST;
	    else
	        // invalid NTS header -> 412 Precondition failed
	        return HTTP_STATUS_PRECOND_FAILED;
    else
   	    if(strcmp(pszHeaderBuf, "upnp:propchange"))
	        // invalid NTS header -> 412 Precondition failed
	        return HTTP_STATUS_PRECOND_FAILED;
	
	
	// SID:
	if(!pecb->GetServerVariable(pecb->ConnID, "HTTP_SID", pszHeaderBuf, &(dw = sizeof(pszHeaderBuf) - 1)))
	    if(GetLastError() == ERROR_NO_DATA)
	        // missing SID header -> 400 Bad request
	        return HTTP_STATUS_BAD_REQUEST;
	
	
	// SEQ:
	if(!pecb->GetServerVariable(pecb->ConnID, "HTTP_SEQ", pszHeaderBuf, &(dw = sizeof(pszHeaderBuf) - 1)))
		// invalid or missing SEQ -> 412 Precondition failed
        return HTTP_STATUS_BAD_REQUEST;
    else
        dwEventSEQ = atoi(pszHeaderBuf);
        
	// convert query string to unicode - it is part of URL so it is limited to ANSI code page
    ce::MultiByteToWideChar(CP_ACP, pecb->lpszQueryString, -1, &strQueryString);

	ce::gate<ce::critical_section> _gate(m_cs);

	// find notification sink for the query string
	for(ce::list<notification_sink>::iterator it = m_listSinks.begin(), itEnd = m_listSinks.end(); it != itEnd; ++it)
		if(it->getQueryString() == strQueryString && (it->getType() & NOTIFY_PROP_CHANGE))
		{
			Assert(pecb->cbAvailable == pecb->cbTotalBytes);

			ce::wstring	strMessage;
    		
			// assuming UTF8 for content encoding but fallback to ANSI if UTF8 is not available
			if(ce::MultiByteToWideChar(CP_UTF8, (LPCSTR)pecb->lpbData, pecb->cbAvailable, &strMessage))
			{
			    it->event(strMessage, dwEventSEQ);
    			
	            status = HTTP_STATUS_OK;
			}
		}

	return status;
}


// alive_byebye
bool notification_mgr::alive_byebye(PSSDP_REQUEST pSsdpRequest)
{
	Assert(pSsdpRequest->Headers[SSDP_NTS]);

	bool		bFound = false;
	ce::wstring strUSN;
    ce::wstring strNT;

	// ANSI code page for USN
	ce::MultiByteToWideChar(CP_ACP, pSsdpRequest->Headers[SSDP_USN], -1, &strUSN);

    // ANSI code page for NT
	ce::MultiByteToWideChar(CP_ACP, pSsdpRequest->Headers[SSDP_NT], -1, &strNT);
	
	ce::gate<ce::critical_section> _gate(m_cs);

	// find notification sink for the Unique Service Name
	for(ce::list<notification_sink>::iterator it = m_listSinks.begin(), itEnd = m_listSinks.end(); it != itEnd; ++it)
		if(it->getUSN() == strUSN || 0 == wcsncmp(it->getUSN(), strNT, wcslen(it->getUSN())))
        {
            bFound = true;

			if((it->getType() & NOTIFY_BYEBYE) && 0 == strcmp("ssdp:byebye", pSsdpRequest->Headers[SSDP_NTS]))
				it->byebye(pSsdpRequest);

			if((it->getType() & NOTIFY_ALIVE) && 0 == strcmp("ssdp:alive", pSsdpRequest->Headers[SSDP_NTS]))
				it->alive(pSsdpRequest);
		}

	return bFound;
}


// register_notification
HANDLE notification_mgr::register_notification(DWORD dwNotificationType, LPCWSTR pwszUSN, LPCWSTR pwszQueryString, LPCWSTR pwszMsgQueue, HANDLE hOwner)
{
	ce::gate<ce::critical_section> _gate(m_cs);

    ce::list<msg_queue>::iterator it, itEnd;

    // find message queue
    for(it = m_listMsgQueues.begin(), itEnd = m_listMsgQueues.end(); it != itEnd; ++it)
        if(it->name() == pwszMsgQueue)
            break;

    if(it == itEnd)
    {
        // not found -> new message queue
        m_listMsgQueues.push_front(msg_queue(pwszMsgQueue));

        it = m_listMsgQueues.begin();

        it->init();
    }

    notification_sink sink(dwNotificationType, pwszUSN, pwszQueryString, *it, hOwner);

	m_listSinks.push_front(sink);

	return m_listSinks.begin()->getHandle();
}


// deregister_notification
void notification_mgr::deregister_notification(HANDLE hSubscription)
{
	ce::gate<ce::critical_section> _gate(m_cs);

	// can't assume that hSubscription is a valid iterator so I look for it in the list
	for(ce::list<notification_sink>::iterator it = m_listSinks.begin(), itEnd = m_listSinks.end(); it != itEnd; ++it)
		if(it->getHandle() == hSubscription)
		{
			m_listSinks.erase(it);
			break;
		}
}


// cleanup_notifications
void notification_mgr::cleanup_notifications(HANDLE hOwner)
{
    ce::gate<ce::critical_section> _gate(m_cs);

    for(ce::list<notification_sink>::iterator it = m_listSinks.begin(), itEnd = m_listSinks.end(); it != itEnd;)
        if(it->getOwner() == hOwner)
			m_listSinks.erase(it++);
        else
            ++it;
}


///////////////////////////////////////////////////////////
// notification_sink

// notification_sink
notification_sink::notification_sink(DWORD dwType, LPCWSTR pwszUSN, LPCWSTR pwszQueryString, msg_queue& MsgQueue, HANDLE hOwner)
	: m_dwType(dwType),
	  m_strUSN(pwszUSN),
	  m_strQueryString(pwszQueryString),
      m_hOwner(hOwner),
	  m_MsgQueue(MsgQueue)
{
}


// event
void notification_sink::event(LPCWSTR pwszMessage, DWORD dwEventSEQ)
{
	ce::gate<ce::critical_section_with_copy> _gate(m_cs);

	DWORD			dwMsgSize = wcslen(pwszMessage) * sizeof(*pwszMessage);
    event_msg_hdr   hdr;

    hdr.hSubscription = getHandle();
	hdr.nt = NOTIFY_PROP_CHANGE;
	hdr.nNumberOfBlocks = (dwMsgSize + MAX_MSG_SIZE - 1) / MAX_MSG_SIZE;
	hdr.dwEventSEQ = dwEventSEQ;

	ce::gate<msg_queue> _msg_gate(m_MsgQueue);
    
    if(!m_MsgQueue.check_terminate_msg())
        return;
    
    // write header to message queue
	if(WriteMsgQueue(m_MsgQueue, &hdr, sizeof(hdr), 0, 0))
	{
		int i;

        // write event message body to message queue
		for(i = 0; i < hdr.nNumberOfBlocks; ++i)
		{
			// message is unicode text so the length is even
			Assert(dwMsgSize / 2 == (dwMsgSize + 1) / 2);
			
			// MAX_MSG_SIZE is also even
			Assert(MAX_MSG_SIZE / 2 == (MAX_MSG_SIZE + 1) / 2);
			
			// ... this means there can't be a valid message of 1 byte - we use 1 byte message as terminator message
			Assert(__min(MAX_MSG_SIZE, dwMsgSize - i * MAX_MSG_SIZE) != 1);

			if(!WriteMsgQueue(m_MsgQueue, (char*)pwszMessage + i * MAX_MSG_SIZE, __min(MAX_MSG_SIZE, dwMsgSize - i * MAX_MSG_SIZE), 0, 0))
			{
				m_MsgQueue.write_terminate_msg();
				break;
			}
		}

        if(i == hdr.nNumberOfBlocks)
        {
            // all parts of the message written
#ifdef DEBUG
            MSGQUEUEINFO msg_info;
            
            msg_info.dwSize = sizeof(MSGQUEUEINFO);
            
            if(GetMsgQueueInfo(m_MsgQueue, &msg_info))
                TraceTag(ttidTrace, "notification_sink::event: msg queue size: %d", msg_info.dwCurrentMessages);
#endif
            return;
        }
	}

    TraceTag(ttidError, "Error writing event message to queue");
}


// byebye
void notification_sink::byebye(PSSDP_REQUEST pSsdpRequest)
{
	ce::gate<ce::critical_section_with_copy> _gate(m_cs);

    event_msg_hdr	hdr = {0};
    ce::wstring     strUSN;

	hdr.hSubscription = getHandle();
	hdr.nt = NOTIFY_BYEBYE;
    hdr.nNumberOfBlocks = 1;

    if(!ce::MultiByteToWideChar(CP_ACP, pSsdpRequest->Headers[SSDP_USN], -1, &strUSN))
    {
        TraceTag(ttidError, "Invalid or missing USN header in byebye message");
        return;
    }

    ce::gate<msg_queue> _msg_gate(m_MsgQueue);
    
    if(!m_MsgQueue.check_terminate_msg())
        return;
    
    if(WriteMsgQueue(m_MsgQueue, &hdr, sizeof(hdr), 0, 0))
    {
        if(WriteMsgQueue(m_MsgQueue, const_cast<LPWSTR>(static_cast<LPCWSTR>(strUSN)), strUSN.length() * sizeof(wchar_t), 0, 0))
        {
#ifdef DEBUG
            MSGQUEUEINFO msg_info;
            
            msg_info.dwSize = sizeof(MSGQUEUEINFO);
            
            if(GetMsgQueueInfo(m_MsgQueue, &msg_info))
                TraceTag(ttidTrace, "notification_sink::byebye: msg queue size: %d", msg_info.dwCurrentMessages);
#endif            
            return;
        }

        m_MsgQueue.write_terminate_msg();
    }
	
    TraceTag(ttidError, "Error writing byebye message to queue");
}


// alive
void notification_sink::alive(PSSDP_REQUEST pSsdpRequest)
{
	ce::gate<ce::critical_section_with_copy> _gate(m_cs);

    event_msg_hdr	hdr = {0};
    ce::wstring     strUSN;
    ce::wstring     strLocation;
    ce::wstring     strNLS;

	hdr.hSubscription = getHandle();
	hdr.nt = NOTIFY_ALIVE;
    hdr.nNumberOfBlocks = 3;

    if(!ce::MultiByteToWideChar(CP_ACP, pSsdpRequest->Headers[SSDP_USN], -1, &strUSN))
    {
        TraceTag(ttidError, "Invalid or missing USN header in alive message");
        return;
    }

    if(!ce::MultiByteToWideChar(CP_ACP, pSsdpRequest->Headers[SSDP_LOCATION], -1, &strLocation))
    {
        TraceTag(ttidError, "Invalid or missing LOCATION header in alive message");
        return;
    }
    
    if(!ce::MultiByteToWideChar(CP_ACP, pSsdpRequest->Headers[SSDP_NLS], -1, &strNLS))
    {
        hdr.nNumberOfBlocks = 2;
    }
    
    if(1 != sscanf(pSsdpRequest->Headers[SSDP_CACHECONTROL], "max-age=%d", &hdr.dwLifeTime))
    {
        TraceTag(ttidError, "Invalid or missing CACHE-CONTROL header in alive message");
        return;
    }

    ce::gate<msg_queue> _msg_gate(m_MsgQueue);
    
    if(!m_MsgQueue.check_terminate_msg())
        return;
    
    if(WriteMsgQueue(m_MsgQueue, &hdr, sizeof(hdr), 0, 0))
    {
        if(WriteMsgQueue(m_MsgQueue, const_cast<LPWSTR>(static_cast<LPCWSTR>(strUSN)), strUSN.length() * sizeof(wchar_t), 0, 0))
            if(WriteMsgQueue(m_MsgQueue, const_cast<LPWSTR>(static_cast<LPCWSTR>(strLocation)), strLocation.length() * sizeof(wchar_t), 0, 0))
                if(hdr.nNumberOfBlocks == 2 || WriteMsgQueue(m_MsgQueue, const_cast<LPWSTR>(static_cast<LPCWSTR>(strNLS)), strNLS.length() * sizeof(wchar_t), 0, 0))
                {
#ifdef DEBUG
                    MSGQUEUEINFO msg_info;
                    
                    msg_info.dwSize = sizeof(MSGQUEUEINFO);
            
                    if(GetMsgQueueInfo(m_MsgQueue, &msg_info))
                        TraceTag(ttidTrace, "notification_sink::alive: msg queue size: %d", msg_info.dwCurrentMessages);
#endif                    
                    return;
                }
        
        m_MsgQueue.write_terminate_msg();
    }
    
    TraceTag(ttidError, "Error writing alive message to queue");
}
