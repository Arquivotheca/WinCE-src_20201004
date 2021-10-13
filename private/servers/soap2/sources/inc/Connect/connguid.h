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
//+----------------------------------------------------------------------------
//
//
// File:
//      ConnGuid.h
//
// Contents:
//
//      Soap Connector GUID and DISPID definitions
//
//-----------------------------------------------------------------------------


#ifndef __CONNGUID_H_INCLUDED__
#define __CONNGUID_H_INCLUDED__


#define UUID_ISOAPCONNECTOR             09BC1FF4-5711-4B93-B01D-DDD826EBD353
#define UUID_ISOAPCONNECTORFACTORY      8905303D-7ED3-40C2-A37F-27A46F98346A
#define UUID_ITESTCONNECTOR             B69183D6-854F-4964-B0DF-ED6065968CB4


#define CLSID_SOAPCONNECTOR             A295EAB4-73AC-4725-A7DE-51047843B421
#define CLSID_TRACECONNECTOR            F1FCA124-F728-46CA-A6DF-7D3F00348907
#define CLSID_HTTPCONNECTOR             6205B8C9-75FF-4623-A50A-88E1F14EAFF2
#define CLSID_WININETCONNECTOR          BED3FEBC-C3EE-4EA8-836B-F7D6D4D2EEDC
#define CLSID_HTTPLIBCONNECTOR          57A6894B-1EEC-4761-B30A-809C80AEAAC4
#define CLSID_XMLHTTPCONNECTOR          88909F6E-60C7-4343-819B-63D8CA91E1AA
#define CLSID_SMTPCONNECTOR             BBBB8C24-894E-4E32-8C62-8E27A113A811
#define CLSID_SOAPCONNECTORFACTORY      4CE546FF-9128-465E-B5C5-5A36CFC2C285
#define CLSID_TESTCONNECTOR             F30F3DB7-3B7A-4799-87D4-1951F1608ECE


#define LIBID_HTTPSOAPCONNECTOR         B1A0EFE5-F5AE-4910-B827-8357893F3EF2
#define LIBID_WININETSOAPCONNECTOR      CA3EEFE4-FB91-4318-AD18-90FCE4CE5C96
#define LIBID_HTTPLIBSOAPCONNECTOR      52B59C59-B2CA-4CB9-A1B4-563A6EA9EB5A
#define LIBID_XMLHTTPSOAPCONNECTOR      40F5C54D-58E0-4FD3-A44E-4484D960BBCD
#define LIBID_TESTCONNECTOR             FFCCFB58-D4D9-4E04-95E1-EA0D5B181EAD


#define DISPID_CONNECTOR_CONNECT_WSDL   1
#define DISPID_CONNECTOR_RESET          2
#define DISPID_CONNECTOR_BEGINMSG_WSDL  3
#define DISPID_CONNECTOR_ENDMSG         4
#define DISPID_CONNECTOR_CONNECT        5
#define DISPID_CONNECTOR_BEGINMSG       6

#define DISPID_CONNECTFACT_CREATE       1



#endif //__CONNGUID_H_INCLUDED__
