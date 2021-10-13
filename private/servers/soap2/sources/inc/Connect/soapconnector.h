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
//      SoapConnector.h
//
// Contents:
//
//      CSoapConnector class declaration
//
//-----------------------------------------------------------------------------


#ifndef __SOAPCONNECTOR_H_INCLUDED__
#define __SOAPCONNECTOR_H_INCLUDED__

#define DECLARE_PROPERTY_MAP(class) \
protected: \
    static const PropertyElement s_PropertyRiderElements[]; \
    static const PropertyRider   s_PropertyRider


#define BEGIN_PROPERTY_MAP(class) \
const PropertyElement class::s_PropertyRiderElements[] = {

#define ADD_PROPERTY(class, type, name, fname) \
    { name, type, \
        (FnPropertyHandler)(HRESULT (class::*)(VARIANT *pVal))(&class::get_##fname), \
        (FnPropertyHandler)(HRESULT (class::*)(VARIANT *pVal))(&class::put_##fname) },

#define END_PROPERTY_MAP(class) \
}; \
const PropertyRider class::s_PropertyRider = \
{ countof(class::s_PropertyRiderElements), class::s_PropertyRiderElements };

#define GET_PROPERTY(name, pVal) CSoapConnector::get_Property(&s_PropertyRider, name, pVal)
#define PUT_PROPERTY(name, pVal)  CSoapConnector::put_Property(&s_PropertyRider, name, pVal)

class CSoapConnector;

typedef HRESULT (CSoapConnector::*FnPropertyHandler)(VARIANT *);

struct PropertyElement
{
    const WCHAR        *pProperyName;
    VARTYPE             PropertyType;
    FnPropertyHandler   get_Property;
    FnPropertyHandler   put_Property;
};

struct PropertyRider
{
    LONG elc;
    const PropertyElement *elv;
};


class CSoapConnector : public ISoapConnector
{
protected:
    CSoapConnector();
    ~CSoapConnector();

public:
    //
    // ISoapConnector's BeginMessage and Connect
    //

    STDMETHOD(Connect)();
    STDMETHOD(BeginMessage)();

    //
    // ISoapConnector Property handling
    //

    HRESULT get_Property(const PropertyRider *pRider, BSTR pPropertyName, VARIANT *pPropertyValue);
    HRESULT put_Property(const PropertyRider *pRider, BSTR pPropertyName, VARIANT *pPropertyValue);

private:
    static HRESULT FindPropertyElement(const PropertyRider *pRider, const OLECHAR *pPropertyName, const PropertyElement **pElement);
};

#endif //__SOAPCONNECTOR_H_INCLUDED__
