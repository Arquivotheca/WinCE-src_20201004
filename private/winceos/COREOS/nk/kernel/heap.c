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
/*
 *      NK Kernel heap code
 *
 *
 * Module Name:
 *
 *      heap.c
 *
 * Abstract:
 *
 *      This file implements the NK kernel heap
 *
 */

/* The kernel heap is designed to support objects which are smaller than one page
   in length.  The design assumes that kernel objects are either a divisor of the
   page size, or small enough that the remainder when the page size is divided by
   the object size is very small.
   The design of the heap is a structure (heapptr) which contains the size of each
   object, whether the memory is to be zero'ed by the alloc routine for that object,
   and a pointer to a list of pages.  Pages, once added to a pool, are never freed.
   When a page is added to a pool, it is linked into the master pointer for that
   pool as a linked list of objects.  Each object has as it's first DWORD a pointer
   to the next object on the list.  So, to allocate an object, if the pointer it
   non-null, the block pointed to is returned, and the new pointer is set to be
   the first DWORD of where the pointer was pointing.  If the pointer is NULL, then
   a new page is threaded onto the list, and the same procedure is followed.  When
   an object is freed, it is simply added to the head of the list.  Because the
   pool number is passed to the free and alloc routines, and since the block sizes
   are fixed at compile time, it is very efficient time-wise. */

#include "kernel.h"

#if defined(MIPS)
//  Quadword align all elements on the heap to allow use of LD and SD instructions.
#define ALIGNED_SIZE(n) ((HEAP_SIZE ## n + 7) & ~7)
#else   //  MIPS
#define ALIGNED_SIZE(n) ((HEAP_SIZE ## n + 3) & ~3)
#endif  //  MIPS

#define HEAPENTRY(n) {ALIGNED_SIZE(n), 0, HEAP_NAME ## n, 0, 0, 0}

static heapptr_t heapptr[NUMARENAS] = {
    HEAPENTRY(0),   HEAPENTRY(1),   HEAPENTRY(2),   HEAPENTRY(3),
    HEAPENTRY(4),   HEAPENTRY(5),   HEAPENTRY(6),   HEAPENTRY(7)
};

static LONG g_lIndex; // ptr to arena to receive tail of heap page
static uint SmallestSize;

#if defined (ARM) || defined (x86)
#define MAX_KHEAP_SIZE      0x4000000      // 64M max

LPBYTE g_pKHeapBase;
LPBYTE g_pCurKHeapFree;
#endif

//------------------------------------------------------------------------------
// Distrubute unusable bytes on the end of a heap page to the heap item
// free lists.
//------------------------------------------------------------------------------
static void FreeSpareBytes (LPBYTE lpb, uint size) 
{
    LONG index;
    heapptr_t *hptr;

    DEBUGMSG(ZONE_MEMORY, (TEXT("FSB: %8.8lx (%d)\r\n"), lpb, size));
    do {
        index = g_lIndex;
    } while (InterlockedTestExchange(&g_lIndex, index, (index+1)%NUMARENAS) != index);

    if (size >= heapptr[3].size) {
        InterlockedPushList(&heapptr[3].fptr, lpb);
        InterlockedIncrement(&heapptr[3].cMax);
        size -= heapptr[3].size;
        lpb += heapptr[3].size;
    }


    while (size >= SmallestSize) {
        hptr = &heapptr[index];
        if (size >= hptr->size) {
            InterlockedPushList(&hptr->fptr, lpb);
            InterlockedIncrement(&hptr->cMax);
            size -= hptr->size;
            lpb += hptr->size;
        }
        index = (index+1) % NUMARENAS;
    }
    DEBUGMSG(ZONE_MEMORY, (TEXT("FSB: waste=%d\r\n"), size));
    KInfoTable[KINX_HEAP_WASTE] += size;
}



//------------------------------------------------------------------------------
// Initialize the kernel heap
//------------------------------------------------------------------------------
void HeapInit (void) 
{
    KInfoTable[KINX_KHEAP] = (long)heapptr;
    /* thread must be 16 byte aligned */
    heapptr[HEAP_THREAD].size = (heapptr[HEAP_THREAD].size + 0xf) & ~0xf;
    /* Find the smallest size of block. */
    SmallestSize = heapptr[0].size;
    KInfoTable[KINX_HEAP_WASTE] = 0;

#if defined (ARM) || defined (x86)
    g_pprcNK->vaFree = ALIGNUP_4M (g_pprcNK->vaFree);
    g_pKHeapBase = (LPBYTE) VMReserve (g_pprcNK, MAX_KHEAP_SIZE, 0, 0);
    DEBUGCHK (g_pKHeapBase);
    g_pCurKHeapFree = g_pKHeapBase;
    g_pKData->dwKVMStart = (DWORD) g_pKHeapBase + MAX_KHEAP_SIZE;
#endif
#ifdef DEBUG
{
    int loop;
#if defined (ARM) || defined (x86)
    DEBUGLOG (ZONE_MEMORY, g_pKHeapBase);
#endif
    for (loop = 0; loop < NUMARENAS; loop++) {
        DEBUGMSG(ZONE_MEMORY,(TEXT("HeapInit: Entry %2.2d, size %4.4d\r\n"),loop, heapptr[loop].size));
        DEBUGCHK(IsDwordAligned(heapptr[loop].size));
    }
    DEBUGMSG(ZONE_MEMORY,(TEXT("HeapInit: SmallestSize = %4.4d\r\n"), SmallestSize));
}
#endif
}

static LPBYTE pFree;
static DWORD dwLen;

//------------------------------------------------------------------------------
// GetKHeap - find free memory of a particular size
//------------------------------------------------------------------------------
static LPVOID GetKHeap (DWORD size)
{
    LPBYTE pRet = NULL;

    AcquireMemoryLock (21);
    
    if (dwLen >= size) {
        pRet = pFree;
        dwLen -= size;
        pFree += size;
        ReleaseMemoryLock (21);

    } else {
        DWORD flags, dwPFN;
        
        ReleaseMemoryLock (21);
#if defined (ARM) || defined (x86)
        if (g_pCurKHeapFree < (g_pKHeapBase + MAX_KHEAP_SIZE)) {
            
            flags = 0;
            dwPFN = PHYS_GrabFirstPhysPage (1, &flags, FALSE);

            if (INVALID_PHYSICAL_ADDRESS != dwPFN) {

                pRet = (LPBYTE) InterlockedExchangeAdd ((PLONG) &g_pCurKHeapFree, VM_PAGE_SIZE);

                if (pRet >= (g_pKHeapBase + MAX_KHEAP_SIZE)) {
                    // If we're in Syscall, we will never be able to survive beyond this.
                    FreePhysPage (dwPFN);
                    pRet = NULL;
                    
                } else {
                    // map the page at pRet
                    VERIFY (VMCreateKernelPageMapping (pRet, dwPFN));

                }
            }
        }
        // There must be a serious memory leakage for GetKHeap to be full.
        ERRORMSG (g_pCurKHeapFree >= (g_pKHeapBase + MAX_KHEAP_SIZE), (L"GetKHeap: Kernel Heap FULL!!!\r\n"));
        DEBUGCHK (g_pCurKHeapFree < (g_pKHeapBase + MAX_KHEAP_SIZE));
#else
        // MIPS/SHx - process and thread structure needs to be in non-TLB addresses. For
        //            they can be accessed in code that cannot take TLB miss.
        flags = PM_PT_STATIC_ONLY;
        dwPFN = PHYS_GrabFirstPhysPage (1, &flags, FALSE);
        if (INVALID_PHYSICAL_ADDRESS != dwPFN) {
            pRet = Pfn2Virt (dwPFN);
        }
#endif

        if (pRet) {

            LPBYTE pNeedsFree     = pRet + size;
            DWORD  dwNeedsFreeLen = VM_PAGE_SIZE - size;

            InterlockedIncrement((PLONG)&KInfoTable[KINX_SYSPAGES]);

            AcquireMemoryLock (21);
            
            if (dwLen < VM_PAGE_SIZE - size) {
                pNeedsFree = pFree;
                dwNeedsFreeLen = dwLen;
                pFree = pRet + size;
                dwLen = VM_PAGE_SIZE - size;
            }
            
            ReleaseMemoryLock (21);

            FreeSpareBytes(pNeedsFree,dwNeedsFreeLen);
        }

    }
    
    return pRet;
}


//------------------------------------------------------------------------------
// Allocate a block from the kernel heap
//------------------------------------------------------------------------------
LPVOID AllocMem (ulong poolnum) 
{
    LPBYTE pptr;
    heapptr_t *hptr;
    DEBUGCHK(poolnum<NUMARENAS);

    //DEBUGMSG (ZONE_MEMORY, (L"+AllocMem %d\r\n", poolnum));
    hptr = &heapptr[poolnum];
    pptr = InterlockedPopList(&hptr->fptr);
    if (!pptr) {
        pptr = GetKHeap(hptr->size);
        if (!pptr) {
            return 0;
        }
        InterlockedIncrement(&hptr->cMax);
    }
    InterlockedIncrement(&hptr->cUsed);
    //DEBUGMSG (ZONE_MEMORY, (L"-AllocMem %8.8x\r\n", pptr));
    return pptr;
    
}



//------------------------------------------------------------------------------
// Free a block from the kernel heap
//------------------------------------------------------------------------------
VOID FreeMem (LPVOID pMem, ulong poolnum) 
{
    heapptr_t *hptr;
    DEBUGCHK(poolnum<NUMARENAS);

    hptr = &heapptr[poolnum];
#ifdef DEBUG
    if (HEAP_THREAD == poolnum) {
        // we need to save/restore wCount and wCount2 to keep the sequence number
        // going.
        PTHREAD pth = (PTHREAD) pMem;
        WORD wCount, wCount2;
        wCount = pth->wCount;
        wCount2 = pth->wCount2;
        memset(pMem,0xab,hptr->size);
        pth->wCount = ++ wCount;
        pth->wCount2 = ++ wCount2;
    } else {
        memset(pMem,0xab,hptr->size);
    }
#endif

    DEBUGCHK ((int) hptr->cUsed >0);

    InterlockedPushList(&hptr->fptr, pMem);
    InterlockedDecrement(&hptr->cUsed);
}



//------------------------------------------------------------------------------
// allocate a variable length block for memory pool
//------------------------------------------------------------------------------
PNAME AllocName (DWORD dwLen) 
{
    DWORD dwPoolNum;
    PNAME lpn = NULL;
    
    dwLen += 2; // for pool number

    for (dwPoolNum = 0; dwPoolNum < NUMARENAS; dwPoolNum++) {
        if (heapptr[dwPoolNum].size >= dwLen) {
            lpn = AllocMem (dwPoolNum);
            if (lpn) {
                lpn->wPool = (WORD) dwPoolNum;
            }
            break;
        }
    }
    return lpn;
}

BOOL GetKHeapInfo (DWORD idx, LPVOID pBuf, DWORD cbBuf, LPDWORD pcbRet)
{
    DWORD dwErr = 0;
    __try {
        if (idx >= NUMARENAS) {
            dwErr = ERROR_INVALID_PARAMETER;
        } else {
            heapptr_t *hptr = &heapptr[idx];
            DWORD len = strlen (hptr->classname) + 1;
            if (sizeof (heapptr_t) + len > cbBuf) {
                dwErr = ERROR_INSUFFICIENT_BUFFER;
            } else {
                heapptr_t *hptrDst = (heapptr_t *) pBuf;
                memcpy (hptrDst, hptr, sizeof (heapptr_t));
                memcpy (hptrDst+1, hptr->classname, len);
                hptrDst->classname = (const char *) (hptrDst + 1);
            }
            *pcbRet = sizeof (heapptr_t) + len;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwErr = ERROR_INVALID_PARAMETER;
    }

    NKSetLastError (dwErr);
    return !dwErr;
}

#ifdef DEBUG
int ShowKernelHeapInfo(void)
{
    int loop;
    heapptr_t *hptr;
    ulong cbUsed, cbExtra, cbMax;
    ulong cbTotalUsed, cbTotalExtra;
    hptr = (heapptr_t *)UserKInfo[KINX_KHEAP];
    cbTotalUsed = cbTotalExtra = 0;

    NKDbgPrintfW (L"Page size=%d, %d total pages, %d free pages. %d MinFree pages (%d MaxUsed bytes)\r\n",
            VM_PAGE_SIZE, KInfoTable[KINX_NUMPAGES],
            KInfoTable[KINX_PAGEFREE], KInfoTable[KINX_MINPAGEFREE],
            VM_PAGE_SIZE * (KInfoTable[KINX_NUMPAGES]-KInfoTable[KINX_MINPAGEFREE]));
    NKDbgPrintfW (L"%d pages used by kernel, %d pages held by kernel, %d pages consumed.\r\n",
           KInfoTable[KINX_SYSPAGES],KInfoTable[KINX_KERNRESERVE],
           KInfoTable[KINX_NUMPAGES]-KInfoTable[KINX_PAGEFREE]);
    
    NKDbgPrintfW(L"Inx Size   Used    Max Extra  Entries Name\r\n");
    for (loop = 0 ; loop < NUMARENAS ; ++loop, ++hptr) {
        cbUsed = hptr->size * hptr->cUsed;
        cbMax = hptr->size * hptr->cMax;
        cbExtra = cbMax - cbUsed;
        cbTotalUsed += cbUsed;
        cbTotalExtra += cbExtra;
        NKDbgPrintfW (L"%2d: %4d %6ld %6ld %5ld %3d(%3d) %hs\r\n", loop, hptr->size,
                cbUsed, cbMax, cbExtra, hptr->cUsed, hptr->cMax,
                hptr->classname);
    }
    NKDbgPrintfW (L"Total Used = %ld  Total Extra = %ld  Waste = %d\r\n",
            cbTotalUsed, cbTotalExtra, KInfoTable[KINX_HEAP_WASTE]);

    return 0;
}
#endif

//
// generic heap support
//

static PFN_HeapCreate   g_pfnHeapCreate;
static PFN_HeapDestroy  g_pfnHeapDestroy;
static PFN_HeapAlloc    g_pfnHeapAlloc;
static PFN_HeapReAlloc  g_pfnHeapReAlloc;
static PFN_HeapFree     g_pfnHeapFree;
static PFN_HeapCompact  g_pfnHeapCompact;

static HANDLE g_hKernelHeap;        // generic kernel heap for allocations that is not time critical.

//
// creating a generic kernel heap (variable sized)
//
HANDLE NKCreateHeap (void)
{
    HANDLE hHeap = NULL;
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    if (!KSEN_CreateHeap()) {
        DEBUGCHK (g_pfnHeapCreate);
        hHeap = (* g_pfnHeapCreate) (0, 0, 0);
    }
    KSLV_CreateHeap (&hHeap);
    SwitchActiveProcess (pprc);
    return hHeap;
}

LPVOID NKHeapAlloc (HANDLE hHeap, DWORD cbSize)
{
    LPVOID lpRet;
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    __try {
        lpRet = (* g_pfnHeapAlloc) (hHeap, 0, cbSize);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        lpRet = NULL;
    }
    SwitchActiveProcess (pprc);
    return lpRet;
}

BOOL NKHeapFree (HANDLE hHeap, LPVOID ptr)
{
    BOOL fRet;
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    __try {
        fRet = (* g_pfnHeapFree) (hHeap, 0, ptr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        fRet = FALSE;
    }
    SwitchActiveProcess (pprc);
    return fRet;
}

BOOL NKHeapCompact (HANDLE hHeap)
{
    BOOL fRet;
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    __try {
        fRet = (* g_pfnHeapCompact) (hHeap, 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        fRet = FALSE;
    }
    SwitchActiveProcess (pprc);
    return fRet;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID NKmalloc (DWORD cbSize)
{
    return NKHeapAlloc (g_hKernelHeap, cbSize);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL NKfree (LPVOID ptr)
{
    return NKHeapFree (g_hKernelHeap, ptr);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL NKcompact (void)
{
    return NKHeapCompact (g_hKernelHeap);
}


BOOL InitGenericHeap (HANDLE hKCoreDll)
{
    // get all the heap function pointers from kcoredll
    g_pfnHeapCreate     = (PFN_HeapCreate)  GetProcAddressA (hKCoreDll, (LPCSTR)44);
    g_pfnHeapDestroy    = (PFN_HeapDestroy) GetProcAddressA (hKCoreDll, (LPCSTR)45);
    g_pfnHeapAlloc      = (PFN_HeapAlloc)   GetProcAddressA (hKCoreDll, (LPCSTR)46);
    g_pfnHeapReAlloc    = (PFN_HeapReAlloc) GetProcAddressA (hKCoreDll, (LPCSTR)47);
    g_pfnHeapFree       = (PFN_HeapFree)    GetProcAddressA (hKCoreDll, (LPCSTR)49);
    g_pfnHeapCompact    = (PFN_HeapCompact) GetProcAddressA (hKCoreDll, (LPCSTR)1884);

    g_hKernelHeap = NKCreateHeap ();
    DEBUGCHK (g_hKernelHeap);

    return NULL != g_hKernelHeap;
}
