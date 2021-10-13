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
