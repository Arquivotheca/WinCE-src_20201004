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

#include "parser.h"
#include "proxy.h"
#include "proxydbg.h"
#include "bldver.h"


CHttpParser* g_pParser;



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
        iSize = this->strMethod.length() + 1 +           // add 1 for space
                this->strURL.length() + 1 +                // add 1 for space
                this->strVersion.length() + 2;            // add 2 for \r\n
    }
    else if (this->strReason == "") {
        // Response packet without a reason
        iSize = this->strVersion.length() + 1 +            // add 1 for space
                this->strStatusCode.length() + 2;        // add 2 for \r\n
    }
    else {
        // Response packet
        iSize = this->strVersion.length() + 1 +            // add 1 for space
                this->strStatusCode.length() + 1 +        // add 1 for space
                this->strReason.length() + 2;            // add 2 for \r\n
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
    iSize += GetHeaderLength(sizeof(gc_Auth), this->strProxyAuthenticate2);
    iSize += GetHeaderLength(sizeof(gc_Authorization), this->strProxyAuthorization);
    iSize += GetHeaderLength(sizeof(gc_ContentLength), this->strContentLength);
    iSize += GetHeaderLength(sizeof(gc_ContentType), this->strContentType);
    iSize += GetHeaderLength(sizeof(gc_ProxySupport), this->strProxySupport);
    iSize += GetHeaderLength(sizeof(gc_TransferEncoding), this->strTransferEncoding);
    iSize += GetHeaderLength(sizeof(gc_Location), this->strLocation);
    iSize += GetHeaderLength(sizeof(gc_MaxForwards), this->strMaxForwards);

    iSize += strRest.length() + 2;                        // add 2 for \r\n

    return iSize;
}

int CHttpHeaders::GetHeaderLength(int cbField, const char* szValue)
{
    int iSize = strlen(szValue);
    if (iSize > 0) {
        iSize += cbField;
        iSize += 2;    // add 2 for \r\n
    }
    return iSize;
}

void CHttpHeaders::UpdateRequest(BOOL fThruProxy)
{
    //
    // Append proxy name to via header
    //

    char szServer[64];
    
    sprintf(szServer, "%s %s", "1.1", (const char*) g_pSettings->strHostName);
    if (this->strVia != "") {
        this->strVia += ", ";
    }
    this->strVia += szServer;
    
    //
    // If no host name is specified, get it from the URL
    //

    int cchHost;
    const char* szAbsURL = this->strURL;
    const char* pchAfterHost = strchr(&szAbsURL[7], '/');
    
    if (this->strHost == "") {
        if (NULL == pchAfterHost) {
            cchHost = strlen(&szAbsURL[7]);
        }
        else {
            cchHost = (pchAfterHost - &szAbsURL[7]);
        }
        strncpy(this->strHost.get_buffer(), &szAbsURL[7], cchHost);
    }    

    if (! fThruProxy) {
        //
        // Change proxy connection header to be connection
        //

        if (this->strConnection == "") {
            this->strConnection = this->strProxyConnection;
        }
        this->strProxyConnection = "";
    
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
    }
    
    //
    // If no version is supplied then request as HTTP/1.0
    //

    if (this->strVersion == "") {
        this->strVersion = gc_HTTP10;
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
    if (this->strStatusCode == "401") {
        this->strProxySupport = gc_SessionBasedAuth;
        this->strProxyConnection = "";
        this->strConnection = gc_ConnKeepAlive;
    }
    else if (this->strStatusCode == "407") {
        // Leave as is
    }
    else if (this->strProxyConnection == "") {
        this->strProxyConnection = gc_ConnKeepAlive;
    }
    
    //
    // Append proxy name to via header
    //

    char szServer[25];    
    StringCchPrintfA(szServer, sizeof(szServer), "1.1 %s", (const char*) g_pSettings->strHostName);
    if (this->strVia != "") {
        this->strVia += ", ";
    }
    this->strVia += szServer;

    //
    // Always respond that we are a 1.1 server
    //

    this->strVersion = gc_HTTP11;
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
    if (this->strProxyAuthenticate2 != "") {
        AppendHeader(pBuffer, gc_Auth, this->strProxyAuthenticate2);        
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
    if (this->strMaxForwards!= "") {
        AppendHeader(pBuffer, gc_MaxForwards, this->strMaxForwards);
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
    const char* pszPort = strchr(headers.strURL, ':');
    if (pszPort) {
        pszPort++;    // Advance past ':'
        const char* pszEnd = strchr(pszPort, '/');
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
        else if (strField == gc_Auth) {
            if (headers.strProxyAuthenticate == "") {
                headers.strProxyAuthenticate = parser.QueryLine();
            }
            else {
                headers.strProxyAuthenticate2 = parser.QueryLine();
            }
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
        else if (strField == gc_ProxySupport) {
            headers.strProxySupport = parser.QueryLine();
        }
        else if (strField == gc_Location) {
            headers.strLocation = parser.QueryLine();
        }
        else if (strField == gc_MaxForwards) {
            headers.strMaxForwards = parser.QueryLine();
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
                headers.strVia += ", ";
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

void CHttpParser::ParseAuthorization(CHttpHeaders& headers, DWORD* pdwAuthType)
{
    ASSERT(pdwAuthType);
    
    INET_PARSER parser(headers.strProxyAuthorization.get_buffer());

    //
    // This header is of the form "<type> <data>\r\n" and we
    // want to change this to copy just "<data>" to strProxyAuthorization.
    //

    CHAR* pszType = parser.QueryToken();
    if (0 == strcmp(pszType, gc_AuthNTLM)) {
        *pdwAuthType = AUTH_TYPE_NTLM;
    }
    else if (0 == strcmp(pszType, gc_AuthBasic)) {
        *pdwAuthType = AUTH_TYPE_BASIC;        
    }
    else {
        *pdwAuthType = AUTH_TYPE_UNKNOWN;
    }

    parser.NextToken();
    headers.strProxyAuthorization = parser.QueryLine();
}

