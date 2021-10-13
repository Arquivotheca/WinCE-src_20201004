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
using namespace av_upnp::details;


//
// LastChangeParsing
//
LastChangeParsing::LastChangeParsing(LPCWSTR pszNamespace)
{
    m_strEventElement = L"<";
    m_strEventElement += pszNamespace;
    m_strEventElement += L"><Event>";
    
    m_strInstanceElement = m_strEventElement;
    m_strInstanceElement += L"<";
    m_strInstanceElement += pszNamespace;
    m_strInstanceElement += L"><InstanceID>";
}


//
// endDocument
//
HRESULT STDMETHODCALLTYPE LastChangeParsing::startDocument(void)
{
    ce::SAXContentHandler::startDocument();
    
    m_bUnderInstanceID = false;
    
    return S_OK;
}


//
// startElement
//
HRESULT STDMETHODCALLTYPE LastChangeParsing::startElement( 
    /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t __RPC_FAR *pwchQName,
    /* [in] */ int cchQName,
    /* [in] */ ISAXAttributes __RPC_FAR *pAttributes)
{
    ce::SAXContentHandler::startElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName, pAttributes);
    
    if(m_strInstanceElement == m_strFullElementName)
    {
        assert(!m_bUnderInstanceID);
        
        const wchar_t*  pwszInstanceID;
        int             cchInstanceID;
        
        if(SUCCEEDED(pAttributes->getValueFromName(L"", 0, L"val", 3, &pwszInstanceID, &cchInstanceID)))
        {
            ce::wstring strInstanceID(pwszInstanceID, cchInstanceID);
            
            m_InstanceID = _wtol(strInstanceID);
            m_nLevelsUnderInstanceID = 0;
            m_bUnderInstanceID = true;
        }
    }
    else if(m_bUnderInstanceID)
    {
        if(1 == ++m_nLevelsUnderInstanceID)
        {
            const wchar_t   *pwszValue, *pwszChannel;
            int             cchValue, cchChannel;
            ce::wstring     strStateVar(pwchLocalName, cchLocalName);
            
            if(SUCCEEDED(pAttributes->getValueFromName(L"", 0, L"val", 3, &pwszValue, &cchValue)))
            {
                ce::wstring strValue(pwszValue, cchValue);
                
                if(SUCCEEDED(pAttributes->getValueFromName(L"", 0, L"channel", 7, &pwszChannel, &cchChannel)))
                {
                    ce::wstring strChannel(pwszChannel, cchChannel);
                    long        nValue = _wtol(strValue);
                    
                    OnStateChanged(m_InstanceID, strStateVar, strChannel, nValue);
                }
                else
                {
                    ce::hash_map<wstring, StateVar>::iterator it;
                    
                    if(m_mapStateVars.end() != (it = m_mapStateVars.find(strStateVar)) && it->second.bNumeric)
                        OnStateChanged(m_InstanceID, strStateVar, _wtol(strValue));
                    else
                        OnStateChanged(m_InstanceID, strStateVar, strValue);
                }
            }
            else
            {
                DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("Element %s doesn't have required 'val' attribute"), static_cast<LPCWSTR>(m_strFullElementName)));
            }
        }
        else
        {
            //
            // Only one level of elements under InstanceID is expected 
            //
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("Ignoring unexpected element %s while parsing LastChange event message."), static_cast<LPCWSTR>(m_strFullElementName)));
        }
    }
    else if(m_strEventElement != m_strFullElementName)
    {
        //
        // The only element other than InstnaceID and state variable elementes under InstanceID is the root Event element
        //
        DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("Ignoring unexpected element %s while parsing LastChange event message."), static_cast<LPCWSTR>(m_strFullElementName)));
    }

    return S_OK;
}


//
// endElement
//
HRESULT STDMETHODCALLTYPE LastChangeParsing::endElement( 
    /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t __RPC_FAR *pwchQName,
    /* [in] */ int cchQName)
{
    if(m_strInstanceElement == m_strFullElementName)
    {
        assert(m_nLevelsUnderInstanceID == 0);
        
        m_bUnderInstanceID = false;
    }
    else if(m_bUnderInstanceID)
    {
        --m_nLevelsUnderInstanceID;
    }

    return ce::SAXContentHandler::endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);
}
