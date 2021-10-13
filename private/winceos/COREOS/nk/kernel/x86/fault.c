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

/*+
    fault.c - iX86 fault handlers
 */
#include "kernel.h"

// disable short jump warning.
#pragma warning(disable:4414)

///#define LIGHTS(n)   mov dword ptr ss:[0AA001010h], ~(n)&0xFF

BOOL HandleException(PTHREAD pth, int id, ulong addr);
void NextThread(void);
void KCNextThread(void);
void Reschedule(void);
void RunThread(void);
void RunThreadNotReschedule (void);
void DumpTctx(PTHREAD pth, int id, ulong addr, int level);
DWORD HandleIpi (void);

extern DWORD ProcessorFeatures;

FXSAVE_AREA g_InitialFPUState;

PTHREAD pthFakeStruct;  // Used to implement GetEPC() for profiling


extern volatile LONG g_nCpuStopped;

#define CSTK_IMETHOD_OFFSET   0x58
ERRFALSE(CSTK_IMETHOD_OFFSET == offsetof(CALLSTACK, iMethod));

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


#define PERFORMCALLBACK     -30    // MUST be -PerformCallback Win32Methods in kwin32.c 
                                    // 30 == -(APISet 0, method 30)

#define Naked void __declspec(naked)

#define SANATIZE_SEG_REGS(eax, ax)       \
    _asm { cld } \
    _asm { mov eax, KGDT_R3_DATA }  \
    _asm { mov ds, ax } \
    _asm { mov es, ax } \
    _asm { mov eax, KGDT_PCR }  \
    _asm { mov fs, ax } \
    _asm { mov eax, KGDT_KPCB } \
    _asm { mov gs, ax }


// #define ONE_ENTRY


//
// The Physical to Virtual mapping table is supplied by OEM.
//

BOOL MDValidateRomChain (ROMChain_t *pROMChain)
{
    PADDRMAP pAddrMap;
    DWORD dwEnd;
    
    for ( ; pROMChain; pROMChain = pROMChain->pNext) {
        for (pAddrMap = g_pOEMAddressTable; pAddrMap->dwSize; pAddrMap ++) {
            dwEnd = pAddrMap->dwVA + pAddrMap->dwSize;
            if (IsInRange (pROMChain->pTOC->physfirst, pAddrMap->dwVA, dwEnd)) {
                if (IsInRange (pROMChain->pTOC->physlast, pAddrMap->dwVA, dwEnd)) {
                    // good XIP, break inner loop and go on to the next region
                    break;
                }
                // bad
                NKDbgPrintfW (L"MDValidateRomChain: XIP (%8.8lx -> %8.8lx) span accross multiple memory region\r\n",
                        pROMChain->pTOC->physfirst, pROMChain->pTOC->physlast);
                return FALSE;
            }
        }
        if (!pAddrMap->dwSize) {
            NKDbgPrintfW (L"MDValidateRomChain: XIP (%8.8lx -> %8.8lx) doesn't exist in OEMAddressTable \r\n",
                        pROMChain->pTOC->physfirst, pROMChain->pTOC->physlast);
            return FALSE;
        }
    }
    return TRUE;
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

#define PFCODE_UMODE_WRITE  (PFCODE_WRITE|PFCODE_USER_MODE)

BOOL LoadPageTable (DWORD addr, DWORD dwEip, DWORD dwErrCode)
{
    BOOL fRet = FALSE;
    DWORD idxPD = VA2PDIDX (addr);
    DWORD idx2nd = VA2PT2ND (addr);    
    DWORD dwEntry = g_ppdirNK->pte[idxPD] & ~PG_ACCESSED_MASK;

    if ((addr >= VM_SHARED_HEAP_BASE) && dwEntry) {
        PPAGEDIRECTORY ppdir = pVMProc->ppdir;
        DWORD dwPfn = GetPFN (ppdir);
        DWORD dwCr3;

        _asm {
            mov eax, cr3
            mov dwCr3, eax
        }

        if (dwCr3 != dwPfn) {
            // pVMProc changed, but CR3 hasn't been updated yet. Update CR3 and return TRUE.
            _asm {
                mov eax, dwPfn
                mov cr3, eax
            }
            return TRUE;
        }

        if (addr < VM_KMODE_BASE) {
            // shared heap. Need to look into page table entry
            PPAGETABLE pptbl = GetPageTable (g_ppdirNK, idxPD);

            if (((dwErrCode & PFCODE_UMODE_WRITE) != PFCODE_UMODE_WRITE)            // r/o from user mode
                && (PG_VALID_MASK & pptbl->pte[idx2nd])) {
                ppdir->pte[idxPD] = (PFCODE_USER_MODE & dwErrCode)
                                  ? ((dwEntry & ~PG_WRITE_MASK) | PG_USER_MASK)     // user mode - r/o
                                  : ((dwEntry & ~PG_USER_MASK) | PG_WRITE_MASK);    // kernel mode - r/w
                OEMCacheRangeFlush (0, 0, CACHE_SYNC_FLUSH_TLB);
                return TRUE;
            }
        } else if (!ppdir->pte[idxPD]) {
            ppdir->pte[idxPD] = dwEntry;
            return TRUE;
        }
    }

    if (KSEN_LoadPageTable(addr, dwEip, dwErrCode)
        && !IsInSharedHeap(addr)
        && (IsKernelVa((LPCVOID)dwEip)
            || IsAddrInMod ((PMODULE)(IsKModeAddr(dwEip) ? hKCoreDll : hCoreDll), dwEip))) {
            
        PPROCESS pprc = (addr > VM_KMODE_BASE) ? g_pprcNK : pVMProc;
        PPAGETABLE pptbl = GetPageTable (pprc->ppdir, idxPD);
        
        KSLV_LoadPageTable(addr, dwEip, &pptbl->pte[idx2nd], &fRet);    
        if (fRet) {
            OEMCacheRangeFlush ((LPVOID)PAGEALIGN_DOWN(addr), 1, CACHE_SYNC_FLUSH_TLB);
        }
    }

    DEBUGMSG (fRet && ZONE_VIRTMEM, (L"LoadPageTable failed, addr = %8.8lx, dwErrCode = %8.8lx\r\n", addr, dwErrCode));
    return fRet;
    
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
//  ESP     points to stack frame containing the faulted context.
//
//  EDX     is the faulting address which is passed to HandleException
//
//  EAX     is the exception id which is passed to HandleException
//
//  Return:
//   CommonFault jumps to Reschedule or resumes execution based on the return
//   value of HandleException.
//
//------------------------------------------------------------------------------
Naked 
CommonFault()
{
    SANATIZE_SEG_REGS(ecx, cx)

    _asm {
        mov     ebx, gs:[0] PCB.pSelf       // ebx = ppcb
        dec     [ebx].cNest                 // decrement ppcb->cNest
        cmp     word ptr [esp].TcxCs, KGDT_R0_CODE   // exception occurred at ring 0?
        je      short cf20                  // nested fault

        // copy context on stack onto thread structure
        mov     edi, [ebx].pCurThd                  // (edi) = pCurThread
        add     edi, THREAD_CONTEXT_OFFSET          // (edi) = &pCurThread->ctx
        mov     esi, esp                            // (esi) = ptr to saved context on stack
        mov     ecx, size CPUCONTEXT / 4            // (ecx) = # of DWORDS
        rep     movsd                               // copy
        mov     edi, [ebx].pCurThd                  // (edi) = pCurThread
        
cf10:   sti
        push    edx
        push    eax
        push    edi
        call    HandleException
        add     esp, 3*4
        test    eax, eax
        jnz     RunThread
        jmp     Reschedule

// Nested exception. Create a fake thread structure on the stack
cf20:   sub     esp, THREAD_CONTEXT_OFFSET
        mov     edi, esp            // (edi) = ptr to fake thread struct
        jmp     short cf10
    }
}


//
// (ebx) = ppcb
// (esp) = (edi) = Fake thread structure
//
Naked
RtnFromNested (void)
{
    _asm {
        inc     [ebx].cNest
        add     esp, THREAD_CONTEXT_OFFSET
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
//
// Do a reschedule.
//
//  (edi) = ptr to current thread or 0 to force a context reload
//  (ebx) = ptr to per-CPU global
//
//------------------------------------------------------------------------------
Naked 
Reschedule()
{
    __asm {
        sti

        // cannot reschedule for nested faults
        cmp     edi, esp 
        je      RtnFromNested

        // cannot reschedule if current cpu holds a spinlock
        mov     eax, dword ptr [ebx].ownspinlock
        test    eax, eax
        jnz     RunThreadNotReschedule          // we own a spin lock - can't reschedule

        // acquire scheduler spin lock
        push    offset g_schedLock
        call    AcquireSpinLock
        pop     ecx

rsd10:
        cmp     word ptr ([ebx].bResched), 1    // NOTE: comparing bResched && cNest at the same time (word ptr is the trick)
        jne     short rsd11
        mov     word ptr ([ebx].bResched), 0
        call    NextThread
rsd11:
        cmp     dword ptr ([ebx].dwKCRes), 1
        jne     short rsd12
        mov     dword ptr ([ebx].dwKCRes), 0
        call    KCNextThread

        cmp     dword ptr ([ebx].dwKCRes), 1
        je      short rsd10

rsd12:
        call    SendIPIAndReleaseSchedulerLock

        mov     eax, [ebx].pthSched
        test    eax, eax
        jz      short rsd50           // nothing to run
        cmp     eax, edi
        je      RunThread             // redispatch the same thread

// Switch to a new thread's process context.
// Switching to a new thread. Update current process and address space
// information. Edit the ring0 stack pointer in the TSS to point to the
// new thread's register save area.
//
//      (eax) = ptr to thread structure
//      (ebx) = ptr to ppcb

        mov     edi, eax                // Save thread pointer
        push    edi
        call    SetCPUASID              // Sets dwCurProcId for us!
        pop     ecx                     // Clean up stack

        cmp     edi, [ebx].pCurFPUOwner
        jne     SetTSBit
        clts
        jmp     short RunThread

SetTSBit:
        mov     eax, CR0
        test    eax, TS_MASK
        jnz     short RunThread
        or      eax, TS_MASK
        mov     CR0, eax

        jmp     short RunThread               // Run thread pointed to by edi

// No threads ready to run. Call OEMIdle to shutdown the cpu.
// (ebx) = pcb
rsd50:  cli
        cmp     word ptr ([ebx].bResched), 1    // NOTE: comparing bResched && cNest at the same time (word ptr is the trick)
        je      Reschedule
        call    NKIdle
        mov     byte ptr ([ebx].bResched), 1
        xor     edi, edi
        jmp     Reschedule
    }
}



//------------------------------------------------------------------------------
// (esi) = ISR to call, registers all saved to thread structure (or stack, if nested)
//------------------------------------------------------------------------------
Naked 
CommonIntDispatch()
{
    SANATIZE_SEG_REGS(eax, ax)

    _asm {
        mov     ebx, gs:[0] PCB.pSelf   // ebx = ppcb
        dec     [ebx].cNest
        cmp     word ptr [esp].TcxCs, KGDT_R0_CODE
        je      short cid20             // nested fault

        // copy context on stack onto thread structure
        mov     eax, esi                            // save esi
        mov     edi, [ebx].pCurThd                  // (edi) = pCurThread
        add     edi, THREAD_CONTEXT_OFFSET          // (edi) = &pCurThread->ctx
        mov     esi, esp                            // (esi) = ptr to saved context on stack
        mov     [ebx].pSavedContext, esi            // Save pointer to this process's context.
        mov     ecx, size CPUCONTEXT / 4            // (ecx) = # of DWORDS
        rep     movsd                               // copy
        mov     edi, [ebx].pCurThd                  // (edi) = pCurThread
        mov     esi, eax                            // restore esi
        
cid10:
        //===================================================================
        //
        // Log the ISR entry event to CeLog
        //
        
        // Skip logging if KDataCelogStatus == 0
        mov     ecx, dword ptr [g_pKData]           // (ecx) = g_pKData
        cmp     [ecx].dwCelogStatus, 0
        je      short cid12

        mov     eax, 80000000h      // mark as ISR entry
        push    eax                 // Arg 0, cNest + SYSINTR_xxx
        call    CELOG_Interrupt
        pop     eax                 // cleanup the stack from the call
cid12:
        //===================================================================


        sti

        push    [edi].ctx.TcxEip                    // push interrupted PC
        call    esi
        pop     ecx

        // handle IPI
        cmp     eax, SYSINTR_IPI
        jne     short cid11
        call    HandleIpi

cid11:
        push    eax                                 // push argument == SYSINTR returned
        call    OEMNotifyIntrOccurs                 // notify OEM interrupt occurred
        pop     ecx                                 // dummy pop

        cli


        //===================================================================
        //
        // Log the ISR exit event to CeLog
        //
        
        // Skip logging if dwCelogStatus == 0
        mov     ecx, dword ptr [g_pKData]           // (ecx) = g_pKData
        cmp     [ecx].dwCelogStatus, 0
        je      short cid13

        push    eax                 // Save original SYSINTR return value.
        bswap   eax                 // Reverse endian
        mov     ah, [ebx].cNest     // Nesting level (0 = no nesting, -1 = nested once)
        neg     ah                  // Nesting level (0 = no nesting,  1 = nested once)
        bswap   eax                 // Reverse endian
        push    eax                 // Arg 0, cNest + SYSINTR_xxx
        call    CELOG_Interrupt
        pop     eax                 // cleanup the stack from the call
        pop     eax                 // restore original SYSINTR value
cid13:
        //===================================================================


        test    eax, eax
        jz      short RunThread     // SYSINTR_NOP: nothing more to do

        mov     byte ptr [ebx].bResched, 1 // force reschedule even for invalid sysintr
        
        lea     ecx, [eax-SYSINTR_DEVICES]
        cmp     ecx, SYSINTR_MAX_DEVICES    // SYSINTR_RESCHED is filtered with this compare
        jae     short RunThread         

// A device interrupt has been signaled. Set the appropriate bit in the pending
// events mask and set the reschedule flag. The device event will be signaled
// by the scheduler.
//  (ebx) = pcb
//  (ecx) = SYSINTR
//  DEBUGCHK (ppcb == g_pKData);    // only BSP should've ever taken interrupts

        lea     esi, [ebx].aPend1   // (esi) = &g_pKData->aPend1
        cmp     ecx, 32             // ISR# >= 32?
        jb      short cid14         // jump to common handler if ISR# < 32

        // ISR# >= 32
        sub     cl, 32              // sysintr -= 32
        lea     esi, [ebx].aPend2   // (esi) = &g_pKData->aPend2

cid14:
        // common part for updating PendEvent(1/2)
        mov     ebp, 1
        shl     ebp, cl             // (ebp) = bit to set (1 << sysintr) or (1 << (sysintr-32))


        // InterlockedCompareExchange loop to update PendEvent(1/2)
cid15:
        mov     eax, dword ptr [esi]// (eax) = g_pKData->aPend(1/2) (old value)
        mov     edx, eax            // (edx) = old value 
        mov     ecx, eax
        or      ecx, ebp            // (ecx) = eax|ebp (new value)
        lock cmpxchg [esi], ecx
        cmp     eax, edx
        je      short RunThread     // continue if successful

        // failed cmpxchg, retry
        jmp     cid15

// Nested exception. Create a fake thread structure on the stack
cid20:  sub     esp, THREAD_CONTEXT_OFFSET
        mov     edi, esp            // (edi) = ptr to fake thread struct
        mov     dword ptr (pthFakeStruct), edi  // Used to implement GetEPC() for profiling
        jmp     short cid10
    }
}


//------------------------------------------------------------------------------
//
// Continue thread execution.
//
//  (edi) = ptr to Thread structure
//  (ebx) = ptr to ppcb
//
//------------------------------------------------------------------------------
Naked 
RunThread (void)
{
    _asm {
        cli

        cmp     esp, edi
        je      RtnFromNested

        cmp     word ptr ([ebx].bResched), 1    // NOTE: comparing bResched && cNest at the same time (word ptr is the trick)
        je      Reschedule
        jmp     RunThreadNotReschedule

    }
}


//------------------------------------------------------------------------------
//
// Continue thread execution.
//
//  (edi) = ptr to Thread structure
//  (ebx) = ptr to ppcb
//
//------------------------------------------------------------------------------
Naked
RunThreadNotReschedule()
{
    _asm {
        cmp     esp, edi
        je      RtnFromNested

        // restore segment registers
        inc     [ebx].cNest

        add     edi, THREAD_CONTEXT_OFFSET
        mov     ax, word ptr [edi].TcxGs
        mov     gs, ax
        mov     ax, word ptr [edi].TcxFs
        mov     fs, ax
        mov     ax, word ptr [edi].TcxEs
        mov     es, ax
        mov     ax, word ptr [edi].TcxDs
        mov     ds, ax

        // restore general purpose registers, except edi
        mov     eax, [edi].TcxEax
        mov     ebx, [edi].TcxEbx
        mov     ecx, [edi].TcxEcx
        mov     edx, [edi].TcxEdx
        mov     esi, [edi].TcxEsi
        mov     ebp, [edi].TcxEbp

        // prepare return frame
        push    [edi].TcxSs
        push    [edi].TcxEsp
        push    [edi].TcxEFlags
        push    [edi].TcxCs
        push    [edi].TcxEip

        // restore edi
        mov     edi, [edi].TcxEdi

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
        push    ds
        push    es
        push    fs
        push    gs
        
        SANATIZE_SEG_REGS(eax, ax)

        mov     ebx, gs:[0] PCB.pSelf       // ebx = ppcb
        mov     edi, cr2
        test    edi, edi
        jns     short pf05 

        // Address > 2GB, kmode only
        mov     esi, [esp].TcxError
        and     esi, 4
        jnz     short pf50              // prevelige vialoation, get out now
        
pf05:
        dec     [ebx].cNest             // count kernel reentrancy level
        mov     esi, esp                // (esi) = original stack pointer
        
//  Process a page fault for the "user" address space (0 to 0x7FFFFFFF)
//
//  (edi) = Faulting address
//  (ebx) = ptr to ppcb
//  (esi) = ptr to context saved (original esp)

        cmp     dword ptr ([ebx].dwInDebugger), 0   // see if debugger active
        jne     short pf20                          // if so, skip turning on of interrupts
        sti                                         // enable interrupts

pf20:   push    [esi].TcxError
        push    [esi].TcxEip
        push    edi
        call    LoadPageTable
        cli
        test    eax, eax
        jz      short pf40          // page not found in the Virtual memory tree
        cmp     word ptr ([ebx].bResched), 1    // NOTE: comparing bResched && cNest at the same time (word ptr is the trick)
        je      short pf60          // must reschedule now
        inc     [ebx].cNest         // back out of kernel one level
        mov     esp, esi            // restore stack pointer

        pop     gs
        pop     fs
        pop     es 
        pop     ds
        popad
        add     esp, 4
        iretd

        
//  This one was not a good one!  Jump to common fault handler
//
//  (edi) = faulting address

pf40:   inc     [ebx].cNest         // back out of kernel one level
        mov     esp, esi            // restore stack pointer
pf50:   mov     edx, edi            // (edx) = fault effective address
        mov     eax, 0Eh
        jmp     CommonFault

// The reschedule flag was set and we are at the first nest level into the kernel
// so we must reschedule now.

pf60:
        // copy context onto thread structure
        mov     edi, [ebx].pCurThd                  // (edi) = ptr to current THREAD
        add     edi, THREAD_CONTEXT_OFFSET          // (edi) = &pCurThread->ctx
        mov     ecx, size CPUCONTEXT / 4            // (ecx) = # of DWORDS
        rep     movsd                               // copy
        mov     edi, [ebx].pCurThd                  // (edi) = pCurThread
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
        push    ds
        push    es
        push    fs
        push    gs
        xor     edx, edx            // (edx) = 0 (fault effective address)
        mov     eax, 13
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
        push    ds
        push    es
        push    fs
        push    gs
        mov     eax, 6
        xor     edx, edx            // (edx) = 0 (fault effective address)
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
        push    ds
        push    es
        push    fs
        push    gs
        xor     eax, eax            // (eax) = 0 (divide by zero fault)
        xor     edx, edx            // (edx) = 0 (fault effective address)
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
LONG __declspec(naked) 
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
        push    ds
        push    es
        push    fs
        push    gs
        mov     eax, 1
        xor     edx, edx            // (edx) = 0 (fault effective address)
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
        push    ds
        push    es
        push    fs
        push    gs
        mov     eax, 2
        xor     edx, edx            // (edx) = 0 (fault effective address)
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
        push    ds
        push    es
        push    fs
        push    gs
        mov     eax, 3
        xor     edx, edx            // (ecx) = 0 (fault effective address)
        jmp     CommonFault
    }
}


#pragma warning(disable:4035 4733)
//
// worker funciton to update registration pointer
// (ecx) = pcr at entrance
//
Naked DoUpdateRegistrationPtr (void)
{
    _asm {
        inc     gs:[0] PCB.ownspinlock          // stop preemption

        mov     edx, gs:[0] PCB.pGDT
        add     edx, KGDT_PCR                   // (edx) = FS entry in global descriptor table
        mov     word ptr [edx+2], cx            // set low word of FS base
        shr     ecx, 16
        mov     byte ptr [edx+4], cl            // set third byte of FS base
        mov     byte ptr [edx+7], ch            // set high byte of FS base
        push    fs
        pop     fs                              // cause fs to reload

        dec     gs:[0] PCB.ownspinlock          // enable preemption

        cmp     byte ptr gs:[0] PCB.bResched, 1 // need to reschedule?
        jne     ReturnToCaller

        cmp     byte ptr gs:[0] PCB.cNest, 1    // nested?
        jne     ReturnToCaller

        // need to reschedule
        mov     edx, offset ReturnSelf
        int     KCALL_INT

ReturnToCaller:
        ret
    }
}

//------------------------------------------------------------------------------
//
// Function: 
//      void UpdateRegistrationPtr (NK_PCR *pcr);
//
// Description:
//      update registration pointer on thread switch
//
Naked UpdateRegistrationPtr (NK_PCR *pcr)
{
    _asm {
        mov     ecx, [esp+4]                    // (ecx) = pcr
        jmp     DoUpdateRegistrationPtr
    }
}

//------------------------------------------------------------------------------
//
// Function: 
//      void UpdateRegistrationPtrWithTLS 
//
// Description:
//      update registration pointer on stack switch
//      AT ENTRANCE (ecx) = TLSPTR
//      NOTE: ONLY USE ECX and EDX, FOR THIS IS CALLED DIRECT FROM ASSEMBLY
//
Naked UpdateRegistrationPtrWithTLS (void)
{
    _asm {
        sub     ecx, FS_LIMIT+1
        jmp     DoUpdateRegistrationPtr
    }
}


//------------------------------------------------------------------------------
//
// Function: 
//      void CallUModeFunction 
//
// Description:
//      call user mode function
//      AT ENTRANCE (edi) = target SP, (eax) = function to call, retun
//                          address must have been setup correctly on
//                          target stack (ususally SYSCALL_RETURN).
//
//      NOTE: THIS IS CALLED DIRECT FROM ASSEMBLY, edi must have been 
//            saved correctly per calling convention.
//            USE ESI
//
Naked CallUModeFunction (void)
{
    _asm {
        // (edi) = newSP (SYSCALL_RETURN already pushed onto target stack)
        // (eax) = function to call

        // switch TLS to nonsecure stack
        mov     edx, gs:[0] PCB.pCurThd         // (edx) = pCurThread
        mov     ecx, [edx].tlsNonSecure         // (ecx) = pCurThread->tlsNonSecure
        mov     [edx].tlsPtr, ecx               // pCurThread->tlsPtr = ecx              
        mov     gs:[0] PCB.lpvTls, ecx          // update global tls ptr
        
        // update fs, (ecx) = tlsptr
        call    UpdateRegistrationPtrWithTLS    // UpdateRegistrationPtrWithTLS uses only ecx and edx

        mov     ecx, KGDT_UPCB
        mov     gs, cx
        
        // setup far return stack
        push    KGDT_R3_DATA | 3                // SS of ring 3
        push    edi                             // target ESP
        push    KGDT_R3_CODE | 3                // CS of ring 3
        push    eax                             // function to call
        
        // return to user code
        retf
    }
}


//------------------------------------------------------------------------------
//
// Function: 
//      DWORD NKPerformCallback (PCALLBACKINFO pcbi, ...);
//
// Description:
//      dirct call to perform callback to user code
//
DWORD __declspec(naked) NKPerformCallBack (PCALLBACKINFO pcbi, ...)
{
    _asm {
        mov     ecx, esp                            // save esp
        sub     esp, size CALLSTACK                 // reserve space for callstack (pNewcstk)
        
        // setup argument to PerfomCallback
        mov     eax, fs:[0]                         // (eax) = registration pointer
        mov     [esp].dwPrevSP, ecx                 // pcstk->dwPrevSP == SP at the point of call
        mov     [esp].regs[REG_OFST_EXCPLIST], eax  // save registration pointer on callstack
        
        push    esp                                 // arg - pNewcstk

        call    NKPrepareCallback

        pop     ecx                                 // pop argument

        // (eax) = function to call
        // NOTE: pcstk->dwNewSP has the target SP, 0 if kernel mode call (call direct, no callstack setup)
        cmp     [esp].dwNewSP, 0
        je      short PCBCallDirect

        // callback to user code

        // save all Callee Saved Registers
        mov     [esp].regs[REG_OFST_ESP], esp        // .\    v
        mov     [esp].regs[REG_OFST_EBP], ebp        // ..\    v
        mov     [esp].regs[REG_OFST_EBX], ebx        // ...> save callee saved registers
        mov     [esp].regs[REG_OFST_ESI], esi        // ../
        mov     [esp].regs[REG_OFST_EDI], edi        // ./

        mov     edi, [esp].dwNewSP                   // (edi) = target SP

        // (edi) = target SP, (eax) = function to call
        jmp     short CallUModeFunction

    PCBCallDirect:
        // direct call, no callstack setup
        add     esp, size CALLSTACK                 // restore SP
        jmp     eax                                 // just jump to EAX, for arg/retaddr are all setup correctly
        
    }
}

//------------------------------------------------------------------------------
//
// System call trap handler.
//
// Pop the iret frame from the stack, switch back to the caller's stack, enable interrupts,
// and dispatch the system call.
//
//      CPU State:  ring1 stack & CS, interrupts disabled.
//                  eax == rtn value if PSL return
//
//
//      top of Stack when called from user-mode:
//                  EIP         (the API call we're trying to make)
//                  CS          KGDT_R3_CODE
//                  EFLAGS      
//                  ESP         (caller's ESP)
//                  SS          KGDT_R3_DATA
//
//      top of stack when called from k-mode
//                  EIP         (the API call we're trying to make)
//                  CS          KGDT_R1_CODE
//                  EFLAGS      
//
//
//------------------------------------------------------------------------------
PFNVOID __declspec(naked) Int20SyscallHandler (void)
{
    __asm {
        // The following three instructions are only executed once on Init time.
        // It sets up the KData PSL return function pointer and returns the
        // the 'real' address of the Int20 handler (sc00 in this case).
        mov eax, dword ptr [g_pKData]
        mov [eax].pAPIReturn, offset APICallReturn
        mov eax, offset sc00
        ret


sc00:   
        SANATIZE_SEG_REGS(ecx, cx)


        pop     ecx                     // (ecx) = randomized EIP+2 of "int SYSCALL"
        push    edx                     // save edx
        mov     edx, gs:[0] PCB.dwPslTrapSeed   // (edx) = PSL trap seed
        sub     ecx, 2                  // (ecx) = randomized EIP of "int SYSCALL"
        xor     ecx, edx                // (ecx) = address of "int SYSCALL"
        pop     edx                     // restore edx

        sub     ecx, FIRST_METHOD       // (ecx) = iMethod * APICALL_SCALE
        cmp     ecx, -APICALL_SCALE     // check callback return
        jne     short NotCbRtn           

        // possibly api/callback return, check to make sure we're in callback
        mov     ecx, gs:[0] PCB.pCurThd // (ecx) = pCurThread
        mov     ecx, [ecx].pcstkTop     // (ecx) = pCurThread->pcstkTop
        test    ecx, ecx                // pCurThread->pcstkTop?
        jnz     short trapReturn

        // pcstkTop is NULL, not in callback/APIcall
        // we let it fall though here for 0 is
        // an invalid API and we'll raise an exception as a result.

NotCbRtn:
        // (ecx) = iMethod << 1
        sar     ecx, 1                  // (ecx) == iMethod
        pop     eax                     // (eax) == caller's CS
        and     al, 0FCh
        cmp     al, KGDT_R1_CODE
        je      short KPSLCall          // caller was in kernel mode

        // caller was in user mode - switch stack
        mov     eax, gs:[0] PCB.pCurThd // (ecx) = pCurThread
        mov     edx, [eax].pcstkTop     // (edx) = pCurThread->pcstkTop
        test    edx, edx                // first trip into kernel?
        jne     short UPSLCallCmn

        // 1st trip into kernel
        mov     edx, [eax].tlsSecure    // (edx) = pCurThread->tlsSecure
        sub     edx, SECURESTK_RESERVE  // (edx) = sp to use
        
UPSLCallCmn:

        // update TlsPtr and ESP
        //      (eax) = pCurThread
        //      (edx) = new SP
        //      (ecx) = iMethod
        //
        // NOTE: must update ESP before we start writing to the secure stack
        //       or we'll fail to DemandCommit stack pages

        // save ecx for we need a register to work with
        push    ecx

        // update TLSPTR
        mov     ecx, [eax].tlsSecure    // (ecx) = pCurThread->tlsSecure
        mov     [eax].tlsPtr, ecx       // pCurThread->tlsPtr = pCurThread->tlsSecure

        // restore ecx
        pop     ecx


        // switch stack
        mov     esp, [esp+4]            // retrieve SP
        xchg    edx, esp                // switch stack, (edx) = old stack pointer
        mov     eax, CST_MODE_FROM_USER // we're calling from user mode

PSLCommon:
        //
        // (eax) - mode
        // (ecx) - iMethod
        // (edx) - prevSP
        // (esp) - new SP
        //

        // enable interrupts
        sti

        //
        //  we're free to use stack from this point
        // 

        // save info in callstack

        sub     esp, size CALLSTACK             // reserve space for callstack, (esp) = pcstk

        // save esi/edi on callstack
        mov     [esp].regs[REG_OFST_ESI], esi     // 
        mov     [esp].regs[REG_OFST_EDI], edi     //
        mov     [esp + CSTK_IMETHOD_OFFSET], ecx // pcstk->iMethod = iMethod

        mov     esi, esp                        // (esi) == pcstk
        
        mov     [esi].dwPrevSP, edx             // pcstk->dwPrevSP = old-SP
//        mov     [esi].retAddr, 0                // retaddr to be filled in ObjectCall, for it's accessing user stack
        mov     [esi].dwPrcInfo, eax            // pcstk->dwPrcInfo == current mode

        // don't touch fs:[0] here, for it's point to user tls. do it in Object call, after validating stack.
        //mov     edx, fs:dword ptr [0]         // exception information
        //mov     [esi].extra, edx              // pcstk->extra = fs:[0]

        // save the rest of registers required for exception recovery
        mov     [esi].regs[REG_OFST_EBP], ebp   // 
        mov     [esi].regs[REG_OFST_EBX], ebx   // 
        mov     [esi].regs[REG_OFST_ESP], esp   // save esp for SEH handling

        test    eax, eax                        // are we called from kernel mode?
        jz      short DoObjectCall      

        // called from user mode, stack changed, update fs
        mov     ecx, gs:[0] PCB.pCurThd
        mov     ecx, [ecx].tlsPtr               // (ecx) = arg to UpdateRegistrationPtrWithTLS
        call    UpdateRegistrationPtrWithTLS    // update FS

        mov     fs:dword ptr [0], REGISTRATION_RECORD_PSL_BOUNDARY            // mark PSL boundary in exception chain (on secure stack)

DoObjectCall:
        // (esi) = pcstk
        push    esi                             // arg0 == pcstk
        
        call    ObjectCall                      // (eax) = api function address

        pop     ecx                             // get rid of arg0
        
        mov     edi, [esi].dwNewSP              // (edi) == new SP if user-mode server

        test    edi, edi

        // eax = function to call, edi = target SP if non-zero
        jnz     short CallUModeFunction

        call    eax                             // & call the api function in KMODE
APICallReturn:

        //  (edx:eax) = function return value

        // save return value in esi:edi (orginal esi/edi are saved in pcstk)
        mov     edi, edx                        // (edi) = high part of return value
        mov     esi, eax                        // (esi) = low part of return value

        call    ServerCallReturn        
        // return address at pcstk->retAddr
    
        // restore eax (edx will be restored later for we still need a register to use)
        mov     eax, esi

        mov     esi, esp                        // (esi) = pcstk

        cmp     [esi].dwPrcInfo, 0              // which mode to return to (0 == kernel-mode)

        jne     short UPSLRtn           

        // returning to KMode caller, just restore registers and return
        mov     edx, edi                        // restore EDX (high part of return value)
        mov     esp, [esi].dwPrevSP             // restore SP
        mov     ebx, [esi].regs[REG_OFST_EBX]   // restore EBX
        mov     edi, [esi].regs[REG_OFST_EDI]   // restore EDI
        mov     esi, [esi].regs[REG_OFST_ESI]   // restore ESI
        ret

UPSLRtn:        
        // returning to user mode process
        // (esi) = pcstk

        // update fs, (esi) = pcstk
        mov     ecx, [esi].pOldTls              // (ecx) = pcstk->pOldTls
        call    UpdateRegistrationPtrWithTLS    // UpdateRegistrationPtrWithTLS uses only ecx and edx

        // restore edx, edi, ebx
        mov     edx, edi                        // restore EDX
        mov     edi, [esi].regs[REG_OFST_EDI]   // restore EDI
        mov     ebx, [esi].regs[REG_OFST_EBX]   // restore ebx

        // setup far return stack
        mov     ecx, KGDT_UPCB
        mov     gs, cx
        mov     ecx, [esi].dwPrevSP             // (ecx) = previous SP
        add     ecx, 4                          // (ecx) = previous SP, without return address
        push    KGDT_R3_DATA | 3                // SS of ring 3
        push    ecx                             // target ESP
        push    KGDT_R3_CODE | 3                // CS of ring 3
        push    [esi].retAddr                   // return address

        mov     esi, [esi].regs[REG_OFST_ESI]   // restore ESI

        // return to user code
        retf

KPSLCall:
        // caller was in kernel mode
        add     esp, 4                  // discard the EFLAGS
        
        xor     eax, eax                // we're in kernel mode

        // is this a callback?
        cmp     ecx, PERFORMCALLBACK
        mov     edx, esp                // prevSP set to the same as ESP

        // jump to PSL common code if not a callback
        jne     short PSLCommon

        // callback - enable interrupt and jump to NKPerformCallback
        sti
        jmp     short NKPerformCallBack


////////////////////////////////////////////////////////////////////////////////////////
// api/callback returns
//      (edx:eax) = return value
//      (ecx) = pCurThread->pcstkTop
//
trapReturn:
        // callback return from user mode
        mov     esp, ecx                        // (esp) = pCurThread->pcstkTop
        sti                                     // interrupt okay now

        // switch to secure stack    
        mov     esi, gs:[0] PCB.pCurThd              // (esi) = pCurThread
        mov     edi, edx                        // save edx in edi
        mov     ecx, [esi].tlsSecure            // (ecx) = pCurThread->tlsSecure
        mov     [esi].tlsPtr, ecx               // pCurThread->tlsPtr = pCurThread->tlsSecure

        // update fs to pointer to secure stack
        call    UpdateRegistrationPtrWithTLS    // assembly function, only use ecx and edx

        // restore ebp, edx the rest will be restored later
        mov     edx, edi
        mov     ebp, [esp].regs[REG_OFST_EBP]

        // jump to common return handler
        jmp     short APICallReturn

    }
}

DWORD  __declspec (naked) MDCallUserHAPI (PHDATA phd, PDHCALL_STRUCT phc)
{
    _asm {
        mov  edx, esp               // edx = old sp
        sub  esp, size CALLSTACK    // esp = room for pcstk

        // save callee saved registers and exception chain
        mov  eax, fs:[0]
        mov  [esp].regs[REG_OFST_EBP], ebp
        mov  [esp].regs[REG_OFST_EBX], ebx
        mov  [esp].regs[REG_OFST_ESI], esi
        mov  [esp].regs[REG_OFST_EDI], edi
        mov  [esp].regs[REG_OFST_ESP], esp
        mov  [esp].regs[REG_OFST_EXCPLIST], eax

        mov  esi, esp               // (esi) = pcstk
        mov  ecx, [edx]             // ecx = return address
        mov  [esi].dwPrevSP, edx    // pcstk->dwPrevSP = original SP
        mov  [esi].retAddr, ecx     // pcstk->retAddr  = return address

        push esi                    // arg3 to SetupCallToUserServer == pcstk
        push dword ptr [edx+8]      // arg2 == phc
        push dword ptr [edx+4]      // arg1 == phd
        call SetupCallToUserServer

        
        // eax == function to call upon return
        mov  edi, [esi].dwNewSP     // (edi) = target SP

        test edi, edi
        
        // edi = target SP, eax = function to call
        jne  short CallUModeFunction

        // k-mode function, call directy (error, in this case)
        mov  esp, esi               // (esp) = pcstk
        call eax

        // jump to direct return
        // (edx):(eax) = return value
        mov  ebx, dword ptr [g_pKData]
        mov  ecx, [ebx].pAPIReturn
        jmp  ecx
    }
}

DWORD __declspec (naked) MDCallKernelHAPI (FARPROC pfnAPI, DWORD cArgs, LPVOID phObj, REGTYPE *pArgs)
{
    _asm {
        push ebp
        mov  ebp, esp

        mov  ecx, cArgs      // (ecx) = cArgs
        test ecx, ecx
        jz   short docall

        cld
        mov  eax, esi        // save esi in eax
        mov  edx, ecx        // (edx) = # of arguments
        mov  esi, pArgs      // (esi) = pArgs = source of movsd
        shl  edx, 2          // (edx) = total size of variable args
        sub  esp, edx        // (esp) = room for arguments on stack
        mov  edx, edi        // save edi in edx
        mov  edi, esp        // (edi) = destination of movsd

        rep  movsd           // copy the arguments

        // restore esi, edi
        mov  edi, edx
        mov  esi, eax

    docall:
        push phObj           // push the handle object
        call pfnAPI

        // eax already the return value.
        mov  esp, ebp
        pop  ebp
        ret
           
    }
}

// jump to pfn, with arguments pointed by pArgs
Naked MDSwitchToUserCode (FARPROC pfn, REGTYPE *pArgs)
{
    _asm {
        mov     eax, [esp+4]            // (eax) = pfn
        mov     edi, [esp+8]            // (edi) = pArgs == target SP
        jmp     short CallUModeFunction // jump to common code to call UMode function
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
        push    edi
        mov     eax, 12[ebp]    // (eax) = arg0
        mov     ecx, 16[ebp]    // (ecx) = arg1
        mov     edi, 20[ebp]    // (edi) = arg2
        cmp     byte ptr gs:[0] PCB.cNest, 1  // nested?
        mov     edx, 8[ebp]     // (edx) = function address
        jne     short kcl50     // Already in non-preemtible state
        int KCALL_INT           // trap to ring0 for non-preemtible stuff
        pop     edi
        pop     ebp             // restore original EBP
        ret

kcl50:  push    edi             // push Arg2
        push    ecx             // push Arg1
        push    eax             // push Arg0
        call    edx             // invoke function
        add     esp, 3*4        // remove args from stack
        pop     edi 
        pop     ebp             // restore original EBP
        ret
    }
}
#pragma warning(default:4035 4733)         // Turn warning back on




//------------------------------------------------------------------------------
// (edx) = funciton to call, eax, edi, and ecx are arguments to KCall
// NOTE: int22 is a Ring1 trap gate (i.e. no user mode access) thus
//       no sanitize registers here
//------------------------------------------------------------------------------
Naked 
Int22KCallHandler(void)
{
    __asm {
        push    eax                 // fake Error Code
        pushad
        push    ds
        push    es
        push    fs
        push    gs
        mov     esi, esp            // save ptr to register save area
        mov     ebx, gs:[0] PCB.pSelf
        dec     [ebx].cNest
        sti
        push    edi                 // push Arg2
        push    ecx                 // push Arg1
        push    eax                 // push Arg0
        call    edx                 // invoke non-preemtible function
        mov     [esi].TcxEax, eax   // save return value into PUSHAD frame
        cli

        cmp     gs:[0] PCB.ownspinlock, 0
        jne     DontReschedule

        // If in the debugger, don't even bother with rescheduling
        mov     eax, DWORD PTR [g_pKData]
        cmp     DWORD PTR [eax].dwInDebugger, 0
        ja      DontReschedule

        cmp     word ptr ([ebx].bResched), 1    // NOTE: comparing bResched && cNest at the same time (word ptr is the trick)
        je      CopyCtxAndReschedule

        cmp     dword ptr ([ebx].dwKCRes), 1
        je      CopyCtxAndReschedule

DontReschedule:
        mov     esp, esi
        inc     [ebx].cNest
        
        pop     gs
        pop     fs
        pop     es 
        pop     ds
        popad
        add     esp, 4              // throw away the error code
        iretd
        
CopyCtxAndReschedule:
        cld
        mov     edi, [ebx].pCurThd                  // (edi) = pCurThread
        add     edi, THREAD_CONTEXT_OFFSET          // (edi) = &pCurThread->ctx
        mov     ecx, size CPUCONTEXT / 4            // (ecx) = # of DWORDS
        rep     movsd                               // copy
        xor     edi, edi            // force Reschedule to reload the thread's state
        jmp     Reschedule
    }
}

Naked
Int24Reschedule (void)
{
    // (eax) = spinlock to release
    _asm {
        push    eax                 // fake Error Code
        pushad
        push    ds
        push    es
        push    fs
        push    gs
        mov     ebx, gs:[0] PCB.pSelf
        dec     [ebx].cNest

        // save thread context into thread structure
        mov     esi, esp                            // (esi) = save ptr to register save area
        mov     edi, [ebx].pCurThd                  // (edi) = pCurThread
        add     edi, THREAD_CONTEXT_OFFSET          // (edi) = &pCurThread->ctx
        mov     ecx, size CPUCONTEXT / 4            // (ecx) = # of DWORDS
        rep     movsd                               // copy

        // release scheduler spinlock
        mov     ecx, dword ptr [eax].next_owner
        mov     dword ptr [eax].owner_cpu, ecx      // psplock->owner_cpu = psplock->next_owner
        mov     dword ptr [ebx].ownspinlock, 0

        // reschedule        
        xor     edi, edi            // force Reschedule to reload the thread's state
        jmp     Reschedule
    }

}

void MDReleaseSpinlockAndReschedule (volatile SPINLOCK *psplock)
{
    DEBUGCHK (!InPrivilegeCall () && (1 == GetPCB ()->ownspinlock));
    _asm {
        mov     eax, psplock
        int     RESCHED_INT
    }
}


//------------------------------------------------------------------------------
// rdmsr - C wrapper for the assembly call. Must be in RING 0 to make this call!
// Assumes that the rdmsr instruction is supported on this CPU; you must check
// using the CPUID instruction before calling this function.
//------------------------------------------------------------------------------
 void rdmsr(
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
    if (InPrivilegeCall()) {
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
void wrmsr(
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
    if (InPrivilegeCall()) {
        wrmsr(dwAddr, dwValHigh, dwValLow);
    } else {
        KCall((PKFN)wrmsr, dwAddr, dwValHigh, dwValLow);
    }

    return TRUE;
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
Naked 
FPUNotPresentException(void)
{
    _asm {
        push    eax                 // Fake error code
        
        // We cannot be emulating FP if we arrive here. It is safe to not check
        // if CR0.EM is set.
        pushad
        push    ds
        push    es
        push    fs
        push    gs

        SANATIZE_SEG_REGS(eax, ax)
        
        clts

        mov     ebx, gs:[0] PCB.pSelf
        dec     [ebx].cNest         // count kernel reentrancy level
        mov     esi, esp            // (esi) = original stack pointer
        sti

        mov     eax, [ebx].pCurFPUOwner
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
        mov     eax, [ebx].pCurThd
        mov     [ebx].pCurFPUOwner, eax
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
        
        cmp     word ptr ([ebx].bResched), 1    // NOTE: comparing bResched && cNest at the same time (word ptr is the trick)
        je      short fpu_resched           // must reschedule now
        inc     [ebx].cNest                 // back out of kernel one level
        mov     esp, esi                    // restore stack pointer

        pop     gs
        pop     fs
        pop     es 
        pop     ds
        popad
        add     esp, 4                      // skip fake error code
        iretd
    
        // The reschedule flag was set and we are at the first nest level into the kernel
        // so we must reschedule now.

fpu_resched:
        mov     edi, [ebx].pCurThd                  // (edi) = ptr to current THREAD
        add     edi, THREAD_CONTEXT_OFFSET          // (edi) = &pCurThread->ctx
        mov     ecx, size CPUCONTEXT / 4            // (ecx) = # of DWORDS
        rep     movsd                               // copy
        mov     edi, [ebx].pCurThd                  // (edi) = pCurThread
        jmp     Reschedule
    }
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void DoFPUFlushContext (void) 
{
    PPCB ppcb         = GetPCB ();
    PTHREAD pFPUOwner = ppcb->pCurFPUOwner;
    FLOATING_SAVE_AREA *pFSave;

    DEBUGCHK (InPrivilegeCall () && pFPUOwner);


    _asm {
        // If we are emulating FP, g_CurFPUOwner is always 0 so we don't
        // have to test if CR0.EM is set(i.e. fnsave will not GP fault).
        clts
    }
    if (pFPUOwner->pSavedCtx) {
        pFSave = &pFPUOwner->pSavedCtx->FloatSave;
        _asm {
            mov eax, pFSave
            fnsave [eax]
        }
    }  else  {
        pFSave = PTH_TO_FLTSAVEAREAPTR(pFPUOwner);
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
    _asm {
        // force trap 7 on next FP instruction 
        mov     eax, CR0
        or      eax, TS_MASK
        mov     CR0, eax    
    }
    ppcb->pCurFPUOwner = NULL;
}

void MDFlushFPU (BOOL fPreempted)
{
    DEBUGCHK (InSysCall ());

    if (g_pKData->nCpus > 1) {
        if (GetPCB ()->pCurFPUOwner) {
            DEBUGCHK (pCurThread == GetPCB ()->pCurFPUOwner);
            if (InPrivilegeCall ()) {
                DoFPUFlushContext ();
            } else {
                KCall ((PKFN) DoFPUFlushContext);
            }
        }
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
FPUFlushContext(void) 
{
    if (!InPrivilegeCall ()) {
        KCall ((PKFN) FPUFlushContext);
    } else if (GetPCB ()->pCurFPUOwner) {
        DoFPUFlushContext ();
    }
}

void MDDiscardFPUContent (void)
{
    PPCB ppcb = GetPCB ();
    DEBUGCHK (InPrivilegeCall ());

    if (ppcb->pCurFPUOwner == ppcb->pCurThd) {
        ppcb->pCurFPUOwner = NULL;
        _asm {
            // force trap 7 on next FP instruction 
            clts
            mov     eax, CR0
            or      eax, TS_MASK
            mov     CR0, eax    
        }
    }
}

//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
Naked 
FPUException(void)
{
    _asm {
        push    eax                 // Fake error code
        pushad
        push    ds
        push    es
        push    fs
        push    gs
        xor     edx, edx            // EA = 0
        mov     eax, 16
        jmp     CommonFault
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
InitializeFPU(void)
{
    KCALLPROFON(69);    

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


#undef INTERRUPTS_OFF
#undef INTERRUPTS_ON

Naked INTERRUPTS_ON (void)
{
    __asm {
        sti
        ret
    }
}

Naked INTERRUPTS_OFF (void)
{
    __asm {
        cli
        ret
    }
}

BOOL __declspec(naked) INTERRUPTS_ENABLE (BOOL fEnable)
{
    __asm {
        mov     ecx, [esp+4]            // (ecx) = argument
        pushfd
        pop     eax                     // (eax) = current flags
        shr     eax, EFLAGS_IF_BIT      // 
        and     eax, 1                  // (eax) = 0 if interrupt was disabled, 1 if enabled
        
        test    ecx, ecx                // enable or disable?

        jne     short INTERRUPTS_ON

        // disable interrupt
        cli
        ret
    }
}


Naked HwTrap (LPVOID argument)
{
    #define DWORD_NOP \
        __asm nop \
        __asm nop \
        __asm nop \
        __asm nop \

    #define DWORD4_NOP \
        DWORD_NOP \
        DWORD_NOP \
        DWORD_NOP \
        DWORD_NOP \
        
    __asm {
        int 3
        ret
        nop
        nop
    }

    DWORD_NOP
    DWORD_NOP
    DWORD_NOP // 4 dwords

    DWORD4_NOP
    DWORD4_NOP
    DWORD4_NOP


    #undef DWORD_NOP
    #undef DWORD4_NOP
}


