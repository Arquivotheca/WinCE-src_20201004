//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "pch.h"
#pragma hdrstop
#include <httpext.h>

#include "http_status.h"

// default SOAP error response returned when there is an unknown error
// in the control request, but the request is syntactically ok

const CHAR c_szDefaultSOAPFault [] =
      "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\">\r\n"
      "  <SOAP-ENV:Body>\r\n"
      "    <SOAP-ENV:Fault>\r\n"
      "      <faultcode>SOAP-ENV:Client</faultcode>\r\n"
      "      <faultstring>UPnPError</faultstring>\r\n"
      "      <detail>\r\n"
      "        <UPnPError xmlns=\"urn:schemas-upnp-org:control-1-0\">\r\n"
      "          <errorCode>501</errorCode>\r\n"
      "          <errorDescription>Action Failed</errorDescription>\r\n"
      "        </UPnPError>\r\n"
      "      </detail>\r\n"
      "    </SOAP-ENV:Fault>\r\n"
      "  </SOAP-ENV:Body>\r\n"
      "</SOAP-ENV:Envelope>";


ControlRequest::ControlRequest(LPEXTENSION_CONTROL_BLOCK pecb) :
	m_pecb(pecb),
	m_dwHttpStatus(HTTP_STATUS_OK),
	m_pszServiceId(NULL),
    m_pszUDN(NULL),
	m_pszServiceType(NULL),
	m_pszResponse(NULL),
	m_pszRequestXML(NULL)
{
	char    pszHeaderBuf[100];  
    DWORD   dw;
    
	// POST action
	if(!_stricmp(pecb->lpszMethod, "POST"))
    {
    	// SOAPACTION header required
    	if(!pecb->GetServerVariable(pecb->ConnID, "HTTP_SOAPACTION", pszHeaderBuf, &(dw = sizeof(pszHeaderBuf) - 1)))
	        if(ERROR_NO_DATA == GetLastError())
	        {
	            // missing SOAPACTION header -> 400 Bad request
	            m_dwHttpStatus = HTTP_STATUS_BAD_REQUEST;
	            return;
	        }
    }
	
	// M-POST action
	if(!_stricmp(pecb->lpszMethod, "M-POST"))
    {
    	// MAN header required
    	if(!pecb->GetServerVariable(pecb->ConnID, "HTTP_MAN", pszHeaderBuf, &(dw = sizeof(pszHeaderBuf) - 1)))
	        if(ERROR_NO_DATA == GetLastError())
	        {
	            // missing MAN header -> 400 Bad request
	            m_dwHttpStatus = HTTP_STATUS_BAD_REQUEST;
	            return;
	        }
    }
	
	m_pDevTree = FindDevTreeAndServiceIdFromUri(m_pecb->lpszQueryString, &m_pszUDN, &m_pszServiceId);
	
	if (!m_pDevTree)
	{
		// invalid UDN or service 
		m_dwHttpStatus = HTTP_STATUS_SERVER_ERROR;
	}
	else if (!(m_pszRequestXML = new WCHAR [m_pecb->cbAvailable + 1]))
	{
		m_dwHttpStatus = HTTP_STATUS_SERVICE_UNAVAIL;
	}
	else
	{
		// we are relying on IIS behavior that pre-reads upto 48K of request
		// data
		Assert(m_pecb->cbAvailable == m_pecb->cbTotalBytes || m_pecb->cbTotalBytes == 0);
		// need to convert the request body  to UNICODE for the benefit
		// of the XML parser
		// UTF8 encoding
		if(!MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)m_pecb->lpbData, m_pecb->cbAvailable, m_pszRequestXML, m_pecb->cbAvailable))
            MultiByteToWideChar(CP_ACP, 0, (LPCSTR)m_pecb->lpbData, m_pecb->cbAvailable, m_pszRequestXML, m_pecb->cbAvailable);

		m_pszRequestXML[m_pecb->cbAvailable] = 0;	// null terminate
	}
	
}

ControlRequest::~ControlRequest()
{
	m_SvcCtl.Reserved1 = NULL;	// this helps catch bogus callbacks after we're gone
	if (m_pszServiceId)
		delete m_pszServiceId;	// allocated by FindDevTree...
	if (m_pszServiceType)
		delete m_pszServiceType;
	if (m_pDevTree)
		m_pDevTree->DecRef();
	if (m_pszRequestXML)
	    delete m_pszRequestXML;
	if (m_pszResponse)
	    delete m_pszResponse;
    delete m_pszUDN;
}


//
// ForwardRequest is called to dispatch the parsed and validated request to the device implementation

BOOL
ControlRequest::ForwardRequest()
{
	BOOL fRet;
	Assert(m_pszServiceId);
	// Call into the device implementation. This will usually cross the process boundary
	// The device is expected to set any output parameters during this callback by
	// calling SetRawControlResponse.
	fRet = m_pDevTree->Control(this);
	// If the actual device has not responded then we need
	// to come up with a default fault response
	if (!m_pszResponse)
		fRet = FALSE;
			
	if (!fRet)
	{
		m_dwHttpStatus = HTTP_STATUS_SERVER_ERROR;
	}
	
	return fRet;
}


BOOL
ControlRequest::SendResponse()
{
  	HSE_SEND_HEADER_EX_INFO hse;
  	CHAR szHeader[256];
	PSTR pszResponse;
	DWORD cbResponse;

	if(m_pszResponse)
	    pszResponse = m_pszResponse;
	else
		// If the actual device has not responded then we need
		// to come up with a default fault response
		pszResponse = const_cast<PSTR>(c_szDefaultSOAPFault);
	
	cbResponse = strlen(pszResponse);
    
	hse.pszStatus = ce::http_status_string(m_dwHttpStatus);
	hse.pszHeader = NULL; // Should be empty for HTTP errors.other than 500
	hse.cchStatus = strlen(hse.pszStatus);
	hse.cchHeader = 0;
	hse.fKeepConn = FALSE;

	if (pszResponse && cbResponse)
	{
		hse.cchHeader = _snprintf(szHeader, sizeof(szHeader),
						"Content-Type: text/xml; charset=\"utf-8\"\r\n"
						"Content-Length: %d\r\n"
						"EXT:\r\n"
						"\r\n", cbResponse);
						
		hse.pszHeader = szHeader;
		hse.fKeepConn = TRUE;
	}
	
	m_pecb->dwHttpStatusCode = m_dwHttpStatus;

	m_pecb->ServerSupportFunction(m_pecb->ConnID, 
		HSE_REQ_SEND_RESPONSE_HEADER_EX,
		(LPVOID)&hse,
		NULL,
		NULL
		);
	m_pecb->WriteClient(m_pecb->ConnID,pszResponse,&cbResponse,NULL);
	
	return TRUE;
}

ControlRequest::SetResponse(DWORD dwHttpStatus, PCWSTR pszResp)
{
	Assert(!m_pszResponse);
	
	if (m_pszResponse)
		return FALSE;
	
	m_dwHttpStatus = dwHttpStatus;
	
    UINT cp = CP_UTF8;
    DWORD cbResponse;

	cbResponse = WideCharToMultiByte(cp, 0, pszResp, -1, NULL, 0, NULL, NULL);

    if(!cbResponse)
        if(cbResponse = WideCharToMultiByte(CP_ACP, 0, pszResp, -1, NULL, 0, NULL, NULL))
            cp = CP_ACP;

	if(m_pszResponse = new char[cbResponse])
		WideCharToMultiByte(cp, 0, pszResp, -1, m_pszResponse, cbResponse, NULL, NULL);

	return (m_pszResponse != NULL);
}


// This is the ISAPI extension handler for UPNP control requests
DWORD  ControlExtensionProc(LPEXTENSION_CONTROL_BLOCK pecb)
{
    ControlRequest creq(pecb);

	Assert(pecb->cbAvailable && pecb->lpbData);
	
	if (creq.StatusOk())
	{
		creq.ForwardRequest();
	}
	creq.SendResponse();
	
	
	return HSE_STATUS_SUCCESS;
}


