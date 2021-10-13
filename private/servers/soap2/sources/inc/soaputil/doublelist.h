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
//+----------------------------------------------------------------------------
//
//
// File:
//      DoubleList.h
//
// Contents:
//
//      CDoubleList - Double linked list class definition
//
//-----------------------------------------------------------------------------


#ifndef __DOUBLELIST_H_INCLUDED__
#define __DOUBLELIST_H_INCLUDED__


class CDoubleListEntry
{
public:
    CDoubleListEntry();

protected:
    CDoubleListEntry     *m_pNext;
    CDoubleListEntry     *m_pPrev;

    friend class CDoubleList;
};

class CDoubleList
{
    CDoubleListEntry     m_Head;

public:
    CDoubleList();
    ~CDoubleList();

    bool IsEmpty() const;

    void InsertHead(CDoubleListEntry *pEntry);
    void InsertTail(CDoubleListEntry *pEntry);
    void InsertBefore(CDoubleListEntry *pPosition, CDoubleListEntry *pEntry);
    void InsertAfter(CDoubleListEntry *pPosition, CDoubleListEntry *pEntry);

    CDoubleListEntry *RemoveHead();
    CDoubleListEntry *RemoveTail();

    CDoubleListEntry *prev(CDoubleListEntry *pEntry) const;
    CDoubleListEntry *next(CDoubleListEntry *pEntry) const;

    CDoubleListEntry *getHead() const;
    CDoubleListEntry *getTail() const;

    void RemoveEntry(CDoubleListEntry *pEntry);
    
    // Used in the case of an AV. Cannot delete memory since it may be corrupted.
    //  We have to live with memory leak in case of an AV.
    void ClearList(void) { m_Head.m_pNext = m_Head.m_pPrev = &m_Head; } ;

};


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline CDoubleListEntry::CDoubleListEntry()
//
//  parameters:
//          
//  description:
//          Constructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
inline CDoubleListEntry::CDoubleListEntry()
: m_pNext(0),
  m_pPrev(0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline CDoubleList::IsEmpty()
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool CDoubleList::IsEmpty() const
{
    return m_Head.m_pPrev == &m_Head;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


#endif //__DOUBLELIST_H_INCLUDED__
