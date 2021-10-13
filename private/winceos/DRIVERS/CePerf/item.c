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
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//  PARTICULAR PURPOSE.
//
//------------------------------------------------------------------------------
//
//  Module Name:
//
//      item.c
//
//  Abstract:
//
//      Implements tracked item management for the Windows CE
//      Performance Measurement API.
//
//------------------------------------------------------------------------------

#include <windows.h>
#include "ceperf.h"
#include "ceperf_log.h"
#include "pceperf.h"


// Validate an ugly assumption
C_ASSERT(CEPERF_DURATION_RECORD_NONE == CEPERF_STATISTIC_RECORD_NONE);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Validation and preparation that every logging call has to do
BOOL ITEM_PROLOG(
    SessionHeader**  ppSession,
    TrackedItem**    ppItem,
    HANDLE           hTrackedItem,
    CEPERF_ITEM_TYPE type,          // Expected type
    HRESULT*         phResult       // Modified only if an error occurs
    )
{
    if (INVALID_HANDLE_VALUE != hTrackedItem) {
        *ppSession = LookupSessionHandle(ITEM_SESSION_HANDLE(hTrackedItem));
    } else {
        *ppSession = NULL;
    }

    if (*ppSession) {
        if (((*ppSession)->settings.dwStatusFlags & CEPERF_STATUS_NOT_THREAD_SAFE)
            || AcquireLoggerLock(&((*ppSession)->lock))) {

            *ppItem = LookupItemHandle(*ppSession, hTrackedItem);
            if (*ppItem && ((*ppItem)->type == type)) {
                if (!((*ppSession)->settings.dwStatusFlags & CEPERF_STATUS_RECORDING_DISABLED)
                    && !((*ppItem)->dwRecordingFlags & CEPERF_DURATION_RECORD_NONE)) {  // BUGBUG ugly assumption
                    // Successful return - hold the logger lock until the epilog
                    return TRUE;

                } else {
                    // Recording is disabled
                    *phResult = CEPERF_HR_RECORDING_DISABLED;
                }
            } else {
                // Invalid item handle or invalid item type
                *phResult = CEPERF_HR_INVALID_HANDLE;
            }

            // Failure return - release the logger lock
            if (!((*ppSession)->settings.dwStatusFlags & CEPERF_STATUS_NOT_THREAD_SAFE)) {
                ReleaseLoggerLock(&((*ppSession)->lock));
            }

        } else {
            // Failed to get session logger lock
            *phResult = CEPERF_HR_SHARING_VIOLATION;
        }
    } else {
        // Invalid session handle
        *phResult = CEPERF_HR_INVALID_HANDLE;
    }

    return FALSE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Cleanup that every logging call has to do
VOID ITEM_EPILOG(
    SessionHeader* pSession
    )
{
    if (!(pSession->settings.dwStatusFlags & CEPERF_STATUS_NOT_THREAD_SAFE)) {
        ReleaseLoggerLock(&(pSession->lock));
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
TrackedItem* LookupItemHandle(
    SessionHeader* pSession,
    HANDLE hItem
    )
{
    ItemPageHeader* pHeader;
    DWORD dwIndex;

    DEBUGCHK(pSession);
    if (pSession) {
        dwIndex = ITEM_INDEX(hItem);
        DEBUGCHK(dwIndex < MAX_ITEM_COUNT);
        if (dwIndex < MAX_ITEMS_ON_FIRST_PAGE) {
            // First page
            return SESSION_PFIRSTITEM(pSession) + dwIndex;
        } else {
            // Following page
            dwIndex -= MAX_ITEMS_ON_FIRST_PAGE;
            for (pHeader = (ItemPageHeader *)GET_MAP_POINTER(pSession->items.offsetNextPage); pHeader!= NULL; pHeader = (ItemPageHeader *)GET_MAP_POINTER( pHeader->offsetNextPage)) {
                if (dwIndex < MAX_ITEMS_ON_OTHER_PAGES) {
                    return ITEMPAGE_PFIRSTITEM(pHeader) + dwIndex;
                }
                dwIndex -= MAX_ITEMS_ON_OTHER_PAGES;
            }

            // Ran out of pages; it's a bad handle
            DEBUGCHK(!ZONE_ITEM);  // stops if ZONE_ITEM is turned on
        }
    }

    return NULL;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Allocate and initialize a new item page
_inline static ItemPageHeader* AllocItemPage(
    WORD wStartIndex        // Index of first item on the new page
    )
{
    ItemPageHeader* pNewPage;
    TrackedItem* pItem;
    DWORD dwPageIndex;

    pNewPage = AllocPage();
    if (pNewPage) {
        pNewPage->offsetNextPage = 0;
        pNewPage->wFirstFree = 0;
        pNewPage->wStartIndex = wStartIndex;

        // Initialize the items on the page
        pItem = ITEMPAGE_PFIRSTITEM(pNewPage);
        for (dwPageIndex = 0; dwPageIndex < MAX_ITEMS_ON_OTHER_PAGES; dwPageIndex++) {
            TRACKEDITEM_MARKFREE(pItem, dwPageIndex+1);
            pItem++;
        }

        return pNewPage;
    }

    return NULL;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Validate pExt and copy resulting values (mapped pointers, etc) to pNewExt
_inline static BOOL ValidateTypeDescriptor(
    const CEPERF_BASIC_ITEM_DESCRIPTOR* pBasic,
    const CEPERF_EXT_ITEM_DESCRIPTOR* pExt,
    CEPERF_EXT_ITEM_DESCRIPTOR* pNewExt
    )
{
    if (pBasic->type == CEPERF_TYPE_DURATION) {
        return ValidateDurationDescriptor(pBasic, pExt, pNewExt);
    }

    return ValidateStatisticDescriptor(pBasic, pExt, pNewExt);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
_inline static BOOL CompareTypeDescriptor(
    TrackedItem* pItem,
    const CEPERF_EXT_ITEM_DESCRIPTOR* pExt
    )
{
    // Cheating a bit, only LocalStatistic objects do anything right now
    if (pItem->type == CEPERF_TYPE_LOCALSTATISTIC) {
        return CompareStatisticDescriptor(pItem, pExt);
    }

    // Otherwise it matches
    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Used when the item is first created as well as to reinitialize data during
// CePerfControlCPU.
static HRESULT InitializeTypeData(
    HANDLE       hTrackedItem,
    TrackedItem* pItem,
    const CEPERF_EXT_ITEM_DESCRIPTOR* pExt  // Will be non-NULL on creation; will be NULL on recreation
    )
{
    BOOL fResult;

    if (pItem->type == CEPERF_TYPE_DURATION) {
        fResult = InitializeDurationData(hTrackedItem, pItem, pExt);
    } else {
        fResult = InitializeStatisticData(pItem, pExt);
    }

    // Change to HRESULT
    if (!fResult) {
        return CEPERF_HR_NOT_ENOUGH_MEMORY;
    }

    return ERROR_SUCCESS;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Helper function to pass to WalkAllItems.  Used to reinitialize data during
// CePerfControlCPU.
HRESULT Walk_InitializeTypeData(
    HANDLE hTrackedItem,
    TrackedItem* pItem,
    LPVOID lpWalkParam      // Unused
    )
{
    return InitializeTypeData(hTrackedItem, pItem, NULL);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Helper function to pass to WalkAllItems.  Used to change settings during
// CePerfControlSession.
HRESULT Walk_ChangeTypeSettings(
    HANDLE       hTrackedItem,
    TrackedItem* pItem,
    LPVOID       lpWalkParam    // const DWORD rgdwRecordingFlags[CEPERF_NUMBER_OF_TYPES]
    )
{
    const DWORD* prgdwRecordingFlags = (const DWORD*) lpWalkParam;
    BOOL fResult = TRUE;

    // Clear data for all items, even if their settings are not being changed.
    if (pItem->type == CEPERF_TYPE_DURATION) {
        // Clean up old data
        FreeDurationData(pItem, FALSE);

        // Change settings
        if (prgdwRecordingFlags[CEPERF_TYPE_DURATION] != 0) {
            pItem->dwRecordingFlags = prgdwRecordingFlags[CEPERF_TYPE_DURATION];
        }

        // Set up memory for storing new data
        fResult = InitializeDurationData(hTrackedItem, pItem, NULL);

    } else {
        // Clean up old data
        FreeStatisticData(pItem, FALSE);

        // Change settings
        if (prgdwRecordingFlags[pItem->type] != 0) {
            pItem->dwRecordingFlags = prgdwRecordingFlags[pItem->type];
        }

        // Set up memory for storing new data
        fResult = InitializeStatisticData(pItem, NULL);
    }

    // Change to HRESULT
    if (!fResult) {
        return CEPERF_HR_NOT_ENOUGH_MEMORY;
    }

    return ERROR_SUCCESS;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static HRESULT FreeTypeData(
    HANDLE       hTrackedItem,    // Ignored
    TrackedItem* pItem,
    BOOL         fReleasePages
    )
{
    DEBUGMSG((ZONE_ITEM || ZONE_MEMORY) && hTrackedItem,
             (TEXT("CePerf: Cleaning up %u leaked handles to hItem=0x%08x %s\r\n"),
              pItem->dwRefCount, hTrackedItem, pItem->szItemName));

    // Free any private RAM used by the item, but don't release pages yet
    if (pItem->type == CEPERF_TYPE_DURATION) {
        FreeDurationData(pItem, fReleasePages);
    } else {
        FreeStatisticData(pItem, fReleasePages);
    }
    TRACKEDITEM_MARKFREE(pItem, 0);

    return ERROR_SUCCESS;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Helper function to pass to WalkAllItems.
static HRESULT Walk_FreeTypeData(
    HANDLE       hTrackedItem,    // Ignored
    TrackedItem* pItem,
    LPVOID       lpWalkParam      // BOOL fReleasePages
    )
{
    return FreeTypeData(hTrackedItem, pItem, (BOOL) lpWalkParam);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HRESULT FlushItemDescriptorBinary(
    FlushState*      pFlush,
    HANDLE           hTrackedItem,
    TrackedItem*     pItem,
    CEPERF_ITEM_TYPE type
    )
{
    // Convenient declaration of the data that will be flushed
#pragma pack(push,4)
    typedef struct {
        CEPERF_LOGID id;
        CEPERF_LOG_ITEM_DESCRIPTOR item;
        WCHAR szItemName[CEPERF_MAX_ITEM_NAME_LEN];
    } Data;
#pragma pack(pop)

    DEBUGCHK(pFlush->pSession->settings.dwStorageFlags & CEPERF_STORE_CELOG);
    if (pFlush->lpTempBuffer && (pFlush->cbTempBuffer >= sizeof(Data))) {
        Data*  pData = (Data*) pFlush->lpTempBuffer;
        DWORD  cbData;
        size_t cchCopy;

        pData->id = CEPERF_LOGID_ITEM_DESCRIPTOR;
        pData->item.dwItemID = (DWORD) hTrackedItem;
        pData->item.type = type;
        pData->item.dwRecordingFlags = pItem->dwRecordingFlags;
        pData->item.ext.wVersion = 1;
        cbData = sizeof(CEPERF_LOGID) + sizeof(CEPERF_LOG_ITEM_DESCRIPTOR);

        if (SUCCEEDED(StringCchLength(pItem->szItemName, CEPERF_MAX_ITEM_NAME_LEN, &cchCopy))) {
            StringCchCopyN(pData->szItemName, CEPERF_MAX_ITEM_NAME_LEN, pItem->szItemName, cchCopy);
        } else {
            pData->szItemName[0] = 0;
            cchCopy = 0;
        }
        cbData += (cchCopy+1)*sizeof(WCHAR);

        // Write the buffer to the output location
        return FlushBytes(pFlush, (LPBYTE) pData, cbData);
    }

    DEBUGCHK(0);  // Underestimated necessary buffer size
    return CEPERF_HR_NOT_ENOUGH_MEMORY;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Initialize the first free item on the page with this data.
static BOOL InitializeNewItem(
    ItemPageHeader* pHeader,                // Item page to add the item to;
                                            // if NULL, the item is being added
                                            // to the session header page
    HANDLE hSession,                        // Session handle is just used to create item handles
    SessionHeader* pSession,
    CEPERF_BASIC_ITEM_DESCRIPTOR* pBasic,   // Item handle will be set in the struct
    const CEPERF_EXT_ITEM_DESCRIPTOR* pExt
    )
{
    HANDLE hTrackedItem;
    TrackedItem* pItem;
    WORD*  pwFirstFree;
    WORD   wNextFree;

    if (!pHeader) {
        // Session header page
        DEBUGCHK(pSession->items.wFirstFree < MAX_ITEMS_ON_FIRST_PAGE);
        pItem = SESSION_PFIRSTITEM(pSession);
        pItem += pSession->items.wFirstFree;  // Index into array
    } else {
        // Other pages
        DEBUGCHK(pHeader->wFirstFree < MAX_ITEMS_ON_OTHER_PAGES);
        pItem = ITEMPAGE_PFIRSTITEM(pHeader);
        pItem += pHeader->wFirstFree;  // Index into array
    }
    wNextFree = TRACKEDITEM_NEXTFREE(pItem);  // NextFree can now be overwritten

    // Copy the item descriptor
    wcsncpy(pItem->szItemName, pBasic->lpszItemName, CEPERF_MAX_ITEM_NAME_LEN);
    pItem->szItemName[CEPERF_MAX_ITEM_NAME_LEN-1] = 0;
    pItem->dwRefCount = 1;
    pItem->type = pBasic->type;
    if ((pBasic->dwRecordingFlags == 0)
        || (pBasic->dwRecordingFlags == CEPERF_DEFAULT_FLAGS)) {
        // Unspecified recording flags means they want to use the session settings
        DEBUGCHK(pSession->settings.rgdwRecordingFlags[pBasic->type] != 0);  // BUGBUG this case NYI
        pItem->dwRecordingFlags = pSession->settings.rgdwRecordingFlags[pBasic->type];
    } else {
        pItem->dwRecordingFlags = pBasic->dwRecordingFlags;
    }

    // Figure out what the item handle is going to be, but don't commit that until the init succeeds
    if (!pHeader) {
        // Session header page
        hTrackedItem = MAKE_ITEM_HANDLE(hSession, pSession->items.wFirstFree);
        pwFirstFree = &pSession->items.wFirstFree;
    } else {
        // Other pages
        hTrackedItem = MAKE_ITEM_HANDLE(hSession, pHeader->wStartIndex + pHeader->wFirstFree);
        pwFirstFree = &pHeader->wFirstFree;
    }

    // Initialize the item's data; may fail if private RAM can't be allocated
    if (InitializeTypeData(hTrackedItem, pItem, pExt) != ERROR_SUCCESS) {
        TRACKEDITEM_MARKFREE(pItem, wNextFree);
        return FALSE;
    }

    // Return the item handle in the struct
    pBasic->hTrackedItem = hTrackedItem;
    *pwFirstFree = wNextFree;
    pSession->dwNumObjects++;

    DEBUGMSG(ZONE_ITEM || ZONE_MEMORY,
             (TEXT("CePerf: New hItem=0x%08x pItem=0x%08x\r\n"),
              pBasic->hTrackedItem, pItem));

    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Create an item that does not already exist in the session.
static HRESULT CreateNewItem(
    HANDLE hSession,                        // Session handle is just used to create item handles
    SessionHeader* pSession,
    CEPERF_BASIC_ITEM_DESCRIPTOR* pBasic,   // Item handle will be set in the struct
    const CEPERF_EXT_ITEM_DESCRIPTOR* pExt
    )
{
    ItemPageHeader* pHeader = NULL;
    ItemPageHeader* pTemp = NULL;
    DWORD *         poffsetNewPage = NULL;   // Pointer to pointer to newly-allocated
                                               // page, in case it must be freed again

    if (ZONE_VERBOSE && ZONE_ITEM) {
        DEBUGMSG(1, (TEXT("BEFORE TRACKED ITEM ALLOC:\r\n")));
        DumpSessionItemList(hSession, pSession, FALSE);
    }

    // pBasic->hTrackedItem has already been initialized

    if (pSession->dwNumObjects < MAX_ITEM_COUNT) {
        // First look for a page that has an empty slot
        if (pSession->items.wFirstFree < MAX_ITEMS_ON_FIRST_PAGE) {
            // There's a free slot on the session header page
            goto DoInit;

        } else if (pSession->items.offsetNextPage == 0) {
            // Session header has no open slots and no following page

            // Allocate a new page
            pHeader = AllocItemPage(MAX_ITEMS_ON_FIRST_PAGE);
            if (!pHeader) {
                return CEPERF_HR_NOT_ENOUGH_MEMORY;
            }
            pSession->items.offsetNextPage = GET_MAP_OFFSET(pHeader);
            poffsetNewPage = &pSession->items.offsetNextPage;  // Track pointer to new page
            goto DoInit;

        } else {
            pHeader = (ItemPageHeader *)GET_MAP_POINTER(pSession->items.offsetNextPage);

            // Search for a tracked item page with a free slot
            do {
                if (pHeader->wFirstFree < MAX_ITEMS_ON_OTHER_PAGES) {
                    // There's a free slot on the item page
                    goto DoInit;

                } else if (pHeader->offsetNextPage == 0) {
                    // Item page has no open slots and no following page;
                    // allocate a new page
                    pTemp = AllocItemPage(pHeader->wStartIndex + MAX_ITEMS_ON_OTHER_PAGES);
                    pHeader->offsetNextPage = GET_MAP_OFFSET(pTemp);
                    if (0 == pHeader->offsetNextPage) {
                        return CEPERF_HR_NOT_ENOUGH_MEMORY;
                    }

                    poffsetNewPage = &pHeader->offsetNextPage;  // Track pointer to new page
                    pHeader = pTemp;
                    goto DoInit;
                }
                pHeader = (ItemPageHeader *)GET_MAP_POINTER(pHeader->offsetNextPage);

            } while (pHeader);
        }
    }

DoInit:
    if (!InitializeNewItem(pHeader, hSession, pSession, pBasic, pExt)) {
        // If we just allocated a new page, we must free it and fix up the pointer
        if (poffsetNewPage) {
            FreePage((LPVOID)GET_MAP_POINTER(*poffsetNewPage));   // Free the page
            *poffsetNewPage = 0;      // Fix up the offsetNextPage in the session header or ItemPageHeader
        }
        return CEPERF_HR_NOT_ENOUGH_MEMORY;
    }

    if (ZONE_VERBOSE && ZONE_ITEM) {
        DEBUGMSG(1, (TEXT("AFTER TRACKED ITEM ALLOC:\r\n")));
        DumpSessionItemList(hSession, pSession, FALSE);
    }

    return ERROR_SUCCESS;
}


// Used to pass data to and from CompareItemName during a find.
typedef struct {
    LPWSTR       szItemName;    // Input to find
    HANDLE       hTrackedItem;  // Return value from a successful find
    TrackedItem* pItem;         // Return value from a successful find
} FindData;


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Helper function to pass to WalkAllItems.
static HRESULT Walk_CompareItemName(
    HANDLE       hTrackedItem,
    TrackedItem* pItem,
    LPVOID       lpWalkParam    // FindData* pFindData
    )
{
    FindData* pFindData = (FindData*) lpWalkParam;

    if (!wcsnicmp(pItem->szItemName, pFindData->szItemName, CEPERF_MAX_ITEM_NAME_LEN)) {
        // Found it
        pFindData->pItem = pItem;
        pFindData->hTrackedItem = hTrackedItem;

        return CEPERF_HR_ALREADY_EXISTS;    // Return an "error" to stop walking
    }

    return ERROR_SUCCESS;   // Keep walking
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static HRESULT FindOrCreateItem(
    HANDLE hSession,                        // Session handle is just used to create item handles
    SessionHeader* pSession,
    CEPERF_BASIC_ITEM_DESCRIPTOR* pBasic,   // Item handle will be set in the struct
    const CEPERF_EXT_ITEM_DESCRIPTOR* pExt
    )
{
    FindData data;
    HRESULT hResult;
    CEPERF_EXT_ITEM_DESCRIPTOR NewExt;  // Writeable copy of pExt

    pBasic->hTrackedItem = INVALID_HANDLE_VALUE;

    // Validate parameters
    if (pBasic->type >= CEPERF_NUMBER_OF_TYPES) {
        DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Registration failed, invalid item type %u\r\n"), pBasic->type));
        return CEPERF_HR_INVALID_PARAMETER;
    } else if (!pBasic->lpszItemName) {
        DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Registration failed, item name is null\r\n")));
        return CEPERF_HR_INVALID_PARAMETER;
    } else if (wcslen(pBasic->lpszItemName) > CEPERF_MAX_ITEM_NAME_LEN-1) {
        DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Registration failed, item name is greater than %u chars\r\n"),
                 CEPERF_MAX_ITEM_NAME_LEN));
        return CEPERF_HR_INVALID_PARAMETER;
    } else if (!ValidateRecordingFlags(pBasic->dwRecordingFlags, pBasic->type)
               || !ValidateTypeDescriptor(pBasic, pExt, &NewExt)) {
        return CEPERF_HR_INVALID_PARAMETER;
    }
    // Now use NewExt instead of pExt


    /**
     *
     * When CEPERF_STORE_CELOG is one of the StorageFlags, force all
     * CEPERF_DURATION_RECORD_MIN durations to be CEPERF_DURATION_RECORD_FULL.
     *
     **/
    //
    // In CE we do not want this behavior.  
    // This code has been commented on purpose and left in to make any 
    // potential integrations in the future easier.
    //
    //if (pSession->settings.dwStorageFlags & CEPERF_STORE_CELOG)
    //{
    //    pBasic->dwRecordingFlags = (pBasic->dwRecordingFlags &
    //        ~(CEPERF_DURATION_RECORD_MIN)) | CEPERF_DURATION_RECORD_FULL;
    //}

    // Determine whether the item is already registered for this session by
    // looking for it by name.

    // Indirect method of passing and receiving data from CompareItemName
    data.szItemName   = pBasic->lpszItemName;
    data.hTrackedItem = INVALID_HANDLE_VALUE;
    data.pItem        = NULL;

    // BUGBUG linear search, may need to speed this up?
    hResult = WalkAllItems(hSession, pSession, Walk_CompareItemName, (LPVOID)&data, FALSE);
    if (hResult == CEPERF_HR_ALREADY_EXISTS) {  // CompareItemName returns this value
        // Found the item

        // When the item already exists with different recording flags,
        // we keep the existing item settings and succeed the open.
        // However the rest of the settings must match.
        if ((data.pItem->type != pBasic->type)
            || !CompareTypeDescriptor(data.pItem, &NewExt)) {
            return CEPERF_HR_ALREADY_EXISTS;
        }

        // Increment the refcount and return the handle
        data.pItem->dwRefCount++;
        pBasic->hTrackedItem = data.hTrackedItem;

        DEBUGMSG(ZONE_ITEM || ZONE_MEMORY,
                 (TEXT("CePerf: Open hItem=0x%08x pItem=0x%08x\r\n"),
                  pBasic->hTrackedItem, data.pItem));

        return ERROR_SUCCESS;
    }

    // Not found -- add it
    return CreateNewItem(hSession, pSession, pBasic, &NewExt);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static ItemPageHeader* ReverseList(
    ItemPageHeader* pOldHead
    )
{
    ItemPageHeader* pNewHead;
    ItemPageHeader* pTemp;

    pNewHead = NULL;
    while (pOldHead) {
        // Remove from old list
        pTemp = pOldHead;
        pOldHead = (ItemPageHeader *)GET_MAP_POINTER(pOldHead->offsetNextPage);

        // Add to new list
        pTemp->offsetNextPage = GET_MAP_OFFSET(pNewHead);
        pNewHead = pTemp;
    }

    return pNewHead;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Frees tracked item pages that no longer contain items in use.  Should be
// called once for a series of item deletes rather than once per deleted item.
static BOOL ReleaseItemPages(
    SessionHeader* pSession
    )
{
    ItemPageHeader* pListHead;
    ItemPageHeader* pTemp;
    TrackedItem*    pItem;
    WORD            wPageIndex;    // Index on the current page

    // Need to start with last page and move backwards, because we cannot free
    // pages in the middle since items are referred to via their indexes.
    // Cannot free the page with the session header so don't bother with it.
    pListHead = ReverseList((ItemPageHeader *)GET_MAP_POINTER(pSession->items.offsetNextPage));

    while (pListHead) {
        pItem = ITEMPAGE_PFIRSTITEM(pListHead);
        for (wPageIndex = 0; wPageIndex < MAX_ITEMS_ON_OTHER_PAGES; wPageIndex++) {
            // Sanity-check on type in case the pointer is misaligned somehow
            DEBUGCHK(TRACKEDITEM_ISFREE(pItem) || (pItem->type < CEPERF_NUMBER_OF_TYPES));

            if (!TRACKEDITEM_ISFREE(pItem)) {
                // Found an allocated item; this page can't be released
                goto exit;
            }
            pItem++;
        }

        // No allocated items on the page; it can be released
        pTemp = pListHead;
        pListHead = (ItemPageHeader *)GET_MAP_POINTER(pListHead->offsetNextPage);
        FreePage(pTemp);
    }

exit:
    pSession->items.offsetNextPage = GET_MAP_OFFSET(ReverseList(pListHead));

    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Marks the item as not in use.  Does not check to see if the page needs to be
// released, unless fReleasePage is TRUE.
static HRESULT DeleteItem(
    SessionHeader* pSession,
    HANDLE hItem,
    BOOL fReleasePages
    )
{
    TrackedItem* pItem;
    WORD wIndex;

    pItem = LookupItemHandle(pSession, hItem);
    if (pItem) {
        pItem->dwRefCount--;
        if (pItem->dwRefCount == 0) {
            DEBUGMSG(ZONE_ITEM || ZONE_MEMORY,
                     (TEXT("CePerf: Free hItem=0x%08x pItem=0x%08x\r\n"),
                      hItem, pItem));

            // Free any private RAM used by the item
            FreeTypeData(0, pItem, fReleasePages);

            // Mark the item as being unused
            pSession->dwNumObjects--;
            wIndex = ITEM_INDEX(hItem);
            if (wIndex < MAX_ITEMS_ON_FIRST_PAGE) {
                TRACKEDITEM_MARKFREE(pItem, pSession->items.wFirstFree);
                pSession->items.wFirstFree = wIndex;
            } else {
                ItemPageHeader* pHeader;

                pHeader = (ItemPageHeader*) ((DWORD)pItem & ~(PGSIZE-1));
                TRACKEDITEM_MARKFREE(pItem, pHeader->wFirstFree);
                pHeader->wFirstFree = ((wIndex - MAX_ITEMS_ON_FIRST_PAGE) % MAX_ITEMS_ON_OTHER_PAGES);

                if (fReleasePages) {
                    // It will only be possible to free up memory if this is the last page
                    if (pHeader->offsetNextPage == 0) {
                        ReleaseItemPages(pSession);
                    }
                }
            }
        }

        return ERROR_SUCCESS;
    }

    return CEPERF_HR_INVALID_HANDLE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Deletes all items in the session, along with their extended resources, and
// releases all item pages.
HRESULT DeleteAllItems(
    HANDLE         hSession, // Session handle is just used to create item handles
    SessionHeader* pSession
    )
{
    ItemPageHeader* pHeader;

    // If there are any tracked items still open, then someone closed their
    // session handle without deregistering their tracked items.  Items aren't
    // refcounted per session handle so they can't be cleaned up until the last
    // session handle is closed.
    DEBUGMSG(pSession->dwNumObjects,
             (TEXT("CePerf: Deleting %u tracked items still open in hSession=0x%08x\r\n"),
              pSession->dwNumObjects, hSession));

    // Run FreeTypeData on all items, to free private RAM and mark the items
    // as free.
    WalkAllItems(hSession, pSession, Walk_FreeTypeData, (LPVOID)FALSE, TRUE);
    pSession->dwNumObjects = 0;

    // Free item pages in a separate loop so that they are guaranteed to be
    // freed, even if the number of open item handles somehow got out of sync
    pHeader = (ItemPageHeader *)GET_MAP_POINTER(pSession->items.offsetNextPage);
    while (pHeader) {
        ItemPageHeader* pTempHeader = pHeader;
        pHeader = (ItemPageHeader *)GET_MAP_POINTER(pHeader->offsetNextPage);
        FreePage(pTempHeader);
    }
    pSession->items.offsetNextPage = 0;

    // Free RAM from the lists that span all sessions
    FreeUnusedDurationPages();
    FreeUnusedStatisticPages();

    return ERROR_SUCCESS;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Walk all items in a session.  For each item, call the supplied function with
// the supplied argument.  Keeps walking until pItemFunction returns something
// other than ERROR_SUCCESS.
HRESULT WalkAllItems(
    HANDLE           hSession,      // Session handle is just used to create item handles
    SessionHeader*   pSession,
    PFN_ItemFunction pItemFunction, // Function to call on all items
    LPVOID           lpWalkParam,   // Argument to pass to function
    BOOL             fForce         // TRUE=continue walking if encounter failure,
                                    // FALSE=stop walking if encounter failure
    )
{
    ItemPageHeader* pHeader;
    TrackedItem*    pItem;
    DWORD           dwPageIndex, dwIndex, dwItemsFound;
    HRESULT         hResult = ERROR_SUCCESS;
    HRESULT         hrTemp;

    dwPageIndex = 0;    // Index on the current page
    dwIndex = 0;        // Index overall within the session
    dwItemsFound = 0;   // Number of in-use items found

    pItem = SESSION_PFIRSTITEM(pSession);
    pHeader = NULL;

    while (dwItemsFound < pSession->dwNumObjects) {

        DEBUGCHK(dwIndex < MAX_ITEM_COUNT);
        if (!TRACKEDITEM_ISFREE(pItem)) {
            dwItemsFound++;
            hrTemp = pItemFunction(MAKE_ITEM_HANDLE(hSession, dwIndex),
                                   pItem, lpWalkParam);
            if (hrTemp != ERROR_SUCCESS) {
                hResult = hrTemp;
                if (!fForce) {
                    return hResult;
                }
            }
        }
        DEBUGCHK(dwItemsFound <= pSession->dwNumObjects);

        // Increment counter and pointer
        dwPageIndex++;
        dwIndex++;
        pItem++;

        // Deal with page wrap
        if (!pHeader) {
            // First page
            DEBUGCHK(dwIndex <= MAX_ITEMS_ON_FIRST_PAGE);
            if (dwPageIndex >= MAX_ITEMS_ON_FIRST_PAGE) {
                // Go to the next page
                dwPageIndex = 0;
                if (pSession->items.offsetNextPage == 0) {
                    // Should only happen when we're done iterating
                    DEBUGCHK(dwItemsFound == pSession->dwNumObjects);
                    return hResult;
                }
                pHeader = (ItemPageHeader *)GET_MAP_POINTER(pSession->items.offsetNextPage);
                pItem = ITEMPAGE_PFIRSTITEM(pHeader);
                DEBUGCHK(dwIndex == pHeader->wStartIndex);
            }
        } else {
            // Following page
            if (dwPageIndex >= MAX_ITEMS_ON_OTHER_PAGES) {
                // Go to the next page
                dwPageIndex = 0;
                if (pHeader->offsetNextPage == 0) {
                    // Should only happen when we're done iterating
                    DEBUGCHK(dwItemsFound == pSession->dwNumObjects);
                    return hResult;
                }
                pHeader = (ItemPageHeader *)GET_MAP_POINTER(pHeader->offsetNextPage);
                pItem = ITEMPAGE_PFIRSTITEM(pHeader);
                DEBUGCHK(dwIndex == pHeader->wStartIndex);
            }
        }
    }

    return hResult;
}


//------------------------------------------------------------------------------
// CePerfRegisterTrackedItem - Register a perf item within an open session
//------------------------------------------------------------------------------
HRESULT CePerf_RegisterTrackedItem(
    HANDLE  hSession,           // Perf session the tracked item belongs to
    HANDLE* lphTrackedItem,     // Will receive handle to identify item with,
                                // can be NULL; if NULL the item will be tracked
                                // until the last session handle is closed,
                                // since it cannot be deregistered
    CEPERF_ITEM_TYPE type,      // Type of tracked item
    LPCWSTR lpszItemName,       // Name of tracked item (NULL-terminated, limit
                                // 32 chars, case-insensitive). If the same name
                                // is registered twice in the same session, both
                                // instances will refer to the
                                // same item.
    DWORD   dwRecordingFlags,   // Flags for default recording mode (may be
                                // overridden by session defaults or dynamic
                                // session control)
    const CEPERF_EXT_ITEM_DESCRIPTOR* lpInfo  // Pointer to extended information
                                // about tracked item, or NULL
    )
{
    HRESULT hResult = CEPERF_HR_INVALID_HANDLE;
    SessionHeader* pSession;

    DEBUGMSG(ZONE_API && ZONE_ITEM,
             (TEXT("+CePerfRegisterTrackedItem(0x%08x, %u, %s, 0x%08x, 0x%08x)\r\n"),
              hSession, type, lpszItemName, dwRecordingFlags, lpInfo));

    if (lphTrackedItem) {
        *lphTrackedItem = INVALID_HANDLE_VALUE;
    }

    if (!g_hMasterMutex) {
        // No mutex means there aren't any open handles
        goto exit;
    }

    AcquireControllerLock();
    // Registration does not require logger lock control
    __try {

        pSession = LookupSessionHandle(hSession);
        if (pSession) {
            CEPERF_BASIC_ITEM_DESCRIPTOR item;

            item.hTrackedItem = INVALID_HANDLE_VALUE;
            item.type = type;
            item.lpszItemName = (LPWSTR)lpszItemName;  // not const but won't be modified
            item.dwRecordingFlags = dwRecordingFlags;

            hResult = FindOrCreateItem(hSession, pSession, &item, lpInfo);
            if ((hResult == ERROR_SUCCESS) && lphTrackedItem) {
                *lphTrackedItem = item.hTrackedItem;
            }

            if (ZONE_ITEM && (item.hTrackedItem != INVALID_HANDLE_VALUE)) {
                DumpTrackedItem(item.hTrackedItem,
                                LookupItemHandle(pSession, item.hTrackedItem),
                                (LPVOID)FALSE);
            }
        }

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Safety catch so that we release the lock no matter what
        DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Exception in CePerfRegisterTrackedItem!\r\n")));
        hResult = CEPERF_HR_EXCEPTION;
    }
    ReleaseControllerLock();

exit:
    DEBUGMSG(ZONE_API && ZONE_ITEM, (TEXT("-CePerfRegisterTrackedItem (0x%08x)\r\n"), hResult));

    return hResult;
}


//------------------------------------------------------------------------------
// CePerfDeregisterTrackedItem - Deregister a previously registered item
//------------------------------------------------------------------------------
HRESULT CePerf_DeregisterTrackedItem(
    HANDLE hTrackedItem         // Handle of item being deregistered
    )
{
    HRESULT hResult = CEPERF_HR_INVALID_HANDLE;
    HANDLE  hSession;
    SessionHeader* pSession;

    DEBUGMSG(ZONE_API && ZONE_ITEM,
             (TEXT("+CePerfDeregisterTrackedItem(0x%08x)\r\n"),
              hTrackedItem));

    if (!g_hMasterMutex) {
        // No mutex means there aren't any open handles
        goto exit;
    }

    AcquireControllerLock();
    __try {

        hSession = ITEM_SESSION_HANDLE(hTrackedItem);
        pSession = LookupSessionHandle(hSession);
        if (pSession) {
            AcquireLoggerLockControl(&pSession->lock);  // Block loggers during deregistration
            hResult = DeleteItem(pSession, hTrackedItem, TRUE);
            ReleaseLoggerLockControl(&pSession->lock);
        }

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Safety catch so that we release the lock no matter what
        DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Exception in CePerfDeregisterTrackedItem!\r\n")));
        hResult = CEPERF_HR_EXCEPTION;
    }
    ReleaseControllerLock();

exit:
    DEBUGMSG(ZONE_API && ZONE_ITEM, (TEXT("-CePerfDeregisterTrackedItem (0x%08x)\r\n"), hResult));

    return hResult;
}


//------------------------------------------------------------------------------
// CePerfRegisterBulk - Bulk-register a set of very basic perf items within an
// open session.  Handles for all items which succeed will be assigned to their
// structs.  Only the most basic information can be registered in bulk.  Any
// item that requires extended settings must be registered individually.
//------------------------------------------------------------------------------
HRESULT CePerf_RegisterBulk(
    HANDLE  hSession,           // Perf session the tracked items belong to
    CEPERF_BASIC_ITEM_DESCRIPTOR* lprgItemList, // Array of items to register
    DWORD   dwNumItems,         // Number of descriptors in array
    DWORD   dwReserved          // Not currently used; set to 0
    )
{
    HRESULT hResult = CEPERF_HR_INVALID_HANDLE;
    SessionHeader* pSession;

    DEBUGMSG(ZONE_API && ZONE_ITEM,
             (TEXT("+CePerfRegisterBulk(0x%08x, 0x%08x, %u)\r\n"),
              hSession, lprgItemList, dwNumItems));

    if (!lprgItemList || (dwNumItems == 0) || (dwReserved != 0)) {
        hResult = CEPERF_HR_INVALID_PARAMETER;
        goto exit;
    }

    if (!g_hMasterMutex) {
        // No mutex means there aren't any open handles
        goto exit;
    }

    AcquireControllerLock();
    // Registration does not require logger lock control
    __try {

        pSession = LookupSessionHandle(hSession);
        if (pSession) {
            HRESULT hrTemp;
            DWORD   i;

            // Start with success and overwrite with each error, so that the return
            // value is the most recent error
            hResult = ERROR_SUCCESS;
            for (i = 0; i < dwNumItems; i++) {
                hrTemp = FindOrCreateItem(hSession, pSession, &lprgItemList[i], NULL);
                if (hrTemp != ERROR_SUCCESS) {
                    DEBUGMSG(ZONE_ITEM, (TEXT("Error %u on bulk item %u\r\n"), hrTemp, i));
                    hResult = hrTemp;
                }
            }

            if (ZONE_ITEM && (hResult == ERROR_SUCCESS)) {
                DumpSession(hSession, TRUE, FALSE);
            }
        }

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Safety catch so that we release the lock no matter what
        DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Exception in CePerfRegisterBulk!\r\n")));
        hResult = CEPERF_HR_EXCEPTION;
    }
    ReleaseControllerLock();

exit:
    DEBUGMSG(ZONE_API && ZONE_ITEM, (TEXT("-CePerfRegisterBulk (0x%08x)\r\n"), hResult));

    return hResult;
}


//------------------------------------------------------------------------------
// CePerfDeregisterBulk - Bulk-deregister a set of very basic perf items.
// Handles for all items will be set to INVALID_HANDLE_VALUE in their structs.
//------------------------------------------------------------------------------
HRESULT CePerf_DeregisterBulk(
    CEPERF_BASIC_ITEM_DESCRIPTOR* lprgItemList, // Array of items to deregister
    DWORD   dwNumItems          // Number of descriptors in array
    )
{
    HRESULT hResult = CEPERF_HR_INVALID_HANDLE;
    HANDLE  hSession = INVALID_HANDLE_VALUE;
    SessionHeader* pSession = NULL;
    DWORD   i;

    DEBUGMSG(ZONE_API && ZONE_ITEM,
             (TEXT("+CePerfDeregisterBulk(0x%08x, %u)\r\n"),
              lprgItemList, dwNumItems));

    if (!lprgItemList || (dwNumItems == 0)) {
        hResult = CEPERF_HR_INVALID_PARAMETER;
        goto exit;
    }

    if (!g_hMasterMutex) {
        // No mutex means there aren't any open handles
        goto exit;
    }

    AcquireControllerLock();
    __try {

        // Start with success and overwrite with each error, so that the return
        // value is the most recent error
        hResult = ERROR_SUCCESS;
        for (i = 0; i < dwNumItems; i++) {
            HRESULT hrTemp;

            // Detect session changes and release pages when they happen
            if (hSession != ITEM_SESSION_HANDLE(lprgItemList[i].hTrackedItem)) {
                if (pSession) {
                    ReleaseItemPages(pSession);
                    ReleaseLoggerLockControl(&pSession->lock);  // Resume logging for this session
                }

                hSession = ITEM_SESSION_HANDLE(lprgItemList[i].hTrackedItem);
                pSession = LookupSessionHandle(hSession);
                if (pSession) {
                    AcquireLoggerLockControl(&pSession->lock);  // Block loggers during deregistration
                }
            }

            if (pSession) {
                hrTemp = DeleteItem(pSession, lprgItemList[i].hTrackedItem, FALSE);
            } else {
                hrTemp = CEPERF_HR_INVALID_HANDLE;
            }

            if (hrTemp != ERROR_SUCCESS) {
                DEBUGMSG(ZONE_ITEM, (TEXT("Error %u on bulk item %u\r\n"), hrTemp, i));
                hResult = hrTemp;
            }
        }

        if (pSession) {
            ReleaseItemPages(pSession);
            ReleaseLoggerLockControl(&pSession->lock);  // Resume logging for this session

            // Free RAM from the lists that span all sessions
            FreeUnusedDurationPages();
            FreeUnusedStatisticPages();
        }

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Safety catch so that we release the lock no matter what
        DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Exception in CePerfDeregisterBulk!\r\n")));
        hResult = CEPERF_HR_EXCEPTION;
    }
    ReleaseControllerLock();

exit:
    DEBUGMSG(ZONE_API && ZONE_ITEM, (TEXT("-CePerfDeregisterBulk (0x%08x)\r\n"), hResult));

    return hResult;
}

