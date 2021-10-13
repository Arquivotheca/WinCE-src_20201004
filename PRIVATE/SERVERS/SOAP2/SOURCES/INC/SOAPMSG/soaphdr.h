//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      soaphdr.h
//
// Contents:
//
//      Main headers include file for soapmsg.dll
//
//----------------------------------------------------------------------------------

#ifndef SOAPHDR_H_INCLUDED
#define SOAPHDR_H_INCLUDED

#define _WIN32_WINNT 0x0400

#ifdef DEBUG
#ifndef _DEBUG
#define _DEBUG
#endif
#endif

#ifdef UNICODE
#ifndef _UNICODE
#define _UNICODE
#endif
#endif


#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <tchar.h>

#ifdef MYINIT_GUID
#include <initguid.h>
#endif  //MYINIT_GUID

#include "soaputil.h"
#include "memutil.h"
#include "faultinf.h"

#include "rcsoap.h"
#include "soapglo.h"
#include "SoapMsgIds.h"
#include "mssoapid.h"
#include "mssoap.h"

#include "server.h"
#include "client.h"


#endif
