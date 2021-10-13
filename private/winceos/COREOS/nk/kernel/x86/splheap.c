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
//    splheap.c - specific heap special page implementation
//
#include <windows.h>
#include <vm.h>
#include <kernel.h>
#include <splheap.h>

#if SPL_HEAP_ENABLED

#include "..\\..\\core\\lmem\\heap.h"

// PTE entry for special heap pages marked as n/a
#define PG_HEAP_NOACCESS 0x00000800  // bit unused in x86 pte

// break into debugger on read access
BOOL g_fBreakOnReadAccess = FALSE; 

// set when the first heap page is marked as n/a
BOOL g_fHeapPagedOut; 

// set when we break into debugger to disable any further pageouts
BOOL g_fDisablePageOut;

// enumeration function
extern BOOL Enumerate2ndPT (PPAGEDIRECTORY ppdir, DWORD dwAddr, DWORD cPages, BOOL fWrite, PFN_PTEnumFunc pfnApply, LPVOID pEnumData);

// page enumeration struct
typedef struct _EnumHeapStruct {
    PPROCESS pprc;
    DWORD dwAddr;
} EnumHeapStruct, *PEnumHeapStruct;

//-----------------------------------------------------------------------------------------------------------------
//
// IsNoAccessPage - check if given entry is the special no access page.
//
static BOOL IsNoAccessPage (DWORD dwEntry) 
{
    return IsPageCommitted(dwEntry) && (PG_HEAP_NOACCESS & dwEntry);
}

//-----------------------------------------------------------------------------------------------------------------
//
// IsHeapFreeItem - check if given address falls within a free heap item.
// - must be from an embedded heap
// - vmlock held so none of the rgn pages can be unmapped
//
static BOOL IsHeapFreeItem (DWORD dwAddr)
{
    BOOL fRet = FALSE;
    DWORD dwSentinelSize = (*((LPDWORD)BLOCKALIGN_DOWN (dwAddr)) & 0xFF) * sizeof(DWORD);
    PRHRGN prrgn = (PRHRGN) (BLOCKALIGN_DOWN (dwAddr) + dwSentinelSize);

    // get the rgn ptr if this is heap ptr
    if (prrgn->cbSize == sizeof(RHEAP)) {
        prrgn = &(((PRHEAP)prrgn)->rgn);
    }

    // get the rgn allocmap for given addr
    if (prrgn) {
        DWORD idxAlloc;

        DEBUGCHK ((16 == dwSentinelSize) || (32 == dwSentinelSize));
        
        if (16 == dwSentinelSize) {
            idxAlloc = ((dwAddr & ~(0x0F)) - (DWORD)prrgn->pLocalBase) >> 4; // 16 byte aligned
        } else {
            idxAlloc = ((dwAddr & ~(0x1F)) - (DWORD)prrgn->pLocalBase) >> 5; // 32 byte aligned            
        }
        
        if (FreeListRgn(prrgn)) {
            DWORD idxSubFree = IDX2SUBRGNIDX(idxAlloc, prrgn);
            DWORD idxBlkFree = IDX2FREELISTIDX(idxAlloc, prrgn);
            PSUBRGN pSubRgn = IDX2SUBRGN(idxSubFree, prrgn);    
            LPBYTE pFreeList = SUBRGNFREELIST(pSubRgn->idxFreeList, prrgn);
            fRet = (pFreeList[idxBlkFree] != SUBRGN_ALLOC_BLK);
            
        } else {
            DWORD idxBitmap = IDX2BITMAPDWORD(idxAlloc);
            DWORD idxSubBlk = IDX2BLKINBITMAPDWORD(idxAlloc);
            DWORD dwBitmap  = prrgn->allocMap[idxBitmap];
            if (idxSubBlk) {
                dwBitmap >>= (idxSubBlk << 1);
            }
            fRet = (RHF_FREEBLOCK == (dwBitmap & RHF_BITMASK));
        }
    }    

    return fRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// CommitSplHeapPage - commit the "special" heap page marked as n/a or r/o
//
static DWORD CommitSplHeapPage (DWORD dwEntry, DWORD dwAddr, BOOL fWrite)
{
    // mark the page valid
    dwEntry |= (PG_VALID_MASK | (!IsKModeAddr(dwAddr) ? PG_USER_MASK : 0));

    // if changing to r/w, remove the n/a bit
    if (fWrite) {
        dwEntry |= PG_WRITE_MASK;
        dwEntry &= ~PG_HEAP_NOACCESS;
    }            

    return dwEntry;
}

//-----------------------------------------------------------------------------------------------------------------
//
// Enumeration function to commit spl heap pages
//
static BOOL EnCommitSplHeapPage (LPDWORD pdwEntry, LPVOID pEnumData)
{
    DWORD dwEntry = *pdwEntry;
    PEnumHeapStruct phs = (PEnumHeapStruct)pEnumData;

    if (IsNoAccessPage(dwEntry)) {
        InterlockedCompareExchange((LONG*)pdwEntry, (LONG)CommitSplHeapPage (dwEntry, phs->dwAddr, TRUE), (LONG)dwEntry);
        InvalidatePages (phs->pprc, phs->dwAddr, 1);            
    }        

    phs->dwAddr += VM_PAGE_SIZE;
    return TRUE; // continue enumeration
}

//-----------------------------------------------------------------------------------------------------------------
//
// SplHeapProcessPageFault - Page fault for special heap page marked as n/a or r/o
//
// Note: caller should flush TLB if function returns TRUE
//
static BOOL SplHeapProcessPageFault (PPROCESS pprc, LPDWORD pdwEntry, DWORD dwAddr, BOOL fWrite, DWORD dwPc)
{
    BOOL fRet = (NULL != LockVM(pprc));

    if (fRet) {        
        BOOL fFreeItem = FALSE;
        DWORD dwEntry = *pdwEntry; // after vm lock
        BOOL fSplHeapPage = IsNoAccessPage (dwEntry);

        fWrite |= !dwPc; // if internal, always page in r/w

        // check if the entry is updated by another thread
        fRet = fWrite ? IsPageWritable(dwEntry) : IsPageReadable(dwEntry);

        // commit the heap page as r/o or r/w
        if (!fRet && fSplHeapPage) {
            InterlockedCompareExchange((LONG*)pdwEntry, (LONG)CommitSplHeapPage (dwEntry, dwAddr, fWrite), (LONG)dwEntry);
            InvalidatePages (pprc, dwAddr, 1);            
            fRet = TRUE;
        }
        
        // is this a free item?
        fFreeItem = fRet && dwPc && IsHeapFreeItem(dwAddr & ~3L);

        // check for free item access
        if (fFreeItem) {
            BOOL NoFaultSet = IsNoFaultSet(pCurThread->tlsPtr);

            //
            // Fail the page fault call (will raise exception to caller):
            // - write access to the freed heap item
            // - read access to a kernel mode heap item and no fault is not set
            //
            fRet = (!fWrite && (!IsKModeAddr(dwAddr) || NoFaultSet));
            
            NKDbgPrintfW (L"[HEAPCHECKS] Heap use-after-free detected. addr:0x%8.8lx pc:0x%8.8lx r/w:%d\r\n", dwAddr, dwPc, fWrite);

            if (!fRet || (g_fBreakOnReadAccess && !NoFaultSet)) {
                PVALIST pitem;                
                g_fDisablePageOut = TRUE; // disable heap page outs

                // page in all heap pages currently marked as n/a
                for (pitem = pprc->pVaList; pitem; pitem = pitem->pNext) {
                    EnumHeapStruct en = {pprc, (DWORD)pitem->pVaBase};
                    Enumerate2ndPT(pprc->ppdir, (DWORD)pitem->pVaBase, PAGECOUNT(pitem->cbVaSize), TRUE, EnCommitSplHeapPage, &en);
                }

                // break in
                DebugBreak();                

                g_fDisablePageOut = FALSE; // enable heap page outs
            }
        }

        UnlockVM (pprc);
    }

    return fRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// SplHeapCommitPage - commit heap pages marked as n/a or r/o
// This is called in a non-preemptible context from LoadPageTable.
// Return TRUE only if page fault is within known modules
// (coredll, kcoredll, and kernel)
//
// Note: caller should flush TLB if function returns TRUE
//
static BOOL SplHeapCommitPage (DWORD dwAddr, DWORD dwPc, LPDWORD pdwEntry)
{
    BOOL fRet = FALSE;
    DWORD dwEntry;
        
    do {
        dwEntry = *pdwEntry;                
        fRet = IsNoAccessPage (dwEntry);
    } while(fRet && (dwEntry != (DWORD)InterlockedCompareExchange((LONG*)pdwEntry, (LONG)CommitSplHeapPage (dwEntry, dwAddr, TRUE), (LONG)dwEntry)));

    return fRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// KSEN_ProcessPageFault - Shim for ProcessPageFault entry
// 1: shim handled this call (skip normal processing)
// 0: shim bypassed this call (continue normal processing)
//
BOOL KSEN_ProcessPageFault (PPROCESS pprc, DWORD dwAddr, BOOL fWrite, DWORD dwPc, DWORD dwEntry) 
{
    return IsNoAccessPage (dwEntry);
}

//-----------------------------------------------------------------------------------------------------------------
//
// KSLV_ProcessPageFault - Shim for ProcessPageFault exit
//
void KSLV_ProcessPageFault (PPROCESS pprc, DWORD dwAddr, BOOL fWrite, DWORD dwPc, LPDWORD pdwEntry, LPBOOL pfRet) 
{
    if (!*pfRet) {
        *pfRet = SplHeapProcessPageFault (pprc, pdwEntry, dwAddr, fWrite, dwPc);
    }
}

//-----------------------------------------------------------------------------------------------------------------
//
// KSEN_LoadPageTable - Shim for LoadPageTable entry
// 1: shim handled this call (skip normal processing)
// 0: shim bypassed this call (continue normal processing)
//
BOOL KSEN_LoadPageTable (DWORD addr, DWORD dwEip, DWORD dwErrCode) 
{
    return g_fHeapPagedOut;
}

//-----------------------------------------------------------------------------------------------------------------
//
// KSLV_LoadPageTable - Shim for LoadPageTable exit
//
void KSLV_LoadPageTable (DWORD addr, DWORD dwEip, LPDWORD pdwEntry, LPBOOL pfRet) 
{
    if (!*pfRet) {
        *pfRet = SplHeapCommitPage (addr, dwEip, pdwEntry);
    }            
}

//-----------------------------------------------------------------------------------------------------------------
//
// KSEN_ProtectFromEntry - Shim for ProtectFromEntry entry
// 1: shim handled this call (skip normal processing)
// 0: shim bypassed this call (continue normal processing)
//
BOOL KSEN_ProtectFromEntry (DWORD dwEntry) 
{
    return FALSE;
}

//-----------------------------------------------------------------------------------------------------------------
//
// KSLV_ProtectFromEntry - Shim for ProtectFromEntry exit
//
void KSLV_ProtectFromEntry (DWORD dwEntry, LPDWORD pdwProtect) 
{
    if (PG_HEAP_NOACCESS & dwEntry) {
        *pdwProtect |= PAGE_HEAP_NOACCESS;
    }
}

//-----------------------------------------------------------------------------------------------------------------
//
// KSEN_PageParamFormProtect - Shim for PageParamFormProtect entry
// 1: shim handled this call (skip normal processing)
// 0: shim bypassed this call (continue normal processing)
//
BOOL KSEN_PageParamFormProtect (PPROCESS pprc, DWORD fProtect, DWORD dwAddr) 
{
    return (PAGE_HEAP_NOACCESS & fProtect);    
}

//-----------------------------------------------------------------------------------------------------------------
//
// KSLV_PageParamFormProtect - Shim for PageParamFormProtect exit
//
void KSLV_PageParamFormProtect (PPROCESS pprc, DWORD fProtect, DWORD dwAddr, LPDWORD pdwRet) 
{
    if (PAGE_HEAP_NOACCESS & fProtect) {
        *pdwRet = PG_HEAP_NOACCESS;
    }
}

//-----------------------------------------------------------------------------------------------------------------
//
// KSEN_CreateHeap - Shim for CreateHeap entry
// 1: shim handled this call (skip normal processing)
// 0: shim bypassed this call (continue normal processing)
//
BOOL KSEN_CreateHeap ()
{
    return FALSE;    
}

//-----------------------------------------------------------------------------------------------------------------
//
// KSLV_CreateHeap - Shim for CreateHeap exit
//
void KSLV_CreateHeap (LPHANDLE lphHeap)
{
    PRHEAP php = (PRHEAP)(*lphHeap);
    php->flOptions &= ~HEAP_FREE_CHECKING_ENABLED;
}

//-----------------------------------------------------------------------------------------------------------------
//
// KSEN_VirtualProtect - Shim for VirtualProtect entry
// 1: shim handled this call (skip normal processing)
// 0: shim bypassed this call (continue normal processing)
//
BOOL KSEN_VirtualProtect (PPROCESS pprc, DWORD dwAddr, DWORD cPages, DWORD fNewProtect) 
{
    BOOL fRet = (PAGE_HEAP_NOACCESS & fNewProtect) && !g_fDisablePageOut;
    g_fHeapPagedOut |= fRet;
    return fRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// KSLV_VirtualProtect - Shim for VirtualProtect exit
//
void KSLV_VirtualProtect (PPROCESS pprc, DWORD dwAddr, DWORD cPages, DWORD fNewProtect, LPDWORD pdwRet) 
{
}

#endif // SPL_HEAP_ENABLED