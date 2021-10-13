//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "cache.h"

CCache::CCache (HDSK hDsk, DWORD dwStart, DWORD dwEnd, DWORD dwCacheSize, DWORD dwBlockSize, DWORD dwCreateFlags) :
        m_hDsk (hDsk),
        m_dwStart (dwStart),
        m_dwEnd (dwEnd),
        m_dwCacheSize (dwCacheSize),
        m_dwBlockSize (dwBlockSize),
        m_dwCreateFlags (dwCreateFlags),
        m_dwInternalFlags(0),
        m_pCacheLookup (NULL),
        m_dwDirtyCount(0),
        m_hDirtySectorsEvent(NULL),
        m_hLazyWriterThread(NULL)
{
}

CCache::~CCache()
{
    if (IsWriteBack()) {

        // Notify lazy-writer thread it is time to exit.
        SetEventData (m_hDirtySectorsEvent, EVENT_DATA_EXIT);
        SetEvent (m_hDirtySectorsEvent);

        // Wait for the lazy-writer thread to terminate within 20 seconds.       
        DWORD dwReturn = WaitForSingleObject (m_hLazyWriterThread, 20000);
        if (dwReturn == WAIT_TIMEOUT)
            TerminateThread (m_hLazyWriterThread, 0);

        CloseHandle (m_hLazyWriterThread);
        CloseHandle (m_hDirtySectorsEvent);
    }
    
    EnterCriticalSection (&m_cs);

    DWORD iBuffer;
    DWORD dwTotalBuffers = (m_dwCacheSize  * m_dwBlockSize + BUFFER_SIZE - 1) / BUFFER_SIZE;

    for (iBuffer = 0; iBuffer < dwTotalBuffers; iBuffer++) {        
        VirtualFree (m_apBufferPool[iBuffer], 0, MEM_RELEASE);
    }        
            
    LocalFree (m_apBufferPool);
    LocalFree (m_pCacheLookup);
    LocalFree (m_pStatus);

    LeaveCriticalSection (&m_cs);
    DeleteCriticalSection (&m_cs);
}

DWORD CCache::ReadWriteDisk (DWORD dwIOCTL, DWORD dwSectorNum, DWORD dwNumSectors, PVOID pBuffer)
{
    SG_REQ sg;
    BOOL fSuccess;
    
    sg.sr_start = dwSectorNum;
    sg.sr_num_sec = dwNumSectors; 
    sg.sr_num_sg = 1;
    sg.sr_status = ERROR_NOT_SUPPORTED;  // not used by ATADisk
    sg.sr_callback = NULL;
    sg.sr_sglist[0].sb_buf = (LPBYTE) MapPtrToProcess(pBuffer, GetCurrentProcess());
    sg.sr_sglist[0].sb_len = m_dwBlockSize * dwNumSectors;

    fSuccess = FSDMGR_DiskIoControl(m_hDsk, dwIOCTL, &sg, sizeof(sg),
        NULL, 0, NULL, NULL);

    DEBUGMSG(ZONE_IO, (L"ReadWriteDisk: Command: %s, Start Sector: %d, Num Sectors: %d\r\n", 
            dwIOCTL == DISK_IOCTL_READ ? L"Read" : L"Write", dwSectorNum, dwNumSectors));
    
    if (!fSuccess) {
        DWORD dwError = GetLastError();
        if (dwError == ERROR_SUCCESS) {
            DEBUGMSG(ZONE_IO, (L"ReadWriteDisk: Warning - Error code not set by block driver.  Setting to ERROR_GEN_FAILURE.")); 
            dwError = ERROR_GEN_FAILURE;
        }
        DEBUGMSG(ZONE_ERROR, (L"Read/Write Sector failed (%d) on Sector %d\r\n", dwError, dwSectorNum));
        return dwError;
    }

    return ERROR_SUCCESS;    
}

VOID CCache::ReadFromCacheBuffer (DWORD dwCacheIndex, LPBYTE pBuffer)
{
    DWORD dwByteOffset = dwCacheIndex * m_dwBlockSize;
    LPBYTE pCacheBuffer = m_apBufferPool[dwByteOffset / BUFFER_SIZE] + (dwByteOffset % BUFFER_SIZE);
    memcpy (pBuffer, pCacheBuffer, m_dwBlockSize);
}

VOID CCache::WriteToCacheBuffer (DWORD dwCacheIndex, LPBYTE pBuffer, BOOL fDirty)
{
    DWORD dwByteOffset = dwCacheIndex * m_dwBlockSize;
    LPBYTE pCacheBuffer = m_apBufferPool[dwByteOffset / BUFFER_SIZE] + (dwByteOffset % BUFFER_SIZE);
    memcpy (pCacheBuffer, pBuffer, m_dwBlockSize);

    if (IsWriteBack() && !IsDirty(dwCacheIndex) && fDirty) {
        SetDirtyStatus (dwCacheIndex);
    }
}


VOID CCache::WarmCache()
{
    DWORD iBuffer;
    DWORD dwTotalBuffers = (m_dwCacheSize  * m_dwBlockSize + BUFFER_SIZE - 1) / BUFFER_SIZE;
    DWORD dwSectorsPerBuffer = BUFFER_SIZE / m_dwBlockSize;

    // Read all data into cache buffers

    for (iBuffer = 0; iBuffer < dwTotalBuffers; iBuffer++) {        
        DWORD dwNumSectors = dwSectorsPerBuffer;

        // If this is the last buffer, only warm what is needed
        if ((iBuffer == (dwTotalBuffers - 1)) && (m_dwCacheSize % dwSectorsPerBuffer)) {
            dwNumSectors = m_dwCacheSize % dwSectorsPerBuffer;
        }

        ReadWriteDisk (DISK_IOCTL_READ, iBuffer * dwSectorsPerBuffer, dwNumSectors, m_apBufferPool[iBuffer]);
    }        

    // Update lookup table
    for (DWORD i = 0; i < m_dwCacheSize; i++) {
        m_pCacheLookup[i] = i;
    }
        
}

VOID CCache::SetDirtyStatus (DWORD dwIndex)
{
    if (m_dwDirtyCount == 0)
        SetEvent (m_hDirtySectorsEvent);
    
    m_pStatus[dwIndex] |= STATUS_DIRTY;
    m_dwDirtyCount++;
}

VOID CCache::ClearDirtyStatus (DWORD dwStartIndex, DWORD dwEndIndex)
{
    DWORD i;
    
    for (i = dwStartIndex; i <= dwEndIndex; i++) {
        m_pStatus[i] &= ~STATUS_DIRTY;
    }

    ASSERT (m_dwDirtyCount >= (dwEndIndex - dwStartIndex + 1));
    m_dwDirtyCount -= (dwEndIndex - dwStartIndex + 1);

    if (m_dwDirtyCount == 0)
        ResetEvent (m_hDirtySectorsEvent);
}

/*  CCache::CommitCacheSectors
 *
 *  Commits a set of consecutively cached sectors.
 *
 *  ENTRY
 *      dwStartIndex - The starting cache index to commit
 *      dwEndIndex - The ending cache index to commit
 *
 *  EXIT
 *      Returns appropriate error code.
 */

DWORD CCache::CommitCacheSectors (DWORD dwStartIndex, DWORD dwEndIndex)
{
    DWORD iBuffer, i;
    DWORD dwSectorsPerBuffer = BUFFER_SIZE / m_dwBlockSize;
    
    DWORD dwStartBuffer = dwStartIndex / dwSectorsPerBuffer;
    DWORD dwEndBuffer = dwEndIndex / dwSectorsPerBuffer;

    ASSERT (dwStartBuffer <= dwEndBuffer);
    
    DWORD dwTempStartBuffer = dwStartBuffer;
    DWORD dwTempStartIndex = dwStartIndex;

    while (dwTempStartBuffer <= dwEndBuffer) {

        DWORD dwTempEndBuffer = dwEndBuffer;
        DWORD dwTempEndIndex = dwEndIndex;

        // Only allow a max of MAX_SG_BUF buffers in the request
        if (dwTempStartBuffer + MAX_SG_BUF <= dwEndBuffer) {
            dwTempEndBuffer = dwTempStartBuffer + MAX_SG_BUF - 1;
            dwTempEndIndex = (dwTempEndBuffer + 1) * dwSectorsPerBuffer - 1;
        }

        DWORD dwSizeSg = sizeof(SG_REQ) + (dwTempEndBuffer - dwTempStartBuffer) * sizeof(SG_BUF);
        PSG_REQ psg = (PSG_REQ) LocalAlloc (LMEM_FIXED, dwSizeSg);
        if (!psg) {
            DEBUGMSG(ZONE_APIS, (TEXT("CommitCacheSectors: Memory allocation failed.\r\n")));        
            return ERROR_OUTOFMEMORY;
        }

        DWORD dwResult;
        
        psg->sr_start = m_pCacheLookup[dwTempStartIndex];
        psg->sr_num_sec = dwTempEndIndex - dwTempStartIndex + 1; 
        psg->sr_num_sg = dwTempEndBuffer - dwTempStartBuffer + 1;
        psg->sr_status = ERROR_NOT_SUPPORTED;  // not used by ATADisk
        psg->sr_callback = NULL;
        
        for (iBuffer = dwTempStartBuffer, i = 0; iBuffer <= dwTempEndBuffer; iBuffer++, i++) {

            // Determine the starting location within the buffer.  This will only be non-zero for the first buffer.
            DWORD dwStartBufIndex = (iBuffer == dwTempStartBuffer) ? (dwTempStartIndex % dwSectorsPerBuffer) : 0;

            // Determine the number of sectors to process for this current buffer.
            DWORD dwNumSectors = (iBuffer == dwTempEndBuffer) ? 
                (dwTempEndIndex % dwSectorsPerBuffer) - dwStartBufIndex + 1 : dwSectorsPerBuffer - dwStartBufIndex;
            
            psg->sr_sglist[i].sb_buf = (LPBYTE) MapPtrToProcess(m_apBufferPool[iBuffer] + m_dwBlockSize * dwStartBufIndex, GetCurrentProcess());
            psg->sr_sglist[i].sb_len = m_dwBlockSize * dwNumSectors;
            
        }    

        dwResult = FSDMGR_DiskIoControl(m_hDsk, DISK_IOCTL_WRITE, psg, dwSizeSg, NULL, 0, NULL, NULL);

        DEBUGMSG(ZONE_IO, (L"CommitCacheSectors: Command: Write, Start Sector: %d, Num Sectors: %d\r\n", psg->sr_start, psg->sr_num_sec));
        
        if (!dwResult) {
            DEBUGMSG(ZONE_ERROR, (L"CommitCacheSectors failed on Sectors [%d, %d]\r\n", m_pCacheLookup[dwTempStartIndex], m_pCacheLookup[dwTempEndIndex]));
            SetLastError(psg->sr_status);
            LocalFree (psg);
            return psg->sr_status;
        }

        // Clear the dirty bits if write is successful
        ClearDirtyStatus (dwTempStartIndex, dwTempEndIndex);
        
        LocalFree (psg);

        dwTempStartBuffer = dwTempEndBuffer + 1;
        dwTempStartIndex = dwTempEndIndex + 1;

    }
    
    return ERROR_SUCCESS;
    
}


/*  CCache::CommitDirtySectors
 *
 *  Commits all cached dirty sectors within the desired range.
 *
 *  ENTRY
 *      dwStartSector - Starting sector to search for sectors to commit
 *      dwNumSectors - Number of sectors to search
 *      dwOption - COMMIT_ALL_SECTORS to commit all dirty sectors that falls in the desired range
 *               - COMMIT_DIFF_SECTORS to commit the cached sector only if it is different  
 *                 from the sector passed in, since anything different will be replaced.
 *               - COMMIT_SPECIFIED_SECTORS to commit the cached sector only if it is the same as
 *                 the sector passed in, indicating that we want to flush those sectors.
 *
 *  EXIT
 *      Returns appropriate error code.
 */
 
VOID CCache::CommitDirtySectors (DWORD dwStartSector, DWORD dwNumSectors, DWORD dwOption)
{    
    DWORD dwEndSector = dwStartSector + dwNumSectors - 1;
    DWORD dwStartCacheIndex = GetIndex(dwStartSector); 
    DWORD dwEndCacheIndex = GetIndex(dwEndSector); 
    DWORD dwNumCacheSectors = dwNumSectors;

    // If the number of sectors to write is greater than the cache size, then flush the whole cache

    if ((dwOption == COMMIT_DIFF_SECTORS) && (dwNumSectors > m_dwCacheSize)) {
        dwOption = COMMIT_ALL_SECTORS;
        dwStartCacheIndex = 0;
        dwEndCacheIndex = m_dwCacheSize - 1;
        dwNumCacheSectors = m_dwCacheSize;
    }

    DWORD dwCurrentCacheIndex = dwStartCacheIndex;
    DWORD dwCurrentSector = dwStartSector;

    while (dwNumCacheSectors) {

        // If we've reached the end of the cache and still have more sectors to 
        // commit, wrap around to the beginning

        if (dwCurrentCacheIndex == m_dwCacheSize) {
            dwCurrentCacheIndex = 0;
        }

        // If COMMIT_ALL_SECTORS is set, commit any cached dirty sector
        // If COMMIT_REPLACE_SECTORS is set, commit any dirty sector only if it is not the 
        // same sector that we plan to write to.
        // If COMMIT_FLUSH_SECTORS is set, commit any dirty sector only if it is the 
        // same sector that the caller wanted flushed.

        if (IsDirty(dwCurrentCacheIndex) && 
           ((dwOption == COMMIT_ALL_SECTORS) ||
           ((dwOption == COMMIT_DIFF_SECTORS) && (m_pCacheLookup[dwCurrentCacheIndex] != dwCurrentSector)) ||
           ((dwOption == COMMIT_SPECIFIED_SECTORS) && (m_pCacheLookup[dwCurrentCacheIndex] == dwCurrentSector))))
        {
            DWORD dwPrevCachedSector;
            dwStartCacheIndex = dwCurrentCacheIndex;
            
            // Found a dirty sector that we want to commit.  Commit all consecutive
            // sectors starting with this one that are dirty

            do {
                
                dwPrevCachedSector = m_pCacheLookup[dwCurrentCacheIndex++];
                dwNumCacheSectors--;
                dwCurrentSector++;                
            
            } while (dwNumCacheSectors && IsDirty(dwCurrentCacheIndex) && 
                    (m_pCacheLookup[dwCurrentCacheIndex] == dwPrevCachedSector+1) &&
                    (dwCurrentCacheIndex < m_dwCacheSize));

            // Commit these dirty sectors
            CommitCacheSectors (dwStartCacheIndex, dwCurrentCacheIndex - 1);
        
        }
        else
        {
            dwNumCacheSectors--;
            dwCurrentCacheIndex++;
            dwCurrentSector++;
        }
        
    }
    
}

/*  CCache::CommitNextDirtyRun
 *
 *  Commits the next set of consecutively cached sectors starting
 *  the cache index passed in.
 *
 *  ENTRY
 *      dwStartIndex - Starting cache index to search for sectors to commit
 *
 *  EXIT
 *      Returns appropriate error code.
 */
 
DWORD CCache::CommitNextDirtyRun (DWORD dwStartIndex)
{
    DWORD dwEndIndex;
    DWORD dwPrevCachedSector;
    
    // Find first dirty cache index
    while (dwStartIndex < m_dwCacheSize && !IsDirty(dwStartIndex)) {
        dwStartIndex++;
    }
    
    if (dwStartIndex == m_dwCacheSize) {
        return 0;
    }

    // Commit all consecutive sectors starting with this one
    // that are dirty

    dwEndIndex = dwStartIndex + 1;
    dwPrevCachedSector = m_pCacheLookup[dwStartIndex];
    
    while ((dwEndIndex < m_dwCacheSize) && IsDirty(dwEndIndex) &&
               (m_pCacheLookup[dwEndIndex] == dwPrevCachedSector+1))
    {
        dwPrevCachedSector++;
        dwEndIndex++;
    }

    CommitCacheSectors(dwStartIndex, dwEndIndex-1);

    // Return the next starting point.
    return (dwEndIndex == m_dwCacheSize) ? 0 : dwEndIndex;
    
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
 
BOOL CCache::DeleteCachedSectors (PDELETE_SECTOR_INFO pInfo)
{
    DWORD dwSector;
    DWORD dwRet;

    EnterCriticalSection (&m_cs);
    
    for (dwSector = pInfo->startsector; dwSector < pInfo->startsector + pInfo->numsectors; dwSector++) {
        InvalidateSector(dwSector);
    }

    LeaveCriticalSection (&m_cs);

    // If delete sectors is not implemented on block driver, we are done.
    if (m_dwInternalFlags & FLAG_DISABLE_SEND_DELETE) {
        return TRUE;
    }

    // Call delete sectors on the disk if it is implemented
    if (!FSDMGR_DiskIoControl(m_hDsk, IOCTL_DISK_DELETE_SECTORS, pInfo, sizeof(DELETE_SECTOR_INFO), 
        NULL, 0, &dwRet, NULL))
    {
        m_dwInternalFlags |= FLAG_DISABLE_SEND_DELETE;
    }

    return TRUE;
    
    
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
    DWORD dwNextCacheIndex = 0;
    
    while (TRUE) {        
    
        DWORD dwResult = WaitForSingleObject (m_hDirtySectorsEvent, INFINITE);

        // If the exiting event was set or we had a failure, then exit this thread.
        if (dwResult == WAIT_FAILED || GetEventData(m_hDirtySectorsEvent) == EVENT_DATA_EXIT) {
            return;
        }
    
        EnterCriticalSection (&m_cs);

        if (m_dwDirtyCount) {
            dwNextCacheIndex = CommitNextDirtyRun (dwNextCacheIndex);
        }

        LeaveCriticalSection (&m_cs);
    }
}


BOOL CCache::InitCache()
{
    DWORD iBuffer;
    DWORD dwTotalBuffers = (m_dwCacheSize  * m_dwBlockSize + BUFFER_SIZE - 1) / BUFFER_SIZE;
    DWORD dwPartialBufferSize = (m_dwCacheSize  * m_dwBlockSize) % BUFFER_SIZE;

    InitializeCriticalSection (&m_cs);
    
    m_apBufferPool = (LPBYTE*) LocalAlloc (LMEM_FIXED, dwTotalBuffers * sizeof(LPBYTE));
    if (!m_apBufferPool) {       
        DEBUGMSG(ZONE_ERROR, (TEXT("InitCache: Memory allocation failed for 0x%x bytes.\r\n"), dwTotalBuffers * sizeof(LPBYTE)));        
        return FALSE;
    }

    m_pCacheLookup = (PDWORD) LocalAlloc (LMEM_FIXED, m_dwCacheSize * sizeof(DWORD));
    if (!m_pCacheLookup) {
        DEBUGMSG(ZONE_ERROR, (TEXT("InitCache: Memory allocation failed for 0x%x bytes.\r\n"), m_dwCacheSize * sizeof(DWORD)));    
        LocalFree (m_apBufferPool);
        return FALSE;
    }
    memset (m_pCacheLookup, 0xff, m_dwCacheSize * sizeof(DWORD));

    m_pStatus = (LPBYTE) LocalAlloc (LMEM_ZEROINIT, m_dwCacheSize);
    if (!m_pStatus) {
        DEBUGMSG(ZONE_ERROR, (TEXT("InitCache: Memory allocation failed for 0x%x bytes.\r\n"), m_dwCacheSize));    
        LocalFree (m_apBufferPool);
        LocalFree (m_pCacheLookup);        
        return FALSE;
    }

    for (iBuffer = 0; iBuffer < dwTotalBuffers; iBuffer++) {

        DWORD dwBufferSize = (dwPartialBufferSize && (iBuffer == dwTotalBuffers-1)) ? dwPartialBufferSize : BUFFER_SIZE;
        m_apBufferPool[iBuffer]  = (LPBYTE) VirtualAlloc (NULL, dwBufferSize, MEM_COMMIT, PAGE_READWRITE);
        
        if (!m_apBufferPool[iBuffer]) {
            
            // If we can't allocate, then the number of valid buffers up to this point becomes the cache size
            DEBUGMSG(ZONE_APIS, (TEXT("CreateCache: VirtualAlloc failed for 0x%x bytes.\r\n"), m_dwCacheSize  * m_dwBlockSize));        
            m_dwCacheSize = iBuffer * BUFFER_SIZE / m_dwBlockSize;
            RETAILMSG(1, (TEXT("CreateCache: VirtualAlloc failed for 0x%x bytes.\r\n"), m_dwCacheSize));                    
            break;
        }
    }  

    if (m_dwCreateFlags & CACHE_FLAG_WARM) {
        WarmCache();
    }

    if (IsWriteBack()) {
        
        DWORD dwPriority;
        
        m_hDirtySectorsEvent = CreateEvent (NULL, TRUE, FALSE, NULL);
        SetEventData (m_hDirtySectorsEvent, EVENT_DATA_INIT);
        m_hLazyWriterThread = CreateThread(NULL, 0, ::LazyWriterThread, (LPVOID)this, 0, NULL);

        // Get the priority for the lazy-writer thread from the registry
        if (!FSDMGR_GetRegistryValue((HDSK)m_hDsk, L"LazyWriterThreadPrio256", &dwPriority)) {
            dwPriority = CACHE_THREAD_PRIORITY;
        }        

        CeSetThreadPriority (m_hLazyWriterThread, dwPriority);
    }

    DEBUGMSG(ZONE_INIT, (TEXT("CreateCache: Successful.  Cache Size: %d KB, Start: %d, End: %d.\r\n"), m_dwCacheSize * m_dwBlockSize / 1024, m_dwStart, m_dwEnd));        

    return TRUE;
}

DWORD CCache::ResizeCache (DWORD dwResizeFlags, DWORD dwSize)
{
    // Calculate number of whole buffers
    DWORD dwOldNumBuffers = (m_dwCacheSize  * m_dwBlockSize) / BUFFER_SIZE;
    DWORD dwNewNumBuffers = (dwSize  * m_dwBlockSize) / BUFFER_SIZE;
    
    if (m_dwBlockSize == dwSize) {
        return ERROR_SUCCESS;
    }

    FlushCache (NULL, 0);
   
    PDWORD pNewLookup = (PDWORD) LocalAlloc (LMEM_FIXED, dwSize * sizeof(DWORD));
    if (!pNewLookup) {
        DEBUGMSG(ZONE_ERROR, (TEXT("ResizeCache: Not enough memory.\r\n")));        
        return ERROR_OUTOFMEMORY;
    }

    // Add one extra entry to account for a potential extra partial buffer.
    LPBYTE* apNewBufferPool = (LPBYTE*) LocalAlloc (LMEM_FIXED, (dwNewNumBuffers+1) * sizeof(LPBYTE));
    if (!apNewBufferPool) {
        LocalFree (pNewLookup);
        DEBUGMSG(ZONE_ERROR, (TEXT("ResizeCache: Not enough memory.\r\n")));        
        return ERROR_OUTOFMEMORY;
    }

    LPBYTE pNewStatus = (LPBYTE) LocalAlloc (LMEM_ZEROINIT, dwSize);
    if (!pNewStatus) {
        LocalFree (pNewLookup);
        LocalFree (apNewBufferPool);
        DEBUGMSG(ZONE_ERROR, (TEXT("ResizeCache: Not enough memory.\r\n")));        
        return ERROR_OUTOFMEMORY;
    }

    EnterCriticalSection (&m_cs);

    if ((m_dwCacheSize  * m_dwBlockSize) % BUFFER_SIZE) {
        // There is an extra partial buffer which we have to free in the old buffer pool
        VirtualFree (m_apBufferPool[dwOldNumBuffers], 0, MEM_RELEASE);
    }

    DWORD iBuffer = 0;
    m_dwCacheSize = dwSize;

    DWORD dwNumBuffersToCopy = (dwOldNumBuffers > dwNewNumBuffers) ? dwNewNumBuffers : dwOldNumBuffers;
    while (iBuffer < dwNumBuffersToCopy) {
        apNewBufferPool[iBuffer] = m_apBufferPool[iBuffer];
        iBuffer++;
    }
        
    while (iBuffer < dwOldNumBuffers) {
        
        // Shrink the number of buffers
        VirtualFree (m_apBufferPool[iBuffer], 0, MEM_RELEASE);
        iBuffer++;
    }
    
    while (iBuffer < dwNewNumBuffers) {
        
        // Increase the number of buffers
        apNewBufferPool[iBuffer]  = (LPBYTE) VirtualAlloc (NULL, BUFFER_SIZE, MEM_COMMIT, PAGE_READWRITE);

        // If we can't allocate, then the number of valid buffers up to this point becomes the cache size
        if (!apNewBufferPool[iBuffer]) {
            m_dwCacheSize = iBuffer * BUFFER_SIZE / m_dwBlockSize;
            break;
        }
        iBuffer++;
    }

    if ((m_dwCacheSize  * m_dwBlockSize) % BUFFER_SIZE) {
        // There is an extra partial buffer which we have to allocate in the new buffer pool
        apNewBufferPool[dwNewNumBuffers]  = (LPBYTE) VirtualAlloc (NULL, (m_dwCacheSize  * m_dwBlockSize) % BUFFER_SIZE, MEM_COMMIT, PAGE_READWRITE);
        if (!apNewBufferPool[dwNewNumBuffers]) {
            m_dwCacheSize = dwNewNumBuffers * BUFFER_SIZE / m_dwBlockSize;
        }
    }

    LocalFree (m_pCacheLookup);
    LocalFree (m_apBufferPool);
    LocalFree (m_pStatus);
    m_pCacheLookup = pNewLookup;
    m_apBufferPool = apNewBufferPool;
    m_pStatus = pNewStatus;
    
    if (dwResizeFlags & CACHE_FLAG_WARM) {
        WarmCache();
    } else {
        memset (m_pCacheLookup, 0xff, m_dwCacheSize * sizeof(DWORD));
    }

    LeaveCriticalSection (&m_cs);
    
    return ERROR_SUCCESS;
    

}    

DWORD CCache::ReadCache (DWORD dwReadFlags, DWORD dwSectorNum, DWORD dwNumSectors, PVOID pBuffer)
{
    BOOL bReadDisk = FALSE;
    DWORD dwSector;
    DWORD dwError = ERROR_SUCCESS;

    // Verify sector range is valid

    DWORD dwReadSectorEnd = dwSectorNum + dwNumSectors - 1;
    if (dwSectorNum < m_dwStart || dwReadSectorEnd > m_dwEnd || dwSectorNum > dwReadSectorEnd) {
        DEBUGMSG(ZONE_ERROR, (TEXT("CachedRead: Invalid range of sectors to read [0x%x, 0x%x].\r\n"), dwSectorNum, dwReadSectorEnd));        
        return ERROR_INVALID_PARAMETER;
    }

    // If cache size is 0, read directly from disk

    if (!m_dwCacheSize) {
        return ReadWriteDisk (DISK_IOCTL_READ, dwSectorNum, dwNumSectors, pBuffer);
    }

    EnterCriticalSection (&m_cs);

    if (dwNumSectors > m_dwCacheSize) {

        // If the request is bigger than cache size, some sectors will not be cached, 
        // so read whole request from disk
        bReadDisk = TRUE;
        
    } else {
    
        // If any sector to be read is not in the cache, then read whole request from disk

        for (dwSector = dwSectorNum; dwSector < dwSectorNum+dwNumSectors; dwSector++) {
            if (!IsCached(dwSector)) {
                bReadDisk = TRUE;
                break;
            }
        }
   }

    // If reading from disk into user buffer, commit any dirty sectors in cache that map to the
    // range before we update the cache

    if (bReadDisk) {
        if (IsWriteBack()) {
            CommitDirtySectors (dwSectorNum, dwNumSectors, COMMIT_ALL_SECTORS);
        }
        dwError = ReadWriteDisk(DISK_IOCTL_READ, dwSectorNum, dwNumSectors, pBuffer);
    }    

    if (dwError != ERROR_SUCCESS) {
        LeaveCriticalSection (&m_cs);
        return dwError;
    }

    // Now, dwNumSectors will refer to number of sectors to update in the cache.  If the number of 
    // sectors is greater than cache size, set it to cache size to update the entire cache

    if (dwNumSectors > m_dwCacheSize) {
        dwNumSectors = m_dwCacheSize;
    }

    // If we read from disk, update the cache with the new data.  Otherwise, copy the data from the 
    // cache into the user buffer.

    for (dwSector = dwSectorNum; dwSector < dwSectorNum+dwNumSectors; dwSector++) {
        
        DWORD dwCacheIndex = GetIndex(dwSector); 
        LPBYTE pBufferData = (LPBYTE)pBuffer + ((dwSector - dwSectorNum) * m_dwBlockSize);
        
        if (bReadDisk) {
            if (m_pCacheLookup[dwCacheIndex] != dwSector) {
                WriteToCacheBuffer (dwCacheIndex, pBufferData, FALSE);
                m_pCacheLookup[dwCacheIndex] = dwSector;
            }    
        } else {
            ReadFromCacheBuffer (dwCacheIndex, pBufferData);
        }
    }    

    LeaveCriticalSection (&m_cs);

    return ERROR_SUCCESS;
    
}


DWORD CCache::WriteCache (DWORD dwWriteFlags, DWORD dwSectorNum, DWORD dwNumSectors, PVOID pBuffer)
{
    DWORD dwSector;
    DWORD dwError = ERROR_SUCCESS;
    
    // Verify sector range is valid

    DWORD dwReadSectorEnd = dwSectorNum + dwNumSectors - 1;
    if (dwSectorNum < m_dwStart || dwReadSectorEnd > m_dwEnd || dwSectorNum > dwReadSectorEnd) {
        DEBUGMSG(ZONE_ERROR, (TEXT("CachedWrite: Invalid range of sectors to write [0x%x, 0x%x].\r\n"), dwSectorNum, dwReadSectorEnd));        
        return ERROR_INVALID_PARAMETER;
    }

    if (!m_dwCacheSize) {
        return ReadWriteDisk(DISK_IOCTL_WRITE, dwSectorNum, dwNumSectors, pBuffer);        
    }

    EnterCriticalSection (&m_cs);
  
    if (IsWriteBack()) {

        // Write-back case

        CommitDirtySectors (dwSectorNum, dwNumSectors, COMMIT_DIFF_SECTORS);

        // If the number of sectors is greater than the cache size, write-through.  
        // Also, if write-through specifically specified, write-though

        if ((dwNumSectors > m_dwCacheSize) || (dwWriteFlags & CACHE_FORCE_WRITETHROUGH)) {
            dwError = ReadWriteDisk(DISK_IOCTL_WRITE, dwSectorNum, dwNumSectors, pBuffer);
            if (dwError != ERROR_SUCCESS)
                goto exit;
        }

        // If the number of sectors is greater than the cache size,
        // set dwNumSectors to the cache size to indicate to update entire cache.
        
        if (dwNumSectors > m_dwCacheSize) {
            dwNumSectors = m_dwCacheSize;
        }
        
    } else {        

        // Write-through case

        dwError = ReadWriteDisk(DISK_IOCTL_WRITE, dwSectorNum, dwNumSectors, pBuffer);

        // Even with an error, go ahead and update the cache, so that any sector writes that might
        // have succeeded will have the correct cached sector.        
    }

    // Copy the written sectors to the cache

    for (dwSector = dwSectorNum; dwSector < dwSectorNum+dwNumSectors; dwSector++) {
        
        DWORD dwCacheIndex = GetIndex(dwSector);
        LPBYTE pBufferData = (LPBYTE)pBuffer + ((dwSector - dwSectorNum) * m_dwBlockSize);
        
        WriteToCacheBuffer (dwCacheIndex, pBufferData, !(dwWriteFlags & CACHE_FORCE_WRITETHROUGH));
        m_pCacheLookup[dwCacheIndex] = dwSector;
    }    

    DEBUGMSG(ZONE_APIS, (TEXT("CachedWrite: Cached sectors.  Start: %d, Num: %d.\r\n"), dwSectorNum, dwNumSectors));        

exit:
    LeaveCriticalSection (&m_cs);
    return dwError;
    
}


DWORD CCache::FlushCache (PSECTOR_LIST_ENTRY pSectorList, DWORD dwNumEntries)
{    
    if (m_dwDirtyCount && IsWriteBack()) {
        EnterCriticalSection (&m_cs);

        // If a sector list is not provided, flush the entire cache.
        if (!pSectorList) {        
            CommitDirtySectors (0, m_dwCacheSize, COMMIT_ALL_SECTORS);
        }
        else {
            for (DWORD iEntry = 0; iEntry < dwNumEntries; iEntry++) {
                CommitDirtySectors (pSectorList[iEntry].dwStartSector, pSectorList[iEntry].dwNumSectors, COMMIT_SPECIFIED_SECTORS);
            }
        }
        LeaveCriticalSection (&m_cs);
    }

    // If flush cache is not implemented on block driver, we are done.
    if (m_dwInternalFlags & FLAG_DISABLE_SEND_FLUSH_CACHE) {
        return ERROR_SUCCESS;
    }

    // Call flush cache on the disk if it is implemented
    if (!FSDMGR_DiskIoControl(m_hDsk, IOCTL_DISK_FLUSH_CACHE, NULL, 0, NULL, 0, NULL, NULL)) {
        m_dwInternalFlags |= FLAG_DISABLE_SEND_FLUSH_CACHE;
    }


    return ERROR_SUCCESS;
}


VOID CCache::InvalidateSector (DWORD dwSector)
{
    DWORD dwCacheIndex = GetIndex(dwSector); 
    
    // If the sector is cached, invalidate it by setting the lookup value to -1
    if (m_pCacheLookup[dwCacheIndex] == dwSector) {

        m_pCacheLookup[dwCacheIndex] = INVALID_CACHE_ID;

        // If the sector is dirty, no need to commit it anymore, so clear status.
        if (IsWriteBack() && IsDirty(dwCacheIndex)) {
            ClearDirtyStatus (dwCacheIndex, dwCacheIndex);
        }
    }
}

DWORD CCache::InvalidateCache (PSECTOR_LIST_ENTRY pSectorList, DWORD dwNumEntries, DWORD dwFlags)
{
    EnterCriticalSection (&m_cs);

    for (DWORD iEntry = 0; iEntry < dwNumEntries; iEntry++) {

        for (DWORD iSector = pSectorList[iEntry].dwStartSector; 
             iSector < pSectorList[iEntry].dwStartSector + pSectorList[iEntry].dwNumSectors; 
             iSector++) 
        {
            InvalidateSector(iSector);
        }
        
    }
    
    LeaveCriticalSection (&m_cs);
    
    return ERROR_SUCCESS;
}

BOOL CCache::CacheIoControl(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
    switch (dwIoControlCode) {

    case IOCTL_DISK_DELETE_SECTORS:
        if (nInBufSize != sizeof(DELETE_SECTOR_INFO))
            return ERROR_INVALID_PARAMETER;
        return DeleteCachedSectors ((PDELETE_SECTOR_INFO)lpInBuf);

    default:
        return FSDMGR_DiskIoControl(m_hDsk, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped);
    }
}

DWORD WINAPI LazyWriterThread (LPVOID lpParameter)
{
    CCache* pCache = (CCache*)lpParameter;
    pCache->LazyWriterThread();
    return 0;
}
        
