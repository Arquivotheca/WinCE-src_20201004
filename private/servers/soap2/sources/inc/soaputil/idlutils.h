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
//      IdlUtils.h
//
// Contents:
//
//      Idl File utility macros
//
//-----------------------------------------------------------------------------


#ifndef __IDLUTILS_H_INCLUDED__
#define __IDLUTILS_H_INCLUDED__

#ifndef UNDER_CE
#define INTERFACE_ATTRIBUTES(id,ver,help) \
oleautomation, \
uuid(id), \
version(ver), \
local, \
object, \
pointer_default(unique), \
helpstring(help) 
#else
#define INTERFACE_ATTRIBUTES(id,ver,help) \
oleautomation, \
uuid(id), \
version(ver), \
object, \
pointer_default(unique), \
helpstring(help) 
#endif



#ifdef UNDER_CE
#define REAL_LOCAL_INTERFACE(id,ver,help) \
[ \
    INTERFACE_ATTRIBUTES(id,ver,help), local \
]
#endif

#define LOCAL_INTERFACE(id,ver,help) \
[ \
    INTERFACE_ATTRIBUTES(id,ver,help) \
]

#ifdef UNDER_CE
#define NOT_LOCAL_INTERFACE(id,ver,help) \
[ \
    INTERFACE_ATTRIBUTES(id,ver,help) \
]
#endif


#define DUAL_INTERFACE(id,ver,help) \
[ \
    INTERFACE_ATTRIBUTES(id,ver,help), \
    dual \
]

#ifdef UNDER_CE
#define DUAL_INTERFACE_LOCAL(id,ver,help) \
[ \
    INTERFACE_ATTRIBUTES(id,ver,help), \
    dual, local \
]
#endif


#define IDL_LOCAL_SIMPLE_ENTITY(id,ver,help) \
[ \
uuid(id), \
version(ver), \
helpstring(help) \
]

#define LOCAL_COCLASS(id,ver,help)      IDL_LOCAL_SIMPLE_ENTITY(id,ver,help)
#define TYPE_LIBRARY(id,ver,help)       IDL_LOCAL_SIMPLE_ENTITY(id,ver,help)


#endif //__IDLUTILS_H_INCLUDED__
