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
//    heap.cpp - DDx heap analysis
//


#include "osaxs_p.h"
#include "DwCeDump.h"
#include "diagnose.h"
#include "..\..\..\coreos\core\lmem\heap.h"
#include "..\..\..\coreos\core\inc\dllheap.h"



#define DDX_MIN_HEAP_ITEM_HEAD_SIZE     128
#define DDX_MIN_HEAP_ITEM_TAIL_SIZE     128
#define DDX_MAX_HEAP_ITEM_SIZE          (DDX_MIN_HEAP_ITEM_HEAD_SIZE + DDX_MIN_HEAP_ITEM_TAIL_SIZE)
#define DDX_CORRUPTION_ADDR_PADDING     32  // number of bytes to add to the text memory dump on either side of the corruption location



typedef struct _ITEM_DATA
{
    LPBYTE  pMem;
    DWORD   cbSize; 
    DWORD   dwFlags;
}
ITEM_DATA, *PITEM_DATA;


typedef struct _ENUM_VALIDATE_DATA
{
    ITEM_DATA lastItem;
    int       nFailures;
    PRHEAP    pHeap;
    PPROCESS  pProcess;
    DWORD     dwProcessId;
}
ENUM_VALIDATE_DATA, *PENUM_VALIDATE_DATA;




// From coreos\core\lmem ...

#define BLOCK_ALL_CONTINUE      0x55555555
#define AllInUse(x)             (((x) & BLOCK_ALL_CONTINUE) == BLOCK_ALL_CONTINUE)

#define BLOCK_HEAD_MASK         0xAAAAAAAA
#define HasItemHead(x)          ((x) & BLOCK_HEAD_MASK)

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


const DWORD alcHeadBlk [BLOCKS_PER_BITMAP_DWORD] = {
        0x80000000,
        0x20000000,
        0x08000000,
        0x02000000,
        0x00800000,
        0x00200000,
        0x00080000,
        0x00020000,
        0x00008000,
        0x00002000,
        0x00000800,
        0x00000200,
        0x00000080,
        0x00000020,
        0x00000008,
        0x00000002,
};



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
        
        while ((nBlksFound < nBlksNeeded) && (++idxBitmap < idxLastBitmap))
        {
            dwBitmap = prrgn->allocMap[idxBitmap];

            if (dwBitmap)
                break;

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

            while (++idxBitmap < idxLastBitmap) 
            {
                dwBitmap = prrgn->allocMap[idxBitmap];

                if (dwBitmap != BLOCK_ALL_CONTINUE)
                    break;

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
        
        nBlks = CountAllocBlocks (prrgn, idxBlock);

        if (nBlks) {
            // allocated blocks
            DEBUGCHK (idxBlock + nBlks <= prrgn->idxBlkCommit);
            
            idxLastFree = prrgn->idxBlkCommit;

            if (!(*pfnEnum) (LOCALPTR(prrgn, idxBlock), BLK2SIZE (nBlks), RHE_NORMAL_ALLOC, pEnumData)) {
                break;
            }
            
        } else {
            
            nBlks = CountFreeBlocks (prrgn, idxBlock, prrgn->numBlkFree);
            
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
                DEBUGGERMSG(OXZONE_ALERT, (L"EnumerateRegionItems: encountered bad block (idx = 0x%x) in region 0x%8.8lx\r\n", idxBlock, prrgn));
                 break;
            }
        }
    }

    return (idxBlock == prrgn->idxBlkCommit);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
MODULEHEAPINFO*
GetModuleHeapInfoPtr (
                      PPROCESS pProc
                      )
{
    MODULEHEAPINFO* pModHeapInfo = NULL;
    DWORD dwIoctl = 0;

    if (NKKernelLibIoControl != NULL && g_hProbeDll)
    {
        if (pProc == g_pprcNK)
            dwIoctl = IOCTL_DDX_PROBE_GET_NK_HEAP_INFO;
        else
            dwIoctl = IOCTL_DDX_PROBE_GET_HEAP_INFO;

        if (!NKKernelLibIoControl(
                                g_hProbeDll, 
                                dwIoctl, 
                                NULL, 0, 
                                (LPVOID) &pModHeapInfo, sizeof(MODULEHEAPINFO*), 
                                NULL))
        {
            DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  DDx!GetProcessHeap:  FAILED to get mod heap info.\r\n"));
            return NULL;
        } 

        return pModHeapInfo;
    }

    return NULL; 
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL EnumFindItem (LPBYTE pMem, DWORD cbSize, DWORD dwFlags, LPVOID pEnumData)
{
    DDX_HEAP_ITEM_INFO* pid     = (DDX_HEAP_ITEM_INFO*) pEnumData;
    DWORD cbAlloc               = BLK2SIZE (RHEAP_BLOCK_CNT (cbSize));
    DWORD cbData                = cbAlloc - HEAP_SENTINELS;

    if (!pid || !pMem)
    {
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"DDxHeap!EnumFindItem:  Invalid enum data, bailing ...\r\n"));
        return FALSE;
    }


    // TODO: Deal with ptrs in header or outside of requested alloc 
    // (but inside block aligned allocation).

    if ((pid->ptr > pMem) && (pid->ptr <= pMem + cbAlloc))
    {
        pid->pMem   = pMem;
        pid->cbSize = cbData;
        pid->dwFlags = dwFlags;

#if HEAP_SENTINELS
    
        RHEAP_SENTINEL_HEADER* phdr = (PRHEAP_SENTINEL_HEADER) pMem;

        pid->dwAllocSentinel1 = phdr->dwAllocSentinel1;
        pid->dwAllocSentinel2 = phdr->dwAllocSentinel2;

#else

        pid->dwAllocSentinel1 = 0;
        pid->dwAllocSentinel2 = 0;

#endif // HEAP_SENTINELS

        return FALSE;  // Stop looking
    }

    return TRUE;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL
FindItemInHeap (
                PRHEAP pHeap,
                DDX_HEAP_ITEM_INFO* phii,
                DWORD dwAddr
                )
{
    PRHRGN    prrgn  = NULL;
    PRHVAITEM pitem = NULL;
    DWORD     dwLocOffset; 
    DWORD     dwRemOffset;

    if (!pHeap || !phii)
        return FALSE;

    for (prrgn = &pHeap->rgn; prrgn; prrgn = prrgn->prgnNext) 
    {
        dwLocOffset = phii->ptr - prrgn->pLocalBase;
        dwRemOffset = phii->ptr - prrgn->pRemoteBase;
            
        if ((dwLocOffset < BLK2SIZE(prrgn->numBlkTotal)) ||
            (dwRemOffset < BLK2SIZE(prrgn->numBlkTotal))) 
        {
            // For now, walk the region from the beginning to find the item.
 
            if (!EnumerateRegionItems (prrgn, EnumFindItem, &phii))
            {
                return FALSE;
            }

            // TODO: Find the item by directly determining the sub-block and walking backwards
            //       until the alloc start is reached.  Should be big perf improvement for regions
            //       with a large number of individual allocs
        }
    }

    if (!prrgn) 
    {
        // enumerate through VirtualAlloc'd items
        for (pitem = pHeap->pvaList; pitem; pitem = pitem->pNext) 
        {
            /*if (dwAddr == ((!fIsRemotePtr) ? pitem->pLocalBase : pitem->pRemoteBase)) {
                break;
            }*/
        }
    }

    return TRUE;
}

#if 0

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DWORD
FindHeadSubBlock (
                  DWORD dwBitmap,
                  DWORD idxSubBlk
                  )
{
    for (DWORD idxBlk = idxSubBlk + 1; idxBlk; idxBlk--)
    {
        DWORD dwMask = RHF_STARTBLOCK << (2 * (BLOCKS_PER_BITMAP_DWORD - idxBlk));

        if ((dwBitmap & dwMask) 
        {
            return (idxBlk - 1);
        }
    }

    return 0xFFFFFFFF;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
LPBYTE
FindItemHead(
             PRHRGN prrgn,
             LPBYTE pAddr
             )
{

    DWORD idxAlloc = pAddr - prrgn->pLocalBase; // TODO: 
    //#define LOCALPTR(prrgn, idx)    ((prrgn)->pLocalBase  + BLK2SIZE(idx))

    DWORD idxBitmap     = IDX2BITMAPDWORD(idxAlloc);
    DWORD idxSubBlk     = IDX2BLKINBITMAPDWORD(idxAlloc);
    DWORD dwBitmap      = prrgn->allocMap[idxBitmap];
    DWORD idxHeadSubBlk = FindHeadSubBlock(dwBitmap, idxSubBlk);

    if (idxHeadSubBlk == 0xFFFFFFFF)
    {
        while (idxBitmap)
        {
            idxBitmap--;

            dwBitmap = prrgn->allocMap[idxBitmap];
            
            if (HasItemHead(dwBitmap))
                break;
        }
        
        idxHeadSubBlk = FindHeadSubBlock(dwBitmap, idxSubBlk);
        
        if (idxHeadSubBlk == 0xFFFFFFFF)
        {
            // TODO: Msg - not found
            return NULL;
        }
    }
    

    DWORD idxItem = (idxBitmap * BLOCKS_PER_BITMAP_DWORD) + idxHeadSubBlk; 
    
    return LOCALPTR(prrgn, idxItem);
}

#endif // 0

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL
IsHeapItem (
            DWORD dwAddr
            )
{
    BOOL fFound = FALSE;
    PPROCESS pProc = NULL;
    DDX_HEAP_ITEM_INFO hii;

    memset((void*)&hii, 0x0, sizeof(DDX_HEAP_ITEM_INFO));
    hii.ptr = (LPBYTE) dwAddr;


    // Validate address range

    // TODO PERF: Tighten this down to strict limits for heap ranges

    if (((dwAddr > VM_SHARED_HEAP_BASE) && (dwAddr < 0x80000000)) ||
        ((dwAddr > g_pKData->dwKVMStart) && (dwAddr < VM_CPU_SPECIFIC_BASE)))
    {
        pProc = g_pprcNK;
    }
    else if ((dwAddr > 0x00000000) && (dwAddr < VM_SHARED_HEAP_BASE))
    {
        pProc = pActvProc;
    }
    else
    {
        return FALSE;
    }


    // Get heap info from the coredll instance loaded by the owner process
    // If the process has coredll loaded.  Otherwise, no heaps (that we can detect).

    MODULEHEAPINFO* pModHeapInfo = GetModuleHeapInfoPtr(pProc);

    if (!pModHeapInfo)
    {
        return FALSE;
    }

    PRHEAP pProcessHeap = (PRHEAP) pModHeapInfo->hProcessHeap;
    PRHEAP phpListAll   = (PRHEAP) (pModHeapInfo->pphpListAll ? *(pModHeapInfo->pphpListAll) : NULL);


    
    // Walk the process heap looking for corrupt sentinels ...

    if (pProcessHeap)
    {
        if (FindItemInHeap(pProcessHeap, &hii, dwAddr))
        { 
            fFound = TRUE;
        }
    }


    // Now got through the user heaps (if any) ...

    PRHEAP pHeap = phpListAll;

     
    while (!fFound && pHeap && (pHeap != pProcessHeap))
    {
        if (FindItemInHeap(pHeap, &hii, dwAddr))
        {
            fFound = TRUE;
            break;
        }

        pHeap = pHeap->phpNext;
    }


    // Found it, log it

    if (fFound)
    {
        LPCWSTR pwszType = NULL;

        switch(hii.dwFlags)
        {
        case RHE_FREE:
            pwszType = L"FREE";
            break;
        case RHE_NORMAL_ALLOC:
            pwszType = L"Normal Alloc";
            break;
        case RHE_VIRTUAL_ALLOC:
            pwszType = L"Virtual Alloc";
            break;
        default:
            pwszType = L"unknown type";
            break;
        }



        ddxlog(L"     Heap Item (%s)\n", pwszType);
        ddxlog(L"       Addr of item:     0x%08x\n", hii.pMem + HEAP_SENTINELS);
        ddxlog(L"       Offset into item: 0x%08x\n", (LPBYTE) dwAddr - hii.pMem + HEAP_SENTINELS);
        ddxlog(L"       Item size:        %d\n",     hii.cbSize);

        if (HEAP_SENTINELS)
        {
            ddxlog(L"       Alloc Sentinel 1: 0x%08x\n", hii.dwAllocSentinel1);
            ddxlog(L"       Alloc Sentinel 1: 0x%08x\n", hii.dwAllocSentinel2);
        }
    }

    return fFound;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL
GetHeapData (
             PRHEAP pHeap,
             CEDUMP_HEAP_INFO* phi
             )
{
    PRHRGN prrgn    = NULL;
    PRHVAITEM pitem = NULL;

    if (!pHeap)
        return FALSE;

    phi->ModId  = pHeap->dwProcessId;

    for (prrgn = &pHeap->rgn; prrgn; prrgn = prrgn->prgnNext) 
    {
        phi->cbSize += BLK2SIZE(prrgn->numBlkTotal) - BLK2SIZE(prrgn->numBlkFree);
    }


    // enumerate through VirtualAlloc'd items
    for (pitem = pHeap->pvaList; pitem; pitem = pitem->pNext) 
    {
        phi->cbVaSize += pitem->cbSize;
    }

    return TRUE;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL
GetProcessHeapInfo (
                    /*[in]    */ PPROCESS pProc,
                    /*[out]   */ CEDUMP_HEAP_INFO* phi,
                    /*[in/out]*/ int* pindex
                    )
{
    MODULEHEAPINFO* pModHeapInfo = GetModuleHeapInfoPtr(pProc);

    if (!pModHeapInfo || !pindex)
    {
        return FALSE;
    }

    int i = *pindex;
    PRHEAP pProcessHeap = (PRHEAP) pModHeapInfo->hProcessHeap;

    
    // Walk the process heap looking for corrupt sentinels ...

    if (pProcessHeap)
    {
        phi[i].pProcess = (ULONG32) pProc;
        phi[i].ProcId   = pProc ? pProc->dwId : NULL;

        GetHeapData(pProcessHeap, &(phi[i]));
        i++;
    }


    // Now got through the user heaps (if any) ...

    PRHEAP pHeap = (PRHEAP) (pModHeapInfo->pphpListAll ? *(pModHeapInfo->pphpListAll) : NULL);
     
    while (pHeap && (pHeap != pProcessHeap))
    {
        phi[i].pProcess = (ULONG32) pProc;
        phi[i].ProcId   = pProc ? pProc->dwId : NULL;

        GetHeapData(pHeap, &(phi[i]));
        i++;

        pHeap = pHeap->phpNext;
    }

    *pindex = i;

    return TRUE;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int
CountHeaps (
            PPROCESS pProc
            )
{
    int nHeaps = 0;

    // Get heap info from the coredll instance loaded by the owner process
    // If the process has coredll loaded.  Otherwise, no heaps (that we can detect).

    MODULEHEAPINFO* pModHeapInfo = GetModuleHeapInfoPtr(pProc);

    if (pModHeapInfo)
    {
        // Process heap

        PRHEAP pProcessHeap = (PRHEAP) pModHeapInfo->hProcessHeap;

        if (pProcessHeap)
        {
            nHeaps++;
        }

        // Now got through the user heaps (if any) ...

        PRHEAP pHeap = (PRHEAP) (pModHeapInfo->pphpListAll ? *(pModHeapInfo->pphpListAll) : NULL);

        while (pHeap && (pHeap != pProcessHeap))
        {
            nHeaps++;
            pHeap = pHeap->phpNext;
        }
    }

    return nHeaps;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void 
TruncateHeapItem (
                  CEDUMP_HEAP_ITEM_DATA* pItemData
                  )
{
    if (pItemData)
    {
        if (pItemData->cbSize > DDX_MAX_HEAP_ITEM_SIZE)
        {
            int cbRemove = pItemData->cbSize - DDX_MAX_HEAP_ITEM_SIZE;

            pItemData->cbTruncSize = DDX_MAX_HEAP_ITEM_SIZE;
            pItemData->pBeginTruncateOffset = DDX_MIN_HEAP_ITEM_HEAD_SIZE;
            pItemData->pEndTruncateOffset = DDX_MIN_HEAP_ITEM_HEAD_SIZE + cbRemove;
        }
        else
        {
            pItemData->cbTruncSize = pItemData->cbSize;
            pItemData->pBeginTruncateOffset = 0;
            pItemData->pEndTruncateOffset = 0;
        }

        pItemData->Item.DataSize = pItemData->cbTruncSize;
    }
}



//-----------------------------------------------------------------------------
// Used to truncate around data in the middle of an item
//-----------------------------------------------------------------------------
void 
TruncateHeapItem2 (
                  CEDUMP_HEAP_ITEM_DATA* pItemData,
                  UINT cbCorruption
                  )
{
    if (pItemData)
    {
        if (cbCorruption > DDX_MAX_HEAP_ITEM_SIZE)
        {
            if (cbCorruption > pItemData->cbSize - DDX_CORRUPTION_ADDR_PADDING)
            {
                cbCorruption = pItemData->cbSize - DDX_CORRUPTION_ADDR_PADDING;
            }

            int cbRemove = cbCorruption - DDX_MAX_HEAP_ITEM_SIZE + DDX_CORRUPTION_ADDR_PADDING;

            pItemData->cbTruncSize = DDX_MAX_HEAP_ITEM_SIZE;
            pItemData->pBeginTruncateOffset = DDX_MAX_HEAP_ITEM_SIZE - (DDX_CORRUPTION_ADDR_PADDING * 2);
            pItemData->pEndTruncateOffset = DDX_MAX_HEAP_ITEM_SIZE - (DDX_CORRUPTION_ADDR_PADDING * 2) + cbRemove;
        }
        else
        {
            pItemData->cbTruncSize = pItemData->cbSize;
            pItemData->pBeginTruncateOffset = 0;
            pItemData->pEndTruncateOffset = 0;
        }

        pItemData->Item.DataSize = pItemData->cbTruncSize;
    }
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void
SignatureCorruptionDescription(
                               LPBYTE pItem,
                               DWORD dwSignature,
                               DWORD dwExpected
                               )
{
    ddxlog(L"\r\n");
    ddxlog(L"The signature of a heap item header appears to have been corruptted.\r\n");
    ddxlog(L"\r\n");
    ddxlog(L"Item address:        0x%08x\r\n", pItem);
    ddxlog(L"Signature:           0x%08x\r\n", dwSignature);
    ddxlog(L"Expected signature:  0x%08x\r\n", dwExpected);
    ddxlog(L"\r\n");
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void
SizeCorruptionDescription(
                          LPBYTE pItem,
                          DWORD dwSize,
                          DWORD dwExpected
                          )
{
    ddxlog(L"\r\n");
    ddxlog(L"The size field in a heap item header appears to have been corruptted.\r\n");
    ddxlog(L"\r\n");
    ddxlog(L"Item address:        0x%08x\r\n", pItem);
    ddxlog(L"Size in header:      0x%08x\r\n", dwSize);
    ddxlog(L"Expected size:       <= 0x%08x\r\n", dwExpected);
    ddxlog(L"\r\n");
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void
TailCorruptionDescription(
                          LPBYTE pItem, 
                          LPBYTE pAddr
                          )
{
    ddxlog(L"\r\n");
    ddxlog(L"The tail signature of a heap item appears to be corrupt.\r\n");
    ddxlog(L"This indicates a possible buffer over-run in the heap item.\r\n");
    ddxlog(L"\r\n");
    ddxlog(L"Item address:                 0x%08x\r\n",   pItem);
    ddxlog(L"Tail over-written at address: 0x%08x\r\n",   pAddr);
    ddxlog(L"\r\n");
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void
UseAfterFreeDescription(
                        LPBYTE pItem, 
                        LPBYTE pAddr
                        )
{
    ddxlog(L"\r\n");
    ddxlog(L"A heap item appears to have been used after being freed.\r\n");
    ddxlog(L"\r\n");
    ddxlog(L"Free item address:            0x%08x\r\n", pItem);
    ddxlog(L"Item over-written at address: 0x%08x\r\n", pAddr);
    ddxlog(L"\r\n");
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void
DoubleFreeDescription(
                      LPBYTE pItem,
                      LPBYTE pAddr
                      )
{
    ddxlog(L"\r\n");
    ddxlog(L"A heap item appears to have been freed twice.\r\n");
    ddxlog(L"\r\n");
    ddxlog(L"Item address: 0x%08x\r\n", pItem);
    ddxlog(L"\r\n");
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void
UnknownCorruptionDescription(
                             DWORD dwError,
                             LPBYTE pItem,
                             LPBYTE pAddr
                             )
{
    ddxlog(L"\r\n");
    ddxlog(L"An heap corruption of unknown type was detected.\r\n");
    ddxlog(L"\r\n");
    ddxlog(L"Item address:       0x%08x\r\n", pItem);
    ddxlog(L"Corruption address: 0x%08x\r\n", pAddr);
    ddxlog(L"Courruption type:   0x%08x\r\n", dwError);
    ddxlog(L"\r\n");
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DDxResult_e
GenerateCorruptionDiagnosis(
                            DWORD dwError
                            )
{
    UINT   index   = g_nDiagnoses;
    LPBYTE pItem   = (LPBYTE) DD_ExceptionState.exception->ExceptionInformation[1];
    LPBYTE pAddr   = (LPBYTE) DD_ExceptionState.exception->ExceptionInformation[2];
    UINT   cbAlloc = DD_ExceptionState.exception->ExceptionInformation[3];
    UINT   cbSize  = cbAlloc - HEAP_SENTINELS;
    RHEAP_SENTINEL_HEADER head = *(PRHEAP_SENTINEL_HEADER) pItem;
    RHEAP* pHeap   = NULL; // TODO:
	//PRHRGN prrgn   = NULL; 
    DWORD dwFlags  = 0;
    DWORD dwValue  = 0;
    DWORD dwExpect = 0;

    
        DBGRETAILMSG(1, (L"  DwDmpGen!Heap: DD_ExInfo[1] = 0x%08x\r\n", DD_ExceptionState.exception->ExceptionInformation[1]));
        DBGRETAILMSG(1, (L"  DwDmpGen!Heap: DD_ExInfo[2] = 0x%08x\r\n", DD_ExceptionState.exception->ExceptionInformation[2]));
        DBGRETAILMSG(1, (L"  DwDmpGen!Heap: DD_ExInfo[3] = 0x%08x\r\n", DD_ExceptionState.exception->ExceptionInformation[3]));

    BeginDDxLogging();

    if (g_nDiagnoses >= DDX_MAX_DIAGNOSES)
    {
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"DDxHeap!GenerateDiagnosis:  Diagnosis limit exceeded\r\n"));
        return DDxResult_Error;
    }
    
    g_nDiagnoses++;

    DWORD dwProcessId = pCurThread->pprcActv ? pCurThread->pprcActv->dwId : NULL;

    g_ceDmpDiagnoses[index].Type       = Type_HeapCorruption;
    g_ceDmpDiagnoses[index].SubType    = dwError; 
    g_ceDmpDiagnoses[index].Scope      = Scope_Application; // TODO:  Shared/system heap??
    g_ceDmpDiagnoses[index].Depth      = Depth_Cause;
    g_ceDmpDiagnoses[index].Severity   = Severity_Severe;
    g_ceDmpDiagnoses[index].Confidence = Confidence_Certain;
    g_ceDmpDiagnoses[index].pProcess   = (ULONG32) pCurThread->pprcActv;
    g_ceDmpDiagnoses[index].ProcessId  = dwProcessId;
    g_ceDmpDiagnoses[index].pThread    = (ULONG32) pCurThread;

   

    // Description

    switch (dwError)
    {
        case HEAP_ITEM_SIGNATURE_CORRUPTION:

            dwValue = head.dwSig;
            dwExpect = HEAP_SENTINEL_SIGNATURE; 
            SignatureCorruptionDescription(pItem, dwValue, dwExpect);
            dwFlags = RHE_NORMAL_ALLOC;
            break;

        case HEAP_ITEM_SIZE_CORRUPTION:

            dwValue = head.cbSize;
            dwExpect = cbSize;
            SizeCorruptionDescription(pItem, dwValue, dwExpect);
            dwFlags = RHE_NORMAL_ALLOC;
            break;

        case HEAP_ITEM_TAIL_CORRUPTION:

            TailCorruptionDescription(pItem, pAddr);
            dwFlags = RHE_NORMAL_ALLOC;
            break;

        case HEAP_FREE_ITEM_CORRUPTION:

            UseAfterFreeDescription(pItem, pAddr);
            dwFlags = RHE_FREE;
            break;

        case HEAP_ITEM_DOUBLE_FREE:

            DoubleFreeDescription(pItem, pAddr);
            dwFlags = RHE_FREE;
            break;

        default:
            
            UnknownCorruptionDescription(dwError, pItem, pAddr);
            break;
    }


    // Item data

    CEDUMP_HEAP_CORRUPTION_DATA* pddxData = (CEDUMP_HEAP_CORRUPTION_DATA*) DDxAlloc(sizeof(CEDUMP_HEAP_CORRUPTION_DATA), 0); 

    if (pddxData)
    {
        // TODO: Add corrupt item and previous item to raw memory list
        //       when we do dump file optimization

        pddxData->CorruptItem.ProcessId   = (ULONG32) dwProcessId;
        pddxData->CorruptItem.pHeap       = (ULONG32) pHeap;
        pddxData->CorruptItem.pMem        = (ULONG32) pItem;
        pddxData->CorruptItem.cbSize      = (ULONG32) cbAlloc;
        pddxData->CorruptItem.dwFlags     = (ULONG32) dwFlags;
        pddxData->CorruptItem.cbSentinels = HEAP_SENTINELS;

        // For double free we don't know the item size for sure, so just take the header

        if (dwError == HEAP_ITEM_DOUBLE_FREE)
        {
            pddxData->CorruptItem.cbSize  = HEAP_SENTINELS;
        }


        // Trim the memory to fit

        if (dwError == HEAP_FREE_ITEM_CORRUPTION)
        {
            TruncateHeapItem2(&pddxData->CorruptItem, (BYTE) (pAddr - pItem));
        }
        else
        {
            TruncateHeapItem(&pddxData->CorruptItem);
        }

        memset((void*)&pddxData->PreviousItem, 0x0, sizeof(CEDUMP_HEAP_ITEM_DATA));

        // Get previous item data, if applicable
        switch (dwError)
        {
            case HEAP_ITEM_SIGNATURE_CORRUPTION:
            case HEAP_ITEM_SIZE_CORRUPTION:
                {
                    //pddxData->PreviousItem.cbSize = cbAlloc;

                    //if (prrgn && FindPreviousItem(prrgn, pItem, pddxData->PreviousItem))
                    //{
                    //    DEBUGGERMSG(1, (L"heap!GenerateDiagnosis:  Found previous item = 0x%08x\r\n", pddxData->PreviousItem.pMem));

                    //    AddDDxMemBlock (pCurThread->pprcActv, (DWORD) pddxData->PreviousItem.pMem, sizeof(DWORD), VM_PAGE_SIZE);
                    //    TruncateHeapItem(&pddxData->PreviousItem);
                    //}
                    //else
                    //{
                    //    DEBUGGERMSG(1, (L"heap!GenerateDiagnosis:  Failed to find previous item!!!\r\n"));
                    //}
                }
                
                break;

            case HEAP_ITEM_TAIL_CORRUPTION:
            case HEAP_FREE_ITEM_CORRUPTION:
            case HEAP_ITEM_DOUBLE_FREE:
            default:
                break;

		}
    }
    else
    {
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"heap!GenerateDiagnosis:  DDxAlloc failed to alloc CEDUMP_HEAP_CORRUPTION_DATA!  Bailing ...\r\n"));
        return DDxResult_Error;
    }

    g_ceDmpDiagnoses[index].pDataTemp = (ULONG32) pddxData;
    g_ceDmpDiagnoses[index].Description.DataSize = EndDDxLogging();
    g_ceDmpDiagnoses[index].Data.DataSize = sizeof(CEDUMP_HEAP_CORRUPTION_DATA) 
                                          + pddxData->CorruptItem.cbTruncSize
                                          + pddxData->PreviousItem.cbTruncSize;

    
    // Bucket params

    HRESULT hr = GetBucketParameters(&g_ceDmpDDxBucketParameters, pCurThread, pCurThread->pprcActv);
    
    if (FAILED(hr))
        return DDxResult_Error;

    return DDxResult_Positive;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DDxResult_e
DiagnoseHeap (
             DWORD ExceptionCode
             )
{
    DDxResult_e res;
    
    DWORD dwError = DD_ExceptionState.exception->ExceptionInformation[0];

    switch (dwError)
    {
        case HEAP_ITEM_SIGNATURE_CORRUPTION:
        case HEAP_ITEM_SIZE_CORRUPTION:
        case HEAP_ITEM_TAIL_CORRUPTION:
        case HEAP_FREE_ITEM_CORRUPTION:
        case HEAP_ITEM_DOUBLE_FREE:

            res = GenerateCorruptionDiagnosis(dwError);
            break;

        default:
            
            DEBUGGERMSG(OXZONE_DIAGNOSE, (L"DDxHeap!DiagnoseHeap:  Unknown heap error %d.\r\n", dwError));
            res = DDxResult_Error;
            break;
    }

    return res; 
}
