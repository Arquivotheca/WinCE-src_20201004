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
//
// This source code is licensed under Microsoft Shared Source License
// Version 6.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

//
//    mapfile.h - internal kernel memory mapped file header
//

#ifndef _NK_MAPFILE_H_
#define _NK_MAPFILE_H_

#include "kerncmn.h"
#include "pgtree.h"


//------------------------------------------------------------------------------
// MAIN MAPFILE FUNCTIONALITY
//------------------------------------------------------------------------------

typedef struct _ViewCount_t {
    LONG  count;

#ifdef DEBUG
    DWORD signature;  // debug safety checking
#endif // DEBUG
} ViewCount_t;

#define MAP_VIEWCOUNT_SIG       (0x11999999)

typedef struct _MAPVIEW {
    DLIST   link;               // list of views, must be first
    CommonMapView_t base;
    PHDATA  phdMap;             // the HDATA of the map file
    DWORD   dwRefCount;         // Bottom bits: Lock Count, how many threads are
                                // actively accessing the view
                                // Top bits: View Count, how many instances of
                                // the view are open at once
    WORD    wViewOffset;        // offset of the view where the view is mapped
    WORD    wFlags;             // flags/protection

    ViewCount_t* pViewCntOfMapInProc;   // RAM-backed maps only: counter shared between all views of the map

#ifdef DEBUG
    DWORD   signature;  // debug safety checking
#endif // DEBUG
} MAPVIEW;

#define MAP_VIEW_INCR           0x00010000
#define MAP_LOCK_INCR           0x00000001
#define MAP_MAX_VIEW_REFCOUNT   ((WORD)-1)
#define MAP_VIEW_COUNT(dwRef)   HIWORD(dwRef)
#define MAP_LOCK_COUNT(dwRef)   LOWORD(dwRef)


#define MAP_VIEW_SIG            (0x22999999)


typedef struct _FSMAP {
    DLIST       maplink;        // List of maps, must be first
    PHDATA      phdMap;         // Pointer back to the map handle
    PHDATA      phdFile;        // File, or NULL for just vm
    ULARGE_INTEGER liSize;      // Size of the file mapping
    DWORD       dwFlags;        // Flags/protection
    
    union {
        // Direct-ROM mappings only
        LPBYTE  pROMBase;       // Points directly to ROM
        
        // RAM-backed, or fixed-address mappings only
        struct {
            LPBYTE  pKrnBase;   // Kernel mode base for entire mapping
            LPBYTE  pUsrBase;   // User mode base for entire mapping
        };

    };

    PPAGETREE   ppgtree;        // Page tree tracking all pages for this file
    PAGEOUTOBJ  pageoutobj;     // Pageable object

#ifdef DEBUG
    DWORD       signature;      // debug safety checking
#endif // DEBUG

} FSMAP;

#define MAP_FSMAP_SIG           (0x33999999)

// NOTE: The following attributes cannot conflict with
//       PAGE_READWRITE, PAGE_READONLY, and SEC_COMMIT
#define MAP_PAGEABLE        0x1000
#define MAP_DIRECTROM       0x2000  // File mapped directly from ROM
#define MAP_ATOMIC          0x4000  // Always flush atomically (all pages or no pages) -- used by CEDB volumes and registry hives
#define MAP_GATHER          0x8000  // Perf improvement: transact with WriteFileGather when flushing.  May also be atomic.
#define MAP_USERFIXADDR     0x0200  // user views always mapped to fix address

// Flags the user can pass to CreateFileMapping
#define MAP_VALID_PROT      (PAGE_READONLY | PAGE_READWRITE | SEC_COMMIT | PAGE_INTERNALDBMAPPING | PAGE_FIX_ADDRESS_READONLY)

#define MAP_INTERNAL_FLAGS  (PAGE_INTERNALDBMAPPING)


//
// MapfileInit - init function of memory mapped file
//
BOOL MapfileInit (void);

//
// NKCreateFileMapping - CreateFileMapping API
//
HANDLE
NKCreateFileMapping (
    PPROCESS pprc,
    HANDLE hFile, 
    LPSECURITY_ATTRIBUTES lpsa,
    DWORD flProtect, 
    DWORD dwMaximumSizeHigh, 
    DWORD dwMaximumSizeLow, 
    LPCTSTR lpName 
    );

//
// MAPMapView - implementation of MapViewOfFile
//
LPVOID MAPMapView (PPROCESS pprc, HANDLE hMap, DWORD dwAccess, DWORD dwOffsetHigh, DWORD dwOffsetLow, DWORD cbSize);

//
// MAPUnmapView - implementation of UnmapViewOfFile
//
BOOL MAPUnmapView (PPROCESS pprc, LPVOID pAddr);

//
// MAPFlushView - implement FlushViewOfFile
//
BOOL MAPFlushView (PPROCESS pprc, LPVOID pAddr, DWORD cbSize);

//
// MAPUnmapAllViews - unmap all views on process exit
//
void MAPUnmapAllViews (PPROCESS pprc);

// short hand to perform cleanup.
FORCEINLINE BOOL CleanupView (
    PPROCESS  pprc,
    PPAGETREE ppgtree,
    PMAPVIEW  pview,
    DWORD     dwFlags)
{
    return PageTreeViewFlush (ppgtree, 
                              pprc, 
                              pview->base.pBaseAddr,
                              &pview->base.liMapOffset,
                              pview->base.cbSize,
                              dwFlags);

}

#define IsViewWritable(pView)   ((pView)->wFlags & FILE_MAP_WRITE)
#define IsViewExecutable(pView) ((pView)->wFlags & FILE_MAP_EXECUTE)

// MAP_VALIDATE_SIG always evaluates to TRUE so that it can be used in expressions
#ifdef DEBUG
#define MAP_SET_SIG(pObj, sig)       ((pObj)->signature = sig)
#define MAP_VALIDATE_SIG(pObj, sig)  (DEBUGCHK ((sig) == (pObj)->signature), TRUE)
#else
#define MAP_SET_SIG(pObj, sig)
#define MAP_VALIDATE_SIG(pObj, sig)  (TRUE)
#endif // DEBUG

// page out funciton
BOOL PageOutOneMapFile (PFSMAP pfsmap, BOOL fDecommitOnly, LPBYTE pResuseVM);
BOOL PageOutOnePageTree (PPAGETREE ppgtree, BOOL fDecommitOnly, LPBYTE pReuseVM);

void FlushAllViewsOfPageTree (PPAGETREE ppgtree, DWORD dwFlags);

// MapFileNeedFlush - if a map file can potentially be writen back or decommitted
FORCEINLINE BOOL MapFileNeedFlush (PFSMAP pfsmap)
{
    return pfsmap->phdFile && !(MAP_DIRECTROM & pfsmap->dwFlags);
}

// MapFileNeedWriteBack - if a map file can potentailly be writen back
FORCEINLINE BOOL MapFileNeedWriteBack (PFSMAP pfsmap)
{
    return MapFileNeedFlush (pfsmap) && (pfsmap->dwFlags & PAGE_READWRITE);
}

void FlushAllMapFiles (BOOL fDiscardAtomicOnly);


//------------------------------------------------------------------------------
// FILE CACHE FUNCTIONALITY
//------------------------------------------------------------------------------

BOOL RegisterForCaching (NKCacheExports* pNKFuncs);

extern MEMSTAT g_msSharedMasterCopy;

#endif  // _NK_MAPFILE_H_
