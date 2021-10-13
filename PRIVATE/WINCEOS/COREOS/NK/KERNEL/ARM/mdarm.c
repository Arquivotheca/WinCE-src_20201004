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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#include "kernel.h"

const wchar_t NKCpuType [] = TEXT("ARM");
const wchar_t NKSignon[] = TEXT("Windows CE Kernel for ARM (Thumb Enabled)") TEXT(" Built on ") TEXT(__DATE__) TEXT(" at ") TEXT(__TIME__) TEXT("\r\n");

ulong GetCpuId(void);


// Fault Status register fields:
#define FSR_DOMAIN      0xF0
#define FSR_STATUS      0x0D
#define FSR_PAGE_ERROR  0x02

#define FSR_ALIGNMENT       0x01
#define FSR_TRANSLATION     0x05
#define FSR_DOMAIN_ERROR    0x09
#define FSR_PERMISSION      0x0D

#define OLD_BREAKPOINT  0x06000010          // special undefined instruction (obsolete)
#define BREAKPOINT      0x07F000F1          // special undefined instruction
#define THM_BREAKPOINT  0xDEFE              // Thumb equivalent

// friendly exception name. 1 based (0th entry is for exception id==1)
const LPCSTR g_ppszMDExcpId [MD_MAX_EXCP_ID+1] = {
    IDSTR_RESCHEDULE,                   // exception 0 - reschdule (invalid?)
    IDSTR_UNDEF_INSTR,                  // exception 1 - undefined instruction
    IDSTR_SWI_INSTR,                    // exception 2 - software interrupt (invalid?)
    IDSTR_PREFETCH_ABORT,               // exception 3 - prefetch abort
    IDSTR_DATA_ABORT,                   // exception 4 - data abort
    IDSTR_IRQ,                          // exception 5 - hardware interrupt (invalid?)
};

extern char InterlockedAPIs[], InterlockedEnd[];

PADDRMAP g_pOEMAddressTable;

//
// VFP stuffs (out-dated??)
//
#define VFP_ENABLE_BIT  0x40000000
#define VFP_EX_BIT      0x80000000

DWORD vfpStat = VFP_TESTING;
BOOL TestVFP (void);
DWORD ReadAndSetFpexc (DWORD dwNewFpExc);

// NOTE: Save/Restore FloatContext do not Save/Restore fpexc
//       it needs to be handled differently
void SaveFloatContext(PTHREAD);
void RestoreFloatContext(PTHREAD);


// flag to indicate if VFP is touched. NOTE: Flushing VFP is NOT considered
// touching VFP
BOOL g_fVFPTouched;

static void ARMV6InitTTBR (void)
{
    DWORD dwTTbr = GetPFN (g_ppdirNK) | g_pKData->dwTTBRCtrlBits;
    UpdateAsidAndTranslationBase (g_pprcNK->bASID, dwTTbr);
    SetupTTBR1 (dwTTbr);
}

BOOL MDValidateRomChain (ROMChain_t *pROMChain)
{
    PADDRMAP pAddrMap;
    DWORD dwEnd;

    for ( ; pROMChain; pROMChain = pROMChain->pNext) {
        for (pAddrMap = g_pOEMAddressTable; pAddrMap->dwSize; pAddrMap ++) {
            dwEnd = pAddrMap->dwVA + (pAddrMap->dwSize << 20);
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

void DetectVFP (void)
{
    DEBUGMSG (1, (L"Detecting VFP..."));
    vfpStat = VFP_TESTING | VFP_EXIST;
    TestVFP ();    // would generate an undefined-instruction exception if FPU not exist
                   // and ExceptionDispatch will unset the VFP_EXIST bit
    vfpStat &= ~VFP_TESTING;
    if (vfpStat)
        CEInstructionSet |= PROCESSOR_FEATURE_FP;
    DEBUGMSG (1, (L" VFP %s!\n", vfpStat? L"Found" : L"Not Found"));
}

BOOL IsVFPInstruction (DWORD dwInst)
{
    DWORD dwCoProc = (dwInst >> 8) & 0xf;
    return (VFP_EXIST == vfpStat) && ((dwCoProc == 10) || (dwCoProc == 11));
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL SwitchFPUOwner (PCONTEXT pctx, BOOL *fFirstAccess)
{
    BOOL fRet;
    KCALLPROFON(61);

    *fFirstAccess = !g_fVFPTouched;
    if (g_CurFPUOwner != pCurThread) {
        // DEBUGCHK (!g_fVFPTouched);
        if (g_CurFPUOwner) {
            SaveFloatContext(g_CurFPUOwner);
        }
        g_CurFPUOwner = pCurThread;
        RestoreFloatContext(pCurThread);
        fRet = (pCurThread->ctx.FpExc & VFP_EX_BIT);

        // restore the control registers
        g_pOemGlobal->pfnRestoreVFPCtrlRegs (pCurThread->ctx.FpExtra, NUM_EXTRA_CONTROL_REGS);

    } else {
        if (g_fVFPTouched) {
            // we hit an exception, save fpexc and contrl registers
            g_pOemGlobal->pfnSaveVFPCtrlRegs (pCurThread->ctx.FpExtra, NUM_EXTRA_CONTROL_REGS);
            pCurThread->ctx.FpExc = ReadAndSetFpexc (VFP_ENABLE_BIT);
            DEBUGCHK (pCurThread->ctx.FpExc & VFP_EX_BIT);

        } else {
            // we're preempted and switched back before anyone touching VFP
            // do nothing (resotre fpexc, which will be done on the next statement)
        }
        if (fRet = (pCurThread->ctx.FpExc & VFP_EX_BIT))
            SaveFloatContext(pCurThread);
    }

    g_fVFPTouched = TRUE;

    ReadAndSetFpexc (pCurThread->ctx.FpExc = (pCurThread->ctx.FpExc | VFP_ENABLE_BIT) & ~VFP_EX_BIT);

    KCALLPROFOFF(61);

    return fRet;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
FPUFlushContext(void)
{
    if (VFP_EXIST == vfpStat) {
        if (!InSysCall ())
            KCall ((FARPROC) FPUFlushContext);
        else if (g_CurFPUOwner) {

            // if VFP has been touched, we need to save the control
            // registers. Otherwise, the control registers have already been
            // saved and we CANNOT save it again.
            if (g_fVFPTouched) {
                DEBUGCHK (pCurThread == g_CurFPUOwner);
                g_pOemGlobal->pfnSaveVFPCtrlRegs (g_CurFPUOwner->ctx.FpExtra, NUM_EXTRA_CONTROL_REGS);
                g_CurFPUOwner->ctx.FpExc = ReadAndSetFpexc (VFP_ENABLE_BIT);
            }
            // save the general purpose registers
            SaveFloatContext(g_CurFPUOwner);

            // disable VFP so we'll reload the control registers
            // on the nexe VFP instruction (SaveFloatContext is destructive
            // to control registers).
            ReadAndSetFpexc (0);
            g_fVFPTouched = FALSE;
            g_CurFPUOwner = 0;
        }
    }
}

static void DumpCPUContext (const CPUCONTEXT *pCtx)
{
    NKDbgPrintfW(L" R0=%8.8lx  R1=%8.8lx  R2=%8.8lx  R3=%8.8lx\r\n",
            pCtx->R0, pCtx->R1, pCtx->R2, pCtx->R3);
    NKDbgPrintfW(L" R4=%8.8lx  R5=%8.8lx  R6=%8.8lx  R7=%8.8lx\r\n",
            pCtx->R4, pCtx->R5, pCtx->R6, pCtx->R7);
    NKDbgPrintfW(L" R8=%8.8lx  R9=%8.8lx R10=%8.8lx R11=%8.8lx\r\n",
            pCtx->R8, pCtx->R9, pCtx->R10, pCtx->R11);
    NKDbgPrintfW(L"R12=%8.8lx  SP=%8.8lx  Lr=%8.8lx Psr=%8.8lx\r\n",
            pCtx->R12, pCtx->Sp, pCtx->Lr, pCtx->Psr);
}

BOOL MDDumpThreadContext (PTHREAD pth, DWORD dwExcpId, DWORD dwAddr, DWORD dwInfo, DWORD dwLevel)
{
    NKDbgPrintfW(L"Exception '%a'(%d) Thread-Id=%8.8lx(pth=%8.8lx) PC=%8.8lx BVA=%8.8lx, dwInfo = %8.8lx\r\n",
            GetExceptionString (dwExcpId), dwExcpId, dwCurThId, pth, pth->ctx.Pc, dwAddr, dwInfo);
    DumpCPUContext (&pth->ctx);
    return TRUE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL MDDumpFrame (PTHREAD pth, DWORD dwExcpId, PEXCEPTION_RECORD pExr, PCONTEXT pCtx, DWORD dwLevel)
{
    NKDbgPrintfW(L"Exception '%a'(%d) Thread-Id=%8.8lx(pth=%8.8lx) PC=%8.8lx BVA=%8.8lx\r\n",
            GetExceptionString (dwExcpId), dwExcpId, dwCurThId, pth, pCtx->Pc, pExr->ExceptionInformation[1]);
    DumpCPUContext ((const CPUCONTEXT *)&pCtx->Psr);
    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PTHREAD
HandleException(
    PTHREAD pth,
    int id,
    ulong addr,
    ushort info
    )
{
    if (ID_RESCHEDULE != id) {

        KCALLPROFON(0);

        // encode THUMB state in PC
        if (pth->ctx.Psr & THUMB_STATE) {
            THRD_CTX_TO_PC (pth) |= 1;
        } else {
            THRD_CTX_TO_PC (pth) &= ~1;
        }

        if (!KC_CommonHandleException (pth, id, addr, info)) {
            // handle exception failed - reschedule
            id = ID_RESCHEDULE;
        }
        KCALLPROFOFF(0);

    }

    if (ID_RESCHEDULE == id) {

        do {
            if (ReschedFlag) {
                ReschedFlag = 0;
                NextThread();
            }
            if (KCResched) {
                KCResched = 0;
                KCNextThread();
            }

            if (!RunList.pth) {
                INTERRUPTS_OFF();
                if (!ReschedFlag && !KCResched) {
                    OEMIdle (g_pNKGlobal->dwNextReschedTime - g_pNKGlobal->dwCurMSec);
                    ReschedFlag = 1;
                }
                INTERRUPTS_ON();
            }
        } while (ReschedFlag || KCResched);

        // save the exception state of the thread being preempted if fpu has been touched.
        if (g_fVFPTouched) {
            // read and clear is guaranteed not to generate an exception
            g_pOemGlobal->pfnSaveVFPCtrlRegs (pCurThread->ctx.FpExtra, NUM_EXTRA_CONTROL_REGS);
            pCurThread->ctx.FpExc = ReadAndSetFpexc (0);
            g_fVFPTouched = FALSE;
        }

        pth = RunList.pth;
        SetCPUASID (pth);
        dwCurThId  = pth->dwId;
        pCurThread = pth;
        KPlpvTls   = pth->tlsPtr;

    }

    return pth;
}

DWORD GetTranslationBase (void);

//------------------------------------------------------------------------------
// LoadPageTable - load entries into page tables from kernel structures
//
//  LoadPageTable is called for prefetch and data aborts to copy page table
// entries from the kernel virtual memory tables to the ARM h/w page tables.
//------------------------------------------------------------------------------
BOOL LoadPageTable (ulong addr)
{
    // DO NOT USE VA2PDIDX, for it'll round to 4M boundary
    DWORD idxPD      = (addr >> 20);        // VA2PDIDX (addr);
    DWORD dwEntry    = g_ppdirNK->pte[idxPD];

    if ((addr >= VM_SHARED_HEAP_BASE)
        && (!IsV6OrLater () || !IsKModeAddr (addr))
        && dwEntry) {
        DWORD dwTranslationBase = GetTranslationBase();
        PPAGEDIRECTORY ppdir = (PPAGEDIRECTORY) ( ( g_pOemGlobal->dwTTBRCacheBits ) ? Pfn2Virt(dwTranslationBase) : Pfn2VirtUC (dwTranslationBase) );

        if (ppdir->pte[idxPD] != dwEntry) {
            ppdir->pte[idxPD] = dwEntry;
            ARMPteUpdateBarrier (&ppdir->pte[idxPD], 1);
            return TRUE;
        }
    }
    DEBUGMSG (ZONE_VIRTMEM, (L"LoadPageTable failed, addr = %8.8lx\r\n", addr));
    return FALSE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PVOID
Pfn2Virt(
    DWORD pfn
    )
{
    int i = 0;
    DWORD va;       // Virtual Base Address of section
    DWORD pa;       // Physical Base Address of section
    DWORD pau;      // Physical Address Upper Bound of section
    DWORD pfnmb;    // PFN rounded down to 1MB
    //
    // The end of the table is marked by an entry with a ZERO size.
    //
    while(g_pOEMAddressTable[i].dwSize) {
        va = g_pOEMAddressTable[i].dwVA & 0x1FF00000;
        pa = g_pOEMAddressTable[i].dwPA & 0xFFF00000;
        pau = pa + (g_pOEMAddressTable[i].dwSize << 20) - 1;
        pfnmb = pfn & 0xfff00000;
        if ((pfnmb >= pa) && (pfnmb <= pau))
            return ((PVOID) ((pfn - pa) + va + 0x80000000));
        i++;
    }
    DEBUGMSG(ZONE_PHYSMEM, (TEXT("Phys2Virt() : PFN (0x%08X) not found!\r\n"), pfn));
    return NULL;
}

void MDInitStack(LPBYTE lpStack, DWORD cbSize)
{
}

/* Machine dependent thread creation */
// normal thread stack: from top, TLS then PRETLS then args then free
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
MDSetupThread(
    PTHREAD pTh,
    LPVOID  lpBase,
    LPVOID  lpStart,
    BOOL    kmode,
    ulong   param
    )
{

    // Leave room for arguments and TLS and PRETLS on the stack
    pTh->ctx.Sp = (ulong)pTh->tlsPtr - SECURESTK_RESERVE;

    pTh->ctx.R0 = (ulong)lpStart;
    pTh->ctx.R1 = param;
    pTh->ctx.Lr = 4;

#ifdef DEBUG
    pTh->ctx.R4 = 0x4444;
    pTh->ctx.R5 = 0x5555;
    pTh->ctx.R6 = 0x6666;
    pTh->ctx.R7 = 0x7777;
    pTh->ctx.R8 = 0x8888;
    pTh->ctx.R9 = 0x9999;
    pTh->ctx.R10 = 0x1010;
    pTh->ctx.R11 = 0x1111;
#endif

    pTh->ctx.Pc = (ULONG)lpBase;
    if (kmode) {
        pTh->ctx.Psr = KERNEL_MODE;
        KTHRDINFO (pTh) |= UTLS_INKMODE;
    } else {
        pTh->ctx.Psr = USER_MODE;
        KTHRDINFO (pTh) &= ~UTLS_INKMODE;
    }
    
    // VFP state (init as "RunFast" mode)
    //
    pTh->ctx.Fpscr = (3 << 24);
    pTh->ctx.FpExc = 0;
    memset (pTh->ctx.S, 0, sizeof (pTh->ctx.S));
    memset (pTh->ctx.FpExtra, 0, sizeof (pTh->ctx.FpExtra));

    if ( (pTh->ctx.Pc & 0x01) != 0 ){
        pTh->ctx.Psr |= THUMB_STATE;
    }

    DEBUGMSG(ZONE_SCHEDULE, (L"MDCT: pTh=%8.8lx Pc=%8.8lx Psr=%4.4x GP=%8.8lx Sp=%8.8lx\r\n", pTh, pTh->ctx.Pc, pTh->ctx.Psr, pTh->ctx.R9, pTh->ctx.Sp));
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
DoThreadGetContext (
    PTHREAD pth,
    PCONTEXT lpContext
    )
{
    if (!KC_IsValidThread (pth)
        || (lpContext->ContextFlags & ~CONTEXT_FULL)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (pth && pth->pSavedCtx) {

        if (lpContext->ContextFlags & CONTEXT_CONTROL) {
            lpContext->Psr = pth->pSavedCtx->Psr;
            lpContext->Pc = pth->pSavedCtx->Pc;
            lpContext->Lr = pth->pSavedCtx->Lr;
            lpContext->Sp = pth->pSavedCtx->Sp;
        }
        if (lpContext->ContextFlags & CONTEXT_INTEGER) {
            lpContext->R0 = pth->pSavedCtx->R0;
            lpContext->R1 = pth->pSavedCtx->R1;
            lpContext->R2 = pth->pSavedCtx->R2;
            lpContext->R3 = pth->pSavedCtx->R3;
            lpContext->R4 = pth->pSavedCtx->R4;
            lpContext->R5 = pth->pSavedCtx->R5;
            lpContext->R6 = pth->pSavedCtx->R6;
            lpContext->R7 = pth->pSavedCtx->R7;
            lpContext->R8 = pth->pSavedCtx->R8;
            lpContext->R9 = pth->pSavedCtx->R9;
            lpContext->R10 = pth->pSavedCtx->R10;
            lpContext->R11 = pth->pSavedCtx->R11;
            lpContext->R12 = pth->pSavedCtx->R12;
        }
        if ((VFP_EXIST == vfpStat) && (lpContext->ContextFlags & CONTEXT_FLOATING_POINT)) {
            FPUFlushContext();
            lpContext->Fpscr = pth->pSavedCtx->Fpscr;
            lpContext->FpExc = pth->pSavedCtx->FpExc;
            memcpy (lpContext->S, pth->pSavedCtx->S, sizeof (lpContext->S));
        }

    } else {
        if (lpContext->ContextFlags & CONTEXT_CONTROL) {
            lpContext->Psr = pth->ctx.Psr;
            lpContext->Pc = pth->ctx.Pc;
            lpContext->Lr = pth->ctx.Lr;
            lpContext->Sp = pth->ctx.Sp;
        }
        if (lpContext->ContextFlags & CONTEXT_INTEGER) {
            lpContext->R0 = pth->ctx.R0;
            lpContext->R1 = pth->ctx.R1;
            lpContext->R2 = pth->ctx.R2;
            lpContext->R3 = pth->ctx.R3;
            lpContext->R4 = pth->ctx.R4;
            lpContext->R5 = pth->ctx.R5;
            lpContext->R6 = pth->ctx.R6;
            lpContext->R7 = pth->ctx.R7;
            lpContext->R8 = pth->ctx.R8;
            lpContext->R9 = pth->ctx.R9;
            lpContext->R10 = pth->ctx.R10;
            lpContext->R11 = pth->ctx.R11;
            lpContext->R12 = pth->ctx.R12;
        }
        if ((VFP_EXIST == vfpStat) && (lpContext->ContextFlags & CONTEXT_FLOATING_POINT)) {
            FPUFlushContext();
            lpContext->Fpscr = pth->ctx.Fpscr;
            lpContext->FpExc = pth->ctx.FpExc;
            memcpy (lpContext->S, pth->ctx.S, sizeof (lpContext->S));
            memcpy (lpContext->FpExtra, pth->ctx.FpExtra, sizeof (lpContext->FpExtra));
        }
    }
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
DoThreadSetContext(
    PTHREAD pth,
    const CONTEXT *lpContext
    )
{
    if (!KC_IsValidThread (pth)
        || (lpContext->ContextFlags & ~CONTEXT_FULL)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (pth && pth->pSavedCtx) {

        if (lpContext->ContextFlags & CONTEXT_CONTROL) {
            pth->pSavedCtx->Psr = (pth->pSavedCtx->Psr & 0xdf) | (lpContext->Psr & ~0xdf);
            pth->pSavedCtx->Pc = lpContext->Pc;
            pth->pSavedCtx->Lr = lpContext->Lr;
            pth->pSavedCtx->Sp = lpContext->Sp;
        }
        if (lpContext->ContextFlags & CONTEXT_INTEGER) {
            pth->pSavedCtx->R0 = lpContext->R0;
            pth->pSavedCtx->R1 = lpContext->R1;
            pth->pSavedCtx->R2 = lpContext->R2;
            pth->pSavedCtx->R3 = lpContext->R3;
            pth->pSavedCtx->R4 = lpContext->R4;
            pth->pSavedCtx->R5 = lpContext->R5;
            pth->pSavedCtx->R6 = lpContext->R6;
            pth->pSavedCtx->R7 = lpContext->R7;
            pth->pSavedCtx->R8 = lpContext->R8;
            pth->pSavedCtx->R9 = lpContext->R9;
            pth->pSavedCtx->R10 = lpContext->R10;
            pth->pSavedCtx->R11 = lpContext->R11;
            pth->pSavedCtx->R12 = lpContext->R12;
        }
        if ((VFP_EXIST == vfpStat) && (lpContext->ContextFlags & CONTEXT_FLOATING_POINT)) {
            FPUFlushContext();
            pth->pSavedCtx->Fpscr = lpContext->Fpscr;
            pth->pSavedCtx->FpExc = lpContext->FpExc;
            memcpy (pth->pSavedCtx->S, lpContext->S, sizeof (lpContext->S));
        }

    } else {
        if (lpContext->ContextFlags & CONTEXT_CONTROL) {
            pth->ctx.Psr = (pth->ctx.Psr & 0xdf) | (lpContext->Psr & ~0xdf);
            pth->ctx.Pc = lpContext->Pc;
            pth->ctx.Lr = lpContext->Lr;
            pth->ctx.Sp = lpContext->Sp;
        }
        if (lpContext->ContextFlags & CONTEXT_INTEGER) {
            pth->ctx.R0 = lpContext->R0;
            pth->ctx.R1 = lpContext->R1;
            pth->ctx.R2 = lpContext->R2;
            pth->ctx.R3 = lpContext->R3;
            pth->ctx.R4 = lpContext->R4;
            pth->ctx.R5 = lpContext->R5;
            pth->ctx.R6 = lpContext->R6;
            pth->ctx.R7 = lpContext->R7;
            pth->ctx.R8 = lpContext->R8;
            pth->ctx.R9 = lpContext->R9;
            pth->ctx.R10 = lpContext->R10;
            pth->ctx.R11 = lpContext->R11;
            pth->ctx.R12 = lpContext->R12;
        }
        if ((VFP_EXIST == vfpStat) && (lpContext->ContextFlags & CONTEXT_FLOATING_POINT)) {
            FPUFlushContext();
            pth->ctx.Fpscr = lpContext->Fpscr;
            pth->ctx.FpExc = lpContext->FpExc;
            memcpy (pth->ctx.S, lpContext->S, sizeof (lpContext->S));
            memcpy (pth->ctx.FpExtra, lpContext->FpExtra, sizeof (lpContext->FpExtra));
        }
    }
    return TRUE;
}

void MDRestoreCalleeSavedRegisters (PCALLSTACK pcstk, PCONTEXT pCtx)
{
    ULONG *pRegs = (ULONG *) pcstk->regs;

    pCtx->R4  = pRegs[0];
    pCtx->R5  = pRegs[1];
    pCtx->R6  = pRegs[2];
    pCtx->R7  = pRegs[3];
    pCtx->R8  = pRegs[4];
    pCtx->R9  = pRegs[5];
    pCtx->R10 = pRegs[6];
    pCtx->R11 = pRegs[7];
}

void MDClearVolatileRegs (PCONTEXT pCtx)
{
    pCtx->R0  =
    pCtx->R1  =
    pCtx->R2  =
    pCtx->R3  =
    pCtx->R12 = 0;
}

void MDSkipBreakPoint (PEXCEPTION_RECORD pExr, PCONTEXT pCtx)
{
    pCtx->Pc += ((pCtx->Psr & THUMB_STATE)? 2 : 4);
}

BOOL MDIsPageFault (DWORD dwExcpId)
{
    return (ID_DATA_ABORT == dwExcpId);
}


LPDWORD MDGetRaisedExceptionInfo (PEXCEPTION_RECORD pExr, PCONTEXT pCtx, PCALLSTACK pcstk)
{
    DWORD cArgs = CONTEXT_TO_PARAM_3 (pCtx);
    LPDWORD pArgs = (LPDWORD) CONTEXT_TO_PARAM_4 (pCtx);

    // RaiseException. The stack looks like this. The only difference between
    // a trusted/untrusted caller is that the stack is switched
    //
    //      |                       |
    //      |                       |
    //      -------------------------
    //      |                       |<---- pCtx
    //      |                       |
    //      |                       |
    //      -------------------------
    //      |                       |<---- pExr
    //      |                       |
    //      |                       |
    //      |                       |
    //      -------------------------
    //
    //

    pExr->ExceptionCode  = CONTEXT_TO_PARAM_1 (pCtx);
    pExr->ExceptionFlags = CONTEXT_TO_PARAM_2 (pCtx);

    if (cArgs && pArgs) {

        if (cArgs > EXCEPTION_MAXIMUM_PARAMETERS) {
            pExr->ExceptionCode = STATUS_INVALID_PARAMETER;
            pExr->ExceptionFlags = EXCEPTION_NONCONTINUABLE;

        } else {
            // copy arguments
            CeSafeCopyMemory (pExr->ExceptionInformation, pArgs, cArgs*sizeof(DWORD));
            pExr->NumberParameters = cArgs;
        }
    }

    return pArgs;
}

//
// argument id, addr, info are exactly the same as the value HandleException passed to KC_CommonHandleException
//
void MDSetupExcpInfo (PTHREAD pth, PCALLSTACK pcstk, DWORD id, DWORD addr, DWORD info)
{
    pcstk->regs[REG_R4] = id;       // R4: id
    pcstk->regs[REG_R5] = addr;     // R5: addr
    pcstk->regs[REG_R6] = info;     // R6: info

    if (GetThreadMode(pth) == USER_MODE)
        pth->ctx.Psr = (pth->ctx.Psr & ~0xFF) | SYSTEM_MODE;
    else
        pth->ctx.Psr &= ~THUMB_STATE;
}


static BOOL BCWorkAround (DWORD FaultAddr, PCONTEXT pCtx)
{
    BOOL fHandled = FALSE;
    // workaround WINCEMACRO used in CRT startup functions for TerminateProcess

    if (OLD_TERMINATE_PROCESS == FaultAddr) {
        // update PC to the new API set mapping
        pCtx->Pc += (FIRST_METHOD - OLD_FIRST_METHOD);
        fHandled = TRUE;
    }
    return fHandled;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL MDHandleHardwareException (PCALLSTACK pcstk, PEXCEPTION_RECORD pExr, PCONTEXT pCtx, LPDWORD pdwExcpId)
{
    PTHREAD   pth = pCurThread;
    DWORD      id = pcstk->regs[REG_R4];
    DWORD    addr = pcstk->regs[REG_R5];
    DWORD     fsr = pcstk->regs[REG_R6] & FSR_STATUS;    // status bits of fsr
    BOOL   fThumb = pCtx->Psr & THUMB_STATE;
    BOOL fInKMode = !(pcstk->dwPrcInfo & CST_MODE_FROM_USER);
    BOOL fHandled = FALSE;
    DWORD FaultAddr = (pCtx->Pc & ~0x01);

    DEBUGCHK (!fThumb || (pCtx->Pc & 0x01));

    *pdwExcpId = id;

    if ((ID_PREFETCH_ABORT == id)
        && BCWorkAround (FaultAddr, pCtx)) {
        DEBUGMSG (ZONE_SEH, (L"BC workaround, changing PC from %8.8lx to %8.8lx\r\n", FaultAddr, pCtx->Pc));
        return TRUE;
    }


    switch (id) {
    case ID_UNDEF_INSTR:

        if (VFP_TESTING & vfpStat) {
            // we get an exception testing if there is a VFP
            vfpStat = VFP_NOT_EXIST;
            pCtx->Pc += 4;  // skip over the fmrx (must be in ARM mode)
            pCtx->Psr &= ~THUMB_STATE;
            fHandled = TRUE;
            break;
        }
        pExr->ExceptionCode = STATUS_ILLEGAL_INSTRUCTION;
        if (fThumb){
            if (*(PUSHORT)FaultAddr == THM_BREAKPOINT) {
                pExr->ExceptionCode = STATUS_BREAKPOINT;
                pExr->ExceptionInformation[0] = *(PUSHORT) FaultAddr;
            }
            break;
        }

        if (((*(LPDWORD)FaultAddr & 0x0fffffff) == BREAKPOINT)
         || ((*(LPDWORD)FaultAddr & 0x0fffffff) == OLD_BREAKPOINT)) {
            pExr->ExceptionCode = STATUS_BREAKPOINT;
            pExr->ExceptionInformation[0] = *(LPDWORD)FaultAddr;
            break;
        }

        if (fThumb) {
            // VFP instruction not supported in thumb mode
            break;
        }

        if (IsVFPInstruction (*(LPDWORD)FaultAddr)) {
            BOOL fFirstAccess = FALSE, fExcept;
            if (!(fExcept = KCall((PKFN)SwitchFPUOwner, pCtx,  &fFirstAccess))
                && fFirstAccess) {
                // first time accessing FPU, and not in exception state
                fHandled = TRUE;
                break;
            }

            // if we're not in exception state, must be an previlege violation
            if (!fExcept) {
                break;
            }

            pCtx->Fpscr = pth->ctx.Fpscr;
            pCtx->FpExc = pth->ctx.FpExc;
            memcpy (pCtx->S, pth->ctx.S, sizeof (pCtx->S));
            memcpy (pCtx->FpExtra, pth->ctx.FpExtra, sizeof (pCtx->FpExtra));

            // floating point exception
            if (fHandled = g_pOemGlobal->pfnHandleVFPExcp (pExr, pCtx)) {
                // exception handled, restore thread context
                pth->ctx.Fpscr = pCtx->Fpscr;
                memcpy (pth->ctx.S, pCtx->S, sizeof (pCtx->S));
                memcpy (pth->ctx.FpExtra, pCtx->FpExtra, sizeof (pCtx->FpExtra));
                pth->ctx.FpExc = VFP_ENABLE_BIT;
                RestoreFloatContext (pth);
            }
            // EX bit is already cleared here.
        }
        break;

    case ID_DATA_ABORT:     // Data page fault

        if (FSR_ALIGNMENT == fsr) {
            pExr->ExceptionCode = STATUS_DATATYPE_MISALIGNMENT;
            break;
        }

        if (   (FSR_TRANSLATION  != fsr)
            && (FSR_DOMAIN_ERROR != fsr)
            && (FSR_PERMISSION   != fsr)) {
            pExr->ExceptionCode = 0xdfff0123;  
            break;
        }

        //
        // NOTE: Special case for data abort occurs in interlocked operations.
        //       When we get a Data Abort in the middle of an interlocked operation,
        //       the instruction pointer always gets moved back to the 1st instruction
        //       of the interlock function, which is a "load" instruciton. However,
        //       the code will always try to write to the location later. If we
        //       just checks the instruction at FaultAddr, we'll always page-in the page
        //       read-only and fault forever. Therefore, we special case it here to
        //       always requset write access when DataAbort occurs in the middle of
        //       of interlock operations.
        //
        if ((DWORD) (FaultAddr - (DWORD) g_pKData) < 0x400) {
            // instruction between 0xffffc800 and 0xffffcc00 - interlocked operations (or invalid)
            pExr->ExceptionInformation[0] = TRUE;

        //
        // Determine load vs. store depending on whether ARM or Thumb
        // mode:
        } else if (fThumb){
            ULONG Instr;
            Instr = *(PUSHORT)FaultAddr;
            // Discontinuity in the Thumb instruction set:
            //  LDRSB instruction does not have bit 11 set.
            //  All other load instructions have bit 11 set. The
            //  corresponding store instructions have bit 11 clear
            pExr->ExceptionInformation[0] = ((Instr & 0xFE00) == 0x5600)
                                            ? 0
                                            : !(Instr & (1<<11));
        } else {
            pExr->ExceptionInformation[0] = !((*(PULONG)FaultAddr) & (1<<20));
        }

        FaultAddr = addr;

        // fall through to handle page fault
        __fallthrough;

    case ID_PREFETCH_ABORT:     // Code page fault

        DEBUGMSG(ZONE_PAGING, (L"%a, addr = %8.8lx, fWrite = %8.8lx\r\n", GetExceptionString (id), FaultAddr, pExr->ExceptionInformation[0]));

        pExr->ExceptionInformation[1]   = FaultAddr;
        pExr->ExceptionCode             = STATUS_ACCESS_VIOLATION;
        pExr->NumberParameters          = 2;

        // pExr->ExceptionInformation[0] == read or write (1 for write)
        if (!InSysCall ()                               // faulted inside kcall - not much we can do
            && (fInKMode || !(FaultAddr & 0x80000000))  // faulted access kernel address while not in kmode
            && !IsInSharedHeap (FaultAddr)) {           // faulted while access shared heap - always fail
            fHandled = VMProcessPageFault (pVMProc, FaultAddr, pExr->ExceptionInformation[0]);
        }
        break;

    default:
        // unknown excepiton!!!
        pExr->ExceptionCode = STATUS_ILLEGAL_INSTRUCTION;
        DEBUGCHK (0);
    }

    return fHandled;
}

#ifdef DEBUG
int MDDbgPrintExceptionInfo (PEXCEPTION_RECORD pExr, PCALLSTACK pcstk)
{
    DEBUGMSG(ZONE_SEH || !IsInKVM ((DWORD) pcstk), (TEXT("ExceptionDispatch: pcstk=%8.8lx PC=%8.8lx id=%x\r\n"),
            pcstk, pcstk->retAddr, pcstk->regs[REG_R4]));
    return TRUE;
}
#endif

#define ARM_LOAD_VECTOR             0xe59ff3d8          // ldr     pc, [pc, #0x3E0-8]
#define ARM_DOMAIN_15               0x1e0               // bit 5-8 is domain

#define ARM_NUM_VECTORS             8                   // # of exception vectors

#define ARM_IDX_L1_CACHED_BASE      0x800               // idx for of page directory, for cached are (0x80000000)
#define ARM_IDX_L1_UNCACHED_BASE    0xA00               // idx for of page directory, for uncached are (0xa0000000)
#define ARM_IDX_L1_PSL              0xF10               // idx for of page directory, for PSL trapping addresses (0xf0000000)
#define ARM_IDX_L1_ARMHIGH          0xFFF               // idx for of page directory, for ArmHigh (0xfff00000)


extern const DWORD VectorTable [];
void SetupModeStacks (void);
void InitASID (void);


//
// ARMSetup - perform addition setup for CPU (vector table, 1st-level PT mappings, etc)
//
void ARMSetup (void)
{
    DWORD   dwEntry = g_ppdirNK->pte[ARM_IDX_L1_ARMHIGH];           // PD entry for ArmHigh
    LPDWORD pdwHighPT = (LPDWORD) Pfn2Virt (dwEntry & ~0x3ff);      // ptr to the second level PT for mapping of 0xFFF00000
                                                                    // NOTE: of size 1K, DO NOT USE PFNFromEntry
    LPDWORD pdwVectorPage = (LPDWORD) Pfn2Virt (PFNfromEntry (pdwHighPT[0xf0]));      // physical page at 0xffff0000
    DWORD   idx, dwCacheBits;

    // copy the 1st level PT of address from 0x80000000-0x9FFFFFFF, to 0xA0000000-0xBFFFFFFF
    memcpy (&g_ppdirNK->pte[ARM_IDX_L1_UNCACHED_BASE], &g_ppdirNK->pte[ARM_IDX_L1_CACHED_BASE], 0x800);

    // update g_ppdirNK to use 0xAxxxxxxx address
    g_ppdirNK = (PPAGEDIRECTORY) Pfn2VirtUC (PFNfromEntry (pdwHighPT[0xd0]));      // physical page at 0xfffd0000
    g_pprcNK->ppdir = g_ppdirNK;

    // initialize g_pNKGlobal->pdwCurrVectors. 0x100 == # of DWORDs in 1K
    g_pNKGlobal->pdwCurrVectors = (LPDWORD) ((DWORD)&pdwVectorPage[0x100 - ARM_NUM_VECTORS]|0xa0000000);

    // setup interrupt vectors
    for (idx = 0; idx < ARM_NUM_VECTORS; idx ++) {
        pdwVectorPage[idx] = ARM_LOAD_VECTOR;
        g_pNKGlobal->pdwCurrVectors[idx] = VectorTable[idx];
    }

    // Set up page table entry for PSL calls to turn the pre-fetch abort into a permission fault rather
    //  than a translation fault. This speeds up the time to execute a PSL call, as this entry can be
    //  cached in the TLB
    g_ppdirNK->pte[ARM_IDX_L1_PSL] = PD_SECTION | ARM_DOMAIN_15;     // maping of 0xf0000000 -> 0xf0100000

    // v6 or later - set non-executable bit, for domains might no longer be supported.
    if (g_pKData->dwArchitectureId >= ARMArchitectureV6) {
        g_ppdirNK->pte[ARM_IDX_L1_PSL] |= PG_V6_L1_NO_EXECUTE;
    }

    // update cache settings in ArmHigh
    pdwHighPT[0xf0] |= PG_CACHE;                      // page at 0xffff0000 is cacheable (vector table)
    pdwHighPT[0xfc] |= PG_CACHE;                      // page at 0xffffc000 is cacheable (KData page)

    // add a page of kernel stack
    dwEntry = pTOC->ulRAMFree + MemForPT;
    MemForPT += VM_PAGE_SIZE;
    pdwHighPT[0xfb] = GetPFN ((LPVOID)dwEntry) | PG_KRN_READ_WRITE;

    // update cache settings for entries in OEMAddressTable
    dwCacheBits = g_pOemGlobal->dwARMCacheMode;

    if (IsV6OrLater ()) {
        // TEX bits for 4K mappings is bit 6-8, while for 1M mappings is bit 12-14. We need to take 
        // it into account when updating 1M entries.
        dwCacheBits = (dwCacheBits & 0xC)               // C+B bits
                    | ((dwCacheBits & 0x1C0) << 6);     // TEX bits for 1M mappings (shift bit 6-8 left by 6)
    }

    // update cache settings for 0x80000000 -> 0x9FFFFFFF
    for (idx = ARM_IDX_L1_CACHED_BASE; idx < ARM_IDX_L1_UNCACHED_BASE; idx ++) {
        if (g_ppdirNK->pte[idx]) {
            g_ppdirNK->pte[idx] |= dwCacheBits;
        }
    }

    // Init ASID if supported
    InitASID ();
}

void KernelStart (void);

//
// NKStartup - entry point of kernel.dll.
//
// NK Loader setup only the minimal mappings, which includes ARMHigh area, and the cached static mapping area,
// with *EVERYTHING UNCACHED*. Interrupt vectors are not setup either. So, the init sequence reqiures:
// (1) pickup data passed from nk loader
// (2) Find entry point of oal, exchange globals, find out the cache mode.
// (3) fill in the rest of static mapped area (0xa0000000 - 0xbfffffff), PSL faulting address, interrupt vectors,
//     mod stacks, etc. Then, change the 'cached' static mapping area to use cache, and flush I&D TLB.
// (4) continue normal loading of kernel (find KITLdll, call OEMInitDebugSerial, etc.)
//
void NKStartup (struct KDataStruct * pKData)
{
    PFN_OEMInitGlobals pfnInitGlob;
    PFN_DllMain pfnKitlEntry;
    DWORD dwCpuId = GetCpuId ();

    // (1) pickup arguments from the nk loader
    g_pKData            = pKData;
    pTOC                = (const ROMHDR *) pKData->dwTOCAddr;
    g_pOEMAddressTable  = (PADDRMAP) pKData->pAddrMap;

    /* get architecture id and update page protection attributes */
    pKData->dwArchitectureId = (dwCpuId >> 16) & 0xf;
    if (pKData->dwArchitectureId >= ARMArchitectureV6) {
        // v6 or later
        pKData->dwProtMask = PG_V6_PROTECTION;
        pKData->dwRead     = PG_V6_PROT_READ;
        pKData->dwWrite    = PG_V6_PROT_WRITE;
        pKData->dwKrwUro   = PG_V6_PROT_URO_KRW;
        pKData->dwKrwUno   = PG_V6_PROT_UNO_KRW;

    } else {
        // pre-v6
        pKData->dwProtMask = PG_V4_PROTECTION;
        pKData->dwRead     = PG_V4_PROT_READ;
        pKData->dwWrite    = PG_V4_PROT_WRITE;
        pKData->dwKrwUro   = PG_V4_PROT_URO_KRW;
        pKData->dwKrwUno   = PG_V4_PROT_UNO_KRW;
    }

    // initialize nk globals
    FirstROM.pTOC       = (ROMHDR *) pTOC;
    FirstROM.pNext      = 0;
    ROMChain            = &FirstROM;
    KInfoTable[KINX_PTOC] = (long)pTOC;
    KInfoTable[KINX_PAGESIZE] = VM_PAGE_SIZE;

    g_ppdirNK = (PPAGEDIRECTORY) &ArmHigh->firstPT[0];
    pKData->pNk  = g_pNKGlobal;

    // (2) find entry of oal
    pfnInitGlob = (PFN_OEMInitGlobals) pKData->dwOEMInitGlobalsAddr;

    // no checking here, if OAL entry point doesn't exist, we can't continue
    g_pOemGlobal = pfnInitGlob (g_pNKGlobal);
    g_pOemGlobal->dwMainMemoryEndAddress = pTOC->ulRAMEnd;
    pKData->pOem = g_pOemGlobal;

    // setup globals
    pVMProc         = g_pprcNK;
    pActvProc       = g_pprcNK;
    g_pprcNK->vaFree = VM_NKVM_BASE;    

    // faked current thread id so ECS won't put 0 in OwnerThread
    dwCurThId = 2;

    g_pNKGlobal->pfnWriteDebugString = g_pOemGlobal->pfnWriteDebugString;

    // (3) setup vectors, UC mappings, mode stacks, etc.
    ARMSetup ();

    //
    // cache is enabled from here on
    //

    // (4) common startup code.

    // try to load KITL if exist
    if ((pfnKitlEntry = (PFN_DllMain) g_pOemGlobal->pfnKITLGlobalInit) ||
        (pfnKitlEntry = (PFN_DllMain) FindROMDllEntry (pTOC, KITLDLL))) {
        (* pfnKitlEntry) (NULL, DLL_PROCESS_ATTACH, (DWORD) NKKernelLibIoControl);
    }

#ifdef DEBUG
    CurMSec = dwPrevReschedTime = (DWORD) -200000;      // ~3 minutes before wrap
#endif

    OEMInitDebugSerial ();

    // debugchk only works after we have something to print to.
    DEBUGCHK (pKData == (struct KDataStruct *) PUserKData);
    DEBUGCHK (pKData == &ArmHigh->kdata);

    OEMWriteDebugString ((LPWSTR)NKSignon);

    /* Copy interlocked api code into the kpage */
    DEBUGCHK(sizeof(struct KDataStruct) <= FIRST_INTERLOCK);
    DEBUGCHK((InterlockedEnd-InterlockedAPIs)+FIRST_INTERLOCK <= 0x400);
    memcpy((char *)g_pKData+FIRST_INTERLOCK, InterlockedAPIs, InterlockedEnd-InterlockedAPIs);

    /* setup processor version information */
    CEProcessorType     = (dwCpuId >> 4) & 0xFFF;
    CEProcessorLevel    = 4;
    CEProcessorRevision = (WORD) dwCpuId & 0x0f;
    CEInstructionSet    = PROCESSOR_ARM_V4I_INSTRUCTION;

    RETAILMSG (1, (L"ProcessorType=%4.4x  Revision=%d CpuId=0x%08x\r\n", CEProcessorType, CEProcessorRevision, dwCpuId));
    RETAILMSG (1, (L"OEMAddressTable = %8.8lx\r\n", g_pOEMAddressTable));

    OEMInit();          // initialize firmware

    if (g_pOemGlobal->dwTTBRCacheBits) 
    {
        DWORD   dwEntry = g_ppdirNK->pte[ARM_IDX_L1_ARMHIGH];           // PD entry for ArmHigh    
        LPDWORD pdwHighPT = (LPDWORD) Pfn2VirtUC(dwEntry & ~0x3ff);     // ptr to the second level PT for mapping of 0xFFF00000
                                                                        // NOTE: of size 1K, DO NOT USE PFNFromEntry
        // update cache bits for page-table access (0xFFFD0000 - 0xFFFD3FFF)
        pdwHighPT[0xd0] |= PG_CACHE;
        pdwHighPT[0xd1] |= PG_CACHE;
        pdwHighPT[0xd2] |= PG_CACHE;
        pdwHighPT[0xd3] |= PG_CACHE;
        
        // update TTBR cache setting bits.
        g_pKData->dwTTBRCtrlBits |= g_pOemGlobal->dwTTBRCacheBits;

        // call ARMV6InitTTBR again to update the bits
        ARMV6InitTTBR ();

        // update g_ppdirNK to use 0x8xxxxxxx address
        g_ppdirNK = (PPAGEDIRECTORY) (((ULONG)g_ppdirNK)&(~0x20000000));
        g_pprcNK->ppdir = g_ppdirNK;
    }    

    OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_ALL);

    KernelFindMemory();

    DEBUGMSG (1, (TEXT("NKStartup done, starting up kernel.\r\n")));

    KernelStart ();

    // never returned
    DEBUGCHK (0);
}

