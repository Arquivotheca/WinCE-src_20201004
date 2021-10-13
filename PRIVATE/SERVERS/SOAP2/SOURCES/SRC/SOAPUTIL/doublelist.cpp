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
//      DoubleList.cpp
//
// Contents:
//
//      CDoubleList class implementation
//
//-----------------------------------------------------------------------------

#include "Headers.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CDoubleList::CDoubleList()
//
//  parameters:
//          
//  description:
//          CDoubleList constructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
CDoubleList::CDoubleList()
{
    m_Head.m_pNext = m_Head.m_pPrev = &m_Head;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CDoubleList::~CDoubleList()
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
CDoubleList::~CDoubleList()
{
    ASSERT( IsEmpty() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void CDoubleList::InsertHead(CDoubleListEntry *pEntry)
//
//  parameters:
//          CDoubleListEntry (must not be 0 and must have both m_pNext and m_pPrev == 0
//  description:
//          Inserts entry at the head of the queue
//  returns:
//          void
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDoubleList::InsertHead(CDoubleListEntry *pEntry)
{
    ASSERT(pEntry != 0);
    ASSERT(pEntry->m_pNext == 0);
    ASSERT(pEntry->m_pPrev == 0);

    pEntry->m_pNext = m_Head.m_pNext;
    pEntry->m_pPrev = &m_Head;

    m_Head.m_pNext->m_pPrev = pEntry;
    m_Head.m_pNext          = pEntry;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void CDoubleList::InsertTail(CDoubleListEntry *pEntry)
//
//  parameters:
//          CDoubleListEntry (must not be 0 and must have both m_pNext and m_pPrev == 0
//  description:
//          Inserts entry at the tail of the queue
//  returns:
//          void
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDoubleList::InsertTail(CDoubleListEntry *pEntry)
{
    ASSERT(pEntry != 0);
    ASSERT(pEntry->m_pNext == 0);
    ASSERT(pEntry->m_pPrev == 0);

    pEntry->m_pNext = &m_Head;
    pEntry->m_pPrev = m_Head.m_pPrev;

    m_Head.m_pPrev->m_pNext = pEntry;
    m_Head.m_pPrev          = pEntry;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void InsertBefore(CDoubleListEntry *pPosition, CDoubleListEntry *pEntry)
//
//  parameters:
//          
//  description:
//          
//  returns:
//          void
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDoubleList::InsertBefore(CDoubleListEntry *pPosition, CDoubleListEntry *pEntry)
{
    ASSERT(pPosition != 0);
    ASSERT(pEntry    != 0);
    ASSERT(pPosition->m_pNext != 0);
    ASSERT(pPosition->m_pPrev != 0);
    ASSERT(pEntry->m_pNext    == 0);
    ASSERT(pEntry->m_pPrev    == 0);

    pEntry->m_pNext = pPosition;
    pEntry->m_pPrev = pPosition->m_pPrev;

    pPosition->m_pPrev->m_pNext = pEntry;
    pPosition->m_pPrev          = pEntry;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void CDoubleList::InsertAfter(CDoubleListEntry *pPosition, CDoubleListEntry *pEntry)
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDoubleList::InsertAfter(CDoubleListEntry *pPosition, CDoubleListEntry *pEntry)
{
    ASSERT(pPosition != 0);
    ASSERT(pEntry    != 0);
    ASSERT(pPosition->m_pNext != 0);
    ASSERT(pPosition->m_pPrev != 0);
    ASSERT(pEntry->m_pNext    == 0);
    ASSERT(pEntry->m_pPrev    == 0);

    pEntry->m_pNext = pPosition->m_pNext;
    pEntry->m_pPrev = pPosition;

    pPosition->m_pNext->m_pPrev = pEntry;
    pPosition->m_pNext          = pEntry;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CDoubleListEntry *CDoubleList::getHead()
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
CDoubleListEntry *CDoubleList::getHead() const
{
    if ( IsEmpty() )
    {
        return 0;
    }

    return m_Head.m_pNext;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CDoubleListEntry *CDoubleList::getTail()
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
CDoubleListEntry *CDoubleList::getTail() const
{
    if ( IsEmpty() )
    {
        return 0;
    }

    return m_Head.m_pPrev;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CDoubleListEntry *CDoubleList::RemoveHead()
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
CDoubleListEntry *CDoubleList::RemoveHead()
{
    if ( IsEmpty() )
    {
        return 0;
    }

    CDoubleListEntry *pEntry = m_Head.m_pNext;

    ASSERT(pEntry != 0);
    ASSERT(pEntry != &m_Head);

    RemoveEntry(pEntry);

    ASSERT(pEntry->m_pNext == 0);
    ASSERT(pEntry->m_pPrev == 0);

    return pEntry;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CDoubleListEntry *CDoubleList::RemoveTail()
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
CDoubleListEntry *CDoubleList::RemoveTail()
{
    if ( IsEmpty() )
    {
        return 0;
    }

    CDoubleListEntry *pEntry = m_Head.m_pPrev;

    ASSERT(pEntry != 0);
    ASSERT(pEntry != &m_Head);

    RemoveEntry(pEntry);

    ASSERT(pEntry->m_pNext == 0);
    ASSERT(pEntry->m_pPrev == 0);

    return pEntry;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void CDoubleList::RemoveEntry(CDoubleListEntry *pEntry)
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDoubleList::RemoveEntry(CDoubleListEntry *pEntry)
{
    ASSERT(pEntry != 0);
    ASSERT(pEntry->m_pNext != 0);
    ASSERT(pEntry->m_pPrev != 0);
    ASSERT(pEntry != &m_Head);

    if (pEntry)
    {
        pEntry->m_pNext->m_pPrev = pEntry->m_pPrev;
        pEntry->m_pPrev->m_pNext = pEntry->m_pNext;
        pEntry->m_pNext = 0;
        pEntry->m_pPrev = 0;
    }    
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CDoubleListEntry * CDoubleList::next(CDoubleListEntry *pEntry)
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
CDoubleListEntry * CDoubleList::next(CDoubleListEntry *pEntry) const
{
    ASSERT(pEntry != 0);
    ASSERT(pEntry->m_pNext != 0);
    ASSERT(pEntry->m_pPrev != 0);
    ASSERT(pEntry != &m_Head);

	// in case we are at the end
    if (pEntry->m_pNext == &m_Head)
    	return NULL;

    return pEntry->m_pNext;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CDoubleListEntry * CDoubleList::prev(CDoubleListEntry *pEntry)
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
CDoubleListEntry * CDoubleList::prev(CDoubleListEntry *pEntry) const
{
    ASSERT(pEntry != 0);
    ASSERT(pEntry->m_pNext != 0);
    ASSERT(pEntry->m_pPrev != 0);
    ASSERT(pEntry != &m_Head);

	// in case we are at the beginning
    if (pEntry->m_pPrev == &m_Head)
    	return NULL;

    return pEntry->m_pPrev;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


