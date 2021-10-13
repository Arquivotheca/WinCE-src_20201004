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
//      IUnknown implementation for Soap objects w/ extension to QI
//
//-----------------------------------------------------------------------------


#ifndef __SOAPQIOB_H_INCLUDED__
#define __SOAPQIOB_H_INCLUDED__

template <class T> class CSoapObjectPrivQI : public CSoapObjectFtm<T>
{

public:
    CSoapObjectPrivQI() : CSoapObjectFtm<T>()
    {
    }
    CSoapObjectPrivQI(ULONG cRef) : CSoapObjectFtm<T>(cRef)
    {
    }

    STDMETHOD(QueryInterface)(REFIID riid, void **ppvObject);
};



////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template <class T> STDMETHODIMP CSoapObjectPrivQI<T>::QueryInterface(REFIID riid, void **ppvObject)
//
//  parameters:
//          
//  description:
//          Query interface (checks for IMarshal which gets delegated to marshaller)
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T> STDMETHODIMP CSoapObjectPrivQI<T>::QueryInterface(REFIID riid, void **ppvObject)
{
    HRESULT hr; 
    
    if (riid == IID_IMarshal)
    {
        return(m_pUnkMarshaler->QueryInterface(riid, ppvObject));
    }
    
    hr = QUERYINTERFACE(riid, ppvObject); 

    if(hr == E_NOINTERFACE)
    {
        return(_privQI(riid, ppvObject));
    }
    return(hr);
}


#endif //__SOAPQIOB_H_INCLUDED__
