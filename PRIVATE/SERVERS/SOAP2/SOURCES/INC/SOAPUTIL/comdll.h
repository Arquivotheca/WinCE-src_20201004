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
