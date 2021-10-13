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
//      typeinfomap.h
//
// Contents:
//
//      Interface for the CTypeInfoMap class.
//
//----------------------------------------------------------------------------------

#ifndef __TYPEINFOMAP_H_INCLUDED__
#define __TYPEINFOMAP_H_INCLUDED__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CTypeInfoMapInfo;
class CTypeInfoMap;

class CTypeInfoMapInfo
{
public:
    CTypeInfoMapInfo(BSTR bstr, int nLen, DISPID id);
    ~CTypeInfoMapInfo();
    
    CAutoBSTR m_bstr;
    int       m_nLen;
    DISPID    m_id;
};

class CTypeInfoMap  
{
public:
    CTypeInfoMap();
    ~CTypeInfoMap();
    static void DeleteObject(void * pObject);
    CTypeInfoMapInfo * Next(CLinkedList ** pLinkIndex);
    CTypeInfoMapInfo * First(CLinkedList ** pLinkIndex);
    CTypeInfoMapInfo * Add(BSTR bstr, int nLen, DISPID id);

private:
    CLinkedList * m_pList;
};

#endif  // __TYPEINFOMAP_H_INCLUDED__
