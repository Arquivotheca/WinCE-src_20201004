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
// File:    xsdpars.cpp
// 
// Contents:
//
//  implementation file 
//
//		xsdparser  implemenation
//	
//
//-----------------------------------------------------------------------------
#include "headers.h"
#include "soapmapr.h"
#include "typemapr.h"
#include "xsdpars.h"


typedef struct _XSDSchemaEntries
{
    schemaRevisionNr m_enSchemaRevision;
    TCHAR * 		 m_schemaNS;
    TCHAR * 		 m_schemaInstanceNS; 
    TCHAR * 		 m_schemaInstanceXPATH; 
} XSDSchemaEntries;


const XSDSchemaEntries g_arrSchemaEntries[] = 
{
    { 
        enSchema2001,     
        L"http://www.w3.org/2001/XMLSchema",    
        L"http://www.w3.org/2001/XMLSchema-instance",    
        L"xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'" 
    },

    {
        enSchema2000,
        L"http://www.w3.org/2000/10/XMLSchema",            
        L"http://www.w3.org/2000/10/XMLSchema-instance",
        L"xmlns:xsi='http://www.w3.org/2000/10/XMLSchema-instance'"
    },
    {
        enSchema1999,
        L"http://www.w3.org/1999/XMLSchema",
        L"http://www.w3.org/1999/XMLSchema-instance",
        L"xmlns:xsi='http://www.w3.org/1999/XMLSchema-instance'"        
    }    
};



 
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: _XSDFindURIForPrefix(IXMLDOMNode *pStartNode, TCHAR *pchPrefix, BSTR * pbstrURI)
//
//  parameters:
//      pStartNode -> the DOM node to start looking outwards from
//      pchPrefix  -> the namespace prefix in the QNAME (like "my:something", my is the prefix)
//      pbstrURI   -> the URI namespace (out)
//  
//  description:
//		takes a startnode and searches for the XMLNS:PREFIX attribute on all nodes outwards
//  returns: 
//      S_OK = found the namespace
//      E_FAIL = namespace not found
//      E_INVALIDARG = arguments are wrong
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XSDFindURIForPrefix(IXMLDOMNode *pStartNode, TCHAR *pchPrefix, BSTR * pbstrURI)
{
    HRESULT                 hr = E_FAIL;
    TCHAR                   achBuffer[ONEK+1];
    CAutoRefc<IXMLDOMNode>  pCurrentNode;
    CAutoRefc<IXMLDOMNode>  pNextNode;
    bool                    bFound=false;
    
    if (!pStartNode || !pbstrURI || !pchPrefix)
    {
        hr = E_INVALIDARG;
        goto Cleanup;
    }

    assign(&pCurrentNode, pStartNode);


    // create the attribute we are looking for

    if (wcslen(pchPrefix)==0)
    {
        // empty prefix, no URI, valid case
        wcscpy(achBuffer, _T("xmlns"));
    }
    else
    {
        swprintf(achBuffer, _T("xmlns:%s"), pchPrefix);                
    }
         
    while (!bFound)
    {
        hr = _WSDLUtilFindAttribute(pCurrentNode, achBuffer, pbstrURI);    
        if (hr == E_FAIL)
        {
             hr = pCurrentNode->get_parentNode(&pNextNode);
             if (hr == S_OK && pNextNode)
             {
                pCurrentNode.Clear();
                assign(&pCurrentNode, (IXMLDOMNode*)pNextNode);
                pNextNode.Clear();   
             }
             else
             {
                hr = E_FAIL;
                goto Cleanup;   
             }
        }
        else
        {
            bFound = true;
        }
    }
    
Cleanup:
    return(hr);
    
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _XSDLFindTargetSchema(TCHAR *pchURI, IXMLDOMNode *pDom, IXMLDOMNode **ppSchemaNode)
//
//  parameters: 
//      pchURI -> URI for the targetnamespace attribute
//      pDom    => document to search
//      ppSchemanode -> out -> schemanode to return
//  description:    
//      searches for a schema with the specified TNS
//  returns: 
//      S_OK : found
//      E_FAIL : not found
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XSDLFindTargetSchema(TCHAR *pchURI, IXMLDOMNode *pDom, IXMLDOMNode **ppSchemaNode)
{
    HRESULT hr;
    CAutoFormat autoFormat;

    
    if (pchURI)
    {
        CHK(autoFormat.sprintf(_T("//def:types/schema:schema[@targetNamespace=\"%s\"]"), pchURI));
    }
    else
    {
        CHK(autoFormat.copy(_T("//def:types/schema:schema[not(@targetNamespace)]")));
    }
    


	hr = _XPATHUtilFindNodeFromRoot(pDom, &autoFormat, ppSchemaNode);
Cleanup:    
    return(hr);
        
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _XSDFindTargetNameSpace(IXMLDOMNode *pStartNode, BSTR *pbstrURI)
//
//  parameters: 
//      pSchemanode -> starting schema element
//      BSTR * pbstrURI -> out -> targetNamespace
//  description:    
//      searches node upwards until it finds targetnamespace declaration
//  returns: 
//      S_OK : found
//      E_FAIL : not found
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XSDFindTargetNameSpace(IXMLDOMNode *pStartNode, BSTR *pbstrURI)
{
    HRESULT                 hr = E_FAIL;
    CAutoRefc<IXMLDOMNode>  pCurrentNode;
    CAutoRefc<IXMLDOMNode>  pNextNode;
    bool                    bFound=false;
    
    if (!pStartNode || !pbstrURI )
    {
        hr = E_INVALIDARG;
        goto Cleanup;
    }

    assign(&pCurrentNode, pStartNode);

    while (!bFound)
    {
        hr = _WSDLUtilFindAttribute(pCurrentNode, L"targetNamespace", pbstrURI);    
        if (hr == E_FAIL)
        {
             hr = pCurrentNode->get_parentNode(&pNextNode);
             if (hr == S_OK && pNextNode)
             {
                pCurrentNode.Clear();
                assign(&pCurrentNode, (IXMLDOMNode*)pNextNode);
                pNextNode.Clear();   
             }
             else
             {
                hr = E_FAIL;
                goto Cleanup;   
             }
        }
        else
        {
            bFound = true;
        }
    }
    
Cleanup:
    return(hr);
    
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: TCHAR * _XSDIsPublicSchema(TCHAR *pchURI)
//
//  parameters: 
//      pchURI -> URI to check
//  description:    
//      verifies if the URI is a public XSD schema
//  returns: 
//      global pointer to public schema def
//      0 if not
/////////////////////////////////////////////////////////////////////////////////////////////////////////
schemaRevisionNr _XSDIsPublicSchema(TCHAR *pchURI) 
{
    if (pchURI)
    {
        for (int i = 0; i < countof(g_arrSchemaEntries); i++)
        {
            if (wcscmp(pchURI, g_arrSchemaEntries[i].m_schemaNS)==0)
            {
                return (g_arrSchemaEntries[i].m_enSchemaRevision) ; 
            }
        }
    }
    return (enSchemaInvalid);        
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _XSDFindRevision(IXMLDOMNode *pNode, schemaRevisionNr *pRevisionNr) 
//
//  parameters: 
//      pNode -> node in a document with a schema definition
//  description:    
//      schecks for schemas, returns revisionnr of schema
//      when NO node is passed in, return latest schema revision
//  returns: 
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XSDFindRevision(IXMLDOMNode *pNode, schemaRevisionNr *pRevisionNr)
{
    CAutoRefc<IXMLDOMDocument2> pDoc; 
    CAutoRefc<IXMLDOMDocument>  pTemp; 
    HRESULT hr; 

    if (!pNode)
    {
        *pRevisionNr = enSchemaLast; 
        hr = S_OK;
        goto Cleanup;
    }

   	CHK ( pNode->get_ownerDocument(&pTemp));
   	

	if (pTemp == NULL)
	{
		// we are already at the root and pDoc is NULL (msxml behavior)
		CHK(pNode->QueryInterface(IID_IXMLDOMDocument2, (void **)&pDoc));
	}
    else
    {
    	CHK ( pTemp->QueryInterface(IID_IXMLDOMDocument2, (void **)&pDoc));
    }

    CHK(_XSDFindRevision(pDoc, pRevisionNr));

Cleanup:
    return hr; 
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _XSDFindRevision(IXMLDOMDocument2 *pDocument, schemaRevisionNr *pRevisionNr)
//
//  parameters: 
//      DomDocument -> document with schema definitons
//  description:    
//      schecks for schemas, returns revisionnr of schema
//  returns: 
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XSDFindRevision(IXMLDOMDocument2 *pDocument, schemaRevisionNr *pRevisionNr)
{
    HRESULT hr=S_OK; 
    CAutoRefc<IXMLDOMSchemaCollection> pIXMLDOMSchemaCollection=0;
    LONG        lLength; 
    CAutoBSTR   bstrTemp;
#ifdef UNDER_CE
   long lIndex; 
#endif 
    CHK(pDocument->get_namespaces(&pIXMLDOMSchemaCollection));
    CHK(pIXMLDOMSchemaCollection->get_length(&lLength));
    
#ifndef UNDER_CE
    for(long lIndex=0; lIndex < lLength; lIndex++)
#else
    for(lIndex=0; lIndex < lLength; lIndex++)
#endif 
    {
        CHK(pIXMLDOMSchemaCollection->get_namespaceURI(lIndex, &bstrTemp));
        *pRevisionNr = _XSDIsPublicSchema(bstrTemp); 
        if (*pRevisionNr != enSchemaInvalid)
        {
            break;
        }
        bstrTemp.Clear();
    }
    if (lIndex>=lLength)
    {
       // did not find a schema namespace, invalid
        hr = E_INVALIDARG;    
    }
Cleanup:    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _XSDFindRevision(ISoapSerializer* pSoapSerializer, schemaRevisionNr *pRevision)
//
//  parameters: 
//      
//  description:    
//      finds the currently used revision in the started payload
//  returns: 
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XSDFindRevision(ISoapSerializer* pSoapSerializer, schemaRevisionNr *pRevision)
{
    HRESULT hr=S_OK;
    CAutoBSTR   bstrPrefix; 

    for (int i = 0; i < countof(g_arrSchemaEntries); i++)
    {
        pSoapSerializer->getPrefixForNamespace((BSTR) g_arrSchemaEntries[i].m_schemaNS, &bstrPrefix);
        if (bstrPrefix)
        {
            *pRevision = g_arrSchemaEntries[i].m_enSchemaRevision; 
            goto Cleanup;
        }
        bstrPrefix.Clear(); 
    }
    hr = E_FAIL; 

Cleanup:
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _XSDAddNS(ISoapSerializer* pSoapSerializer, BSTR *pPrefix, WCHAR *pchNS)
//
//  parameters: 
//      
//  description:    
// 
//  returns: 
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XSDAddNS(ISoapSerializer* pSoapSerializer, BSTR *pPrefix, WCHAR *pchNS)
{
    HRESULT hr = S_OK;
    
    // is the first namespace defined
    pSoapSerializer->getPrefixForNamespace((BSTR) pchNS, pPrefix);
    if (!(*pPrefix))
    {
        // ok , not existend, we might want to create one
        CHK(pSoapSerializer->SoapNamespace( (BSTR)g_pwstrEmpty, (BSTR) pchNS));
        CHK(pSoapSerializer->getPrefixForNamespace( (BSTR)pchNS, pPrefix));
     }

Cleanup:
    ASSERT (SUCCEEDED(hr));
    return hr;

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _XSDAddTKDataNS(ISoapSerializer* pSoapSerializer, schemaRevisionNr revision, BSTR *pPrefix)
//
//  parameters: 
//      
//  description:    
//      adds the desired schema namespace to the payload
//  returns: 
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XSDAddTKDataNS(ISoapSerializer* pSoapSerializer, schemaRevisionNr revision, BSTR *pPrefix)
{
    HRESULT hr = S_OK;
    WCHAR   *pchNS;

    *pPrefix = NULL;
    CHK_BOOL(revision != enSchemaInvalid, E_FAIL);

    pchNS = (WCHAR *) g_pwstrTKData;
    
    CHK (_XSDAddNS(pSoapSerializer, pPrefix, pchNS));

Cleanup:
    ASSERT (SUCCEEDED(hr));
    return hr;

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _XSDAddSchemaNS(ISoapSerializer* pSoapSerializer, schemaRevisionNr revision, BSTR *pPrefix)
//
//  parameters: 
//      
//  description:    
//      adds the desired schema namespace to the payload
//  returns: 
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XSDAddSchemaNS(ISoapSerializer* pSoapSerializer, schemaRevisionNr revision, BSTR *pPrefix)
{
    HRESULT hr = S_OK;
    TCHAR   *pchNS;

    *pPrefix = NULL;
    CHK_BOOL(revision != enSchemaInvalid, E_FAIL);

    pchNS = (TCHAR*) _XSDSchemaNS(revision); 
    // is the first namespace defined
    
    CHK (_XSDAddNS(pSoapSerializer, pPrefix, pchNS));

Cleanup:
    ASSERT (SUCCEEDED(hr));
    return hr;

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _XSDAddSchemaInstanceNS(ISoapSerializer* pSoapSerializer, schemaRevisionNr revision, BSTR *pPrefix)
//
//  parameters: 
//      DomDocument -> document with schema definitons
//  description:    
//    adds the desired schema Instance namespace to the payload
//  returns: 
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XSDAddSchemaInstanceNS(ISoapSerializer* pSoapSerializer, schemaRevisionNr revision, BSTR *pPrefix)
{
    HRESULT hr = S_OK;
    WCHAR   *pchNS;

    *pPrefix = NULL;
    CHK_BOOL(revision != enSchemaInvalid, E_FAIL);
    pchNS = (WCHAR*) _XSDSchemaInstanceNS(revision);
    
    CHK (_XSDAddNS(pSoapSerializer, pPrefix, pchNS));

Cleanup:
    ASSERT (SUCCEEDED(hr));
    return hr;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

// simple access methods to get the strings behind the revision numbers


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:  const TCHAR * _XSDSchemaNS(schemaRevisionNr revision)
//
//  parameters: 
//     
//  description:    
//    
//  returns: 
/////////////////////////////////////////////////////////////////////////////////////////////////////////
const TCHAR * _XSDSchemaNS(schemaRevisionNr revision)
{
    if (revision != enSchemaInvalid)
    {
        ASSERT(g_arrSchemaEntries[revision-1].m_enSchemaRevision == revision);
        return g_arrSchemaEntries[revision-1].m_schemaNS; 
    }
    return (0);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: const TCHAR * _XSDSchemaInstanceNS(schemaRevisionNr revision)
//
//  parameters: 
//     
//  description:    
//    
//  returns: 
/////////////////////////////////////////////////////////////////////////////////////////////////////////
const TCHAR * _XSDSchemaInstanceNS(schemaRevisionNr revision)
{
    if (revision != enSchemaInvalid)
    {
        ASSERT(g_arrSchemaEntries[revision-1].m_enSchemaRevision == revision);
        return g_arrSchemaEntries[revision-1].m_schemaInstanceNS; 
    }
    return (0);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: const TCHAR * _XSDSchemaInstanceXPATH(schemaRevisionNr revision)
//
//  parameters: 
//     
//  description:    
//    
//  returns: 
/////////////////////////////////////////////////////////////////////////////////////////////////////////
const TCHAR * _XSDSchemaInstanceXPATH(schemaRevisionNr revision)
{
    if (revision != enSchemaInvalid)
    {
        ASSERT(g_arrSchemaEntries[revision-1].m_enSchemaRevision == revision);        
        return g_arrSchemaEntries[revision-1].m_schemaInstanceXPATH; 
    }
    return (0);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _XSDFindNodeInSchemaNS(const TCHAR *pchQuery, IXMLDOMNode *pStartNode, IXMLDOMNode **ppNode)
//
//  parameters: 
//      
//  description:    
//      finds a node by trying all available schemaNS
//  returns: 
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XSDFindNodeInSchemaNS(const TCHAR *pchQuery, IXMLDOMNode *pStartNode, IXMLDOMNode **ppNode)
{
    HRESULT hr = S_OK;
    

    for (int i = 0; i < countof(g_arrSchemaEntries); i++)
    {
        {
            XPathState  xp;
            CHK (xp.init(pStartNode));
            // need the XSI namespace
            CHK (xp.addNamespace(g_arrSchemaEntries[i].m_schemaInstanceXPATH));
            CHK (pStartNode->selectSingleNode((BSTR)g_pwstrXpathXSIanyType, ppNode));
            if (*ppNode)
                goto Cleanup;
        }
    }    
    // if we get here, we did not find anything
    hr =E_FAIL;
    
Cleanup:
    return (hr);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _XSDFindChildElements(IXMLDOMNode *pStartNode, IXMLDOMNodeList **ppResultList, 
//					                                TCHAR * pchElementName, int iIDS, BSTR *pbstrNSUri)
//
//  parameters:
//
//  description:
//      this function is a helper to expand the part=parameters behaviour.
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XSDFindChildElements(IXMLDOMNode *pStartNode, IXMLDOMNodeList **ppResultList, 
                                TCHAR * pchElementName, int iIDS, BSTR *pbstrNSUri)
{
    HRESULT                 hr = E_FAIL;
    TCHAR		            achPrefix[_MAX_ATTRIBUTE_LEN+1];
    CAutoBSTR               bstrElement;
    CAutoRefc<IXMLDOMNode>  pTargetSchema; 
    CAutoRefc<IXMLDOMNode>  pTargetNode;     
#ifndef UNDER_CE
    long                    lLen;
#endif 
    CAutoFormat             autoFormat;
    

    *ppResultList = 0;     

    achPrefix[0] = 0;
    CHK(_WSDLUtilSplitQName(pchElementName, achPrefix, &bstrElement));
    if (wcslen(achPrefix)>0)
    {
        hr = _XSDFindURIForPrefix(pStartNode, achPrefix, pbstrNSUri);        
        if (FAILED(hr)) 
        {
            globalAddError(WSDL_IDS_URIFORQNAMENF, iIDS, hr, achPrefix, bstrElement);
            goto Cleanup;
        }
    }    
    
    if (_XSDIsPublicSchema(*pbstrNSUri))
    {
        // wrong: an element declaration has to point to a private schema
        hr = E_FAIL;
        globalAddError(WSDL_IDS_MAPPERNOSCHEMA, iIDS, hr, *pbstrNSUri, bstrElement);
        goto Cleanup;
    }
    else
    {
        // user schema, look for the node
        hr = _XSDLFindTargetSchema(*pbstrNSUri, pStartNode, &pTargetSchema);
        if (FAILED(hr)) 
        {
            globalAddError(WSDL_IDS_MAPPERNOSCHEMA, iIDS, hr, *pbstrNSUri, bstrElement);    
            goto Cleanup;
        }
        // now search for the subnode:
        CHK(autoFormat.sprintf(_T(".//schema:element[@name=\"%s\"]"), bstrElement));
        pTargetSchema->selectSingleNode(&autoFormat, &pTargetNode);
        if (pTargetNode)
        {
            // found the root... 
            // now we try to find all subelements of the form
            // element/complexType/sequence/element OR
            // element/complexType/all/element
            // as this is the PID case, we assume that this is .NET, hence we do a very lazy lookup and will treat each
            // empty result as a call with no parts

            CHK(autoFormat.sprintf(_T("(//schema:element[@name=\"%s\"]/schema:complexType/schema:all/schema:element | //schema:element[@name=\"%s\"]/schema:complexType/schema:sequence/schema:element)"), bstrElement, bstrElement));
            CHK(pTargetNode->selectNodes(&autoFormat, ppResultList));
            
        }
        else
        {
            globalAddError(WSDL_IDS_MAPPERNODEFINITION, iIDS, hr, bstrElement);   
            hr = E_FAIL;
            goto Cleanup;
        }
    }    


Cleanup:
    ASSERT(SUCCEEDED(hr));
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _XSDSetupDefaultXPath(IXMLDOMNode*pNode, schemaRevisionNr enRevision)
//
//  parameters:
//
//  description:
//      set's up our default namespace search
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XSDSetupDefaultXPath(IXMLDOMNode*pNode, schemaRevisionNr enRevision)
{
    WCHAR                       *achNameSpaces[] = { _T("def"), _T("soap"), _T("ext"), _T("schema") };
    WCHAR                       *achNameSpaceURIs[4];
    HRESULT                     hr = E_FAIL;


    // set this one up as our search namespace:

    achNameSpaceURIs[0] = (WCHAR*)g_pwstrWSDL;
    achNameSpaceURIs[1] = (WCHAR*)g_pwstrSOAP;
    achNameSpaceURIs[2] = (WCHAR*)g_pwstrMSExtension;
    achNameSpaceURIs[3] = (WCHAR *)_XSDSchemaNS(enRevision);

    CHK(_XPATHUtilPrepareLanguage(pNode, &(achNameSpaces[0]), &(achNameSpaceURIs[0]), 4));

Cleanup:
    return hr;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




