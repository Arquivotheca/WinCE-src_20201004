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
#include "fsdcache.hpp"


#ifdef x86
// x86, no prefetch as we're running on Virtualized Hardware and prefetch could actually slow things down
#define DEFAULT_PREFETCH_SIZE       0x1000          // 4k default prefetch size.
#else
#define DEFAULT_PREFETCH_SIZE       0x4000          // 16k default prefetch size.
#endif

//
// If we need to support different prefetch size, we'll need to expose 'CreateCacheEx' to take a prefetch size
// as argument. We'll use 16k for all caches for now.
//
#ifdef UNDER_CE    

CCache** g_cacheTable = NULL;
CRITICAL_SECTION g_csCacheTable;
DWORD g_dwTotalEntries = 0;
DWORD g_dwCurEntries = 0;

NKCacheExports g_NKCacheFuncs;

#endif

CCache* GetCache (DWORD dwCacheId)
{
    CCache* pCache = NULL;
#ifdef UNDER_CE    
    EnterCriticalSection (&g_csCacheTable);
    
    if (dwCacheId < g_dwTotalEntries) {
        pCache = g_cacheTable[dwCacheId];
    }
    
    LeaveCriticalSection (&g_csCacheTable);
    
    DEBUGMSG(!pCache && ZONE_ERRORS, (TEXT("GetCache: Invalid Cache ID: %d.\n"), dwCacheId));        
#endif
    return pCache;
}

#ifdef UNDER_CE    
// valid block size: 256, 512, 1024, 2048, 4096
DWORD GetBlockShift (DWORD dwBlockSize)
{
    DWORD dwBlockShift = 0;
    switch (dwBlockSize) {
    case 256:
        dwBlockShift = 8;
        break;
    case 512:
        dwBlockShift = 9;
        break;
    case 1024:
        dwBlockShift = 10;
        break;
    case 2048:
        dwBlockShift = 11;
        break;
    case 4096:
        dwBlockShift = 12;
        break;
    }
    // block size cannot exceed page size
    DEBUGCHK (dwBlockShift <= VM_PAGE_SHIFT);
    return dwBlockShift;
}
#endif

DWORD AllocateCacheId (CCache *pCache)
{
    DWORD dwCacheId = INVALID_CACHE_ID;

#ifdef UNDER_CE    

    EnterCriticalSection (&g_csCacheTable);

    // grow table if necessary
    if (g_dwCurEntries == g_dwTotalEntries) {
        CCache **pTable;
        DWORD  cEntries;
        if (g_dwTotalEntries) {
            cEntries = g_dwTotalEntries * 2;
            pTable   = (CCache**) LocalReAlloc (g_cacheTable, cEntries * sizeof(CCache*), LMEM_ZEROINIT | LMEM_MOVEABLE);
        } else {
            cEntries = DEFAULT_NUM_TABLE_ENTRIES;
            pTable   = (CCache**) LocalAlloc (LMEM_ZEROINIT, cEntries * sizeof(CCache*));
        }
        if (pTable) {
            g_cacheTable     = pTable;
            g_dwTotalEntries = cEntries;
        }
    }

    if (g_dwCurEntries < g_dwTotalEntries) {
        // find an unused entry in the cache table and return the index
        for (dwCacheId = 0; dwCacheId < g_dwTotalEntries; dwCacheId++) {
            if (!g_cacheTable[dwCacheId]) {
                g_cacheTable[dwCacheId] = pCache;
                ++ g_dwCurEntries;
                break;
            }
        }
        DEBUGCHK (INVALID_CACHE_ID != dwCacheId);
    }
    
    LeaveCriticalSection (&g_csCacheTable);
#endif
    return dwCacheId;
}

#ifdef UNDER_CE
FORCEINLINE DWORD AdjustWriteFlags (const CCache *pCache, DWORD dwWriteFlags)
{
    if (!dwWriteFlags) {
        dwWriteFlags = pCache->IsWriteBack ()
                     ? CACHE_FORCE_WRITEBACK
                     : CACHE_FORCE_WRITETHROUGH;
    }
    return dwWriteFlags;
}
#endif

/*  CreateCache
 *
 *  Creates a new cache.
 *
 *  ENTRY
 *      hDsk - An FSDMGR disk handle.
 *      dwStart - Starting disk block to cache
 *      dwEnd - Ending disk block to cache
 *      dwCacheSize - cache size - ignored.
 *      dwBlockSize - Size of each block, in bytes.  This must be equal to the sector size.
 *      dwPriority - The priority (256) for the lazy-writer thread in a write-back
 *          cache.  Not used for write-through cache
 *      dwCreateFlags - Can be one or more of the following:
 *          CACHE_FLAG_WARM - Preload the cache on init, ignored
 *          CACHE_FLAG_WRITEBACK - Allow for write-back cache.
 *
 *  EXIT
 *      Cache ID that can be used on subsequent cache operations.  INVALID_CACHE_ID on error.
 */

DWORD FSDMGR_CreateCacheEx (HDSK hDsk, DWORD dwStart, DWORD dwEnd, DWORD dwBlockSize, DWORD dwCreateFlags, DWORD dwPrefetchSize)
{
    DWORD dwCacheId = INVALID_CACHE_ID;
#ifdef UNDER_CE    
    DWORD dwBlockShift = GetBlockShift (dwBlockSize);

    if (dwBlockShift) {
        CCache *pCache = new CCache;

        if (pCache && pCache->InitCache (hDsk, dwStart, dwEnd, dwPrefetchSize, dwBlockShift, dwCreateFlags)) {
            dwCacheId = AllocateCacheId (pCache);
        }

        if ((INVALID_CACHE_ID == dwCacheId) && pCache) {
            delete pCache;
        }
        
    } else {
        // this is an internal function and caller should've passed in the right blocksize
        DEBUGCHK (0);
    }
#endif
    return dwCacheId;
}

DWORD FSDMGR_CreateCache( HDSK hDsk, DWORD dwStart, DWORD dwEnd, DWORD dwCacheSize, DWORD dwBlockSize, DWORD dwCreateFlags)
{
    return FSDMGR_CreateCacheEx (hDsk, dwStart, dwEnd, dwBlockSize, dwCreateFlags, DEFAULT_PREFETCH_SIZE);
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
 
DWORD FSDMGR_DeleteCache( DWORD dwCacheId)
{
    CCache* pCache = GetCache(dwCacheId);
#ifdef UNDER_CE    
    if (pCache) {
        DEBUGCHK (pCache == g_cacheTable[dwCacheId]);
        pCache->FlushCache (NULL, 0, 0);

        EnterCriticalSection (&g_csCacheTable);
        
        g_cacheTable[dwCacheId] = 0;
        g_dwCurEntries--;
        
        LeaveCriticalSection (&g_csCacheTable);

        // delete the page tree outside of cache table lock as it needs to destory the page tree.
        delete pCache;
    }
#endif    
    return pCache? ERROR_SUCCESS : ERROR_INVALID_PARAMETER;
}

/*  ResizesCache
 *      - No-op now.
 */
 
DWORD FSDMGR_ResizeCache( DWORD dwCacheId, DWORD dwSize, DWORD dwResizeFlags)
{
    return ERROR_SUCCESS;
}

/*  CachedRead
 *
 *  Prefetch is implicit. So we'll only need to call ReadCache when pBuffer isn't NULL.
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
 
DWORD FSDMGR_CachedRead(DWORD dwCacheId, DWORD dwBlockNum, DWORD dwNumBlocks, PVOID pBuffer, DWORD dwReadFlags)
{
#ifdef UNDER_CE    
    CCache* pCache = pBuffer? GetCache(dwCacheId) : NULL;

    return pCache
         ? pCache->ReadWriteCache (0, dwBlockNum, dwNumBlocks, pBuffer)
         : ERROR_INVALID_PARAMETER;
#else
    return ERROR_INVALID_PARAMETER;
#endif
    
}

/*  CachedReadAtOffset
 *
 *  Prefetch is implicit. So we'll only need to call ReadCache when pBuffer isn't NULL.
 *
 *  ENTRY
 *      dwCacheId - The cache ID returned by CreateCache
 *      ulOfst - Starting offset to read from
 *      pBuffer - Pointer to buffer to read data into
 *      cbSize  - size of buffer to read.
 *      pcbRead - [out] number of bytes read
 *      dwReadFlags - unused
 *
 *  EXIT
 *      Returns ERROR_SUCCESS on success or appropriate error code on failure.
 */
 
DWORD FSDMGR_CachedReadAtOffset (DWORD dwCacheId, ULONGLONG ul64Ofst, PVOID pBuffer, DWORD cbSize, LPDWORD pcbRead, DWORD dwReadFlags)
{
#ifdef UNDER_CE    
    CCache* pCache = pBuffer? GetCache(dwCacheId) : NULL;

    return pCache
         ? pCache->ReadWriteCacheAtOffset (0, ul64Ofst, pBuffer, cbSize, pcbRead)
         : ERROR_INVALID_PARAMETER;
#else
    return ERROR_INVALID_PARAMETER;
#endif
    
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
 *      dwBlockNum - Starting block number to write to
 *      dwNumBlocks - Number of sectors to write
 *      pBuffer - pointing to data to write from
 *      dwWriteFlags - Can be one of the following
 *          CACHE_FORCE_WRITETHROUGH - always write-thru.  
 *          CACHE_FORCE_WRITEBACK - always write-back
 *          0 - write-back/write-through based on the cache settings.
 *
 *
 *  EXIT
 *      Returns ERROR_SUCCESS on success or appropriate error code on failure.
 */

DWORD FSDMGR_CachedWrite(DWORD dwCacheId, DWORD dwBlockNum, DWORD dwNumBlocks, PVOID pBuffer, DWORD dwWriteFlags)
{
#ifdef UNDER_CE    
    CCache* pCache = GetCache(dwCacheId);

    return pCache
         ? pCache->ReadWriteCache (AdjustWriteFlags (pCache, dwWriteFlags), dwBlockNum, dwNumBlocks, pBuffer)
         : ERROR_INVALID_PARAMETER;
    
#else
    return ERROR_INVALID_PARAMETER;
#endif
    
}

/*  CachedWriteAtOffset
 *
 *  In the case of write-through, it writes the data both to the disk and the cache.  
 *  In the case of write-back, it writes the data to the cache only and allows the 
 *  lazy-writer to eventually commit the data to the disk.   If the cache location 
 *  that the data is to be written to overwrites a dirty block that is different from 
 *  the block to be written, then that dirty block is committed to disk. 
 *
 *  EXIT
 *      Returns ERROR_SUCCESS on success or appropriate error code on failure.
 *  ENTRY
 *      dwCacheId - The cache ID returned by CreateCache
 *      ulOfst - Starting offset to write
 *      pBuffer - Pointer to data to write
 *      cbSize  - size to write.
 *      pcbWritten - [out] number of bytes written
 *      dwWriteFlags - Can be one of the following
 *          CACHE_FORCE_WRITETHROUGH - always write-thru.  
 *          CACHE_FORCE_WRITEBACK - always write-back
 *          0 - write-back/write-through based on the cache settings.
 *
 *  EXIT
 *      Returns ERROR_SUCCESS on success or appropriate error code on failure.
 */
 
DWORD FSDMGR_CachedWriteAtOffset (DWORD dwCacheId, ULONGLONG ul64Ofst, LPCVOID pBuffer, DWORD cbSize, LPDWORD pcbWritten, DWORD dwWriteFlags)
{
#ifdef UNDER_CE    
    CCache* pCache = GetCache(dwCacheId);

    return pCache
         ? pCache->ReadWriteCacheAtOffset (AdjustWriteFlags (pCache, dwWriteFlags), ul64Ofst, (PVOID) pBuffer, cbSize, pcbWritten)
         : ERROR_INVALID_PARAMETER;
#else
    return ERROR_INVALID_PARAMETER;
#endif
    
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
 
DWORD  FSDMGR_FlushCache(DWORD dwCacheId, PSECTOR_LIST_ENTRY pSectorList, DWORD dwNumEntries, DWORD dwFlushFlags)
{
#ifdef UNDER_CE    
    DEBUGMSG(ZONE_APIS, (TEXT("FlushCache: Flushing Cache %d.\n"), dwCacheId));        
    
    CCache* pCache = GetCache(dwCacheId);

    return pCache
         ? pCache->FlushCache (pSectorList, dwNumEntries, dwFlushFlags)
         : ERROR_INVALID_PARAMETER;
#else
    return ERROR_INVALID_PARAMETER;
#endif
    
}

DWORD FSDMGR_SyncCache(DWORD dwCacheId, SECTOR_LIST_ENTRY pSectorList, DWORD dwNumEntries, DWORD dwSyncFlags)
{
    return ERROR_SUCCESS;
}

DWORD FSDMGR_InvalidateCache(DWORD dwCacheId, PSECTOR_LIST_ENTRY pSectorList, DWORD dwNumEntries, DWORD dwFlags)
{
#ifdef UNDER_CE    
    DEBUGMSG(ZONE_APIS, (TEXT("InvalidateCache: Cache %d.\n"), dwCacheId));        
    
    CCache* pCache = GetCache(dwCacheId);

    return pCache
         ? pCache->InvalidateCache(pSectorList, dwNumEntries, dwFlags)
         : ERROR_INVALID_PARAMETER;
#else
    return ERROR_INVALID_PARAMETER;
#endif
    
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
 
BOOL FSDMGR_CacheIoControl(DWORD dwCacheId, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
#ifdef UNDER_CE    
    CCache* pCache = GetCache(dwCacheId);

    return pCache
         ? pCache->CacheIoControl(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped)
         : ERROR_INVALID_PARAMETER;
#else
    return ERROR_INVALID_PARAMETER;
#endif
    
}

LRESULT InitFsdCache (void)
{
#ifdef UNDER_CE    
    InitializeCriticalSection (&g_csCacheTable);
    // Get the kernel exported cache functions. 
    g_NKCacheFuncs.Version = KERNEL_CACHE_VERSION;

    VERIFY (KernelLibIoControl ((HANDLE) KMOD_CORE, IOCTL_KLIB_CACHE_REGISTER,
                             NULL, 0,
                             &g_NKCacheFuncs, sizeof(NKCacheExports), NULL));
#endif
    return ERROR_SUCCESS;
}
