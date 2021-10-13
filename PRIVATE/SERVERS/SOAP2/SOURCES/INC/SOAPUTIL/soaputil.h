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
//      soaputil.h
//
// Contents:
//
//      SoapUtil header
//
//----------------------------------------------------------------------------------

#ifndef _SOAPUTIL_H_
#define _SOAPUTIL_H_

#include <windows.h>
#include <ole2.h>
#include <stdio.h>
#include <malloc.h>

#if !defined(UNDER_CE) || defined(DESKTOP_BUILD)
#include <asptlb.h>
#else
DEFINE_GUID(IID_IRequest,0xD97A6DA0,0xA861,0x11cf,0x93,0xAE,0x00,0xA0,0xC9,0x0C,0x2B,0xD8);
DEFINE_GUID(IID_IResponse,0xD97A6DA0,0xA864,0x11cf,0x83,0xBE,0x00,0xA0,0xC9,0x0C,0x2B,0xD8);
#include <asp.h>
#endif
#include <objidl.h>
#include <tchar.h>
//#include "crt\tchar.h"
 
#include "soappragma.h"
#include "mssoap.h"

#include "Debug.h"
#include "rcsoap.h"
#include "registry.h"
#include "autoptr.h"
#include "Utility.h"
#include "CritSect.h"
#include "ComDll.h"
#include "ComPointer.h"
#include "DispatchImpl.h"
#include "SoapObject.h"
#include "SoapAggObject.h"
#include "SoapObjectFtm.h"
#include "SoapAggObjectFtm.h"
#include "linkedlist.h"
#include "typeinfomap.h"
#include "SoapOwnedObject.h"
#include "ClassFactory.h"
#include "FileStream.h"
#include "soaphead.h"
#include "DoubleList.h"
#include "TypedDoubleList.h"
#include "Set.h"
#include "HResultMap.h"
#include "dynarray.h"
#include "cvariant.h"
#include "xmlbase64.h"
#include "errcoll.h"
#include "strhash.h"
#include "objcache.h"
#include "soaphresult.h"
#include "xpathutil.h"
#include "spinlock.h"
#include "Globals.h"
#include "xsdtypes.h"
#include "soapqiob.h"
#include "isaresponse.h"
#include "git.h"
#include "charencoder.h"
#include "encstream.h"
#include "rwlock.h"
#include "UrlUtil.h"
#include "StaticBstr.h"

#endif // _SOAPUTIL_H_
