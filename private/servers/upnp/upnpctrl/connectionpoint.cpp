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
#include "pch.h"
#pragma hdrstop

#include "Winsock2.h"
#include "Iphlpapi.h"

#include "upnp.h"
#include "ssdpapi.h"
#include "Ncbase.h"
#include "ipsupport.h"
#include "upnp_config.h"

#include "auto_xxx.hxx"
#include "variant.h"
#include "HttpChannel.h"
#include "HttpRequest.h"
#include "ConnectionPoint.h"
#include "CookieJar.h"
#include "dlna.h"
#include "main.h"


ConnectionPoint* g_pConnectionPoint = NULL;
CookieJar<ConnectionPoint::sink,1>    g_CJSink;

ce::SAXReader* ConnectionPoint::sink::m_pReader = NULL;


// ConnectionPoint
ConnectionPoint::ConnectionPoint() :
        m_pThreadPool(NULL),
        m_bInitialized(false)
{
}


// init
HRESULT ConnectionPoint::init()
{
    // create shutdown event
    if ( !m_hEventShuttingDown.valid() )
    {
        m_hEventShuttingDown = CreateEvent(NULL, true, false, NULL);
    }
    if ( !m_hStarted.valid() )
    {
        m_hStarted = CreateEvent(NULL, false, false, NULL);
    }

    if ( !m_hEventShuttingDown.valid() || !m_hStarted.valid() )
    {
        return E_OUTOFMEMORY;
    }

    if ( !m_hMsgQueue.valid() )
    {
        // create message queue
        MSGQUEUEOPTIONS options = {0};
        DWORD           nMsgQueue;

        options.dwSize = sizeof(MSGQUEUEOPTIONS);
        options.dwFlags = MSGQUEUE_ALLOW_BROKEN;
        options.cbMaxMessage = MAX_MSG_SIZE;
        options.bReadAccess = TRUE;
        options.dwMaxMessages = upnp_config::notification_queue_size();

        // random number for a unique queue name
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        nMsgQueue = (DWORD)now.QuadPart;

        if ( !m_strMsgQueueName.reserve(20) )
        {
            return E_OUTOFMEMORY;
        }

        // create a new message queue
        for(DWORD dwError = ERROR_ALREADY_EXISTS; dwError == ERROR_ALREADY_EXISTS; ++nMsgQueue)
        {
            _snwprintf_s(m_strMsgQueueName.get_buffer(), m_strMsgQueueName.capacity(), _TRUNCATE,
                              L"UPnPMsgQueue%x", nMsgQueue);

            m_hMsgQueue = CreateMsgQueue(m_strMsgQueueName, &options);

            dwError = GetLastError();
            if(dwError == ERROR_ALREADY_EXISTS)
            {
                CloseMsgQueue(m_hMsgQueue);
            }
        }

        TraceTag(ttidEvents, "Created new MsgQueue [%s]", m_strMsgQueueName);
        if(!m_hMsgQueue.valid())
        {
            return E_OUTOFMEMORY;
        }
    }

    ce::gate<ce::critical_section> _gate(m_csInit);

    assert(!m_pThreadPool);

    // make sure the listening_thread is not running
    if(WAIT_TIMEOUT == WaitForSingleObject(m_hListeningThread, 5 * 1000))
    {
        return E_FAIL;
    }

    // init thread pool
    if(!(m_pThreadPool = new SVSThreadPool(10))) // max 10 threads
    {
        return E_OUTOFMEMORY;
    }

    ResetEvent(m_hEventShuttingDown);

    // start listening thread
    m_hListeningThread = CreateThread(NULL, 0, &listening_thread, this, 0, NULL);

    if(!m_hListeningThread.valid())
    {
        return E_OUTOFMEMORY;
    }

    WaitForSingleObject(m_hStarted, INFINITE);

    m_bInitialized = true;

    return S_OK;
}


// ~ConnectionPoint
ConnectionPoint::~ConnectionPoint()
{
    uninit();
    CloseMsgQueue(m_hMsgQueue);
    // The cookie jar deletes all the sinks (and their timers)
}


// uninit
void ConnectionPoint::uninit()
{
    ce::gate<ce::critical_section> _gate(m_csInit);

    if(m_hListeningThread.valid())
    {
        // signal event so that listening thread exits
        SetEvent(m_hEventShuttingDown);

        // wait for listening_thread to exit
        DWORD dw = WaitForSingleObject(m_hListeningThread, 60 * 1000);
    }

    delete m_pThreadPool;
    m_pThreadPool = NULL;
    m_bInitialized = false;
}


// advise
HRESULT ConnectionPoint::advise(LPCWSTR pwszUSN, UINT nLifeTime, ICallback* pCallback, DWORD* pdwCookie)
{
    HRESULT hr = S_OK;
    DWORD dwCookie = CJ_NO_COOKIE;
    sink *pSink = NULL;
    int index = -1;

    ChkBool(pCallback != NULL, E_POINTER);
    ChkBool(pdwCookie != NULL, E_POINTER);

    // Do first time init if needed.
    if (!m_bInitialized)
    {
        Chk(init());
    }

    ChkBool(m_bInitialized != false, E_FAIL);
    ChkBool(m_hMsgQueue.valid() != FALSE, E_FAIL);
    ChkBool(m_hListeningThread.valid() != FALSE, E_FAIL);
    ChkBool(m_pThreadPool != FALSE, E_FAIL);

    // Get object and dwCookie
    Chk(g_CJSink.Alloc(&dwCookie));
    Chk(g_CJSink.Acquire(dwCookie, pSink));


    // init and register
    Chk(pSink->init(pwszUSN, pCallback, m_pThreadPool));
    Chk(pSink->register_notification(m_strMsgQueueName));
    pSink->alive(pwszUSN, NULL, NULL, nLifeTime);
    *pdwCookie = dwCookie;

Cleanup:
    g_CJSink.Release(dwCookie, pSink);
    if (FAILED(hr))
    {
        g_CJSink.Free(dwCookie);
    }
    return hr;
}


// unadvise
void ConnectionPoint::unadvise(DWORD dwCookie)
{
    HRESULT hr = S_OK;
    ChkBool(m_bInitialized != false, E_FAIL);

    // one less sink.  
    Chk(g_CJSink.Free(dwCookie));
Cleanup:
    return;
}


// subscribe
HRESULT ConnectionPoint::subscribe(DWORD dwCookie, LPCSTR pszURL)
{
    HRESULT hr = S_OK;
    sink *pSink = NULL;

    ChkBool(m_bInitialized != false, E_FAIL);

    // Get the sink and subscribe, incs ref count
    Chk(g_CJSink.Acquire(dwCookie, pSink));
    if (pSink != NULL)
    {
        hr = pSink->subscribe(pszURL);
    }
Cleanup:
    g_CJSink.Release(dwCookie, pSink);
    return hr;
}


// dispatch_message
void ConnectionPoint::dispatch_message(const event_msg_hdr& hdr)
{
    bool msgFail = FALSE;
    DWORD       dw, dwFlags;
    ce::wstring strEventMessage;
    ce::wstring strUSN;
    ce::wstring strLocation;
    ce::wstring strNLS;
    wchar_t     pwchBuffer[MAX_MSG_SIZE / sizeof(wchar_t)];
    HANDLE      pObjects[2] = {m_hEventShuttingDown, m_hMsgQueue};

    // reserve memory to avoid reallocations
    // keep track of whether string operations on the event message fails
    // we want to keep reading the events from the queue even if there is a local failure
    msgFail = strEventMessage.reserve(hdr.nNumberOfBlocks * MAX_MSG_SIZE / sizeof(wchar_t));

    // read the body of event message
    for(int i = 0; i < hdr.nNumberOfBlocks; ++i)
    {
        dw = WaitForMultipleObjects(sizeof(pObjects)/sizeof(*pObjects), pObjects, false, INFINITE);

        if(WAIT_OBJECT_0 == dw)
        {
            // shutdown event is signaled - return
            return;
        }

        assert(dw == WAIT_OBJECT_0 + 1);

        if(!ReadMsgQueue(m_hMsgQueue, pwchBuffer, sizeof(pwchBuffer), &dw, INFINITE, &dwFlags))
        {
#ifdef DEBUG
            dw = GetLastError();
#endif
            assert(0);

            // should never get here
            // we don't know how to recover so just return
            return;
        }

        if(dw == 1)
        {
            // one byte message is a terminator message
            // writer could not finish writing message for this event, we have to abort
            // next message in the queue will be header for next event

            assert(*((char*)pwchBuffer) == 0);
            TraceTag(ttidError, "Detected terminate message in events msg queue; discarding current event.");
            return;
        }

        switch(hdr.nt)
        {
            case NOTIFY_PROP_CHANGE:
                // append to the event message body
                msgFail = msgFail && strEventMessage.append(pwchBuffer, dw/sizeof(*pwchBuffer));
                break;

            case NOTIFY_ALIVE:
                // two messages in alive body: USN and Location
                switch(i)
                {
                    case 0:
                        // USN
                        msgFail = msgFail && strUSN.assign(pwchBuffer, dw/sizeof(*pwchBuffer));
                        break;

                    case 1:
                        // LOCATION
                        msgFail = msgFail && strLocation.assign(pwchBuffer, dw/sizeof(*pwchBuffer));
                        break;

                    case 2:
                        // NLS
                        msgFail = msgFail && strNLS.assign(pwchBuffer, dw/sizeof(*pwchBuffer));
                        break;

                    default:
                        ASSERT(FALSE);
                }
                break;

            case NOTIFY_BYEBYE:
                // just one message in byebye body: USN
                ASSERT(i == 0);
                msgFail = msgFail && strUSN.assign(pwchBuffer, dw/sizeof(*pwchBuffer));
                break;

            default:
                ASSERT(FALSE);
                break;
        }
    }

    // if there was an error constructing the message then we don't want to attempt to process it
    if(!msgFail)
    {
        TraceTag(ttidError, "Failed reading event/alive/or byebye message into buffer.");
        return;
    }

    // find sink for the subscription
    DWORD dwCookie = CJ_NO_COOKIE;
    sink *pSink = NULL;
    bool bFound = false;
    for (int i = 0; !bFound; i++)
    {
        HRESULT hr = g_CJSink.Iterate(i, &dwCookie);
        if (FAILED(hr))
        {
            break;
        }
        if (hr == S_FALSE || FAILED(g_CJSink.Acquire(dwCookie, pSink)))
        {
            continue;
        }

        // Check the record per our needs
        if (pSink->m_hSubscription == hdr.hSubscription)
        {
            bFound = true;
            switch(hdr.nt)
            {
                case NOTIFY_PROP_CHANGE:
                    pSink->event(strEventMessage, hdr.dwEventSEQ);
                    break;

                case NOTIFY_ALIVE:
                    pSink->alive(strUSN, strLocation, strNLS, hdr.dwLifeTime);
                    break;

                case NOTIFY_BYEBYE:
                    pSink->byebye(strUSN);
                    break;

                default:
                    ASSERT(FALSE);
                    break;
            }
        }

        // Release the lock on the cookie
        g_CJSink.Release(dwCookie, pSink);
    }
}


// listening_thread
DWORD WINAPI ConnectionPoint::listening_thread(void* p)
{
    assert(ConnectionPoint::sink::m_pReader == NULL);

    DWORD errCode = ERROR_SUCCESS;
    HINSTANCE           hLib;
    ConnectionPoint*    pThis = reinterpret_cast<ConnectionPoint*>(p);
    HANDLE              pObjects[2] = {pThis->m_hEventShuttingDown, pThis->m_hMsgQueue};

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    hLib = LoadLibrary(L"upnpctrl.dll");
    if(!hLib)
    {
        errCode = ERROR_OUTOFMEMORY;
        goto Finish;
    }

    SetEvent(pThis->m_hStarted);

    ConnectionPoint::sink::m_pReader = new ce::SAXReader;
    if(!ConnectionPoint::sink::m_pReader)
    {
        errCode = ERROR_OUTOFMEMORY;
        goto Finish;
    }

    if (FAILED(ConnectionPoint::sink::m_pReader->Init()))
    {
        errCode = ERROR_OUTOFMEMORY;
        goto Finish;
    }

    for(;;)
    {
        event_msg_hdr   hdr;
        DWORD           dw, dwFlags;

        dw = WaitForMultipleObjects(sizeof(pObjects)/sizeof(*pObjects), pObjects, false, INFINITE);

        if(WAIT_OBJECT_0 == dw)
        {
            break;
        }

        assert(dw == WAIT_OBJECT_0 + 1);

        // read message header
        if(ReadMsgQueue(pThis->m_hMsgQueue, &hdr, sizeof(hdr), &dw, INFINITE, &dwFlags))
        {
            assert(dw == sizeof(hdr));
            pThis->dispatch_message(hdr);
        }
        else
        {
            // should not get here
            TraceTag(ttidError, "ReadMsgQueue failed on events msg queue. Error code (%d).", GetLastError());
            assert(0);
        }
    }

Finish:
    if(ConnectionPoint::sink::m_pReader)
    {
        delete ConnectionPoint::sink::m_pReader;
        ConnectionPoint::sink::m_pReader = NULL;
    }

    CoUninitialize();

    if(hLib)
    {
        FreeLibraryAndExitThread(hLib, 0);
    }

    return errCode;
}

// ctor and dtor
ConnectionPoint::sink::sink(DWORD dwCookie)
    : m_hSubscription(NULL)
    , m_dwNotificationType(0)
    , m_pCallback(NULL)
    , m_strUSN(L"")
    , m_dwTimeoutSeconds(0)
    , m_dwChannelCookie(0)
    , m_dwSinkCookie(dwCookie)
    , m_SinkState(SINK_STATE_UNINITIALIZED)
    , m_dwEventSEQ(0)
{
}
ConnectionPoint::sink::~sink()
{
    unsubscribe();
    deregister_notification();
    g_CJHttpChannel.Free(m_dwChannelCookie);
    m_dwChannelCookie = 0;
    m_SinkState = SINK_STATE_UNINITIALIZED;
}

void ConnectionPoint::sink::stop_timers()
{
    m_timerAlive.stop();
    m_timerResubscribe.stop();
}

// register_notification
HRESULT ConnectionPoint::sink::register_notification(LPCWSTR pwszMsgQueueName)
{
    // generate unique query string
    LONGLONG uuid64 = GenerateUUID64();

    swprintf(m_pwchQueryString, L"notify%.8x-%.8x", (DWORD)uuid64, *((DWORD *)&uuid64 + 1));
    sprintf(m_pchQueryString, "notify%.8x-%.8x", (DWORD)uuid64, *((DWORD *)&uuid64 + 1));

    // register notification
    m_hSubscription = RegisterNotification(NOTIFY_BYEBYE | NOTIFY_ALIVE | NOTIFY_PROP_CHANGE, m_strUSN, m_pwchQueryString, pwszMsgQueueName);
    if(!m_hSubscription)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    return S_OK;
}


// deregister_notification
void ConnectionPoint::sink::deregister_notification()
{
    // deregister notification
    DeregisterNotification(m_hSubscription);
}


// GetBestIP
// gets HTTP URL and returns best IP address that can be used by device at this URL to call us back
static DWORD GetBestAddress(LPCSTR pszUrl, unsigned nPort, LPSTR pszAddress, int nSize)
{
    char                pszHost[INTERNET_MAX_HOST_NAME_LENGTH];
    URL_COMPONENTSA     urlComp = {0};
    DWORD               result;

    urlComp.dwStructSize = sizeof(URL_COMPONENTS);

    urlComp.lpszHostName = pszHost;
    urlComp.dwHostNameLength = INTERNET_MAX_HOST_NAME_LENGTH;

    if(InternetCrackUrlA(pszUrl, 0, ICU_DECODE, &urlComp))
    {
        struct addrinfo hints = {0}, *ai = NULL;
        DWORD           dwIndex;

        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE;

        if(ERROR_SUCCESS == (result = getaddrinfo(pszHost, "", &hints, &ai)) &&
           ERROR_SUCCESS == (result = GetBestInterfaceEx(ai->ai_addr, &dwIndex)))
        {
            PIP_ADAPTER_ADDRESSES   pAdapterAddresses = NULL;
            DWORD                   ulBufSize;

            // Find out size of returned buffer
            result = GetAdaptersAddresses(
                        ai->ai_addr->sa_family,
                        GAA_FLAG_SKIP_ANYCAST |GAA_FLAG_SKIP_DNS_SERVER,
                        NULL,
                        pAdapterAddresses,
                        &ulBufSize
                        );

            if(ulBufSize)
            {
                // Allocate sufficient Space
                pAdapterAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(malloc(ulBufSize));

                if(pAdapterAddresses !=NULL)
                {
                    // Get Adapter List
                    result = GetAdaptersAddresses(
                                ai->ai_addr->sa_family,
                                GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_MULTICAST,
                                NULL,
                                pAdapterAddresses,
                                &ulBufSize
                                );

                    if(result == ERROR_SUCCESS)
                    {
                        for(PIP_ADAPTER_ADDRESSES pAdapterIter = pAdapterAddresses; pAdapterIter; pAdapterIter = pAdapterIter->Next)
                        {
                            if(ai->ai_addr->sa_family == AF_INET6 && pAdapterIter->Ipv6IfIndex != dwIndex ||
                               ai->ai_addr->sa_family == AF_INET && pAdapterIter->IfIndex != dwIndex)
                            {
                                continue;
                            }

                            PIP_ADAPTER_UNICAST_ADDRESS pUnicastAddress;

                            // found the adapter - get best address
                            for(pUnicastAddress = pAdapterIter->FirstUnicastAddress; pUnicastAddress; pUnicastAddress = pUnicastAddress->Next)
                            {
                                if(ai->ai_addr->sa_family == AF_INET)
                                {
                                    if(pUnicastAddress->Address.lpSockaddr->sa_family == AF_INET)
                                        // if device address is IPv4 use first IPv4 address for subscription callback
                                    {
                                        break;
                                    }
                                }
                                else
                                {
                                    ASSERT(ai->ai_addr->sa_family == AF_INET6);

                                    if(IN6_IS_ADDR_SITELOCAL(&((PSOCKADDR_IN6)ai->ai_addr)->sin6_addr))
                                    {
                                        if(IN6_IS_ADDR_SITELOCAL(&((PSOCKADDR_IN6)pUnicastAddress->Address.lpSockaddr)->sin6_addr))
                                        {
                                            // if device address is site-local use first site-local address for subscription callback
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        if(IN6_IS_ADDR_LINKLOCAL(&((PSOCKADDR_IN6)pUnicastAddress->Address.lpSockaddr)->sin6_addr))
                                            // if device address is other than site-local use first link-local address for subscription callback
                                        {
                                            break;
                                        }
                                    }
                                }
                            }

                            if(pUnicastAddress)
                            {
                                wchar_t pwszAddress[256];

                                if(ai->ai_addr->sa_family == AF_INET6)
                                {
                                    ASSERT(pUnicastAddress->Address.lpSockaddr->sa_family == AF_INET6);

                                    ((PSOCKADDR_IN6)pUnicastAddress->Address.lpSockaddr)->sin6_port = htons(nPort);
                                    ((PSOCKADDR_IN6)pUnicastAddress->Address.lpSockaddr)->sin6_scope_id = 0;
                                }

                                if(ai->ai_addr->sa_family == AF_INET)
                                {
                                    ASSERT(pUnicastAddress->Address.lpSockaddr->sa_family == AF_INET);

                                    ((PSOCKADDR_IN)pUnicastAddress->Address.lpSockaddr)->sin_port = htons(nPort);
                                }

                                result = WSAAddressToString((LPSOCKADDR)pUnicastAddress->Address.lpSockaddr, pUnicastAddress->Address.iSockaddrLength, NULL, pwszAddress, &(ulBufSize = sizeof(pwszAddress)));

                                wcstombs(pszAddress, pwszAddress, nSize);
                            }
                            else
                            {
                                // couldn't find proper address - should never happen
                                result = ERROR_INVALID_DATA;
                            }

                            break;
                        }
                    }

                    free(pAdapterAddresses);
                }
                else
                {
                    result = ERROR_OUTOFMEMORY;
                }
            }
        }

        if(ai)
        {
            freeaddrinfo(ai);
        }
    }
    else
    {
        result = GetLastError();
    }

    return result;
}

// initialize
HRESULT ConnectionPoint::sink::init(LPCWSTR pwszUSN, ICallback* pCallback, SVSThreadPool* pTimer)
{
    HRESULT hr = S_OK;
    ChkBool(pwszUSN != NULL, E_POINTER);
    ChkBool(pCallback != NULL, E_POINTER);
    ChkBool(m_dwChannelCookie == NULL, E_UNEXPECTED);
    Chk(g_CJHttpChannel.Alloc(&m_dwChannelCookie));
    m_pCallback = pCallback;
    m_strUSN = pwszUSN;
    Chk(m_timerAlive.init(pTimer, AliveTimerProc, m_dwSinkCookie));
    Chk(m_timerResubscribe.init(pTimer, ResubscribeTimerProc, m_dwSinkCookie));
    m_SinkState = SINK_STATE_UNSUBSCRIBED;
Cleanup:
    return hr;
}

// subscribe
HRESULT ConnectionPoint::sink::subscribe(LPCSTR pszEventsURL)
{
    char         pszCallback[INTERNET_MAX_URL_LENGTH];
    char         pszIP[256];
    DWORD        dw, result;
    HRESULT      hr = S_OK;
    DWORD dwRequestCookie = 0;
    HttpRequest *pRequest = NULL;

    // Check the input
    ChkBool(pszEventsURL != NULL, E_POINTER);

    // Check the state
    ChkBool(m_SinkState == SINK_STATE_UNSUBSCRIBED, E_FAIL);

    // Record the data
    m_strEventsURL = pszEventsURL;

    // Format callback URL  
    result = GetBestAddress(m_strEventsURL, upnp_config::port(), pszIP, sizeof(pszIP));
    ChkBool((ERROR_SUCCESS == result), HRESULT_FROM_WIN32(result));
    sprintf(pszCallback, "<http://%s/upnpisapi?%s>", pszIP, m_pchQueryString);

    // Prepare SUBSCRIBE Request
    Chk(g_CJHttpRequest.Alloc(&dwRequestCookie));
    Chk(g_CJHttpRequest.Acquire(dwRequestCookie, pRequest));
    Chk(pRequest->Init(m_dwChannelCookie, pszEventsURL));
    Chk(pRequest->Open("SUBSCRIBE"));
    Chk(pRequest->AddHeader("CALLBACK", pszCallback));
    Chk(pRequest->AddHeader("NT", "upnp:event"));
    Chk(pRequest->AddHeader("TIMEOUT", "Second-1800")); // Request subscription for 30 minutes

    // Send Request
    Chk(pRequest->Send());
    ChkBool(hr == S_OK, E_FAIL);

    // Extract timeout value from response
    char pszBuffer[50];
    dw = _countof(pszBuffer);
    Chk(pRequest->GetHeader("TIMEOUT", pszBuffer, &dw));
    ChkBool(1 == sscanf(pszBuffer, "Second-%d", &m_dwTimeoutSeconds), E_FAIL);

    // Extract SID from response
    m_strSID.reserve(48);    // enough for a formatted GUID
    dw = m_strSID.capacity();
    Chk(pRequest->GetHeader("SID", m_strSID.get_buffer(), &dw));

    // Start timer.  leave 7% margin 
    m_timerResubscribe.start(m_dwTimeoutSeconds * 930);

    // Successful transition to the subscribed state
    m_SinkState = SINK_STATE_SUBSCRIBED;
    m_dwEventSEQ = 0;    // Start with expecting sequence 0

Cleanup:
    g_CJHttpRequest.Release(dwRequestCookie, pRequest);
    g_CJHttpRequest.Free(dwRequestCookie);
    return hr;
}


// resubscribe
HRESULT ConnectionPoint::sink::resubscribe()
{
    HRESULT hr = S_OK;
    DWORD dwRequestCookie = 0;
    HttpRequest *pRequest = NULL;

    // Check state
    ChkBool(m_SinkState == SINK_STATE_SUBSCRIBED, E_UNEXPECTED);

    // prepare SUBSCRIBE Request
    Chk(g_CJHttpRequest.Alloc(&dwRequestCookie));
    Chk(g_CJHttpRequest.Acquire(dwRequestCookie, pRequest));
    Chk(pRequest->Init(m_dwChannelCookie, m_strEventsURL));
    Chk(pRequest->Open("SUBSCRIBE"));
    Chk(pRequest->AddHeader("SID", m_strSID));
    Chk(pRequest->AddHeader("TIMEOUT", "Second-1800")); // Request subscription for 30 minutes

    // send Request
    Chk(pRequest->Send());
    ChkBool(hr == S_OK, E_FAIL);

    // Extract timeout from response
    char    pszBuffer[50];
    DWORD   dw = _countof(pszBuffer);
    Chk(pRequest->GetHeader("TIMEOUT", pszBuffer, &dw));
    ChkBool(1 == sscanf(pszBuffer, "Second-%d", &m_dwTimeoutSeconds), E_FAIL);

    // leave 7% margin
    m_timerResubscribe.restart(m_dwTimeoutSeconds * 930);

Cleanup:
    g_CJHttpRequest.Release(dwRequestCookie, pRequest);
    g_CJHttpRequest.Free(dwRequestCookie);
    return hr;
}


// unsubscribe
HRESULT ConnectionPoint::sink::unsubscribe()
{
    HRESULT hr = S_OK;
    ce::string  strSID = m_strSID;
    DWORD dwRequestCookie = 0;
    HttpRequest *pRequest = NULL;

    // Stop what we can
    stop_timers();

    // Check state
    ChkBool(m_SinkState == SINK_STATE_SUBSCRIBED, E_UNEXPECTED);

    // prepare UNSUBSCRIBE Request
    Chk(g_CJHttpRequest.Alloc(&dwRequestCookie));
    Chk(g_CJHttpRequest.Acquire(dwRequestCookie, pRequest));
    Chk(pRequest->Init(m_dwChannelCookie, m_strEventsURL));
    Chk(pRequest->Open("UNSUBSCRIBE"));
    Chk(pRequest->AddHeader("SID", strSID));
    Chk(pRequest->Send());
    ChkBool(hr == S_OK, E_FAIL);

Cleanup:
    g_CJHttpRequest.Release(dwRequestCookie, pRequest);
    g_CJHttpRequest.Free(dwRequestCookie);
    m_SinkState = SINK_STATE_UNSUBSCRIBED;
    return hr;
}


// Fix subscribe
HRESULT ConnectionPoint::sink::fixsubscribe()
{
    HRESULT hr = S_OK;

    unsubscribe();   // best effort
    hr = subscribe(m_strEventsURL);
    if (FAILED(hr))
    {
        byebye();
    }
    return hr;
}


// ResubscribeTimerProc.   This is a static function
DWORD WINAPI ConnectionPoint::sink::ResubscribeTimerProc(void* pvContext)
{
    DWORD dwSinkCookie = reinterpret_cast<DWORD>(pvContext);
    sink *pSink = NULL;
    HRESULT hr = S_OK;

    // Renew
    Chk(g_CJSink.Acquire(dwSinkCookie, pSink));
    if(FAILED(pSink->resubscribe()))
    {
        // "repair" subscription
        pSink->fixsubscribe();
    }
Cleanup:
    g_CJSink.Release(dwSinkCookie, pSink);
    return 0;
}


// event
void ConnectionPoint::sink::event(LPCWSTR pwszEventMessage, DWORD dwEventSEQ)
{
    if (m_SinkState != SINK_STATE_SUBSCRIBED)
    {
        return;
    }

    // calculate next sequence number that we are expecting
    if (dwEventSEQ != 0)
    {
        // Increment the sequence number - skip over 0.
        m_dwEventSEQ++;
        if(m_dwEventSEQ == 0)
        {
            m_dwEventSEQ = 1;
        }
    }

    // not consecutive event SEQ number -> we lost some events
    if(dwEventSEQ != m_dwEventSEQ)
    {
        // "repair" subscription
        fixsubscribe();
    }
    else
    {
        ISAXXMLReader *pXMLReader = NULL;

        if (m_pReader)
        {
            pXMLReader = m_pReader->GetXMLReader();
        }

        // parse event document
        if(pXMLReader)
        {
            HRESULT hr = S_OK;

            pXMLReader->putContentHandler(this);

            hr = pXMLReader->parse(ce::variant(pwszEventMessage));

            if(FAILED(hr))
            {
                TraceTag(ttidError, "SAXXMLReader::parse returned error 0x%08x", hr);
            }

            pXMLReader->putContentHandler(NULL);
        }
    }
}


// alive.  
void ConnectionPoint::sink::alive(LPCWSTR pwszUSN, LPCWSTR pwszLocation, LPCWSTR pwszNLS, DWORD dwLifeTime)
{
    if(dwLifeTime)
    {
        m_timerAlive.stop();
        m_timerAlive.start(dwLifeTime * 1000);
        m_pCallback->AliveNotification(pwszUSN, pwszLocation, pwszNLS, dwLifeTime);
    }
}


// AliveTimerProc.   This is a static function.
DWORD WINAPI ConnectionPoint::sink::AliveTimerProc(VOID *pvContext)
{
    DWORD dwSinkCookie = reinterpret_cast<DWORD>(pvContext);
    sink *pSink = NULL;
    HRESULT hr = S_OK;

    Chk(g_CJSink.Acquire(dwSinkCookie, pSink));
    pSink->byebye();
Cleanup:
    g_CJSink.Release(dwSinkCookie, pSink);
    return 0;
}


// byebye
void ConnectionPoint::sink::byebye(LPCWSTR pwszUSN)
{
    stop_timers();

    if (pwszUSN == NULL)
    {
        pwszUSN = m_strUSN;
    }
    m_pCallback->ServiceInstanceDied(pwszUSN);
}


// startDocument
HRESULT STDMETHODCALLTYPE ConnectionPoint::sink::startDocument(void)
{
    SAXContentHandler::startDocument();

    m_bParsingVariable = false;
    m_bParsingProperty = false;

    return S_OK;
}


// startElement
HRESULT STDMETHODCALLTYPE ConnectionPoint::sink::startElement(
    /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t __RPC_FAR *pwchQName,
    /* [in] */ int cchQName,
    /* [in] */ ISAXAttributes __RPC_FAR *pAttributes)
{
    static wchar_t* pwszPropertyElement =
        L"<urn:schemas-upnp-org:event-1-0>"
        L"<propertyset>"
        L"<urn:schemas-upnp-org:event-1-0>"
        L"<property>";

    SAXContentHandler::startElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName, pAttributes);

    if(m_bParsingProperty)
    {
        m_strName.assign(pwchLocalName, cchLocalName);
        m_strValue.resize(0);

        m_bParsingVariable = true;
        m_bParsingProperty = false;
    }

    if(pwszPropertyElement == m_strFullElementName)
    {
        assert(!m_bParsingVariable);

        m_bParsingProperty = true;
    }

    return S_OK;
}


// endElement
HRESULT STDMETHODCALLTYPE ConnectionPoint::sink::endElement(
    /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t __RPC_FAR *pwchQName,
    /* [in] */ int cchQName)
{
    if(m_bParsingVariable)
    {
        assert(!m_bParsingProperty);
        assert(m_strName.size());

        m_bParsingVariable = false;

        m_strValue.trim(L"\n\r\t ");

        m_pCallback->StateVariableChanged(m_strName, m_strValue);
    }

    return SAXContentHandler::endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);
}


// characters
HRESULT STDMETHODCALLTYPE ConnectionPoint::sink::characters(
    /* [in] */ const wchar_t __RPC_FAR *pwchChars,
    /* [in] */ int cchChars)
{
    if(m_bParsingVariable)
    {
        m_strValue.append(pwchChars, cchChars);
    }

    return S_OK;
}




ConnectionPoint::timer::timer()
    : m_pThreadPool(NULL)
    , m_pfTimerProc(NULL)
    , m_dwTimerID(0)
    , m_dwSinkCookie(0)
{
}
ConnectionPoint::timer::~timer()
{
}

HRESULT ConnectionPoint::timer::init(SVSThreadPool* pThreadPool, LPTHREAD_START_ROUTINE pfTimerProc, DWORD dwSinkCookie)
{
    if (pThreadPool == NULL || pfTimerProc == NULL)
    {
        return E_POINTER;
    }
    m_pThreadPool = pThreadPool;
    m_pfTimerProc = pfTimerProc;
    m_dwSinkCookie = dwSinkCookie;
    return S_OK;
}


// start
void ConnectionPoint::timer::start(DWORD dwTimeout)
{
    ASSERT(m_dwTimerID == 0);
    m_dwTimerID = m_pThreadPool->StartTimer(m_pfTimerProc, (void*)m_dwSinkCookie, dwTimeout);
}

// restart
void ConnectionPoint::timer::restart(DWORD dwTimeout)
{
    ASSERT(m_dwTimerID != 0);
    m_dwTimerID = m_pThreadPool->StartTimer(m_pfTimerProc, (void*)m_dwSinkCookie, dwTimeout);
}

// stop
void ConnectionPoint::timer::stop()
{
    if (m_dwTimerID != 0)
    {
        m_pThreadPool->StopTimer(m_dwTimerID); 
        m_dwTimerID = 0;
    }
}
