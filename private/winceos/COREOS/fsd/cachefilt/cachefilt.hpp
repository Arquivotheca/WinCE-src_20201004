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

#ifndef __CACHEFILT_HPP_INCLUDED__
#define __CACHEFILT_HPP_INCLUDED__

#ifndef UNDER_NT
#include <windows.h>
#endif // UNDER_NT

#include <fsdmgr.h>
#include <fsioctl.h>

#include "DirectoryHash.hpp"

#define ZONEID_INIT             0
#define ZONEID_API              1
#define ZONEID_IO               2
#define ZONEID_ALLOC            3
#define ZONEID_VOLUME           4
#define ZONEID_HANDLE           5
#define ZONEID_FILEMAP          6
#define ZONEID_VIEWPOOL         7
#define ZONEID_CELOG            12
#define ZONEID_WARNING_BREAK    13
#define ZONEID_WARNING          14
#define ZONEID_ERROR            15

#define ZONE_INIT               DEBUGZONE (ZONEID_INIT)     // Module init
#define ZONE_API                DEBUGZONE (ZONEID_API)      // File System APIs and exported interface
#define ZONE_IO                 DEBUGZONE (ZONEID_IO)       // Calls to underlying FS
#define ZONE_ALLOC              DEBUGZONE (ZONEID_ALLOC)    // Memory allocations, free lists, etc
#define ZONE_VOLUME             DEBUGZONE (ZONEID_VOLUME)
#define ZONE_HANDLE             DEBUGZONE (ZONEID_HANDLE)
#define ZONE_FILEMAP            DEBUGZONE (ZONEID_FILEMAP)
#define ZONE_VIEWPOOL           DEBUGZONE (ZONEID_VIEWPOOL)
#define ZONE_CELOG              DEBUGZONE (ZONEID_CELOG)    // Log CeLog events (mostly paging related)
#define ZONE_WARNING_BREAK      DEBUGZONE (ZONEID_WARNING_BREAK) // DEBUGCHK on some warnings
#define ZONE_WARNING            DEBUGZONE (ZONEID_WARNING)
#define ZONE_ERROR              DEBUGZONE (ZONEID_ERROR)

#define ZONEMASK_INIT           (1 << ZONEID_INIT)
#define ZONEMASK_API            (1 << ZONEID_API)
#define ZONEMASK_IO             (1 << ZONEID_IO)
#define ZONEMASK_ALLOC          (1 << ZONEID_ALLOC)
#define ZONEMASK_VOLUME         (1 << ZONEID_VOLUME)
#define ZONEMASK_HANDLE         (1 << ZONEID_HANDLE)
#define ZONEMASK_FILEMAP        (1 << ZONEID_FILEMAP)
#define ZONEMASK_VIEWPOOL       (1 << ZONEID_VIEWPOOL)
#define ZONEMASK_CELOG          (1 << ZONEID_CELOG)
#define ZONEMASK_WARNING_BREAK  (1 << ZONEID_WARNING_BREAK)
#define ZONEMASK_WARNING        (1 << ZONEID_WARNING)
#define ZONEMASK_ERROR          (1 << ZONEID_ERROR)

#ifndef CACHEFILT_ZONES
#define CACHEFILT_ZONES         (ZONEMASK_ERROR)
#endif  // CACHEFILT_ZONES

extern DBGPARAM dpCurSettings;


#ifndef OwnCS
#define OwnCS(cs)  ((DWORD)(cs)->OwnerThread == GetCurrentThreadId())
#endif


// Caching limits and default parameters
#define CACHE_MINIMUM_VM_SIZE             (  1*1024*1024)  // 1MB
#define CACHE_MAXIMUM_VM_SIZE             (128*1024*1024)  // 128MB
#define CACHE_DEFAULT_VM_SIZE             ( 32*1024*1024)  // 32MB
#define CACHE_DEFAULT_WRITEBACK_STATUS    (TRUE)  // Write-back enabled (instead of write-through)
#define CACHE_DEFAULT_WRITEBACK_PRIORITY  (248 + THREAD_PRIORITY_IDLE)  // Idle
#define CACHE_DEFAULT_WRITEBACK_TIMEOUT   (5*1000)  // Wait 5 seconds after each write

// CS ordering rules:
// - Should not grab any other CS's while holding volume list CS
// - Never take CS' from two volumes at the same time
// - If taking both FSSharedFileMap IO CS and volume CS, take volume CS first
// - Never take IO CS' from two FSSharedFileMaps at the same time


class PrivateFileHandle_t;
class CachedVolume_t;
class FSSharedFileMap_t;
class CacheViewPool_t;
class DirectoryHash_t;

// To be combined with MAP_FLUSH_* from pkfuncs.h.  These flags only
// apply inside the filter.  The kernel has no knowledge of them.
#define MAP_FLUSH_HARD_FREE_VIEW    (0x80000000)    // Not to be combined with soft_free
#define MAP_FLUSH_SOFT_FREE_VIEW    (0x40000000)    // Not to be combined with hard_free
#define MAP_FLUSH_CACHE_MASK        (0xFFFF0000)    // Bitmask of the cache-specific flush flags

#define LOCKED_MAP_LIST_SIZE 32
typedef struct LockedMapList_t {
    struct LockedMapList_t* pNext;
    FSSharedFileMap_t* pMap[LOCKED_MAP_LIST_SIZE];
} LockedMapList_t;

// m_VolumeFlags values
#define CACHE_VOLUME_UNCACHED           0x00000001  // If set, volume is uncached.  Else it's cached.
#define CACHE_VOLUME_WRITE_BACK         0x00000002  // If set, volume is write-back.  Else it's write-through.
#define CACHE_VOLUME_READ_ONLY          0x00000004  // If set, volume is read-only.  Else it's read/write.
#define CACHE_VOLUME_WFSC_SUPPORTED     0x00000008  // If set, volume supports WriteFileScatter.  Else not.
#define CACHE_VOLUME_TRANSACTION_SAFE   0x00000010  // If set, volume is transaction-safe file system. 
#define CACHE_VOLUME_IDLE_COMMIT_TXN    0x00000020  // If set, volume calls commit transactions on idle.

#define MAP_LIST_COUNT_THRESHOLD        50          // The number of maps left open, even after CloseFile.

extern bool gBootCompleted;

class   CachedVolume_t
{
public:
    // Called during HookVolume 
    CachedVolume_t (
        HDSK hdsk, 
        FILTERHOOK *pHook);
    BOOL    Init ();

    // Called during UnhookVolume 
    ~CachedVolume_t ();

    __inline BOOL IsUncached () {
        return (m_VolumeFlags & CACHE_VOLUME_UNCACHED) ? TRUE : FALSE;
    }
    __inline BOOL IsWriteBack () {
        return (m_VolumeFlags & CACHE_VOLUME_WRITE_BACK) ? TRUE : FALSE;
    }
    __inline BOOL SupportsScatterGather () {
        return (m_VolumeFlags & CACHE_VOLUME_WFSC_SUPPORTED) ? TRUE : FALSE;
    }
    __inline BOOL IsTransactionSafe () {
        return (m_VolumeFlags & CACHE_VOLUME_TRANSACTION_SAFE) ? TRUE : FALSE;
    }
    __inline BOOL CommitTransactionsOnIdle () {
        return (m_VolumeFlags & CACHE_VOLUME_IDLE_COMMIT_TXN) ? TRUE : FALSE;
    }
    __inline BOOL IsReadOnly() {
        return (m_VolumeFlags & CACHE_VOLUME_READ_ONLY) ? TRUE : FALSE;
    }
    __inline BOOL IsBootComplete () {
        return gBootCompleted ? TRUE : FALSE;
    }

    // Volume does not have a use-count; use FSDMGR_Async* instead

    void    Notify (DWORD dwFlags);
    BOOL    GetVolumeInfo (FSD_VOLUME_INFO *pInfo);

    void    SignalWrite ();
    BOOL    CompletePendingFlushes ();

    __inline void CachedVolume_t::SetPendingFlush () {
        InterlockedIncrement ((LPLONG) &m_PendingFlushes);
        DEBUGCHK (m_PendingFlushes);
    }

    __inline void CachedVolume_t::ClearPendingFlush () {
        DEBUGCHK (m_PendingFlushes);
        InterlockedDecrement ((LPLONG) &m_PendingFlushes);
    }


    HANDLE  CreateFile (
        DWORD   ProcessId,
        LPCWSTR pFileName, 
        DWORD   dwDesiredAccess, 
        DWORD   dwShareMode,
        LPSECURITY_ATTRIBUTES lpSecurityAttributes, 
        DWORD   dwCreationDisposition,
        DWORD   dwFlagsAndAttributes, 
        HANDLE  hTemplateFile
        );
    BOOL    CloseFile (
        FSSharedFileMap_t*   pMap,
        PrivateFileHandle_t* pHandle
        );

    __inline BOOL MoveFile (PCWSTR pSrcName, PCWSTR pDestName) {
        return MoveHelper (pSrcName, pDestName, FALSE);
    }

    __inline BOOL DeleteAndRenameFile (LPCWSTR pDestName, LPCWSTR pSrcName) {
        return MoveHelper (pSrcName, pDestName, TRUE);
    }

    BOOL    DeleteFile (LPCWSTR pFileName);
    BOOL    SetFileAttributes (PCWSTR pwsFileName, DWORD dwFileAttributes);        

    void    DisableCommitTransactions ();
    BOOL    CommitTransactions ();

    BOOL    FsIoControl (
        DWORD   dwIoControlCode,
        PVOID   pInBuf,
        DWORD   nInBufSize,
        PVOID   pOutBuf,
        DWORD   nOutBufSize,
        PDWORD  pBytesReturned
        );
    
    void    WriteBackThread ();
    
    
    FilterHook_t            m_FilterHook;           // Hook to underlying FS
    DirectoryHash_t*        m_pDirectoryHash;

protected:
    // These members are only for use by FSSharedFileMap_t
    friend class FSSharedFileMap_t;

    // Allow default constructor in derived classes for unit testing
    CachedVolume_t ();

    BOOL    IsRootVolume();
    
    HANDLE  OpenUnderlyingFile (
        DWORD         Disposition,
        LPCWSTR       pFileName,
        LPSECURITY_ATTRIBUTES pSecurityAttributes,
        DWORD         dwFlagsAndAttributes,
        HANDLE        hTemplateFile,
        BOOL*         pIsFileReadOnly
        );


    // The volume lock protects the map's in-use count and handle list
    __inline void AcquireVolumeLock () {
        DEBUGCHK (!OwnUnderlyingIOLock () || OwnCS (&m_csVolume));
        EnterCriticalSection (&m_csVolume);
        ASSERT (!OwnAnyMapCS ());
    }
    __inline void ReleaseVolumeLock () {
        LeaveCriticalSection (&m_csVolume);
    }
    __inline BOOL OwnVolumeLock () {
        return (OwnCS (&m_csVolume));
    }

    // The underlying I/O lock wraps all I/O to the underlying file system.
    __inline void AcquireUnderlyingIOLock () {
        EnterCriticalSection (&m_csUnderlyingIO);
    }
    __inline void ReleaseUnderlyingIOLock () {
        LeaveCriticalSection (&m_csUnderlyingIO);
    }
    __inline BOOL OwnUnderlyingIOLock () {
        return (OwnCS (&m_csUnderlyingIO));
    }

    BOOL    AsyncWriteMap (FSSharedFileMap_t* pMap, DWORD FlushFlags);
    
    // Force asynchronous writeback to finish and let go of any locked files / views.
    __inline void CompleteAsyncWrites () {
        AcquireWriteBackLock ();
        // There is no work to do here.  The pure act of taking the lock
        // should have forced all asynchronous work on the map to complete, and
        // the fact that it's no longer in the map list will prevent any future
        // asynchronous accesses.
        ReleaseWriteBackLock ();
    }

private:
    HDSK                    m_hDisk;                // Disk handle used by FSDMGR
    HVOL                    m_hVolume;              // Volume handle corresponding to m_hDisk

    DWORD                   m_OptimalPagingSize;    // The size that's most efficient to page in
    DWORD                   m_VolumeFlags;          // CACHE_VOLUME_*
    CRITICAL_SECTION        m_csVolume;             // Protects volume operations
    
    FSSharedFileMap_t *     m_pMapList;             // List of open files
    DWORD                   m_MapListCount;         // Number of map files
    DWORD                   m_LRUCounter;           // Counter to keep track of LRU map

    CachedVolume_t *        m_pNext;                // Linked list of volumes

    // Used only if the volume is write-back
    HANDLE                  m_hWriteBackThread;
    HANDLE                  m_hWriteBackEvent;      // Auto-reset event tells write-back thread there's data to write
    HANDLE                  m_hExitEvent;           // Manual-reset event that tells write-back thread to exit
    DWORD                   m_WriteBackTimeout;     // Timeout that thread should wake at
    DWORD                   m_PendingFlushes;       // Used to prevent disk-full conditions from filling the cache with
                                                    // unflushable data.
    // Used to force asynchronous operations to complete and release files/views.
    // It's okay to take m_csVolume or m_csUnderlyingIO while holding this lock,
    // but not vice-versa.
    CRITICAL_SECTION        m_csWriteBack;

    // Protects the underlying file pointer for all files in the volume.
    // It's okay to hold m_csVolume or m_csWriteBack while taking this lock,
    // but not vice-versa.
    CRITICAL_SECTION        m_csUnderlyingIO;

    // Prevent use of copy constructor
    CachedVolume_t (CachedVolume_t&);

    void    InitWriteBack (BOOL WriteBack);
    BOOL    AddMapToList (FSSharedFileMap_t* pMap);
    BOOL    RemoveMapFromList (FSSharedFileMap_t* pMap);

    _inline DWORD IncrementLRUCounter() {
        return InterlockedIncrement((LPLONG) &m_LRUCounter);
    }

    // Find and optionally lock the map
    FSSharedFileMap_t* FindMap (LPCWSTR pFileName, BOOL Lock);
    BOOL CloseMap (FSSharedFileMap_t* pMap, BOOL FlushMap);
    void CloseAllMaps();
    BOOL TrimMapList();
    BOOL DeleteMap(LPCWSTR pFileName);

    BOOL UpdateMapDirectory(LPCWSTR pSrcName, LPCWSTR pDestName);
    
    // Internal helper for both MoveFile and DeleteAndRenameFile
    BOOL    MoveHelper (
        PCWSTR pSrcName,
        PCWSTR pDestName,
        BOOL   DeleteAndRename  // FALSE: MoveFile, TRUE: DeleteAndRenameFile
        );

    // Writeback operations
    LockedMapList_t* LockMaps ();
    BOOL    WriteBackAllMaps (BOOL StopOnError);
    BOOL    AsyncWriteBackAllMaps (BOOL StopOnError);

    // The write-back lock is used to force asynchronous write-back to complete.
    __inline void AcquireWriteBackLock () {
        DEBUGCHK (!OwnVolumeLock ());
        DEBUGCHK (!OwnUnderlyingIOLock ());
        EnterCriticalSection (&m_csWriteBack);
    }
    __inline void ReleaseWriteBackLock () {
        LeaveCriticalSection (&m_csWriteBack);
    }
    __inline BOOL OwnWriteBackLock () {
        return (OwnCS (&m_csWriteBack));
    }

#ifdef DEBUG
    BOOL    OwnAnyOtherMapCS (FSSharedFileMap_t* pTargetMap);
    __inline BOOL OwnAnyMapCS () {
        return OwnAnyOtherMapCS (NULL);
    }
#endif // DEBUG
};


// Global initialization
void InitVolumes ();
void CleanupVolumes ();
void InitViewPool ();
BOOL CheckViewPool ();
void FlushViewPool ();


// m_SharedMapFlags values
#define CACHE_SHAREDMAP_WRITABLE        0x00000001  // If set, map is writable.  Else it's read-only.
#define CACHE_SHAREDMAP_PAGEABLE        0x00000002  // If set, map is pageable.  Else RFWS/WFWS unsupported.
#define CACHE_SHAREDMAP_PENDING_FLUSH   0x00000004  // Set if a file flush failed, cleared when it succeeds
#define CACHE_SHAREDMAP_PRECLOSED       0x00000008  // If set, map has been preclosed.
#define CACHE_SHAREDMAP_CLOSE_IMMEDIATE 0x00000010  // If set, map is closed immediate after all handles are closed.
#define CACHE_SHAREDMAP_HANDLE_WRITABLE 0x00000020  // If set, underlying handle is opened for GENERIC_WRITE.
#define CACHE_SHAREDMAP_DIRTY           0x00000040  // If set, then the shared map has dirty views to be flushed.

class   FSSharedFileMap_t
{
public:

    FSSharedFileMap_t (
        CachedVolume_t* pVolume,
        FilterHook_t* pFilterHook,
        HANDLE  hFile,
        LPCWSTR pFileName,
        BOOL    IsWritable);

    ~FSSharedFileMap_t ();

    BOOL    CloseFileHandle (PrivateFileHandle_t* pHandle);

    // Use a critical section to protect file I/O from above
    __inline void AcquireIOLock () {
        DEBUGCHK (!m_pVolume->OwnUnderlyingIOLock ());
        DEBUGCHK (!m_pVolume->OwnWriteBackLock ());
        EnterCriticalSection (&m_csFileIO);
    }
    __inline void ReleaseIOLock () {
        LeaveCriticalSection (&m_csFileIO);
    }
    __inline BOOL OwnIOLock () {
        return OwnCS (&m_csFileIO);
    }

    // This is a wrapper for letting the kernel mapfile flush code acquire
    // a per-volume lock during the flush.  We can't pass the volume CS to
    // the kernel because kernel.dll stores and accesses critical sections
    // different from the rest of the system.  It stores the pCrit in
    // lpcs->hCrit whereas the rest of the system stores a handle there.
    __inline void LockUnlock (BOOL Lock) {
        if (Lock) {
            m_pVolume->AcquireUnderlyingIOLock ();
        } else {
            m_pVolume->ReleaseUnderlyingIOLock ();
        }
    }
        

    //
    // Access the underlying file system through the cache
    //
    
    BOOL    CompletePendingFlush (DWORD PrivateFileFlags);

    BOOL    FlushMap (DWORD FlushFlags);

    BOOL    ReadWriteScatterGather (
        FILE_SEGMENT_ELEMENT aSegmentArray[],
        DWORD cbToAccess, 
        ULARGE_INTEGER* pOffsetArray,
        BOOL  IsWrite,
        DWORD PrivateFileFlags);    // CACHE_PRIVFILE_*

    BOOL    ReadWriteWithSeek (
        PBYTE  pBuffer, 
        DWORD  cbToAccess, 
        PDWORD pcbAccessed, 
        DWORD  dwLowOffset,
        DWORD  dwHighOffset,
        BOOL   IsWrite,
        DWORD  PrivateFileFlags);    // CACHE_PRIVFILE_*

    BOOL    SetEndOfFile (
        ULARGE_INTEGER Offset);

    BOOL    FlushFileBuffers ();

    DWORD   GetFileSize (
        PDWORD pFileSizeHigh);

    BOOL    GetFileInformationByHandle (
        PBY_HANDLE_FILE_INFORMATION pFileInfo);

    BOOL    GetFileTime (
        FILETIME* pCreation,
        FILETIME* pLastAccess, 
        FILETIME* pLastWrite);

    BOOL    SetFileTime (
        const FILETIME* pCreation, 
        const FILETIME* pLastAccess, 
        const FILETIME* pLastWrite);

    BOOL    DeviceIoControl (
         DWORD  dwIoControlCode, 
         PVOID  pInBuf, 
         DWORD  nInBufSize, 
         PVOID  pOutBuf, 
         DWORD  nOutBufSize,
         PDWORD pBytesReturned,
         DWORD  PrivateFileFlags);    // CACHE_PRIVFILE_*

    //
    // Helpers used by the FCFILT_ equivalents
    //
    
    BOOL    AcquireFileLockState (
        PFILELOCKSTATE* ppFileLockState,
        ULARGE_INTEGER  FilePosition,
        DWORD           AccessFlags);
    BOOL    ReleaseFileLockState (
        PFILELOCKSTATE* ppFileLockState);

    BOOL    UncachedReadWriteWithSeek (
        PVOID  pBuffer, 
        DWORD  cbToAccess, 
        PDWORD pcbAccessed, 
        DWORD  dwLowOffset,
        DWORD  dwHighOffset,
        BOOL   IsWrite);

    BOOL    UncachedFlushFileBuffers();

    __inline BOOL AsyncFlush (DWORD FlushFlags) {
        // Asynchronous calls must go to the volume, in order to lock the vol
        return m_pVolume->AsyncWriteMap (this, FlushFlags);
    }

    __inline CachedVolume_t* GetVolume () {
        return m_pVolume;
    }    

protected:
    // These members are only for use by CachedVolume_t
    friend class CachedVolume_t;

    BOOL    Init ();
    BOOL    InitCaching ();
    BOOL    PreCloseCaching ();

    __inline BOOL IsWritable () {
        return (m_SharedMapFlags & CACHE_SHAREDMAP_WRITABLE) ? TRUE : FALSE;
    }

    __inline BOOL HasPendingFlush () {
        return (m_SharedMapFlags & CACHE_SHAREDMAP_PENDING_FLUSH) ? TRUE : FALSE;
    }

    __inline BOOL IsCloseImmediate () {
        return (m_SharedMapFlags & CACHE_SHAREDMAP_CLOSE_IMMEDIATE) ? TRUE : FALSE;
    }

    __inline BOOL IsDirty () {
        return (m_SharedMapFlags & CACHE_SHAREDMAP_DIRTY) ? TRUE : FALSE;
    }

    BOOL SetWritable(BOOL IsWritable);
   
    BOOL    IncUseCount ();
    BOOL    DecUseCount ();

    _inline DWORD GetUseCount() {
        return m_UseCount;
    }        

    void UpdateWriteTime();

    __inline BOOL IsWriteTimeCached() {
        DEBUGCHK(OwnIOLock());
        return (m_WriteTime.dwHighDateTime != 0) || (m_WriteTime.dwLowDateTime != 0);
    }

    __inline BOOL MatchName (LPCWSTR pFileName) {
        DEBUGCHK (m_pVolume->OwnVolumeLock ());
        DEBUGCHK (m_pFileName);
        if (m_pFileName) {
            return !_wcsnicmp(m_pFileName, pFileName, MAX_PATH);
        }
        return FALSE;
    }

    // DOES NOT copy the name -- takes the pointer directly
    void    SetName (LPCWSTR pNewName);
    BOOL    UpdateDirectoryName(LPCWSTR pSourceName, LPCWSTR pDestName);

    void    SetUnderlyingFileHandle(HANDLE hNewHandle);

    // Track the PrivateFileHandle_t's
    BOOL    AddHandle (
        PrivateFileHandle_t* pHandle
        );
    // Returns TRUE if this was the last handle in the map
    BOOL    RemoveHandle (PrivateFileHandle_t* pHandle);
    BOOL    CheckHandleMoveSharing ();

    _inline BOOL AreHandlesInMap() {
        return (m_pPrivList != NULL);
    }

    __inline void StartCachedWrite (ULARGE_INTEGER StartWriteRequest, DWORD WriteRequestLength) {
        DEBUGCHK(OwnIOLock());
        m_StartWriteRequest = StartWriteRequest;
        m_WriteRequestLength = WriteRequestLength;
    }

    __inline void EndCachedWrite() {
        DEBUGCHK(OwnIOLock());
        m_StartWriteRequest.QuadPart = 0;
        m_WriteRequestLength = 0;
    }

    _inline BOOL IsRangeOverwritten (DWORD LowOffset, DWORD HighOffset, DWORD Length) {
        DEBUGCHK(OwnIOLock());
        ULARGE_INTEGER Offset;
        Offset.LowPart = LowOffset;
        Offset.HighPart = HighOffset;
        return (m_WriteRequestLength != 0) &&
               (Offset.QuadPart >= m_StartWriteRequest.QuadPart) &&
               ((Offset.QuadPart + Length) <= (m_StartWriteRequest.QuadPart + m_WriteRequestLength));
    }        

    _inline DWORD GetLRUCounter() {
        return m_LRUCounter;
    }

    _inline void IncrementLRUCounter() {
        m_LRUCounter = m_pVolume->IncrementLRUCounter();
    }

    // Doubly-linked list of maps on the same volume
    // Must own the Volume CS in order to walk the list
    FSSharedFileMap_t*      m_pPrev;
    FSSharedFileMap_t*      m_pNext;

protected:
    // These members are only for use by CacheViewPool_t
    friend class CacheViewPool_t;

    // Allow CacheViewPool to maintain the CacheView list.  Must own the pool
    // lock in order to walk the list.  The views will be sorted by offset
    // within the file.
    struct CacheView_t*     m_pViewList;

    BOOL    FlushView (CommonMapView_t* pView, DWORD FlushFlags, PVOID* pPageList);

private:
    static const DWORD s_MaxUseCount = (DWORD) 0x80000000;  // 2G
    DWORD                   m_UseCount;         // Count of ongoing accesses to the object

    FILELOCKSTATE           m_LockState;
    DWORD                   m_SharedMapFlags;   // CACHE_SHAREDMAP_*
    CRITICAL_SECTION        m_csFileIO;         // Protects file I/O from above

    // Canonicalized file name; shorter than MAX_PATH if possible, to save RAM.
    LPCWSTR                 m_pFileName;    // Must own the volume CS to touch the name
    // Note: To properly handle short filenames and hard links we'll need an
    // open file ID from the underlying FSD.

    CachedVolume_t*         m_pVolume;
    DWORD                   m_NKSharedFileMapId;  // Reference to kernel object
    PrivateFileHandle_t*    m_pPrivList;

    ULARGE_INTEGER          m_StartWriteRequest;
    DWORD                   m_WriteRequestLength;
    FILETIME                m_WriteTime;
    DWORD                   m_LRUCounter;

    // Pulled into a structure to emphasize that all accesses via these
    // members do not go through the cache.  Should only be accessed
    // inside FSSharedFileMap_Uncached.cpp.
    struct {
        FilterHook_t*       pFilterHook;
        // pseudo handle for underlying filterhook or INVALID_HANDLE_VALUE for just VM
        HANDLE              FilePseudoHandle;   
    } m_UncachedHooks;

    
    // Prevent use of default constructor, copy constructor
    FSSharedFileMap_t ();
    FSSharedFileMap_t (FSSharedFileMap_t&);
    

    //
    // Uncached access to the underlying file system
    //
    
    void    UncachedCloseHandle ();

    BOOL    UncachedReadWriteScatterGather (
        FILE_SEGMENT_ELEMENT aSegmentArray[], 
        DWORD cbToAccess, 
        ULARGE_INTEGER* pOffsetArray,
        BOOL  IsWrite);

    BOOL    UncachedSetEndOfFile (
        ULARGE_INTEGER Offset);

    BOOL    UncachedGetFileSize (
        ULARGE_INTEGER* pFileSize);

    BOOL    UncachedGetFileInformationByHandle (
        PBY_HANDLE_FILE_INFORMATION pFileInfo);

    BOOL    UncachedGetFileTime (
        FILETIME* pCreation,
        FILETIME* pLastAccess, 
        FILETIME* pLastWrite);

    BOOL    UncachedSetFileTime (
        CONST FILETIME *pCreation, 
        CONST FILETIME *pLastAccess, 
        CONST FILETIME *pLastWrite);

    BOOL    UncachedSetExtendedFlags (
        DWORD Flags);

    BOOL    UncachedDeviceIoControl (
        DWORD  dwIoControlCode, 
        PVOID  pInBuf, 
        DWORD  nInBufSize, 
        PVOID  pOutBuf, 
        DWORD  nOutBufSize,
        PDWORD pBytesReturned);


    //
    // Private worker functions
    //

    DWORD ClearFlags (
        DWORD dwFlagsToClear
        );

    DWORD SetFlags (
        DWORD dwFlagsToSet
        );

    __inline void MarkDirty () {
        if (!IsDirty ()) {
            SetFlags (CACHE_SHAREDMAP_DIRTY);
        }
    }
    
    __inline void ClearDirty () {
        ClearFlags (CACHE_SHAREDMAP_DIRTY);
    }

    BOOL    CleanupCaching (BOOL fWriteBack = TRUE);
    void    SetPendingFlush ();
    void    ClearPendingFlush ();

    BOOL    CheckHandleSharing (
        DWORD NewSharing,
        DWORD NewAccess,
        DWORD CreationDisposition
        );

    BOOL    UncachedSetFilePointer (ULARGE_INTEGER FileOffset);
    BOOL    GetCachedFileSize (ULARGE_INTEGER* pFileSize);
    BOOL    SetCachedFileSize (ULARGE_INTEGER NewFileSize);
    BOOL    SetFileSizeBeforeWrite (ULARGE_INTEGER WriteEndOffset, ULARGE_INTEGER* pCurFileSize);
    BOOL    SetFileSizeAfterFailedWrite (ULARGE_INTEGER WriteEndOffset, ULARGE_INTEGER OldFileSize);
};


typedef struct FindHandle_t {
    CachedVolume_t* pVolume;
    HANDLE hFindPseudoHandle;   // Handle for the underlying filterhook
    DWORD  ProcessId;
} FindHandle_t;


// m_PrivFileFlags values
#define CACHE_PRIVFILE_UNCACHED     0x00000001  // If set, handle is uncached.  Else it's cached.
#define CACHE_PRIVFILE_WRITE_BACK   0x00000002  // If set, handle is write-back.  Else it's write-through.

// This object corresponds to the pseudo-handle returned by the cache manager CreateFile.
class PrivateFileHandle_t
{
public:
    PrivateFileHandle_t (
        FSSharedFileMap_t* pMap,
        DWORD  dwSharing,
        DWORD  dwAccess,
        DWORD  dwFlagsAndAttributes,
        DWORD  ProcessId);
    ~PrivateFileHandle_t ();

    DWORD   GetAccess () {
        return m_AccessFlags;
    }
    DWORD   GetSharing () {
        return m_SharingFlags;
    }

    // PrivateFileHandle wraps APIs which need the handle state (access, position)

    BOOL    ReadWrite (
        PBYTE   pBuffer,
        DWORD   cbToAccess,
        LPDWORD pcbAccessed, 
        BOOL    IsWrite);

    BOOL    ReadWriteScatterGather (
        FILE_SEGMENT_ELEMENT aSegmentArray[], 
        DWORD   cbToAccess, 
        LPDWORD pReserved,
        BOOL    IsWrite);

    BOOL    ReadWriteWithSeek (
        PBYTE  pBuffer, 
        DWORD  cbToAccess, 
        PDWORD pcbAccessed, 
        DWORD  dwLowOffset,
        DWORD  dwHighOffset,
        BOOL   IsWrite);

    DWORD   SetFilePointer (
        LONG  lDistanceToMove, 
        PLONG lpDistanceToMoveHigh, 
        DWORD dwMoveMethod);

    BOOL    SetEndOfFile ();

    BOOL    DeviceIoControl (DWORD dwIoControlCode, PVOID pInBuf,
                             DWORD nInBufSize, PVOID pOutBuf,
                             DWORD nOutBufSize, PDWORD pBytesReturned);

    BOOL    LockFileEx (
        DWORD dwFlags, 
        DWORD dwReserved, 
        DWORD nNumberOfBytesToLockLow,
        DWORD nNumberOfBytesToLockHigh, 
        OVERLAPPED *pOverlapped);

    BOOL    UnlockFileEx (
        DWORD dwReserved, 
        DWORD nNumberOfBytesToUnlockLow, 
        DWORD nNumberOfBytesToUnlockHigh,
        OVERLAPPED * pOverlapped);

    // Public helpers used by the FCFILT_ equivalents
    __inline BOOL AcquireFileLockState (PFILELOCKSTATE* ppFileLockState)
    {
        return m_pSharedMap->AcquireFileLockState(ppFileLockState, m_FilePosition,
                                                  m_AccessFlags);
    }
    
    __inline BOOL ReleaseFileLockState (PFILELOCKSTATE* ppFileLockState)
    {
        return m_pSharedMap->ReleaseFileLockState (ppFileLockState);
    }


    FSSharedFileMap_t*      m_pSharedMap;

protected:
    // These members are only for use by FSSharedFileMap_t
    friend class FSSharedFileMap_t;

    // Linked list of private file handles for the map
    // Must own the FSSharedFileMap I/O lock in order to walk the list
    PrivateFileHandle_t*    m_pNext;

private:
    DWORD                   m_AccessFlags;
    DWORD                   m_SharingFlags;
    ULARGE_INTEGER          m_FilePosition;     // No critsec to protect. Always make a temp copy instead.
    DWORD                   m_PrivFileFlags;    // CACHE_PRIVFILE_FLAG_*
    DWORD                   m_ProcessId;

    // Prevent use of default constructor, copy constructor
    PrivateFileHandle_t ();
    PrivateFileHandle_t (PrivateFileHandle_t&);
};


// CacheView_t::wFlags values
#define CACHE_VIEW_FREE                 0x0001  // View is not assigned to a file
#define CACHE_VIEW_FREE_IN_PROGRESS     0x0002  // View is in the process of being freed


// NOTENOTE this could be its own class but I decided it'd be simpler to manage
// the lists if we didn't encapsulate those.
typedef struct CacheView_t {
    // Base info that applies to all views
    CommonMapView_t Base;       // MapOffset is only valid while allocated
    WORD    wUseCount;          // Count of ongoing accesses to this view
    WORD    wFlags;             // CACHE_VIEW_*

    union {
        // Allocated views use these member variables
        struct {
            FSSharedFileMap_t* pSharedMap;

            // Must own the cache view pool lock in order to walk the lists

            // Doubly-linked list of views for the same file, in order by file offset.
            CacheView_t* pNextInFile;
            CacheView_t* pPrevInFile;

            // Doubly-linked global LRU of cache views that are allocated (assigned to
            // a file) but not currently in use (actively being read/written/flushed)
            // Only views with wUseCount=0 and wFlags!=FREE are on the LRU.
            CacheView_t* pLRUOlder;
            CacheView_t* pLRUNewer;

            // Each allocated view with dirty pages has a page-list, temp space for
            // flushing the view.  The view is not dirtied if the page-list can't be
            // allocated.  That protects us from getting into OOM situations where we
            // need to allocate memory in order to flush views and decommit cache pages.
            PVOID* pPageList;
        } Alloc;
        
        // Free views use these member variables
        struct {
            // Singly-linked list of free views.
            CacheView_t* pNextFree;
        } Free;
    };
} CacheView_t;


class CacheViewPool_t {
public:
    // Used to receive extended information from GetLockedView and pass it back
    // to UnlockView
    typedef struct {
        LPBYTE pBaseAddr;       // Base address requested
        DWORD  cbView;          // Size requested, up to the end of the view
        CacheView_t* pView;     // Passed for easy unlock
    } LockedViewInfo;

    CacheViewPool_t ();
    ~CacheViewPool_t ();
    BOOL Init (pfnNKAllocCacheVM pAllocCacheVM);


    BOOL    GetView (
        DWORD  dwAddr,
        DWORD* pNKSharedFileMapId,
        CommonMapView_t** ppview);

    BOOL    LockView (
        FSSharedFileMap_t* pMap,
        const ULARGE_INTEGER& CurFileSize,
        const ULARGE_INTEGER& StartFileOffset,
        DWORD cbToLock,
        BOOL  IsWrite,
        LockedViewInfo& info,
        CacheView_t* pPrevView);
    void    UnlockView (
        FSSharedFileMap_t* pMap,
        const LockedViewInfo& info);
    
    BOOL    WalkViewsInRange (
        FSSharedFileMap_t* pMap,
        const ULARGE_INTEGER& StartFileOffset,
        DWORD cbSize,
        DWORD FlushFlags);
    
    __inline BOOL WalkViewsAfterOffset (FSSharedFileMap_t* pMap, const ULARGE_INTEGER& FileOffset, DWORD FlushFlags) {
        return WalkViewsInRange (pMap, FileOffset, 0, FlushFlags);
    }
    __inline BOOL WalkAllViews (FSSharedFileMap_t* pMap, DWORD FlushFlags) {
        ULARGE_INTEGER zero = { 0, 0 };
        return WalkViewsInRange (pMap, zero, 0, FlushFlags);
    }
    
    BOOL    WalkAllFileViews (DWORD FlushFlags);

    __inline DWORD GetNumPageListEntries () {
        return s_NumPageListEntries;
    }

private:
    static const DWORD s_cbView = 64*1024;  // Size of each view in the pool
    static const WORD  s_MaxViewUseCount = (WORD) -1;  // 64K
    static DWORD       s_NumPageListEntries;

    // Keep a single global free PageList, to cut down on alloc thrashing
    static PVOID*    s_pFreePageList;

    // Must not acquire any other CS while holding the pool CS!
    CRITICAL_SECTION m_csPool;
    
    const VOID*      m_pVM;           // The actual view VM area
    DWORD            m_NumViews;      // Number of views
    CacheView_t*     m_ViewArray;     // Array of CacheViews describing the VM
    CacheView_t*     m_pLRUOldest;    // Oldest entry in LRU list of allocated views
    CacheView_t*     m_pLRUNewest;    // Newest entry in LRU list of allocated views
    CacheView_t*     m_pFirstFree;    // Linked list of free views

#ifdef DEBUG
    // Debugging information
    struct {
        DWORD CurFree;                // Current # of free views
        DWORD Allocations;            // Total # of times we've allocated a view
        DWORD Displacements;          // Number of times we had to displace a view
                                      // in order to allocate a new one
        DWORD LockFailures;           // # times the views required for file I/O could
                                      // not be locked, falling back to uncached I/O
        DWORD FreeRetries;            // # of times we had to retry a free
    } m_DebugInfo;
#endif

    // Prevent use of the copy constructor
    CacheViewPool_t (CacheViewPool_t&);


    //
    // Private worker functions
    //

    __inline void AcquirePoolLock () {
        EnterCriticalSection (&m_csPool);
    }
    __inline void ReleasePoolLock () {
        LeaveCriticalSection (&m_csPool);
    }
    __inline BOOL OwnPoolLock () {
        return (OwnCS (&m_csPool));
    }

    BOOL    IncViewUseCount (CacheView_t* pView);
    BOOL    DecViewUseCount (CacheView_t* pView);

    void    LRUAdd (CacheView_t* pView);
    void    LRURemove (CacheView_t* pView);

    CacheView_t* PopFreeView ();
    void    PushFreeView (CacheView_t* pView);
    
    void    UpdateViewSize (CacheView_t* pView, const ULARGE_INTEGER& CurFileSize);
    void    AssignViewToFile (
        FSSharedFileMap_t* pMap,
        const ULARGE_INTEGER& CurFileSize,
        CacheView_t* pView,
        PVOID* pPageList,
        const  ULARGE_INTEGER& Offset,
        CacheView_t* pPrevInFile);
    void    RemoveViewFromFile (CacheView_t* pView);
    
    CacheView_t* FindViewBeforeOffset (FSSharedFileMap_t* pMap, const ULARGE_INTEGER& FileOffset);
    CacheView_t* FindViewAfterOffset (FSSharedFileMap_t* pMap, const ULARGE_INTEGER& FileOffset);

    BOOL    FreeOldestView ();
    BOOL    FlushCacheView (CacheView_t* pView, DWORD FlushFlags, CacheView_t** ppNextInFile);

    PVOID*  AllocPageList ();
    void    FreePageList (PVOID* pPageList);
};


#define BLOCKALIGN_UP(x)        (((x) + VM_BLOCK_OFST_MASK) & ~VM_BLOCK_OFST_MASK)


// Helpers used with LockFileEx and UnlockFileEx
BOOL FCFILT_AcquireFileLockState (DWORD dwHandle, PFILELOCKSTATE *ppFileLockState);
BOOL FCFILT_ReleaseFileLockState (DWORD dwHandle, PFILELOCKSTATE *ppFileLockState);

// Misc other helpers
LPWSTR DuplicateFileName (LPCWSTR pFileName);

extern "C" HANDLE   FindFirstFileInternal (PVOLUME pvol, HANDLE  hProcess, PCWSTR  pwsFileSpec, PWIN32_FIND_DATAW pfd);
extern "C" BOOL     FCFILT_FindNextFileW (PSEARCH pSearch, PWIN32_FIND_DATAW pfd);
extern "C" BOOL     FCFILT_FindClose (PSEARCH pSearch);
extern "C" DWORD    FCFILT_GetFileAttributesW (PVOLUME pvol, PCWSTR pwsFileName);


#endif // __CACHEFILT_HPP_INCLUDED__
