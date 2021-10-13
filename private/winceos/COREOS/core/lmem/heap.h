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
#ifndef _RHEAP_H_
#define _RHEAP_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ERRFALSE
#define ERRFALSE(exp)           extern char __ERRXX[(exp)!=0];
#endif

#define CE_FIXED_HEAP_MAXSIZE   (15*VM_PAGE_SIZE)   // size 60K for heap (last page intentionally left open to track buffer overrun)
#define CE_VALLOC_MINSIZE       (4*VM_PAGE_SIZE)    // use VirtualAlloc for size > 16K

//
// Sentinel Support
//
#ifdef DISABLE_HEAP_SENTINEL

    #define HEAP_SENTINELS    (0)
#include "heap16.h"    

#else 

    // Indexes where allocpc and freepc are stored in sentinel
    enum {
        AllocPcIdx = 0,
        FreePcIdx
    };

    #define RHEAP_SENTINEL_DEFAULT_FRAMES                (2)  // default # of callstack frames
    #define RHEAP_SENTINEL_DEFAULT_HDR_SIZE              (16) // 16 bytes - default sentinel size

    #ifdef x86

        // in x86 we can support from 2-6 frames


        #define RHEAP_SENTINEL_EXTRA_FRAMES              (4) // additional frames (when HEAP_SENTINEL_EXTRA_FRAMES is set)

        #ifdef HEAP_SENTINEL_EXTRA_FRAMES

            #define HEAP_SENTINELS                       (RHEAP_SENTINEL_DEFAULT_HDR_SIZE + 4 * RHEAP_SENTINEL_EXTRA_FRAMES)
            #define RHEAP_SENTINEL_HEADER_FRAMECOUNT     (RHEAP_SENTINEL_DEFAULT_FRAMES + RHEAP_SENTINEL_EXTRA_FRAMES)
#include "heap32.h"    


        #else

            #define HEAP_SENTINELS                       (RHEAP_SENTINEL_DEFAULT_HDR_SIZE)
            #define RHEAP_SENTINEL_HEADER_FRAMECOUNT     (RHEAP_SENTINEL_DEFAULT_FRAMES)
#include "heap16.h"    

        #endif // HEAP_SENTINEL_EXTRA_FRAMES

        // currently we support only 0 or 4 additional frames
        ERRFALSE ((RHEAP_SENTINEL_EXTRA_FRAMES  >= 0) && (RHEAP_SENTINEL_EXTRA_FRAMES  <= 4));

    #else

        // non-x86 we support only two frames

        #define HEAP_SENTINELS                           (RHEAP_SENTINEL_DEFAULT_HDR_SIZE)
        #define RHEAP_SENTINEL_HEADER_FRAMECOUNT         (RHEAP_SENTINEL_DEFAULT_FRAMES)
#include "heap16.h"    

    #endif // x86

    // 
    // Sentinel structure. Layout of the structure is as follows:
    // dwSig: Top 24 bits : constant value (0xA9E4B6xx)
    // dwSig: Bottom 8 bits: size of sentinel structure in dwords
    // cbSize: size of the allocation item in bytes
    // dwFrames[] : callstack frames
    // Allocation Item layout:
    // - dwFrames[AllocPcIdx to end] - callstack of LocalAlloc call
    // Free Item layout:
    // - dwFrames[FreePcIdx] - PC of LocalFree call
    // - dwFrames[AllocPcIdx to end] - callstack of LocalAlloc call
    //   (excluding the FreePcIdx index)
    //
    typedef struct _RHEAP_SENTINEL_HEADER {
        DWORD dwSig;                                          // sentinal signature
        DWORD cbSize;                                         // size of the item
        union {
            struct {
                DWORD dwAllocSentinel1;
                DWORD dwAllocSentinel2;
            };
            DWORD dwFrames[RHEAP_SENTINEL_HEADER_FRAMECOUNT]; // callstack
        };
    } RHEAP_SENTINEL_HEADER, *PRHEAP_SENTINEL_HEADER;

    #define HEAP_TAILSIG_START                0xa5
    #define HEAP_BYTE_FREE                    0xcc
    #define HEAP_DWORD_FREE                   0xcccccccc
    #define HEAP_SENTINEL_HDR_SIZE_IN_DWORDS  (HEAP_SENTINELS / 4)
    #define HEAP_SENTINEL_SIGNATURE           (0xa9e4b600 | HEAP_SENTINEL_HDR_SIZE_IN_DWORDS)

    // sentinel structure size should be 4 or 8 dwords (should be a multiple of 16 bytes)
    ERRFALSE ((HEAP_SENTINEL_HDR_SIZE_IN_DWORDS == 4) || (HEAP_SENTINEL_HDR_SIZE_IN_DWORDS == 8));

    // size of hep sentinel structure should fit in lower 8 bits of signature
    ERRFALSE (HEAP_SENTINEL_HDR_SIZE_IN_DWORDS <= 0xFF);

    // heap sentinel size must match
    ERRFALSE (HEAP_SENTINELS == sizeof(RHEAP_SENTINEL_HEADER));

#endif  // DISABLE_HEAP_SENTINEL



#define RHEAP_BLOCK_SIZE        (1 << RHEAP_BLOCK_SIZE_SHIFT)

// 2-bits per block (a block == 64 bytes)
#define RHEAP_BITS_PER_BLOCK    2           // 2 bits per block

#define RHEAP_BLOCKS_PER_PAGE   (VM_PAGE_SIZE / RHEAP_BLOCK_SIZE)
#define RHEAP_VA_BLOCK_CNT      (CE_VALLOC_MINSIZE / RHEAP_BLOCK_SIZE)

#define RHEAP_BLOCK_CNT(size)   (((size) + RHEAP_BLOCK_SIZE - 1) >> RHEAP_BLOCK_SIZE_SHIFT)
#define RHEAP_BLOCKALIGN_UP(size) (((size) + RHEAP_BLOCK_SIZE - 1) & -RHEAP_BLOCK_SIZE)

#define RHF_FREEBLOCK           0x00        // free block                   - 0b00
#define RHF_CONTBLOCK           0x01        // middle/end of allocation     - 0b01
#define RHF_HEADBIT             0x02        // head bit                     - 0b10
#define RHF_STARTBLOCK          0x03        // start of allocation          - 0b11

#define RHF_BITMASK             0x03

// a bitmap-dword == 16 blocks
#define BLOCKS_PER_BITMAP_DWORD                 16          // 32 bits / 2 bits per block
#define BLOCKS_PER_BITMAP_DWORD_SHIFT           4           // >> 4 == / 16

#define IDX2BITMAPDWORD(idx)        ((idx) >> BLOCKS_PER_BITMAP_DWORD_SHIFT)
#define IDX2BLKINBITMAPDWORD(idx)   ((idx) & (BLOCKS_PER_BITMAP_DWORD-1))

// # of blocks in a 64K region
#define NUM_DEFAULT_BLOCKS_PER_RGN          (CE_FIXED_HEAP_MAXSIZE/RHEAP_BLOCK_SIZE)
#define NUM_DEFAULT_DWORD_BITMAP_PER_RGN    (NUM_DEFAULT_BLOCKS_PER_RGN/BLOCKS_PER_BITMAP_DWORD)

// Internal: cannot conflict with any HEAP_XXX flags in winnt.h
#define HEAP_IS_PROC_HEAP         0x00002000
#define HEAP_IS_REMOTE            0x00004000
//following are defined in pkfuncs.h
//#define HEAP_CLIENT_READWRITE   0x00008000  // give client r/w access to the heap
//#define HEAP_SUPPORT_FIX_BLKS   0x00020000  // support fix size allocations with the heap
#define HEAPRGN_IS_EMBEDDED       0x00010000  // heap and rgn control structure is embedded with data (valid for local growable heaps)

#define ALIGNSIZE(x)            (((x) + 0xf) & ~0xf)
#define HEAP_MAX_ALLOC          0x40000000   // Maximum allocation request is 1GB.

#ifdef DEBUG
#define HEAP_STATISTICS
#endif

typedef struct _RHRGN       RHRGN, *PRHRGN;
typedef struct _RHEAP       RHEAP, *PRHEAP;

typedef struct _RHVAITEM    RHVAITEM, *PRHVAITEM;

#pragma warning(disable:4200) // nonstandard extensions warning

//
// Heap region structure. There are two
// types of regions: rgn which supports
// fix allocation sizes and one which
// has allocations of all different 
// sizes.
//
// Heap with support for fix alloc rgns:
// a) dwRgnData is ! 0 and describes the
// subrgn info for the rgn.
// b) prgnPrev is the previous rgn ptr
// 
// Heap with no support for fix alloc rgns:
// a) dwRgnData is either 0 if the heap is
// growable local heap or ptr to the rgn
// structure otherwise.
// b) maxBlkFree describes the max # of
// consecutive blocks free in the rgn.
//
struct _RHRGN {
    DWORD    cbSize;                     // size of region structure (for debug extention)
    PRHRGN   prgnNext;                   // link to next region
    PRHEAP   phpOwner;                   // heap where the region belongs
    union {
        DWORD    dwRgnData;              // per-region user data
        struct {
            BYTE    idxSubFree;          // index of sub rgn which has a free item
            BYTE    dfltBlkShift;        // blkcnt >> 1
            BYTE    dfltBlkCnt;          // allocation unit in blocks of 32 bytes
            BYTE    flags;               // fix alloc rgn and free list rgn flags
        };
    };        
    HANDLE   hMapfile;                   // mapfile handle, used only in remote heap
    LPBYTE   pRemoteBase;                // base address of the region (remote)
    LPBYTE   pLocalBase;                 // base address of the region (local
    DWORD    numBlkFree;                 // # of free blocks (uncommitted pages included)
    DWORD    numBlkTotal;                // # of blocks total.
    DWORD    idxBlkFree;                 // idx to a likely free block
    DWORD    idxBlkCommit;               // idx of next page to commit
    union {
        DWORD   maxBlkFree;              // max # of consective free blocks (uncommitted pages included)    
        PRHRGN  prgnPrev;                // used only with rgns which have free list
    };
    DWORD    allocMap[];                 // allocation bit maps
};

// four rgn lists by default: 0 (unused), 32, 64, 96, and 128 byte allocation rgns
#define RLIST_MAX 5

typedef struct _RGNLIST
{
    PRHRGN prgnHead;      // first rgn ptr
    PRHRGN prgnFree;      // last rgn alloc/free from
    PRHRGN prgnLast;      // last rgn ptr
} RGNLIST, *PRGNLIST;

typedef struct _SUBRGN
{
    BYTE idxBlkFree;     // index of next free block
    BYTE idxSubFree;     // index of next subrgn which has at-least one free block
    WORD idxFreeList;    // free list for this subrgn == &prrgn->allocMap[idxFreeList]
} SUBRGN, *PSUBRGN;

#define NUM_TOTAL_BLKS_IN_SUBRGN   256
#define NUM_RSVD_BLKS_IN_SUBRGN    2
#define NUM_ALLOC_BLKS_IN_SUBRGN   (NUM_TOTAL_BLKS_IN_SUBRGN - NUM_RSVD_BLKS_IN_SUBRGN)
#define NUM_BLKS_IN_SUBRGN_SHIFT   8
#define SUBRGN_ALLOC_BLK 0xFF
#define SUBRGN_END_BLK 0xFE

#define IDX2FREELISTIDX(idxBlock, prrgn)          ((idxBlock >> prrgn->dfltBlkShift) & (NUM_TOTAL_BLKS_IN_SUBRGN - 1))
#define IDX2SUBRGNIDX(idxBlock, prrgn)            ((BYTE)(idxBlock >> (NUM_BLKS_IN_SUBRGN_SHIFT + prrgn->dfltBlkShift)))
#define SUBRGNFREELIST(idxFreeList, prrgn)        ((LPBYTE) &(prrgn->allocMap[idxFreeList]))
#define IDX2SUBRGN(idxSubRgn, prrgn)              ((PSUBRGN) &(prrgn->allocMap[idxSubRgn]))

#pragma warning(default:4200) // nonstandard extensions warning

// remote heap structure
// NOTE: 1st DWORD must be signature
struct _RHEAP {
    DWORD       cbSize;                     // size of heap structure (for debug extention)
    DWORD       dwSig;                      // heap signature "Rhap", MUST BE 1st
    DWORD       dwProcessId;                // the remote process id of the heap or module base address if it is dll heap
    DWORD       cbMaximum;                  // max heap size (0 if growable)
    DWORD       flOptions;                  // option
    PRHEAP      phpNext;                    // link to next heap
    PRHVAITEM   pvaList;                    // list of VirtualAlloc'ed items
    PRHRGN      prgnLast;                   // last region of the heap
    CRITICAL_SECTION cs;                    // critial section to guard the heap
    PFN_AllocHeapMem pfnAlloc;              // allocator
    PFN_FreeHeapMem pfnFree;                // de-allocator
    PRGNLIST    pRgnList;                   // list of fix alloc rgns sorted by block size (== NULL if heap doesn't support fix blks)
    PRHRGN      prgnfree;                   // the region we last alloc/free an item
    RHRGN       rgn;                        // 1st heap region -- MUST BE LAST
};

typedef struct _RHVAITEM {
    DWORD       cbSize;                     // size of the allocation (NOT the size of the structure)
    PRHVAITEM   pNext;                      // link to next item
    PRHEAP      phpOwner;                   // heap where the item belongs
    DWORD       dwRgnData;                  // per-reservation user data
    HANDLE      hMapfile;                   // mapfile handle, used only in remote heap
    LPBYTE      pRemoteBase;                // remote base address of the item
    LPBYTE      pLocalBase;                 // local base address of the item
    DWORD       cbReserve;                  // size of reservation
} RHVAITEM, *PRHVAITEM;

// WIN32 exported functions
LPBYTE WINAPI RHeapAlloc (PRHEAP php, DWORD dwFlags, DWORD cbSize);
LPBYTE WINAPI RHeapReAlloc (PRHEAP php, DWORD dwFlags, LPVOID ptr, DWORD cbSize);
BOOL   WINAPI RHeapFree (PRHEAP php, DWORD dwFlags, LPVOID pMem);
DWORD  WINAPI RHeapSize (PRHEAP php, DWORD dwFlags, LPCVOID pMem);
PRHEAP WINAPI RHeapCreate (DWORD flOptions, DWORD dwInitialSize, DWORD dwMaximumSize);
BOOL   WINAPI RHeapDestroy (PRHEAP php);
BOOL   WINAPI RHeapValidate (PRHEAP php, DWORD dwFlags, LPCVOID pMem);
UINT   WINAPI RHeapCompact (PRHEAP php, DWORD dwFlags);

// internal used funcitons
PRHEAP DoRHeapCreate (DWORD fOptions, DWORD dwInitialSize, DWORD dwMaximumSize, PFN_AllocHeapMem pfnAlloc, PFN_FreeHeapMem pfnFree, DWORD dwProcessId);
BOOL   RHeapInit (void);
void   RHeapDeInit (void);

// heap item enumeration.
#define RHE_FREE            0x0
#define RHE_NORMAL_ALLOC    0x1
#define RHE_VIRTUAL_ALLOC   0x2
typedef BOOL (* PFN_HeapEnum) (LPBYTE pMem, DWORD cbSize, DWORD dwFlags, LPVOID pEnumData);

BOOL EnumerateHeapItems (PRHEAP php, PFN_HeapEnum pfnEnum, LPVOID pEnumData);

extern PRHEAP g_hProcessHeap, g_phpListAll;
extern CRITICAL_SECTION g_csHeapList;

//
// Is heap and rgn control structure embedded with the
// data? This is currently true only for growable local
// heaps.
//
__inline BOOL IsHeapRgnEmbedded (PRHEAP php)
{
    return (php->flOptions & HEAPRGN_IS_EMBEDDED);
}

//
// Does heap have support for fix alloc rgns?
//
__inline BOOL HeapSupportFixBlks (PRHEAP php)
{
#if HEAP_SUPPORT_FIX_BLKS != 0x00020000
    // HEAP_SUPPORT_FIX_BLKS is not defined when this is compiled under NT.
    const int HEAP_SUPPORT_FIX_BLKS = 0x00020000;
#endif // HEAP_SUPPORT_FIX_BLKS
    return (php->flOptions & HEAP_SUPPORT_FIX_BLKS);
}

//
// used with rgn dwRgnData field for fix alloc rgns
//
#define RHRGN_FLAGS_USE_FREE_LIST 0x1
#define RHRGN_FLAGS_FIX_ALLOC_RGN 0x2

//
// Does the rgn belong to a:
// a) embedded rgn AND
// b) use free list implementation
//
// Note: All fix alloc rgns do not use free list 
// implementation. Free list implementation can 
// be used only by those rgns whose default 
// allocation size is a power of two.
//
__inline BOOL FreeListRgn (PRHRGN prrgn)
{
    return ((prrgn->phpOwner->flOptions & HEAPRGN_IS_EMBEDDED) 
            && (prrgn->flags & RHRGN_FLAGS_USE_FREE_LIST));
}

//
// Does the rgn belong to a:
// a) embedded rgn AND
// b) fix allocation rgn
//
__inline BOOL FixAllocRgn (PRHRGN prrgn)
{
    return ((prrgn->phpOwner->flOptions & HEAPRGN_IS_EMBEDDED) 
            && (prrgn->flags & RHRGN_FLAGS_FIX_ALLOC_RGN));
}

#ifdef DEBUG
void RHeapDump (PRHEAP php);
#endif

// shared between rheap and kernel code
// should not conflict with PAGE_xxx flags in winnt
#define PAGE_HEAP_NOACCESS            (0x80000000)

#ifdef __cplusplus
}
#endif

#endif //  _RHEAP_H_
