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
Module Name: REQUEST.H
Abstract: HTTP request class
--*/


// HTTP version numbers
#define HTTP_VER_UNSET   MAKELONG(0,0)   // invalid
#define HTTP_VER_10      MAKELONG(0,1)   // HTTP/1.0
#define HTTP_VER_11      MAKELONG(1,1)   // HTTP/1.1

// Scalar types used by CHTTPRequest
typedef enum 
{
    TOK_DATE,
    TOK_PRAGMA,
    TOK_COOKIE,
    TOK_ACCEPT,
    TOK_REFERER,
    TOK_UAGENT,
    TOK_AUTH,
    TOK_IFMOD,
    TOK_TYPE,
    TOK_LENGTH,
    TOK_ENCODING,
    TOK_CONNECTION,
    TOK_HOST,
}
TOKEN;

// NOTE: This follows same order as "const char *rg_cszMethods" list in parser.cpp
//       and also davTable in webdav.cpp
typedef enum 
{
    VERB_UNKNOWN,
    VERB_GET,
    VERB_HEAD,
    VERB_POST,
    VERB_OPTIONS,
    VERB_PUT,
    VERB_MOVE,
    VERB_COPY,
    VERB_DELETE,
    VERB_MKCOL,
    VERB_PROPFIND,
    VERB_PROPPATCH,
//    VERB_SEARCH,
    VERB_LOCK,
    VERB_UNLOCK,
#if 1 
    VERB_LAST_VALID = VERB_UNLOCK,
#else // unsupported DAV vebs
    VERB_SUBSCRIBE,
    VERB_UNSUBSCRIBE,
    VERB_POLL,
    VERB_BDELETE,
    VERB_BCOPY,
    VERB_BMOVE,
    VERB_BPROPPATCH,
    VERB_BPROPFIND,
    VERB_X_MS_ENUMATTS,
    VERB_LAST_VALID = VERB_X_MS_ENUMATTS,
#endif
}
VERB;

inline BOOL IsBasic(AUTHTYPE a)          
{ 
    return (a == AUTHTYPE_BASIC); 
}
inline BOOL IsNTLM(AUTHTYPE a)           
{ 
    return (a == AUTHTYPE_NTLM); 
}
inline BOOL IsNegotiate(AUTHTYPE a)      
{ 
    return (a == AUTHTYPE_NEGOTIATE); 
}
inline BOOL IsSupportedAuth(AUTHTYPE a)  
{ 
    return (IsNTLM(a) || IsBasic(a) || IsNegotiate(a)); 
}

typedef enum 
{
    CONN_NOPREF       = 0,   // no preference 
    CONN_WANTCLOSE    = 1,   // preference a closure
    CONN_WANTKEEP     = 2,   // preference a keep-alive
    CONN_FORCECLOSE   = 3    // force a closure
}
CONNHEADER;

#define CHTTPREQUEST_SIG 0xAB0D

// If strlen(url) is already known, don't recalc it but pass it in.  If unknown set = -1.
inline BOOL URLHasTrailingSlashA(PCSTR pszURL, int iURLLen=-1) 
{
    if (iURLLen == -1)
    {
        iURLLen = strlen(pszURL);
    }

    DEBUGCHK((iURLLen == (int)strlen(pszURL)) && (iURLLen>=1));
    return (pszURL[iURLLen-1]=='/' || pszURL[iURLLen-1]=='\\');
}

inline BOOL URLHasTrailingSlashW(WCHAR *wszURL, int iURLLen=-1) 
{
    if (iURLLen == -1)
    {
        iURLLen = wcslen(wszURL);
    }

    DEBUGCHK((iURLLen == (int)wcslen(wszURL)) && (iURLLen>=1));
    return (wszURL[iURLLen-1]=='/' || wszURL[iURLLen-1]=='\\');
}

inline void RemoveTrailingSlashIfNeededA(PSTR pszURL, int iURLLen=-1) 
{
    if (iURLLen == -1)
    {
        iURLLen = strlen(pszURL);
    }

    if (URLHasTrailingSlashA(pszURL,iURLLen))
    {
        pszURL[iURLLen-1] = 0;
    }
}

RESPONSESTATUS GLEtoStatus(int iGLE);


// This object is the top-level object for an incoming HTTP request. One such object
// is created per request & a thread is created to handle it.

class CFilterInfo;
class CWebDav;
class CHttpResponse;
class CChunkParser;

class CHttpRequest 
{
    // socket
    DWORD    m_dwSig;
    SOCKET   m_socket;
    CHttpRequest *m_pNext;
    
    // buffers
    CBuffer  m_bufRequest;
    CBuffer  m_bufRespBody;
    
    // method, version, URL etc. Direct results of parse
    PSTR     m_pszMethod;
    PSTR     m_pszURL;
    PSTR     m_pszContentType;
    DWORD    m_dwContentLength;
    PSTR     m_pszAccept;
    FILETIME m_ftIfModifiedSince;
    DWORD    m_dwIfModifiedLength;
    PSTR     m_pszCookie;
    PSTR     m_pszHost;

    // Decoded URL (indirect results of parse)
    PSTR     m_pszQueryString;
    PWSTR    m_wszPath;
    PWSTR    m_wszExt;
    PSTR     m_pszPathInfo;
    PSTR     m_pszPathTranslated;
    DWORD    m_dwFileAttributes;  // results of GetFileAttributes(m_wszPath) call.

    DWORD    m_dwInitialPostRead;  // how much we read off wire before calling ISAPI extension
    DWORD    m_ccWPath; // # of wide chars of m_wszPath.

    BOOL     m_fIsChunkedUpload;        // Is request using transfer-encoding: chunked on upload?
    DWORD    m_dwBytesRemainingInChunk; // Number of bytes remaining in this chunked upload, -1==EOF.

    CVRoots *m_pVrootTable; // Virtual root table using during this request, must be Released() at end of each request.
    PVROOTINFO m_pVRoot; // do NOT free, points to global data that is alloced/freed on server init/deinit.

    // Logging members
    PSTR     m_pszLogParam;
    RESPONSESTATUS m_rs;

    BOOL     m_fBufferedResponse;    // Are we using m_bufResponse or sending straight to client?
    HRINPUT  m_hrPreviousReadClient;    // Should next call to ReadClient() timeout?

    // Parsing functions
    void FreeHeaders(void) 
    {
        MyFree(m_pszMethod);
        MyFree(m_pszURL);
        MyFree(m_pszContentType);
        MyFree(m_pszAccept);
        MyFree(m_pszQueryString);
        MyFree(m_wszPath);
        MyFree(m_wszExt);
        MyFree(m_pszCookie);
        MyFree(m_pszHost);
        MyFree(m_pszLogParam);    
        MyFree(m_pszPathInfo);
        MyFree(m_pszPathTranslated);
        MyFree(m_pszAuthType);
    }
    BOOL ParseHeaders();
    BOOL MyCrackURL(PSTR pszRawURL, int iLen);

    BOOL CheckAuth(AUTHLEVEL auth) 
    {
        return (m_AuthLevelGranted >= auth) ? TRUE : FALSE;
    }

    // These variables influence the keeping/closing of the socket
    BOOL    m_fForceClose;          // Forced closure due to error or redirect
    DWORD   m_dwVersion;            // LOWORD=minor, HIWORD=major.  
    BOOL    m_fStrictHTTP10;        // Force strict 1.0 compliance
    BOOL    m_fConnClose;           // request said 'Connection: close'
    BOOL    m_fConnKeep;            // request said 'Connection: keep-alive'
    CONNHEADER m_connHdr;           // caller specification

    // These variables indicate the current decision to keep/close the socket
    BOOL    m_fPersistSocket;       // what to do with the socket
    BOOL    m_fPersistSocketLocked; // Changes are no longer permitted

public:
    BOOL ParseMethod(PCSTR pszMethod, int cbMethod);
    BOOL ParseContentLength(PCSTR pszMethod, TOKEN id);
    BOOL ParseContentType(PCSTR pszMethod, TOKEN id);
    BOOL ParseIfModifiedSince(PCSTR pszMethod, TOKEN id);
    BOOL ParseAuthorization(PCSTR pszMethod, TOKEN id);
    BOOL ParseAccept(PCSTR pszMethod, TOKEN id);
    BOOL ParseConnection(PCSTR pszMethod, TOKEN id);
    BOOL ParseCookie(PCSTR pszMethod, TOKEN id);
    BOOL ParseHost(PCSTR pszMethod, TOKEN id);
    BOOL HandleNTLMNegotiateAuth(PSTR pszSecurityToken, BOOL fUseNTLM);
    PSTR    m_pszSecurityOutBuf;  // buffer to send client on NTLM/negotiate response, Base64 encoded
    VERB    m_idMethod;
    CBuffer m_bufRespHeaders;
    CFilterInfo *m_pFInfo;        // Filter state information

    inline BOOL GetConnectionPersist(void)
    {
        return m_fPersistSocket;
    }
    inline void LockConnectionPersist(void)
    {
        m_fPersistSocketLocked = TRUE;
    }
    inline void SetConnectionPersist(CONNHEADER connHdr)
    {
        // Set caller specification
        m_connHdr = connHdr;

        // Check for Forced closure
        if (m_connHdr == CONN_FORCECLOSE)
        {
            m_fForceClose = TRUE;
        }

       // Check for 'Connection: close'
        if (m_connHdr == CONN_WANTCLOSE)
        {
           m_fConnClose = TRUE;
        }
        // update the persistance variable
        CalcConnectionPersist();
    }
    void CalcConnectionPersist(void)
    {
        // Decide if the socket should be closed, or not.
        // Inputs, in order of precedence, are:
        //    Caller request with CONN_FORCECLOSE (forces closure)
        //    Failure to get HTTP version number (forces closure)
        //    LockDown (no more changes allowed)
        //    Request HTTP version (1.0 means close and 1.1 means keep)
        //    StrictHTTP10 which prohibits 'connection: keep-alive'
        //    Request HTTP headers (saying 'Connection: keep-alive' or 'Connection: close')
        //    Caller request (saying CONN_WANTKEEP or CONN_WANTCLOSE)

        // Forced closure takes precedence
        if (m_fForceClose)
        {
            m_fPersistSocket = FALSE;
            return;
        }
        // application want to close the connection
        if (m_fConnClose)
        {
            m_fPersistSocket = FALSE;
            return;
        }
        // Lack of HTTP version number is fatal
        if (m_dwVersion == HTTP_VER_UNSET)
        {
            m_fPersistSocket = FALSE;
            return;
        }

        // changes can not be made after lock down
        if (m_fPersistSocketLocked)
        {
            return;
        }

        // process HTTP/1.1 requests
        if (m_dwVersion >= HTTP_VER_11)
        {
            // Look for close command in the request
            if (m_fConnClose)
            {
                m_fPersistSocket = FALSE;
            }

            // or told to close by the application
            else if (m_connHdr == CONN_WANTCLOSE)
            {
                m_fPersistSocket = FALSE;
            }

            // default is to keep the socket
            else
            {
                m_fPersistSocket = TRUE;
            }
        }

        // process HTTP/1.0 requests 
        else
        {
            // In Strict HTTP/1.0, keep-alive is forbidden
            if (m_fStrictHTTP10)
            {
                m_fPersistSocket = FALSE;
            }

            // Look for keep-alive in the request
            else if (m_fConnKeep)
            {
                m_fPersistSocket = TRUE;
            }

            // or told to keep-alive by the application
            else if (m_connHdr == CONN_WANTKEEP)
            {
                m_fPersistSocket = TRUE;
            }

            // default is to close the socket
            else
            {
                m_fPersistSocket = FALSE;
            }
        }
    }

    inline void    SetHTTPVersion(int minor, int major)
    {
        m_dwVersion = MAKELONG(minor, major);
        CalcConnectionPersist();
    }
    inline DWORD   MajorHTTPVersion(void) 
    { 
        return HIWORD(m_dwVersion);
    }
    inline DWORD   MinorHTTPVersion(void) 
    { 
        return LOWORD(m_dwVersion);
    }
    inline DWORD   MinorHTTPVersionMax(void) 
    { 
        return 1;    // Maximum HTTP is 1.1
    }
    inline void    SetConnClose(void)
    {
        m_fConnClose = TRUE;
        CalcConnectionPersist();
    }
    inline void    SetConnKeepAlive(void)
    {
        m_fConnKeep = TRUE;
        CalcConnectionPersist();
    }
    inline void    SetStrictHTTP10(void)
    {
        m_fStrictHTTP10 = TRUE;
        CalcConnectionPersist();
    }
    inline BOOL    IsStrictHTTP10()
    {
        return m_fStrictHTTP10;
    }


    RESPONSESTATUS GetStatus(void) 
    { 
        return m_rs; 
    }

#if defined (DEBUG)
    // When HTTP header buf has a \0 temporarily put into it to create a string, no further 
    // calls to FindHttpHeader can be made, as header we're looking for maybe after '\0'.
    BOOL             m_fHeadersInvalid;     
    void             DebugSetHeadersInvalid(BOOL f) 
    { 
        m_fHeadersInvalid = f; 
    }
#else
    void             DebugSetHeadersInvalid(BOOL f) 
    { 
        ; 
    }
#endif

    PSTR FindHttpHeader(PCSTR szHeaderName, DWORD ccHeaderName);
    BOOL TranslateRequestToScript(void);

    // which set of config variables (vroots, auth, etc...) to use, depending on website request services.
    // Note that certain functionality (the thread pool, ISAPI extension dll cache, 
    // parts of auth like function table interfaces into security DLLs, and ISAPI filters)
    // are handled globally and will access g_pVars (default website) directly.
    CGlobalVariables *m_pWebsite;
    BOOL m_fResolvedHostWebSite;

    //  When using SSL client certificates, we don't use standard BASIC/NTLM/... 
    //  authentication at the same time.
    BOOL SendAuthenticate(void)  
    { 
        return !(m_SSLInfo.m_pClientCertContext); 
    }

    void CloseSocket(void) 
    {
        if (InterlockedExchange(&m_lSocketIsValid, FALSE))
        {
            static char recvBuf[1024];  // buffer shared by all socket closers

            // We are done sending
            shutdown(m_socket, SD_SEND);

            // We are waiting for data to be delivered from the peer.tcp to the peer.app.
            // We can only know that this has happened when the peer.app does a shutdown and peer.tcp sends FIN.
            // Unfortunately, we cannot afford to tie up this thread for minutes on end.
            // So we will give the peer a short time to read the data and then close the socket.
            // Notifies are at least 200ms apart. We want this thread freed before then.   Thus 180ms.
            int rcvto = 180;    // 180ms
            setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&rcvto, sizeof(rcvto));
            while (recv(m_socket, recvBuf, sizeof(recvBuf), 0) > 0)
            {
                ;
            }

            // Now kill the socket
            closesocket(m_socket);
        }
    }

    // In some scenarios it's easier to manipulate m_wszPath if it can 
    // be assumed there's a '\' as trailing character.
    void AddTrailingSlashToPhysicalPath(void) 
    {
        if ((m_ccWPath > 1) && (! URLHasTrailingSlashW(m_wszPath,m_ccWPath))) 
        {
            m_wszPath[m_ccWPath++]  = '\\';
            m_wszPath[m_ccWPath]    = 0;
        }
        DEBUGCHK(m_ccWPath == wcslen(m_wszPath));
    }

    void RemoveTrailingSlashToPhysicalPath(void) 
    {
        if ((m_ccWPath > 1) && URLHasTrailingSlashW(m_wszPath,m_ccWPath)) 
        {
            m_wszPath[--m_ccWPath]  = 0;
        }
        DEBUGCHK((m_wszPath==NULL) || m_ccWPath == wcslen(m_wszPath));
    }

    BOOL IsRootPath(void) 
    {
        return ((m_wszPath[0] == '\\' || m_wszPath[0] == '/') && m_wszPath[1] == 0);
    }

    BOOL IsRootVDir(void) 
    {
        return (IsSlash(m_pszURL[0]) && m_pszURL[1] == 0);
    }

    // Only case we'd be inactive is during shutdown.
    BOOL IsActive(void)  
    { 
        return (m_socket != INVALID_SOCKET); 
    }

    void GetContentTypeOfRequestURI(PSTR szContentType, DWORD ccContentType, const WCHAR *wszExt=NULL);

    static void DeleteAllRequestObjects(void);
    
private:
    friend class CHttpResponse;
    friend class CChunkParser;

    // Authentication data members
    AUTHLEVEL m_AuthLevelGranted;
    PSTR    m_pszAuthType;
    PSTR    m_pszRawRemoteUser;        // Holds base64 encoded data, before auth decodes it
    PSTR    m_pszRemoteUser;
    WCHAR*  m_wszRemoteUser;
    WCHAR*  m_wszMemberOf;
    PSTR    m_pszPassword;
    AUTH_STATE m_AuthState;          // state info for NTLM process, needs to be saved across requests
    DWORD   m_dwAuthFlags;

    BOOL    m_fDisabledNagle;
    LONG    m_lSocketIsValid;

    // Used for sensitive strings that should be zeroed out, in particular password info.
    void ZeroAndFree(PSTR &pszData) 
    {
        if (pszData) 
        {
            DWORD dw = strlen(pszData);
            memset(pszData,0,dw);
        }
        MyFree(pszData);
    }

    void ZeroAndFree(WCHAR *&wszData) 
    {
        if (wszData) 
        {
            DWORD dw = wcslen(wszData)*sizeof(WCHAR);
            memset(wszData,0,dw);
        }
        MyFree(wszData);
    }

    // Authentication functions
    void FreeAuth(void)  
    {
        MyFree(m_pszSecurityOutBuf);
        ZeroAndFree(m_pszPassword);
        ZeroAndFree(m_pszRawRemoteUser);
        //    Don't free NTLM structs or user name info, they're possibly persisted across multiple requests.
    }

    // User name info is persisted across the session.
    void FreePersistedAuthInfo(void) 
    {
        ZeroAndFree(m_pszRemoteUser);
        ZeroAndFree(m_wszRemoteUser);
        ZeroAndFree(m_wszMemberOf);
        m_AuthLevelGranted = AUTH_PUBLIC;
    }

    BOOL CheckAuth(); 

    // Basic AUTH
    BOOL HandleBasicAuth(PSTR pszData);
    BOOL BasicToNTLM(WCHAR * wszPassword);

    // SSL
    BOOL      m_fHandleSSL;
    SSL_INFO  m_SSLInfo;
    BOOL      m_fIsSecurePort;
    BOOL      m_fPerformedSSLRenegotiateRequest;
    BOOL      m_fPerformedClientInitiatedRenegotiate;

    BOOL GetChannelBindingToken(PSecPkgContext_Bindings pChannelBindingToken);
    BOOL HandleSSLHandShake(BOOL fRenegotiate=FALSE, BYTE *pInitialData=NULL, DWORD cbInitialData=NULL);
    BOOL SSLRenegotiateRequest(void);
    BOOL CheckClientCert(void);

    // File GET/HEAD handling functions
    BOOL IsNotModified(HANDLE hFile, DWORD dwLength);
    void CloseSSLSession();

    // Directory: Default page & browsing functions
    BOOL MapDirToDefaultPage(void);
    BOOL EmitDirListing(void);

    // if it doesn't end in '/' send a redirect
    // returns TRUE if a redirect was sent
    BOOL SendRedirectIfNeeded(void);

    // For simple GET and HEAD we allow direct dir browsing.  Otherwise if a dir is 
    // requested on another verb then we let the DAV extensions do the work on it.
    BOOL VerbSupportsDirBrowsing(void) 
    {
        return (m_idMethod == VERB_GET || m_idMethod == VERB_HEAD);
    }

    void Init(SOCKET sock, BOOL fSecure);

    BOOL ReadPostData(DWORD dwMaxSizeToRead, BOOL fInitialPostRead, BOOL fSSLRenegotiate);
    BOOL CheckContentLengthValid(BOOL fAcceptChunkedEncoding);

    // ISAPI extension handling functions
    BOOL HandleScript();
    BOOL SetPathTranslated();
    BOOL ExecuteISAPI(WCHAR *wszExecutePath);
    void FillECB(LPEXTENSION_CONTROL_BLOCK pECB);
    BOOL GetServerVariable(PSTR pszVar, PVOID pvOutBuf, PDWORD pdwOutSize, BOOL fFromFilter);
    BOOL WriteClient(PVOID pvBuf, PDWORD pdwSize, BOOL fFromFilter);
    BOOL ServerSupportFunction(DWORD dwReq, PVOID pvBuf, PDWORD pdwSize, PDWORD pdwType);
    BOOL ReadClient(PVOID pv, PDWORD pdw);
    BOOL ReadClientNonChunked(PVOID pv, PDWORD pdw);
    BOOL ReadClientChunked(PVOID pv, PDWORD pdw);

    BOOL IsScript(BOOL fCheckAccess);

    //  Filter Specific
    BOOL IsCurrentFilterCalled(DWORD dwNotificationType, int iCurrentFilter);
    BOOL FillFC(PHTTP_FILTER_CONTEXT pfc, DWORD dwNotifyType, 
                          LPVOID *ppStFilter, LPVOID *ppStFilterOrg,
                          PSTR *ppvBuf1, int *pcbBuf, PSTR *ppvBuf2, int *pcbBuf2);
    BOOL CleanupFC(DWORD dwNotifyType, LPVOID* pFilterStruct, LPVOID *pFilterStructOrg, 
                            PSTR *ppvBuf1, int *pcbBuf, PSTR *ppvBuf2);
    
    BOOL AuthenticateFilter();    
    BOOL CallAuthFilterIfNeeded(void);

    
    BOOL FilterMapURL(PSTR pvBuf, WCHAR *wszPath, DWORD *pdwSize, DWORD dwBufNeeded, PSTR pszURLEx=NULL);    
    BOOL MapURLToPath(PSTR pszBuffer, PDWORD pdwSize, LPHSE_URL_MAPEX_INFO pUrlMapEx=NULL);
    void GetVirtualRootsTableFromWebsite(void);
    BOOL HandleAuthentication(void);

    // Filter Callbacks
    BOOL ServerSupportFunction(enum SF_REQ_TYPE sfReq,PVOID pData,DWORD ul1, DWORD ul2);
    BOOL GetHeader(LPSTR lpszName, LPVOID lpvBuffer, LPDWORD lpdwSize);
    BOOL SetHeader(LPSTR lpszName, LPSTR lpszValue);
    BOOL AddResponseHeaders(LPSTR lpszHeaders,DWORD dwReserved);

    // WebDav
    BOOL HandleWebDav(void);
    friend class CWebDav;
    friend class CWebDavFileLockManager;
    BOOL m_fOptionsAsterisk; // Is this an 'OPTIONS *' request we need special handeling for.

    //  ASP Setup Fcns
    BOOL ExecuteASP();
    void FillACB(void *p, HINSTANCE hInst);
    BOOL ReceiveCompleteRequest(PASP_CONTROL_BLOCK pcb);

public:
    CHttpRequest(SOCKET sock, BOOL fSecure) 
    {
        Init(sock,fSecure);
    }

    ~CHttpRequest();
    BOOL ReInit();

    BOOL IsSecure(void)           
    { 
        return m_fHandleSSL; 
    }
    void GetUserAndGroupInfo(PAUTH_STATE pAuth);
    AUTHLEVEL DeterminePermissionGranted(const WCHAR *wszUserList, AUTHLEVEL authDefault);

    // VRoot Accessors
    DWORD       GetPerms(void)          
    { 
        return m_pVRoot ? m_pVRoot->dwPermissions : 0; 
    }
    AUTHLEVEL   GetAuthReq(void)        
    { 
        return m_pVRoot ? m_pVRoot->AuthLevel     : AUTH_PUBLIC;     
    }
    SCRIPT_TYPE GetScriptType(void)     
    { 
        return m_pVRoot ? m_pVRoot->ScriptType    : SCRIPT_TYPE_NONE;    
    }
    BOOL        AllowBasic(void)        
    { 
        return m_pVRoot ? m_pVRoot->fBasic        : 0; 
    }
    BOOL        AllowNTLM(void)         
    { 
        return m_pVRoot ? m_pVRoot->fNTLM         : 0; 
    }
    BOOL        AllowNegotiate(void)    
    { 
        return m_pVRoot ? m_pVRoot->fNegotiate    : 0; 
    }
    BOOL        AllowDirBrowse(void)    
    { 
        return m_pVRoot ? m_pVRoot->fDirBrowse    : 0; 
    }
    WCHAR *     GetUserList(void)       
    { 
        return m_pVRoot ? m_pVRoot->wszUserList   : 0; 
    }

    BOOL URLHasTrailingSlash(int iURLLen=-1)   
    {
        return URLHasTrailingSlashA(m_pszURL,iURLLen);
    }

    void HandleRequest();
    friend DWORD WINAPI HttpConnectionThread(LPVOID lpv);
    friend void CloseAllConnections(void);
    void RemoveFromList();

    void GenerateLog(PSTR szBuffer, PDWORD pdwToWrite);
    DWORD GetLogBufferSize();

    BOOL SendData(PSTR pszBuf, DWORD dwLen, BOOL fCopyBuffer=FALSE);
    BOOL SendEncryptedData(PSTR pszBuf, DWORD dwLen, BOOL fCopyBuffer);

    //  ISAPI Extension / ASP Specific
    friend BOOL WINAPI GetServerVariable(HCONN hConn, PSTR psz, PVOID pv, PDWORD pdw);
    friend BOOL WINAPI ReadClient(HCONN hConn, PVOID pv, PDWORD pdw);
    friend BOOL WINAPI WriteClient(HCONN hConn, PVOID pv, PDWORD pdw, DWORD dw);
    friend BOOL WINAPI ServerSupportFunction(HCONN hConn, DWORD dwReq, PVOID pvBuf, PDWORD pdwSize, PDWORD pdwType);


    //     ASP Specific
    friend BOOL WINAPI Flush(HCONN hConn);
    friend BOOL WINAPI Clear(HCONN hConn);
    friend BOOL WINAPI SetBuffer(HCONN hConn, BOOL fBuffer);

    friend BOOL WINAPI AddHeader (HCONN hConn, LPSTR lpszName, LPSTR lpszValue);
    friend BOOL WINAPI ReceiveCompleteRequest(struct _ASP_CONTROL_BLOCK *pcb);

    // SSL Specific
    SECURITY_STATUS SSLDecrypt(PSTR pszBuf, DWORD dwLen, DWORD *pdwBytesDecrypted, DWORD *pdwOffset, DWORD *pdwExtraRequired, DWORD *pdwIgnore, BOOL *pfCompletedRenegotiate);
    BOOL HandleSSLClientCertCheck(void);

    // Filter Specific
    BOOL CallFilter(DWORD dwNotifyType, PSTR *ppvBuf1 = NULL,int *pcbBuf = NULL, 
                    PSTR *ppvBuf2 = NULL, int *pcbBuf2 = NULL);
    BOOL FilterNoResponse(void);

    // Filters Friends  (exposed to Filter dll)
    friend BOOL WINAPI GetServerVariable(PHTTP_FILTER_CONTEXT pfc, PSTR psz, PVOID pv, PDWORD pdw);
    friend BOOL WINAPI AddResponseHeaders(PHTTP_FILTER_CONTEXT pfc,LPSTR lpszHeaders,DWORD dwReserved);
    friend VOID* WINAPI AllocMem(PHTTP_FILTER_CONTEXT pfc, DWORD cbSize, DWORD dwReserved);
    friend BOOL WINAPI WriteClient(PHTTP_FILTER_CONTEXT pfc, PVOID pv, PDWORD pdw, DWORD dwFlags);
    friend BOOL WINAPI ServerSupportFunction(PHTTP_FILTER_CONTEXT pfc,enum SF_REQ_TYPE sfReq,
                                    PVOID pData, DWORD ul1, DWORD ul2);
    friend BOOL WINAPI SetHeader(PHTTP_FILTER_CONTEXT pfc, LPSTR lpszName, LPSTR lpszValue);
    friend BOOL WINAPI GetHeader(PHTTP_FILTER_CONTEXT pfc, LPSTR lpszName, LPVOID lpvBuffer, LPDWORD lpdwSize);
};


void SendFile(SOCKET sock, HANDLE hFile, CHttpRequest *pRequest);
// Response object. This object doesn't own any of the handles or pointers 
// it uses so it doesnt free anything. The caller is responsible in all cases
// for keeping the handles & memory alive while this object is extant & freeing
// them as approp at a later time
class CHttpResponse 
{
    PCSTR      m_pszType;
    DWORD      m_dwLength;
    PCSTR      m_pszRedirect;
    PCSTR      m_pszExtraHeaders;
    PCSTR      m_pszBody;
    HANDLE     m_hFile;
    char       m_szMime[MAXMIME];
    CHttpRequest *m_pRequest;   // calling request class, for callbacks
    
private:
    void SetTypeFromExtW(PCWSTR wszExt)  
    {
        DEBUGCHK(!m_pszType);

        if (wszExt) 
        {
            m_pRequest->GetContentTypeOfRequestURI(m_szMime,sizeof(m_szMime),wszExt);
            m_pszType = m_szMime;
        }
        else
        {
            m_pszType = cszOctetStream;
        }
    }
public:
    CHttpResponse(CHttpRequest *pRequest, RESPONSESTATUS status=STATUS_OK, CONNHEADER connHdr = CONN_NOPREF)
    {
        ZEROMEM(this);

        m_pRequest       = pRequest;
        m_pRequest->m_rs = status;

        // Inform the request of the desire for keeping the connection
        m_pRequest->SetConnectionPersist(connHdr);
    }

    // for generated bodies (dir listings, redirects etc) & default bodies
    void SetBody(PCSTR pszBody, PCSTR pszType) 
    {
        DEBUGCHK(!m_hFile && !m_pszBody && !m_pszType && !m_dwLength);
        m_pszType = pszType;
        m_dwLength = strlen(pszBody);
        m_pszBody = pszBody;
    }

    // for error reponses (NOTE: some have no body, and hence psztatusBdy will be NULL in the table)
    void SetDefaultBody() 
    {
        if(rgStatus[m_pRequest->m_rs].pszStatusBody)
        {
            SetBody(rgStatus[m_pRequest->m_rs].pszStatusBody, cszTextHtml);
        }
    }
    // for real files
    void SetBody(HANDLE hFile, PCWSTR wszExt=NULL, DWORD dwLen=0) 
    {
        DEBUGCHK(!m_hFile && !m_pszBody && !m_pszType && !m_dwLength);
        m_hFile = hFile;
        SetTypeFromExtW(wszExt);
        if (dwLen)
        {
            m_dwLength = dwLen;
        }
        else 
        {
            m_dwLength = GetFileSize(m_hFile, 0);
            if (m_dwLength == 0)
            {
                m_dwLength = -1;  // Use this to signify empty file, needed for keep-alives
            }
        }
    }

    // Setup "Content-Length: 0" to make keep-alives happy.
    void SetZeroLenBody(void) 
    {
        m_dwLength = -1;
    }

    void SetLength(DWORD dw) 
    {
        m_dwLength = dw;
    }

    void UnSetMimeType(void) 
    {
        m_pszType = 0;
    }

public:
    void SendHeadersAndDefaultBodyIfAvailable(PCSTR pszExtraHeaders, PCSTR pszNewRespStatus);
    void SendRedirect(PCSTR pszRedirect, BOOL fFromFilter=FALSE);
    void SendResponse(PCSTR pszExtraHeaders=NULL, PCSTR pszNewRespStatus=NULL, BOOL fFromFilter=FALSE) 
    {
        // see FilterNoResponse for comments on this  
        if (!fFromFilter && m_pRequest->FilterNoResponse())
        {
            return;    
        }

        SendHeadersAndDefaultBodyIfAvailable(pszExtraHeaders,pszNewRespStatus);
        if (VERB_HEAD == m_pRequest->m_idMethod)
        {
            return;
        }

        if(m_hFile)
        {
            SendFile(m_pRequest->m_socket, m_hFile, m_pRequest);
        }
    }
};


