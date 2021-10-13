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
