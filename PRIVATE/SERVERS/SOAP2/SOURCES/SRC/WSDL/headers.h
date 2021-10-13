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
// File:    headers.h
// 
// Contents:
//
//  Header File 
//
//
//-----------------------------------------------------------------------------

#ifndef __HEADERS_H__
#define __HEADERS_H__

#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef UNICODE
#define UNICODE
#endif

#ifdef UNDER_CE
#include "WinCEUtils.h"
#include "msxml2.h"
#endif


#include "SoapUtil.h"

#include "msxml2.h"
#include "wsdlguid.h"
#include "mssoap.h"
#include "mssoapid.h"
#include "soapglo.h"

#include "wsdlutil.h"


#endif //__HEADERS_H__
