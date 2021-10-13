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
