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
//
//
//------------------------------------------------------------------------------
#include "kernel.h"
#include "shx.h"

extern void UnusedHandler(void);
extern void OEMInitDebugSerial(void);
extern void InitClock(void);
extern void DumpFrame(PTHREAD pth, PCONTEXT pctx, DWORD dwExc, DWORD info);
extern void APICallReturn(void);
extern void LoadKPage(void);

void InitPageDirectory ();

void SaveFloatContext(PTHREAD);
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
const wchar_t NKSignon[] = TEXT("Windows CE Kernel for SH4 Built on ") TEXT(__DATE__) TEXT(" at ") TEXT(__TIME__) TEXT("\r\n");

// OEM definable extra bits for the Cache Control Register
unsigned long OEMExtraCCR;

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
    KCALLPROFON(61);
    if (g_CurFPUOwner != pCurThread) {
        if (g_CurFPUOwner)
            SaveFloatContext(g_CurFPUOwner);
        g_CurFPUOwner = pCurThread;
        RestoreFloatContext(pCurThread);
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
    else if (g_CurFPUOwner) {
        SaveFloatContext(g_CurFPUOwner);
        g_CurFPUOwner->ctx.Psr |= SR_FPU_DISABLED;
        DisableFPU();
        g_CurFPUOwner = 0;
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


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
HandleException(
    PTHREAD pth,
    DWORD dwExc,
    DWORD info
    ) 
{
    BOOL fRet;
    KCALLPROFON(0);
    fRet = KC_CommonHandleException (pth, dwExc, info, 0);
    KCALLPROFOFF(0);
    return fRet;
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
            pExr->ExceptionCode = STATUS_DATATYPE_MISALIGNMENT;
        } else {
            // access violation
            pExr->ExceptionCode = STATUS_ACCESS_VIOLATION;
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
        pExr->ExceptionCode = STATUS_ACCESS_VIOLATION;
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
            fHandled = VMProcessPageFault (pVMProc, info, pExr->ExceptionInformation[0]);
            
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
        KCall((PKFN)SwitchFPUOwner,pCtx);
        fHandled = TRUE;
        break;
    case ID_FPU_EXCEPTION: {           // floating point exception
        DWORD code;
        code = GetCauseFloatCode();
        if (code & 0x10)
            pExr->ExceptionCode = STATUS_FLOAT_INVALID_OPERATION;
        else if (code & 0x8)
            pExr->ExceptionCode = STATUS_FLOAT_DIVIDE_BY_ZERO;
        else if (code & 0x4)
            pExr->ExceptionCode = STATUS_FLOAT_OVERFLOW;
        else if (code & 0x2)
            pExr->ExceptionCode = STATUS_FLOAT_UNDERFLOW;
        else if (code & 0x1)
            pExr->ExceptionCode = STATUS_FLOAT_INEXACT_RESULT;
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

            if (fHandled = HandleHWFloatException(pExr,pCtx)) 
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
        pExr->ExceptionAddress = (RETADDR) (((ULONG)(pExr->ExceptionAddress)) - 2);        
        pExr->ExceptionInformation[0] = info;
        pExr->ExceptionCode = STATUS_BREAKPOINT;
        break;

    case ID_RESERVED_INSTR:         // Reserved instruction
    case ID_ILLEGAL_INSTRUCTION:    // Illegal slot instruction
        pExr->ExceptionCode = STATUS_ILLEGAL_INSTRUCTION;
        break;
    case ID_HARDWARE_BREAK:    // Hardware breakpoint
        pExr->ExceptionInformation[0] = info;
        pExr->ExceptionCode = STATUS_USER_BREAK;
        break;

    default:
        // unknown exception
        pExr->ExceptionCode = STATUS_ILLEGAL_INSTRUCTION;
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

// TODO: Review with Bor-Ming
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
    LPVOID  lpBase,
    LPVOID  lpStart,
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
    if (kmode) {
        SetThreadMode (pTh, KERNEL_MODE);
        KTHRDINFO (pTh) |= UTLS_INKMODE;
    } else {
        SetThreadMode (pTh, USER_MODE);
        KTHRDINFO (pTh) &= ~UTLS_INKMODE;
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
DoThreadGetContext(
    PTHREAD pth,
    LPCONTEXT lpContext
    ) 
{
    ULONG   ulContextFlags = lpContext->ContextFlags; // Keep a local copy of the context flag
    if (!KC_IsValidThread (pth)
        || (ulContextFlags & ~CONTEXT_FULL)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    // Clear the SH3 and SH4 bits in the context flags.  These are use to differentiate
    // SH3 and SH4.  Without doing so, the masking will always be true because of these 
    // bits.  For example, (CONTEXT_CONTROL & CONTEXT_INTEGER) will be either 0x40 or 0xc0 
    // depending on the processor, but it will never be zero.  So the consequence is that
    // we always return the full context no mater what flags users specify.
    // (Please see \public\common\sdk\inc\winnt.h for details.)
    ulContextFlags &= ~CONTEXT_SH4; 

    if (pth && pth->pSavedCtx) {
        if (ulContextFlags & CONTEXT_CONTROL) {
            lpContext->PR = pth->pSavedCtx->PR;
            lpContext->R15 = pth->pSavedCtx->R15;
            lpContext->Fir = pth->pSavedCtx->Fir;
            lpContext->Psr = pth->pSavedCtx->Psr;
        }
        if (ulContextFlags & CONTEXT_INTEGER) {
            lpContext->MACH = pth->pSavedCtx->MACH;
            lpContext->MACL = pth->pSavedCtx->MACL;
            lpContext->GBR = pth->pSavedCtx->GBR;
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
            lpContext->R13 = pth->pSavedCtx->R13;
            lpContext->R14 = pth->pSavedCtx->R14;
        }

        if (ulContextFlags & CONTEXT_FLOATING_POINT) {
            FPUFlushContext();
            lpContext->Fpscr = pth->pSavedCtx->Fpscr;
            lpContext->Fpul = pth->pSavedCtx->Fpul;
            memcpy(lpContext->FRegs,pth->pSavedCtx->FRegs,sizeof(lpContext->FRegs));
            memcpy(lpContext->xFRegs,pth->pSavedCtx->xFRegs,sizeof(lpContext->xFRegs));
        }
        if (ulContextFlags & CONTEXT_DEBUG_REGISTERS) {

        }
    } else {
        if (ulContextFlags & CONTEXT_CONTROL) {
            lpContext->PR = pth->ctx.PR;
            lpContext->R15 = pth->ctx.R15;
            lpContext->Fir = pth->ctx.Fir;
            lpContext->Psr = pth->ctx.Psr;
        }
        if (ulContextFlags & CONTEXT_INTEGER) {
            lpContext->MACH = pth->ctx.MACH;
            lpContext->MACL = pth->ctx.MACL;
            lpContext->GBR = pth->ctx.GBR;
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
            lpContext->R13 = pth->ctx.R13;
            lpContext->R14 = pth->ctx.R14;
        }
        if (ulContextFlags & CONTEXT_FLOATING_POINT) {
            FPUFlushContext();
            lpContext->Fpscr = pth->ctx.Fpscr;
            lpContext->Fpul = pth->ctx.Fpul;
            memcpy(lpContext->FRegs,pth->ctx.FRegs,sizeof(lpContext->FRegs));
            memcpy(lpContext->xFRegs,pth->ctx.xFRegs,sizeof(lpContext->xFRegs));
        }
        if (ulContextFlags & CONTEXT_DEBUG_REGISTERS) {

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
    ULONG   ulContextFlags = lpContext->ContextFlags; // Keep a local copy of the context flag
    if (!KC_IsValidThread (pth)
        || (ulContextFlags & ~CONTEXT_FULL)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    // Clear the SH3 and SH4 bits in the context flags.  These are use to differentiate
    // SH3 and SH4.  Without doing so, the masking will always be true because of these 
    // bits.  For example, (CONTEXT_CONTROL & CONTEXT_INTEGER) will be either 0x40 or 0xc0 
    // depending on the processor, but it will never be zero.  So the consequence is that
    // we always return the full context no mater what flags users specify.
    // (Please see \public\common\sdk\inc\winnt.h for details.)
    ulContextFlags &= ~CONTEXT_SH4; 

    if (pth && pth->pSavedCtx) {
        if (ulContextFlags & CONTEXT_CONTROL) {
            pth->pSavedCtx->PR = lpContext->PR;
            pth->pSavedCtx->R15 = lpContext->R15;
            pth->pSavedCtx->Fir = lpContext->Fir;
            pth->pSavedCtx->Psr = (pth->ctx.Psr & 0xfffffcfc) | (lpContext->Psr & 0x00000303);
        }
        if (ulContextFlags & CONTEXT_INTEGER) {
            pth->pSavedCtx->MACH = lpContext->MACH;
            pth->pSavedCtx->MACL = lpContext->MACL;
            pth->pSavedCtx->GBR = lpContext->GBR;
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
            pth->pSavedCtx->R13 = lpContext->R13;
            pth->pSavedCtx->R14 = lpContext->R14;
        }
        if (ulContextFlags & CONTEXT_FLOATING_POINT) {
            FPUFlushContext();
            pth->pSavedCtx->Fpscr = lpContext->Fpscr;
            pth->pSavedCtx->Fpul = lpContext->Fpul;
            memcpy(pth->pSavedCtx->FRegs,lpContext->FRegs,sizeof(lpContext->FRegs));
            memcpy(pth->pSavedCtx->xFRegs,lpContext->xFRegs,sizeof(lpContext->xFRegs));
        }
        if (ulContextFlags & CONTEXT_DEBUG_REGISTERS) {

        }
    } else {
        if (ulContextFlags & CONTEXT_CONTROL) {
            pth->ctx.PR = lpContext->PR;
            pth->ctx.R15 = lpContext->R15;
            pth->ctx.Fir = lpContext->Fir;
            pth->ctx.Psr = (pth->ctx.Psr & 0xfffffcfc) | (lpContext->Psr & 0x00000303);
        }
        if (ulContextFlags & CONTEXT_INTEGER) {
            pth->ctx.MACH = lpContext->MACH;
            pth->ctx.MACL = lpContext->MACL;
            pth->ctx.GBR = lpContext->GBR;
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
            pth->ctx.R13 = lpContext->R13;
            pth->ctx.R14 = lpContext->R14;
        }
        if (ulContextFlags & CONTEXT_FLOATING_POINT) {
            FPUFlushContext();
            pth->ctx.Fpscr = lpContext->Fpscr;
            pth->ctx.Fpul = lpContext->Fpul;
            memcpy(pth->ctx.FRegs,lpContext->FRegs,sizeof(lpContext->FRegs));
            memcpy(pth->ctx.xFRegs,lpContext->xFRegs,sizeof(lpContext->xFRegs));
        }
        if (ulContextFlags & CONTEXT_DEBUG_REGISTERS) {

        }
    }
    return TRUE;
}

void MDRestoreCalleeSavedRegisters (PCALLSTACK pcstk, PCONTEXT pCtx)
{
    ULONG *pRegs = (ULONG *) pcstk->regs;

    pCtx->R8  = pRegs[REG_R8];
    pCtx->R9  = pRegs[REG_R9];
    pCtx->R10 = pRegs[REG_R10];
    pCtx->R11 = pRegs[REG_R11];
    pCtx->R12 = pRegs[REG_R12];
    pCtx->R13 = pRegs[REG_R13];
    pCtx->R14 = pRegs[REG_R14];
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SHInit(
    struct KDataStruct *const pProtoKDataFromNKloader,
    DWORD_PTR dwOEMInitGlobals
    ) 
{
    PFN_DllMain pfnKitlEntry;
    PFN_OEMInitGlobals pfnInitGlob;

    // (1) Disable the cpu cache & flush it
    CCR = 0;
    CCR = CACHE_FLUSH;

    // SH4 cpu architecture requires when modifying CCR from P2 area,
    // at least 8 instructions must be in between before issuing    // a branch to U0/P0/P1/P3 area.
    __asm("nop\n"
          "nop\n"
          "nop\n"
          "nop\n"
          "nop\n"
          "nop\n"
          "nop\n"
          "nop\n");

    // (2) Zero out kernel data page
    memset(&KData, 0, sizeof(KData));

    KData.dwTOCAddr         = pProtoKDataFromNKloader->dwTOCAddr;
    KData.pOsAxsDataBlock   = pProtoKDataFromNKloader->pOsAxsDataBlock;
    KData.pOsAxsHwTrap      = pProtoKDataFromNKloader->pOsAxsHwTrap;

    KData.pAPIReturn    = (ulong)APICallReturn;
    g_pKData = &KData;
    pTOC = (ROMHDR const *)KData.dwTOCAddr;

    // (3) initialize ROM globals
    FirstROM.pTOC = (ROMHDR *) pTOC;
    FirstROM.pNext = 0;
    ROMChain = &FirstROM;

    // (3a) setup any kdata variables needed by OEMInit below
    KInfoTable[KINX_PTOC] = (long)pTOC;
    KInfoTable[KINX_PAGESIZE] = (long)VM_PAGE_SIZE;

    // (4) exchange globals with oal
    pfnInitGlob = (PFN_OEMInitGlobals) dwOEMInitGlobals;
    g_pOemGlobal = pfnInitGlob (g_pNKGlobal);
    g_pOemGlobal->dwMainMemoryEndAddress = pTOC->ulRAMEnd;
    
    // (5) initialize nk globals
    g_pKData->pOem = g_pOemGlobal;
    g_pKData->pNk  = g_pNKGlobal;
    g_pKData->pAPIReturn = (ulong)APICallReturn;
    g_pNKGlobal->pfnWriteDebugString = g_pOemGlobal->pfnWriteDebugString;
    g_pNKGlobal->IntrPrio = IntrPrio;
    g_pNKGlobal->InterruptTable = InterruptTable;
    ExceptionTable[0xE] = (DWORD) g_pOemGlobal->pfnNMIHandler;

    CEProcessorType  = PROCESSOR_HITACHI_SH4;
    CEProcessorLevel = 4;
    CEInstructionSet = PROCESSOR_HITACHI_SH4_INSTRUCTION;

    // (6) initialize kernel page directory entry
    InitPageDirectory (0);
    g_pprcNK->ppdir = g_ppdirNK;
    pVMProc = g_pprcNK;
    pActvProc = g_pprcNK;
    g_pprcNK->vaFree = VM_NKVM_BASE;    

    // faked current thread id so ECS won't put 0 in OwnerThread
    dwCurThId = 2;

    // (7) try to load KITL if exist
    if ((pfnKitlEntry = (PFN_DllMain) g_pOemGlobal->pfnKITLGlobalInit) ||
        (pfnKitlEntry = (PFN_DllMain) FindROMDllEntry (pTOC, KITLDLL))) {
        (* pfnKitlEntry) (NULL, DLL_PROCESS_ATTACH, (DWORD) NKKernelLibIoControl);
    }

#ifdef DEBUG
    CurMSec = dwPrevReschedTime = (DWORD) -200000;      // ~3 minutes before wrap
#endif

    // (8) initialize debug serial
    OEMInitDebugSerial();
    OEMWriteDebugString((LPWSTR) NKSignon);

    // (9) initialize address translation hardware
    MMUTEA = 0;         /* clear transation address */
    MMUTTB = (DWORD)g_pprcNK->ppdir; /* set translation table base address */
    MMUPTEH = 0;        /* clear ASID */
    MMUCR = TLB_FLUSH | TLB_ENABLE;
    LoadKPage();

    // (10) Copy interlocked api code into the kpage
    DEBUGCHK(sizeof(KData) <= FIRST_INTERLOCK);
    DEBUGCHK((InterlockedEnd-InterlockedAPIs)+FIRST_INTERLOCK == 0x400);
    memcpy((char *)&KData+FIRST_INTERLOCK, InterlockedAPIs, InterlockedEnd-InterlockedAPIs);

    // (11) Enable the CPU cache.
    CCR = CACHE_ENABLE | 0x80000105; //| OEMExtraCCR;

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

    NKDbgPrintfW(L"CCR=%4.4x INTEVT=%4.4x\r\n", CCR, g_pOemGlobal->dwSHxIntEventCodeLength);

    // (12) initialize firmware
    OEMInit();
  
    // (13) setup RAM
    KernelFindMemory();

#ifdef DEBUG
    OEMWriteDebugString(TEXT("SH Initialization is done.\r\n"));
#endif    

}

