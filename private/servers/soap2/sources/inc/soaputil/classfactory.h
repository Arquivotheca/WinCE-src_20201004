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
