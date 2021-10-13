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
//      ClassFactory.h
//
// Contents:
//
//      CClassFactory class declaration
//
//-----------------------------------------------------------------------------


#ifndef __CLASSFACTORY_H_INCLUDED__
#define __CLASSFACTORY_H_INCLUDED__


class CClassFactory : public IClassFactory
{
protected:
    const FactoryProduct *m_pProduct;

protected:
    CClassFactory();
    virtual ~CClassFactory();

public:
    //
    // IClassFactory interface
    //
    STDMETHOD(CreateInstance)(IUnknown *pUnkOuter, REFIID riid, void **ppvObject);
    STDMETHOD(LockServer)(BOOL fLock);

    HRESULT Init(REFCLSID rclsid);

    DECLARE_INTERFACE_MAP;
    DECLARE_FACTORY_MAP;
};


#endif //__CLASSFACTORY_H_INCLUDED__
