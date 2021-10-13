#ifndef __PARSER_H__
#define __PARSER_H__

#include "global.h"
#include "inetparse.h"
#include "string.hxx"
#include "utils.h"

class CHttpHeaders {
public:
	CHttpHeaders(void)
	{
	}
	
	// Generic headers
	stringi strVersion;
	stringi strProxyConnection;
	stringi strVia;
	stringi strTransferEncoding;

	// Request headers
	string strMethod;
	stringi strURL;
	stringi strPort;
	stringi strHost;
	string strProxyAuthorization;
	string strProxyAuthenticate;

	// Response headers
	stringi strLocation;
	stringi strContentLength;
	stringi strContentType;
	stringi strConnection;
	stringi strStatusCode;
	stringi strReason;
	stringi strProxySupport;

	stringi strRest;

	int GetBufferSize(void);
	void GetRequestBuffer(PBYTE pBuffer);
	void GetResponseBuffer(PBYTE pBuffer);
	void UpdateRequest(void);
	void UpdateResponse(void);
	
private:
	void GetGenericBuffer(PBYTE pBuffer);
	int GetHeaderLength(int cbField, const char* szValue);
	void AppendHeader(PBYTE pBuffer, const char* szField, const char* szValue);
};

class CHttpParser {
public:
	CHttpParser() {}
	
	DWORD ParseRequest(const PBYTE pBuf, int cbBuf, CHttpHeaders& headers);
	DWORD ParseResponse(const PBYTE pBuf, int cbBuf, CHttpHeaders& headers);
	void ParseNTLMAuthorization(CHttpHeaders& headers);

private:
	DWORD ParseGenericHeaders(INET_PARSER& parser, CHttpHeaders& headers);
};


#endif // __PARSER_H__

