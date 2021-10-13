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
