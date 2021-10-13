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
//      SoapAggObject.h
//
// Contents:
//
//      CSoapAggObject - Aggregated IUnknown implementation
//
//-----------------------------------------------------------------------------


#ifndef __SOAPAGGOBJECT_H_INCLUDED__
#define __SOAPAGGOBJECT_H_INCLUDED__

interface IInnerUnknown
{
public:
    STDMETHOD(InnerQueryInterface)(REFIID riid, void **ppvObject) = 0;
    STDMETHOD_(ULONG, InnerAddRef)(void) = 0;
    STDMETHOD_(ULONG, InnerRelease)(void) = 0;
};

template <class T> class CSoapAggObject : public IInnerUnknown, public T
{
private:
    ULONG     m_cRef;

protected:
    IUnknown *m_pUnkOuter;

public:
    CSoapAggObject(IUnknown *pUnkOuter);
    CSoapAggObject(IUnknown *pUnkOuter, ULONG cRef);
    virtual ~CSoapAggObject();

public:
    //
    // IUnknown
    //
    STDMETHOD(QueryInterface)(REFIID riid, void **ppvObject);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

    //
    // IInnerUnknown
    //
    STDMETHOD(InnerQueryInterface)(REFIID riid, void **ppvObject);
    STDMETHOD_(ULONG, InnerAddRef)(void);
    STDMETHOD_(ULONG, InnerRelease)(void);

    static HRESULT WINAPI CreateObject(IUnknown *pUnkOuter, REFIID riid, void **ppvObject);
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> CSoapAggObject<T>::CSoapAggObject(IUnknown *pUnkOuter)
//
//  parameters:
//          pUnkOuter - outer IUnknown pointer
//  description:
//          Constructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> CSoapAggObject<T>::CSoapAggObject(IUnknown *pUnkOuter)
: m_cRef(0)
{
    m_pUnkOuter = pUnkOuter ? pUnkOuter : reinterpret_cast<IUnknown*>(static_cast<IInnerUnknown *>(this));

    OBJECT_CREATED();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> CSoapAggObject<T>::CSoapAggObject(IUnknown *pUnkOuter, ULONG cRef)
//
//  parameters:
//          pUnkOuter - outer IUnknown pointer
//  description:
//          Constructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> CSoapAggObject<T>::CSoapAggObject(IUnknown *pUnkOuter, ULONG cRef)
: m_cRef(cRef)
{
    ASSERT(cRef == 0 || cRef == INITIAL_REFERENCE);

    m_pUnkOuter = pUnkOuter ? pUnkOuter : reinterpret_cast<IUnknown*>(static_cast<IInnerUnknown *>(this));

    OBJECT_CREATED();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> CSoapAggObject<T>::~CSoapAggObject()
//
//  parameters:
//          
//  description:
//          Destructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> CSoapAggObject<T>::~CSoapAggObject()
{
    m_pUnkOuter = 0;

    OBJECT_DELETED();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> STDMETHODIMP CSoapAggObject<T>::QueryInterface(REFIID riid, void **ppvObject)
//
//  parameters:
//          
//  description:
//          Delegating QueryInterface method
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> STDMETHODIMP CSoapAggObject<T>::QueryInterface(REFIID riid, void **ppvObject)
{
    ASSERT(m_pUnkOuter);
    return m_pUnkOuter->QueryInterface(riid, ppvObject);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> STDMETHODIMP_(ULONG) CSoapAggObject<T>::AddRef(void)
//
//  parameters:
//          
//  description:
//          Delegating AddRef method
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> STDMETHODIMP_(ULONG) CSoapAggObject<T>::AddRef(void)
{
    ASSERT(m_pUnkOuter);
    return m_pUnkOuter->AddRef();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> STDMETHODIMP_(ULONG) CSoapAggObject<T>::Release(void)
//
//  parameters:
//          
//  description:
//          Delegating Release method
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> STDMETHODIMP_(ULONG) CSoapAggObject<T>::Release(void)
{
    ASSERT(m_pUnkOuter);
    return m_pUnkOuter->Release();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> STDMETHODIMP CSoapAggObject<T>::InnerQueryInterface(REFIID riid, void **ppvObject)
//
//  parameters:
//          
//  description:
//          Nondelegating QueryInterface implementation
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> STDMETHODIMP CSoapAggObject<T>::InnerQueryInterface(REFIID riid, void **ppvObject)
{
    if(riid == IID_IUnknown)
    {
        *ppvObject = reinterpret_cast<IUnknown*>(static_cast<IInnerUnknown*>(this));
        InnerAddRef();
        return S_OK;
    }

    return QUERYINTERFACE(riid, ppvObject);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> STDMETHODIMP_(ULONG) CSoapAggObject<T>::InnerAddRef(void)
//
//  parameters:
//          
//  description:
//          Nonelegating AddRef implementation
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> STDMETHODIMP_(ULONG) CSoapAggObject<T>::InnerAddRef(void)
{
    return ::AtomicIncrement(reinterpret_cast<LPLONG>(& m_cRef));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> STDMETHODIMP_(ULONG) CSoapAggObject<T>::InnerRelease(void)
//
//  parameters:
//          
//  description:
//          Nonelegating Release implementation
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> STDMETHODIMP_(ULONG) CSoapAggObject<T>::InnerRelease(void)
{
    ULONG cRef = ::AtomicDecrement(reinterpret_cast<LPLONG>(& m_cRef));
    if(! cRef)
        delete this;
    return cRef;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> HRESULT WINAPI CSoapAggObject<T>::CreateObject(IUnknown *pUnkOuter, REFIID riid, void **ppvObject)
//
//  parameters:
//          
//  description:
//          Aggregated object creation routine
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> HRESULT WINAPI CSoapAggObject<T>::CreateObject(IUnknown *pUnkOuter, REFIID riid, void **ppvObject)
{
    if(! ppvObject)
        return E_INVALIDARG;

    if(pUnkOuter && riid != IID_IUnknown)
        return CLASS_E_NOAGGREGATION;

    CSoapAggObject<T> *pObj = new CSoapAggObject<T>(pUnkOuter);
    if (! pObj)
        return E_OUTOFMEMORY;

    HRESULT hr = pObj->InnerQueryInterface(riid, ppvObject);
    if(hr != S_OK)
        delete pObj;
    return hr;
}


#endif //__SOAPAGGOBJECT_H_INCLUDED__
