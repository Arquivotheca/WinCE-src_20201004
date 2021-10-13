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
HINSTANCE g_hInst;                    // handle to DLL
DWORD     g_fState;                   // Current Service State (running, stopped, starting, shutting down, unloading)
// Global lock.  Callers can hold onto this for moderatly long periods of time (eg
// refreshing registry settings, cleaning WebDAV locks, freeing ISAPI cache) but
// obviously shouldn't do any long-running fcns because the IOCTL_SERVICE_START,etc
// use this critical section also.
CRITICAL_SECTION g_CritSect;


CWebDavFileLockManager *g_pFileLockManager;  // Implements LOCK and UNLOCK support for WebDav.
SVSThreadPool *g_pTimer;                     // ISAPI extension unload and WebDAV file unlock timer thread (wakes up once a minute).

// We keep track of whether we've spun a thread in this function to deal with
// stopping+restarting, in the event that when we're unloa
HANDLE g_hAdminThread;

CHttpRequest *g_pRequestList;       // List of active http connections.

BOOL g_fUTF8;

//------------- Const data -----------------------
//

const char cszTextHtml[] = "text/html";
const char cszOctetStream[] = "application/octet-stream";
const char cszEmpty[] = "";
const char cszMaxConnectionHeader[] =  "HTTP/1.0 503 Service Unavailable\r\n\r\n";

// 
//------------- Debug data -----------------------
//
#if defined(UNDER_CE) && defined (DEBUG)
  DBGPARAM dpCurSettings = 
  {
    TEXT("HTTPD"), 
    {
        TEXT("Error"),TEXT("Init"),TEXT("Listen"),TEXT("Socket"),
        TEXT("Request"),TEXT("Response"),TEXT("ISAPI"),TEXT("VROOTS"),
        TEXT("ASP"),TEXT("Device"),TEXT("SSL"),TEXT("Verbose"),
        TEXT("Authentication"),TEXT("WebDAV"),TEXT("Parser"),TEXT("Tokens") 
    },
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

extern "C" BOOL WINAPI DllEntry( HANDLE hInstDll, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason) 
    {
        case DLL_PROCESS_ATTACH:
            DEBUGMSG(ZONE_INIT,(TEXT("HTTPD: DllMain attached\r\n")));
            g_hAdminThread = 0;
            g_hInst = (HINSTANCE)hInstDll;    
            svsutil_Initialize();
            DEBUGREGISTER((HINSTANCE)hInstDll);

            InitializeCriticalSection(&g_CritSect);
            DisableThreadLibraryCalls((HMODULE)hInstDll);

            g_fState = SERVICE_STATE_UNINITIALIZED;
            break;

        case DLL_PROCESS_DETACH:
            DEBUGMSG(ZONE_INIT, (TEXT("HTTPD: DllMain detach\r\n")));
            DeleteCriticalSection(&g_CritSect);
            svsutil_DeInitialize();
            DEBUGCHK(g_fState == SERVICE_STATE_UNLOADING && !g_pVars && !g_pWebsites && (g_cWebsites==0) && !g_pTimer);
            break;
    }
    return TRUE;
}

extern "C" int HttpInitialize(TCHAR *szRegPath) 
{
    // This was to support httpd being started in CE 3.0 - in the device.exe days
    // Leave the function intact because it's in httpd.def, but no longer
    // do anything here.  Apps need to do the right thing here and use
    // standard services.exe API's to start + stop HTTPD.
    DEBUGMSG(ZONE_ERROR,(L"HTTPD:HttpInitialize is no longer supported - use standard services.exe functions to use this\r\n"));
    DEBUGCHK(0);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}


// Routines that are called once after HTTPD.dll has been started, but are not safe to be called from DLLMain().
BOOL HttpInitializeOnce(void) 
{
    g_pTimer = new SVSThreadPool(1);
    if (!g_pTimer)
    {
        return FALSE;
    }

    g_pSecurePortList = new CSecurePortList();
    if (! g_pSecurePortList || !g_pSecurePortList->InitSecureList())
    {
        return FALSE;
    }

    svsutil_InitializeInterfaceMapperOnce();
    svsutil_ResetInterfaceMapper();
    DoesSystemSupportUTF8();

    g_pTimer->ScheduleEvent(PeriodicWakeupThread,0,PERIODIC_TIMER_SLEEP_TIMEOUT);
    return TRUE;
}

extern "C" int HttpInitializeFromExe()  
{
    // No longer supported - HttpInitialize sets proper error code for this call
    return HttpInitialize(NULL);
}

const WCHAR wszDefaultLogDirectory[] = L"\\windows\\www";

BOOL IsHttpdEnabled(void) 
{
    CReg reg(HKEY_LOCAL_MACHINE, RK_HTTPD);
    if (reg.ValueDW(RV_ISENABLED,1))
    {
        return TRUE;
    }

    SetLastError(ERROR_SERVICE_DISABLED);

    DEBUGMSG(ZONE_INIT,(L"HTTPD: IsEnabled=0, will not start web server\r\n"));
    DWORD dwMaxLogSize;
    WCHAR wszLogDir[MAX_PATH + 1];

    if (0 == (dwMaxLogSize = reg.ValueDW(RV_MAXLOGSIZE)))
    {
        return FALSE;
    }

    if ( ! reg.ValueSZ(RV_LOGDIR,wszLogDir,MAX_PATH+1))
    {
        wcscpy(wszLogDir,wszDefaultLogDirectory);
    }

    CLog cLog(dwMaxLogSize,wszLogDir);
    cLog.WriteEvent(IDS_HTTPD_DISABLED);

    return FALSE;
}

#define STACK_SIZE (32*4096)

void CGlobalVariables::InitGlobals(CReg *pWebsite, const WCHAR *szSiteName)  
{
    DWORD dwMaxLogSize;
    WCHAR wszLogDir[MAX_PATH + 1];

    ZEROMEM(this);

    CReg reg;
    m_fRootSite = !pWebsite;

    // if pWebsite==NULL, then this is initialization of default website.
    if (m_fRootSite) 
    {
        DEBUGCHK(g_pVars == NULL);
        m_szSiteName[0] = L'0';

        // certain functions, including listen thread management and ISAPI Filters
        // are managed globally by the main site.

        reg.Open(HKEY_LOCAL_MACHINE, RK_HTTPD);
        if (!reg.IsOK())   
        {
            CLog cLog(DEFAULTLOGSIZE,L"\\windows\\www");
            cLog.WriteEvent(IDS_HTTPD_NO_REGKEY);
            DEBUGMSG(ZONE_ERROR,(L"HTTPD:  No registry key setup, will not handle requests\r\n"));
            return;
        }
        pWebsite = &reg;

        if (! IsHttpdEnabled())
        {
            return;
        }

        // thread pool is global
        m_nMaxConnections = pWebsite->ValueDW(RV_MAXCONNECTIONS,10);
        m_pThreadPool = new SVSThreadPool(m_nMaxConnections + 1, STACK_SIZE);  // +1 for ISAPI Cache removal thread
        if (!m_pThreadPool)
        {
            return;
        }

        // initial string setup only needs to be done once.
        if (NULL == (m_pszStatusBodyBuf = MyRgAllocNZ(CHAR,BODYSTRINGSIZE)))
        {
            return;
        }

        InitializeResponseCodes(m_pszStatusBodyBuf);

        strcpy(m_szMaxConnectionMsg,cszMaxConnectionHeader);
        WCHAR wszMaxConnectionMsg[256];

        LoadString(g_hInst,IDS_SERVER_BUSY,wszMaxConnectionMsg,ARRAYSIZEOF(wszMaxConnectionMsg));
        MyW2A(wszMaxConnectionMsg,m_szMaxConnectionMsg + sizeof(cszMaxConnectionHeader) - 1,
                  sizeof(m_szMaxConnectionMsg) - sizeof(cszMaxConnectionHeader));

        m_fUseDefaultSite = pWebsite->ValueDW(RV_ALLOW_DEFAULTSITE,TRUE);

        // setup logging
        dwMaxLogSize = pWebsite->ValueDW(RV_MAXLOGSIZE);
        if ( ! pWebsite->ValueSZ(RV_LOGDIR,wszLogDir,MAX_PATH+1)) 
        {
            wcscpy(wszLogDir,wszDefaultLogDirectory);
        }

        WCHAR *wszBasicRealm;
        if (NULL != (wszBasicRealm = (WCHAR*)pWebsite->ValueSZ(RV_BASIC_REALM))) 
        {
            if (NULL == (m_szBasicRealm = MySzDupWtoA(wszBasicRealm)))
            {
                return;
            }
        }
        else 
        {
            static const CHAR cszBasicRealmDefault[] = "Microsoft-WinCE";
            if (NULL == (m_szBasicRealm = MySzDupA(cszBasicRealmDefault)))
            {
                return;
            }
        }

        m_pLog = new CLog(dwMaxLogSize,wszLogDir);
        if (!m_pLog)
        {
            return;
        }

        // max POST/Header sizes, 
        m_dwPostReadSize       = pWebsite->ValueDW(RV_POSTREADSIZE,  48*1024); // 48 K default
        if (m_dwPostReadSize < MIN_POST_READ_SIZE) 
        {
            DEBUGMSG(ZONE_ERROR,(L"HTTPD: PostReadSize set in registry = %d, however minimum must be %d.  Resetting value to minimum acceptable\r\n",m_dwPostReadSize,MIN_POST_READ_SIZE));
            m_dwPostReadSize = MIN_POST_READ_SIZE;
        }
        
        m_dwMaxHeaderReadSize  = pWebsite->ValueDW(RV_MAXHEADERSIZE, 48*1024); // 48 K default
        m_dwSystemChangeNumber = pWebsite->ValueDW(RV_CHANGENUMBER,  1);
        m_dwConnectionTimeout  = pWebsite->ValueDW(RV_CONN_TIMEOUT,30*1000); // 30 second default

        m_fExtensions = InitExtensions(pWebsite);
    }
    else 
    {
        DEBUGCHK(g_pVars);
        DEBUGCHK(wcslen(szSiteName) < ARRAYSIZEOF(m_szSiteName));
        StringCchCopyW(m_szSiteName,ARRAYSIZEOF(m_szSiteName),szSiteName);
    }

    //  nagling disable option
    m_fDisableNagling = pWebsite->ValueDW(RV_DISABLE_NAGLING, HTTPD_NAGLING_DEFAULT);

    m_dwListenPort = pWebsite->ValueDW(RV_PORT, IPPORT_HTTP);
    DEBUGCHK(m_dwListenPort);

    m_fASP            = InitASP(pWebsite);
    m_fWebDav         = InitWebDav(pWebsite);
    m_fDirBrowse      = pWebsite->ValueDW(RV_DIRBROWSE, HTTPD_ALLOW_DIR_BROWSE);
    
    m_wszDefaultPages = MassageMultiString(pWebsite->ValueSZ(RV_DEFAULTPAGE),HTTPD_DEFAULT_PAGES);

    const WCHAR *sz = pWebsite->ValueSZ(RV_ADMINUSERS);
    if (sz)
    {
        m_wszAdminUsers =  svsutil_wcsdup((WCHAR*)sz);
    }
    else
    {
        m_wszAdminUsers = NULL;
    }

    InitAuthentication(pWebsite);
    if (m_fRootSite)
    {
        InitSSL(pWebsite);
    }

    WCHAR *wszServerID;
    if (NULL != (wszServerID = (WCHAR*)pWebsite->ValueSZ(RV_SERVER_ID))) 
    {
        // User specified an over-ride server ID, convert to ANSI and use it.
        if (NULL == (m_szServerID = MySzDupWtoA(wszServerID)))
        {
            return;
        }
    }
    else 
    {
        // Use default server ID if not overridden
        static const char szDefaultServerIdFmt[]    = "Microsoft-WinCE/%d.%02d";

        if (NULL == (m_szServerID =  MyRgAllocNZ(CHAR,CCHSIZEOF(szDefaultServerIdFmt)+20)))
        {
            return;
        }

        sprintf(m_szServerID,szDefaultServerIdFmt,CE_MAJOR_VER,CE_MINOR_VER);
    }

    InitMapping(*pWebsite,RV_HOSTEDSITES); // SVSInterfaceMapper does hard work here.

    m_pVroots = new CVRoots(pWebsite,m_fDirBrowse,m_fBasicAuth, m_fNTLMAuth, m_fNegotiateAuth);
    if (!m_pVroots)
    {
        return;
    }

    m_pVroots->AddRef();

    // vroot table must be initialized or web server can't return files.  Warn
    // user if this is not the case
    if ((m_pVroots->Count() == 0) && m_fRootSite) 
    {
        m_pLog->WriteEvent(IDS_HTTPD_NO_VROOTS);
    }

    m_fAcceptConnections = TRUE;
}


void CGlobalVariables::DeInitGlobals(void)  
{
    ULONG   index = 0;
    // note that certain variables (like ISAPIs and logging) are only
    // initialized on the default web site.
    DEBUGCHK(g_pVars->m_nConnections == 0);
    DEBUGCHK(g_fState != SERVICE_STATE_ON);

    MyFree(m_wszDefaultPages);
    MyFree(m_wszAdminUsers);
    MyFree(m_pszStatusBodyBuf);
    MyFree(m_pszSSLIssuer);
    MyFree(m_pszSSLSubject);
    MyFree(m_szServerID);
    MyFree(m_szBasicRealm);
    MyFree(m_SpnNameCheckList);

    if (m_ChannelBindingInfo.pServiceNames)
    {
        for (index = 0; index < m_ChannelBindingInfo.NumberOfServiceNames; index++)
        {
            MyFree(m_ChannelBindingInfo.pServiceNames[index].Buffer);
        }

        MyFree(m_ChannelBindingInfo.pServiceNames);
    }   

    if (m_pVroots) 
    {
        m_pVroots->DelRef();
    }

    if (m_fRootSite) 
    {
        DEBUGCHK(this == g_pVars);

        if (m_fWebDav && (g_fState == SERVICE_STATE_UNLOADING))
        {
            DeInitWebDav(); // only on unload because lock manager persists across refreshes
        }
    
        if (m_fFilters)
        {
            CleanupFilters();
        }

        if (m_fExtensions)
        {
            DeInitExtensions();
        }

        if (m_pISAPICache) 
        {
            // Flush all ISAPIs.  This will block until everyone's unloaded.
            m_pISAPICache->RemoveUnusedISAPIs(TRUE);
            delete m_pISAPICache;
        }

        if (m_pThreadPool)
        {
            delete m_pThreadPool;
        }

        if (m_fSSL)
        {
            FreeSSLResources();
        }

        MyFreeLib(m_hSecurityLib);

        // Log must be the last thing we destroy, because theoretically
        // a cleanup function could make a call into it to report an error condition.
        if (m_pLog)
        {
            delete m_pLog;
        }

        m_dwSystemChangeNumber++;
        CReg reg(HKEY_LOCAL_MACHINE,RK_HTTPD);
        reg.SetDW(RV_CHANGENUMBER,m_dwSystemChangeNumber);
    }
#if defined (DEBUG)
    else 
    {
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


// Forces virtual root table for this website to be re-read
BOOL CGlobalVariables::RefreshVrootTable(void) 
{
    CReg regWebsite;

    if (m_fRootSite) 
    {
        regWebsite.Open(HKEY_LOCAL_MACHINE,RK_HTTPD);
    }
    else 
    {
        WCHAR szWebsiteReg[MAX_PATH*2];
        StringCchPrintf(szWebsiteReg,ARRAYSIZEOF(szWebsiteReg),L"%s\\%s",
                        RK_WEBSITES,m_szSiteName);
        regWebsite.Open(HKEY_LOCAL_MACHINE,szWebsiteReg);
    }

    if (! regWebsite.IsOK()) 
    {
        // Leave existing VRoots in place if there was some failure.  There's no
        // great way to handle this, because HTTPD doesn't have a mechanism for
        // an internal error like this causing it to come down during the middle
        // of its processing of regular requests.
        return FALSE;
    }

    CVRoots *pNewVRoots = new CVRoots(&regWebsite,m_fDirBrowse,m_fBasicAuth, m_fNTLMAuth, m_fNegotiateAuth);
    if (! pNewVRoots)
    {
        return FALSE;
    }

    // Set initial ref count of pNewVRoots==1
    pNewVRoots->AddRef();

    // Deref pre-existing VRoots, will be deleted either now or when last
    // active request finishes using them.
    if (m_pVroots)
    {
        m_pVroots->DelRef();
    }

    m_pVroots = pNewVRoots;
    return TRUE;
}

DWORD WINAPI InitializeGlobalsThread(LPVOID lpv) 
{
    MyWaitForAdminThreadReadyState();

    DEBUGCHK(g_fState == SERVICE_STATE_STARTING_UP);

    InitializeGlobals();
    CloseHandle(g_hAdminThread);
    g_hAdminThread = 0;
    return 0;
}


// This causes the VRoot table of the web server to be refreshed, without 
// forcing existing connections to terminate first.
BOOL RefreshVRootTable(void) 
{
    SVSLocalCriticalSection critSect(&g_CritSect);
    DWORD i;

    if (g_pVars && !g_pVars->RefreshVrootTable())
    {
        return FALSE;
    }

    for (i = 0; i < g_cWebsites; i++) 
    {
        if (! g_pWebsites[i].RefreshVrootTable())
        {
            return FALSE;
        }
    }
    return TRUE;
}


DWORD WINAPI HttpConnectionThread(LPVOID lpv) 
{
    CHttpRequest *pRequest = (CHttpRequest*) lpv;
    DEBUGCHK(pRequest && pRequest->m_dwSig == CHTTPREQUEST_SIG);

    SOCKET sock = pRequest->m_socket;

    // this outer _try--_except is to catch crashes in the destructor
    __try 
    {
        __try 
        {
            // This loops as long the the socket is being kept alive 
            for(;;) 
            {                
                pRequest->HandleRequest();
                    
                // figure out if we must keep this connection alive,
                // Either session is over through this request's actions or
                // globally set to accept no more connections.

                // We do the global g_pVars->m_fAcceptConnections check 
                // because it's possible that we may be in the process of 
                // shutting down the web server, in which case we want to
                // exit even if we're performing a keep alive.
                if(! (g_pVars->m_fAcceptConnections && pRequest->GetConnectionPersist())) 
                {
                    if(g_pVars->m_fFilters)
                    {
                        pRequest->CallFilter(SF_NOTIFY_END_OF_NET_SESSION);
                    }
                    break;
                }

                // If we're continuing the session don't delete all data, just request specific data
                if ( ! pRequest->ReInit())
                {
                    break;
                }
            }
        }
        __finally 
        {
            // Note:  To get this to compile under Visual Studio on NT, the /Gx- compile line option is set
            delete pRequest;
            pRequest = 0;
        }
    }
    __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) 
    {
        RETAILMSG(1, (L"HTTP Server got an exception!!!\r\n"));
        g_pVars->m_pLog->WriteEvent(IDS_HTTPD_EXCEPTION,GetExceptionCode(),GetLastError());
    }

    return 0;
}
 

//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
//  UTILITY FUNCTIONS
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
void WaitForConnectionThreadsToEnd(void) 
{
    DEBUGCHK (g_fState == SERVICE_STATE_SHUTTING_DOWN || g_fState == SERVICE_STATE_UNLOADING);

    //  NOTE.  It's possible ASP pages or ISAPI extns may have an 
    //  infinite loop in them, in which case we never decrement this value and
    //  never get to stop the server.  There is no clean way to fix this.

    DEBUGMSG(ZONE_LISTEN,(L"HTTPD: Wating for %d HTTP threads to come to a halt\r\n", g_pVars->m_nConnections));
    g_pVars->m_pLog->WriteEvent(IDS_HTTPD_SHUTDOWN_START);

    g_pVars->m_pThreadPool->Shutdown();
    
    DEBUGMSG(ZONE_LISTEN,(L"HTTPD: All HTTPD threads have come to halt, shutting down server\r\n"));
}

// Note: This function is executed on the super services accept thread, blocking all 
// other services from receiving new connections.  Need to execute this code as quickly as possible.
BOOL CreateHTTPConnection(SOCKET sock, PSOCKADDR pSockAddr, DWORD cbSockAddr) 
{
    DEBUG_CODE_INIT;
    BOOL fRet        = FALSE;
    CHttpRequest     *pRequest = NULL;
    BOOL fSecure;

    EnterCriticalSection(&g_CritSect);
    DEBUGCHK(g_pVars->m_nConnections <= g_pVars->m_nMaxConnections);
    
    if (g_pVars->m_nConnections + 1 > g_pVars->m_nMaxConnections)  
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("HTTPD: Connection Count -- Reached maximum # of connections, won't accept current request.")));
        send(sock,g_pVars->m_szMaxConnectionMsg,strlen(g_pVars->m_szMaxConnectionMsg),0);
        myleave(59);
    }

    fSecure = g_pVars->IsSecureSocket(pSockAddr,cbSockAddr);
    if (fSecure && !g_pVars->m_fSSL) 
    {
        DEBUGMSG(ZONE_ERROR,(L"HTTPD: Request fails because client requests SSL, SSL not activated\r\n"));
        goto done;
    }            

    pRequest = new CHttpRequest(sock,fSecure);
    if (!pRequest)
    {
        myleave(60);
    }

    g_pVars->m_nConnections++;

    if (! g_pVars->m_pThreadPool->ScheduleEvent(HttpConnectionThread,(LPVOID) pRequest))
    {
        myleave(61);
    }

    fRet = TRUE;
done:
    if (!fRet) 
    {
        SetLastError(ERROR_SERVICE_CANNOT_ACCEPT_CTRL);
        shutdown(sock, 1);
        closesocket(sock);

        if (pRequest)
        {
            delete pRequest; // pRequest destructor decrements m_nConnections
        }
    }
    LeaveCriticalSection(&g_CritSect);

    return fRet;
}

// When we get a connection socket from IOCTL_SERVICE_CONNECTION, need to determine if 
// it's an SSL sock or not.  We'll only accept SSL over TCP/IP port 443, so should 
// getsockname() fail (for instance in case where service provider had a larger SOCKADDR) assume
// clear text.
BOOL CGlobalVariables::IsSecureSocket(PSOCKADDR pSockAddr, DWORD cbSockAddr) 
{
    // First, check "default" SSL port (443 unless it was overridden in registry)
    if ((pSockAddr->sa_family == AF_INET || pSockAddr->sa_family == AF_INET6)) 
    {
        if (GetSocketPort(pSockAddr) == (WORD)m_dwSSLPort)
        {
            return TRUE;
        }
    }

    // It's also possible user indicated that the port should be secure via 
    // IOCTL_HTTPD_SET_PORT_SECURE.  Check below.
    return g_pSecurePortList->IsPortInSecureList(GetSocketPort(pSockAddr));
}

// runs through active sockets and closes them during shutdown.  This will cause
// any threads waiting for recv timeouts to exit more promptly.  It will not help
// in the case of an ISAPI or ASP page in an infinite loop, only TerminateThread will
// do the trick there, and we're not willing to do this given how much it'd gunk up services.exe.
void CloseAllConnections(void) 
{
    EnterCriticalSection(&g_CritSect);
    CHttpRequest *pTrav = g_pRequestList;

    while (pTrav) 
    {
        pTrav->CloseSocket();
        pTrav = pTrav->m_pNext;
        // leave it to the ~CHttpRequest to remove itself from the list and do its own memory cleanup.
    }

    LeaveCriticalSection(&g_CritSect);
}

DWORD WINAPI PeriodicWakeupThread(LPVOID lpv) 
{
    EnterCriticalSection(&g_CritSect);
    if (g_pVars && g_pVars->m_pISAPICache)
    {
        g_pVars->m_pISAPICache->RemoveUnusedISAPIs(FALSE);
    }

    if (g_pFileLockManager)
    {
        g_pFileLockManager->RemoveTimedOutLocks();
    }

    g_pTimer->ScheduleEvent(PeriodicWakeupThread,0,PERIODIC_TIMER_SLEEP_TIMEOUT);
    LeaveCriticalSection(&g_CritSect);

    return 0;
}

PSTR MassageMultiStringToAnsi(PCWSTR wszIn, PCWSTR wszDefault) 
{
    if(!wszIn) wszIn = wszDefault;
    if(!wszIn) return NULL;

    int iAlloc = MyW2A(wszIn,0,0);
    if (!iAlloc)
    {
        return NULL;
    }

    iAlloc++; // +1 for dbl-null term

    PSTR szBuffer = MySzAllocA(iAlloc); 
    if (!szBuffer)
    {
        return NULL;
    }

    MyW2A(wszIn,szBuffer,iAlloc);
    PSTR szRead  = szBuffer;
    PSTR szWrite = szBuffer;

    // now that we've converted buffer, write out in place.
    for ( ; *szRead; szRead++, szWrite++) 
    {
        PREFAST_SUPPRESS(394,"Updating buffer pointer is safe here");
        *szWrite = (*szRead==';' ? '\0' : *szRead);

        // Ignore space between ";" and next non-space
        if (';' == *szRead || 0 == *szRead) 
        {
            szRead++;
            svsutil_SkipWhiteSpace(szRead);
            szRead--;        // otherwise we skip first char of new string.    
        }
    }
    szWrite[0] = szWrite[1] = 0; // dbl-null

    DEBUGCHK((szWrite - szBuffer + 2) <= iAlloc);
    return szBuffer;
}


PWSTR MassageMultiString(PCWSTR wszIn, PCWSTR wszDefault) 
{
    if(!wszIn) 
    {
        wszIn = wszDefault;
    }
    if(!wszIn) 
    {
        return NULL;
    }
    
    PWSTR wszOut = MyRgAllocNZ(WCHAR, 2+wcslen(wszIn)); // +2 for dbl-null term
    if (!wszOut)
    {
        return NULL;
    }

    for(PWSTR wszNext=wszOut; *wszIn; wszIn++, wszNext++) 
    {
        PREFAST_SUPPRESS(394,"Updating buffer pointer is safe here");
        *wszNext = (*wszIn==L';' ? L'\0' : *wszIn);

        // Ignore space between ";" and next non-space
        if ( L';' == *wszIn) 
        {
            wszIn++;
            svsutil_SkipWWhiteSpace(wszIn);
            wszIn--;        // otherwise we skip first char of new string.    
        }
    }
    wszNext[0] = wszNext[1] = 0; // dbl-null
    return wszOut;
}


BOOL GetRemoteAddress(SOCKET sock, PSTR pszBuf, BOOL fTryHostName, DWORD cbBuf) 
{
    SOCKADDR_STORAGE sockAddr;
    int iLen = sizeof(sockAddr);

    *pszBuf=0;
    if(getpeername(sock, (PSOCKADDR)&sockAddr, &iLen)) 
    {
        DEBUGMSG(ZONE_SOCKET, (L"HTTPD: getpeername failed GLE=%d\r\n", GetLastError()));
        return FALSE;
    }

    if (fTryHostName && (0 == getnameinfo((PSOCKADDR)&sockAddr,iLen,pszBuf,cbBuf,NULL,0,0)))
    {
        return TRUE;
    }

    // if getnameinfo fails on resolving host name, fall through to use IP ADDR.
    return (0 == getnameinfo((PSOCKADDR)&sockAddr,iLen,pszBuf,cbBuf,NULL,0,NI_NUMERICHOST));
}

PSTR MySzDupA(PCSTR pszIn, int iLen) 
{ 
    if(!pszIn) 
    {
        return NULL;
    }
    if(!iLen) 
    {
        iLen = strlen(pszIn);
    }
    PSTR pszOut=MySzAllocA(iLen);
    if(pszOut) 
    {
        memcpy(pszOut, pszIn, iLen); 
        pszOut[iLen] = 0;
    }
    return pszOut; 
}

PWSTR MySzDupW(PCWSTR wszIn, int iLen) 
{ 
    if(!wszIn) 
    {
        return NULL;
    }
    if(!iLen) 
    {
        iLen = wcslen(wszIn);
    }
    PWSTR wszOut=MySzAllocW(iLen);
    if(wszOut) 
    {
        memcpy(wszOut, wszIn, sizeof(WCHAR)*iLen); 
        wszOut[iLen] = 0;
    }
    return wszOut; 
}

PWSTR MySzDupAtoW(PCSTR pszIn, int iInLen) 
{
    PWSTR pwszOut = 0;
    int   iOutLen = MultiByteToWideChar(CP_ACP, 0, pszIn, iInLen, 0, 0);
    if(!iOutLen)
    {
        goto error;
    }
    pwszOut = MySzAllocW(iOutLen);
    if(!pwszOut)
    {
        goto error;
    }
    if(MultiByteToWideChar(CP_ACP, 0, pszIn, iInLen, pwszOut, iOutLen))
    {
        return pwszOut;
    }

error:
    DEBUGMSG(ZONE_ERROR, (L"HTTPD: MySzDupAtoW(%a, %d) failed. pOut=%0x08x GLE=%d\r\n", pszIn, iInLen, pwszOut, GetLastError()));
    MyFree(pwszOut);
    return FALSE;
}

PSTR MySzDupWtoA(PCWSTR wszIn, int iInLen) 
{
    PSTR pszOut = 0;
    int   iOutLen = WideCharToMultiByte(CP_ACP, 0, wszIn, iInLen, 0, 0, 0, 0);
    if(!iOutLen)
    {
        goto error;
    }
    pszOut = MySzAllocA(iOutLen);
    if(!pszOut)
    {
        goto error;
    }
    if(WideCharToMultiByte(CP_ACP, 0, wszIn, iInLen, pszOut, iOutLen, 0, 0))
    {
        return pszOut;
    }

error:
    DEBUGMSG(ZONE_ERROR, (L"HTTPD: MySzDupWtoA(%s, %d) failed. pOut=%0x08x GLE=%d\r\n", wszIn, iInLen, pszOut, GetLastError()));
    MyFree(pszOut);
    return FALSE;
}

BOOL MyStrCatA(PSTR *ppszDest, PSTR pszSource, PSTR pszDivider) 
{
    DEBUG_CODE_INIT;
    BOOL ret = FALSE;
    PSTR pszNew = *ppszDest;  // protect orignal ptr should realloc fail
    PSTR pszTrav;
    DWORD dwSrcLen = MyStrlenA(pszSource);
    DWORD dwDestLen = MyStrlenA(*ppszDest);
    DWORD dwDivLen = MyStrlenA(pszDivider);

    if (!pszSource)  
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        myleave(259);
    }

    if (!pszNew)  
    {  // do an alloc first time, ignore divider
        if (NULL == (pszNew = MySzDupA(pszSource,dwSrcLen)))
        {
            myleave(260);
        }
    }
    else 
    {
        if (NULL == (pszNew = MyRgReAlloc(char,pszNew,dwDestLen,dwSrcLen + dwDestLen + dwDivLen + 1)))
        {
            myleave(261);
        }

        pszTrav = pszNew + dwDestLen;
        if (pszDivider)  
        {
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

char *strcpyEx(char *szDest, const char *szSrc) 
{
    while (*szDest++ = *szSrc++)
    {
        ;
    }
    return szDest - 1;
}

// Changes HTTPD server state to new setting.  We grab critical sections
// despite this being an atomic write just to keep things a bit simpler.
void SetWebServerState(DWORD dwNewState) 
{
    SVSLocalCriticalSection critSect(&g_CritSect);
#ifdef DEBUG
    if (g_fState != dwNewState) 
    {
        DEBUGMSG(ZONE_INIT,(L"HTTPD: Changing server state from state <%d> to state <%d>\r\n",g_fState,dwNewState));
    }
#endif
    g_fState = dwNewState;
}

BOOL SetWebServerState(DWORD dwNewState, DWORD dwRequiredOldState) 
{
    SVSLocalCriticalSection critSect(&g_CritSect);
    if (dwRequiredOldState != g_fState) 
    {
        // Don't DEBUGMSG(ZONE_ERROR) here because it's not always a serious error worth
        // calling dev's attention to, for instance on unloading we accept
        // multiple different oldStates and may have to call this fcn multiple times.
        return FALSE;
    }
    DEBUGMSG(ZONE_INIT,(L"HTTPD: Changing server state from state <%d> to state <%d>\r\n",g_fState,dwNewState));
    g_fState = dwNewState;
    return TRUE;
}




