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
//      SoapOwnedObject.h
//
// Contents:
//
//      Owned object generic implementation
//
//----------------------------------------------------------------------------------

#ifndef __SOAPOWNEDOBJECT_H_INCLUDED__
#define __SOAPOWNEDOBJECT_H_INCLUDED__

template<class T> class CSoapOwnedObject : public CSoapObject<T>
{
public:
    CSoapOwnedObject();
    CSoapOwnedObject(int cRef);
    virtual ~CSoapOwnedObject();

public:
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

    static HRESULT WINAPI CreateObject(IUnknown *pUnkOuter, REFIID riid, void **ppvObject);
};

template<class T> CSoapOwnedObject<T>::CSoapOwnedObject()
: CSoapObject<T>()
{
}

template<class T> CSoapOwnedObject<T>::CSoapOwnedObject(int cRef)
: CSoapObject<T>(cRef)
{
}

template<class T> CSoapOwnedObject<T>::~CSoapOwnedObject()
{
    ASSERT(m_pOwner == 0);
}

template <class T> STDMETHODIMP_(ULONG) CSoapOwnedObject<T>::AddRef(void)
{
    ULONG cRef = ::AtomicIncrement(reinterpret_cast<LPLONG>(& m_cRef));

    if (cRef == 2)
    {
        IUnknown *pOwner = m_pOwner;
        if (pOwner)
        {
            pOwner->AddRef();
        }
    }

    return cRef;
}

template<class T> STDMETHODIMP_(ULONG) CSoapOwnedObject<T>::Release(void)
{
    ULONG cRef = ::AtomicDecrement(reinterpret_cast<LPLONG>(& m_cRef));

    if(! cRef)
    {
        m_pOwner = 0;
        // Artificial reference count per aggregation rules (we aggregate free threaded marshaller)
        ::AtomicIncrement(reinterpret_cast<LPLONG>(& m_cRef));
        delete this;
    }
    else if (cRef == 1)
    {
        IUnknown *pOwner = m_pOwner;
        if (pOwner)
        {
            pOwner->Release();
        }
    }

    return cRef;
}

template <class T> HRESULT CSoapOwnedObject<T>::CreateObject(IUnknown *pUnkOuter, REFIID riid, void **ppvObject)
{
    if (! ppvObject)
        return E_INVALIDARG;
    if (pUnkOuter)
        return CLASS_E_NOAGGREGATION;

    *ppvObject = 0;

    CSoapOwnedObject<T> *pObj = new CSoapOwnedObject<T>;
    if (!pObj)
        return E_OUTOFMEMORY;

    HRESULT hr = pObj->QueryInterface(riid, ppvObject);
    if (hr != S_OK)
        delete pObj;
    return hr;
}

#endif //__SOAPOWNEDOBJECT_H_INCLUDED__
