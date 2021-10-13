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
//      linkedlist.h
//
// Contents:
//
//      interface for the CLinkedList class.
//
//----------------------------------------------------------------------------------

#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__

class CLinkedList
{
public:
    CLinkedList(void (*pfncDeleteObjectFunction)(void *));
    ~CLinkedList();
    void Add(void * pObject);
    void DeleteList();
    void * First(CLinkedList ** pLinkIndex);
    void * Next(CLinkedList ** pLinkIndex);

protected:
    CLinkedList * m_pFirstLink;
    CLinkedList * m_pNextLink;
    // NOTE: we have no idea what this object is, so locking/addrefing/etc MUST
    // be done by caller.
    void (*m_pfncDeleteObjectFunction)(void *);
    void * m_pObject;
};

#endif // #define __LINKEDLIST_H__
