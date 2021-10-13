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

#ifdef UNDER_CE

#define SIZE_1G                     0x40000000

// Write cached pages to the underlying file
extern "C" BOOL
DISKCACHE_UncachedWrite (
    DWORD CacheId, 
    const ULARGE_INTEGER *pliOffset, 
    const BYTE *pData, 
    DWORD cbSize,
    LPDWORD pcbWritten
    )
{
    CCache *pCache = (CCache *) CacheId;
    DEBUGCHK (pCache);
    return pCache->UncachedReadWrite (FALSE, pliOffset, (LPVOID)pData, cbSize, pcbWritten);
}

extern "C" BOOL
DISKCACHE_UncachedRead (
    DWORD CacheId, 
    const ULARGE_INTEGER *pliOffset, 
    BYTE *pData, 
    DWORD cbSize,
    LPDWORD pcbRead
    )
{
    CCache *pCache = (CCache *) CacheId;
    DEBUGCHK (pCache);
    return pCache->UncachedReadWrite (TRUE, pliOffset, pData, cbSize, pcbRead);
}

extern "C" BOOL
DISKCACHE_UncachedSetFileSize (
    DWORD CacheId, 
    const ULARGE_INTEGER *pliSize
    )
{
    // should've never been called
    DEBUGCHK (0);
    return FALSE;
}

static DWORD WINAPI LazyWriterThread (LPVOID lpParameter)
{
    CCache* pCache = (CCache*)lpParameter;
    pCache->LazyWriterThread();
    
    return 0;
}


const FSCacheExports g_DiskCacheFuncs = {
    KERNEL_CACHE_VERSION,
    DISKCACHE_UncachedWrite,
    DISKCACHE_UncachedRead,
    DISKCACHE_UncachedSetFileSize,
};

CCache::CCache () : m_dwPageTreeId (0)
{
}

CCache::~CCache()
{
    if (m_dwPageTreeId) {

        // clear page tree id so lazy write will exit as soon as it got signaled
        DWORD dwPageTreeId = m_dwPageTreeId;
        m_dwPageTreeId = 0;
        
        if (m_hLazyWriterThread) {
            HANDLE hEvt = m_hDirtySectorsEvent;
            m_hDirtySectorsEvent = NULL;    // this guarantee that lazy writer will not get into the wait from if
                                            // it's not currently waiting blocked.
            
            // Signal lazy-writer thread. It'll exit right away if it's currently blocked.
            SetEvent (hEvt);

            // close the event
            CloseHandle (hEvt);

            if (m_hLazyWriterThread) {
                // Wait for the lazy-writer thread to terminate within 20 seconds.       
                WaitForSingleObject (m_hLazyWriterThread, 20000);
                CloseHandle (m_hLazyWriterThread);
            }
            
        }
        g_NKCacheFuncs.pPageTreeClose (dwPageTreeId);
    }
}

BOOL CCache::UncachedReadWrite (
    BOOL fRead,
    const ULARGE_INTEGER * pliOffset, 
    LPVOID pData, 
    DWORD cbSize, 
    LPDWORD pcbAccessed)
{
    // offset and size must be multiple of block size
    DEBUGCHK (IsCacheBlockAligned (pliOffset->LowPart));
    DEBUGCHK (IsCacheBlockAligned (cbSize));
    DWORD dwError = ERROR_SUCCESS;
    
    if (pliOffset->QuadPart >= m_liTreeSize.QuadPart) {
        // accessing past end of page tree
        dwError = ERROR_INVALID_PARAMETER;
        
    } else {

        // if the request goes past the page tree, reduce the request down
        if ((pliOffset->QuadPart + cbSize) > m_liTreeSize.QuadPart) {
            cbSize = (DWORD)(m_liTreeSize.QuadPart - pliOffset->QuadPart);
        }
        
        DWORD dwSectorNum  = SizeToSector (pliOffset->QuadPart) + m_dwStart;
        DWORD dwNumSectors = SizeToSector (cbSize);
        
        LogStartReadWrite();

        dwError = fRead
                ? FSDMGR_ReadDisk (m_hDsk, dwSectorNum, dwNumSectors, (PBYTE) pData, cbSize)
                : FSDMGR_WriteDisk(m_hDsk, dwSectorNum, dwNumSectors, (PBYTE) pData, cbSize);

        LogEndReadWrite(fRead, dwNumSectors);

        DEBUGMSG(ZONE_DISKIO, (L"ReadWriteDisk: Command: %s, Start Sector: %d, Num Sectors: %d\r\n", 
                fRead ? L"Read" : L"Write", dwSectorNum, dwNumSectors));
    
        DEBUGMSG(ZONE_ERRORS && (ERROR_SUCCESS != dwError), (L"Read/Write Sector failed (%d) on Sector %d\r\n", dwError, dwSectorNum));

        if (pcbAccessed && (ERROR_SUCCESS == dwError)) {
            *pcbAccessed = cbSize;
        }
    }
    
    if (ERROR_SUCCESS != dwError) {
        SetLastError (dwError);
    }    
    return (ERROR_SUCCESS == dwError);
}

/*  CCache::DeleteCachedSectors
 *
 *  Removes the specified sectors from the cache and unmarks them dirty.
 *
 *  ENTRY
 *      pInfo - Structure containing start sector and num sectors to remove.
 *
 *  EXIT
 *      TRUE on success.  
 */


DWORD CCache::DeleteCachedSectors (PDELETE_SECTOR_INFO pInfo)
{
    DWORD dwError = ERROR_SUCCESS;

    if (pInfo->startsector < m_dwStart) {
        dwError = ERROR_INVALID_PARAMETER;
        
    } else {
    
        ULARGE_INTEGER liStart;
        ULARGE_INTEGER liEnd;

        liStart.QuadPart = SectorToSize (pInfo->startsector - m_dwStart);
        liEnd.QuadPart   = liStart.QuadPart + SectorToSize (pInfo->numsectors);

        if (liEnd.QuadPart > m_liTreeSize.QuadPart) {
            // accessing past end of page tree
            dwError = ERROR_INVALID_PARAMETER;
            
        } else {

            // page align up Start
            liStart.QuadPart += VM_PAGE_OFST_MASK;
            liStart.LowPart  = PAGEALIGN_DOWN (liStart.LowPart);

            // page align down end.
            liEnd.LowPart    = PAGEALIGN_DOWN (liEnd.LowPart);

            // call to page tree to discard all dirty pages in the range
            while (liEnd.QuadPart > liStart.QuadPart) {
                DWORD cbDiscard = ((liEnd.QuadPart - liStart.QuadPart) > SIZE_1G)
                                ? SIZE_1G
                                : (DWORD) (liEnd.QuadPart - liStart.QuadPart);
                g_NKCacheFuncs.pPageTreeInvalidate (m_dwPageTreeId, &liStart, cbDiscard, PAGE_TREE_INVALIDATE_DISCARD_DIRTY);
                liStart.QuadPart += cbDiscard;
            }
            

            // If delete sectors is not implemented on block driver, we are done.
            if (m_dwInternalFlags & FLAG_DISABLE_SEND_DELETE) {
                return dwError;
            }

            // Call delete sectors on the disk if it is implemented
            DWORD dwSavedLastError = GetLastError();
            DWORD dwDummy;

            if (!FSDMGR_DiskIoControl(m_hDsk, IOCTL_DISK_DELETE_SECTORS, pInfo, sizeof(DELETE_SECTOR_INFO), 
                NULL, 0, &dwDummy, NULL))
            {
                m_dwInternalFlags |= FLAG_DISABLE_SEND_DELETE;
            }

            SetLastError(dwSavedLastError);
        }
    }
    return dwError;
    
    
}

/*  CCache::LazyWriterThread
 *
 *  For write-back cache, this thread commits dirty sectors
 *
 *  ENTRY
 *      None.
 *
 *  EXIT
 *      None.  
 */
 
VOID CCache::LazyWriterThread ()
{
    while (WaitForSingleObject (m_hDirtySectorsEvent, INFINITE) == WAIT_OBJECT_0) {
    
        volatile DWORD dwPageTreeId = m_dwPageTreeId;   // 'volatile' to make sure compiler doesn't optimize access to dwPageTreeId away

        // If the exiting event was set or we had a failure, then exit this thread.
        if (!dwPageTreeId) {
            break;
        }

        if (g_NKCacheFuncs.pIsPageTreeDirty (dwPageTreeId)) {
            g_NKCacheFuncs.pPageTreeFlush (dwPageTreeId, NULL, 0, 0, NULL);
        }
    }
}


BOOL CCache::InitCache(HDSK hDsk, DWORD dwStart, DWORD dwEnd, DWORD cbPrefetch, DWORD dwBlockShift, DWORD dwCreateFlags)
{
    BOOL fRet = (dwEnd >= dwStart) && dwBlockShift;

    // we should've never call InitCache with invalid parameters
    DEBUGCHK (fRet);

    if (fRet) {

        m_dwStart            = dwStart;
        m_dwBlockShift       = dwBlockShift;
        m_hDsk               = hDsk;
        m_dwCreateFlags      = dwCreateFlags;
        m_dwInternalFlags    = 0;
        m_hDirtySectorsEvent = NULL;
        m_hLazyWriterThread  = NULL;
#ifdef CACHE_MEASURE_PERF    
        m_dwReadRequests     = 0;
        m_dwReadCount        = 0;
        m_dwReadTime         = 0;
        m_dwWriteRequests    = 0;
        m_dwWriteCount       = 0;
        m_dwWriteTime        = 0;
        m_dwTempTime         = 0;
#endif
        m_liTreeSize.QuadPart = SectorToSize (dwEnd - dwStart + 1);

        RETAILMSG (1, (L"InitCache: Creating page tree of size 0x%016I64x\r\n", m_liTreeSize.QuadPart));

        m_dwPageTreeId = g_NKCacheFuncs.pPageTreeCreate ((DWORD)this, &m_liTreeSize, &g_DiskCacheFuncs, cbPrefetch, PGT_DISKCACHE);

        if (!m_dwPageTreeId) {
            fRet = FALSE;
            
        } else if (IsWriteBack ()) {
            
            DWORD fDisableLazyWriter = FALSE;
            if (FSDMGR_GetRegistryValue((HDSK)m_hDsk, L"DisableLazyWriter", &fDisableLazyWriter) && fDisableLazyWriter) {
                m_dwCreateFlags |= CACHE_FLAG_DISABLE_LAZY_WRITER;
            }        

            if (!(m_dwCreateFlags & CACHE_FLAG_DISABLE_LAZY_WRITER)) {
                
                m_hDirtySectorsEvent = CreateEvent (NULL, FALSE, FALSE, NULL);

                if (!m_hDirtySectorsEvent) {

                    // Failed to create event, operate in write-through mode.
                    m_dwCreateFlags &= ~CACHE_FLAG_WRITEBACK;

                } else {

                    m_hLazyWriterThread = CreateThread(NULL, 0, ::LazyWriterThread, (LPVOID)this, 0, NULL);
                    if (!m_hLazyWriterThread) {

                        // Failed to create thread, operate in write-through mode.
                        CloseHandle (m_hDirtySectorsEvent);
                        m_hDirtySectorsEvent = NULL;
                        m_dwCreateFlags &= ~CACHE_FLAG_WRITEBACK;

                    } else {
                        DWORD dwPriority;

                        // Get the priority for the lazy-writer thread from the registry
                        if (!FSDMGR_GetRegistryValue((HDSK)m_hDsk, L"LazyWriterThreadPrio256", &dwPriority)) {
                            dwPriority = CACHE_THREAD_PRIORITY;
                        }        
            
                        CeSetThreadPriority (m_hLazyWriterThread, dwPriority);
                    }
                }
            }
        }

        DEBUGMSG(ZONE_INIT, (TEXT("CreateCache: Successful.  page tree Size: %d KB, Start: %d, End: %d, CreateFlags: %d.\r\n"), 
            (DWORD) (m_liTreeSize.QuadPart / 1024), m_dwStart, dwEnd, m_dwCreateFlags));        
    }
    return fRet;
}

//
// ReadWriteCache: read/write diskcache for a number of sectors
//
//  dwFlags: 0 - perform cache read
//           CACHE_FORCE_WRITETHROUGH - perform a write and flush
//           CACHE_FORCE_WRITEBACK - perform a write, no flush, signal lazywriter if needed
//
DWORD CCache::ReadWriteCache (DWORD dwFlags, DWORD dwSectorNum, DWORD dwNumSectors, PVOID pBuffer)
{
    DWORD dwError = ERROR_SUCCESS;

    // Verify sector range is valid
    if (dwSectorNum < m_dwStart) {
        dwError = ERROR_INVALID_PARAMETER;
    } else {
        ULONGLONG ul64Ofst = SectorToSize (dwSectorNum - m_dwStart);
        ULONGLONG ul64End  = ul64Ofst + SectorToSize (dwNumSectors);

        if (ul64End > m_liTreeSize.QuadPart) {
            // accessing past end of page tree
            dwError = ERROR_INVALID_PARAMETER;
            
        } else {

            LPBYTE pData = (LPBYTE) pBuffer;

            while (ul64Ofst < ul64End) {
                DWORD cbAccessed = 0;
                DWORD cbSize = ((ul64End - ul64Ofst) > SIZE_1G)
                             ? SIZE_1G
                             : (DWORD) (ul64End - ul64Ofst);

                dwError = ReadWritePageTree (dwFlags, ul64Ofst, pData, cbSize, &cbAccessed);

                if (ERROR_SUCCESS != dwError) {
                    break;
                }

                DEBUGCHK (cbAccessed == cbSize);

                ul64Ofst += cbSize;

                // pData == NULL for zeroing cache
                if (pData) {
                    pData += cbSize;
                }
            }
        }
    }
    return dwError;
    
}

//
// ReadWriteCacheAtOffset: read/write diskcache for a number of bytes
//
//  dwFlags: 0 - perform cache read
//           CACHE_FORCE_WRITETHROUGH - perform a write and flush
//           CACHE_FORCE_WRITEBACK - perform a write, no flush, signal lazywriter if needed
//
DWORD CCache::ReadWriteCacheAtOffset (DWORD dwFlags, ULONGLONG ul64Ofst, PVOID pBuffer, DWORD cbSize,LPDWORD pcbAccessed)
{
    DWORD dwError;
    ULONGLONG ul64Start = SectorToSize (m_dwStart);

    // Verify offset is valid
    if (ul64Ofst < ul64Start) {
        dwError = ERROR_INVALID_PARAMETER;
    } else {
        ul64Ofst -= ul64Start;
        if (ul64Ofst >= m_liTreeSize.QuadPart) {
            dwError = ERROR_INVALID_PARAMETER;
        } else {
            // adjust size if necessary
            if (cbSize > m_liTreeSize.QuadPart - ul64Ofst) {
                cbSize = (DWORD) (m_liTreeSize.QuadPart - ul64Ofst);
            }

            dwError = ReadWritePageTree (dwFlags, ul64Ofst, (LPBYTE) pBuffer, cbSize, pcbAccessed);
        }
    }
    return dwError;
    
}


DWORD CCache::ReadWritePageTree (DWORD dwFlags, ULONGLONG ul64Ofst, LPBYTE pBuffer, DWORD cbSize, LPDWORD pcbAccessed)
{
    BOOL fSuccess;
    DWORD Result = ERROR_SUCCESS;
    ULARGE_INTEGER liOfst;

    liOfst.QuadPart = ul64Ofst;
    
    if (dwFlags) {
        // non-zero flag indicate it's write
        fSuccess = g_NKCacheFuncs.pPageTreeWrite (m_dwPageTreeId, &liOfst, pBuffer, cbSize, pcbAccessed);

        if (fSuccess) {
            if (CACHE_FORCE_WRITETHROUGH & dwFlags) {
                fSuccess = g_NKCacheFuncs.pPageTreeFlush (m_dwPageTreeId, &liOfst, cbSize, 0, NULL);
            } else if (m_hDirtySectorsEvent) {
                // signal lazy writer to flush on idle
                SetEvent (m_hDirtySectorsEvent);
            }
        }
    } else {
        // read
        fSuccess = g_NKCacheFuncs.pPageTreeRead  (m_dwPageTreeId, &liOfst, pBuffer, cbSize, pcbAccessed);
    }

    if (!fSuccess) {
        Result = GetLastError ();
        if (!Result) {
            Result = ERROR_GEN_FAILURE;
        }
    }

    return Result;
}

DWORD CCache::FlushSegment (const SECTOR_LIST_ENTRY *pSectorList)
{
    DWORD dwError = ERROR_SUCCESS;

    if (pSectorList->dwStartSector < m_dwStart) {
        dwError = ERROR_INVALID_PARAMETER;
        
    } else {
    
        ULARGE_INTEGER liStart;
        ULARGE_INTEGER liEnd;

        liStart.QuadPart = SectorToSize (pSectorList->dwStartSector - m_dwStart);
        liEnd.QuadPart   = liStart.QuadPart + SectorToSize (pSectorList->dwNumSectors);

        if (liEnd.QuadPart > m_liTreeSize.QuadPart) {
            // accessing past end of page tree
            dwError = ERROR_INVALID_PARAMETER;
            
        } else {
            // page align down Start
            liStart.LowPart  = PAGEALIGN_DOWN (liStart.LowPart);

            // page align up end.
            liEnd.QuadPart  += VM_PAGE_OFST_MASK;
            liEnd.LowPart    = PAGEALIGN_DOWN (liEnd.LowPart);

            // call to page tree to write-back and invalidate all pages in the range
            while (liEnd.QuadPart > liStart.QuadPart) {
                DWORD cbFlush = ((liEnd.QuadPart - liStart.QuadPart) > SIZE_1G)
                              ? SIZE_1G
                              : (DWORD) (liEnd.QuadPart - liStart.QuadPart);
                g_NKCacheFuncs.pPageTreeFlush (m_dwPageTreeId, &liStart, cbFlush, 0, NULL);
                liStart.QuadPart += cbFlush;
            }
        }
    }

    return dwError;
}


DWORD CCache::FlushCache (const SECTOR_LIST_ENTRY *pSectorList, DWORD dwNumEntries, DWORD dwFlushFlags)
{    
    DWORD dwError = ERROR_SUCCESS;

    if (pSectorList) {
        for (DWORD idx = 0; idx < dwNumEntries; idx ++) {
            DWORD dwFlushError = FlushSegment (pSectorList + idx);
            if (ERROR_SUCCESS != dwFlushError) {
                dwError = dwFlushError;
            }
        }
    } else {
        // flush the full tree
        if (!g_NKCacheFuncs.pPageTreeFlush (m_dwPageTreeId, NULL, 0, 0, NULL)) {
            dwError = GetLastError ();
            if (!dwError) {
                // PageTree function doesn't set last error??
                dwError = ERROR_GEN_FAILURE;
            }
        }
    }

    // If flush cache is not implemented on block driver or 
    // caller has disabled it for this request, we are done.
    if (!(m_dwInternalFlags & FLAG_DISABLE_SEND_FLUSH_CACHE)
        && !(dwFlushFlags & CACHE_SKIP_DEVICE_FLUSH)) {

        // save/restore last error since flush may be unsupported
        // i.e., this results in ERROR_INVALID_PARAMETER on a ramdisk
        DWORD dwLastError = GetLastError();

        // Call flush cache on the disk if it is implemented
        if (!FSDMGR_DiskIoControl(m_hDsk, IOCTL_DISK_FLUSH_CACHE, NULL, 0, NULL, 0, NULL, NULL)) {
            m_dwInternalFlags |= FLAG_DISABLE_SEND_FLUSH_CACHE;
        }

        SetLastError(dwLastError);

    }
    return dwError;
}

DWORD CCache::InvalidateSegment (const SECTOR_LIST_ENTRY *pSectorList)
{
    DWORD dwError = ERROR_SUCCESS;

    if (pSectorList->dwStartSector < m_dwStart) {
        dwError = ERROR_INVALID_PARAMETER;
        
    } else {
    
        ULARGE_INTEGER liStart;
        ULARGE_INTEGER liEnd;

        liStart.QuadPart = SectorToSize (pSectorList->dwStartSector - m_dwStart);
        liEnd.QuadPart   = liStart.QuadPart + SectorToSize (pSectorList->dwNumSectors);

        if (liEnd.QuadPart > m_liTreeSize.QuadPart) {
            // accessing past end of page tree
            dwError = ERROR_INVALID_PARAMETER;
            
        } else {
            // page align down Start
            liStart.LowPart  = PAGEALIGN_DOWN (liStart.LowPart);

            // page align up end.
            liEnd.QuadPart  += VM_PAGE_OFST_MASK;
            liEnd.LowPart    = PAGEALIGN_DOWN (liEnd.LowPart);

            // call to page tree to write-back and invalidate all pages in the range
            while (liEnd.QuadPart > liStart.QuadPart) {
                DWORD cbInvalidate = ((liEnd.QuadPart - liStart.QuadPart) > SIZE_1G)
                                   ? SIZE_1G
                                   : (DWORD) (liEnd.QuadPart - liStart.QuadPart);

                // flush dirty pages if there is any
                g_NKCacheFuncs.pPageTreeFlush (m_dwPageTreeId, &liStart, cbInvalidate, 0, NULL);                    

                g_NKCacheFuncs.pPageTreeInvalidate (m_dwPageTreeId, &liStart, cbInvalidate, 0);
                liStart.QuadPart += cbInvalidate;
            }
        }
    }

    return dwError;
}

DWORD CCache::InvalidateCache (const SECTOR_LIST_ENTRY *pSectorList, DWORD dwNumEntries, DWORD dwFlags)
{
    DWORD dwError = ERROR_SUCCESS;

    if (pSectorList) {
    
        for (DWORD idx = 0; idx < dwNumEntries; idx ++) {
            DWORD dwInvalidateError = InvalidateSegment (pSectorList + idx);
            if (ERROR_SUCCESS != dwInvalidateError) {
                dwError = dwInvalidateError;
            }
        }
    } else {
        g_NKCacheFuncs.pPageTreeInvalidate (m_dwPageTreeId, NULL, 0, 0);
    }
    return dwError;
}

BOOL CCache::CacheIoControl(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
    DWORD dwError = ERROR_SUCCESS;
    
    switch (dwIoControlCode) {

    case IOCTL_DISK_DELETE_SECTORS:
        if (nInBufSize != sizeof(DELETE_SECTOR_INFO)) {
            dwError = ERROR_INVALID_PARAMETER;
        } else {
            dwError = DeleteCachedSectors ((PDELETE_SECTOR_INFO)lpInBuf);
        }
        if (ERROR_SUCCESS != dwError)
        {
            SetLastError(dwError);
        }
        return (dwError == ERROR_SUCCESS);

    default:
        return FSDMGR_DiskIoControl(m_hDsk, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped);
    }
}

#endif
