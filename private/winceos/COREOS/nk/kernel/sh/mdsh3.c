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
//------------------------------------------------------------------------------
//
//
//------------------------------------------------------------------------------
#include "kernel.h"
#include "shx.h"

static BOOL sg_fResumeFromSnapshot = FALSE;

extern void UnusedHandler(void);
extern void OEMInitDebugSerial(void);
extern void InitClock(void);
extern void DumpFrame(PTHREAD pth, PCONTEXT pctx, DWORD dwExc, DWORD info);
extern void APICallReturn(void);

void LoadKPage (PPCB ppcb, struct KDataStruct *pKData);
void InitPageDirectory (void);

void SaveFloatContext(PTHREAD, BOOL fPreempted);
void RestoreFloatContext(PTHREAD);
DWORD GetAndClearFloatCode(void);
DWORD GetAndClearFloatCode(void);
DWORD GetCauseFloatCode(void);
extern BOOL HandleHWFloatException(EXCEPTION_RECORD *ExceptionRecord,
                                   PCONTEXT pctx);
extern unsigned int get_fpscr();
extern void set_fpscr(unsigned int);
extern void clr_fpscr(unsigned int);
extern void DisableFPU();

const wchar_t NKCpuType [] = TEXT("SH-4");
const wchar_t NKSignon[] = TEXT("Windows CE Kernel for SH4") 
#ifdef DEBUG
                           TEXT(" Built on ") TEXT(__DATE__) TEXT(" at ") TEXT(__TIME__)
#endif // DEBUG
                           TEXT("\r\n");

extern char InterlockedAPIs[], InterlockedEnd[];
extern DWORD ExceptionTable[];

// friendly exception name. 1 based (0th entry is for exception id==1)
const LPCSTR g_ppszMDExcpId [MD_MAX_EXCP_ID+1] = {
    IDSTR_INVALID_EXCEPTION,        // invalid exception (0)
    IDSTR_INVALID_EXCEPTION,        // invalid exception (1)
    IDSTR_ACCESS_VIOLATION,         // TLB miss (load) (2)
    IDSTR_ACCESS_VIOLATION,         // TLB miss (store) (3)
    IDSTR_ACCESS_VIOLATION,         // TLB modification (4)
    IDSTR_ACCESS_VIOLATION,         // TLB protection (load) (5)
    IDSTR_ACCESS_VIOLATION,         // TLB protection (store) (6)
    IDSTR_ALIGNMENT_ERROR,          // address alignment error (load) (7)
    IDSTR_ALIGNMENT_ERROR,          // address alignment error (store) (8)
    IDSTR_FPU_EXCEPTION,            // FPU exception (9)
    IDSTR_INVALID_EXCEPTION,        // invalid exception (10)
    IDSTR_INVALID_EXCEPTION,        // break point (11)
    IDSTR_ILLEGAL_INSTRUCTION,      // reserved instruction (12)
    IDSTR_ILLEGAL_INSTRUCTION,      // illegal instruction (13)
    IDSTR_INVALID_EXCEPTION,        // invalid exception (14)
    IDSTR_HARDWARE_BREAK,           // hardaware break (15)
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
HookInterrupt(
    int hwInterruptNumber,
    FARPROC pfnHandler
    ) 
{
    // exclude lower 5 bits and pre-defined 16 codes
    int interruptNumberMax = (1 << (g_pOemGlobal->dwSHxIntEventCodeLength - 5)) - 16;
    if (hwInterruptNumber > interruptNumberMax)
        return FALSE;
    InterruptTable[hwInterruptNumber] = (DWORD)pfnHandler;        
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
UnhookInterrupt(
    int hwInterruptNumber,
    FARPROC pfnHandler
    ) 
{
    // exclude lower 5 bits and pre-defined 16 codes
    int interruptNumberMax = (1 << (g_pOemGlobal->dwSHxIntEventCodeLength - 5)) - 16;
    if (hwInterruptNumber > interruptNumberMax ||
        InterruptTable[hwInterruptNumber] != (DWORD)pfnHandler)
        return FALSE;
    InterruptTable[hwInterruptNumber] = (DWORD)UnusedHandler;
    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SwitchFPUOwner(
    PCONTEXT pctx
    ) 
{
    PPCB    ppcb         = GetPCB ();
    PTHREAD pCurFPUOwner = ppcb->pCurFPUOwner;
    PTHREAD pCurTh       = ppcb->pCurThd;
    KCALLPROFON(61);
    if (pCurFPUOwner != pCurTh) {
        if (pCurFPUOwner)
            SaveFloatContext(pCurFPUOwner, TRUE);
        ppcb->pCurFPUOwner = pCurTh;
        RestoreFloatContext(pCurTh);
    }
    KCALLPROFOFF(61);
    pctx->Psr &= ~SR_FPU_DISABLED;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
FPUFlushContext(void) 
{
    if (!InSysCall())
        KCall ((FARPROC) FPUFlushContext);
    else {
        PPCB    ppcb         = GetPCB ();
        PTHREAD pCurFPUOwner = ppcb->pCurFPUOwner;
        if (pCurFPUOwner) {
            SaveFloatContext(pCurFPUOwner, TRUE);
            pCurFPUOwner->ctx.Psr |= SR_FPU_DISABLED;
            DisableFPU();
            ppcb->pCurFPUOwner = NULL;
        }
    }
}

DWORD dwStoreQueueBase;
BOOL DoSetRAMMode(BOOL bEnable, LPVOID *lplpvAddress, LPDWORD lpLength);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
NKSetRAMMode(
    BOOL bEnable,
    LPVOID *lplpvAddress,
    LPDWORD lpLength
    ) 
{
    TRUSTED_API (L"SC_SetRAMMode", FALSE);

    return DoSetRAMMode(bEnable, lplpvAddress, lpLength);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID 
NKSetStoreQueueBase(
    DWORD dwPhysPage
    ) 
{
    TRUSTED_API (L"SC_SetStoreQueueBase", NULL);
    
    if (dwPhysPage & (1024*1024-1)) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        return 0;
    }
    if (dwPhysPage & 0xe0000000) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        return 0;
    }
    dwStoreQueueBase = dwPhysPage | PG_VALID_MASK | PG_1M_MASK | PG_PROT_WRITE | PG_DIRTY_MASK | 1;
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_ALL);
    return (LPVOID)0xE0000000;
}

#define ILOCK_REGION_SIZE       (0x400 - FIRST_INTERLOCK)
#define FirstILockAddr          ((DWORD) PUserKData+FIRST_INTERLOCK)
#define IsPCInIlockAddr(pc)     ((DWORD) ((pc) - FirstILockAddr) < ILOCK_REGION_SIZE)

void MDCheckInterlockOperation (PCONTEXT pCtx)
{
    DWORD dwPc = CONTEXT_TO_PROGRAM_COUNTER (pCtx);
    if (IsPCInIlockAddr (dwPc)      // in interlocked region
        && !(dwPc & 0x1)) {         // PC is WORD aligned
        // Interlocked operation in progress, reset PC to start of interlocked operation
        SoftLog (0xdddd0000, dwPc);
        CONTEXT_TO_PROGRAM_COUNTER (pCtx) = dwPc & -8;
    }
}

//
// FPU flush function on context switch
//
void MDFlushFPU (BOOL fPreempted)
{
    DEBUGCHK (InSysCall ());
    if (g_pKData->nCpus > 1) {
        PPCB    ppcb         = GetPCB ();
        PTHREAD pCurFPUOwner = ppcb->pCurFPUOwner;
        if (pCurFPUOwner) {
            DEBUGCHK (pCurFPUOwner == ppcb->pCurThd);
            SaveFloatContext(pCurFPUOwner, fPreempted);
            pCurFPUOwner->ctx.Psr |= SR_FPU_DISABLED;
            DisableFPU();
            ppcb->pCurFPUOwner = NULL;
        }
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void MDCaptureFPUContext (PCONTEXT pCtx)
{
    FPUFlushContext();
    // fpsrc + fpul + 32 FP registers
    memcpy (&pCtx->Fpscr, &pCurThread->ctx.Fpscr, sizeof(pCtx->FRegs)+sizeof(pCtx->xFRegs)+2*REGSIZE);
    pCtx->ContextFlags |= CONTEXT_FLOATING_POINT;
}


void MDDiscardFPUContent (void)
{
    PPCB    ppcb         = GetPCB ();
    PTHREAD pCurFPUOwner = ppcb->pCurFPUOwner;
    
    DEBUGCHK (InPrivilegeCall ());

    if (pCurFPUOwner == ppcb->pCurThd) {
        pCurFPUOwner->ctx.Psr |= SR_FPU_DISABLED;
        DisableFPU();
        ppcb->pCurFPUOwner = NULL;
    }
}

void MDSetupUserKPage (PPAGETABLE pptbl)
{
    // page 5 access to KPAGE
    pptbl->pte[VM_KPAGE_IDX] = KPAGE_PTE;               // setup UseKPage access
}

void MDClearUserKPage (PPAGETABLE pptbl)
{
    pptbl->pte[VM_KPAGE_IDX] = 0;                       // clear UseKPage access
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PTHREAD 
HandleException(
    PTHREAD pth,
    DWORD dwExc,    // exception code, 0 for interrupts/reschedule
    DWORD info      // TEA/TRPA, or ISR to call for interrupts
    ) 
{
    if (!KC_CommonHandleException (pth, dwExc, info, 0)) {
        PPCB ppcb = GetPCB ();
        DEBUGCHK (ppcb->dwKCRes);
        if (ppcb->ownspinlock || ppcb->cNest) {
            // recursive fault while in KCall, or holding a spinlock.
            // not much we can do but halting the system
            NKD (L"!!!FATAL ERROR: Recursive Fault inside KCall, system halt!!!\r\n");
            DumpFrame (ppcb->pCurThd, (PCONTEXT) &pth->ctx, dwExc, info);
            OEMIoControl (IOCTL_HAL_HALT, NULL, 0, NULL, 0, NULL);
            
        } else {
            pth = NULL;
        }
    }
    
    return pth;
}


//
// argument dwExc, info, info are exactly the same as the value HandleException passed to KC_CommonHandleException
//
void MDSetupExcpInfo (PTHREAD pth, PCALLSTACK pcstk, DWORD dwExc, DWORD info, DWORD unused)
{
    // use non-volatile register fields in pcstk to save exception information
    pcstk->regs[REG_R8] = dwExc;    // R8: cause
    pcstk->regs[REG_R9] = info;     // R9: badVAddr

    // update PSR
    pth->ctx.Psr |= 0x40000000; // Kernel mode
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL MDHandleHardwareException (PCALLSTACK pcstk, PEXCEPTION_RECORD pExr, PCONTEXT pCtx, LPDWORD pdwExcpId)
{
    PTHREAD pth = pCurThread; 
    DWORD exc = (DWORD) pcstk->regs[REG_R8] >> 5;
    BOOL fInKMode = !(pcstk->dwPrcInfo & CST_MODE_FROM_USER);    
    BOOL fHandled = FALSE;
    ULONG info = (ULONG) pcstk->regs[REG_R9];

    // Construct an EXCEPTION_RECORD from the EXCINFO structure
    pExr->ExceptionInformation[1] = info;
    *pdwExcpId = exc;
        
    switch (exc) {
    case ID_ADDRESS_ERROR_LOAD:         // Address error (store)
    case ID_ADDRESS_ERROR_STORE:        // Address error (load or instruction fetch)
        if (fInKMode || !IsKModeAddr (info)) {
            // misalignment
            pExr->ExceptionCode = (DWORD) STATUS_DATATYPE_MISALIGNMENT;
        } else {
            // access violation
            pExr->ExceptionCode = (DWORD) STATUS_ACCESS_VIOLATION;
            pExr->NumberParameters = 2;
        }
        break;

    case ID_TLB_MISS_STORE:             // TLB Miss (store)
    case ID_TLB_MODIFICATION:           // TLB modification
    case ID_TLB_PROTECTION_STORE:       // TLB protection violation (store)
        pExr->ExceptionInformation[0] = 1;      // ExceptionInformation[0] = fWrite == TRUE
        // faull through for access error (access violation)
        __fallthrough;
        
    case ID_TLB_MISS_LOAD:              // TLB miss (load or instruction fetch)
    case ID_TLB_PROTECTION_LOAD:        // TLB protection violation (load)
        pExr->ExceptionCode = (DWORD) STATUS_ACCESS_VIOLATION;
        pExr->NumberParameters = 2;
        if (!InSysCall ()
            && (fInKMode || !IsKModeAddr (info))    // have access to the address
            && !IsInSharedHeap (info)) {            // faulted while access shared heap - always fail                 
            //
            // since we might be calling out of kernel to perform paging and there's
            // no gurantee that the driver underneath won't touch FPU, we need to save
            // the floating point context and restoring it after paging.
            //
            // get the contents of the floating point registers
            FPUFlushContext ();
            // save FPU context in exception context
            memcpy (pCtx->FRegs, pCurThread->ctx.FRegs, sizeof(pCtx->FRegs));
            memcpy (pCtx->xFRegs, pCurThread->ctx.xFRegs, sizeof(pCtx->xFRegs));
            pCtx->Fpscr = pCurThread->ctx.Fpscr;
            pCtx->Fpul = pCurThread->ctx.Fpul;
            fHandled = VMProcessPageFault (pVMProc, info, pExr->ExceptionInformation[0], 0);
            
            // throw away what's in the floating point registers by flushing it.
            FPUFlushContext ();
            // restore FPU context from excepiton context
            memcpy (pCurThread->ctx.FRegs, pCtx->FRegs, sizeof(pCtx->FRegs));
            memcpy (pCurThread->ctx.xFRegs, pCtx->xFRegs, sizeof(pCtx->xFRegs));
            pCurThread->ctx.Fpscr = pCtx->Fpscr;
            pCurThread->ctx.Fpul = pCtx->Fpul;
        }
        break;
    case 0x40:
    case 0x41:
        (fInKMode? pth->tlsSecure : pth->tlsNonSecure)[TLSSLOT_KERNEL] |= TLSKERN_FPU_USED;
        KCall((PKFN)SwitchFPUOwner,pCtx);
        fHandled = TRUE;
        break;
    case ID_FPU_EXCEPTION: {           // floating point exception
        DWORD code;
        code = GetCauseFloatCode();
        if (code & 0x10)
            pExr->ExceptionCode = (DWORD) STATUS_FLOAT_INVALID_OPERATION;
        else if (code & 0x8)
            pExr->ExceptionCode = (DWORD) STATUS_FLOAT_DIVIDE_BY_ZERO;
        else if (code & 0x4)
            pExr->ExceptionCode = (DWORD) STATUS_FLOAT_OVERFLOW;
        else if (code & 0x2)
            pExr->ExceptionCode = (DWORD) STATUS_FLOAT_UNDERFLOW;
        else if (code & 0x1)
            pExr->ExceptionCode = (DWORD) STATUS_FLOAT_INEXACT_RESULT;
        else
        {
            //
            // Reach here 
            // --if code is 0x20 (FPU error)
            // --if code is 0x0 (processor thinks an ieee
            //   exception is possible)
            //
            // both cases require that fp operation be emulated
            // for correct ieee result.
            //
            // save the fpscr before FPUFlushContext will flush it
            // to zero.
            //
            // FPUFlushContext clears fpscr
            //
            FPUFlushContext();

            //
            // Copy current thread fregs and xfreg to user context
            //
            memcpy(&pCtx->FRegs[0],&pCurThread->ctx.FRegs[0],sizeof(DWORD)*16);
            memcpy(&pCtx->xFRegs[0],&pCurThread->ctx.xFRegs[0],sizeof(DWORD)*16);
            pCtx->Fpscr = pCurThread->ctx.Fpscr;
            pCtx->Fpul = pCurThread->ctx.Fpul;

            fHandled = HandleHWFloatException(pExr,pCtx);
            if (fHandled) 
            {
                // flush float context back to thread context after exception was handled
                FPUFlushContext();

                //
                // update current thread context with user context
                //
                memcpy(&pCurThread->ctx.FRegs[0],&pCtx->FRegs[0],sizeof(DWORD)*16);
                memcpy(&pCurThread->ctx.xFRegs[0],&pCtx->xFRegs[0],sizeof(DWORD)*16);

                pCurThread->ctx.Fpul = pCtx->Fpul;
                pCurThread->ctx.Fpscr = pCtx->Fpscr;
                pCurThread->ctx.Psr = pCtx->Psr;

                pCtx->Fir+=2; // +2: return control to instruction successor
                break;
            }
        }
        //
        // Update user context fpscr and fpul
        //
        pCurThread->ctx.Fpscr = pCtx->Fpscr;
        pCurThread->ctx.Fpul = pCtx->Fpul;
        pCurThread->ctx.Psr = pCtx->Psr;
        break;
    }
    case ID_BREAK_POINT:        // Breakpoint
        pCtx->Fir -= 2; // backup to breakpoint instruction    
        pExr->ExceptionAddress = (PVOID) (((ULONG)(pExr->ExceptionAddress)) - 2);        
        pExr->ExceptionInformation[0] = info;
        pExr->ExceptionCode = (DWORD) STATUS_BREAKPOINT;
        break;

    case ID_RESERVED_INSTR:         // Reserved instruction
    case ID_ILLEGAL_INSTRUCTION:    // Illegal slot instruction
        pExr->ExceptionCode = (DWORD) STATUS_ILLEGAL_INSTRUCTION;
        break;
    case ID_HARDWARE_BREAK:    // Hardware breakpoint
        pExr->ExceptionInformation[0] = info;
        pExr->ExceptionCode = (DWORD) STATUS_USER_BREAK;
        break;

    default:
        // unknown exception
        pExr->ExceptionCode = (DWORD) STATUS_ILLEGAL_INSTRUCTION;
        break;
        
    }

    return fHandled;            
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
DumpFrame(
    PTHREAD pth,
    PCONTEXT pctx,
    DWORD dwExc,
    DWORD info
    ) 
{
    NKDbgPrintfW(L"Exception %03x Thread=%8.8lx PC=%8.8lx\r\n",
                 dwExc, pth, pctx->Fir);
    NKDbgPrintfW(L" R0=%8.8lx  R1=%8.8lx  R2=%8.8lx  R3=%8.8lx\r\n",
                 pctx->R0, pctx->R1, pctx->R2, pctx->R3);
    NKDbgPrintfW(L" R4=%8.8lx  R5=%8.8lx  R6=%8.8lx  R7=%8.8lx\r\n",
                 pctx->R4, pctx->R5, pctx->R6, pctx->R7);
    NKDbgPrintfW(L" R8=%8.8lx  R9=%8.8lx R10=%8.8lx R11=%8.8lx\r\n",
                 pctx->R8, pctx->R9, pctx->R10, pctx->R11);
    NKDbgPrintfW(L" R12=%8.8lx R13=%8.8lx R14=%8.8lx R15=%8.8lx\r\n",
                 pctx->R12, pctx->R13, pctx->R14, pctx->R15);
    NKDbgPrintfW(L" PR=%8.8lx  SR=%8.8lx TEA/TRPA=%8.8x\r\n",
                 pctx->PR, pctx->Psr, info);
    NKDbgPrintfW(L" PTEL=%8.8lx PTEH=%8.8lx MMUCR=%8.8lx TTB=%8.8lx\r\n",
                 MMUPTEL, MMUPTEH, MMUCR, MMUTTB);
}
    
BOOL MDDumpFrame (PTHREAD pth, DWORD dwExcpId, PEXCEPTION_RECORD pExr, PCONTEXT pCtx, DWORD dwLevel)
{
    DumpFrame (pth, pCtx, dwExcpId, pExr->ExceptionInformation[1]);
    return TRUE;
}


void MDSkipBreakPoint (PEXCEPTION_RECORD pExr, PCONTEXT pCtx)
{
    pCtx->Fir += 2;     // skip over the trapa instruction
}


BOOL MDIsPageFault (DWORD dwExcpId)
{
    // TLB error range from 2 to 5
    return (dwExcpId >= 2) && (dwExcpId <= 5); 
}

void MDClearVolatileRegs (PCONTEXT pCtx)
{
    pCtx->PR = 0;
    pCtx->MACL = 0;
    pCtx->MACH = 0;
    pCtx->GBR = 0;
    pCtx->R0 = 0;
    pCtx->R1 = 0;
    pCtx->R2 = 0;
    pCtx->R3 = 0;
    pCtx->R4 = 0;
    pCtx->R5 = 0;
    pCtx->R6 = 0;
    pCtx->R7 = 0;
}

LPDWORD MDGetRaisedExceptionInfo (PEXCEPTION_RECORD pExr, PCONTEXT pCtx, PCALLSTACK pcstk)
{
    DWORD cArgs = (DWORD) CONTEXT_TO_PARAM_3 (pCtx);
    LPDWORD pArgs = (LPDWORD) CONTEXT_TO_PARAM_4 (pCtx);

    // to avoid boundary problems at the end of a try block.    
    CONTEXT_TO_PROGRAM_COUNTER (pCtx) -= INST_SIZE;
    
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
    
    pExr->ExceptionCode  = (DWORD) CONTEXT_TO_PARAM_1 (pCtx);
    pExr->ExceptionFlags = (DWORD) CONTEXT_TO_PARAM_2 (pCtx);

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


BOOL MDDumpThreadContext (PTHREAD pth, DWORD dwCause, DWORD badVAddr, DWORD unused, DWORD level)
{
    DumpFrame (pth, &pth->ctx, dwCause, badVAddr);
    return TRUE;
}


#ifdef DEBUG
int MDDbgPrintExceptionInfo (PEXCEPTION_RECORD pExr, PCALLSTACK pcstk)
{
    DEBUGMSG(ZONE_SEH || !IsInKVM ((DWORD) pcstk), (TEXT("ExceptionDispatch: pcstk=%8.8lx PC=%8.8lx cause=%x\r\n"),
            pcstk, pcstk->retAddr, pcstk->regs[REG_R8]));
    return TRUE;
}
#endif


//------------------------------------------------------------------------------
// normal thread stack: from top, TLS, PRETLS, then args then free
//------------------------------------------------------------------------------
void 
MDSetupThread(
    PTHREAD pTh,
    FARPROC  lpBase,
    FARPROC  lpStart,
    BOOL    kmode,
    ulong   param
    ) 
{
    // Clear all registers: sp. fpu state for SH-4
    memset(&pTh->ctx, 0, sizeof(pTh->ctx));
    // Leave room for arguments and TLS and PRETLS on the stack
    pTh->ctx.R15 = (ulong) pTh->tlsPtr - SECURESTK_RESERVE;

    pTh->ctx.R4 = (ulong)lpStart;
    pTh->ctx.R5 = param;
    pTh->ctx.PR = 0;
    pTh->ctx.Psr = SR_FPU_DISABLED; // disable floating point
    pTh->ctx.Fpscr = 0x40000;  // handle no exceptions
    pTh->ctx.Fir = (ULONG)lpBase;
    pTh->ctx.ContextFlags = CONTEXT_FULL;

#ifdef DEBUG
    pTh->ctx.R1 = 0x1111111;
    pTh->ctx.R2 = 0x2222222;
    pTh->ctx.R3 = 0x3333333;
    pTh->ctx.R6 = 0x6666666;
    pTh->ctx.R7 = 0x7777777;
    pTh->ctx.R8 = 0x8888888;
    pTh->ctx.R9 = 0x9999999;
    pTh->ctx.R10 = 0xaaaaaaa;
    pTh->ctx.R11 = 0xbbbbbbb;
    pTh->ctx.R12 = 0xccccccc;
    pTh->ctx.R13 = 0xddddddd;
    pTh->ctx.R14 = 0xeeeeeee;
#endif
    
    if (kmode) {
        SetThreadMode (pTh, KERNEL_MODE);
    } else {
        SetThreadMode (pTh, USER_MODE);
    }
}


//------------------------------------------------------------------------------
// main thread stack: from top, TLS then buf then buf2 then buf2 (ascii) then args then free
//------------------------------------------------------------------------------
void MDInitStack(LPBYTE lpStack, DWORD cbSize)
{
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SCHL_DoThreadGetContext(
    PTHREAD pth,
    LPCONTEXT lpContext
    ) 
{
    ULONG ContextFlags = lpContext->ContextFlags & ~CONTEXT_SH4; // clear the CPU type, or the "&" operation will never be 0
    BOOL fRet = TRUE;
    AcquireSchedulerLock (0);

    if (!KC_IsValidThread (pth) || (ContextFlags & ~CONTEXT_FULL)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        fRet = FALSE;
    } else {
        // for SH4, "CPUCONTEXT structure" and "CONTEXT structure" are exactly the same
        PCONTEXT pThrdCtx = pth->pSavedCtx ? pth->pSavedCtx : (PCONTEXT) &pth->ctx;

        if (ContextFlags & CONTEXT_CONTROL) {
            lpContext->PR  = pThrdCtx->PR;
            lpContext->R15 = pThrdCtx->R15;
            lpContext->Fir = pThrdCtx->Fir;
            lpContext->Psr = pThrdCtx->Psr;
        }
        if (ContextFlags & CONTEXT_INTEGER) {
            // MACH, MACL, GBR, R0-R14
            memcpy (&lpContext->MACH, &pThrdCtx->MACH, 18 * REGSIZE);
        }

        if (ContextFlags & CONTEXT_FLOATING_POINT) {
            FPUFlushContext();
            // fpsrc + fpul + 32 FP registers
            memcpy (&lpContext->Fpscr, &pThrdCtx->Fpscr, sizeof(pThrdCtx->FRegs)+sizeof(pThrdCtx->xFRegs)+2*REGSIZE);
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
    ULONG ContextFlags = lpContext->ContextFlags & ~CONTEXT_SH4; // clear the CPU type, or the "&" operation will never be 0
    BOOL fRet = TRUE;
    AcquireSchedulerLock (0);

    if (!KC_IsValidThread (pth) || (ContextFlags & ~CONTEXT_FULL)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        fRet = FALSE;
    } else {
        // for SH4, "CPUCONTEXT structure" and "CONTEXT structure" are exactly the same
        PCONTEXT pThrdCtx = pth->pSavedCtx ? pth->pSavedCtx : (PCONTEXT) &pth->ctx;

        if (ContextFlags & CONTEXT_CONTROL) {
            pThrdCtx->PR  = lpContext->PR;
            pThrdCtx->R15 = lpContext->R15;
            pThrdCtx->Fir = lpContext->Fir;
            pThrdCtx->Psr = (pth->ctx.Psr & 0xfffffcfc) | (lpContext->Psr & 0x00000303);
        }
        if (ContextFlags & CONTEXT_INTEGER) {
            // MACH, MACL, GBR, R0-R14
            memcpy (&pThrdCtx->MACH, &lpContext->MACH, 18 * REGSIZE);
        }
        if (ContextFlags & CONTEXT_FLOATING_POINT) {
            FPUFlushContext();
            // fpsrc + fpul + 32 FP registers
            memcpy (&pThrdCtx->Fpscr, &lpContext->Fpscr, sizeof(pThrdCtx->FRegs)+sizeof(pThrdCtx->xFRegs)+2*REGSIZE);
        }
    }
    ReleaseSchedulerLock (0);
    return fRet;
}

void MDRestoreCalleeSavedRegisters (PCALLSTACK pcstk, PCONTEXT pCtx)
{
    const ULONG *pRegs = (ULONG *) pcstk->regs;

    pCtx->R8  = pRegs[REG_R8];
    pCtx->R9  = pRegs[REG_R9];
    pCtx->R10 = pRegs[REG_R10];
    pCtx->R11 = pRegs[REG_R11];
    pCtx->R12 = pRegs[REG_R12];
    pCtx->R13 = pRegs[REG_R13];
    pCtx->R14 = pRegs[REG_R14];
}

void NKMpStart (void);
void SHSetASID (DWORD dwASID);

BOOL  g_fStartMP;

extern volatile LONG g_nCpuStopped;
extern volatile LONG g_nCpuReady;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void MpStartContinue (PPCB ppcb)
{
    LoadKPage (ppcb, g_pKData);
    SHSetASID (g_pprcNK->bASID);

    // call per CPU initialization function and get the hardware CPU id
    ppcb->dwHardwareCPUId    = g_pOemGlobal->pfnMpPerCPUInit ();
    ppcb->ActvProcId         = g_pprcNK->dwId;
    ppcb->dwCpuState         = sg_fResumeFromSnapshot? CE_PROCESSOR_STATE_POWERED_OFF : CE_PROCESSOR_STATE_POWERED_ON;
    ppcb->OwnerProcId        = g_pprcNK->dwId;
    ppcb->dwPrevTimeModeTime = ppcb->dwPrevReschedTime = GETCURRTICK ();
    SetReschedule (ppcb);

    DEBUGMSG (1, (L"CPU %d started, ready to reschedule, ppcb = %8.8lx\r\n", ppcb->dwCpuId, ppcb));

    InterlockedIncrement (&g_nCpuReady);

    while (g_nCpuStopped) {
    }
    
}

void InitAllOtherCPUs (void)
{
    if (g_pOemGlobal->fMPEnable) {
        DWORD nCpus;

        // call to OAL to perform MP Init
        if (g_pOemGlobal->pfnStartAllCpus ((PLONG) &nCpus, (FARPROC) ((DWORD) NKMpStart|0x20000000)) && (nCpus > 1)) {
            DWORD dwPerMPMemoryBase, idx;
            PPCB ppcb;
            // MP detected. allocate memory for PCB/stacks for the CPUs
            DEBUGMSG (1, (L"MP Detected, # of CPUs = %8.8lx\r\n", nCpus));

            g_pKData->nCpus = nCpus;

            dwPerMPMemoryBase = pTOC->ulRAMFree + MemForPT;

            // per-CPU base needs to be 8k aligned to avoid page coloring issue
            if (dwPerMPMemoryBase & (2 * VM_PAGE_SIZE - 1)) {
                dwPerMPMemoryBase += VM_PAGE_SIZE;
                MemForPT += VM_PAGE_SIZE;
                
            }
            
            // each core got 2 pages for PCB, stacks, etc.
            MemForPT += (nCpus - 1) * 2 * VM_PAGE_SIZE;

            // initialize AP Pages
            for (idx = 1; idx < nCpus; idx ++) {
                ppcb = (PPCB) (dwPerMPMemoryBase + 0x800 + (2 * idx - 1) * VM_PAGE_SIZE);
                memset (ppcb, 0, sizeof (PCB));

                ppcb->pSelf      = ppcb;
                ppcb->pKStack    = (LPBYTE) ppcb - 8 * REG_SIZE;
                ppcb->pVMPrc     = ppcb->pCurPrc = g_pprcNK;
                ppcb->pCurThd    = &dummyThread;
                ppcb->CurThId    = DUMMY_THREAD_ID;
                ppcb->ActvProcId = g_pprcNK->dwId;
                ppcb->dwCpuId    = idx+1;
                ppcb->dwSyscallReturnTrap = SYSCALL_RETURN_RAW;
                g_ppcbs[idx]     = ppcb;
            }
            InitInterlockedFunctions ();
            KCall ((PKFN)OEMCacheRangeFlush, 0, 0, CACHE_SYNC_DISCARD);
        }
    }
}

void StartAllCPUs (void)
{

    if (g_pKData->nCpus > 1) {

        DEBUGMSG (1, (L"Resuming all CPUs, g_pKData->nCpus = %d\r\n", g_pKData->nCpus));

        INTERRUPTS_OFF ();
        g_fStartMP = TRUE;

        while (g_pKData->nCpus != (DWORD) g_nCpuReady) {
            ;
        }
        if(sg_fResumeFromSnapshot){
            g_nCpuReady = 1;
        }
        g_nCpuStopped = 0;
        INTERRUPTS_ON ();
        KCall ((PKFN) OEMCacheRangeFlush, 0, 0, CACHE_SYNC_DISCARD);
        DEBUGMSG (1, (L"All CPUs resumed, g_nCpuReady = %d\r\n", g_nCpuReady));
    } else {
        g_nCpuStopped = 0;
    }
}


//------------------------------------------------------------------------------
//
// SHSetup is the only C function that runs on bank 1. 
// - Cannot have any TLB miss/lookup
// - Cannot access PCB function (PCBGetxxx/PCBSetxxx).
//
//------------------------------------------------------------------------------
void 
SHSetup(
    struct KDataStruct *pKDataUncached,
    DWORD_PTR dwOEMInitGlobals
    ) 
{
    PFN_OEMInitGlobals pfnInitGlob;
    PPCB  ppcb;
    struct _OSAXS_KERN_POINTERS_2 *pOsAxsDataBlock;
    struct _OSAXS_HWTRAP *pOsAxsHwTrap;

    // (1) Zero out kernel data page and initialize it
    // save off the members passed from nkloader
    pTOC            = (ROMHDR const *) pKDataUncached->dwTOCAddr;
    pOsAxsDataBlock = pKDataUncached->pOsAxsDataBlock;
    pOsAxsHwTrap    = pKDataUncached->pOsAxsHwTrap;

    // clear KData
    memset (pKDataUncached, 0, sizeof(struct KDataStruct));

    // restore fields
    g_pKData = (struct KDataStruct *) (((DWORD) pKDataUncached) & ~0x20000000);
    g_pKData->dwTOCAddr         = (DWORD) pTOC;
    g_pKData->pOsAxsDataBlock   = pOsAxsDataBlock;
    g_pKData->pOsAxsHwTrap      = pOsAxsHwTrap;

    // initialize other KData fields
    g_pKData->nCpus             = 1;
    g_pKData->pAPIReturn        = KInfoTable[KINX_PTOC] = (DWORD) APICallReturn;
    g_pKData->pNk               = g_pNKGlobal;
    g_pKData->dwKVMStart        = VM_NKVM_BASE;
    KInfoTable[KINX_PAGESIZE]   = VM_PAGE_SIZE;

    // initialized PCB fields
    ppcb            = (PPCB) g_pKData;
    ppcb->pKStack   = ((LPBYTE) ppcb) - 8 * REGSIZE;      // leave room for 8 registers
    ppcb->dwCpuId   = 1;
    ppcb->pSelf     = ppcb;
    ppcb->pVMPrc    = ppcb->pCurPrc = g_pprcNK;
    ppcb->CurThId   = 2;        // faked current thread id so ECS won't put 0 in OwnerThread
    ppcb->dwCpuState = CE_PROCESSOR_STATE_POWERED_ON;
    ppcb->dwSyscallReturnTrap = SYSCALL_RETURN_RAW;
    g_ppcbs[0]      = ppcb;
#ifdef DEBUG
    dwLastKCTime = CurMSec = ppcb->dwPrevTimeModeTime = ppcb->dwPrevReschedTime = (DWORD) -200000;      // ~3 minutes before wrap
#endif

    // (2) initialize ROM globals
    FirstROM.pTOC = (ROMHDR *) pTOC;
    FirstROM.pNext = 0;
    ROMChain = &FirstROM;

    // (4) exchange globals with oal
    pfnInitGlob     = (PFN_OEMInitGlobals) dwOEMInitGlobals;
    g_pOemGlobal    = pfnInitGlob (g_pNKGlobal);
    g_pOemGlobal->dwMainMemoryEndAddress = pTOC->ulRAMEnd;
    
    // (5) initialize nk globals
    g_pKData->pOem = g_pOemGlobal;
    g_pNKGlobal->pfnWriteDebugString = g_pOemGlobal->pfnWriteDebugString;
    g_pNKGlobal->IntrPrio = IntrPrio;
    g_pNKGlobal->InterruptTable = InterruptTable;
    ExceptionTable[0xE] = (DWORD) g_pOemGlobal->pfnNMIHandler;

    CEProcessorType  = PROCESSOR_HITACHI_SH4;
    CEProcessorLevel = 4;
    CEInstructionSet = PROCESSOR_HITACHI_SH4_INSTRUCTION;

    // (6) initialize kernel page directory entry
    InitPageDirectory ();
    g_pprcNK->ppdir = g_ppdirNK;
    g_pprcNK->vaFree = VM_NKVM_BASE;

    // (7) Copy interlocked api code into the kpage
    DEBUGCHK (sizeof(struct KDataStruct) <= FIRST_INTERLOCK);
    DEBUGCHK ((InterlockedEnd-InterlockedAPIs)+FIRST_INTERLOCK == 0x400);
    memcpy ((char *)g_pKData+FIRST_INTERLOCK, InterlockedAPIs, InterlockedEnd-InterlockedAPIs);


    // (8) initialize address translation hardware
    MMUTEA  = 0;                    /* clear transation address */
    MMUTTB  = (DWORD) g_ppdirNK;    /* set translation table base address */
    MMUPTEH = 0;                    /* clear ASID */
    MMUCR   = TLB_FLUSH | TLB_ENABLE;   /* flush TLB and enable MMU */

    __asm("nop\n"
          "nop\n"
          "nop\n"
          "nop\n"
          "nop\n"
          "nop\n"
          "nop\n"
          "nop\n");

    LoadKPage (ppcb, g_pKData);

    // Enable the CPU cache; OEM will set any extra bits in OEMInit call.
    CCR = CACHE_ENABLE;

    // SH4 cpu architecture requires when modifying CCR from P2 area,
    // at least 8 instructions must be in between before issuing a branch    // to U0/P0/P1/P3 area.
    __asm("nop\n"
          "nop\n"
          "nop\n"
          "nop\n"
          "nop\n"
          "nop\n"
          "nop\n"
          "nop\n");

}

void SHInit (void)
{
    PFN_DllMain pfnKitlEntry;

    // try to load KITL if exist
    pfnKitlEntry = (PFN_DllMain) g_pOemGlobal->pfnKITLGlobalInit;

    if (!pfnKitlEntry) {
        pfnKitlEntry = (PFN_DllMain) FindROMDllEntry (pTOC, KITLDLL);
    }
    if (pfnKitlEntry) {
        (* pfnKitlEntry) (NULL, DLL_PROCESS_ATTACH, (DWORD) NKKernelLibIoControl);
    }

    // initialize debug serial
    OEMInitDebugSerial();
    OEMWriteDebugString((LPWSTR) NKSignon);

    NKDbgPrintfW(L"CCR=%4.4x INTEVT=%4.4x\r\n", CCR, g_pOemGlobal->dwSHxIntEventCodeLength);

    // initialize firmware
    OEMInit();

    // startup secondary CPUs, if any
    InitAllOtherCPUs ();

#ifdef DEBUG
    OEMWriteDebugString(TEXT("SH Initialization is done.\r\n"));
#endif    

}

DWORD idxPCB;
DWORD NKSnapshotSMPStart()
{
    DWORD nCpus = 1;
    if ( g_pOemGlobal->fMPEnable ) {
        idxPCB = 0;
        g_fStartMP = FALSE;
        sg_fResumeFromSnapshot = TRUE;
        g_nCpuStopped = 1;
        // call to OAL to perform MP Init
        if (g_pOemGlobal->pfnStartAllCpus ((PLONG) &nCpus, (FARPROC) ((DWORD) NKMpStart|0x20000000)) && (nCpus > 1)) {
            g_pKData->nCpus = nCpus;
        }
        StartAllCPUs();
    }
    return nCpus;
}


