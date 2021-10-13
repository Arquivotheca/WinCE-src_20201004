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
//      soapqiob.h
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
