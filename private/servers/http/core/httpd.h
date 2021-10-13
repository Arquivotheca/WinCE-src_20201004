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
Module Name: HTTPD.H
Abstract: Global defns for the HTTP server
--*/

#ifndef _HTTPD_H_
#define _HTTPD_H_


#ifndef UNDER_CE
#define SECURITY_WIN32
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#if ! defined (UNDER_CE)
typedef PVOID INTERLOCKED_COMP, *PINTERLOCKED_COMP;
#else
typedef LONG  INTERLOCKED_COMP, *PINTERLOCKED_COMP;
typedef LONG INTERLOCKED_RESULT;
#endif // UNDER_CE

#include <stdio.h>
#include <strsafe.h>
#include <httpext.h>
#include <httpfilt.h>
#include <service.h>
#include <msxml2.h>

#include <httpdrc.h>
#include <creg.hxx>


#include <servutil.h>
#include <httpcomn.h>
#ifdef UNDER_CE
#include <sspi.h>
#include <wincrypt.h>
#else
#include <security.h>
#endif

#include <schnlsp.h>
#include <iphlpapi.h>

#ifdef UNDER_CE
#include <windbase.h>
#include <extfile.h>
#include <bldver.h>
#if defined (DEBUG)
#include <windev.h>
#endif // DEBUG
#endif // UNDER_CE

// Doctor Watson error reporting header file.
#include <errorrep.h>

// Note that any code in HTTPD related to processing a request will NOT wrap
// individual uses of safeInt in a __try/__except block, because the entire
// HTTP request processing itself is already in a __try/__except in HttpConnectionThread().  
// If there is an integer overflow, we want the HTTP request to come to a swift end.
#include <safeInt.hxx>

#include <intsafe.h>

//------------- Arbitrary constants -----------------------

// the assumption is that this buffer size covers the vast majority of requests
#define MINBUFSIZE        1024
// maximum size of output headers
#define MAXHEADERS        512
// maximum size of a mime-type
#define MAXMIME            64
// maximum size username+password
#define MAXUSERPASS        256
// the assumption is that this buffer size covers most dir listings
#define DIRBUFSIZE        4096
// Size of response headers ("normal" headers, cookies and other extra headers are dynamic)
#define HEADERBUFSIZE   4096
// Used for dynamically growing arrays
#define VALUE_GROW_SIZE   5        
// Size of buffer to hold all the bodies on web server errors
#define BODYSTRINGSIZE     4096
// Default log size
#define DEFAULTLOGSIZE     32768
// Amount of time timer thread should sleep between firing.
#define PERIODIC_TIMER_SLEEP_TIMEOUT 60000
// Smallest amount of POST data we'll ever read, puts lower bound on PostReadSize registry value.
#define MIN_POST_READ_SIZE   8192

#define HTTPD_DEV_PREFIX   L"HTP"
#define HTTPD_DEV_INDEX   0
#define HTTPD_DEV_NAME     L"HTP0:"

#define LOG_REMOTE_ADDR_SIZE 50


//------------- not-so-arbitrary constants -----------------------

#define IPPORT_HTTP        80
#define IPPORT_SSL      443


//-------------------- Debug defines ------------------------

// Debug zones
#ifdef DEBUG
  #define ZONE_ERROR    DEBUGZONE(0)
  #define ZONE_INIT        DEBUGZONE(1)
  #define ZONE_LISTEN    DEBUGZONE(2)
  #define ZONE_SOCKET    DEBUGZONE(3)
  #define ZONE_REQUEST    DEBUGZONE(4)
  #define ZONE_RESPONSE    DEBUGZONE(5)
  #define ZONE_ISAPI    DEBUGZONE(6)
  #define ZONE_VROOTS    DEBUGZONE(7)
  #define ZONE_ASP      DEBUGZONE(8)
  #define ZONE_DEVICE   DEBUGZONE(9)
  #define ZONE_SSL      DEBUGZONE(10)
  #define ZONE_VERBOSE  DEBUGZONE(11)
  #define ZONE_AUTH     DEBUGZONE(12)  
  #define ZONE_WEBDAV    DEBUGZONE(13)
  #define ZONE_PARSER    DEBUGZONE(14)
  #define ZONE_TOKEN    DEBUGZONE(15)

  #define ZONE_REQUEST_VERBOSE  (ZONE_VERBOSE && ZONE_REQUEST)
  #define ZONE_SSL_VERBOSE      (ZONE_VERBOSE && ZONE_SSL)
  #define ZONE_WEBDAV_VERBOSE   (ZONE_VERBOSE && ZONE_WEBDAV)
#endif

#define NTLM_PACKAGE_NAME       TEXT("NTLM")
#define NEGOTIATE_PACKAGE_NAME  TEXT("Negotiate")

// We need CE_STRING because GetProcAddress takes a LPCSTR as arg on NT, but UNICODE is defined
// so the TEXT macro would return a UNICODE string
#ifdef  UNDER_CE
#define SECURITY_DLL_NAME TEXT("\\windows\\secur32.dll")
#define CE_STRING(x)      TEXT(x)
#define SECURITY_ENTRYPOINT_CE  SECURITY_ENTRYPOINT
#else
#define SECURITY_DLL_NAME TEXT("security.dll")
#define CE_STRING(x)      (LPCSTR) (x)
#define SECURITY_ENTRYPOINT_CE  SECURITY_ENTRYPOINT_ANSIA
#endif

#define ASP_DLL_NAME      L"\\windows\\asp.dll"

/////////////////////////////////////////////////////////////////////////////
// Misc string handling helpers
/////////////////////////////////////////////////////////////////////////////

PSTR MySzDupA(PCSTR pszIn, int iLen=0);
PWSTR MySzDupW(PCWSTR wszIn, int iLen=0);
PWSTR MySzDupAtoW(PCSTR pszIn, int iInLen=-1);
PSTR MySzDupWtoA(PCWSTR wszIn, int iInLen=-1);
BOOL MyStrCatA(PSTR *ppszDest, PSTR pszSource, PSTR pszDivider=NULL);

// Misc HTTP helper macros
#define CHECKSIG(h)    (((CHttpRequest*)h)->m_dwSig == CHTTPREQUEST_SIG)
#define CHECKHCONN(h)                                             \
        if(!h || ((CHttpRequest*)h)->m_dwSig != CHTTPREQUEST_SIG) \
        {                                                         \
            SetLastError(ERROR_INVALID_PARAMETER);                \
            return FALSE;                                         \
        }
#define CHECKPFC(h)                                                               \
        if (!h || ((CHttpRequest*)h->ServerContext)->m_dwSig != CHTTPREQUEST_SIG) \
        {                                                                         \
            SetLastError(ERROR_INVALID_PARAMETER);                                \
            return FALSE;                                                         \
        }
#define CHECKPTR(p)                                  \
        if (!p)                                      \
        {                                            \
            SetLastError(ERROR_INVALID_PARAMETER);   \
            return FALSE;                            \
        }
#define CHECKPTRS2(p1, p2)                           \
        if(!p1 || !p2)                               \
        {                                            \
            SetLastError(ERROR_INVALID_PARAMETER);   \
            return FALSE;                            \
        }
#define CHECKPTRS3(p1, p2, p3)                       \
        if(!p1 || !p2 || !p3)                        \
        {                                            \
            SetLastError(ERROR_INVALID_PARAMETER);   \
            return FALSE;                            \
        }

#define CHECKFILTER(pfc)                                                     \
        {                                                                    \
            if (! ((CHttpRequest*)pfc->ServerContext)->m_pFInfo->m_fFAccept) \
            {                                                                \
                SetLastError(ERROR_OPERATION_ABORTED);                       \
                return FALSE;                                                \
            }                                                                \
        }

inline BOOL IsDirectory(DWORD dwFileAttributes) 
{
    return (dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

inline BOOL IsDirectory(WIN32_FIND_DATAW *pFileData) 
{
    return IsDirectory(pFileData->dwFileAttributes);
}

inline BOOL IsDirectory(WIN32_FILE_ATTRIBUTE_DATA *pFileData) 
{
    return IsDirectory(pFileData->dwFileAttributes);
}

//------------- Channel Binding Data typedefs ----------------

//
// Channel binding token verification flags
//

#define HTTP_CHANNEL_BIND_PROXY  0x1
#define HTTP_CHANNEL_BIND_PROXY_COHOSTING 0x20

//
// Service bind verification flags
//

#define HTTP_CHANNEL_BIND_NO_SERVICE_NAME_CHECK 0x2
#define HTTP_CHANNEL_BIND_DOTLESS_SERVICE 0x4

//
// Flags triggering channel bind parameters retrieval
//

#define HTTP_CHANNEL_BIND_SECURE_CHANNEL_TOKEN 0x8
#define HTTP_CHANNEL_BIND_CLIENT_SERVICE 0x10

typedef enum _HTTP_AUTHENTICATION_HARDENING_LEVELS
{
    HttpAuthenticationHardeningLegacy = 0,
    HttpAuthenticationHardenningMedium,
    HttpAuthenticationHardeningStrict,
    HttpAuthenticationHardeningMax
} HTTP_AUTHENTICATION_HARDENING_LEVELS;

typedef enum _HTTP_SERVICE_BINDING_TYPE {
    
    HttpServiceBindingTypeNone = 0,
    HttpServiceBindingTypeW,
    HttpServiceBindingTypeA
    
} HTTP_SERVICE_BINDING_TYPE;

typedef struct _HTTP_SERVICE_BINDING_BASE {
    
    HTTP_SERVICE_BINDING_TYPE Type;
    
} HTTP_SERVICE_BINDING_BASE, *PHTTP_SERVICE_BINDING_BASE;

typedef struct _HTTP_SERVICE_BINDING_A {

    HTTP_SERVICE_BINDING_BASE Base;
    PCHAR Buffer;
    ULONG BufferSize;
    
} HTTP_SERVICE_BINDING_A, *PHTTP_SERVICE_BINDING_A;


typedef struct _HTTP_SERVICE_BINDING_W
{
    HTTP_SERVICE_BINDING_BASE Base;
    PWCHAR Buffer;
    ULONG  BufferSize;      // in BYTE
    
} HTTP_SERVICE_BINDING_W, *PHTTP_SERVICE_BINDING_W;

typedef struct _HTTP_CHANNEL_BIND_INFO
{
    HTTP_AUTHENTICATION_HARDENING_LEVELS Hardening;
    ULONG Flags;
    
    HTTP_SERVICE_BINDING_W  *pServiceNames;
    ULONG    NumberOfServiceNames;  // validated SPN maybe less than it
    
} HTTP_CHANNEL_BIND_INFO, *PHTTP_CHANNEL_BIND_INFO;


//------------- Scalar Data typedefs -----------------------
// HTTP status codes
typedef enum {
    STATUS_OK = 0,                     // 200
    STATUS_MOVED,                      // 302
    STATUS_NOTMODIFIED,                // 304
    STATUS_BADREQ,                     // 400
    STATUS_UNAUTHORIZED,               // 401
    STATUS_FORBIDDEN,                  // 403
    STATUS_NOTFOUND,                   // 404
    STATUS_INTERNALERR,                // 500
    STATUS_NOTIMPLEM,                  // 501
    STATUS_CREATED,                    // 201
    STATUS_NOCONTENT,                  // 204
    STATUS_MULTISTATUS,                // 207
    STATUS_METHODNOTALLOWED,           // 405
    STATUS_CONFLICT,                   // 409
    STATUS_LENGTH_REQUIRED,            // 411
    STATUS_PRECONDITION_FAILED,        // 412
    STATUS_REQUEST_TOO_LARGE,          // 413
    STATUS_UNSUPPORTED_MEDIA_TYPE,     // 415
    STATUS_LOCKED,                     // 423
    STATUS_INSUFFICIENT_STORAGE,       // 507
    STATUS_MAX,
}
RESPONSESTATUS;

// Data used for response static data
typedef struct 
{
    DWORD dwStatusNumber;
    PCSTR pszStatusText;
    PCSTR pszStatusBody;
    BOOL  fHasDefaultBody;
}
STATUSINFO;

#define IS_STATUS_2XX(status)      ((rgStatus[(status)].dwStatusNumber >= 200) && (rgStatus[(status)].dwStatusNumber < 300))

#define    SERVICE_CALLER_PROCESS_HTTPDEXE_EXE         101
#define MAX_SOCKET_LIST                             64

//------------- Const data prototypes -----------------------


extern STATUSINFO rgStatus[STATUS_MAX];
extern const char cszTextHtml[];
extern const char cszOctetStream[];
extern const char cszEmpty[];
extern const char* rgMonth[];
extern const char cszKeepAlive[];
extern const char cszConnClose[];
extern const char cszHTTPVER[];
extern const char cszDateParseFmt[];
extern const char cszDateOutputFmt[];
extern const char* rgWkday[];
extern const char* rgMonth[];
extern const char cszCRLF[];
extern const char cszBasic[];
extern const char cszNTLM[];
extern const char cszHTTPVER[];
extern const CHAR g_szHexDigits[];

extern const DWORD g_fIsapiExtensionModule;
extern const DWORD g_fIsapiFilterModule;
extern const DWORD g_fAuthModule;
extern const DWORD g_fWebDavModule;

extern CRITICAL_SECTION g_CritSect;

//----------------------- Class defns -----------------------
class CGlobalVariables;

#include <asp.h>
#include <buffio.h>
#include <extns.h>
#include <vroots.hpp>
#include <auth.h>
#include <ssl.h>
#include <request.h>
#include <log.h>
#include <filters.h>
#include <httpstr.h>
#include <webdav.h>
#include <davlock.h>
#include <davxml.h>
#include <authhlp.h>


extern CHttpRequest *g_pRequestList;


//-------------------- All global data is accessed through Global Class ----------

class CGlobalVariables : public SVSInterfaceMapper 
{
public:
    CVRoots*  m_pVroots;                // ptr to VRoot structure, containing or URL-->Paths mappings

    DWORD     m_dwSystemChangeNumber;   // Current state of web server.  Increments one each time settings are refreshed.

    // Support for multiple websites
    BOOL      m_fRootSite;              // TRUE if we're default website, FALSE otherwise
    BOOL      m_fUseDefaultSite;        // TRUE if we allow g_pVars to host website if there's no IP address map, FALSE otherwise.
    WCHAR     m_szSiteName[MAX_PATH];   // Name of current website (HKLM\COMM\HTTPD\Websites\<WebsiteName>)

    // Basic web server config variables
    DWORD       m_dwListenPort;         // port we're listening on (can be modified in registry. default=80)
    BOOL      m_fBasicAuth;             // are we allowing Basic auth?
    BOOL      m_fNTLMAuth;              // are we allowing NTLM auth?
    BOOL      m_fNegotiateAuth;         // are we willing to Negotiate auth?
    BOOL      m_fFilters;               // Is ISAPI filter component included?
    BOOL      m_fExtensions;            // Is ISAPI extension component included?
    BOOL      m_fASP;                   // Is ASP component included?
    BOOL      m_fDirBrowse;             // are we allowing directory browsing
    PWSTR     m_wszDefaultPages;        // are we allowing directory browsing
    BOOL      m_fAcceptConnections;     // are we accepting new threads?
    LONG      m_nConnections;           // # of connections (threads) we're handling
    LONG      m_nMaxConnections;        // Maximum # of connections we support concurrently
    CLog*      m_pLog;                  // Logging structure
    DWORD     m_dwPostReadSize;         // Size of chunks of data to recv() in POST request.
    DWORD     m_dwMaxHeaderReadSize;    // Maximum # of bytes of header to read before rejecting request.
    PSTR      m_szServerID;             // Returned to client in "Server:" HTTP response header.
    PSTR      m_szBasicRealm;           // Realm specified in Basic-Realm during Basic Auth.
    DWORD     m_dwConnectionTimeout;    // Number of milliseconds inactive connections are kept open before being closed.
    BOOL      m_fDisableNagling;        // disable nagling for all httpd sockets?

    // WebDAV
    BOOL      m_fWebDav;                // Are WebDAV extensions supported?
    DWORD     m_dwDavDefaultLockTimeout;// Default number of milliseconds until DAV locks expire.

    CISAPICache *m_pISAPICache;         // Used to cache ISAPI extension and ASP dlls
    DWORD     m_dwCacheSleep;           // How often (in millesecs) do we 

    PWSTR      m_wszAdminUsers;         // List of users who have administrative privelages

    PSTR      m_pszStatusBodyBuf;       // Holds the strings of http bodies loaded from rc file

    SVSThreadPool *m_pThreadPool;       // All httpd threads other than HttpConnectionThread use this
    BOOL      m_fReadFilters;           // TRUE only if there's a registered read filter.

    SCRIPT_MAP m_scriptMap;             // Map file extensions to ISAPI / ASP scripts.
    SCRIPT_NOUNLOAD m_scriptNoUnload;   // List of ISAPI extensions never to unload.

    CHAR      m_szMaxConnectionMsg[256];// Message to send when max # of connections have been established.

    // ASP Specific
    SCRIPT_LANG m_ASPScriptLang;        // Default scripting language
    LCID        m_ASPlcid;              // Default LCID
    UINT        m_lASPCodePage;         // Default Code Page
    DWORD       m_fASPVerboseErrorMessages; // Whether to display descriptive messages on script error or not.
    DWORD       m_dwAspMaxFormSize;     // Max size of HTTP body ASP will read as a result of Request.Form access

    // Authentication Specific
    void                    InitAuthentication(CReg *pReg);
    BOOL                    InitSecurityLib(void);
    BOOL                    InitChannelNameList(void);
    BOOL                    InitChannelFlagsCheck(PWCHAR spnString);
    HINSTANCE               m_hSecurityLib; // Global Security library handle
    DWORD                   m_cbMaxToken;   // max ntlm allowable data size
    SecurityFunctionTable     m_SecurityInterface; // fcn table for Security requests

    // Channel Binding Info
    PWSTR                   m_SpnNameCheckList; // server/ipv4 name list for spn check, the format of this list aka "name0$name1$name2\0"
    HTTP_CHANNEL_BIND_INFO  m_ChannelBindingInfo;                   
    
    // SSL Specific
    void InitSSL(CReg *pWebsite);
    void FreeSSLResources(void);
    BOOL         m_fSSL;                // Is SSL enabled?
    BOOL         m_fSSLSkipProcessing;  // Bind to SSL Port but don't handle requests, letting user custom ISAPI Filter do the work.
    DWORD        m_dwSSLPort;           // Port to bind to for SSL for.
    PSTR         m_pszSSLIssuer;        // certificate's issuer
    PSTR         m_pszSSLSubject;       // certificate's subject
    CSSLUsers    m_SSLUsers;            // User <-> certificate info mapping table.
    CredHandle   m_hSSLCreds;           // global credentials info
    BOOL         m_fHasSSLCreds;        // Do we need to free credentials on shutdown?
    HCERTSTORE   m_hSSLCertStore;       // certificate store
    FixedMemDescr *m_SSLUserMemDescr;   // Fixed mem descriptor to hold SSL user info.
    DWORD        m_dwSSLCertTrustOverride;  // what errors to ignore from call to CertGetCertificateChain() on client certs.

    void InitGlobals(CReg *pWebsite, const WCHAR *szSiteName);
    void DeInitGlobals(void);
    
    CGlobalVariables(CReg *pWebsite, const WCHAR *szSiteName) 
    { 
        InitGlobals(pWebsite,szSiteName); 
    }
    ~CGlobalVariables()              
    { 
        DeInitGlobals(); 
    }

    void DeInitExtensions(void);
    BOOL InitExtensions(CReg *pWebsite);
    BOOL InitASP(CReg *pWebsite);
    BOOL RefreshVrootTable(void);

    // deal with connection level socket
    BOOL CreateHTTPSocket(BOOL fSecure);
    BOOL IsSecureSocket(PSOCKADDR pSockAddr, DWORD cbSockAddr);

    // WebDAV support
    BOOL InitWebDav(CReg *pWebsite);
    void DeInitWebDav(void);

    friend CGlobalVariables* MapWebsite(SOCKET sock, PSTR szHostHeader, CVRoots **ppVRootTable);
};

extern CGlobalVariables *g_pVars;
extern CGlobalVariables *g_pWebsites;
extern DWORD            g_cWebsites;

void CleanupGlobalMemory(DWORD dwWebsites);
CGlobalVariables* MapWebsite(SOCKET sock, PSTR szHostHeader, CVRoots **ppVRootTable);

// Lists which ports are associated with SSL.  All ports are in network byte order (big-endian).
class CSecurePortList 
{
private:
    SVSLinkManager *m_pSecurePorts;   // Lists which ports are to be secure

public:
    CSecurePortList() 
    {
        m_pSecurePorts = NULL;
    }

    ~CSecurePortList() 
    {
        if (m_pSecurePorts)
        {
            delete m_pSecurePorts;
        }
    }

    BOOL InitSecureList(void);
    BOOL AddPortToSecureList(DWORD dwPort);
    BOOL RemovePortFromSecureList(DWORD dwPort);
    BOOL IsPortInSecureList(DWORD dwPort);
    void ResetSecureList(void);
    void ProcessSocketDeregister(SOCKADDR_STORAGE *pSockAddr);
};

extern CSecurePortList *g_pSecurePortList;

extern HINSTANCE g_hInst;
extern HANDLE    g_hAdminThread;
extern DWORD      g_fState;
extern CWebDavFileLockManager *g_pFileLockManager;
extern SVSThreadPool *g_pTimer;
extern const char *rg_cszMethods[];

//------------- Function prototypes -------------------------
DWORD WINAPI HttpConnectionThread(LPVOID lpv);
BOOL IsPathScript(WCHAR *szPath, SCRIPT_TYPE scriptType=SCRIPT_TYPE_NONE);


BOOL GetRemoteAddress(SOCKET sock, PSTR pszBuf, BOOL fTryHostName, DWORD cbBuf);
DWORD WINAPI PeriodicWakeupThread(LPVOID lpv);
char *strcpyEx(char *szDest, const char *szSrc);

// Writes date into buffer, returns next point to write out from.
inline int WriteDateGMT(PSTR szBuf,const SYSTEMTIME *pst, BOOL fCRLF=FALSE) 
{
#pragma warning(push)
#pragma warning(disable:4996)
    int iLen = sprintf(szBuf, cszDateFmtGMT, rgWkday[pst->wDayOfWeek], pst->wDay, rgMonth[pst->wMonth], pst->wYear, pst->wHour, pst->wMinute, pst->wSecond);
    if (fCRLF) 
    {
        strcpy(szBuf+iLen,cszCRLF);
        iLen += 2;
    }
    return iLen;
#pragma warning(pop)
}

inline int WriteDateGMT(PSTR szBuf, const FILETIME *pft, BOOL fCRLF=FALSE) 
{
    SYSTEMTIME st;
    FileTimeToSystemTime(pft,&st);
    return WriteDateGMT(szBuf,&st,fCRLF);
}

void InitializeResponseCodes(PSTR pszStatusBodyBuf);     

BOOL HttpInitializeOnce(void);
BOOL CreateHTTPConnection(SOCKET sock, PSOCKADDR pSockAddr, DWORD cbSockAddr);
DWORD WINAPI InitializeGlobalsThread(LPVOID lpv);
BOOL InitializeGlobals();
void WaitForConnectionThreadsToEnd(void);
BOOL IsHttpdEnabled(void);
PSTR MassageMultiStringToAnsi(PCWSTR wszIn, PCWSTR wszDefault);
DWORD GetETagFromFiletime(FILETIME *pft, DWORD dwFileSize, PSTR szWriteBuffer);
int PathNameCompare(WCHAR *szSrc, WCHAR *szDest);
int PathNameCompareN(WCHAR *src, WCHAR *dst, int len);

inline int IsNonCRLFSpace(CHAR c) 
{
    return ( c == ' ' || c == '\t' || c == '\f' || c == '\v');
}

inline RESPONSESTATUS GLEtoStatus(void) 
{
    return GLEtoStatus(GetLastError());
}

inline USHORT GetSocketPort(PSOCKADDR pSockAddr) 
{
    PREFAST_SUPPRESS(24002,"Since port is in same place for both v4 + v6 sockets, this typecast is OK")
    SOCKADDR_IN *pSockIn = (SOCKADDR_IN *)pSockAddr; 
    return pSockIn->sin_port;
}

#define HEX_ESCAPE    '%'

// to milliseconds (10 * 1000)
#define FILETIME_TO_MILLISECONDS  ((__int64)10000L) 

#define SkipWhiteSpaceNonCRLF(lpsz)     while ( (lpsz)[0] != '\0' && IsNonCRLFSpace((lpsz)[0]))   ++(lpsz)


// This routine waits for the specified global thread handle to be set before
// it allows the given thread to execute
inline void MyWaitForReadyState(HANDLE &hThread) 
{
#if defined (DEBUG)
    int iAttempts = 0;
#endif

    while (!hThread) 
    {
        Sleep(10);

#if defined (DEBUG)
        if (iAttempts == 5) // we shouldn't have to wait this long for thread to be ready - something is probably wrong.
        {
            DEBUGCHK(0);
        }
#endif
    }
}

inline void MyWaitForAdminThreadReadyState(void) 
{ 
    MyWaitForReadyState(g_hAdminThread); 
}

void SetWebServerState(DWORD dwNewState);
BOOL SetWebServerState(DWORD dwNewState, DWORD dwRequiredOldState);

BOOL RefreshVRootTable(void);

#endif //_HTTPD_H_

