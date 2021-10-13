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
// gencf.h - Copyright 1994-2001, Don Box (http://www.donbox.com)
//
// This file contains the class definition of GenericClassFactory,
// that implements IClassFactory in terms of a function:
//    
//    HRESULT STDAPICALLTYPE CreateInstance(IUnknown*, REFIID, void**)
//
// There is also a macro that defines a static routine, GetClassObject
// that returns a GenericClassFactory as a singleton:
//
//    IMPLEMENT_GENERIC_CLASS_FACTORY(ClassName)
//
// Usage:
/*
class Hello : public IHello
{
  IMPLEMENT_UNKNOWN(Hello)
  IMPLEMENT_CREATE_INSTANCE(Hello)
  IMPLEMENT_GENERIC_CLASS_FACTORY(Hello)
};
*/

#ifndef _GENCF_H
#define _GENCF_H


typedef HRESULT (STDAPICALLTYPE *CREATE_INSTANCE_PROC)(IUnknown *pUnkOuter, REFIID riid, void **ppv);

class GenericClassFactory : public IClassFactory
{
    CREATE_INSTANCE_PROC    m_pfnCreateInstance;
public:
    GenericClassFactory(CREATE_INSTANCE_PROC pfnCreateInstance);

    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    STDMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv);
    STDMETHODIMP LockServer(BOOL bLock);
};

inline 
GenericClassFactory::GenericClassFactory(CREATE_INSTANCE_PROC pfnCreateInstance)
    : m_pfnCreateInstance(pfnCreateInstance)
{
}


#define IMPLEMENT_GENERIC_CLASS_FACTORY(ClassName)\
static HRESULT STDAPICALLTYPE \
GetClassObject(REFIID riid, void **ppv)\
{\
    static GenericClassFactory cf(CreateInstance);\
    return cf.QueryInterface(riid, ppv);\
}


#endif

