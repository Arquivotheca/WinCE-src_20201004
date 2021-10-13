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
//      SoapObjectFtm.h
//
// Contents:
//
//      IUnknown implementation for Soap objects with Free Threaded Marshaller
//
//-----------------------------------------------------------------------------


#ifndef __SOAPOBJECTFTM_H_INCLUDED__
#define __SOAPOBJECTFTM_H_INCLUDED__

template <class T> class CSoapObjectFtm : public CSoapObject<T>
{
protected:
    void CreateMarshaller(void);

protected:
    CComPointer<IUnknown> m_pUnkMarshaler;

#ifdef CE_NO_EXCEPTIONS
	private:
		BOOL m_fInitFailed;
#endif 
	

public:
    CSoapObjectFtm();
    CSoapObjectFtm(ULONG cRef);

public:
    STDMETHOD(QueryInterface)(REFIID riid, void **ppvObject);

    static HRESULT WINAPI CreateObject(IUnknown *pUnkOuter, REFIID riid, void **ppvObject);
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template <class T> CSoapObjectFtm<T>::CSoapObjectFtm()
//
//  parameters:
//          
//  description:
//          Constructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T> CSoapObjectFtm<T>::CSoapObjectFtm()
{
#ifndef CE_NO_EXCEPTIONS
    CreateMarshaller();
#else
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
//  function: template <class T> CSoapObjectFtm<T>::CSoapObjectFtm(int cRef)
//
//  parameters:
//          
//  description:
//          Constructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T> CSoapObjectFtm<T>::CSoapObjectFtm(ULONG cRef)
: CSoapObject<T>(cRef)
{
#ifndef CE_NO_EXCEPTIONS
    CreateMarshaller();
#else
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
//  function: template <class T> void CSoapObjectFtm<T>::CreateMarshaller(void)
//
//  parameters:
//          
//  description:
//          Creates free threaded marshaller for the particular object
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T> void CSoapObjectFtm<T>::CreateMarshaller(void)
{
    IUnknown  *pUnknown;
    long       lTempRef = m_cRef; 
 
    // create freethreaded marshaller
    if (SUCCEEDED(QueryInterface(IID_IUnknown, (void**)&pUnknown)))
    {
       // reset ref count due to the QI up there without getting DELETED
         m_cRef = lTempRef;    

#if !defined(UNDER_CE) || defined(DESKTOP_BUILD)
        if (FAILED(CoCreateFreeThreadedMarshaler(pUnknown, &m_pUnkMarshaler)))
#else
		//Use the E to load CoCreateFTM at runtime... 
		//  so we can have COM/DCOM detection
		if (FAILED(CoCreateFreeThreadedMarshalerE(pUnknown, &m_pUnkMarshaler)))
#endif

        {
#ifdef CE_NO_EXCEPTIONS
		RaiseException(EXCEPTION_NONCONTINUABLE_EXCEPTION,0,0,0);
#else
        throw; 
#endif
        }
    }

}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template <class T> STDMETHODIMP CSoapObjectFtm<T>::QueryInterface(REFIID riid, void **ppvObject)
//
//  parameters:
//          
//  description:
//          Query interface (checks for IMarshal which gets delegated to marshaller)
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T> STDMETHODIMP CSoapObjectFtm<T>::QueryInterface(REFIID riid, void **ppvObject)
{
#ifdef CE_NO_EXCEPTIONS
	if(m_fInitFailed)
		return E_FAIL;	
#endif

    if (riid == IID_IMarshal)
    {
        return(m_pUnkMarshaler->QueryInterface(riid, ppvObject));
    }

    return CSoapObject<T>::QueryInterface(riid, ppvObject);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template <class T> HRESULT CSoapObjectFtm<T>::CreateObject(IUnknown *pUnkOuter, REFIID riid, void **ppvObject)
//
//  parameters:
//          
//  description:
//          CSoapObjectFtm creation routine
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T> HRESULT CSoapObjectFtm<T>::CreateObject(IUnknown *pUnkOuter, REFIID riid, void **ppvObject)
{
    if(! ppvObject)
        return E_INVALIDARG;
    if(pUnkOuter)
        return CLASS_E_NOAGGREGATION;

    *ppvObject = 0;

    CSoapObjectFtm<T> *pObj = new CSoapObjectFtm<T>;
    if (! pObj)
        return E_OUTOFMEMORY;

    HRESULT hr = pObj->QueryInterface(riid, ppvObject);
    if(hr != S_OK)
        delete pObj;
    return hr;
}

#endif //__SOAPOBJECTFTM_H_INCLUDED__
