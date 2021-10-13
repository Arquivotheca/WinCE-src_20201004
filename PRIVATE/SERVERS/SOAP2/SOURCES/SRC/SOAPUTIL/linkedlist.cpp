//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      linkedlist.cpp
//
// Contents:
//
//      implementation of the CLinkedList class.
//
//----------------------------------------------------------------------------------

#include "Headers.h"

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLinkedList::CLinkedList(void (*pfncDeleteObjectFunction)(void *))
{
    // Initialize vars
    m_pFirstLink = NULL;
    m_pNextLink = NULL;
    m_pObject = NULL;

    // store function pointer to object destructor
    m_pfncDeleteObjectFunction = pfncDeleteObjectFunction;
}

CLinkedList::~CLinkedList()
{
#ifdef CE_NO_EXCEPTIONS
    __try
#else
    try
#endif
    {
        // delete next link prior to deleting this one
        if (m_pNextLink)
        {
            delete m_pNextLink;
            m_pNextLink = NULL;
        }

        // if an object was stored then call destructor code
        if (m_pObject)
        {
            (*m_pfncDeleteObjectFunction)(m_pObject);
            m_pObject = NULL;
        }
    }
#ifdef CE_NO_EXCEPTIONS
    __except(1)
#else
     catch(...)
#endif
    {
    }

}

// add unknown object to linked list
void CLinkedList::Add(void * pObject)
{
    // add to chain if existant
    if (m_pFirstLink)
    {
        m_pNextLink->m_pNextLink = new CLinkedList(m_pfncDeleteObjectFunction);
#ifdef UNDER_CE
        if(!m_pNextLink->m_pNextLink)
            return;
#endif       
        m_pNextLink = m_pNextLink->m_pNextLink;
    }
    // else create first link in chain
    else
    {
        m_pFirstLink = new CLinkedList(m_pfncDeleteObjectFunction);
#ifdef CE_NO_EXCEPTIONS
        if(!m_pFirstLink)
            return;
#endif       
        m_pNextLink = m_pFirstLink;
    }

    // store pointer to object
    // NOTE: we have no idea what this object is, so locking/addrefing/etc MUST
    // be done by caller.
    m_pNextLink->m_pObject = pObject;
    return;
}

// return first link in chain
void * CLinkedList::First(CLinkedList ** pLinkIndex)
{
    if (pLinkIndex)
        *pLinkIndex = m_pFirstLink->m_pNextLink;
    return m_pFirstLink->m_pObject;
}

// return next link in chain
void * CLinkedList::Next(CLinkedList ** pLinkIndex)
{
    void * pResult = NULL;
    CLinkedList * pLoopLink;

    if (pLinkIndex)
    {
        pLoopLink = *pLinkIndex;;
        if (pLoopLink)
        {
            pResult = pLoopLink->m_pObject;
            pLoopLink= pLoopLink->m_pNextLink;
            *pLinkIndex = pLoopLink;
        }
    }
    return pResult;
}

// delete list
void CLinkedList::DeleteList()
{
#ifdef CE_NO_EXCEPTIONS
    __try
#else
    try
#endif
    {
        delete m_pFirstLink;
        m_pFirstLink = NULL;
        m_pNextLink = NULL;
        m_pObject = NULL;
    }
#ifdef CE_NO_EXCEPTIONS
    __except(1)
#else
     catch(...)  
#endif
    {
    }
}
