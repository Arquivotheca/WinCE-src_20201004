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
// File:    elementstack.cpp
// 
// Contents:
//
//  implementation file 
//
//
//
//
//-----------------------------------------------------------------------------
#include "headers.h"

#include "soapser.h"


HRESULT CElement::Init(void)
{
    HRESULT hr;

    CHK (allocateAndCopy(&m_pcURI, g_pwstrEmpty));
    CHK (allocateAndCopy(&m_pcName, g_pwstrEmpty));
    CHK (allocateAndCopy(&m_pcPrefix, g_pwstrEmpty));
    CHK (allocateAndCopy(&m_pcValue, g_pwstrEmpty));
    CHK (allocateAndCopy(&m_pcQName, g_pwstrEmpty));
    
Cleanup:
    ASSERT(hr == S_OK);
    return hr;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElement::FixNamespace(WCHAR *pDefaultNamespace, CNamespaceHelper * pnsh)
//
//  parameters: the current default namespace and a handler to
//            the current namespace context
//
//  description:This functions looks at the current element and makes 
//            adjustments to the contained namespace information.
//            In case we are on the default namespace the prefix is removed.
//            
//            As the final step the complete qualified name is created.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CElement::FixNamespace(WCHAR *pDefaultNamespace, CNamespaceHelper * pnsh)
{
    HRESULT hr = S_OK;
    
    if ((pDefaultNamespace) && wcslen(pDefaultNamespace))
    {
        //  we have a default namespace, check the element against it    
        if ( !wcslen(m_pcURI) && wcslen(m_pcPrefix) )
        {
            // we got a prefix, but no URI
            CNamespaceListEntry *    pNS;

            pNS = pnsh->FindURI(m_pcPrefix);
            if (pNS)
            {
                //there was one, let's set it 
                setURI(pNS->getURI());
            }
        
        }

        // does the element live in this namespace
        if (wcscmp(pDefaultNamespace, m_pcURI) == NULL)
        {
            // They have the same URI, we can remove the prefix from the Element
            CHK(setPrefix(g_pwstrEmpty));
        }
    }

    // lets fix up or create the qualified name
    {
        int iPrefixLen;
        int iNameLen;
        WCHAR * pBuffer;
            
        iPrefixLen = m_pcPrefix ? wcslen(m_pcPrefix) : 0;
        iNameLen = m_pcName ? wcslen(m_pcName) : 0;

#ifdef UNDER_CE
        __try{
#endif
        pBuffer = (WCHAR *) _alloca( sizeof(WCHAR) * (iPrefixLen + iNameLen + 2) );
#ifdef UNDER_CE
        }
        __except(1){   
            return E_OUTOFMEMORY;
        }
#endif
        
        if  ( (iPrefixLen > 0) && (iNameLen > 0))
        {
            wcscpy(pBuffer, m_pcPrefix);
            wcscpy(pBuffer + iPrefixLen, L":");            
            wcscpy(pBuffer + iPrefixLen + 1, m_pcName);
        }    
        else if(iNameLen > 0)
        {
            wcscpy(pBuffer, m_pcName);
        }
        else
        {
            pBuffer[0] = NULL;
        }

        CHK (setQName(pBuffer));
    }

Cleanup:
    ASSERT (hr == S_OK);
    return hr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElement::setURI(const WCHAR *pcText)
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CElement::setURI(const WCHAR *pcText)
{
    return  (allocateAndCopy(&m_pcURI, pcText));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElement::setName(const WCHAR *pcText)
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CElement::setName(const WCHAR *pcText)
{
    return  (allocateAndCopy(&m_pcName, pcText));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElement::setQName(const WCHAR *pcText)
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CElement::setQName(const WCHAR *pcText)
{
    return  (allocateAndCopy(&m_pcQName, pcText));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElement::setPrefix(const WCHAR *pcText)
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CElement::setPrefix( const WCHAR *pcText)
{
    return  (allocateAndCopy(&m_pcPrefix, pcText));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElement::setValue(const WCHAR *pcText)
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CElement::setValue( const WCHAR *pcText)
{
    return  (allocateAndCopy(&m_pcValue, pcText));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElement::FlushElementContents(CSoapSerializer *pSoapSerializer)
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CElement::FlushElementContents(    CSoapSerializer *pSoapSerializer)
{
    HRESULT hr = S_OK;

    CHK (pSoapSerializer->_WriterStartElement( 
            m_pcURI, wcslen(m_pcURI),
            m_pcName, wcslen(m_pcName),
            m_pcQName, wcslen(m_pcQName)));
Cleanup:
    ASSERT (hr == S_OK);
    return hr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElement::FlushAttributeContents(CSoapSerializer *pSoapSerializer)
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CElement::FlushAttributeContents( CSoapSerializer *pSoapSerializer)
{
    CAutoBSTR   bstrQN;
    CAutoBSTR   bstrVAL;
    
    HRESULT hr = S_OK;

    CHK (bstrQN.Assign(m_pcQName));
    CHK (bstrVAL.Assign(m_pcValue));

    CHK ( pSoapSerializer->_WriterAddAttribute(NULL, NULL, bstrQN, NULL, bstrVAL) ) ;

Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElementStackEntry::~CElementStackEntry()
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CElementStackEntry::~CElementStackEntry()
{ 
    delete[] m_pcDefaultNamespace;

    while (!m_AttributeList.IsEmpty())
    {
        CElement * pele;

        pele = m_AttributeList.RemoveHead();
        delete pele;
    }
} 


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElementStackEntry::Init()
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CElementStackEntry::Init(void)
{
    HRESULT hr;

    CHK(CElement::Init());
    CHK (allocateAndCopy(&m_pcDefaultNamespace, g_pwstrEmpty));
    
Cleanup:
    ASSERT(hr == S_OK);
    return hr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElementStackEntry::setDefaultNamespace(const WCHAR *pcText)
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CElementStackEntry::setDefaultNamespace(const WCHAR *pcText)
{
    return  (allocateAndCopy(&m_pcDefaultNamespace, pcText));
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElementStackEntry::AddAttribute(    WCHAR * prefix, WCHAR * ns, WCHAR * name, WCHAR * value)
//
//  parameters: All information to add an Attribute, prefix, namespace, name 
//                and value
//
//  description:Function adds the information to the element stack, all needed
//                namespace handling operations will happen during the ouput 
//                generation
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CElementStackEntry::AddAttribute(
        WCHAR * prefix, 
        WCHAR * ns, 
        WCHAR * name, 
        WCHAR * value)
{
    HRESULT hr;
    CAutoP<CElement>    pele(NULL);

    pele = new CElement();

    CHK_BOOL (pele != NULL, E_OUTOFMEMORY);

    CHK (pele->Init());
    CHK (pele->setPrefix(prefix));
    CHK (pele->setURI(ns));
    CHK (pele->setName(name));
    CHK (pele->setValue(value));

    m_AttributeList.InsertTail(pele.PvReturn());
    
Cleanup:
    ASSERT(hr == S_OK);
    return hr;
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElementStackEntry::FixNamespace(CNamespaceHelper * pnsh)
//
//  parameters: Handler to the current namespace context
//
//  description:
//            This functions looks at the current element and makes adjustments
//            to the contained namespace information in this element and all
//            attributes attached to the element.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CElementStackEntry::FixNamespace(CNamespaceHelper * pnsh)
{
    HRESULT     hr = S_OK;
    CElement*    pEle;    

    CHK (CElement::FixNamespace(m_pcDefaultNamespace, pnsh));

    pEle = m_AttributeList.getHead();

    while (pEle)
        {
        CHK(pEle->FixNamespace(NULL, pnsh));
        pEle = m_AttributeList.next(pEle);
        }

Cleanup:
    ASSERT (hr == S_OK);
    return hr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElementStackEntry::FlushElement(CSoapSerializer *pSoapSerializer)
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CElementStackEntry::FlushElement(CSoapSerializer *pSoapSerializer)
{
    HRESULT hr = S_OK;
    CElement*    pEle;    
    
    if (m_IsSerialized)
        return S_OK;


    CHK (CElement::FlushElementContents(pSoapSerializer));
    
    pEle = m_AttributeList.RemoveHead();

    while (pEle)
        {
        CHK(pEle->FlushAttributeContents(pSoapSerializer));
        delete pEle;
        pEle = m_AttributeList.RemoveHead();
        }


    setIsSerialized();
    
Cleanup:
    ASSERT (hr == S_OK);
    return hr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElementStack::CElementStack()
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CElementStack::CElementStack()
{
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElementStack::~CElementStack()
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CElementStack::~CElementStack()
{
    HRESULT hr;

    hr = reset();
    ASSERT (hr == S_OK);
    return;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElementStack::reset()
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CElementStack::reset(void)
{
    HRESULT hr = S_OK;

    while (!m_ElementStack.IsEmpty())
    {
        CElementStackEntry    * pele;

        pele = Pop();
        delete pele;
    }
    
    ASSERT (hr == S_OK);
    return hr;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElementStack::Push(CElementStackEntry * pele)
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void CElementStack::Push(CElementStackEntry * pele)
{
#ifndef UNDER_CE
    return (m_ElementStack.InsertHead(pele));
#else
    //cant return w/ void
    m_ElementStack.InsertHead(pele);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElementStack::Pop()
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CElementStackEntry* CElementStack::Pop(void)
{
    return (m_ElementStack.RemoveHead());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElementStack::Peek()
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CElementStackEntry* CElementStack::Peek(void) const
{
    return (m_ElementStack.getHead());
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CElementStack::AddAttribute(WCHAR * prefix, WCHAR * ns, WCHAR * name, WCHAR * value)
//
//  parameters: 
//
//
//  description:
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CElementStack::AddAttribute(WCHAR * prefix, WCHAR * ns, WCHAR * name, WCHAR * value)
{
    CElementStackEntry * pele;

    pele = m_ElementStack.getHead();
    if (pele)
        return pele->AddAttribute(prefix, ns, name, value);

    return E_FAIL;
}



// End Of File




