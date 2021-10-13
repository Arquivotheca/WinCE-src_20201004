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
//      ComDll.h
//
// Contents:
//
//      Com Dll global function declarations
//
//-----------------------------------------------------------------------------


#ifndef __COMDLL_H_INCLUDED__
#define __COMDLL_H_INCLUDED__


#define DEFINE_STANDARD_COMDLL_FUNCTIONS() \
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv) \
{ \
    return GetClassObject(rclsid, riid, ppv); \
} \
STDAPI DllCanUnloadNow() \
{ \
    return CanUnloadNow(); \
} \


STDAPI GetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv);
STDAPI CanUnloadNow();


#endif //__COMDLL_H_INCLUDED__
