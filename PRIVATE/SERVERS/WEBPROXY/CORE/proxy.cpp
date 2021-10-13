//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include "proxy.h"
#include "proxydev.h"
#include "auth.h"
#include "proxydbg.h"
#include <iphlpapi.h>
#include "resource.h"

PFN_ProxyInitializeFilter ProxyInitializeFilter;
PFN_ProxyUninitializeFilter ProxyUninitializeFilter;
PFN_ProxyFilterHttpRequest ProxyFilterHttpRequest;
PFN_ProxyNotifyAddrChange ProxyNotifyAddrChange;

char* g_szURLNotFound;
int g_cchURLNotFound;
char* g_szProxyAuthFailed;
int g_cchProxyAuthFailed;
char* g_szHeaderNotSupported;
int g_cchHeaderNotSupported;
char* g_szProtocolNotSupported;
int g_cchProtocolNotSupported;
char* g_szVersionNotSupported;
int g_cchVersionNotSupported;

ce::string g_strHostName;
ce::string g_strPrivateAddr;
ce::string g_strPrivateMask;
stringi g_strProxyPacURL;
int g_iPort;

DWORD InitStrings(void);
void DeinitStrings(void);

DWORD InitFilter(PPROXY_HTTP_INFORMATION pInfo)
{
	DWORD dwRetVal;
	
	__try {
		dwRetVal = ProxyInitializeFilter(pInfo);
	}
	__except (1) {
		dwRetVal = ERROR_INTERNAL_ERROR;
	}
	
	return dwRetVal;
}

void UninitFilter(void)
{
	__try {
		ProxyUninitializeFilter();
	}
	__except (1) {
	}
}

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

	dwRetVal = InitStrings();
	if (ERROR_SUCCESS != dwRetVal) {
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error initializing proxy strings.\n")));
		goto exit;
	}
	
	m_fRunning = FALSE;
	IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: Service has been initialized successfully.\n")));

exit:
	return dwRetVal;
}

DWORD CHttpProxy::Deinit(void)
{	
	DeinitStrings();
	WSACleanup();	
	svsutil_DeInitialize();
	
	IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: Service has been deinitialized successfully.\n")));
	return ERROR_SUCCESS;
}

DWORD CHttpProxy::InitSettings(void)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	ce::wstring wstrPubIntf;
	ce::wstring wstrPrivIntf;
	CReg regWebProxy;
	CReg regICS;
	
	//
	// Set the default values for all registry configurable settings
	//

	DWORD cSessionThreads = DEFAULT_SESSION_THREADS;
	ce::wstring wstrFilterLib = DEFAULT_FILTER_LIB_SZ;

	m_iMaxConnections = DEFAULT_MAX_CONNECTIONS;
	m_iMaxHeadersSize = DEFAULT_MAX_HEADERS_SIZE;
	m_fRunning = FALSE;
	m_fFilterInitialized = FALSE;
	m_fAuth = DEFAULT_AUTH;
	g_iPort = DEFAULT_PROXY_PORT;
	m_hlibFilter = NULL;
	m_iSessionTimeout = DEFAULT_SESSION_TIMEOUT;

	//
	// If we fail to open the registry key, that is ok as it is possible to desire all
	// the default settings.
	//
	
	if (regWebProxy.Open(HKEY_LOCAL_MACHINE, RK_WEBPROXY)) {
		WCHAR* pszTmp;

		m_iMaxConnections = regWebProxy.ValueDW(RV_MAX_CONNECTIONS, DEFAULT_MAX_CONNECTIONS);
		g_iPort = regWebProxy.ValueDW(RV_PORT, DEFAULT_PROXY_PORT);
		m_fAuth = regWebProxy.ValueDW(RV_AUTH, DEFAULT_AUTH);
		m_iSessionTimeout = regWebProxy.ValueDW(RV_SESSION_TIMEOUT, DEFAULT_SESSION_TIMEOUT);
		m_iMaxHeadersSize = regWebProxy.ValueDW(RV_MAXHEADERSSIZE, DEFAULT_MAX_HEADERS_SIZE);
		cSessionThreads = regWebProxy.ValueDW(RV_SESSION_THREADS, DEFAULT_SESSION_THREADS);
		
		pszTmp = (WCHAR *) regWebProxy.ValueSZ(RV_FILTERLIB);
		if (pszTmp) {
			wstrFilterLib = pszTmp;
		}
	}

	// Get public and private interface names for RG
	if (regICS.Open(HKEY_LOCAL_MACHINE, RK_ICS)) {
		wstrPubIntf.reserve(MAX_PATH);
		if (FALSE == regICS.ValueSZ(RV_PUBLIC_INTF, wstrPubIntf.get_buffer(), wstrPubIntf.capacity())) {
			IFDBG(DebugOut(ZONE_WARN, _T("WebProxy: Unable to read Public Interface registry key.\n")));
		}
		else {
			// Get the size of the buffer
			int cchPubIntf = WideCharToMultiByte(CP_ACP, 0, wstrPubIntf, -1, m_strPubIntf.get_buffer(), 0, NULL, NULL);

			// Get the name of the public interface
			m_strPubIntf.reserve(cchPubIntf);
			WideCharToMultiByte(CP_ACP, 0, wstrPubIntf, -1, m_strPubIntf.get_buffer(), m_strPubIntf.capacity(), NULL, NULL);			
		}

		wstrPrivIntf.reserve(MAX_PATH);
		if (FALSE == regICS.ValueSZ(RV_PRIVATE_INTF, wstrPrivIntf.get_buffer(), wstrPrivIntf.capacity())) {
			IFDBG(DebugOut(ZONE_WARN, _T("WebProxy: Unable to read Private Interface registry key.\n")));
		}
		else {
			// Get the size of the buffer
			int cchPrivIntf = WideCharToMultiByte(CP_ACP, 0, wstrPrivIntf, -1, m_strPrivIntf.get_buffer(), 0, NULL, NULL);

			// Get the name of the private interface
			m_strPrivIntf.reserve(cchPrivIntf);
			WideCharToMultiByte(CP_ACP, 0, wstrPrivIntf, -1, m_strPrivIntf.get_buffer(), m_strPrivIntf.capacity(), NULL, NULL);			
		}
	}

	m_pThreadPool = new SVSThreadPool(cSessionThreads);
	if (! m_pThreadPool) {
		dwRetVal = ERROR_OUTOFMEMORY;
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Out of memory allocating SVSThreadPool.\n")));
		goto exit;
	}

	g_strHostName.reserve(MAX_PATH);
	dwRetVal = gethostname(g_strHostName.get_buffer(), g_strHostName.capacity());
	if (ERROR_SUCCESS != dwRetVal) {
		g_strHostName = DEFAULT_HOST_NAME_SZ;
	}

	ProxyInitializeFilter = NULL;
	ProxyUninitializeFilter = NULL;
	ProxyFilterHttpRequest = NULL;
	ProxyNotifyAddrChange = NULL;
	
	m_hlibFilter = LoadLibrary(wstrFilterLib);
	if (m_hlibFilter.valid()) {
		ProxyInitializeFilter = (PFN_ProxyInitializeFilter) GetProcAddress(m_hlibFilter, FILTER_INIT_PROC_SZ);
		if (! ProxyInitializeFilter) {
			IFDBG(DebugOut(ZONE_WARN, _T("WebProxy: Could not GetProcAddress %s. Error: %d\n"), FILTER_INIT_PROC_SZ, GetLastError()));
		}
		ProxyUninitializeFilter = (PFN_ProxyUninitializeFilter) GetProcAddress(m_hlibFilter, FILTER_UNINIT_PROC_SZ);
		if (! ProxyUninitializeFilter) {
			IFDBG(DebugOut(ZONE_WARN, _T("WebProxy: Could not GetProcAddress %s. Error: %d\n"), FILTER_UNINIT_PROC_SZ, GetLastError()));
		}
		ProxyFilterHttpRequest = (PFN_ProxyFilterHttpRequest) GetProcAddress(m_hlibFilter, FILTER_REQUEST_PROC_SZ);
		if (! ProxyFilterHttpRequest) {
			IFDBG(DebugOut(ZONE_WARN, _T("WebProxy: Could not GetProcAddress %s. Error: %d\n"), FILTER_REQUEST_PROC_SZ, GetLastError()));
		}
		ProxyNotifyAddrChange = (PFN_ProxyNotifyAddrChange) GetProcAddress(m_hlibFilter, FILTER_ADDR_CHANGE_PROC_SZ);
		if (! ProxyNotifyAddrChange) {
			IFDBG(DebugOut(ZONE_WARN, _T("WebProxy: Could not GetProcAddress %s. Error: %d\n"), FILTER_ADDR_CHANGE_PROC_SZ, GetLastError()));
		}

		if (ProxyInitializeFilter) {
			PROXY_HTTP_INFORMATION info;
			info.dwSize = sizeof(info);
			info.dwProxyVersion = PROXY_VERSION;
			
			dwRetVal = InitFilter(&info);
			if (ERROR_SUCCESS != dwRetVal) {
				IFDBG(DebugOut(ZONE_WARN, _T("WebProxy: Error initializing filter! %d\n"), dwRetVal));
			}
			else {
				m_fFilterInitialized = TRUE;
			}
		}
		
	}
	else {
		IFDBG(DebugOut(ZONE_WARN, _T("WebProxy: Could not load Filter library %s. Error: %d\n"), (LPCWSTR)wstrFilterLib, GetLastError()));
	}		

	if (m_fAuth) {
		dwRetVal = InitSecurityLib();
		if (ERROR_SUCCESS != dwRetVal) {
			goto exit;
		}
	}

exit:
	if (ERROR_SUCCESS != dwRetVal) {
		if (m_pThreadPool) {
			delete m_pThreadPool;
			m_pThreadPool = NULL;
		}
		if (m_hlibFilter) {
			if (ProxyUninitializeFilter && m_fFilterInitialized) {
				UninitFilter();
				m_fFilterInitialized = FALSE;
			}
			FreeLibrary(m_hlibFilter);
			m_hlibFilter = NULL;
			
			ProxyInitializeFilter = NULL;
			ProxyUninitializeFilter = NULL;
			ProxyFilterHttpRequest = NULL;
		}
	}
	
	return dwRetVal;
}

DWORD CHttpProxy::DeinitSettings(void)
{	
	DeinitSecurityLib();

	if (m_hlibFilter) {
		if (ProxyUninitializeFilter && m_fFilterInitialized) {
			UninitFilter();
			m_fFilterInitialized = FALSE;
		}
		FreeLibrary(m_hlibFilter);
		m_hlibFilter = NULL;

		ProxyInitializeFilter = NULL;
		ProxyUninitializeFilter = NULL;
		ProxyFilterHttpRequest = NULL;
	}

	delete m_pThreadPool;
	m_pThreadPool = NULL;
	
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
		IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: Error creating ServerThread: %d.\n"), dwRetVal));
		ASSERT(0);
		goto exit;
	}
	
	IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: Service has been started successfully.\n")));

exit:
	Unlock();
	return dwRetVal;
}

DWORD CHttpProxy::Stop(void)
{
	DWORD dwRetVal = ERROR_SUCCESS;

	Lock();
	m_fRunning = FALSE;
	m_sockServer.close();

	// Need to shutdown each item in the list, then shutdown thread pool, 
	// then remove the items from the list.
	sessions.ShutdownAllSessions();
	m_pThreadPool->Shutdown();
	sessions.RemoveAllSessions();
	
	// If this function is being called from the ServerThread then we should not wait
	if ((HANDLE)GetCurrentThreadId() != m_hServerThread) {
		HANDLE h = m_hServerThread;
		Unlock();
		dwRetVal = WaitForSingleObject(h, INFINITE);
		Lock();
		if (WAIT_FAILED == dwRetVal) {
			dwRetVal = GetLastError();
			IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: Error stopping ServerThread: %d.\n"), dwRetVal));
			ASSERT(0);
			goto exit;
		}
	}

	DeinitSettings();
	
	dwRetVal = ERROR_SUCCESS;
	IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: Service has been stopped successfully.\n")));

exit:
	Unlock();
	return dwRetVal;
}

void NotifyFilterOfAddrChange(PPROXY_ADDRCHANGE_PROPS pProps)
{
	__try {
		ProxyNotifyAddrChange(pProps);
	}
	__except (1) {
	}
}

DWORD CHttpProxy::NotifyAddrChange(PBYTE pBufIn, DWORD cbBufIn)
{
	IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: Received address change notification.\n")));
	
	UpdateAdapterInfo();	
	return ERROR_SUCCESS;
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

	dwErr = SetupServerSocket();
	if (ERROR_SUCCESS != dwErr) {
		Stop();
		g_SvcMgr.SetState(SERVICE_STATE_OFF);
		goto exit;
	}

	while (1) {
		Unlock();
		sockaddr_in saClient;
		int cbsa = sizeof(saClient);
		auto_socket sockClient = accept(m_sockServer, (sockaddr*)&saClient, &cbsa);
		Lock();

		// Check if service is being signalled to stop
		if (! m_fRunning) {
			IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: Server thread is being signalled to stop, exiting...\n")));
			break;
		}

		if (INVALID_SOCKET == sockClient) {
			IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Accept on server thread failed: %d\n"), WSAGetLastError()));
			continue;
		}

		if (! memcmp(&saClient, &m_saPublicIntf, sizeof(sockaddr))) {
			IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: Recved request on public interface.  Ignoring...\n")));
			continue;
		}

		if (sessions.GetSessionCount() >= m_iMaxConnections) {
			IFDBG(DebugOut(ZONE_WARN, _T("WebProxy: Cannot accept any new connections - connection limit has been reached.\n")));
			continue;
		}

		SessionSettings settings;
		
		memset(&settings, 0, sizeof(settings));
		settings.sockClient = sockClient;
		settings.saClient = saClient;
		settings.fAuthEnabled = m_fAuth;
		settings.iMaxHeadersSize = m_iMaxHeadersSize;
		settings.pThreadPool = m_pThreadPool;
		settings.iSessionTimeout = m_iSessionTimeout;

		settings.saPrivate = m_saPrivIntf;

		// Start the HTTP session and insert it in the list
		dwErr = sessions.StartSession(&settings, NULL);
		if (ERROR_SUCCESS != dwErr) {
			IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error adding session to list %d\n"), dwErr));
			continue;
		}
	}
	
exit:
	Unlock();
	return;
}

DWORD CHttpProxy::SetupServerSocket()
{
	DWORD dwRetVal = ERROR_SUCCESS;
	SOCKADDR_IN saServer;

	m_sockServer = socket(PF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == m_sockServer) {
		dwRetVal = WSAGetLastError();
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error creating server socket: %d.\n"), dwRetVal));
		ASSERT(0);
		goto exit;
	}

	// Set address to bind to
	memset(&saServer, 0, sizeof(saServer));
	saServer.sin_family				= PF_INET;
	saServer.sin_port				= htons(g_iPort);
	saServer.sin_addr.S_un.S_addr	= INADDR_ANY;

	// Bind address to socket
	if (SOCKET_ERROR == bind(m_sockServer, (SOCKADDR *)&saServer, sizeof(saServer))) {
		dwRetVal = WSAGetLastError();
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error binding to socket: %d.\n"), dwRetVal));
		ASSERT(0);
		m_sockServer.close();
		goto exit;
	}	

	if (SOCKET_ERROR == listen(m_sockServer, SOMAXCONN)) {
		dwRetVal = WSAGetLastError();
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error listening on server socket: %d.\n"), dwRetVal));
		ASSERT(0);
		m_sockServer.close();
		goto exit;
	}

exit:
	return dwRetVal;
}

void CHttpProxy::UpdateAdapterInfo(void)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	sockaddr_in saTmp;
	ULONG cbBuffer;
	PIP_ADAPTER_INFO pAdapterInfoHead = NULL;
	PIP_ADAPTER_INFO pAdapterInfo = NULL;
	
	Lock();

	memset(&m_saPublicIntf, 0, sizeof(sockaddr));
	memset(&m_saPrivIntf, 0, sizeof(sockaddr));

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
		if ((m_strPubIntf != "") && (m_strPubIntf == pAdapterInfo->AdapterName)) {
			memset(&saTmp, 0, sizeof(sockaddr));
			saTmp.sin_family = AF_INET;
			saTmp.sin_addr.S_un.S_addr = inet_addr(pAdapterInfo->IpAddressList.IpAddress.String);

			if (INADDR_NONE == saTmp.sin_addr.S_un.S_addr) {
				IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error calling inet_addr.\n")));
				ASSERT(0);
			}
			else {
				m_saPublicIntf = saTmp;
			}			
		}
		else if ((m_strPrivIntf != "") && (m_strPrivIntf == pAdapterInfo->AdapterName)) {
			memset(&saTmp, 0, sizeof(sockaddr));

			g_strPrivateAddr = pAdapterInfo->IpAddressList.IpAddress.String;
			g_strPrivateMask = pAdapterInfo->IpAddressList.IpMask.String;			

			saTmp.sin_family = AF_INET;
			saTmp.sin_addr.S_un.S_addr = inet_addr(g_strPrivateAddr);
			if (INADDR_NONE != saTmp.sin_addr.S_un.S_addr) {
				m_saPrivIntf = saTmp;
			}

			WCHAR wszRegName[64];
			swprintf(wszRegName, L"Comm\\%S\\DhcpAllocator\\DhcpOptions", pAdapterInfo->AdapterName);

			CReg regDhcpOptions;
			if (regDhcpOptions.Open(HKEY_LOCAL_MACHINE, wszRegName)) {
				CHAR szOptions[MAX_PATH];
				if (regDhcpOptions.ValueBinary(L"252", (LPBYTE)szOptions, MAX_PATH)) {
					CHAR* pszStart = szOptions;
					CHAR* pszEnd;
					CHAR szNewValue[MAX_PATH];

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
						g_strProxyPacURL = p;
					}

					strcpy(szNewValue, "http://");
					strcat(szNewValue, g_strPrivateAddr);
					strcat(szNewValue, pszEnd);					
					regDhcpOptions.SetBinary(L"252", (LPBYTE)szNewValue, strlen(szNewValue)+1);
				}
				
			}		
		}

		pAdapterInfo = pAdapterInfo->Next;
	}

	if (ProxyNotifyAddrChange) {
		PROXY_ADDRCHANGE_PROPS pProps;
		pProps.dwSize = sizeof(pProps);
		pProps.saPrivateIp = *(SOCKADDR_STORAGE*)&m_saPrivIntf;
		pProps.cbsaPrivateIp = sizeof(m_saPrivIntf);
		pProps.saPublicIp = *(SOCKADDR_STORAGE*)&m_saPublicIntf;
		pProps.cbsaPublicIp = sizeof(m_saPublicIntf);
		NotifyFilterOfAddrChange(&pProps);
	}
	
exit:
	if (pAdapterInfoHead) {
		LocalFree(pAdapterInfoHead);
	}
	
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
	g_szURLNotFound = new CHAR[cbBuff];
	if (! g_szURLNotFound) {
		dwRetVal = ERROR_OUTOFMEMORY;
		goto exit;
	}
	g_cchURLNotFound = WideCharToMultiByte(CP_ACP, 0, wszTmp, -1, g_szURLNotFound, cbBuff, NULL, NULL);
	if (0 == g_cchURLNotFound) {
		dwRetVal = GetLastError();
		goto exit;
	}
	g_cchURLNotFound--; // Do not count NULL terminating char

	//
	// Get Header Not Supported string
	//

	if (0 == LoadString(g_hInst, IDS_HEADER_NOT_SUPPORT, wszTmp, cbBuff)) {
		dwRetVal = GetLastError();
		goto exit;
	}
	g_szHeaderNotSupported = new CHAR[cbBuff];
	if (! g_szHeaderNotSupported) {
		dwRetVal = ERROR_OUTOFMEMORY;
		goto exit;
	}
	g_cchHeaderNotSupported= WideCharToMultiByte(CP_ACP, 0, wszTmp, -1, g_szHeaderNotSupported, cbBuff, NULL, NULL);
	if (0 == g_cchHeaderNotSupported) {
		dwRetVal = GetLastError();
		goto exit;
	}
	g_cchHeaderNotSupported--; // Do not count NULL terminating char

	//
	// Get Protocol Not Supported string
	//

	if (0 == LoadString(g_hInst, IDS_PROTO_NOT_SUPPORT, wszTmp, cbBuff)) {
		dwRetVal = GetLastError();
		goto exit;
	}
	g_szProtocolNotSupported = new CHAR[cbBuff];
	if (! g_szProtocolNotSupported) {
		dwRetVal = ERROR_OUTOFMEMORY;
		goto exit;
	}
	g_cchProtocolNotSupported = WideCharToMultiByte(CP_ACP, 0, wszTmp, -1, g_szProtocolNotSupported, cbBuff, NULL, NULL);
	if (0 == g_cchProtocolNotSupported) {
		dwRetVal = GetLastError();
		goto exit;
	}
	g_cchProtocolNotSupported--; // Do not count NULL terminating char

	//
	// Get Proxy Auth Failed string
	//

	if (0 == LoadString(g_hInst, IDS_PROXY_AUTH_FAILED, wszTmp, cbBuff)) {
		dwRetVal = GetLastError();
		goto exit;
	}
	g_szProxyAuthFailed = new CHAR[cbBuff];
	if (! g_szProxyAuthFailed) {
		dwRetVal = ERROR_OUTOFMEMORY;
		goto exit;
	}
	g_cchProxyAuthFailed = WideCharToMultiByte(CP_ACP, 0, wszTmp, -1, g_szProxyAuthFailed, cbBuff, NULL, NULL);
	if (0 == g_cchProxyAuthFailed) {
		dwRetVal = GetLastError();
		goto exit;
	}
	g_cchProxyAuthFailed--; // Do not count NULL terminating char

	//
	// Get Version Not Supported string
	//

	if (0 == LoadString(g_hInst, IDS_VER_NOT_SUPPORT, wszTmp, cbBuff)) {
		dwRetVal = GetLastError();
		goto exit;
	}
	g_szVersionNotSupported = new CHAR[cbBuff];
	if (! g_szVersionNotSupported) {
		dwRetVal = ERROR_OUTOFMEMORY;
		goto exit;
	}
	g_cchVersionNotSupported = WideCharToMultiByte(CP_ACP, 0, wszTmp, -1, g_szVersionNotSupported, cbBuff, NULL, NULL);
	if (0 == g_cchVersionNotSupported) {
		dwRetVal = GetLastError();
		goto exit;
	}
	g_cchVersionNotSupported--; // Do not count NULL terminating char
	
exit:
	if (wszTmp) {
		delete[] wszTmp;
	}
	
	return dwRetVal;
}

void DeinitStrings(void)
{
	delete[] g_szURLNotFound;
	delete[] g_szHeaderNotSupported;
	delete[] g_szProtocolNotSupported;
	delete[] g_szProxyAuthFailed;
	delete[] g_szVersionNotSupported;
}
