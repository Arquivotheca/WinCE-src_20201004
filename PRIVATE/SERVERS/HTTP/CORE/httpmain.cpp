//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: HTTPMAIN.CPP
Abstract: HTTP server initialization & listener thread 
--*/


#include "httpd.h"

//
//-------------------- Global data --------------
//

CGlobalVariables *g_pVars;            // Global data.  This is the "default" website.
CGlobalVariables *g_pWebsites;        // Multiple websites.
DWORD             g_cWebsites;        // Number of websites (does not count the default)
HANDLE	  g_hListenThread;            // handle to the main listen thread
HINSTANCE g_hInst;                    // handle to DLL
BOOL  g_fRegistered;                  // Whether RegisterDevice("HTP0:") has been, only needed when running from device.exe.
LONG  g_fState;                       // Current Service State (running, stopped, starting, shutting down, unloading)
DWORD g_CallerExe;                    // Whether we've been called from device.exe, services.exe, httpdexe.exe, or elsewhere.
CRITICAL_SECTION g_CritSect;          // Global lock

CWebDavFileLockManager *g_pFileLockManager;  // Implements LOCK and UNLOCK support for WebDav.
SVSThreadPool *g_pTimer;                     // ISAPI extension unload and WebDAV file unlock timer thread (wakes up once a minute).

// We keep track of whether we've spun a thread in this function to deal with
// stopping+restarting, in the event that when we're unloa
HANDLE g_hAdminThread;

BOOL   g_fSuperServices;            // If we get stopped and someone wants to restart us, whether we spin thread or rely on Super Services.

CHttpRequest *g_pRequestList;       // List of active http connections.

BOOL g_fUTF8;

DWORD g_dwServerTimeout = RECVTIMEOUT;

//------------- Const data -----------------------
//

const char cszTextHtml[] = "text/html";
const char cszOctetStream[] = "application/octet-stream";
const char cszEmpty[] = "";
const char cszMaxConnectionHeader[] =  "HTTP/1.0 503\r\n\r\n";

// 
//------------- Debug data -----------------------
//
#if defined(UNDER_CE) && defined (DEBUG)
  DBGPARAM dpCurSettings = {
    TEXT("HTTPD"), {
    TEXT("Error"),TEXT("Init"),TEXT("Listen"),TEXT("Socket"),
    TEXT("Request"),TEXT("Response"),TEXT("ISAPI"),TEXT("VROOTS"),
    TEXT("ASP"),TEXT("Device"),TEXT("SSL"),TEXT("Verbose"),
    TEXT("Authentication"),TEXT("WebDAV"),TEXT("Parser"),TEXT("Tokens") },
    0x0003
  }; 
#endif


// 
//------------- Prototypes -----------------------
//
PWSTR MassageMultiString(PCWSTR wszIn, PCWSTR wszDefault=NULL);
// 
//------------- Startup functions -----------------------
//

// Using the build environment, we set our entry point to DllEnty.  For Visual Studio,
// we set it to DllMain (which is called by DllMainCRTStartup)
#ifndef OLD_CE_BUILD
extern "C" BOOL WINAPI DllEntry( HANDLE hInstDll, DWORD fdwReason, LPVOID lpvReserved)
#else
extern "C" BOOL WINAPI DllMain( HANDLE hInstDll, DWORD fdwReason, LPVOID lpvReserved)
#endif
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			DEBUGMSG(ZONE_INIT,(TEXT("HTTPD: DllMain attached\r\n")));
			g_fRegistered  = FALSE;
			g_hAdminThread = 0;
			g_hInst = (HINSTANCE)hInstDll;	
			svsutil_Initialize();

			InitializeCriticalSection(&g_CritSect);

#if ! defined (OLD_CE_BUILD)
			DisableThreadLibraryCalls((HMODULE)hInstDll);
#endif

#ifdef UNDER_CE
			DEBUGREGISTER((HINSTANCE)hInstDll);
			// We need to figure out what context we're running in.
			WCHAR szModule[MAX_PATH];

			if (GetModuleFileName((HINSTANCE)GetCurrentProcess(),szModule,ARRAYSIZEOF(szModule)))  {
				DEBUGMSG(ZONE_INIT,(L"HTTPD: Being called from executable <<%s>>\r\n",szModule));
				if (wcsstr(szModule,SERVICE_DEVICE_EXE_PROCNAME))  {
					g_CallerExe = SERVICE_CALLER_PROCESS_DEVICE_EXE;
#if defined (OLD_CE_BUILD)
					// on legacy versions of WinCE, we use HttpInitialize as the entry point,
					// in which case we need to be stopped to make everything get started.
					g_fState = SERVICE_STATE_OFF;
#else
					g_fState = SERVICE_STATE_UNINITIALIZED;
#endif // UNDER_CE
				}
				else if (wcsstr(szModule,SERVICE_SERVICES_EXE_PROCNAME)) {
					DEBUGCHK(IsAPIReady(SH_SERVICES));
					g_CallerExe = SERVICE_CALLER_PROCESS_SERVICES_EXE;
					g_fState = SERVICE_STATE_UNINITIALIZED;
				}
				else if (wcsstr(szModule,L"httpdexe.exe")) {
					g_CallerExe = SERVICE_CALLER_PROCESS_HTTPDEXE_EXE;
					g_fState = SERVICE_STATE_OFF;
				}
				else {
					g_CallerExe = SERVICE_CALLER_PROCESS_OTHER_EXE;
					g_fState = SERVICE_STATE_OFF;
				}
			}
			else {
				DEBUGMSG(ZONE_ERROR,(L"HTTPD: DllMain: Unable to determine calling process.  Assuming neither device.exe or services.exe\r\n"));
				g_CallerExe = SERVICE_CALLER_PROCESS_OTHER_EXE;
				g_fState = SERVICE_STATE_OFF;
			}
#else
			g_fState = SERVICE_STATE_OFF;
#endif
			break;

		case DLL_PROCESS_DETACH:
			DEBUGMSG(ZONE_INIT, (TEXT("HTTPD: DllMain detach\r\n")));
			// kill the listener thread

			// NOTE: If there are connection threads active when httpd.dll 
			// is unloaded they will not be notified and will attempt to access
			// the freed code pages of httpd.dll.
			if (g_hListenThread)
				TerminateThread(g_hListenThread, 0);

			DeleteCriticalSection(&g_CritSect);
			DEBUGCHK(g_fState == SERVICE_STATE_UNLOADING && !g_pVars && !g_hListenThread && !g_pWebsites && (g_cWebsites==0) && !g_pTimer);
			break;
	}
	return TRUE;
}

extern "C" int HttpInitialize(TCHAR *szRegPath) {
	if (InterlockedCompareExchange((PINTERLOCKED_COMP)&g_fState,  (INTERLOCKED_COMP)SERVICE_STATE_STARTING_UP,
	                               SERVICE_STATE_OFF) != SERVICE_STATE_OFF) 	{
		SetLastError(ERROR_SERVICE_ALREADY_RUNNING);
		DEBUGMSG(ZONE_INIT|ZONE_ERROR,(L"HTTPD: HttpInitialize not starting service - not in SERVICE_STATE_OFF, current state=%d\r\n",g_fState));
		return 0;
	}
	DEBUGCHK(!g_hListenThread);
	
	g_hListenThread = MyCreateThread(HttpListenThread, 0);
	if (g_hListenThread) {
		CloseHandle(g_hListenThread);
		g_fSuperServices = 0;
		return 1;
	}

	DEBUGMSG(ZONE_ERROR,(L"HTTPD:HttpInitialize unable to CreateThread, GLE=0x%08x\r\n",GetLastError()));
	g_fState = SERVICE_STATE_OFF;
	return 0;
}

DWORD GetServerTimeout(void) {
    CReg reg;

    reg.Open(HKEY_LOCAL_MACHINE, RK_HTTPD);
	if (!reg.IsOK())
        return RECVTIMEOUT;

    return reg.ValueDW(TEXT("ServerTimeout"), RECVTIMEOUT);
}

// Routines that are called once after HTTPD.dll has been started, but are not safe to be called from DLLMain().
BOOL HttpInitializeOnce(void) {
	g_pTimer = new SVSThreadPool(1);
	if (!g_pTimer)
		return FALSE;

	svsutil_InitializeInterfaceMapperOnce();
	svsutil_ResetInterfaceMapper();
	DoesSystemSupportUTF8();
    g_dwServerTimeout = GetServerTimeout();

	g_pTimer->ScheduleEvent(PeriodicWakeupThread,0,PERIODIC_TIMER_SLEEP_TIMEOUT);
	return TRUE;
}

// Called from httpdexe.exe
extern "C" int HttpInitializeFromExe()  {
	DEBUGCHK(SERVICE_CALLER_PROCESS_HTTPDEXE_EXE == g_CallerExe);
	DEBUGCHK(SERVICE_STATE_OFF == g_fState);

	if (! HttpInitializeOnce())
		return FALSE;

	return HttpInitialize(NULL);
}

const WCHAR wszDefaultLogDirectory[] = L"\\windows\\www";

BOOL IsHttpdEnabled(void) {
	// If being run by httpdexe.exe, ignore registry settings.  We do this
	// because people will disable HTTPD in registry to have HTTPD not running
	// from device.exe/services.exe in the first place.
	if (SERVICE_CALLER_PROCESS_HTTPDEXE_EXE == g_CallerExe)
		return TRUE;

	CReg reg(HKEY_LOCAL_MACHINE, RK_HTTPD);
	if (reg.ValueDW(RV_ISENABLED,1))
		return TRUE;

	SetLastError(ERROR_SERVICE_DISABLED);

	DEBUGMSG(ZONE_INIT,(L"HTTPD: IsEnabled=0, will not start web server\r\n"));
	DWORD dwMaxLogSize;
	WCHAR wszLogDir[MAX_PATH + 1];

	if (0 == (dwMaxLogSize = reg.ValueDW(RV_MAXLOGSIZE)))
		return FALSE;

	if ( ! reg.ValueSZ(RV_LOGDIR,wszLogDir,MAX_PATH+1))
		wcscpy(wszLogDir,wszDefaultLogDirectory);

	CLog cLog(dwMaxLogSize,wszLogDir);
	cLog.WriteEvent(IDS_HTTPD_DISABLED);

	return FALSE;
}

void CGlobalVariables::InitGlobals(CReg *pWebsite)  {
	DWORD dwMaxLogSize;
	WCHAR wszLogDir[MAX_PATH + 1];

	ZEROMEM(this);
	memset(&m_sockList,INVALID_SOCKET,sizeof(m_sockList));

	CReg reg;
	m_fRootSite = !pWebsite;

	// if pWebsite==NULL, then this is initialization of default website.
	if (m_fRootSite) {
		DEBUGCHK(g_pVars == NULL);

		// certain functions, including listen thread management and ISAPI Filters
		// are managed globally by the main site.

		reg.Open(HKEY_LOCAL_MACHINE, RK_HTTPD);
		if (!reg.IsOK())   {
			CLog cLog(DEFAULTLOGSIZE,L"\\windows\\www");
			cLog.WriteEvent(IDS_HTTPD_NO_REGKEY);
			DEBUGMSG(ZONE_ERROR,(L"HTTPD:  No registry key setup, will not handle requests\r\n"));
			return;
		}
		pWebsite = &reg;

		if (! IsHttpdEnabled())
			return;

		// thread pool is global
		m_nMaxConnections = pWebsite->ValueDW(RV_MAXCONNECTIONS,10);
		m_pThreadPool = new SVSThreadPool(m_nMaxConnections + 1);  // +1 for ISAPI Cache removal thread
		if (!m_pThreadPool)
			return;

		// initial string setup only needs to be done once.
		if (NULL == (m_pszStatusBodyBuf = MyRgAllocNZ(CHAR,BODYSTRINGSIZE)))
			return;

		InitializeResponseCodes(m_pszStatusBodyBuf);

		strcpy(m_szMaxConnectionMsg,cszMaxConnectionHeader);
		WCHAR wszMaxConnectionMsg[256];

		LoadString(g_hInst,IDS_SERVER_BUSY,wszMaxConnectionMsg,ARRAYSIZEOF(wszMaxConnectionMsg));
		MyW2A(wszMaxConnectionMsg,m_szMaxConnectionMsg + sizeof(cszMaxConnectionHeader) - 1,
				  sizeof(m_szMaxConnectionMsg) - sizeof(cszMaxConnectionHeader));

		m_fUseDefaultSite = pWebsite->ValueDW(RV_ALLOW_DEFAULTSITE,TRUE);

		// setup logging
		dwMaxLogSize = pWebsite->ValueDW(RV_MAXLOGSIZE);
		if ( ! pWebsite->ValueSZ(RV_LOGDIR,wszLogDir,MAX_PATH+1)) {
			wcscpy(wszLogDir,wszDefaultLogDirectory);
		}

		m_pLog = new CLog(dwMaxLogSize,wszLogDir);
		if (!m_pLog)
			return;

		// max POST/Header sizes, 
		m_dwPostReadSize       = pWebsite->ValueDW(RV_POSTREADSIZE,  48*1024); // 48 K default
		if (m_dwPostReadSize < MIN_POST_READ_SIZE) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: PostReadSize set in registry = %d, however minimum must be %d.  Resetting value to minimum acceptable\r\n",m_dwPostReadSize,MIN_POST_READ_SIZE));
			m_dwPostReadSize = MIN_POST_READ_SIZE;
		}
		
		m_dwMaxHeaderReadSize  = pWebsite->ValueDW(RV_MAXHEADERSIZE, 48*1024); // 48 K default
		m_dwSystemChangeNumber = pWebsite->ValueDW(RV_CHANGENUMBER,  1);

		m_fExtensions = InitExtensions(pWebsite);
	}
#if defined (DEBUG) || defined (_DEBUG)
	else	
		DEBUGCHK(g_pVars);
#endif

	m_dwListenPort = pWebsite->ValueDW(RV_PORT, IPPORT_HTTP);
	DEBUGCHK(m_dwListenPort);

	m_fASP            = InitASP(pWebsite);
	m_fWebDav         = InitWebDav(pWebsite);
	m_fDirBrowse      = pWebsite->ValueDW(RV_DIRBROWSE, HTTPD_ALLOW_DIR_BROWSE);
	
	m_wszDefaultPages = MassageMultiString(pWebsite->ValueSZ(RV_DEFAULTPAGE),HTTPD_DEFAULT_PAGES);
	m_wszAdminUsers   = MassageMultiString(pWebsite->ValueSZ(RV_ADMINUSERS));
	m_wszAdminGroups  = MassageMultiString(pWebsite->ValueSZ(RV_ADMINGROUPS));

	InitAuthentication(pWebsite);
	if (m_fRootSite)
		InitSSL(pWebsite);

	WCHAR *wszServerID;
	if (NULL != (wszServerID = (WCHAR*)pWebsite->ValueSZ(RV_SERVER_ID))) {
		// User specified an over-ride server ID, convert to ANSI and use it.
		if (NULL == (m_szServerID = MySzDupWtoA(wszServerID)))
			return;
	}
	else {
		// Use default server ID if not overridden
		static const char szDefaultServerIdFmt[]    = "Microsoft-WinCE/%d.%d";

		if (NULL == (m_szServerID =  MyRgAllocNZ(CHAR,CCHSIZEOF(szDefaultServerIdFmt)+20)))
			return;

		sprintf(m_szServerID,szDefaultServerIdFmt,CE_MAJOR_VER,CE_MINOR_VER);
	}

	InitMapping(*pWebsite,RV_HOSTEDSITES); // SVSInterfaceMapper does hard work here.

	m_pVroots = new CVRoots(pWebsite,m_fDirBrowse,m_fBasicAuth, m_fNTLMAuth, m_fBasicToNTLM);
	if (!m_pVroots)
		return;

	// vroot table must be initialized or web server can't return files.  Warn
	// user if this is not the case
	if ((m_pVroots->Count() == 0) && m_fRootSite) 
		m_pLog->WriteEvent(IDS_HTTPD_NO_VROOTS);

	m_fAcceptConnections = TRUE;
}


void CGlobalVariables::DeInitGlobals(void)  {
	// note that certain variables (like ISAPIs and logging) are only
	// initialized on the default web site.
	DEBUGCHK(g_pVars->m_nConnections == 0);
	DEBUGCHK(g_fState != SERVICE_STATE_ON);

	MyFree(m_wszDefaultPages);
	MyFree(m_wszAdminUsers);
	MyFree(m_wszAdminGroups);	
	MyFree(m_pszStatusBodyBuf);
	MyFree(m_pszSSLIssuer);
	MyFree(m_pszSSLSubject);
	MyFree(m_szServerID);

	if (m_pVroots)
		delete m_pVroots;

	if (m_fRootSite) {
		DEBUGCHK(this == g_pVars);

		if (m_fWebDav && (g_fState == SERVICE_STATE_UNLOADING))
			DeInitWebDav(); // only on unload because lock manager persists across refreshes
	
		if (m_fFilters)
			CleanupFilters();

		if (m_fExtensions)
			DeInitExtensions();

		CloseHTTPSockets();

		if (m_pISAPICache) {
			// Flush all ISAPIs.  This will block until everyone's unloaded.
			m_pISAPICache->RemoveUnusedISAPIs(TRUE);
			delete m_pISAPICache;
		}

		if (m_pThreadPool)
			delete m_pThreadPool;

		if (m_fSSL)
			FreeSSLResources();

		MyFreeLib(m_hSecurityLib);

		// Log must be the last thing we destroy, because theoretically
		// a cleanup function could make a call into it to report an error condition.
		if (m_pLog)
			delete m_pLog;

		m_dwSystemChangeNumber++;
		CReg reg(HKEY_LOCAL_MACHINE,RK_HTTPD);
		reg.SetDW(RV_CHANGENUMBER,m_dwSystemChangeNumber);
	}
#if defined (DEBUG)
	else {
		// non-root website should never have got any of this set, always use default for these settings.
		DEBUGCHK(m_pISAPICache == NULL);
		DEBUGCHK(m_pLog == NULL);
		DEBUGCHK(m_pThreadPool == NULL);
		DEBUGCHK(m_hSSLCertStore == 0);
		DEBUGCHK(m_fHasSSLCreds == FALSE);
		DEBUGCHK(m_hSecurityLib == NULL);
	}
#endif

	DeInitMapping(); // SVSInterfaceMapper call
}


DWORD WINAPI InitializeGlobalsThread(LPVOID lpv) {
	DEBUGCHK(g_fState == SERVICE_STATE_STARTING_UP);
	DEBUGCHK(g_hAdminThread);
	DEBUGCHK(g_fSuperServices);

	InitializeGlobals(TRUE);
	g_hAdminThread = 0;
	return 0;
}

#if defined (OLD_CE_BUILD)
BOOL CGlobalVariables::CreateHTTPSocket(BOOL fSecure) {
	DEBUG_CODE_INIT
	BOOL fRet = FALSE;
	SOCKADDR_IN addrListen;

	DWORD i      = fSecure ? 1 : 0;
	DWORD dwPort = fSecure ? m_dwSSLPort : m_dwListenPort;

	if(INVALID_SOCKET == (m_sockList[i] = socket(AF_INET, SOCK_STREAM, 0))) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: socket() fails, GLE=0x%08x\r\n",GetLastError()));
		myleave(2);
	}

	memset(&addrListen, 0, sizeof(addrListen));
	addrListen.sin_family = AF_INET;
	addrListen.sin_port = htons((WORD)dwPort);
	addrListen.sin_addr.s_addr = INADDR_ANY;

	if (fSecure)
		m_nSSLSockets   = 1;
	else
		m_nPlainSockets = 1;

	if(bind(m_sockList[i], (PSOCKADDR)&addrListen, sizeof(addrListen))) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: bind() fails, GLE=0x%08x\r\n",GetLastError()));
		g_pVars->m_pLog->WriteEvent(IDS_HTTPD_BIND_ERROR,dwPort,GetLastError());
		myleave(3);
	}	

	if(listen(m_sockList[i], SOMAXCONN)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: listen() fails, GLE=0x%08x\r\n",GetLastError()));
		g_pVars->m_pLog->WriteEvent(IDS_HTTPD_BIND_ERROR,dwPort,GetLastError());
		myleave(4);
	}

	fRet = TRUE;
done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"HTTPD: CreateHTTPSocket fails, GLE=0x%08x, err=%d\r\n",GetLastError(),err));
	return fRet;
}
#else // OLD_CE_BUILD
BOOL CGlobalVariables::CreateHTTPSocket(BOOL fSecure) {
	ADDRINFO aiHints;
	ADDRINFO *paiLocal = NULL;
	ADDRINFO *paiTrav  = NULL;

	DWORD i      = fSecure ? m_nPlainSockets : 0;
	DWORD dwPort = fSecure ? m_dwSSLPort     : m_dwListenPort;

	int err = 0;
	BOOL fRet = FALSE;
	CHAR szPort[16];

	memset(&aiHints, 0, sizeof(aiHints));
	aiHints.ai_family = PF_UNSPEC;
	aiHints.ai_socktype = SOCK_STREAM;
	aiHints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;

	sprintf(szPort,"%d",dwPort);

	if (0 != getaddrinfo(NULL, szPort, &aiHints, &paiLocal)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: getaddrinfo() fails, GLE=0x%08x\r\n",GetLastError()));
		myleave(1);
	}

	for (paiTrav = paiLocal; paiTrav; paiTrav = paiTrav->ai_next) {
		if (INVALID_SOCKET == (m_sockList[i] = socket(paiTrav->ai_family, paiTrav->ai_socktype, paiTrav->ai_protocol)))
			continue;

		// increment count now in the event of failure on bind/listen so we know to close this socket.
		if (fSecure)
			m_nSSLSockets++;
		else
			m_nPlainSockets++;

		if (SOCKET_ERROR == bind(m_sockList[i], paiTrav->ai_addr, paiTrav->ai_addrlen)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: bind() fails, GLE=0x%08x\r\n",GetLastError()));
			g_pVars->m_pLog->WriteEvent(IDS_HTTPD_BIND_ERROR,dwPort,GetLastError());
			myleave(3);
		}

		if (SOCKET_ERROR == listen(m_sockList[i], SOMAXCONN)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: listen() fails, GLE=0x%08x\r\n",GetLastError()));
			g_pVars->m_pLog->WriteEvent(IDS_HTTPD_BIND_ERROR,dwPort,GetLastError());
			myleave(4);
		}

		i++;
	}

	// need to have got at least one socket (ipv4 presumably) for this call to be success.
	if (fSecure)
		fRet = m_nSSLSockets    ? TRUE : FALSE;
	else
		fRet = m_nPlainSockets  ? TRUE : FALSE;

done:
	if (paiLocal)
		freeaddrinfo(paiLocal);

	DEBUGMSG_ERR(ZONE_ERROR,(L"HTTPD: CreateHTTPSocket fails, GLE=0x%08x, err=%d\r\n",GetLastError(),err));
	return fRet;
}
#endif // OLD_CE_BUILD

BOOL CGlobalVariables::IsSocketInSecureList(SOCKET sock) {
	DWORD i;
	for (i = 0; i < m_nPlainSockets; i++) {
		if (m_sockList[i] == sock)
			return FALSE;
	}

#if defined (DEBUG)
	// socket better be in SSL list then...
	for (i = m_nPlainSockets; i < m_nSSLSockets+m_nPlainSockets; i++) {
		if (m_sockList[i] == sock)
			break;
	}
	DEBUGCHK (i != m_nSSLSockets+m_nPlainSockets);
#endif

	return TRUE;
}

#if defined (OLD_CE_BUILD)
// legacy CE devices don't support Winsock2
#define WSOCK_VERSION_HIGH 1
#define WSOCK_VERSION_LOW  1
#else
#define WSOCK_VERSION_HIGH 2
#define WSOCK_VERSION_LOW  2
#endif

//
// Main HTTP listener thread. Launched from HTTPInitialize 
// or called directly by main() in test mode
//
DWORD WINAPI HttpListenThread(LPVOID lpv) {
	int 		err=0;
	SOCKET 		sockConnection = 0;
	WSADATA		wsadata;
	fd_set      sockSet;
	BOOL        fGracefulShutdown = FALSE;
	
#if defined (OLD_CE_BUILD) && (CE_MAJOR_VER < 3)
	// NOTE: On PalmSized PC (WinCE 2.11) trying to open accept during right after system
	// startup causes device to hang.  Sleep in for saftey.
	Sleep(15000);
#endif

	DEBUGCHK (g_fState == SERVICE_STATE_STARTING_UP);
	if (!InitializeGlobals(FALSE))
		myleave(10);

	if(WSAStartup(MAKEWORD(WSOCK_VERSION_HIGH,WSOCK_VERSION_LOW), &wsadata)) 	{
		goto done;
	}

	if (! g_pVars->CreateHTTPSocket(FALSE)) {
		myleave(2);
	}

	if (g_pVars->m_fSSL)  {
		if (! g_pVars->CreateHTTPSocket(TRUE)) {
			myleave(3);
		}
	}
	g_fState = SERVICE_STATE_ON;

	while (g_pVars->m_fAcceptConnections) {
		int iSockets;
		
		FD_ZERO(&sockSet);
		for (DWORD i =0; i < g_pVars->m_nPlainSockets + g_pVars->m_nSSLSockets; i++)
			FD_SET(g_pVars->m_sockList[i], &sockSet);

		DEBUGMSG(ZONE_LISTEN, (TEXT("HTTPD: Calling select....\r\n")));
		iSockets = select(0,&sockSet,NULL,NULL,NULL);
		if (iSockets == 0 || iSockets == SOCKET_ERROR) {
			// select failing either means there's an internal error in winsock
			// or that another thread has set the global sockets to INVALID_SOCKET.
			// Either way we're through.
			goto done;
		}

		for (i=0; i < (DWORD) iSockets; i++) {
			if (!g_pVars->m_fAcceptConnections)
				break; // we've received shutdown request partway through.
		
			if (INVALID_SOCKET == (sockConnection = accept(sockSet.fd_array[i],NULL,NULL))) 				
				goto done;

#if defined (OLD_CE_BUILD)
			SOCKADDR_IN      sockAddr;
#else
			SOCKADDR_STORAGE sockAddr;
#endif
			int              cbSockAddr = sizeof(sockAddr);

			if ((0 != getsockname(sockConnection,(PSOCKADDR) &sockAddr,&cbSockAddr))) {
				DEBUGMSG(ZONE_ERROR,(L"HTTPD: getsockname fails, GLE=0x%08x\r\n",GetLastError()));
				closesocket(sockConnection);
				goto done;
			}

			CreateHTTPConnection(sockConnection,(PSOCKADDR)&sockAddr,cbSockAddr);
		}
	}
	WaitForConnectionThreadsToEnd();
	fGracefulShutdown = TRUE;

done:
	DEBUGMSG(ZONE_LISTEN, (TEXT("HTTPD: ListenThread: exiting with fGracefulShutdown=%d, GLE=0x%08x\r\n"), fGracefulShutdown,GetLastError()));
	DEBUGCHK(g_fState == SERVICE_STATE_SHUTTING_DOWN || g_fState == SERVICE_STATE_UNLOADING || g_fState == SERVICE_STATE_OFF || g_fState == SERVICE_STATE_STARTING_UP);

	CleanupGlobalMemory(g_cWebsites);
	
	// if something went wrong then set us to off state.
	if (! fGracefulShutdown)
		g_fState = SERVICE_STATE_OFF;

	DEBUGCHK(g_hListenThread);
	g_hListenThread	= 0;  // main thread is no longer running.

	return 0;
}

DWORD WINAPI HttpConnectionThread(LPVOID lpv) {
	CHttpRequest *pRequest = (CHttpRequest*) lpv;
	DEBUGCHK(pRequest && pRequest->m_dwSig == CHTTPREQUEST_SIG);

	SOCKET sock = pRequest->m_socket;

	// this outer _try--_except is to catch crashes in the destructor
	__try {
		__try {
			// This loops as long the the socket is being kept alive 
			for(;;) {				
				pRequest->HandleRequest();
					
				// figure out if we must keep this connection alive,
				// Either session is over through this request's actions or
				// globally set to accept no more connections.

				// We do the global g_pVars->m_fAcceptConnections check 
				// because it's possible that we may be in the process of 
				// shutting down the web server, in which case we want to
				// exit even if we're performing a keep alive.
				if(! (g_pVars->m_fAcceptConnections && pRequest->m_fKeepAlive)) {
					if(g_pVars->m_fFilters)
						pRequest->CallFilter(SF_NOTIFY_END_OF_NET_SESSION);
					break;
				}

				// If we're continuing the session don't delete all data, just request specific data
				if ( ! pRequest->ReInit())
					break;
			}
		}
		__finally {
			// Note:  To get this to compile under Visual Studio on NT, the /Gx- compile line option is set
			delete pRequest;
			pRequest = 0;
		}
	}
	__except(1) {
		RETAILMSG(1, (L"HTTP Server got an exception!!!\r\n"));
		g_pVars->m_pLog->WriteEvent(IDS_HTTPD_EXCEPTION,GetExceptionCode(),GetLastError());
	}

	return 0;
}
 

//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
//  UTILITY FUNCTIONS
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
void WaitForConnectionThreadsToEnd(void) {
	DEBUGCHK (g_fState == SERVICE_STATE_SHUTTING_DOWN || g_fState == SERVICE_STATE_UNLOADING);

	//  NOTE.  It's possible ASP pages or ISAPI extns may have an 
	//  infinite loop in them, in which case we never decrement this value and
	//  never get to stop the server.  There is no clean way to fix this.

	DEBUGMSG(ZONE_LISTEN,(L"HTTPD: Wating for %d HTTP threads to come to a halt\r\n", g_pVars->m_nConnections));
	g_pVars->m_pLog->WriteEvent(IDS_HTTPD_SHUTDOWN_START);

	g_pVars->m_pThreadPool->Shutdown();
	DEBUGCHK(g_pVars->m_nConnections == 0);
	DEBUGMSG(ZONE_LISTEN,(L"HTTPD: All HTTPD threads have come to halt, shutting down server\r\n"));
}

// Note: This function is executed on the super services accept thread, blocking all 
// other services from receiving new connections.  Need to execute this code as quickly as possible.
BOOL CreateHTTPConnection(SOCKET sock, PSOCKADDR pSockAddr, DWORD cbSockAddr) {
	DEBUG_CODE_INIT;
	BOOL fRet        = FALSE;
	CHttpRequest     *pRequest = NULL;
	BOOL fSecure;

	EnterCriticalSection(&g_CritSect);
	DEBUGCHK(g_pVars->m_nConnections <= g_pVars->m_nMaxConnections);
	
	if (g_pVars->m_nConnections + 1 > g_pVars->m_nMaxConnections)  {
		DEBUGMSG(ZONE_ERROR, (TEXT("HTTPD: Connection Count -- Reached maximum # of connections, won't accept current request.")));
		send(sock,g_pVars->m_szMaxConnectionMsg,strlen(g_pVars->m_szMaxConnectionMsg),0);
		myleave(59);
	}

	fSecure = g_pVars->IsSecureSocket(pSockAddr,cbSockAddr);
	if (fSecure && !g_pVars->m_fSSL) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: Request fails because client requests SSL, SSL not activated\r\n"));
		goto done;
	}			

	pRequest = new CHttpRequest(sock,fSecure);
	if (!pRequest)
		myleave(60);

	g_pVars->m_nConnections++;

	if (! g_pVars->m_pThreadPool->ScheduleEvent(HttpConnectionThread,(LPVOID) pRequest))
		myleave(61);

	fRet = TRUE;
done:
	if (!fRet) {
		SetLastError(ERROR_SERVICE_CANNOT_ACCEPT_CTRL);
		shutdown(sock, 1);
		closesocket(sock);

		if (pRequest)
			delete pRequest; // pRequest destructor decrements m_nConnections
	}
	LeaveCriticalSection(&g_CritSect);

	return fRet;
}

// When we get a connection socket from IOCTL_SERVICE_CONNECTION, need to determine if 
// it's an SSL sock or not.  We'll only accept SSL over TCP/IP port 443, so should 
// getsockname() fail (for instance in case where service provider had a larger SOCKADDR) assume
// clear text.
BOOL CGlobalVariables::IsSecureSocket(PSOCKADDR pSockAddr, DWORD cbSockAddr) {
#if ! defined (OLD_CE_BUILD)
	if ((pSockAddr->sa_family == AF_INET || pSockAddr->sa_family == AF_INET6)) 
#else
	if ((pSockAddr->sa_family == AF_INET))
#endif
	{
		SOCKADDR_IN *pSockIn = (SOCKADDR_IN *)pSockAddr;
		return (pSockIn->sin_port == htons((WORD)m_dwSSLPort));
	}
	return FALSE;
}

// runs through active sockets and closes them during shutdown.  This will cause
// any threads waiting for recv timeouts to exit more promptly.  It will not help
// in the case of an ISAPI or ASP page in an infinite loop, only TerminateThread will
// do the trick there, and we're not willing to do this given how much it'd gunk up services.exe.
void CloseAllConnections(void) {
	EnterCriticalSection(&g_CritSect);
	CHttpRequest *pTrav = g_pRequestList;

	while (pTrav) {
		pTrav->CloseSocket();
		pTrav = pTrav->m_pNext;
		// leave it to the ~CHttpRequest to remove itself from the list and do its own memory cleanup.
	}

	LeaveCriticalSection(&g_CritSect);
}

void CGlobalVariables::CloseHTTPSockets(void)  {
	DEBUGCHK (g_fState == SERVICE_STATE_SHUTTING_DOWN || g_fState == SERVICE_STATE_UNLOADING);
	g_pVars->m_fAcceptConnections = FALSE;

	for (DWORD i = 0; i < m_nPlainSockets + m_nSSLSockets; i++) {
		if (m_sockList[i] != INVALID_SOCKET) {
			closesocket(m_sockList[i]);
			m_sockList[i] = INVALID_SOCKET;
		}
	}

#if defined (DEBUG)
	for (i = m_nPlainSockets + m_nSSLSockets; i < MAX_SOCKET_LIST; i++) {
		// make sure no sockets past this point aren't set, otherwise they won't be closed and there's a .
		DEBUGCHK(m_sockList[i] == INVALID_SOCKET);
	}
#endif
}

DWORD WINAPI PeriodicWakeupThread(LPVOID lpv) {
	EnterCriticalSection(&g_CritSect);
	if (g_pVars && g_pVars->m_pISAPICache)
		g_pVars->m_pISAPICache->RemoveUnusedISAPIs(FALSE);

	if (g_pFileLockManager)
		g_pFileLockManager->RemoveTimedOutLocks();

	g_pTimer->ScheduleEvent(PeriodicWakeupThread,0,PERIODIC_TIMER_SLEEP_TIMEOUT);
	LeaveCriticalSection(&g_CritSect);

	return 0;
}



PSTR MassageMultiStringToAnsi(PCWSTR wszIn, PCWSTR wszDefault) {
	if(!wszIn) wszIn = wszDefault;
	if(!wszIn) return NULL;

	int iAlloc = MyW2A(wszIn,0,0);
	if (!iAlloc)
		return NULL;

	iAlloc++; // +1 for dbl-null term

	PSTR szBuffer = MySzAllocA(iAlloc); 
	if (!szBuffer)
		return NULL;

	MyW2A(wszIn,szBuffer,iAlloc);
	PSTR szRead  = szBuffer;
	PSTR szWrite = szBuffer;

	// now that we've converted buffer, write out in place.
	for ( ; *szRead; szRead++, szWrite++) {
		*szWrite = (*szRead==';' ? '\0' : *szRead);

		// Ignore space between ";" and next non-space
		if (';' == *szRead || 0 == *szRead) {
			szRead++;
			svsutil_SkipWWhiteSpace(szRead);
			szRead--;		// otherwise we skip first char of new string.	
		}
	}
	szWrite[0] = szWrite[1] = 0; // dbl-null

	DEBUGCHK((szWrite - szBuffer + 2) <= iAlloc);
	return szBuffer;
}


PWSTR MassageMultiString(PCWSTR wszIn, PCWSTR wszDefault) {
	if(!wszIn) wszIn = wszDefault;
	if(!wszIn) return NULL;
	
	PWSTR wszOut = MyRgAllocNZ(WCHAR, 2+wcslen(wszIn)); // +2 for dbl-null term
	if (!wszOut)
		return NULL;

	for(PWSTR wszNext=wszOut; *wszIn; wszIn++, wszNext++) {
		*wszNext = (*wszIn==L';' ? L'\0' : *wszIn);

		// Ignore space between ";" and next non-space
		if ( L';' == *wszIn) {
			wszIn++;
			svsutil_SkipWWhiteSpace(wszIn);
			wszIn--;		// otherwise we skip first char of new string.	
		}
	}
	wszNext[0] = wszNext[1] = 0; // dbl-null
	return wszOut;
}

#if defined (OLD_CE_BUILD)
BOOL GetRemoteAddress(SOCKET sock, PSTR pszBuf, BOOL fTryHostName, DWORD cbBuf) {
	SOCKADDR_IN sockAddr;
	int iLen = sizeof(sockAddr);
	PSTR pszTemp;

	*pszBuf=0;
	if(getpeername(sock, (PSOCKADDR)&sockAddr, &iLen)) 	{
		DEBUGMSG(ZONE_SOCKET, (L"HTTPD: getpeername failed GLE=%d\r\n", GetLastError()));
		return FALSE;
	}
	if (fTryHostName)  {
		HOSTENT *pHost;
		if (NULL != (pHost = gethostbyaddr((char *)&sockAddr.sin_addr,sizeof(sockAddr.sin_addr),AF_INET)))  {
			DWORD cbHostLen = strlen(pHost->h_name);
			if (cbHostLen < cbBuf) {
				strcpy(pszBuf,pHost->h_name);
				return TRUE;
			}
		}
		// on failure through to use IP ADDR
	}
	
	if(!(pszTemp = inet_ntoa(sockAddr.sin_addr))) {
		DEBUGMSG(ZONE_SOCKET, (L"HTTPD: inet_ntoa failed GLE=%d\r\n", GetLastError()));
		return FALSE;
	}
	strcpy(pszBuf, pszTemp);
	return TRUE;
}
#else  // OLD_CE_BUILD
BOOL GetRemoteAddress(SOCKET sock, PSTR pszBuf, BOOL fTryHostName, DWORD cbBuf) {
	SOCKADDR_STORAGE sockAddr;
	int iLen = sizeof(sockAddr);

	*pszBuf=0;
	if(getpeername(sock, (PSOCKADDR)&sockAddr, &iLen)) {
		DEBUGMSG(ZONE_SOCKET, (L"HTTPD: getpeername failed GLE=%d\r\n", GetLastError()));
		return FALSE;
	}

	if (fTryHostName && (0 == getnameinfo((PSOCKADDR)&sockAddr,iLen,pszBuf,cbBuf,NULL,0,0)))
		return TRUE;

	// if getnameinfo fails on resolving host name, fall through to use IP ADDR.
	return (0 == getnameinfo((PSOCKADDR)&sockAddr,iLen,pszBuf,cbBuf,NULL,0,NI_NUMERICHOST));
}
#endif // OLD_CE_BUILD

PSTR MySzDupA(PCSTR pszIn, int iLen) { 
	if(!pszIn) return NULL;
	if(!iLen) iLen = strlen(pszIn);
	PSTR pszOut=MySzAllocA(iLen);
	if(pszOut) {
		memcpy(pszOut, pszIn, iLen); 
		pszOut[iLen] = 0;
	}
	return pszOut; 
}

PWSTR MySzDupW(PCWSTR wszIn, int iLen) { 
	if(!wszIn) return NULL;
	if(!iLen) iLen = wcslen(wszIn);
	PWSTR wszOut=MySzAllocW(iLen);
	if(wszOut) {
		memcpy(wszOut, wszIn, sizeof(WCHAR)*iLen); 
		wszOut[iLen] = 0;
	}
	return wszOut; 
}

PWSTR MySzDupAtoW(PCSTR pszIn, int iInLen) {
	PWSTR pwszOut = 0;
	int   iOutLen = MultiByteToWideChar(CP_ACP, 0, pszIn, iInLen, 0, 0);
	if(!iOutLen)
		goto error;
	pwszOut = MySzAllocW(iOutLen);
	if(!pwszOut)
		goto error;
	if(MultiByteToWideChar(CP_ACP, 0, pszIn, iInLen, pwszOut, iOutLen))
		return pwszOut;

error:
	DEBUGMSG(ZONE_ERROR, (L"HTTPD: MySzDupAtoW(%a, %d) failed. pOut=%0x08x GLE=%d\r\n", pszIn, iInLen, pwszOut, GetLastError()));
	MyFree(pwszOut);
	return FALSE;
}

PSTR MySzDupWtoA(PCWSTR wszIn, int iInLen) {
	PSTR pszOut = 0;
	int   iOutLen = WideCharToMultiByte(CP_ACP, 0, wszIn, iInLen, 0, 0, 0, 0);
	if(!iOutLen)
		goto error;
	pszOut = MySzAllocA(iOutLen);
	if(!pszOut)
		goto error;
	if(WideCharToMultiByte(CP_ACP, 0, wszIn, iInLen, pszOut, iOutLen, 0, 0))
		return pszOut;

error:
	DEBUGMSG(ZONE_ERROR, (L"HTTPD: MySzDupWtoA(%s, %d) failed. pOut=%0x08x GLE=%d\r\n", wszIn, iInLen, pszOut, GetLastError()));
	MyFree(pszOut);
	return FALSE;
}

BOOL MyStrCatA(PSTR *ppszDest, PSTR pszSource, PSTR pszDivider) {
	DEBUG_CODE_INIT;
	BOOL ret = FALSE;
	PSTR pszNew = *ppszDest;  // protect orignal ptr should realloc fail
	PSTR pszTrav;
	DWORD dwSrcLen = MyStrlenA(pszSource);
	DWORD dwDestLen = MyStrlenA(*ppszDest);
	DWORD dwDivLen = MyStrlenA(pszDivider);

	if (!pszSource)  {
		DEBUGCHK(0);
		myleave(259);
	}

	if (!pszNew)  {  // do an alloc first time, ignore divider
		if (NULL == (pszNew = MySzDupA(pszSource,dwSrcLen)))
			myleave(260);
	}
	else {
		if (NULL == (pszNew = MyRgReAlloc(char,pszNew,dwDestLen,dwSrcLen + dwDestLen + dwDivLen + 1)))
			myleave(261);

		pszTrav = pszNew + dwDestLen;
		if (pszDivider)  {
			memcpy(pszTrav, pszDivider, dwDivLen);
			pszTrav += dwDivLen;
		}
		
		strcpy(pszTrav, pszSource);
	}
	
	*ppszDest = pszNew;
	ret = TRUE;
done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"HTTPD: MyStrCat err = %d\r\n",GetLastError()));
	
	return ret;
}

//**************************************************************************
//  Component Notes

//  This is used by Filters and by Authentication components.  The only common
//  component they both rest on is core, so we include it here.
//**************************************************************************

//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
//  BASE64 ENCODE/DECODE FUNCTIONS from sicily.c
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

const int base642six[256]={
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,
    52,53,54,55,56,57,58,59,60,61,64,64,64,64,64,64,64,0,1,2,3,4,5,6,7,8,9,
    10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,64,26,27,
    28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64
};

const char six2base64[64] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z',
    '0','1','2','3','4','5','6','7','8','9','+','/'
};

//-----------------------------------------------------------------------------
// Function:  encode()
//-----------------------------------------------------------------------------
BOOL Base64Encode(
            BYTE *   bufin,          // in
            DWORD    nbytes,         // in
            char *   pbuffEncoded)   // out
{
   unsigned char *outptr;
   unsigned int   i;
   const char    *rgchDict = six2base64;

   outptr = (unsigned char *)pbuffEncoded;

   for (i=0; i < nbytes; i += 3) {
      *(outptr++) = rgchDict[*bufin >> 2];            /* c1 */
      *(outptr++) = rgchDict[((*bufin << 4) & 060) | ((bufin[1] >> 4) & 017)]; /*c2*/
      *(outptr++) = rgchDict[((bufin[1] << 2) & 074) | ((bufin[2] >> 6) & 03)];/*c3*/
      *(outptr++) = rgchDict[bufin[2] & 077];         /* c4 */

      bufin += 3;
   }

   /* If nbytes was not a multiple of 3, then we have encoded too
    * many characters.  Adjust appropriately.
    */
   if(i == nbytes+1) {
      /* There were only 2 bytes in that last group */
      outptr[-1] = '=';
   } else if(i == nbytes+2) {
      /* There was only 1 byte in that last group */
      outptr[-1] = '=';
      outptr[-2] = '=';
   }

   *outptr = '\0';

   return TRUE;
}


//-----------------------------------------------------------------------------
// Function:  decode()
//-----------------------------------------------------------------------------
BOOL Base64Decode(
            char   * bufcoded,       // in
            char   * pbuffdecoded,   // out
            DWORD  * pcbDecoded)     // in out
{
    int            nbytesdecoded;
    char          *bufin;
    unsigned char *bufout;
    int            nprbytes;
    const int     *rgiDict = base642six;

    /* Strip leading whitespace. */

    while(*bufcoded==' ' || *bufcoded == '\t') bufcoded++;

    /* Figure out how many characters are in the input buffer.
     * If this would decode into more bytes than would fit into
     * the output buffer, adjust the number of input bytes downwards.
     */
    bufin = bufcoded;
    while(rgiDict[*(bufin++)] <= 63);
    nprbytes = bufin - bufcoded - 1;
    nbytesdecoded = ((nprbytes+3)/4) * 3;

    if ( pcbDecoded )
        *pcbDecoded = nbytesdecoded;

    bufout = (unsigned char *)pbuffdecoded;

    bufin = bufcoded;

    while (nprbytes > 0) {
        *(bufout++) =
            (unsigned char) (rgiDict[*bufin] << 2 | rgiDict[bufin[1]] >> 4);
        *(bufout++) =
            (unsigned char) (rgiDict[bufin[1]] << 4 | rgiDict[bufin[2]] >> 2);
        *(bufout++) =
            (unsigned char) (rgiDict[bufin[2]] << 6 | rgiDict[bufin[3]]);
        bufin += 4;
        nprbytes -= 4;
    }

    if(nprbytes & 03) {
        if(rgiDict[bufin[-2]] > 63)
            nbytesdecoded -= 2;
        else
            nbytesdecoded -= 1;
    }

    ((CHAR *)pbuffdecoded)[nbytesdecoded] = '\0';

    return TRUE;
}

/*===================================================================
strcpyEx

Copy one string to another, returning a pointer to the NUL character
in the destination

Parameters:
    szDest - pointer to the destination string
    szSrc - pointer to the source string

Returns:
    A pointer to the NUL terminator is returned.
===================================================================*/

char *strcpyEx(char *szDest, const char *szSrc) {
	while (*szDest++ = *szSrc++)
		;
	return szDest - 1;
}

#if defined(OLD_CE_BUILD)
#if (_WIN32_WCE < 210)
VOID GetCurrentFT(LPFILETIME lpFileTime)  {
	SYSTEMTIME st;
	GetSystemTime(&st);
	SystemTimeToFileTime(&st,lpFileTime);
	LocalFileTimeToFileTime(lpFileTime,lpFileTime);
}
#endif
#endif // OLD_CE_BUILD


