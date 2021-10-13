//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include "parser.h"
#include "proxy.h"
#include "proxydbg.h"
#include "bldver.h"

const char gc_Host[] = "Host:";
const char gc_Via[] = "Via:";
const char gc_ContentLength[] = "Content-Length:";
const char gc_ContentType[] = "Content-Type:";
const char gc_Connection[] = "Connection:";
const char gc_ProxyConnection[] = "Proxy-Connection:";
const char gc_Auth[] = "Proxy-Authenticate:";
const char gc_Authorization[] = "Proxy-Authorization:";
const char gc_ProxySupport[] = "Proxy-Support:";
const char gc_TransferEncoding[] = "Transfer-Encoding:";
const char gc_Location[] = "Location:";


//
// CHttpHeaders class implementation
//

int CHttpHeaders::GetBufferSize(void)
{
	int iSize = 0;
	int iTmp = 0;

	//
	// If method is not empty then this is a request packet, otherwise it is
	// a response.  It is also possible to be a response with or without a reason.
	//
	
	if (this->strMethod != "") {
		// Request packet
		iSize = this->strMethod.length() + 1 +   		// add 1 for space
				this->strURL.length() + 1 +				// add 1 for space
				this->strVersion.length() + 2;			// add 2 for \r\n
	}
	else if (this->strReason == "") {
		// Response packet without a reason
		iSize = this->strVersion.length() + 1 +			// add 1 for space
				this->strStatusCode.length() + 2;		// add 2 for \r\n
	}
	else {
		// Response packet
		iSize = this->strVersion.length() + 1 +			// add 1 for space
				this->strStatusCode.length() + 1 +		// add 1 for space
				this->strReason.length() + 2;			// add 2 for \r\n
	}

	//
	// GetHeaderLength will return 0 if header does not exist, otherwise it returns the
	// number of bytes required in the buffer to store the header.
	//
	
	iSize += GetHeaderLength(sizeof(gc_Host), this->strHost);
	iSize += GetHeaderLength(sizeof(gc_Via), this->strVia);
	iSize += GetHeaderLength(sizeof(gc_Connection), this->strConnection);
	iSize += GetHeaderLength(sizeof(gc_ProxyConnection), this->strProxyConnection);
	iSize += GetHeaderLength(sizeof(gc_Auth), this->strProxyAuthenticate);
	iSize += GetHeaderLength(sizeof(gc_Authorization), this->strProxyAuthorization);
	iSize += GetHeaderLength(sizeof(gc_ContentLength), this->strContentLength);
	iSize += GetHeaderLength(sizeof(gc_ContentType), this->strContentType);
	iSize += GetHeaderLength(sizeof(gc_ProxySupport), this->strProxySupport);
	iSize += GetHeaderLength(sizeof(gc_TransferEncoding), this->strTransferEncoding);
	iSize += GetHeaderLength(sizeof(gc_Location), this->strLocation);

	iSize += strRest.length() + 2;						// add 2 for \r\n

	return iSize;
}

int CHttpHeaders::GetHeaderLength(int cbField, const char* szValue)
{
	int iSize = strlen(szValue);
	if (iSize > 0) {
		iSize += cbField;
		iSize += 2;	// add 2 for \r\n
	}
	return iSize;
}

void CHttpHeaders::UpdateRequest(void)
{
	//
	// Change proxy connection header to be connection
	//

	if (this->strConnection == "") {
		this->strConnection = this->strProxyConnection;
	}
	this->strProxyConnection = "";

	//
	// Append proxy name to via header
	//

	char szServer[64];
	
	sprintf(szServer, "%s %s", "1.1", (const char*) g_strHostName);
	if (this->strVia != "") {
		this->strVia += " ";
	}
	this->strVia += szServer;
	
	//
	// If no host name is specified, get it from the URL
	//

	int cchHost;
	const char* szAbsURL = this->strURL;
	char* pchAfterHost = strchr(&szAbsURL[7], '/');
	
	if (this->strHost == "") {
		if (NULL == pchAfterHost) {
			cchHost = strlen(&szAbsURL[7]);
		}
		else {
			cchHost = (pchAfterHost - &szAbsURL[7]);
		}
		strncpy(this->strHost.get_buffer(), &szAbsURL[7], cchHost);
	}	

	//
	// Change URL from absolute to relative
	//

	int cbNewURL;
	char szSlash[] = "/";
	int iURLLen = this->strURL.length();
	if (NULL == pchAfterHost) {
		pchAfterHost = szSlash;
		cbNewURL = 2;
	}
	else {
		cbNewURL = iURLLen - (pchAfterHost - szAbsURL) + 1;
	}
	memmove(this->strURL.get_buffer(), pchAfterHost, cbNewURL);

	//
	// If no version is supplied then request as HTTP/1.0
	//

	if (this->strVersion == "") {
		this->strVersion = "HTTP/1.0";
	}
}

void CHttpHeaders::GetRequestBuffer(PBYTE pBuffer)
{
	strcpy((char *)pBuffer, this->strMethod);
	strcat((char *)pBuffer, " ");
	strcat((char *)pBuffer, this->strURL);
	strcat((char *)pBuffer, " ");
	strcat((char *)pBuffer, this->strVersion);
	strcat((char *)pBuffer, "\r\n");

	GetGenericBuffer(pBuffer);
}

void CHttpHeaders::UpdateResponse(void)
{	
	char szViaVersion[4];

	// By default Via version is 1.1
	strcpy(szViaVersion, "1.1");
	
	if (this->strStatusCode == "401") {
		this->strProxySupport = "Session-Based-Authentication";
		strcpy(szViaVersion, "1.0");
		this->strProxyConnection = "";
		this->strConnection = "Keep-Alive";
	}
	else if ((this->strProxyConnection == "") && (this->strStatusCode != "407")) {
		this->strProxyConnection = "Keep-Alive";
	}
	
	//
	// Append proxy name to via header
	//

	char szServer[25];	
	sprintf(szServer, "%s %s", szViaVersion, (const char*) g_strHostName);
	if (this->strVia != "") {
		this->strVia += " ";
	}
	this->strVia += szServer;

	//
	// Always respond that we are HTTP/1.1 server
	//

	this->strVersion = "HTTP/1.1";
}

void CHttpHeaders::GetResponseBuffer(PBYTE pBuffer)
{
	strcpy((char *)pBuffer, this->strVersion);
	strcat((char *)pBuffer, " ");
	strcat((char *)pBuffer, this->strStatusCode);
	if (this->strReason != "") {
		strcat((char *)pBuffer, " ");
		strcat((char *)pBuffer, this->strReason);
	}
	strcat((char *)pBuffer, "\r\n");

	GetGenericBuffer(pBuffer);
}


void CHttpHeaders::GetGenericBuffer(PBYTE pBuffer)
{
	if (this->strConnection != "") {
		AppendHeader(pBuffer, gc_Connection, this->strConnection);
	}
	if (this->strProxyConnection != "") {
		AppendHeader(pBuffer, gc_ProxyConnection, this->strProxyConnection);
	}
	if (this->strProxyAuthenticate != "") {
		AppendHeader(pBuffer, gc_Auth, this->strProxyAuthenticate);		
	}
	if (this->strContentLength != "") {
		AppendHeader(pBuffer, gc_ContentLength, this->strContentLength);
	}
	if (this->strContentType != "") {
		AppendHeader(pBuffer, gc_ContentType, this->strContentType);
	}
	if (this->strHost!= "") {
		AppendHeader(pBuffer, gc_Host, this->strHost);
	}
	if (this->strVia != "") {
		AppendHeader(pBuffer, gc_Via, this->strVia);
	}
	if (this->strProxySupport != "") {
		AppendHeader(pBuffer, gc_ProxySupport, this->strProxySupport);
	}
	if (this->strTransferEncoding != "") {
		AppendHeader(pBuffer, gc_TransferEncoding, this->strTransferEncoding);
	}
	if (this->strLocation != "") {
		AppendHeader(pBuffer, gc_Location, this->strLocation);
	}
	if (this->strRest != "") {
		strcat((char *)pBuffer, this->strRest);
	}

	strcat((char *)pBuffer, "\r\n");
}

void CHttpHeaders::AppendHeader(PBYTE pBuffer, const char* szField, const char* szValue)
{
	strcat((char *)pBuffer, szField);
	strcat((char *)pBuffer, " ");
	strcat((char *)pBuffer, szValue);
	strcat((char *)pBuffer, "\r\n");
}


//
// CHttpParser class implementation
//

DWORD CHttpParser::ParseRequest(const PBYTE pBuf, int cbBuf, CHttpHeaders& headers)
{
	DWORD dwRetVal = ERROR_SUCCESS;

	INET_PARSER parser((char*) pBuf);

	// Get Method, URL, and version
	headers.strMethod = parser.QueryToken();
	headers.strURL = parser.NextToken();
	headers.strVersion = parser.NextToken();

	// Get the port number (if supplied) from the URL
	char* pszPort = strchr(headers.strURL, ':');
	if (pszPort) {
		pszPort++;	// Advance past ':'
		char* pszEnd = strchr(pszPort, '/');
		if (NULL == pszEnd) {
			pszEnd = strchr(pszPort, ' ');
		}

		// If pszEnd is NULL then string is terminated after
		// the port.  Otherwise, need to do a range copy.
		if (NULL == pszEnd) {
			headers.strPort = pszPort;
		}
		else {
			headers.strPort.assign(pszPort, (pszEnd - pszPort));
		}
	}
	
	parser.NextLine();

	dwRetVal = ParseGenericHeaders(parser, headers);

	return dwRetVal;
}

DWORD CHttpParser::ParseResponse(const PBYTE pBuf, int cbBuf, CHttpHeaders& headers)
{
	DWORD dwRetVal = ERROR_SUCCESS;

	INET_PARSER parser((char*) pBuf);

	// Get version, and status code
	headers.strVersion = parser.QueryToken();
	headers.strStatusCode = parser.NextToken();
	
	// Reason string can contain spaces so advance to the next
	// token and get the rest of the line.
	parser.NextToken();
	headers.strReason = parser.QueryLine();	
	
	parser.NextLine();

	dwRetVal = ParseGenericHeaders(parser, headers);

	return dwRetVal;
}

DWORD CHttpParser::ParseGenericHeaders(INET_PARSER& parser, CHttpHeaders& headers)
{
	DWORD dwRetVal = ERROR_SUCCESS;

	//
	// Fill in the headers object.  Each of these is of the form 'field: value\r\n'
	// in any particular order.
	//
	while (1) {
		stringi strField = parser.QueryToken();
		if (strField == "") {
			break;
		}
		parser.NextToken(); // Advance to the next token

		if (strField == gc_Host) {
			headers.strHost = parser.QueryLine();
		}
		else if (strField == gc_ProxyConnection) {
			headers.strProxyConnection = parser.QueryLine();
		}
		else if (strField == gc_Authorization) {
			headers.strProxyAuthorization = parser.QueryLine();
		}
		else if (strField == gc_Connection) {
			headers.strConnection = parser.QueryLine();
		}
		else if (strField == gc_ContentLength) {
			headers.strContentLength = parser.QueryLine();
		}
		else if (strField == gc_Location) {
			headers.strLocation = parser.QueryLine();
		}
		else if (strField == gc_ContentType) {
			// Content Type header can be replicated several times in response packet
			if (headers.strContentType != "") {
				headers.strContentType += " ";
			}
			headers.strContentType = parser.QueryLine();
		}
		else if (strField == gc_Via) {
			// via header can be replicated several times in request or response packet
			if (headers.strVia != "") {
				headers.strVia += " ";
			}
			headers.strVia += parser.QueryLine();
		}
		else if (strField == gc_TransferEncoding) {
			// transfer-encoding header can be replicated several times in request or response packet
			if (headers.strTransferEncoding != "") {
				headers.strTransferEncoding += " ";
			}
			headers.strTransferEncoding += parser.QueryLine();
		}
		else {
			// If it is an unknown header then append headers in 
			// the form: "header: value\r\n"
			
			headers.strRest += strField;
			headers.strRest += " ";
			headers.strRest += parser.QueryLine();
			headers.strRest += "\r\n";
		}

		parser.NextLine();
	}

	return dwRetVal;
}

void CHttpParser::ParseNTLMAuthorization(CHttpHeaders& headers)
{
	INET_PARSER parser(headers.strProxyAuthorization.get_buffer());

	// This header is of the form "Proxy-Authorization: NTLM data\r\n" and we
	// want to change this to copy just "data" to strProxyAuthorization.

	parser.NextToken();
	headers.strProxyAuthorization = parser.QueryLine();
}

