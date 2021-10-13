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

#include "av_upnp.h"
#include "av_upnp_ctrl_internal.h"

using namespace av_upnp;
using namespace av_upnp::DIDL_Lite;
using namespace av_upnp::DIDL_Lite::details;


/////////////////////////////////////////////////
// DIDL_Lite::parser
/////////////////////////////////////////////////

DIDL_Lite::parser::parser()
    :
    m_pObjects(NULL)
{
}

// ~parser
DIDL_Lite::parser::~parser()
{
    if (m_pObjects)
    {
        delete m_pObjects;
    }
}

//
// GetFirstObject
//
bool DIDL_Lite::parser::GetFirstObject(LPCWSTR pszXml, object* pObj)
{
    HRESULT hr = S_OK;
    ce::SAXReader Reader;
    ISAXXMLReader *pXMLReader = NULL;

    //
    // Create instance of "objects" if it doesn't exist yet. 
    // Existing instance will be reinitialized for reuse in SAX::startDocument
    //
    if(!m_pObjects)
    {
        if(!(m_pObjects = new objects))
        {
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("OOM creating \"objects\"")));
            return false;
        }
    }
    
    // Get rid of the internal allocations in m_pObjects
    m_pObjects->Cleanup();
    
    //
    // Parse provided DIDL_Lite document
    //
    
    hr = Reader.Init();
    
    if (SUCCEEDED(hr))
    {
        pXMLReader = Reader.GetXMLReader();
    }
    
    if (pXMLReader)
    {
        ce::variant varXML(pszXml);
        
        hr = pXMLReader->putContentHandler(m_pObjects);
        
        assert(SUCCEEDED(hr));

        if(FAILED(hr = pXMLReader->parse(varXML)))
        {
            DEBUGMSG(TRUE, (TEXT("Error 0x%08x when parsing DIDL_Lite"), hr));
            return false;
        }
        
        hr = pXMLReader->putContentHandler(NULL);
    }
    else
    {
        DEBUGMSG(TRUE, (TEXT("Error 0x%08x instantiating SAXXMLReader"), hr));
        return false;
    }
    
    //
    // Return first object
    //
    return m_pObjects->GetFirstObject(pObj);
}


//
// GetNextObject
//
bool DIDL_Lite::parser::GetNextObject(object* pObj)
{
    //
    // GetNextObject called w/o calling GetFirstObject (or after GetFirstObject failed)
    //
    if (m_pObjects && pObj)
    {
        return m_pObjects->GetNextObject(pObj);
    }
    
    return false;
}


/////////////////////////////////////////////////
// DIDL_Lite::object
/////////////////////////////////////////////////

DIDL_Lite::object::object()
    :
    m_pPropertiesRef(NULL)
{
}

DIDL_Lite::object::~object()
{
}

// GetProperty
bool DIDL_Lite::object::GetProperty(const WCHAR *pszName, wstring *pValue, unsigned long nIndex)
{
    HRESULT hr = S_OK;
    bool fResult = false;
    WCHAR *pszValue = NULL;
    
    ChkBool(pszName, E_POINTER);
    ChkBool(pValue, E_POINTER);
    ChkBool(m_pPropertiesRef, E_UNEXPECTED);
    
    fResult = m_pPropertiesRef->GetProperty(pszName, &pszValue, nIndex);
    ChkBool(fResult, S_FALSE);
    
    fResult = pValue->assign(pszValue);

Cleanup:    
    return fResult;
}

bool DIDL_Lite::object::GetProperty(const WCHAR *pszName, WCHAR *pValue, unsigned int cchValue, unsigned long nIndex)
{
    HRESULT hr = S_OK;
    bool fResult = false;
    WCHAR *pszValue = NULL;
    
    ChkBool(pszName, E_POINTER);
    ChkBool(pValue, E_POINTER);
    ChkBool(m_pPropertiesRef, E_UNEXPECTED);
    
    fResult = m_pPropertiesRef->GetProperty(pszName, &pszValue, nIndex);
    ChkBool(fResult, S_FALSE);
    
    fResult = (wcsncpy_s(pValue, cchValue, pszValue, _TRUNCATE) == 0);

Cleanup:    
    return fResult;
}

// GetProperty
bool DIDL_Lite::object::GetProperty(const WCHAR *pszName, signed long *pValue, unsigned long nIndex)
{
    HRESULT hr = S_OK;
    bool fResult = false;
    WCHAR *pszValue = NULL;
    long lValue = 0;
    
    ChkBool(pszName, E_POINTER);
    ChkBool(pValue, E_POINTER);
    ChkBool(m_pPropertiesRef, E_UNEXPECTED);
    
    fResult = m_pPropertiesRef->GetProperty(pszName, &pszValue, nIndex);
    ChkBool(fResult, S_FALSE);
    
    lValue = _wtol(pszValue);

    if(0 == lValue)
    {
        LPCWSTR pszZero = wcschr(pszName, L'0');
        
        if (!pszZero)
        {
            ChkBool(false, S_FALSE);
        }
    }
    
    fResult = true;
    
    *pValue = lValue;

Cleanup:    
    return fResult;
}


// GetProperty
bool DIDL_Lite::object::GetProperty(const WCHAR *pszName, unsigned long *pValue, unsigned long nIndex)
{
    HRESULT hr = S_OK;
    bool fResult = false;
    WCHAR *pszValue = NULL;
    long lValue = 0;
    
    ChkBool(pszName, E_POINTER);
    ChkBool(pValue, E_POINTER);
    ChkBool(m_pPropertiesRef, E_UNEXPECTED);
    
    fResult = GetProperty(pszName, &lValue, nIndex);
    ChkBool(fResult, S_FALSE);
    
    ChkBool(lValue > 0, S_FALSE);
    
    *pValue = (unsigned long)lValue;
    
Cleanup:
    return fResult;
}


// GetProperty
bool DIDL_Lite::object::GetProperty(const WCHAR *pszName, bool *pValue, unsigned long nIndex)
{
    HRESULT hr = S_OK;
    bool fResult = false;
    WCHAR *pszValue = NULL;
    long lValue = 0;
    
    ChkBool(pszName, E_POINTER);
    ChkBool(pValue, E_POINTER);
    ChkBool(m_pPropertiesRef, E_UNEXPECTED);    
    
    fResult = m_pPropertiesRef->GetProperty(pszName, &pszValue, nIndex);
    ChkBool(fResult, S_FALSE);
    
    if((wcscmp(pszValue, L"0") == 0) ||
       (wcscmp(pszValue, L"false") == 0))
    {
        *pValue = false;
        fResult = true;
    }
    else
    if((wcscmp(pszValue, L"1") == 0) ||
       (wcscmp(pszValue, L"true") == 0))
    {
        *pValue = true;
        fResult = true;
    }
    
Cleanup:
    return fResult;
}

/////////////////////////////////////////////////
// DIDL_Lite::details::objects
/////////////////////////////////////////////////

typedef struct NameSpaceMap_tag
{
    WCHAR szNamespace[80];
    WCHAR szPrefix[10];
}
NameSpaceMap;

static NameSpaceMap s_NameSpaceMapping[] =
{
    { L"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/", L"" },
    { L"http://purl.org/dc/elements/1.1/", L"dc:" },
    { L"urn:schemas-upnp-org:metadata-1-0/upnp/", L"upnp:" }
};

static WCHAR *FindPrefix(const WCHAR *pwchNamespace, unsigned int cchNamespace)
{
    WCHAR *pwchResult = NULL;
    
    if (pwchNamespace)
    {
        unsigned int iMap = 0;
        
        for(iMap = 0; iMap < _countof(s_NameSpaceMapping); iMap++)
        {
            if (wcscmp(s_NameSpaceMapping[iMap].szNamespace, pwchNamespace) == 0)
            {
                pwchResult = s_NameSpaceMapping[iMap].szPrefix;
                break;
            }
        }
    }
    
    return pwchResult;
}

DIDL_Lite::details::objects::objects()
{
    m_itCurrent = m_listObjects.end();
}

DIDL_Lite::details::objects::~objects()
{
    Cleanup();
}

void DIDL_Lite::details::objects::Cleanup()
{
    m_listObjects.clear();
    
    m_itCurrent = m_listObjects.end();
    m_strElementQName.clear();
    m_strElementText.clear();
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
    {
        return false;
    }
    
    // The strange & of deref is changing the type and then getting
    // the address of the internal item inside the iterator.
    // This is only a ref - the m_listObjects "owns" the property info
    pObj->SetPropertiesRef(&*m_itCurrent++);
    
    //
    // Parser validates that all required property are present
    // and are correct type
    //
    bool bResult = false;
    
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
// ISAXContentHandler
//

static const WCHAR pszItemElement[] = 
    L"<urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/>"
    L"<DIDL-Lite>"
    L"<urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/>"
    L"<item>";
    
static const WCHAR pszContainerElement[] = 
    L"<urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/>"
    L"<DIDL-Lite>"
    L"<urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/>"
    L"<container>";

// startDocument
HRESULT STDMETHODCALLTYPE DIDL_Lite::details::objects::startDocument(void)
{
    ce::SAXContentHandler::startDocument();
    
    Cleanup();
        
    return S_OK;
}

// startElement
HRESULT STDMETHODCALLTYPE DIDL_Lite::details::objects::startElement(
    /* [in] */ const WCHAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const WCHAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const WCHAR *pwchQName,
    /* [in] */ int cchQName,
    /* [in] */ ISAXAttributes *pAttributes)
{
    WCHAR *pwchPrefix = NULL;
    
    pwchPrefix = FindPrefix(pwchNamespaceUri, cchNamespaceUri);
    
    //
    // Before call to base class startElement m_strFullElementName is name of the parent element
    //
    if(pszItemElement == m_strFullElementName || pszContainerElement == m_strFullElementName)
    {
        //
        // child of <item> or <container>
        //
        if(pwchPrefix != NULL)
        {
            // We have a prefix mapping for this namespace so lets use it instead of prefix used in XML
            m_strElementQName.assign(pwchPrefix);
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
            
            if(pwchPrefix != NULL)
            {
                // We have a prefix mapping for this namespace so lets use it instead of prefix used in XML
                m_strElementQName.append(pwchPrefix);
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
    if (pszItemElement == m_strFullElementName || pszContainerElement == m_strFullElementName)
    {
        // Clear m_strElementQName if current element is <item> or <container>
        m_strElementQName.clear();
        
        // Add new object
        m_itCurrent = m_listObjects.insert(m_listObjects.end());
    }
    
    // Clear text value for current element
    m_strElementText.clear();
    
    if (m_itCurrent != m_listObjects.end())
    {
        //
        // Add attributes for the current element to the list of properties
        //
        int                 nCount;
        wstring::size_type  nOffset;
        const WCHAR       *pszAttributeQName, *pszAttributeValue;
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
    /* [in] */ const WCHAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const WCHAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const WCHAR *pwchQName,
    /* [in] */ int cchQName)
{
    WCHAR *pwchPrefix = NULL;
    
    pwchPrefix = FindPrefix(pwchNamespaceUri, cchNamespaceUri);
    
    //
    // Before call to base class endElement m_strFullElementName is name of the current element
    //
    if(pszItemElement == m_strFullElementName || pszContainerElement == m_strFullElementName)
    {
        WCHAR *pszID = NULL;
        WCHAR *pszClass = NULL;
        WCHAR *pszParentID = NULL;
        WCHAR *pszTitle = NULL;
        WCHAR *pszRestricted = NULL;
        
        // Verify that all required properties for the item/container are set and have proper type
        if(!m_itCurrent->GetProperty(L"@id", &pszID, 0) ||
           !m_itCurrent->GetProperty(L"upnp:class", &pszClass, 0) ||
           !m_itCurrent->GetProperty(L"@parentID", &pszParentID, 0) ||
           !m_itCurrent->GetProperty(L"dc:title", &pszTitle, 0) ||
           !m_itCurrent->GetProperty(L"@restricted", &pszRestricted, 0) ||
           !pszRestricted)
        {
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("DIDL-Lite object doesn't have one or more required properties")));
            return E_FAIL;
        }

        // Restricted has to be one of these 4 values
        if (pszRestricted)
        {        
            if ((wcscmp(pszRestricted, L"0") != 0) &&
                (wcscmp(pszRestricted, L"1") != 0) &&
                (wcscmp(pszRestricted, L"false") != 0) &&
                (wcscmp(pszRestricted, L"true") != 0))
            {
                DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("DIDL-Lite object doesn't have one or more required properties")));
                return E_FAIL;
            }
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
        if (!m_itCurrent->AddProperty(m_strElementQName, m_strElementText, m_strElementText.length()))
            return E_OUTOFMEMORY;
    
        if(pszItemElement != m_strFullElementName && pszContainerElement != m_strFullElementName)
        {
            // grandchild of <item> or <container>
            if(pwchPrefix != NULL)
            {
                unsigned int length;
                
                length = wcslen(pwchPrefix);
                
                assert(0 == m_strElementQName.compare(L"/", 1, m_strElementQName.length() - length - cchLocalName - 1));
                assert(0 == m_strElementQName.compare(pwchPrefix, length, m_strElementQName.length() - length - cchLocalName));
                assert(0 == m_strElementQName.compare(pwchLocalName, cchLocalName, m_strElementQName.length() - cchLocalName));
                
                // remove /QName from the end of m_strElementQName
                m_strElementQName.resize(m_strElementQName.length() - length - cchLocalName - 1);
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
    /* [in] */ const WCHAR *pwchChars,
    /* [in] */ int cchChars)
{
    if (m_strElementText.append(pwchChars, cchChars))
    {
        return S_OK;
    }
    else
    {
        return E_OUTOFMEMORY;
    }
}

/////////////////////////////////////////////////
// DIDL_Lite::details::PropertyInfo
/////////////////////////////////////////////////

DIDL_Lite::details::PropertyInfo::PropertyInfo()
    :
    m_bstrName(NULL),
    m_bstrValue(NULL),
    m_pNext(NULL)
{
}

DIDL_Lite::details::PropertyInfo::~PropertyInfo()
{
}

HRESULT DIDL_Lite::details::PropertyInfo::Init(LPCWSTR pszName, LPCWSTR pszValue, size_t cchValue)
{
    HRESULT hr = S_OK;
    
    if (!pszName || !pszValue)
    {
        return E_UNEXPECTED;
    }
    
    m_bstrName = SysAllocString(pszName);
    m_bstrValue = SysAllocStringLen(pszValue, cchValue);
    
    if (!m_bstrName || !m_bstrValue)
    {
        m_bstrName.close();
        m_bstrValue.close();
        
        hr = E_OUTOFMEMORY;
    }
    
    return hr;
}

HRESULT DIDL_Lite::details::PropertyInfo::SetValue(WCHAR *pszValue)
{
    HRESULT hr = S_OK;
    BSTR bstrNew;
    
    if (!pszValue)
    {
        return E_UNEXPECTED;
    }
    
    bstrNew = SysAllocString(pszValue);
    ChkBool(bstrNew, E_OUTOFMEMORY);
    
    // Release the old BSTR (if any)
    m_bstrValue.close();
    
    // and assign the new value
    m_bstrValue = bstrNew;

Cleanup:    
    return hr;
}

/////////////////////////////////////////////////
// DIDL_Lite::details::properties
/////////////////////////////////////////////////

DIDL_Lite::details::properties::properties()
    :
    m_pPropertyInfo(NULL)
{
}

DIDL_Lite::details::properties::~properties()
{
    Cleanup();
}

void DIDL_Lite::details::properties::Cleanup()
{
    while(m_pPropertyInfo)
    {
        PropertyInfo *pDead = m_pPropertyInfo;
        
        m_pPropertyInfo = m_pPropertyInfo->m_pNext;
        
        delete pDead;
    }
}

// GetProperty
bool DIDL_Lite::details::properties::GetProperty(const WCHAR *pszName, WCHAR **ppszValue, unsigned long nIndex)
{
    bool fResult = false;
    unsigned long iFound = 0;
    
    if (!pszName || !ppszValue)
    {
        return fResult;
    }
    
    // The m_pPropertyInfo simple forward-linked-list implementation is much
    // cheaper in terms of allocations than the previous hash_map<wstring, wstring>
    // implementation as the list of properties never grows beyond about 15.
    PropertyInfo *pPropInfo = m_pPropertyInfo;
    
    while(pPropInfo && !fResult)
    {
        if (pPropInfo->m_bstrName)
        {
            if (wcscmp(pPropInfo->m_bstrName, pszName) == 0)
            {
                if (iFound == nIndex)
                {
                    *ppszValue = pPropInfo->m_bstrValue;
                    fResult = true;
                }
                iFound++;
            }
        }
        
        pPropInfo = pPropInfo->m_pNext;
    }
    
    return fResult;    
}

// AddProperty
bool DIDL_Lite::details::properties::AddProperty(const WCHAR *pszName, const WCHAR *pszValue, size_t cchValue)
{
    bool fResult = false;
    
    PropertyInfo *pPropInfo = NULL;

#if 0 // This overwrites any value
    // TODO - remove the nIndex parameter on GetProperty and enable this code since
    // nobody ever asks for anything but the 0th property
    pPropInfo = m_pPropertyInfo;
    
    while(pPropInfo)
    {
        if (pPropInfo->bstrName)
        {
            if (wcscmp(pPropInfo->bstrName, pszName) == 0)
            {
                // Found the property, just change its value
                BSTR bstrNewValue = SysAllocStringLen(pszValue, cchValue);
                
                if (bstrNewValue)
                {
                    fResult = SUCCEEDED(pPropInfo->SetValue(pszValue));
                    
                    return fResult;
                }
            }
        }
        
        pPropItem = pPropItem->pNext;
    }
#endif // overwrite
    
    if (pPropInfo == NULL)
    {
        // Need to add the property to the list
        pPropInfo = new PropertyInfo();
        
        if (pPropInfo)
        {
            fResult = SUCCEEDED(pPropInfo->Init(pszName, pszValue, cchValue));
            
            if (!fResult)
            {
                delete pPropInfo;
            }
            else
            {
                if (m_pPropertyInfo == NULL)
                {
                    // list was empty
                    m_pPropertyInfo = pPropInfo;
                }
                else
                {
                    // add to the end of the list
                    PropertyInfo *pPropLocation = m_pPropertyInfo;
                    
                    while(pPropLocation->m_pNext != NULL)
                    {
                        pPropLocation = pPropLocation->m_pNext;
                    }
                    
                    pPropLocation->m_pNext = pPropInfo;
                }
            }
        }
    }
        
    return fResult;
}
