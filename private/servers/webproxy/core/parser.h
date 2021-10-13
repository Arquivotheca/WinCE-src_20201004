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
#ifndef __PARSER_H__
#define __PARSER_H__

#include "global.h"
#include "inetparse.h"
#include "string.hxx"
#include "utils.h"

// HTTP header names
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
const char gc_MaxForwards[] = "Max-Forwards:";

// HTTP header values
const char gc_ConnKeepAlive[] = "Keep-Alive";
const char gc_ConnClose[] = "close";
const char gc_AuthNTLM[] = "NTLM";
const char gc_AuthBasic[] = "Basic";
const char gc_HTTP10[] = "HTTP/1.0";
const char gc_HTTP11[] = "HTTP/1.1";
const char gc_SessionBasedAuth[] = "Session-Based-Authentication";
const char gc_MethodPost[] = "POST";
const char gc_MethodConnect[] = "CONNECT";
const char gc_MethodOptions[] = "OPTIONS";
const char gc_TEChunked[] = "chunked";
const char gc_Reason200[] = "OK";
const char gc_Reason200Conn[] = "Connection established";
const char gc_Reason302[] = "Found";
const char gc_Reason400[] = "Bad Request";
const char gc_Reason403[] = "Forbidden";
const char gc_Reason407[] = "Proxy Authentication Required";
const char gc_Reason502[] = "Bad Gateway";


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
    stringi strMaxForwards;
    string strProxyAuthorization;
    string strProxyAuthenticate;
    string strProxyAuthenticate2;

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
    void UpdateRequest(BOOL fThruProxy = FALSE);
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
    void ParseAuthorization(CHttpHeaders& headers, DWORD* pdwAuthType);

private:
    DWORD ParseGenericHeaders(INET_PARSER& parser, CHttpHeaders& headers);
};

extern CHttpParser* g_pParser;

#endif // __PARSER_H__

