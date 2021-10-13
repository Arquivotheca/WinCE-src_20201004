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
//      ConnectorDll.h
//
// Contents:
//
//      Interface which connector Dlls have
//
//-----------------------------------------------------------------------------


#ifndef __CONNECTORDLL_H_INCLUDED__
#define __CONNECTORDLL_H_INCLUDED__


STDAPI CreateConnector(REFCLSID clsid, IUnknown *pUnkOuter, REFIID riid, void **ppvObject);
STDAPI DllCanUnloadNow();

typedef HRESULT (__stdcall *pfnCreateConnector)(REFCLSID clsid, IUnknown *pUnkOuter, REFIID riid, void **ppvObject);
typedef HRESULT (__stdcall *pfnDllCanUnloadNow)();


#endif //__CONNECTORDLL_H_INCLUDED__
