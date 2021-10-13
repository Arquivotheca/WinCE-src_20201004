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
//+---------------------------------------------------------------------------------
//
//
// File:
//      typeinfomap.cpp
//
// Contents:
//
//      implementation of the CTypeInfoMap class.
//
//----------------------------------------------------------------------------------

#include "Headers.h"

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CTypeInfoMapInfo::CTypeInfoMapInfo(BSTR bstr, int nLen, DISPID id)
//
//  parameters:
//
//  description:
//        Constructor
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CTypeInfoMapInfo::CTypeInfoMapInfo(BSTR bstr, int nLen, DISPID id)
{
    m_bstr.Assign(bstr, FALSE);
    m_nLen = nLen;
    m_id = id;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CTypeInfoMapInfo::~CTypeInfoMapInfo()
//
//  parameters:
//
//  description:
//        Destructor
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CTypeInfoMapInfo::~CTypeInfoMapInfo()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CTypeInfoMap::CTypeInfoMap()
//
//  parameters:
//
//  description:
//        Constructor
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CTypeInfoMap::CTypeInfoMap()
{
    m_pList = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CTypeInfoMap::~CTypeInfoMap()
//
//  parameters:
//
//  description:
//        Destructor
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CTypeInfoMap::~CTypeInfoMap()
{
    if (m_pList)
    {
        m_pList->DeleteList();
        delete m_pList;
        m_pList = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CTypeInfoMapInfo * CTypeInfoMap::Add(BSTR bstr, int nLen, DISPID id)
//
//  parameters:
//
//  description:
//        Add typeinfo in the linked list
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CTypeInfoMapInfo * CTypeInfoMap::Add(BSTR bstr, int nLen, DISPID id)
{
    CTypeInfoMapInfo * pP;

    if (m_pList == NULL)
        m_pList = new CLinkedList(&DeleteObject);
        
#ifdef CE_NO_EXCEPTIONS
    if(!m_pList)
        return 0;
#endif
    
    pP = new CTypeInfoMapInfo(bstr, nLen, id);
    
#ifdef CE_NO_EXCEPTIONS
    if(!pP)
        return 0;
#endif
    
    m_pList->Add((void *)pP);
    return pP;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void CTypeInfoMap::DeleteObject(void * pObject)
//
//  parameters:
//
//  description:
//        Delete the object
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTypeInfoMap::DeleteObject(void * pObject)
{
    if (pObject)
    {
        delete (CTypeInfoMapInfo *)pObject;
        pObject = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CTypeInfoMapInfo * CTypeInfoMap::First(CLinkedList ** pLinkIndex)
//
//  parameters:
//
//  description:
//        Get first type info in the list
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CTypeInfoMapInfo * CTypeInfoMap::First(CLinkedList ** pLinkIndex)
{
    if (m_pList)
        return ((CTypeInfoMapInfo *)(m_pList->First(pLinkIndex)));
    else
        return (CTypeInfoMapInfo *)NULL;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CTypeInfoMapInfo * CTypeInfoMap::Next(CLinkedList ** pLinkIndex)
//
//  parameters:
//
//  description:
//        Get next type info in the list
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CTypeInfoMapInfo * CTypeInfoMap::Next(CLinkedList ** pLinkIndex)
{
    if (m_pList)
        return ((CTypeInfoMapInfo *)(m_pList->Next(pLinkIndex)));
    else
        return (CTypeInfoMapInfo *)NULL;
}
