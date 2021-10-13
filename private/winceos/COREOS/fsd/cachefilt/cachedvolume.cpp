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

#include "cachefilt.hpp"

bool gBootCompleted = false;

static DWORD WINAPI WriteBackThread (LPVOID pParam);

// used in UnitTests
static HANDLE ThunkFindFirstFileW (
    DWORD   dwVolume,
    HANDLE  hProcess,
    PCWSTR  pwsFileSpec,
    PWIN32_FIND_DATAW pfd
    )
{
    return ::FindFirstFileW( pwsFileSpec, pfd);
}


//------------------------------------------------------------------------------
// GLOBAL STATE
//------------------------------------------------------------------------------

static CachedVolume_t *g_pCachedVolumeList; // Head of list of volumes
static CRITICAL_SECTION g_csVolumeList;
static HANDLE g_hDll;


void InitVolumes ()
{
    InitializeCriticalSection (&g_csVolumeList);
    InitViewPool ();
}

void CleanupVolumes ()
{
    DeleteCriticalSection (&g_csVolumeList);
}


//------------------------------------------------------------------------------
// CACHED VOLUME METHODS: Volume Operations
//------------------------------------------------------------------------------

//
// Default constructor for unit testing
//
CachedVolume_t::CachedVolume_t ()
{
    m_hDisk = NULL;
    m_OptimalPagingSize = 0;
    m_VolumeFlags = 0;

    m_pMapList = NULL;
    m_MapListCount = 0;
    m_pNext = NULL;

    m_hWriteBackThread = NULL;
    m_hWriteBackEvent = NULL;
    m_hExitEvent = NULL;
    m_WriteBackTimeout = 0;
    m_PendingFlushes = 0;
    m_LRUCounter = 0;
    m_pDirectoryHash = new DirectoryHash_t(this, TRUE);

    FILTERHOOK hook = {0};

    hook.cbSize =sizeof(hook);
    hook.pFindFirstFileW = (PFINDFIRSTFILEW)::ThunkFindFirstFileW;
    hook.pFindNextFileW = (PFINDNEXTFILEW)::FindNextFileW;
    hook.pFindClose = (PFINDCLOSE)::FindClose;

    m_FilterHook.Init(&hook);
}


//
// Constructor for a CachedVolume_t - adds itself to the list of volumes
//
CachedVolume_t::CachedVolume_t (
   HDSK        hdsk,
   FILTERHOOK *pHook
   )  
{
    DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:CachedVolume_t, pVolume=0x%08x hDsk=0x%08x\r\n",
                            this, hdsk));
    
    // Initialize this volume
    m_hDisk = hdsk;
    m_hVolume = FSDMGR_GetVolumeHandle (m_hDisk);
    DEBUGCHK (m_hVolume);
    m_OptimalPagingSize = UserKInfo[KINX_PAGESIZE];
    m_VolumeFlags = 0;
    
    m_pMapList = NULL; 
    m_MapListCount = 0;
    m_pNext = NULL;
    
    m_hWriteBackThread = NULL;
    m_hWriteBackEvent = NULL;
    m_hExitEvent = NULL;
    m_WriteBackTimeout = 0;
    m_PendingFlushes = 0;
    m_LRUCounter = 0;
    m_pDirectoryHash = NULL;
    
    InitializeCriticalSection (&m_csVolume);
    InitializeCriticalSection (&m_csUnderlyingIO);
    InitializeCriticalSection (&m_csWriteBack);

    m_FilterHook.Init (pHook);

    // Get volume properties
    FSD_VOLUME_INFO volinfo;
    memset (&volinfo, 0, sizeof (FSD_VOLUME_INFO));
    volinfo.cbSize = sizeof (FSD_VOLUME_INFO);
    if (GetVolumeInfo (&volinfo)) {
        if (volinfo.dwAttributes & FSD_ATTRIBUTE_READONLY) {
            m_VolumeFlags |= CACHE_VOLUME_READ_ONLY;
        }
        
        // Must page in multiples of RAM pages 
        if (volinfo.dwBlockSize
            && (volinfo.dwBlockSize >= m_OptimalPagingSize)
            && (0 == (volinfo.dwBlockSize % m_OptimalPagingSize))) {
            m_OptimalPagingSize = volinfo.dwBlockSize;
        }

        if (volinfo.dwFlags & FSD_FLAG_WFSC_SUPPORTED) {
            m_VolumeFlags |= CACHE_VOLUME_WFSC_SUPPORTED;
        }
        if (volinfo.dwFlags & FSD_FLAG_TRANSACTION_SAFE) {
            m_VolumeFlags |= CACHE_VOLUME_TRANSACTION_SAFE;
        }
    }

    // Caching can also be turned off for the entire volume, by setting
    // a registry flag.  The only reason to do this would be if the
    // filter was being used to add LockFile support to the volume.
    DWORD EnableFileCache = TRUE;
    DWORD TempVal;
    if (FSDMGR_GetRegistryValue (m_hDisk, TEXT("EnableFileCache"), &TempVal)) {
        EnableFileCache = TempVal;
    }

    // We cannot guarantee transacted writes for cached files.  If someone
    // is trying to transact writes for an entire volume, disable caching.
    if (volinfo.dwFlags & FSD_FLAG_TRANSACT_WRITES) {
        DEBUGMSG (ZONE_INIT || ZONE_ERROR, (L"CACHEFILT: Disabling file cache on write-transacted volume\r\n"));
        DEBUGCHK (!ZONE_WARNING_BREAK);  // Remove cachefilt, turn off write transactions, or disable this zone to stop the DEBUGCHK
        EnableFileCache = FALSE;
    }     

#ifdef DEBUG
    // Require LockIOBuffers registry flag
    DWORD LockIOBuffers = FALSE;
    FSDMGR_GetRegistryValue (m_hDisk, TEXT("LockIOBuffers"), &LockIOBuffers);
    if (!LockIOBuffers) {
        // Without this registry setting, it's illegal to pass memory-mapped
        // file buffers to ReadFile or WriteFile.  Taking a page fault on those
        // buffers could lead to deadlocks.
        // Look at %_WINCEROOT%\public\common\oak\files\common.reg to see how
        // LockIOBuffers is set on a few different file systems.
        DEBUGMSG (ZONE_INIT || ZONE_ERROR, (L"CACHEFILT: Missing LockIOBuffers registry setting!\r\n"));
        DEBUGCHK (!ZONE_WARNING_BREAK);  // Add LockIOBuffers setting or disable this zone to stop the DEBUGCHK
    }
#endif // DEBUG
    
    if (!EnableFileCache) {
        DEBUGMSG (ZONE_INIT, (L"CACHEFILT: pVolume=0x%08x in uncached mode\r\n", this));
        m_VolumeFlags |= CACHE_VOLUME_UNCACHED;
    } else {
        DWORD EnableWriteBack = CACHE_DEFAULT_WRITEBACK_STATUS;
        if (FSDMGR_GetRegistryValue (m_hDisk, TEXT("EnableFileCacheWriteBack"), &TempVal)) {
            EnableWriteBack = TempVal;
        }

        InitWriteBack (EnableWriteBack);

        DWORD LazyCommitTransactions = EnableWriteBack;
        if (FSDMGR_GetRegistryValue (m_hDisk, TEXT("LazyCommitTransactions"), &TempVal)) {
            LazyCommitTransactions = TempVal;
        }

        // If the volume is transaction safe, disable the volume from doing commit
        // transactions, and have the cache filter writeback thread do it instead.
        if (LazyCommitTransactions && (volinfo.dwFlags & FSD_FLAG_TRANSACTION_SAFE)) {
            DisableCommitTransactions();
        }
    }

    DEBUGMSG (ZONE_INIT, (L"CACHEFILT: LazyCommitTransactions = %d.\r\n", CommitTransactionsOnIdle()));
    
    // Add a ref to the cache filter DLL so that it cannot be unloaded.
    // This prevents the writeback thread from unloading the DLL on itself.
    if (!g_hDll) {
        WCHAR DllName[MAX_PATH];
        DllName[0] = 0;
        VERIFY (FSDMGR_GetRegistryString (m_hDisk, L"Dll", DllName, MAX_PATH));
        g_hDll = LoadLibrary (DllName);
        DEBUGCHK (g_hDll);
    }
}


//
// Destructor for a CachedVolume_t - assumes the volume is already removed from the list.
//
CachedVolume_t::~CachedVolume_t ()
{
    DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:~CachedVolume_t, pVolume=0x%08x hDsk=0x%08x\r\n",
                            this, m_hDisk));
    
    // Flush and close any remaining open map files
    FlushViewPool();

    CloseAllMaps();

    if (CommitTransactionsOnIdle()) {
        CommitTransactions();
    }
    
    // Remove from the volume list
    EnterCriticalSection (&g_csVolumeList);
    if ((NULL == g_pCachedVolumeList) || (g_pCachedVolumeList == this)) {
        g_pCachedVolumeList = m_pNext;
    } else {
        CachedVolume_t* pCurVolume  = g_pCachedVolumeList;
        while (pCurVolume->m_pNext && (pCurVolume->m_pNext != this)) {
             pCurVolume = pCurVolume->m_pNext;
        }
        if (pCurVolume->m_pNext == this) {
            pCurVolume->m_pNext = m_pNext;
        }
    }
    LeaveCriticalSection (&g_csVolumeList);
    m_pNext = NULL;

    // Clean up write-back resources
    if (m_hWriteBackThread) {
        DEBUGCHK (m_VolumeFlags & CACHE_VOLUME_WRITE_BACK);
        
        // This might be called on the write-back thread itself (via the
        // FSDMGR_AsyncExitVolume call), so we don't want to wait here in that
        // case or we'll deadlock.
        if (SetEvent (m_hExitEvent) && 
                (GetThreadId(m_hWriteBackThread) != GetCurrentThreadId())) {
            // Force the write-back thread to our current priority so that we
            // never block here on a lower-priority thread.
            SetThreadPriority (m_hWriteBackThread, GetThreadPriority(GetCurrentThread()));
            WaitForSingleObject (m_hWriteBackThread, INFINITE);
        }
        CloseHandle (m_hWriteBackThread);
        m_hWriteBackThread = NULL;
    }
    if (m_hExitEvent) {
        CloseHandle (m_hExitEvent);
        m_hExitEvent = NULL;
    }
    if (m_hWriteBackEvent) {
        CloseHandle (m_hWriteBackEvent);
        m_hWriteBackEvent = NULL;
    }

    delete m_pDirectoryHash;

    // Now clean up this volume
    DeleteCriticalSection (&m_csUnderlyingIO);
    DeleteCriticalSection (&m_csVolume);
    DeleteCriticalSection (&m_csWriteBack);
}


void
CachedVolume_t::InitWriteBack (
   BOOL WriteBack
   )
{
    // If initialization fails we'll fall back to write-through mode
    if (WriteBack) {
        m_hWriteBackEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
        m_hExitEvent = CreateEvent (NULL, TRUE, FALSE, NULL);
        if (m_hWriteBackEvent && m_hExitEvent) {
            
            m_hWriteBackThread = CreateThread (NULL, 0, ::WriteBackThread,
                                               (LPVOID) this, 0, NULL);
            if (m_hWriteBackThread) {
                DWORD dwPriority;

                m_VolumeFlags |= CACHE_VOLUME_WRITE_BACK;

                // Get the write-back parameters from the registry
                if (!FSDMGR_GetRegistryValue (m_hDisk, TEXT("FileCacheWriteBackTimeout"),
                                              &m_WriteBackTimeout)) {
                    m_WriteBackTimeout = CACHE_DEFAULT_WRITEBACK_TIMEOUT;
                }
                if (!FSDMGR_GetRegistryValue (m_hDisk, TEXT("FileCacheWriteBackPriority256"),
                                              &dwPriority)) {
                    dwPriority = CACHE_DEFAULT_WRITEBACK_PRIORITY;
                }

                CeSetThreadPriority (m_hWriteBackThread, dwPriority);

                DEBUGMSG (ZONE_INIT, (L"CACHEFILT: pVolume=0x%08x in write-back mode, prio=%u timeout=%u\r\n",
                                      this, dwPriority, m_WriteBackTimeout));
            } else {
                DeleteCriticalSection (&m_csWriteBack);
                CloseHandle (m_hExitEvent);
                m_hExitEvent = NULL;
                CloseHandle (m_hWriteBackEvent);
                m_hWriteBackEvent = NULL;
            }
        }
    }
    DEBUGMSG (ZONE_INIT && !(m_VolumeFlags & CACHE_VOLUME_WRITE_BACK),
              (L"CACHEFILT: pVolume=0x%08x in write-through mode\r\n", this));
}


BOOL
CachedVolume_t::Init ()
{
    BOOL fResult = TRUE;
    
    EnterCriticalSection (&g_csVolumeList);
    
    // Check if m_hVolume is unique; if not, cachefilt is already loaded on this disk
    for (CachedVolume_t* pCurVolume = g_pCachedVolumeList;
         pCurVolume && fResult;
         pCurVolume = pCurVolume->m_pNext) {

        if (pCurVolume->m_hVolume == m_hVolume) {
            DEBUGMSG (ZONE_INIT || ZONE_ERROR, (L"CACHEFILT: Loaded twice on the same volume!  Failing second load.\r\n"));
            fResult = FALSE;
        }
    }

    // Add to the volume list
    if (fResult) {
        m_pNext = g_pCachedVolumeList;
        g_pCachedVolumeList = this;
    }

    m_pDirectoryHash = new DirectoryHash_t(this, IsRootVolume());
    if (!m_pDirectoryHash) {
        fResult = FALSE;
    }
    
    LeaveCriticalSection (&g_csVolumeList);

    return fResult;
}

BOOL 
CachedVolume_t::IsRootVolume()
{
    DWORD MountFlags = 0;
    FSDMGR_GetMountFlags(m_hVolume, &MountFlags);
    return (MountFlags & AFS_FLAG_ROOTFS);
}

// Attempts to add the map to the list inside the volume.
// Increment the in-use count of the provided map.
// If any error occurs, returns FALSE.
BOOL
CachedVolume_t::AddMapToList (
    FSSharedFileMap_t* pNewMap
    )
{
    DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:CachedVolume_t::AddMap, pVolume=0x%08x pNewMap=0x%08x\r\n",
                            this, pNewMap));

    DEBUGCHK (OwnVolumeLock ());

    // For now the canonicalized filename is the only file identifier.
    // Later we'll need an open file ID from the underlying FSD.
        
    // Add to the volume's map list.  
    pNewMap->m_pPrev = NULL;
    pNewMap->m_pNext = m_pMapList;
    if (m_pMapList) {
        m_pMapList->m_pPrev = pNewMap;
    }
    m_pMapList = pNewMap;
    m_MapListCount++;

    return TRUE;
}


// Called during CloseHandle when the last reference to a map is closed.
// The map should still have a use-count on the volume until the map is destroyed.
BOOL
CachedVolume_t::RemoveMapFromList (
    FSSharedFileMap_t* pMap
    )
{
    DEBUGCHK (OwnVolumeLock ());

#ifdef DEBUG
    // Verify the map is in the map list
    FSSharedFileMap_t* pCurMap = m_pMapList;
    while (pCurMap) {
        if (pMap == pCurMap) {
            break;
        }
        pCurMap = pCurMap->m_pNext;
    }
    if (!pCurMap) {
        DEBUGMSG (ZONE_ERROR,
                  (L"CACHEFILT:CachedVolume_t::RemoveMap, Not found, pVolume=0x%08x pMap=0x%08x\r\n",
                   this, pMap));

        DEBUGCHK (0);
        return FALSE;
    }    
#endif

    if (pMap == m_pMapList) {
        m_pMapList = pMap->m_pNext;
    }
    if (pMap->m_pNext) {
        pMap->m_pNext->m_pPrev = pMap->m_pPrev;
    }
    if (pMap->m_pPrev) {
        pMap->m_pPrev->m_pNext = pMap->m_pNext;
    }
    
    // Safety null out the map entries to protect against multiple removes
    pMap->m_pNext = NULL;
    pMap->m_pPrev = NULL;
    if (m_MapListCount) {
        m_MapListCount--;
    } else {
        DEBUGCHK (0);  // Should never happen
    }

    DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:CachedVolume_t::RemoveMap, pVolume=0x%08x pMap=0x%08x\r\n",
                            this, pMap));
    return TRUE;
    
}


// Find an open map with the given name and increment its in-use count.
// Must own the volume CS.
FSSharedFileMap_t*
CachedVolume_t::FindMap (
    LPCWSTR pFileName,
    BOOL    Lock
    )
{
    DEBUGCHK (OwnVolumeLock ());
    DEBUGCHK (NULL != pFileName);
    for (FSSharedFileMap_t* pMap = m_pMapList; pMap; pMap = pMap->m_pNext) {
        if (pMap->MatchName (pFileName)) {
            if (Lock) {
                if (!pMap->IncUseCount()) {
                    // Map exists but we couldn't add a reference
                    DEBUGCHK (0);  // Probably a leak
                    SetLastError (ERROR_TOO_MANY_OPEN_FILES);
                    return NULL;
                }
                pMap->IncrementLRUCounter();
            }
            
            DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:CachedVolume_t::FindMap found pMap=0x%08x %s\r\n",
                                    pMap, pFileName));
            return pMap;
        }
    }
    return NULL;
}


BOOL 
CachedVolume_t::CloseMap(
    FSSharedFileMap_t* pMap,
    BOOL FlushMap
    )
{
    BOOL result = TRUE;
    
    if (FlushMap) {
        result = pMap->FlushMap (MAP_FLUSH_WRITEBACK);
    }
    
    // Similar to how RemoveMap removes the map from the volume map list above,
    // PreCloseCaching removes the map from the kernel's map list.
    if (!(pMap->m_SharedMapFlags & CACHE_SHAREDMAP_PRECLOSED)) {
        pMap->PreCloseCaching ();
    }

    // Force asynchronous writeback to finish (either from the writeback
    // thread or the page pool) and let go of any locked files.
    CompleteAsyncWrites ();

    // Now there are no more references to the map
    VERIFY (pMap->DecUseCount ());

    ASSERT (!pMap->AreHandlesInMap());
    delete pMap;

    return result;
}

void 
CachedVolume_t::CloseAllMaps()
{
    FSSharedFileMap_t* pMap = m_pMapList;
    while (pMap) {
        pMap->IncUseCount();
        
        AcquireVolumeLock();
        RemoveMapFromList (pMap);
        ReleaseVolumeLock();

        CloseMap(pMap, FALSE);
        pMap = m_pMapList;
    }
}

BOOL
CachedVolume_t::TrimMapList()
{
    BOOL result = TRUE;
       
    while (m_MapListCount > MAP_LIST_COUNT_THRESHOLD) {

        AcquireVolumeLock();

        FSSharedFileMap_t* pBestMap = NULL;
        DWORD BestLRUCounter = MAXDWORD;

        for (FSSharedFileMap_t* pMap = m_pMapList; pMap; pMap = pMap->m_pNext) {

            // Choose the map if it does not have any open handles and is the
            // least recently used so far.
            if (!pMap->AreHandlesInMap() && 
                (pMap->GetUseCount() == 0) &&
                (BestLRUCounter > pMap->GetLRUCounter())) {

                pBestMap = pMap;
                BestLRUCounter = pMap->GetLRUCounter();
            }   
        }

        if (pBestMap) {            

            // Increment the use count so that it's not deleted yet
            VERIFY (pBestMap->IncUseCount ());
            RemoveMapFromList (pBestMap);
            ReleaseVolumeLock();
            
            result = CloseMap(pBestMap, TRUE);
        } else {
            ReleaseVolumeLock();
            break;
        }
    }

    
    return result;
}

BOOL
CachedVolume_t::DeleteMap(
    LPCWSTR pFileName
    )
{
    BOOL result = TRUE;
    DEBUGCHK (!OwnVolumeLock ());

    AcquireVolumeLock();

    FSSharedFileMap_t* pMap = FindMap(pFileName, TRUE);
    if (!pMap) {
        goto exit;
    }
    
    if (pMap->AreHandlesInMap()) {
        result = FALSE;
        pMap->DecUseCount();
        goto exit;
    } 
    
    RemoveMapFromList (pMap);
    ReleaseVolumeLock();

    CloseMap(pMap, FALSE);
    return TRUE;
    
exit:
    ReleaseVolumeLock();
    return result;
}

BOOL
CachedVolume_t::UpdateMapDirectory(
    LPCWSTR pSrcName, 
    LPCWSTR pDestName
    )
{
    BOOL result = TRUE;
    DEBUGCHK (OwnVolumeLock ());
    
    for (FSSharedFileMap_t* pMap = m_pMapList; pMap; pMap = pMap->m_pNext) {
        if (!pMap->UpdateDirectoryName (pSrcName, pDestName)) {
            result = FALSE;
        }
    }

    return result;
}

#ifdef DEBUG

// Check if the current thread owns the I/O CS from any map other than the
// supplied one.  Or all maps, if target=NULL.  Used for debug verification.
BOOL
CachedVolume_t::OwnAnyOtherMapCS (
    FSSharedFileMap_t* pTargetMap
    )
{
    if (!pTargetMap) {
        // Should not own any map CS, so it's safe to acquire the volume lock
        EnterCriticalSection (&m_csVolume);  // Not using Acquire() since that redundantly checks CS list
    } else {
        // Should already own the volume lock
        DEBUGCHK (OwnVolumeLock ());
    }

    BOOL OwnOtherCS = FALSE;
    for (FSSharedFileMap_t* pMap = m_pMapList; pMap; pMap = pMap->m_pNext) {
        if ((pMap != pTargetMap) && pMap->OwnIOLock ()) {
            DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:CachedVolume_t::OwnAnyOtherMapCS, !! CS ordering violation, holding I/O CS from pMap=0x%08x\r\n",
                                    pMap));
            OwnOtherCS = TRUE;
        }
    }
    
    if (!pTargetMap) {
        LeaveCriticalSection (&m_csVolume);
    }
    
    return OwnOtherCS;
}

#endif // DEBUG


// Lock all of the maps on the volume, so that we can enumerate through
// them later without holding any critical sections.  WriteBackAllMaps
// unlocks the maps and frees the list.
LockedMapList_t*
CachedVolume_t::LockMaps ()
{
    LockedMapList_t* pList = NULL;
    DWORD  MapIndex = 0;

    DEBUGCHK (OwnWriteBackLock ());  // Used to force us to unlock maps
    
    AcquireVolumeLock ();

    FSSharedFileMap_t* pMap = m_pMapList;
    while (pMap) {
        if (pMap->IncUseCount ()) {
            if (!pList || (MapIndex >= LOCKED_MAP_LIST_SIZE)) {
                LockedMapList_t* pNew = new LockedMapList_t;
                DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:CachedVolume_t::LockMaps, Alloc LockedMapList 0x%08x\r\n", pNew));
                if (pNew) {
                    memset (pNew, 0, sizeof(LockedMapList_t));
                    pNew->pNext = pList;
                    pList = pNew;
                    MapIndex = 0;
                } else {
                    pMap->DecUseCount ();
                    
                    // Critically low on memory -- cannot lock any more maps.
                    // Return pList so that we flush whatever we can.
                    break;
                }
            }
            DEBUGCHK (pList && (MapIndex < LOCKED_MAP_LIST_SIZE));
            pList->pMap[MapIndex] = pMap;
            MapIndex++;
        }
        pMap = pMap->m_pNext;
    }

    ReleaseVolumeLock ();
    
    return pList;
}


BOOL
CachedVolume_t::WriteBackAllMaps (
    BOOL StopOnError
    )
{
    // Verify CS ordering rules
    DEBUGCHK (!OwnAnyMapCS ());
    DEBUGCHK (!OwnVolumeLock ());

    AcquireWriteBackLock (); // Used to force us to unlock maps

    // Use the volume CS to lock maps and then release it.
    BOOL result = TRUE;
    LockedMapList_t* pLockedMaps = LockMaps ();
    
    while (pLockedMaps) {
        for (int MapIndex = 0; MapIndex < LOCKED_MAP_LIST_SIZE; MapIndex++) {
            if (pLockedMaps->pMap[MapIndex]) {
                // Flush without holding the volume CS.  
                // If we hit an error with StopOnError, keep iterating but don't flush.
                if ((!StopOnError || result)
                    && !pLockedMaps->pMap[MapIndex]->FlushMap (MAP_FLUSH_WRITEBACK)) {
                    result = FALSE;
                }
                pLockedMaps->pMap[MapIndex]->DecUseCount ();
                pLockedMaps->pMap[MapIndex] = NULL;
            }
        }
        
        LockedMapList_t* pTempList = pLockedMaps;
        pLockedMaps = pLockedMaps->pNext;
        DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:CachedVolume_t::AsyncWriteBackAllMaps, Free LockedMapList 0x%08x\r\n", pTempList));
        delete pTempList;
    }

    ReleaseWriteBackLock ();

    return result;
}


BOOL
CachedVolume_t::AsyncWriteBackAllMaps (
    BOOL StopOnError
    )
{
    HANDLE  hLock = 0;
    LPVOID  pLockData = NULL;
    LRESULT result = FSDMGR_AsyncEnterVolume (m_hVolume, &hLock, &pLockData);
    if (ERROR_SUCCESS == result) {
        result = WriteBackAllMaps (StopOnError);
        FSDMGR_AsyncExitVolume (hLock, pLockData);
    } else {
        DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:CachedVolume_t::AsyncWriteBackAllMaps cannot write back, pVolume=0x%08x FSDMGR result=%u\r\n",
                               this, result));
    }

    return result;
}


// Asynchronously write back map contents.
BOOL
CachedVolume_t::AsyncWriteMap (FSSharedFileMap_t* pMap, DWORD FlushFlags)
{
    BOOL    result = FALSE;
    HANDLE  hLock = 0;
    LPVOID  pLockData = NULL;
    LRESULT FSDMgrResult = FSDMGR_AsyncEnterVolume (m_hVolume, &hLock, &pLockData);
    if (ERROR_SUCCESS == FSDMgrResult) {
        // Verify CS ordering rules
        DEBUGCHK (!OwnAnyMapCS ());
        DEBUGCHK (!OwnVolumeLock ());

        AcquireWriteBackLock (); // Used to force us to unlock maps

        result = pMap->FlushMap (FlushFlags);

        ReleaseWriteBackLock ();

        FSDMGR_AsyncExitVolume (hLock, pLockData);
    } else {
        DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:CachedVolume_t::WriteBackMap cannot write back, pVolume=0x%08x FSDMGR result=%u\r\n",
                               this, FSDMgrResult));
    }

    return result;
}


void
CachedVolume_t::WriteBackThread ()
{
    HANDLE Events[2] = { m_hExitEvent, m_hWriteBackEvent };
    DWORD  WaitResult;

    DEBUGMSG (ZONE_VOLUME || ZONE_INIT,
              (L"CACHEFILT:CachedVolume_t::WriteBackThread starting, pVolume=0x%08x\r\n", this));
    
    do {
        // Wait until a write happens, then wait for a while longer before writing back.
        // This should prevent us from writing back too often and from waking when
        // there's no data to write back.
        WaitResult = WaitForMultipleObjects (2, Events, FALSE, INFINITE);
        if (WaitResult == WAIT_OBJECT_0) {
            // Exit event was signaled.        
            break;
        }

        WaitResult = WaitForSingleObject (m_hExitEvent, m_WriteBackTimeout);
        if (WaitResult == WAIT_OBJECT_0) {
            // Exit event was signaled.        
            break;
        }
        
        DEBUGMSG (ZONE_IO, (L"CACHEFILT:CachedVolume_t::WriteBackThread writing back, pVolume=0x%08x\r\n",
                            this));
        
        AsyncWriteBackAllMaps (FALSE);

        if (CommitTransactionsOnIdle()) {
            CommitTransactions();
        }

    } while (WAIT_TIMEOUT == WaitResult);

    DEBUGMSG (ZONE_VOLUME || ZONE_INIT,
              (L"CACHEFILT:CachedVolume_t::WriteBackThread exit, pVolume=0x%08x\r\n", this));
}


static DWORD WINAPI WriteBackThread (LPVOID pParam)
{
    CachedVolume_t* pVolume = (CachedVolume_t*) pParam;
    pVolume->WriteBackThread ();
    return 0;
}


void
CachedVolume_t::SignalWrite ()
{
    DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:CachedVolume_t::SignalWrite, pVolume=0x%08x\r\n",
                            this));
    
    if (m_hWriteBackEvent && IsBootComplete()) {
        SetEvent (m_hWriteBackEvent);
    }
}


BOOL CachedVolume_t::CompletePendingFlushes ()
{
    DEBUGCHK (!OwnAnyMapCS ());  // Avoid deadlock

    if (m_PendingFlushes) {
        return WriteBackAllMaps (TRUE);
    }

    return TRUE;
}


void
CachedVolume_t::Notify (
    DWORD   dwFlags
    )
{
    static BOOL s_FlushedPool = FALSE;

    switch (dwFlags) {
    case FSNOTIFY_POWER_OFF:
        DEBUGMSG (ZONE_INIT, (L"CACHEFILT:CachedVolume_t::Notify POWER_OFF pVolume=0x%08x\r\n", this));

        // Flush all file data before we power off.  Don't flush more than once if
        // there are multiple volumes being notified.
        if (!s_FlushedPool) {
            FlushViewPool ();
            s_FlushedPool = TRUE;
        }

        // Commit transactions on power-off.
        if (CommitTransactionsOnIdle()) {
            CommitTransactions();
        }

        // NOTENOTE Write-back by the page pool trim thread is prevented during
        // power-off because the trim thread calls FCFILT_AsyncFlushMapping which
        // uses the FSDMGR volume enter/exit helpers.
        
        // Block all reads and writes to the volume
        DEBUGCHK (!OwnUnderlyingIOLock ());  // Not fatal, but odd
        AcquireUnderlyingIOLock ();
        break;

    case FSNOTIFY_POWER_ON:
        DEBUGMSG (ZONE_INIT, (L"CACHEFILT:CachedVolume_t::Notify POWER_ON pVolume=0x%08x\r\n", this));

        // Clear the flush flag for the next power-off
        s_FlushedPool = FALSE;

        // Allow reads and writes again
        DEBUGCHK (OwnUnderlyingIOLock ());
        ReleaseUnderlyingIOLock ();
        break;

    case FSNOTIFY_BOOT_COMPLETE:
        DEBUGMSG (ZONE_INIT, (L"CACHEFILT:CachedVolume_t::Notify BOOT_COMPLETE pVolume=0x%08x\r\n", this));
        gBootCompleted = true;
        SignalWrite();
        break;


    default:
        ;
    }
    
    DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.Notify, hVolume=0x%08x\r\n",
                        m_FilterHook.GetVolumeHandle ()));
    m_FilterHook.Notify (dwFlags);
}


//------------------------------------------------------------------------------
// CACHED VOLUME METHODS: File System Operations
//------------------------------------------------------------------------------

// Local helper function
HANDLE
CachedVolume_t::OpenUnderlyingFile (
    DWORD         Disposition,
    LPCWSTR       pFileName,
    LPSECURITY_ATTRIBUTES pSecurityAttributes,
    DWORD         dwFlagsAndAttributes,
    HANDLE        hTemplateFile,
    BOOL*         pIsFileReadOnly
    )
{
    // Open the file with different settings than the user requested

    // Open the file without modifying it since we won't know until later
    // whether there are any open handles to the file
    if (CREATE_ALWAYS == Disposition) {
        Disposition = OPEN_ALWAYS;
    } else if (TRUNCATE_EXISTING == Disposition) {
        Disposition = OPEN_EXISTING;
    }

    // Tell the underlying FS not to cache since we're doing the caching.
    // Even if the volume is uncached - uncached flag turns off underlying caching
    // too.  There's no other useful meaning with only one underlying handle.
    dwFlagsAndAttributes |= FILE_FLAG_NO_BUFFERING;
    dwFlagsAndAttributes &= ~FILE_FLAG_WRITE_THROUGH;

    DWORD Access = 0;
    if (CACHE_VOLUME_READ_ONLY & m_VolumeFlags) {
        // Read-only is the only option
        Access = GENERIC_READ;
        *pIsFileReadOnly = TRUE;
    } else {
        // Include both generic read and write in the underlying handle,
        // so that the same handle can be used if another handle is opened
        // with different access.
        Access = GENERIC_READ | GENERIC_WRITE;
        *pIsFileReadOnly = FALSE;
    }

    HANDLE hFile = INVALID_HANDLE_VALUE;
    SetLastError (NO_ERROR);
    
    DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.CreateFileW, hVolume=0x%08x\r\n",
                        m_FilterHook.GetVolumeHandle ()));
    
    hFile = m_FilterHook.CreateFileW (
        GetCurrentProcess (),                       // Open on behalf of kernel, not caller
        pFileName,
        Access,                                     // Override access
        FILE_SHARE_READ | FILE_SHARE_WRITE,         // Override sharing
        pSecurityAttributes, Disposition,           
        dwFlagsAndAttributes, hTemplateFile);

    DWORD LastError = GetLastError();
    if ((hFile == INVALID_HANDLE_VALUE) && 
        ((LastError == ERROR_ACCESS_DENIED) || (LastError == ERROR_WRITE_PROTECT)) &&
        (Access & GENERIC_WRITE)) {
        
        // The CreateFile failed with GENERIC_READ and GENERIC_WRITE.  Try
        // only GENERIC_READ to see if the file is read-only.

        SetLastError (NO_ERROR);
        hFile = m_FilterHook.CreateFileW (
            GetCurrentProcess (),                       // Open on behalf of kernel, not caller
            pFileName,
            GENERIC_READ,                                     
            FILE_SHARE_READ | FILE_SHARE_WRITE,         // Override sharing
            pSecurityAttributes, Disposition,           
            dwFlagsAndAttributes, hTemplateFile);

        if (hFile != INVALID_HANDLE_VALUE) {
            *pIsFileReadOnly = TRUE;
        }
    }

    return hFile;
}


HANDLE
CachedVolume_t::CreateFile (
    DWORD   ProcessId,
    LPCWSTR pFileName,          // Already canonicalized by FSDMGR
    DWORD   dwDesiredAccess, 
    DWORD   dwShareMode,
    LPSECURITY_ATTRIBUTES pSecurityAttributes, 
    DWORD   dwCreationDisposition, 
    DWORD   dwFlagsAndAttributes,
    HANDLE  hTemplateFile
    )
{
    PrivateFileHandle_t* pHandle = NULL;
    FSSharedFileMap_t* pMap = NULL;
    FSSharedFileMap_t* pNewMap = NULL;      // Used if we allocate a new map
    HANDLE hFile = INVALID_HANDLE_VALUE;
    BOOL AlreadyExists = TRUE;
    // Check for truncation
    BOOL TruncateFile = (CREATE_ALWAYS == dwCreationDisposition) ||
                        (TRUNCATE_EXISTING == dwCreationDisposition);
        
    DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:CachedVolume_t::CreateFile, pVolume=0x%08x ProcId=0x%08x Name=%s Access=0x%08x ShareMode=0x%08x Create=0x%08x Attribs=0x%08x\r\n",
                            this, ProcessId, pFileName, dwDesiredAccess,
                            dwShareMode, dwCreationDisposition,
                            dwFlagsAndAttributes));

    // Use the volume CS to prevent race conditions or exploits based on changing
    // the filename after opening the file.
    AcquireVolumeLock ();

    // This will bail immediately if the volume is already hashed or we're not
    // supposed to be hashing it.  It has to be called during create file,
    // because the underlying file system may not be accessible when initializing
    // the filter.
    m_pDirectoryHash->HashVolume();

    if (dwCreationDisposition == OPEN_EXISTING ||
        dwCreationDisposition == TRUNCATE_EXISTING) {
        
        DWORD dwError = ERROR_SUCCESS;
        if(!m_pDirectoryHash->FileMightExist( pFileName, &dwError ) ) {
            SetLastError( dwError );
            goto exit;
        }
    }
    
    if (dwShareMode & ~(FILE_SHARE_READ | FILE_SHARE_WRITE)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto exit;
    }

    if (dwDesiredAccess & ~(GENERIC_READ | GENERIC_WRITE)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto exit;
    }
    
    // Find an existing map for this file
    pMap = FindMap (pFileName, TRUE);

    // If a map exists for the file, check the handle sharing
    if (pMap) {
        if (!pMap->CheckHandleSharing(dwShareMode, dwDesiredAccess, dwCreationDisposition)) {
            goto exit;
        }

        if (CREATE_NEW == dwCreationDisposition) {
            DEBUGMSG (ZONE_WARNING, (L"CACHEFILT:CachedVolume_t::CreateFile, !! CREATE_NEW denied to existing file\r\n"));
            SetLastError(ERROR_FILE_EXISTS);
            goto exit;
        }
    }
    else {
        // We will open the file with different settings than the user requested

        BOOL IsReadOnly = FALSE;
        hFile = OpenUnderlyingFile (dwCreationDisposition, pFileName, pSecurityAttributes, 
                                    dwFlagsAndAttributes, hTemplateFile, &IsReadOnly);
        if (INVALID_HANDLE_VALUE == hFile) {
            goto exit;
        }

        AlreadyExists = (GetLastError() == ERROR_ALREADY_EXISTS);

        // Create a new map
        pNewMap = new FSSharedFileMap_t (this, &m_FilterHook, hFile, pFileName, !IsReadOnly);                                         
        DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:CachedVolume_t::CreateFile, Alloc FSSharedFileMap 0x%08x\r\n", pNewMap));
        if (!pNewMap) {
            goto exit;
        }
        
        // The file handle is now owned by the map & closed when the map is deleted
        hFile = INVALID_HANDLE_VALUE;
        if (!pNewMap->Init ()) {
            goto exit;
        }
        
        if (!pNewMap->IncUseCount()) {
            goto exit;
        }
       
        VERIFY (AddMapToList (pNewMap));
        pMap = pNewMap;
    }

    if (dwCreationDisposition == CREATE_NEW ||
        dwCreationDisposition == CREATE_ALWAYS ||
        dwCreationDisposition == OPEN_ALWAYS ) {

        m_pDirectoryHash->AddFilePathToHashMap( pFileName );
    }      

    
    // Check if user got the access they wanted
    if ((GENERIC_WRITE & dwDesiredAccess) && !pMap->IsWritable()) {
        DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:CachedVolume_t::CreateFile, !! Write access denied to read-only file\r\n"));
        SetLastError (ERROR_ACCESS_DENIED);
        goto exit;
    }     
    
    // If this is a new file and created with the read-only attribute, then
    // this handle can write to the file, but subsequent handles cannot.
    if ((dwFlagsAndAttributes & FILE_ATTRIBUTE_READONLY) && pMap->IsWritable() && !AlreadyExists) {
        if ((CREATE_ALWAYS == dwCreationDisposition) ||
            (CREATE_NEW == dwCreationDisposition) ||
            (OPEN_ALWAYS == dwCreationDisposition)) {
            pMap->SetWritable(FALSE);
        }
    }        

    // Check for truncation
    TruncateFile = (CREATE_ALWAYS == dwCreationDisposition) ||
                        (TRUNCATE_EXISTING == dwCreationDisposition);

    if (AlreadyExists && TruncateFile) {
        
        // TRUNCATE_EXISTING requires GENERIC_WRITE but CREATE_ALWAYS does not
        if (!pMap->IsWritable() ||             
            ((TRUNCATE_EXISTING == dwCreationDisposition) && !(GENERIC_WRITE & dwDesiredAccess))) {
            DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:CachedVolume_t::CreateFile, !! Truncate access denied to read-only handle\r\n"));
            SetLastError (ERROR_ACCESS_DENIED);
            goto exit;
        }
    }
        
    
    // Create a new handle object
    pHandle = new PrivateFileHandle_t (pMap, dwShareMode, dwDesiredAccess,
                                       dwFlagsAndAttributes, ProcessId);
    DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:CachedVolume_t::CreateFile, Alloc PrivateFileHandle 0x%08x\r\n", pHandle));
    if (!pHandle) {
        goto exit;
    }
    
    if (!pMap->AddHandle (pHandle)) {
        DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:CachedVolume_t::CreateFile, Free PrivateFileHandle 0x%08x\r\n", pHandle));
        delete pHandle;
        pHandle = NULL;
        goto exit;
    } 

    // If the file handle is cached, prepare the map for caching
    if (!(dwFlagsAndAttributes & FILE_FLAG_NO_BUFFERING)) {

        if (pMap->InitCaching ()) {            
            // Disable having the underlying file system update the write time,
            // since cachefilt will do this instead.
            if ((dwDesiredAccess & GENERIC_WRITE) && IsWriteBack()) {
                pMap->UncachedSetExtendedFlags(CE_FILE_FLAG_DISABLE_WRITE_TIME);
            }
        }
    }


exit:
    if (INVALID_HANDLE_VALUE != hFile) {
        // We opened a file handle but haven't attached it to a map object
        DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.CloseFile, hFile=0x%08x\r\n",
                            hFile));
        m_FilterHook.CloseFile ((DWORD) hFile);
    }
    if (pNewMap) {
        if (pNewMap != pMap) {
            DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:CachedVolume_t::CreateFile, Free FSSharedFileMap 0x%08x\r\n", pNewMap));
            delete pNewMap;
        }
        else if (!pHandle) {
            RemoveMapFromList(pMap);
            pMap->DecUseCount();
            delete pMap;
            pMap = NULL;
        }
    }

    if (pMap) {
        // Remove the refcount that AddMap added
        pMap->DecUseCount ();
    }

    ReleaseVolumeLock ();

    if (pHandle) {
        
        if (AlreadyExists && TruncateFile) {

            // If the file was opened with CREATE_ALWAYS, call SetFileAttributes to 
            // modify the attributes and CreateFile to notify underlying file systems 
            // and filters of the CREATE_ALWAYS call.
            if (CREATE_ALWAYS == dwCreationDisposition) {

                HANDLE hNewHandle = INVALID_HANDLE_VALUE;
                VERIFY (SetFileAttributes (pFileName, LOWORD(dwFlagsAndAttributes)));

                pMap->AcquireIOLock();

                // Update the NK cached map file size.
                ULARGE_INTEGER Offset;
                Offset.QuadPart = 0;
                VERIFY (pMap->SetEndOfFile (Offset));
                
                pMap->UncachedCloseHandle();
                
                hNewHandle = m_FilterHook.CreateFileW (
                    GetCurrentProcess (),                       
                    pFileName,
                    GENERIC_READ | GENERIC_WRITE,                                     
                    FILE_SHARE_READ | FILE_SHARE_WRITE,         
                    NULL, CREATE_ALWAYS,           
                    LOWORD(dwFlagsAndAttributes), NULL);

                pMap->SetUnderlyingFileHandle (hNewHandle);                
                pMap->ReleaseIOLock();

                if (hNewHandle == INVALID_HANDLE_VALUE) {
                    delete pHandle;
                    pHandle = NULL;
                }                
            } else {
                // Perform the truncation.  This is done outside the volume lock to prevent
                // a deadlock with the write-back lock.
                ULARGE_INTEGER Offset;
                Offset.QuadPart = 0;
                VERIFY (pMap->SetEndOfFile (Offset));            
            }
            
        }

        if (!AlreadyExists) {
            if (dwFlagsAndAttributes & FILE_FLAG_WRITE_THROUGH) {
                // for Write-through handles, flush the file so that metadata gets written to disk
                pHandle->m_pSharedMap->FlushFileBuffers();
            }
            else if (CommitTransactionsOnIdle() ) {
                // If the commit transactions is delayed until idle, then signal the idle thread
                // that the file system was modified.
                SignalWrite();
            }
        }

        if (AlreadyExists) {
            SetLastError (ERROR_ALREADY_EXISTS);
        }        
    }

    // Treat as a warning if the file is not found.  Else it's an error.
    DEBUGMSG (!pHandle && ZONE_WARNING,
              (L"CACHEFILT:CachedVolume_t::CreateFile, !! Failed on '%s', error=%u\r\n",
              pFileName ? pFileName : TEXT("NULL"), GetLastError ()));
    return pHandle ? (HANDLE) pHandle : INVALID_HANDLE_VALUE;
}


BOOL
CachedVolume_t::CloseFile (
    FSSharedFileMap_t*   pMap,
    PrivateFileHandle_t* pHandle
    )
{
    DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:CachedVolume_t::CloseFile, pVolume=0x%08x pMap=0x%08x pHandle=0x%08x\r\n",
                            this, pMap, pHandle));

    BOOL result = TRUE;
    BOOL ClosingMap = FALSE;
    
    AcquireVolumeLock ();

    pMap->RemoveHandle (pHandle);
    if (!pMap->AreHandlesInMap() && pMap->IsCloseImmediate()) {
        VERIFY (pMap->IncUseCount());
        RemoveMapFromList (pMap);
        ClosingMap = TRUE;
    }

    ReleaseVolumeLock ();

    if (ClosingMap) {
        result = CloseMap(pMap, TRUE);
    }

    // Trim the list of open maps, if the number of maps has reached
    // the threshold.
    if (result) {
        result = TrimMapList();
    }
    return result;
}


// Internal helper for both MoveFile and DeleteAndRenameFile
BOOL
CachedVolume_t::MoveHelper (
    PCWSTR pSrcName,
    PCWSTR pDestName,
    BOOL   DeleteAndRename  // FALSE: MoveFile, TRUE: DeleteAndRenameFile
    )
{
    FSSharedFileMap_t* pMap = NULL;
    LPCWSTR pCopiedName = NULL;
    BOOL result = FALSE;
    
    DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:CachedVolume_t::MoveHelper, pVolume=0x%08x SrcName=%s DestName=%s Delete=%u\r\n",
                            this, pSrcName, pDestName, DeleteAndRename));

    if (!pSrcName || !pDestName) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // Delete the map of the destination file.  If there are any open handles, 
    // this will fail.
    if (!DeleteMap (pDestName)) {
        DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:CachedVolume_t::MoveHelper, !! MoveFile/DeleteAndRenameFile denied overwrite of open file %s\r\n",
                               pDestName));
        SetLastError (ERROR_SHARING_VIOLATION);
        return FALSE;
    }

    AcquireVolumeLock ();

    // This will bail immediately if the volume is already hashed or we're not
    // supposed to be hashing it.  It has to be called during create file,
    // because the underlying file system may not be accessible when initializing
    // the filter.
    m_pDirectoryHash->HashVolume();

    // Make sure the map doesn't still exist after volume lock is acquired.
    if (FindMap (pDestName, FALSE)) {
        DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:CachedVolume_t::MoveHelper, !! MoveFile/DeleteAndRenameFile denied overwrite of open file %s\r\n",
                               pDestName));
        SetLastError (ERROR_SHARING_VIOLATION);
        goto exit;
    }

    pMap = FindMap (pSrcName, TRUE);
    if (pMap) {
        // The file is open - check sharing rules on moves
        if (!pMap->CheckHandleMoveSharing ()) {
            goto exit;
        }
        // Prepare the new name before moving the file
        pCopiedName = DuplicateFileName (pDestName);
        if (!pCopiedName) {
            goto exit;
        }
    }

    // Flush the map, since MoveFile and DeleteAndRename are often used by
    // applications for transactioning purposes.
    if (pMap) {
        pMap->FlushMap(MAP_FLUSH_WRITEBACK);
    }
    
    if (!DeleteAndRename) {
        DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.MoveFileW, hVolume=0x%08x\r\n",
                            m_FilterHook.GetVolumeHandle ()));
        result = m_FilterHook.MoveFileW (pSrcName, pDestName);
    
    } else {
        DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.DeleteAndRenameFileW, hVolume=0x%08x\r\n",
                            m_FilterHook.GetVolumeHandle ()));
        result = m_FilterHook.DeleteAndRenameFileW (pDestName, pSrcName);
    }

    // If the commit transactions is delayed until idle, then signal the idle thread
    // that the file system was modified.
    if (CommitTransactionsOnIdle()) {
        SignalWrite();
    }

exit:
    if (pMap) {
        // If the file was moved successfully, track the file using the new name
        if (result) {
            pMap->SetName (pCopiedName);
        } else if (pCopiedName) {
            delete [] pCopiedName;
        }
        pMap->DecUseCount ();
    } else {
        // If a directory was moved, then update the file paths of all open maps
        // within the moved directory.  If a file was moved, then this function
        // will do nothing.
        if (result && !DeleteAndRename) {
            UpdateMapDirectory(pSrcName, pDestName);
        }
    }

    // If hashing names, add the new name
    if (result) {
        m_pDirectoryHash->AddFilePathToHashMap(pDestName);
    }
    
    ReleaseVolumeLock ();

    DEBUGMSG (ZONE_ERROR && !result,
              (L"CACHEFILT:CachedVolume_t::MoveHelper, !! Fail, error=%u\r\n",
               GetLastError ()));
    return result;
}


BOOL
CachedVolume_t::DeleteFile (
    LPCWSTR pFileName
    )
{
    BOOL result = FALSE;
    
    DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:CachedVolume_t::DeleteFile, pVolume=0x%08x Name=%s\r\n",
                            this, pFileName));

    if (!pFileName) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // Delete the map of the file.  If there are any open handles, this will fail.
    if (!DeleteMap (pFileName)) {
        DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:CachedVolume_t::DeleteFile, !! DeleteFile denied on open file %s\r\n",
                               pFileName));
        SetLastError (ERROR_SHARING_VIOLATION);
        return FALSE;
    }

    AcquireVolumeLock ();
    
    // Make sure the map doesn't still exist after volume lock is acquired.
    if (FindMap (pFileName, FALSE)) {
        DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:CachedVolume_t::DeleteFile, !! DeleteFile denied on open file %s\r\n",
                               pFileName));
        SetLastError (ERROR_SHARING_VIOLATION);
    } else {
        DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.DeleteFileW, hVolume=0x%08x\r\n",
                            m_FilterHook.GetVolumeHandle ()));
        result = m_FilterHook.DeleteFileW (pFileName);
    }
    
    // If the commit transactions is delayed until idle, then signal the idle thread
    // that the file system was modified.
    if (CommitTransactionsOnIdle()) {
        SignalWrite();
    }

    ReleaseVolumeLock ();
    
    // Treat as a warning if the file is not found.  Else it's an error.
    DEBUGMSG (!result
              && (ZONE_WARNING || (ZONE_ERROR && (ERROR_FILE_NOT_FOUND != GetLastError ()))),
              (L"CACHEFILT:CachedVolume_t::DeleteFile, !! Fail, error=%u\r\n",
               GetLastError ()));
    return result;
}

BOOL
CachedVolume_t::SetFileAttributes (
    PCWSTR  pwsFileName, 
    DWORD   dwFileAttributes
    )
{
    DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.SetFileAttributesW, hVolume=0x%08x\r\n",
                        m_FilterHook.GetVolumeHandle ()));

    BOOL result = m_FilterHook.SetFileAttributesW (pwsFileName, dwFileAttributes);
    
    DEBUGMSG (ZONE_ERROR && !result,
              (L"CACHEFILT:SetFileAttributesW, !! Failed, error=%u\r\n", GetLastError ()));

    if (result) {

        AcquireVolumeLock();

        FSSharedFileMap_t* pMap = FindMap (pwsFileName, TRUE);

        if (pMap) {
            // If the read-only status of the file and map are different, 
            // then update the status of the map
            BOOL IsFileWritable = (dwFileAttributes & FILE_ATTRIBUTE_READONLY) == 0;
            if (IsFileWritable != pMap->IsWritable()) {
                pMap->SetWritable(IsFileWritable);
            } 
            pMap->DecUseCount();
        }

        // If the commit transactions is delayed until idle, then signal the idle thread
        // that the file system was modified.
        if (CommitTransactionsOnIdle()) {
            SignalWrite();
        }

        ReleaseVolumeLock ();
    }

    return result;
    
}

BOOL
CachedVolume_t::GetVolumeInfo (
    FSD_VOLUME_INFO *pInfo
    )
{
    BOOL result = FALSE;
    
    DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:CachedVolume_t::GetVolumeInfo, pVolume=0x%08x\r\n",
                            this));

    DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.GetVolumeInfo, hVolume=0x%08x\r\n",
                        m_FilterHook.GetVolumeHandle ()));
    result = m_FilterHook.GetVolumeInfo (pInfo);

    if (pInfo) {
        // NOTENOTE removing the transactioning flags would mean that mapfiles will
        // not use WriteFileGather.  On one hand the cache may damage transactioning
        // properties.  On the other hand, well-behaved callers would open the file
        // uncached so we'd be okay.

        // The cache adds locking support
        pInfo->dwFlags |= FSD_FLAG_LOCKFILE_SUPPORTED;

        // But only pass-through the WFSC_SUPPORTED flag.  The cache may end up adding
        // scatter-gather support for cached files, but for uncached files the call
        // would go directly through.

        // The cache does not damage 64-bit file size support, nor does it add it,
        // so pass that flag through.

        if (!(m_VolumeFlags & CACHE_VOLUME_UNCACHED)) {
            // Indicate whether or not file caching is supported.
            pInfo->dwFlags |= FSD_FLAG_FILE_CACHE_SUPPORTED;
        }
    }


    DEBUGMSG (ZONE_ERROR && !result,
              (L"CACHEFILT:CachedVolume_t::GetVolumeInfo, !! Fail, error=%u\r\n",
               GetLastError ()));
    return result;
}

void
CachedVolume_t::DisableCommitTransactions ()
{
    DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:CachedVolume_t::DisableCommitTransactions, pVolume=0x%08x\r\n",
                            this));

    if (m_FilterHook.FsIoControl (FSCTL_DISABLE_COMMIT_TRANSACTIONS, NULL, 0,
                                  NULL, 0, NULL, NULL))
    {
        m_VolumeFlags |= CACHE_VOLUME_IDLE_COMMIT_TXN;
    }
}

BOOL
CachedVolume_t::CommitTransactions ()
{
    DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:CachedVolume_t::CommitTransactions, pVolume=0x%08x\r\n",
                            this));

    return m_FilterHook.FsIoControl (FSCTL_FLUSH_BUFFERS, NULL, 0,
                                     NULL, 0, NULL, NULL);
}


BOOL
CachedVolume_t::FsIoControl (
    DWORD   dwIoControlCode,
    PVOID   pInBuf,
    DWORD   nInBufSize,
    PVOID   pOutBuf,
    DWORD   nOutBufSize,
    PDWORD  pBytesReturned
    )
{
    BOOL result = TRUE;
    
    switch (dwIoControlCode) {

    // These IOCTLs are not valid to call on a volume.  Fail them instead of
    // passing them through, because they affect or are affected by caching.
    case IOCTL_FILE_WRITE_GATHER:
    case IOCTL_FILE_READ_SCATTER:
    case FSCTL_SET_EXTENDED_FLAGS:
    case FSCTL_GET_STREAM_INFORMATION:
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;

    // FSDMGR converts these IOCTLs to regular calls to the FCFILT_ entry
    // points, so they should never get here.  Deny them if they happen,
    // because they'd need to go through the cache.
    case IOCTL_FILE_GET_VOLUME_INFO:    // FSCTL_GET_VOLUME_INFO
        DEBUGCHK (0);
        SetLastError (ERROR_NOT_SUPPORTED);
        return FALSE;
    
    case FSCTL_SET_FILE_CACHE:
        // Whether enabling or disabling, we'd have to inform all file handles about
        // the status change.  Not implemented right now.
        DEBUGCHK (0);
        SetLastError (ERROR_NOT_SUPPORTED);
        return FALSE;

    // Back door to flush the volume
    case FSCTL_FLUSH_BUFFERS:
        result = AsyncWriteBackAllMaps (FALSE);
        if (CommitTransactionsOnIdle()) {
            CommitTransactions();
        }
        break;

    default:
        break;
    }

    DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.FsIoControl, hVolume=0x%08x\r\n",
                        m_FilterHook.GetVolumeHandle ()));
    if (!m_FilterHook.FsIoControl (dwIoControlCode, pInBuf, nInBufSize,
                                   pOutBuf, nOutBufSize, pBytesReturned, NULL)) {
        result = FALSE;
    }
    
    // Treat as a warning if this call fails
    DEBUGMSG (ZONE_WARNING && !result,
              (L"CACHEFILT:CachedVolume_t::FsIoControl, !! Failed, error=%u\r\n", GetLastError ()));
    return result;
}
