//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: PARSER.CPP
Abstract: HTTP request parser
--*/

#include "httpd.h"

// Methods
// NOTE: This follows same order as "enum VERB" list in request.h.

const char *rg_cszMethods[] = {
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
//	"SEARCH",
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
// const CHAR  cszMS_Author_Via[]           = "MS--Via:";
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

const CHAR  cszRespWWWAuthBasic[]          = "WWW-Authenticate: Basic realm=\"Microsoft-WinCE\"\r\n";
const DWORD ccRespWWWAuthBasic             = SVSUTIL_CONSTSTRLEN(cszRespWWWAuthBasic);
const CHAR  cszRespWWWAuthNTLM[]           = "WWW-Authenticate: NTLM";
const DWORD ccRespWWWAuthNTLM              = SVSUTIL_CONSTSTRLEN(cszRespWWWAuthNTLM);
const CHAR  cszRespDate[]                  = "Date: ";
const DWORD ccRespDate                     = SVSUTIL_CONSTSTRLEN(cszRespDate);
const CHAR  cszRespLocation[]              = "Location: ";
const DWORD ccRespLocation                 = SVSUTIL_CONSTSTRLEN(cszRespLocation);

const CHAR  cszConnectionResp[]            = "Connection: %s\r\n";
const DWORD ccConnectionResp               = SVSUTIL_CONSTSTRLEN(cszConnectionResp);
const CHAR  cszKeepAlive[]                 = "keep-alive";
const DWORD ccKeepAlive                    = SVSUTIL_CONSTSTRLEN(cszKeepAlive);
const CHAR  cszCRLF[]                      = "\r\n";
const DWORD ccCRLF                         = SVSUTIL_CONSTSTRLEN(cszCRLF);
const CHAR  cszRespLastMod[]               = "Last-Modified:";
const DWORD ccRespLastMod                  = SVSUTIL_CONSTSTRLEN(cszRespLastMod);


const CHAR  cszOpaquelocktokenPrefix[]     = "opaquelocktoken:";
const DWORD ccOpaquelocktokenPrefix        = SVSUTIL_CONSTSTRLEN(cszOpaquelocktokenPrefix);


#define PFNPARSE(x)	&(CHttpRequest::Parse##x)
#define TABLEENTRY(csz, id, pfn)	   { csz, sizeof(csz)-1, id, PFNPARSE(pfn) }

typedef (CHttpRequest::*PFNPARSEPROC)(PCSTR pszTok, TOKEN idHeader);

typedef struct tagHeaderDesc {
	const char*		sz;
	int		        iLen;
	TOKEN		    id;
	PFNPARSEPROC	pfn;
} HEADERDESC;


const HEADERDESC rgHeaders[] =
{
// General headers
	TABLEENTRY(cszConnection, TOK_CONNECTION, Connection),
	//TABLEENTRY(cszDate,	  TOK_DATE,   Date),
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
	TABLEENTRY(cszContent_Type, TOK_TYPE,	 ContentType),
	TABLEENTRY(cszContent_Length, TOK_LENGTH,	 ContentLength),
	TABLEENTRY(cszHost, TOK_HOST,    Host),
	{ 0, 0, (TOKEN)0, 0}
};



// Parse all the headers, line by line
BOOL CHttpRequest::ParseHeaders() {	
	DEBUG_CODE_INIT;
	PSTR pszTok;
	PWSTR pwszTemp;
	PSTR pszPathInfo = NULL;
	int i, iLen;
	BOOL ret = FALSE;

	if (!m_bufRequest.NextTokenWS(&pszTok, &iLen)) {
		m_rs = STATUS_BADREQ;
		myleave(287);
	}

	if (! ParseMethod(pszTok,iLen)) {
		m_rs = STATUS_BADREQ;
		myleave(288);
	}

	if (!m_bufRequest.NextLine()) {
		m_rs = STATUS_BADREQ;
		myleave(290);
	}
	
	// outer-loop. one header per iteration
	while (m_bufRequest.NextTokenHeaderName(&pszTok, &iLen)) {
		// compare token with tokens in table
		for (i=0; rgHeaders[i].sz; i++) 	{
			//DEBUGMSG(ZONE_PARSER, (L"HTTPD: Comparing %a %d %d\r\n", rgHeaders[i].sz, rgHeaders[i].iLen, rgHeaders[i].pfn));
			if ( ((int)rgHeaders[i].iLen == iLen) &&
				0==_memicmp(rgHeaders[i].sz, pszTok, iLen) )
					break;
		}
		if (rgHeaders[i].pfn) {
			DEBUGMSG(ZONE_PARSER, (L"HTTPD: Parsing %a\r\n", rgHeaders[i].sz));
			// call the specific function to parse this header. 
			if (! ((this->*(rgHeaders[i].pfn))(pszTok, rgHeaders[i].id)) ) {
				DEBUGMSG(ZONE_PARSER, (L"HTTPD: Parser: failed to parse %a\r\n", rgHeaders[i].sz));

				// If none of the Parse functions set last error, set to bad req
				if (m_rs == STATUS_OK)
					m_rs = STATUS_BADREQ;

				myleave(296);
			}
		}
		else {
			DEBUGMSG(ZONE_PARSER, (L"HTTPD: Ignoring header %a\r\n", pszTok));
		}
		if (!m_bufRequest.NextLine()) {
			m_rs = STATUS_BADREQ;
			myleave(290);
		}
	}

	if (!m_bufRequest.NextLine()) { // eat the blank line
		m_rs = STATUS_BADREQ;
		myleave(295);
	}
	DEBUGMSG(ZONE_PARSER, (L"HTTPD: Parser: DONE\r\n"));

	// check what we got 
	if (!m_pszMethod) {
		DEBUGMSG(ZONE_PARSER, (L"HTTPD: Parser: missing URL or method, illformatted Request-line\r\n"));
		m_rs = STATUS_BADREQ;
		myleave(291);
	}

	// Once we've read the request line, give filter shot at modifying the
	// remaining headers.
	if (g_pVars->m_fFilters && 
		! CallFilter(SF_NOTIFY_PREPROC_HEADERS))
		myleave(292);


	if (!m_fResolvedHostWebSite) {
		// Now that we have Host: (if we'll get it) map website on first request.
		m_pWebsite = MapWebsite(m_socket,m_pszHost);

		if (!m_pWebsite) {
			m_pWebsite = g_pVars; // reset for when we write out log/filters.
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Unable to map website on socket %x\r\n",m_socket));
			m_rs = STATUS_NOTFOUND;
			myleave(297);
		}
		m_fResolvedHostWebSite = TRUE;
	}

	m_wszPath = m_pWebsite->m_pVroots->URLAtoPathW(m_pszURL, &m_pVRoot, &m_pszPathInfo,TRUE);

	if (g_pVars->m_fFilters &&
	    ! CallFilter(SF_NOTIFY_URL_MAP))
		myleave(293);

	// get extension
	if (m_wszPath && (pwszTemp = wcsrchr(m_wszPath, '.')))
		m_wszExt = MySzDupW(pwszTemp);

	if (m_wszPath)
		m_ccWPath = wcslen(m_wszPath);


	ret = TRUE;
done:
	DEBUGMSG_ERR(ZONE_PARSER,(L"HTTPD: Parse headers failed, err = %d\r\n",err));
	return ret;
}

VERB FindMethod(PCSTR pszMethod, int cbMethod) {
	for (int i = 0; i < cKnownMethods; i++) {
		if (0 == memcmp(pszMethod,rg_cszMethods[i],cbMethod)) {
			DEBUGCHK( (VERB_UNKNOWN+i+1) <= VERB_LAST_VALID);
			return (VERB) (VERB_UNKNOWN+i+1);
		}
	}
	return VERB_UNKNOWN;
}

BOOL CHttpRequest::ParseMethod(PCSTR pszMethod, int cbMethod) {
	DEBUG_CODE_INIT;
	PSTR pszTok, pszTok2;
	int iLen;
	BOOL ret = FALSE;

	// save method
	m_pszMethod = MySzDupA(pszMethod);
	m_idMethod = FindMethod(pszMethod,cbMethod);

	// get URL and HTTP/x.y together (allows for spaces in URL)
	if (!m_bufRequest.NextTokenEOL(&pszTok, &iLen))
		myretleave(FALSE, 201);

	// separate out the HTTP/x.y
	if (pszTok2 = strrchr(pszTok, ' ')) {
		*pszTok2 = 0;
		iLen = pszTok2-pszTok;
		pszTok2++;
	}
	
	// clean up & parse the URL
	if (! MyCrackURL(pszTok, iLen))
		goto done;
	
	// get version (optional. HTTP 0.9 wont have this)
	if (!pszTok2)
		m_dwVersion = MAKELONG(9, 0);
	else {
		int iMajor, iMinor;
		if (2 != sscanf(pszTok2, cszHTTPVER, &iMajor, &iMinor)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: unable to parse HTTP version from request\r\n"));
			pszTok2[-1] = ' ';	// reset this to a space
			myretleave(FALSE,202);
		}
		m_dwVersion = MAKELONG(iMinor, iMajor);
		
		pszTok2[-1] = ' ';	// reset this to a space
	}
	ret = TRUE;

done:
	DEBUGMSG_ERR(ZONE_PARSER, (L"HTTPD: end ParseMethod (iGLE=%d iErr=%d)\r\n", GLE(err),err));
	return ret;
}

// We assume a raw URL in the form that we receive in the HTTP headers (no scheme, port number etc)
// We extract the path, extra-path, and query
BOOL CHttpRequest::MyCrackURL(PSTR pszRawURL, int iLen) {
	DEBUG_CODE_INIT;
	BOOL ret = FALSE;
	PSTR  pszDecodedURL=0, pszTemp=0;
	int iLen2;

	// special case handeling of 'OPTIONS *' case.
	if (*pszRawURL == '*' && *(pszRawURL+1) == 0 && (m_idMethod == VERB_OPTIONS)) {
		m_fOptionsAsterisk = TRUE;
		m_pszURL = MySzDupA("*");
		return TRUE;
	}

	// In all other cases, HTTPD and a number of ISAPIs assume that the path
	// begins with a '/'.  If it doesn't reject request now.
	if (*pszRawURL != '/') {
		m_rs = STATUS_BADREQ;
		goto done;
	}

	// decode URL (convert escape sequences etc)
	if (NULL == (pszDecodedURL = MyRgAllocNZ(CHAR, iLen+1)))
		myleave(382);

	// Use private Internet canonicalization to avoid wininet dependency.

	// There's no memory allocation in our version of this fcn, always succeeds.
	// The size needed to hold the unencoded URL will always be less than or
	// equal to the original URL.
	MyInternetCanonicalizeUrlA(pszRawURL, pszDecodedURL, (DWORD*)&iLen, 0);

	// get query string
	if (pszTemp = strchr(pszDecodedURL, '?')) {
		if (NULL == (m_pszQueryString = MySzDupA(pszTemp+1)))
			goto done;
		*pszTemp = 0;
	}

	// Searching for an embedded ISAPI dll name, ie /wwww/isapi.dll/a/b.
	// We load the file /www/isapi.dll and set PATH_INFO to /a/b
	// Emebbed ASP file names are handled similiarly.
	if (g_pVars->m_fExtensions) {
		if (pszTemp = strstr(pszDecodedURL,".dll/")) {
			m_pszPathInfo = MySzDupA(pszTemp + sizeof(".dll/") - 2);
			pszTemp[sizeof(".dll/") - 2] = 0;
		}
		else if (pszTemp = strstr(pszDecodedURL,".asp/")) {
			m_pszPathInfo = MySzDupA(pszTemp + sizeof(".asp/") - 2);
			pszTemp[sizeof(".asp/") - 2] = 0;
		}
	}
	
	// save a copy of the cleaned up URL (MINUS query!)
	// alloc one extra char in case we have to send a redirect back (see request.cpp)
	iLen2 = strlen(pszDecodedURL);
	if (NULL == (m_pszURL = MySzAllocA(1+iLen2)))
		goto done;

	Nstrcpy(m_pszURL, pszDecodedURL, iLen2); // copy null-term too.
		
	ret = TRUE;
done:
	MyFree(pszDecodedURL);  
	DEBUGMSG_ERR(ZONE_PARSER, (L"HTTPD: end MyCrackURL(%a) path=%s ext=%s query=%a (iGLE=%d iErr=%d)\r\n", 
		pszRawURL, m_wszPath, m_wszExt, m_pszQueryString, GLE(err), err));
	
	return ret;

}

BOOL CHttpRequest::ParseContentLength(PCSTR pszMethod, TOKEN id) {
	PSTR pszTok = 0;
	int iLen = 0;

	if (m_dwContentLength) {
		DEBUGMSG(ZONE_PARSER,(L"HTTPD: Error: Duplicate Content-length headers in request\r\n"));
		return FALSE;
	}
	
	// get length (first token after "Content-Type;")
	if (m_bufRequest.NextTokenEOL(&pszTok, &iLen) && pszTok && iLen) {
		m_dwContentLength = atoi(pszTok);
		if ((int)m_dwContentLength < 0) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Error: Content-Length = %d, negative.  Rejecting the request\r\n",(int)m_dwContentLength));
			return FALSE;
		}
		
		return TRUE;
	}

	return FALSE;
}

BOOL CHttpRequest::ParseCookie(PCSTR pszMethod, TOKEN id) {
	PSTR pszTok = 0;
	int iLen = 0;

	if (m_pszCookie) {
		DEBUGMSG(ZONE_PARSER,(L"HTTPD: Error: Duplicate Cookie headers in request\r\n"));
		return FALSE;
	}
	
	// get cookie (upto \r\n after "Cookies;")
	if (m_bufRequest.NextTokenEOL(&pszTok, &iLen) && pszTok && iLen) {
		if (NULL == (m_pszCookie = MySzDupA(pszTok)))
			return FALSE;
		return TRUE;
	}
	return FALSE;
}

BOOL CHttpRequest::ParseAccept(PCSTR pszMethod, TOKEN id) {
	PSTR pszTok = 0;
	int iLen = 0;

	if (m_pszAccept) {
		DEBUGMSG(ZONE_PARSER,(L"HTTPD: Error: Duplicate accept headers in request\r\n"));
		return FALSE;
	}
	
	// get cookie (upto \r\n after "Cookies;")
	if (m_bufRequest.NextTokenEOL(&pszTok, &iLen) && pszTok && iLen) {
		if (NULL == (m_pszAccept = MySzDupA(pszTok)))
			return FALSE;
		return TRUE;
	}
	return FALSE;
}


BOOL CHttpRequest::ParseContentType(PCSTR pszMethod, TOKEN id) {
	PSTR pszTok = 0;
	int iLen = 0;

	if (m_pszContentType) {
		DEBUGMSG(ZONE_PARSER,(L"HTTPD: Error: Duplicate content-type headers in request\r\n"));
		return FALSE;
	}

	// get type (first token after "Content-Type;")
	if (m_bufRequest.NextTokenEOL(&pszTok, &iLen) && pszTok && iLen) {
		if (NULL == (m_pszContentType = MySzDupA(pszTok)))
			return FALSE;
		return TRUE;
	}
	return FALSE;
}

const char cszDateParseFmt[] = " %*3s, %02hd %3s %04hd %02hd:%02hd:%02hd GMT; length=%d";

BOOL CHttpRequest::ParseIfModifiedSince(PCSTR pszMethod, TOKEN id) {
	PSTR pszTok = 0;
	int iLen = 0;
	int i = 0;
	char szMonth[10];
	SYSTEMTIME st;
	ZEROMEM(&st);

	if (m_ftIfModifiedSince.dwLowDateTime || m_ftIfModifiedSince.dwHighDateTime) {
		DEBUGMSG(ZONE_PARSER,(L"HTTPD:  Error: Duplicate if-modified-since headers in request\r\n"));
		return FALSE;
	}
	
	// get the date (rest of line after If-Modified-Since)
	// NOTE: Note we are handling only one date format (the "reccomended" one)
	if (m_bufRequest.NextTokenEOL(&pszTok, &iLen) && pszTok && iLen) 	{
		i = sscanf(pszTok, cszDateParseFmt, &st.wDay, &szMonth, &st.wYear, &st.wHour, &st.wMinute, &st.wSecond, &m_dwIfModifiedLength);
		if ( i >= 6) {
			// try to match month
			for (i=0; rgMonth[i]; i++) {
				if (0==strcmpi(szMonth, rgMonth[i]))  {
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

BOOL CHttpRequest::ParseAuthorization(PCSTR pszMethod, TOKEN id) {
	DEBUG_CODE_INIT;
	BOOL ret = FALSE;
	PSTR pszTok=0;
	int iLen=0;

	if (m_pszAuthType || m_pszRawRemoteUser) {
		DEBUGMSG(ZONE_PARSER,(L"HTTPD: Error: Duplicate Authorization headers in request\r\n"));
		myretleave(FALSE,99);
	}

	// get the auth scheme (first token after "Authorization;")
	if (!m_bufRequest.NextTokenWS(&pszTok, &iLen) || !pszTok || !iLen)
		myretleave(FALSE, 91);

	if (NULL == (m_pszAuthType = MySzDupA(pszTok)))
		myretleave(FALSE,100);

	// Read in the authentication paramaters and store for after we do vroot parsing.
	if (!m_bufRequest.NextTokenWS(&pszTok, &iLen) || !pszTok || !iLen)
		myretleave(FALSE, 97);

	if (NULL == (m_pszRawRemoteUser = MySzDupA(pszTok)))
		myretleave(FALSE, 98);
	
	ret = TRUE;
done:
	DEBUGMSG_ERR(ZONE_ERROR, (L"HTTPD: Auth FAILED (err=%d ret=%d)\r\n", err, ret));
	return ret; 
}

BOOL CHttpRequest::ParseConnection(PCSTR pszMethod, TOKEN id) {
	PSTR pszTok = 0;
	int iLen = 0;
	
	// get first token after "Connnection;"
	if (m_bufRequest.NextTokenEOL(&pszTok, &iLen) && pszTok && iLen) {
		if (0==strcmpi(pszTok, cszKeepAlive))
			m_fKeepAlive = TRUE;
		return TRUE;
	}
	return FALSE;
}

BOOL CHttpRequest::ParseHost(PCSTR pszMethod, TOKEN id) {
	PSTR pszTok = 0;
	int iLen = 0;

	if (m_pszHost) {
		DEBUGMSG(ZONE_PARSER,(L"HTTPD:  Error: Duplicate host header in request\r\n"));
		return FALSE;
	}

	if (!m_bufRequest.NextTokenEOL(&pszTok, &iLen))
		return FALSE;

	if (NULL == (m_pszHost = MySzDupA(pszTok)))
		return FALSE;

	return TRUE;
}


//  Functions related to MyInternetCanonicalizationURL
 
static const char *HTSkipLeadingWhitespace(const char *str) {
	while (*str == ' ' || *str == '\t') {
		++str;
	}
	return str;
}


// When parsing an URL, if it is something like http://WinCE////Foo/\a.dll/\\john?queryString,
// only allow one '/' or '\' to be consecutive, skip others, up to start of query string.
void HTCopyBufSkipMultipleSlashes(char *szWrite, const char *szRead) {
	BOOL fSkip = FALSE;

	while (*szRead && *szRead != '?') {
		if ((*szRead == '/') || (*szRead == '\\')) {
			if (!fSkip)
				*szWrite++ = *szRead;
		
			fSkip = TRUE; // skip next if it has a '/' or '\\'
		}
		else {
			*szWrite++ = *szRead;
			fSkip = FALSE;
		}
		szRead++;
	}
	strcpy(szWrite,szRead);
}

char from_hex(char c) {
	return c >= '0' && c <= '9' ? c - '0'
	    : c >= 'A' && c <= 'F' ? c - 'A' + 10
	    : c - 'a' + 10;         /* accept small letters just in case */
}


static void HTTrimTrailingWhitespace(char *str)  {
#define CR          '\r'
#define LF          '\n'
	char *ptr;

	ptr = str;

	// First, find end of string

	while (*ptr)
		++ptr;

	for (ptr--; ptr >= str; ptr--) {
		if (IsNonCRLFSpace(*ptr))
			*ptr = 0; /* Zap trailing blanks */
		else
			break;
	}
}

PSTR HTUnEscape(char *pszPath) {
	PSTR pszRead = pszPath;
	PSTR pszWrite = pszPath;

	while (*pszRead)  {
		if (*pszRead == '?')  {
			// leave the query string intact, no conversion.
			while (*pszRead)  {
				*pszWrite++ = *pszRead++;
			}	
			break;
	   	}
		if ((*pszRead == HEX_ESCAPE) && isxdigit(*(pszRead+1)) && isxdigit(*(pszRead+2)))  {
			pszRead++;
			if (*pszRead)
				*pszWrite = from_hex(*pszRead++) * 16;
			if (*pszRead)
				*pszWrite = *pszWrite + from_hex(*pszRead++);
			pszWrite++;
			continue;
		}
		if (*pszRead)
			*pszWrite++ = *pszRead++;
	}
	*pszWrite++ = 0;

	// After conversion, scan for "/.." and remove ".." and path immediatly before it.  "/doc/../home.htm" becomes "/home.htm".
	pszRead = pszWrite = pszPath;

	while (*pszRead) {
		DEBUGCHK(pszWrite <= pszRead); // pszWrite can never skip ahead of where we're reading from.

		if (pszRead[0] == '?') {
			// leave query string intact.
			break;
		}

		if (IsSlash(pszRead[0]) &&
			(pszRead[1] == '.'  && pszRead[2] == '.') &&
			(IsSlash(pszRead[3]) || pszRead[3] == 0))  {
			// Back track along write pointer until we find the previous slash
			// or until we get to beginning of buffer.

			if ((pszWrite != pszPath)) {
				// deal with the case where write pointer is a slash -
				// skip a character back so we can work toward the '/' before current one.
				if (IsSlash(*pszWrite))
					pszWrite --;
			}

			while (!IsSlash(*pszWrite) && pszWrite != pszPath)
				pszWrite--;

			pszRead += 3;
		}
		else {
			*pszWrite++ = *pszRead++; // typical case.  No conversion.
		}
	}

	DEBUGCHK(pszWrite <= pszRead);

	// If there is remaining data after query string, copy it now.
	while (*pszRead)
		*pszWrite++ = *pszRead++;

	if (pszWrite == pszPath) {
		// handle case where '..' ate up all real content.
		*pszWrite++ = '/';
	}

	*pszWrite = 0;

	return pszPath;
}

BOOL
MyInternetCanonicalizeUrlA(IN LPCSTR lpszUrl,OUT LPSTR lpszBuffer,
                           IN OUT LPDWORD lpdwBufferLength, IN DWORD dwFlags)
{
	const char * pszTrav = HTSkipLeadingWhitespace(lpszUrl);
	HTCopyBufSkipMultipleSlashes(lpszBuffer,pszTrav);
	HTTrimTrailingWhitespace(lpszBuffer);
	HTUnEscape(lpszBuffer);	

	return TRUE;
}
