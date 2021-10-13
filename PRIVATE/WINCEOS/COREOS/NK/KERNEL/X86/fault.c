//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

/*+
    fault.c - iX86 fault handlers
 */
#include "kernel.h"

// disable short jump warning.
#pragma warning(disable:4414)

///#define LIGHTS(n)   mov dword ptr ss:[0AA001010h], ~(n)&0xFF

extern RETADDR ServerCallReturn(PSVRRTNSTRUCT psrs);
extern RETADDR CallbackReturn (LPDWORD pLinkage);
extern RETADDR ObjectCall (POBJCALLSTRUCT pobs);
extern RETADDR MapArgs(const CINFO *pci, int iMethod, void *args);
extern BOOL HandleException(PTHREAD pth, int id, ulong addr);
extern void NextThread(void);
extern void KCNextThread(void);
extern void OEMIdle(void);
extern KTSS MainTSS;
extern void Reschedule(void);
extern void RunThread(void);
extern void DumpTctx(PTHREAD pth, int id, ulong addr, int level);

extern DWORD (*pfnOEMIntrOccurs) (DWORD dwSysIntr);

RETADDR PerformCallBackExt (POBJCALLSTRUCT pobs);

extern unsigned __int64 g_aGlobalDescriptorTable[];
extern CRITICAL_SECTION VAcs;

#ifdef NKPROF
extern void ProfilerHit(unsigned long ra);
extern void CeLogInterrupt(DWORD dwLogValue);
#endif
void CeLogThreadMigrate(HANDLE hProcess, DWORD dwReserved);

#define LOAD_SEGS    0

#define ADDR_SLOT_SIZE  0x02000000
#define PDES_PER_SLOT   (ADDR_SLOT_SIZE / 1024 / PAGE_SIZE)

#define NUM_SLOTS        32
#define PID_TO_PT_INDEX(pid) ((pid+1) * PDES_PER_SLOT)


#define BLOCKS_PER_PAGE_TABLE (1024 / PAGES_PER_BLOCK)
#define BLOCK_SIZE (PAGES_PER_BLOCK * PAGE_SIZE)

#define SYSCALL_INT         0x20
#define KCALL_INT           0x22

#define PT_PTR_TO_INDEX(pp) ((pp) - g_pptbl)

extern PPAGETABLE g_pPageDir;
extern DWORD ProcessorFeatures;

FXSAVE_AREA g_InitialFPUState;
PTHREAD g_CurFPUOwner;

DWORD  MD_CBRtn;
#ifdef NKPROF
PTHREAD pthFakeStruct;
#endif

//
//  CR0 bit definitions for numeric coprocessor
//
#define MP_MASK     0x00000002
#define EM_MASK     0x00000004
#define TS_MASK     0x00000008
#define NE_MASK     0x00000020

#define NPX_CW_PRECISION_MASK   0x300
#define NPX_CW_PRECISION_24     0x000
#define NPX_CW_PRECISION_53     0x200
#define NPX_CW_PRECISION_64     0x300


#define VA_TO_PD_IDX(va)        ((DWORD) (va) >> 22)        // (va)/4M == PDE idx
#define PD_IDX_TO_VA(idx)       ((DWORD) (idx) << 22)      // PDE idx * 4M == va base


#define PERFORMCALLBACK     -113    // MUST be -PerformCallback Win32Methods in kwin32.c 
                                    // 113 == -(APISet 0, method 113)
#define RAISEEXCEPTION      -114    // MUST be -RaiseException Win32Methods in kwin32.c
#define hCurThd  [KData].ahSys[SH_CURTHREAD*4]
#define PtrCurThd  [KData].pCurThd
#define PtrCurProc  [KData].pCurPrc

#define THREAD_CTX_ES  (THREAD_CONTEXT_OFFSET+8)
ERRFALSE(8 == offsetof(CPUCONTEXT, TcxEs));
#define THREAD_CTX_EDI  (THREAD_CONTEXT_OFFSET+16)
ERRFALSE(16 == offsetof(CPUCONTEXT, TcxEdi));

#define Naked void __declspec(naked)

// #define ONE_ENTRY

#pragma warning(disable:4035)               // Disable warning about no return value

//
// The Physical to Virtual mapping table is supplied by OEM.
//
extern PPTE g_pOEMAddressTable;

BOOL MDValidateRomChain (ROMChain_t *pROMChain)
{
    PPTE ppte;
    DWORD dwEnd;
    
    for ( ; pROMChain; pROMChain = pROMChain->pNext) {
        for (ppte = g_pOEMAddressTable; ppte->dwSize; ppte ++) {
            dwEnd = ppte->dwVA + ppte->dwSize;
            if (IsInRange (pROMChain->pTOC->physfirst, ppte->dwVA, dwEnd)) {
                if (IsInRange (pROMChain->pTOC->physlast, ppte->dwVA, dwEnd)) {
                    // good XIP, break inner loop and go on to the next region
                    break;
                }
                // bad
                NKDbgPrintfW (L"MDValidateRomChain: XIP (%8.8lx -> %8.8lx) span accross multiple memory region\r\n",
                        pROMChain->pTOC->physfirst, pROMChain->pTOC->physlast);
                return FALSE;
            }
        }
        if (!ppte->dwSize) {
            NKDbgPrintfW (L"MDValidateRomChain: XIP (%8.8lx -> %8.8lx) doesn't exist in OEMAddressTable \r\n",
                        pROMChain->pTOC->physfirst, pROMChain->pTOC->physlast);
            return FALSE;
        }
    }
    return TRUE;
}


extern CRITICAL_SECTION VAcs;
LPBYTE GrabFirstPhysPage(DWORD dwCount);

// hardware pagetable for the 2nd gig address and secure section
ulong gHwPTBL2G[32*HARDWARE_PT_PER_PROC];    // 32 == shared section + mapping address from 0x42000000 -> 0x7fffffff
DWORD gdwSlotTouched, gfObjStoreTouched;

#define MAPPER_SLOT_TO_PTBL(slot)   (gHwPTBL2G + HARDWARE_PT_PER_PROC * ((slot) - 32))

MEMBLOCK *MDAllocMemBlock (DWORD dwBase, DWORD ixBlock)
{
    LPDWORD pPtbls;                             // which hardware PT to use
    DWORD   ixTbl  = ixBlock >> 6;              // ixTbl == which 4M block
    MEMBLOCK *pmb;

    DEBUGMSG (ZONE_VIRTMEM, (L"MDAllocMemBlock: dwBase = %8.8lx, ixBlock = %8.8lx\r\n", dwBase, ixBlock));
    DEBUGCHK (!IsKernelVa (dwBase));
    DEBUGCHK (!VAcs.hCrit || (VAcs.OwnerThread == hCurThread));

    switch (dwBase) {
    case SECURE_VMBASE:
        // secure section, use NK's page table
        pPtbls = ProcArray[0].pPTBL;
        break;
        
    case MODULE_BASE_ADDRESS:
        // module section (slot 1), use 1st 8 entries of gHwPTBL2G
        pPtbls = gHwPTBL2G;
        break;
        
    case 0:
        // current process
        pPtbls = pCurProc->pPTBL;
        break;
        
    default:
        // either a sloted process address or 2nd gig
        if (dwBase < FIRST_MAPPER_ADDRESS) {
            // process address
            pPtbls = ProcArray[(dwBase >> VA_SECTION) - 1].pPTBL;
        } else {
            // in the 2nd gig
            pPtbls = MAPPER_SLOT_TO_PTBL(dwBase >> VA_SECTION);
        }
        break;
    }

    //
    // allocate a page if not allocate yet
    //
    if (!pPtbls[ixTbl]) {
        // allocate and setup top-level page table
        DWORD dwVA = (DWORD) KCall((PKFN)GrabFirstPhysPage,1);
        if (!dwVA) {
            // Out of memory
            return NULL;
        }

        // we're relying on the fact that the page we just got has already been zero'd
        // or we need to memset it to 0 here.
        pPtbls[ixTbl] = LIN_TO_PHYS (dwVA) | ((SECURE_VMBASE == dwBase)? PG_KRN_READ_WRITE : PG_READ_WRITE);

        DEBUGMSG (ZONE_VIRTMEM, (L"MDAllocMemBlock: Allocated a new page for Page Table va = %8.8lx, entry = %8.8lx\r\n",
            dwVA, pPtbls[ixTbl]));

    }

    DEBUGMSG (ZONE_VIRTMEM, (L"MDAllocMemBlock: Allocating MemBlock\r\n"));
    // allocate MEM_BLOCK
    if (pmb = AllocMem (HEAP_MEMBLOCK)) {

        // initialize memblock
        memset (pmb, 0, sizeof(MEMBLOCK));

        // pagetable access needs to be uncached.
        pmb->aPages = (LPDWORD) (PHYS_TO_LIN (pPtbls[ixTbl] & -PAGE_SIZE) + ((ixBlock << 6) & (PAGE_SIZE-1)));

    }

    DEBUGMSG (ZONE_VIRTMEM, (L"MDAllocMemBlock: returning %8.8lx, aPages = %8.8lx\r\n", pmb, pmb->aPages));
    return pmb;
}

void FreeHardwarePT (DWORD dwVMBase)
{
    LPDWORD pPtbls;                             // which hardware PT to use
    int i;
    // can not free current process, shared section, or secure section
    DEBUGCHK ((dwVMBase != pCurProc->dwVMBase) && ((int) dwVMBase >= (2 << VA_SECTION)));
    DEBUGCHK (!VAcs.hCrit || (VAcs.OwnerThread == hCurThread));

    // clear first-level page table
    memset (&g_pPageDir->PTE[(dwVMBase >> 22)], 0, HARDWARE_PT_PER_PROC * sizeof (ULONG));

    if (dwVMBase < FIRST_MAPPER_ADDRESS) {
        pPtbls = ProcArray [(dwVMBase >> VA_SECTION) - 1].pPTBL;
        // per process
    } else {
        // 2nd gig
        DEBUGCHK ((dwVMBase >= FIRST_MAPPER_ADDRESS) && (dwVMBase < LAST_MAPPER_ADDRESS));
        pPtbls = MAPPER_SLOT_TO_PTBL (dwVMBase >> VA_SECTION);
    }
    for (i = 0; i < HARDWARE_PT_PER_PROC; i ++) {
        if (pPtbls[i]) {
            FreePhysPage (pPtbls[i] & -PAGE_SIZE);
            pPtbls[i] = 0;
        }
    }
    KCall ((FARPROC) OEMCacheRangeFlush, 0, 0, CACHE_SYNC_FLUSH_TLB);
}

void MDFreeMemBlock (MEMBLOCK *pmb)
{
    DEBUGCHK ((NULL_BLOCK != pmb) && (RESERVED_BLOCK != pmb));
    DEBUGCHK (!VAcs.hCrit || (VAcs.OwnerThread == hCurThread));
#ifdef DEBUG
    {
        int i;
        for (i = 0; i < PAGES_PER_BLOCK; i ++) {
            DEBUGCHK (!pmb->aPages[i] || (pmb->aPages[i] == BAD_PAGE));
        }
    }
#endif
    // need to zero it out incase there're bad pages.
    memset (pmb->aPages, 0, PAGES_PER_BLOCK * sizeof (ulong));
    FreeMem (pmb, HEAP_MEMBLOCK);
}

//------------------------------------------------------------------------------
// LoadPageTable: handle Page Fault exception
//      addr: address that causes the fault
//      dwErrCode: the error code. (reference: x86 spec)
//                  bit 0 (P): if page mapping is present
//                  bit 1 (R/W): Read or write access 
//                  bit 2 (U/S): user or supervisor mode
//------------------------------------------------------------------------------
#define PFCODE_PRESENT      0x01        // present
#define PFCODE_WRITE        0x02        // trying to write
#define PFCODE_USER_MODE    0x04        // in user mode

BOOL LoadPageTable (DWORD addr, DWORD dwErrCode)
{
    DWORD   dwSlot = addr >> VA_SECTION, aky = 0;
    DWORD   bitClear = 0;

    //DEBUGMSG (ZONE_VIRTMEM, (L"LoadPageTable: addr = %8.8lx\r\n", addr));
    if (!IsKernelVa(addr)       // not kernel address
        && (((dwSlot = addr >> VA_SECTION) <= 1)                    // current process or slot 1
            || (dwSlot > MAX_PROCESSES)                             // memmap area
            || ((aky = 1 << (dwSlot-1)) & CurAKey))) {              // slotted process address, have access to the slot

        LPDWORD pPtbls;
        DWORD   ix4M = (addr >> 22);

        switch (dwSlot) {
        case SECURE_SECTION:
            // secure slot, use NK's page table
            pPtbls = ProcArray[0].pPTBL;
            break;
        case 0:
            pPtbls = pCurProc->pPTBL;
            break;
            
        case MODULE_SECTION:
            // module section (slot 1), use 1st 8 entries of gHwPTBL2G
            pPtbls = gHwPTBL2G;
            break;
            
        case SHARED_SECTION:
            // write access requires kernel mode
            if ((dwErrCode & (PFCODE_WRITE|PFCODE_USER_MODE)) == (PFCODE_WRITE|PFCODE_USER_MODE)) {
                return FALSE;
            }

            // user mode read-only
            if (dwErrCode & PFCODE_USER_MODE) {
                bitClear = PG_WRITE_MASK;
                
            } else if (dwErrCode & PFCODE_PRESENT) {
                // kernel mode trying to write, while page directory still marked R/O
                // just clear the page directory and we'll fill in the right access
                // DEBUGCHK (dwErrCode & PFCODE_WRITE)
                g_pPageDir->PTE[ix4M] = 0;
            }

            // fall through
        default:
            // either a sloted process address or 2nd gig
            if (dwSlot <= MAX_PROCESSES) {
                DEBUGCHK (dwSlot > 1);
                // process address
                pPtbls = ProcArray[dwSlot-1].pPTBL;
                gdwSlotTouched |= aky;
            } else {
                // in the 2nd gig
                pPtbls = MAPPER_SLOT_TO_PTBL(dwSlot);

                // check object store
                if (IsSlotInObjStore (dwSlot)) {
                    if (!(CurAKey & pFileSysProc->aky))
                        return FALSE;           // don't have access

                    gfObjStoreTouched = TRUE;
                }
            }
            break;
        }

        if (pPtbls[ix4M & 7] && !g_pPageDir->PTE[ix4M]) {
            
            // we have an entry on the per-slot page table, but not on HW PageTable
            g_pPageDir->PTE[ix4M] = pPtbls[ix4M & 7] & ~bitClear;
            
            return TRUE;
        }
    }
    DEBUGMSG (ZONE_VIRTMEM, (L"LoadPageTable failed, addr = %8.8lx, dwSlot = %8.8lx, aky = %8.8lx, CurAKey = %8.8lx\r\n", addr, dwSlot, aky, CurAKey));
    return FALSE;
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PVOID 
Phys2Virt(
    DWORD pfn
    ) 
{
    PPTE ppte;

    for (ppte = g_pOEMAddressTable; ppte->dwSize; ppte ++) {
        if ((pfn >= ppte->dwPA) && (pfn < ppte->dwPA + ppte->dwSize))
            return (LPVOID) (pfn - ppte->dwPA + ppte->dwVA);
    }

    DEBUGMSG(ZONE_PHYSMEM, (TEXT("Phys2Virt() : PFN (0x%08X) not found!\r\n"), pfn));
    return NULL;
}


#pragma warning(default:4035)               // Turn warning back on


_inline void ClearOneSlot (DWORD dwVMBase)
{
    memset (&g_pPageDir->PTE[dwVMBase >> 22], 0, 32);
}


void ClearSlots (BOOL fNeedFlush)
{
    DWORD dwSlotRst;
    if (dwSlotRst = gdwSlotTouched & ~CurAKey) {
        int i;
        for (i = 1; dwSlotRst; i ++) {
            if (dwSlotRst & (1 << i)) {
                dwSlotRst &= ~(1 << i);
                ClearOneSlot (ProcArray[i].dwVMBase);
            }
        }
        fNeedFlush = TRUE;
        gdwSlotTouched &= CurAKey;
    }
    if (gfObjStoreTouched && !(pFileSysProc->aky & CurAKey)) {
        DWORD dwBits = g_ObjStoreSlotBits;
        int i;
        for (i = 0; dwBits; i ++) {
            if (dwBits & (1 << i)) {
                dwBits &= ~(1 << i);
                ClearOneSlot ((FIRST_MAPPER_SLOT + i) << VA_SECTION);
            }
        }
        fNeedFlush = TRUE;
        gfObjStoreTouched = FALSE;
    }
    if (fNeedFlush) {
        OEMCacheRangeFlush (0, 0, CACHE_SYNC_FLUSH_TLB);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SetCPUASID(
    PTHREAD pth
    )
{
    if (!InSysCall ()) {
        KCall ((FARPROC) SetCPUASID, pth);
    } else {
        BOOL fNeedFlush = (pCurProc != pth->pProc);
        if (fNeedFlush) {
            DWORD dwPDE = SHARED_BASE_ADDRESS >> 22;
            int   i;
            if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
                CeLogThreadMigrate(pth->pProc->hProc, 0);
            }
            memcpy (&g_pPageDir->PTE[0], &pth->pProc->pPTBL[0], 32);

            // make shared section read-only
            for (i = 0; i < PDES_PER_SLOT; i ++) {
                g_pPageDir->PTE[dwPDE+i] &= ~PG_WRITE_MASK;
            }
        }
        ClearSlots (fNeedFlush);
        pCurProc = pth->pProc;
        hCurProc = pCurProc->hProc;
        SectionTable[0] = pCurProc->procnum? SectionTable[pCurProc->procnum+1] : &NKSection;
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID MDValidateKVA (DWORD dwAddr)
{
    DWORD       dwPageDir;
    PPAGETABLE  pPageTable;

    //
    // Find entry in 1st level page dir
    //
    if ((dwPageDir = g_pPageDir->PTE[VA_TO_PD_IDX (dwAddr)]) != 0) {
        if ((dwPageDir & (PG_LARGE_PAGE_MASK|PG_VALID_MASK)) == (PG_LARGE_PAGE_MASK|PG_VALID_MASK)) {
            return (LPVOID) dwAddr;
        } else {
            pPageTable = (PPAGETABLE)PHYS_TO_LIN(dwPageDir & PG_PHYS_ADDR_MASK);
            if (pPageTable->PTE[(dwAddr>>VA_PAGE)&0x3FF] & PG_VALID_MASK)
            return (LPVOID) dwAddr;
        }
    }
    return 0;
}


//------------------------------------------------------------------------------
//
// Function:
//  CommonFault
//
// Description:
//  CommonFault is jumped to by the specific fault handlers for unhandled
//  exceptions which are then dispatched to the C routine HandleException.
//
// At entry:
//  ESP     points to stack frame containing PUSHAD, ERROR CODE, EIP, CS,
//          EFLAGS, (and optionally Old ESP, Old SS).  Normally this is the
//          last part of the thread structure, the saved context.  In the case
//          of a nested exception the context has been saved on the ring 0
//          stack.  We will create a fake thread structure on the stack to hold
//          the captured context.  The remaining segment registers are added by
//          this routine.
//
//  ECX     is the faulting address which is passed to HandleException
//
//  ESI     is the exception id which is passed to HandleException
//
//  Return:
//   CommonFault jumps to Reschedule or resumes execution based on the return
//   value of HandleException.
//
//------------------------------------------------------------------------------
Naked 
CommonFault()
{
    _asm {
        cld
        mov     eax, KGDT_R3_DATA
        mov     ds, ax
        mov     es, ax
        dec     [KData].cNest
        jnz     short cf20          // nested fault
        mov     esp, offset KData-4
        mov     edi, PtrCurThd
cf10:   sti
        push    ecx
        push    esi
        push    edi
        call    HandleException
        add     esp, 3*4
        test    eax, eax
        jnz     short NoReschedule
        jmp     Reschedule
NoReschedule:
        jmp     RunThread

// Nested exception. Create a fake thread structure on the stack
cf20:   push    ds
        push    es
        push    fs
        push    gs
        sub     esp, THREAD_CONTEXT_OFFSET
        mov     edi, esp            // (edi) = ptr to fake thread struct
        jmp     short cf10
    }
}




//------------------------------------------------------------------------------
//
// Do a reschedule.
//
//  (edi) = ptr to current thread or 0 to force a context reload
//
//------------------------------------------------------------------------------
Naked 
Reschedule()
{
    __asm {
rsd10:
        sti
        cmp     word ptr ([KData].bResched), 1
        jne     short rsd11
        mov     word ptr ([KData].bResched), 0
        call    NextThread
rsd11:
        cmp     dword ptr ([KData].dwKCRes), 1
        jne     short rsd12
        mov     dword ptr ([KData].dwKCRes), 0
        call    KCNextThread

        cmp     dword ptr ([KData].dwKCRes), 1
        je      short rsd10

rsd12:
        mov     eax, [RunList.pth]
        test    eax, eax
        jz      short rsd50           // nothing to run
        cmp     eax, edi
        jne     short rsd20
        jmp     RunThread           // redispatch the same thread

// Switch to a new thread's process context.
// Switching to a new thread. Update current process and address space
// information. Edit the ring0 stack pointer in the TSS to point to the
// new thread's register save area.
//
//      (eax) = ptr to thread structure

rsd20:  mov     edi, eax                // Save thread pointer
        mov     esi, (THREAD)[eax].hTh          // (esi) = thread handle
        push    edi
        call    SetCPUASID              // Sets hCurProc for us!
        pop     ecx                     // Clean up stack

        mov     hCurThd, esi            // set the current thread handle
        mov     PtrCurThd, edi          //   and the current thread pointer
        mov     ecx, [edi].tlsPtr       // (ecx) = thread local storage ptr
        mov     [KData].lpvTls, ecx     // set TLS pointer

        cmp     edi, g_CurFPUOwner
        jne     SetTSBit
        clts
        jmp     MuckWithFSBase

SetTSBit:
        mov     eax, CR0
        test    eax, TS_MASK
        jnz     MuckWithFSBase
        or      eax, TS_MASK
        mov     CR0, eax

MuckWithFSBase:
        mov     edx, offset g_aGlobalDescriptorTable+KGDT_PCR
        sub     ecx, FS_LIMIT+1         // (ecx) = ptr to NK_PCR base
        mov     word ptr [edx+2], cx    // set low word of FS base
        shr     ecx, 16
        mov     byte ptr [edx+4], cl    // set third byte of FS base
        mov     byte ptr [edx+7], ch    // set high byte of FS base

        push    fs
        pop     fs

        lea     ecx, [edi].ctx.TcxSs+4  // (ecx) = ptr to end of context save area
        mov     [MainTSS].Esp0, ecx
        jmp     RunThread               // Run thread pointed to by edi

// No threads ready to run. Call OEMIdle to shutdown the cpu.

rsd50:  cli

        cmp     word ptr ([KData].bResched), 1
        je      short DoReschedule
        call    OEMIdle
        mov     byte ptr ([KData].bResched), 1
        jmp     Reschedule
DoReschedule:
        sti
        jmp     Reschedule
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
Naked 
CommonIntDispatch()
{
    _asm {
        cld
        mov     eax, KGDT_R3_DATA
        mov     ds, ax
        mov     es, ax
        dec     [KData].cNest
        jnz     short cid20         // nested fault
        mov     esp, offset KData-4
        mov     edi, PtrCurThd
cid10:
#ifdef NKPROF
        push    esi                 // save ESI
        mov     eax, 80000000h      // mark as ISR entry
        push    eax                 // Arg 0, cNest + SYSINTR_xxx
        call    CeLogInterrupt
        pop     eax                 // cleanup the stack from the call
        pop     esi                 // restore ESI
#endif // NKPROF

        sti

        call    esi

        mov     ecx, dword ptr [pfnOEMIntrOccurs]   // (ecx) = pfnOEMIntrOccurs
        push    eax                 // push argument == SYSINTR returned
        call    ecx                 // call pfnOEMintrOccurs
        pop     ecx                 // dummy pop

        cli

#ifdef NKPROF
        push    eax                 // Save original SYSINTR return value.
        bswap   eax                 // Reverse endian
        mov     ah, [KData].cNest   // Nesting level (0 = no nesting, -1 = nested once)
        neg     ah                  // Nesting level (0 = no nesting,  1 = nested once)
        bswap   eax                 // Reverse endian
        push    eax                 // Arg 0, cNest + SYSINTR_xxx
        call    CeLogInterrupt
        pop     eax                 // cleanup the stack from the call
        pop     eax                 // restore original SYSINTR value
#endif // NKPROF
        test    eax, eax
        jz      short RunThread     // SYSINTR_NOP: nothing more to do

#ifdef NKPROF
        cmp     eax, SYSINTR_PROFILE
        jne     short cid13
        call    ProfilerHit
        jmp     RunThread           // Continue on our merry way...
cid13:
#endif // NKPROF

        cmp     eax, SYSINTR_RESCHED
        je      short cid15
        lea     ecx, [eax-SYSINTR_DEVICES]
        cmp     ecx, SYSINTR_MAX_DEVICES
        jae     short cid15         // force a reschedule for good measure

// A device interrupt has been signaled. Set the appropriate bit in the pending
// events mask and set the reschedule flag. The device event will be signaled
// by the scheduler.

        mov     eax, 1              // (eax) = 1
        cmp     ecx, 32             // ISR# >= 32?
        jae     cid18               // take care of ISR# >= 32 if true

        // ISR# < 32
        shl     eax, cl             // (eax) = 1 << ISR#
        or      [KData].aPend1, eax // update PendEvent1
cid15:  or      [KData].bResched, 1 // must reschedule
        jmp     RunThread

        // ISR# >= 32
cid18:  sub     cl, 32              // ISR# -= 32
        shl     eax, cl             // (eax) = 1 << (ISR#-32)
        or      [KData].aPend2, eax // update PendEvent2
        or      [KData].bResched, 1 // must reschedule
        jmp     RunThread

// Nested exception. Create a fake thread structure on the stack
cid20:  push    ds
        push    es
        push    fs
        push    gs
        sub     esp, THREAD_CONTEXT_OFFSET
        mov     edi, esp            // (edi) = ptr to fake thread struct
#ifdef NKPROF
        mov     dword ptr (pthFakeStruct), edi
#endif        
        jmp     short cid10
    }
}




//------------------------------------------------------------------------------
//
// Continue thread execution.
//
//  (edi) = ptr to Thread structure
//
//------------------------------------------------------------------------------
Naked 
RunThread()
{
    _asm {
        cli
        cmp     word ptr ([KData].bResched), 1
        jne short NotReschedule
        jmp     Reschedule
NotReschedule:
        inc     [KData].cNest
        lea     esp, [edi].ctx.TcxGs
        pop     gs
        pop     fs
        pop     es
        pop     ds
        popad
        add     esp, 4
        iretd
        cli
        hlt
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
Naked 
PageFault()
{
    _asm {
        pushad
        mov     ebx, OFFSET KData
        mov     edi, cr2
        test    edi, edi
        jns     short pf05 

        // Address > 2GB, kmode only
        mov     esi, [esp+32]
        and     esi, 1
        jnz     short pf50              // prevelige vialoation, get out now
        mov     ecx, edi
        shr     ecx, VA_SECTION
        cmp     ecx, SECURE_SECTION
        jne     short pf50              // get out if not secure section
        
pf05:
        dec     [ebx].cNest             // count kernel reentrancy level
        mov     esi, esp                // (esi) = original stack pointer
        jnz     short pf10
        lea     esp, [ebx-4]            // switch to kernel stack (&KData-4)

//  Process a page fault for the "user" address space (0 to 0x7FFFFFFF)
//
//  (edi) = Faulting address
//  (ebx) = ptr to KData
//  (esi) = Original ESP

pf10:   cld                         // Need to do this since the page fault could have come from anywhere!
        cmp     dword ptr ([KData].dwInDebugger), 0 // see if debugger active
        jne     short pf20                          // if so, skip turning on of interrupts
        sti                                         // enable interrupts

pf20:   push    [esi+32]
        push    edi
        call    LoadPageTable
        cli
        test    eax, eax
        jz      short pf40          // page not found in the Virtual memory tree
        cmp     word ptr ([KData].bResched), 1
        je      short pf60          // must reschedule now
        inc     [ebx].cNest         // back out of kernel one level
        mov     esp, esi            // restore stack pointer
        popad
        add     esp, 4
        iretd

        
//  This one was not a good one!  Jump to common fault handler
//
//  (edi) = faulting address

pf40:   inc     [ebx].cNest         // back out of kernel one level
        mov     esp, esi            // restore stack pointer
pf50:   mov     ecx, edi            // (ecx) = fault effective address
        mov     esi, 0Eh
        jmp     CommonFault

// The reschedule flag was set and we are at the first nest level into the kernel
// so we must reschedule now.

pf60:   mov     edi, PtrCurThd      // (edi) = ptr to current THREAD
        jmp     Reschedule
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
Naked 
GeneralFault()
{
    _asm {
        pushad
        xor     ecx, ecx            // (ecx) = 0 (fault effective address)
        mov     esi, 13
        jmp     CommonFault
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
Naked 
InvalidOpcode(void)
{
    __asm {
        push    eax
        pushad
        mov     esi, 6
        xor     ecx, ecx            // (ecx) = 0 (fault effective address)
        jmp     CommonFault
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
Naked 
ZeroDivide(void)
{
    __asm {
        push    eax
        pushad
        xor     esi, esi            // (esi) = 0 (divide by zero fault)
        xor     ecx, ecx            // (ecx) = 0 (fault effective address)
        jmp     CommonFault
    }
}

const BYTE PosTable[256] = {
    0,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,
    6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,
    7,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,
    6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,
    8,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,
    6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,
    7,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,
    6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
};




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void __declspec(naked) 
GetHighPos(
    DWORD foo
    ) 
{
    _asm {
        mov ecx, dword ptr [esp + 4]
        push ebx
        lea ebx, PosTable

        mov dl, 0xff
        xor eax, eax
        mov al, cl
        xlatb
        test al, al
        jne res

        shr ecx, 8
        add dl, 8
        mov al, cl
        xlatb
        test al, al
        jne res

        shr ecx, 8
        add dl, 8
        mov al, cl
        xlatb
        test al, al
        jne res

        shr ecx, 8
        add dl, 8
        mov al, cl
        xlatb
        test al, al
        jne res

        mov al, 9
res:
        add al, dl
        pop ebx
        ret
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
Naked 
Int1Fault(void)
{
    __asm {
        push    eax                             // Save orig EAX as fake error code
        cmp     word ptr [esp + 6], 0FFFFh      // Is it an API call fault?
        je      skip_debug                      // Yes - handle page fault first
        mov     eax, [esp + 4]                  // (eax) = faulting EIP
        and     dword ptr [esp + 12], not 0100h  // Clear TF if set
        test    byte ptr [esp + 8], 3           // Are we trying to SS ring 0?
        jz      skip_debug                      // Yes - get out quick
        mov     eax, dword ptr [esp]            // Restore original EAX

        pushad
        mov     esi, 1
        xor     ecx, ecx            // (ecx) = 0 (fault effective address)
        jmp     CommonFault

skip_debug:
        pop     eax
        iretd
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
Naked 
Int2Fault(void)
{
    __asm {
        push    eax                 // Fake error code
        pushad
        mov     esi, 2
        xor     ecx, ecx            // (ecx) = 0 (fault effective address)
        jmp     CommonFault
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
Naked 
Int3Fault(void)
{
    __asm {
        dec     dword ptr [esp]     // Back up EIP
        push    eax                 // Fake error code
        pushad
        mov     esi, 3
        xor     ecx, ecx            // (ecx) = 0 (fault effective address)
        jmp     CommonFault
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
InvalidateRange(
    PVOID pvAddr,
    ULONG ulSize
    )
{
    // just flush TLB
    KCall ((FARPROC) OEMCacheRangeFlush, 0, 0, CACHE_SYNC_FLUSH_TLB);
}


static DWORD addrPagesForPT;
#define IsSystemStarted()   (VAcs.hCrit)

static BOOL AllocPagesForPT (DWORD cbPages)
{
    if (IsSystemStarted ()) {
        return HoldPages (cbPages, FALSE);
    }

    // called from OEMInit - special handling
    addrPagesForPT = pTOC->ulRAMFree + MemForPT;
    MemForPT += cbPages * PAGE_SIZE;

    return TRUE;
}

static DWORD GetNextPageForPT (void)
{
    DWORD dwPhys;
    if (IsSystemStarted ()) {
        return GetHeldPage ();
    }

    // called from OEMInit - special handling
    dwPhys = LIN_TO_PHYS (addrPagesForPT);
    addrPagesForPT += PAGE_SIZE;
    
    return dwPhys;
}

#define PG_KERNEL_RW        (PG_WRITE_MASK | PG_VALID_MASK | PG_ACCESSED_MASK | PG_DIRTY_MASK)

#define FIRST_STATIC_PDE    (0xC40 >> 2)    // VA 0xC4000000, in 4M chunks
#define STATIC_PDE_LIMIT    (0xE00 >> 2)    // VA 0xE0000000, in 4M chunks

#define LARGE_PAGE_SUPPORT_DETECTED     (KData.dwCpuCap & CPUID_PSE)

const int dw4M = 4 * 1024 * 1024;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD
FindFreePDE(
    DWORD dwNumPDE              // Number of consecutive 4MB sections needed
    ) 
{
    DWORD pdeStart, pdeWalk;
    DWORD dwNumFound;

    for (pdeStart = FIRST_STATIC_PDE; (pdeStart + dwNumPDE - 1) < STATIC_PDE_LIMIT; pdeStart++) {

        dwNumFound = 0;

        if (!g_pPageDir->PTE[pdeStart]) {
            //
            // This is a potential starting point. See if enough consecutive
            // blocks are available.
            //
            dwNumFound = 1;
            
            for (pdeWalk = pdeStart + 1; pdeWalk < pdeStart + dwNumPDE; pdeWalk++) {
                if (g_pPageDir->PTE[pdeWalk]) {
                    break;
                } else {
                    dwNumFound++;
                }
            }

            if (dwNumFound == dwNumPDE) {
                return pdeStart;
            }
        }
    }

    // none found
    return 0;
}


DWORD FindExistingMapping (DWORD dw4MPhysBase, DWORD dwNumPDE)
{
    DWORD pdeStart, pdeWalk, dwPDE;
    DWORD dwNumFound;
    PPAGETABLE pPageTable;

    DEBUGCHK (!(dw4MPhysBase & (dw4M - 1)));    // must be 4M aligned

    // loop through the page directory until we find an unused entry or existing mapping
    for (pdeStart = FIRST_STATIC_PDE; g_pPageDir->PTE[pdeStart] && ((pdeStart + dwNumPDE - 1) < STATIC_PDE_LIMIT); pdeStart++) {

        // find the consective PDE entries that satisfy the request
        for (dwNumFound = 0, pdeWalk = pdeStart; dwNumFound < dwNumPDE; dwNumFound ++, pdeWalk ++) {
            
            if (LARGE_PAGE_SUPPORT_DETECTED) {
                // large page, physical base is in the entry itself
                dwPDE = g_pPageDir->PTE[pdeWalk] & PG_PHYS_ADDR_MASK;
            } else {
                // 4K page tables, first entry in the pagetable is the 4M aligned address
                pPageTable = (PPAGETABLE) Phys2Virt (g_pPageDir->PTE[pdeWalk] & PG_PHYS_ADDR_MASK);
                DEBUGCHK (pPageTable);
                dwPDE = pPageTable->PTE[0] & PG_PHYS_ADDR_MASK;
            }
            
            if (dwPDE != dw4MPhysBase + dwNumFound * dw4M) {
                break;
            }
        }
        if (dwNumFound == dwNumPDE) {
            return pdeStart;
        }
    }

    // not found
    return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID
NKCreateStaticMapping(
    DWORD dwPhysBase,
    DWORD dwSize
    ) 
{
    LPVOID pvRet = NULL;
    DWORD dw4MBase;                         // 4M aligned address

    dwPhysBase <<= 8;                       // Only supports 32-bit physical address.
    dw4MBase = dwPhysBase & -dw4M;          // 4M alignment
    
    if (!dwSize || (dwPhysBase & (PAGE_SIZE-1))) {
        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
        return NULL;
    }

    if (IsSystemStarted ())
        EnterCriticalSection(&VAcs);
    
    if (dwPhysBase < 0x20000000 && ((dwPhysBase + dwSize) < 0x20000000)) {
        
        pvRet = Phys2VirtUC(dwPhysBase);
        
    } else {

        DWORD dwPDENeeded;
        DWORD pdeStart, pdeWalk;

        dwPDENeeded = ((dwPhysBase + dwSize - dw4MBase - 1) / dw4M) + 1;

        DEBUGCHK (dwPDENeeded);

        if (pdeStart = FindExistingMapping (dw4MBase, dwPDENeeded)) {

            dwPDENeeded = 0;    // indicate success
        
        } else if (pdeStart = FindFreePDE (dwPDENeeded)) {
            
            if (LARGE_PAGE_SUPPORT_DETECTED) {
                //
                // Supports 4MB page mappings.
                //
                for (pdeWalk = pdeStart; dwPDENeeded; pdeWalk ++, dw4MBase += dw4M, dwPDENeeded --) {
                    g_pPageDir->PTE[pdeWalk] = dw4MBase | PG_KERNEL_RW | PG_LARGE_PAGE_MASK | PG_NOCACHE | PG_WRITE_THRU_MASK;
                }

            } else if ((((DWORD)PageFreeCount > dwPDENeeded + MIN_PROCESS_PAGES) || !IsSystemStarted())
                    && AllocPagesForPT (dwPDENeeded)) {
                //
                // Use 4k page tables to map.
                //
                // We've at least got enough pages available and that many have been set
                // aside for us (though not yet assigned). 
                //
                PAGETABLE* pPageTable;
                DWORD pfnAddr;
                int i;
                
                for (pdeWalk = pdeStart; dwPDENeeded; pdeWalk ++, dwPDENeeded --) {
                    
                    pfnAddr = GetNextPageForPT ();

                    pPageTable = (PAGETABLE*) Phys2Virt(pfnAddr);
                    DEBUGCHK(pPageTable);   

                    g_pPageDir->PTE[pdeWalk] = pfnAddr | PG_KERNEL_RW;

                    for (i = 0; i < 1024; i++, dw4MBase += PFN_INCR) {
                        pPageTable->PTE[i] = dw4MBase | PG_KERNEL_RW | PG_NOCACHE | PG_WRITE_THRU_MASK;
                    }

                    DEBUGCHK (!(dw4MBase & (dw4M -1)));
                }
            }

        }
        
        // dwPDENeeded will be zero if success
        if (dwPDENeeded == 0) {
            pvRet = (LPVOID) ((pdeStart << 22) + (dwPhysBase & (dw4M - 1)));  // create a virtual address from PD index
        }
    }
    if (IsSystemStarted ())
        LeaveCriticalSection(&VAcs);
    
    return (pvRet);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID
SC_CreateStaticMapping(
    DWORD dwPhysBase,
    DWORD dwSize
    ) 
{
    TRUSTED_API (L"SC_CreateStaticMapping", NULL);

    return NKCreateStaticMapping(dwPhysBase, dwSize);
}






#pragma warning(disable:4035 4733)
//------------------------------------------------------------------------------
//
// System call trap handler.
//
// Pop the iret frame from the stack, switch back to the caller's stack, enable interrupts,
// and dispatch the system call.
//
//      CPU State:  ring1 stack & CS, interrupts disabled.
//                  edx:eax == rtn value if PSL return
//
//------------------------------------------------------------------------------
PVOID __declspec(naked) Int20SyscallHandler (void)
{
    __asm {
        // The following three instructions are only executed once on Init time.
        // It sets up the KData PSL return function pointer and returns the
        // the 'real' address of the Int20 handler (sc00 in this case).
        mov [KData.pAPIReturn], offset APICallReturn
        mov dword ptr [MD_CBRtn], offset CBRtnCommon
        mov eax, offset sc00
        ret

sc00:   pop     ecx                     // (ecx) = EIP of "int SYSCALL"
        sub     ecx, FIRST_METHOD+2     // (ecx) = iMethod * APICALL_SCALE
        cmp     ecx, -APICALL_SCALE     // check callback return
        je      short CallbackRtn       // returning from callback

        sar     ecx, 1                  // (ecx) == iMethod
        pop     eax                     // (eax) == caller's CS
        and     al, 0FCh
        cmp     al, KGDT_R1_CODE
        je      short KPSLCall          // caller was in kernel mode

        // caller was in user mode

        // special casing RaiseException
        cmp     ecx, RAISEEXCEPTION
        je      short UPSLCallTrusted   // do not switch stack for RaiseException
        
        // PerformCallBack MUST BE CALLED IN KMODE (From PSL)
        // cmp     ecx, PERFORMCALLBACK
        // je      short DoPerformCallback

        // check if the caller is trusted
        mov     eax, PtrCurProc         // (eax) == pCurProc
        cmp     byte ptr [eax+OFFSET_TRUSTLVL], KERN_TRUST_FULL     // is it fully trusted?
        je      short UPSLCallTrusted   // do fully trusted if yes

        // not trusted, switch to secure stack (need to update TLSPTR too)
        mov     eax, PtrCurThd          // (eax) == pCurThread
        mov     edx, [eax].tlsSecure    // (eax) == TLSPTR of the secure stack
        mov     [eax].tlsPtr, edx       // update thread's tlsPtr
        mov     [KData].lpvTls, edx     // set KData's TLS pointer

        // find the 'real' callstack if there is one(SEH might create a faked one)
        mov     edx, [eax].pcstkTop     // (edx) == pCurThread->pcstkTop
UPSLNextSTK:        
        test    edx, edx
        je      short UPSLCallFirstTrip // 0 if first trip
        cmp     dword ptr [edx].akyLast, 0  // akyLast == 0 if faked
        jne     short UPSLFoundStkTop   // found the real ccallstack
        mov     edx, [edx].pcstkNext
        jmp     short UPSLNextSTK
        
        
UPSLFoundStkTop:
        // (not trusted) in callback function, calling into PSL again
        // (edx) = 'real' pcstk
        mov     edx, [edx].dwPrevSP     // (edx) == pcstk->dwPrevSP
        jmp     short UPSLCallNonTrusted

UPSLCallFirstTrip:
        // (non-trusted) first trip into PSL, use tlsPtr to figure out where SP should be
        // (eax) == PthCurThd
        mov     edx, [eax].tlsSecure    // (eax) == TLSPTR of the secure stack
        sub     edx, SECURESTK_RESERVE  // (edx) == new stack
        
UPSLCallNonTrusted:
        // (edx) == new SP
        mov     esp, [esp+4]            // (esp) == caller's SP
        xchg    esp, edx                // switch stack
        
        // make roon for arguments on the new stack
        sub     esp, MAX_PSL_ARGS       // save space for MAX # of PSL arguments + return address
        mov     eax, USER_MODE          // calling from user mode
        jmp     short PSLCallCommon
        
UPSLCallTrusted:
        // caller is fully trusted, use current stack
        mov     esp, [esp+4]            // get the user stack
        mov     edx, 0                  // prevSP set to 0 when no stack swtich
        mov     eax, USER_MODE          // calling from user mode
        jmp     short PSLCallCommon
        
KPSLCall:
        // caller was in kernel mode

        // is this a callback?
        cmp     ecx, PERFORMCALLBACK
        je      short DoPerformCallback

        // caller was in kernel mode
        add     esp, 4                  // discard the EFLAGS
        mov     edx, 0                  // prevSP set to 0 when no stack swtich
        mov     eax, KERNEL_MODE        // we're in kernel mode

PSLCallCommon:
        // (eax) == caller's mode
        // (ecx) == iMethod
        // (edx) == caller's stack
        // (esp) == secure stack if not trusted, caller stack if trusted/kmode
        
        push    fs:dword ptr [0]        // save exception chain linkage
        sti                             // interrupt ok now

        cmp     edx, 0                  // are we using the same stack?
        je      short DoObjectCall      

        // stack changed, need to copy argument from one to the other
        push    ecx
        push    esi
        push    edi

        mov     esi, edx
        lea     edi, [esp+16]           // +16 == fs:0 and ecx, esi, edi we just pushed
        mov     ecx, MAX_PSL_ARGS/4
        rep     movsd

        // update fs
        mov     ecx, PtrCurThd
        mov     ecx, [ecx].tlsPtr
        mov     edi, offset g_aGlobalDescriptorTable+KGDT_PCR
        sub     ecx, FS_LIMIT+1         // (ecx) = ptr to NK_PCR base
        mov     word ptr [edi+2], cx    // set low word of FS base
        shr     ecx, 16
        mov     byte ptr [edi+4], cl    // set third byte of FS base
        mov     byte ptr [edi+7], ch    // set high byte of FS base
        push    fs
        pop     fs                      // cause fs to reload

        pop     edi
        pop     esi
        pop     ecx
        pop     fs:dword ptr [0]        // make sure the new fs:[0] is consistent with the old one
        push    fs:dword ptr [0]        // save exception chain linkage

DoObjectCall:
        // PSL Call, setup OBJCALLSTRUCT on stack (linkage already pushed)
        push    eax                     // mode
        push    edx                     // previous ESP
        push    ecx                     // iMethod

        push    esp                     // arg0 == pointer to OBJCALLSTRUCT
        
        call    ObjectCall              // (eax) = api function address (0 if completed)
        add     esp, 20                 // clear ObjectCall args off the stack
        mov     fs:dword ptr [0], -2    // mark PSL boundary in exception chain
        mov     ecx, PtrCurThd          // (ecx) = ptr to THREAD struct
        mov     ecx, [ecx].pcstkTop     // (ecx) = ptr to CALLSTACK struct
        mov     [ecx].ExEsp, esp        // .\     v
        mov     [ecx].ExEbp, ebp        // ..\    v
        mov     [ecx].ExEbx, ebx        // ...> save registers for possible exception recovery
        mov     [ecx].ExEsi, esi        // ../
        mov     [ecx].ExEdi, edi        // ./

        pop     edx                     // get rid of return address
        call    eax                     // & call the api function in KMODE (all PSL calls in KMODE now)
APICallReturn:

        push    0                       // space for exception chain linkage
        push    KERNEL_MODE             // current thread mode

// Retrieve return address, mode, and exception linkage from the
// thread's call stack.
//
//  (eax:edx) = function return value
//  (TOS) = thread's execution mode
//  (TOS+4) = space to receive previous exception chain linkage
        sub     esp, 4              // room for dwPrevSP (must be at esp+8 when returned from ServerCallReturn)
        push    eax                 // save return value
        push    edx                 // ...
        
        lea     eax, [esp+8]        // (eax) = ptr to SVRRTNSTRUCT
        push    eax                 // arg0 - ptr to SVRRTNSTRUCT
        call    ServerCallReturn    // (eax) = api return address
        add     esp, 4              // get rid of arg 0
        mov     edx, [esp]          // restore edx
        mov     [esp], eax          // save return address
        mov     eax, [esp+4]        // restore eax
        mov     ecx, [esp+16]       // (ecx) = saved exception linkage
        cmp     dword ptr [esp+12], KERNEL_MODE
        mov     fs:[0], ecx         // restore exception linkage
        jne     short UPSLRtn       // dispatch thread in kernel mode

        // returning to KMode caller, just pop working data and return
        ret     16

UPSLRtn:        
        // returning to user mode process

        // check if stack switch is needed
        cmp     dword ptr [esp+8], 0 // need stack switch?
        je      short UPSLRtnNoStkSwitch

        // not trusted, dwPrevSP has the right value
        add     dword ptr [esp+8], 4  // [esp+8] is already dwPrevSP, add 4 to remove the return address

        // update TLS
        push    edx
        mov     edx, PtrCurThd          // (edx) == pCurThread
        mov     ecx, [edx].tlsNonSecure // (ecx) == TLSPTR of the non-secure stack
        mov     [edx].tlsPtr, ecx       // update thread's tlsPtr
        mov     [KData].lpvTls, ecx     // set KData's TLS pointer

        // need to update fs
        mov     edx, offset g_aGlobalDescriptorTable+KGDT_PCR
        sub     ecx, FS_LIMIT+1         // (ecx) = ptr to NK_PCR base
        mov     word ptr [edx+2], cx    // set low word of FS base
        shr     ecx, 16
        mov     byte ptr [edx+4], cl    // set third byte of FS base
        mov     byte ptr [edx+7], ch    // set high byte of FS base
        push    fs
        pop     fs                      // cause fs to reload

        pop     edx

        // restore fs:[0] on new stack
        mov     ecx, [esp+16]
        mov     fs:[0], ecx
        
        jmp     short UPSLRtnToCaller

UPSLRtnNoStkSwitch:
        // pCurProc is fully trusted, no stack switch required
        lea     ecx, [esp+20]
        mov     [esp+8], ecx        // ESP restore value

UPSLRtnToCaller:
        
        // [esp] == return address, [esp+8] == stack, both must have already been setup correctly
        mov     dword ptr [esp+4], KGDT_R3_CODE | 3
        mov     dword ptr [esp+12], KGDT_R3_DATA | 3
        retf                        // return to ring3 & restore stack pointer

////////////////////////////////////////////////////////////////////////////////////////
// callback returns from user mode process
//
CallbackRtn:
        pop     ecx                     // (eax) == caller's CS
        and     cl, 0FCh
        cmp     cl, KGDT_R1_CODE
        je      short CBRtnKMode        // Are we in kmode? (User called SetKMode in the callback function)

        // we're in user mode
        mov     esp, [esp+4]            // edx == stack of returning thread
        jmp     short CBRtnChkTrust
        
CBRtnKMode:
        // in KMODE
        add     esp, 4                   // skip the EFLAGS

CBRtnChkTrust:
        sti                             // interrupt okay now

        // do we need to switch stack?
        mov     edx, PtrCurThd              // (edx) == pCurThread
        mov     ecx, [edx].pcstkTop         // (ecx) == pCurThread->pcstkTop
        cmp     dword ptr [ecx].dwPrevSP, 0 // need stack change?
        je      short CBRtnCommon

        // the stack needs to be changed

        // get the new SP
        mov     esp, [ecx].dwPrevSP
        add     esp, 4                  // less retrun address

        // update TLS   (edx == pCurThread on entrance)
        mov     ecx, [edx].tlsSecure    // (ecx) == TLSPTR of the secure stack
        mov     [edx].tlsPtr, ecx       // update thread's tlsPtr
        mov     [KData].lpvTls, ecx     // set KData's TLS pointer

        // update fs
        mov     edx, offset g_aGlobalDescriptorTable+KGDT_PCR
        sub     ecx, FS_LIMIT+1         // (ecx) = ptr to NK_PCR base
        mov     word ptr [edx+2], cx    // set low word of FS base
        shr     ecx, 16
        mov     byte ptr [edx+4], cl    // set third byte of FS base
        mov     byte ptr [edx+7], ch    // set high byte of FS base
        push    fs
        pop     fs                      // cause fs to reload

        jmp     short CBRtnCommon

        
/////////////////////////////////////////////////////////////////////////////////////////
// callbacks
//      CALLER MUST BE IN KMODE
DoPerformCallback:

        sti                             // interrupt okay now
        // setup OBJCALLSTRUCT
        mov     edx, fs:[0]             // replace EFLAGS with linkage
        mov     [esp], edx              // replace EFLAGS with linkage
        sub     esp, 12                 // room for mode, prevSP, and method
        push    esp                     // arg1 == ptr to OBJCALLSTRUCT
        lea     ecx, [esp+20]           // (ecx) = original SP
        mov     [esp+8], ecx            // pobs->prevSP = original SP
        
        call    PerformCallBackExt      // argument to PerformCallBackExt is (excpLink, ra, pcbi, pmode)

        // eax == function to call

        mov     ecx, [esp+12]           // (ecx) = mode
        add     esp, 20                 // remove args we pushed
        cmp     dword ptr [esp-12], 0   // use same stack?
        je      short CBCommon

        // need to change stack
        mov     edx, PtrCurThd          // (edx) == pCurThread
        mov     ecx, [edx].tlsNonSecure // (ecx) == TLSPTR of the non-secure stack
        mov     [edx].tlsPtr, ecx       // update thread's tlsPtr
        mov     [KData].lpvTls, ecx     // set KData's TLS pointer

        // update fs
        //  (ecx) == pCurThread->tlsPtr
        mov     edx, offset g_aGlobalDescriptorTable+KGDT_PCR
        sub     ecx, FS_LIMIT+1         // (ecx) = ptr to NK_PCR base
        mov     word ptr [edx+2], cx    // set low word of FS base
        shr     ecx, 16
        mov     byte ptr [edx+4], cl    // set third byte of FS base
        mov     byte ptr [edx+7], ch    // set high byte of FS base
        // make sure fs got updated
        push    fs
        pop     fs                      // cause fs to reload
        

        // save current ESP in callstack structure
        mov     ecx, [esp-12]           // (ecx) == new SP
        xchg    ecx, esp                // switch stack


        


        push    dword ptr [ecx+32]
        push    dword ptr [ecx+28]
        push    dword ptr [ecx+24]
        push    dword ptr [ecx+20]
        push    dword ptr [ecx+16]
        push    dword ptr [ecx+12]
        push    dword ptr [ecx+8]
        push    dword ptr [ecx+4]
        sub     esp, 4                  // room for return address

        mov     ecx, [ecx-8]            // reload mode

CBCommon:
        // (eax) == function to call
        // (ecx) == mode
        
        // mark PSL boundary
        mov     dword ptr fs:[0], -2

        // save registers for exception handling
        mov     edx, PtrCurThd          // (eax) = ptr to THREAD struct
        mov     edx, [edx].pcstkTop     // (eax) = ptr to CALLSTACK struct
        mov     [edx].ExEsp, esp        // .\     v
        mov     [edx].ExEbp, ebp        // ..\    v
        mov     [edx].ExEbx, ebx        // ...> save registers for possible exception recovery
        mov     [edx].ExEsi, esi        // ../
        mov     [edx].ExEdi, edi        // ./

        // (ecx) == mode
        cmp     ecx, KERNEL_MODE
        je      short CBInKMode

        // setup return address to trap back        
        mov     dword ptr [esp], SYSCALL_RETURN

        // setup far-return stack
        mov     edx, esp
        push    KGDT_R3_DATA | 3        // SS of ring 3
        push    edx                     // target ESP
        push    KGDT_R3_CODE | 3        // CS of ring 3
        push    eax                     // function to call
        // return to user code
        retf

CBInKMode:
        pop     edx                 // get rid of return address
        call    eax                 // call the function directly

CBRtnCommon:        
        // (eax) == return value for callback
        push    eax                 // save return value
        sub     esp, 4              // room for linkage
        push    esp                 // arg0 == pLinkage
        call    CallbackReturn

        // (eax) == return address
        mov     ecx, eax            // (ecx) == return address
        
        mov     eax, [esp+4]
        mov     fs:[0], eax         // restore exception linkage
        mov     eax, [esp+8]        // restore return value
        add     esp, 12             // remove the stuffs we pushed onto 
        push    ecx                 // push return address onto stack
        
        ret                         // return to caller of PerformCallback in KMODE
        
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int __declspec(naked) 
KCall(
    PKFN pfn, 
    ...
    )
{
    __asm {
        push    ebp
        mov     ebp, esp
        push    ebx
        mov     eax, 12[ebp]    // (eax) = arg0
        mov     edx, 16[ebp]    // (edx) = arg1
        mov     ecx, 20[ebp]    // (ecx) = arg2
        mov     ebx, 8[ebp]     // (ebx) = function address
        cmp     [KData].cNest, 1
        jne     short kcl50     // Already in non-preemtible state
        int KCALL_INT           // trap to ring0 for non-preemtible stuff
        pop     ebx
        pop     ebp             // restore original EBP
        ret

kcl50:  push    ecx             // push Arg2
        push    edx             // push Arg1
        push    eax             // push Arg0
        call    ebx             // invoke function
        add     esp, 3*4        // remove args from stack
        pop     ebx
        pop     ebp             // restore original EBP
        ret
    }
}
#pragma warning(default:4035 4733)         // Turn warning back on




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
Naked 
Int22KCallHandler(void)
{
    __asm {
        push    eax                 // fake Error Code
        pushad
        mov     esi, esp            // save ptr to register save area
        mov     esp, offset KData-4 // switch to kernel stack
        dec     [KData].cNest
        sti
        push    ecx                 // push Arg2
        push    edx                 // push Arg1
        push    eax                 // push Arg0
        call    ebx                 // invoke non-preemtible function
        mov     [esi+7*4], eax      // save return value into PUSHAD frame
        mov     ecx, OFFSET KData
        xor     edi, edi            // force Reschedule to reload the thread's state
        cli

        cmp     word ptr ([ecx].bResched), 1
        jne     short NotReschedule
        jmp     Reschedule
NotReschedule:
        cmp     dword ptr ([ecx].dwKCRes), 1
        jne     short NotResched2
        jmp     Reschedule
NotResched2:
        mov     esp, esi
        inc     [ecx].cNest
        popad
        add     esp, 4              // throw away the error code
        iretd
    }
}


//------------------------------------------------------------------------------
// rdmsr - C wrapper for the assembly call. Must be in RING 0 to make this call!
// Assumes that the rdmsr instruction is supported on this CPU; you must check
// using the CPUID instruction before calling this function.
//------------------------------------------------------------------------------
static void rdmsr(
    DWORD dwAddr,       // Address of MSR being read
    DWORD *lpdwValHigh, // Receives upper 32 bits of value, can be NULL
    DWORD *lpdwValLow   // Receives lower 32 bits of value, can be NULL
    )
{
    DWORD dwValHigh, dwValLow;

    _asm {
        ;// RDMSR: address read from ECX, data returned in EDX:EAX
        mov     ecx, dwAddr
        rdmsr
        mov     dwValHigh, edx
        mov     dwValLow, eax
    }

    if (lpdwValHigh) {
        *lpdwValHigh = dwValHigh;
    }
    if (lpdwValLow) {
        *lpdwValLow = dwValLow;
    }
}


//------------------------------------------------------------------------------
// NKrdmsr - C wrapper for the rdmsr assembly call.  Handles getting into
// ring 0 but does not check whether MSRs are supported or whether the
// particular MSR address being read from is supported.
//------------------------------------------------------------------------------
BOOL
NKrdmsr(
    DWORD dwAddr,       // Address of MSR being read
    DWORD *lpdwValHigh, // Receives upper 32 bits of value, can be NULL
    DWORD *lpdwValLow   // Receives lower 32 bits of value, can be NULL
    )
{
    if (InSysCall()) {
        rdmsr(dwAddr, lpdwValHigh, lpdwValLow);
    } else {
        KCall((PKFN)rdmsr, dwAddr, lpdwValHigh, lpdwValLow);
    }

    return TRUE;
}


//------------------------------------------------------------------------------
// wrmsr - C wrapper for the assembly call. Must be in RING 0 to make this call!
// Assumes that the wrmsr instruction is supported on this CPU; you must check
// using the CPUID instruction before calling this function.
//------------------------------------------------------------------------------
static void wrmsr(
    DWORD dwAddr,       // Address of MSR being written
    DWORD dwValHigh,    // Upper 32 bits of value being written
    DWORD dwValLow      // Lower 32 bits of value being written
    )
{
    _asm {
        ;// WRMSR: address read from ECX, data read from EDX:EAX
        mov     ecx, dwAddr
        mov     edx, dwValHigh
        mov     eax, dwValLow
        wrmsr
    }
}


//------------------------------------------------------------------------------
// NKwrmsr - C wrapper for the wrmsr assembly call.  Handles getting into
// ring 0 but does not check whether MSRs are supported or whether the
// particular MSR address being written to is supported.
//------------------------------------------------------------------------------
BOOL
NKwrmsr(
    DWORD dwAddr,       // Address of MSR being written
    DWORD dwValHigh,    // Upper 32 bits of value being written
    DWORD dwValLow      // Lower 32 bits of value being written
    )
{
    if (InSysCall()) {
        wrmsr(dwAddr, dwValHigh, dwValLow);
    } else {
        KCall((PKFN)wrmsr, dwAddr, dwValHigh, dwValLow);
    }

    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
InitializePageTables(void)
{

    InitIDTEntry(SYSCALL_INT, KGDT_R1_CODE | 1, Int20SyscallHandler(), RING3_INT_GATE);
    InitIDTEntry(KCALL_INT, KGDT_R0_CODE, Int22KCallHandler, RING1_INT_GATE);

    NKDbgPrintfW (L"g_pPageDir = %8.8lx\n", g_pPageDir);
}

///////////////////////////// FLOATING POINT UNIT CODE /////////////////////////////////



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
InitializeEmx87(void) 
{
    // Fast FP save/restore instructions are not available when emulating FP
    KCALLPROFON(70);
    ProcessorFeatures &= ~CPUID_FXSR;
    _asm {
        mov eax, cr0
        or  eax, MP_MASK or EM_MASK
        and eax, NOT (TS_MASK or NE_MASK)
        mov cr0, eax
    }
    KCALLPROFOFF(70);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
InitNPXHPHandler(
    LPVOID NPXNPHandler
    ) 
{
    KCALLPROFON(71);
    if(pTOC->ulKernelFlags & KFLAG_NOTALLKMODE)
    {
        InitIDTEntry(0x07, KGDT_R3_CODE, NPXNPHandler, RING3_TRAP_GATE);
    }
    else
    {
        InitIDTEntry(0x07, KGDT_R1_CODE, NPXNPHandler, RING1_TRAP_GATE);
    }
    KCALLPROFOFF(71);
}

BOOL __declspec(naked) NKIsSysIntrValid (DWORD idInt)
{
    _asm {
        mov     eax, [esp+4]            // (eax) = idInt
        sub     eax, SYSINTR_DEVICES    // (eax) = idInt - SYSINTR_DEVICES
        jb      NKIRetFalse             // return FALSE if < SYSINTR_DEVICES

        cmp     eax, SYSINTR_MAX_DEVICES // (eax) >= SYSINTR_MAX_DEVICES?
        jae     NKIRetFalse             // return FALSE if >= SYSINTR_MAX_DEVICES

        //; idInt is valid, return IntrEvents[idInt-SYSINTR_DEVICES]
        mov     eax, dword ptr KData.alpeIntrEvents[eax*4]

        ret

    NKIRetFalse:
        xor     eax, eax                // return FALSE
        ret
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
Naked 
FPUNotPresentException(void)
{
    _asm {
        push    eax                 // Fake error code
        
        // We cannot be emulating FP if we arrive here. It is safe to not check
        // if CR0.EM is set.
        pushad
        clts

        mov     ebx, OFFSET KData
        dec     [ebx].cNest         // count kernel reentrancy level
        mov     esi, esp            // (esi) = original stack pointer
        jnz     short fpu10
        lea     esp, [ebx-4]        // switch to kernel stack (&KData-4)
 fpu10:
        sti

        mov     eax, PtrCurThd
        push    (PTHREAD)[eax].aky
        mov     (PTHREAD)[eax].aky, 0xffffffff

        mov     eax, g_CurFPUOwner
        test    eax, eax
        jz      NoCurOwner
        mov     eax, [eax].tlsSecure
        sub     eax, FLTSAVE_BACKOFF
        and     eax, 0xfffffff0             // and al, f0 causes processor stall
        test    ProcessorFeatures, CPUID_FXSR
        jz      fpu_fnsave
        FXSAVE_EAX
        jmp     NoCurOwner
fpu_fnsave:
        fnsave  [eax]
NoCurOwner:
        mov     eax, PtrCurThd
        mov     g_CurFPUOwner, eax
        pop     (PTHREAD)[eax].aky
        mov     eax, [eax].tlsSecure
        sub     eax, FLTSAVE_BACKOFF
        and     eax, 0xfffffff0             // and al, f0 causes processor stall
        test    ProcessorFeatures, CPUID_FXSR
        jz      fpu_frestor
        FXRESTOR_EAX
        jmp     fpu_done
fpu_frestor:
        frstor  [eax]
fpu_done:
        cli
        
        cmp     word ptr ([KData].bResched), 1
        je      short fpu_resched           // must reschedule now
        inc     [ebx].cNest                 // back out of kernel one level
        mov     esp, esi                    // restore stack pointer
        popad
        add     esp, 4                      // skip fake error code
        iretd
    
        // The reschedule flag was set and we are at the first nest level into the kernel
        // so we must reschedule now.

fpu_resched:
        mov     edi, PtrCurThd      // (edi) = ptr to current THREAD
        jmp     Reschedule
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
FPUFlushContext(void) 
{
    FLOATING_SAVE_AREA *pFSave;
    if (g_CurFPUOwner) {
        ACCESSKEY ulOldKey;
        SWITCHKEY(ulOldKey,0xffffffff);
        _asm {
            // If we are emulating FP, g_CurFPUOwner is always 0 so we don't
            // have to test if CR0.EM is set(i.e. fnsave will not GP fault).
            clts
        }
        if (g_CurFPUOwner->pThrdDbg && g_CurFPUOwner->pThrdDbg->psavedctx) {
            pFSave = &g_CurFPUOwner->pThrdDbg->psavedctx->FloatSave;
            
            _asm {
                mov eax, pFSave
                fnsave [eax]
            }
        }  else  {
            pFSave = PTH_TO_FLTSAVEAREAPTR(g_CurFPUOwner);
            _asm  {
                mov     eax, pFSave
                test    ProcessorFeatures, CPUID_FXSR
                jz      flush_fsave
                FXSAVE_EAX
                jmp     flush_done
            flush_fsave:
                fnsave   [eax]
                fwait
            flush_done:
            }
        }
        _asm  {
            mov     eax, CR0        // fnsave destroys FP state &
            or      eax, TS_MASK    // g_CurFPUOwner is 0 so we must force
            mov     CR0, eax        // trap 7 on next FP instruction
        }
        SETCURKEY(ulOldKey);
        g_CurFPUOwner = 0;
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
Naked 
FPUException(void)
{
    _asm {
        push    eax                 // Fake error code
        pushad
        xor     ecx, ecx            // EA = 0
        mov     esi, 16
        jmp     CommonFault
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
InitializeFPU(void)
{
    KCALLPROFON(69);    
    InitIDTEntry(0x07, KGDT_R0_CODE, FPUNotPresentException, INTERRUPT_GATE);
    InitIDTEntry(0x10, KGDT_R0_CODE, FPUException, INTERRUPT_GATE);

    _asm {
        mov     eax, cr0
        or      eax, MP_MASK OR NE_MASK
        and     eax, NOT (TS_MASK OR EM_MASK)
        mov     cr0, eax

        finit

        fwait
        mov     ecx, offset g_InitialFPUState
        add     ecx, 10h                    // Force 16 byte alignment else
        and     cl, 0f0h                    // fxsave will fault
        test    ProcessorFeatures, CPUID_FXSR
        jz      no_fxsr

        MOV_EDX_CR4
        or      edx, CR4_FXSR
        MOV_CR4_EDX

        FXSAVE_ECX
        mov     [ecx].MXCsr, 01f80h                       // Mask KNI exceptions
        and     word ptr [ecx], NOT NPX_CW_PRECISION_MASK // Control word is
        or      word ptr [ecx], NPX_CW_PRECISION_53       // 16 bits wide here
        jmp     init_done
no_fxsr:
        fnsave  [ecx]
        // Win32 threads default to long real (53-bit significand).
        // Control word is 32 bits wide here
        and     dword ptr [ecx], NOT NPX_CW_PRECISION_MASK
        or      dword ptr [ecx], NPX_CW_PRECISION_53
init_done:

        or      eax, TS_MASK
        mov     cr0, eax
   }
   KCALLPROFOFF(69);    
}


