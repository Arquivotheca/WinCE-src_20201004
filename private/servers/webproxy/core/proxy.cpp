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

#include "proxy.h"
#include "proxydev.h"
#include "auth.h"
#include "proxydbg.h"
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include "resource.h"
#include "parser.h"
#include "filter.h"

ProxySettings* g_pSettings;
ProxyErrors* g_pErrors;
SVSThreadPool* g_pThreadPool;


DWORD InitStrings(void);
void DeinitStrings(void);

//
// The following methods belong to the CHttpProxy class which is the central
// class for the Web Proxy.
//

CHttpProxy::CHttpProxy(void)
{
}

DWORD CHttpProxy::Init(void)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    WSADATA wsadata;    
    
    svsutil_Initialize();
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    g_pErrors = new ProxyErrors;
    if (! g_pErrors) {
        dwRetVal = ERROR_OUTOFMEMORY;
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Out of memory.\n")));
        goto exit;
    }

    g_pSettings = new ProxySettings;
    if (! g_pSettings) {
        dwRetVal = ERROR_OUTOFMEMORY;
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Out of memory.\n")));
        goto exit;
    }

    g_pParser = new CHttpParser;
    if (! g_pParser) {
        dwRetVal = ERROR_OUTOFMEMORY;
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Out of memory.\n")));
        goto exit;
    }

    g_pSessionMgr = new CSessionMgr;
    if (! g_pSessionMgr) {
        dwRetVal = ERROR_OUTOFMEMORY;
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Out of memory.\n")));
        goto exit;
    }

    g_pProxyFilter = new CProxyFilter;
    if (! g_pProxyFilter) {
        dwRetVal = ERROR_OUTOFMEMORY;
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Out of memory.\n")));
        goto exit;
    }

    dwRetVal = InitStrings();
    if (ERROR_SUCCESS != dwRetVal) {
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error initializing proxy strings.\n")));
        goto exit;
    }
    
    m_fRunning = FALSE;
    IFDBG(DebugOut(ZONE_SERVICE, _T("WebProxy: Service has been initialized successfully.\n")));

exit:
    if (ERROR_SUCCESS != dwRetVal) {
        if (g_pProxyFilter) {
            delete g_pProxyFilter;
            g_pProxyFilter = NULL;
        }
        if (g_pParser) {
            delete g_pParser;
            g_pParser = NULL;
        }
        if (g_pSessionMgr) {
            delete g_pSessionMgr;
            g_pSessionMgr = NULL;
        }
    }
    
    return dwRetVal;
}

DWORD CHttpProxy::Deinit(void)
{    
    DeinitStrings();
    
    delete g_pProxyFilter;
    g_pProxyFilter = NULL;
    delete g_pSessionMgr;
    g_pSessionMgr = NULL;
    delete g_pParser;
    g_pParser = NULL;
    
    WSACleanup();    
    svsutil_DeInitialize();

    IFDBG(DebugOut(ZONE_SERVICE, _T("WebProxy: Service has been deinitialized successfully.\n")));
    return ERROR_SUCCESS;
}

DWORD CHttpProxy::InitSettings(void)
{
    DWORD dwRetVal = ERROR_SUCCESS;    
    CReg regWebProxy;    
    
    //
    // Set the default values for all registry configurable settings
    //

    DWORD cSessionThreads = DEFAULT_MAX_CONNECTIONS;

    g_pSettings->Reset();

    m_fRunning = FALSE;

    //
    // If we fail to open the registry key, that is ok as it is possible to desire all
    // the default settings.
    //
    
    if (regWebProxy.Open(HKEY_LOCAL_MACHINE, RK_WEBPROXY)) {
        WCHAR* pszTmp;

        g_pSettings->iMaxConnections = regWebProxy.ValueDW(RV_MAX_CONNECTIONS, DEFAULT_MAX_CONNECTIONS);
        g_pSettings->iPort = regWebProxy.ValueDW(RV_PORT, DEFAULT_PROXY_PORT);
        g_pSettings->fNTLMAuth = regWebProxy.ValueDW(RV_AUTH_NTLM, DEFAULT_AUTH_NTLM);
        g_pSettings->fBasicAuth = regWebProxy.ValueDW(RV_AUTH_BASIC, DEFAULT_AUTH_BASIC);
        g_pSettings->iSessionTimeout = regWebProxy.ValueDW(RV_SESSION_TIMEOUT, DEFAULT_SESSION_TIMEOUT);
        g_pSettings->iMaxBufferSize = regWebProxy.ValueDW(RV_MAXHEADERSSIZE, DEFAULT_MAX_BUFFER_SIZE);
        g_pSettings->iSecondProxyPort = regWebProxy.ValueDW(RV_SECOND_PROXY_PORT, DEFAULT_HTTP_PORT);

        cSessionThreads = regWebProxy.ValueDW(RV_SESSION_THREADS, g_pSettings->iMaxConnections);
  
        pszTmp = (WCHAR *) regWebProxy.ValueSZ(RV_SECOND_PROXY_HOST);
        if (pszTmp) {
            DWORD cchTmp = wcslen(pszTmp);
            g_pSettings->strSecondProxyHost.reserve(cchTmp+1);
            WideCharToMultiByte(CP_ACP, 0, pszTmp, -1, g_pSettings->strSecondProxyHost.get_buffer(), g_pSettings->strSecondProxyHost.capacity(), NULL, NULL);
        }
    }

    

    g_pThreadPool = new SVSThreadPool(cSessionThreads);
    if (! g_pThreadPool) {
        dwRetVal = ERROR_OUTOFMEMORY;
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Out of memory allocating SVSThreadPool.\n")));
        goto exit;
   }

    g_pSettings->strHostName.reserve(MAX_PATH);
    dwRetVal = gethostname(g_pSettings->strHostName.get_buffer(), g_pSettings->strHostName.capacity());
    if (ERROR_SUCCESS != dwRetVal) {
        g_pSettings->strHostName = DEFAULT_HOST_NAME_SZ;
    }

    g_pProxyFilter->LoadFilters();
    
    if (g_pSettings->fNTLMAuth) {
        dwRetVal = InitNTLMSecurityLib();
        if (ERROR_SUCCESS != dwRetVal) {
            goto exit;
        }
    }

    if (g_pSettings->fBasicAuth) {
        dwRetVal = InitBasicAuth();
        if (ERROR_SUCCESS != dwRetVal) {
            goto exit;
        }
    }

exit:
    if (ERROR_SUCCESS != dwRetVal) {
        g_pProxyFilter->RemoveAllFilters();
        
        if (g_pThreadPool) {
            delete g_pThreadPool;
            g_pThreadPool = NULL;
        }        
    }
    
    return dwRetVal;
}

DWORD CHttpProxy::DeinitSettings(void)
{   
    if (g_pSettings->fNTLMAuth) {
        DeinitNTLMSecurityLib();
    }
    if (g_pSettings->fBasicAuth) {
        DeinitBasicAuth();
    }

    g_pProxyFilter->RemoveAllFilters();

    delete g_pThreadPool;
    g_pThreadPool = NULL;
    
    return ERROR_SUCCESS;
}

DWORD CHttpProxy::Start(void)
{
    DWORD dwRetVal = ERROR_SUCCESS;

    Lock();

    InitSettings();
    m_fRunning = TRUE;

    UpdateAdapterInfo();

    m_hServerThread = CreateThread(NULL, 0, ServerThread, (LPVOID)this, 0, NULL);
    if (! m_hServerThread.valid()) {
        dwRetVal = GetLastError();
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error creating ServerThread: %d.\n"), dwRetVal));
        goto exit;
    }
    
    IFDBG(DebugOut(ZONE_SERVICE, _T("WebProxy: Service has been started successfully.\n")));

exit:
    Unlock();
    return dwRetVal;
}

DWORD CHttpProxy::Stop(void)
{
    DWORD dwRetVal = ERROR_SUCCESS;

    Lock();
    m_fRunning = FALSE;

    for (int i = 0; i < m_cSockets; i++) {
        m_sockServer[i].close();
    }
    m_cSockets = 0;

    // Need to shutdown each item in the list, then shutdown thread pool, 
    // then remove the items from the list.
    g_pSessionMgr->ShutdownAllSessions();
    g_pThreadPool->Shutdown();
    g_pSessionMgr->RemoveAllSessions();
    
    // If this function is being called from the ServerThread then we should not wait
    if ((HANDLE)GetCurrentThreadId() != m_hServerThread) {
        HANDLE h = m_hServerThread;
        Unlock();
        dwRetVal = WaitForSingleObject(h, INFINITE);
        Lock();
        if (WAIT_FAILED == dwRetVal) {
            dwRetVal = GetLastError();
            IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error stopping ServerThread: %d.\n"), dwRetVal));
            ASSERT(0);
            goto exit;
        }
    }

    DeinitSettings();
    
    dwRetVal = ERROR_SUCCESS;
    IFDBG(DebugOut(ZONE_SERVICE, _T("WebProxy: Service has been stopped successfully.\n")));

exit:
    Unlock();
    return dwRetVal;
}

DWORD CHttpProxy::NotifyAddrChange(void)
{
    IFDBG(DebugOut(ZONE_SERVICE, _T("WebProxy: Received address change notification.\n")));
    
    UpdateAdapterInfo();
    return ERROR_SUCCESS;
}

DWORD CHttpProxy::SignalFilter(DWORD dwSignal)
{
    return g_pProxyFilter->Signal(dwSignal);
}

DWORD WINAPI CHttpProxy::ServerThread(LPVOID pv)
{
    CHttpProxy* pInst = reinterpret_cast<CHttpProxy*> (pv);
    pInst->Run();
    return 0;
}

void CHttpProxy::Run(void)
{
    DWORD dwErr = ERROR_SUCCESS;

    Lock();

    dwErr = SetupServerSockets();
    if (ERROR_SUCCESS != dwErr) {
        Stop();
        g_dwState = SERVICE_STATE_OFF;
        goto exit;
    }

    while (1) {
        int i = 0;
        fd_set sockSet;
        
        FD_ZERO(&sockSet);
        for (i = 0; i < m_cSockets; i++) {
            FD_SET(m_sockServer[i], &sockSet);
        }

        Unlock();

        if (SERVICE_STATE_ON != g_dwState) {
            // Another thread has indicated that the program should be terminated
            IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Web Proxy main thread will not continue because another thread has changed service state.\n")));
            // do not break out of loop or goto exit block, but quit immediatly.
            // Also no need to Stop() because the other thread shutting us down has performed this already
            return;  
        }

        int iSockets = select(0,&sockSet,NULL,NULL,NULL);
        Lock();

        // Check if service is being signalled to stop
        if (! m_fRunning) {
            IFDBG(DebugOut(ZONE_SERVICE, _T("WebProxy: Server thread is being signalled to stop, exiting...\n")));
            break;
        }

        if ((0 == iSockets) || (SOCKET_ERROR == iSockets)) {
            IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error in call to select: %d\n"), WSAGetLastError()));
            goto exit;
        }

        for (i = 0; i < iSockets; i++) {
            SOCKADDR_STORAGE saClient;
            int cbsa = sizeof(saClient);
            
            auto_socket sockClient = accept(sockSet.fd_array[i], (SOCKADDR*)&saClient, &cbsa);

            if (INVALID_SOCKET == sockClient) {
                IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Accept on server socket failed: %d\n"), WSAGetLastError()));
                continue;
            }

            // Check if we have exceeded the maximum number of connections
            if (g_pSessionMgr->GetSessionCount() > g_pSettings->iMaxConnections) {
                sockClient.close();
                IFDBG(DebugOut(ZONE_WARN, _T("WebProxy: Cannot accept any new connections - connection limit has been reached.\n")));
                continue;
            }

            SessionSettings settings;

            memset(&settings, 0, sizeof(settings));
            settings.sockClient = sockClient;
            settings.saClient = saClient;

            // Start the HTTP session and insert it in the list
            dwErr = g_pSessionMgr->StartSession(&settings, NULL);
            if (ERROR_SUCCESS != dwErr) {
                sockClient.close();
                IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error adding session to list %d\n"), dwErr));
                continue;
            }
        }
    }
    
exit:
    Unlock();
    return;
}

DWORD CHttpProxy::SetupServerSockets()
{
    DWORD dwRetVal = ERROR_SUCCESS;
    ADDRINFO aiHints;
    ADDRINFO* paiLocal = NULL;
    ADDRINFO* paiTrav = NULL;
    CHAR szPort[16];

    _itoa(g_pSettings->iPort, szPort, 10);

    memset(&aiHints, 0, sizeof(aiHints));
    aiHints.ai_family = PF_UNSPEC;
    aiHints.ai_socktype = SOCK_STREAM;
    aiHints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;

    dwRetVal = getaddrinfo(NULL, szPort, &aiHints, &paiLocal);
    if (ERROR_SUCCESS != dwRetVal) {
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error getting addr info: %d.\n"), dwRetVal));
        goto exit;
    }

    m_cSockets = 0;

    for (paiTrav = paiLocal; paiTrav && (m_cSockets < MAX_SOCKET_LIST); paiTrav = paiTrav->ai_next) {
        m_sockServer[m_cSockets] = socket(paiTrav->ai_family, paiTrav->ai_socktype, paiTrav->ai_protocol);
        if (INVALID_SOCKET == m_sockServer[m_cSockets]) {
            continue;
        }

        if (SOCKET_ERROR == bind(m_sockServer[m_cSockets], paiTrav->ai_addr, paiTrav->ai_addrlen)) {
            m_sockServer[m_cSockets].close();
            dwRetVal = WSAGetLastError();
            IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error binding to socket: %d.\n"), dwRetVal));
            goto exit;
        }

        if (SOCKET_ERROR == listen(m_sockServer[m_cSockets], SOMAXCONN)) {
            m_sockServer[m_cSockets].close();
            dwRetVal = WSAGetLastError();
            IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error listening on socket: %d.\n"), dwRetVal));
            goto exit;
        }

        m_cSockets++;
    }

exit:
    if (paiLocal) {
        freeaddrinfo(paiLocal);
    }
    
    return dwRetVal;
}


void CHttpProxy::UpdateAdapterInfo(void)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    sockaddr_in saTmp;
    ULONG cbBuffer;
    PIP_ADAPTER_INFO pAdapterInfoHead = NULL;
    PIP_ADAPTER_INFO pAdapterInfo = NULL;
    SOCKADDR_IN saPrivIntf, saPublicIntf;
	CReg regICS;
	string strPubIntf;
	string strPrivIntf;
	        
    Lock();
    g_pSettings->Lock();

	// Get public and private interface names for RG
    if (regICS.Open(HKEY_LOCAL_MACHINE, RK_ICS)) {
		ce::wstring wstrPubIntf;
	    ce::wstring wstrPrivIntf;
		
        wstrPubIntf.reserve(MAX_PATH);
        if (FALSE == regICS.ValueSZ(RV_PUBLIC_INTF, wstrPubIntf.get_buffer(), wstrPubIntf.capacity())) {
            IFDBG(DebugOut(ZONE_WARN, _T("WebProxy: Unable to read Public Interface registry key.\n")));
        }
        else {
            // Get the size of the buffer
            int cchPubIntf = WideCharToMultiByte(CP_ACP, 0, wstrPubIntf, -1, strPubIntf.get_buffer(), 0, NULL, NULL);

            // Get the name of the public interface
            strPubIntf.reserve(cchPubIntf);
            WideCharToMultiByte(CP_ACP, 0, wstrPubIntf, -1, strPubIntf.get_buffer(), strPubIntf.capacity(), NULL, NULL);            
        }

        wstrPrivIntf.reserve(MAX_PATH);
        if (FALSE == regICS.ValueSZ(RV_PRIVATE_INTF, wstrPrivIntf.get_buffer(), wstrPrivIntf.capacity())) {
            IFDBG(DebugOut(ZONE_WARN, _T("WebProxy: Unable to read Private Interface registry key.\n")));
        }
        else {
            // Get the size of the buffer
            int cchPrivIntf = WideCharToMultiByte(CP_ACP, 0, wstrPrivIntf, -1, strPrivIntf.get_buffer(), 0, NULL, NULL);

            // Get the name of the private interface
            strPrivIntf.reserve(cchPrivIntf);
            WideCharToMultiByte(CP_ACP, 0, wstrPrivIntf, -1, strPrivIntf.get_buffer(), strPrivIntf.capacity(), NULL, NULL);            
        }
    }

    memset(&saPublicIntf, 0, sizeof(SOCKADDR_IN));
    memset(&saPrivIntf, 0, sizeof(SOCKADDR_IN));

    dwRetVal = GetAdaptersInfo(NULL, &cbBuffer);
    if (ERROR_BUFFER_OVERFLOW == dwRetVal) {
        pAdapterInfo = (PIP_ADAPTER_INFO)LocalAlloc(0, cbBuffer);
        if (NULL == pAdapterInfo) {
            IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Out of memory.\n")));
            goto exit;
        }

        dwRetVal = GetAdaptersInfo(pAdapterInfo, &cbBuffer);
        if (ERROR_SUCCESS != dwRetVal) {
            IFDBG(DebugOut(ZONE_ERROR, TEXT("Could not get adapter info.\n")));
            goto exit;
        }
    } 
    else if (dwRetVal != ERROR_SUCCESS) {
        IFDBG(DebugOut(ZONE_ERROR, TEXT("Could not get adapter info.\n")));
        goto exit;
    }

    if (NULL == pAdapterInfo) {
        IFDBG(DebugOut(ZONE_ERROR, TEXT("No IP Interface present.\n")));
        goto exit;
    }

    pAdapterInfoHead = pAdapterInfo;

    while (pAdapterInfo) {
        if ((strPubIntf != "") && (strPubIntf == pAdapterInfo->AdapterName)) {
            memset(&saTmp, 0, sizeof(sockaddr));
            saTmp.sin_family = AF_INET;
            saTmp.sin_addr.S_un.S_addr = inet_addr(pAdapterInfo->IpAddressList.IpAddress.String);

            if (INADDR_NONE == saTmp.sin_addr.S_un.S_addr) {
                IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error calling inet_addr.\n")));
                ASSERT(0);
            }
            else {
                saPublicIntf = saTmp;
            }            
        }
        else if ((strPrivIntf != "") && (strPrivIntf == pAdapterInfo->AdapterName)) {
            memset(&saTmp, 0, sizeof(sockaddr));

            g_pSettings->strPrivateAddrV4 = pAdapterInfo->IpAddressList.IpAddress.String;
            g_pSettings->strPrivateMaskV4 = pAdapterInfo->IpAddressList.IpMask.String;            

            saTmp.sin_family = AF_INET;
            saTmp.sin_addr.S_un.S_addr = inet_addr(g_pSettings->strPrivateAddrV4);
            if (INADDR_NONE != saTmp.sin_addr.S_un.S_addr) {
                saPrivIntf = saTmp;
            }

            WCHAR wszRegName[64];
            swprintf(wszRegName, L"Comm\\%S\\DhcpAllocator\\DhcpOptions", pAdapterInfo->AdapterName);

            CReg regDhcpOptions;
            if (regDhcpOptions.Open(HKEY_LOCAL_MACHINE, wszRegName)) {
                CHAR szOptions[MAX_PATH];
                if (regDhcpOptions.ValueBinary(L"252", (LPBYTE)szOptions, MAX_PATH)) {
                    CHAR* pszStart = szOptions;
                    CHAR* pszEnd;
                    CHAR szNewValue[MAX_PATH*2];

                    // We need to read the Proxy auto-discovery registry key
                    // and change only the IP portion of it.  Then we have to
                    // store the name of the URL which will be requested.
                    
                    if (0 == strncmp(pszStart, "http://", 7)) {
                        pszStart += 7;
                    }
                    pszEnd = pszStart;
                    while((*pszEnd != '/') && (*pszEnd != ':') && (*pszEnd != '\0')) {
                        pszEnd++;
                    }

                    if (*pszEnd == ':') {
                        CHAR* p = pszEnd;
                        while ((*p != '/') && (*p != '\0')) {
                            p++;
                        }
                        g_pSettings->strProxyPacURL = p;
                    }

                    strcpy(szNewValue, "http://");
                    strcat(szNewValue, g_pSettings->strPrivateAddrV4);
                    strcat(szNewValue, pszEnd);                    
                    regDhcpOptions.SetBinary(L"252", (LPBYTE)szNewValue, strlen(szNewValue)+1);
                }
                
            }        
        }

        pAdapterInfo = pAdapterInfo->Next;
    }
    
    g_pProxyFilter->NotifyAddrChange();
    
exit:
    if (pAdapterInfoHead) {
        LocalFree(pAdapterInfoHead);
    }

    g_pSettings->Unlock();
    Unlock();
    return;
}

DWORD InitStrings(void)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    WCHAR* wszTmp;
    DWORD cbBuff = DEFAULT_PROXY_STRING_SIZE;

    CReg regWebProxy;

    if (regWebProxy.Open(HKEY_LOCAL_MACHINE, RK_WEBPROXY)) {
        cbBuff = regWebProxy.ValueDW(RV_PROXYSTRINGSIZE, DEFAULT_PROXY_STRING_SIZE);
    }

    wszTmp = new WCHAR[cbBuff];
    if (! wszTmp) {
        dwRetVal = ERROR_OUTOFMEMORY;
        goto exit;
    }

    //
    // Get URL Not Found string
    //

    if (0 == LoadString(g_hInst, IDS_URL_NOT_FOUND, wszTmp, cbBuff)) {
        dwRetVal = GetLastError();
        goto exit;
    }
    g_pErrors->szURLNotFound = new CHAR[cbBuff];
    if (! g_pErrors->szURLNotFound) {
        dwRetVal = ERROR_OUTOFMEMORY;
        goto exit;
    }
    g_pErrors->cchURLNotFound = WideCharToMultiByte(CP_ACP, 0, wszTmp, -1, g_pErrors->szURLNotFound, cbBuff, NULL, NULL);
    if (0 == g_pErrors->cchURLNotFound) {
        dwRetVal = GetLastError();
        goto exit;
    }
    g_pErrors->cchURLNotFound--; // Do not count NULL terminating char

    //
    // Get Header Not Supported string
    //

    if (0 == LoadString(g_hInst, IDS_HEADER_NOT_SUPPORT, wszTmp, cbBuff)) {
        dwRetVal = GetLastError();
        goto exit;
    }
    g_pErrors->szHeaderNotSupported = new CHAR[cbBuff];
    if (! g_pErrors->szHeaderNotSupported) {
        dwRetVal = ERROR_OUTOFMEMORY;
        goto exit;
    }
    g_pErrors->cchHeaderNotSupported= WideCharToMultiByte(CP_ACP, 0, wszTmp, -1, g_pErrors->szHeaderNotSupported, cbBuff, NULL, NULL);
    if (0 == g_pErrors->cchHeaderNotSupported) {
        dwRetVal = GetLastError();
        goto exit;
    }
    g_pErrors->cchHeaderNotSupported--; // Do not count NULL terminating char

    //
    // Get Protocol Not Supported string
    //

    if (0 == LoadString(g_hInst, IDS_PROTO_NOT_SUPPORT, wszTmp, cbBuff)) {
        dwRetVal = GetLastError();
        goto exit;
    }
    g_pErrors->szProtocolNotSupported = new CHAR[cbBuff];
    if (! g_pErrors->szProtocolNotSupported) {
        dwRetVal = ERROR_OUTOFMEMORY;
        goto exit;
    }
    g_pErrors->cchProtocolNotSupported = WideCharToMultiByte(CP_ACP, 0, wszTmp, -1, g_pErrors->szProtocolNotSupported, cbBuff, NULL, NULL);
    if (0 == g_pErrors->cchProtocolNotSupported) {
        dwRetVal = GetLastError();
        goto exit;
    }
    g_pErrors->cchProtocolNotSupported--; // Do not count NULL terminating char

    //
    // Get Proxy Auth Failed string
    //

    if (0 == LoadString(g_hInst, IDS_PROXY_AUTH_FAILED, wszTmp, cbBuff)) {
        dwRetVal = GetLastError();
        goto exit;
    }
    g_pErrors->szProxyAuthFailed = new CHAR[cbBuff];
    if (! g_pErrors->szProxyAuthFailed) {
        dwRetVal = ERROR_OUTOFMEMORY;
        goto exit;
    }
    g_pErrors->cchProxyAuthFailed = WideCharToMultiByte(CP_ACP, 0, wszTmp, -1, g_pErrors->szProxyAuthFailed, cbBuff, NULL, NULL);
    if (0 == g_pErrors->cchProxyAuthFailed) {
        dwRetVal = GetLastError();
        goto exit;
    }
    g_pErrors->cchProxyAuthFailed--; // Do not count NULL terminating char

    //
    // Get Version Not Supported string
    //

    if (0 == LoadString(g_hInst, IDS_VER_NOT_SUPPORT, wszTmp, cbBuff)) {
        dwRetVal = GetLastError();
        goto exit;
    }
    g_pErrors->szVersionNotSupported = new CHAR[cbBuff];
    if (! g_pErrors->szVersionNotSupported) {
        dwRetVal = ERROR_OUTOFMEMORY;
        goto exit;
    }
    g_pErrors->cchVersionNotSupported = WideCharToMultiByte(CP_ACP, 0, wszTmp, -1, g_pErrors->szVersionNotSupported, cbBuff, NULL, NULL);
    if (0 == g_pErrors->cchVersionNotSupported) {
        dwRetVal = GetLastError();
        goto exit;
    }
    g_pErrors->cchVersionNotSupported--; // Do not count NULL terminating char

    //
    // Get Misc Error string
    //

    if (0 == LoadString(g_hInst, IDS_MISC_ERROR, wszTmp, cbBuff)) {
        dwRetVal = GetLastError();
        goto exit;
    }
    g_pErrors->szMiscError = new CHAR[cbBuff];
    if (! g_pErrors->szMiscError) {
        dwRetVal = ERROR_OUTOFMEMORY;
        goto exit;
    }
    g_pErrors->cchMiscError = WideCharToMultiByte(CP_ACP, 0, wszTmp, -1, g_pErrors->szMiscError, cbBuff, NULL, NULL);
    if (0 == g_pErrors->cchMiscError) {
        dwRetVal = GetLastError();
        goto exit;
    }
    g_pErrors->cchMiscError--; // Do not count NULL terminating char
    
exit:
    if (wszTmp) {
        delete[] wszTmp;
    }
    
    return dwRetVal;
}

void DeinitStrings(void)
{
    delete[] g_pErrors->szURLNotFound;
    delete[] g_pErrors->szHeaderNotSupported;
    delete[] g_pErrors->szProtocolNotSupported;
    delete[] g_pErrors->szProxyAuthFailed;
    delete[] g_pErrors->szVersionNotSupported;
    delete[] g_pErrors->szMiscError;
}
