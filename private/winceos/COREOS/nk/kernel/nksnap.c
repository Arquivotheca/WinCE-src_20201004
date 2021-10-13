#include "kernel.h"
#include "snapboot.h"
#include "pager.h"

typedef struct _SNAPINFO {
    // CPU independent fields
    DWORD   cpTotal;                // total page snapped
    DWORD   cpPageable;             // # of pageable snapshot pages
    DWORD   cbSnapSize;             // Actual size of 1st snap, round up to 4k alignment
    LPDWORD pSnapTable;
} SNAPINFO, *PSNAPINFO;

typedef const SNAPINFO *PCSNAPINFO;

extern const ROMHDR *pTOC;
extern MEMORYINFO   MemoryInfo;
extern CRITICAL_SECTION ModListcs, PhysCS;
extern PHDATA phdEvtDirtyPage;

void FPUFlushContext (void);

DWORD g_cLocked;
DWORD g_cAliased;

SNAPINFO g_snap;
DWORD g_cSnapPaged;

CRITICAL_SECTION SSPLcs;
PCSnapshotSupport g_pSnapSupport = NULL; 

#define SNAP_TAKEN      0
#define SNAP_FAILURE    1
#define SNAP_RESUME     2

// max snapshot size == 512MB - 8MB, as we're using the user address space below 1G to map
// both snap pages and compress buffer.
#define MAX_SNAPSHOT_SIZE       (0x20000000 - 0x800000)

TSnapState g_snapState = eSnapNone;

#define MB_4M                           (0x400000)

void InitPageList (PPHYSPAGELIST pUseMap, DWORD idxHead);

#define SNAP_MAPPING_BASE               MB_4M       // always map the pages from 4MB

FORCEINLINE LPDWORD FirstUnusedEntryInSnap (PSNAPINFO pSnap)
{
    return (LPDWORD) pSnap->pSnapTable + pSnap->cpTotal;
}

FORCEINLINE LPDWORD FirstEntryOfSnap (PSNAPINFO pSnap)
{
    return (LPDWORD) pSnap->pSnapTable;
}

FORCEINLINE BOOL Is4MBAligned (DWORD dwAddr)
{
    return (dwAddr & (MB_4M-1)) == 0;
}

//
// AddPagesToSnapshotTable - add a range of physical pages into the snapshot page table
//
static BOOL AddPagesToSnapshotTable (PSNAPINFO pSnap, DWORD dwPfnStart, DWORD cbSize, DWORD idxSnapLimit)
{
    LPDWORD pdwEntry = FirstUnusedEntryInSnap (pSnap);
    DWORD   idx;
    DWORD   cPages = PAGECOUNT (cbSize);
    DWORD   dwPFN  = dwPfnStart;
    BOOL    fRet   = (pSnap->cpTotal + cPages <= idxSnapLimit);

    DEBUGCHK (fRet);

    if (fRet) {
        DEBUGCHK (IsPageAligned (cbSize));

        if (!dwPFN || (INVALID_PHYSICAL_ADDRESS == dwPFN)) {
            ERRORMSG (1, (L"Invalid PFN detected\r\n"));
            DebugBreak ();
        }

        RETAILMSG (1, (L"Snapping pages from 0x%x of size 0x%x\r\n", dwPfnStart, cbSize));
        for (idx = 0; idx < cPages; idx ++) {
            pdwEntry[idx] = dwPFN;
            dwPFN = NextPFN (dwPFN);
            DEBUGMSG (ZONE_PAGING, (L"pdwEntry[%d] = 0x%x\r\n", idx, pdwEntry[idx]));
        }

        pSnap->cpTotal += cPages;

        RETAILMSG (1, (L"----- Total Pages snapped so far = 0x%x --------\r\n", pSnap->cpTotal));
    }
    return fRet;
}

//
// AddInUsePagesToSnapshotTable - add all in use pages of a memory region to the snapshot table
//
static BOOL AddInUsePagesToSnapshotTable (PSNAPINFO pSnap, PFREEINFO pfi, DWORD idxSnapLimit)
{
    DWORD   idx;
    DWORD   idxLimit = IDX_FROM_PFN (pfi, pfi->paEnd);
    LPDWORD pdwEntry = FirstUnusedEntryInSnap (pSnap);

    RETAILMSG (1, (L"Walking usemap, pfi = 0x%x, snapshot page table entry = 0x%x\r\n", pfi, pdwEntry));

    for (idx = IDX_PAGEBASE; (idx < idxLimit) && (pSnap->cpTotal < idxSnapLimit); idx ++) {
        if (IS_PAGE_INUSE (pfi->pUseMap, idx)) {
            DWORD dwPFN = PFN_FROM_IDX (pfi, idx);
            if (pfi->pUseMap[idx].refCnt > 1) {
                g_cAliased ++;
            }
            *pdwEntry = dwPFN;
            pdwEntry ++;
            pSnap->cpTotal ++;
        }
    }
    
    RETAILMSG (1, (L"----- Total Pages snapped so far = 0x%x --------\r\n", pSnap->cpTotal));
    return idx >= idxLimit;
}

#ifdef DEBUG
int DumpSnap (PSNAPINFO pSnap)
{
    DWORD idx;
    LPDWORD pdwEntry = FirstEntryOfSnap (pSnap);
    for (idx = 0; idx < pSnap->cpTotal; idx ++) {
        NKD (L"%d: 0x%x\r\n", idx, pdwEntry[idx]);
    }

    return 0;
}
#endif

//
// IsPageRefCountEqualOne - check if a particular page has a ref-count of 1.
//
static BOOL IsPageRefCountEqualOne (DWORD dwPFN)
{
    PFREEINFO pfi = GetRegionFromPFN (dwPFN);

    return pfi && (1 == pfi->pUseMap[IDX_FROM_PFN (pfi, dwPFN)].refCnt);
}

//
// MarkRangePageable - mark a range of address as pageable so they'll be moved to pageable snapshot area
//
static void MarkRangePageable (PPROCESS pprc, PSNAPINFO pSnap, DWORD dwStartAddr, DWORD dwEndAddr)
{
    LPDWORD pdwBase = FirstEntryOfSnap (pSnap);
    DWORD dwAddr = dwStartAddr;
    DWORD idxPD  = VA2PDIDX (dwAddr);
    DWORD idxPT  = VA2PT2ND (dwAddr);

    for ( ; dwAddr < dwEndAddr; idxPD = NextPDEntry (idxPD)) {
        PPAGETABLE pptbl = GetPageTable (pprc->ppdir, idxPD);
        if (pptbl) {
            for ( ; (idxPT < VM_NUM_PT_ENTRIES) && (dwAddr < dwEndAddr); idxPT ++, dwAddr += VM_PAGE_SIZE) {
                DWORD dwEntry = pptbl->pte[idxPT];
                if (IsPageMarkedPageableSnap (dwEntry)) {
                    // this is a page we marked to be moved to pageablesnapshot
                    DWORD dwPFN    = PFNfromEntry (dwEntry);
                    BOOL  fRefCnt1 = IsPageRefCountEqualOne (dwPFN);
                    
                    dwEntry |= PG_VALID_MASK;
                    pptbl->pte[idxPT] = dwEntry;
                    
                    if (fRefCnt1 && !VMIsPagesLocked (pprc, dwAddr, 1)) {
                        LONG idxLast = pSnap->cpTotal - pSnap->cpPageable - 1;
                        LONG idxSnap;

                        DEBUGCHK (idxLast);

                        // search the snap page table for the particular page, and
                        // (1) swap with the last non-pageable page
                        // (2) update page table entry to the corresponding index
                        for (idxSnap = idxLast; idxSnap >= 0; idxSnap --) {
                            if (pdwBase[idxSnap] == dwPFN) {
                                // swap the entry with the last non-pageable entry
                                pdwBase[idxSnap] = pdwBase[idxLast];
                                pdwBase[idxLast] = dwEntry;

                                // update page table entry to be [idx|PG_SNAPSHOT] so we know what to do to page in pages
                                pptbl->pte[idxPT] = MakeCommittedEntry (PA2PFN (idxLast << VM_PAGE_SHIFT), PG_SNAPSHOT);

                                // increment # of pageable pages
                                pSnap->cpPageable ++;
                                break;
                            }
                        }
                        DEBUGCHK (idxSnap);

                    } else if (fRefCnt1) {
                        g_cLocked ++;
                    }
                }
            }
        } else {
            DEBUGCHK (idxPD);
            dwAddr += MB_4M;

            // the address must be 4MB aligned
            DEBUGCHK (!(dwAddr & (MB_4M-1)));
        }
        idxPT = 0;
    }
}

//
// EnumMarkUserPages - enumeration function to mark all user-mode pages of a process snapshot-pageable
//
static BOOL EnumMarkUserPages (PDLIST pdl, PSNAPINFO pSnap)
{
    MarkRangePageable ((PPROCESS) pdl, pSnap, VM_USER_BASE, VM_SHARED_HEAP_BASE);
    return FALSE;   // keep enumerating
    
}

//
// EnumMarkKDLLPages - enumeration function to mark pages of a kernel mode DLL snapshot-pageable
//
static BOOL EnumMarkKDLLPages (PDLIST pdl, PSNAPINFO pSnap)
{
    PMODULE pMod  = (PMODULE) pdl;
    BOOL    fKDll = IsKModeAddr ((DWORD) pMod->BasePtr);

    if (fKDll && FullyPageAble (&pMod->oe)) {
        DEBUGCHK (!IsKernelVa (pMod->BasePtr));
        MarkRangePageable (g_pprcNK, pSnap, (DWORD) pMod->BasePtr, (DWORD) pMod->BasePtr + PAGEALIGN_UP (pMod->e32.e32_vsize));
    }
    return FALSE;
}

//
// GetOEMSnapRegions - get the OEM regions that need to be included in snapshot.
//
static PCRamTable GetOEMSnapRegions (void)
{
    static PCRamTable pOemRam;
    if (!pOemRam && g_pSnapSupport->pfnSnapGetOemRegions) {
        pOemRam = g_pSnapSupport->pfnSnapGetOemRegions ();
    }
    return pOemRam;
}

//
// ClearUserMaps - set the usemap entry for unused pages to 0 (so we get a better compression ratio).
//
static void ClearUseMaps (void)
{
    PFREEINFO pfi = MemoryInfo.pFi + MemoryInfo.cFi; 

    while (pfi -- > MemoryInfo.pFi) {
        if (pfi->NumFreePages) {
            DWORD idxLimit = IDX_FROM_PFN (pfi, pfi->paEnd);
            DWORD idx;
            for (idx = IDX_PAGEBASE; idx < idxLimit; idx ++) {
                if (!IS_PAGE_INUSE (pfi->pUseMap, idx)) {
                    PPHYSPAGELIST pPage = &pfi->pUseMap[idx];
                    pPage->idxBack = pPage->idxFwd = 0;
                }
            }
        }
    }
}

//
// RebuildFreeLists - re-build the "free list" of all the memory regions. This needs to be done
//                    when we boot from snapshot as memory need to be zero'd.
//
static void RebuildFreeLists (void)
{
    PFREEINFO pfi = MemoryInfo.pFi + MemoryInfo.cFi; 

    while (pfi -- > MemoryInfo.pFi) {
        if (pfi->NumFreePages) {
            DWORD idxLimit = IDX_FROM_PFN (pfi, pfi->paEnd);
            PPHYSPAGELIST pHead = &pfi->pUseMap[IDX_UNZEROED_PAGE_LIST];
            DWORD idx;

            // initialize all the page lists
            InitPageList (pfi->pUseMap, IDX_ZEROED_PAGE_LIST);
            InitPageList (pfi->pUseMap, IDX_UNZEROED_PAGE_LIST);
            InitPageList (pfi->pUseMap, IDX_DIRTY_PAGE_LIST);
            
            for (idx = IDX_PAGEBASE; idx < idxLimit; idx ++) {
                if (!IS_PAGE_INUSE (pfi->pUseMap, idx)) {
                    // add page to tail of the free list
                    PPHYSPAGELIST pPage = &pfi->pUseMap[idx];
                    WORD   idxBack = pHead->idxBack;
                    pPage->idxBack = idxBack;
                    pPage->idxFwd  = IDX_UNZEROED_PAGE_LIST;
                    pHead->idxBack = pfi->pUseMap[idxBack].idxFwd = (WORD) idx;
                }
            }
        }
    }
}

//
// CollectSnapshotPages - collect all the pages that need to be included in the snapshot
// 
static void CollectSnapshotPages (PSNAPINFO pSnap, DWORD idxSnapLimit)
{
    PCRamTable  pOemRam  = GetOEMSnapRegions ();
    DWORD       idx, dwPFN;

    // add static region used
    dwPFN = GetPFN ((LPCVOID) pTOC->ulRAMStart);
    VERIFY (AddPagesToSnapshotTable (pSnap, dwPFN, pTOC->ulRAMFree + MemForPT - pTOC->ulRAMStart, idxSnapLimit));

    // add OEM regions
    if (pOemRam) {
        for (idx = 0; idx < pOemRam->dwNumEntries; idx ++) {
            dwPFN = PFNfrom256 (pOemRam->pRamEntries[idx].ShiftedRamBase);
            VERIFY (AddPagesToSnapshotTable (pSnap, dwPFN, pOemRam->pRamEntries[idx].RamSize, idxSnapLimit));
        }
    }

    // add usemap
    for (idx = 0; idx < MemoryInfo.cFi; idx ++) {
        dwPFN = GetPFN (MemoryInfo.pFi[idx].pUseMap);
        VERIFY (AddPagesToSnapshotTable (pSnap, dwPFN, PFN2PA (MemoryInfo.pFi[idx].paStart - dwPFN), idxSnapLimit));
    }

    // parse use-map and add all inuse page
    for (idx = 0; idx < MemoryInfo.cFi; idx ++) {
        VERIFY (AddInUsePagesToSnapshotTable (pSnap, &MemoryInfo.pFi[idx], idxSnapLimit));
    }

    // memory snapshot fully taken when we reached here.

    if (g_pSnapSupport->pfnSnapRead) {
        // snapshot paging is supported
        // move all the pageable pages. These pages includes:
        // - all user mode pages that aren'tlocked and aliased.
        // - all shared-heap pages that aren't locked and alised.
        // - all r/w pages of pageable kernel mode DLLs that aren't locked and aliased.
        // mark the user mode page
        EnumerateDList (&g_pprcNK->prclist, EnumMarkUserPages, pSnap);

        // shared heap pages
        MarkRangePageable (g_pprcNK, pSnap, VM_SHARED_HEAP_BASE, VM_SHARED_HEAP_BOUND);

        // mark pageable kernel DLL
        EnumerateDList (&g_pprcNK->modList, EnumMarkKDLLPages, pSnap);

    }

    RETAILMSG (1, (L"Snapshot prepared (total = %d pages, snap1 = %d pages, locked = %d pages, aliased = %d pages\r\n", 
                pSnap->cpTotal, pSnap->cpTotal - pSnap->cpPageable, g_cLocked, g_cAliased));

    DEBUGMSG (ZONE_PAGING, (L"", DumpSnap (pSnap)));

}

//
// SearchAndMarkFreePage - search for unused pages in a region and mark it "temporary in use for buffer to take snapshot"
// 
static BOOL SearchAndMarkFreePage (PFREEINFO pfi, LPDWORD pdwIdx)
{
    DWORD idxLimit = IDX_FROM_PFN (pfi, pfi->paEnd);
    DWORD idx      = *pdwIdx;

    for ( ; idx < idxLimit; idx ++) {
        if (pfi->pUseMap[idx].idxFwd < MAX_PAGE_IDX) {
            // this page is free and not marked
            pfi->pUseMap[idx].idxFwd = MAX_PAGE_IDX;
            *pdwIdx = idx;
            break;
        }
    }
    return idx < idxLimit;
}

//
// GetUnusedPage - get an unuse page to use as buffer to take snapshot.
//
static DWORD GetUnusedPage (BOOL fStaticMapped)
{
    DWORD dwPFN = INVALID_PHYSICAL_ADDRESS;

    if (fStaticMapped || (1 == MemoryInfo.cFi)) {
        // search forward from region 0
        static PFREEINFO pfi = NULL;
        static DWORD     idx = IDX_PAGEBASE;

        if (!pfi) {
            pfi = MemoryInfo.pFi;
        }

        while (pfi < MemoryInfo.pFi + MemoryInfo.cFi) {
            if (SearchAndMarkFreePage (pfi, &idx)) {
                dwPFN = PFN_FROM_IDX (pfi, idx);
                idx ++;
                break;
            }
            pfi ++;
            idx = IDX_PAGEBASE;
        }
        
    } else {
        // search backward from last region
        static PFREEINFO pfi = NULL;
        static DWORD     idx = IDX_PAGEBASE;
        if (!pfi) {
            pfi = MemoryInfo.pFi + MemoryInfo.cFi - 1;
        }
        while (pfi >= MemoryInfo.pFi) {
            if (SearchAndMarkFreePage (pfi, &idx)) {
                dwPFN = PFN_FROM_IDX (pfi, idx);
                idx ++;
                break;
            }
            pfi --;
            idx = IDX_PAGEBASE;
        }
    }

    return dwPFN;
}

//
// GetUnusedPageForPageTable - get an unused page to use as temporary page table to map snapshot pages/buffers
//
static LPDWORD GetUnusedPageForPageTable (LPDWORD pdwPFN)
{
    *pdwPFN = GetUnusedPage (TRUE);

    return (INVALID_PHYSICAL_ADDRESS == *pdwPFN)? NULL : (LPDWORD) Pfn2Virt (*pdwPFN);
}

void SetupPDEntres (LPDWORD pde, DWORD dwPfn);

//
// SetupKernPDEntry - Setup kernel page directory entry to map to a page table.
// 
static void SetupKernPDEntry (DWORD dwAddr, DWORD dwPFN)
{
    DWORD idxPD = VA2PDIDX (dwAddr);
    DEBUGCHK (Is4MBAligned (dwAddr));
#if defined (ARM)
    SetupPDEntres (&g_ppdirNK->pte[idxPD], dwPFN);
#elif defined (x86)
    g_ppdirNK->pte[idxPD] = dwPFN | PG_KRN_READ_WRITE;
#else
    g_ppdirNK->pte[idxPD] = (DWORD) Pfn2Virt (dwPFN);
#endif
}

//
// ClearKernUserMappings - clear the lower 1G mapping of kernel page directory.
//
static void ClearKernUserMappings (void)
{
    DWORD idxPD = VA2PDIDX (SNAP_MAPPING_BASE);

    memset (&g_ppdirNK->pte[idxPD], 0, (VA2PDIDX(VM_DLL_BASE) - idxPD) * sizeof(DWORD));

#ifdef ARM
    // ARM page table access is special, need to flush cache to make sure our PTE update take effect
    OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_DISCARD|CACHE_SYNC_L2_DISCARD|CACHE_SYNC_FLUSH_TLB|CSF_CURR_CPU_ONLY);
#else
    OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_FLUSH_TLB|CSF_CURR_CPU_ONLY);
#endif
}

//
// SetupAddrRangeMapping - map a range of addresses to either map to pages provided (pdwSource != NULL), or 
//                  unused pages (pdwSource = NULL)
//
static LPDWORD SetupAddrRangeMapping (LPDWORD pdwAddr, DWORD cPages, LPDWORD pdwPT, LPDWORD pdwSource)
{
    DWORD dwAddr = *pdwAddr;

    while (cPages) {
        DWORD dwPFN;
        if (Is4MBAligned (dwAddr)) {
            DWORD dwPFN;
            pdwPT = GetUnusedPageForPageTable (&dwPFN);
            if (!pdwPT) {
                break;
            }
            SetupKernPDEntry (dwAddr, dwPFN);
            DEBUGMSG (ZONE_PAGING, (L"New Page Table at 0x%x, pfn = 0x%x\r\n", dwAddr, dwPFN));
        }
        PREFAST_DEBUGCHK (pdwPT);
        if (pdwSource) {
            dwPFN = PFNfromEntry (*pdwSource);
            pdwSource ++;
        } else {
            dwPFN = GetUnusedPage (FALSE);
            if (INVALID_PHYSICAL_ADDRESS == dwPFN) {
                break;
            }
        }
        
        *pdwPT ++ = MakeCommittedEntry (dwPFN, PG_KRN_READ_WRITE);
        cPages --;
        dwAddr += VM_PAGE_SIZE;
    }
    *pdwAddr = dwAddr;

    return pdwPT;
}

//
// SetupCompressBuffer - setup a compress buffer using unsed pages.
//
static BOOL SetupCompressBuffer (PSNAPINFO pSnap, LPDWORD pdwCompressBufferAddr, LPDWORD pdwCompressBufferSize)
{
    DWORD       dwAddr = SNAP_MAPPING_BASE;
    LPDWORD     pdwPT  = SetupAddrRangeMapping (&dwAddr, pSnap->cpTotal, NULL, g_snap.pSnapTable);
    
    // the only case for the above SetupAddrRangeMapping call to return NULL is when we can't get enough pages
    // for page tables. In this case, we cannot take the snapshot

    if (pdwPT) {
        *pdwCompressBufferAddr = dwAddr;

        // we'll try to get up to the size of the first snapshot for compression buffer. Even if we can't 
        // get enough pages, we can still try to compress the snap, hoping it'll fit.
        // In the case when it doesn't fit, we're fail to capture the snapshot.
        SetupAddrRangeMapping (&dwAddr, pSnap->cpTotal - pSnap->cpPageable, pdwPT, NULL);

        *pdwCompressBufferSize = dwAddr - *pdwCompressBufferAddr;

#ifdef ARM
        // ARM page table access is special, need to write-back all cache to make sure our PTE update take effect
        OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_DISCARD|CACHE_SYNC_L2_DISCARD|CSF_CURR_CPU_ONLY);
#endif

        memset ((LPVOID)*pdwCompressBufferAddr, 0, *pdwCompressBufferSize);
    }

    return NULL != pdwPT;

}

extern LONG g_lCntPageFault;

//
// funciton to resume when booted from snapshot
//
static BOOL BootFromSnap (void)
{
    g_lCntPageFault = 0;
    g_snapState = eSnapBoot;
    g_pSnapSupport->pfnSnapshotResume ();
    return TRUE;
}

// calculate total size of the snapshot
// (1) header    ==> 1 page (VM_PAGE_SIZE)
// (2) snaptable ==> PAGEALIGN_UP (pSnapHeader->cSnapPages * sizeof (DWORD))
// (3) snapdata  ==> PAGEALIGN_UP (pSnapHeader->cbCompressed)
FORCEINLINE DWORD CalcSnapSize (PSNAP_HEADER pSnapHeader)
{
    return VM_PAGE_SIZE                                                 // snap header
         + PAGEALIGN_UP (pSnapHeader->cSnapPages * sizeof (DWORD))      // snap page table
         + PAGEALIGN_UP (pSnapHeader->cbCompressed);                    // snap data

}

//
// heper function to take snapshot
// NOTE: the stack frame for this function can alter between the time we take the snapshot
//       and resuming from snapshot. It's very important that we return from this function
//       as soon as we resume from snapshot, as the content in the current frame cannot be
//       trusted when cold boot from snapshot.
//
__declspec (noinline)
void DoTakeSnapshot (PSNAPINFO pSnap, PSNAP_HEADER pSnapHeader, DWORD dwCompressBufAddr, DWORD cbCompressBufSize)
{
    INTERRUPTS_OFF ();
    if (g_pSnapSupport->pfnSnapCPU ((DWORD) BootFromSnap, pSnapHeader+1, VM_PAGE_SIZE - sizeof (SNAP_HEADER))) {
        // snapshot taken, or reboot from snap
        
        if (eSnapBoot != g_snapState) {

            // taking snapshot
            const DWORD cSnapPages     = pSnap->cpTotal - pSnap->cpPageable;
            const DWORD cbUncompressed = cSnapPages << VM_PAGE_SHIFT;
            const DWORD cbPageableSnap = pSnap->cpPageable << VM_PAGE_SHIFT;
            LPBYTE      pSnapshotData  = (LPBYTE) dwCompressBufAddr;

            // snap header only contain the needed information for 1st snap such that bootloader can decompress.
            // No need to record any page beyond that
            pSnapHeader->dwSig           = SSPL_SIG_HEADER;
            pSnapHeader->dwOfstSnapTable = VM_PAGE_SIZE;
            pSnapHeader->cSnapPages      = cSnapPages;
            pSnapHeader->cbCompressed    = cbUncompressed;  // default uncompressed
            pSnapHeader->cbPageable      = cbPageableSnap;
            pSnapHeader->dwOemData       = g_pSnapSupport->dwOemData;

            // compress the snapshot
            if (cbCompressBufSize) {

                DWORD cbCompressedSnapSize = VM_PAGE_SIZE;

                //
                // this is a do-while loop because the pSnap->cbSnapSize needs to be calculated based on 
                // the size *after* compression. As the compressed snap contains only the old value, we 
                // need to re-do the compression to get the correct size in the snapshot.
                //
                do {
                    pSnap->cbSnapSize = cbCompressedSnapSize;

                    pSnapHeader->cbCompressed = cbCompressBufSize;
                    if (!g_pSnapSupport->pfnCompress ((LPDWORD) SNAP_MAPPING_BASE, 
                                                        cbUncompressed,
                                                        (LPDWORD) pSnapshotData,
                                                        &pSnapHeader->cbCompressed)) {
                        ERRORMSG (1, (L"Unable to compress snapshot, try to create snapshot uncompressed\r\n"));
                        pSnapHeader->cbCompressed = cbUncompressed;
                        break;
                    }

                    // if the size difference is less than 64k, no point doing compression.
                    if (pSnapHeader->cbCompressed + VM_BLOCK_SIZE >= cbUncompressed) {
                        RETAILMSG (1, (L"Compressed size is almost the same as uncompressed size, create snapshot uncompressed\r\n"));
                        pSnapHeader->cbCompressed = cbUncompressed;
                        break;
                    }

                    // recalculate the size of the snap
                    cbCompressedSnapSize = CalcSnapSize (pSnapHeader);

                } while (cbCompressedSnapSize > pSnap->cbSnapSize);
            }

            if (cbUncompressed == pSnapHeader->cbCompressed) {
                // snap data into memory

                // update size of 1st snap
                pSnap->cbSnapSize = CalcSnapSize (pSnapHeader);

                // copy memory
                if (cbCompressBufSize >= cbUncompressed) {
                    memcpy (pSnapshotData, (LPVOID) SNAP_MAPPING_BASE, cbUncompressed);

                } else {
                    // insufficient memory to take a snapshot
                    ERRORMSG (1, (L"Insufficient memroy to take a shotshot\r\n"));
                    pSnap->cbSnapSize = 0;       // mark 1st snap size = 0 to indicate failure
                }
            }

            if (pSnap->cbSnapSize) {
                PFN_OEMWriteSnapshot pfnSnapWrite = g_pSnapSupport->pfnSnapWrite;
                const DWORD cbHeaderAndSnapPageTable = VM_PAGE_SIZE + PAGEALIGN_UP (pSnapHeader->cSnapPages * sizeof (DWORD));

                RETAILMSG (1, (L"Total Snapshot size, including pageable snap = 0x%x\r\n", pSnap->cbSnapSize + cbPageableSnap));

                // total size of storage needed == pSnap->cbSnapSize + cbPageableSnap
                if (g_pSnapSupport->pfnPrepareSnapshot (pSnap->cbSnapSize + cbPageableSnap)) {
                    pSnapHeader->dwOfstPageable = pSnap->cbSnapSize;
                    // calculate checksum
                    pSnapHeader->dwTableChkSum  = SnapChkSum (pSnap->pSnapTable, PAGEALIGN_UP (pSnapHeader->cSnapPages * sizeof (DWORD)));
                    pSnapHeader->dwSnapChkSum   = SnapChkSum (pSnapshotData, PAGEALIGN_UP (pSnapHeader->cbCompressed));
                    pSnapHeader->dwPageableChkSum = SnapChkSum ((LPVOID) (SNAP_MAPPING_BASE + (cSnapPages << VM_PAGE_SHIFT)), cbPageableSnap);

                    // header checksum is special. As it's part of the checksum, we use the negative value
                    // such that the checksum result, including the checksum itself, is always 0.
                    pSnapHeader->dwHeaderAdjust = 0 - SnapChkSum (pSnapHeader, VM_PAGE_SIZE);
                    // NO CHANGE TO pSnapHeader after this line.

                    if (SnapChkSum (pSnapHeader, VM_PAGE_SIZE) != 0) {
                        ERRORMSG (1, (L"!!!Checksum Calculation is wrong!!!\r\n"));
                    }

                    // ready to write the snapshot

                    DEBUGCHK (cbHeaderAndSnapPageTable + PAGEALIGN_UP (pSnapHeader->cbCompressed) <= pSnap->cbSnapSize);

                    // write header and snap page table
                    if (!pfnSnapWrite (pSnapHeader, cbHeaderAndSnapPageTable, 0)) {
                        ERRORMSG (1, (L"Failed to write the snap header and snap table\r\n"));
                        
                    // write compressed data in 1st snap
                    } else if (!pfnSnapWrite (pSnapshotData, PAGEALIGN_UP (pSnapHeader->cbCompressed), cbHeaderAndSnapPageTable)) {
                        ERRORMSG (1, (L"Failed to write the compressed snap data\r\n"));

                    // write pageable snap
                    } else if (pSnap->cpPageable
                               && !pfnSnapWrite ((LPVOID) (SNAP_MAPPING_BASE + (cSnapPages << VM_PAGE_SHIFT)),
                                             pSnap->cpPageable << VM_PAGE_SHIFT,
                                             pSnap->cbSnapSize)) {
                        ERRORMSG (1, (L"Failed to write pageable snapshot\r\n"));
                    } else if (!pfnSnapWrite (NULL, 0, 0)) {
                        ERRORMSG (1, (L"Failed to close the snapshot\r\n"));
                    } else {
                        RETAILMSG (1, (L"Finish taking snapshot\r\n"));
                        g_snapState = eSnapTaken;
                    }

                }
                
            }
        }
    }
}

//
// KC_TakeSnapShot: Non-preemptible part of snapshot taking
//
void KC_TakeSnapShot (LPBYTE pSnapBuffer, DWORD cbSnapBuffer)
{
    PSNAP_HEADER pSnapHeader = (PSNAP_HEADER) pSnapBuffer;
    DWORD dwCompressBufSize  = 0;
    DWORD cbCompressBufAddr;

    // NK.exe must be the current VM and active process, as we'll be using the user mode VM of NK to map/access the snapshot.
    DEBUGCHK (g_pprcNK == pActvProc);
    DEBUGCHK (g_pprcNK == pVMProc);

    // flush FPU content so we don't need to restore FPU context on snapshot boot
    FPUFlushContext ();

    g_snap.pSnapTable  = (LPDWORD) (pSnapBuffer + VM_PAGE_SIZE);

    // create the snapshot
    CollectSnapshotPages (&g_snap, (cbSnapBuffer-VM_PAGE_SIZE) / sizeof(DWORD));

    // at this point we might have changed the page table mapping to page from snapshot. Update
    // snap state to "snap prepared".
    g_snapState = eSnapPrepared;

    // setup compression buffer
    if (SetupCompressBuffer (&g_snap, &cbCompressBufAddr, &dwCompressBufSize)) {

        RETAILMSG (1, (L"Compression buffer at 0x%x of size 0x%x\r\n", cbCompressBufAddr, dwCompressBufSize));

        // clear uses maps for better compression result. We need to reconstruct it on snapshot boot anyway.
        ClearUseMaps ();

        // take the snapshot
        DoTakeSnapshot (&g_snap, pSnapHeader, cbCompressBufAddr, dwCompressBufSize);

    }

    // clear user mapping
    ClearKernUserMappings ();
    RebuildFreeLists ();

    // flush cache
    OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_DISCARD);

}

//
// SnapReleaseUnusedMemory - release the memory we allocated for taking snapshot which is no longer needed
//
static LPVOID SnapReleaseUnusedMemory (LPVOID pSnapBuffer, DWORD cbSnapBuffer)
{
    if (g_snap.cpPageable) {
        DWORD cbUsed = PAGEALIGN_UP (g_snap.cpPageable * sizeof (DWORD));

        memmove (pSnapBuffer, &g_snap.pSnapTable[g_snap.cpTotal - g_snap.cpPageable], cbUsed);
        g_snap.pSnapTable  = (LPDWORD) pSnapBuffer;

        if (cbSnapBuffer > cbUsed) {
            RETAILMSG (1, (L"SnapReleaseUnusedMemory: Decommiting unused memory 0x%x of size 0x%x\r\n", (LPBYTE)pSnapBuffer + cbUsed, cbSnapBuffer - cbUsed));
            VMDecommit (g_pprcNK, (LPBYTE)pSnapBuffer + cbUsed, cbSnapBuffer - cbUsed, VM_PAGER_NONE, NULL);
        }
    } else {
        RETAILMSG (1, (L"SnapReleaseUnusedMemory: Release unused memory unused memory 0x%x of size 0x%x\r\n", pSnapBuffer, cbSnapBuffer));
        VMFreeAndRelease (g_pprcNK, pSnapBuffer, cbSnapBuffer);
        pSnapBuffer = NULL;
    }
    return pSnapBuffer;
}

// 
// CountSnapPages - count the total number pages to be included in snapshot
//
static DWORD CountSnapPages (void)
{
    // cPages initialized to the number of pages in static area
    DWORD      cPages  = PAGECOUNT (pTOC->ulRAMFree + MemForPT - pTOC->ulRAMStart);

    // get the OEM snap region
    PCRamTable pOemRam = GetOEMSnapRegions ();
    DWORD      idx;

    // OEMArea
    if (pOemRam) {
        for (idx = 0; idx < pOemRam->dwNumEntries; idx ++) {
            cPages += PAGECOUNT (pOemRam->pRamEntries[idx].RamSize);
        }
    }

    // usemap
    for (idx = 0; idx < MemoryInfo.cFi; idx ++) {
        PFREEINFO pfi = MemoryInfo.pFi + idx;
        // add the usemap itself
        // PA256 == (pa) >> 8, so (PA256 >> 4) == # of pages 
        cPages += (PA256FromPfn (pfi->paStart - GetPFN (pfi->pUseMap)) >> 4);

        // add the pages in this region
        cPages += (PA256FromPfn (pfi->paEnd - pfi->paStart) >> 4) - pfi->NumFreePages;
    }

    RETAILMSG (1, (L"Total Number of Page in Snapshot = %d pages\r\n", cPages));
    return cPages;
}

//
// ReadSnap - read pages from snapshot.
//
static DWORD ReadSnap (LPVOID pPagingMem, DWORD cbSize, DWORD idxPage)
{
    DWORD dwPageResult = PAGEIN_SUCCESS;
    
    DEBUGCHK (eSnapNone != g_snapState);

    if (eSnapBoot == g_snapState) {
        // call Snap Paging function only when booting from snapshot
        if (g_pSnapSupport->pfnSnapRead (pPagingMem, cbSize, g_snap.cbSnapSize + (idxPage << VM_PAGE_SHIFT))) {
            // flush d-cache, as the page can be used for exeutable pages and we'll have a problem if we have a separate I/D cache.
            NKCacheRangeFlush (pPagingMem, VM_PAGE_SIZE, CACHE_SYNC_WRITEBACK);
        } else {
            ERRORMSG (1, (L"pfnSnapRead failed, unable to page in page, idxPage = 0x%x\r\n", idxPage));
            dwPageResult = PAGEIN_FAILURE;
        }
    }
    
    return dwPageResult;
}

#define ENTRY2IDX(dwEntry)      ((PFN2PA(dwEntry) >> VM_PAGE_SHIFT)- (g_snap.cpTotal - g_snap.cpPageable))

//
// SnapshotChangeProtection - change protection of a snapshot pageable page
//
BOOL SnapshotChangeProtection (DWORD dwEntry, DWORD dwPgProt)
{
    DWORD idxPage = ENTRY2IDX (dwEntry);

    if (idxPage < g_snap.cpPageable) {
        LPDWORD pdwBase = FirstEntryOfSnap (&g_snap);
        pdwBase[idxPage] = (pdwBase[idxPage] & ~PG_PERMISSION_MASK) | dwPgProt;
    }

    return (idxPage < g_snap.cpPageable);
}

//
// SnapshotGetProtection - get the protection of a snapshot pageable page
//
DWORD SnapshotGetProtection (DWORD dwEntry)
{
    DWORD idxPage  = ENTRY2IDX (dwEntry);

    return (idxPage < g_snap.cpPageable)
         ? (FirstEntryOfSnap (&g_snap)[idxPage] & PG_PERMISSION_MASK)
         : 0;
}

//
// PageFromSnapShot - page fault handler to demand page from snapshot.
//
DWORD PageFromSnapShot (PPROCESS pprc, DWORD dwAddr, BOOL fWrite)
{
    DWORD       dwPageResult = PAGEIN_FAILURE;
    PPAGETABLE  pptbl;
    
    DEBUGMSG (ZONE_PAGING, (L"Paging from Snapshot: proc-id = 0x%x dwAddr = 0x%x, fWrite = %d\r\n", pprc->dwId, dwAddr, fWrite));
    DEBUGCHK (IsPageAligned (dwAddr));

    EnterCriticalSection (&SSPLcs);
    LockVM (pprc);

    pptbl = GetPageTable (pprc->ppdir, VA2PDIDX (dwAddr));
    DEBUGCHK (pptbl);

    // although VMProcessPageFault already examine the PTE of this address, it's possible that we go preempted 
    // and the PTE got altered. We need to make sure everything is consistent after grabing VM lock.
    if (pptbl) {
        DWORD idxPT   = VA2PT2ND (dwAddr);
        DWORD dwEntry = pptbl->pte[idxPT];
        DWORD idxPage = ENTRY2IDX (dwEntry);

        if (IsPageCommitted (dwEntry) && IsSnapshotPage (dwEntry) && (idxPage < g_snap.cpPageable)) {
            // still a snapshot page

            // create an alias to the physical memory
            LPBYTE pPagingVM = (LPBYTE) ReserveTempVM ();

            if (pPagingVM) {
                DWORD       dwSnapEntry = FirstEntryOfSnap (&g_snap)[idxPage];
                DWORD       dwPFN       = PFNfromEntry (dwSnapEntry);
                LPVOID      pPagingMem  = pPagingVM + (dwAddr & VM_BLOCK_OFST_MASK);
                PPROCESS    pprcActv;
                
                // NOTE: we need to increment the ref-count of the page as we need to release VM lock while reading snapshot.
                //       It's possible other thread preempt us and free the memory if we don't inccrement the ref-count.
                VERIFY (VMCopyPhysical (g_pprcNK, (DWORD) pPagingMem, dwPFN, 1, PG_KRN_READ_WRITE, TRUE));

                // release VM lock before calling out to OEM
                UnlockVM (pprc);

                // switch active process to NK before calling out
                pprcActv = SwitchActiveProcess (g_pprcNK);
                dwPageResult = ReadSnap (pPagingMem, VM_PAGE_SIZE, idxPage);
                SwitchActiveProcess (pprcActv);

                // remove the temp mapping we created for paging.
                VERIFY (VMDecommit (g_pprcNK, pPagingMem, VM_PAGE_SIZE, VM_PAGER_NONE, NULL));

                // free the temp VM
                FreeTempVM (pPagingVM);

                LockVM (pprc);

                if (pptbl->pte[idxPT] == dwEntry) {
                    // the page hasn't altered while we're reading from snapshot
                    if (PAGEIN_SUCCESS == dwPageResult) {

                        pptbl->pte[idxPT] = dwSnapEntry;
#ifdef ARM
                        ARMPteUpdateBarrier (&pptbl->pte[idxPT], 1);
#endif
                        InvalidatePages (pprc, dwAddr, 1);
                        g_cSnapPaged ++;
                    }
                } else {
                    // someone changed the PTE while we get a chance to page it in. just return PAGEIN_SUCCESS.
                    // it'll page-fault again if the page become invalid.
                    dwPageResult = PAGEIN_SUCCESS;
                }
                
            }
        } else {
            // someone changed the PTE while we get a chance to page it in. just return PAGEIN_SUCCESS.
            // it'll page-fault again if the page become invalid.
            dwPageResult = PAGEIN_SUCCESS;
        }
    }

    UnlockVM (pprc);
    LeaveCriticalSection (&SSPLcs);
    DEBUGMSG (ZONE_PAGING, (L"Paging from Snapshot: result = %d\r\n", dwPageResult));

    return dwPageResult;
}

extern void DumpDwords (const DWORD * pdw, int len);

int DumpSnapHeader (PCSNAP_HEADER pSnapHeader)
{
    NKLOG (pSnapHeader->dwSig);
    NKLOG (pSnapHeader->dwOfstSnapTable);
    NKLOG (pSnapHeader->dwOfstPageable);
    NKLOG (pSnapHeader->cSnapPages);
    NKLOG (pSnapHeader->cbCompressed);
    NKLOG (pSnapHeader->cbPageable);
    NKLOG (pSnapHeader->dwHeaderAdjust);
    NKLOG (pSnapHeader->dwTableChkSum);
    NKLOG (pSnapHeader->dwSnapChkSum);
    NKLOG (pSnapHeader->dwPageableChkSum);

#ifdef DEBUG
    // dump the rest of the page in dwords
    DumpDwords ((const DWORD *) ((DWORD) (pSnapHeader+1) & ~0xf), (VM_PAGE_SIZE - sizeof (SNAP_HEADER))/sizeof (DWORD));
#endif

    return 0;
}

//
// VerifyChkSum - verify the checksum of an snapshot area by reading page-by-page from the snapshot we just wrote
//
BOOL VerifyChkSum (LPVOID pBuf, DWORD dwOfst, DWORD cPages, DWORD dwChkSumToVerify)
{
    DWORD dwChkSum = 0;

    while (cPages > 0) {
        if (!g_pSnapSupport->pfnSnapRead (pBuf, VM_PAGE_SIZE, dwOfst)) {
            break;
        }
        dwChkSum += SnapChkSum (pBuf, VM_PAGE_SIZE);
        cPages --;
        dwOfst += VM_PAGE_SIZE;
    }
    return !cPages && (dwChkSum == dwChkSumToVerify);
}

//
// VerifySnap - very that the snapshot we just wrote is what we actually wrote.
// 
BOOL VerifySnap (void)
{
    // can't verify if read function is not available.
    BOOL fRet = !g_pSnapSupport->pfnSnapRead;

    if (!fRet) {
        LPBYTE pBuf = VMAlloc (g_pprcNK, NULL, VM_PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);

        if (!pBuf) {
            ERRORMSG (1,(L"Out of Memory trying to very snapshot\r\n"));
        } else if (g_pSnapSupport->pfnSnapRead (pBuf, VM_PAGE_SIZE, 0)) {
            
            SNAP_HEADER snapheader = *(PSNAP_HEADER) pBuf;

            DumpSnapHeader ((PCSNAP_HEADER) pBuf);

            if (SnapChkSum (pBuf, VM_PAGE_SIZE) != 0) {
                ERRORMSG (1, (L"Checksum failure\r\n"));
            } else {
                DWORD dwOfst = VM_PAGE_SIZE;
                DWORD cPages = PAGECOUNT (snapheader.cSnapPages * sizeof (DWORD));
                if (VerifyChkSum (pBuf, dwOfst, cPages, snapheader.dwTableChkSum)) {
                    dwOfst += (cPages << VM_PAGE_SHIFT);
                    cPages = PAGECOUNT (snapheader.cbCompressed);
                    if (VerifyChkSum (pBuf, dwOfst, cPages, snapheader.dwSnapChkSum)) {
                        dwOfst += (cPages << VM_PAGE_SHIFT);

                        if (g_snap.cbSnapSize >= dwOfst) {
                            fRet = VerifyChkSum (pBuf, g_snap.cbSnapSize, PAGECOUNT (snapheader.cbPageable), snapheader.dwPageableChkSum);
                        }
                    }
                }
            }
        }

        if (pBuf) {
            VMFreeAndRelease (g_pprcNK, pBuf, VM_PAGE_SIZE);
        }
    }
    return fRet;
}



//
// MarkRangeInvalid - mark a range of address invalid
//
static void MarkRangeInvalid (PPROCESS pprc, DWORD dwStartAddr, DWORD dwEndAddr)
{
    DWORD dwAddr = dwStartAddr;
    DWORD idxPD  = VA2PDIDX (dwAddr);
    DWORD idxPT  = VA2PT2ND (dwAddr);

    for ( ; dwAddr < dwEndAddr; idxPD = NextPDEntry (idxPD)) {
        PPAGETABLE pptbl = GetPageTable (pprc->ppdir, idxPD);
        if (pptbl) {
            for ( ; (idxPT < VM_NUM_PT_ENTRIES) && (dwAddr < dwEndAddr); idxPT ++, dwAddr += VM_PAGE_SIZE) {
                DWORD dwEntry = pptbl->pte[idxPT];
                if (IsPageReadable (dwEntry)
                    && !VMIsPagesLocked (pprc, dwAddr, 1)
                    && IsPageRefCountEqualOne (PFNfromEntry (dwEntry))) {

                    pptbl->pte[idxPT] &= ~PG_VALID_MASK;
#ifdef ARM
                    pptbl->pte[idxPT] &= ~PG_V6_L2_NO_EXECUTE;
                    ARMPteUpdateBarrier (&pptbl->pte[idxPT], 1);
#endif
                }
            }
        } else {
            DEBUGCHK (idxPD);
            dwAddr += MB_4M;

            // the address must be 4MB aligned
            DEBUGCHK (!(dwAddr & (MB_4M-1)));
        }
        idxPT = 0;
    }
    // flush TLB to make sure we'll page-fault when the pages are accessed.
    OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_FLUSH_TLB);
    
}

//
// EnumMarkUserPagesInvalid - enumeration function to mark all user-mode pages of a process invalid
//
static BOOL EnumMarkUserPagesInvalid (PDLIST pdl, LPVOID pParam)
{
    PPROCESS pprc = (PPROCESS) pdl;
    LockVM (pprc);
    MarkRangeInvalid (pprc, VM_USER_BASE, VM_SHARED_HEAP_BASE);
    UnlockVM (pprc);
    return FALSE;   // keep enumerating
    
}

//
// EnumMarkKDLLPagesInvalid - enumeration function to mark pages of a kernel mode DLL snapshot-pageable
//
static BOOL EnumMarkKDLLPagesInvalid (PDLIST pdl, LPVOID pParam)
{
    PMODULE pMod  = (PMODULE) pdl;
    BOOL    fKDll = IsKModeAddr ((DWORD) pMod->BasePtr);

    if (fKDll && FullyPageAble (&pMod->oe)) {
        DEBUGCHK (!IsKernelVa (pMod->BasePtr));
        MarkRangeInvalid (g_pprcNK,(DWORD) pMod->BasePtr, (DWORD) pMod->BasePtr + PAGEALIGN_UP (pMod->e32.e32_vsize));
    }
    return FALSE;
}


BOOL NKPrepareForSnapshot (void)
{
    g_pSnapSupport = g_pOemGlobal->pSnapshotSupport;

    if (g_pSnapSupport->pfnSnapRead) {
        LockModuleList ();
        // mark the user mode page
        EnumerateDList (&g_pprcNK->prclist, EnumMarkUserPagesInvalid, NULL);

        LockVM (g_pprcNK);    
        // shared heap pages
        MarkRangeInvalid (g_pprcNK, VM_SHARED_HEAP_BASE, VM_SHARED_HEAP_BOUND);

        // mark pageable kernel DLL
        EnumerateDList (&g_pprcNK->modList, EnumMarkKDLLPagesInvalid, NULL);
        
        UnlockVM (g_pprcNK);    
        UnlockModuleList ();
    }

    g_snapState = eSnapPass2;
    return TRUE;
}

//------------------------------------------------------------------------------
// NKMakeSnapshot - exported function to create a snapshot
//------------------------------------------------------------------------------
BOOL NKMakeSnapshot (void)
{
    LPVOID      pSnapBuffer = NULL;
    DWORD       cPagesSnapshot;

    RETAILMSG (1, (TEXT("Start taking system snapshot:\r\n"))); 

    DEBUGCHK (g_pOemGlobal->pSnapshotSupport);
    DEBUGCHK (eSnapPass2 == g_snapState);

    // Acquire all CSes required to change memory state and process list
    // so that we don't need to worry about walking a list that is changing.
    LockModuleList ();
    EnterCriticalSection (&g_pprcNK->csVM);
    EnterCriticalSection (&PhysCS);

    // SSPLcs is only used for snapshot paging. Initialize it here
    InitializeCriticalSection (&SSPLcs);

    // calculate the # of pages in the snapshot. 
    cPagesSnapshot = CountSnapPages ();

    if ((cPagesSnapshot << VM_PAGE_SHIFT) > MAX_SNAPSHOT_SIZE) {
        ERRORMSG (1, (L"Snapshot size too big (0x%x), unable to take the snapshot\r\n", cPagesSnapshot << VM_PAGE_SHIFT));
    } else {
        PPROCESS    pprcVM      = SwitchVM (g_pprcNK);
    
        // add the number of pages we will allocate for the snapshot table
        cPagesSnapshot += PAGECOUNT (cPagesSnapshot * sizeof (DWORD));

        // convert to # of pages needed. Add 1 to make sure that we don't overflow in boundary case
        // +1 for the header and +1 for the page table entries for the new pages we allocated
        cPagesSnapshot = PAGECOUNT (cPagesSnapshot * sizeof (DWORD)) + 2;
        
        RETAILMSG (1, (L"Total Pages allocated for snapshot table = %d\r\n", cPagesSnapshot));
        
        // allocate the snaptable.
        pSnapBuffer = DoVMAlloc (g_pprcNK, 0, cPagesSnapshot << VM_PAGE_SHIFT, MEM_COMMIT, PAGE_READWRITE, PM_PT_ZEROED, NULL);

        if (pSnapBuffer) {
            // take the snapshot
            KCall ((PKFN) KC_TakeSnapShot, pSnapBuffer, cPagesSnapshot << VM_PAGE_SHIFT);

            // release unused snap buffer    
            pSnapBuffer  = SnapReleaseUnusedMemory (pSnapBuffer, cPagesSnapshot << VM_PAGE_SHIFT);
        }
        SwitchVM (pprcVM);
    }

    // release the locks.    
    LeaveCriticalSection (&PhysCS);
    LeaveCriticalSection (&g_pprcNK->csVM);
    UnlockModuleList ();

    // need to clean the dirty pages as the memory pool is reconstructed
    FastSignalEvent (GetEventPtr (phdEvtDirtyPage));
    
    switch (g_snapState) {
    case eSnapNone:
    case eSnapPass1:
    case eSnapPass2:
    case eSnapPrepared:
        RETAILMSG(1, (TEXT("Failed to take Snapshot, either OOM or failed to write to flash\r\n")));
        break;
    case eSnapTaken:
        RETAILMSG(1, (TEXT("Finished taking Snapshot\r\n")));
        break;
    case eSnapBoot:
        RETAILMSG(1, (TEXT("Booting from snapshot\r\n")));
        break;
    default:
        DEBUGCHK (0);
        break;
    }

    // return success if we finished taking snapshot or reboot from snapshot
    return (eSnapBoot == g_snapState) || (eSnapTaken == g_snapState);

}



