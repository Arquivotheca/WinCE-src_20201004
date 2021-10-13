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
// gencf.cpp - Copyright 1994-2001, Don Box (http://www.donbox.com)
//
// This file contains the class definition of GenericClassFactory,
// that implements IClassFactory in terms of a function:
//    
//    HRESULT STDAPICALLTYPE CreateInstance(IUnknown*, REFIID, void**)
//

#ifndef _GENCF_CPP
#define _GENCF_CPP

#include <windows.h>
#include "gencf.h"

STDMETHODIMP 
GenericClassFactory::QueryInterface(REFIID riid, void **ppv)
{
    if (riid == IID_IUnknown || riid == IID_IClassFactory)
    {
        *ppv = static_cast<IClassFactory*>(this);
    }
    else
    {
        return (*ppv = 0), E_NOINTERFACE;
    }
    ((IUnknown*)*ppv)->AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG)
GenericClassFactory::AddRef(void)
{
#ifndef EXESVC
    extern void STDAPICALLTYPE ModuleAddRef(void);
    ModuleAddRef();
#endif
    return 2;
}

STDMETHODIMP_(ULONG)
GenericClassFactory::Release(void)
{
#ifndef EXESVC
    extern void STDAPICALLTYPE ModuleRelease(void);
    ModuleRelease();
#endif
    return 1;
}

STDMETHODIMP 
GenericClassFactory::CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv)
{
    extern BOOL STDAPICALLTYPE ModuleIsStopping(void);
    LockServer(TRUE);
    *ppv = 0;
#if defined( _WIN32_DCOM) && defined(CO_E_SERVER_IS_STOPPING)
    HRESULT hr = CO_E_SERVER_IS_STOPPING;
#else
    HRESULT hr = E_FAIL;
#endif
    if (!ModuleIsStopping())
    {
        hr = m_pfnCreateInstance(pUnkOuter, riid, ppv);
    }
    LockServer(FALSE);
    return hr;
}

STDMETHODIMP 
GenericClassFactory::LockServer(BOOL bLock)
{
    extern void STDAPICALLTYPE ModuleAddRef(void);
    extern void STDAPICALLTYPE ModuleRelease(void);
    if (bLock)
    {
        ModuleAddRef();
    }
    else
        ModuleRelease();
    return S_OK;
}

#endif
