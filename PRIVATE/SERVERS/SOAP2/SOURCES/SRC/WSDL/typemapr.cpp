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
// File:    typemapr.cpp
// 
// Contents:
//
//  implementation file 
//
//		ITypeMapper classes  implemenation
//	
//
//-----------------------------------------------------------------------------
#include "headers.h"
#include "soapmapr.h"
#include "typemapr.h"

TYPEINFOIDS(ISoapTypeMapper2, MSSOAPLib)

BEGIN_INTERFACE_MAP(CTypeMapper)
	ADD_IUNKNOWN(CTypeMapper, ISoapTypeMapper2)
	ADD_INTERFACE(CTypeMapper, ISoapTypeMapper2)
	ADD_INTERFACE(CTypeMapper, ISoapTypeMapper)	
	ADD_INTERFACE(CTypeMapper, IDispatch)	
END_INTERFACE_MAP(CTypeMapper)



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CTypeMapper::CTypeMapper()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CTypeMapper::CTypeMapper()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CTypeMapper::~CTypeMapper()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CTypeMapper::~CTypeMapper()
{
 }
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CTypeMapper::init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType xsdType)
//
//  parameters:
//
//  description:
//      for child overloading (like decimal mapper)
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTypeMapper::init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType xsdType)
{
    m_enXSDType = xsdType;
    return (S_OK);
}

 
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CTypeMapper::varType (long *pvtType)
//
//  parameters:
//
//  description:
//      returns the VariantType of the mapper
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTypeMapper::varType(long *pvtType)
{
    return xsdVariantType(m_enXSDType, pvtType);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CTypeMapper::iid(BSTR *pbstrIID)
//
//  parameters:
//
//  description:
//      iid for custom objects
//  returns: 
//      none for all implementations beside custom mappers
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTypeMapper::iid(BSTR *pbstrIID)
{
    if (!pbstrIID)
        return E_INVALIDARG;

    *pbstrIID = 0; 
    return S_OK;
}



 





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT	CTypeMapper::ConvertData(VARIANT varInput, VARIANT *pvarOutput, VARTYPE vtTarget)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT	CTypeMapper::ConvertData(VARIANT varInput, VARIANT *pvarOutput, VARTYPE vtTarget)
{
	HRESULT hr;

	VariantInit(pvarOutput);
	__try
	{
	hr =  VariantChangeTypeEx(pvarOutput, &varInput, LCID_TOUSE, VARIANT_ALPHABOOL, vtTarget);
    }
    __except(1)
    {
        hr = E_FAIL;
    }
    if (FAILED(hr))
    {
        globalAddError(WSML_IDS_TYPEMAPPERCONVERTFAILED, WSDL_IDS_MAPPER, hr);                            
    }
    
	return (hr);
   
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
