//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
//
// File:
//      SoapHResult.h
//
// Contents:
//
//      Contains definitions of SOAP SDK's defined HRESULT values
//
//-----------------------------------------------------------------------------

#ifndef __SOAPHRESULT_H_INCLUDED__
#define __SOAPHRESULT_H_INCLUDED__


#define SOAP_ERROR(err)                  (MAKE_SCODE(SEVERITY_ERROR, FACILITY_CONTROL, err))
#define SOAP_WARNING(wrn)                (MAKE_SCODE(SEVERITY_WARNING, FACILITY_CONTROL, wrn))
#define SOAP_ERROR_(base, ofs)           (SOAP_ERROR(base + ofs))
#define SOAP_WARNING_(base, ofs)         (SOAP_WARNING(base + ofs))


#define CONNECTOR_BASE                   (5000)

#define CONNECTOR_ERROR(err)             (SOAP_ERROR_(CONNECTOR_BASE, err))

#define CONN_E_AMBIGUOUS                 CONNECTOR_ERROR(0)

#define CONN_E_BAD_REQUEST               CONNECTOR_ERROR(50)
#define CONN_E_ACCESS_DENIED             CONNECTOR_ERROR(51)
#define CONN_E_FORBIDDEN                 CONNECTOR_ERROR(52)
#define CONN_E_NOT_FOUND                 CONNECTOR_ERROR(53)
#define CONN_E_BAD_METHOD                CONNECTOR_ERROR(54)
#define CONN_E_REQ_TIMEOUT               CONNECTOR_ERROR(55)
#define CONN_E_CONFLICT                  CONNECTOR_ERROR(56)
#define CONN_E_GONE                      CONNECTOR_ERROR(57)
#define CONN_E_TOO_LARGE                 CONNECTOR_ERROR(58)
#define CONN_E_ADDRESS                   CONNECTOR_ERROR(59)

#define CONN_E_SERVER_ERROR              CONNECTOR_ERROR(100)
#define CONN_E_SRV_NOT_SUPPORTED         CONNECTOR_ERROR(101)
#define CONN_E_BAD_GATEWAY               CONNECTOR_ERROR(102)
#define CONN_E_NOT_AVAILABLE             CONNECTOR_ERROR(103)
#define CONN_E_SRV_TIMEOUT               CONNECTOR_ERROR(104)
#define CONN_E_VER_NOT_SUPPORTED         CONNECTOR_ERROR(105)

#define CONN_E_BAD_CONTENT               CONNECTOR_ERROR(200)

#define CONN_E_CONNECTION_ERROR          CONNECTOR_ERROR(300)
#define CONN_E_BAD_CERTIFICATE_NAME      CONNECTOR_ERROR(301)


#define CONN_E_HTTP_UNSPECIFIED          CONNECTOR_ERROR(400)
#define CONN_E_HTTP_SENDRECV             CONNECTOR_ERROR(401)
#define CONN_E_HTTP_OUTOFMEMORY          E_OUTOFMEMORY
#define CONN_E_HTTP_BAD_REQUEST          CONNECTOR_ERROR(402)
#define CONN_E_HTTP_BAD_RESPONSE         CONNECTOR_ERROR(403)
#define CONN_E_HTTP_BAD_URL              CONNECTOR_ERROR(404)
#define CONN_E_HTTP_DNS_FAILURE          CONNECTOR_ERROR(405)
#define CONN_E_HTTP_CONNECT_FAILED       CONNECTOR_ERROR(406)
#define CONN_E_HTTP_SEND_FAILED          CONNECTOR_ERROR(407)
#define CONN_E_HTTP_RECV_FAILED          CONNECTOR_ERROR(408)
#define CONN_E_HTTP_HOST_NOT_FOUND       CONNECTOR_ERROR(409)
#define CONN_E_HTTP_OVERLOADED           CONNECTOR_ERROR(410)
#define CONN_E_HTTP_FORCED_ABORT         CONNECTOR_ERROR(411)
#define CONN_E_HTTP_NO_RESPONSE          CONNECTOR_ERROR(412)
#define CONN_E_HTTP_BAD_CHUNK            CONNECTOR_ERROR(413)
#define CONN_E_HTTP_PARSE_RESPONSE       CONNECTOR_ERROR(414)

#define CONN_E_HTTP_TIMEOUT              CONNECTOR_ERROR(415)
#define CONN_E_HTTP_CANNOT_USE_PROXY     CONNECTOR_ERROR(416)
#define CONN_E_HTTP_BAD_CERTIFICATE      CONNECTOR_ERROR(417)
#define CONN_E_HTTP_BAD_CERT_AUTHORITY   CONNECTOR_ERROR(418)
#define CONN_E_HTTP_SSL_ERROR            CONNECTOR_ERROR(419)

#define CONN_E_UNKNOWN_PROPERTY          HRESULT_FROM_WIN32(ERROR_UNKNOWN_PROPERTY)


#define CLIENT_BASE                      (6000)
#define CLIENT_ERROR(err)                (SOAP_ERROR_(CLIENT_BASE, err))
#define CLIENT_UNKNOWN_PROPERTY          HRESULT_FROM_WIN32(ERROR_UNKNOWN_PROPERTY)


#define WSDL_BASE                       (7000)
#define WSDL_ERROR(err)                 (SOAP_ERROR_(WSDL_BASE, err))
#define WSDL_MUSTUNDERSTAND             WSDL_ERROR(100)


#endif //__SOAPHRESULT_H_INCLUDED__
