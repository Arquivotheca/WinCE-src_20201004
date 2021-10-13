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
#define ZONEID_WRITEBACK        8
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
#define ZONE_WRITEBACK          DEBUGZONE (ZONEID_WRITEBACK)
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
#define ZONEMASK_WRITEBACK      (1 << ZONEID_WRITEBACK)
#define ZONEMASK_CELOG          (1 << ZONEID_CELOG)
#define ZONEMASK_WARNING_BREAK  (1 << ZONEID_WARNING_BREAK)
#define ZONEMASK_WARNING        (1 << ZONEID_WARNING)
#define ZONEMASK_ERROR          (1 << ZONEID_ERROR)

#ifndef CACHEFILT_ZONES
#define CACHEFILT_ZONES         (ZONEMASK_ERROR)
#endif  // CACHEFILT_ZONES

extern DBGPARAM dpCurSettings;


#ifndef OwnCS
#define OwnCS(cs)       OWN_CS(cs)
#endif


// Caching limits and default parameters
#define CACHE_MINIMUM_VM_SIZE             (  1*1024*1024)  // 1MB
#define CACHE_MAXIMUM_VM_SIZE             (128*1024*1024)  // 128MB
#define CACHE_DEFAULT_VM_SIZE             ( 64*1024*1024)  // 64MB
#define CACHE_DEFAULT_WRITEBACK_STATUS    (TRUE)  // Write-back enabled (instead of write-through)
#define CACHE_DEFAULT_WRITEBACK_PRIORITY  (248 + THREAD_PRIORITY_IDLE)  // Idle
#define CACHE_DEFAULT_WRITEBACK_TIMEOUT   (5*1000)  // Wait 5 seconds after each write
#define CACHE_DEFAULT_WRITEBACK_BACKOFF   (1000) // Back-off 1 second when write-back is interrupted


// CS ordering rules:
// - Should not grab any other CS's while holding volume list CS
// - Never take CS' from two volumes at the same time
// - If taking both FSSharedFileMap CS and volume CS, take FSSharedFileMap CS first
// - Taking two FSSharedFileMaps CS's is only allowed during move, which is protected by MoveLock.


class PrivateFileHandle_t;
class CachedVolume_t;
class DirectoryHash_t;
class FSSharedFileMap_t;
class FileMapCollection;

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
#define CACHE_VOLUME_BOOT_COMPLETE      0x00000040  // If set, indicates that boot has completed.
#define CACHE_VOLUME_CLEAN_BOOT         0x00000080  // If set, indicates the system has clean booted.


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
    __inline BOOL IsBootComplete () {
        return (m_VolumeFlags & CACHE_VOLUME_BOOT_COMPLETE) ? TRUE : FALSE;
    }
    __inline BOOL IsCleanBoot () {
        return (m_VolumeFlags & CACHE_VOLUME_CLEAN_BOOT) ? TRUE : FALSE;
    }
    __inline BOOL IsFlushingDisabled () {
        return (IsCleanBoot() && !IsBootComplete());
    }

    // Volume does not have a use-count; use FSDMGR_Async* instead

    void    Notify (DWORD dwFlags);
    BOOL    GetVolumeInfo (FSD_VOLUME_INFO *pInfo);

    void    SignalWrite ();

    __inline void SignalIO () {
        SetCancelFlushFlag(TRUE);
    }
    __inline BOOL* GetCancelFlushFlag () {
        return &m_fCancelFlush;
    }

    __inline void SetCancelFlushFlag (BOOL fValue) {
        m_fCancelFlush = fValue;
    }

    __inline BOOL GetVolumeHandle () {
        return m_hVolume;
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
    DWORD   GetFileAttributes (PCWSTR pwsFileName);

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
        EnterCriticalSection (&m_csVolume);
    }
    __inline void ReleaseVolumeLock () {
        LeaveCriticalSection (&m_csVolume);
    }
    __inline BOOL OwnVolumeLock () {
        return (OwnCS (&m_csVolume));
    }

    // The move lock protects MoveFile/DeleteAndRename to prevent map lock
    // ordering issues.
    __inline void AcquireMoveLock () {
        EnterCriticalSection (&m_csMove);
    }
    __inline void ReleaseMoveLock () {
        LeaveCriticalSection (&m_csMove);
    }
    __inline BOOL OwnMoveLock () {
        return (OwnCS (&m_csMove));
    }

    void IncrementUnreferencedMapCount();

    void DecrementUnreferencedMapCount();

    BOOL CloseMap (FSSharedFileMap_t* pMap, BOOL FlushMap);
    
private:
    HDSK                    m_hDisk;                // Disk handle used by FSDMGR
    HVOL                    m_hVolume;              // Volume handle corresponding to m_hDisk

    DWORD                   m_OptimalPagingSize;    // The size that's most efficient to page in
    DWORD                   m_VolumeFlags;          // CACHE_VOLUME_*
    CRITICAL_SECTION        m_csVolume;             // Protects volume operations
    CRITICAL_SECTION        m_csMove;               // Protects move operations
    
    FileMapCollection*      m_pMapCollection;

    CachedVolume_t *        m_pNext;                // Linked list of volumes

    // Used only if the volume is write-back
    HTHREAD                 m_hWriteBackThread;
    HANDLE                  m_hWriteBackEvent;      // Auto-reset event tells write-back thread there's data to write
    HANDLE                  m_hExitEvent;           // Manual-reset event that tells write-back thread to exit
    DWORD                   m_WriteBackTimeout;     // Timeout that thread should wake at
    BOOL                    m_fCancelFlush;         // Flag to cancel write-back flushing

    // Prevent use of copy constructor
    CachedVolume_t (CachedVolume_t&);

    void    InitWriteBack (BOOL WriteBack);
    
    BOOL TrimMapList();
    
    BOOL DeleteMap(
        FSSharedFileMap_t* pMap,
        BOOL FlushMap);

    // Internal helper for both MoveFile and DeleteAndRenameFile
    BOOL    MoveHelper (
        PCWSTR pSrcName,
        PCWSTR pDestName,
        BOOL   DeleteAndRename  // FALSE: MoveFile, TRUE: DeleteAndRenameFile
        );

    // Writeback operations
    LockedMapList_t* LockMaps ();

    BOOL    AsyncWriteBackAllMaps (BOOL fStopOnIO = FALSE, BOOL fForceFlush = FALSE);

    static BOOL IsDeviceIdle();

    FSSharedFileMap_t* AcquireMapLock (LPCWSTR pFileName);

    void ReleaseMapLock(
        FSSharedFileMap_t* pMap,
        BOOL fRemoveMapIfClosed);

};


// Global initialization
void InitVolumes ();
void CleanupVolumes ();
void InitPageTree ();


// m_SharedMapFlags values
#define CACHE_SHAREDMAP_WRITABLE        0x00000001  // If set, map is writable.  Else it's read-only.
#define CACHE_SHAREDMAP_CLOSE_IMMEDIATE 0x00000010  // If set, map is closed immediate after all handles are closed.
#define CACHE_SHAREDMAP_HANDLE_WRITABLE 0x00000020  // If set, underlying handle is opened for GENERIC_WRITE.
#define CACHE_SHAREDMAP_DELETE_ON_CLOSE 0x00000080  // If set, the underlying file is deleted when the last handle is closed.
#define CACHE_SHAREDMAP_HANDLE_OPEN     0x00000100  // If set, this indicates a handle is open on the file.
#define CACHE_SHAREDMAP_INVALID         0x00000200  // If set, map is removed from map list and is no longer valid.

#if 0
// CELog Functions
inline BOOL IsCeLogZoneStorageEnabled();
inline void LogCacheFiltMapName(FSSharedFileMap_t* pMap);
inline void LogCacheFiltFileOp(CEL_FILEOP_TYPE dwOp, PFILE pFile, DWORD dwFileOffset, DWORD dwLen);
inline void LogCacheFiltFileOpEnd(CEL_FILEOP_TYPE dwOp, BOOL fResult);
inline void LogCacheFiltFileOpEndValue(CEL_FILEOP_TYPE dwOp, DWORD dwResult);
inline void LogCacheFiltFindOp(CEL_FILEOP_TYPE dwOp, PSEARCH pSearch);
inline void LogCacheFiltIoctl(CEL_FILEOP_TYPE dwOp, PFILE pFile, DWORD dwIoctl);
inline void LogCacheFiltLockFile(CEL_FILEOP_TYPE dwOp, PFILE pFile, DWORD dwLockFlags, DWORD dwLowBytes, DWORD dwHighBytes);
inline void LogCacheFiltString(CEL_FILEOP_TYPE dwOp, DWORD dwHandle, const WCHAR *szFileName);
inline void LogCacheFiltTwoStrings(CEL_FILEOP_TYPE dwOp, DWORD dwHandle, const WCHAR *szFileName1, const WCHAR *szFileName2);

inline void LogCacheFiltCreateFile(
    const WCHAR *szFileName, 
    DWORD dwAccess, 
    DWORD dwShareMode, 
    DWORD dwCreate, 
    DWORD dwFlagsAndAttributes);

void LogCacheFiltMapOp(CEL_FILEOP_TYPE dwOp, FSSharedFileMap_t* pMap);
#endif

class   FSSharedFileMap_t
{
public:

    FSSharedFileMap_t (
        CachedVolume_t* pVolume,
        FilterHook_t* pFilterHook,
        LPCWSTR pFileName);
private:
    ~FSSharedFileMap_t ();
public:

    static BOOL CloseMap(__in FSSharedFileMap_t *pMap, const BOOL performFlush);

    BOOL    CloseFileHandle (PrivateFileHandle_t* pHandle);

    // Use a critical section to protect file I/O and path-based operations on file.
    __inline void AcquireMapLock () {
        DEBUGCHK (!m_pVolume->OwnVolumeLock());
        EnterCriticalSection (&m_csFile);
    }
    __inline void ReleaseMapLock () {
        LeaveCriticalSection (&m_csFile);
    }
    __inline BOOL OwnMapLock () {
        return OwnCS (&m_csFile);
    }       

    BOOL    InvalidateMap();
    BOOL    FlushMap (BOOL fStopOnIO = FALSE);
    BOOL    WriteBackMap (BOOL fStopOnIO);

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

    BOOL    CompleteWrite (
        DWORD  cbToAccess, 
        DWORD  dwLowOffset,
        DWORD  dwHighOffset,
        DWORD  dwPrivateFileFlags);

    BOOL    SetEndOfFile (
        ULARGE_INTEGER Offset);

    BOOL    CommitTransactions ();

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

    BOOL    UncachedSetEndOfFile (
        ULARGE_INTEGER Offset);

    BOOL    UncachedFlushFileBuffers();

    DWORD   GetPageTreeId();
    DWORD   TransferPageTree();

    __inline CachedVolume_t* GetVolume () {
        return m_pVolume;
    }    

    __inline LPCWSTR GetName () {
        DEBUGCHK (m_pFileName);
        return m_pFileName;
    }

    // Used by CELOG to track the last celog "generation"
    // when the filename corresponding to this object was logged
    DWORD m_CurrentCeLogGeneration;                                                       

protected:
    // These members are only for use by CachedVolume_t
    friend class CachedVolume_t;
    friend class FileMapCollection;

    BOOL    Init (HANDLE hFile, BOOL IsWritable);

    BOOL    InitCaching ();

    __inline BOOL IsWritable () {
        return (m_SharedMapFlags & CACHE_SHAREDMAP_WRITABLE) ? TRUE : FALSE;
    }

    __inline BOOL IsCloseImmediate () {
        return (m_SharedMapFlags & CACHE_SHAREDMAP_CLOSE_IMMEDIATE) ? TRUE : FALSE;
    }

    __inline BOOL IsDeleteOnClose () {
        return (m_SharedMapFlags & CACHE_SHAREDMAP_DELETE_ON_CLOSE) ? TRUE : FALSE;
    }

    __inline void SetDeleteOnClose () {
        m_SharedMapFlags |= CACHE_SHAREDMAP_DELETE_ON_CLOSE;
    }

    __inline BOOL IsInvalid () {
        return (m_SharedMapFlags & CACHE_SHAREDMAP_INVALID) ? TRUE : FALSE;
    }
        
    BOOL IsDirty ();        
    BOOL SetWritable(BOOL IsWritable);
   
    BOOL    IncUseCount ();
    BOOL    DecUseCount ();

    _inline DWORD GetUseCount() {
        return m_UseCount;
    }        

    void UpdateWriteTime();

    __inline BOOL IsWriteTimeCached() {
        DEBUGCHK(OwnMapLock());
        return (m_WriteTime.dwHighDateTime != 0) || (m_WriteTime.dwLowDateTime != 0);
    }

    __inline BOOL MatchName (LPCWSTR pFileName) {
        BOOL result = FALSE;
        DEBUGCHK (m_pFileName);
        m_pVolume->AcquireVolumeLock();
        if (m_pFileName) {
            result = !_wcsnicmp(m_pFileName, pFileName, MAX_PATH);
        }
        m_pVolume->ReleaseVolumeLock();
        return result;
    }

    // DOES NOT copy the name -- takes the pointer directly
    void    SetName (LPCWSTR pNewName);

    LPWSTR  CreateNameForDirectoryUpdate(__in_z LPCWSTR pSourceName, __in_z LPCWSTR pDestName);

    void    SetUnderlyingFileHandle(HANDLE hNewHandle);

    // Track the PrivateFileHandle_t's
    BOOL    AddHandle (
        PrivateFileHandle_t* pHandle
        );
    // Returns TRUE if this was the last handle in the map
    BOOL    RemoveHandle (PrivateFileHandle_t* pHandle);
    BOOL    CheckHandleMoveSharing ();

    _inline BOOL AreHandlesInMap() {
        return (m_SharedMapFlags & CACHE_SHAREDMAP_HANDLE_OPEN) ? TRUE : FALSE;
    }

    _inline BOOL DoesPageTreeExist() {
        return (m_PageTreeId != 0);
    }        

    __inline DWORD GetCollectionGeneration() {
        return m_collectionGeneration;
    }

    __inline void SetCollectionGeneration(const DWORD newGeneration) {
        m_collectionGeneration = newGeneration;
    }

    __inline BOOL IsFileOpen() {
        return (m_UncachedHooks.FilePseudoHandle != INVALID_HANDLE_VALUE);
    }
    
private:
    static const DWORD s_MaxUseCount = (DWORD) 0x80000000;  // 2G
    DWORD                   m_UseCount;         // Count of ongoing accesses to the object

    FILELOCKSTATE           m_LockState;
    volatile DWORD          m_SharedMapFlags;   // CACHE_SHAREDMAP_*
    CRITICAL_SECTION        m_csFile;           // Protects file I/O from above

    // Canonicalized file name; shorter than MAX_PATH if possible, to save RAM.
    LPCWSTR                 m_pFileName;    // Must own the volume CS to touch the name
    // Note: To properly handle short filenames and hard links we'll need an
    // open file ID from the underlying FSD.

    CachedVolume_t*         m_pVolume;
    DWORD                   m_PageTreeId;  // Reference to kernel object
    PrivateFileHandle_t*    m_pPrivList;

    FILETIME                m_WriteTime;
    DWORD                   m_collectionGeneration;

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

    BOOL    UncachedGetValidDataLength (
        ULARGE_INTEGER* pValidDataLength);

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

    BOOL    CleanupCaching (BOOL fWriteBack = TRUE);

    BOOL    CheckHandleSharing (
        DWORD NewSharing,
        DWORD NewAccess,
        DWORD CreationDisposition
        );

    BOOL    UncachedSetFilePointer (ULARGE_INTEGER FileOffset);
    BOOL    GetCachedFileSize (ULARGE_INTEGER* pFileSize);
    BOOL    SetCachedFileSize (ULARGE_INTEGER NewFileSize, BOOL fDeleteFile = FALSE);

    BOOL    FlushPageTree(
        ULARGE_INTEGER StartOffset,
        DWORD cbSize,
        BOOL fStopOnIO);

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

    ULARGE_INTEGER GetFilePosition() {
        return m_FilePosition;
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

    __inline void SignalIO ()
    {
        m_pSharedMap->GetVolume()->SignalIO();
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

// Exports to the kernel
extern "C" BOOL FCFILT_UncachedWrite(
    DWORD FSSharedFileMapId, 
    const ULARGE_INTEGER *pliOffset, 
    const BYTE *pData, 
    DWORD cbSize,
    LPDWORD pcbWritten);

extern "C" BOOL FCFILT_UncachedRead(
    DWORD FSSharedFileMapId, 
    const ULARGE_INTEGER *pliOffset, 
    BYTE *pData, 
    DWORD cbSize,
    LPDWORD pcbRead);

extern "C" BOOL FCFILT_UncachedSetFileSize (
    DWORD FSSharedFileMapId, 
    const ULARGE_INTEGER *pliSize
    );

#define SetFsPowerOffFlag()             (UTlsPtr()[TLSSLOT_KERNEL] |= TLSKERN_IN_POWEROFF)
#define ClearFsPowerOffFlag()           (UTlsPtr()[TLSSLOT_KERNEL] &= ~TLSKERN_IN_POWEROFF)
#define IsFsPowerOffThread()            (UTlsPtr()[TLSSLOT_KERNEL] & TLSKERN_IN_POWEROFF)

#endif // __CACHEFILT_HPP_INCLUDED__
