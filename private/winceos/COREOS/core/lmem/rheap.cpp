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
#include <windows.h>
#include "heap.h"
#include "celog.h"
#include "coredll.h"
#include "dllheap.h"
#include <ddx.h>
extern "C"
VOID WINAPI xxx_RaiseException (DWORD dwExceptionCode, DWORD dwExceptionFlags, DWORD cArgs, CONST DWORD * lpArgs);

extern "C"
LPVOID WINAPI xxx_VirtualAllocWithTag (HANDLE hProcess, LPVOID lpAddress, DWORD dwSize, DWORD flAllocationType, DWORD flProtect, PDWORD pdwTag);

extern "C"
BOOL WINAPI xxx_VirtualFreeWithTag (HANDLE hProcess, LPVOID lpAddress, DWORD dwSize, DWORD dwFreeType, DWORD dwTag);

extern "C"
LPVOID WINAPI xxx_CeVirtualSharedAllocWithTag (LPVOID lpAddress, DWORD dwSize, DWORD fdwAction, PDWORD pdwTag);

DWORD g_dwHeapAllocTag = 0x48454150; // 'HEAP'

/*

There are two types of regions in the current heap implementation.

a) Non-fix allocation region in which allocations of all sizes are present.

b) Fix allocation region in which all allocations are exactly same size.

For fix allocation rgns, each rgn is subdivided into fix number of subrgns.
Each subrgn in fix alloc rgn has 256 blocks where each block represents 
rgn allocation dflt blk size.

For ex: for 64 byte allocation rgn, there are four subrgns pre-defined with
each subrgn of size 16k. Within each subrgn, there will be 256 blocks with
each block representing 64 bytes of data.
64x256x4 == 64k (size of a rgn)

Each subrgn is defined with the following structure:

- idxBlkFree: next free block index (0-0xFE)
if idxBlkFree == 0xFE --> subrgn is completely full

- idxSubFree: index of next subrgn
if idxSubFree == 0xFE --> all subrgns are full

- idxFreeList: offset into allocMap to get to free list
prrgn->allocMap[idxFreeList] is the free list for the subrgn

Subrgns are defined in the allocMap field of the region structure. Taking
the example of 64 byte fix allocation rgn, we know that we need four
subrgns to make up 64k block of memory. These four subrgns are
organized as follows.

allocMap[0] -- subrgn 0 structure
allocMap[1] -- subrgn 1 structure
allocMap[2] -- subrgn 2 structure
allocMap[3] -- subrgn 3 structure

In all subrgns, [0-253] are used for allocations. There are two blocks which 
are unused and have special meaning.
[254 = 0xFE] is used to mark the end of free list
[255 = 0xFF] is used to mark an allocated item.

Inside each subrgn all 256 blocks are maintained with a free list with each
block pointing to the next block as shown below.

[1][2][3][4].............................[253][254][0][0]
 ^                                              ^   ^  ^
 |[0] start of free list                        |   |  | [255] block unused
                                                |   |
                                                |   | [254] block unused
                                                |
                                                |[253] end of free list


And the free lists within each subrgn are allocated in 256 byte chunks
in allocMap after the subrgn hdrs. So for 64 byte fix allocation rgn,
subrgn free lists are placed as follows:

allocMap[4] - allocMap[67] -- first subrgn free list
allocMap[68] - allocMap[131] -- second subrgn free list
allocMap[132] - allocMap[195] -- third subrgn free list
allocMap[196] - allocMap[259] -- fourth subrgn free list

This layout for fix alloc rgns enables all heap alloc and free calls to
be O(1) for these rgns:

Heap
  |
  |__ Rgns
        |
        |_ SubRgns
              |
              |_ FreeList
                    |
                    |_ First element in free list is the free item

If free list is full, then the free list will point to subrgn full blk, at 
which time, we have to allocate a whole new region to satisfy the alloc call.

Free is always O(1) as we will push the free node at the head of the
free list and adjust the free list head ptr.

*/


/*

    Non-fix allocation RGN macros/structures

*/

// compiler intrinsic to retrieve immediate caller
EXTERN_C void * _ReturnAddress(void);
#pragma intrinsic(_ReturnAddress)

#define BLOCK_ALL_CONTINUE        0x55555555

#define AllInUse(x)             (((x) & BLOCK_ALL_CONTINUE) == BLOCK_ALL_CONTINUE)

#define RHF_SEARCH_2ND_PASS     0x01
#define RHF_IN_PLACE_ONLY       0x02

#define MAKEIDX(idxBitmap, idxSubBlk) (((idxBitmap) << BLOCKS_PER_BITMAP_DWORD_SHIFT) + (idxSubBlk))

#define BITMAPMASK(idxSubBlk)       (RHF_BITMASK << ((idxSubBlk) << 1))
#define BITMAPBITS(idxSubBlk, bits) ((bits) << ((idxSubBlk) << 1))

#define BLK2SIZE(nBlks)         ((nBlks) << RHEAP_BLOCK_SIZE_SHIFT)      // convert # of blocks to # of bytes
#define LOCALPTR(prrgn, idx)    ((prrgn)->pLocalBase  + BLK2SIZE(idx))

#define ISALLOCSTART(dwBitmap, idxSubBlk)            (((dwBitmap) & BITMAPMASK(idxSubBlk)) == (DWORD)BITMAPBITS((idxSubBlk), RHF_STARTBLOCK))

#define GROWABLE_REGION_SIZE    (sizeof (RHRGN) + NUM_DEFAULT_DWORD_BITMAP_PER_RGN * sizeof (DWORD))
#define GROWABLE_HEAP_SIZE      (sizeof (RHEAP) + NUM_DEFAULT_DWORD_BITMAP_PER_RGN * sizeof (DWORD))

static DWORD CountFreeBlocks (PRHRGN prrgn, DWORD idxFree, DWORD nBlksNeeded);

//
// alcValHead - allocation bitmap from index till end of bitmap dword belongs to the same allocation
//
// for example, alcValHead[3] == from index 3 of the dword till the end belongs to the same allocation
//
const DWORD alcValHead [BLOCKS_PER_BITMAP_DWORD] = {
        0x55555557,
        0x5555555c,
        0x55555570,
        0x555555c0,
        0x55555700,
        0x55555c00,
        0x55557000,
        0x5555c000,
        0x55570000,
        0x555c0000,
        0x55700000,
        0x55c00000,
        0x57000000,
        0x5c000000,
        0x70000000,
        0xc0000000,
};

//
// alcValTail - allocation bitmap from 0 to index belongs to the same allocation (continuation)
//
// for example, alcValTail[3] == from index 0 - 3 of the dword belongs to the same allocation
//
const DWORD alcValTail [BLOCKS_PER_BITMAP_DWORD] = {
        0x00000000,         // never referenced
        0x00000001,
        0x00000005,
        0x00000015,
        0x00000055,
        0x00000155,
        0x00000555,
        0x00001555,
        0x00005555,
        0x00015555,
        0x00055555,
        0x00155555,
        0x00555555,
        0x01555555,
        0x05555555,
        0x15555555,
};

//
// alcMasks - allocation mask
//
//      alcMask[n] masks the 1st n blocks for a bitmap dword
//     ~alcMask[n] masks the last (BLOCKS_PER_BITMAP_DWORD-n) blocks of a bitmap dword
//
const DWORD alcMasks [BLOCKS_PER_BITMAP_DWORD] = {
        0x00000000,
        0x00000003,
        0x0000000f,
        0x0000003f,
        0x000000ff,
        0x000003ff,
        0x00000fff,
        0x00003fff,
        0x0000ffff,
        0x0003ffff,
        0x000fffff,
        0x003fffff,
        0x00ffffff,
        0x03ffffff,
        0x0fffffff,
        0x3fffffff,
};

// uncomment the following line to enable subrgn checking
// - use only in debug as it will slow down alloc/free calls
//#define HEAP_VALIDATE_SUBRGNS 1

//
// Number of blks unused in a fix alloc rgn.
//
// Each subrgn has "2" blocks unused - except 
// the last one which has only 1 blk unused 
// as rgn size is < 64k.
//
//
const int g_NumUnusedBlks[RLIST_MAX] =
{
    0,                                  // generic region   (does not use subrgns)
    RHEAP_FIXBLK_NUM_UNUSED_BLKS_1,     // 1 blk fix rgn    
    RHEAP_FIXBLK_NUM_UNUSED_BLKS_2,     // 2 blk fix rgn    
    0,                                  // 3 blk fix rgn    (does not use subrgns)
    RHEAP_FIXBLK_NUM_UNUSED_BLKS_4      // 4 blk fix rgn    
};

//
// Number of sub rgns within a fix alloc rgn.
//
// Each rgn size is 60k (last page in 64k
// block is not committed).
//
// Each subrgn is given by 256 blocks where
// each block size is 16, 32, or 64 bytes.
//  
//
const int g_NumSubRgns[RLIST_MAX] =
{
    0,                                  // generic region   (does not use subrgns)
    RHEAP_FIXBLK_NUM_SUBRGNS_1,         // 1 blk fix rgn    
    RHEAP_FIXBLK_NUM_SUBRGNS_2,         // 2 blk fix rgn    
    0,                                  // 3 blk fix rgn    (does not use subrgns)
    RHEAP_FIXBLK_NUM_SUBRGNS_4          // 4 blk fix rgn    
};

//    
// Pre-computed fix alloc rgn hdr sizes.
//
// Each rgn header has size allocated for:
// a) size of the region structure
// b) link of all free items 
//    (1 byte for every block in the rgn)
//    (so total size = # of blocks in rgn)
// c) sub-rgn hdr sizes
//    (each subrgn hdr size is dword size)
// d) sentinel
// 
// The rgn hdr size is aligned to the 
// default allocation size for that rgn
//
#define FIX_ALLOC_RGN_HDR_SIZE_1    (((sizeof(RHRGN) + RHEAP_FIXBLK_NUM_SUBRGNS_1*NUM_TOTAL_BLKS_IN_SUBRGN + RHEAP_FIXBLK_NUM_SUBRGNS_1*sizeof(DWORD) + HEAP_SENTINELS) + RHEAP_FIXBLK_HDR_ALIGN_1) & ~RHEAP_FIXBLK_HDR_ALIGN_1)
#define FIX_ALLOC_RGN_HDR_SIZE_2    (((sizeof(RHRGN) + RHEAP_FIXBLK_NUM_SUBRGNS_2*NUM_TOTAL_BLKS_IN_SUBRGN + RHEAP_FIXBLK_NUM_SUBRGNS_2*sizeof(DWORD) + HEAP_SENTINELS) + RHEAP_FIXBLK_HDR_ALIGN_2) & ~RHEAP_FIXBLK_HDR_ALIGN_2)
#define FIX_ALLOC_RGN_HDR_SIZE_4    (((sizeof(RHRGN) + RHEAP_FIXBLK_NUM_SUBRGNS_4*NUM_TOTAL_BLKS_IN_SUBRGN + RHEAP_FIXBLK_NUM_SUBRGNS_4*sizeof(DWORD) + HEAP_SENTINELS) + RHEAP_FIXBLK_HDR_ALIGN_4) & ~RHEAP_FIXBLK_HDR_ALIGN_4)

//
// Rgn hdr size normalized to rgn allocation size
//
const DWORD g_dwRgnHdrSize[RLIST_MAX] =
{
    GROWABLE_REGION_SIZE+HEAP_SENTINELS,    // generic region   (does not use subrgns)
    FIX_ALLOC_RGN_HDR_SIZE_1,               // 1 blk fix rgn
    FIX_ALLOC_RGN_HDR_SIZE_2,               // 2 blk fix rgn
    GROWABLE_REGION_SIZE+HEAP_SENTINELS,    // 3 blk fix rgn    (does not use subrgns)
    FIX_ALLOC_RGN_HDR_SIZE_4,               // 4 blk fix rgn
};

//
// Total number of blocks in a rgn (in terms of rgn default allocation size)
//
const DWORD g_dwRgnTotalBlks[RLIST_MAX] =
{
    0, // unused
    NUM_DEFAULT_BLOCKS_PER_RGN,         // 1 blk fix rgn
    NUM_DEFAULT_BLOCKS_PER_RGN >> 1,    // 2 blk fix rgn
    NUM_DEFAULT_BLOCKS_PER_RGN,         // 3 blk fix rgn
    NUM_DEFAULT_BLOCKS_PER_RGN >> 2,    // 4 blk fix rgn
};
    
//
// Mapping from block index to sub rgn array index and free list index within that
//
#define MAKEIDXFX(idxFreeBlock, idxSubRgn, shift) ((idxFreeBlock << shift) | (idxSubRgn << (NUM_BLKS_IN_SUBRGN_SHIFT + shift)))                   

static DWORD CountFreeBlocksFx (PRHRGN prrgn, DWORD idxFree);
    
PRHEAP g_hProcessHeap;
BOOL   g_fFullyCompacted;
CRITICAL_SECTION g_csHeapList;
PRHEAP g_phpListAll;

// structure used by dll heap implementation
MODULEHEAPINFO g_ModuleHeapInfo;

#ifdef HEAP_STATISTICS

#define NUM_ALLOC_BUCKETS 64
#define BUCKET_VIRTUAL (NUM_ALLOC_BUCKETS - 1)
#define BUCKET_LARGE   (NUM_ALLOC_BUCKETS - 2)
LONG lNumAllocs[NUM_ALLOC_BUCKETS];

LONG lTotalIterations;
LONG lTotalAllocations;

#endif

ERRFALSE (offsetof(RHRGN, dwRgnData)   == offsetof(RHVAITEM, dwRgnData));
ERRFALSE (offsetof(RHRGN, hMapfile)    == offsetof(RHVAITEM, hMapfile));
ERRFALSE (offsetof(RHRGN, pRemoteBase) == offsetof(RHVAITEM, pRemoteBase));
ERRFALSE (offsetof(RHRGN, pLocalBase)  == offsetof(RHVAITEM, pLocalBase));
ERRFALSE (offsetof(RHRGN, phpOwner)    == offsetof(RHVAITEM, phpOwner));
ERRFALSE (offsetof(RHEAP, rgn) + sizeof(RHRGN) == sizeof (RHEAP));

// HEAP CHECK SUPPORT
#ifndef SHIP_BUILD
extern  DBGPARAM    dpCurSettings;
#if HEAP_SENTINELS
#if defined(x86)
#define HEAP_CHECKS 1
#else
#define HEAP_CHECKS 0
#endif // x86
#else
#define HEAP_CHECKS 0
#endif // HEAP_SENTINELS
#else
#define HEAP_CHECKS 0
#endif // SHIP_BUILD

#if HEAP_CHECKS
#define IsPageOutEnabled(php) (DBGMEMCHK && ((php)->flOptions & HEAP_FREE_CHECKING_ENABLED))
#else
#define IsPageOutEnabled(php) ((php) != (php))
#endif

#if HEAP_SENTINELS

#ifdef x86

#include <loader.h>

#define IsDwordAligned(p)       (!((DWORD) (p) & 3))

// coredll or k.coredll base and size
DWORD g_dwCoreCodeBase;
DWORD g_dwCoreCodeSize;

static __inline BOOL IsValidStkFrame(DWORD dwFrameAddr, DWORD stkLow, DWORD stkHigh)
{
    return (dwFrameAddr 
            && (dwFrameAddr > stkLow) 
            && (dwFrameAddr < stkHigh) 
            && IsDwordAligned(dwFrameAddr));
}

static void GetCallerFrames(PRHEAP_SENTINEL_HEADER pHead, BOOL fAllocSentinel)
{
    DWORD dwEbp = 0;
    DWORD dwReturnAddr = 0;
    DWORD dwMaxCoredllFrames = 10;
    BOOL fNonCoredllFrame = FALSE;
    LPDWORD tlsptr = (LPDWORD) UTlsPtr ();
    static const WORD RetAddrOffset = sizeof(DWORD);
    DWORD LowStackLimit = tlsptr[PRETLS_STACKBOUND];
    DWORD HighSTackLimit = tlsptr[PRETLS_TLSBASE] - RetAddrOffset;
    
    __try {

        __asm {
            mov dwEbp, ebp
        }

        // loop through dwMaxCoredllFrames until we see non-coredll frame
        while (dwEbp && dwMaxCoredllFrames--) {
            
            // check for valid frame pointer
            if (!IsValidStkFrame(dwEbp, LowStackLimit, HighSTackLimit)) {
                break;
            }

            //  check for return address
            dwReturnAddr = *(LPDWORD)(dwEbp + RetAddrOffset);
            if ((dwReturnAddr - g_dwCoreCodeBase) < g_dwCoreCodeSize) {
                dwEbp = *(LPDWORD)(dwEbp);  // frame is in coredll/kcoredll; skip this frame

            } else {
                fNonCoredllFrame = TRUE;
                break;
            }
        }

        if (fNonCoredllFrame) {
            
            if (fAllocSentinel) {
                // setting alloc sentinel - store the callstack               
                DWORD iFrame = 0;

                do {

                    // store the current frame                    
                    pHead->dwFrames[iFrame] = dwReturnAddr;
                    dwEbp = *(LPDWORD)(dwEbp);

                    // check if we reached end or invalid frame
                    if (!IsValidStkFrame(dwEbp, LowStackLimit, HighSTackLimit)) {
                        break;
                    }

                    // get the next frame
                    dwReturnAddr = *(LPDWORD)(dwEbp + RetAddrOffset);
                } while (++iFrame < RHEAP_SENTINEL_HEADER_FRAMECOUNT);
                
            } else {
                // setting free sentinel - store the current frame
                pHead->dwFrames[FreePcIdx] = dwReturnAddr;
            }
        }

    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    return;
}

#endif // x86

// dump all allocation details between two memory locations
void RHeapDumpAllocations (DWORD dwCorruptionAddress, PRHEAP_SENTINEL_HEADER pFirstHdr, PRHEAP_SENTINEL_HEADER pLastHdr)
{
    LPDWORD pdwMem = (LPDWORD) pFirstHdr;
    PRHEAP_SENTINEL_HEADER pHdr = NULL;
    DWORD cdwSentinelSize = sizeof(RHEAP_SENTINEL_HEADER) / sizeof(DWORD);

    while ((DWORD)pdwMem <= (DWORD)pLastHdr) {
        if (HEAP_SENTINEL_SIGNATURE == *pdwMem) {
            pHdr = (PRHEAP_SENTINEL_HEADER)pdwMem;
            if ((dwCorruptionAddress - (DWORD)(pdwMem+cdwSentinelSize)) <= pHdr->cbSize) {
                RETAILMSG(1, (L"RHeapCheckFreeSentinels: Previous Allocation details @0x%8.8lx Size:0x%8.8lx FreePC:0x%8.8lx AllocPC:0x%8.8lx\r\n",
                                        dwCorruptionAddress, pHdr->cbSize, pHdr->dwFrames[FreePcIdx], pHdr->dwFrames[AllocPcIdx]));
            }
        }
        pdwMem += cdwSentinelSize;
    }
}

void RHeapSetAllocSentinels (LPBYTE pMem, DWORD cbSize, BOOL fInternal)
{
    PRHEAP_SENTINEL_HEADER pHead = (PRHEAP_SENTINEL_HEADER) pMem;
    DWORD cbUsed    = cbSize + HEAP_SENTINELS;
    DWORD cbTail    = BLK2SIZE (RHEAP_BLOCK_CNT (cbUsed)) - cbUsed;
    BYTE  chTailSig = HEAP_TAILSIG_START;
    DWORD dwCaller = (DWORD)_ReturnAddress();

    PREFAST_DEBUGCHK ((int) cbSize > 0);
    DEBUGMSG (DBGHEAP2, (L"RHeapSetAllocSentinels for pMem = %8.8lx of size %8.8lx\r\n", pMem, cbSize));

    pMem += cbUsed;   // pMem == beginning of un-allocated memory of the allocation

    pHead->cbSize = cbSize;
    pHead->dwSig  = HEAP_SENTINEL_SIGNATURE;
    
#ifdef x86

    // free previous allocation details
    memset(pHead->dwFrames, 0, RHEAP_SENTINEL_HEADER_FRAMECOUNT * sizeof(DWORD));

    // if called from internal, use immediate caller
    if (fInternal) {
        pHead->dwFrames[AllocPcIdx] = dwCaller;

    } else {
        GetCallerFrames(pHead, TRUE);
        if (!pHead->dwFrames[AllocPcIdx]) {
            // no frames outside of coredll; use immediate caller
            pHead->dwFrames[AllocPcIdx] = dwCaller;
        }
    }

#else
    pHead->dwFrames[AllocPcIdx] = dwCaller;
#endif

    while (cbTail --) {
        *pMem ++ = chTailSig ++;
    }

}

// RHeapCheckAllocSentinels returns the actual item size (-1 if invalid)
DWORD RHeapCheckAllocSentinels (LPBYTE pMem, DWORD nAllocBlks, BOOL fStopOnError)
{
    // NOTE: we're making a local copy of the sentinel head, to make sure size don't change asynchronously
    RHEAP_SENTINEL_HEADER head = *(PRHEAP_SENTINEL_HEADER) pMem;

    DWORD cbAlloc = BLK2SIZE (nAllocBlks);
    DWORD cbSize  = cbAlloc - HEAP_SENTINELS;
    LPBYTE pMemCorrupt = NULL;
    DWORD dwError      = NULL;

    PREFAST_DEBUGCHK (((int) cbAlloc > 0) && ((int) cbSize > 0));

    if (HEAP_SENTINEL_SIGNATURE != head.dwSig) {
        pMemCorrupt = (LPBYTE) &(((PRHEAP_SENTINEL_HEADER)pMem)->dwSig);
        dwError     = HEAP_ITEM_SIGNATURE_CORRUPTION;

        RETAILMSG(1, (L"RHeapCheckAllocSentinels: Bad head signature @0x%08X  --> 0x%08X-0x%08X\r\n",
                pMem, head.dwSig, HEAP_SENTINEL_SIGNATURE));

    } else if (head.cbSize > cbSize) {
        pMemCorrupt = (LPBYTE) &(((PRHEAP_SENTINEL_HEADER)pMem)->cbSize);
        dwError     = HEAP_ITEM_SIZE_CORRUPTION;

        RETAILMSG(1, (L"RHeapCheckAllocSentinels: Bad item size @0x%08X  --> 0x%08X SHDB at most 0x%08X\r\n",
                pMem, head.cbSize, cbAlloc - HEAP_SENTINELS));

    } else {
        BYTE chTailSig = HEAP_TAILSIG_START;
        LPBYTE pMemTail;

        cbSize   = head.cbSize;
        cbAlloc -= cbSize + HEAP_SENTINELS;
        pMemTail = pMem + cbSize + HEAP_SENTINELS;


        for ( ; cbAlloc; cbAlloc --, pMemTail ++, chTailSig ++) {
            if (*pMemTail != chTailSig) {
                pMemCorrupt = pMemTail;
                dwError     = HEAP_ITEM_TAIL_CORRUPTION;

                RETAILMSG(1, (L"RHeapCheckAllocSentinels: Bad tail signature item: 0x%08X  @0x%08X -> 0x%02x SHDB 0x%02x\r\n",
                        pMem, pMemTail, *pMemTail, chTailSig));
                break;
            }
        }
    }

    if (cbAlloc) {
        RETAILMSG(1, (L"RHeapCheckAllocSentinels: Previous Allocation details Size:0x%8.8lx FreePC:0x%8.8lx AllocPC:0x%8.8lx\r\n",
                                        head.cbSize, head.dwFrames[FreePcIdx], head.dwFrames[AllocPcIdx]));

        RETAILMSG(1, (L"RHeapCheckAllocSentinels: Raising exception ...\r\n"));

        if (fStopOnError) {
            // Notify DDx of potential heap corruption 
            __try {
                DWORD exceptInfo[4] = {dwError, (DWORD)pMem, (DWORD)pMemCorrupt, BLK2SIZE (nAllocBlks)};
                xxx_RaiseException((DWORD)STATUS_HEAP_CORRUPTION, 0, _countof(exceptInfo), exceptInfo);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
            }
        }
    }

    return cbSize;
}

void RHeapSetFreeSentinels (LPBYTE pMem, DWORD nBlks, DWORD dwCaller)
{
    DWORD cbSize = BLK2SIZE (nBlks);
    DWORD cbSentinelSize = sizeof(RHEAP_SENTINEL_HEADER);
    PRHEAP_SENTINEL_HEADER pHdr = (PRHEAP_SENTINEL_HEADER) pMem;

    DEBUGMSG (DBGHEAP2, (L"RHeapSetFreeSentinels from pMem = %8.8lx of size %8.8lx\r\n", pMem, cbSize));

    if ((cbSize >= cbSentinelSize) && (pHdr->dwSig == HEAP_SENTINEL_SIGNATURE)) {

        // set the default caller
        pHdr->dwFrames[FreePcIdx] = (dwCaller) ? dwCaller : (DWORD)_ReturnAddress();

#ifdef x86
        // get additional stack frames on x86
        GetCallerFrames(pHdr, FALSE);
#endif

        // skip the sentinel
        pMem += cbSentinelSize;
        cbSize -= cbSentinelSize;
    }

    memset (pMem, HEAP_BYTE_FREE, cbSize);
}

BOOL RHeapCheckFreeSentinels (LPBYTE pMem, DWORD nBlks, BOOL fStopOnError)
{
    DWORD cdwFree = BLK2SIZE (nBlks) / sizeof(DWORD);
    LPDWORD pdwMem = (LPDWORD) pMem;

    PRHEAP_SENTINEL_HEADER pHdrFirst = NULL, pHdrLast = NULL;
    DWORD cdwSentinelSize = sizeof(RHEAP_SENTINEL_HEADER) / sizeof(DWORD);

    while (cdwFree) {

        if ((HEAP_SENTINEL_SIGNATURE == *pdwMem) && (cdwFree >= cdwSentinelSize)) {
            // record the last known sentinel
            if (!pHdrFirst)
                pHdrFirst = (PRHEAP_SENTINEL_HEADER)pdwMem;
            pHdrLast = (PRHEAP_SENTINEL_HEADER)pdwMem;

            // skip the sentinel
            cdwFree -= cdwSentinelSize;
            pdwMem += cdwSentinelSize;

        } else {

        if (HEAP_DWORD_FREE != *pdwMem) {
                RETAILMSG(1, (L"RHeapCheckFreeSentinels: Bad free item: 0x%08X  @0x%08X -> 0x%02x SHDB 0x%02x.  Raising exception ...\r\n",
                        pMem, pdwMem, *pdwMem, HEAP_DWORD_FREE));

                if (pHdrLast) {
                    RHeapDumpAllocations((DWORD)pdwMem, pHdrFirst, pHdrLast);
                }

                if (fStopOnError) {
                    // Notify DDx of potential heap corruption
                    __try {
                        DWORD exceptInfo[4] = {HEAP_FREE_ITEM_CORRUPTION, (DWORD)pMem, (DWORD)pdwMem, BLK2SIZE (nBlks)};
                    xxx_RaiseException((DWORD)STATUS_HEAP_CORRUPTION, 0, _countof(exceptInfo), exceptInfo);
                    }
                __except(EXCEPTION_EXECUTE_HANDLER) {
                    }
                }
            break;
        }
            cdwFree--;
            pdwMem++;
        }
    }

    return !cdwFree;
}

//
// Used to move the sentinel when carving out an 
// allocation from a block which has been freed 
// before.
//
void RHeapMoveSentinel (PRHRGN prrgn, DWORD dwIdxStart, DWORD cbSize)
{
    PRHEAP_SENTINEL_HEADER psrc = (PRHEAP_SENTINEL_HEADER) LOCALPTR (prrgn, dwIdxStart);    
    if (HEAP_SENTINEL_SIGNATURE == psrc->dwSig) {

        DWORD nAlloc = RHEAP_BLOCK_CNT(cbSize + HEAP_SENTINELS);
        DWORD nMin = RHEAP_BLOCK_CNT(HEAP_SENTINELS);
        DWORD dwIdxEnd = dwIdxStart + nAlloc;
        
        // check if we have enough room to move the sentinel
        if (dwIdxEnd < prrgn->idxBlkCommit) {
            if ((FreeListRgn (prrgn) && (CountFreeBlocksFx (prrgn, dwIdxEnd) >= nMin))
                || (!(FreeListRgn (prrgn)) && (CountFreeBlocks (prrgn, dwIdxEnd, nMin) >= nMin))) {
                PRHEAP_SENTINEL_HEADER pdst = (PRHEAP_SENTINEL_HEADER) LOCALPTR (prrgn, dwIdxEnd);
                memcpy (pdst, psrc, sizeof(RHEAP_SENTINEL_HEADER));
            }
        }
    }
}

LPBYTE RHeapSentinelAdjust (LPBYTE pMem)
{
    return pMem? (pMem - HEAP_SENTINELS) : NULL;
}

BOOL EnumValidate (LPBYTE pMem, DWORD cbSize, DWORD dwFlags, LPVOID pEnumData)
{
    BOOL fRet;

    if (RHE_FREE == dwFlags) {
        fRet = RHeapCheckFreeSentinels (pMem, RHEAP_BLOCK_CNT (cbSize), TRUE);
    } else {
        fRet = RHeapCheckAllocSentinels (pMem, RHEAP_BLOCK_CNT (cbSize), TRUE);
    }

    return fRet;
}

BOOL RHeapValidateSentinels (PRHEAP php)
{
    return EnumerateHeapItems (php, EnumValidate, NULL);
}

#endif // HEAP_SENTINELS

//
// MarkPagesNoAccess - Mark the given address range as n/a
//
static void MarkPagesNoAccess (PRHRGN prrgn, LPBYTE pMem, DWORD cbSize)
{
    // page out enabled and not the first page of the region
    if (IsPageOutEnabled(prrgn->phpOwner) 
        && (PAGEALIGN_DOWN((DWORD)prrgn->pLocalBase) != PAGEALIGN_DOWN((DWORD)pMem))) {
        VirtualProtectEx (hActiveProc, pMem, cbSize, PAGE_HEAP_NOACCESS, &cbSize);
    }            
}

static LPVOID AllocMemShare (LPVOID pAddr, DWORD cbSize, DWORD fdwAction, LPDWORD pdwUnused)
{
    return xxx_CeVirtualSharedAllocWithTag ((MEM_RESERVE & fdwAction)? NULL : pAddr, cbSize, fdwAction, &g_dwHeapAllocTag);
}

static LPVOID AllocMemShareExecutable (LPVOID pAddr, DWORD cbSize, DWORD fdwAction, LPDWORD pdwUnused)
{
    LPVOID p;
    if (MEM_RESERVE & fdwAction) {
        p = xxx_CeVirtualSharedAllocWithTag (NULL, cbSize, fdwAction, &g_dwHeapAllocTag);
    } else {
        p = xxx_VirtualAllocWithTag (hActiveProc, pAddr, cbSize, fdwAction, PAGE_EXECUTE_READWRITE, &g_dwHeapAllocTag);
    }
    return p;
}

static LPVOID AllocMemVirt (LPVOID pAddr, DWORD cbSize, DWORD fdwAction, LPDWORD pdwUnused)
{
    LPVOID p =  xxx_VirtualAllocWithTag (hActiveProc, pAddr, cbSize, fdwAction, PAGE_READWRITE, &g_dwHeapAllocTag);
    DEBUGMSG (DBGFIXHP && (fdwAction == MEM_RESERVE), (L"Reserving new VM of size %8.8lx, returns %8.8lx\r\n", cbSize, p));
    DEBUGMSG (DBGFIXHP && (fdwAction == MEM_COMMIT), (L"Commit memory at 0x%8.8lx of size %8.8lx, returns %8.8lx\r\n", pAddr, cbSize, p));
    return p;
}

static LPVOID AllocMemVirtExecutable (LPVOID pAddr, DWORD cbSize, DWORD fdwAction, LPDWORD pdwUnused)
{
    LPVOID p =  xxx_VirtualAllocWithTag (hActiveProc, pAddr, cbSize, fdwAction, PAGE_EXECUTE_READWRITE, &g_dwHeapAllocTag);
    DEBUGMSG (DBGFIXHP && (fdwAction == MEM_RESERVE), (L"Reserving new VM of size %8.8lx, returns %8.8lx\r\n", cbSize, p));
    DEBUGMSG (DBGFIXHP && (fdwAction == MEM_COMMIT), (L"Commit executable memory at 0x%8.8lx of size %8.8lx, returns %8.8lx\r\n", pAddr, cbSize, p));
    return p;
}

static BOOL FreeMemVirt (LPVOID pAddr, DWORD cbSize, DWORD fdwAction, DWORD dwUnused)
{
    BOOL fRet = xxx_VirtualFreeWithTag (hActiveProc, pAddr, cbSize, fdwAction, g_dwHeapAllocTag);

    DEBUGMSG (DBGFIXHP && (fdwAction == MEM_RELEASE), (L"Releasing VM at address %8.8lx (cbSize = %d)\r\n", pAddr, cbSize));
    DEBUGMSG (DBGFIXHP && (fdwAction == MEM_DECOMMIT), (L"De-commit memory at 0x%8.8lx of size %8.8lx\r\n", pAddr, cbSize));
    return fRet;
}

static void ReleaseMapMemory (HPROCESS hProc, PRHVAITEM pvaitem)
{
    if (hProc && pvaitem->pRemoteBase) {
        // the following call can fail, if the process exited before we get a chance to make the call. But it's okay
        // because process exit automatically cleaned up all its views.
        UnmapViewInProc (hProc, pvaitem->pRemoteBase);
        pvaitem->pRemoteBase = NULL;
    }
    if (pvaitem->pLocalBase) {
        VERIFY (UnmapViewOfFile (pvaitem->pLocalBase));
        pvaitem->pLocalBase = NULL;
    }
    if (pvaitem->hMapfile) {
        VERIFY (CloseHandle (pvaitem->hMapfile));
        pvaitem->hMapfile  = NULL;
    }
}

static BOOL FreeMemMappedFile (LPVOID pAddr, DWORD cbSize, DWORD fdwAction, DWORD dwRsvData)
{
    // no-op if MEM_DECOMMIT, for we don't have a way to discard view pages yet
    if (MEM_RELEASE == fdwAction) {
        PRHVAITEM pvaitem = (PRHVAITEM) dwRsvData;
        HANDLE hProc = OpenProcess (PROCESS_ALL_ACCESS, 0, pvaitem->phpOwner->dwProcessId);
        DEBUGCHK (!cbSize);
        DEBUGCHK (pAddr == pvaitem->pLocalBase);

        // if hProc is NULL, the process has exited, but we still need to cleanup local resources
        ReleaseMapMemory (hProc, pvaitem);
        
        if (hProc) {
            VERIFY (CloseHandle (hProc));
        }
    }
    return TRUE;
}

static LPVOID AllocMemMappedFile (LPVOID pAddr, DWORD cbSize, DWORD fdwAction, LPDWORD pRsvData)
{
    if (MEM_RESERVE == fdwAction) {
        PRHVAITEM pvaitem = (PRHVAITEM) *pRsvData;
        HANDLE hProc = OpenProcess (PROCESS_ALL_ACCESS, 0, pvaitem->phpOwner->dwProcessId);
        DWORD dwClientProtection = (pvaitem->phpOwner->flOptions & HEAP_CLIENT_READWRITE)? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;

        DEBUGCHK (!pAddr);
        DEBUGCHK (GetCurrentProcessId () != pvaitem->phpOwner->dwProcessId);

        if (hProc) {
            if (   (NULL != (pvaitem->hMapfile    = CreateFileMapping (INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, cbSize, NULL)))
                && (NULL != (pvaitem->pLocalBase  = (LPBYTE) MapViewOfFile (pvaitem->hMapfile, FILE_MAP_ALL_ACCESS, 0, 0, 0)))
                && (NULL != (pvaitem->pRemoteBase = (LPBYTE) MapViewInProc (hProc, pvaitem->hMapfile, dwClientProtection, 0, 0, 0)))) {
                pAddr = pvaitem->pLocalBase;
            } else {
                // failed
                ReleaseMapMemory (hProc, pvaitem);
            }
            VERIFY (CloseHandle (hProc));
        }
    } else {

        // just touches all the pages with LocalBase
        DWORD dwDummy;
        volatile BYTE *pCurr = (volatile BYTE *) pAddr;
        DEBUGCHK (MEM_COMMIT == fdwAction);
        DEBUGCHK (pAddr);
        while ((int) cbSize > 0) {
            dwDummy = *pCurr;
            cbSize -= VM_PAGE_SIZE;
            pCurr  += VM_PAGE_SIZE;
        }
    }
    return pAddr;
}

/*++

Routine Description:

    This helper routine is used to validate a subrgn.

Arguments:

    prrgn - rgn pointer.
    idxSubRgn - subrgn index.

Return Value:

    TRUE/FALSE

--*/
static BOOL ValidateSubRgn (PRHRGN prrgn, DWORD idxSubRgn)
{
    BOOL fRet = TRUE;
    PSUBRGN pSubRgn = IDX2SUBRGN(idxSubRgn, prrgn);
    LPBYTE pFreeList = SUBRGNFREELIST(pSubRgn->idxFreeList, prrgn);
    
    if (SUBRGN_END_BLK == pSubRgn->idxBlkFree) {
        // all blocks in subrgn are allocated; check for bad blks in free list.
        int idx = 0;
        BYTE blkbit = 0;
        do {
            blkbit = pFreeList[idx];
            if ((blkbit && (blkbit != SUBRGN_ALLOC_BLK)) || (idx > NUM_ALLOC_BLKS_IN_SUBRGN)) {
                NKDbgPrintfW (L"alloc blk [%d][%d] in rgn 0x%8.8lx is bad\r\n", idxSubRgn, idx, prrgn);
                DebugBreak();
                fRet = FALSE;
                break;
            }
            idx++;
        } while (blkbit == SUBRGN_ALLOC_BLK);

    } else {
        // subrgn has free blocks; check for bad blks and loops in free list.
        BYTE nodesvisited[256] = {0};
        int idx = pSubRgn->idxBlkFree;
        do {
            BYTE nxtidx = pFreeList[idx];
            if (nxtidx == SUBRGN_ALLOC_BLK) {
                NKDbgPrintfW (L"free blk [%d][%d] in rgn 0x%8.8lx is bad\r\n", idxSubRgn, idx, prrgn);
                DebugBreak();
                fRet = FALSE;
                break;
            }
            if (nodesvisited[idx]) {
                NKDbgPrintfW (L"loop in free blks [%d][%d] in rgn 0x%8.8lx is bad\r\n", idxSubRgn, idx, prrgn);
                DebugBreak();
                fRet = FALSE;
                break;
            }
            nodesvisited[idx] = 0x1;
            idx = nxtidx;
        } while (idx != SUBRGN_END_BLK);
    }            

    return fRet;
}

/*++

Routine Description:

    This helper routine is used to take 'nBlks' from
    the given rgn. The caller should call this only
    if the given rgn has at-least one free block.

    This function updates the subrgn from where
    the block is being allocated.

Arguments:

    prrgn - rgn pointer.
    nBlks - # of blocks to take. This has to be same
    as rgn default allocation size.

Return Value:

    none.

--*/
static void TakeBlocksFx (PRHRGN prrgn, DWORD nBlks)
{
    if (nBlks) {
        DWORD idxSubFree = prrgn->idxSubFree;
        PSUBRGN pSubRgn = IDX2SUBRGN(idxSubFree, prrgn);
        DWORD idxBlkFree = pSubRgn->idxBlkFree;
        LPBYTE pFreeList = SUBRGNFREELIST(pSubRgn->idxFreeList, prrgn);

        DEBUGCHK (nBlks == prrgn->dfltBlkCnt);       
        DEBUGCHK (prrgn->numBlkFree >= nBlks);
        DEBUGCHK (FreeListRgn (prrgn));
        DEBUGCHK (pSubRgn->idxBlkFree != SUBRGN_END_BLK);
        
        // rgn book keeping
        prrgn->numBlkFree -= nBlks;
            
        // free list book keeping
        pSubRgn->idxBlkFree = pFreeList[idxBlkFree];  
        pFreeList[idxBlkFree] = SUBRGN_ALLOC_BLK;

        // if subrgn is empty now. head moves to next subrgn.
        if (SUBRGN_END_BLK == pSubRgn->idxBlkFree) {
            prrgn->idxSubFree = pSubRgn->idxSubFree;
#ifdef HEAP_VALIDATE_SUBRGNS            
            ValidateSubRgn (prrgn, idxSubFree);            
#endif
        }
    }
}

//
// TakeBlock - mark 'nBlks' blocks from 'idxBlock' in-use.
//
// NOTE: TakeBlocks doesn't update maxBlkFree, caller of this function is responsible
//       of updating maxBlkFree
//
static void TakeBlocks (PRHRGN prrgn, DWORD idxBlock, DWORD nBlks)
{    
    DWORD idxBitmap = IDX2BITMAPDWORD(idxBlock);
    DWORD idxSubBlk = IDX2BLKINBITMAPDWORD(idxBlock);
    DWORD dwMask    = alcValHead[idxSubBlk];

    DEBUGCHK (nBlks);
    DEBUGCHK (prrgn->numBlkFree >= nBlks);
    DEBUGCHK (idxBlock + nBlks <= prrgn->idxBlkCommit);
    DEBUGCHK (!FreeListRgn (prrgn));

    prrgn->numBlkFree -= nBlks;
    prrgn->idxBlkFree = idxBlock + nBlks;

    // take care of head blocks
    if (nBlks + idxSubBlk < BLOCKS_PER_BITMAP_DWORD) {
        dwMask &= alcMasks[nBlks + idxSubBlk];
        nBlks = 0;
    } else {
        nBlks -= BLOCKS_PER_BITMAP_DWORD - idxSubBlk;
    }
    prrgn->allocMap[idxBitmap] |= dwMask;

    // take care of middle blocks of size >= 16
    while (nBlks >= BLOCKS_PER_BITMAP_DWORD) {
        prrgn->allocMap[++idxBitmap] = BLOCK_ALL_CONTINUE;
        nBlks -= BLOCKS_PER_BITMAP_DWORD;
    }

    // take care of end blocks
    if (nBlks) {
        prrgn->allocMap[++idxBitmap] |= alcValTail[nBlks];
    }
    
    // update idxBlkFree
    if (prrgn->idxBlkFree == prrgn->idxBlkCommit) {
        prrgn->idxBlkFree = 0;
    }
}

/*++

Routine Description:

    This helper routine is used to return block at
    idxBlock back to the system. Called via heap
    free functions.

    This function updates the subrgn from where
    the block is being freed.

Arguments:

    prrgn - rgn pointer.
    idxBlock - block to free.
    dwCaller - address of the caller (sentinel)

Return Value:

    Number of blocks freed. Number of blocks
    returned are a multiple of 32 bytes. If the
    given idxBlock is not allocated, then 0 is
    returned.

--*/
static DWORD FreeBlocksFx (PRHRGN prrgn, DWORD idxBlock, DWORD dwCaller)
{    
    DWORD nBlks = 0;
    BYTE idxSubFree = IDX2SUBRGNIDX(idxBlock, prrgn);
    BYTE idxBlkFree = IDX2FREELISTIDX(idxBlock, prrgn);
    PSUBRGN pSubRgn = IDX2SUBRGN(idxSubFree, prrgn);
    LPBYTE pFreeList = SUBRGNFREELIST(pSubRgn->idxFreeList, prrgn);
    
    DEBUGCHK (FreeListRgn (prrgn));
    
    if (SUBRGN_ALLOC_BLK == pFreeList[idxBlkFree]) {     
        nBlks = prrgn->dfltBlkCnt;
        
#if HEAP_SENTINELS
        RHeapCheckAllocSentinels (LOCALPTR (prrgn, idxBlock), nBlks, TRUE);
        RHeapSetFreeSentinels (LOCALPTR (prrgn, idxBlock), nBlks, dwCaller);
#endif

        // rgn book keeping
        prrgn->numBlkFree += nBlks;
        
        // if this subrgn was empty before, move this to head of subrgns.
        if (SUBRGN_END_BLK == pSubRgn->idxBlkFree) {
#ifdef HEAP_VALIDATE_SUBRGNS            
            ValidateSubRgn (prrgn, idxSubFree);
#endif
            pSubRgn->idxSubFree = prrgn->idxSubFree;
            prrgn->idxSubFree = idxSubFree;            
        }

        // free list book keeping
        pFreeList[idxBlkFree] = pSubRgn->idxBlkFree;
        pSubRgn->idxBlkFree = idxBlkFree;

        g_fFullyCompacted = FALSE;    
    }
    
    return nBlks;
}

//
// FreeBlocks - mark 'nBlks' blocks from 'idxBlock' free.
//
static void FreeBlocks (PRHRGN prrgn, DWORD idxBlock, DWORD nBlks, DWORD dwCaller)
{    
    DWORD idxBitmap = IDX2BITMAPDWORD(idxBlock);
    DWORD idxSubBlk = IDX2BLKINBITMAPDWORD(idxBlock);
    DWORD dwMask    = alcMasks[idxSubBlk];
    DWORD idx       = idxBitmap;

    DEBUGCHK (nBlks);
    DEBUGCHK (idxBlock + nBlks <= prrgn->idxBlkCommit);
    DEBUGCHK (!FreeListRgn (prrgn));

#if HEAP_SENTINELS
    RHeapSetFreeSentinels (LOCALPTR (prrgn, idxBlock), nBlks, dwCaller);
#endif

    // update # of free blocks
    prrgn->numBlkFree += nBlks;

    // assume we have all free blocks in one single free block
    prrgn->maxBlkFree = prrgn->numBlkFree;

    // take care of head blocks
    if (nBlks + idxSubBlk < BLOCKS_PER_BITMAP_DWORD) {
        DEBUGCHK (nBlks + idxSubBlk);
        dwMask |= ~alcMasks[nBlks + idxSubBlk];
        nBlks = 0;
    } else {
        nBlks -= BLOCKS_PER_BITMAP_DWORD - idxSubBlk;
    }
    prrgn->allocMap[idx] &= dwMask;

    // take care of middle blocks of size >= 16
    while (nBlks >= BLOCKS_PER_BITMAP_DWORD) {
        prrgn->allocMap[++idx] = 0;
        nBlks -= BLOCKS_PER_BITMAP_DWORD;
    }

    // take care of end blocks
    if (nBlks) {
        prrgn->allocMap[++idx] &= ~alcMasks[nBlks];
    }

    // update the free block index
    prrgn->idxBlkFree = prrgn->allocMap[idxBitmap]? idxBlock : MAKEIDX(idxBitmap, 0);
    
    // set fully-compacted to FALSE. Theoretically we only need to set it to false
    // when we freed the last allocated item in the region. However, it'll require
    // scanning through the bitmaps and would panelize all "HeapFree". We'd rather
    // have CompactAllHeaps paying the panelty.
    g_fFullyCompacted = FALSE;
}

//
// CommitBlocks - commit memory for a region to hold at least 'idxBlock' blocks
//
static BOOL CommitBlocks (PRHRGN prrgn, DWORD idxBlock)
{
    BOOL fRet = TRUE;
    if (idxBlock > prrgn->idxBlkCommit) {
        // page align idxBlock
        DWORD cbSize = PAGEALIGN_UP (BLK2SIZE (idxBlock - prrgn->idxBlkCommit));

        fRet = (BOOL) prrgn->phpOwner->pfnAlloc (
                                LOCALPTR (prrgn, prrgn->idxBlkCommit),
                                cbSize,
                                MEM_COMMIT,
                                (IsHeapRgnEmbedded (prrgn->phpOwner)) ? NULL : &prrgn->dwRgnData);
        if (fRet) {
#if HEAP_SENTINELS
            // fill 'free sentinels' in the newly committed pages
            RHeapSetFreeSentinels (LOCALPTR (prrgn, prrgn->idxBlkCommit), RHEAP_BLOCK_CNT(cbSize), 0);
#endif
            prrgn->idxBlkCommit += RHEAP_BLOCK_CNT (cbSize);
        }
    }
    return fRet;
}

/*++

Routine Description:

    This helper routine is used to return number
    of blocks free at the given index in the given
    rgn.

    This function checks if the given block is a
    free block and if it is, it returns the number
    of blocks free at the given index which is
    always same as rgn default allocation size.

Arguments:

    prrgn - rgn pointer.
    idxFree - free block index to check for.

Return Value:

    If the given idex is free, number of blocks
    returned are a multiple of 32 bytes.

--*/
static DWORD CountFreeBlocksFx (PRHRGN prrgn, DWORD idxFree)
{
    DWORD nBlksFound = 0;

    DEBUGCHK (idxFree < prrgn->idxBlkCommit);
    DEBUGCHK (FreeListRgn(prrgn));
    
    DWORD idxSubFree = IDX2SUBRGNIDX(idxFree, prrgn);
    DWORD idxBlkFree = IDX2FREELISTIDX(idxFree, prrgn);
    PSUBRGN pSubRgn = IDX2SUBRGN(idxSubFree, prrgn);    
    LPBYTE pFreeList = SUBRGNFREELIST(pSubRgn->idxFreeList, prrgn);

    if (pFreeList[idxBlkFree] != SUBRGN_ALLOC_BLK)
        nBlksFound = prrgn->dfltBlkCnt;

    return nBlksFound;
}
//
// CountFreeBlocks - Count the size, in blocks, of a free item, excluding uncommitted memory
//
static DWORD CountFreeBlocks (PRHRGN prrgn, DWORD idxFree, DWORD nBlksNeeded)
{
    DWORD nBlksFound = 0;
    DWORD idxBitmap  = IDX2BITMAPDWORD(idxFree);
    DWORD idxSubBlk  = IDX2BLKINBITMAPDWORD(idxFree);
    DWORD dwBitmap   = prrgn->allocMap[idxBitmap];

    DEBUGCHK (idxFree < prrgn->idxBlkCommit);
    DEBUGCHK (!FreeListRgn(prrgn));
    
    // test to see if we can extend to next bitmap dword
    if (!(dwBitmap & ~alcMasks[idxSubBlk])) {

        // yes, we can extend to next bitmap dword

        DWORD idxLastBitmap = IDX2BITMAPDWORD(prrgn->idxBlkCommit);

        nBlksFound = BLOCKS_PER_BITMAP_DWORD - idxSubBlk;

        while ((nBlksFound < nBlksNeeded) 
            && (++ idxBitmap < idxLastBitmap) 
            && (0 ==(dwBitmap = prrgn->allocMap[idxBitmap]))) {
            nBlksFound += BLOCKS_PER_BITMAP_DWORD;
        }

        if (idxBitmap == idxLastBitmap) {
            return nBlksFound;
        }
        idxSubBlk = 0;

    } else if (idxSubBlk) {
        // no, in this bitmap only
        dwBitmap >>= (idxSubBlk << 1);
    }

    if ((int) (nBlksNeeded -= nBlksFound) > 0) {

        if ((nBlksNeeded < BLOCKS_PER_BITMAP_DWORD)
            && !(alcMasks[nBlksNeeded] & dwBitmap)) {
            // Found consecutive free blocks.
            nBlksFound += nBlksNeeded;

        } else {
            // can't find enough free blocks, we need to find out the exact number of free blocks in this
            // bitmap, such that the search can continue.
            DEBUGCHK (dwBitmap);
            while ((idxSubBlk < BLOCKS_PER_BITMAP_DWORD) && !(RHF_BITMASK & dwBitmap)) {
                idxSubBlk ++;
                nBlksFound ++;
                dwBitmap >>= RHEAP_BITS_PER_BLOCK;
            }
        }
    }
    
    return nBlksFound;
}

/*++

Routine Description:

    This helper routine is used to return number
    of blocks allocated at the given index in the 
    given rgn.

    This function checks if the given block is an
    allocated block and if it is, it returns the #
    of blocks allocated at the given index which 
    is always same as rgn default allocation size.

Arguments:

    prrgn - rgn pointer.
    idxAlloc - allocated block index to check for.

Return Value:

    If the given idex is allocated, number of blocks
    returned are a multiple of 32 bytes.

--*/
static DWORD CountAllocBlocksFx (PRHRGN prrgn, DWORD idxAlloc)
{    
    DWORD nBlks = 0;

    DEBUGCHK (FreeListRgn (prrgn));

    if (!idxAlloc) {
        // idxAlloc == 0 is a rgn hdr allocation
        nBlks = RHEAP_BLOCK_CNT(g_dwRgnHdrSize[prrgn->dfltBlkCnt]);
        
    } else if (idxAlloc < prrgn->idxBlkCommit) {
        DWORD idxSubFree = IDX2SUBRGNIDX(idxAlloc, prrgn);
        DWORD idxBlkFree = IDX2FREELISTIDX(idxAlloc, prrgn);
        PSUBRGN pSubRgn = IDX2SUBRGN(idxSubFree, prrgn);    
        LPBYTE pFreeList = SUBRGNFREELIST(pSubRgn->idxFreeList, prrgn);

        if (SUBRGN_ALLOC_BLK == pFreeList[idxBlkFree])
            nBlks = prrgn->dfltBlkCnt;
    }

    return nBlks;
}

//
// CountAllocBlocks - count the size, in block, of an allocated item.
//
static DWORD CountAllocBlocks (PRHRGN prrgn, DWORD idxAlloc)
{    
    DWORD nBlks = 0;

    DEBUGCHK (!FreeListRgn (prrgn));
    
    if (idxAlloc < prrgn->idxBlkCommit) {
        
        DWORD idxBitmap = IDX2BITMAPDWORD(idxAlloc);
        DWORD idxSubBlk = IDX2BLKINBITMAPDWORD(idxAlloc);
        DWORD dwBitmap  = prrgn->allocMap[idxBitmap];

        // test to see if we can extend to next bitmap dword
        if ((dwBitmap & ~alcMasks[idxSubBlk]) == alcValHead[idxSubBlk]) {

            // yes, we can extend to next bitmap dword

            DWORD idxLastBitmap = IDX2BITMAPDWORD(prrgn->idxBlkCommit);

            nBlks = BLOCKS_PER_BITMAP_DWORD - idxSubBlk;

            while ((++ idxBitmap < idxLastBitmap) && (BLOCK_ALL_CONTINUE == (dwBitmap = prrgn->allocMap[idxBitmap]))) {
                nBlks += BLOCKS_PER_BITMAP_DWORD;
            }

            if (idxBitmap == idxLastBitmap) {
                return nBlks;
            }
            idxSubBlk = 0;

        } else {
            // no, we won't extend to next bitmap dword
            if (idxSubBlk) {
                dwBitmap >>= (idxSubBlk << 1);
            }

            if (RHF_STARTBLOCK != (dwBitmap & RHF_BITMASK)) {
                return 0;
            }

            nBlks = 1;
            idxSubBlk ++;
            dwBitmap >>= RHEAP_BITS_PER_BLOCK;
        }

        while ((idxSubBlk < BLOCKS_PER_BITMAP_DWORD) && ((RHF_BITMASK & dwBitmap) == RHF_CONTBLOCK)) {
            idxSubBlk ++;
            nBlks ++;
            dwBitmap >>= RHEAP_BITS_PER_BLOCK;
        }
    }
    
    return nBlks;
}

//
// SkipAllocBlocks - skip all allocated blocks, return # of allocated blocks.
//
static DWORD SkipAllocBlocks (PRHRGN prrgn, DWORD idxAlloc)
{
    DWORD nBlks = 0;

    DWORD idxBitmap = IDX2BITMAPDWORD(idxAlloc);
    DWORD idxSubBlk = IDX2BLKINBITMAPDWORD(idxAlloc);
    DWORD dwBitmap  = prrgn->allocMap[idxBitmap] & BLOCK_ALL_CONTINUE;

    DEBUGCHK (idxAlloc < prrgn->idxBlkCommit);
    DEBUGCHK (!FreeListRgn(prrgn));

    // test to see if we can extend to next bitmap dword
    if ((dwBitmap & ~alcMasks[idxSubBlk]) == (alcValHead[idxSubBlk] & BLOCK_ALL_CONTINUE)) {

        // yes, we can extend to next bitmap dword

        DWORD idxLastBitmap = IDX2BITMAPDWORD(prrgn->idxBlkCommit);

        nBlks = BLOCKS_PER_BITMAP_DWORD - idxSubBlk;

        while ((++ idxBitmap < idxLastBitmap) && AllInUse (dwBitmap = prrgn->allocMap[idxBitmap])) {
            nBlks += BLOCKS_PER_BITMAP_DWORD;
        }

        if (idxBitmap == idxLastBitmap) {
            return nBlks;
        }
        idxSubBlk = 0;

    } else if (idxSubBlk) {

        // no, we won't extend to next bitmap dword
        dwBitmap >>= (idxSubBlk << 1);
    }

    while ((idxSubBlk < BLOCKS_PER_BITMAP_DWORD) && ((RHF_CONTBLOCK & dwBitmap) == RHF_CONTBLOCK)) {
        idxSubBlk ++;
        nBlks ++;
        dwBitmap >>= RHEAP_BITS_PER_BLOCK;
    }
    
    return nBlks;
}

//
// DoFindBlockInRegion - Worker function to find free blocks in a region, starting from idxBlkFrom.
//
static DWORD DoFindBlockInRegion (PRHRGN prrgn, DWORD nBlks, DWORD idxBlock, DWORD idxLimit)
{
    DWORD maxBlkFree = 0;
    DWORD nBlksFound = 0;

    DEBUGCHK (!FreeListRgn (prrgn));

    while (idxBlock < idxLimit) {

        nBlksFound = CountFreeBlocks (prrgn, idxBlock, nBlks);

        if (nBlksFound >= nBlks) {
            // found enough committed blocks
            return idxBlock;
        }

        if (maxBlkFree < nBlksFound) {
            maxBlkFree = nBlksFound;
        }

        if (idxBlock + nBlksFound >= idxLimit) {
            // last item encountered.
            break;
        }

        idxBlock += nBlksFound;

        idxBlock += SkipAllocBlocks (prrgn, idxBlock);

        nBlksFound = 0;
    }

    DEBUGCHK (idxBlock <= prrgn->idxBlkCommit);

    if (idxBlock + nBlksFound >= prrgn->idxBlkCommit) {
        DEBUGCHK (idxBlock + nBlksFound == prrgn->idxBlkCommit);
        nBlksFound = prrgn->numBlkTotal - idxBlock;

        if (maxBlkFree < nBlksFound) {
            maxBlkFree = nBlksFound;
        }
    } else {
        DEBUGCHK (idxLimit < prrgn->idxBlkCommit);
    }

    if (prrgn->maxBlkFree < maxBlkFree) {
        prrgn->maxBlkFree = maxBlkFree;
    }

    return (nBlksFound >= nBlks)? idxBlock : (DWORD)-1;
}

/*++

Routine Description:

    This helper routine is used to find and take 'nBlks'
    from the given rgn.

Arguments:

    prrgn - rgn pointer.
    nBlks - # of blocks to take. This has to be same
    as rgn default allocation size.

Return Value:

    -1 - failed to get a block from the rgn.
    Any other value is the block index which was
    allocated.

--*/
static DWORD FindBlocksFx (PRHRGN prrgn, DWORD nBlks)
{
    DWORD idxBlkRet;
    BYTE dfltBlkShift = prrgn->dfltBlkShift;
    DWORD idxSubFree = prrgn->idxSubFree;  
    PSUBRGN pSubRgn = IDX2SUBRGN(idxSubFree, prrgn);

    DEBUGCHK (SUBRGN_END_BLK != pSubRgn->idxBlkFree);
    DEBUGCHK (nBlks == prrgn->dfltBlkCnt);  

    idxBlkRet = MAKEIDXFX(pSubRgn->idxBlkFree, idxSubFree, dfltBlkShift);

    // found enough blocks. Commit memory if necessary
    if (!CommitBlocks (prrgn, idxBlkRet + nBlks)) {
        DEBUGCHK (idxBlkRet + nBlks > prrgn->idxBlkCommit);
        idxBlkRet = (DWORD)-1;

    } else {
        // mark the blocks in-use
        DEBUGCHK (idxBlkRet + nBlks <= prrgn->idxBlkCommit);
#if HEAP_SENTINELS
        RHeapCheckFreeSentinels (LOCALPTR (prrgn, idxBlkRet), nBlks, TRUE);
#endif
        TakeBlocksFx (prrgn, nBlks);
    }

    return idxBlkRet;    
}

//
// FindBlockInRegion - try to find 'nBlks' consecutive free blocks in a region.
//
static DWORD FindBlockInRegion (PRHRGN prrgn, DWORD nBlks)
{
    DWORD idxBlkRet;
    DWORD dwOldMax = prrgn->maxBlkFree;
            
    DEBUGCHK (!FreeListRgn (prrgn));

    // reset max, will be updated automatically through the search
    prrgn->maxBlkFree = 0;

    // search for idxFree first
    idxBlkRet = DoFindBlockInRegion (prrgn, nBlks, prrgn->idxBlkFree, prrgn->idxBlkCommit);

    if (((DWORD)-1 == idxBlkRet)
        || (idxBlkRet + nBlks > prrgn->idxBlkCommit)) {
        // couldn't find a block, or it requires committing extra page
        DEBUGCHK (prrgn->numBlkFree >= (prrgn->numBlkTotal - prrgn->idxBlkCommit));

        if (prrgn->idxBlkFree)  {
            // search from start and see if we can find one
            DWORD idx = DoFindBlockInRegion (prrgn, nBlks, 0, prrgn->idxBlkFree);

            if ((DWORD)-1 != idx) {
                // found blocks that doesn't require committing
                idxBlkRet = idx;
            }
        }
    }
    
    if ((DWORD)-1 != idxBlkRet) {

        // found enough blocks. Commit memory if necessary
        if (!CommitBlocks (prrgn, idxBlkRet + nBlks)) {
            DEBUGCHK (idxBlkRet + nBlks > prrgn->idxBlkCommit);
            idxBlkRet = (DWORD)-1;

        } else {
            // mark the blocks in-use and calculate max
            DEBUGCHK (idxBlkRet + nBlks <= prrgn->idxBlkCommit);
#if HEAP_SENTINELS
            RHeapCheckFreeSentinels (LOCALPTR (prrgn, idxBlkRet), nBlks, TRUE);
#endif
            TakeBlocks (prrgn, idxBlkRet, nBlks);
            // assume we didn't allocate from the max block (i.e. max doesn't change)
            prrgn->maxBlkFree = (dwOldMax <= prrgn->numBlkFree)
                                ? dwOldMax
                                : prrgn->numBlkFree;
        }
    }

    return idxBlkRet;
}

//
// InitRegion - initialize a region of size cbMax bytes, committing cbInit
// bytes initially. If lpBase is specified, then rgn data is assumed to be
// created by caller. nHdrBlks is the initial allocation at the base of
// the region pre-committed by caller.
//
static BOOL InitRegion (PRHEAP php, PRHRGN prrgn, LPBYTE lpBase, DWORD cbInit, DWORD cbMax, DWORD nHdrBlks, DWORD dfltBlkCnt)
{
    DEBUGCHK (PAGEALIGN_UP (cbMax)  == cbMax);
    DEBUGCHK (PAGEALIGN_UP (cbInit) == cbInit);

    prrgn->phpOwner     = php;
    prrgn->cbSize       = sizeof (RHRGN);
    prrgn->pLocalBase   = lpBase;

    if (IsHeapRgnEmbedded (php)) {
        DEBUGCHK (lpBase);
        if (dfltBlkCnt) {
            prrgn->dfltBlkCnt = (BYTE)dfltBlkCnt;
            prrgn->flags = RHRGN_FLAGS_FIX_ALLOC_RGN;            
            if (dfltBlkCnt != 3) {
                prrgn->flags |= RHRGN_FLAGS_USE_FREE_LIST;    
                prrgn->dfltBlkShift = prrgn->dfltBlkCnt >> 1;
            }
        }      
        
    } else {
        DEBUGCHK (!lpBase);
        prrgn->dwRgnData  = (DWORD) prrgn;
        // reserve and if necessary commit memory for region.
        lpBase = (LPBYTE) php->pfnAlloc (NULL, cbMax, MEM_RESERVE, &prrgn->dwRgnData);
        if (lpBase && cbInit && !php->pfnAlloc (lpBase, cbInit, MEM_COMMIT, &prrgn->dwRgnData)) {
            // fail to commit
            php->pfnFree (lpBase, 0, MEM_RELEASE, prrgn->dwRgnData);
            lpBase = NULL;
        }
        prrgn->pLocalBase = lpBase;
    }

    // initialize rgn structure
    if (lpBase) {
        
        prrgn->idxBlkCommit = RHEAP_BLOCK_CNT (cbInit);
        prrgn->numBlkFree   = prrgn->numBlkTotal = RHEAP_BLOCK_CNT (cbMax);

        if (FreeListRgn (prrgn)) {

            DWORD nUnusedBlks = g_NumUnusedBlks[dfltBlkCnt];
            DWORD nTotalBlks = g_dwRgnTotalBlks[dfltBlkCnt] - nUnusedBlks;           

            // setup free list for fix alloc rgns            
            DWORD nSubRgns = g_NumSubRgns[dfltBlkCnt];
            for (DWORD i = 0; i < nSubRgns; ++i) {

                WORD idxFreeList = (WORD)(nSubRgns + (i<<(NUM_BLKS_IN_SUBRGN_SHIFT-2)));
                LPBYTE pFreeList = (LPBYTE) (&(prrgn->allocMap[idxFreeList]));
                int maxAllocBlks = (nTotalBlks > NUM_ALLOC_BLKS_IN_SUBRGN) ? NUM_ALLOC_BLKS_IN_SUBRGN : nTotalBlks;
                DEBUGCHK (nTotalBlks);

                // every block in free list points to next block
                for (int j = 0; j < maxAllocBlks; ++j) {
                    pFreeList[j] = (BYTE)(j+1);
                }
                pFreeList[maxAllocBlks - 1] = SUBRGN_END_BLK;

                // setup the subrgn structure
                PSUBRGN pSubRgn = IDX2SUBRGN(i, prrgn);
                pSubRgn->idxBlkFree = 0;
                pSubRgn->idxSubFree = (i == (nSubRgns - 1)) ? SUBRGN_END_BLK : (BYTE)(i+1); // last one will point to end mask
                pSubRgn->idxFreeList = idxFreeList;
                nTotalBlks -= NUM_ALLOC_BLKS_IN_SUBRGN;
            }

            // take away the unused blocks from free count
            prrgn->numBlkFree -= (nUnusedBlks << prrgn->dfltBlkShift);
                        
        } else {
            // maxBlkFree is used only for rgns which don't use free list
            prrgn->maxBlkFree = RHEAP_BLOCK_CNT (cbMax);
        }

#if HEAP_SENTINELS
        RHeapSetFreeSentinels (LOCALPTR(prrgn, nHdrBlks), prrgn->idxBlkCommit - nHdrBlks, 0);
#endif
    }

    return NULL != lpBase;
}

//
// RHeapGrowNewRegion - Grow a heap by creating a new region
// Note: Heap should have been created with max size == 0
//
static PRHRGN RHeapGrowNewRegion (PRHEAP php, DWORD nInitBlks, DWORD dfltBlkCnt, LPDWORD pdwIdxBlk)
{
    PRHRGN prrgn = NULL;
    LPBYTE lpBase = NULL;
    DWORD cbInit;

    if (IsHeapRgnEmbedded(php)) {
        // rgn structure to be embedded with data
        *pdwIdxBlk = RHEAP_BLOCK_CNT(g_dwRgnHdrSize[dfltBlkCnt]);
        cbInit = PAGEALIGN_UP (BLK2SIZE(nInitBlks + *pdwIdxBlk));        
        lpBase = (LPBYTE) php->pfnAlloc (NULL, CE_FIXED_HEAP_MAXSIZE, MEM_RESERVE, NULL);
        if (lpBase) {
            if (php->pfnAlloc (lpBase, cbInit, MEM_COMMIT, NULL)) {
                prrgn = (PRHRGN) (lpBase + HEAP_SENTINELS);
            } else {
                php->pfnFree (lpBase, 0, MEM_RELEASE, 0);
            }
        }

    } else {
        // rgn structure is separate from data
        *pdwIdxBlk = 0;
        cbInit = PAGEALIGN_UP (BLK2SIZE(nInitBlks));
        prrgn = (PRHRGN) RHeapAlloc (g_hProcessHeap, HEAP_ZERO_MEMORY, GROWABLE_REGION_SIZE);
    }

    DEBUGCHK (!php->cbMaximum);

    if (prrgn) {

        DWORD nHdrBlocks = *pdwIdxBlk;

        if (!InitRegion (php, prrgn, lpBase, cbInit, CE_FIXED_HEAP_MAXSIZE, nHdrBlocks, dfltBlkCnt)) {
            // InitRegion cannot fail if lpBase is not NULL
            DEBUGCHK (!lpBase && !IsHeapRgnEmbedded (php));
            VERIFY (RHeapFree (g_hProcessHeap, 0, prrgn));
            prrgn = NULL;

        } else {

            BOOL fUseFreeList = FreeListRgn(prrgn);

            if (!dfltBlkCnt) {
                // default rgn list, add new rgn to the end of the list
                php->prgnLast->prgnNext = prrgn;               
                php->prgnLast = prrgn;

            } else {
                // fix alloc rgn, add new rgn to the front of the list
                prrgn->prgnNext = php->pRgnList[dfltBlkCnt].prgnHead;
                if (!prrgn->prgnNext) {
                    php->pRgnList[dfltBlkCnt].prgnLast = prrgn;                    

                } else if (fUseFreeList) {
                    // for free list rgns, setup prev ptr
                    prrgn->prgnNext->prgnPrev= prrgn;                                
                }

                // point head to the new rgn
                php->pRgnList[dfltBlkCnt].prgnHead = prrgn;
            }

            // for embedded heap, take space for rgn structure
            if (IsHeapRgnEmbedded(php)) {         

                if (fUseFreeList) {

                    // sub rgn blks are given blks normalized to default rgn allocation size
                    int idxSubRgn = 0;
                    int nSubRgnBlks = nHdrBlocks >> prrgn->dfltBlkShift;
                    
                    // subrgn book keeping
                    while (nSubRgnBlks) {
                        PSUBRGN pSubRgn = IDX2SUBRGN(idxSubRgn, prrgn);
                        int nAllocBlks = (nSubRgnBlks > NUM_ALLOC_BLKS_IN_SUBRGN) ? NUM_ALLOC_BLKS_IN_SUBRGN : nSubRgnBlks;

                        if (nAllocBlks == NUM_ALLOC_BLKS_IN_SUBRGN) {
                            pSubRgn->idxBlkFree = (BYTE)SUBRGN_END_BLK;
                            prrgn->idxSubFree = pSubRgn->idxSubFree;
                            memset (SUBRGNFREELIST(pSubRgn->idxFreeList, prrgn), SUBRGN_ALLOC_BLK, nAllocBlks*sizeof(BYTE));

                        } else {
                            pSubRgn->idxBlkFree = (BYTE)nAllocBlks;
                            memset (SUBRGNFREELIST(pSubRgn->idxFreeList, prrgn), SUBRGN_ALLOC_BLK, nAllocBlks*sizeof(BYTE));
                            break;
                        }
                        
                        nSubRgnBlks -= NUM_ALLOC_BLKS_IN_SUBRGN;
                        idxSubRgn++;
                    }
                    
                    // rgn book keeping
                    prrgn->numBlkFree -= nHdrBlocks;
                    
                } else {
                    TakeBlocks (prrgn, 0, nHdrBlocks);
                }

#if HEAP_SENTINELS
                RHeapSetAllocSentinels (prrgn->pLocalBase, BLK2SIZE(nHdrBlocks)-HEAP_SENTINELS, TRUE);
#endif
            }

            // take the blocks for initial allocation
            if (nInitBlks) {
                if (fUseFreeList) {
                    TakeBlocksFx (prrgn, nInitBlks);

                } else {
                    TakeBlocks (prrgn, nHdrBlocks, nInitBlks);
                    prrgn->maxBlkFree = prrgn->numBlkFree;
                }
            }
        }
    }

    if (!prrgn) {
        *pdwIdxBlk = (DWORD)-1;
    }

    return prrgn;
}

//
// FindRegionByPtr - find the region where a pointer belongs to
//
static PRHRGN FindRegionByPtr (PRHEAP php, const BYTE *ptr, BOOL fIsRemotePtr)
{
    PRHRGN prrgn;
    DWORD  dwOfst;
    
    DEBUGCHK((DWORD) php->cs.OwnerThread == GetCurrentThreadId ());

    if (IsHeapRgnEmbedded(php)) {

        // rgn control structure is at the start of the rgn data
        LPBYTE lpBaseOfRegion = (LPBYTE) BLOCKALIGN_DOWN ((DWORD)ptr); // 64k aligned base address
        if (lpBaseOfRegion) {
            PRHEAP phpLocal = (PRHEAP) (lpBaseOfRegion + HEAP_SENTINELS); // header is after the sentinels

            if (phpLocal == php) {
                // this is the first region (header is heap structure)
                prrgn = (PRHRGN) &php->rgn;

            } else {
                // secondary regions (header is a region structure)
                prrgn = (PRHRGN) phpLocal;
            }

            __try {
                if ((prrgn->pLocalBase == lpBaseOfRegion) && (prrgn->phpOwner == php)) {
                    // valid item ptr
                    return prrgn;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }

    } else {

        prrgn = php->prgnfree;
        PREFAST_DEBUGCHK (prrgn);

        // search from php->prgnfree
        do {
            dwOfst = ptr - ((!fIsRemotePtr) ? prrgn->pLocalBase : prrgn->pRemoteBase);

            if (dwOfst < BLK2SIZE(prrgn->numBlkTotal)) {
                return prrgn;
            }

            prrgn = prrgn->prgnNext;
            if (!prrgn) {
                prrgn = &php->rgn;
            }
        } while (prrgn != php->prgnfree);
    }

    return NULL;
}

//
// FindVaItemByPtr - find a VirtualAlloc'd item
//
static PRHVAITEM FindVaItemByPtr (PRHEAP php, const BYTE *ptr, BOOL fIsRemotePtr)
{
    PRHVAITEM pitem = NULL;

    DEBUGCHK((DWORD) php->cs.OwnerThread == GetCurrentThreadId ());

    // VirtualAlloc'd item must be 64K aligned.
    if (!(VM_BLOCK_OFST_MASK & (DWORD) ptr)) {
        // search through the VirtualAlloc'd list
        for (pitem = php->pvaList; pitem; pitem = pitem->pNext) {
            if (ptr == ((!fIsRemotePtr) ? pitem->pLocalBase : pitem->pRemoteBase)) {
                break;
            }
        }        
    }

    return pitem;
}

//
// DoRHeapValidatePtr - validate if a pointer is a valid heap pointer
//
static PRHVAITEM DoRHeapValidatePtr (PRHEAP php, const BYTE *ptr, PBOOL pfInRegion, BOOL fIsRemotePtr)
{
    EnterCriticalSection (&php->cs);
    PRHVAITEM pitem = FindVaItemByPtr (php, ptr, fIsRemotePtr);

    if (pitem) {
        *pfInRegion = FALSE;
        
    } else {
        PRHRGN prrgn = FindRegionByPtr (php, ptr, fIsRemotePtr);
        if (prrgn) {

            DWORD idxBlock  = RHEAP_BLOCK_CNT (ptr - ((!fIsRemotePtr) ? prrgn->pLocalBase : prrgn->pRemoteBase));

            if (FreeListRgn (prrgn)) {
                DWORD idxSubRgn   = IDX2SUBRGNIDX(idxBlock, prrgn);
                DWORD idxFreeList = IDX2FREELISTIDX(idxBlock, prrgn);
                PSUBRGN pSubRgn   = IDX2SUBRGN(idxSubRgn, prrgn);                
                LPBYTE pFreeList  = SUBRGNFREELIST(pSubRgn->idxFreeList, prrgn);
                
                if (SUBRGN_ALLOC_BLK == pFreeList[idxFreeList]) {
                    *pfInRegion = TRUE;
                    pitem = (PRHVAITEM) prrgn;
                }

            } else {
                DWORD idxSubBlk = IDX2BLKINBITMAPDWORD(idxBlock);
                if (ISALLOCSTART (prrgn->allocMap[IDX2BITMAPDWORD(idxBlock)], idxSubBlk)) {
                    *pfInRegion = TRUE;
                    pitem = (PRHVAITEM) prrgn;
                }
            }
        }
    }
    LeaveCriticalSection (&php->cs);

    return pitem;
}

//
// ValidateRegion - validate if a region is valid
//
static BOOL ValidateRegion (PRHEAP php, PRHRGN prrgn)
{
    return (prrgn->phpOwner          == php)
        && (sizeof (RHRGN)           == prrgn->cbSize)
        && (prrgn->idxBlkFree        <= prrgn->idxBlkCommit)
        && (prrgn->idxBlkCommit      <= prrgn->numBlkTotal)
        && (FreeListRgn(prrgn) 
            || (prrgn->maxBlkFree    <= prrgn->numBlkFree))
        && (prrgn->numBlkFree        <= prrgn->numBlkTotal)
        && !((DWORD) prrgn->pLocalBase  & VM_BLOCK_OFST_MASK)
        && !((DWORD) prrgn->pRemoteBase & VM_BLOCK_OFST_MASK);
}

//
// DoRHeapVMAlloc - create a VirtualAlloc'd item
//
static LPBYTE DoRHeapVMAlloc (PRHEAP php, DWORD dwFlags, DWORD cbSize)
{
    PRHVAITEM pitem = (PRHVAITEM) RHeapAlloc (g_hProcessHeap, HEAP_ZERO_MEMORY, sizeof (RHVAITEM));
    LPBYTE    lpRet = NULL;

    DEBUGCHK ((int) cbSize > 0);
    
    if (pitem) {
        // initialize the item
        pitem->dwRgnData = (DWORD) pitem;   // block align the reservation
        pitem->cbReserve = (cbSize + HEAP_SENTINELS + VM_BLOCK_OFST_MASK) & ~VM_BLOCK_OFST_MASK;
        pitem->cbSize    = cbSize;
        pitem->phpOwner  = php;

#ifdef HEAP_STATISTICS
        InterlockedIncrement (&lNumAllocs[BUCKET_VIRTUAL]);
#endif

        // allocate memory
        pitem->pLocalBase = (LPBYTE) php->pfnAlloc (NULL, pitem->cbReserve, MEM_RESERVE, &pitem->dwRgnData);
        if (pitem->pLocalBase) {
            if (php->pfnAlloc (pitem->pLocalBase, cbSize + HEAP_SENTINELS, MEM_COMMIT, &pitem->dwRgnData)) {

                // set sentinels before adding to the list (without cs)
                lpRet = pitem->pLocalBase;
#if HEAP_SENTINELS
                RHeapSetAllocSentinels (lpRet, cbSize, FALSE);
                lpRet += HEAP_SENTINELS;
#endif

                // insert to heap's VirtualAlloc's item list
                EnterCriticalSection (&php->cs);
                pitem->pNext = php->pvaList;
                php->pvaList = pitem;
                LeaveCriticalSection (&php->cs);

            } else {
                // failed
                VERIFY (php->pfnFree (pitem->pLocalBase, 0, MEM_RELEASE, pitem->dwRgnData));
                VERIFY (RHeapFree (g_hProcessHeap, 0, pitem));
            }

        } else {
            // failed to reserve VM
            VERIFY (RHeapFree (g_hProcessHeap, 0, pitem));
        }
    }
    
    return lpRet;
}

//
// DoRHeapVMFree - Free a VirtualAlloc'd item
//
static LPBYTE DoRHeapVMFree (PRHEAP php, DWORD dwFlags, LPBYTE pMem)
{
    DEBUGCHK ((DWORD) php->cs.OwnerThread == GetCurrentThreadId ());

    // VirtualAlloc'd item must be 64K aligned.
    if (!(VM_BLOCK_OFST_MASK & (DWORD) pMem)) {
        PRHVAITEM *ppitem, pitem;

        for (ppitem = &php->pvaList; (NULL != (pitem = *ppitem)); ppitem = &pitem->pNext) {
            if (pMem == pitem->pLocalBase) {
                // found allocation,

                DEBUGMSG (DBGFIXHP, (L"Freeing VirtualAlloc'd item at %8.8lx\r\n", pMem));
                // remove it from the va list
                *ppitem = pitem->pNext;

#if HEAP_SENTINELS
                RHeapCheckAllocSentinels (pMem, RHEAP_BLOCK_CNT (pitem->cbSize+HEAP_SENTINELS), TRUE);
#endif
                // free VM
                VERIFY (php->pfnFree (pMem, 0, MEM_RELEASE, pitem->dwRgnData));

                // free the item itself
                VERIFY (RHeapFree (g_hProcessHeap, 0, pitem));

                // update return value
                pMem = NULL;
                break;
            }

        }
    }

    return pMem;
}

static void ReleaseHeapMemory (PRHEAP php)
{
    PRHRGN prrgn;
    PRHVAITEM pitem;            
    BOOL fHeapRgnEmbedded = IsHeapRgnEmbedded(php);

    // destroy the heap itself
    // php->cs.hCrit could be NULL if failed in HeapCreate. In which case, there 
    // will not be any allocation orther than the heap itself to be freed
    if (php->cs.hCrit) {
        EnterCriticalSection (&php->cs);

        // free all virtual-allocated items
        while (NULL != (pitem = php->pvaList)) {
            php->pvaList = pitem->pNext;
            // free VM
            VERIFY (php->pfnFree (pitem->pLocalBase, 0, MEM_RELEASE, pitem->dwRgnData));
            // free the item itself
            VERIFY (RHeapFree (g_hProcessHeap, 0, pitem));
        }

        // free all fix alloc rgns
        if (HeapSupportFixBlks (php)) {
            for (int i = 0; i < RLIST_MAX; ++i) {
                while (NULL != (prrgn = php->pRgnList[i].prgnHead)) {
                    php->pRgnList[i].prgnHead = prrgn->prgnNext;
                    VERIFY (php->pfnFree (prrgn->pLocalBase, 0, MEM_RELEASE, 0));
                }
            }
        }
        
        // free all dynamically created regions
        while (NULL != (prrgn = php->rgn.prgnNext)) {
            php->rgn.prgnNext = prrgn->prgnNext;
            // free VM
            VERIFY (php->pfnFree (prrgn->pLocalBase, 0, MEM_RELEASE, fHeapRgnEmbedded ? 0 : prrgn->dwRgnData));

            // free region itself
            if (!fHeapRgnEmbedded) {
                VERIFY (RHeapFree (g_hProcessHeap, 0, prrgn));
            }     
        }            

    // [cs] need to be deleted before base rgn gets deleted
    // for local heaps destroying base rgn will destroy heap
        LeaveCriticalSection (&php->cs);
        DeleteCriticalSection (&php->cs);
    }
    
    // decommit the main region (local base could be null when failed creating heap
    prrgn = &php->rgn;
    if (prrgn->pLocalBase) {
        VERIFY ((php->pfnFree (prrgn->pLocalBase, 0, MEM_RELEASE, fHeapRgnEmbedded ? 0 : prrgn->dwRgnData)));
    }

    // for remote and non-growable heaps, heap structure is separate from data
    if (!fHeapRgnEmbedded) {
        VERIFY (RHeapFree (g_hProcessHeap, 0, php));
    }        
}

//
// DoRHeapCreate - worker function to create a heap
//
PRHEAP DoRHeapCreate (
    DWORD            flOptions,
    DWORD            dwInitialSize,
    DWORD            dwMaximumSize,
    PFN_AllocHeapMem pfnAlloc,
    PFN_FreeHeapMem  pfnFree,
    DWORD            dwProcessId
    )
{
    PRHEAP php = NULL, phpRet = NULL;
    LPBYTE lpBase = NULL;
    DWORD  nBitmapDwords;
    DWORD  nHdrBlocks = 0;

    DEBUGCHK ((int) dwMaximumSize >= 0);

    // use default allocator and free fns if user passed in null
    if (!pfnAlloc) {
        DEBUGCHK (!(flOptions & HEAP_IS_REMOTE));

        if (HEAP_CREATE_ENABLE_EXECUTE & flOptions) {
            pfnAlloc = (flOptions & HEAP_SHARED_READONLY)? AllocMemShareExecutable : AllocMemVirtExecutable; 
        } else {
            pfnAlloc = (flOptions & HEAP_SHARED_READONLY)? AllocMemShare : AllocMemVirt; 
        }

        if (!dwMaximumSize) {
            flOptions |= HEAPRGN_IS_EMBEDDED;
            if (flOptions & HEAP_SUPPORT_FIX_BLKS)
                dwInitialSize = GROWABLE_HEAP_SIZE + HEAP_SENTINELS + RLIST_MAX * sizeof(RGNLIST);
            else 
                dwInitialSize = GROWABLE_HEAP_SIZE + HEAP_SENTINELS;
            nHdrBlocks = RHEAP_BLOCK_CNT (dwInitialSize);
        }
    } else {
        flOptions &= ~HEAP_SUPPORT_FIX_BLKS;
    }    
    
    if (!pfnFree) {
        pfnFree = FreeMemVirt;
    }

    dwInitialSize = PAGEALIGN_UP (dwInitialSize);

    if (dwMaximumSize) {
        // heap with fixed size
        dwMaximumSize = PAGEALIGN_UP (dwMaximumSize);
        dwInitialSize = PAGEALIGN_UP (dwInitialSize);
        nBitmapDwords = IDX2BITMAPDWORD (RHEAP_BLOCK_CNT (dwMaximumSize));

    } else {
        // growable heap
        nBitmapDwords = NUM_DEFAULT_DWORD_BITMAP_PER_RGN;

        if (dwInitialSize > CE_FIXED_HEAP_MAXSIZE)
            dwInitialSize = CE_FIXED_HEAP_MAXSIZE;
    }

    if (nHdrBlocks) {
        // embedded heap/regions
        DEBUGCHK (flOptions & HEAPRGN_IS_EMBEDDED);
        lpBase = (LPBYTE) pfnAlloc (NULL, CE_FIXED_HEAP_MAXSIZE, MEM_RESERVE, NULL);
        if (lpBase && pfnAlloc (lpBase, dwInitialSize, MEM_COMMIT, NULL)) {
            php = (PRHEAP) (lpBase + HEAP_SENTINELS);                
        }
    } else {
        // for non-embbeded heaps, create heap structure separate from data
        DEBUGCHK (!(flOptions & HEAPRGN_IS_EMBEDDED));
        php = (PRHEAP) RHeapAlloc (g_hProcessHeap, HEAP_ZERO_MEMORY, sizeof (RHEAP) + nBitmapDwords * sizeof (DWORD));
    }

    if (php) {
        php->cbSize      = sizeof (RHEAP);
        php->cbMaximum   = dwMaximumSize;
        php->dwSig       = HEAPSIG;
        php->pfnAlloc    = pfnAlloc;
        php->pfnFree     = pfnFree;
        php->prgnLast    = &php->rgn;        
        php->dwProcessId = dwProcessId;
        php->prgnfree    = &php->rgn;

        // add support for heap corruption checks (embedded, not shared, not remote)
        if ((flOptions & HEAPRGN_IS_EMBEDDED)
            && !(flOptions & (HEAP_SHARED_READONLY | HEAP_IS_REMOTE))) {
            flOptions |= HEAP_FREE_CHECKING_ENABLED;
        }
        
        php->flOptions   = flOptions;
        
        // set up the rgn lists
        if (flOptions & HEAP_SUPPORT_FIX_BLKS) {
            php->pRgnList = (PRGNLIST) ((LPBYTE)php + GROWABLE_HEAP_SIZE);
        }
        InitializeCriticalSection (&php->cs);

        if (php->cs.hCrit
            && InitRegion (php, &php->rgn, lpBase, dwInitialSize, dwMaximumSize? dwMaximumSize : CE_FIXED_HEAP_MAXSIZE, nHdrBlocks, 0)) {

            phpRet = php;

            // for embedded heap, take space for heap/rgn structure
            if (nHdrBlocks) {
                PRHRGN prrgn = &php->rgn;
                TakeBlocks (prrgn, 0, nHdrBlocks);
                prrgn->maxBlkFree = prrgn->numBlkFree;
#if HEAP_SENTINELS
                if (HeapSupportFixBlks (php)) {
                    RHeapSetAllocSentinels (prrgn->pLocalBase, GROWABLE_HEAP_SIZE + sizeof(RGNLIST) * RLIST_MAX, TRUE);

                } else {
                    RHeapSetAllocSentinels (prrgn->pLocalBase, GROWABLE_HEAP_SIZE, TRUE);
                }
#endif
                if (HeapSupportFixBlks (php)) {                    
                    DWORD dummy;
                    // pRgnList[0] is unused
                    EnterCriticalSection (&php->cs);
                    for (int i = 1; i < RLIST_MAX; ++i) {
                        php->pRgnList[i].prgnFree = RHeapGrowNewRegion (php, 0, i, &dummy);
                        if (!php->pRgnList[i].prgnFree) {
                            phpRet = NULL;
                            break;
                        }
                    }
                    LeaveCriticalSection (&php->cs);
                }
            }

        }
    }

    if (phpRet) {
        DEBUGCHK (phpRet == php);
        // add the heap to the heap list
        EnterCriticalSection (&g_csHeapList);
        php->phpNext = g_phpListAll;
        g_phpListAll = php;
        LeaveCriticalSection (&g_csHeapList);
        if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
            CELOG_HeapCreate(flOptions, dwInitialSize, dwMaximumSize, (HANDLE)php);
        }
        
    } else {

        if (lpBase && (!php || !php->cs.hCrit)) {
            pfnFree (lpBase, 0, MEM_RELEASE, 0);
            
        } else if (php) {
            ReleaseHeapMemory (php);
        }
    }


    return phpRet;
}

//
// EnumerateRegionItems - enumerate through all the items in a region
//
static BOOL EnumerateRegionItems (PRHRGN prrgn, PFN_HeapEnum pfnEnum, LPVOID pEnumData)
{
    DWORD maxBlkFree  = 0;
    DWORD idx1stFree  = prrgn->idxBlkCommit;
    DWORD idxLastFree = prrgn->idxBlkCommit;
    DWORD idxBlock;
    DWORD nBlks;
    BOOL fUseFreeList = FreeListRgn(prrgn);

    for (idxBlock = 0; idxBlock < prrgn->idxBlkCommit; idxBlock += nBlks) {

        nBlks = ((fUseFreeList) ? CountAllocBlocksFx(prrgn, idxBlock) : CountAllocBlocks (prrgn, idxBlock));
        if (nBlks) {
            // allocated blocks
            DEBUGCHK (idxBlock + nBlks <= prrgn->idxBlkCommit);

            idxLastFree = prrgn->idxBlkCommit;

            if (!(*pfnEnum) (LOCALPTR(prrgn, idxBlock), BLK2SIZE (nBlks), RHE_NORMAL_ALLOC, pEnumData)) {
                break;
            }

        } else {

            nBlks = ((fUseFreeList) ? CountFreeBlocksFx (prrgn, idxBlock) : CountFreeBlocks (prrgn, idxBlock, prrgn->numBlkFree));
            if (nBlks) {

                // free block
                if (idx1stFree > idxBlock) {
                    idx1stFree = idxBlock;
                }
                if (maxBlkFree < nBlks) {
                    maxBlkFree = nBlks;
                }

                idxLastFree = idxBlock;

                if (!(*pfnEnum) (LOCALPTR(prrgn, idxBlock), BLK2SIZE (nBlks), RHE_FREE, pEnumData)) {
                    break;
                }


            } else {
                // bad block
                RETAILMSG (1, (L"EnumerateRegionItems: encountered bad block (idx = 0x%x) in region 0x%8.8lx\r\n", idxBlock, prrgn));
                DEBUGCHK (0);
                break;
            }
        }
    }

    DEBUGCHK (idxBlock <= prrgn->idxBlkCommit);

    if ((idxBlock == prrgn->idxBlkCommit) && !fUseFreeList) {
        // successfully enumerated through the region

        // calculate max, take into account uncommitted pages
        if (maxBlkFree < prrgn->numBlkTotal - idxLastFree) {
            maxBlkFree = prrgn->numBlkTotal - idxLastFree;
        }

        prrgn->idxBlkFree = idx1stFree;
        prrgn->maxBlkFree = maxBlkFree;
    }

    return (idxBlock == prrgn->idxBlkCommit);
}

//
// EnumerateHeapItems - enumerate through all items of a heap.
//
BOOL EnumerateHeapItems (PRHEAP php, PFN_HeapEnum pfnEnum, LPVOID pEnumData)
{
    BOOL      fRet  = TRUE;
    PRHRGN    prrgn = &php->rgn;
    PRHVAITEM pitem = NULL;

    EnterCriticalSection (&php->cs);

    __try {

        for (; prrgn && fRet; prrgn = prrgn->prgnNext) {
            fRet = EnumerateRegionItems (prrgn, pfnEnum, pEnumData);
        }

        if (HeapSupportFixBlks (php)) {                
            for (int i = 0; (i < RLIST_MAX) && fRet; ++i) {
                for ( prrgn = php->pRgnList[i].prgnHead; prrgn && fRet; prrgn = prrgn->prgnNext) {
                    fRet = EnumerateRegionItems (prrgn, pfnEnum, pEnumData);
                }
            }
        }        
        
        // enumerate through VirtualAlloc'd items
        for (pitem = php->pvaList; pitem && fRet; pitem = pitem->pNext) {
            fRet = (*pfnEnum) (pitem->pLocalBase, pitem->cbSize + HEAP_SENTINELS, RHE_VIRTUAL_ALLOC, pEnumData);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // empty except block
    }

    LeaveCriticalSection (&php->cs);

    return fRet;
}

static BOOL EnumCountSize (LPBYTE pMem, DWORD cbSize, DWORD dwFlags, LPVOID pEnumData)
{
    LPDWORD puiMax = (LPDWORD) pEnumData;
    if ((RHE_FREE == dwFlags) && (*puiMax < cbSize)) {
        *puiMax = cbSize;
    }

#if HEAP_SENTINELS
    if (RHE_FREE == dwFlags) {
        RHeapCheckFreeSentinels (pMem, RHEAP_BLOCK_CNT (cbSize), TRUE);

    } else {
        RHeapCheckAllocSentinels (pMem, RHEAP_BLOCK_CNT (cbSize), TRUE);
    }
#endif

    // keep enumerating
    return TRUE;
}

// Move a rgn to the end of the list
static void MoveRgnToEnd (PRHRGN prrgn)
{
    PRHEAP   php = prrgn->phpOwner;
    PRGNLIST pRgnList = &php->pRgnList[prrgn->dfltBlkCnt];

    DEBUGCHK (prrgn->prgnNext);
    DEBUGCHK (FreeListRgn (prrgn));
    DEBUGCHK (HeapSupportFixBlks (php));
    
    // de-couple prrgn from current position    
    prrgn->prgnNext->prgnPrev = prrgn->prgnPrev;
    if (prrgn->prgnPrev)
        prrgn->prgnPrev->prgnNext = prrgn->prgnNext;
    else
        pRgnList->prgnHead = prrgn->prgnNext;

    // set the next/prev ptrs for prrgn
    prrgn->prgnPrev = pRgnList->prgnLast;
    prrgn->prgnPrev->prgnNext = prrgn;
    prrgn->prgnNext = NULL;

    // point the last to prrgn
    pRgnList->prgnLast = prrgn;    
}

// Move a rgn to the beginning of the list
static void MoveRgnToBegin (PRHRGN prrgn)
{
    PRHEAP   php = prrgn->phpOwner;
    PRGNLIST pRgnList = &php->pRgnList[prrgn->dfltBlkCnt];
    
    DEBUGCHK (prrgn->prgnPrev);
    DEBUGCHK (FreeListRgn (prrgn));
    DEBUGCHK (HeapSupportFixBlks (php));
    
    // de-couple prrgn from current position    
    prrgn->prgnPrev->prgnNext = prrgn->prgnNext;
    if (prrgn->prgnNext)
        prrgn->prgnNext->prgnPrev = prrgn->prgnPrev;
    else
        pRgnList->prgnLast = prrgn->prgnPrev;

    // set the next/prev ptrs for prrgn
    prrgn->prgnNext = pRgnList->prgnHead;
    prrgn->prgnNext->prgnPrev = prrgn;
    prrgn->prgnPrev = NULL;

    // point the first to prrgn
    pRgnList->prgnHead = prrgn;
        
}

//
// DoRHeapCompactRegion - compact a region
//
static UINT DoRHeapCompactRegion (PRHRGN prrgn)
{
    UINT  uiMax = 0;
    DWORD idxNewCommit;

    if (FreeListRgn (prrgn)) {
        
        DWORD idxSubRgn = g_NumSubRgns[prrgn->dfltBlkCnt];
        DWORD idxFreeBlock = 0;

        // find the first subrgn which is not completely full (scan backwards)
        while (!idxFreeBlock && idxSubRgn--) {
            PSUBRGN pSubRgn = IDX2SUBRGN(idxSubRgn, prrgn);
            LPBYTE pFreeList = SUBRGNFREELIST(pSubRgn->idxFreeList, prrgn);

            // scan free list within this subrgn to find first allocated block from end
            idxFreeBlock = NUM_ALLOC_BLKS_IN_SUBRGN;                
            while (idxFreeBlock) {
                if (SUBRGN_ALLOC_BLK == pFreeList[idxFreeBlock-1]) {
                    // found an allocated block
                    break;
                }
                --idxFreeBlock;
            }
        }        

        // calculate the size to be decommitted
        idxNewCommit = RHEAP_BLOCK_CNT (PAGEALIGN_UP (BLK2SIZE (MAKEIDXFX(idxFreeBlock, idxSubRgn, prrgn->dfltBlkShift))));

    } else {
    
        DWORD idxBitmap = IDX2BITMAPDWORD(prrgn->idxBlkCommit);

        // find out the last dword that is not completely freed
        while (idxBitmap && !prrgn->allocMap[idxBitmap-1]) {
            idxBitmap --;
        }

        // calculate the size to be decommitted
        idxNewCommit = RHEAP_BLOCK_CNT (PAGEALIGN_UP (BLK2SIZE (MAKEIDX (idxBitmap, 0))));
    }

    // decommit pages and update prrgn->idxBlkCommit if needed
    if (idxNewCommit < prrgn->idxBlkCommit) {
        VERIFY (prrgn->phpOwner->pfnFree (LOCALPTR (prrgn, idxNewCommit),
                                          BLK2SIZE (prrgn->idxBlkCommit - idxNewCommit),
                                          MEM_DECOMMIT,
                                          (IsHeapRgnEmbedded (prrgn->phpOwner)) ? 0 : prrgn->dwRgnData));
        prrgn->idxBlkCommit = idxNewCommit;
        if (prrgn->idxBlkFree >= idxNewCommit) {
            prrgn->idxBlkFree = 0;
        }
    }

    VERIFY (EnumerateRegionItems (prrgn, EnumCountSize, &uiMax));
    return uiMax;
}

//
// RHeapAlloc - allocate memory from a heap
//
extern "C"
LPBYTE WINAPI RHeapAlloc (PRHEAP php, DWORD dwFlags, DWORD cbSize)
{
    LPBYTE pMem  = NULL;
    PREFAST_DEBUGCHK (cbSize <= HEAP_MAX_ALLOC);
    DWORD  nBlks = RHEAP_BLOCK_CNT(cbSize+HEAP_SENTINELS);

    DEBUGMSG (DBGHEAP2, (L"RHeapAlloc %8.8lx %8.8lx %8.8lx\r\n", php, dwFlags, cbSize));
    
    if (!php->cbMaximum && (nBlks > RHEAP_VA_BLOCK_CNT)) {
        pMem = DoRHeapVMAlloc (php, dwFlags, cbSize);
        // memory already zero'd for VirtualAlloc'd items

    } else {

        PRHRGN prrgn;
        DWORD  idxBlock = (DWORD)-1;
        DWORD  cbActual = cbSize;
        PRHRGN prgnHead, prgnFree;
        
#ifdef HEAP_STATISTICS
        int cIters = 0;
        int idx = (cbSize >> 4);   // slot number

        if (idx > BUCKET_LARGE)      // idx BUCKET_LARGE is for items >= 1K in size
            idx = BUCKET_LARGE;

        InterlockedIncrement (&lNumAllocs[idx]);
#endif

        DEBUGCHK (nBlks);

        EnterCriticalSection (&php->cs);

        // get the appropriate rgn list to use to scan for free block
        DWORD dfltBlkCnt = ((nBlks < RLIST_MAX) && HeapSupportFixBlks(php)) ? nBlks : 0;        

        // use fix alloc rgn list or defautl rgn list
        if (dfltBlkCnt) {
            prgnFree = php->pRgnList[dfltBlkCnt].prgnFree;
            prgnHead = php->pRgnList[dfltBlkCnt].prgnHead;

        } else {
            prgnFree = php->prgnfree;
            prgnHead = &php->rgn;
        }

        // use free list implementation for 1, 2, and 4 block allocations        
        if (dfltBlkCnt && (dfltBlkCnt != 3)) {
            prrgn = prgnHead;
            if (prrgn->numBlkFree) {
                // get the blk from the first free subrgn
                idxBlock = FindBlocksFx (prrgn, nBlks);
                DEBUGCHK ((DWORD)-1 != idxBlock);

                // if rgn full and not the last one, move the rgn to the end of the list
                if (!prrgn->numBlkFree && prrgn->prgnNext) {
                    MoveRgnToEnd (prrgn);
                }
            }
            
        } else {

            // start from last known alloc/free rgn
            prrgn = prgnFree;
            PREFAST_DEBUGCHK (prrgn);
            
            do {
                if (prrgn->maxBlkFree >= nBlks) {
                    idxBlock = FindBlockInRegion (prrgn, nBlks);
                    if ((DWORD)-1 != idxBlock) {
                        // free sentinels already checked in FindBlockInRegion
                        break;
                    }
                }
#ifdef HEAP_STATISTICS
                cIters++;
#endif
                prrgn = prrgn->prgnNext;
                if (!prrgn) {
                    prrgn = prgnHead;
                }
            } while (prrgn != prgnFree);
        }
        
        if (((DWORD)-1 == idxBlock) && !php->cbMaximum) {
            prrgn = RHeapGrowNewRegion (php, nBlks, dfltBlkCnt, &idxBlock);
#ifdef HEAP_STATISTICS
            cIters++;
#endif
        }

#ifdef HEAP_STATISTICS
        InterlockedIncrement (&lTotalAllocations);
        InterlockedExchangeAdd (&lTotalIterations, cIters);
#endif

        if ((DWORD)-1 != idxBlock) {

            DEBUGCHK (prrgn);
            if (dfltBlkCnt)
                php->pRgnList[prrgn->dfltBlkCnt].prgnFree = prrgn;                        
            else
                php->prgnfree = prrgn;
            
            // return the local address
            pMem = LOCALPTR (prrgn, idxBlock);

#if HEAP_SENTINELS
            RHeapMoveSentinel (prrgn, idxBlock, cbSize);            
            RHeapSetAllocSentinels (pMem, cbSize, FALSE);
            pMem += HEAP_SENTINELS;
#else
            cbActual = BLK2SIZE (nBlks);
#endif
        }

        LeaveCriticalSection (&php->cs);

        // zero memory without holding heap CS.
        if ((HEAP_ZERO_MEMORY & dwFlags) && pMem) {
            memset (pMem, 0, cbActual);
        }
    }

    if (!pMem) {
        SetLastError (ERROR_NOT_ENOUGH_MEMORY);
    }

    if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CELOG_HeapAlloc (php, dwFlags, cbSize, (DWORD)pMem);
    }
    
    DEBUGMSG (DBGHEAP2, (L"RHeapAlloc returns %8.8lx\r\n", pMem));
    return pMem;
}

//
// Internal version which takes caller address
//
extern "C"
BOOL WINAPI RHeapFreeWithCaller (PRHEAP php, DWORD dwFlags, LPVOID ptr, DWORD dwCaller)
{
    PRHRGN prrgn = NULL;    
    DWORD  idxBlock, nBlks = 0;
    LPBYTE pMem = (LPBYTE) ptr;

#if HEAP_SENTINELS
    pMem = RHeapSentinelAdjust (pMem);
#endif

    DEBUGCHK (pMem && !((DWORD) pMem & (RHEAP_BLOCK_SIZE - 1)));
    DEBUGMSG (DBGHEAP2, (L"RHeapFree %8.8lx %8.8lx %8.8lx\r\n", php, dwFlags, ptr));

    // don't allow freeing "process heap"
    if (pMem != (LPBYTE) g_hProcessHeap) {

        EnterCriticalSection (&php->cs);

        // find the region contain this memory pointer
        prrgn = FindRegionByPtr (php, pMem, 0);

        if (prrgn)  {

            idxBlock = RHEAP_BLOCK_CNT (pMem - prrgn->pLocalBase);
            
            if (FreeListRgn(prrgn)) {
                nBlks = FreeBlocksFx (prrgn, idxBlock, dwCaller);
                
                if (nBlks) {
                    // if rgn is not the first rgn and all blocks are committed, move to the front
                    if (prrgn->prgnPrev 
                        && (prrgn->idxBlkCommit == prrgn->numBlkTotal)) {
                        MoveRgnToBegin (prrgn);
                    }
                }
                
            } else {            
                nBlks = CountAllocBlocks (prrgn, idxBlock);   

                if (FixAllocRgn(prrgn)) {
                    php->pRgnList[prrgn->dfltBlkCnt].prgnFree = prrgn;

                } else {
                    php->prgnfree = prrgn;
                }
                
                if (nBlks) {
#if HEAP_SENTINELS
                    RHeapCheckAllocSentinels (pMem, nBlks, TRUE);
#endif
                    FreeBlocks (prrgn, idxBlock, nBlks, dwCaller);
                }
            }

            if (nBlks) {
                // mark the page no-access if use-after-free check is enabled
                MarkPagesNoAccess(prrgn, pMem, BLK2SIZE(nBlks));
                pMem = NULL;
            }
            
        } else {
            // not in regions, try virtual alloced item
            pMem = DoRHeapVMFree (php, dwFlags, pMem);
        }

        LeaveCriticalSection (&php->cs);
    }

    if (pMem) {
        SetLastError (ERROR_INVALID_PARAMETER);
        if (!g_ModuleHeapInfo.nDllHeapModules) {
            // Most likley double free of a heap ptr.
            
            RETAILMSG(1, (L"RHeapFreeWithCaller: Possible double free @ 0x%08X.  Raising exception ...\r\n", pMem));

            // Notify DDx of potential heap corruption 
            __try {
                DWORD exceptInfo[4] = {HEAP_ITEM_DOUBLE_FREE, (DWORD)pMem, (DWORD)pMem, 0};
                xxx_RaiseException((DWORD)STATUS_HEAP_CORRUPTION, 0, _countof(exceptInfo), exceptInfo);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
            }
        }

    }
    
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CELOG_HeapFree (php, dwFlags, (DWORD)ptr);
    }
    
    DEBUGMSG (DBGHEAP2, (L"RHeapFree returns %8.8lx\r\n", !pMem));
    return !pMem;
}

//
// RHeapFree - free memory from a heap
//
extern "C"
BOOL WINAPI RHeapFree (PRHEAP php, DWORD dwFlags, LPVOID ptr)
{
    return RHeapFreeWithCaller (php, dwFlags, ptr, 0);
}

//
// RHeapSize - get the size of a heap item
//
extern "C"
DWORD WINAPI RHeapSize (PRHEAP php, DWORD dwFlags, LPCVOID ptr)
{
    DWORD       dwRet = (DWORD)-1;
    PRHRGN      prrgn;
    PRHVAITEM   pitem;
    LPBYTE      pMem = (LPBYTE) ptr;

#if HEAP_SENTINELS
    pMem = RHeapSentinelAdjust (pMem);
#endif

    DEBUGCHK (pMem && !((DWORD) pMem & (RHEAP_BLOCK_SIZE - 1)));
    DEBUGMSG (DBGHEAP2, (L"RHeapSize %8.8lx %8.8lx %8.8lx\r\n", php, dwFlags, ptr));

    EnterCriticalSection (&php->cs);

    // find the region contain this memory pointer
    prrgn = FindRegionByPtr (php, pMem, 0);

    if (prrgn) {
        DWORD  nBlks = 0;
        DWORD  idxBlock;
        
        // in region, count # of allocated blocks
        idxBlock = RHEAP_BLOCK_CNT (pMem - prrgn->pLocalBase);

        nBlks = (FreeListRgn(prrgn) ? CountAllocBlocksFx (prrgn, idxBlock) : CountAllocBlocks (prrgn, idxBlock));
        if (nBlks) {
            dwRet = BLK2SIZE (nBlks);
#if HEAP_SENTINELS
            RHeapCheckAllocSentinels (pMem, nBlks, TRUE);
            dwRet = ((PRHEAP_SENTINEL_HEADER) pMem)->cbSize;
#endif
        }

    } else if (NULL != (pitem = FindVaItemByPtr (php, pMem, 0))) {
        // virtual alloced item
        dwRet = pitem->cbSize;
#if HEAP_SENTINELS
        RHeapCheckAllocSentinels (pMem, RHEAP_BLOCK_CNT (dwRet + HEAP_SENTINELS), TRUE);
#endif
    }

    LeaveCriticalSection (&php->cs);

    if ((DWORD)-1 == dwRet) {
        SetLastError (ERROR_INVALID_PARAMETER);
        if (!g_ModuleHeapInfo.nDllHeapModules) {
            // debug check only if we are not using dll heap
            // since this condition is possible when using dll heap
            DEBUGCHK (0);
        }
    }

    DEBUGMSG (DBGHEAP2, (L"RHeapFree returns %8.8lx\r\n", dwRet));
    return dwRet;
}

//
// RHeapReAlloc - re-allocate a heap memory to a different size
//
extern "C"
LPBYTE WINAPI RHeapReAlloc (PRHEAP php, DWORD dwFlags, LPVOID ptr, DWORD cbSize)
{
    PRHRGN prrgn;
    PRHVAITEM pitem;
    LPBYTE pMem        = (LPBYTE) ptr;
    LPBYTE pMemRet     = NULL;
    DWORD  dwErr       = ERROR_INVALID_PARAMETER;
    DWORD  cbOldSize   = 0;
    DWORD  cbActual    = cbSize;

#if HEAP_SENTINELS
    pMem = RHeapSentinelAdjust (pMem);
#endif

    DEBUGMSG (DBGHEAP2, (L"RHeapReAlloc %8.8lx %8.8lx %8.8lx %8.8lx\r\n", php, dwFlags, ptr, cbSize));

    DEBUGCHK ((int) cbSize > 0);
    DEBUGCHK (pMem && !((DWORD) pMem & (RHEAP_BLOCK_SIZE - 1)));

    EnterCriticalSection (&php->cs);

    // find the region contain this memory pointer
    prrgn = FindRegionByPtr (php, pMem, 0);

    if (prrgn) {

        // found in region, count allocation needed
        DWORD idxBlock = RHEAP_BLOCK_CNT (pMem - prrgn->pLocalBase);
        DWORD nBlks  = FreeListRgn(prrgn) ? CountAllocBlocksFx (prrgn, idxBlock) : CountAllocBlocks (prrgn, idxBlock);

        if (nBlks) {
            int   nBlksNeeded = RHEAP_BLOCK_CNT(cbSize+HEAP_SENTINELS) - nBlks;

            pMemRet = pMem;
            idxBlock += nBlks;    // idxBlock = index to next block after the allocation

#if HEAP_SENTINELS
            cbOldSize = RHeapCheckAllocSentinels (pMem, nBlks, TRUE);
#else
            cbOldSize = BLK2SIZE (nBlks);
#endif
            if (nBlksNeeded > 0) {
                // grow

                if (FixAllocRgn(prrgn)) {
                    // fix allocation size rgn; can't grow in place
                    pMem = NULL;

                } else {

                    nBlks = (idxBlock < prrgn->idxBlkCommit)? CountFreeBlocks (prrgn, idxBlock, nBlksNeeded) : 0;
                    if (idxBlock + nBlks >= prrgn->idxBlkCommit) {
                        nBlks += prrgn->numBlkTotal - prrgn->idxBlkCommit;
                    }

                    if (nBlks < (DWORD) nBlksNeeded) {
                        // can't grow in-place
                        pMem = NULL;

                    } else if (CommitBlocks (prrgn, idxBlock+nBlksNeeded)) {
                        // can grow in place
                        TakeBlocks (prrgn, idxBlock, nBlksNeeded);
                        // turn the "head block" TakeBlocks put in into a "continuous block"
                        prrgn->allocMap[IDX2BITMAPDWORD(idxBlock)] &= ~ BITMAPBITS (IDX2BLKINBITMAPDWORD(idxBlock), RHF_HEADBIT);
                        if (prrgn->maxBlkFree > prrgn->numBlkFree) {
                            prrgn->maxBlkFree = prrgn->numBlkFree;
                        }
#if !HEAP_SENTINELS
                        // block align new size if sentinels not set
                        cbActual = cbOldSize + BLK2SIZE (nBlksNeeded);
#endif
                    } else {
                        // set pMem to NULL to indicate in-place re-allocation failed.
                        pMemRet = NULL;
                        dwErr   = ERROR_NOT_ENOUGH_MEMORY;
                    }
                }

            } else if (nBlksNeeded < 0) {
                // shrink

                if (FixAllocRgn(prrgn)) {
                    // fix allocation size rgn; can't shrink
                    cbSize = cbOldSize;

                } else {
                    FreeBlocks (prrgn, idxBlock+nBlksNeeded, -nBlksNeeded, 0);
#if HEAP_SENTINELS
                    RHeapMoveSentinel (prrgn, idxBlock-nBlks, cbSize);
#endif
                }
            }
        }
    } else if (NULL != (pitem = FindVaItemByPtr (php, pMem, 0))) {

        // in VirtualAlloc'd list
        DWORD cbNeeded    = PAGEALIGN_UP (cbSize+HEAP_SENTINELS);
        DWORD cbCommitted = PAGEALIGN_UP (pitem->cbSize+HEAP_SENTINELS);

#if HEAP_SENTINELS
        RHeapCheckAllocSentinels (pMem, RHEAP_BLOCK_CNT (pitem->cbSize+HEAP_SENTINELS), TRUE);
#endif
        pMemRet   = pMem;
        cbOldSize = pitem->cbSize;

        if (cbNeeded < cbCommitted) {
            // shrink
            VERIFY (php->pfnFree (pitem->pLocalBase+cbNeeded, cbCommitted - cbNeeded, MEM_DECOMMIT, pitem->dwRgnData));

        } else if (cbNeeded > cbCommitted) {
            // grow
            DEBUGCHK (cbSize > pitem->cbSize);

            if (cbNeeded > pitem->cbReserve) {
                // can't do in-place allocation, try out of place
                pMem       = NULL;

            } else if (!php->pfnAlloc (pitem->pLocalBase+cbCommitted, cbNeeded - cbCommitted, MEM_COMMIT, &pitem->dwRgnData)) {
                // Out of memory
                pMemRet = pMem = NULL;
                dwErr   = ERROR_NOT_ENOUGH_MEMORY;
            }
        }

        if (pMem) {
            pitem->cbSize = cbSize;
        }
    }

#if HEAP_SENTINELS

    // update sentinels only if we're doing in-place re-allocation.
    if (pMemRet && pMem) {
        DEBUGCHK (pMemRet == pMem);
        RHeapSetAllocSentinels (pMemRet, cbSize, FALSE);
    }

#endif

    LeaveCriticalSection (&php->cs);

    if (pMemRet) {

#if HEAP_SENTINELS
        pMemRet += HEAP_SENTINELS;
#endif
        DEBUGCHK (cbOldSize);

        if (pMem) {

            // re-allocation in-place, zero memory if needed
            DEBUGCHK (pMem + HEAP_SENTINELS == pMemRet);

            if ((HEAP_ZERO_MEMORY & dwFlags) && (cbActual > cbOldSize)) {
                memset (pMemRet+cbOldSize, 0, cbActual - cbOldSize);
            }

        } else {
            // cannot grow in-place, try out of place if possible

            pMem  = pMemRet;                    // restore pMem, was saved in pMemRet
            dwErr = ERROR_NOT_ENOUGH_MEMORY;    // any failure here is due to OOM

            if (HEAP_REALLOC_IN_PLACE_ONLY & dwFlags) {
                pMemRet = NULL;
            } else if (NULL != (pMemRet = RHeapAlloc (php, dwFlags, cbSize))) {
                memcpy (pMemRet, pMem, cbOldSize);
                VERIFY (RHeapFree (php, 0, pMem));
            }
        }
    }

    if (!pMemRet) {
        SetLastError (dwErr);
    }
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CELOG_HeapRealloc (php, dwFlags, cbSize, (DWORD)ptr, (DWORD)pMemRet);
    }

    DEBUGMSG (DBGHEAP2, (L"RHeapReAlloc returning %8.8lx\r\n", pMemRet));
    return pMemRet;
}

//
// RHeapCreate - create a heap
//
extern "C"
PRHEAP WINAPI RHeapCreate (DWORD flOptions, DWORD dwInitialSize, DWORD dwMaximumSize)
{
    PRHEAP php;

    DEBUGCHK ((int) dwMaximumSize >= 0);

    DEBUGMSG(DBGFIXHP, (L"HeapCreate %8.8lx %8.8lx %8.8lx\r\n", flOptions, dwInitialSize, dwMaximumSize));

    php = DoRHeapCreate (flOptions, dwInitialSize, dwMaximumSize, NULL, NULL, GetCurrentProcessId ());

    DEBUGMSG(DBGFIXHP, (L"HeapCreate returns %8.8lx\r\n", php));
    return php;
}

//
// RHeapDestroy - destroy a heap
//
extern "C"
BOOL WINAPI RHeapDestroy (PRHEAP php)
{
    DWORD  dwErr = 0;
    PRHEAP *pphp;

    DEBUGMSG(DBGFIXHP, (L"RHeapDestroy %8.8lx\r\n", php));

    DEBUGCHK (php && (php != g_hProcessHeap));

    if (php == g_hProcessHeap) {
        dwErr = ERROR_INVALID_PARAMETER;

    } else {

        // remove heap from heap list, fail if not found
        EnterCriticalSection (&g_csHeapList);
        for (pphp = &g_phpListAll; *pphp; pphp = &(*pphp)->phpNext) {
            if (*pphp == php) {
                break;
            }
        }

        if (*pphp) {
            // found, remove the heap from the heap list
            DEBUGCHK (php == *pphp);
            *pphp = php->phpNext;

        } else {
            // not found
            dwErr = ERROR_INVALID_PARAMETER;
        }
        LeaveCriticalSection (&g_csHeapList);

        if (!dwErr) {
            ReleaseHeapMemory (php);
        }
    }

    if (dwErr) {
        SetLastError (dwErr);
    }

    if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CELOG_HeapDestroy (php);
    }

    DEBUGMSG(DBGFIXHP, (L"RHeapDestroy exit, dwErr = 0x%x.\r\n", dwErr));
    return !dwErr;
}

//
// Compact all rgns within a rgn list starting at prgnHead in heap php
//
static UINT DoRHeapCompactRgnList (PRHEAP php, PRHRGN prgnHead, PRHRGN* pprgnLast)
{
    UINT   uiMax = 0;
    DWORD dwDfltBlks = 0;
    PRHRGN prrgnPrev = prgnHead;
    PRHRGN prrgn = prgnHead->prgnNext;
    BOOL fHeapRgnEmbedded = IsHeapRgnEmbedded(php);
    
    if (fHeapRgnEmbedded) {
        // every embedded region (after the first one) has
        // certain # of blks reserved (discount these)    
        DWORD dfltBlkCnt = prgnHead->dfltBlkCnt;
        dwDfltBlks = RHEAP_BLOCK_CNT(g_dwRgnHdrSize[dfltBlkCnt]);

        if (FreeListRgn (prgnHead)) {
            // discount unused blocks for fix alloc rgns which use free list implementation
            dwDfltBlks += (((NUM_RSVD_BLKS_IN_SUBRGN * (g_NumSubRgns[dfltBlkCnt] - 1)) + 1) << prgnHead->dfltBlkShift);
        }
    }
    
    // take care of base region
    uiMax = DoRHeapCompactRegion (prgnHead);

    // compact the rest of the regions
    while (prrgn) {

        if (prrgn->numBlkTotal == prrgn->numBlkFree+dwDfltBlks) {

            // completely free region, destroy it
            prrgnPrev->prgnNext = prrgn->prgnNext;

            // prev ptr is valid only for fix alloc rgns
            if (prrgn->prgnNext && FreeListRgn(prrgn)) {
                prrgn->prgnNext->prgnPrev = prrgnPrev;
            }
            
            VERIFY (php->pfnFree (prrgn->pLocalBase, 0, MEM_RELEASE, fHeapRgnEmbedded ? 0 : prrgn->dwRgnData));

            // if rgn structure is separate from data, free the rgn structure
            if (!fHeapRgnEmbedded) {
                VERIFY (RHeapFree (g_hProcessHeap, 0, prrgn));
            }

        } else {
            UINT uiCurRgn = DoRHeapCompactRegion (prrgn);
            if (uiMax < uiCurRgn) {
                uiMax = uiCurRgn;
            }

            // move on to the next region
            prrgnPrev = prrgn;
        }
        
        prrgn = prrgnPrev->prgnNext;
    }

    *pprgnLast = prrgnPrev;
    return uiMax;
}

//
// RHeapCompact - compact a heap
//
extern "C"
UINT WINAPI RHeapCompact (PRHEAP php, DWORD dwFlags)
{
    UINT   uiMax = 0;
    PRHRGN prgnHead;
    
    DEBUGMSG(DBGFIXHP, (L"RHeapCompact %8.8lx %8.8lx\r\n", php, dwFlags));

    EnterCriticalSection (&php->cs);

    // compact base rgn list
    uiMax = DoRHeapCompactRgnList (php, &php->rgn, &php->prgnLast);
    php->prgnfree = &php->rgn;

    // compact fix alloc rgn list
    if (HeapSupportFixBlks (php)) {
        for (int i = 0; i < RLIST_MAX; ++i) {        
            prgnHead = php->pRgnList[i].prgnHead;
            if (prgnHead) {
                UINT uiCurMax = DoRHeapCompactRgnList (php, prgnHead, &php->pRgnList[i].prgnLast);
                if (uiMax < uiCurMax) {
                    uiMax = uiCurMax;
                }            
                php->pRgnList[i].prgnFree = prgnHead;
            }
        }    
    }                            
    
    LeaveCriticalSection (&php->cs);

    if (!uiMax) {
        // rare, but possible where there is no committed free space.
        SetLastError (NO_ERROR);
    }

    DEBUGMSG(DBGFIXHP, (L"RHeapCompact returns %8.8lx\r\n", uiMax));
    
    return uiMax;
}

//
// RHeapValidate - validate a heap
//
extern "C"
BOOL WINAPI RHeapValidate (PRHEAP php, DWORD dwFlags, LPCVOID ptr)
{
    BOOL fRet = FALSE;
    const BYTE * pMem = (const BYTE *) ptr;
    
    DEBUGMSG (DBGHEAP2, (L"RHeapValidate %8.8lx %8.8lx %8.8lx\r\n", php, dwFlags, ptr));
    
    if (pMem) {
        // validate a single pointer
        BOOL dummy;
#if HEAP_SENTINELS
        pMem = RHeapSentinelAdjust ((LPBYTE) pMem);
#endif
        fRet = (DoRHeapValidatePtr (php, pMem, &dummy, 0) != NULL);

    } else {
        // validate the whole heap.
        fRet = (HEAPSIG == php->dwSig)                  // signature valid
            && (sizeof (RHEAP) == php->cbSize)          // size valid
            && !(php->cbMaximum & VM_PAGE_OFST_MASK)    // max valid
            && ((php->dwProcessId & 3) == 2)           // process id valid
            && !((DWORD) php->prgnLast & 3)             // region ptr valid            
            && !((DWORD) php->pvaList & 3);             // virtual allocate item valid
            

        if (fRet) {
            PRHRGN prrgn;

            // validate regions
            EnterCriticalSection (&php->cs);

            __try {
                for (prrgn = &php->rgn; prrgn && fRet; prrgn = prrgn->prgnNext) {
                    fRet = ValidateRegion (php, prrgn)
                            && (prrgn->prgnNext || (php->prgnLast == prrgn));
                }         

                if (HeapSupportFixBlks (php)) {
                    for (int i = 0; i < RLIST_MAX && fRet; ++i) {
                        for (prrgn = php->pRgnList[i].prgnHead; prrgn && fRet; prrgn = prrgn->prgnNext) {
                            DWORD nSubRgns = g_NumSubRgns[prrgn->dfltBlkCnt];
                            fRet = ValidateRegion (php, prrgn);
                            for (DWORD j = 0; j < nSubRgns && fRet; ++j) {
                                fRet = ValidateSubRgn (prrgn, j);
                            }
                            
                        }                    
                    }
                }

#if HEAP_SENTINELS
                if (fRet) {
                    fRet = RHeapValidateSentinels (php);
                }
#endif
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                fRet = FALSE;
            }

            LeaveCriticalSection (&php->cs);
        }
    }
    
    DEBUGMSG (DBGHEAP2, (L"RHeapValidate returns %8.8lx\r\n", fRet));
    return fRet;
}

#ifdef DEBUG
BOOL EnumDumpItem (LPBYTE pMem, DWORD cbSize, DWORD dwFlags, LPVOID pEnumData)
{
    const LPCWSTR pszMemType[] = {
        L"Free",
        L"Regular Allocation",
        L"Virtual Allocation",
    };

    PREFAST_DEBUGCHK (dwFlags < 3);

    if (RHE_FREE == dwFlags) {
        LPDWORD pcbMax = (LPDWORD) pEnumData;
        if (*pcbMax < cbSize) {
            *pcbMax = cbSize;
        }
    }

    NKDbgPrintfW (L"          0x%8.8lx (%d) - %s\r\n", pMem, cbSize, pszMemType[dwFlags]);
    return TRUE;
}

static void DumpRgn (PRHRGN prrgn)
{
    DWORD cbMax = 0;
    
    NKDbgPrintfW (L"    -- Region, At 0x%8.8lx --\r\n", prrgn);
    NKDbgPrintfW (L"       Base Of Region:              0x%8.8lx\r\n", prrgn->pLocalBase);
    NKDbgPrintfW (L"       Total Free Memory:           %d\r\n", BLK2SIZE (prrgn->numBlkFree));
    NKDbgPrintfW (L"       Total Memory Committed:      %d\r\n", BLK2SIZE (prrgn->idxBlkCommit));
    NKDbgPrintfW (L"       Total Memory Reserved:       %d\r\n", BLK2SIZE (prrgn->numBlkTotal));

    if (prrgn->pRemoteBase) {
        NKDbgPrintfW (L"       ** Remote Heap information **\r\n");
        NKDbgPrintfW (L"          Remote Base:              0x%8.8lx\r\n", prrgn->pRemoteBase);
        NKDbgPrintfW (L"          Handle To Mapfile:        0x%8.8lx\r\n", prrgn->hMapfile);
    }
    NKDbgPrintfW (L"          -- Region Items\r\n");

    cbMax = 0;
    EnumerateRegionItems (prrgn, EnumDumpItem, &cbMax);
    if (!FreeListRgn (prrgn)) {
        NKDbgPrintfW (L"       -- Max Committed Consecutive Free Memory: %d (estimate = %d)\r\n", cbMax, BLK2SIZE (prrgn->maxBlkFree));    
    }
}

//
// RHeapDump - dump the heap
//
extern "C"
void RHeapDump (PRHEAP php)
{
    PRHRGN prrgn;
    PRHVAITEM pitem;
    
    NKDbgPrintfW (L"====== Dumping Heap At 0x%8.8lx ======\r\n", php);
    NKDbgPrintfW (L"       ProcessId:    0x%8.8lx\r\n",                     php->dwProcessId);
    NKDbgPrintfW (L"       Options:      0x%8.8lx\r\n",                     php->flOptions);
    NKDbgPrintfW (L"       Maximum Size: 0x%8.8lx (0 means growable)\r\n",  php->cbMaximum);

    for (prrgn = &php->rgn; prrgn; prrgn = prrgn->prgnNext) {
        DumpRgn (prrgn);
    }

    if (HeapSupportFixBlks(php)) {
        for (int i = 0; i < RLIST_MAX; ++i) {
            for (prrgn = php->pRgnList[i].prgnHead; prrgn; prrgn = prrgn->prgnNext) {
                DumpRgn (prrgn);
            }
        }
    }    
    
    for (pitem = php->pvaList; pitem; pitem = pitem->pNext) {
        NKDbgPrintfW (L"          0x%8.8lx (%d) - Virtual Allocated item\r\n", pitem->pLocalBase, pitem->cbSize+HEAP_SENTINELS);
        if (pitem->pRemoteBase) {
            NKDbgPrintfW (L"           ** Remote Heap information **\r\n");
            NKDbgPrintfW (L"              Remote Base:              0x%8.8lx\r\n", pitem->pRemoteBase);
            NKDbgPrintfW (L"              Handle To Mapfile:        0x%8.8lx\r\n", pitem->hMapfile);
        }
    }
}

#endif

//
// CompactAllHeaps - compact all heaps of a process.
//
void CompactAllHeaps(void)
{
    PRHEAP php;

    DEBUGMSG(1, (L"CompactAllHeaps: starting\r\n"));
    
    if (!g_fFullyCompacted) {
        g_fFullyCompacted = TRUE;
        EnterCriticalSection(&g_csHeapList);
        for (php = g_phpListAll ; php ; php = php->phpNext)
            RHeapCompact (php, 0);
        LeaveCriticalSection(&g_csHeapList);
    }
    
    DEBUGMSG(1, (L"CompactAllHeaps: done\r\n"));
}

#define RHEAP_VALID_REMOTE_FLAGS       HEAP_CLIENT_READWRITE

//
// CeRemoteHeapCreate - create a "remote heap"
//
extern "C"
HANDLE CeRemoteHeapCreate (DWORD dwProcessId, DWORD dwFlags)
{
    PRHEAP php = NULL;

    DEBUGMSG(DBGFIXHP, (L"CeRemoteHeapCreate %8.8lx %8.8lx\r\n", dwProcessId, dwFlags));

    // dwFlags is reserved, must be 0
    if (dwFlags & ~RHEAP_VALID_REMOTE_FLAGS) {
        SetLastError (ERROR_INVALID_PARAMETER);

    } else {

        if (GetCurrentProcessId () == dwProcessId) {
            php = DoRHeapCreate (dwFlags | HEAP_IS_REMOTE, 0, 0, AllocMemVirt, FreeMemVirt, dwProcessId);
        } else {
            php = DoRHeapCreate (dwFlags | HEAP_IS_REMOTE, 0, 0, AllocMemMappedFile, FreeMemMappedFile, dwProcessId);
        }
    }

    DEBUGMSG(DBGFIXHP, (L"CeRemoteHeapCreate returns %8.8lx\r\n", php));
    return php;
}

//
// CeRemoteHeapTranslatePointer - translate a local pointer to a remote heap item to a remote pointer
// or vice-versa (if dwFlags == CE_HEAP_REVERSE_TRANSLATE)
//
extern "C"
LPVOID CeRemoteHeapTranslatePointer (HANDLE hHeap, DWORD dwFlags, LPCVOID pMem)
{
    PRHEAP php = (PRHEAP) hHeap;

    BOOL fIsRemotePtr = (dwFlags & CE_HEAP_REVERSE_TRANSLATE);
    PRHVAITEM   pitem;
    BYTE        *ptr;
    BOOL        fInRegion;
    DEBUGMSG(DBGFIXHP, (L"CeRemoteHeapTranslatePointer %8.8lx %8.8lx\r\n", php, pMem));

    if ((dwFlags & ~CE_HEAP_REVERSE_TRANSLATE)
        || !(php->flOptions & HEAP_IS_REMOTE)) {
        SetLastError(ERROR_INVALID_PARAMETER);

        return NULL;
    }

#if HEAP_SENTINELS
        pMem = RHeapSentinelAdjust ((LPBYTE) pMem);
#endif
        ptr = (BYTE *) pMem;

        pitem = DoRHeapValidatePtr (php, ptr, &fInRegion, fIsRemotePtr);

        if (pitem) {

            ptr += HEAP_SENTINELS;

            // if pitem->pRemoteBase is NULL, it's a remote heap created with dwProcId == current process id.
            // in which case, no translation is needed.
            if (pitem->pRemoteBase) {
                if (!fIsRemotePtr) {
                    ptr += pitem->pRemoteBase - pitem->pLocalBase;
                } else {
                    ptr += pitem->pLocalBase - pitem->pRemoteBase;
                }
            }
        } else {
            ptr = NULL;
        }
    
    DEBUGMSG(DBGFIXHP, (L"CeRemoteHeapTranslatePointer returns %8.8lx\r\n", ptr));
    return ptr;
}

//
// RHeapInit - initialization function for heap manager
//
extern "C"
BOOL WINAPI LMemInit(void)
{
    PRHEAP php;
    BOOL fInKernel = IsKModeAddr ((DWORD)LMemInit);
    
    InitializeCriticalSection(&g_csHeapList);

#if HEAP_SENTINELS
#ifdef x86
    if (fInKernel) {
        // get the k.coredll.dll limits
        g_dwCoreCodeBase = (DWORD) ((PMODULE)hInstCoreDll)->BasePtr;
        g_dwCoreCodeSize = ((PMODULE)hInstCoreDll)->e32.e32_vsize;

    } else {
        // get coredll.dll limits
        e32_lite e32;
        if (ReadPEHeader (hActiveProc, RP_READ_PE_BY_MODULE, (DWORD) hInstCoreDll, &e32, sizeof (e32))) {
            g_dwCoreCodeBase = e32.e32_vbase;
            g_dwCoreCodeSize = e32.e32_vsize;
        }
    }
#endif // x86
#endif // HEAP_SENTINELS

    // enable fix blk support in kernel default process heap
    php = DoRHeapCreate (fInKernel ? HEAP_IS_PROC_HEAP | HEAP_SUPPORT_FIX_BLKS : HEAP_IS_PROC_HEAP, 0, 0, NULL, NULL, GetCurrentProcessId());
    if (php) {
        g_hProcessHeap = php;
        DEBUGMSG(DBGFIXHP, (L"   LMemInit: Done   g_hProcessHeap = %08x\r\n", g_hProcessHeap));
    }
    
    // no error handling - process won't start if process heap can't be created and memory will be
    // freed automatically.

#ifdef HEAP_STATISTICS
    DEBUGMSG (DBGHEAP2, (L"Heap Statistics at 0x%08X\r\n", lNumAllocs));
#endif

    return NULL != g_hProcessHeap;
}

#ifdef HEAP_STATISTICS
int DumpHeapStatistic (void)
{
    int idx;
    // dump heap statistics
    NKDbgPrintfW (L"****** Heap Statistic ******\r\n");
    for (idx = 0; idx < BUCKET_LARGE; idx ++) {
        if (lNumAllocs[idx]) {
            NKDbgPrintfW (L"Number of Allocations of size between %4d and %4d: %d\r\n", idx << 4, ((idx+1) << 4) - 1, lNumAllocs[idx]);
        }
    }
    if (lNumAllocs[BUCKET_LARGE]) {
        NKDbgPrintfW (L"Number of Allocations of size between 1K and    16K: %d\r\n", lNumAllocs[BUCKET_LARGE]);
    }
    if (lNumAllocs[BUCKET_VIRTUAL]) {
        NKDbgPrintfW (L"Number of Allocations of size greater than      16K: %d\r\n", lNumAllocs[BUCKET_VIRTUAL]);
    }

    NKDbgPrintfW (L"Number of Allocations made: %d\r\n", lTotalAllocations);
    NKDbgPrintfW (L"Total number of iterations for the allocations: %d\r\n", lTotalIterations);
    return 0;
}
#endif

extern "C"
void LMemDeInit (void)
{
    PRHEAP php;
    HANDLE hProc;

    PRHRGN prrgn;
    PRHVAITEM pitem;
    
    // NOTE: we're single-threaded here, no need to acquire CS.
    //       We only need to clean-up Remote Heap, where views will be mapped in client process.
    for (php = g_phpListAll ; php ; php = php->phpNext) {
        if ((AllocMemMappedFile == php->pfnAlloc)
            && (NULL != (hProc = OpenProcess (PROCESS_ALL_ACCESS, FALSE, php->dwProcessId)))) {

            for (prrgn = &php->rgn; prrgn; prrgn = prrgn->prgnNext) {
                if (prrgn->pRemoteBase) {
                    UnmapViewInProc (hProc, prrgn->pRemoteBase);
                }
            }

            for (pitem = php->pvaList; pitem; pitem = pitem->pNext) {
                if (pitem->pRemoteBase) {
                    UnmapViewInProc (hProc, pitem->pRemoteBase);
                }
            }

            VERIFY (CloseHandle (hProc));
        }
    }
#ifdef HEAP_STATISTICS
    DEBUGMSG (DBGFIXHP, (L"", DumpHeapStatistic ()));
#endif
}

//
// GetModuleHeapInfo - returns module heap info structure used by
// per-dll-heap implementation. Since this is global data within
// coredll, all the information in this structure is applicable
// only during the duration of the call to DoImports in loader.
//
extern "C"
LPVOID WINAPI CeGetModuleHeapInfo(void)
{
    DWORD dwCurrCount = g_ModuleHeapInfo.nDllHeapModules;

    // clear the structure
    memset (&g_ModuleHeapInfo, 0, sizeof(g_ModuleHeapInfo));

    // data to be passed to the dll heap implementation
    g_ModuleHeapInfo.dwHeapSentinels      = HEAP_SENTINELS; 
    g_ModuleHeapInfo.nDllHeapModules      = dwCurrCount;
    g_ModuleHeapInfo.hProcessHeap         = (HANDLE) g_hProcessHeap;
    g_ModuleHeapInfo.pphpListAll          = (HANDLE*) &g_phpListAll;
    g_ModuleHeapInfo.lpcsHeapList         = &g_csHeapList;
    g_ModuleHeapInfo.dwNextHeapOffset     = (DWORD*)&g_hProcessHeap->phpNext - (DWORD*)g_hProcessHeap;

    // process heap function overrides
    g_ModuleHeapInfo.pfnLocalAlloc        = (PFNVOID) &LocalAlloc;
    g_ModuleHeapInfo.pfnLocalFree         = (PFNVOID) &LocalFree;
    g_ModuleHeapInfo.pfnLocalAllocTrace   = (PFNVOID) &LocalAllocTrace;
    g_ModuleHeapInfo.pfnLocalReAlloc      = (PFNVOID) &LocalReAlloc;
    g_ModuleHeapInfo.pfnLocalSize         = (PFNVOID) &LocalSize;

    // Custom heap function overrides
    g_ModuleHeapInfo.pfnHeapReAlloc       = (PFNVOID) &HeapReAlloc;
    g_ModuleHeapInfo.pfnHeapAlloc         = (PFNVOID) &HeapAlloc;
    g_ModuleHeapInfo.pfnHeapAllocTrace    = (PFNVOID) &HeapAllocTrace;
    g_ModuleHeapInfo.pfnHeapFree          = (PFNVOID) &HeapFree;
   
    return &g_ModuleHeapInfo;
}

