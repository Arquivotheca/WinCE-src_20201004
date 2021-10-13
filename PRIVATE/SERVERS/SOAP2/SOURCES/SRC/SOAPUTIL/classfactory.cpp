//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      ClassFactory.cpp
//
// Contents:
//
//      implementation of the CClassFactory class.
//
//----------------------------------------------------------------------------------

#include "Headers.h"

BEGIN_INTERFACE_MAP(CClassFactory)
    ADD_IUNKNOWN(CClassFactory, IClassFactory)
    ADD_INTERFACE(CClassFactory, IClassFactory)
END_INTERFACE_MAP(CClassFactory)

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CClassFactory::CClassFactory()
//
//  parameters:
//
//  description:
//        Constructor
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CClassFactory::CClassFactory()
: m_pProduct(0)
{
    OBJECT_CREATED();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CClassFactory::~CClassFactory()
//
//  parameters:
//
//  description:
//        Destructor
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CClassFactory::~CClassFactory()
{
    OBJECT_DELETED();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CClassFactory::CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObject)
//
//  parameters:
//
//  description:
//        Create instance implementation
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CClassFactory::CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObject)
{
    ASSERT(m_pProduct);

    return (*(pUnkOuter ? m_pProduct->pCtorA : m_pProduct->pCtorN))(pUnkOuter, riid, ppvObject);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CClassFactory::LockServer(BOOL fLock)
//
//  parameters:
//
//  description:
//        LockServer implementation
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CClassFactory::LockServer(BOOL fLock)
{
    if(fLock)
        ::InterlockedIncrement(&g_cLock);
    else
        ::InterlockedDecrement(&g_cLock);
    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CClassFactory::Init(REFCLSID rclsid)
//
//  parameters:
//
//  description:
//        Initialization
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CClassFactory::Init(REFCLSID rclsid)
{
    ASSERT(m_pProduct == 0);
    return FIND_FACTORY_PRODUCT(rclsid, &m_pProduct);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDAPI GetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
//
//  parameters:
//          
//  description:
//          Creates ClassFactory
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDAPI GetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
    if(! ppv)
        return E_INVALIDARG;

    *ppv = 0;

    CSoapObject<CClassFactory> *pClassFactory = new CSoapObject<CClassFactory>;
    if (pClassFactory == 0)
    {
        return E_OUTOFMEMORY;
    }

    HRESULT hr = S_OK;

    if(FAILED(hr = pClassFactory->Init(rclsid)) ||
       FAILED(hr = pClassFactory->QueryInterface(riid, ppv)))
    {
        delete pClassFactory;
        return hr;
    }

    return S_OK;
}

