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
/*
 *  Microsoft Confidential
 *  All Rights Reserved.
 *
 *  FSDCACHE.H 
 *
 *  This defines function pointer type for the cache as well as types for nullcache
 */
 
#ifndef FSDCACHE_H
#define FSDCACHE_H

#define DEFAULT_NUM_TABLE_ENTRIES 10

typedef struct tagNullCache
{
    HDSK hDsk;
    DWORD dwBlockSize;
    DWORD dwFlags;
} NullCache, *PNullCache;

// Flags for dwFlags, i.e. specific to this implementation of the cache
#define FLAG_DISABLE_SEND_DELETE 0x1
#define FLAG_DISABLE_SEND_FLUSH_CACHE 0x2


extern "C" {
DWORD CreateCache( HDSK hDsk, DWORD dwStart, DWORD dwEnd, DWORD dwCacheSize, DWORD dwBlockSize, DWORD dwCreateFlags);
DWORD DeleteCache( DWORD dwCacheId);
DWORD ResizeCache( DWORD dwCacheId, DWORD dwSize, DWORD dwResizeFlags);
DWORD CachedRead(DWORD dwCacheId, DWORD dwBlockNum, DWORD dwNumBlocks, PVOID pBuffer, DWORD dwReadFlags);
DWORD CachedWrite(DWORD dwCacheId, DWORD dwBlockNum, DWORD dwNumBlocks, PVOID pBuffer, DWORD dwWriteFlags);
DWORD FlushCache(DWORD dwCacheId, PSECTOR_LIST_ENTRY pSectorList, DWORD dwNumEntries, DWORD dwFlushFlags);
DWORD SyncCache(DWORD dwCacheId, PSECTOR_LIST_ENTRY pSectorList, DWORD dwNumEntries, DWORD dwSyncFlags);
DWORD InvalidateCache (DWORD dwCacheId, PSECTOR_LIST_ENTRY pSectorList, DWORD dwNumEntries, DWORD dwFlags);
BOOL CacheIoControl(DWORD dwCacheId, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);
}

typedef DWORD (*PFN_CREATECACHE)( HDSK hDsk, DWORD dwStart, DWORD dwEnd, DWORD dwCacheSize, DWORD dwBlockSize, DWORD dwCreateFlags);
typedef DWORD (*PFN_DELETECACHE)( DWORD dwCacheId);
typedef DWORD (*PFN_RESIZECACHE)( DWORD dwCacheId, DWORD dwSize, DWORD dwResizeFlags);
typedef DWORD (*PFN_CACHEDREAD)(DWORD dwCacheId, DWORD dwSectorNum, DWORD dwNumSectors, PVOID pBuffer, DWORD dwReadFlags);
typedef DWORD (*PFN_CACHEDWRITE)(DWORD dwCacheId, DWORD dwSectorNum, DWORD dwNumSectors, PVOID pBuffer, DWORD dwWriteFlags);
typedef DWORD (*PFN_FLUSHCACHE)(DWORD dwCacheId, PSECTOR_LIST_ENTRY pSectorList, DWORD dwNumEntries, DWORD dwFlushFlags);
typedef DWORD (*PFN_SYNCCACHE)(DWORD dwCacheId, PSECTOR_LIST_ENTRY pSectorList, DWORD dwNumEntries, DWORD dwSyncFlags);
typedef DWORD (*PFN_INVALIDATECACHE)(DWORD dwCacheId, PSECTOR_LIST_ENTRY pSectorList, DWORD dwNumEntries, DWORD dwFlags);
typedef BOOL (*PFN_CACHEIOCONTROL)(DWORD dwCacheId, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);

// Null cache specific functions
VOID InitNullCache();
VOID DeInitNullCache();


#endif //FSDCACHE_H
