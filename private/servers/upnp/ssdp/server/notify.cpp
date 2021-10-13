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
#include <ssdppch.h>
#pragma hdrstop

#include "http_status.h"
#include "notify.h"


// Forward Declarations
void DebugMessageQueueSize(msg_queue msgQueue, const char* function);


/******************************************************************************
 *  g_pNotificationMgr:
 *
 *      - single notification manager externally referenced
 *      - manages HTTP NOTIFY requests into the upnpsvc
 *
 ******************************************************************************/
notification_mgr* g_pNotificationMgr;



/******************************************************************************
 *  EventsExtensionProc:
 *
 *      - HTTP NOTIFY ISAPI Extension entry point
 *      - passes the NOTIFY message to the notification manager
 *      - sends the HTTP response
 *      
 ******************************************************************************/
DWORD EventsExtensionProc(LPEXTENSION_CONTROL_BLOCK pecb)
{
    if(g_pNotificationMgr)
    {
        pecb->dwHttpStatusCode = g_pNotificationMgr->event(pecb);

        HSE_SEND_HEADER_EX_INFO hse = {0};

        hse.pszStatus = ce::http_status_string(pecb->dwHttpStatusCode);
        hse.cchStatus = strlen(hse.pszStatus);

        // send response
        pecb->ServerSupportFunction(pecb->ConnID, HSE_REQ_SEND_RESPONSE_HEADER_EXV, &hse, NULL, NULL);
    }

    return HSE_STATUS_SUCCESS;
}


// NotifyAliveByebye
bool NotifyAliveByebye(PSSDP_REQUEST pSsdpRequest)
{
    bool retCode = false;

    if(g_pNotificationMgr)
    {
        retCode = g_pNotificationMgr->alive_byebye(pSsdpRequest);
    }

    return retCode;
}


// RegisterNotificationSink
BOOL RegisterNotificationSink(
    __in HANDLE                                  hOwner, 
    __in DWORD                                   dwNotificationType, 
    __in ce::marshal_arg<ce::copy_in, LPCWSTR>   pwszUSN, 
    __in ce::marshal_arg<ce::copy_in, LPCWSTR>   pwszQueryString, 
    __in ce::marshal_arg<ce::copy_in, LPCWSTR>   pwszMsgQueue, 
    __out ce::marshal_arg<ce::copy_out, HANDLE*> phNotify)
{
    // the buffer for the output handle must be valid
    if(!phNotify)
    {
        goto Finish;
    }

    // the buffer for the message queue must be valid
    if(!pwszMsgQueue || !*pwszMsgQueue)
    {
        goto Finish; 
    }

    // the universal service number for alive/bye-bye messages must be valid to determine source
    if(!pwszUSN || !*pwszUSN)
    {
        goto Finish;
    }

    // the query string for event messages must be valid to determine event source
    if(!pwszQueryString || !*pwszQueryString)
    {
        goto Finish;
    }

    if(g_pNotificationMgr) 
    {
        *phNotify = g_pNotificationMgr->register_notification(dwNotificationType, pwszUSN, pwszQueryString, pwszMsgQueue, hOwner);
    }

    return (*phNotify != NULL);

Finish:
    SetLastError(ERROR_INVALID_PARAMETER); 
    return FALSE;
}


// DeregisterNotificationSink
BOOL DeregisterNotificationSink(ce::PSL_HANDLE hSubscription)
{
    if(g_pNotificationMgr) 
    {
        g_pNotificationMgr->deregister_notification((HANDLE)hSubscription);
    }

    return TRUE;
}


// CleanupNotifications
void CleanupNotifications(HANDLE hOwner)
{
    if(g_pNotificationMgr) 
    {
        g_pNotificationMgr->cleanup_notifications(hOwner);
    }
}


// CheckListNotifyForAliveByebye
VOID CheckListNotifyForAliveByebye(PSSDP_REQUEST pSsdpRequest)
{
}



/******************************************************************************
 *  notification_mgr::event:
 *
 *      - handle event notification
 *      - find the associated notification_sink based on the QueryString SID (subscription ID)
 *
 ******************************************************************************/
DWORD notification_mgr::event(LPEXTENSION_CONTROL_BLOCK pecb)
{
    ce::wstring strQueryString;
    DWORD       dw;
    DWORD        dwEventSEQ;
    char        pszHeaderBuf[44] = {0};
    int         status = HTTP_STATUS_PRECOND_FAILED;

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
        if(ERROR_NO_DATA != GetLastError())
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
        if(ERROR_NO_DATA != GetLastError())
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
    if(!pecb->GetServerVariable(pecb->ConnID, "HTTP_SID", pszHeaderBuf, &(dw = sizeof(pszHeaderBuf) - 1))
    || dw < 30)
        if(GetLastError() != ERROR_NO_DATA)
            // missing SID header -> 412 Bad request
            return HTTP_STATUS_PRECOND_FAILED;


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

            ce::wstring    strMessage;

            // assuming UTF8 for content encoding but fallback to ANSI if UTF8 is not available
            if(ce::MultiByteToWideChar(CP_UTF8, (LPCSTR)pecb->lpbData, pecb->cbAvailable, &strMessage))
            {
                // watermark
                if(it->getWatermark().IncrementWatermark())
                {
                    it->event(strMessage, dwEventSEQ);
                    status = HTTP_STATUS_OK;
                }
                else
                {
                    status = HTTP_STATUS_SERVICE_UNAVAIL;
                }
                it->getWatermark().DecrementWatermark();
            }
        }

    return status;
}



/******************************************************************************
 *  notification_mgr::alive_byebye:
 *
 *      - handle alive/byebye notification
 *      - find the associated notification_sink based on the USN
 *
 ******************************************************************************/
bool notification_mgr::alive_byebye(PSSDP_REQUEST pSsdpRequest)
{
    Assert(pSsdpRequest->Headers[SSDP_NTS]);

    bool        bFound = false;
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

            if(it->getWatermark().IncrementWatermark())
            {
                if((it->getType() & NOTIFY_BYEBYE) && 0 == strcmp("ssdp:byebye", pSsdpRequest->Headers[SSDP_NTS]))
                    it->byebye(pSsdpRequest);

                if((it->getType() & NOTIFY_ALIVE) && 0 == strcmp("ssdp:alive", pSsdpRequest->Headers[SSDP_NTS]))
                    it->alive(pSsdpRequest);
            }
            it->getWatermark().DecrementWatermark();
        }

    return bFound;
}



/******************************************************************************
 *  notification_mgr::register_notification:
 *
 *      - registers a new control point for notifications
 *      
 ******************************************************************************/
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
        if(m_listMsgQueues.push_front(msg_queue(pwszMsgQueue, hOwner)))
        {
            it = m_listMsgQueues.begin();

            it->init();
        }
        else
            return NULL;
    }

    notification_sink sink(dwNotificationType, pwszUSN, pwszQueryString, *it, hOwner);

    if(m_listSinks.push_front(sink))
        return m_listSinks.begin()->getHandle();
    else
        return NULL;
}



/******************************************************************************
 *  notification_mgr::deregister_notification:
 *
 *      - deregister notification_sinks associated with the given hSubsription
 *
 ******************************************************************************/
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



/******************************************************************************
 *  notification_mgr::cleanup_notifications:
 *
 *      - removes all notification_sinks created by hOwner from the manager
 *
 ******************************************************************************/
void notification_mgr::cleanup_notifications(HANDLE hOwner)
{
    ce::gate<ce::critical_section> _gate(m_cs);

    // erase sink(s) created by the hOwner
    for(ce::list<notification_sink>::iterator it = m_listSinks.begin(), itEnd = m_listSinks.end(); it != itEnd;)
        if(it->getOwner() == hOwner)
            m_listSinks.erase(it++);
        else
            ++it;

    // erase message queue(s) created by the hOwner
    for(ce::list<msg_queue>::iterator it = m_listMsgQueues.begin(), itEnd = m_listMsgQueues.end(); it != itEnd;)
        if(it->owner() == hOwner)
        {
            it->deinit();
            m_listMsgQueues.erase(it++);
        }
        else
            ++it;
}



/******************************************************************************
 *  notification_sink::ctor
 *
 *      - notification sink constructor
 *
 ******************************************************************************/
const static int MAXIMUM_WATERMARK_LEVEL = 5;
notification_sink::notification_sink(DWORD dwType, LPCWSTR pwszUSN, LPCWSTR pwszQueryString, msg_queue& MsgQueue, HANDLE hOwner)
    : m_dwType(dwType),
      m_strUSN(pwszUSN),
      m_strQueryString(pwszQueryString),
      m_hOwner(hOwner),
      m_MsgQueue(MsgQueue),
      m_Watermark(Watermark(MAXIMUM_WATERMARK_LEVEL, 0))
{
}



/******************************************************************************
 *  notification_sink::event
 *
 *      - performs an event notification
 *      - writes a upnp event to the message queue
 *
 ******************************************************************************/
void notification_sink::event(LPCWSTR pwszMessage, DWORD dwEventSEQ)
{
    ce::gate<ce::critical_section_with_copy> _gate(m_cs);

    DWORD            dwMsgSize = wcslen(pwszMessage) * sizeof(*pwszMessage);
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
            DebugMessageQueueSize(m_MsgQueue, __FUNCTION__);
            return;
        }
    }

    DebugMessageQueueSize(m_MsgQueue, __FUNCTION__);
    TraceTag(ttidError, "%s: Error writing event message to queue. [%d]", __FUNCTION__, GetLastError());
}



/******************************************************************************
 *  notification_sink::byebye
 *
 *      - performs a byebye notification
 *      - writes a byebye event to the message queue
 *
 ******************************************************************************/
void notification_sink::byebye(PSSDP_REQUEST pSsdpRequest)
{
    ce::gate<ce::critical_section_with_copy> _gate(m_cs);

    event_msg_hdr    hdr = {0};
    ce::wstring     strUSN;

    hdr.hSubscription = getHandle();
    hdr.nt = NOTIFY_BYEBYE;
    hdr.nNumberOfBlocks = 1;

    if(!ce::MultiByteToWideChar(CP_ACP, pSsdpRequest->Headers[SSDP_USN], -1, &strUSN))
    {
        TraceTag(ttidError, "%s: Invalid or missing USN header in byebye message", __FUNCTION__);
        return;
    }

    ce::gate<msg_queue> _msg_gate(m_MsgQueue);

    if(!m_MsgQueue.check_terminate_msg())
        return;

    if(WriteMsgQueue(m_MsgQueue, &hdr, sizeof(hdr), 0, 0))
    {
        if(WriteMsgQueue(m_MsgQueue, const_cast<LPWSTR>(static_cast<LPCWSTR>(strUSN)), strUSN.length() * sizeof(wchar_t), 0, 0))
        {
            DebugMessageQueueSize(m_MsgQueue, __FUNCTION__);
            return;
        }

        m_MsgQueue.write_terminate_msg();
    }

    DebugMessageQueueSize(m_MsgQueue, __FUNCTION__);
    TraceTag(ttidError, "%s: Error writing byebye message to queue. [%d]", __FUNCTION__, GetLastError());
}



/******************************************************************************
 *  notification_sink::alive
 *
 *      - performs an alive notification
 *      - writes an alive event to the message queue
 *      
 ******************************************************************************/
void notification_sink::alive(PSSDP_REQUEST pSsdpRequest)
{
    ce::gate<ce::critical_section_with_copy> _gate(m_cs);

    event_msg_hdr    hdr = {0};
    ce::wstring     strUSN;
    ce::wstring     strLocation;
    ce::wstring     strNLS;

    hdr.hSubscription = getHandle();
    hdr.nt = NOTIFY_ALIVE;
    hdr.nNumberOfBlocks = 3;

    if(!ce::MultiByteToWideChar(CP_ACP, pSsdpRequest->Headers[SSDP_USN], -1, &strUSN))
    {
        TraceTag(ttidError, "%s: Invalid or missing USN header in alive message", __FUNCTION__);
        return;
    }

    if(!ce::MultiByteToWideChar(CP_ACP, pSsdpRequest->Headers[SSDP_LOCATION], -1, &strLocation))
    {
        TraceTag(ttidError, "%s: Invalid or missing LOCATION header in alive message", __FUNCTION__);
        return;
    }

    if(!ce::MultiByteToWideChar(CP_ACP, pSsdpRequest->Headers[SSDP_NLS], -1, &strNLS))
    {
        hdr.nNumberOfBlocks = 2;
    }

    if(1 != sscanf(pSsdpRequest->Headers[SSDP_CACHECONTROL], "max-age=%d", &hdr.dwLifeTime))
    {
        TraceTag(ttidError, "%s: Invalid or missing CACHE-CONTROL header in alive message", __FUNCTION__);
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
                DebugMessageQueueSize(m_MsgQueue, __FUNCTION__);
                return;
                }

        m_MsgQueue.write_terminate_msg();
    }

    DebugMessageQueueSize(m_MsgQueue, __FUNCTION__);
    TraceTag(ttidError, "%s: Error writing alive message to queue. [%d]", __FUNCTION__, GetLastError());
}



/******************************************************************************
 *  DebugMessageQueueSize:
 *
 *      - outputs the debug message queue size
 *
 ******************************************************************************/
void DebugMessageQueueSize(msg_queue msgQueue, const char* function)
{
#ifdef DEBUG
    MSGQUEUEINFO msg_info;
    msg_info.dwSize = sizeof(MSGQUEUEINFO);

    if(GetMsgQueueInfo(msgQueue, &msg_info))
    {
        TraceTag(ttidTrace, "%s: msg queue size: %d", function, msg_info.dwCurrentMessages);
    }
#endif
}


/******************************************************************************
 *  init:
 *
 *      - initialize the message queue
 *
 ******************************************************************************/
void msg_queue::init()
{
    MSGQUEUEOPTIONS options = {0};

    //
    // before checking if msgqueque with this name already exists; because of this
    // cbMaxMessage can't be 0
    //
    options.cbMaxMessage = 1;
    options.dwSize = sizeof(MSGQUEUEOPTIONS);

    // create message queue
    m_handle = CreateMsgQueue(m_strName, &options);

#ifdef DEBUG
    DWORD errCode;
    Assert((errCode = GetLastError()) == ERROR_ALREADY_EXISTS);
    if(errCode != ERROR_ALREADY_EXISTS) TraceTag(ttidError, "CreateMsgQueue failed not already exists [%d]", errCode);
    Assert(m_handle.valid());
#endif
}

/******************************************************************************
 *  deinit:
 *
 *      - ensure that the message queue is closed
 *
 ******************************************************************************/
void msg_queue::deinit()
{
    CloseMsgQueue(m_handle);
}

