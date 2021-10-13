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
#include "storeincludes.hpp"
#include "fsdcache.hpp"

NullCache* g_cacheTable = NULL;
HANDLE g_hResizingEvent;
CRITICAL_SECTION g_csAddRemove;
DWORD g_dwTotalEntries = DEFAULT_NUM_TABLE_ENTRIES;
DWORD g_dwCurEntries = 0;


NullCache* GetCache (DWORD dwCacheId)
{
    WaitForSingleObject (g_hResizingEvent, INFINITE);
    
    if (dwCacheId >= g_dwTotalEntries || !g_cacheTable[dwCacheId].hDsk) {
        DEBUGMSG(ZONE_ERRORS, (TEXT("FSDMGR!GetCache: Invalid Cache ID: %d.\n"), dwCacheId));        
        return NULL;
    }

    return &g_cacheTable[dwCacheId];
}


DWORD CreateCache( HDSK hDsk, DWORD /* dwStart */, DWORD /* dwEnd */, DWORD /* dwCacheSize */, DWORD dwBlockSize, DWORD /* dwCreateFlags */)
{
    DWORD dwCacheId = INVALID_CACHE_ID;
    BOOL fError = TRUE;

    EnterCriticalSection (&g_csAddRemove);
    
    if (!g_cacheTable) {       
        g_cacheTable = (NullCache*)LocalAlloc (LMEM_ZEROINIT, g_dwTotalEntries * sizeof(NullCache));
        if (!g_cacheTable)
            goto error;
    }    

    // If we have filled the table, double its size       

    if (g_dwCurEntries == g_dwTotalEntries) {
        ResetEvent (g_hResizingEvent);
        NullCache* newTable = (NullCache*)LocalReAlloc (g_cacheTable, 2 * g_dwTotalEntries * sizeof(NullCache), LMEM_ZEROINIT | LMEM_MOVEABLE);
        if (!newTable) {
            DEBUGMSG(ZONE_ERRORS, (TEXT("FSDMGR!CreateCache: Could not create any more caches.  Table full. \n")));   
            SetEvent (g_hResizingEvent);
            goto error;
        }
        g_cacheTable = newTable;
        g_dwTotalEntries *= 2;
        SetEvent (g_hResizingEvent);
    }

    // Find the first empty cache entry
    
    for (dwCacheId = 0; dwCacheId < g_dwTotalEntries; dwCacheId++) {
        if (!g_cacheTable[dwCacheId].hDsk) {
            g_cacheTable[dwCacheId].hDsk = hDsk;
            g_cacheTable[dwCacheId].dwBlockSize = dwBlockSize;
            break;
        }
    }

    // Should never hit this, otherwise there is an inconsistency.
    DEBUGCHK (dwCacheId < g_dwTotalEntries);
   
    g_dwCurEntries++;
    fError = FALSE;
    
error:
    LeaveCriticalSection (&g_csAddRemove);
    return fError ? INVALID_CACHE_ID : dwCacheId;

}

 
DWORD DeleteCache( DWORD dwCacheId)
{
    NullCache* pCache = GetCache(dwCacheId);
    if (!pCache)
        return ERROR_INVALID_PARAMETER;

    EnterCriticalSection (&g_csAddRemove);
    
    g_cacheTable[dwCacheId].hDsk = 0;
    g_dwCurEntries--;
    
    LeaveCriticalSection (&g_csAddRemove);
    
    return ERROR_SUCCESS;
}

 
DWORD ResizeCache( DWORD /* dwCacheId */, DWORD /* dwSize */, DWORD /* dwResizeFlags */)
{
    return ERROR_SUCCESS;
}


DWORD CachedRead(DWORD dwCacheId, DWORD dwBlockNum, DWORD dwNumBlocks, PVOID pBuffer, DWORD /* dwReadFlags */)
{
    NullCache* pCache = GetCache(dwCacheId);
    if (!pCache)
        return ERROR_INVALID_PARAMETER;
    
    return FSDMGR_ReadDisk (pCache->hDsk, dwBlockNum, dwNumBlocks, (PBYTE)pBuffer, dwNumBlocks * pCache->dwBlockSize);
}


DWORD CachedWrite(DWORD dwCacheId, DWORD dwBlockNum, DWORD dwNumBlocks, PVOID pBuffer, DWORD /* dwWriteFlags */)
{
    NullCache* pCache = GetCache(dwCacheId);
    if (!pCache)
        return ERROR_INVALID_PARAMETER;

    return FSDMGR_WriteDisk (pCache->hDsk, dwBlockNum, dwNumBlocks, (PBYTE)pBuffer, dwNumBlocks * pCache->dwBlockSize);
}

 
DWORD FlushCache(DWORD dwCacheId, PSECTOR_LIST_ENTRY /* pSectorList */, DWORD /* dwNumEntries */, DWORD /* dwFlushFlags */)
{
    NullCache* pCache = GetCache(dwCacheId);
    if (!pCache)
        return ERROR_INVALID_PARAMETER;
    
    // If flush cache is not implemented on block driver, we are done.
    if (pCache->dwFlags & FLAG_DISABLE_SEND_FLUSH_CACHE) {
        return ERROR_SUCCESS;
    }

    // Call flush cache on the disk if it is implemented
    if (!FSDMGR_DiskIoControl(pCache->hDsk, IOCTL_DISK_FLUSH_CACHE, NULL, 0, NULL, 0, NULL, NULL)) {
        pCache->dwFlags |= FLAG_DISABLE_SEND_FLUSH_CACHE;
    }
    
    return ERROR_SUCCESS;
}

DWORD SyncCache(DWORD /* dwCacheId */, PSECTOR_LIST_ENTRY /* pSectorList */, DWORD /* dwNumEntries */, DWORD /* dwSyncFlags */)
{
    return ERROR_SUCCESS;
}

DWORD InvalidateCache(DWORD /* dwCacheId */, PSECTOR_LIST_ENTRY /* pSectorList */, DWORD /* dwNumEntries */, DWORD /* dwFlags */)
{
    return ERROR_SUCCESS;
}
 
BOOL CacheIoControl(DWORD dwCacheId, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
    NullCache* pCache = GetCache(dwCacheId);
    if (!pCache)
        return ERROR_INVALID_PARAMETER;
    
    switch (dwIoControlCode) {

        case IOCTL_DISK_DELETE_SECTORS:
            if (pCache->dwFlags & FLAG_DISABLE_SEND_DELETE)
                return TRUE;
            if (!FSDMGR_DiskIoControl(pCache->hDsk, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped))
                pCache->dwFlags |= FLAG_DISABLE_SEND_DELETE;
            return TRUE;
            
        default:
            return FSDMGR_DiskIoControl(pCache->hDsk, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped);
    }   
}

VOID InitNullCache()
{
    InitializeCriticalSection (&g_csAddRemove);
    g_hResizingEvent = CreateEvent (NULL, TRUE, TRUE, NULL);        
}    

VOID DeInitNullCache()
{
    DeleteCriticalSection (&g_csAddRemove);
    CloseHandle (g_hResizingEvent);
    LocalFree (g_cacheTable);
}

