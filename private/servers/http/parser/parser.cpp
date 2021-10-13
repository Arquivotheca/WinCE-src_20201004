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
Module Name: PARSER.CPP
Abstract: HTTP request parser
--*/

#include "httpd.h"

// Methods
// NOTE: This follows same order as "enum VERB" list in request.h.

const char *rg_cszMethods[] = 
{
    "GET",
    "HEAD",
    "POST",
    "OPTIONS",
    "PUT",
    "MOVE",
    "COPY",
    "DELETE",
    "MKCOL",
    "PROPFIND",
    "PROPPATCH",
//    "SEARCH",
    "LOCK",
    "UNLOCK",
#if 0 // unsupported DAV vebs
    "SUBSCRIBE",
    "UNSUBSCRIBE",
    "POLL",
    "BDELETE",
    "BCOPY",
    "BMOVE",
    "BPROPPATCH",
    "BPROPFIND",
    "X-MS-ENUMATTS",
#endif
};

const int cKnownMethods = ARRAYSIZEOF(rg_cszMethods);

// other Header tokens
const char cszHTTPVER[] = "HTTP/%d.%d";


// String constants

// const CHAR  cszAccept_CHAR set[]         = "Accept-CHAR set:";
// const CHAR  cszAccept_Encoding[]         = "Accept-Encoding:";
// const CHAR  cszAccept_Language[]         = "Accept-Language:";
const CHAR  cszAccept_Ranges[]              = "Accept-Ranges:";
const DWORD ccAccept_Ranges                 = SVSUTIL_CONSTSTRLEN(cszAccept_Ranges);
const CHAR  cszAccept[]                     = "Accept:";
const DWORD ccAccept                        = SVSUTIL_CONSTSTRLEN(cszAccept);
// const CHAR  cszAge[]                     = "Age:";
const CHAR  cszAllow[]                      = "Allow:";
const DWORD ccAllow                         = SVSUTIL_CONSTSTRLEN(cszAllow);
// const CHAR  cszAtomic[]                  = "Atomic:";
const CHAR  cszAuthorization[]              = "Authorization:";
const DWORD ccAuthorization                 = SVSUTIL_CONSTSTRLEN(cszAuthorization);
// const CHAR  cszCollection_Member[]       = "Collection-Member:";
// const CHAR  cszCompatibility[]           = "Compatibility:";
const CHAR  cszConnection[]                 = "Connection:";
const DWORD ccConnection                    = SVSUTIL_CONSTSTRLEN(cszConnection);
// const CHAR  cszContent_Base[]            = "Content-Base:";
// const CHAR  cszContent_Encoding[]        = "Content-Encoding:";
// const CHAR  cszContent_Language[]        = "Content-Language:";
const CHAR  cszContent_Length[]             = "Content-Length:";
const DWORD ccContent_Length                = SVSUTIL_CONSTSTRLEN(cszContent_Length);
const CHAR  cszContent_Location[]           = "Content-Location:";
const DWORD ccContent_Location              = SVSUTIL_CONSTSTRLEN(cszContent_Location);
// const CHAR  cszContent_MD5[]             = "Content-MD5:";
const CHAR  cszContent_Range[]              = "Content-Range:";
const DWORD ccContent_Range                 = SVSUTIL_CONSTSTRLEN(cszContent_Range);
const CHAR  cszContent_Type[]               = "Content-Type:";
const DWORD ccContent_Type                  = SVSUTIL_CONSTSTRLEN(cszContent_Type);
// const CHAR  cszContent_Disposition[]     = "Content-Disposition:";
const CHAR  cszCookie[]                     = "Cookie:";
const DWORD ccCookie                        = SVSUTIL_CONSTSTRLEN(cszCookie);
const CHAR  cszDate[]                       = "Date:";
const DWORD ccDate                          = SVSUTIL_CONSTSTRLEN(cszDate);
const CHAR  cszDepth[]                      = "Depth:";
const DWORD ccDepth                         = SVSUTIL_CONSTSTRLEN(cszDepth);
const CHAR  cszDestination[]                = "Destination:";
const DWORD ccDestination                   = SVSUTIL_CONSTSTRLEN(cszDestination);
// const CHAR  cszDestroy[]                 = "Destroy:";
// const CHAR  cszExpires[]                 = "Expires:";
const CHAR  cszETag[]                       = "ETag:";
const DWORD ccETag                          = SVSUTIL_CONSTSTRLEN(cszETag);
// const CHAR  cszFrom[]                    = "From:";
const CHAR  cszHost[]                       = "Host:";
const DWORD ccHost                          = SVSUTIL_CONSTSTRLEN(cszHost);
const CHAR  cszIfToken[]                    = "If:";
const DWORD ccIfToken                       = SVSUTIL_CONSTSTRLEN(cszIfToken);
const CHAR  cszIf_Match[]                   = "If-Match:";
const DWORD ccIf_Match                      = SVSUTIL_CONSTSTRLEN(cszIf_Match);
const CHAR  cszIf_Modified_Since[]          = "If-Modified-Since:";
const DWORD ccIf_Modified_Since             = SVSUTIL_CONSTSTRLEN(cszIf_Modified_Since);
const CHAR  cszIf_None_Match[]              = "If-None-Match:";
const DWORD ccIf_None_Match                 = SVSUTIL_CONSTSTRLEN(cszIf_None_Match);
const CHAR  cszIf_None_State_Match[]        = "If-None-State-Match:";
const DWORD ccIf_None_State_Match           = SVSUTIL_CONSTSTRLEN(cszIf_None_State_Match);
const CHAR  cszIf_Range[]                   = "If-Range:";
const DWORD ccIf_Range                      = SVSUTIL_CONSTSTRLEN(cszIf_Range);
const CHAR  cszIf_State_Match[]             = "If-State-Match:";
const DWORD ccIf_State_Match                = SVSUTIL_CONSTSTRLEN(cszIf_State_Match);
const CHAR  cszIf_Unmodified_Since[]        = "If-Unmodified-Since:";
const DWORD ccIf_Unmodified_Since           = SVSUTIL_CONSTSTRLEN(cszIf_Unmodified_Since);
// const CHAR  cszLast_Modified[]           = "Last-Modified:";
const CHAR  cszLocation[]                   = "Location:";
const DWORD ccLocation                      = SVSUTIL_CONSTSTRLEN(cszLocation);
const CHAR  cszLockInfo[]                   = "Lock-Info:";
const DWORD ccLockInfo                      = SVSUTIL_CONSTSTRLEN(cszLockInfo);
const CHAR  cszLockToken[]                  = "Lock-Token:";
const DWORD ccLockToken                     = SVSUTIL_CONSTSTRLEN(cszLockToken);
// const CHAR  cszMS_Author_Via[]           = "MS-Author-Via:";
// const CHAR  cszMS_Exchange_FlatURL[]     = "MS-Exchange-Permanent-URL:";
const CHAR  cszOverwrite[]                  = "Overwrite:";
const DWORD ccOverwrite                     = SVSUTIL_CONSTSTRLEN(cszOverwrite);
const CHAR  cszAllowRename[]                = "Allow-Rename:";
const DWORD ccAllowRename                   = SVSUTIL_CONSTSTRLEN(cszAllowRename);
const CHAR  cszPublic[]                     = "Public:";
const DWORD ccPublic                        = SVSUTIL_CONSTSTRLEN(cszPublic);
const CHAR  cszRange[]                      = "Range:";
const DWORD ccRange                         = SVSUTIL_CONSTSTRLEN(cszRange);
const CHAR  cszReferer[]                    = "Referer:";
const DWORD ccReferer                       = SVSUTIL_CONSTSTRLEN(cszReferer);
// const CHAR  cszRetry_After[]             = "Retry-After:";
const CHAR  cszServer[]                     = "Server:";
const DWORD ccServer                        = SVSUTIL_CONSTSTRLEN(cszServer);
// const CHAR  cszSet_Cookie[]              = "Set-Cookie:";
const CHAR  cszTimeOut[]                    = "Timeout:";
const DWORD ccTimeOut                       = SVSUTIL_CONSTSTRLEN(cszTimeOut);
const CHAR  cszTransfer_Encoding[]          = "Transfer-Encoding:";
const DWORD ccTransfer_Encoding             = SVSUTIL_CONSTSTRLEN(cszTransfer_Encoding);
const CHAR  cszTranslate[]                  = "Translate:";
const DWORD ccTranslate                     = SVSUTIL_CONSTSTRLEN(cszTranslate);
// const CHAR  cszUpdate[]                  = "Update:";
// const CHAR  cszUser_Agent[]              = "User-Agent:";
// const CHAR  cszVary[]                    = "Vary:";
// const CHAR  cszWarning[]                 = "Warning:";
const CHAR  cszWWW_Authenticate[]           = "WWW-Authenticate:";
const DWORD ccWWW_Authenticate              = SVSUTIL_CONSTSTRLEN(cszWWW_Authenticate);
// const CHAR  cszVersioning_Support[]      = "Versioning-Support:";
// const CHAR  cszBrief[]                   = "Brief:";

//
//    Common header values
//
const CHAR cszDateFmtGMT[]                 = "%3s, %02d %3s %04d %02d:%02d:%02d GMT";
const CHAR* rgWkday[]                      = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", 0 };
const CHAR* rgMonth[]                      = { "", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", 0 };

const CHAR  cszNone[]                      = "none";
const DWORD ccNone                         = SVSUTIL_CONSTSTRLEN(cszNone);
const CHAR  cszChunked[]                   = "chunked";
const DWORD ccChunked                      = SVSUTIL_CONSTSTRLEN(cszChunked);

const CHAR  cszBasic[]                     = "Basic";
const DWORD ccBasic                        = SVSUTIL_CONSTSTRLEN(cszBasic);
const CHAR  cszNTLM[]                      = "NTLM";
const DWORD ccNTLM                         = SVSUTIL_CONSTSTRLEN(cszNTLM);
const CHAR  cszNegotiate[]                 = "Negotiate";
const DWORD ccNegotiate                    = SVSUTIL_CONSTSTRLEN(cszNegotiate);

const CHAR  cszBytes[]                     = "bytes";
const DWORD ccBytes                        = SVSUTIL_CONSTSTRLEN(cszBytes);
const CHAR  cszAnd[]                       = "and";
const DWORD ccAnd                          = SVSUTIL_CONSTSTRLEN(cszAnd);
const CHAR  cszOr[]                        = "or";
const DWORD ccOr                           = SVSUTIL_CONSTSTRLEN(cszOr);
const CHAR  cszMS_Author_Via_Dav[]         = "DAV";
const DWORD ccMS_Author_Via_Dav            = SVSUTIL_CONSTSTRLEN(cszMS_Author_Via_Dav);


const CHAR  cszHttpPrefix[]                = "http://";
const DWORD ccHttpPrefix                   = SVSUTIL_CONSTSTRLEN(cszHttpPrefix);
const CHAR  cszHttpsPrefix[]               = "https://";
const DWORD ccHttpsPrefix                  = SVSUTIL_CONSTSTRLEN(cszHttpsPrefix);


const CHAR  cszRespStatus[]                = "HTTP/1.%d %d %s\r\n";
const DWORD ccRespStatus                   = SVSUTIL_CONSTSTRLEN(cszRespStatus);
const CHAR  cszRespServer[]                = "Server: %s\r\n";
const DWORD ccRespServer                   = SVSUTIL_CONSTSTRLEN(cszRespServer);
const CHAR  cszRespType[]                  = "Content-Type: %s\r\n";
const DWORD ccRespType                     = SVSUTIL_CONSTSTRLEN(cszRespType);
const CHAR  cszRespLength[]                = "Content-Length: %d\r\n";
const DWORD ccRespLength                   = SVSUTIL_CONSTSTRLEN(cszRespLength);

const CHAR  cszRespDate[]                  = "Date: ";
const DWORD ccRespDate                     = SVSUTIL_CONSTSTRLEN(cszRespDate);
const CHAR  cszRespLocation[]              = "Location: ";
const DWORD ccRespLocation                 = SVSUTIL_CONSTSTRLEN(cszRespLocation);

const CHAR  cszConnectionResp[]            = "Connection: %s\r\n";
const DWORD ccConnectionResp               = SVSUTIL_CONSTSTRLEN(cszConnectionResp);
const CHAR  cszConnClose[]                 = "close";
const DWORD ccConnClose                    = SVSUTIL_CONSTSTRLEN(cszConnClose);
const CHAR  cszKeepAlive[]                 = "keep-alive";
const DWORD ccKeepAlive                    = SVSUTIL_CONSTSTRLEN(cszKeepAlive);
const CHAR  cszCRLF[]                      = "\r\n";
const DWORD ccCRLF                         = SVSUTIL_CONSTSTRLEN(cszCRLF);
const CHAR  cszRespLastMod[]               = "Last-Modified:";
const DWORD ccRespLastMod                  = SVSUTIL_CONSTSTRLEN(cszRespLastMod);


const CHAR  cszOpaquelocktokenPrefix[]     = "opaquelocktoken:";
const DWORD ccOpaquelocktokenPrefix        = SVSUTIL_CONSTSTRLEN(cszOpaquelocktokenPrefix);


#define PFNPARSE(x)    &(CHttpRequest::Parse##x)
#define TABLEENTRY(csz, id, pfn)           { csz, sizeof(csz)-1, id, PFNPARSE(pfn) }

typedef (CHttpRequest::*PFNPARSEPROC)(PCSTR pszTok, TOKEN idHeader);

typedef struct tagHeaderDesc 
{
    const char*        sz;
    int                iLen;
    TOKEN            id;
    PFNPARSEPROC    pfn;
} HEADERDESC;


const HEADERDESC rgHeaders[] =
{
// General headers
    TABLEENTRY(cszConnection, TOK_CONNECTION, Connection),
    //TABLEENTRY(cszDate,      TOK_DATE,   Date),
    //TABLEENTRY(cszPragma, TOK_PRAGMA, Pragma),
// Request headers
    TABLEENTRY(cszCookie, TOK_COOKIE, Cookie),
    TABLEENTRY(cszAccept, TOK_ACCEPT, Accept),
    //TABLEENTRY(cszReferer,  TOK_REFERER Referer),
    //TABLEENTRY(cszUserAgent,TOK_UAGENT, UserAgent),
    TABLEENTRY(cszAuthorization, TOK_AUTH,  Authorization),
    TABLEENTRY(cszIf_Modified_Since, TOK_IFMOD, IfModifiedSince),
// Entity Headers
    //TABLEENTRY(cszContentEncoding, TOK_ENCODING Encoding),
    TABLEENTRY(cszContent_Type, TOK_TYPE,     ContentType),
    TABLEENTRY(cszContent_Length, TOK_LENGTH,     ContentLength),
    TABLEENTRY(cszHost, TOK_HOST,    Host),
    { 0, 0, (TOKEN)0, 0}
};



// Parse all the headers, line by line
BOOL CHttpRequest::ParseHeaders() 
{   
    DEBUG_CODE_INIT;
    PSTR pszTok;
    PWSTR pwszTemp;
    PSTR pszPathInfo = NULL;
    int i, iLen;
    BOOL ret = FALSE;

    if (!m_bufRequest.NextTokenWS(&pszTok, &iLen)) 
    {
        m_rs = STATUS_BADREQ;
        myleave(287);
    }

    if (! ParseMethod(pszTok,iLen)) 
    {
        m_rs = STATUS_BADREQ;
        myleave(288);
    }

    if (!m_bufRequest.NextLine()) 
    {
        m_rs = STATUS_BADREQ;
        myleave(290);
    }
    
    // outer-loop. one header per iteration
    while (m_bufRequest.NextTokenHeaderName(&pszTok, &iLen)) 
    {
        // compare token with tokens in table
        for (i=0; rgHeaders[i].sz; i++)     
        {
            //DEBUGMSG(ZONE_PARSER, (L"HTTPD: Comparing %a %d %d\r\n", rgHeaders[i].sz, rgHeaders[i].iLen, rgHeaders[i].pfn));
            if ( ((int)rgHeaders[i].iLen == iLen) &&
                0==_memicmp(rgHeaders[i].sz, pszTok, iLen) )
            {
                    break;
            }
        }
        if (rgHeaders[i].pfn) 
        {
            DEBUGMSG(ZONE_PARSER, (L"HTTPD: Parsing %a\r\n", rgHeaders[i].sz));
            // call the specific function to parse this header. 
            if (! ((this->*(rgHeaders[i].pfn))(pszTok, rgHeaders[i].id)) ) 
            {
                DEBUGMSG(ZONE_PARSER, (L"HTTPD: Parser: failed to parse %a\r\n", rgHeaders[i].sz));

                // If none of the Parse functions set last error, set to bad req
                if (m_rs == STATUS_OK)
                {
                    m_rs = STATUS_BADREQ;
                }

                myleave(296);
            }
        }
        else 
        {
            DEBUGMSG(ZONE_PARSER, (L"HTTPD: Ignoring header %a\r\n", pszTok));
        }
        if (!m_bufRequest.NextLine()) 
        {
            m_rs = STATUS_BADREQ;
            myleave(290);
        }
    }

    if (!m_bufRequest.NextLine()) 
    { // eat the blank line
        m_rs = STATUS_BADREQ;
        myleave(295);
    }
    DEBUGMSG(ZONE_PARSER, (L"HTTPD: Parser: DONE\r\n"));

    // check what we got 
    if (!m_pszMethod) 
    {
        DEBUGMSG(ZONE_PARSER, (L"HTTPD: Parser: missing URL or method, illformatted Request-line\r\n"));
        m_rs = STATUS_BADREQ;
        myleave(291);
    }

    // Once we've read the request line, give filter shot at modifying the
    // remaining headers.
    if (g_pVars->m_fFilters && ! CallFilter(SF_NOTIFY_PREPROC_HEADERS))
    {
        myleave(292);
    }

    if (!m_fResolvedHostWebSite) 
    {
        // Now that we have Host: (if we'll get it) map website on first request.
        m_pWebsite = MapWebsite(m_socket,m_pszHost,&m_pVrootTable);

        if (!m_pWebsite) 
        {
            m_pWebsite = g_pVars; // reset for when we write out log/filters.
            DEBUGMSG(ZONE_ERROR,(L"HTTPD: Unable to map website on socket %x\r\n",m_socket));
            m_rs = STATUS_NOTFOUND;
            myleave(297);
        }
        m_fResolvedHostWebSite = TRUE;
    }

    m_wszPath = m_pVrootTable->URLAtoPathW(m_pszURL, &m_pVRoot, &m_pszPathInfo,TRUE);

    if (g_pVars->m_fFilters && ! CallFilter(SF_NOTIFY_URL_MAP))
    {
        myleave(293);
    }

    // get extension
    if (m_wszPath && (pwszTemp = wcsrchr(m_wszPath, '.')))
    {
        m_wszExt = MySzDupW(pwszTemp);
    }

    if (m_wszPath)
    {
        m_ccWPath = wcslen(m_wszPath);
    }

    // Is the request chunked?
    CHAR *szHeader = FindHttpHeader(cszTransfer_Encoding,ccTransfer_Encoding);
    if (szHeader && (0 == _strnicmp(szHeader,cszChunked,ccChunked))) 
    {
        m_fIsChunkedUpload = TRUE;
        // Per spec, ignore content-length if Chunked upload ever set
        m_dwContentLength = 0; 
    }

    ret = TRUE;
done:
    DEBUGMSG_ERR(ZONE_PARSER,(L"HTTPD: Parse headers failed, err = %d\r\n",err));
    return ret;
}

VERB FindMethod(PCSTR pszMethod, int cbMethod) 
{
    for (int i = 0; i < cKnownMethods; i++) 
    {
        if (0 == memcmp(pszMethod,rg_cszMethods[i],cbMethod)) 
        {
            DEBUGCHK( (VERB_UNKNOWN+i+1) <= VERB_LAST_VALID);
            return (VERB) (VERB_UNKNOWN+i+1);
        }
    }
    return VERB_UNKNOWN;
}

BOOL CHttpRequest::ParseMethod(PCSTR pszMethod, int cbMethod) 
{
    DEBUG_CODE_INIT;
    PSTR pszTok, pszTok2;
    int iLen;
    BOOL ret = FALSE;

    // save method
    m_pszMethod = MySzDupA(pszMethod);
    m_idMethod = FindMethod(pszMethod,cbMethod);

    // get URL and HTTP/x.y together (allows for spaces in URL)
    if (!m_bufRequest.NextTokenEOL(&pszTok, &iLen))
    {
        myretleave(FALSE, 201);
    }

    // separate out the HTTP/x.y
    if (pszTok2 = strrchr(pszTok, ' ')) 
    {
        *pszTok2 = 0;
        iLen = pszTok2-pszTok;
        pszTok2++;
    }
    
    // clean up & parse the URL
    if (! MyCrackURL(pszTok, iLen))
    {
        goto done;
    }
    
    // get version 
    if (!pszTok2)
    {
        goto done;
    }

    int iMajor, iMinor;
    if (2 != sscanf(pszTok2, cszHTTPVER, &iMajor, &iMinor)) 
    {
        DEBUGMSG(ZONE_ERROR,(L"HTTPD: unable to parse HTTP version from request\r\n"));
        pszTok2[-1] = ' ';    // reset this to a space
        myretleave(FALSE,202);
    }

    // Set the HTTP version
    SetHTTPVersion(iMinor, iMajor);
        
    pszTok2[-1] = ' ';    // reset this to a space
    ret = TRUE;

done:
    DEBUGMSG_ERR(ZONE_PARSER, (L"HTTPD: end ParseMethod (iGLE=%d iErr=%d)\r\n", GLE(err),err));
    return ret;
}

// We assume a raw URL in the form that we receive in the HTTP headers (no scheme, port number etc)
// We extract the path, extra-path, and query
BOOL CHttpRequest::MyCrackURL(PSTR pszRawURL, int iLen) 
{
    DEBUG_CODE_INIT;
    BOOL ret = FALSE;
    PSTR  pszDecodedURL=0, pszTemp=0;
    int iLen2;
    char *pUPnP;

    // special case handeling of 'OPTIONS *' case.
    if (*pszRawURL == '*' && *(pszRawURL+1) == 0 && (m_idMethod == VERB_OPTIONS)) 
    {
        m_fOptionsAsterisk = TRUE;
        m_pszURL = MySzDupA("*");
        return TRUE;
    }

    // In all other cases, HTTPD and a number of ISAPIs assume that the path
    // begins with a '/'.  If it doesn't reject request now.
    if (*pszRawURL != '/') 
    {
        //if this is an absolute URL request for UPnP, we don't fail.
        if(pUPnP = strstr(pszRawURL, "/upnp"))
        {
            pszRawURL = pUPnP;
        }
        else
        {
           m_rs = STATUS_BADREQ;
           goto done;
        }
    }

    // decode URL (convert escape sequences etc)
    if (NULL == (pszDecodedURL = MyRgAllocNZ(CHAR, iLen+1)))
    {
        myleave(382);
    }

    // Use private Internet canonicalization to avoid wininet dependency.

    // There's no memory allocation in our version of this fcn, always succeeds.
    // The size needed to hold the unencoded URL will always be less than or
    // equal to the original URL.
    svsutil_HttpCanonicalizeUrlA(pszRawURL, pszDecodedURL);

    // get query string
    if (pszTemp = strchr(pszDecodedURL, '?')) 
    {
        if (NULL == (m_pszQueryString = MySzDupA(pszTemp+1)))
        {
            goto done;
        }
        *pszTemp = 0;
    }

    // Searching for an embedded ISAPI dll name, ie /wwww/isapi.dll/a/b.
    // We load the file /www/isapi.dll and set PATH_INFO to /a/b
    // Emebbed ASP file names are handled similiarly.
    if (g_pVars->m_fExtensions) 
    {
        if (pszTemp = strstr(pszDecodedURL,".dll/")) 
        {
            m_pszPathInfo = MySzDupA(pszTemp + sizeof(".dll/") - 2);
            pszTemp[sizeof(".dll/") - 2] = 0;
        }
        else if (pszTemp = strstr(pszDecodedURL,".asp/")) 
        {
            m_pszPathInfo = MySzDupA(pszTemp + sizeof(".asp/") - 2);
            pszTemp[sizeof(".asp/") - 2] = 0;
        }
    }
    
    // save a copy of the cleaned up URL (MINUS query!)
    // alloc one extra char in case we have to send a redirect back (see request.cpp)
    iLen2 = strlen(pszDecodedURL);
    if (NULL == (m_pszURL = MySzAllocA(1+iLen2)))
    {
        goto done;
    }

    Nstrcpy(m_pszURL, pszDecodedURL, iLen2); // copy null-term too.
        
    ret = TRUE;
done:
    MyFree(pszDecodedURL);  
    DEBUGMSG_ERR(ZONE_PARSER, (L"HTTPD: end MyCrackURL(%a) path=%s ext=%s query=%a (iGLE=%d iErr=%d)\r\n", 
        pszRawURL, m_wszPath, m_wszExt, m_pszQueryString, GLE(err), err));
    
    return ret;

}

BOOL CHttpRequest::ParseContentLength(PCSTR pszMethod, TOKEN id) 
{
    PSTR pszTok = 0;
    int iLen = 0;

    if (m_dwContentLength) 
    {
        DEBUGMSG(ZONE_PARSER,(L"HTTPD: Error: Duplicate Content-length headers in request\r\n"));
        return FALSE;
    }
    
    // get length (first token after "Content-Type;")
    if (m_bufRequest.NextTokenEOL(&pszTok, &iLen) && pszTok && iLen) 
    {
        m_dwContentLength = atoi(pszTok);
        if ((int)m_dwContentLength < 0) 
        {
            DEBUGMSG(ZONE_ERROR,(L"HTTPD: Error: Content-Length = %d, negative.  Rejecting the request\r\n",(int)m_dwContentLength));
            return FALSE;
        }
        
        return TRUE;
    }

    return FALSE;
}

BOOL CHttpRequest::ParseCookie(PCSTR pszMethod, TOKEN id) 
{
    PSTR pszTok = 0;
    int iLen = 0;

    if (m_pszCookie) 
    {
        DEBUGMSG(ZONE_PARSER,(L"HTTPD: Error: Duplicate Cookie headers in request\r\n"));
        return FALSE;
    }
    
    // get cookie (upto \r\n after "Cookies;")
    if (m_bufRequest.NextTokenEOL(&pszTok, &iLen) && pszTok && iLen) 
    {
        if (NULL == (m_pszCookie = MySzDupA(pszTok)))
        {
            return FALSE;
        }
        return TRUE;
    }
    return FALSE;
}

BOOL CHttpRequest::ParseAccept(PCSTR pszMethod, TOKEN id) 
{
    PSTR pszTok = 0;
    int iLen = 0;

    if (m_pszAccept) 
    {
        DEBUGMSG(ZONE_PARSER,(L"HTTPD: Error: Duplicate accept headers in request\r\n"));
        return FALSE;
    }
    
    // get cookie (upto \r\n after "Cookies;")
    if (m_bufRequest.NextTokenEOL(&pszTok, &iLen) && pszTok && iLen) 
    {
        if (NULL == (m_pszAccept = MySzDupA(pszTok)))
        {
            return FALSE;
        }
        return TRUE;
    }
    return FALSE;
}


BOOL CHttpRequest::ParseContentType(PCSTR pszMethod, TOKEN id) 
{
    PSTR pszTok = 0;
    int iLen = 0;

    if (m_pszContentType) 
    {
        DEBUGMSG(ZONE_PARSER,(L"HTTPD: Error: Duplicate content-type headers in request\r\n"));
        return FALSE;
    }

    // get type (first token after "Content-Type;")
    if (m_bufRequest.NextTokenEOL(&pszTok, &iLen) && pszTok && iLen) 
    {
        if (NULL == (m_pszContentType = MySzDupA(pszTok)))
        {
            return FALSE;
        }
        return TRUE;
    }
    return FALSE;
}

const char cszDateParseFmt[] = " %*3s, %02hd %3s %04hd %02hd:%02hd:%02hd GMT; length=%d";

BOOL CHttpRequest::ParseIfModifiedSince(PCSTR pszMethod, TOKEN id) 
{
    PSTR pszTok = 0;
    int iLen = 0;
    int i = 0;
    char szMonth[10];
    SYSTEMTIME st;
    ZEROMEM(&st);

    if (m_ftIfModifiedSince.dwLowDateTime || m_ftIfModifiedSince.dwHighDateTime) 
    {
        DEBUGMSG(ZONE_PARSER,(L"HTTPD:  Error: Duplicate if-modified-since headers in request\r\n"));
        return FALSE;
    }
    
    // get the date (rest of line after If-Modified-Since)
    // NOTE: Note we are handling only one date format (the "reccomended" one)
    if (m_bufRequest.NextTokenEOL(&pszTok, &iLen) && pszTok && iLen)     
    {
        i = sscanf(pszTok, cszDateParseFmt, &st.wDay, szMonth, &st.wYear, &st.wHour, &st.wMinute, &st.wSecond, &m_dwIfModifiedLength);
        if ( i >= 6) 
        {
            // try to match month
            for (i=0; rgMonth[i]; i++) 
            {
                if (0==strcmpi(szMonth, rgMonth[i]))  
                {
                    st.wMonth = i;
                    // convert to filetime & store
                    SystemTimeToFileTime(&st, &m_ftIfModifiedSince);
                    return TRUE;
                }
            }
        }
        DEBUGMSG(ZONE_ERROR, (L"HTTPD: Failed to parse If-Modified-Since(%a) Parsed: day=%02d month=%a(%d) year=%04d time=%02d:%02d:%02d len=%d\r\n", 
            pszTok, st.wDay, szMonth, i, st.wYear, st.wHour, st.wMinute, st.wSecond, m_dwIfModifiedLength));
    }
    return FALSE;
}

BOOL CHttpRequest::ParseAuthorization(PCSTR pszMethod, TOKEN id) 
{
    DEBUG_CODE_INIT;
    BOOL ret = FALSE;
    PSTR pszTok=0;
    int iLen=0;

    if (m_pszAuthType || m_pszRawRemoteUser) 
    {
        DEBUGMSG(ZONE_PARSER,(L"HTTPD: Error: Duplicate Authorization headers in request\r\n"));
        myretleave(FALSE,99);
    }

    // get the auth scheme (first token after "Authorization;")
    if (!m_bufRequest.NextTokenWS(&pszTok, &iLen) || !pszTok || !iLen)
    {
        myretleave(FALSE, 91);
    }

    if (NULL == (m_pszAuthType = MySzDupA(pszTok)))
    {
        myretleave(FALSE,100);
    }

    // Read in the authentication paramaters and store for after we do vroot parsing.
    if (!m_bufRequest.NextTokenWS(&pszTok, &iLen) || !pszTok || !iLen)
    {
        myretleave(FALSE, 97);
    }

    if (NULL == (m_pszRawRemoteUser = MySzDupA(pszTok)))
    {
        myretleave(FALSE, 98);
    }
    
    ret = TRUE;
done:
    DEBUGMSG_ERR(ZONE_ERROR, (L"HTTPD: Auth FAILED (err=%d ret=%d)\r\n", err, ret));
    return ret; 
}

BOOL CHttpRequest::ParseConnection(PCSTR pszMethod, TOKEN id) 
{
    PSTR pszTok = 0;
    int iLen = 0;
    
    // get first token after "Connnection:"
    if (m_bufRequest.NextTokenEOL(&pszTok, &iLen) && pszTok && iLen) 
    {
        if (0==strcmpi(pszTok, cszKeepAlive))
        {
            SetConnKeepAlive();
        }
        else if (0==strcmpi(pszTok, cszConnClose))
        {
            SetConnClose();
        }
        return TRUE;
    }
    return FALSE;
}

BOOL CHttpRequest::ParseHost(PCSTR pszMethod, TOKEN id) 
{
    PSTR pszTok = 0;
    int iLen = 0;

    if (m_pszHost) 
    {
        DEBUGMSG(ZONE_PARSER,(L"HTTPD:  Error: Duplicate host header in request\r\n"));
        return FALSE;
    }

    if (!m_bufRequest.NextTokenEOL(&pszTok, &iLen))
    {
        return FALSE;
    }

    if (NULL == (m_pszHost = MySzDupA(pszTok)))
    {
        return FALSE;
    }

    return TRUE;
}

//
// CChunkParser parses chunked data during uploads.
//
class CChunkParser 
{
private:
    // Beginning of the buffer to start parsing.  This will usually contain
    // the trailing \r\n from the previous chunk (except on first read). 
    // Do not free this pointer, object doesn't own the memory.
    BYTE *m_pChunk;
    // Whether the trailer \r\n is present in this chunk
    BOOL m_fReadTrailerCRLF;
    // Size of buffer read in
    DWORD m_dwBufferSize;
    // Next bytes to parse out
    DWORD m_dwNextOut;
    // Maximum chunk-extension we'll read in.  This is to mitigate DoS type attacks
    // where malicious client sends a huge chunk extension just to have HTTPD sit
    // and parse it forever.
    static const DWORD m_dwMaxChunkExtension = 5000;
    // Pointer to calling request object (in case we need to read additional bytes)
    CHttpRequest *m_pRequest;
    // CBuffer and socket are only valid for ParseInitialChunkedUpload().
    CBuffer      *m_pBuffer;
    SOCKET        m_socket;

    // Retrieve the next byte to parse.
    BOOL GetNextChunkedChar(CHAR *pc);
    // Check is next character what we expect?
    BOOL CheckNextByte(CHAR cExpectedByte, BOOL *pfReadError=NULL);

public:
    CChunkParser(CHttpRequest *pRequest, CBuffer *pBuffer, SOCKET sock) 
    {
        m_pRequest         = pRequest;
        m_fReadTrailerCRLF = FALSE;
        m_pBuffer          = pBuffer;
        m_socket           = sock;
    }

    // Used for ISAPI ReadClient
    CChunkParser(CHttpRequest *pRequest) 
    {
        m_pRequest         = pRequest;
        m_fReadTrailerCRLF = TRUE;
        m_pBuffer          = NULL;
        m_socket           = INVALID_SOCKET;
    }
    
    // Needs to be called multiple times per read
    void SetChunkHeader(BYTE *pChunk, DWORD dwBufferSize) 
    {
        m_pChunk       = pChunk;
        m_dwBufferSize = dwBufferSize;
        m_dwNextOut    = 0;
    }

    // Parse out the chunk headers
    BOOL ParseChunkHeader(DWORD *pdwChunkHeaderBytesToSwallow, DWORD *pdwChunkSize);
};


// Retrieve the next byte to parse.  Either use the buffer read in already, or else
// read in one more byte from the network.
BOOL CChunkParser::GetNextChunkedChar(CHAR *pc) 
{
    if (m_dwNextOut < m_dwBufferSize) 
    {
        // The next byte has been read in already.
        *pc = m_pChunk[m_dwNextOut];
        m_dwNextOut++;
        return TRUE;
    }

    BOOL fRet;

    // Otherwise we need to read in the next byte now.  Using m_pBuffer directly
    // versus ReadClientNonChunked is the result of the way buffer is implemented in HTTPD 
    // and particular the processing of the first bytes of POST on an SSL request.
    // Fixing HTTP core buffer to not require extra step is too involved at present.

    if (! m_pBuffer) 
    {
        DEBUGCHK((m_pBuffer==NULL) && (m_socket == INVALID_SOCKET));
        DWORD dwToRead = 1;
        fRet = m_pRequest->ReadClientNonChunked((PVOID)pc,&dwToRead);
    }
    else 
    {
        DEBUGCHK(m_pChunk==((BYTE*)m_pBuffer->m_pszBuf+m_pBuffer->m_iNextUnparsedChunk));
        HRINPUT hi = m_pBuffer->RecvBody(m_socket,1,TRUE,FALSE,m_pRequest,FALSE);
        if (hi != S_OK)
        {
            return FALSE;
        }

        // Adjust pointers and lengths, which may have changed during the read
        m_pChunk = (BYTE *)m_pBuffer->m_pszBuf + m_pBuffer->m_iNextUnparsedChunk;
        m_dwBufferSize = m_pBuffer->m_iNextIn - m_pBuffer->m_iNextUnparsedChunk;

        *pc = m_pChunk[m_dwNextOut];
        m_dwNextOut++;

        fRet = TRUE;
    }

    return fRet;
}

// Get next byte and make sure it's what we expect it to be
BOOL CChunkParser::CheckNextByte(CHAR cExpectedByte, BOOL *pfReadError) 
{
    CHAR c;
    if (! GetNextChunkedChar(&c)) 
    {
        if (pfReadError)
        {
            *pfReadError = TRUE;
        }
        return FALSE;
    }

    if (pfReadError)
    {
        *pfReadError = FALSE;
    }

    if (c !=  cExpectedByte) 
    {
        // Even though this isn't most useful error, we set it anyway so that 
        // calling ISAPI extension doesn't get confused by what may 
        // already be in lastError value.
        SetLastError(ERROR_INTERNAL_ERROR);
        return FALSE;
    }

    return TRUE;
}

// Parse out the chunk header, returning the size of the chunk itself
// as well as the chunk body size.  This function will always parse out an entire chunk header,
// calling recv() itself if needed to get additional chunk data in the event that the 
// entire chunk header wasn't read in the previous recv().
BOOL CChunkParser::ParseChunkHeader(DWORD *pdwChunkHeaderBytesToSwallow, DWORD *pdwChunkSize) 
{
    DEBUGCHK(m_pChunk && (m_dwNextOut == 0));

    CHAR c1;
    DWORD dwChunkSize = 0;
    DWORD dwChunkExtensionSize = 0;

    //
    //  Swallow the CRLF from the preceeding chunk.
    //
    if (m_fReadTrailerCRLF) 
    {
        if (! CheckNextByte('\r') || !CheckNextByte('\n')) 
        {
            DEBUGMSG(ZONE_ERROR,(L"HTTPD: ERROR: Unable to read trailing CRLF for chunked header\r\n"));
            return FALSE;
        }
    }
    else 
    {
        // Always read this after the first chunk is processed.
        m_fReadTrailerCRLF = TRUE;
    }

    // 
    //  Read in the length of the new chunk.  Length is encoded in base 16, will
    //  be terminated by a CRLF or by a ';' for the optional chunk-extension field
    //
    while (1) 
    {
        DWORD dwDigit;
        if (! GetNextChunkedChar(&c1)) 
        {
            DEBUGMSG(ZONE_ERROR,(L"HTTPD: ERROR: Unable to read number of bytes in chunk\r\n"));
            return FALSE;
        }

        if (isdigit(c1))
        {
            dwDigit = c1 - '0';
        }
        else if ((c1 >= 'A') && (c1 <= 'F'))
        {
            dwDigit = c1 - 'A' + 10;
        }
        else if ((c1 >= 'a') && (c1 <= 'f'))
        {
            dwDigit = c1 - 'a' + 10;
        }
        else if (c1 == ';')
        {
            break; // optional extension
        }
        else if (c1 == '\r')
        {
            break; // closing CRLF is beginning
        }
        else 
        {
            DEBUGMSG(ZONE_ERROR,(L"HTTPD: ERROR: Chunked upload failed because non-hex digit/; specified in number\r\n"));
            SetLastError(ERROR_INTERNAL_ERROR);
            return FALSE;
        }

        // dwChunkSize = dwChunkSize*16 + dwDigit
        if (FAILED(ULongMult(dwChunkSize,16,&dwChunkSize)) || FAILED(ULongAdd(dwChunkSize,dwDigit,&dwChunkSize)))  
        {
            DEBUGMSG(ZONE_ERROR,(L"HTTPD: ERROR: Chunked upload failed because of integer overflow on chunked length\r\n"));
            SetLastError(ERROR_INTERNAL_ERROR);
            return FALSE;            
        }
    }

    //
    //  Read the optional chunk-extension.  This begins with a ';' and is terminated
    //  by a CRLF.  We just swallow chunk-extension.
    //
    if (c1 == ';') 
    {
        while (dwChunkExtensionSize < m_dwMaxChunkExtension) 
        {
            if (! GetNextChunkedChar(&c1)) 
            {
                DEBUGMSG(ZONE_ERROR,(L"HTTPD: ERROR: Unable to read a byte from chunked-extension\r\n"));
                return FALSE;
            }
            dwChunkExtensionSize++;

            if (c1 == '\r') 
            {
                // Check for '\n'.  It is not required at this stage, spec allows
                // for \r without \n to be part of chunk-extension.
                BOOL fReadError;

                if (CheckNextByte('\n',&fReadError))
                {
                    break; // We're done reading.
                }

                if (fReadError) 
                {
                    DEBUGMSG(ZONE_ERROR,(L"HTTPD: ERROR: Unable to read a byte from chunked-extension\r\n"));
                    return FALSE;
                }

                dwChunkExtensionSize++;
            }
        }

        if (dwChunkExtensionSize >= m_dwMaxChunkExtension) 
        {
            DEBUGMSG(ZONE_ERROR,(L"HTTPD: ERROR: Chunk-Extension has exceeded maximum size (%d) bytes.  Ending request\r\n",m_dwMaxChunkExtension));
            SetLastError(ERROR_INTERNAL_ERROR);
            return FALSE;
        }
    }
    else 
    {
        // In non-chunk extension case, just read closing LF
        DEBUGCHK(c1 == '\r');

        if (! CheckNextByte('\n')) 
        {
            DEBUGMSG(ZONE_ERROR,(L"HTTPD: Could not read closing '\\n' in chunked header\r\n"));
            return FALSE;
        }     
    }

    //
    //  Indicate to caller how many bytes are in chunk and how many bytes of its buffer
    //  we've read.  If we've had to call recv() because buffer caller passed wasn't
    //  big enough, we still only return size of its buffer (rather than additional recv()
    //  bytes) because caller only uses this to figure out how much of buffer to throw-away

    *pdwChunkSize                 = dwChunkSize;
    *pdwChunkHeaderBytesToSwallow = m_dwNextOut;

    return TRUE;
}

// When client has sent HTTPD a chunked-upload, parse through any initial POST
// data and validated chunked headers and strip them out so ISAPI doesn't have to.
BOOL CBuffer::ParseInitialChunkedUpload(CHttpRequest *pRequest, SOCKET sock, DWORD *pdwBytesInNextChunk) 
{
    DEBUGCHK((*pdwBytesInNextChunk) == 0);
    DEBUGCHK(m_iNextUnparsedChunk == 0);

    DWORD dwRemaining; // Amount of POST data that remains to be processed.
    DWORD dwBytesInNextChunk;
    HRINPUT hi;

    DWORD dwChunkHeaderSwallow; // # of bytes in current chunked header, to ignore

    CChunkParser chunkParser(pRequest,this,sock);

    m_iNextUnparsedChunk = m_iNextOut;

    while (1) 
    {
        // 
        // Parse the chunked header, and then remove the chunk header itself
        // from the stream via memmove
        // 
        BufferConsistencyChecks();

        dwRemaining = m_iNextIn - m_iNextUnparsedChunk;
        chunkParser.SetChunkHeader(GetNextChunkToParse(), dwRemaining);
            
        if (! chunkParser.ParseChunkHeader(&dwChunkHeaderSwallow,&dwBytesInNextChunk))
        {
            return FALSE;
        }

        // Recalculate dwRemaining because m_iNextIn may have been incremented on a read.
        dwRemaining = m_iNextIn - m_iNextUnparsedChunk;

        DEBUGCHK(dwChunkHeaderSwallow <= dwRemaining);

        if (dwBytesInNextChunk == 0) 
        {
            // we've read in last chunk.
            dwBytesInNextChunk = (DWORD)-1;
            // There may be extra info after this chunk.  Discard it.
            m_iNextDecrypt = m_iNextInFollow = m_iNextIn = m_iNextUnparsedChunk;
            break;
        }

        if (dwRemaining) 
        {
            if (dwRemaining - dwChunkHeaderSwallow > dwRemaining){
                return E_FAIL;
            }
            DWORD dwToMove = dwRemaining-dwChunkHeaderSwallow;
            memmove(GetNextChunkToParse(),GetNextChunkToParse()+dwChunkHeaderSwallow,dwToMove);
        }

        // Because we've stripped bits out of buffer, cleanup other pointers as well
        m_iNextDecrypt  = m_iNextDecrypt - dwChunkHeaderSwallow;
        m_iNextInFollow = m_iNextInFollow - dwChunkHeaderSwallow;
        m_iNextIn       = m_iNextIn - dwChunkHeaderSwallow;
        dwRemaining     = m_iNextIn - m_iNextUnparsedChunk;

        if (dwBytesInNextChunk > dwRemaining) 
        {
            //
            //  There are more bytes in current chunk than we have received already.
            //  Since we want to still give ISAPI extensions upto PostReadSize worth of 
            //  data (usually 48KB), receive additional chunk (or as much as we can)
            //  into the buffer.
            //

            m_iNextUnparsedChunk += dwRemaining;
            dwBytesInNextChunk -= dwRemaining;

            if (Count() >= g_pVars->m_dwPostReadSize) 
            {
                // We have already read PostReadSize (48KB usually) worth of data.
                // The ISAPI extension must do ReadClient() from here on to get more data.
                break;
            }

            // Either read in remainder of chunk or as much data as we can per m_dwPostReadSize.
            DWORD dwToRead = min(g_pVars->m_dwPostReadSize-Count(),dwBytesInNextChunk);

            DWORD dwInitialBufEnd = m_iNextIn;

            // Receive the data
            hi = RecvBody(sock,dwToRead,TRUE,FALSE,pRequest,FALSE);
            if (hi != S_OK)
            {
                return FALSE;
            }

            // Above call will fill internal buffer to at least dwToRead bytes
            // or else it will fail.  It may get more for SSL case (since
            // it reads in enough to decrypt entire buffer, rather than leaving 
            // unencrypted stuff at end).
            DWORD dwNumBytesRead = m_iNextIn - dwInitialBufEnd;
            DEBUGCHK(dwNumBytesRead >= dwToRead);

            if (dwNumBytesRead < dwBytesInNextChunk) 
            {
                // We're hit the maximum amount we can read on an initial POST.  
                // Decrement the amount remaining in this block, but can't do anything
                // beyond this.  It's on ISAPI ReadClient() now.
                dwBytesInNextChunk -= dwNumBytesRead;
                break; 
            }
        }
        else 
        {
            // We have already read into the entire chunk into memory on a previous recv()
            // We'll go through loop again to look at next chunk.
        }

        m_iNextUnparsedChunk += dwBytesInNextChunk;
    }

    BufferConsistencyChecks();

    m_iNextDecrypt = m_iNextInFollow = m_iNextIn;

    *pdwBytesInNextChunk = dwBytesInNextChunk;
    return TRUE;
}

// When ISAPI extension is calling ReadClient() and there is chunked uploading,
// do appropriate stripping out of chunked headers and do basic validation.
BOOL CHttpRequest::ReadClientChunked(PVOID pv, PDWORD pdw) 
{
    DEBUGCHK(m_fIsChunkedUpload);

    //
    //  Directly receive bytes from network
    //

    if (m_dwBytesRemainingInChunk == (DWORD)-1) 
    {
        // Signal to ISAPI extension that there is no more data available to be read.
        *pdw = 0;
        return TRUE;
    }

    // Do not recv() beyond current chunk length (since this could be last chunk)
    DWORD dwRead = min(*pdw,m_dwBytesRemainingInChunk);

    // Call through to real read client to perform the read
    BOOL fReadClient = ReadClientNonChunked(pv,&dwRead);
    if (! fReadClient)
    {
        return FALSE;
    }

    *pdw = dwRead;

    if (m_dwBytesRemainingInChunk > dwRead) 
    {
        // The caller buffer was not large enough to contain the current chunk
        // we're reading.  In this case we're done.
        m_dwBytesRemainingInChunk -= *pdw;
        return TRUE;
    }

    //
    //  We've read to the end of current chunk, need to process chunk headers
    //  and trailers and strip them from what we send back to ISAPI.  If there
    //  are multiple chunks in one read buffer, handle that, but don't read 
    //  beyond last chunk in buffer (since ISAPI does its own buffer mgmt).  
    //

    // Number of bytes past the chunk body, starting with trailing CRLF
    DWORD dwRemaining    = *pdw - m_dwBytesRemainingInChunk;
    BYTE *pbUnparsedData = (BYTE*)pv + m_dwBytesRemainingInChunk;

    CChunkParser chunkParser(this);

    while (1) 
    {
        DWORD dwChunkHeaderSwallow;
        chunkParser.SetChunkHeader(pbUnparsedData, dwRemaining);
        
        if (! chunkParser.ParseChunkHeader(&dwChunkHeaderSwallow, &m_dwBytesRemainingInChunk))
        {
            return FALSE;
        }

        DEBUGCHK(dwChunkHeaderSwallow <= dwRemaining);

        // Account for bytes we've stripped out in chunked header/trailer.
        *pdw = (*pdw) - dwChunkHeaderSwallow;
        // no overflow
        if (dwRemaining - dwChunkHeaderSwallow > dwRemaining){
            return FALSE;
        }
        dwRemaining -= dwChunkHeaderSwallow;

        // Remove the chunk trailer & header from the stream to be returned to client.
        if (dwRemaining)
        {
            memmove(pbUnparsedData,pbUnparsedData+dwChunkHeaderSwallow,dwRemaining);
        }

        if (m_dwBytesRemainingInChunk == 0) 
        {
            // This is the last chunk.  Setup for next ReadClient() call so we indicate this.
            m_dwBytesRemainingInChunk = (DWORD)-1;
            break;
        }

        if (m_dwBytesRemainingInChunk > dwRemaining) 
        {
            // The size of this chunk is larger than the rest of the buffer we have
            // available.  So ISAPI will need another ReadClient with new buffer.
            m_dwBytesRemainingInChunk -= dwRemaining;
            break;
        }
        else 
        {
            // The buffer contains the entire chunk body.  Read next chunk.
            pbUnparsedData = pbUnparsedData + m_dwBytesRemainingInChunk;
        }
    }

    return TRUE;
}


