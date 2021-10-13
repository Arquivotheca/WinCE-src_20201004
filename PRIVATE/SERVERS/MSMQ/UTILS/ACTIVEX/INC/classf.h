//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

