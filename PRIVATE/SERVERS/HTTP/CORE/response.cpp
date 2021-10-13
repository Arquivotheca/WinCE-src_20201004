//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: RESPONSE.CPP
Abstract: Handles response to client.
--*/

#include "httpd.h"


const CHAR cszClose[]                  = "close";


#define APPENDCRLF(psz)     { *psz++ = '\r';  *psz++ = '\n'; }

// ***NOTE***: The order of items in this table must match that in enum RESPONSESTATUS.
// If any additional entries are added they must match this enum, and update httpd.rc as well.

STATUSINFO rgStatus[STATUS_MAX] = {
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
{ 423, "Locked",                    NULL, TRUE}
};

#define LCID_USA	MAKELANGID(LAND_ENGLISH, SUBLANG_ENGLISH_US);

DWORD GetETagFromFiletime(FILETIME *pft, DWORD dwFileSize, PSTR szWriteBuffer) {
	return sprintf (szWriteBuffer,"\"%x%x%x%x%x%x%x%x:%x:%x\"",
            (DWORD)(((PUCHAR)pft)[0]),(DWORD)(((PUCHAR)pft)[1]),
            (DWORD)(((PUCHAR)pft)[2]),(DWORD)(((PUCHAR)pft)[3]),
            (DWORD)(((PUCHAR)pft)[4]),(DWORD)(((PUCHAR)pft)[5]),
            (DWORD)(((PUCHAR)pft)[6]),(DWORD)(((PUCHAR)pft)[7]),
            dwFileSize,
	        g_pVars->m_dwSystemChangeNumber);
}



void CHttpResponse::SendHeaders(PCSTR pszExtraHeaders, PCSTR pszNewRespStatus) {
	DEBUG_CODE_INIT;
	CHAR szBuf[HEADERBUFSIZE]; 
	PSTR pszBuf = szBuf;
	int iLen;
	PSTR pszTrav;
	DWORD cbNeeded ;
	
	if ( g_pVars->m_fFilters &&
	    ! m_pRequest->CallFilter(SF_NOTIFY_SEND_RESPONSE))
		myleave(97);

	// For HTTP 0.9, we don't send headers back.
	if (m_pRequest->m_dwVersion <= MAKELONG(9, 0))
		myleave(0);	


	// We do calculation up front to see if we need a allocate a buffer or not.
	// Certain may be set by an ISAPI Extension, Filter, or ASP
	// and we must do the check to make sure their sum plus http headers server
	// uses will be less than buffer we created.
	cbNeeded = m_pRequest->m_bufRespHeaders.Count()    	+ 
					 MyStrlenA(pszExtraHeaders)  				+ 
					 MyStrlenA(pszNewRespStatus) 				+ 
					 MyStrlenA(m_pszRedirect)    				+ 
					 MyStrlenA(m_pRequest->m_pszNTLMOutBuf)   	+
					 ((m_pRequest->m_pFInfo && m_pRequest->m_rs == STATUS_UNAUTHORIZED) ? 
					 		MyStrlenA(m_pRequest->m_pFInfo->m_pszDenyHeader) : 0);


	// If what we need for extra + 1000 bytes (for the regular Http headers) is
	// > our buf, allocate a new one.

	if (cbNeeded + 1000 > HEADERBUFSIZE) {
		if (!(pszBuf = MyRgAllocNZ(CHAR,cbNeeded+1000)))
			myleave(98);
	}
	pszTrav = pszBuf;

	// the status line
	// ServerSupportFunction on SEND_RESPONSE_HEADER can override the server value

	// NOTE: using sprintf on user supplied strings is very dangerous as there may be
	// sprintf qualifiers in it, so we'll be safe and strcpy.

	if (pszNewRespStatus)  {
		pszTrav += sprintf(pszTrav,"HTTP/1.%d ",m_pRequest->MinorHTTPStatus());
		pszTrav = strcpyEx(pszTrav,pszNewRespStatus);
		APPENDCRLF(pszTrav);
	}
	else
		pszTrav += sprintf(pszTrav, cszRespStatus,m_pRequest->MinorHTTPStatus(), rgStatus[m_pRequest->m_rs].dwStatusNumber, rgStatus[m_pRequest->m_rs].pszStatusText);

	// GENERAL HEADERS
	// the Date line
	SYSTEMTIME st;
	GetSystemTime(&st); // NOTE: Must be GMT!

	pszTrav = strcpyEx(pszTrav, cszRespDate);
	pszTrav += WriteDateGMT(pszTrav,&st,TRUE);

	// Connection: header 
	if (m_connhdr != CONN_NONE) {
		pszTrav += sprintf(pszTrav, cszConnectionResp, (m_connhdr==CONN_KEEP ? cszKeepAlive : cszClose));
	}

	// RESPONSE HEADERS	
	// the server line
	pszTrav += sprintf(pszTrav, cszRespServer, m_pRequest->m_pWebsite->m_szServerID);

	// the Location header (for redirects)
	if (m_pszRedirect) {
		DEBUGCHK(m_pRequest->m_rs == STATUS_MOVED);
		pszTrav = strcpyEx(pszTrav, cszRespLocation);
		pszTrav = strcpyEx(pszTrav, m_pszRedirect);
		APPENDCRLF(pszTrav);		
	}

	// the WWW-Authenticate line
	if (m_pRequest->m_rs == STATUS_UNAUTHORIZED && m_pRequest->SendAuthenticate()) {
		// In the middle of an NTLM authentication session
		if (m_pRequest->AllowNTLM() && m_pRequest->m_pszNTLMOutBuf) {
			pszTrav += sprintf(pszTrav,"%s %s\r\n",cszRespWWWAuthNTLM,m_pRequest->m_pszNTLMOutBuf);	
		}
		// If both schemes are enabled, send both back to client and let the browser decide which to use
		else if(m_pRequest->AllowBasic() && m_pRequest->AllowNTLM()) {
			pszTrav += sprintf(pszTrav,"%s\r\n%s",cszRespWWWAuthNTLM,cszRespWWWAuthBasic);
		}
		else if(m_pRequest->AllowBasic()) {
			pszTrav += sprintf(pszTrav, cszRespWWWAuthBasic);
		}
		else if(m_pRequest->AllowNTLM()) {
			pszTrav += sprintf(pszTrav,"%s\r\n",cszRespWWWAuthNTLM);
		}
	}

	// ENTITY HEADERS
	// the Last-Modified line
	if (m_hFile) {
		WIN32_FILE_ATTRIBUTE_DATA fData;
		
		if( GetFileAttributesEx(m_pRequest->m_wszPath,GetFileExInfoStandard,&fData) && 
		    FileTimeToSystemTime(&fData.ftLastWriteTime, &st) )
		{
			pszTrav += sprintf(pszTrav, "%s ",cszRespLastMod);
			pszTrav += WriteDateGMT(pszTrav,&st);
			APPENDCRLF(pszTrav);

			if (m_pRequest->m_pWebsite->m_fWebDav) {
				pszTrav = strcpyEx(pszTrav, cszETag);
				*pszTrav++ = ' ';
				pszTrav += GetETagFromFiletime(&fData.ftLastWriteTime,fData.dwFileAttributes,pszTrav);
				APPENDCRLF(pszTrav);
			}
		}
	}

	// the Content-Type line
	if (m_pszType) {
		pszTrav += sprintf(pszTrav,cszRespType,m_pszType);
	}

	// the Content-Length line
	if (m_dwLength) {
		// If we have a file that is 0 length, it is set to -1.
		if (m_dwLength == -1)
			m_dwLength = 0;
		pszTrav += sprintf(pszTrav, cszRespLength, m_dwLength);
	}

	if ((m_pRequest->m_rs == STATUS_UNAUTHORIZED) && m_pRequest->m_pFInfo && m_pRequest->m_pFInfo->m_pszDenyHeader) {
		pszTrav = strcpyEx(pszTrav,m_pRequest->m_pFInfo->m_pszDenyHeader);
		// It's the script's responsibility to add any \r\n to the headers.		
	}

	if (m_pRequest->m_bufRespHeaders.Data()) {
		memcpy(pszTrav,m_pRequest->m_bufRespHeaders.Data(),m_pRequest->m_bufRespHeaders.Count());
		pszTrav += m_pRequest->m_bufRespHeaders.Count();
	}

	if (pszExtraHeaders) {
		pszTrav = strcpyEx(pszTrav,pszExtraHeaders);
		// It's the script's responsibility to include the final double CRLF, as per IIS.
	}
	else 
		APPENDCRLF(pszTrav);	// the double CRLF signifies that header data is at an end

	*pszTrav = 0;

	iLen = pszTrav - pszBuf;
	m_pRequest->SendData(pszBuf,iLen);

done:
	DEBUGMSG_ERR(ZONE_ERROR, (L"HTTPD: SendResponse failed, error = %d\r\n"));
	DEBUGMSG(ZONE_RESPONSE, (L"HTTPD: Sending RESPONSE HEADER<<%a>>\r\n", pszBuf));

	if (pszBuf != szBuf)
		MyFree(pszBuf);
}


void CHttpResponse::SendBody(void) {
	if (VERB_HEAD == m_pRequest->m_idMethod)
		return;
		
	if(m_hFile)
		SendFile(m_pRequest->m_socket, m_hFile, m_pRequest);
	else if(m_pszBody) {
		DEBUGMSG(ZONE_RESPONSE, (L"HTTPD: Sending RESPONSE BODY<<%a>> len=%d\r\n", m_pszBody, m_dwLength));
		m_pRequest->SendData((PSTR) m_pszBody, m_dwLength,TRUE);
	}
}

void SendFile(SOCKET sock, HANDLE hFile, CHttpRequest *pRequest) {
	BYTE bBuf[4096];
	DWORD dwRead;

	while(ReadFile(hFile, bBuf, sizeof(bBuf), &dwRead, 0) && dwRead) {
		if (! pRequest->SendData((PSTR) bBuf, dwRead))
			return;
	}
}

//  This function used to display the message "Object moved
void CHttpResponse::SendRedirect(PCSTR pszRedirect, BOOL fFromFilter) {
	if (!fFromFilter && m_pRequest->FilterNoResponse())
		return;	
		
	DEBUGCHK(!m_hFile);
	DEBUGCHK(m_pRequest->m_rs==STATUS_MOVED);
	// create a body
	PSTR pszBody = MySzAllocA(strlen(rgStatus[m_pRequest->m_rs].pszStatusBody)+strlen(pszRedirect)+30);
	DWORD dwLen = 0;
	if(pszBody) {
		sprintf(pszBody, rgStatus[m_pRequest->m_rs].pszStatusBody, pszRedirect);
		SetBody(pszBody, cszTextHtml);
	}
	// set redirect header & send all headers, then the synthetic body
	m_pszRedirect = pszRedirect;
	SendHeaders(NULL,NULL);
	SendBody();
	MyFree(pszBody);
}

//  Read strings of the bodies to send on web server errors ("Object not found",
//  "Server Error",...) using load string.  These strings are in wide character
//  format, so we first convert them to regular strings, marching along
//  pszBodyCodes (which is a buffer size BODYSTRINGSIZE).  After the conversion,
//  we set each rgStatus[i].pszStatusBody to the pointer in the buffer.
void InitializeResponseCodes(PSTR pszStatusBodyBuf) {	
	PSTR pszTrav = pszStatusBodyBuf;
	UINT i;
	int iLen;
	WCHAR wszBuf[256];

	for(i = 0; i < ARRAYSIZEOF(rgStatus); i++) {
		if (!rgStatus[i].fHasDefaultBody)
			continue;

		LoadString(g_hInst,RESBASE_body + i,wszBuf,ARRAYSIZEOF(wszBuf));

		iLen = MyW2A(wszBuf, pszTrav, BODYSTRINGSIZE - (pszTrav - pszStatusBodyBuf));
		if (!iLen)
			return;

		rgStatus[i].pszStatusBody = pszTrav;
		pszTrav += iLen;
	}
}

BOOL CHttpRequest::SendData(PSTR pszBuf, DWORD dwLen, BOOL fCopyBuffer) {
	DEBUG_CODE_INIT;
	BOOL fRet = FALSE;
	PSTR pszFilterData = pszBuf;
	DWORD dwFilterData = dwLen;

	if (g_pVars->m_fFilters &&
	    ! CallFilter(SF_NOTIFY_SEND_RAW_DATA, &pszFilterData, (int*)&dwFilterData))
	{
		return FALSE;
	}

	if (!m_fIsSecurePort) {
		return (dwLen == (DWORD) send(m_socket,pszFilterData,dwFilterData,0));
	}

	// If the filter modified the data's pointer then we'll need to copy data, filter
	// is the owner of this buffer and we don't want to trounce it.  

	fCopyBuffer = fCopyBuffer || (pszBuf != pszFilterData);
	return SendEncryptedData(pszFilterData,dwFilterData,fCopyBuffer);
}

void CHttpRequest::GetContentTypeOfRequestURI(PSTR szContentType, DWORD ccContentType, const WCHAR *wszExt) {
	const WCHAR *wszContentType;
	CReg reg;

	szContentType[0] = 0;

	if (!wszExt)
		wszExt = m_wszExt;

	if (!wszExt)
		goto done;

	if (! reg.Open(HKEY_CLASSES_ROOT,wszExt))
		goto done;

	wszContentType = reg.ValueSZ(L"Content Type");

	if (wszContentType) {
		if (0 == MyW2A(wszContentType, szContentType, ccContentType))
			szContentType[0] = 0;
	}

done:
	if(! szContentType[0])
		strcpy(szContentType,cszOctetStream);
}
