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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

#include "cachefilt.hpp"


static DWORD WINAPI WriteBackThread (LPVOID pParam);


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
    m_pNext = NULL;
    
    m_hWriteBackThread = NULL;
    m_hWriteBackEvent = NULL;
    m_hExitEvent = NULL;
    m_WriteBackTimeout = 0;
    m_PendingFlushes = 0;
    
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
    }
    
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
    
    // Should not be any open files left
    DEBUGCHK (!m_pMapList);

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
    
    LeaveCriticalSection (&g_csVolumeList);

    return fResult;
}


// Attempts to add the map to the list inside the volume.
// Increment the in-use count of the provided map.
// If any error occurs, returns FALSE.
BOOL
CachedVolume_t::AddMap (
    FSSharedFileMap_t* pNewMap
    )
{
    BOOL fReturn = TRUE;
    
    DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:CachedVolume_t::AddMap, pVolume=0x%08x pNewMap=0x%08x\r\n",
                            this, pNewMap));

    AcquireVolumeLock ();

    // For now the canonicalized filename is the only file identifier.
    // Later we'll need an open file ID from the underlying FSD.
    if (pNewMap->IncUseCount ()) {
        // Add to the volume's map list
        pNewMap->m_pPrev = NULL;
        pNewMap->m_pNext = m_pMapList;
        if (m_pMapList) {
            m_pMapList->m_pPrev = pNewMap;
        }
        m_pMapList = pNewMap;

    } else {
        DEBUGCHK (0);  // Should never happen
        fReturn = FALSE;
    }

    ReleaseVolumeLock ();

    return fReturn;
}


// Called during CloseHandle when the last reference to a map is closed.
// The map should still have a use-count on the volume until the map is destroyed.
BOOL
CachedVolume_t::RemoveMap (
    FSSharedFileMap_t* pMap
    )
{
    DEBUGCHK (OwnVolumeLock ());

    FSSharedFileMap_t* pCurMap = m_pMapList;
    while (pCurMap) {
        if (pMap == pCurMap) {
            if (pCurMap == m_pMapList) {
                m_pMapList = pCurMap->m_pNext;
            }
            if (pCurMap->m_pNext) {
                pCurMap->m_pNext->m_pPrev = pMap->m_pPrev;
            }
            if (pCurMap->m_pPrev) {
                pCurMap->m_pPrev->m_pNext = pMap->m_pNext;
            }
            
            // Safety null out the map entries to protect against multiple removes
            pMap->m_pNext = NULL;
            pMap->m_pPrev = NULL;

            DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:CachedVolume_t::RemoveMap, pVolume=0x%08x pMap=0x%08x\r\n",
                                    this, pMap));
            return TRUE;
        }
        pCurMap = pCurMap->m_pNext;
    }

    DEBUGMSG (ZONE_ERROR,
              (L"CACHEFILT:CachedVolume_t::RemoveMap, Not found, pVolume=0x%08x pMap=0x%08x\r\n",
               this, pMap));
    DEBUGCHK (!pMap->m_pNext && !pMap->m_pPrev);
    
    return FALSE;
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
            if (!Lock || pMap->IncUseCount ()) {
                DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:CachedVolume_t::FindMap found pMap=0x%08x %s\r\n",
                                        pMap, pFileName));
                return pMap;
            } else {
                // Map exists but we couldn't add a reference
                DEBUGCHK (0);  // Probably a leak
                SetLastError (ERROR_TOO_MANY_OPEN_FILES);
                return NULL;
            }
        }
    }
    return NULL;
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
                    && !pLockedMaps->pMap[MapIndex]->FlushFileBuffers (MAP_FLUSH_WRITEBACK)) {
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

        result = pMap->FlushFileBuffers (FlushFlags);

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
    
    if (m_hWriteBackEvent) {
        SetEvent (m_hWriteBackEvent);
    }
}


BOOL CachedVolume_t::CompletePendingFlushes ()
{
    DEBUGCHK (!OwnAnyMapCS ());  // Avoid deadlock
    while (m_PendingFlushes) {
        if (!WriteBackAllMaps (TRUE)) {
            return FALSE;
        }
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
static HANDLE
DoOpenFile (
    FilterHook_t* pFilterHook,
    DWORD         Access,
    DWORD         Disposition,
    LPCWSTR       pFileName,
    LPSECURITY_ATTRIBUTES pSecurityAttributes,
    DWORD         dwFlagsAndAttributes,
    HANDLE        hTemplateFile
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
    dwFlagsAndAttributes |= (FILE_FLAG_NO_BUFFERING|FILE_FLAG_WRITE_THROUGH);
    
    HANDLE hFile = INVALID_HANDLE_VALUE;
    SetLastError (NO_ERROR);

    DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.CreateFileW, hVolume=0x%08x\r\n",
                        pFilterHook->GetVolumeHandle ()));
    hFile = pFilterHook->CreateFileW (
        GetCurrentProcess (),                       // Open on behalf of kernel, not caller
        pFileName,
        Access,                                     
        FILE_SHARE_READ | FILE_SHARE_WRITE,         // Override sharing
        pSecurityAttributes, Disposition,           // Override disposition
        dwFlagsAndAttributes, hTemplateFile);

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
    
    DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:CachedVolume_t::CreateFile, pVolume=0x%08x ProcId=0x%08x Name=%s Access=0x%08x ShareMode=0x%08x Create=0x%08x Attribs=0x%08x\r\n",
                            this, ProcessId, pFileName, dwDesiredAccess,
                            dwShareMode, dwCreationDisposition,
                            dwFlagsAndAttributes));

    // Note: We always re-open the file even if it's already open.  Since we
    // open the file using the current thread's privilege, we'll be allowing
    // the underlying file system to access-check the thread.
    
    

    // Use the volume CS to prevent race conditions or exploits based on changing
    // the filename after opening the file.
    AcquireVolumeLock ();
    
    DWORD Access = dwDesiredAccess;
    if (CACHE_VOLUME_READ_ONLY & m_VolumeFlags) {
        // Read-only is the only option
        Access = GENERIC_READ;
    } else {
        // Include generic read in the underlying handle so that the same handle
        // can be used if another handle is opened with generic read.
        Access |= GENERIC_READ;
    }

    if (dwShareMode & ~(FILE_SHARE_READ | FILE_SHARE_WRITE)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto exit;
    }

    // We will open the file with different settings than the user requested
    hFile = DoOpenFile (&m_FilterHook, Access, dwCreationDisposition, pFileName,
                        pSecurityAttributes, dwFlagsAndAttributes, hTemplateFile);
    if (INVALID_HANDLE_VALUE == hFile) {
        goto exit;
    }

    // Check for illegal truncation
    BOOL TruncateFile = FALSE;
    BOOL  AlreadyExists = (GetLastError() == ERROR_ALREADY_EXISTS);
    
    if (AlreadyExists && ((CREATE_ALWAYS == dwCreationDisposition)
                          || (TRUNCATE_EXISTING == dwCreationDisposition))) {
        if (!(Access & GENERIC_WRITE)               // File is read-only
            // TRUNCATE_EXISTING requires GENERIC_WRITE but CREATE_ALWAYS does not
            || ((TRUNCATE_EXISTING == dwCreationDisposition) && !(GENERIC_WRITE & dwDesiredAccess))) {
            DEBUGMSG (ZONE_WARNING, (L"CACHEFILT:CachedVolume_t::CreateFile, !! Truncate access denied to read-only handle\r\n"));
            SetLastError (ERROR_ACCESS_DENIED);
            goto exit;
        }
        // Else truncation is legal
        TruncateFile = TRUE;
    }


    // Find an existing map for this file
    pMap = FindMap (pFileName, TRUE);

    if (!pMap) {

        // First create a new map
        pNewMap = new FSSharedFileMap_t (this, &m_FilterHook, hFile, pFileName,
                                         (Access & GENERIC_WRITE) ? TRUE : FALSE);
        DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:CachedVolume_t::CreateFile, Alloc FSSharedFileMap 0x%08x\r\n", pNewMap));
        if (!pNewMap) {
            goto exit;
        }
        // The file handle is now owned by the map & closed when the map is deleted
        hFile = INVALID_HANDLE_VALUE;
        if (!pNewMap->Init ()) {
            goto exit;
        }
       
        if (!AddMap (pNewMap)) {
            goto exit;
        }

        pMap = pNewMap;
        
    } else {

        if (!pMap->IsWritable() && (Access & GENERIC_WRITE)) {
            // Replace the existing read-only handle with a read/write handle
            pMap->ReplaceUnderlyingHandle (hFile, TRUE);

            // The file handle is now owned by the map & closed when the map is deleted
            hFile = INVALID_HANDLE_VALUE;
        }

    }
    
    // Create a new handle object
    pHandle = new PrivateFileHandle_t (pMap, dwShareMode, dwDesiredAccess,
                                       dwFlagsAndAttributes, ProcessId);
    DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:CachedVolume_t::CreateFile, Alloc PrivateFileHandle 0x%08x\r\n", pHandle));
    if (pHandle) {
        // AddHandle will return FALSE if the handle conflicted with any existing handles.
        if (!pMap->AddHandle (pHandle, dwCreationDisposition)) {
            DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:CachedVolume_t::CreateFile, Free PrivateFileHandle 0x%08x\r\n", pHandle));
            delete pHandle;
            pHandle = NULL;
        } else {
            // Success!  Truncate if necessary
            if (TruncateFile) {
                ULARGE_INTEGER Offset;
                Offset.QuadPart = 0;
                VERIFY (pMap->SetEndOfFile (Offset));

                // CREATE_ALWAYS modifies the attributes but TRUNCATE_EXISTING does not
                if (CREATE_ALWAYS == dwCreationDisposition) {
                    VERIFY (m_FilterHook.SetFileAttributesW (pFileName, LOWORD(dwFlagsAndAttributes)));
                }
            }
            
            // If the file handle is cached, prepare the map for caching
            if (!(dwFlagsAndAttributes & FILE_FLAG_NO_BUFFERING)) {
                pMap->InitCaching ();
            }

            if (AlreadyExists) {
                SetLastError (ERROR_ALREADY_EXISTS);
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
    if (pNewMap && (pNewMap != pMap)) {
        DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:CachedVolume_t::CreateFile, Free FSSharedFileMap 0x%08x\r\n", pNewMap));
        delete pNewMap;
    }
    if (pMap) {
        // Remove the refcount that AddMap added
        pMap->DecUseCount ();
    }
    ReleaseVolumeLock ();

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

    BOOL ClosingMap = FALSE;
    AcquireVolumeLock ();
    if (pMap->RemoveHandle (pHandle)) {
        // This was the last open handle to the map
        ClosingMap = TRUE;

        // Increment the use count so that it's not deleted yet
        VERIFY (pMap->IncUseCount ());
        VERIFY (RemoveMap (pMap));
    }
    ReleaseVolumeLock ();

    if (ClosingMap) {
        DEBUGCHK (!OwnVolumeLock ());

        // Similar to how RemoveMap removes the map from the volume map list above,
        // PreCloseCaching removes the map from the kernel's map list.
        pMap->PreCloseCaching ();

        // Force asynchronous writeback to finish (either from the writeback
        // thread or the page pool) and let go of any locked files.
        CompleteAsyncWrites ();

        // Now there are no more references to the map
        VERIFY (pMap->DecUseCount ());
    }

    return TRUE;
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

    AcquireVolumeLock ();
    
    // Cannot delete/overwrite open file
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

    if (!DeleteAndRename) {
        DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.MoveFileW, hVolume=0x%08x\r\n",
                            m_FilterHook.GetVolumeHandle ()));
        result = m_FilterHook.MoveFileW (pSrcName, pDestName);
    
    } else {
        DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.DeleteAndRenameFileW, hVolume=0x%08x\r\n",
                            m_FilterHook.GetVolumeHandle ()));
        result = m_FilterHook.DeleteAndRenameFileW (pDestName, pSrcName);
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

    AcquireVolumeLock ();
    
    // Cannot delete open file
    if (FindMap (pFileName, FALSE)) {
        DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:CachedVolume_t::CreateFile, !! DeleteFile denied on open file %s\r\n",
                               pFileName));
        SetLastError (ERROR_SHARING_VIOLATION);
    } else {
        DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.DeleteFileW, hVolume=0x%08x\r\n",
                            m_FilterHook.GetVolumeHandle ()));
        result = m_FilterHook.DeleteFileW (pFileName);
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
        




        // The cache adds locking support
        pInfo->dwFlags |= FSD_FLAG_LOCKFILE_SUPPORTED;

        // But only pass-through the WFSC_SUPPORTED flag.  The cache may end up adding
        // scatter-gather support for cached files, but for uncached files the call
        // would go directly through.

        // The cache does not damage 64-bit file size support, nor does it add it,
        // so pass that flag through.
    }


    DEBUGMSG (ZONE_ERROR && !result,
              (L"CACHEFILT:CachedVolume_t::GetVolumeInfo, !! Fail, error=%u\r\n",
               GetLastError ()));
    return result;
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
