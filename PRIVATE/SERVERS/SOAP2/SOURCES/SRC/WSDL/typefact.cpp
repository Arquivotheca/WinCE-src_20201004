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
// File:    typefact.cpp
// 
// Contents:
//
//  implementation file 
//
//	
//
//-----------------------------------------------------------------------------
#include "headers.h"
#include "typefact.h"

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


const int c_growSize = 16;

TYPEINFOIDS(ISoapTypeMapperFactory, MSSOAPLib)




BEGIN_INTERFACE_MAP(CTypeMapperFactory)
	ADD_IUNKNOWN(CTypeMapperFactory, ISoapTypeMapperFactory)
	ADD_INTERFACE(CTypeMapperFactory, ISoapTypeMapperFactory)		
	ADD_INTERFACE(CTypeMapperFactory, IDispatch)
END_INTERFACE_MAP(CTypeMapperFactory)



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CTypeMapperFactory::CTypeMapperFactory()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CTypeMapperFactory::CTypeMapperFactory()
{
    m_ppCustomMapperInfo = 0; 
    m_lCacheSize = 0; 
    m_lNextMapperID = 0; 
    m_enRevision = enSchemaLast;
    // if this fires, we updated the table with checking...
    ASSERT(enSchemaLast == enSchema2001);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CTypeMapperFactory::~CTypeMapperFactory()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CTypeMapperFactory::~CTypeMapperFactory()
{
    ULONG ulCount;
    CCustomMapperInfo * pCustom; 

    if (m_ppCustomMapperInfo)
    {
        // free the custom mappers
        for (ulCount = 0; ulCount < (ULONG) m_lNextMapperID; ulCount++)
        {
            pCustom = m_ppCustomMapperInfo[ulCount]; 
            if (pCustom)
            {
                delete pCustom;
            }
        }
        
        delete [] m_ppCustomMapperInfo; 
    }    
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: BOOLEAN CTypeMapperFactory::IsArrayDefinition( IXMLDOMNode *pSchemaNode)
//
//  parameters:
//
//  description:
//      will return TRUE if we recognized an array definition
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOLEAN CTypeMapperFactory::IsArrayDefinition(
            IXMLDOMNode *pSchemaNode)
{
    CAutoRefc<IXMLDOMNode> pCheck(NULL);
    HRESULT hr = S_OK;
    XPathState  xp;

    xp.init(pSchemaNode);
    showNode(pSchemaNode);

    {
        schemaRevisionNr revision;
        CAutoFormat autoFormat;
        
        if FAILED(_XSDFindRevision(pSchemaNode, &revision))
            revision = enSchemaLast;

        CHK(autoFormat.sprintf(L"xmlns:schema='%s'", _XSDSchemaNS(revision)));        
        CHK (xp.addNamespace(&autoFormat));         //gives us schema: 
        CHK (xp.addNamespace(g_pwstrXpathDef)); //give us def: wsdl
        
    }

    // there are 5 different array scenarios we will check:
    // (1) <element><complexType><sequence><element>
    // (2) <complexType><sequence><element>
    // (3) <element><complexType><element>
    // (4) <complexType><complexContent><restriction><attribute>
    // (5) <complexType><complexContent><restriction><sequence><attribute>
    // as we are in getElement, only 1 + 3 are checked for at this place

    // let's check on scenario 1
    pSchemaNode->selectSingleNode(L"self::*[schema:complexType[schema:sequence[schema:element[@maxOccurs] and not (schema:element[2])] and not (schema:sequence[2])] and not (schema:complexType[2])]", &pCheck);

    if (pCheck)
        return TRUE;
        
    // let's check on scenario 3
    pSchemaNode->selectSingleNode(L"self::*[schema:complexType[schema:element[@maxOccurs] and not (schema:element[2])] and not (schema:complexType[2])]", &pCheck);

    if (pCheck)
        return TRUE;

            
    // let's check on scenario 2
    pSchemaNode->selectSingleNode(L"self::*[schema:sequence[schema:element[@maxOccurs] and not (schema:element[2])] and not (schema:sequence[2])]", &pCheck);

    if (pCheck)
        return TRUE;

    // let's check on scenario 4
    pSchemaNode->selectSingleNode(L"self::*[schema:complexContent[schema:restriction[schema:attribute[@def:arrayType] and not (schema:attribute[2])] and not (schema:restriction[2])] and not (schema:complexContent[2])]", &pCheck);

    if (pCheck)
        return TRUE;

    // let's check on scenario 5
    pSchemaNode->selectSingleNode(L"self::*[schema:complexContent[schema:restriction [schema:sequence [schema:element[@maxOccurs] and not(schema:element[2])] and not (schema:sequence[2])] and not (schema:restriction[2])] and not (schema:complexContent[2])]", &pCheck);
    if (pCheck)
        return TRUE;
    
Cleanup:
    // no array 
    return FALSE; 
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CTypeMapperFactory::getElementMapper(IXMLDOMNode *pSchemaNode, ISoapTypeMapper ** pptypeMapper )
//
//  parameters:
//
//  description:
//      returns a typemapper based on elementnode
//
//  returns: 
//      S_OK if everything is fine
//      E_FAIL
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTypeMapperFactory::getElementMapper(
                IXMLDOMNode *pSchemaNode,
                ISoapTypeMapper ** pptypeMapper
                )
{
    HRESULT hr = E_INVALIDARG;
    CAutoBSTR   bstrElementNameSpace;
    CAutoBSTR   bstrElementName;
    CAutoBSTR   bstrTemp;
    CAutoBSTR   bstrURI;
    CAutoBSTR   bstrBuffer; 
    TCHAR       achPrefix[_MAX_ATTRIBUTE_LEN];
#ifndef UNDER_CE
    TCHAR       achName[_MAX_ATTRIBUTE_LEN];  
    TCHAR       achURI[_MAX_ATTRIBUTE_LEN];
#endif 

    XPathState  xp;

#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        CHK_BOOL(m_pSchemaDocument, E_FAIL);
        
        CHK(xp.init(m_pSchemaDocument));
        
        CHK(_XSDSetupDefaultXPath(m_pSchemaDocument, m_enRevision));        
        
        if (!pSchemaNode)
        {
            goto Cleanup;
        }

        if (FAILED(_WSDLUtilFindAttribute(pSchemaNode, _T("name"), &bstrElementName)))
        {
            CHK(_WSDLUtilFindAttribute(pSchemaNode, _T("ref"), &bstrElementName));
            // as we have a REF now, find that node and call myself again

            CHK(_WSDLUtilSplitQName(bstrElementName, achPrefix,&bstrBuffer));            
            if (_tcslen(achPrefix) > 0)
            {
                // defined in different schema
                hr = _XSDFindURIForPrefix(pSchemaNode, achPrefix, &bstrURI);
            }
            else
            {
                // get the targetNameSpace attribute
                hr = _XSDFindTargetNameSpace(pSchemaNode, &bstrURI);
            }    
            if (FAILED(hr))
            {
                globalAddError(WSDL_IDS_URIFORQNAMENF, WSDL_IDS_MAPPER, hr, achPrefix, bstrElementName);                
                goto Cleanup;
            }
            
            hr = getElementMapperbyName(bstrBuffer, bstrURI, pptypeMapper );                            
            goto Cleanup;
        }

        // get the 2 things we need: namespace and name
        CHK(_XSDFindTargetNameSpace(pSchemaNode, &bstrElementNameSpace));


        // first verify if there is a buildin/custom one for this
        hr = findRegisteredTypeMapper(bstrElementName, bstrElementNameSpace, false, pSchemaNode, pptypeMapper);
        if (hr == S_FALSE)
        {
            showNode(pSchemaNode);
            if (pSchemaNode)
            {
                // we have the guy.
                hr = _WSDLUtilFindAttribute(pSchemaNode, _T("type"), &bstrTemp);
                if (FAILED(hr)) 
                {
                    // there is the chance we are having an array mapper here
                    if (IsArrayDefinition(pSchemaNode))
                    {
                        // let's assume we have found an array for now
                        hr = createBuildInMapper(pptypeMapper, pSchemaNode, enXSDarray);
                    }
    				else
    				{
    					// this could be an element with inline array ... we can recognize this:
    					CAutoRefc<IXMLDOMNode>		 pChild(NULL);
    					pSchemaNode->get_firstChild(&pChild);

    					if (pChild)
    					{
                            if (IsArrayDefinition(pChild))
                                {
                                    // let's assume we have found an array for now
                                    hr = createBuildInMapper(pptypeMapper, pChild, enXSDarray);
                                }
    					}
    				}
                    if (FAILED(hr))
                    {
                        // NO array, let's try something else, this will create dom mapper as fallback
                        CHK(checkForXSDSubclassing(pptypeMapper, pSchemaNode));
                    }
                }
                else
                {
                    CHK(_WSDLUtilSplitQName(bstrTemp, achPrefix,&bstrBuffer));            
                    hr = _XSDFindURIForPrefix(pSchemaNode, achPrefix, &bstrURI);
                    if (FAILED(hr))
                    {
                        globalAddError(WSDL_IDS_URIFORQNAMENF, WSDL_IDS_MAPPER, hr, achPrefix, bstrTemp);                
                        goto Cleanup;
                    }
                    
                    hr = getTypeMapperbyName(bstrBuffer, bstrURI, pptypeMapper );                            
                }
            }
            else
            {
                globalAddError(WSDL_IDS_MAPPERNODEFINITION, WSDL_IDS_MAPPER, hr, bstrElementName);                  
                hr = E_FAIL;
            }
        
        }    
#ifndef CE_NO_EXCEPTIONS
    }
    catch(...)
    {
        hr = E_UNEXPECTED;
    }
#endif 


Cleanup:
	ASSERT(hr==S_OK);
	return (hr);


}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CTypeMapperFactory::getElementMapperbyName(BSTR bstrElementName, BSTR bstrElementNamespace,
//															ISoapTypeMapper **ppSoapTypeMapper)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTypeMapperFactory::getElementMapperbyName(
					BSTR bstrElementName,
					BSTR bstrElementNamespace,
					ISoapTypeMapper **ppSoapTypeMapper
				)
{
    CAutoRefc<IXMLDOMNode>		 pTargetNode=0;
	CAutoRefc<IXMLDOMNode>		 pTargetSchema=0;
    CAutoFormat                  autoFormat;;
	HRESULT                     hr=E_FAIL;
    XPathState xp;
    
#ifndef CE_NO_EXCEPTIONS
	try
	{
#endif 

        CHK_BOOL(m_pSchemaDocument, E_FAIL);

        CHK(xp.init(m_pSchemaDocument));
        
        CHK(_XSDSetupDefaultXPath(m_pSchemaDocument, m_enRevision));        

		if (_XSDIsPublicSchema(bstrElementNamespace))
	    {
	        // wrong: an element declaration has to point to a private schema
	        hr = E_FAIL;
	        globalAddError(WSDL_IDS_MAPPERNOSCHEMA, WSDL_IDS_MAPPER, hr, bstrElementNamespace, bstrElementName);
	        goto Cleanup;
	    }
	    else
	    {
	        // user schema, look for the node
	        hr = _XSDLFindTargetSchema(bstrElementNamespace,m_pSchemaDocument, &pTargetSchema);
	        if (FAILED(hr)) 
	        {
	            globalAddError(WSDL_IDS_MAPPERNOSCHEMA, WSDL_IDS_MAPPER, hr, bstrElementNamespace, bstrElementName);
	            goto Cleanup;
	        }

	        showNode(pTargetSchema);
	        // now search for the subnode:
	        CHK(autoFormat.sprintf(_T(".//schema:element[@name=\"%s\"]"), bstrElementName));
	        pTargetSchema->selectSingleNode(&autoFormat, &pTargetNode);

	        if (!pTargetNode)
	        {
	            globalAddError(WSDL_IDS_MAPPERNODEFINITION, WSDL_IDS_MAPPER, hr, bstrElementName);                  
	            hr = E_FAIL;
	            goto Cleanup;
	        }   

	        hr = getElementMapper(pTargetNode, ppSoapTypeMapper);
	    }
#ifndef CE_NO_EXCEPTIONS
	}
	catch(...)
	{
	    hr = E_UNEXPECTED;
	}
#endif 

Cleanup:
	ASSERT(hr==S_OK);
	return (hr);

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT  CTypeMapperFactory::getTypeMapper( IXMLDOMNode *pSchemaNode, ISoapTypeMapper ** pptypeMapper )
//
//  parameters:
//
//  description:
//      returns a typemapper based on typenode
//
//  returns: 
//      S_OK if everything is fine
//      E_FAIL
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTypeMapperFactory::getTypeMapper(
                IXMLDOMNode *pSchemaNode,
                ISoapTypeMapper ** pptypeMapper
                )
{
    HRESULT hr = E_INVALIDARG;
    CAutoBSTR   bstrTypeNameSpace;
    CAutoBSTR   bstrNodeName;
    CAutoBSTR   bstrTypeName;
    CAutoBSTR   bstrTemp;
    CAutoBSTR   bstrURI;
    CAutoBSTR   bstrBuffer;
    TCHAR       achPrefix[_MAX_ATTRIBUTE_LEN];
#ifndef UNDER_CE
    TCHAR       achName[_MAX_ATTRIBUTE_LEN];
    TCHAR       achURI[_MAX_ATTRIBUTE_LEN];
#endif 

    XPathState xp;
    
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        CHK_BOOL(m_pSchemaDocument, E_FAIL);

        CHK(xp.init(m_pSchemaDocument));

        CHK(_XSDSetupDefaultXPath(m_pSchemaDocument, m_enRevision));        
        if (!pSchemaNode)
        {
            goto Cleanup;
        }

        // get the 3 things we need: simple/complextype, namespace and name
        CHK(pSchemaNode->get_nodeName(&bstrNodeName));
        CHK(_XSDFindTargetNameSpace(pSchemaNode, &bstrTypeNameSpace));
        CHK(_WSDLUtilFindAttribute(pSchemaNode, _T("name"), &bstrTypeName));

        // first verify if there is a buildin/custom one for this
        hr = findRegisteredTypeMapper(bstrTypeName, bstrTypeNameSpace, true, pSchemaNode, pptypeMapper);
        if (hr == S_FALSE)
        {
            // none found
            if (_tcscmp(bstrNodeName, _T("simpleType"))==0)
            {
                // we can deal with simple types just fine, look up the reference for this guy
                // to do this, we just call getmapperbyname

                // look up the type and get the new typename, and namespace
                hr = _WSDLUtilFindAttribute(pSchemaNode, _T("type"), &bstrTemp);
                if (FAILED(hr))
                {
                    // a simple type with no type attribute, may mean subclassing
                    // this either succeeds: and we have one
                    // or it fails: and then all is lost anyway
                    //  so goto cleanup
                    hr = checkForXSDSubclassing(pptypeMapper, pSchemaNode);
                    goto Cleanup;
                }
            
                CHK(_WSDLUtilSplitQName(bstrTemp, achPrefix,&bstrBuffer));            
                hr = _XSDFindURIForPrefix(pSchemaNode, achPrefix, &bstrURI);
                if (FAILED(hr))
                {
                    globalAddError(WSDL_IDS_URIFORQNAMENF, WSDL_IDS_MAPPER, hr, achPrefix, bstrTemp);                
                    goto Cleanup;
                }
                hr = getTypeMapperbyName(bstrBuffer, bstrURI, pptypeMapper);            
            }
            else
            {
                hr = E_FAIL;
                // create a dom mapper for everything that is not simple
                // BUT : first check for array possibility:
                if (IsArrayDefinition(pSchemaNode))
                {
                    // let's assume we have found an array for now
                    hr = createBuildInMapper(pptypeMapper, pSchemaNode, enXSDarray);                        
                }
                
                if (FAILED(hr))
                {
                    // NO Array, so let's fall back to DOM mapper
                    hr = createBuildInMapper(pptypeMapper, pSchemaNode, enXSDDOM);                                                                        
                }
            }
        }
#ifndef CE_NO_EXCEPTIONS
    }
    catch(...)
    {
        hr = E_UNEXPECTED;
    }
#endif 
  
Cleanup:    
    return hr;    
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT  CTypeMapperFactory::getTypeMapperbyName( BSTR bstrTypeName, BSTR bstrTypeNamespace,
//														ISoapTypeMapper **ppSoapTypeMapper)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTypeMapperFactory::getTypeMapperbyName(
					BSTR bstrTypeName,
					BSTR bstrTypeNamespace,
					ISoapTypeMapper **ppSoapTypeMapper)
{
    HRESULT                      hr=S_FALSE;
	CAutoRefc<IXMLDOMNode>		 pTargetNode=0;
	CAutoRefc<IXMLDOMNode>		 pTargetSchema=0;
	CAutoFormat                  autoFormat;
    CAutoBSTR                    bstrTemp;
    CAutoBSTR                    bstrURI;
    XPathState  xp;
    
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        CHK_BOOL(m_pSchemaDocument, E_FAIL);

        CHK(xp.init(m_pSchemaDocument));

        CHK(_XSDSetupDefaultXPath(m_pSchemaDocument, m_enRevision));        

        hr = S_FALSE;
        
        if (_XSDIsPublicSchema(bstrTypeNamespace))
        {
            hr = findRegisteredTypeMapper(bstrTypeName, bstrTypeNamespace, true, m_pSchemaDocument, ppSoapTypeMapper);
        }    

      	if (hr==S_FALSE)
        {
            // user schema, look for the node
            hr = _XSDLFindTargetSchema(bstrTypeNamespace,m_pSchemaDocument, &pTargetSchema);
            if (FAILED(hr)) 
            {
                globalAddError(WSDL_IDS_MAPPERNOSCHEMA, WSDL_IDS_MAPPER, hr, bstrTypeNamespace, bstrTypeName);    
                goto Cleanup;
            }
            // now search for the subnode:
            CHK(autoFormat.sprintf(_T(".//schema:simpleType[@name=\"%s\"]"), bstrTypeName));
            pTargetSchema->selectSingleNode(&autoFormat, &pTargetNode);
            if (!pTargetNode)
            {
                CHK(autoFormat.sprintf(_T(".//schema:complexType[@name=\"%s\"]"), bstrTypeName));
                pTargetSchema->selectSingleNode(&autoFormat, &pTargetNode);
            }
            if (!pTargetNode)
            {
                hr = E_FAIL;                            
                globalAddError(WSDL_IDS_MAPPERNODEFINITION, WSDL_IDS_MAPPER, hr, bstrTypeName);                  
                goto Cleanup;
                
            }
            hr = getTypeMapper(pTargetNode, ppSoapTypeMapper);
        }
#ifndef CE_NO_EXCEPTIONS
    }
    catch(...)
    {
        hr = E_UNEXPECTED;
    }
#endif 

Cleanup:
	ASSERT(hr==S_OK);
	return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT  CTypeMapperFactory::addSchema( IXMLDOMNode *pSchema )
//
//  parameters:
//
//  description:
//      registeres a new schema document for lookup
//
//  returns: 
//      S_OK if everything is fine
//      S_FALSE if this resulted in a REPLACE
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTypeMapperFactory::addSchema(
                IXMLDOMNode *pSchema
                )
{
    HRESULT hr = E_INVALIDARG; 

#ifndef CE_NO_EXCEPTIONS
    try
#else
    __try
#endif     
    {
        if (!pSchema)
            goto Cleanup;

        
        hr = m_pSchemaDocument ? S_FALSE : S_OK;
        assign(&m_pSchemaDocument, pSchema);
        _XSDFindRevision(pSchema, &m_enRevision);
    }
#ifndef CE_NO_EXCEPTIONS    
    catch(...)
#else
    __except(1)
#endif     
    {
        hr = E_UNEXPECTED;
    }


Cleanup:
    return hr;    
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT  CTypeMapperFactory::addType( BSTR bstrTypeName, BSTR bstrTypeNameSpace, BSTR bstrProgID)
//
//  parameters:
//
//  description:
//      registeres a new type for the typefactory
//
//  returns: 
//      S_OK if everything is fine
//      E_OUTOFMEMORY
//      S_FALSE if this resulted in a REPLACE
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTypeMapperFactory::addType(
				BSTR bstrTypeName, 
				BSTR bstrTypeNameSpace, 
				BSTR bstrProgID
				)
{
    return addTypeObjectMapper(bstrTypeName, bstrTypeNameSpace, bstrProgID, 0);     
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CTypeMapperFactory::addElement( BSTR bstrTypeName,  BSTR bstrTypeNameSpace,  BSTR bstrProgID)
//
//  parameters:
//
//  description:
//      registeres a new element for the typefactory
//
//  returns: 
//      S_OK if everything is fine
//      E_OUTOFMEMORY
//      S_FALSE if this resulted in a REPLACE
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTypeMapperFactory::addElement(
				BSTR bstrTypeName, 
				BSTR bstrTypeNameSpace, 
				BSTR bstrProgID
				)
{
    return addElementObjectMapper(bstrTypeName, bstrTypeNameSpace, bstrProgID, 0); 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CTypeMapperFactory::addElementObjectMapper( BSTR bstrTypeName,  BSTR bstrTypeNameSpace,  BSTR bstrProgID)
//
//  parameters:
//
//  description:
//      registeres a new element for the typefactory
//
//  returns: 
//      S_OK if everything is fine
//      E_OUTOFMEMORY
//      S_FALSE if this resulted in a REPLACE
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTypeMapperFactory::addElementObjectMapper(
				BSTR bstrTypeName, 
				BSTR bstrTypeNameSpace, 
				BSTR bstrProgID,
				BSTR bstrIID
				)
{
    HRESULT hr = S_OK;
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        hr =_addCustomMapper(bstrTypeName, bstrTypeNameSpace, bstrProgID, bstrIID, false); 
#ifndef CE_NO_EXCEPTIONS
    }
    catch(...)
    {
        hr = E_UNEXPECTED;
    }
#endif 
    return hr;
  
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CTypeMapperFactory::addTypeObjectMapper( BSTR bstrTypeName,  BSTR bstrTypeNameSpace,  BSTR bstrProgID)
//
//  parameters:
//
//  description:
//      registeres a new element for the typefactory
//
//  returns: 
//      S_OK if everything is fine
//      E_OUTOFMEMORY
//      S_FALSE if this resulted in a REPLACE
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTypeMapperFactory::addTypeObjectMapper(
				BSTR bstrTypeName, 
				BSTR bstrTypeNameSpace, 
				BSTR bstrProgID,
				BSTR bstrIID
				)
{
    HRESULT hr = S_OK;
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        hr =_addCustomMapper(bstrTypeName, bstrTypeNameSpace, bstrProgID, bstrIID, true); 
#ifndef CE_NO_EXCEPTIONS
    }
    catch(...)
    {
        hr = E_UNEXPECTED;
    }
#endif 
    return hr;
  
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT  CTypeMapperFactory::_addCustomMapper( BSTR bstrTypeName,  BSTR bstrTypeNameSpace, BSTR bstrProgID, 
//												bool bIsType)
//
//  parameters:
//
//  description:
//      registeres a new element or type for the typefactory
//
//  returns: 
//      S_OK if everything is fine
//      E_OUTOFMEMORY
//      E_INVALIDARG it the mapper is already registered
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTypeMapperFactory::_addCustomMapper(
				BSTR bstrTypeName, 
				BSTR bstrTypeNameSpace, 
				BSTR bstrProgID, 
				BSTR bstrIID, 
				bool bIsType
				)
{
    // first do a lookup by typename/namespace to see if this guy is already there
    HRESULT          hr = S_OK;
    long              lMapperIndex;

    lMapperIndex= _findCustomMapper(bstrTypeName, bstrTypeNameSpace, bIsType);

    if (lMapperIndex== (long) enXSDUndefined)
    {
        // need a new one
       lMapperIndex= m_lNextMapperID;        
        if (!m_ppCustomMapperInfo)
        {
            m_lCacheSize = c_growSize;
            m_ppCustomMapperInfo= new CCustomMapperInfo*[m_lCacheSize];
            CHK_BOOL(m_ppCustomMapperInfo, E_OUTOFMEMORY);        
            ZeroMemory(m_ppCustomMapperInfo, sizeof(CCustomMapperInfo*)*m_lCacheSize);
        }
        if (m_lNextMapperID >= m_lCacheSize)
        {
            m_lCacheSize += c_growSize; 
            // need to grow the array
#ifndef UNDER_CE
            m_ppCustomMapperInfo = (CCustomMapperInfo**)realloc(m_ppCustomMapperInfo, m_lCacheSize * sizeof(CCustomMapperInfo*));
            CHK_BOOL(m_ppCustomMapperInfo!=0, E_OUTOFMEMORY);        
#else
            CCustomMapperInfo** pTempCustomMapper = (CCustomMapperInfo**)realloc(m_ppCustomMapperInfo, m_lCacheSize * sizeof(CCustomMapperInfo*));
            if(NULL == pTempCustomMapper)
            {
                delete [] m_ppCustomMapperInfo;
                m_ppCustomMapperInfo = NULL;
                hr = E_OUTOFMEMORY;
                goto Cleanup;
            }            
            m_ppCustomMapperInfo = pTempCustomMapper;
#endif 
            
            // now we need to walk over that memory and 0 it out 
            for (long l=m_lNextMapperID; l<m_lCacheSize; l++)
            {
                m_ppCustomMapperInfo[l] = 0; 
            }
        }

        m_ppCustomMapperInfo[m_lNextMapperID] = new CCustomMapperInfo();
        CHK_BOOL(m_ppCustomMapperInfo[m_lNextMapperID], E_OUTOFMEMORY);        

        // make sure that the mapper ID starts with EndOfBuildin + 1
    	CHK(m_ppCustomMapperInfo[m_lNextMapperID]->Init(bstrTypeName, bstrTypeNameSpace, bstrProgID, bstrIID, enXSDUndefined, bIsType));
    	m_lNextMapperID++;

        
    }
    
    
Cleanup:
    ASSERT(SUCCEEDED(hr));
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CTypeMapperFactory::getMapper( enXSDType xsdType, IXMLDOMNode *pSchemaNode, ISoapTypeMapper **ppSoapTypeMapper)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTypeMapperFactory::getMapper(
				enXSDType xsdType,
				IXMLDOMNode *pSchemaNode,
				ISoapTypeMapper **ppSoapTypeMapper
				)
{
    HRESULT hr;
    CHK(createBuildInMapper(ppSoapTypeMapper, pSchemaNode, xsdType));    
Cleanup:
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////







/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT  CTypeMapperFactory::createBuildInMapper(ISoapTypeMapper **ppSoapTypeMapper, IXMLDOMNode *pSchemaNode, long lXSDType)
//
//  parameters:
//
//  description:
//
//  returns: 
//      S_OK
//      E_OUTOFMEMORY
//      E_FAIL
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT  CTypeMapperFactory::createBuildInMapper(ISoapTypeMapper **ppSoapTypeMapper, IXMLDOMNode *pSchemaNode, long lXSDType)
{
    HRESULT hr = S_OK;
    CAutoRefc<ISoapTypeMapper> pNewMapper; 

    CHK_BOOL((lXSDType < enXSDEndOfBuildin), E_INVALIDARG);
	switch (lXSDType)
	{
		case enXSDDOM:
			pNewMapper = new CSoapObject<CDOMMapper>(INITIAL_REFERENCE);
			break;

        case enXSDcdata:
		case enXSDncname:
		case enXSDname:
		case enXSDnmtoken:
		case enXSDnmtokens:
		case enXSDlanguage:
		case enXSDtoken:
		case enXSDstring:
			pNewMapper = new CSoapObject<CStringMapper>(INITIAL_REFERENCE);
			break;


		case enXSDboolean:
			pNewMapper = new CSoapObject<CBoolMapper>(INITIAL_REFERENCE);
			break;

		case enXSDfloat:
		case enXSDDouble:
			pNewMapper = new CSoapObject<CDoubleMapper>(INITIAL_REFERENCE);
			break;

		case enXSDdecimal:		    
		case enXSDlong:
		case enXSDnonNegativeInteger:
		case enXSDnegativeInteger:
		case enXSDnonpositiveInteger:
		case enXSDinteger:
		case enXSDint:
		case enXSDshort:
		case enXSDbyte:
		case enXSDunsignedLong:
		case enXSDunsignedInt:
		case enXSDunsignedShort:
		case enXSDunsignedByte:
		case enXSDpositiveInteger:
			pNewMapper = new CSoapObject<CDecimalMapper>(INITIAL_REFERENCE);
			break;

		case enXSDbinary:
			// not valid as a direct datatype, the init function checks that...
            pNewMapper = new CSoapObject<CBinaryMapper>(INITIAL_REFERENCE);
			break;
            
		case enXSDmonth:
		case enXSDyear:
		case enXSDcentury:
		case enXSDrecurringDate:
		case enXSDrecurringDay:
		case enXSDtimePeriod:
		case enXSDrecurringDuration:		    
		case enXSDtimeDuration:		    
			pNewMapper = new CSoapObject<CStringMapper>(INITIAL_REFERENCE);
			break;
		case enXSDdate:
		case enXSDtime:		    
		case enXSDtimeInstant:		    
			pNewMapper = new CSoapObject<CTimeMapper>(INITIAL_REFERENCE);
			break;
   		case enXSDuriReference:
			pNewMapper = new CSoapObject<CStringMapper>(INITIAL_REFERENCE);
			break;
		case enXSDid:
			pNewMapper = new CSoapObject<CStringMapper>(INITIAL_REFERENCE);
			break;
		case enXSDidRef:
		case enXSDidRefs:
			pNewMapper = new CSoapObject<CStringMapper>(INITIAL_REFERENCE);
			break;
		case enXSDentity:
		case enXSDentities:
			pNewMapper = new CSoapObject<CStringMapper>(INITIAL_REFERENCE);
			break;
		case enXSDQName:
		case enXSDnotation:
			pNewMapper = new CSoapObject<CStringMapper>(INITIAL_REFERENCE);
			break;
		case enXSDarray:
			pNewMapper = new CSoapObject<CSafearrayMapper>(INITIAL_REFERENCE);
			break;
		case enXSDanyType:
			pNewMapper = new CSoapObject<CAnyTypeMapper>(INITIAL_REFERENCE);
			break;
		case enTKempty:
			pNewMapper = new CSoapObject<CEmptyTypeMapper>(INITIAL_REFERENCE);
			break;
        default:
            ASSERT(0);
            // this would be the custom mapper
            break;
	}


    if (!pNewMapper)
    {
        hr = E_OUTOFMEMORY;
        goto Cleanup;
    }

    CHK(pNewMapper->init(this , pSchemaNode, (enXSDType) lXSDType));
    assign(ppSoapTypeMapper, (ISoapTypeMapper*)pNewMapper);
        

Cleanup:
    ASSERT(SUCCEEDED(hr));
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CTypeMapperFactory::findRegisteredTypeMapper(BSTR bstrTypeName,   BSTR bstrTypeNameSpace,  bool bIsType,  IXMLDOMNode *pSchemaNode,  ISoapTypeMapper **ppSoapTypeMapper)
//
//  parameters:
//
//  description:
//      searches the type/namespace combo in our internal cache
//  returns: 
//      S_OK        -> known mapper
//      S_FAILSE     -> unknown mapper
//      E_FAIL       -> real problem
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTypeMapperFactory::findRegisteredTypeMapper(BSTR bstrTypeName, 
                                                 BSTR bstrTypeNameSpace, 
                                                 bool bIsType, 
                                                 IXMLDOMNode *pSchemaNode,
                                                 ISoapTypeMapper **ppSoapTypeMapper)
{
    HRESULT hr;
    long     lMapperIndex; 

    if (_XSDIsPublicSchema(bstrTypeNameSpace))
    {
        // the type should be a PUBLIC type
        hr = xsdVerifyenXSDType(m_enRevision, bstrTypeName, (enXSDType*)&lMapperIndex);
        if (SUCCEEDED(hr))
        {
            CHK(createBuildInMapper(ppSoapTypeMapper, pSchemaNode, lMapperIndex));
        }    
        else
            hr = S_FALSE;
    }
    else
    {
        // try to find type/namespace combo in our custom mapper list
        lMapperIndex= _findCustomMapper(bstrTypeName, bstrTypeNameSpace, bIsType);
        if (lMapperIndex!= (long) enXSDUndefined)
        {
            CHK(m_ppCustomMapperInfo[lMapperIndex]->GetCustomMapper(ppSoapTypeMapper));
            CHK((*ppSoapTypeMapper)->init(this , pSchemaNode, enXSDUndefined));
        }
        else
        {
            hr = S_FALSE;
        }
    }


Cleanup:
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:long CTypeMapperFactory::_findCustomMapper(BSTR bstrTypeName, BSTR bstrTypeNameSpace, bool bIsType)
//
//  parameters:
//
//  description:
//      searches the custom mapper list
//  returns: 
//      dwMapperID -> enXSDUndefined or a valid entry
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
long CTypeMapperFactory::_findCustomMapper(BSTR bstrTypeName, BSTR bstrTypeNameSpace, bool bIsType)
{
    long lRet = (DWORD) enXSDUndefined;
    long lIteration;

    if (m_lCacheSize > 0)
    {
        for (lIteration= 0; lIteration< m_lNextMapperID; lIteration++)
        {
            if (m_ppCustomMapperInfo[lIteration]->isMatch(bstrTypeName, bstrTypeNameSpace, bIsType))
            {
                lRet = lIteration;
                break;
            }
        }
    }
    
    return (lRet);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:HRESULT CTypeMapperFactory::checkForXSDSubclassing(ISoapTypeMapper **ppSoapTypeMapper, IXMLDOMNode * pschemaNode)
//
//  parameters:
//
//  description:
//      takes a current schema node and checks for an inline restriction base subclass
//  returns: 
//      S_OK -> found a valid mapper
//      E_FAIL -> not an understandable node for us
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTypeMapperFactory::checkForXSDSubclassing(ISoapTypeMapper **ppSoapTypeMapper, IXMLDOMNode * pschemaNode)
{
    HRESULT hr;
    CAutoRefc<IXMLDOMNode> pNode;
    CAutoBSTR               bstrTypeBase;
    long                    lMapperIndex; 

    showNode(pschemaNode);
    CHK(pschemaNode->selectSingleNode(_T("descendant-or-self::schema:simpleType/schema:restriction"), &pNode));

    if (pNode)
    {
        CHK(_WSDLUtilFindAttribute(pNode, _T("base"), &bstrTypeBase));

        // so we found a base type. If this is a VALID and UNDERSTOOD basetype, we can use this guy
    // first verify if there is a buildin/custom one for this
        CHK(xsdVerifyenXSDType(m_enRevision, bstrTypeBase, (enXSDType*)&lMapperIndex));
        CHK(createBuildInMapper(ppSoapTypeMapper, pschemaNode, lMapperIndex));
    }
    else
    {
        CHK(createBuildInMapper(ppSoapTypeMapper, pschemaNode, enXSDDOM));        
    }

Cleanup:
    return hr;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CCustomMapperInfo::Init(BSTR bstrTypeName, BSTR bstrNameSpace, BSTR bstrProgID, long lMapperID, bool bIsType)
//
//  parameters:
//
//  description:
//      just stores the passed in info from addType
//  returns: 
//      S_OK
//      E_OUTOFMEMORY
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CCustomMapperInfo::Init(BSTR bstrTypeName, BSTR bstrNameSpace, BSTR bstrProgID, BSTR bstrIID, 
            long lMapperID, bool bIsType)
{
    HRESULT hr = E_OUTOFMEMORY;
    
    CHK(m_bstrNameSpace.Assign(bstrNameSpace));
    CHK(m_bstrTypeName.Assign(bstrTypeName));
    CHK(m_bstrProgID.Assign(bstrProgID));
    CHK(m_bstrIID.Assign(bstrIID));

    m_bIsType = bIsType;

    // if we hold an interface currently, release it.

    m_typeMapper.Clear();

Cleanup:
    ASSERT(hr==S_OK);
    return hr; 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CCustomMapperInfo::GetCustomMapper(CCustomMapper **ppCustomMapper)
//
//  parameters:
//
//  description:
//      retrieves the interface pointer, takes caching into account.
//  returns: 
//      S_OK
//      E_OUTOFMEMORY
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CCustomMapperInfo::GetCustomMapper(ISoapTypeMapper **ppCustomMapper)
{
    HRESULT hr;
    CAutoRefc<CCustomMapper> pMapper;

    pMapper = new CSoapObject<CCustomMapper>(INITIAL_REFERENCE);
    CHK_BOOL(pMapper, E_OUTOFMEMORY);
    CHK(pMapper->privateInit(m_bstrProgID, m_bstrIID));

    // caller takes ownership of addrefed object
    *ppCustomMapper = pMapper.PvReturn();
        
Cleanup:
    return hr;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:  bool CCustomMapperInfo::isMatch(BSTR bstrTypeName, BSTR bstrTypeNameSpace, bool  bIsType)
//
//  parameters:
//
//  description:
//      compares the type and namespace
//  returns: 
//      true -> correct guy
//      false -> not
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCustomMapperInfo::isMatch(BSTR bstrTypeName, BSTR bstrTypeNameSpace, bool bIsType)
{
    int iRet=1; 

    // first they both have to be type or element
    if (bIsType == m_bIsType)
    {
        if (bstrTypeName && m_bstrTypeName)
        {
            iRet = _tcscmp(bstrTypeName, m_bstrTypeName);
        }
        if (iRet == 0 && bstrTypeNameSpace && m_bstrNameSpace)
        {
            iRet = _tcscmp(bstrTypeNameSpace, m_bstrNameSpace);
        }
    }    
    return (iRet==0 ? true : false); 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CCustomMapper::CCustomMapper()
//
//  parameters:
//
//  description:
//      constructor
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CCustomMapper::CCustomMapper()
{
    m_state = enMapperUndefined; 
    m_bCachable = false;    
    m_bCheckedForCachable = false;
    m_vtType = VT_ERROR;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CCustomMapper::~CCustomMapper()
//
//  parameters:
//
//  description:
//      detructor
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CCustomMapper::~CCustomMapper()
{
    if (m_state > enMapperUndefined)
    {
        if (m_bCachable==false)
        {
            m_critSect.Delete();
        }
        m_gipTypeMapper.Unglobalize();    
    }    
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CCustomMapper::_verifyState(enCustomMapperState stateToCheck)
//
//  parameters:
//
//  description:
//      verifies that the custom mapper is in properstate
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CCustomMapper::_verifyState(enCustomMapperState stateToCheck)
{
    HRESULT hr = S_OK;
    CHK_BOOL(m_state >= stateToCheck, E_UNEXPECTED);

Cleanup:
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CCustomMapper::init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType xsdType)
//
//  parameters:
//
//  description:
//      ISoapMapper::Init()
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CCustomMapper::init(ISoapTypeMapperFactory *ptypeFactory, IXMLDOMNode * pSchema, enXSDType xsdType)
{
    HRESULT hr = E_INVALIDARG;
    CAutoRefc<ISoapTypeMapper> pMapper; 

    if (!ptypeFactory || !pSchema)
    {
        goto Cleanup;
    }
    CHK(_verifyState(enMapperPrivInit));
    m_typeFactory.Clear();
    m_pSchemaNode.Clear();

    assign(&m_typeFactory, ptypeFactory);
    assign(&m_pSchemaNode, pSchema);

    m_enXSDType = xsdType;

#ifndef CE_NO_EXCEPTIONS
    try 
    {
#endif 
        if (!m_bCachable)
        {
            CAutoRefc<ISoapTypeMapper> pTypeMapper;
            CAutoRefc<ISoapTypeMapper2> pT2; 
            CCritSectWrapper csw(&m_critSect);

            CHK(csw.Enter());
            
            CHK(GetTypeMapperInterface(&pTypeMapper));
            CHK(pTypeMapper->varType(&m_vtType));
            if (pTypeMapper->QueryInterface(IID_ISoapTypeMapper2, (void**)&pT2)== S_OK)
            {
                m_bstrIID.Clear(); 
                CHK(pT2->iid(&m_bstrIID)); 
            }
        }
        else
        {
            // use the GIT entry
            LIP(ISoapTypeMapper) pTypeMapper(m_gipTypeMapper, hr);
            CHK(hr);
            CHK(pTypeMapper->init(ptypeFactory, pSchema, xsdType));
            CHK(pTypeMapper->varType(&m_vtType));
        }
#ifndef CE_NO_EXCEPTIONS
    }    
    catch(...)
    {
        hr = E_UNEXPECTED;
        goto Cleanup;
    }
#endif 

    m_state = enMapperWorking;

Cleanup:
    if (FAILED(hr))
        globalAddErrorFromErrorInfo(0, hr);
    return hr;
    
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CCustomMapper::read(IXMLDOMNode * pNode, BSTR bstrEncoding, enEncodingStyle enStyle, LONG lFlags, VARIANT * pvar)
//
//  parameters:
//
//  description:
//      ISoapMapper::Read()
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CCustomMapper::read(IXMLDOMNode * pNode, BSTR bstrEncoding, enEncodingStyle enStyle, LONG lFlags, VARIANT * pvar)
{
    HRESULT hr;
    CAutoRefc<ISoapTypeMapper> pMapper; 

    CHK(_verifyState(enMapperWorking));
#ifndef UNDER_CE
    try 
    {
#endif 
        if (!m_bCachable)
        {
            CAutoRefc<ISoapTypeMapper> pTypeMapper;
            CCritSectWrapper csw(&m_critSect);
            
            CHK( csw.Enter() );
            
            CHK(GetTypeMapperInterface(&pTypeMapper));            
            CHK(pTypeMapper->read(pNode, bstrEncoding, enStyle, 0, pvar));
        }
        else
        {
            // use the GIT entry
            LIP(ISoapTypeMapper) pTypeMapper(m_gipTypeMapper, hr);
            CHK(hr);
            CHK(pTypeMapper->read(pNode, bstrEncoding, enStyle, 0, pvar));            
        }    
#ifndef UNDER_CE
    }    
    catch(...)
    {
        hr = E_UNEXPECTED;
    }
#endif 

Cleanup:
    if (FAILED(hr))
        globalAddErrorFromErrorInfo(0, hr);

    return hr;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CCustomMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, LONG lFlags, VARIANT * pvar)
//
//  parameters:
//
//  description:
//      ISoapMapper::write()
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CCustomMapper::write(ISoapSerializer* pSoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, LONG lFlags, VARIANT * pvar)
{
    HRESULT hr;
    CAutoRefc<ISoapTypeMapper> pMapper; 

    CHK(_verifyState(enMapperWorking));
#ifndef CE_NO_EXCEPTIONS
    try 
    {
#endif 
        if (!m_bCachable)
        {
            CAutoRefc<ISoapTypeMapper> pTypeMapper;
            CCritSectWrapper csw(&m_critSect);
            
            CHK( csw.Enter());
            
            CHK(GetTypeMapperInterface(&pTypeMapper));            
            CHK(pTypeMapper->write(pSoapSerializer, bstrEncoding, enStyle, 0, pvar));
        }    
        else
        {
            LIP(ISoapTypeMapper) pTypeMapper(m_gipTypeMapper, hr);
            CHK(hr);
            CHK(pTypeMapper->write(pSoapSerializer, bstrEncoding, enStyle, 0, pvar));
        }
#ifndef CE_NO_EXCEPTIONS
    }    
    catch(...)
    {
        hr = E_UNEXPECTED;
    }
#endif 

Cleanup:
    if (FAILED(hr))
        globalAddErrorFromErrorInfo(0, hr);
    return hr;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CCustomMapper::varType(long *pvtType)
//
//  parameters:
//
//  description:
//      ISoapMapper::varType()
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CCustomMapper::varType(long *pvtType)
{
    HRESULT hr;
    CHK(_verifyState(enMapperWorking));    
    *pvtType = m_vtType;
Cleanup:    
    return hr;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CCustomMapper::iid(BSTR *pbstrIID)
//
//  parameters:
//
//  description:
//      ISoapMapper2::iid()
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CCustomMapper::iid(BSTR *pbstrIID)
{
    HRESULT hr; 
    CHK(_verifyState(enMapperWorking));        
    CHK(_WSDLUtilReturnAutomationBSTR(pbstrIID, m_bstrIID));
Cleanup:
    return hr; 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CCustomMapper::privateInit(BSTR bstrProgID)
//
//  parameters:
//
//  description:
//      privateInit(BSTR bstrProgID) -> set's up the progid for creation of the real thing
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CCustomMapper::privateInit(BSTR bstrProgID, BSTR bstrIID)
{
    HRESULT hr;
#ifndef CE_NO_EXCEPTIONS
    try 
    {
#endif 
        CHK(m_bstrProgID.Assign(bstrProgID));
        CHK(m_bstrIID.Assign(bstrIID));
        CHK(_checkForCachable());
        if (m_bCachable==false)
        {
            CHK(m_critSect.Initialize());
        }
        m_state = enMapperPrivInit;
#ifndef CE_NO_EXCEPTIONS
    }
    catch(...)
    {
        hr = E_UNEXPECTED;
    }
#endif 
Cleanup:    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CCustomMapper::_checkForCachable(void)
//
//  parameters:
//
//  description:
//      checks if the typemapper is cachable
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CCustomMapper::_checkForCachable(void)
{
    HRESULT hr;
    LPOLESTR    p=0; 
    DWORD       dwType;
    DWORD       dwSize=_MAX_BUFFER_SIZE;
#ifndef UNDER_CE
    TCHAR       achBuffer[_MAX_BUFFER_SIZE+1]; 
#else //moving to heap since CE only has a 4k guard page
    TCHAR       *achBuffer= new TCHAR[_MAX_BUFFER_SIZE+1]; 
    CHK_MEM(achBuffer);
#endif 

	CHK(CLSIDFromProgID(m_bstrProgID, &m_clsid));

	if (!m_bCheckedForCachable)
	{
	    CHkey        hKey; 

        // now as this is the first time, check if the component is boththreaded or freethreaded...
        CHK(StringFromCLSID(m_clsid, &p)); 
        wcscpy(achBuffer, _T("CLSID\\")); 
        wcscat(achBuffer, p); 
        wcscat(achBuffer, _T("\\InProcServer32")); 
        if (RegOpenKeyEx(HKEY_CLASSES_ROOT, achBuffer, 0, KEY_READ, &hKey.hkey)==ERROR_SUCCESS)
        {
            if (RegQueryValueEx(
                  hKey.hkey,            // handle to key
                  _T("ThreadingModel"),  // value name
                  0,   // reserved
                  &dwType,       // type buffer
                  (LPBYTE)achBuffer,        // data buffer
                  &dwSize      // size of data buffer
                )==ERROR_SUCCESS)
                {
                    if (dwSize > 0)
                    {
                        if (wcscmp(achBuffer, _T("Free"))==0)
                        {
                            hr = E_UNEXPECTED;
                            globalAddError(WSDL_IDS_CUSTOMMAPPERFREETHREADED, WSDL_IDS_MAPPER, hr, m_bstrProgID);
                            CHK(hr);
                        }
                        if (wcscmp(achBuffer, _T("Both"))==0 || 
                           wcscmp(achBuffer, _T("Neutral"))==0)
                        {
                            m_bCachable = true;                            
                        }
                    }
                }
                            
        }
        // verify the flag.
        m_bCheckedForCachable = true; 

        // if the guy is cachable, we want to put him in the GIT
        if (m_bCachable)
        {
            CAutoRefc<ISoapTypeMapper> pTypeMapper;
            CHK(GetTypeMapperInterface(&pTypeMapper));
            CHK(m_gipTypeMapper.Globalize(pTypeMapper));
        }    
	}
	
Cleanup:
    if (p)
    {
        CoTaskMemFree(p);
    }
    
#ifdef UNDER_CE
    if(achBuffer)
        delete [] achBuffer;
#endif 
    return(hr);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CCustomMapper::GetTypeMapperInterface(ISoapTypeMapper **ppTypeMapper)
//
//  parameters:
//
//  description:
//      GetTypeMapperInterface(void) - creates the real thing if needed and calls INIT on it
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CCustomMapper::GetTypeMapperInterface(ISoapTypeMapper **ppTypeMapper)
{
    HRESULT hr = S_OK;
	CHK(CoCreateInstance(m_clsid, 0, CLSCTX_ALL, IID_ISoapTypeMapper, (void**)ppTypeMapper));
	if (m_pSchemaNode)
	{
        showNode(m_pSchemaNode);    	
        CHK((*ppTypeMapper)->init(m_typeFactory, m_pSchemaNode, m_enXSDType));
	}    
Cleanup:
	return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

