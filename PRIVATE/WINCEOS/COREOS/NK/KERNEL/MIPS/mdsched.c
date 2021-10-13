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
#define HEADER_FILE
#include "kxmips.h"

#ifdef MIPS_HAS_FPU
void SaveFloatContext(PTHREAD);
void RestoreFloatContext(PTHREAD);
DWORD GetAndClearFloatCode(void);
BOOL HandleHWFloatException(EXCEPTION_RECORD *er, PCONTEXT pctx, CAUSE cause);
void FPUFlushContext(void);
#endif

const wchar_t NKSignon[] = TEXT("Windows CE Kernel for MIPS Built on ") TEXT(__DATE__) TEXT(" at ") TEXT(__TIME__) TEXT("\r\n");

#ifdef _MIPS64
    const wchar_t NKCpuType [] = TEXT("MIPS64");
#else
    const wchar_t NKCpuType [] = TEXT("MIPS32");
#endif

// Define breakpoint instruction values.

#define MIPS32_BREAK(_t) ((SPEC_OP << 26) | ((_t) << 16) | BREAK_OP)
#define MIPS16_BREAK(_t) ((RR_OP16 << 11) | ((_t) << 5) | BREAK_OP16)

// friendly exception name. 1 based (0th entry is for exception id==1)
const LPCSTR g_ppszMDExcpId [MD_MAX_EXCP_ID+1] = {
    IDSTR_INVALID_EXCEPTION,        // invalid exception (0)
    IDSTR_ACCESS_VIOLATION,         // TLB modification (1)
    IDSTR_ACCESS_VIOLATION,         // TLB load (2)
    IDSTR_ACCESS_VIOLATION,         // TLB store (3)
    IDSTR_ALIGNMENT_ERROR,          // address alignment error (load) (4)
    IDSTR_ALIGNMENT_ERROR,          // address alignment error (store) (5)
    IDSTR_ACCESS_VIOLATION,         // bus error (instruction) (6)
    IDSTR_ACCESS_VIOLATION,         // bus error (data) (7)
    IDSTR_INVALID_EXCEPTION,        // sys-call (8)
    IDSTR_DIVIDE_BY_ZERO,           // divide by zero (9)
    IDSTR_INVALID_EXCEPTION,        // reserved instruction (10)
    IDSTR_ILLEGAL_INSTRUCTION,      // co-proc unusable (11)
    IDSTR_INTEGER_OVERFLOW,         // integer overflow (12)
    IDSTR_INVALID_EXCEPTION,        // trap (13)
    IDSTR_INVALID_EXCEPTION,        // unused (14)
    IDSTR_FPU_EXCEPTION,            // FPU exception (15)
};



DWORD dwNKCoProcEnableBits = 0x20000000;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
DumpFrame(
    PTHREAD     pth,
    PICONTEXT   pctx,
    CAUSE       cause,
    ULONG       badVAddr,
    int         level
    ) 
{
    NKDbgPrintfW(L"Exception '%a' (%d): Thread-Id=%8.8lx(pth=%8.8lx), PC=%8.8lx BVA=%8.8lx\r\n",
            GetExceptionString (cause.XCODE), cause.XCODE, dwCurThId, pth, pctx->Fir, badVAddr);
#ifdef  _MIPS64
    NKDbgPrintfW(L"SR=%8.8lx AT=%8.8lx%8.8lx V0=%8.8lx%8.8lx V1=%8.8lx%8.8lx\r\n",
            (long) pctx->Psr,
            (long)( pctx->IntAt >> 32 ), (long)( pctx->IntAt ),
            (long)( pctx->IntV0 >> 32 ), (long)( pctx->IntV0 ),
            (long)( pctx->IntV1 >> 32 ), (long)( pctx->IntV1 )
            );
    NKDbgPrintfW(L"A0=%8.8lx%8.8lx A1=%8.8lx%8.8lx A2=%8.8lx%8.8lx A3=%8.8lx%8.8lx\r\n",
            (long)( pctx->IntA0 >> 32 ), (long)( pctx->IntA0 ),
            (long)( pctx->IntA1 >> 32 ), (long)( pctx->IntA1 ),
            (long)( pctx->IntA2 >> 32 ), (long)( pctx->IntA2 ),
            (long)( pctx->IntA3 >> 32 ), (long)( pctx->IntA3 )
            );
    NKDbgPrintfW(L"T0=%8.8lx%8.8lx T1=%8.8lx%8.8lx T2=%8.8lx%8.8lx T3=%8.8lx%8.8lx\r\n",
            (long)( pctx->IntT0 >> 32 ), (long)( pctx->IntT0 ),
            (long)( pctx->IntT1 >> 32 ), (long)( pctx->IntT1 ),
            (long)( pctx->IntT2 >> 32 ), (long)( pctx->IntT2 ),
            (long)( pctx->IntT3 >> 32 ), (long)( pctx->IntT3 )
            );
    NKDbgPrintfW(L"T4=%8.8lx%8.8lx T5=%8.8lx%8.8lx T6=%8.8lx%8.8lx T7=%8.8lx%8.8lx\r\n",
            (long)( pctx->IntT4 >> 32 ), (long)( pctx->IntT4 ),
            (long)( pctx->IntT5 >> 32 ), (long)( pctx->IntT5 ),
            (long)( pctx->IntT6 >> 32 ), (long)( pctx->IntT6 ),
            (long)( pctx->IntT7 >> 32 ), (long)( pctx->IntT7 )
            );
    NKDbgPrintfW(L"S0=%8.8lx%8.8lx S1=%8.8lx%8.8lx S2=%8.8lx%8.8lx S3=%8.8lx%8.8lx\r\n",
            (long)( pctx->IntS0 >> 32 ), (long)( pctx->IntS0 ),
            (long)( pctx->IntS1 >> 32 ), (long)( pctx->IntS1 ),
            (long)( pctx->IntS2 >> 32 ), (long)( pctx->IntS2 ),
            (long)( pctx->IntS3 >> 32 ), (long)( pctx->IntS3 )
            );
    NKDbgPrintfW(L"S4=%8.8lx%8.8lx S5=%8.8lx%8.8lx S6=%8.8lx%8.8lx S7=%8.8lx%8.8lx\r\n",
            (long)( pctx->IntS4 >> 32 ), (long)( pctx->IntS4 ),
            (long)( pctx->IntS5 >> 32 ), (long)( pctx->IntS5 ),
            (long)( pctx->IntS6 >> 32 ), (long)( pctx->IntS6 ),
            (long)( pctx->IntS7 >> 32 ), (long)( pctx->IntS7 )
            );
    NKDbgPrintfW(L"T8=%8.8lx%8.8lx T9=%8.8lx%8.8lx LO=%8.8lx%8.8lx HI=%8.8lx%8.8lx\r\n",
            (long)( pctx->IntT8 >> 32 ), (long)( pctx->IntT8 ),
            (long)( pctx->IntT9 >> 32 ), (long)( pctx->IntT9 ),
            (long)( pctx->IntLo >> 32 ), (long)( pctx->IntLo ),
            (long)( pctx->IntHi >> 32 ), (long)( pctx->IntHi )
            );
    NKDbgPrintfW(L"GP=%8.8lx%8.8lx SP=%8.8lx%8.8lx S8=%8.8lx%8.8lx RA=%8.8lx%8.8lx\r\n",
            (long)( pctx->IntGp >> 32 ), (long)( pctx->IntGp ),
            (long)( pctx->IntSp >> 32 ), (long)( pctx->IntSp ),
            (long)( pctx->IntS8 >> 32 ), (long)( pctx->IntS8 ),
            (long)( pctx->IntRa >> 32 ), (long)( pctx->IntRa )
            );
#else   //  _MIPS64
    NKDbgPrintfW(L"SR=%8.8lx AT=%8.8lx V0=%8.8lx V1=%8.8lx\r\n",
            pctx->Psr, pctx->IntAt, pctx->IntV0, pctx->IntV1);
    NKDbgPrintfW(L"A0=%8.8lx A1=%8.8lx A2=%8.8lx A3=%8.8lx\r\n",
            pctx->IntA0, pctx->IntA1, pctx->IntA2, pctx->IntA3);
    NKDbgPrintfW(L"T0=%8.8lx T1=%8.8lx T2=%8.8lx T3=%8.8lx\r\n",
            pctx->IntT0, pctx->IntT1, pctx->IntT2, pctx->IntT3);
    NKDbgPrintfW(L"T4=%8.8lx T5=%8.8lx T6=%8.8lx T7=%8.8lx\r\n",
            pctx->IntT4, pctx->IntT5, pctx->IntT6, pctx->IntT7);
    NKDbgPrintfW(L"S0=%8.8lx S1=%8.8lx S2=%8.8lx S3=%8.8lx\r\n",
            pctx->IntS0, pctx->IntS1, pctx->IntS2, pctx->IntS3);
    NKDbgPrintfW(L"S4=%8.8lx S5=%8.8lx S6=%8.8lx S7=%8.8lx\r\n",
            pctx->IntS4, pctx->IntS5, pctx->IntS6, pctx->IntS7);
    NKDbgPrintfW(L"T8=%8.8lx T9=%8.8lx LO=%8.8lx HI=%8.8lx\r\n",
            pctx->IntT8, pctx->IntT9, pctx->IntLo, pctx->IntHi);
    NKDbgPrintfW(L"GP=%8.8lx SP=%8.8lx S8=%8.8lx RA=%8.8lx\r\n",
            pctx->IntGp, pctx->IntSp, pctx->IntS8, pctx->IntRa);
#endif  //  _MIPS64
}

BOOL MDDumpFrame (PTHREAD pth, DWORD dwExcpId, PEXCEPTION_RECORD pExr, PCONTEXT pCtx, DWORD dwLevel)
{
    CAUSE cause;
    cause.XCODE = dwExcpId;
    DumpFrame (pth, (PICONTEXT) &pCtx->IntZero, cause, pExr->ExceptionInformation[1], dwLevel);
    return TRUE;
}

void MDSkipBreakPoint (PEXCEPTION_RECORD pExr, PCONTEXT pCtx)
{
    pCtx->Fir += (pCtx->Fir & 1)? 2 : 4;     // skip over the BREAK instruction
}

BOOL MDIsPageFault (DWORD dwExcpId)
{
    return (XID_TLB_LOAD  == dwExcpId)
        || (XID_TLB_STORE == dwExcpId); 
}

void MDClearVolatileRegs (PCONTEXT pCtx)
{
    pCtx->IntAt = 0;
    pCtx->IntV0 = 0;
    pCtx->IntV1 = 0;
    pCtx->IntA0 = 0;
    pCtx->IntA1 = 0;
    pCtx->IntA2 = 0;
    pCtx->IntA3 = 0;
    pCtx->IntT0 = 0;
    pCtx->IntT1 = 0;
    pCtx->IntT2 = 0;
    pCtx->IntT3 = 0;
    pCtx->IntT4 = 0;
    pCtx->IntT5 = 0;
    pCtx->IntT6 = 0;
    pCtx->IntT7 = 0;
    pCtx->IntT8 = 0;
    pCtx->IntT9 = 0;
    pCtx->IntK0 = 0;
    pCtx->IntK1 = 0;
    pCtx->IntLo = 0;
    pCtx->IntHi = 0;
    
}

LPDWORD MDGetRaisedExceptionInfo (PEXCEPTION_RECORD pExr, PCONTEXT pCtx, PCALLSTACK pcstk)
{
    DWORD cArgs = (DWORD) CONTEXT_TO_PARAM_3 (pCtx);
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
    CAUSE cause;
    cause.AsUlong = dwCause;
    
    DumpFrame (pth, &pth->ctx, cause, badVAddr, level);
    return TRUE;
}

#ifdef DEBUG
int MDDbgPrintExceptionInfo (PEXCEPTION_RECORD pExr, PCALLSTACK pcstk)
{
    DEBUGMSG(ZONE_SEH || !IsInKVM ((DWORD) pcstk), (TEXT("ExceptionDispatch: pcstk=%8.8lx PC=%8.8lx cause=%x\r\n"),
            pcstk, pcstk->retAddr, pcstk->regs[REG_S0]));
    return TRUE;
}
#endif

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
HookInterrupt(
    int hwInterruptNumber,
    FARPROC pfnHandler
    ) 
{
    BOOL fRet = ((DWORD) hwInterruptNumber < 6);
    if (fRet) {
        ISRTable[hwInterruptNumber] = (DWORD)pfnHandler;
        g_pKData->basePSR |= (0x0400 << hwInterruptNumber);
    }
    return fRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
UnhookInterrupt(
    int hwInterruptNumber,
    FARPROC pfnHandler
    ) 
{
    BOOL fRet = ((DWORD) hwInterruptNumber < 6)
                && (ISRTable[hwInterruptNumber] == (DWORD)pfnHandler);

    if (fRet) {
        extern int DisabledInterruptHandler(void);
        ISRTable[hwInterruptNumber] = (DWORD) DisabledInterruptHandler;
        g_pKData->basePSR &= ~(0x0400 << hwInterruptNumber);
    }
    return fRet;
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

#ifdef DEBUG
    // Clear out the register context for debugging.
    memset(&pTh->ctx, 0xBD, sizeof(pTh->ctx));
#endif

    // Leave room for arguments and PRETLS on the stack
    pTh->ctx.IntSp = (ulong) pTh->tlsPtr - SECURESTK_RESERVE;

    pTh->ctx.IntA0 = (ulong)lpStart;
    pTh->ctx.IntA1 = param;
    pTh->ctx.IntK0 = 0;
    pTh->ctx.IntK1 = 0;
    pTh->ctx.IntRa = 0;
    pTh->ctx.Fir   = (ULONG)lpBase;
    if (kmode) {
        pTh->ctx.Psr = PSR_XX_C | PSR_FR_C | PSR_UX_C | KERNEL_MODE;
        KTHRDINFO (pTh) |= UTLS_INKMODE;
    } else {
        pTh->ctx.Psr = PSR_XX_C | PSR_FR_C | PSR_UX_C | USER_MODE;
        KTHRDINFO (pTh) &= ~UTLS_INKMODE;
    }
#ifdef MIPS_HAS_FPU
    pTh->ctx.Fsr = 0x01000000; // handle FPU exceptions
#endif
    pTh->ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
}

// main thread stack: from top, TLS then PRETLS then buf then buf2 (ascii) then args then free


void MDInitStack(LPBYTE lpStack, DWORD cbSize)
{
}

#ifdef MIPS_HAS_FPU


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
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
FPUFlushContext(void) 
{
    if (!InSysCall ())
        KCall ((FARPROC) FPUFlushContext);
    else if (g_CurFPUOwner) {
        SaveFloatContext(g_CurFPUOwner);
        g_CurFPUOwner = 0;
    }
}
#endif



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
HandleException (
    PTHREAD pth,
    DWORD   cause,
    ULONG   badVAddr
    ) 
{
    BOOL fRet;
    KCALLPROFON(0);
    fRet = KC_CommonHandleException (pth, cause, badVAddr, 0);
    KCALLPROFOFF(0);
    return fRet;
}

//
// argument cause, badVAddr, info are exactly the same as the value HandleException passed to KC_CommonHandleException
//
void MDSetupExcpInfo (PTHREAD pth, PCALLSTACK pcstk, DWORD cause, DWORD badVAddr, DWORD unused)
{
    pcstk->regs[REG_S0] = cause;        // S0: cause
    pcstk->regs[REG_S1] = badVAddr;     // S1: badVAddr

    // update PSR
    pth->ctx.Psr = PSR_XX_C | PSR_FR_C | PSR_UX_C | KERNEL_MODE;
}

static BOOL BCWorkAround (DWORD xCode, DWORD badVAddr, PCONTEXT pCtx)
{
    BOOL fHandled = FALSE;
    // workaround WINCEMACRO used in CRT startup functions for old binaries

    switch (xCode) {
    case XID_ADDR_LOAD:         // Address error (load or instruction fetch)
    case XID_TLB_LOAD:          // TLB miss (load or instruction fetch)
    case XID_BUS_ERR_INST:      // Bus error (instruction fetch)
        if (OLD_TERMINATE_PROCESS == badVAddr) {
            // update PC to the new API set mapping
            pCtx->Fir += (FIRST_METHOD - OLD_FIRST_METHOD);
            fHandled = TRUE;
        }
        break;
    default:
        break;
    }
    return fHandled;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL MDHandleHardwareException (PCALLSTACK pcstk, PEXCEPTION_RECORD pExr, PCONTEXT pCtx, LPDWORD pdwExcpId)
{
    PTHREAD     pth      = pCurThread;
    DWORD       badVAddr = (DWORD) pcstk->regs[REG_S1];
    BOOL        fInKMode = !(pcstk->dwPrcInfo & CST_MODE_FROM_USER);
    BOOL        fHandled = FALSE, fBadAccess = FALSE;
    CAUSE       cause;

#if (_M_MRX000 >= 5000) 
    // Mips4
    pCtx->Psr |= PSR_XX_C | PSR_FR_C | PSR_UX_C;
#endif

    cause.AsUlong = (ULONG) pcstk->regs[REG_S0];
    *pdwExcpId    = cause.XCODE;

    if (BCWorkAround (cause.XCODE, badVAddr, pCtx)) {
        DEBUGMSG (ZONE_SEH, (L"BC workaround, changing PC from %8.8lx to %8.8lx\r\n", badVAddr, pCtx->Fir));
        return TRUE;
    }

    switch (cause.XCODE) {

    case XID_ADDR_STORE:        // Address error (store)
    case XID_ADDR_LOAD:         // Address error (load or instruction fetch)
        if (fInKMode || !IsKModeAddr (badVAddr)) {
            pExr->ExceptionCode = STATUS_DATATYPE_MISALIGNMENT;
            break;
        }
        // faull through for access error (access violation)

    case XID_BUS_ERR_INST:     // Bus error (instruction fetch)
    case XID_BUS_ERR_DATA:     // Bus error (data reference)
        fBadAccess = TRUE;
        // faull through for access error (access violation)

    case XID_TLB_LOAD:          // TLB miss (load or instruction fetch)
    case XID_TLB_STORE:         // TLB Miss (store)
    case XID_TLB_MOD:           // TLB modification
        pExr->ExceptionInformation[0]   = cause.XCODE & 1;
        pExr->ExceptionInformation[1]   = badVAddr;
        pExr->ExceptionCode             = STATUS_ACCESS_VIOLATION;
        pExr->NumberParameters          = 2;
        if (!fBadAccess                                 // known bad access (from fall through above)
            && !InSysCall ()                            // not in syscall
            && (fInKMode || !IsKModeAddr (badVAddr))    // have access to the address
            && !IsInSharedHeap (badVAddr)) {            // faulted while access shared heap - always fail
            fHandled = VMProcessPageFault (pVMProc, badVAddr, pExr->ExceptionInformation[0]);
        }
        break;

    case XID_BREAK_POINT:     // Breakpoint
        if (IsMIPS16Supported && (pCtx->Fir & 1)) {  // MIPS16 mode
            pExr->ExceptionInformation[0] = *(USHORT *)(pCtx->Fir & ~1);
            pExr->ExceptionCode = STATUS_BREAKPOINT;
            if (pExr->ExceptionInformation[0] == MIPS16_BREAK (DIVIDE_BY_ZERO_BREAKPOINT)) {
                pExr->ExceptionCode = STATUS_INTEGER_DIVIDE_BY_ZERO;
            } else if (pExr->ExceptionInformation[0] == MIPS16_BREAK (MULTIPLY_OVERFLOW_BREAKPOINT) 
                    || pExr->ExceptionInformation[0] == MIPS16_BREAK (DIVIDE_OVERFLOW_BREAKPOINT)) {
                pExr->ExceptionCode = STATUS_INTEGER_OVERFLOW;
            } else if (pExr->ExceptionInformation[0] == MIPS16_BREAK (BREAKIN_BREAKPOINT)) {
                pCtx->Fir += 2;
            }
        } else {  // MIPS32 mode
            pExr->ExceptionInformation[0] = *(ULONG *)pCtx->Fir;
            pExr->ExceptionCode = STATUS_BREAKPOINT;
            if (pExr->ExceptionInformation[0] == MIPS32_BREAK (DIVIDE_BY_ZERO_BREAKPOINT)) {
                pExr->ExceptionCode = STATUS_INTEGER_DIVIDE_BY_ZERO;
            } else if ((pExr->ExceptionInformation[0] == MIPS32_BREAK (MULTIPLY_OVERFLOW_BREAKPOINT))
                    || (pExr->ExceptionInformation[0] == MIPS32_BREAK (DIVIDE_OVERFLOW_BREAKPOINT))) {
                pExr->ExceptionCode = STATUS_INTEGER_OVERFLOW;
            } else if (pExr->ExceptionInformation[0] == MIPS32_BREAK (BREAKIN_BREAKPOINT)) {
                pCtx->Fir += 4;
            }
        }
        break;
        
    case XID_INT_OVERFLOW:      // arithmetic overflow
        pExr->ExceptionCode = STATUS_INTEGER_OVERFLOW;
        break;
        
#ifdef MIPS_HAS_FPU
    case XID_COPROCESSOR_UNUSABLE:
        // accessing FPU for the 1st time
        KCall ((PKFN)SwitchFPUOwner, pCtx);
        fHandled = TRUE;
        break;

    case XID_FPU_EXCEPTION: {
        DWORD code;
        code = GetAndClearFloatCode();
        DEBUGMSG(ZONE_SEH, (L"FPU Exception register: %8.8lx\r\n", code));
        if (code & 0x10000) {
            pExr->ExceptionCode = STATUS_FLOAT_INVALID_OPERATION;
        } else if (code & 0x08000) {
            pExr->ExceptionCode = STATUS_FLOAT_DIVIDE_BY_ZERO;
        } else if (code & 0x04000) {
            pExr->ExceptionCode = STATUS_FLOAT_OVERFLOW;
        } else if (code & 0x02000) {
            pExr->ExceptionCode = STATUS_FLOAT_UNDERFLOW;
        } else if (code & 0x01000) {
            pExr->ExceptionCode = STATUS_FLOAT_INEXACT_RESULT;
        } else {
            pExr->ExceptionCode = STATUS_FLOAT_DENORMAL_OPERAND;
        }
        FPUFlushContext ();
        memcpy (&pCtx->FltF0, &pth->ctx.FltF0, sizeof(FREG_TYPE)*32);
        pCtx->Fsr = pth->ctx.Fsr;

        if (fHandled = HandleHWFloatException (pExr, pCtx, cause)) {
            // flush FPU context and set FPU owner to zero in case exception handler use any float-point operation.
            FPUFlushContext();
            memcpy (&pth->ctx.FltF0, &pCtx->FltF0, sizeof(FREG_TYPE)*32);
            pth->ctx.Fsr = pCtx->Fsr;
            break;
        }
        break;
    }
#else
    case XID_COPROCESSOR_UNUSABLE:
        pExr->ExceptionCode = STATUS_PRIVILEGED_INSTRUCTION;
        break;
#endif

    case XID_WATCHPOINT:    // watch breakpoint
        pExr->ExceptionCode = STATUS_BREAKPOINT;
        if (IsMIPS16Supported && (pCtx->Fir & 1)) {  // MIPS16
            pExr->ExceptionInformation[0] = MIPS16_BREAK (BREAKIN_BREAKPOINT);
            pCtx->Fir -= 2;
        } else {
            pExr->ExceptionInformation[0] = MIPS32_BREAK (BREAKIN_BREAKPOINT);
            pCtx->Fir -= 4;
        }
        // If you are using the SetCPUHardware watch function you will probably 
        // want to uncomment the following line so that it will clear the register
        // automatically on exception
//            SetCPUHardwareWatch(0,0);
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
BOOL 
DoThreadGetContext(
    PTHREAD pth,
    LPCONTEXT lpContext
    ) 
{
    if (!KC_IsValidThread (pth)
           || (lpContext->ContextFlags & ~CONTEXT_FULL)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (pth && pth->pSavedCtx) {
        if (lpContext->ContextFlags & CONTEXT_FLOATING_POINT) {
#ifdef MIPS_HAS_FPU
            FPUFlushContext();
            lpContext->Fsr = pth->ctx.Fsr;
            lpContext->FltF0 = pth->ctx.FltF0;
            lpContext->FltF1 = pth->ctx.FltF1;
            lpContext->FltF2 = pth->ctx.FltF2;
            lpContext->FltF3 = pth->ctx.FltF3;
            lpContext->FltF4 = pth->ctx.FltF4;
            lpContext->FltF5 = pth->ctx.FltF5;
            lpContext->FltF6 = pth->ctx.FltF6;
            lpContext->FltF7 = pth->ctx.FltF7;
            lpContext->FltF8 = pth->ctx.FltF8;
            lpContext->FltF9 = pth->ctx.FltF9;
            lpContext->FltF10 = pth->ctx.FltF10;
            lpContext->FltF11 = pth->ctx.FltF11;
            lpContext->FltF12 = pth->ctx.FltF12;
            lpContext->FltF13 = pth->ctx.FltF13;
            lpContext->FltF14 = pth->ctx.FltF14;
            lpContext->FltF15 = pth->ctx.FltF15;
            lpContext->FltF16 = pth->ctx.FltF16;
            lpContext->FltF17 = pth->ctx.FltF17;
            lpContext->FltF18 = pth->ctx.FltF18;
            lpContext->FltF19 = pth->ctx.FltF19;
            lpContext->FltF20 = pth->ctx.FltF20;
            lpContext->FltF21 = pth->ctx.FltF21;
            lpContext->FltF22 = pth->ctx.FltF22;
            lpContext->FltF23 = pth->ctx.FltF23;
            lpContext->FltF24 = pth->ctx.FltF24;
            lpContext->FltF25 = pth->ctx.FltF25;
            lpContext->FltF26 = pth->ctx.FltF26;
            lpContext->FltF27 = pth->ctx.FltF27;
            lpContext->FltF28 = pth->ctx.FltF28;
            lpContext->FltF29 = pth->ctx.FltF29;
            lpContext->FltF30 = pth->ctx.FltF30;
            lpContext->FltF31 = pth->ctx.FltF31;
#endif
        }
        if (lpContext->ContextFlags & CONTEXT_CONTROL) {
            lpContext->IntGp = pth->pSavedCtx->IntGp;
            lpContext->IntSp = pth->pSavedCtx->IntSp;
            lpContext->IntRa = pth->pSavedCtx->IntRa;
            lpContext->Fir = pth->pSavedCtx->Fir;
            lpContext->Psr = pth->pSavedCtx->Psr;
        }
        if (lpContext->ContextFlags & CONTEXT_INTEGER) {
            lpContext->IntZero = 0;
            lpContext->IntAt = pth->pSavedCtx->IntAt;
            lpContext->IntV0 = pth->pSavedCtx->IntV0;
            lpContext->IntV1 = pth->pSavedCtx->IntV1;
            lpContext->IntA0 = pth->pSavedCtx->IntA0;
            lpContext->IntA1 = pth->pSavedCtx->IntA1;
            lpContext->IntA2 = pth->pSavedCtx->IntA2;
            lpContext->IntA3 = pth->pSavedCtx->IntA3;
            lpContext->IntT0 = pth->pSavedCtx->IntT0;
            lpContext->IntT1 = pth->pSavedCtx->IntT1;
            lpContext->IntT2 = pth->pSavedCtx->IntT2;
            lpContext->IntT3 = pth->pSavedCtx->IntT3;
            lpContext->IntT4 = pth->pSavedCtx->IntT4;
            lpContext->IntT5 = pth->pSavedCtx->IntT5;
            lpContext->IntT6 = pth->pSavedCtx->IntT6;
            lpContext->IntT7 = pth->pSavedCtx->IntT7;
            lpContext->IntS0 = pth->pSavedCtx->IntS0;
            lpContext->IntS1 = pth->pSavedCtx->IntS1;
            lpContext->IntS2 = pth->pSavedCtx->IntS2;
            lpContext->IntS3 = pth->pSavedCtx->IntS3;
            lpContext->IntS4 = pth->pSavedCtx->IntS4;
            lpContext->IntS5 = pth->pSavedCtx->IntS5;
            lpContext->IntS6 = pth->pSavedCtx->IntS6;
            lpContext->IntS7 = pth->pSavedCtx->IntS7;
            lpContext->IntT8 = pth->pSavedCtx->IntT8;
            lpContext->IntT9 = pth->pSavedCtx->IntT9;
            lpContext->IntS8 = pth->pSavedCtx->IntS8;
            lpContext->IntLo = pth->pSavedCtx->IntLo;
            lpContext->IntHi = pth->pSavedCtx->IntHi;
        }
    } else {
        if (lpContext->ContextFlags & CONTEXT_FLOATING_POINT) {
#ifdef MIPS_HAS_FPU
            FPUFlushContext();
            lpContext->Fsr = pth->ctx.Fsr;
            lpContext->FltF0 = pth->ctx.FltF0;
            lpContext->FltF1 = pth->ctx.FltF1;
            lpContext->FltF2 = pth->ctx.FltF2;
            lpContext->FltF3 = pth->ctx.FltF3;
            lpContext->FltF4 = pth->ctx.FltF4;
            lpContext->FltF5 = pth->ctx.FltF5;
            lpContext->FltF6 = pth->ctx.FltF6;
            lpContext->FltF7 = pth->ctx.FltF7;
            lpContext->FltF8 = pth->ctx.FltF8;
            lpContext->FltF9 = pth->ctx.FltF9;
            lpContext->FltF10 = pth->ctx.FltF10;
            lpContext->FltF11 = pth->ctx.FltF11;
            lpContext->FltF12 = pth->ctx.FltF12;
            lpContext->FltF13 = pth->ctx.FltF13;
            lpContext->FltF14 = pth->ctx.FltF14;
            lpContext->FltF15 = pth->ctx.FltF15;
            lpContext->FltF16 = pth->ctx.FltF16;
            lpContext->FltF17 = pth->ctx.FltF17;
            lpContext->FltF18 = pth->ctx.FltF18;
            lpContext->FltF19 = pth->ctx.FltF19;
            lpContext->FltF20 = pth->ctx.FltF20;
            lpContext->FltF21 = pth->ctx.FltF21;
            lpContext->FltF22 = pth->ctx.FltF22;
            lpContext->FltF23 = pth->ctx.FltF23;
            lpContext->FltF24 = pth->ctx.FltF24;
            lpContext->FltF25 = pth->ctx.FltF25;
            lpContext->FltF26 = pth->ctx.FltF26;
            lpContext->FltF27 = pth->ctx.FltF27;
            lpContext->FltF28 = pth->ctx.FltF28;
            lpContext->FltF29 = pth->ctx.FltF29;
            lpContext->FltF30 = pth->ctx.FltF30;
            lpContext->FltF31 = pth->ctx.FltF31;
#endif
        }
        if (lpContext->ContextFlags & CONTEXT_CONTROL) {
            lpContext->IntGp = pth->ctx.IntGp;
            lpContext->IntSp = pth->ctx.IntSp;
            lpContext->IntRa = pth->ctx.IntRa;
            lpContext->Fir = pth->ctx.Fir;
            lpContext->Psr = pth->ctx.Psr;
        }
        if (lpContext->ContextFlags & CONTEXT_INTEGER) {
            lpContext->IntZero = 0;
            lpContext->IntAt = pth->ctx.IntAt;
            lpContext->IntV0 = pth->ctx.IntV0;
            lpContext->IntV1 = pth->ctx.IntV1;
            lpContext->IntA0 = pth->ctx.IntA0;
            lpContext->IntA1 = pth->ctx.IntA1;
            lpContext->IntA2 = pth->ctx.IntA2;
            lpContext->IntA3 = pth->ctx.IntA3;
            lpContext->IntT0 = pth->ctx.IntT0;
            lpContext->IntT1 = pth->ctx.IntT1;
            lpContext->IntT2 = pth->ctx.IntT2;
            lpContext->IntT3 = pth->ctx.IntT3;
            lpContext->IntT4 = pth->ctx.IntT4;
            lpContext->IntT5 = pth->ctx.IntT5;
            lpContext->IntT6 = pth->ctx.IntT6;
            lpContext->IntT7 = pth->ctx.IntT7;
            lpContext->IntS0 = pth->ctx.IntS0;
            lpContext->IntS1 = pth->ctx.IntS1;
            lpContext->IntS2 = pth->ctx.IntS2;
            lpContext->IntS3 = pth->ctx.IntS3;
            lpContext->IntS4 = pth->ctx.IntS4;
            lpContext->IntS5 = pth->ctx.IntS5;
            lpContext->IntS6 = pth->ctx.IntS6;
            lpContext->IntS7 = pth->ctx.IntS7;
            lpContext->IntT8 = pth->ctx.IntT8;
            lpContext->IntT9 = pth->ctx.IntT9;
            lpContext->IntS8 = pth->ctx.IntS8;
            lpContext->IntLo = pth->ctx.IntLo;
            lpContext->IntHi = pth->ctx.IntHi;
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
        if (lpContext->ContextFlags & CONTEXT_FLOATING_POINT) {
#ifdef MIPS_HAS_FPU
            FPUFlushContext();
            pth->ctx.Fsr = lpContext->Fsr;
            pth->ctx.FltF0 = lpContext->FltF0;
            pth->ctx.FltF1 = lpContext->FltF1;
            pth->ctx.FltF2 = lpContext->FltF2;
            pth->ctx.FltF3 = lpContext->FltF3;
            pth->ctx.FltF4 = lpContext->FltF4;
            pth->ctx.FltF5 = lpContext->FltF5;
            pth->ctx.FltF6 = lpContext->FltF6;
            pth->ctx.FltF7 = lpContext->FltF7;
            pth->ctx.FltF8 = lpContext->FltF8;
            pth->ctx.FltF9 = lpContext->FltF9;
            pth->ctx.FltF10 = lpContext->FltF10;
            pth->ctx.FltF11 = lpContext->FltF11;
            pth->ctx.FltF12 = lpContext->FltF12;
            pth->ctx.FltF13 = lpContext->FltF13;
            pth->ctx.FltF14 = lpContext->FltF14;
            pth->ctx.FltF15 = lpContext->FltF15;
            pth->ctx.FltF16 = lpContext->FltF16;
            pth->ctx.FltF17 = lpContext->FltF17;
            pth->ctx.FltF18 = lpContext->FltF18;
            pth->ctx.FltF19 = lpContext->FltF19;
            pth->ctx.FltF20 = lpContext->FltF20;
            pth->ctx.FltF21 = lpContext->FltF21;
            pth->ctx.FltF22 = lpContext->FltF22;
            pth->ctx.FltF23 = lpContext->FltF23;
            pth->ctx.FltF24 = lpContext->FltF24;
            pth->ctx.FltF25 = lpContext->FltF25;
            pth->ctx.FltF26 = lpContext->FltF26;
            pth->ctx.FltF27 = lpContext->FltF27;
            pth->ctx.FltF28 = lpContext->FltF28;
            pth->ctx.FltF29 = lpContext->FltF29;
            pth->ctx.FltF30 = lpContext->FltF30;
            pth->ctx.FltF31 = lpContext->FltF31;
#endif
        }
        if (lpContext->ContextFlags & CONTEXT_CONTROL) {
            pth->pSavedCtx->IntGp = lpContext->IntGp;
            pth->pSavedCtx->IntSp = lpContext->IntSp;
            pth->pSavedCtx->IntRa = lpContext->IntRa;
            pth->pSavedCtx->Fir = lpContext->Fir;
        }
        if (lpContext->ContextFlags & CONTEXT_INTEGER) {
            pth->pSavedCtx->IntAt = lpContext->IntAt;
            pth->pSavedCtx->IntV0 = lpContext->IntV0;
            pth->pSavedCtx->IntV1 = lpContext->IntV1;
            pth->pSavedCtx->IntA0 = lpContext->IntA0;
            pth->pSavedCtx->IntA1 = lpContext->IntA1;
            pth->pSavedCtx->IntA2 = lpContext->IntA2;
            pth->pSavedCtx->IntA3 = lpContext->IntA3;
            pth->pSavedCtx->IntT0 = lpContext->IntT0;
            pth->pSavedCtx->IntT1 = lpContext->IntT1;
            pth->pSavedCtx->IntT2 = lpContext->IntT2;
            pth->pSavedCtx->IntT3 = lpContext->IntT3;
            pth->pSavedCtx->IntT4 = lpContext->IntT4;
            pth->pSavedCtx->IntT5 = lpContext->IntT5;
            pth->pSavedCtx->IntT6 = lpContext->IntT6;
            pth->pSavedCtx->IntT7 = lpContext->IntT7;
            pth->pSavedCtx->IntS0 = lpContext->IntS0;
            pth->pSavedCtx->IntS1 = lpContext->IntS1;
            pth->pSavedCtx->IntS2 = lpContext->IntS2;
            pth->pSavedCtx->IntS3 = lpContext->IntS3;
            pth->pSavedCtx->IntS4 = lpContext->IntS4;
            pth->pSavedCtx->IntS5 = lpContext->IntS5;
            pth->pSavedCtx->IntS6 = lpContext->IntS6;
            pth->pSavedCtx->IntS7 = lpContext->IntS7;
            pth->pSavedCtx->IntT8 = lpContext->IntT8;
            pth->pSavedCtx->IntT9 = lpContext->IntT9;
            pth->pSavedCtx->IntS8 = lpContext->IntS8;
            pth->pSavedCtx->IntLo = lpContext->IntLo;
            pth->pSavedCtx->IntHi = lpContext->IntHi;
        }
    } else {
        if (lpContext->ContextFlags & CONTEXT_FLOATING_POINT) {
#ifdef MIPS_HAS_FPU
            FPUFlushContext();
            pth->ctx.Fsr = lpContext->Fsr;
            pth->ctx.FltF0 = lpContext->FltF0;
            pth->ctx.FltF1 = lpContext->FltF1;
            pth->ctx.FltF2 = lpContext->FltF2;
            pth->ctx.FltF3 = lpContext->FltF3;
            pth->ctx.FltF4 = lpContext->FltF4;
            pth->ctx.FltF5 = lpContext->FltF5;
            pth->ctx.FltF6 = lpContext->FltF6;
            pth->ctx.FltF7 = lpContext->FltF7;
            pth->ctx.FltF8 = lpContext->FltF8;
            pth->ctx.FltF9 = lpContext->FltF9;
            pth->ctx.FltF10 = lpContext->FltF10;
            pth->ctx.FltF11 = lpContext->FltF11;
            pth->ctx.FltF12 = lpContext->FltF12;
            pth->ctx.FltF13 = lpContext->FltF13;
            pth->ctx.FltF14 = lpContext->FltF14;
            pth->ctx.FltF15 = lpContext->FltF15;
            pth->ctx.FltF16 = lpContext->FltF16;
            pth->ctx.FltF17 = lpContext->FltF17;
            pth->ctx.FltF18 = lpContext->FltF18;
            pth->ctx.FltF19 = lpContext->FltF19;
            pth->ctx.FltF20 = lpContext->FltF20;
            pth->ctx.FltF21 = lpContext->FltF21;
            pth->ctx.FltF22 = lpContext->FltF22;
            pth->ctx.FltF23 = lpContext->FltF23;
            pth->ctx.FltF24 = lpContext->FltF24;
            pth->ctx.FltF25 = lpContext->FltF25;
            pth->ctx.FltF26 = lpContext->FltF26;
            pth->ctx.FltF27 = lpContext->FltF27;
            pth->ctx.FltF28 = lpContext->FltF28;
            pth->ctx.FltF29 = lpContext->FltF29;
            pth->ctx.FltF30 = lpContext->FltF30;
            pth->ctx.FltF31 = lpContext->FltF31;
#endif
        }
        if (lpContext->ContextFlags & CONTEXT_CONTROL) {
            pth->ctx.IntGp = lpContext->IntGp;
            pth->ctx.IntSp = lpContext->IntSp;
            pth->ctx.IntRa = lpContext->IntRa;
            pth->ctx.Fir = lpContext->Fir;
        }
        if (lpContext->ContextFlags & CONTEXT_INTEGER) {
            pth->ctx.IntAt = lpContext->IntAt;
            pth->ctx.IntV0 = lpContext->IntV0;
            pth->ctx.IntV1 = lpContext->IntV1;
            pth->ctx.IntA0 = lpContext->IntA0;
            pth->ctx.IntA1 = lpContext->IntA1;
            pth->ctx.IntA2 = lpContext->IntA2;
            pth->ctx.IntA3 = lpContext->IntA3;
            pth->ctx.IntT0 = lpContext->IntT0;
            pth->ctx.IntT1 = lpContext->IntT1;
            pth->ctx.IntT2 = lpContext->IntT2;
            pth->ctx.IntT3 = lpContext->IntT3;
            pth->ctx.IntT4 = lpContext->IntT4;
            pth->ctx.IntT5 = lpContext->IntT5;
            pth->ctx.IntT6 = lpContext->IntT6;
            pth->ctx.IntT7 = lpContext->IntT7;
            pth->ctx.IntS0 = lpContext->IntS0;
            pth->ctx.IntS1 = lpContext->IntS1;
            pth->ctx.IntS2 = lpContext->IntS2;
            pth->ctx.IntS3 = lpContext->IntS3;
            pth->ctx.IntS4 = lpContext->IntS4;
            pth->ctx.IntS5 = lpContext->IntS5;
            pth->ctx.IntS6 = lpContext->IntS6;
            pth->ctx.IntS7 = lpContext->IntS7;
            pth->ctx.IntT8 = lpContext->IntT8;
            pth->ctx.IntT9 = lpContext->IntT9;
            pth->ctx.IntS8 = lpContext->IntS8;
            pth->ctx.IntLo = lpContext->IntLo;
            pth->ctx.IntHi = lpContext->IntHi;
        }
    }
    return TRUE;
}

void MDRestoreCalleeSavedRegisters (PCALLSTACK pcstk, PCONTEXT pCtx)
{
    REGTYPE *pRegs = (REGTYPE *) pcstk->regs;
    pCtx->IntS0 = pRegs[REG_S0];
    pCtx->IntS1 = pRegs[REG_S1];
    pCtx->IntS2 = pRegs[REG_S2];
    pCtx->IntS3 = pRegs[REG_S3];
    pCtx->IntS4 = pRegs[REG_S4];
    pCtx->IntS5 = pRegs[REG_S5];
    pCtx->IntS6 = pRegs[REG_S6];
    pCtx->IntS7 = pRegs[REG_S7];
    pCtx->IntS8 = pRegs[REG_S8];
    pCtx->IntGp = pRegs[REG_GP];
}

const BYTE *g_pIntrPrio;
const BYTE *g_pIntrMask;
void InitPageDirectory (DWORD dwKDBasePTE);

//
// LoadOAL function to call OAL entry point
// NOTE: Access to KData is not allowed in this function - it's not initialized yet.
// 
void LoadOAL (const ROMHDR* ptoc, DWORD dwOEMInitGlobalsAddr)
{
    PFN_OEMInitGlobals pfnInitGlob = (PFN_OEMInitGlobals) dwOEMInitGlobalsAddr;
    g_pOemGlobal = pfnInitGlob (g_pNKGlobal);
    pTOC = ptoc;
    g_pIntrPrio = g_pOemGlobal->pIntrPrio;
    g_pIntrMask = g_pOemGlobal->pIntrMask;
        
}

void MIPSInit (DWORD dwConfigRegister, DWORD dwKDBasePTE, struct KDataStruct *pKData)
{
    PFN_DllMain pfnKitlEntry;

    DEBUGCHK ((DWORD) pKData == 0xffffd800);    // KData must be at a fixed location
    
    // pickup arguments from the nk loader
    g_pKData   = pKData;

    // initialize nk globals
    FirstROM.pTOC = (ROMHDR *) pTOC;
    FirstROM.pNext = 0;
    ROMChain = &FirstROM;
    KInfoTable[KINX_PTOC] = (long)pTOC;

    g_pKData->pNk  = g_pNKGlobal;
    g_pKData->pOem = g_pOemGlobal;
    g_pOemGlobal->dwMainMemoryEndAddress = pTOC->ulRAMEnd;

    // update co-proc enable bits.
    g_pKData->dwCoProcBits = g_pOemGlobal->dwCoProcBits;

    InitPageDirectory (dwKDBasePTE);
    g_pprcNK->ppdir = g_ppdirNK;
    pVMProc         = g_pprcNK;
    pActvProc       = g_pprcNK;
    g_pprcNK->vaFree = VM_NKVM_BASE;    

    // faked current thread id so ECS won't put 0 in OwnerThread
    dwCurThId = 2;

    g_pNKGlobal->pfnWriteDebugString = g_pOemGlobal->pfnWriteDebugString;

// Note that R4000 is defined for all MIPS at or above R4000, so they
// need to be defined at the top of the chain, or else they will get
// superceded by the R4000 switch
#if defined(R5000) || defined(MIPSIV)
    CEProcessorType  = PROCESSOR_MIPS_R5000;
    CEProcessorLevel = 5;
#elif defined(R4000)
    CEProcessorType  = PROCESSOR_MIPS_R4000;
    CEProcessorLevel = 4;
#else
#pragma error ("Unsupported CPU");
#endif

    CEInstructionSet = 
#ifdef _MIPS64
    #ifdef MIPS_HAS_FPU
        PROCESSOR_MIPS_MIPSIVFP_INSTRUCTION;
    #else
        PROCESSOR_MIPS_MIPSIV_INSTRUCTION;
    #endif
#else
    #ifdef MIPS_HAS_FPU
        PROCESSOR_MIPS_MIPSIIFP_INSTRUCTION;
    #else
        PROCESSOR_MIPS_MIPSII_INSTRUCTION;
    #endif
    if (IsMIPS16Supported) {
        CEInstructionSet = PROCESSOR_MIPS_MIPS16_INSTRUCTION;
    }
#endif

    CEProcessorRevision = (WORD) dwConfigRegister;

    // try to load KITL if exist
    if ((pfnKitlEntry = (PFN_DllMain) g_pOemGlobal->pfnKITLGlobalInit) ||
        (pfnKitlEntry = (PFN_DllMain) FindROMDllEntry (pTOC, KITLDLL))) {
        (* pfnKitlEntry) (NULL, DLL_PROCESS_ATTACH, (DWORD) NKKernelLibIoControl);
    }

#ifdef DEBUG
    CurMSec = dwPrevReschedTime = (DWORD) -200000;      // ~3 minutes before wrap
#endif

    OEMInitDebugSerial ();

    OEMWriteDebugString ((LPWSTR) NKSignon);

}

