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

//------------------------------------------------------------------------------
//
//  Module Name:  
//  
//      list.c
//  
//  Abstract:  
//
//      Implements object list memory management for the Windows CE
//      Performance Measurement API.
//      
//------------------------------------------------------------------------------

#include <windows.h>
#include "ceperf.h"
#include "pceperf.h"


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Allocate and initialize a new list page
static ListPageHeader* AllocListPage(
    DLLListFuncs* pListFuncs,       // List operators
    WORD wListItemSize,             // List item size
    WORD wStartIndex                // Index of first item on the new page
    )
{
    ListPageHeader* pNewPage;
    LPBYTE pItem;
    WORD wPageIndex, wPageMax;

    pNewPage = AllocPage();
    if (pNewPage) {
        pNewPage->offsetNextPage = 0;
        pNewPage->wFirstFree     = 0;
        pNewPage->wStartIndex    = wStartIndex;
        
        // Initialize the items on the page
        pItem = LISTPAGE_PFIRSTOBJECT(pNewPage);
        wPageMax = MAX_LIST_ITEMS_PER_PAGE(wListItemSize);
        for (wPageIndex = 0; wPageIndex < wPageMax; wPageIndex++) {
            DEBUGCHK((DWORD)pItem + wListItemSize <= (DWORD)pNewPage + PGSIZE);
            pListFuncs->pFreeItem(pItem, wPageIndex+1);
            pItem += wListItemSize;
        }

        return pNewPage;
    }
    
    return NULL;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Frees list pages that no longer contain items in use.  Should be
// called once for a series of item deletes rather than once per deleted item.
VOID FreeUnusedListPages(
    DLLListFuncs* pListFuncs,       // List operators
    WORD   wListItemSize,           // List item size
    DWORD* poffsetListHeader        // List head pointer
    )
{
    ListPageHeader*  pHeader;
    DWORD *pOffset;
    LPBYTE pItem;
    WORD   wPageIndex, wPageMax;
    
    wPageMax = MAX_LIST_ITEMS_PER_PAGE(wListItemSize);
    pOffset = poffsetListHeader;
    pHeader = (ListPageHeader *)GET_MAP_POINTER(*pOffset);
    
    while (pHeader) {
        pItem = LISTPAGE_PFIRSTOBJECT(pHeader);
        for (wPageIndex = 0; (wPageIndex < wPageMax) && pItem; wPageIndex++) {
            DEBUGCHK((DWORD)pItem + wListItemSize <= (DWORD)pHeader + PGSIZE);
            if (pListFuncs->pIsItemFree(pItem)) {
                pItem += wListItemSize;
            } else {
                // Found an allocated item; this page can't be released
                pItem = NULL;
            }
        }
        
        if (wPageIndex < wPageMax) {
            // There are allocated items on the page; it cannot be released
            
            // Move to the next list page
            pOffset = &pHeader->offsetNextPage;
            pHeader = (ListPageHeader *)GET_MAP_POINTER(*pOffset);

        } else {
            // No allocated items on the page; it can be released
            ListPageHeader* pTemp;
            
            // Fix up the list pointers
            *pOffset = pHeader->offsetNextPage;                     // Remove the page from the list
            pTemp = pHeader;                                        // Save temp copy
            pHeader = (ListPageHeader *)GET_MAP_POINTER(*pOffset);  // Move to next list page
            
            // Now free the page
            FreePage(pTemp);
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Frees all list pages, whether they contain items in use or not.
VOID FreeAllListPages(
    DWORD* poffsetListHeader    // List head pointer
    )
{
    ListPageHeader* pHeader;

    pHeader = (ListPageHeader *)GET_MAP_POINTER(*(poffsetListHeader));
    while (pHeader) {
        ListPageHeader* pTemp;
        
        pTemp = pHeader;
        pHeader = (ListPageHeader *)GET_MAP_POINTER(pHeader->offsetNextPage);
        FreePage(pTemp);
    }

    *(poffsetListHeader) = 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Returns a pointer to a newly-allocated list item.  Does not do any
// initialization of the item.
LPBYTE AllocListItem(
    DLLListFuncs* pListFuncs,       // List operators
    WORD wListItemSize,             // List item size
    DWORD* poffsetListHeader   // List head offset
    )
{
    ListPageHeader* pHeader = NULL;
    LPBYTE pItem = NULL;

    // BUGBUG should probably alloc a page in the middle of the list whenever
    // there's a skip in index (a page in the middle previously freed)
    
    if (ZONE_VERBOSE && ZONE_MEMORY) {
        DEBUGMSG(1, (TEXT("BEFORE LIST ALLOC:\r\n")));
        DumpList(pListFuncs, wListItemSize, poffsetListHeader);
    }

    // First look for a page that has an empty slot
    if (*poffsetListHeader == 0) {
        // No pages at all

        // Allocate a new page
        pHeader = AllocListPage(pListFuncs, wListItemSize, 0);
        if (!pHeader) {
            return NULL;
        }
        *poffsetListHeader = GET_MAP_OFFSET(pHeader);
        goto UseFirstFree;

    } else {
        WORD wPageMax;
        
        pHeader = (ListPageHeader *)GET_MAP_POINTER(*(poffsetListHeader));
        wPageMax = MAX_LIST_ITEMS_PER_PAGE(wListItemSize);

        // Search for a list page with a free slot
        do {
            if (pHeader->wFirstFree < wPageMax) {
                // There's a free slot on the list page
                goto UseFirstFree;
            
            } else if (pHeader->offsetNextPage == 0) {
                // List page has no open slots and no following page;
                // allocate a new page
                
                // BUGBUG if lists are used for TrackedItems, need to block overflow in wStartIndex
//                if (pHeader->wStartIndex <= (WORD)-1 - wPageMax) {
                ListPageHeader *pTemp = AllocListPage(pListFuncs, wListItemSize,
                                                      pHeader->wStartIndex + wPageMax);
//                }
                if (!pTemp) {
                    return NULL;
                }
                pHeader->offsetNextPage = GET_MAP_OFFSET(pTemp);
                pHeader = pTemp;
                goto UseFirstFree;
            }
            pHeader = (ListPageHeader *)GET_MAP_POINTER(pHeader->offsetNextPage);
        
        } while (pHeader);
    }

    // Should not get here
    DEBUGCHK(0);    
    return NULL;

UseFirstFree:
    // pHeader points to a page which has at least one free slot; alloc & return
    pItem = LISTPAGE_PFIRSTOBJECT(pHeader)
            + (wListItemSize * pHeader->wFirstFree);   // Index into array
    pHeader->wFirstFree = pListFuncs->pGetNextFreeItem(pItem);
    
    if (ZONE_VERBOSE && ZONE_MEMORY) {
        DEBUGMSG(1, (TEXT("AFTER LIST ALLOC:\r\n")));
        DumpList(pListFuncs, wListItemSize, poffsetListHeader);
    }

    return pItem;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Marks the item as not in use.  Does not check to see if the page needs to be
// released, unless fReleasePage is TRUE.
HRESULT FreeListItem(
    DLLListFuncs* pListFuncs,       // List operators
    WORD   wListItemSize,           // List item size
    DWORD* poffsetListHeader,       // List head pointer
    LPBYTE pItem,
    BOOL   fReleasePages
    )
{
    ListPageHeader* pHeader;
    WORD wPageIndex;

    pHeader = (ListPageHeader*) ((DWORD)pItem & ~(PGSIZE-1));
    wPageIndex = (WORD) (((DWORD)pItem - (DWORD)pHeader - sizeof(ListPageHeader)) / wListItemSize);
    DEBUGCHK(wPageIndex < MAX_LIST_ITEMS_PER_PAGE(wListItemSize));
    
    // Mark the list item as being free
    pListFuncs->pFreeItem(pItem, pHeader->wFirstFree);
    pHeader->wFirstFree = wPageIndex;

    if (fReleasePages) {
        FreeUnusedListPages(pListFuncs, wListItemSize, poffsetListHeader);
    }
    
    return ERROR_SUCCESS;
}

