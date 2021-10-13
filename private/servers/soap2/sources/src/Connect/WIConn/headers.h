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
//      Headers.h
//
// Contents:
//
//      Precompiled headers file for WIConn
//
//-----------------------------------------------------------------------------

#ifndef __HEADERS_H_INCLUDED__
#define __HEADERS_H_INCLUDED__

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


#include <windows.h>


#ifdef HTTP_LITE
#	include "dubinet.h"
#else
#	include "wininet.h"
#endif

#include <stdio.h>
#include <malloc.h>

#include "msxml2.h"
#include "mssoap.h"
#include "wisc.h"

#include "SoapUtil.h"
#include "ConnGuid.h"

#include "MemoryStream.h"
#include "ConnectorStream.h"
#include "Url.h"
#include "StatusMap.h"
#include "SoapConnector.h"
#include "HttpConnBase.h"

#include "WinInetRequestStream.h"
#include "WinInetResponseStream.h"
#include "WinInetConnector.h"


#endif //__HEADERS_H_INCLUDED__
