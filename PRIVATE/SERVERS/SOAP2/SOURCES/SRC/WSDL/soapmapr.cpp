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
// File:    soapmapr.cpp
// 
// Contents:
//
//  implementation file 
//
//		ISoapMapper Interface implemenation
//	
//	Created 
//
//-----------------------------------------------------------------------------
#include "headers.h"
#include "wsdlserv.h"
#include "wsdloper.h"
#include "soapmapr.h"
#include "typemapr.h"

BEGIN_INTERFACE_MAP(CSoapMapper)
	ADD_IUNKNOWN(CSoapMapper, ISoapMapper)
	ADD_INTERFACE(CSoapMapper, ISoapMapper)
END_INTERFACE_MAP(CSoapMapper)


/////////////////////////////////////////////////////////////////////////////
// helper class to keep the state in variants inside the list
//

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: DWORD CKeyedVariant::GetKey(void)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////

DWORD CKeyedVariant::GetKey(void)
{
    return(m_dwCookie);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CKeyedVariant::SetValue(VARIANT* tValue)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT CKeyedVariant::SetValue(VARIANT* tValue)
{
    return m_tValue.Assign(tValue, TRUE);
    return S_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CKeyedVariant::SetValue(VARIANT tValue)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT CKeyedVariant::SetValue(VARIANT tValue)
{
    return m_tValue.Assign(&tValue, TRUE);
    return S_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: VARIANT* CKeyedVariant::GetValue(void)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////

VARIANT* CKeyedVariant::GetValue(void)
{
    return(&(m_tValue));
}
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapMapper::CSoapMapper()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CSoapMapper::CSoapMapper()
{
	m_lCallIndex  = DISPID_UNKNOWN; 
    m_vtType = 0;    
	m_lCallIndex = -1;
    m_lparameterOrder=-1;
    m_basedOnType = false;
	
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapMapper::~CSoapMapper()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CSoapMapper::~CSoapMapper()
{
	TRACEL((3, "Mapper %S shutdown\n",getVarName()));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapMapper::ShutDown(void)
//
//  parameters:
//
//  description:
//		called from the operation when it's destroyed. Forces a release on the soaptypemapper, 
//			to avoid circular references.
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoapMapper::Shutdown(void)
{
	m_pOwner.Clear();			
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapMapper::get_elementName(BSTR *pbstrElementName)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapMapper::get_elementName(BSTR *pbstrElementName)
{
    return _WSDLUtilReturnAutomationBSTR(pbstrElementName, getVarName());                        
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapMapper::get_variantType(LONG *pvtType)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapMapper::get_variantType(long *pvtType)
{
    if (!pvtType)
    {
        return E_INVALIDARG;
    }
    *pvtType = m_vtType;
    return S_OK;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapMapper::get_callIndex(LONG *plCallIndex)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapMapper::get_callIndex(LONG *plCallIndex)
{
	HRESULT hr = S_OK;
	if (!plCallIndex)
	{
		hr = E_INVALIDARG;
		goto Cleanup;
	}

	*plCallIndex = m_lCallIndex;

Cleanup:
	ASSERT(hr==S_OK);
	return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapMapper::get_parameterOrder(LONG *plparaOrder)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapMapper::get_parameterOrder(LONG *plparaOrder)
{
	HRESULT hr = S_OK;
	if (!plparaOrder)
	{
		hr = E_INVALIDARG;
		goto Cleanup;
	}

	*plparaOrder= m_lparameterOrder;

Cleanup:
	ASSERT(hr==S_OK);
	return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapMapper::get_partName(BSTR *pbstrPartName)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapMapper::get_partName(BSTR *pbstrPartName)
{
    return _WSDLUtilReturnAutomationBSTR(pbstrPartName, m_bstrPartName);                            
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapMapper::get_messageName(BSTR *pbstrmessageName)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapMapper::get_messageName(BSTR *pbstrmessageName)
{
	ASSERT(m_pOwner!=0);

	if (m_pOwner==0)
	{
		return(E_FAIL);
	}

	return m_pOwner->get_messageName(pbstrmessageName, m_enInput!=smOutput);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapMapper::get_elementType(BSTR *pbstrElementType)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapMapper::get_elementType(BSTR *pbstrElementType)
{
    return _WSDLUtilReturnAutomationBSTR(pbstrElementType , m_bstrSchemaElement);                                
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapMapper::get_isInput(smIsInputEnum *psmInput)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapMapper::get_isInput(smIsInputEnum *psmInput)
{
	HRESULT hr = S_OK;

	*psmInput = m_enInput;
	
	return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:HRESULT CSoapMapper::get_encoding(BSTR *pbstrEncoding)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapMapper::get_encoding(BSTR *pbstrEncoding)
{
	ASSERT(m_pOwner!=0);
	if (m_pOwner==0)
	{
		return(E_FAIL);
	}
    return _WSDLUtilReturnAutomationBSTR(pbstrEncoding, m_pOwner->getEncoding(m_enInput!=smOutput));                                	
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapMapper::Init(IXMLDOMNode *pPartNode, ISoapTypeMapperFactory *ptypeFactory, bool fInput,
//								      BSTR bstrSchemaNS, CWSDLOperation *pParent)
//
//  parameters:
//
//  description:
//		takes the pointer to the message part, sucks info out of it and
//		finds corresponding schema info
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapMapper::Init(IXMLDOMNode *pPartNode, ISoapTypeMapperFactory *ptypeFactory, bool fInput, BSTR bstrSchemaNS, CWSDLOperation *pParent)
{
	HRESULT 	hr = E_FAIL;
    CAutoBSTR   bstrTemp;
  	TCHAR		achPrefix[_MAX_ATTRIBUTE_LEN+1];
    CAutoBSTR   bstrURI;

	ASSERT(pPartNode!=0);
	ASSERT(pParent!=0);
	ASSERT(ptypeFactory!=0);
	

	assign(&m_pOwner,pParent);
	assign(&m_ptypeFactory, ptypeFactory);

	m_enInput = fInput ? smInput : smOutput;
    
    if (!bstrSchemaNS)    
    {
	    hr = _WSDLUtilFindAttribute(pPartNode, _T("name"), &m_bstrPartName);
	    if (FAILED(hr)) 
	    {
            globalAddError(WSDL_IDS_MAPPERNONAME, WSDL_IDS_MAPPER, hr);
		    goto Cleanup;
	    }
    
        // check for uniqueness of partname
        hr = pParent->verifyPartName(m_enInput, m_bstrPartName);
        if (FAILED(hr)) 
        {
            globalAddError(WSDL_IDS_MAPPERDUPLICATENAME, WSDL_IDS_MAPPER, hr, m_bstrPartName);
            goto Cleanup;
        }
    
	    hr = _WSDLUtilFindAttribute(pPartNode, _T("element"), &bstrTemp);
	    if (FAILED(hr)) 
	    {
	        bstrTemp.Clear();
		    // if there is NO element declaration, there must be type declaration
		    hr = _WSDLUtilFindAttribute(pPartNode, _T("type"), &bstrTemp);
		    if (FAILED(hr)) 
		    {
		        globalAddError(WSDL_IDS_MAPPERNOELEMENTORTYPE, WSDL_IDS_MAPPER, hr, m_bstrPartName);        
		        goto Cleanup;
		    }
		    m_basedOnType = true;
	    }
    
    
        CHK(_WSDLUtilSplitQName(bstrTemp, achPrefix, &m_bstrSchemaElement));
        hr = _XSDFindURIForPrefix(pPartNode, achPrefix, &bstrURI);        
        if (FAILED(hr)) 
        {
            globalAddError(WSDL_IDS_URIFORQNAMENF, WSDL_IDS_MAPPER, hr, achPrefix, bstrTemp);
            goto Cleanup;
        }
    }
    else
    {
        // so this is PID case

        // we might need to do an additonal lookup if there is no NAME, because then it might be a REF situaion
        // as a REF points to another ELEMENT of the same NAME we can just stick the REF value into our NAME 
        // property and let the typefactory worry about finding the type
        if (FAILED(_WSDLUtilFindAttribute(pPartNode, _T("name"), &bstrTemp)))
        {
            CHK(_WSDLUtilFindAttribute(pPartNode, _T("ref"), &bstrTemp));
        }
        CHK(_WSDLUtilSplitQName(bstrTemp, achPrefix, &m_bstrSchemaElement));
        // remember this guy for later use
        m_pSchemaNode = pPartNode;
        m_pSchemaNode->AddRef();
    }

    CHK(setSchemaURI(bstrSchemaNS ? bstrSchemaNS : bstrURI));
    CHK(setVarName(m_basedOnType ? m_bstrPartName : m_bstrSchemaElement));
    CHK(createTypeMapper());
	TRACEL((3, "Mapper %S created\n",getVarName()));

Cleanup:
	ASSERT(hr==S_OK);
	return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapMapper::AddWSMLMetaInfo(IUnknown *pExecuteNode,CWSDLService *pService, 
//                                                                                IXMLDOMDocument *pWSDLDom,  IXMLDOMDocument *pWSMLDom, 
//												bool bLoadOnServer)
//
//  parameters:
//
//  description:
//		the method is called during initialisation to give a mapper a chance
//		to read the information he stored in the WSML file
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapMapper::AddWSMLMetaInfo(IXMLDOMNode *pExecuteNode, 
                                    CWSDLService *pService, 
                                    IXMLDOMDocument *pWSDLDom, 
                                    IXMLDOMDocument *pWSMLDom,
                                    bool bLoadOnServer)
{
	HRESULT 				hr = E_FAIL;
	CAutoRefc<IXMLDOMNode> 	pExecute=0;
	CAutoRefc<IXMLDOMNode> 	pPara=0;
	CAutoRefc<IXMLDOMNode> 	pType=0;
	CAutoFormat             autoFormat;
#ifndef UNDER_CE
	CDispatchHolder			*pDispatchHolder;
#endif
    if (bLoadOnServer)
    {
	    hr = pExecuteNode->QueryInterface(IID_IXMLDOMNode, (void **)&pExecute); 
	    if (FAILED(hr)) 
	    {
		    goto Cleanup;
	    }
	    CHK(autoFormat.sprintf(_T("./parameter[@elementName=\"%s\"]"), getVarName()));

	    hr = pExecute->selectSingleNode(&autoFormat, &pPara);
	    if (hr != S_OK) 
	    {
            globalAddError(WSML_IDS_MAPPERNOELEMENTNAME, WSDL_IDS_MAPPER, hr, getVarName());                           
            hr = E_FAIL; 
		    goto Cleanup;
	    }

	    if (pPara)
	    {
	        CAutoBSTR bstrTemp; 
		    hr = _WSDLUtilFindAttribute(pPara, _T("callIndex"), &bstrTemp);
		    if (FAILED(hr)) 
		    {
                globalAddError(WSML_IDS_MAPPERNOCALLINDEX, WSDL_IDS_MAPPER, hr, getVarName());                            
			    goto Cleanup;
		    }
		    m_lCallIndex = _wtoi(bstrTemp);
	    }
	}
    else
    {
        hr = pWSMLDom->QueryInterface(IID_IXMLDOMNode, (void **)&pExecute); 
        if (FAILED(hr)) 
        {
            goto Cleanup;
        }
    }

    if (pService->doCustomMappersExist())
    {
		// at least one, recreated the mappers
		// they were all registered during WSDLService::AddWSMLMetaInfo, just call create
        CHK(createTypeMapper());
	}
	else
	{
		// we don't have to have a custom mapper, so ignore
		hr = S_OK;
	}

Cleanup:
	ASSERT(hr==S_OK);
	return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapMapper::createTypeMapper()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapMapper::createTypeMapper()
{
    HRESULT hr; 

    m_pTypeMapper.Clear();

    if (m_pSchemaNode)
    {
        // in this case we have a node inside a complex definition
        hr = m_ptypeFactory->getElementMapper(m_pSchemaNode, &m_pTypeMapper); 
    }
    else
    {
        if (m_basedOnType)
        {
            // can only happen from our code above
            hr = m_ptypeFactory->getTypeMapperbyName(m_bstrSchemaElement, getSchemaURI(), &m_pTypeMapper);
        }
        else
        {
            hr = m_ptypeFactory->getElementMapperbyName(m_bstrSchemaElement, getSchemaURI(), &m_pTypeMapper);
        }
    }
    if (FAILED(hr))
    {
        globalAddError(WSDL_IDS_MAPPERNOTCREATED, WSDL_IDS_MAPPER, hr, m_bstrSchemaElement);            
        goto Cleanup;
    }
    
    CHK(m_pTypeMapper->varType(&m_vtType));

Cleanup:
    ASSERT(SUCCEEDED(hr));
    return (hr);
    
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapMapper::get_xmlNameSpace(BSTR *pbstrxmlNameSpace)
//
//  parameters:
//
//  description:
//      returns the targetnamespace of the schema
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapMapper::get_xmlNameSpace(BSTR *pbstrxmlNameSpace)
{

    if (!pbstrxmlNameSpace)
        return(E_INVALIDARG);

    *pbstrxmlNameSpace=0;
    
    if (m_bstrSchemaURI.Len()>0)
    {
        *pbstrxmlNameSpace = ::SysAllocString(m_bstrSchemaURI);
        if (!(*pbstrxmlNameSpace))    
            return(E_OUTOFMEMORY);
        
    }

    return(S_OK);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapMapper::get_comValue(VARIANT *pVarOut)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapMapper::get_comValue(VARIANT *pVarOut)
{
	HRESULT hr;
	VARIANT *pVar;

    pVar = m_variantList.Lookup(GetCurrentThreadId()); 
    if (pVar)
    {
        CHK (VariantCopy(pVarOut, pVar));
    }    
    else
    {
        CHK(PrepareVarOut(pVarOut)); 
    }


Cleanup:
	ASSERT(hr==S_OK);
	if (FAILED(hr))
	{
	     globalAddError(WSML_IDS_TYPEMAPPERPUTVALUE, WSDL_IDS_MAPPER, hr, m_bstrSchemaElement);                                               
	}
	return (hr);
  
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapMapper::put_comValue(VARIANT varIn)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapMapper::put_comValue(VARIANT varIn)
{
	HRESULT hr = E_INVALIDARG;

	if (V_VT(&varIn)==VT_RECORD)
	{
        globalAddError(WSDL_IDS_VTRECORDNOTSUPPORTED, WSDL_IDS_MAPPER,  hr, m_bstrSchemaElement);                                    
        goto Cleanup;
	}

    CHK( m_variantList.Replace(&varIn, GetCurrentThreadId()));
    
Cleanup:
	ASSERT(hr==S_OK);
	if (FAILED(hr))
	{
	     globalAddError(WSML_IDS_TYPEMAPPERPUTVALUE, WSDL_IDS_MAPPER, hr, m_bstrSchemaElement);                                               
	}
    
	return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapMapper::Save(ISoapSerializer *pISoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle,
//									 BSTR bstrMessageNS, long lFlags)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapMapper::Save(ISoapSerializer *pISoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle, BSTR bstrMessageNS, long lFlags)
{
	HRESULT	 hr;
	HRESULT hrFromTypeMapper; 
	VARIANT *pVar;
	BOOL    fSaveHREF;
	TCHAR   achhref[_MAX_ATTRIBUTE_LEN];
	BOOL    fCreatedInnerElement = true; 

	ASSERT(m_pTypeMapper);

    CHK_BOOL(m_pTypeMapper, E_UNEXPECTED);
	CHK_BOOL (pISoapSerializer, E_INVALIDARG);

    fSaveHREF = lFlags & c_saveIDRefs; 


    if (!fSaveHREF)
    {
        // we only need the value if we are actually going to call the typemapper here
        pVar = m_variantList.Lookup(GetCurrentThreadId()); 
        CHK_BOOL(pVar, E_INVALIDARG);
    }    
    CHK_BOOL(enStyle!=enDocumentEncoded, E_INVALIDARG);

    switch (enStyle)
    {
        case enRPCLiteral:
            if (!m_basedOnType)
            {
                // need to add the partname as a wrapper element for part element=
                // put it into the method namespace by using the prefix
            	CHK (pISoapSerializer->startElement(getPartName(), bstrMessageNS, NULL, NULL) ); 
               	CHK (pISoapSerializer->startElement(getVarName(), m_bstrSchemaURI, NULL, NULL));
            }
            else
            {
                // if based on type, no wrapper, but the partname is the wrapper anyway and is the the message namespace
               	CHK (pISoapSerializer->startElement(getVarName(), bstrMessageNS, NULL, NULL));
            }
            break;
        case enDocumentLiteral:
            if (!m_basedOnType)
            {
                // if based on type, we do NOT write a wrapper element
               	CHK (pISoapSerializer->startElement(getVarName(), m_bstrSchemaURI, NULL, NULL));
            }   	
            else
            {
                fCreatedInnerElement = false;
            }
           	break;
        case enRPCEncoded:
            CHK (pISoapSerializer->startElement(getVarName(), NULL, NULL, NULL) );    
            break;
    }

    if (fSaveHREF)
    {
        // now add the href attribute to the element
        swprintf(achhref, _T("#%d"), m_lparameterOrder+2);

        CHK(pISoapSerializer->SoapAttribute( 
    							(BSTR)g_pwstrHref,			// name,
    							L"",				// ns_uri,
    							achhref,	// value
    							L""));					// prefix

        hrFromTypeMapper = S_OK;							
    }			
    else
    {
    	hrFromTypeMapper = m_pTypeMapper->write (pISoapSerializer, bstrEncoding, enStyle, 0, pVar); 
    }	

	
	// now end the inner element
	if (fCreatedInnerElement)
	{
	    // will be false for document/literal/type
    	CHK (pISoapSerializer->endElement());
	}	

 
	if (enStyle == enRPCLiteral  && m_basedOnType==false)
    {
        // need to close the partname as a wrapper element
        CHK (pISoapSerializer->endElement());
    }    

	hr = hrFromTypeMapper; 

Cleanup:
	ASSERT(hr==S_OK);
    
	if (FAILED(hr))
	{
	     globalAddError(WSML_IDS_TYPEMAPPERSAVE, WSDL_IDS_MAPPER, hr, m_bstrSchemaElement);                                               
	}
    
	return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapMapper::SaveValue(ISoapSerializer *pISoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle)
//
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapMapper::SaveValue(ISoapSerializer *pISoapSerializer, BSTR bstrEncoding, enEncodingStyle enStyle)
{
    HRESULT hr; 
    HRESULT hrFromTypeMapper;
    
	TCHAR   achhref[_MAX_ATTRIBUTE_LEN];
	VARIANT *pVar;

	ASSERT(m_pTypeMapper);

    CHK_BOOL(m_pTypeMapper, E_UNEXPECTED);
	CHK_BOOL (pISoapSerializer, E_INVALIDARG);

    pVar = m_variantList.Lookup(GetCurrentThreadId()); 

    CHK_BOOL(pVar, E_INVALIDARG);
    CHK_BOOL(enStyle!=enDocumentEncoded, E_INVALIDARG);

	
	// now write the value element
    CHK (pISoapSerializer->startElement(_T("soapValue"), NULL, NULL, NULL) );        

    swprintf(achhref, _T("%d"), m_lparameterOrder+2);
	CHK(pISoapSerializer->SoapAttribute( 
							_T("id"),			// name,
							L"",				// ns_uri,
							achhref,	// value
							L""));					// prefix

	// store the hr from the typemapper away, we need to close the elements no matter what happens
	hrFromTypeMapper = m_pTypeMapper->write (pISoapSerializer, bstrEncoding, enStyle, 0, pVar); 

	// now end the inner element
	CHK (pISoapSerializer->endElement());


	hr = hrFromTypeMapper; 
Cleanup:
    return (hr);
    
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:HRESULT CSoapMapper::Load(IXMLDOMNode *pNodeRoot, BSTR bstrEncoding, enEncodingStyle enStyle, 
//									BSTR bstrMessageNS)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapMapper::Load(IXMLDOMNode *pNodeRoot, BSTR bstrEncoding, enEncodingStyle enStyle, BSTR bstrMessageNS)
{
	HRESULT 	hr = E_FAIL;
	CAutoRefc<IXMLDOMNode> pNode; 
	CAutoRefc<IXMLDOMNode> pSearch; 	
	CVariant varIn;
 	XPathState  xp;
	CAutoFormat     autoFormat;
	

    CHK_BOOL(enStyle!=enDocumentEncoded, E_INVALIDARG);
    CHK (xp.init(pNodeRoot));

	VariantInit(&varIn);

	ASSERT(m_pTypeMapper);
    CHK_BOOL(m_pTypeMapper, E_UNEXPECTED);


    CHK(autoFormat.sprintf(_T("xmlns:%s='%s'"), g_pwstrMessageSchemaNSPrefix, m_bstrSchemaURI));
    CHK (xp.setLanguage(g_pwstrXpathLanguage)); //give us enc: for encoding    
    CHK (xp.addNamespace(&autoFormat)); //give us m:messageNamespace    
    
    if (bstrMessageNS)
    {
        CHK(autoFormat.sprintf(_T("xmlns:%s='%s'"), g_pwstrMessageNSPrefix, bstrMessageNS));
        CHK (xp.addNamespace(&autoFormat)); //give us m:messageNamespace
    }


	// let's first go and select the correct subnode for our element
	// the way to FIND the node is different pending modes:
	//  in document mode we always look for the correctly named element
	//  in rpc encoded mode we mostly look for the correctly named element
	//      only when we are the RESULT, we take the FIRST childnode

	if (enStyle > enDocumentEncoded && m_lparameterOrder == -1)
	{
	    // so if we are IN RPC mode AND we are the RESULT, we ignore all name rules.
        // find the first child of the body here
        showNode(pNodeRoot);
        CHK(_WSDLUtilFindFirstChild(pNodeRoot, &pSearch));
        showNode(pSearch);
        if (enStyle == enRPCLiteral && m_basedOnType == false)
        {
            CHK(_WSDLUtilFindFirstChild(pSearch, &pNode));            
            showNode(pNode);
        }
        else
        {
            pNode = pSearch.PvReturn();
        }
	}
    else
    {
        switch (enStyle)
        {
            case enRPCLiteral:
                {
                    CAutoFormat     afPart;

                    // construct first part of the search string based on the message namespace
           	        if (bstrMessageNS)
           	        {
           	            CHK(afPart.sprintf(_T("%s:%s"), g_pwstrMessageNSPrefix, getPartName()));
           	        }
           	        else
           	        {
           	             CHK(afPart.copy(getPartName()));
           	        }
                    
                    if (!m_basedOnType)
                    {
                        CHK(autoFormat.sprintf(_T("%s/%s:%s"), &afPart, g_pwstrMessageSchemaNSPrefix , getVarName()));
                    }
                    else
                    {
                        // we have no wrapper, so we look for message:part
                        CHK(autoFormat.copy(&afPart));
                    }
                }
                break;

                
            case enDocumentLiteral:
                if (!m_basedOnType)
                {
                    CHK(autoFormat.sprintf(_T("%s:%s"), g_pwstrMessageSchemaNSPrefix , getVarName()));
                }    
                else
                {
                    // now we do NOT make a lookup, we just  assign the node
                    pNode = pNodeRoot;
                    pNode->AddRef();
                }
                break;
                
            case enRPCEncoded:
                // if encoded, no namespace added
                CHK(autoFormat.copy(getVarName()));
                break;
        }
        if (!pNode)
        {
            // should not be true for document/literal/@type
            CHK(pNodeRoot->selectSingleNode(&autoFormat, &pNode));
        }    
    }

        
    CHK_BOOL(pNode, E_INVALIDARG);

    // now we do have the correct SOAPMAPPER node. This might be a node with an href attribute. check for it
    CHK (FollowHref(&pNode) );
    

    CHK(m_pTypeMapper->read(pNode, bstrEncoding, enStyle, 0, (VARIANT *)&varIn) );

    // if we are the mapper did not claim to return an array or variant

    if (   !(m_vtType & VT_SAFEARRAY) &&  !(m_vtType & VT_ARRAY) &&  !(m_vtType & VT_VARIANT) )
    {
        // check if typemapr returned correct vartype compared to what he told us before
        CHK_BOOL((V_VT((VARIANT*)&varIn) == m_vtType), E_INVALIDARG);
    }
    
    CHK (put_comValue((VARIANT)varIn));

Cleanup:
	VariantClear(&varIn);    
    if (FAILED(hr))
    {
         globalAddError(WSML_IDS_TYPEMAPPERPUTSOAPVALUE, WSDL_IDS_MAPPER, hr, m_bstrSchemaElement);                                               
    }

	return (hr);

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapMapper::PrepareVarOut(VARIANT *pVarOut)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapMapper::PrepareVarOut(VARIANT *pVarOut)
{
	HRESULT hr = S_OK;
	if (!pVarOut)
	{
		hr = E_INVALIDARG;
		goto Cleanup;
	}

    memset(pVarOut, 0, sizeof(VARIANT));
	V_VT(pVarOut) = (VARTYPE)m_vtType;;

Cleanup:
	ASSERT(hr==S_OK);
	return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
