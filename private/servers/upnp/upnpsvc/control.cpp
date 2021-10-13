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
        TraceTag(ttidControl, "%s: POST Received\n", __FUNCTION__);

        // SOAPACTION header required
        if(!pecb->GetServerVariable(pecb->ConnID, "HTTP_SOAPACTION", pszHeaderBuf, &(dw = sizeof(pszHeaderBuf) - 1)))
        {
            if(ERROR_NO_DATA == GetLastError())
            {
                // missing SOAPACTION header -> 400 Bad request
                m_dwHttpStatus = HTTP_STATUS_BAD_REQUEST;
                TraceTag(ttidControl, "%s: POST:  Error retrieving HTTP_SOAPACTION parameter [%d]\n", __FUNCTION__, m_dwHttpStatus);
                return;
            }
        }
        TraceTag(ttidControl, "%s: POST:  read HTTP_SOAPACTION parameter\n", __FUNCTION__);
    }

    m_pDevTree = FindDevTreeAndServiceIdFromUri(m_pecb->lpszQueryString, &m_pszUDN, &m_pszServiceId);
    TraceTag(ttidControl, "%s: HostedDeviceTree[%08x] UDN[%s], ServiceID[%s]\n", __FUNCTION__, m_pDevTree, m_pszUDN, m_pszServiceId);

    if (!m_pDevTree)
    {
        // invalid UDN or service 
        m_dwHttpStatus = HTTP_STATUS_SERVER_ERROR;
        TraceTag(ttidControl, "%s: Error retrieving HostedDeviceTree [%d]\n", __FUNCTION__, m_dwHttpStatus);
    }
    else if (!(m_pszRequestXML = new WCHAR [m_pecb->cbAvailable + 1]))
    {
        m_dwHttpStatus = HTTP_STATUS_SERVICE_UNAVAIL;
        TraceTag(ttidControl, "%s: Error OOM allocating XML Request Buffer [%d]\n", __FUNCTION__, m_dwHttpStatus);
    }
    else
    {
        // we are relying on IIS behavior that pre-reads upto 48K of request data
        Assert(m_pecb->cbAvailable == m_pecb->cbTotalBytes || m_pecb->cbTotalBytes == 0);

        // need to convert the request body  to UNICODE for the benefit of the XML parser
        // UTF8 encoding
        int cch;

        if(!(cch = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)m_pecb->lpbData, m_pecb->cbAvailable, m_pszRequestXML, m_pecb->cbAvailable)))
        {
            cch = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)m_pecb->lpbData, m_pecb->cbAvailable, m_pszRequestXML, m_pecb->cbAvailable);
        }

        m_pszRequestXML[cch] = 0;   // null terminate
        TraceTag(ttidControl, "%s: Request XML [%S]\n", __FUNCTION__, m_pszRequestXML);
    }

}



ControlRequest::~ControlRequest()
{
    m_SvcCtl.Reserved1 = NULL;  // this helps catch bogus callbacks after we're gone

    delete m_pszServiceId;  // allocated by FindDevTree...
    delete m_pszServiceType;

    if (m_pDevTree)
    {
        m_pDevTree->DecRef();
    }

    delete m_pszRequestXML;
    delete m_pszResponse;
    delete m_pszUDN;
}


//
// ForwardRequest is called to dispatch the parsed and validated request to the device implementation

BOOL
ControlRequest::ForwardRequest()
{
    BOOL fRet;

    TraceTag(ttidControl, "%s: Forwarding Control Request HostedDeviceTree[%08x]\n", __FUNCTION__, m_pDevTree);
    Assert(m_pszServiceId);
    Assert(m_pDevTree);

    // call into the hosted device implementation
    // this will usually cross the process boundary
    // the device is expected to set any output parameters
    // this is accomplished by calling back into SetRawControlResponse
    fRet = m_pDevTree->Control(this);

    // if the actual device has not responded then we need
    // to come up with a default fault response
    if (!m_pszResponse)
    {
        fRet = FALSE;
    }

    if (!fRet)
    {
        m_dwHttpStatus = HTTP_STATUS_SERVER_ERROR;
        TraceTag(ttidControl, "%s: Device has not responded returning default error [%08x] [%d]\n", __FUNCTION__, m_pDevTree, m_dwHttpStatus);
    }

    TraceTag(ttidControl, "%s: Completed Forward Request for HostedDeviceTree[%08x] [%S]\n", __FUNCTION__, m_pDevTree, fRet ? L"TRUE" : L"FALSE");
    return fRet;
}


BOOL
ControlRequest::SendResponse()
{
    HSE_SEND_HEADER_EX_INFO hse;
    CHAR szHeader[256];
    PSTR pszResponse;
    DWORD cbResponse;

    TraceTag(ttidControl, "%s: Sending Control Request Response\n", __FUNCTION__);

    if(m_pszResponse)
    {
        pszResponse = m_pszResponse;
    }
    else
    {
        // If the actual device has not responded then we need
        // to come up with a default fault response
        pszResponse = const_cast<PSTR>(c_szDefaultSOAPFault);
    }

    cbResponse = strlen(pszResponse);
    TraceTag(ttidControl, "%s: Response Size [%d] Response XML\n", __FUNCTION__, cbResponse);

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
        HSE_REQ_SEND_RESPONSE_HEADER_EXV,
        (LPVOID)&hse,
        NULL,
        NULL
        );
    m_pecb->WriteClient(m_pecb->ConnID,pszResponse,&cbResponse,NULL);

    TraceTag(ttidControl, "%s: Finished Sending Control Request Response\n", __FUNCTION__);
    return TRUE;
}



ControlRequest::SetResponse(DWORD dwHttpStatus, PCWSTR pszResp)
{
    if (m_pszResponse)
    {
        Assert(!m_pszResponse);
        return FALSE;
    }

    m_dwHttpStatus = dwHttpStatus;

    UINT cp = CP_UTF8;
    DWORD cbResponse;

    cbResponse = WideCharToMultiByte(cp, 0, pszResp, -1, NULL, 0, NULL, NULL);

    TraceTag(ttidControl, "%s: [%08x] Setting Response\n", __FUNCTION__, this);
    TraceTag(ttidControl, "%s: [%08x] Response[%S]\n", __FUNCTION__, this, pszResp);
    TraceTag(ttidControl, "%s: [%08x] Response Size[%d]\n", __FUNCTION__, this, cbResponse);

    if(!cbResponse)
    {
        if(cbResponse = WideCharToMultiByte(CP_ACP, 0, pszResp, -1, NULL, 0, NULL, NULL))
        {
            cp = CP_ACP;
        }
    }

    if(m_pszResponse = new char[cbResponse])
    {
        WideCharToMultiByte(cp, 0, pszResp, -1, m_pszResponse, cbResponse, NULL, NULL);
    }

    return (m_pszResponse != NULL);
}


// This is the ISAPI extension handler for UPNP control requests
DWORD  ControlExtensionProc(LPEXTENSION_CONTROL_BLOCK pecb)
{
    ControlRequest creq(pecb);

    TraceTag(ttidControl, "%s: Processing Control Request for UDN[%s], ServiceID[%S]\n", __FUNCTION__, creq.UDN(), creq.ServiceId());
    TraceTag(ttidControl, "%s: Processing Control Request for RequestXML[%S]\n", __FUNCTION__, creq.RequestXML());
    Assert(pecb->cbAvailable && pecb->lpbData);

    if (creq.StatusOk())
    {
        creq.ForwardRequest();
    }
    creq.SendResponse();

    return HSE_STATUS_SUCCESS_AND_KEEP_CONN;
}


