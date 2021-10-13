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
