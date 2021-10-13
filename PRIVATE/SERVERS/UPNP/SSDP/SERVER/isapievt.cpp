//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  File:       I S A P I E V T . C P P
//
//  Contents:  
//
//----------------------------------------------------------------------------

#include <ssdppch.h>
#include <httpext.h>
#include <iphlpapi.h>

#include "http_status.h"
#include "upnp_config.h"

DWORD ControlExtensionProc(LPEXTENSION_CONTROL_BLOCK pecb);
DWORD EventsExtensionProc(LPEXTENSION_CONTROL_BLOCK pecb);
DWORD SubscriptionExtensionProc(LPEXTENSION_CONTROL_BLOCK pecb, DWORD dwIndex);

// The first entrypoint called by IIS.
// Gets the version of the ISAPI control.

extern "C"
BOOL WINAPI GetExtensionVersion(HSE_VERSION_INFO * pver) 
{
	pver->dwExtensionVersion = MAKELONG(1, 0);
	lstrcpyA(pver->lpszExtensionDesc, "UPnP ISAPI Extension");
	TraceTag(ttidInit, "ISAPIEVT: GetExtensionVersion\n");
  
	return TRUE;
}


// Main control handler for the ISAPI extensions. 
extern "C"
DWORD WINAPI HttpExtensionProc(LPEXTENSION_CONTROL_BLOCK pecb) 
{
    if (!cInitialized)
  		return HSE_STATUS_ERROR;
  	
    char    szPort[15];
    char    szRemoteAddress[INTERNET_MAX_HOST_NAME_LENGTH];
    DWORD   dw;
    DWORD   dwIndex;
    bool    bHandleRequest;
    
    // don't handle request unless it comes on one of the interfaces that UPnP is enabled on
    bHandleRequest = false;
    
    if(pecb->GetServerVariable(pecb->ConnID, "REMOTE_ADDR", szRemoteAddress, &(dw = sizeof(szRemoteAddress))))
    {
        struct addrinfo hints = {0}, *ai = NULL;

        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;
		
		if(ERROR_SUCCESS == getaddrinfo(szRemoteAddress, "", &hints, &ai))
		{
			// get index of interface currently used to reach the remote peer
			if(ERROR_SUCCESS == GetBestInterfaceEx(ai->ai_addr, &dwIndex))
			{
			    // check if UPnP is enabled on this interface
			    GetNetworkLock();
    	
	            for(PSSDPNetwork pNet = GetNextNetwork(NULL); pNet != NULL; pNet = GetNextNetwork(pNet))
	                if(pNet->dwIndex == dwIndex)
	                {
	                    bHandleRequest = true;
	                    break;
	                }
    			
			    FreeNetworkLock();
			}
			
			freeaddrinfo(ai);
		}
    }
    
    // don't handle request if it doesn't come on UPnP port
    if(!pecb->GetServerVariable(pecb->ConnID, "SERVER_PORT", szPort, &(dw = sizeof(szPort))) ||
       upnp_config::port() != atoi(szPort))
    {
        bHandleRequest = false;
    }
    
    if(bHandleRequest)
    {
        // NOTIFY - events
	    if(!_stricmp(pecb->lpszMethod, "NOTIFY"))
        {
    	    return EventsExtensionProc(pecb);
        }
	    
	    // POST or M-POST - control
        if (!_stricmp(pecb->lpszMethod, "M-POST") || !_stricmp(pecb->lpszMethod, "POST"))
        {
    	    return ControlExtensionProc(pecb);
        }

	    // SUBSCRIBE or UNSUBSCRIBE - subscription
        if (!_stricmp(pecb->lpszMethod, "SUBSCRIBE") || !_stricmp(pecb->lpszMethod, "UNSUBSCRIBE"))
        {
    	    return SubscriptionExtensionProc(pecb, dwIndex);
        }
    }

	// send response "405 Method Not Allowed"
	pecb->dwHttpStatusCode = HTTP_STATUS_BAD_METHOD;
	
	HSE_SEND_HEADER_EX_INFO hse = {0};

	hse.pszStatus = ce::http_status_string(pecb->dwHttpStatusCode);
	hse.cchStatus = strlen(hse.pszStatus);
	hse.fKeepConn = FALSE;

	pecb->ServerSupportFunction(pecb->ConnID, HSE_REQ_SEND_RESPONSE_HEADER_EX, &hse, NULL, NULL);
	
	return HSE_STATUS_SUCCESS;
}  // end HttpExtensionProc
