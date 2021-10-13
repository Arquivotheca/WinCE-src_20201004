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
//=--------------------------------------------------------------------------=
// AutoCF.H
//=--------------------------------------------------------------------------=
//
// header for the ClassFactory Object.  we support IClassFactory and 
// IClassFactory2
//
#ifndef _AUTOCF_H_

#include "olectl.h"

class CClassFactory : public IClassFactory2 {

  public:
    // IUnknown methods
    //
    STDMETHOD(QueryInterface)(REFIID riid, void **ppvObjOut);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

    // IClassFactory methods
    //
    STDMETHOD(CreateInstance)(IUnknown *pUnkOuter, REFIID riid, void **ppbObjOut);
    STDMETHOD(LockServer)(BOOL fLock);

    // IClassFactory2 methods
    //
    STDMETHOD(GetLicInfo)(LICINFO *pLicInfo);
    STDMETHOD(RequestLicKey)(DWORD dwReserved, BSTR *pbstrKey);
    STDMETHOD(CreateInstanceLic)(IUnknown *pUnkOuter, IUnknown *pUnkReserved, REFIID riid, BSTR bstrKey, void **ppvObjOut);

    CClassFactory(int iIndex);
    ~CClassFactory();

  private:
    ULONG m_cRefs;
    int   m_iIndex;
};


// global variable for Locks on our DLL
//
extern LONG g_cLocks;

#define _AUTOCF_H_
#endif // _AUTOCF_H_

