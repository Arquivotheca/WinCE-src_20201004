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
//      SoapObject.h
//
// Contents:
//
//      IUnknown implementation for Soap objects
//
//-----------------------------------------------------------------------------


#ifndef __SOAPOBJECT_H_INCLUDED__
#define __SOAPOBJECT_H_INCLUDED__

template <class T> class CSoapObject : public T
{
protected:
    ULONG m_cRef;

public:
    CSoapObject();
    CSoapObject(ULONG cRef);
    virtual ~CSoapObject();

public:
    STDMETHOD(QueryInterface)(REFIID riid, void **ppvObject);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

    static HRESULT WINAPI CreateObject(IUnknown *pUnkOuter, REFIID riid, void **ppvObject);
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template <class T> CSoapObject<T>::CSoapObject()
//
//  parameters:
//          
//  description:
//          Constructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T> CSoapObject<T>::CSoapObject()
: m_cRef(0)
{
    OBJECT_CREATED();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template <class T> CSoapObject<T>::CSoapObject(int cRef)
//
//  parameters:
//          
//  description:
//          Constructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T> CSoapObject<T>::CSoapObject(ULONG cRef)
: m_cRef(cRef)
{
    OBJECT_CREATED();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template <class T> CSoapObject<T>::~CSoapObject()
//
//  parameters:
//          
//  description:
//          Destructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T> CSoapObject<T>::~CSoapObject()
{
    OBJECT_DELETED();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template <class T> STDMETHODIMP CSoapObject<T>::QueryInterface(REFIID riid, void **ppvObject)
//
//  parameters:
//          
//  description:
//          Query interface (checks for IMarshal which gets delegated to marshaller)
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T> STDMETHODIMP CSoapObject<T>::QueryInterface(REFIID riid, void **ppvObject)
{
    return QUERYINTERFACE(riid, ppvObject);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template <class T> STDMETHODIMP_(ULONG) CSoapObject<T>::AddRef(void)
//
//  parameters:
//          
//  description:
//          AddRef
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T> STDMETHODIMP_(ULONG) CSoapObject<T>::AddRef(void)
{
	return ::AtomicIncrement(reinterpret_cast<LPLONG>(& m_cRef));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template <class T> STDMETHODIMP_(ULONG) CSoapObject<T>::Release(void)
//
//  parameters:
//          
//  description:
//          Release
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T> STDMETHODIMP_(ULONG) CSoapObject<T>::Release(void)
{
    ULONG cRef = ::AtomicDecrement(reinterpret_cast<LPLONG>(& m_cRef));
    if(! cRef)
    {
        // Artificial reference count
        ::AtomicIncrement(reinterpret_cast<LPLONG>(& m_cRef));
        delete this;
    }
    return cRef;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template <class T> HRESULT CSoapObject<T>::CreateObject(IUnknown *pUnkOuter, REFIID riid, void **ppvObject)
//
//  parameters:
//          
//  description:
//          CSoapObject creation routine
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T> HRESULT CSoapObject<T>::CreateObject(IUnknown *pUnkOuter, REFIID riid, void **ppvObject)
{
    if(! ppvObject)
        return E_INVALIDARG;
    if(pUnkOuter)
        return CLASS_E_NOAGGREGATION;

    *ppvObject = 0;

    CSoapObject<T> *pObj = new CSoapObject<T>;
    if (!pObj)
        return E_OUTOFMEMORY;

    HRESULT hr = pObj->QueryInterface(riid, ppvObject);
    if(hr != S_OK)
        delete pObj;
    return hr;
}


#endif //__SOAPOBJECT_H_INCLUDED__
