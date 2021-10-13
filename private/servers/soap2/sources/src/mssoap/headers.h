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
//      headers.h
//
// Contents:
//
//
//-----------------------------------------------------------------------------
#ifndef __HEADERS_H__
#define __HEADERS_H__

#ifdef UNDER_CE
#include "WinCEUtils.h"
#include "msxml2.h"
#endif


#include "soaphdr.h"
#include "msxml2.h"
#include "server.h"
#include "client.h"

#include "..\soapreader\soaprdr.h"

#include "SoapConnector.h"
#include "ConnectorDll.h"
#include "HttpConnector.h"
#include "SoapConnectorFactory.h"
#include "SoapConn.h"
#include "SoapVer.h"

#include "..\soapser\soapser.h"
#include "..\wsdl\wsdlutil.h"
#include "..\wsdl\soapmapr.h"
#include "..\wsdl\ensoapmp.h"
#include "..\wsdl\wsdlread.h"
#include "..\wsdl\wsdlserv.h"
#include "..\wsdl\enwsdlse.h"
#include "..\wsdl\wsdlport.h"
#include "..\wsdl\wsdloper.h"
#include "..\wsdl\enwsdlop.h"
#include "..\wsdl\ensoappt.h"
#include "..\wsdl\typefact.h"

#endif //__HEADERS_H__
