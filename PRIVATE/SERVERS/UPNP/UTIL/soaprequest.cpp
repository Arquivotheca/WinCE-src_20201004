//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>

#include "SoapRequest.h"


// SendMessage
bool SoapRequest::SendMessage(LPCSTR pszUrl, LPCSTR pszAction, LPCWSTR pwszMessage)
{
	// try POST request first
    Open("POST", pszUrl, "HTTP/1.1");

    AddHeader("SOAPACTION", pszAction);
    AddHeader("CONTENT-TYPE", "text/xml; charset=\"utf-8\"");
    
    Write(pwszMessage, CP_UTF8);
    
    if(!Send())
        return false;

    if(GetStatus() == HTTP_STATUS_BAD_METHOD)
    {
        // method not allowed
        // try M-POST

        Open("M-POST", pszUrl, "HTTP/1.1");

        AddHeader("MAN", "\"http://schemas.xmlsoap.org/soap/envelope/\"; ns=01\r\n");
        AddHeader("01-SOAPACTION", pszAction);
        AddHeader("CONTENT-TYPE", "text/xml; charset=\"utf-8\""); 
        
        Write(pwszMessage, CP_UTF8);
    
        if(!Send())
            return false;
    }
    
    return true;
}
