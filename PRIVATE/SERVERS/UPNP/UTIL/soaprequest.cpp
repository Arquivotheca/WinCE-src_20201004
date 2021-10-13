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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include <windows.h>

#include "SoapRequest.h"


// SendMessage
bool SoapRequest::SendMessage(LPCSTR pszUrl, LPCSTR pszAction, LPCWSTR pwszMessage)
{
	// try POST request first
    if(!Open("POST", pszUrl, "HTTP/1.1"))
        return false;

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
