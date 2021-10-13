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
