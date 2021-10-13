//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "cache.h"

CCache** g_cacheTable = NULL;
HANDLE g_hResizingEvent;
CRITICAL_SECTION g_csAddRemove;
DWORD g_dwTotalEntries = DEFAULT_NUM_TABLE_ENTRIES;
DWORD g_dwCurEntries = 0;

#ifdef DEBUG
DBGPARAM dpCurSettings = {
    TEXT("DiskCache"),
    {
        TEXT("Init"),
        TEXT("APIs"),
        TEXT("Error"),
        TEXT("Disk I/O"),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT("")
    },
    ZONEMASK_DEFAULT
};
#endif

CCache* GetCache (DWORD dwCacheId)
{
    WaitForSingleObject (g_hResizingEvent, INFINITE);
    
    if (dwCacheId >= g_dwTotalEntries || !g_cacheTable[dwCacheId]) {
        DEBUGMSG(ZONE_ERROR, (TEXT("GetCache: Invalid Cache ID: %d.\n"), dwCacheId));        
        return NULL;
    }

    return g_cacheTable[dwCacheId];
}

/*  CreateCache
 *
 *  Creates a new cache.
 *
 *  ENTRY
 *      hDsk - An FSDMGR disk handle.
 *      dwStart - Starting disk block to cache
 *      dwEnd - Ending disk block to cache
 *      dwCacheSize - Size of the cache, in blocks
 *      dwBlockSize - Size of each block, in bytes.  This must be equal to the sector size.
 *      dwPriority - The priority (256) for the lazy-writer thread in a write-back
 *          cache.  Not used for write-through cache
 *      dwCreateFlags - Can be one or more of the following:
 *          CACHE_FLAG_WARM - Preload the cache on init.
 *          CACHE_FLAG_WRITEBACK - Allow for write-back cache.
 *
 *  EXIT
 *      Cache ID that can be used on subsequent cache operations.  INVALID_CACHE_ID on error.
 */

DWORD CreateCache( HDSK hDsk, DWORD dwStart, DWORD dwEnd, DWORD dwCacheSize, DWORD dwBlockSize, DWORD dwCreateFlags)
{
    DWORD dwCacheId;
    BOOL fError = TRUE;
    DWORD dwMappingSize;

    EnterCriticalSection (&g_csAddRemove);
    
    if (!g_cacheTable) {       
        g_cacheTable = (CCache**)LocalAlloc (LMEM_ZEROINIT, g_dwTotalEntries * sizeof(CCache*));
        if (!g_cacheTable)
            goto error;
    }    

    // If we have filled the table, double its size       

    if (g_dwCurEntries == g_dwTotalEntries) {
        ResetEvent (g_hResizingEvent);
        CCache** newTable = (CCache**)LocalReAlloc (g_cacheTable, 2 * g_dwTotalEntries * sizeof(CCache*), LMEM_ZEROINIT | LMEM_MOVEABLE);
        if (!newTable) {
            DEBUGMSG(ZONE_ERROR, (TEXT("CreateCache: Could not create any more caches.  Table full. \n")));   
            SetEvent (g_hResizingEvent);
            goto error;
        }
        g_cacheTable = newTable;
        g_dwTotalEntries *= 2;
        SetEvent (g_hResizingEvent);
    }

    // Make sure block size is a power of 2 and less than the buffer size
    
    if ((dwBlockSize & (dwBlockSize - 1)) || (dwBlockSize > BUFFER_SIZE)){
        DEBUGMSG(ZONE_ERROR, (TEXT("CreateCache: Invalid block size (%d)\n"), dwBlockSize));        
        goto error;
    }

    // The size of the cache should not be greater than the number of sectors it
    // maps to on disk

    dwMappingSize = dwEnd - dwStart + 1;
    if (dwMappingSize < dwCacheSize) {
        dwCacheSize = dwMappingSize;
    }

    // Find the first empty cache entry
    
    for (dwCacheId = 0; dwCacheId < g_dwTotalEntries; dwCacheId++) {
        if (!g_cacheTable[dwCacheId]) {
            g_cacheTable[dwCacheId] = new CCache (hDsk, dwStart, dwEnd, dwCacheSize, dwBlockSize, dwCreateFlags);
            break;
        }
    }

    // Should never hit this, otherwise there is an inconsistency.
    ASSERT (dwCacheId < g_dwTotalEntries);

    // We allow a cache size of 0, meaning we always read/write directly to disk.  Create buffers only if 
    // cache size is non-zero.
    
    if (dwCacheSize) {
        CCache* pCache = g_cacheTable[dwCacheId];

        // Create cache buffer pool and lookup table
        if (pCache && !pCache->InitCache()) {
            delete pCache;
            g_cacheTable[dwCacheId] = NULL;
            goto error;
        }
    }
    
    g_dwCurEntries++;
    fError = FALSE;
    
error:
    LeaveCriticalSection (&g_csAddRemove);
    return fError ? INVALID_CACHE_ID : dwCacheId;

}

/*  DeleteCache
 *
 *  Deletes a cache.  This will free all memory allocated.
 *
 *  ENTRY
 *       dwCacheId - The cache ID returned by CreateCache
 *
 *  EXIT
 *      ERROR_SUCCESS on success.  ERROR_INVALID_PARAMETER on an invalid cache ID.
 */
 
DWORD DeleteCache( DWORD dwCacheId)
{
    CCache* pCache = GetCache(dwCacheId);
    if (!pCache)
        return ERROR_INVALID_PARAMETER;

    pCache->FlushCache (NULL, 0);

    EnterCriticalSection (&g_csAddRemove);
    
    delete g_cacheTable[dwCacheId];
    g_cacheTable[dwCacheId] = 0;
    g_dwCurEntries--;
    
    LeaveCriticalSection (&g_csAddRemove);
    
    return ERROR_SUCCESS;
}

/*  ResizesCache
 *
 *  Resizes a cache.  The cache can either be reduced or expanded in size.  If the request is to expand 
 *  the cache, and there is not enough memory, the cache remains its original size and is not modified.
 *  The cache will be first flushed before resizing, and all cached entries will be lost.
 *
 *  ENTRY
 *      dwCacheId - The cache ID returned by CreateCache
 *      dwSize - The new number of blocks of the cache.
 *      dwResizeFlags - Can be one or more of the following:
 *          CACHE_FLAG_WARM - Preload the cache on init.
 *
 *  EXIT
 *      Returns ERROR_SUCCESS on success or appropriate error code on failure.
 */
 
DWORD ResizeCache( DWORD dwCacheId, DWORD dwSize, DWORD dwResizeFlags)
{
    DEBUGMSG(ZONE_APIS, (TEXT("ResizeCache: Resizing Cache %d.\n"), dwCacheId));        
    
    CCache* pCache = GetCache(dwCacheId);
    if (!pCache)
        return ERROR_INVALID_PARAMETER;

    return pCache->ResizeCache(dwResizeFlags, dwSize);
}

/*  CachedRead
 *
 *  If desired block is cached, then it copies data from the cache into the caller's buffer.  
 *  Otherwise, it reads the data from disk and copies it into the caller's buffer as well 
 *  as the cache. 
 *
 *  ENTRY
 *      dwCacheId - The cache ID returned by CreateCache
 *      dwBlockNum - Starting block to read from
 *      dwNumBlocks - Number of blocks to read
 *      pBuffer - Pointer to buffer to read data into
 *      dwReadFlags - Not currently used.
 *
 *  EXIT
 *      Returns ERROR_SUCCESS on success or appropriate error code on failure.
 */
 
DWORD CachedRead(DWORD dwCacheId, DWORD dwBlockNum, DWORD dwNumBlocks, PVOID pBuffer, DWORD dwReadFlags)
{
    CCache* pCache = GetCache(dwCacheId);
    if (!pCache)
        return ERROR_INVALID_PARAMETER;

    return pCache->ReadCache (dwReadFlags, dwBlockNum, dwNumBlocks, pBuffer);
}

/*  CachedWrite
 *
 *  In the case of write-through, it writes the data both to the disk and the cache.  
 *  In the case of write-back, it writes the data to the cache only and allows the 
 *  lazy-writer to eventually commit the data to the disk.   If the cache location 
 *  that the data is to be written to overwrites a dirty block that is different from 
 *  the block to be written, then that dirty block is committed to disk. 
 *
 *  ENTRY
 *      dwCacheId - Cache ID to read.
 *      dwBlockNum - Starting block number to read from
 *      dwNumBlocks - Number of sectors to read
 *      pBuffer - Buffer to read contents into
 *      dwWriteFlags - Can be a combination of the flags below.
 *          CACHE_FORCE_WRITETHROUGH - Only applies for a write-back cache.  
 *          This will force the written blocks to be committed to disk.
 *
 *
 *  EXIT
 *      Returns ERROR_SUCCESS on success or appropriate error code on failure.
 */

DWORD CachedWrite(DWORD dwCacheId, DWORD dwBlockNum, DWORD dwNumBlocks, PVOID pBuffer, DWORD dwWriteFlags)
{
    CCache* pCache = GetCache(dwCacheId);
    if (!pCache)
        return ERROR_INVALID_PARAMETER;

    return pCache->WriteCache (dwWriteFlags, dwBlockNum, dwNumBlocks, pBuffer);
}

/*  FlushCache
 *
 *  Commits any dirty blocks in the cache to disk.  Only needed for write-back cache.
 *
 *  ENTRY
 *      dwCacheId - The cache ID returned by CreateCache
 *      dwFlushFlags - Not currently used.
 *
 *  EXIT
 *      Returns ERROR_SUCCESS on success or appropriate error code on failure.
 */
 
DWORD FlushCache(DWORD dwCacheId, PSECTOR_LIST_ENTRY pSectorList, DWORD dwNumEntries, DWORD dwFlushFlags)
{
    DEBUGMSG(ZONE_APIS, (TEXT("FlushCache: Flushing Cache %d.\n"), dwCacheId));        
    
    CCache* pCache = GetCache(dwCacheId);
    if (!pCache)
        return ERROR_INVALID_PARAMETER;

    return pCache->FlushCache (pSectorList, dwNumEntries);
}

DWORD SyncCache(DWORD dwCacheId, PSECTOR_LIST_ENTRY pSectorList, DWORD dwNumEntries, DWORD dwSyncFlags)
{
    return ERROR_SUCCESS;
}

DWORD InvalidateCache(DWORD dwCacheId, PSECTOR_LIST_ENTRY pSectorList, DWORD dwNumEntries, DWORD dwFlags)
{
    DEBUGMSG(ZONE_APIS, (TEXT("InvalidateCache: Cache %d.\n"), dwCacheId));        
    
    CCache* pCache = GetCache(dwCacheId);
    if (!pCache)
        return ERROR_INVALID_PARAMETER;

    return pCache->InvalidateCache(pSectorList, dwNumEntries, dwFlags);
}

/*  CacheIoControl
 *
 *  Used to extend the Cache APIs by allowing user defined cache IOCTLs.
 *  Any unhandled IOCTLs are passed down to the block driver.
 *
 *  ENTRY
 *      dwCacheId - The cache ID returned by CreateCache
 *      Rest - Standard DeviceIoControl parameters
 *
 *  EXIT
 *      Returns TRUE on success.  FALSE on failure.
 */
 
BOOL CacheIoControl(DWORD dwCacheId, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
    CCache* pCache = GetCache(dwCacheId);
    if (!pCache)
        return ERROR_INVALID_PARAMETER;

    return pCache->CacheIoControl(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped);
}

BOOL WINAPI DllMain(HANDLE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls( (HMODULE)hInstance);
        DEBUGREGISTER((HINSTANCE)hInstance);
        InitializeCriticalSection (&g_csAddRemove);
        g_hResizingEvent = CreateEvent (NULL, TRUE, TRUE, NULL);
        break;
        
    case DLL_PROCESS_DETACH:
        DeleteCriticalSection (&g_csAddRemove);
        CloseHandle (g_hResizingEvent);
        LocalFree (g_cacheTable);
        break;
    }
    return TRUE;
}
