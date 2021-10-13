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

#if HEAP_SENTINELS
// compiler intrinsic to retrieve immediate caller
EXTERN_C void * _ReturnAddress(void);
#pragma intrinsic(_ReturnAddress)
#endif //HEAP_SENTINELS

#define BLOCK_ALL_CONTINUE      0x55555555
#define AllInUse(x)             (((x) & BLOCK_ALL_CONTINUE) == BLOCK_ALL_CONTINUE)

#define RHF_SEARCH_2ND_PASS     0x01
#define RHF_IN_PLACE_ONLY       0x02

#define IDX2BITMAPDWORD(idx)        ((idx) >> BLOCKS_PER_BITMAP_DWORD_SHIFT)
#define IDX2BLKINBITMAPDWORD(idx)   ((idx) & (BLOCKS_PER_BITMAP_DWORD-1))

#define MAKEIDX(idxBitmap, idxSubBlk) (((idxBitmap) << BLOCKS_PER_BITMAP_DWORD_SHIFT) + (idxSubBlk))

#define BITMAPMASK(idxSubBlk)       (RHF_BITMASK << ((idxSubBlk) << 1))
#define BITMAPBITS(idxSubBlk, bits) ((bits) << ((idxSubBlk) << 1))

#define BLK2SIZE(nBlks)         ((nBlks) << RHEAP_BLOCK_SIZE_SHIFT)      // convert # of blocks to # of bytes
#define LOCALPTR(prrgn, idx)    ((prrgn)->pLocalBase  + BLK2SIZE(idx))

#define ISALLOCSTART(dwBitmap, idxSubBlk)            (((dwBitmap) & BITMAPMASK(idxSubBlk)) == BITMAPBITS((idxSubBlk), RHF_STARTBLOCK))

#define GROWABLE_REGION_SIZE    (sizeof (RHRGN) + NUM_DEFAULT_DWORD_BITMAP_PER_RGN * sizeof (DWORD))
#define GROWABLE_HEAP_SIZE      (sizeof (RHEAP) + NUM_DEFAULT_DWORD_BITMAP_PER_RGN * sizeof (DWORD))

#define PAGEALIGN_UP(size)      (((size)+(VM_PAGE_SIZE-1))&~(VM_PAGE_SIZE-1))
#define BLOCKALIGN_DOWN(x)      ((x) & ~VM_BLOCK_OFST_MASK)

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

#if HEAP_SENTINELS

#ifdef x86

#include <loader.h>

#define IsDwordAligned(p)       (!((DWORD) (p) & 3))

// coredll or k.coredll base and size
DWORD g_dwCoreCodeBase;
DWORD g_dwCoreCodeSize;

static __inline BOOL IsValidStkFrame(DWORD dwFrameAddr, DWORD stkLow, DWORD stkHigh)
{
    return (dwFrameAddr && (dwFrameAddr >= stkLow) && (dwFrameAddr <= stkHigh) && IsDwordAligned(dwFrameAddr));
}

static void GetCallerFrames(PRHEAP_SENTINEL_HEADER pHead, BOOL fAllocSentinel)
{
    DWORD dwEbp = 0;
    DWORD dwReturnAddr = 0;
    DWORD dwMaxCoredllFrames = 10;
    DWORD LowStackLimit = UStkBound ();
    DWORD HighSTackLimit = UStkBase() + UStkSize();
    BOOL fNonCoredllFrame = FALSE;
    static const WORD RetAddrOffset = sizeof(DWORD);
    
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

    PREFAST_DEBUGCHK (((int) cbAlloc > 0) && ((int) cbSize > 0));

    if (   (HEAP_SENTINEL_SIGNATURE != head.dwSig)){
        RETAILMSG(1, (L"RHeapCheckAllocSentinels: Bad head signature @0x%08X  --> 0x%08X-0x%08X\r\n",
                pMem, head.dwSig, HEAP_SENTINEL_SIGNATURE));

    } else if (head.cbSize > cbSize) {
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
                RETAILMSG(1, (L"RHeapCheckAllocSentinels: Bad tail signature item: 0x%08X  @0x%08X -> 0x%02x SHDB 0x%02x\r\n",
                        pMem, pMemTail, *pMemTail, chTailSig));
                break;
            }
        }
    }
    
    if (cbAlloc) {
        RETAILMSG(1, (L"RHeapCheckAllocSentinels: Previous Allocation details Size:0x%8.8lx FreePC:0x%8.8lx AllocPC:0x%8.8lx\r\n",
                                        head.cbSize, head.dwFrames[FreePcIdx], head.dwFrames[AllocPcIdx]));

        if (fStopOnError) {
            DebugBreak ();
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
                RETAILMSG(1, (L"RHeapCheckFreeSentinels: Bad free item: 0x%08X  @0x%08X -> 0x%02x SHDB 0x%02x\r\n",
                        pMem, pdwMem, *pdwMem, HEAP_DWORD_FREE));

                if (pHdrLast) {
                    RHeapDumpAllocations((DWORD)pdwMem, pHdrFirst, pHdrLast);
                }

            if (fStopOnError) {
                DebugBreak ();
            }
            break;
        }
            cdwFree--;
            pdwMem++;
        }
    }

    return !cdwFree;
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

#endif //HEAP_SENTINELS

static LPVOID AllocMemShare (LPVOID pAddr, DWORD cbSize, DWORD fdwAction, LPDWORD pdwUnused)
{
    return CeVirtualSharedAlloc ((MEM_RESERVE & fdwAction)? NULL : pAddr, cbSize, fdwAction);
}

static LPVOID AllocMemVirt (LPVOID pAddr, DWORD cbSize, DWORD fdwAction, LPDWORD pdwUnused)
{
    LPVOID p =  VirtualAlloc (pAddr, cbSize, fdwAction, PAGE_READWRITE);
    DEBUGMSG (DBGFIXHP && (fdwAction == MEM_RESERVE), (L"Reserving new VM of size %8.8lx, returns %8.8lx\r\n", cbSize, p));
    DEBUGMSG (DBGFIXHP && (fdwAction == MEM_COMMIT), (L"Commit memory at 0x%8.8lx of size %8.8lx, returns %8.8lx\r\n", pAddr, cbSize, p));
    return p;
}

static BOOL FreeMemVirt (LPVOID pAddr, DWORD cbSize, DWORD fdwAction, DWORD dwUnused)
{
    BOOL fRet = VirtualFree (pAddr, cbSize, fdwAction);

    DEBUGMSG (DBGFIXHP && (fdwAction == MEM_RELEASE), (L"Releasing VM at address %8.8lx (cbSize = %d)\r\n", pAddr, cbSize));
    DEBUGMSG (DBGFIXHP && (fdwAction == MEM_DECOMMIT), (L"De-commit memory at 0x%8.8lx of size %8.8lx\r\n", pAddr, cbSize));
    return fRet;
}

static void ReleaseMapMemory (HPROCESS hProc, PRHVAITEM pvaitem)
{
    if (pvaitem->pRemoteBase) {
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

        // if hProc is NULL, the process has exited.
        if (hProc) {
            ReleaseMapMemory (hProc, pvaitem);
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
            if (   (pvaitem->hMapfile    = CreateFileMapping (INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, cbSize, NULL))
                && (pvaitem->pLocalBase  = (LPBYTE) MapViewOfFile (pvaitem->hMapfile, FILE_MAP_ALL_ACCESS, 0, 0, 0))
                && (pvaitem->pRemoteBase = (LPBYTE) MapViewInProc (hProc, pvaitem->hMapfile, dwClientProtection, 0, 0, 0))) {
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

//
// FreeBlocks - mark 'nBlks' blocks from 'idxBlock' free.
//
#if HEAP_SENTINELS
static void FreeBlocks (PRHRGN prrgn, DWORD idxBlock, DWORD nBlks, DWORD dwCaller)
#else
static void FreeBlocks (PRHRGN prrgn, DWORD idxBlock, DWORD nBlks)
#endif //HEAP_SENTINELS
{
    DWORD idxBitmap = IDX2BITMAPDWORD(idxBlock);
    DWORD idxSubBlk = IDX2BLKINBITMAPDWORD(idxBlock);
    DWORD dwMask    = alcMasks[idxSubBlk];
    DWORD idx       = idxBitmap;

    DEBUGCHK (nBlks);
    DEBUGCHK (idxBlock + nBlks <= prrgn->idxBlkCommit);

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
#if HEAP_SENTINELS
                                (IsHeapRgnEmbedded (prrgn->phpOwner)) ? NULL : &prrgn->dwRgnData);
#else
                                &prrgn->dwRgnData);
#endif

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


//
// CountFreeBlocks - Count the size, in blocks, of a free item, excluding uncommitted memory
//
static DWORD CountFreeBlocks (PRHRGN prrgn, DWORD idxFree, DWORD nBlksNeeded)
{
    DWORD nBlksFound = 0;
    DWORD idxBitmap  = IDX2BITMAPDWORD(idxFree);
    DWORD idxSubBlk     = IDX2BLKINBITMAPDWORD(idxFree);
    DWORD dwBitmap   = prrgn->allocMap[idxBitmap];

    DEBUGCHK (idxFree < prrgn->idxBlkCommit);

    // test to see if we can extend to next bitmap dword
    if (!(dwBitmap & ~alcMasks[idxSubBlk])) {
        
        // yes, we can extend to next bitmap dword
        
        DWORD idxLastBitmap = IDX2BITMAPDWORD(prrgn->idxBlkCommit);

        nBlksFound = BLOCKS_PER_BITMAP_DWORD - idxSubBlk;
        
        while ((nBlksFound < nBlksNeeded) && (++ idxBitmap < idxLastBitmap) && !(dwBitmap = prrgn->allocMap[idxBitmap])) {
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

//
// CountAllocBlocks - count the size, in block, of an allocated item.
//
static DWORD CountAllocBlocks (PRHRGN prrgn, DWORD idxAlloc)
{
    DWORD nBlks = 0;

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

#ifdef HEAP_STATISTICS
    prrgn->phpOwner->cIters ++;
#endif

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

#ifdef HEAP_STATISTICS
        prrgn->phpOwner->cIters ++;
#endif
        idxBlock += nBlksFound;
        
        idxBlock += SkipAllocBlocks (prrgn, idxBlock);

        nBlksFound = 0;
    }

    DEBUGCHK (idxBlock <= prrgn->idxBlkCommit);

#if HEAP_SENTINELS
    if (idxBlock < idxLimit) {
        RHeapCheckFreeSentinels (LOCALPTR (prrgn, idxBlock), nBlksFound, TRUE);
    }
#endif

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

    return (nBlksFound >= nBlks)? idxBlock : -1;
}

//
// FindBlockInRegion - try to find 'nBlks' consecutive free blocks in a region.
//
static DWORD FindBlockInRegion (PRHRGN prrgn, DWORD nBlks)
{
    DWORD idxBlkRet;
    DWORD dwOldMax = prrgn->maxBlkFree;

    
    // reset max, will be updated automatically through the search
    prrgn->maxBlkFree = 0;

    // search for idxFree first
    idxBlkRet = DoFindBlockInRegion (prrgn, nBlks, prrgn->idxBlkFree, prrgn->idxBlkCommit);

    if ((-1 == idxBlkRet)
        || (idxBlkRet + nBlks > prrgn->idxBlkCommit)) {
        // couldn't find a block, or it requires committing extra page
        DEBUGCHK (prrgn->numBlkFree >= (prrgn->numBlkTotal - prrgn->idxBlkCommit));

        if (prrgn->idxBlkFree)  {
            // search from start and see if we can find one
            DWORD idx = DoFindBlockInRegion (prrgn, nBlks, 0, prrgn->idxBlkFree);

            if (-1 != idx) {
                // found blocks that doesn't require committing
                idxBlkRet = idx;
            }
        }
    }

    if (-1 != idxBlkRet) {

        // found enough blocks. Commit memory if necessary        
        if (!CommitBlocks (prrgn, idxBlkRet + nBlks)) {
            DEBUGCHK (idxBlkRet + nBlks > prrgn->idxBlkCommit);
            idxBlkRet = -1;
            
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
static BOOL InitRegion (PRHEAP php, PRHRGN prrgn, LPBYTE lpBase, DWORD cbInit, DWORD cbMax, DWORD nHdrBlks)
{   
    DEBUGCHK (PAGEALIGN_UP (cbMax)  == cbMax);
    DEBUGCHK (PAGEALIGN_UP (cbInit) == cbInit);
    
    memset (prrgn, 0, sizeof (RHRGN));
    prrgn->dwRgnData   = (DWORD) prrgn;
    prrgn->phpOwner    = php;
    prrgn->cbSize      = sizeof (RHRGN);
    prrgn->pLocalBase  = lpBase;
    
    // reserve and if necessary commit memory for region.
    if (!prrgn->pLocalBase) {
        if (prrgn->pLocalBase = (LPBYTE) php->pfnAlloc (NULL, cbMax, MEM_RESERVE, &prrgn->dwRgnData)) {
            if (cbInit && !php->pfnAlloc (prrgn->pLocalBase, cbInit, MEM_COMMIT, &prrgn->dwRgnData)) {
                php->pfnFree (prrgn->pLocalBase, 0, MEM_RELEASE, prrgn->dwRgnData);
                prrgn->pLocalBase = NULL;
            }
        }
    }

    // initialize rgn structure
    if (prrgn->pLocalBase) {
        prrgn->idxBlkCommit = RHEAP_BLOCK_CNT (cbInit);
        prrgn->numBlkFree   = prrgn->maxBlkFree = prrgn->numBlkTotal = RHEAP_BLOCK_CNT (cbMax);

        // avoid memset for allocators which return zeroed memory (uncomment the following line if this needs to be optimized)
        // if (!IsHeapRgnEmbedded(php) || ((php->pfnAlloc != AllocMemVirt)  && (php->pfnAlloc != AllocMemShare)))
        memset ((prrgn+1), 0, IDX2BITMAPDWORD(prrgn->numBlkTotal) * sizeof (DWORD));
        
#if HEAP_SENTINELS
        RHeapSetFreeSentinels (LOCALPTR(prrgn, nHdrBlks), prrgn->idxBlkCommit - nHdrBlks, 0);
#endif            
    }

    return NULL != prrgn->pLocalBase;
}

//
// RHeapGrowNewRegion - Grow a heap by creating a new region 
// Note: Heap should have been created with max size == 0
//
static PRHRGN RHeapGrowNewRegion (PRHEAP php, DWORD nInitBlks, LPDWORD pdwIdxBlk)
{
    PRHRGN prrgn = NULL;
    LPBYTE lpBase = NULL;
    DWORD cbInit, dwRgnData = 0;
    
    DEBUGCHK ((DWORD) php->cs.OwnerThread == GetCurrentThreadId ());

    if (IsHeapRgnEmbedded(php)) { 
        // rgn structure to be embedded with data
        *pdwIdxBlk = RHEAP_BLOCK_CNT (GROWABLE_REGION_SIZE+HEAP_SENTINELS);    
        cbInit = PAGEALIGN_UP (BLK2SIZE(nInitBlks + *pdwIdxBlk));
        if (lpBase = (LPBYTE) php->pfnAlloc (NULL, CE_FIXED_HEAP_MAXSIZE, MEM_RESERVE, &dwRgnData)) {
            if (php->pfnAlloc (lpBase, cbInit, MEM_COMMIT, &dwRgnData)) {
                prrgn = (PRHRGN) (lpBase + HEAP_SENTINELS);                
            } else {
                php->pfnFree (lpBase, 0, MEM_RELEASE, dwRgnData);
            }
        }

    } else {
        // rgn structure is separate from data
        *pdwIdxBlk = 0;
        cbInit = PAGEALIGN_UP (BLK2SIZE(nInitBlks));
        prrgn = (PRHRGN) RHeapAlloc (g_hProcessHeap, 0, GROWABLE_REGION_SIZE);
    }
        
    DEBUGCHK (!php->cbMaximum);
    
    if (prrgn) {

        if (!InitRegion (php, prrgn, lpBase, cbInit, CE_FIXED_HEAP_MAXSIZE, *pdwIdxBlk)) {
            if (IsHeapRgnEmbedded (php)) {
                php->pfnFree (lpBase, 0, MEM_RELEASE, dwRgnData);
            } else {
                RHeapFree (g_hProcessHeap, 0, prrgn);
            }
            prrgn = NULL;
            
        } else {

            // add region to region list
            php->prgnLast->prgnNext = prrgn;
            php->prgnLast = prrgn;

            // for embedded heap, take space for rgn structure
            if (IsHeapRgnEmbedded(php)) {
                TakeBlocks (prrgn, 0, *pdwIdxBlk);
#if HEAP_SENTINELS
                RHeapSetAllocSentinels (prrgn->pLocalBase, GROWABLE_REGION_SIZE, TRUE);
#endif                
            }
                    
            // take the blocks for initial allocation
            TakeBlocks (prrgn, *pdwIdxBlk, nInitBlks);
            prrgn->maxBlkFree = prrgn->numBlkFree;
        }
    }

    if (!prrgn) {
        *pdwIdxBlk = -1;
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

#ifdef HEAP_STATISTICS
            php->cIters ++;
#endif
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
            DWORD idxSubBlk = IDX2BLKINBITMAPDWORD(idxBlock);

            if (ISALLOCSTART (prrgn->allocMap[IDX2BITMAPDWORD(idxBlock)], idxSubBlk)) {
                *pfInRegion = TRUE;
                pitem = (PRHVAITEM) prrgn;
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
        && (prrgn->maxBlkFree        <= prrgn->numBlkFree)
        && (prrgn->numBlkFree        <= prrgn->numBlkTotal)
        && !((DWORD) prrgn->pLocalBase  & VM_BLOCK_OFST_MASK)
        && !((DWORD) prrgn->pRemoteBase & VM_BLOCK_OFST_MASK);
        
}

//
// DoRHeapVMAlloc - create a VirtualAlloc'd item
//
static LPBYTE DoRHeapVMAlloc (PRHEAP php, DWORD dwFlags, DWORD cbSize)
{
    PRHVAITEM pitem = (PRHVAITEM) RHeapAlloc (g_hProcessHeap, 0, sizeof (RHVAITEM));
    LPBYTE    lpRet = NULL;

    DEBUGCHK ((int) cbSize > 0);
    if (pitem) {
        // initialize the item
        memset (pitem, 0, sizeof (RHVAITEM));
        pitem->dwRgnData = (DWORD) pitem;   // block align the reservation
        pitem->cbReserve = (cbSize + HEAP_SENTINELS + VM_BLOCK_OFST_MASK) & ~VM_BLOCK_OFST_MASK;
        pitem->cbSize    = cbSize;
        pitem->phpOwner  = php;

        // allocate memory
        EnterCriticalSection (&php->cs);
#ifdef HEAP_STATISTICS
        InterlockedIncrement (&lNumAllocs[BUCKET_VIRTUAL]);
        php->cIters ++;
#endif
        if (pitem->pLocalBase = (LPBYTE) php->pfnAlloc (NULL, pitem->cbReserve, MEM_RESERVE, &pitem->dwRgnData)) {
            if (php->pfnAlloc (pitem->pLocalBase, cbSize + HEAP_SENTINELS, MEM_COMMIT, &pitem->dwRgnData)) {
                // insert to heap's VirtualAlloc's item list
                pitem->pNext = php->pvaList;
                php->pvaList = pitem;

                lpRet = pitem->pLocalBase;

#if HEAP_SENTINELS
                RHeapSetAllocSentinels (lpRet, cbSize, FALSE);
                lpRet += HEAP_SENTINELS;
#endif

            } else {
                // failed
                VERIFY (php->pfnFree (pitem->pLocalBase, 0, MEM_RELEASE, pitem->dwRgnData));
                VERIFY (RHeapFree (g_hProcessHeap, 0, pitem));
            }

        } else {
            // failed to reserve VM
            VERIFY (RHeapFree (g_hProcessHeap, 0, pitem));
        }

        LeaveCriticalSection (&php->cs);
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

        for (ppitem = &php->pvaList; pitem = *ppitem; ppitem = &pitem->pNext) {
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
    PRHEAP php = NULL;
    LPBYTE lpBase = NULL; 
    DWORD  nBitmapDwords;    
    DWORD nHdrBlocks = 0, dwRgnData = 0;
    BOOL fEmbeddedRgn = FALSE;

    DEBUGCHK ((int) dwMaximumSize >= 0);

    if (dwMaximumSize) {
        // heap with fixed size
        dwMaximumSize = PAGEALIGN_UP (dwMaximumSize);
        dwInitialSize = PAGEALIGN_UP (dwInitialSize);
        nBitmapDwords = IDX2BITMAPDWORD (RHEAP_BLOCK_CNT (dwMaximumSize));

    } else {
        // growable heap        
        if (!(flOptions & HEAP_IS_REMOTE) 
            && ((pfnAlloc == AllocMemVirt) || (pfnAlloc == AllocMemShare))) {
            //
            // embed rgn structure with data only for local growable
            // heaps with standard memory allocators. Reason for this 
            // is that for rgn structure to be embedded, two conditions 
            // have to be met:
            // a) rgn structure has to start on a 64k boundary
            // b) rgn data should not be more than 64k in size
            //
            fEmbeddedRgn = TRUE;
            flOptions |= HEAPRGN_IS_EMBEDDED;
            dwInitialSize = GROWABLE_HEAP_SIZE + HEAP_SENTINELS;
            nHdrBlocks = RHEAP_BLOCK_CNT (dwInitialSize);
        }

        nBitmapDwords = NUM_DEFAULT_DWORD_BITMAP_PER_RGN;
        dwInitialSize = PAGEALIGN_UP (dwInitialSize);
        if (dwInitialSize > CE_FIXED_HEAP_MAXSIZE)
            dwInitialSize = CE_FIXED_HEAP_MAXSIZE;
    }

    if (fEmbeddedRgn) {
        // for embedded heaps, create the heap structure to be embedded with data
        if (lpBase = (LPBYTE) pfnAlloc (NULL, CE_FIXED_HEAP_MAXSIZE, MEM_RESERVE, &dwRgnData)) {
            if (pfnAlloc (lpBase, dwInitialSize, MEM_COMMIT, &dwRgnData)) {
                php = (PRHEAP) (lpBase + HEAP_SENTINELS);                
            } else {
                pfnFree (lpBase, 0, MEM_RELEASE, dwRgnData);                
            }
        }
        
    } else {
        // for remote or fixed heaps, create heap structure separate from data
        php = (PRHEAP) RHeapAlloc (g_hProcessHeap, 0, sizeof (RHEAP) + nBitmapDwords * sizeof (DWORD));
    }
    
    if (php) {
        php->cbSize      = sizeof (RHEAP);
        php->cbMaximum   = dwMaximumSize;
        php->dwSig       = HEAPSIG;
        php->pfnAlloc    = pfnAlloc;
        php->pfnFree     = pfnFree;
        php->prgnLast    = &php->rgn;
        php->pvaList     = NULL;
        php->dwProcessId = dwProcessId;
        php->flOptions = flOptions;
        php->prgnfree    = &php->rgn;

        if (!InitRegion (php, &php->rgn, lpBase, dwInitialSize, dwMaximumSize? dwMaximumSize : CE_FIXED_HEAP_MAXSIZE, nHdrBlocks)) {
            if (fEmbeddedRgn) {
                pfnFree (lpBase, 0, MEM_RELEASE, dwRgnData);                
            } else {
                VERIFY (RHeapFree (g_hProcessHeap, 0 , php));
            }
            php = NULL;
            
        } else {

            // for local heap, take space for heap/rgn structure
            if (fEmbeddedRgn) {
                PRHRGN prrgn = &php->rgn;
                TakeBlocks (prrgn, 0, nHdrBlocks);
                prrgn->maxBlkFree = prrgn->numBlkFree;                
#if HEAP_SENTINELS
                    RHeapSetAllocSentinels (prrgn->pLocalBase, GROWABLE_HEAP_SIZE, TRUE);
#endif                
            }

            // add the heap to the heap list
            InitializeCriticalSection (&php->cs);
            EnterCriticalSection (&g_csHeapList);
            php->phpNext = g_phpListAll;
            g_phpListAll = php;
            LeaveCriticalSection (&g_csHeapList);

            if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
                CELOG_HeapCreate(flOptions, dwInitialSize, dwMaximumSize, (HANDLE)php);
            }
        }
    }

    return php;
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

    for (idxBlock = 0; idxBlock < prrgn->idxBlkCommit; idxBlock += nBlks) {
        
        if (nBlks = CountAllocBlocks (prrgn, idxBlock)) {
            // allocated blocks
            DEBUGCHK (idxBlock + nBlks <= prrgn->idxBlkCommit);
            
            idxLastFree = prrgn->idxBlkCommit;

            if (!(*pfnEnum) (LOCALPTR(prrgn, idxBlock), BLK2SIZE (nBlks), RHE_NORMAL_ALLOC, pEnumData)) {
                break;
            }
            
        } else if (nBlks = CountFreeBlocks (prrgn, idxBlock, prrgn->numBlkFree)) {

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

    DEBUGCHK (idxBlock <= prrgn->idxBlkCommit);

    if (idxBlock == prrgn->idxBlkCommit) {
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
    PRHRGN    prrgn = &php->rgn;
    PRHVAITEM pitem = NULL;
    EnterCriticalSection (&php->cs);

    __try {
        for ( ; prrgn; prrgn = prrgn->prgnNext) {
            if (!EnumerateRegionItems (prrgn, pfnEnum, pEnumData)) {
                break;
            }
        }

        if (!prrgn) {
            // enumerate through VirtualAlloc'd items
            for (pitem = php->pvaList; pitem; pitem = pitem->pNext) {
                if (! (*pfnEnum) (pitem->pLocalBase, pitem->cbSize + HEAP_SENTINELS, RHE_VIRTUAL_ALLOC, pEnumData)) {
                    break;
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // empty except block
    }
    
    LeaveCriticalSection (&php->cs);

    return !prrgn && !pitem;
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

//
// DoRHeapCompactRegion - compact a region
//
static UINT DoRHeapCompactRegion (PRHRGN prrgn)
{
    UINT  uiMax = 0;
    DWORD idxBitmap = IDX2BITMAPDWORD(prrgn->idxBlkCommit);
    DWORD idxNewCommit;
    
    // find out the last dword that is not completely freed
    while (idxBitmap && !prrgn->allocMap[idxBitmap-1]) {
        idxBitmap --;
    }

    // calculate the size to be decommitted
    idxNewCommit = RHEAP_BLOCK_CNT (PAGEALIGN_UP (BLK2SIZE (MAKEIDX (idxBitmap, 0))));

    // decommit pages and update prrgn->idxBlkCommit if needed
    if (idxNewCommit < prrgn->idxBlkCommit) {
        VERIFY (prrgn->phpOwner->pfnFree (LOCALPTR (prrgn, idxNewCommit),
                                          BLK2SIZE (prrgn->idxBlkCommit - idxNewCommit),
                                          MEM_DECOMMIT,
                                          prrgn->dwRgnData));
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
    DWORD  nBlks = RHEAP_BLOCK_CNT(cbSize+HEAP_SENTINELS);

    DEBUGMSG (DBGHEAP2, (L"RHeapAlloc %8.8lx %8.8lx %8.8lx\r\n", php, dwFlags, cbSize));
    if (!php->cbMaximum && (nBlks > RHEAP_VA_BLOCK_CNT)) {
        pMem = DoRHeapVMAlloc (php, dwFlags, cbSize);
        // memory already zero'd for VirtualAlloc'd items
        
    } else {

        PRHRGN prrgn;
        DWORD  idxBlock = -1;
        DWORD  cbActual = cbSize;

#ifdef HEAP_STATISTICS
        int idx = (cbSize >> 4);   // slot number

        if (idx > BUCKET_LARGE)      // idx BUCKET_LARGE is for items >= 1K in size
            idx = BUCKET_LARGE;
        
        InterlockedIncrement (&lNumAllocs[idx]);
#endif

        DEBUGCHK (nBlks);

        EnterCriticalSection (&php->cs);
#ifdef HEAP_STATISTICS
        php->cIters = 0;
#endif

        // search from php->prgnfree
        prrgn = php->prgnfree;

        PREFAST_DEBUGCHK (prrgn);

        do {
            if (prrgn->maxBlkFree >= nBlks) {
                idxBlock = FindBlockInRegion (prrgn, nBlks);
                if (-1 != idxBlock) {
                    // free sentinels already checked in FindBlockInRegion
                    break;
                }
            }
#ifdef HEAP_STATISTICS
            php->cIters ++;
#endif
            prrgn = prrgn->prgnNext;
            if (!prrgn) {
                prrgn = &php->rgn;
            }
        } while (prrgn != php->prgnfree);

        if ((-1 == idxBlock) && !php->cbMaximum) {
            prrgn = RHeapGrowNewRegion (php, nBlks, &idxBlock);
#ifdef HEAP_STATISTICS
            php->cIters ++;
#endif
        }


#ifdef HEAP_STATISTICS
        InterlockedIncrement (&lTotalAllocations);
        InterlockedExchangeAdd (&lTotalIterations, php->cIters);
#endif

        if (-1 != idxBlock) {

            DEBUGCHK (prrgn);

            php->prgnfree = prrgn;
            
            // return the local address
            pMem = LOCALPTR (prrgn, idxBlock);

#if HEAP_SENTINELS
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
// RHeapFree - free memory from a heap
//
extern "C"
#if HEAP_SENTINELS
BOOL WINAPI RHeapFreeWithCaller (PRHEAP php, DWORD dwFlags, LPVOID ptr, DWORD dwCaller)
#else
BOOL WINAPI RHeapFree (PRHEAP php, DWORD dwFlags, LPVOID ptr)
#endif
{
    PRHRGN prrgn;
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
            nBlks    = CountAllocBlocks (prrgn, idxBlock);
            php->prgnfree = prrgn;

            if (nBlks) {
#if HEAP_SENTINELS
                RHeapCheckAllocSentinels (pMem, nBlks, TRUE);
                FreeBlocks (prrgn, idxBlock, nBlks, dwCaller);
#else
                FreeBlocks (prrgn, idxBlock, nBlks);
#endif
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
            // debug check only if we are not using dll heap
            // since this condition is possible when using dll heap
            DEBUGCHK (0);
        }
    }
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CELOG_HeapFree (php, dwFlags, (DWORD)ptr);
    }
    DEBUGMSG (DBGHEAP2, (L"RHeapFree returns %8.8lx\r\n", !pMem));

    return !pMem;
}

#if HEAP_SENTINELS
extern "C"
BOOL WINAPI RHeapFree (PRHEAP php, DWORD dwFlags, LPVOID ptr)
{
    return RHeapFreeWithCaller (php, dwFlags, ptr, 0);
}
#endif

//
// RHeapSize - get the size of a heap item
//
extern "C" 
DWORD WINAPI RHeapSize (PRHEAP php, DWORD dwFlags, LPCVOID ptr)
{
    DWORD       dwRet = -1;
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
        if (nBlks = CountAllocBlocks (prrgn, idxBlock)) {
            dwRet = BLK2SIZE (nBlks);
#if HEAP_SENTINELS
            RHeapCheckAllocSentinels (pMem, nBlks, TRUE);
            dwRet = ((PRHEAP_SENTINEL_HEADER) pMem)->cbSize;
#endif
        }
        
    } else if (pitem = FindVaItemByPtr (php, pMem, 0)) {
        // virtual alloced item
        dwRet = pitem->cbSize;
#if HEAP_SENTINELS
        RHeapCheckAllocSentinels (pMem, RHEAP_BLOCK_CNT (dwRet + HEAP_SENTINELS), TRUE);
#endif
    }


    LeaveCriticalSection (&php->cs);

    if (-1 == dwRet) {
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
        DWORD nBlks  = CountAllocBlocks (prrgn, idxBlock);

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

                
            } else if (nBlksNeeded < 0) {
                // shrink
#if HEAP_SENTINELS
                FreeBlocks (prrgn, idxBlock+nBlksNeeded, -nBlksNeeded, 0);
#else
                FreeBlocks (prrgn, idxBlock+nBlksNeeded, -nBlksNeeded);
#endif
            }
            
        }
    } else if (pitem = FindVaItemByPtr (php, pMem, 0)) {
    
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
            } else if (pMemRet = RHeapAlloc (php, dwFlags, cbSize)) {
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

    DEBUGCHK (dwMaximumSize >= 0);

    DEBUGMSG(DBGFIXHP, (L"HeapCreate %8.8lx %8.8lx %8.8lx\r\n", flOptions, dwInitialSize, dwMaximumSize));

    php = DoRHeapCreate (flOptions, dwInitialSize, dwMaximumSize, (flOptions & HEAP_SHARED_READONLY)? AllocMemShare : AllocMemVirt, FreeMemVirt, GetCurrentProcessId ());
    
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
            PRHRGN prrgn;
            PRHVAITEM pitem;
            BOOL fFreeHeapRgnStructure = !IsHeapRgnEmbedded(php);
            
            // destroy the heap itself
            
            EnterCriticalSection (&php->cs);

            // free all virtual-allocated items
            while (pitem = php->pvaList) {
                php->pvaList = pitem->pNext;
                // free VM
                VERIFY (php->pfnFree (pitem->pLocalBase, 0, MEM_RELEASE, pitem->dwRgnData));
                // free the item itself
                VERIFY (RHeapFree (g_hProcessHeap, 0, pitem));
            }

            // free all dynamically created regions
            while (prrgn = php->rgn.prgnNext) {
                php->rgn.prgnNext = prrgn->prgnNext;
                // free VM
                VERIFY (php->pfnFree (prrgn->pLocalBase, 0, MEM_RELEASE, prrgn->dwRgnData));
                // free region itself
                if (fFreeHeapRgnStructure) {
                    VERIFY (RHeapFree (g_hProcessHeap, 0, prrgn));
                }
            }

            // [cs] need to be deleted before base rgn gets deleted
            // for local heaps destroying base rgn will destroy heap
            LeaveCriticalSection (&php->cs);
            DeleteCriticalSection (&php->cs);

            // decommit the main region
            prrgn = &php->rgn;
            VERIFY ((php->pfnFree (prrgn->pLocalBase, 0, MEM_RELEASE, prrgn->dwRgnData)));

            // for remote and non-growable heaps, heap structure is separate from data            
            if (fFreeHeapRgnStructure) {
                VERIFY (RHeapFree (g_hProcessHeap, 0, php));
            }
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
// RHeapCompact - compact a heap
//
extern "C"
UINT WINAPI RHeapCompact (PRHEAP php, DWORD dwFlags)
{
    PRHRGN prrgnPrev = &php->rgn, prrgn;
    UINT   uiMax = 0, uiCurRgn;
    DWORD dwDfltBlks = 0;
    
    DEBUGMSG(DBGFIXHP, (L"RHeapCompact %8.8lx %8.8lx\r\n", php, dwFlags));
    
    EnterCriticalSection (&php->cs);

    if (IsHeapRgnEmbedded(php)) {
        // every embedded region (after the first one) has 
        // certain # of blks reserved (discount these)
        dwDfltBlks = RHEAP_BLOCK_CNT(GROWABLE_REGION_SIZE+HEAP_SENTINELS);
    }        

    php->prgnfree = &php->rgn;
    
    prrgn = prrgnPrev->prgnNext;
    
    // take care of base region
    uiMax = DoRHeapCompactRegion (prrgnPrev);

    // compact the rest of the regions
    while (prrgn) {
        
        if (prrgn->numBlkTotal == prrgn->numBlkFree+dwDfltBlks) {

            // completely free region, destroy it
            prrgnPrev->prgnNext = prrgn->prgnNext;
            if (php->prgnLast == prrgn) {
                DEBUGCHK (!prrgn->prgnNext);
                php->prgnLast = prrgnPrev;
            }

            VERIFY (php->pfnFree (prrgn->pLocalBase, 0, MEM_RELEASE, prrgn->dwRgnData));

            // if rgn structure is separate from data, free the rgn structure
            if (!IsHeapRgnEmbedded(php)) {
                VERIFY (RHeapFree (g_hProcessHeap, 0, prrgn));
            }
            
        } else {
            uiCurRgn = DoRHeapCompactRegion (prrgn);
            if (uiMax < uiCurRgn) {
                uiMax = uiCurRgn;
            }

            // move on to the next region
            prrgnPrev = prrgn;
        }
        prrgn = prrgnPrev->prgnNext;
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
            && ((php->dwProcessId & 3) == 2)            // process id valid
            && !((DWORD) php->prgnLast & 3)             // region ptr valid
            && !((DWORD) php->pvaList & 3);             // virtual allocate item valid

        if (fRet) {
            PRHRGN prrgn;
            // validate regions
            EnterCriticalSection (&php->cs);

            __try {
                for (prrgn = &php->rgn; prrgn; prrgn = prrgn->prgnNext) {
                    if (!(fRet = ValidateRegion (php, prrgn))) {
                        // region structure is not init properly
                        break;

                    } else if (!prrgn->prgnNext && (php->prgnLast != prrgn)) {
                        // last rgn pointer is not init properly
                        fRet = FALSE;
                        break;
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

//
// RHeapDump - dump the heap
//
extern "C"
void RHeapDump (PRHEAP php)
{
    PRHRGN prrgn;
    PRHVAITEM pitem;
    DWORD  cbMax = 0;
    NKDbgPrintfW (L"====== Dumping Heap At 0x%8.8lx ======\r\n", php);
    NKDbgPrintfW (L"       ProcessId:    0x%8.8lx\r\n",                     php->dwProcessId);
    NKDbgPrintfW (L"       Options:      0x%8.8lx\r\n",                     php->flOptions);
    NKDbgPrintfW (L"       Maximum Size: 0x%8.8lx (0 means growable)\r\n",  php->cbMaximum);

    for (prrgn = &php->rgn; prrgn; prrgn = prrgn->prgnNext) {
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
        NKDbgPrintfW (L"       -- Max Committed Consecutive Free Memory: %d (estimate = %d)\r\n", cbMax, BLK2SIZE (prrgn->maxBlkFree));
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
LPCVOID CeRemoteHeapTranslatePointer (HANDLE hHeap, DWORD dwFlags, LPCVOID pMem)
{
    PRHEAP      php = (PRHEAP) hHeap;
    BOOL fIsRemotePtr = (dwFlags & CE_HEAP_REVERSE_TRANSLATE);
    PRHVAITEM   pitem;
    const BYTE *ptr;
    BOOL        fInRegion;
    DEBUGMSG(DBGFIXHP, (L"CeRemoteHeapTranslatePointer %8.8lx %8.8lx\r\n", php, pMem));

    if (    (dwFlags & ~CE_HEAP_REVERSE_TRANSLATE)
        || !(php->flOptions & HEAP_IS_REMOTE)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    
#if HEAP_SENTINELS
    pMem = RHeapSentinelAdjust ((LPBYTE) pMem);
#endif
    ptr = (const BYTE *) pMem;

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

    InitializeCriticalSection(&g_csHeapList);
    
#if HEAP_SENTINELS
#ifdef x86
    if (IsKModeAddr ((DWORD)LMemInit)) {
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
    
    if (php = DoRHeapCreate (HEAP_IS_PROC_HEAP, 0, 0, AllocMemVirt, FreeMemVirt, GetCurrentProcessId())) {
        
        g_hProcessHeap = php;

        // data to be passed to the dll heap implementation
        g_ModuleHeapInfo.nDllHeapModules = 0;
        g_ModuleHeapInfo.hProcessHeap = (HANDLE) g_hProcessHeap; // can pass the value as this is always fixed to the heap created in coredll
        g_ModuleHeapInfo.pphpListAll = (HANDLE*) &g_phpListAll; // need to pass address since this variable changes to point to the head of the heap list always
        g_ModuleHeapInfo.lpcsHeapList = &g_csHeapList;
        g_ModuleHeapInfo.dwNextHeapOffset = (DWORD*)&g_hProcessHeap->phpNext - (DWORD*)g_hProcessHeap;

        if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
            CELOG_HeapCreate (HEAP_IS_PROC_HEAP, 0, CE_FIXED_HEAP_MAXSIZE, g_hProcessHeap);
        }

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
            && (hProc = OpenProcess (PROCESS_ALL_ACCESS, FALSE, php->dwProcessId))) {

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
#ifdef HEAP_SENTINELS
    DWORD dwCurrCount = g_ModuleHeapInfo.nDllHeapModules;

    // clear the structure
    memset (&g_ModuleHeapInfo, 0, sizeof(g_ModuleHeapInfo));
    
    // data to be passed to the dll heap implementation
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
#endif
    return &g_ModuleHeapInfo;
}

