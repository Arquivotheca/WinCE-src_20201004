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

extern CRITICAL_SECTION MapCS, FileFlushCS, ListWriteCS;

DLIST g_MapList;  // Protected by MapCS

#define VALID_VIEW_ACCESS       (FILE_MAP_READ|FILE_MAP_WRITE|FILE_MAP_EXECUTE)

// internal flag to distinguish whether to call FlushFileBuffer or not
#define MAP_FORCE_FILE_FLUSH       0x80000000

FORCEINLINE WORD PageProtectionFromAccess (DWORD dwAccess)
{
    return (dwAccess & FILE_MAP_EXECUTE)
            ? ((dwAccess & FILE_MAP_WRITE)? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ)
            : ((dwAccess & FILE_MAP_WRITE)? PAGE_READWRITE : PAGE_READONLY);
}

static BOOL MAPClose (PFSMAP pfsmap);

#define MAP_PRECLOSED      0x80000000

const ULARGE_INTEGER liZero = {0, 0};

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
        // at this point, there is no more "handle" to the view, i.e. there cannot be
        // any view pages locked or it's a bug.
        if (pfsmap->ppgtree) {
            VERIFY (CleanupView (pprc, pfsmap->ppgtree, pview, VFF_DECOMMIT));
        }

        if (pfsmap->phdFile) {
            //
            // file backed mapfile, release VM.
            //
            VERIFY (VMFreeAndRelease (pprc, pview->base.pBaseAddr, pview->base.cbSize));
            pview->base.pBaseAddr = NULL;
            
        } else if (pview->pViewCntOfMapInProc
                && MAP_VALIDATE_SIG (pview->pViewCntOfMapInProc, MAP_VIEWCOUNT_SIG)
                && !InterlockedDecrement (&pview->pViewCntOfMapInProc->count)) {

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
                                     pview->pViewCntOfMapInProc));
            FreeMem (pview->pViewCntOfMapInProc, HEAP_VIEWCNTOFMAPINPROC);
            pview->pViewCntOfMapInProc = NULL;
        }
    } else {
        DEBUGCHK (pfsmap->phdFile || !pview->pViewCntOfMapInProc);
    }
}


// Second phase of destroying a view
void ReleaseView (PPROCESS pprc, PMAPVIEW pview)
{
    PFSMAP pfsmap = (PFSMAP) pview->phdMap->pvObj;
    PPAGETREE ppgtree = pfsmap->ppgtree;
    
    DEBUGCHK (!OwnCS (&MapCS));

    CELOG_MapFileViewClose(pprc, pview);

    if (ppgtree) {
        PageTreeDecView (ppgtree);
    }

    // pview->base.pBaseAddr is freed in ReleaseViewInProcess

    MAP_VALIDATE_SIG (pfsmap, MAP_FSMAP_SIG);
    UnlockHandleData (pview->phdMap);
    MAP_VALIDATE_SIG (pview, MAP_VIEW_SIG);
    FreeMem (pview, HEAP_MAPVIEW);
}


//
// Lock a view - return the new ref count of the view
//
DWORD LockView (PMAPVIEW pview)
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
DWORD UnlockView (PPROCESS pprc, PMAPVIEW pview)
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
    DWORD  dwViewEnd   = (DWORD) pview->base.pBaseAddr + pview->base.cbSize;

    // integer overflow check should have been performed before the call.
    DEBUGCHK (pelvs->dwAddr + pelvs->cbSize >= pelvs->dwAddr);

    return (MAP_VIEW_COUNT (pview->dwRefCount)                  // view is not being destroyed
            && (!(LVA_WRITE_ACCESS & pelvs->dwFlags) || IsViewWritable (pview))   // writeable if write requested
            && ((LVA_EXACT & pelvs->dwFlags)
                ? (pelvs->dwAddr == dwViewBase)                                         // exact base if requested, otherwise
                : (   (pelvs->dwAddr >= PAGEALIGN_DOWN (dwViewBase))                    // begin address >= page-aligned start address
                   && (pelvs->dwAddr + pelvs->cbSize <= PAGEALIGN_UP(dwViewEnd)))));    // end address <= page-aligned end address
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

    // integer overflow check should have been performed before the call.
    DEBUGCHK ((DWORD)pAddr + cbSize >= (DWORD)pAddr);

    pview = (PMAPVIEW) EnumerateDList (pviewList, TestAddrInView, &elvs);
    if (pview) {
        // Sanity check CS ordering rules now that we know what view it is
        DEBUGCHK (!OwnPgTreeLock (((PFSMAP)(pview->phdMap->pvObj))->ppgtree) && !OwnCS(&pprc->csVM));

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

    // Reserve a temporary buffer to copy with
    pFileBuf = VMAlloc (g_pprcNK, NULL, pfsmap->liSize.LowPart, MEM_COMMIT, PAGE_READWRITE);
    if (pFileBuf) {
        DWORD cbRead;
        if ((PHD_SetFilePointer (pfsmap->phdFile, 0, 0, FILE_BEGIN) != (DWORD)-1)
            && PHD_ReadFile (pfsmap->phdFile, pFileBuf, pfsmap->liSize.LowPart, &cbRead, NULL)
            && (cbRead == pfsmap->liSize.LowPart)) {

            // Read successfully, so copy to mapfile page tree
            fRet = PageTreeFill (pfsmap->ppgtree, pFileBuf, cbRead);
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
    if (pfsmap->ppgtree) {
        NKLOG (pfsmap->ppgtree->RootEntry);
        NKLOG (pfsmap->ppgtree->wHeight);
    }
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

    InitDList (&pfsmap->pageoutobj.link);

    DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
                             pfsmap, phdFile, flProtect, pliSize->HighPart, pliSize->LowPart));

    if ((flProtect & ~MAP_VALID_PROT)
         || (0 == (pfsmap->dwFlags = (flProtect & (PAGE_READONLY | PAGE_READWRITE))))) {
        dwErr = ERROR_INVALID_PARAMETER;

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
                pfsmap->dwFlags |= MAP_DIRECTROM;
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
        
        // PAGE_INTERNALDBMAPPING ignored unless it's called from kernel
        if ((flProtect & PAGE_INTERNALDBMAPPING) && (g_pprcNK == pActvProc)){
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
        // Check paging/access
        //
        if (PHD_ReadFileWithSeek (phdFile, NULL, 0, NULL, NULL, 0, 0)) {
            pfsmap->dwFlags |= MAP_PAGEABLE;        
        } else if (!PHD_ReadFile (phdFile, NULL, 0, NULL, NULL)) {
            dwErr = ERROR_ACCESS_DENIED;
        }

        if (!dwErr && (pfsmap->dwFlags & PAGE_READWRITE)) {
            if (!PHD_WriteFileWithSeek (phdFile, NULL, 0, NULL, NULL, 0, 0)) {
                pfsmap->dwFlags &= ~MAP_PAGEABLE;
                if (!PHD_WriteFile (phdFile, NULL, 0, NULL, NULL)) {
                    dwErr = ERROR_ACCESS_DENIED;
                }
            }
        }

        if (dwErr) {
            DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap: Insufficient file access\r\n"));
            goto exit;
        }

        if (!(pfsmap->dwFlags & MAP_PAGEABLE)) {
            DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap: ReadFileWithSeek failed, file not pageable\r\n"));
            // atomic and fix address map requires paging support
            if (pfsmap->dwFlags & (MAP_ATOMIC|MAP_USERFIXADDR)) {
                dwErr = ERROR_NOT_SUPPORTED;
                goto exit;
            }
        }

        // Determine what additional functionality the file system supports.
        // we only use gather write on ATOMIC mapping, no need to call the IOCTL unless it's atomic
        if (pfsmap->dwFlags & MAP_ATOMIC) {

            {
                // use the existing page tree if one exists and remove it from the
                // cachefilt file map
                FILE_CACHE_INFO cacheinfo;
                DWORD dwPgTreeId = 0;

                cacheinfo.fInfoLevelId = FileCacheTransferStandard;
                if (PHD_FSIoctl (phdFile, FSCTL_SET_FILE_CACHE, 
                                 &cacheinfo, sizeof(FILE_CACHE_INFO), 
                                 &dwPgTreeId, sizeof(DWORD), 
                                 NULL, NULL)
                    && (0 != dwPgTreeId)) {
                    PPAGETREE ppgtree = (PPAGETREE)dwPgTreeId;
                    ppgtree->wFlags |= MAP_ATOMIC;
                    ppgtree->dwCacheableFileId = 0;
                    if (ppgtree->liSize.QuadPart < pfsmap->liSize.QuadPart) {
                        ppgtree->liSize.QuadPart = pfsmap->liSize.QuadPart;
                    }                    
                    PagePoolRemoveObject (&ppgtree->pageoutobj, TRUE);
                    pfsmap->ppgtree = ppgtree;
                }
            }

            //
            // check if gather write is supported
            // BUBGUG: is gather write acutally faster????
            //
            {
                DWORD dwExtendedFlags = CE_FILE_FLAG_TRANS_DATA;
                CE_VOLUME_INFO volinfo;
                CE_VOLUME_INFO_LEVEL level;
                DWORD cbReturned;
                level = CeVolumeInfoLevelStandard;
                volinfo.cbSize = sizeof(CE_VOLUME_INFO);

                if (PHD_FSIoctl (phdFile, IOCTL_FILE_GET_VOLUME_INFO,               // support get_volume_info
                                 (LPVOID) &level, sizeof(CE_VOLUME_INFO_LEVEL),
                                 (LPVOID) &volinfo, sizeof(CE_VOLUME_INFO),
                                 &cbReturned, NULL)
                    && (volinfo.dwFlags & CE_VOLUME_FLAG_WFSC_SUPPORTED)            // support wfsc
                    && (volinfo.dwFlags & CE_VOLUME_TRANSACTION_SAFE)               // support transaction
                    && PHD_FSIoctl (phdFile, FSCTL_SET_EXTENDED_FLAGS,              // support set_extended_flags
                                    (LPVOID) &dwExtendedFlags, sizeof(DWORD),
                                    NULL, 0, NULL, NULL)) {

                    pfsmap->dwFlags |= MAP_GATHER;
                    if (pfsmap->ppgtree) {
                        pfsmap->ppgtree->wFlags |= MAP_GATHER;
                    }
                }
            }
            DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap: WriteFileGather=%u\r\n",
                                     (pfsmap->dwFlags & MAP_GATHER) ? TRUE : FALSE));
        }
    }

    if (!dwErr && !(MAP_DIRECTROM & pfsmap->dwFlags)) {

        // get the page tree if file is cached
        if (phdFile && !(MAP_ATOMIC & pfsmap->dwFlags) && !pfsmap->ppgtree) {
            pfsmap->ppgtree = GetPageTree (phdFile);
            if (pfsmap->ppgtree) {
                DEBUGCHK (pfsmap->ppgtree->dwCacheableFileId);
                // add a "handle count" to the page tree
                PageTreeOpen (pfsmap->ppgtree);
                
            }
        }

        // create page tree otherwise.
        if (!pfsmap->ppgtree) {
            DWORD dwFlags = pfsmap->dwFlags & (MAP_ATOMIC|MAP_GATHER);
            if (!phdFile) {
                dwFlags |= PGT_RAMBACKED;
            } else if (MAP_PAGEABLE & pfsmap->dwFlags) {
                dwFlags |= PGT_USEPOOL;
            }
            pfsmap->ppgtree = PageTreeCreate (0, &pfsmap->liSize, NULL, 0, dwFlags);
        }
                                
        if (!pfsmap->ppgtree) {
            DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap: Failed to create page tree\r\n"));
            dwErr = ERROR_OUTOFMEMORY;

        } else if (!(MAP_PAGEABLE & pfsmap->dwFlags)
                   && !MAPReadFile (pfsmap)) {
            // Non-pageable, but failed to read the whole file
            DEBUGMSG (ZONE_MAPFILE, (L"InitFSMap: Failed to read non-pageable file into memory\r\n"));
            dwErr = ERROR_OUTOFMEMORY;

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

    if (lpName) {

        // validate parameters
        if ((flProtect & MAP_INTERNAL_FLAGS)                        // internal mapping cannot be named
            || !wcscmp (lpName, L"\\")                              // "\\" is not a valid name
            || (NULL == (pMapName = DupStrToPNAME (lpName)))) {     // other invalid name
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
            
        } else if (!KernelObjectMemoryAvailable (pprc->wMemoryPriority)) {
            
            dwErr = ERROR_OUTOFMEMORY;

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
            
            pfsmap->pageoutobj.pObj  = pfsmap->phdMap;
            pfsmap->pageoutobj.eType = eMapFile;
            
            EnterCriticalSection (&MapCS);
            AddToDListHead (&g_MapList, &pfsmap->maplink);
            LeaveCriticalSection (&MapCS);
        
            // NOTE: phdMap is not unlocked until pre-closed (removed from all lists referencing this mapfile).
            //       This way we can safely lock/unlock mapfiles while enumerating the lists.
            
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

    if (!pfsmap->phdFile && (pNewView->phdMap == pExistingView->phdMap)) {
        pefvs->pViewCntOfMapInProc = pExistingView->pViewCntOfMapInProc;
    }

    fRet = (   (MAP_VIEW_COUNT (pExistingView->dwRefCount) != 0)        // view is not being destroyed
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

    return fRet;
}

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
    
    return pExistingview;
}


// Returns the view address on success
static LPVOID AddFileBackedViewToProcess (PPROCESS pprc, PFSMAP pfsmap, PMAPVIEW pview, LPDWORD pdwErr)
{
    PMAPVIEW pExistingView;
    DWORD    dwErr = 0;
    LPVOID   pAddr = NULL;
    
    // add the view to the view list
    DEBUGCHK (!OwnPgTreeLock (pfsmap->ppgtree) && !OwnCS(&pprc->csVM));  // CS ordering rules
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
    DEBUGCHK (!OwnPgTreeLock(pfsmap->ppgtree) && !OwnCS(&pprc->csVM));  // CS ordering rules

    EnterCriticalSection (&MapCS);

    if (!IsViewCreationAllowed (pprc)) {
        // someone is creating a view on a process that is in the final stage of exiting
        DEBUGCHK (0);
        dwErr = ERROR_OUTOFMEMORY;
        
    } else {

        // Try to find an existing view to use instead
        pExistingView = FindExistingView (pprc, pview, &pview->pViewCntOfMapInProc);
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
            DEBUGCHK (pfsmap->phdFile || pExistingView->pViewCntOfMapInProc);
            pview->pViewCntOfMapInProc = NULL;
        
        } else {
            // No existing view
            LPBYTE *ppMapBase;
            DWORD  dwSearchBase;
            ViewCount_t* pNewCount = NULL;  // Track if new memory was allocated

            if (pfsmap->phdFile) {
                // read-only fixed address mapfile
                DEBUGCHK (pprc != g_pprcNK);
                DEBUGCHK (MAP_USERFIXADDR & pfsmap->dwFlags);
                DEBUGCHK (!pview->pViewCntOfMapInProc);

            } else if (pview->pViewCntOfMapInProc) {
                MAP_VALIDATE_SIG (pview->pViewCntOfMapInProc, MAP_VIEWCOUNT_SIG);
                DEBUGCHK (pview->pViewCntOfMapInProc->count);
            } else if (NULL != (pNewCount = AllocMem (HEAP_VIEWCNTOFMAPINPROC))) {
                MAP_SET_SIG (pNewCount, MAP_VIEWCOUNT_SIG);
                pNewCount->count = 0;
                pview->pViewCntOfMapInProc = pNewCount;
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
                        && (pNewCount == pview->pViewCntOfMapInProc)          // newly allocated, or r/o fixed address, where both are NULL
                        && ! VMAlloc (pprc, pfsmap->pUsrBase, pfsmap->liSize.LowPart,   // Map user VM
                                      MEM_RESERVE | MEM_MAPPED, PAGE_NOACCESS))) {

                    // failed reserving per-process mapfile VM.
                    dwErr = ERROR_OUTOFMEMORY;
                    if (pNewCount) {
                        FreeMem (pNewCount, HEAP_VIEWCNTOFMAPINPROC);
                    }
                    pview->pViewCntOfMapInProc = NULL;
                
                } else {

                    if (!pfsmap->phdFile) {
                        // ram-backed mapfile
                        MAP_VALIDATE_SIG (pview->pViewCntOfMapInProc, MAP_VIEWCOUNT_SIG);
                        InterlockedIncrement (&pview->pViewCntOfMapInProc->count);
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
        PPAGETREE ppgtree   = pfsmap->ppgtree;
        PMAPVIEW pview      = NULL;
        ULARGE_INTEGER liMapOffset;
        WORD     wViewOffset;

        DEBUGCHK (pfsmap);
        liMapOffset.HighPart = dwOffsetHigh;
        liMapOffset.LowPart = dwOffsetLow;

        dwErr = ValidateViewParameters (pfsmap, &dwAccess, &wViewOffset, &cbSize, &liMapOffset);
        if (dwErr) {
            // Fall through to error case

        } else if ((pfsmap->dwFlags & MAP_ATOMIC)
                   && (ppgtree->cViews || (cbSize >= 64*1024*VM_PAGE_SIZE))) {
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

            DWORD cPages;

            // initialize view structure
            MAP_SET_SIG (pview, MAP_VIEW_SIG);
            cbSize               += wViewOffset;
            liMapOffset.QuadPart -= wViewOffset;
            
            cPages = PAGECOUNT (cbSize);
            
            pview->base.pBaseAddr = NULL;
            pview->base.cbSize    = cbSize;
            pview->base.liMapOffset.QuadPart = liMapOffset.QuadPart;
            pview->wViewOffset    = wViewOffset;
            pview->wFlags        = (WORD) dwAccess | FILE_MAP_READ;
            pview->dwRefCount     = MAP_VIEW_INCR;
            pview->phdMap         = phdMap;

            if (!PageTreeIncView (ppgtree)) {
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
                                 cbSize, PageProtectionFromAccess (pview->wFlags), NULL)) {
                        DEBUGMSG (ZONE_MAPFILE, (L"MAPMapView: VMCopy Failed\r\n"));
                        dwErr = ERROR_OUTOFMEMORY;
                    }
                
                } else if (pfsmap->dwFlags & MAP_ATOMIC) {

                    if (!PageTreeAllocateFlushBuffer (ppgtree, pview)) {
                        DEBUGMSG (ZONE_MAPFILE, (L"MAPMapView: Failed to prepare flush buffer\r\n"));
                        dwErr = ERROR_OUTOFMEMORY;
                    }
                }

                if (!dwErr) {
                    pAddr = AddFileBackedViewToProcess (pprc, pfsmap, pview, &dwErr);
                    DEBUGMSG (ZONE_MAPFILE, (L"", DumpMapView (pview)));
                }
            }

            if (!dwErr) {
                // new view created
                if (ppgtree && ppgtree->cCommitted) {
                    // map the existing pages in page tree to view to reduce page fault.
                    // NOTE: we only map it r/o for file-backed mapfile so we don't dirty the tree.

                    DWORD dwFlags = pview->wFlags;

                    if (pfsmap->phdFile) {
                        dwFlags &= ~FILE_MAP_WRITE;
                    }
                    PageTreeMapRange (ppgtree, pprc, pview->base.pBaseAddr, &liMapOffset, cPages,
                                      PageProtectionFromAccess (dwFlags));

                    if (ppgtree->wFlags & MAP_ATOMIC) {
                        PagePoolAddObject (&pfsmap->pageoutobj);
                    }
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

static BOOL DoFlushTreeView (PPAGETREE ppgtree, PHDATA phdFile, PPROCESS pprc, PMAPVIEW pview, DWORD dwFlags, LPBYTE pReuseVM)
{
    BOOL fRet = TRUE;

    DEBUGCHK (ppgtree->dwCacheableFileId || phdFile);

    if (MAP_ATOMIC & ppgtree->wFlags) {
        
        // flush atomic map file right now. 
        fRet = PageTreeAtomicFlush (ppgtree, dwFlags);
        
    } else {

        if (pview) {
            // flush a view
            DEBUGCHK (pprc);

            // clean up the view
            fRet = CleanupView (pprc, ppgtree, pview, dwFlags);

            // if write-back requested, cleanup error is ignored as we're only interested flush error in this case
            if (VFF_WRITEBACK & dwFlags) {
                fRet = PageTreeFlush (ppgtree, phdFile, &pview->base.liMapOffset, pview->base.cbSize, NULL, pReuseVM);
            }
            
        } else {

            // flush all views in all process
            FlushAllViewsOfPageTree (ppgtree, dwFlags);

            if (dwFlags & VFF_WRITEBACK) {
                // write back the page tree
                fRet = PageTreeFlush (ppgtree, phdFile, NULL, 0, NULL, pReuseVM);
            }

            if (dwFlags & VFF_DECOMMIT) {
                // flush error is ignored if decommit flag is specified. This is only used in
                // page out code path, where we will page out whatever we can.
                fRet = PageTreePageOut (ppgtree, NULL, 0, 0);
            }
        }
    
    }

    return fRet;
}


//
// MAPUnmapView - implementation of UnmapViewOfFile
//
BOOL MAPUnmapView (PPROCESS pprc, LPVOID pAddr)
{
    PMAPVIEW pview;
    DWORD    dwErr     = ERROR_INVALID_PARAMETER;
    DWORD    dwViewRef = 1; // anything but 0

    DEBUGMSG (ZONE_MAPFILE, (L"MAPUnmapView %8.8lx %8.8lx\r\n", pprc, pAddr));

    EnterCriticalSection (&MapCS);
    pview = LockViewByAddr (pprc, &pprc->viewList, pAddr, 0, LVA_EXACT);
    if (pview) {

        BOOL      fCloseMap;
        DEBUGCHK (MAP_VIEW_COUNT (pview->dwRefCount) > 0);
        pview->dwRefCount -= MAP_VIEW_INCR;
        fCloseMap = !MAP_VIEW_COUNT (pview->dwRefCount);
        
        dwErr = 0;

        if (fCloseMap) {
            PFSMAP pfsmap;

            // release map lock
            LeaveCriticalSection (&MapCS);

            pfsmap = (PFSMAP) pview->phdMap->pvObj;
            
            if (MapFileNeedFlush (pfsmap)
                && !DoFlushTreeView (pfsmap->ppgtree, pfsmap->phdFile, pprc, pview, VFF_DECOMMIT|VFF_RELEASE, NULL)) {
                dwErr = ERROR_ACCESS_DENIED;
            }

            // acquire map lock again
            EnterCriticalSection (&MapCS);

            // release the map handle
            DoUnlockHDATA (pview->phdMap, -HNDLCNT_INCR);
        }

        // unlock the view, release VM if last ref.
        dwViewRef = UnlockView (pprc, pview);
    }
    LeaveCriticalSection (&MapCS);

    if (!dwViewRef) {
        DEBUGCHK (pview);
        ReleaseView (pprc, pview);
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }
    
    DEBUGMSG (ZONE_MAPFILE, (L"MAPUnmapView returns, dwErr %8.8lx\r\n", dwErr));

    return (ERROR_SUCCESS == dwErr);
}


//
// MAPFlushView - implement FlushViewOfFile
//
BOOL MAPFlushView (PPROCESS pprc, LPVOID pAddr, DWORD cbSize)
{
    BOOL     dwErr = ERROR_INVALID_PARAMETER;
    PMAPVIEW pview;
    BOOL     dwViewRef = 1; // anything but 0
    
    DEBUGMSG (ZONE_MAPFILE, (L"MAPFlushView %8.8lx %8.8lx, %8.8lx\r\n", pprc, pAddr, cbSize));

    EnterCriticalSection (&MapCS);
    
    // Since file-backed views will never overlap in VM and since memory-backed
    // views never need flushing, there shouldn't be any problems that cause us
    // to choose the wrong view here.
    pview = LockViewByAddr (pprc, &pprc->viewList, pAddr, cbSize, 0);
    if (pview) {
        PFSMAP    pfsmap;

        // release MapCS while flushing
        LeaveCriticalSection (&MapCS);
        pfsmap = (PFSMAP) pview->phdMap->pvObj;
        dwErr  = 0;
        
        if (MapFileNeedWriteBack (pfsmap)) {
            if (!DoFlushTreeView (pfsmap->ppgtree, pfsmap->phdFile, pprc, pview, VFF_WRITEBACK, NULL)) {
                dwErr = KGetLastError (pCurThread);
                if (!dwErr) {
                    dwErr = ERROR_OUTOFMEMORY;
                }
            }
        }

        // acquire MapCS again
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

        if (pfsmap->ppgtree) {
            VERIFY (CleanupView (pprc, pfsmap->ppgtree, pview, VFF_DECOMMIT));
        }
        
        // no need to flush the view. let preclose of map file deal with flushing

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
    PPAGETREE ppgtree = pfsmap->ppgtree;
    DEBUGMSG (ZONE_MAPFILE, (L"MAPPreClose %8.8lx\r\n", pfsmap));

    // The map-handles are closed and views are unmapped, so no more data will be
    // in the pool.  Remove the map from the list so that the pool thread won't
    // lock it anymore.
    EnterCriticalSection (&MapCS);
    RemoveDList (&pfsmap->maplink);
    InitDList (&pfsmap->maplink);  // point at self for safety against double-remove
    LeaveCriticalSection (&MapCS);

    // Remove the map from the pageout list
    PagePoolRemoveObject (&pfsmap->pageoutobj, TRUE);

    // - atomic mapping is flushed by filesys before view is unmapped. no need to flush here.
    // - cacheable mapping doesn't need to be flushed here. It'll be flushed asynchronously
    //   as needed or when the file cache manager decided to remove it from cache.
    if (MapFileNeedWriteBack (pfsmap)
        && !ppgtree->dwCacheableFileId
        && !(MAP_ATOMIC & pfsmap->dwFlags)) {
        PageTreeFlush (ppgtree, pfsmap->phdFile, NULL, 0, NULL, NULL);
    }

    // all fsmap are locked until preclose, such that the pageout function can safely access it.
    UnlockHandleData (pfsmap->phdMap);
    
    return TRUE;
}


//
// MAPClose - close a memory mapped file (no more ref in the system)
//
static BOOL MAPClose (PFSMAP pfsmap)
{
    DEBUGMSG (ZONE_MAPFILE, (L"MAPClose %8.8lx\r\n", pfsmap));

    // we shouldn't be in the pageout list 
    DEBUGCHK (IsDListEmpty (&pfsmap->pageoutobj.link));

    PagePoolRemoveObject (&pfsmap->pageoutobj, TRUE);

    // Should already have gone through the pre-close
    DEBUGCHK ((pfsmap->maplink.pFwd == &pfsmap->maplink)
              && (pfsmap->maplink.pBack == &pfsmap->maplink));

    // Avoid logging this when InitFSMAP fails.
    if (pfsmap->ppgtree || (pfsmap->dwFlags & MAP_DIRECTROM)) {
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
    if (!pfsmap->phdFile && pfsmap->pKrnBase) {

        DEBUGCHK (!pfsmap->liSize.HighPart);

        VERIFY (PageTreeViewFlush (pfsmap->ppgtree, g_pprcNK, pfsmap->pKrnBase, &liZero, pfsmap->liSize.LowPart, VFF_DECOMMIT));

        VERIFY (VMFreeAndRelease (g_pprcNK, pfsmap->pKrnBase, pfsmap->liSize.LowPart));
        pfsmap->pKrnBase = NULL;
    }
    
    if (pfsmap->ppgtree) {
        PageTreeClose (pfsmap->ppgtree);
        pfsmap->ppgtree = NULL;
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
// PageFromMapper - paging function for memory mapped files
//
DWORD PageFromMapper (PPROCESS pprc, DWORD dwAddr, BOOL fWrite)
{
    DWORD    dwRet = PAGEIN_FAILURE;
    PMAPVIEW pMapView;
    
    DEBUGMSG (ZONE_MAPFILE || ZONE_PAGING, (L"PageFromMapper: %8.8lx %8.8lx %8.8lx\r\n", pprc, dwAddr, fWrite));

    DEBUGCHK (IsPageAligned (dwAddr));

    EnterCriticalSection (&MapCS);
    pMapView = LockViewByAddr (pprc, &pprc->viewList, (LPVOID) dwAddr, VM_PAGE_SIZE, fWrite ? LVA_WRITE_ACCESS : 0);
    LeaveCriticalSection (&MapCS);
    
    
    if (pMapView) {
        ULARGE_INTEGER liFileOffset;
        DWORD           dwViewRef;
        PFSMAP          pfsmap;
        PPAGETREE       ppgtree;
        // do not use PageProtectionFromAccess (pMapView) here. We don't want to page in the page r/w unless
        // write is requested or all the view pages will be "dirty"
        DWORD  fProtect = (pMapView && IsViewExecutable (pMapView))
                        ? (fWrite ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ)
                        : (fWrite ? PAGE_READWRITE : PAGE_READONLY);

        pfsmap = (PFSMAP) pMapView->phdMap->pvObj;
        PREFAST_DEBUGCHK(pfsmap);
        DEBUGCHK (!(MAP_DIRECTROM & pfsmap->dwFlags));

        ppgtree = pfsmap->ppgtree;
        DEBUGCHK (ppgtree);
        
        // regular RAM-backed view - make it writable if view is writeable.
        if (!pfsmap->phdFile && IsViewWritable (pMapView)) {
            fWrite = TRUE;
        }

        DEBUGMSG (ZONE_MAPFILE && ZONE_PAGING, (L"PageFromMapper: pview = %8.8lx\r\n", pMapView));

        liFileOffset.QuadPart = pMapView->base.liMapOffset.QuadPart          // offset of the view in the mapfile
                              + (dwAddr - (DWORD) pMapView->base.pBaseAddr); // offset into the view
        
        // Perf shortcut: see if the file data is already in RAM somewhere
        if (PageTreeMapRange (ppgtree, pprc, (LPCVOID) dwAddr, &liFileOffset, 1, fProtect)) {
            dwRet = PAGEIN_SUCCESS;
        
        // note: none-pageable files are mapped with r/o oringally such that we can keep track of dirty pages
        //       and don't need to do unnecessary flush. It should've never failed in PageTreeMapRange above
        //       unless it's writing to r/o mapfile. In which case, we will return PAGEIN_FAILURE
        } else if (MAP_PAGEABLE & pfsmap->dwFlags) {

            LPBYTE pPagingVM = VMReserve (g_pprcNK, VM_BLOCK_SIZE, 0, 0);

            if (pPagingVM) {
                dwRet = PageTreeFetchPages (ppgtree, pfsmap->phdFile, &liFileOffset, pPagingVM);

                VMFreeAndRelease (g_pprcNK, pPagingVM, VM_BLOCK_SIZE);

                // Now add the page to the process VM
                if (PAGEIN_SUCCESS == dwRet) {
                    if (pfsmap->phdFile) {
                        PagePoolAddObject (&pfsmap->pageoutobj);
                    }

                    PageTreeMapRange (ppgtree, pprc, (LPCVOID) dwAddr, &liFileOffset, 1, fProtect);

                    // Already added to the page tree. If we failed here:  
                    // 1) The page may have been removed from the page tree by another thread.
                    // 2) The same page had been paged in by another thread.
                    // retry right away when this happen (return PAGEIN_SUCCESS)
                }

            } else {
                dwRet = PAGEIN_RETRY_MEMORY;
            }
        }

        // Check that page refcounts still make sense
        //DEBUGLOG (ZONE_MAPFILE, MapVMValidate (pfsmap));

        // unlock the view
        EnterCriticalSection (&MapCS);
        dwViewRef = UnlockView (pprc, pMapView);
        LeaveCriticalSection (&MapCS);
        if (!dwViewRef) {
            // view unmapped while we're paging
            DEBUGCHK (0);
            dwRet = PAGEIN_FAILURE;
        }
    }
    DEBUGMSG (ZONE_MAPFILE || ZONE_PAGING, (L"PageFromMapper: returns %8.8lx\r\n", dwRet));

    return dwRet;
}


BOOL MapfileInit (void)
{

    DEBUGMSG (ZONE_DEBUG, (L"Initializing Memory Mapped File Support\r\n"));
    
    InitDList (&g_MapList);
    SystemAPISets[HT_FSMAP] = &cinfMap;
    
    return (NULL != SystemAPISets[HT_FSMAP]);
}

// Worker for FlushAllViewsOfMap.  cleanup all views of a given map
static void CleanupProcessViewOfPageTree (
    PPROCESS  pprc,
    PPAGETREE ppgtree,
    DWORD     dwFlags
    )
{
    PMAPVIEW pview = (PMAPVIEW) &pprc->viewList;
    PFSMAP   pfsmap;
    PMAPVIEW pUnlock;
    DWORD    dwViewRef;

    DEBUGCHK (!(ppgtree->wFlags & MAP_ATOMIC));

    //
    // flush each view in the list until we run out of views.
    //
    for ( ; ; ) {
        
        // Cannot unlock view until after locking the next view
        pUnlock = pview;

        EnterCriticalSection (&MapCS);
        if (IsViewCreationAllowed (pprc)) {
            pview = IsDListEmpty (&pview->link)
                  ? (PMAPVIEW) pprc->viewList.pFwd         // the view is unmapped while we're flushing, restart
                  : (PMAPVIEW) pview->link.pFwd;           // still valid, just move on to next
            if (&pprc->viewList != &pview->link) {
                LockView (pview);
            }
        } else {
            pview = (PMAPVIEW) &pprc->viewList;
        }

        dwViewRef = (&pprc->viewList == &pUnlock->link)
                  ? 1                                   // anything but 0
                  : UnlockView (pprc, pUnlock);         // unlock the view
        LeaveCriticalSection (&MapCS);

        if (!dwViewRef) {
            DEBUGCHK (&pprc->viewList != &pUnlock->link);
            ReleaseView (pprc, pUnlock);
        }

        if (&pprc->viewList == &pview->link) {
            // done
            break;
        }
        pfsmap = (PFSMAP) (pview->phdMap->pvObj);

        if (pfsmap->ppgtree == ppgtree) {
            CleanupView (pprc, ppgtree, pview, dwFlags);
        }
    }
}

// Flush and/or decommit pages for a mapfile, from all views in all processes.
// Returns FALSE if any flushes failed
void FlushAllViewsOfPageTree (
    PPAGETREE ppgtree,
    DWORD     dwFlags
    )
{
    if (ppgtree->cViews) {
        PPROCESS pprc = g_pprcNK;

        DEBUGCHK (!OwnCS (&MapCS));
        DEBUGCHK (!(ppgtree->wFlags & MAP_ATOMIC));

        // Now each of the processes list
        do {
            PPROCESS pprcUnlock = pprc;
            
            CleanupProcessViewOfPageTree (pprc, ppgtree, dwFlags);

            EnterCriticalSection (&ListWriteCS);
            pprc = (PPROCESS) (IsDListEmpty (&pprc->prclist)
                 ? g_pprcNK->prclist.pFwd       // process exited during flush, restart.
                 : pprc->prclist.pFwd);         // process still valid, move on to the next.

            if (pprc != g_pprcNK) {
                LockProcessObject (pprc);
            }
            LeaveCriticalSection (&ListWriteCS);

            if (pprcUnlock != g_pprcNK) {
                UnlockProcessObject (pprcUnlock);
            }

        } while (pprc != g_pprcNK);
    }
}

BOOL PageOutOneMapFile (PFSMAP pfsmap, BOOL fDecommitOnly, LPBYTE pReuseVM)
{
    return DoFlushTreeView (pfsmap->ppgtree, pfsmap->phdFile, NULL, NULL, VFF_DECOMMIT|(fDecommitOnly? 0 : VFF_WRITEBACK), pReuseVM);
}

BOOL PageOutOnePageTree (PPAGETREE ppgtree, BOOL fDecommitOnly, LPBYTE pReuseVM)
{
    DEBUGCHK (!(ppgtree->wFlags & MAP_ATOMIC));
    DEBUGCHK (ppgtree->dwCacheableFileId);

    return DoFlushTreeView (ppgtree, NULL, NULL, NULL, VFF_DECOMMIT|(fDecommitOnly? 0 : VFF_WRITEBACK), pReuseVM);
}

// Used on suspend, shutdown.  writeback or decommit all mapfile pages.
void FlushAllMapFiles (BOOL fDiscardAtomicOnly)
{
    // keep compiler happy, &g_MapList casting to (PFSMAP) causes inconsistency.
    // So, we declare both here as PDLIST and cast to PFSMAP when needed.
    PDLIST pWalk = &g_MapList, pUnlock;
    LPBYTE pFlushVM = VMReserve (g_pprcNK, VM_BLOCK_SIZE, 0, 0);
    PFSMAP pfsmap;
    DWORD  dwFlags = fDiscardAtomicOnly? VFF_DECOMMIT : VFF_WRITEBACK;

    DEBUGCHK (pActvProc == g_pprcNK);  // Never call out to filesys if active proc != NK

    if (pFlushVM) {
        //
        // flush each mapping in the list until we run out of maps or get
        // enough memory.
        //

        for ( ; ; ) {
            
            // Cannot unlock pWalk until after locking the next map
            pUnlock = pWalk;

            EnterCriticalSection (&MapCS);
            pWalk = IsDListEmpty (pWalk)
                   ? &g_MapList                 // the map file is closed while we're flushing, restart
                   : pWalk->pFwd;               // still valid, just move on to next

            pfsmap = (PFSMAP) pWalk;
            
            if (&g_MapList != pWalk) {
                LockMap (pfsmap);
            }
            LeaveCriticalSection (&MapCS);

            if (&g_MapList != pUnlock) {
                UnlockMap ((PFSMAP) pUnlock);
            }

            if (&g_MapList == pWalk) {
                break;
            }

            if (MapFileNeedWriteBack (pfsmap)
                && (!fDiscardAtomicOnly || (MAP_ATOMIC & pfsmap->dwFlags))) {
                DoFlushTreeView (pfsmap->ppgtree, pfsmap->phdFile, NULL, NULL, dwFlags, pFlushVM);
            }
        }

        VMFreeAndRelease (g_pprcNK, pFlushVM, VM_BLOCK_SIZE);
    }
}

//
// flush function exposed to cache filter to perform tree flush as needed
//
static BOOL FSPageTreeFlush (DWORD dwPgTreeId, const ULARGE_INTEGER *pliOffset, DWORD cbSize, DWORD dwFlushFlags, BOOL* pfCancelFlush)
{
    PPAGETREE ppgtree = (PPAGETREE) dwPgTreeId;
    BOOL      fRet;

    if (ppgtree->wFlags & MAP_ATOMIC) {
        // would filesys ever call this function with atomic map?
        DEBUGCHK (0);
        fRet = PageTreeAtomicFlush (ppgtree, VFF_WRITEBACK);
    } else {
    
        ULARGE_INTEGER liOfst = {0};
        if (pliOffset) {
            liOfst.HighPart = pliOffset->HighPart;
            liOfst.LowPart  = PAGEALIGN_DOWN (pliOffset->LowPart);
            cbSize         += pliOffset->LowPart - liOfst.LowPart;
        }
        fRet = PageTreeFlush (ppgtree, NULL, &liOfst, cbSize, pfCancelFlush, NULL);

        if (!pfCancelFlush && fRet) {
            // ensure that async flush complete
            AcquireFlushLock (ppgtree);
            ReleaseFlushLock (ppgtree);
        }
    }
    return fRet;
}

//
// page tree invalidation function
//
static BOOL FSPageTreeInvalidate (DWORD dwCacheableFileId, const ULARGE_INTEGER *pliOfst, DWORD cbSize, DWORD dwFlags)
{
    BOOL fRet = (!pliOfst || IsPageAligned (pliOfst->LowPart)) && IsPageAligned (cbSize);
    DEBUGCHK (fRet);

    if (fRet) {
        if (dwFlags & PAGE_TREE_INVALIDATE_DISCARD_DIRTY) {
            dwFlags = VFF_RELEASE;
        } else {
            dwFlags = 0;
        }
        
        PageTreePageOut ((PPAGETREE) dwCacheableFileId, pliOfst, cbSize, dwFlags);
    }

    return fRet;
}

BOOL RegisterForCaching (NKCacheExports * pNKFuncs)
{
    pNKFuncs->pPageTreeCreate   = (pfnNKPageTreeCreate)  PageTreeCreate;
    pNKFuncs->pPageTreeClose    = (pfnNKPageTreeClose)   PageTreeClose;
    pNKFuncs->pPageTreeGetSize  = (pfnNKPageTreeGetSize) PageTreeGetSize;
    pNKFuncs->pPageTreeSetSize  = (pfnNKPageTreeSetSize) PageTreeSetSize;
    pNKFuncs->pIsPageTreeDirty  = (pfnNKPageTreeDirty)   PageTreeIsDirty;
    pNKFuncs->pPageTreeFlush    = FSPageTreeFlush;
    pNKFuncs->pPageTreeRead     = (pfnNKPageTreeRead)    PageTreeRead;
    pNKFuncs->pPageTreeWrite    = (pfnNKPageTreeWrite)   PageTreeWrite;
    pNKFuncs->pPageTreeInvalidate = FSPageTreeInvalidate;
    return TRUE;
}


