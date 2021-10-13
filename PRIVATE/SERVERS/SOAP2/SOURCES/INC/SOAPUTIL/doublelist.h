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
