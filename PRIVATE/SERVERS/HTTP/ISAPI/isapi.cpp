//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: ISAPI.CPP
Abstract: ISAPI handling code
--*/

#include "httpd.h"

// Prefix for GetServerVariable for generic HTTP header retrieval, so "HTTPD_FOOBAR" gets HTTP header FOOBAR.
const char cszHTTP[] = "HTTP_";
const DWORD dwHTTP   = CONSTSIZEOF(cszHTTP);


// This function is used when looking through HTTP headers for HTTP_VARIABLE.  We
// can't use strcmp because headers (for most part) have words separated by "-",
// the pszVar format has vars separated by "_".  IE pszVar=HTTP_ACCEPT_LANGUAGE
// should return for us HTTP header "ACCEPT-LANGUAGE"
BOOL GetVariableStrCmp(PSTR pszHeader, PSTR pszVar, DWORD dwLen) {
	BOOL fRet = FALSE;

	for (DWORD i = 0; i < dwLen; i++)  {
		if ( (tolower(*pszHeader) != tolower(*pszVar)) &&
		     (*pszVar != '_' && *pszHeader != '-'))  {
		     goto done;
		}
		pszHeader++;
		pszVar++;
	}
	fRet = (*pszHeader == ':');

done:
	return fRet;
}

BOOL CHttpRequest::GetServerVariable(PSTR pszVar, PVOID pvOutBuf, PDWORD pdwOutSize, BOOL fFromFilter) {
	DWORD dwLen;
	char szBuf[4096];
	PSTR pszRet = (PSTR)-1;
	PSTR pszTrav = NULL;
	CHAR chSave;

	if (!pdwOutSize) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	if(0==_stricmp(pszVar, "AUTH_TYPE"))
		pszRet = m_pszAuthType;
	else if (0 == _stricmp(pszVar, "AUTH_USER"))
		pszRet = m_pszRemoteUser;
	else if (0 == _stricmp(pszVar, "AUTH_PASSWORD"))
		pszRet = m_pszPassword;
	else if(0==_stricmp(pszVar, "CONTENT_LENGTH")) {
		sprintf(szBuf, "%d", m_dwContentLength);
		pszRet = szBuf;
	}
	else if(0==_stricmp(pszVar, "CONTENT_TYPE"))
		pszRet = m_pszContentType;
	else if(0==_stricmp(pszVar, "PATH_INFO"))
		pszRet = m_pszPathInfo;	
	else if(0==_stricmp(pszVar, "PATH_TRANSLATED"))
		pszRet = m_pszPathTranslated;		
	else if(0==_stricmp(pszVar, "QUERY_STRING"))
		pszRet = m_pszQueryString;
	else if(0==_stricmp(pszVar, "REMOTE_ADDR"))  {
		if (! GetRemoteAddress(m_socket, szBuf,FALSE,sizeof(szBuf)))
			return FALSE; // WSA layer calls SetLastError() in this case

		pszRet = szBuf;
	}
	else if (0==_stricmp(pszVar, "REMOTE_HOST"))  {
		if (! GetRemoteAddress(m_socket, szBuf,TRUE,sizeof(szBuf)))
			return FALSE;  // WSA layer calls SetLastError() in this case

		pszRet = szBuf;
	}
	else if(0==_stricmp(pszVar, "REMOTE_USER"))
		pszRet = m_pszRemoteUser;
	else if(0==_stricmp(pszVar, "UNMAPPED_REMOTE_USER"))
		pszRet = m_pszRemoteUser;
	else if(0==_stricmp(pszVar, "REQUEST_METHOD"))
		pszRet = m_pszMethod;
	else if (0==_stricmp(pszVar, "URL"))  {
		pszRet = m_pszURL;
	}
	else if (0==_stricmp(pszVar, "SCRIPT_NAME")) {
		if (fFromFilter)
			pszRet = NULL;
		else
			pszRet = m_pszURL;	
	}
	else if (0==_stricmp(pszVar, "SERVER_NAME")) {
		pszRet = m_pszHost;
	}
	else if(0==_stricmp(pszVar, "SERVER_PORT")) {
#if defined (OLD_CE_BUILD)
		SOCKADDR_IN sockAddr;
		int         iSockLen = sizeof(sockAddr);

		if ((0 != getsockname(m_socket,(PSOCKADDR) &sockAddr,&iSockLen)) || (sockAddr.sin_family != AF_INET)) {
			// we can't handle anything other than TCP/IP ports currently, no way to communicate AF back to ISAPI.
			// Set this error instead of default ERROR_INVALID_INDEX.
			SetLastError(WSAESOCKTNOSUPPORT);
			return FALSE; 
		}
		sprintf(szBuf, "%d", htons(sockAddr.sin_port));
		pszRet = szBuf;
#else // OLD_CE_BUILD
		SOCKADDR_STORAGE  sockAddr;
		int               iSockLen = sizeof(sockAddr);

		if ((0 != getsockname(m_socket,(PSOCKADDR) &sockAddr,&iSockLen)) || (sockAddr.ss_family != AF_INET && sockAddr.ss_family != AF_INET6)) {
			// we can't handle anything other than TCP/IP ports currently, no way to communicate AF back to ISAPI.
			// Set this error instead of default ERROR_INVALID_INDEX.
			SetLastError(WSAESOCKTNOSUPPORT);
			return FALSE; 
		}
		SOCKADDR_IN *pSockIP = (SOCKADDR_IN*) &sockAddr; // can cast because port is located in same place for ipv6 + ipv4

		sprintf(szBuf, "%d", htons(pSockIP->sin_port));
		pszRet = szBuf;
#endif // OLD_CE_BUILD
	}
	else if (0 == _stricmp(pszVar,"LOCAL_ADDR")) {
		SOCKADDR_STORAGE sockAddr;
		int              iSockLen = sizeof(sockAddr);

		if (0 != getsockname(m_socket,(PSOCKADDR) &sockAddr,&iSockLen)) {
			SetLastError(WSAESOCKTNOSUPPORT);
			return FALSE; 
		}
		if (0 != getnameinfo((PSOCKADDR)&sockAddr,iSockLen,szBuf,sizeof(szBuf),NULL,0,NI_NUMERICHOST)) {
			SetLastError(WSAESOCKTNOSUPPORT);
			return FALSE; 
		}
		pszRet = szBuf;
	}
	else if(0==_stricmp(pszVar, "SERVER_PORT_SECURE")) 	{
		sprintf(szBuf, "%d", m_fIsSecurePort ? 1 : 0);
		pszRet = szBuf;
	}

	// HTTP_VERSION is version info as received from the client
	else if(0==_stricmp(pszVar, "HTTP_VERSION")) {
		sprintf(szBuf, cszHTTPVER, HIWORD(m_dwVersion), LOWORD(m_dwVersion));
		pszRet = szBuf;
	}

	// SERVER_PROTOCOL is the version of http server supports, currently 1.0
	else if(0==_stricmp(pszVar, "SERVER_PROTOCOL")) {
		strcpy(szBuf,"HTTP/1.0");
		pszRet = szBuf;
	}
	else if(0==_stricmp(pszVar, "SERVER_SOFTWARE"))
		pszRet = (PSTR)m_pWebsite->m_szServerID;
	else if(0==_stricmp(pszVar, "ALL_HTTP"))
		pszRet = 0;

	// ALL_RAW return http headers, other than the simple request line.
	// The way our buffer is set up, we can have POST data immediatly following
	// header data.  So the client doesn't get confused, we have to set a \0 to it.
	else if (0 == _stricmp(pszVar, "ALL_RAW")) {
		pszRet = m_bufRequest.Headers();	

		if (pszRet) {
			// skip past simple request line.
			pszRet = strstr(pszRet,"\r\n") + 2;

			// If there's unaccessed data, buffer has POST data in it
			if (m_bufRequest.HasPostData()) {
				pszTrav = strstr(pszRet,("\r\n\r\n")) + 4;
				chSave = *pszTrav; *pszTrav = 0;			
			}
		}
	}
	else if(0==_stricmp(pszVar, "HTTP_ACCEPT"))
		pszRet = m_pszAccept;
	else if (0 == _stricmp(pszVar, "HTTPS")) {
		strcpy(szBuf,m_fIsSecurePort ? "on" : "off");
		pszRet = szBuf;				
	}
	else if (0 == _stricmp(pszVar, "HTTPS_KEYSIZE") || 0 == _stricmp(pszVar,"CERT_KEYSIZE"))  {
		// Don't return any HTTPS_ data unless we have a secure connection.
		// Note that if someone is writing a custom filter (m_fIsSecurePort=TRUE, 
		// m_fSkipSSLProcessing=TRUE) then no way for GetServerVariables to access this data.
		if (m_fHandleSSL) {
			sprintf(szBuf,"%d",m_SSLInfo.m_dwSessionKeySize);
			pszRet = szBuf;
		}
		else
			pszRet = NULL;
	}
	else if (0 == _stricmp(pszVar, "HTTPS_SECRETKEYSIZE") || 0 == _stricmp(pszVar,"CERT_SECRETKEYSIZE")) {
		if (m_fHandleSSL) {
			sprintf(szBuf,"%d",m_SSLInfo.m_dwSSLPrivKeySize);
			pszRet = szBuf;
		}
		else
			pszRet = NULL;
	}
	else if (0 == _stricmp(pszVar, "HTTPS_SERVER_ISSUER") || 0 == _stricmp(pszVar,"CERT_SERVER_ISSUER")) {
		if (m_fHandleSSL)
			pszRet = g_pVars->m_pszSSLIssuer;
		else
			pszRet = NULL;
	}
	else if (0 == _stricmp(pszVar, "HTTPS_SERVER_SUBJECT") || 0 == _stricmp(pszVar,"CERT_SERVER_SUBJECT")) {
		if (m_fHandleSSL)
			pszRet = g_pVars->m_pszSSLSubject;
		else
			pszRet = NULL;
	}
	else if (0 == _stricmp(pszVar, "CERT_SUBJECT") || 0 == _stricmp(pszVar,"CERT_ISSUER")) {
		if (m_fHandleSSL && m_SSLInfo.m_pClientCertContext) {
			DWORD dwFlags = (0 == _stricmp(pszVar, "CERT_SUBJECT")) ? 0 : CERT_NAME_ISSUER_FLAG;

			if (pCertGetNameStringA(m_SSLInfo.m_pClientCertContext,CERT_NAME_SIMPLE_DISPLAY_TYPE,dwFlags,0,szBuf,sizeof(szBuf)))
				pszRet = szBuf;
			else
				pszRet = NULL;
		}
		else
			pszRet = NULL;
	}
	else if (0 == _stricmp(pszVar,"CERT_FLAGS")) {
		if (!m_SSLInfo.m_pClientCertContext)
			szBuf[0] = 0;
		else
			_ultoa(m_SSLInfo.m_dwCertFlags,szBuf,10);

		pszRet = szBuf;
	}
	else if (0 == _stricmp(pszVar,"CERT_SERIALNUMBER")) {
		// Buffer size (cert bytes * 2 + # of '-' + NULL)
		if (m_fHandleSSL && m_SSLInfo.m_pClientCertContext && m_SSLInfo.m_pClientCertContext->pCertInfo)  {
			int cbSerialNum = m_SSLInfo.m_pClientCertContext->pCertInfo->SerialNumber.cbData;
			int cbRequired  = cbSerialNum*3;
			PSTR pSerialNumber = (PSTR)m_SSLInfo.m_pClientCertContext->pCertInfo->SerialNumber.pbData;

			DEBUGCHK(pszTrav == NULL);  // we shouldn't have to worry about reseting chSave.

			if ((DWORD)cbRequired > *pdwOutSize || NULL == pvOutBuf) {
				*pdwOutSize = cbRequired;
				SetLastError(ERROR_INSUFFICIENT_BUFFER);
				return FALSE;
			}
			*pdwOutSize = cbRequired;

			if (pSerialNumber) {
				// Serial number is in reverse byte order.  Write directly to pvOutBuf.
				int i = 0;
				pszRet = (PSTR) pvOutBuf;

				for (int iSerialByte = cbSerialNum - 1; iSerialByte >= 0; iSerialByte--) {
					pszRet[i++] = g_szHexDigits[((BYTE)(*(pSerialNumber + iSerialByte))) >> 4];
					pszRet[i++] = g_szHexDigits[((BYTE)(*(pSerialNumber + iSerialByte))) & 0xf];
					pszRet[i++] = '-';
				}
				DEBUGCHK(i == cbRequired);
				pszRet[i-1] = 0;
				
				return TRUE;
			}
		}
		pszRet = NULL;
	}
	else if (0==_strnicmp(pszVar,cszHTTP,dwHTTP)) {
		PSTR  pszStart  = pszVar + dwHTTP;
		PSTR  pszEnd    = strchr(pszStart,'\0');
		DWORD dwLen2    = pszEnd - pszStart;
		DWORD dwOutLen  = 0;
		PSTR  pszHeader = m_bufRequest.Headers();

		if (pszHeader && (dwLen2 > 1)) {
			do {
				if (GetVariableStrCmp(pszHeader,pszStart,dwLen2))  {
					pszHeader += dwLen2+1;  // skip past header + ':'
					while ( (pszHeader)[0] != '\0' && IsNonCRLFSpace((pszHeader)[0])) { ++(pszHeader); }
					pszRet = pszHeader;
					pszTrav = strstr(pszHeader,"\r\n");
					chSave = '\r';
					*pszTrav = 0;
					break;
				}
				pszHeader = strstr(pszHeader,"\r\n")+2;
				if (*pszHeader == '\r')  {
					DEBUGCHK(*(pszHeader+1) == '\n');
					break;
				}				
			} while (1);
		}
	}	
	// end of pseudo-case stmnt

	if((PSTR)(-1) == pszRet) {
		// unknown var
		SetLastError(ERROR_INVALID_INDEX);
		return FALSE;
	}
	// no such header/value. return empty (not NULL!) string
	if(!pszRet)
		pszRet = (PSTR)cszEmpty;

	dwLen = strlen(pszRet)+1;

	if(dwLen > *pdwOutSize || NULL == pvOutBuf)  {
		*pdwOutSize = dwLen;
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		if (pszTrav)
			*pszTrav = chSave;
		return FALSE;
	}
	*pdwOutSize = dwLen;

	memcpy(pvOutBuf, pszRet, dwLen);
	if (pszTrav)
		*pszTrav = chSave;	
	return TRUE;
}



