//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: REQUEST.H
Abstract: HTTP request class
--*/


// Scalar types used by CHTTPRequest
typedef enum {
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
typedef enum {
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
//  VERB_SEARCH,
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

inline BOOL IsBasic(AUTHTYPE a)          { return (a == AUTHTYPE_BASIC); }
inline BOOL IsNTLM(AUTHTYPE a)           { return (a == AUTHTYPE_NTLM); }
inline BOOL IsNegotiate(AUTHTYPE a)      { return (a == AUTHTYPE_NEGOTIATE); }
inline BOOL IsSupportedAuth(AUTHTYPE a)  { return (IsNTLM(a) || IsBasic(a) || IsNegotiate(a)); }

typedef enum {
    CONN_NONE    = 0,
    CONN_CLOSE   = 1,
    CONN_KEEP    = 2,
    CONN_UNKNOWN = 3
}
CONNHEADER;

#define CHTTPREQUEST_SIG 0xAB0D

// If strlen(url) is already known, don't recalc it but pass it in.  If unknown set = -1.
inline BOOL URLHasTrailingSlashA(PCSTR pszURL, int iURLLen=-1) {
    if (iURLLen == -1)
        iURLLen = strlen(pszURL);

    DEBUGCHK((iURLLen == (int)strlen(pszURL)) && (iURLLen>=1));
    return (pszURL[iURLLen-1]=='/' || pszURL[iURLLen-1]=='\\');
}

inline BOOL URLHasTrailingSlashW(WCHAR *wszURL, int iURLLen=-1) {
    if (iURLLen == -1)
        iURLLen = wcslen(wszURL);

    DEBUGCHK((iURLLen == (int)wcslen(wszURL)) && (iURLLen>=1));
    return (wszURL[iURLLen-1]=='/' || wszURL[iURLLen-1]=='\\');
}

inline void RemoveTrailingSlashIfNeededA(PSTR pszURL, int iURLLen=-1) {
    if (iURLLen == -1)
        iURLLen = strlen(pszURL);

    if (URLHasTrailingSlashA(pszURL,iURLLen))
        pszURL[iURLLen-1] = 0;
}

RESPONSESTATUS GLEtoStatus(int iGLE);


// This object is the top-level object for an incoming HTTP request. One such object
// is created per request & a thread is created to handle it.

class CFilterInfo;
class CWebDav;
class CHttpResponse;

class CHttpRequest {
    // socket
    DWORD   m_dwSig;
    SOCKET  m_socket;
    CHttpRequest *m_pNext;
    
    // buffers
    CBuffer m_bufRequest;
    CBuffer m_bufRespBody;
    
    // method, version, URL etc. Direct results of parse
    PSTR    m_pszMethod;
    PSTR    m_pszURL;
    PSTR    m_pszContentType;
    DWORD   m_dwContentLength;
    PSTR    m_pszAccept;
    FILETIME m_ftIfModifiedSince;
    DWORD   m_dwIfModifiedLength;
    PSTR    m_pszCookie;
    PSTR    m_pszHost;

    // Decoded URL (indirect results of parse)
    PSTR    m_pszQueryString;
    PWSTR   m_wszPath;
    PWSTR   m_wszExt;
    PSTR    m_pszPathInfo;
    PSTR    m_pszPathTranslated;
    DWORD   m_dwFileAttributes;  // results of GetFileAttributes(m_wszPath) call.

    DWORD   m_dwInitialPostRead;  // how much we read off wire before calling ISAPI extension
    DWORD   m_ccWPath; // # of wide chars of m_wszPath.

    PVROOTINFO m_pVRoot; // do NOT free, points to global data that is alloced/freed on server init/deinit.

    // Logging members
    PSTR    m_pszLogParam;
    RESPONSESTATUS m_rs;

    BOOL m_fBufferedResponse;   // Are we using m_bufResponse or sending straight to client?
    HRINPUT m_hrPreviousReadClient;    // Should next call to ReadClient() timeout?

    // Parsing functions
    void FreeHeaders(void) {
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

    BOOL CheckAuth(AUTHLEVEL auth) {
        return (m_AuthLevelGranted >= auth) ? TRUE : FALSE;
    }

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
    PSTR    m_pszSecurityOutBuf;        // buffer to send client on NTLM/negotiate response, Base64 encoded
    DWORD   m_dwVersion;    // LOWORD=minor, HIWORD=major.  
    VERB    m_idMethod;
    CBuffer m_bufRespHeaders;
    CFilterInfo *m_pFInfo;      // Filter state information
    BOOL    m_fKeepAlive;
    BOOL    m_fResponseHTTP11;  // put "HTTP/1.1" in response headers?  Currently only WebDAV will spit out HTTP 1.1 headers, so an HTTP/1.1 request to non WebDAV resource still returns 1.0.

    DWORD   MinorHTTPStatus(void) { return m_fResponseHTTP11 ? 1 : 0; }
    RESPONSESTATUS GetStatus(void) { return m_rs; }

#if defined (DEBUG)
    // When HTTP header buf has a \0 temporarily put into it to create a string, no further 
    // calls to FindHttpHeader can be made, as header we're looking for maybe after '\0'.
    BOOL             m_fHeadersInvalid;     
    void             DebugSetHeadersInvalid(BOOL f) { m_fHeadersInvalid = f; }
#else
    void             DebugSetHeadersInvalid(BOOL f) { ; }
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
    BOOL SendAuthenticate(void)  { return !(m_SSLInfo.m_pClientCertContext); }

    void CloseSocket(void) {
        if (m_socket) {
            // In certain scenarios, HTTPD may not be reading the last 2 bytes of content.
            // Read them now if they are available so we do not send a TCP reset to the client.
            // For certain TCP stacks, receiving this reset will cause it to kill the client
            // connection even though HTTPD has sent back all the required bytes and closed the connection.
            BYTE recvBuf[4]; // Recv up to 4 bytes just in case
            RecvToBuf(m_socket,recvBuf,sizeof(recvBuf),0);

            // Now perform socket shutdown.
            shutdown(m_socket, 1);
            closesocket(m_socket);
            m_socket = 0;
        }
    }

    // In some scenarios it's easier to manipulate m_wszPath if it can 
    // be assumed there's a '\' as trailing character.
    void AddTrailingSlashToPhysicalPath(void) {
        if ((m_ccWPath > 1) && (! URLHasTrailingSlashW(m_wszPath,m_ccWPath))) {
            m_wszPath[m_ccWPath++]  = '\\';
            m_wszPath[m_ccWPath]    = 0;
        }
        DEBUGCHK(m_ccWPath == wcslen(m_wszPath));
    }

    void RemoveTrailingSlashToPhysicalPath(void) {
        if ((m_ccWPath > 1) && URLHasTrailingSlashW(m_wszPath,m_ccWPath)) {
            m_wszPath[--m_ccWPath]  = 0;
        }
        DEBUGCHK((m_wszPath==NULL) || m_ccWPath == wcslen(m_wszPath));
    }

    BOOL IsRootPath(void) {
        return ((m_wszPath[0] == '\\' || m_wszPath[0] == '/') && m_wszPath[1] == 0);
    }

    BOOL IsRootVDir(void) {
        return (IsSlash(m_pszURL[0]) && m_pszURL[1] == 0);
    }

    // Only case we'd be inactive is during shutdown.
    BOOL IsActive(void)  { return (m_socket != 0); }
    CONNHEADER GetConnHeader()   { return m_fKeepAlive ? CONN_KEEP : CONN_CLOSE; }

    void GetContentTypeOfRequestURI(PSTR szContentType, DWORD ccContentType, const WCHAR *wszExt=NULL);
    
private:
    friend class CHttpResponse;

    // Authentication data members
    AUTHLEVEL m_AuthLevelGranted;
    PSTR    m_pszAuthType;
    PSTR    m_pszRawRemoteUser;     // Holds base64 encoded data, before auth decodes it
    PSTR    m_pszRemoteUser;
    WCHAR*  m_wszRemoteUser;
    WCHAR*  m_wszMemberOf;
    PSTR    m_pszPassword;
    AUTH_STATE m_AuthState;          // state info for NTLM process, needs to be saved across requests
    DWORD   m_dwAuthFlags;

    BOOL    m_fDisabledNagle;

    // Used for sensitive strings that should be zeroed out, in particular password info.
    void ZeroAndFree(PSTR &pszData) {
        if (pszData) {
            DWORD dw = strlen(pszData);
            memset(pszData,0,dw);
        }
        MyFree(pszData);
    }

    void ZeroAndFree(WCHAR *&wszData) {
        if (wszData) {
            DWORD dw = wcslen(wszData)*sizeof(WCHAR);
            memset(wszData,0,dw);
        }
        MyFree(wszData);
    }

    // Authentication functions
    void FreeAuth(void)  {
        MyFree(m_pszSecurityOutBuf);
        ZeroAndFree(m_pszPassword);
        ZeroAndFree(m_pszRawRemoteUser);
        //  Don't free NTLM structs or user name info, they're possibly persisted across multiple requests.
    }

    // User name info is persisted across the session.
    void FreePersistedAuthInfo(void) {
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
    BOOL VerbSupportsDirBrowsing(void) {
        return (m_idMethod == VERB_GET || m_idMethod == VERB_HEAD);
    }

    void Init(SOCKET sock, BOOL fSecure);

    BOOL ReadPostData(DWORD dwMaxSizeToRead, BOOL fInitialPostRead, BOOL fSSLRenegotiate);

    // ISAPI extension handling functions
    BOOL HandleScript();
    BOOL SetPathTranslated();
    BOOL ExecuteISAPI(WCHAR *wszExecutePath);
    void FillECB(LPEXTENSION_CONTROL_BLOCK pECB);
    BOOL GetServerVariable(PSTR pszVar, PVOID pvOutBuf, PDWORD pdwOutSize, BOOL fFromFilter);
    BOOL WriteClient(PVOID pvBuf, PDWORD pdwSize, BOOL fFromFilter);
    BOOL ServerSupportFunction(DWORD dwReq, PVOID pvBuf, PDWORD pdwSize, PDWORD pdwType);
    BOOL ReadClient(PVOID pv, PDWORD pdw);

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
    CHttpRequest(SOCKET sock, BOOL fSecure) {
        Init(sock,fSecure);
    }

    ~CHttpRequest();
    BOOL ReInit();

    BOOL IsSecure(void)           { return m_fHandleSSL; }
    void GetUserAndGroupInfo(PAUTH_STATE pAuth);
    AUTHLEVEL DeterminePermissionGranted(const WCHAR *wszUserList, AUTHLEVEL authDefault);

    // VRoot Accessors
    DWORD       GetPerms(void)          { return m_pVRoot ? m_pVRoot->dwPermissions : 0; }
    AUTHLEVEL   GetAuthReq(void)        { return m_pVRoot ? m_pVRoot->AuthLevel     : AUTH_PUBLIC;     }
    SCRIPT_TYPE GetScriptType(void)     { return m_pVRoot ? m_pVRoot->ScriptType    : SCRIPT_TYPE_NONE;    }
    BOOL        AllowBasic(void)        { return m_pVRoot ? m_pVRoot->fBasic        : 0; }
    BOOL        AllowNTLM(void)         { return m_pVRoot ? m_pVRoot->fNTLM         : 0; }
    BOOL        AllowBasicToNTLM(void)  { return m_pVRoot ? m_pVRoot->fBasicToNTLM  : 0; }
    BOOL        AllowNegotiate(void)    { return m_pVRoot ? m_pVRoot->fNegotiate    : 0; }
    BOOL        AllowDirBrowse(void)    { return m_pVRoot ? m_pVRoot->fDirBrowse    : 0; }
    WCHAR *     GetUserList(void)       { return m_pVRoot ? m_pVRoot->wszUserList   : 0; }

    BOOL URLHasTrailingSlash(int iURLLen=-1)   {
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


    //  ASP Specific
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
class CHttpResponse {
    CONNHEADER m_connhdr;
    PCSTR   m_pszType;
    DWORD   m_dwLength;
    PCSTR   m_pszRedirect;
    PCSTR   m_pszExtraHeaders;
    PCSTR   m_pszBody;
    HANDLE  m_hFile;
    char    m_szMime[MAXMIME];
    CHttpRequest *m_pRequest;   // calling request class, for callbacks
    
private:
    void SetTypeFromExtW(PCWSTR wszExt)  {
        DEBUGCHK(!m_pszType);

        if (wszExt) {
            m_pRequest->GetContentTypeOfRequestURI(m_szMime,sizeof(m_szMime),wszExt);
            m_pszType = m_szMime;
        }
        else
            m_pszType = cszOctetStream;
    }
public:
    CHttpResponse(CHttpRequest *pRequest, RESPONSESTATUS status=STATUS_OK, CONNHEADER connhdr=CONN_UNKNOWN) {
        ZEROMEM(this);

        m_connhdr        = (connhdr!=CONN_UNKNOWN) ? connhdr : pRequest->GetConnHeader();
        m_pRequest       = pRequest;
        m_pRequest->m_rs = status;
    }
    // for generated bodies (dir listings, redirects etc) & default bodies
    void SetBody(PCSTR pszBody, PCSTR pszType) {
        DEBUGCHK(!m_hFile && !m_pszBody && !m_pszType && !m_dwLength);
        m_pszType = pszType;
        m_dwLength = strlen(pszBody);
        m_pszBody = pszBody;
    }
    // for error reponses (NOTE: some have no body, and hence psztatusBdy will be NULL in the table)
    void SetDefaultBody() {
        if(rgStatus[m_pRequest->m_rs].pszStatusBody)
            SetBody(rgStatus[m_pRequest->m_rs].pszStatusBody, cszTextHtml);
    }
    // for real files
    void SetBody(HANDLE hFile, PCWSTR wszExt=NULL, DWORD dwLen=0) {
        DEBUGCHK(!m_hFile && !m_pszBody && !m_pszType && !m_dwLength);
        m_hFile = hFile;
        SetTypeFromExtW(wszExt);
        if (dwLen)
            m_dwLength = dwLen;
        else {
            m_dwLength = GetFileSize(m_hFile, 0);
            if (m_dwLength == 0)
                m_dwLength = -1;  // Use this to signify empty file, needed for keep-alives
        }
    }

    // Setup "Content-Length: 0" to make keep-alives happy.
    void SetZeroLenBody(void) {
        m_dwLength = -1;
    }

    void SetLength(DWORD dw) {
        m_dwLength = dw;
    }

    void UnSetMimeType(void) {
        m_pszType = 0;
    }
private:
    void SendBody();
public:
    void SendHeaders(PCSTR pszExtraHeaders, PCSTR pszNewRespStatus);
    void SendRedirect(PCSTR pszRedirect, BOOL fFromFilter=FALSE);
    void SendResponse(PCSTR pszExtraHeaders=NULL, PCSTR pszNewRespStatus=NULL, BOOL fFromFilter=FALSE) {
        // see FilterNoResponse for comments on this  
        if (!fFromFilter && m_pRequest->FilterNoResponse())
            return; 

            
        SendHeaders(pszExtraHeaders,pszNewRespStatus);
        SendBody();
    }
};


