//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include "av_upnp.h"
#include "av_upnp_ctrl_internal.h"

using namespace av_upnp;
using namespace av_upnp::DIDL_Lite;
using namespace av_upnp::DIDL_Lite::details;


/////////////////////////////////////////////////
// DIDL_Lite::parser
/////////////////////////////////////////////////


// ~parser
DIDL_Lite::parser::~parser()
{
    delete pObjects;
}


//
// GetFirstObject
//
bool DIDL_Lite::parser::GetFirstObject(LPCWSTR pszXml, object* pObj)
{
    //
    // Create instance of "objects" if it doesn't exist yet. 
    // Existing instance will be reinitialized for reuse in SAX::startDocument
    //
    if(!pObjects)
    {
        if(!(pObjects = new objects))
        {
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("OOM creating \"objects\"")));
            return false;
        }
    }
    
    //
    // Parse provided DIDL_Lite document
    //
    ce::SAXReader   Reader;
    HRESULT         hr;
    ce::variant     varXML(pszXml);
    
    if(Reader.valid())
    {
        hr = Reader->putContentHandler(pObjects);
        
        assert(SUCCEEDED(hr));

        if(FAILED(hr = Reader->parse(varXML)))
        {
            DEBUGMSG(TRUE, (TEXT("Error 0x%08x when parsing DIDL_Lite"), hr));
            return false;
        }
    }
    else
    {
        DEBUGMSG(TRUE, (TEXT("Error 0x%08x instantiating SAXXMLReader"), Reader.Error()));
        return false;
    }
    
    //
    // Return first object
    //
    return pObjects->GetFirstObject(pObj);
}


//
// GetNextObject
//
bool DIDL_Lite::parser::GetNextObject(object* pObj)
{
    //
    // GetNextObject called w/o calling GetFirstObject (or after GetFirstObject failed)
    //
    assert(pObjects);
    
    return pObjects->GetNextObject(pObj);
}


//
// AddNamespaceMapping
//
bool DIDL_Lite::parser::AddNamespaceMapping(LPCWSTR pszNamespace, LPCWSTR pszPrefix)
{
    //
    // Create instance of "objects" if it doesn't exist yet. 
    // Existing instance will be reinitialized for reuse in SAX::startDocument
    //
    if(!pObjects)
    {
        if(!(pObjects = new objects))
        {
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("OOM creating \"objects\"")));
            return false;
        }
    }
    
    return pObjects->AddNamespaceMapping(pszNamespace, pszPrefix);
}


/////////////////////////////////////////////////
// DIDL_Lite::object
/////////////////////////////////////////////////

// GetProperty
bool DIDL_Lite::object::GetProperty(LPCWSTR pszName, wstring* pValue, unsigned long nIndex/* = 0*/)
{
    assert(pProperties);
    
    return pProperties->GetProperty(pszName, pValue, nIndex);
}


// GetProperty
bool DIDL_Lite::object::GetProperty(LPCWSTR pszName, signed long* pValue, unsigned long nIndex/* = 0*/)
{
    wstring strValue;
    
    if(!GetProperty(pszName, &strValue, nIndex))
        return false;
        
    signed long lValue = _wtol(strValue);
    
    if(0 == lValue && wstring::npos != strValue.find_first_not_of(L"0"))
        return false;
    
    *pValue = lValue;
    
    return true;
}


// GetProperty
bool DIDL_Lite::object::GetProperty(LPCWSTR pszName, unsigned long* pValue, unsigned long nIndex/* = 0*/)
{
    signed long lValue;
    
    if(!GetProperty(pszName, &lValue, nIndex))
        return false;
        
    if(lValue < 0)
        return false;
        
    *pValue = static_cast<unsigned long>(lValue);
    
    return true;
}


// GetProperty
bool DIDL_Lite::object::GetProperty(LPCWSTR pszName, bool* pValue, unsigned long nIndex/* = 0*/)
{
    wstring strValue;
    
    if(!GetProperty(pszName, &strValue, nIndex))
        return false;
    
    if(strValue == L"0" || strValue == L"false")
        *pValue = false;
    else if(strValue == L"1" || strValue == L"true")
        *pValue = true;
    else
        return false;
    
    return true;
}


/////////////////////////////////////////////////
// DIDL_Lite::details::objects
/////////////////////////////////////////////////

DIDL_Lite::details::objects::objects()
{
    m_itCurrent = m_listObjects.end();
    
    // initialize namespace prefixes
    AddNamespaceMapping(L"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/", L"");
    AddNamespaceMapping(L"http://purl.org/dc/elements/1.1/", L"dc");
    AddNamespaceMapping(L"urn:schemas-upnp-org:metadata-1-0/upnp/", L"upnp");
}


//
// GetFirstObject
//
bool DIDL_Lite::details::objects::GetFirstObject(object* pObj)
{
    assert(pObj);
    
    m_itCurrent = m_listObjects.begin();
    
    return GetNextObject(pObj);
}


//
// GetNextObject
//
bool DIDL_Lite::details::objects::GetNextObject(object* pObj)
{
    assert(pObj);
    
    if(m_itCurrent == m_listObjects.end())
        return false;
    
    pObj->pProperties = &*m_itCurrent++;
    
    //
    // Parser validates that all required property are present
    // and are correct type
    //
    bool bResult;
    
    bResult = pObj->GetProperty(L"@id", &pObj->strID);
    assert(bResult);
    
    bResult = pObj->GetProperty(L"upnp:class", &pObj->strClass);
    assert(bResult);
    
    bResult = pObj->GetProperty(L"@parentID", &pObj->strParentID);
    assert(bResult);
    
    bResult = pObj->GetProperty(L"@restricted", &pObj->bRestricted);
    assert(bResult);
    
    bResult = pObj->GetProperty(L"dc:title", &pObj->strTitle);
    assert(bResult);
    
    pObj->bContainer = (0 == pObj->strClass.compare(L"object.container", 16));
    
    return true;
}


//
// AddNamespaceMapping
//
bool DIDL_Lite::details::objects::AddNamespaceMapping(LPCWSTR pszNamespace, LPCWSTR pszPrefix)
{
    assert(pszNamespace);
    assert(pszPrefix);
    assert(pszPrefix[0] != L':');
    
    wstring strPrefix;
    
    if(*pszPrefix)
    {
        strPrefix.assign(pszPrefix);
        strPrefix.append(L":");
    }
    
    return (m_mapNamespacePrefixes.end() != m_mapNamespacePrefixes.insert(pszNamespace, strPrefix));
}


//
// ISAXContentHandler
//

static const wchar_t pszItemElement[] = 
    L"<urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/>"
    L"<DIDL-Lite>"
    L"<urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/>"
    L"<item>";
    
static const wchar_t pszContainerElement[] = 
    L"<urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/>"
    L"<DIDL-Lite>"
    L"<urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/>"
    L"<container>";


// startDocument
HRESULT STDMETHODCALLTYPE DIDL_Lite::details::objects::startDocument(void)
{
    ce::SAXContentHandler::startDocument();
    
    m_listObjects.clear();
    	
	return S_OK;
}


// startElement
HRESULT STDMETHODCALLTYPE DIDL_Lite::details::objects::startElement(
    /* [in] */ const wchar_t *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t *pwchQName,
    /* [in] */ int cchQName,
    /* [in] */ ISAXAttributes *pAttributes)
{
    ce::wstring                 strNamespace(pwchNamespaceUri, cchNamespaceUri);
    NamespacePrefixes::iterator it;
    
    it = m_mapNamespacePrefixes.find(strNamespace);
    
    //
    // Before call to base class startElement m_strFullElementName is name of the parent element
    //
    if(pszItemElement == m_strFullElementName || pszContainerElement == m_strFullElementName)
    {
        //
        // child of <item> or <container>
        //
        if(m_mapNamespacePrefixes.end() != it)
        {
            // We have a prefix mapping for this namespace so lets use it instead of prefix used in XML
            m_strElementQName.assign(it->second);
            m_strElementQName.append(pwchLocalName, cchLocalName);
        }
        else
        {
            // We don't have a mapping for this namespace so we will use the prefix from XML
            m_strElementQName.assign(pwchQName, cchQName);
        }
    }
    else
    {
        if(m_itCurrent != m_listObjects.end())
        {
            //
            // grand(...)child of <item> or <container>
            //
            assert(!m_strElementQName.empty());
            
            // append /QName to m_strElementQName
            m_strElementQName.append(L"/");
            
            if(m_mapNamespacePrefixes.end() != it)
            {
                // We have a prefix mapping for this namespace so lets use it instead of prefix used in XML
                m_strElementQName.append(it->second);
                m_strElementQName.append(pwchLocalName, cchLocalName);
            }
            else
            {
                // We don't have a mapping for this namespace so we will use the prefix from XML
                m_strElementQName.append(pwchQName, cchQName);
            }
        }
    }
    
    // call base class
    ce::SAXContentHandler::startElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName, pAttributes);
    
    //
    // After call to base class startElement m_strFullElementName is name of the current element 
    //
    if(pszItemElement == m_strFullElementName || pszContainerElement == m_strFullElementName)
    {
        // Clear m_strElementQName if current element is <item> or <container>
        m_strElementQName.clear();
        
        // Add new object
        m_itCurrent = m_listObjects.insert(m_listObjects.end());
    }
    
    // Clear text value for current element
    m_strElementText.clear();
    
    if(m_itCurrent != m_listObjects.end())
    {
        //
        // Add attributes for the current element to the list of properties
        //
        int                 nCount;
        wstring::size_type  nOffset;
        const wchar_t       *pszAttributeQName, *pszAttributeValue;
        int                 cchAttributeQName, cchAttributeValue;
        wstring             strPropertyName;
        
        // Property name is built as folowing: element_qname@attribute_qname
        strPropertyName = m_strElementQName;
        strPropertyName += L"@";
        
        nOffset = strPropertyName.length();
        
        // Get nubmer of attributes
        pAttributes->getLength(&nCount);
        
        for(int i = 0; i < nCount; ++i)
        {
            pAttributes->getQName(i, &pszAttributeQName, &cchAttributeQName);
            pAttributes->getValue(i, &pszAttributeValue, &cchAttributeValue);
            
            strPropertyName.assign(pszAttributeQName, cchAttributeQName, nOffset);
            
            m_itCurrent->AddProperty(strPropertyName, pszAttributeValue, cchAttributeValue);
        }
    }
    
    return S_OK;
}

// endElement
HRESULT STDMETHODCALLTYPE DIDL_Lite::details::objects::endElement( 
    /* [in] */ const wchar_t *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t *pwchQName,
    /* [in] */ int cchQName)
{
    ce::wstring                 strNamespace(pwchNamespaceUri, cchNamespaceUri);
    NamespacePrefixes::iterator it;
    
    it = m_mapNamespacePrefixes.find(strNamespace);
    
    //
    // Before call to base class endElement m_strFullElementName is name of the current element
    //
    if(pszItemElement == m_strFullElementName || pszContainerElement == m_strFullElementName)
    {
        wstring str;
        
        // Verify that all required properties for the item/container are set and have proper type
        if(!m_itCurrent->GetProperty(L"@id", &str, 0) ||
           !m_itCurrent->GetProperty(L"upnp:class", &str, 0) ||
           !m_itCurrent->GetProperty(L"@parentID", &str, 0) ||
           !m_itCurrent->GetProperty(L"dc:title", &str, 0) ||
           !m_itCurrent->GetProperty(L"@restricted", &str, 0) ||
           (str != L"0" && str != L"1" && str != L"false" && str != L"true"))
        {
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("DIDL-Lite object doesn't have one or more required properties")));
            return E_FAIL;
        }
        
        // End of <item> or <container> element so reset current iterator 
        m_itCurrent = m_listObjects.end();
    }
    
    // call base class
    HRESULT hrRet = ce::SAXContentHandler::endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);
    
    //
    // After call to base class endElement m_strFullElementName is name of the parent element 
    //
    if(m_itCurrent != m_listObjects.end())
    {
        m_strElementText.trim(L"\n\r\t ");
        
        // Add text value of the current element to the list of properties
        m_itCurrent->AddProperty(m_strElementQName, m_strElementText, m_strElementText.length());
    
        if(pszItemElement != m_strFullElementName && pszContainerElement != m_strFullElementName)
        {
            // grandchild of <item> or <container>
            if(m_mapNamespacePrefixes.end() != it)
            {
                assert(0 == m_strElementQName.compare(L"/", 1, m_strElementQName.length() - it->second.length() - cchLocalName - 1));
                assert(0 == m_strElementQName.compare(it->second, it->second.length(), m_strElementQName.length() - it->second.length() - cchLocalName));
                assert(0 == m_strElementQName.compare(pwchLocalName, cchLocalName, m_strElementQName.length() - cchLocalName));
                
                // remove /QName from the end of m_strElementQName
                m_strElementQName.resize(m_strElementQName.length() - it->second.length() - cchLocalName - 1);
            }
            else
            {
                assert(0 == m_strElementQName.compare(L"/", 1, m_strElementQName.length() - cchQName - 1));
                assert(0 == m_strElementQName.compare(pwchQName, cchQName, m_strElementQName.length() - cchQName));
                
                // remove /QName from the end of m_strElementQName
                m_strElementQName.resize(m_strElementQName.length() - cchQName - 1);
            }
        }
    }
    
    return hrRet;
}

    
// characters
HRESULT STDMETHODCALLTYPE DIDL_Lite::details::objects::characters( 
    /* [in] */ const wchar_t *pwchChars,
    /* [in] */ int cchChars)
{
    m_strElementText.append(pwchChars, cchChars);
    
    return S_OK;
}


/////////////////////////////////////////////////
// DIDL_Lite::details::properties
/////////////////////////////////////////////////

// GetProperty
bool DIDL_Lite::details::properties::GetProperty(LPCWSTR pszName, wstring* pValue, unsigned long nIndex)
{
    PropertiesMap::iterator it;
    
    if(m_mapProperties.end() != (it = m_mapProperties.find(pszName)))
    {
        if(it->second.size() > nIndex)
        {
            *pValue = it->second[nIndex];
            return true;
        }
    }
    
    return false;    
}


// AddProperty
void DIDL_Lite::details::properties::AddProperty(LPCWSTR pszName, LPCWSTR pszValue, size_t cchValue)
{
    if(ce::vector<wstring>* pValues = m_mapProperties[pszName])
    {
        ce::vector<wstring>::iterator it;
        
        if(pValues->end() != (it = pValues->insert(pValues->end(), cchValue)))
        {
            it->assign(pszValue, cchValue);
        }
    }
}
