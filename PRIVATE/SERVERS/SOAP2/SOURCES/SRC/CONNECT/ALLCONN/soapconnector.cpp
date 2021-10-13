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
//      SoapConnector.cpp
//
// Contents:
//
//      CSoapConnector class implemenation
//
//-----------------------------------------------------------------------------

#include "Headers.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapConnector::CSoapConnector
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CSoapConnector::CSoapConnector()
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapConnector::~CSoapConnector
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CSoapConnector::~CSoapConnector()
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CSoapConnector::Connect()
//
//  parameters:
//          
//  description:
//          Connect with (default) wsdl pointer == 0
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CSoapConnector::Connect()
{
    return ConnectWSDL(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CSoapConnector::BeginMessage()
//
//  parameters:
//          
//  description:
//          BeginMessage with (default) wsdl pointer == 0
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CSoapConnector::BeginMessage()
{
    return BeginMessageWSDL(0);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapConnector::get_Property( const PropertyRider *pRider, BSTR pPropertyName, VARIANT *pPropertyValue)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapConnector::get_Property(const PropertyRider *pRider, BSTR pPropertyName, VARIANT *pPropertyValue)
{
    ASSERT(pRider != 0);

    const PropertyElement *pElement = 0;
    HRESULT                hr       = S_OK;

    CHK_ARG(pPropertyName);
    CHK_ARG(pPropertyValue);

    ::VariantInit(pPropertyValue);

    CHK(FindPropertyElement(pRider, pPropertyName, &pElement));

    ASSERT(pElement != 0);

    CHK((this->*(pElement->get_Property))(pPropertyValue));

    ASSERT(pPropertyValue->vt == pElement->PropertyType);

Cleanup:
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapConnector::put_Property( const PropertyRider *pRider, BSTR pPropertyName, VARIANT *pPropertyValue)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapConnector::put_Property(const PropertyRider *pRider, BSTR pPropertyName, VARIANT *pPropertyValue)
{
    ASSERT(pRider != 0);

    CHK_ARG(pPropertyName);
    CHK_ARG(pPropertyValue);

    const PropertyElement *pElement = 0;
    HRESULT                hr       = S_OK;
    VARIANT                varTemp;

    ::VariantInit(&varTemp);
    CHK(FindPropertyElement(pRider, pPropertyName, &pElement));

    ASSERT(pElement != 0);

    CHK(::VariantChangeType(&varTemp, pPropertyValue, 0, pElement->PropertyType));
    CHK_BOOL(pElement->PropertyType == varTemp.vt, E_INVALIDARG);
    CHK((this->*(pElement->put_Property))(&varTemp));

Cleanup:
    ::VariantClear(&varTemp);
    return hr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapConnector::FindPropertyElement( const PropertyRider *pRider, const OLECHAR *pPropertyName,const PropertyElement **pElement)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapConnector::FindPropertyElement(const PropertyRider *pRider, const OLECHAR *pPropertyName, const PropertyElement **pElement)
{
    ASSERT(pRider);
    ASSERT(pPropertyName);
    ASSERT(pElement && ! *pElement);

    for(int i = 0; i < pRider->elc; i ++)
    {
        const PropertyElement *pElm = pRider->elv + i;
        if(wcscmp(pPropertyName, pElm->pProperyName) == 0)
        {
            *pElement = pElm;
            return S_OK;
        }
    }

    return CONN_E_UNKNOWN_PROPERTY;
}
