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
//--------------------------------------------------------------------
// Microsoft SOAP Listener ISAPI Implementation 
//
//  isapihdr.h | Main headers include file 
//
//-----------------------------------------------------------------------------

#ifndef ISAPIHDR_H_INCLUDED
#define ISAPIHDR_H_INCLUDED

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


#include "soaputil.h"
#include <stdlib.h>		// exit()
#include <time.h>		// time()
#include <string.h>		// strcat()
#include <stdarg.h>		// va_start
#include <tchar.h>
#include <httpext.h>

#ifdef MYINIT_GUID
#include <initguid.h>
#endif  //MYINIT_GUID

#include "memutil.h"

#include "mssoap.h"         // CLSID_SoapServer class

#include "queue.h"
#include "workers.h"
#include "thdcache.h"
#include "thrdpool.h"
#include "isapiglo.h"
#include "isapistrm.h"
#include "request.h"



#endif
