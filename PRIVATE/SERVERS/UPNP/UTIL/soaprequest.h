//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __SOAP_REQUEST__
#define __SOAP_REQUEST__

#include "HttpRequest.h"
#include "string.hxx"

class SoapRequest : public HttpRequest
{
public:
    bool SendMessage(LPCSTR pszUrl, LPCSTR pszAction, LPCWSTR pwszMessage);
};

#endif // __SOAP_REQUEST__
