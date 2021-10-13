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

#include "kernel.h"


const wchar_t NKCpuType [] = TEXT("ARM");
const wchar_t NKSignon[] = TEXT("Windows CE Kernel for ARM (Thumb Enabled)") 
#ifdef DEBUG
                           TEXT(" Built on ") TEXT(__DATE__) TEXT(" at ") TEXT(__TIME__) 
#endif // DEBUG
                           TEXT("\r\n");
ulong GetCpuId(void);

static BOOL sg_fResumeFromSnapshot = FALSE;

// Fault Status register fields:
#define DFSR_STATUS         0x0D
#define DFSR_RW_BIT         0x800

#define FSR_ALIGNMENT       0x01
#define FSR_TRANSLATION     0x05
#define FSR_DOMAIN_ERROR    0x09
#define FSR_PERMISSION      0x0D
#define FSR_DEBUG_EVENT 0x02

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
    IDSTR_FPU_EXCEPTION,                // exception 6 - FPU exception
};

extern char InterlockedAPIs[], InterlockedEnd[];

#define Thumb2Is32BitInstr(instr)  ((((instr) & 0xFFFF) >> 11) > 0x1C)


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
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


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL InSysCall (void)
{
    return InPrivilegeCall () || PcbOwnSpinLock ();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void MDCheckInterlockOperation (PCONTEXT pCtx)
{
    DWORD dwPc = CONTEXT_TO_PROGRAM_COUNTER (pCtx);
    if (IsPCInIlockAddr (dwPc)      // in interlocked region
        && !(dwPc & 0x3)            // PC is DWORD aligned
        && pCtx->R3                 // R3 != 0
        && (pCtx->R3 != dwPc)) {    // R3 != PC
        // Interlocked operation in progress, reset PC to start of interlocked operation
        DEBUGMSG (1, (L"Interlocked operation in progress, restart (pc = 0x%x)\r\n", dwPc));
        CONTEXT_TO_PROGRAM_COUNTER (pCtx) = dwPc & ~0x1f;
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void MDSetupUserKPage (PPAGETABLE pptbl)
{
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void MDClearUserKPage (PPAGETABLE pptbl)
{
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void MDSpin (void)
{
    BOOL fWasEnable;
    DEBUGCHK (g_pKData->nCpus > 1);
    fWasEnable = INTERRUPTS_ENABLE (TRUE);      // turn interrupts on
    __emit (0xE320F002);                        // WFE
    INTERRUPTS_ENABLE (fWasEnable);             // restore interrupt state
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void MDSignal (void)
{
    if (g_pKData->nCpus > 1) {
        DataSyncBarrier ();
        __emit (0xE320F004);    // SEV
    }
}


//------------------------------------------------------------------------------
// VFP helpers
//------------------------------------------------------------------------------

DWORD ReadAndSetFpexc(DWORD dwNewFpExc);
void SaveFloatContext(PTHREAD pth, BOOL fPreempted, BOOL fExtendedVfp);
void RestoreFloatContext(PTHREAD pth, BOOL fExtendedVfp);


//------------------------------------------------------------------------------
//
// Returns true if the passed instruction encoding is under the VFP or Neon encoding space
// 
// NOTE: we assume that the high/low haldwords have already been switched for Thumb2
//
// NOTE: we're not very strict with the test for the VFP encoding space,
//   since we check the coproc field wich has meaning only for coproc instruction
//   (under the assuption that we'll only get an undefined instruction from a coproc
//   encoding space)
//
//------------------------------------------------------------------------------
static
BOOL IsVfpOrNeonInstruction (DWORD dwInst, BOOL fThumb)
{
    // The Thumb2 / ARM encodings for Neon instructions are sligtly different
    //
    if(fThumb) {
        return 
            (dwInst & 0x00000e00) == 0x00000a00 ||  // Coproc 10/11 instruction (VFP/Neon)
            (dwInst & 0xef000000) == 0xef000000 ||  // Neon data processing
            (dwInst & 0xff100000) == 0xf9000000;    // Neon element or structure load/store
    }
    else {
        return 
            (dwInst & 0x00000e00) == 0x00000a00 ||  // Coproc 10/11 instruction (VFP/Neon)
            (dwInst & 0xfe000000) == 0xf2000000 ||  // Neon data processing
            (dwInst & 0xff100000) == 0xf4000000;    // Neon element or structure load/store
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static
void VfpCpuContextToContext(CONTEXT *pCtx, const CPUCONTEXT *pCpuCtx)
{
    pCtx->Fpscr = pCpuCtx->Fpscr;
    pCtx->FpExc = pCpuCtx->FpExc;
    memcpy(pCtx->S, pCpuCtx->S, sizeof(pCpuCtx->S));
    memcpy(pCtx->FpExtra, pCpuCtx->FpExtra, sizeof(pCpuCtx->FpExtra));
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static
void VfpContextToCpuContext(CPUCONTEXT *pCpuCtx, const CONTEXT *pCtx)
{
    pCpuCtx->Fpscr = pCtx->Fpscr;
    pCpuCtx->FpExc = pCtx->FpExc;
    memcpy(pCpuCtx->S, pCtx->S, sizeof(pCtx->S));
    memcpy(pCpuCtx->FpExtra, pCtx->FpExtra, sizeof(pCtx->FpExtra));
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static
void VfpCoprocControl(DWORD dwCommand)
{
    if (NULL != g_pOemGlobal->pfnVFPCoprocControl) {
        g_pOemGlobal->pfnVFPCoprocControl(dwCommand);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void MDFlushFPU (BOOL fPreempted)
{
    PPCB ppcb = GetPCB ();
    PTHREAD pCurFPUOwner = ppcb->pCurFPUOwner;

    DEBUGCHK (InSysCall ());

    if (pCurFPUOwner) {
        
        DEBUGCHK (HaveVfpHardware());

        // clear FPU owner
        ppcb->pCurFPUOwner = NULL;

        // save control registers
        g_pOemGlobal->pfnSaveVFPCtrlRegs (pCurFPUOwner->ctx.FpExtra, NUM_EXTRA_CONTROL_REGS);

        // save fpexc, and clear FPEXC.EX bit if we're in the middle of an MP exception.
        pCurFPUOwner->ctx.FpExc = ReadAndSetFpexc (FPEXC_ENABLE_BIT);
        
        // save all VFP general purpose registers
        SaveFloatContext (pCurFPUOwner, fPreempted, HaveExtendedVfp());
        
        // disable VFP 
        ReadAndSetFpexc (0);
        VfpCoprocControl(VFP_CONTROL_POWER_RETENTION);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void MDDiscardFPUContent (void)
{
    PPCB ppcb = GetPCB ();
    
    DEBUGCHK (InPrivilegeCall ());

    if (ppcb->pCurFPUOwner) {
        DEBUGCHK (ppcb->pCurFPUOwner == pCurThread);
        ppcb->pCurFPUOwner = NULL;
        ReadAndSetFpexc (0);
        VfpCoprocControl(VFP_CONTROL_POWER_RETENTION);
    }
}


//------------------------------------------------------------------------------
//
// Try to make the current thread the owner of the VFP unit.Returns TRUE if the VFP
// switch is successful (no exceptional state)
//
// NOTE: This function must run atomically (KCall)
//
// NOTE: If the return value is FALSE (exceptional state), then *pOldFpExc contains
//   the old FpExc and all the VFP state is saved in the thread context. In this
//   case we'll give up the VFP ownership until we have dealt with the exception
//   (this avoids potential race conditions during exception processing)
//
//------------------------------------------------------------------------------
static
BOOL AcquireVfpOwnership(ULONG* pOldFpExc)
{
    BOOL fHandled = FALSE;
    PPCB ppcb = GetPCB ();
    PTHREAD pCurTh = ppcb->pCurThd;

    KCALLPROFON(61);

    DEBUGCHK(HaveVfpHardware());

    if (!ppcb->pCurFPUOwner) {

        // first time touching VFP since the thread is scheduled,
        // restore registers and enable VFP
        //
        ppcb->pCurFPUOwner = pCurTh;
        VfpCoprocControl(VFP_CONTROL_POWER_ON);
        RestoreFloatContext (pCurTh, HaveExtendedVfp());

        fHandled = !(pCurTh->ctx.FpExc & FPEXC_EX_BIT);
    }
    else {

        DEBUGCHK(ppcb->pCurFPUOwner == pCurTh);

        // we hit an 'real' VFP exception, save the VFP state
        // (note that this could be either a synch or an asynch VFP exception)
        //
        g_pOemGlobal->pfnSaveVFPCtrlRegs (pCurTh->ctx.FpExtra, NUM_EXTRA_CONTROL_REGS);
        pCurTh->ctx.FpExc = ReadAndSetFpexc (FPEXC_ENABLE_BIT);
        SaveFloatContext(pCurTh, TRUE, HaveExtendedVfp());
        fHandled = FALSE;
    }

    *pOldFpExc = pCurTh->ctx.FpExc;

    if (fHandled) {

        // if we acquired the VFP ownership successfully then
        // clear the FPEXC bits and enable VFP
        //
        pCurTh->ctx.FpExc = (pCurTh->ctx.FpExc | FPEXC_ENABLE_BIT) & ~FPEXC_CLEAR_BITS;
    }    
    else {

        // we're dealing with an exceptional state that requires
        // further handling, so give up the VFP ownership for now
        //
        ppcb->pCurFPUOwner = NULL;
        pCurTh->ctx.FpExc = 0;
    }
    
    ReadAndSetFpexc(pCurTh->ctx.FpExc);

    KCALLPROFOFF(61);

    return fHandled;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void FPUFlushContext(void)
{
    if (HaveVfpHardware()) {
        KCall ((PKFN) MDFlushFPU, TRUE);
    }
}


//------------------------------------------------------------------------------
// Capture the VFP context if VFP is available
//------------------------------------------------------------------------------
void MDCaptureFPUContext (PCONTEXT pCtx)
{
    if (HaveVfpSupport()) {
    
        FPUFlushContext();
        VfpCpuContextToContext(pCtx, &pCurThread->ctx);
        pCtx->ContextFlags |= CONTEXT_FLOATING_POINT;

        if(HaveExtendedVfp()) {
            pCtx->ContextFlags |= CONTEXT_EXTENDED_FLOAT;
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void DumpContext (const CONTEXT *pCtx)
{
    NKDbgPrintfW(L" R0=%8.8lx  R1=%8.8lx  R2=%8.8lx  R3=%8.8lx\r\n",
            pCtx->R0, pCtx->R1, pCtx->R2, pCtx->R3);
    NKDbgPrintfW(L" R4=%8.8lx  R5=%8.8lx  R6=%8.8lx  R7=%8.8lx\r\n",
            pCtx->R4, pCtx->R5, pCtx->R6, pCtx->R7);
    NKDbgPrintfW(L" R8=%8.8lx  R9=%8.8lx R10=%8.8lx R11=%8.8lx\r\n",
            pCtx->R8, pCtx->R9, pCtx->R10, pCtx->R11);
    NKDbgPrintfW(L"R12=%8.8lx  SP=%8.8lx  Lr=%8.8lx PC=%8.8lx, Psr=%8.8lx\r\n",
            pCtx->R12, pCtx->Sp, pCtx->Lr, pCtx->Pc, pCtx->Psr);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static 
void DumpCPUContext (const CPUCONTEXT *pCtx)
{
    NKDbgPrintfW(L" R0=%8.8lx  R1=%8.8lx  R2=%8.8lx  R3=%8.8lx\r\n",
            pCtx->R0, pCtx->R1, pCtx->R2, pCtx->R3);
    NKDbgPrintfW(L" R4=%8.8lx  R5=%8.8lx  R6=%8.8lx  R7=%8.8lx\r\n",
            pCtx->R4, pCtx->R5, pCtx->R6, pCtx->R7);
    NKDbgPrintfW(L" R8=%8.8lx  R9=%8.8lx R10=%8.8lx R11=%8.8lx\r\n",
            pCtx->R8, pCtx->R9, pCtx->R10, pCtx->R11);
    NKDbgPrintfW(L"R12=%8.8lx  SP=%8.8lx  Lr=%8.8lx PC=%8.8lx, Psr=%8.8lx\r\n",
            pCtx->R12, pCtx->Sp, pCtx->Lr, pCtx->Pc, pCtx->Psr);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
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
    DumpContext (pCtx);
    return TRUE;
}

extern void V6ClearExclusive (LPDWORD pdwDummy);

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
    PPCB ppcb = GetPCB ();

    if (ID_RESCHEDULE != id) {

        DEBUGCHK (pth);

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
            DEBUGCHK (0);
        }
        KCALLPROFOFF(0);

    }

    if ((ID_RESCHEDULE == id) && (!ppcb->ownspinlock)) {
        do {
            AcquireSchedulerLock (0);
            if (ppcb->bResched) {
                ppcb->bResched = 0;
                NextThread();
            }
            if (ppcb->dwKCRes) {
                ppcb->dwKCRes = 0;
                KCNextThread();
            }
            ReleaseSchedulerLock (0);

            if (!ppcb->pthSched) {
                INTERRUPTS_OFF();
                if (!ppcb->bResched && !ppcb->dwKCRes) {
                    NKIdle (g_pNKGlobal->dwNextReschedTime - g_pNKGlobal->dwCurMSec);
                    ppcb->bResched = 1;
                }
                INTERRUPTS_ON();
            }
        } while (ppcb->bResched || ppcb->dwKCRes);

        //
        // ARM TRM for v6+: need to run clrex on context switch
        //
        if (IsV6OrLater ()) {
            DWORD dummy;
            V6ClearExclusive (&dummy);
        }
        pth = ppcb->pthSched;
        SetCPUASID (pth);

    }

    DEBUGCHK (pth);

    return pth;
}

void MDInitStack(LPBYTE lpStack, DWORD cbSize)
{
}


//------------------------------------------------------------------------------
// Machine dependent thread creation
// normal thread stack: from top, TLS then PRETLS then args then free
//------------------------------------------------------------------------------
void
MDSetupThread(
    PTHREAD pTh,
    FARPROC lpBase,
    FARPROC lpStart,
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
    } else {
        pTh->ctx.Psr = USER_MODE;
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
SCHL_DoThreadGetContext (
    PTHREAD pth,
    PCONTEXT lpContext
    )
{
    BOOL fRet = TRUE;
    PCONTEXT pSavedCtx;
    DWORD ContextFlags = (lpContext->ContextFlags & ~CONTEXT_ARM);  // clear the CPU type, or the "&" operation will never be 0
    AcquireSchedulerLock (0);

    if (!KC_IsValidThread (pth) || (ContextFlags & ~(CONTEXT_FULL | CONTEXT_EXTENDED_FLOAT))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        fRet = FALSE;
    } else if (NULL != (pSavedCtx = pth->pSavedCtx)) {

        if (ContextFlags & CONTEXT_CONTROL) {
            lpContext->Sp  = pSavedCtx->Sp;
            lpContext->Lr  = pSavedCtx->Lr;
            lpContext->Pc  = pSavedCtx->Pc;
            lpContext->Psr = pSavedCtx->Psr;
        }
        
        if (ContextFlags & CONTEXT_INTEGER) {
            memcpy (&lpContext->R0, &pSavedCtx->R0, 13 * REGSIZE);  // R0-R12
        }
        
        if (ContextFlags & (CONTEXT_FLOATING_POINT | CONTEXT_EXTENDED_FLOAT)) {
        
            FPUFlushContext();
            lpContext->Fpscr = pSavedCtx->Fpscr;
            lpContext->FpExc = pSavedCtx->FpExc;

            if (ContextFlags & CONTEXT_EXTENDED_FLOAT) {
                memcpy(lpContext->S, pSavedCtx->S, NUM_VFP_REGS * REGSIZE);
            } else {
                memcpy(lpContext->S, pSavedCtx->S, NUM_VFP_REGS_COMMON * REGSIZE);
            }
        }

    } else {
    
        if (ContextFlags & CONTEXT_CONTROL) {
            lpContext->Sp  = pth->ctx.Sp;
            lpContext->Lr  = pth->ctx.Lr;
            lpContext->Pc  = pth->ctx.Pc;
            lpContext->Psr = pth->ctx.Psr;
        }
        
        if (ContextFlags & CONTEXT_INTEGER) {
            memcpy (&lpContext->R0, &pth->ctx.R0, 13 * REGSIZE);    // R0-R12
        }
        
        if (ContextFlags & (CONTEXT_FLOATING_POINT | CONTEXT_EXTENDED_FLOAT)) {
        
            FPUFlushContext();
            lpContext->Fpscr = pth->ctx.Fpscr;
            lpContext->FpExc = pth->ctx.FpExc;
            
            if (ContextFlags & CONTEXT_EXTENDED_FLOAT) {
                memcpy(lpContext->S, pth->ctx.S, NUM_VFP_REGS * REGSIZE);
            } else {
                memcpy(lpContext->S, pth->ctx.S, NUM_VFP_REGS_COMMON * REGSIZE);
            }
        }
    }
    ReleaseSchedulerLock (0);
    return fRet;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
SCHL_DoThreadSetContext(
    PTHREAD pth,
    const CONTEXT *lpContext
    )
{
    BOOL fRet = TRUE;
    PCONTEXT pSavedCtx;
    DWORD ContextFlags = (lpContext->ContextFlags & ~CONTEXT_ARM); // clear the CPU type, or the "&" operation will never be 0
    AcquireSchedulerLock (0);

    if (!KC_IsValidThread (pth) || (ContextFlags & ~(CONTEXT_FULL | CONTEXT_EXTENDED_FLOAT))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        fRet = FALSE;
        
    } else if (NULL != (pSavedCtx = pth->pSavedCtx)) {

        if (ContextFlags & CONTEXT_CONTROL) {
            pSavedCtx->Sp  = lpContext->Sp;
            pSavedCtx->Lr  = lpContext->Lr;
            pSavedCtx->Pc  = lpContext->Pc;
            pSavedCtx->Psr = (pSavedCtx->Psr & 0xdf) | (lpContext->Psr & ~0xdf);
        }
        
        if (ContextFlags & CONTEXT_INTEGER) {
            memcpy (&pSavedCtx->R0, &lpContext->R0, 13 * REGSIZE);                  // R0-R12
        }
        
        if (ContextFlags & (CONTEXT_FLOATING_POINT | CONTEXT_EXTENDED_FLOAT)) {
        
            FPUFlushContext();
            pSavedCtx->Fpscr = lpContext->Fpscr;
            pSavedCtx->FpExc = lpContext->FpExc;

            if (ContextFlags & CONTEXT_EXTENDED_FLOAT) {
                memcpy(pSavedCtx->S, lpContext->S, NUM_VFP_REGS * REGSIZE);
            } else {
                memcpy(pSavedCtx->S, lpContext->S, NUM_VFP_REGS_COMMON * REGSIZE);
            }
        }

    } else {
    
        if (ContextFlags & CONTEXT_CONTROL) {
            pth->ctx.Sp  = lpContext->Sp;
            pth->ctx.Lr  = lpContext->Lr;
            pth->ctx.Pc  = lpContext->Pc;
            pth->ctx.Psr = (pth->ctx.Psr & 0xdf) | (lpContext->Psr & ~0xdf);
        }
        
        if (ContextFlags & CONTEXT_INTEGER) {
            memcpy (&pth->ctx.R0, &lpContext->R0, 13 * REGSIZE);                    // R0-R12
        }
        
        if (ContextFlags & (CONTEXT_FLOATING_POINT | CONTEXT_EXTENDED_FLOAT)) {
        
            FPUFlushContext();
            pth->ctx.Fpscr = lpContext->Fpscr;
            pth->ctx.FpExc = lpContext->FpExc;

            if (ContextFlags & CONTEXT_EXTENDED_FLOAT) {
                memcpy(pth->ctx.S, lpContext->S, NUM_VFP_REGS * REGSIZE);
            } else {
                memcpy(pth->ctx.S, lpContext->S, NUM_VFP_REGS_COMMON * REGSIZE);
            }
        }
    }

    ReleaseSchedulerLock (0);
    return fRet;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void MDRestoreCalleeSavedRegisters (PCALLSTACK pcstk, PCONTEXT pCtx)
{
    const ULONG *pRegs = (ULONG *) pcstk->regs;

    pCtx->R4  = pRegs[0];
    pCtx->R5  = pRegs[1];
    pCtx->R6  = pRegs[2];
    pCtx->R7  = pRegs[3];
    pCtx->R8  = pRegs[4];
    pCtx->R9  = pRegs[5];
    pCtx->R10 = pRegs[6];
    pCtx->R11 = pRegs[7];
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void MDClearVolatileRegs (PCONTEXT pCtx)
{
    pCtx->R0  =
    pCtx->R1  =
    pCtx->R2  =
    pCtx->R3  =
    pCtx->R12 = 0;
}


//------------------------------------------------------------------------------
// Advance the Thumb-2 ITSTATE field in CPSR.  This is necessary when the
// program counter is advanced.
//------------------------------------------------------------------------------
static
void AdvanceItState (PCONTEXT pContext)
{
    union {
        DWORD dw;
        struct {
            DWORD unused0     : 10;
            DWORD itstate7_2  : 6;
            DWORD unused1     : 9;
            DWORD itstate1_0  : 2;
            DWORD unused2     : 5;
        } bits;
    } u;
    DWORD dwItstate;

    u.dw = pContext->Psr;
    dwItstate = (u.bits.itstate7_2 << 2) | u.bits.itstate1_0;

    if ((dwItstate & 7) == 0) {
        // No more predicate bits
        //
        dwItstate = 0;
    } else {
        // ITSTATE[4:0] <<= 1;
        //
        dwItstate = (dwItstate & 0xE0) | ((dwItstate << 1) & 0x1F);
    }

    u.bits.itstate7_2 = dwItstate >> 2;
    u.bits.itstate1_0 = dwItstate & 3;

    pContext->Psr = u.dw;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void MDSkipBreakPoint (PEXCEPTION_RECORD pExr, PCONTEXT pCtx)
{
    if (pCtx->Psr & THUMB_STATE) {
        pCtx->Pc += 2;
        AdvanceItState(pCtx);
    } else {
        pCtx->Pc += 4;
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL MDIsPageFault (DWORD dwExcpId)
{
    return (ID_DATA_ABORT == dwExcpId);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
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
            pExr->ExceptionCode = (DWORD) STATUS_INVALID_PARAMETER;
            pExr->ExceptionFlags = EXCEPTION_NONCONTINUABLE;

        } else {
            // copy arguments
            CeSafeCopyMemory (pExr->ExceptionInformation, pArgs, cArgs*sizeof(DWORD));
            pExr->NumberParameters = cArgs;
        }
    }

    return pArgs;
}


//------------------------------------------------------------------------------
//
// argument id, addr, info are exactly the same as the value HandleException passed to KC_CommonHandleException
//
//------------------------------------------------------------------------------
void MDSetupExcpInfo (PTHREAD pth, PCALLSTACK pcstk, DWORD id, DWORD addr, DWORD info)
{
    pcstk->regs[REG_R4] = id;           // R4: id
    pcstk->regs[REG_R5] = addr;         // R5: addr
    pcstk->regs[REG_R6] = info;         // R6: info
    pcstk->regs[REG_R7] = pth->ctx.Psr; // R7: original CPSR

    if (GetThreadMode(pth) == USER_MODE) {
        pth->ctx.Psr = SYSTEM_MODE;
    } else {
        pth->ctx.Psr &= ~EXEC_STATE_MASK;
    }
}

#define ATL_THUNK_SIZE      4
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL ATLWorkaroundNX (DWORD FaultAddr, PCONTEXT pCtx)
{
    BOOL fHandled = FALSE;
    DWORD atlThunk[ATL_THUNK_SIZE];

    // workaround ATL thunk problem.
    // - ATL thunk can be allocated on heap/stack, where it's not executable if NX is enabled.
    // - it has the exact pattern of 
    //      ldr r0, [pc]    ==> 0xE59F0000 (load r0 from [pc+8])
    //      ldr pc, [pc]    ==> 0xE59FF000 (load pc from [pc+8])
    //      <value for r0>
    //      <value for pc>
    // if we detect the above pattern, we'll assume that it's ATL thunk and emulate the execution.
    if (   IsValidUsrPtr ((LPCVOID)FaultAddr, ATL_THUNK_SIZE * sizeof(DWORD), FALSE)
        && IsDwordAligned (pCtx->Pc)) {
        // disable fault message/catch
        LPDWORD pTlsPtr = pCurThread->tlsSecure;
        
        DWORD dwOldTlsVal = pTlsPtr[TLSSLOT_KERNEL];
        pTlsPtr[TLSSLOT_KERNEL] |= TLSKERN_NOFAULT | TLSKERN_NOFAULTMSG;
        
        if (CeSafeCopyMemory (atlThunk, (LPCVOID) FaultAddr, sizeof (atlThunk))
            && (0xE59F0000 == atlThunk[0])
            && (0xE59FF000 == atlThunk[1])) {
            // emulate the ATL thunk
            pCtx->R0 = atlThunk[2];
            pCtx->Pc = atlThunk[3];
            fHandled = TRUE;
        }
        pTlsPtr[TLSSLOT_KERNEL] = dwOldTlsVal;
    }
    return fHandled;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL BCWorkAround (DWORD FaultAddr, PCONTEXT pCtx)
{
    BOOL fHandled = FALSE;

    // workaround WINCEMACRO used in CRT startup functions for TerminateProcess
    if (OLD_TERMINATE_PROCESS == FaultAddr) {
        // update PC to the new API set mapping
        pCtx->Pc = (pCtx->Pc + (FIRST_METHOD - OLD_FIRST_METHOD)) ^ g_pKData->dwPslTrapSeed;
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
    DWORD     fsr = pcstk->regs[REG_R6] & DFSR_STATUS;    // status bits of fsr
    BOOL   fThumb = pCtx->Psr & THUMB_STATE;
    BOOL fInKMode = !(pcstk->dwPrcInfo & CST_MODE_FROM_USER);
    BOOL fHandled = FALSE;
    DWORD FaultAddr = (pCtx->Pc & ~0x01);
    DWORD FaultInst = 0;
    // pcstk->regs[REG_R6] contains DFSR, which is only meaningful when it's data abort. And we
    // should only set fDebugEvent when it's data abort.
    BOOL fDebugEvent = (ID_DATA_ABORT == id) && ((pcstk->regs[REG_R6] & 0xf) == FSR_DEBUG_EVENT);

    DEBUGCHK (!fThumb || (pCtx->Pc & 0x01));

    pCtx->Psr = pcstk->regs[REG_R7]; // restore exception CPSR which was trashed for CaptureContext

    *pdwExcpId = id;

    if ((ID_PREFETCH_ABORT == id)
        && BCWorkAround (FaultAddr, pCtx)) {
        DEBUGMSG (ZONE_SEH, (L"BC workaround, changing PC from %8.8lx to %8.8lx\r\n", FaultAddr, pCtx->Pc));
        return TRUE;
    }

    switch (id) {
    case ID_UNDEF_INSTR:

        pExr->ExceptionCode = (DWORD) STATUS_ILLEGAL_INSTRUCTION;

        // in Thumb mode we use unaligned access since the 4bytes VFP
        // instructions may be unaligned; and we swap halfwords.
        // ARM accesses are always aligned
        //
        if (fThumb) {

            if (*(PUSHORT)FaultAddr == THM_BREAKPOINT) {

                pExr->ExceptionCode = (DWORD) STATUS_BREAKPOINT;
                pExr->ExceptionInformation[0] = *(PUSHORT) FaultAddr;
                break;
            }

            FaultInst = *(PUSHORT)FaultAddr;

            if (Thumb2Is32BitInstr(FaultInst)) {
                // Note that if this is a 32-bit instruction we load the bits
                // in the reversed halfword order, so that VFP decoding stuff
                // works correctly.
                // In fact, VFP instructions have the same encoding in ARM and
                // Thumb-2 modulo the halfword ordering.
                FaultInst = (FaultInst << 16) | (*(((PUSHORT)FaultAddr)+1));
            }
        } else {
            FaultInst = *(LPDWORD)FaultAddr;

            if (((FaultInst & 0x0fffffff) == BREAKPOINT) ||
                ((FaultInst & 0x0fffffff) == OLD_BREAKPOINT)) {
                
                pExr->ExceptionCode = (DWORD) STATUS_BREAKPOINT;
                pExr->ExceptionInformation[0] = FaultInst;
                break;
            }
        }

        // VFP is now supported both on ARM and Thumb-2
        //
        if (IsVfpOrNeonInstruction(FaultInst, fThumb)) {

            ULONG oldFpExc = 0;

            (fInKMode? pth->tlsSecure : pth->tlsNonSecure)[TLSSLOT_KERNEL] |= TLSKERN_FPU_USED;

            // if we have VFP hardware, try acquiring the VFP ownership first
            //
            if (HaveVfpHardware()) {
                if (KCall((PKFN)AcquireVfpOwnership, &oldFpExc)) {

                    // first time accessing FPU, and not in exception state
                    //
                    fHandled = TRUE;
                    break;
                }
            }

            VfpCpuContextToContext(pCtx, &pth->ctx);

            // try to handle the VFP exception
            //
            fHandled = g_pOemGlobal->pfnHandleVFPExcp(oldFpExc, pExr, pCtx, pdwExcpId, fInKMode);

            // if the exception was handled, restore thread context
            //
            if (fHandled) {

                pCtx->FpExc = FPEXC_ENABLE_BIT;
                VfpContextToCpuContext(&pth->ctx, pCtx);

                if (HaveVfpHardware()) {

                    // restore the VFP context
                    //
                    VERIFY(KCall((PKFN)AcquireVfpOwnership, &oldFpExc));
                }    
            }
            
            // EX bit is already cleared here.
            //
            break;
        }

        // If the debugger is loaded, attempt to validate this illegal
        // instruction by having the thread rerun the instruction.  If
        // it's illegal 2x, then it's probably a real illegal
        // instruction.
        //
        // If not, this was probably a stale breakpoint that went away.
        if (KDIgnoreIllegalInstruction (FaultAddr))
        {
            fHandled = TRUE;
            break;
        }
        break;

    case ID_DATA_ABORT:     // Data page fault
        if (FSR_ALIGNMENT == fsr) {
            pExr->ExceptionCode = (DWORD) STATUS_DATATYPE_MISALIGNMENT;
            pExr->ExceptionInformation[1] = addr;
            break;
        }

        if (   (FSR_TRANSLATION  != fsr)
            && (FSR_DOMAIN_ERROR != fsr)
            && (FSR_PERMISSION   != fsr)
            && !fDebugEvent) {
            // generic status - (DWORD) STATUS_EXTERNAL_ABORT
            pExr->ExceptionCode = 0xdfff0123;
            break;
        }

        if (IsV6OrLater()) {
            // pcstk->regs[REG_R6] contains the full DFSR, bit 11 of DFSR indicate whether
            // it's a read or write access.
            pExr->ExceptionInformation[0] = ((pcstk->regs[REG_R6] & DFSR_RW_BIT) != 0);
        }
        else {
            //
            // Determine load vs. store depending on whether ARM or Thumb
            // mode:
            if (fThumb){
                ULONG Instr;
                Instr = *(PUSHORT)FaultAddr;
                if (Thumb2Is32BitInstr(Instr)) {
                    pExr->ExceptionInformation[0] = ((Instr & 0x0010) == 0x0010) ? 0 : 1;
                } else {
                    // Discontinuity in the Thumb instruction set:
                    //  LDRSB instruction does not have bit 11 set.
                    //  All other load instructions have bit 11 set. The
                    //  corresponding store instructions have bit 11 clear
                    pExr->ExceptionInformation[0] = ((Instr & 0xFE00) == 0x5600)
                                                    ? 0
                                                    : !(Instr & (1<<11));
                }
            } else {
                pExr->ExceptionInformation[0] = !((*(PULONG)FaultAddr) & (1<<20));
            }
        }

        FaultAddr = addr;

        // fall through to handle page fault
        __fallthrough;

    case ID_PREFETCH_ABORT:     // Code page fault

        DEBUGMSG(ZONE_PAGING, (L"%a, addr = %8.8lx, fWrite = %8.8lx\r\n", GetExceptionString (id), FaultAddr, pExr->ExceptionInformation[0]));

        pExr->ExceptionInformation[1]   = FaultAddr;
        pExr->ExceptionCode             = (DWORD) STATUS_ACCESS_VIOLATION;
        pExr->NumberParameters          = 2;

        // pExr->ExceptionInformation[0] == read or write (1 for write)
        if (!InSysCall ()                               // faulted inside kcall - not much we can do
            && (fInKMode || !(FaultAddr & 0x80000000))  // faulted access kernel address while not in kmode
            && !IsInSharedHeap (FaultAddr)  // faulted while access shared heap - always fail
            && !fDebugEvent) {           
            fHandled = VMProcessPageFault (pVMProc, FaultAddr, pExr->ExceptionInformation[0], 0);
            if (fHandled && IsV6OrLater () && (ID_PREFETCH_ABORT == id)) {
                PPAGETABLE pptbl = GetPageTable (pVMProc->ppdir, VA2PDIDX (FaultAddr));
                if (pptbl) {
                    DWORD dwEntry = pptbl->pte[VA2PT2ND (FaultAddr)];
                    // page can be paged out while we're doing the check. If it's paged out,
                    // return handled such that we'll retry again.
                    fHandled = !IsPageCommitted (dwEntry)
                            || IsV6PageExecutable (dwEntry)
                            || ATLWorkaroundNX (FaultAddr, pCtx);
                    DEBUGMSG (!fHandled, (L"DEP: Executing on pages (%8.8lx) with NX bit set\r\n", FaultAddr));
                }
                // if faulting instruction in a prefetch is a 32bit one crossing a page,
                // then we have to page in also the following page to avoid an infinite loop
                if (   fThumb
                    && ((FaultAddr & 0xfff) == 0xffe)
                    && fHandled
                    && Thumb2Is32BitInstr(*(LPWORD)FaultAddr))
                {
                    fHandled = VMProcessPageFault (pVMProc, FaultAddr+4, pExr->ExceptionInformation[0], 0);
                }
            }
        }
        
        break;

    default:
        // unknown excepiton!!!
        pExr->ExceptionCode = (DWORD) STATUS_ILLEGAL_INSTRUCTION;
        DEBUGCHK (0);
    }

    return fHandled;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#ifdef DEBUG
int MDDbgPrintExceptionInfo (PEXCEPTION_RECORD pExr, PCALLSTACK pcstk)
{
    DEBUGMSG(ZONE_SEH || !IsInKVM ((DWORD) pcstk), (TEXT("ExceptionDispatch: pcstk=%8.8lx PC=%8.8lx id=%x\r\n"),
            pcstk, pcstk->retAddr, pcstk->regs[REG_R4]));
    return TRUE;
}
#endif


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define ARM_LOAD_VECTOR             0xe59ff3d8          // ldr     pc, [pc, #0x3E0-8]
#define ARM_DOMAIN_15               0x1e0               // bit 5-8 is domain

#define ARM_NUM_VECTORS             8                   // # of exception vectors

#define ARM_IDX_L1_PSL              0xF10               // idx for of page directory, for PSL trapping addresses (0xf0000000)
#define ARM_IDX_L1_ARMHIGH          0xFFF               // idx for of page directory, for ArmHigh (0xfff00000)

extern volatile LONG g_nCpuStopped;
extern volatile LONG g_nCpuReady;

extern volatile LONG g_fStartMP;


extern const DWORD VectorTable [];
void SetupModeStacks (void);
void InitASID (void);
void KernelStart (void);

void NKMpStart (void);
void SetPCBRegister (PPCB ppcb);

#define MP_PCB_BASE                 0xFFFD8000  // where PCB resides

#define FIQ_STACK_SIZE              0x100       // FIQ stack     256 bytes
#define IRQ_STACK_SIZE              0x100       // IRQ stack     256 bytes
#define ABORT_STACK_SIZE            0x400       // Abort stack   512 bytes
#define REGISTER_SAVE_AREA          0x020       // register save area 32 bytes
#define KSTACK_SIZE                 (0x1800 - FIQ_STACK_SIZE - IRQ_STACK_SIZE - ABORT_STACK_SIZE - REGISTER_SAVE_AREA)

typedef struct _APPage {
    BYTE FIQStack[FIQ_STACK_SIZE];
    BYTE IRQStack[IRQ_STACK_SIZE];
    BYTE AbortStack[ABORT_STACK_SIZE];
    BYTE KStack[KSTACK_SIZE];
    BYTE RegSaveArea[REGISTER_SAVE_AREA];       // MUST BE LAST, USED IN ASSEMBLY CODE
} APPage, *PAPPage;

#define ApPCBFromApPage(pApPage)        ((PPCB) ((PAPPage) (pApPage)+1))

ERRFALSE (sizeof (APPage) == 0x1800);
ERRFALSE (sizeof (PCB) <= 0x400);


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void InitAPPages (PAPPage pApPage, DWORD idx)
{
    PPCB ppcb = ApPCBFromApPage (pApPage);

    memset (ppcb, 0, sizeof (PCB));

    g_ppcbs[idx+1]  = ppcb;
    ppcb->dwCpuId   = idx+2;
    ppcb->pSelf     = ppcb;
    ppcb->pKStack   = &pApPage->KStack[KSTACK_SIZE];
    ppcb->pSavedContext = (LPBYTE)ppcb + 0x400;     // last 1k of the AP pages is unused. 
                                                    // use it to save context for debugger support
    ppcb->dwSyscallReturnTrap = SYSCALL_RETURN_RAW;
}

static void ARMV6InitTTBR (void)
{
    DWORD dwTTbr = GetPFN (g_ppdirNK) | g_pKData->dwTTBRCtrlBits;
    UpdateAsidAndTranslationBase (g_pprcNK->bASID, dwTTbr);
    SetupTTBR1 (dwTTbr);
}

void InitAllOtherCPUs (void)
{
    if (IsV6OrLater ()) {
        LONG   nCpus     = 1, idx;
        // page table to map the high address (0xfff00000 - 0xffffffff) resides in the same page as KData (see armstrt.s).
        // the address is at 0xffffcc00 (g_pKData + 0x400)
        LPDWORD pdwHighPT = (LPDWORD) ((DWORD) g_pKData + 0x400);       // ptr to the second level PT for mapping of 0xFFF00000
        DWORD   dwPFN;

        if (   g_pOemGlobal->fMPEnable                                          // MP enabled
            && g_pOemGlobal->pfnStartAllCpus (&nCpus, (FARPROC) NKMpStart)      // CPU successfully started
            && (nCpus > 1)) {                                                   // more than 1 CPUs
            DWORD   dwMpPCBBase = pTOC->ulRAMFree + MemForPT;

            DEBUGMSG (1, (L"MP Detected, # of CPUs = %8.8lx\r\n", nCpus));

            // setup Thread ID register for CPU 0 before update # of CPUs, or we can
            // fault accessing it.
            SetPCBRegister ((PPCB)g_pKData);

            g_pKData->nCpus = nCpus;

            InitPCBFunctions ();
            InitInterlockedFunctions ();

            nCpus --;       // don't count CPU 0.

            // each additional core got 2 pages for PCB, stacks, etc.
            MemForPT += nCpus * 2 * VM_PAGE_SIZE;

            // map MP pages from MP_PCB_BASE (0xFFFD8000)
            for (idx = 0; idx < nCpus; idx ++) {
                dwPFN = GetPFN ((LPVOID)(dwMpPCBBase + ((idx * 2) * VM_PAGE_SIZE)));
                pdwHighPT[0xD8+(2*idx)]   = dwPFN                | PG_KRN_READ_WRITE | PG_V6_SHRAED;
                pdwHighPT[0xD8+(2*idx)+1] = dwPFN + VM_PAGE_SIZE | PG_KRW_URO        | PG_V6_SHRAED;
            }

            OEMCacheRangeFlush (pdwHighPT, 0x400, CACHE_SYNC_DISCARD|CACHE_SYNC_L2_DISCARD);

            // initialize AP Pages for all the cores
            for (idx = 0; idx < nCpus; idx ++) {
                InitAPPages ((PAPPage) (MP_PCB_BASE + idx * VM_PAGE_SIZE * 2), idx);
            }
        } else {
            PPAGETABLE pptbl;
            PADDRMAP pAddrMap;
            
            // Single core, clear "S" bit for both L1 and KData area - perf impact if we don't clear it.
            g_pKData->dwPTExtraBits  &= ~PG_V6_SHRAED;
            g_pOemGlobal->dwPageTableCacheBits &= ~PG_V6_SHRAED; // don't set share-bit for page table access either
            pdwHighPT[0xD4] &= ~PG_V6_SHRAED;      // clear share bit for page at 0xfffd4000 (page table array)
            pdwHighPT[0xf0] &= ~PG_V6_SHRAED;      // clear share bit for page at 0xffff0000 is cacheable (vector table)
            pdwHighPT[0xfb] &= ~PG_V6_SHRAED;      // clear share bit for page at 0xffffb000 is cacheable (extra page of kstack)
            pdwHighPT[0xfc] &= ~PG_V6_SHRAED;      // clear share bit for page at 0xffffc000 is cacheable (KData page)

            // clear the share bit in OEMAddressTable
            for (pAddrMap = g_pOEMAddressTable; pAddrMap->dwSize; pAddrMap ++ ) {
                LONG idxEnd;
                idx    = (pAddrMap->dwVA >> VM_SECTION_SHIFT);
                idxEnd = idx + pAddrMap->dwSize; // size is in MB
                for (; idx < idxEnd; idx ++) {
                    g_ppdirNK->pte[idx] &= ~PG_V6_L1_SHARED;
                }
            }

            // clear the share bit in the page table access
            pptbl = GetPageTable (g_ppdirNK, VA2PDIDX (g_pKData->dwKVMStart));
            DEBUGCHK (pptbl);
            pptbl = GetPageTable (g_ppdirNK, VA2PDIDX (pptbl));
            DEBUGCHK (pptbl);

            for (idx = 0; idx < VM_NUM_PT_ENTRIES; idx ++) {
                if (IsPageCommitted (pptbl->pte[idx])) {
                    pptbl->pte[idx] &= ~PG_V6_SHRAED;
                }
            }

            // clear the "S" bit in TTBR0/TTBR1
            g_pKData->dwTTBRCtrlBits &= ~TTBR_CTRL_S_BIT;
            ARMV6InitTTBR ();
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void MpStartContinue (PPCB ppcb)
{
    DataMemoryBarrier();
    ppcb->pVMPrc      = ppcb->pCurPrc = g_pprcNK;
    ppcb->pCurThd     = &dummyThread;
    ppcb->CurThId     = DUMMY_THREAD_ID; 
    ppcb->ActvProcId  = g_pprcNK->dwId;
    ppcb->fIdle       = FALSE;
    ppcb->dwCpuState  = sg_fResumeFromSnapshot? CE_PROCESSOR_STATE_POWERED_OFF : CE_PROCESSOR_STATE_POWERED_ON;
    ppcb->OwnerProcId = g_pprcNK->dwId;

    ARMV6InitTTBR ();

    // call per CPU initialization function and get the hardware CPU id
    ppcb->dwHardwareCPUId = g_pOemGlobal->pfnMpPerCPUInit ();

    ppcb->dwPrevTimeModeTime = ppcb->dwPrevReschedTime = GETCURRTICK ();
    SetReschedule (ppcb);

    DEBUGMSG (1, (L"CPU %d started, ready to reschedule, ppcb = %8.8lx\r\n", ppcb->dwCpuId, ppcb));
    
    InterlockedIncrement (&g_nCpuReady);

    while (g_nCpuStopped) {
        DataMemoryBarrier();
    }

}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void StartAllCPUs (void)
{
    if (g_pKData->nCpus > 1) {

        DEBUGMSG (1, (L"Resuming all CPUs, g_pKData->nCpus = %d\r\n", g_pKData->nCpus));

        INTERRUPTS_OFF ();
        g_fStartMP = TRUE;

        while (g_pKData->nCpus != (DWORD) g_nCpuReady) {
            DataMemoryBarrier();
        }
        if(sg_fResumeFromSnapshot){
            g_nCpuReady = 1;
        }
        g_nCpuStopped = 0;
        INTERRUPTS_ON ();
        KCall ((PKFN) OEMCacheRangeFlush, 0, 0, CACHE_SYNC_DISCARD|CACHE_SYNC_FLUSH_TLB);
        DEBUGMSG (1, (L"All CPUs resumed, g_nCpuReady = %d\r\n", g_nCpuReady));
    } else {
        g_nCpuStopped = 0;
    }
}

void ReserveVMForKernelPageTables (void);
DWORD CalcMaxDeviceMapping (void);
void  MapDeviceTable (void);
void ARMFlushTLB (void);

//------------------------------------------------------------------------------
//
// ARMSetup - perform addition setup for CPU (vector table, 1st-level PT mappings, etc)
//
//------------------------------------------------------------------------------
void ARMSetup (void)
{
    // page table to map the high address (0xfff00000 - 0xffffffff) resides in the same page as KData (see armstrt.s).
    // the address is at 0xffffcc00 (g_pKData + 0x400)
    LPDWORD pdwHighPT = (LPDWORD) (0xffffcc00);       // ptr to the second level PT for mapping of 0xFFF00000
    PPAGETABLE  pptbl = GrabOnePage (PM_PT_ZEROED);
    PADDRMAP pAddrMap;
    LPDWORD pdwVectorPage;
    DWORD   dwCacheBits, idx;

    g_pprcNK->ppdir = g_ppdirNK;

    // map a page at 0xfffd4000 for access to kernel page table (will be setup in ReserveVMForKernelPageTables)
    pptbl->pte[0x3ff] = 0xffffc000;
    pdwHighPT[0xD4] = GetPFN (pptbl) | PG_KRN_READ_WRITE;     // NOTE: this page is cacheable 

    pdwVectorPage = (LPDWORD) Pfn2Virt (PFNfromEntry (pdwHighPT[0xf0]));      // physical page at 0xffff0000
    // initialize g_pNKGlobal->pdwCurrVectors. 0x100 == # of DWORDs in 1K
    g_pNKGlobal->pdwCurrVectors = (LPDWORD) ((DWORD)&pdwVectorPage[0x100 - ARM_NUM_VECTORS]);

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
        pdwHighPT[0xD4] |= PG_V6_L2_NO_EXECUTE; // mapping to page tables are not executable
    }

    /* Copy interlocked api code into the kpage */
    DEBUGCHK(sizeof(struct KDataStruct) <= FIRST_INTERLOCK);
    DEBUGCHK((InterlockedEnd-InterlockedAPIs) <= ILOCK_REGION_SIZE);
    memcpy((char *)g_pKData+FIRST_INTERLOCK, InterlockedAPIs, InterlockedEnd-InterlockedAPIs);

    // reserve VM for kernel page tables
    ReserveVMForKernelPageTables ();

    // map OEMDeviceTable
    MapDeviceTable ();

    // update cache bits for page-table access (0xFFFD0000 - 0xFFFD3FFF)
    dwCacheBits = g_pOemGlobal->dwPageTableCacheBits;
    pdwHighPT[0xD0] |= dwCacheBits;
    pdwHighPT[0xD1] |= dwCacheBits;
    pdwHighPT[0xD2] |= dwCacheBits;
    pdwHighPT[0xD3] |= dwCacheBits;

    // update cache settings for entries in OEMAddressTable
    dwCacheBits = g_pOemGlobal->dwARMCacheMode;

    if (IsV6OrLater ()) {
        // TEX bits for 4K mappings is bit 6-8, while for 1M mappings is bit 12-14. We need to take 
        // it into account when updating 1M entries.
        dwCacheBits = (dwCacheBits & 0xC)               // C+B bits
                    | ((dwCacheBits & 0x1C0) << 6)      // TEX bits for 1M mappings (shift bit 6-8 left by 6)
                    | PG_V6_L1_SHARED;                  // shared
    }

    for (pAddrMap = g_pOEMAddressTable; pAddrMap->dwSize; pAddrMap ++ ) {
        DWORD idxEnd;
        idx    = (pAddrMap->dwVA >> VM_SECTION_SHIFT);
        idxEnd = idx + pAddrMap->dwSize; // size is in MB
        for (; idx < idxEnd; idx ++) {
            g_ppdirNK->pte[idx] |= dwCacheBits;
        }
    }

    // add a page of kernel stack
    dwCacheBits = PG_CACHE;
    pdwHighPT[0xfb] = GetPFN (GrabOnePage (PM_PT_ZEROED)) | PG_VALID_MASK | PG_PROT_UNO_KRW | dwCacheBits;

    // update cache settings in ArmHigh
    pdwHighPT[0xf0] |= dwCacheBits;         // page at 0xffff0000 is cacheable (vector table)
    pdwHighPT[0xfc] |= dwCacheBits;         // page at 0xffffc000 is cacheable (KData page)

    // Init ASID if supported
    InitASID ();

    // flush TLB
    ARMFlushTLB ();
}

#undef g_pKData
extern struct KDataStruct *g_pKData;

static CPUCONTEXT ctxMainCpu;
void UpdatePageTableProtectionBits (void);

//------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------
void NKStartup (struct KDataStruct * pKData)
{
    PFN_OEMInitGlobals pfnInitGlob;
    PFN_DllMain pfnKitlEntry;
    DWORD dwCpuCap = GetCpuId ();
    PPCB  ppcb;

    // (1) pickup arguments from the nk loader
    g_pKData            = pKData;
    pTOC                = (const ROMHDR *) pKData->dwTOCAddr;
    g_pOEMAddressTable  = (PADDRMAP) pKData->pAddrMap;

    ppcb                = (PPCB) pKData;
    ppcb->pKStack       = (LPBYTE) pKData - REGISTER_SAVE_AREA;  // room for temporary register saved area
    ppcb->CurThId       = 2;        // faked current thread id so ECS won't put 0 in OwnerThread
    ppcb->dwCpuId       = 1;
    ppcb->dwCpuState    = CE_PROCESSOR_STATE_POWERED_ON;
    ppcb->pSavedContext = &ctxMainCpu;
    ppcb->dwSyscallReturnTrap = SYSCALL_RETURN_RAW;
    pKData->nCpus       = 1;

    /* get architecture id and update page protection attributes */
    pKData->dwArchitectureId = (dwCpuCap >> 16) & 0xf;
    if (pKData->dwArchitectureId >= ARMArchitectureV6) {
        // v6 or later
        pKData->dwProtMask = PG_V6_PROTECTION;
        pKData->dwRead     = PG_V6_PROT_READ;
        pKData->dwWrite    = PG_V6_PROT_WRITE;
        pKData->dwKrwUro   = PG_V6_PROT_URO_KRW;
        pKData->dwKrwUno   = PG_V6_PROT_UNO_KRW;

        // default set S bit. We'll clear it if we found that there is only one CPU.
        pKData->dwPTExtraBits  = PG_V6_SHRAED;
        pKData->dwTTBRCtrlBits = TTBR_CTRL_S_BIT;

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
    ppcb->pVMPrc = ppcb->pCurPrc = g_pprcNK;
    g_pKData->dwKVMStart = g_pprcNK->vaFree = CalcMaxDeviceMapping ();
    g_ppcbs[0] = ppcb;

    g_pNKGlobal->pfnWriteDebugString = g_pOemGlobal->pfnWriteDebugString;

    // (3) setup vectors, UC mappings, mode stacks, etc.
    ARMSetup ();

    //
    // cache is enabled from here on
    //

    // (4) common startup code.

    // try to load KITL if exist
    pfnKitlEntry = (PFN_DllMain) g_pOemGlobal->pfnKITLGlobalInit;
    if (!pfnKitlEntry) {
        pfnKitlEntry = (PFN_DllMain) FindROMDllEntry (pTOC, KITLDLL);
    }
    if (pfnKitlEntry) {
        (* pfnKitlEntry) (NULL, DLL_PROCESS_ATTACH, (DWORD) NKKernelLibIoControl);
    }

#ifdef DEBUG
    dwLastKCTime = CurMSec = ppcb->dwPrevTimeModeTime = ppcb->dwPrevReschedTime = (DWORD) -200000;      // ~3 minutes before wrap
#endif

    OEMInitDebugSerial ();

    // debugchk only works after we have something to print to.
    DEBUGCHK (pKData == (struct KDataStruct *) PUserKData);
    DEBUGCHK (pKData == &ArmHigh->kdata);

    OEMWriteDebugString ((LPWSTR)NKSignon);

    /* setup processor version information */
    CEProcessorType     = (dwCpuCap >> 4) & 0xFFF;
    CEProcessorRevision = (WORD) dwCpuCap & 0x0f;
    
#if defined(ARMV5)
    CEProcessorLevel    = 5;
    CEInstructionSet    = PROCESSOR_ARM_V5_INSTRUCTION;
#elif defined(ARMV6)
    CEProcessorLevel    = 6;
    CEInstructionSet    = PROCESSOR_ARM_V6_INSTRUCTION;
#elif defined(ARMV7)
    CEProcessorLevel    = 7;
    CEInstructionSet    = PROCESSOR_ARM_V7_INSTRUCTION;
#else  //B.C. Issue.  Tools need to compile for ARMv4I
    CEProcessorLevel    = 4;
    CEInstructionSet    = PROCESSOR_ARM_V4I_INSTRUCTION;
#endif

    RETAILMSG (1, (L"ProcessorType=%4.4x  Revision=%d CpuId=0x%08x\r\n", CEProcessorType, CEProcessorRevision, dwCpuCap));
    RETAILMSG (1, (L"OEMAddressTable = %8.8lx\r\n", g_pOEMAddressTable));

    OEMInit();          // initialize firmware

    // update the instruction set flags
    //
    if (HaveVfpHardware()) {
        CEInstructionSet |= PROCESSOR_FEATURE_FP;
    }

    // make page tables L2 cacheable. 
    // the only page table we need to worry about is kernel's master page table, which would
    // have some pages committed. Need to update the page table access bit to be L2 cacheable.
    if (IsV6OrLater ()) {

        DWORD idx;

        if (g_pOemGlobal->dwPageTableCacheBits) {
            PPAGETABLE pptbl = GetPageTable (g_ppdirNK, VA2PDIDX (g_pKData->dwKVMStart));
            DWORD      dwPTCacheBits = g_pOemGlobal->dwPageTableCacheBits;

            DEBUGCHK (pptbl);
            
            UpdatePageTableProtectionBits ();

            pptbl = GetPageTable (g_ppdirNK, VA2PDIDX (pptbl));
            DEBUGCHK (pptbl);

            for (idx = 0; idx < VM_NUM_PT_ENTRIES; idx ++) {
                if (IsPageCommitted (pptbl->pte[idx])) {
                    pptbl->pte[idx] |= dwPTCacheBits;
                }
            }

            // now update the access to kernel page direcotry
            pptbl = GetPageTable (g_ppdirNK, VA2PDIDX (g_ppdirNK));
            idx   = VA2PT2ND (g_ppdirNK);
            // 4 consecutive entries (page directory is 16K)
            pptbl->pte[idx]   |= dwPTCacheBits;
            pptbl->pte[idx+1] |= dwPTCacheBits;
            pptbl->pte[idx+2] |= dwPTCacheBits;
            pptbl->pte[idx+3] |= dwPTCacheBits;
        }

        // update 1st level page table entries if needed
        if (g_pOemGlobal->dwARM1stLvlBits) {
            // only the top half of the page directory (kernel area) can have non-zero page directory entries.
            for (idx = VM_NUM_PD_ENTRIES/2; idx < VM_NUM_PD_ENTRIES; idx ++) {
                if (PD_COARSE_TABLE == (g_ppdirNK->pte[idx] & PD_TYPE_MASK)) {
                    g_ppdirNK->pte[idx] |= g_pOemGlobal->dwARM1stLvlBits;
                }
            }
        }

        if (g_pOemGlobal->dwTTBRCacheBits) {
            // update TTBR cache setting bits.
            g_pKData->dwTTBRCtrlBits |= g_pOemGlobal->dwTTBRCacheBits;

            // call ARMV6InitTTBR again to update the bits
            ARMV6InitTTBR ();

        }

        OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_ALL);

        // initialize all other CPUs
        InitAllOtherCPUs ();
        

    }

    OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_ALL);

    // make sure the BSP didn't accidentally leave VFP enabled
    // (that would break the kernel VFP context switching)
    //
    if (HaveVfpHardware()) {
        ReadAndSetFpexc (0);
    }
    
    DEBUGMSG (1, (TEXT("NKStartup done, starting up kernel. nCpus = %d\r\n"), g_pKData->nCpus));
    DEBUGLOG (ZONE_SCHEDULE, &g_schedLock);
    DEBUGLOG (ZONE_SCHEDULE, &g_physLock);
    DEBUGLOG (ZONE_SCHEDULE, &g_oalLock);

    KernelStart ();

    // never returned
    DEBUGCHK (0);
}


DWORD NKSnapshotSMPStart()
{
    extern DWORD idCpu;
    LONG nCpus = 1;

    if ( g_pOemGlobal->fMPEnable ) {

        idCpu = 1;    
        g_fStartMP = FALSE;
        sg_fResumeFromSnapshot = TRUE;
        g_nCpuStopped = 1;
        DataSyncBarrier();
    
        g_pOemGlobal->pfnStartAllCpus (&nCpus, (FARPROC) NKMpStart);

        g_pKData->nCpus = nCpus;
            
        StartAllCPUs();
    }
    
    return nCpus;
}


