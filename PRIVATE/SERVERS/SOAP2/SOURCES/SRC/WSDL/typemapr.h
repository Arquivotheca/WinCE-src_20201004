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
// File:    typemapr.h
// 
// Contents:
//
//  Header File 
//
//		ISoapMapper Interface describtion
//	
//
//-----------------------------------------------------------------------------
#ifndef __TYPEMAPR_H_INCLUDED__
#define __TYPEMAPR_H_INCLUDED__


#include "xsdpars.h"


	
// forward declaration
class CSoapMapper; 


#define CBSTREntry      CKeyedEntry<BSTR, CAutoBSTR>
#define CDoubleEntry    CKeyedEntry<double, double>
#define CBoolEntry      CKeyedEntry<VARIANT_BOOL, VARIANT_BOOL>
#define CBinaryEntry    CKeyedEntry<SAFEARRAY *, SAFEARRAY *>
#define CDecimalEntry   CKeyedEntry<__int64, __int64>
#define CDomEntry       CKeyedObj<IXMLDOMNodeList *, IXMLDOMNodeList *>




///////////////////////////////////////////////////////////////////////
//
//	class CTypeMapper
//
//  description:
//		base class for all used and derived typemappers
///////////////////////////////////////////////////////////////////////
class CTypeMapper : public CDispatchImpl<ISoapTypeMapper2>
{
public:
	CTypeMapper ();  // class constructor
	~CTypeMapper (); // class destructor

// methods from ISoapTypeMapper
    HRESULT STDMETHODCALLTYPE init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType enXSDType);    
    virtual HRESULT STDMETHODCALLTYPE read(IXMLDOMNode * pNode, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar) =0;
    virtual HRESULT STDMETHODCALLTYPE write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar) =0;
    HRESULT STDMETHODCALLTYPE varType(long*pvtType);
    HRESULT STDMETHODCALLTYPE iid(BSTR *pbstrIID);    
 	DECLARE_INTERFACE_MAP;
 
protected:
	HRESULT	ConvertData(VARIANT varInput, VARIANT *pvarOutput, VARTYPE vtTarget);    

protected:
    enXSDType m_enXSDType; 
 
}; 
///////////////////////////////////////////////////////////////////////





///////////////////////////////////////////////////////////////////////
//
//	class CStringMapper
//
//  description:
//
///////////////////////////////////////////////////////////////////////
class CStringMapper : public CTypeMapper
{
public:
    HRESULT STDMETHODCALLTYPE read(IXMLDOMNode * pNode, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar);
    HRESULT STDMETHODCALLTYPE write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar);
}; 
///////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////
//
//	class CBoolMapper
//
//  description:
//
///////////////////////////////////////////////////////////////////////
class CBoolMapper : public CTypeMapper
{
public:
    HRESULT STDMETHODCALLTYPE read(IXMLDOMNode * pNode, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar);
    HRESULT STDMETHODCALLTYPE write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar);
}; 
///////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////
//
//	class CDoubleMapper
//
//  description:
//
///////////////////////////////////////////////////////////////////////
class CDoubleMapper : public CTypeMapper
{
public:
    HRESULT STDMETHODCALLTYPE read(IXMLDOMNode * pNode, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar);
    HRESULT STDMETHODCALLTYPE write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar);
}; 
///////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////
//
//	class CIntegerMapper
//
//  description:
//
///////////////////////////////////////////////////////////////////////
class CDecimalMapper: public CTypeMapper
{
public:
    HRESULT STDMETHODCALLTYPE init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType enXSDType);
    HRESULT STDMETHODCALLTYPE read(IXMLDOMNode * pNode, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar);
    HRESULT STDMETHODCALLTYPE write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar);

protected:
    HRESULT checkConstrains(VARIANT *pVar);

protected:
    long                     m_vtType;
	// constrains
 	XSDConstrains 			m_xsdConstrains;
	DECIMAL                 m_decmaxInclusive;
	DECIMAL                 m_decminInclusive;
	long 					m_lscale;
}; 
///////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////
//
//	class CTimeMapper
//      handles xsdDate, xsdTime and xsdTimeInstant, which are all subtypes
//          of xsdRecurringDuration
//  description:
//
///////////////////////////////////////////////////////////////////////
class CTimeMapper : public CTypeMapper
{
public:
    HRESULT STDMETHODCALLTYPE read(IXMLDOMNode * pNode, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar);
    HRESULT STDMETHODCALLTYPE write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar);

protected:
    HRESULT verifyTimeString(TCHAR *pchTimeString);
    HRESULT adjustTimeZone(DOUBLE *pdblDate, bool fToUTC);
}; 
///////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////
//
//	class CDOMMapper
//
//  description:
//
///////////////////////////////////////////////////////////////////////
class CDOMMapper : public CTypeMapper
{
public:
    HRESULT STDMETHODCALLTYPE read(IXMLDOMNode * pNode, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar);
    HRESULT STDMETHODCALLTYPE write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar);
    HRESULT STDMETHODCALLTYPE iid(BSTR *pbstrIID);


protected:
	HRESULT saveNode(ISoapSerializer *pISoapSerializer, IXMLDOMNode *pNode);
	HRESULT processChildren(ISoapSerializer *pISoapSerializer, IXMLDOMNode *pNode);
	HRESULT saveAttributes(ISoapSerializer *pISoapSerializer, IXMLDOMNode *pNode);
    HRESULT saveList(ISoapSerializer *pISoapSerializer, IXMLDOMNodeList *pNodeList);
}; 
///////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////
//
//	class CURIMapper
//
//  description:
//
///////////////////////////////////////////////////////////////////////
class CURIMapper : public CStringMapper
{
public:
    HRESULT STDMETHODCALLTYPE read(IXMLDOMNode * pNode, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar);
    HRESULT STDMETHODCALLTYPE write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar);
}; 
///////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////
//
//	class CBinaryMapper
//
//  description:
//
///////////////////////////////////////////////////////////////////////
class CBinaryMapper : public CStringMapper
{
public:
    HRESULT STDMETHODCALLTYPE init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType enXSDType);        
    HRESULT STDMETHODCALLTYPE read(IXMLDOMNode * pNode, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar);
    HRESULT STDMETHODCALLTYPE write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar);

protected:
    schemaRevisionNr m_enRevision;    
}; 
///////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////
//
//	class CCDATAMapper
//
//  description:
//
///////////////////////////////////////////////////////////////////////
class CCDATAMapper : public CStringMapper
{
public:
    HRESULT STDMETHODCALLTYPE write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar);

}; 
///////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////
//
//	class CSafearrayMapper
//
//  description:
//
///////////////////////////////////////////////////////////////////////
class CSafearrayMapper : public CTypeMapper
{
public:
    HRESULT STDMETHODCALLTYPE init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType enXSDType);    
    HRESULT STDMETHODCALLTYPE read(IXMLDOMNode * pNode, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar);
    HRESULT STDMETHODCALLTYPE write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags,  VARIANT * pvar);
    HRESULT STDMETHODCALLTYPE varType(long*pvtType);
  
private:
    WCHAR *skipNoise(WCHAR * pc);
    HRESULT ParseArrayDimensions(WCHAR * pcDefDim, long * pDimCount, long ** ppDim);

    CAutoRefc<ISoapTypeMapperFactory> m_ptypeFactory;
    CAutoP<SAFEARRAYBOUND> m_pabound;
    CAutoBSTR m_bstrElementName;
    CAutoBSTR m_bstrTypeURI;
    CAutoBSTR m_bstrTypeName;
    long m_lDimensions;
    BOOL m_soaparraydef;
    schemaRevisionNr m_enRevision;
    long	m_LvtType;

    
}; 
///////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////
//
//	class CAnyTypeMapper
//
//  description:
//
///////////////////////////////////////////////////////////////////////
class CAnyTypeMapper : public CTypeMapper
{
public:
    HRESULT STDMETHODCALLTYPE init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType enXSDType);    
    HRESULT STDMETHODCALLTYPE read(IXMLDOMNode * pNode, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags, VARIANT * pvar);
    HRESULT STDMETHODCALLTYPE write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags, VARIANT * pvar);

private:
    CAutoRefc<ISoapTypeMapperFactory> m_ptypeFactory;
    schemaRevisionNr m_enRevision;    
}; 
///////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////
//
//	class CEmptyTypeMapper
//
//  description:
//
///////////////////////////////////////////////////////////////////////
class CEmptyTypeMapper : public CTypeMapper
{
public:
    HRESULT STDMETHODCALLTYPE init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType enXSDType);    
    HRESULT STDMETHODCALLTYPE read(IXMLDOMNode * pNode, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags, VARIANT * pvar);
    HRESULT STDMETHODCALLTYPE write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, long lFlags, VARIANT * pvar);
    
private:
    schemaRevisionNr m_enRevision;    
}; 
///////////////////////////////////////////////////////////////////////


#endif
