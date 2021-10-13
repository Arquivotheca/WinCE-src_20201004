//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "kernel.h"

//#define PHYSPAGESTOLEDS 1

#ifdef PHYSPAGESTOLEDS
#define LogPhysicalPages(PageFreeCount) OEMWriteDebugLED(0,PageFreeCount);
#else
#define LogPhysicalPages(PageFreeCount) (0)
#endif

#define PAGEOUT_LOW         (( 68*1024)/PAGE_SIZE)  // do pageout once below this mark, reset when above high
#define PAGEOUT_HIGH        ((132*1024)/PAGE_SIZE)
#define PAGE_OUT_TRIGGER    (( 24*1024)/PAGE_SIZE)  // page out if below highwater and this much paged in recently

void ZeroPage(void *pvPage);
BOOL ScavengeStacks(int cNeed);

HANDLE GwesOOMEvent;
long GwesLowThreshold;
long GwesCriticalThreshold;
long GwesLowBlockSize;
long GwesCriticalBlockSize;
LPBYTE pFreeKStacks;
LPBYTE pDirtyList;

// the maximum (lowest) priority to keep a thread's stack from being scavanged
// any thread of this priority or higher will never have its stack scvanaged.
// stack scavanging will never occur if set to 255
DWORD dwNKMaxPrioNoScav = 247;  // default to lowest RT priority

extern void FreeSpareBytes(LPBYTE lpb, uint size);

extern CRITICAL_SECTION PhysCS;
extern CRITICAL_SECTION DirtyPageCS;
extern PTHREAD pCleanupThread;
extern DWORD dwOEMCleanPages;

// PageOutNeeded is set to 1 when the number of free pages drops below
// PageOutTrigger and the cleanup thread's event is signalled. When the
// cleanup thread starts performing page out, it sets the variable to 2.
// When the page free count rises above PageOutLevel, PageOutNeeded is
// reset to 0.
//
// The free page count thresholds are set based upon the Gwes OOM level.

long PageOutNeeded;
long PageOutTrigger;    // Threshold level to start page out.
long PageOutLevel;      // Threshold to stop page out.

static DWORD PFNReserveStart;   // starting PFN of the reserved memory
static DWORD PFNReserveEnd;     // ending PFN of the reserved memory

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PFREEINFO GetRegionFromAddr(PHYSICAL_ADDRESS addr)
{
    // don't free the reserved area
    if ((addr >= PFNReserveEnd) || (addr < PFNReserveStart)) {
        PFREEINFO pfi, pfiEnd;
        pfi = MemoryInfo.pFi;
        pfiEnd = pfi + MemoryInfo.cFi;
        for ( ; pfi < pfiEnd ; pfi++) {
            if (addr >= pfi->paStart && addr < pfi->paEnd)
                return pfi;
        }
    }
    return NULL;
}

void SetOOMEvt (void)
{
    ACCESSKEY oldKey;
    SWITCHKEY (oldKey, 0xffffffff);
    SetEvent(GwesOOMEvent);
    SETCURKEY (oldKey);
}

//------------------------------------------------------------------------------
//  GWE calls this function to register an Out Of Memory event, which
//  HoldPages sets when free physical memory drops below the given threshold.
//------------------------------------------------------------------------------
VOID 
SC_SetGwesOOMEvent(
    HANDLE hEvent, 
    DWORD cpLow, 
    DWORD cpCritical,
    DWORD cpLowBlockSize, 
    DWORD cpCriticalBlockSize
    ) 
{
    TRUSTED_API_VOID (L"SC_SetGwesOOMEvent");
    DEBUGMSG(ZONE_ENTRY,(L"SC_SetGwesOOMEvent entry: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
        hEvent,cpLow,cpCritical,cpLowBlockSize,cpCriticalBlockSize));
    GwesOOMEvent = hEvent;
    GwesLowThreshold = cpLow;
    GwesCriticalThreshold = cpCritical;
    GwesLowBlockSize = cpLowBlockSize;
    GwesCriticalBlockSize = cpCriticalBlockSize;
    PageOutLevel = GwesLowThreshold + PAGEOUT_HIGH;
    PageOutTrigger = GwesLowThreshold + PAGEOUT_LOW;
    DEBUGMSG(ZONE_ENTRY,(L"SC_SetGwesOOMEvent exit\r\n"));
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
UnlinkPhysPage(
    LPBYTE pMem
    ) 
{
    KCALLPROFON(38);
    if (*(DWORD *)((DWORD)pMem + 0x20000000))
        *(LPBYTE *)(*(DWORD *)((DWORD)pMem + 0x20000000) + 0x20000004) =
            *(LPBYTE *)((DWORD)pMem + 0x20000004);
    if (*(DWORD *)((DWORD)pMem + 0x20000004))
        *(LPBYTE *)(*(DWORD *)((DWORD)pMem + 0x20000004) + 0x20000000) =
            *(LPBYTE *)((DWORD)pMem + 0x20000000);
    else
        LogPtr->pKList = *(LPBYTE *)((DWORD)pMem + 0x20000000);
    *(LPDWORD)(pMem + 0x20000000) = 0;
    *(LPDWORD)(pMem + 0x20000004) = 0;
    KCALLPROFOFF(38);
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
LinkPhysPageOnly(
    LPBYTE pMem
    ) 
{
    KCALLPROFON(32);
    *(LPBYTE *)((DWORD)pMem + 0x20000000) = LogPtr->pKList;
    *(LPBYTE *)((DWORD)pMem + 0x20000004) = 0;
    if (LogPtr->pKList)
        *(LPBYTE *)((DWORD)LogPtr->pKList + 0x20000004) = pMem;
    LogPtr->pKList = pMem;
    KCALLPROFOFF(32);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
LinkPhysPage(
    LPBYTE pMem
    ) 
{
    KCALLPROFON(32);
    *(LPBYTE *)((DWORD)pMem + 0x20000000) = LogPtr->pKList;
    *(LPBYTE *)((DWORD)pMem + 0x20000004) = 0;
    if (LogPtr->pKList)
        *(LPBYTE *)((DWORD)LogPtr->pKList + 0x20000004) = pMem;
    LogPtr->pKList = pMem;
    PageFreeCount++;
    LogPhysicalPages(PageFreeCount);
    KCALLPROFOFF(32);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
LinkPhysPageList(
    LPBYTE pListStart,
    LPBYTE pListEnd,
    DWORD cPages
    ) 
{
    PFREEINFO pfi, pfiEnd;

    KCALLPROFON(32);
    *(LPBYTE *)((DWORD)pListEnd + 0x20000000) = LogPtr->pKList;
    if (LogPtr->pKList)
        *(LPBYTE *)((DWORD)LogPtr->pKList + 0x20000004) = pListEnd;
    LogPtr->pKList = pListStart;
    PageFreeCount += cPages;
    LogPhysicalPages(PageFreeCount);
    // restore end address of each memory section, loop less than MAX_MEMORY_SECTIONS times
    for (pfi = &MemoryInfo.pFi[0], pfiEnd = pfi+MemoryInfo.cFi ; pfi < pfiEnd ; ++pfi) {
        pfi->paEnd = pfi->paRealEnd;
    }
    KCALLPROFOFF(32);
}



//------------------------------------------------------------------------------
//
// GrabFirstPhysPage gets the next available physical page from the kernel's
// linked list of free pages (LogPtr->pKList).
//
//------------------------------------------------------------------------------
LPBYTE 
GrabFirstPhysPage(
    DWORD dwCount
    ) 
{
    PFREEINFO pfi;
    uint ix;
    PHYSICAL_ADDRESS paRet;
    LPBYTE pMem = NULL;
    
    KCALLPROFON(33);

    // need to check PageFreecount instead of LogPtr->pKList or we might end up
    // with PageFreeCount < 0, and DEBUGCHK'd
    if (PageFreeCount) {
        
        pMem = LogPtr->pKList;
        paRet = GetPFN(pMem);
        pfi = GetRegionFromAddr(paRet);
        
        if (pfi) {
            //
            // Unlink the first page (first two DWORD's are link pointers).
            // Accesses are uncached.
            //
            if (LogPtr->pKList = *(LPBYTE *)((DWORD)pMem + 0x20000000)) {
                *(LPBYTE *)((DWORD)LogPtr->pKList + 0x20000004) = 0;
            }
            //
            // Clear out the link info so we have a clean page.
            //
            *(LPDWORD)(pMem + 0x20000000) = 0;
            *(LPDWORD)(pMem + 0x20000004) = 0;
            
            ix = (paRet - pfi->paStart) / PFN_INCR;
            DEBUGCHK(pfi->pUseMap[ix] == 0);
            DEBUGCHK(dwCount && (dwCount <= 0xff));
            pfi->pUseMap[ix] = (BYTE)dwCount;
            if (-- PageFreeCount < (long) KInfoTable[KINX_MINPAGEFREE]) {
                KInfoTable[KINX_MINPAGEFREE] = PageFreeCount;
            }
            LogPhysicalPages(PageFreeCount);
            DEBUGCHK(PageFreeCount >= 0);
        } else {
            ERRORMSG(1, (TEXT("GrabFirstPhysPage : invalid address 0x%08x (PFN 0x%08x)\r\n"), pMem, paRet));
            DEBUGCHK(0);
            pMem = NULL;
        }
    }
    DEBUGCHK(pMem);
    KCALLPROFOFF(33);
    return pMem;    
}



//------------------------------------------------------------------------------
// called by loader.c when moving memory "slider"
//------------------------------------------------------------------------------
void 
RemovePage(
    DWORD dwAddr
    ) 
{
    DWORD pfn, ix;
    FREEINFO *pfi;
    
    pfn = GetPFN(dwAddr);
    pfi = GetRegionFromAddr(pfn);
    
    if (pfi) {
        ix = (pfn - pfi->paStart) / PFN_INCR;
        pfi->pUseMap[ix] = 0xff;
    } else {
        ERRORMSG(1, (TEXT("RemovePage : removing invalid address 0x%08x (PFN 0x%08x)\r\n"), dwAddr, pfn));
        DEBUGCHK(0);
    }
}




//------------------------------------------------------------------------------
//
// DemandCommit is used to automatically commit stack pages. It is called 
// at the start of the exception handling code since the exception handlers
// need to use the stack. 
//
//------------------------------------------------------------------------------
BOOL 
DemandCommit(
    ulong addr
    ) 
{
    MEMBLOCK *pmb;
    LPDWORD pPage;
    PFREEINFO pfi;
    uint ix;
    PHYSICAL_ADDRESS paRet;
    LPBYTE pMem;
    PSECTION pscn = IsSecureVa(addr)? &NKSection : SectionTable[addr>>VA_SECTION];
    
    pmb = (*pscn)[(addr>>VA_BLOCK) & BLOCK_MASK];
    //
    // Don't bother checking MB_FLAG_AUTOCOMMIT - we're dead if it's not set
    //
    if (!*(pPage = &pmb->aPages[(addr>>VA_PAGE) & PAGE_MASK])) {
        pMem = LogPtr->pKList;

        if (GwesOOMEvent && (PageFreeCount < GwesCriticalThreshold)) {
            SetOOMEvt ();
        }
        
        if (!PageFreeCount || !pMem) {
            RETAILMSG(1,(L"--->>> DemandCommit: FATAL ERROR!  COMPLETELY OUT OF MEMORY (%8.8lx %8.8lx)!\r\n",pMem,PageFreeCount));
            INTERRUPTS_OFF();
            while (1) {
                // we don't recover if there's no memory, so better off to just stop
            }
            return FALSE;
        }
        paRet = GetPFN(pMem);
        pMem = (LPBYTE)((DWORD)pMem + 0x20000000);
        if (LogPtr->pKList = *(LPBYTE *)pMem)
            *(LPBYTE *)((DWORD)LogPtr->pKList + 0x20000004) = 0;
        *(LPDWORD)pMem = 0;
        *(LPDWORD)(pMem + 4) = 0;
        pfi = GetRegionFromAddr(paRet);
        if (pfi) {
            ix = (paRet - pfi->paStart) / PFN_INCR;
            DEBUGCHK(!pfi->pUseMap[ix]);
            pfi->pUseMap[ix] = 1;
#ifdef ARM
            *pPage = paRet | ((pscn == &NKSection)? PG_KRN_READ_WRITE : PG_READ_WRITE);
#else
            *pPage = paRet | PG_READ_WRITE;
#endif
            if (-- PageFreeCount < (long) KInfoTable[KINX_MINPAGEFREE]) {
                KInfoTable[KINX_MINPAGEFREE] = PageFreeCount;
            }
            LogPhysicalPages(PageFreeCount);
            DEBUGCHK(PageFreeCount >= 0);
            return TRUE;
        } else {
            ERRORMSG(1, (TEXT("DemandCommit : invalid address 0x%08x (PFN 0x%08x)\r\n"), pMem, paRet));
            DEBUGCHK(0);
        }
    }
    return FALSE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
EnterPhysCS(void) 
{
    LPVOID pMem;
    EnterCriticalSection(&PhysCS);
    DEBUGCHK(PhysCS.OwnerThread == hCurThread);
    if (PhysCS.LockCount == 1)
        while (pMem = InterlockedPopList(&pFreeKStacks))
            FreeHelperStack(pMem);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_GiveKPhys(
    void *ptr, 
    ulong length
    ) 
{
    BOOL bRet = FALSE;
    LPBYTE pPage;
    DWORD loop;
    PFREEINFO pfi;
    PHYSICAL_ADDRESS paPFN;
    uint ix;

    TRUSTED_API (L"SC_GiveKPhys", FALSE);

    DEBUGMSG(ZONE_ENTRY,(L"SC_GiveKPhys entry: %8.8lx %8.8lx\r\n",ptr,length));
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_DISCARD);
    EnterPhysCS();
    for (loop = 0; loop < length; loop++) {
        pPage = *((LPBYTE *)ptr+loop);
        ZeroPage(pPage + 0x20000000);
        paPFN = GetPFN(pPage);
        pfi = GetRegionFromAddr(paPFN);
        if (pfi) {
            ix = (paPFN - pfi->paStart) / PFN_INCR;
            DEBUGCHK(pfi->pUseMap[ix] == 0xff);
            pfi->pUseMap[ix] = 0;
            KCall((PKFN)LinkPhysPage,pPage);
        } else {
            ERRORMSG(1, (TEXT("GiveKPhys : invalid address 0x%08x (PFN 0x%08x)\r\n"), pPage, paPFN));
            DEBUGCHK(0);
        }
    }
    KInfoTable[KINX_NUMPAGES] += length;
    // If there are enough free pages, clear the PageOutNeeded flag.
    if (PageFreeCount > PageOutLevel)
        PageOutNeeded = 0;
    bRet = TRUE;
    LeaveCriticalSection(&PhysCS);
    DEBUGMSG(ZONE_ENTRY,(L"SC_GiveKPhys exit: %8.8lx\r\n",bRet));
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_GetKPhys(
    void *ptr, 
    ulong length
    ) 
{
    LPVOID *pPages;
    BOOL bRet = TRUE;
    DWORD dwCount;
    LPBYTE pMem;

    TRUSTED_API (L"SC_GetKPhys", FALSE);

    DEBUGMSG(ZONE_ENTRY,(L"SC_GetKPhys entry: %8.8lx %8.8lx\r\n",ptr,length));

    if ((int) length < 0) {
        return FALSE;
    }
    pPages = (LPVOID *)ptr;
    EnterPhysCS();
    ScavengeStacks(100000);     // Reclaim all extra stack pages.
    for (dwCount = length; (int) dwCount > 0; dwCount--) {
        if (((length > 1) && ((DWORD)PageFreeCount <= dwCount + PageOutTrigger))
            || !(pMem = (LPBYTE)KCall((PKFN)GrabFirstPhysPage,0xff))) {
            bRet = FALSE;
            break;
        }
        KInfoTable[KINX_NUMPAGES]--;
        *pPages++ = pMem;
    }
    LeaveCriticalSection(&PhysCS);

    if (!bRet) {
        SC_GiveKPhys(ptr,length - dwCount);
        RETAILMSG(1,(L"Error getting pages!\r\n"));
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_GetKPhys exit: %8.8lx\r\n",bRet));
    return bRet;
}
    


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
InitMemoryPool() 
{
    PFREEINFO pfi, pfiEnd;
    DWORD loop;
    LPBYTE pPage;
    /* Fill in data fields in user visible kernel data page */
    KInfoTable[KINX_PROCARRAY] = (long)ProcArray;
    KInfoTable[KINX_PAGESIZE] = PAGE_SIZE;
#ifdef PFN_SHIFT
    KInfoTable[KINX_PFN_SHIFT] = PFN_SHIFT;
#endif
    KInfoTable[KINX_PFN_MASK] = (DWORD)PFNfromEntry(~0);
    KInfoTable[KINX_SECTIONS] = (long)SectionTable;
    KInfoTable[KINX_MEMINFO] = (long)&MemoryInfo;
    KInfoTable[KINX_KDATA_ADDR] = (long)&KData;

    PFNReserveStart = GetPFN (pTOC->ulRAMFree);
    PFNReserveEnd = GetPFN (pTOC->ulRAMFree + MemForPT);
    
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_DISCARD);
    pDirtyList = NULL;
    for (pfi = MemoryInfo.pFi, pfiEnd = pfi+MemoryInfo.cFi ; pfi < pfiEnd ; ++pfi) {
        DEBUGMSG(ZONE_MEMORY, (TEXT("InitMemoryPool: Init range: map=%8.8lx start=%8.8lx end=%8.8lx\r\n"),
                pfi->pUseMap, pfi->paStart, pfi->paEnd));
        pPage = 0;
        for (loop = pfi->paStart; loop < pfi->paEnd; loop += PFN_INCR) {
            if (!pfi->pUseMap[(loop-pfi->paStart)/PFN_INCR]) {
                pPage = Phys2Virt(loop);
                if ((dwOEMCleanPages == 0) || ((DWORD)PageFreeCount < dwOEMCleanPages)) {
                    ZeroPage(pPage + 0x20000000);
                    LinkPhysPage(pPage);
                    LogPtr->pKList = pPage;
                    KInfoTable[KINX_NUMPAGES]++;
                } else {
                    *(LPBYTE *)((DWORD)pPage + 0x20000000) = pDirtyList;
                    *(LONG *)((DWORD)pPage + 0x20000004) = (pfi->paEnd - loop)/ PFN_INCR;
                    pDirtyList = pPage;
                    pfi->paEnd = loop;
                }
            }
        }
    }
    KInfoTable[KINX_MINPAGEFREE] = PageFreeCount;
    DEBUGMSG(ZONE_MEMORY,(TEXT("InitMemoryPool done, free=%d\r\n"),PageFreeCount));
}


void CleanDirtyPagesThread(LPVOID pv)
{
    LPBYTE pList = NULL, pListEnd = NULL, pPage;
    DWORD cTotalPages = 0, cPages;

    EnterCriticalSection(&DirtyPageCS);
    if (!pDirtyList) {
        goto exit;
    }

    SC_ThreadSetPrio (hCurThread, THREAD_PRIORITY_IDLE);

    //
    // clean memory and form a list from dirty memory trunks
    //
    pList = NULL;
    cTotalPages = 0;
    // since we push new page into pList, thus the first page pushed in will
    // be the last page in the list.
    pListEnd = pDirtyList;
    while (pDirtyList) {
        // remove trunk from dirty list
        pPage = pDirtyList;
        pDirtyList = *(LPBYTE *)pPage;              // first dword is the link to next trunk
        cPages = *(DWORD *)((DWORD)pPage+4);        // second dword contains # of pages in this trunk
        DEBUGCHK(cPages);

        cTotalPages += cPages;
        while (cPages) {
            ZeroPage(pPage+0x20000000);
            // add page into pList
            *(LPBYTE *)((DWORD)pPage + 0x20000000) = pList;
            *(LPBYTE *)((DWORD)pPage + 0x20000004) = 0;
            if (pList)
                *(LPBYTE *)((DWORD)pList + 0x20000004) = pPage;
            pList = pPage;
            pPage += PAGE_SIZE;
            cPages--;
        }
    }

    //
    // add page list into pKList
    //
    EnterPhysCS();
    KCall((PKFN)LinkPhysPageList, pList, pListEnd, cTotalPages);
    KInfoTable[KINX_NUMPAGES] += cTotalPages;
    // if there are enough free pages, clear the PageOutNeeded flag.
    if (PageFreeCount > PageOutLevel)
        PageOutNeeded = 0;
    LeaveCriticalSection(&PhysCS);

exit:
    LeaveCriticalSection(&DirtyPageCS);
    DEBUGMSG(ZONE_MEMORY, (TEXT("CleanDirtyPagesThread: Add memory list start = %X end = %X, %d pages\n"),
                            pList, pListEnd, cTotalPages));
}

PTHREAD PthScavTarget;
PPROCESS PprcScavTarget = &ProcArray[0];
int StackScavCount;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HTHREAD 
ScavengeOnePage(void) 
{
    HTHREAD ret;
    LPDWORD pFiber;
    KCALLPROFON(13);
    if (!PthScavTarget) {
        if (++PprcScavTarget >= &ProcArray[MAX_PROCESSES])
            PprcScavTarget = ProcArray;
        ++StackScavCount;
        if (!PprcScavTarget->dwVMBase || !(PthScavTarget = PprcScavTarget->pTh)) {
            KCALLPROFOFF(13);
            return 0;
        }
        if (PthScavTarget == pCurThread)
            PthScavTarget = pCurThread->pNextInProc;
        KCALLPROFOFF(13);
        return 0;
    }
    if (HandleToThread(PthScavTarget->hTh) != PthScavTarget) {
        PthScavTarget = 0;
        KCALLPROFOFF(13);
        return 0;
    }
    pFiber = (LPDWORD) KCURFIBER(PthScavTarget);
    // scavange the stack of a thread only if
    // (1) it's not the current running thread, and
    // (2) the state of its stack is not changing, and
    // (3) the state of the fiber is changing, and
    // (4) the priority of the thread is lower than dwNKMaxPrioNoScav
    // (5) not in the middle of ServerCallReturn that's switching stack
    ret = (    (pCurThread != PthScavTarget)    // not current thread?
            && MDTestStack(PthScavTarget)       // stack okay?
            && (!pFiber || !(*pFiber & 1))      // fiber state not changing?
            && (GET_CPRIO(PthScavTarget) > dwNKMaxPrioNoScav)   // priority low enough?
            && (1)                              
          ) ? PthScavTarget->hTh : 0;
          
    if ((PthScavTarget = PthScavTarget->pNextInProc) == pCurThread)
        PthScavTarget = pCurThread->pNextInProc;
    KCALLPROFOFF(13);
    return ret;
}



HANDLE hCurrScav;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
ScavengeStacks(
    int cNeed
    ) 
{
    HTHREAD hth;
    PTHREAD pth;
    ulong addr, pages, base;
    ulong pte;
    int ixPage, ixBlock;
    PSECTION pscn;
    MEMBLOCK *pmb;
    ACCESSKEY ulOldKey;
    
    DEBUGMSG(ZONE_MEMORY, (TEXT("Scavenging stacks for %d pages.\r\n"), cNeed-PageFreeCount));
    StackScavCount = 0;
    SWITCHKEY(ulOldKey,0xffffffff);
    
    while ((StackScavCount <= MAX_PROCESSES) && (cNeed > PageFreeCount)) {
        if (hth = (HTHREAD)KCall((PKFN)ScavengeOnePage)) {
            // need to call ThreadSuspend directly (fail if can't suspend immediately)
            if (!(KCall (ThreadSuspend, pth = HandleToThread(hth), FALSE) & 0x80000000)) {
                hCurrScav = hth;
                pages = 0;
                OEMCacheRangeFlush (0, 0, CACHE_SYNC_DISCARD);
                while (addr = MDTestStack(pth)) {
                    pscn = IsSecureVa(addr)? &NKSection : SectionTable[addr>>VA_SECTION];
                    ixBlock = (addr>>VA_BLOCK) & BLOCK_MASK;
                    ixPage = (addr>>VA_PAGE) & PAGE_MASK;
                    if ((pmb = (*pscn)[ixBlock]) == NULL_BLOCK || pmb == RESERVED_BLOCK ||
                        !(pmb->flags&MB_FLAG_AUTOCOMMIT) || pmb == PmbDecommitting ||
                        !(pte = pmb->aPages[ixPage]) || pte == BAD_PAGE)
                        break;
                    pmb->aPages[ixPage] = 0;
                    MDShrinkStack(pth);
                    FreePhysPage(PFNfromEntry(pte));
                    DEBUGCHK(!pages || (base == addr - PAGE_SIZE));
                    base = addr;
                    pages++;
                }
                if (pages)
                    InvalidateRange((PVOID)base, PAGE_SIZE*pages);
                hCurrScav = 0;
                ResumeThread(hth);
            }
        }
    }
    SETCURKEY(ulOldKey);
    return (StackScavCount > MAX_PROCESSES) ? FALSE : TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PHYSICAL_ADDRESS 
GetHeldPage(void) 
{
    PHYSICAL_ADDRESS paRet;
    LPBYTE pMem;
    if (pMem = (LPBYTE)KCall((PKFN)GrabFirstPhysPage,1)) {
        PageFreeCount++; // since we already reserved it
        LogPhysicalPages(PageFreeCount);
        paRet = GetPFN(pMem);
    } else
        paRet = 0;
    DEBUGMSG(ZONE_PHYSMEM,(TEXT("GetHeldPage: Returning %8.8lx\r\n"), paRet));
    return paRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
HoldPages(
    int cpNeed, 
    BOOL bForce
    ) 
{
    BOOL fSetGweOomEvent = FALSE;
    BOOL bRet = FALSE;
    WORD prio;
    EnterPhysCS();
    // Check if this request will drop the page free count below the
    // page out trigger and signal the clean up thread to start doing
    // pageouts if so.
    if ((cpNeed+PageOutTrigger > PageFreeCount) &&
        (!PageOutNeeded || (PagedInCount > PAGE_OUT_TRIGGER))) {
        PageOutNeeded = 1;
        PagedInCount = 0;
        if (prio = GET_CPRIO(pCurThread))
            prio--;
        if (prio < GET_CPRIO(pCleanupThread))
            KCall((PKFN)SetThreadBasePrio, pCleanupThread->hTh, (DWORD)prio);
        SetEvent(hAlarmThreadWakeup);
    }
    do {
        if (cpNeed+GwesLowThreshold <= PageFreeCount) {
            DWORD pfc, pfc2;
            do {
                pfc = PageFreeCount;
            } while (InterlockedTestExchange(&PageFreeCount,pfc,pfc-cpNeed) != (LONG)pfc);

            // update page free water mark. need to take into account other threads updating it
            // at the same time. NOTE: don't use PageFreeCount directly since other threads might increment
            // it and it'll make the water mark inaccurate. And if it been decremented, other threads will
            // update it correctly.
            pfc -= cpNeed;
            do {
                pfc2 = KInfoTable[KINX_MINPAGEFREE];
            } while ((pfc2 > pfc)
                && (InterlockedTestExchange ((PLONG)&KInfoTable[KINX_MINPAGEFREE], pfc2, pfc) != (LONG)pfc2));
            LogPhysicalPages(PageFreeCount);
            bRet = TRUE;
            goto hpDone;
        }
    } while (ScavengeStacks(cpNeed+GwesLowThreshold));

    // Even after scavenging stacks, we were unable to satisfy the request
    // without going below the GWE low threshold.

    // Do not allow a request of size GwesLowBlockSize to succeed if
    // doing so would leave less than the low threshold.  Same with
    // GwesCriticalBlockSize and GwesCriticalThreshold.
    if (bForce || !((cpNeed > GwesLowBlockSize
            && cpNeed+GwesLowThreshold > PageFreeCount)
            || (cpNeed > GwesCriticalBlockSize
            && cpNeed + GwesCriticalThreshold > PageFreeCount))) {

        // Memory is low. Notify GWE, so that GWE can ask
        // the user to close some apps.
        if (GwesOOMEvent &&
            ((PageFreeCount >= GwesLowThreshold) ||
             ((PageFreeCount < cpNeed + GwesCriticalThreshold))))
            fSetGweOomEvent = TRUE;
        
        if ((cpNeed + (bForce?0:STACK_RESERVE)) <= PageFreeCount) {
            DWORD pfc, pfc2;
            do {
                pfc = PageFreeCount;
            } while (InterlockedTestExchange(&PageFreeCount,pfc,pfc-cpNeed) != (LONG)pfc);

            // update page free water mark. need to take into account other threads updating it
            // at the same time. NOTE: don't use PageFreeCount directly since other threads might increment
            // it and it'll make the water mark inaccurate. And if it been decremented, other threads will
            // update it correctly.
            pfc -= cpNeed;
            do {
                pfc2 = KInfoTable[KINX_MINPAGEFREE];
            } while ((pfc2 > pfc)
                && (InterlockedTestExchange ((PLONG)&KInfoTable[KINX_MINPAGEFREE], pfc2, pfc) != (LONG)pfc2));
            LogPhysicalPages(PageFreeCount);
            bRet = TRUE;
        }
    }
hpDone:
    LeaveCriticalSection(&PhysCS);
    if (fSetGweOomEvent) {
        SetOOMEvt ();
    }
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
DupPhysPage(
    PHYSICAL_ADDRESS paPFN
    ) 
{
    PFREEINFO pfi;
    uint ix;

    //NOTE: DupPhysage and FreePhysPage are only called by the virtual memory
    // system with the VA critical section held so DupPhysPage does not need to
    // claim the physical memory critical section because it does not change
    // the page free count.
    if ((pfi = GetRegionFromAddr(paPFN)) != 0) {
        ix = (paPFN - pfi->paStart) / PFN_INCR;
        DEBUGCHK(pfi->pUseMap[ix] != 0);
        DEBUGMSG(ZONE_PHYSMEM, (TEXT("DupPhysPage: PFN=%8.8lx ix=%x rc=%d\r\n"), paPFN,
                ix, pfi->pUseMap[ix]));
        ++pfi->pUseMap[ix];
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
FreePhysPage(
    PHYSICAL_ADDRESS paPFN
    ) 
{
    PFREEINFO pfi;
    uint ix;

    if ((pfi = GetRegionFromAddr(paPFN)) != 0) {
        ix = (paPFN - pfi->paStart) / PFN_INCR;
        DEBUGMSG(ZONE_PHYSMEM, (TEXT("FreePhysPage: PFN=%8.8lx ix=%x rc=%d\r\n"), paPFN,
                ix, pfi->pUseMap[ix]));
        EnterPhysCS();
        DEBUGCHK(pfi->pUseMap[ix] != 0);
        if ((pfi->pUseMap[ix] != 0xff) && (!--pfi->pUseMap[ix])) {
            DEBUGMSG(ZONE_PHYSMEM, (TEXT("FreePhysPage: PFN=%8.8lx\r\n"), paPFN));
            ZeroPage((LPBYTE)Phys2Virt(paPFN)+0x20000000);
            KCall((PKFN)LinkPhysPage,Phys2Virt(paPFN));
            // If there are enough free pages, clear the PageOutNeeded flag.
            if (PageFreeCount > PageOutLevel)
                PageOutNeeded = 0;
        }
        LeaveCriticalSection(&PhysCS);
    }
}

LPVOID PhysPageToZero (PHYSICAL_ADDRESS paPFN) 
{
    PFREEINFO pfi;
    uint ix;
    LPVOID pPage = NULL;

    if ((pfi = GetRegionFromAddr(paPFN)) != 0) {
        ix = (paPFN - pfi->paStart) / PFN_INCR;
        DEBUGMSG(ZONE_PHYSMEM, (TEXT("PhysPageToZero: PFN=%8.8lx ix=%x rc=%d\r\n"), paPFN,
                ix, pfi->pUseMap[ix]));
        EnterPhysCS();
        DEBUGCHK(pfi->pUseMap[ix] != 0);
        if ((pfi->pUseMap[ix] != 0xff) && (!--pfi->pUseMap[ix])) {
            DEBUGMSG(ZONE_PHYSMEM, (TEXT("PhysPageToZero: PFN=%8.8lx\r\n"), paPFN));
            pPage = (LPVOID) Phys2VirtUC (paPFN);
            DEBUGCHK (pPage);
            // set pfi->pUseMap[ix] = 0xff to avoid being used by GetContigiousPages, which
            // scans pUseMap to find pages.
            pfi->pUseMap[ix] = 0xff;
            // save &pfi->pUseMap[ix] on the page so we can clean it later
            *(LPDWORD) ((DWORD)pPage + sizeof(LPVOID)) = (DWORD) &pfi->pUseMap[ix];
            
        }
        LeaveCriticalSection(&PhysCS);
    }
    return pPage;
}

void ZeroAndLinkPhysPage (LPVOID pPage)
{
    LPBYTE pUseMap = (LPBYTE) (*(LPDWORD) ((DWORD)pPage + sizeof(LPVOID)));
    DEBUGMSG(ZONE_PHYSMEM, (TEXT("ZeroAndLinkPhysPage: pPage=%8.8lx\r\n"), pPage));
    DEBUGCHK ((DWORD)pPage & 0x20000000);  // MUST BE A UNCACHED ADDRESS
    ZeroPage (pPage);
    
    EnterPhysCS ();
    // reset pfi->pUseMap[ix] to 0 (set to 0xff in PhysPageToZero)
    DEBUGCHK (0xff == *pUseMap);
    *pUseMap = 0;
    KCall((PKFN)LinkPhysPage, (LPBYTE) pPage - 0x20000000); // LinkPhysPage takes cached address as argument
    // If there are enough free pages, clear the PageOutNeeded flag.
    if (PageFreeCount > PageOutLevel)
        PageOutNeeded = 0;
    LeaveCriticalSection (&PhysCS);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
TakeSpecificPage(
    PFREEINFO pfi, 
    uint ix,
    LPBYTE pMem
    ) 
{
    KCALLPROFON(40);
    if (pfi->pUseMap[ix]) {
        KCALLPROFOFF(40);
        return FALSE;
    }
    pfi->pUseMap[ix] = 1;
    UnlinkPhysPage(pMem);
    KCALLPROFOFF(40);
    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPBYTE 
GiveSpecificPage(
    PFREEINFO pfi, 
    uint ix, 
    LPBYTE pMem
    ) 
{
    KCALLPROFON(40);
    if (pfi->pUseMap[ix] != 1) {
        KCALLPROFOFF(40);
        return 0;
    }
    pfi->pUseMap[ix] = 0;
    LinkPhysPageOnly(pMem);
    KCALLPROFOFF(40);
    return pMem;
}


//------------------------------------------------------------------------------
//
// Try to grab physically contiguous pages. This routine needs to be preemptive
// as much as possible, so we may take some pages and end up giving them back if
// a full string of pages can't be found. Pages are potentially coming and 
// going, but we'll only make one pass through memory.
//
//------------------------------------------------------------------------------
PHYSICAL_ADDRESS
GetContiguousPages(
    DWORD dwPages,
    DWORD dwAlignment, 
    DWORD dwFlags
    ) 
{
    PFREEINFO pfi, pfiEnd;
    uint ixWalk, ixTarget, ixStart, ixTotal, ixTemp;
    PHYSICAL_ADDRESS paRet = INVALID_PHYSICAL_ADDRESS;
    DWORD pfc;
    
    if (((DWORD) PageFreeCount > dwPages + MIN_PROCESS_PAGES) && HoldPages(dwPages, FALSE)) {
        //
        // We've at least got enough pages available and that many have been set
        // aside for us (though not yet assigned). 
        //
        pfi = &MemoryInfo.pFi[0];
        pfiEnd = pfi + MemoryInfo.cFi;

        for (; pfi < pfiEnd ; ++pfi) {
            //
            // For each memory section, scan the free pages.
            //
            ixTotal = (pfi->paEnd - pfi->paStart) / PFN_INCR;
            //
            // Find a string of free pages
            //
            for (ixStart = 0; ixStart <= (ixTotal - dwPages); ) {
                
                if (pfi->pUseMap[ixStart] || (dwAlignment & PFN2PA(pfi->paStart + (ixStart * PFN_INCR))) ) {
                    //
                    // Page in use or doesn't match alignment request, skip ahead
                    //
                    ixStart++;
                } else {
                    ixTarget = ixStart + dwPages;
                    for (ixWalk = ixStart; ixWalk < ixTarget; ixWalk++) {
                        
                        if (pfi->pUseMap[ixWalk] || 
                            !KCall((PKFN)TakeSpecificPage, pfi, ixWalk, Phys2Virt(pfi->paStart + (ixWalk * PFN_INCR)))) {

                            //
                            // Page in use, free all the pages so far and reset start
                            //
                            for (ixTemp = ixStart; ixTemp < ixWalk; ixTemp++) {
                                KCall((PKFN)GiveSpecificPage, pfi, ixTemp, Phys2Virt(pfi->paStart + (ixTemp * PFN_INCR)));
                            }
                            ixStart = ixWalk + 1;
                            break;
                        } 
                    }

                    if (ixWalk == ixTarget) {
                        paRet = pfi->paStart + (ixStart * PFN_INCR);
                        goto foundPages;
                    }
                }
            }
        }
        //
        // We couldn't lock down a contiguous section so free up the "held" 
        // pages before leaving.
        //
        do {
            pfc = PageFreeCount;
        } while (InterlockedTestExchange(&PageFreeCount,pfc,pfc+dwPages) != (LONG)pfc);
    }
foundPages:
    DEBUGMSG(ZONE_PHYSMEM,(TEXT("GetContiguousPages: Returning %8.8lx\r\n"), paRet));
    return paRet;
}



ERRFALSE(sizeof(SECTION) == 2048);

#if PAGE_SIZE == 4096


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PSECTION 
GetSection(void)
{
    PHYSICAL_ADDRESS pAddr;
    if ((PageFreeCount > 1+MIN_PROCESS_PAGES) && HoldPages(1, FALSE)) {
        if (pAddr = GetHeldPage()) {
            InterlockedIncrement(&KInfoTable[KINX_SYSPAGES]);
            return Phys2Virt(pAddr); // can't put GetHeldPage, since Phys2Virt's a macro!
        }
        InterlockedIncrement(&PageFreeCount);
    }
    return 0;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
FreeSection(
    PSECTION pscn
    ) 
{
#if defined (MIPS) || defined (SHx)
	// MIPS and SHx: flush TLB since the same ASID can be reused by new process
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_DISCARD | CACHE_SYNC_FLUSH_TLB);
#else
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_DISCARD);
#endif
    FreePhysPage(GetPFN(pscn));
    InterlockedDecrement(&KInfoTable[KINX_SYSPAGES]);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID 
GetHelperStack(void) 
{
    return AllocMem(HEAP_HLPRSTK);
//  return GetSection();
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
FreeHelperStack(
    LPVOID pMem
    ) 
{
    FreeMem(pMem,HEAP_HLPRSTK);
//  FreeSection(pMem);
}

#else

ERRFALSE((sizeof(SECTION)+PAGE_SIZE-1)/PAGE_SIZE == 2);



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
FreeSection(
    PSECTION pscn
    ) 
{
    PHYSICAL_ADDRESS paSect;
    paSect = GetPFN(pscn);
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_DISCARD);
    FreePhysPage(paSect);
    FreePhysPage(NextPFN(paSect));
    InterlockedDecrement(&KInfoTable[KINX_SYSPAGES]);
    InterlockedDecrement(&KInfoTable[KINX_SYSPAGES]);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPBYTE 
TakeTwoPages(
    PFREEINFO pfi, 
    uint ix, 
    LPBYTE pMem
    ) 
{
    KCALLPROFON(40);
    if (pfi->pUseMap[ix] || pfi->pUseMap[ix+1]) {
        KCALLPROFOFF(40);
        return 0;
    }
    pfi->pUseMap[ix] = 1;
    pfi->pUseMap[ix+1] = 1;
    UnlinkPhysPage(pMem);
    UnlinkPhysPage(pMem+PAGE_SIZE);
    KCALLPROFOFF(40);
    return pMem;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PSECTION 
GetSection(void) 
{
    PFREEINFO pfi, pfiEnd;
    uint ix;
    PHYSICAL_ADDRESS paSect;
    PSECTION pscn = 0;
    LPVOID pMem;
    while (pMem = InterlockedPopList(&pFreeKStacks))
        FreeHelperStack(pMem);
    if ((PageFreeCount > 2+MIN_PROCESS_PAGES) && HoldPages(2, FALSE)) {
        pfi = &MemoryInfo.pFi[0];
        for (pfi = &MemoryInfo.pFi[0], pfiEnd = pfi+MemoryInfo.cFi ; pfi < pfiEnd ; ++pfi) {
            paSect = pfi->paEnd - 2 * PFN_INCR;
            ix = (paSect - pfi->paStart) / PFN_INCR;
            while (paSect >= pfi->paStart) {
                if (pscn = (PSECTION)KCall((PKFN)TakeTwoPages,pfi,ix,Phys2Virt(paSect))) {
                    InterlockedIncrement(&KInfoTable[KINX_SYSPAGES]);
                    InterlockedIncrement(&KInfoTable[KINX_SYSPAGES]);
                    goto foundPages;
                }
                ix--;
                paSect -= PFN_INCR;
            }
        }
        InterlockedIncrement(&PageFreeCount);
        InterlockedIncrement(&PageFreeCount);
    }
foundPages:
    DEBUGMSG(ZONE_PHYSMEM,(TEXT("GetSection: Returning %8.8lx\r\n"), pscn));
    return pscn;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID 
GetHelperStack(void) 
{
    PHYSICAL_ADDRESS pAddr;
    if (HoldPages(1, FALSE)) {
        if (pAddr = GetHeldPage()) {
            InterlockedIncrement(&KInfoTable[KINX_SYSPAGES]);
            return Phys2Virt(pAddr); // can't put GetHeldPage, since Phys2Virt's a macro!
        }
        InterlockedIncrement(&PageFreeCount);
    }
    return 0;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
FreeHelperStack(
    LPVOID pMem
    ) 
{
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_DISCARD);
    FreePhysPage(GetPFN(pMem));
    InterlockedDecrement(&KInfoTable[KINX_SYSPAGES]);
}

#endif

