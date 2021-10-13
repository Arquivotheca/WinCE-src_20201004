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
// File:    typefact.h
// 
// Contents:
//
//  Header File 
//
//		ITypeMapperfactory Interface describtion
//	
//
//-----------------------------------------------------------------------------
#ifndef __TYPEMFACT_H_INCLUDED__
#define __TYPEMFACT_H_INCLUDED__


#include "xsdpars.h"
#include "typemapr.h"

typedef enum 
{
    enMapperUndefined,
    enMapperPrivInit,
    enMapperWorking
} enCustomMapperState;

///////////////////////////////////////////////////////////////////////
//
//	class CCustomMapper
//
//  description:
//      proxy for the custom mapper 
///////////////////////////////////////////////////////////////////////
class CCustomMapper : public CTypeMapper
{
public:    
	CCustomMapper ();  // class constructor
	~CCustomMapper (); // class destructor

// methods from ISoapTypeMapper
    HRESULT STDMETHODCALLTYPE init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType enXSDType);    
    HRESULT STDMETHODCALLTYPE read(IXMLDOMNode * pNode, BSTR bstrEncoding, enEncodingStyle enStyle, LONG lFlags, VARIANT * pvar);
    HRESULT STDMETHODCALLTYPE write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, LONG lFlags, VARIANT * pvar);
    HRESULT STDMETHODCALLTYPE varType(long *pvtType);
    HRESULT STDMETHODCALLTYPE iid(BSTR *pbstrIID);

// method for internal use
    HRESULT privateInit(BSTR bstrProgID, BSTR bstrIID);

protected:
    HRESULT GetTypeMapperInterface(ISoapTypeMapper **ppTypeMapper);
    HRESULT _protectedRead(IXMLDOMNode * pNode, VARIANT * pvar);
    HRESULT _protectedWrite(ISoapSerializer* pSoapSerializer, VARIANT * pvar);
    HRESULT _checkForCachable(void);
    HRESULT _verifyState(enCustomMapperState stateToCheck);
    
protected:    
    
    GIP(ISoapTypeMapper)       m_gipTypeMapper; 
    CAutoRefc<ISoapTypeMapperFactory> m_typeFactory;
    CAutoRefc<IXMLDOMNode> m_pSchemaNode;
    CAutoBSTR   m_bstrProgID; 
    CAutoBSTR   m_bstrIID;
    CLSID       m_clsid;
    long         m_vtType;    
    enXSDType   m_enXSDType;
    CCritSect     m_critSect;        
    enCustomMapperState m_state;
    bool         m_bCheckedForCachable;
    bool         m_bCachable;

};
//////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////
//
//	class CCustomMapperInfo
//
//  description:
//      array of custom mapper information
///////////////////////////////////////////////////////////////////////
class CCustomMapperInfo
{

public:
    CCustomMapperInfo()
    {
        m_bCheckedForCachable = false;
        m_bIsType = true; 
    }
    HRESULT Init(BSTR bstrTypeName, BSTR bstrTypeNameSpace, BSTR bstrProgID, BSTR bstrIID, long dwMapperID, bool bIsType);
    HRESULT GetCustomMapper(ISoapTypeMapper **ppCustomMapper);
    bool     isMatch(BSTR bstrTypeName, BSTR bstrTypeNameSpace, bool bIsType);
        

protected:
    CAutoRefc<ISoapTypeMapper> m_typeMapper; 
    CAutoBSTR   m_bstrTypeName;
    CAutoBSTR   m_bstrNameSpace;
    CAutoBSTR   m_bstrProgID; 
    CAutoBSTR   m_bstrIID;
    bool        m_bIsType;     
    bool        m_bCheckedForCachable;
};







///////////////////////////////////////////////////////////////////////
//
//	class CTypeMapperFactory
//
//  description:
//      implements ISoapTypeMapperFactory
///////////////////////////////////////////////////////////////////////
class CTypeMapperFactory: 
    public CDispatchImpl<ISoapTypeMapperFactory>
{
public:
    CTypeMapperFactory();
    ~CTypeMapperFactory();


    // interface from ISoapTypeMapperFactory
    HRESULT STDMETHODCALLTYPE addSchema (
                    IXMLDOMNode *pSchema           
				);
    
    HRESULT STDMETHODCALLTYPE getElementMapperbyName(
					BSTR bstrElementName,
					BSTR bstrElementNameSpace,
					ISoapTypeMapper **ppSoapTypeMapper
				);

    HRESULT STDMETHODCALLTYPE getTypeMapperbyName(
					BSTR bstrTypeName,
					BSTR bstrTypeNamespace,
					ISoapTypeMapper **ppSoapTypeMapper
				);

    HRESULT STDMETHODCALLTYPE getElementMapper(
                    IXMLDOMNode *pElement,
					ISoapTypeMapper **ppSoapTypeMapper
				);

    HRESULT STDMETHODCALLTYPE getTypeMapper(
                    IXMLDOMNode *pType,
					ISoapTypeMapper **ppSoapTypeMapper
				);

    HRESULT STDMETHODCALLTYPE getMapper(
				enXSDType enMapperType,
				IXMLDOMNode *pSchemaNode,
				ISoapTypeMapper **ppSoapTypeMapper
				);

    HRESULT STDMETHODCALLTYPE addType(
				BSTR bstrTypeName, 
				BSTR bstrTypeNamespace, 
				BSTR bstrProgID
				);

    HRESULT STDMETHODCALLTYPE addElement(
				BSTR bstrTypeName, 
				BSTR bstrTypeNamespace, 
				BSTR bstrProgID 
				);

    HRESULT STDMETHODCALLTYPE addTypeObjectMapper(
				BSTR bstrTypeName, 
				BSTR bstrTypeNamespace, 
				BSTR bstrProgID,
				BSTR bstrIID
				);

    HRESULT STDMETHODCALLTYPE addElementObjectMapper(
				BSTR bstrTypeName, 
				BSTR bstrTypeNamespace, 
				BSTR bstrProgID,
				BSTR bstrIID
				);

    
    DECLARE_INTERFACE_MAP;
protected:    
    HRESULT  createBuildInMapper(ISoapTypeMapper **ppSoapTypeMapper, IXMLDOMNode *pschemaNode, long dwXSDType);    
    HRESULT  _addCustomMapper(BSTR bstrTypeName, 
				BSTR bstrTypeNamespace, 
				BSTR bstrProgID, 
				BSTR bstrIID,
				bool  bIsType);

    HRESULT  findRegisteredTypeMapper(BSTR bstrTypeName, BSTR bstrNameSpace, bool bIsType, IXMLDOMNode *pSchemaNode, ISoapTypeMapper **ppSoapTypeMapper);
    long      _findCustomMapper(BSTR bstrTypeName, BSTR bstrNameSpace, bool bIsType);

    HRESULT  checkForXSDSubclassing(ISoapTypeMapper **ppSoapTypeMapper, IXMLDOMNode * pschemaNode);
    BOOLEAN IsArrayDefinition(IXMLDOMNode *pSchemaNode);
 

private:
    CCustomMapperInfo         **m_ppCustomMapperInfo;       // this only holds the list of custom mappers
    long                   m_lCacheSize; 
    long                   m_lNextMapperID;
    CAutoRefc<IXMLDOMNode> m_pSchemaDocument;
    schemaRevisionNr         m_enRevision;
};


#endif
