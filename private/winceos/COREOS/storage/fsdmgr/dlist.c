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
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:
    dlist.c

Abstract:
    Helper functions to work on double linked lists (used in coredll and kernel)

--*/
#include <windows.h>

#include "dlist.h"

//
// EnumerateDList - enumerate DList, call pfnEnum on every item. Stop when pfnEnum returns TRUE, 
// or the list is exhausted. Return the item when pfnEnum returns TRUE, or NULL
// if list is exhausted.
//
PDLIST EnumerateDList (PDLIST phead, PFN_DLISTENUM pfnEnum, LPVOID pEnumData)
{
    PDLIST ptrav, pnext;
    for (ptrav = phead->pFwd; ptrav != phead; ptrav = pnext) {
        pnext = ptrav->pFwd;
        if (pfnEnum (ptrav, pEnumData)) {
            return ptrav;
        }
    }
    return NULL;
}

//
// DestroyDList - Remove all items in a DLIST, call pfnEnum on every item destroyed, 
//                return value of pfnEnum is ignored.
//
void DestroyDList (PDLIST phead, PFN_DLISTENUM pfnEnum, LPVOID pEnumData)
{
    PDLIST ptrav;
    while ((ptrav = phead->pFwd) != phead) {
        RemoveDList (ptrav);
        pfnEnum (ptrav, pEnumData);
    }
}

//
// MergeDList - merge two DList, the source DList will be emptied
// 
void MergeDList (PDLIST pdlDst, PDLIST pdlSrc)
{
    if (!IsDListEmpty (pdlSrc)) {
        PDLIST pFwd  = pdlSrc->pFwd;    // 1st item in pdlSrc
        PDLIST pBack = pdlSrc->pBack;   // last item in pdlSrc

        pFwd->pBack = pdlDst->pBack;    // back ptr of 1st element set to the last element of dst DList
        pBack->pFwd = pdlDst;           // fwd ptr of last element set to the head of dst DList
        pdlDst->pBack->pFwd = pFwd;     // fwd ptr of the last element of dst DList set to 1st element of src DList
        pdlDst->pBack = pBack;          // back ptr of the head of dst DList set to last element of src DList

        InitDList (pdlSrc);             // empy the source DList
    }
}

