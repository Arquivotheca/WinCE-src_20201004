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
//      Precompiled header file
//
//-----------------------------------------------------------------------------

#ifndef __HEADERS_H_INCLUDED__
#define __HEADERS_H_INCLUDED__

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif

#include <windows.h>
#include <stdio.h>
#include <WinInet.h>

#include "HttpLibrary.h"

#include "SoapUtil.h"
#include "mssoap.h"

#include "ConnGuid.h"
#include "MemoryStream.h"
#include "Url.h"
#include "ConnectorStream.h"
#include "StatusMap.h"

#include "AllConn.h"
#include "SoapConnector.h"
#include "HttpConnBase.h"

#endif //__HEADERS_H_INCLUDED__
