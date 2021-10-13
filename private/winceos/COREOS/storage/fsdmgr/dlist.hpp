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
#ifndef __DLIST_HPP__
#define __DLIST_HPP__

class DListNode_t;

template <typename DListNodeType>
class DList_t {    
public:
    DList_t () :
        m_pRoot (NULL),
        m_ItemCount (0)
    { ; }
    
    inline void AddItem (DListNodeType* pItem)
    {
        DEBUGCHK (pItem);
        DEBUGCHK (NULL == pItem->pPrev);
        DEBUGCHK (NULL == pItem->pNext);

        // Insert at the head of the list.
        pItem->pNext = m_pRoot;
        if (pItem->pNext) {
            pItem->pNext->pPrev = pItem;
        }
        m_pRoot = pItem;
        m_ItemCount ++;
    }
    
    inline void RemoveItem (DListNodeType* pItem)
    {
        DEBUGCHK (pItem->pNext || pItem->pPrev || 
            (1 == m_ItemCount && m_pRoot == pItem));
        DEBUGCHK (FindItem (pItem));
        if (pItem->pPrev) {
            pItem->pPrev->pNext = pItem->pNext;
        } else {
            m_pRoot = reinterpret_cast<DListNodeType*> (pItem->pNext);
        }
        if (pItem->pNext) {
            pItem->pNext->pPrev = pItem->pPrev;
        }
        m_ItemCount --;
        pItem->pNext = pItem->pPrev = NULL;
    }

    inline DWORD GetItemCount ()
    {
        return m_ItemCount;
    }

    inline DListNodeType* FirstItem () 
    {
        return m_pRoot;
    }

    inline DListNodeType* NextItem (DListNodeType* pItem)
    {
        return reinterpret_cast<DListNodeType*> (pItem->pNext);
    }
    
#ifdef DEBUG
    inline BOOL FindItem (DListNodeType* pItem)
    {
        for (DListNodeType* pCurItem = FirstItem ();
             pCurItem != NULL;
             pCurItem = NextItem (pCurItem))
        {
            if (pItem == pCurItem) {
                return TRUE;
            }                    
        }
        return FALSE;
    }
#endif

private:

    DListNodeType* m_pRoot;
    DWORD m_ItemCount;
};

// A list item object for the DLink template class. Any type used in
// the DLink template class must inherit from this DListNode_t class.
class DListNode_t {
public:
    DListNode_t () :
        pPrev (NULL),
        pNext (NULL)
    { ; }    

private:
    // DList_t template class can access these private members. Keep
    // subclasses from tampering with these values.
    template <typename DListNodeType>
    friend class DList_t;
    DListNode_t* pPrev;
    DListNode_t* pNext;
};

#endif // __DLIST_HPP__

