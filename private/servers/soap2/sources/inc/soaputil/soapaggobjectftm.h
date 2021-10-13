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
//      SoapAggObjectFtm.h
//
// Contents:
//
//      CSoapAggObjectFtm - Aggregated IUnknown implementation with Free Threaded Marshaller
//
//-----------------------------------------------------------------------------


#ifndef __SOAPAGGOBJECTFTM_H_INCLUDED__
#define __SOAPAGGOBJECTFTM_H_INCLUDED__

template <class T> class CSoapAggObjectFtm : public CSoapAggObject<T>
{
private:
    CComPointer<IUnknown> m_pUnkMarshaler;
#ifdef CE_NO_EXCEPTIONS
	BOOL m_fInitFailed;
#endif 

public:
    CSoapAggObjectFtm(IUnknown *pUnkOuter);
    CSoapAggObjectFtm(IUnknown *pUnkOuter, ULONG cRef);

protected:
#ifdef UNDER_CE
	void CeCreateMarshaller();
#endif
    void CreateMarshaller();

public:

    //
    // IInnerUnknown
    //
    STDMETHOD(InnerQueryInterface)(REFIID riid, void **ppvObject);

    static HRESULT WINAPI CreateObject(IUnknown *pUnkOuter, REFIID riid, void **ppvObject);
};


#ifdef UNDER_CE
// Workaround compiler being confused on __try during constructor
template<class T> void CSoapAggObjectFtm<T>::CeCreateMarshaller() {
	__try {
		CreateMarshaller();
		m_fInitFailed = FALSE;
	}
	__except(1) {
		m_fInitFailed = TRUE;
	}	
}
#endif // UNDER_CE

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> CSoapAggObjectFtm<T>::CSoapAggObjectFtm(IUnknown *pUnkOuter)
//
//  parameters:
//          pUnkOuter - outer IUnknown pointer
//  description:
//          Constructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> CSoapAggObjectFtm<T>::CSoapAggObjectFtm(IUnknown *pUnkOuter)
: CSoapAggObject<T>(pUnkOuter)
{
#ifdef UNDER_CE
    CeCreateMarshaller();
#else
	CreateMarshaller();
#endif
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> CSoapAggObjectFtm<T>::CSoapAggObjectFtm(IUnknown *pUnkOuter, ULONG cRef)
//
//  parameters:
//          pUnkOuter - outer IUnknown pointer
//  description:
//          Constructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> CSoapAggObjectFtm<T>::CSoapAggObjectFtm(IUnknown *pUnkOuter, ULONG cRef)
: CSoapAggObject<T>(pUnkOuter, cRef)
{
#ifndef CE_NO_EXCEPTIONS
    CreateMarshaller();
else
	__try {
		CreateMarshaller();
		m_fInitFailed = FALSE;
	}
	__except(1) {
		m_fInitFailed = TRUE;
	}	
#endif 
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> void CSoapAggObjectFtm<T>::CreateMarshaller()
//
//  parameters:
//          
//  description:
//          Creates free threaded marshaller for the particular object
//  returns:
//          
//////////////////////////////////////////////////////////////////////////////////////////////////// 
template<class T> void CSoapAggObjectFtm<T>::CreateMarshaller()
{
    ASSERT(m_pUnkOuter != 0);

#if !defined(UNDER_CE) || defined(DESKTOP_BUILD)
	if (FAILED(CoCreateFreeThreadedMarshaler(m_pUnkOuter, &m_pUnkMarshaler)))
#else
		//Use the E to load CoCreateFTM at runtime... 
		//  so we can have COM/DCOM detection
	if (FAILED(CoCreateFreeThreadedMarshalerE(m_pUnkOuter, &m_pUnkMarshaler)))
#endif
    {
#ifdef CE_NO_EXCEPTIONS
		RaiseException(EXCEPTION_NONCONTINUABLE_EXCEPTION,0,0,0);
#else
        throw; 
#endif
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> STDMETHODIMP CSoapAggObjectFtm<T>::InnerQueryInterface(REFIID riid, void **ppvObject)
//
//  parameters:
//          
//  description:
//          Nondelegating QueryInterface implementation
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> STDMETHODIMP CSoapAggObjectFtm<T>::InnerQueryInterface(REFIID riid, void **ppvObject)
{
#ifdef CE_NO_EXCEPTIONS
	if(m_fInitFailed)
		return E_FAIL;
#endif

    if (riid == IID_IMarshal)
    {
        return m_pUnkMarshaler->QueryInterface(riid, ppvObject);
    }

    return CSoapAggObject<T>::InnerQueryInterface(riid, ppvObject);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> HRESULT WINAPI CSoapAggObjectFtm<T>::CreateObject(IUnknown *pUnkOuter, REFIID riid, void **ppvObject)
//
//  parameters:
//          
//  description:
//          Aggregated object creation routine
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> HRESULT WINAPI CSoapAggObjectFtm<T>::CreateObject(IUnknown *pUnkOuter, REFIID riid, void **ppvObject)
{
    if(! ppvObject)
        return E_INVALIDARG;

    if(pUnkOuter && riid != IID_IUnknown)
        return CLASS_E_NOAGGREGATION;

    CSoapAggObjectFtm<T> *pObj = new CSoapAggObjectFtm<T>(pUnkOuter);
    if (! pObj)
        return E_OUTOFMEMORY;

    HRESULT hr = pObj->InnerQueryInterface(riid, ppvObject);
    if(hr != S_OK)
        delete pObj;
    return hr;
}


#endif //__SOAPAGGOBJECTFTM_H_INCLUDED__
