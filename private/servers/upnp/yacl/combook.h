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
///////////////////////////////////////////////////////////////////////////////
//
// combook.h - Copyright 1994-2001, Don Box (http://www.donbox.com)
//
// This file includes the whole enchilada and can be brought into
// every translation unit
//
// It also defines one monster-sized macro that implements heap-based IUnknown,
// a default registry table and class factory, and begins an interface table definition:
//
//    IMPLEMENT_COCLASS(ClassName, szCLSID, szFriendlyName, szProgID, szVersionIndependentProgID, szThreadingModel)
//
// Usage:
/*

class Hello : public IHello, public IGoodbye
{
IMPLEMENT_COCLASS(Hello, "{12341234-1234-1234-1234-123412341234}", "Hello Class", "HelloLib.Hello.1", "HelloLib.Hello", "both")
    IMPLEMENTS_INTERFACE(IHello)
    IMPLEMENTS_INTERFACE(IGoodbye)
END_INTERFACE_TABLE()
};

BEGIN_COCLASS_TABLE(Classes)
    IMPLEMENTS_COCLASS(Hello)
END_COCLASS_TABLE()

IMPLEMENT_DLL_MODULE_ROUTINES()
IMPLEMENT_DLL_ENTRY_POINTS(Classes, 0, TRUE)

*/

#ifndef _COMBOOK_H
#define _COMBOOK_H

#include "clstable.h"
#include "inttable.h"
#include "regtable.h"
#if defined(DLLSVC) || defined(EXESVC)
#include "gencf.h"
#endif
#include "impunk.h"
#include "impsrv.h"
#include "smartif.h"

#define IMPLEMENT_COCLASS(ClassName, szCLSID, szFriendlyName, szProgID, szVersionIndependentProgID, szThreadingModel)\
DEFAULT_CLASS_REGISTRY_TABLE(ClassName, szCLSID, szFriendlyName, szProgID, szVersionIndependentProgID, szThreadingModel)\
IMPLEMENT_UNKNOWN(ClassName)\
IMPLEMENT_GENERIC_CLASS_FACTORY(ClassName)\
IMPLEMENT_CREATE_INSTANCE(ClassName)\
BEGIN_INTERFACE_TABLE(ClassName)\



#endif
