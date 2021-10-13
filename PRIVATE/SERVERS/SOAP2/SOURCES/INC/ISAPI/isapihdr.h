//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
