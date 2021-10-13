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
Module Name: RESPONSE.CPP
Abstract: Handles response to client.
--*/

#include "httpd.h"


#define APPENDCRLF(psz, cchBufSpace)     { ASSERT(cchBufSpace >= 2); *psz++ = '\r';  *psz++ = '\n'; cchBufSpace -= 2;}

// ***NOTE***: The order of items in this table must match that in enum RESPONSESTATUS.
// If any additional entries are added they must match this enum, and update httpd.rc as well.

STATUSINFO rgStatus[STATUS_MAX] = 
{
{ 200, "OK",                        NULL, FALSE},
{ 302, "Object Moved",              NULL, TRUE},
{ 304, "Not Modified",              NULL, FALSE},
{ 400, "Bad Request",               NULL, TRUE},
{ 401, "Unauthorized",              NULL, TRUE},
{ 403, "Forbidden",                 NULL, TRUE},
{ 404, "Object Not Found",          NULL, TRUE},
{ 500, "Internal Server Error",     NULL, TRUE},
{ 501, "Not Implemented",           NULL, TRUE},
// status codes added for WebDAV support.
{ 201, "Created",                   NULL, FALSE},
{ 204, "No Content",                NULL, FALSE},
{ 207, "Multi-Status",              NULL, FALSE},
{ 405, "Method Not Allowed",        NULL, TRUE},
{ 409, "Conflict",                  NULL, TRUE},
{ 411, "Length Required",           NULL, TRUE},
{ 412, "Precondition Failed",       NULL, TRUE},
{ 413, "Request Entity Too Large",  NULL, TRUE},
{ 415, "Unsupported Media Type",    NULL, TRUE},
{ 423, "Locked",                    NULL, TRUE},
{ 507, "Insufficient Storage",      NULL, TRUE}
};

#define LCID_USA    MAKELANGID(LAND_ENGLISH, SUBLANG_ENGLISH_US);

// Automatically updates cchBufferSpace
DWORD GetETagFromFiletime(FILETIME *pft, DWORD dwFileSize, PSTR szWriteBuffer, size_t& cchWriteBuffer) 
{
    DWORD ret = _snprintf_s (szWriteBuffer,cchWriteBuffer,cchWriteBuffer,"\"%x%x%x%x%x%x%x%x:%x:%x\"",
                    (DWORD)(((PUCHAR)pft)[0]),(DWORD)(((PUCHAR)pft)[1]),
                    (DWORD)(((PUCHAR)pft)[2]),(DWORD)(((PUCHAR)pft)[3]),
                    (DWORD)(((PUCHAR)pft)[4]),(DWORD)(((PUCHAR)pft)[5]),
                    (DWORD)(((PUCHAR)pft)[6]),(DWORD)(((PUCHAR)pft)[7]),
                    dwFileSize,
                    g_pVars->m_dwSystemChangeNumber);
    ASSERT(ret != -1);
    cchWriteBuffer -= ret;
    return ret;
}


void CHttpResponse::SendHeadersAndDefaultBodyIfAvailable(PCSTR pszExtraHeaders, PCSTR pszNewRespStatus) 
{
    DEBUG_CODE_INIT;
    HRESULT hr = S_OK;
    CHAR szBuf[HEADERBUFSIZE - 100];  // Subtract a little to keep stack size of this < 4KB.
    size_t cchBufSpace = sizeof(szBuf)/sizeof(szBuf[0]);
    PSTR pszBuf = szBuf;
    int iLen;
    PSTR pszTrav;
    size_t cchNeeded;
    SafeInt<DWORD> cchNeededSafe;
    BOOL fUsingHTTP10 = (m_pRequest->MinorHTTPVersion() == 0);
    BOOL fIsStrictHTTP10 = m_pRequest->IsStrictHTTP10();
    BOOL fPersistingConnection = m_pRequest->GetConnectionPersist();
    
    if ( g_pVars->m_fFilters && ! m_pRequest->CallFilter(SF_NOTIFY_SEND_RESPONSE)) 
    {
        myleave(97);
    }

    // We do calculation up front to see if we need a allocate a buffer or not.
    // Certain may be set by an ISAPI Extension, Filter, or ASP
    // and we must do the check to make sure their sum plus http headers server
    // uses will be less than buffer we created.

    // Note: we don't use , even though SafeInt throws exceptions
    // on error cases, because the entire HTTP request is wrapped in a __try/__
    cchNeededSafe = m_pRequest->m_bufRespHeaders.Count();
    cchNeededSafe += MyStrlenA(pszExtraHeaders);
    cchNeededSafe += MyStrlenA(pszNewRespStatus); 
    cchNeededSafe += MyStrlenA(m_pszRedirect);
    cchNeededSafe += MyStrlenA(m_pRequest->m_pszSecurityOutBuf);
    cchNeededSafe += ((m_pRequest->m_pFInfo && m_pRequest->m_rs == STATUS_UNAUTHORIZED) ? 
                      MyStrlenA(m_pRequest->m_pFInfo->m_pszDenyHeader) : 0);

    if (VERB_HEAD != m_pRequest->m_idMethod) 
    {
        cchNeededSafe += MyStrlenA(m_pszBody);
    }

    cchNeededSafe += 1000; // Add 1000 for extra HTTP headers this function adds.  Always well < this.
    cchNeeded = cchNeededSafe.Value(); // Store in a regular DWORD for use outside this block.

    // If what we need is > header buf size, 
    if (cchNeeded > _countof(szBuf)) 
    {
        if (!(pszBuf = MyRgAllocNZ(CHAR,cchNeeded))) 
        {
            myleave(98);
        }
        cchBufSpace = cchNeeded;
    }
    pszTrav = pszBuf;

    // the status line
    // ServerSupportFunction on SEND_RESPONSE_HEADER can override the server value

    // NOTE: using sprintf on user supplied strings is very dangerous as there may be
    // sprintf qualifiers in it, so we'll be safe and strcpy.

    if (pszNewRespStatus)  
    {
        hr = StringCchPrintfExA(pszTrav, cchBufSpace, &pszTrav, &cchBufSpace, 0,
                    "HTTP/%d.%d %s\r\n", 
                    m_pRequest->MajorHTTPVersion(), 
                    m_pRequest->MinorHTTPVersionMax(),
                    pszNewRespStatus);
        ASSERT(SUCCEEDED(hr));
    }
    else 
    {
        hr = StringCchPrintfExA(pszTrav,cchBufSpace, &pszTrav, &cchBufSpace, 0,
                   cszRespStatus, 
                   m_pRequest->MinorHTTPVersionMax(),
                   rgStatus[m_pRequest->m_rs].dwStatusNumber, 
                   rgStatus[m_pRequest->m_rs].pszStatusText);
        ASSERT(SUCCEEDED(hr));       
    }

    // GENERAL HEADERS
    // the Date line
    SYSTEMTIME st;
    GetSystemTime(&st); // NOTE: Must be GMT!

    hr = StringCchCopyExA(pszTrav, cchBufSpace, cszRespDate, &pszTrav, &cchBufSpace, 0);
    ASSERT(SUCCEEDED(hr));
    // Note: WriteDateGMT updates cchBufSpace automatically
    pszTrav += WriteDateGMT(pszTrav,cchBufSpace,&st,TRUE);

    // In HTTP 1.1 we say "Connection: close" when closing
    if (!fUsingHTTP10 && !fPersistingConnection)
    {
        hr = StringCchPrintfExA(pszTrav, cchBufSpace, &pszTrav, &cchBufSpace, 0, cszConnectionResp, cszConnClose);
        ASSERT(SUCCEEDED(hr)); 
    }

    // In HTTP 1.0 we say "Connection   : Keep-Alive" when keeping alive
    if (fUsingHTTP10 && !fIsStrictHTTP10 && fPersistingConnection)
    {
        hr = StringCchPrintfExA(pszTrav, cchBufSpace, &pszTrav, &cchBufSpace, 0, cszConnectionResp, cszKeepAlive);
        ASSERT(SUCCEEDED(hr));
    }

    // Having set the reply, we may not change this anymore
    m_pRequest->LockConnectionPersist();

    // RESPONSE HEADERS    
    // the server line
    hr = StringCchPrintfExA(pszTrav, cchBufSpace, &pszTrav, &cchBufSpace, 0, cszRespServer, m_pRequest->m_pWebsite->m_szServerID); 
    ASSERT(SUCCEEDED(hr));

    // the Location header (for redirects)
    if (m_pszRedirect) 
    {
        DEBUGCHK(m_pRequest->m_rs == STATUS_MOVED);
        
        hr = StringCchCopyExA(pszTrav, cchBufSpace, cszRespLocation, &pszTrav, &cchBufSpace, 0);
        ASSERT(SUCCEEDED(hr));       
        hr = StringCchCopyExA(pszTrav, cchBufSpace, m_pszRedirect, &pszTrav, &cchBufSpace, 0);
        ASSERT(SUCCEEDED(hr));
     
        APPENDCRLF(pszTrav, cchBufSpace);        
    }

    // the WWW-Authenticate line
    if (m_pRequest->m_rs == STATUS_UNAUTHORIZED && m_pRequest->SendAuthenticate()) 
    {
        // In the middle of an NTLM/negotiate authentication session, send back security context info to client.
        if (m_pRequest->m_pszSecurityOutBuf) 
        {
            if (m_pRequest->m_AuthState.m_AuthType==AUTHTYPE_NTLM) 
            {
                hr = StringCchPrintfExA(pszTrav,cchBufSpace,&pszTrav,&cchBufSpace,0,
                                NTLM_AUTH_FORMAT " %s\r\n",NTLM_AUTH_STR,m_pRequest->m_pszSecurityOutBuf);
                ASSERT(SUCCEEDED(hr));
            }
            else if (m_pRequest->m_AuthState.m_AuthType==AUTHTYPE_NEGOTIATE) 
            {
                hr = StringCchPrintfExA(pszTrav,cchBufSpace,&pszTrav,&cchBufSpace,0,
                                NEGOTIATE_AUTH_FORMAT " %s\r\n",NEGOTIATE_AUTH_STR,m_pRequest->m_pszSecurityOutBuf);
                ASSERT(SUCCEEDED(hr));
            }
            else 
            {
                DEBUGCHK(0);
            }
        }
        else 
        {
            // If we're not in the middle of security session, send back all security session types we support.
            if (m_pRequest->AllowBasic()) 
            {
                hr = StringCchPrintfExA(pszTrav,cchBufSpace,&pszTrav,&cchBufSpace,0,
                                BASIC_AUTH_FORMAT "\r\n",BASIC_AUTH_STR(g_pVars->m_szBasicRealm));
                ASSERT(SUCCEEDED(hr));
            }
            if (m_pRequest->AllowNTLM()) 
            {
                hr = StringCchPrintfExA(pszTrav,cchBufSpace,&pszTrav,&cchBufSpace,0,
                                NTLM_AUTH_FORMAT "\r\n",NTLM_AUTH_STR);
                ASSERT(SUCCEEDED(hr));
            }
            if (m_pRequest->AllowNegotiate()) 
            {
                hr = StringCchPrintfExA(pszTrav,cchBufSpace,&pszTrav,&cchBufSpace,0,
                                NEGOTIATE_AUTH_FORMAT "\r\n",NEGOTIATE_AUTH_STR);
                ASSERT(SUCCEEDED(hr));
            }
        }
    }

    // ENTITY HEADERS
    // the Last-Modified line
    if (m_hFile) 
    {
        WIN32_FILE_ATTRIBUTE_DATA fData;
        
        if( GetFileAttributesEx(m_pRequest->m_wszPath,GetFileExInfoStandard,&fData) && 
            FileTimeToSystemTime(&fData.ftLastWriteTime, &st) )
        {
            hr = StringCchPrintfExA(pszTrav,cchBufSpace,&pszTrav,&cchBufSpace,0,"%s ",cszRespLastMod);
            ASSERT(SUCCEEDED(hr));
            // Note: WriteDateGMT updates cbBufSpace automatically
            pszTrav += WriteDateGMT(pszTrav,cchBufSpace,&st);
            APPENDCRLF(pszTrav, cchBufSpace);

            if (m_pRequest->m_pWebsite->m_fWebDav) 
            {
                hr = StringCchCopyExA(pszTrav, cchBufSpace, cszETag, &pszTrav, &cchBufSpace, 0);
                ASSERT(SUCCEEDED(hr));
                *pszTrav++ = ' ';
                cchBufSpace--;
                pszTrav += GetETagFromFiletime(&fData.ftLastWriteTime,fData.dwFileAttributes,pszTrav,cchBufSpace);
                APPENDCRLF(pszTrav, cchBufSpace);
            }
        }
    }

    // the Content-Type line
    if (m_pszType) 
    {
        hr = StringCchPrintfExA(pszTrav,cchBufSpace,&pszTrav,&cchBufSpace,0,cszRespType,m_pszType);
        ASSERT(SUCCEEDED(hr));
    }

    // the Content-Length line
    if (m_dwLength) 
    {
        // If we have a file that is 0 length, it is set to -1.
        if (m_dwLength == -1) 
        {
            m_dwLength = 0;
        }
        hr = StringCchPrintfExA(pszTrav, cchBufSpace, &pszTrav, &cchBufSpace, 0, cszRespLength, m_dwLength);
        ASSERT(SUCCEEDED(hr));
    }

    if ((m_pRequest->m_rs == STATUS_UNAUTHORIZED) && m_pRequest->m_pFInfo && m_pRequest->m_pFInfo->m_pszDenyHeader) 
    {
        hr = StringCchCopyExA(pszTrav,cchBufSpace,m_pRequest->m_pFInfo->m_pszDenyHeader,&pszTrav,&cchBufSpace,0);
        ASSERT(SUCCEEDED(hr));
        // It's the script's responsibility to add any \r\n to the headers.        
    }

    if (m_pRequest->m_bufRespHeaders.Data()) 
    {
        ASSERT(cchBufSpace >= m_pRequest->m_bufRespHeaders.Count());
        memcpy(pszTrav,m_pRequest->m_bufRespHeaders.Data(),m_pRequest->m_bufRespHeaders.Count());
        pszTrav += m_pRequest->m_bufRespHeaders.Count();
        cchBufSpace -= m_pRequest->m_bufRespHeaders.Count();
    }

    if (pszExtraHeaders) 
    {
        hr = StringCchCopyExA(pszTrav,cchBufSpace,pszExtraHeaders,&pszTrav,&cchBufSpace,0);
        ASSERT(SUCCEEDED(hr));
        // It's the script's responsibility to include the final double CRLF, as per IIS.
    }
    else  
    {
        APPENDCRLF(pszTrav,cchBufSpace);    // the double CRLF signifies that header data is at an end
    }

    if (m_pszBody && (VERB_HEAD != m_pRequest->m_idMethod)) 
    {
        // A major HTTP client has a parsing bug where it will wrongly indicate
        // a parse error if the HTTP headers and body are sent with different send()
        // calls during a 401.  Work around this problem by sending
        // a body in this call if we have been provided with one at this stage.  Note that
        // this body will only be a default (eg from rgStatus) or a directory listing.
        // ISAPI extensions/ASP, sending raw files back, and WebDAV never set m_pszBody
        // but handle their own send() calls.
        hr = StringCchCopyExA(pszTrav,cchBufSpace,m_pszBody,&pszTrav,&cchBufSpace,0);
        ASSERT(SUCCEEDED(hr));
    }

    *pszTrav = 0;

    iLen = pszTrav - pszBuf;
    m_pRequest->SendData(pszBuf,iLen);

done:
    DEBUGMSG_ERR(ZONE_ERROR, (L"HTTPD: SendResponse failed, error = %d\r\n"));
    DEBUGMSG(ZONE_RESPONSE, (L"HTTPD: Sending RESPONSE HEADER<<%a>>\r\n", pszBuf));

    if (pszBuf != szBuf) 
    {
        MyFree(pszBuf);
    }
}


void SendFile(SOCKET sock, HANDLE hFile, CHttpRequest *pRequest) 
{
    BYTE bBuf[4096];
    DWORD dwRead;

    while(ReadFile(hFile, bBuf, sizeof(bBuf), &dwRead, 0) && dwRead) 
    {
        if (! pRequest->SendData((PSTR) bBuf, dwRead)) 
        {
            return;
        }
    }
}

//  This function used to display the message "Object moved
void CHttpResponse::SendRedirect(PCSTR pszRedirect, BOOL fFromFilter) 
{   
    HRESULT hr;
    
    if (!fFromFilter && m_pRequest->FilterNoResponse()) 
    {
        return;    
    }
        
    DEBUGCHK(!m_hFile);
    DEBUGCHK(m_pRequest->m_rs==STATUS_MOVED);
    DWORD cchLen = strlen(rgStatus[m_pRequest->m_rs].pszStatusBody)+strlen(pszRedirect)+30;
    // create a body
    PSTR pszBody = MySzAllocA(cchLen);
    
    if(pszBody) 
    {   
        hr = StringCchPrintfA(pszBody,cchLen,rgStatus[m_pRequest->m_rs].pszStatusBody, pszRedirect);
        ASSERT(SUCCEEDED(hr));
        SetBody(pszBody, cszTextHtml);
    }
    // set redirect header & send all headers, then the synthetic body
    m_pszRedirect = pszRedirect;
    SendHeadersAndDefaultBodyIfAvailable(NULL,NULL);
    MyFree(pszBody);
}

//  Read strings of the bodies to send on web server errors ("Object not found",
//  "Server Error",...) using load string.  These strings are in wide character
//  format, so we first convert them to regular strings, marching along
//  pszBodyCodes (which is a buffer size BODYSTRINGSIZE).  After the conversion,
//  we set each rgStatus[i].pszStatusBody to the pointer in the buffer.
void InitializeResponseCodes(PSTR pszStatusBodyBuf) 
{  
    PSTR pszTrav = pszStatusBodyBuf;
    UINT i;
    int iLen;
    WCHAR wszBuf[256];

    for(i = 0; i < ARRAYSIZEOF(rgStatus); i++) 
    {
        if (!rgStatus[i].fHasDefaultBody) 
        {
            continue;
        }

        if (0 == LoadString(g_hInst,RESBASE_body + i,wszBuf,ARRAYSIZEOF(wszBuf))) 
        {
            DEBUGMSG(ZONE_ERROR,(L"HTTPD: LoadString(%d) failed.  GLE=0x%08x\r\n",RESBASE_body + i,GetLastError()));
            rgStatus[i].pszStatusBody = NULL;
            continue;
        }

        iLen = MyW2A(wszBuf, pszTrav, BODYSTRINGSIZE - (pszTrav - pszStatusBodyBuf));
        if (!iLen) 
        {
            return;
        }

        rgStatus[i].pszStatusBody = pszTrav;
        pszTrav += iLen;
    }
}

BOOL CHttpRequest::SendData(PSTR pszBuf, DWORD dwLen, BOOL fCopyBuffer) 
{
    DEBUG_CODE_INIT;
    BOOL fRet = FALSE;
    PSTR pszFilterData = pszBuf;
    DWORD dwFilterData = dwLen;

    if (g_pVars->m_fFilters &&
        ! CallFilter(SF_NOTIFY_SEND_RAW_DATA, &pszFilterData, (int*)&dwFilterData))
    {
        return FALSE;
    }

    if (!m_fIsSecurePort) 
    {
        return (dwLen == (DWORD) send(m_socket,pszFilterData,dwFilterData,0));
    }
    
    // If the filter modified the data's pointer then we'll need to copy data, filter
    // is the owner of this buffer and we don't want to trounce it.  

    fCopyBuffer = fCopyBuffer || (pszBuf != pszFilterData);
    return SendEncryptedData(pszFilterData,dwFilterData,fCopyBuffer);
}

void CHttpRequest::GetContentTypeOfRequestURI(PSTR szContentType, DWORD ccContentType, const WCHAR *wszExt) 
{
    HRESULT hr;
    const WCHAR *wszContentType;
    CReg reg;

    szContentType[0] = 0;

    if (!wszExt) 
    {
        wszExt = m_wszExt;
    }

    if (!wszExt) 
    {
        goto done;
    }

    if (! reg.Open(HKEY_CLASSES_ROOT,wszExt)) 
    {
        goto done;
    }

    wszContentType = reg.ValueSZ(L"Content Type");

    if (wszContentType) 
    {
        if (0 == MyW2A(wszContentType, szContentType, ccContentType)) 
        {
            szContentType[0] = 0;
        }
    }

done:
    if(! szContentType[0]) 
    {
        hr = StringCchCopyA(szContentType,ccContentType,cszOctetStream);
        ASSERT(SUCCEEDED(hr));
    }
}
