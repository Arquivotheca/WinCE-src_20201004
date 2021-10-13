//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include "kernel.h"

#define LOG(x)  NKDbgPrintfW(L"%s == %8.8lx\n", TEXT(#x), x)

int FirstFreeSection = RESERVED_SECTIONS+1;

MEMBLOCK *PmbDecommitting;
extern CRITICAL_SECTION VAcs;
extern HANDLE GwesOOMEvent;
extern long GwesCriticalThreshold;
extern PFREEINFO GetRegionFromAddr(PHYSICAL_ADDRESS addr);
extern LPVOID PhysPageToZero (PHYSICAL_ADDRESS paPFN);
extern void ZeroAndLinkPhysPage (LPVOID pPage);

const DWORD AllocationType[MB_FLAG_PAGER_TYPE+1] = {MEM_PRIVATE, MEM_IMAGE, MEM_MAPPED};

BOOL (* pfnOEMSetMemoryAttributes) (LPVOID pVirtualAddr, LPVOID pPhysAddr, DWORD cbSize, DWORD dwAttributes);


static BOOL FakedOEMIsROM (LPVOID pvAddr, DWORD cbSize)
{
    return FALSE;
}

BOOL (*pfnOEMIsRom) (LPVOID pvAddr, DWORD cbSize) = FakedOEMIsROM;


static LPVOID EntryToKVA (DWORD dwEntry)
{
    LPVOID pRet = NULL;
    if (dwEntry & PG_VALID_MASK) {
    
    // check for VirtualCopy'd entries on MIPS and SHx seperately
#ifdef MIPS
        if ((dwEntry & ~PG_PERMISSION_MASK) >= (DWORD)(0x20000000 >> PFN_SHIFT)) {
            return NULL;
        }
#elif defined (SHx)
        if (dwEntry >= 0x20000000) {
            return NULL;
        }
#endif
        pRet = Phys2Virt (PFNfromEntry (dwEntry));

        if (pRet && ((dwEntry & PG_CACHE_MASK) == PG_NOCACHE))
            pRet = (LPVOID) ((DWORD) pRet | 0x20000000);
    }

    return pRet;
}


PVOID VerifyAccess (LPVOID pvAddr, DWORD dwFlags, ACCESSKEY aky)
{
    DWORD dwAddr = (DWORD) pvAddr;
    extern LPVOID MDValidateKVA (DWORD);

    if (!(dwFlags & VERIFY_KERNEL_OK)                       // "kernel ok" flag not set
        && (GetThreadMode(pCurThread) != KERNEL_MODE)       // not in kernel mode
        && ((dwAddr & 0x80000000)                           // kmode only address
//#if defined (x86) || defined (ARM)
            || (IsInSharedSection (dwAddr) && (dwFlags & VERIFY_WRITE_FLAG))    // write to shared section
//#endif
        )) {

        return NULL;
    }

    if (IsKernelVa (dwAddr)) {
        pvAddr = MDValidateKVA (dwAddr);
    } else {
        PSECTION pscn = (dwAddr & 0x80000000)? &NKSection : SectionTable[dwAddr>>VA_SECTION];
        MEMBLOCK *pmb;
        ulong entry;

        if (pscn
            && (pmb = (*pscn)[(dwAddr>>VA_BLOCK)&BLOCK_MASK])
            && (pmb != RESERVED_BLOCK)
            && (pmb->alk & aky)
            && ((entry = pmb->aPages[(dwAddr>>VA_PAGE)&PAGE_MASK]) & PG_VALID_MASK)
            && (!(dwFlags & VERIFY_WRITE_FLAG) || IsPageWritable (entry))) {

            if (!(pvAddr = EntryToKVA (entry))) {
                // a VirtualCopy'd address that doesn't have a statically mapped kernel
                // address equivalent. Just return the user mode VA.
                //DEBUGMSG (1, (L"VerifyAccess (%8.8lx %8.8lx %8.8lx) return %8.8lx (entry = %8.8lx)\r\n", dwAddr, dwFlags, aky, dwAddr, entry));
                return (LPVOID) dwAddr;
            }
            pvAddr = (LPVOID) ((DWORD)pvAddr | (dwAddr & (PAGE_SIZE-1)));
        } else {
            return NULL;
        }
    }

    // DEBUGMSG (1, (L"VerifyAccess (%8.8lx %8.8lx %8.8lx) return %8.8lx\r\n", dwAddr, dwFlags, aky, pvAddr));
    return pvAddr;
}

static BOOL IsROMNoCS (LPVOID pvAddr, DWORD cbSize)
{
    PSECTION pscn;
    MEMBLOCK *pmb;
    DWORD dwAddr = (DWORD) pvAddr;
    DWORD ixBlock, ixPage, cPages;

    // kernel address: just pass it to OEM
    if (IsKernelVa (pvAddr))
        return pfnOEMIsRom (pvAddr, cbSize);

    // consider kpageblock ROM
    if (ZeroPtr (pvAddr) < 0x10000)
        return TRUE;

    // virtual address, figure out from VM structure
    pscn = (dwAddr & 0x80000000)? &NKSection : SectionTable[dwAddr>>VA_SECTION];
    ixBlock = (dwAddr >> VA_BLOCK) & BLOCK_MASK;
    ixPage = (dwAddr >> VA_PAGE) & PAGE_MASK;
    cPages = ((dwAddr + cbSize + PAGE_SIZE-1) / PAGE_SIZE) - (dwAddr / PAGE_SIZE);

    while (cPages) {
        if ((pmb = (*pscn)[ixBlock ++]) == NULL_BLOCK)
            // not reserved, cannot be written to -- consider it ROM
            return TRUE;
            
        if (RESERVED_BLOCK == pmb) {
            // reseved, but not commited, cannot be ROM
            if ((int) (cPages -= (PAGES_PER_BLOCK - ixPage)) <= 0)
                return FALSE;
        } else {
            // examine all pages in the memblock
            for ( ; cPages && (ixPage < PAGES_PER_BLOCK); cPages --, ixPage ++ ) {
                DWORD dwEntry = pmb->aPages[ixPage];
                if (BAD_PAGE == dwEntry)
                    return TRUE;
                if ((pvAddr = EntryToKVA(dwEntry))
                    && pfnOEMIsRom (pvAddr, 1))
                    return TRUE;
            }
        }
        ixPage = 0;
    }

    return FALSE;
}

BOOL IsROM (LPVOID pvAddr, DWORD cbSize)
{
    BOOL fRet;
    EnterCriticalSection (&VAcs);
    fRet = IsROMNoCS (pvAddr, cbSize);
    LeaveCriticalSection (&VAcs);
    return fRet;
}


BOOL IsStackVA (DWORD dwAddr, DWORD dwLen)
{
    DWORD dwStkEnd, dwStkBeg = KSTKBASE(pCurThread);

    if (pCurThread->tlsPtr != pCurThread->tlsNonSecure) {
        // non-trusted app
        dwStkEnd = dwStkBeg + CNP_STACK_SIZE;   // fix size for secure stack
    } else {
        dwStkEnd = dwStkBeg + (KCURFIBER(pCurThread)? KPROCSTKSIZE(pCurThread) : pCurThread->dwOrigStkSize);
    }
    if (!(dwAddr >> VA_SECTION)) {
        dwAddr |= pCurThread->pProc->dwVMBase;   // will return FALSE if dwAddr is VA of other process
    }
    if ((dwStkBeg < (dwAddr + dwLen)) && (dwStkEnd > dwAddr)) {
        NKDbgPrintfW (L"KPROCSTKSIZE = %8.8lx (tls = %8.8lx, tlsNonSec = %8.8lx)\n", KPROCSTKSIZE(pCurThread), pCurThread->tlsPtr, pCurThread->tlsNonSecure);
        NKDbgPrintfW (L"dwAddr = 0x%x, dwLen = 0x%x, dwStkBeg = 0x%x, dwStkEnd = 0x%x\n", dwAddr, dwLen, KSTKBASE(pCurThread), dwStkEnd);
        DEBUGCHK (0);
        return TRUE;
    }
    return FALSE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
ulong 
MakePagePerms( 
    DWORD fdwProtect, DWORD dwAddr
    ) 
{
    ulong ulMask;
    ulong uWriteProt = PG_PROT_WRITE;

    if (IsInSharedSection (dwAddr)
        && (PAGE_READWRITE != fdwProtect)) {
        // protection at shared section must be READWRITE
        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
        return 0;
    }
    
#ifdef ARM
    if ((dwAddr & 0x80000000)
        || ((dwAddr < (1 << VA_SECTION)) && (ProcArray == pCurProc))) {
        // secure section - kmode access only
        uWriteProt = PG_PROT_UNO_KRW;
    } else if (IsInSharedSection (dwAddr)) {
        // shared section - user R/O, kernel R/W
        uWriteProt = PG_PROT_URO_KRW;
    }
#endif
    
    switch (fdwProtect & ~(PAGE_GUARD | PAGE_NOCACHE
#ifdef ARM
            | PAGE_ARM_MINICACHE
#endif
#ifdef x86
            | PAGE_x86_WRITETHRU
#endif
            )) {
        case PAGE_READONLY:
            ulMask = PG_VALID_MASK | PG_PROT_READ;
            break;
        case PAGE_EXECUTE:
        case PAGE_EXECUTE_READ:
            ulMask = PG_VALID_MASK | PG_PROT_READ | PG_EXECUTE_MASK;
            break;
        case PAGE_EXECUTE_READWRITE:
            ulMask = PG_VALID_MASK | PG_DIRTY_MASK | PG_EXECUTE_MASK | uWriteProt;
            break;
        case PAGE_READWRITE:
            ulMask = PG_VALID_MASK | PG_DIRTY_MASK | uWriteProt;
            break;
        case PAGE_WRITECOPY:
        case PAGE_EXECUTE_WRITECOPY:
            KSetLastError(pCurThread, ERROR_NOT_SUPPORTED);
            return 0;
        case PAGE_NOACCESS:
            if (fdwProtect != PAGE_NOACCESS) {
                KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
                return 0;
            }
            ulMask = PG_GUARD;  /* dummy return value */
            break;
        default:
            KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
            return 0;
    }
    DEBUGCHK(ulMask);
    if (fdwProtect & PAGE_GUARD)
        ulMask = (ulMask & ~PG_VALID_MASK) | PG_GUARD;
#ifdef ARM
    ulMask |= (fdwProtect & PAGE_NOCACHE) ? PG_NOCACHE : (fdwProtect & PAGE_ARM_MINICACHE) ? PG_MINICACHE : PG_CACHE;
#elif defined(x86)
    ulMask |= (fdwProtect & PAGE_NOCACHE) ? PG_NOCACHE : (fdwProtect & PAGE_x86_WRITETHRU) ? PG_WRITE_THRU_MASK : PG_CACHE;
#else
    ulMask |= (fdwProtect & PAGE_NOCACHE) ? PG_NOCACHE : PG_CACHE;
#endif
    ulMask |= PG_SIZE_MASK;
    DEBUGMSG(ZONE_VIRTMEM, (TEXT("MakePagePerms: %8.8lx returns %8.8lx\r\n"), fdwProtect, ulMask));
    return ulMask;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
ProtectFromPerms (ulong ulPage, DWORD dwAddr) 
{
    DWORD fdwProt;
    DWORD dwWriteProt = PAGE_READWRITE;

    if (IsInSharedSection (dwAddr) &&  !(pCurThread->tlsPtr[PRETLS_THRDINFO] & UTLS_INKMODE)) {
        dwWriteProt = PAGE_READONLY;
    }
    switch (ulPage & (PG_PROTECTION|PG_EXECUTE_MASK)) {
        case PG_PROT_READ:
            fdwProt = PAGE_READONLY;
            break;
        case PG_PROT_WRITE:
#ifdef ARM
        case PG_PROT_UNO_KRW:       // secure section
        case PG_PROT_URO_KRW:       // shared section
#endif
            fdwProt = dwWriteProt;
            break;
#if PG_EXECUTE_MASK
        case PG_EXECUTE_MASK|PG_PROT_READ:
            fdwProt = PAGE_EXECUTE_READ;
            break;
        case PG_EXECUTE_MASK|PG_PROT_READ|PG_PROT_WRITE:
            fdwProt = PAGE_EXECUTE_READWRITE;
            break;
#endif
        default: // invalid combinations get mapped to PAGE_NOACCESS
            return PAGE_NOACCESS;
    }
    if ((ulPage & (PG_VALID_MASK|PG_GUARD)) == PG_GUARD)
        fdwProt |= PAGE_GUARD;
    if ((ulPage & PG_CACHE_MASK) == PG_NOCACHE)
        fdwProt |= PAGE_NOCACHE;
    return fdwProt;
}

BOOL NKSetMemoryAttributes (LPVOID pVirtualAddr, LPVOID pShiftedPhysAddr, DWORD cbSize, DWORD dwAttributes)
{
    DWORD dwPhysAddr = (PHYSICAL_ADDRESS_UNKNOWN == pShiftedPhysAddr)? 0 : ((DWORD) pShiftedPhysAddr << 8);
    BOOL  fRet;
    // validate parameter
    if (!pfnOEMSetMemoryAttributes
        || !cbSize
        || !pVirtualAddr
        || ((DWORD) pVirtualAddr & (PAGE_SIZE-1))
        || (dwPhysAddr & (PAGE_SIZE-1))
        || (cbSize & (PAGE_SIZE-1))) {
        KSetLastError (pCurThread, pfnOEMSetMemoryAttributes? ERROR_NOT_SUPPORTED : ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    EnterCriticalSection (&VAcs);
    // call to OEM to set the memory attributes
    // NOTE: we intentionally not checking dwAttributes so we don't have to change kernel
    //       in case we support more attributes in the future.
    fRet = pfnOEMSetMemoryAttributes (pVirtualAddr, pShiftedPhysAddr, cbSize, dwAttributes);
    LeaveCriticalSection (&VAcs);

    if (!fRet) {
        KSetLastError (pCurThread, ERROR_NOT_SUPPORTED);
    }
    return fRet;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
FindFirstBlock(
    PSECTION pscn,
    int ix
    ) 
{
    MEMBLOCK *pmb;
    do {
        if (ix < 0 || (pmb = (*pscn)[ix]) == NULL_BLOCK)
            return UNKNOWN_BASE;
        --ix;
    } while (pmb == RESERVED_BLOCK);
    return pmb->ixBase;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
ScanRegion(
    PSECTION pscn,      /* section array */
    int ixFirB,         /* first block in region */
    int ixBlock,        /* starting block to scan */
    int ixPage,         /* starting page in block */
    int cPgScan,        /* # of pages to scan */
    int *pcPages,       /* ptr to count of pages in region */
    DWORD dwVMBase      /* VM Base address of the section */
    )
{
    register int cPages;
    int cpAlloc;
    register MEMBLOCK *pmb;
    DWORD err;
    DEBUGMSG(ZONE_VIRTMEM, (TEXT("Scanning %d pages. ixFirB=%x, dwVMBase = %8.8lx\r\n"), cPgScan, ixFirB, dwVMBase));
    cpAlloc = 0;
    for (cPages = 0 ; cPages < cPgScan && ixBlock < BLOCK_MASK+1 ; ++ixBlock) {
        pmb = (*pscn)[ixBlock];
        DEBUGMSG(ZONE_VIRTMEM, (TEXT("Scanning block %8.8lx (%x), ix=%d cPgScan=%d\r\n"),
                pmb, (pmb!=RESERVED_BLOCK&&pmb!=NULL_BLOCK)?pmb->ixBase:-1, ixPage, cPgScan));
        if (pmb == RESERVED_BLOCK) {
            /* Reserved block. If not just counting pages, allocate a new block
             * and initialize it as part of the current region.
             */
            if (!pcPages) {
                if (!(pmb = MDAllocMemBlock (dwVMBase, ixBlock))) {
                    err = ERROR_NOT_ENOUGH_MEMORY;
                    goto setError;
                }
                pmb->alk = (*pscn)[ixFirB]->alk;
                pmb->flags = (*pscn)[ixFirB]->flags;
                pmb->hPf = (*pscn)[ixFirB]->hPf;
                pmb->cUses = 1;
                pmb->ixBase = ixFirB;
                (*pscn)[ixBlock] = pmb;
                DEBUGMSG(ZONE_VIRTMEM, (TEXT("ScanRegion: created block %8.8lx, ix=%d\r\n"), pmb, ixBlock));
            }
            ixPage = PAGES_PER_BLOCK - ixPage;  /* # of pages relevant to scan */
            if ((cPages += ixPage) > cPgScan) {
                cpAlloc += cPgScan - (cPages - ixPage);
                cPages = cPgScan;
            } else
                cpAlloc += ixPage;
        } else if (pmb == NULL_BLOCK || pmb->ixBase != ixFirB) {
            /* This block is not part of the orginal region. If not just
             * counting pages, then fail the request.
             */
            if (pcPages)
                break;      /* just counting, so not an error */
            err = ERROR_INVALID_PARAMETER;
            goto setError;
        } else {
            /* Scan block to count uncommited pages and to verify that the
             * protection on any existing pages is compatible with the protection
             * being requested.
             */
            for ( ; cPages < cPgScan && ixPage < PAGES_PER_BLOCK
                    ; ++ixPage, ++cPages) {
                if (pmb->aPages[ixPage] == BAD_PAGE) {
                    if (pcPages)
                        goto allDone;
                    err = ERROR_INVALID_PARAMETER;
                    goto setError;
                } else if (pmb->aPages[ixPage] == 0)
                    ++cpAlloc;      /* count # of pages to allocate */
            }
        }
        ixPage = 0;            /* start with first page of next block */
    }
allDone:
    if (pcPages)
        *pcPages = cPages;      /* return # of pages in the region */
    return cpAlloc;

setError:
    DEBUGMSG(ZONE_VIRTMEM, (TEXT("ScanRegion failed err=%d\r\n"), err));
    KSetLastError(pCurThread, err);
    return -1;
}

static LPDWORD DecommitOnePage (PSECTION pscn, int ixBlock, int ixPage, LPVOID pvAddr, BOOL bIgnoreLock, LPDWORD pPageList)
{
    MEMBLOCK *pmb = (*pscn)[ixBlock];
    DWORD dwEntry;

    DEBUGCHK (NULL_BLOCK != pmb);
    if ((RESERVED_BLOCK != pmb)
        && (bIgnoreLock || !pmb->cLocks)
        && (dwEntry = pmb->aPages[ixPage])
        && (BAD_PAGE != dwEntry)) {

        LPDWORD p;

		// need to do this before calling OEMCacheRangeFlush or we might except if the cache
        // is physically-tagged (MIPS/SHx) and the page is commited no-access/guard-page.
        pmb->aPages[ixPage] |= PG_VALID_MASK;
        OEMCacheRangeFlush (MapPtrProc (pvAddr, pCurProc), PAGE_SIZE, CACHE_SYNC_DISCARD|CACHE_SYNC_INSTRUCTIONS);
        DEBUGMSG(ZONE_VIRTMEM, (TEXT("Decommitting one page %8.8lx @%3.3x%x000\r\n"), PFNfromEntry(dwEntry), ixBlock, ixPage));
        pmb->aPages[ixPage] = 0;
        if (p = PhysPageToZero (PFNfromEntry(dwEntry))) {
            DEBUGMSG (ZONE_VIRTMEM, (L"Delay Freeing Page (%8.8lx)\r\n", p));
            DEBUGCHK ((DWORD) p & 0x20000000);
            *p = (DWORD) pPageList;
            pPageList = p;
        }
        InvalidateRange (pvAddr, PAGE_SIZE);
    }

    return pPageList;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPDWORD
DecommitPages(
    PSECTION pscn,      /* section array */
    int ixBlock,        /* starting block to decommit */
    int ixPage,         /* starting page in block */
    int cPages,         /* # of pages to decommit */
    DWORD baseScn,      /* base address of section */
    BOOL bIgnoreLock,    /* ignore lockcount when decommitting */
    LPDWORD pPageList   /* the accumlated page list to be freed */
    )
{
    register MEMBLOCK *pmb;
    PHYSICAL_ADDRESS paPFN;
    int cbInvalidate;
    PVOID pvInvalidate = (PVOID)(baseScn + (ixBlock<<VA_BLOCK) + (ixPage<<VA_PAGE));

    if (1 == cPages)
        return DecommitOnePage (pscn, ixBlock, ixPage, pvInvalidate, bIgnoreLock, pPageList);

    cbInvalidate = cPages * PAGE_SIZE;
    
    /* Walk the page tables to free the physical pages. */
    DEBUGMSG(ZONE_VIRTMEM, (TEXT("DecommitPages: %d pages from %3.3x%x000\r\n"), cPages, ixBlock, ixPage));
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_DISCARD|CACHE_SYNC_INSTRUCTIONS);
    for (; cPages > 0 ; ++ixBlock) {
        DEBUGCHK(ixBlock < BLOCK_MASK+1);
        if ((pmb = (*pscn)[ixBlock]) == RESERVED_BLOCK)
            cPages -= PAGES_PER_BLOCK - ixPage;
        else {
            DEBUGCHK(pmb != NULL_BLOCK);
            DEBUGMSG(ZONE_VIRTMEM,(TEXT("Decommiting block (%8.8lx) uc=%d ixPg=%d cPages=%d\r\n"),
                    pmb, pmb->cUses, ixPage, cPages));
            if (pmb->cUses > 1 && ixPage == 0 && (cPages >= PAGES_PER_BLOCK || pmb->aPages[cPages] == BAD_PAGE)) {
                /* Decommiting a duplicated block. Remove the additional
                 * use of the block and change the block entry to ReservedBlock */
                (*pscn)[ixBlock] = RESERVED_BLOCK;
                --pmb->cUses;
                cPages -= PAGES_PER_BLOCK;
            } else if (bIgnoreLock || !pmb->cLocks) {
                PmbDecommitting = pmb;
                for ( ; cPages && ixPage < PAGES_PER_BLOCK ; ++ixPage, --cPages) {
                    if ((paPFN = pmb->aPages[ixPage]) != 0 && paPFN != BAD_PAGE) {
                        LPDWORD p;
                        paPFN = PFNfromEntry(paPFN);
                        DEBUGMSG(ZONE_VIRTMEM, (TEXT("Decommitting %8.8lx @%3.3x%x000\r\n"), paPFN, ixBlock, ixPage));
                        pmb->aPages[ixPage] = 0;
                        if (p = PhysPageToZero (paPFN)) {
                            DEBUGMSG (ZONE_VIRTMEM, (L"Delay Freeing Page (%8.8lx)\r\n", p));
                            DEBUGCHK ((DWORD) p & 0x20000000);
                            *p = (DWORD) pPageList;
                            pPageList = p;
                        }
                    }
                }
                PmbDecommitting = 0;
            } else {
                cPages -= PAGES_PER_BLOCK - ixPage;
                DEBUGMSG(1,(L"DecommitPages: Cannot decommit block at %8.8lx, lock count %d\r\n",
                    (PVOID)(baseScn + (ixBlock<<VA_BLOCK) + (ixPage<<VA_PAGE)),pmb->cLocks));
            }
        }
        ixPage = 0;            /* start with first page of next block */
    }
    // Since pages have been removed from the page tables, it is necessary to flush the TLB.
    InvalidateRange(pvInvalidate, cbInvalidate);
    return pPageList;
}

void FreePageList (LPDWORD pPageList)
{
    LPDWORD pCurrPage;
    for (pCurrPage = pPageList; pCurrPage; pCurrPage = pPageList) {
        DEBUGMSG (ZONE_VIRTMEM, (L"FreePageList Freeing Page (%8.8lx, pfn = %8.8lx)\r\n", pCurrPage, GetPFN ((DWORD) pCurrPage & ~0x20000000)));
        pPageList = (LPDWORD) *pCurrPage;
        ZeroAndLinkPhysPage (pCurrPage);
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
ReleaseRegion(
    PSECTION pscn,          /* section array */
    int ixFirB              /* first block in region */
    )
{
    register MEMBLOCK *pmb;
    register int ixBlock;
    /* Walk the section array to free the MEMBLOCK entries. */
    for (ixBlock = ixFirB ; ixBlock < BLOCK_MASK+1 ; ++ixBlock) {
        pmb = (*pscn)[ixBlock];
        if (pmb == RESERVED_BLOCK)
            (*pscn)[ixBlock] = NULL_BLOCK;
        else if (pmb == NULL_BLOCK || pmb->ixBase != ixFirB)
            break;
        else {
#ifdef DEBUG
            int ix;
            ulong ulPFN;
            for (ix = 0 ; ix < PAGES_PER_BLOCK ; ++ix) {
                if ((ulPFN = pmb->aPages[ix]) != 0 && ulPFN != BAD_PAGE) {
                    DEBUGMSG(ZONE_VIRTMEM, (TEXT("ReleaseRegion: Commited page found: %8.8lx @%3.3x%x000\r\n"),
                        ulPFN, ixBlock, ix));
                    DEBUGCHK(0);
                }
            }
#endif
            DEBUGMSG(ZONE_VIRTMEM, (TEXT("Releasing memblock %8.8lx @%3.3x0000\r\n"), pmb, ixBlock));
            (*pscn)[ixBlock] = NULL_BLOCK;
            MDFreeMemBlock (pmb);
        }
    }
    /*Note: Since no pages are freed by this routine, no TLB flushing is needed */
}



//------------------------------------------------------------------------------
//  IsAccessOK() - check access permissions for Address
// 
//   This function checks the access permissions for an address. For user
//  virtual addresses, only the access key for the memory region is checked.
//  Kernel space addresses are always allowed because access to them is not
//  access key dependant.
// 
//  This function doesn't check that the page is either present or valid.
// 
//  Environment:
//    Kernel mode, preemtible, running on the thread's stack.
//------------------------------------------------------------------------------
BOOL 
IsAccessOK(
    void *addr,
    ACCESSKEY aky
    ) 
{
    if (!IsKernelVa (addr)) {

        PSECTION pscn = IsSecureVa (addr)? &NKSection : SectionTable[(ulong)addr >> VA_SECTION];
        MEMBLOCK *pmb;
        int ixBlock;

        ixBlock = ((ulong)addr>>VA_BLOCK) & BLOCK_MASK;

        if ((pmb = (*pscn)[ixBlock]) != NULL_BLOCK) {
            if (pmb == RESERVED_BLOCK)
                pmb = (*pscn)[FindFirstBlock(pscn, ixBlock)];
            if (!TestAccess(&pmb->alk, &aky)) {
                DEBUGMSG(1, (TEXT("IsAccessOK returning FALSE\r\n")));
                return FALSE;
            }
        }
        
    }
    return TRUE;
}


BOOL g_fForcedPaging = TRUE; // This is global because DbgVerify is called from DBG_CallCheck too


/*++

Routine Name:

    DbgVerify

    Routine Description:

    This function takes a void pointer to an address that the caller wants to read
    or write to and determines whether it is possible. Through this process, address 
    in pages that are not currently mapped (paged in) will be paged in if Force Page mode is ON.

    Most of the work is done through calls to VerifyAccess(), which determines 
    specific characteristics of the page in question and will specify whether access 
    is allowed.

    Argument(s):

    pvAddr - [IN] the address to be verified
    fProbeOnly - [IN] Should only test the presence of the VM, do not page-in even if Force Paging is ON
    pfPageInFailed - [OUT] an optional parameter that a caller can use to see if a page
                                was to be paged in, but was not since forced paging was disabled

    Return Value:

    Returns the same address if access if okay, or a different address, or NULL

--*/
PVOID
DbgVerify(
    PVOID pvAddr,
    BOOL fProbeOnly,
    BOOL* pfPageInFailed
)
{    
    BOOL fPageInFailed = FALSE;
    void *pvRet = NULL;
    int flVerify = VERIFY_KERNEL_OK;
    int flLock = LOCKFLAG_QUERY_ONLY | LOCKFLAG_READ;
    
    // Just verify that the Virtual address is valid (no page-in)
    if (!(pvRet = VerifyAccess (pvAddr, flVerify, (ACCESSKEY) -1)))
    { // VA invalid:
        if (g_fForcedPaging && !fProbeOnly)
        { // Forced Page-in active (intrusive debugger):
            if (!IsKernelVa (pvAddr) && !InSysCall () && 
                LockPages (pvAddr, 1, 0, flLock)) // TODO: We should not call this here if VAcs is already hold!!!! Change this (to fail directly or update page table directly if possible)
            { // Query only, do not lock
                pvRet = VerifyAccess (pvAddr, flVerify, (ACCESSKEY)-1);
            }
        }
        else
        { // Forced Page-in inactive (less intrusive debugger):
            fPageInFailed = TRUE;
            DEBUGMSG(ZONE_VIRTMEM, (TEXT(" DebugVerify: VMPage not paged in.\r\n")));
        }
    }

    if (pfPageInFailed) *pfPageInFailed = fPageInFailed;
    return pvRet;
}


#if HARDWARE_PT_PER_PROC

ERRFALSE(PAGE_SIZE == 4096);

MEMBLOCK *GetKPagePMB (DWORD dwVMBase)
{
    MEMBLOCK *pmb = MDAllocMemBlock (dwVMBase, 0);
    if (pmb) {
        DEBUGMSG (ZONE_VIRTMEM, (L"pmb = %8.8lx, pmb->aPages = %8.8lx\r\n", pmb, pmb->aPages));
        pmb->alk = ~0ul;
        pmb->cLocks = 1;
        pmb->ixBase = UNKNOWN_BASE;
        pmb->aPages[5] = KPAGE_PTE; // KPage is from 0x5000 to 0x5fff
    }
    return pmb;
}

#else
const MEMBLOCK KPageBlock = {
    ~0ul,       // alk
    0,          // cUses
    0,          // flags
    UNKNOWN_BASE,// ixBase
    0,          // hPf
    1,          // cLocks
    {
#if PAGE_SIZE == 4096
        0, 0, 0, 0, 0,
#elif PAGE_SIZE == 2048
        /* 0x0000: */ 0, 0, 0, 0,
        /* 0x2000: */ 0, 0, 0, 0,
        /* 0x4000: */ 0, 0, 0,
        /* 0x5800: (start of user visible kernel data page) */
#elif PAGE_SIZE == 1024
        /* 0x0000: */ 0, 0, 0, 0,
        /* 0x1000: */ 0, 0, 0, 0,
        /* 0x2000: */ 0, 0, 0, 0,
        /* 0x3000: */ 0, 0, 0, 0,
        /* 0x4000: */ 0, 0, 0, 0,
        /* 0x5000: */ 0, 0,
        /* 0x5800: (start of user visible kernel data page) */
#else
    #error Unsupported page size.
#endif
        KPAGE_PTE
    }
};

#define GetKPagePMB(dwVMBase)   ((MEMBLOCK*)&KPageBlock)

#endif

SECTION NKSection;


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID 
InitNKSection(void) 
{
    NKSection[0] = GetKPagePMB (SECURE_VMBASE);
    DEBUGCHK (NKSection[0]);
    // save the address of NKSection in KInfo
    KInfoTable[KINX_NKSECTION] = (DWORD) &NKSection;
    
    return (LPVOID) SECURE_VMBASE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID 
DeleteSection(
    LPVOID lpvSect
    ) 
{
    PSECTION pscn;
    DWORD dwSect = (DWORD) lpvSect;
    int ix;
    int ixScn;
    LPDWORD pPageList = NULL;
    
    DEBUGMSG(ZONE_VIRTMEM,(TEXT("DeleteSection %8.8lx\r\n"), dwSect));
    EnterCriticalSection(&VAcs);
    if ((ixScn = dwSect>>VA_SECTION) <= SECTION_MASK
            && ixScn >= RESERVED_SECTIONS
            && (pscn = SectionTable[ixScn]) != NULL_SECTION) {

        DEBUGCHK (pscn != &NKSection);

        // if this is a mapper section, clear the "filesys owned bit for this section
        if ((dwSect >= FIRST_MAPPER_ADDRESS) && (dwSect < LAST_MAPPER_ADDRESS)) {
            g_ObjStoreSlotBits &= ~ MapperSlotToBit (ixScn);
        }
       
        // For process sections, start freeing at block 1. Otherwise, this
        // should be a mapper section which starts freeing at block 0.
#if HARDWARE_PT_PER_PROC
        ix = 0;
#else
        ix = ((*pscn)[0] != &KPageBlock)? 0 : 1;
#endif
        for ( ; ix < BLOCK_MASK+1 ; ++ix) {
            if ((*pscn)[ix] != NULL_BLOCK) {
                if ((*pscn)[ix] != RESERVED_BLOCK) {
                    pPageList = DecommitPages(pscn, ix, 0, PAGES_PER_BLOCK, dwSect, TRUE, pPageList);
                    if ((*pscn)[ix] != RESERVED_BLOCK) {
                        MDFreeMemBlock ((*pscn)[ix]);
                    }
                }
            }
        }
        SectionTable[ixScn] = NULL_SECTION;
        FreeSection(pscn);
        if (ixScn < FirstFreeSection)
            FirstFreeSection = ixScn;
#if HARDWARE_PT_PER_PROC
        {
            extern void FreeHardwarePT (DWORD dwVMBase);
            FreeHardwarePT (dwSect);
        }
#endif
    } else
        DEBUGMSG(ZONE_VIRTMEM,(TEXT("DeleteSection failed.\r\n")));
    // do the zeroing of pages outside VAcs
    LeaveCriticalSection(&VAcs);
    FreePageList (pPageList);

    return;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID CreateSection (LPVOID lpvAddr, BOOL fInitKPage) 
{
    PSECTION pSect;
    uint ixSect;
    DEBUGMSG(ZONE_VIRTMEM,(TEXT("CreateSection %8.8lx\r\n"), lpvAddr));
    EnterCriticalSection(&VAcs);
    if (lpvAddr != 0) {
        if ((ixSect = (ulong)lpvAddr>>VA_SECTION) > SECTION_MASK
                || SectionTable[ixSect] != NULL_SECTION) {
            DEBUGMSG(1,(TEXT("CreateSection failed (1)\r\n")));
            lpvAddr = 0;
            goto exitCreate;
        }
    } else {
        /* scan for an available section table entry */
        // NOTE: never use slot 1
        for (;;) {
            for (ixSect = FirstFreeSection ; ixSect < MAX_PROCESSES+RESERVED_SECTIONS
                    ; ++ixSect) {
                if (SectionTable[ixSect] == NULL_SECTION)
                    goto foundSect;
            }
            if (FirstFreeSection == (RESERVED_SECTIONS+1))
                break;
            FirstFreeSection = RESERVED_SECTIONS+1;
        }
        /* no sections available */
        DEBUGMSG(1, (TEXT("CreateSection failed (2)\r\n")));
        goto exitCreate;
        /* found a free section set new first free index */
foundSect:
        FirstFreeSection = ixSect+1;
    }
    /* Allocate a section and initialize it to invalid blocks */
    if ((pSect = GetSection()) == 0) {
        DEBUGMSG(1, (TEXT("CreateSection failed (3)\r\n")));
        lpvAddr = 0;
        goto exitCreate;
    }
    ///memcpy(pSect, &NullSection, sizeof(NullSection));
    SectionTable[ixSect] = pSect;
    lpvAddr = (LPVOID)(ixSect << VA_SECTION);
    
    if (fInitKPage && !((*pSect)[0] = GetKPagePMB (ixSect << VA_SECTION))) {
        // failed to create Memblock
        DeleteSection (lpvAddr);
        lpvAddr = NULL;
    }
exitCreate:
    LeaveCriticalSection(&VAcs);
    DEBUGMSG(ZONE_VIRTMEM, (TEXT("CreateSection done: addr=%8.8lx, pSect=%8.8lx\r\n"), lpvAddr, pSect));
    return lpvAddr;
}



// CreateMapperSection - allocate memory section for file mapping
//
//  This function will allocate a 32mb section and reserve the entire range for
// use by file mapping.

DWORD g_ObjStoreSlotBits;  // bit fields

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL CreateMapperSection (DWORD dwBase, BOOL fAddProtect) 
{
    BOOL bRet = FALSE;
    MEMBLOCK *pmb;
    PSECTION pscn;
    int ix;
    DEBUGMSG(ZONE_VIRTMEM,(TEXT("CreateMapperSection %8.8lx\r\n"), dwBase));
    if (dwBase < FIRST_MAPPER_ADDRESS || dwBase >= LAST_MAPPER_ADDRESS) {
        DEBUGMSG(1, (TEXT("CreateMapperSection: %8.8lx is out of range.\r\n"), dwBase));
        return FALSE;
    }
    // First, create a normal section. Then convert it into a special section
    // for the Mapper.
    EnterCriticalSection(&VAcs);
    if (CreateSection((LPVOID)dwBase, FALSE) != 0) {
        DWORD dwSlot = (dwBase>>VA_SECTION) & SECTION_MASK;
        pscn = SectionTable[dwSlot];
        if (pmb = MDAllocMemBlock (dwBase, 0)) {
            if (fAddProtect) {
                LockFromKey(&pmb->alk, &pCurProc->aky);
                // 2nd gig section with protection, it's object store
                g_ObjStoreSlotBits |= MapperSlotToBit (dwSlot);
            } else
                LockFromKey(&pmb->alk, &ProcArray[0].aky);
            pmb->cUses = 1;
            pmb->flags = MB_PAGER_MAPPING;
            pmb->ixBase = 0;
            (*pscn)[0] = pmb;
            for (ix = 1 ; ix < BLOCK_MASK+1 ; ++ix)
                (*pscn)[ix] = RESERVED_BLOCK;
            bRet = TRUE;
        } else {
            DeleteSection((LPVOID)dwBase);
            DEBUGMSG(1, (TEXT("CreateMapperSection: unable to allocate MEMBLOCK\r\n")));
        }
    }
    LeaveCriticalSection(&VAcs);
    return bRet;
}



//------------------------------------------------------------------------------
// DeleteMapperSection - delete a section created by CreateMapperSection()
//
//  This function must be used to delete a memory section which is created by
// CreateMapperSection.
//------------------------------------------------------------------------------
void 
DeleteMapperSection(
    DWORD dwBase
    ) 
{
    if ((dwBase >= FIRST_MAPPER_ADDRESS) && (dwBase < LAST_MAPPER_ADDRESS))
        DeleteSection((LPVOID)dwBase);
    else 
        DEBUGMSG(1, (TEXT("DeleteMapperSection: %8.8lx is out of range.\r\n"), dwBase));
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID 
DoVirtualAlloc(
    LPVOID lpvAddress0,          /* address of region to reserve or commit */
    DWORD cbSize,               /* size of the region */
    DWORD fdwAllocationType,    /* type of allocation */
    DWORD fdwProtect,           /* type of access protection */
    DWORD fdwInternal,
    DWORD dwPFNBase
    )
{
    int ixBlock;
    int ixPage;
    int ix;
    int ixFirB;                 /* index of first block in region */
    PSECTION pscn;
    int cPages;                 /* # of pages to allocate */
    int cNeed;
    int cpAlloc;                /* # of physical pages to allocate */
    ulong ulPgMask;             /* page permissions */
    ulong ulPFN;                /* page physical frame number */
    MEMBLOCK *pmb;
    DWORD err;
    DWORD dwEnd;
    LPVOID lpvResult = NULL;
    DWORD dwAddr = (DWORD) lpvAddress0;
    DWORD dwSlot0Addr = ZeroPtrABS(dwAddr);
    ACCESSKEY aky = pCurProc->aky;
    DWORD dwBase;
    BOOL  fNeedInvalidate = FALSE;
    
    ixFirB = UNKNOWN_BASE;

    DEBUGMSG(ZONE_VIRTMEM, (TEXT("VirtualAlloc(%8.8lx, %lx, %lx, %lx)\r\n"),
            lpvAddress0, cbSize, fdwAllocationType, fdwProtect));


    // validate parameters
    if (IsKernelVa (dwAddr)          // invalid address?
        || !cbSize                   // size == 0?
        || ((cbSize >= (1<<VA_SECTION)) // size too big? (okay to reserver > 32M with specific parameters)
            && (dwSlot0Addr || (fdwAllocationType != MEM_RESERVE) || (fdwProtect != PAGE_NOACCESS)))
        || !(ulPgMask = MakePagePerms(fdwProtect, dwAddr))) {   // invalid flag?
        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
        return NULL;
    }        

    dwEnd = dwAddr + cbSize;

    //    
    // Lockout other changes to the virtual memory state. 
    //
    EnterCriticalSection(&VAcs);
    
    if (IsSecureVa(dwAddr)) {
        aky = ProcArray[0].aky;
        pscn = &NKSection;
        dwBase = SECURE_VMBASE;
    } else {
        ix = dwAddr >> VA_SECTION;
        DEBUGCHK (ix <= SECTION_MASK);    // can't be 0, but can be NULL_SECTION
        if ((pscn = SectionTable[ix]) == NULL_SECTION)
            goto invalidParm;
        dwBase = dwAddr & (SECTION_MASK<<VA_SECTION);
        // Make sure that when remotely allocating memory in another process that
        // we use the access key for the remote process instead of the current one.
        if (dwAddr < FIRST_MAPPER_ADDRESS) {
            if (ix && ix <= MAX_PROCESSES && ProcArray[ix-1].dwVMBase)
                aky = ProcArray[ix-1].aky;
        } else if (IsInResouceSection (dwAddr)) {
            aky = ProcArray[0].aky;
        } else if (IsInSharedSection (dwAddr)) {
#if defined (x86) || defined (ARM)
            aky = ProcArray[0].aky;
#endif
        }
    }

    if ((fdwAllocationType & MEM_RESERVE) || (!dwAddr && (fdwAllocationType & MEM_COMMIT))) {
        // Set MEM_RESERVE so the error cleanup works properly.
        fdwAllocationType |= MEM_RESERVE;

        ixBlock = dwSlot0Addr >> VA_BLOCK;
        if (dwSlot0Addr) {
            /* The client has asked to reserve a specific region of memory.
             * Verify that the requested region is within range and within an
             * existing memory section that the client is allowed to access.
             */
            /* adjust lpvAddress to 64K boundary. */
            dwAddr &= 0xFFFF0000l;
            /* Verify that the entire range is available to be reserved. */
            cPages = (dwEnd - dwAddr + PAGE_SIZE-1) / PAGE_SIZE;
            for (cNeed = cPages, ix = ixBlock ; cNeed > 0
                    ; ++ix, cNeed -= PAGES_PER_BLOCK) {
                if ((*pscn)[ix] != NULL_BLOCK)
                    goto invalidParm;
            }
        } else {
            /* No specific address given. Go find a region of free blocks */
            cPages = (dwEnd - dwAddr + PAGE_SIZE-1) / PAGE_SIZE;
            cNeed = cPages;
            if (((cPages * PAGE_SIZE >= 2*1024*1024) || (fdwInternal & MEM_SHAREDONLY))
                && (fdwAllocationType == MEM_RESERVE)
                && (fdwProtect == PAGE_NOACCESS)) {
                DEBUGMSG(ZONE_VIRTMEM, (TEXT("VirtualAlloc(%8.8lx, %lx, %lx, %lx): doing HugeVirtualAlloc\r\n"),
                         lpvAddress0, cbSize, fdwAllocationType, fdwProtect));
                LeaveCriticalSection(&VAcs);
                lpvResult = HugeVirtualReserve(cbSize, fdwInternal & MEM_SHAREDONLY);
                if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
                    CELOG_VirtualAlloc((DWORD)lpvResult, (DWORD)dwAddr, cbSize, fdwAllocationType, fdwProtect);
                }
                return lpvResult;
            }
            if (fdwAllocationType & MEM_TOP_DOWN) {
                /* Scan backwards from the end of the section */
                for (ix = BLOCK_MASK+1 ; --ix > 0 ; ) {
                    if ((*pscn)[ix] != NULL_BLOCK)
                        cNeed = cPages;
                    else if ((cNeed -= PAGES_PER_BLOCK) <= 0) {
                        ixBlock = ix;
                        goto foundRegion;
                    }
                }
            } else {
                /* Scan forwards from the beginning of the section */
                ixBlock = 1;
                for (ix = 1 ; ix < BLOCK_MASK+1 ; ++ix) {
                    if ((*pscn)[ix] != NULL_BLOCK) {
                        ixBlock = ix+1;
                        cNeed = cPages;
                    } else if ((cNeed -= PAGES_PER_BLOCK) <= 0)
                        goto foundRegion;
                }
            }
            err = ERROR_NOT_ENOUGH_MEMORY;
            goto setError;
foundRegion:
            dwAddr = dwBase | ((ulong)ixBlock << VA_BLOCK);
        }
        /* Reserve the range of blocks */
        if (!(pmb = MDAllocMemBlock (dwBase, ixBlock))) {
            err = ERROR_NOT_ENOUGH_MEMORY;
            goto setError;
        }
        LockFromKey(&pmb->alk, &aky);

        DEBUGMSG(ZONE_VIRTMEM, (TEXT("VirtualAlloc: reserved lock=%lx\r\n"), pmb->alk));
        pmb->cUses = 1;
        switch (fdwAllocationType & (MEM_MAPPED|MEM_IMAGE)) {
        case MEM_MAPPED:
            pmb->flags = MB_PAGER_MAPPING;
            break;
        case MEM_IMAGE:
            pmb->flags = MB_PAGER_LOADER;
            break;
        case 0:
            pmb->flags = MB_PAGER_NONE;
            break;
        default:
            goto invalidParm;
        }
        if (fdwAllocationType & MEM_AUTO_COMMIT)
            pmb->flags |= MB_FLAG_AUTOCOMMIT;
        pmb->ixBase = ixBlock;
        (*pscn)[ixBlock] = pmb;
        DEBUGMSG(ZONE_VIRTMEM,
                (TEXT("VirtualAlloc: created head block pmb=%8.8lx (%x)\r\n"),
                pmb, ixBlock));
        ixFirB = ixBlock;   /* note start of region for error recovery */
        for (cNeed = cPages, ix = ixBlock+1 ; (cNeed -= PAGES_PER_BLOCK) > 0 ; ++ix) {
            if (cNeed < PAGES_PER_BLOCK || (fdwAllocationType & MEM_AUTO_COMMIT)) {
                /* The last block is not full so must allocate a MEMBLOCK
                 * and mark the extra pages as invalid or this is an auto-commit
                 * region which must have all memblocks filled in so that
                 * pages can be committed without entering the virtual memory
                 * critical section. */
                if (!(pmb = MDAllocMemBlock (dwBase, ix))) {
                    err = ERROR_NOT_ENOUGH_MEMORY;
                    goto setError;
                }
                pmb->alk = aky;
                pmb->flags = (*pscn)[ixFirB]->flags;
                pmb->cUses = 1;
                pmb->ixBase = ixFirB;
                (*pscn)[ix] = pmb;
                DEBUGMSG(ZONE_VIRTMEM,
                        (TEXT("VirtualAlloc: created a tail block pmb=%8.8lx (%x)\r\n"),
                        pmb, ixBlock));
            } else
                (*pscn)[ix] = RESERVED_BLOCK;
        }
        DEBUGMSG(ZONE_VIRTMEM, (TEXT("Reserved %d pages @%8.8lx\r\n"), cPages, dwAddr));
        if (cNeed) {
            /* Set unused entries to BAD_PAGE */
            DEBUGMSG(ZONE_VIRTMEM, (TEXT("Reserved %d extra pages.\r\n"), -cNeed));
            cNeed += PAGES_PER_BLOCK;
            for (ix = cNeed ; ix < PAGES_PER_BLOCK ; ++ix)
                pmb->aPages[ix] = BAD_PAGE;
        }
        /* If not committing pages, then return the address of the
         * reserved region */
        if (!(fdwAllocationType & MEM_COMMIT)) {
            LeaveCriticalSection(&VAcs);
            ERRORMSG(!dwAddr,(L"Failed VirtualAlloc/reserve of %8.8lx bytes\r\n",cbSize));
            if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
                CELOG_VirtualAlloc(dwAddr, (DWORD)lpvAddress0, cbSize, fdwAllocationType, fdwProtect);
            }
            return (LPVOID) dwAddr;

        }
    } else {
        //
        // Not reserving memory, so must be committing. Verify that the
        // requested region is within range and within an existing reserved
        // memory region that the client is allowed to access.
        //
        if (!(fdwAllocationType & MEM_COMMIT)
            || !dwAddr
//            || (!(fdwInternal&MEM_NOSTKCHK) && IsStackVA (dwSlot0Addr | (dwBase? dwBase : pCurProc->dwVMBase), cbSize))
            || (fdwInternal & MEM_CONTIGUOUS))
            goto invalidParm;

        ixBlock = (dwAddr >> VA_BLOCK) & BLOCK_MASK;
        /* Adjust lpvAddress to PAGE boundary and calculate the number
         * of pages to commit. */
        dwAddr &= ~(PAGE_SIZE-1);
        cPages = (dwEnd - dwAddr + PAGE_SIZE-1) / PAGE_SIZE;
        /* locate the starting block of the region */
        if ((ixFirB = FindFirstBlock(pscn, ixBlock)) == UNKNOWN_BASE)
            goto invalidParm;
    }
    /* Verify that cPages of memory starting with the first page indicated by
     * lpvAddress can be committed within the region.
     *
     *  (pscn) = ptr to section array
     *  (ixBlock) = index of block containing the first page to commit
     *  (cPages) = # of pages to commit
     */
    ixPage = (dwAddr >> VA_PAGE) & PAGE_MASK;
    cpAlloc = ScanRegion(pscn, ixFirB, ixBlock, ixPage, cPages, 0, dwBase);
    if (cpAlloc == -1)
        goto cleanUp;
    //
    // Commit cPages of memory starting with the first page indicated by
    // lpvAddress. Allocate all required pages before any changes to the
    // virtual region are performed.
    //
    //  (pscn) = ptr to section array
    //  (ixBlock) = index of block containing the first page to commit
    //  (cPages) = # of pages to commit
    //  (cpAlloc) = # of physical pages required
    //
    DEBUGMSG(ZONE_VIRTMEM, (TEXT("Allocating %d pages.\r\n"), cpAlloc));
    
    if (fdwInternal & MEM_CONTIGUOUS) {
        //
        // Map to physically contiguous pages. Mark as LOCKED as we go.
        // Walk the page tables to map in the physical pages.
        //
        ulPFN = dwPFNBase;

        for (; cPages ; ixBlock++) {
            pmb = (*pscn)[ixBlock];
            pmb->cLocks++;
            ix = 0;
            for (; cPages && ix < PAGES_PER_BLOCK; --cPages) {
                DEBUGCHK(pmb->aPages[ix] == 0);
                pmb->aPages[ix] = ulPFN | ulPgMask;
                ulPFN = NextPFN(ulPFN);
                ix++;
            }
        }
    } else {
        if (!HoldPages(cpAlloc, FALSE)) {
            err = ERROR_NOT_ENOUGH_MEMORY;
            goto setError;
        }
        ixBlock = (ixBlock*PAGES_PER_BLOCK+ixPage+cPages-1)/PAGES_PER_BLOCK;
        ix = ((ixPage + cPages - 1) % PAGES_PER_BLOCK) + 1;
        //
        // Walk the page tables to map in the physical pages.
        //
        for (; cPages ; --ixBlock) {
            pmb = (*pscn)[ixBlock];
            for (; cPages && ix-- > 0 ; --cPages) {
                if (pmb->aPages[ix] == 0) {
                    DWORD dwRetries;
                    for (dwRetries = 0; (dwRetries < 20) && !(ulPFN = GetHeldPage()); dwRetries++)
                        Sleep(100);
                    if (ulPFN) {
                        DEBUGMSG(ZONE_VIRTMEM, (TEXT("Mapping %8.8lx @%3.3x%x000 perm=%x\r\n"),
                                ulPFN, ixBlock, ix, ulPgMask));
                        pmb->aPages[ix] = ulPFN | ulPgMask;
                    } else {
                        InterlockedIncrement(&PageFreeCount);
                        RETAILMSG(1,(L"--->>> VirtualAlloc: FATAL ERROR!  COMPLETELY OUT OF MEMORY (%8.8lx)!\r\n",PageFreeCount));
                        err = ERROR_NOT_ENOUGH_MEMORY;
                        goto setError;
                    }
                } else {
                    fNeedInvalidate = TRUE;
                    pmb->aPages[ix] = (pmb->aPages[ix] & ~PG_PERMISSION_MASK) | ulPgMask;
                }
            }
            ix = PAGES_PER_BLOCK;            /* start with last page of previous block */
        }
    }
    if (fNeedInvalidate) {
        DEBUGMSG(ZONE_VIRTMEM, (TEXT("Calling InvalidateRange (%8.8lx, %d)\n"), dwAddr, cbSize));
        InvalidateRange((LPVOID) dwAddr, cbSize); // in case we changed permissions above
    }
    LeaveCriticalSection(&VAcs);
    ERRORMSG(!dwAddr,(L"Failed VirtualAlloc(%8.8lx) of %8.8lx bytes\r\n",fdwAllocationType,cbSize));
    
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CELOG_VirtualAlloc(dwAddr, dwAddr, cbSize, fdwAllocationType, fdwProtect);
    }
    DEBUGMSG(ZONE_VIRTMEM, (TEXT("Returning %8.8lx\n"), dwAddr));
    return (LPVOID) dwAddr;
    
    //
    // There was an error reserving or commiting a range of pages. If reserving
    // pages, release any pages which were reserved before the failure occured.
    //
invalidParm:
    err = ERROR_INVALID_PARAMETER;
setError:
    DEBUGMSG(ZONE_VIRTMEM, (TEXT("VirtualAlloc failed. error=%d\r\n"), err));
    KSetLastError(pCurThread, err);
cleanUp:
    if (fdwAllocationType & MEM_RESERVE && ixFirB != UNKNOWN_BASE) {
        /* Remove the reservation */
        ReleaseRegion(pscn, ixFirB);
    }
    LeaveCriticalSection(&VAcs);
    ERRORMSG(!dwAddr,(L"Failed VirtualAlloc(%8.8lx) of %8.8lx bytes\r\n",fdwAllocationType,cbSize));
    return 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID 
SC_VirtualAlloc(
    LPVOID lpvAddress,          /* address of region to reserve or commit */
    DWORD cbSize,               /* size of the region */
    DWORD fdwAllocationType,    /* type of allocation */
    DWORD fdwProtect            /* type of access protection */
    )
{
    return DoVirtualAlloc(lpvAddress, cbSize, fdwAllocationType, fdwProtect, 0, 0);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_VirtualFree(
    LPVOID lpvAddress0,      /* address of region of committed pages */
    DWORD cbSize,           /* size of the region */
    DWORD fdwFreeType       /* type of free operation */
    )
{
    int ixBlock;
    int ixPage;
    int ixFirB;         /* index of first block in region */
    PSECTION pscn;
    int cpReserved;     /* # of reserved (not commited) pages in region */
    int cpRegion;       /* total # of pages in region */
    int cPages;         /* # of pages to free */
    DWORD baseScn;      /* base address of section */
    DWORD dwAddr = (DWORD) lpvAddress0;
    BOOL bForceDecommit = (fdwFreeType & 0x80000000);
    LPDWORD pPageList = NULL;
    /* Verify that the requested region is within range and within an
     * existing reserved memory region that the client is allowed to
     * access and locate the starting block of the region.
     */
    fdwFreeType &= ~0x80000000;
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CELOG_VirtualFree(dwAddr, cbSize, fdwFreeType);
    }
    DEBUGMSG(ZONE_VIRTMEM, (TEXT("VirtualFree @%8.8lx size=%lx freetype=%lx\r\n"),
            dwAddr, cbSize, fdwFreeType));

    if (!dwAddr || IsKernelVa (dwAddr) || IsStackVA(dwAddr, cbSize)) {
        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if ((dwAddr < LAST_MAPPER_ADDRESS) && (dwAddr >= FIRST_MAPPER_ADDRESS) && !cbSize && (fdwFreeType == MEM_RELEASE)) {
        DEBUGMSG(ZONE_VIRTMEM, (TEXT("VirtualFree @%8.8lx size=%lx freetype=%lx : doing HugeVirtualRelease\r\n"),
            dwAddr, cbSize, fdwFreeType));
        DEBUGCHK (dwAddr == (DWORD) lpvAddress0);
        return HugeVirtualRelease(lpvAddress0);
    }
    /* Lockout other changes to the virtual memory state. */
    EnterCriticalSection(&VAcs);

    if (IsSecureVa(dwAddr)) {
        pscn = &NKSection;
        baseScn = SECURE_VMBASE;
    } else {
        DEBUGCHK ((dwAddr >> VA_SECTION) <= SECTION_MASK);    // can't be 0, but can be NULL_SECTION
        if ((pscn = SectionTable[dwAddr >>VA_SECTION]) == NULL_SECTION)
            goto invalidParm;
        baseScn = dwAddr & (SECTION_MASK<<VA_SECTION);
    }

    ixBlock = (dwAddr >> VA_BLOCK) & BLOCK_MASK;
    if ((ixFirB = FindFirstBlock(pscn, ixBlock)) == UNKNOWN_BASE)
        goto invalidParm;
    /* Verify the status of the region based upon the type of free operation
     * being performed.
     *
     *  (pscn) = ptr to section array
     *  (ixBlock) = index of block containing the first page of lpvAddress
     *  (ixFirB) = index of first block in the region
     */
    ixPage = (dwAddr >> VA_PAGE) & PAGE_MASK;
    if (fdwFreeType == MEM_RELEASE) {
        DEBUGMSG(ZONE_VIRTMEM, (TEXT("VirtualFree releasing region ixBlock=%x.\r\n"),ixBlock));
        if (cbSize != 0 || ixPage != 0 || ixBlock != ixFirB)
            goto invalidParm;
        cpReserved = ScanRegion(pscn, ixFirB, ixFirB, 0, (BLOCK_MASK+1)*PAGES_PER_BLOCK, &cpRegion, baseScn);
        DEBUGCHK(cpReserved != -1);
        DEBUGMSG(ZONE_VIRTMEM, (TEXT("VirtualFree: cpReserved=%d cpRegion=%d\r\n"), cpReserved, cpRegion));
        /* The entire region must be either reserved or commited. */
        if (cpReserved != cpRegion) {
            if (cpReserved != 0)
                goto invalidParm;
            pPageList = DecommitPages(pscn, ixFirB, 0, cpRegion, baseScn, bForceDecommit, NULL);
        }
        ReleaseRegion(pscn, ixFirB);
        LeaveCriticalSection(&VAcs);
        FreePageList (pPageList);
        return TRUE;
    } else if (fdwFreeType == MEM_DECOMMIT) {
        DEBUGMSG(ZONE_VIRTMEM, (TEXT("VirtualFree decommitting block %x page %x.\r\n"), ixBlock, ixPage));
        if (cbSize == 0)
            goto invalidParm;
        cPages = ((dwAddr + cbSize + PAGE_SIZE-1) / PAGE_SIZE)
                - (dwAddr / PAGE_SIZE);
        cpReserved = ScanRegion(pscn, ixFirB, ixBlock, ixPage, cPages, &cpRegion, baseScn);
        if (cpRegion < cPages)
            goto invalidParm;
        if (cpReserved != cPages)
            pPageList = DecommitPages(pscn, ixBlock, ixPage, cPages, baseScn, bForceDecommit, NULL);
        LeaveCriticalSection(&VAcs);
        FreePageList (pPageList);
        return TRUE;
    }
invalidParm:
    LeaveCriticalSection(&VAcs);
    DEBUGMSG(ZONE_VIRTMEM, (TEXT("VirtualFree failed.\r\n")));
    KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
    return FALSE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID SC_CeVirtualSharedAlloc (LPVOID lpvAddr, DWORD cbSize, DWORD fdwAction)
{
    DWORD dwErr = 0;
    TRUSTED_API (L"SC_CeVirtualSharedAlloc", NULL);

    // verify parameters
    if ((fdwAction & ~(MEM_RESERVE|MEM_COMMIT)) || !fdwAction) {
        dwErr = ERROR_INVALID_PARAMETER;
    } else if (lpvAddr) {
        if (!IsInSharedSection (lpvAddr) || (MEM_COMMIT != fdwAction)) {
            dwErr = ERROR_INVALID_PARAMETER;
        }
    } else {
        // lpvAddr is NULL, always add the reserve flag and change the address
        // to the base of the shared section
        fdwAction |= MEM_RESERVE;
        lpvAddr = (LPVOID) SHARED_BASE_ADDRESS;
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
        return NULL;
    }
    return DoVirtualAlloc (lpvAddr, cbSize, fdwAction, PAGE_READWRITE, 0, 0);
}


//------------------------------------------------------------------------------
//  @doc INTERNAL
//  @func BOOL | VirtualCopy | Duplicate a virtual memory range (Windows CE Only)
//  @parm LPVOID | lpvDest | address of destination pages 
//  @parm LPVOID | lpvSrc | address of source pages 
//  @parm DWORD | cbSize | number of bytes to copy 
//  @parm DWORD | fdwProtect) | access protection for destination pages 
//  @comm Description unavailable at this time.
//------------------------------------------------------------------------------
BOOL 
DoVirtualCopy(
    LPVOID lpvDest0,     /* address of destination pages */
    LPVOID lpvSrc0,      /* address of source pages */
    DWORD cbSize,       /* # of bytes to copy */
    DWORD fdwProtect    /* access protection for destination pages */
    )
{
    int ixDestBlk, ixSrcBlk = 0;    // keep prefast happy. Don't really need to 
                                    // initialize as it'll not be used
                                    // if bPhys is true.
    int ixPage;
    int ixDestFirB;     /* index of first block in destination region */
    int ixSrcFirB;      /* index of first block in destination region */
    PSECTION pscnDest;
    PSECTION pscnSrc;
    int cpReserved;     /* # of reserved (not commited) pages in region */
    int cpRegion;       /* total # of pages in region */
    int cPages;         /* # of pages to copy */
    ulong ulPFN;        /* page physical frame number */
    ulong ulPgMask;
    BOOL bPhys = FALSE; /* TRUE if mapping physical pages */
    DWORD dwDstAddr = (DWORD) lpvDest0;
    DWORD dwSrcAddr = (DWORD) lpvSrc0;

    /* Verify that the requested regions are within range and within
     * existing reserved memory regions that the client is allowed to
     * access and locate the starting block of both regions.
     */
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CELOG_VirtualCopy(dwDstAddr, dwSrcAddr, cbSize, fdwProtect);
    }
    DEBUGMSG(ZONE_VIRTMEM, (TEXT("VirtualCopy %8.8lx <= %8.8lx size=%lx prot=%lx\r\n"),
            dwDstAddr, dwSrcAddr, cbSize, fdwProtect));
    if (fdwProtect & PAGE_PHYSICAL) {
        bPhys = TRUE;
        fdwProtect &= ~PAGE_PHYSICAL;
    }
    if (!cbSize
        || !dwDstAddr
        || !(ulPgMask = MakePagePerms(fdwProtect, dwDstAddr))
        || IsKernelVa (dwDstAddr)) {
        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
        return FALSE;   /* invalid protection flags, error # already set */
    }

    /* Lockout other changes to the virtual memory state. */
    EnterCriticalSection(&VAcs);
    /* Validate the destination parameters */

    pscnDest = IsSecureVa (dwDstAddr)? &NKSection : SectionTable[(dwDstAddr>>VA_SECTION) & SECTION_MASK];
    if (pscnDest == NULL_SECTION)
        goto invalidParm;
    
    ixDestBlk = (dwDstAddr >> VA_BLOCK) & BLOCK_MASK;
    ixDestFirB = FindFirstBlock(pscnDest, ixDestBlk);
    if (ixDestFirB == UNKNOWN_BASE)
        goto invalidParm;
    /* Verify that all of the destination pages are reserved (not committed). */
    ixPage = (dwDstAddr >> VA_PAGE) & PAGE_MASK;
    cPages = ((dwDstAddr + cbSize + PAGE_SIZE-1) / PAGE_SIZE) - (dwDstAddr / PAGE_SIZE);
    cpReserved = ScanRegion(pscnDest, ixDestFirB, ixDestBlk, ixPage, cPages, 0, dwDstAddr & -(1 << VA_SECTION));
    if (cpReserved != cPages)
        goto invalidParm;
    /* Validate the source address parameters */
    if (bPhys) {
        ulPFN = PFNfrom256(dwSrcAddr);
        if ((dwDstAddr&(PAGE_SIZE-1)) != ((dwSrcAddr<<8) &(PAGE_SIZE-1)))
            goto invalidParm;
    } else if (IsKernelVa (dwSrcAddr)) {
        /* Mapping pages from a physical region. */
        bPhys = TRUE;
        ulPFN = GetPFN(dwSrcAddr);
        if ((dwDstAddr&(PAGE_SIZE-1)) != (dwSrcAddr&(PAGE_SIZE-1)))
            goto invalidParm;
    } else {
        /* Mapping pages from another virtual region. */
        bPhys = FALSE;
        if (!dwSrcAddr || (dwDstAddr&0xFFFFL) != (dwSrcAddr&0xFFFFL))
            goto invalidParm;
        pscnSrc = IsSecureVa (dwSrcAddr)? &NKSection : SectionTable[(dwSrcAddr>>VA_SECTION) & SECTION_MASK];
        if (pscnSrc == NULL_SECTION)
            goto invalidParm;
        ixSrcBlk = (dwSrcAddr >> VA_BLOCK) & BLOCK_MASK;
        ixSrcFirB = FindFirstBlock(pscnSrc, ixSrcBlk);
        if (ixSrcFirB == UNKNOWN_BASE)
            goto invalidParm;
        /* Verify that all of the source pages are committed */
        cpReserved = ScanRegion(pscnSrc, ixSrcFirB, ixSrcBlk, ixPage, cPages, &cpRegion, dwSrcAddr & -(1 << VA_SECTION));
        if (cpReserved || cpRegion != cPages)
            goto invalidParm;
    }
    /* Walk the the page tables mapping the pages in the source region into
     * the destination region. */
    for (; cPages > 0 ; ++ixDestBlk, ++ixSrcBlk) {
        MEMBLOCK *pmbSrc;
        MEMBLOCK *pmbDest = (*pscnDest)[ixDestBlk];
        if (!bPhys)
            pmbSrc = (*pscnSrc)[ixSrcBlk];
#if !HARDWARE_PT_PER_PROC          // MEMBLOCK cannot be shared for CPUs using hardware page table
        if (!bPhys               
            && (ixDestFirB == ixSrcFirB) 
            && (ixPage == 0)
            && ((cPages >= PAGES_PER_BLOCK) || (pmbSrc->aPages[cPages] == BAD_PAGE))
            && (pmbSrc->aPages[0]&PG_PERMISSION_MASK) == ulPgMask) {
            /* Copying an entire block with the same access permissions into
             * the same offset within the two sections.  Share the same MEMBLOCK
             * by bumping the use count on the MEMBLOCK. */
            DEBUGCHK(pmbDest != NULL_BLOCK && pmbDest != RESERVED_BLOCK);
            ++pmbSrc->cUses;
            AddAccess (&pmbSrc->alk, pCurProc->aky);
            // add the original access too
            AddAccess (&pmbSrc->alk, pmbDest->alk);
            MDFreeMemBlock (pmbDest);
            (*pscnDest)[ixDestBlk] = pmbSrc;
            cPages -= PAGES_PER_BLOCK;
#if defined(SH4) && (PAGE_SIZE==4096)
        } else if (bPhys && !ixPage && !(ixDestBlk & 15) && !(ulPFN&0x000FFFFF) && (cPages >= 256)) {
            int loop, loop2;
            DWORD dwSetting = ulPgMask | PG_1M_MASK;
            for (loop = 0; loop < 16; loop++) {
                pmbDest = (*pscnDest)[ixDestBlk+loop];
                for (loop2 = 0; loop2 < 16; loop2++) {
                    pmbDest->aPages[loop2] = dwSetting | ulPFN;
                    ulPFN = NextPFN(ulPFN);
                }
            }
            cPages-=256;
            ixDestBlk+=15;
#endif              
        } else 
#endif
        {
            for ( ; cPages && ixPage < PAGES_PER_BLOCK ; ++ixPage, --cPages) {
                if (bPhys) {
                    DEBUGMSG(ZONE_VIRTMEM,
                        (TEXT("Mapping physical page %8.8lx @%3.3x%x000 perm=%x\r\n"),
                        ulPFN, ixDestBlk, ixPage, ulPgMask));
                    DEBUGCHK(pmbDest->aPages[ixPage] == 0);
                    pmbDest->aPages[ixPage] = ulPFN | ulPgMask;
                    ulPFN = NextPFN(ulPFN);
                } else {
                    DWORD dwOrig = pmbSrc->aPages[ixPage];
                    DEBUGCHK(dwOrig != 0 && dwOrig != BAD_PAGE);
                    ulPFN = PFNfromEntry(dwOrig);
                    DupPhysPage(ulPFN);
                    DEBUGMSG(ZONE_VIRTMEM, (TEXT("Copying page %8.8lx @%3.3x%x000 perm=%x\r\n"),
                            ulPFN, ixDestBlk, ixPage, ulPgMask));
#if defined(SH3) && (PAGE_SIZE==1024)
                    pmbDest->aPages[ixPage] = ulPFN | ulPgMask | (dwOrig & PG_4K_MASK);
#elif defined(SH4) && (PAGE_SIZE==4096)
                    pmbDest->aPages[ixPage] = ulPFN | (ulPgMask & ~PG_1M_MASK) | (dwOrig & PG_1M_MASK);
#else
                    pmbDest->aPages[ixPage] = ulPFN | ulPgMask;
#endif
                }
            }
        }
        ixPage = 0;            /* start with first page of next block */
    }
    LeaveCriticalSection(&VAcs);
    return TRUE;
invalidParm:
    LeaveCriticalSection(&VAcs);
    DEBUGMSG(ZONE_VIRTMEM, (TEXT("VirtualCopy failed.\r\n")));
    KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
    return FALSE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_VirtualCopy(
    LPVOID lpvDest,
    LPVOID lpvSrc,
    DWORD cbSize,
    DWORD fdwProtect
    ) 
{
    TRUSTED_API (L"SC_VirtualCopy", FALSE);

    return DoVirtualCopy(lpvDest, lpvSrc, cbSize, fdwProtect);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
VirtualSetPages (
    LPVOID pvAddr,      // virtual address
    DWORD  nPages,      // # of pages
    LPDWORD pPages,     // the pages
    DWORD  fdwProtect   // protection
    )
{
    DWORD dwAddr  = (DWORD) pvAddr;
    int   ixBlock = (dwAddr >> VA_BLOCK) & BLOCK_MASK;      // block #
    int   ixPage  =  (dwAddr >> VA_PAGE) & PAGE_MASK;       // page #
    DWORD ulPgMask = MakePagePerms (fdwProtect, dwAddr);
    
    int         ixFirB;     // first memblock of the reservation
    PSECTION    pscn;
    MEMBLOCK    *pmb;
    ACCESSKEY   aky;        
    DWORD       dwBase;

    // internal function. parameters are assumed to be always valid
    DEBUGCHK (pPages && nPages && pvAddr);

    EnterCriticalSection (&VAcs);

    if (0x80000000 & dwAddr) {
        dwBase = SECURE_VMBASE;
        pscn = &NKSection;
        aky = ProcArray[0].aky;
    } else {
        dwBase  = dwAddr & (SECTION_MASK<<VA_SECTION);
        pscn = SectionTable[(dwAddr>>VA_SECTION) & SECTION_MASK];
        aky = pCurProc->aky;
    }

    ixFirB = FindFirstBlock (pscn, ixBlock);
    DEBUGCHK (UNKNOWN_BASE != ixFirB);

    for (ixPage = (dwAddr >> VA_PAGE) & PAGE_MASK ; nPages; ixPage = 0, ixBlock ++) {

        // allocate memblock if necessary        
        if ((pmb = (*pscn)[ixBlock]) == RESERVED_BLOCK) {
            if (!(pmb = MDAllocMemBlock (dwBase, ixBlock))) {
                break;
            }
            pmb->alk = aky;
            pmb->flags = (*pscn)[ixFirB]->flags;
            pmb->cUses = 1;
            pmb->ixBase = ixFirB;
            (*pscn)[ixBlock] = pmb;
            DEBUGMSG(ZONE_VIRTMEM,
                    (TEXT("VirtualSetPages: created pmb=%8.8lx (%x)\r\n"), pmb, ixBlock));
        }

        for ( ; nPages && (ixPage < PAGES_PER_BLOCK); ixPage ++, nPages --, pPages ++) {
            DEBUGCHK (!pmb->aPages[ixPage]);
            
            pmb->aPages[ixPage] = GetPFN(*pPages) | ulPgMask;
            DEBUGMSG(ZONE_VIRTMEM,
                (TEXT("VirtualSetPages: Mapping physical page %8.8lx @%3.3x%x000 perm=%x\r\n"),
                GetPFN(*pPages), ixBlock, ixPage, ulPgMask));
        }
    }
    LeaveCriticalSection (&VAcs);

    return 0 == nPages;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
NKVirtualSetAttributes(
    LPVOID lpvAddress,
    DWORD cbSize,
    DWORD dwNewFlags,
    DWORD dwMask,
    LPDWORD lpdwOldFlags
    ) 
{
    int ixBlock;
    int ixPage;
    int ixFirB;         /* index of first block in region */
    PSECTION pscn;
    int cPages;         /* # of pages to adjust */
    MEMBLOCK *pmb;
    DWORD dwAddr = (DWORD) lpvAddress;
    DWORD entry;
    BOOL  fRet = FALSE;
    
    TRUSTED_API (L"NKVirtualSetAttributes", FALSE);
    
    DEBUGMSG(ZONE_VIRTMEM, (TEXT("NKVirtualSetAttributes (%8.8lx, %lx, %lx, %lx, %lx)\r\n"),
            lpvAddress, cbSize, dwNewFlags, dwMask, lpdwOldFlags));

    // validate parameters
    if (((int)cbSize > 0) 
        && (dwAddr >= 0x10000) 
        && !IsKernelVa (dwAddr)) {

        DEBUGMSG (dwMask & ~PG_CHANGEABLE, 
            (L"WARNING! VirtualSetAttributes: Changing bits that's not changable! dwMask = %8.8lx\r\n", dwMask));

        ixBlock = (dwAddr >> VA_BLOCK) & BLOCK_MASK;
        ixPage = (dwAddr >> VA_PAGE) & PAGE_MASK;
        cPages = ((dwAddr + cbSize + PAGE_SIZE-1) / PAGE_SIZE) - (dwAddr / PAGE_SIZE);

        EnterCriticalSection (&VAcs);

        if (((pscn = IsSecureVa (dwAddr)? &NKSection : SectionTable[(dwAddr>>VA_SECTION) & SECTION_MASK]) != NULL_SECTION)
            && ((ixFirB = FindFirstBlock(pscn, ixBlock)) != UNKNOWN_BASE)
            && !ScanRegion(pscn, ixFirB, ixBlock, ixPage, cPages, 0, dwAddr & -(1 << VA_SECTION))) {

            if (lpdwOldFlags) {
                *lpdwOldFlags = (*pscn)[ixBlock]->aPages[ixPage];
            }

            // if dwMask is 0, just return the value in the 1st page. Otherwise update the entries.
            if (dwMask) {

                for (; cPages ; ++ixBlock) {
                    pmb = (*pscn)[ixBlock];
                    do {
                        entry = pmb->aPages[ixPage];
                        DEBUGCHK(entry != 0 && (entry != BAD_PAGE));

                        entry = (entry & ~dwMask) | (dwNewFlags & dwMask);
                        DEBUGMSG(ZONE_VIRTMEM, (TEXT("Changing page table entry at section %8.8lx, page %8.8lx (%8.8lx => %8.8lx)\r\n"),
                                ixBlock, ixPage, pmb->aPages[ixPage], entry));

                        pmb->aPages[ixPage] = entry;
                    } while (--cPages && ++ixPage < PAGES_PER_BLOCK);
                    ixPage = 0;            /* start with first page of next block */
                }
            }
            fRet = TRUE;
        }

        LeaveCriticalSection(&VAcs);
    }
    
    if (!fRet) {
        DEBUGMSG(ZONE_VIRTMEM, (TEXT("NKVirtualSetAttributes failed.\r\n")));
        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
    }
    return fRet;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_VirtualProtect(
    LPVOID lpvAddress,      /* address of region of committed pages */
    DWORD cbSize,           /* size of the region */
    DWORD fdwNewProtect,    /* desired access protection */
    PDWORD pfdwOldProtect   /* address of variable to get old protection */
    )
{
    int ixBlock;
    int ixPage;
    int ixFirB;         /* index of first block in region */
    PSECTION pscn;
    int cPages;         /* # of pages to adjust */
    ulong ulPgMask;     /* page permissions */
    MEMBLOCK *pmb;
    BOOL bRet = TRUE;
    DWORD dwAddr = (DWORD) lpvAddress;

    DEBUGMSG(ZONE_VIRTMEM, (TEXT("VirtualProtect @%8.8lx size=%lx fdwNewProtect=%lx\r\n"),
            lpvAddress, cbSize, fdwNewProtect));
    if ((ulPgMask = MakePagePerms(fdwNewProtect, dwAddr)) == 0)
        return FALSE;       /* invalid protection flags, error # already set */

    /* Verify that the requested region is within range and within an
     * existing reserved memory region that the client is allowed to
     * access and locate the starting block of the region.
     */
    /* Lockout other changes to the virtual memory state. */
    EnterCriticalSection(&VAcs);
    if (cbSize == 0 || pfdwOldProtect == 0)
        goto invalidParm;
    if (!dwAddr || IsKernelVa (dwAddr) || IsStackVA ((DWORD)lpvAddress, cbSize))
        goto invalidParm;
    pscn = IsSecureVa (dwAddr)? &NKSection : SectionTable[(dwAddr>>VA_SECTION) & SECTION_MASK];
    if (pscn == NULL_SECTION)
        goto invalidParm;
    ixBlock = (dwAddr >> VA_BLOCK) & BLOCK_MASK;
    if ((ixFirB = FindFirstBlock(pscn, ixBlock)) == UNKNOWN_BASE)
        goto invalidParm;
    /* Verify that all of the pages within the specified range belong to the
     * same VirtualAlloc region and are committed.
     *
     *  (pscn) = ptr to section array
     *  (ixBlock) = index of block containing the first page of dwAddr
     *  (ixFirB) = index of first block in the region
     */
    ixPage = (dwAddr >> VA_PAGE) & PAGE_MASK;
    cPages = ((dwAddr + cbSize + PAGE_SIZE-1) / PAGE_SIZE)
            - (dwAddr / PAGE_SIZE);

    if (ScanRegion(pscn, ixFirB, ixBlock, ixPage, cPages, 0, dwAddr & -(1 << VA_SECTION)) != 0)
        goto invalidParm;

    /* Walk the page tables to set the new page permissions */
    *pfdwOldProtect = ProtectFromPerms((*pscn)[ixBlock]->aPages[ixPage], dwAddr);
    for (; cPages ; ++ixBlock) {
        pmb = (*pscn)[ixBlock];
        do {
            if (!pmb->cLocks || IsPageWritable (ulPgMask)) {
                DEBUGCHK(pmb->aPages[ixPage] != 0 && pmb->aPages[ixPage] != BAD_PAGE);
                DEBUGMSG(ZONE_VIRTMEM, (TEXT("Changing perms @%3.3x%x000 perm=%x\r\n"),
                        ixBlock, ixPage, ulPgMask));
#if defined(SH3) && (PAGE_SIZE == 1024)
                pmb->aPages[ixPage] = (pmb->aPages[ixPage] & ~(PG_PERMISSION_MASK & ~PG_4K_MASK)) | ulPgMask;
#elif defined(SH4) && (PAGE_SIZE == 4096)
                pmb->aPages[ixPage] = (pmb->aPages[ixPage] & ~(PG_PERMISSION_MASK & ~PG_1M_MASK)) | (ulPgMask & ~PG_1M_MASK);
#else
                pmb->aPages[ixPage] = (pmb->aPages[ixPage] & ~PG_PERMISSION_MASK) | ulPgMask;
#endif
            } else {
                DEBUGMSG(1,(L"VirtualProtect: Cannot reduce access at %8.8lx, lock count %d\r\n",
                    (dwAddr & (SECTION_MASK<<VA_SECTION)) + (ixBlock<<VA_BLOCK) + (ixPage<<VA_PAGE), pmb->cLocks));
                if (cbSize == PAGE_SIZE)
                    bRet = FALSE;
            }
        } while (--cPages && ++ixPage < PAGES_PER_BLOCK);
        ixPage = 0;            /* start with first page of next block */
    }

    // Since page permissions have been modified, it is necessary to flush the TLB.
    InvalidateRange(lpvAddress, cbSize);
    LeaveCriticalSection(&VAcs);
    return bRet;
invalidParm:
    LeaveCriticalSection(&VAcs);
    DEBUGMSG(ZONE_VIRTMEM, (TEXT("VirtualProtect failed.\r\n")));
    KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
    return FALSE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
SC_VirtualQuery(
    LPVOID lpvAddress,                      /* address of region */
    PMEMORY_BASIC_INFORMATION pmbiBuffer,   /* address of information buffer */
    DWORD cbLength                          /* size of buffer */
    )
{
    int ixBlock;
    int ixPage;
    int ixFirB;         /* index of first block in region */
    PSECTION pscn;
    MEMBLOCK *pmb;
    int cPages;
    ulong ulPgPerm;
    DWORD dwAddr = (DWORD) lpvAddress;
    if (cbLength < sizeof(MEMORY_BASIC_INFORMATION)) {
        KSetLastError(pCurThread, ERROR_BAD_LENGTH);
        return 0;
    }
    DEBUGMSG(ZONE_VIRTMEM, (TEXT("VirtualQuery @%8.8lx buf=%8.8lx cbLen=%d\r\n"),
            lpvAddress, pmbiBuffer, cbLength));
    
    if (!pmbiBuffer || !dwAddr || IsKernelVa (dwAddr))
        goto invalidParm;
    pscn = IsSecureVa (dwAddr)? &NKSection : SectionTable[(dwAddr>>VA_SECTION) & SECTION_MASK];
    if (pscn == NULL_SECTION)
        goto invalidParm;
    ixPage = (dwAddr >> VA_PAGE) & PAGE_MASK;
    ixBlock = (dwAddr >> VA_BLOCK) & BLOCK_MASK;

    // need to grab VAcs since we might be preempted and the memblock can get freed
    EnterCriticalSection (&VAcs);
    
    if ((pmb = (*pscn)[ixBlock]) == NULL_BLOCK ||
        (pmb != RESERVED_BLOCK && pmb->aPages[ixPage] == BAD_PAGE) ||
        (ixFirB = FindFirstBlock(pscn, ixBlock)) == UNKNOWN_BASE) {
        /* The given page is in a free region, walk the section to compute
         * the size of the region */
        cPages = PAGES_PER_BLOCK - ixPage;
        while (++ixBlock < BLOCK_MASK+1 && (*pscn)[ixBlock] == NULL_BLOCK)
            cPages += PAGES_PER_BLOCK;
        pmbiBuffer->AllocationBase = 0;
        pmbiBuffer->State = MEM_FREE;
        pmbiBuffer->AllocationProtect = PAGE_NOACCESS;
        pmbiBuffer->Protect = PAGE_NOACCESS;
        pmbiBuffer->Type = 0;
    } else {
        cPages = 0;
        pmbiBuffer->AllocationProtect = PAGE_NOACCESS;
        pmbiBuffer->Type = MEM_PRIVATE;
        if (pmb == RESERVED_BLOCK || pmb->aPages[ixPage] == 0) {
            /* Count reserved pages */
            pmbiBuffer->State = MEM_RESERVE;
            pmbiBuffer->Protect = PAGE_NOACCESS;
            do {
                if (pmb == RESERVED_BLOCK)
                    cPages += PAGES_PER_BLOCK - ixPage;
                else {
                    pmbiBuffer->Type = AllocationType[pmb->flags & MB_FLAG_PAGER_TYPE];
                    do {
                        if (pmb->aPages[ixPage] != 0)
                            goto allCounted;
                        ++cPages;
                    } while (++ixPage < PAGES_PER_BLOCK);
                }
                ixPage = 0;
            } while (++ixBlock < BLOCK_MASK+1 &&
                     ((pmb = (*pscn)[ixBlock]) == RESERVED_BLOCK ||
                      (pmb != NULL_BLOCK && pmb->ixBase == ixFirB)));
        } else {
            /* Count committed pages */
            pmbiBuffer->State = MEM_COMMIT;
            ulPgPerm = pmb->aPages[ixPage] & PG_PERMISSION_MASK;
            pmbiBuffer->Protect = ProtectFromPerms(pmb->aPages[ixPage], dwAddr);
            pmbiBuffer->Type = AllocationType[pmb->flags & MB_FLAG_PAGER_TYPE];
            do {
                do {
                    ulong ulPgInfo = pmb->aPages[ixPage];
                    if (ulPgInfo == 0 || ulPgInfo == BAD_PAGE
                            || (ulPgInfo&PG_PERMISSION_MASK) != ulPgPerm)
                        goto allCounted;
                    ++cPages;
                } while (++ixPage < PAGES_PER_BLOCK);
                ixPage = 0;
            } while (++ixBlock < BLOCK_MASK+1 &&
                     (pmb = (*pscn)[ixBlock]) != NULL_BLOCK &&
                     pmb != RESERVED_BLOCK && pmb->ixBase == ixFirB);
        }
allCounted:
        pmbiBuffer->AllocationBase =
                    (LPVOID)((dwAddr&(SECTION_MASK<<VA_SECTION)) |
                             (ixFirB << VA_BLOCK));
    }

    LeaveCriticalSection (&VAcs);
    
    pmbiBuffer->RegionSize = (DWORD)cPages << VA_PAGE;
    pmbiBuffer->BaseAddress = (LPVOID)(dwAddr & ~(PAGE_SIZE-1));
    return sizeof(MEMORY_BASIC_INFORMATION);
invalidParm:
    DEBUGMSG(ZONE_VIRTMEM, (TEXT("VirtualQuery failed.\r\n")));
    KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
    return 0;
}

BOOL IsCommittedSecureSlot (DWORD addr)
{
    // addr MUST be a slot-less address
    DEBUGCHK ((addr < (2 << VA_SECTION)) || IsInResouceSection (addr));
    
    if (addr < (1 << VA_SECTION)) {
        MEMBLOCK *pmb = NKSection[(addr >> VA_BLOCK) & BLOCK_MASK];
        if ((NULL_BLOCK != pmb) && (RESERVED_BLOCK != pmb)) {
            ulong ulPgInfo = pmb->aPages[(addr >> VA_PAGE) & PAGE_MASK];
            return ulPgInfo && (BAD_PAGE != ulPgInfo);
        }
    }
    return FALSE;
}

//------------------------------------------------------------------------------
// lock pages in the same slot.
//------------------------------------------------------------------------------
DWORD 
DoLockPagesInSlot (
    DWORD  dwAddr,              /* address of region of committed pages */
    DWORD  cbSize,              /* size of the region */
    PDWORD pPFNs,               /* address of array to receive real PFN's */
    int    fOptions             /* options: see LOCKFLAG_* in kernel.h */
    )
{
    int ixBlock;
    int ixPage;
    int ixFirB;         /* index of first block in region */
    int ixStart;        /* index of start block of lock region */
    PSECTION pscn;
    int cPages;         /* # of pages to adjust */
    ulong ulBase;       /* base virtual address */
    MEMBLOCK *pmb;
    DWORD dwErr = 0;
    BOOL  fLockWrite = (fOptions & LOCKFLAG_WRITE);
    
    DEBUGMSG(ZONE_VIRTMEM, (TEXT("LockPages @%8.8lx size=%lx pPFNs=%lx options=%x\r\n"),
            dwAddr, cbSize, pPFNs, fOptions));

    // dwAddr and cbSize are both page aligned
    DEBUGCHK (!(dwAddr & (PAGE_SIZE-1)));
    DEBUGCHK (!(cbSize & (PAGE_SIZE-1)));

    // find the section pointer of the address    
    if (IsSecureVa(dwAddr)) {
        ulBase = SECURE_VMBASE;
        pscn = &NKSection;
    } else {
        ulBase = dwAddr & (SECTION_MASK << VA_SECTION);
        pscn = SectionTable[ulBase>>VA_SECTION];
        if (pscn == NULL_SECTION)
            return ERROR_INVALID_PARAMETER;
    }

    // find the memblock and the beginning of the reservation
    ixBlock = (dwAddr >> VA_BLOCK) & BLOCK_MASK;
    if ((ixFirB = FindFirstBlock (pscn, ixBlock)) == UNKNOWN_BASE)
        return ERROR_INVALID_PARAMETER;

    /* Verify that all of the pages within the specified range belong to the
     * same VirtualAlloc region and are committed.
     *
     *  (pscn) = ptr to section array
     *  (ixBlock) = index of block containing the first page of dwAddr
     *  (ixFirB) = index of first block in the region
     */
    ixPage = (dwAddr >> VA_PAGE) & PAGE_MASK;
    cPages = (cbSize >> VA_PAGE);

    if (ScanRegion (pscn, ixFirB, ixBlock, ixPage, cPages, 0, ulBase) == -1)
        return ERROR_INVALID_PARAMETER;
    
    /* Walk the page tables to check that all pages are present and to
     * increment the lock count for each MEMBLOCK in the region.
     */
    ixStart = ixBlock;
    for (; cPages && !dwErr; ++ixBlock) {
        pmb = (*pscn)[ixBlock];
        if (!(fOptions & LOCKFLAG_QUERY_ONLY)) {
            ++pmb->cLocks;
            DEBUGMSG(ZONE_VIRTMEM, (TEXT("Locking pages @%8.8x LC=%d\r\n"), 
                    ulBase | (ixBlock<<VA_BLOCK), pmb->cLocks));
        }
        do {

            DWORD dwEntry = pmb->aPages[ixPage]; 
            DEBUGCHK (BAD_PAGE != dwEntry);

            if (!dwEntry) {
                if (pmb->flags&MB_FLAG_AUTOCOMMIT) {
                    // a yet to committed auto-commit page, calculate max we can commit
                    int nPages = PAGES_PER_BLOCK - ixPage;
                    if (nPages > cPages)
                        nPages = cPages;
                    // commit the pages
                    if (DoVirtualAlloc((void*)dwAddr, nPages*PAGE_SIZE, MEM_COMMIT,PAGE_READWRITE, MEM_NOSTKCHK, 0)) {
                        cPages -= nPages;
                    } else {
                        dwErr = ERROR_OUTOFMEMORY;
                    }
                    break;
                }
            }

            if (!dwEntry || (fLockWrite && !IsPageWritable (dwEntry))) {

                BOOL bPaged;

                LeaveCriticalSection (&VAcs);

                // try to page-in r/w if lock write, or paging in the 1st time
                bPaged = ProcessPageFault (!dwEntry || fLockWrite, dwAddr);

                // try to page-in read-only if paging inthe first time, couldn't page-in r/w, and the lock flag is r/o
                if (!bPaged && !dwEntry && !fLockWrite) {
                    bPaged = ProcessPageFault (FALSE, dwAddr);
                }
                EnterCriticalSection (&VAcs);

                if (!bPaged) {
                    dwErr = ERROR_NOACCESS;
                    break;
                }
                    
            }else if (!IsPageReadable (dwEntry)) { 
                dwErr = ERROR_NOACCESS;
                break;
            }

            // move to next page
            dwAddr += PAGE_SIZE;
            if (pPFNs)
                *pPFNs++ = PFNfromEntry(pmb->aPages[ixPage]);
        } while (--cPages && ++ixPage < PAGES_PER_BLOCK);
        ixPage = 0;            /* start with first page of next block */
    }

    if (dwErr && !(fOptions & LOCKFLAG_QUERY_ONLY)) {
        /* Unable to page in a page or this thread doesn't have the desired access
         * to a page in the range. Back out any locks which have been set.
         */
        do {
            --pmb->cLocks;
            DEBUGMSG(ZONE_VIRTMEM, (TEXT("Restoring lock count @%8.8x LC=%d\r\n"), 
                    ulBase | (ixBlock<<VA_BLOCK), pmb->cLocks));
        } while (ixBlock != ixStart && (pmb = (*pscn)[--ixBlock]));
    }

    return dwErr;
}

BOOL 
DoLockPages(
    LPVOID lpvAddress,          /* address of region of committed pages */
    DWORD  cbSize,              /* size of the region */
    PDWORD pPFNs,               /* address of array to receive real PFN's */
    int    fOptions             /* options: see LOCKFLAG_* in kernel.h */
    )
{

    DWORD cbInSlot;
    DWORD dwStartAddr = (DWORD) lpvAddress & -PAGE_SIZE;
    DWORD dwEndAddr = ((DWORD) lpvAddress + cbSize + PAGE_SIZE - 1) & -PAGE_SIZE;
    DWORD dwAddr = dwStartAddr;
    DWORD dwErr;

    if (IsKernelVa(lpvAddress)) {
        // kernel addresses, don't need to lock, but need to update if pPFNs is it's not NULL.
        if (pPFNs) {
            DWORD ulPFN;
            for (ulPFN = GetPFN (dwAddr); dwAddr < dwEndAddr; dwAddr += PAGE_SIZE, pPFNs ++) {
                *pPFNs = ulPFN;
                ulPFN = NextPFN (ulPFN);
            }
        }
        return TRUE;
    }
    
    // validate parameters
    if (!cbSize || !lpvAddress || IsKernelVa (dwEndAddr-1)) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    cbSize = dwEndAddr - dwStartAddr;

    EnterCriticalSection (&VAcs);

    while (TRUE) {

        // figure out the size for the slot
        if ((dwAddr >> VA_SECTION) == ((dwAddr + cbSize - 1) >> VA_SECTION)) {
            cbInSlot = cbSize;
        } else {
            cbInSlot = (((dwAddr >> VA_SECTION) + 1) << VA_SECTION) - dwAddr;
        }

        // lock the pages in the slot
        dwErr = DoLockPagesInSlot (dwAddr, cbInSlot, pPFNs, fOptions);

        // break if done or error occurs
        if (dwErr || !(cbSize -= cbInSlot)) {
            break;
        }

        // move to next slot

        // increment pPFNs if non-NULL
        if (pPFNs) {
            pPFNs  += cbInSlot >> VA_PAGE; // incremented by # of pages
        }
        
        //update address
        dwAddr += cbInSlot;
    }

    // unlock the pages we locked if error occurs
    if (dwErr) {
        if (cbSize = (dwAddr - dwStartAddr)) {
            DoUnlockPages ((LPCVOID) dwStartAddr, cbSize);
        }
        DEBUGMSG (ZONE_VIRTMEM, (L"LockPages failed, dwErr = %8.8lx\r\n", dwErr));
        KSetLastError (pCurThread, dwErr);
    }

    LeaveCriticalSection (&VAcs);

    return !dwErr;

}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_LockPages(
    LPVOID lpvAddress,
    DWORD cbSize,
    PDWORD pPFNs,
    int fOptions
    ) 
{
    TRUSTED_API (L"SC_LockPages", FALSE);

    return DoLockPages (lpvAddress, cbSize, pPFNs, fOptions);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
DoUnlockPagesInSlot(
    DWORD dwAddr,     /* address of region of committed pages */
    DWORD cbSize            /* size of the region */
    )
{
    int ixPage;
    int ixBlock;
    int ixFirB;         /* index of first block in region */
    int ixLastBlock;    /* last block to be unlocked */
    PSECTION pscn;
    int cPages;         /* # of pages to adjust */
    MEMBLOCK *pmb;

    // dwAddr and cbSize are both page aligned
    DEBUGCHK (!(dwAddr & (PAGE_SIZE-1)));
    DEBUGCHK (!(cbSize & (PAGE_SIZE-1)));

    DEBUGMSG(ZONE_VIRTMEM, (TEXT("UnlockPages @%8.8lx size=%lx\r\n"),
            dwAddr, cbSize));

    pscn = IsSecureVa(dwAddr)? &NKSection : SectionTable[(dwAddr>>VA_SECTION) & SECTION_MASK];
    if (pscn == NULL_SECTION)
        return ERROR_INVALID_PARAMETER;
    
    ixBlock = (dwAddr >> VA_BLOCK) & BLOCK_MASK;
    if ((ixFirB = FindFirstBlock(pscn, ixBlock)) == UNKNOWN_BASE)
        return ERROR_INVALID_PARAMETER;
    /* Verify that all of the pages within the specified range belong to the
     * same VirtualAlloc region and are committed.
     *
     *  (pscn) = ptr to section array
     *  (ixBlock) = index of block containing the first page of dwAddr
     *  (ixFirB) = index of first block in the region
     */
    ixPage = (dwAddr >> VA_PAGE) & PAGE_MASK;
    cPages = (cbSize >> VA_PAGE);

    if (ScanRegion(pscn, ixFirB, ixBlock, ixPage, cPages, 0, dwAddr & -(1 << VA_SECTION)) == -1)
        return ERROR_INVALID_PARAMETER;
    
    /* Walk the page tables to decrement the lock count for 
     * each MEMBLOCK in the region.
     */
    ixLastBlock = ixBlock + ((ixPage+cPages+PAGES_PER_BLOCK-1) >> (VA_BLOCK-VA_PAGE));
    for ( ; ixBlock < ixLastBlock ; ++ixBlock) {
        pmb = (*pscn)[ixBlock];
        if (pmb->cLocks)
            --pmb->cLocks;
        else
            DEBUGCHK(0);
        DEBUGMSG(ZONE_VIRTMEM, (TEXT("Unlocking pages @%8.8x LC=%d\r\n"), 
                (dwAddr&(SECTION_MASK<<VA_SECTION)) | (ixBlock<<VA_BLOCK),
                pmb->cLocks));
    }
    return 0;

}

BOOL 
DoUnlockPages (
    LPVOID lpvAddress,          /* address of region of committed pages */
    DWORD  cbSize               /* size of the region */
    )
{

    DWORD cbInSlot;
    DWORD dwStartAddr = (DWORD) lpvAddress & -PAGE_SIZE;
    DWORD dwEndAddr = ((DWORD) lpvAddress + cbSize + PAGE_SIZE - 1) & -PAGE_SIZE;
    DWORD dwAddr;
    DWORD dwErr = 0;

    if (IsKernelVa(lpvAddress))
        return TRUE;
    
    // validate parameters
    if (!cbSize || !lpvAddress || IsKernelVa (dwEndAddr-1)) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    cbSize = dwEndAddr - dwStartAddr;

    EnterCriticalSection (&VAcs);

    for (dwAddr = dwStartAddr; cbSize && !dwErr; cbSize -= cbInSlot, dwAddr += cbInSlot) {

        // figure out the size for the slot
        if ((dwAddr >> VA_SECTION) == ((dwAddr + cbSize - 1) >> VA_SECTION)) {
            cbInSlot = cbSize;
        } else {
            cbInSlot = (((dwAddr >> VA_SECTION) + 1) << VA_SECTION) - dwAddr;
        }

        // lock the pages in the slot
        dwErr = DoUnlockPagesInSlot (dwAddr, cbInSlot);

    }

    LeaveCriticalSection (&VAcs);

    if (dwErr) {
        DEBUGMSG(ZONE_VIRTMEM, (TEXT("UnlockPages failed, dwErr = %8.8lx.\r\n"), dwErr));
        KSetLastError (pCurThread, dwErr);
    }


    return !dwErr;

}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_UnlockPages(
    LPVOID lpvAddress,
    DWORD cbSize
    ) 
{
    TRUSTED_API (L"SC_UnlockPages", FALSE);
    return DoUnlockPages (lpvAddress, cbSize);
}

LPBYTE GrabFirstPhysPage(DWORD dwCount);

extern void SetOOMEvt (void);


//------------------------------------------------------------------------------
//
//  AutoCommit - auto-commit a page
//  
//   This function is called for a TLB miss due to a memory load or store.
//   It is invoked from the general exception handler on the kernel stack.
//   If the faulting page, is within an auto-commit region then a new page
//   will be allocated and committed.
//  
//------------------------------------------------------------------------------
BOOL 
AutoCommit(
    ulong addr
    ) 
{
    register MEMBLOCK *pmb;
    PSECTION pscn;
    int     ixBlock, ixPage;
    ulong   ulPFN;
    LPBYTE  pMem;
    DWORD   loop;
    DEBUGMSG(ZONE_VIRTMEM, (TEXT("Auto-committing %8.8lx\n"), addr));
    if (IsKernelVa(addr)) {
        return FALSE;
    }
    ixPage = (addr>>VA_PAGE) & PAGE_MASK;
    ixBlock = (addr>>VA_BLOCK) & BLOCK_MASK;
    pscn = IsSecureVa(addr)? &NKSection : SectionTable[addr>>VA_SECTION];

    if ((pscn != NULL_SECTION)
        && ((pmb = (*pscn)[ixBlock]) != NULL_BLOCK)
        && (pmb != RESERVED_BLOCK)
        && (pmb->flags&MB_FLAG_AUTOCOMMIT) && !pmb->aPages[ixPage]) {
        for (loop = 0; loop < 20; loop++) {
            //
            // We don't want to let AutoCommit deplete all physical memory. 
            // Yes, the thread needing the memory committed is going to fault
            // unexpectedly if we can't allocate, so we'll loop for 2 seconds
            // checking for available RAM. We need to reserve a small amount
            // so that DemandCommit can allocate stack for the exception
            // handlers if required.
            //
            if ((PageFreeCount > 5) && (pMem = (LPBYTE)KCall((PKFN)GrabFirstPhysPage,1))) {
                ulPFN = GetPFN(pMem);
                /* Map the new page in as read/write. */
                DEBUGMSG(ZONE_VIRTMEM, (TEXT("Auto-committing2 %8.8lx @%8.8lx\r\n"),
                        ulPFN, addr & ~(PAGE_SIZE-1)));
#ifdef ARM
                pmb->aPages[ixPage] = ulPFN | ((pscn == &NKSection)? PG_KRN_READ_WRITE : PG_READ_WRITE);
#else
                pmb->aPages[ixPage] = ulPFN | PG_READ_WRITE;
#endif
                //
                // Notify OOM thread if we're low on memory.
                //
                if (GwesOOMEvent && (PageFreeCount < GwesCriticalThreshold)) {
                    SetOOMEvt ();
                }
                return TRUE;
            }
            //
            // Notify OOM thread if we're low on memory.
            //
            if (GwesOOMEvent && (PageFreeCount < GwesCriticalThreshold)) {
                SetOOMEvt ();
            }
            Sleep(100);
        }
        RETAILMSG(1, (TEXT("WARNING (low memory) : Failed auto-commit of 0x%08X @ %d free pages\r\n"), addr, PageFreeCount));
        DEBUGCHK(0);
    }
    return FALSE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
GuardCommit(
    ulong addr
    ) 
{
    if (!IsKernelVa (addr)) {
        register MEMBLOCK *pmb;
        PSECTION pscn;
        int ixBlock, ixPage;
        DEBUGMSG(ZONE_VIRTMEM, (TEXT("GuardCommit %8.8lx\n"), addr));
        ixPage = addr>>VA_PAGE & PAGE_MASK;
        ixBlock = addr>>VA_BLOCK & BLOCK_MASK;
        pscn = IsSecureVa(addr)? &NKSection : SectionTable[addr>>VA_SECTION];
        if ((pscn != NULL_SECTION)
            && ((pmb = (*pscn)[ixBlock]) != NULL_BLOCK)
            && (pmb != RESERVED_BLOCK)
            && (pmb->aPages[ixPage] != BAD_PAGE)
            && (pmb->aPages[ixPage] & ~PG_PERMISSION_MASK)) {
            pmb->aPages[ixPage] |= PG_VALID_MASK;
#ifdef MUST_CLEAR_GUARD_BIT
            pmb->aPages[ixPage] &= ~PG_GUARD;
#endif
        }
    }
}

ERRFALSE(MB_PAGER_FIRST==1);
FN_PAGEIN * const PageInFuncs[MB_MAX_PAGER_TYPE] = { LoaderPageIn, MappedPageIn };

extern SECTION NKSection;

//------------------------------------------------------------------------------
//   ProcessPageFault - general page fault handler
//  
//   This function is called from the exception handling code to attempt to handle
//  a fault due to a missing page. If the function is able to resolve the fault,
//  it returns TRUE.
//  
//  Environment:
//    Kernel mode, preemtible, running on the thread's stack.
//  
//------------------------------------------------------------------------------
BOOL 
ProcessPageFault(
    BOOL bWrite,
    ulong addr0
    ) 
{
    DWORD dwLast;
    register MEMBLOCK *pmb;
    PSECTION pscn;
    int ixBlock, ixPage, pageresult, retrycnt = 0;
    uint pagerType = MB_PAGER_NONE;
    BOOL bRet = FALSE;
    DWORD addr = addr0;
    DEBUGMSG(ZONE_PAGING, (TEXT("ProcessPageFault: %a @%8.8lx, InSysCall() = %d\r\n"), bWrite?"Write":"Read", addr, InSysCall()));
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CELOG_SystemPage(addr, bWrite);
    }
        
    DEBUGCHK (!InSysCall ());

    // on ARM and x86, it's possible that we fault on an unmapped kernel address. Need to fail
    // gracefully in this case.
    if (IsKernelVa(addr)) {
        DEBUGMSG (ZONE_PAGING, (TEXT("PorcessPageFault Failed: Unmapped address %8.8lx\n"), addr));
        return FALSE;
    }

    ixPage = addr>>VA_PAGE & PAGE_MASK;
    ixBlock = addr>>VA_BLOCK & BLOCK_MASK;

retryPagein:
    EnterCriticalSection(&VAcs);
    pscn = IsSecureVa (addr)? &NKSection : SectionTable[addr >> VA_SECTION];
    if ((pscn != NULL_SECTION) &&
        ((pmb = (*pscn)[ixBlock]) != NULL_BLOCK)) {

        if (pmb == RESERVED_BLOCK) {
            pmb = (*pscn)[FindFirstBlock(pscn, ixBlock)];
            pagerType = pmb->flags & MB_FLAG_PAGER_TYPE;
        } else {
            if (pmb->aPages[ixPage] & PG_VALID_MASK) {
                if (!bWrite || IsPageWritable(pmb->aPages[ixPage]))
                    bRet = TRUE;
                else
                    pagerType = pmb->flags & MB_FLAG_PAGER_TYPE;
            } else if (pmb->aPages[ixPage] != BAD_PAGE)
                pagerType = pmb->flags & MB_FLAG_PAGER_TYPE;
        }
        DEBUGCHK(pagerType <= MB_MAX_PAGER_TYPE);
        if (TestAccess(&pmb->alk, &CurAKey)) {
            if (pagerType != MB_PAGER_NONE) {
                DEBUGMSG(ZONE_PAGING, (TEXT("Calling Page In Function. pagerType=%d\r\n"), pagerType));
                LeaveCriticalSection(&VAcs);
                dwLast = KGetLastError(pCurThread);
                pageresult = PageInFuncs[pagerType-MB_PAGER_FIRST](bWrite, addr0);
                KSetLastError(pCurThread,dwLast);
                if (pageresult == PAGEIN_RETRY) {
                    DEBUGMSG(ZONE_PAGING, (TEXT("Page function returned 'retry' %d\r\n"),pageresult));
                    Sleep(250); // arbitrary number, should be > 0 but pretty small
                    bRet = FALSE;
                    pagerType = MB_PAGER_NONE;
                    // arbitrary number, total of 2 second "timeout" on pagein
                    // Don't give up if it's out of memory since it's not going to help anyway
                    // if (retrycnt++ < 20 || (PAGEIN_RETRY == pageresult)) {
#ifdef DEBUG
                    {
                        extern CRITICAL_SECTION PhysCS, PagerCS;
                        DEBUGCHK (VAcs.OwnerThread != pCurThread);
                        DEBUGCHK (PhysCS.OwnerThread != pCurThread);
                        DEBUGCHK (PagerCS.OwnerThread != pCurThread);
                    }
#endif
                        goto retryPagein;
                    //}
                    
                    DEBUGMSG(ZONE_PAGING, (TEXT("ProcessPageFault : PageIn failed for address 0x%08X\r\n"), addr0));
//                  DEBUGCHK(0);
                    goto FailPageInNoCS;
                }
                bRet = (BOOL)pageresult;
                EnterCriticalSection(&VAcs);
            }
        } else {
            bRet = FALSE;   // Thread not allowed access, return failure.
            DEBUGMSG (ZONE_PAGING, (L"ProcessPageFault: no access (pmb->alk = %8.8lx, CurAKey = %8.8lx)\n", pmb->alk, CurAKey));
        }
    }
    LeaveCriticalSection(&VAcs);
    
    // give pagein notification only if pagein success
    if (bRet) {
        HDPageIn (addr0 & ~(PAGE_SIZE-1), bWrite);
    }
FailPageInNoCS:
    DEBUGMSG(ZONE_PAGING, (TEXT("ProcessPageFault returning: %a\r\n"), bRet?"TRUE":"FALSE"));
    return bRet;
}

LPVOID pProfBase;
DWORD dwProfState;
DWORD dwOldEntry;



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
ProfInit(void) 
{
    pProfBase = VirtualAlloc((LPVOID)MODULE_BASE_ADDRESS,PAGE_SIZE,MEM_RESERVE,PAGE_NOACCESS);
    pProfBase = MapPtr(pProfBase);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID 
SC_GetProfileBaseAddress(void) 
{
    TRUSTED_API (L"SC_GetProfileBaseAddress", NULL);

    EnterCriticalSection(&VAcs);
    if (!dwProfState) {
        pProfBase = VirtualAlloc(pProfBase,PAGE_SIZE,MEM_COMMIT,PAGE_READWRITE);
        dwProfState = 1;
    }
    LeaveCriticalSection(&VAcs);
    return pProfBase;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID 
SC_SetProfilePortAddress(
    LPVOID lpv
    ) 
{
    PSECTION pscn;
    MEMBLOCK *pmb;
    DWORD dwBase;

    TRUSTED_API_VOID (L"SC_SetProfilePortAddress");
    EnterCriticalSection(&VAcs);
    if (!dwProfState) {
        pProfBase = VirtualAlloc(pProfBase,PAGE_SIZE,MEM_COMMIT,PAGE_READWRITE);
        dwProfState = 1;
    }
    dwBase = (DWORD) pProfBase;

    pscn = IsSecureVa(dwBase)? &NKSection : SectionTable[(dwBase>>VA_SECTION) & SECTION_MASK];
    pmb = (*pscn)[(dwBase >> VA_BLOCK) & BLOCK_MASK];
    if (!lpv) {
        if (dwProfState == 2) {
            pmb->aPages[0] = dwOldEntry;
            dwProfState = 1;
        }
    } else {
        if (dwProfState == 1)
            dwOldEntry = pmb->aPages[0];
        pmb->aPages[0] = PFNfrom256(lpv) | MakePagePerms(PAGE_READWRITE|PAGE_NOCACHE, dwBase);
        dwProfState = 2;
    }
    InvalidateRange(pProfBase, PAGE_SIZE);
    LeaveCriticalSection(&VAcs);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID
SC_AllocPhysMem(
    DWORD   cbSize,
    DWORD   fdwProtect,                 // type of access protection
    DWORD   dwAlignmentMask, 
    DWORD   dwFlags,                    // Reserved for now, must be zero
    PULONG  pPhysicalAddress
    )
{
    LPVOID pVirtualBuffer = NULL;
    ULONG ulPFN;
    DWORD dwPages = (cbSize + (PAGE_SIZE - 1)) / PAGE_SIZE;

    TRUSTED_API (L"SC_AllocPhysMem", NULL);
    
    if (!cbSize || !pPhysicalAddress || dwFlags) {
        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
        return NULL;
    }
    
    if ((ulPFN = GetContiguousPages(dwPages, dwAlignmentMask, dwFlags)) == INVALID_PHYSICAL_ADDRESS) {
        KSetLastError(pCurThread, ERROR_NOT_ENOUGH_MEMORY);
        DEBUGMSG(ZONE_VIRTMEM, (TEXT("Couldn't get %d pages contiguous\r\n"), dwPages));
    } else {
        DEBUGMSG(ZONE_VIRTMEM, (TEXT("Got %d pages contiguous @ 0x%08X\r\n"), dwPages, ulPFN));
        //
        // Allocate the virtual memory with contiguous physical pages
        //
        pVirtualBuffer = DoVirtualAlloc(0, cbSize, MEM_COMMIT, fdwProtect|PAGE_NOCACHE, MEM_CONTIGUOUS, ulPFN);

        if (pVirtualBuffer) {
            *pPhysicalAddress = PFN2PA(ulPFN);
        }

    }
    DEBUGMSG(ZONE_VIRTMEM, (TEXT("CeAllocPhysMem - VA: %08X PA:%08X \r\n"), pVirtualBuffer, *pPhysicalAddress));

    return pVirtualBuffer;
}



//------------------------------------------------------------------------------
//  Release memory previously acquired with AllocPhysMem.
//------------------------------------------------------------------------------
BOOL
SC_FreePhysMem(
    LPVOID lpvAddress
    )
{
    TRUSTED_API (L"SC_FreePhysMem", FALSE);
    return (VirtualFree(lpvAddress, 0, MEM_RELEASE | 0x80000000));
}

//-------------------------------------------------------------------------------
// get process VM information by index
//-------------------------------------------------------------------------------
BOOL GetVMInfo (int idxProc, PPROCVMINFO pInfo)
{
    PSECTION pscn;
    MEMBLOCK *pmb;
    DWORD    ixBlock, ixPage;
    DWORD    cPages = 0;
    DWORD    ulPTE;
    BOOL     fRet = FALSE;

    EnterCriticalSection (&VAcs);
    __try {
        if (ProcArray[idxProc].dwVMBase) {
            pscn = SectionTable[idxProc+1];
            for (ixBlock = 0 ; ixBlock < BLOCK_MASK+1 ; ++ixBlock) {
                if (((pmb = (*pscn)[ixBlock]) != NULL_BLOCK) && (RESERVED_BLOCK != pmb)) {
                    for (ixPage = 0 ; ixPage < PAGES_PER_BLOCK ; ++ixPage) {
                        ulPTE = pmb->aPages[ixPage];
                        if ((ulPTE & PG_VALID_MASK)                     // valid page
                            && IsPageWritable (ulPTE)                   // R/W
                            && !(ulPTE & PG_EXECUTE_MASK)               // not code
                            && GetRegionFromAddr (PFNfromEntry(ulPTE))) // in RAM
                        {
                            cPages ++;
                        }
                    }
                }
            }
            pInfo->hProc = ProcArray[idxProc].hProc;
            pInfo->cbRwMemUsed = cPages << VA_PAGE;
            fRet = TRUE;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        fRet = FALSE;
    }
    LeaveCriticalSection (&VAcs);
    return fRet;
}


