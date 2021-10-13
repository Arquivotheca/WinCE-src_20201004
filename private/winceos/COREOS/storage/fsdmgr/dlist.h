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
//
// This source code is licensed under Microsoft Shared Source License
// Version 6.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

//
//    dlist.h - double linked list definition
//

#ifndef _NK_DLIST_H_
#define _NK_DLIST_H_

#ifdef __cplusplus 
extern "C" { 
#endif 

#define INVALID_LINK   ((PDLIST)(DWORD)(-1))

typedef struct _DLIST           DLIST, *PDLIST;                 // doubly linked list (circular)
typedef const DLIST             *PCDLIST;

struct _DLIST {
    PDLIST pFwd;
    PDLIST pBack;
};

// add an item to the head of a circular list
__inline void AddToDListHead (PDLIST pHead, PDLIST pItem)
{
    PDLIST pFwd  = pHead->pFwd;
    pItem->pFwd  = pFwd;
    pItem->pBack = pHead;
    pHead->pFwd  = pFwd->pBack = pItem;
}

// add an item to the tail a circular list
__inline void AddToDListTail (PDLIST pHead, PDLIST pItem)
{
    PDLIST pBack  = pHead->pBack;
    pItem->pBack  = pBack;
    pItem->pFwd   = pHead;
    pHead->pBack  = pBack->pFwd = pItem;
}

// remove an item from a circular list
__inline void RemoveDList (PDLIST pItem)
{
    pItem->pFwd->pBack = pItem->pBack;
    pItem->pBack->pFwd = pItem->pFwd;
    pItem->pFwd = pItem->pBack = pItem;
}

__inline void InitDList (PDLIST pHead)
{
    pHead->pBack = pHead->pFwd = pHead;
}

__inline BOOL IsDListEmpty (const DLIST *pHead)
{
    return pHead == pHead->pFwd;
}

typedef BOOL (* PFN_DLISTENUM) (PDLIST pItem, LPVOID pEnumData);

// EnumerateDList - enumerate DList, call pfnEnum on every item. Stop when pfnEnum returns TRUE, 
// or the list is exhausted. Return the item when pfnEnum returns TRUE, or NULL
// if list is exhausted.
PDLIST EnumerateDList (PDLIST phead, PFN_DLISTENUM pfnEnum, LPVOID pEnumData);

//
// DestroyDList - Remove all items in a DLIST, call pfnEnum on every item destroyed, return value ignored.
//
void DestroyDList (PDLIST phead, PFN_DLISTENUM pfnEnum, LPVOID pEnumData);

//
// MergeDList - merge two DList, the source DList will be emptied
// 
void MergeDList (PDLIST pdlDst, PDLIST pdlSrc);

#ifdef __cplusplus 
}
#endif 

#endif // _NK_DLIST_H_

