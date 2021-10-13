//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
#include "HttpRequest.h"
#include "ConnectionPoint.h"


ConnectionPoint* g_pConnectionPoint;

ce::SAXReader* ConnectionPoint::sink::m_pReader;

// ConnectionPoint
ConnectionPoint::ConnectionPoint()
	: m_pThreadPool(NULL),
      m_bInitialized(false)
{
    // create shutdown event
    m_hEventShuttingDown = CreateEvent(NULL, false, false, NULL);
	m_hStarted = CreateEvent(NULL, false, false, NULL);

    // create message queue
    MSGQUEUEOPTIONS options = {0};
	DWORD           nMsgQueue;
    
    options.dwSize = sizeof(MSGQUEUEOPTIONS);
	options.dwFlags = MSGQUEUE_ALLOW_BROKEN;
	options.cbMaxMessage = MAX_MSG_SIZE;
	options.bReadAccess = TRUE;
    options.dwMaxMessages = upnp_config::notification_queue_size();

	// random number for a unique queue name
    srand(GetTickCount());
	nMsgQueue = rand();
	
	// create a new message queue
	for(DWORD dwError = ERROR_ALREADY_EXISTS; dwError == ERROR_ALREADY_EXISTS; ++nMsgQueue)
	{
		m_strMsgQueueName.reserve(20);
		_snwprintf(m_strMsgQueueName.get_buffer(), m_strMsgQueueName.capacity(), L"UPnPMsgQueue%x", nMsgQueue);
		
		m_hMsgQueue = CreateMsgQueue(m_strMsgQueueName, &options);
		
		dwError = GetLastError();
	}

	assert(m_hMsgQueue.valid());
}


// init
HRESULT ConnectionPoint::init()
{
    ce::gate<ce::critical_section> _gate(m_csInit);

    assert(!m_pThreadPool);

	// make sure the listening_thread is not running
    if(WAIT_TIMEOUT == WaitForSingleObject(m_hListeningThread, 5 * 1000))
        return E_FAIL;
    
    // init thread pool
    if(!(m_pThreadPool = new SVSThreadPool(10))) // max 10 threads
        return E_OUTOFMEMORY;
    
    // start listening thread
	m_hListeningThread = CreateThread(NULL, 0, &listening_thread, this, 0, NULL);

	WaitForSingleObject(m_hStarted, INFINITE);

    m_bInitialized = true;

	return S_OK;
}


// ~ConnectionPoint
ConnectionPoint::~ConnectionPoint()
{
    // all the connections should be unadvised by now but in case there are some left 
	// we need to make sure that timers are stopped before sink objects are destroyed
	// sink object does not stop timer in destructor because we use sink with containers
	for(ce::list<sink>::iterator it = m_listSinks.begin(), itEnd = m_listSinks.end(); it != itEnd; ++it)
    	it->stop_timers();
}


// uninit
void ConnectionPoint::uninit()
{
	ce::gate<ce::critical_section> _gate(m_csInit);
    
    if(m_hListeningThread.valid())
	{
		// signal event so that listening thread exits
		SetEvent(m_hEventShuttingDown);
	}

    delete m_pThreadPool;
    m_pThreadPool = NULL;

    m_bInitialized = false;
}


// advise
HRESULT	ConnectionPoint::advise(LPCWSTR pwszUSN, UINT nLifeTime, ICallback* pCallback, DWORD* pdwCookie)
{
	assert(pCallback);
	assert(pdwCookie);

	HRESULT	hr;

	if(!m_bInitialized)
		if(FAILED(hr = init()))
			return hr;

	assert(m_hMsgQueue.valid());
	assert(m_hListeningThread.valid());
    assert(m_pThreadPool);

	sink s(pwszUSN, pCallback, *m_pThreadPool);

	// register
	if(FAILED(hr = s.register_notification(m_strMsgQueueName)))
		return hr;

	ce::gate<ce::critical_section> _gate(m_csListSinks);

	// add sink to the list
	m_listSinks.push_front(s);

	ce::list<sink>::iterator it = m_listSinks.begin();

	it->alive(pwszUSN, NULL, NULL, nLifeTime);
    
    // return iterator as cookie
	assert(sizeof(ce::list<sink>::iterator) == sizeof(*pdwCookie));
	*((ce::list<sink>::iterator*)pdwCookie) = it;

	return S_OK;
}


// unadvise
void ConnectionPoint::unadvise(DWORD dwCookie)
{
	assert(sizeof(ce::list<sink>::iterator) == sizeof(dwCookie));
	ce::list<sink>::iterator itSink = *((ce::list<sink>::iterator*)&dwCookie);

	ce::gate<ce::critical_section> _gate(m_csListSinks);

	// can't assume that dwCookie is a valid iterator so I look for it in the list
	for(ce::list<sink>::iterator it = m_listSinks.begin(), itEnd = m_listSinks.end(); it != itEnd; ++it)
		if(it == itSink)
		{
			assert(m_bInitialized);
            
            it->stop_timers();
            it->unsubscribe();
			it->deregister_notification();
			
			m_listSinks.erase(it);
			break;
		}

    if(m_bInitialized && m_listSinks.empty())
        uninit();
}


// subscribe
HRESULT ConnectionPoint::subscribe(DWORD dwCookie, LPCSTR pszURL)
{
	assert(sizeof(ce::list<sink>::iterator) == sizeof(dwCookie));
	ce::list<sink>::iterator itSink = *((ce::list<sink>::iterator*)&dwCookie);

	ce::gate<ce::critical_section> _gate(m_csListSinks);

	// can't assume that dwCookie is a valid iterator so I look for it in the list
	for(ce::list<sink>::iterator it = m_listSinks.begin(), itEnd = m_listSinks.end(); it != itEnd; ++it)
		if(it == itSink)
        {
            assert(m_bInitialized);
            
            return it->subscribe(pszURL);
        }

	return E_FAIL;
}


// dispatch_message
void ConnectionPoint::dispatch_message(const event_msg_hdr& hdr)
{
    DWORD		dw, dwFlags;
	ce::wstring	strEventMessage;
    ce::wstring strUSN;
    ce::wstring strLocation;
    ce::wstring strNLS;
	wchar_t		pwchBuffer[MAX_MSG_SIZE / sizeof(wchar_t)];

	// reserve memory to avoid reallocations
	strEventMessage.reserve(hdr.nNumberOfBlocks * MAX_MSG_SIZE / sizeof(wchar_t));
	
	// read the body of event message
	for(int i = 0; i < hdr.nNumberOfBlocks; ++i)
	{
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
		        strEventMessage.append(pwchBuffer, dw/sizeof(*pwchBuffer));
                break;

            case NOTIFY_ALIVE:
                // two messages in byebye body: USN and Location
                switch(i)
                {
                    case 0:
                        // USN
                        strUSN.assign(pwchBuffer, dw/sizeof(*pwchBuffer));
                        break;

                    case 1:
                        // LOCATION
                        strLocation.assign(pwchBuffer, dw/sizeof(*pwchBuffer));
                        break;
                        
                    case 2:
                        // NLS
                        strNLS.assign(pwchBuffer, dw/sizeof(*pwchBuffer));
                        break;

                    default:
                        ASSERT(FALSE);
                }
                break;

            case NOTIFY_BYEBYE:
                // just one message in byebye body: USN
                ASSERT(i == 0);
                strUSN.assign(pwchBuffer, dw/sizeof(*pwchBuffer));
                break;

            default:
                ASSERT(FALSE);
                break;
        }
	}

	ce::gate<ce::critical_section> _gate(m_csListSinks);

	// find sink for the subscription
	for(ce::list<sink>::iterator it = m_listSinks.begin(), itEnd = m_listSinks.end(); it != itEnd; ++it)
		if(it->getSubscriptionHandle() == hdr.hSubscription)
		{
            switch(hdr.nt)
            {
                case NOTIFY_PROP_CHANGE:
                    it->event(strEventMessage, hdr.dwEventSEQ);
                    break;

                case NOTIFY_ALIVE:
                    it->alive(strUSN, strLocation, strNLS, hdr.dwLifeTime);
                    break;

                case NOTIFY_BYEBYE:
                    it->byebye(strUSN);
                    break;

                default:
                    ASSERT(FALSE);
                    break;
            }
			
            break;
		}
}


// listening_thread
DWORD WINAPI ConnectionPoint::listening_thread(void* p)
{
	assert(ConnectionPoint::sink::m_pReader == NULL);
    
    HINSTANCE           hLib;
    ConnectionPoint*    pThis = reinterpret_cast<ConnectionPoint*>(p);
	HANDLE	            pObjects[2] = {pThis->m_hEventShuttingDown, pThis->m_hMsgQueue};

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    
    hLib = LoadLibrary(L"upnpctrl.dll");
    
    SetEvent(pThis->m_hStarted);

    srand(GetTickCount());

    ConnectionPoint::sink::m_pReader = new ce::SAXReader;	
    
    for(;;)
	{
		event_msg_hdr	hdr;
		DWORD			dw, dwFlags;

		dw = WaitForMultipleObjects(sizeof(pObjects)/sizeof(*pObjects), pObjects, false, INFINITE);
		
		if(WAIT_OBJECT_0 == dw)
            break;

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

    delete ConnectionPoint::sink::m_pReader;

    ConnectionPoint::sink::m_pReader = NULL;

    CoUninitialize();
    
    FreeLibraryAndExitThread(hLib, 0);

    return 0;
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
		return HRESULT_FROM_WIN32(GetLastError());

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
	char			    pszHost[INTERNET_MAX_HOST_NAME_LENGTH];
	URL_COMPONENTSA	    urlComp = {0};
	DWORD			    result;

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
                                continue;
                            
                            PIP_ADAPTER_UNICAST_ADDRESS pUnicastAddress;
    
                            // found the adapter - get best address
                            for(pUnicastAddress = pAdapterIter->FirstUnicastAddress; pUnicastAddress; pUnicastAddress = pUnicastAddress->Next)
                            {
                                if(ai->ai_addr->sa_family == AF_INET)
                                {
                                    if(pUnicastAddress->Address.lpSockaddr->sa_family == AF_INET)
                                        // if device address is IPv4 use first IPv4 address for subscription callback
                                        break;
                                }
                                else
                                {    
                                    ASSERT(ai->ai_addr->sa_family == AF_INET6);
                                    
                                    if(IN6_IS_ADDR_SITELOCAL(&((PSOCKADDR_IN6)ai->ai_addr)->sin6_addr))
                                        if(IN6_IS_ADDR_SITELOCAL(&((PSOCKADDR_IN6)pUnicastAddress->Address.lpSockaddr)->sin6_addr))
                                            // if device address is site-local use first site-local address for subscription callback
                                            break;
                                        else;
                                    else
                                        if(IN6_IS_ADDR_LINKLOCAL(&((PSOCKADDR_IN6)pUnicastAddress->Address.lpSockaddr)->sin6_addr))
                                            // if device address is other than site-local use first link-local address for subscription callback
                                            break;
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
								ASSERT(FALSE);

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
            freeaddrinfo(ai);
	}
	else
	{
	    result = GetLastError();
	}

	return result;
}

	
// subscribe
HRESULT ConnectionPoint::sink::subscribe(LPCSTR pszEventsURL /*= NULL*/)
{
	assert(m_strSID.empty());

	HttpRequest request;
	char		pszCallback[INTERNET_MAX_URL_LENGTH];
	char		pszIP[256];
	DWORD		dw, result;
	
    if(pszEventsURL)
        m_strEventsURL = pszEventsURL;
    
    m_bInitialEventReceived = false;

	// format callback URL	
	if(ERROR_SUCCESS != (result = GetBestAddress(m_strEventsURL, upnp_config::port(), pszIP, sizeof(pszIP))))
	    return HRESULT_FROM_WIN32(result);
	
	sprintf(pszCallback, "<http://%s/upnpisapi?%s>", pszIP, m_pchQueryString);
	
	// prepare SUBSCRIBE request
	if(!request.Open("SUBSCRIBE", m_strEventsURL, "HTTP/1.1"))
		return request.GetHresult();

	request.AddHeader("CALLBACK", pszCallback);
	request.AddHeader("NT", "upnp:event");
	request.AddHeader("TIMEOUT", "Second-1800"); // request subscription for 30 minutes

	// send request
	if(!request.Send())
		return request.GetHresult();

	if(HTTP_STATUS_OK != request.GetStatus())
		return E_FAIL; // TO DO: error

	char pszBuffer[50];

	if(!request.GetHeader("TIMEOUT", pszBuffer, &(dw = sizeof(pszBuffer)/sizeof(*pszBuffer))))
		return request.GetHresult();

	if(1 != sscanf(pszBuffer, "Second-%d", &m_dwTimeoutSeconds))
		return E_FAIL; // TO DO: error

	m_strSID.reserve(10);

	if(!request.GetHeader("SID", m_strSID.get_buffer(), &(dw = m_strSID.capacity())))
	{
		m_strSID.reserve(dw);
		
		if(!request.GetHeader("SID", m_strSID.get_buffer(), &(dw = m_strSID.capacity())))
			return request.GetHresult();
    }

	// leave 7% margin
    m_timerResubscribe.start(m_dwTimeoutSeconds * 930, this);

	return S_OK;
}


// resubscribe
HRESULT ConnectionPoint::sink::resubscribe()
{
	assert(!m_strSID.empty());
	
	HttpRequest request;
	
	// prepare SUBSCRIBE request
	if(!request.Open("SUBSCRIBE", m_strEventsURL, "HTTP/1.1"))
		return request.GetHresult();

	// add SID header
	request.AddHeader("SID", m_strSID);
	request.AddHeader("TIMEOUT", "Second-1800"); // request subscription for 30 minutes

	// send request
	if(!request.Send())
		return request.GetHresult();

	if(HTTP_STATUS_PRECOND_FAILED == request.GetStatus())
	{
		// looks like we lost our subscription
		m_strSID.resize(0);

		return subscribe();
	}
	
	if(HTTP_STATUS_OK != request.GetStatus())
		return E_FAIL; // TO DO: error

	DWORD	dw;
	char	pszBuffer[50];

	if(!request.GetHeader("TIMEOUT", pszBuffer, &(dw = sizeof(pszBuffer)/sizeof(*pszBuffer))))
		return request.GetHresult();

	if(1 != sscanf(pszBuffer, "Second-%d", &m_dwTimeoutSeconds))
		return E_FAIL; // TO DO: error

	// leave 7% margin
    m_timerResubscribe.start(m_dwTimeoutSeconds * 930, this);

	return S_OK;
}


// unsubscribe
HRESULT ConnectionPoint::sink::unsubscribe()
{
	if(!m_strSID.empty())
	{
		HttpRequest request;
		ce::string	strSID = m_strSID;

		m_timerResubscribe.stop();
		m_strSID.resize(0);

		// prepare UNSUBSCRIBE request
		if(!request.Open("UNSUBSCRIBE", m_strEventsURL, "HTTP/1.1"))
			return request.GetHresult();

		// add SID header
		request.AddHeader("SID", strSID);

		// send request
		if(!request.Send())
			return request.GetHresult();

		if(HTTP_STATUS_OK != request.GetStatus())
			return E_FAIL; // TO DO: error
	}

	assert(m_strSID.empty());
		
	return S_OK;
}


// ResubscribeTimerProc
DWORD WINAPI ConnectionPoint::sink::ResubscribeTimerProc(void* pvContext)
{
	sink* pThis = reinterpret_cast<sink*>(pvContext);
	
	if(FAILED(pThis->resubscribe()))
	{
		// "repair" subscription
		pThis->unsubscribe();
		
		if(FAILED(pThis->subscribe()))
			// notify application that service is not available
			pThis->byebye(pThis->m_strUSN);
	}

	return 0;
}


// event
void ConnectionPoint::sink::event(LPCWSTR pwszEventMessage, DWORD dwEventSEQ)
{
	if(dwEventSEQ == 0)
	{
		// initial event
		m_dwEventSEQ = 0;
		m_bInitialEventReceived = true;
	}
	else
	{
		m_dwEventSEQ++;
		
		// on overflow wrap to 1
		if(m_dwEventSEQ == 0)
			m_dwEventSEQ = 1;
	}

	// not consecutive event SEQ number -> we lost some events
	if(dwEventSEQ != m_dwEventSEQ || !m_bInitialEventReceived)
	{
		// "repair" subscription
		unsubscribe();
		
		if(FAILED(subscribe()))
			// notify application that service is not available
			byebye(m_strUSN);
	}
	else
		// parse event document
		if(m_pReader->valid())
		{
			(*m_pReader)->putContentHandler(this);

			HRESULT hr = (*m_pReader)->parse(ce::variant(pwszEventMessage));
			
			if(FAILED(hr))
                TraceTag(ttidError, "SAXXMLReader::parse returned error 0x%08x", hr);

            (*m_pReader)->putContentHandler(NULL);
		}
}


// alive
void ConnectionPoint::sink::alive(LPCWSTR pwszUSN, LPCWSTR pwszLocation, LPCWSTR pwszNLS, DWORD dwLifeTime)
{
    if(dwLifeTime)
	{
        m_timerAlive.stop();
        m_timerAlive.start(dwLifeTime * 1000, this);

        m_pCallback->AliveNotification(pwszUSN, pwszLocation, pwszNLS, dwLifeTime);
    }
}


// AliveTimerProc
DWORD WINAPI ConnectionPoint::sink::AliveTimerProc(VOID *pvContext)
{
    sink* pThis = reinterpret_cast<sink*>(pvContext);

    pThis->byebye(pThis->m_strUSN);

    return 0;
}


// byebye
void ConnectionPoint::sink::byebye(LPCWSTR pwszUSN)
{
	stop_timers();

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
		m_strValue.append(pwchChars, cchChars);
	
	return S_OK;
}
