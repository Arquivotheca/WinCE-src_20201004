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
Module Name: ISAPI.CPP
Abstract: ISAPI handling code
--*/

#include "httpd.h"

// Amount of time before unloading an unused ISAPI
#define SCRIPT_TIMEOUT_DEFAULT   (1000*60*30)

const DWORD g_fIsapiExtensionModule = TRUE;

BOOL CGlobalVariables::InitExtensions(CReg *pWebsite) 
{
    DEBUGCHK(m_fRootSite);

    m_dwCacheSleep = pWebsite->ValueDW(RV_SCRIPTTIMEOUT,SCRIPT_TIMEOUT_DEFAULT);
    m_pISAPICache  = new CISAPICache();

    if (NULL == m_pISAPICache)
    {
        return FALSE;
    }

    InitScriptMapping(&m_scriptMap);
    InitScriptNoUnloadList(&m_scriptNoUnload);
    return TRUE;
}

void RemoveScriptNodes(PSCRIPT_MAP_NODE pScriptNodes, DWORD dwEntries);
void RemoveScriptNoUnloadList(WCHAR **szArray, DWORD dwEntries);

void CGlobalVariables::DeInitExtensions(void) 
{
    DEBUGCHK(m_fRootSite);
    RemoveScriptNodes(m_scriptMap.pScriptNodes,m_scriptMap.dwEntries);
    RemoveScriptNoUnloadList(m_scriptNoUnload.ppszNoUnloadEntry,m_scriptNoUnload.dwEntries);
}

BOOL CHttpRequest::ExecuteISAPI(WCHAR *wszExecutePath) 
{
    DWORD dwRet = HSE_STATUS_ERROR;
    EXTENSION_CONTROL_BLOCK ECB;
    CISAPI *pISAPIDLL = NULL;

    __try 
    {
        if (m_fIsChunkedUpload && !m_bufRequest.ParseInitialChunkedUpload(this,m_socket,&m_dwBytesRemainingInChunk)) 
        {
            m_rs = STATUS_BADREQ;
            return FALSE;
        }
            
        if (! g_pVars->m_pISAPICache->Load(wszExecutePath,&pISAPIDLL))
            return FALSE;
        
        // create an ECB (this allocates no memory)
        FillECB(&ECB);
        
        // call the ISAPI
        dwRet = pISAPIDLL->CallExtension(&ECB);
        
        // grab log data if any
        if(ECB.lpszLogData[0]) 
        {
            MyFree(m_pszLogParam);
            // silently fail on alloc failure.  ISAPI has sent all request
            // headers at this point anyway, we just lose log info.
            m_pszLogParam = MySzDupA(ECB.lpszLogData);
        }
    }
    __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) 
    { // catch all exceptions
        DEBUGMSG(ZONE_ERROR, (L"HTTPD: ISAPI DLL caused exception 0x%08x and was terminated\r\n", GetExceptionCode()));
        g_pVars->m_pLog->WriteEvent(IDS_HTTPD_EXT_EXCEPTION,wszExecutePath,GetExceptionCode(),L"HttpExtensionProc",GetLastError());
    }

    g_pVars->m_pISAPICache->Unload(pISAPIDLL);

    // set keep-alive status based on return code - if permitted
    switch(dwRet) 
    {
    case HSE_STATUS_SUCCESS_AND_KEEP_CONN:  
        SetConnectionPersist(CONN_WANTKEEP);
        break;
    case HSE_STATUS_SUCCESS:
        SetConnectionPersist(CONN_WANTCLOSE);
        break;
    }
    
    DEBUGMSG(ZONE_ISAPI, (L"HTTPD: ISAPI ExtProc returned %d keep-alive=%d\r\n", dwRet, GetConnectionPersist()));
    DEBUGCHK(dwRet != HSE_STATUS_PENDING);
    return (dwRet!=HSE_STATUS_ERROR);
}

void CHttpRequest::FillECB(LPEXTENSION_CONTROL_BLOCK pECB) 
{
    ZEROMEM(pECB);
    pECB->cbSize = sizeof(*pECB);
    pECB->dwVersion = HSE_VERSION;
    pECB->ConnID = (HCONN)this;
    
    DEBUGCHK(m_pszMethod);

    // NOTE:  IIS examines dwHttpStatusCode on the return of HttpExtensionProc and 
    // uses this value for logging.  CE does not support this functionality.
    pECB->dwHttpStatusCode = 200;    
    pECB->lpszMethod = m_pszMethod;
    pECB->lpszQueryString = (PSTR)(m_pszQueryString ? m_pszQueryString : cszEmpty);
    pECB->lpszContentType = (PSTR)(m_pszContentType ? m_pszContentType : cszEmpty);
    pECB->lpszPathInfo = (PSTR) (m_pszPathInfo ? m_pszPathInfo : cszEmpty);    
    pECB->lpszPathTranslated = (PSTR) (m_pszPathTranslated ? m_pszPathTranslated : cszEmpty);

    if (m_fIsChunkedUpload) 
    {
        // The ISAPI must determine that this is a chunked request and do the proper parsing.  
        // We set the cbTotalBytes to the number of bytes we've read in already because
        // (1) we don't know the actual available because we didn't parse out 
        // the chunked hdrs, and (2) for ISAPI extensions that don't 
        // process chunked encoding this will give it something consistent,
        // they'll think that all available data has been read in.
        pECB->cbTotalBytes = m_bufRequest.Count();
        pECB->cbAvailable  = m_bufRequest.Count();
    }
    else 
    {
        DEBUGCHK(pECB->cbAvailable <= pECB->cbTotalBytes);
        pECB->cbTotalBytes = m_dwContentLength;
        pECB->cbAvailable  = m_bufRequest.Count();
    }

    
    pECB->lpbData = m_bufRequest.Data();

    pECB->GetServerVariable = ::GetServerVariable;
    pECB->WriteClient = ::WriteClient;
    pECB->ReadClient = ::ReadClient;
    pECB->ServerSupportFunction = ::ServerSupportFunction;
}

BOOL WINAPI GetServerVariable(HCONN hConn, PSTR psz, PVOID pv, PDWORD pdw) 
{ 
    CHECKHCONN(hConn);
    CHECKPTRS2(psz, pdw);
    
    return ((CHttpRequest*)hConn)->GetServerVariable(psz, pv, pdw, FALSE);
}

int RecvToBuf(SOCKET socket, PVOID pv, DWORD dwReadBufSize, DWORD dwTimeout) 
{
    DEBUG_CODE_INIT;
    int iRecv = SOCKET_ERROR;
    DWORD dwAvailable;
    
    if (!MySelect(socket,dwTimeout)) 
    {
        SetLastError(WSAETIMEDOUT);
        myleave(1400);
    }

    if(ioctlsocket(socket, FIONREAD, &dwAvailable)) 
    {
        SetLastError(WSAETIMEDOUT);
        myleave(1401);
    }        

    iRecv = recv(socket, (PSTR) pv, (dwAvailable < dwReadBufSize) ? dwAvailable : dwReadBufSize, 0);
    if (iRecv == 0) 
    {
        DEBUGMSG(ZONE_REQUEST,(L"HTTP: ReadBuffer call to recv() returns 0\r\n"));
        SetLastError(WSAETIMEDOUT); 
        myleave(1402);
    }
    if (iRecv == SOCKET_ERROR) 
    {
        DEBUGMSG(ZONE_REQUEST,(L"HTTP: ReadBuffer call to recv() returns SOCKET_ERROR,GLE=0x%08x\r\n",GetLastError()));
        // recv() already has called SetLastError with appropriate message
        myleave(1403);
    }

done:
    DEBUGMSG_ERR(ZONE_REQUEST,(L"HTTPD: RecvToBuf returns error err = %d, GLE = 0x%08x\r\n",err,GetLastError()));
    return iRecv;
}

BOOL WINAPI ReadClient(HCONN hConn, PVOID pv, PDWORD pdw) 
{
    CHECKHCONN(hConn);
    CHECKPTRS2(pv, pdw);
    if ( *pdw == 0) 
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    return ((CHttpRequest*)hConn)->ReadClient(pv,pdw);
}

BOOL CHttpRequest::ReadClient(PVOID pv, PDWORD pdw) 
{
    if (m_fIsChunkedUpload)
    {
        return ReadClientChunked(pv,pdw);
    }
    else
    {
        return ReadClientNonChunked(pv,pdw);
    }
}

BOOL CHttpRequest::ReadClientNonChunked(PVOID pv, PDWORD pdw) 
{
    DEBUG_CODE_INIT;
    BOOL ret = FALSE;
    PVOID pvFilterModify = pv;
    DWORD dwBytesReceived;  
    DWORD dwBufferSize    = *pdw;

    m_bufRequest.InvalidateNextRequestIfAlreadyRead();

    if (!m_fHandleSSL)
    {
        dwBytesReceived = RecvToBuf(m_socket,pv,*pdw,g_pVars->m_dwConnectionTimeout);
    }
    else 
    {
        DEBUGCHK(g_pVars->m_fSSL);

        // The buffer is designed so it will immediatly unencrypt a request sent to
        // it, so there's never encrypted data at end of buf.
        DWORD dwReadyBytes = m_bufRequest.UnaccessedCount();
        if (dwReadyBytes >= *pdw) 
        {
            // We've already read in this data
            memcpy(pv,m_bufRequest.m_pszBuf+m_bufRequest.m_iNextInFollow,*pdw);
            m_bufRequest.m_iNextInFollow += (int) *pdw;
            dwBytesReceived = *pdw;
        }
        else 
        {
            if (m_hrPreviousReadClient == INPUT_ERROR) 
            {
                myleave(1395);
            }
            else if (m_hrPreviousReadClient == INPUT_TIMEOUT) 
            {
                SetLastError(WSAETIMEDOUT);
                myleave(1396);
            }
        
            // copy what's in buf, reset where our read starts (we'll reuse
            // portion of buffer that's not in initial POST), and recv()
            PSTR  pszNextWrite = (PSTR)pv + dwReadyBytes;
            DWORD dwToRead     = *pdw-dwReadyBytes;

            if (dwReadyBytes)
            {
                memcpy(pv,m_bufRequest.m_pszBuf+m_bufRequest.m_iNextInFollow,dwReadyBytes);
            }
            m_bufRequest.m_iNextDecrypt = m_bufRequest.m_iNextInFollow = m_bufRequest.m_iNextIn = (int) m_dwInitialPostRead;

            // It's theoretically possible that the amount of bytes we have to read after
            // doing the above memcpy() is less than the size of an SSL header.  (ie if buf size=500,
            // we have 496 encrypted bytes, then we'd only be trying to read 4 bytes, which is bad if SSL header is 5 bytes long.)
            DWORD dwRecvOffWire = max(m_SSLInfo.m_Sizes.cbHeader,dwToRead);
            m_hrPreviousReadClient = m_bufRequest.RecvBody(m_socket,dwRecvOffWire,TRUE,FALSE,this,FALSE);

            if (m_hrPreviousReadClient == INPUT_TIMEOUT || m_hrPreviousReadClient == INPUT_ERROR) 
            {
                // Only return FALSE in this case if there was no data in buffer, otherwise
                // we'll copy what we have and return FALSE next time client calls us.
                if (dwReadyBytes == 0) 
                {
                    if (m_hrPreviousReadClient == INPUT_ERROR)
                    {
                        SetLastError(WSAETIMEDOUT);
                    }
                    myleave(1398);
                }
            }

            DWORD dwToCopy = min(m_bufRequest.UnaccessedCount(),dwToRead);
            memcpy(pszNextWrite,m_bufRequest.m_pszBuf+m_bufRequest.m_iNextInFollow,dwToCopy);
            m_bufRequest.m_iNextInFollow += dwToCopy;
            dwBytesReceived = dwReadyBytes + dwToCopy;
        }
    }

    if (dwBytesReceived == 0 || dwBytesReceived == SOCKET_ERROR)
    {
        myleave(1399);
    }

    if (g_pVars->m_fFilters && !CallFilter(SF_NOTIFY_READ_RAW_DATA,(PSTR*) &pvFilterModify,(int*) &dwBytesReceived, NULL, (int *) &dwBufferSize))
    {
        myleave(1404);
    }

    // Check if filter modified pointer, copy if there's enough room for it.
    if (pvFilterModify != pv)     
    {
        if (*pdw <= dwBufferSize)         
        {
            memcpy(pv,pvFilterModify,dwBufferSize);
        }    
        else 
        {
            DEBUGMSG(ZONE_ERROR,(L"HTTPD: An ISAPI read filter modified data buffer so it was too big for ISAPI ReadClient buf, no copy will be done\r\n"));
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            myleave(1405);
        }            
    }
    *pdw = dwBytesReceived;
    ret = TRUE;
done:
    DEBUGMSG_ERR(ZONE_ISAPI,(L"HTTPD:ReadClient failed, GLE = 0x%08x, err = % err\r\n",GetLastError(), err));
    return ret;
}

BOOL WINAPI WriteClient(HCONN hConn, PVOID pv, PDWORD pdw, DWORD dwFlags) 
{
    CHECKHCONN(hConn);
    CHECKPTRS2(pv, pdw);
    if(dwFlags & HSE_IO_ASYNC) 
    { 
        SetLastError(ERROR_INVALID_PARAMETER); 
        return FALSE; 
    }

    return ((CHttpRequest*)hConn)->WriteClient(pv, pdw,FALSE);
}

BOOL WINAPI ServerSupportFunction(HCONN hConn, DWORD dwReq, PVOID pvBuf, PDWORD pdwSize, PDWORD pdwType) 
{
    CHECKHCONN(hConn);
    return ((CHttpRequest*)hConn)->ServerSupportFunction(dwReq, pvBuf, pdwSize, pdwType);
}

BOOL CHttpRequest::ServerSupportFunction(DWORD dwReq, PVOID pvBuf, PDWORD pdwSize, PDWORD pdwType) 
{
    switch(dwReq)
    {
        // Can never support these
        //case HSE_REQ_ABORTIVE_CLOSE:  
        //case HSE_REQ_DONE_WITH_SESSION:    
        //case HSE_REQ_ASYNC_READ_CLIENT:
        //case HSE_REQ_GET_CERT_INFO_EX:    
        //case HSE_REQ_GET_IMPERSONATION_TOKEN:
        //case HSE_REQ_GET_SSPI_INFO:
        //case HSE_REQ_IO_COMPLETION:
        //case HSE_REQ_REFRESH_ISAPI_ACL:
        //case HSE_REQ_TRANSMIT_FILE:

        default:
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        case HSE_REQ_IS_KEEP_CONN: 
        {    
            CHECKPTR(pvBuf);
            *((BOOL *) pvBuf) = GetConnectionPersist();
            return TRUE;
        }

        case HSE_REQ_SEND_URL:
        case HSE_REQ_SEND_URL_REDIRECT_RESP:
        {
            CHECKPTR(pvBuf);

            // close connection, because ISAPI won't have a chance to add headers anyway
            CHttpResponse resp(this, STATUS_MOVED, CONN_FORCECLOSE);
            resp.SendRedirect((PSTR)pvBuf); // send a special redirect body
            return TRUE;
        }        

        case HSE_REQ_MAP_URL_TO_PATH_EX:
        case HSE_REQ_MAP_URL_TO_PATH:
        {    
            CHECKPTRS2(pvBuf,pdwSize);
            PSTR szURLToMap = (PSTR)pvBuf;
            if (!IsSlash(szURLToMap[0])) 
            {
                DEBUGMSG(ZONE_ERROR,(L"HTTPD: ServerSupportFunction(HSE_REQ_MAP_URL_TO_PATH) returns failure because URL to map did not begin with a '/' or '\\'\r\n"));
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            if (dwReq == HSE_REQ_MAP_URL_TO_PATH_EX) 
            {
                if (!pdwType) 
                {
                    SetLastError(ERROR_INVALID_PARAMETER);
                    return FALSE;
                }
                return MapURLToPath(szURLToMap,pdwSize,(LPHSE_URL_MAPEX_INFO) pdwType);                
            }
            else 
            {
                // IIS docs are misleading here, but even if a valid param is passed in non-EX
                // case, ignore it.  (Like IIS.)    
                return MapURLToPath(szURLToMap,pdwSize);
            }
        }

        case HSE_REQ_SEND_RESPONSE_HEADER:
        {
            // no Connection header...let ISAPI send one if it wants
            CHttpResponse resp(this, STATUS_OK);
            // no body, default or otherwise (leave that to the ISAPI), but add default headers
            resp.SendResponse((PSTR) pdwType, (PSTR) pvBuf);
            return TRUE;
        }        
        
        case HSE_REQ_SEND_RESPONSE_HEADER_EXV:
        {
            SetStrictHTTP10();
            //Fall through to HSE_REQ_SEND_RESPONSEHEADER_EX
        }

        case HSE_REQ_SEND_RESPONSE_HEADER_EX:
        {
            // Note:  We ignore cchStatus and cchHeader members.  
            CHECKPTR(pvBuf);
            HSE_SEND_HEADER_EX_INFO *pHeaderEx = (HSE_SEND_HEADER_EX_INFO *) pvBuf;
                        
            // extended info may change our intention to keep the connection 

            // no body, default or otherwise (leave that to the ISAPI), but add default headers
            CHttpResponse resp(this, STATUS_OK, 
                               pHeaderEx->fKeepConn?CONN_WANTKEEP:CONN_WANTCLOSE);

            if (pHeaderEx->pszHeader == NULL)
            {
                resp.SetZeroLenBody();
            }

            resp.SendResponse(pHeaderEx->pszHeader, pHeaderEx->pszStatus);
            return TRUE;
        }
        
        case HSE_APPEND_LOG_PARAMETER:
        {
            return MyStrCatA(&m_pszLogParam,(PSTR) pvBuf,",");
        }

        // HSE_REQ_GET_LOCAL_SOCKADDR and HSE_REQ_GET_REMOTE_SOCKADDR are WinCE 
        // extensions to ISAPI's.  Makes it possible for apps to retrieve
        // the SOCKADDR of either local or remote client.
        case HSE_REQ_GET_LOCAL_SOCKADDR:
        {
            return (0 == getsockname(m_socket,(PSOCKADDR)pvBuf,(int*)pdwSize));
        }

        case HSE_REQ_GET_REMOTE_SOCKADDR:
        {
            return (0 == getpeername(m_socket,(PSOCKADDR)pvBuf,(int*)pdwSize));
        }        
    }
}    

BOOL CHttpRequest::MapURLToPath(PSTR pszBuffer, PDWORD pdwSize, LPHSE_URL_MAPEX_INFO pUrlMapEx)
{
    PWSTR wszPath;
    PSTR pszURL;
    DWORD dwBufNeeded = 0;
    BOOL ret;
    PVROOTINFO pVRoot;

    wszPath = m_pVrootTable->URLAtoPathW(pszBuffer,&pVRoot);
    if(!wszPath) 
    {
        // Assume failure on matching to virtual root, and not on mem alloc
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    dwBufNeeded = (DWORD) WideCharToMultiByte(CP_ACP,0, wszPath, -1, pszBuffer, 0 ,0,0);

    // For MAP_EX case, we set these vars from the passed structure, else we use the raw ptrs.
    if (pUrlMapEx) 
    {
        pszURL = pUrlMapEx->lpszPath;
        *pdwSize = MAX_PATH;
    }
    else 
    {
        pszURL = pszBuffer;
    }

    // To keep compat with IIS, we translate "/" to "\".  We do the conversion
    // only if we're using filters or if there's enough space in the buffer.
    if (g_pVars->m_fFilters || *pdwSize >= dwBufNeeded) 
    {
        for (int i = 0; i < (int) wcslen(wszPath); i++) 
        {
            if ( wszPath[i] == L'/')
            {
                wszPath[i] = L'\\';
            }
        }
    }

    if (FALSE == g_pVars->m_fFilters) 
    {
        // We check to make sure buffer is the right size because WideToMultyByte
        // will overwrite pieces of pszURL even on failure, leaving pszURL's 
        // content invalid
        if (*pdwSize < dwBufNeeded) 
        {
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            ret = FALSE;
        }
        else 
        {
            MyW2A(wszPath, pszURL, *pdwSize);
            ret = TRUE;
        }
        *pdwSize = dwBufNeeded;  
    }
    else 
    {
        // for EX case, put original URL as optional 5th parameter.
        ret = FilterMapURL(pszURL, wszPath, pdwSize,dwBufNeeded, pUrlMapEx ? pszBuffer : NULL);
    }    

    if (ret && pUrlMapEx) 
    {
        pUrlMapEx->cchMatchingPath = *pdwSize - 1;        // don't count \0
        pUrlMapEx->cchMatchingURL  = strlen(pszBuffer);
        pUrlMapEx->dwFlags = pVRoot->dwPermissions;
    }

    MyFree(wszPath);
    return ret;
}


void CISAPI::Unload(PWSTR wszDLLName) 
{ 
    if(m_pfnTerminate) 
    {
        __try 
        {
            if(SCRIPT_TYPE_FILTER == m_scriptType)
            {
                ((PFN_TERMINATEFILTER)m_pfnTerminate)(HSE_TERM_MUST_UNLOAD);
            }
            else if (SCRIPT_TYPE_EXTENSION == m_scriptType)
            {
                ((PFN_TERMINATEEXTENSION)m_pfnTerminate)(HSE_TERM_MUST_UNLOAD);
            }
            else if (SCRIPT_TYPE_ASP == m_scriptType)
            {
                ((PFN_TERMINATEASP)m_pfnTerminate)();
            }

        }
        __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) 
        {
            DWORD dwExceptionCode = SCRIPT_TYPE_FILTER == m_scriptType ?
                                    IDS_HTTPD_FILT_EXCEPTION : IDS_HTTPD_EXT_EXCEPTION;
            PWSTR wszFunction =     SCRIPT_TYPE_FILTER == m_scriptType ?
                                    L"TerminateFilter" : L"TerminateExtension"; 
                                    
            DEBUGMSG(ZONE_ERROR, (L"HTTPD: TerminateExtension faulted\r\n"));
            g_pVars->m_pLog->WriteEvent(dwExceptionCode,wszDLLName ? wszDLLName : m_wszDLLName,
                                        GetExceptionCode(),wszFunction,GetLastError());
        }
    }
    MyFreeLib(m_hinst); 
    m_hinst = NULL;
    m_pfnTerminate = NULL;
}


// To see if two file names are equal,
// do a normal string compare with the difference that "/" and "\" are treated
// as equiavalents.  (They may not be same in two passed variables due to the
// way web server does VRoot name mapping).

// Format like wcsicmp, returns 0 if strings are equal.
int PathNameCompare(WCHAR *src, WCHAR *dst) 
{
    WCHAR f,l;
    BOOL fSlashMatch;

    do  
    {
        fSlashMatch = (*dst == L'/' || *dst == L'\\') && (*src == L'/' || *src == L'\\');
        f = towlower(*dst++);
        l = towlower(*src++);
    } while (f && ((f == l) || fSlashMatch));

    if (fSlashMatch)
    {
        return 0;
    }

    return (int)(f - l);
}

int PathNameCompareN(WCHAR *src, WCHAR *dst, int len) 
{
    WCHAR f,l;
    BOOL fSlashMatch;

    do  
    {
        fSlashMatch = (*dst == L'/' || *dst == L'\\') && (*src == L'/' || *src == L'\\');
        f = towlower(*dst++);
        l = towlower(*src++);
    } while (f && ((f == l) || fSlashMatch) && --len);

    if (fSlashMatch)
    {
        return 0;
    }

    return (int)(f - l);
}


BOOL IsScriptInNoUnloadList(WCHAR *szDLLName) 
{
    for (DWORD i = 0; i < g_pVars->m_scriptNoUnload.dwEntries; i++) 
    {
        if (0 == PathNameCompare(g_pVars->m_scriptNoUnload.ppszNoUnloadEntry[i],szDLLName))
        {
            return TRUE;
        }
    }
    return FALSE;
}

BOOL CISAPI::LoadExports(HINSTANCE hInst, BOOL fVerifyOnly) 
{
    BOOL fRet = FALSE;

    if (SCRIPT_TYPE_ASP == m_scriptType) 
    {
        m_pfnHttpProc  = GetProcAddress(hInst, CE_STRING("ExecuteASP"));
        m_pfnTerminate = GetProcAddress(hInst, CE_STRING("TerminateASP"));

        fRet = (m_pfnHttpProc && m_pfnTerminate);
        goto done;
    }

    if (SCRIPT_TYPE_EXTENSION == m_scriptType) 
    {
        m_pfnGetVersion = GetProcAddress(hInst, CE_STRING("GetExtensionVersion"));
        m_pfnHttpProc     = GetProcAddress(hInst, CE_STRING("HttpExtensionProc"));
        m_pfnTerminate  = GetProcAddress(hInst, CE_STRING("TerminateExtension"));
    }
    else if (SCRIPT_TYPE_FILTER == m_scriptType) 
    {
        m_pfnGetVersion = GetProcAddress(hInst, CE_STRING("GetFilterVersion"));
        m_pfnHttpProc     = GetProcAddress(hInst, CE_STRING("HttpFilterProc"));
        m_pfnTerminate  = GetProcAddress(hInst, CE_STRING("TerminateFilter"));
    }
    else 
    {
        DEBUGCHK(0); // unknown script type
    }
    
    fRet = (m_pfnHttpProc && m_pfnGetVersion);

done:
    if (fVerifyOnly) 
    {
        // NULL out proc ptrs since we'll be unloading DLL data file.
        m_pfnGetVersion = m_pfnHttpProc = m_pfnTerminate = NULL;
    }

    return fRet;
}

BOOL CISAPI::Load(PWSTR wszPath)  
{
    // First we load the ISAPI as a LOAD_LIBRARY_AS_DATAFILE and verify
    // that it has the proper exports for given ISAPI.  We do this
    // because it guarantees that DllMain() won't be called until we're
    // sure we really are dealing with an ISAPI extension.

    HINSTANCE hLibData = LoadLibraryEx(wszPath,NULL,DONT_RESOLVE_DLL_REFERENCES);
    if (!hLibData) 
    {
        DEBUGMSG(ZONE_ERROR,(L"HTTPD: LoadLibraryEx(%s) fails.  GLE=0x%08x\r\n",wszPath,GetLastError()));
        return FALSE;
    }

    if (! LoadExports(hLibData,TRUE)) 
    {
        FreeLibrary(hLibData);
        return FALSE;
    }
    FreeLibrary(hLibData);

    // Now load the DLL "for real"
    m_hinst = LoadLibrary(wszPath); 
    if(!m_hinst) 
    {
        return FALSE;
    }

    if (SCRIPT_TYPE_EXTENSION == m_scriptType || SCRIPT_TYPE_ASP == m_scriptType)
    {
        m_fCanTimeout = ! (IsScriptInNoUnloadList(wszPath));
    }

    if (! LoadExports(m_hinst,FALSE))
    {
        goto done;
    }

    __try 
    {
        // call GetVersion immediately after load on extensions and Filters, but not ASP
        // if it's a filter we need to do some flags work first
        if(SCRIPT_TYPE_FILTER == m_scriptType) 
        {
            HTTP_FILTER_VERSION vFilt;
            // IIS ignores ISAPI version info, so do we.

            PREFAST_ASSERT(m_pfnGetVersion != NULL); // LoadExports verifies that m_pfnGetVersion != NULL.
            ((PFN_GETFILTERVERSION)m_pfnGetVersion)(&vFilt); 
            dwFilterFlags = vFilt.dwFlags;

            // client didn't set the prio flags, assign them to default
            // If they set more than one prio we use the highest + ignore others
            if (0 == (dwFilterFlags & SF_NOTIFY_ORDER_MASK)) 
            {
                dwFilterFlags |= SF_NOTIFY_ORDER_DEFAULT;
            }
            // If the Filter set neither SF_NOTIFY_SECURE_PORT nor SF_NOTIFY_NONSECURE_PORT
            // then we notify them for both.

            if (0 == (dwFilterFlags & (SF_NOTIFY_SECURE_PORT | SF_NOTIFY_NONSECURE_PORT))) 
            {
                dwFilterFlags |= SF_NOTIFY_SECURE_PORT | SF_NOTIFY_NONSECURE_PORT;
            }
        }
        else if (SCRIPT_TYPE_EXTENSION == m_scriptType) 
        {
            HSE_VERSION_INFO     vExt;
            // IIS ignores ISAPI version info, so do we.
            ((PFN_GETEXTENSIONVERSION)m_pfnGetVersion)(&vExt); 
        }

        return TRUE;
    }
    __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) 
    {
        DWORD dwExceptionCode = SCRIPT_TYPE_FILTER == m_scriptType ?
                                IDS_HTTPD_FILT_EXCEPTION : IDS_HTTPD_EXT_EXCEPTION;
        PWSTR wszFunction =     SCRIPT_TYPE_FILTER == m_scriptType ?
                                L"GetFilterVersion" : L"GetExtensionVersion"; 

        DEBUGMSG(ZONE_ERROR, (L"HTTPD: GetExtensionVersion faulted\r\n"));
        g_pVars->m_pLog->WriteEvent(dwExceptionCode,wszPath,GetExceptionCode(),wszFunction,GetLastError());
    }
    // fall through to error

done:
    m_pfnGetVersion = 0;
    m_pfnHttpProc = 0;
    m_pfnTerminate = 0;
    MyFreeLib(m_hinst);
    m_hinst = 0;
    return FALSE;
}


//**********************************************************************
// ISAPI Caching functions
//**********************************************************************
HINSTANCE CISAPICache::Load(PWSTR wszDLLName, CISAPI **ppISAPI, SCRIPT_TYPE st) 
{
    DEBUG_CODE_INIT;
    BOOL ret = FALSE;
    PISAPINODE pTrav = NULL;

    EnterCriticalSection(&m_CritSec);

    for (pTrav = m_pHead; pTrav != NULL; pTrav = pTrav->m_pNext) 
    {
        if ( 0 == wcsicmp(pTrav->m_pISAPI->m_wszDLLName, wszDLLName)) 
        {
            DEBUGMSG(ZONE_ISAPI,(L"HTTPD: Found ISAPI dll in cache, name = %s, cur ref count = %d\r\n",
                                  wszDLLName, pTrav->m_pISAPI->m_cRef));
            break;
        }
    }

    if (NULL == pTrav)     
    {
        DEBUGMSG(ZONE_ISAPI,(L"HTTPD: ISAPI dll name = %s not found in cache, creating new entry\r\n",wszDLLName));
        if (NULL == (pTrav = MyAllocNZ(ISAPINODE)))
        {
            myleave(1200);
        }

        if (NULL == (pTrav->m_pISAPI = new CISAPI(st)))
        {
            myleave(1201);
        }
            
        if (NULL == (pTrav->m_pISAPI->m_wszDLLName = MySzDupW(wszDLLName)))
        {
            myleave(1202);
        }

        if (! pTrav->m_pISAPI->Load(wszDLLName))
        {
            myleave(1203);
        }

        pTrav->m_pNext = m_pHead;
        m_pHead = pTrav;            
    }
    pTrav->m_pISAPI->m_cRef++;


    *ppISAPI = pTrav->m_pISAPI;
    ret = TRUE;
done:
    if (!ret) 
    {
        if (pTrav && pTrav->m_pISAPI) 
        {
            MyFree(pTrav->m_pISAPI->m_wszDLLName);
            delete pTrav->m_pISAPI;
        }
            
        MyFree(pTrav);
    }
        
    DEBUGMSG_ERR(ZONE_ISAPI,(L"HTTPD: CISAPICache::LoadISAPI failed, err = %d, GLE = 0x%08x\r\n",err,GetLastError()));
    LeaveCriticalSection(&m_CritSec);

    if (ret)
    {
        return pTrav->m_pISAPI->m_hinst;
    }
        
    return NULL;
}


// fRemoveAll = TRUE  ==> remove all ISAPI's, we're shutting down
//             = FALSE ==> only remove ones who aren't in use and whose time has expired

void CISAPICache::RemoveUnusedISAPIs(BOOL fRemoveAll) 
{
    PISAPINODE pTrav   = NULL;
    PISAPINODE pFollow = NULL;    
    PISAPINODE pDelete = NULL;
    SYSTEMTIME st;
    __int64 ft;

    EnterCriticalSection(&m_CritSec);
    GetSystemTime(&st);
    SystemTimeToFileTime(&st,(FILETIME*) &ft);

    // Figure out what time it was g_pVars->m_dwCacheSleep milliseconds ago.
    // Elements that haven't been used since then and that have no references
    // are deleted.

    ft -= FILETIME_TO_MILLISECONDS * g_pVars->m_dwCacheSleep;
    

    for (pTrav = m_pHead; pTrav != NULL; ) 
    {
        if (fRemoveAll ||
           (pTrav->m_pISAPI->m_fCanTimeout && pTrav->m_pISAPI->m_cRef == 0 && pTrav->m_pISAPI->m_ftLastUsed < ft))
        
        {
            DEBUGMSG(ZONE_ISAPI,(L"HTTPD: Freeing unused ISAPI Dll %s\r\n",pTrav->m_pISAPI->m_wszDLLName));
    
            pTrav->m_pISAPI->Unload();    
            delete pTrav->m_pISAPI;

            if (pFollow)
            {
                pFollow->m_pNext = pTrav->m_pNext;
            }
            else
            {
                m_pHead = pTrav->m_pNext;
            }

            pDelete = pTrav;
            pTrav = pTrav->m_pNext;
            MyFree(pDelete);
        }
        else 
        {
            if (pTrav->m_pISAPI->m_cRef == 0)
            {
                pTrav->m_pISAPI->CoFreeUnusedLibrariesIfASP();
            }
                
            pFollow = pTrav;
            pTrav = pTrav->m_pNext;
        }
    }

    LeaveCriticalSection(&m_CritSec);
}

//
// Script Mapping code.  Using script mapping causes all files of extension .xxx to
// be mapped to an ISAPI dll or ASP, as specified in the registry.
// 
void RemoveScriptNodes(PSCRIPT_MAP_NODE pScriptNodes, DWORD dwEntries) 
{
    for (DWORD i = 0; i < dwEntries; i++) 
    {
        MyFree(pScriptNodes[i].wszFileExtension);
        MyFree(pScriptNodes[i].wszISAPIPath);
    }
    MyFree(pScriptNodes);
}

void RemoveScriptNoUnloadList(WCHAR **szArray, DWORD dwEntries) 
{
    for (DWORD i = 0; i < dwEntries; i++)
    {
        MyFree(szArray[i]);
    }

    MyFree(szArray);
}

#define MAX_NO_UNLOAD_ENTRIES     10000

void InitScriptNoUnloadList(PSCRIPT_NOUNLOAD pNoUnloadList) 
{
    CReg reg(HKEY_LOCAL_MACHINE,RK_SCRIPT_NOUNLOAD);
    DWORD dwRegValues;
    DWORD dwEntries = 0;

    WCHAR szScriptName[MAX_PATH];
    WCHAR szBuf[MAX_PATH];
    WCHAR **szScriptNames;

    if (!reg.IsOK() || (0 == (dwRegValues = reg.NumValues()))) 
    {
        DEBUGMSG(ZONE_ISAPI,(L"HTTPD: No scripts to keep permanently loaded listed!\r\n"));
        return;
    }

    if (dwRegValues > MAX_NO_UNLOAD_ENTRIES) 
    {
        DEBUGMSG(ZONE_ERROR,(L"HTTPD: # registry entries in no-unload list is %d - too large\r\n",dwRegValues));
        DEBUGCHK(0);
        return;
    }

    if (NULL == (szScriptNames = MyRgAllocZ(WCHAR*,dwRegValues))) 
    {
        DEBUGMSG(ZONE_ERROR,(L"HTTP: Out of memory, GLE=0x%08x\r\n",GetLastError()));
        return;
    }

    for (DWORD j = 0; j < dwRegValues; j++) 
    {
        if (! reg.EnumValue(szBuf,CCHSIZEOF(szBuf),szScriptName,CCHSIZEOF(szScriptName)) || wcslen(szScriptName) == 0)
        {
            continue;
        }

        if (NULL == (szScriptNames[dwEntries] = MySzDupW(szScriptName))) 
        {
            DEBUGMSG(ZONE_ERROR,(L"HTTP: Out of memory, GLE=0x%08x\r\n",GetLastError()));
            RemoveScriptNoUnloadList(szScriptNames,dwEntries);
            return;
        }
        dwEntries++;
    }
    DEBUGCHK(dwEntries <= dwRegValues);

    if (dwEntries == 0)  
    {
        DEBUGMSG(ZONE_ISAPI,(L"HTTPD: No ScriptMaps defined!\r\n"));
        MyFree(szScriptNames);
        return;
    }

    pNoUnloadList->dwEntries          = dwEntries;
    pNoUnloadList->ppszNoUnloadEntry  = szScriptNames;
}

void InitScriptMapping(PSCRIPT_MAP pTable) 
{
    CReg reg(HKEY_LOCAL_MACHINE,RK_SCRIPT_MAP);
    DWORD dwRegValues;
    DWORD dwEntries = 0;
    PSCRIPT_MAP_NODE pScriptNodes = NULL;

    WCHAR szFileExt[MAX_PATH];
    WCHAR szPath[MAX_PATH];

    if (!reg.IsOK() || (0 == (dwRegValues = reg.NumValues()))) 
    {
        DEBUGMSG(ZONE_ISAPI,(L"HTTPD: No ScriptMaps defined!\r\n"));
        return;
    }

    if (NULL == (pScriptNodes = MyRgAllocZ(SCRIPT_MAP_NODE,dwRegValues))) 
    {
        DEBUGMSG(ZONE_ERROR,(L"HTTP: Out of memory, GLE=0x%08x\r\n",GetLastError()));
        return;
    }


    // Registry value name specifies the file extension, value is the ISAPI's path.
    PREFAST_SUPPRESS(12009,"dwRegValues is number of registry values from a protected reg key and is always small");
    for (DWORD j = 0; j < dwRegValues; j++) 
    {
        if (! reg.EnumValue(szFileExt,CCHSIZEOF(szFileExt),szPath,CCHSIZEOF(szPath)))
        {
            continue;
        }

        if (wcslen(szFileExt) < 2 || szFileExt[0] != L'.' || wcslen(szPath) == 0 || 0 == wcsicmp(szFileExt,L".dll")) 
        {
            DEBUGMSG(ZONE_ERROR,(L"HTTPD: ScriptPath (fileExtension=%s, path=%s) invalid.  Must begin with '.', have at least 2 characters, may NOT be .dll\r\n",szFileExt,szPath));
            continue;
        }

        pScriptNodes[dwEntries].wszFileExtension = MySzDupW(szFileExt);

        if (wcsicmp(szPath,L"\\windows\\asp.dll") == 0)
        {
            pScriptNodes[dwEntries].fASP = TRUE;
        }
        else        
        {
            pScriptNodes[dwEntries].wszISAPIPath = MySzDupW(szPath);
        }

        if (!pScriptNodes[dwEntries].wszFileExtension || (!pScriptNodes[dwEntries].fASP && ! pScriptNodes[dwEntries].wszISAPIPath)) 
        {
            DEBUGMSG(ZONE_ERROR,(L"HTTP: Out of memory, GLE=0x%08x\r\n",GetLastError()));
            RemoveScriptNodes(pScriptNodes,dwEntries);
            return;
        }

        dwEntries++;
    }
    DEBUGCHK(dwEntries <= dwRegValues);

    if (dwEntries == 0)  
    {
        DEBUGMSG(ZONE_ISAPI,(L"HTTPD: No ScriptMaps defined!\r\n"));
        MyFree(pScriptNodes);
        return;
    }

    pTable->dwEntries      = dwEntries;
    pTable->pScriptNodes   = pScriptNodes;
}

BOOL MapScriptExtension(WCHAR *wszFileExt, WCHAR **pwszISAPIPath, BOOL *pfIsASP) 
{
    PSCRIPT_MAP_NODE pScriptNodes = g_pVars->m_scriptMap.pScriptNodes;

    for (DWORD i = 0; i < g_pVars->m_scriptMap.dwEntries; i++) 
    {
        if (wcsicmp(wszFileExt,pScriptNodes[i].wszFileExtension) == 0) 
        {
            if (pScriptNodes[i].fASP)
            {
                *pfIsASP = TRUE;
            }
            else
            {
                *pwszISAPIPath = pScriptNodes[i].wszISAPIPath;
            }

            return TRUE;
        }
    }

    return FALSE;
}

BOOL CHttpRequest::SetPathTranslated()  
{
    static const DWORD dwExtraAlloc = 200;
    CHAR szBuf[MAX_PATH + 1];
    CHAR *szPathInfo;
    DWORD dwBufLen;
    DWORD ccPathInfo;

    DEBUGCHK(!m_pszPathTranslated);

    if (m_pszPathInfo) 
    {
        ccPathInfo = strlen(m_pszPathInfo);

        if (ccPathInfo+1 < sizeof(szBuf)) 
        {
            dwBufLen = sizeof(szBuf);
            strcpy(szBuf,m_pszPathInfo);
            szPathInfo = szBuf;
        }
        else 
        {
            // we alloc a little more than we need in case ServerSupportFunction(HSE_REQ_MAP_URL_TO_PATH) wants a bigger buffer.
            if (NULL == (szPathInfo = MySzAllocA(ccPathInfo+dwExtraAlloc)))
            {
                return FALSE;
            }

            memcpy(szPathInfo,m_pszPathInfo,ccPathInfo+1);
            dwBufLen = ccPathInfo + dwExtraAlloc;
        }
    }
    else 
    {
        // On IIS, if no PATH_INFO variable is used,
        // PATH_TRANSLATED is set to the mapping of the "/" directory, no matter
        // what directory the isapi was called from.
        strcpy(szBuf,"/");
        szPathInfo = szBuf;
        ccPathInfo = 1;
        dwBufLen = sizeof(szBuf);
    }

    if (!ServerSupportFunction(HSE_REQ_MAP_URL_TO_PATH,szPathInfo,&dwBufLen,0)) 
    {
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) 
        {
            if (szPathInfo != szBuf)
            {
                g_funcFree(szPathInfo,g_pvFreeData);
            }

            if (NULL == (szPathInfo = MySzAllocA(dwBufLen)))
            {
                return FALSE;
            }

            if (m_pszPathInfo)
            {
                memcpy(szPathInfo,m_pszPathInfo,ccPathInfo+1);
            }
            else
            {
                strcpy(szPathInfo,"/");
            }

            // there's a number of quasi-legitimate reasons a second call could fail (for instance
            // an ISAPI filter not working correctly), so we just give up and won't set PathTranslated info but don't fail request.
            if (! ServerSupportFunction(HSE_REQ_MAP_URL_TO_PATH,szPathInfo,&dwBufLen,0))
            {
                g_funcFree(szPathInfo,g_pvFreeData);
            }
            else
            {
                m_pszPathTranslated = szPathInfo;
            }
        }
        // else... ServerSupportFunction failed on something other than buf not large enough, fair enough.  Don't end session based on this.
    }
    else 
    {
        if (szPathInfo == szBuf) 
        {
            // make a copy of stack allocated buffer.
            if (NULL == (m_pszPathTranslated = MySzDupA(szBuf)))
            {
                return FALSE;
            }
        }
        else
        {
            m_pszPathTranslated = szPathInfo; // we've alloc'd already.
        }
    }
    return TRUE;
}

const CHAR g_szHexDigits[] = "0123456789abcdef";


BOOL CHttpRequest::WriteClient(PVOID pvBuf, PDWORD pdwSize, BOOL fFromFilter) 
{
    // On a HEAD request to an ASP page or ISAPI extension results in no data
    // being sent back, however for filters we do send data back when they 
    // tell us to with WriteClient call.
    if (m_idMethod == VERB_HEAD && !fFromFilter)
        return TRUE;

    // are we buffering?
    if (m_fBufferedResponse) 
    {
        return m_bufRespBody.AppendData((PSTR) pvBuf, (int) *pdwSize);
    }

    return SendData((PSTR) pvBuf, *pdwSize,TRUE);
}

//  Acts as the custom header class (for Filters call to AddHeader, SetHeader
//  and for ASP Call to AddHeader and for ASP Cookie handler.

//  We made this function part of the class because there's no reason to memcpy
//  data into a temp buffer before memcpy'ing it into the real buffer.

BOOL CBuffer::AddHeader(PCSTR pszName, PCSTR pszValue, BOOL fAddColon) 
{
    DEBUG_CODE_INIT;
    BOOL ret = FALSE;
    PSTR pszTrav;

    if (!pszName || !pszValue) 
    {
        DEBUGCHK(0);
        return FALSE;
    }

    int cbName  = strlen(pszName);
    int cbValue = strlen(pszValue);

    //  we need a buffer size of pszName + pszValue, a space, a trailing \r\n, and \0
    int cbTotal;
    SafeInt <DWORD> cbTotalSafe;

    // Use safeInt class to detect integer overflow.
    cbTotalSafe = cbName;
    cbTotalSafe += cbValue;
    cbTotalSafe += (CONSTSIZEOF("\r\n") + 1); // OK to do '+' in RHS expression because RHS is constant int.
    cbTotalSafe += (fAddColon ? 1 : 0);
    cbTotal = cbTotalSafe.Value();

    if ( ! AllocMem( cbTotal ))
    {
        myleave(900);
    }

    pszTrav = m_pszBuf + m_iNextIn;
    memcpy(pszTrav, pszName, cbName);
    pszTrav += cbName;

    // put space between name and value and colon if needed. 
    if (fAddColon)
    {
        *pszTrav++ = ':';
    }

    *pszTrav++ = ' ';      

    memcpy(pszTrav, pszValue, cbValue);
    memcpy(pszTrav + cbValue,"\r\n", CONSTSIZEOF("\r\n"));

    m_iNextIn += cbTotal;
    ret = TRUE;
done:
    DEBUGMSG_ERR(ZONE_RESPONSE,(L"HTTPD: CBuffer::AddHeader failed, err = %d\r\n",err));

    return ret;
}

