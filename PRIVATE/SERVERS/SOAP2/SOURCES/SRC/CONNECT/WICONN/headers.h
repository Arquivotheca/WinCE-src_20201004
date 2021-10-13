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
