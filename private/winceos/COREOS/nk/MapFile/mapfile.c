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

#include "kernel.h"
#include "pager.h"

// Several headers are required for supporting gathered flushes
#include <storemgr.h>
#include <diskio.h>
#include <fsioctl.h>

extern CRITICAL_SECTION MapCS, FlushCS, PageOutCS;
#ifdef DEBUG
extern CRITICAL_SECTION PagerCS; // Used to sanity-check CS usage on debug builds
#endif

DLIST g_MapList;  // Protected by MapCS

// As many process references as can fit into HEAP_SIZE3
#ifdef DEBUG
#define LOCKED_PROCESS_LIST_SIZE  ((HEAP_SIZE3 - sizeof(struct LockedProcessList_t*) - sizeof(DWORD)) / sizeof(PHDATA))
#else
#define LOCKED_PROCESS_LIST_SIZE  ((HEAP_SIZE3 - sizeof(struct LockedProcessList_t*)) / sizeof(PHDATA))
#endif

// Used during low-memory reclamation, page pool trimming and lazy writeback
typedef struct LockedProcessList_t {
#ifdef DEBUG
    DWORD  signature;  // debug safety checking
#endif // DEBUG

    struct LockedProcessList_t* pNext;
    PHDATA phdProcess[LOCKED_PROCESS_LIST_SIZE];
} LockedProcessList_t;

ERRFALSE (sizeof(LockedProcessList_t) <= HEAP_SIZE_LOCKPROCLIST);

#define MAP_LOCKPROCLIST_SIG    (0x44999999)

#define VALID_VIEW_ACCESS       (FILE_MAP_READ|FILE_MAP_WRITE|FILE_MAP_EXECUTE)

#define IsViewWritable(pView)   ((pView)->wFlags & FILE_MAP_WRITE)
#define IsViewExecutable(pView) ((pView)->wFlags & FILE_MAP_EXECUTE)

FORCEINLINE WORD PageProtectionFromAccess (DWORD dwAccess)
{
    return (dwAccess & FILE_MAP_EXECUTE)
            ? ((dwAccess & FILE_MAP_WRITE)? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ)
            : ((dwAccess & FILE_MAP_WRITE)? PAGE_READWRITE : PAGE_READONLY);
}

// Used to implement PageOutFile, and enable page-out even when there's no memory
static LockedProcessList_t* g_pLockedProcessList;
static DWORD g_NumProcessesToSkip;

// we'll grab a max of 64 dirty pages at a time
#define MAX_DIRTY   64

// max number of views on a map file - due to we use a byte to ref-count physical page
#define MAX_VIEW_PER_MAP    20000


static BOOL MAPClose (PFSMAP pfsmap);
static BOOL FlushFile (PFSMAP pfsmap);


static void LockMap (PFSMAP pfsmap)
{
    DEBUGCHK (OwnCS (&MapCS));
    LockHDATA (pfsmap->phdMap);
}

static void UnlockMap (PFSMAP pfsmap)
{
    UnlockHandleData (pfsmap->phdMap);
}


// First phase of destroying a view, decommit and free the process VM.  Returns
// TRUE if the view was freed.
static void ReleaseViewInProcess (PPROCESS pprc, PMAPVIEW pview)
{
    PFSMAP pfsmap = (PFSMAP) pview->phdMap->pvObj;

    if (pview->base.pBaseAddr) {
        
        // Decommit the pages from the view while properly updating the refcounts
        // in the mapfile page tree
        if (!(MAP_DIRECTROM & pfsmap->dwFlags)) {
            VERIFY (MapVMDecommit (pfsmap, pprc, pview->base.pBaseAddr,
                                   pview->base.liMapOffset,
                                   pview->base.cbSize, TRUE));
            // NOTE: There is no race condition where a page can be committed
            // between the call to MapVMDecommit and VMFreeAndRelease, because
            // this thread is freeing the last possible lock on the view.
        }

        if (pfsmap->phdFile) {
            //
            // file backed mapfile, release VM.
            //
            VERIFY (VMFreeAndRelease (pprc, pview->base.pBaseAddr, pview->base.cbSize));
            pview->base.pBaseAddr = NULL;
            
        } else if (!(pfsmap->dwFlags & MAP_FILECACHE)
                && pview->rambacked.pViewCntOfMapInProc
                && MAP_VALIDATE_SIG (pview->rambacked.pViewCntOfMapInProc, MAP_VIEWCOUNT_SIG)
                && !InterlockedDecrement (&pview->rambacked.pViewCntOfMapInProc->count)) {

            //
            // last view of a RAM backed mapfile in process. Release VM for the whole mapfile.
            //
            if (pprc != g_pprcNK) {  // RAM-backed only release outside NK
                // RAM-backed allocs VM for the whole mapping
                DEBUGCHK (pfsmap->pUsrBase);
                VERIFY (VMFreeAndRelease (pprc, pfsmap->pUsrBase, pfsmap->liSize.LowPart));
                pview->base.pBaseAddr = NULL;
            }    
            DEBUGMSG (ZONE_MAPFILE, (L"Freeing pViewCntOfMapInProc %8.8lx\r\n",
                                     pview->rambacked.pViewCntOfMapInProc));
            FreeMem (pview->rambacked.pViewCntOfMapInProc, HEAP_VIEWCNTOFMAPINPROC);
            pview->rambacked.pViewCntOfMapInProc = NULL;
        }
    } else {
        DEBUGCHK (pfsmap->phdFile || (pfsmap->dwFlags & MAP_FILECACHE) || !pview->rambacked.pViewCntOfMapInProc);
    }
}


// Second phase of destroying a view
static void ReleaseView (PPROCESS pprc, PMAPVIEW pview)
{
    PFSMAP pfsmap = (PFSMAP) pview->phdMap->pvObj;
    
    DEBUGCHK (!OwnCS (&MapCS));
    
    CELOG_MapFileViewClose(pprc, pview);

    // Free the flush buffer
    if (pfsmap->phdFile && pview->flush.cbFlushSpace) {
        PVOID pBuffer = (MAP_GATHER & pfsmap->dwFlags)
                        ? (PVOID) pview->flush.gather.prgSegments
                        : pview->flush.nogather.prgPages;
        if (pBuffer) {
            VERIFY (VMFreeAndRelease (g_pprcNK, pBuffer, pview->flush.cbFlushSpace));
        }
    }

    // pview->base.pBaseAddr is freed in ReleaseViewInProcess

    MAP_VALIDATE_SIG (pfsmap, MAP_FSMAP_SIG);
    InterlockedDecrement ((LONG*)&pfsmap->dwNumViews);
    UnlockHandleData (pview->phdMap);
    MAP_VALIDATE_SIG (pview, MAP_VIEW_SIG);
    FreeMem (pview, HEAP_MAPVIEW);
}


//
// Lock a view - return the new ref count of the view
//
static DWORD LockView (PMAPVIEW pview)
{
    DEBUGCHK (OwnCS (&MapCS));
    MAP_VALIDATE_SIG (pview, MAP_VIEW_SIG);
    DEBUGCHK (MAP_LOCK_COUNT (pview->dwRefCount) < MAP_MAX_VIEW_REFCOUNT);
    pview->dwRefCount += MAP_LOCK_INCR;
    return (pview->dwRefCount);
}


//
// Unlock a view - return the new ref count of the view
//
static DWORD UnlockView (PPROCESS pprc, PMAPVIEW pview)
{
    DEBUGCHK (OwnCS (&MapCS));

    MAP_VALIDATE_SIG (pview, MAP_VIEW_SIG);
    DEBUGCHK (MAP_LOCK_COUNT(pview->dwRefCount) > 0);
    pview->dwRefCount -= MAP_LOCK_INCR;

    if (!pview->dwRefCount) {
        // Time to destroy the view.
        ReleaseViewInProcess (pprc, pview);

        // the debugchk is no longer valid. the situation can happen during process exit, where we already removed
        // the view from the linked list.
        // DEBUGCHK ((pview->link.pBack != &pview->link) && (pview->link.pFwd != &pview->link));  // check for double-remove
        RemoveDList (&pview->link);
        InitDList (&pview->link);  // point at self for safety against double-remove

        // NOTE: We cannot unlock the handle of the mapfile here because
        //       it might close the file behind the mapfile. Since we're
        //       holding the mapfile CS here, it's the caller's responsibility
        //       to call ReleaseView if UnlockView returns 0.
        //
        //       Since the view had already being removed from the viewList, it
        //       is safe to access pview later without locking it.
    }

    return pview->dwRefCount;
}


typedef struct {
    DWORD  dwAddr;
    DWORD  cbSize;
    DWORD  dwFlags;
} EnumLockViewStruct, *PEnumLockViewStruct;

#define LVA_EXACT           0x1     // address must be exact (cbSize ignored)
#define LVA_WRITE_ACCESS    0x2     // require write access


static BOOL TestAddrInView (PDLIST pItem, LPVOID pEnumData)
{
    PMAPVIEW pview = (PMAPVIEW) pItem;
    PEnumLockViewStruct pelvs = (PEnumLockViewStruct) pEnumData;
    
    // Find out actual view base/size.  We do not require views to be block
    // aligned, thus the base address of view is base + view offset inside 64K
    // block, and the size is the mapping size - view offset inside 64K block
    DWORD  dwViewBase = (DWORD) pview->base.pBaseAddr + pview->wViewOffset;
    DWORD  dwViewSize = pview->base.cbSize - pview->wViewOffset;

    return (MAP_VIEW_COUNT (pview->dwRefCount)                  // view is not being destroyed
            && (!(LVA_WRITE_ACCESS & pelvs->dwFlags) || IsViewWritable (pview))   // writeable if write requested
            && (!(LVA_EXACT & pelvs->dwFlags) || (pelvs->dwAddr == dwViewBase))  // exact base if requested
            && (pelvs->cbSize <= dwViewSize)                    // valid size (need this test to prevent integer overflow)
            && (pelvs->dwAddr - dwViewBase < dwViewSize)        // start address in range
            && (pelvs->dwAddr + pelvs->cbSize - dwViewBase <= dwViewSize)); // end address in range
}


//
// LockViewByAddr - given an address find the view of the address
//
// NOTE: Because of overlapping view support and RAM-backed mapfile virtual
// address sharing, it is possible that two views of RAM-backed mapfiles have
// overlapping in virtual addresses.  Therefore we need to do all the testing
// in TestAddrInView such that we can find the best view.  For example, maybe
// one view is being destroyed, but there's another view that fits our request.
//
static PMAPVIEW LockViewByAddr (PPROCESS pprc, PDLIST pviewList, LPVOID pAddr, DWORD cbSize, DWORD dwFlags)
{
    PMAPVIEW pview;
    EnumLockViewStruct elvs = { (DWORD) pAddr, cbSize, dwFlags };

    DEBUGCHK (OwnCS (&MapCS));

    pview = (PMAPVIEW) EnumerateDList (pviewList, TestAddrInView, &elvs);
    if (pview) {
        // Sanity check CS ordering rules now that we know what view it is
        DEBUGCHK (!OwnCS(&((PFSMAP)(pview->phdMap->pvObj))->pgtree.csVM) && !OwnCS(&pprc->csVM));

        VERIFY (LockView (pview));
    }

    return pview;
}


//
// Non-pageable file, read the whole file
//
static BOOL MAPReadFile (PFSMAP pfsmap)
{
    BOOL fRet = FALSE;
    LPVOID pFileBuf;

    DEBUGCHK (!OwnCS (&MapCS));
    DEBUGMSG (ZONE_MAPFILE, (L"MAPReadFile: Reading entire non-pageable file\r\n"));
    DEBUGCHK (!pfsmap->liSize.HighPart);
    DEBUGCHK (!(pfsmap->dwFlags & MAP_FILECACHE));

    // Reserve a temporary buffer to copy with
    pFileBuf = VMAlloc (g_pprcNK, NULL, pfsmap->liSize.LowPart, MEM_COMMIT, PAGE_READWRITE);
    if (pFileBuf) {
        DWORD cbRead;
        if ((PHD_SetFilePointer (pfsmap->phdFile, 0, 0, FILE_BEGIN) != (DWORD)-1)
            && PHD_ReadFile (pfsmap->phdFile, pFileBuf, pfsmap->liSize.LowPart, &cbRead, NULL)
            && (cbRead == pfsmap->liSize.LowPart)) {

            // Read successfully, so copy to mapfile page tree
            fRet = MapVMFill (pfsmap, g_pprcNK, pFileBuf, cbRead);
        }

        // Successful or not, release the VM allocated.  If successful the pages
        // are still in the page tree.
        VERIFY (VMFreeAndRelease (g_pprcNK, pFileBuf, pfsmap->liSize.LowPart));
    }

    DEBUGMSG (ZONE_MAPFILE, (L"MAPReadFile: Returns %8.8lx\r\n", fRet));

    return fRet;
}


#ifdef DEBUG
static int DumpFSMap (PFSMAP pfsmap)
{
    NKLOG (pfsmap->dwFlags);
    NKLOGI64 (pfsmap->liSize.QuadPart);
    NKLOG (pfsmap->phdFile);
    if (!pfsmap->phdFile) {
        NKLOG (pfsmap->pUsrBase);
        NKLOG (pfsmap->pKrnBase);
    } else {
        NKLOG (pfsmap->pROMBase);
    }
    NKLOG (pfsmap->pgtree.RootEntry);
    NKLOG (pfsmap->pgtree.height);
    return 0;
}

static int DumpMapView (PMAPVIEW pview)
{
    NKLOGI64 (pview->base.liMapOffset.QuadPart);
    NKLOG (pview->base.pBaseAddr);
    NKLOG (pview->base.cbSize);
    NKLOG (pview->wViewOffset);
    NKLOG (pview->phdMap);
    NKLOG (pview->wFlags);
    return 0;
}
#endif


//
// InitFSMAP - initialize FSMAP structure, setup flags based on the arguments
//
static DWORD InitFSMAP (
    PFSMAP pfsmap,
    PHDATA phdFile,
    DWORD  flProtect,
    ULARGE_INTEGER* pliSize
    )
{
    DWORD dwErr = 0;
    BY_HANDLE_FILE_INFORMATION bhi;

    memset(pfsmap, 0, sizeof(FSMAP));
    MAP_SET_SIG (pfsmap, MAP_FSMAP_SIG);
    pfsmap->phdFile = phdFile;
    InitDList (&pfsmap->maplink);

    DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
                             pfsmap, phdFile, flProtect, pliSize->HighPart, pliSize->LowPart));

    if ((flProtect & ~MAP_VALID_PROT)
         || (0 == (pfsmap->dwFlags = (flProtect & (PAGE_READONLY | PAGE_READWRITE))))) {
        dwErr = ERROR_INVALID_PARAMETER;

    // Special handling for cache mappings
    } else if (flProtect & PAGE_INTERNALCACHE) {
        // Ideally we'd check if the volume supports scatter-gather and use
        // use WriteFileGather when possible.  However WFG requires writes
        // to be a multiple of the page size, which is a tiny minority of
        // files.  So for now we just use Read/WriteFileWithSeek.
        DEBUGCHK (!pfsmap->phdFile);
        pfsmap->dwFlags |= (MAP_FILECACHE | MAP_PAGEABLE);  // Pageability checked by filesys
        pfsmap->liSize = *pliSize;
        
        // This value will be initialized by CACHE_CreateMapping
        pfsmap->FSSharedFileMapId = 0;

    // Check if it's a memory-backed mapfile
    } else if (!phdFile) {
        // Memory backed
        if (pliSize->HighPart || ((int) pliSize->LowPart <= 0)
            || (PAGE_READWRITE != pfsmap->dwFlags)) {
            dwErr = ERROR_INVALID_PARAMETER;
        } else {
            pfsmap->liSize = *pliSize;
            pfsmap->dwFlags |= MAP_PAGEABLE;
        }

    // File-backed, verify if the file can be accessed based on the protection
    } else if (!PHD_GetFileInfo (phdFile, &bhi)) {
        dwErr = ERROR_INVALID_HANDLE;

    // Save memory by mapping directly from ROM, if the right conditions exist.
    } else if ((bhi.dwFileAttributes & FILE_ATTRIBUTE_INROM)
               && !(bhi.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED)
               && IS_ROMFILE_OID (bhi.dwOID)) {

        DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap: ROM File\r\n"));

        // pliSize is ignored
        if (pfsmap->dwFlags & PAGE_READWRITE) {
            // Can't write to ROM file
            dwErr = ERROR_ACCESS_DENIED;
        
        } else {
            DWORD dwType, dwFileIndex;
            ROMFILE_FROM_OID (bhi.dwOID, dwType, dwFileIndex);

            if (FindROMFile (dwType, dwFileIndex, &pfsmap->pROMBase, &pfsmap->liSize.LowPart)) {
                pfsmap->dwFlags |= (MAP_DIRECTROM | MAP_PAGEABLE);
                DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap: uncompressed ROM file, base = %8.8lx, length = %8.8lx\r\n",
                                         pfsmap->pROMBase, pfsmap->liSize.LowPart));
            } else {
                // Filesys told us the file is in ROM and not compressed, but
                // we can't find it in ROM???
                DEBUGCHK(0);
                dwErr = ERROR_FILE_NOT_FOUND;
            }
        }

    } else if ((bhi.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
               && (flProtect & PAGE_READWRITE)) {
        // Trying to open read/write on read-only file
        dwErr = ERROR_ACCESS_DENIED;

    } else {
        // Regular file
        ULARGE_INTEGER liFileSize;

        DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap: Regular File\r\n"));
        
        if (flProtect & PAGE_INTERNALDBMAPPING) {
            pfsmap->dwFlags |= MAP_ATOMIC;
        }

        if (flProtect & PAGE_FIX_ADDRESS_READONLY) {
            pfsmap->dwFlags |= MAP_USERFIXADDR;
        }
        
        //
        // Update length
        //

        if (!PHD_GetLargeFileSize (phdFile, &liFileSize)) {
            DEBUGCHK(0);  // Why would this fail?
            dwErr = ERROR_INVALID_PARAMETER;
            goto exit;
        }
        if (!pliSize->QuadPart) {
            if (!liFileSize.QuadPart) {
                dwErr = ERROR_INVALID_PARAMETER;
                goto exit;
            }
            pliSize->QuadPart = liFileSize.QuadPart;
        }
        
        // Expand file if necessary
        // For atomic maps we allow the file size to be smaller than the
        // map size, and leave management to filesys (see FSVolExtendFile)
        if ((pliSize->QuadPart > liFileSize.QuadPart) && !(pfsmap->dwFlags & MAP_ATOMIC)) {
            if (!(flProtect & PAGE_READWRITE)) {
                // Read-only, cannot modify the file
                DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap: Unable to expand read-only file\r\n"));
                dwErr = ERROR_ACCESS_DENIED;
                goto exit;
            } else if (!PHD_SetLargeFilePointer (phdFile, *pliSize)
                       || !PHD_SetEndOfFile (phdFile)) {
                DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap: Unable to expand file\r\n"));
                dwErr = ERROR_DISK_FULL;
                goto exit;
            }
        }

        pfsmap->liSize.QuadPart = pliSize->QuadPart;


        //
        // Check paging
        //

        // Note, in previous OS versions we checked that RF/WF or RFWS/WFWS
        // succeeded on the file handle, and failed CreateFileMapping if the
        // desired access was unavailable.  Now we only check for pageability.
        // The user may succeed mapping a file handle that does not have the
        // desired capability, and only fail later when trying to page that way.
        // The primary reason for this change is to avoid the negative perf and
        // disk-wear issues incurred from the writes.
        if (PHD_ReadFileWithSeek (phdFile, 0, 0, 0, 0, 0, 0)) {
            pfsmap->dwFlags |= MAP_PAGEABLE;
        } else {
            DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap: ReadFileWithSeek failed, file not pageable\r\n"));
            
            // Atomic and fix-addr maps require paging
            if (pfsmap->dwFlags & (MAP_ATOMIC|MAP_USERFIXADDR)) {
                dwErr = ERROR_NOT_SUPPORTED;
                goto exit;
            }
        }

        // Verify read/write access.
        if (liFileSize.QuadPart) {
            BYTE  testbyte;
            DWORD cbReadWrite;

            if (pfsmap->dwFlags & MAP_PAGEABLE) {
                // Paging requires RFWS/WFWS
                if (!PHD_ReadFileWithSeek (phdFile, &testbyte, sizeof(BYTE), &cbReadWrite, NULL, 0, 0)
                    || (sizeof(BYTE) != cbReadWrite)
                    || ((pfsmap->dwFlags & PAGE_READWRITE)
                        && (!PHD_WriteFileWithSeek (phdFile, &testbyte, sizeof(BYTE), &cbReadWrite, NULL, 0, 0)
                            || (sizeof(BYTE) != cbReadWrite)))) {
                    DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap: Insufficient file access\r\n"));
                    dwErr = ERROR_ACCESS_DENIED;
                    goto exit;
                }
            } else {
                // Non-paging requires ReadFile/WriteFile
                if (((DWORD)-1 == PHD_SetFilePointer (phdFile, 0, NULL, FILE_BEGIN))
                    || !PHD_ReadFile (phdFile, &testbyte, sizeof(BYTE), &cbReadWrite, NULL)
                    || (sizeof(BYTE) != cbReadWrite)
                    || ((pfsmap->dwFlags & PAGE_READWRITE)
                        && (((DWORD)-1 == PHD_SetFilePointer (phdFile, 0, NULL, FILE_BEGIN))
                            || !PHD_WriteFile (phdFile, &testbyte, sizeof(BYTE), &cbReadWrite, NULL)
                            || (sizeof(BYTE) != cbReadWrite)))) {
                    DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap: Insufficient file access\r\n"));
                    dwErr = ERROR_ACCESS_DENIED;
                    goto exit;
                }
            }
        }

        //
        // Disable Caching
        //

        // Allowing a memory-mapped file to be cached would not only hurt perf
        // and memory usage (since the data is duplicated between the cache and
        // the memory-mapped file), it also could hang the page pool trim thread.
        // Because that thread would have to write back data through the pool,
        // and it could run out of pool memory to write back with.
        //
        // NOTE: caching must be disabled before enabling file data transactioning
        // (below) because Cache Manager does not allow transactioning on a cached
        // file.
        { 
            FILE_CACHE_INFO cacheinfo;
            cacheinfo.fInfoLevelId = FileCacheDisableStandard;
            PHD_FSIoctl (phdFile, FSCTL_SET_FILE_CACHE, (LPVOID) &cacheinfo,
                         sizeof(FILE_CACHE_INFO), NULL, 0, NULL, NULL);
        }

        // Paging out could also possibly be done using WriteFileGather.
        // WFG only does page-size writes, so map length must be page-aligned.
        if ((pfsmap->dwFlags & PAGE_READWRITE) && (pfsmap->dwFlags & MAP_PAGEABLE)
            && ((pfsmap->liSize.LowPart % VM_PAGE_SIZE) == 0)) {
            // Check if the file system supports gathered writes
            CE_VOLUME_INFO volinfo;
            CE_VOLUME_INFO_LEVEL level;
            DWORD dwExtendedFlags, cbReturned;

            level = CeVolumeInfoLevelStandard;
            volinfo.cbSize = sizeof(CE_VOLUME_INFO);
            if (PHD_FSIoctl (phdFile, IOCTL_FILE_GET_VOLUME_INFO,
                             (LPVOID) &level, sizeof(CE_VOLUME_INFO_LEVEL),
                             (LPVOID) &volinfo, sizeof(CE_VOLUME_INFO),
                             &cbReturned, NULL)
                && (cbReturned == sizeof(CE_VOLUME_INFO))
                && (volinfo.dwFlags & CE_VOLUME_FLAG_WFSC_SUPPORTED)) {

                // Atomic maps also require volume transactioning, and need
                // to turn on data transactioning for the file, to ensure
                // that gathered writes are atomic.
                dwExtendedFlags = CE_FILE_FLAG_TRANS_DATA;
                if (!(pfsmap->dwFlags & MAP_ATOMIC)
                    || ((volinfo.dwFlags & CE_VOLUME_TRANSACTION_SAFE)
                        && PHD_FSIoctl (phdFile, FSCTL_SET_EXTENDED_FLAGS,
                                        (LPVOID) &dwExtendedFlags, sizeof(DWORD),
                                        NULL, 0, NULL, NULL))) {
                    pfsmap->dwFlags |= MAP_GATHER;
                } else {
                    DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap: Transacted writes unavailable.\r\n"));
                }
            } else {
                DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap: IOCTL_FILE_GET_VOLUME_INFO: WFSC unsupported.\r\n"));
            }
        } else {
            // Atomic maps should always be paged and always page-aligned in length
            DEBUGCHK(!(pfsmap->dwFlags & MAP_ATOMIC));
        }
        DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap: WriteFileGather=%u\r\n",
                                 (pfsmap->dwFlags & MAP_GATHER) ? TRUE : FALSE));
    }

    if (!dwErr && !(MAP_DIRECTROM & pfsmap->dwFlags)) {
        // Created a new map file, prepare additional resources
        
        if (!MapVMInit (pfsmap)) {
            DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap: MapVMInit failed\r\n"));
            dwErr = ERROR_OUTOFMEMORY;

        } else if (!(MAP_PAGEABLE & pfsmap->dwFlags)
                   && !MAPReadFile (pfsmap)) {
            // Non-pageable, but failed to read the whole file
            DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap: Failed to read non-pageable file into memory\r\n"));
            dwErr = ERROR_OUTOFMEMORY;

        } else {
            // An atomic file might have been closed partway through a
            // flush.  It must be validated before it can be used.
            if (pfsmap->dwFlags & MAP_ATOMIC) {
                dwErr = ValidateFile (pfsmap);
            }
        }
    }

exit:
    DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap returns, dwErr = %8.8lx",
                             dwErr, DumpFSMap (pfsmap)));
    return dwErr;
}


//---------------------------------------------------------------------
// NKCreateFileMapping - Create a file mapping object
//---------------------------------------------------------------------
HANDLE WINAPI
NKCreateFileMapping (
    PPROCESS pprc,
    HANDLE   hFile, 
    LPSECURITY_ATTRIBUTES lpsa, 
    DWORD    flProtect, 
    DWORD    dwMaximumSizeHigh, 
    DWORD    dwMaximumSizeLow, 
    LPCTSTR  lpName 
    )
{
    HANDLE hMap = NULL;
    DWORD  dwErr = 0;
    PNAME  pMapName = NULL;
    DWORD dwRequestedAccess = FILE_GENERIC_READ | ((flProtect & PAGE_READWRITE) ? FILE_GENERIC_WRITE : 0);

    DEBUGMSG (ZONE_MAPFILE, (L"NKCreateFileMapping %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %s\r\n",
        pprc, hFile, lpsa, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, lpName));

    // Only the kernel process can create cache maps, and they must be unnamed
    if ((flProtect & PAGE_INTERNALCACHE) && ((pprc != g_pprcNK))) {
        DEBUGCHK(0);
        dwErr = ERROR_INVALID_PARAMETER;

    } else if (lpName) {

        // validate parameters
        if ((flProtect & (PAGE_INTERNALCACHE|PAGE_INTERNALDBMAPPING))   // internal mapping cannot be named
            || !wcscmp (lpName, L"\\")                                  // "\\" is not a valid name
            || (NULL == (pMapName = DupStrToPNAME (lpName)))) {   // other invalid name
            dwErr = ERROR_INVALID_PARAMETER;
        } else {

            // try to find existing named mapfile object
            hMap = HNDLCreateNamedHandle (&cinfMap, NULL, pprc, dwRequestedAccess, pMapName, KERNEL_FSMAP, FALSE, &dwErr);
            DEBUGCHK (!hMap || (ERROR_ALREADY_EXISTS == dwErr));
            if (!hMap) {
                // couldn't find existing mapfile, reset dwErr so we'll try to create one.
                dwErr = 0;
            }
        }
    }

    if (!dwErr) {
        ULARGE_INTEGER liSize;
        PHDATA phdFile = NULL;
        PFSMAP pfsmap = NULL;

        // either un-named mapfile, or 1st instance of a named mapfile
        DEBUGCHK (!hMap);

        liSize.HighPart = dwMaximumSizeHigh;
        liSize.LowPart  = dwMaximumSizeLow;

        // fix address maps must be read-only and file-backed
        if (flProtect & PAGE_FIX_ADDRESS_READONLY) {
            flProtect |= PAGE_READONLY;
        }
        
        if ((flProtect & PAGE_FIX_ADDRESS_READONLY)
            && ((PAGE_READWRITE & flProtect) || (INVALID_HANDLE_VALUE == hFile))) {
            
            dwErr = ERROR_INVALID_PARAMETER;
            
        } else if ((INVALID_HANDLE_VALUE != hFile)
                && (NULL == (phdFile = LockHandleData (hFile, pActvProc)))) {
        
            dwErr = ERROR_INVALID_HANDLE;

        } else if ((NULL != (pfsmap = AllocMem (HEAP_FSMAP)))
                && (0 == (dwErr = InitFSMAP (pfsmap, phdFile, flProtect, &liSize)))) {

            CELOG_MapFileCreate(pfsmap, pMapName);

            hMap = pMapName
                ? HNDLCreateNamedHandle (&cinfMap, pfsmap, pprc, dwRequestedAccess, pMapName, KERNEL_FSMAP, TRUE, &dwErr)
                : HNDLCreateHandle (&cinfMap, pfsmap, pprc, 0);
        }

        if (!hMap && !dwErr) {
            dwErr = ERROR_OUTOFMEMORY;
        }

        if (!dwErr) {
            // Get pointer to HDATA by locking the handle
            VERIFY (NULL != (pfsmap->phdMap = LockHandleData (hMap, pprc)));
            
            EnterCriticalSection (&MapCS);
            AddToDListHead (&g_MapList, &pfsmap->maplink);
            LeaveCriticalSection (&MapCS);
        
            UnlockMap (pfsmap);
            
        } else if (pfsmap) {
            MAPClose(pfsmap);
            
        } else if (phdFile) {
            UnlockHandleData (phdFile);
        }
        
    }

    DEBUGMSG (ZONE_MAPFILE, (L"NKCreateFileMapping hMap = %8.8lx dwErr = %8.8lx\r\n", hMap, dwErr));
    
    if (dwErr) {
        DEBUGCHK (!hMap || (ERROR_ALREADY_EXISTS == dwErr));

        if (pMapName) {
            FreeName (pMapName);
        }
    }

    // Set lasterr even on success, so that callers know if the map already existed
    KSetLastError (pCurThread, dwErr);

    DEBUGMSG (ZONE_MAPFILE, (L"NKCreateFileMapping returns %8.8lx, dwErr = %8.8lx\r\n", hMap, dwErr));

    return hMap;    
}


// Flushing buffers are allocated up-front when the view is created, so that
// low memory conditions cannot cause flush failure.
static BOOL PrepareForFlush(PMAPVIEW pview, DWORD dwMapFlags)
{
    PVOID pBuffer;

    DEBUGCHK (((PFSMAP) pview->phdMap->pvObj)->phdFile);

    pview->flush.dwNumElements = PAGEALIGN_UP(pview->base.cbSize) / VM_PAGE_SIZE;

    // Calculate the space needed for flushing 1 page
    if (MAP_GATHER & dwMapFlags) {
        // sizeof(ULARGE_INTEGER) + sizeof(FILE_SEGMENT_ELEMENT) = 16B of params per map page
        // ==> 4KB of params per 1MB of data
        pview->flush.cbFlushSpace = sizeof(ULARGE_INTEGER) + sizeof(FILE_SEGMENT_ELEMENT);
    } else {
        // sizeof(PBYTE) = 4B of params per map page
        // ==> 4KB of params per 4MB of data
        pview->flush.cbFlushSpace = sizeof(PBYTE);
    }

    pview->flush.cbFlushSpace *= pview->flush.dwNumElements;             // Times max number of pages that might be flushed at once
    pview->flush.cbFlushSpace = PAGEALIGN_UP(pview->flush.cbFlushSpace); // Rounded up to nearest page boundary
    
    pBuffer = VMAlloc(g_pprcNK, 0, pview->flush.cbFlushSpace,
                      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (pBuffer) {
        if (MAP_GATHER & dwMapFlags) {
            pview->flush.gather.prgSegments  = (FILE_SEGMENT_ELEMENT*) pBuffer;
            pview->flush.gather.prgliOffsets = (ULARGE_INTEGER*) (pview->flush.gather.prgSegments
                                                                  + pview->flush.dwNumElements);
        } else {
            pview->flush.nogather.prgPages   = pBuffer;
        }
        
        DEBUGMSG (ZONE_MAPFILE, (L"PrepareForFlush: Flush buffer size=0x%08x\r\n",
                                 pview->flush.cbFlushSpace));
        return TRUE;
    }
    
    return FALSE;
}

#define EVF_OVERLAP_EXIST       0x1
#define EVF_PROTECTION_CHANGED  0x2

typedef struct {
    PMAPVIEW     pview;
    BOOL         dwFlags;
    ViewCount_t* pViewCntOfMapInProc;
} EnumFindViewStruct, *PEnumFindViewStruct;


static BOOL EnumExistingView (PDLIST pItem, LPVOID pEnumData)
{
    PEnumFindViewStruct pefvs = (PEnumFindViewStruct) pEnumData;
    PMAPVIEW    pExistingView = (PMAPVIEW) pItem;
    PMAPVIEW         pNewView = pefvs->pview;
    PFSMAP             pfsmap = (PFSMAP) pNewView->phdMap->pvObj;
    BOOL                 fRet;

    // For memory backed view, they're always in same address if offset is the same, the following 
    // 3 calls returns exactly the same value
    //      MapViewOfFile (hMap, 0, FILE_MAP_WRITE, 0, 0, VM_PAGE_SIZE);    // map a page r/w
    //      MapViewOfFile (hMap, 0, FILE_MAP_WRITE, 0, 0, 0);               // map whole file r/w
    //      MapViewOfFile (hMap, 0, FILE_MAP_READ, 0, 0, 2*VM_PAGE_SIZE);   // map 2 pages r/o
    //
    // when UnmapViewOfFile is called (it only takes a view address), we really don't know which of 
    // the above 3 views to unmap. The only solution is to consider views to be the same as long as
    // they are of the same offset, and 
    //      (1) use the maximum size as the view size
    //      (2) view is r/w as long as there is a r/w view of the same offset.
    //
    // File backed view doesn't have the problem as they can be mapped in different addresses. However,
    // we still want to 'reuse view' if possible. i.e. don't create a new view if there is an existing
    // view of exactly the same settting.
    //
    DEBUGCHK (OwnCS (&MapCS));
    DEBUGCHK (!(pfsmap->dwFlags & MAP_FILECACHE));  // Cache maps don't have kernel views

    if (!pfsmap->phdFile && (pNewView->phdMap == pExistingView->phdMap)) {
        pefvs->pViewCntOfMapInProc = pExistingView->rambacked.pViewCntOfMapInProc;
    }

    fRet =  ( MAP_VIEW_COUNT (pExistingView ->dwRefCount)                  // view is not being destroyed
		    && (pNewView->phdMap == pExistingView->phdMap)
            && (pExistingView->wViewOffset == pNewView->wViewOffset)
            && (pExistingView->base.liMapOffset.QuadPart == pNewView->base.liMapOffset.QuadPart));

    if (pfsmap->phdFile) {
        // file backed - size and protection must match
        fRet = fRet
               && (pExistingView->base.cbSize == pNewView->base.cbSize)
               && (pExistingView->wFlags == pNewView->wFlags);

    } else if (fRet) {

        WORD wAccess = 0;

        // Memory backed, with existing view found.
        // Update the existing view if necessary.
        if (pExistingView->base.cbSize < pNewView->base.cbSize) {
            pExistingView->base.cbSize = pNewView->base.cbSize;
        }

        // make the access union of both access.
        wAccess = pExistingView->wFlags | pNewView->wFlags;

        if (wAccess != pExistingView->wFlags) {
            // protection of view changed
            pExistingView->wFlags = wAccess;
            pefvs->dwFlags |= EVF_PROTECTION_CHANGED;
        }
        
    }

#ifdef ARM

    // ARM VIVT specific, need to find if there is any overlapped view such that
    // we know if we need to turn view uncached.
    else if (IsVirtualTaggedCache ()
        && (pNewView->phdMap == pExistingView->phdMap)
        && (pExistingView->base.liMapOffset.QuadPart + pExistingView->base.cbSize > pNewView->base.liMapOffset.QuadPart)
        && (pExistingView->base.liMapOffset.QuadPart < pNewView->base.liMapOffset.QuadPart + pNewView->base.cbSize)) {
        pefvs->dwFlags |= EVF_OVERLAP_EXIST;
    }

#endif

    return fRet;
}

#ifdef ARM
static BOOL UncacheViews (PDLIST pItem, LPVOID pEnumData)
{
    // do nothing. all r/w mapfiles are uncached already
    return TRUE;
}

// currently all r/w mapfile are uncached already. We only need to
// check if the address is in any view and return TRUE if it is
BOOL MAPUncacheViews (PPROCESS pprc, DWORD dwAddr, DWORD cbSize)
{
    BOOL fRet;
    EnumLockViewStruct elvs = { dwAddr, cbSize, 0 };

    EnterCriticalSection (&MapCS);
    fRet = (BOOL) EnumerateDList (&pprc->viewList, TestAddrInView, &elvs);
    LeaveCriticalSection (&MapCS);        
     
    return fRet;
}


#endif

// Used to determine if there's an existing view we should use instead of
// creating a new one.
static PMAPVIEW
FindExistingView (
    PPROCESS      pprc,
    PMAPVIEW      pNewview,
    ViewCount_t** ppViewCntOfMapInProc
    )
{
    PMAPVIEW pExistingview;
    EnumFindViewStruct efvs = { pNewview, 0, NULL };

    DEBUGCHK (OwnCS (&MapCS));
    pExistingview = (PMAPVIEW) EnumerateDList (&pprc->viewList, EnumExistingView, &efvs);
    if (ppViewCntOfMapInProc) {
        *ppViewCntOfMapInProc = efvs.pViewCntOfMapInProc;
    }

    if ((EVF_PROTECTION_CHANGED & efvs.dwFlags) && pExistingview) {
        InvalidatePages (pprc, (DWORD) pExistingview->base.pBaseAddr, PAGECOUNT (pExistingview->base.cbSize));
    }
    
#ifdef ARM
    if ((EVF_OVERLAP_EXIST & efvs.dwFlags) && !pExistingview) {
        EnumerateDList (&pprc->viewList, UncacheViews, &efvs);
    }
#endif

    return pExistingview;
}


// Returns the view address on success
static LPVOID AddFileBackedViewToProcess (PPROCESS pprc, PFSMAP pfsmap, PMAPVIEW pview, LPDWORD pdwErr)
{
    PMAPVIEW pExistingView;
    DWORD    dwErr = 0;
    LPVOID   pAddr = NULL;
    
    // add the view to the view list
    DEBUGCHK (!OwnCS(&pfsmap->pgtree.csVM) && !OwnCS(&pprc->csVM));  // CS ordering rules
    DEBUGCHK (pfsmap->phdFile);   // memory backed mapfile handled in different function

    EnterCriticalSection (&MapCS);

    if (!IsViewCreationAllowed (pprc)) {
        // someone is creating a view on a process that is in the final stage of exiting
        DEBUGCHK (0);
        dwErr = ERROR_OUTOFMEMORY;
    } else {
        // Try to find an existing view to use instead
        pExistingView = FindExistingView (pprc, pview, NULL);
        if (pExistingView) {
            if (MAP_VIEW_COUNT(pExistingView->dwRefCount) < MAP_MAX_VIEW_REFCOUNT) {
                pExistingView->dwRefCount += MAP_VIEW_INCR;
                dwErr = ERROR_ALREADY_EXISTS;
                pAddr = pExistingView->base.pBaseAddr + pExistingView->wViewOffset;
            } else {
                // Found an existing view but its refcount is too high to increment
                dwErr = ERROR_OUTOFMEMORY;
            }

        } else {
            AddToDListHead (&pprc->viewList, &pview->link);
            pAddr = pview->base.pBaseAddr + pview->wViewOffset;
        }
    }
    LeaveCriticalSection (&MapCS);

    *pdwErr = dwErr;
    return pAddr;
}


//
// Map a view of a fix-address mapfile (ram-backed, or r/o fixed address map).
//
// For ram-backed mapfile, since all views addresses are fixed, we need to keep track of the # of views
// per-process, to know when to reserve/free VM on Map/UnmapViewOfFile.
// Otherwise, we can release the view VM while other views still use the VM.
//
static LPVOID MapFixedAddressView (PPROCESS pprc, PFSMAP pfsmap, PMAPVIEW pview, LPDWORD pdwErr)
{
    PMAPVIEW pExistingView;
    DWORD    dwErr = 0;
    LPVOID   pAddr = NULL;
    
    // Memory-backed mapping cannot exceed 4GB size
    DEBUGCHK (!pfsmap->liSize.HighPart);
    DEBUGCHK (!pview->base.liMapOffset.HighPart);
    DEBUGCHK (pfsmap->phdFile || (pfsmap->dwFlags & PAGE_READWRITE));
    DEBUGCHK (!(pfsmap->dwFlags & MAP_FILECACHE));

    DEBUGCHK (!OwnCS(&pfsmap->pgtree.csVM) && !OwnCS(&pprc->csVM));  // CS ordering rules
    EnterCriticalSection (&MapCS);

    if (!IsViewCreationAllowed (pprc)) {
        // someone is creating a view on a process that is in the final stage of exiting
        DEBUGCHK (0);
        dwErr = ERROR_OUTOFMEMORY;
        
    } else {

        // Try to find an existing view to use instead
        pExistingView = FindExistingView (pprc, pview, &pview->rambacked.pViewCntOfMapInProc);
        if (pExistingView) {
            if (MAP_VIEW_COUNT(pExistingView->dwRefCount) < MAP_MAX_VIEW_REFCOUNT) {
                pExistingView->dwRefCount += MAP_VIEW_INCR;
                dwErr = ERROR_ALREADY_EXISTS;
                pAddr = pExistingView->base.pBaseAddr + pExistingView->wViewOffset;
            } else {
                // Found an existing view but its refcount is too high to increment
                dwErr = ERROR_OUTOFMEMORY;
            }

            // NOTE: we don't increment *pViewCntOfMapInProc here. The value is
            //       to keep track of # of distinct view objects.
            DEBUGCHK (pfsmap->phdFile || pExistingView->rambacked.pViewCntOfMapInProc);
            pview->rambacked.pViewCntOfMapInProc = NULL;
        
        } else {
            // No existing view
            LPBYTE *ppMapBase;
            DWORD  dwSearchBase;
            ViewCount_t* pNewCount = NULL;  // Track if new memory was allocated

            if (pfsmap->phdFile) {
                // read-only fixed address mapfile
                DEBUGCHK (pprc != g_pprcNK);
                DEBUGCHK (MAP_USERFIXADDR & pfsmap->dwFlags);
                DEBUGCHK (!pview->rambacked.pViewCntOfMapInProc);

            } else if (pview->rambacked.pViewCntOfMapInProc) {
                MAP_VALIDATE_SIG (pview->rambacked.pViewCntOfMapInProc, MAP_VIEWCOUNT_SIG);
                DEBUGCHK (pview->rambacked.pViewCntOfMapInProc->count);
            } else if (NULL != (pNewCount = AllocMem (HEAP_VIEWCNTOFMAPINPROC))) {
                MAP_SET_SIG (pNewCount, MAP_VIEWCOUNT_SIG);
                pNewCount->count = 0;
                pview->rambacked.pViewCntOfMapInProc = pNewCount;
            } else {
                dwErr = ERROR_OUTOFMEMORY;
            }

            if (!dwErr) {
                if (pprc == g_pprcNK) {
                    dwSearchBase = 0;
                    ppMapBase = &pfsmap->pKrnBase;
                } else {
                    dwSearchBase = VM_RAM_MAP_BASE;
                    ppMapBase = &pfsmap->pUsrBase;
                }

                if (! *ppMapBase) {
                    // always reserve from kernel VM
                    *ppMapBase = VMReserve (g_pprcNK, pfsmap->liSize.LowPart, dwSearchBase? 0 : MEM_MAPPED, dwSearchBase);
                    DEBUGMSG (ZONE_MAPFILE && *ppMapBase,
                              (L"MapFixedAddressView: RAM-backed mapping at common VM range 0x%08x-0x%08x\r\n",
                               *ppMapBase, *ppMapBase + pfsmap->liSize.LowPart));
                }

                if (!*ppMapBase
                    || ((pprc != g_pprcNK) 
                        && (pNewCount == pview->rambacked.pViewCntOfMapInProc)          // newly allocated, or r/o fixed address, where both are NULL
                        && ! VMAlloc (pprc, pfsmap->pUsrBase, pfsmap->liSize.LowPart,   // Map user VM
                                      MEM_RESERVE | MEM_MAPPED, PAGE_NOACCESS))) {

                    // failed reserving per-process mapfile VM.
                    dwErr = ERROR_OUTOFMEMORY;
                    if (pNewCount) {
                        FreeMem (pNewCount, HEAP_VIEWCNTOFMAPINPROC);
                    }
                    pview->rambacked.pViewCntOfMapInProc = NULL;
                
                } else {

                    if (!pfsmap->phdFile) {
                        // ram-backed mapfile
                        MAP_VALIDATE_SIG (pview->rambacked.pViewCntOfMapInProc, MAP_VIEWCOUNT_SIG);
                        InterlockedIncrement (&pview->rambacked.pViewCntOfMapInProc->count);
                    }

                    pview->base.pBaseAddr = *ppMapBase + pview->base.liMapOffset.LowPart;
                    AddToDListHead (&pprc->viewList, &pview->link);
                    pAddr = pview->base.pBaseAddr + pview->wViewOffset;
                }
            }
        }
    }

    LeaveCriticalSection (&MapCS);

    *pdwErr = dwErr;

    return pAddr;
}


// Adjust parameters as necessary, and return an error value if invalid.
static DWORD ValidateViewParameters (
    PFSMAP pfsmap,
    DWORD* pdwAccess,
    WORD*  pwViewOffset,
    DWORD* pcbSize,
    ULARGE_INTEGER* pliMapOffset
    )
{
    if (FILE_MAP_ALL_ACCESS == *pdwAccess) {
        *pdwAccess = VALID_VIEW_ACCESS;
    }

    // The actual view may be created on a block or page boundary, earlier than
    // the start of the view that's returned to the user.
    if (MAP_DIRECTROM & pfsmap->dwFlags) {
        // Direct-ROM mapfile views are created on a page boundary
        DEBUGCHK (!pliMapOffset->HighPart);
        *pwViewOffset = (WORD) (((DWORD) pfsmap->pROMBase + pliMapOffset->LowPart) & VM_PAGE_OFST_MASK);

    } else {
        // fix address map must be mapped with offset 0 and size 0
        if ((MAP_USERFIXADDR & pfsmap->dwFlags) 
            && (*pcbSize || pliMapOffset->QuadPart)) {
            return ERROR_INVALID_PARAMETER;
        }
    
        // Ordinary mapfile views are created on a block boundary
        *pwViewOffset = (WORD) (pliMapOffset->LowPart & VM_BLOCK_OFST_MASK);
    }

    // View size of 0 means the view goes to the end of the mapping
    if (!*pcbSize) {
        ULARGE_INTEGER liSize;

        liSize.QuadPart = pfsmap->liSize.QuadPart - pliMapOffset->QuadPart;
        if (liSize.HighPart) {
            // It's impossible to map a >4GB view into RAM.
            return ERROR_OUTOFMEMORY;
        }
        *pcbSize = liSize.LowPart;
    }

    // Validate parameters
    if (!*pcbSize                                                        // size=0 illegal, would fail TestAddrInView
        || (*pcbSize > pfsmap->liSize.QuadPart)                          // size > mapfile size
        || (pliMapOffset->QuadPart > pfsmap->liSize.QuadPart)            // offset > mapfile size
        || (pliMapOffset->QuadPart > pfsmap->liSize.QuadPart - *pcbSize) // (offset+size) exceeds mapfile size
        || (*pdwAccess & ~VALID_VIEW_ACCESS)                             // invalid access
        || ((FILE_MAP_WRITE & *pdwAccess)
            && !(PAGE_READWRITE & pfsmap->dwFlags))) {                   // R/W view of R/O map
        return ERROR_INVALID_PARAMETER;
    }

    return 0;  // no error
}

//
// MAPMapView - implementation of MapViewOfFile
//
LPVOID MAPMapView (PPROCESS pprc, HANDLE hMap, DWORD dwAccess, DWORD dwOffsetHigh, DWORD dwOffsetLow, DWORD cbSize)
{
    PHDATA   phdMap;
    LPVOID   pAddr = NULL;
    DWORD    dwErr = 0;

    DEBUGMSG (ZONE_MAPFILE, (L"MAPMapView %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
        hMap, dwAccess, dwOffsetHigh, dwOffsetLow, cbSize));

    // Lock the map and increment the handle refcount, until the view is unmapped.
    // That way we can induce a map pre-close when the view is unmapped but not
    // yet destroyed.
    //
    // The semantics are: every view "object" has a handle-count and lock-count on
    // the map, not every view "reference."  If you map 2000 views that overlap
    // exactly (=1 view object underneath), the map handle-count and lock-count will
    // both be 1.  If you map 2000 views that don't overlap, the map counts will both
    // be 2000.
    phdMap = LockHandleData (hMap, pActvProc);
    if (!phdMap) {
        dwErr = ERROR_INVALID_HANDLE;
        
    } else if (!DoLockHDATA (phdMap, HNDLCNT_INCR)) {
        DEBUGMSG (ZONE_MAPFILE, (L"MAPMapView: Failing, hit maximum number of handles per mapfile\r\n"));
        UnlockHandleData (phdMap);
        dwErr = ERROR_OUTOFMEMORY;

    } else {
        PFSMAP   pfsmap     = (PFSMAP) phdMap->pvObj;
        PMAPVIEW pview      = NULL;
        ULARGE_INTEGER liMapOffset;
        WORD     wViewOffset;

        DEBUGCHK (pfsmap);
        liMapOffset.HighPart = dwOffsetHigh;
        liMapOffset.LowPart = dwOffsetLow;

        dwErr = ValidateViewParameters (pfsmap, &dwAccess, &wViewOffset, &cbSize, &liMapOffset);
        if (dwErr) {
            // Fall through to error case

        } else if (pfsmap->dwFlags & MAP_FILECACHE) {
            // We should use MapCacheView, not MapViewOfFile, on file cache maps
            DEBUGCHK(0);
            dwErr = ERROR_INVALID_PARAMETER;

        } else if ((pfsmap->dwFlags & MAP_ATOMIC)
                   && (pfsmap->dwNumViews || (cbSize >= 64*1024*VM_PAGE_SIZE))) {
            // Atomic maps are only allowed to have one view.  This is because
            // the page table for that view is used to determine whether each
            // page is dirty, instead of a central page table encompassing all
            // views of the same map.  Atomic maps must flush ALL dirty pages
            // for the map at once.
            //
            // Limit 64K pages because of the fslog_t format.  The dwRestoreSize
            // count WORD must not overflow.
            
            DEBUGCHK(0);  // If we get here, filesys isn't following the rules
            dwErr = ERROR_INVALID_PARAMETER;

        // allocate the view structure
        } else if (NULL == (pview = AllocMem (HEAP_MAPVIEW))) {
            dwErr = ERROR_OUTOFMEMORY;

        } else {
            // initialize view structure
            MAP_SET_SIG (pview, MAP_VIEW_SIG);
            cbSize               += wViewOffset;
            liMapOffset.QuadPart -= wViewOffset;
            
            pview->base.pBaseAddr = NULL;
            pview->base.cbSize    = cbSize;
            pview->base.liMapOffset.QuadPart = liMapOffset.QuadPart;
            pview->base.dwDirty   = 0;
            pview->wViewOffset    = wViewOffset;
            pview->wFlags        = (WORD) dwAccess | FILE_MAP_READ;
            pview->dwRefCount     = MAP_VIEW_INCR;
            pview->phdMap         = phdMap;

            // Initialize flush information
            pview->flush.dwNumElements       = 0;
            pview->flush.cbFlushSpace        = 0;
            pview->flush.gather.prgSegments  = NULL;
            pview->flush.gather.prgliOffsets = NULL;
            pview->flush.nogather.prgPages   = NULL;  // redundant but safer
            
            if (InterlockedIncrement ((LONG*)&pfsmap->dwNumViews) > MAX_VIEW_PER_MAP) {
                DEBUGMSG (ZONE_MAPFILE, (L"MAPMapView: Failing, hit maximum number of views per mapfile\r\n"));
                // The counter will be decremented again in ReleaseView below
                dwErr = ERROR_OUTOFMEMORY;

            } else if (!pfsmap->phdFile
                        || ((pprc != g_pprcNK) && (pfsmap->dwFlags & MAP_USERFIXADDR))) {
                // Memory-backed mapfile, or fix address r/o map file in user space
                pAddr = MapFixedAddressView (pprc, pfsmap, pview, &dwErr);

            } else {
                // regular File-backed mapfile
                pview->base.pBaseAddr = VMReserve (pprc, cbSize,
                                                   (MAP_PAGEABLE & pfsmap->dwFlags) ? MEM_MAPPED : 0, 0);
                if (!pview->base.pBaseAddr) {
                    DEBUGMSG (ZONE_MAPFILE, (L"MAPMapView: Process VM reservation failed\r\n"));
                    dwErr = ERROR_OUTOFMEMORY;

                } else if (MAP_DIRECTROM & pfsmap->dwFlags) {
                    // Direct ROM, VirtualCopy directly from ROM address
                    // NOTE: liMapOffset can be negative in this case, because
                    // we can be potentially mapping from the page before if
                    // pfsmap->pROMBase is not page aligned.
                    DEBUGCHK ((0 == liMapOffset.HighPart) || ((DWORD)-1 == liMapOffset.HighPart));
                    if (!VMCopy (pprc, (DWORD) pview->base.pBaseAddr, g_pprcNK,
                                 (DWORD) pfsmap->pROMBase + liMapOffset.LowPart,
                                 cbSize, PageProtectionFromAccess (pview->wFlags))) {
                        DEBUGMSG (ZONE_MAPFILE, (L"MAPMapView: VMCopy Failed\r\n"));
                        dwErr = ERROR_OUTOFMEMORY;
                    }
                
                } else if (!(MAP_PAGEABLE & pfsmap->dwFlags)) {
                    // Non-pageable, VC from the master map file
                    if (!MapVMCopy (pfsmap, pprc, pview->base.pBaseAddr, liMapOffset, cbSize,
                                    PageProtectionFromAccess (pview->wFlags))) {
                        dwErr = ERROR_OUTOFMEMORY;
                        DEBUGMSG (ZONE_MAPFILE, (L"MAPMapView: MapVMCopy Failed\r\n"));
                    }
                
                } else if (pfsmap->phdFile
                           && IsViewWritable (pview)
                           && !PrepareForFlush(pview, pfsmap->dwFlags)) {
                    DEBUGMSG (ZONE_MAPFILE, (L"MAPMapView: Failed to prepare flush buffer\r\n"));
                    dwErr = ERROR_OUTOFMEMORY;
                }

                if (!dwErr) {
                    pAddr = AddFileBackedViewToProcess (pprc, pfsmap, pview, &dwErr);
                    DEBUGMSG (ZONE_MAPFILE, (L"", DumpMapView (pview)));
                }
            }
        }

        // error cleanup
        if (dwErr) {
            DoUnlockHDATA (phdMap, -HNDLCNT_INCR);
            if (pview) {
                ReleaseViewInProcess (pprc, pview);
                ReleaseView (pprc, pview);
            } else {
                UnlockHandleData (phdMap);
            }
        } else {
            // Only log an open event if the view was newly created
            CELOG_MapFileViewOpen (pprc, pview);
        }
    }

    if (dwErr)
        KSetLastError (pCurThread, dwErr);
    DEBUGMSG (ZONE_MAPFILE, (L"MAPMapView returns %8.8lx\r\n", pAddr));
    
    return pAddr;
}


//
// MAPUnmapView - implementation of UnmapViewOfFile
//
BOOL MAPUnmapView (PPROCESS pprc, LPVOID pAddr)
{
    PMAPVIEW pview;
    DWORD    dwViewRef = 1; // anything but 0
    DWORD    dwErr = ERROR_INVALID_PARAMETER;
    BOOL     fCloseMap;

    DEBUGMSG (ZONE_MAPFILE, (L"MAPUnmapView %8.8lx %8.8lx\r\n", pprc, pAddr));

    EnterCriticalSection (&MapCS);
    pview = LockViewByAddr (pprc, &pprc->viewList, pAddr, 0, LVA_EXACT);
    if (pview) {

        // Remove the original view ref, so that the view ref can reach 0.  If
        // this is the last ref, the view will no longer be lockable.  That
        // prevents race conditions with two threads unmapping simultaneously.
        DEBUGCHK (MAP_VIEW_COUNT (pview->dwRefCount) > 0);
        pview->dwRefCount -= MAP_VIEW_INCR;
        fCloseMap = (0 == MAP_VIEW_COUNT (pview->dwRefCount));  // Close if last view ref
        
        LeaveCriticalSection (&MapCS);

        if (fCloseMap) {
            // View pre-close case: Release map handle refcount when the
            // "view" count is removed but the view is still locked, which may
            // invoke the map pre-close.  The map handle is still locked until
            // the view is destroyed.
            DoUnlockHDATA (pview->phdMap, -HNDLCNT_INCR);
        }

        // Flush without holding MapCS.  We must always unmap even if the flush
        // fails.  The return value value indicates only whether the flush failed.
        dwErr = DoFlushView (pprc, (PFSMAP) pview->phdMap->pvObj,
                             &pview->base, &pview->flush,
                             pview->base.pBaseAddr, pview->base.cbSize,
                             MAP_FLUSH_WRITEBACK);
        
        EnterCriticalSection (&MapCS);

        // This unlock corresponds to the Lock above
        dwViewRef = UnlockView (pprc, pview);
    }
    LeaveCriticalSection (&MapCS);

    if (!dwViewRef) {
        DEBUGCHK (pview);
        ReleaseView (pprc, pview);
    }

    if (dwErr)
        KSetLastError (pCurThread, dwErr);
    
    DEBUGMSG (ZONE_MAPFILE, (L"MAPUnmapView returns, dwErr %8.8lx\r\n", dwErr));

    return (ERROR_SUCCESS == dwErr);
}


//
// MAPFlushView - implement FlushViewOfFile
//
BOOL MAPFlushView (PPROCESS pprc, LPVOID pAddr, DWORD cbSize)
{
    PMAPVIEW pview;
    BOOL     dwErr = ERROR_INVALID_PARAMETER;
    BOOL     dwViewRef = 1; // anything but 0
    
    DEBUGMSG (ZONE_MAPFILE, (L"MAPFlushView %8.8lx %8.8lx, %8.8lx\r\n", pprc, pAddr, cbSize));

    EnterCriticalSection (&MapCS);
    
    // Since file-backed views will never overlap in VM and since memory-backed
    // views never need flushing, there shouldn't be any problems that cause us
    // to choose the wrong view here.
    pview = LockViewByAddr (pprc, &pprc->viewList, pAddr, cbSize, 0);
    if (pview) {
        // Flush without holding MapCS
        LeaveCriticalSection (&MapCS);
        dwErr = DoFlushView (pprc, (PFSMAP) pview->phdMap->pvObj,
                             &pview->base, &pview->flush,
                             pAddr, cbSize, MAP_FLUSH_WRITEBACK);
        EnterCriticalSection (&MapCS);

        dwViewRef = UnlockView (pprc, pview);
    }
    LeaveCriticalSection (&MapCS);

    if (!dwViewRef) {
        DEBUGCHK (pview);
        ReleaseView (pprc, pview);
    }

    if (dwErr)
        KSetLastError (pCurThread, dwErr);
    DEBUGMSG (ZONE_MAPFILE, (L"MAPFlushView returns, dwErr = %8.8lx\r\n", dwErr));
    return !dwErr;
}


//
// MAPUnmapAllViews - called on process exit. Close all views in a process
//
void MAPUnmapAllViews (PPROCESS pprc)
{
    DLIST    viewList;
    PMAPVIEW pview;
    PFSMAP   pfsmap;
    DWORD    dwRefCnt;
    
    DEBUGMSG (ZONE_MAPFILE, (L"MAPUnmapAllViews: Enter %8.8lx\r\n", pprc));
    DEBUGCHK (!OwnCS (&MapCS));
    DEBUGCHK (!OwnCS(&pprc->csVM));  // CS ordering rules

    InitDList (&viewList);

    // Remove all the views from the list, though some may still be locked
    // (eg. by the page pool trim thread)
    EnterCriticalSection (&MapCS);
    pprc->bState = PROCESS_STATE_VIEW_UNMAPPED;
    if (!IsDListEmpty (&pprc->viewList)) {
        viewList = pprc->viewList;
        viewList.pBack->pFwd = viewList.pFwd->pBack = &viewList;
        InitDList (&pprc->viewList);    // this effectively clear the process's viewList
    }
    LeaveCriticalSection (&MapCS);

    // Wait for all asynchronous work to complete, so that no other threads
    // should have the views locked anymore
    EnterCriticalSection (&PageOutCS);
    // There is no work to do here.  The act of taking the CS forced all 
    // asynchronous work on the views to complete, and removing them from
    // the process view list prevents any further asynchronous accesses.
    LeaveCriticalSection (&PageOutCS);

    for ( ; ; ) {
        EnterCriticalSection (&MapCS);
        pview = IsDListEmpty (&viewList)? NULL : (PMAPVIEW) viewList.pFwd;
        if (pview) {
            // remove the view from the DList
            RemoveDList (&pview->link);
            InitDList (&pview->link);  // point at self so it's okay to call RemoveDList again on the view
            LockView (pview);
        }
        LeaveCriticalSection (&MapCS);

        if (!pview) {
            break;
        }
        
        pfsmap = (PFSMAP) pview->phdMap->pvObj;
        // Only the kernel process should be creating atomic maps, and the kernel
        // should not be exiting.
        DEBUGCHK(!(pfsmap->dwFlags & MAP_ATOMIC));

        if (pview->base.dwDirty) {
            // If this flush fails, some changes may be lost.  But there is nothing
            // we can do about that.  Typically flush failures are due to disk-full
            // conditions or other problems in the file system.
            DoFlushView (pprc, pfsmap, &pview->base, &pview->flush,
                         pview->base.pBaseAddr, pview->base.cbSize,
                         MAP_FLUSH_WRITEBACK);
        }

        // at this point, view count cannot change by other threads (but lock count can), so it's safe to look at
        // view count without holding MapCS;
        
        // Release resources only if not already released, in case the view was
        // partway through being cleaned up already
        if (MAP_VIEW_COUNT (pview->dwRefCount) > 0) {
            DoUnlockHDATA (pview->phdMap, -HNDLCNT_INCR);
        }

        EnterCriticalSection (&MapCS);
        // remove view count, but keep lock count
        pview->dwRefCount = MAP_LOCK_COUNT (pview->dwRefCount);
        
        // unlock the view, which will remove the mapping of the view if there are no other thread accessing the view
        dwRefCnt = UnlockView (pprc, pview);
        LeaveCriticalSection (&MapCS);

        if (!dwRefCnt) {
            ReleaseView (pprc, pview);
        }

    }

    DEBUGMSG (ZONE_MAPFILE, (L"MAPUnmapAllViews: Exit\r\n"));
}


//
// MAPPreClose - Operations to take when the last handle-count on the mapfile
// is closed.  There may still be active operations taking place on the mapfile,
// but there are no more open map-handles or mapped views.
//
static BOOL MAPPreClose (PFSMAP pfsmap)
{
    DEBUGMSG (ZONE_MAPFILE, (L"MAPPreClose %8.8lx\r\n", pfsmap));

    // The map-handles are closed and views are unmapped, so no more data will be
    // in the pool.  Remove the map from the list so that the pool thread won't
    // lock it anymore.
    EnterCriticalSection (&MapCS);
    PagePoolDeleteFile (pfsmap);
    RemoveDList (&pfsmap->maplink);
    InitDList (&pfsmap->maplink);  // point at self for safety against double-remove
    LeaveCriticalSection (&MapCS);

    // Write back all data and force the page pool trim thread to finish any
    // write-back that it is doing.
    FlushFile (pfsmap);
    
    return TRUE;
}


//
// MAPClose - close a memory mapped file (no more ref in the system)
//
static BOOL MAPClose (PFSMAP pfsmap)
{
    DEBUGMSG (ZONE_MAPFILE, (L"MAPClose %8.8lx\r\n", pfsmap));

    // Should already have gone through the pre-close
    DEBUGCHK ((pfsmap->maplink.pFwd == &pfsmap->maplink)
              && (pfsmap->maplink.pBack == &pfsmap->maplink));

    // Avoid logging this when InitFSMAP fails.
    if ((MAP_PT_FLAGS_INITIALIZED & pfsmap->pgtree.flags)
        || (pfsmap->dwFlags & MAP_DIRECTROM)) {
        CELOG_MapFileDestroy (pfsmap);
    }

    pfsmap->phdMap = NULL;

    // fixed address map or ram-back VM cleanup
    if (pfsmap->pUsrBase) {
        DEBUGCHK (!pfsmap->phdFile || (pfsmap->dwFlags & MAP_USERFIXADDR));
        // the kernel reservationis reserved with no pager, there cannot be any page committed in this range
        // using paging memory.
        VERIFY (VMFreeAndRelease (g_pprcNK, pfsmap->pUsrBase, pfsmap->liSize.LowPart));
        pfsmap->pUsrBase = NULL;
    }

    // RAM-backed cleanup for kernel VM
    if (!pfsmap->phdFile
        && pfsmap->pKrnBase
        && !(pfsmap->dwFlags & MAP_FILECACHE)) {
        const ULARGE_INTEGER liZero = { 0, 0 };
        DEBUGCHK (!pfsmap->liSize.HighPart);

        VERIFY (MapVMDecommit (pfsmap, g_pprcNK, pfsmap->pKrnBase, liZero,
                               pfsmap->liSize.LowPart, TRUE));
        VERIFY (VMFreeAndRelease (g_pprcNK, pfsmap->pKrnBase, pfsmap->liSize.LowPart));
        pfsmap->pKrnBase = NULL;
    }
    
    if (!(MAP_DIRECTROM & pfsmap->dwFlags)) {
        MapVMCleanup (pfsmap);
    }

    if (pfsmap->phdFile) {
        UnlockHandleData (pfsmap->phdFile);
        pfsmap->phdFile = NULL;
    }

    MAP_VALIDATE_SIG (pfsmap, MAP_FSMAP_SIG);
    FreeMem (pfsmap, HEAP_FSMAP);

    return TRUE;
}


/*

Note about the API set: All APIs should either return 0 on error or return
no value at-all. This will allow us to return gracefully to the user when an
argument to an API doesn't pass the PSL checks. Otherwise we will end
up throwing exceptions on invalid arguments to API calls.

*/

const PFNVOID MapMthds[] = {
    (PFNVOID) MAPClose,
    (PFNVOID) MAPPreClose,
};

static const ULONGLONG MapSigs[] = {
    FNSIG1 (DW),                                // CloseHandle
    FNSIG1 (DW),                                // PreClose
};

ERRFALSE ((sizeof(MapMthds) / sizeof(MapMthds[0])) == (sizeof(MapSigs) / sizeof(MapSigs[0])));

const CINFO cinfMap = {
    { 'F', 'M', 'A', 'P' },
    DISPATCH_KERNEL_PSL,
    HT_FSMAP,
    sizeof(MapMthds)/sizeof(MapMthds[0]),
    MapMthds,
    MapMthds,
    MapSigs,
    0,
    0,
    0,
};


//
// MAPPageInPage: page in a page into MAP VM, return PAGEIN_SUCCESS if successful
//                PAGEIN_FAILURE/PAGEIN_RETRY if failed (depends on the reason of failure)
//
static DWORD MAPPageInPage (PFSMAP pfsmap, ULARGE_INTEGER* pliFileOffset, LPBYTE pPagingPage)
{
    DWORD dwRet = PAGEIN_SUCCESS;
    WORD PageRefCount;

    // The file mapping object must have been locked when we called this function
    // or we can get into big trouble if the mapfile object got destroyed.
    DEBUGCHK (!OwnCS (&MapCS));
    DEBUGCHK (!OwnCS (&PagerCS));

    DEBUGMSG (ZONE_MAPFILE || ZONE_PAGING,
              (L"MAPPageInPage: %8.8lx %8.8lx offset %8.8lx %8.8lx\r\n",
               pfsmap, pPagingPage, pliFileOffset->HighPart, pliFileOffset->LowPart));

    if (pfsmap->phdFile || (pfsmap->dwFlags & MAP_FILECACHE)) {
        // File backed
        DWORD cbToRead;
        DWORD cbRead;
        BOOL  fSuccess;
        
        DEBUGCHK ((MAP_PAGEABLE & pfsmap->dwFlags) && !(MAP_DIRECTROM & pfsmap->dwFlags));

        // Read less than a page if we're at the end of the file
        cbToRead = VM_PAGE_SIZE;
        if (cbToRead > pfsmap->liSize.QuadPart - pliFileOffset->QuadPart) {
            cbToRead = (DWORD) (pfsmap->liSize.QuadPart - pliFileOffset->QuadPart);
        }
        if (0 == cbToRead) {
            DEBUGCHK (0);
            return PAGEIN_FAILURE;
        }

        // Call to filesys to read the page
        if (pfsmap->dwFlags & MAP_FILECACHE) {
            DEBUGCHK (!IsKernelVa (pPagingPage));  // Don't hand static-mapped VA to file system
            DEBUGCHK (pActvProc == g_pprcNK);  // Never call out to filesys if active proc != NK
            fSuccess = g_FSCacheFuncs.pReadWriteCacheData (pfsmap->FSSharedFileMapId,
                                                           pPagingPage, cbToRead, &cbRead,
                                                           pliFileOffset, FALSE);
        } else {
            fSuccess = PHD_ReadFileWithSeek (pfsmap->phdFile,
                                             pPagingPage, cbToRead, &cbRead,
                                             NULL, pliFileOffset->LowPart,
                                             pliFileOffset->HighPart);
        }
        
        if (!fSuccess) {
            if (pfsmap->dwFlags & MAP_FILECACHE) {
                DEBUGCHK (0);
                dwRet = PAGEIN_FAILURE;
            } else switch (KGetLastError (pCurThread)) {
            case ERROR_DEVICE_REMOVED:
            case ERROR_DEVICE_NOT_AVAILABLE:
                dwRet = PAGEIN_FAILURE;
                break;
            default:
                dwRet = PAGEIN_RETRY;
                break;
            }

        } else if (cbRead != cbToRead) {
            DEBUGCHK (0);
            dwRet = PAGEIN_FAILURE;

        }
    }

    // Refcount=1 for RAM-backed, else 0
    PageRefCount = (pfsmap->phdFile || (pfsmap->dwFlags & MAP_FILECACHE)) ? 0 : 1;

    // Now VirtualCopy to MAP VM.  RAM-backed mapfiles keep a reference count
    // on the page until the mapfile is destroyed, while all others have no
    // references until MapVMCopy adds the page to a view.
    if ((PAGEIN_SUCCESS == dwRet)
        && !VMMapMovePage (pfsmap, *pliFileOffset, pPagingPage, PageRefCount)) {
        DEBUGMSG (ZONE_MAPFILE || ZONE_PAGING, (L"MAPPageInPage: MapVMAddPage Failed! Other thread paged in the page for us. Retry!\r\n"));
        // VirtualCopy failed, most likely other thread paged in the page already, just retry
        dwRet = PAGEIN_RETRY;
    }
    
    DEBUGMSG (ZONE_MAPFILE || ZONE_PAGING, (L"MAPPageInPage: returns %8.8lx\r\n", dwRet));
    return dwRet;
}


static BOOL
TryExistingPage (
    PFSMAP   pfsmap,
    CommonMapView_t* pViewBase,
    PPROCESS pprc,
    DWORD    dwAddr,
    ULARGE_INTEGER liFileOffset,
    DWORD    fProtect
    )
{
    VMMTEResult result = VMMTE_SUCCESS;  // Anything but ALREADY_EXISTS
    BOOL fTakeLock = ((PAGE_READWRITE|PAGE_EXECUTE_READWRITE) & fProtect)
                  && (pfsmap->dwFlags & MAP_ATOMIC);

    // Pre-increment the dirty counter.  The dirty counter can exceed the
    // true number of dirty pages in the view, but must never be too low.
    // If a R/W page is added to the view without incrementing dwDirty, and
    // another thread tries to flush, it'd underflow the dirty counter.
    if ((PAGE_READWRITE|PAGE_EXECUTE_READWRITE) & fProtect) {

        if (fTakeLock) {
            EnterCriticalSection (&PageOutCS);
        }
        InterlockedIncrement ((LONG*)&pViewBase->dwDirty);
        DEBUGCHK (pViewBase->dwDirty);
    }

    // Check if it's already in the mapfile page tree.  Otherwise, check if
    // it's already committed in the process.  Do it in this order so that
    // if another thread beats us to adding the page to the process (causing
    // MapVMCopy to fail) we can recover with a subsequent VMMapTryExisting call.
    
    if (!MapVMCopy (pfsmap, pprc, (LPVOID) dwAddr, liFileOffset,
                    VM_PAGE_SIZE, fProtect)) {
        
        result = VMMapTryExisting (pprc, (LPVOID) dwAddr, fProtect);
        
        if (((PAGE_READWRITE|PAGE_EXECUTE_READWRITE) & fProtect) && (VMMTE_SUCCESS != result)) {  // ALREADY_EXISTS or FAIL
            DEBUGCHK (pViewBase->dwDirty);
            InterlockedDecrement ((LONG*)&pViewBase->dwDirty);
        }
        
    }
    if (fTakeLock) {
        LeaveCriticalSection (&PageOutCS);
    }
    
    DEBUGMSG (ZONE_MAPFILE && ZONE_PAGING, (L"TryExistingPage: exit, result = %d\r\n", result));
    return (VMMTE_FAIL != result);
}


//
// PageFromMapper - paging function for memory mapped files
//
DWORD PageFromMapper (PPROCESS pprc, DWORD dwAddr, BOOL fWrite)
{
    DWORD    dwRet = PAGEIN_FAILURE;
    PFSMAP   pfsmap = NULL;
    PMAPVIEW pMapView = NULL;
    CommonMapView_t* pViewBase = NULL;
    
    DEBUGMSG (ZONE_MAPFILE || ZONE_PAGING, (L"PageFromMapper: %8.8lx %8.8lx %8.8lx\r\n", pprc, dwAddr, fWrite));

    // Try a cache view first, then a mapped view
    if ((pprc != g_pprcNK) || !LockCacheView (dwAddr, &pfsmap, &pViewBase)) {
        EnterCriticalSection (&MapCS);
        pMapView = LockViewByAddr (pprc, &pprc->viewList, (LPVOID) dwAddr, 0, fWrite ? LVA_WRITE_ACCESS : 0);
        LeaveCriticalSection (&MapCS);
        if (pMapView) {
            pfsmap = (PFSMAP) pMapView->phdMap->pvObj;
            pViewBase = &pMapView->base;

            // regular RAM-backed view - make it writable if view is writeable.
            if (!pfsmap->phdFile
                && !(pfsmap->dwFlags & MAP_FILECACHE)
                && IsViewWritable (pMapView)) {
                fWrite = TRUE;
            }
        }
    }
    DEBUGMSG (ZONE_MAPFILE && ZONE_PAGING, (L"PageFromMapper: pview = %8.8lx\r\n", pViewBase));

    dwAddr &= ~VM_PAGE_OFST_MASK;
    
    if (pViewBase) {
        ULARGE_INTEGER liFileOffset;
        // do not use PageProtectionFromAccess (pMapView) here. We don't want to page in the page r/w unless
        // write is requested or all the view pages will be "dirty"
        DWORD  fProtect = (pMapView && IsViewExecutable (pMapView))
                        ? (fWrite ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ)
                        : (fWrite ? PAGE_READWRITE : PAGE_READONLY);
        DWORD  dwViewRef;
        
        DEBUGCHK (pfsmap);
#ifdef ARM
        // Current implementation: for VIVT cache, all R/W mapfile are uncached except for CEDB and Cache Manager pages.
        // we might want to look into more complicated scheme to enable more mapfiles being cached if we find this causing
        // performance issues.
        if (IsVirtualTaggedCache ()
            && (pfsmap->dwFlags & PAGE_READWRITE)
            && !(pfsmap->dwFlags & (MAP_ATOMIC|MAP_FILECACHE))) {
            fProtect |= PAGE_NOCACHE;
        }
#endif
        liFileOffset.QuadPart = pViewBase->liMapOffset.QuadPart             // offset of the view in the mapfile
                                + (dwAddr - (DWORD) pViewBase->pBaseAddr);  // offset into the view
        
        DEBUGCHK ((MAP_PAGEABLE & pfsmap->dwFlags) && !(MAP_DIRECTROM & pfsmap->dwFlags));

        
        // Perf shortcut: see if the file data is already in RAM somewhere
        if (TryExistingPage (pfsmap, pViewBase, pprc, dwAddr, liFileOffset, fProtect)) {
            dwRet = PAGEIN_SUCCESS;
        
        } else {

            LPBYTE pPagingPage;
            PagePool_t *pPool;
                
            // Use the pool if file-backed and pageable
            pPool = (   (pfsmap->phdFile && (pfsmap->dwFlags & MAP_PAGEABLE))
                     || (pfsmap->dwFlags & MAP_FILECACHE)) ? &g_FilePool : NULL;

            // Get a page to write to
            pPagingPage = GetPagingPage (pPool, dwAddr);
            
            if (pPagingPage) {
                // Read the file data into the page, and store it in the page tree
                dwRet = MAPPageInPage (pfsmap, &liFileOffset, pPagingPage);

                // Free the page regardless of success, because it will already be
                // moved to the tree on success.
                FreePagingPage (pPool, pPagingPage);

                // Now add the page to the process VM
                if ((PAGEIN_SUCCESS == dwRet)
                    && !TryExistingPage (pfsmap, pViewBase, pprc, dwAddr, liFileOffset, fProtect)) {
                    // Already added to the page tree, yet failed to VirtualCopy.  The page
                    // may have been removed from the page tree by another thread.
                    DEBUGMSG (ZONE_MAPFILE || ZONE_PAGING, (L"PageFromMapper: TryExistingPage Failed! Other thread took the page out of the tree. Retry!\r\n"));
                    dwRet = PAGEIN_RETRY;
                }

            } else {
                dwRet = PAGEIN_RETRY;
            }
        }

        // Check that page refcounts still make sense
        DEBUGLOG (ZONE_MAPFILE, MapVMValidate (pfsmap));

        // unlock the view
        if (pMapView) {
            EnterCriticalSection (&MapCS);
            dwViewRef = UnlockView (pprc, pMapView);
            LeaveCriticalSection (&MapCS);
            if (!dwViewRef) {
                // view unmapped while we're paging
                ReleaseView (pprc, pMapView);
                DEBUGCHK (0);
                dwRet = PAGEIN_FAILURE;
            }
        } else {
            UnlockCacheView (pfsmap, pViewBase);
        }
    }
    DEBUGMSG (ZONE_MAPFILE || ZONE_PAGING, (L"PageFromMapper: returns %8.8lx\r\n", dwRet));

    return dwRet;
}


BOOL MapfileInit (void)
{

    DEBUGMSG (ZONE_DEBUG, (L"Initializing Memory Mapped File Support\r\n"));
    
    InitializeCriticalSection(&FlushCS);
    DEBUGCHK (NULL == csarray[CSARRAY_FLUSH]);  // Not yet assigned to anything
    csarray[CSARRAY_FLUSH] = &FlushCS;
    InitDList (&g_MapList);
    SystemAPISets[HT_FSMAP] = &cinfMap;
    g_pLockedProcessList = AllocMem (HEAP_LOCKED_PROCESS_LIST);
    DEBUGCHK (g_pLockedProcessList);
    MAP_SET_SIG (g_pLockedProcessList, 0);  // Global list should never be freed
    
    return (NULL != SystemAPISets[HT_FSMAP]);
}




//------------------------------------------------------------------------------
// ASYNCHRONOUS FLUSHING
// Page pool trimming, low memory cleanup and other lazy file management.
//------------------------------------------------------------------------------

// Clear all locks created by LockProcesses.  Free the list items.
static void
UnlockProcesses ()
{
    LockedProcessList_t* pList;
    int ProcsInList;

    DEBUGCHK (OwnCS (&PageOutCS));  // Protects global locked process list

    pList = g_pLockedProcessList;  // pList always either points at the head or the 2nd list element
    do {
        for (ProcsInList = 0; ProcsInList < LOCKED_PROCESS_LIST_SIZE; ProcsInList++) {
            if (pList->phdProcess[ProcsInList]) {
                UnlockHandleData (pList->phdProcess[ProcsInList]);
            }
        }
        
        g_pLockedProcessList->pNext = pList->pNext;
        if (pList != g_pLockedProcessList) {
            MAP_VALIDATE_SIG (pList, MAP_LOCKPROCLIST_SIG);
            FreeMem (pList, HEAP_LOCKED_PROCESS_LIST);
        }
        pList = g_pLockedProcessList->pNext;
    
    } while (pList);
}


// Lock all of the processes of in the system, so that we can enumerate through
// them later without holding any critical sections.
static void
LockProcesses ()
{
    LockedProcessList_t* pList = g_pLockedProcessList;
    DWORD  ProcsInList, TotalProcesses;
    PDLIST ptrav;
    
    DEBUGCHK (OwnCS (&PageOutCS));  // Protects global locked process list
    DEBUGCHK (!OwnCS (&MapCS));     // CS ordering: Must take modlist CS before MapCS

    ProcsInList = 0;
    TotalProcesses = 0;
    DEBUGCHK (pList);
    if (pList) {
        memset (pList->phdProcess, 0, LOCKED_PROCESS_LIST_SIZE * sizeof(PHDATA));
        pList->pNext = NULL;

        LockModuleList ();
        for (ptrav = (PDLIST) g_pprcNK->prclist.pFwd;
             ptrav != &g_pprcNK->prclist;
             ptrav = ptrav->pFwd) {

            if (TotalProcesses >= g_NumProcessesToSkip) {
                if (ProcsInList >= LOCKED_PROCESS_LIST_SIZE) {
                    LockedProcessList_t* pNew = AllocMem (HEAP_LOCKED_PROCESS_LIST);
                    if (pNew) {
                        memset (pNew, 0, sizeof(LockedProcessList_t));
                        MAP_SET_SIG (pNew, MAP_LOCKPROCLIST_SIG);

                        pList->pNext = pNew;
                        pList = pNew;
                        ProcsInList = 0;
                    } else {
                        // We at least locked some processes so try to flush views
                        // in those.  Next time we flush, we'll start later in the list.
                        g_NumProcessesToSkip = TotalProcesses;
                        break;
                    }
                }
                
                DEBUGCHK (ProcsInList < LOCKED_PROCESS_LIST_SIZE);
                pList->phdProcess[ProcsInList] = LockHandleData ((HANDLE) ((PPROCESS) ptrav)->dwId, g_pprcNK);
                ProcsInList++;
            }
            TotalProcesses++;
        }
        if (ptrav == &g_pprcNK->prclist) {
            // Reached the end of the list
            g_NumProcessesToSkip = 0;
        }

        UnlockModuleList ();
    }
}


// Worker for FlushAllViewsOfMap.  Flush/decommit all views of a given map
// inside the process.  Returns FALSE if any flushes failed.
static BOOL
FlushProcessViewsOfMap (
    PPROCESS pprc,
    PFSMAP   pfsmap,
    DWORD    FlushFlags                 // Bitmask of MAP_FLUSH_*
    )
{
    PPROCESS pprcVM;
    PMAPVIEW pview;
    PMAPVIEW pUnlock;
    DWORD    dwViewRef;
    BOOL     fSuccess = TRUE;

    // Don't flush or page out while holding MapCS.  Lock the view and
    // then let go of the CS.  Cannot unlock while holding the CS either.
    
    // Switch to the other process' VM
    pprcVM = SwitchVM (pprc);

    // Find a starting view and lock it
    EnterCriticalSection (&MapCS);
    pview = (PMAPVIEW) pprc->viewList.pFwd;
    if (pview != (PMAPVIEW) &pprc->viewList) {
        LockView (pview);
    }
    LeaveCriticalSection (&MapCS);

    // Page out each view in the list
    while (pview != (PMAPVIEW) &pprc->viewList) {

        // Is it a view of the given mapping?
        if ((PFSMAP) (pview->phdMap->pvObj) == pfsmap) {

            // Write back data, convert R/W pages to R/O, decommit R/O pages
            if (!DoFlushView (pprc, pfsmap, &pview->base, &pview->flush,
                              pview->base.pBaseAddr, pview->base.cbSize,
                              FlushFlags)) {
                fSuccess = FALSE;
            }
        }

        // Move to the next list entry
        EnterCriticalSection (&MapCS);
        {
            // Cannot unlock until after locking the next view
            pUnlock = pview;
            if (IsViewCreationAllowed (pprc)) {
                pview = (PMAPVIEW) pview->link.pFwd;
            } else {
                pview = (PMAPVIEW) &pprc->viewList;
                fSuccess = FALSE;
            }
            if (pview != (PMAPVIEW) &pprc->viewList) {
                LockView (pview);
            }
            dwViewRef = UnlockView (pprc, pUnlock);
        }
        LeaveCriticalSection (&MapCS);

        if (!dwViewRef) {
            DEBUGCHK (pUnlock);
            ReleaseView (pprc, pUnlock);
        }
    }

    SwitchVM (pprcVM);

    return fSuccess;
}


// Flush and/or decommit pages for a mapfile, from all views in all processes.
// Returns FALSE if any flushes failed
static BOOL
FlushAllViewsOfMap (
    PFSMAP pfsmap,
    LockedProcessList_t* pProcessList,
    DWORD FlushFlags                    // Bitmask of MAP_FLUSH_*
    )
{
    int  Proc;
    BOOL fSuccess = TRUE;

    DEBUGCHK (!OwnCS (&MapCS));
    DEBUGCHK (!(pfsmap->dwFlags & MAP_FILECACHE));

    // Only flush/decommit file-backed pageable maps
    if (pfsmap->phdFile
        && !(MAP_DIRECTROM & pfsmap->dwFlags)
        && (MAP_PAGEABLE & pfsmap->dwFlags)) {

        // NK is not on the process list so it must be invoked separately
        if (!FlushProcessViewsOfMap (g_pprcNK, pfsmap, FlushFlags)) {
            fSuccess = FALSE;
        }
        
        // Now walk each of the processes we locked
        while (pProcessList) {
            for (Proc = 0; Proc < LOCKED_PROCESS_LIST_SIZE; Proc++) {
                if (pProcessList->phdProcess[Proc]) {
                    PPROCESS pprc = (PPROCESS) (pProcessList->phdProcess[Proc]->pvObj);
                    if (!FlushProcessViewsOfMap (pprc, pfsmap, FlushFlags)) {
                        fSuccess = FALSE;
                    }
                }
            }
            pProcessList = pProcessList->pNext;
        }
    }

    return fSuccess;
}


// FlushAllFiles - Flush and/or decommit pages from all mapfiles in the system,
// until enough memory is reclaimed.  Used during low-memory reclamation and
// page pool trimming.
static void 
FlushAllFiles (
    DWORD  FlushFlags,  // Bitmask of MAP_FLUSH_*
    BOOL   Force        // TRUE: Flush all files regardless of memory levels
                        // FALSE: Stop flushing if the pool is at acceptable memory level
    )
{
    PFSMAP pfsmap;
    BOOL   fCritical = !(FlushFlags & MAP_FLUSH_WRITEBACK);  // Critical if no write

    EnterCriticalSection (&PageOutCS);     // Protect GetNextFile enumeration state and global locked proc list


    // We'd need to hold the NK loader lock in order to walk the list of
    // processes and flush the views of this map, but it would be very bad
    // to hold the loader lock for so long.  Instead, get a list of
    // processes that exist at this moment, lock them, and only flush in those.
    LockProcesses ();
    
    //
    // Find a starting map and lock it
    //
    EnterCriticalSection (&MapCS);
    pfsmap = (PFSMAP) PagePoolGetNextFile (TRUE, fCritical);
    if (pfsmap)
        LockMap (pfsmap);
    LeaveCriticalSection (&MapCS);

    if (pfsmap) {
    
        //
        // Page out each mapping in the list until we run out of maps or get
        // enough memory.
        //

        do {

            if (pfsmap->dwFlags & MAP_FILECACHE) {
                // Only filesys knows which views belong to this map.  It will call back to
                // flush each view.
                DEBUGCHK (pActvProc == g_pprcNK);  // Never call out to filesys if active proc != NK
                if (pfsmap->FSSharedFileMapId) {
                    // check if cache filter has initialized the map object (see CACHE_CreateMapping)
                    g_FSCacheFuncs.pAsyncFlushCacheMapping (pfsmap->FSSharedFileMapId, FlushFlags);
                }
                
            } else {
                FlushAllViewsOfMap (pfsmap, g_pLockedProcessList, FlushFlags);
            }
            
            // Move to the next list entry
            {
                // Cannot unlock pfsmap until after locking the next map
                PFSMAP pUnlock = pfsmap;

                EnterCriticalSection (&MapCS);
                pfsmap = (PFSMAP) PagePoolGetNextFile (FALSE, fCritical);
                if (pfsmap)
                    LockMap (pfsmap);
                LeaveCriticalSection (&MapCS);

                UnlockMap (pUnlock);
            }
        
        } while (pfsmap && (Force || PGPOOLNeedsTrim (&g_FilePool)));


        if (pfsmap)
            UnlockMap (pfsmap);
    }

    UnlockProcesses ();

    LeaveCriticalSection (&PageOutCS);
}


// FlushFile - Flush pages from a single mapfile.
// Used during map pre-close to flush data and force the page pool trim
// thread to release the file.  pfsmap should already be locked.
static BOOL 
FlushFile (
    PFSMAP pfsmap
    )
{
    BOOL fSuccess = TRUE;

    if (pfsmap->phdFile || (pfsmap->dwFlags & MAP_FILECACHE)) {

        // PageOutCS forces the page pool trim thread to release locks on the
        // map, and protects the global locked process list.
        EnterCriticalSection (&PageOutCS);

        // Cache maps have no data left to flush when the map handle is closed.  But
        // they still need to acquire PageOutCS to make the trim thread release the map.
        if (!(pfsmap->dwFlags & MAP_FILECACHE)) {
            LockProcesses ();
            fSuccess = FlushAllViewsOfMap (pfsmap, g_pLockedProcessList,
                                           MAP_FLUSH_WRITEBACK);
            UnlockProcesses ();
        }

        LeaveCriticalSection (&PageOutCS);
    }
    
    return fSuccess;
}


// Part of DoPageOut.  Page out mapfile pages until enough memory is released.
void 
PageOutFiles (
    BOOL fCritical
    )
{
    DEBUGMSG(ZONE_PAGING || ZONE_MAPFILE,
             (L"MMFILE: PageOut starting free count: %d\r\n", PageFreeCount));

    DEBUGCHK (OwnCS (&PageOutCS));
    DEBUGCHK (!OwnCS (&MapCS));  // Must take NK loader CS before MapCS
    
    // Write-back dirty data only if non-critical, stop when enough pool is free
    FlushAllFiles (fCritical ? MAP_FLUSH_DECOMMIT_RO : (MAP_FLUSH_WRITEBACK | MAP_FLUSH_DECOMMIT_RO),
                   FALSE);
    
    DEBUGMSG(ZONE_PAGING || ZONE_MAPFILE,
             (L"MMFILE: PageOut ending free count: %d\r\n", PageFreeCount));
}


// Used on suspend, shutdown.  Flush all dirty mapfile pages.  Does not decommit.
void 
WriteBackAllFiles ()
{
    DEBUGCHK (!OwnCS (&MapCS));  // Must take NK loader CS before MapCS
    EnterCriticalSection (&PageOutCS);
    FlushAllFiles (MAP_FLUSH_WRITEBACK, TRUE);
    LeaveCriticalSection (&PageOutCS);
}
