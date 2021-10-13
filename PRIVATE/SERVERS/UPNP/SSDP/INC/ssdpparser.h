//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _SSDPPARSER_
#define _SSDPPARSER_

#include "ssdp.h"

const CHAR OKResponseHeader[40] = "HTTP/1.1 200 OK\r\n\r\n";

VOID InitializeSsdpRequest(SSDP_REQUEST *pRequest);

BOOL ComposeSsdpRequest(SSDP_REQUEST *Source, SSDP_HEADER* pIncludeHeaders, int nHeaders, CHAR **pszBytes);

BOOL ComposeSsdpResponse(SSDP_REQUEST *Source, SSDP_HEADER* pIncludeHeaders, int nHeaders, CHAR **pszBytes);

BOOL ParseSsdpRequest(CHAR * szMessage, SSDP_REQUEST *Result);

BOOL ParseSsdpResponse(CHAR *szMessage, SSDP_REQUEST *Result);

char* ParseHeaders(CHAR *szMessage, SSDP_REQUEST *Result, CHAR* pszHeaderPrefix = NULL);

BOOL CompareSsdpRequest(const PSSDP_REQUEST pRequestA, const PSSDP_REQUEST pRequestB); 

CHAR * ParseRequestLine(CHAR * szMessage, SSDP_REQUEST *Result); 

VOID FreeSsdpRequest(SSDP_REQUEST *pSsdpRequest);

INT GetMaxAgeFromCacheControl(const CHAR *szValue);

VOID PrintSsdpRequest(const SSDP_REQUEST *pssdpRequest);

BOOL CopySsdpRequest(PSSDP_REQUEST Destination, const PSSDP_REQUEST Source);

BOOL ConvertToByebyeNotify(PSSDP_REQUEST pSsdpRequest); 

BOOL ConvertToAliveNotify(PSSDP_REQUEST pSsdpRequest);

CHAR* IsHeadersComplete(const CHAR *szHeaders); 

BOOL VerifySsdpHeaders(SSDP_REQUEST *Result);

BOOL VerifySsdpMethod(CHAR *szMethod, SSDP_REQUEST *Result);

BOOL HasContentBody(PSSDP_REQUEST Result);

BOOL ParseContent(const char *pContent, SSDP_REQUEST *Result); 

// SSDP_MESSAGE functions
PSSDP_MESSAGE InitializeSsdpMessageFromRequest(const PSSDP_REQUEST pSsdpRequest);
PSSDP_MESSAGE CopySsdpMessage(const SSDP_MESSAGE *pSource);
//void FreeSsdpMessage(PSSDP_MESSAGE pSsdpMessage);

BOOL FixURLAddressScopeId(LPCSTR pszURL, DWORD ScopeId, LPSTR pszBuffer, DWORD* pdwSize);

#endif // _SSDPPARSER_


