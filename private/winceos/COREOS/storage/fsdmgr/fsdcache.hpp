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
#include <fsdmgr.h>
#ifdef UNDER_CE
#include <tchar.h>
#include <diskio.h>
#include <celog.h>
#include "fsddbg.h"


#define DEFAULT_NUM_TABLE_ENTRIES 10

// Flags for m_dwInteralFlags, i.e. specific to this implementation of the cache
#define FLAG_DISABLE_SEND_DELETE      0x1
#define FLAG_DISABLE_SEND_FLUSH_CACHE 0x2
#define FLAG_START_READ               0x8

#define CACHE_THREAD_PRIORITY (THREAD_PRIORITY_IDLE + 248)

class CCache {
private:    
    ULARGE_INTEGER m_liTreeSize;
    HDSK    m_hDsk;
    DWORD   m_dwPageTreeId;
    DWORD   m_dwStart;
    DWORD   m_dwBlockShift;
    DWORD   m_dwCreateFlags;
    DWORD   m_dwInternalFlags;
    HANDLE  m_hDirtySectorsEvent;
    HTHREAD m_hLazyWriterThread;

protected:
    DWORD FlushSegment (const SECTOR_LIST_ENTRY *pSectorList);
    DWORD InvalidateSegment (const SECTOR_LIST_ENTRY *pSectorList);
    DWORD DeleteCachedSectors (PDELETE_SECTOR_INFO pInfo);
    DWORD ReadWritePageTree (DWORD dwFlags, ULONGLONG ul64Ofst, LPBYTE pBuffer, DWORD cbSize, LPDWORD pcbAccessed);

public:
    CCache ();
    ~CCache();
    BOOL InitCache(HDSK hDsk, DWORD dwStart, DWORD dwEnd, DWORD cbPrefetch, DWORD dwBlockShift, DWORD dwCreateFlags);
    DWORD ReadWriteCache (DWORD dwFlags, DWORD dwSectorNum, DWORD dwNumSectors, PVOID pBuffer);
    DWORD ReadWriteCacheAtOffset (DWORD dwFlags, ULONGLONG ul64Ofst, PVOID pBuffer, DWORD cbSize, LPDWORD pcbAccessed);
    BOOL UncachedReadWrite (BOOL fRead, const ULARGE_INTEGER *pliOffset, LPVOID pData, DWORD cbSize, LPDWORD pcbAccessed);
    DWORD FlushCache (const SECTOR_LIST_ENTRY *pSectorList, DWORD dwNumEntries, DWORD dwFlushFlags);
    DWORD InvalidateCache (const SECTOR_LIST_ENTRY *pSectorList, DWORD dwNumEntries, DWORD dwFlags);
    BOOL CacheIoControl(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);
    VOID LazyWriterThread ();

    inline DWORD SizeToSector (ULONGLONG ul64Size) const
    {
        return (DWORD) (ul64Size >> m_dwBlockShift);
    }
    inline ULONGLONG SectorToSize (DWORD dwSector) const
    {
        return (ULONGLONG) dwSector << m_dwBlockShift;
    }
    inline BOOL IsCacheBlockAligned (DWORD dwOffset) const
    {
        return (0 == (dwOffset & ((1 << m_dwBlockShift) - 1)));
    }
    inline BOOL IsWriteBack () const 
    { 
        return (m_dwCreateFlags & CACHE_FLAG_WRITEBACK);
    }

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

    inline VOID CelogStartRead()
    {
        m_dwInternalFlags |= FLAG_START_READ;
    }

    inline VOID CelogEndRead()
    {
        m_dwInternalFlags &= ~FLAG_START_READ;
    }
};

extern NKCacheExports g_NKCacheFuncs;

#else

class CCache;

#endif  // UNDER_CE

#endif  // #ifndef CACHE_H

