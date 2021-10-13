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
#ifndef CACHE_H
#define CACHE_H

#include <windows.h>
#include <tchar.h>
#include <diskio.h>
#include <fsdmgr.h>

#ifdef DEBUG

/* debug zones */
#define ZONEID_INIT  0
#define ZONEID_APIS  1
#define ZONEID_ERROR 2
#define ZONEID_IO 3

/* zone masks */
#define ZONEMASK_INIT   (1 << ZONEID_INIT)
#define ZONEMASK_APIS   (1 << ZONEID_APIS)
#define ZONEMASK_ERROR  (1 << ZONEID_ERROR)
#define ZONEMASK_IO (1 << ZONEID_IO)
#define ZONEMASK_DEFAULT        (ZONEMASK_INIT)

/* these are used as the first arg to DEBUGMSG */
#define ZONE_INIT   DEBUGZONE(ZONEID_INIT)
#define ZONE_APIS   DEBUGZONE(ZONEID_APIS)
#define ZONE_ERROR  DEBUGZONE(ZONEID_ERROR)
#define ZONE_IO DEBUGZONE(ZONEID_IO)

#define CACHE_MEASURE_PERF

#endif /* DEBUG_H */


#define BUFFER_SIZE (64 * 1024)

#define DEFAULT_NUM_TABLE_ENTRIES 10

// Flags for m_pStatus in CCache
#define STATUS_DIRTY    0x1

// Used by the lazy-writer to determine when to terminate.
#define EVENT_DATA_INIT 0x1
#define EVENT_DATA_EXIT 0x2

// Flags for m_dwInteralFlags, i.e. specific to this implementation of the cache
#define FLAG_DISABLE_SEND_DELETE      0x1
#define FLAG_DISABLE_SEND_FLUSH_CACHE 0x2
#define FLAG_DISABLE_SCATTER_GATHER   0x4

// Flags for dwOption in CommitDirtySectors
#define COMMIT_ALL_SECTORS        0x1   
#define COMMIT_DIFF_SECTORS       0x2
#define COMMIT_SPECIFIED_SECTORS  0x3

#define CACHE_THREAD_PRIORITY (THREAD_PRIORITY_IDLE + 248)

class CCache {
private:    
    HDSK m_hDsk;
    DWORD  m_dwStart;
    DWORD m_dwEnd;
    DWORD m_dwCacheSize;
    DWORD m_dwBlockSize;
    DWORD m_dwCreateFlags;
    DWORD m_dwInternalFlags;
    DWORD m_dwDirtyCount;
    PDWORD m_pCacheLookup;
    LPBYTE m_pStatus;
    LPBYTE* m_apBufferPool;
    CRITICAL_SECTION m_cs;
    HANDLE m_hDirtySectorsEvent;
    HANDLE m_hLazyWriterThread;

protected:
    DWORD ReadFromCacheBuffer (DWORD dwCacheIndex, LPBYTE pBuffer);    
    DWORD WriteToCacheBuffer (DWORD dwCacheIndex, LPBYTE pBuffer, BOOL fDirty);
    DWORD ReadWriteDisk (DWORD dwIOCTL, DWORD dwSectorNum, DWORD dwNumSectors, PVOID pBuffer);  
    VOID WarmCache();
    DWORD CommitDirtySectors (DWORD dwStartSector, DWORD dwNumSectors, DWORD dwOption);
    DWORD CommitCacheSectors (DWORD dwStartSector, DWORD dwNumSectors);
    DWORD CommitNextDirtyRun (DWORD dwStartIndex);
    VOID SetDirtyStatus (DWORD dwIndex);
    VOID ClearDirtyStatus (DWORD dwStartIndex, DWORD dwEndIndex);
    DWORD DeleteCachedSectors (PDELETE_SECTOR_INFO pInfo);
    VOID InvalidateSector (DWORD dwSector);

    inline BOOL IsDirty(DWORD dwCacheIndex) { return (m_pStatus[dwCacheIndex] & STATUS_DIRTY); }
    inline BOOL IsCached(DWORD dwSector) { return (m_pCacheLookup[dwSector % m_dwCacheSize] == dwSector); }
    inline BOOL IsWriteBack () { return (m_dwCreateFlags & CACHE_FLAG_WRITEBACK); }
    inline BOOL IsScatterGather () { return !(m_dwInternalFlags & FLAG_DISABLE_SCATTER_GATHER); }
    inline DWORD GetIndex(DWORD dwSector) { return (dwSector % m_dwCacheSize); }

public:
    CCache (HDSK hDsk, DWORD dwStart, DWORD dwEnd, DWORD dwCacheSize, DWORD dwBlockSize, DWORD dwCreateFlags);
    ~CCache();
    BOOL InitCache();
    DWORD ReadCache (DWORD dwReadFlags, DWORD dwSectorNum, DWORD dwNumSectors, PVOID pBuffer);
    DWORD WriteCache (DWORD dwWriteFlags, DWORD dwSectorNum, DWORD dwNumSectors, PVOID pBuffer);    
    DWORD ResizeCache (DWORD dwResizeFlags, DWORD dwSize);
    DWORD FlushCache (PSECTOR_LIST_ENTRY pSectorList, DWORD dwNumEntries, DWORD dwFlushFlags);
    DWORD InvalidateCache (PSECTOR_LIST_ENTRY pSectorList, DWORD dwNumEntries, DWORD dwFlags);
    BOOL CacheIoControl(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);
    VOID LazyWriterThread ();

    //
    // Performance measurements for read and write requests
    // 
#ifdef CACHE_MEASURE_PERF    
    DWORD m_dwReadRequests;
    DWORD m_dwReadCount;
    DWORD m_dwReadTime;
    DWORD m_dwWriteRequests;
    DWORD m_dwWriteCount;
    DWORD m_dwWriteTime;
    DWORD m_dwTempTime;

    inline VOID LogStartReadWrite ()
    {
        m_dwTempTime = GetTickCount();
    }
    inline VOID LogEndReadWrite (BOOL fRead, DWORD dwNumSectors)
    {
        if (fRead) {
            m_dwReadRequests++;
            m_dwReadCount += dwNumSectors;
            m_dwReadTime += (GetTickCount() - m_dwTempTime);
        } else {
            m_dwWriteRequests++;
            m_dwWriteCount += dwNumSectors;
            m_dwWriteTime += (GetTickCount() - m_dwTempTime);
        }
    }
#else
    inline VOID LogStartReadWrite ()
    {
    }
    inline VOID LogEndReadWrite (BOOL fRead, DWORD dwNumSectors)
    {
    }
#endif

};


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

DWORD WINAPI LazyWriterThread (LPVOID lpParameter);

#endif  // #ifndef CACHE_H

