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
#include <pm.h>
#include <string.hxx>

#include <snapboot.h>
#include "FileMapCollection.hpp"

#define AVERAGE_FILENAME_LEN   82  //(Average filename len observed during 5 days of intensive selfhost)
#define AVERAGE_MAPSTRUCT_SIZE (sizeof(FSSharedFileMap_t)+(sizeof(WCHAR) * AVERAGE_FILENAME_LEN))

//Regular map list trimming occurs on User Idle.
//We have this upper limit for the platforms that never reach such power state (such as VCEPC and some other
//test platforms). upper limit of 800K worth of average size maps in case platform never goes user idle.
#define MAPLIST_TRIM_UPPER_THRESHOLD ((800 * 1024) / AVERAGE_MAPSTRUCT_SIZE)

//When we trim, we don't want to trim all the way to 0.. that's not needed
//When we hit user idle we want trim down only if we crossed the trim stop line
//(if not, dont even bother)
#define MAPLIST_TRIM_ALLOWANCE ((100 * 1024) / AVERAGE_MAPSTRUCT_SIZE)

//These compile asserts are in case the structures start growing and our limits become maybe to small.
//currently we want the upper to be at least 1500 maps (or more) based on size and the allowance to be at least 200 maps
//If these asserts ever fire you need to just be aware that the limits are not longer allowing 1500 and 200.
C_ASSERT(MAPLIST_TRIM_UPPER_THRESHOLD >= 1500);
C_ASSERT(MAPLIST_TRIM_ALLOWANCE >= 200);

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
    InitPageTree ();
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

    m_pNext = NULL;

    m_hWriteBackThread = NULL;
    m_hWriteBackEvent = NULL;
    m_hExitEvent = NULL;
    m_WriteBackTimeout = 0;
    m_fCancelFlush = FALSE;
    m_pDirectoryHash = new DirectoryHash_t(this, TRUE);
    m_pMapCollection = new FileMapCollection();

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

    m_pNext = NULL;
    
    m_hWriteBackThread = NULL;
    m_hWriteBackEvent = NULL;
    m_hExitEvent = NULL;
    m_WriteBackTimeout = 0;
    m_pDirectoryHash = NULL;
    m_pMapCollection = NULL;
    m_fCancelFlush = FALSE;

    InitializeCriticalSection (&m_csVolume);
    InitializeCriticalSection (&m_csMove);
    
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
    
    if (m_pMapCollection != NULL)
    {
        m_pMapCollection->CloseAllMapFiles();
    }

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
    delete m_pMapCollection;

    // Now clean up this volume
    DeleteCriticalSection (&m_csVolume);
    DeleteCriticalSection (&m_csMove);

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
                DEBUGMSG(ZONE_ERROR, (L"CACHEFILT: ERROR! Failed to initialize write-back; pVolume=0x%08x; error-%u\r\n",
                                      this, GetLastError()));
                DEBUGCHK(0);
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
    // fail initialization if the volume is uncached - no reason to put a cache filter on a uncached volume.
    BOOL fResult = !(m_VolumeFlags & CACHE_VOLUME_UNCACHED);

    if (fResult) {
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

        if (fResult)
        {
            m_pDirectoryHash = new DirectoryHash_t(this, IsRootVolume());
            if (!m_pDirectoryHash) {
                fResult = FALSE;
            }
        }

        if (fResult)
        {
            m_pMapCollection = new FileMapCollection();
            if (m_pMapCollection)
            {
                //the collection will use the same volume lock in the cached-volume
                //to protect the collection operations
                m_pMapCollection->Initialize(&m_csVolume, DirectoryHash_t::HashFileName);
                fResult = TRUE;
            }
            else
            {
                fResult = FALSE;
            }
        }

        // Add to the volume list
        if (fResult)
        {
            m_pNext = g_pCachedVolumeList;
            g_pCachedVolumeList = this;

            // in case cache filter isn't loaded on boot (e.g. plug-in USB disk), we'll never get the notification from 
            // filesystem to notify boot complete. Add this check to make sure write-back thread got signaled in this case.
            if (IsNamedEventSignaled (L"SYSTEM/SystemStarted", 0)) {
                m_VolumeFlags |= CACHE_VOLUME_BOOT_COMPLETE;
            }
        }

        
        LeaveCriticalSection (&g_csVolumeList);
    }

    return fResult;
}

BOOL 
CachedVolume_t::IsRootVolume()
{
    DWORD MountFlags = 0;
    FSDMGR_GetMountFlags(m_hVolume, &MountFlags);
    return (MountFlags & AFS_FLAG_ROOTFS);
}

void CachedVolume_t::IncrementUnreferencedMapCount()
{
    m_pMapCollection->IncrementUnreferencedMapCount();
}

void CachedVolume_t::DecrementUnreferencedMapCount()
{
    m_pMapCollection->DecrementUnreferencedMapCount();
}

BOOL 
CachedVolume_t::CloseMap(
    FSSharedFileMap_t* pMap,
    BOOL FlushMap
    )
{
    DEBUGMSG(ZONE_WRITEBACK, (L"CACHEFILT:CachedVolume_t::CloseMap closing %s\r\n", pMap->m_pFileName));    
    return FSSharedFileMap_t::CloseMap(pMap, FlushMap);
}

BOOL
CachedVolume_t::TrimMapList()
{
    BOOL result = TRUE;

    volatile LONG unreferencedMapCount = m_pMapCollection->GetUnreferencedFileMapCount();

    BOOL mustTrim = FALSE;

    if (unreferencedMapCount > MAPLIST_TRIM_UPPER_THRESHOLD)
    {
        //if we have crossed the upper limit (protection), we need to trim
        mustTrim = TRUE;
    }
    else if (IsDeviceIdle() && (unreferencedMapCount > MAPLIST_TRIM_ALLOWANCE))
    {
        //if we are user-idle and there is more unferenced maps than the allowance, we need to trim
        mustTrim = TRUE;
    }

    if (mustTrim)
    {
        DEBUGMSG (ZONE_WRITEBACK, (L"CACHEFILT:CachedVolume_t::WriteBackThread trimming map list. pVolume=0x%08x\r\n", this));
        
        DWORD PrevPriority = GetThreadPriority (GetCurrentThread());
        if (PrevPriority > THREAD_PRIORITY_NORMAL) 
        {
            SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_NORMAL);
        }

        //call Close only on unreferenced maps.
        m_pMapCollection->CloseUnreferencedMapFiles(unreferencedMapCount - MAPLIST_TRIM_ALLOWANCE);

        if (PrevPriority > THREAD_PRIORITY_NORMAL) 
        {
            SetThreadPriority (GetCurrentThread(), PrevPriority);
        }

        DEBUGMSG (ZONE_WRITEBACK, (L"CACHEFILT:CachedVolume_t::WriteBackThread finished trimming map list. pVolume=0x%08x\r\n", this));    
    }
    
    return result;
}

BOOL
CachedVolume_t::DeleteMap(
    FSSharedFileMap_t* pMap,
    BOOL FlushMap
    )
{
    LRESULT result = ERROR_SUCCESS;
    DEBUGCHK (pMap->OwnMapLock ());
    
    if (pMap->AreHandlesInMap()) {
        result = ERROR_SHARING_VIOLATION;
        goto exit;
    }
    
    if (FlushMap) {
        if (!pMap->FlushMap()) {
            result = ERROR_WRITE_FAULT;
            goto exit;
        }
    } else {
        const ULARGE_INTEGER liZero = {0};
        VERIFY (pMap->SetCachedFileSize (liZero, TRUE));
    }
        
    // It is possible that the WriteBackThread still has a reference to the
    // map object, which prevents it from being deallocated. Explictly close
    // the handle to ensure the file can be deleted.  This is safe because
    // the pages have already been either flushed or deallocated.
    pMap->UncachedCloseHandle();
        
exit:
    if (result != ERROR_SUCCESS) {
        SetLastError (result);
    }
    return (result == ERROR_SUCCESS);
}

FSSharedFileMap_t*
CachedVolume_t::AcquireMapLock (
    LPCWSTR pFileName
    )
{
    FSSharedFileMap_t* pMap = NULL;
    
    for (;;) {
        AcquireVolumeLock();

        // Check if the map is already in the map list.  If so, the use count will be incremented.
        pMap = m_pMapCollection->FindMap (pFileName, TRUE);
        
        if (!pMap) {
            
            // If a map doesn't exist, create an empty map object to track this file, even
            // for path-based APIs. The CS of the map object will be used to serialize
            // access to the file.
            pMap = new FSSharedFileMap_t (this, &m_FilterHook, pFileName);                                         
            DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:CachedVolume_t::AcquireMapLock, Alloc FSSharedFileMap 0x%08x\r\n", pMap));
           
            if (pMap && !pMap->IncUseCount()) {
                delete pMap;
                pMap = NULL;
            }

            if (pMap) {
                VERIFY (m_pMapCollection->AddMap(pMap));
            }

        }
        ReleaseVolumeLock();

        if (!pMap) {
            goto exit;
        }

        pMap->AcquireMapLock();
        if (!pMap->IsInvalid() && pMap->MatchName(pFileName)) {
            // If this is still a valid map after acquiring the lock and the
            // name still matches, then return this map with the lock still held.
            break;
        }
        pMap->ReleaseMapLock();
        pMap->DecUseCount();
    }
    
exit:
    return pMap;
}

void 
CachedVolume_t::ReleaseMapLock(
    FSSharedFileMap_t* pMap,
    BOOL fRemoveMapIfClosed
    )
{
    if (pMap) {
        if (fRemoveMapIfClosed) {
            AcquireVolumeLock();
            if (!pMap->IsFileOpen()) {
                pMap->SetFlags (CACHE_SHAREDMAP_INVALID);
                VERIFY (m_pMapCollection->RemoveMap(pMap));
            }
            ReleaseVolumeLock();
        }
        pMap->ReleaseMapLock();
        pMap->DecUseCount();
    }
}

BOOL
CachedVolume_t::IsDeviceIdle ()
{
    BOOL DeviceIdle = TRUE;

    WCHAR szState[128] = L"";
    DWORD PowerState = 0;

    // Get the system power state. The string, szState, is not used but needs to be passed for the 
    // call to succeed. NOTE: this is a very inexpensive call into power manager. It simply grabs
    // and resturns some internal state variables, and does not call-out to the file system or registry.
    if ((NO_ERROR == GetSystemPowerState (szState, _countof(szState), &PowerState))) {

        PowerState = POWER_STATE(PowerState);
        if (!((POWER_STATE_IDLE | POWER_STATE_USERIDLE) & PowerState)) {
            DeviceIdle = FALSE;
        }
    }

    return DeviceIdle;
}

BOOL
CachedVolume_t::AsyncWriteBackAllMaps (
    BOOL fStopOnIO,
    BOOL fForceFlush
    )
{
    HANDLE  hLock = 0;
    LPVOID  pLockData = NULL;
    LRESULT result = FSDMGR_AsyncEnterVolume (m_hVolume, &hLock, &pLockData);
    if (ERROR_SUCCESS == result) {
        result = m_pMapCollection->WriteBackAllMaps (fStopOnIO, fForceFlush);
        FSDMGR_AsyncExitVolume (hLock, pLockData);
    } else {
        DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:CachedVolume_t::AsyncWriteBackAllMaps cannot write back, pVolume=0x%08x FSDMGR result=%u\r\n",
                               this, result));
    }

    return result;
}


void
CachedVolume_t::WriteBackThread ()
{
    DWORD  WaitResult;

    DEBUGMSG (ZONE_VOLUME || ZONE_INIT,
              (L"CACHEFILT:CachedVolume_t::WriteBackThread starting, pVolume=0x%08x\r\n", this));
    
    HANDLE rgWaitStage1Events[] = { m_hExitEvent, m_hWriteBackEvent };
    HANDLE rgWaitStage2Events[] = { m_hExitEvent };

    BOOL fFlushFailed = FALSE;

    for (;;) {

        // Wait until a write happens, then wait for the system to go idle before writing back.
        // This should prevent us from writing back too often and from waking when there's no
        // no data to write back.
        WaitResult = WaitForMultipleObjects (_countof(rgWaitStage1Events), rgWaitStage1Events, FALSE, INFINITE);
        if ((WaitResult == WAIT_OBJECT_0) || (WaitResult == WAIT_FAILED)) {
            // Exit event was signaled.
            goto exit;
        }

        // Start by waiting for the full flush timeout.
        DWORD dwTimeout = m_WriteBackTimeout;

        do {

            // At least one write has occurred because m_hWriteBackEvent was signalled. Unless
            // the pool is full, wait here for the write-back timeout before proceeding.
            WaitResult = WaitForMultipleObjects (_countof(rgWaitStage2Events), rgWaitStage2Events, FALSE, dwTimeout);
            if ((WaitResult == WAIT_OBJECT_0) || (WaitResult == WAIT_FAILED)) {
                // Exit event was signaled.
                goto exit;
            }

            DEBUGMSG (ZONE_WRITEBACK, (L"CACHEFILT:CachedVolume_t::WriteBackThread beginning writing back, pVolume=0x%08x\r\n",
                            this));

            m_fCancelFlush = FALSE;

            // call WaitForSingleObject to rest the writeback event, in case it was signaled again
            WaitForSingleObject (m_hWriteBackEvent, 0);

            // AsyncWriteBackAllMaps will fail if simultaneous reads or writes occur on the
            // volume. If it fails in this case, loop again and wait for the
            // write-back timeout before retrying.
            fFlushFailed = !AsyncWriteBackAllMaps (TRUE);

            // Use a smaller timeout for secondary flush iterations when we've been
            // interrupted.
            dwTimeout = min(m_WriteBackTimeout, CACHE_DEFAULT_WRITEBACK_BACKOFF);

        } while (fFlushFailed);

        // Trim the list of open maps, if necessary
        TrimMapList();

        // Commit transactions on the volume.
        if (CommitTransactionsOnIdle()) {
            DEBUGMSG (ZONE_WRITEBACK, (L"CACHEFILT:CachedVolume_t::WriteBackThread committing transactions, pVolume=0x%08x\r\n", this));
            CommitTransactions();
        }

        DEBUGMSG (ZONE_WRITEBACK, (L"CACHEFILT:CachedVolume_t::WriteBackThread write-back complete, pVolume=0x%08x\r\n", this));
    }
 
exit:

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

void
CachedVolume_t::Notify (
    DWORD   dwFlags
    )
{
    TSnapState snapState = GetSnapshotState ();

    switch (dwFlags) {
    case FSNOTIFY_POWER_OFF:
        DEBUGMSG (ZONE_INIT, (L"CACHEFILT:CachedVolume_t::Notify POWER_OFF pVolume=0x%08x\r\n", this));

        // Flush all file data before we power off.  Don't flush more than once if
        // there are multiple volumes being notified.
        SetFsPowerOffFlag ();

        if (eSnapPass2 == snapState) {
            m_pMapCollection->CloseUnreferencedMapFiles ();
        }
        m_pMapCollection->WriteBackAllMaps(FALSE, TRUE);

        // Commit transactions on power-off.
        if (CommitTransactionsOnIdle()) {
            CommitTransactions();
        }        
        ClearFsPowerOffFlag ();

        // NOTENOTE Write-back by the page pool trim thread is prevented during
        // power-off because the trim thread calls FCFILT_AsyncFlushMapping which
        // uses the FSDMGR volume enter/exit helpers.
        
        break;

    case FSNOTIFY_POWER_ON:
        // when powering on, make sure the underlying volume had gone through power on sequence
        // before we continue.
        m_FilterHook.Notify (dwFlags);
        if (eSnapBoot == snapState) {
            m_pDirectoryHash->ReHashVolume ();
        }
        DEBUGMSG (ZONE_INIT, (L"CACHEFILT:CachedVolume_t::Notify POWER_ON pVolume=0x%08x\r\n", this));

        // NOTE: do not use break here as it'll call into filterhook again.
        return;

    case FSNOTIFY_BOOT_COMPLETE:
        DEBUGMSG (ZONE_INIT, (L"CACHEFILT:CachedVolume_t::Notify BOOT_COMPLETE pVolume=0x%08x\r\n", this));
        m_VolumeFlags |= CACHE_VOLUME_BOOT_COMPLETE;
        SignalWrite();
        break;

    case FSNOTIFY_CLEAN_BOOT:
        DEBUGMSG (ZONE_INIT, (L"CACHEFILT:CachedVolume_t::Notify CLEAN_BOOT pVolume=0x%08x\r\n", this));
        m_VolumeFlags |= CACHE_VOLUME_CLEAN_BOOT;
        break;

    case FSNOTIFY_DEVICE_IDLE:
        // NOTE: This must be a fast, non-blocking operation: we cannot block device manager on its
        // state transition path.
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
    HANDLE hFile = INVALID_HANDLE_VALUE;
    BOOL AlreadyExists = TRUE;
    // Check for truncation
    BOOL TruncateFile = (CREATE_ALWAYS == dwCreationDisposition) ||
                        (TRUNCATE_EXISTING == dwCreationDisposition);

    DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:CachedVolume_t::CreateFile, pVolume=0x%08x ProcId=0x%08x Name=%s Access=0x%08x ShareMode=0x%08x Create=0x%08x Attribs=0x%08x\r\n",
                            this, ProcessId, pFileName, dwDesiredAccess,
                            dwShareMode, dwCreationDisposition,
                            dwFlagsAndAttributes));

    // mask out modes not supported in mobile
    dwShareMode &= ~FILE_SHARE_DELETE;

    // Use the map CS to prevent race conditions or exploits based on changing
    // the filename after opening the file.
    pMap = AcquireMapLock (pFileName);
    if (!pMap) {
        goto exit;
    }
    
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
    
    if (pMap->IsFileOpen()) {
    
        if (CREATE_NEW == dwCreationDisposition) {
            DEBUGMSG (ZONE_WARNING, (L"CACHEFILT:CachedVolume_t::CreateFile, !! CREATE_NEW denied to existing file\r\n"));
            SetLastError(ERROR_FILE_EXISTS);
            goto exit;
        }

        // If a map exists for the file, check the handle sharing
        if (!pMap->CheckHandleSharing(dwShareMode, dwDesiredAccess, dwCreationDisposition)) {
            goto exit;
        }

        if ((dwFlagsAndAttributes & FILE_FLAG_NO_BUFFERING) && pMap->DoesPageTreeExist()) {

            // If there is an existing page tree, then clean up the page tree. Only do this 
            // if there are no other handles to prevent concurrent access.  Also, don't 
            // close if the page tree is dirty to avoid a potentially expensive flush.
            if (!pMap->AreHandlesInMap() && !pMap->IsDirty()) {
                pMap->CleanupCaching(TRUE);             
            }
        }        
    } else {
    
        // We will open the file with different settings than the user requested
        BOOL IsReadOnly = FALSE;
        hFile = OpenUnderlyingFile (dwCreationDisposition, pFileName, pSecurityAttributes, 
                                    dwFlagsAndAttributes, hTemplateFile, &IsReadOnly);
        if (INVALID_HANDLE_VALUE == hFile) {
            goto exit;
        }

        AlreadyExists = (GetLastError() == ERROR_ALREADY_EXISTS);
        
        // The file handle is now owned by the map & closed when the map is deleted
        if (!pMap->Init (hFile, !IsReadOnly)) {
            goto exit;
        }               
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

    // if delete on close is specified, add that to the map (existing or new)
    if (dwFlagsAndAttributes & FILE_FLAG_DELETE_ON_CLOSE) {
        pMap->SetDeleteOnClose();
    }
        
    if (AlreadyExists && TruncateFile) {

        // If the file was opened with CREATE_ALWAYS, call SetFileAttributes to 
        // modify the attributes.
        if (CREATE_ALWAYS == dwCreationDisposition) {
            // on CreateFile, we always clear NORMAL and set ARCHIVE bit.
            DWORD dwAttributes = LOWORD(dwFlagsAndAttributes);
            dwAttributes &= ~FILE_ATTRIBUTE_NORMAL;
            dwAttributes |= FILE_ATTRIBUTE_ARCHIVE;
            if (!SetFileAttributes (pFileName, dwAttributes)) {
                DEBUGCHK(0);
            }
        } 

        // Perform the truncation.  
        ULARGE_INTEGER Offset;
        Offset.QuadPart = 0;
        if (!pMap->SetEndOfFile (Offset)) {
            DEBUGCHK(0);
        }
    }

    // If the commit transactions is delayed until idle, then signal the idle thread
    // that the file system was modified.
    if (CommitTransactionsOnIdle() && !AlreadyExists) {
        SignalWrite();
    }

    if (AlreadyExists) {
        SetLastError (ERROR_ALREADY_EXISTS);
    }        

exit:
    // Release lock and remove map from list if create failed.
    ReleaseMapLock (pMap, !pHandle);
    
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
    WCHAR FileName[MAX_PATH];

    pMap->IncUseCount();
    pMap->AcquireMapLock();

    pMap->RemoveHandle (pHandle);
    if (!pMap->AreHandlesInMap() && pMap->IsDeleteOnClose()) {
        VERIFY (SUCCEEDED(StringCchCopyW(FileName, MAX_PATH, pMap->m_pFileName)));
        result = DeleteFile(FileName);
    }
    
    pMap->ReleaseMapLock();
    pMap->DecUseCount();

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
    FSSharedFileMap_t* pSrcMap = NULL;
    FSSharedFileMap_t* pDestMap = NULL;
    LPCWSTR pCopiedName = NULL;
    BOOL result = FALSE;
    
    DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:CachedVolume_t::MoveHelper, pVolume=0x%08x SrcName=%s DestName=%s Delete=%u\r\n",
                            this, pSrcName, pDestName, DeleteAndRename));

    if (!pSrcName || !pDestName) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // For MoveFile, if the source and destination match, then pass
    // the call to the underlying filesystem to allow for case changes.
    // For DeleteAndRename, this is a no-op.
    if (_wcsnicmp (pSrcName, pDestName, MAX_PATH) == 0) {
        if (!DeleteAndRename) {
            return m_FilterHook.MoveFileW (pSrcName, pDestName);        
        } else {
            return m_FilterHook.DeleteAndRenameFileW (pDestName, pSrcName);
        }        
    }

    if (DeleteAndRename) {
        pDestMap = m_pMapCollection->FindMap(pDestName, TRUE);
        if (pDestMap) {
            if (pDestMap->AreHandlesInMap()) {
                DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:CachedVolume_t::MoveHelper, !! MoveFile/DeleteAndRenameFile denied overwrite of open file %s\r\n", pDestName));
                pDestMap->DecUseCount();
                SetLastError (ERROR_SHARING_VIOLATION);
                return FALSE;
            }

            // Pre-flush the file outside the map lock to avoid an expensive flush
            // in DeleteMap with the map lock.
            if (!pDestMap->FlushMap()) {
                pDestMap->DecUseCount();
                SetLastError (ERROR_WRITE_FAULT);
                return FALSE;
            }
            pDestMap->DecUseCount();
            pDestMap = NULL;
        }
    }

    // The move lock is used to prevent map lock ordering issues between
    // multiple concurrent moves (e.g. A->B and B->A)
    AcquireMoveLock();    

    pSrcMap = AcquireMapLock(pSrcName);
    if (!pSrcMap) {
        goto exit;
    }

    pDestMap = AcquireMapLock(pDestName);
    if (!pDestMap) {
        goto exit;
    }

    // This will bail immediately if the volume is already hashed or we're not
    // supposed to be hashing it.  It has to be called during create file,
    // because the underlying file system may not be accessible when initializing
    // the filter.
    m_pDirectoryHash->HashVolume();

    if (DeleteAndRename) {
        if (!DeleteMap (pDestMap, TRUE)) {
            DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:CachedVolume_t::MoveHelper, !! MoveFile/DeleteAndRenameFile failed to delete existing file %s\r\n",
                                   pDestName));
            goto exit;
        }
    }
    else {
        // For MoveFile, the destination file should not exist. 
        if (pDestMap->IsFileOpen()) {
            DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:CachedVolume_t::MoveHelper, !! MoveFile denied to existing file %s\r\n",
                                   pDestName));
            SetLastError (ERROR_ALREADY_EXISTS);
            goto exit;
        }
    }

    if (pSrcMap->IsFileOpen()) {
        // The file is open - check sharing rules on moves
        if (!pSrcMap->CheckHandleMoveSharing ()) {
            goto exit;
        }
    }
    
    // Prepare the new name before moving the file
    pCopiedName = DuplicateFileName (pDestName);
    if (!pCopiedName) {
        goto exit;
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
    if (!result) {
        goto exit;
    }
    
    // If the commit transactions is delayed until idle, then signal the idle thread
    // that the file system was modified.
    if (CommitTransactionsOnIdle()) {
        SignalWrite();
    }

    AcquireVolumeLock();

    // Release lock and remove old destination map from list.
    DEBUGCHK (!pDestMap->IsFileOpen());
    ReleaseMapLock (pDestMap, TRUE);
    pDestMap = NULL;

    // Remove source map from the list
    VERIFY(m_pMapCollection->RemoveMap(pSrcMap));

    // Update the source map with the new name.
    pSrcMap->SetName (pCopiedName);
    pCopiedName = NULL;
   
    // This AddMap cannot fail because we should have an empty slot on the bucket due to pDestMap.
    VERIFY(m_pMapCollection->AddMap(pSrcMap));

    ReleaseVolumeLock();

    // If a directory was moved, then update the file paths of all open maps
    // within the moved directory.  If a file was moved, then this function
    // will do nothing.
    if (!DeleteAndRename && !pSrcMap->IsFileOpen()) {
        m_pMapCollection->UpdateMapDirectory(pSrcName, pDestName);
    }

    // If hashing names, add the new name
    m_pDirectoryHash->AddFilePathToHashMap(pDestName);
    
exit:       
    if (pCopiedName) 
    {
        delete [] pCopiedName;
    }

    // pDestMap is only non-NULL on failure.
    ReleaseMapLock (pDestMap, TRUE);

    // Release source lock and remove source map from list only on failure.
    ReleaseMapLock (pSrcMap, !result);

    ReleaseMoveLock();

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
    FSSharedFileMap_t* pMap = NULL;
    BOOL fRemoveMapIfClosed = FALSE;
    
    DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:CachedVolume_t::DeleteFile, pVolume=0x%08x Name=%s\r\n",
                            this, pFileName));

    if (!pFileName) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pMap = AcquireMapLock (pFileName);
    if (!pMap) {
        goto exit;
    }

    // Delete the map of the file.  If there are any open handles, this will fail.
    if (!DeleteMap (pMap, FALSE)) {
        DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:CachedVolume_t::DeleteFile, !! DeleteFile denied on open file %s\r\n",
                               pFileName));
        goto exit;
    }

    // once DeleteMap succeeded, we need to remove the mapping from the collection regardless of the result
    fRemoveMapIfClosed = TRUE;
    
    DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.DeleteFileW, hVolume=0x%08x\r\n",
                        m_FilterHook.GetVolumeHandle ()));
    result = m_FilterHook.DeleteFileW (pFileName);

    // If the commit transactions is delayed until idle, then signal the idle thread
    // that the file system was modified.
    if (CommitTransactionsOnIdle()) {
        SignalWrite();
    }

exit:
    // Release lock and remove map from list if delete succeeded.
    ReleaseMapLock (pMap, fRemoveMapIfClosed);
    
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
    FSSharedFileMap_t* pMap = NULL;
    BOOL result = FALSE;
    
    DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.SetFileAttributesW, hVolume=0x%08x\r\n",
                        m_FilterHook.GetVolumeHandle ()));

    pMap = AcquireMapLock (pwsFileName);
    if (!pMap) {
        goto exit;
    }

    result = m_FilterHook.SetFileAttributesW (pwsFileName, dwFileAttributes);
    
    DEBUGMSG (ZONE_ERROR && !result,
              (L"CACHEFILT:SetFileAttributesW, !! Failed, error=%u\r\n", GetLastError ()));

    if (result) {
        if (pMap->IsFileOpen()) {
            // If the read-only status of the file and map are different, 
            // then update the status of the map
            BOOL IsFileWritable = (dwFileAttributes & FILE_ATTRIBUTE_READONLY) == 0;
            if (IsFileWritable != pMap->IsWritable()) {
                pMap->SetWritable(IsFileWritable);
            } 
        }

        // If the commit transactions is delayed until idle, then signal the idle thread
        // that the file system was modified.
        if (CommitTransactionsOnIdle()) {
            SignalWrite();
        }        
    }

exit:
    ReleaseMapLock (pMap, !result);
    return result;
    
}

DWORD CachedVolume_t::GetFileAttributes (
    PCWSTR pwsFileName
    )
{
    FSSharedFileMap_t* pMap = NULL;
    DWORD attribs = INVALID_FILE_ATTRIBUTES;
    
    pMap = AcquireMapLock (pwsFileName);
    if (!pMap) {
        goto exit;
    }

    //Try to get the attributes from the cache first 
    if (pMap->IsFileOpen())
    { 
        //If we have the map for that file in the cache, then we can get the attributes using the FilePseudoHandle
        //Which is cheaper than the path based way.
        BY_HANDLE_FILE_INFORMATION fileInfo = {0};
        if (pMap->UncachedGetFileInformationByHandle(&fileInfo))
        {
            attribs = fileInfo.dwFileAttributes;
        }
        else
        {
            //If for any reason the call fails, default back to using the path based API
            attribs = INVALID_FILE_ATTRIBUTES;
            DEBUGMSG (ZONE_VOLUME, (L"CachedVolume_t::GetFileAttributes failed for %S. Defaulting back to path based API\r\n", pMap->GetName()));
        }
    }

    if (attribs == INVALID_FILE_ATTRIBUTES)
    {
        //If we didn't find the map in the cache or we reached this point and still haven't got the attributes
        //then default back to the path API
        attribs = m_FilterHook.GetFileAttributesW (pwsFileName);
    }

exit:
    ReleaseMapLock (pMap, attribs == INVALID_FILE_ATTRIBUTES);
    return attribs;
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

    DWORD PrevPriority = GetThreadPriority (GetCurrentThread());
    if (PrevPriority > THREAD_PRIORITY_NORMAL) {
        SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    }

    BOOL retval = m_FilterHook.FsIoControl (FSCTL_FLUSH_BUFFERS, NULL, 0,
                                     NULL, 0, NULL, NULL);

    if (PrevPriority > THREAD_PRIORITY_NORMAL) {
        SetThreadPriority (GetCurrentThread(), PrevPriority);
    }

    return retval;

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
        result = AsyncWriteBackAllMaps (FALSE, TRUE);
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
